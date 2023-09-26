/*
 * Copyright (c) 2012-2016 Apple Inc. All Rights Reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

//
//  SecFileLocations.c
//  utilities
//

/*
    This file incorporates code from securityd_files.c (iOS) and iOSforOSX.c (OSX).
 */

#include <TargetConditionals.h>
#include <AssertMacros.h>
#include <CoreFoundation/CFPriv.h>
#include <CoreFoundation/CFString.h>
#include <CoreFoundation/CFURL.h>
#include <CoreFoundation/CFUtilities.h>
#include <IOKit/IOKitLib.h>
#include <os/feature_private.h>
#include <utilities/SecCFWrappers.h>
#include <utilities/SecCFRelease.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/errno.h>
#include <sys/sysctl.h>
#include <uuid/uuid.h>
#include <copyfile.h>
#include <syslog.h>

#include "SecFileLocations.h"
#include "OSX/sec/Security/SecKnownFilePaths.h"

#include "SecAKSWrappers.h" // for TARGET_HAS_KEYSTORE, needed to determine edu mode
#include <SoftLinking/SoftLinking.h>

#if TARGET_OS_IOS && TARGET_HAS_KEYSTORE
#define HAVE_SOFTLINK_MOBILE_KEYBAG_SUPPORT 1
SOFT_LINK_OPTIONAL_FRAMEWORK(PrivateFrameworks, MobileKeyBag)
SOFT_LINK_FUNCTION(MobileKeyBag, MKBUserTypeDeviceMode, soft_MKBUserTypeDeviceMode, CFDictionaryRef, (CFDictionaryRef options, CFErrorRef * error), (options, error))
SOFT_LINK_CONSTANT(MobileKeyBag, kMKBDeviceModeKey, CFStringRef)
SOFT_LINK_CONSTANT(MobileKeyBag, kMKBDeviceModeSharedIPad, CFStringRef)
#endif


#if TARGET_OS_OSX
static const char * get_host_uuid(void)
{
    static uuid_string_t hostuuid = {};
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        struct timespec timeout = {30, 0};
        uuid_t uuid = {};
        if (gethostuuid(uuid, &timeout) == 0) {
            uuid_unparse(uuid, hostuuid);
        } else {
            secerror("failed to get host uuid");
        }
    });

    return hostuuid;
}

static CFStringRef copy_keychain_uuid_path(CFURLRef keyChainBaseURL)
{
    CFStringRef baseURLString = NULL;
    CFStringRef uuid_path = NULL;

    require(keyChainBaseURL, done);

    baseURLString = CFURLCopyFileSystemPath(keyChainBaseURL, kCFURLPOSIXPathStyle);
    require(baseURLString, done);

    uuid_path = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("%@/%s"), baseURLString, get_host_uuid());

done:
    CFReleaseSafe(baseURLString);
    return uuid_path;
}

// See _kb_verify_create_path in securityd
static bool keychain_verify_create_path(const char *keychainBasePath)
{
    bool created = false;
    struct stat st_info = {};
    char new_path[PATH_MAX] = {};
    char kb_path[PATH_MAX] = {};
    snprintf(kb_path, sizeof(kb_path), "%s", keychainBasePath);
    if (lstat(kb_path, &st_info) == 0) {
        if (S_ISDIR(st_info.st_mode)) {
            created = true;
        } else {
            secerror("invalid directory at '%s' moving aside", kb_path);
            snprintf(new_path, sizeof(new_path), "%s-invalid", kb_path);
            unlink(new_path);
            if (rename(kb_path, new_path) != 0) {
                secerror("failed to rename file: %s (%s)", kb_path, strerror(errno));
                goto done;
            }
        }
    }
    if (!created) {
        errno_t err = mkpath_np(kb_path, 0700);
        require_action(err == 0 || err == EEXIST, done, secerror("could not create path: %s (%s)", kb_path, strerror(err)));
        created = true;
    }

done:
    return created;
}
#endif /* TARGET_OS_OSX */

#if TARGET_OS_IOS
static bool SecSharedDataVolumeBootArgSet(void) {
    static bool boot_arg_set = false;

    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        char bootargs[PATH_MAX];
        size_t bsize=sizeof(bootargs)-1;
        bzero(bootargs,sizeof(bootargs));
        if (sysctlbyname("kern.bootargs", bootargs, &bsize, NULL, 0) == 0) {
            if (strnstr(bootargs, "-apfs_shared_datavolume", bsize)) {
                boot_arg_set = true;
            }
        }
        secnotice("eapfs", "eapfs boot-arg set to %{bool}d", boot_arg_set);
    });

    return boot_arg_set;
}

static bool DeviceTree_SupportsEnhancedApfs(void) {
    static bool supported = false;

    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        io_registry_entry_t fs_props = IORegistryEntryFromPath(kIOMainPortDefault, kIODeviceTreePlane ":/filesystems"); // todo: use kEDTFilesystems when available
        if (fs_props != IO_OBJECT_NULL) {
            CFDataRef eapfs = IORegistryEntryCreateCFProperty(fs_props, CFSTR("e-apfs"), kCFAllocatorDefault, 0); // todo: use kEDTFilesystemsEnhancedAPFS when available
            if (eapfs != NULL) {
                CFRelease(eapfs);
                supported = true;
            }
            IOObjectRelease(fs_props);
        }
        secnotice("eapfs", "eapfs IODT set to %{bool}d", supported);
    });

    return supported;
}

bool SecSupportsEnhancedApfs(void) {
    return DeviceTree_SupportsEnhancedApfs() || SecSharedDataVolumeBootArgSet();
}

#if HAVE_SOFTLINK_MOBILE_KEYBAG_SUPPORT
static bool SecForceEduMode(void) {
#if DEBUG
    struct stat st;
    int result = stat("/Library/Keychains/force_edu_mode", &st);
    return result == 0;
#else
    return false;
#endif /* DEBUG */
}
#endif /* HAVE_SOFTLINK_MOBILE_KEYBAG_SUPPORT */

// Called when deciding whether to use the system keychain XPC name & whether to use the system keybag
bool SecIsEduMode(void)
{
    static bool result = false;
#if HAVE_SOFTLINK_MOBILE_KEYBAG_SUPPORT
    static dispatch_once_t once;
    dispatch_once(&once, ^{
        CFDictionaryRef deviceMode = soft_MKBUserTypeDeviceMode(NULL, NULL);
        if (deviceMode) {
            CFTypeRef value = NULL;
            bool valuePresent = CFDictionaryGetValueIfPresent(deviceMode, getkMKBDeviceModeKey(), &value);

            if (valuePresent && CFEqual(value, getkMKBDeviceModeSharedIPad())) {
                result = true;
            }
#ifndef __clang_analyzer__ // because SOFT_LINK_FUNCTION doesn't like CF_RETURNS_RETAINED decoration
            CFReleaseNull(deviceMode);
#endif
        } else {
            secnotice("edumode", "Cannot determine because deviceMode is NULL");
        }
        if (!result && SecForceEduMode()) {
            secnotice("edumode", "Forcing edu mode");
            result = true;
        }
    });
#endif // HAVE_SOFTLINK_MOBILE_KEYBAG_SUPPORT
    return result;
}
#else
bool SecIsEduMode(void)
{
    return false;
}
#endif // TARGET_OS_IOS

bool SecSeparateUserKeychain(void) {
#if TARGET_OS_OSX
    return true;
#else
    static bool ffSeparateUserKeychain = false;
#if TARGET_OS_IOS
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        ffSeparateUserKeychain = os_feature_enabled(Security, SeparateUserKeychain);
        secnotice("keychain", "SeparateUserKeychain set via feature flag to %s", ffSeparateUserKeychain ? "enabled" : "disabled");
    });
#endif // TARGET_OS_IOS
    return ffSeparateUserKeychain;
#endif // TARGET_OS_OSX
}

CFURLRef SecCopyURLForFileInBaseDirectory(bool system, CFStringRef directoryPath, CFStringRef fileName)
{
    CFURLRef fileURL = NULL;
    CFStringRef suffix = NULL;
    CFURLRef homeURL = SecCopyBaseFilesURL(system);

    if (fileName) {
        suffix = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("%@/%@"), directoryPath, fileName);
    } else if (directoryPath) {
        suffix = CFStringCreateCopy(kCFAllocatorDefault, directoryPath);
    }

    bool isDirectory = !fileName;
    if (homeURL && suffix) {
        fileURL = CFURLCreateCopyAppendingPathComponent(kCFAllocatorDefault, homeURL, suffix, isDirectory);
    }
    CFReleaseSafe(suffix);
    CFReleaseSafe(homeURL);
    return fileURL;
}

static CFURLRef SecCopyURLForFileInParameterizedKeychainDirectory(CFStringRef fileName, bool forceUserScope)
{
#if TARGET_OS_OSX
    // need to tack on uuid here
    Boolean isDirectory = (fileName == NULL);
    CFURLRef resultURL = NULL;
    CFStringRef resultStr = NULL;
    __block bool directoryExists = false;

    CFURLRef keyChainBaseURL = SecCopyURLForFileInBaseDirectory(false, CFSTR("Library/Keychains"), NULL);
    CFStringRef uuid_path = copy_keychain_uuid_path(keyChainBaseURL);
    CFStringPerformWithCString(uuid_path, ^(const char *utf8Str) {
        directoryExists = keychain_verify_create_path(utf8Str);
    });
    require(directoryExists, done);
    if (fileName)
        resultStr = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("%@/%@"), uuid_path, fileName);
    else
        resultStr = CFStringCreateCopy(kCFAllocatorDefault, uuid_path);

done:
    CFReleaseSafe(uuid_path);
    CFReleaseSafe(keyChainBaseURL);
    if (resultStr)
    {
        resultURL = CFURLCreateWithFileSystemPath(kCFAllocatorDefault, resultStr, kCFURLPOSIXPathStyle, isDirectory);
        CFRelease(resultStr);
    }
    return resultURL;
#else /* !TARGET_OS_OSX */
    syslog(LOG_NOTICE, "SecCopyURLForFileInParameterizedKeychainDirectory: forceUserScope:%d", forceUserScope);
    return SecCopyURLForFileInBaseDirectory(!forceUserScope, CFSTR("Library/Keychains"), fileName);
#endif
}

CFURLRef SecCopyURLForFileInUserScopedKeychainDirectory(CFStringRef fileName)
{
    return SecCopyURLForFileInParameterizedKeychainDirectory(fileName, SecSeparateUserKeychain());
}

CFURLRef SecCopyURLForFileInKeychainDirectory(CFStringRef fileName)
{
    return SecCopyURLForFileInParameterizedKeychainDirectory(fileName, false);
}

CFURLRef SecCopyURLForFileInSystemKeychainDirectory(CFStringRef fileName) {
    return SecCopyURLForFileInBaseDirectory(true, CFSTR("Library/Keychains"), fileName);
}

CFURLRef SecCopyURLForFileInUserCacheDirectory(CFStringRef fileName)
{
#if TARGET_OS_OSX
    Boolean isDirectory = (fileName == NULL);
    CFURLRef resultURL = NULL;
    CFStringRef cacheDirStr = NULL;
    char strBuffer[PATH_MAX + 1];
    size_t result = confstr(_CS_DARWIN_USER_CACHE_DIR, strBuffer, sizeof(strBuffer));
    if (result == 0) {
        syslog(LOG_CRIT, "SecCopyURLForFileInUserCacheDirectory: confstr on _CS_DARWIN_USER_CACHE_DIR failed: %d", errno);
        return resultURL;
    }
    cacheDirStr = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("%s/%@"), strBuffer, fileName);
    if (cacheDirStr) {
        resultURL = CFURLCreateWithFileSystemPath(kCFAllocatorDefault, cacheDirStr, kCFURLPOSIXPathStyle, isDirectory);
    }
    CFReleaseSafe(cacheDirStr);
    return resultURL;
#else
    return SecCopyURLForFileInBaseDirectory(true, CFSTR("Library/Caches"), fileName);
#endif
}

CFURLRef SecCopyURLForFileInPreferencesDirectory(CFStringRef fileName)
{
    return SecCopyURLForFileInBaseDirectory(false, CFSTR("Library/Preferences"), fileName);
}

CFURLRef SecCopyURLForFileInManagedPreferencesDirectory(CFStringRef fileName)
{
    CFURLRef resultURL = NULL;

    CFStringRef userName;
#if TARGET_OS_OSX
    userName = CFCopyUserName();
#else
    userName = CFStringCreateWithCString(kCFAllocatorDefault, "mobile", kCFStringEncodingASCII);
#endif

    if (userName) {
        CFStringRef path = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("/Library/Managed Preferences/%@/%@"), userName, fileName);
        if (path) {
            resultURL = CFURLCreateWithFileSystemPath(kCFAllocatorDefault, path, kCFURLPOSIXPathStyle, false);
            CFReleaseSafe(path);
        }
        CFReleaseSafe(userName);
    }

    return resultURL;
}

CFURLRef SecCopyURLForFileInProtectedDirectory(CFStringRef fileName)
{
    return SecCopyURLForFileInBaseDirectory(true, CFSTR("private/var/protected/"), fileName);
}

void WithPathInDirectory(CFURLRef fileURL, void(^operation)(const char *utf8String))
{
    /* Ownership of fileURL is taken by this function and so we release it. */
    if (fileURL) {
        UInt8 buffer[PATH_MAX];
        CFURLGetFileSystemRepresentation(fileURL, false, buffer, sizeof(buffer));

        operation((const char*)buffer);
        CFRelease(fileURL);
    }
}

void WithPathInKeychainDirectory(CFStringRef fileName, void(^operation)(const char *utf8String))
{
    WithPathInDirectory(SecCopyURLForFileInKeychainDirectory(fileName), operation);
}

void WithPathInUserCacheDirectory(CFStringRef fileName, void(^operation)(const char *utf8String))
{
    WithPathInDirectory(SecCopyURLForFileInUserCacheDirectory(fileName), operation);
}

void WithPathInProtectedDirectory(CFStringRef fileName, void(^operation)(const char *utf8String))
{
    WithPathInDirectory(SecCopyURLForFileInProtectedDirectory(fileName), operation);
}

void SetCustomHomePath(const char* path)
{
    if (path) {
        CFStringRef path_cf = CFStringCreateWithCStringNoCopy(NULL, path, kCFStringEncodingUTF8, kCFAllocatorNull);
        SecSetCustomHomeURLString(path_cf);
        CFReleaseSafe(path_cf);
    } else {
        SecSetCustomHomeURLString(NULL);
    }
}



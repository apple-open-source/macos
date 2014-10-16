/*
 * Copyright (c) 2012-2014 Apple Inc. All Rights Reserved.
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
#include <utilities/SecCFWrappers.h>
#include <utilities/SecCFRelease.h>
#include <sys/stat.h>
#include <uuid/uuid.h>
#include <copyfile.h>

#include "SecFileLocations.h"

static CFURLRef sCustomHomeURL = NULL;

static CFURLRef SecCopyHomeURL(void)
{
    // This returns a CFURLRef so that it can be passed as the second parameter
    // to CFURLCreateCopyAppendingPathComponent

    CFURLRef homeURL = sCustomHomeURL;
    if (homeURL) {
        CFRetain(homeURL);
    } else {
#if (TARGET_OS_MAC && !(TARGET_OS_EMBEDDED || TARGET_OS_IPHONE))
        // We would use CFCopyHomeDirectoryURL but it doesn't exist on MACOS.
        // This does the same.
        homeURL = CFCopyHomeDirectoryURLForUser(NULL);
#else
        homeURL = CFCopyHomeDirectoryURL();
#endif
    }

    return homeURL;
}

#if (TARGET_OS_MAC && !(TARGET_OS_EMBEDDED || TARGET_OS_IPHONE))
static const char * get_host_uuid()
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
        require_action(mkpath_np(kb_path, 0700) == 0, done, secerror("could not create path: %s (%s)", kb_path, strerror(errno)));
        created = true;
    }

done:
    return created;
}
#endif /*(TARGET_OS_MAC && !(TARGET_OS_EMBEDDED || TARGET_OS_IPHONE)) */

static CFURLRef SecCopyBaseFilesURL()
{
    CFURLRef baseURL = sCustomHomeURL;
    if (baseURL) {
        CFRetain(baseURL);
    } else {
#if (TARGET_OS_MAC && !(TARGET_OS_EMBEDDED))
        baseURL = SecCopyHomeURL();
#else
        baseURL = CFURLCreateWithFileSystemPath(NULL, CFSTR("/"), kCFURLPOSIXPathStyle, true);
#endif
    }
    return baseURL;
}

static CFURLRef SecCopyURLForFileInBaseDirectory(CFStringRef directoryPath, CFStringRef fileName)
{
    CFURLRef fileURL = NULL;
    CFStringRef suffix = NULL;
    CFURLRef homeURL = SecCopyBaseFilesURL();

    if (fileName)
        suffix = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("%@/%@"), directoryPath, fileName);
    else
    if (directoryPath)
        suffix = CFStringCreateCopy(kCFAllocatorDefault, directoryPath);

    if (homeURL && suffix)
        fileURL = CFURLCreateCopyAppendingPathComponent(kCFAllocatorDefault, homeURL, suffix, false);
    CFReleaseSafe(suffix);
    CFReleaseSafe(homeURL);
    return fileURL;
}

CFURLRef SecCopyURLForFileInKeychainDirectory(CFStringRef fileName)
{
#if (TARGET_OS_MAC && !(TARGET_OS_EMBEDDED || TARGET_OS_IPHONE))
    // need to tack on uuid here
    Boolean isDirectory = (fileName == NULL);
    CFURLRef resultURL = NULL;
    CFStringRef resultStr = NULL;
    __block bool directoryExists = false;

    CFURLRef keyChainBaseURL = SecCopyURLForFileInBaseDirectory(CFSTR("Library/Keychains"), NULL);
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
#else
    return SecCopyURLForFileInBaseDirectory(CFSTR("Library/Keychains"), fileName);
#endif
}

CFURLRef SecCopyURLForFileInPreferencesDirectory(CFStringRef fileName)
{
    return SecCopyURLForFileInBaseDirectory(CFSTR("Library/Preferences"), fileName);
}

void WithPathInKeychainDirectory(CFStringRef fileName, void(^operation)(const char *utf8String))
{
    CFURLRef fileURL = SecCopyURLForFileInKeychainDirectory(fileName);
    UInt8 buffer[MAXPATHLEN];
    CFURLGetFileSystemRepresentation(fileURL, false, buffer, sizeof(buffer));

    operation((const char*)buffer);
    CFRelease(fileURL);
}

void SetCustomHomeURLString(CFStringRef home_path)
{
    CFReleaseNull(sCustomHomeURL);
    if (home_path) {
        sCustomHomeURL = CFURLCreateWithFileSystemPath(NULL, home_path, kCFURLPOSIXPathStyle, true);
    }
}

void SetCustomHomeURL(const char* path)
{
    if (path) {
        CFStringRef path_cf = CFStringCreateWithCStringNoCopy(NULL, path, kCFStringEncodingUTF8, kCFAllocatorNull);
        SetCustomHomeURLString(path_cf);
        CFReleaseSafe(path_cf);
    } else {
        SetCustomHomeURLString(NULL);
    }
}



/*
 * Copyright (c) 2021 Apple Inc. All Rights Reserved.
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

/*
 * SecTrustSettingsServer - read and write Trust Settings plists.
 *
 */
#include "SecTrustSettingsServer.h"
#include <Security/Authorization.h>
#include <Security/AuthorizationDB.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/csr.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <Security/SecBase.h>
#include <Security/SecTrustInternal.h>
#include <Security/SecTrustSettings.h>
#include <Security/TrustSettingsSchema.h>
#include <os/variant_private.h>
#include <utilities/debugging.h>
#include <utilities/SecCFError.h>
#include <utilities/SecInternalReleasePriv.h>
#include "utilities/SecFileLocations.h"
#include "trustdFileLocations.h"
#include "trustdVariants.h"
#include <membership.h> /* for mbr_uid_to_uuid() */

// MARK: -
// MARK: File utilities
/*
 ==============================================================================
   File utilities
 ==============================================================================
*/
static int _writeFile(
    const char          *fileName,
    const unsigned char *bytes,
    size_t              numBytes,
    mode_t              mode)
{
    int rtn;
    int fd;

    fd = open(fileName, O_RDWR | O_CREAT | O_TRUNC, mode);
    if(fd == -1) {
        return errno;
    }
    rtn = (int)write(fd, bytes, (size_t)numBytes);
    if((size_t)rtn != numBytes) {
        if(rtn >= 0) {
            secerror("_writeFile: short write (%ld)", (long)rtn);
        }
        rtn = EIO;
    }
    else {
        rtn = 0;
    }
    close(fd);
    return rtn;
}

static int _readFile(
    const char    *fileName,
    unsigned char **bytes,   // mallocd and returned
    size_t        *numBytes) // returned
{
    int rtn;
    int fd;
    unsigned char *buf;
    struct stat sb;
    size_t length;

    *numBytes = 0;
    *bytes = NULL;
    fd = open(fileName, O_RDONLY, 0);
    if(fd == -1) {
        return errno;
    }
    rtn = fstat(fd, &sb);
    if(rtn) {
        goto errOut;
    }
    length = (size_t)sb.st_size;
    if((buf = malloc(length)) == NULL) {
        rtn = ENOMEM;
        goto errOut;
    }
    rtn = (int)lseek(fd, 0, SEEK_SET);
    if(rtn < 0) {
        free(buf);
        goto errOut;
    }
    rtn = (int)read(fd, buf, (size_t)length);
    if((size_t)rtn != length) {
        if(rtn >= 0) {
            secerror("_readFile: short read (%ld)", (long)rtn);
        }
        free(buf);
        rtn = EIO;
    } else {
        rtn = 0;
        *bytes = buf;
        *numBytes = length;
    }
errOut:
    close(fd);
    return rtn;
}

static int _writeEmptyTrustSettingsFile(const char *fileName, mode_t mode)
{
    CFMutableDictionaryRef dict = CFDictionaryCreateMutable(NULL,
        kSecTrustRecordNumTopDictKeys,
        &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFMutableDictionaryRef trustDict = CFDictionaryCreateMutable(NULL, 0,
        &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFDictionaryAddValue(dict, kTrustRecordTrustList, trustDict);
    CFReleaseNull(trustDict);
    SInt32 vers = kSecTrustRecordVersionCurrent;
    CFNumberRef cfVers = CFNumberCreate(NULL, kCFNumberSInt32Type, &vers);
    CFDictionaryAddValue(dict, kTrustRecordVersion, cfVers);
    CFReleaseNull(cfVers);

    CFErrorRef error = NULL;
    CFDataRef xmlData = CFPropertyListCreateData(kCFAllocatorDefault,
        dict, kCFPropertyListXMLFormat_v1_0, 0, &error);
    CFReleaseNull(dict);

    int rtn = 0;
    if (xmlData && CFDataGetLength(xmlData) > 0) {
        rtn = _writeFile(fileName,
            CFDataGetBytePtr(xmlData), (size_t)CFDataGetLength(xmlData), mode);
    } else {
        rtn = (error) ? (int)CFErrorGetCode(error) : 1;
    }
    CFReleaseNull(xmlData);
    CFReleaseNull(error);
    return rtn;
}

/*
 * Maintain separate read and write queues, so each type of operation is
 * handled serially. This is done so trust settings updates (which are
 * processed asynchronously) do not block trust settings reads (which are
 * synchronous) until we dispatch the last step of the update: file write.
 */

typedef enum {
    kSecTSReadQueue,
    kSecTSWriteQueue,
} SecTSQueue;

static dispatch_queue_t tsQueue(SecTSQueue kind) {
    static dispatch_queue_t readQueue = NULL;
    static dispatch_queue_t writeQueue = NULL;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        readQueue = dispatch_queue_create("trustsettings.read", DISPATCH_QUEUE_SERIAL);
        writeQueue = dispatch_queue_create("trustsettings.write", DISPATCH_QUEUE_SERIAL);
    });
    switch (kind) {
        case kSecTSReadQueue: { return readQueue; }
        case kSecTSWriteQueue: { return writeQueue; }
        default: { return NULL; }
    }
}

static dispatch_queue_t tsReadQueue(void) {
    return tsQueue(kSecTSReadQueue);
}

static dispatch_queue_t tsWriteQueue(void) {
    return tsQueue(kSecTSWriteQueue);
}

/*
 * Given a uid from an incoming message, obtain the path to the
 * appropriate settings file. Caller provides the path buffer.
 */
static bool trustSettingsPath(
    uid_t uid,
    SecTrustSettingsDomain domain,
    char *path,
    size_t maxlen,
    CFErrorRef * _Nonnull error)
{
    if (maxlen > LONG_MAX) {
        return SecError(errSecAllocate, error, CFSTR("path buffer too long"));
    }

    switch(domain) {
        case kSecTrustSettingsDomainUser:
        {
            CFURLRef fileURL = SecCopyURLForFileInPrivateUserTrustdDirectory(CFSTR(USER_TRUST_SETTINGS));
            if (fileURL) {
                CFURLGetFileSystemRepresentation(fileURL, false, (UInt8*)path, (CFIndex)maxlen);
                CFReleaseNull(fileURL);
            } else {
                return SecError(errSecWrPerm, error, CFSTR("unable to create user trust settings path"));
            }
            break;
        }
        case kSecTrustSettingsDomainAdmin:
        {
            CFURLRef fileURL = SecCopyURLForFileInPrivateTrustdDirectory(CFSTR(ADMIN_TRUST_SETTINGS));
            if (fileURL) {
                CFURLGetFileSystemRepresentation(fileURL, false, (UInt8*)path, (CFIndex)maxlen);
                CFReleaseNull(fileURL);
            } else {
                return SecError(errSecWrPerm, error, CFSTR("unable to create admin trust settings path"));
            }
            break;
        }
        case kSecTrustSettingsDomainSystem:
            /*
             * The client could just read this file themselves. But we'll do it if asked.
             */
            strlcpy(path, SYSTEM_TRUST_SETTINGS_PATH, maxlen);
            break;
        default:
            break;
    }
    return true;
}

/*
 ==============================================================================
   Functions for reading and writing trust settings to plist files.
 ==============================================================================
*/
/* Important: _SecTrustSettingsVerifyAuthorization will re-enter trustd,
 * as authd will perform a trust evaluation which reads trust settings.
 * That means it cannot be called from a synchronous XPC handler or it will
 * deadlock waiting on the re-entrant XPC read that never gets processed.
 * We must call this within the block that we dispatched async on the write
 * queue, but outside of any code that is dispatched on the read queue.
 */
static bool _SecTrustSettingsVerifyAuthorization(uid_t uid,
    const char *authRight,
    SecTrustSettingsDomain tsDomain,
    CFDataRef _Nullable authExternalForm,
    CFDataRef _Nullable trustSettings,
    CFErrorRef _Nonnull * _Nullable error)
{
#if TARGET_OS_OSX
    bool result = true;
    AuthorizationExternalForm extForm;
    CFIndex authLen = (authExternalForm) ? CFDataGetLength(authExternalForm) : 0;
    const UInt8 *authPtr = (authExternalForm) ? CFDataGetBytePtr(authExternalForm) : NULL;
    if(!authPtr || authLen <= 0 || authLen > (CFIndex)sizeof(extForm)) {
        return SecError(errSecParam, error, CFSTR("auth blob empty or invalid size"));
    }
    /*
     * Reconstitute auth object from the client's blob
     */
    AuthorizationRef authRef = NULL;
    memcpy(&extForm, authPtr, authLen);
    OSStatus status = AuthorizationCreateFromExternalForm(&extForm, &authRef);
    if(status) {
        return SecError(status, error, CFSTR("AuthorizationCreateFromExternalForm failure"));
    }
    /* now, see if we're authorized to do this thing */
    AuthorizationItem authItem     = {authRight, 0, NULL, 0};
    AuthorizationRights authRights = { 1, &authItem };
    AuthorizationFlags authFlags   = kAuthorizationFlagDefaults |
                                     kAuthorizationFlagExtendRights |
                                     kAuthorizationFlagInteractionAllowed;
    status = AuthorizationCopyRights(authRef, &authRights, NULL, authFlags, NULL);
    /* authorization is for a one-time use, so we destroy right after obtaining it */
    if (authRef) {
        AuthorizationFree(authRef, kAuthorizationFlagDestroyRights);
    }
    if (status) {
        return SecError(status, error, CFSTR("AuthorizationCopyRights failure"));
    }
    return result;
#else /* !TARGET_OS_OSX */
    /* authorization is only used on macOS */
    return SecError(errSecParam, error, CFSTR("Authorization not present"));;
#endif /* !TARGET_OS_OSX */
}

/* Important: _SecTrustSettingsSetData is designed to be invoked only by
 * SecTrustSettingsSetDataBlock, which dispatches it asynchronously on the
 * write queue to avoid blocking reads.
 *
 * Note that the provided uid comes either from the XPC audit token when we
 * are called via the XPC interface, or directly from the client via getuid
 * if libtrustd is being compiled into a standalone test app. For modifying
 * settings, we verify the authorization rights from the externalized auth
 * form, and will fail if the preauthorized right cannot be obtained.
 */
static bool _SecTrustSettingsSetData(uid_t uid,
    CFStringRef _Nonnull domain,
    CFDataRef _Nullable authExternalForm,
    CFDataRef _Nullable trustSettings,
    CFErrorRef _Nonnull * _Nullable error)
{
    __block bool result = true;
    __block mode_t mode = 0;
    const char *authRight = NULL;
    SecTrustSettingsDomain tsDomain = SecTrustSettingsDomainForName(domain);

    switch (tsDomain) {
        case kSecTrustSettingsDomainUser:
            authRight = TRUST_SETTINGS_RIGHT_USER;
            mode = TRUST_SETTINGS_USER_MODE;
            break;
        case kSecTrustSettingsDomainAdmin:
            authRight = TRUST_SETTINGS_RIGHT_ADMIN;
            mode = TRUST_SETTINGS_ADMIN_MODE;
            break;
        case kSecTrustSettingsDomainSystem:
            /* this domain is immutable */
            return SecError(errSecReadOnly, error, CFSTR("system trust settings are not modifiable"));
        default:
            return SecError(errSecInvalidPrefsDomain, error, CFSTR("invalid trust settings domain"));
    }
    /* this re-enters trustd to read trust settings. */
    result = _SecTrustSettingsVerifyAuthorization(uid, authRight, tsDomain,
        authExternalForm, trustSettings, error);
    if (!result) {
        return result;
    }
    secdebug("trustsettings", "_SecTrustSettingsSetData: authorization verified, will write settings");
    /*
     * we've confirmed our authorization, so perform the write
     * on the read queue (so we block other readers while doing it)
     */
    dispatch_sync(tsReadQueue(), ^{
        char path[MAXPATHLEN + 1];
        trustSettingsPath(uid, tsDomain, path, sizeof(path), error);
        CFIndex trustSettingsLen = (trustSettings) ? CFDataGetLength(trustSettings) : 0;
        const UInt8 *trustSettingsPtr = (trustSettings) ? CFDataGetBytePtr(trustSettings) : NULL;
        if (trustSettingsLen <= 0) {
            /* empty trust settings provided, so remove trust settings.
             * note: legacy code would call unlink here to remove the file.
             * we want the file's presence to indicate that it has been
             * migrated from its old location, so we write an empty list instead.
             */
            int rtn = _writeEmptyTrustSettingsFile(path, mode);
            if (rtn) {
                result = SecError((OSStatus)rtn, error, CFSTR("_SecTrustSettingsSetData: _writeEmptyTrustSettingsFile failure"));
            } else {
                secdebug("trustsettings", "_SecTrustSettingsSetData: cleared %s", path);
            }
        } else {
            int rtn = _writeFile(path, (const unsigned char *)trustSettingsPtr, (size_t)trustSettingsLen, mode);
            if (rtn) {
                result = SecError((OSStatus)rtn, error, CFSTR("_SecTrustSettingsSetData: _writeFile failure"));
            } else {
                secdebug("trustsettings", "_SecTrustSettingsSetData: wrote %lu bytes to %s",
                         (unsigned long)trustSettingsLen, path);
            }
        }
    });
    return result;
}

typedef void (^SecTrustSettingsSetDataCompletionHandler)(bool result, CFErrorRef error);

static void SecTrustSettingsSetDataCompleted(const void *userData, bool result, CFErrorRef error) {
    SecTrustSettingsSetDataCompletionHandler completed = (SecTrustSettingsSetDataCompletionHandler)userData;
    secdebug("trustsettings", "SecTrustSettingsSetDataCompleted: calling completion handler");
    completed(result, error);
    Block_release(completed);
}

void SecTrustSettingsSetDataBlock(uid_t uid, CFStringRef _Nonnull domain, CFDataRef _Nullable auth, CFDataRef _Nullable settings, void (^ _Nonnull completed)(bool result, CFErrorRef _Nullable error)) {
    if (!TrustdVariantAllowsFileWrite() || !TrustdVariantAllowsKeychain()) {
        CFErrorRef localError = NULL;
        SecError(errSecUnimplemented, &localError, CFSTR("Trust settings not implemented in this environment"));
        completed(false, localError);
        return;
    }

    SecTrustSettingsSetDataCompletionHandler userData = (SecTrustSettingsSetDataCompletionHandler) Block_copy(completed);
    CFRetainSafe(domain);
    CFRetainSafe(auth);
    CFRetainSafe(settings);
    secdebug("trustsettings", "SecTrustSettingsSetDataBlock: queuing async task on trustsettings.write");

    /* Dispatch the actual function call to set trust settings asynchronously,
     * and return immediately to avoid blocking incoming XPC messages. The
     * completion block takes care of sending back a reply to the client. */
    dispatch_async(tsWriteQueue(), ^{
        secdebug("trustsettings", "SecTrustSettingsSetDataBlock: task started, calling _SecTrustSettingsSetData");
        CFErrorRef localError = NULL;
        bool ok = _SecTrustSettingsSetData(uid, domain, auth, settings, &localError);
        SecTrustSettingsSetDataCompleted(userData, ok, localError);
        CFReleaseSafe(localError);
        CFReleaseSafe(domain);
        CFReleaseSafe(auth);
        CFReleaseSafe(settings);
    });
}

// NO_SERVER Shim code only, xpc interface should call SecTrustSettingsSetDataBlock() directly
bool SecTrustSettingsSetData(uid_t uid,
    CFStringRef _Nonnull domain,
    CFDataRef _Nullable authExternalForm,
    CFDataRef _Nullable trustSettings,
    CFErrorRef _Nonnull * _Nullable error)
{
    __block dispatch_semaphore_t done = dispatch_semaphore_create(0);
    __block bool result = false;
    __block dispatch_queue_t queue = dispatch_queue_create("trustsettings.write.recursive", DISPATCH_QUEUE_SERIAL);
    secdebug("trustsettings", "SecTrustSettingsSetData: queuing async task on trustsettings.write.recursive");

    /* We need to use the async call with the semaphore here instead of a synchronous call
     * because we will return from SecTrustSettingsSetDataBlock immediately before the work
     * is completed. The return is necessary in the XPC interface to avoid blocking while we
     * are waiting, but here, we need to wait for completion before we can return a result. */
    dispatch_async(queue, ^{
        secdebug("trustsettings", "SecTrustSettingsSetData: calling SecTrustSettingsSetDataBlock");
        SecTrustSettingsSetDataBlock(uid, domain, authExternalForm, trustSettings, ^(bool completionResult, CFErrorRef completionError) {
            secdebug("trustsettings", "SecTrustSettingsSetDataBlock: completion block called");
            result = completionResult;
            if (completionResult == false) {
                if (error) {
                    *error = completionError;
                    CFRetainSafe(completionError);
                }
            }
            dispatch_semaphore_signal(done);
        });
    });
    dispatch_semaphore_wait(done, DISPATCH_TIME_FOREVER);
    dispatch_release(done);
    dispatch_release(queue);
    return result;
}

bool SecTrustSettingsCopyData(uid_t uid,
    CFStringRef _Nonnull domain,
    CFDataRef * _Nonnull CF_RETURNS_RETAINED trustSettings,
    CFErrorRef _Nonnull * _Nullable error)
{
    if (!TrustdVariantAllowsFileWrite() || !TrustdVariantAllowsKeychain()) {
        SecError(errSecUnimplemented, error, CFSTR("Trust settings not implemented in this environment"));
        return false;
    }
    __block bool result = true;
    dispatch_sync(tsReadQueue(), ^{
        char path[MAXPATHLEN + 1];
        trustSettingsPath(uid, SecTrustSettingsDomainForName(domain), path, sizeof(path), error);
        unsigned char *fileData = NULL;
        size_t fileDataLen = 0;
        CFDataRef data = NULL;
        if(_readFile(path, &fileData, &fileDataLen) || fileDataLen == 0) {
            secdebug("trustsettings", "SecTrustSettingsCopyData: empty or no file at %s", path);
            result = SecError(errSecNoTrustSettings, error, CFSTR("no trust settings for domain"));
        } else {
            secdebug("trustsettings", "SecTrustSettingsCopyData: read %lu bytes from %s", (unsigned long)fileDataLen, path);
            data = CFDataCreate(kCFAllocatorDefault,
                                (const UInt8 *)fileData,
                                (CFIndex)fileDataLen);
        }
        *trustSettings = data;
        result = (data != NULL);
        free(fileData);
    });
    return result;
}

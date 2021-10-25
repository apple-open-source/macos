//
//  trustdFileHelper.m
//  trustdFileHelper
//
//  Copyright © 2020-2021 Apple Inc. All rights reserved.
//

#import <Foundation/Foundation.h>
#import <Foundation/NSError_Private.h>
#include <sys/stat.h>

#include <utilities/SecCFWrappers.h>
#include <utilities/SecFileLocations.h>

#include "trust/trustd/trustdFileLocations.h"
#include "trust/trustd/trustdFileHelper/trustdFileHelper.h"

@implementation TrustdFileHelper

- (BOOL)changeOwnerOfValidFile:(NSString *)fileName error:(NSError **)error
{
    __block BOOL result = YES;
    __block NSError *localError = nil;
    WithPathInRevocationInfoDirectory((__bridge CFStringRef)fileName, ^(const char *utf8String) {
        int ret = chown(utf8String, TRUSTD_ROLE_ACCOUNT, TRUSTD_ROLE_ACCOUNT);
        if (!(ret == 0)) {
            int localErrno = errno;
            localError = [NSError errorWithDomain:NSPOSIXErrorDomain code:localErrno
                                         userInfo:@{NSLocalizedDescriptionKey : [NSString localizedStringWithFormat:@"failed to change owner of %s: %s", utf8String, strerror(localErrno)]}];
            if (localErrno != ENOENT) { // missing file is not a failure
                secerror("failed to change owner of %s: %s", utf8String, strerror(localErrno));
                result = NO;
            }
        }
    });
    if (!result && error && !*error) {
        *error = localError;
    }
    return result;
}

- (BOOL)fixValidPermissions:(NSError **)error
{
    secdebug("helper", "fixing permissions on valid database");
    NSString *baseFilename = @"valid.sqlite3";
    BOOL result = [self changeOwnerOfValidFile:baseFilename error:error];
    if (result) {
        NSString *shmFile = [NSString stringWithFormat:@"%@-shm", baseFilename];
        result = [self changeOwnerOfValidFile:shmFile error:error];
    }
    if (result) {
        NSString *walFile = [NSString stringWithFormat:@"%@-wal", baseFilename];
        result = [self changeOwnerOfValidFile:walFile error:error];
    }
    if (result) {
        NSString *journalFile = [NSString stringWithFormat:@"%@-journal", baseFilename];
        result = [self changeOwnerOfValidFile:journalFile error:error];
    }
    // Always change the replacement semaphore if it exists
    if (![self changeOwnerOfValidFile:@".valid_replace" error:error]) {
        result = NO;
    }
    return result;
}

- (BOOL)fixTrustSettingsPermissions:(NSError **)error
{
    secdebug("helper", "fixing permissions on trust settings plists");
    NSFileManager *fileManager = [NSFileManager defaultManager];
    NSString *basePath = @"/Library/Security/Trust Settings";
    NSURL *directoryURL = [NSURL fileURLWithPath:basePath isDirectory:YES];
    NSArray *keys = [NSArray arrayWithObject:NSURLIsDirectoryKey];
    NSError *localError = nil;
    BOOL result = YES;

    NSDirectoryEnumerator *enumerator = [fileManager enumeratorAtURL:directoryURL
        includingPropertiesForKeys:keys options:0
        errorHandler:^BOOL(NSURL *url, NSError *fileOrDirError) {
            const char *path = [url fileSystemRepresentation];
            const char *reason = [[fileOrDirError localizedFailureReason] UTF8String];
            if (!reason) { reason = "unknown error"; }
            secerror("unable to fix permissions for %s: %s", path, reason);
            return YES; // continue enumeration
        }];
    for (NSURL *url in enumerator) {
        NSNumber *isDirectory = nil;
        if ([url getResourceValue:&isDirectory forKey:NSURLIsDirectoryKey error:&localError] && ![isDirectory boolValue] && [[url pathExtension] isEqualToString:@"plist"]) {
            const char *path = [url fileSystemRepresentation];
            int ret = chmod(path, 0644); // make world-readable so that trustd can migrate contents
            if (ret) {
                int localErrno = errno;
                localError = [NSError errorWithDomain:NSPOSIXErrorDomain code:localErrno
                                        userInfo:@{NSLocalizedDescriptionKey : [NSString localizedStringWithFormat:@"failed to change permissions of %s: %s", path, strerror(localErrno)]}];
                secerror("failed to change permissions of %s: %s", path, strerror(localErrno));
                result = NO;
            } else {
                secdebug("helper", "fixed permissions on %s", path);
            }
        }
    }
    if (!result && error && !*error) {
        *error = localError;
    }
    return result;
}

- (BOOL)changePermissionsOfKeychainDirectoryFile:(NSString *)fileName error:(NSError **)error
{
    __block BOOL result = YES;
    __block NSError *localError = nil;
    WithPathInKeychainDirectory((__bridge CFStringRef)fileName, ^(const char *utf8String) {
        int ret = chmod(utf8String, 0644); // allow all users to read so that _trustd user can migrate contents
        if (!(ret == 0)) {
            int localErrno = errno;
            localError = [NSError errorWithDomain:NSPOSIXErrorDomain code:localErrno
                                         userInfo:@{NSLocalizedDescriptionKey : [NSString localizedStringWithFormat:@"failed to change permissions of %s: %s", utf8String, strerror(localErrno)]}];
            if (localErrno != ENOENT) { // missing file is not a failure
                secerror("failed to change permissions of %s: %s", utf8String, strerror(localErrno));
                result = NO;
            }
        }
    });
    if (error && !*error) {
        *error = localError;
    }
    return result;
}

- (BOOL)allowTrustdToReadFilesForMigration:(NSError **)error
{
    secdebug("helper", "fixing permissions files that need migration");
    BOOL result = [self changePermissionsOfKeychainDirectoryFile:@"TrustStore.sqlite3" error:error];
    if (![self changePermissionsOfKeychainDirectoryFile:@"com.apple.security.exception_reset_counter.plist" error:error]) {
        result = NO;
    }
    if (![self changePermissionsOfKeychainDirectoryFile:@"CTExceptions.plist" error:error]) {
        result = NO;
    }
    if (![self changePermissionsOfKeychainDirectoryFile:@"CARevocation.plist" error:error]) {
        result = NO;
    }
    if (![self changePermissionsOfKeychainDirectoryFile:@"TransparentConnectionPins.plist" error:error]) {
        result = NO;
    }
    return result;
}

- (void)allowTrustdToWriteAnalyticsFiles
{
#if TARGET_OS_OSX
    WithPathInProtectedDirectory(CFSTR("sfanalytics"), ^(const char *path) {
        /* We need user agents, _securityd, _trustd, and root all to be able to write. They share no groups. */
        mode_t permissions = 0777;
        int ret = mkpath_np(path, permissions);
        if (!(ret == 0 || ret ==  EEXIST)) {
            secerror("could not create path: %s (%s)", path, strerror(ret));
        }

        ret = chmod(path, permissions);
        if (!(ret == 0)) {
            secerror("failed to change permissions of %s: %s", path, strerror(errno));
        }
    });
#else
    WithPathInKeychainDirectory(CFSTR("Analytics"), ^(const char *path) {
        /* We need user agents, _securityd, _trustd, and root all to be able to write. They share no groups. */
        mode_t permissions = 0777;
        int ret = mkpath_np(path, permissions);
        if (!(ret == 0 || ret ==  EEXIST)) {
            secerror("could not create path: %s (%s)", path, strerror(ret));
        }

        ret = chmod(path, permissions);
        if (!(ret == 0)) {
            secerror("failed to change permissions of %s: %s", path, strerror(errno));
        }
    });
#endif
}

- (void)deleteSystemDbFiles:(CFStringRef)baseFilename
{
    WithPathInDirectory(SecCopyURLForFileInSystemKeychainDirectory(baseFilename), ^(const char *utf8String) {
        (void)remove(utf8String);
    });
    CFStringRef shmFile = CFStringCreateWithFormat(NULL, NULL, CFSTR("%@-shm"), baseFilename);
    WithPathInDirectory(SecCopyURLForFileInSystemKeychainDirectory(shmFile), ^(const char *utf8String) {
        (void)remove(utf8String);
    });
    CFReleaseNull(shmFile);
    CFStringRef walFile = CFStringCreateWithFormat(NULL, NULL, CFSTR("%@-wal"), baseFilename);
    WithPathInDirectory(SecCopyURLForFileInSystemKeychainDirectory(walFile), ^(const char *utf8String) {
        (void)remove(utf8String);
    });
    CFReleaseNull(walFile);
    CFStringRef journalFile = CFStringCreateWithFormat(NULL, NULL, CFSTR("%@-journal"), baseFilename);
    WithPathInDirectory(SecCopyURLForFileInSystemKeychainDirectory(journalFile), ^(const char *utf8String) {
        (void)remove(utf8String);
    });
    CFReleaseNull(journalFile);
}

- (void)deleteSupplementalsAssetsDir
{
    WithPathInDirectory(SecCopyURLForFileInSystemKeychainDirectory(CFSTR("SupplementalsAssets/OTAPKIContext.plist")), ^(const char *utf8String) {
        (void)remove(utf8String);
    });
    WithPathInDirectory(SecCopyURLForFileInSystemKeychainDirectory(CFSTR("SupplementalsAssets/TrustedCTLogs.plist")), ^(const char *utf8String) {
        (void)remove(utf8String);
    });
    WithPathInDirectory(SecCopyURLForFileInSystemKeychainDirectory(CFSTR("SupplementalsAssets/TrustedCTLogs_nonTLS.plist")), ^(const char *utf8String) {
        (void)remove(utf8String);
    });
    WithPathInDirectory(SecCopyURLForFileInSystemKeychainDirectory(CFSTR("SupplementalsAssets/AnalyticsSamplingRates.plist")), ^(const char *utf8String) {
        (void)remove(utf8String);
    });
    WithPathInDirectory(SecCopyURLForFileInSystemKeychainDirectory(CFSTR("SupplementalsAssets/AppleCertificateAuthorities.plist")), ^(const char *utf8String) {
        (void)remove(utf8String);
    });
    WithPathInDirectory(SecCopyURLForFileInSystemKeychainDirectory(CFSTR("SupplementalsAssets")), ^(const char *utf8String) {
        (void)remove(utf8String);
    });
}

- (void)deleteOldFiles
{
    secdebug("helper", "deleting /Library/Keychains/crls/valid.sqlite3");
    [self deleteSystemDbFiles:CFSTR("crls/valid.sqlite3")];
    WithPathInDirectory(SecCopyURLForFileInSystemKeychainDirectory(CFSTR("crls")), ^(const char *utf8String) {
        secdebug("helper", "deleting crls directory");
        (void)remove(utf8String);
    });

    secdebug("helper", "deleting /Library/Keychains/pinningrules.sqlite3");
    [self deleteSystemDbFiles:CFSTR("pinningrules.sqlite3")];

    secdebug("helper", "deleting SupplementalsAssets directory");
    [self deleteSupplementalsAssetsDir];

    secdebug("helper", "deleting CAIssuer cache");
#if TARGET_OS_IPHONE
    WithPathInKeychainDirectory(CFSTR("caissuercache.sqlite3"), ^(const char *utf8String) {
        (void)remove(utf8String);
    });
#else // !TARGET_OS_IPHONE
    WithPathInUserCacheDirectory(CFSTR("caissuercache.sqlite3"), ^(const char *utf8String) {
        (void)remove(utf8String);
    });
#endif // !TARGET_OS_IPHONE

#if TARGET_OS_IPHONE
    secdebug("helper", "deleting OCSP cache");
    [self deleteSystemDbFiles:CFSTR("ocspcache.sqlite3")];
#endif // TARGET_OS_IPHONE

}

- (void)fixFiles:(void (^)(BOOL, NSError*))reply
{
    secdebug("ipc", "received trustd request to fix files");
    NSError *error = nil;
    [self deleteOldFiles];
    secdebug("helper", "update Analytics directory permissions");
    [self allowTrustdToWriteAnalyticsFiles];
#if TARGET_OS_OSX
    BOOL success = YES;
    if (![self fixValidPermissions:&error]) {
        secerror("failed to fix Valid permissions for trustd");
        success = NO;
    }
    if (![self fixTrustSettingsPermissions:&error]) {
        secerror("failed to fix trust settings plist permissions for trustd");
        success = NO;
    }
    if (!success) {
        reply(NO, error);
        return;
    }
#else // !TARGET_OS_OSX
    if (![self allowTrustdToReadFilesForMigration:&error]) {
        secerror("failed to change permissions so trustd can read files for migration");
        reply(NO, error);
        return;
    }
#endif // !TARGET_OS_OSX
    reply(YES, error);
}

@end

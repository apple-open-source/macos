//
//  KeychainDBMover.m
//  KeychainDBMover
//

#import "KeychainDBMover.h"
#import <sqlite3.h>
#import <sqlite3_private.h>
#import <sys/types.h>
#import <unistd.h>
#import <stdlib.h>
#import <stdio.h>
#import <pwd.h>
#import "debugging.h"
#if __has_include(<UserManagement/UserManagement.h>)
#import <UserManagement/UserManagement.h>
#endif

@implementation KeychainDBMover

+ (BOOL)moveDBWithURL:(NSURL *)srcURL
                toURL:(NSURL *)dstURL
                error:(NSError**)errorOut
{
    BOOL success = NO;
    sqlite3 *sourceDB = NULL;
    NSError *error = nil;

    int err = sqlite3_open_v2(srcURL.fileSystemRepresentation, &sourceDB, SQLITE_OPEN_READONLY, NULL);
    if (SQLITE_OK != err) {
        int errcode = sqlite3_extended_errcode(sourceDB);
        error = [NSError errorWithDomain:NSOSStatusErrorDomain code:errSecInternalError userInfo:nil];
        secerror("KeychainDBMover: Opening %@ failed: %d", srcURL.path, errcode);
        goto done;
    }

    err = _sqlite3_db_copy_compact(dstURL.fileSystemRepresentation, sourceDB, NULL);
    if (SQLITE_OK != err) {
        int errcode = sqlite3_extended_errcode(sourceDB);
        error = [NSError errorWithDomain:NSOSStatusErrorDomain code:errSecInternalError userInfo:nil];
        secerror("KeychainDBMover: Copying %@ failed: %d", srcURL.path, errcode);
        goto done;
    }

    success = YES;

    secnotice("KeychainDBMover", "Moved [%@] to [%@]", srcURL.path, dstURL.path);

done:
    if (sourceDB) {
        sqlite3_close(sourceDB);
    }
    if (!success && errorOut) {
        *errorOut = error;
    }
    return success;
}

+(NSDictionary*)foregroundUserFileAttrs {
#if __has_include(<UserManagement/UserManagement.h>)
    UMUser* fgUser = [[UMUserManager sharedManager] currentUser];
    uid_t fguUID = [fgUser uid];
    gid_t fguGID = [fgUser gid];
    return @{NSFileOwnerAccountID:[NSNumber numberWithLong:fguUID], NSFileGroupOwnerAccountID:[NSNumber numberWithLong:fguGID]};
#else
    return nil;
#endif
}

+ (BOOL)chownToMobileAtPath:(NSString*)path
                      error:(NSError**)error
{
    NSError* localError = nil;
    if (![[NSFileManager defaultManager] setAttributes:[KeychainDBMover foregroundUserFileAttrs] ofItemAtPath:path error:&localError]) {
        secerror("KeychainDBMover: Could not chown file [%@] %@", path, localError);
        if (error) {
            *error = localError;
        }
        return NO;
    }

    return YES;
}

-(void)moveUserDbWithReply:(void (^)(NSError * nullable))reply {
    NSError* error = nil;

#if __has_include(<UserManagement/UserManagement.h>)
    NSString* srcDirPath = @"/Library/Keychains";
    NSString* touchPath = [srcDirPath stringByAppendingPathComponent:@"user-keychain-moved"];

    if ([[NSFileManager defaultManager] fileExistsAtPath:touchPath]) {
        // Already done
        secnotice("moveUserDb", "Already done");
        reply(nil);
        return;
    }

    NSString* srcPath = [srcDirPath stringByAppendingPathComponent:@"keychain-2.db"];

    if (![[NSFileManager defaultManager] fileExistsAtPath:srcPath]) {
        secnotice("moveUserDb", "Nothing to do");
        reply(nil);
        return;
    }

    NSURL* srcURL = [NSURL fileURLWithPath:srcPath];

    uid_t userUID = [[[UMUserManager sharedManager] currentUser] uid];
    secinfo("moveUserDb", "uid from UM: %d", userUID);

    long bufsize;
    if ((bufsize = sysconf(_SC_GETPW_R_SIZE_MAX)) == -1) {
        secerror("moveUserDb: Failed to get bufsize");
        error = [NSError errorWithDomain:NSOSStatusErrorDomain code:errSecInternalError userInfo:nil];
        reply(error);
        return;
    }

    char buffer[bufsize];
    struct passwd pwd, *result = NULL;
    if (getpwuid_r(userUID, &pwd, buffer, bufsize, &result) != 0 || !result) {
        secerror("moveUserDb: Failed to get bufsize");
        error = [NSError errorWithDomain:NSOSStatusErrorDomain code:errSecInternalError userInfo:nil];
        reply(error);
        return;
    }

    secinfo("moveUserDb", "home dir from getpw: %s", pwd.pw_dir);

    NSString* dstDirPath = [[[NSString stringWithUTF8String:pwd.pw_dir] stringByAppendingPathComponent:@"Library"] stringByAppendingPathComponent:@"Keychains"];

    if (![[NSFileManager defaultManager] createDirectoryAtPath:dstDirPath withIntermediateDirectories:YES attributes:[KeychainDBMover foregroundUserFileAttrs] error:&error]) {
        secerror("moveUserDb: Failed to create path [%@]: %@", dstDirPath, error);
        reply(error);
        return;
    }

    NSString* dstPath = [dstDirPath stringByAppendingPathComponent:@"keychain-2.db"];
    NSURL* dstURL = [NSURL fileURLWithPath:dstPath];

    if (![KeychainDBMover moveDBWithURL:srcURL toURL:dstURL error:&error]) {
        secerror("moveUserDb: Failed [%@] [%@]", srcURL.path, dstURL.path);
        reply(error);
        return;
    }

    if (![KeychainDBMover chownToMobileAtPath:dstPath error:&error] ||
        ![KeychainDBMover chownToMobileAtPath:[dstPath stringByAppendingString:@"-shm"] error:&error] ||
        ![KeychainDBMover chownToMobileAtPath:[dstPath stringByAppendingString:@"-wal"] error:&error]) {
        // Failures logged by chownToMobileAtPath
        reply(error);
        return;
    }

    if (![[NSFileManager defaultManager] createFileAtPath:touchPath contents:nil attributes:[KeychainDBMover foregroundUserFileAttrs]]) {
        secerror("moveUserDb: Failed to create touch file (but returning no error): %@", touchPath);
        // But fall through, because next attempt will fail because the DB file already exists.
        // The second user will also copy the DB, is there anything we can do about that? ¯\_(ツ)_/¯
    }

    reply(error);
#else
    secerror("moveUserDb: UM not available, failing with unimplemented");
    error = [NSError errorWithDomain:NSOSStatusErrorDomain code:errSecUnimplemented userInfo:nil];
    reply(error);
#endif
}

@end

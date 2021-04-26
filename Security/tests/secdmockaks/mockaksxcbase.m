
//
//  mocksecdbase.m
//  Security
//

#import "SecKeybagSupport.h"
#import "SecDbKeychainItem.h"
#import "SecdTestKeychainUtilities.h"
#import "CKKS.h"
#import "SecItemPriv.h"
#import "SecItemServer.h"
#import "spi.h"
#include <ipc/server_security_helpers.h>
#import <utilities/SecCFWrappers.h>
#import <utilities/SecFileLocations.h>
#import <SecurityFoundation/SFEncryptionOperation.h>
#import <XCTest/XCTest.h>
#import <OCMock/OCMock.h>
#import <sqlite3.h>
#import "mockaks.h"

#import "mockaksxcbase.h"

NSString* homeDirUUID;

@interface mockaksxcbase ()
@property NSArray<NSString *>* originalAccessGroups;
@property NSMutableArray<NSString *>* currentAccessGroups;
@end


@implementation mockaksxcbase

+ (void)setUp
{
    [super setUp];

    securityd_init_local_spi();
    securityd_init(NULL);

    SecCKKSDisable();
    /*
     * Disable all of SOS syncing since that triggers retains of database
     * and these tests muck around with the database over and over again, so
     * that leads to the vnode delete kevent trap triggering for sqlite
     * over and over again.
     */
#if OCTAGON
    SecCKKSTestSetDisableSOS(true);
#endif

    // Give this test run a UUID within which each test has a directory
    homeDirUUID = [[NSUUID UUID] UUIDString];
}

- (void)addAccessGroup:(NSString *)accessGroup
{
    [self.currentAccessGroups addObject:accessGroup];
    SecAccessGroupsSetCurrent((__bridge CFArrayRef)self.currentAccessGroups);
}

- (NSString*)createKeychainDirectoryWithSubPath:(NSString*)suffix
{
    NSError* error;
    NSString* dir = suffix ? [NSString stringWithFormat:@"%@/%@/", self.testHomeDirectory, suffix] : self.testHomeDirectory;
    [[NSFileManager defaultManager] createDirectoryAtPath:dir
                              withIntermediateDirectories:YES
                                               attributes:nil
                                                    error:&error];
    XCTAssertNil(error, "Unable to create directory: %@", error);
    return dir;
}

- (void)createKeychainDirectory
{
    [self createKeychainDirectoryWithSubPath:nil];
}

- (void)setUp {
    [super setUp];
    self.originalAccessGroups = (__bridge NSArray *)SecAccessGroupsGetCurrent();
    self.currentAccessGroups = [self.originalAccessGroups mutableCopy];

    NSString* testName = [self.name componentsSeparatedByString:@" "][1];
    testName = [testName stringByReplacingOccurrencesOfString:@"]" withString:@""];
    secnotice("mockaks", "Beginning test %@", testName);

    self.testHomeDirectory = [NSTemporaryDirectory() stringByAppendingPathComponent:[NSString stringWithFormat:@"%@/%@/", homeDirUUID, testName]];
    [self createKeychainDirectory];

    SecSetCustomHomeURLString((__bridge CFStringRef) self.testHomeDirectory);
    SecKeychainDbReset(NULL);

    // Actually load the database.
    kc_with_dbt(true, NULL, ^bool (SecDbConnectionRef dbt) { return false; });
}

- (void)tearDown
{
    NSURL* keychainDir = (NSURL*)CFBridgingRelease(SecCopyHomeURL());

    SecKeychainDbForceClose();

    // Only perform the desctructive step if the url matches what we expect!
    if([keychainDir.path hasPrefix:self.testHomeDirectory]) {
        secnotice("mockaks", "Removing test-specific keychain directory at %@", keychainDir);

        NSError* removeError = nil;
        [[NSFileManager defaultManager] removeItemAtURL:keychainDir error:&removeError];

        XCTAssertNil(removeError, "Should have been able to remove temporary files");
     } else {
         XCTFail("Unsure what happened to the keychain directory URL: %@", keychainDir);
    }

    SecAccessGroupsSetCurrent((__bridge CFArrayRef)self.originalAccessGroups);
    [super tearDown];
}

+ (void)tearDown
{
    SecSetCustomHomeURLString(NULL);
    SecKeychainDbReset(NULL);
}



@end

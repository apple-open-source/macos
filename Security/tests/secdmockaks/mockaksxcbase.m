
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
#import <utilities/SecCFWrappers.h>
#import <utilities/SecFileLocations.h>
#import <SecurityFoundation/SFEncryptionOperation.h>
#import <XCTest/XCTest.h>
#import <OCMock/OCMock.h>
#import <sqlite3.h>
#import "mockaks.h"

#import "mockaksxcbase.h"

NSString* homeDirUUID;

@implementation mockaksxcbase

+ (void)setUp
{
    [super setUp];

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

    NSString* testName = [self.name componentsSeparatedByString:@" "][1];
    testName = [testName stringByReplacingOccurrencesOfString:@"]" withString:@""];
    secnotice("ckkstest", "Beginning test %@", testName);

    self.testHomeDirectory = [NSTemporaryDirectory() stringByAppendingPathComponent:[NSString stringWithFormat:@"%@/%@/", homeDirUUID, testName]];
    [self createKeychainDirectory];

    SetCustomHomeURLString((__bridge CFStringRef) self.testHomeDirectory);
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        securityd_init(NULL);
    });
    SecKeychainDbReset(NULL);

    // Actually load the database.
    kc_with_dbt(true, NULL, ^bool (SecDbConnectionRef dbt) { return false; });
}

+ (void)tearDown
{
    SetCustomHomeURLString(NULL);
    SecKeychainDbReset(NULL);
}

@end

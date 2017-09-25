/*
 * Copyright (c) 2017 Apple Inc. All Rights Reserved.
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

#import "CKKSTests.h"
#import "keychain/ckks/CKKSAnalyticsLogger.h"
#import <Security/SFSQLite.h>
#import <Foundation/Foundation.h>
#import <XCTest/XCTest.h>

static NSString* tablePath = nil;

@interface SQLiteTests : XCTestCase
@end

@implementation SQLiteTests

+ (void)setUp
{
    [super setUp];

    tablePath = [NSTemporaryDirectory() stringByAppendingPathComponent:@"test_table.db"];
}

- (void)setUp
{
    [super setUp];

    [[NSFileManager defaultManager] removeItemAtPath:tablePath error:nil];
}

- (void)tearDown
{
    [[NSFileManager defaultManager] removeItemAtPath:tablePath error:nil];

    [super tearDown];
}

- (void)testCreateLoggingDatabase
{
    NSString* schema = @"CREATE table test (test_column INTEGER);";
    SFSQLite* sqlTable = [[SFSQLite alloc] initWithPath:tablePath schema:schema];
    XCTAssertTrue([sqlTable openWithError:nil]);
    XCTAssertTrue([[NSFileManager defaultManager] fileExistsAtPath:tablePath]);
    [sqlTable close];
}

- (void)testInsertAndDelete
{
    NSString* schema = @"CREATE table test (test_column INTEGER);";
    SFSQLite* sqlTable = [[SFSQLite alloc] initWithPath:tablePath schema:schema];
    XCTAssertTrue([sqlTable openWithError:nil]);

    [sqlTable insertOrReplaceInto:@"test" values:@{@"test_column" : @(1)}];
    [sqlTable insertOrReplaceInto:@"test" values:@{@"test_column" : @(2)}];
    [sqlTable insertOrReplaceInto:@"test" values:@{@"test_column" : @(3)}];
    XCTAssertTrue([[sqlTable selectAllFrom:@"test" where:nil bindings:nil] count] == 3);

    [sqlTable deleteFrom:@"test" where:@"test_column = ?" bindings:@[@3]];
    XCTAssertTrue([[sqlTable selectAllFrom:@"test" where:nil bindings:nil] count] == 2);

    [sqlTable executeSQL:@"delete from test"];
    XCTAssertTrue([[sqlTable selectAllFrom:@"test" where:nil bindings:nil] count] == 0);
}

@end

@interface CKKSAnalyticsLoggerTests : CloudKitKeychainSyncingTestsBase
@end

@implementation CKKSAnalyticsLoggerTests

- (void)testLoggingJSONGenerated
{
    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID]; // Make life easy for this test.

    // We expect a single record to be uploaded.
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID];

    [self startCKKSSubsystem];

    [self addGenericPassword: @"data" account: @"account-delete-me"];

    OCMVerifyAllWithDelay(self.mockDatabase, 8);

    NSError* error = nil;
    NSData* json = [[CKKSAnalyticsLogger logger] getLoggingJSONWithError:&error];
    XCTAssertNotNil(json, @"failed to generate logging json");
    XCTAssertNil(error, @"encourntered error getting logging json: %@", error);

    NSDictionary* dictionary = [NSJSONSerialization JSONObjectWithData:json options:0 error:&error];
    XCTAssertNotNil(dictionary, @"failed to generate dictionary from json data");
    XCTAssertNil(error, @"encountered error deserializing json: %@", error);
    XCTAssertTrue([dictionary isKindOfClass:[NSDictionary class]], @"did not get the class we expected from json deserialization");
}

- (void)testSplunkDefaultTopicNameExists
{
    CKKSAnalyticsLogger* logger = [CKKSAnalyticsLogger logger];
    dispatch_sync(logger.splunkLoggingQueue, ^{
        XCTAssertNotNil(logger.splunkTopicName);
    });
}

- (void)testSplunkDefaultBagURLExists
{
    CKKSAnalyticsLogger* logger = [CKKSAnalyticsLogger logger];
    dispatch_sync(logger.splunkLoggingQueue, ^{
        XCTAssertNotNil(logger.splunkBagURL);
    });
}

// <rdar://problem/32983193> test_KeychainCKKS | CKKSTests failed: "Failed subtests: -[CloudKitKeychainSyncingTests testSplunkUploadURLExists]" [j71ap][CoreOSTigris15Z240][bats-e-27-204-1]
#if 0
- (void)testSplunkUploadURLExists
{
    CKKSAnalyticsLogger* logger = [CKKSAnalyticsLogger logger];
    dispatch_sync(logger.splunkLoggingQueue, ^{
        logger.ignoreServerDisablingMessages = YES;
        XCTAssertNotNil(logger.splunkUploadURL);
    });
}
#endif

- (void)testLastSuccessfulSyncDate
{
    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID]; // Make life easy for this test.
    [self startCKKSSubsystem];
    CKRecord* ckr = [self createFakeRecord: self.keychainZoneID recordName:@"7B598D31-F9C5-481E-98AC-5A507ACB2D85"];
    [self.keychainZone addToZone: ckr];
    
    // Trigger a notification (with hilariously fake data)
    [self.keychainView notifyZoneChange:nil];
    
    [[[self.keychainView waitForFetchAndIncomingQueueProcessing] completionHandlerDidRunCondition] wait:4 * NSEC_PER_SEC];
    
    NSDate* syncDate = [[CKKSAnalyticsLogger logger] dateOfLastSuccessForEvent:CKKSEventProcessIncomingQueueClassC inView:self.keychainView];
    XCTAssertNotNil(syncDate, "Failed to get a last successful sync date");
    NSDate* nowDate = [NSDate dateWithTimeIntervalSinceNow:0];
    NSTimeInterval timeIntervalSinceSyncDate = [nowDate timeIntervalSinceDate:syncDate];
    XCTAssertTrue(timeIntervalSinceSyncDate >= 0.0 && timeIntervalSinceSyncDate <= 15.0, "Last sync date does not look like a reasonable one");
}

- (void)testExtraValuesToUploadToServer
{
    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID]; // Make life easy for this test.
    [self startCKKSSubsystem];
    CKRecord* ckr = [self createFakeRecord: self.keychainZoneID recordName:@"7B598D31-F9C5-481E-98AC-5A507ACB2D85"];
    [self.keychainZone addToZone: ckr];

    // Trigger a notification (with hilariously fake data)
    [self.keychainView notifyZoneChange:nil];

    [[[self.keychainView waitForFetchAndIncomingQueueProcessing] completionHandlerDidRunCondition] wait:4 * NSEC_PER_SEC];

    NSDictionary* extraValues = [[CKKSAnalyticsLogger logger] extraValuesToUploadToServer];
    XCTAssertTrue([extraValues[@"keychain-TLKs"] boolValue]);
    XCTAssertTrue([extraValues[@"keychain-inSyncA"] boolValue]);
    XCTAssertTrue([extraValues[@"keychain-inSyncC"] boolValue]);
    XCTAssertTrue([extraValues[@"keychain-IQNOE"] boolValue]);
    XCTAssertTrue([extraValues[@"keychain-OQNOE"] boolValue]);
    XCTAssertTrue([extraValues[@"keychain-inSync"] boolValue]);
}

- (void)testNilEventDoesNotCrashTheSystem
{
    CKKSAnalyticsLogger* logger = [CKKSAnalyticsLogger logger];
    [logger logSuccessForEventNamed:nil];

    NSData* json = nil;
    NSError* error = nil;
    XCTAssertNoThrow(json = [logger getLoggingJSONWithError:&error]);
    XCTAssertNotNil(json, @"Failed to get JSON after logging nil event");
    XCTAssertNil(error, @"Got error when grabbing JSON after logging nil event: %@", error);
}

- (void)testRaceToCreateLoggers
{
    dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);
    for (NSInteger i = 0; i < 5; i++) {
        dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
            CKKSAnalyticsLogger* logger = [CKKSAnalyticsLogger logger];
            [logger logSuccessForEvent:(CKKSAnalyticsFailableEvent*)@"test_event" inView:self.keychainView];
            dispatch_semaphore_signal(semaphore);
        });
    }

    for (NSInteger i = 0; i < 5; i++) {
        dispatch_semaphore_wait(semaphore, DISPATCH_TIME_FOREVER);
    }
}

- (void)testSysdiagnoseDump
{
    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID]; // Make life easy for this test.
    [self startCKKSSubsystem];
    CKRecord* ckr = [self createFakeRecord: self.keychainZoneID recordName:@"7B598D31-F9C5-481E-98AC-5A507ACB2D85"];
    [self.keychainZone addToZone: ckr];

    // Trigger a notification (with hilariously fake data)
    [self.keychainView notifyZoneChange:nil];

    [self.keychainView waitForFetchAndIncomingQueueProcessing];

    NSError* error = nil;
    NSString* sysdiagnose = [[CKKSAnalyticsLogger logger] getSysdiagnoseDumpWithError:&error];
    XCTAssertNil(error, @"encountered an error grabbing CKKS analytics sysdiagnose: %@", error);
    XCTAssertTrue(sysdiagnose.length > 0, @"failed to get a sysdiagnose from CKKS analytics");
}

@end

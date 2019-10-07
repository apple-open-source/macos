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

#if OCTAGON

#import "CKKSTests.h"
#import "keychain/ckks/CKKSAnalytics.h"
#import <Security/SFSQLite.h>
#import <Foundation/Foundation.h>
#import <CloudKit/CloudKit.h>
#import <CloudKit/CloudKit_Private.h>
#import <XCTest/XCTest.h>
#import <OCMock/OCMock.h>

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
    NSError* error = nil;
    XCTAssertTrue([sqlTable openWithError:&error], @"failed to open sql database");
    XCTAssertNil(error, "encountered error opening database: %@", error);
    XCTAssertTrue([[NSFileManager defaultManager] fileExistsAtPath:tablePath]);
    [sqlTable close];
}

- (void)testInsertAndDelete
{
    NSString* schema = @"CREATE table test (test_column INTEGER);";
    SFSQLite* sqlTable = [[SFSQLite alloc] initWithPath:tablePath schema:schema];
    NSError* error = nil;
    XCTAssertTrue([sqlTable openWithError:&error], @"failed to open sql database");
    XCTAssertNil(error, "encountered error opening database: %@", error);

    [sqlTable insertOrReplaceInto:@"test" values:@{@"test_column" : @(1)}];
    [sqlTable insertOrReplaceInto:@"test" values:@{@"test_column" : @(2)}];
    [sqlTable insertOrReplaceInto:@"test" values:@{@"test_column" : @(3)}];
    XCTAssertTrue([[sqlTable selectAllFrom:@"test" where:nil bindings:nil] count] == 3);

    [sqlTable deleteFrom:@"test" where:@"test_column = ?" bindings:@[@3]];
    XCTAssertTrue([[sqlTable selectAllFrom:@"test" where:nil bindings:nil] count] == 2);

    [sqlTable executeSQL:@"delete from test"];
    XCTAssertTrue([[sqlTable selectAllFrom:@"test" where:nil bindings:nil] count] == 0);
}

- (void)testDontCrashWhenThereAreNoWritePermissions
{
    NSString* schema = @"CREATE table test (test_column INTEGER);";
    SFSQLite* sqlTable = [[SFSQLite alloc] initWithPath:tablePath schema:schema];

    NSError* error = nil;
    XCTAssertNoThrow([sqlTable openWithError:&error], @"opening database threw an exception");
    XCTAssertNil(error, "encountered error opening database: %@", error);
    XCTAssertNoThrow([sqlTable close], @"closing database threw an exception");

    NSDictionary* originalAttributes = [[NSFileManager defaultManager] attributesOfItemAtPath:tablePath error:&error];
    XCTAssertNil(error, @"encountered error getting database file attributes: %@", error);

    [[NSFileManager defaultManager] setAttributes:@{NSFilePosixPermissions : @(400), NSFileImmutable : @(YES)} ofItemAtPath:tablePath error:&error];
    XCTAssertNil(error, @"encountered error setting database file attributes: %@", error);
    XCTAssertNoThrow([sqlTable openWithError:&error]);
    XCTAssertNil(error, @"encounterd error when opening file without permissions: %@", error);

    XCTAssertFalse([sqlTable executeSQL:@"insert or replace into test (test_column) VALUES (1)"],
                   @"writing to read-only database succeeded");

    [[NSFileManager defaultManager] setAttributes:originalAttributes ofItemAtPath:tablePath error:&error];
    XCTAssertNil(error, @"encountered error setting database file attributes back to original attributes: %@", error);
}

- (void)testDontCrashFromInternalErrors
{
    NSString* schema = @"CREATE table test (test_column INTEGER);";
    SFSQLite* sqlTable = [[SFSQLite alloc] initWithPath:tablePath schema:schema];

    NSError* error = nil;
    XCTAssertTrue([sqlTable openWithError:&error], @"failed to open database");
    XCTAssertNil(error, "encountered error opening database: %@", error);

    // delete the table to create havoc
    XCTAssertTrue([sqlTable executeSQL:@"drop table test;"], @"deleting test table should have worked");

    XCTAssertNoThrow([sqlTable insertOrReplaceInto:@"test" values:@{@"test_column" : @(1)}], @"inserting into deleted table threw an exception");
}

@end

@interface CKKSAnalyticsTests : CloudKitKeychainSyncingTestsBase
@property id mockCKKSAnalytics;
@end

@implementation CKKSAnalyticsTests

- (void)setUp
{
    self.mockCKKSAnalytics = OCMClassMock([CKKSAnalytics class]);
    OCMStub([self.mockCKKSAnalytics databasePath]).andCall(self, @selector(databasePath));
    [super setUp];
}

- (void)tearDown
{
    [self.mockCKKSAnalytics stopMocking];
    self.mockCKKSAnalytics = nil;
    [super tearDown];
}

- (NSString*)databasePath
{
    return [NSTemporaryDirectory() stringByAppendingPathComponent:@"test_ckks_analytics_v2.db"];
}

static void _XCTAssertTimeDiffWithInterval(CKKSAnalyticsTests* self, const char* filename, int line, NSDate* a, NSDate* b, int delta, NSString * _Nullable format, ...) NS_FORMAT_FUNCTION(7,8);

/*
 * Check if [a,b] are within expected time difference.
 * The actual acceptable range is (-1.0, d.0] -- to deal with rounding errors when stored in SQLite.
 */

#define XCTAssertTimeDiffWithInterval(a, b, delta, ...)          \
  _XCTAssertTimeDiffWithInterval(self, __FILE__, __LINE__, a, b, delta, @"" __VA_ARGS__)

static void _XCTAssertTimeDiffWithInterval(CKKSAnalyticsTests* self, const char* filename, int line, NSDate* a, NSDate* b, int delta, NSString * _Nullable format, ...) {
    NSTimeInterval interval = [b timeIntervalSinceDate:a];
    if (interval <= -1.0 || interval > delta) {
        NSString *comparison = [[NSString alloc] initWithFormat:@"time diff not expected (a=%@(%f), b=%@(%f), delta = %f) -- valid range (-1.0, %d.0]: ", a, [a timeIntervalSince1970], b, [b timeIntervalSince1970], interval, delta];
        NSString *arg = [[NSString alloc] init];
        if (format) {
            va_list args;
            va_start(args, format);
            va_end(args);
            arg = [[NSString alloc] initWithFormat: format arguments: args];
        }
        [self recordFailureWithDescription: [comparison stringByAppendingString: arg] inFile: [NSString stringWithUTF8String: filename] atLine: line expected: YES];
    }
}

- (void)testLastSuccessfulXDate
{
    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID]; // Make life easy for this test.
    [self startCKKSSubsystem];
    CKRecord* ckr = [self createFakeRecord: self.keychainZoneID recordName:@"7B598D31-F9C5-481E-98AC-5A507ACB2D85"];
    [self.keychainZone addToZone: ckr];
    
    // Trigger a notification (with hilariously fake data)
    [self.keychainView notifyZoneChange:nil];
    
    [[[self.keychainView waitForFetchAndIncomingQueueProcessing] completionHandlerDidRunCondition] wait:4 * NSEC_PER_SEC];

    NSDate* nowDate = [NSDate date];

    /*
     * Check last sync date for class A
     */
    NSDate* syncADate = [[CKKSAnalytics logger] dateOfLastSuccessForEvent:CKKSEventProcessIncomingQueueClassA inView:self.keychainView];
    XCTAssertNotNil(syncADate, "Failed to get a last successful A sync date");
    XCTAssertTimeDiffWithInterval(syncADate, nowDate, 15, "Last sync A date should be recent");

    /*
     * Check last sync date for class C
     */
    NSDate *syncCDate = [[CKKSAnalytics logger] dateOfLastSuccessForEvent:CKKSEventProcessIncomingQueueClassC inView:self.keychainView];
    XCTAssertNotNil(syncCDate, "Failed to get a last successful C sync date");
    XCTAssertTimeDiffWithInterval(syncCDate, nowDate, 15, "Last sync C date should be recent");

    /*
     * Check last unlock date
     */
    NSDate* unlockDate = [[CKKSAnalytics logger] datePropertyForKey:CKKSAnalyticsLastUnlock];
    XCTAssertNotNil(unlockDate, "Failed to get a last unlock date");
    XCTAssertTimeDiffWithInterval(unlockDate, nowDate, 15, "Last unlock date should be recent");

    sleep(2); // wait to be a different second (+/- 1s)

    self.aksLockState = true;
    [self.lockStateTracker recheck];

    NSDate* newUnlockDate = [[CKKSAnalytics logger] datePropertyForKey:CKKSAnalyticsLastUnlock];
    XCTAssertNotNil(newUnlockDate, "Failed to get a last unlock date");

    XCTAssertTimeDiffWithInterval(newUnlockDate, unlockDate, 1, "unlock dates not the same (within one second)");

    sleep(1); // wait to be a differnt second

    self.aksLockState = false;
    [self.lockStateTracker recheck];

    sleep(1); // wait for the completion block to have time to fire

    newUnlockDate = [[CKKSAnalytics logger] datePropertyForKey:CKKSAnalyticsLastUnlock];
    XCTAssertNotNil(newUnlockDate, "Failed to get a last unlock date");
    XCTAssertNotEqualObjects(newUnlockDate, unlockDate, "unlock date the same");
}

- (void)testRaceToCreateLoggers
{
    dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);
    for (NSInteger i = 0; i < 5; i++) {
        dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
            CKKSAnalytics* logger = [CKKSAnalytics logger];
            [logger logSuccessForEvent:(CKKSAnalyticsFailableEvent*)@"test_event" inView:self.keychainView];
            dispatch_semaphore_signal(semaphore);
        });
    }

    for (NSInteger i = 0; i < 5; i++) {
        dispatch_semaphore_wait(semaphore, DISPATCH_TIME_FOREVER);
    }
}

- (void)testUnderlayingError
{
    NSDictionary *errorString = nil;
    NSError *error = nil;

    error = [NSError errorWithDomain:CKErrorDomain code:CKErrorPartialFailure userInfo:@{
        CKPartialErrorsByItemIDKey : @{
                @"recordid" : [NSError errorWithDomain:CKErrorDomain code:1 userInfo:nil],
        }
    }];

    errorString = [[CKKSAnalytics logger] errorChain:error depth:0];

    XCTAssertEqualObjects(errorString[@"domain"], CKErrorDomain, "error domain");
    XCTAssertEqual([errorString[@"code"] intValue], CKErrorPartialFailure, "error code");

    XCTAssertEqualObjects(errorString[@"oneCloudKitPartialFailure"][@"domain"], CKErrorDomain, "error domain");
    XCTAssertEqual([errorString[@"oneCloudKitPartialFailure"][@"code"] intValue], 1, "error code");

    /* interal partial error leaks out of CK */

    error = [NSError errorWithDomain:CKErrorDomain code:CKErrorPartialFailure userInfo:@{
        CKPartialErrorsByItemIDKey : @{
                @"recordid1" : [NSError errorWithDomain:CKErrorDomain code:CKErrorBatchRequestFailed userInfo:nil],
                @"recordid2" : [NSError errorWithDomain:CKErrorDomain code:CKErrorBatchRequestFailed userInfo:nil],
                @"recordid3" : [NSError errorWithDomain:CKErrorDomain code:CKErrorBatchRequestFailed userInfo:nil],
                @"recordid4" : [NSError errorWithDomain:CKErrorDomain code:CKErrorBatchRequestFailed userInfo:nil],
                @"recordid5" : [NSError errorWithDomain:CKErrorDomain code:CKErrorBatchRequestFailed userInfo:nil],
                @"recordid6" : [NSError errorWithDomain:CKErrorDomain code:CKErrorBatchRequestFailed userInfo:nil],
                @"recordid7" : [NSError errorWithDomain:CKErrorDomain code:CKErrorBatchRequestFailed userInfo:nil],
                @"recordid8" : [NSError errorWithDomain:CKErrorDomain code:CKErrorBatchRequestFailed userInfo:nil],
                @"recordid9" : [NSError errorWithDomain:CKErrorDomain code:CKErrorBatchRequestFailed userInfo:nil],
                @"recordid0" : [NSError errorWithDomain:CKErrorDomain code:1 userInfo:nil],
                @"recordid10" : [NSError errorWithDomain:CKErrorDomain code:CKErrorBatchRequestFailed userInfo:nil],
                @"recordid12" : [NSError errorWithDomain:CKErrorDomain code:CKErrorBatchRequestFailed userInfo:nil],
                @"recordid13" : [NSError errorWithDomain:CKErrorDomain code:CKErrorBatchRequestFailed userInfo:nil],
                @"recordid14" : [NSError errorWithDomain:CKErrorDomain code:CKErrorBatchRequestFailed userInfo:nil],
                @"recordid15" : [NSError errorWithDomain:CKErrorDomain code:CKErrorBatchRequestFailed userInfo:nil],
                @"recordid16" : [NSError errorWithDomain:CKErrorDomain code:CKErrorBatchRequestFailed userInfo:nil],
                @"recordid17" : [NSError errorWithDomain:CKErrorDomain code:CKErrorBatchRequestFailed userInfo:nil],
                @"recordid18" : [NSError errorWithDomain:CKErrorDomain code:CKErrorBatchRequestFailed userInfo:nil],
                @"recordid19" : [NSError errorWithDomain:CKErrorDomain code:CKErrorBatchRequestFailed userInfo:nil],
        }
    }];

    errorString = [[CKKSAnalytics logger] errorChain:error depth:0];

    XCTAssertEqualObjects(errorString[@"domain"], CKErrorDomain, "error domain");
    XCTAssertEqual([errorString[@"code"] intValue], CKErrorPartialFailure, "error code");

    XCTAssertEqualObjects(errorString[@"oneCloudKitPartialFailure"][@"domain"], CKErrorDomain, "error domain");
    XCTAssertEqualObjects(errorString[@"oneCloudKitPartialFailure"][@"code"], @1, "error code");




    error = [NSError errorWithDomain:@"domain" code:1 userInfo:@{
        NSUnderlyingErrorKey : [NSError errorWithDomain:CKErrorDomain code:1 userInfo:nil],
    }];

    errorString = [[CKKSAnalytics logger] errorChain:error depth:0];

    XCTAssertEqualObjects(errorString[@"domain"], @"domain", "error domain");
    XCTAssertEqual([errorString[@"code"] intValue], 1, "error code");

    XCTAssertEqualObjects(errorString[@"child"][@"domain"], CKErrorDomain, "error domain");
    XCTAssertEqual([errorString[@"child"][@"code"] intValue], 1, "error code");
}


@end

#endif

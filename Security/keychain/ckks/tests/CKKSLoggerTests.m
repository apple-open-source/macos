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

#if OCTAGON

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
    XCTAssertNotNil(error, @"failed to generate error when opening file without permissions");
    error = nil;

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

    // delete the database to create havoc
    [[NSFileManager defaultManager] removeItemAtPath:tablePath error:nil];

    XCTAssertNoThrow([sqlTable insertOrReplaceInto:@"test" values:@{@"test_column" : @(1)}], @"inserting into deleted table threw an exception");
}

@end

@interface CKKSAnalyticsTests : CloudKitKeychainSyncingTestsBase
@end

@implementation CKKSAnalyticsTests

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

@end

#endif

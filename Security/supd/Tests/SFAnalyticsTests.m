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

#import <XCTest/XCTest.h>
#import <Security/SFAnalytics.h>
#import <Security/SecLaunchSequence.h>
#import "SFAnalytics+Internal.h"
#import "SFAnalyticsDefines.h"
#import "SFAnalyticsSQLiteStore.h"
#import "NSDate+SFAnalytics.h"
#import "SFAnalyticsCollection.h"
#import "SFSQLite.h"
#import "SECSFARules.h"
#import "SECSFARule.h"
#import "SECSFAAction.h"
#import "SECSFAActionAutomaticBugCapture.h"
#import "SECSFAActionDropEvent.h"
#import <Prequelite/Prequelite.h>
#import <CoreFoundation/CFPriv.h>
#import <notify.h>
#import <stdatomic.h>
#import "keychain/analytics/TestResourceUsage.h"

@interface UnitTestAnalytics : SFAnalytics
+ (NSString*)databasePath;
+ (void)setDatabasePath:(NSString*)path;
@end

// MARK: SFAnalytics subclass for custom DB

@implementation UnitTestAnalytics
static NSString* _utapath;

+ (NSString*)databasePath
{
    return _utapath;
}

+ (void)setDatabasePath:(NSString*)path
{
    _utapath = path;
}

@end

@interface TestCollectionActions: NSObject <SFAnalyticsCollectionAction>
@property XCTestExpectation *abcExpectation;
@property XCTestExpectation *ttrExpectation;
@property BOOL ratelimit;
@end

@implementation TestCollectionActions

- (void)autoBugCaptureWithType:(nonnull NSString *)type subType:(nonnull NSString *)subType domain:(nonnull NSString *)domain {
    [self.abcExpectation fulfill];
}

- (void)tapToRadar:(NSString*)alert
       description:(NSString*)description
             radar:(NSString*)radar
     componentName:(NSString*)componentName
  componentVersion:(NSString*)componentVersion
       componentID:(NSString*)componentID
        attributes:(NSDictionary *_Nullable)attributes
{
    [self.ttrExpectation fulfill];
}

- (BOOL)shouldRatelimit:(SFAnalytics*)logger rule:(SFAnalyticsMatchingRule*)rule
{
    return self.ratelimit;
}

@end


@interface SFAnalyticsTests : XCTestCase
@end

@implementation SFAnalyticsTests
{
    UnitTestAnalytics* _analytics;
    NSString* _dbpath;
    PQLConnection* _db;
}

static NSString* _path;
static NSInteger _testnum;
static NSString* build = NULL;
static NSString* product = NULL;
static NSString* modelID = nil;

// MARK: Test helper methods

- (void)assertNoSuccessEvents
{
    XCTAssertFalse([[_db fetch:@"select * from success_count"] next]);
}

- (void)assertNoHardFailures
{
    XCTAssertFalse([[_db fetch:@"select * from hard_failures"] next]);
}

- (void)assertNoSoftFailures
{
    XCTAssertFalse([[_db fetch:@"select * from soft_failures"] next]);
}

- (void)assertNoNotes
{
    XCTAssertFalse([[_db fetch:@"select * from notes"] next]);
}

- (void)assertNoSamples
{
    XCTAssertFalse([[_db fetch:@"select * from samples"] next]);
}

- (void)assertNoEventsAnywhere
{
    [self assertNoNotes];
    [self assertNoSuccessEvents];
    [self assertNoHardFailures];
    [self assertNoSoftFailures];
    [self assertNoSamples];
}

- (void)recentTimeStamp:(NSTimeInterval)timestamp timestampBucket:(SFAnalyticsTimestampBucket)bucket
{
    NSTimeInterval roundedTimestamp = [[NSDate date] timeIntervalSince1970WithBucket:bucket];
    XCTAssertEqualWithAccuracy(roundedTimestamp, timestamp, 5, @"Timestamp is pretty recent (%f ~? %f)", timestamp, roundedTimestamp);
}

- (void)properEventLogged:(PQLResultSet*)result eventType:(NSString*)eventType class:(SFAnalyticsEventClass)class attributes:(NSDictionary*)attrs
{
    [self _properEventLogged:result eventType:eventType class:class];
    
    NSDictionary* rowdata = [NSPropertyListSerialization propertyListWithData:[result dataAtIndex:2] options:NSPropertyListImmutable format:nil error:nil];
    for (NSString* key in [attrs allKeys]) {
        XCTAssert([attrs[key] isEqualToString:rowdata[key]], @"Attribute \"%@\" value \"%@\" matches expected \"%@\"", key, rowdata[key], attrs[key]);
    }
    XCTAssertFalse([result next], @"only one row returned");
}

- (void)properEventLogged:(PQLResultSet*)result eventType:(NSString*)eventType class:(SFAnalyticsEventClass)class
{
    [self _properEventLogged:result eventType:eventType class:class];
    XCTAssertFalse([result next], @"only one row returned");
}

- (void)properEventLogged:(PQLResultSet*)result eventType:(NSString*)eventType class:(SFAnalyticsEventClass)class timestampBucket:(SFAnalyticsTimestampBucket)bucket
{
    [self _properEventLogged:result eventType:eventType class:class timestampBucket:bucket];
    XCTAssertFalse([result next], @"only one row returned");
}

- (void)_properEventLogged:(PQLResultSet*)result eventType:(NSString*)eventType class:(SFAnalyticsEventClass)class
{
    [self _properEventLogged:result eventType:eventType class:class timestampBucket:SFAnalyticsTimestampBucketSecond];
}

- (void)_properEventLogged:(PQLResultSet*)result eventType:(NSString*)eventType class:(SFAnalyticsEventClass)class timestampBucket:(SFAnalyticsTimestampBucket)bucket
{
    XCTAssert([result next], @"result found after adding an event");
    NSError* error = nil;
    [result doubleAtIndex:1];
    NSDictionary* rowdata = [NSPropertyListSerialization propertyListWithData:[result dataAtIndex:2] options:NSPropertyListImmutable format:nil error:&error];
    XCTAssertNotNil(rowdata, @"able to deserialize db data, %@", error);
    [self assertIsProperRecord:rowdata eventType:eventType class:class timestampBucket:bucket];
}

- (void)assertIsProperRecord:(NSDictionary*)rowdata eventType:(NSString*)eventType class:(SFAnalyticsEventClass)class timestampBucket:(SFAnalyticsTimestampBucket)bucket
{
    NSNumber *timestamp = rowdata[SFAnalyticsEventTime];
    XCTAssert([timestamp isKindOfClass:[NSNumber class]], @"Timestamp is an NSNumber");
    [self recentTimeStamp:([timestamp doubleValue] / 1000) timestampBucket:bucket]; // We need to convert to seconds, as it's stored in milliseconds
    [self assertIsProperRecord:rowdata eventType:eventType class:class];
}

- (void)assertIsProperRecord:(NSDictionary*)rowdata eventType:(NSString*)eventType class:(SFAnalyticsEventClass)class
{
    XCTAssertTrue([rowdata[SFAnalyticsEventType] isKindOfClass:[NSString class]] && [rowdata[SFAnalyticsEventType] isEqualToString:eventType], @"found eventType \"%@\" in db", eventType);
    XCTAssertTrue([rowdata[SFAnalyticsEventClassKey] isKindOfClass:[NSNumber class]] && [rowdata[SFAnalyticsEventClassKey] intValue] == class, @"eventClass is %ld", (long)class);
    XCTAssertTrue([rowdata[@"build"] isEqualToString:build], @"event row includes build");
    XCTAssertTrue([rowdata[@"product"] isEqualToString:product], @"event row includes product");
    XCTAssertTrue([rowdata[@"modelid"] isEqualToString:modelID], @"event row includes modelid");
    XCTAssertTrue(rowdata[@"internal"], @"event row includes internal");
}

- (void)checkSuccessCountsForEvent:(NSString*)eventType success:(int)success hard:(int)hard soft:(int)soft
{
    PQLResultSet* result = [_db fetch:@"select * from success_count where event_type = %@", eventType];
    XCTAssert([result next]);
    XCTAssertTrue([[result stringAtIndex:0] isEqualToString:eventType], @"event name \"%@\", expected \"%@\"", [result stringAtIndex:0], eventType);
    XCTAssertEqual([result intAtIndex:1], success, @"correct count of successes: %d / %d", [result intAtIndex:1], success);
    XCTAssertEqual([result intAtIndex:2], hard, @"correct count of successes: %d / %d", [result intAtIndex:2], hard);
    XCTAssertEqual([result intAtIndex:3], soft, @"correct count of successes: %d / %d", [result intAtIndex:3], soft);
    XCTAssertFalse([result next], @"no more than one row returned");
}

- (void)checkSamples:(NSArray*)samples name:(NSString*)samplerName totalSamples:(NSUInteger)total accuracy:(double)accuracy
{
    // before checking the database for log entries, make sure they made it to the disk
    [_analytics drainLogQueue];

    NSUInteger samplescount = 0, targetcount = 0;
    NSMutableArray* samplesfound = [NSMutableArray array];
    PQLResultSet* result = [_db fetch:@"select * from samples"];
    while ([result next]) {
        ++samplescount;
        [self recentTimeStamp:[[result numberAtIndex:1] doubleValue] timestampBucket:SFAnalyticsTimestampBucketSecond];
        if ([[result stringAtIndex:2] isEqual:samplerName]) {
            ++targetcount;
            [samplesfound addObject:[result numberAtIndex:3]];
        }
    }

    XCTAssertEqual([samples count], targetcount);
    XCTAssertEqual(samplescount, total);

    [samplesfound sortUsingSelector:@selector(compare:)];
    NSArray* sortedInput = [samples sortedArrayUsingSelector:@selector(compare:)];
    for (NSUInteger idx = 0; idx < [samples count]; ++idx) {
        XCTAssertEqualWithAccuracy([samplesfound[idx] doubleValue], [sortedInput[idx] doubleValue], accuracy);
    }
}

- (void)waitForSamplerWork:(double)interval
{
    dispatch_semaphore_t sema = dispatch_semaphore_create(0);
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC * interval), dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_HIGH, 0), ^{
        dispatch_semaphore_signal(sema);
    });
    dispatch_semaphore_wait(sema, DISPATCH_TIME_FOREVER);
}

// MARK: Test administration

+ (void)setUp
{
    NSError* error;
    _path = [NSTemporaryDirectory() stringByAppendingPathComponent:[NSString stringWithFormat:@"%@/", [[NSUUID UUID] UUIDString]]];
    [[NSFileManager defaultManager] createDirectoryAtPath:_path
                              withIntermediateDirectories:YES
                                               attributes:nil
                                                    error:&error];
    // No XCTAssert in class method
    if (error) {
        NSLog(@"Could not make directory at %@", _path);
    }
    NSDictionary *version = CFBridgingRelease(_CFCopySystemVersionDictionary());
    if (version) {
        build = version[(__bridge NSString *)_kCFSystemVersionBuildVersionKey];
        product = version[(__bridge NSString *)_kCFSystemVersionProductNameKey];
    } else {
        NSLog(@"could not get build version/product, tests should fail");
    }

    modelID = [SFAnalytics hwModelID];

    [TestResourceUsage monitorTestResourceUsage];
}

- (void)setUp
{
    [super setUp];
    self.continueAfterFailure = NO;
    NSError* error = nil;
    _dbpath = [_path stringByAppendingFormat:@"/test_%ld.db", (long)++_testnum];
    NSLog(@"sqlite3 %@", _dbpath);
    [UnitTestAnalytics setDatabasePath:_dbpath];
    _analytics = [UnitTestAnalytics logger];
    _db = [PQLConnection new];

    XCTAssertTrue([_db openAtURL:[NSURL URLWithString:_dbpath] sharedCache:NO error:&error]);
    XCTAssertNil(error, @"could open db");
    XCTAssertNotNil(_db);
}

- (void)tearDown
{
    NSError *error = nil;
    XCTAssertTrue([_db close:&error], @"could close db");
    XCTAssertNil(error, @"No error from closing db");
    [_analytics removeState];
    [super tearDown];
}

+ (void)tearDown
{
    [[NSFileManager defaultManager] removeItemAtPath:_path error:nil];
}

// MARK: SFAnalytics Tests

- (void)testDbIsEmptyAtStartup
{
    [self assertNoEventsAnywhere];
}

- (void)testDontCrashWithEmptyDBPath
{
    NSString* schema = @"CREATE TABLE IF NOT EXISTS test (id INTEGER PRIMARY KEY AUTOINCREMENT,data BLOB);";
    NSString* path = [NSString stringWithFormat:@"%@/empty", _path];

    XCTAssertNil([SFAnalyticsSQLiteStore storeWithPath:@"" schema:schema]);
    XCTAssertNil([SFAnalyticsSQLiteStore storeWithPath:path schema:@""]);

    XCTAssertNil([[SFSQLite alloc] initWithPath:nil schema:schema]);
    XCTAssertNil([[SFSQLite alloc] initWithPath:@"" schema:schema]);
    XCTAssertNil([[SFSQLite alloc] initWithPath:path schema:nil]);
    XCTAssertNil([[SFSQLite alloc] initWithPath:path schema:@""]);
}

- (void)testAddingEventsWithNilName
{
#ifndef __clang_analyzer__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnonnull"
    [_analytics logSuccessForEventNamed:nil];
    [self assertNoEventsAnywhere];

    [_analytics logHardFailureForEventNamed:nil withAttributes:nil];
    [self assertNoEventsAnywhere];

    [_analytics logSoftFailureForEventNamed:nil withAttributes:nil];
    [self assertNoEventsAnywhere];

    [_analytics noteEventNamed:nil];
    [self assertNoEventsAnywhere];
#pragma clang diagnostic pop
#endif // __clang_analyzer__
}

- (void)testLogSuccess
{
    [_analytics logSuccessForEventNamed:@"unittestevent"];
    [self assertNoHardFailures];
    [self assertNoSoftFailures];

    PQLResultSet* result = [_db fetch:@"select success_count from success_count"];
    XCTAssert([result next], @"a row was found after adding an event");
    XCTAssertEqual([result intAtIndex:0], 1, @"success count is 1 after adding an event");
    XCTAssertFalse([result next], @"only one row found in success_count after inserting a single event");
    [self assertNoNotes];
    [self assertNoHardFailures];
    [self assertNoSoftFailures];
}

- (void)testLogWithRoundedTimestamp
{
    SFAnalyticsTimestampBucket bucket = SFAnalyticsTimestampBucketMinute;

    [_analytics logSoftFailureForEventNamed:@"unittestevent" withAttributes:nil timestampBucket:bucket];
    [self assertNoHardFailures];

    // First check success_count has logged a soft failure
    [self checkSuccessCountsForEvent:@"unittestevent" success:0 hard:0 soft:1];

    // then check soft_failures itself
    PQLResultSet* result = [_db fetch:@"select * from soft_failures"];
    [self properEventLogged:result eventType:@"unittestevent" class:SFAnalyticsEventClassSoftFailure timestampBucket:bucket];
    [self assertNoNotes];
    [self assertNoHardFailures];

    NSArray *allEvents = _analytics.database.allEvents;
    XCTAssertEqual(allEvents.count, 1, @"Store should contain one event");
    [self assertIsProperRecord:allEvents.firstObject eventType:@"unittestevent" class:SFAnalyticsEventClassSoftFailure timestampBucket:bucket];
    NSArray *softFailures = _analytics.database.softFailures;
    XCTAssertEqualObjects(allEvents, softFailures, @"Store should contain one soft failure");
    XCTAssertEqual(_analytics.database.hardFailures.count, 0, @"Store should not contain any hard failures");
}

- (void)testLogRecoverableFailure
{
    [_analytics logSoftFailureForEventNamed:@"unittestevent" withAttributes:nil];
    [self assertNoHardFailures];

    // First check success_count has logged a soft failure
    [self checkSuccessCountsForEvent:@"unittestevent" success:0 hard:0 soft:1];

    // then check soft_failures itself
    PQLResultSet* result = [_db fetch:@"select * from soft_failures"];
    [self properEventLogged:result eventType:@"unittestevent" class:SFAnalyticsEventClassSoftFailure];
    [self assertNoNotes];
    [self assertNoHardFailures];
}

- (void)testLogRecoverablyFailureWithAttributes
{
    NSDictionary* attrs = @{@"attr1" : @"value1", @"attr2" : @"value2"};
    [_analytics logSoftFailureForEventNamed:@"unittestevent" withAttributes:attrs];
    [self assertNoHardFailures];

    [self checkSuccessCountsForEvent:@"unittestevent" success:0 hard:0 soft:1];

    // then check soft_failures itself
    PQLResultSet* result = [_db fetch:@"select * from soft_failures"];
    [self properEventLogged:result eventType:@"unittestevent" class:SFAnalyticsEventClassSoftFailure attributes:attrs];
    [self assertNoNotes];
    [self assertNoHardFailures];
}

- (void)testLogUnrecoverableFailure
{
    [_analytics logHardFailureForEventNamed:@"unittestevent" withAttributes:nil];
    [self assertNoSoftFailures];

    // First check success_count has logged a hard failure
    [self checkSuccessCountsForEvent:@"unittestevent" success:0 hard:1 soft:0];

    // then check hard_failures itself
    PQLResultSet* result = [_db fetch:@"select * from hard_failures"];
    [self properEventLogged:result eventType:@"unittestevent" class:SFAnalyticsEventClassHardFailure];
    [self assertNoNotes];
    [self assertNoSoftFailures];

    NSArray *allEvents = _analytics.database.allEvents;
    XCTAssertEqual(allEvents.count, 1, @"Store should contain one event");
    [self assertIsProperRecord:allEvents.firstObject eventType:@"unittestevent" class:SFAnalyticsEventClassHardFailure];
    NSArray *hardFailures = _analytics.database.hardFailures;
    XCTAssertEqualObjects(allEvents, hardFailures, @"Store should contain one hard failure");
    XCTAssertEqual(_analytics.database.softFailures.count, 0, @"Store should not contain any soft failures");
}

- (void)testLogUnrecoverableFailureWithAttributes
{
    NSDictionary* attrs = @{@"attr1" : @"value1", @"attr2" : @"value2"};
    [_analytics logHardFailureForEventNamed:@"unittestevent" withAttributes:attrs];
    [self assertNoSoftFailures];

    // First check success_count has logged a hard failure
    [self checkSuccessCountsForEvent:@"unittestevent" success:0 hard:1 soft:0];

    // then check hard_failures itself
    PQLResultSet* result = [_db fetch:@"select * from hard_failures"];
    [self properEventLogged:result eventType:@"unittestevent" class:SFAnalyticsEventClassHardFailure attributes:attrs];
    [self assertNoNotes];
    [self assertNoSoftFailures];
}

- (void)testLogSeveralEvents
{
    NSDictionary* attrs = @{@"attr1" : @"value1", @"attr2" : @"value2"};
    int iterations = 100;
    for (int idx = 0; idx < iterations; ++idx) {
        [_analytics logHardFailureForEventNamed:@"unittesthardfailure" withAttributes:attrs];
        [_analytics logSoftFailureForEventNamed:@"unittestsoftfailure" withAttributes:attrs];
        [_analytics logSuccessForEventNamed:@"unittestsuccess"];
        [_analytics logHardFailureForEventNamed:@"unittestcombined" withAttributes:attrs];
        [_analytics logSoftFailureForEventNamed:@"unittestcombined" withAttributes:attrs];
        [_analytics logSuccessForEventNamed:@"unittestcombined"];
    }
    
    [self checkSuccessCountsForEvent:@"unittesthardfailure" success:0 hard:iterations soft:0];
    [self checkSuccessCountsForEvent:@"unittestsoftfailure" success:0 hard:0 soft:iterations];
    [self checkSuccessCountsForEvent:@"unittestsuccess" success:iterations hard:0 soft:0];
    [self checkSuccessCountsForEvent:@"unittestcombined" success:iterations hard:iterations soft:iterations];
}

- (void)testNoteEvent
{
    [_analytics noteEventNamed:@"unittestevent"];
    [self assertNoSoftFailures];
    [self assertNoHardFailures];

    // First check success_count has logged a success
    [self checkSuccessCountsForEvent:@"unittestevent" success:1 hard:0 soft:0];

    PQLResultSet* result = [_db fetch:@"select * from notes"];
    [self properEventLogged:result eventType:@"unittestevent" class:SFAnalyticsEventClassNote];

    NSArray *allEvents = _analytics.database.allEvents;
    XCTAssertEqual(allEvents.count, 1, @"Store should contain one event");
    [self assertIsProperRecord:allEvents.firstObject eventType:@"unittestevent" class:SFAnalyticsEventClassNote];
}

// MARK: SFAnalyticsSampler Tests

- (void)testSamplerSimple
{
    NSString* samplerName = [NSString stringWithFormat:@"UnitTestSamplerSimple_%li", (long)_testnum];
    
    // This block should be set immediately and fire in 1000ms. Give it a little slack in checking though
    XCTestExpectation* exp = [self expectationWithDescription:@"waiting for sampler to fire"];
    [_analytics addMetricSamplerForName:samplerName withTimeInterval:1.0f block:^NSNumber *{
        [exp fulfill];
        return @15.3;
    }];
    [self waitForExpectations:@[exp] timeout:1.2f];
    [_analytics removeMetricSamplerForName:samplerName];

    // The expectation is fulfilled before returning and after returning some more work needs to happen. Let it settle down.
    [self waitForSamplerWork:0.2f];

    [self checkSamples:@[@15.3] name:samplerName totalSamples:1 accuracy:0.01f];
}

// Test state removal mostly
- (void)testSamplerSimpleLoop
{
    [self tearDown];
    for (int idx = 0; idx < 3; ++idx) {
        [self setUp];
        @autoreleasepool {
            [self testSamplerSimple];
        }
        [self tearDown];
    }
}


- (void)testSamplerDoesNotFirePrematurely
{
    NSString* samplerName = [NSString stringWithFormat:@"UnitTestSamplerDoesNotFirePrematurely_%li", (long)_testnum];
    __block BOOL run = NO;
    [_analytics addMetricSamplerForName:samplerName withTimeInterval:1.0f block:^NSNumber *{
        run = YES;
        return @0.9;
    }];
    
    [self waitForSamplerWork:0.5f];
    XCTAssertFalse(run, @"sample did not fire prematurely");
    [_analytics removeMetricSamplerForName:samplerName];
}

- (void)testSamplerRemove
{
    NSString* samplerName = [NSString stringWithFormat:@"UnitTestSamplerRemove_%li", (long)_testnum];
    __block BOOL run = NO;
    [_analytics addMetricSamplerForName:samplerName withTimeInterval:1.0f block:^NSNumber *{
        run = YES;
        return @23.8;
    }];
    XCTAssertNotNil([_analytics existingMetricSamplerForName:samplerName], @"SFAnalytics held onto the sampler we setup");
    [_analytics removeMetricSamplerForName:samplerName];
    XCTAssertNil([_analytics existingMetricSamplerForName:samplerName], @"SFAnalytics got rid of our sampler");
    
    [self waitForSamplerWork:2.0f];
    XCTAssertFalse(run, @"sampler did not run after removal");
}

- (void)testSamplerRepeatedSampling
{
    NSString* samplerName = [NSString stringWithFormat:@"UnitTestSamplerRepeatedSampling_%li", (long)_testnum];
    __block int run = 0;
    [_analytics addMetricSamplerForName:samplerName withTimeInterval:1.0f block:^NSNumber *{
        run += 1;
        return @1.5;
    }];
    
    [self waitForSamplerWork:3.5f];
    [_analytics removeMetricSamplerForName:samplerName];
    XCTAssertEqual(run, 3, @"sampler ran correct number of times");
    [self checkSamples:@[@1.5, @1.5, @1.5] name:samplerName totalSamples:3 accuracy:0.01f];
}

- (void)testSamplerDisable
{
    NSString* samplerName = [NSString stringWithFormat:@"UnitTestSamplerDisable_%li", (long)_testnum];
    __block int run = 0;
    [_analytics addMetricSamplerForName:samplerName withTimeInterval:1.0f block:^NSNumber *{
        run += 1;
        return @44.9;
    }];
    
    [[_analytics existingMetricSamplerForName:samplerName] pauseSampling];
    [self waitForSamplerWork:2.0f];
    XCTAssertEqual(run, 0, @"sampler did not run while disabled");
    
    [[_analytics existingMetricSamplerForName:samplerName] resumeSampling];
    [self waitForSamplerWork:1.3f];
    XCTAssertEqual(run, 1, @"sampler ran after resuming");
    
    [self checkSamples:@[@44.9] name:samplerName totalSamples:1 accuracy:0.01f];
}

- (void)testSamplerWithBadData
{
#ifndef __clang_analyzer__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnonnull"
    NSString* samplerName = [NSString stringWithFormat:@"UnitTestSamplerWithBadData_%li", (long)_testnum];

    // bad name
    XCTAssertNil([_analytics addMetricSamplerForName:nil withTimeInterval:3.0f block:^NSNumber *{
        return @0.0;
    }]);

    // bad interval
    XCTAssertNil([_analytics addMetricSamplerForName:samplerName withTimeInterval:0.0f block:^NSNumber *{
        return @0.0;
    }]);

    XCTAssertNil([_analytics addMetricSamplerForName:samplerName withTimeInterval:2.0f block:nil]);
#pragma clang diagnostic pop
#endif // __clang_analyzer__
}

- (void)testSamplerOncePerReport
{
    NSString* samplerName = [NSString stringWithFormat:@"UnitTestSamplerOncePerReport_%li", (long)_testnum];
    __block int run = 0;
    [_analytics addMetricSamplerForName:samplerName withTimeInterval:SFAnalyticsSamplerIntervalOncePerReport block:^NSNumber *{
        run += 1;
        return @74.1;
    }];

    // There's no point in waiting, it could have been set to some arbitrarily long timer instead
    
    notify_post(SFAnalyticsFireSamplersNotification);
    [self waitForSamplerWork:0.5f];
    XCTAssertEqual(run, 1, @"once-per-report sampler fired once in response to notification");
    [self checkSamples:@[@74.1] name:samplerName totalSamples:1 accuracy:0.01f];
}

- (void)testSamplerOncePerReportEnsuresSingleSampleInDatabase
{
    NSString* samplerName = [NSString stringWithFormat:@"UnitTestSamplerSetTimeInterval_%li", (long)_testnum];
    [_analytics addMetricSamplerForName:samplerName withTimeInterval:SFAnalyticsSamplerIntervalOncePerReport block:^NSNumber *{
        return @57.6;
    }];
    notify_post(SFAnalyticsFireSamplersNotification);
    [self waitForSamplerWork:0.5f];
    notify_post(SFAnalyticsFireSamplersNotification);
    [self waitForSamplerWork:0.5f];
    [self checkSamples:@[@57.6] name:samplerName totalSamples:1 accuracy:0.01f];
}

- (void)testSamplerAddSamplerTwice
{
    NSString* samplerName = [NSString stringWithFormat:@"UnitTestSamplerDisable_%li", (long)_testnum];
    XCTAssertNotNil([_analytics addMetricSamplerForName:samplerName withTimeInterval:3.0f block:^NSNumber *{
        return @7.7;
    }], @"adding first sampler works okay");
    
    XCTAssertNil([_analytics addMetricSamplerForName:samplerName withTimeInterval:3.0f block:^NSNumber *{
        return @7.8;
    }], @"adding duplicate sampler did not work");
}

- (void)testSamplerLogBadSample
{
#ifndef __clang_analyzer__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnonnull"
    [_analytics logMetric:nil withName:@"testsampler"];
    [self checkSamples:@[] name:@"testsampler" totalSamples:0 accuracy:0.01f];
#pragma clang diagnostic pop
#endif // __clang_analyzer__

    id badobj = [NSString stringWithUTF8String:"yolo!"];
    [_analytics logMetric:badobj withName:@"testSampler"];
    [self checkSamples:@[] name:@"testsampler" totalSamples:0 accuracy:0.01f];
}

- (void)testSamplerSetTimeInterval
{
    NSString* samplerName = [NSString stringWithFormat:@"UnitTestSamplerSetTimeInterval_%li", (long)_testnum];
    __block NSUInteger run = 0;

    [_analytics addMetricSamplerForName:samplerName withTimeInterval:1.0f block:^NSNumber *{
        ++run;
        return @23.8;
    }];
    [self waitForSamplerWork:1.2f];
    [_analytics existingMetricSamplerForName:samplerName].samplingInterval = 1.5f;
    [self waitForSamplerWork:2.5f];
    XCTAssertEqual(run, 2ul);
    [self checkSamples:@[@23.8, @23.8] name:samplerName totalSamples:2 accuracy:0.01f];
}

// MARK: SFAnalyticsMultiSampler Tests

- (void)testMultiSamplerSimple
{
    NSString* samplerName = [NSString stringWithFormat:@"UnitTestMultiSamplerSimple_%li", (long)_testnum];

    XCTestExpectation* exp = [self expectationWithDescription:@"waiting for sampler to fire"];
    [_analytics AddMultiSamplerForName:samplerName withTimeInterval:1.0f block:^NSDictionary<NSString *,NSNumber *> *{
        [exp fulfill];
        return @{@"val1" : @89.4f, @"val2" : @11.2f};
    }];
    [self waitForExpectations:@[exp] timeout:1.3f];
    [_analytics removeMultiSamplerForName:samplerName];

    // The expectation is fulfilled before returning and after returning some more work needs to happen. Let it settle down.
    [self waitForSamplerWork:0.2f];

    [self checkSamples:@[@89.4f] name:@"val1" totalSamples:2 accuracy:0.01f];
    [self checkSamples:@[@11.2f] name:@"val2" totalSamples:2 accuracy:0.01f];
}

- (void)testMultiSamplerOncePerReport
{
    NSString* samplerName = [NSString stringWithFormat:@"UnitTestMultiSamplerOncePerReport_%li", (long)_testnum];
    __block int run = 0;
    [_analytics AddMultiSamplerForName:samplerName withTimeInterval:SFAnalyticsSamplerIntervalOncePerReport block:^NSDictionary<NSString *,NSNumber *> *{
        run += 1;
        return @{@"val1" : @33.8f, @"val2" : @54.6f};
    }];

    // There's no point in waiting, it could have been set to some arbitrarily long timer instead

    notify_post(SFAnalyticsFireSamplersNotification);
    [self waitForSamplerWork:1.0f];
    XCTAssertEqual(run, 1, @"once-per-report sampler fired once in response to notification");
    [self checkSamples:@[@33.8f] name:@"val1" totalSamples:2 accuracy:0.01f];
    [self checkSamples:@[@54.6f] name:@"val2" totalSamples:2 accuracy:0.01f];
}

- (void)testMultiSamplerSetTimeInterval
{
    NSString* samplerName = [NSString stringWithFormat:@"UnitTestMultiSamplerSetTimeInterval_%li", (long)_testnum];
    __block NSUInteger run = 0;
    [_analytics AddMultiSamplerForName:samplerName withTimeInterval:1.0f block:^NSDictionary<NSString *,NSNumber *> *{
        ++run;
        return @{@"val1" : @29.3f, @"val2" : @19.3f};
    }];
    [self waitForSamplerWork:1.2f];
    [_analytics existingMultiSamplerForName:samplerName].samplingInterval = 1.5f;
    [self waitForSamplerWork:2.5f];
    XCTAssertEqual(run, 2ul);
    [self checkSamples:@[@29.3f, @29.3f] name:@"val1" totalSamples:4 accuracy:0.01f];
    [self checkSamples:@[@19.3f, @19.3f] name:@"val2" totalSamples:4 accuracy:0.01f];
}


// MARK: SFAnalyticsActivityTracker Tests

- (void)testTrackerSimple
{
    NSString* trackerName = @"UnitTestTrackerSimple";
    @autoreleasepool {
        [_analytics logSystemMetricsForActivityNamed:trackerName withAction:^{
            [NSThread sleepForTimeInterval:0.3f];
        }];
    }


    [self checkSamples:@[@(0.3f * NSEC_PER_SEC)] name:trackerName totalSamples:1 accuracy:(0.05f * NSEC_PER_SEC)];
}

- (void)testTrackerMultipleBlocks
{
    NSString* trackerName = @"UnitTestTrackerMultipleBlocks";
    @autoreleasepool {
        SFAnalyticsActivityTracker* tracker = [_analytics logSystemMetricsForActivityNamed:trackerName withAction:^{
            [NSThread sleepForTimeInterval:0.3f];
        }];

        [tracker performAction:^{
            [NSThread sleepForTimeInterval:0.2f];
        }];
    }

    [self checkSamples:@[@(0.5f * NSEC_PER_SEC)] name:trackerName totalSamples:1 accuracy:(0.1f * NSEC_PER_SEC)];
}

- (void)testTrackerAction
{
    NSString* trackerName = @"UnitTestTrackerOneBlock";
    @autoreleasepool {
        SFAnalyticsActivityTracker* tracker = [_analytics logSystemMetricsForActivityNamed:trackerName withAction:NULL];
        [tracker performAction:^{
            [NSThread sleepForTimeInterval:0.2f];
        }];
    }

    [self checkSamples:@[@(0.2f * NSEC_PER_SEC)] name:trackerName totalSamples:1 accuracy:(0.1f * NSEC_PER_SEC)];
}

- (void)testTrackerStartStop {

    NSString* trackerName = @"UnitTestTrackerStartStop";
    @autoreleasepool {
        SFAnalyticsActivityTracker* tracker = [_analytics logSystemMetricsForActivityNamed:trackerName withAction:NULL];
        [tracker start];
        [NSThread sleepForTimeInterval:0.2f];
        [tracker stop];
    }

    [self checkSamples:@[@(0.2f * NSEC_PER_SEC)] name:trackerName totalSamples:1 accuracy:(0.1f * NSEC_PER_SEC)];
}

- (void)testTrackerCancel
{
    NSString* trackerName = @"UnitTestTrackerCancel";
    @autoreleasepool {
        [[_analytics logSystemMetricsForActivityNamed:trackerName withAction:^{
            [NSThread sleepForTimeInterval:0.3f];
        }] cancel];
    }

    [self assertNoEventsAnywhere];
}

- (void)testTrackerBadData
{
#ifndef __clang_analyzer__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnonnull"
    // Inspect database to find out it's empty
    [_analytics logMetric:nil withName:@"fake"];
    [_analytics logMetric:@3.0 withName:nil];

    // get object back so inspect that, too
    XCTAssertNil([_analytics logSystemMetricsForActivityNamed:nil withAction:^{return;}]);

    [self assertNoEventsAnywhere];
#pragma clang diagnostic pop
#endif // __clang_analyzer__
}

// MARK: Underlying error generation

- (void)testUnderlyingNoError {
    NSError *e = [NSError errorWithDomain:@"foo" code:1 userInfo:@{}];
    XCTAssertNil([SFAnalytics underlyingErrors:e]);
}

- (void)testUnderlyingError {
    NSError *e = [NSError errorWithDomain:@"foo" code:1 userInfo:@{
        NSUnderlyingErrorKey: [NSError errorWithDomain:@"bar" code:2 userInfo:nil],
    }];
    XCTAssertEqualObjects([SFAnalytics underlyingErrors:e], @"{\"u\":{\"c\":2,\"d\":\"bar\"}}");
}

- (void)testUnderlyingMultipleErrors {
    NSError *e = [NSError errorWithDomain:@"foo" code:1 userInfo:@{
        NSMultipleUnderlyingErrorsKey: @[ 
            [NSError errorWithDomain:@"bar" code:2 userInfo:nil],
            [NSError errorWithDomain:@"baz" code:3 userInfo:nil]
        ],
    }];
    XCTAssertEqualObjects([SFAnalytics underlyingErrors:e], @"{\"m\":[{\"c\":2,\"d\":\"bar\"},{\"c\":3,\"d\":\"baz\"}]}");
}

- (void)testUnderlyingBrokeError {
    NSError *e = [NSError errorWithDomain:@"foo" code:1 userInfo:@{
        NSUnderlyingErrorKey: @YES,
    }];
    XCTAssertNil([SFAnalytics underlyingErrors:e]);
}

- (void)testUnderlyingBrokeMultipleErrors {
    NSError *e = [NSError errorWithDomain:@"foo" code:1 userInfo:@{
        NSMultipleUnderlyingErrorsKey: @YES,
    }];
    XCTAssertNil([SFAnalytics underlyingErrors:e]);
}

- (void)testUnderlyingBrokeMultipleErrorsInner {
    NSError *e = [NSError errorWithDomain:@"foo" code:1 userInfo:@{
        NSMultipleUnderlyingErrorsKey: @[
            [NSError errorWithDomain:@"bar" code:2 userInfo:nil],
            @YES,
        ],
    }];
    XCTAssertEqualObjects([SFAnalytics underlyingErrors:e], @"{\"m\":[{\"c\":2,\"d\":\"bar\"}]}");
}


// MARK: Miscellaneous

- (void)testInstantiateBaseClass
{
    XCTAssertNil([SFAnalytics logger]);
}

- (void)testFuzzyDaysSinceDate
{
    NSInteger secondsPerDay = 60 * 60 * 24;
    XCTAssertEqual([SFAnalytics fuzzyDaysSinceDate:[NSDate date]], 0);
    XCTAssertEqual([SFAnalytics fuzzyDaysSinceDate:[NSDate dateWithTimeIntervalSinceNow:secondsPerDay * -3]], 1);
    XCTAssertEqual([SFAnalytics fuzzyDaysSinceDate:[NSDate dateWithTimeIntervalSinceNow:secondsPerDay * -18]], 7);
    XCTAssertEqual([SFAnalytics fuzzyDaysSinceDate:[NSDate dateWithTimeIntervalSinceNow:secondsPerDay * -77]], 30);
    XCTAssertEqual([SFAnalytics fuzzyDaysSinceDate:[NSDate dateWithTimeIntervalSinceNow:secondsPerDay * -370]], 365);
    XCTAssertEqual([SFAnalytics fuzzyDaysSinceDate:[NSDate distantPast]], 1000);
#ifndef __clang_analyzer__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnonnull"
    XCTAssertEqual([SFAnalytics fuzzyDaysSinceDate:nil], -1);
#pragma clang diagnostic pop
#endif // __clang_analyzer__
}

- (void)testRingBuffer {
    [self assertNoEventsAnywhere];
    for (unsigned idx = 0; idx < (SFAnalyticsMaxEventsToReport + 50); ++idx) {
        [_analytics logHardFailureForEventNamed:@"ringbufferevent" withAttributes:nil];
    }

    PQLResultSet* result = [_db fetch:@"select count(*) from hard_failures"];
    XCTAssertTrue([result next], @"Got a count from hard_failures");
    XCTAssertLessThanOrEqual([result unsignedIntAtIndex:0], SFAnalyticsMaxEventsToReport, @"Ring buffer does not exceed the maximum number of events");
}

- (void)testRaceToCreateLoggers
{
    dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);
    for (NSInteger idx = 0; idx < 500; ++idx) {
        dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
            UnitTestAnalytics* logger = [UnitTestAnalytics logger];
            [logger logSuccessForEventNamed:@"testevent"];
            dispatch_semaphore_signal(semaphore);
        });
    }

    for (NSInteger idx = 0; idx < 500; ++idx) {
        dispatch_semaphore_wait(semaphore, DISPATCH_TIME_FOREVER);
    }
}

- (void)testDateProperty
{
    NSString* propertyKey = @"testDataPropertyKey";
    XCTAssertNil([_analytics datePropertyForKey:propertyKey]);
    NSDate* test = [NSDate date];
    [_analytics setDateProperty:test forKey:propertyKey];
    NSDate* retrieved = [_analytics datePropertyForKey:propertyKey];
    XCTAssertNotNil(retrieved);
    // Storing in SQLite as string loses subsecond resolution, so we need some slack
    XCTAssertEqualWithAccuracy([test timeIntervalSinceDate:retrieved], 0, 1);

    [_analytics setDateProperty:nil forKey:propertyKey];
    XCTAssertNil([_analytics datePropertyForKey:propertyKey]);
}

- (void)testMetricsAccountID
{
    NSString* accountID = @"accountID";

    XCTAssertNil([_analytics metricsAccountID], "should be nil before set");

    [_analytics setMetricsAccountID:accountID];
    NSString *value = [_analytics metricsAccountID];
    XCTAssertEqualObjects(value, accountID);

    [_analytics setMetricsAccountID:nil];
    XCTAssertNil([_analytics metricsAccountID], "should be nil before clear");
}

// MARK: Metrics hooks

- (void)testMetricsHooks
{
    XCTestExpectation *e = [self expectationWithDescription:@"hook"];

    [_analytics addMetricsHook:^SFAnalyticsMetricsHookActions(NSString * _Nonnull eventName, SFAnalyticsEventClass eventClass, NSDictionary * _Nonnull attributes, SFAnalyticsTimestampBucket timestampBucket) {
        XCTAssertEqualObjects(eventName, @"event");
        XCTAssertEqualObjects(attributes[@"a"], @1);
        XCTAssertEqualObjects(attributes[@"errorDomain"], @"domain");
        XCTAssertEqualObjects(attributes[@"errorCode"], @1);
        XCTAssertEqual(eventClass, SFAnalyticsEventClassHardFailure);
        [e fulfill];
        return 0;
    }];

    NSError *error = [NSError errorWithDomain:@"domain" code:1 userInfo:nil];
    [_analytics logResultForEvent:@"event" hardFailure:YES result:error withAttributes:@{
        @"a": @1,
    }];

    [self waitForExpectations:@[e] timeout:1.0];

    PQLResultSet* result = [_db fetch:@"select count(*) from hard_failures"];
    XCTAssertTrue([result next], @"Got a count from hard_failures");
    XCTAssertLessThanOrEqual([result unsignedIntAtIndex:0], 1, @"Should have one event");

}

- (void)testMetricsHooksDrop
{
    XCTestExpectation *e = [self expectationWithDescription:@"hook"];

    [_analytics addMetricsHook:^SFAnalyticsMetricsHookActions(NSString * _Nonnull eventName, SFAnalyticsEventClass eventClass, NSDictionary * _Nonnull attributes, SFAnalyticsTimestampBucket timestampBucket) {
        XCTAssertEqualObjects(eventName, @"event");
        [e fulfill];
        return SFAnalyticsMetricsHookExcludeEvent;
    }];

    NSError *error = [NSError errorWithDomain:@"domain" code:1 userInfo:nil];
    [_analytics logResultForEvent:@"event" hardFailure:YES result:error];

    [self waitForExpectations:@[e] timeout:1.0];

    PQLResultSet* result = [_db fetch:@"select count(*) from hard_failures"];
    XCTAssertTrue([result next], @"Got a count from hard_failures");
    XCTAssertLessThanOrEqual([result unsignedIntAtIndex:0], 0, @"Should have zero events");
}

- (NSData *)basicSFARules {
    NSDictionary *match1 = @{
        @"attribute": @"1",
    };

    SECSFAActionAutomaticBugCapture *abc = [[SECSFAActionAutomaticBugCapture alloc] init];
    abc.domain = @"domain";
    abc.type = @"type";

    SECSFARule *r1 = [[SECSFARule alloc] init];
    r1.eventType = @"event";
    r1.match = [NSPropertyListSerialization dataWithPropertyList:match1 format:NSPropertyListBinaryFormat_v1_0 options:0 error:nil];
    r1.action = [[SECSFAAction alloc] init];
    r1.action.radarnumber = @"1";
    r1.action.abc = abc;
    
    NSDictionary *match2 = @{
        @"attribute": @"2",
    };
    
    SECSFARule *r2 = [[SECSFARule alloc] init];
    r2.eventType = @"soft";
    r2.eventClass = SECSFAEventClass_Errors;
    r2.match = [NSPropertyListSerialization dataWithPropertyList:match2 format:NSPropertyListBinaryFormat_v1_0 options:0 error:nil];
    r2.action = [[SECSFAAction alloc] init];
    r2.action.radarnumber = @"2";
    r2.action.abc = abc;
    
    SECSFARule *r3 = [[SECSFARule alloc] init];
    r3.eventType = @"hard";
    r3.eventClass = SECSFAEventClass_HardFailure;
    r3.match = [NSPropertyListSerialization dataWithPropertyList:match2 format:NSPropertyListBinaryFormat_v1_0 options:0 error:nil];
    r3.action = [[SECSFAAction alloc] init];
    r3.action.radarnumber = @"2";
    r3.action.abc = abc;
    
    SECSFARule *r4 = [[SECSFARule alloc] init];
    r4.eventType = @"hard2";
    r4.eventClass = SECSFAEventClass_HardFailure;
    r4.match = [NSPropertyListSerialization dataWithPropertyList:match2 format:NSPropertyListBinaryFormat_v1_0 options:0 error:nil];
    r4.action = [[SECSFAAction alloc] init];
    r4.action.radarnumber = @"2";
    r4.action.abc = abc;

    
    SECSFARule *r5 = [[SECSFARule alloc] init];
    r5.eventType = @"default";
    r5.match = [NSPropertyListSerialization dataWithPropertyList:match2 format:NSPropertyListBinaryFormat_v1_0 options:0 error:nil];
    r5.action = [[SECSFAAction alloc] init];
    r5.action.radarnumber = @"2";
    r5.action.abc = abc;


    SECSFARules *dr = [[SECSFARules alloc] init];
    [dr addRules:r1];
    [dr addRules:r2];
    [dr addRules:r3];
    [dr addRules:r4];
    [dr addRules:r5];

    SECSFAActionDropEvent *drop = [[SECSFAActionDropEvent alloc] init];
    drop.excludeEvent = true;

    SECSFARule *ruleDrop = [[SECSFARule alloc] init];
    ruleDrop.eventType = @"drop-event";
    ruleDrop.match = nil;
    ruleDrop.action = [[SECSFAAction alloc] init];
    ruleDrop.action.drop = drop;

    [dr addRules:ruleDrop];

    return [dr.data compressedDataUsingAlgorithm:NSDataCompressionAlgorithmLZFSE error:nil];
}

- (NSData *)complextSFARules {
    NSDictionary *matchA = @{
        @"attribute": @[ @"2", @3 ],
    };
    NSDictionary *matchD = @{
        @"attribute": @{
            @"3": @3,
            @"4": @4,
        },
    };

    SECSFAActionAutomaticBugCapture *abca = [[SECSFAActionAutomaticBugCapture alloc] init];
    abca.domain = @"domain";
    abca.type = @"type";

    SECSFARule *r = [[SECSFARule alloc] init];
    r.eventType = @"event-array";
    r.match = [NSPropertyListSerialization dataWithPropertyList:matchA format:NSPropertyListBinaryFormat_v1_0 options:0 error:nil];
    r.action = [[SECSFAAction alloc] init];
    r.action.radarnumber = @"1";
    r.action.abc = abca;

    SECSFARules *dr = [[SECSFARules alloc] init];
    [dr addRules:r];

    SECSFAActionAutomaticBugCapture *abcd = [[SECSFAActionAutomaticBugCapture alloc] init];
    abcd.domain = @"domain";
    abcd.type = @"type";

    SECSFARule *rd = [[SECSFARule alloc] init];
    rd.eventType = @"event-dictionary";
    rd.match = [NSPropertyListSerialization dataWithPropertyList:matchD format:NSPropertyListBinaryFormat_v1_0 options:0 error:nil];
    rd.action = [[SECSFAAction alloc] init];
    rd.action.radarnumber = @"1";
    rd.action.abc = abcd;

    [dr addRules:rd];

    return [dr.data compressedDataUsingAlgorithm:NSDataCompressionAlgorithmLZFSE error:nil];
}


- (void)testMetricsCollection
{
    SFAnalyticsCollection *c = [[SFAnalyticsCollection alloc] init];
    XCTAssertNotNil(c);

    NSData *data = [self basicSFARules];
    XCTAssertNotNil(data);

    NSMutableDictionary<NSString*, NSMutableSet<SFAnalyticsMatchingRule*>*>* rules;

    rules = [c parseCollection:data logger:_analytics];
    XCTAssertNotNil(rules);

    NSMutableSet<SFAnalyticsMatchingRule*>* items = rules[@"event"];
    XCTAssertNotNil(items);
    XCTAssertEqual(items.count, 1);

    SFAnalyticsMatchingRule *mr = items.anyObject;
    XCTAssertNotNil(mr);
    XCTAssertEqualObjects(mr.eventName, @"event");
    XCTAssertEqualObjects(mr.rule.action.radarnumber, @"1");
    XCTAssertEqualObjects(mr.rule.action.abc.domain, @"domain");
    XCTAssertEqualObjects(mr.rule.action.abc.type, @"type");
}

- (void)testCollectionMatchItem
{
    TestCollectionActions *actions = [[TestCollectionActions alloc] init];
    SFAnalyticsCollection *collection = [[SFAnalyticsCollection alloc] initWithActionInterface:actions];
    XCTAssertNotNil(collection);

    NSData *data =[self basicSFARules];
    XCTAssertNotNil(data);
    [collection storeCollection:data logger:nil];

    /* Check that we DO match an almost like it */
    actions.abcExpectation = [self expectationWithDescription:@"expected matching abc"];
    NSDictionary *attributesMatch = @{
        @"attribute": @"1",
    };

    [collection match:@"event"
           eventClass:SFAnalyticsEventClassHardFailure
           attributes:attributesMatch
               bucket:SFAnalyticsTimestampBucketSecond
               logger:_analytics];

    [self waitForExpectations:@[actions.abcExpectation] timeout:1.0];
}

- (void)testCollectionMatchItemArray
{
    TestCollectionActions *actions = [[TestCollectionActions alloc] init];
    SFAnalyticsCollection *collection = [[SFAnalyticsCollection alloc] initWithActionInterface:actions];
    XCTAssertNotNil(collection);

    NSData *data =[self complextSFARules];
    XCTAssertNotNil(data);
    [collection storeCollection:data logger:nil];

    actions.abcExpectation = [self expectationWithDescription:@"expected matching abc"];
    NSDictionary *attributesMatch = @{
        @"attribute": @[ @"2", @3 ],
    };

    [collection match:@"event-array"
           eventClass:SFAnalyticsEventClassHardFailure
           attributes:attributesMatch
               bucket:SFAnalyticsTimestampBucketSecond
               logger:_analytics];

    [self waitForExpectations:@[actions.abcExpectation] timeout:1.0];
}

- (void)testCollectionMatchItemDictionary
{
    TestCollectionActions *actions = [[TestCollectionActions alloc] init];
    SFAnalyticsCollection *collection = [[SFAnalyticsCollection alloc] initWithActionInterface:actions];
    XCTAssertNotNil(collection);

    [collection storeCollection:[self complextSFARules] logger:nil];

    actions.abcExpectation = [self expectationWithDescription:@"expected matching abc"];
    NSDictionary *attributesMatch = @{
        @"attribute": @{
            @"3": @3,
            @"4": @4,
        },
    };

    [collection match:@"event-dictionary"
           eventClass:SFAnalyticsEventClassHardFailure
           attributes:attributesMatch
               bucket:SFAnalyticsTimestampBucketSecond
               logger:_analytics];

    [self waitForExpectations:@[actions.abcExpectation] timeout:1.0];
}

- (void)testRateLimit
{
    SFAnalyticsCollection *c = [[SFAnalyticsCollection alloc] init];
    XCTAssertNotNil(c);
    
    [c storeCollection:[self basicSFARules] logger:nil];
    
    /* Check that we DO match an item */
    NSDictionary *attributesMatch = @{
        @"attribute": @"1",
    };

    SFAnalyticsMetricsHookActions action;

    action = [c match:@"drop-event"
           eventClass:SFAnalyticsEventClassHardFailure
           attributes:attributesMatch
               bucket:SFAnalyticsTimestampBucketSecond
               logger:_analytics];
    XCTAssertEqual(action, SFAnalyticsMetricsHookExcludeEvent);
    
    action = [c match:@"drop-event"
           eventClass:SFAnalyticsEventClassHardFailure
           attributes:attributesMatch
               bucket:SFAnalyticsTimestampBucketSecond
               logger:_analytics];
    XCTAssertEqual(action, SFAnalyticsMetricsHookExcludeEvent);
    
}

- (void)testCollectionRateLimitWorkingForTTR
{
    TestCollectionActions *actions = [[TestCollectionActions alloc] init];
    SFAnalyticsCollection *collection = [[SFAnalyticsCollection alloc] initWithActionInterface:actions];
    XCTAssertNotNil(collection);

    [collection storeCollection:[self basicSFARules] logger:nil];

    /* Check that we DO match an item */
    actions.abcExpectation = [self expectationWithDescription:@"expected matching abc"];
    NSDictionary *attributesMatch = @{
        @"attribute": @"1",
    };

    [collection match:@"event"
           eventClass:SFAnalyticsEventClassHardFailure
           attributes:attributesMatch
               bucket:SFAnalyticsTimestampBucketSecond
               logger:_analytics];

    [self waitForExpectations:@[actions.abcExpectation] timeout:1.0];


    /* But not again, because now ratelimting is in force */
    actions.ratelimit = YES;
    actions.abcExpectation = [self expectationWithDescription:@"expected missing abc"];
    actions.abcExpectation.inverted = YES;

    [collection match:@"event"
           eventClass:SFAnalyticsEventClassHardFailure
           attributes:attributesMatch
               bucket:SFAnalyticsTimestampBucketSecond
               logger:_analytics];

    [self waitForExpectations:@[actions.abcExpectation] timeout:1.0];
}

- (void)testCollectionRateLimitWorkingForSkip
{
    TestCollectionActions *actions = [[TestCollectionActions alloc] init];
    SFAnalyticsCollection *collection = [[SFAnalyticsCollection alloc] initWithActionInterface:actions];
    XCTAssertNotNil(collection);

    [collection storeCollection:[self basicSFARules] logger:nil];

    /* Check that we DO match an item */
    NSDictionary *attributesMatch = @{
        @"attribute": @"1",
    };

    SFAnalyticsMetricsHookActions action;

    action = [collection match:@"drop-event"
                    eventClass:SFAnalyticsEventClassHardFailure
                    attributes:attributesMatch
                        bucket:SFAnalyticsTimestampBucketSecond
                        logger:_analytics];
    XCTAssertEqual(action, SFAnalyticsMetricsHookExcludeEvent);
    
    actions.ratelimit = YES;

    action = [collection match:@"drop-event"
                    eventClass:SFAnalyticsEventClassHardFailure
                    attributes:attributesMatch
                        bucket:SFAnalyticsTimestampBucketSecond
                        logger:_analytics];
    XCTAssertEqual(action, SFAnalyticsMetricsHookExcludeEvent);
}


- (void)testCollectionNotMatchEvent
{
    TestCollectionActions *actions = [[TestCollectionActions alloc] init];
    SFAnalyticsCollection *collection = [[SFAnalyticsCollection alloc] initWithActionInterface:actions];
    XCTAssertNotNil(collection);

    [collection storeCollection:[self basicSFARules] logger:nil];

    NSDictionary *attributesMatch = @{
        @"attribute": @"1",
    };

    /* Check that we DON'T match with a different event */
    actions.abcExpectation = [self expectationWithDescription:@"expected missing abc"];
    actions.abcExpectation.inverted = YES;

    [collection match:@"event2"
           eventClass:SFAnalyticsEventClassHardFailure
           attributes:attributesMatch
               bucket:SFAnalyticsTimestampBucketSecond
               logger:_analytics];

    [self waitForExpectations:@[actions.abcExpectation] timeout:1.0];
}

- (void)testCollectionNotMatchEventAttributes
{
    TestCollectionActions *actions = [[TestCollectionActions alloc] init];
    SFAnalyticsCollection *collection = [[SFAnalyticsCollection alloc] initWithActionInterface:actions];
    XCTAssertNotNil(collection);

    [collection storeCollection:[self basicSFARules] logger:nil];

    NSDictionary *attributesNotMatch = @{
        @"attribute": @"2",
    };

    /* Check that we DON'T match with a different event */
    actions.abcExpectation = [self expectationWithDescription:@"expected missing abc"];
    actions.abcExpectation.inverted = YES;

    [collection match:@"event"
           eventClass:SFAnalyticsEventClassHardFailure
           attributes:attributesNotMatch
               bucket:SFAnalyticsTimestampBucketSecond
               logger:_analytics];

    [self waitForExpectations:@[actions.abcExpectation] timeout:1.0];
}

- (void)testCollectionNotMatchingEventType
{
    TestCollectionActions *actions = [[TestCollectionActions alloc] init];
    SFAnalyticsCollection *collection = [[SFAnalyticsCollection alloc] initWithActionInterface:actions];
    XCTAssertNotNil(collection);

    [collection storeCollection:[self basicSFARules] logger:nil];

    NSDictionary *attributesMatch2 = @{
        @"attribute": @"2",
    };

    /* Check that we DON'T match success */
    actions.abcExpectation = [self expectationWithDescription:@"expected missing abc"];
    actions.abcExpectation.inverted = YES;

    [collection match:@"hard"
           eventClass:SFAnalyticsEventClassSuccess
           attributes:attributesMatch2
               bucket:SFAnalyticsTimestampBucketSecond
               logger:_analytics];

    [self waitForExpectations:@[actions.abcExpectation] timeout:0.1];
    
    /* Check that we match with event */
    actions.abcExpectation = [self expectationWithDescription:@"expected abc for hard error"];

    [collection match:@"hard"
           eventClass:SFAnalyticsEventClassHardFailure
           attributes:attributesMatch2
               bucket:SFAnalyticsTimestampBucketSecond
               logger:_analytics];
    
    [self waitForExpectations:@[actions.abcExpectation] timeout:0.1];
        
    /* Check that we DONT match with event */
    actions.abcExpectation = [self expectationWithDescription:@"expected not abc for soft error on hard trigger"];
    actions.abcExpectation.inverted = YES;

    [collection match:@"hard"
           eventClass:SFAnalyticsEventClassSoftFailure
           attributes:attributesMatch2
               bucket:SFAnalyticsTimestampBucketSecond
               logger:_analytics];
    
    [self waitForExpectations:@[actions.abcExpectation] timeout:0.1];
    
    /* Check that we match with event */
    actions.abcExpectation = [self expectationWithDescription:@"expected abc for soft error"];

    [collection match:@"soft"
           eventClass:SFAnalyticsEventClassSoftFailure
           attributes:attributesMatch2
               bucket:SFAnalyticsTimestampBucketSecond
               logger:_analytics];

    [self waitForExpectations:@[actions.abcExpectation] timeout:0.1];
    
    /* Check that we match with event */
    actions.abcExpectation = [self expectationWithDescription:@"expected abc for default"];

    [collection match:@"default"
           eventClass:SFAnalyticsEventClassSoftFailure
           attributes:attributesMatch2
               bucket:SFAnalyticsTimestampBucketSecond
               logger:_analytics];

    [self waitForExpectations:@[actions.abcExpectation] timeout:0.1];
    
    /* Check that we DONT match when ratelimited */
    actions.abcExpectation = [self expectationWithDescription:@"expected not abc for soft error on hard trigger"];
    actions.abcExpectation.inverted = YES;
    actions.ratelimit = YES;

    [collection match:@"default"
           eventClass:SFAnalyticsEventClassSoftFailure
           attributes:attributesMatch2
               bucket:SFAnalyticsTimestampBucketSecond
               logger:_analytics];

    [self waitForExpectations:@[actions.abcExpectation] timeout:0.1];

}

- (void)testSetupHookVSLogging {
    dispatch_queue_t setdeleteQueue = dispatch_queue_create("setup", DISPATCH_QUEUE_CONCURRENT);
    dispatch_queue_t logQueue = dispatch_queue_create("setup", DISPATCH_QUEUE_CONCURRENT);
    __block atomic_bool stop = false;

    __block void (^setupLoop)(void) = nil;
    __block void (^logLoop)(void) = nil;
    __block long countSetup = 0;
    __block long countLog = 0;

    XCTestExpectation *setupExpection = [self expectationWithDescription:@"setupLoop"];
    XCTestExpectation *logExpection = [self expectationWithDescription:@"logLoop"];

    setupLoop = ^{
        if (stop) {
            [setupExpection fulfill];
            setupLoop = nil;
            return;
        }
        SFAnalyticsMetricsHook hook;

        hook = ^(NSString * _Nonnull eventName, SFAnalyticsEventClass eventClass, NSDictionary * _Nonnull attributes, SFAnalyticsTimestampBucket timestampBucket) {
            return SFAnalyticsMetricsHookNoAction;
        };

        [self->_analytics addMetricsHook:hook];
        [self->_analytics removeMetricsHook:hook];

        countSetup++;
        dispatch_async(setdeleteQueue, setupLoop);
    };

    dispatch_async(setdeleteQueue, setupLoop);

    logLoop = ^{
        if (stop) {
            [logExpection fulfill];
            logLoop = nil;
            return;
        }
        [self->_analytics logSuccessForEventNamed:@"foo"];

        countLog++;
        dispatch_async(logQueue, logLoop);
    };

    dispatch_async(logQueue, logLoop);

    sleep(2);

    stop = true;

    [self waitForExpectations:@[setupExpection, logExpection] timeout:5];

    printf("setup: %ld\n", countSetup);
    printf("log: %ld\n", countLog);
}



- (void)testCollectionParse {

    NSError *error;
    NSDictionary *collection = @{
        @"rules": @[
            @{
                @"eventType": @"sfaEvent",
                @"match": @{
                    @"key": @"value",
                },
                @"action": @{
                    @"radarNumber": @"1",
                    @"actionType": @"abc",
                    @"domain": @"abc-domain",
                    @"type": @"abc-type",
                }
            },
            @{
                @"eventType": @"sfaEvent2",
                @"match": @{
                    @"key2": @"value2",
                },
                @"action": @{
                    @"radarNumber": @"1",
                    @"actionType": @"abc",
                    @"domain": @"abc-domain",
                    @"type": @"abc-type",
                    @"subtype": @"abc-subtype",
                }
            },
            @{
                @"eventType": @"sfaEvent3",
                @"match": @{
                    @"key3": @"value3",
                },
                @"action": @{
                    @"radarNumber": @"1",
                    @"actionType": @"ttr",
                    @"alert": @"ttr-alert",
                    @"componentID": @"ttr-componentID",
                    @"componentName": @"ttr-componentName",
                    @"componentVersion": @"ttr-componentVersion",
                    @"radarDescription": @"ttr-radarDescription",
                }
            },
            @{
                @"eventType": @"sfaEvent4",
                @"match": @{
                    @"key3": @"value3",
                },
                @"action": @{
                    @"actionType": @"drop",
                }
            },
        ],
    };
    NSData *json = [NSJSONSerialization dataWithJSONObject:collection options:0 error:&error];
    XCTAssert(json, "dataWithJSONObject: %@", error);

    NSData *binary = [SFAnalytics encodeSFACollection:json error:&error];
    XCTAssertNotNil(binary, @"encodeSFACollection: %@", error);
    XCTAssertEqual(binary.length, 267);
}


- (void)testCollectionParseEventClass {

    NSError *error;
    NSDictionary *collection = @{
        @"rules": @[
            @{
                @"eventType": @"sfaEvent5",
                @"eventClass": @"all",
                @"match": @{},
                @"action": @{
                    @"radarNumber": @"1",
                    @"actionType": @"abc",
                    @"domain": @"abc-domain",
                    @"type": @"abc-type",
                    @"subtype": @"abc-subtype",
                }
            },
            @{
                @"eventType": @"sfaEvent5",
                @"eventClass": @"errors",
                @"match": @{},
                @"action": @{
                    @"radarNumber": @"1",
                    @"actionType": @"abc",
                    @"domain": @"abc-domain",
                    @"type": @"abc-type",
                    @"subtype": @"abc-subtype",
                }
            },
            @{
                @"eventType": @"sfaEvent6",
                @"eventClass": @"success",
                @"match": @{},
                @"action": @{
                    @"radarNumber": @"1",
                    @"actionType": @"abc",
                    @"domain": @"abc-domain",
                    @"type": @"abc-type",
                    @"subtype": @"abc-subtype",
                }
            },
            @{
                @"eventType": @"sfaEvent6",
                @"eventClass": @"hardfail",
                @"match": @{},
                @"action": @{
                    @"radarNumber": @"1",
                    @"actionType": @"abc",
                    @"domain": @"abc-domain",
                    @"type": @"abc-type",
                    @"subtype": @"abc-subtype",
                }
            },
            @{
                @"eventType": @"sfaEvent6",
                @"eventClass": @"softfail",
                @"match": @{},
                @"action": @{
                    @"radarNumber": @"1",
                    @"actionType": @"abc",
                    @"domain": @"abc-domain",
                    @"type": @"abc-type",
                    @"subtype": @"abc-subtype",
                }
            },
        ],
    };
    NSData *json = [NSJSONSerialization dataWithJSONObject:collection options:0 error:&error];
    XCTAssert(json, "dataWithJSONObject: %@", error);

    NSData *binary = [SFAnalytics encodeSFACollection:json error:&error];
    XCTAssertNotNil(binary, @"encodeSFACollection: %@", error);
    XCTAssertEqual(binary.length, 135);
}


- (void)testCollectionJSON {

    NSError *error;
    char json[] = "{\"rules\":[{\"eventType\":\"sfaEvent\",\"match\":{\"key\":\"value\"},\"action\":{\"radarNumber\":\"1\",\"actionType\":\"abc\",\"domain\":\"abc-domain\",\"type\":\"abc-type\"}}]}";

    NSData *jsonData = [NSData dataWithBytes:json length:sizeof(json)-1];
    NSData *binary = [SFAnalytics encodeSFACollection:jsonData error:&error];
    XCTAssertNotNil(binary, @"encodeSFACollection: %@", error);
    XCTAssertEqual(binary.length, 110);
}

- (void)testLaunchSeqence {
    SecLaunchSequence *ls = [[SecLaunchSequence alloc] initWithRocketName:@"com.apple.sear.test"];
    [ls addEvent:@"event1"];
    [ls addEvent:@"event2"];
    [ls launch];

    [_analytics noteLaunchSequence:ls];

    PQLResultSet* result = [_db fetch:@"select count(*) from rockwell"];
    XCTAssertTrue([result next], @"Got a count from rockwell");
    XCTAssertEqual([result unsignedIntAtIndex:0], 1, @"Should have one event");
}

- (void)testLaunchSeqenceTwoSame {
    SecLaunchSequence *ls1 = [[SecLaunchSequence alloc] initWithRocketName:@"com.apple.sear.test"];
    [ls1 launch];
    [_analytics noteLaunchSequence:ls1];

    SecLaunchSequence *ls2 = [[SecLaunchSequence alloc] initWithRocketName:@"com.apple.sear.test"];
    [ls2 launch];
    [_analytics noteLaunchSequence:ls2];

    PQLResultSet* result = [_db fetch:@"select count(*) from rockwell"];
    XCTAssertTrue([result next], @"Got a count from rockwell");
    XCTAssertEqual([result unsignedIntAtIndex:0], 1, @"Should have one rockwell event");
}

- (void)testLaunchSeqenceTwoDifference {
    SecLaunchSequence *ls1 = [[SecLaunchSequence alloc] initWithRocketName:@"com.apple.sear.test1"];
    [ls1 launch];
    [_analytics noteLaunchSequence:ls1];

    SecLaunchSequence *ls2 = [[SecLaunchSequence alloc] initWithRocketName:@"com.apple.sear.test2"];
    [ls2 launch];
    [_analytics noteLaunchSequence:ls2];

    PQLResultSet* result = [_db fetch:@"select count(*) from rockwell"];
    XCTAssertTrue([result next], @"Got a count from rockwell");
    XCTAssertEqual([result unsignedIntAtIndex:0], 2, @"Should have two rockwell events");
}

- (void)testLaunchSeqenceDeleted{
    SecLaunchSequence *ls = [[SecLaunchSequence alloc] initWithRocketName:@"com.apple.sear.test"];
    [ls launch];
    [_analytics noteLaunchSequence:ls];

    [_analytics.database clearAllData];

    PQLResultSet* result = [_db fetch:@"select count(*) from rockwell"];
    XCTAssertTrue([result next], @"Got a count from rockwell");
    XCTAssertEqual([result unsignedIntAtIndex:0], 0, @"Should have zero rockwell events");
}


@end


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
#import "SFAnalyticsDefines.h"
#import "SFAnalyticsSQLiteStore.h"
#import "NSDate+SFAnalytics.h"
#import "SFSQLite.h"
#import <Prequelite/Prequelite.h>
#import <CoreFoundation/CFPriv.h>
#import <notify.h>
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
    NSNumber *timestamp = rowdata[SFAnalyticsEventTime];
    XCTAssert([timestamp isKindOfClass:[NSNumber class]], @"Timestamp is an NSNumber");
    [self recentTimeStamp:([timestamp doubleValue] / 1000) timestampBucket:bucket]; // We need to convert to seconds, as its stored in milliseconds
    XCTAssertTrue([rowdata[SFAnalyticsEventType] isKindOfClass:[NSString class]] && [rowdata[SFAnalyticsEventType] isEqualToString:eventType], @"found eventType \"%@\" in db", eventType);
    XCTAssertTrue([rowdata[SFAnalyticsEventClassKey] isKindOfClass:[NSNumber class]] && [rowdata[SFAnalyticsEventClassKey] intValue] == class, @"eventClass is %ld", (long)class);
    XCTAssertTrue([rowdata[@"build"] isEqualToString:build], @"event row includes build");
    XCTAssertTrue([rowdata[@"product"] isEqualToString:product], @"event row includes product");
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

    XCTAssertNil([SFAnalyticsSQLiteStore storeWithPath:nil schema:schema]);
    XCTAssertNil([SFAnalyticsSQLiteStore storeWithPath:@"" schema:schema]);
    XCTAssertNil([SFAnalyticsSQLiteStore storeWithPath:path schema:nil]);
    XCTAssertNil([SFAnalyticsSQLiteStore storeWithPath:path schema:@""]);

    XCTAssertNil([[SFSQLite alloc] initWithPath:nil schema:schema]);
    XCTAssertNil([[SFSQLite alloc] initWithPath:@"" schema:schema]);
    XCTAssertNil([[SFSQLite alloc] initWithPath:path schema:nil]);
    XCTAssertNil([[SFSQLite alloc] initWithPath:path schema:@""]);
}

- (void)testAddingEventsWithNilName
{
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
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnonnull"
    [_analytics logMetric:nil withName:@"testsampler"];
    [self checkSamples:@[] name:@"testsampler" totalSamples:0 accuracy:0.01f];
#pragma clang diagnostic pop

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
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnonnull"
    // Inspect database to find out it's empty
    [_analytics logMetric:nil withName:@"fake"];
    [_analytics logMetric:@3.0 withName:nil];

    // get object back so inspect that, too
    XCTAssertNil([_analytics logSystemMetricsForActivityNamed:nil withAction:^{return;}]);

    [self assertNoEventsAnywhere];
#pragma clang diagnostic pop
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
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnonnull"
    XCTAssertEqual([SFAnalytics fuzzyDaysSinceDate:nil], -1);
#pragma clang diagnostic pop
}

- (void)testRingBuffer {
    [self assertNoEventsAnywhere];
    for (unsigned idx = 0; idx < (SFAnalyticsMaxEventsToReport + 50); ++idx) {
        [_analytics logHardFailureForEventNamed:@"ringbufferevent" withAttributes:nil];
    }

    PQLResultSet* result = [_db fetch:@"select count(*) from hard_failures"];
    XCTAssertTrue([result next], @"Got a count from hard_failures");
    XCTAssertLessThanOrEqual([result unsignedIntAtIndex:0], SFAnalyticsMaxEventsToReport, @"Ring buffer contains a sane number of events");
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

@end

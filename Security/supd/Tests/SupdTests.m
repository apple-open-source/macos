/*
 * Copyright (c) 2017-2018 Apple Inc. All Rights Reserved.
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

// securityuploadd does not do anything or build meaningful code on simulator, so no tests either.
#if TARGET_OS_SIMULATOR

@interface SupdTests : XCTestCase
@end

@implementation SupdTests
@end

#else

#import <OCMock/OCMock.h>
#import "supd.h"
#import <Security/SFAnalytics.h>
#import "SFAnalyticsDefines.h"
#import <CoreFoundation/CFPriv.h>

static NSString* _path;
static NSInteger _testnum;
static NSString* build = NULL;
static NSString* product = NULL;
static NSInteger _reporterWrites;

// MARK: Stub FakeCKKSAnalytics

@interface FakeCKKSAnalytics : SFAnalytics

@end

@implementation FakeCKKSAnalytics

+ (NSString*)databasePath
{
    return [_path stringByAppendingFormat:@"/ckks_%ld.db", (long)_testnum];
}

@end


// MARK: Stub FakeSOSAnalytics

@interface FakeSOSAnalytics : SFAnalytics

@end

@implementation FakeSOSAnalytics

+ (NSString*)databasePath
{
    return [_path stringByAppendingFormat:@"/sos_%ld.db", (long)_testnum];
}

@end


// MARK: Stub FakePCSAnalytics

@interface FakePCSAnalytics : SFAnalytics

@end

@implementation FakePCSAnalytics

+ (NSString*)databasePath
{
    return [_path stringByAppendingFormat:@"/pcs_%ld.db", (long)_testnum];
}

@end

// MARK: Stub FakeTLSAnalytics

@interface FakeTLSAnalytics : SFAnalytics

@end

@implementation FakeTLSAnalytics

+ (NSString*)databasePath
{
    return [_path stringByAppendingFormat:@"/tls_%ld.db", (long)_testnum];
}

@end

// MARK: Start SupdTests

@interface SupdTests : XCTestCase

@end

@implementation SupdTests {
    supd* _supd;
    id mockReporter;
    FakeCKKSAnalytics* _ckksAnalytics;
    FakeSOSAnalytics* _sosAnalytics;
    FakePCSAnalytics* _pcsAnalytics;
    FakeTLSAnalytics* _tlsAnalytics;
}

// MARK: Test helper methods
- (SFAnalyticsTopic *)keySyncTopic {
    for (SFAnalyticsTopic *topic in _supd.analyticsTopics) {
        if ([topic.internalTopicName isEqualToString:SFAnalyticsTopicKeySync]) {
            return topic;
        }
    }
    return nil;
}

- (SFAnalyticsTopic *)TrustTopic {
    for (SFAnalyticsTopic *topic in _supd.analyticsTopics) {
        if ([topic.internalTopicName isEqualToString:SFAnalyticsTopicTrust]) {
            return topic;
        }
    }
    return nil;
}

- (void)inspectDataBlobStructure:(NSDictionary*)data
{
    [self inspectDataBlobStructure:data forTopic:[[self keySyncTopic] splunkTopicName]];
}

- (void)inspectDataBlobStructure:(NSDictionary*)data forTopic:(NSString*)topic
{
    if (!data || ![data isKindOfClass:[NSDictionary class]]) {
        XCTFail(@"data is an NSDictionary");
    }

    XCTAssert(_supd.analyticsTopics, @"supd has nonnull topics list");
    XCTAssert([[self keySyncTopic] splunkTopicName], @"keysync topic has a splunk name");
    XCTAssert([[self TrustTopic] splunkTopicName], @"trust topic has a splunk name");
    XCTAssertEqual([data count], 2ul, @"dictionary event and posttime objects");
    XCTAssertTrue(data[@"events"] && [data[@"events"] isKindOfClass:[NSArray class]], @"data blob contains an NSArray 'events'");
    XCTAssertTrue(data[@"postTime"] && [data[@"postTime"] isKindOfClass:[NSNumber class]], @"data blob contains an NSNumber 'postTime");
    NSDate* postTime = [NSDate dateWithTimeIntervalSince1970:[data[@"postTime"] doubleValue]];
    XCTAssertTrue([[NSDate date] timeIntervalSinceDate:postTime] < 3, @"postTime is sane");

    for (NSDictionary* event in data[@"events"]) {
        if ([event isKindOfClass:[NSDictionary class]]) {
            NSLog(@"build: \"%@\", eventbuild: \"%@\"", build, event[@"build"]);
            XCTAssertEqualObjects(event[@"build"], build, @"event contains correct build string");
            XCTAssertEqualObjects(event[@"product"], product, @"event contains correct product string");
            XCTAssertTrue([event[@"eventTime"] isKindOfClass:[NSNumber class]], @"event contains an NSNumber 'eventTime");
            NSDate* eventTime = [NSDate dateWithTimeIntervalSince1970:[event[@"eventTime"] doubleValue]];
            XCTAssertTrue([[NSDate date] timeIntervalSinceDate:eventTime] < 3, @"eventTime is sane");
            XCTAssertTrue([event[@"eventType"] isKindOfClass:[NSString class]], @"all events have a type");
            XCTAssertEqualObjects(event[@"topic"], topic, @"all events have a topic name");
        } else {
            XCTFail(@"event %@ is an NSDictionary", event);
        }
    }
}

- (BOOL)event:(NSDictionary*)event containsAttributes:(NSDictionary*)attrs {
    if (!attrs) {
        return YES;
    }
    __block BOOL equal = YES;
    [attrs enumerateKeysAndObjectsUsingBlock:^(id  _Nonnull key, id  _Nonnull obj, BOOL * _Nonnull stop) {
        equal &= [event[key] isEqualToString:obj];
    }];
    return equal;
}

- (int)failures:(NSDictionary*)data eventType:(NSString*)type attributes:(NSDictionary*)attrs class:(SFAnalyticsEventClass)class
{
    int encountered = 0;
    for (NSDictionary* event in data[@"events"]) {
        if ([event[@"eventType"] isEqualToString:type] &&
            [event[@"eventClass"] isKindOfClass:[NSNumber class]] &&
            [event[@"eventClass"] intValue] == class && [self event:event containsAttributes:attrs]) {
            ++encountered;
        }
    }
    return encountered;
}

- (void)checkTotalEventCount:(NSDictionary*)data hard:(int)hard soft:(int)soft accuracy:(int)accuracy summaries:(int)summ
{
    int hardfound = 0, softfound = 0, summfound = 0;
    for (NSDictionary* event in data[@"events"]) {
        if ([event[SFAnalyticsEventType] hasSuffix:@"HealthSummary"]) {
            ++summfound;
        } else if ([event[SFAnalyticsEventClassKey] integerValue] == SFAnalyticsEventClassHardFailure) {
            ++hardfound;
        } else if ([event[SFAnalyticsEventClassKey] integerValue] == SFAnalyticsEventClassSoftFailure) {
            ++softfound;
        }
    }

    XCTAssertLessThanOrEqual(((NSArray*)data[@"events"]).count, 1000ul, @"Total event count fits in alloted data");
    XCTAssertEqual(summfound, summ);

    // Add customizable fuzziness
    XCTAssertEqualWithAccuracy(hardfound, hard, accuracy);
    XCTAssertEqualWithAccuracy(softfound, soft, accuracy);
}

- (void)checkTotalEventCount:(NSDictionary*)data hard:(int)hard soft:(int)soft
{
    [self checkTotalEventCount:data hard:hard soft:soft accuracy:10 summaries:(int)[[[self keySyncTopic] topicClients] count]];
}

- (void)checkTotalEventCount:(NSDictionary*)data hard:(int)hard soft:(int)soft accuracy:(int)accuracy
{
    [self checkTotalEventCount:data hard:hard soft:soft accuracy:accuracy summaries:(int)[[[self keySyncTopic] topicClients] count]];
}

// This is a dumb hack, but inlining stringWithFormat causes the compiler to growl for unknown reasons
- (NSString*)string:(NSString*)name item:(NSString*)item
{
    return [NSString stringWithFormat:@"%@-%@", name, item];
}

- (void)sampleStatisticsInEvents:(NSArray*)events name:(NSString*)name values:(NSArray*)values
{
    [self sampleStatisticsInEvents:events name:name values:values amount:1];
}

// Usually amount == 1 but for testing sampler with same name in different subclasses this is higher
- (void)sampleStatisticsInEvents:(NSArray*)events name:(NSString*)name values:(NSArray*)values amount:(int)num
{
    int found = 0;
    for (NSDictionary* event in events) {
        if (([values count] == 1 && ![event objectForKey:[NSString stringWithFormat:@"%@", name]]) ||
            ([values count] > 1 && ![event objectForKey:[NSString stringWithFormat:@"%@-min", name]])) {
            continue;
        }

        ++found;
        if (values.count == 1) {
            XCTAssertEqual([event[name] doubleValue], [values[0] doubleValue]);
            XCTAssertNil(event[[self string:name item:@"min"]]);
            XCTAssertNil(event[[self string:name item:@"max"]]);
            XCTAssertNil(event[[self string:name item:@"avg"]]);
            XCTAssertNil(event[[self string:name item:@"med"]]);
        } else {
            XCTAssertEqualWithAccuracy([event[[self string:name item:@"min"]] doubleValue], [values[0] doubleValue], 0.01f);
            XCTAssertEqualWithAccuracy([event[[self string:name item:@"max"]] doubleValue], [values[1] doubleValue], 0.01f);
            XCTAssertEqualWithAccuracy([event[[self string:name item:@"avg"]] doubleValue], [values[2] doubleValue], 0.01f);
            XCTAssertEqualWithAccuracy([event[[self string:name item:@"med"]] doubleValue], [values[3] doubleValue], 0.01f);
        }

        if (values.count > 4) {
            XCTAssertEqualWithAccuracy([event[[self string:name item:@"dev"]] doubleValue], [values[4] doubleValue], 0.01f);
        } else {
            XCTAssertNil(event[[self string:name item:@"dev"]]);
        }

        if (values.count > 5) {
            XCTAssertEqualWithAccuracy([event[[self string:name item:@"1q"]] doubleValue], [values[5] doubleValue], 0.01f);
            XCTAssertEqualWithAccuracy([event[[self string:name item:@"3q"]] doubleValue], [values[6] doubleValue], 0.01f);
        } else {
            XCTAssertNil(event[[self string:name item:@"1q"]]);
            XCTAssertNil(event[[self string:name item:@"3q"]]);
        }
    }
    XCTAssertEqual(found, num);
}

- (NSDictionary*)getJSONDataFromSupd
{
    return [self getJSONDataFromSupdWithTopic:SFAnalyticsTopicKeySync];
}

- (NSDictionary*)getJSONDataFromSupdWithTopic:(NSString*)topic
{
    dispatch_semaphore_t sema = dispatch_semaphore_create(0);
    __block NSDictionary* data;
    [_supd getLoggingJSON:YES topic:topic reply:^(NSData *json, NSError *error) {
        XCTAssertNil(error);
        XCTAssertNotNil(json);
        if (!error) {
            data = [NSJSONSerialization JSONObjectWithData:json options:0 error:&error];
        }
        XCTAssertNil(error, @"no error deserializing json: %@", error);
        dispatch_semaphore_signal(sema);
    }];
    if (dispatch_semaphore_wait(sema, dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC * 5)) != 0) {
        XCTFail(@"supd returns JSON data in a timely fashion");
    }
    return data;
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
    if (error) {
        NSLog(@"sad trombone, couldn't create path");
    }

    NSDictionary *version = CFBridgingRelease(_CFCopySystemVersionDictionary());
    if (version) {
        build = version[(__bridge NSString *)_kCFSystemVersionBuildVersionKey];
        product = version[(__bridge NSString *)_kCFSystemVersionProductNameKey];
    } else {
        NSLog(@"could not get build version/product, tests should fail");
    }
}

- (void)setUp
{
    [super setUp];
    self.continueAfterFailure = NO;
    ++_testnum;

    id mockTopic = OCMStrictClassMock([SFAnalyticsTopic class]);
    NSString *ckksPath = [_path stringByAppendingFormat:@"/ckks_%ld.db", (long)_testnum];
    NSString *sosPath = [_path stringByAppendingFormat:@"/sos_%ld.db", (long)_testnum];
    NSString *pcsPath = [_path stringByAppendingFormat:@"/pcs_%ld.db", (long)_testnum];
    NSString *tlsPath = [_path stringByAppendingFormat:@"/tls_%ld.db", (long)_testnum];
    NSString *signInPath = [_path stringByAppendingFormat:@"/signin_%ld.db", (long)_testnum];
    NSString *cloudServicesPath = [_path stringByAppendingFormat:@"/cloudServices_%ld.db", (long)_testnum];
    OCMStub([mockTopic databasePathForCKKS]).andReturn(ckksPath);
    OCMStub([mockTopic databasePathForSOS]).andReturn(sosPath);
    OCMStub([mockTopic databasePathForPCS]).andReturn(pcsPath);
    OCMStub([mockTopic databasePathForTLS]).andReturn(tlsPath);
    OCMStub([mockTopic databasePathForSignIn]).andReturn(signInPath);
    OCMStub([mockTopic databasePathForCloudServices]).andReturn(cloudServicesPath);

    // These are not used for testing, but real data can pollute tests so point to empty DBs
    NSString *localpath = [_path stringByAppendingFormat:@"/local_empty_%ld.db", (long)_testnum];
    NSString *trustPath = [_path stringByAppendingFormat:@"/trust_empty_%ld.db", (long)_testnum];
    NSString *trustdhealthPath = [_path stringByAppendingFormat:@"/trustdhealth_empty_%ld.db", (long)_testnum];
    OCMStub([mockTopic databasePathForLocal]).andReturn(localpath);
    OCMStub([mockTopic databasePathForTrust]).andReturn(trustPath);
    OCMStub([mockTopic databasePathForTrustdHealth]).andReturn(trustdhealthPath);

    _reporterWrites = 0;
    mockReporter = OCMClassMock([SFAnalyticsReporter class]);
    OCMStub([mockReporter saveReport:[OCMArg isNotNil] fileName:[OCMArg isNotNil]]).andDo(^(NSInvocation *invocation) {
        _reporterWrites++;
    }).andReturn(YES);

    [supd removeInstance];
    _supd = [[supd alloc] initWithReporter:mockReporter];
    _ckksAnalytics = [FakeCKKSAnalytics new];
    _sosAnalytics = [FakeSOSAnalytics new];
    _pcsAnalytics = [FakePCSAnalytics new];
    _tlsAnalytics = [FakeTLSAnalytics new];

    // Forcibly override analytics flags and enable them by default
    deviceAnalyticsOverride = YES;
    deviceAnalyticsEnabled = YES;
    iCloudAnalyticsOverride = YES;
    iCloudAnalyticsEnabled = YES;
    runningTests = YES;
}

- (void)tearDown
{

    [super tearDown];
}

// MARK: Actual tests

// Note! This test relies on Security being installed because supd reads from a plist in Security.framework
- (void)testSplunkDefaultTopicNameExists
{
    XCTAssertNotNil([[self keySyncTopic] splunkTopicName]);
}

// Note! This test relies on Security being installed because supd reads from a plist in Security.framework
- (void)testSplunkDefaultBagURLExists
{
    XCTAssertNotNil([[self keySyncTopic] splunkBagURL]);
}

- (void)testHaveEligibleClientsKeySync
{
    // KeySyncTopic has no clients requiring deviceAnalytics currently
    SFAnalyticsTopic* keytopic = [[SFAnalyticsTopic alloc] initWithDictionary:@{} name:@"KeySyncTopic" samplingRates:@{}];

    XCTAssertTrue([keytopic haveEligibleClients], @"Both analytics enabled -> we have keysync clients");

    deviceAnalyticsEnabled = NO;
    XCTAssertTrue([keytopic haveEligibleClients], @"Only iCloud analytics enabled -> we have keysync clients");

    iCloudAnalyticsEnabled = NO;
    XCTAssertFalse([keytopic haveEligibleClients], @"Both analytics disabled -> no keysync clients");

    deviceAnalyticsEnabled = YES;
    XCTAssertTrue([keytopic haveEligibleClients], @"Only device analytics enabled -> we have keysync clients (localkeychain for now)");
}

- (void)testHaveEligibleClientsTrust
{
    // TrustTopic has no clients requiring iCloudAnalytics currently
    SFAnalyticsTopic* trusttopic = [[SFAnalyticsTopic alloc] initWithDictionary:@{} name:@"TrustTopic" samplingRates:@{}];

    XCTAssertTrue([trusttopic haveEligibleClients], @"Both analytics enabled -> we have trust clients");

    deviceAnalyticsEnabled = NO;
    XCTAssertFalse([trusttopic haveEligibleClients], @"Only iCloud analytics enabled -> no trust clients");

    iCloudAnalyticsEnabled = NO;
    XCTAssertFalse([trusttopic haveEligibleClients], @"Both analytics disabled -> no trust clients");

    deviceAnalyticsEnabled = YES;
    XCTAssertTrue([trusttopic haveEligibleClients], @"Only device analytics enabled -> we have trust clients");
}

- (void)testLoggingJSONSimple:(BOOL)analyticsEnabled
{
    iCloudAnalyticsEnabled = analyticsEnabled;

    [_ckksAnalytics logSuccessForEventNamed:@"ckksunittestevent"];
    NSDictionary* ckksAttrs = @{@"cattr" : @"cvalue"};
    [_ckksAnalytics logHardFailureForEventNamed:@"ckksunittestevent" withAttributes:ckksAttrs];
    [_ckksAnalytics logSoftFailureForEventNamed:@"ckksunittestevent" withAttributes:ckksAttrs];
    [_sosAnalytics logSuccessForEventNamed:@"unittestevent"];
    NSDictionary* utAttrs = @{@"uattr" : @"uvalue"};
    [_sosAnalytics logHardFailureForEventNamed:@"unittestevent" withAttributes:utAttrs];
    [_sosAnalytics logSoftFailureForEventNamed:@"unittestevent" withAttributes:utAttrs];

    NSDictionary* data = [self getJSONDataFromSupd];
    [self inspectDataBlobStructure:data];

    // TODO: inspect health summaries

    if (analyticsEnabled) {
        XCTAssertEqual([self failures:data eventType:@"ckksunittestevent" attributes:ckksAttrs class:SFAnalyticsEventClassHardFailure], 1);
        XCTAssertEqual([self failures:data eventType:@"ckksunittestevent" attributes:ckksAttrs class:SFAnalyticsEventClassSoftFailure], 1);
        XCTAssertEqual([self failures:data eventType:@"unittestevent" attributes:utAttrs class:SFAnalyticsEventClassHardFailure], 1);
        XCTAssertEqual([self failures:data eventType:@"unittestevent" attributes:utAttrs class:SFAnalyticsEventClassSoftFailure], 1);

        [self checkTotalEventCount:data hard:2 soft:2 accuracy:0];
    } else {
        // localkeychain requires device analytics only so we still get it
        [self checkTotalEventCount:data hard:0 soft:0 accuracy:0 summaries:1];
    }
}

- (void)testLoggingJSONSimpleWithiCloudAnalyticsEnabled
{
    [self testLoggingJSONSimple:YES];
}

- (void)testLoggingJSONSimpleWithiCloudAnalyticsDisabled
{
    [self testLoggingJSONSimple:NO];
}

- (void)testTLSLoggingJSONSimple:(BOOL)analyticsEnabled
{
    deviceAnalyticsEnabled = analyticsEnabled;

    [_tlsAnalytics logSuccessForEventNamed:@"tlsunittestevent"];
    NSDictionary* tlsAttrs = @{@"cattr" : @"cvalue"};
    [_tlsAnalytics logHardFailureForEventNamed:@"tlsunittestevent" withAttributes:tlsAttrs];
    [_tlsAnalytics logSoftFailureForEventNamed:@"tlsunittestevent" withAttributes:tlsAttrs];

    NSDictionary* data = [self getJSONDataFromSupdWithTopic:SFAnalyticsTopicTrust];
    [self inspectDataBlobStructure:data forTopic:[[self TrustTopic] splunkTopicName]];

    if (analyticsEnabled) {
        [self checkTotalEventCount:data hard:1 soft:1 accuracy:0 summaries:(int)[[[self TrustTopic] topicClients] count]];
    } else {
        [self checkTotalEventCount:data hard:0 soft:0 accuracy:0 summaries:0];
    }
}

- (void)testTLSLoggingJSONSimpleWithDeviceAnalyticsEnabled
{
    [self testTLSLoggingJSONSimple:YES];
}

- (void)testTLSLoggingJSONSimpleWithDeviceAnalyticsDisabled
{
    [self testTLSLoggingJSONSimple:NO];
}

- (void)testMockDiagnosticReportGeneration
{
    SFAnalyticsReporter *reporter = mockReporter;

    uint8_t report_data[] = {0x00, 0x01, 0x02, 0x03};
    NSData *reportData = [[NSData alloc] initWithBytes:report_data length:sizeof(report_data)];
    BOOL writtenToLog = YES;
    size_t numWrites = 5;
    for (size_t i = 0; i < numWrites; i++) {
        writtenToLog &= [reporter saveReport:reportData fileName:@"log.txt"];
    }

    XCTAssertTrue(writtenToLog, "Failed to write to log");
    XCTAssertTrue((int)_reporterWrites == (int)numWrites, "Expected %zu report, got %d", numWrites, (int)_reporterWrites);
}

- (void)testSuccessCounts
{
    NSString* eventName1 = @"successCountsEvent1";
    NSString* eventName2 = @"successCountsEvent2";

    for (int idx = 0; idx < 3; ++idx) {
        [_ckksAnalytics logSuccessForEventNamed:eventName1];
        [_ckksAnalytics logSuccessForEventNamed:eventName2];
        [_ckksAnalytics logHardFailureForEventNamed:eventName1 withAttributes:nil];
        [_ckksAnalytics logSoftFailureForEventNamed:eventName2 withAttributes:nil];
    }
    [_ckksAnalytics logSuccessForEventNamed:eventName2];

    NSDictionary* data = [self getJSONDataFromSupd];
    [self inspectDataBlobStructure:data];

    NSDictionary* hs;
    for (NSDictionary* event in data[@"events"]) {
        if ([event[SFAnalyticsEventType] isEqual:@"ckksHealthSummary"]) {
            hs = event;
            break;
        }
    }
    XCTAssert(hs);

    XCTAssertEqual([hs[SFAnalyticsColumnSuccessCount] integerValue], 7);
    XCTAssertEqual([hs[SFAnalyticsColumnHardFailureCount] integerValue], 3);
    XCTAssertEqual([hs[SFAnalyticsColumnSoftFailureCount] integerValue], 3);
    XCTAssertEqual([hs[[self string:eventName1 item:@"success"]] integerValue], 3);
    XCTAssertEqual([hs[[self string:eventName1 item:@"hardfail"]] integerValue], 3);
    XCTAssertEqual([hs[[self string:eventName1 item:@"softfail"]] integerValue], 0);
    XCTAssertEqual([hs[[self string:eventName2 item:@"success"]] integerValue], 4);
    XCTAssertEqual([hs[[self string:eventName2 item:@"hardfail"]] integerValue], 0);
    XCTAssertEqual([hs[[self string:eventName2 item:@"softfail"]] integerValue], 3);
}

// There was a failure with thresholds if some, but not all clients exceeded their 'threshold' number of failures,
// causing the addFailures:toUploadRecords:threshold method to crash with out of bounds.
// This is also implicitly tested in testTooManyHardFailures and testTooManyCombinedFailures but I wanted an explicit case.
- (void)testExceedThresholdForOneClientOnly
{
    int testAmount = ((int)SFAnalyticsMaxEventsToReport / 4);
    for (int idx = 0; idx < testAmount; ++idx) {
        [_ckksAnalytics logHardFailureForEventNamed:@"ckkshardfail" withAttributes:nil];
        [_ckksAnalytics logSoftFailureForEventNamed:@"ckkssoftfail" withAttributes:nil];
    }
    
    [_sosAnalytics logHardFailureForEventNamed:@"soshardfail" withAttributes:nil];
    [_sosAnalytics logSoftFailureForEventNamed:@"sossoftfail" withAttributes:nil];
    
    NSDictionary* data = [self getJSONDataFromSupd];
    [self inspectDataBlobStructure:data];
    
    [self checkTotalEventCount:data hard:testAmount + 1 soft:testAmount + 1 accuracy:0];
    
    XCTAssertEqual([self failures:data eventType:@"ckkshardfail" attributes:nil class:SFAnalyticsEventClassHardFailure], testAmount);
    XCTAssertEqual([self failures:data eventType:@"ckkssoftfail" attributes:nil class:SFAnalyticsEventClassSoftFailure], testAmount);
    XCTAssertEqual([self failures:data eventType:@"soshardfail" attributes:nil class:SFAnalyticsEventClassHardFailure], 1);
    XCTAssertEqual([self failures:data eventType:@"sossoftfail" attributes:nil class:SFAnalyticsEventClassSoftFailure], 1);
}


// We have so many hard failures they won't fit in the upload buffer
- (void)testTooManyHardFailures
{
    NSDictionary* ckksAttrs = @{@"cattr" : @"cvalue"};
    NSDictionary* utAttrs = @{@"uattr" : @"uvalue"};
    for (int idx = 0; idx < 400; ++idx) {
        [_ckksAnalytics logHardFailureForEventNamed:@"ckksunittestfailure" withAttributes:ckksAttrs];
        [_ckksAnalytics logHardFailureForEventNamed:@"ckksunittestfailure" withAttributes:ckksAttrs];
        [_sosAnalytics logHardFailureForEventNamed:@"utunittestfailure" withAttributes:utAttrs];
    }

    NSDictionary* data = [self getJSONDataFromSupd];
    [self inspectDataBlobStructure:data];

    [self checkTotalEventCount:data hard:998 soft:0];
    // Based on threshold = records_to_upload/10 with a nice margin
    XCTAssertEqualWithAccuracy([self failures:data eventType:@"ckksunittestfailure" attributes:ckksAttrs class:SFAnalyticsEventClassHardFailure], 658, 50);
    XCTAssertEqualWithAccuracy([self failures:data eventType:@"utunittestfailure" attributes:utAttrs class:SFAnalyticsEventClassHardFailure], 339, 50);
}

// So many soft failures they won't fit in the buffer
- (void)testTooManySoftFailures
{
    NSDictionary* ckksAttrs = @{@"cattr" : @"cvalue"};
    NSDictionary* utAttrs = @{@"uattr" : @"uvalue"};
    for (int idx = 0; idx < 400; ++idx) {
        [_ckksAnalytics logSoftFailureForEventNamed:@"ckksunittestfailure" withAttributes:ckksAttrs];
        [_ckksAnalytics logSoftFailureForEventNamed:@"ckksunittestfailure" withAttributes:ckksAttrs];
        [_sosAnalytics logSoftFailureForEventNamed:@"utunittestfailure" withAttributes:utAttrs];
    }

    NSDictionary* data = [self getJSONDataFromSupd];
    [self inspectDataBlobStructure:data];

    [self checkTotalEventCount:data hard:0 soft:998];
    // Based on threshold = records_to_upload/10 with a nice margin
    XCTAssertEqualWithAccuracy([self failures:data eventType:@"ckksunittestfailure" attributes:ckksAttrs class:SFAnalyticsEventClassSoftFailure], 665, 50);
    XCTAssertEqualWithAccuracy([self failures:data eventType:@"utunittestfailure" attributes:utAttrs class:SFAnalyticsEventClassSoftFailure], 332, 50);
}

- (void)testTooManyCombinedFailures
{
    NSDictionary* ckksAttrs = @{@"cattr1" : @"cvalue1", @"cattrthatisalotlongerthanthepreviousone" : @"cvaluethatisalsoalotlongerthantheother"};
    NSDictionary* utAttrs = @{@"uattr" : @"uvalue", @"uattrthatisalotlongerthanthepreviousone" : @"uvaluethatisalsoalotlongerthantheother"};
    for (int idx = 0; idx < 400; ++idx) {
        [_ckksAnalytics logHardFailureForEventNamed:@"ckksunittestfailure" withAttributes:ckksAttrs];
        [_ckksAnalytics logSoftFailureForEventNamed:@"ckksunittestfailure" withAttributes:ckksAttrs];
        [_sosAnalytics logHardFailureForEventNamed:@"utunittestfailure" withAttributes:utAttrs];
        [_sosAnalytics logSoftFailureForEventNamed:@"utunittestfailure" withAttributes:utAttrs];
    }

    NSDictionary* data = [self getJSONDataFromSupd];
    [self inspectDataBlobStructure:data];

    [self checkTotalEventCount:data hard:800 soft:198];
    // Based on threshold = records_to_upload/10 with a nice margin
    XCTAssertEqualWithAccuracy([self failures:data eventType:@"ckksunittestfailure" attributes:ckksAttrs class:SFAnalyticsEventClassHardFailure], 400, 50);
    XCTAssertEqualWithAccuracy([self failures:data eventType:@"utunittestfailure" attributes:utAttrs class:SFAnalyticsEventClassHardFailure], 400, 50);
    XCTAssertEqualWithAccuracy([self failures:data eventType:@"ckksunittestfailure" attributes:ckksAttrs class:SFAnalyticsEventClassSoftFailure], 100, 50);
    XCTAssertEqualWithAccuracy([self failures:data eventType:@"utunittestfailure" attributes:utAttrs class:SFAnalyticsEventClassSoftFailure], 100, 50);
}

// There's an even number of samples
- (void)testSamplesEvenSampleCount
{
    NSString* sampleNameEven = @"evenSample";

    for (NSNumber* value in @[@36.831855250339714, @90.78721762172914, @49.24392301762506,
                              @42.806362283260036, @16.76725375576855, @34.50969130579674,
                              @25.956509180834637, @36.8268555935645, @35.54069258036879,
                              @7.26364884595062, @45.414180770615395, @5.223213570809022]) {
        [_ckksAnalytics logMetric:value withName:sampleNameEven];
    }

    NSDictionary* data = [self getJSONDataFromSupd];
    [self inspectDataBlobStructure:data];

    // min, max, avg, med, dev, 1q, 3q
    [self checkTotalEventCount:data hard:0 soft:0 accuracy:0];
    [self sampleStatisticsInEvents:data[@"events"] name:sampleNameEven values:@[@5.22, @90.78, @35.60, @36.18, @21.52, @21.36, @44.11]];
}

// There are 4*n + 1 samples
- (void)testSamples4n1SampleCount
{
    NSString* sampleName4n1 = @"4n1Sample";
    for (NSNumber* value in @[@37.76544251068022, @27.36378948426223, @45.10503077614114,
                              @43.90635413191473, @54.78709742040113, @52.34879597889124,
                              @70.95760312196856, @23.23648158872921, @75.34678687445064,
                              @10.723238854026203, @41.98468801166455, @17.074404554908476,
                              @94.24252031232739]) {
        [_ckksAnalytics logMetric:value withName:sampleName4n1];
    }

    NSDictionary* data = [self getJSONDataFromSupd];
    [self inspectDataBlobStructure:data];

    [self checkTotalEventCount:data hard:0 soft:0 accuracy:0];
    [self sampleStatisticsInEvents:data[@"events"] name:sampleName4n1 values:@[@10.72, @94.24, @45.76, @43.90, @23.14, @26.33, @58.83]];
}

// There are 4*n + 3 samples
- (void)testSamples4n3SampleCount
{
    NSString* sampleName4n3 = @"4n3Sample";

    for (NSNumber* value in @[@42.012971885655496, @87.85629592375282, @5.748491212287082,
                              @38.451850063872975, @81.96900109690873, @99.83098790545392,
                              @80.89400981437815, @5.719237885152143, @1.6740622555032196,
                              @14.437000556079038, @29.046050177512395]) {
        [_sosAnalytics logMetric:value withName:sampleName4n3];
    }

    NSDictionary* data = [self getJSONDataFromSupd];
    [self inspectDataBlobStructure:data];
    [self checkTotalEventCount:data hard:0 soft:0 accuracy:0];

    [self sampleStatisticsInEvents:data[@"events"] name:sampleName4n3 values:@[@1.67, @99.83, @44.33, @38.45, @35.28, @7.92, @81.70]];
}

// stddev and quartiles undefined for single sample
- (void)testSamplesSingleSample
{
    NSString* sampleName = @"singleSample";

    [_ckksAnalytics logMetric:@3.14159 withName:sampleName];

    NSDictionary* data = [self getJSONDataFromSupd];
    [self inspectDataBlobStructure:data];
    [self checkTotalEventCount:data hard:0 soft:0 accuracy:0];

    [self sampleStatisticsInEvents:data[@"events"] name:sampleName values:@[@3.14159]];
}

// quartiles meaningless for fewer than 4 samples (but stddev exists)
- (void)testSamplesFewerThanFour
{
    NSString* sampleName = @"fewSamples";

    [_ckksAnalytics logMetric:@3.14159 withName:sampleName];
    [_ckksAnalytics logMetric:@6.28318 withName:sampleName];

    NSDictionary* data = [self getJSONDataFromSupd];
    [self inspectDataBlobStructure:data];
    [self checkTotalEventCount:data hard:0 soft:0 accuracy:0];

    [self sampleStatisticsInEvents:data[@"events"] name:sampleName values:@[@3.14, @6.28, @4.71, @4.71, @1.57]];
}

- (void)testSamplesSameNameDifferentSubclass
{
    NSString* sampleName = @"differentSubclassSamples";

    [_sosAnalytics logMetric:@313.37 withName:sampleName];
    [_ckksAnalytics logMetric:@313.37 withName:sampleName];

    NSDictionary* data = [self getJSONDataFromSupd];
    [self inspectDataBlobStructure:data];
    [self checkTotalEventCount:data hard:0 soft:0 accuracy:0];

    [self sampleStatisticsInEvents:data[@"events"] name:sampleName values:@[@313.37] amount:2];
}

- (void)testInvalidJSON
{
    NSData* bad = [@"let's break supd!" dataUsingEncoding:NSUTF8StringEncoding];
    [_ckksAnalytics logHardFailureForEventNamed:@"testEvent" withAttributes:@{ @"dataAttribute" : bad}];

    NSDictionary* data = [self getJSONDataFromSupd];
    XCTAssertNotNil(data);
    XCTAssertNotNil(data[@"events"]);
    NSUInteger foundErrorEvents = 0;
    for (NSDictionary* event in data[@"events"]) {
        if ([event[SFAnalyticsEventType] isEqualToString:SFAnalyticsEventTypeErrorEvent] && [event[SFAnalyticsEventErrorDestription] isEqualToString:@"JSON:testEvent"]) {
            ++foundErrorEvents;
        }
    }
    XCTAssertEqual(foundErrorEvents, 1);
}


// TODO
- (void)testGetSysdiagnoseDump
{
    
}

// TODO (need mock server)
- (void)testSplunkUpload
{

}

// TODO (need mock server)
- (void)testDBIsEmptiedAfterUpload
{

}

@end

#endif  // !TARGET_OS_SIMULATOR

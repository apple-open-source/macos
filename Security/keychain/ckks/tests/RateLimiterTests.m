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

#import <Foundation/Foundation.h>
#import <Foundation/NSKeyedArchiver_Private.h>
#import <XCTest/XCTest.h>
#import "keychain/ckks/RateLimiter.h"

@interface TestObject : NSObject
@property NSString *uuid;
- (NSString *)invalid;
@end

@implementation TestObject
- (instancetype)init {
    self = [super init];
    if (self) {
        _uuid = [[NSUUID UUID] UUIDString];
    }
    return self;
}

- (instancetype)initWithNilUuid {
    self = [super init];
    if (self) {
        _uuid = nil;
    }
    return self;
}

// It's super illegal for this to get called because of property allowlisting
- (NSString *)invalid {
    NSAssert(NO, @"'invalid' is not an approved property");
    return nil;
}
@end

@interface RateLimiterTests : XCTestCase
@property NSDictionary *config;
@property NSString *filepath;
@property NSDate *time;
@property RateLimiter *RL;
@property TestObject *obj;
@end

@implementation RateLimiterTests

- (void)setUp {
    [super setUp];
    // instantiate config, write to disk
    NSData *configData = [@"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\
<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\
<plist version=\"1.0\">\
<dict>\
    <key>general</key>\
    <dict>\
        <key>maxStateSize</key>\
        <integer>250</integer>\
        <key>maxItemAge</key>\
        <integer>3600</integer>\
        <key>overloadDuration</key>\
        <integer>1800</integer>\
        <key>name</key>\
        <string>CKKS</string>\
        <key>MAType</key>\
        <string></string>\
    </dict>\
    <key>groups</key>\
    <array>\
        <dict>\
            <key>property</key>\
            <string>global</string>\
            <key>capacity</key>\
            <integer>20</integer>\
            <key>rate</key>\
            <integer>30</integer>\
            <key>badness</key>\
            <integer>1</integer>\
        </dict>\
        <dict>\
            <key>property</key>\
            <string>uuid</string>\
            <key>capacity</key>\
            <integer>3</integer>\
            <key>rate</key>\
            <integer>600</integer>\
            <key>badness</key>\
            <integer>3</integer>\
        </dict>\
        <dict>\
            <key>property</key>\
            <string>invalid</string>\
            <key>capacity</key>\
            <integer>0</integer>\
            <key>rate</key>\
            <integer>60000</integer>\
            <key>badness</key>\
            <integer>4</integer>\
	    </dict>\
    </array>\
</dict>\
</plist>\
" dataUsingEncoding:NSUTF8StringEncoding];
    NSError *err = nil;
    _config = [NSPropertyListSerialization propertyListWithData:configData options:NSPropertyListImmutable format:nil error:&err];
    if (!_config) {
        XCTFail(@"Could not deserialize property list: %@", err);
    }
    _filepath = [NSString stringWithFormat:@"/tmp/ratelimitertests_%@.plist", [[NSUUID UUID] UUIDString]];
    if (![configData writeToFile:_filepath atomically:NO]) {
        XCTFail(@"Could not write plist to %@", _filepath);
    }
    _RL = [[RateLimiter alloc] initWithConfig:_config];
    _obj = [TestObject new];
    _time = [NSDate date];
}

- (void)tearDown {
    NSError *err = nil;
    if (![[NSFileManager defaultManager] removeItemAtPath:_filepath error:&err]) {
        XCTFail(@"Couldn't delete file %@: %@", _filepath, err);
    }
    [super tearDown];
}

- (void)testInitWithConfig {
    self.RL = [[RateLimiter alloc] initWithConfig:self.config];
    XCTAssertNotNil(self.RL, @"RateLimiter with config succeeds");
    XCTAssertNil(self.RL.assetType, @"initWithConfig means no assetType");
    XCTAssertEqualObjects(self.config, self.RL.config, @"config was copied properly");
}

- (void)testInitWithPlist {
    RateLimiter *RL = [[RateLimiter alloc] initWithPlistFromURL:[NSURL URLWithString:[NSString stringWithFormat:@"file://%@", self.filepath]]];
    XCTAssertNotNil(RL, @"RateLimiter with plist succeeds");
    XCTAssertNil(RL.assetType, @"initWithPlist means no assetType");
    XCTAssertEqualObjects(self.config, RL.config, @"config was loaded properly");
    RL = [[RateLimiter alloc] initWithPlistFromURL:[NSURL URLWithString:[NSString stringWithFormat:@"file://%@.nonexisting", self.filepath]]];
    XCTAssertNil(RL, "Cannot instantiate RateLimiter with invalid plist URL");
}

- (void)testEncodingAndDecoding {
    NSDate* date = [NSDate date];
    NSDate* limit = nil;
    [self.RL judge:self.obj at:date limitTime:&limit];

    NSKeyedArchiver *encoder = [[NSKeyedArchiver alloc] initRequiringSecureCoding:YES];
    [self.RL encodeWithCoder:encoder];
    NSData* data = encoder.encodedData;

    XCTAssertEqualObjects(self.config, self.RL.config, @"config unmodified after encoding");
    XCTAssertNil(self.RL.assetType, @"assetType still nil after encoding");

    NSKeyedUnarchiver *decoder = [[NSKeyedUnarchiver alloc] initForReadingFromData:data error:nil];
    RateLimiter *RL2 = [[RateLimiter alloc] initWithCoder:decoder];
    XCTAssertNotNil(RL2, @"Received an object from initWithCoder");
    XCTAssertEqualObjects(self.RL.config, RL2.config, @"config is the same after encoding and decoding");
    XCTAssertTrue([self.RL isEqual:RL2], @"RateLimiters believe they are the same");
    XCTAssertNil(RL2.assetType, @"assetType remains nil");
}

- (void)testInitWithAssetType {
    // Not implemented yet, expect nil
    XCTAssertNil([[RateLimiter alloc] initWithAssetType:@"test"]);
}

- (void)testReset {
    NSDate *limitTime = nil;
    [self.RL judge:[TestObject new] at:self.time limitTime:&limitTime];
    XCTAssertEqual([self.RL stateSize], 2ul, @"Single property judged once, state is 1 global plus 1 property");
    [self.RL reset];
    XCTAssertEqual([self.RL stateSize], 0ul, @"No buckets after reset");
    XCTAssertEqualObjects(self.config, self.RL.config);
}

// Cause it to complain based on one item being hit repeatedly
- (void)testJudgeSingleItem {
    NSDate *limitTime = nil;
    for (int idx = 0; idx < [self.config[@"groups"][1][@"capacity"] intValue]; ++idx) {
        XCTAssertEqual([self.RL judge:self.obj at:self.time limitTime:&limitTime], RateLimiterBadnessClear, @"Received RateLimiterBadnessClear");
        XCTAssertNil(limitTime, @"single object, clear to process right now");
    }

    XCTAssertEqual([self.RL judge:self.obj at:self.time limitTime:&limitTime], RateLimiterBadnessGridlocked, @"Received RateLimiterBadnessGridlocked");
    XCTAssertNotNil(limitTime, @"After hammering same object need to wait now");
    XCTAssertEqualObjects(limitTime, [self.time dateByAddingTimeInterval:[self.config[@"groups"][1][@"rate"] intValue]], @"time: %@, process-OK time is time + rate (%d)", self.time, [self.config[@"groups"][1][@"rate"] intValue]);
}

// Cause it to complain based on too many items in total
- (void)testJudgeRandomItems {
    NSDate *limitTime = nil;
    TestObject *obj;
    for (int idx = 0; idx < [self.config[@"groups"][0][@"capacity"] intValue]; ++idx) {
        obj = [TestObject new];
        XCTAssertEqual([self.RL judge:obj at:self.time limitTime:&limitTime], RateLimiterBadnessClear, @"Received RateLimiterBadnessClear");
        XCTAssertNil(limitTime, @"single object, clear to process right now");
    }

    XCTAssertEqual([self.RL judge:obj at:self.time limitTime:&limitTime], RateLimiterBadnessCongested, @"Received RateLimiterBadnessCongested");
    XCTAssertNotNil(limitTime, @"After hammering same object need to wait now");
    XCTAssertEqualObjects(limitTime, [self.time dateByAddingTimeInterval:[self.config[@"groups"][0][@"rate"] intValue]], @"time: %@, process-OK time is time + rate (%d)", self.time, [self.config[@"groups"][0][@"rate"] intValue]);
}

- (void)testOverload {
    NSDate *limitTime = nil;
    while ([self.RL stateSize] <= [self.config[@"general"][@"maxStateSize"] unsignedIntegerValue]) {
        TestObject *obj = [TestObject new];
        RateLimiterBadness rlb = [self.RL judge:obj at:self.time limitTime:&limitTime];
        XCTAssertTrue(rlb != RateLimiterBadnessOverloaded, @"No issues judging random objects under max state size");
    }

    // While check is performed at the start of the loop, so now stateSize > maxStateSize. Judge should realize this right away, try to cope, fail and throw a fit
    XCTAssertEqual([self.RL judge:self.obj at:self.time limitTime:&limitTime], RateLimiterBadnessOverloaded, @"RateLimiter overloaded");
    XCTAssertEqualObjects(limitTime, [self.time dateByAddingTimeInterval:[self.config[@"general"][@"overloadDuration"] intValue]], @"Overload duration matches expectations");
}

- (void)testTrimmingDueToTime {
    NSDate *limitTime = nil;
    for (int idx = 0; idx < [self.config[@"general"][@"maxStateSize"] intValue]/2; ++idx) {
        TestObject *obj = [TestObject new];
        [self.RL judge:obj at:self.time limitTime:&limitTime];
    }
    NSUInteger stateSize = [self.RL stateSize];
    XCTAssertEqual(stateSize, [self.config[@"general"][@"maxStateSize"] unsignedIntegerValue] / 2 + 1, @"Number of objects added matches expectations");
    // Advance time enough to age out the existing objects
    NSDate *time = [self.time dateByAddingTimeInterval:[self.config[@"general"][@"maxItemAge"] intValue] + 1];

    // It's been so long, judge should first trim and decide to throw away everything it has
    XCTAssertEqual([self.RL judge:self.obj at:time limitTime:&limitTime], RateLimiterBadnessClear, @"New judgment after long time goes fine");
    XCTAssertEqual([self.RL stateSize], 2ul, @"Old items gone, just global and one new item left");
}

// RateLimiter is set to ignore properties that return nil
- (void)testNilUuid {
    NSDate *limitTime = nil;
    TestObject *obj = [[TestObject alloc] initWithNilUuid];
    for (int idx = 0; idx < [self.config[@"groups"][0][@"capacity"] intValue]; ++idx) {
        XCTAssertEqual([self.RL judge:obj at:self.time limitTime:&limitTime], RateLimiterBadnessClear, @"Same object with nil property only judged on global rate");
        XCTAssertEqual([self.RL stateSize], 1ul, @"Nil property objects can't be added to state");
    }
}

- (void)testTrimmingDueToSize {
    NSDate *limitTime = nil;
    // Put first half of items in
    for (int idx = 0; idx < [self.config[@"general"][@"maxStateSize"] intValue] / 2; ++idx) {
        TestObject *obj = [TestObject new];
        [self.RL judge:obj at:self.time limitTime:&limitTime];
    }

    NSDate *time = [self.time dateByAddingTimeInterval:[self.config[@"general"][@"maxItemAge"] intValue] / 2];

    // Put second half in later so trim has something to do afterwards
    while ([self.RL stateSize] <= [self.config[@"general"][@"maxStateSize"] unsignedIntegerValue]) {
        TestObject *obj = [TestObject new];
        RateLimiterBadness rlb = [self.RL judge:obj at:time limitTime:&limitTime];
        XCTAssertTrue(rlb != RateLimiterBadnessOverloaded, @"No issues judging random objects under max state size");
    }

    NSUInteger expectedStateSize = [self.RL stateSize] - [self.config[@"general"][@"maxStateSize"] intValue] / 2 + 1;

    // Advance time past first batch but before second batch
    time = [self.time dateByAddingTimeInterval:[self.config[@"general"][@"maxItemAge"] intValue] + 1];
    // ...which requires adjusting for the fact that the token buckes will be almost full (i.e. further in the past)
    time = [time dateByAddingTimeInterval:-(([self.config[@"groups"][1][@"capacity"] integerValue] - 1) * [self.config[@"groups"][1][@"rate"] integerValue])];

    XCTAssertNotEqual([self.RL judge:self.obj at:time limitTime:&limitTime], RateLimiterBadnessOverloaded, @"Judgment caused RL to trim out old items");
    XCTAssertEqual([self.RL stateSize], expectedStateSize);
}

@end

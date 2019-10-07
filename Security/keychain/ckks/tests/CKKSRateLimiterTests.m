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

#import <XCTest/XCTest.h>
#import <Foundation/NSKeyedArchiver_Private.h>
#import "keychain/ckks/CKKSOutgoingQueueEntry.h"
#import "keychain/ckks/CKKSRateLimiter.h"

@interface CKKSRateLimiterTests : XCTestCase
- (CKKSOutgoingQueueEntry *)oqe;
@property CKKSRateLimiter *rl;
@property NSDictionary *conf;
@property CKKSOutgoingQueueEntry *oqe;
@property NSDate *date;
@property NSDate *compare;
@end

@implementation CKKSRateLimiterTests

- (void)setUp {
    [super setUp];
    self.rl = [CKKSRateLimiter new];
    self.conf = [self.rl config];
    self.oqe = [[CKKSOutgoingQueueEntry alloc] initWithCKKSItem:
                [[CKKSItem alloc] initWithUUID:@"123"
                                 parentKeyUUID:@""
                                        zoneID:[[CKRecordZoneID alloc] initWithZoneName:@"testzone" ownerName:CKCurrentUserDefaultName]
                                       encItem:nil
                                    wrappedkey:nil
                               generationCount:1
                                        encver:0]
                                                         action:@""
                                                          state:@""
                                                      waitUntil:nil
                                                    accessGroup:@"defaultgroup"];
}

- (void)tearDown {
    [super tearDown];
    self.rl = nil;
    self.conf = nil;
    self.oqe = nil;
}

- (int)get:(NSDictionary *)dict key:(NSString *)key {
    id obj = dict[key];
    XCTAssertNotNil(obj, "Key %@ is in the dictionary", key);
    XCTAssert([obj isKindOfClass:[NSNumber class]], "Value for %@ is an NSNumber (%@)", key, [obj class]);
    XCTAssertGreaterThan([obj intValue], 0, "Value for %@ is at least non-zero", key);
    return [obj intValue];
}

- (unsigned)getUnsigned:(NSDictionary *)dict key:(NSString *)key {
    id obj = dict[key];
    XCTAssertNotNil(obj, "Key %@ is in the dictionary", key);
    XCTAssert([obj isKindOfClass:[NSNumber class]], "Value for %@ is an NSNumber (%@)", key, [obj class]);
    XCTAssertGreaterThan([obj unsignedIntValue], 0, "Value for %@ is at least non-zero", key);
    return [obj unsignedIntValue];
}

- (void)testConfig {
    [self get:[self.rl config] key:@"rateAll"];
    [self get:[self.rl config] key:@"rateGroup"];
    [self get:[self.rl config] key:@"rateUUID"];
    [self get:[self.rl config] key:@"capacityAll"];
    [self get:[self.rl config] key:@"capacityGroup"];
    [self get:[self.rl config] key:@"capacityUUID"];
    [self get:[self.rl config] key:@"trimSize"];
    [self get:[self.rl config] key:@"trimTime"];
}

- (void)testBasics {
    NSDate *date = [NSDate date];
    NSDate *limit = nil;
    
    XCTAssertEqual([self.rl judge:self.oqe at:date limitTime:&limit], 0, "judge single item");
    XCTAssertNil(limit, "NSDate argument nil for zero result");
    XCTAssertEqual([self.rl stateSize], 3UL, "RL contains three nodes after single judgment");
    [self.rl reset];
    XCTAssertEqual([self.rl stateSize], 0UL, "RL is empty after reset");
}

- (void)testAll {
    NSDate *date = [NSDate date];
    NSDate *limit = nil;
    int capacityAll = [self get:[self.rl config] key:@"capacityAll"];
    int rateAll = [self get:[self.rl config] key:@"rateAll"];
    
    for (int idx = 0; idx < capacityAll; ++idx) {
        self.oqe.accessgroup = [NSString stringWithFormat:@"%d", idx];
        self.oqe.uuid = [NSString stringWithFormat:@"%d", idx];
        XCTAssertEqual([self.rl judge:self.oqe at:date limitTime:&limit], 0, "Repeat (%d) All succeeds, limit %d", idx, capacityAll);
        XCTAssertNil(limit, "Time nil while under All limit");
    }

    self.oqe.accessgroup = @"Ga";
    self.oqe.uuid = @"Ua";
    XCTAssertEqual([self.rl judge:self.oqe at:date limitTime:&limit], 1, "Repeat All implies congestion");
    int delta = [limit timeIntervalSinceDate:date];
    XCTAssert(delta > 0 && delta <= rateAll, "send-OK at most one All token into the future");
    self.oqe.accessgroup = @"Gb";
    self.oqe.uuid = @"Ub";
    XCTAssertEqual([self.rl judge:self.oqe at:date limitTime:&limit], 1, "Repeat All, still congested");
    delta = [limit timeIntervalSinceDate:date];
    XCTAssert(delta > rateAll && delta <= (2 * rateAll), "send-OK between one and two All tokens into the future");
    
    self.oqe.accessgroup = @"Gc";
    self.oqe.uuid = @"Uc";
    date = [limit dateByAddingTimeInterval:rateAll];
    XCTAssertEqual([self.rl judge:self.oqe at:date limitTime:&limit], 0, "Repeat All is fine after waiting");
    XCTAssertEqual([self.rl stateSize], (unsigned long)(2 * (capacityAll + 3) + 1), "state size is %d", 2 * (capacityAll + 3) + 1);
}

- (void)testGroup {
    NSDate *date = [NSDate date];
    NSDate *limit = nil;
    int capacityGroup = [self get:[self.rl config] key:@"capacityGroup"];
    int rateGroup = [self get:[self.rl config] key:@"rateGroup"];
    
    for (int idx = 0; idx < capacityGroup; ++idx) {
        self.oqe.uuid = [NSString stringWithFormat:@"%d",idx];
        XCTAssertEqual([self.rl judge:self.oqe at:date limitTime:&limit], 0, "Repeat (%d) Group succeeds, limit %d", idx, capacityGroup);
        XCTAssertNil(limit, "sendTime nil while under Group limit");
    }
    
    self.oqe.uuid = @"a";
    XCTAssertEqual([self.rl judge:self.oqe at:date limitTime:&limit], 2, "Repeat Group entry not good");
    int delta = [limit timeIntervalSinceDate:date];
    XCTAssert(delta > 0 && delta <= rateGroup, "send-OK at most one Group token into the future");

    self.oqe.uuid = @"b";
    XCTAssertEqual([self.rl judge:self.oqe at:date limitTime:&limit], 3, "Repeat Group entry still not good");
    delta = [limit timeIntervalSinceDate:date];
    XCTAssert(delta > rateGroup && delta <= (2 * rateGroup), "send-OK between one and two Group tokens into the future");
    self.oqe.uuid = @"c";
    XCTAssertEqual([self.rl judge:self.oqe at:date limitTime:&limit], 3, "Repeat Group entry extra bad after not quitting");
    delta = [limit timeIntervalSinceDate:date];
    XCTAssert(delta > (2 * rateGroup) && delta <= (3 * rateGroup), "send-OK between two and three Group tokens into the future");

    self.oqe.uuid = @"d";
    date = [limit dateByAddingTimeInterval:rateGroup];
    XCTAssertEqual([self.rl judge:self.oqe at:date limitTime:&limit], 0, "Repeat Group is fine after waiting");
    XCTAssertEqual([self.rl stateSize], (unsigned long)(capacityGroup + 6), "State size is %d", capacityGroup + 6);
    
}

- (void)testUUID {
    NSDate *date = [NSDate date];
    NSDate *limit = nil;
    int capacityUUID = [self get:[self.rl config] key:@"capacityUUID"];
    int rateUUID = [self get:[self.rl config] key:@"rateUUID"];

    for (int idx = 0; idx < capacityUUID; ++idx) {
        XCTAssertEqual([self.rl judge:self.oqe at:date limitTime:&limit], 0, "Repeat (%d) UUID succeeds, limit %d", idx, capacityUUID);
        XCTAssertNil(limit, "Time unmodified while under UUID limit");
    }

    XCTAssertEqual([self.rl judge:self.oqe at:date limitTime:&limit], 4, "Repeat UUID over limit is bad behavior");
    int delta = [limit timeIntervalSinceDate:date];
    XCTAssert(delta > 0 && delta <= rateUUID, "Received send-OK at most one UUID token into the future");
    XCTAssertEqual([self.rl judge:self.oqe at:date limitTime:&limit], 4, "Repeat over-limit UUID is bad behavior");
    delta = [limit timeIntervalSinceDate:date];
    XCTAssert(delta > rateUUID && delta <= (2 * rateUUID), "send-OK between one and two UUID tokens into the future");

    // test UUID success after borrow
    date = [limit dateByAddingTimeInterval:rateUUID];
    XCTAssertEqual([self.rl judge:self.oqe at:date limitTime:&limit], 0, "Repeat UUID is fine after waiting");
    XCTAssertEqual([self.rl stateSize], 3UL, "state size is 3");
    
}

- (void)testTrimTime {
    NSDate *date = [NSDate date];
    NSDate *limit = nil;
    int trimTime = [self get:[self.rl config] key:@"trimTime"];
    int capacityAll = [self get:[self.rl config] key:@"capacityAll"];
    
    for (int idx = 0; idx < capacityAll; ++idx) {
        self.oqe.accessgroup = [NSString stringWithFormat:@"%d", idx];
        self.oqe.uuid = [NSString stringWithFormat:@"%d", idx];
        XCTAssertEqual([self.rl judge:self.oqe at:date limitTime:&limit], 0, "Repeat (%d) All succeeds, limit %d", idx, capacityAll);
        XCTAssertNil(limit, "Time nil while under All limit");
    }
    
    XCTAssertEqual([self.rl stateSize],  (unsigned long)(2 * capacityAll + 1), "state size is %d", 2 * capacityAll + 1);
    
    date = [date dateByAddingTimeInterval:trimTime + 1];
    XCTAssertEqual([self.rl judge:self.oqe at:date limitTime:&limit], 0, "Entry addition succeeds after long silence");
    XCTAssertEqual([self.rl stateSize], 3UL, "Trim has wiped out all buckets except from call immediately before");
}

- (void)testTrimSize {
    NSDate *date = [NSDate date];
    NSDate *limit = nil;
    NSDate *compare = [date copy];
    int trimSize =  [self get:[self.rl config] key:@"trimSize"];
    int trimTime =  [self get:[self.rl config] key:@"trimTime"];
    int rateAll = [self get:[self.rl config] key:@"rateAll"];
    
    self.oqe.accessgroup = @"a";
    self.oqe.uuid = @"a";
    XCTAssertEqual([self.rl judge:self.oqe at:date limitTime:&limit], 0, "Adding single item, no problem");
    
    // Cannot fill state completely due to repeated judgements, it'll trigger overload otherwise
    for (int idx = 0; [self.rl stateSize] < (unsigned long)(trimSize - 2); ++idx) {
        self.oqe.accessgroup = [NSString stringWithFormat:@"%d", idx];
        self.oqe.uuid = [NSString stringWithFormat:@"%d", idx];
        for (int i = 0; i < rateAll; ++i) {
            XCTAssertLessThan([self.rl judge:self.oqe at:date limitTime:&limit], 5, "Adding items and hitting their dates");
            date = [compare copy];
        }
    }
    
    unsigned long statesize = [self.rl stateSize];
    
    self.oqe.accessgroup = @"b";
    self.oqe.uuid = @"b";
    XCTAssertLessThan([self.rl judge:self.oqe at:date limitTime:&limit], 5, "Top off state");

    date = [compare dateByAddingTimeInterval:trimTime];
    
    XCTAssertLessThan([self.rl judge:self.oqe at:date limitTime:&limit], 5, "New entry after at least one has expired fits");
    XCTAssertEqual([self.rl stateSize], statesize, "Item 'a' was trimmed and 'b' got added");
}

- (void)testOverload {
    NSDate *date = [NSDate date];
    NSDate *limit = nil;
    int trimSize = [self get:[self.rl config] key:@"trimSize"];
    //int rateAll = [self get:[self.rl config] key:@"rateAll"];;
    unsigned overloadDuration = [self getUnsigned:[self.rl config] key:@"overloadDuration"];;

    for (int idx = 0; idx < (trimSize / 2); ++idx) {
        self.oqe.accessgroup = [NSString stringWithFormat:@"%d", idx];
        self.oqe.uuid = [NSString stringWithFormat:@"%d", idx];
        XCTAssertLessThan([self.rl judge:self.oqe at:date limitTime:&limit], 5, "No overload triggered yet ramming data into RL");
    }

    NSDate *preOverload = [limit copy];
    self.oqe.accessgroup = @"a";
    self.oqe.uuid = @"a";
    XCTAssertEqual([self.rl judge:self.oqe at:date limitTime:&limit], 5, "Triggered overload");
    NSDate *postOverload = [preOverload dateByAddingTimeInterval:overloadDuration];
    // NSDates are doubles so we add a little rounding error leeway
    XCTAssertEqualWithAccuracy(limit.timeIntervalSinceReferenceDate, postOverload.timeIntervalSinceReferenceDate, 1, "Got expected overload duration");

    unsigned long statesize = [self.rl stateSize];
    
    self.oqe.accessgroup = @"b";
    self.oqe.uuid = @"b";
    XCTAssertEqual([self.rl judge:self.oqe at:date limitTime:&limit], 5, "Overload");
    XCTAssertEqual(statesize, [self.rl stateSize], "No items added during overload");
    
    XCTAssertEqual([self.rl judge:self.oqe at:limit limitTime:&limit], 0, "Judgment succeeds post-overload");
}

- (void)testEncoding {
    NSDate* date = [NSDate date];
    NSDate* limit = nil;
    [self.rl judge:self.oqe at:date limitTime:&limit];

    NSKeyedArchiver* encoder = [[NSKeyedArchiver alloc] initRequiringSecureCoding:YES];
    [encoder encodeObject: self.rl forKey:@"unneeded"];
    NSData* data = encoder.encodedData;
    XCTAssertNotNil(data, "Still have our data object");
    XCTAssertTrue(data.length > 0u, "Encoder produced some data");

    NSKeyedUnarchiver* decoder = [[NSKeyedUnarchiver alloc] initForReadingFromData: data error:nil];
    CKKSRateLimiter* rl = [decoder decodeObjectOfClass: [CKKSRateLimiter class] forKey:@"unneeded"];
    XCTAssertNotNil(rl, "Decoded data into a CKKSRateLimiter");

    XCTAssertEqualObjects(self.rl, rl, "RateLimiter objects are equal after encoding/decoding");
}

@end

#endif //OCTAGON

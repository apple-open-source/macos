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
#import <XCTest/XCTest.h>
#import "keychain/ckks/CKKSCondition.h"

@interface CKKSConditionTests : XCTestCase
@end

@implementation CKKSConditionTests

- (void)setUp {
    [super setUp];
}

- (void)tearDown {
    [super tearDown];
}

-(void)testConditionAfterFulfill {
    CKKSCondition* c = [[CKKSCondition alloc] init];

    [c fulfill];
    XCTAssertEqual(0, [c wait:100*NSEC_PER_MSEC], "first wait after fulfill succeeds");
    XCTAssertEqual(0, [c wait:100*NSEC_PER_MSEC], "second wait after fulfill succeeds");
    XCTAssertEqual(0, [c wait:100*NSEC_PER_MSEC], "third wait after fulfill succeeds");
}

-(void)testConditionTimeout {
    CKKSCondition* c = [[CKKSCondition alloc] init];
    XCTAssertNotEqual(0, [c wait:100*NSEC_PER_MSEC], "waiting without fulfilling times out");
}

-(void)testConditionWait {
    CKKSCondition* c = [[CKKSCondition alloc] init];

    dispatch_queue_t queue = dispatch_queue_create("testConditionWait", DISPATCH_QUEUE_CONCURRENT);

    XCTestExpectation *toofastexpectation = [self expectationWithDescription:@"wait ended (too soon)"];
    toofastexpectation.inverted = YES;

    XCTestExpectation *expectation = [self expectationWithDescription:@"wait ended"];

    dispatch_async(queue, ^{
        XCTAssertEqual(0, [c wait:1*NSEC_PER_SEC], "Wait did not time out");
        [toofastexpectation fulfill];
        [expectation fulfill];
    });

    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(250 * NSEC_PER_MSEC)), queue, ^{
        [c fulfill];
    });

    // Make sure it waits at least 0.1 seconds
    [self waitForExpectations: @[toofastexpectation] timeout:0.1];

    // But finishes within .6s (total)
    [self waitForExpectations: @[expectation] timeout:0.5];
}

-(void)testConditionChain {
    CKKSCondition* chained = [[CKKSCondition alloc] init];
    CKKSCondition* c = [[CKKSCondition alloc] initToChain: chained];

    XCTAssertNotEqual(0, [chained wait:50*NSEC_PER_MSEC], "waiting on chained condition without fulfilling times out");
    XCTAssertNotEqual(0, [c       wait:50*NSEC_PER_MSEC], "waiting on condition without fulfilling times out");

    [c fulfill];
    XCTAssertEqual(0, [c       wait:100*NSEC_PER_MSEC], "first wait after fulfill succeeds");
    XCTAssertEqual(0, [chained wait:100*NSEC_PER_MSEC], "first chained wait after fulfill succeeds");
    XCTAssertEqual(0, [c       wait:100*NSEC_PER_MSEC], "second wait after fulfill succeeds");
    XCTAssertEqual(0, [chained wait:100*NSEC_PER_MSEC], "second chained wait after fulfill succeeds");
}

@end

#endif /* OCTAGON */

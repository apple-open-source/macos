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

#include <dispatch/dispatch.h>
#import <XCTest/XCTest.h>
#import "keychain/ckks/CKKSNearFutureScheduler.h"

@interface CKKSNearFutureSchedulerTests : XCTestCase

@end

@implementation CKKSNearFutureSchedulerTests

- (void)setUp {
    [super setUp];
}

- (void)tearDown {
    [super tearDown];
}

- (void)testOneShot {
    XCTestExpectation *expectation = [self expectationWithDescription:@"FutureScheduler fired"];

    CKKSNearFutureScheduler* scheduler = [[CKKSNearFutureScheduler alloc] initWithName: @"test" delay:50*NSEC_PER_MSEC keepProcessAlive:true block:^{
        [expectation fulfill];
    }];

    [scheduler trigger];

    [self waitForExpectationsWithTimeout:1 handler:nil];
}

- (void)testOneShotDelay {
    XCTestExpectation *toofastexpectation = [self expectationWithDescription:@"FutureScheduler fired (too soon)"];
    toofastexpectation.inverted = YES;

    XCTestExpectation *expectation = [self expectationWithDescription:@"FutureScheduler fired"];

    CKKSNearFutureScheduler* scheduler = [[CKKSNearFutureScheduler alloc] initWithName: @"test" delay: 200*NSEC_PER_MSEC keepProcessAlive:false block:^{
        [toofastexpectation fulfill];
        [expectation fulfill];
    }];

    [scheduler trigger];

    // Make sure it waits at least 0.1 seconds
    [self waitForExpectations: @[toofastexpectation] timeout:0.1];

    // But finishes within 1.1s (total)
    [self waitForExpectations: @[expectation] timeout:1];
}

- (void)testOneShotManyTrigger {
    XCTestExpectation *toofastexpectation = [self expectationWithDescription:@"FutureScheduler fired (too soon)"];
    toofastexpectation.inverted = YES;

    XCTestExpectation *expectation = [self expectationWithDescription:@"FutureScheduler fired"];
    expectation.assertForOverFulfill = YES;

    CKKSNearFutureScheduler* scheduler = [[CKKSNearFutureScheduler alloc] initWithName: @"test" delay: 200*NSEC_PER_MSEC keepProcessAlive:true block:^{
        [toofastexpectation fulfill];
        [expectation fulfill];
    }];

    [scheduler trigger];
    [scheduler trigger];
    [scheduler trigger];
    [scheduler trigger];
    [scheduler trigger];
    [scheduler trigger];
    [scheduler trigger];
    [scheduler trigger];

    // Make sure it waits at least 0.1 seconds
    [self waitForExpectations: @[toofastexpectation] timeout:0.1];

    // But finishes within .6s (total)
    [self waitForExpectations: @[expectation] timeout:0.5];

    // Ensure we don't get called again in the next 0.3 s
    XCTestExpectation* waitmore = [self expectationWithDescription:@"waiting"];
    waitmore.inverted = YES;
    [self waitForExpectations: @[waitmore] timeout: 0.3];
}


- (void)testMultiShot {
    XCTestExpectation *first = [self expectationWithDescription:@"FutureScheduler fired (one)"];
    first.assertForOverFulfill = NO;

    XCTestExpectation *second = [self expectationWithDescription:@"FutureScheduler fired (two)"];
    second.expectedFulfillmentCount = 2;
    second.assertForOverFulfill = YES;

    CKKSNearFutureScheduler* scheduler = [[CKKSNearFutureScheduler alloc] initWithName: @"test" delay: 100*NSEC_PER_MSEC keepProcessAlive:false block:^{
        [first fulfill];
        [second fulfill];
    }];

    [scheduler trigger];

    [self waitForExpectations: @[first] timeout:0.2];

    [scheduler trigger];
    [scheduler trigger];
    [scheduler trigger];

    [self waitForExpectations: @[second] timeout:0.2];

    XCTestExpectation* waitmore = [self expectationWithDescription:@"waiting"];
    waitmore.inverted = YES;
    [self waitForExpectations: @[waitmore] timeout: 0.2];
}

- (void)testMultiShotDelays {
    XCTestExpectation *first = [self expectationWithDescription:@"FutureScheduler fired (one)"];
    first.assertForOverFulfill = NO;

    XCTestExpectation *longdelay = [self expectationWithDescription:@"FutureScheduler fired (long delay expectation)"];
    longdelay.inverted = YES;
    longdelay.expectedFulfillmentCount = 2;

    XCTestExpectation *second = [self expectationWithDescription:@"FutureScheduler fired (two)"];
    second.expectedFulfillmentCount = 2;
    second.assertForOverFulfill = YES;

    CKKSNearFutureScheduler* scheduler = [[CKKSNearFutureScheduler alloc] initWithName: @"test" initialDelay: 50*NSEC_PER_MSEC continuingDelay:300*NSEC_PER_MSEC keepProcessAlive:false block:^{
        [first fulfill];
        [longdelay fulfill];
        [second fulfill];
    }];

    [scheduler trigger];

    [self waitForExpectations: @[first] timeout:0.2];

    [scheduler trigger];
    [scheduler trigger];
    [scheduler trigger];

    // longdelay should NOT be fulfilled twice in the first 0.3 seconds
    [self waitForExpectations: @[longdelay] timeout:0.2];

    // But second should be fulfilled in the first 0.8 seconds
    [self waitForExpectations: @[second] timeout:0.5];

    XCTestExpectation* waitmore = [self expectationWithDescription:@"waiting"];
    waitmore.inverted = YES;
    [self waitForExpectations: @[waitmore] timeout: 0.2];
}

- (void)testCancel {
    XCTestExpectation *cancelexpectation = [self expectationWithDescription:@"FutureScheduler fired (after cancel)"];
    cancelexpectation.inverted = YES;

    CKKSNearFutureScheduler* scheduler = [[CKKSNearFutureScheduler alloc] initWithName: @"test" delay: 100*NSEC_PER_MSEC keepProcessAlive:true block:^{
        [cancelexpectation fulfill];
    }];

    [scheduler trigger];
    [scheduler cancel];

    // Make sure it does not fire in 0.5 s
    [self waitForExpectations: @[cancelexpectation] timeout:0.2];
}

- (void)testDelayedNoShot {
    XCTestExpectation *toofastexpectation = [self expectationWithDescription:@"FutureScheduler fired (too soon)"];
    toofastexpectation.inverted = YES;

    CKKSNearFutureScheduler* scheduler = [[CKKSNearFutureScheduler alloc] initWithName: @"test" delay: 10*NSEC_PER_MSEC keepProcessAlive:false block:^{
        [toofastexpectation fulfill];
    }];

    // Tell the scheduler to wait, but don't trigger it. It shouldn't fire.
    [scheduler waitUntil: 50*NSEC_PER_MSEC];

    [self waitForExpectations: @[toofastexpectation] timeout:0.1];
}

- (void)testDelayedOneShot {
    XCTestExpectation *first = [self expectationWithDescription:@"FutureScheduler fired (one)"];
    first.assertForOverFulfill = NO;

    XCTestExpectation *toofastexpectation = [self expectationWithDescription:@"FutureScheduler fired (too soon)"];
    toofastexpectation.inverted = YES;

    CKKSNearFutureScheduler* scheduler = [[CKKSNearFutureScheduler alloc] initWithName: @"test" delay: 10*NSEC_PER_MSEC keepProcessAlive:false block:^{
        [first fulfill];
        [toofastexpectation fulfill];
    }];

    [scheduler waitUntil: 150*NSEC_PER_MSEC];
    [scheduler trigger];

    [self waitForExpectations: @[toofastexpectation] timeout:0.1];
    [self waitForExpectations: @[first] timeout:0.2];
}

- (void)testDelayedMultiShot {
    XCTestExpectation *first = [self expectationWithDescription:@"FutureScheduler fired (one)"];
    first.assertForOverFulfill = NO;

    XCTestExpectation *toofastexpectation = [self expectationWithDescription:@"FutureScheduler fired (too soon)"];
    toofastexpectation.expectedFulfillmentCount = 2;
    toofastexpectation.inverted = YES;

    XCTestExpectation *second = [self expectationWithDescription:@"FutureScheduler fired (two)"];
    second.expectedFulfillmentCount = 2;
    second.assertForOverFulfill = YES;

    CKKSNearFutureScheduler* scheduler = [[CKKSNearFutureScheduler alloc] initWithName: @"test" delay: 10*NSEC_PER_MSEC keepProcessAlive:false block:^{
        [first fulfill];
        [second fulfill];
        [toofastexpectation fulfill];
    }];

    [scheduler trigger];
    [self waitForExpectations: @[first] timeout:0.2];

    [scheduler waitUntil: 150*NSEC_PER_MSEC];
    [scheduler trigger];

    [self waitForExpectations: @[toofastexpectation] timeout:0.1];
    [self waitForExpectations: @[second] timeout:0.3];
}

@end

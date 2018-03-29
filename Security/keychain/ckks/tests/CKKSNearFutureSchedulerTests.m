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

#include <dispatch/dispatch.h>
#import <XCTest/XCTest.h>
#import "keychain/ckks/CKKSNearFutureScheduler.h"
#import "keychain/ckks/CKKSResultOperation.h"
#import "keychain/ckks/CKKS.h"

@interface CKKSNearFutureSchedulerTests : XCTestCase
@property NSOperationQueue* operationQueue;
@end

@implementation CKKSNearFutureSchedulerTests

- (void)setUp {
    [super setUp];

    self.operationQueue = [[NSOperationQueue alloc] init];
}

- (void)tearDown {
    [super tearDown];
}

#pragma mark - Block-based tests

- (void)testBlockOneShot {
    XCTestExpectation *expectation = [self expectationWithDescription:@"FutureScheduler fired"];

    CKKSNearFutureScheduler* scheduler = [[CKKSNearFutureScheduler alloc] initWithName: @"test" delay:50*NSEC_PER_MSEC keepProcessAlive:true
                                                             dependencyDescriptionCode:CKKSResultDescriptionNone
                                                                                 block:^{
        [expectation fulfill];
    }];

    [scheduler trigger];

    [self waitForExpectationsWithTimeout:1 handler:nil];
}

- (void)testBlockOneShotDelay {
    XCTestExpectation *toofastexpectation = [self expectationWithDescription:@"FutureScheduler fired (too soon)"];
    toofastexpectation.inverted = YES;

    XCTestExpectation *expectation = [self expectationWithDescription:@"FutureScheduler fired"];

    CKKSNearFutureScheduler* scheduler = [[CKKSNearFutureScheduler alloc] initWithName: @"test" delay: 200*NSEC_PER_MSEC keepProcessAlive:false
                                                             dependencyDescriptionCode:CKKSResultDescriptionNone
                                                                                 block:^{
        [toofastexpectation fulfill];
        [expectation fulfill];
    }];

    [scheduler trigger];

    // Make sure it waits at least 0.1 seconds
    [self waitForExpectations: @[toofastexpectation] timeout:0.1];

    // But finishes within 1.1s (total)
    [self waitForExpectations: @[expectation] timeout:1];
}

- (void)testBlockOneShotManyTrigger {
    XCTestExpectation *toofastexpectation = [self expectationWithDescription:@"FutureScheduler fired (too soon)"];
    toofastexpectation.inverted = YES;

    XCTestExpectation *expectation = [self expectationWithDescription:@"FutureScheduler fired"];
    expectation.assertForOverFulfill = YES;

    CKKSNearFutureScheduler* scheduler = [[CKKSNearFutureScheduler alloc] initWithName: @"test" delay: 200*NSEC_PER_MSEC keepProcessAlive:true
                                                             dependencyDescriptionCode:CKKSResultDescriptionNone
                                                                                 block:^{
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


- (void)testBlockMultiShot {
    XCTestExpectation *first = [self expectationWithDescription:@"FutureScheduler fired (one)"];
    first.assertForOverFulfill = NO;

    XCTestExpectation *second = [self expectationWithDescription:@"FutureScheduler fired (two)"];
    second.expectedFulfillmentCount = 2;
    second.assertForOverFulfill = YES;

    CKKSNearFutureScheduler* scheduler = [[CKKSNearFutureScheduler alloc] initWithName: @"test" delay: 100*NSEC_PER_MSEC keepProcessAlive:false
                                                             dependencyDescriptionCode:CKKSResultDescriptionNone
                                                                                 block:^{
        [first fulfill];
        [second fulfill];
    }];

    [scheduler trigger];

    [self waitForExpectations: @[first] timeout:0.4];

    [scheduler trigger];
    [scheduler trigger];
    [scheduler trigger];

    [self waitForExpectations: @[second] timeout:0.4];

    XCTestExpectation* waitmore = [self expectationWithDescription:@"waiting"];
    waitmore.inverted = YES;
    [self waitForExpectations: @[waitmore] timeout: 0.4];
}

- (void)testBlockMultiShotDelays {
    XCTestExpectation *first = [self expectationWithDescription:@"FutureScheduler fired (one)"];
    first.assertForOverFulfill = NO;

    XCTestExpectation *longdelay = [self expectationWithDescription:@"FutureScheduler fired (long delay expectation)"];
    longdelay.inverted = YES;
    longdelay.expectedFulfillmentCount = 2;

    XCTestExpectation *second = [self expectationWithDescription:@"FutureScheduler fired (two)"];
    second.expectedFulfillmentCount = 2;
    second.assertForOverFulfill = YES;

    CKKSNearFutureScheduler* scheduler = [[CKKSNearFutureScheduler alloc] initWithName: @"test" initialDelay: 50*NSEC_PER_MSEC continuingDelay:600*NSEC_PER_MSEC keepProcessAlive:false
                                                             dependencyDescriptionCode:CKKSResultDescriptionNone
                                                                                 block:^{
        [first fulfill];
        [longdelay fulfill];
        [second fulfill];
    }];

    [scheduler trigger];

    [self waitForExpectations: @[first] timeout:0.5];

    [scheduler trigger];
    [scheduler trigger];
    [scheduler trigger];

    // longdelay should NOT be fulfilled twice in the first 0.9 seconds
    [self waitForExpectations: @[longdelay] timeout:0.4];

    // But second should be fulfilled in the first 1.4 seconds
    [self waitForExpectations: @[second] timeout:0.5];

    XCTestExpectation* waitmore = [self expectationWithDescription:@"waiting"];
    waitmore.inverted = YES;
    [self waitForExpectations: @[waitmore] timeout: 0.2];
}

- (void)testBlockCancel {
    XCTestExpectation *cancelexpectation = [self expectationWithDescription:@"FutureScheduler fired (after cancel)"];
    cancelexpectation.inverted = YES;

    CKKSNearFutureScheduler* scheduler = [[CKKSNearFutureScheduler alloc] initWithName: @"test" delay: 100*NSEC_PER_MSEC keepProcessAlive:true
                                                             dependencyDescriptionCode:CKKSResultDescriptionNone
                                                                                 block:^{
        [cancelexpectation fulfill];
    }];

    [scheduler trigger];
    [scheduler cancel];

    // Make sure it does not fire in 0.5 s
    [self waitForExpectations: @[cancelexpectation] timeout:0.2];
}

- (void)testBlockDelayedNoShot {
    XCTestExpectation *toofastexpectation = [self expectationWithDescription:@"FutureScheduler fired (too soon)"];
    toofastexpectation.inverted = YES;

    CKKSNearFutureScheduler* scheduler = [[CKKSNearFutureScheduler alloc] initWithName: @"test" delay: 10*NSEC_PER_MSEC keepProcessAlive:false
                                                             dependencyDescriptionCode:CKKSResultDescriptionNone
                                                                                 block:^{
        [toofastexpectation fulfill];
    }];

    // Tell the scheduler to wait, but don't trigger it. It shouldn't fire.
    [scheduler waitUntil: 50*NSEC_PER_MSEC];

    [self waitForExpectations: @[toofastexpectation] timeout:0.1];
}

- (void)testBlockDelayedOneShot {
    XCTestExpectation *first = [self expectationWithDescription:@"FutureScheduler fired (one)"];
    first.assertForOverFulfill = NO;

    XCTestExpectation *toofastexpectation = [self expectationWithDescription:@"FutureScheduler fired (too soon)"];
    toofastexpectation.inverted = YES;

    CKKSNearFutureScheduler* scheduler = [[CKKSNearFutureScheduler alloc] initWithName: @"test" delay: 10*NSEC_PER_MSEC keepProcessAlive:false
                                                             dependencyDescriptionCode:CKKSResultDescriptionNone
                                                                                 block:^{
        [first fulfill];
        [toofastexpectation fulfill];
    }];

    [scheduler waitUntil: 150*NSEC_PER_MSEC];
    [scheduler trigger];

    [self waitForExpectations: @[toofastexpectation] timeout:0.1];
    [self waitForExpectations: @[first] timeout:0.5];
}

- (void)testBlockWaitedMultiShot {
    XCTestExpectation *first = [self expectationWithDescription:@"FutureScheduler fired (one)"];
    first.assertForOverFulfill = NO;

    XCTestExpectation *toofastexpectation = [self expectationWithDescription:@"FutureScheduler fired (too soon)"];
    toofastexpectation.expectedFulfillmentCount = 2;
    toofastexpectation.inverted = YES;

    XCTestExpectation *second = [self expectationWithDescription:@"FutureScheduler fired (two)"];
    second.expectedFulfillmentCount = 2;
    second.assertForOverFulfill = YES;

    CKKSNearFutureScheduler* scheduler = [[CKKSNearFutureScheduler alloc] initWithName: @"test" delay: 10*NSEC_PER_MSEC keepProcessAlive:false
                                                             dependencyDescriptionCode:CKKSResultDescriptionNone
                                                                                 block:^{
        [first fulfill];
        [second fulfill];
        [toofastexpectation fulfill];
    }];

    [scheduler trigger];
    [self waitForExpectations: @[first] timeout:0.5];

    [scheduler waitUntil: 150*NSEC_PER_MSEC];
    [scheduler trigger];

    [self waitForExpectations: @[toofastexpectation] timeout:0.1];
    [self waitForExpectations: @[second] timeout:0.3];
}

#pragma mark - Operation-based tests

- (NSOperation*)operationFulfillingExpectations:(NSArray<XCTestExpectation*>*)expectations {
    return [NSBlockOperation named:@"test" withBlock:^{
        for(XCTestExpectation* e in expectations) {
            [e fulfill];
        }
    }];
}

- (void)addOperationFulfillingExpectations:(NSArray<XCTestExpectation*>*)expectations scheduler:(CKKSNearFutureScheduler*)scheduler {
    NSOperation* op = [self operationFulfillingExpectations:expectations];
    XCTAssertNotNil(scheduler.operationDependency, "Should be an operation dependency");
    XCTAssertTrue([scheduler.operationDependency isPending], "operation dependency shouldn't have run yet");
    [op addDependency:scheduler.operationDependency];
    [self.operationQueue addOperation:op];
}

- (void)testOperationOneShot {
    XCTestExpectation *expectation = [self expectationWithDescription:@"FutureScheduler fired"];

    CKKSNearFutureScheduler* scheduler = [[CKKSNearFutureScheduler alloc] initWithName: @"test" delay:50*NSEC_PER_MSEC keepProcessAlive:true
                                                             dependencyDescriptionCode:CKKSResultDescriptionNone
                                                                                 block:^{}];
    [self addOperationFulfillingExpectations:@[expectation] scheduler:scheduler];

    [scheduler trigger];

    [self waitForExpectationsWithTimeout:1 handler:nil];
}

- (void)testOperationOneShotDelay {
    XCTestExpectation *toofastexpectation = [self expectationWithDescription:@"FutureScheduler fired (too soon)"];
    toofastexpectation.inverted = YES;

    XCTestExpectation *expectation = [self expectationWithDescription:@"FutureScheduler fired"];

    CKKSNearFutureScheduler* scheduler = [[CKKSNearFutureScheduler alloc] initWithName: @"test" delay: 200*NSEC_PER_MSEC keepProcessAlive:false
                                                             dependencyDescriptionCode:CKKSResultDescriptionNone
                                                                                 block:^{}];
    [self addOperationFulfillingExpectations:@[expectation,toofastexpectation] scheduler:scheduler];

    [scheduler trigger];

    // Make sure it waits at least 0.1 seconds
    [self waitForExpectations: @[toofastexpectation] timeout:0.1];

    // But finishes within 1.1s (total)
    [self waitForExpectations: @[expectation] timeout:1];
}

- (void)testOperationOneShotManyTrigger {
    XCTestExpectation *toofastexpectation = [self expectationWithDescription:@"FutureScheduler fired (too soon)"];
    toofastexpectation.inverted = YES;

    XCTestExpectation *expectation = [self expectationWithDescription:@"FutureScheduler fired"];
    expectation.assertForOverFulfill = YES;

    CKKSNearFutureScheduler* scheduler = [[CKKSNearFutureScheduler alloc] initWithName: @"test" delay: 200*NSEC_PER_MSEC keepProcessAlive:true
                                                             dependencyDescriptionCode:CKKSResultDescriptionNone
                                                                                 block:^{}];
    [self addOperationFulfillingExpectations:@[expectation,toofastexpectation] scheduler:scheduler];

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


- (void)testOperationMultiShot {
    XCTestExpectation *first = [self expectationWithDescription:@"FutureScheduler fired (one)"];

    XCTestExpectation *second = [self expectationWithDescription:@"FutureScheduler fired (two)"];

    CKKSNearFutureScheduler* scheduler = [[CKKSNearFutureScheduler alloc] initWithName: @"test" delay: 100*NSEC_PER_MSEC keepProcessAlive:false
                                                             dependencyDescriptionCode:CKKSResultDescriptionNone
                                                                                 block:^{}];

    [self addOperationFulfillingExpectations:@[first] scheduler:scheduler];

    [scheduler trigger];

    [self waitForExpectations: @[first] timeout:0.2];

    [self addOperationFulfillingExpectations:@[second] scheduler:scheduler];

    [scheduler trigger];
    [scheduler trigger];
    [scheduler trigger];

    [self waitForExpectations: @[second] timeout:0.2];

    XCTestExpectation* waitmore = [self expectationWithDescription:@"waiting"];
    waitmore.inverted = YES;
    [self waitForExpectations: @[waitmore] timeout: 0.2];
}

- (void)testOperationMultiShotDelays {
    XCTestExpectation *first = [self expectationWithDescription:@"FutureScheduler fired (one)"];

    XCTestExpectation *longdelay = [self expectationWithDescription:@"FutureScheduler fired (long delay expectation)"];
    longdelay.inverted = YES;
    XCTestExpectation *second = [self expectationWithDescription:@"FutureScheduler fired (two)"];

    CKKSNearFutureScheduler* scheduler = [[CKKSNearFutureScheduler alloc] initWithName: @"test" initialDelay: 50*NSEC_PER_MSEC continuingDelay:300*NSEC_PER_MSEC keepProcessAlive:false
                                                             dependencyDescriptionCode:CKKSResultDescriptionNone
                                                                                 block:^{}];

    [self addOperationFulfillingExpectations:@[first] scheduler:scheduler];

    [scheduler trigger];

    [self waitForExpectations: @[first] timeout:0.2];

    [self addOperationFulfillingExpectations:@[second,longdelay] scheduler:scheduler];

    [scheduler trigger];
    [scheduler trigger];
    [scheduler trigger];

    // longdelay shouldn't be fulfilled in the first 0.2 seconds
    [self waitForExpectations: @[longdelay] timeout:0.2];

    // But second should be fulfilled in the next 0.5 seconds
    [self waitForExpectations: @[second] timeout:0.5];

    XCTestExpectation* waitmore = [self expectationWithDescription:@"waiting"];
    waitmore.inverted = YES;
    [self waitForExpectations: @[waitmore] timeout: 0.2];
}

- (void)testOperationCancel {
    XCTestExpectation *cancelexpectation = [self expectationWithDescription:@"FutureScheduler fired (after cancel)"];
    cancelexpectation.inverted = YES;

    CKKSNearFutureScheduler* scheduler = [[CKKSNearFutureScheduler alloc] initWithName: @"test" delay: 100*NSEC_PER_MSEC keepProcessAlive:true
                                                             dependencyDescriptionCode:CKKSResultDescriptionNone
                                                                                 block:^{}];

    [self addOperationFulfillingExpectations:@[cancelexpectation] scheduler:scheduler];

    [scheduler trigger];
    [scheduler cancel];

    // Make sure it does not fire in 0.5 s
    [self waitForExpectations: @[cancelexpectation] timeout:0.2];
}

- (void)testOperationDelayedNoShot {
    XCTestExpectation *toofastexpectation = [self expectationWithDescription:@"FutureScheduler fired (too soon)"];
    toofastexpectation.inverted = YES;

    CKKSNearFutureScheduler* scheduler = [[CKKSNearFutureScheduler alloc] initWithName: @"test" delay: 10*NSEC_PER_MSEC keepProcessAlive:false
                                                             dependencyDescriptionCode:CKKSResultDescriptionNone
                                                                                 block:^{}];
    [self addOperationFulfillingExpectations:@[toofastexpectation] scheduler:scheduler];

    // Tell the scheduler to wait, but don't trigger it. It shouldn't fire.
    [scheduler waitUntil: 50*NSEC_PER_MSEC];

    [self waitForExpectations: @[toofastexpectation] timeout:0.1];
}

- (void)testOperationDelayedOneShot {
    XCTestExpectation *first = [self expectationWithDescription:@"FutureScheduler fired (one)"];
    first.assertForOverFulfill = NO;

    XCTestExpectation *toofastexpectation = [self expectationWithDescription:@"FutureScheduler fired (too soon)"];
    toofastexpectation.inverted = YES;

    CKKSNearFutureScheduler* scheduler = [[CKKSNearFutureScheduler alloc] initWithName: @"test" delay: 10*NSEC_PER_MSEC keepProcessAlive:false
                                                             dependencyDescriptionCode:CKKSResultDescriptionNone
                                                                                 block:^{}];
    [self addOperationFulfillingExpectations:@[first,toofastexpectation] scheduler:scheduler];

    [scheduler waitUntil: 150*NSEC_PER_MSEC];
    [scheduler trigger];

    [self waitForExpectations: @[toofastexpectation] timeout:0.1];
    [self waitForExpectations: @[first] timeout:0.5];
}

@end

#endif /* OCTAGON */

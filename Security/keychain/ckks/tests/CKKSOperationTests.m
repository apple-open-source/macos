/*
 * Copyright (c) 2016 Apple Inc. All Rights Reserved.
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

#import "keychain/ckks/CKKSGroupOperation.h"
#import "keychain/ckks/CKKSCondition.h"

// Helper Operations
@interface CKKSResultCancelOperation : CKKSResultOperation
- (instancetype) init;
@end

@implementation CKKSResultCancelOperation
- (instancetype)init {
    if(self = [super init]) {
        __weak __typeof(self) weakSelf = self;
        [self addExecutionBlock:^{
            [weakSelf cancel];
        }];
    }
    return self;
}
@end



@interface CKKSResultErrorOperation : CKKSResultOperation
- (instancetype) init;
@end

@implementation CKKSResultErrorOperation
- (instancetype)init {
    if(self = [super init]) {
        __weak __typeof(self) weakSelf = self;
        [self addExecutionBlock:^{
            weakSelf.error = [NSError errorWithDomain:@"test domain" code:5 userInfo:nil];
        }];
    }
    return self;
}
@end


@interface CKKSOperationTests : XCTestCase
@property NSOperationQueue* queue;
@end

// Remaining tests to write:
// TODO: subclass of CKKSResultOperation implementing main() respects addSuccessDependency without any special code
// TODO: chain of automatic dependencies
// TODO: test showing that CKKSGroupOperations don't start if they success-depend on a failed CKKSResultOperation

@implementation CKKSOperationTests

- (void)setUp {
    [super setUp];

    self.queue = [[NSOperationQueue alloc] init];
}

- (void)tearDown {
    [self.queue cancelAllOperations];
    self.queue = nil;

    [super tearDown];
}

- (void)testIsPending {
    NSBlockOperation* run     = [NSBlockOperation blockOperationWithBlock:^{}];
    NSBlockOperation* cancel  = [NSBlockOperation blockOperationWithBlock:^{}];
    NSBlockOperation* pending = [NSBlockOperation blockOperationWithBlock:^{}];

    [self.queue addOperation: run];
    [cancel cancel];
    [self.queue waitUntilAllOperationsAreFinished];

    XCTAssertTrue( [pending isPending], @"Pending operation should be pending");
    XCTAssertFalse([run isPending],     @"run operation should not be pending");
    XCTAssertFalse([cancel isPending],  @"Cancelled operation should not be pending");
}

- (void)testResultOperation {
    CKKSResultOperation* op = [[CKKSResultOperation alloc] init];
    __weak __typeof(op) weakOp = op;

    [op addExecutionBlock:^{
        weakOp.error = [NSError errorWithDomain:@"test domain" code:0 userInfo:nil];
    }];

    [self.queue addOperation: op];

    [op waitUntilFinished];

    XCTAssertNotNil(op.error, "errors can persist");
}

- (void)testResultSuccessDependency {
    __block bool firstRun = false;
    __block bool secondRun = false;

    CKKSResultOperation* first = [[CKKSResultOperation alloc] init];
    [first addExecutionBlock:^{
        firstRun = true;
    }];

    CKKSResultOperation* second = [[CKKSResultOperation alloc] init];
    [second addExecutionBlock:^{
        XCTAssertTrue(firstRun);
        secondRun = true;
    }];
    [second addSuccessDependency: first];

    [self.queue addOperation: second];
    [self.queue addOperation: first];

    [self.queue waitUntilAllOperationsAreFinished];

    XCTAssertTrue(firstRun);
    XCTAssertTrue(secondRun);

    XCTAssertTrue(first.finished,    "First operation finished");
    XCTAssertFalse(first.cancelled,  "First operation not cancelled");
    XCTAssertTrue(second.finished,   "Second operation finished");
    XCTAssertFalse(second.cancelled, "Second operation not cancelled");
}

- (void)testResultSuccessDependencyCancel {
    CKKSResultCancelOperation* first = [[CKKSResultCancelOperation alloc] init];

    CKKSResultOperation* second = [[CKKSResultOperation alloc] init];
    [second addExecutionBlock:^{
        XCTFail("Second operation should never run");
    }];
    [second addSuccessDependency: first];

    [self.queue addOperation: second];
    [self.queue addOperation: first];

    [self.queue waitUntilAllOperationsAreFinished];

    XCTAssertTrue(first.finished,  "First operation finished");
    XCTAssertTrue(first.cancelled, "First operation is canceled (as requested)");

    XCTAssertTrue(second.cancelled, "Second operation is canceled");
    XCTAssertTrue(second.finished,  "Second operation finished");
    XCTAssertNotNil(second.error, "Error is generated when CKKSResultOperation is cancelled");
    XCTAssertEqual(second.error.code, CKKSResultSubresultCancelled, "Error code is CKKSResultSubresultCancelled");
}

- (void)testResultSuccessDependencyError {
    CKKSResultErrorOperation* first = [[CKKSResultErrorOperation alloc] init];

    CKKSResultOperation* second = [[CKKSResultOperation alloc] init];
    [second addExecutionBlock:^{
        XCTFail("Second operation should never run");
    }];
    [second addSuccessDependency: first];

    [self.queue addOperation: second];
    [self.queue addOperation: first];

    [self.queue waitUntilAllOperationsAreFinished];

    XCTAssertTrue(first.finished,   "First operation finished");
    XCTAssertFalse(first.cancelled, "First operation is not canceled");
    XCTAssertNotNil(first.error,    "First operation has an error");

    XCTAssertTrue(second.cancelled, "Second operation is canceled");
    XCTAssertTrue(second.finished,  "Second operation finished");
    XCTAssertNotNil(second.error, "Error is generated when dependent CKKSResultOperation has an error");
    XCTAssertEqual(second.error.code, CKKSResultSubresultError, "Error code is CKKSResultSubresultError");

    XCTAssertNotNil(second.error.userInfo[NSUnderlyingErrorKey], "Passed up the error from the first operation");
    XCTAssertEqual([second.error.userInfo[NSUnderlyingErrorKey] code], 5, "Passed up the right error from the first operation");
}

- (void)testResultTimeout {
    __block bool firstRun = false;
    __block bool secondRun = false;

    CKKSResultOperation* first = [[CKKSResultOperation alloc] init];
    [first addExecutionBlock:^{
        firstRun = true;
    }];

    CKKSResultOperation* second = [[CKKSResultOperation alloc] init];
    [second addExecutionBlock:^{
        XCTAssertTrue(firstRun);
        secondRun = true;
    }];
    [second addDependency: first];

    [self.queue addOperation: [second timeout:(50)* NSEC_PER_MSEC]];
    [self.queue waitUntilAllOperationsAreFinished];

    XCTAssertFalse(firstRun);
    XCTAssertFalse(secondRun);

    XCTAssertFalse(first.finished,   "First operation not finished");
    XCTAssertFalse(first.cancelled, "First operation not cancelled");
    XCTAssertTrue(second.finished,  "Second operation finished");
    XCTAssertTrue(second.cancelled, "Second operation cancelled");
    XCTAssertNotNil(second.error,   "Second operation has an error");
    XCTAssertEqual(second.error.code, CKKSResultTimedOut, "Second operation error is good");
    NSError* underlying = second.error.userInfo[NSUnderlyingErrorKey];
    XCTAssertNil(underlying, "Second operation's error doesn't have an underlying explanation");
}

- (void)testResultTimeoutWithUnderlyingError {
    __block bool firstRun = false;
    __block bool secondRun = false;

    CKKSResultOperation* first = [[CKKSResultOperation alloc] init];
    [first addExecutionBlock:^{
        firstRun = true;
    }];
    first.descriptionErrorCode = 604;

    CKKSResultOperation* second = [[CKKSResultOperation alloc] init];
    [second addExecutionBlock:^{
        XCTAssertTrue(firstRun);
        secondRun = true;
    }];
    [second addDependency: first];

    [self.queue addOperation: [second timeout:(50)* NSEC_PER_MSEC]];
    [self.queue waitUntilAllOperationsAreFinished];

    XCTAssertFalse(firstRun);
    XCTAssertFalse(secondRun);

    XCTAssertFalse(first.finished,   "First operation not finished");
    XCTAssertFalse(first.cancelled, "First operation not cancelled");
    XCTAssertTrue(second.finished,  "Second operation finished");
    XCTAssertTrue(second.cancelled, "Second operation cancelled");
    XCTAssertNotNil(second.error,   "Second operation has an error");
    XCTAssertEqual(second.error.code, CKKSResultTimedOut, "Second operation error is good");
    NSError* underlying = second.error.userInfo[NSUnderlyingErrorKey];
    XCTAssertNotNil(underlying, "second operation's error has an underlying reason");
    XCTAssertEqualObjects(underlying.domain, CKKSResultDescriptionErrorDomain, "second operation's underlying error's domain should be CKKSResultDescriptionErrorDomain");
    XCTAssertEqual(underlying.code, 604, "second operation's underlying error's domain should be first's description");
}


- (void)testResultNoTimeout {
    __block bool firstRun = false;
    __block bool secondRun = false;

    CKKSResultOperation* first = [[CKKSResultOperation alloc] init];
    [first addExecutionBlock:^{
        firstRun = true;
    }];

    CKKSResultOperation* second = [[CKKSResultOperation alloc] init];
    [second addExecutionBlock:^{
        XCTAssertTrue(firstRun);
        secondRun = true;
    }];
    [second addDependency: first];

    [self.queue addOperation: [second timeout:(100)* NSEC_PER_MSEC]];
    [self.queue addOperation: first];
    [self.queue waitUntilAllOperationsAreFinished];

    XCTAssertTrue(firstRun);
    XCTAssertTrue(secondRun);

    XCTAssertTrue(first.finished,   "First operation finished");
    XCTAssertFalse(first.cancelled, "First operation not cancelled");
    XCTAssertTrue(second.finished,  "Second operation finished");
    XCTAssertFalse(second.cancelled, "Second operation not cancelled");
    XCTAssertNil(second.error,      "Second operation has no error");
}

- (void)testResultFinishDate
{
    CKKSResultOperation* operation = [[CKKSResultOperation alloc] init];
    XCTAssertNil(operation.finishDate, "Result operation does not have a finish date before it is run");

    [operation addExecutionBlock:^{}];

    [self.queue addOperation:operation];
    [self.queue waitUntilAllOperationsAreFinished];
    sleep(0.1); // wait for the completion block to have time to fire
    XCTAssertNotNil(operation.finishDate, "Result operation has a finish date after everything is done");
    NSTimeInterval timeIntervalSinceFinishDate = [[NSDate date] timeIntervalSinceDate:operation.finishDate];
    XCTAssertTrue(timeIntervalSinceFinishDate >= 0.0 && timeIntervalSinceFinishDate <= 10.0, "Result operation finish datelooks reasonable");
}

- (void)testGroupOperation {
    CKKSGroupOperation* group = [[CKKSGroupOperation alloc] init];

    CKKSResultOperation* op1 = [[CKKSResultOperation alloc] init];
    [group runBeforeGroupFinished: op1];

    CKKSResultOperation* op2 = [[CKKSResultOperation alloc] init];
    [group runBeforeGroupFinished: op2];

    [self.queue addOperation: group];

    [self.queue waitUntilAllOperationsAreFinished];

    XCTAssertEqual(op1.finished,   YES, "First operation finished");
    XCTAssertEqual(op2.finished,   YES, "Second operation finished");
    XCTAssertEqual(group.finished, YES, "Group operation finished");

    XCTAssertEqual(op1.cancelled,   NO, "First operation not cancelled");
    XCTAssertEqual(op2.cancelled,   NO, "Second operation cancelled");
    XCTAssertEqual(group.cancelled, NO, "Group operation not cancelled");

    XCTAssertNil(op1.error, "First operation: no error");
    XCTAssertNil(op2.error, "Second operation: no error");
    XCTAssertNil(group.error, "Group operation: no error");
}

- (void)testGroupOperationRunBlock {
    XCTestExpectation* operationRun = [self expectationWithDescription:@"operation run with named:withBlock:"];
    CKKSGroupOperation* group = [CKKSGroupOperation named:@"asdf" withBlock: ^{
        [operationRun fulfill];
    }];
    [self.queue addOperation:group];
    [self waitForExpectations: @[operationRun] timeout:5];

    operationRun = [self expectationWithDescription:@"operation run with named:withBlockTakingSelf:"];
    group = [CKKSGroupOperation named:@"asdf" withBlockTakingSelf:^(CKKSGroupOperation *strongOp) {
        [operationRun fulfill];
    }];
    [self.queue addOperation:group];
    [self waitForExpectations: @[operationRun] timeout:5];
}

- (void)testGroupOperationSubOperationCancel {
    CKKSGroupOperation* group = [[CKKSGroupOperation alloc] init];

    CKKSResultOperation* op1 = [[CKKSResultOperation alloc] init];
    [group runBeforeGroupFinished: op1];

    CKKSResultCancelOperation* op2 = [[CKKSResultCancelOperation alloc] init];
    [group runBeforeGroupFinished: op2];

    [self.queue addOperation: group];

    [self.queue waitUntilAllOperationsAreFinished];

    XCTAssertEqual(op1.finished,   YES, "First operation finished");
    XCTAssertEqual(op2.finished,   YES, "Second operation finished");
    XCTAssertEqual(group.finished, YES, "Group operation finished");

    XCTAssertEqual(op1.cancelled,   NO, "First operation not cancelled");
    XCTAssertEqual(op2.cancelled,   YES, "Second operation not cancelled");
    XCTAssertEqual(group.cancelled, NO, "Group operation not cancelled");

    XCTAssertNil(op1.error, "First operation: no error");
    XCTAssertNil(op2.error, "Second operation: no error");
    XCTAssertNotNil(group.error, "Group operation: has an error");
    XCTAssertEqual(group.error.code, CKKSResultSubresultCancelled, "Error code is CKKSResultSubresultCancelled");
}

- (void)testGroupOperationCancel {
    CKKSGroupOperation* group = [[CKKSGroupOperation alloc] init];

    CKKSResultOperation* op1 = [[CKKSResultOperation alloc] init];
    [group runBeforeGroupFinished: op1];

    [group cancel];
    [self.queue addOperation: group];

    [self.queue waitUntilAllOperationsAreFinished];

    XCTAssertEqual(op1.finished,   YES, "First operation finished");
    XCTAssertEqual(group.finished, YES, "Group operation finished");

    XCTAssertEqual(op1.cancelled,   YES, "First operation cancelled");
    XCTAssertEqual(group.cancelled, YES, "Group operation cancelled");

    XCTAssertNil(op1.error, "First operation: no error");
    XCTAssertNil(group.error, "Group operation: no error");
}

- (void)testGroupOperationCancelAfterAdd {
    CKKSGroupOperation* group = [[CKKSGroupOperation alloc] init];

    CKKSResultOperation* op1 = [[CKKSResultOperation alloc] init];
    [group runBeforeGroupFinished: op1];

    CKKSResultOperation* never = [[CKKSResultOperation alloc] init];
    [group addDependency: never];

    [self.queue addOperation: group];

    [group cancel];

    // Both of these should finish. Wait for that.
    [op1 waitUntilFinished];
    [group waitUntilFinished];

    XCTAssertEqual(op1.finished,   YES, "First operation finished");
    XCTAssertEqual(group.finished, YES, "Group operation finished");

    XCTAssertEqual(op1.cancelled,   YES, "First operation cancelled");
    XCTAssertEqual(group.cancelled, YES, "Group operation cancelled");

    XCTAssertNil(op1.error, "First operation: no error");
    XCTAssertNil(group.error, "Group operation: no error");
}

- (void)testGroupOperationCancelWhileRunning {
    CKKSGroupOperation* group = [[CKKSGroupOperation alloc] init];
    group.name = @"operation-under-test";

    XCTestExpectation* groupStarted = [self expectationWithDescription: @"group started"];
    XCTestExpectation* cancelOccurs = [self expectationWithDescription: @"cancel occurs"];

    CKKSCondition* everythingFinished = [[CKKSCondition alloc] init];
    CKKSResultOperation* op1 = [CKKSResultOperation named:@"op1" withBlock:^{
        [groupStarted fulfill];

        [self waitForExpectations:@[cancelOccurs] timeout:8.0];

        // 'do some work'. Will wait 200msec.
        [everythingFinished wait:200*NSEC_PER_MSEC];
    }];
    [group runBeforeGroupFinished: op1];
    [self.queue addOperation: group];

    [self waitForExpectations:@[groupStarted] timeout:8.0];
    [group cancel];
    [cancelOccurs fulfill];

    [group waitUntilFinished];

    XCTAssertEqual(op1.finished,   YES, "First operation finished");
    XCTAssertEqual(group.finished, YES, "Group operation finished");

    [everythingFinished fulfill];

    XCTAssertEqual(op1.cancelled,   YES, "First operation cancelled");
    XCTAssertEqual(group.cancelled, YES, "Group operation cancelled");

    XCTAssertNil(op1.error, "First operation: no error");
    XCTAssertNotNil(group.error, "Group operation: has an error");
    XCTAssertEqual(group.error.code, CKKSResultSubresultCancelled, "Error code is CKKSResultSubresultCancelled");
}

- (void)testGroupOperationWithDependsOn {
    CKKSGroupOperation* group = [[CKKSGroupOperation alloc] init];

    CKKSResultOperation* op1 = [[CKKSResultOperation alloc] init];
    [group dependOnBeforeGroupFinished:op1];

    [group cancel];
    [self.queue addOperation: group];

    [self.queue waitUntilAllOperationsAreFinished];

    XCTAssertEqual(op1.finished,   NO, "First operation not finished");
    XCTAssertEqual(group.finished, YES, "Group operation finished");

    XCTAssertEqual(op1.cancelled,   NO, "First operation not cancelled");
    XCTAssertEqual(group.cancelled, YES, "Group operation cancelled");

    XCTAssertNil(op1.error, "First operation: no error");
    XCTAssertNil(group.error, "Group operation: no error");
}

- (void)testGroupOperationTimeout {
    CKKSGroupOperation* group = [[CKKSGroupOperation alloc] init];

    __block bool run1 = false;
    CKKSResultOperation* op1 = [CKKSResultOperation operationWithBlock: ^{
        run1 = true;
    }];
    [group runBeforeGroupFinished: op1];

    __block bool run2 = false;
    CKKSResultOperation* op2 = [CKKSResultOperation operationWithBlock: ^{
        run2 = true;
    }];
    [group runBeforeGroupFinished: op2];

    CKKSResultOperation* never = [[CKKSResultOperation alloc] init];
    [group addDependency: never];

    [group timeout:10*NSEC_PER_MSEC];
    [self.queue addOperation: group];

    [self.queue waitUntilAllOperationsAreFinished];

    // Shouldn't be necessary, but I'm not sure the NSOperation's finished property vs. dependency triggering is thread-safe
    [op1 waitUntilFinished];
    [op2 waitUntilFinished];

    XCTAssertEqual(op1.finished,   YES, "First operation finished");
    XCTAssertEqual(op2.finished,   YES, "Second operation finished");
    XCTAssertEqual(group.finished, YES, "Group operation finished");

    XCTAssertEqual(op1.cancelled,   YES, "First operation cancelled");
    XCTAssertEqual(op2.cancelled,   YES, "Second operation cancelled");
    XCTAssertEqual(group.cancelled, YES, "Group operation cancelled");

    XCTAssertFalse(run1, "First operation did not run");
    XCTAssertFalse(run2, "Second operation did not run");

    XCTAssertNil(op1.error, "First operation: no error");
    XCTAssertNil(op2.error, "Second operation: no error");
    XCTAssertNotNil(group.error, "Group operation: error");
    XCTAssertEqual(group.error.code, CKKSResultTimedOut, "Error code is CKKSResultTimedOut");

    // Try a few more times, just in case
    for(int i = 0; i < 100; i++) {
        CKKSGroupOperation* g = [[CKKSGroupOperation alloc] init];
        [g addDependency: never];
        [g timeout:((i%20)+1)*NSEC_PER_MSEC];

        [self.queue addOperation: g];
        [self.queue waitUntilAllOperationsAreFinished];
    }
}

- (void)testGroupOperationError {
    CKKSGroupOperation* group = [[CKKSGroupOperation alloc] init];

    CKKSResultOperation* op1 = [[CKKSResultOperation alloc] init];
    [group runBeforeGroupFinished: op1];

    CKKSResultErrorOperation* op2 = [[CKKSResultErrorOperation alloc] init];
    [group runBeforeGroupFinished: op2];

    [self.queue addOperation: group];

    [self.queue waitUntilAllOperationsAreFinished];

    XCTAssertEqual(op1.finished,   YES, "First operation finished");
    XCTAssertEqual(op2.finished,   YES, "Second operation finished");
    XCTAssertEqual(group.finished, YES, "Group operation finished");

    XCTAssertEqual(op1.cancelled,   NO, "First operation not cancelled");
    XCTAssertEqual(op2.cancelled,   NO, "Second operation cancelled");
    XCTAssertEqual(group.cancelled, NO, "Group operation not cancelled");

    XCTAssertNil(op1.error, "First operation: no error");
    XCTAssertNotNil(op2.error, "Second operation: error (as expected)");
    XCTAssertEqual(op2.error.code, 5, "Rght error from the erroring operation");

    XCTAssertNotNil(group.error, "Error is generated when dependent CKKSResultOperation has an error");
    XCTAssertEqual(group.error.code, CKKSResultSubresultError, "Error code is CKKSResultSubresultError");
    XCTAssertNotNil(group.error.userInfo[NSUnderlyingErrorKey], "Passed up the error from the first operation");
    XCTAssertEqual([group.error.userInfo[NSUnderlyingErrorKey] code], 5, "Passed up the right error from the first operation");
}

- (void)testGroupOperationPending {
    CKKSGroupOperation* group = [[CKKSGroupOperation alloc] init];

    CKKSResultOperation* op1 = [[CKKSResultOperation alloc] init];
    [group runBeforeGroupFinished: op1];

    CKKSResultOperation* op2 = [[CKKSResultOperation alloc] init];
    [group addDependency: op2];

    [self.queue addOperation: group];

    XCTAssertTrue([group isPending], "group operation hasn't started yet");

    [self.queue addOperation: op2];
    [self.queue waitUntilAllOperationsAreFinished];
    XCTAssertFalse([group isPending], "group operation has started");

    XCTAssertEqual(op1.finished,   YES, "First operation finished");
    XCTAssertEqual(op2.finished,   YES, "Second operation finished");
    XCTAssertEqual(group.finished, YES, "Group operation finished");

    XCTAssertEqual(op1.cancelled,   NO, "First operation not cancelled");
    XCTAssertEqual(op2.cancelled,   NO, "Second operation cancelled");
    XCTAssertEqual(group.cancelled, NO, "Group operation not cancelled");

    XCTAssertNil(op1.error, "First operation: no error");
    XCTAssertNil(op2.error, "Second operation: no error");
    XCTAssertNil(group.error, "Group operation: no error");
}

- (void)testGroupOperationPendingAfterCancel {
    CKKSGroupOperation* group = [[CKKSGroupOperation alloc] init];

    CKKSResultOperation* op1 = [[CKKSResultOperation alloc] init];
    [group runBeforeGroupFinished: op1];

    CKKSResultOperation* op2 = [[CKKSResultOperation alloc] init];
    [group addDependency: op2];

    [group cancel];

    XCTAssertFalse([group isPending], "group operation isn't pending, as it's cancelled");
}


@end

#endif /* OCTAGON */

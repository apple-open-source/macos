#if OCTAGON

#import <XCTest/XCTest.h>
#import <OCMock/OCMock.h>

#import "keychain/ckks/CKKSLockStateTracker.h"
#import "keychain/ckks/tests/CKKSMockLockStateProvider.h"
#import "tests/secdmockaks/mockaks.h"

@interface CKKSTests_LockStateTracker : XCTestCase

@property CKKSMockLockStateProvider* lockStateProvider;
@property CKKSLockStateTracker* lockStateTracker;
@end

@implementation CKKSTests_LockStateTracker

- (void)setUp {
    [super setUp];

    self.lockStateProvider = [[CKKSMockLockStateProvider alloc] initWithCurrentLockStatus:NO];
    self.lockStateTracker = [[CKKSLockStateTracker alloc] initWithProvider:self.lockStateProvider];

    [SecMockAKS reset];
}

- (void)tearDown {
    self.lockStateProvider = nil;
    self.lockStateTracker = nil;
}

- (void)testLockedBehindOurBack {

    /*
     * check that we detect that lock errors force a recheck
     */

    NSError *lockError = [NSError errorWithDomain:NSOSStatusErrorDomain code:errSecInteractionNotAllowed userInfo:nil];
    NSError *fileError = [NSError errorWithDomain:NSOSStatusErrorDomain code:ENOENT userInfo:nil];

    XCTAssertFalse([self.lockStateTracker isLocked], "should start out unlocked");
    XCTAssertTrue([self.lockStateTracker isLockedError:lockError], "errSecInteractionNotAllowed is a lock errors");
    XCTAssertFalse([self.lockStateTracker isLocked], "should be unlocked after lock failure");

    XCTAssertFalse([self.lockStateTracker isLockedError:fileError], "file errors are not lock errors");
    XCTAssertFalse([self.lockStateTracker isLocked], "should be unlocked after lock failure");

    self.lockStateProvider.aksCurrentlyLocked = true;
    XCTAssertFalse([self.lockStateTracker isLocked], "should be reporting unlocked since we 'missed' the notification");

    XCTAssertFalse([self.lockStateTracker isLockedError:fileError], "file errors are not lock errors");
    XCTAssertFalse([self.lockStateTracker isLocked], "should be 'unlocked' after file errors");

    XCTAssertTrue([self.lockStateTracker isLockedError:lockError], "errSecInteractionNotAllowed is a lock errors");
    XCTAssertTrue([self.lockStateTracker isLocked], "should be locked after lock failure");

    self.lockStateProvider.aksCurrentlyLocked = false;
    [self.lockStateTracker recheck];

    XCTAssertFalse([self.lockStateTracker isLocked], "should be unlocked");
}

- (void)testWaitForUnlock {

    self.lockStateProvider.aksCurrentlyLocked = true;
    [self.lockStateTracker recheck];

    XCTestExpectation* expectation = [self expectationWithDescription: @"unlock occurs"];

    NSBlockOperation *unlockEvent = [NSBlockOperation blockOperationWithBlock:^{
        [expectation fulfill];
    }];
    [unlockEvent addDependency:[self.lockStateTracker unlockDependency]];
    NSOperationQueue *queue = [[NSOperationQueue alloc] init];

    [queue addOperation:unlockEvent];

    self.lockStateProvider.aksCurrentlyLocked = false;
    [self.lockStateTracker recheck];

    [self waitForExpectations:@[expectation] timeout:5];

}


@end

#endif /* OCTAGON */

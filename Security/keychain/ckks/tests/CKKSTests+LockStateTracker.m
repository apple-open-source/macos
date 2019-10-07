#if OCTAGON

#import <XCTest/XCTest.h>
#import <OCMock/OCMock.h>

#import "keychain/ckks/CKKSLockStateTracker.h"
#import "tests/secdmockaks/mockaks.h"

@interface CKKSTests_LockStateTracker : XCTestCase
@property bool aksLockState;
@property (nullable) id mockLockStateTracker;
@property CKKSLockStateTracker* lockStateTracker;
@end

@implementation CKKSTests_LockStateTracker

@synthesize aksLockState = _aksLockState;

- (void)setUp {
    [super setUp];

    self.aksLockState = false; // Lie and say AKS is always unlocked
    self.mockLockStateTracker = OCMClassMock([CKKSLockStateTracker class]);
    OCMStub([self.mockLockStateTracker queryAKSLocked]).andCall(self, @selector(aksLockState));

    self.lockStateTracker = [[CKKSLockStateTracker alloc] init];


    [SecMockAKS reset];
}

- (void)tearDown {
    [self.mockLockStateTracker stopMocking];
    self.lockStateTracker = nil;
}

- (bool)aksLockState
{
    return _aksLockState;
}

- (void)setAksLockState:(bool)aksLockState
{

    if(aksLockState) {
        [SecMockAKS lockClassA];
    } else {
        [SecMockAKS unlockAllClasses];
    }
    _aksLockState = aksLockState;
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

    self.aksLockState = true;
    XCTAssertFalse([self.lockStateTracker isLocked], "should be reporting unlocked since we 'missed' the notification");

    XCTAssertFalse([self.lockStateTracker isLockedError:fileError], "file errors are not lock errors");
    XCTAssertFalse([self.lockStateTracker isLocked], "should be 'unlocked' after file errors");

    XCTAssertTrue([self.lockStateTracker isLockedError:lockError], "errSecInteractionNotAllowed is a lock errors");
    XCTAssertTrue([self.lockStateTracker isLocked], "should be locked after lock failure");

    self.aksLockState = false;
    [self.lockStateTracker recheck];

    XCTAssertFalse([self.lockStateTracker isLocked], "should be unlocked");
}

- (void)testWaitForUnlock {

    self.aksLockState = true;
    [self.lockStateTracker recheck];

    XCTestExpectation* expectation = [self expectationWithDescription: @"unlock occurs"];

    NSBlockOperation *unlockEvent = [NSBlockOperation blockOperationWithBlock:^{
        [expectation fulfill];
    }];
    [unlockEvent addDependency:[self.lockStateTracker unlockDependency]];
    NSOperationQueue *queue = [[NSOperationQueue alloc] init];

    [queue addOperation:unlockEvent];

    self.aksLockState = false;
    [self.lockStateTracker recheck];

    [self waitForExpectations:@[expectation] timeout:5];

}


@end

#endif /* OCTAGON */

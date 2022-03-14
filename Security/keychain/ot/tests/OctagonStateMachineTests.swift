import Foundation

extension OctagonStateMachine {
    func condition(_ state: OctagonStateMachineTests.TestEngine.State) -> CKKSCondition {
        return self.stateConditions[state.rawValue]!
    }

    func handle(flag: OctagonStateMachineTests.TestEngine.Flag) {
        self.handleFlag(flag.rawValue)
    }
}

extension OctagonStateTransitionOperation {
    convenience init(_ state: OctagonStateMachineTests.TestEngine.State) {
        self.init(name: "test-op", entering: state.rawValue)
    }
}

extension OctagonFlags {
    func checkAndRemove(_ flag: OctagonStateMachineTests.TestEngine.Flag) -> Bool {
        if self._onqueueContains(flag.rawValue) {
            self._onqueueRemoveFlag(flag.rawValue)
            return true
        }
        return false
    }
}

class OctagonStateMachineTests: XCTestCase {
    class TestEngine: OctagonStateMachineEngine {
        enum State: String, CaseIterable {
            case base = "base"
            case receivedFlag = "received_flag"
        }

        enum Flag: String, CaseIterable {
            case aFlag = "a_flag"
            case returnToBase = "return_to_base"
        }

        func _onqueueNextStateMachineTransition(_ currentState: String,
                                                flags: OctagonFlags,
                                                pendingFlags: OctagonStateOnqueuePendingFlagHandler) -> (CKKSResultOperation & OctagonStateTransitionOperationProtocol)? {
            guard let state = State(rawValue: currentState) else {
                return nil
            }

            switch state {
            case .base:
                if flags.checkAndRemove(.aFlag) {
                    return OctagonStateTransitionOperation(.receivedFlag)
                }
                return nil
            case .receivedFlag:
                if flags.checkAndRemove(.returnToBase) {
                    return OctagonStateTransitionOperation(.base)
                }
                return nil
            }
        }
    }

    var stateMachineQueue: DispatchQueue!
    var stateMachine: OctagonStateMachine!
    var lockStateProvider: CKKSMockLockStateProvider!

    var reachabilityTracker: CKKSReachabilityTracker!

    // The state machine doesn't hold a strong reference to its engine. Help!
    var testEngine: TestEngine!

    override func setUp() {
        super.setUp()

        self.lockStateProvider = CKKSMockLockStateProvider(currentLockStatus: false)
        self.reachabilityTracker = CKKSReachabilityTracker()
        self.testEngine = TestEngine()

        self.stateMachineQueue = DispatchQueue(label: "test-state-machine")
        self.stateMachine = OctagonStateMachine(name: "test-machine",
                                                states: Set(TestEngine.State.allCases.map { $0.rawValue as String }),
                                                flags: Set(TestEngine.Flag.allCases.map { $0.rawValue as String }),
                                                initialState: TestEngine.State.base.rawValue,
                                                queue: self.stateMachineQueue,
                                                stateEngine: self.testEngine,
                                                lockStateTracker: CKKSLockStateTracker(provider: self.lockStateProvider),
                                                reachabilityTracker: self.reachabilityTracker)
    }

    override func tearDown() {
        self.stateMachine.haltOperation()

        self.lockStateProvider = nil
        self.stateMachine = nil
        super.tearDown()
    }

    func testHandleFlag() {
        self.stateMachine.startOperation()

        XCTAssertEqual(0, self.stateMachine.condition(.base).wait(1 * NSEC_PER_SEC), "Should enter base state")
        XCTAssertTrue(self.stateMachine.isPaused(), "State machine should consider itself paused")
        XCTAssertEqual(0, self.stateMachine.paused.wait(1 * NSEC_PER_SEC), "Paused condition should be fulfilled")

        self.stateMachine.handle(flag: .aFlag)

        XCTAssertEqual(0, self.stateMachine.condition(.receivedFlag).wait(100 * NSEC_PER_SEC), "Should enter 'received flag' state")
        XCTAssertTrue(self.stateMachine.isPaused(), "State machine should consider itself paused")
        XCTAssertEqual(0, self.stateMachine.paused.wait(1 * NSEC_PER_SEC), "Paused condition should be fulfilled")

        self.stateMachine.handle(flag: .returnToBase)

        XCTAssertEqual(0, self.stateMachine.condition(.base).wait(1 * NSEC_PER_SEC), "Should enter base state")
        XCTAssertTrue(self.stateMachine.isPaused(), "State machine should consider itself paused")
        XCTAssertEqual(0, self.stateMachine.paused.wait(1 * NSEC_PER_SEC), "Paused condition should be fulfilled")
    }

    func testPendingFlagByTime() {
        self.stateMachine.startOperation()

        XCTAssertEqual(0, self.stateMachine.condition(.base).wait(1 * NSEC_PER_SEC), "Should enter base state")

        self.stateMachine.handle(OctagonPendingFlag(flag: TestEngine.Flag.aFlag.rawValue,
                                                    delayInSeconds: 2))

        // We should _not_ advance within a second
        XCTAssertNotEqual(0, self.stateMachine.condition(.receivedFlag).wait(1 * NSEC_PER_SEC), "Should not enter 'received flag' state")

        // And then we should!
        XCTAssertEqual(0, self.stateMachine.condition(.receivedFlag).wait(2 * NSEC_PER_SEC), "Should enter 'received flag' state")
    }

    func testPendingFlagByLockState() {
        self.stateMachine.startOperation()

        XCTAssertEqual(0, self.stateMachine.condition(.base).wait(1 * NSEC_PER_SEC), "Should enter base state")

        self.lockStateProvider.aksCurrentlyLocked = true
        self.stateMachine.lockStateTracker?.recheck()

        self.stateMachine.handle(OctagonPendingFlag(flag: TestEngine.Flag.aFlag.rawValue,
                                                    conditions: .deviceUnlocked))

        // We should _not_ advance
        XCTAssertNotEqual(0, self.stateMachine.condition(.receivedFlag).wait(1 * NSEC_PER_SEC), "Should not enter 'received flag' state")

        self.lockStateProvider.aksCurrentlyLocked = false
        self.stateMachine.lockStateTracker?.recheck()

        // But after an unlock, we should
        XCTAssertEqual(0, self.stateMachine.condition(.receivedFlag).wait(1 * NSEC_PER_SEC), "Should enter 'received flag' state")

        // And it works the same in the other direction
        self.lockStateProvider.aksCurrentlyLocked = true
        self.stateMachine.lockStateTracker?.recheck()

        self.stateMachine.handle(OctagonPendingFlag(flag: TestEngine.Flag.returnToBase.rawValue,
                                                    conditions: .deviceUnlocked))
        XCTAssertNotEqual(0, self.stateMachine.condition(.base).wait(5 * NSEC_PER_SEC), "Should not enter base state")

        self.lockStateProvider.aksCurrentlyLocked = false
        self.stateMachine.lockStateTracker?.recheck()
        XCTAssertEqual(0, self.stateMachine.condition(.base).wait(1 * NSEC_PER_SEC), "Should enter base state")
    }

    func testPendingFlagByNetworkCondition() {
        self.stateMachine.startOperation()

        XCTAssertEqual(0, self.stateMachine.condition(.base).wait(1 * NSEC_PER_SEC), "Should enter base state")

        self.reachabilityTracker.setNetworkReachability(false)

        self.stateMachine.handle(OctagonPendingFlag(flag: TestEngine.Flag.aFlag.rawValue,
                                                    conditions: .networkReachable))

        // We should _not_ advance
        XCTAssertNotEqual(0, self.stateMachine.condition(.receivedFlag).wait(1 * NSEC_PER_SEC), "Should not enter 'received flag' state")

        self.reachabilityTracker.setNetworkReachability(true)

        // But after the network changes, we should
        XCTAssertEqual(0, self.stateMachine.condition(.receivedFlag).wait(1 * NSEC_PER_SEC), "Should enter 'received flag' state")

        // And it works the same in the other direction
        self.reachabilityTracker.setNetworkReachability(false)

        self.stateMachine.handle(OctagonPendingFlag(flag: TestEngine.Flag.returnToBase.rawValue,
                                                    conditions: .networkReachable))
        XCTAssertNotEqual(0, self.stateMachine.condition(.base).wait(5 * NSEC_PER_SEC), "Should not enter base state")

        self.reachabilityTracker.setNetworkReachability(true)
        XCTAssertEqual(0, self.stateMachine.condition(.base).wait(1 * NSEC_PER_SEC), "Should enter base state")
    }

    func testPendingFlagByMultipleConditions() {
        self.stateMachine.startOperation()

        XCTAssertEqual(0, self.stateMachine.condition(.base).wait(1 * NSEC_PER_SEC), "Should enter base state")

        self.lockStateProvider.aksCurrentlyLocked = true
        self.stateMachine.lockStateTracker?.recheck()
        self.reachabilityTracker.setNetworkReachability(false)

        self.stateMachine.handle(OctagonPendingFlag(flag: TestEngine.Flag.aFlag.rawValue,
                                                    conditions: [.networkReachable, .deviceUnlocked]))

        // We should _not_ advance
        XCTAssertNotEqual(0, self.stateMachine.condition(.receivedFlag).wait(500 * NSEC_PER_MSEC), "Should not enter 'received flag' state")

        self.reachabilityTracker.setNetworkReachability(true)

        // Still no....
        XCTAssertNotEqual(0, self.stateMachine.condition(.receivedFlag).wait(500 * NSEC_PER_MSEC), "Should not enter 'received flag' state")

        // Until both fire.
        self.lockStateProvider.aksCurrentlyLocked = false
        self.stateMachine.lockStateTracker?.recheck()
        XCTAssertEqual(0, self.stateMachine.condition(.receivedFlag).wait(1 * NSEC_PER_SEC), "Should enter 'received flag' state")

        // And in reverse...
        self.lockStateProvider.aksCurrentlyLocked = true
        self.stateMachine.lockStateTracker?.recheck()
        self.reachabilityTracker.setNetworkReachability(false)
        self.stateMachine.handle(OctagonPendingFlag(flag: TestEngine.Flag.returnToBase.rawValue,
                                                    conditions: [.networkReachable, .deviceUnlocked]))

        XCTAssertNotEqual(0, self.stateMachine.condition(.base).wait(500 * NSEC_PER_MSEC), "Should not enter 'base' state")

        self.lockStateProvider.aksCurrentlyLocked = false
        self.stateMachine.lockStateTracker?.recheck()
        XCTAssertNotEqual(0, self.stateMachine.condition(.base).wait(500 * NSEC_PER_MSEC), "Should not enter 'base' state")

        self.reachabilityTracker.setNetworkReachability(true)
        XCTAssertEqual(0, self.stateMachine.condition(.base).wait(1 * NSEC_PER_SEC), "Should enter 'base' state")
    }

    func testPendingFlagByMultipleConditionsWithConditionFlapping() {
        self.stateMachine.startOperation()

        XCTAssertEqual(0, self.stateMachine.condition(.base).wait(1 * NSEC_PER_SEC), "Should enter base state")

        self.lockStateProvider.aksCurrentlyLocked = true
        self.stateMachine.lockStateTracker?.recheck()
        self.reachabilityTracker.setNetworkReachability(false)

        self.stateMachine.handle(OctagonPendingFlag(flag: TestEngine.Flag.aFlag.rawValue,
                                                    conditions: [.networkReachable, .deviceUnlocked]))

        // We should not receive this flag until both conditions are true at the same time. Test that by changing the lock state repeatedly, but ending in the 'locked' state
        self.lockStateProvider.aksCurrentlyLocked = false
        self.stateMachine.lockStateTracker?.recheck()

        self.lockStateProvider.aksCurrentlyLocked = true
        self.stateMachine.lockStateTracker?.recheck()

        self.lockStateProvider.aksCurrentlyLocked = false
        self.stateMachine.lockStateTracker?.recheck()

        self.lockStateProvider.aksCurrentlyLocked = true
        self.stateMachine.lockStateTracker?.recheck()

        // Network is regained while the device is locked.
        self.reachabilityTracker.setNetworkReachability(true)
        self.reachabilityTracker.setNetworkReachability(false)
        self.reachabilityTracker.setNetworkReachability(true)

        // We should not receive the flag.
        XCTAssertNotEqual(0, self.stateMachine.condition(.receivedFlag).wait(1 * NSEC_PER_SEC), "Should not enter 'received flag' state")

        // And then again, if the network goes away but then the lock state returns..
        self.reachabilityTracker.setNetworkReachability(false)

        XCTAssertNotEqual(0, self.stateMachine.condition(.receivedFlag).wait(500 * NSEC_PER_MSEC), "Should not enter 'received flag' state")

        self.lockStateProvider.aksCurrentlyLocked = false
        self.stateMachine.lockStateTracker?.recheck()

        XCTAssertNotEqual(0, self.stateMachine.condition(.receivedFlag).wait(1 * NSEC_PER_SEC), "Should not enter 'received flag' state")

        // until finally both are in the right state
        self.reachabilityTracker.setNetworkReachability(true)

        XCTAssertEqual(0, self.stateMachine.condition(.receivedFlag).wait(1 * NSEC_PER_SEC), "Should enter 'received flag' state")
    }

    func testPendingFlagBothConditionAndDelay() {
        self.stateMachine.startOperation()

        XCTAssertEqual(0, self.stateMachine.condition(.base).wait(1 * NSEC_PER_SEC), "Should enter base state")

        // First, the condition arrives before the timeout expires
        self.lockStateProvider.aksCurrentlyLocked = true
        self.stateMachine.lockStateTracker?.recheck()

        self.stateMachine.handle(OctagonPendingFlag(flag: TestEngine.Flag.aFlag.rawValue,
                                                    conditions: .deviceUnlocked,
                                                    delayInSeconds: 2))

        XCTAssertNotEqual(0, self.stateMachine.condition(.receivedFlag).wait(500 * NSEC_PER_MSEC), "Should not enter 'received flag' state")

        self.lockStateProvider.aksCurrentlyLocked = false
        self.stateMachine.lockStateTracker?.recheck()

        XCTAssertNotEqual(0, self.stateMachine.condition(.receivedFlag).wait(1 * NSEC_PER_SEC), "Should not enter 'received flag' state")

        // But after 2s total, we should
        XCTAssertEqual(0, self.stateMachine.condition(.receivedFlag).wait(3 * NSEC_PER_SEC), "Should enter 'received flag' state")

        // And the same thing happens if the delay expires before the condition
        self.lockStateProvider.aksCurrentlyLocked = true
        self.stateMachine.lockStateTracker?.recheck()

        self.stateMachine.handle(OctagonPendingFlag(flag: TestEngine.Flag.returnToBase.rawValue,
                                                    conditions: .deviceUnlocked,
                                                    delayInSeconds: 0.5))

        XCTAssertNotEqual(0, self.stateMachine.condition(.base).wait(1 * NSEC_PER_SEC), "Should not enter 'base' state")
        self.lockStateProvider.aksCurrentlyLocked = false
        self.stateMachine.lockStateTracker?.recheck()
        XCTAssertEqual(0, self.stateMachine.condition(.base).wait(1 * NSEC_PER_SEC), "Should not enter 'base' state")
    }

    func testPendingFlagScheduler() {
        self.stateMachine.startOperation()

        XCTAssertEqual(0, self.stateMachine.condition(.base).wait(1 * NSEC_PER_SEC), "Should enter base state")

        let scheduler = CKKSNearFutureScheduler(name: "test-scheduler",
                                                delay: 1 * NSEC_PER_SEC,
                                                keepProcessAlive: false,
                                                dependencyDescriptionCode: 0) {}

        self.stateMachine.handle(OctagonPendingFlag(flag: TestEngine.Flag.aFlag.rawValue,
                                                    scheduler: scheduler))

        XCTAssertNotEqual(0, self.stateMachine.condition(.receivedFlag).wait(500 * NSEC_PER_MSEC), "Should not enter 'received flag' state")

        // But after 2s total, we should receive the flag
        XCTAssertEqual(0, self.stateMachine.condition(.receivedFlag).wait(2 * NSEC_PER_SEC), "Should enter 'received flag' state")

        self.stateMachine.handle(OctagonPendingFlag(flag: TestEngine.Flag.returnToBase.rawValue,
                                                    scheduler: scheduler))

        XCTAssertNotEqual(0, self.stateMachine.condition(.base).wait(500 * NSEC_PER_MSEC), "Should not enter 'base' state")
        XCTAssertEqual(0, self.stateMachine.condition(.base).wait(2 * NSEC_PER_SEC), "Should enter 'base' state")
    }

    func testPendingFlagSchedulerPlusCondition() {
        self.stateMachine.startOperation()

        XCTAssertEqual(0, self.stateMachine.condition(.base).wait(1 * NSEC_PER_SEC), "Should enter base state")

        let scheduler = CKKSNearFutureScheduler(name: "test-scheduler",
                                                delay: 500 * NSEC_PER_MSEC,
                                                keepProcessAlive: false,
                                                dependencyDescriptionCode: 0) {}

        self.lockStateProvider.aksCurrentlyLocked = true
        self.stateMachine.lockStateTracker?.recheck()

        self.stateMachine.handle(OctagonPendingFlag(flag: TestEngine.Flag.aFlag.rawValue,
                                                    conditions: .deviceUnlocked,
                                                    scheduler: scheduler))

        XCTAssertNotEqual(0, self.stateMachine.condition(.receivedFlag).wait(1500 * NSEC_PER_MSEC), "Should not enter 'received flag' state")

        // But we do when the device is unlocked!
        // But after 2s total, we should receive the flag
        self.lockStateProvider.aksCurrentlyLocked = false
        self.stateMachine.lockStateTracker?.recheck()
        XCTAssertEqual(0, self.stateMachine.condition(.receivedFlag).wait(500 * NSEC_PER_MSEC), "Should enter 'received flag' state")

        // And when the machine stays unlocked, things are fast as well
        self.stateMachine.handle(OctagonPendingFlag(flag: TestEngine.Flag.returnToBase.rawValue,
                                                    conditions: .deviceUnlocked,
                                                    scheduler: scheduler))

        XCTAssertEqual(0, self.stateMachine.condition(.base).wait(1 * NSEC_PER_SEC), "Should enter 'base' state")
    }

    func testWatchers() {
        self.stateMachine.startOperation()

        XCTAssertEqual(0, self.stateMachine.condition(.base).wait(1 * NSEC_PER_SEC), "Should enter base state")
        XCTAssertTrue(self.stateMachine.isPaused(), "State machine should consider itself paused")
        XCTAssertEqual(0, self.stateMachine.paused.wait(1 * NSEC_PER_SEC), "Paused condition should be fulfilled")

        let immediateSuccessWatcher = OctagonStateMultiStateArrivalWatcher(named: "immediate-successs",
                                                                           serialQueue: self.stateMachineQueue,
                                                                           states: Set([
                                                                            TestEngine.State.base.rawValue,
                                                                           ]))
        immediateSuccessWatcher.timeout(10*NSEC_PER_SEC)
        self.stateMachine.register(immediateSuccessWatcher)

        let immediateFailureWatcher = OctagonStateMultiStateArrivalWatcher(named: "immediate-failure",
                                                                           serialQueue: self.stateMachineQueue,
                                                                           states: Set([
                                                                            TestEngine.State.receivedFlag.rawValue,
                                                                           ]),
                                                                           failStates: [
                                                                            TestEngine.State.base.rawValue : NSError(domain: "test", code: 1, userInfo: [:])
                                                                           ])
        immediateFailureWatcher.timeout(10*NSEC_PER_SEC)
        self.stateMachine.register(immediateFailureWatcher)

        let pendSuccessWatcher = OctagonStateMultiStateArrivalWatcher(named: "pend-successs",
                                                                      serialQueue: self.stateMachineQueue,
                                                                      states: Set([
                                                                        TestEngine.State.receivedFlag.rawValue,
                                                                      ]))
        pendSuccessWatcher.timeout(10*NSEC_PER_SEC)
        self.stateMachine.register(pendSuccessWatcher)

        let pendFailureWatcher = OctagonStateMultiStateArrivalWatcher(named: "pend-successs",
                                                                      serialQueue: self.stateMachineQueue,
                                                                      states: Set(),
                                                                      failStates: [
                                                                        TestEngine.State.receivedFlag.rawValue : NSError(domain: "test", code: 1, userInfo: [:])
                                                                      ])
        pendFailureWatcher.timeout(10*NSEC_PER_SEC)
        self.stateMachine.register(pendFailureWatcher)

        immediateSuccessWatcher.result.waitUntilFinished()
        XCTAssertTrue(immediateSuccessWatcher.result.isFinished, "Should have immediately completed")
        XCTAssertNil(immediateSuccessWatcher.result.error, "Should have no error when immediately succeeding")

        immediateSuccessWatcher.completeWithErrorIfPending(NSError(domain: "test", code: 1, userInfo: [:]))
        XCTAssertNil(immediateSuccessWatcher.result.error, "completeWithErrorIfPending should be a no-op on a completed watcher")

        immediateFailureWatcher.result.waitUntilFinished()
        XCTAssertTrue(immediateFailureWatcher.result.isFinished, "Should have immediately completed")
        XCTAssertNotNil(immediateFailureWatcher.result.error, "Should have an error when immediately failing")
        do {
            let error = immediateFailureWatcher.result.error as NSError?
            XCTAssertEqual(error?.domain, "test", "domain should match")
            XCTAssertEqual(error?.code, 1, "Code should match")
        }

        XCTAssertFalse(pendSuccessWatcher.result.isFinished, "pend-success should not have immediately finished")
        XCTAssertFalse(pendFailureWatcher.result.isFinished, "pend-failure should not have immediately finished")

        self.stateMachine.handle(flag: .aFlag)

        XCTAssertEqual(0, self.stateMachine.condition(.receivedFlag).wait(100 * NSEC_PER_SEC), "Should enter 'received flag' state")
        XCTAssertTrue(self.stateMachine.isPaused(), "State machine should consider itself paused")
        XCTAssertEqual(0, self.stateMachine.paused.wait(1 * NSEC_PER_SEC), "Paused condition should be fulfilled")

        pendSuccessWatcher.result.waitUntilFinished()
        XCTAssertTrue(pendSuccessWatcher.result.isFinished, "pend-success should have completed")
        XCTAssertNil(pendSuccessWatcher.result.error, "Should have no error when expecting success")

        pendFailureWatcher.result.waitUntilFinished()
        XCTAssertTrue(pendFailureWatcher.result.isFinished, "pend-faiulre have completed")
        XCTAssertNotNil(pendFailureWatcher.result.error, "Should have an error when expecting failure")
        do {
            let error = pendFailureWatcher.result.error as NSError?
            XCTAssertEqual(error?.domain, "test", "domain should match")
            XCTAssertEqual(error?.code, 1, "Code should match")
        }

        self.stateMachine.handle(flag: .returnToBase)

        XCTAssertEqual(0, self.stateMachine.condition(.base).wait(1 * NSEC_PER_SEC), "Should enter base state")
        XCTAssertTrue(self.stateMachine.isPaused(), "State machine should consider itself paused")
        XCTAssertEqual(0, self.stateMachine.paused.wait(1 * NSEC_PER_SEC), "Paused condition should be fulfilled")


        let completeWithErrorWatcher = OctagonStateMultiStateArrivalWatcher(named: "complete-with-error",
                                                                            serialQueue: self.stateMachineQueue,
                                                                            states: Set(),
                                                                            failStates: [
                                                                                TestEngine.State.receivedFlag.rawValue : NSError(domain: "test", code: 1, userInfo: [:])
                                                                            ])
        completeWithErrorWatcher.completeWithErrorIfPending(NSError(domain: "test", code: 1, userInfo: [:]))
        completeWithErrorWatcher.result.waitUntilFinished()
        XCTAssertTrue(completeWithErrorWatcher.result.isFinished, "pend-failure have completed")
        XCTAssertNotNil(completeWithErrorWatcher.result.error, "Should have an error when expecting failure")
        do {
            let error = completeWithErrorWatcher.result.error as NSError?
            XCTAssertEqual(error?.domain, "test", "domain should match")
            XCTAssertEqual(error?.code, 1, "Code should match")
        }
    }
}

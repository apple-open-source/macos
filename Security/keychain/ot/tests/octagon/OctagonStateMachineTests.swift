
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
        if(self._onqueueContains(flag.rawValue)) {
            self._onqueueRemoveFlag(flag.rawValue)
            return true
        }
        return false
    }
}

class OctagonStateMachineTests : XCTestCase {
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

            switch(state) {
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

    var stateMachine : OctagonStateMachine!
    var lockStateProvider : CKKSMockLockStateProvider!

    // The state machine doesn't hold a strong reference to its engine. Help!
    var testEngine : TestEngine!

    override func setUp() {
        super.setUp()

        self.lockStateProvider = CKKSMockLockStateProvider(currentLockStatus: false)
        self.testEngine = TestEngine()
        self.stateMachine = OctagonStateMachine(name: "test-machine",
                                                states: Set(TestEngine.State.allCases.map { $0.rawValue as String } ),
                                                flags: Set(TestEngine.Flag.allCases.map { $0.rawValue as String } ),
                                                initialState: TestEngine.State.base.rawValue,
                                                queue: DispatchQueue(label: "test-state-machine"),
                                                stateEngine: self.testEngine,
                                                lockStateTracker: CKKSLockStateTracker(provider: self.lockStateProvider),
                                                reachabilityTracker: CKKSReachabilityTracker())
    }

    override func tearDown() {
        self.stateMachine.haltOperation()

        self.lockStateProvider = nil
        self.stateMachine = nil
    }

    func testHandleFlag() {
        self.stateMachine.startOperation()

        XCTAssertEqual(0, self.stateMachine.condition(.base).wait(1*NSEC_PER_SEC), "Should enter base state")
        XCTAssertTrue(self.stateMachine.isPaused(), "State machine should consider itself paused")
        XCTAssertEqual(0, self.stateMachine.paused.wait(1*NSEC_PER_SEC), "Paused condition should be fulfilled")

        self.stateMachine.handle(flag: .aFlag)

        XCTAssertEqual(0, self.stateMachine.condition(.receivedFlag).wait(100*NSEC_PER_SEC), "Should enter 'received flag' state")
        XCTAssertTrue(self.stateMachine.isPaused(), "State machine should consider itself paused")
        XCTAssertEqual(0, self.stateMachine.paused.wait(1*NSEC_PER_SEC), "Paused condition should be fulfilled")

        self.stateMachine.handle(flag: .returnToBase)

        XCTAssertEqual(0, self.stateMachine.condition(.base).wait(1*NSEC_PER_SEC), "Should enter base state")
        XCTAssertTrue(self.stateMachine.isPaused(), "State machine should consider itself paused")
        XCTAssertEqual(0, self.stateMachine.paused.wait(1*NSEC_PER_SEC), "Paused condition should be fulfilled")
    }

    func testPendingFlagByTime() {
        self.stateMachine.startOperation()

        XCTAssertEqual(0, self.stateMachine.condition(.base).wait(1*NSEC_PER_SEC), "Should enter base state")

        self.stateMachine.handle(OctagonPendingFlag(flag: TestEngine.Flag.aFlag.rawValue,
                                                    delayInSeconds: 2))

        // We should _not_ advance within a second
        XCTAssertNotEqual(0, self.stateMachine.condition(.receivedFlag).wait(1*NSEC_PER_SEC), "Should not enter 'received flag' state")

        // And then we should!
        XCTAssertEqual(0, self.stateMachine.condition(.receivedFlag).wait(2*NSEC_PER_SEC), "Should enter 'received flag' state")
    }

    func testPendingFlagByLockState() {
        self.stateMachine.startOperation()

        XCTAssertEqual(0, self.stateMachine.condition(.base).wait(1*NSEC_PER_SEC), "Should enter base state")

        self.lockStateProvider.aksCurrentlyLocked = true
        self.stateMachine.lockStateTracker?.recheck()

        self.stateMachine.handle(OctagonPendingFlag(flag: TestEngine.Flag.aFlag.rawValue,
                                                    conditions: .deviceUnlocked))

        // We should _not_ advance
        XCTAssertNotEqual(0, self.stateMachine.condition(.receivedFlag).wait(1*NSEC_PER_SEC), "Should not enter 'received flag' state")

        self.lockStateProvider.aksCurrentlyLocked = false
        self.stateMachine.lockStateTracker?.recheck()

        // But after an unlock, we should
        XCTAssertEqual(0, self.stateMachine.condition(.receivedFlag).wait(1*NSEC_PER_SEC), "Should not enter 'received flag' state")

        // And it works the same in the other direction
        self.lockStateProvider.aksCurrentlyLocked = true
        self.stateMachine.lockStateTracker?.recheck()

        self.stateMachine.handle(OctagonPendingFlag(flag: TestEngine.Flag.returnToBase.rawValue,
                                                    conditions: .deviceUnlocked))
        XCTAssertNotEqual(0, self.stateMachine.condition(.base).wait(5*NSEC_PER_SEC), "Should not enter base state")

        self.lockStateProvider.aksCurrentlyLocked = false
        self.stateMachine.lockStateTracker?.recheck()
        XCTAssertEqual(0, self.stateMachine.condition(.base).wait(1*NSEC_PER_SEC), "Should enter base state")
    }

    func testPendingFlagCombination() {
        self.stateMachine.startOperation()

        XCTAssertEqual(0, self.stateMachine.condition(.base).wait(1*NSEC_PER_SEC), "Should enter base state")

        // First, the condition arrives before the timeout expires
        self.lockStateProvider.aksCurrentlyLocked = true
        self.stateMachine.lockStateTracker?.recheck()

        self.stateMachine.handle(OctagonPendingFlag(flag: TestEngine.Flag.aFlag.rawValue,
                                                    conditions: .deviceUnlocked,
                                                    delayInSeconds: 2))

        XCTAssertNotEqual(0, self.stateMachine.condition(.receivedFlag).wait(500*NSEC_PER_MSEC), "Should not enter 'received flag' state")

        self.lockStateProvider.aksCurrentlyLocked = false
        self.stateMachine.lockStateTracker?.recheck()

        XCTAssertNotEqual(0, self.stateMachine.condition(.receivedFlag).wait(1*NSEC_PER_SEC), "Should not enter 'received flag' state")

        // But after 2s total, we should
        XCTAssertEqual(0, self.stateMachine.condition(.receivedFlag).wait(3*NSEC_PER_SEC), "Should enter 'received flag' state")

        // And the same thing happens if the delay expires before the condition
        self.lockStateProvider.aksCurrentlyLocked = true
        self.stateMachine.lockStateTracker?.recheck()

        self.stateMachine.handle(OctagonPendingFlag(flag: TestEngine.Flag.returnToBase.rawValue,
                                                    conditions: .deviceUnlocked,
                                                    delayInSeconds: 0.5))

        XCTAssertNotEqual(0, self.stateMachine.condition(.base).wait(1*NSEC_PER_SEC), "Should not enter 'base' state")
        self.lockStateProvider.aksCurrentlyLocked = false
        self.stateMachine.lockStateTracker?.recheck()
        XCTAssertEqual(0, self.stateMachine.condition(.base).wait(1*NSEC_PER_SEC), "Should not enter 'base' state")
    }
}


#if OCTAGON

#import "keychain/categories/NSError+UsefulConstructors.h"
#import "keychain/ot/OctagonStateMachine.h"
#import "keychain/ot/OctagonStateMachineObservers.h"
#import "keychain/ot/ObjCImprovements.h"
#import "keychain/ckks/CKKSNearFutureScheduler.h"
#import "keychain/ckks/CKKS.h"
#import "keychain/ot/OTStates.h"
#import "utilities/debugging.h"

#define statemachinelog(scope, format, ...)                                                                        \
{                                                                                                            \
os_log(secLogObjForCFScope((__bridge CFStringRef)[NSString stringWithFormat:@"%@-%@", self.name, @(scope)]), \
format,                                                                                                      \
##__VA_ARGS__);                                                                                              \
}

@interface OctagonStateMachine ()
{
    OctagonState* _currentState;
}

@property (weak) id<OctagonStateMachineEngine> stateEngine;

@property NSMutableDictionary<OctagonState*, CKKSCondition*>* mutableStateConditions;

@property NSOperationQueue* operationQueue;

@property NSString* name;

// Make writable
@property CKKSCondition* paused;
@property OctagonState* currentState;
@property OctagonFlags* currentFlags;

// Set this to an operation to pause the state machine in-flight
@property NSOperation* holdStateMachineOperation;

// When entering any state in this set, holdStateMachineOperation will be filled in
@property NSMutableSet<OctagonState*>* testHoldStates;

@property (nullable) CKKSResultOperation* nextStateMachineCycleOperation;

@property NSMutableArray<OctagonStateTransitionRequest<CKKSResultOperation<OctagonStateTransitionOperationProtocol>*>*>* stateMachineRequests;
@property NSMutableArray<id<OctagonStateTransitionWatcherProtocol>>* stateMachineWatchers;

@property BOOL halted;
@property bool allowPendingFlags;
@property NSMutableDictionary<OctagonFlag*, OctagonPendingFlag*>* pendingFlags;
@property CKKSNearFutureScheduler* pendingFlagsScheduler;

@property OctagonPendingConditions conditionChecksInFlight;
@property OctagonPendingConditions currentConditions;
@property (nullable) NSOperation* checkUnlockOperation;
@property (nullable) NSOperation* checkReachabilityOperation;
@end

@implementation OctagonStateMachine

- (instancetype)initWithName:(NSString*)name
                      states:(NSDictionary<OctagonState*, NSNumber*>*)possibleStates
                       flags:(NSSet<OctagonFlag*>*)possibleFlags
                initialState:(OctagonState*)initialState
                       queue:(dispatch_queue_t)queue
                 stateEngine:(id<OctagonStateMachineEngine>)stateEngine
  unexpectedStateErrorDomain:(NSString*)unexpectedStateErrorDomain
            lockStateTracker:(CKKSLockStateTracker*)lockStateTracker
         reachabilityTracker:(CKKSReachabilityTracker*)reachabilityTracker
{
    if ((self = [super init])) {
        _name = name;

        _lockStateTracker = lockStateTracker;
        _reachabilityTracker = reachabilityTracker;
        _conditionChecksInFlight = 0;
        _currentConditions = 0;

        // Every state machine starts in OctagonStateMachineNotStarted, so help them out a bit.
        NSMutableDictionary<OctagonState*, NSNumber*>* actualPossibleStates = [possibleStates mutableCopy];
        actualPossibleStates[OctagonStateMachineNotStarted] = @-1;
        actualPossibleStates[OctagonStateMachineHalted] = @-2;

        _stateNumberMap = actualPossibleStates;
        _unexpectedStateErrorDomain = unexpectedStateErrorDomain;

        _allowableStates = [NSSet setWithArray:[actualPossibleStates allKeys]];

        _queue = queue;
        _operationQueue = [[NSOperationQueue alloc] init];
        _currentFlags = [[OctagonFlags alloc] initWithQueue:queue flags:possibleFlags];

        _stateEngine = stateEngine;

        _holdStateMachineOperation = [NSBlockOperation blockOperationWithBlock:^{}];
        _testHoldStates = [NSMutableSet set];
        _halted = false;

        _mutableStateConditions = [[NSMutableDictionary alloc] init];
        for(OctagonState* state in actualPossibleStates.allKeys) {
            self.mutableStateConditions[state] = [[CKKSCondition alloc] init];
        };

        // Use the setter method to set the condition variables
        self.currentState = OctagonStateMachineNotStarted;

        _stateMachineRequests = [NSMutableArray array];
        _stateMachineWatchers = [NSMutableArray array];

        WEAKIFY(self);
        _allowPendingFlags = true;
        _pendingFlags = [NSMutableDictionary dictionary];
        _pendingFlagsScheduler =  [[CKKSNearFutureScheduler alloc] initWithName:[NSString stringWithFormat:@"%@-pending-flag", name]
                                                                          delay:100*NSEC_PER_MSEC
                                                               keepProcessAlive:false
                                                      dependencyDescriptionCode:CKKSResultDescriptionPendingFlag
                                                                block:^{
                                                                    STRONGIFY(self);
                                                                    dispatch_sync(self.queue, ^{
                                                                        [self _onqueueSendAnyPendingFlags];
                                                                    });
                                                                }];

        OctagonStateTransitionOperation* initializeOp = [OctagonStateTransitionOperation named:@"initialize"
                                                                                      entering:initialState];
        [initializeOp addDependency:_holdStateMachineOperation];
        [_operationQueue addOperation:initializeOp];

        _paused = [[CKKSCondition alloc] init];

        _nextStateMachineCycleOperation = [self createOperationToFinishAttempt:initializeOp];
        [_operationQueue addOperation:_nextStateMachineCycleOperation];
    }
    return self;
}


- (NSDictionary<OctagonState*, CKKSCondition*>*)stateConditions
{
    __block NSDictionary* conditions = nil;
    dispatch_sync(self.queue, ^{
        conditions = [self.mutableStateConditions copy];
    });

    return conditions;
}

- (NSString*)pendingFlagsString
{
    return [self.pendingFlags.allValues componentsJoinedByString:@","];
}

- (NSString*)description
{
    NSString* pendingFlags = @"";
    if(self.pendingFlags.count != 0) {
        pendingFlags = [NSString stringWithFormat:@" (pending: %@)", [self pendingFlagsString]];
    }
    return [NSString stringWithFormat:@"<OctagonStateMachine(%@,%@,%@)>", self.name, self.currentState, pendingFlags];
}

#pragma mark - Bookkeeping

- (id<OctagonFlagSetter>)flags {
    return self.currentFlags;
}

- (OctagonState* _Nonnull)currentState {
    return _currentState;
}

- (void)setCurrentState:(OctagonState* _Nonnull)state {
    if((state == nil && _currentState == nil) || ([state isEqualToString:_currentState])) {
        // No change, do nothing.
    } else {
        // Fixup the condition variables as part of setting this state
        if(_currentState) {
            self.mutableStateConditions[_currentState] = [[CKKSCondition alloc] init];
        }

        NSAssert([self.allowableStates containsObject:state], @"state machine tried to enter unknown state %@", state);
        _currentState = state;

        if(state) {
            [self.mutableStateConditions[state] fulfill];
        }
    }
}

- (OctagonState* _Nonnull)waitForState:(OctagonState* _Nonnull)wantedState wait:(uint64_t)timeout {
    if ([self.stateConditions[wantedState] wait:timeout]) {
        return _currentState;
    } else {
        return wantedState;
    }
}

#pragma mark - Machinery

- (CKKSResultOperation<OctagonStateTransitionOperationProtocol>* _Nullable)_onqueueNextStateMachineTransition
{
    dispatch_assert_queue(self.queue);

    if(self.halted) {
        if([self.currentState isEqualToString:OctagonStateMachineHalted]) {
            return nil;
        } else {
            return [OctagonStateTransitionOperation named:@"halt"
                                                 entering:OctagonStateMachineHalted];
        }
    }

    // Check requests: do any of them want to come from this state?
    for(OctagonStateTransitionRequest<OctagonStateTransitionOperation*>* request in self.stateMachineRequests) {
        if([request.sourceStates containsObject:self.currentState]) {
            OctagonStateTransitionOperation* attempt = [request _onqueueStart];

            if(attempt) {
                statemachinelog("state", "Running state machine request %@ (from %@)", request, self.currentState);
                return attempt;
            }
        }
    }

    // Ask the stateEngine what it would like to do
    return [self.stateEngine _onqueueNextStateMachineTransition:self.currentState
                                                          flags:self.currentFlags
                                                   pendingFlags:self];
}

- (void)_onqueueStartNextStateMachineOperation:(bool)immediatelyAfterPreviousOp {
    dispatch_assert_queue(self.queue);

    // early-exit if there's an existing operation. That operation will call this function after it's done
    if(self.nextStateMachineCycleOperation) {
        return;
    }

    if([self.testHoldStates containsObject:self.currentState]) {
        statemachinelog("state", "In test hold for state %@; pausing", self.currentState);
        [self.paused fulfill];
        return;
    }

    CKKSResultOperation<OctagonStateTransitionOperationProtocol>* nextOp = [self _onqueueNextStateMachineTransition];
    if(nextOp) {
        statemachinelog("state", "Beginning state transition attempt %@", nextOp);

        self.nextStateMachineCycleOperation = [self createOperationToFinishAttempt:nextOp];
        [self.operationQueue addOperation:self.nextStateMachineCycleOperation];

        [nextOp addNullableDependency:self.holdStateMachineOperation];
        nextOp.qualityOfService = NSQualityOfServiceUserInitiated;
        [self.operationQueue addOperation:nextOp];

        if(!immediatelyAfterPreviousOp) {
            self.paused = [[CKKSCondition alloc] init];
        }
    } else {
        statemachinelog("state", "State machine rests (%@, f:[%@] p:[%@])", self.currentState, [self.currentFlags contentsAsString], [self pendingFlagsString]);
        [self.paused fulfill];
    }
}


- (CKKSResultOperation*)createOperationToFinishAttempt:(CKKSResultOperation<OctagonStateTransitionOperationProtocol>*)op
{
    WEAKIFY(self);

    CKKSResultOperation* followUp = [CKKSResultOperation named:@"octagon-state-follow-up" withBlock:^{
        STRONGIFY(self);

        dispatch_sync(self.queue, ^{
            statemachinelog("state", "Finishing state transition attempt (ending in %@, intended: %@, f:[%@], p:[%@]): %@ %@",
                      op.nextState,
                      op.intendedState,
                      [self.currentFlags contentsAsString],
                      [self pendingFlagsString],
                      op,
                      op.error ?: @"(no error)");

            for(id<OctagonStateTransitionWatcherProtocol> watcher in self.stateMachineWatchers) {
                statemachinelog("state", "notifying watcher: %@", watcher);
                [watcher onqueueHandleTransition:op];
            }

            // finished watchers can be removed from the list. Use a reversed for loop to enable removal
            for (NSInteger i = self.stateMachineWatchers.count - 1; i >= 0; i--) {
                if([self.stateMachineWatchers[i].result isFinished]) {
                    [self.stateMachineWatchers removeObjectAtIndex:i];
                }
            }

            self.currentState = op.nextState;
            self.nextStateMachineCycleOperation = nil;

            [self _onqueueStartNextStateMachineOperation:true];
        });
    }];
    [followUp addNullableDependency:self.holdStateMachineOperation];
    [followUp addNullableDependency:op];
    followUp.qualityOfService = NSQualityOfServiceUserInitiated;
    return followUp;
}

- (void)pokeStateMachine
{
    dispatch_sync(self.queue, ^{
        [self _onqueuePokeStateMachine];
    });
}

- (void)_onqueuePokeStateMachine
{
    dispatch_assert_queue(self.queue);
    [self _onqueueStartNextStateMachineOperation:false];
}

- (void)handleFlag:(OctagonFlag*)flag
{
    dispatch_sync(self.queue, ^{
        [self _onqueueHandleFlag:flag];
    });
}


- (void)_onqueueHandleFlag:(OctagonFlag*)flag
{
    dispatch_assert_queue(self.queue);
    [self.currentFlags _onqueueSetFlag:flag];
    [self _onqueuePokeStateMachine];
}

- (void)handlePendingFlag:(OctagonPendingFlag *)pendingFlag {
    dispatch_sync(self.queue, ^{
        [self _onqueueHandlePendingFlag:pendingFlag];
    });
}

- (void)_onqueueHandlePendingFlagLater:(OctagonPendingFlag*)pendingFlag {
    dispatch_assert_queue(self.queue);
    dispatch_async(self.queue, ^{
        [self _onqueueHandlePendingFlag:pendingFlag];
    });
}

- (void)_onqueueHandlePendingFlag:(OctagonPendingFlag*)pendingFlag {
    dispatch_assert_queue(self.queue);

    // Overwrite any existing pending flag!
    self.pendingFlags[pendingFlag.flag] = pendingFlag;

    if(pendingFlag.afterOperation) {
        WEAKIFY(self);
        NSOperation* after = [NSBlockOperation blockOperationWithBlock:^{
            STRONGIFY(self);
            dispatch_sync(self.queue, ^{
                statemachinelog("pending-flag", "Finished waiting for operation");
                [self _onqueueSendAnyPendingFlags];
            });
        }];

        [after addNullableDependency:pendingFlag.afterOperation];
        [self.operationQueue addOperation:after];
    }

    [self _onqueueRecheckConditions];
    [self _onqueueSendAnyPendingFlags];
}

- (void)disablePendingFlags {
    dispatch_sync(self.queue, ^{
        self.allowPendingFlags = false;
    });
}

- (NSDictionary<NSString*, NSString*>*)dumpPendingFlags
{
    __block NSMutableDictionary<NSString*, NSString*>* d = [NSMutableDictionary dictionary];
    dispatch_sync(self.queue, ^{
        for(OctagonFlag* flag in [self.pendingFlags allKeys]) {
            d[flag] = [self.pendingFlags[flag] description];
        }
    });

    return d;
}

- (NSArray<OctagonFlag*>*)possiblePendingFlags
{
    return [self.pendingFlags allKeys];
}

- (void)_onqueueRecheckConditions
{
    dispatch_assert_queue(self.queue);

    if(!self.allowPendingFlags) {
        return;
    }

    NSArray<OctagonPendingFlag*>* flags = [self.pendingFlags.allValues copy];
    OctagonPendingConditions allConditions = 0;
    for(OctagonPendingFlag* flag in flags) {
        allConditions |= flag.conditions;
    }
    if(allConditions == 0x0) {
        // No conditions? Don't bother.
        return;
    }

    // But we don't need to recheck anything that's currently being checked
    OctagonPendingConditions conditionsToCheck = allConditions & ~(self.conditionChecksInFlight);

    WEAKIFY(self);

    if(conditionsToCheck & OctagonPendingConditionsDeviceUnlocked) {
        NSAssert(self.lockStateTracker != nil, @"Must have a lock state tracker to wait for unlock");

        if(self.lockStateTracker.isLocked) {
            statemachinelog("pending-flag", "Waiting for unlock");
            self.currentConditions &= ~OctagonPendingConditionsDeviceUnlocked;

            self.checkUnlockOperation = [NSBlockOperation blockOperationWithBlock:^{
                STRONGIFY(self);
                if (self == nil) {
                    return;
                }
                dispatch_sync(self.queue, ^{
                    statemachinelog("pending-flag", "Unlock occurred");
                    self.conditionChecksInFlight &= ~OctagonPendingConditionsDeviceUnlocked;
                    [self _onqueueRecheckConditions];
                    [self _onqueueSendAnyPendingFlags];
                });
            }];
            self.conditionChecksInFlight |= OctagonPendingConditionsDeviceUnlocked;

            [self.checkUnlockOperation addNullableDependency:self.lockStateTracker.unlockDependency];
            [self.operationQueue addOperation:self.checkUnlockOperation];
        } else {
            self.currentConditions |= OctagonPendingConditionsDeviceUnlocked;
        }
    }

    if(conditionsToCheck & OctagonPendingConditionsNetworkReachable) {
        NSAssert(self.reachabilityTracker != nil, @"Must have a network reachability tracker to use network reachability pending flags");

        if(!self.reachabilityTracker.currentReachability) {
            statemachinelog("pending-flag", "Waiting for network reachability");
            self.currentConditions &= ~OctagonPendingConditionsNetworkReachable;

            self.checkReachabilityOperation = [NSBlockOperation blockOperationWithBlock:^{
                STRONGIFY(self);
                if (self == nil) {
                    return;
                }
                dispatch_sync(self.queue, ^{
                    statemachinelog("pending-flag", "Network is reachable");
                    self.conditionChecksInFlight &= ~OctagonPendingConditionsNetworkReachable;
                    [self _onqueueRecheckConditions];
                    [self _onqueueSendAnyPendingFlags];
                });
            }];
            self.conditionChecksInFlight |= OctagonPendingConditionsNetworkReachable;

            [self.checkReachabilityOperation addNullableDependency:self.reachabilityTracker.reachabilityDependency];
            [self.operationQueue addOperation:self.checkReachabilityOperation];
        } else {
            self.currentConditions |= OctagonPendingConditionsNetworkReachable;
        }
    }
}

- (void)_onqueueSendAnyPendingFlags
{
    dispatch_assert_queue(self.queue);

    if(!self.allowPendingFlags) {
        return;
    }

    // Copy pending flags so we can edit the list
    NSArray<OctagonPendingFlag*>* flags = [self.pendingFlags.allValues copy];
    bool setFlag = false;

    NSDate* now = [NSDate date];
    NSDate* earliestDeadline = nil;
    for(OctagonPendingFlag* pendingFlag in flags) {
        bool send = true;

        if(pendingFlag.fireTime) {
            if([pendingFlag.fireTime compare:now] == NSOrderedAscending) {
                statemachinelog("pending-flag", "Delay has ended for pending flag %@", pendingFlag.flag);
            } else {
                send = false;
                earliestDeadline = earliestDeadline == nil ?
                    pendingFlag.fireTime :
                    [earliestDeadline earlierDate:pendingFlag.fireTime];
            }
        }

        if(pendingFlag.afterOperation) {
            if(![pendingFlag.afterOperation isFinished]) {
                send = false;
            } else {
                statemachinelog("pending-flag", "Operation has ended for pending flag %@: %@", pendingFlag.flag, pendingFlag.afterOperation);
            }
        }

        if(pendingFlag.conditions != 0x0) {
            // Also, send the flag if the conditions are right
            if((pendingFlag.conditions & self.currentConditions) == pendingFlag.conditions) {
                // leave send alone!
                statemachinelog("pending-flag", "Conditions are right for %@", pendingFlag.flag);
            } else {
                send = false;
            }
        }

        if(send) {
            [self.currentFlags _onqueueSetFlag:pendingFlag.flag];
            self.pendingFlags[pendingFlag.flag] = nil;
            setFlag = true;
        }
    }

    if(earliestDeadline != nil) {
        NSTimeInterval delay = [earliestDeadline timeIntervalSinceDate:now];
        uint64_t delayNanoseconds = delay * NSEC_PER_SEC;

        [self.pendingFlagsScheduler triggerAt:delayNanoseconds];
    }

    if(setFlag) {
        [self _onqueuePokeStateMachine];
    }
}

#pragma mark - Client Services


- (void)testPauseStateMachineAfterEntering:(OctagonState*)pauseState
{
    dispatch_sync(self.queue, ^{
        [self.testHoldStates addObject:pauseState];
    });
}

- (void)testReleaseStateMachinePause:(OctagonState*)pauseState
{
    dispatch_sync(self.queue, ^{
        statemachinelog("test", "Releasing state machine test pause from %@", pauseState);
        [self.testHoldStates removeObject:pauseState];
        [self _onqueuePokeStateMachine];
    });
}

- (BOOL)isPaused
{
    __block BOOL ret = false;
    dispatch_sync(self.queue, ^{
        ret = self.nextStateMachineCycleOperation == nil;
    });

    return ret;
}

- (void)startOperation {
    dispatch_sync(self.queue, ^{
        if(self.holdStateMachineOperation) {
            [self.operationQueue addOperation: self.holdStateMachineOperation];
            self.holdStateMachineOperation = nil;
        }
    });
}

- (void)haltOperation
{
    dispatch_sync(self.queue, ^{
        if(self.holdStateMachineOperation) {
            [self.operationQueue addOperation:self.holdStateMachineOperation];
            self.holdStateMachineOperation = nil;
        }

        self.halted = true;
        self.allowPendingFlags = false;

        // Ask the state machine to halt itself
        [self _onqueuePokeStateMachine];
    });

    [self.nextStateMachineCycleOperation waitUntilFinished];
}

- (NSError* _Nullable)timeoutErrorForState:(OctagonState*)state
{
    NSNumber* number = self.stateNumberMap[state];
    if(number != nil) {
        return [NSError errorWithDomain:self.unexpectedStateErrorDomain
                                   code:[number integerValue]
                            description:[NSString stringWithFormat:@"Current state: '%@'", state]];
    }

    return nil;
}

- (void)handleExternalRequest:(OctagonStateTransitionRequest<CKKSResultOperation<OctagonStateTransitionOperationProtocol>*>*)request
                 startTimeout:(dispatch_time_t)timeout
{
    dispatch_sync(self.queue, ^{
        [self.stateMachineRequests addObject:request];
        [self _onqueuePokeStateMachine];

        if(timeout != 0 && timeout != DISPATCH_TIME_FOREVER) {
            WEAKIFY(self);
            dispatch_after(dispatch_time(DISPATCH_TIME_NOW, timeout), self.queue, ^{
                STRONGIFY(self);
                [request onqueueHandleStartTimeout:[self timeoutErrorForState:self.currentState]];
            });
        }
    });
}

- (void)registerStateTransitionWatcher:(OctagonStateTransitionWatcher*)watcher
                          startTimeout:(dispatch_time_t)timeout
{
    dispatch_sync(self.queue, ^{
        [self.stateMachineWatchers addObject: watcher];
        [self _onqueuePokeStateMachine];

        if(timeout != 0 && timeout != DISPATCH_TIME_FOREVER) {
            WEAKIFY(self);
            dispatch_after(dispatch_time(DISPATCH_TIME_NOW, timeout), self.queue, ^{
                STRONGIFY(self);
                [watcher onqueueHandleStartTimeout:[self timeoutErrorForState:self.currentState]];
            });
        }
    });
}

- (void)registerMultiStateArrivalWatcher:(OctagonStateMultiStateArrivalWatcher*)watcher
                            startTimeout:(dispatch_time_t)timeout
{
    dispatch_sync(self.queue, ^{
        [self _onqueueRegisterMultiStateArrivalWatcher:watcher startTimeout:timeout];
    });
}

- (void)_onqueueRegisterMultiStateArrivalWatcher:(OctagonStateMultiStateArrivalWatcher*)watcher
                                    startTimeout:(dispatch_time_t)timeout
{
    if([watcher.states containsObject:self.currentState]) {
        [watcher onqueueEnterState:self.currentState];
    } else {
        [self.stateMachineWatchers addObject:watcher];
        [self _onqueuePokeStateMachine];

        if(timeout != 0 && timeout != DISPATCH_TIME_FOREVER) {
            WEAKIFY(self);
            dispatch_after(dispatch_time(DISPATCH_TIME_NOW, timeout), self.queue, ^{
                STRONGIFY(self);
                [watcher onqueueHandleStartTimeout:[self timeoutErrorForState:self.currentState]];
            });
        }
    }
}

#pragma mark - RPC Helpers

- (void)doSimpleStateMachineRPC:(NSString*)name
                             op:(CKKSResultOperation<OctagonStateTransitionOperationProtocol>*)op
                   sourceStates:(NSSet<OctagonState*>*)sourceStates
                          reply:(nonnull void (^)(NSError * _Nullable))reply
{
    statemachinelog("state-rpc", "Beginning a '%@' rpc", name);

    if (self.lockStateTracker) {
        [self.lockStateTracker recheck];
    }
    OctagonStateTransitionRequest* request = [[OctagonStateTransitionRequest alloc] init:name
                                                                            sourceStates:sourceStates
                                                                             serialQueue:self.queue
                                                                            transitionOp:op];

#if TARGET_OS_TV
    [self handleExternalRequest:request
                   startTimeout:60*NSEC_PER_SEC];
#else
    [self handleExternalRequest:request
                   startTimeout:30*NSEC_PER_SEC];
#endif

    WEAKIFY(self);
    CKKSResultOperation* callback = [CKKSResultOperation named:[NSString stringWithFormat: @"%@-callback", name]
                                                     withBlock:^{
                                                         STRONGIFY(self);
                                                         statemachinelog("state-rpc", "Returning '%@' result: %@", name, op.error ?: @"no error");
                                                         reply(op.error);
                                                     }];
    [callback addDependency:op];
    [self.operationQueue addOperation: callback];
}

- (void)setWatcherTimeout:(uint64_t)timeout
{
    self.timeout = timeout;
}

- (CKKSResultOperation*)doWatchedStateMachineRPC:(NSString*)name
                                    sourceStates:(NSSet<OctagonState*>*)sourceStates
                                            path:(OctagonStateTransitionPath*)path
                                           reply:(nonnull void (^)(NSError * _Nullable error))reply
{
    CKKSResultOperation<OctagonStateTransitionOperationProtocol>* initialTransitionOp
        = [OctagonStateTransitionOperation named:[NSString stringWithFormat:@"intial-transition-%@", name]
                                        entering:path.initialState];

    return [self doWatchedStateMachineRPC:name
                             sourceStates:sourceStates
                                     path:path
                             transitionOp:initialTransitionOp
                                    reply:reply];
}

- (CKKSResultOperation*)doWatchedStateMachineRPC:(NSString*)name
                                    sourceStates:(NSSet<OctagonState*>*)sourceStates
                                            path:(OctagonStateTransitionPath*)path
                                    transitionOp:(CKKSResultOperation<OctagonStateTransitionOperationProtocol>*)initialTransitionOp
                                           reply:(nonnull void (^)(NSError * _Nullable error))reply
{
    statemachinelog("state-rpc", "Beginning a '%@' rpc", name);

    if (self.lockStateTracker) {
        [self.lockStateTracker recheck];
    }

    OctagonStateTransitionRequest* request = [[OctagonStateTransitionRequest alloc] init:name
                                                                            sourceStates:sourceStates
                                                                             serialQueue:self.queue
                                                                            transitionOp:initialTransitionOp];

    OctagonStateTransitionWatcher* watcher = [[OctagonStateTransitionWatcher alloc] initNamed:[NSString stringWithFormat:@"watcher-%@", name]
                                                                                  stateMachine:self
                                                                                         path:path
                                                                               initialRequest:request];
#if TARGET_OS_TV
    [self registerStateTransitionWatcher:watcher
                            startTimeout:self.timeout?:180*NSEC_PER_SEC];
#else
    [self registerStateTransitionWatcher:watcher
                            startTimeout:self.timeout?:120*NSEC_PER_SEC];
#endif

    CKKSResultOperation* replyOp = [CKKSResultOperation named:[NSString stringWithFormat: @"%@-callback", name]
                                          withBlockTakingSelf:^(CKKSResultOperation * _Nonnull op) {
        statemachinelog("state-rpc", "Returning '%@' result: %@", name, watcher.result.error ?: @"no error");
        if(reply) {
            reply(watcher.result.error);
        }
        op.error = watcher.result.error;
    }];

    [replyOp addDependency:watcher.result];
    [self.operationQueue addOperation:replyOp];

    [self handleExternalRequest:request
                   startTimeout:self.timeout?:120*NSEC_PER_SEC];
    return replyOp;
}

@end

#endif

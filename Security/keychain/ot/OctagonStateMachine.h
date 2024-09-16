
#if OCTAGON

#import <Foundation/Foundation.h>
#import "keychain/ckks/CKKSResultOperation.h"
#import "keychain/ckks/CKKSCondition.h"
#import "keychain/ckks/CKKSLockStateTracker.h"
#import "keychain/ckks/CKKSReachabilityTracker.h"

#import "keychain/ot/OctagonStateMachineHelpers.h"
#import "keychain/ot/OctagonStateMachineObservers.h"
#import "keychain/ot/OctagonFlags.h"
#import "keychain/ot/OctagonPendingFlag.h"

NS_ASSUME_NONNULL_BEGIN

@protocol OctagonStateOnqueuePendingFlagHandler
// Delivering this flag will require re-entering the state machine logic. So, it will necessarily not happen until you depart the current queue.
- (void)_onqueueHandlePendingFlagLater:(OctagonPendingFlag*)pendingFlag;
@end

// A State Machine Engine provides the actual implementation of a state machine.
// Its sole callback will be called on the queue passed into the OctagonStateMachine.
// Its inputs are the current state, and any interrupt flags that have been set on the state machine.

// The returned operation should not yet be started; the state machine will run it.
// Return nil if there's nothing to be done yet; the state machine will remain in the currentState until poked.
// The engine will be held weakly, so, ensure you keep it around.
@protocol OctagonStateMachineEngine
- (CKKSResultOperation<OctagonStateTransitionOperationProtocol>* _Nullable)_onqueueNextStateMachineTransition:(OctagonState*)currentState
                                                                                                        flags:(OctagonFlags*)flags
                                                                                                 pendingFlags:(id<OctagonStateOnqueuePendingFlagHandler>)pendingFlags;
@end

@protocol OctagonStateFlagHandler
- (void)handleFlag:(OctagonFlag*)flag;
- (void)handlePendingFlag:(OctagonPendingFlag*)pendingFlag;

// If you've truly broken your queue ordering, then call this from whatever queue your flag handler is using.
- (void)_onqueueHandleFlag:(OctagonFlag*)flag;
@end

@interface OctagonStateMachine : NSObject <OctagonStateFlagHandler, OctagonStateOnqueuePendingFlagHandler>
@property (readonly) OctagonState* currentState;
@property (readonly) dispatch_queue_t queue;

// The state machine transition function is the only location which should remove flags.
// Adding flags should use -handleFlag on the state machine
@property (readonly) id<OctagonFlagContainer> flags;

@property (readonly) NSDictionary<OctagonState*, CKKSCondition*>* stateConditions;

@property (readonly) CKKSCondition* paused;

// Note: the possibleStates map that you provide should use positive integers only in its NSNumbers.
// Negative numbers are reserved for internal OctagonStateMachine states (like OctagonStateMachineNotStarted
// and OctagonStateMachineHalted).
@property (readonly) NSSet<OctagonState*>* allowableStates;
@property (readonly) NSDictionary<OctagonState*, NSNumber*>* stateNumberMap;

@property (readonly) NSString* unexpectedStateErrorDomain;

@property (nonatomic) uint64_t timeout;

@property (readonly, nullable) CKKSLockStateTracker* lockStateTracker;
@property (readonly, nullable) CKKSReachabilityTracker* reachabilityTracker;

// If you don't pass a lock state tracker, then you cannot reasonably use OctagonPendingConditionsDeviceUnlocked
// If you don't pass a reachability tracker, then you cannot reasonably use OctagonPendingConditionsNetworkReachable

- (instancetype)initWithName:(NSString*)name
                      states:(NSDictionary<OctagonState*, NSNumber*>*)possibleStates
                       flags:(NSSet<OctagonFlag*>*)possibleFlags
                initialState:(OctagonState*)initialState
                       queue:(dispatch_queue_t)queue
                 stateEngine:(id<OctagonStateMachineEngine>)stateEngine
  unexpectedStateErrorDomain:(NSString*)unexpectedStateErrorDomain
            lockStateTracker:(CKKSLockStateTracker* _Nullable)lockStateTracker
         reachabilityTracker:(CKKSReachabilityTracker* _Nullable)reachabilityTracker;

- (void)startOperation;
- (void)haltOperation;

// If the state machine is paused, this will kick it to start up again. Otherwise, it is a no-op.
- (void)pokeStateMachine;
- (void)_onqueuePokeStateMachine;

// This will set the given flag, and ensure that the state machine spins to handle it.
- (void)handleFlag:(OctagonFlag*)flag;
- (void)_onqueueHandleFlag:(OctagonFlag*)flag;

// This will schedule the flag for future addition
- (void)handlePendingFlag:(OctagonPendingFlag *)pendingFlag;

- (NSDictionary<NSString*, NSString*>*)dumpPendingFlags;

- (void)handleExternalRequest:(OctagonStateTransitionRequest<CKKSResultOperation<OctagonStateTransitionOperationProtocol>*>*)request
                 startTimeout:(dispatch_time_t)timeout;

- (void)registerStateTransitionWatcher:(OctagonStateTransitionWatcher*)watcher
                          startTimeout:(dispatch_time_t)timeout;
- (void)registerMultiStateArrivalWatcher:(OctagonStateMultiStateArrivalWatcher*)watcher
                            startTimeout:(dispatch_time_t)timeout;
- (void)_onqueueRegisterMultiStateArrivalWatcher:(OctagonStateMultiStateArrivalWatcher*)watcher
                                    startTimeout:(dispatch_time_t)timeout;

- (void)doSimpleStateMachineRPC:(NSString*)name
                             op:(CKKSResultOperation<OctagonStateTransitionOperationProtocol>*)op
                   sourceStates:(NSSet<OctagonState*>*)sourceStates
                          reply:(nonnull void (^)(NSError * _Nullable))reply;

- (CKKSResultOperation*)doWatchedStateMachineRPC:(NSString*)name
                                    sourceStates:(NSSet<OctagonState*>*)sourceStates
                                            path:(OctagonStateTransitionPath*)path
                                           reply:(nonnull void (^)(NSError * _Nullable error))reply;

- (CKKSResultOperation*)doWatchedStateMachineRPC:(NSString*)name
                                    sourceStates:(NSSet<OctagonState*>*)sourceStates
                                            path:(OctagonStateTransitionPath*)path
                                    transitionOp:(CKKSResultOperation<OctagonStateTransitionOperationProtocol>*)initialTransitionOp
                                           reply:(nonnull void (^)(NSError * _Nullable error))reply;
- (void)setWatcherTimeout:(uint64_t)timeout;
- (BOOL)isPaused;

// Wait the state `wantedState' for `timeout' ns, if we transition though that state
// return that state, otherwise return a snapshot. No garantee that you still are in that state,
// if you want that, you need to run an RPC.
- (OctagonState* _Nonnull)waitForState:(OctagonState* _Nonnull)wantedState wait:(uint64_t)timeout;

@end

@interface OctagonStateMachine (Testing)
// For testing
- (NSArray<OctagonFlag*>*)possiblePendingFlags;
- (void)disablePendingFlags;

- (void)testPauseStateMachineAfterEntering:(OctagonState*)pauseState;
- (void)testReleaseStateMachinePause:(OctagonState*)pauseState;
@end

NS_ASSUME_NONNULL_END

#endif


#if OCTAGON

#import <Foundation/Foundation.h>
#import "keychain/ckks/CKKSResultOperation.h"
#import "keychain/ckks/CKKSCondition.h"
#import "keychain/ckks/CKKSLockStateTracker.h"

#import "keychain/ot/OctagonStateMachineHelpers.h"
#import "keychain/ot/OctagonStateMachineObservers.h"
#import "keychain/ot/OctagonFlags.h"
#import "keychain/ot/OctagonPendingFlag.h"

NS_ASSUME_NONNULL_BEGIN

@protocol OctagonStateOnqueuePendingFlagHandler
- (void)_onqueueHandlePendingFlag:(OctagonPendingFlag*)pendingFlag;
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
                                                                                                 pendingFlags:(id<OctagonStateOnqueuePendingFlagHandler>)pendingFlagHandler;
@end

@protocol OctagonStateFlagHandler
- (void)handleFlag:(OctagonFlag*)flag;
- (void)handlePendingFlag:(OctagonPendingFlag*)pendingFlag;
@end

@interface OctagonStateMachine : NSObject <OctagonStateFlagHandler, OctagonStateOnqueuePendingFlagHandler>
@property (readonly) OctagonState* currentState;

// The state machine transition function is the only location which should remove flags.
// Adding flags should use -handleFlag on the state machine
@property (readonly) id<OctagonFlagContainer> flags;

@property NSMutableDictionary<OctagonState*, CKKSCondition*>* stateConditions;
@property (readonly) CKKSCondition* paused;

@property (readonly) NSSet* allowableStates;
@property (nonatomic) uint64_t timeout;

@property (nullable) CKKSLockStateTracker* lockStateTracker;

// If you don't pass a lock state tracker, then you cannot reasonably use OctagonPendingConditionsDeviceUnlocked

- (instancetype)initWithName:(NSString*)name
                      states:(NSSet<OctagonState*>*)possibleStates
                initialState:(OctagonState*)initialState
                       queue:(dispatch_queue_t)queue
                 stateEngine:(id<OctagonStateMachineEngine>)stateEngine
            lockStateTracker:(CKKSLockStateTracker* _Nullable)lockStateTracker;

- (void)startOperation;
- (void)haltOperation;

// If the state machine is paused, this will kick it to start up again. Otherwise, it is a no-op.
- (void)pokeStateMachine;
- (void)_onqueuePokeStateMachine;

// This will set the given flag, and ensure that the state machine spins to handle it.
- (void)handleFlag:(OctagonFlag*)flag;

// This will schedule the flag for future addition
- (void)handlePendingFlag:(OctagonPendingFlag *)pendingFlag;

- (NSDictionary<NSString*, NSString*>*)dumpPendingFlags;
// For testing
- (NSArray<OctagonFlag*>*)possiblePendingFlags;
- (void)disablePendingFlags;

- (void)handleExternalRequest:(OctagonStateTransitionRequest<CKKSResultOperation<OctagonStateTransitionOperationProtocol>*>*)request;
- (void)registerStateTransitionWatcher:(OctagonStateTransitionWatcher*)watcher;

- (void)doSimpleStateMachineRPC:(NSString*)name
                             op:(CKKSResultOperation<OctagonStateTransitionOperationProtocol>*)op
                   sourceStates:(NSSet<OctagonState*>*)sourceStates
                          reply:(nonnull void (^)(NSError * _Nullable))reply;

- (void)doWatchedStateMachineRPC:(NSString*)name
                    sourceStates:(NSSet<OctagonState*>*)sourceStates
                            path:(OctagonStateTransitionPath*)path
                           reply:(nonnull void (^)(NSError *error))reply;
- (void)setWatcherTimeout:(uint64_t)timeout;
- (BOOL)isPaused;

// Wait the state `wantedState' for `timeout' ns, if we transition though that state
// return that state, otherwise return a snapshot. No garantee that you still are in that state,
// if you want that, you need to run an RPC.
- (OctagonState* _Nonnull)waitForState:(OctagonState* _Nonnull)wantedState wait:(uint64_t)timeout;

@end

NS_ASSUME_NONNULL_END

#endif

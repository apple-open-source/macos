
#if OCTAGON

#import <Foundation/Foundation.h>
#import "keychain/ckks/CKKSResultOperation.h"
#import "keychain/ckks/CKKSAnalytics.h"
#import "keychain/ot/OctagonStateMachineHelpers.h"

NS_ASSUME_NONNULL_BEGIN

@interface OctagonStateTransitionPathStep : NSObject
@property BOOL successState;
@property (readonly) NSDictionary<OctagonState*, OctagonStateTransitionPathStep*>* followStates;

- (instancetype)initAsSuccess;
- (instancetype)initWithPath:(NSDictionary<OctagonState*, OctagonStateTransitionPathStep*>*)followStates;

- (BOOL)successState;

+ (OctagonStateTransitionPathStep*)success;

// Dict should be a map of states to either:
//  1. A dictionary matching this specifiction
//  2. an OctagonStateTransitionPathStep object (which is likely a success object, but doesn't have to be)
// Any other object will be ignored. A malformed dictionary will be converted into an empty success path.
+ (OctagonStateTransitionPathStep*)pathFromDictionary:(NSDictionary<OctagonState*, id>*)pathDict;
@end


@interface OctagonStateTransitionPath : NSObject
@property OctagonState* initialState;
@property OctagonStateTransitionPathStep* pathStep;

- (instancetype)initWithState:(OctagonState*)initialState
                     pathStep:(OctagonStateTransitionPathStep*)pathSteps;

- (OctagonStateTransitionPathStep*)asPathStep;

// Uses the same rules as OctagonStateTransitionPathStep pathFromDictionary, but selects one of the top-level dictionary keys
// to be the path initialization state. Not well defined if you pass in two keys in the top-level dictionary.
// If the dictionary has no keys in it, returns nil.
+ (OctagonStateTransitionPath* _Nullable)pathFromDictionary:(NSDictionary<OctagonState*, id>*)pathDict;

@end



@protocol OctagonStateTransitionWatcherProtocol
@property (readonly) CKKSResultOperation* result;
- (void)onqueueHandleTransition:(CKKSResultOperation<OctagonStateTransitionOperationProtocol>*)attempt;
@end

@interface OctagonStateTransitionWatcher : NSObject <OctagonStateTransitionWatcherProtocol>
@property (readonly) NSString* name;
@property (readonly) CKKSResultOperation* result;
@property (readonly) OctagonStateTransitionPath* intendedPath;

// If the initial request times out, the watcher will fail as well.
- (instancetype)initNamed:(NSString*)name
              serialQueue:(dispatch_queue_t)queue
                     path:(OctagonStateTransitionPath*)path
           initialRequest:(OctagonStateTransitionRequest* _Nullable)initialRequest;

- (instancetype)timeout:(dispatch_time_t)timeout;

@end

// Reports on if any of the given states are entered
@interface OctagonStateMultiStateArrivalWatcher : NSObject <OctagonStateTransitionWatcherProtocol>
@property (readonly) NSString* name;
@property (readonly) CKKSResultOperation* result;
@property (readonly) NSSet<OctagonState*>* states;

- (instancetype)initNamed:(NSString*)name
              serialQueue:(dispatch_queue_t)queue
                   states:(NSSet<OctagonState*>*)states;

// Called by the state machine if it's already in a state at registration time
- (void)onqueueEnterState:(OctagonState*)state;

- (instancetype)timeout:(dispatch_time_t)timeout;
@end

NS_ASSUME_NONNULL_END

#endif // OCTAGON

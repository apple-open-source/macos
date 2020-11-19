#if OCTAGON

#import <Foundation/Foundation.h>
#import <CloudKit/CloudKit.h>

#import "keychain/ckks/CKKSCurrentKeyPointer.h"
#import "keychain/ckks/CKKSLockStateTracker.h"
#import "keychain/ckks/CKKSPeerProvider.h"
#import "keychain/ckks/CKKSProvideKeySetOperation.h"
#import "keychain/ckks/CKKSReachabilityTracker.h"
#import "keychain/ckks/CKKSNearFutureScheduler.h"
#import "keychain/ckks/CKKSSQLDatabaseObject.h"
#import "keychain/ckks/CKKSZoneModifier.h"
#import "keychain/analytics/CKKSLaunchSequence.h"
#import "keychain/ot/OctagonStateMachineHelpers.h"
#import "keychain/trust/TrustedPeers/TPSyncingPolicy.h"

NS_ASSUME_NONNULL_BEGIN

@interface CKKSOperationDependencies : NSObject

@property (readonly) CKRecordZoneID* zoneID;
@property (nullable) CKOperationGroup* ckoperationGroup;

@property (readonly) CKKSLaunchSequence* launch;
@property (readonly) id<OctagonStateFlagHandler> flagHandler;

@property (readonly) CKKSLockStateTracker* lockStateTracker;
@property (readonly) CKKSReachabilityTracker* reachabilityTracker;

// Due to CKKS's current operation scheduling model, these might be updated after object creation
// For example, if an operation is created and waiting to run, and trust arrives, CKKS will reach in
// and inject the new providers, possibly before the operation runs.
@property (atomic) NSArray<id<CKKSPeerProvider>>* peerProviders;

// Filled in after creation item creation
@property (nullable) TPSyncingPolicy* syncingPolicy;

// This is weak as, currently, the databaseProvider owns the CKKSOperationDependencies.
@property (readonly,weak) id<CKKSDatabaseProviderProtocol> databaseProvider;

@property CKKSZoneModifier* zoneModifier;

@property (readonly) CKKSNearFutureScheduler* notifyViewChangedScheduler;
@property (readonly) CKKSNearFutureScheduler* savedTLKNotifier;

// This might contain some key set provider operations. if you're an operation that knows about keysets, feel free to provide them.
@property NSHashTable<CKKSResultOperation<CKKSKeySetProviderOperationProtocol>*>* keysetProviderOperations;

- (instancetype)initWithZoneID:(CKRecordZoneID*)zoneID
                  zoneModifier:(CKKSZoneModifier*)zoneModifier
              ckoperationGroup:(CKOperationGroup* _Nullable)operationGroup
                   flagHandler:(id<OctagonStateFlagHandler>)flagHandler
                launchSequence:(CKKSLaunchSequence*)launchSequence
              lockStateTracker:(CKKSLockStateTracker*)lockStateTracker
           reachabilityTracker:(CKKSReachabilityTracker*)reachabilityTracker
                 peerProviders:(NSArray<id<CKKSPeerProvider>>*)peerProviders
              databaseProvider:(id<CKKSDatabaseProviderProtocol>)databaseProvider
    notifyViewChangedScheduler:(CKKSNearFutureScheduler*)notifyViewChangedScheduler
              savedTLKNotifier:(CKKSNearFutureScheduler*)savedTLKNotifier;

// Convenience method to fetch the trust states from all peer providers
// Do not call this while on the SQL transaction queue!
- (NSArray<CKKSPeerProviderState*>*)currentTrustStates;

- (void)provideKeySet:(CKKSCurrentKeySet*)keyset;

@end

NS_ASSUME_NONNULL_END

#endif // OCTAGON

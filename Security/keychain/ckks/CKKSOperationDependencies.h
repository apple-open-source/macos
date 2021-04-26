#if OCTAGON

#import <Foundation/Foundation.h>
#import <CloudKit/CloudKit.h>

#import "keychain/ckks/CKKSAccountStateTracker.h"
#import "keychain/ckks/CKKSCurrentKeyPointer.h"
#import "keychain/ckks/CKKSKeychainViewState.h"
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

@property (readonly) NSSet<CKKSKeychainViewState*>* zones;
@property (readonly) CKRecordZoneID* zoneID;

@property (nullable) CKOperationGroup* ckoperationGroup;
@property CKDatabase* ckdatabase;

@property (nullable) CKOperationGroup* currentOutgoingQueueOperationGroup;

@property (readonly) CKKSLaunchSequence* launch;
@property (readonly) id<OctagonStateFlagHandler> flagHandler;

@property (readonly) CKKSAccountStateTracker* accountStateTracker;
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

// Trigger this to request that the syncing policy be rechecked and reasserted.
@property (nullable) CKKSNearFutureScheduler* requestPolicyCheck;

// This might contain some key set provider operations. if you're an operation that knows about keysets, feel free to provide them.
@property NSHashTable<CKKSResultOperation<CKKSKeySetProviderOperationProtocol>*>* keysetProviderOperations;

- (instancetype)initWithViewState:(CKKSKeychainViewState*)viewState
                     zoneModifier:(CKKSZoneModifier*)zoneModifier
                       ckdatabase:(CKDatabase*)ckdatabase
                 ckoperationGroup:(CKOperationGroup* _Nullable)operationGroup
                      flagHandler:(id<OctagonStateFlagHandler>)flagHandler
                   launchSequence:(CKKSLaunchSequence*)launchSequence
              accountStateTracker:(CKKSAccountStateTracker*)accountStateTracker
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

// Helper Methods to change on-disk state
- (bool)intransactionCKRecordChanged:(CKRecord*)record resync:(bool)resync;
- (bool)intransactionCKRecordDeleted:(CKRecordID*)recordID recordType:(NSString*)recordType resync:(bool)resync;

// Call this if you've done a write and received an error. It'll pull out any new records returned as CKErrorServerRecordChanged and pretend we received them in a fetch
//
// Note that you need to tell this function the records you wanted to save, so it can determine which record failed from its CKRecordID.
// I don't know why CKRecordIDs don't have record types, either.
- (bool)intransactionCKWriteFailed:(NSError*)ckerror attemptedRecordsChanged:(NSDictionary<CKRecordID*, CKRecord*>*)savedRecords;

@end

NS_ASSUME_NONNULL_END

#endif // OCTAGON

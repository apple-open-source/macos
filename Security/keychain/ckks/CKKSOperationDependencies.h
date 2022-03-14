#if OCTAGON

#import <Foundation/Foundation.h>
#import <CloudKit/CloudKit.h>

#import "keychain/ckks/CKKSAccountStateTracker.h"
#import "keychain/ckks/CKKSCurrentKeyPointer.h"
#import "keychain/ckks/CKKSCloudKitClassDependencies.h"
#import "keychain/ckks/CKKSFetchAllRecordZoneChangesOperation.h"
#import "keychain/ckks/CKKSKeychainViewState.h"
#import "keychain/ckks/CKKSLockStateTracker.h"
#import "keychain/ckks/CKKSPeerProvider.h"
#import "keychain/ckks/CKKSProvideKeySetOperation.h"
#import "keychain/ckks/CKKSReachabilityTracker.h"
#import "keychain/ckks/CKKSNearFutureScheduler.h"
#import "keychain/ckks/CKKSSQLDatabaseObject.h"
#import "keychain/ckks/CKKSZoneModifier.h"
#import "keychain/ot/OctagonStateMachineHelpers.h"
#import "keychain/trust/TrustedPeers/TPSyncingPolicy.h"

NS_ASSUME_NONNULL_BEGIN

@interface CKKSOperationDependencies : NSObject

// activeManagedViews contains the set of views that CKKS currently manages and will use for syncing
@property (readonly) NSSet<CKKSKeychainViewState*>* activeManagedViews;

// views contains the set of all views+CK zones that CKKS is actively operating on
@property (readonly) NSSet<CKKSKeychainViewState*>* views;

// allCKKSManagedViews contains every view that CKKS currently manages and will use for syncing,
// whether or not they're currently considered active
@property (readonly) NSSet<CKKSKeychainViewState*>* allCKKSManagedViews;

// allExternalViews contains every view that CKKS currently manages on behalf of external clients.
// CKKS should not try to write these views, or to parse their keys.
@property (readonly) NSSet<CKKSKeychainViewState*>* allExternalManagedViews;

// allPriorityViews contains every view that the current policy claims is Priority
@property (readonly) NSSet<CKKSKeychainViewState*>* allPriorityViews;

// allViews contains every view+CKZone that CKKS currently knows about
@property (readonly) NSSet<CKKSKeychainViewState*>* allViews;


@property (readonly) NSMutableSet<CKKSFetchBecause*>* currentFetchReasons;
@property (nullable) CKOperationGroup* ckoperationGroup;
@property CKDatabase* ckdatabase;

@property CKKSCloudKitClassDependencies* cloudKitClassDependencies;

@property (nullable) CKOperationGroup* currentOutgoingQueueOperationGroup;

@property (readonly) id<OctagonStateFlagHandler> flagHandler;

@property (readonly) CKKSAccountStateTracker* accountStateTracker;
@property (readonly) CKKSLockStateTracker* lockStateTracker;
@property (readonly) CKKSReachabilityTracker* reachabilityTracker;

// Due to CKKS's current operation scheduling model, these might be updated after object creation
// For example, if an operation is created and waiting to run, and trust arrives, CKKS will reach in
// and inject the new providers, possibly before the operation runs.
@property (atomic) NSArray<id<CKKSPeerProvider>>* peerProviders;

// Filled in after creation item creation
@property (nullable, readonly) TPSyncingPolicy* syncingPolicy;

@property (readonly) id<CKKSDatabaseProviderProtocol> databaseProvider;

@property CKKSZoneModifier* zoneModifier;

@property (readonly) CKKSNearFutureScheduler* savedTLKNotifier;

// Trigger this to request that the syncing policy be rechecked and reasserted.
@property (nullable) CKKSNearFutureScheduler* requestPolicyCheck;

// This might contain some key set provider operations. if you're an operation that knows about keysets, feel free to provide them.
@property NSHashTable<CKKSResultOperation<CKKSKeySetProviderOperationProtocol>*>* keysetProviderOperations;

- (instancetype)initWithViewStates:(NSSet<CKKSKeychainViewState*>*)viewStates
                      zoneModifier:(CKKSZoneModifier*)zoneModifier
                        ckdatabase:(CKDatabase*)ckdatabase
         cloudKitClassDependencies:(CKKSCloudKitClassDependencies*)cloudKitClassDependencies
                  ckoperationGroup:(CKOperationGroup* _Nullable)operationGroup
                       flagHandler:(id<OctagonStateFlagHandler>)flagHandler
               accountStateTracker:(CKKSAccountStateTracker*)accountStateTracker
                  lockStateTracker:(CKKSLockStateTracker*)lockStateTracker
               reachabilityTracker:(CKKSReachabilityTracker*)reachabilityTracker
                     peerProviders:(NSArray<id<CKKSPeerProvider>>*)peerProviders
                  databaseProvider:(id<CKKSDatabaseProviderProtocol>)databaseProvider
                  savedTLKNotifier:(CKKSNearFutureScheduler*)savedTLKNotifier;

// Convenience method to fetch the trust states from all peer providers
// Do not call this while on the SQL transaction queue!
- (NSArray<CKKSPeerProviderState*>*)currentTrustStates;

// Convenience method to look across a set off trust states and determine if all of the essential states have trust
- (BOOL)considerSelfTrusted:(NSArray<CKKSPeerProviderState*>*)states error:(NSError**)error;

- (void)provideKeySets:(NSDictionary<CKRecordZoneID*, CKKSCurrentKeySet*>*)keysets;
- (NSString* _Nullable)viewNameForItem:(SecDbItemRef)item;

// Helper Methods to change on-disk state
- (bool)intransactionCKRecordChanged:(CKRecord*)record resync:(bool)resync;
- (bool)intransactionCKRecordDeleted:(CKRecordID*)recordID recordType:(NSString*)recordType resync:(bool)resync;

// Helper methods to configure the views property
- (void)operateOnSelectViews:(NSSet<CKKSKeychainViewState*>*)views;
- (void)operateOnAllViews;

- (void)limitOperationToPriorityViews;

- (void)setStateForActiveZones:(CKKSZoneKeyState*)newZoneKeyState;
- (void)setStateForActiveCKKSManagedViews:(CKKSZoneKeyState*)newZoneKeyState;
- (void)setStateForActiveExternallyManagedViews:(CKKSZoneKeyState*)newZoneKeyState;
- (void)setStateForAllViews:(CKKSZoneKeyState*)newZoneKeyState;

// Calling this will update the syncing policy, and also remove any active zone filter previously applied
- (void)applyNewSyncingPolicy:(TPSyncingPolicy*)policy
                   viewStates:(NSSet<CKKSKeychainViewState*>*)viewStates;

- (NSSet<CKKSKeychainViewState*>*)viewsInState:(CKKSZoneKeyState*)state;
- (NSSet<CKKSKeychainViewState*>*)viewStatesByNames:(NSSet<NSString*>*)names;
- (CKKSKeychainViewState* _Nullable)viewStateForName:(NSString*)name;

// Returns the set of views that are both Enabled by the configured policy, and in Key State Ready.
- (NSSet<CKKSKeychainViewState*>*)readyAndSyncingViews;

// Call this if you've done a write and received an error. It'll pull out any new records returned as CKErrorServerRecordChanged and pretend we received them in a fetch
//
// Note that you need to tell this function the records you wanted to save, so it can determine which record failed from its CKRecordID.
// I don't know why CKRecordIDs don't have record types, either.
- (bool)intransactionCKWriteFailed:(NSError*)ckerror attemptedRecordsChanged:(NSDictionary<CKRecordID*, CKRecord*>*)savedRecords;

@end

NS_ASSUME_NONNULL_END

#endif // OCTAGON


#if OCTAGON

#import <Foundation/Foundation.h>
#import "keychain/ckks/CKKSGroupOperation.h"
#import "keychain/ot/OctagonStateMachine.h"

NS_ASSUME_NONNULL_BEGIN

typedef OctagonState CKKSState;

NSSet<CKKSState*>* CKKSAllStates(void);

extern CKKSState* const CKKSStateWaitForCloudKitAccountStatus;

// CKKS is currently logged out
extern CKKSState* const CKKSStateLoggedOut;

extern CKKSState* const CKKSStateWaitForTrust;
extern CKKSState* const CKKSStateLoseTrust;

extern CKKSState* const CKKSStateInitializing;
extern CKKSState* const CKKSStateInitialized;
extern CKKSState* const CKKSStateZoneCreationFailed;

extern CKKSState* const CKKSStateFixupRefetchCurrentItemPointers;
extern CKKSState* const CKKSStateFixupFetchTLKShares;
extern CKKSState* const CKKSStateFixupLocalReload;
extern CKKSState* const CKKSStateFixupResaveDeviceStateEntries;
extern CKKSState* const CKKSStateFixupDeleteAllCKKSTombstones;

extern CKKSState* const CKKSStateBeginFetch;
extern CKKSState* const CKKSStateFetch;
extern CKKSState* const CKKSStateFetchComplete;
extern CKKSState* const CKKSStateNeedFullRefetch;

extern CKKSState* const CKKSStateProcessReceivedKeys;

extern CKKSState* const CKKSStateCheckZoneHierarchies;

extern CKKSState* const CKKSStateProvideKeyHierarchy;
extern CKKSState* const CKKSStateProvideKeyHierarchyUntrusted;

extern CKKSState* const CKKSStateHealTLKShares;
extern CKKSState* const CKKSStateHealTLKSharesFailed;


// States to handle individual zones misbehaving
extern CKKSState* const CKKSStateTLKMissing;

extern CKKSState* const CKKSStateUnhealthy;

extern CKKSState* const CKKSStateResettingZone;
extern CKKSState* const CKKSStateResettingLocalData;

extern CKKSState* const CKKSStateBecomeReady;
extern CKKSState* const CKKSStateReady;

extern CKKSState* const CKKSStateProcessIncomingQueue;
extern CKKSState* const CKKSStateRemainingClassAIncomingItems;

extern CKKSState* const CKKSStateScanLocalItems;
extern CKKSState* const CKKSStateReencryptOutgoingItems;
extern CKKSState* const CKKSStateProcessOutgoingQueue;
extern CKKSState* const CKKSStateOutgoingQueueOperationFailed;

extern CKKSState* const CKKSStateExpandToHandleAllViews;

// Fatal error. Will not proceed unless fixed from outside class.
extern CKKSState* const CKKSStateError;

// --------------------------------
// Flag initialization
typedef OctagonFlag CKKSFlag;

// The set of trusted peers has changed
extern CKKSFlag* const CKKSFlagTrustedPeersSetChanged;

extern CKKSFlag* const CKKSFlagCloudKitLoggedIn;
extern CKKSFlag* const CKKSFlagCloudKitLoggedOut;

extern CKKSFlag* const CKKSFlagBeginTrustedOperation;
extern CKKSFlag* const CKKSFlagEndTrustedOperation;

extern CKKSFlag* const CKKSFlagChangeTokenExpired;
extern CKKSFlag* const CKKSFlagCloudKitZoneMissing;

extern CKKSFlag* const CKKSFlagDeviceUnlocked;

extern CKKSFlag* const CKKSFlagFetchRequested;
// Added when a key hierarchy fetch completes.
extern CKKSFlag* const CKKSFlagFetchComplete;

extern CKKSFlag* const CKKSFlagKeyStateProcessRequested;
extern CKKSFlag* const CKKSFlagKeySetRequested;

extern CKKSFlag* const CKKSFlagCheckQueues;
extern CKKSFlag* const CKKSFlagProcessIncomingQueueWithFreshPolicy;

extern CKKSFlag* const CKKSFlagProcessIncomingQueue;
extern CKKSFlag* const CKKSFlagProcessOutgoingQueue;
extern CKKSFlag* const CKKSFlagScanLocalItems;
extern CKKSFlag* const CKKSFlagItemReencryptionNeeded;

// Used to rate-limit CK writes
extern CKKSFlag* const CKKSFlagOutgoingQueueOperationRateToken;

extern CKKSFlag* const CKKSFlagNewPriorityViews;

extern CKKSFlag* const CKKSFlag24hrNotification;

NSSet<CKKSFlag*>* CKKSAllStateFlags(void);

NS_ASSUME_NONNULL_END

#endif

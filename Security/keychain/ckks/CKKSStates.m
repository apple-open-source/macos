
#if OCTAGON

#import "keychain/ckks/CKKSStates.h"
#import "keychain/ckks/CKKSKeychainView.h"
#import "keychain/ot/ObjCImprovements.h"

NSString* const CKKSStateTransitionErrorDomain = @"com.appple.ckks.state";

CKKSState* const CKKSStateLoggedOut = (CKKSState*) @"loggedout";
CKKSState* const CKKSStateWaitForCloudKitAccountStatus = (CKKSState*)@"wait_for_ck_account_status";

CKKSState* const CKKSStateLoseTrust = (CKKSState*) @"lose_trust";
CKKSState* const CKKSStateWaitForTrust = (CKKSState*) @"waitfortrust";

CKKSState* const CKKSStateInitializing = (CKKSState*) @"initializing";
CKKSState* const CKKSStateInitialized = (CKKSState*) @"initialized";
CKKSState* const CKKSStateZoneCreationFailed = (CKKSState*) @"zonecreationfailed";

CKKSState* const CKKSStateFixupRefetchCurrentItemPointers = (CKKSState*) @"fixup_fetch_cip";
CKKSState* const CKKSStateFixupFetchTLKShares = (CKKSState*) @"fixup_fetch_tlkshares";
CKKSState* const CKKSStateFixupLocalReload = (CKKSState*) @"fixup_local_reload";
CKKSState* const CKKSStateFixupResaveDeviceStateEntries = (CKKSState*) @"fixup_resave_cdse";
CKKSState* const CKKSStateFixupDeleteAllCKKSTombstones = (CKKSState*) @"fixup_delete_tombstones";

CKKSState* const CKKSStateBeginFetch = (CKKSState*) @"begin_fetch";
CKKSState* const CKKSStateFetch = (CKKSState*) @"fetching";
CKKSState* const CKKSStateFetchComplete = (CKKSState*) @"fetchcomplete";
CKKSState* const CKKSStateNeedFullRefetch = (CKKSState*) @"needrefetch";

CKKSState* const CKKSStateProcessReceivedKeys = (CKKSState*) @"process_key_hierarchy";
CKKSState* const CKKSStateCheckZoneHierarchies = (CKKSState*)@"check_zone_hierarchies";

CKKSState* const CKKSStateProvideKeyHierarchy = (CKKSState*)@"provide_key_hieararchy";
CKKSState* const CKKSStateProvideKeyHierarchyUntrusted = (CKKSState*)@"provide_key_hieararchy_untrusted";

CKKSState* const CKKSStateHealTLKShares = (CKKSState*) @"heal_tlk_shares";
CKKSState* const CKKSStateHealTLKSharesFailed = (CKKSState*) @"healtlksharesfailed";

CKKSState* const CKKSStateTLKMissing = (CKKSState*) @"tlkmissing";
CKKSState* const CKKSStateUnhealthy = (CKKSState*) @"unhealthy";

CKKSState* const CKKSStateResettingZone = (CKKSState*) @"resetzone";
CKKSState* const CKKSStateResettingLocalData = (CKKSState*) @"resetlocal";

CKKSState* const CKKSStateReady = (CKKSState*) @"ready";
CKKSState* const CKKSStateBecomeReady = (CKKSState*) @"become_ready";
CKKSState* const CKKSStateError = (CKKSState*) @"error";


CKKSState* const CKKSStateProcessIncomingQueue = (CKKSState*) @"process_incoming_queue";
CKKSState* const CKKSStateRemainingClassAIncomingItems = (CKKSState*) @"class_a_incoming_items_remaining";

CKKSState* const CKKSStateScanLocalItems = (CKKSState*) @"scan_local_items";
CKKSState* const CKKSStateReencryptOutgoingItems = (CKKSState*) @"reencrypt_outgoing_items";

CKKSState* const CKKSStateProcessOutgoingQueue = (CKKSState*) @"process_outgoing_queue";
CKKSState* const CKKSStateOutgoingQueueOperationFailed = (CKKSState*) @"process_outgoing_queue_failed";

CKKSState* const CKKSStateExpandToHandleAllViews = (CKKSState*)@"handle_all_views";

NSDictionary<CKKSState*, NSNumber*>* CKKSStateMap(void)
{
    static NSDictionary<CKKSState*, NSNumber*>* stateMap = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{

        stateMap = @{
            CKKSStateReady                           : @0,
            CKKSStateError                           : @1,

            CKKSStateInitializing                    : @2,
            CKKSStateInitialized                     : @3,
            CKKSStateFetchComplete                   : @4,
            CKKSStateUnhealthy                       : @5,
            CKKSStateNeedFullRefetch                 : @6,
            CKKSStateFetch                           : @7,
            CKKSStateResettingZone                   : @8,
            CKKSStateResettingLocalData              : @9,
            CKKSStateLoggedOut                       : @10,
            CKKSStateZoneCreationFailed              : @11,
            CKKSStateWaitForTrust                    : @12,

            CKKSStateProcessReceivedKeys             : @13,
            CKKSStateCheckZoneHierarchies            : @14,
            CKKSStateBecomeReady                     : @15,
            CKKSStateLoseTrust                       : @16,
            CKKSStateTLKMissing                      : @17,
            CKKSStateWaitForCloudKitAccountStatus    : @18,
            CKKSStateBeginFetch                      : @19,

            CKKSStateFixupRefetchCurrentItemPointers : @20,
            CKKSStateFixupFetchTLKShares             : @21,
            CKKSStateFixupLocalReload                : @22,
            CKKSStateFixupResaveDeviceStateEntries   : @23,
            CKKSStateFixupDeleteAllCKKSTombstones    : @24,

            CKKSStateHealTLKShares                   : @25,
            CKKSStateHealTLKSharesFailed             : @26,

            CKKSStateProvideKeyHierarchy             : @27,
            CKKSStateProvideKeyHierarchyUntrusted    : @28,

            CKKSStateProcessIncomingQueue            : @29,
            CKKSStateRemainingClassAIncomingItems    : @30,
            CKKSStateScanLocalItems                  : @31,
            CKKSStateReencryptOutgoingItems          : @32,
            CKKSStateProcessOutgoingQueue            : @33,
            CKKSStateOutgoingQueueOperationFailed    : @34,

            CKKSStateExpandToHandleAllViews          : @35,
        };
    });
    return stateMap;
}

NSSet<CKKSState*>* CKKSAllStates(void)
{
    static NSSet<CKKSState*>* set = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        set = [NSSet setWithArray:[CKKSStateMap() allKeys]];
    });
    return set;
}

CKKSFlag* const CKKSFlagTrustedPeersSetChanged = (CKKSFlag*) @"trusted_peers_changed";

CKKSFlag* const CKKSFlagCloudKitLoggedIn = (CKKSFlag*)@"ck_account_logged_in";
CKKSFlag* const CKKSFlagCloudKitLoggedOut = (CKKSFlag*)@"ck_account_logged_out";

CKKSFlag* const CKKSFlagBeginTrustedOperation = (CKKSFlag*)@"trusted_operation_begin";
CKKSFlag* const CKKSFlagEndTrustedOperation = (CKKSFlag*)@"trusted_operation_end";

CKKSFlag* const CKKSFlagChangeTokenExpired = (CKKSFlag*)@"ck_change_token_expired";
CKKSFlag* const CKKSFlagCloudKitZoneMissing = (CKKSFlag*)@"ck_zone_missing";

CKKSFlag* const CKKSFlagDeviceUnlocked = (CKKSFlag*)@"device_unlocked";

CKKSFlag* const CKKSFlagFetchRequested = (CKKSFlag*) @"fetch_requested";
CKKSFlag* const CKKSFlagFetchComplete = (CKKSFlag*)@"fetch_complete";

CKKSFlag* const CKKSFlagKeyStateProcessRequested = (CKKSFlag*) @"key_process_requested";

CKKSFlag* const CKKSFlagKeySetRequested = (CKKSFlag*) @"key_set";

CKKSFlag* const CKKSFlagProcessIncomingQueue = (CKKSFlag*)@"process_incoming_queue";
CKKSFlag* const CKKSFlagProcessOutgoingQueue = (CKKSFlag*)@"process_outgoing_queue";
CKKSFlag* const CKKSFlagScanLocalItems = (CKKSFlag*)@"dropped_items";
CKKSFlag* const CKKSFlagItemReencryptionNeeded = (CKKSFlag*)@"item_reencryption_needed";

CKKSFlag* const CKKSFlag24hrNotification = (CKKSFlag*)@"24_hr_notification";

CKKSFlag* const CKKSFlagCheckQueues = (CKKSFlag*) @"check_queues";
CKKSFlag* const CKKSFlagProcessIncomingQueueWithFreshPolicy = (CKKSFlag*) @"policy_fresh";
CKKSFlag* const CKKSFlagOutgoingQueueOperationRateToken = (CKKSFlag*) @"oqo_token";

CKKSFlag* const CKKSFlagNewPriorityViews = (CKKSFlag*)@"new_priority_views";

NSSet<CKKSFlag*>* CKKSAllStateFlags(void)
{
    static NSSet<CKKSFlag*>* s = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        s = [NSSet setWithArray:@[
            CKKSFlagFetchRequested,
            CKKSFlagKeyStateProcessRequested,
            CKKSFlagTrustedPeersSetChanged,
            CKKSFlagScanLocalItems,
            CKKSFlagCloudKitLoggedIn,
            CKKSFlagCloudKitLoggedOut,
            CKKSFlagCloudKitZoneMissing,
            CKKSFlagChangeTokenExpired,
            CKKSFlagProcessIncomingQueue,
            CKKSFlagProcessOutgoingQueue,
            CKKSFlagItemReencryptionNeeded,
            CKKSFlagBeginTrustedOperation,
            CKKSFlagEndTrustedOperation,
            CKKSFlagDeviceUnlocked,
            CKKSFlagFetchComplete,
            CKKSFlag24hrNotification,
            CKKSFlagKeySetRequested,
            CKKSFlagCheckQueues,
            CKKSFlagProcessIncomingQueueWithFreshPolicy,
            CKKSFlagOutgoingQueueOperationRateToken,
            CKKSFlagNewPriorityViews,
        ]];
    });
    return s;
}

#endif

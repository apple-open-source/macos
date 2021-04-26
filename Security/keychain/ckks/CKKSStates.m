
#if OCTAGON

#import "keychain/ckks/CKKSStates.h"
#import "keychain/ckks/CKKSKeychainView.h"
#import "keychain/ot/ObjCImprovements.h"

CKKSFlag* const CKKSFlagTrustedPeersSetChanged = (CKKSFlag*) @"trusted_peers_changed";

CKKSFlag* const CKKSFlagTLKCreationRequested = (CKKSFlag*)@"tlk_creation";
CKKSFlag* const CKKSFlagKeyStateTLKsUploaded = (CKKSFlag*)@"tlks_uploaded";

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

NSSet<CKKSFlag*>* CKKSAllStateFlags(void)
{
    static NSSet<CKKSFlag*>* s = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        s = [NSSet setWithArray:@[
            CKKSFlagFetchRequested,
            CKKSFlagKeyStateProcessRequested,
            CKKSFlagTrustedPeersSetChanged,
            CKKSFlagTLKCreationRequested,
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
            CKKSFlagKeyStateTLKsUploaded,
            CKKSFlagFetchComplete,
            CKKSFlag24hrNotification,
            CKKSFlagKeySetRequested,
        ]];
    });
    return s;
}

#endif

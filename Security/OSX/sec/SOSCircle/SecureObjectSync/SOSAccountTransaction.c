//
//  SOSAccountTransaction.c
//  sec
//
//

#include "SOSAccountTransaction.h"

#include <utilities/SecCFWrappers.h>
#include <CoreFoundation/CoreFoundation.h>

#include <Security/SecureObjectSync/SOSAccount.h>
#include <Security/SecureObjectSync/SOSAccountPriv.h>
#include <Security/SecureObjectSync/SOSPeerInfoV2.h>
#include <Security/SecureObjectSync/SOSTransport.h>
#include <Security/SecureObjectSync/SOSTransportCircle.h>

#define kPublicKeyNotAvailable "com.apple.security.publickeynotavailable"

CFGiblisFor(SOSAccountTransaction);

static void SOSAccountTransactionDestroy(CFTypeRef aObj) {
    SOSAccountTransactionRef at = (SOSAccountTransactionRef) aObj;
    
	CFReleaseNull(at->initialUnsyncedViews);
    CFReleaseNull(at->initialID);
    CFReleaseNull(at->account);
    CFReleaseNull(at->initialViews);
    CFReleaseNull(at->initialKeyParameters);
    CFReleaseNull(at->peersToRequestSync);
}

static CFStringRef SOSAccountTransactionCopyFormatDescription(CFTypeRef aObj, CFDictionaryRef formatOptions) {
    SOSAccountTransactionRef at = (SOSAccountTransactionRef) aObj;
    
    CFMutableStringRef description = CFStringCreateMutable(kCFAllocatorDefault, 0);
    
    CFStringAppendFormat(description, NULL, CFSTR("<SOSAccountTransactionRef@%p %ld>"),
                         at, at->initialViews ? CFSetGetCount(at->initialViews) : 0);
    
    return description;
}

static void SOSAccountTransactionRestart(SOSAccountTransactionRef txn) {
    txn->initialInCircle = SOSAccountIsInCircle(txn->account, NULL);

    if(txn->account)
        txn->initialTrusted = (txn->account)->user_public_trusted;

    if (txn->initialInCircle) {
        SOSAccountEnsureSyncChecking(txn->account);
    }

    CFAssignRetained(txn->initialUnsyncedViews, SOSAccountCopyOutstandingViews(txn->account));

    CFReleaseNull(txn->initialKeyParameters);
    
    if(txn->account && txn->account->user_key_parameters){
        CFReleaseNull(txn->initialKeyParameters);
        txn->initialKeyParameters  = CFDataCreateCopy(kCFAllocatorDefault, txn->account->user_key_parameters);
    }
    SOSPeerInfoRef mpi = SOSAccountGetMyPeerInfo(txn->account);
    CFAssignRetained(txn->initialViews, mpi ? SOSPeerInfoCopyEnabledViews(mpi) : NULL);

    CFRetainAssign(txn->initialID, SOSPeerInfoGetPeerID(mpi));

    CFReleaseNull(txn->peersToRequestSync);
    
    CFStringSetPerformWithDescription(txn->initialViews, ^(CFStringRef description) {
        secnotice("acct-txn", "Starting as:%s v:%@", txn->initialInCircle ? "member" : "non-member", description);
    });
}


SOSAccountTransactionRef SOSAccountTransactionCreate(SOSAccountRef account) {
    SOSAccountTransactionRef at = CFTypeAllocate(SOSAccountTransaction, struct __OpaqueSOSAccountTransaction, kCFAllocatorDefault);
    
    at->account = CFRetainSafe(account);

    at->initialInCircle = false;
    at->initialViews = NULL;
    at->initialKeyParameters = NULL;
    at->initialTrusted = false;
    at->initialUnsyncedViews = NULL;
    at->initialID = NULL;
    at->peersToRequestSync = NULL;

    SOSAccountTransactionRestart(at);

    return at;
}

#define ACCOUNT_STATE_INTERVAL 20

void SOSAccountTransactionFinish(SOSAccountTransactionRef txn) {
    CFErrorRef localError = NULL;
    bool notifyEngines = false;
    static int do_account_state_at_zero = 0;

    SOSPeerInfoRef mpi = SOSAccountGetMyPeerInfo(txn->account);

    bool isInCircle = SOSAccountIsInCircle(txn->account, NULL);

    if (isInCircle && txn->peersToRequestSync) {
        SOSCCRequestSyncWithPeers(txn->peersToRequestSync);
    }
    CFReleaseNull(txn->peersToRequestSync);

    if (isInCircle) {
        SOSAccountEnsureSyncChecking(txn->account);
    } else {
        SOSAccountCancelSyncChecking(txn->account);
    }

    // If our identity changed our inital set should be everything.
    if (!CFEqualSafe(txn->initialID, SOSPeerInfoGetPeerID(mpi))) {
        CFAssignRetained(txn->initialUnsyncedViews, SOSViewCopyViewSet(kViewSetAll));
    }

    CFSetRef finalUnsyncedViews = SOSAccountCopyOutstandingViews(txn->account);
    if (!CFEqualSafe(txn->initialUnsyncedViews, finalUnsyncedViews)) {
        if (SOSAccountHandleOutOfSyncUpdate(txn->account, txn->initialUnsyncedViews, finalUnsyncedViews)) {
            notifyEngines = true;
        }

        CFStringSetPerformWithDescription(txn->initialUnsyncedViews, ^(CFStringRef newUnsyncedDescripion) {
            CFStringSetPerformWithDescription(finalUnsyncedViews, ^(CFStringRef unsyncedDescription) {
                secnotice("initial-sync", "Unsynced was: %@", unsyncedDescription);
                secnotice("initial-sync", "Unsynced is: %@", newUnsyncedDescripion);
            });
        });
    }
    CFReleaseNull(finalUnsyncedViews);

    if (txn->account->engine_peer_state_needs_repair) {
        // We currently only get here from a failed syncwithallpeers, so
        // that will retry. If this logic changes, force a syncwithallpeers
        if (!SOSAccountEnsurePeerRegistration(txn->account, &localError)) {
            secerror("Ensure peer registration while repairing failed: %@", localError);
        }
        CFReleaseNull(localError);
        
        notifyEngines = true;
    }

    if(txn->account->circle_rings_retirements_need_attention){
        SOSAccountRecordRetiredPeersInCircle(txn->account);

        SOSAccountEnsureRecoveryRing(txn->account);
        SOSAccountEnsureInBackupRings(txn->account);

        CFErrorRef localError = NULL;
        if(!SOSTransportCircleFlushChanges(txn->account->circle_transport, &localError)) {
            secerror("flush circle failed %@", localError);
        }
        CFReleaseSafe(localError);
        
        notifyEngines = true;
    }

    if (notifyEngines) {
        SOSAccountNotifyEngines(txn->account);
    }
    
    if(txn->account->key_interests_need_updating){
        SOSUpdateKeyInterest(txn->account);
    }

    txn->account->key_interests_need_updating = false;
    txn->account->circle_rings_retirements_need_attention = false;
    txn->account->engine_peer_state_needs_repair = false;

    SOSAccountFlattenToSaveBlock(txn->account);
    
    // Refresh isInCircle since we could have changed our mind
    isInCircle = SOSAccountIsInCircle(txn->account, NULL);
    
    mpi = SOSAccountGetMyPeerInfo(txn->account);
    CFSetRef views = mpi ? SOSPeerInfoCopyEnabledViews(mpi) : NULL;

    CFStringSetPerformWithDescription(views, ^(CFStringRef description) {
        secnotice("acct-txn", "Finished as:%s v:%@", isInCircle ? "member" : "non-member", description);
    });
    if(!CFEqualSafe(txn->initialViews, views) || txn->initialInCircle != isInCircle) {
        notify_post(kSOSCCViewMembershipChangedNotification);
        do_account_state_at_zero = 0;
    }
    
    if((txn->initialTrusted != (txn->account)->user_public_trusted) || (!CFEqualSafe(txn->initialKeyParameters, txn->account->user_key_parameters))){
        notify_post(kPublicKeyNotAvailable);
        do_account_state_at_zero = 0;
    }
    
    if(do_account_state_at_zero <= 0) {
        SOSAccountLogState(txn->account);
        SOSAccountLogViewState(txn->account);
        do_account_state_at_zero = ACCOUNT_STATE_INTERVAL;
    }
    do_account_state_at_zero--;
    
    CFReleaseNull(views);

}

void SOSAccountTransactionFinishAndRestart(SOSAccountTransactionRef txn) {
    SOSAccountTransactionFinish(txn);
    SOSAccountTransactionRestart(txn);
}

void SOSAccountTransactionAddSyncRequestForPeerID(SOSAccountTransactionRef txn, CFStringRef peerID) {
    if (!txn->peersToRequestSync) {
        txn->peersToRequestSync = CFSetCreateMutableForCFTypes(kCFAllocatorDefault);
    }

    CFSetAddValue(txn->peersToRequestSync, peerID);
}

void SOSAccountTransactionAddSyncRequestForAllPeerIDs(SOSAccountTransactionRef txn, CFSetRef /* CFStringRef */ peerIDs) {
    if (!txn->peersToRequestSync) {
        txn->peersToRequestSync = CFSetCreateMutableForCFTypes(kCFAllocatorDefault);
    }

    CFSetUnion(txn->peersToRequestSync, peerIDs);
}



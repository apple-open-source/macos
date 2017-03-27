//
//  SOSAccountRingUpdate.c
//  sec
//
//

#include <stdio.h>

#include "SOSAccountPriv.h"
#include <Security/SecureObjectSync/SOSTransportCircle.h>
#include <Security/SecureObjectSync/SOSTransport.h>
#include <Security/SecureObjectSync/SOSViews.h>
#include <Security/SecureObjectSync/SOSRing.h>
#include <Security/SecureObjectSync/SOSRingUtils.h>
#include <Security/SecureObjectSync/SOSPeerInfoCollections.h>

#if 0
static inline bool SOSAccountHasLeft(SOSAccountRef account) {
    switch(account->departure_code) {
        case kSOSWithdrewMembership: /* Fallthrough */
        case kSOSMembershipRevoked: /* Fallthrough */
        case kSOSLeftUntrustedCircle:
            return true;
        case kSOSNeverAppliedToCircle: /* Fallthrough */
        case kSOSNeverLeftCircle: /* Fallthrough */
        default:
            return false;
    }
}
#endif

static const char * __unused concordstring[] = {
    "kSOSConcordanceTrusted",
    "kSOSConcordanceGenOld",     // kSOSErrorReplay
    "kSOSConcordanceNoUserSig",  // kSOSErrorBadSignature
    "kSOSConcordanceNoUserKey",  // kSOSErrorNoKey
    "kSOSConcordanceNoPeer",     // kSOSErrorPeerNotFound
    "kSOSConcordanceBadUserSig", // kSOSErrorBadSignature
    "kSOSConcordanceBadPeerSig", // kSOSErrorBadSignature
    "kSOSConcordanceNoPeerSig",
    "kSOSConcordanceWeSigned",
    "kSOSConcordanceInvalidMembership",
    "kSOSConcordanceMissingMe",
    "kSOSConcordanceImNotWorthy",
};


static bool SOSAccountIsPeerRetired(SOSAccountRef account, CFSetRef peers){
    CFMutableArrayRef peerInfos = CFArrayCreateMutableForCFTypes(kCFAllocatorDefault);
    bool result = false;
    
    CFSetForEach(peers, ^(const void *value) {
        SOSPeerInfoRef peer = (SOSPeerInfoRef)value;
        if(SOSPeerInfoIsRetirementTicket(peer))
            CFArrayAppendValue(peerInfos, peer);
    });
    if(CFArrayGetCount(peerInfos) > 0){
        if(!SOSAccountRemoveBackupPeers(account, peerInfos, NULL))
            secerror("Could not remove peers: %@, from the backup", peerInfos);
        else
            return true;
    }
    else
        result = true;
    
    CFReleaseNull(peerInfos);
    
    return result;
}

static bool SOSAccountBackupSliceKeyBagNeedsFix(SOSAccountRef account, SOSBackupSliceKeyBagRef bskb) {

    if (SOSBSKBIsDirect(bskb) || account->backup_key == NULL)
        return false;

    CFSetRef peers = SOSBSKBGetPeers(bskb);
    
    /* first scan for retired peers, and kick'em out!*/
    SOSAccountIsPeerRetired(account, peers);
    
    SOSPeerInfoRef myPeer = SOSAccountGetMyPeerInfo(account);
    bool needsFix = true;

    if (myPeer) {
        SOSPeerInfoRef meInBag = (SOSPeerInfoRef) CFSetGetValue(peers, myPeer);
        CFDataRef myBK = SOSPeerInfoCopyBackupKey(myPeer);
        CFDataRef meInBagBK = SOSPeerInfoCopyBackupKey(meInBag);
        needsFix = !(meInBag && CFEqualSafe(myBK,
                                            meInBagBK));
        CFReleaseNull(myBK);
        CFReleaseNull(meInBagBK);
    }
    
    CFDataRef rkbg = SOSAccountCopyRecoveryPublic(kCFAllocatorDefault, account, NULL);
    if(rkbg) needsFix |= !SOSBKSBPrefixedKeyIsInKeyBag(bskb, bskbRkbgPrefix, rkbg);
    else needsFix |= SOSBSKBHasRecoveryKey(bskb); // if we don't have a recovery key - the bskb shouldn't
    CFReleaseNull(rkbg);

    return needsFix;
}


typedef enum {
    accept,
    countersign,
    leave,
    revert,
    modify,
    ignore
} ringAction_t;

static const char * __unused actionstring[] = {
    "accept", "countersign", "leave", "revert", "modify", "ignore",
};

bool SOSAccountHandleUpdateRing(SOSAccountRef account, SOSRingRef prospectiveRing, bool writeUpdate, CFErrorRef *error) {
    bool success = true;
    bool haveOldRing = true;
    const char * __unused localRemote = writeUpdate ? "local": "remote";
    SOSFullPeerInfoRef fpi = account->my_identity;
    SOSPeerInfoRef     pi = SOSFullPeerInfoGetPeerInfo(fpi);
    CFStringRef        peerID = SOSPeerInfoGetPeerID(pi);
    bool               peerActive = (fpi && pi && peerID && SOSAccountIsInCircle(account, NULL));
    

    secdebug("ringSigning", "start:[%s] %@", localRemote, prospectiveRing);

    require_quiet(SOSAccountHasPublicKey(account, error), errOut);

    require_action_quiet(prospectiveRing, errOut,
                         SOSCreateError(kSOSErrorIncompatibleCircle, CFSTR("No Ring to work with"), NULL, error));

    require_action_quiet(SOSRingIsStable(prospectiveRing), errOut, SOSCreateError(kSOSErrorIncompatibleCircle, CFSTR("You give rings a bad name"), NULL, error));

    // We should at least have a sane ring system in the account object
    require_quiet(SOSAccountCheckForRings(account, error), errOut);

    CFStringRef ringName = SOSRingGetName(prospectiveRing);
    SOSRingRef oldRing = SOSAccountCopyRing(account, ringName, NULL);

    SOSTransportCircleRef transport = account->circle_transport;
    
    SOSRingRef newRing = CFRetainSafe(prospectiveRing); // TODO:  SOSAccountCloneRingWithRetirement(account, prospectiveRing, error);

    ringAction_t ringAction = ignore;

    bool userTrustedoldRing = true;

    SOSCircleRef circle = SOSAccountGetCircle(account, NULL);
    CFSetRef peers = SOSCircleCopyPeers(circle, kCFAllocatorDefault);

    SecKeyRef oldKey = account->user_public;

#if 0
    // for now user keys aren't explored.
    // we should ask the ring if it cares about it and then do the magic to find the right user keys.
    SecKeyRef oldKey = account->user_public;

    if(SOSRingPKTrusted(oldRing, account->user_public, NULL)) oldKey = account->user_public;
    else if(account->previous_public && SOSRingPKTrusted(oldRing, account->previous_public, NULL)) oldKey = account->previous_public;
    bool userTrustedoldRing = (oldKey != NULL) && haveOldRing;

#endif

    if (!oldRing) {
        oldRing = CFRetainSafe(newRing);
    }

    SOSConcordanceStatus concstat = SOSRingConcordanceTrust(fpi, peers, oldRing, newRing, oldKey, account->user_public, peerID, error);
    CFReleaseNull(peers);

    CFStringRef concStr = NULL;
    switch(concstat) {
        case kSOSConcordanceTrusted:
            ringAction = countersign;
            concStr = CFSTR("Trusted");
            break;
        case kSOSConcordanceGenOld:
            ringAction = userTrustedoldRing ? revert : ignore;
            concStr = CFSTR("Generation Old");
            break;
        case kSOSConcordanceBadUserSig:
        case kSOSConcordanceBadPeerSig:
            ringAction = userTrustedoldRing ? revert : accept;
            concStr = CFSTR("Bad Signature");
            break;
        case kSOSConcordanceNoUserSig:
            ringAction = userTrustedoldRing ? revert : accept;
            concStr = CFSTR("No User Signature");
            break;
        case kSOSConcordanceNoPeerSig:
            ringAction = accept; // We might like this one eventually but don't countersign.
            concStr = CFSTR("No trusted peer signature");
            secnotice("signing", "##### No trusted peer signature found, accepting hoping for concordance later %@", newRing);
            break;
        case kSOSConcordanceNoPeer:
            ringAction = leave;
            concStr = CFSTR("No trusted peer left");
            break;
        case kSOSConcordanceNoUserKey:
            secerror("##### No User Public Key Available, this shouldn't ever happen!!!");
            ringAction = ignore;
            break;
            
        case kSOSConcordanceMissingMe:
        case kSOSConcordanceImNotWorthy:
            ringAction = modify;
            concStr = CFSTR("Incorrect membership for me");
            break;
        case kSOSConcordanceInvalidMembership:
            ringAction = userTrustedoldRing ? revert : ignore;
            concStr = CFSTR("Invalid Ring Membership");
            break;
        default:
            secerror("##### Bad Error Return from ConcordanceTrust");
            ringAction = ignore;
            break;
    }

    (void)concStr;
    
    secdebug("ringSigning", "Decided on action [%s] based on concordance state [%s] and [%s] circle.", actionstring[ringAction], concordstring[concstat], userTrustedoldRing ? "trusted" : "untrusted");

    SOSRingRef ringToPush = NULL;
    bool iWasInOldRing = peerID && SOSRingHasPeerID(oldRing, peerID);
    bool iAmInNewRing = peerID && SOSRingHasPeerID(newRing, peerID);
    bool ringIsBackup = SOSRingGetType(newRing) == kSOSRingBackup;
    bool ringIsRecovery = SOSRingGetType(newRing) == kSOSRingRecovery;

    if (ringIsBackup && peerActive) {
        if (ringAction == accept || ringAction == countersign) {
            CFErrorRef localError = NULL;
            SOSBackupSliceKeyBagRef bskb = SOSRingCopyBackupSliceKeyBag(newRing, &localError);

            if(!bskb) {
                secnotice("ringSigning", "Backup ring with no backup slice keybag (%@)", localError);
            } else if (SOSAccountBackupSliceKeyBagNeedsFix(account, bskb)) {
                ringAction = modify;
            }
            CFReleaseSafe(localError);
            CFReleaseSafe(bskb);
        }

        if (ringAction == modify) {
            CFErrorRef updateError = NULL;
            SOSAccountSetRing(account, newRing, ringName, error);

            if(SOSAccountUpdateOurPeerInBackup(account, newRing, &updateError)) {
                secdebug("signing", "Modified backup ring to include us");
            } else {
                secerror("Could not add ourselves to the backup: (%@)", updateError);
            }
            CFReleaseSafe(updateError);

            // Fall through to normal modify handling.
        }
    }
    
    if (ringIsRecovery && peerActive && (ringAction == modify)) {
        SOSAccountSetRing(account, newRing, ringName, error);
    }


    if (ringAction == modify) {
        ringAction = ignore;
    }

    if (ringAction == leave) {
        if (iWasInOldRing) {
            if (sosAccountLeaveRing(account, newRing, error)) {
                ringToPush = newRing;
            } else {
                secdebug("ringSigning", "Can't leave ring %@", oldRing);
                success = false;
            }
            ringAction = accept;
        } else {
            // We are not in this ring, but we need to update account with it, since we got it from cloud
            ringAction = accept;
        }
    }

    if (ringAction == countersign) {
        if (iAmInNewRing) {
            if (SOSRingPeerTrusted(newRing, fpi, NULL)) {
                secdebug("ringSigning", "Already concur with: %@", newRing);
            } else {
                CFErrorRef signingError = NULL;

                if (fpi && SOSRingConcordanceSign(newRing, fpi, &signingError)) {
                    ringToPush = newRing;
                } else {
                    secerror("Failed to concordance sign, error: %@  Old: %@ New: %@", signingError, oldRing, newRing);
                    success = false;
                }
                CFReleaseSafe(signingError);
            }
        } else {
            secdebug("ringSigning", "Not countersigning, not in ring: %@", newRing);
        }
        ringAction = accept;
    }

    if (ringAction == accept) {
        if (iWasInOldRing && !iAmInNewRing) {

            //  Don't destroy evidence of other code determining reason for leaving.
            //if(!SOSAccountHasLeft(account)) account->departure_code = kSOSMembershipRevoked;
            // TODO: LeaveReason for rings
        }

        if (pi && SOSRingHasRejection(newRing, peerID)) {
            // TODO: ReasonForLeaving for rings
            SOSRingRemoveRejection(newRing, peerID);
        }

        SOSAccountSetRing(account, newRing, ringName, error);

        if (pi && account->user_public_trusted
            && SOSRingHasApplicant(oldRing, peerID)
            && SOSRingCountPeers(newRing) > 0
            && !iAmInNewRing && !SOSRingHasApplicant(newRing, peerID)) {
            // We weren't rejected (above would have set me to NULL.
            // We were applying and we weren't accepted.
            // Our application is declared lost, let us reapply.

            if (SOSRingApply(newRing, account->user_public, fpi, NULL))
                if(peerActive) writeUpdate = true;
        }

        if (pi && SOSRingHasPeerID(oldRing, peerID)) {
            SOSAccountCleanupRetirementTickets(account, RETIREMENT_FINALIZATION_SECONDS, NULL);
        }


        account->circle_rings_retirements_need_attention = true;

        if (writeUpdate)
            ringToPush = newRing;
        account->key_interests_need_updating = true;
    }

    /*
     * In the revert section we'll guard the KVS idea of circles by rejecting "bad" new rings
     * and pushing our current view of the ring (oldRing).  We'll only do this if we actually
     * are a member of oldRing - never for an empty ring.
     */

    if (ringAction == revert) {
        if(haveOldRing && peerActive && SOSRingHasPeerID(oldRing, peerID)) {
            secdebug("ringSigning", "%@, Rejecting: %@ re-publishing %@", concStr, newRing, oldRing);
            ringToPush = oldRing;
        } else {
            secdebug("ringSigning", "%@, Rejecting: %@ Have no old circle - would reset", concStr, newRing);
        }
    }


    if (ringToPush != NULL) {
        secdebug("ringSigning", "Pushing:[%s] %@", localRemote, ringToPush);
        CFDataRef ringData = SOSRingCopyEncodedData(ringToPush, error);
        if (ringData) {
            success &= SOSTransportCircleRingPostRing(transport, SOSRingGetName(ringToPush), ringData, error);
        } else {
            success = false;
        }
        CFReleaseNull(ringData);
    }
    CFReleaseNull(oldRing);
    CFReleaseSafe(newRing);
    return success;
errOut:
    return false;
}

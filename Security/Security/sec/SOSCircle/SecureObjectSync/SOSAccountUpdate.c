//
//  SOSAccountUpdate.c
//  sec
//

#include "SOSAccountPriv.h"
#include <SecureObjectSync/SOSTransportCircle.h>
#include <SecureObjectSync/SOSTransport.h>
#include <SecureObjectSync/SOSPeerInfoCollections.h>
#include <CKBridge/SOSCloudKeychainClient.h>

static void DifferenceAndCall(CFSetRef old_members, CFSetRef new_members, void (^updatedCircle)(CFSetRef additions, CFSetRef removals))
{
    CFMutableSetRef additions = CFSetCreateMutableCopy(kCFAllocatorDefault, 0, new_members);
    CFMutableSetRef removals = CFSetCreateMutableCopy(kCFAllocatorDefault, 0, old_members);
    
    
    CFSetForEach(old_members, ^(const void * value) {
        CFSetRemoveValue(additions, value);
    });
    
    CFSetForEach(new_members, ^(const void * value) {
        CFSetRemoveValue(removals, value);
    });
    
    updatedCircle(additions, removals);
    
    CFReleaseSafe(additions);
    CFReleaseSafe(removals);
}

static void SOSAccountNotifyEngines(SOSAccountRef account, SOSCircleRef new_circle,
                                    CFSetRef added_peers, CFSetRef removed_peers,
                                    CFSetRef added_applicants, CFSetRef removed_applicants)
{
    SOSPeerInfoRef myPi = SOSAccountGetMyPeerInCircle(account, new_circle, NULL);
    CFStringRef myPi_id = NULL;
    CFMutableArrayRef trusted_peer_ids = NULL;
    CFMutableArrayRef untrusted_peer_ids = NULL;

    if (myPi && SOSCircleHasPeer(new_circle, myPi, NULL)) {
        myPi_id = SOSPeerInfoGetPeerID(myPi);
        trusted_peer_ids = CFArrayCreateMutableForCFTypes(kCFAllocatorDefault);
        untrusted_peer_ids = CFArrayCreateMutableForCFTypes(kCFAllocatorDefault);
        SOSCircleForEachPeer(new_circle, ^(SOSPeerInfoRef peer) {
            CFMutableArrayRef arrayToAddTo = SOSPeerInfoApplicationVerify(peer, account->user_public, NULL) ? trusted_peer_ids : untrusted_peer_ids;
            CFArrayAppendValue(arrayToAddTo, SOSPeerInfoGetPeerID(peer));
        });
    }

    CFArrayRef dsNames = account->factory->copy_names(account->factory);
    CFStringRef dsName = NULL;
    CFArrayForEachC(dsNames, dsName) {
        SOSEngineRef engine = SOSDataSourceFactoryGetEngineForDataSourceName(account->factory, dsName, NULL);
        if (engine)
            SOSEngineCircleChanged(engine, myPi_id, trusted_peer_ids, untrusted_peer_ids);
    }
    CFReleaseSafe(dsNames);
    CFReleaseNull(trusted_peer_ids);
    CFReleaseNull(untrusted_peer_ids);
}

static void SOSAccountNotifyOfChange(SOSAccountRef account, SOSCircleRef oldCircle, SOSCircleRef newCircle)
{
    CFMutableSetRef old_members = SOSCircleCopyPeers(oldCircle, kCFAllocatorDefault);
    CFMutableSetRef new_members = SOSCircleCopyPeers(newCircle, kCFAllocatorDefault);
    
    CFMutableSetRef old_applicants = SOSCircleCopyApplicants(oldCircle, kCFAllocatorDefault);
    CFMutableSetRef new_applicants = SOSCircleCopyApplicants(newCircle, kCFAllocatorDefault);
    
    DifferenceAndCall(old_members, new_members, ^(CFSetRef added_members, CFSetRef removed_members) {
        DifferenceAndCall(old_applicants, new_applicants, ^(CFSetRef added_applicants, CFSetRef removed_applicants) {
            SOSAccountNotifyEngines(account, newCircle, added_members, removed_members, added_applicants, removed_applicants);
            CFArrayForEach(account->change_blocks, ^(const void * notificationBlock) {
                ((SOSAccountCircleMembershipChangeBlock) notificationBlock)(newCircle, added_members, removed_members, added_applicants, removed_applicants);
            });
        });
    });
    
    CFReleaseNull(old_applicants);
    CFReleaseNull(new_applicants);
    
    CFReleaseNull(old_members);
    CFReleaseNull(new_members);
}

void SOSAccountRecordRetiredPeerInCircleNamed(SOSAccountRef account, CFStringRef circleName, SOSPeerInfoRef retiree)
{
    // Replace Peer with RetiredPeer, if were a peer.
    SOSAccountModifyCircle(account, circleName, NULL, ^(SOSCircleRef circle) {
        SOSPeerInfoRef me = SOSAccountGetMyPeerInCircleNamed(account, circleName, NULL);
        if(!me || !SOSCircleHasActivePeer(circle, me, NULL)) return (bool) false;
        
        bool updated = SOSCircleUpdatePeerInfo(circle, retiree);
        if (updated) {
            CFErrorRef cleanupError = NULL;
            if (!SOSAccountCleanupAfterPeer(account, RETIREMENT_FINALIZATION_SECONDS, circle, retiree, &cleanupError))
                secerror("Error cleanup up after peer (%@): %@", retiree, cleanupError);
            CFReleaseSafe(cleanupError);
        }
        return updated;
    });
}

static SOSCircleRef SOSAccountCreateCircleFrom(CFStringRef circleName, CFTypeRef value, CFErrorRef *error) {
    if (value && !isData(value) && !isNull(value)) {
        secnotice("circleCreat", "Value provided not appropriate for a circle");
        CFStringRef description = CFCopyTypeIDDescription(CFGetTypeID(value));
        SOSCreateErrorWithFormat(kSOSErrorUnexpectedType, NULL, error, NULL,
                                 CFSTR("Expected data or NULL got %@"), description);
        CFReleaseSafe(description);
        return NULL;
    }
    
    SOSCircleRef circle = NULL;
    if (!value || isNull(value)) {
        secnotice("circleCreat", "No circle found in data: %@", value);
        circle = NULL;
    } else {
        circle = SOSCircleCreateFromData(NULL, (CFDataRef) value, error);
        if (circle) {
            CFStringRef name = SOSCircleGetName(circle);
            if (!CFEqualSafe(name, circleName)) {
                secnotice("circleCreat", "Expected circle named %@, got %@", circleName, name);
                SOSCreateErrorWithFormat(kSOSErrorNameMismatch, NULL, error, NULL,
                                         CFSTR("Expected circle named %@, got %@"), circleName, name);
                CFReleaseNull(circle);
            }
        } else {
            secnotice("circleCreat", "SOSCircleCreateFromData returned NULL.");
        }
    }
    return circle;
}

bool SOSAccountHandleCircleMessage(SOSAccountRef account,
                                   CFStringRef circleName, CFDataRef encodedCircleMessage, CFErrorRef *error) {
    bool success = false;
    CFErrorRef localError = NULL;
    SOSCircleRef circle = SOSAccountCreateCircleFrom(circleName, encodedCircleMessage, &localError);
    if (circle) {
        success = SOSAccountUpdateCircleFromRemote(account, circle, &localError);
        CFReleaseSafe(circle);
    } else {
        secerror("NULL circle found, ignoring ...");
        success = true;  // don't pend this NULL thing.
    }

    if (!success) {
        if (isSOSErrorCoded(localError, kSOSErrorIncompatibleCircle)) {
            secerror("Incompatible circle found, abandoning membership: %@", circleName);
            SOSAccountDestroyCirclePeerInfoNamed(account, circleName, NULL);
            CFDictionarySetValue(account->circles, circleName, kCFNull);
        }

        if (error) {
            *error = localError;
            localError = NULL;
        }

    }

    CFReleaseNull(localError);

    return success;
}

bool SOSAccountHandleParametersChange(SOSAccountRef account, CFDataRef parameters, CFErrorRef *error){
    
    SecKeyRef newKey = NULL;
    CFDataRef newParameters = NULL;
    bool success = false;
    
    if(SOSAccountRetrieveCloudParameters(account, &newKey, parameters, &newParameters, error)) {
        if (CFEqualSafe(account->user_public, newKey)) {
            secnotice("updates", "Got same public key sent our way. Ignoring.");
            success = true;
        } else if (CFEqualSafe(account->previous_public, newKey)) {
            secnotice("updates", "Got previous public key repeated. Ignoring.");
            success = true;
        } else {
            CFReleaseNull(account->user_public);
            SOSAccountPurgePrivateCredential(account);
            CFReleaseNull(account->user_key_parameters);
            
            account->user_public_trusted = false;
            
            account->user_public = newKey;
            newKey = NULL;
            
            account->user_key_parameters = newParameters;
            newParameters = NULL;
            
            secnotice("updates", "Got new parameters for public key: %@", account->user_public);
            debugDumpUserParameters(CFSTR("params"), account->user_key_parameters);

            SOSUpdateKeyInterest();

            success = true;
        }
    }
    
    CFReleaseNull(newKey);
    CFReleaseNull(newParameters);
    
    return success;
}

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

static const char *concordstring[] = {
    "kSOSConcordanceTrusted",
    "kSOSConcordanceGenOld",     // kSOSErrorReplay
    "kSOSConcordanceNoUserSig",  // kSOSErrorBadSignature
    "kSOSConcordanceNoUserKey",  // kSOSErrorNoKey
    "kSOSConcordanceNoPeer",     // kSOSErrorPeerNotFound
    "kSOSConcordanceBadUserSig", // kSOSErrorBadSignature
    "kSOSConcordanceBadPeerSig", // kSOSErrorBadSignature
    "kSOSConcordanceNoPeerSig",
    "kSOSConcordanceWeSigned",
};

bool SOSAccountHandleUpdateCircle(SOSAccountRef account, SOSCircleRef prospective_circle, bool writeUpdate, CFErrorRef *error)
{
    bool success = true;
    bool haveOldCircle = true;
    const char *local_remote = writeUpdate ? "local": "remote";

    secnotice("signing", "start:[%s] %@", local_remote, prospective_circle);
    if (!account->user_public || !account->user_public_trusted) {
        SOSCreateError(kSOSErrorPublicKeyAbsent, CFSTR("Can't handle updates with no trusted public key here"), NULL, error);
        return false;
    }
    
    if (!prospective_circle) {
        secerror("##### Can't update to a NULL circle ######");
        return false; // Can't update one we don't have.
    }
    
    CFStringRef newCircleName = SOSCircleGetName(prospective_circle);
    SOSCircleRef oldCircle = (SOSCircleRef) CFDictionaryGetValue(account->circles, newCircleName);
    SOSCircleRef emptyCircle = NULL;
    
    if(oldCircle == NULL) {
        SOSCreateErrorWithFormat(kSOSErrorIncompatibleCircle, NULL, error, NULL, CFSTR("Current Entry is NULL; rejecting %@"), prospective_circle);
        secerror("##### Can't replace circle - we don't care about %@ ######", prospective_circle);
        return false;
    }
    if (CFGetTypeID(oldCircle) != SOSCircleGetTypeID()) {
        secdebug("badcircle", ">>>>>>>>>>>>>>>  Non-Circle Circle found <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<");
        // We don't know what is in our table, likely it was kCFNull indicating we didn't
        // understand a circle that came by. We seem to like this one lets make our entry be empty circle
        emptyCircle = SOSCircleCreate(kCFAllocatorDefault, newCircleName, NULL);
        oldCircle = emptyCircle;
        haveOldCircle = false;
        // And we're paranoid, drop our old peer info if for some reason we didn't before.
        // SOSAccountDestroyCirclePeerInfo(account, oldCircle, NULL);
    }
    
    // Changed to just get the fullpeerinfo if present.  We don't want to make up FPIs here.
    SOSPeerInfoRef     me = NULL;
    SOSFullPeerInfoRef me_full = SOSAccountGetMyFullPeerInCircleNamedIfPresent(account, SOSCircleGetName(oldCircle), NULL);
    if(me_full) me = SOSFullPeerInfoGetPeerInfo(me_full);
    
    SOSTransportCircleRef transport = (SOSTransportCircleRef)CFDictionaryGetValue(account->circle_transports, SOSCircleGetName(prospective_circle));

    SOSAccountScanForRetired(account, prospective_circle, error);
    SOSCircleRef newCircle = SOSAccountCloneCircleWithRetirement(account, prospective_circle, error);
    if(!newCircle) return false;

    if (me && SOSCircleUpdatePeerInfo(newCircle, me)) {
        writeUpdate = true; // If we update our peer in the new circle we should write it if we accept it.
    }

    typedef enum {
        accept,
        countersign,
        leave,
        revert,
        ignore
    } circle_action_t;
    
    static const char *actionstring[] = {
        "accept", "countersign", "leave", "revert", "ignore",
    };
    
    circle_action_t circle_action = ignore;
    enum DepartureReason leave_reason = kSOSNeverLeftCircle;
    
    SecKeyRef old_circle_key = NULL;
    if(SOSCircleVerify(oldCircle, account->user_public, NULL)) old_circle_key = account->user_public;
    else if(account->previous_public && SOSCircleVerify(oldCircle, account->previous_public, NULL)) old_circle_key = account->previous_public;
    bool userTrustedOldCircle = (old_circle_key != NULL) && haveOldCircle;
    
    SOSConcordanceStatus concstat =
    SOSCircleConcordanceTrust(oldCircle, newCircle,
                              old_circle_key, account->user_public,
                              me, error);
    
    CFStringRef concStr = NULL;
    switch(concstat) {
        case kSOSConcordanceTrusted:
            circle_action = countersign;
            concStr = CFSTR("Trusted");
            break;
        case kSOSConcordanceGenOld:
            circle_action = userTrustedOldCircle ? revert : ignore;
            concStr = CFSTR("Generation Old");
            break;
        case kSOSConcordanceBadUserSig:
        case kSOSConcordanceBadPeerSig:
            circle_action = userTrustedOldCircle ? revert : accept;
            concStr = CFSTR("Bad Signature");
            break;
        case kSOSConcordanceNoUserSig:
            circle_action = userTrustedOldCircle ? revert : accept;
            concStr = CFSTR("No User Signature");
            break;
        case kSOSConcordanceNoPeerSig:
            circle_action = accept; // We might like this one eventually but don't countersign.
            concStr = CFSTR("No trusted peer signature");
            secerror("##### No trusted peer signature found, accepting hoping for concordance later %@", newCircle);
            break;
        case kSOSConcordanceNoPeer:
            circle_action = leave;
            leave_reason = kSOSLeftUntrustedCircle;
            concStr = CFSTR("No trusted peer left");
            break;
        case kSOSConcordanceNoUserKey:
            secerror("##### No User Public Key Available, this shouldn't ever happen!!!");
            abort();
            break;
        default:
            secerror("##### Bad Error Return from ConcordanceTrust");
            abort();
            break;
    }
    
    secnotice("signing", "Decided on action [%s] based on concordance state [%s] and [%s] circle.", actionstring[circle_action], concordstring[concstat], userTrustedOldCircle ? "trusted" : "untrusted");
    
    SOSCircleRef circleToPush = NULL;

    if (circle_action == leave) {
        circle_action = ignore; (void) circle_action; // Acknowledge this is a dead store.
        
        if (me && SOSCircleHasPeer(oldCircle, me, NULL)) {
            if (sosAccountLeaveCircle(account, newCircle, error)) {
                circleToPush = newCircle;
            } else {
                secnotice("signing", "Can't leave circle %@, but dumping identities", oldCircle);
                success = false;
            }
            account->departure_code = leave_reason;
            circle_action = accept;
            me = NULL;
            me_full = NULL;
        } else {
            // We are not in this circle, but we need to update account with it, since we got it from cloud
            secnotice("signing", "We are not in this circle, but we need to update account with it");
            circle_action = accept;
        }
    }
    
    if (circle_action == countersign) {
        if (me && SOSCircleHasPeer(newCircle, me, NULL) && !SOSCircleVerifyPeerSigned(newCircle, me, NULL)) {
            CFErrorRef signing_error = NULL;
            
            if (me_full && SOSCircleConcordanceSign(newCircle, me_full, &signing_error)) {
                circleToPush = newCircle;
                secnotice("signing", "Concurred with: %@", newCircle);
            } else {
                secerror("Failed to concurrence sign, error: %@  Old: %@ New: %@", signing_error, oldCircle, newCircle);
                success = false;
            }
            CFReleaseSafe(signing_error);
        }
        circle_action = accept;
    }
    
    if (circle_action == accept) {
        if (me && SOSCircleHasActivePeer(oldCircle, me, NULL) && !SOSCircleHasPeer(newCircle, me, NULL)) {
            //  Don't destroy evidence of other code determining reason for leaving.
            if(!SOSAccountHasLeft(account)) account->departure_code = kSOSMembershipRevoked;
        }
        
        if (me
            && SOSCircleHasActivePeer(oldCircle, me, NULL)
            && !(SOSCircleCountPeers(oldCircle) == 1 && SOSCircleHasPeer(oldCircle, me, NULL)) // If it was our offering, don't change ID to avoid ghosts
            && !SOSCircleHasPeer(newCircle, me, NULL) && !SOSCircleHasApplicant(newCircle, me, NULL)) {
            secnotice("circle", "Purging my peer (ID: %@) for circle '%@'!!!", SOSPeerInfoGetPeerID(me), SOSCircleGetName(oldCircle));
            SOSAccountDestroyCirclePeerInfo(account, oldCircle, NULL);
            me = NULL;
            me_full = NULL;
        }
        
        if (me && SOSCircleHasRejectedApplicant(newCircle, me, NULL)) {
            SOSPeerInfoRef  reject = SOSCircleCopyRejectedApplicant(newCircle, me, NULL);
            if(CFEqualSafe(reject, me) && SOSPeerInfoApplicationVerify(me, account->user_public, NULL)) {
                secnotice("circle", "Rejected, Purging my applicant peer (ID: %@) for circle '%@'", SOSPeerInfoGetPeerID(me), SOSCircleGetName(oldCircle));
                SOSAccountDestroyCirclePeerInfo(account, oldCircle, NULL);
                me = NULL;
                me_full = NULL;
            } else {
                SOSCircleRequestReadmission(newCircle, account->user_public, me, NULL);
                writeUpdate = true;
            }
        }
        
        CFRetain(oldCircle); // About to replace the oldCircle
        CFDictionarySetValue(account->circles, newCircleName, newCircle);
        SOSAccountSetPreviousPublic(account);
        
        secnotice("signing", "%@, Accepting circle: %@", concStr, newCircle);
        
        if (me && account->user_public_trusted
            && SOSCircleHasApplicant(oldCircle, me, NULL)
            && SOSCircleCountPeers(newCircle) > 0
            && !SOSCircleHasPeer(newCircle, me, NULL) && !SOSCircleHasApplicant(newCircle, me, NULL)) {
            // We weren't rejected (above would have set me to NULL.
            // We were applying and we weren't accepted.
            // Our application is declared lost, let us reapply.
            
            if (SOSCircleRequestReadmission(newCircle, account->user_public, me, NULL))
                writeUpdate = true;
        }
        
        if (me && SOSCircleHasActivePeer(oldCircle, me, NULL)) {
            SOSAccountCleanupRetirementTickets(account, RETIREMENT_FINALIZATION_SECONDS, NULL);
        }
        
        SOSAccountNotifyOfChange(account, oldCircle, newCircle);
        
        CFReleaseNull(oldCircle);
        
        if (writeUpdate)
            circleToPush = newCircle;
        SOSUpdateKeyInterest();
    }
    
    /*
     * In the revert section we'll guard the KVS idea of circles by rejecting "bad" new circles
     * and pushing our current view of the circle (oldCircle).  We'll only do this if we actually
     * are a member of oldCircle - never for an empty circle.
     */
    
    if (circle_action == revert) {
        if(haveOldCircle && me && SOSCircleHasActivePeer(oldCircle, me, NULL)) {
            secnotice("signing", "%@, Rejecting: %@ re-publishing %@", concStr, newCircle, oldCircle);
            circleToPush = oldCircle;
        } else {
            secnotice("canary", "%@, Rejecting: %@ Have no old circle - would reset", concStr, newCircle);
        }
    }
    
    
    if (circleToPush != NULL) {
        secnotice("signing", "Pushing:[%s] %@", local_remote, circleToPush);
        CFDataRef circle_data = SOSCircleCopyEncodedData(circleToPush, kCFAllocatorDefault, error);
        if (circle_data) {
            success &= SOSTransportCirclePostCircle(transport, SOSCircleGetName(circleToPush), circle_data, error);
        } else {
            success = false;
        }
        CFReleaseNull(circle_data);

        success = (success && SOSTransportCircleFlushChanges(transport, error));
    }
    
    CFReleaseSafe(newCircle);
    CFReleaseNull(emptyCircle);
    return success;
}

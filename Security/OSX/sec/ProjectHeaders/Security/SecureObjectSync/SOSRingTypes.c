//
//  SOSRingTypes.c
//  sec
//
//  Created by Richard Murphy on 2/23/15.
//
//

#include "SOSRing.h"
#include "SOSRingTypes.h"
#include "SOSRingBasic.h"
#include "SOSRingBackup.h"
#include <Security/SecureObjectSync/SOSAccountPriv.h>
#include <Security/SecureObjectSync/SOSBackupSliceKeyBag.h>

static ringFuncs ringTypes[] = {
    &basic,
    &backup,
};
static const size_t typecount = sizeof(ringTypes) / sizeof(ringFuncs);

static bool SOSRingValidType(SOSRingType type) {
    return type < typecount;
}

// MARK: Exported Functions


SOSRingRef SOSRingCreate(CFStringRef name, CFStringRef myPeerID, SOSRingType type, CFErrorRef *error) {
    require(SOSRingValidType(type), errOut);
    require(ringTypes[type]->sosRingCreate, errOut);
    return ringTypes[type]->sosRingCreate(name, myPeerID, error);
errOut:
    SOSCreateError(kSOSErrorUnexpectedType, CFSTR("Not valid ring type"), NULL, error);
    return NULL;
}

bool SOSRingResetToEmpty(SOSRingRef ring, CFStringRef myPeerID, CFErrorRef *error) {
    SOSRingAssertStable(ring);
    SOSRingType type = SOSRingGetType(ring);
    require(SOSRingValidType(type), errOut);
    require(ringTypes[type]->sosRingResetToEmpty, errOut);
    return ringTypes[type]->sosRingResetToEmpty(ring, myPeerID, error);
errOut:
    SOSCreateError(kSOSErrorUnexpectedType, CFSTR("Not valid ring type"), NULL, error);
    return false;
}

bool SOSRingResetToOffering(SOSRingRef ring, SecKeyRef user_privkey, SOSFullPeerInfoRef requestor, CFErrorRef *error) {
    SOSRingAssertStable(ring);
    SOSRingType type = SOSRingGetType(ring);
    require(SOSRingValidType(type), errOut);
    require(ringTypes[type]->sosRingResetToOffering, errOut);
    return ringTypes[type]->sosRingResetToOffering(ring, user_privkey, requestor, error);
errOut:
    SOSCreateError(kSOSErrorUnexpectedType, CFSTR("Not valid ring type"), NULL, error);
    return false;
}

SOSRingStatus SOSRingDeviceIsInRing(SOSRingRef ring, CFStringRef peerID) {
    SOSRingAssertStable(ring);
    SOSRingType type = SOSRingGetType(ring);
    require(SOSRingValidType(type), errOut);
    require(ringTypes[type]->sosRingDeviceIsInRing, errOut);
    return ringTypes[type]->sosRingDeviceIsInRing(ring, peerID);
errOut:
    return kSOSRingError;
}

bool SOSRingApply(SOSRingRef ring, SecKeyRef user_pubkey, SOSFullPeerInfoRef fpi, CFErrorRef *error) {
    SOSRingAssertStable(ring);
    SOSRingType type = SOSRingGetType(ring);
    require(SOSRingValidType(type), errOut);
    require(ringTypes[type]->sosRingApply, shortCircuit);
    require_quiet(SOSPeerInfoApplicationVerify(SOSFullPeerInfoGetPeerInfo(fpi), user_pubkey, error), errOut2);

    return ringTypes[type]->sosRingApply(ring, user_pubkey, fpi, error);
errOut:
    SOSCreateError(kSOSErrorUnexpectedType, CFSTR("Not valid ring type"), NULL, error);
    return false;
errOut2:
    SOSCreateError(kSOSErrorBadSignature, CFSTR("FullPeerInfo fails userkey signature check"), NULL, error);
    return false;
shortCircuit:
    return true;
}

bool SOSRingWithdraw(SOSRingRef ring, SecKeyRef user_privkey, SOSFullPeerInfoRef requestor, CFErrorRef *error) {
    SOSRingAssertStable(ring);
    SOSRingType type = SOSRingGetType(ring);
    require(SOSRingValidType(type), errOut);
    require(ringTypes[type]->sosRingWithdraw, shortCircuit);
    return ringTypes[type]->sosRingWithdraw(ring, user_privkey, requestor, error);
errOut:
    SOSCreateError(kSOSErrorUnexpectedType, CFSTR("Not valid ring type"), NULL, error);
    return false;
shortCircuit:
    return true;
}

bool SOSRingGenerationSign(SOSRingRef ring, SecKeyRef user_privkey, SOSFullPeerInfoRef requestor, CFErrorRef *error) {
    SOSRingAssertStable(ring);
    SOSRingType type = SOSRingGetType(ring);
    require(SOSRingValidType(type), errOut);
    require(ringTypes[type]->sosRingGenerationSign, shortCircuit);
    return ringTypes[type]->sosRingGenerationSign(ring, user_privkey, requestor, error);
errOut:
    SOSCreateError(kSOSErrorUnexpectedType, CFSTR("Not valid ring type"), NULL, error);
    return false;
shortCircuit:
    return true;
}

bool SOSRingConcordanceSign(SOSRingRef ring, SOSFullPeerInfoRef requestor, CFErrorRef *error) {
    SOSRingAssertStable(ring);
    SOSRingType type = SOSRingGetType(ring);
    require(SOSRingValidType(type), errOut);
    require(ringTypes[type]->sosRingConcordanceSign, shortCircuit);
    return ringTypes[type]->sosRingConcordanceSign(ring, requestor, error);
errOut:
    SOSCreateError(kSOSErrorUnexpectedType, CFSTR("Not valid ring type"), NULL, error);
    return false;
shortCircuit:
    return true;
}

SOSConcordanceStatus SOSRingConcordanceTrust(SOSFullPeerInfoRef me, CFSetRef peers,
                                             SOSRingRef knownRing, SOSRingRef proposedRing,
                                             SecKeyRef knownPubkey, SecKeyRef userPubkey,
                                             CFStringRef excludePeerID, CFErrorRef *error) {
    SOSRingAssertStable(knownRing);
    SOSRingAssertStable(proposedRing);
    SOSRingType type1 = SOSRingGetType(knownRing);
    SOSRingType type2 = SOSRingGetType(proposedRing);
    require(SOSRingValidType(type1), errOut);
    require(SOSRingValidType(type2), errOut);
    require(type1 == type2, errOut);

    secnotice("ring", "concordance trust (%s) knownRing: %@ proposedRing: %@ knownkey: %@ userkey: %@ excluded: %@",
              ringTypes[type1]->typeName, knownRing, proposedRing, knownPubkey, userPubkey, excludePeerID);

    require(ringTypes[type1]->sosRingConcordanceTrust, errOut);
    return ringTypes[type1]->sosRingConcordanceTrust(me, peers, knownRing, proposedRing, knownPubkey, userPubkey, excludePeerID, error);
errOut:
    return kSOSConcordanceError;
}

bool SOSRingAccept(SOSRingRef ring, SecKeyRef user_privkey, SOSFullPeerInfoRef requestor, CFErrorRef *error) {
    SOSRingAssertStable(ring);
    SOSRingType type = SOSRingGetType(ring);
    require(SOSRingValidType(type), errOut);
    require(ringTypes[type]->sosRingAccept, shortCircuit);
    return ringTypes[type]->sosRingAccept(ring, user_privkey, requestor, error);
errOut:
    SOSCreateError(kSOSErrorUnexpectedType, CFSTR("Not valid ring type"), NULL, error);
    return false;
shortCircuit:
    return true;
}

bool SOSRingReject(SOSRingRef ring, SecKeyRef user_privkey, SOSFullPeerInfoRef requestor, CFErrorRef *error) {
    SOSRingAssertStable(ring);
    SOSRingType type = SOSRingGetType(ring);
    require(SOSRingValidType(type), errOut);
    require(ringTypes[type]->sosRingReject, shortCircuit);
    return ringTypes[type]->sosRingReject(ring, user_privkey, requestor, error);
errOut:
    SOSCreateError(kSOSErrorUnexpectedType, CFSTR("Not valid ring type"), NULL, error);
    return false;
shortCircuit:
    return true;
}

bool SOSRingSetPayload(SOSRingRef ring, SecKeyRef user_privkey, CFDataRef payload, SOSFullPeerInfoRef requestor, CFErrorRef *error) {
    SOSRingAssertStable(ring);
    SOSRingType type = SOSRingGetType(ring);
    require(SOSRingValidType(type), errOut);
    require(ringTypes[type]->sosRingSetPayload, errOut);
    return ringTypes[type]->sosRingSetPayload(ring, user_privkey, payload, requestor, error);
errOut:
    SOSCreateError(kSOSErrorUnexpectedType, CFSTR("Not valid ring type"), NULL, error);
    return false;
}

CFDataRef SOSRingGetPayload(SOSRingRef ring, CFErrorRef *error) {
    SOSRingAssertStable(ring);
    SOSRingType type = SOSRingGetType(ring);
    require(SOSRingValidType(type), errOut);
    require(ringTypes[type]->sosRingGetPayload, errOut);
    return ringTypes[type]->sosRingGetPayload(ring, error);
errOut:
    SOSCreateError(kSOSErrorUnexpectedType, CFSTR("Not valid ring type"), NULL, error);
    return false;
}

CFSetRef SOSRingGetBackupViewset(SOSRingRef ring, CFErrorRef *error) {
    SOSRingAssertStable(ring);
    SOSRingType type = SOSRingGetType(ring);
    require(kSOSRingBackup == type, errOut);
    return SOSRingGetBackupViewset_Internal(ring);
errOut:
    SOSCreateError(kSOSErrorUnexpectedType, CFSTR("Not backup ring type"), NULL, error);
    return false;
}

static bool isBackupRing(SOSRingRef ring, CFErrorRef *error) {
    SOSRingType type = SOSRingGetType(ring);
    require_quiet(kSOSRingBackup == type, errOut);
    return true;
errOut:
    SOSCreateError(kSOSErrorUnexpectedType, CFSTR("Not backup ring type"), NULL, error);
    return false;
}

bool SOSRingSetBackupKeyBag(SOSRingRef ring, SOSFullPeerInfoRef fpi, CFSetRef viewSet, SOSBackupSliceKeyBagRef bskb, CFErrorRef *error) {
    SOSRingAssertStable(ring);
    CFDataRef bskb_as_data = NULL;
    bool result = false;
    require_quiet(isBackupRing(ring, error), errOut);

    bskb_as_data = SOSBSKBCopyEncoded(bskb, error);
    result = bskb_as_data &&
             SOSRingSetBackupViewset_Internal(ring, viewSet) &&
             SOSRingSetPayload(ring, NULL, bskb_as_data, fpi, error);
errOut:
    CFReleaseNull(bskb_as_data);
    return result;
}

SOSBackupSliceKeyBagRef SOSRingCopyBackupSliceKeyBag(SOSRingRef ring, CFErrorRef *error) {
    SOSRingAssertStable(ring);

    CFDataRef bskb_as_data = NULL;
    SOSBackupSliceKeyBagRef result = NULL;
    require_quiet(isBackupRing(ring, error), errOut);

    bskb_as_data = SOSRingGetPayload(ring, error);
    require_quiet(bskb_as_data, errOut);

    result = SOSBackupSliceKeyBagCreateFromData(kCFAllocatorDefault, bskb_as_data, error);

errOut:
    return result;
}


bool SOSRingPKTrusted(SOSRingRef ring, SecKeyRef pubkey, CFErrorRef *error) {
    SOSRingAssertStable(ring);
    SOSRingType type = SOSRingGetType(ring);
    require(SOSRingValidType(type), errOut);
    return SOSRingVerify(ring, pubkey, error);
errOut:
    SOSCreateError(kSOSErrorUnexpectedType, CFSTR("Not valid ring type"), NULL, error);
    return false;
}

bool SOSRingPeerTrusted(SOSRingRef ring, SOSFullPeerInfoRef requestor, CFErrorRef *error) {
    SOSPeerInfoRef pi = SOSFullPeerInfoGetPeerInfo(requestor);
    SecKeyRef pubkey = SOSPeerInfoCopyPubKey(pi);
    bool retval = SOSRingPKTrusted(ring, pubkey, error);
    CFReleaseNull(pubkey);
    return retval;
}


#if 0
static inline
bool SOSAccountKnowsRings(SOSAccountRef account, CFErrorRef *error) {
    if(account->rings) return true;
    SOSCreateError(kSOSErrorUnsupported, CFSTR("This account doesn't support rings"), NULL, error);
    return false;
}


// ViewRequirements
bool SOSRingRequirementKnown(SOSAccountRef account, CFStringRef name, CFErrorRef *error) {
    bool retval = false;
    require_quiet(SOSAccountKnowsRings(account, error), errOut);
    retval =  CFDictionaryContainsValue(account->rings, name);
errOut:
    return retval;
}

bool SOSRingRequirementCreate(SOSAccountRef account, CFStringRef name, SOSRingType type, CFErrorRef *error) {
    if(account->rings) return false;
    if(CFDictionaryContainsValue(account->rings, name)) return true;
    if(!SOSRingValidType(type)) return false;
    SOSRingRef ring = SOSRingCreate(NULL, name, type, error);
    if(!ring) return false;
    CFDictionaryAddValue(account->rings, name, ring);
    return false;
}

static SOSRingRef getRingFromAccount(SOSAccountRef account, CFStringRef name, CFErrorRef* error) {
    SOSRingRef retval = NULL;
    require_quiet(SOSAccountKnowsRings(account, error), errOut);
    retval =  (SOSRingRef) CFDictionaryGetValue(account->rings, name);

errOut:
    return retval;

}

// Admins
bool SOSRingRequirementResetToOffering(SOSAccountRef account, CFStringRef name, CFErrorRef* error) {
    SOSRingRef ring = getRingFromAccount(account, name, error);
    SOSFullPeerInfoRef fpi = SOSAccountGetMyFullPeerInfo(account);
    require_action_quiet(ring, errOut,
        SOSCreateError(kSOSErrorNoCircle, CFSTR("No ring by name specified"), NULL, error));
    switch(SOSRingGetType(ring)) {

    }
    SOSRingResetToOffering(ring, account->__user_private, fpi, error);

errOut:
    return false;
}


bool SOSRingRequirementResetToEmpty(SOSAccountRef account, CFStringRef name, CFErrorRef* error) {
    return false;
}



// Clients
bool SOSRingRequirementRequestToJoin(SOSAccountRef account, CFStringRef name, CFErrorRef* error) {
    return false;
}

bool SOSRingRequirementRemoveThisDevice(SOSAccountRef account, CFStringRef name, CFErrorRef* error) {
    return false;
}

// Approvers
CFArrayRef SOSRingRequirementGetApplicants(SOSAccountRef account, CFStringRef name, CFErrorRef* error) {
    return false;
}


bool SOSRingRequirementAcceptApplicants(SOSAccountRef account, CFStringRef name, CFArrayRef applicants, CFErrorRef* error) {
    return false;
}


bool SOSRingRequirementRejectApplicants(SOSAccountRef account, CFStringRef name, CFArrayRef applicants, CFErrorRef *error) {
    return false;
}


bool SOSRingResetToOffering(SOSCircleRef circle, SOSRingRef ring, SecKeyRef user_privkey, SOSFullPeerInfoRef requestor, CFErrorRef *error){

    return SOSRingResetToEmpty(ring, error)
    && SOSRingRequestAdmission(ring, user_privkey, requestor, error)
    && SOSRingAcceptRequest(ring, user_privkey, requestor, SOSFullPeerInfoGetPeerInfo(requestor), error);
}


static bool SOSRingRecordAdmissionRequest(SOSRingRef ring, SecKeyRef user_pubkey, CFStringRef peerID, CFErrorRef *error) {
    SOSRingAssertStable(ring);

    bool isPeer = SOSRingHasPeerWithID(ring, peerID, error);
    require_action_quiet(!isPeer, fail, SOSCreateError(kSOSErrorAlreadyPeer, CFSTR("Cannot request admission when already a peer"), NULL, error));
    CFSetRemoveValue(ring->rejections, requestorPeerInfo); // We remove from rejected list, in case?
    CFSetSetValue(ring->applicants, requestorPeerInfo);

    return true;

fail:
    return false;

}

bool SOSRingRequestReadmission(SOSRingRef ring, SecKeyRef user_pubkey, SOSPeerInfoRef peer, CFErrorRef *error) {
    bool success = false;

    require_quiet(SOSPeerInfoApplicationVerify(peer, user_pubkey, error), fail);
    success = SOSRingRecordAdmissionRequest(ring, user_pubkey, peer, error);
fail:
    return success;
}

bool SOSRingRequestAdmission(SOSRingRef ring, SecKeyRef user_privkey, SOSFullPeerInfoRef requestor, CFErrorRef *error) {
    bool success = false;

    SecKeyRef user_pubkey = SecKeyCreatePublicFromPrivate(user_privkey);
    require_action_quiet(user_pubkey, fail, SOSCreateError(kSOSErrorBadKey, CFSTR("No public key for key"), NULL, error));

    require(SOSFullPeerInfoPromoteToApplication(requestor, user_privkey, error), fail);

    success = SOSRingRecordAdmissionRequest(ring, user_pubkey, SOSFullPeerInfoGetPeerInfo(requestor), error);
fail:
    CFReleaseNull(user_pubkey);
    return success;
}


bool SOSRingRemovePeer(SOSRingRef ring, SecKeyRef user_privkey, SOSFullPeerInfoRef requestor, SOSPeerInfoRef peer_to_remove, CFErrorRef *error) {
    SOSPeerInfoRef requestor_peer_info = SOSFullPeerInfoGetPeerInfo(requestor);

    if (SOSRingHasApplicant(ring, peer_to_remove, error)) {
        return SOSRingRejectRequest(ring, requestor, peer_to_remove, error);
    }

    if (!SOSRingHasPeer(ring, requestor_peer_info, error)) {
        SOSCreateError(kSOSErrorAlreadyPeer, CFSTR("Must be peer to remove peer"), NULL, error);
        return false;
    }

    CFSetRemoveValue(ring->peers, peer_to_remove);

    SOSRingGenerationSign(ring, user_privkey, requestor, error);

    return true;
}

bool SOSRingAcceptRequest(SOSRingRef ring, SecKeyRef user_privkey, SOSFullPeerInfoRef device_approver, SOSPeerInfoRef peerInfo, CFErrorRef *error) {
    SOSRingAssertStable(ring);

    SecKeyRef publicKey = NULL;
    bool result = false;

    require_action_quiet(CFSetContainsValue(ring->applicants, peerInfo), fail,
                         SOSCreateError(kSOSErrorNotApplicant, CFSTR("Cannot accept non-applicant"), NULL, error));

    publicKey = SecKeyCreatePublicFromPrivate(user_privkey);
    require_quiet(SOSPeerInfoApplicationVerify(peerInfo, publicKey, error), fail);

    CFSetRemoveValue(ring->applicants, peerInfo);
    CFSetSetValue(ring->peers, peerInfo);

    result = SOSRingGenerationSign(ring, user_privkey, device_approver, error);
    secnotice("ring", "Accepted %@", peerInfo);

fail:
    CFReleaseNull(publicKey);
    return result;
}

bool SOSRingWithdrawRequest(SOSRingRef ring, SOSPeerInfoRef peerInfo, CFErrorRef *error) {
    SOSRingAssertStable(ring);

    CFSetRemoveValue(ring->applicants, peerInfo);

    return true;
}

bool SOSRingRemoveRejectedPeer(SOSRingRef ring, SOSPeerInfoRef peerInfo, CFErrorRef *error) {
    SOSRingAssertStable(ring);

    CFSetRemoveValue(ring->rejected_applicants, peerInfo);

    return true;
}


bool SOSRingRejectRequest(SOSRingRef ring, SOSFullPeerInfoRef device_rejector,
                          SOSPeerInfoRef peerInfo, CFErrorRef *error) {
    SOSRingAssertStable(ring);

    if (CFEqual(SOSPeerInfoGetPeerID(peerInfo), SOSPeerInfoGetPeerID(SOSFullPeerInfoGetPeerInfo(device_rejector))))
        return SOSRingWithdrawRequest(ring, peerInfo, error);

    if (!CFSetContainsValue(ring->applicants, peerInfo)) {
        SOSCreateError(kSOSErrorNotApplicant, CFSTR("Cannot reject non-applicant"), NULL, error);
        return false;
    }

    CFSetRemoveValue(ring->applicants, peerInfo);
    CFSetSetValue(ring->rejected_applicants, peerInfo);

    // TODO: Maybe we sign the rejection with device_rejector.

    return true;
}

bool SOSRingAcceptRequests(SOSRingRef ring, SecKeyRef user_privkey, SOSFullPeerInfoRef device_approver,
                           CFErrorRef *error) {
    // Returns true if we accepted someone and therefore have to post the ring back to KVS
    __block bool result = false;

    SOSRingForEachApplicant(ring, ^(SOSPeerInfoRef peer) {
        if (!SOSRingAcceptRequest(ring, user_privkey, device_approver, peer, error))
            printf("error in SOSRingAcceptRequest\n");
        else {
            secnotice("ring", "Accepted peer: %@", peer);
            result = true;
        }
    });

    if (result) {
        SOSRingGenerationSign(ring, user_privkey, device_approver, error);
        secnotice("ring", "Countersigned accepted requests");
    }

    return result;
}

bool SOSRingPeerSigUpdate(SOSRingRef ring, SecKeyRef userPrivKey, SOSFullPeerInfoRef fpi,
                          CFErrorRef *error) {
    // Returns true if we accepted someone and therefore have to post the ring back to KVS
    __block bool result = false;
    SecKeyRef userPubKey = SecKeyCreatePublicFromPrivate(userPrivKey);

    // We're going to remove any applicants using a mismatched user key.
    SOSRingForEachApplicant(ring, ^(SOSPeerInfoRef peer) {
        if(!SOSPeerInfoApplicationVerify(peer, userPubKey, NULL)) {
            if(!SOSRingRejectRequest(ring, fpi, peer, NULL)) {
                // do we care?
            }
        }
    });

    result = SOSRingUpdatePeerInfo(ring, SOSFullPeerInfoGetPeerInfo(fpi));

    if (result) {
        SOSRingGenerationSign(ring, userPrivKey, fpi, error);
        secnotice("ring", "Generation signed updated signatures on peerinfo");
    }

    return result;
}


SOSFullPeerInfoRef SOSRingGetiCloudFullPeerInfoRef(SOSCircleRef circle, SOSRingRef ring) {
    __block SOSFullPeerInfoRef cloud_full_peer = NULL;
    SOSRingForEachActivePeer(circle, ring, ^(SOSPeerInfoRef peer) {
        if (SOSPeerInfoIsCloudIdentity(peer)) {
            if (cloud_full_peer == NULL) {
                CFErrorRef localError = NULL;
                cloud_full_peer = SOSFullPeerInfoCreateCloudIdentity(kCFAllocatorDefault, peer, &localError);

                if (localError) {
                    secerror("Found cloud peer in ring but can't make full peer: %@", localError);
                    CFReleaseNull(localError);
                }

            } else {
                secerror("More than one cloud identity found in ring: %@", ring);
            }
        }
    });
    return cloud_full_peer;
}


CFMutableArrayRef SOSRingCopyConcurringPeers(SOSRingRef ring, CFErrorRef* error) {
    SOSRingAssertStable(ring);

    CFMutableArrayRef concurringPeers = CFArrayCreateMutableForCFTypes(kCFAllocatorDefault);

    if (!SOSRingAppendConcurringPeers(ring, concurringPeers, error))
        CFReleaseNull(concurringPeers);

    return concurringPeers;
}


bool SOSRingAppendConcurringPeers(SOSCircleRef circle, SOSRingRef ring, CFMutableArrayRef appendHere, CFErrorRef *error) {
    SOSRingForEachActivePeer(circle, ring, ^(SOSPeerInfoRef peer) {
        CFErrorRef localError = NULL;
        if (SOSRingVerifyPeerSigned(ring, peer, &localError)) {
            CFArrayAppendValue(appendHere, peer);
        } else if (error != NULL) {
            secerror("Error checking concurrence: %@", localError);
        }
        CFReleaseNull(localError);
    });

    return true;
}
#endif

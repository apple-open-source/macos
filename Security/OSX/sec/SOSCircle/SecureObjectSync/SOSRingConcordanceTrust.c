//
//  SOSRingConcordanceTrust.c
//  sec
//
//  Created by Richard Murphy on 3/15/15.
//
//

#include <AssertMacros.h>

#include <Security/SecureObjectSync/SOSInternal.h>
#include <Security/SecureObjectSync/SOSPeer.h>
#include <Security/SecureObjectSync/SOSPeerInfoInternal.h>
#include <Security/SecureObjectSync/SOSPeerInfoCollections.h>
#include <Security/SecureObjectSync/SOSCircle.h>
#include <Security/SecFramework.h>

#include <Security/SecKey.h>
#include <Security/SecKeyPriv.h>
#include <CoreFoundation/CoreFoundation.h>

#include <utilities/SecCFWrappers.h>

//#include "ckdUtilities.h"

#include <corecrypto/ccder.h>
#include <corecrypto/ccdigest.h>
#include <corecrypto/ccsha2.h>


#include <utilities/der_plist.h>
#include <utilities/der_plist_internal.h>
#include <corecrypto/ccder.h>
#include <utilities/der_date.h>

#include <stdlib.h>
#include <assert.h>

#include "SOSRing.h"
#include "SOSRingUtils.h"

static inline CFDictionaryRef SOSPeerInfoDictionaryCreate(CFSetRef peers) {
    size_t n = CFSetGetCount(peers);
    SOSPeerInfoRef  peerInfos[n];
    CFStringRef     peerIDs[n];
    CFSetGetValues(peers, (const void **) peerInfos);
    for(size_t i = 0; i < n; i++) peerIDs[i] = SOSPeerInfoGetPeerID(peerInfos[i]);
    return CFDictionaryCreate(NULL, (const void **)peerIDs, (const void **)peerInfos, n, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
}

static inline SOSConcordanceStatus CheckPeerStatus(CFStringRef peerID, SOSPeerInfoRef peer, SOSRingRef ring, SecKeyRef userPub, CFErrorRef *error) {
    SOSConcordanceStatus result = kSOSConcordanceNoPeer;
    SecKeyRef pubKey = NULL;

    require_action_quiet(peer, exit, result = kSOSConcordanceNoPeer);
    pubKey = SOSPeerInfoCopyPubKey(peer, error);
    require_quiet(pubKey, exit);
    require_action_quiet(SOSRingHasPeerID(ring, peerID), exit, result = kSOSConcordanceNoPeer);
    require_action_quiet(SOSPeerInfoApplicationVerify(peer, userPub, NULL), exit, result = kSOSConcordanceNoPeer);
    require_action_quiet(SOSRingVerifySignatureExists(ring, pubKey, error), exit, result = kSOSConcordanceNoPeerSig);
    require_action_quiet(SOSRingVerify(ring, pubKey, error), exit, result = kSOSConcordanceBadPeerSig);

    result = kSOSConcordanceTrusted;

exit:
    CFReleaseNull(pubKey);
    return result;
}

static inline SOSConcordanceStatus CombineStatus(SOSConcordanceStatus status1, SOSConcordanceStatus status2)
{
    if (status1 == kSOSConcordanceTrusted || status2 == kSOSConcordanceTrusted)
        return kSOSConcordanceTrusted;

    if (status1 == kSOSConcordanceBadPeerSig || status2 == kSOSConcordanceBadPeerSig)
        return kSOSConcordanceBadPeerSig;

    if (status1 == kSOSConcordanceNoPeerSig || status2 == kSOSConcordanceNoPeerSig)
        return kSOSConcordanceNoPeerSig;

    return status1;
}

SOSConcordanceStatus GetSignersStatus(CFSetRef peers, SOSRingRef signersRing, SOSRingRef statusRing,
                                                    SecKeyRef userPubkey, CFStringRef excludePeerID, CFErrorRef *error) {
    CFDictionaryRef ringPeerInfos = SOSPeerInfoDictionaryCreate(peers);
    __block SOSConcordanceStatus status = kSOSConcordanceNoPeer;
    SOSRingForEachPeerID(signersRing, ^(CFStringRef peerID) {
        SOSPeerInfoRef pi = (SOSPeerInfoRef) CFDictionaryGetValue(ringPeerInfos, peerID);
        SOSConcordanceStatus peerStatus = CheckPeerStatus(peerID, pi, statusRing, userPubkey, error);

        secnotice("ring", "concordance-signer-status: %@ -> %d", peerID, peerStatus);

        if (peerStatus == kSOSConcordanceNoPeerSig &&
            (CFEqualSafe(SOSPeerInfoGetPeerID(pi), excludePeerID) || SOSPeerInfoIsCloudIdentity(pi)))
            peerStatus = kSOSConcordanceNoPeer;

        status = CombineStatus(status, peerStatus);
    });

    return status;
}


SOSConcordanceStatus GetSignersStatus_Transitive(CFSetRef peers, SOSRingRef signersRing, SOSRingRef statusRing,
                                                 SecKeyRef userPubkey, CFStringRef excludePeerID, CFErrorRef *error) {
    __block SOSConcordanceStatus status = kSOSConcordanceNoPeer;
    
    CFSetForEach(peers, ^(const void *value) {
        SOSPeerInfoRef pi = (SOSPeerInfoRef) value;
        CFStringRef peerID = SOSPeerInfoGetPeerID(pi);
        if(SOSRingHasPeerWithID(statusRing, peerID, NULL)) {
            SOSConcordanceStatus peerStatus = CheckPeerStatus(peerID, pi, statusRing, userPubkey, error);
            
            if (peerStatus == kSOSConcordanceNoPeerSig &&
                (CFEqualSafe(SOSPeerInfoGetPeerID(pi), excludePeerID) || SOSPeerInfoIsCloudIdentity(pi)))
                peerStatus = kSOSConcordanceNoPeer;
            
            status = CombineStatus(status, peerStatus);
        }
    });
    
    return status;
}


SOSConcordanceStatus SOSRingUserKeyConcordanceTrust(SOSFullPeerInfoRef me, CFSetRef peers, SOSRingRef knownRing, SOSRingRef proposedRing,
                                                    SecKeyRef knownPubkey, SecKeyRef userPubkey,
                                                    CFStringRef excludePeerID, CFErrorRef *error) {
    if(userPubkey == NULL) {
        SOSCreateError(kSOSErrorPublicKeyAbsent, CFSTR("Concordance with no public key"), NULL, error);
        return kSOSConcordanceNoUserKey;
    }

    if (SOSRingIsEmpty_Internal(proposedRing)) {
        return kSOSConcordanceTrusted;
    }

    if(!SOSRingVerifySignatureExists(proposedRing, userPubkey, error)) {
        SOSCreateError(kSOSErrorBadSignature, CFSTR("No public signature"), (error != NULL) ? *error : NULL, error);
        return kSOSConcordanceNoUserSig;
    }

    if(!SOSRingVerify(proposedRing, userPubkey, error)) {
        SOSCreateError(kSOSErrorBadSignature, CFSTR("Bad public signature"), (error != NULL) ? *error : NULL, error);
        return kSOSConcordanceBadUserSig;
    }

    if (SOSRingIsEmpty_Internal(knownRing) || SOSRingIsOffering_Internal(proposedRing)) {
        return GetSignersStatus(peers, proposedRing, proposedRing, userPubkey, NULL, error);
    }

    if(SOSRingIsOlderGeneration(proposedRing, knownRing)) {
        SOSCreateError(kSOSErrorReplay, CFSTR("Bad generation"), NULL, error);
        return kSOSConcordanceGenOld;
    }

    if(knownPubkey == NULL) knownPubkey = userPubkey;
    if(!SOSRingVerify(knownRing, knownPubkey, error)) knownPubkey = userPubkey;
    return GetSignersStatus(peers, knownRing, proposedRing, knownPubkey, CFSTR("novalue"), error);
}


SOSConcordanceStatus SOSRingPeerKeyConcordanceTrust(SOSFullPeerInfoRef me, CFSetRef peers, SOSRingRef knownRing, SOSRingRef proposedRing,
                                                    __unused SecKeyRef knownPubkey, SecKeyRef userPubkey,
                                                    CFStringRef excludePeerID, CFErrorRef *error) {
    if(userPubkey == NULL) {
        SOSCreateError(kSOSErrorPublicKeyAbsent, CFSTR("Concordance with no public key - need to validate application"), NULL, error);
        return kSOSConcordanceNoUserKey;
    }

    if (SOSRingIsEmpty_Internal(proposedRing)) {
        secnotice("ring", "ring empty -> trusted");
        return kSOSConcordanceTrusted;
    }

    if (SOSRingIsEmpty_Internal(knownRing) || SOSRingIsOffering_Internal(proposedRing)) {
        return GetSignersStatus(peers, proposedRing, proposedRing, userPubkey, NULL, error);
    }

    if(SOSRingIsOlderGeneration(proposedRing, knownRing)) {
        SOSCreateError(kSOSErrorReplay, CFSTR("Bad generation"), NULL, error);
        return kSOSConcordanceGenOld;
    }
    return GetSignersStatus(peers, knownRing, proposedRing, userPubkey, excludePeerID, error);
}

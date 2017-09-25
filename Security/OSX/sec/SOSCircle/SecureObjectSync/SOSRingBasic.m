//
//  SOSRingBasic.c
//  sec
//
//  Created by Richard Murphy on 3/3/15.
//
//

#include "SOSRingBasic.h"

#include <AssertMacros.h>

#include <Security/SecureObjectSync/SOSInternal.h>
#include <Security/SecureObjectSync/SOSPeerInfoInternal.h>
#include <Security/SecureObjectSync/SOSPeerInfoCollections.h>
#include <Security/SecureObjectSync/SOSCircle.h>
#include <Security/SecFramework.h>

#include <Security/SecKey.h>
#include <Security/SecKeyPriv.h>
#include <CoreFoundation/CoreFoundation.h>

#include <utilities/SecCFWrappers.h>

#include <stdlib.h>
#include <assert.h>

#include "SOSRingUtils.h"
#include "SOSRingTypes.h"

// MARK: Basic Ring Ops

SOSRingRef SOSRingCreate_Basic(CFStringRef name, CFStringRef myPeerID, CFErrorRef *error) {
    return SOSRingCreate_ForType(name, kSOSRingBase, myPeerID, error);
}

bool SOSRingResetToEmpty_Basic(SOSRingRef ring, CFStringRef myPeerID, CFErrorRef *error) {
    return SOSRingResetToEmpty_Internal(ring, error) && SOSRingSetLastModifier(ring, myPeerID);
}

bool SOSRingResetToOffering_Basic(SOSRingRef ring, SecKeyRef user_privkey, SOSFullPeerInfoRef requestor, CFErrorRef *error) {
    CFStringRef myPeerID = SOSPeerInfoGetPeerID(SOSFullPeerInfoGetPeerInfo(requestor));
    SecKeyRef priv = SOSFullPeerInfoCopyDeviceKey(requestor, error);
    bool retval = priv && myPeerID &&
    SOSRingResetToEmpty_Internal(ring, error) &&
    SOSRingAddPeerID(ring, myPeerID) &&
    SOSRingSetLastModifier(ring, myPeerID) &&
    SOSRingGenerationSign_Internal(ring, priv, error);
    if(user_privkey) SOSRingConcordanceSign_Internal(ring, user_privkey, error);
    CFReleaseNull(priv);
    return retval;
}

SOSRingStatus SOSRingDeviceIsInRing_Basic(SOSRingRef ring, CFStringRef peerID) {
    if(SOSRingHasPeerID(ring, peerID)) return kSOSRingMember;
    if(SOSRingHasApplicant(ring, peerID)) return kSOSRingApplicant;
    if(SOSRingHasRejection(ring, peerID)) return kSOSRingReject;
    return kSOSRingNotInRing;
}

bool SOSRingApply_Basic(SOSRingRef ring, SecKeyRef user_pubkey, SOSFullPeerInfoRef requestor, CFErrorRef *error) {
    bool retval = false;
    CFStringRef myPeerID = SOSPeerInfoGetPeerID(SOSFullPeerInfoGetPeerInfo(requestor));
    SecKeyRef priv = SOSFullPeerInfoCopyDeviceKey(requestor, error);
    require_action_quiet(SOSRingDeviceIsInRing_Basic(ring, myPeerID) == kSOSRingNotInRing, errOut, secnotice("ring", "Already associated with ring"));
    retval = priv && myPeerID &&
        SOSRingAddPeerID(ring, myPeerID) &&
        SOSRingSetLastModifier(ring, myPeerID) &&
        SOSRingGenerationSign_Internal(ring, priv, error);
    CFReleaseNull(priv);
errOut:
    return retval;

}

bool SOSRingWithdraw_Basic(SOSRingRef ring, SecKeyRef user_privkey, SOSFullPeerInfoRef requestor, CFErrorRef *error) {
    CFStringRef myPeerID = SOSPeerInfoGetPeerID(SOSFullPeerInfoGetPeerInfo(requestor));
    if(SOSRingHasPeerID(ring, myPeerID)) {
        SOSRingRemovePeerID(ring, myPeerID);
    } else if(SOSRingHasApplicant(ring, myPeerID)) {
        SOSRingRemoveApplicant(ring, myPeerID);
    } else if(SOSRingHasRejection(ring, myPeerID)) {
        SOSRingRemoveRejection(ring, myPeerID);
    } else {
        SOSCreateError(kSOSErrorPeerNotFound, CFSTR("Not associated with Ring"), NULL, error);
        return false;
    }
    SOSRingSetLastModifier(ring, myPeerID);

    SecKeyRef priv = SOSFullPeerInfoCopyDeviceKey(requestor, error);
    SOSRingGenerationSign_Internal(ring, priv, error);
    if(user_privkey) SOSRingConcordanceSign_Internal(ring, user_privkey, error);
    CFReleaseNull(priv);
    return true;
}

bool SOSRingGenerationSign_Basic(SOSRingRef ring, SecKeyRef user_privkey, SOSFullPeerInfoRef requestor, CFErrorRef *error) {
    CFStringRef myPeerID = SOSPeerInfoGetPeerID(SOSFullPeerInfoGetPeerInfo(requestor));
    SecKeyRef priv = SOSFullPeerInfoCopyDeviceKey(requestor, error);
    bool retval = priv && myPeerID &&
    SOSRingSetLastModifier(ring, myPeerID) &&
    SOSRingGenerationSign_Internal(ring, priv, error);
    if(user_privkey) SOSRingConcordanceSign_Internal(ring, user_privkey, error);
    CFReleaseNull(priv);
    return retval;
}

bool SOSRingConcordanceSign_Basic(SOSRingRef ring, SOSFullPeerInfoRef requestor, CFErrorRef *error) {
    CFStringRef myPeerID = SOSPeerInfoGetPeerID(SOSFullPeerInfoGetPeerInfo(requestor));
    SecKeyRef priv = SOSFullPeerInfoCopyDeviceKey(requestor, error);
    bool retval = priv && myPeerID &&
    SOSRingSetLastModifier(ring, myPeerID) &&
    SOSRingConcordanceSign_Internal(ring, priv, error);
    CFReleaseNull(priv);
    return retval;
}

bool SOSRingSetPayload_Basic(SOSRingRef ring, SecKeyRef user_privkey, CFDataRef payload, SOSFullPeerInfoRef requestor, CFErrorRef *error) {
    CFStringRef myPeerID = SOSPeerInfoGetPeerID(SOSFullPeerInfoGetPeerInfo(requestor));
    SecKeyRef priv = SOSFullPeerInfoCopyDeviceKey(requestor, error);
    bool retval = priv && myPeerID &&
    SOSRingSetLastModifier(ring, myPeerID) &&
    SOSRingSetPayload_Internal(ring, payload) &&
    SOSRingGenerationSign_Internal(ring, priv, error);
    if(user_privkey) SOSRingConcordanceSign_Internal(ring, user_privkey, error);
    CFReleaseNull(priv);
    return retval;
}

CFDataRef SOSRingGetPayload_Basic(SOSRingRef ring, CFErrorRef *error) {
    return SOSRingGetPayload_Internal(ring);
}


ringFuncStruct basic = {
    "Basic",
    1,
    SOSRingCreate_Basic,
    SOSRingResetToEmpty_Basic,
    SOSRingResetToOffering_Basic,
    SOSRingDeviceIsInRing_Basic,
    SOSRingApply_Basic,
    SOSRingWithdraw_Basic,
    SOSRingGenerationSign_Basic,
    SOSRingConcordanceSign_Basic,
    SOSRingPeerKeyConcordanceTrust,
    NULL,
    NULL,
    SOSRingSetPayload_Basic,
    SOSRingGetPayload_Basic,
};

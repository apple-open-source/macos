//
//  SOSRingV0.c
//  sec
//
//  Created by Richard Murphy on 3/5/15.
//
//

#include "SOSRingV0.h"

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

// MARK: V0 Ring Ops - same operation as V0 Circles

static SOSRingRef SOSRingCreate_V0(CFStringRef name, CFStringRef myPeerID, CFErrorRef *error) {
    SOSRingRef retval = NULL;
    retval = SOSRingCreate_Internal(name, 0, error);
    if(!retval) return NULL;
    SOSRingSetLastModifier(retval, myPeerID);
    return retval;
}

static bool SOSRingResetToEmpty_V0(SOSRingRef ring, CFStringRef myPeerID, CFErrorRef *error) {
    return SOSRingResetToEmpty_Internal(ring, error) && SOSRingSetLastModifier(ring, myPeerID);
}

static bool SOSRingResetToOffering_V0(SOSRingRef ring, SecKeyRef user_privkey, SOSFullPeerInfoRef requestor, CFErrorRef *error) {
    CFStringRef myPeerID = SOSPeerInfoGetPeerID(SOSFullPeerInfoGetPeerInfo(requestor));
    SecKeyRef priv = SOSFullPeerInfoCopyDeviceKey(requestor, error);
    bool retval = priv && myPeerID &&
    SOSRingResetToEmpty_Internal(ring, error) &&
    SOSRingAddPeerID(ring, myPeerID) &&
    SOSRingSetLastModifier(ring, myPeerID) &&
    SOSRingGenerationSign_Internal(ring, user_privkey, error);
    SOSRingConcordanceSign_Internal(ring, priv, error);
    CFReleaseNull(priv);
    return retval;
}

static SOSRingStatus SOSRingDeviceIsInRing_V0(SOSRingRef ring, CFStringRef peerID) {
    if(SOSRingHasPeerID(ring, peerID)) return kSOSRingMember;
    if(SOSRingHasApplicant(ring, peerID)) return kSOSRingApplicant;
    if(SOSRingHasRejection(ring, peerID)) return kSOSRingReject;
    return kSOSRingNotInRing;
}

static bool SOSRingApply_V0(SOSRingRef ring, SecKeyRef user_privkey, SOSFullPeerInfoRef requestor, CFErrorRef *error) {
    bool retval = false;
    CFStringRef myPeerID = SOSPeerInfoGetPeerID(SOSFullPeerInfoGetPeerInfo(requestor));
    if(SOSRingDeviceIsInRing_V0(ring, myPeerID) == kSOSRingReject) SOSRingRemoveRejection(ring, myPeerID);
    require_action_quiet(SOSRingDeviceIsInRing_V0(ring, myPeerID) == kSOSRingNotInRing, errOut, secnotice("ring", "Already associated with ring"));
    retval = myPeerID &&
    SOSRingAddApplicant(ring, myPeerID) &&
    SOSRingSetLastModifier(ring, myPeerID);
errOut:
    return retval;
}

static bool SOSRingWithdraw_V0(SOSRingRef ring, SecKeyRef user_privkey, SOSFullPeerInfoRef requestor, CFErrorRef *error) {
    CFStringRef myPeerID = SOSPeerInfoGetPeerID(SOSFullPeerInfoGetPeerInfo(requestor));
    SOSRingSetLastModifier(ring, myPeerID);
    if(SOSRingHasPeerID(ring, myPeerID)) {
        SOSRingRemovePeerID(ring, myPeerID);// Maybe we need a retired peerID list?
        SecKeyRef priv = SOSFullPeerInfoCopyDeviceKey(requestor, error);
        SOSRingGenerationSign_Internal(ring, priv, error);
        if(user_privkey) SOSRingGenerationSign_Internal(ring, user_privkey, error);
        CFReleaseNull(priv);
    } else if(SOSRingHasApplicant(ring, myPeerID)) {
        SOSRingRemoveApplicant(ring, myPeerID);
    } else if(SOSRingHasRejection(ring, myPeerID)) {
        SOSRingRemoveRejection(ring, myPeerID);
    } else {
        SOSCreateError(kSOSErrorPeerNotFound, CFSTR("Not associated with Ring"), NULL, error);
        return false;
    }

    return true;
}

static bool SOSRingGenerationSign_V0(SOSRingRef ring, SecKeyRef user_privkey, SOSFullPeerInfoRef requestor, CFErrorRef *error) {
    CFStringRef myPeerID = SOSPeerInfoGetPeerID(SOSFullPeerInfoGetPeerInfo(requestor));
    SecKeyRef priv = SOSFullPeerInfoCopyDeviceKey(requestor, error);
    bool retval = priv && myPeerID &&
    SOSRingSetLastModifier(ring, myPeerID) &&
    SOSRingGenerationSign_Internal(ring, priv, error);
    if(user_privkey) SOSRingGenerationSign_Internal(ring, user_privkey, error);
    CFReleaseNull(priv);
    return retval;
}

static bool SOSRingConcordanceSign_V0(SOSRingRef ring, SOSFullPeerInfoRef requestor, CFErrorRef *error) {
    CFStringRef myPeerID = SOSPeerInfoGetPeerID(SOSFullPeerInfoGetPeerInfo(requestor));
    SecKeyRef priv = SOSFullPeerInfoCopyDeviceKey(requestor, error);
    bool retval = priv && myPeerID &&
    SOSRingSetLastModifier(ring, myPeerID) &&
    SOSRingConcordanceSign_Internal(ring, priv, error);
    CFReleaseNull(priv);
    return retval;
}


__unused static bool SOSRingSetPayload_V0(SOSRingRef ring, SecKeyRef user_privkey, CFDataRef payload, SOSFullPeerInfoRef requestor, CFErrorRef *error) {
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

__unused static CFDataRef SOSRingGetPayload_V0(SOSRingRef ring, CFErrorRef *error) {
    return SOSRingGetPayload_Internal(ring);
}


ringFuncStruct basic = {
    "V0",
    1,
    SOSRingCreate_V0,
    SOSRingResetToEmpty_V0,
    SOSRingResetToOffering_V0,
    SOSRingDeviceIsInRing_V0,
    SOSRingApply_V0,
    SOSRingWithdraw_V0,
    SOSRingGenerationSign_V0,
    SOSRingConcordanceSign_V0,
    SOSRingUserKeyConcordanceTrust,
    NULL,
    NULL
};

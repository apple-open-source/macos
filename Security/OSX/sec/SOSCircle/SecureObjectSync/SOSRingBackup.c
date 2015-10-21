//
//  SOSRingBasic.c
//  sec
//
//  Created by Richard Murphy on 3/3/15.
//
//

#include "SOSRingBackup.h"

#include <AssertMacros.h>

#include <Security/SecureObjectSync/SOSInternal.h>
#include <Security/SecureObjectSync/SOSPeerInfoInternal.h>
#include <Security/SecureObjectSync/SOSPeerInfoCollections.h>
#include <Security/SecureObjectSync/SOSCircle.h>
#include <Security/SecureObjectSync/SOSViews.h>
#include <Security/SecFramework.h>

#include <Security/SecKey.h>
#include <Security/SecKeyPriv.h>
#include <CoreFoundation/CoreFoundation.h>

#include <utilities/SecCFWrappers.h>

#include <stdlib.h>
#include <assert.h>

#include "SOSRingUtils.h"
#include "SOSRingTypes.h"

// MARK: Backup Ring Ops

static SOSRingRef SOSRingCreate_Backup(CFStringRef name, CFStringRef myPeerID, CFErrorRef *error) {
    SOSRingRef retval = NULL;
    retval = SOSRingCreate_Internal(name, kSOSRingBackup, error);
    if(!retval) return NULL;
    SOSRingSetLastModifier(retval, myPeerID);
    return retval;
}

static bool SOSRingResetToEmpty_Backup(SOSRingRef ring, CFStringRef myPeerID, CFErrorRef *error) {
    return SOSRingResetToEmpty_Internal(ring, error) && SOSRingSetLastModifier(ring, myPeerID);
}

static bool SOSRingResetToOffering_Backup(SOSRingRef ring, SecKeyRef user_privkey, SOSFullPeerInfoRef requestor, CFErrorRef *error) {
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

static SOSRingStatus SOSRingDeviceIsInRing_Backup(SOSRingRef ring, CFStringRef peerID) {
    if(SOSRingHasPeerID(ring, peerID)) return kSOSRingMember;
    if(SOSRingHasApplicant(ring, peerID)) return kSOSRingApplicant;
    if(SOSRingHasRejection(ring, peerID)) return kSOSRingReject;
    return kSOSRingNotInRing;
}

static bool SOSRingApply_Backup(SOSRingRef ring, SecKeyRef user_pubkey, SOSFullPeerInfoRef requestor, CFErrorRef *error) {
    bool retval = false;
    CFStringRef myPeerID = SOSPeerInfoGetPeerID(SOSFullPeerInfoGetPeerInfo(requestor));
    SecKeyRef priv = SOSFullPeerInfoCopyDeviceKey(requestor, error);
    require_action_quiet(SOSRingDeviceIsInRing_Backup(ring, myPeerID) == kSOSRingNotInRing, errOut, secnotice("ring", "Already associated with ring"));
    retval = priv && myPeerID &&
        SOSRingAddPeerID(ring, myPeerID) &&
        SOSRingSetLastModifier(ring, myPeerID) &&
        SOSRingGenerationSign_Internal(ring, priv, error);
    CFReleaseNull(priv);
errOut:
    return retval;

}

static bool SOSRingWithdraw_Backup(SOSRingRef ring, SecKeyRef user_privkey, SOSFullPeerInfoRef requestor, CFErrorRef *error) {
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

static bool SOSRingGenerationSign_Backup(SOSRingRef ring, SecKeyRef user_privkey, SOSFullPeerInfoRef requestor, CFErrorRef *error) {
    CFStringRef myPeerID = SOSPeerInfoGetPeerID(SOSFullPeerInfoGetPeerInfo(requestor));
    SecKeyRef priv = SOSFullPeerInfoCopyDeviceKey(requestor, error);
    bool retval = priv && myPeerID &&
    SOSRingSetLastModifier(ring, myPeerID) &&
    SOSRingGenerationSign_Internal(ring, priv, error);
    if(user_privkey) SOSRingConcordanceSign_Internal(ring, user_privkey, error);
    CFReleaseNull(priv);
    return retval;
}

// Make sure all the peers in the ring have access to the ring views
static bool SOSBackupRingPeersInViews(CFSetRef peers, SOSRingRef ring) {
    CFSetRef ringViews = SOSBackupRingGetViews(ring, NULL);
    if(!ringViews) return false;
    __block bool retval = true;
    SOSRingForEachPeerID(ring, ^(CFStringRef peerID) {
        SOSPeerInfoRef peerInfo = SOSPeerInfoSetFindByID(peers, peerID);
        if(peerInfo) {
            CFSetForEach(ringViews, ^(const void *value) {
                if(!SOSPeerInfoIsViewPermitted(peerInfo, (CFStringRef) value)) retval = false;
            });
        } else {
            retval = false;
        }
    });
    return retval;
}

static bool CFSetIsSubset(CFSetRef little, CFSetRef big) {
    __block bool retval = true;
    CFSetForEach(little, ^(const void *value) {
        if(!CFSetContainsValue(big, value)) retval = false;
    });
    return retval;
}

// Make sure that the ring includes me if I'm enabled for its view.
static SOSConcordanceStatus SOSBackupRingEvaluateMyInclusion(SOSRingRef ring, SOSFullPeerInfoRef me) {
    bool shouldBeInRing = false;
    bool amInThisRing = false;

    if (me) {
        SOSPeerInfoRef pi = SOSFullPeerInfoGetPeerInfo(me);
        CFStringRef peerID = SOSPeerInfoGetPeerID(pi);
        CFSetRef ringViews = SOSRingGetBackupViewset_Internal(ring);
        CFSetRef piViews = SOSPeerInfoGetPermittedViews(pi);
        shouldBeInRing = CFSetIsSubset(ringViews, piViews);
        amInThisRing = SOSRingHasPeerWithID(ring, peerID, NULL);
    }

    if(shouldBeInRing && !amInThisRing) return kSOSConcordanceMissingMe;
    if(!shouldBeInRing && amInThisRing) return kSOSConcordanceImNotWorthy;
    return kSOSConcordanceTrusted;
}

static SOSConcordanceStatus SOSRingPeerKeyConcordanceTrust_Backup(SOSFullPeerInfoRef me, CFSetRef peers, SOSRingRef knownRing, SOSRingRef proposedRing,
                                                    __unused SecKeyRef knownPubkey, SecKeyRef userPubkey,
                                                    CFStringRef excludePeerID, CFErrorRef *error) {
    if(userPubkey == NULL) {
        SOSCreateError(kSOSErrorPublicKeyAbsent, CFSTR("Concordance with no public key - need to validate application"), NULL, error);
        return kSOSConcordanceNoUserKey;
    }
    
    if(SOSRingIsOlderGeneration(knownRing, proposedRing)) {
        SOSCreateError(kSOSErrorReplay, CFSTR("Bad generation"), NULL, error);
        return kSOSConcordanceGenOld;
    }
    
    
    if (SOSRingIsEmpty_Internal(proposedRing)) {
        return kSOSConcordanceTrusted;
    }

    SOSConcordanceStatus localstatus = SOSBackupRingEvaluateMyInclusion(proposedRing, me);
    if(localstatus == kSOSConcordanceMissingMe) {
        SOSCreateError(kSOSErrorReplay, CFSTR("Improper exclusion of this peer"), NULL, error);
        return localstatus;
    }
    
    if(localstatus == kSOSConcordanceImNotWorthy) {
        SOSCreateError(kSOSErrorReplay, CFSTR("Improper inclusion of this peer"), NULL, error);
        return localstatus;
    }
    
    if(!SOSBackupRingPeersInViews(peers, proposedRing)) {
        return kSOSConcordanceInvalidMembership;
    }

    return GetSignersStatus_Transitive(peers, knownRing, proposedRing, userPubkey, excludePeerID, error);
}


static bool SOSRingConcordanceSign_Backup(SOSRingRef ring, SOSFullPeerInfoRef requestor, CFErrorRef *error) {
    CFStringRef myPeerID = SOSPeerInfoGetPeerID(SOSFullPeerInfoGetPeerInfo(requestor));
    SecKeyRef priv = SOSFullPeerInfoCopyDeviceKey(requestor, error);
    bool retval = priv && myPeerID &&
    SOSRingSetLastModifier(ring, myPeerID) &&
    SOSRingConcordanceSign_Internal(ring, priv, error);
    CFReleaseNull(priv);
    return retval;
}

static bool SOSRingSetPayload_Backup(SOSRingRef ring, SecKeyRef user_privkey, CFDataRef payload, SOSFullPeerInfoRef requestor, CFErrorRef *error) {
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

static CFDataRef SOSRingGetPayload_Backup(SOSRingRef ring, CFErrorRef *error) {
    return SOSRingGetPayload_Internal(ring);
}

bool SOSBackupRingSetViews(SOSRingRef ring, SOSFullPeerInfoRef requestor, CFSetRef viewSet, CFErrorRef *error) {
    CFStringRef myPeerID = SOSPeerInfoGetPeerID(SOSFullPeerInfoGetPeerInfo(requestor));
    SecKeyRef priv = SOSFullPeerInfoCopyDeviceKey(requestor, error);
    bool retval = priv && myPeerID &&
    SOSRingSetLastModifier(ring, myPeerID) &&
    SOSRingSetBackupViewset_Internal(ring, viewSet) &&
    SOSRingGenerationSign_Internal(ring, priv, error);
    CFReleaseNull(priv);
    return retval;
}

CFSetRef SOSBackupRingGetViews(SOSRingRef ring, CFErrorRef *error) {
    return SOSRingGetBackupViewset_Internal(ring);
}

ringFuncStruct backup = {
    "Backup",
    1,
    SOSRingCreate_Backup,
    SOSRingResetToEmpty_Backup,
    SOSRingResetToOffering_Backup,
    SOSRingDeviceIsInRing_Backup,
    SOSRingApply_Backup,
    SOSRingWithdraw_Backup,
    SOSRingGenerationSign_Backup,
    SOSRingConcordanceSign_Backup,
    SOSRingPeerKeyConcordanceTrust_Backup,
    NULL,
    NULL,
    SOSRingSetPayload_Backup,
    SOSRingGetPayload_Backup,
};

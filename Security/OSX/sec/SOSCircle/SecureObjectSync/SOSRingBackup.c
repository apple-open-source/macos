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
#include "SOSRingBasic.h"

// MARK: Backup Ring Ops

static SOSRingRef SOSRingCreate_Backup(CFStringRef name, CFStringRef myPeerID, CFErrorRef *error) {
    return SOSRingCreate_ForType(name, kSOSRingBackup, myPeerID, error);
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
    
    if(SOSRingIsOlderGeneration(proposedRing, knownRing)) {
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
    SOSRingResetToEmpty_Basic,
    SOSRingResetToOffering_Basic,
    SOSRingDeviceIsInRing_Basic,
    SOSRingApply_Basic,
    SOSRingWithdraw_Basic,
    SOSRingGenerationSign_Basic,
    SOSRingConcordanceSign_Basic,
    SOSRingPeerKeyConcordanceTrust_Backup,
    NULL,
    NULL,
    SOSRingSetPayload_Basic,
    SOSRingGetPayload_Basic,
};

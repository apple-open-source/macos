//
//  SOSAccountHSAJoin.c
//  sec
//
//  Created by Richard Murphy on 3/23/15.
//
//

#include "SOSAccountHSAJoin.h"
#include "SOSAccountPriv.h"
#include <Security/SecureObjectSync/SOSPeerInfo.h>
#include <Security/SecureObjectSync/SOSPeerInfoV2.h>
#include <Security/SecureObjectSync/SOSPeerInfoPriv.h>
#include <Security/SecureObjectSync/SOSPeerInfoCollections.h>
#include <Security/SecureObjectSync/SOSTransportCircle.h>
#include <Security/SecureObjectSync/SOSTransportMessage.h>
#include <Security/SecureObjectSync/SOSTransportMessageIDS.h>
#include <Security/SecureObjectSync/SOSKVSKeys.h>
#include <Security/SecureObjectSync/SOSTransport.h>
#include <Security/SecureObjectSync/SOSTransportKeyParameter.h>
#include <Security/SecureObjectSync/SOSTransportKeyParameterKVS.h>
#include <Security/SecureObjectSync/SOSEngine.h>
#include <Security/SecureObjectSync/SOSPeerCoder.h>
#include <Security/SecureObjectSync/SOSInternal.h>
#include <sys/unistd.h>

const CFStringRef kSOSHsaPreApprovedPeerKeyInfo = CFSTR("HSAPreApprovedPeer");

const CFStringRef kSOSHsaCrKeyUUID = CFSTR("HSAUUID");
const CFStringRef kSOSHsaCrKeyDescription = CFSTR("HSADESC");

CFMutableSetRef SOSAccountCopyPreApprovedHSA2Info(SOSAccountRef account) {
    CFMutableSetRef preApprovedPeers = (CFMutableSetRef) SOSAccountGetValue(account, kSOSHsaPreApprovedPeerKeyInfo, NULL);
    if(preApprovedPeers) {
        preApprovedPeers = CFSetCreateMutableCopy(NULL, 0, preApprovedPeers);
    } else {
        preApprovedPeers = CFSetCreateMutableForCFTypes(NULL);
    }
    return preApprovedPeers;
}

static bool sosAccountSetPreApprovedInfo(SOSAccountRef account, CFStringRef peerID, CFErrorRef *error) {
    bool retval = false;
    CFMutableSetRef preApprovedPeers = SOSAccountCopyPreApprovedHSA2Info(account);
    require_action_quiet(preApprovedPeers, errOut, SOSCreateError(kSOSErrorAllocationFailure, CFSTR("Can't Alloc Pre-Approved Peers Set"), NULL, error));
    CFSetSetValue(preApprovedPeers, peerID);
    require(SOSAccountSetValue(account, kSOSHsaPreApprovedPeerKeyInfo, preApprovedPeers, error), errOut);
    retval = true;
errOut:
    CFReleaseNull(preApprovedPeers);
    return retval;
}

bool SOSAccountSetHSAPubKeyExpected(SOSAccountRef account, CFDataRef pubKeyBytes, CFErrorRef *error) {
    bool retval = false;
    SecKeyRef publicKey = SecKeyCreateFromPublicBytes(NULL, kSecECDSAAlgorithmID, CFDataGetBytePtr(pubKeyBytes), CFDataGetLength(pubKeyBytes));
    CFStringRef peerID = SOSCopyIDOfKey(publicKey, error);
    require(sosAccountSetPreApprovedInfo(account, peerID, error), errOut);
    retval = true;
errOut:
    CFReleaseNull(publicKey);
    CFReleaseNull(peerID);
    return retval;
}

bool SOSAccountVerifyAndAcceptHSAApplicants(SOSAccountRef account, SOSCircleRef newCircle, CFErrorRef *error) {
    SOSFullPeerInfoRef fpi = account->my_identity;
    __block bool circleChanged = false;
    CFMutableSetRef approvals = SOSAccountCopyPreApprovedHSA2Info(account);
    if(approvals && CFSetGetCount(approvals) > 0) {
        SOSCircleForEachApplicant(newCircle, ^(SOSPeerInfoRef peer) {
            CFStringRef peerID = SOSPeerInfoGetPeerID(peer);
            if(CFSetContainsValue(approvals, peerID)) {
                SOSPeerInfoRef copypi = SOSPeerInfoCreateCopy(NULL, peer, NULL);
                circleChanged = SOSCircleAcceptRequest(newCircle, SOSAccountGetPrivateCredential(account, NULL), fpi, copypi, error);
                CFSetRemoveValue(approvals, peerID);
            }
        });
    }
    if(circleChanged) {
        bool local = SOSAccountSetValue(account, kSOSHsaPreApprovedPeerKeyInfo, approvals, error);
        if(!local) secnotice("hsa2approval", "Couldn't clean pre-approved peer list");
    }
    CFReleaseNull(approvals);
    return circleChanged;
}

bool SOSAccountClientPing(SOSAccountRef account) {
    if (account->trusted_circle && account->my_identity
        && SOSFullPeerInfoPing(account->my_identity, NULL)) {
        SOSAccountModifyCircle(account, NULL, ^(SOSCircleRef circle_to_change) {
            secnotice("circleChange", "Calling SOSCircleUpdatePeerInfo for gestalt change");
            return SOSCircleUpdatePeerInfo(circle_to_change, SOSFullPeerInfoGetPeerInfo(account->my_identity));
        });
    }
    
    return true;
}


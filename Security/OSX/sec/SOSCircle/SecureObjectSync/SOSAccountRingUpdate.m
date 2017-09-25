//
//  SOSAccountRingUpdate.c
//  sec
//
//

#include <stdio.h>

#include "SOSAccountPriv.h"
#include <Security/SecureObjectSync/SOSTransportCircleKVS.h>
#include <Security/SecureObjectSync/SOSTransport.h>
#include <Security/SecureObjectSync/SOSViews.h>
#include <Security/SecureObjectSync/SOSRing.h>
#include <Security/SecureObjectSync/SOSRingUtils.h>
#include <Security/SecureObjectSync/SOSPeerInfoCollections.h>
#import <Security/SecureObjectSync/SOSAccountTrust.h>

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


bool SOSAccountIsPeerRetired(SOSAccount* account, CFSetRef peers){
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

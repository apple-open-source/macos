//
//  SOSAccountRingUpdate.c
//  sec
//
//

#include <stdio.h>

#include "SOSAccountPriv.h"
#include "keychain/SecureObjectSync/SOSTransportCircleKVS.h"
#include "keychain/SecureObjectSync/SOSTransport.h"
#include <Security/SecureObjectSync/SOSViews.h>
#include "keychain/SecureObjectSync/SOSRing.h"
#include "keychain/SecureObjectSync/SOSRingUtils.h"
#include "keychain/SecureObjectSync/SOSPeerInfoCollections.h"
#import "keychain/SecureObjectSync/SOSAccountTrust.h"

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

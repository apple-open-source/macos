//
//  SOSAccountTrustClassicRetirement.m
//  Security
//
//  Created by Michelle Auricchio on 12/27/16.
//
//

#import <Foundation/Foundation.h>
#import "Security/SecureObjectSync/SOSAccountTrustClassic.h"
#import "Security/SecureObjectSync/SOSAccountTrustClassic+Retirement.h"
#import "Security/SecureObjectSync/SOSPeerInfoCollections.h"
#import "Security/SecureObjectSync/SOSTransportMessageKVS.h"

@implementation SOSAccountTrustClassic (Retirement)

-(bool) cleanupRetirementTickets:(SOSAccount*)account circle:(SOSCircleRef)circle time:(size_t) seconds err:(CFErrorRef*) error
{
    CFMutableSetRef retirees_to_remove = CFSetCreateMutableForSOSPeerInfosByID(kCFAllocatorDefault);
    
    __block bool success = true;
    
    CFSetForEach((__bridge CFSetRef)(self.retirees), ^(const void *value) {
        SOSPeerInfoRef retiree = (SOSPeerInfoRef) value;
        
        if (retiree) {
            // Remove the entry if it's not a retired peer or if it's retirment ticket has expired AND he's no longer in the circle.
            if (!SOSPeerInfoIsRetirementTicket(retiree) ||
                (SOSPeerInfoRetireRetirementTicket(seconds, retiree) && !SOSCircleHasActivePeer(circle, retiree, NULL))) {
                CFSetAddValue(retirees_to_remove, retiree);
            };
        }
    });
    
    CFMutableArrayRef retirees_to_cleanup = CFArrayCreateMutableForCFTypes(kCFAllocatorDefault);
    CFSetForEach(retirees_to_remove, ^(const void *value) {
        CFArrayAppendValue(retirees_to_cleanup, value);
        CFSetRemoveValue((__bridge CFMutableSetRef)self.retirees, value);
    });
    
    CFReleaseNull(retirees_to_remove);
    
    CFDictionaryRef retirements_to_remove = CFDictionaryCreateForCFTypes(kCFAllocatorDefault,
                                                                         SOSCircleGetName(circle), retirees_to_cleanup,
                                                                         NULL);
    
    CFReleaseNull(retirees_to_cleanup);
    
    success = [account.circle_transport expireRetirementRecords:retirements_to_remove err:error];

    CFReleaseNull(retirements_to_remove);
    
    return success;
}

static inline CFMutableArrayRef CFDictionaryEnsureCFArrayAndGetCurrentValue(CFMutableDictionaryRef dict, CFTypeRef key)
{
    CFMutableArrayRef result = (CFMutableArrayRef) CFDictionaryGetValue(dict, key);
    
    if (!isArray(result)) {
        result = CFArrayCreateMutableForCFTypes(kCFAllocatorDefault);
        CFDictionarySetValue(dict, key, result);
        CFReleaseSafe(result);
    }
    
    return result;
}


-(bool) cleanupAfterPeer:(SOSMessageKVS*)kvsTransport circleTransport:(SOSCircleStorageTransport*)circleTransport seconds:(size_t) seconds circle:(SOSCircleRef) circle cleanupPeer:(SOSPeerInfoRef) cleanupPeer err:(CFErrorRef*) error
{
    bool success = true;
    
    SOSPeerInfoRef myPeerInfo = self.peerInfo;
    require_action_quiet(self.fullPeerInfo && myPeerInfo, xit, SOSErrorCreate(kSOSErrorPeerNotFound, error, NULL, CFSTR("I have no peer")));
    require_quiet(SOSCircleHasActivePeer(circle, self.peerInfo, error), xit);
    
    CFStringRef cleanupPeerID = SOSPeerInfoGetPeerID(cleanupPeer);

    CFStringRef circle_name = SOSCircleGetName(circle);
    
    CFMutableDictionaryRef circleToPeerIDs = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
    CFArrayAppendValue(CFDictionaryEnsureCFArrayAndGetCurrentValue(circleToPeerIDs, circle_name), cleanupPeerID);
    
    CFErrorRef localError = NULL;
    
    if (!(success &= [kvsTransport SOSTransportMessageCleanupAfterPeerMessages:kvsTransport peers:circleToPeerIDs err:&localError])) {
        secnotice("account", "Failed to cleanup after peer %@ messages: %@", cleanupPeerID, localError);
    }
    
    CFReleaseNull(localError);
    
    if((success &= SOSPeerInfoRetireRetirementTicket(seconds, cleanupPeer))) {

        if (!(success &= [circleTransport expireRetirementRecords:circleToPeerIDs err:&localError])) {
            secnotice("account", "Failed to cleanup after peer %@ retirement: %@", cleanupPeerID, localError);
        }
    }
    CFReleaseNull(localError);
    CFReleaseNull(circleToPeerIDs);
    
xit:
    return success;
}

@end

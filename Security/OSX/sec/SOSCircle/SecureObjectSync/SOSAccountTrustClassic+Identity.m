 //
//  SOSAccountTrustClassicIdentity.m
//  Security
//


#import <Foundation/Foundation.h>
#include <AssertMacros.h>
#import "Security/SecureObjectSync/SOSAccountTrustClassic.h"
#import "Security/SecureObjectSync/SOSAccountTrustClassic+Expansion.h"
#import "Security/SecureObjectSync/SOSAccountTrustClassic+Identity.h"
#import "Security/SecureObjectSync/SOSAccountTrustClassic+Circle.h"

#import "Security/SecureObjectSync/SOSViews.h"

@implementation SOSAccountTrustClassic (Identity)

-(bool) updateFullPeerInfo:(SOSAccount*)account minimum:(CFSetRef)minimumViews excluded:(CFSetRef)excludedViews
{
    if (self.trustedCircle && self.fullPeerInfo) {
        if(SOSFullPeerInfoUpdateToCurrent(self.fullPeerInfo, minimumViews, excludedViews)) {
            [self modifyCircle:account.circle_transport err:NULL action:^(SOSCircleRef circle_to_change) {
                secnotice("circleChange", "Calling SOSCircleUpdatePeerInfo for gestalt change");
                return SOSCircleUpdatePeerInfo(circle_to_change, self.peerInfo);
            }];
        }
    }
    
    return true;
}

-(SOSFullPeerInfoRef) getMyFullPeerInfo
{
    return self.trustedCircle ? self.fullPeerInfo : NULL;
}

-(bool) fullPeerInfoVerify:(SecKeyRef) privKey err:(CFErrorRef *)error
{
    if(!self.fullPeerInfo) return false;
    SecKeyRef pubKey = SecKeyCreatePublicFromPrivate(privKey);
    bool retval = SOSPeerInfoApplicationVerify(self.peerInfo, pubKey, error);
    CFReleaseNull(pubKey);
    return retval;
}

-(bool) hasFullPeerInfo:(CFErrorRef*) error
{
    bool hasPeer = false;
    if(![self hasCircle:error]){
        return hasPeer;
    }
    hasPeer = self.fullPeerInfo != NULL;
    
    if (!hasPeer)
        SOSCreateErrorWithFormat(kSOSErrorPeerNotFound, NULL, error, NULL, CFSTR("No peer for circle"));
    
    return hasPeer;
}

-(SOSFullPeerInfoRef) CopyAccountIdentityPeerInfo
{
    return SOSFullPeerInfoCopyFullPeerInfo(self.fullPeerInfo);
}

-(bool) ensureFullPeerAvailable:(CFDictionaryRef)gestalt deviceID:(CFStringRef)deviceID backupKey:(CFDataRef)backup err:(CFErrorRef *) error
{
    require_action_quiet(self.trustedCircle, fail, SOSCreateErrorWithFormat(kSOSErrorNoCircle, NULL, error, NULL, CFSTR("Don't have circle")));
    
    if (self.fullPeerInfo == NULL) {
        CFStringRef keyName = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("ID for %@-%@"), SOSPeerGestaltGetName(gestalt), SOSCircleGetName(self.trustedCircle));
        SecKeyRef full_key = GeneratePermanentFullECKey(256, keyName, error);
        
        NSString* octagonKeyName = [@"Octagon " stringByAppendingString:(__bridge NSString*)keyName];
        SecKeyRef octagonFullKey = GeneratePermanentFullECKey(384, (__bridge CFStringRef)octagonKeyName, error);
        
        if (full_key && octagonFullKey) {
            CFSetRef initialViews = SOSViewCopyViewSet(kViewSetInitial);
            
            self.fullPeerInfo = nil;
            self.fullPeerInfo = SOSFullPeerInfoCreateWithViews(kCFAllocatorDefault, gestalt, backup, initialViews, full_key,octagonFullKey, error);
            CFDictionaryRef v2dictionaryTestUpdates = [self getValueFromExpansion:kSOSTestV2Settings err:NULL];
            if(v2dictionaryTestUpdates) SOSFullPeerInfoUpdateV2Dictionary(self.fullPeerInfo, v2dictionaryTestUpdates, NULL);
            CFReleaseNull(initialViews);
            CFReleaseNull(full_key);
            
            CFSetRef pendingDefaultViews = SOSViewCopyViewSet(kViewSetDefault);
            [self pendEnableViewSet:pendingDefaultViews];
            CFReleaseNull(pendingDefaultViews);
            
            [self setValueInExpansion:kSOSUnsyncedViewsKey value:kCFBooleanTrue err:NULL];
            
            if (!self.fullPeerInfo) {
                secerror("Can't make FullPeerInfo for %@-%@ (%@) - is AKS ok?", SOSPeerGestaltGetName(gestalt), SOSCircleGetName(self.trustedCircle), error ? (void*)*error : (void*)CFSTR("-"));
            }
            else{
                secnotice("fpi", "alert KeychainSyncingOverIDSProxy the fpi is available");
                notify_post(kSecServerPeerInfoAvailable);
                if(deviceID)
                    SOSFullPeerInfoUpdateDeviceID(self.fullPeerInfo, deviceID, error);
            }
        }
        else {
            secerror("No full_key: %@:", error ? *error : NULL);
            
        }
        
        CFReleaseNull(keyName);
    }
    
fail:
    return self.fullPeerInfo != NULL;
}
-(bool) isMyPeerActive:(CFErrorRef*) error
{
    return (self.peerInfo ? SOSCircleHasActivePeer(self.trustedCircle, self.peerInfo, error) : false);
}

-(void) purgeIdentity
{
    if (self.fullPeerInfo) {
        // Purge private key but don't return error if we can't.
        CFErrorRef purgeError = NULL;
        if (!SOSFullPeerInfoPurgePersistentKey(self.fullPeerInfo, &purgeError)) {
            secwarning("Couldn't purge persistent key for %@ [%@]", self.fullPeerInfo, purgeError);
        }
        CFReleaseNull(purgeError);
        
        self.fullPeerInfo=nil;
    }
}
@end

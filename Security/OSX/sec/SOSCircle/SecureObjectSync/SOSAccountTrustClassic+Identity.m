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

- (SecKeyRef)randomPermanentFullECKey:(int)keysize name:(NSString *)name error:(CFErrorRef*)cferror
{
    return GeneratePermanentFullECKey(keysize, (__bridge CFStringRef)name, cferror);
}

- (void)ensureOctagonPeerKeys:(SOSKVSCircleStorageTransport*)circleTransport
{
    NSString* octagonKeyName;
    SecKeyRef publicKey;

    if (SOSFullPeerInfoHaveOctagonKeys(self.fullPeerInfo)) {
        return;
    }

    bool changedSelf = false;

    CFErrorRef copyError = NULL;
    publicKey = SOSFullPeerInfoCopyOctagonSigningKey(self.fullPeerInfo, &copyError);
    if(copyError) {
        secerror("circleChange: Error fetching Octagon signing key: %@", copyError);
        CFReleaseNull(copyError);
    }

    if (publicKey == NULL) {
        octagonKeyName = [NSString stringWithFormat:@"Octagon Peer Signing ID for %@", SOSCircleGetName(self.trustedCircle)];
        CFErrorRef cferror = NULL;
        SecKeyRef octagonSigningFullKey = [self randomPermanentFullECKey:384 name:octagonKeyName error:&cferror];
        if(cferror || !octagonSigningFullKey) {
            secerror("circleChange: Error upgrading Octagon signing key: %@", cferror);
        } else {
            SOSFullPeerInfoUpdateOctagonSigningKey(self.fullPeerInfo, octagonSigningFullKey, &cferror);
            if(cferror) {
                secerror("circleChange: Error upgrading Octagon signing key: %@", cferror);
            }
            changedSelf = true;
        }

        CFReleaseNull(cferror);
        CFReleaseNull(octagonSigningFullKey);
    }
    CFReleaseNull(publicKey);

    CFReleaseNull(copyError);
    publicKey = SOSFullPeerInfoCopyOctagonEncryptionKey(self.fullPeerInfo, &copyError);
    if(copyError) {
        secerror("circleChange: Error fetching Octagon encryption key: %@", copyError);
        CFReleaseNull(copyError);
    }

    if (publicKey == NULL) {
        octagonKeyName = [NSString stringWithFormat:@"Octagon Peer Encryption ID for %@", SOSCircleGetName(self.trustedCircle)];
        CFErrorRef cferror = NULL;
        SecKeyRef octagonEncryptionFullKey = [self randomPermanentFullECKey:384 name:octagonKeyName error:&cferror];
        if(cferror || !octagonEncryptionFullKey) {
            secerror("circleChange: Error upgrading Octagon encryption key: %@", cferror);
        } else {

            SOSFullPeerInfoUpdateOctagonEncryptionKey(self.fullPeerInfo, octagonEncryptionFullKey, &cferror);
            if(cferror) {
                secerror("circleChange: Error upgrading Octagon encryption key: %@", cferror);
            }
            changedSelf = true;
        }

        CFReleaseNull(cferror);
        CFReleaseNull(octagonEncryptionFullKey);
    }
    CFReleaseNull(publicKey);

    if(changedSelf) {
        [self modifyCircle:circleTransport err:NULL action:^bool (SOSCircleRef circle_to_change) {
            return SOSCircleUpdatePeerInfo(circle_to_change, SOSFullPeerInfoGetPeerInfo(self.fullPeerInfo));
        }];
    }
}

-(bool) ensureFullPeerAvailable:(CFDictionaryRef)gestalt deviceID:(CFStringRef)deviceID backupKey:(CFDataRef)backup err:(CFErrorRef *) error
{
    require_action_quiet(self.trustedCircle, fail, SOSCreateErrorWithFormat(kSOSErrorNoCircle, NULL, error, NULL, CFSTR("Don't have circle")));
    
    if (self.fullPeerInfo == NULL) {
        NSString* octagonKeyName;
        CFStringRef keyName = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("ID for %@-%@"), SOSPeerGestaltGetName(gestalt), SOSCircleGetName(self.trustedCircle));
        SecKeyRef full_key = [self randomPermanentFullECKey:256 name:(__bridge NSString *)keyName error:NULL];
        
        octagonKeyName = [@"Octagon Peer Signing " stringByAppendingString:(__bridge NSString*)keyName];
        SecKeyRef octagonSigningFullKey = [self randomPermanentFullECKey:384 name:octagonKeyName error:NULL];

        octagonKeyName = [@"Octagon Peer Encryption " stringByAppendingString:(__bridge NSString*)keyName];
        SecKeyRef octagonEncryptionFullKey = [self randomPermanentFullECKey:384 name:octagonKeyName error:NULL];

        if (full_key && octagonSigningFullKey && octagonEncryptionFullKey) {
            CFSetRef initialViews = SOSViewCopyViewSet(kViewSetInitial);
            
            self.fullPeerInfo = nil;
            self.fullPeerInfo = SOSFullPeerInfoCreateWithViews(kCFAllocatorDefault, gestalt, backup, initialViews, full_key, octagonSigningFullKey, octagonEncryptionFullKey, error);

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

        CFReleaseNull(full_key);
        CFReleaseNull(octagonSigningFullKey);
        CFReleaseNull(octagonEncryptionFullKey);
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

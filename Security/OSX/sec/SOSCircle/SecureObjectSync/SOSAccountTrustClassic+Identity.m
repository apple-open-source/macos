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
#if __OBJC2__
#import "Analytics/Clients/SOSAnalytics.h"
#endif // __OBJC2__

#import "Security/SecureObjectSync/SOSViews.h"

@implementation SOSAccountTrustClassic (Identity)

-(bool)isLockedError:(NSError *)error {
    return error &&
    ([error.domain isEqualToString:(__bridge NSString*)kSecErrorDomain])
    && error.code == errSecInteractionNotAllowed;
}

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

- (SecKeyRef)randomPermanentFullECKey:(int)keysize name:(NSString *)name error:(CFErrorRef*)cferror CF_RETURNS_RETAINED
{
    return GeneratePermanentFullECKey(keysize, (__bridge CFStringRef)name, cferror);
}

// Check that cached values of what is in keychain with what we have in the peer info,
// if they ware the same, we could read the items while this process was alive, assume
// all is swell.
#if OCTAGON
- (bool)haveConfirmedOctagonKeys
{
    bool haveSigningKey = false;
    bool haveEncryptionKey = false;

    SecKeyRef signingKey = SOSFullPeerInfoCopyOctagonPublicSigningKey(self.fullPeerInfo, NULL);
    if (self.cachedOctagonSigningKey && CFEqualSafe(signingKey, self.cachedOctagonSigningKey)) {
        haveSigningKey = true;
    } else {
        secerror("circleChange: No extant octagon signing key");
    }

    SecKeyRef encrytionKey = SOSFullPeerInfoCopyOctagonPublicEncryptionKey(self.fullPeerInfo, NULL);
    if (self.cachedOctagonEncryptionKey && CFEqualSafe(encrytionKey, self.cachedOctagonEncryptionKey)) {
        haveEncryptionKey = true;
    } else {
        secerror("circleChange: No extant octagon encryption key");
    }

    CFReleaseNull(signingKey);
    CFReleaseNull(encrytionKey);

    return haveSigningKey && haveEncryptionKey;
}
#endif

- (void)ensureOctagonPeerKeys:(SOSKVSCircleStorageTransport*)circleTransport
{
#if OCTAGON
    NSString* octagonKeyName;
    SecKeyRef octagonSigningFullKey = NULL;
    SecKeyRef octagonEncryptionFullKey = NULL;

    // check if we already confirmed the keys
    if ([self haveConfirmedOctagonKeys]) {
        return;
    }

    bool changedSelf = false;

    CFErrorRef copyError = NULL;
    octagonSigningFullKey = SOSFullPeerInfoCopyOctagonSigningKey(self.fullPeerInfo, &copyError);
    if(copyError && ![self isLockedError:(__bridge NSError *)copyError]) {
        secerror("circleChange: Error fetching Octagon signing key: %@", copyError);
    }

    // Cache that public key we found, to so that we don't need to make the roundtrip though
    // keychain to get them item, if we don't find a key, try to create a new key if the error
    // is specifically "couldn't find key", "couldn't read key", or "something went very very wrong".
    // Otherwise, log a fatal error.

    if (octagonSigningFullKey) {
        secnotice("circleChange", "Already have Octagon signing key");
        CFReleaseNull(self->_cachedOctagonSigningKey);
        _cachedOctagonSigningKey = SecKeyCopyPublicKey(octagonSigningFullKey);
    } else if (octagonSigningFullKey == NULL && copyError &&
        ((CFEqualSafe(CFErrorGetDomain(copyError), kCFErrorDomainOSStatus) && CFErrorGetCode(copyError) == errSecItemNotFound) ||
         (CFEqualSafe(CFErrorGetDomain(copyError), kCFErrorDomainOSStatus) && CFErrorGetCode(copyError) == errSecDecode) ||
         (CFEqualSafe(CFErrorGetDomain(copyError), kCFErrorDomainOSStatus) && CFErrorGetCode(copyError) == errSecParam)))
    {
        octagonKeyName = [NSString stringWithFormat:@"Octagon Peer Signing ID for %@", SOSCircleGetName(self.trustedCircle)];
        CFErrorRef cferror = NULL;
        octagonSigningFullKey = [self randomPermanentFullECKey:384 name:octagonKeyName error:&cferror];
        if(cferror || !octagonSigningFullKey) {
            secerror("circleChange: Error creating Octagon signing key: %@", cferror);
        } else {
            SOSFullPeerInfoUpdateOctagonSigningKey(self.fullPeerInfo, octagonSigningFullKey, &cferror);
            if(cferror) {
                secerror("circleChange: Error upgrading Octagon signing key: %@", cferror);
            } else {
                secnotice("circleChange", "Successfully created new Octagon signing key");
            }
            changedSelf = true;
        }

        CFReleaseNull(cferror);
    } else if((octagonSigningFullKey == NULL || copyError) && ![self isLockedError:(__bridge NSError *)copyError]) {
        secerror("error is too scary, not creating new Octagon signing key: %@", copyError);
#if __OBJC2__
            [[SOSAnalytics logger] logResultForEvent:@"SOSCheckOctagonSigningKey" hardFailure:true result:(__bridge NSError*)copyError];
#endif // __OBJC2__
    }

    CFReleaseNull(copyError);
    CFReleaseNull(octagonSigningFullKey);

    // Now do the same thing for encryption key

    CFReleaseNull(copyError);
    octagonEncryptionFullKey = SOSFullPeerInfoCopyOctagonEncryptionKey(self.fullPeerInfo, &copyError);
    if(copyError && ![self isLockedError:(__bridge NSError *)copyError]) {
        secerror("circleChange: Error fetching Octagon encryption key: %@", copyError);
    }

    if (octagonEncryptionFullKey) {
        secnotice("circleChange", "Already have Octagon encryption key");
        CFReleaseNull(self->_cachedOctagonEncryptionKey);
        _cachedOctagonEncryptionKey = SecKeyCopyPublicKey(octagonEncryptionFullKey);
    } else if (octagonEncryptionFullKey == NULL && copyError &&
        ((CFEqualSafe(CFErrorGetDomain(copyError), kCFErrorDomainOSStatus) && CFErrorGetCode(copyError) == errSecItemNotFound) ||
         (CFEqualSafe(CFErrorGetDomain(copyError), kCFErrorDomainOSStatus) && CFErrorGetCode(copyError) == errSecDecode) ||
         (CFEqualSafe(CFErrorGetDomain(copyError), kCFErrorDomainOSStatus) && CFErrorGetCode(copyError) == errSecParam)))
    {
        octagonKeyName = [NSString stringWithFormat:@"Octagon Peer Encryption ID for %@", SOSCircleGetName(self.trustedCircle)];
        CFErrorRef cferror = NULL;
        octagonEncryptionFullKey = [self randomPermanentFullECKey:384 name:octagonKeyName error:&cferror];
        if(cferror || !octagonEncryptionFullKey) {
            secerror("circleChange: Error creating Octagon encryption key: %@", cferror);
        } else {
            SOSFullPeerInfoUpdateOctagonEncryptionKey(self.fullPeerInfo, octagonEncryptionFullKey, &cferror);
            if(cferror) {
                secerror("circleChange: Error upgrading Octagon encryption key: %@", cferror);
            } else {
                secnotice("circleChange", "Successfully created new Octagon encryption key");
            }
            changedSelf = true;
        }

        CFReleaseNull(cferror);

    } else if((octagonEncryptionFullKey == NULL || copyError) && ![self isLockedError:(__bridge NSError *)copyError]) {
        secerror("error is too scary, not creating new Octagon encryption key: %@", copyError);
#if __OBJC2__
            [[SOSAnalytics logger] logResultForEvent:@"SOSCheckOctagonEncryptionKey" hardFailure:true result:(__bridge NSError*)copyError];
#endif
    }
    CFReleaseNull(copyError);
    CFReleaseNull(octagonEncryptionFullKey);

    if(changedSelf) {
        [self modifyCircle:circleTransport err:NULL action:^bool (SOSCircleRef circle_to_change) {
            return SOSCircleUpdatePeerInfo(circle_to_change, SOSFullPeerInfoGetPeerInfo(self.fullPeerInfo));
        }];
    }
#endif /* OCTAGON */
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

            // setting fullPeerInfo takes an extra ref, so...
            SOSFullPeerInfoRef fpi = SOSFullPeerInfoCreateWithViews(kCFAllocatorDefault, gestalt, backup, initialViews, full_key, octagonSigningFullKey, octagonEncryptionFullKey, error);
            self.fullPeerInfo = fpi;
            CFReleaseNull(fpi);

            CFDictionaryRef v2dictionaryTestUpdates = [self getValueFromExpansion:kSOSTestV2Settings err:NULL];
            if(v2dictionaryTestUpdates) SOSFullPeerInfoUpdateV2Dictionary(self.fullPeerInfo, v2dictionaryTestUpdates, NULL);
            CFReleaseNull(initialViews);
            
            CFSetRef pendingDefaultViews = SOSViewCopyViewSet(kViewSetDefault);
            [self pendEnableViewSet:pendingDefaultViews];
            CFReleaseNull(pendingDefaultViews);
            
            [self setValueInExpansion:kSOSUnsyncedViewsKey value:kCFBooleanTrue err:NULL];
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

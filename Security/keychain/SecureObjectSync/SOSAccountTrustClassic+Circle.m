//
//  SOSAccountTrustClassicCircle.m
//  Security
//

#import <Foundation/Foundation.h>
#include <AssertMacros.h>

#import "keychain/SecureObjectSync/SOSAccount.h"
#import "keychain/SecureObjectSync/SOSAccountTrustClassic.h"
#import "keychain/SecureObjectSync/SOSTransportCircle.h"
#import "keychain/SecureObjectSync/SOSAccountTrustClassic+Identity.h"
#import "keychain/SecureObjectSync/SOSAccountTrustClassic+Expansion.h"
#import "keychain/SecureObjectSync/SOSAccountTrustClassic+Retirement.h"

#import "keychain/SecureObjectSync/SOSAccountGhost.h"
#import "keychain/SecureObjectSync/SOSIntervalEvent.h"
#import "keychain/SecureObjectSync/SOSViews.h"
#import "Analytics/Clients/SOSAnalytics.h"


@implementation SOSAccountTrustClassic (Circle)

#define ICLOUDIDDATE @"iCloudIDDate"

-(bool) isInCircleOnly:(CFErrorRef *)error
{
    SOSCCStatus result = [self getCircleStatusOnly:error];
    
    if (result != kSOSCCInCircle) {
        SOSErrorCreate(kSOSErrorNoCircle, error, NULL, CFSTR("Not in circle"));
        return false;
    }
    
    return true;
}

-(bool) hasCircle:(CFErrorRef*) error
{
    if (!self.trustedCircle)
        SOSCreateErrorWithFormat(kSOSErrorNoCircle, NULL, error, NULL, CFSTR("No trusted circle"));
    
    return self.trustedCircle != NULL;
}

-(SOSCCStatus) thisDeviceStatusInCircle:(SOSCircleRef) circle peer:(SOSPeerInfoRef) this_peer
{
    if (!circle)
        return kSOSCCNotInCircle;
    
    if (circle && SOSCircleCountPeers(circle) == 0)
        return kSOSCCCircleAbsent;
    
    if (this_peer) {
        
        if(SOSPeerInfoIsRetirementTicket(this_peer))
            return kSOSCCNotInCircle;
        
        if (SOSCircleHasPeer(circle, this_peer, NULL))
            return kSOSCCInCircle;
        
        if (SOSCircleHasApplicant(circle, this_peer, NULL))
            return kSOSCCRequestPending;
    }
    
    return kSOSCCNotInCircle;
}
-(SOSCCStatus) getCircleStatusOnly:(CFErrorRef*) error
{
    return [self thisDeviceStatusInCircle:self.trustedCircle peer:self.peerInfo];
}


-(SOSCircleRef) getCircle:(CFErrorRef *)error
{
    CFTypeRef entry = self.trustedCircle;
    require_action_quiet(!isNull(entry), fail,
                         SOSCreateError(kSOSErrorIncompatibleCircle, CFSTR("Incompatible circle in KVS"), NULL, error));
    return (SOSCircleRef) entry;
    
fail:
    return NULL;
}


//Circle

-(SOSCircleRef) ensureCircle:(SOSAccount*)a name:(CFStringRef)name err:(CFErrorRef *)error
{
    CFErrorRef localError = NULL;
    if (self.trustedCircle == NULL) {
        SOSCircleRef newCircle = SOSCircleCreate(NULL, name, NULL);
        self.trustedCircle = newCircle; // Note that this setter adds a retain
        CFReleaseNull(newCircle);
        secnotice("circleop", "Setting key_interests_need_updating to true in ensureCircle");
        a.key_interests_need_updating = true;
    }
    
    require_action_quiet(self.trustedCircle || !isSOSErrorCoded(localError, kSOSErrorIncompatibleCircle), fail,
                         if (error) { *error = localError; localError = NULL; });
    
fail:
    CFReleaseNull(localError);
    return self.trustedCircle;
}

-(bool) hasLeft
{
    switch(self.departureCode) {
        case kSOSDiscoveredRetirement: /* Fallthrough */
        case kSOSLostPrivateKey: /* Fallthrough */
        case kSOSWithdrewMembership: /* Fallthrough */
        case kSOSMembershipRevoked: /* Fallthrough */
        case kSOSLeftUntrustedCircle:
            return true;
        case kSOSNeverAppliedToCircle: /* Fallthrough */
        case kSOSNeverLeftCircle: /* Fallthrough */
        default:
            return false;
    }
}

/* This check is new to protect piggybacking by the current peer - in that case we have a remote peer signature that
   can't have ghost cleanup changing the circle hash.
 */

-(bool) ghostBustingOK:(SOSCircleRef) oldCircle updatingTo:(SOSCircleRef) newCircle {
    bool retval = false;
    // Preliminaries - we must have a peer and it must be in the newCircle in order to attempt busting
    SOSFullPeerInfoRef me_full = self.fullPeerInfo;
    if(!me_full) return false;
    SOSPeerInfoRef me = SOSFullPeerInfoGetPeerInfo(me_full);
    if(!me || (!SOSCircleHasPeer(newCircle, me, NULL) && !SOSCircleHasApplicant(newCircle, me, NULL))) return false;
    
    CFStringRef myPid = SOSPeerInfoGetPeerID(me);
    CFDictionaryRef newSigs = SOSCircleCopyAllSignatures(newCircle);
    bool iSignedNew = CFDictionaryGetCountOfKey(newSigs, myPid);
    long otherPeerSigCount = CFDictionaryGetCount(newSigs) - ((iSignedNew) ? 2: 1);

    if (SOSCircleHasPeer(oldCircle, me, NULL)) { // If we're already in the old one we're not PBing
        retval = true;
    } else if (!iSignedNew) { // Piggybacking peers always have signed as part of genSigning - so this indicates we're safe to bust.
        retval = true;
    } else if(iSignedNew && otherPeerSigCount > 1) { // if others have seen this we're good to bust.
        retval = true;
    }
    CFReleaseNull(newSigs);
    return retval;
}

// If this circle bears a signature from us and a newer gencount and it isn't our "current" circle, we're
// going to trust it.  That's the signature of a piggybacked circle where we were the sponsor.

-(bool) checkForSponsorshipTrust:(SOSCircleRef) prospective_circle {
    if(CFEqualSafe(self.trustedCircle, prospective_circle)) return false;
    SecKeyRef myPubKey = SOSFullPeerInfoCopyPubKey(self.fullPeerInfo, NULL);
    if(!myPubKey) return false;
    if(SOSCircleVerify(prospective_circle, myPubKey, NULL) && SOSCircleIsOlderGeneration(self.trustedCircle, prospective_circle)) {
        [self setTrustedCircle:prospective_circle];
        CFReleaseNull(myPubKey);
        return true;
    }
    CFReleaseNull(myPubKey);
    return false;
}

static bool publicKeysEqual(SecKeyRef pubKey1, SecKeyRef pubKey2)
{
    // If either pub key is NULL, then the keys are equal if both are NULL.
    if(pubKey1 == NULL || pubKey2 == NULL) {
        return pubKey1 == NULL && pubKey2 == NULL;
    }

    NSData *key1SPKI = CFBridgingRelease(SecKeyCopySubjectPublicKeyInfo(pubKey1));
    NSData *key2SPKI = CFBridgingRelease(SecKeyCopySubjectPublicKeyInfo(pubKey2));

    return !![key1SPKI isEqual:key2SPKI];
}

static bool SOSCirclePeerOctagonKeysChanged(SOSPeerInfoRef oldPeer, SOSPeerInfoRef newPeer) {
    if(!oldPeer) {
        // We've run across some situations where a new peer which should have keys isn't returning yes here.
        // Therefore, always return yes on peer addition.
        return !!newPeer;
    }

    CFErrorRef oldSigningKeyError = NULL;
    SecKeyRef oldSigningKey = oldPeer ? SOSPeerInfoCopyOctagonSigningPublicKey(oldPeer, &oldSigningKeyError) : NULL;

    CFErrorRef oldEncryptionKeyError = NULL;
    SecKeyRef oldEncryptionKey = oldPeer ? SOSPeerInfoCopyOctagonEncryptionPublicKey(oldPeer, &oldEncryptionKeyError) : NULL;

    CFErrorRef newSigningKeyError = NULL;
    SecKeyRef newSigningKey = newPeer ? SOSPeerInfoCopyOctagonSigningPublicKey(newPeer, &newSigningKeyError) : NULL;

    CFErrorRef newEncryptionKeyError = NULL;
    SecKeyRef newEncryptionKey = newPeer ? SOSPeerInfoCopyOctagonEncryptionPublicKey(newPeer, &newEncryptionKeyError) : NULL;

    if(oldPeer && oldSigningKeyError) {
        secerror("circleOps: Cannot fetch signing key for old %@: %@", oldPeer, oldSigningKeyError);
    }
    if(oldPeer && oldEncryptionKeyError) {
        secerror("circleOps: Cannot fetch encryption key for old %@: %@", oldPeer, oldEncryptionKeyError);
    }
    if(newPeer && newSigningKeyError) {
        secerror("circleOps: Cannot fetch signing key for new %@: %@", newPeer, newSigningKeyError);
    }
    if(newPeer && newEncryptionKeyError) {
        secerror("circleOps: Cannot fetch encryption key for new %@: %@", newPeer, newEncryptionKeyError);
    }

    bool signingKeyChanged = !publicKeysEqual(oldSigningKey, newSigningKey);
    bool encryptionKeyChanged = !publicKeysEqual(oldEncryptionKey, newEncryptionKey);

    bool keysChanged = signingKeyChanged || encryptionKeyChanged;

    CFReleaseNull(oldSigningKeyError);
    CFReleaseNull(oldSigningKey);
    CFReleaseNull(oldEncryptionKeyError);
    CFReleaseNull(oldEncryptionKey);

    CFReleaseNull(newSigningKeyError);
    CFReleaseNull(newSigningKey);
    CFReleaseNull(newEncryptionKeyError);
    CFReleaseNull(newEncryptionKey);
    return keysChanged;
}
 
static bool SOSCircleHasUpdatedPeerInfoWithOctagonKey(SOSCircleRef oldCircle, SOSCircleRef newCircle)
{
    __block bool hasUpdated = false;
    SOSCircleForEachPeer(oldCircle, ^(SOSPeerInfoRef oldPeer) {
        SOSPeerInfoRef equivalentNewPeer = SOSCircleCopyPeerWithID(newCircle, SOSPeerInfoGetPeerID(oldPeer), NULL);
        hasUpdated |= SOSCirclePeerOctagonKeysChanged(oldPeer, equivalentNewPeer);
        CFReleaseNull(equivalentNewPeer);
    });

    SOSCircleForEachPeer(newCircle, ^(SOSPeerInfoRef newPeer) {
        SOSPeerInfoRef equivalentOldPeer = SOSCircleCopyPeerWithID(oldCircle, SOSPeerInfoGetPeerID(newPeer), NULL);
        hasUpdated |= SOSCirclePeerOctagonKeysChanged(equivalentOldPeer, newPeer);
        CFReleaseNull(equivalentOldPeer);
    });

    return hasUpdated;
}

// Check on the iCloud identity availability every 24-36 hours random interval
- (SOSIntervalEvent *) iCloudCheckEventHandle: (SOSAccount *) account {
    return [[SOSIntervalEvent alloc] initWithDefaults:account.settings dateDescription:@"iCloudIDCheck" earliest:60*60*24 latest:60*60*36];
}

// Cleanup unusable iCloud identities every 5-7 days random interval
- (SOSIntervalEvent *) iCloudCleanerHandle: (SOSAccount *) account {
    return [[SOSIntervalEvent alloc] initWithDefaults:account.settings dateDescription:@"iCloudCleanerCheck" earliest:60*60*24*5 latest:60*60*24*7];
}

-(bool) handleUpdateCircle:(SOSCircleRef) prospective_circle transport:(SOSKVSCircleStorageTransport*)circleTransport update:(bool) writeUpdate err:(CFErrorRef*)error
{
    bool success = true;
    bool haveOldCircle = true;
    const char *local_remote = writeUpdate ? "local": "remote";
    
    SOSAccount* account = [circleTransport getAccount];

    secnotice("signing", "start:[%s]", local_remote);
    if (!account.accountKey || !account.accountKeyIsTrusted) {
        SOSCreateError(kSOSErrorPublicKeyAbsent, CFSTR("Can't handle updates with no trusted public key here"), NULL, error);
        return false;
    }
    
    if (!prospective_circle) {
        secerror("##### Can't update to a NULL circle ######");
        return false; // Can't update one we don't have.
    }
    
    // If this is a remote circle, check to see if this is our first opportunity to trust a circle where we
    // sponsored the only signer.
    if(!writeUpdate && [ self checkForSponsorshipTrust: prospective_circle ]){
        SOSCCEnsurePeerRegistration();
        secnotice("circleop", "Setting key_interests_need_updating to true in handleUpdateCircle");
        account.key_interests_need_updating = true;
        return true;
        
    }

    CFStringRef newCircleName = SOSCircleGetName(prospective_circle);

    SOSCircleRef oldCircle = self.trustedCircle;
    SOSCircleRef emptyCircle = NULL;
    
    if(oldCircle == NULL) {
        SOSCreateErrorWithFormat(kSOSErrorIncompatibleCircle, NULL, error, NULL, CFSTR("Current Entry is NULL; rejecting %@"), prospective_circle);
        secerror("##### Can't replace circle - we don't care about it ######");
        return false;
    }
    if (CFGetTypeID(oldCircle) != SOSCircleGetTypeID()) {
        secdebug("signing", ">>>>>>>>>>>>>>>  Non-Circle Circle found <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<");
        // We don't know what is in our table, likely it was kCFNull indicating we didn't
        // understand a circle that came by. We seem to like this one lets make our entry be empty circle
        emptyCircle = SOSCircleCreate(kCFAllocatorDefault, newCircleName, NULL);
        oldCircle = emptyCircle;
        haveOldCircle = false;
        // And we're paranoid, drop our old peer info if for some reason we didn't before.
        // SOSAccountDestroyCirclePeerInfo(account, oldCircle, NULL);
    }
    
    
    SOSAccountScanForRetired(account, prospective_circle, error);
    SOSCircleRef newCircle = SOSAccountCloneCircleWithRetirement(account, prospective_circle, error);
    if(!newCircle) return false;
    
    SOSFullPeerInfoRef me_full = self.fullPeerInfo;
    SOSPeerInfoRef     me = SOSFullPeerInfoGetPeerInfo(me_full);
    CFStringRef        myPeerID = SOSPeerInfoGetPeerID(me);
    myPeerID = (myPeerID) ? myPeerID: CFSTR("No Peer");
    
    if (me && SOSCircleUpdatePeerInfo(newCircle, me)) {
        writeUpdate = true; // If we update our peer in the new circle we should write it if we accept it.
    }
    
    typedef enum {
        accept,
        countersign,
        leave,
        revert,
        ignore
    } circle_action_t;
    
    static const char *actionstring[] = {
        "accept", "countersign", "leave", "revert", "ignore",
    };
    
    circle_action_t circle_action = ignore;
    enum DepartureReason leave_reason = kSOSNeverLeftCircle;
    
    SecKeyRef old_circle_key = NULL;
    if(SOSCircleVerify(oldCircle, account.accountKey, NULL)){
        old_circle_key = account.accountKey;
    }
    else if(account.previousAccountKey && SOSCircleVerify(oldCircle, account.previousAccountKey, NULL)){
        old_circle_key = account.previousAccountKey;
    }
    
    bool userTrustedOldCircle = (old_circle_key != NULL) && haveOldCircle;
    
    SOSConcordanceStatus concstat =
    SOSCircleConcordanceTrust(oldCircle, newCircle,
                              old_circle_key, account.accountKey,
                                      me, error);
            
            CFStringRef concStr = NULL;
            switch(concstat) {
                case kSOSConcordanceTrusted:
                    circle_action = countersign;
                    concStr = CFSTR("Trusted");
                    break;
                case kSOSConcordanceGenOld:
                    circle_action = userTrustedOldCircle ? revert : ignore;
                    concStr = CFSTR("Generation Old");
                    break;
                case kSOSConcordanceBadUserSig:
                case kSOSConcordanceBadPeerSig:
                    circle_action = userTrustedOldCircle ? revert : accept;
                    concStr = CFSTR("Bad Signature");
                    break;
                case kSOSConcordanceNoUserSig:
                    circle_action = userTrustedOldCircle ? revert : accept;
                    concStr = CFSTR("No User Signature");
                    break;
                case kSOSConcordanceNoPeerSig:
                    circle_action = accept; // We might like this one eventually but don't countersign.
                    concStr = CFSTR("No trusted peer signature");
                    secnotice("signing", "##### No trusted peer signature found, accepting hoping for concordance later");
                    break;
                case kSOSConcordanceNoPeer:
                    circle_action = leave;
                    leave_reason = kSOSLeftUntrustedCircle;
                    concStr = CFSTR("No trusted peer left");
                    break;
                case kSOSConcordanceNoUserKey:
                    secerror("##### No User Public Key Available, this shouldn't ever happen!!!");
                    abort();
                    break;
                default:
                    secerror("##### Bad Error Return from ConcordanceTrust");
                    abort();
                    break;
            }
    
    secnotice("signing", "Decided on action [%s] based on concordance state [%@] and [%s] circle.  My PeerID is %@", actionstring[circle_action], concStr, userTrustedOldCircle ? "trusted" : "untrusted", myPeerID);
    
    SOSCircleRef circleToPush = NULL;
    
    if (circle_action == leave) {
        circle_action = ignore; (void) circle_action; // Acknowledge this is a dead store.
        
        if (me && SOSCircleHasPeer(oldCircle, me, NULL)) {
            secnotice("account", "Leaving circle with peer %@", me);
            debugDumpCircle(CFSTR("oldCircle"), oldCircle);
            debugDumpCircle(CFSTR("newCircle"), newCircle);
            debugDumpCircle(CFSTR("prospective_circle"), prospective_circle);
            secnotice("account", "Key state: accountKey %@, previousAccountKey %@, old_circle_key %@",
                      account.accountKey, account.previousAccountKey, old_circle_key);
            
            if (sosAccountLeaveCircle(account, newCircle, nil, error)) {
                secnotice("circleOps", "Leaving circle by newcircle state");
                circleToPush = newCircle;
            } else {
                secnotice("signing", "Can't leave circle, but dumping identities");
                success = false;
            }
            self.departureCode = leave_reason;
            circle_action = accept;
            me = NULL;
            me_full = NULL;
        } else {
            // We are not in this circle, but we need to update account with it, since we got it from cloud
            secnotice("signing", "We are not in this circle, but we need to update account with it");
            debugDumpCircle(CFSTR("oldCircle"), oldCircle);
            debugDumpCircle(CFSTR("newCircle"), newCircle);
            debugDumpCircle(CFSTR("prospective_circle"), prospective_circle);
            circle_action = accept;
        }
    }
    
    if (circle_action == countersign) {
        if (me && SOSCircleHasPeer(newCircle, me, NULL)) {
            if (SOSCircleVerifyPeerSigned(newCircle, me, NULL)) {
                secnotice("signing", "Already concur with the new circle");
            } else {
                CFErrorRef signing_error = NULL;
                
                if (me_full && SOSCircleConcordanceSign(newCircle, me_full, &signing_error)) {
                    circleToPush = newCircle;
                    secnotice("signing", "Concurred with new circle");
                } else {
                    secerror("Failed to concurrence sign, error: %@", signing_error);
                    success = false;
                }
                CFReleaseSafe(signing_error);
            }
        } else {
            secnotice("signing", "Not countersigning, not in new circle");
            [account.trust resetRingDictionary];
        }
        circle_action = accept;
    }
    
    if (circle_action == accept) {
        if(SOSCircleHasUpdatedPeerInfoWithOctagonKey(oldCircle, newCircle)){
            secnotice("circleOps", "Sending kSOSCCCircleOctagonKeysChangedNotification");
            notify_post(kSOSCCCircleOctagonKeysChangedNotification);
        }
        if (me && SOSCircleHasActivePeer(oldCircle, me, NULL) && !SOSCircleHasPeer(newCircle, me, NULL)) {
            //  Don't destroy evidence of other code determining reason for leaving.
            if(![self hasLeft]) self.departureCode = kSOSMembershipRevoked;
            secnotice("circleOps", "Member of old circle but not of new circle (%d)", self.departureCode);
            debugDumpCircle(CFSTR("oldCircle"), oldCircle);
            debugDumpCircle(CFSTR("newCircle"), newCircle);
        }
        
        if (me
            && SOSCircleHasActivePeer(oldCircle, me, NULL)
            && !(SOSCircleCountPeers(oldCircle) == 1 && SOSCircleHasPeer(oldCircle, me, NULL)) // If it was our offering, don't change ID to avoid ghosts
            && !SOSCircleHasPeer(newCircle, me, NULL) && !SOSCircleHasApplicant(newCircle, me, NULL)) {
            secnotice("circle", "Purging my peer (ID: %@) for circle '%@'!!!", SOSPeerInfoGetPeerID(me), SOSCircleGetName(oldCircle));
            if (self.fullPeerInfo)
                SOSFullPeerInfoPurgePersistentKey(self.fullPeerInfo, NULL);
            me = NULL;
            me_full = NULL;
        }
        
        if (me && SOSCircleHasRejectedApplicant(newCircle, me, NULL)) {
            SOSPeerInfoRef  reject = SOSCircleCopyRejectedApplicant(newCircle, me, NULL);
            if(CFEqualSafe(reject, me) && SOSPeerInfoApplicationVerify(me, account.accountKey, NULL)) {
                secnotice("circle", "Rejected, Purging my applicant peer (ID: %@) for circle '%@'", SOSPeerInfoGetPeerID(me), SOSCircleGetName(oldCircle));
                debugDumpCircle(CFSTR("oldCircle"), oldCircle);
                debugDumpCircle(CFSTR("newCircle"), newCircle);
                if (self.fullPeerInfo)
                    SOSFullPeerInfoPurgePersistentKey(self.fullPeerInfo, NULL);
                me = NULL;
                me_full = NULL;
            } else {
                secnotice("circle", "Rejected, Reapplying (ID: %@) for circle '%@'", SOSPeerInfoGetPeerID(me), SOSCircleGetName(oldCircle));
                debugDumpCircle(CFSTR("oldCircle"), oldCircle);
                debugDumpCircle(CFSTR("newCircle"), newCircle);
                SOSCircleRequestReadmission(newCircle, account.accountKey, me, NULL);
                writeUpdate = true;
            }
            CFReleaseNull(reject);
        }

        if(me && account.accountKeyIsTrusted && SOSCircleHasPeer(newCircle, me, NULL)) {
            // do this on daily interval +/- 8 hours random to keep all peers doing this at the same time
            SOSIntervalEvent *iCloudCheckEvent = [self iCloudCheckEventHandle: account];
            if([iCloudCheckEvent checkDate]) {
                bool fixedIdentities = [self fixICloudIdentities:account circle:newCircle];
                if(fixedIdentities) {
                    writeUpdate = true;
                    secnotice("circleOps", "Fixed iCloud Identity in circle");
                } else {
                    secnotice("circleOps", "Failed to fix broken icloud identity");
                }
                [iCloudCheckEvent followup];
            }
        }
        
        CFRetainSafe(oldCircle);
        account.previousAccountKey = account.accountKey;

        secnotice("signing", "%@, Accepting new circle", concStr);
        if (circle_action == accept) {
            [self setTrustedCircle:newCircle];
        }
        
        if (me && account.accountKeyIsTrusted
            && SOSCircleHasApplicant(oldCircle, me, NULL)
            && SOSCircleCountPeers(newCircle) > 0
            && !SOSCircleHasPeer(newCircle, me, NULL) && !SOSCircleHasApplicant(newCircle, me, NULL)) {
            // We weren't rejected (above would have set me to NULL.
            // We were applying and we weren't accepted.
            // Our application is declared lost, let us reapply.
            
            secnotice("signing", "requesting readmission to new circle");
            if (SOSCircleRequestReadmission(newCircle, account.accountKey, me, NULL))
                writeUpdate = true;
        }
        
        if (me && SOSCircleHasActivePeer(oldCircle, me, NULL)) {
            [account.trust cleanupRetirementTickets:account circle:oldCircle time:RETIREMENT_FINALIZATION_SECONDS err:NULL];
        }
        
        SOSAccountNotifyOfChange(account, oldCircle, newCircle);
        
        CFReleaseNull(oldCircle);
        
        if (writeUpdate)
            circleToPush = newCircle;
        secnotice("circleop", "Setting key_interests_need_updating to true in handleUpdateCircle");
        account.key_interests_need_updating = true;
    }
    
    /*
     * In the revert section we'll guard the KVS idea of circles by rejecting "bad" new circles
     * and pushing our current view of the circle (oldCircle).  We'll only do this if we actually
     * are a member of oldCircle - never for an empty circle.
     */
    
    if (circle_action == revert) {
        if(haveOldCircle && me && SOSCircleHasActivePeer(oldCircle, me, NULL)) {
            secnotice("signing", "%@, Rejecting new circle, re-publishing old circle", concStr);
            circleToPush = oldCircle;
            [self setTrustedCircle:oldCircle];
        } else {
            secnotice("canary", "%@, Rejecting: new circle Have no old circle - would reset", concStr);
        }
    }
    
    
    if (circleToPush != NULL) {
        secnotice("signing", "Pushing:[%s]", local_remote);
        CFDataRef circle_data = SOSCircleCopyEncodedData(circleToPush, kCFAllocatorDefault, error);
        
        if (circle_data) {
            // Ensure we flush changes
            account.circle_rings_retirements_need_attention = true;
            
            //posting new circle to peers
            success &= [circleTransport postCircle:SOSCircleGetName(circleToPush) circleData:circle_data err:error];
        } else {
            success = false;
        }
        CFReleaseNull(circle_data);
    }
    CFReleaseSafe(newCircle);
    CFReleaseNull(emptyCircle);
    
    // There are errors collected above that are soft (worked around)
    if(success && error && *error) {
        CFReleaseNull(*error);
    }
    
    return success;
}

-(bool) updateCircleFromRemote:(SOSKVSCircleStorageTransport*)circleTransport newCircle:(SOSCircleRef)newCircle err:(CFErrorRef*)error
{
    return [self handleUpdateCircle:newCircle transport:circleTransport update:false err:error];
}

-(bool) updateCircle:(SOSKVSCircleStorageTransport*)circleTransport newCircle:(SOSCircleRef) newCircle err:(CFErrorRef*)error
{
    return [self handleUpdateCircle:newCircle transport:circleTransport update:true err:error];
}

-(bool) modifyCircle:(SOSKVSCircleStorageTransport*)circleTransport err:(CFErrorRef*)error action:(SOSModifyCircleBlock)block
{
    bool success = false;
    SOSCircleRef circleCopy = NULL;
    require_action_quiet(self.trustedCircle, fail, SOSErrorCreate(kSOSErrorNoCircle, error, NULL, CFSTR("No circle to get peer key from")));
    
    circleCopy = SOSCircleCopyCircle(kCFAllocatorDefault, self.trustedCircle, error);
    require_quiet(circleCopy, fail);
    
    success = true;
    require_quiet(block(circleCopy), fail);
    
    success = [self updateCircle:circleTransport newCircle:circleCopy err:error];
    
fail:
    CFReleaseSafe(circleCopy);
    return success;
    
}

// true means things changed.
-(bool) fixICloudIdentities:(SOSAccount *) account circle: (SOSCircleRef) circle {
    bool retval = false;
    SOSFullPeerInfoRef icfpi = SOSCircleCopyiCloudFullPeerInfoRef(circle, NULL);
    if(!icfpi) {
        SOSAccountRestartPrivateCredentialTimer(account);
        if((SOSAccountGetPrivateCredential(account, NULL) != NULL) || SOSAccountAssertStashedAccountCredential(account, NULL)) {
            SecKeyRef privKey = SOSAccountGetPrivateCredential(account, NULL);
            if(privKey) {
                SOSIntervalEvent *iCloudCleanupEvent = [self iCloudCleanerHandle: account];
                if([iCloudCleanupEvent checkDate]) {
                    SOSAccountRemoveIncompleteiCloudIdentities(account, circle, privKey, NULL);
                    [iCloudCleanupEvent followup];
                }
                CFErrorRef error = NULL;
                bool identityAdded = [self addiCloudIdentity:circle key:privKey err:&error];
                if(identityAdded) {
                    account.notifyBackupOnExit = true;
                    retval = true;
                    [[SOSAnalytics logger] logSuccessForEventNamed:@"iCloudIdentityFix"];
                } else {
                    [[SOSAnalytics logger] logResultForEvent:@"iCloudIdentityFix" hardFailure:true result:(__bridge NSError * _Nullable)(error)];
                }
                CFReleaseNull(error);
            } else {
                NSDictionary *attr = @{ @"reason" : @"noPrivateKey" };
                [[SOSAnalytics logger] logHardFailureForEventNamed:@"iCloudIdentityFix" withAttributes:attr];
            }
        } else {
            NSDictionary *attr = @{ @"reason" : @"noPrivateKey" };
            [[SOSAnalytics logger] logHardFailureForEventNamed:@"iCloudIdentityFix" withAttributes:attr];
        }
    } else {
        // everything is fine.
        CFReleaseNull(icfpi);
    }
    return retval;
}

-(void) generationSignatureUpdateWith:(SOSAccount*)account key:(SecKeyRef) privKey
{
    // rdar://51233857 - don't gensign if there isn't a change in the userKey
    // also don't rebake the circle to fix the icloud identity if there isn't
    // a change as that will mess up piggybacking.
    if(SOSAccountFullPeerInfoVerify(account, privKey, NULL) && SOSCircleVerify(account.trust.trustedCircle, account.accountKey, NULL)) {
        secnotice("updatingGenSignature", "no change to userKey - skipping gensign");
        return;
    }

    if (self.trustedCircle && self.fullPeerInfo) {
        [self modifyCircle:account.circle_transport err:NULL action:^(SOSCircleRef circle) {
            SOSPeerInfoRef myPI = account.peerInfo;
            bool iAmPeer = SOSCircleHasPeer(circle, myPI, NULL);
            bool change = SOSCircleUpdatePeerInfo(circle, myPI);
            if(iAmPeer && !SOSCircleVerify(circle, account.accountKey, NULL)) {
                change |= [self upgradeiCloudIdentity:circle privKey:privKey];
                [self removeInvalidApplications:circle userPublic:account.accountKey];
                change |= SOSCircleGenerationSign(circle, privKey, self.fullPeerInfo, NULL);
                [self setDepartureCode:kSOSNeverLeftCircle];
            } else if(iAmPeer) {
                change |= [self fixICloudIdentities:account circle:circle];
            }
            secnotice("updatingGenSignature", "we changed the circle? %@", change ? CFSTR("YES") : CFSTR("NO"));
            SOSIntervalEvent *iCloudCheckEvent = [self iCloudCheckEventHandle: account];
            [iCloudCheckEvent followup];
            return change;
        }];
    }
}

-(void) forEachCirclePeerExceptMe:(SOSIteratePeerBlock)block
{
    if (self.trustedCircle && self.peerInfo) {
        NSString* myPi_id = self.peerID;
        SOSCircleForEachPeer(self.trustedCircle, ^(SOSPeerInfoRef peer) {
            CFStringRef peerID = SOSPeerInfoGetPeerID(peer);
            if (peerID && ![myPi_id isEqualToString:(__bridge NSString*) peerID]) {
                block(peer);
            }
        });
    }
}

-(bool) leaveCircleWithAccount:(SOSAccount*)account withAnalytics:(NSData*)parentEvent err:(CFErrorRef*) error
{
    bool result = true;
    secnotice("circleOps", "leaveCircleWithAccount: Leaving circle by client request");
    result &= [self modifyCircle:account.circle_transport err:error action:^(SOSCircleRef circle) {
        return sosAccountLeaveCircle(account, circle, parentEvent, error);
    }];

    self.departureCode = kSOSWithdrewMembership;

    return result;
}

-(bool) leaveCircle:(SOSAccount*)account err:(CFErrorRef*) error
{
    bool result = true;
    secnotice("circleOps", "Leaving circle by client request");
    result &= [self modifyCircle:account.circle_transport err:error action:^(SOSCircleRef circle) {
        return sosAccountLeaveCircle(account, circle, nil, error);
    }];
    account.backup_key = nil;
    self.departureCode = kSOSWithdrewMembership;
    
    return result;
}

-(bool) resetToOffering:(SOSAccountTransaction*) aTxn key:(SecKeyRef)userKey err:(CFErrorRef*) error
{
    SOSFullPeerInfoPurgePersistentKey(self.fullPeerInfo, NULL);
    self.fullPeerInfo = nil;
    
    secnotice("resetToOffering", "Resetting circle to offering by request from client");
    
    return userKey && [self resetCircleToOffering:aTxn userKey:userKey err:error];
}


-(bool) resetCircleToOffering:(SOSAccountTransaction*) aTxn userKey:(SecKeyRef)user_key err:(CFErrorRef *)error
{
    bool result = false;
    
    SOSAccount* account = aTxn.account;
    if(![self hasCircle:error])
        return result;
    
    if(![self ensureFullPeerAvailable:(__bridge CFDictionaryRef)(account.gestalt) deviceID:(__bridge CFStringRef)(account.deviceID) backupKey:(__bridge CFDataRef)(account.backup_key) err:error])
        return result;
    
    (void)[self resetAllRings:account err:error];
    
    [self modifyCircle:account.circle_transport err:error action:^bool(SOSCircleRef circle) {
        bool result = false;
        SOSFullPeerInfoRef cloud_identity = NULL;
        CFErrorRef localError = NULL;
        
        require_quiet(SOSCircleResetToOffering(circle, user_key, self.fullPeerInfo, &localError), err_out);
        
        self.departureCode = kSOSNeverLeftCircle;
        
        require_quiet([self addEscrowToPeerInfo:self.fullPeerInfo err:error], err_out);
        
        require_quiet([self addiCloudIdentity:circle key:user_key err:error], err_out);
        result = true;
        SOSAccountPublishCloudParameters(account, NULL);
        account.notifyBackupOnExit = true;

    err_out:
        if (result == false)
            secerror("error resetting circle (%@) to offering: %@", circle, localError);
        if (localError && error && *error == NULL) {
            *error = localError;
            localError = NULL;
        }
        CFReleaseNull(localError);
        CFReleaseNull(cloud_identity);
        return result;
    }];
    
    SOSAccountInitializeInitialSync(account);
    SOSAccountUpdateOutOfSyncViews(aTxn, SOSViewsGetAllCurrent());
    
    result = true;

    notify_post(kSOSCCCircleOctagonKeysChangedNotification);
    
    return result;
}

void SOSAccountForEachCirclePeerExceptMe(SOSAccount* account, void (^action)(SOSPeerInfoRef peer)) {
    SOSPeerInfoRef myPi = account.peerInfo;
    SOSCircleRef circle = NULL;
    
    SOSAccountTrustClassic *trust = account.trust;
    circle = trust.trustedCircle;
    if (circle && myPi) {
        CFStringRef myPi_id = SOSPeerInfoGetPeerID(myPi);
        SOSCircleForEachPeer(circle, ^(SOSPeerInfoRef peer) {
            CFStringRef peerID = SOSPeerInfoGetPeerID(peer);
            if (peerID && !CFEqual(peerID, myPi_id)) {
                action(peer);
            }
        });
    }
}

-(bool) joinCircle:(SOSAccountTransaction*) aTxn userKey:(SecKeyRef) user_key useCloudPeer:(bool) use_cloud_peer err:(CFErrorRef*) error
{
    __block bool result = false;
    __block SOSFullPeerInfoRef cloud_full_peer = NULL;
    __block SOSAccount* account = aTxn.account;
    require_action_quiet(self.trustedCircle, fail, SOSCreateErrorWithFormat(kSOSErrorPeerNotFound, NULL, error, NULL, CFSTR("Don't have circle when joining???")));
    require_quiet([self ensureFullPeerAvailable:(__bridge CFDictionaryRef)account.gestalt deviceID:(__bridge CFStringRef)account.deviceID backupKey:(__bridge CFDataRef)account.backup_key err:error], fail);
    
    if (SOSCircleCountPeers(self.trustedCircle) == 0 || SOSAccountGhostResultsInReset(account)) {
        secnotice("resetToOffering", "Resetting circle to offering since there are no peers");
        // this also clears initial sync data
        result = [self resetCircleToOffering:aTxn userKey:user_key err:error];
    } else {
        [self setValueInExpansion:kSOSUnsyncedViewsKey value:kCFBooleanTrue err:NULL];
        
        if (use_cloud_peer) {
            cloud_full_peer = SOSCircleCopyiCloudFullPeerInfoRef(self.trustedCircle, NULL);
        }
        
        [self modifyCircle: account.circle_transport err:error action:^(SOSCircleRef circle) {
            result = SOSAccountAddEscrowToPeerInfo(account, self.fullPeerInfo, error);
            result &= SOSCircleRequestAdmission(circle, user_key, self.fullPeerInfo, error);
            self.departureCode = kSOSNeverLeftCircle;
            if(result && cloud_full_peer) {
                CFErrorRef localError = NULL;
                CFStringRef cloudid = SOSPeerInfoGetPeerID(SOSFullPeerInfoGetPeerInfo(cloud_full_peer));
                require_quiet(cloudid, finish);
                require_quiet(SOSCircleHasActivePeerWithID(circle, cloudid, &localError), finish);
                require_quiet(SOSCircleAcceptRequest(circle, user_key, cloud_full_peer, self.peerInfo, &localError), finish);
                
            finish:
                if (localError){
                    secerror("Failed to join with cloud identity: %@", localError);
                    CFReleaseNull(localError);
                }
            }
            return result;
        }];
        
        if (use_cloud_peer || SOSAccountHasCompletedInitialSync(account)) {
            SOSAccountUpdateOutOfSyncViews(aTxn, SOSViewsGetAllCurrent());
        }
    }
    
fail:
    CFReleaseNull(cloud_full_peer);
    return result;
}

@end

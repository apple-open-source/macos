//
//  SOSAccountTrustClassicExpansion.m
//  Security
//


#import <Foundation/Foundation.h>
#import "Security/SecureObjectSync/SOSAccount.h"
#import "Security/SecureObjectSync/SOSAccountTrustClassic.h"
#import "Security/SecureObjectSync/SOSAccountTrustClassic+Expansion.h"
#import "Security/SecureObjectSync/SOSAccountTrustClassic+Retirement.h"
#import "Security/SecureObjectSync/SOSAccountTrustClassic+Circle.h"
#import "Security/SecureObjectSync/SOSViews.h"
#import "Security/SecureObjectSync/SOSPeerInfoV2.h"
#import "Security/SecureObjectSync/SOSTransportCircleKVS.h"

@implementation SOSAccountTrustClassic (Expansion)
typedef enum {
    accept,
    countersign,
    leave,
    revert,
    modify,
    ignore
} ringAction_t;

#if !defined(NDEBUG)
static const char * __unused actionstring[] = {
    "accept", "countersign", "leave", "revert", "modify", "ignore",
};
#endif
static NSString* kSOSRingKey = @"trusted_rings";

//
// Generic Calls to Expansion Dictionary
//
-(CFTypeRef) getValueFromExpansion:(CFStringRef)key err:(CFErrorRef*)error
{
    if (!self.expansion) {
        return NULL;
    }
    return  (__bridge CFTypeRef)([self.expansion objectForKey:(__bridge NSString*)key]);
}

-(bool) ensureExpansion:(CFErrorRef *)error
{
    if (!self.expansion) {
        self.expansion = [NSMutableDictionary dictionary];
    }
    
    return SecAllocationError((__bridge CFTypeRef)(self.expansion), error, CFSTR("Can't Alloc Account Expansion dictionary"));
}

-(bool) clearValueFromExpansion:(CFStringRef) key err:(CFErrorRef *)error
{
    bool success = [self ensureExpansion:error];
    
    require_quiet(success, errOut);
    
    [self.expansion removeObjectForKey: (__bridge NSString*)(key)];
errOut:
    return success;
}

-(bool) setValueInExpansion:(CFStringRef) key value:(CFTypeRef) value err:(CFErrorRef *)error {
    if (value == NULL) return [self clearValueFromExpansion:key err:error];
    
    bool success = [self ensureExpansion:error];
    require_quiet(success, errOut);
    
    [self.expansion setObject:(__bridge id _Nonnull)(value) forKey:(__bridge NSString*)key];
    
errOut:
    return success;
}

-(bool) valueSetContainsValue:(CFStringRef) key value:(CFTypeRef) value
{
    CFSetRef foundSet = asSet([self getValueFromExpansion:key err:NULL], NULL);
    return foundSet && CFSetContainsValue(foundSet, value);
}

-(void) valueUnionWith:(CFStringRef) key valuesToUnion:(CFSetRef) valuesToUnion
{
    CFMutableSetRef unionedSet = CFSetCreateMutableCopy(kCFAllocatorDefault, 0, valuesToUnion);
    CFSetRef foundSet = asSet([self getValueFromExpansion:key err:NULL], NULL);
    if (foundSet) {
        CFSetUnion(unionedSet, foundSet);
    }
    [self setValueInExpansion:key value:unionedSet err:NULL];
    CFReleaseNull(unionedSet);
}

-(void) valueSubtractFrom:(CFStringRef) key valuesToSubtract:(CFSetRef) valuesToSubtract
{
    CFSetRef foundSet = asSet([self getValueFromExpansion:key err:NULL], NULL);
    if (foundSet) {
        CFMutableSetRef subtractedSet = CFSetCreateMutableCopy(kCFAllocatorDefault, 0, foundSet);
        CFSetSubtract(subtractedSet, valuesToSubtract);
        [self setValueInExpansion:key value:subtractedSet err:NULL];
        CFReleaseNull(subtractedSet);
    }
}

//Views
-(void) pendEnableViewSet:(CFSetRef) enabledViews
{
    if(CFSetGetValue(enabledViews, kSOSViewKeychainV0) != NULL) secnotice("viewChange", "Warning, attempting to Add KeychainV0");
    
    [self valueUnionWith:kSOSPendingEnableViewsToBeSetKey valuesToUnion:enabledViews];
    [self valueSubtractFrom:kSOSPendingDisableViewsToBeSetKey valuesToSubtract:enabledViews];
}

// V2 Dictionary
-(bool) updateV2Dictionary:(SOSAccount*)account v2:(CFDictionaryRef) newV2Dict
{
    if(!newV2Dict) return true;
    
    [self setValueInExpansion:kSOSTestV2Settings value:newV2Dict err:NULL];
    
    if (self.trustedCircle && self.fullPeerInfo
        && SOSFullPeerInfoUpdateV2Dictionary(self.fullPeerInfo, newV2Dict, NULL)) {
        [self modifyCircle:account.circle_transport err:NULL action:^(SOSCircleRef circle_to_change) {
            secnotice("circleChange", "Calling SOSCircleUpdatePeerInfo for gestalt change");
            return SOSCircleUpdatePeerInfo(circle_to_change, account.peerInfo);
        }];
    }
    return true;
}

//
// Rings
//

-(bool) forEachRing:(RingNameBlock)block
{
    bool retval = false;
    __block bool changed = false;
    __block CFStringRef ringname = NULL;
    __block  CFDataRef   ringder = NULL;
    __block SOSRingRef  ring = NULL;
    __block SOSRingRef  newring = NULL;
    __block CFDataRef   newringder = NULL;
    
    CFMutableDictionaryRef rings = [self getRings:NULL];
    CFMutableDictionaryRef ringscopy = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
    if(!rings){
        CFReleaseNull(ringscopy);
        return retval;
    }
    if(!ringscopy){
        CFReleaseNull(ringscopy);
        return retval;
    }
    CFDictionaryForEach(rings, ^(const void *key, const void *value) {
        ringname = (CFStringRef) key;
        ringder = CFDataCreateCopy(kCFAllocatorDefault, (CFDataRef) value);
        CFDictionaryAddValue(ringscopy, key, ringder);
        ring = SOSRingCreateFromData(NULL, ringder);
        newring = block(ringname, ring);
        if(newring) {
            newringder = SOSRingCopyEncodedData(newring, NULL);
            CFDictionaryReplaceValue(ringscopy, key, newringder);
            CFReleaseNull(newringder);
            changed = true;
        }
        CFReleaseNull(ring);
        CFReleaseNull(ringder);
        CFReleaseNull(newring);
    });
    if(changed) {
        [self setRings:ringscopy];
    }
    retval = true;
    
    CFReleaseNull(ringscopy);
    return retval;
}

-(bool) resetAllRings:(SOSAccount*)account err:(CFErrorRef *)error
{
    __block bool retval = true;
    CFMutableSetRef ringList = CFSetCreateMutableForCFTypes(kCFAllocatorDefault);
    if(!ringList){
        CFReleaseNull(ringList);
        return retval;
    }
    
    [self forEachRing: ^SOSRingRef(CFStringRef name, SOSRingRef ring) {
        CFSetAddValue(ringList, name);
        return NULL; // just using this to grab names.
    }];
    
    CFSetForEach(ringList, ^(const void *value) {
        CFStringRef ringName = (CFStringRef) value;
        retval = retval && [self resetRing:account ringName:ringName err:error];
    });
    
    CFReleaseNull(ringList);
    return retval;
}

-(bool) resetAccountToEmpty:(SOSAccount*)account transport: (SOSCircleStorageTransport*)circleTransport err:(CFErrorRef*) error
{
    
    __block bool result = true;
    
    result &= [self resetAllRings:account err:error];
    
    self.fullPeerInfo = nil;
    
    self.departureCode = kSOSWithdrewMembership;
    secnotice("circleOps", "Reset Circle to empty by client request");
    
    result &= [self modifyCircle:circleTransport err:error action:^bool(SOSCircleRef circle) {
        result = SOSCircleResetToEmpty(circle, error);
        return result;
    }];
    
    if (!result) {
        secerror("error: %@", error ? *error : NULL);
    }
    return result;
}

-(void) setRings:(CFMutableDictionaryRef) newrings
{
    [self.expansion setObject:(__bridge NSMutableDictionary*)newrings forKey:(kSOSRingKey)];
}

-(bool) checkForRings:(CFErrorRef*)error
{
    __block bool retval = true;
    CFMutableDictionaryRef rings = [self getRings:NULL];
    if(rings && isDictionary(rings)) {
        [self forEachRing:^SOSRingRef(CFStringRef ringname, SOSRingRef ring) {
            if(retval == true) {
                if(!SOSRingIsStable(ring)) {
                    retval = false;
                    secnotice("ring", "Ring %@ not stable", ringname);
                }
            }
            return NULL;
        }];
    } else {
        SOSCreateError(kSOSErrorNotReady, CFSTR("Rings not present"), NULL, error);
        retval = false;
    }
    return retval;
}

-(bool) setRing:(SOSRingRef) addRing ringName:(CFStringRef) ringName err:(CFErrorRef*)error
{
    require_quiet(addRing, errOut);
    CFMutableDictionaryRef rings = [self getRings:NULL];
    require_action_quiet(rings, errOut, SOSCreateError(kSOSErrorNoRing, CFSTR("No Rings found"), NULL, error));
    CFDataRef ringder = SOSRingCopyEncodedData(addRing, error);
    require_quiet(ringder, errOut);
    CFDictionarySetValue(rings, ringName, ringder);
    CFReleaseNull(ringder);
    return true;
errOut:
    return false;
}

static bool SOSAccountBackupSliceKeyBagNeedsFix(SOSAccount* account, SOSBackupSliceKeyBagRef bskb) {
    
    if (SOSBSKBIsDirect(bskb) || account.backup_key == NULL)
        return false;
    
    CFSetRef peers = SOSBSKBGetPeers(bskb);
    
    /* first scan for retired peers, and kick'em out!*/
    SOSAccountIsPeerRetired(account, peers);
    
    bool needsFix = true;
    
    SOSPeerInfoRef myPeer = account.peerInfo;
    if (myPeer) {
        SOSPeerInfoRef meInBag = (SOSPeerInfoRef) CFSetGetValue(peers, myPeer);
        CFDataRef myBK = SOSPeerInfoCopyBackupKey(myPeer);
        CFDataRef meInBagBK = SOSPeerInfoCopyBackupKey(meInBag);
        needsFix = !(meInBag && CFEqualSafe(myBK,
                                            meInBagBK));
        CFReleaseNull(myBK);
        CFReleaseNull(meInBagBK);
    }
    
    CFDataRef rkbg = SOSAccountCopyRecoveryPublic(kCFAllocatorDefault, account, NULL);
    if(rkbg) needsFix |= !SOSBKSBPrefixedKeyIsInKeyBag(bskb, bskbRkbgPrefix, rkbg);
    else needsFix |= SOSBSKBHasRecoveryKey(bskb); // if we don't have a recovery key - the bskb shouldn't
    CFReleaseNull(rkbg);
    
    return needsFix;
}

-(bool) handleUpdateRing:(SOSAccount*)account prospectiveRing:(SOSRingRef)prospectiveRing transport:(SOSKVSCircleStorageTransport*)circleTransport userPublicKey:(SecKeyRef)userPublic writeUpdate:(bool)writeUpdate err:(CFErrorRef *)error
{
    bool success = true;
    bool haveOldRing = true;
    
    const char * __unused localRemote = writeUpdate ? "local": "remote";
    SOSFullPeerInfoRef fpi = self.fullPeerInfo;
    SOSPeerInfoRef     pi = SOSFullPeerInfoGetPeerInfo(fpi);
    CFStringRef        peerID = SOSPeerInfoGetPeerID(pi);
    bool               peerActive = (fpi && pi && peerID && [self isInCircle:NULL]);
    SOSRingRef newRing = NULL;
    SOSRingRef oldRing = NULL;
    
    secdebug("ringSigning", "start:[%s] %@", localRemote, prospectiveRing);
    
    require_quiet(SOSAccountHasPublicKey(account, error), errOut);

    require_action_quiet(prospectiveRing, errOut,
                         SOSCreateError(kSOSErrorIncompatibleCircle, CFSTR("No Ring to work with"), NULL, error));
    
    require_action_quiet(SOSRingIsStable(prospectiveRing), errOut, SOSCreateError(kSOSErrorIncompatibleCircle, CFSTR("You give rings a bad name"), NULL, error));
    
    // We should at least have a sane ring system in the account object
    require_quiet([self checkForRings:error], errOut);
    
    CFStringRef ringName = SOSRingGetName(prospectiveRing);
    oldRing = [self copyRing:ringName err:NULL];
    
    newRing = CFRetainSafe(prospectiveRing); // TODO:  SOSAccountCloneRingWithRetirement(account, prospectiveRing, error);
    
    ringAction_t ringAction = ignore;
    
    bool userTrustedoldRing = true;
    
    CFSetRef peers = SOSCircleCopyPeers(self.trustedCircle, kCFAllocatorDefault);
    
    SecKeyRef oldKey = userPublic;
    
    if (!oldRing) {
        oldRing = CFRetainSafe(newRing);
    }
    
    SOSConcordanceStatus concstat = SOSRingConcordanceTrust(fpi, peers, oldRing, newRing, oldKey, userPublic, peerID, error);
    CFReleaseNull(peers);
    
    CFStringRef concStr = NULL;
    switch(concstat) {
        case kSOSConcordanceTrusted:
            ringAction = countersign;
            concStr = CFSTR("Trusted");
            break;
        case kSOSConcordanceGenOld:
            ringAction = userTrustedoldRing ? revert : ignore;
            concStr = CFSTR("Generation Old");
            break;
        case kSOSConcordanceBadUserSig:
        case kSOSConcordanceBadPeerSig:
            ringAction = userTrustedoldRing ? revert : accept;
            concStr = CFSTR("Bad Signature");
            break;
        case kSOSConcordanceNoUserSig:
            ringAction = userTrustedoldRing ? revert : accept;
            concStr = CFSTR("No User Signature");
            break;
        case kSOSConcordanceNoPeerSig:
            ringAction = accept; // We might like this one eventually but don't countersign.
            concStr = CFSTR("No trusted peer signature");
            secnotice("signing", "##### No trusted peer signature found, accepting hoping for concordance later %@", newRing);
            break;
        case kSOSConcordanceNoPeer:
            ringAction = leave;
            concStr = CFSTR("No trusted peer left");
            break;
        case kSOSConcordanceNoUserKey:
            secerror("##### No User Public Key Available, this shouldn't ever happen!!!");
            ringAction = ignore;
            break;
            
        case kSOSConcordanceMissingMe:
        case kSOSConcordanceImNotWorthy:
            ringAction = modify;
            concStr = CFSTR("Incorrect membership for me");
            break;
        case kSOSConcordanceInvalidMembership:
            ringAction = userTrustedoldRing ? revert : ignore;
            concStr = CFSTR("Invalid Ring Membership");
            break;
        default:
            secerror("##### Bad Error Return from ConcordanceTrust");
            ringAction = ignore;
            break;
    }
    
    (void)concStr;
    
    secdebug("ringSigning", "Decided on action [%s] based on concordance state [%@] and [%s] circle.",
             actionstring[ringAction], concStr, userTrustedoldRing ? "trusted" : "untrusted");
    
    SOSRingRef ringToPush = NULL;
    bool iWasInOldRing = peerID && SOSRingHasPeerID(oldRing, peerID);
    bool iAmInNewRing = peerID && SOSRingHasPeerID(newRing, peerID);
    bool ringIsBackup = SOSRingGetType(newRing) == kSOSRingBackup;
    bool ringIsRecovery = SOSRingGetType(newRing) == kSOSRingRecovery;
    
    if (ringIsBackup && peerActive) {
        if (ringAction == accept || ringAction == countersign) {
            CFErrorRef localError = NULL;
            SOSBackupSliceKeyBagRef bskb = SOSRingCopyBackupSliceKeyBag(newRing, &localError);
            
            if(!bskb) {
                secnotice("ringSigning", "Backup ring with no backup slice keybag (%@)", localError);
            } else if (SOSAccountBackupSliceKeyBagNeedsFix(account, bskb)) {
                ringAction = modify;
            }
            CFReleaseSafe(localError);
            CFReleaseSafe(bskb);
        }
        
        if (ringAction == modify) {
            CFErrorRef updateError = NULL;
            [self setRing:newRing ringName:ringName err:error];
            
            if(SOSAccountUpdateOurPeerInBackup(account, newRing, &updateError)) {
                secdebug("signing", "Modified backup ring to include us");
            } else {
                secerror("Could not add ourselves to the backup: (%@)", updateError);
            }
            CFReleaseSafe(updateError);
            
            // Fall through to normal modify handling.
        }
    }
    
    if (ringIsRecovery && peerActive && (ringAction == modify)) {
        [self setRing:newRing ringName:ringName err:error];
    }
    
    
    if (ringAction == modify) {
        ringAction = ignore;
    }
    
    if (ringAction == leave) {
        if (iWasInOldRing) {
            if ([self leaveRing:circleTransport ring:newRing err:error]){
                ringToPush = newRing;
            } else {
                secdebug("ringSigning", "Can't leave ring %@", oldRing);
                success = false;
            }
            ringAction = accept;
        } else {
            // We are not in this ring, but we need to update account with it, since we got it from cloud
            ringAction = accept;
        }
    }
    
    if (ringAction == countersign) {
        if (iAmInNewRing) {
            if (SOSRingPeerTrusted(newRing, fpi, NULL)) {
                secdebug("ringSigning", "Already concur with: %@", newRing);
            } else {
                CFErrorRef signingError = NULL;
                
                if (fpi && SOSRingConcordanceSign(newRing, fpi, &signingError)) {
                    ringToPush = newRing;
                } else {
                    secerror("Failed to concordance sign, error: %@  Old: %@ New: %@", signingError, oldRing, newRing);
                    success = false;
                }
                CFReleaseSafe(signingError);
            }
        } else {
            secdebug("ringSigning", "Not countersigning, not in ring: %@", newRing);
        }
        ringAction = accept;
    }
    
    if (ringAction == accept) {
        if (iWasInOldRing && !iAmInNewRing) {
            
            //  Don't destroy evidence of other code determining reason for leaving.
            //if(!SOSAccountHasLeft(account)) account.departure_code = kSOSMembershipRevoked;
            // TODO: LeaveReason for rings
        }
        
        if (pi && SOSRingHasRejection(newRing, peerID)) {
            // TODO: ReasonForLeaving for rings
            SOSRingRemoveRejection(newRing, peerID);
        }
        
        [self setRing:newRing ringName:ringName err:error];
        
        if (pi && account.accountKeyIsTrusted
            && SOSRingHasApplicant(oldRing, peerID)
            && SOSRingCountPeers(newRing) > 0
            && !iAmInNewRing && !SOSRingHasApplicant(newRing, peerID)) {
            // We weren't rejected (above would have set me to NULL.
            // We were applying and we weren't accepted.
            // Our application is declared lost, let us reapply.
            
            if (SOSRingApply(newRing, userPublic, fpi, NULL))
                if(peerActive) writeUpdate = true;
        }
        
        if (pi && SOSRingHasPeerID(oldRing, peerID)) {
            [self cleanupRetirementTickets:account circle:self.trustedCircle time:RETIREMENT_FINALIZATION_SECONDS err:NULL];
        }
        
        
        account.circle_rings_retirements_need_attention = true;
        
        if (writeUpdate)
            ringToPush = newRing;
        secnotice("circleop", "Setting account.key_interests_need_updating to true in handleUpdateRing");
        account.key_interests_need_updating = true;
    }
    
    /*
     * In the revert section we'll guard the KVS idea of circles by rejecting "bad" new rings
     * and pushing our current view of the ring (oldRing).  We'll only do this if we actually
     * are a member of oldRing - never for an empty ring.
     */
    
    if (ringAction == revert) {
        if(haveOldRing && peerActive && SOSRingHasPeerID(oldRing, peerID)) {
            secdebug("ringSigning", "%@, Rejecting: %@ re-publishing %@", concStr, newRing, oldRing);
            ringToPush = oldRing;
        } else {
            secdebug("ringSigning", "%@, Rejecting: %@ Have no old circle - would reset", concStr, newRing);
        }
    }
    
    
    if (ringToPush != NULL) {
        secdebug("ringSigning", "Pushing:[%s] %@", localRemote, ringToPush);
        CFDataRef ringData = SOSRingCopyEncodedData(ringToPush, error);
        if (ringData) {
            success &= [circleTransport kvsRingPostRing:SOSRingGetName(ringToPush) ring:ringData err:error];
        } else {
            success = false;
        }
        CFReleaseNull(ringData);
    }
    CFReleaseNull(oldRing);
    CFReleaseNull(newRing);
    return success;
errOut:
    CFReleaseNull(oldRing);
    CFReleaseNull(newRing);
    return false;
    
}

-(SOSRingRef) copyRing:(CFStringRef)ringName err:(CFErrorRef *)error
{
    CFMutableDictionaryRef rings = [self getRings:error];
    require_action_quiet(rings, errOut, SOSCreateError(kSOSErrorNoRing, CFSTR("No Rings found"), NULL, error));
    CFTypeRef ringder = CFDictionaryGetValue(rings, ringName);
    require_action_quiet(ringder, errOut, SOSCreateError(kSOSErrorNoRing, CFSTR("No Ring found"), NULL, error));
    SOSRingRef ring = SOSRingCreateFromData(NULL, ringder);
    return (SOSRingRef) ring;
    
errOut:
    return NULL;
}

-(CFMutableDictionaryRef) getRings:(CFErrorRef *)error
{
    CFMutableDictionaryRef rings = (__bridge CFMutableDictionaryRef) [self.expansion objectForKey:kSOSRingKey];
    if(!rings) {
        [self addRingDictionary];
        rings = [self getRings:error];
    }
    
    return rings;
}

-(bool) resetRing:(SOSAccount*)account ringName:(CFStringRef) ringName err:(CFErrorRef *)error
{
    bool retval = false;
    
    SOSRingRef ring = [self copyRing:ringName err:error];
    SOSRingRef newring = SOSRingCreate(ringName, NULL, SOSRingGetType(ring), error);
    SOSRingGenerationCreateWithBaseline(newring, ring);
    SOSBackupRingSetViews(newring, self.fullPeerInfo, SOSBackupRingGetViews(ring, NULL), error);
    require_quiet(newring, errOut);
    CFReleaseNull(ring);
    retval = SOSAccountUpdateRing(account, newring, error);
errOut:
    CFReleaseNull(ring);
    CFReleaseNull(newring);
    return retval;
}

-(bool) leaveRing:(SOSKVSCircleStorageTransport*)circle_transport ring:(SOSRingRef) ring err:(CFErrorRef*) error
{
    SOSFullPeerInfoRef fpi = self.fullPeerInfo;
    if(!fpi) return false;
    SOSPeerInfoRef     pi = SOSFullPeerInfoGetPeerInfo(fpi);
    CFStringRef        peerID = SOSPeerInfoGetPeerID(pi);
    
    CFErrorRef localError = NULL;
    
    bool retval = false;
    bool writeRing = false;
    bool writePeerInfo = false;
    
    if(SOSRingHasPeerID(ring, peerID)) {
        writePeerInfo = true;
    }
    
    if(writePeerInfo || writeRing) {
        SOSRingWithdraw(ring, NULL, fpi, error);
    }
    
    if (writeRing) {
        CFDataRef ring_data = SOSRingCopyEncodedData(ring, error);
        
        if (ring_data) {
            [circle_transport kvsRingPostRing:SOSRingGetName(ring) ring:ring_data err:NULL];
        }
        CFReleaseNull(ring_data);
    }
    retval = true;
    CFReleaseNull(localError);
    return retval;
}

@end

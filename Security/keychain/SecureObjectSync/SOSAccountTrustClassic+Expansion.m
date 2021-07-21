//
//  SOSAccountTrustClassicExpansion.m
//  Security
//


#import <Foundation/Foundation.h>
#import "keychain/SecureObjectSync/SOSAccount.h"
#import "keychain/SecureObjectSync/SOSAccountTrustClassic.h"
#import "keychain/SecureObjectSync/SOSAccountTrustClassic+Expansion.h"
#import "keychain/SecureObjectSync/SOSAccountTrustClassic+Retirement.h"
#import "keychain/SecureObjectSync/SOSAccountTrustClassic+Circle.h"
#import "keychain/SecureObjectSync/SOSViews.h"
#import "keychain/SecureObjectSync/SOSPeerInfoV2.h"
#import "keychain/SecureObjectSync/SOSPeerInfoCollections.h"
#import "keychain/SecureObjectSync/SOSTransportCircleKVS.h"
#import "keychain/SecureObjectSync/SOSRingRecovery.h"

@implementation SOSAccountTrustClassic (Expansion)
typedef enum {
    accept,
    countersign,
    leave,
    revert,
    modify,
    ignore
} ringAction_t;

static const char *actionstring[] = {
    "accept", "countersign", "leave", "revert", "modify", "ignore",
};
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
    CFErrorRef resetError = NULL;

    result &= [self resetAllRings:account err:&resetError];
    if(resetError){
        secerror("reset all rings error: %@", resetError);
        if(error){
            *error = resetError;
        }else{
            CFReleaseNull(resetError);
        }
    }

    self.fullPeerInfo = nil;

    self.departureCode = kSOSWithdrewMembership;
    secnotice("circleOps", "Reset Rings to empty by client request");

    result &= [self modifyCircle:circleTransport err:error action:^bool(SOSCircleRef circle) {
        result = SOSCircleResetToEmpty(circle, error);
        return result;
    }];

    if (!result) {
        secerror("error: %@", error ? *error : NULL);
    } else {
        notify_post(kSOSCCCircleOctagonKeysChangedNotification);
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

-(bool) handleUpdateRing:(SOSAccount*)account prospectiveRing:(SOSRingRef)prospectiveRing transport:(SOSKVSCircleStorageTransport*)circleTransport userPublicKey:(SecKeyRef)userPublic writeUpdate:(bool)localUpdate err:(CFErrorRef *)error
{
    bool success = false;
    bool haveOldRing = true;
    static uint recRingProcessed = 0;
    static uint bckRingProcessed = 0;
    
    const char * __unused localRemote = localUpdate ? "local": "remote";
    SOSFullPeerInfoRef fpi = self.fullPeerInfo;
    SOSPeerInfoRef     pi = SOSFullPeerInfoGetPeerInfo(fpi);
    CFStringRef        peerID = SOSPeerInfoGetPeerID(pi);
    SecKeyRef          peerPrivKey = SOSFullPeerInfoCopyDeviceKey(fpi, NULL);
    SecKeyRef          peerPubKey = SOSFullPeerInfoCopyPubKey(fpi, NULL);
    __block bool       peerActive = (fpi && pi && peerID && [self isInCircleOnly:NULL]);
    bool ringIsBackup       = SOSRingGetType(prospectiveRing) == kSOSRingBackup;
    bool ringIsRecovery     = SOSRingGetType(prospectiveRing) == kSOSRingRecovery;
    CFStringRef ringName = SOSRingGetName(prospectiveRing);
    CFMutableSetRef peers   = SOSCircleCopyPeers(self.trustedCircle, kCFAllocatorDefault); // retirement tickets and iCloud key filtered out
    CFMutableSetRef filteredPeerIDs = CFSetCreateMutableForCFTypes(kCFAllocatorDefault);
    CFMutableSetRef filteredPeerInfos = CFSetCreateMutableForSOSPeerInfosByID(kCFAllocatorDefault);
    CFStringRef ringBackupViewName = NULL;

    SOSRingRef ringToPush = NULL;
    SOSRingRef newRing = NULL;
    SOSRingRef oldRing = NULL;

    CFStringRef modifierPeerID = CFStringCreateTruncatedCopy(SOSRingGetLastModifier(prospectiveRing), 8);
    secnotice("ring", "start:[%s] modifier: %@", localRemote, modifierPeerID);
    CFReleaseNull(modifierPeerID);

    // don't act on our own echos from KVS (remote ring, our peerID as modifier)
    oldRing = [self copyRing:ringName err:NULL];
    if(!localUpdate && CFEqualSafe(peerID, SOSRingGetLastModifier(prospectiveRing)) && CFEqualSafe(oldRing, prospectiveRing)) {
        secnotice("ring", "Ceasing ring handling for an echo of our own posted ring");
        success = true;
        goto errOut;
    }
    
    require_quiet(SOSAccountHasPublicKey(account, error), errOut);
    require_action_quiet(peerPubKey, errOut, SOSCreateError(kSOSErrorPublicKeyAbsent, CFSTR("No device public key to work with"), NULL, error));
    require_action_quiet(peerPrivKey, errOut, SOSCreateError(kSOSErrorPrivateKeyAbsent, CFSTR("No device private key to work with"), NULL, error));
    require_action_quiet(prospectiveRing, errOut, SOSCreateError(kSOSErrorIncompatibleCircle, CFSTR("No Ring to work with"), NULL, error));
    require_action_quiet(SOSRingIsStable(prospectiveRing), errOut, SOSCreateError(kSOSErrorIncompatibleCircle, CFSTR("You give rings a bad name"), NULL, error));
    
    // We should at least have a sane ring system in the account object
    require_quiet([self checkForRings:error], errOut);

    if(ringIsBackup) {
        ringBackupViewName = SOSRingGetBackupView(prospectiveRing, NULL);
        peerActive &= ringBackupViewName && SOSPeerInfoIsViewPermitted(pi, ringBackupViewName) && SOSPeerInfoHasBackupKey(pi);
    }
    require_action_quiet(peerActive, errOut, success = true);

    newRing = SOSRingCopyRing(prospectiveRing, NULL);
    ringAction_t ringAction = ignore;
    
    bool userTrustedoldRing = (oldRing) ? SOSRingVerify(oldRing, peerPubKey, NULL): false;
    SecKeyRef oldKey = userPublic;

    if (!oldRing) {
        oldRing = CFRetainSafe(newRing);
    }
    
    SOSConcordanceStatus concstat = SOSRingConcordanceTrust(fpi, peers, oldRing, newRing, oldKey, userPublic, peerID, error);
    
    CFStringRef concStr = CFSTR("NA");
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
            secnotice("signing", "##### No trusted peer signature found, accepting hoping for concordance later");
            break;
        case kSOSConcordanceNoPeer:
            ringAction = leave;
            concStr = CFSTR("No trusted peer left");
            break;
        case kSOSConcordanceNoUserKey:
            secerror("##### No User Public Key Available, this shouldn't ever happen!!!");
            concStr = CFSTR("No User Public Key Available");
            ringAction = ignore;
            break;
            
        case kSOSConcordanceMissingMe:
            ringAction = modify;
            concStr = CFSTR("Incorrect membership for me");
            break;
        case kSOSConcordanceImNotWorthy:
            ringAction = leave;
            concStr = CFSTR("This peer shouldn't be in this ring since it isn't in view");
            break;
        case kSOSConcordanceInvalidMembership:
            ringAction = userTrustedoldRing ? revert : ignore;
            concStr = CFSTR("Invalid Ring Membership");
            break;
        default:
            secerror("##### Bad Error Return from ConcordanceTrust");
            concStr = CFSTR("Bad Error Return from ConcordanceTrust");
            ringAction = ignore;
            break;
    }

    secnotice("ring", "Decided on action [%s] based on concordance state [%@] and [%s] ring.",
             actionstring[ringAction], concStr, userTrustedoldRing ? "trusted" : "untrusted");

    // if we're ignoring this ring we're done
    require_action_quiet(ringAction != ignore, errOut, success = true);
    // can't really remove ourselves since we can't sign when we do - need to rely on other peers to remove us
    require_action_quiet(ringAction != leave, leaveAndAccept, ringAction = accept);

    // This will take care of modify, but we're always going to do this scan if we get this far
    CFSetRef ringPeerIDSet = SOSRingCopyPeerIDs(newRing);
    if(CFSetGetCount(ringPeerIDSet) == 0) { // this is a reset ring
        secnotice("ring", "changing state to accept - we have a reset ring");
        ringAction = accept;
    } else {
        // Get the peerIDs appropriate for the ring
        if(ringIsBackup) {
            SOSCircleForEachBackupCapablePeerForView(self.trustedCircle, userPublic, ringBackupViewName, ^(SOSPeerInfoRef peer) {
                CFSetAddValue(filteredPeerIDs, SOSPeerInfoGetPeerID(peer));
                CFSetAddValue(filteredPeerInfos, peer);
            });
        } else {
            SOSCircleForEachValidSyncingPeer(self.trustedCircle, userPublic, ^(SOSPeerInfoRef peer) {
                CFSetAddValue(filteredPeerIDs, SOSPeerInfoGetPeerID(peer));
                CFSetAddValue(filteredPeerInfos, peer);
            });
        }

        if(!CFEqual(filteredPeerIDs, ringPeerIDSet)) {
            secnotice("ring", "mismatch between filteredPeerIDs and ringPeerIDSet, fixing ring and gensigning");
            secnotice("ring", "filteredPeerIDs %@", filteredPeerIDs);
            secnotice("ring", "  ringPeerIDSet %@", ringPeerIDSet);
            SOSRingSetPeerIDs(newRing, filteredPeerIDs);
            SOSRingRemoveSignatures(newRing, NULL);
            ringAction = countersign;
        }
    }
    CFReleaseNull(ringPeerIDSet);

    if (ringAction == countersign) {
        bool stopCountersign = false;
        CFIndex peerCount = CFSetGetCount(filteredPeerIDs);

        if(peerCount > 0) {
            // Fix payloads if necessary
            if (ringIsBackup && SOSPeerInfoHasBackupKey(pi)) {
                __block bool fixBSKB = false;
                CFDataRef recoveryKeyData = SOSAccountCopyRecoveryPublic(kCFAllocatorDefault, account, NULL);
                SOSBackupSliceKeyBagRef currentBSKB = SOSRingCopyBackupSliceKeyBag(newRing, NULL);
                
                if(currentBSKB == NULL) {
                    secnotice("ring", "Backup ring contains no BSKB");
                    fixBSKB = true;
                }
                
                if(SOSBSKBAllPeersBackupKeysAreInKeyBag(currentBSKB, filteredPeerInfos) == false) {
                    secnotice("ring", "BSKB is missing some backup keys");
                    fixBSKB = true;
                }

                if(SOSBSKBHasThisRecoveryKey(currentBSKB, recoveryKeyData) == false) {
                    secnotice("ring", "BSKB is missing recovery key");
                    fixBSKB = true;
                }

                if(fixBSKB) {
                    CFErrorRef localError = NULL;
                    CFSetRef viewSet = SOSRingGetBackupViewset(newRing, NULL);
                    secnotice("ring", "Need to fix BSKB - this will prompt a gensign");

                    SOSBackupSliceKeyBagRef bskb = NULL;
                    if(recoveryKeyData) {
                        CFMutableDictionaryRef additionalKeys = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
                        CFDictionaryAddValue(additionalKeys, bskbRkbgPrefix, recoveryKeyData);
                        bskb = SOSBackupSliceKeyBagCreateWithAdditionalKeys(kCFAllocatorDefault, filteredPeerInfos, additionalKeys, error);
                        CFReleaseNull(additionalKeys);
                    } else {
                        bskb = SOSBackupSliceKeyBagCreate(kCFAllocatorDefault, filteredPeerInfos, error);
                    }

                    if(SOSRingSetBackupKeyBag(newRing, fpi, viewSet, bskb, &localError) == false) {
                        stopCountersign = true;
                        secnotice("ring", "Couldn't fix BSKB (%@)", localError);
                    }
                    SOSRingRemoveSignatures(newRing, NULL);
                    SOSRingGenerationSign(newRing, NULL, fpi, error);
                    ringToPush = newRing;
                    CFReleaseNull(localError);
                    CFReleaseNull(bskb);
                }
                CFReleaseNull(recoveryKeyData);
                CFReleaseNull(currentBSKB);
            }
        }

        if(stopCountersign) {
            ringAction = ignore;
        } else if (SOSRingPeerTrusted(newRing, fpi, NULL)) {
            secnotice("ring", "Already concur with newRing");
            ringAction = accept;
        } else {
            CFErrorRef signingError = NULL;
            if (fpi && SOSRingConcordanceSign(newRing, fpi, &signingError)) {
                secnotice("ring", "concordance signed");
                ringToPush = newRing;
                ringAction = accept;
            } else {
                secnotice("ring", "Failed to concordance sign, error: %@", signingError);
                success = false;
                ringAction = ignore;
            }
            CFReleaseSafe(signingError);
        }
    }

leaveAndAccept:
    
    if (ringAction == accept) {
        if(ringIsRecovery) {
            if(!localUpdate) { // processing a remote ring - we accept the new recovery key here
                if(SOSRingIsEmpty_Internal(newRing)) { // Reset ring will reset the recovery key
                    secnotice("ring", "Reset ring for recovery from remote peer");
                    SOSRecoveryKeyBagRef ringRKBG = SOSRecoveryKeyBagCreateForAccount(kCFAllocatorDefault, (__bridge CFTypeRef)account, SOSRKNullKey(), error);
                    SOSAccountSetRecoveryKeyBagEntry(kCFAllocatorDefault, account, ringRKBG, error);
                    CFReleaseNull(ringRKBG);
                } else {                                // normal ring recovery key harvest
                    secnotice("ring", "normal ring recovery key harvest");
                    SOSRecoveryKeyBagRef ringRKBG = SOSRingCopyRecoveryKeyBag(newRing, NULL);
                    SOSAccountSetRecoveryKeyBagEntry(kCFAllocatorDefault, account, ringRKBG, error);
                    CFReleaseNull(ringRKBG);
                }
            }
        }
        if (pi && SOSRingHasRejection(newRing, peerID)) {
            SOSRingRemoveRejection(newRing, peerID);
        }
        [self setRing:newRing ringName:ringName err:error];
        account.circle_rings_retirements_need_attention = true;
        if (localUpdate) {
            ringToPush = newRing;
        } else if (ringToPush == NULL) {
            success = true;
        }
    }
    
    /*
     * In the revert section we'll guard the KVS idea of circles by rejecting "bad" new rings
     * and pushing our current view of the ring (oldRing).  We'll only do this if we actually
     * are a member of oldRing - never for an empty ring.
     */
    
    if (ringAction == revert) {
        if(haveOldRing && SOSRingHasPeerID(oldRing, peerID)) {
            secnotice("ring", "Rejecting: %@", newRing);
            secnotice("ring", "   RePush: %@", oldRing);
            ringToPush = oldRing;
        } else {
            secnotice("ring", "Rejecting: %@", newRing);
            secnotice("ring", "Have no old ring - would reset");
        }
    }

    if (ringToPush != NULL) {
        if(ringIsBackup) {
            bckRingProcessed++;
        } else if(ringIsRecovery) {
            recRingProcessed++;
        }
        secnotice("ring", "Pushing:[%s] %@", localRemote, ringToPush);
        CFDataRef ringData = SOSRingCopyEncodedData(ringToPush, error);
        if (ringData) {
            success = [circleTransport kvsRingPostRing:SOSRingGetName(ringToPush) ring:ringData err:error];
        } else {
            success = false;
        }
        secnotice("ring", "Setting account.key_interests_need_updating to true in handleUpdateRing");
        account.key_interests_need_updating = true;
        CFReleaseNull(ringData);
    }
errOut:
    CFReleaseNull(filteredPeerIDs);
    CFReleaseNull(filteredPeerInfos);
    CFReleaseNull(oldRing);
    CFReleaseNull(newRing);
    CFReleaseNull(peers);
    CFReleaseNull(peerPubKey);
    CFReleaseNull(peerPrivKey);
    return success;
}

-(SOSRingRef) copyRing:(CFStringRef)ringName err:(CFErrorRef *)error
{
    CFMutableDictionaryRef rings = [self getRings:error];
    require_action_quiet(rings, errOut, SOSCreateError(kSOSErrorNoRing, CFSTR("No Rings found"), NULL, error));
    CFTypeRef ringder = CFDictionaryGetValue(rings, ringName);
    require_action_quiet(ringder, errOut, SOSCreateErrorWithFormat(kSOSErrorNoRing, NULL, error, NULL, CFSTR("No Ring found %@"), ringName));
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


@end

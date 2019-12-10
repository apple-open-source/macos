//
//  SOSAccountUpdate.c
//  sec
//

#include "SOSAccountPriv.h"
#include "SOSAccountLog.h"

#include "keychain/SecureObjectSync/SOSTransportCircleKVS.h"
#include "keychain/SecureObjectSync/SOSTransport.h"
#include <Security/SecureObjectSync/SOSViews.h>
#include "keychain/SecureObjectSync/SOSPeerInfoCollections.h"
#include "keychain/SecureObjectSync/SOSPeerInfoPriv.h"
#include "keychain/SecureObjectSync/SOSPeerInfoV2.h"
#include "keychain/SecureObjectSync/SOSPeerInfoDER.h"
#include <Security/SecureObjectSync/SOSBackupSliceKeyBag.h>
#import "keychain/SecureObjectSync/SOSAccountGhost.h"

#import "keychain/SecureObjectSync/SOSAccountTrust.h"
#import "keychain/SecureObjectSync/SOSAccountTrustClassic+Circle.h"
#import "keychain/SecureObjectSync/SOSAccountTrustClassic+Expansion.h"

static void DifferenceAndCall(CFSetRef old_members, CFSetRef new_members, void (^updatedCircle)(CFSetRef additions, CFSetRef removals))
{
    CFMutableSetRef additions = CFSetCreateMutableCopy(kCFAllocatorDefault, 0, new_members);
    CFMutableSetRef removals = CFSetCreateMutableCopy(kCFAllocatorDefault, 0, old_members);
    
    
    CFSetForEach(old_members, ^(const void * value) {
        CFSetRemoveValue(additions, value);
    });
    
    CFSetForEach(new_members, ^(const void * value) {
        CFSetRemoveValue(removals, value);
    });
    
    updatedCircle(additions, removals);
    
    CFReleaseSafe(additions);
    CFReleaseSafe(removals);
}
static CFMutableSetRef SOSAccountCopyIntersectedViews(CFSetRef peerViews, CFSetRef myViews) {
    __block CFMutableSetRef views = CFSetCreateMutable(NULL, 0, &kCFTypeSetCallBacks);
    if (peerViews && myViews) CFSetForEach(peerViews, ^(const void *view) {
        if (CFSetContainsValue(myViews, view)) {
            CFSetAddValue(views, view);
        }
    });
    return views;
}

static inline bool isSyncing(SOSPeerInfoRef peer, SecKeyRef upub) {
    if(!SOSPeerInfoApplicationVerify(peer, upub, NULL)) return false;
    if(SOSPeerInfoIsRetirementTicket(peer)) return false;
    return true;
}

static bool isBackupSOSRing(SOSRingRef ring)
{
    return isSOSRing(ring) && (kSOSRingBackup == SOSRingGetType(ring));
}

static void SOSAccountAppendPeerMetasForViewBackups(SOSAccount* account, CFSetRef views, CFMutableArrayRef appendTo)
{
    CFMutableDictionaryRef ringToViewTable = NULL;
    
    if([account getCircleStatus:NULL] != kSOSCCInCircle)
        return;
    
    if(!(SOSAccountHasCompletedInitialSync(account))){
        secnotice("backup", "Haven't finished initial backup syncing, not registering backup metas with engine");
        return;
    }
    if(!SOSPeerInfoV2DictionaryHasData(account.peerInfo, sBackupKeyKey)){
        secnotice("backup", "No key to backup to, we don't enable individual view backups");
        return;
    }
    ringToViewTable = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
    
    CFSetForEach(views, ^(const void *value) {
        CFStringRef viewName = value;
        if (isString(viewName) && !CFEqualSafe(viewName, kSOSViewKeychainV0)) {
            CFStringRef ringName = SOSBackupCopyRingNameForView(viewName);
            viewName = ringName;
            SOSRingRef ring = [account.trust copyRing:ringName err:NULL];
            if (ring && isBackupSOSRing(ring)) {
                CFTypeRef currentValue = (CFTypeRef) CFDictionaryGetValue(ringToViewTable, ring);
                
                if (isSet(currentValue)) {
                    CFSetAddValue((CFMutableSetRef)currentValue, viewName);
                } else {
                    CFMutableSetRef viewNameSet = CFSetCreateMutableForCFTypes(kCFAllocatorDefault);
                    CFSetAddValue(viewNameSet, viewName);
                    
                    CFDictionarySetValue(ringToViewTable, ring, viewNameSet);
                    CFReleaseNull(viewNameSet);
                }
            } else {
                secwarning("View '%@' not being backed up – ring %@:%@ not backup ring.", viewName, ringName, ring);
            }
            CFReleaseNull(ringName);
            CFReleaseNull(ring);
        }
    });
    
    CFDictionaryForEach(ringToViewTable, ^(const void *key, const void *value) {
        SOSRingRef ring = (SOSRingRef) key;
        CFSetRef viewNames = asSet(value, NULL);
        if (isSOSRing(ring) && viewNames) {
            if (SOSAccountIntersectsWithOutstanding(account, viewNames)) {
                CFStringSetPerformWithDescription(viewNames, ^(CFStringRef ringViews) {
                    secnotice("engine-notify", "Not ready, no peer meta: R: %@ Vs: %@", SOSRingGetName(ring), ringViews);
                });
            } else {
                bool meta_added = false;
                CFErrorRef create_error = NULL;
                SOSBackupSliceKeyBagRef key_bag = NULL;
                SOSPeerMetaRef newMeta = NULL;
                
                CFDataRef ring_payload = SOSRingGetPayload(ring, NULL);
                require_quiet(isData(ring_payload), skip);
                
                key_bag = SOSBackupSliceKeyBagCreateFromData(kCFAllocatorDefault, ring_payload, &create_error);
                require_quiet(key_bag, skip);
                
                newMeta = SOSPeerMetaCreateWithComponents(SOSRingGetName(ring), viewNames, ring_payload);
                require_quiet(SecAllocationError(newMeta, &create_error, CFSTR("Didn't make peer meta for: %@"), ring), skip);
                CFArrayAppendValue(appendTo, newMeta);
                
                CFStringSetPerformWithDescription(viewNames, ^(CFStringRef ringViews) {
                    secnotice("engine-notify", "Backup peer meta: R: %@ Vs: %@ VD: %@", SOSRingGetName(ring), ringViews, ring_payload);
                });
                
                meta_added = true;
                
            skip:
                if (!meta_added) {
                    CFStringSetPerformWithDescription(viewNames, ^(CFStringRef ringViews) {
                        secerror("Failed to register backup meta from %@ for views %@. Error (%@)", ring, ringViews, create_error);
                    });
                }
                CFReleaseNull(newMeta);
                CFReleaseNull(key_bag);
                CFReleaseNull(create_error);
            }
        }
    });
    
    CFReleaseNull(ringToViewTable);
}

bool SOSAccountSyncingV0(SOSAccount* account) {
    __block bool syncingV0 = false;
    SOSAccountForEachCirclePeerExceptMe(account, ^(SOSPeerInfoRef peer) {
        if (SOSPeerInfoIsEnabledView(peer, kSOSViewKeychainV0)) {
            syncingV0 = true;
        }
    });
    
    return syncingV0;
}

void SOSAccountNotifyEngines(SOSAccount* account)
{
    dispatch_assert_queue(account.queue);

    SOSAccountTrustClassic *trust = account.trust;
    SOSFullPeerInfoRef identity = trust.fullPeerInfo;
    SOSCircleRef circle = trust.trustedCircle;
    
    SOSPeerInfoRef myPi = SOSFullPeerInfoGetPeerInfo(identity);
    CFStringRef myPi_id = SOSPeerInfoGetPeerID(myPi);
    CFMutableArrayRef syncing_peer_metas = NULL;
    CFMutableArrayRef zombie_peer_metas = NULL;
    CFErrorRef localError = NULL;
    SOSPeerMetaRef myMeta = NULL;
    
    if (myPi_id && isSyncing(myPi, account.accountKey) && SOSCircleHasPeer(circle, myPi, NULL)) {
        CFMutableSetRef myViews = SOSPeerInfoCopyEnabledViews(myPi);
        
        // We add V0 views to everyone if we see a V0 peer, or a peer with the view explicity enabled
        // V2 peers shouldn't be explicity enabling the uber V0 view, though the seeds did.
        __block bool addV0Views = SOSAccountSyncingV0(account);
        
        syncing_peer_metas = CFArrayCreateMutableForCFTypes(kCFAllocatorDefault);
        zombie_peer_metas = CFArrayCreateMutableForCFTypes(kCFAllocatorDefault);
        SOSAccountForEachCirclePeerExceptMe(account, ^(SOSPeerInfoRef peer) {
            CFMutableArrayRef arrayToAddTo = isSyncing(peer, account.accountKey) ? syncing_peer_metas : zombie_peer_metas;
            
            // Compute views each peer is in that we are also in ourselves
            CFMutableSetRef peerEnabledViews = SOSPeerInfoCopyEnabledViews(peer);
            CFMutableSetRef views = SOSAccountCopyIntersectedViews(peerEnabledViews, myViews);
            CFReleaseNull(peerEnabledViews);
            
            if(addV0Views) {
                CFSetAddValue(views, kSOSViewKeychainV0);
            }
            
            CFStringSetPerformWithDescription(views, ^(CFStringRef viewsDescription) {
                secnotice("engine-notify", "Meta: %@: %@", SOSPeerInfoGetPeerID(peer), viewsDescription);
            });
            
            SOSPeerMetaRef peerMeta = SOSPeerMetaCreateWithComponents(SOSPeerInfoGetPeerID(peer), views, NULL);
            CFReleaseNull(views);
            
            CFArrayAppendValue(arrayToAddTo, peerMeta);
            CFReleaseNull(peerMeta);
        });
        
        // We don't make a backup peer meta for the magic V0 peer
        // Set up all the rest before we munge the set
        SOSAccountAppendPeerMetasForViewBackups(account, myViews, syncing_peer_metas);
        
        // If we saw someone else needing V0, we sync V0, too!
        if (addV0Views) {
            CFSetAddValue(myViews, kSOSViewKeychainV0);
        }
        
        CFStringSetPerformWithDescription(myViews, ^(CFStringRef viewsDescription) {
            secnotice("engine-notify", "My Meta: %@: %@", myPi_id, viewsDescription);
        });
        myMeta = SOSPeerMetaCreateWithComponents(myPi_id, myViews, NULL);
        CFReleaseSafe(myViews);
    }
    
    SOSEngineRef engine = SOSDataSourceFactoryGetEngineForDataSourceName(account.factory, SOSCircleGetName(circle), NULL);
    if (engine) {
        SOSEngineCircleChanged(engine, myMeta, syncing_peer_metas, zombie_peer_metas);
    }
    
    CFReleaseNull(myMeta);
    CFReleaseSafe(localError);
    
    CFReleaseNull(syncing_peer_metas);
    CFReleaseNull(zombie_peer_metas);
}


// Upcoming call to View Changes Here
void SOSAccountNotifyOfChange(SOSAccount* account, SOSCircleRef oldCircle, SOSCircleRef newCircle)
{
    account.circle_rings_retirements_need_attention = true;

    CFMutableSetRef old_members = SOSCircleCopyPeers(oldCircle, kCFAllocatorDefault);
    CFMutableSetRef new_members = SOSCircleCopyPeers(newCircle, kCFAllocatorDefault);
    
    CFMutableSetRef old_applicants = SOSCircleCopyApplicants(oldCircle, kCFAllocatorDefault);
    CFMutableSetRef new_applicants = SOSCircleCopyApplicants(newCircle, kCFAllocatorDefault);

    SOSPeerInfoRef me = account.peerInfo;
    if(me && CFSetContainsValue(new_members, me))
        SOSAccountSetValue(account, kSOSEscrowRecord, kCFNull, NULL); //removing the escrow records from the account object

    DifferenceAndCall(old_members, new_members, ^(CFSetRef added_members, CFSetRef removed_members) {
        DifferenceAndCall(old_applicants, new_applicants, ^(CFSetRef added_applicants, CFSetRef removed_applicants) {
            CFArrayForEach((__bridge CFArrayRef)(account.change_blocks), ^(const void * notificationBlock) {
                secnotice("updates", "calling change block");
                ((__bridge SOSAccountCircleMembershipChangeBlock) notificationBlock)(account, newCircle, added_members, removed_members, added_applicants, removed_applicants);
            });
        });
    });

    CFReleaseNull(old_applicants);
    CFReleaseNull(new_applicants);
    
    CFReleaseNull(old_members);
    CFReleaseNull(new_members);
}

CF_RETURNS_RETAINED
CFDictionaryRef SOSAccountHandleRetirementMessages(SOSAccount* account, CFDictionaryRef circle_retirement_messages, CFErrorRef *error) {
    SOSAccountTrustClassic* trust = account.trust;
    CFStringRef circle_name = SOSCircleGetName(trust.trustedCircle);
    CFMutableArrayRef handledRetirementIDs = CFArrayCreateMutableForCFTypes(kCFAllocatorDefault);
    // We only handle one circle, look it up:

    if(!trust.trustedCircle) // We don't fail, we intentionally handle nothing.
        return CFDictionaryCreateForCFTypes(kCFAllocatorDefault, NULL);

    CFDictionaryRef retirement_dictionary = asDictionary(CFDictionaryGetValue(circle_retirement_messages, circle_name), NULL);
    if(!retirement_dictionary)
        return CFDictionaryCreateForCFTypes(kCFAllocatorDefault, NULL);

    CFDictionaryForEach(retirement_dictionary, ^(const void *key, const void *value) {
        if(isData(value)) {
            SOSPeerInfoRef pi = SOSPeerInfoCreateFromData(NULL, error, (CFDataRef) value);
            if(pi && CFEqual(key, SOSPeerInfoGetPeerID(pi)) && SOSPeerInfoInspectRetirementTicket(pi, error)) {
                [trust.retirees addObject: (__bridge id _Nonnull)(pi)];

                account.circle_rings_retirements_need_attention = true; // Have to handle retirements.

                CFArrayAppendValue(handledRetirementIDs, key);
            }
            CFReleaseNull(pi);
        }
    });

    // If we are in the retiree list, we somehow got resurrected
    // clearly we took care of proper departure before so leave
    // and delcare that we withdrew this time.
    SOSPeerInfoRef me = account.peerInfo;

    if (me && [trust.retirees containsObject:(__bridge id _Nonnull)(me)]) {
        SOSAccountPurgeIdentity(account);
        trust.departureCode = kSOSDiscoveredRetirement;
    }

    CFDictionaryRef result = (CFArrayGetCount(handledRetirementIDs) == 0) ? CFDictionaryCreateForCFTypes(kCFAllocatorDefault, NULL)
                                                                          : CFDictionaryCreateForCFTypes(kCFAllocatorDefault, circle_name, handledRetirementIDs, NULL);

    CFReleaseNull(handledRetirementIDs);
    return result;
}

static SOSCircleRef SOSAccountCreateCircleFrom(CFStringRef circleName, CFTypeRef value, CFErrorRef *error) {
    if (value && !isData(value) && !isNull(value)) {
        secnotice("circleOps", "Value provided not appropriate for a circle");
        CFStringRef description = CFCopyTypeIDDescription(CFGetTypeID(value));
        SOSCreateErrorWithFormat(kSOSErrorUnexpectedType, NULL, error, NULL,
                                 CFSTR("Expected data or NULL got %@"), description);
        CFReleaseSafe(description);
        return NULL;
    }
    
    SOSCircleRef circle = NULL;
    if (!value || isNull(value)) {
        secnotice("circleOps", "No circle found in data: %@", value);
        circle = NULL;
    } else {
        circle = SOSCircleCreateFromData(NULL, (CFDataRef) value, error);
        if (circle) {
            CFStringRef name = SOSCircleGetName(circle);
            if (!CFEqualSafe(name, circleName)) {
                secnotice("circleOps", "Expected circle named %@, got %@", circleName, name);
                SOSCreateErrorWithFormat(kSOSErrorNameMismatch, NULL, error, NULL,
                                         CFSTR("Expected circle named %@, got %@"), circleName, name);
                CFReleaseNull(circle);
            }
        } else {
            secnotice("circleOps", "SOSCircleCreateFromData returned NULL.");
        }
    }
    return circle;
}

bool SOSAccountHandleCircleMessage(SOSAccount* account,
                                   CFStringRef circleName, CFDataRef encodedCircleMessage, CFErrorRef *error) {
    bool success = false;
    CFErrorRef localError = NULL;
    SOSCircleRef circle = SOSAccountCreateCircleFrom(circleName, encodedCircleMessage, &localError);
    if (circle) {
        success = [account.trust updateCircleFromRemote:account.circle_transport newCircle:circle err:&localError];
        CFReleaseSafe(circle);
    } else {
        secerror("NULL circle found, ignoring ...");
        success = true;  // don't pend this NULL thing.
    }

    if (!success) {
        if (isSOSErrorCoded(localError, kSOSErrorIncompatibleCircle)) {
            secerror("Incompatible circle found, abandoning membership: %@", circleName);
        }

        if (error) {
            *error = localError;
            localError = NULL;
        }

    }

    CFReleaseNull(localError);

    return success;
}

bool SOSAccountHandleParametersChange(SOSAccount* account, CFDataRef parameters, CFErrorRef *error){
    
    SecKeyRef newKey = NULL;
    CFDataRef newParameters = NULL;
    bool success = false;
    
    if(SOSAccountRetrieveCloudParameters(account, &newKey, parameters, &newParameters, error)) {
        debugDumpUserParameters(CFSTR("SOSAccountHandleParametersChange got new user key parameters:"), parameters);
        secnotice("circleOps", "SOSAccountHandleParametersChange got new public key: %@", newKey);

        if (CFEqualSafe(account.accountKey, newKey)) {
            secnotice("circleOps", "Got same public key sent our way. Ignoring.");
            success = true;
        } else if (CFEqualSafe(account.previousAccountKey, newKey)) {
            secnotice("circleOps", "Got previous public key repeated. Ignoring.");
            success = true;
        } else {
            SOSAccountSetUnTrustedUserPublicKey(account, newKey);
            CFReleaseNull(newKey);
            SOSAccountSetParameters(account, newParameters);

            if(SOSAccountRetryUserCredentials(account)) {
                secnotice("circleOps", "Successfully used cached password with new parameters");
                SOSAccountGenerationSignatureUpdate(account, error);
            } else {
                secnotice("circleOps", "Got new parameters for public key - could not find or use cached password");
                SOSAccountPurgePrivateCredential(account);
            }
            secnotice("circleop", "Setting account.key_interests_need_updating to true in SOSAccountHandleParametersChange");
            account.circle_rings_retirements_need_attention = true;
            account.key_interests_need_updating = true;

            success = true;
        }
    }
    
    CFReleaseNull(newKey);
    CFReleaseNull(newParameters);
    
    return success;
}


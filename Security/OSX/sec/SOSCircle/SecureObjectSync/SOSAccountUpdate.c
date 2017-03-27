//
//  SOSAccountUpdate.c
//  sec
//

#include "SOSAccountPriv.h"
#include "SOSAccountLog.h"

#include <Security/SecureObjectSync/SOSAccountHSAJoin.h>
#include <Security/SecureObjectSync/SOSTransportCircle.h>
#include <Security/SecureObjectSync/SOSTransport.h>
#include <Security/SecureObjectSync/SOSViews.h>
#include <Security/SecureObjectSync/SOSPeerInfoCollections.h>
#include <Security/SecureObjectSync/SOSPeerInfoPriv.h>
#include <Security/SecureObjectSync/SOSPeerInfoV2.h>
#include <Security/SecureObjectSync/SOSPeerInfoDER.h>
#include <Security/SecureObjectSync/SOSBackupSliceKeyBag.h>
#include <Security/SecureObjectSync/SOSAccountGhost.h>


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

static void SOSAccountAppendPeerMetasForViewBackups(SOSAccountRef account, CFSetRef views, CFMutableArrayRef appendTo)
{
    CFMutableDictionaryRef ringToViewTable = NULL;

    require_quiet(SOSAccountIsInCircle(account, NULL), done);

    require_action_quiet(SOSAccountHasCompletedRequiredBackupSync(account), done,
                         secnotice("backup", "Haven't finished initial backup syncing, not registering backup metas with engine"));

    require_action_quiet(SOSPeerInfoV2DictionaryHasData(SOSAccountGetMyPeerInfo(account), sBackupKeyKey), done,
                         secnotice("backup", "No key to backup to, we don't enable individual view backups"));

    ringToViewTable = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);

    CFSetForEach(views, ^(const void *value) {
        CFStringRef viewName = value;
        if (isString(viewName) && !CFEqualSafe(viewName, kSOSViewKeychainV0)) {
            CFStringRef ringName = SOSBackupCopyRingNameForView(viewName);
            viewName = ringName;
            SOSRingRef ring = SOSAccountCopyRing(account, ringName, NULL);
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

done:
    CFReleaseNull(ringToViewTable);
}

bool SOSAccountSyncingV0(SOSAccountRef account) {
    __block bool syncingV0 = false;
    SOSAccountForEachCirclePeerExceptMe(account, ^(SOSPeerInfoRef peer) {
        if (SOSPeerInfoIsEnabledView(peer, kSOSViewKeychainV0)) {
            syncingV0 = true;
        }
    });

    return syncingV0;
}

void SOSAccountNotifyEngines(SOSAccountRef account)
{
    SOSPeerInfoRef myPi = SOSFullPeerInfoGetPeerInfo(account->my_identity);
    CFStringRef myPi_id = SOSPeerInfoGetPeerID(myPi);
    CFMutableArrayRef syncing_peer_metas = NULL;
    CFMutableArrayRef zombie_peer_metas = NULL;
    CFErrorRef localError = NULL;
    SOSPeerMetaRef myMeta = NULL;

    if (myPi_id && isSyncing(myPi, account->user_public) && SOSCircleHasPeer(account->trusted_circle, myPi, NULL)) {
        CFMutableSetRef myViews = SOSPeerInfoCopyEnabledViews(myPi);

        // We add V0 views to everyone if we see a V0 peer, or a peer with the view explicity enabled
        // V2 peers shouldn't be explicity enabling the uber V0 view, though the seeds did.
        __block bool addV0Views = SOSAccountSyncingV0(account);

        syncing_peer_metas = CFArrayCreateMutableForCFTypes(kCFAllocatorDefault);
        zombie_peer_metas = CFArrayCreateMutableForCFTypes(kCFAllocatorDefault);
        SOSAccountForEachCirclePeerExceptMe(account, ^(SOSPeerInfoRef peer) {
            CFMutableArrayRef arrayToAddTo = isSyncing(peer, account->user_public) ? syncing_peer_metas : zombie_peer_metas;

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

    SOSEngineRef engine = SOSDataSourceFactoryGetEngineForDataSourceName(account->factory, SOSCircleGetName(account->trusted_circle), NULL);
    if (engine) {
        SOSEngineCircleChanged(engine, myMeta, syncing_peer_metas, zombie_peer_metas);
    }

    CFReleaseNull(myMeta);
    CFReleaseSafe(localError);
    CFReleaseNull(syncing_peer_metas);
    CFReleaseNull(zombie_peer_metas);
}

// Upcoming call to View Changes Here
static void SOSAccountNotifyOfChange(SOSAccountRef account, SOSCircleRef oldCircle, SOSCircleRef newCircle)
{
    account->circle_rings_retirements_need_attention = true;

    CFMutableSetRef old_members = SOSCircleCopyPeers(oldCircle, kCFAllocatorDefault);
    CFMutableSetRef new_members = SOSCircleCopyPeers(newCircle, kCFAllocatorDefault);
    
    CFMutableSetRef old_applicants = SOSCircleCopyApplicants(oldCircle, kCFAllocatorDefault);
    CFMutableSetRef new_applicants = SOSCircleCopyApplicants(newCircle, kCFAllocatorDefault);

    SOSPeerInfoRef me = SOSAccountGetMyPeerInfo(account);
    if(me && CFSetContainsValue(new_members, me))
        SOSAccountSetValue(account, kSOSEscrowRecord, kCFNull, NULL); //removing the escrow records from the account object

    DifferenceAndCall(old_members, new_members, ^(CFSetRef added_members, CFSetRef removed_members) {
        DifferenceAndCall(old_applicants, new_applicants, ^(CFSetRef added_applicants, CFSetRef removed_applicants) {
            CFArrayForEach(account->change_blocks, ^(const void * notificationBlock) {
                secnotice("updates", "calling change block");
                ((SOSAccountCircleMembershipChangeBlock) notificationBlock)(newCircle, added_members, removed_members, added_applicants, removed_applicants);
            });
        });
    });

    CFReleaseNull(old_applicants);
    CFReleaseNull(new_applicants);
    
    CFReleaseNull(old_members);
    CFReleaseNull(new_members);
}

CF_RETURNS_RETAINED
CFDictionaryRef SOSAccountHandleRetirementMessages(SOSAccountRef account, CFDictionaryRef circle_retirement_messages, CFErrorRef *error) {
    CFStringRef circle_name = SOSCircleGetName(account->trusted_circle);
    CFMutableArrayRef handledRetirementIDs = CFArrayCreateMutableForCFTypes(kCFAllocatorDefault);
    // We only handle one circle, look it up:

    require_quiet(account->trusted_circle, finish); // We don't fail, we intentionally handle nothing.
    CFDictionaryRef retirement_dictionary = asDictionary(CFDictionaryGetValue(circle_retirement_messages, circle_name), error);
    require_quiet(retirement_dictionary, finish);
    CFDictionaryForEach(retirement_dictionary, ^(const void *key, const void *value) {
        if(isData(value)) {
            SOSPeerInfoRef pi = SOSPeerInfoCreateFromData(NULL, error, (CFDataRef) value);
            if(pi && CFEqual(key, SOSPeerInfoGetPeerID(pi)) && SOSPeerInfoInspectRetirementTicket(pi, error)) {
                CFSetAddValue(account->retirees, pi);

                account->circle_rings_retirements_need_attention = true; // Have to handle retirements.

                CFArrayAppendValue(handledRetirementIDs, key);
            }
            CFReleaseNull(pi);
        }
    });

    // If we are in the retiree list, we somehow got resurrected
    // clearly we took care of proper departure before so leave
    // and delcare that we withdrew this time.
    SOSPeerInfoRef me = SOSAccountGetMyPeerInfo(account);
    if (me && CFSetContainsValue(account->retirees, me)) {
        SOSAccountPurgeIdentity(account);
        account->departure_code = kSOSDiscoveredRetirement;
    }

finish:
    {
    CFDictionaryRef result = (CFArrayGetCount(handledRetirementIDs) == 0) ? CFDictionaryCreateForCFTypes(kCFAllocatorDefault, NULL)
                                                                          : CFDictionaryCreateForCFTypes(kCFAllocatorDefault, circle_name, handledRetirementIDs, NULL);

    CFReleaseNull(handledRetirementIDs);
    return result;
    }
}

static SOSCircleRef SOSAccountCreateCircleFrom(CFStringRef circleName, CFTypeRef value, CFErrorRef *error) {
    if (value && !isData(value) && !isNull(value)) {
        secnotice("circleCreat", "Value provided not appropriate for a circle");
        CFStringRef description = CFCopyTypeIDDescription(CFGetTypeID(value));
        SOSCreateErrorWithFormat(kSOSErrorUnexpectedType, NULL, error, NULL,
                                 CFSTR("Expected data or NULL got %@"), description);
        CFReleaseSafe(description);
        return NULL;
    }
    
    SOSCircleRef circle = NULL;
    if (!value || isNull(value)) {
        secnotice("circleCreat", "No circle found in data: %@", value);
        circle = NULL;
    } else {
        circle = SOSCircleCreateFromData(NULL, (CFDataRef) value, error);
        if (circle) {
            CFStringRef name = SOSCircleGetName(circle);
            if (!CFEqualSafe(name, circleName)) {
                secnotice("circleCreat", "Expected circle named %@, got %@", circleName, name);
                SOSCreateErrorWithFormat(kSOSErrorNameMismatch, NULL, error, NULL,
                                         CFSTR("Expected circle named %@, got %@"), circleName, name);
                CFReleaseNull(circle);
            }
        } else {
            secnotice("circleCreat", "SOSCircleCreateFromData returned NULL.");
        }
    }
    return circle;
}

bool SOSAccountHandleCircleMessage(SOSAccountRef account,
                                   CFStringRef circleName, CFDataRef encodedCircleMessage, CFErrorRef *error) {
    bool success = false;
    CFErrorRef localError = NULL;
    SOSCircleRef circle = SOSAccountCreateCircleFrom(circleName, encodedCircleMessage, &localError);
    if (circle) {
        success = SOSAccountUpdateCircleFromRemote(account, circle, &localError);
        CFReleaseSafe(circle);
    } else {
        secerror("NULL circle found, ignoring ...");
        success = true;  // don't pend this NULL thing.
    }

    if (!success) {
        if (isSOSErrorCoded(localError, kSOSErrorIncompatibleCircle)) {
            secerror("Incompatible circle found, abandoning membership: %@", circleName);
            CFReleaseNull(account->my_identity);
            CFReleaseNull(account->trusted_circle);
        }

        if (error) {
            *error = localError;
            localError = NULL;
        }

    }

    CFReleaseNull(localError);

    return success;
}

bool SOSAccountHandleParametersChange(SOSAccountRef account, CFDataRef parameters, CFErrorRef *error){
    
    SecKeyRef newKey = NULL;
    CFDataRef newParameters = NULL;
    bool success = false;
    
    if(SOSAccountRetrieveCloudParameters(account, &newKey, parameters, &newParameters, error)) {
        debugDumpUserParameters(CFSTR("SOSAccountHandleParametersChange got new user key parameters:"), parameters);
        secnotice("keygen", "SOSAccountHandleParametersChange got new public key: %@", newKey);

        if (CFEqualSafe(account->user_public, newKey)) {
            secnotice("updates", "Got same public key sent our way. Ignoring.");
            success = true;
        } else if (CFEqualSafe(account->previous_public, newKey)) {
            secnotice("updates", "Got previous public key repeated. Ignoring.");
            success = true;
        } else {
            SOSAccountSetUnTrustedUserPublicKey(account, newKey);
            SOSAccountSetParameters(account, newParameters);
            newKey = NULL;

            if(SOSAccountRetryUserCredentials(account)) {
                secnotice("keygen", "Successfully used cached password with new parameters");
                SOSAccountGenerationSignatureUpdate(account, error);
            } else {
                SOSAccountPurgePrivateCredential(account);
                secnotice("keygen", "Got new parameters for public key - could not find or use cached password");
            }

            account->circle_rings_retirements_need_attention = true;
            account->key_interests_need_updating = true;

            success = true;
        }
    }
    
    CFReleaseNull(newKey);
    CFReleaseNull(newParameters);
    
    return success;
}

static inline bool SOSAccountHasLeft(SOSAccountRef account) {
    switch(account->departure_code) {
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

static const char *concordstring[] = {
    "kSOSConcordanceTrusted",
    "kSOSConcordanceGenOld",     // kSOSErrorReplay
    "kSOSConcordanceNoUserSig",  // kSOSErrorBadSignature
    "kSOSConcordanceNoUserKey",  // kSOSErrorNoKey
    "kSOSConcordanceNoPeer",     // kSOSErrorPeerNotFound
    "kSOSConcordanceBadUserSig", // kSOSErrorBadSignature
    "kSOSConcordanceBadPeerSig", // kSOSErrorBadSignature
    "kSOSConcordanceNoPeerSig",
    "kSOSConcordanceWeSigned",
};


bool SOSAccountHandleUpdateCircle(SOSAccountRef account, SOSCircleRef prospective_circle, bool writeUpdate, CFErrorRef *error)
{
    bool success = true;
    bool haveOldCircle = true;
    const char *local_remote = writeUpdate ? "local": "remote";

    secnotice("signing", "start:[%s]", local_remote);
    if (!account->user_public || !account->user_public_trusted) {
        SOSCreateError(kSOSErrorPublicKeyAbsent, CFSTR("Can't handle updates with no trusted public key here"), NULL, error);
        return false;
    }
    
    if (!prospective_circle) {
        secerror("##### Can't update to a NULL circle ######");
        return false; // Can't update one we don't have.
    }
    
    CFStringRef newCircleName = SOSCircleGetName(prospective_circle);
    SOSCircleRef oldCircle = account->trusted_circle;
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
    
    
    SOSTransportCircleRef transport = account->circle_transport;

    SOSAccountScanForRetired(account, prospective_circle, error);
    SOSCircleRef newCircle = SOSAccountCloneCircleWithRetirement(account, prospective_circle, error);
    if(!newCircle) return false;
    
    SOSCircleRef ghostCleaned = SOSAccountCloneCircleWithoutMyGhosts(account, newCircle);
    if(ghostCleaned) {
        CFRetainAssign(newCircle, ghostCleaned);
        writeUpdate = true;
    }

    SOSFullPeerInfoRef me_full = account->my_identity;
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
    if(SOSCircleVerify(oldCircle, account->user_public, NULL)) old_circle_key = account->user_public;
    else if(account->previous_public && SOSCircleVerify(oldCircle, account->previous_public, NULL)) old_circle_key = account->previous_public;
    bool userTrustedOldCircle = (old_circle_key != NULL) && haveOldCircle;
    
    SOSConcordanceStatus concstat =
    SOSCircleConcordanceTrust(oldCircle, newCircle,
                              old_circle_key, account->user_public,
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
    
    secnotice("signing", "Decided on action [%s] based on concordance state [%s] and [%s] circle.  My PeerID is %@", actionstring[circle_action], concordstring[concstat], userTrustedOldCircle ? "trusted" : "untrusted", myPeerID);
    
    SOSCircleRef circleToPush = NULL;

    if (circle_action == leave) {
        circle_action = ignore; (void) circle_action; // Acknowledge this is a dead store.
        
        if (me && SOSCircleHasPeer(oldCircle, me, NULL)) {
            secnotice("account", "Leaving circle with peer %@", me);
            debugDumpCircle(CFSTR("oldCircle"), oldCircle);
            debugDumpCircle(CFSTR("newCircle"), newCircle);
            debugDumpCircle(CFSTR("prospective_circle"), prospective_circle);
            secnotice("account", "Key state: user_public %@, previous_public %@, old_circle_key %@",
                      account->user_public, account->previous_public, old_circle_key);

            if (sosAccountLeaveCircle(account, newCircle, error)) {
                secnotice("leaveCircle", "Leaving circle by newcircle state");
                circleToPush = newCircle;
            } else {
                secnotice("signing", "Can't leave circle, but dumping identities");
                success = false;
            }
            account->departure_code = leave_reason;
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
            
            if(SOSAccountVerifyAndAcceptHSAApplicants(account, newCircle, error)) {
                circleToPush = newCircle;
                writeUpdate = true;
            }
        } else {
            secnotice("signing", "Not countersigning, not in new circle");
            debugDumpCircle(CFSTR("circle to countersign"), newCircle);
        }
        circle_action = accept;
    }
    
    if (circle_action == accept) {
        if (me && SOSCircleHasActivePeer(oldCircle, me, NULL) && !SOSCircleHasPeer(newCircle, me, NULL)) {
            //  Don't destroy evidence of other code determining reason for leaving.
            if(!SOSAccountHasLeft(account)) account->departure_code = kSOSMembershipRevoked;
            secnotice("account", "Member of old circle but not of new circle");
            debugDumpCircle(CFSTR("oldCircle"), oldCircle);
            debugDumpCircle(CFSTR("newCircle"), newCircle);
        }
        
        if (me
            && SOSCircleHasActivePeer(oldCircle, me, NULL)
            && !(SOSCircleCountPeers(oldCircle) == 1 && SOSCircleHasPeer(oldCircle, me, NULL)) // If it was our offering, don't change ID to avoid ghosts
            && !SOSCircleHasPeer(newCircle, me, NULL) && !SOSCircleHasApplicant(newCircle, me, NULL)) {
            secnotice("circle", "Purging my peer (ID: %@) for circle '%@'!!!", SOSPeerInfoGetPeerID(me), SOSCircleGetName(oldCircle));
            if (account->my_identity)
                SOSFullPeerInfoPurgePersistentKey(account->my_identity, NULL);
            CFReleaseNull(account->my_identity);
            me = NULL;
            me_full = NULL;
        }
        
        if (me && SOSCircleHasRejectedApplicant(newCircle, me, NULL)) {
            SOSPeerInfoRef  reject = SOSCircleCopyRejectedApplicant(newCircle, me, NULL);
            if(CFEqualSafe(reject, me) && SOSPeerInfoApplicationVerify(me, account->user_public, NULL)) {
                secnotice("circle", "Rejected, Purging my applicant peer (ID: %@) for circle '%@'", SOSPeerInfoGetPeerID(me), SOSCircleGetName(oldCircle));
                debugDumpCircle(CFSTR("oldCircle"), oldCircle);
                debugDumpCircle(CFSTR("newCircle"), newCircle);
                if (account->my_identity)
                    SOSFullPeerInfoPurgePersistentKey(account->my_identity, NULL);
                CFReleaseNull(account->my_identity);
                me = NULL;
                me_full = NULL;
            } else {
                secnotice("circle", "Rejected, Reapplying (ID: %@) for circle '%@'", SOSPeerInfoGetPeerID(me), SOSCircleGetName(oldCircle));
                debugDumpCircle(CFSTR("oldCircle"), oldCircle);
                debugDumpCircle(CFSTR("newCircle"), newCircle);
                SOSCircleRequestReadmission(newCircle, account->user_public, me, NULL);
                writeUpdate = true;
            }
        }
        
        CFRetainSafe(oldCircle);
        CFRetainAssign(account->trusted_circle, newCircle);
        SOSAccountSetPreviousPublic(account);
        
        secnotice("signing", "%@, Accepting new circle", concStr);
        
        if (me && account->user_public_trusted
            && SOSCircleHasApplicant(oldCircle, me, NULL)
            && SOSCircleCountPeers(newCircle) > 0
            && !SOSCircleHasPeer(newCircle, me, NULL) && !SOSCircleHasApplicant(newCircle, me, NULL)) {
            // We weren't rejected (above would have set me to NULL.
            // We were applying and we weren't accepted.
            // Our application is declared lost, let us reapply.
            
            secnotice("signing", "requesting readmission to new circle");
            if (SOSCircleRequestReadmission(newCircle, account->user_public, me, NULL))
                writeUpdate = true;
        }
        
        if (me && SOSCircleHasActivePeer(oldCircle, me, NULL)) {
            SOSAccountCleanupRetirementTickets(account, RETIREMENT_FINALIZATION_SECONDS, NULL);
        }

        SOSAccountNotifyOfChange(account, oldCircle, newCircle);
        
        CFReleaseNull(oldCircle);
        
        if (writeUpdate)
            circleToPush = newCircle;
        account->key_interests_need_updating = true;
    }
    
    /*
     * In the revert section we'll guard the KVS idea of circles by rejecting "bad" new circles
     * and pushing our current view of the circle (oldCircle).  We'll only do this if we actually
     * are a member of oldCircle - never for an empty circle.
     */
    
    if (circle_action == revert) {
        if(haveOldCircle && me && SOSCircleHasActivePeer(oldCircle, me, NULL)) {
            secnotice("signing", "%@, Rejecting new circle, re-publishing old circle", concStr);
            debugDumpCircle(CFSTR("oldCircle"), oldCircle);
            debugDumpCircle(CFSTR("newCircle"), newCircle);
            circleToPush = oldCircle;
        } else {
            secnotice("canary", "%@, Rejecting: new circle Have no old circle - would reset", concStr);
        }
    }
    
    
    if (circleToPush != NULL) {
        secnotice("signing", "Pushing:[%s]", local_remote);
        CFDataRef circle_data = SOSCircleCopyEncodedData(circleToPush, kCFAllocatorDefault, error);
        
        if (circle_data) {
            // Ensure we flush changes
            account->circle_rings_retirements_need_attention = true;

            //recording circle we are pushing in KVS
            success &= SOSTransportCircleRecordLastCirclePushedInKVS(transport, SOSCircleGetName(circleToPush), circle_data);
            //posting new circle to peers
            success &= SOSTransportCirclePostCircle(transport, SOSCircleGetName(circleToPush), circle_data, error);
        } else {
            success = false;
        }
        CFReleaseNull(circle_data);
    }
    
    CFReleaseSafe(newCircle);
    CFReleaseNull(emptyCircle);
    
    return success;
}

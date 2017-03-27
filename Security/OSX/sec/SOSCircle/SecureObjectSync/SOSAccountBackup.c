//
//  SOSAccountCircles.c
//  sec
//

#include "SOSAccountPriv.h"
#include "SOSCloudKeychainClient.h"

#include <Security/SecureObjectSync/SOSBackupSliceKeyBag.h>
#include <Security/SecureObjectSync/SOSPeerInfoCollections.h>
#include <Security/SecureObjectSync/SOSViews.h>

#include "SOSInternal.h"



//
// MARK: V0 Keybag keychain stuff
//
static bool SecItemUpdateOrAdd(CFDictionaryRef query, CFDictionaryRef update, CFErrorRef *error)
{
    OSStatus saveStatus = SecItemUpdate(query, update);
    
    if (errSecItemNotFound == saveStatus) {
        CFMutableDictionaryRef add = CFDictionaryCreateMutableCopy(kCFAllocatorDefault, 0, query);
        CFDictionaryForEach(update, ^(const void *key, const void *value) {
            CFDictionaryAddValue(add, key, value);
        });
        saveStatus = SecItemAdd(add, NULL);
        CFReleaseNull(add);
    }
    
    return SecError(saveStatus, error, CFSTR("Error saving %@"), query);
}

static CFDictionaryRef SOSCopyV0Attributes() {
    return  CFDictionaryCreateForCFTypes(kCFAllocatorDefault,
                                         kSecClass,           kSecClassGenericPassword,
                                         kSecAttrAccessGroup, CFSTR("com.apple.sbd"),
                                         kSecAttrAccessible,  kSecAttrAccessibleWhenUnlocked,
                                         kSecAttrAccount,     CFSTR("SecureBackupPublicKeybag"),
                                         kSecAttrService,     CFSTR("SecureBackupService"),
                                         kSecAttrSynchronizable, kCFBooleanTrue,
                                         NULL);
}

bool SOSDeleteV0Keybag(CFErrorRef *error) {
    CFDictionaryRef attributes = SOSCopyV0Attributes();
    
    OSStatus result = SecItemDelete(attributes);
    
    CFReleaseNull(attributes);
    
    return SecError(result != errSecItemNotFound ? result : errSecSuccess, error, CFSTR("Deleting V0 Keybag failed - %ld"), result);
}

static bool SOSSaveV0Keybag(CFDataRef v0Keybag, CFErrorRef *error) {
    CFDictionaryRef attributes = SOSCopyV0Attributes();
    
    CFDictionaryRef update = CFDictionaryCreateForCFTypes(kCFAllocatorDefault,
                                                          kSecValueData,           v0Keybag,
                                                          NULL);
    
    
    bool result = SecItemUpdateOrAdd(attributes, update, error);
    CFReleaseNull(attributes);
    CFReleaseNull(update);
    
    return result;
}


static bool SOSPeerInfoIsViewBackupEnabled(SOSPeerInfoRef peerInfo, CFStringRef viewName) {
    if (CFEqualSafe(kSOSViewKeychainV0, viewName))
        return false;

    return SOSPeerInfoHasBackupKey(peerInfo) && SOSPeerInfoIsViewPermitted(peerInfo, viewName);
}

static CFSetRef SOSAccountCopyBackupPeersForView(SOSAccountRef account, CFStringRef viewName) {
    CFMutableSetRef backupPeers = CFSetCreateMutableForSOSPeerInfosByID(kCFAllocatorDefault);

    SOSCircleRef circle = SOSAccountGetCircle(account, NULL);

    require_quiet(circle, exit);

    SOSCircleForEachValidPeer(circle, account->user_public, ^(SOSPeerInfoRef peer) {
        if (SOSPeerInfoIsViewBackupEnabled(peer, viewName))
            CFSetAddValue(backupPeers, peer);
    });

exit:
    return backupPeers;
}

static void SOSAccountWithBackupPeersForView(SOSAccountRef account, CFStringRef viewName, void (^action)(CFSetRef peers)) {
    CFSetRef backupPeersForView = SOSAccountCopyBackupPeersForView(account, viewName);

    action(backupPeersForView);

    CFReleaseNull(backupPeersForView);
}


static bool SOSAccountWithBSKBForView(SOSAccountRef account, CFStringRef viewName, CFErrorRef *error,
                                      bool (^action)(SOSBackupSliceKeyBagRef bskb, CFErrorRef *error)) {
    __block SOSBackupSliceKeyBagRef bskb = NULL;
    bool result = false;
    CFDataRef rkbg = SOSAccountCopyRecoveryPublic(kCFAllocatorDefault, account, error);

    SOSAccountWithBackupPeersForView(account, viewName, ^(CFSetRef peers) {
        if(! rkbg) {
            bskb = SOSBackupSliceKeyBagCreate(kCFAllocatorDefault, peers, error);
        } else {
            CFMutableDictionaryRef additionalKeys = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
            CFDictionaryAddValue(additionalKeys, bskbRkbgPrefix, rkbg);
            bskb = SOSBackupSliceKeyBagCreateWithAdditionalKeys(kCFAllocatorDefault, peers, additionalKeys, error);
            CFReleaseNull(additionalKeys);
        }
    });
    CFReleaseNull(rkbg);

    require_quiet(bskb, exit);

    action(bskb, error);

    result = true;

exit:
    CFReleaseNull(bskb);
    return result;
}

CFStringRef SOSBackupCopyRingNameForView(CFStringRef viewName) {
    return CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("%@-tomb"), viewName);
}

static bool SOSAccountUpdateBackupRing(SOSAccountRef account, CFStringRef viewName, CFErrorRef *error,
                                       SOSRingRef (^modify)(SOSRingRef existing, CFErrorRef *error)) {

    CFStringRef ringName = SOSBackupCopyRingNameForView(viewName);

    bool result = SOSAccountUpdateNamedRing(account, ringName, error, ^SOSRingRef(CFStringRef ringName, CFErrorRef *error) {
        return SOSRingCreate(ringName, SOSAccountGetMyPeerID(account), kSOSRingBackup, error);
    }, modify);

    CFReleaseNull(ringName);

    return result;
}

static CFSetRef SOSAccountCopyPeerSetForView(SOSAccountRef account, CFStringRef viewName) {
    CFMutableSetRef result = CFSetCreateMutableForSOSPeerInfosByID(kCFAllocatorDefault);
    
    if (account->trusted_circle) {
        SOSCircleForEachPeer(account->trusted_circle, ^(SOSPeerInfoRef peer) {
            if (CFSetContainsValue(SOSPeerInfoGetPermittedViews(peer), viewName)) {
                CFSetAddValue(result, peer);
            }
        });
    }
    
    return result;
}

static bool SOSAccountSetKeybagForViewBackupRing(SOSAccountRef account, CFStringRef viewName, SOSBackupSliceKeyBagRef keyBag, CFErrorRef *error) {
    CFMutableSetRef backupViewSet = CFSetCreateMutableForCFTypes(NULL);
    bool result = false;
    require_quiet(SecAllocationError(backupViewSet, error, CFSTR("No backup view set created")), errOut);
    CFSetAddValue(backupViewSet, viewName);

    result = SOSAccountUpdateBackupRing(account, viewName, error, ^SOSRingRef(SOSRingRef existing, CFErrorRef *error) {
        SOSRingRef newRing = NULL;
        CFSetRef viewPeerSet = SOSAccountCopyPeerSetForView(account, viewName);
        CFMutableSetRef cleared = CFSetCreateMutableForCFTypes(NULL);

        SOSRingSetPeerIDs(existing, cleared);
        SOSRingAddAll(existing, viewPeerSet);

        require_quiet(SOSRingSetBackupKeyBag(existing, SOSAccountGetMyFullPeerInfo(account), backupViewSet, keyBag, error), exit);

        newRing = CFRetainSafe(existing);
    exit:
        CFReleaseNull(viewPeerSet);
        CFReleaseNull(cleared);
        return newRing;
    });
    
errOut:
    
    if (result && NULL != error && NULL != *error) {
        secerror("Got Success and Error (dropping error): %@", *error);
        CFReleaseNull(*error);
    }
    
    if (!result) {
        secnotice("backupring", "Got error setting keybag for backup view '%@': %@", viewName, error ? (CFTypeRef) *error : (CFTypeRef) CFSTR("No error space."));
    }

    CFReleaseNull(backupViewSet);
    return result;
}

bool SOSAccountNewBKSBForView(SOSAccountRef account, CFStringRef viewName, CFErrorRef *error)
{
    return SOSAccountWithBSKBForView(account, viewName, error, ^(SOSBackupSliceKeyBagRef bskb, CFErrorRef *error) {
        bool result = SOSAccountSetKeybagForViewBackupRing(account, viewName, bskb, error);
        return result;
    });
}

bool SOSAccountIsBackupRingEmpty(SOSAccountRef account, CFStringRef viewName) {
    CFStringRef backupRing = SOSBackupCopyRingNameForView(viewName);
    SOSRingRef ring = SOSAccountCopyRing(account, backupRing, NULL);
    CFReleaseNull(backupRing);
    int peercnt = 0;
    if(ring) peercnt = SOSRingCountPeers(ring);
    CFReleaseNull(ring);
    return peercnt == 0;
}

bool SOSAccountUpdatePeerInfo(SOSAccountRef account, CFStringRef updateDescription, CFErrorRef *error, bool (^update)(SOSFullPeerInfoRef fpi, CFErrorRef *error)) {
    if (account->my_identity == NULL)
        return true;

    bool result = update(account->my_identity, error);

    if (result && SOSAccountHasCircle(account, NULL)) {
        return SOSAccountModifyCircle(account, error, ^(SOSCircleRef circle_to_change) {
            secnotice("circleChange", "Calling SOSCircleUpdatePeerInfo for %@", updateDescription);
            return SOSCircleUpdatePeerInfo(circle_to_change, SOSAccountGetMyPeerInfo(account));
        });
    }

    return result;
}

bool SOSAccountIsMyPeerInBackupAndCurrentInView(SOSAccountRef account, CFStringRef viewname){
    bool result = false;
    CFErrorRef bsError = NULL;
    CFDataRef backupSliceData = NULL;
    SOSRingRef ring = NULL;
    SOSBackupSliceKeyBagRef backupSlice = NULL;
    
    require_quiet(SOSPeerInfoIsViewBackupEnabled(SOSAccountGetMyPeerInfo(account), viewname), errOut);
    
    CFStringRef ringName = SOSBackupCopyRingNameForView(viewname);
    ring = SOSAccountCopyRing(account, ringName, &bsError);
    CFReleaseNull(ringName);
    
    require_quiet(ring, errOut);
    
    //grab the backup slice from the ring
    backupSliceData = SOSRingGetPayload(ring, &bsError);
    require_quiet(backupSliceData, errOut);

    backupSlice = SOSBackupSliceKeyBagCreateFromData(kCFAllocatorDefault, backupSliceData, &bsError);
    require_quiet(backupSlice, errOut);
    
    CFSetRef peers = SOSBSKBGetPeers(backupSlice);
    SOSPeerInfoRef myPeer = SOSAccountGetMyPeerInfo(account);
    
    SOSPeerInfoRef myPeerInBSKB = (SOSPeerInfoRef) CFSetGetValue(peers, myPeer);
    require_quiet(isSOSPeerInfo(myPeerInBSKB), errOut);
    
    CFDataRef myBK = SOSPeerInfoCopyBackupKey(myPeer);
    CFDataRef myPeerInBSKBBK = SOSPeerInfoCopyBackupKey(myPeerInBSKB);
    result = CFEqualSafe(myBK, myPeerInBSKBBK);
    CFReleaseNull(myBK);
    CFReleaseNull(myPeerInBSKBBK);
    
errOut:
    CFReleaseNull(ring);

    if (bsError) {
        secnotice("backup", "Failed to find BKSB: %@, %@ (%@)", backupSliceData, backupSlice, bsError);
    }
    CFReleaseNull(bsError);
    return result;
}

bool SOSAccountIsPeerInBackupAndCurrentInView(SOSAccountRef account, SOSPeerInfoRef testPeer, CFStringRef viewname){
    bool result = false;
    CFErrorRef bsError = NULL;
    CFDataRef backupSliceData = NULL;
    SOSRingRef ring = NULL;
    SOSBackupSliceKeyBagRef backupSlice = NULL;

    require_quiet(testPeer, errOut);

    CFStringRef ringName = SOSBackupCopyRingNameForView(viewname);
    ring = SOSAccountCopyRing(account, ringName, &bsError);
    CFReleaseNull(ringName);
    
    require_quiet(ring, errOut);
    
    //grab the backup slice from the ring
    backupSliceData = SOSRingGetPayload(ring, &bsError);
    require_quiet(backupSliceData, errOut);

    backupSlice = SOSBackupSliceKeyBagCreateFromData(kCFAllocatorDefault, backupSliceData, &bsError);
    require_quiet(backupSlice, errOut);
    
    CFSetRef peers = SOSBSKBGetPeers(backupSlice);
    
    SOSPeerInfoRef peerInBSKB = (SOSPeerInfoRef) CFSetGetValue(peers, testPeer);
    require_quiet(isSOSPeerInfo(peerInBSKB), errOut);

    result = CFEqualSafe(testPeer, peerInBSKB);
    
errOut:
    CFReleaseNull(ring);

    if (bsError) {
        secnotice("backup", "Failed to find BKSB: %@, %@ (%@)", backupSliceData, backupSlice, bsError);
    }
    CFReleaseNull(bsError);
    return result;

}

bool SOSAccountUpdateOurPeerInBackup(SOSAccountRef account, SOSRingRef oldRing, CFErrorRef *error){
    bool result = false;
    CFSetRef viewNames = SOSBackupRingGetViews(oldRing, error);
    __block CFStringRef viewName = NULL;
    require_quiet(viewNames, fail);
    require_quiet(SecRequirementError(1 == CFSetGetCount(viewNames), error, CFSTR("Only support single view backup rings")), fail);

    CFSetForEach(viewNames, ^(const void *value) {
        if (isString(value)) {
            viewName = CFRetainSafe((CFStringRef) value);
        }
    });

    result = SOSAccountNewBKSBForView(account, viewName, error);

fail:
    CFReleaseNull(viewName);
    return result;
}

void SOSAccountForEachRingName(SOSAccountRef account, void (^operation)(CFStringRef value)) {
    SOSPeerInfoRef myPeer = SOSAccountGetMyPeerInfo(account);
    if (myPeer) {
        CFSetRef allViews = SOSViewCopyViewSet(kViewSetAll); // All non virtual views.

        CFSetForEach(allViews, ^(const void *value) {
            CFStringRef viewName = asString(value, NULL);

            if (viewName) {
                CFStringRef ringName = SOSBackupCopyRingNameForView(viewName);
                operation(ringName);
                CFReleaseNull(ringName);
            }
        });
        CFReleaseNull(allViews);
        // Only one "ring" now (other than backup rings) when there's more this will need to be modified.
        operation(kSOSRecoveryRing);
    }
}

void SOSAccountForEachBackupView(SOSAccountRef account,  void (^operation)(const void *value)) {
    SOSPeerInfoRef myPeer = SOSAccountGetMyPeerInfo(account);
    
    if (myPeer) {
        CFMutableSetRef myBackupViews = CFSetCreateMutableCopy(kCFAllocatorDefault, 0, SOSPeerInfoGetPermittedViews(myPeer));
        CFSetRemoveValue(myBackupViews, kSOSViewKeychainV0);
        CFSetForEach(myBackupViews, operation);
        CFReleaseNull(myBackupViews);
    }
}


bool SOSAccountSetBackupPublicKey(SOSAccountTransactionRef aTxn, CFDataRef backupKey, CFErrorRef *error)
{
    SOSAccountRef account = aTxn->account;

    __block bool result = false;

    CFDataPerformWithHexString(backupKey, ^(CFStringRef backupKeyString) {
        CFDataPerformWithHexString(account->backup_key, ^(CFStringRef oldBackupKey) {
            secnotice("backup", "SetBackupPublic: %@ from %@", backupKeyString, oldBackupKey);
        });
    });

    require_quiet(SOSAccountIsInCircle(account, error), exit);

    if (CFEqualSafe(backupKey, account->backup_key))
        return true;
    
    CFRetainAssign(account->backup_key, backupKey);

    account->circle_rings_retirements_need_attention = true;

    result = true;
    
exit:
    if (!result) {
        secnotice("backupkey", "SetBackupPublic Failed: %@", error ? (CFTypeRef) *error : (CFTypeRef) CFSTR("No error space"));
    }
    return result;
}

static bool SOSAccountWithBSKBAndPeerInfosForView(SOSAccountRef account, CFArrayRef retiree, CFStringRef viewName, CFErrorRef *error,
                                                  bool (^action)(SOSBackupSliceKeyBagRef bskb, CFErrorRef *error)) {
    __block SOSBackupSliceKeyBagRef bskb = NULL;
    bool result = false;
    
    SOSAccountWithBackupPeersForView(account, viewName, ^(CFSetRef peers) {
        CFMutableSetRef newPeerList = CFSetCreateMutableCopy(kCFAllocatorDefault, CFSetGetCount(peers), peers);
        CFArrayForEach(retiree, ^(const void *value) {
            if (!isSOSPeerInfo(value)) {
                secerror("Peer list contains a non-peerInfo element");
            } else {
                SOSPeerInfoRef retiringPeer = (SOSPeerInfoRef)value;
                CFStringRef retiringPeerID = SOSPeerInfoGetPeerID(retiringPeer);

                CFSetForEach(newPeerList, ^(const void *peerFromAccount) {
                    CFStringRef peerFromAccountID = SOSPeerInfoGetPeerID((SOSPeerInfoRef)peerFromAccount);
                    if (peerFromAccountID && retiringPeerID && CFStringCompare(peerFromAccountID, retiringPeerID, 0) == 0){
                        CFSetRemoveValue(newPeerList, peerFromAccount);
                    }
                });
            }
        });
        bskb = SOSBackupSliceKeyBagCreate(kCFAllocatorDefault, newPeerList, error);
        CFReleaseNull(newPeerList);
    });
    
    require_quiet(bskb, exit);
    
    action(bskb, error);
    
    result = true;
    
exit:
    CFReleaseNull(bskb);
    return result;
}

bool SOSAccountRemoveBackupPublickey(SOSAccountTransactionRef aTxn, CFErrorRef *error)
{
    SOSAccountRef account = aTxn->account;

    __block bool result = false;
    __block CFArrayRef removals = NULL;

    CFReleaseNull(account->backup_key);

    require_quiet(SOSAccountUpdatePeerInfo(account, CFSTR("Backup public key"), error,
                                           ^bool(SOSFullPeerInfoRef fpi, CFErrorRef *error) {
                                               return SOSFullPeerInfoUpdateBackupKey(fpi, NULL, error);
                                           }), exit);

    removals = CFArrayCreateForCFTypes(kCFAllocatorDefault,
                                       SOSAccountGetMyPeerInfo(account), NULL);

    SOSAccountForEachBackupView(account, ^(const void *value) {
        CFStringRef viewName = (CFStringRef)value;
        result = SOSAccountWithBSKBAndPeerInfosForView(account, removals, viewName, error, ^(SOSBackupSliceKeyBagRef bskb, CFErrorRef *error) {
            bool result = SOSAccountSetKeybagForViewBackupRing(account, viewName, bskb, error);
            return result;
        });
    });
   

    result = true;
    
exit:
    return result;
    
}

bool SOSAccountSetBSKBagForAllSlices(SOSAccountRef account, CFDataRef aks_bag, bool setupV0Only, CFErrorRef *error){
    __block bool result = false;
    SOSBackupSliceKeyBagRef backup_slice = NULL;
    
    require_quiet(SOSAccountIsInCircle(account, error), exit);

    if (setupV0Only) {
        result = SOSSaveV0Keybag(aks_bag, error);
        require_action_quiet(result, exit, secnotice("keybag", "failed to set V0 keybag (%@)", *error));
    } else {
        result = true;

        backup_slice = SOSBackupSliceKeyBagCreateDirect(kCFAllocatorDefault, aks_bag, error);

        SOSAccountForEachBackupView(account, ^(const void *value) {
            CFStringRef viewname = (CFStringRef) value;
            result &= SOSAccountSetKeybagForViewBackupRing(account, viewname, backup_slice, error);
        });
    }

exit:
    CFReleaseNull(backup_slice);
    return result;
}

static CFMutableArrayRef SOSAccountIsRetiredPeerIDInBackupPeerList(SOSAccountRef account, CFArrayRef peers, CFSetRef peersInBackup){
    CFMutableArrayRef removals = CFArrayCreateMutableForCFTypes(kCFAllocatorDefault);
    
    CFSetForEach(peersInBackup, ^(const void *value) {
        SOSPeerInfoRef peer = (SOSPeerInfoRef)value;
        CFArrayForEach(peers, ^(const void *value) {
            CFStringRef peerID = SOSPeerInfoGetPeerID((SOSPeerInfoRef)value);
            CFStringRef piPeerID = SOSPeerInfoGetPeerID(peer);
            if (peerID && piPeerID && CFStringCompare(piPeerID, peerID, 0) == 0){
                CFArrayAppendValue(removals, peer);
            }
        });
        
    });
    
    return removals;

}

bool SOSAccountRemoveBackupPeers(SOSAccountRef account, CFArrayRef peers, CFErrorRef *error){
    __block bool result = true;

    SOSFullPeerInfoRef fpi = SOSAccountGetMyFullPeerInfo(account);
    SOSPeerInfoRef myPeer = SOSFullPeerInfoGetPeerInfo(fpi);
    
    CFSetRef permittedViews = SOSPeerInfoGetPermittedViews(myPeer);
    CFSetForEach(permittedViews, ^(const void *value) {
        CFStringRef viewName = (CFStringRef)value;
        if(SOSPeerInfoIsViewBackupEnabled(myPeer, viewName)){
            //grab current peers list
            CFSetRef peersInBackup = SOSAccountCopyBackupPeersForView(account, viewName);
            //get peer infos that have retired but are still in the backup peer list
            CFMutableArrayRef removals = SOSAccountIsRetiredPeerIDInBackupPeerList(account, peers, peersInBackup);
            result = SOSAccountWithBSKBAndPeerInfosForView(account, removals, viewName, error, ^(SOSBackupSliceKeyBagRef bskb, CFErrorRef *error) {
                bool result = SOSAccountSetKeybagForViewBackupRing(account, viewName, bskb, error);
                return result;
            });
        }
    });
    
    return result;
    
}

SOSBackupSliceKeyBagRef SOSAccountBackupSliceKeyBagForView(SOSAccountRef account, CFStringRef viewName, CFErrorRef* error){
    CFDataRef backupSliceData = NULL;
    CFStringRef ringName = NULL;
    SOSRingRef ring = NULL;
    SOSBackupSliceKeyBagRef bskb = NULL;

    ringName = SOSBackupCopyRingNameForView(viewName);

    ring = SOSAccountCopyRing(account, ringName, NULL);
    require_action_quiet(ring, exit, SOSCreateErrorWithFormat(kSOSErrorNoCircle, NULL, error, NULL, CFSTR("failed to get ring")));

    //grab the backup slice from the ring
    backupSliceData = SOSRingGetPayload(ring, error);
    require_action_quiet(backupSliceData, exit, secnotice("backup", "failed to get backup slice (%@)", *error));

    bskb = SOSBackupSliceKeyBagCreateFromData(kCFAllocatorDefault, backupSliceData, error);

exit:
    CFReleaseNull(ring);
    CFReleaseNull(ringName);

    return bskb;
}

bool SOSAccountIsLastBackupPeer(SOSAccountRef account, CFErrorRef *error) {
    __block bool retval = false;
    SOSPeerInfoRef pi = SOSAccountGetMyPeerInfo(account);
    require_quiet(SOSPeerInfoHasBackupKey(pi), errOut);
    SOSCircleRef circle = SOSAccountGetCircle(account, error);
    require_quiet(SOSAccountIsInCircle(account, error), errOut);
    require_action_quiet(SOSCircleCountValidSyncingPeers(circle, SOSAccountGetTrustedPublicCredential(account, error)) != 1, errOut, retval = true);
    // We're in a circle with more than 1 ActiveValidPeers - are they in the backups?
    SOSAccountForEachBackupView(account, ^(const void *value) {
        CFStringRef viewname = (CFStringRef) value;
        SOSBackupSliceKeyBagRef keybag = SOSAccountBackupSliceKeyBagForView(account, viewname, error);
        require_quiet(keybag, inner_errOut);
        retval |= ((SOSBSKBCountPeers(keybag) == 1) && (SOSBSKBPeerIsInKeyBag(keybag, pi)));
    inner_errOut:
        CFReleaseNull(keybag);
    });

errOut:
    return retval;
}

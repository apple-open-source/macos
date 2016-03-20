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

    SOSAccountWithBackupPeersForView(account, viewName, ^(CFSetRef peers) {
        bskb = SOSBackupSliceKeyBagCreate(kCFAllocatorDefault, peers, error);
    });

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

static bool SOSAccountUpdateNamedRing(SOSAccountRef account, CFStringRef ringName, CFErrorRef *error,
                                      SOSRingRef (^create)(CFStringRef ringName, CFErrorRef *error),
                                      SOSRingRef (^copyModified)(SOSRingRef existing, CFErrorRef *error)) {
    bool result = false;
    SOSRingRef newRing = NULL;
    SOSRingRef found = (SOSRingRef) CFDictionaryGetValue(account->trusted_rings, ringName);
    if (isSOSRing(found)) {
        found = SOSRingCopyRing(found, error);
    } else {
        if (found) {
            secerror("Non ring in ring table: %@, purging!", found);
            CFDictionaryRemoveValue(account->trusted_rings, ringName);
        }
        found = create(ringName, error);
    }
    
    require_quiet(found, exit);
    newRing = copyModified(found, error);
    CFReleaseNull(found);

    require_quiet(newRing, exit);

    result = SOSAccountHandleUpdateRing(account, newRing, true, error);

exit:
    CFReleaseNull(found);
    CFReleaseNull(newRing);
    return result;
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

bool SOSAccountStartNewBackup(SOSAccountRef account, CFStringRef viewName, CFErrorRef *error)
{
    return SOSAccountWithBSKBForView(account, viewName, error, ^(SOSBackupSliceKeyBagRef bskb, CFErrorRef *error) {
        bool result = SOSAccountSetKeybagForViewBackupRing(account, viewName, bskb, error);
        return result;
    });
}

bool SOSAccountIsBackupRingEmpty(SOSAccountRef account, CFStringRef viewName) {
    CFStringRef backupRing = SOSBackupCopyRingNameForView(viewName);
    SOSRingRef ring = SOSAccountGetRing(account, backupRing, NULL);
    CFReleaseNull(backupRing);
    int peercnt = 0;
    if(ring) peercnt = SOSRingCountPeers(ring);
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
    SOSBackupSliceKeyBagRef backupSlice = NULL;
    
    require_quiet(SOSPeerInfoIsViewBackupEnabled(SOSAccountGetMyPeerInfo(account), viewname), exit);
    
    CFMutableDictionaryRef trusted_rings = SOSAccountGetRings(account, &bsError);
    require_quiet(trusted_rings, exit);

    CFStringRef ringName = SOSBackupCopyRingNameForView(viewname);
    SOSRingRef ring = (SOSRingRef)CFDictionaryGetValue(trusted_rings, ringName);
    CFReleaseNull(ringName);
    
    require_quiet(ring, exit);
    
    //grab the backup slice from the ring
    backupSliceData = SOSRingGetPayload(ring, &bsError);
    require_quiet(backupSliceData, exit);

    backupSlice = SOSBackupSliceKeyBagCreateFromData(kCFAllocatorDefault, backupSliceData, &bsError);
    require_quiet(backupSlice, exit);
    
    CFSetRef peers = SOSBSKBGetPeers(backupSlice);
    SOSPeerInfoRef myPeer = SOSAccountGetMyPeerInfo(account);
    
    SOSPeerInfoRef myPeerInBSKB = (SOSPeerInfoRef) CFSetGetValue(peers, myPeer);
    require_quiet(isSOSPeerInfo(myPeerInBSKB), exit);
    
    CFDataRef myBK = SOSPeerInfoCopyBackupKey(myPeer);
    CFDataRef myPeerInBSKBBK = SOSPeerInfoCopyBackupKey(myPeerInBSKB);
    result = CFEqualSafe(myBK, myPeerInBSKBBK);
    CFReleaseNull(myBK);
    CFReleaseNull(myPeerInBSKBBK);
    
exit:
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
    SOSBackupSliceKeyBagRef backupSlice = NULL;

    require_quiet(testPeer, exit);

    CFMutableDictionaryRef trusted_rings = SOSAccountGetRings(account, &bsError);
    require_quiet(trusted_rings, exit);

    CFStringRef ringName = SOSBackupCopyRingNameForView(viewname);
    SOSRingRef ring = (SOSRingRef)CFDictionaryGetValue(trusted_rings, ringName);
    CFReleaseNull(ringName);
    
    require_quiet(ring, exit);
    
    //grab the backup slice from the ring
    backupSliceData = SOSRingGetPayload(ring, &bsError);
    require_quiet(backupSliceData, exit);

    backupSlice = SOSBackupSliceKeyBagCreateFromData(kCFAllocatorDefault, backupSliceData, &bsError);
    require_quiet(backupSlice, exit);
    
    CFSetRef peers = SOSBSKBGetPeers(backupSlice);
    
    SOSPeerInfoRef peerInBSKB = (SOSPeerInfoRef) CFSetGetValue(peers, testPeer);
    require_quiet(isSOSPeerInfo(peerInBSKB), exit);

    result = CFEqualSafe(testPeer, peerInBSKB);
    
exit:
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

    result = SOSAccountStartNewBackup(account, viewName, error);

fail:
    CFReleaseNull(viewName);
    return result;
}

void SOSAccountForEachBackupRingName(SOSAccountRef account, void (^operation)(CFStringRef value)) {
    SOSPeerInfoRef myPeer = SOSAccountGetMyPeerInfo(account);
    if (myPeer) {
        CFMutableSetRef myBackupViews = CFSetCreateMutableCopy(kCFAllocatorDefault, 0, SOSPeerInfoGetPermittedViews(myPeer));

        CFSetRemoveValue(myBackupViews, kSOSViewKeychainV0);

        CFSetForEach(myBackupViews, ^(const void *value) {
            CFStringRef viewName = asString(value, NULL);

            if (viewName) {
                CFStringRef ringName = SOSBackupCopyRingNameForView(viewName);
                operation(ringName);
                CFReleaseNull(ringName);
            }
        });

        CFReleaseNull(myBackupViews);
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

bool SOSAccountSetBackupPublicKey(SOSAccountRef account, CFDataRef backupKey, CFErrorRef *error)
{
    __block bool result = false;

    secnotice("backup", "setting backup public key");
    require_quiet(SOSAccountIsInCircle(account, error), exit);

    if (CFEqualSafe(backupKey, account->backup_key))
        return true;
    
    CFRetainAssign(account->backup_key, backupKey);

    SOSAccountEnsureBackupStarts(account);

    result = true;
    
exit:
    if (!result) {
        secnotice("backupkey", "Failed to setup backup public key: %@", error ? (CFTypeRef) *error : (CFTypeRef) CFSTR("No error space provided"));
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

bool SOSAccountRemoveBackupPublickey(SOSAccountRef account, CFErrorRef *error)
{
    __block bool result = false;
    __block CFMutableArrayRef removals = NULL;
    
    require_quiet(SOSAccountUpdatePeerInfo(account, CFSTR("Backup public key"), error,
                                           ^bool(SOSFullPeerInfoRef fpi, CFErrorRef *error) {
                                               return SOSFullPeerInfoUpdateBackupKey(fpi, NULL, error);
                                           }), exit);
    
    CFReleaseNull(account->backup_key);
    
    removals = CFArrayCreateMutableForCFTypes(kCFAllocatorDefault);
    CFArrayAppendValue(removals, SOSAccountGetMyPeerInfo(account));
    
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
    CFMutableDictionaryRef trusted_rings = NULL;
    CFDataRef backupSliceData = NULL;
    CFStringRef ringName = NULL;
    SOSRingRef ring = NULL;
    SOSBackupSliceKeyBagRef bskb = NULL;

    trusted_rings = SOSAccountGetRings(account, error);
    require_action_quiet(trusted_rings, exit, secnotice("keybag", "failed to get trusted rings (%@)", *error));

    ringName = SOSBackupCopyRingNameForView(viewName);

    ring = (SOSRingRef)CFDictionaryGetValue(trusted_rings, ringName);
    require_action_quiet(ring, exit, SOSCreateErrorWithFormat(kSOSErrorNoCircle, NULL, error, NULL, CFSTR("failed to get ring")));

    //grab the backup slice from the ring
    backupSliceData = SOSRingGetPayload(ring, error);
    require_action_quiet(backupSliceData, exit, secnotice("backup", "failed to get backup slice (%@)", *error));

    bskb = SOSBackupSliceKeyBagCreateFromData(kCFAllocatorDefault, backupSliceData, error);

exit:
    CFReleaseNull(ringName);

    return bskb;
}


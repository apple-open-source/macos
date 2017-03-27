/*
 * Copyright (c) 2012-2014 Apple Inc. All Rights Reserved.
 */

/*
 * SOSAccount.c -  Implementation of the secure object syncing account.
 * An account contains a SOSCircle for each protection domain synced.
 */

#include "SOSAccountPriv.h"
#include <Security/SecureObjectSync/SOSPeerInfo.h>
#include <Security/SecureObjectSync/SOSPeerInfoV2.h>
#include <Security/SecureObjectSync/SOSPeerInfoCollections.h>
#include <Security/SecureObjectSync/SOSTransportCircle.h>
#include <Security/SecureObjectSync/SOSTransportMessage.h>
#include <Security/SecureObjectSync/SOSTransportMessageIDS.h>
#include <Security/SecureObjectSync/SOSKVSKeys.h>
#include <Security/SecureObjectSync/SOSTransport.h>
#include <Security/SecureObjectSync/SOSTransportKeyParameter.h>
#include <Security/SecureObjectSync/SOSTransportKeyParameterKVS.h>
#include <Security/SecureObjectSync/SOSPeerCoder.h>
#include <Security/SecureObjectSync/SOSInternal.h>
#include <Security/SecureObjectSync/SOSRing.h>
#include <Security/SecureObjectSync/SOSRingUtils.h>
#include <Security/SecureObjectSync/SOSRingRecovery.h>
#include <Security/SecureObjectSync/SOSPeerInfoSecurityProperties.h>
#include <Security/SecureObjectSync/SOSAccountTransaction.h>
#include <Security/SecureObjectSync/SOSAccountGhost.h>

#include <Security/SecItemInternal.h>
#include <SOSCircle/CKBridge/SOSCloudKeychainClient.h>

#include <utilities/SecCFWrappers.h>
#include <utilities/SecCFError.h>

CFGiblisWithCompareFor(SOSAccount);

const CFStringRef SOSTransportMessageTypeIDS = CFSTR("IDS");
const CFStringRef SOSTransportMessageTypeIDSV2 = CFSTR("IDS2.0");
const CFStringRef SOSTransportMessageTypeKVS = CFSTR("KVS");
const CFStringRef kSOSDSIDKey = CFSTR("AccountDSID");
const CFStringRef kSOSEscrowRecord = CFSTR("EscrowRecord");
const CFStringRef kSOSUnsyncedViewsKey = CFSTR("unsynced");
const CFStringRef kSOSPendingEnableViewsToBeSetKey = CFSTR("pendingEnableViews");
const CFStringRef kSOSPendingDisableViewsToBeSetKey = CFSTR("pendingDisableViews");
const CFStringRef kSOSTestV2Settings = CFSTR("v2dictionary");
const CFStringRef kSOSRecoveryKey = CFSTR("RecoveryKey");
const CFStringRef kSOSRecoveryRing = CFSTR("RecoveryRing");
const CFStringRef kSOSAccountUUID = CFSTR("UUID");

#define DATE_LENGTH 25
const CFStringRef kSOSAccountDebugScope = CFSTR("Scope");

bool SOSAccountEnsureFactoryCircles(SOSAccountRef a)
{
    bool result = false;
    CFStringRef circle_name = NULL;

    require_quiet(a, xit);
    require_quiet(a->factory, xit);

    circle_name = SOSDataSourceFactoryCopyName(a->factory);
    require(circle_name, xit);

    SOSAccountEnsureCircle(a, circle_name, NULL);

    result = true;

xit:
    // We don't own name, so don't release it.
    CFReleaseNull(circle_name);
    return result;
}


SOSAccountRef SOSAccountCreateBasic(CFAllocatorRef allocator,
                                    CFDictionaryRef gestalt,
                                    SOSDataSourceFactoryRef factory) {
    SOSAccountRef a = CFTypeAllocate(SOSAccount, struct __OpaqueSOSAccount, allocator);

    a->queue = dispatch_queue_create("Account Queue", DISPATCH_QUEUE_SERIAL);

    a->gestalt = CFRetainSafe(gestalt);

    a->trusted_circle = NULL;
    a->backups = CFDictionaryCreateMutableForCFTypes(allocator);
    a->my_identity = NULL;
    a->retirees = CFSetCreateMutableForSOSPeerInfosByID(allocator);

    a->factory = factory; // We adopt the factory. kthanksbai.

    a->isListeningForSync = false;
    
    a->_user_private = NULL;
    a->_password_tmp = NULL;
    a->user_private_timer = NULL;
    a->lock_notification_token = NOTIFY_TOKEN_INVALID;

    a->change_blocks = CFArrayCreateMutableForCFTypes(allocator);
    a->waitForInitialSync_blocks = NULL;
    a->departure_code = kSOSNeverAppliedToCircle;

    a->key_transport = (SOSTransportKeyParameterRef)SOSTransportKeyParameterKVSCreate(a, NULL);
    a->circle_transport = NULL;
    a->kvs_message_transport = NULL;
    a->ids_message_transport = NULL;
    a->expansion = CFDictionaryCreateMutableForCFTypes(allocator);

    SOSAccountAddRingDictionary(a);

    a->saveBlock = NULL;
    a->circle_rings_retirements_need_attention = false;
    a->engine_peer_state_needs_repair = false;
    a->key_interests_need_updating = false;
    a->deviceID = NULL;
    
    return a;
}

//
// MARK: Transactional
//

void SOSAccountWithTransaction_Locked(SOSAccountRef account, void (^action)(SOSAccountRef account, SOSAccountTransactionRef txn)) {
    SOSAccountTransactionRef at = SOSAccountTransactionCreate(account);
    action(account, at);
    SOSAccountTransactionFinish(at);
    CFReleaseNull(at);
}



void SOSAccountWithTransaction(SOSAccountRef account, bool sync, void (^action)(SOSAccountRef account, SOSAccountTransactionRef txn)) {
    dispatch_block_t with_transaction =  ^{
        SOSAccountWithTransaction_Locked(account, action);
    };

    if (sync) {
        dispatch_sync(SOSAccountGetQueue(account), with_transaction);
    } else {
        dispatch_async(SOSAccountGetQueue(account), with_transaction);
    }
}

void SOSAccountWithTransactionSync(SOSAccountRef account, void (^action)(SOSAccountRef account, SOSAccountTransactionRef txn)) {
    SOSAccountWithTransaction(account, true, action);
}

void SOSAccountWithTransactionAsync(SOSAccountRef account, bool sync, void (^action)(SOSAccountRef account, SOSAccountTransactionRef txn)) {
    SOSAccountWithTransaction(account, false, action);
}

//
// MARK: Save Block
//

void SOSAccountSetSaveBlock(SOSAccountRef account, SOSAccountSaveBlock saveBlock) {
    CFAssignRetained(account->saveBlock, Block_copy(saveBlock));
}

void SOSAccountFlattenToSaveBlock(SOSAccountRef account) {
    if (account->saveBlock) {
        CFErrorRef localError = NULL;
        CFDataRef saveData = SOSAccountCopyEncodedData(account, kCFAllocatorDefault, &localError);
        
        (account->saveBlock)(saveData, localError);
        
        CFReleaseNull(saveData);
        CFReleaseNull(localError);
    }
}

//
// MARK: Security Properties
//

SOSSecurityPropertyResultCode SOSAccountUpdateSecurityProperty(SOSAccountRef account, CFStringRef property, SOSSecurityPropertyActionCode actionCode, CFErrorRef *error) {
    SOSSecurityPropertyResultCode retval = kSOSCCGeneralSecurityPropertyError;
    bool updateCircle = false;
    require_action_quiet(account->trusted_circle, errOut, SOSCreateError(kSOSErrorNoCircle, CFSTR("No Trusted Circle"), NULL, error));
    require_action_quiet(account->my_identity, errOut, SOSCreateError(kSOSErrorPeerNotFound, CFSTR("No Peer for Account"), NULL, error));
    retval = SOSFullPeerInfoUpdateSecurityProperty(account->my_identity, actionCode, property, error);
    
    if(actionCode == kSOSCCSecurityPropertyEnable && retval == kSOSCCSecurityPropertyValid) {
        updateCircle = true;
    } else if(actionCode == kSOSCCSecurityPropertyDisable && retval == kSOSCCSecurityPropertyNotValid) {
        updateCircle = true;
    } else if(actionCode == kSOSCCSecurityPropertyPending) {
        updateCircle = true;
    }
    
    if (updateCircle) {
        SOSAccountModifyCircle(account, NULL, ^(SOSCircleRef circle_to_change) {
            secnotice("circleChange", "Calling SOSCircleUpdatePeerInfo for security property change");
            return SOSCircleUpdatePeerInfo(circle_to_change, SOSFullPeerInfoGetPeerInfo(account->my_identity));
        });
    }
    
errOut:
    return retval;
}

SOSSecurityPropertyResultCode SOSAccountSecurityPropertyStatus(SOSAccountRef account, CFStringRef property, CFErrorRef *error) {
    SOSSecurityPropertyResultCode retval = kSOSCCGeneralViewError;
    require_action_quiet(account->trusted_circle, errOut, SOSCreateError(kSOSErrorNoCircle, CFSTR("No Trusted Circle"), NULL, error));
    require_action_quiet(account->my_identity, errOut, SOSCreateError(kSOSErrorPeerNotFound, CFSTR("No Peer for Account"), NULL, error));
    retval = SOSFullPeerInfoSecurityPropertyStatus(account->my_identity, property, error);
errOut:
    return retval;
}

bool SOSAccountUpdateGestalt(SOSAccountRef account, CFDictionaryRef new_gestalt)
{
    if (CFEqualSafe(new_gestalt, account->gestalt))
        return false;

    if (account->trusted_circle && account->my_identity
        && SOSFullPeerInfoUpdateGestalt(account->my_identity, new_gestalt, NULL)) {
        SOSAccountModifyCircle(account, NULL, ^(SOSCircleRef circle_to_change) {
            secnotice("circleChange", "Calling SOSCircleUpdatePeerInfo for gestalt change");
            return SOSCircleUpdatePeerInfo(circle_to_change, SOSAccountGetMyPeerInfo(account));
        });
    }

    CFRetainAssign(account->gestalt, new_gestalt);
    return true;
}

CFDictionaryRef SOSAccountCopyGestalt(SOSAccountRef account) {
    return CFDictionaryCreateCopy(kCFAllocatorDefault, account->gestalt);
}

bool SOSAccountUpdateV2Dictionary(SOSAccountRef account, CFDictionaryRef newV2Dict) {
    if(!newV2Dict) return true;
    SOSAccountSetValue(account, kSOSTestV2Settings, newV2Dict, NULL);
    if (account->trusted_circle && account->my_identity
        && SOSFullPeerInfoUpdateV2Dictionary(account->my_identity, newV2Dict, NULL)) {
        SOSAccountModifyCircle(account, NULL, ^(SOSCircleRef circle_to_change) {
            secnotice("circleChange", "Calling SOSCircleUpdatePeerInfo for gestalt change");
            return SOSCircleUpdatePeerInfo(circle_to_change, SOSAccountGetMyPeerInfo(account));
        });
    }
    return true;
}

CFDictionaryRef SOSAccountCopyV2Dictionary(SOSAccountRef account) {
    CFDictionaryRef v2dict = SOSAccountGetValue(account, kSOSTestV2Settings, NULL);
    return CFDictionaryCreateCopy(kCFAllocatorDefault, v2dict);
}

static bool SOSAccountUpdateDSID(SOSAccountRef account, CFStringRef dsid){
    SOSAccountSetValue(account, kSOSDSIDKey, dsid, NULL);
    //send new DSID over account changed
    SOSTransportCircleSendOfficialDSID(account->circle_transport, dsid, NULL);
    return true;
}

void SOSAccountAssertDSID(SOSAccountRef account, CFStringRef dsid) {
    CFStringRef accountDSID = SOSAccountGetValue(account, kSOSDSIDKey, NULL);
    if(accountDSID == NULL) {
        secdebug("updates", "Setting dsid, current dsid is empty for this account: %@", dsid);

        SOSAccountUpdateDSID(account, dsid);
    } else if(CFStringCompare(dsid, accountDSID, 0) != kCFCompareEqualTo) {
        secnotice("updates", "Changing DSID from: %@ to %@", accountDSID, dsid);

        //DSID has changed, blast the account!
        SOSAccountSetToNew(account);

        //update DSID to the new DSID
        SOSAccountUpdateDSID(account, dsid);
    } else {
        secnotice("updates", "Not Changing DSID: %@ to %@", accountDSID, dsid);
    }
}

bool SOSAccountUpdateFullPeerInfo(SOSAccountRef account, CFSetRef minimumViews, CFSetRef excludedViews) {
    if (account->trusted_circle && account->my_identity) {
        if(SOSFullPeerInfoUpdateToCurrent(account->my_identity, minimumViews, excludedViews)) {
            SOSAccountModifyCircle(account, NULL, ^(SOSCircleRef circle_to_change) {
                secnotice("circleChange", "Calling SOSCircleUpdatePeerInfo for gestalt change");
                return SOSCircleUpdatePeerInfo(circle_to_change, SOSFullPeerInfoGetPeerInfo(account->my_identity));
            });
        }
    }
    
    return true;
}

void SOSAccountPendEnableViewSet(SOSAccountRef account, CFSetRef enabledViews)
{
    if(CFSetGetValue(enabledViews, kSOSViewKeychainV0) != NULL) secnotice("viewChange", "Warning, attempting to Add KeychainV0");

    SOSAccountValueUnionWith(account, kSOSPendingEnableViewsToBeSetKey, enabledViews);
    SOSAccountValueSubtractFrom(account, kSOSPendingDisableViewsToBeSetKey, enabledViews);
}


void SOSAccountPendDisableViewSet(SOSAccountRef account, CFSetRef disabledViews)
{
    SOSAccountValueUnionWith(account, kSOSPendingDisableViewsToBeSetKey, disabledViews);
    SOSAccountValueSubtractFrom(account, kSOSPendingEnableViewsToBeSetKey, disabledViews);
}

static SOSViewResultCode SOSAccountVirtualV0Behavior(SOSAccountRef account, SOSViewActionCode actionCode) {
    SOSViewResultCode retval = kSOSCCGeneralViewError;
    // The V0 view switches on and off all on it's own, we allow people the delusion
    // of control and status if it's what we're stuck at., otherwise error.
    if (SOSAccountSyncingV0(account)) {
        require_action_quiet(actionCode == kSOSCCViewDisable, errOut, CFSTR("Can't disable V0 view and it's on right now"));
        retval = kSOSCCViewMember;
    } else {
        require_action_quiet(actionCode == kSOSCCViewEnable, errOut, CFSTR("Can't enable V0 and it's off right now"));
        retval = kSOSCCViewNotMember;
    }
errOut:
    return retval;
}


SOSViewResultCode SOSAccountUpdateView(SOSAccountRef account, CFStringRef viewname, SOSViewActionCode actionCode, CFErrorRef *error) {
    SOSViewResultCode retval = kSOSCCGeneralViewError;
    SOSViewResultCode currentStatus = kSOSCCGeneralViewError;
    bool alreadyInSync = SOSAccountHasCompletedInitialSync(account);

    bool updateCircle = false;
    require_action_quiet(account->trusted_circle, errOut, SOSCreateError(kSOSErrorNoCircle, CFSTR("No Trusted Circle"), NULL, error));
    require_action_quiet(account->my_identity, errOut, SOSCreateError(kSOSErrorPeerNotFound, CFSTR("No Peer for Account"), NULL, error));
    require_action_quiet((actionCode == kSOSCCViewEnable) || (actionCode == kSOSCCViewDisable), errOut, CFSTR("Invalid View Action"));
    currentStatus = SOSAccountViewStatus(account, viewname, error);
    require_action_quiet((currentStatus == kSOSCCViewNotMember) || (currentStatus == kSOSCCViewMember), errOut, CFSTR("View Membership Not Actionable"));

    if (CFEqualSafe(viewname, kSOSViewKeychainV0)) {
        retval = SOSAccountVirtualV0Behavior(account, actionCode);
    } else if (SOSAccountSyncingV0(account) && SOSViewsIsV0Subview(viewname)) {
        // Subviews of V0 syncing can't be turned off if V0 is on.
        require_action_quiet(actionCode = kSOSCCViewDisable, errOut, CFSTR("Have V0 peer can't disable"));
        retval = kSOSCCViewMember;
    } else {
        CFMutableSetRef pendingSet = CFSetCreateMutableForCFTypes(kCFAllocatorDefault);
        CFSetAddValue(pendingSet, viewname);

        if(actionCode == kSOSCCViewEnable && currentStatus == kSOSCCViewNotMember) {
            if(alreadyInSync) {
                retval = SOSFullPeerInfoUpdateViews(account->my_identity, actionCode, viewname, error);
                if(retval == kSOSCCViewMember) updateCircle = true;
            } else {
                SOSAccountPendEnableViewSet(account, pendingSet);
                retval = kSOSCCViewMember;
                updateCircle = false;
            }
        } else if(actionCode == kSOSCCViewDisable && currentStatus == kSOSCCViewMember) {
            if(alreadyInSync) {
                retval = SOSFullPeerInfoUpdateViews(account->my_identity, actionCode, viewname, error);
                if(retval == kSOSCCViewNotMember) updateCircle = true;
            } else {
                SOSAccountPendDisableViewSet(account, pendingSet);
                retval = kSOSCCViewNotMember;
                updateCircle = false;
            }
        } else {
            retval = currentStatus;
        }
        
        CFReleaseNull(pendingSet);

        if (updateCircle) {
            SOSAccountModifyCircle(account, NULL, ^(SOSCircleRef circle_to_change) {
                secnotice("circleChange", "Calling SOSCircleUpdatePeerInfo for views change");
                return SOSCircleUpdatePeerInfo(circle_to_change, SOSFullPeerInfoGetPeerInfo(account->my_identity));
            });
        }
    }
    
errOut:
    return retval;
}

SOSViewResultCode SOSAccountViewStatus(SOSAccountRef account, CFStringRef viewname, CFErrorRef *error) {
    SOSViewResultCode retval = kSOSCCGeneralViewError;
    require_action_quiet(account->trusted_circle, errOut, SOSCreateError(kSOSErrorNoCircle, CFSTR("No Trusted Circle"), NULL, error));
    require_action_quiet(account->my_identity, errOut, SOSCreateError(kSOSErrorPeerNotFound, CFSTR("No Peer for Account"), NULL, error));

    if (SOSAccountValueSetContainsValue(account, kSOSPendingEnableViewsToBeSetKey, viewname)) {
        retval = kSOSCCViewMember;
    } else if (SOSAccountValueSetContainsValue(account, kSOSPendingDisableViewsToBeSetKey, viewname)) {
        retval = kSOSCCViewNotMember;
    } else {
        retval = SOSFullPeerInfoViewStatus(account->my_identity, viewname, error);
    }

    // If that doesn't say we're a member and this view is a V0 subview, and we're syncing V0 views we are a member
    if (retval != kSOSCCViewMember) {
        if ((CFEqualSafe(viewname, kSOSViewKeychainV0) || SOSViewsIsV0Subview(viewname))
          && SOSAccountSyncingV0(account)) {
            retval = kSOSCCViewMember;
        }
    }

    // If we're only an applicant we report pending if we would be a view member
    if (retval == kSOSCCViewMember) {
        bool isApplicant = SOSCircleHasApplicant(account->trusted_circle, SOSAccountGetMyPeerInfo(account), error);
        if (isApplicant) {
            retval = kSOSCCViewPending;
        }
    }

errOut:
    return retval;
}

static void dumpViewSet(CFStringRef label, CFSetRef views) {
    if(views) {
        CFStringSetPerformWithDescription(views, ^(CFStringRef description) {
            secnotice("circleChange", "%@ list: %@", label, description);
        });
    } else {
        secnotice("circleChange", "No %@ list provided.", label);
    }
}

static bool SOSAccountScreenViewListForValidV0(SOSAccountRef account, CFMutableSetRef viewSet, SOSViewActionCode actionCode) {
    bool retval = true;
    if(viewSet && CFSetContainsValue(viewSet, kSOSViewKeychainV0)) {
        retval = SOSAccountVirtualV0Behavior(account, actionCode) != kSOSCCGeneralViewError;
        CFSetRemoveValue(viewSet, kSOSViewKeychainV0);
    }
    return retval;
}

bool SOSAccountUpdateViewSets(SOSAccountRef account, CFSetRef origEnabledViews, CFSetRef origDisabledViews) {
    bool retval = false;
    bool updateCircle = false;
    SOSPeerInfoRef  pi = NULL;
    CFMutableSetRef enabledViews = NULL;
    CFMutableSetRef disabledViews = NULL;
    if(origEnabledViews) enabledViews = CFSetCreateMutableCopy(kCFAllocatorDefault, 0, origEnabledViews);
    if(origDisabledViews) disabledViews = CFSetCreateMutableCopy(kCFAllocatorDefault, 0, origDisabledViews);
    dumpViewSet(CFSTR("Enabled"), enabledViews);
    dumpViewSet(CFSTR("Disabled"), disabledViews);
    
    require_action_quiet(account->trusted_circle, errOut, secnotice("views", "Attempt to set viewsets with no trusted circle"));
    
    // Make sure we have a peerInfo capable of supporting views.
    SOSFullPeerInfoRef fpi = SOSAccountGetMyFullPeerInfo(account);
    require_action_quiet(fpi, errOut, secnotice("views", "Attempt to set viewsets with no fullPeerInfo"));
    require_action_quiet(enabledViews || disabledViews, errOut, secnotice("views", "No work to do"));

    pi = SOSPeerInfoCreateCopy(kCFAllocatorDefault, SOSFullPeerInfoGetPeerInfo(fpi), NULL);
    
    require_action_quiet(pi, errOut, secnotice("views", "Couldn't copy PeerInfoRef"));
    
    if(!SOSPeerInfoVersionIsCurrent(pi)) {
        CFErrorRef updateFailure = NULL;
        require_action_quiet(SOSPeerInfoUpdateToV2(pi, &updateFailure), errOut,
                             (secnotice("views", "Unable to update peer to V2- can't update views: %@", updateFailure), (void) CFReleaseNull(updateFailure)));
        secnotice("V2update", "Updating PeerInfo to V2 within SOSAccountUpdateViewSets");
        updateCircle = true;
    }
    
    CFStringSetPerformWithDescription(enabledViews, ^(CFStringRef description) {
        secnotice("viewChange", "Enabling %@", description);
    });
    
    CFStringSetPerformWithDescription(disabledViews, ^(CFStringRef description) {
        secnotice("viewChange", "Disabling %@", description);
    });
    
    require_action_quiet(SOSAccountScreenViewListForValidV0(account, enabledViews, kSOSCCViewEnable), errOut, secnotice("viewChange", "Bad view change (enable) with kSOSViewKeychainV0"));
    require_action_quiet(SOSAccountScreenViewListForValidV0(account, disabledViews, kSOSCCViewDisable), errOut, secnotice("viewChange", "Bad view change (disable) with kSOSViewKeychainV0"));

    if(SOSAccountHasCompletedInitialSync(account)) {
        if(enabledViews) updateCircle |= SOSViewSetEnable(pi, enabledViews);
        if(disabledViews) updateCircle |= SOSViewSetDisable(pi, disabledViews);
        retval = true;
    } else {
        //hold on to the views and enable them later
        if(enabledViews) SOSAccountPendEnableViewSet(account, enabledViews);
        if(disabledViews) SOSAccountPendDisableViewSet(account, disabledViews);
        retval = true;
    }
    
    if(updateCircle) {
        /* UPDATE FULLPEERINFO VIEWS */
        require_quiet(SOSFullPeerInfoUpdateToThisPeer(fpi, pi, NULL), errOut);
        
        require_quiet(SOSAccountModifyCircle(account, NULL, ^(SOSCircleRef circle_to_change) {
            secnotice("circleChange", "Calling SOSCircleUpdatePeerInfo for views or peerInfo change");
            return SOSCircleUpdatePeerInfo(circle_to_change, SOSFullPeerInfoGetPeerInfo(account->my_identity));
        }), errOut);
        
        // Make sure we update the engine
        account->circle_rings_retirements_need_attention = true;
    }

errOut:
    CFReleaseNull(enabledViews);
    CFReleaseNull(disabledViews);
    CFReleaseNull(pi);
    return retval;
}


SOSAccountRef SOSAccountCreate(CFAllocatorRef allocator,
                               CFDictionaryRef gestalt,
                               SOSDataSourceFactoryRef factory) {
    SOSAccountRef a = SOSAccountCreateBasic(allocator, gestalt, factory);

    SOSAccountEnsureFactoryCircles(a);

    SOSAccountEnsureUUID(a);

    a->key_interests_need_updating = true;
    
    return a;
}

static void SOSAccountDestroy(CFTypeRef aObj) {
    SOSAccountRef a = (SOSAccountRef) aObj;

    // We don't own the factory, merely have a reference to the singleton
    //    Don't free it.
    //   a->factory

    SOSAccountCancelSyncChecking(a);

    SOSEngineRef engine = SOSDataSourceFactoryGetEngineForDataSourceName(a->factory, SOSCircleGetName(a->trusted_circle), NULL);

    if (engine)
        SOSEngineSetSyncCompleteListenerQueue(engine, NULL);

    dispatch_sync(a->queue, ^{
        CFReleaseNull(a->gestalt);

        CFReleaseNull(a->my_identity);
        CFReleaseNull(a->trusted_circle);
        CFReleaseNull(a->backups);
        CFReleaseNull(a->retirees);

        a->user_public_trusted = false;
        CFReleaseNull(a->user_public);
        CFReleaseNull(a->user_key_parameters);

        SOSAccountPurgePrivateCredential(a);
        CFReleaseNull(a->previous_public);
        CFReleaseNull(a->_user_private);
        CFReleaseNull(a->_password_tmp);

        a->departure_code = kSOSNeverAppliedToCircle;
        CFReleaseNull(a->kvs_message_transport);
        CFReleaseNull(a->ids_message_transport);
        CFReleaseNull(a->key_transport);
        CFReleaseNull(a->circle_transport);
        dispatch_release(a->queue);

        dispatch_release(a->user_private_timer);
        CFReleaseNull(a->change_blocks);
        CFReleaseNull(a->waitForInitialSync_blocks);
        CFReleaseNull(a->expansion);

        CFReleaseNull(a->saveBlock);
        CFReleaseNull(a->deviceID);
    });
}

static OSStatus do_delete(CFDictionaryRef query) {
    OSStatus result;
    
    result = SecItemDelete(query);
    if (result) {
        secerror("SecItemDelete: %d", (int)result);
    }
     return result;
}

static int
do_keychain_delete_aks_bags()
{
    OSStatus result;
    CFDictionaryRef item = CFDictionaryCreateForCFTypes(kCFAllocatorDefault,
                                 kSecClass,           kSecClassGenericPassword,
                                 kSecAttrAccessGroup, CFSTR("com.apple.sbd"),
                                 kSecAttrAccount,     CFSTR("SecureBackupPublicKeybag"),
                                 kSecAttrService,     CFSTR("SecureBackupService"),
                                 kSecAttrSynchronizable, kCFBooleanTrue,
                                 kSecUseTombstones,     kCFBooleanFalse,
                                 NULL);

    result = do_delete(item);
    CFReleaseSafe(item);
    
    return result;
}

static int
do_keychain_delete_identities()
{
    OSStatus result;
    CFDictionaryRef item = CFDictionaryCreateForCFTypes(kCFAllocatorDefault,
                                kSecClass, kSecClassKey,
                                kSecAttrSynchronizable, kCFBooleanTrue,
                                kSecUseTombstones, kCFBooleanFalse,
                                kSecAttrAccessGroup, CFSTR("com.apple.security.sos"),
                                NULL);
  
    result = do_delete(item);
    CFReleaseSafe(item);
    
    return result;
}

static int
do_keychain_delete_lakitu()
{
    OSStatus result;
    CFDictionaryRef item = CFDictionaryCreateForCFTypes(kCFAllocatorDefault,
                                                        kSecClass, kSecClassGenericPassword,
                                                        kSecAttrSynchronizable, kCFBooleanTrue,
                                                        kSecUseTombstones, kCFBooleanFalse,
                                                        kSecAttrAccessGroup, CFSTR("com.apple.lakitu"),
                                                        kSecAttrAccount, CFSTR("EscrowServiceBypassToken"),
                                                        kSecAttrService, CFSTR("EscrowService"),
                                                        NULL);
    
    result = do_delete(item);
    CFReleaseSafe(item);
    
    return result;
}

static int
do_keychain_delete_sbd()
{
    OSStatus result;
    CFDictionaryRef item = CFDictionaryCreateForCFTypes(kCFAllocatorDefault,
                                                        kSecClass, kSecClassGenericPassword,
                                                        kSecAttrSynchronizable, kCFBooleanTrue,
                                                        kSecUseTombstones, kCFBooleanFalse,
                                                        kSecAttrAccessGroup, CFSTR("com.apple.sbd"),
                                                        NULL);
    
    result = do_delete(item);
    CFReleaseSafe(item);
    
    return result;
}

void SOSAccountSetToNew(SOSAccountRef a) {
    secnotice("accountChange", "Setting Account to New");
    int result = 0;
    
    CFReleaseNull(a->my_identity);
    CFReleaseNull(a->trusted_circle);
    CFReleaseNull(a->backups);
    CFReleaseNull(a->retirees);

    CFReleaseNull(a->user_key_parameters);
    CFReleaseNull(a->user_public);
    CFReleaseNull(a->previous_public);
    CFReleaseNull(a->_user_private);
    CFReleaseNull(a->_password_tmp);

    CFReleaseNull(a->key_transport);
    CFReleaseNull(a->circle_transport);
    CFReleaseNull(a->kvs_message_transport);
    CFReleaseNull(a->ids_message_transport);
    CFReleaseNull(a->expansion);
    CFReleaseNull(a->deviceID);
    
    /* remove all syncable items */
    result = do_keychain_delete_aks_bags(); (void) result;
    secdebug("set to new", "result for deleting aks bags: %d", result);

    result = do_keychain_delete_identities(); (void) result;
    secdebug("set to new", "result for deleting identities: %d", result);
 
    result = do_keychain_delete_lakitu(); (void) result;
    secdebug("set to new", "result for deleting lakitu: %d", result);
    
    result = do_keychain_delete_sbd(); (void) result;
    secdebug("set to new", "result for deleting sbd: %d", result);

    a->user_public_trusted = false;
    a->departure_code = kSOSNeverAppliedToCircle;

    if (a->user_private_timer) {
        dispatch_source_cancel(a->user_private_timer);
        dispatch_release(a->user_private_timer);
        a->user_private_timer = NULL;
        xpc_transaction_end();

    }
    if (a->lock_notification_token != NOTIFY_TOKEN_INVALID) {
        notify_cancel(a->lock_notification_token);
        a->lock_notification_token = NOTIFY_TOKEN_INVALID;
    }

    // keeping gestalt;
    // keeping factory;
    // Live Notification
    // change_blocks;
    // update_interest_block;
    // update_block;

    a->key_transport = (SOSTransportKeyParameterRef)SOSTransportKeyParameterKVSCreate(a, NULL);
    a->circle_transport = NULL;
    a->kvs_message_transport = NULL;
    a->ids_message_transport = NULL;

    a->backups = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);

    a->retirees = CFSetCreateMutableForSOSPeerInfosByID(kCFAllocatorDefault);
    a->expansion = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
    SOSAccountAddRingDictionary(a);

    SOSAccountEnsureFactoryCircles(a); // Does rings too

    // By resetting our expansion dictionary we've reset our UUID, so we'll be notified properly
    SOSAccountEnsureUUID(a);

    a->key_interests_need_updating = true;
}

bool SOSAccountIsNew(SOSAccountRef account, CFErrorRef *error){
    bool result = false;
    require_quiet(account->user_public_trusted == false, exit);
    require_quiet(account->departure_code == kSOSNeverAppliedToCircle, exit);
    require_quiet(account->user_private_timer == NULL, exit);
    require_quiet(account->lock_notification_token == NOTIFY_TOKEN_INVALID, exit);
    require_quiet (CFDictionaryGetCount(account->backups) == 0, exit);
    require_quiet(CFSetGetCount(account->retirees) == 0, exit);

    result = true;
exit:
    return result;
}

static CFStringRef SOSAccountCopyFormatDescription(CFTypeRef aObj, CFDictionaryRef formatOptions) {
    SOSAccountRef a = (SOSAccountRef) aObj;
    
    CFStringRef gestaltDescription = CFDictionaryCopyCompactDescription(a->gestalt);

    CFStringRef result = CFStringCreateWithFormat(NULL, NULL, CFSTR("<SOSAccount@%p: %c%c%c%c%c G: %@ Me: %@ C: %@ >"), a,
                                                  a->user_public ? 'P' : 'p',
                                                  a->user_public_trusted ? 'T' : 't',
                                                  a->isListeningForSync ? 'L' : 'l',
                                                  SOSAccountHasCompletedInitialSync(a) ? 'C' : 'c',
                                                  SOSAccountHasCompletedRequiredBackupSync(a) ? 'B' : 'b',
                                                  gestaltDescription, a->my_identity, a->trusted_circle);

    CFReleaseNull(gestaltDescription);

    return result;
}

CFStringRef SOSAccountCreateCompactDescription(SOSAccountRef a) {

    CFStringRef gestaltDescription = CFDictionaryCopySuperCompactDescription(a->gestalt);
    
    CFStringRef result = CFStringCreateWithFormat(NULL, NULL, CFSTR("%@"), gestaltDescription);
    
    CFReleaseNull(gestaltDescription);
    
    return result;
}

static Boolean SOSAccountCompare(CFTypeRef lhs, CFTypeRef rhs)
{
    SOSAccountRef laccount = (SOSAccountRef) lhs;
    SOSAccountRef raccount = (SOSAccountRef) rhs;

    return CFEqualSafe(laccount->gestalt, raccount->gestalt)
        && CFEqualSafe(laccount->trusted_circle, raccount->trusted_circle)
        && CFEqualSafe(laccount->expansion, raccount->expansion)
        && CFEqualSafe(laccount->my_identity, raccount->my_identity);
}

dispatch_queue_t SOSAccountGetQueue(SOSAccountRef account) {
    return account->queue;
}

void SOSAccountSetUserPublicTrustedForTesting(SOSAccountRef account){
    account->user_public_trusted = true;
}

SOSFullPeerInfoRef SOSAccountCopyAccountIdentityPeerInfo(SOSAccountRef account, CFAllocatorRef allocator, CFErrorRef* error)
{
    return CFRetainSafe(account->my_identity);
}



bool SOSAccountCleanupAfterPeer(SOSAccountRef account, size_t seconds, SOSCircleRef circle,
                                SOSPeerInfoRef cleanupPeer, CFErrorRef* error)
{
    bool success = true;
    
    SOSPeerInfoRef myPeerInfo = SOSFullPeerInfoGetPeerInfo(account->my_identity);
    require_action_quiet(account->my_identity && myPeerInfo, xit, SOSErrorCreate(kSOSErrorPeerNotFound, error, NULL, CFSTR("I have no peer")));
    require_quiet(SOSCircleHasActivePeer(circle, SOSFullPeerInfoGetPeerInfo(account->my_identity), error), xit);
    
    CFStringRef cleanupPeerID = SOSPeerInfoGetPeerID(cleanupPeer);

    CFStringRef circle_name = SOSCircleGetName(circle);

    CFMutableDictionaryRef circleToPeerIDs = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
    CFArrayAppendValue(CFDictionaryEnsureCFArrayAndGetCurrentValue(circleToPeerIDs, circle_name), cleanupPeerID);

    CFErrorRef localError = NULL;
    if (!(success &= SOSTransportMessageCleanupAfterPeerMessages(account->kvs_message_transport, circleToPeerIDs, &localError))) {
        secnotice("account", "Failed to cleanup after peer %@ messages: %@", cleanupPeerID, localError);
    }

    if (account->ids_message_transport && !SOSTransportMessageCleanupAfterPeerMessages(account->ids_message_transport, circleToPeerIDs, &localError)) {
        secnotice("account", "Failed to cleanup after peer %@ messages: %@", cleanupPeerID, localError);
    }

    CFReleaseNull(localError);

    if((success &= SOSPeerInfoRetireRetirementTicket(seconds, cleanupPeer))) {
        if (!(success &= SOSTransportCircleExpireRetirementRecords(account->circle_transport, circleToPeerIDs, &localError))) {
            secnotice("account", "Failed to cleanup after peer %@ retirement: %@", cleanupPeerID, localError);
        }
    }
    CFReleaseNull(localError);
    CFReleaseNull(circleToPeerIDs);

xit:
    return success;
}

bool SOSAccountCleanupRetirementTickets(SOSAccountRef account, size_t seconds, CFErrorRef* error) {
    CFMutableSetRef retirees_to_remove = CFSetCreateMutableForSOSPeerInfosByID(kCFAllocatorDefault);

    __block bool success = true;

    CFSetForEach(account->retirees, ^(const void *value) {
        SOSPeerInfoRef retiree = (SOSPeerInfoRef) value;

        if (retiree) {
            // Remove the entry if it's not a retired peer or if it's retirment ticket has expired AND he's no longer in the circle.
            if (!SOSPeerInfoIsRetirementTicket(retiree) ||
                (SOSPeerInfoRetireRetirementTicket(seconds, retiree) && !SOSCircleHasActivePeer(account->trusted_circle, retiree, NULL))) {
                CFSetAddValue(retirees_to_remove, retiree);
            };
        }
    });

    CFMutableArrayRef retirees_to_cleanup = CFArrayCreateMutableForCFTypes(kCFAllocatorDefault);
    CFSetForEach(retirees_to_remove, ^(const void *value) {
        CFArrayAppendValue(retirees_to_cleanup, value);
        CFSetRemoveValue(account->retirees, value);
    });

    CFReleaseNull(retirees_to_remove);

    CFDictionaryRef retirements_to_remove = CFDictionaryCreateForCFTypes(kCFAllocatorDefault,
                                                                         SOSCircleGetName(account->trusted_circle), retirees_to_cleanup,
                                                                         NULL);

    CFReleaseNull(retirees_to_cleanup);

    success = SOSTransportCircleExpireRetirementRecords(account->circle_transport, retirements_to_remove, error);

    CFReleaseNull(retirements_to_remove);

    return success;
}

bool SOSAccountScanForRetired(SOSAccountRef account, SOSCircleRef circle, CFErrorRef *error) {
    SOSCircleForEachRetiredPeer(circle, ^(SOSPeerInfoRef peer) {
        CFSetSetValue(account->retirees, peer);
        CFErrorRef cleanupError = NULL;
        if (!SOSAccountCleanupAfterPeer(account, RETIREMENT_FINALIZATION_SECONDS, circle, peer, &cleanupError)) {
            secnotice("retirement", "Error cleaning up after peer, probably orphaned some stuff in KVS: (%@) – moving on", cleanupError);
        }
        CFReleaseSafe(cleanupError);
    });
    return true;
}

SOSCircleRef SOSAccountCloneCircleWithRetirement(SOSAccountRef account, SOSCircleRef starting_circle, CFErrorRef *error) {
    SOSCircleRef new_circle = SOSCircleCopyCircle(NULL, starting_circle, error);
    SOSFullPeerInfoRef meFull = SOSAccountGetMyFullPeerInfo(account);
    SOSPeerInfoRef me = SOSFullPeerInfoGetPeerInfo(meFull);
    bool iAmApplicant = me && SOSCircleHasApplicant(new_circle, me, NULL);
    
    if(!new_circle) return NULL;
    __block bool workDone = false;
    if (account->retirees) {
        CFSetForEach(account->retirees, ^(const void* value) {
            SOSPeerInfoRef pi = (SOSPeerInfoRef) value;
            if (isSOSPeerInfo(pi)) {
                SOSCircleUpdatePeerInfo(new_circle, pi);
                workDone = true;
            }
        });
    }

    if(workDone && SOSCircleCountPeers(new_circle) == 0) {
        SecKeyRef userPrivKey = SOSAccountGetPrivateCredential(account, error);
 
        if(iAmApplicant) {
            if(userPrivKey) {
                secnotice("resetToOffering", "Reset to offering with last retirement and me as applicant");
                if(!SOSCircleResetToOffering(new_circle, userPrivKey, meFull, error) ||
                   !SOSAccountAddiCloudIdentity(account, new_circle, userPrivKey, error)) {
                    CFReleaseNull(new_circle);
                    return NULL;
                }
            } else {
                // Do nothing.  We can't resetToOffering without a userPrivKey.  If we were to resetToEmpty
                // we won't push the result later in handleUpdateCircle.  If we leave the circle as it is
                // we have a chance to set things right with a SetCreds/Join sequence.  This will cause
                // handleUpdateCircle to return false.
                CFReleaseNull(new_circle);
                return NULL;
            }
        } else {
            // This case is when we aren't an applicant and the circle is retirement-empty.
            secnotice("resetToEmpty", "Reset to empty with last retirement");
            SOSCircleResetToEmpty(new_circle, NULL);
        }
    }

    return new_circle;
}

//
// MARK: Circle Membership change notificaion
//

void SOSAccountAddChangeBlock(SOSAccountRef a, SOSAccountCircleMembershipChangeBlock changeBlock) {
    SOSAccountCircleMembershipChangeBlock copy = Block_copy(changeBlock);
    CFArrayAppendValue(a->change_blocks, copy);
    CFReleaseNull(copy);
}

void SOSAccountRemoveChangeBlock(SOSAccountRef a, SOSAccountCircleMembershipChangeBlock changeBlock) {
    CFArrayRemoveAllValue(a->change_blocks, changeBlock);
}

void SOSAccountAddSyncablePeerBlock(SOSAccountRef a, CFStringRef ds_name, SOSAccountSyncablePeersBlock changeBlock) {
    if (!changeBlock) return;

    CFRetainSafe(ds_name);
    SOSAccountCircleMembershipChangeBlock block_to_register = ^void (SOSCircleRef new_circle,
                                                                     CFSetRef added_peers, CFSetRef removed_peers,
                                                                     CFSetRef added_applicants, CFSetRef removed_applicants) {

        if (!CFEqualSafe(SOSCircleGetName(new_circle), ds_name))
            return;

        SOSPeerInfoRef myPi = SOSFullPeerInfoGetPeerInfo(a->my_identity);
        CFStringRef myPi_id = myPi ? SOSPeerInfoGetPeerID(myPi) : NULL;

        CFMutableArrayRef peer_ids = CFArrayCreateMutableForCFTypes(kCFAllocatorDefault);
        CFMutableArrayRef added_ids = CFArrayCreateMutableForCFTypes(kCFAllocatorDefault);
        CFMutableArrayRef removed_ids = CFArrayCreateMutableForCFTypes(kCFAllocatorDefault);

        if (SOSCircleHasPeer(new_circle, myPi, NULL)) {
            SOSCircleForEachPeer(new_circle, ^(SOSPeerInfoRef peer) {
                CFArrayAppendValueIfNot(peer_ids, SOSPeerInfoGetPeerID(peer), myPi_id);
            });

            CFSetForEach(added_peers, ^(const void *value) {
                CFArrayAppendValueIfNot(added_ids, SOSPeerInfoGetPeerID((SOSPeerInfoRef) value), myPi_id);
            });

            CFSetForEach(removed_peers, ^(const void *value) {
                CFArrayAppendValueIfNot(removed_ids, SOSPeerInfoGetPeerID((SOSPeerInfoRef) value), myPi_id);
            });
        }

        if (CFArrayGetCount(peer_ids) || CFSetContainsValue(removed_peers, myPi))
            changeBlock(peer_ids, added_ids, removed_ids);

        CFReleaseSafe(peer_ids);
        CFReleaseSafe(added_ids);
        CFReleaseSafe(removed_ids);
    };

    CFRetainSafe(changeBlock);
    SOSAccountAddChangeBlock(a, block_to_register);

    CFSetRef empty = CFSetCreateMutableForSOSPeerInfosByID(kCFAllocatorDefault);
    if (a->trusted_circle && CFEqualSafe(ds_name, SOSCircleGetName(a->trusted_circle))) {
        block_to_register(a->trusted_circle, empty, empty, empty, empty);
    }
    CFReleaseSafe(empty);
}

void SOSAccountPurgeIdentity(SOSAccountRef account) {
    if (account->my_identity) {
        // Purge private key but don't return error if we can't.
        CFErrorRef purgeError = NULL;
        if (!SOSFullPeerInfoPurgePersistentKey(account->my_identity, &purgeError)) {
            secwarning("Couldn't purge persistent key for %@ [%@]", account->my_identity, purgeError);
        }
        CFReleaseNull(purgeError);

        CFReleaseNull(account->my_identity);
    }
}

bool sosAccountLeaveCircle(SOSAccountRef account, SOSCircleRef circle, CFErrorRef* error) {
    SOSFullPeerInfoRef fpi = account->my_identity;
    if(!fpi) return false;

	CFErrorRef localError = NULL;

    bool retval = false;

    SOSPeerInfoRef retire_peer = SOSFullPeerInfoPromoteToRetiredAndCopy(fpi, &localError);
    if (!retire_peer) {
        secerror("Create ticket failed for peer %@: %@", fpi, localError);
    } else {
        // See if we need to repost the circle we could either be an applicant or a peer already in the circle
        if(SOSCircleHasApplicant(circle, retire_peer, NULL)) {
            // Remove our application if we have one.
            SOSCircleWithdrawRequest(circle, retire_peer, NULL);
        } else if (SOSCircleHasPeer(circle, retire_peer, NULL)) {
            if (SOSCircleUpdatePeerInfo(circle, retire_peer)) {
                CFErrorRef cleanupError = NULL;
                if (!SOSAccountCleanupAfterPeer(account, RETIREMENT_FINALIZATION_SECONDS, circle, retire_peer, &cleanupError)) {
                    secerror("Error cleanup up after peer (%@): %@", retire_peer, cleanupError);
                }
                CFReleaseSafe(cleanupError);
            }
        }

        // Store the retirement record locally.
        CFSetAddValue(account->retirees, retire_peer);

        // Write retirement to Transport
        CFErrorRef postError = NULL;
        if (!SOSTransportCirclePostRetirement(account->circle_transport, SOSCircleGetName(circle), retire_peer, &postError)){
            secwarning("Couldn't post retirement (%@)", postError);
        }
        if(!SOSTransportCircleFlushChanges(account->circle_transport, &postError)){
            secwarning("Couldn't flush retirement data (%@)", postError);
        }
        CFReleaseNull(postError);
    }

    SOSAccountPurgeIdentity(account);

    retval = true;

    CFReleaseNull(localError);
    CFReleaseNull(retire_peer);
    return retval;
}

bool sosAccountLeaveRing(SOSAccountRef account, SOSRingRef ring, CFErrorRef* error) {
    SOSFullPeerInfoRef fpi = account->my_identity;
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
    
#if 0
    // this was circle behavior - at some point
    if(SOSRingHasApplicant(ring, peerID)) {
        writeRing = true;
    }
#endif
    
    if(writePeerInfo || writeRing) {
        SOSRingWithdraw(ring, NULL, fpi, error);
    }
    
    // Write leave thing to Transport
    CFDataRef peerInfoData = SOSFullPeerInfoCopyEncodedData(fpi, kCFAllocatorDefault, error);
    SOSTransportCircleSendPeerInfo(account->circle_transport, peerID, peerInfoData, NULL); // TODO: Handle errors?
    
    if (writeRing) {
        CFDataRef ring_data = SOSRingCopyEncodedData(ring, error);
        
        if (ring_data) {
            SOSTransportCircleRingPostRing(account->circle_transport, SOSRingGetName(ring), ring_data, NULL); // TODO: Handle errors?
        }
        CFReleaseNull(ring_data);
    }
    retval = true;
    CFReleaseNull(localError);
    return retval;
}

bool SOSAccountPostDebugScope(SOSAccountRef account, CFTypeRef scope, CFErrorRef *error) {
    bool result = false;
    SOSTransportCircleRef transport = account->circle_transport;
    if (transport) {
        result = SOSTransportCircleSendDebugInfo(transport, kSOSAccountDebugScope, scope, error);
    }
    return result;
}

/*
 NSUbiquitousKeyValueStoreInitialSyncChange is only posted if there is any
 local value that has been overwritten by a distant value. If there is no
 conflict between the local and the distant values when doing the initial
 sync (e.g. if the cloud has no data stored or the client has not stored
 any data yet), you'll never see that notification.

 NSUbiquitousKeyValueStoreInitialSyncChange implies an initial round trip
 with server but initial round trip with server does not imply
 NSUbiquitousKeyValueStoreInitialSyncChange.
 */


//
// MARK: Status summary
//

static SOSCCStatus SOSCCThisDeviceStatusInCircle(SOSCircleRef circle, SOSPeerInfoRef this_peer) {
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

CFStringRef SOSAccountGetSOSCCStatusString(SOSCCStatus status) {
    switch(status) {
        case kSOSCCInCircle: return CFSTR("kSOSCCInCircle");
        case kSOSCCNotInCircle: return CFSTR("kSOSCCNotInCircle");
        case kSOSCCRequestPending: return CFSTR("kSOSCCRequestPending");
        case kSOSCCCircleAbsent: return CFSTR("kSOSCCCircleAbsent");
        case kSOSCCError: return CFSTR("kSOSCCError");
    }
    return CFSTR("kSOSCCError");
}

bool SOSAccountIsInCircle(SOSAccountRef account, CFErrorRef *error) {
    SOSCCStatus result = SOSAccountGetCircleStatus(account, error);
    
    if (result != kSOSCCInCircle) {
        SOSErrorCreate(kSOSErrorNoCircle, error, NULL, CFSTR("Not in circle"));
        return false;
    }
    
    return true;
}

SOSCCStatus SOSAccountGetCircleStatus(SOSAccountRef account, CFErrorRef* error) {
    if (!SOSAccountHasPublicKey(account, error)) {
        return kSOSCCError;
    }

    return SOSCCThisDeviceStatusInCircle(account->trusted_circle, SOSAccountGetMyPeerInfo(account));
}

//
// MARK: Account Reset Circles
//

// This needs to be called within a SOSAccountModifyCircle() block

bool SOSAccountAddiCloudIdentity(SOSAccountRef account, SOSCircleRef circle, SecKeyRef user_key, CFErrorRef *error) {
    bool result = false;
    SOSFullPeerInfoRef cloud_identity = NULL;
    SOSPeerInfoRef cloud_peer = GenerateNewCloudIdentityPeerInfo(error);
    require_quiet(cloud_peer, err_out);
    cloud_identity = CopyCloudKeychainIdentity(cloud_peer, error);
    CFReleaseNull(cloud_peer);
    require_quiet(cloud_identity, err_out);
    require_quiet(SOSCircleRequestAdmission(circle, user_key, cloud_identity, error), err_out);
    require_quiet(SOSCircleAcceptRequest(circle, user_key, account->my_identity, SOSFullPeerInfoGetPeerInfo(cloud_identity), error), err_out);
    result = true;
err_out:
    return result;
}

bool SOSAccountRemoveIncompleteiCloudIdentities(SOSAccountRef account, SOSCircleRef circle, SecKeyRef privKey, CFErrorRef *error) {
    bool retval = false;
    CFMutableSetRef iCloud2Remove = CFSetCreateMutableForCFTypes(kCFAllocatorDefault);
    
    SOSCircleForEachActivePeer(circle, ^(SOSPeerInfoRef peer) {
        if(SOSPeerInfoIsCloudIdentity(peer)) {
            SOSFullPeerInfoRef icfpi = SOSFullPeerInfoCreateCloudIdentity(kCFAllocatorDefault, peer, NULL);
            if(!icfpi) {
                CFSetAddValue(iCloud2Remove, peer);
            }
            CFReleaseNull(icfpi);
        }
    });
    
    if(CFSetGetCount(iCloud2Remove) > 0) {
        retval = true;
        SOSCircleRemovePeers(circle, privKey, account->my_identity, iCloud2Remove, error);
    }
    CFReleaseNull(iCloud2Remove);
    return retval;
}

static bool SOSAccountResetCircleToOffering(SOSAccountTransactionRef aTxn, SecKeyRef user_key, CFErrorRef *error) {
    SOSAccountRef account = aTxn->account;
    bool result = false;

    require(SOSAccountHasCircle(account, error), fail);
    require(SOSAccountEnsureFullPeerAvailable(account, error), fail);
    
    (void) SOSAccountResetAllRings(account, error);

    SOSAccountModifyCircle(account, error, ^(SOSCircleRef circle) {
        bool result = false;
        SOSFullPeerInfoRef cloud_identity = NULL;
        CFErrorRef localError = NULL;

        require_quiet(SOSCircleResetToOffering(circle, user_key, account->my_identity, &localError), err_out);

        account->departure_code = kSOSNeverLeftCircle;
        require_quiet(SOSAccountAddEscrowToPeerInfo(account, SOSAccountGetMyFullPeerInfo(account), error), err_out);

        require_quiet(SOSAccountAddiCloudIdentity(account, circle, user_key, error), err_out);
        result = true;
        SOSAccountPublishCloudParameters(account, NULL);

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
    });

    SOSAccountSetValue(account, kSOSUnsyncedViewsKey, kCFBooleanTrue, NULL);
    SOSAccountUpdateOutOfSyncViews(aTxn, SOSViewsGetAllCurrent());

    result = true;

fail:
    return result;
}


bool SOSAccountResetToOffering(SOSAccountTransactionRef aTxn, CFErrorRef* error) {
    SOSAccountRef account = aTxn->account;
    SecKeyRef user_key = SOSAccountGetPrivateCredential(account, error);
    if (!user_key)
        return false;

    CFReleaseNull(account->my_identity);
    secnotice("resetToOffering", "Resetting circle to offering by request from client");

    return user_key && SOSAccountResetCircleToOffering(aTxn, user_key, error);
}

bool SOSAccountResetToEmpty(SOSAccountRef account, CFErrorRef* error) {
    if (!SOSAccountHasPublicKey(account, error))
        return false;
    __block bool result = true;

    result &= SOSAccountResetAllRings(account, error);

    CFReleaseNull(account->my_identity);

    account->departure_code = kSOSWithdrewMembership;
    secnotice("resetToEmpty", "Reset Circle to empty by client request");
    result &= SOSAccountModifyCircle(account, error, ^(SOSCircleRef circle) {
        result = SOSCircleResetToEmpty(circle, error);
        return result;
    });

    if (!result) {
        secerror("error: %@", error ? *error : NULL);
    }
    return result;
}
//
// MARK: start backups
//

bool SOSAccountEnsureInBackupRings(SOSAccountRef account) {
    __block bool result = false;
    __block CFErrorRef error = NULL;
    secnotice("backup", "Ensuring in rings");

    CFDataRef backupKey = NULL;

    require_action_quiet(account->backup_key, exit, result = true);

    backupKey = SOSPeerInfoV2DictionaryCopyData(SOSAccountGetMyPeerInfo(account), sBackupKeyKey);
    
    bool updateBackupKey = !CFEqualSafe(backupKey, account->backup_key);
    
    if(updateBackupKey) {
        require_quiet(SOSAccountUpdatePeerInfo(account, CFSTR("Backup public key"), &error, ^bool(SOSFullPeerInfoRef fpi, CFErrorRef *error) {
            return SOSFullPeerInfoUpdateBackupKey(fpi, account->backup_key, error);
        }), exit);
    }
    require_quiet(account->backup_key, exit); // If it went null, we're done now.

    require_quiet(SOSBSKBIsGoodBackupPublic(account->backup_key, &error), exit);
    
    CFDataRef recoveryKeyBackFromRing = SOSAccountCopyRecoveryPublic(kCFAllocatorDefault, account, &error);

    if(updateBackupKey || recoveryKeyBackFromRing) {
        // It's a good key, we're going with it. Stop backing up the old way.
        CFErrorRef localError = NULL;
        if (!SOSDeleteV0Keybag(&localError)) {
            secerror("Failed to delete v0 keybag: %@", localError);
        }
        CFReleaseNull(localError);
        
        result = true;

        // Setup backups the new way.
        SOSAccountForEachBackupView(account, ^(const void *value) {
            CFStringRef viewName = (CFStringRef)value;
            if(updateBackupKey || (recoveryKeyBackFromRing && !SOSAccountRecoveryKeyIsInBackupAndCurrentInView(account, viewName))) {
                result &= SOSAccountNewBKSBForView(account, viewName, &error);
            }
        });
    }

exit:
    if (!result) {
        secnotice("backupkey", "Failed to setup backup public key: %@", error ? (CFTypeRef) error : (CFTypeRef) CFSTR("No error space provided"));
    }
    CFReleaseNull(backupKey);
    return result;
}

//
// MARK: Recovery Public Key Functions
//

bool SOSAccountRegisterRecoveryPublicKey(SOSAccountTransactionRef txn, CFDataRef recovery_key, CFErrorRef *error){
    bool retval = SOSAccountSetRecoveryKey(txn->account, recovery_key, error);
    if(retval) secnotice("recovery", "successfully registered recovery public key");
    else secnotice("recovery", "could not register recovery public key: %@", *error);
    SOSClearErrorIfTrue(retval, error);
    return retval;
}

bool SOSAccountClearRecoveryPublicKey(SOSAccountTransactionRef txn, CFDataRef recovery_key, CFErrorRef *error){
    bool retval = SOSAccountRemoveRecoveryKey(txn->account, error);
    SOSClearErrorIfTrue(retval, error);
    return retval;
}

CFDataRef SOSAccountCopyRecoveryPublicKey(SOSAccountTransactionRef txn, CFErrorRef *error){
    CFDataRef result = NULL;
    result = SOSAccountCopyRecoveryPublic(kCFAllocatorDefault, txn->account, error);
    if(!result)  secnotice("recovery", "Could not retrieve the recovery public key from the ring: %@", *error);

    if (!isData(result)) {
        CFReleaseNull(result);
    }
    SOSClearErrorIfTrue(result != NULL, error);

    return result;
}

//
// MARK: Joining
//

static bool SOSAccountJoinCircle(SOSAccountTransactionRef aTxn, SecKeyRef user_key,
                                bool use_cloud_peer, CFErrorRef* error) {
    SOSAccountRef account = aTxn->account;

    __block bool result = false;
    __block SOSFullPeerInfoRef cloud_full_peer = NULL;

    require_action_quiet(account->trusted_circle, fail, SOSCreateErrorWithFormat(kSOSErrorPeerNotFound, NULL, error, NULL, CFSTR("Don't have circle when joining???")));
    require_quiet(SOSAccountEnsureFullPeerAvailable(account, error), fail);

    SOSFullPeerInfoRef myCirclePeer = account->my_identity;

    if (SOSCircleCountPeers(account->trusted_circle) == 0 || SOSAccountGhostResultsInReset(account)) {
        secnotice("resetToOffering", "Resetting circle to offering since there are no peers");
        // this also clears initial sync data
        result = SOSAccountResetCircleToOffering(aTxn, user_key, error);
    } else {
        SOSAccountSetValue(account, kSOSUnsyncedViewsKey, kCFBooleanTrue, NULL);

        if (use_cloud_peer) {
            cloud_full_peer = SOSCircleCopyiCloudFullPeerInfoRef(account->trusted_circle, NULL);
        }

        SOSAccountModifyCircle(account, error, ^(SOSCircleRef circle) {
            result = SOSAccountAddEscrowToPeerInfo(account, myCirclePeer, error);
            result &= SOSCircleRequestAdmission(circle, user_key, myCirclePeer, error);
            account->departure_code = kSOSNeverLeftCircle;
            if(result && cloud_full_peer) {
                CFErrorRef localError = NULL;
                CFStringRef cloudid = SOSPeerInfoGetPeerID(SOSFullPeerInfoGetPeerInfo(cloud_full_peer));
                require_quiet(cloudid, finish);
                require_quiet(SOSCircleHasActivePeerWithID(circle, cloudid, &localError), finish);
                require_quiet(SOSCircleAcceptRequest(circle, user_key, cloud_full_peer, SOSFullPeerInfoGetPeerInfo(myCirclePeer), &localError), finish);

            finish:
                if (localError){
                    secerror("Failed to join with cloud identity: %@", localError);
                    CFReleaseNull(localError);
                }
            }
            return result;
        });

        if (use_cloud_peer) {
            SOSAccountUpdateOutOfSyncViews(aTxn, SOSViewsGetAllCurrent());
        }
    }

fail:
    CFReleaseNull(cloud_full_peer);
    return result;
}

static bool SOSAccountJoinCircles_internal(SOSAccountTransactionRef aTxn, bool use_cloud_identity, CFErrorRef* error) {
    SOSAccountRef account = aTxn->account;
    bool success = false;

    SecKeyRef user_key = SOSAccountGetPrivateCredential(account, error);
    require_quiet(user_key, done); // Fail if we don't get one.

    require_action_quiet(account->trusted_circle, done, SOSErrorCreate(kSOSErrorNoCircle, error, NULL, CFSTR("No circle to join")));
    
    if (account->my_identity != NULL) {
        SOSPeerInfoRef myPeer = SOSFullPeerInfoGetPeerInfo(account->my_identity);
        success = SOSCircleHasPeer(account->trusted_circle, myPeer, NULL);
        require_quiet(!success, done);

        SOSCircleRemoveRejectedPeer(account->trusted_circle, myPeer, NULL); // If we were rejected we should remove it now.

        if (!SOSCircleHasApplicant(account->trusted_circle, myPeer, NULL)) {
        	secerror("Resetting my peer (ID: %@) for circle '%@' during application", SOSPeerInfoGetPeerID(myPeer), SOSCircleGetName(account->trusted_circle));
            
			CFReleaseNull(account->my_identity);
            myPeer = NULL;
        }
    }

    success = SOSAccountJoinCircle(aTxn, user_key, use_cloud_identity, error);

    require_quiet(success, done);
       
    account->departure_code = kSOSNeverLeftCircle;

done:
    return success;
}

bool SOSAccountJoinCircles(SOSAccountTransactionRef aTxn, CFErrorRef* error) {
	secnotice("circleJoin", "Normal path circle join (SOSAccountJoinCircles)");
    return SOSAccountJoinCircles_internal(aTxn, false, error);
}

CFStringRef SOSAccountCopyDeviceID(SOSAccountRef account, CFErrorRef *error){
    CFStringRef result = NULL;

    require_action_quiet(account->my_identity, fail, SOSErrorCreate(kSOSErrorPeerNotFound, error, NULL, CFSTR("No peer for me")));

    result = SOSPeerInfoCopyDeviceID(SOSFullPeerInfoGetPeerInfo(account->my_identity));

fail:
    return result;
}

bool SOSAccountSetMyDSID(SOSAccountTransactionRef txn, CFStringRef IDS, CFErrorRef* error){
    bool result = true;
    SOSAccountRef account = txn->account;
    
    secdebug("IDS Transport", "We are setting our device ID: %@", IDS);
    if(IDS != NULL && (CFStringGetLength(IDS) > 0)){
        require_action_quiet(account->my_identity, fail, SOSErrorCreate(kSOSErrorPeerNotFound, error, NULL, CFSTR("No peer for me")));
        
        result = SOSAccountModifyCircle(account, error, ^bool(SOSCircleRef circle) {
            
            SOSFullPeerInfoUpdateDeviceID(account->my_identity, IDS, error);
            SOSFullPeerInfoUpdateTransportType(account->my_identity, SOSTransportMessageTypeIDSV2, error);
            SOSFullPeerInfoUpdateTransportPreference(account->my_identity, kCFBooleanFalse, error);
            SOSFullPeerInfoUpdateTransportFragmentationPreference(account->my_identity, kCFBooleanTrue, error);
            SOSFullPeerInfoUpdateTransportAckModelPreference(account->my_identity, kCFBooleanTrue, error);
            return SOSCircleHasPeer(circle, SOSFullPeerInfoGetPeerInfo(account->my_identity), NULL);
        });
    }
    else
        result = false;

    // Initiate sync with all IDS peers, since we just learned we can talk that way.
    SOSAccountForEachCirclePeerExceptMe(account, ^(SOSPeerInfoRef peer) {
        if (SOSPeerInfoShouldUseIDSTransport(SOSAccountGetMyPeerInfo(account), peer)) {
            SOSAccountTransactionAddSyncRequestForPeerID(txn, SOSPeerInfoGetPeerID(peer));
        }
    });

fail:
    CFReleaseNull(account->deviceID);
    account->deviceID = CFRetainSafe(IDS);
    return result;
}

bool SOSAccountSendIDSTestMessage(SOSAccountRef account, CFStringRef message, CFErrorRef *error){
    bool result = true;
    //construct message dictionary, circle -> peerID -> message
    
    CFMutableDictionaryRef peerToMessage = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);

    CFStringRef operationString = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("%d"), kIDSSendOneMessage);
    CFDictionaryRef rawMessage = CFDictionaryCreateForCFTypes(kCFAllocatorDefault,
                                                              kIDSOperationType, operationString,
                                                              kIDSMessageToSendKey, CFSTR("send IDS test message"),
                                                              NULL);

    SOSAccountForEachCirclePeerExceptMe(account, ^(SOSPeerInfoRef peer) {
        CFDictionaryAddValue(peerToMessage, SOSPeerInfoGetPeerID(peer), rawMessage);
    });

    result = SOSTransportMessageSendMessages(account->ids_message_transport, peerToMessage, error);
    
    CFReleaseNull(peerToMessage);
    CFReleaseNull(operationString);
    CFReleaseNull(rawMessage);
    return result;
}

bool SOSAccountStartPingTest(SOSAccountRef account, CFStringRef message, CFErrorRef *error){
    bool result = false;
    //construct message dictionary, circle -> peerID -> message
    
    if(account->ids_message_transport == NULL)
        account->ids_message_transport = (SOSTransportMessageRef)SOSTransportMessageIDSCreate(account, SOSCircleGetName(account->trusted_circle), error);
    
    require_quiet(account->ids_message_transport, fail);
    CFMutableDictionaryRef peerToMessage = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);

    CFStringRef operationString = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("%d"), kIDSStartPingTestMessage);
    CFDictionaryRef rawMessage = CFDictionaryCreateForCFTypes(kCFAllocatorDefault,
                                                              kIDSOperationType, operationString,
                                                              kIDSMessageToSendKey, CFSTR("send IDS test message"),
                                                              NULL);

    
    SOSAccountForEachCirclePeerExceptMe(account, ^(SOSPeerInfoRef peer) {
        CFDictionaryAddValue(peerToMessage, SOSPeerInfoGetPeerID(peer), rawMessage);
    });
    
    result = SOSTransportMessageSendMessages(account->ids_message_transport, peerToMessage, error);
    
    CFReleaseNull(peerToMessage);
    CFReleaseNull(rawMessage);
    CFReleaseNull(operationString);
fail:
    return result;
}

bool SOSAccountRetrieveDeviceIDFromKeychainSyncingOverIDSProxy(SOSAccountRef account, CFErrorRef *error){
    bool result = true;
    
    __block bool success = true;
    __block CFErrorRef localError = NULL;
    dispatch_semaphore_t wait_for = dispatch_semaphore_create(0);
    dispatch_retain(wait_for); // Both this scope and the block own it
    
    SOSCloudKeychainGetIDSDeviceID(^(CFDictionaryRef returnedValues, CFErrorRef sync_error){
        success = (sync_error == NULL);
        if (!success) {
            CFRetainAssign(localError, sync_error);
        }
        
        dispatch_semaphore_signal(wait_for);
        dispatch_release(wait_for);
    });
    
    dispatch_semaphore_wait(wait_for, DISPATCH_TIME_FOREVER);
    dispatch_release(wait_for);
    
    if(!success && localError != NULL && error != NULL){
        secerror("Could not ask KeychainSyncingOverIDSProxy for Device ID: %@", localError);
        *error = localError;
        result = false;
    }
    else{
        secdebug("IDS Transport", "Attempting to retrieve the IDS Device ID");
    }
    return result;
}

bool SOSAccountJoinCirclesAfterRestore(SOSAccountTransactionRef aTxn, CFErrorRef* error) {
	secnotice("circleJoin", "Joining after restore (SOSAccountJoinCirclesAfterRestore)");
    return SOSAccountJoinCircles_internal(aTxn, true, error);
}


bool SOSAccountLeaveCircle(SOSAccountRef account, CFErrorRef* error)
{
    bool result = true;

    secnotice("leaveCircle", "Leaving circle by client request");
    result &= SOSAccountModifyCircle(account, error, ^(SOSCircleRef circle) {
        return sosAccountLeaveCircle(account, circle, error);
    });

    account->departure_code = kSOSWithdrewMembership;

    return result;
}

bool SOSAccountRemovePeersFromCircle(SOSAccountRef account, CFArrayRef peers, CFErrorRef* error)
{
    bool result = false;
    CFMutableSetRef peersToRemove = NULL;
    SecKeyRef user_key = SOSAccountGetPrivateCredential(account, error);
    require_action_quiet(user_key, errOut, secnotice("removePeers", "Can't remove without userKey"));

    SOSFullPeerInfoRef me_full = SOSAccountGetMyFullPeerInfo(account);
    SOSPeerInfoRef me = SOSAccountGetMyPeerInfo(account);
    require_action_quiet(me_full && me, errOut, {
                            SOSErrorCreate(kSOSErrorPeerNotFound, error, NULL, CFSTR("Can't remove without being active peer"));
                            secnotice("removePeers", "Can't remove without being active peer");
                         });
    
    result = true; // beyond this point failures would be rolled up in AccountModifyCircle.

    peersToRemove = CFSetCreateMutableForSOSPeerInfosByIDWithArray(kCFAllocatorDefault, peers);
    require_action_quiet(peersToRemove, errOut, secnotice("removePeers", "No peerSet to remove"));

    // If we're one of the peers expected to leave - note that and then remove ourselves from the set (different handling).
    bool leaveCircle = CFSetContainsValue(peersToRemove, me);
    CFSetRemoveValue(peersToRemove, me);

    result &= SOSAccountModifyCircle(account, error, ^(SOSCircleRef circle) {
        bool success = false;

        if(CFSetGetCount(peersToRemove) != 0) {
            require_quiet(SOSCircleRemovePeers(circle, user_key, me_full, peersToRemove, error), done);
            success = SOSAccountGenerationSignatureUpdate(account, error);
        } else success = true;

        if (success && leaveCircle) {
            secnotice("leaveCircle", "Leaving circle by client request");
            success = sosAccountLeaveCircle(account, circle, error);
        }

    done:
        return success;

    });

errOut:
    CFReleaseNull(peersToRemove);
    return result;
}


bool SOSAccountBail(SOSAccountRef account, uint64_t limit_in_seconds, CFErrorRef* error) {
    dispatch_queue_t queue = dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0);
    dispatch_group_t group = dispatch_group_create();
    __block bool result = false;
    secnotice("circle", "Attempting to leave circle - best effort - in %llu seconds\n", limit_in_seconds);
    // Add a task to the group
    dispatch_group_async(group, queue, ^{
        SOSAccountModifyCircle(account, error, ^(SOSCircleRef circle) {
            secnotice("leaveCircle", "Leaving circle by client request");
            return sosAccountLeaveCircle(account, circle, error);
        });
    });
    dispatch_time_t milestone = dispatch_time(DISPATCH_TIME_NOW, limit_in_seconds * NSEC_PER_SEC);
    dispatch_group_wait(group, milestone);

    account->departure_code = kSOSWithdrewMembership;

    dispatch_release(group);
    return result;
}


//
// MARK: Application
//

static void for_each_applicant_in_each_circle(SOSAccountRef account, CFArrayRef peer_infos,
                                              bool (^action)(SOSCircleRef circle, SOSFullPeerInfoRef myCirclePeer, SOSPeerInfoRef peer)) {
        SOSPeerInfoRef me = SOSFullPeerInfoGetPeerInfo(account->my_identity);
        CFErrorRef peer_error = NULL;
        if (account->trusted_circle && me &&
            SOSCircleHasPeer(account->trusted_circle, me, &peer_error)) {
            SOSAccountModifyCircle(account, NULL, ^(SOSCircleRef circle) {
                __block bool modified = false;
                CFArrayForEach(peer_infos, ^(const void *value) {
                    SOSPeerInfoRef peer = (SOSPeerInfoRef) value;
                    if (isSOSPeerInfo(peer) && SOSCircleHasApplicant(circle, peer, NULL)) {
                        if (action(circle, account->my_identity, peer)) {
                            modified = true;
                        }
                    }
                });
                return modified;
            });
        }
        if (peer_error)
            secerror("Got error in SOSCircleHasPeer: %@", peer_error);
        CFReleaseSafe(peer_error); // TODO: We should be accumulating errors here.
}

bool SOSAccountAcceptApplicants(SOSAccountRef account, CFArrayRef applicants, CFErrorRef* error) {
    SecKeyRef user_key = SOSAccountGetPrivateCredential(account, error);
    if (!user_key)
        return false;

    __block bool success = true;
    __block int64_t num_peers = 0;

    for_each_applicant_in_each_circle(account, applicants, ^(SOSCircleRef circle, SOSFullPeerInfoRef myCirclePeer, SOSPeerInfoRef peer) {
        bool accepted = SOSCircleAcceptRequest(circle, user_key, myCirclePeer, peer, error);
        if (!accepted)
            success = false;
		else
			num_peers = MAX(num_peers, SOSCircleCountPeers(circle));
        return accepted;
    });

    return success;
}

bool SOSAccountRejectApplicants(SOSAccountRef account, CFArrayRef applicants, CFErrorRef* error) {
    __block bool success = true;
    __block int64_t num_peers = 0;

    for_each_applicant_in_each_circle(account, applicants, ^(SOSCircleRef circle, SOSFullPeerInfoRef myCirclePeer, SOSPeerInfoRef peer) {
        bool rejected = SOSCircleRejectRequest(circle, myCirclePeer, peer, error);
        if (!rejected)
            success = false;
		else
			num_peers = MAX(num_peers, SOSCircleCountPeers(circle));
        return rejected;
    });

    return success;
}


CFStringRef SOSAccountCopyIncompatibilityInfo(SOSAccountRef account, CFErrorRef* error) {
    return CFSTR("We're compatible, go away");
}

enum DepartureReason SOSAccountGetLastDepartureReason(SOSAccountRef account, CFErrorRef* error) {
    return account->departure_code;
}

void SOSAccountSetLastDepartureReason(SOSAccountRef account, enum DepartureReason reason) {
	account->departure_code = reason;
}


CFArrayRef SOSAccountCopyGeneration(SOSAccountRef account, CFErrorRef *error) {
    CFArrayRef result = NULL;
    CFNumberRef generation = NULL;

    require_quiet(SOSAccountHasPublicKey(account, error), fail);
    require_action_quiet(account->trusted_circle, fail, SOSErrorCreate(kSOSErrorNoCircle, error, NULL, CFSTR("No circle")));

    generation = (CFNumberRef)SOSCircleGetGeneration(account->trusted_circle);
    result = CFArrayCreateForCFTypes(kCFAllocatorDefault, generation, NULL);

fail:
    return result;
}

bool SOSValidateUserPublic(SOSAccountRef account, CFErrorRef *error) {
    if (!SOSAccountHasPublicKey(account, error))
        return NULL;

    return account->user_public_trusted;
}

bool SOSAccountEnsurePeerRegistration(SOSAccountRef account, CFErrorRef *error) {
    // TODO: this result is never set or used
    bool result = true;

    secnotice("updates", "Ensuring peer registration.");

    require_quiet(account->trusted_circle, done);
    require_quiet(account->my_identity, done);
    require_quiet(account->user_public_trusted, done);
    
    // If we are not in the circle, there is no point in setting up peers
    require_quiet(SOSAccountIsMyPeerActive(account, NULL), done);

    // This code only uses the SOSFullPeerInfoRef for two things:
    //  - Finding out if this device is in the trusted circle
    //  - Using the peerID for this device to see if the current peer is "me"
    //  - It is used indirectly by passing account->my_identity to SOSEngineInitializePeerCoder
    
    CFStringRef my_id = SOSPeerInfoGetPeerID(SOSFullPeerInfoGetPeerInfo(account->my_identity));

    SOSCircleForEachValidSyncingPeer(account->trusted_circle, account->user_public, ^(SOSPeerInfoRef peer) {
        if (!SOSPeerInfoPeerIDEqual(peer, my_id)) {
            CFErrorRef localError = NULL;
            SOSTransportMessageRef messageTransport = NULL;
            
            messageTransport = SOSPeerInfoHasDeviceID(peer) ? account->ids_message_transport : account->kvs_message_transport;
            
            SOSEngineInitializePeerCoder(messageTransport->engine, account->my_identity, peer, &localError);
            if (localError)
                secnotice("updates", "can't initialize transport for peer %@ with %@ (%@)", peer, account->my_identity, localError);
            CFReleaseSafe(localError);
        }
    });

    //Initialize our device ID
    SOSTransportMessageIDSGetIDSDeviceID(account);    
    
    
done:
    return result;
}

bool SOSAccountAddEscrowRecords(SOSAccountRef account, CFStringRef dsid, CFDictionaryRef record, CFErrorRef *error){
    CFMutableDictionaryRef escrowRecords = (CFMutableDictionaryRef)SOSAccountGetValue(account, kSOSEscrowRecord, error);
    CFMutableDictionaryRef escrowCopied = NULL;
    bool success = false;
    
    if(isDictionary(escrowRecords) && escrowRecords != NULL)
        escrowCopied = CFDictionaryCreateMutableCopy(kCFAllocatorDefault, CFDictionaryGetCount(escrowRecords), escrowRecords);
    else
        escrowCopied = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
    
    CFDictionaryAddValue(escrowCopied, dsid, record);
    SOSAccountSetValue(account, kSOSEscrowRecord, escrowCopied, error);
  
    if(*error == NULL)
        success = true;
    
    CFReleaseNull(escrowCopied);
    
    return success;
    
}

bool SOSAccountAddEscrowToPeerInfo(SOSAccountRef account, SOSFullPeerInfoRef myPeer, CFErrorRef *error){
    bool success = false;
    
    CFDictionaryRef escrowRecords = SOSAccountGetValue(account, kSOSEscrowRecord, error);
    success = SOSFullPeerInfoReplaceEscrowRecords(myPeer, escrowRecords, error);
    
    return success;
}

bool SOSAccountCheckPeerAvailability(SOSAccountRef account, CFErrorRef *error)
{
    CFStringRef operationString = NULL;
    CFDictionaryRef rawMessage = NULL;
    CFMutableSetRef peers = NULL;
    CFMutableDictionaryRef peerList = NULL;
    char* message = NULL;
    bool result = false;

    if(account->ids_message_transport == NULL)
        account->ids_message_transport = (SOSTransportMessageRef)SOSTransportMessageIDSCreate(account, SOSCircleGetName(account->trusted_circle), error);
    
    require_quiet(account->ids_message_transport, fail);

    //adding message type kIDSPeerAvailability so KeychainSyncingOverIDSProxy does not send this message as a keychain item

    operationString = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("%d"), kIDSPeerAvailability);
    rawMessage = CFDictionaryCreateForCFTypes(kCFAllocatorDefault,
                                              kIDSOperationType, operationString,
                                              kIDSMessageToSendKey, CFSTR("checking peers"),
                                              NULL);

    peerList = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
    SOSCircleRef circle = account->trusted_circle;
    
    //check each peer to make sure they have the right view set enabled
    CFSetRef mySubSet = SOSViewsGetV0SubviewSet();
    SOSCircleForEachValidPeer(circle, account->user_public, ^(SOSPeerInfoRef peer) {
        if(!CFEqualSafe(peer, SOSAccountGetMyPeerInfo(account))){
            CFMutableSetRef peerViews = SOSPeerInfoCopyEnabledViews(peer);
            CFSetRef intersectSets = CFSetCreateIntersection(kCFAllocatorDefault, mySubSet, peerViews);
            if(CFEqualSafe(intersectSets, mySubSet)){
                CFStringRef deviceID = SOSPeerInfoCopyDeviceID(peer);
                if(deviceID != NULL)
                    CFDictionaryAddValue(peerList, SOSPeerInfoGetPeerID(peer), rawMessage);
                CFReleaseNull(deviceID);
            }
            CFReleaseNull(peerViews);
            CFReleaseNull(intersectSets);
        }
    });
        
    require_quiet(CFDictionaryGetCount(peerList) > 0 , fail);
    result = SOSTransportMessageSendMessages(account->ids_message_transport, peerList, error);
    
fail:
    CFReleaseNull(rawMessage);
    CFReleaseNull(operationString);
    CFReleaseNull(peerList);
    CFReleaseNull(peers);
    free(message);
    return result;
}


void SOSAccountRecordRetiredPeersInCircle(SOSAccountRef account) {
    if (!SOSAccountIsInCircle(account, NULL))
        return;

    SOSAccountModifyCircle(account, NULL, ^bool (SOSCircleRef circle) {
        __block bool updated = false;
        CFSetForEach(account->retirees, ^(CFTypeRef element){
            SOSPeerInfoRef retiree = asSOSPeerInfo(element);

            if (retiree && SOSCircleUpdatePeerInfo(circle, retiree)) {
                updated = true;
                secnotice("retirement", "Updated retired peer %@ in %@", retiree, circle);
                CFErrorRef cleanupError = NULL;
                if (!SOSAccountCleanupAfterPeer(account, RETIREMENT_FINALIZATION_SECONDS, circle, retiree, &cleanupError))
                    secerror("Error cleanup up after peer (%@): %@", retiree, cleanupError);
                CFReleaseSafe(cleanupError);
            }
        });
        return updated;
    });
}


static size_t SOSPiggyBackBlobGetDEREncodedSize(SOSGenCountRef gencount, SecKeyRef pubKey, CFDataRef signature, CFErrorRef *error) {
    size_t total_payload = 0;
    
    CFDataRef publicBytes = NULL;
    OSStatus result = SecKeyCopyPublicBytes(pubKey, &publicBytes);
    
    if (result != errSecSuccess) {
        SOSCreateError(kSOSErrorBadKey, CFSTR("Failed to export public bytes"), NULL, error);
        return 0;
    }

    require_quiet(accumulate_size(&total_payload, der_sizeof_number(gencount, error)), errOut);
    require_quiet(accumulate_size(&total_payload, der_sizeof_data_or_null(publicBytes, error)), errOut);
    require_quiet(accumulate_size(&total_payload, der_sizeof_data_or_null(signature, error)), errOut);
    return ccder_sizeof(CCDER_CONSTRUCTED_SEQUENCE, total_payload);
    
errOut:
    SecCFDERCreateError(kSecDERErrorUnknownEncoding, CFSTR("don't know how to encode"), NULL, error);
    return 0;
}

static uint8_t* SOSPiggyBackBlobEncodeToDER(SOSGenCountRef gencount, SecKeyRef pubKey, CFDataRef signature, CFErrorRef* error, const uint8_t* der, uint8_t* der_end) {
    CFDataRef publicBytes = NULL;

    OSStatus result = SecKeyCopyPublicBytes(pubKey, &publicBytes);
    
    if (result != errSecSuccess) {
        SOSCreateError(kSOSErrorBadKey, CFSTR("Failed to export public bytes"), NULL, error);
        return NULL;
    }

    
    der_end =  ccder_encode_constructed_tl(CCDER_CONSTRUCTED_SEQUENCE, der_end, der,
            der_encode_number(gencount, error, der,
            der_encode_data_or_null(publicBytes, error, der,
            der_encode_data_or_null(signature, error, der, der_end))));
    return der_end;
}

static CFDataRef SOSPiggyBackBlobCopyEncodedData(SOSGenCountRef gencount, SecKeyRef pubKey, CFDataRef signature, CFAllocatorRef allocator, CFErrorRef *error)
{
    return CFDataCreateWithDER(kCFAllocatorDefault, SOSPiggyBackBlobGetDEREncodedSize(gencount, pubKey, signature, error), ^uint8_t*(size_t size, uint8_t *buffer) {
        return SOSPiggyBackBlobEncodeToDER(gencount, pubKey, signature, error, buffer, (uint8_t *) buffer + size);
    });
}

struct piggyBackBlob {
    SOSGenCountRef gencount;
    SecKeyRef pubKey;
    CFDataRef signature;
};

static struct piggyBackBlob *SOSPiggyBackBlobCreateFromDER(CFAllocatorRef allocator, CFErrorRef *error,
                                                           const uint8_t** der_p, const uint8_t *der_end) {
    const uint8_t *sequence_end;
    struct piggyBackBlob *retval = NULL;
    SOSGenCountRef gencount = NULL;
    CFDataRef signature = NULL;
    CFDataRef publicBytes = NULL;
    
    *der_p = ccder_decode_constructed_tl(CCDER_CONSTRUCTED_SEQUENCE, &sequence_end, *der_p, der_end);
    require_action_quiet(sequence_end != NULL, errOut,
                         SOSCreateError(kSOSErrorBadFormat, CFSTR("Bad Blob DER"), (error != NULL) ? *error : NULL, error));
    *der_p = der_decode_number(allocator, 0, &gencount, error, *der_p, sequence_end);
    *der_p = der_decode_data_or_null(kCFAllocatorDefault, &publicBytes, error, *der_p, der_end);
    *der_p = der_decode_data_or_null(kCFAllocatorDefault, &signature, error, *der_p, der_end);
    require_action_quiet(*der_p && *der_p == der_end, errOut,
                         SOSCreateError(kSOSErrorBadFormat, CFSTR("Didn't consume all bytes for pbblob"), (error != NULL) ? *error : NULL, error));
    retval = malloc(sizeof(struct piggyBackBlob));
    retval->gencount = gencount;
    retval->signature = signature;
    retval->pubKey = SecKeyCreateFromPublicData(kCFAllocatorDefault, kSecECDSAAlgorithmID, publicBytes);
    
errOut:
    if(!retval) {
        CFReleaseNull(gencount);
        CFReleaseNull(publicBytes);
        CFReleaseNull(signature);
    }
    return retval;
}

static struct piggyBackBlob *SOSPiggyBackBlobCreateFromData(CFAllocatorRef allocator, CFDataRef blobData, CFErrorRef *error)
{
    size_t size = CFDataGetLength(blobData);
    const uint8_t *der = CFDataGetBytePtr(blobData);
    struct piggyBackBlob *inflated = SOSPiggyBackBlobCreateFromDER(allocator, error, &der, der + size);
    return inflated;
}



SOSPeerInfoRef SOSAccountCopyApplication(SOSAccountRef account, CFErrorRef* error) {
    SOSPeerInfoRef applicant = NULL;
    SecKeyRef userKey = SOSAccountGetPrivateCredential(account, error);
    if(!userKey) return false;
    require_quiet(SOSAccountEnsureFullPeerAvailable(account, error), errOut);
    require(SOSFullPeerInfoPromoteToApplication(account->my_identity, userKey, error), errOut);
    applicant = SOSPeerInfoCreateCopy(kCFAllocatorDefault,  (SOSFullPeerInfoGetPeerInfo(account->my_identity)), error);
errOut:
    return applicant;
}


CFDataRef SOSAccountCopyCircleJoiningBlob(SOSAccountRef account, SOSPeerInfoRef applicant, CFErrorRef *error) {
    SOSGenCountRef gencount = NULL;
    CFDataRef signature = NULL;
    SecKeyRef ourKey = NULL;

    CFDataRef pbblob = NULL;

	secnotice("circleJoin", "Making circle joining blob as sponsor (SOSAccountCopyCircleJoiningBlob)");

    SecKeyRef userKey = SOSAccountGetTrustedPublicCredential(account, error);
    require_quiet(userKey, errOut);

    require_action_quiet(applicant, errOut, SOSCreateError(kSOSErrorProcessingFailure, CFSTR("No applicant provided"), (error != NULL) ? *error : NULL, error));
    require_quiet(SOSPeerInfoApplicationVerify(applicant, userKey, error), errOut);

    {
        SOSFullPeerInfoRef fpi = SOSAccountGetMyFullPeerInfo(account);
        ourKey = SOSFullPeerInfoCopyDeviceKey(fpi, error);
        require_quiet(ourKey, errOut);
    }

    SOSCircleRef currentCircle = SOSAccountGetCircle(account, error);
    require_quiet(currentCircle, errOut);

    SOSCircleRef prunedCircle = SOSCircleCopyCircle(NULL, currentCircle, error);
    require_quiet(prunedCircle, errOut);
    require_quiet(SOSCirclePreGenerationSign(prunedCircle, userKey, error), errOut);

    gencount = SOSGenerationIncrementAndCreate(SOSCircleGetGeneration(prunedCircle));

    signature = SOSCircleCopyNextGenSignatureWithPeerAdded(prunedCircle, applicant, ourKey, error);
    require_quiet(signature, errOut);

    pbblob = SOSPiggyBackBlobCopyEncodedData(gencount, ourKey, signature, kCFAllocatorDefault, error);
    
errOut:
    CFReleaseNull(gencount);
    CFReleaseNull(signature);
    CFReleaseNull(ourKey);

	if(!pbblob) {
		secnotice("circleJoin", "Failed to make circle joining blob as sponsor %@", *error);
	}

    return pbblob;
}

bool SOSAccountJoinWithCircleJoiningBlob(SOSAccountRef account, CFDataRef joiningBlob, CFErrorRef *error) {
    bool retval = false;
    SecKeyRef userKey = NULL;
    struct piggyBackBlob *pbb = NULL;
    
	secnotice("circleJoin", "Joining circles through piggy-back (SOSAccountCopyCircleJoiningBlob)");

    userKey = SOSAccountGetPrivateCredential(account, error);
    require_quiet(userKey, errOut);
    pbb = SOSPiggyBackBlobCreateFromData(kCFAllocatorDefault, joiningBlob, error);
    require_quiet(pbb, errOut);

    SOSAccountSetValue(account, kSOSUnsyncedViewsKey, kCFBooleanTrue, NULL);

    retval = SOSAccountModifyCircle(account, error, ^bool(SOSCircleRef copyOfCurrent) {
        return SOSCircleAcceptPeerFromHSA2(copyOfCurrent, userKey,
                                           pbb->gencount,
                                           pbb->pubKey,
                                           pbb->signature,
                                           account->my_identity, error);;

    });
    
errOut:
    if(pbb) {
        CFReleaseNull(pbb->gencount);
        CFReleaseNull(pbb->pubKey);
        CFReleaseNull(pbb->signature);
        free(pbb);
    }
    return retval;
}

static char boolToChars(bool val, char truechar, char falsechar) {
    return val? truechar: falsechar;
}

#define ACCOUNTLOGSTATE "accountLogState"
void SOSAccountLogState(SOSAccountRef account) {
    bool hasPubKey = account->user_public != NULL;
    bool pubTrusted = account->user_public_trusted;
    bool hasPriv = account->_user_private != NULL;
    SOSCCStatus stat = SOSAccountGetCircleStatus(account, NULL);
    CFStringRef userPubKeyID =  (account->user_public) ? SOSCopyIDOfKeyWithLength(account->user_public, 8, NULL):
            CFStringCreateCopy(kCFAllocatorDefault, CFSTR("*No Key*"));

    secnotice(ACCOUNTLOGSTATE, "Start");

    secnotice(ACCOUNTLOGSTATE, "ACCOUNT: [keyStatus: %c%c%c hpub %@] [SOSCCStatus: %@]",
              boolToChars(hasPubKey, 'U', 'u'), boolToChars(pubTrusted, 'T', 't'), boolToChars(hasPriv, 'I', 'i'),
              userPubKeyID,
              SOSAccountGetSOSCCStatusString(stat)
              );
    CFReleaseNull(userPubKeyID);
    if(account->trusted_circle)  SOSCircleLogState(ACCOUNTLOGSTATE, account->trusted_circle, account->user_public, SOSAccountGetMyPeerID(account));
    else secnotice(ACCOUNTLOGSTATE, "ACCOUNT: No Circle");
}

void SOSAccountLogViewState(SOSAccountRef account) {
    bool isInCircle = SOSAccountIsInCircle(account, NULL);
    require_quiet(isInCircle, imOut);
    SOSPeerInfoRef mpi = SOSAccountGetMyPeerInfo(account);
    bool isInitialComplete = SOSAccountHasCompletedInitialSync(account);
    bool isBackupComplete = SOSAccountHasCompletedRequiredBackupSync(account);

    CFSetRef views = mpi ? SOSPeerInfoCopyEnabledViews(mpi) : NULL;
    CFStringSetPerformWithDescription(views, ^(CFStringRef description) {
        secnotice(ACCOUNTLOGSTATE, "Sync: %c%c PeerViews: %@",
                  boolToChars(isInitialComplete, 'I', 'i'),
                  boolToChars(isBackupComplete, 'B', 'b'),
                  description);
    });
    CFReleaseNull(views);
    CFSetRef unsyncedViews = SOSAccountCopyOutstandingViews(account);
    CFStringSetPerformWithDescription(views, ^(CFStringRef description) {
        secnotice(ACCOUNTLOGSTATE, "outstanding views: %@", description);
    });
    CFReleaseNull(unsyncedViews);

imOut:
    secnotice(ACCOUNTLOGSTATE, "Finish");

    return;
}


void SOSAccountSetTestSerialNumber(SOSAccountRef account, CFStringRef serial) {
    if(!isString(serial)) return;
    CFMutableDictionaryRef newv2dict = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
    CFDictionarySetValue(newv2dict, sSerialNumberKey, serial);
    SOSAccountUpdateV2Dictionary(account, newv2dict);
}

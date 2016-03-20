/*
 * Copyright (c) 2012-2014 Apple Inc. All Rights Reserved.
 */

/*
 * SOSAccount.c -  Implementation of the secure object syncing account.
 * An account contains a SOSCircle for each protection domain synced.
 */

#include "SOSAccountPriv.h"
#include <Security/SecureObjectSync/SOSPeerInfoCollections.h>
#include <Security/SecureObjectSync/SOSTransportCircle.h>
#include <Security/SecureObjectSync/SOSTransportMessage.h>
#include <Security/SecureObjectSync/SOSTransportMessageIDS.h>
#include <Security/SecureObjectSync/SOSKVSKeys.h>
#include <Security/SecureObjectSync/SOSTransport.h>
#include <Security/SecureObjectSync/SOSTransportKeyParameter.h>
#include <Security/SecureObjectSync/SOSTransportKeyParameterKVS.h>
#include <Security/SecureObjectSync/SOSEngine.h>
#include <Security/SecureObjectSync/SOSPeerCoder.h>
#include <Security/SecureObjectSync/SOSInternal.h>
#include <Security/SecureObjectSync/SOSRing.h>
#include <Security/SecureObjectSync/SOSRingUtils.h>
#include <Security/SecureObjectSync/SOSPeerInfoSecurityProperties.h>
#include <Security/SecureObjectSync/SOSPeerInfoV2.h>
#include <Security/SecItemInternal.h>
#include <SOSCircle/CKBridge/SOSCloudKeychainClient.h>
#include <SOSCircle/Regressions/SOSRegressionUtilities.h>

CFGiblisWithCompareFor(SOSAccount);

const CFStringRef SOSTransportMessageTypeIDS = CFSTR("IDS");
const CFStringRef SOSTransportMessageTypeKVS = CFSTR("KVS");
const CFStringRef kSOSDSIDKey = CFSTR("AccountDSID");
const CFStringRef kSOSEscrowRecord = CFSTR("EscrowRecord");
const CFStringRef kSOSUnsyncedViewsKey = CFSTR("unsynced");

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

    a->notification_cleanups = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);

    a->gestalt = CFRetainSafe(gestalt);

    a->trusted_circle = NULL;
    a->trusted_rings = CFDictionaryCreateMutableForCFTypes(allocator);
    a->backups = CFDictionaryCreateMutableForCFTypes(allocator);
    a->my_identity = NULL;
    a->retirees = CFSetCreateMutableForSOSPeerInfosByID(allocator);

    a->factory = factory; // We adopt the factory. kthanksbai.
    
    a->_user_private = NULL;
    a->_password_tmp = NULL;
    a->user_private_timer = NULL;

    a->change_blocks = CFArrayCreateMutableForCFTypes(allocator);
    a->waitForInitialSync_blocks = CFDictionaryCreateMutableForCFTypes(allocator);
    a->departure_code = kSOSNeverAppliedToCircle;

    a->key_transport = (SOSTransportKeyParameterRef)SOSTransportKeyParameterKVSCreate(a, NULL);
    a->circle_transport = NULL;
    a->kvs_message_transport = NULL;
    a->ids_message_transport = NULL;
    a->expansion = CFDictionaryCreateMutableForCFTypes(allocator);
    
    return a;
}

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
            secnotice("circleChange", "dCalling SOSCircleUpdatePeerInfo for gestalt change");
            return SOSCircleUpdatePeerInfo(circle_to_change, SOSAccountGetMyPeerInfo(account));
        });
    }

    CFRetainAssign(account->gestalt, new_gestalt);
    return true;
}

bool SOSAccountUpdateDSID(SOSAccountRef account, CFStringRef dsid){
    SOSAccountSetValue(account, kSOSDSIDKey, dsid, NULL);
    //send new DSID over account changed
    SOSTransportCircleSendOfficialDSID(account->circle_transport, dsid, NULL);
    
    return true;
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

SOSViewResultCode SOSAccountUpdateView(SOSAccountRef account, CFStringRef viewname, SOSViewActionCode actionCode, CFErrorRef *error) {
    SOSViewResultCode retval = kSOSCCGeneralViewError;
    SOSViewResultCode currentStatus = kSOSCCGeneralViewError;
    bool updateCircle = false;
    require_action_quiet(account->trusted_circle, errOut, SOSCreateError(kSOSErrorNoCircle, CFSTR("No Trusted Circle"), NULL, error));
    require_action_quiet(account->my_identity, errOut, SOSCreateError(kSOSErrorPeerNotFound, CFSTR("No Peer for Account"), NULL, error));
    require_action_quiet((actionCode == kSOSCCViewEnable) || (actionCode == kSOSCCViewDisable), errOut, CFSTR("Invalid View Action"));
    currentStatus = SOSAccountViewStatus(account, viewname, error);
    require_action_quiet((currentStatus == kSOSCCViewNotMember) || (currentStatus == kSOSCCViewMember), errOut, CFSTR("View Membership Not Actionable"));

    if (CFEqualSafe(viewname, kSOSViewKeychainV0)) {
        // The V0 view switches on and off all on it's own, we allow people the delusion
        // of control and status if it's what we're stuck at., otherwise error.
        if (SOSAccountSyncingV0(account)) {
            require_action_quiet(actionCode = kSOSCCViewDisable, errOut, CFSTR("Can't disable V0 view and it's on right now"));
            retval = kSOSCCViewMember;
        } else {
            require_action_quiet(actionCode = kSOSCCViewEnable, errOut, CFSTR("Can't enable V0 and it's off right now"));
            retval = kSOSCCViewNotMember;
        }
    } else if (SOSAccountSyncingV0(account) && SOSViewsIsV0Subview(viewname)) {
        // Subviews of V0 syncing can't be turned off if V0 is on.
        require_action_quiet(actionCode = kSOSCCViewDisable, errOut, CFSTR("Have V0 peer can't disable"));
        retval = kSOSCCViewMember;
    } else {
        if(actionCode == kSOSCCViewEnable && currentStatus == kSOSCCViewNotMember) {
            retval = SOSFullPeerInfoUpdateViews(account->my_identity, actionCode, viewname, error);
            if(retval == kSOSCCViewMember) updateCircle = true;
        } else if(actionCode == kSOSCCViewDisable && currentStatus == kSOSCCViewMember) {
            retval = SOSFullPeerInfoUpdateViews(account->my_identity, actionCode, viewname, error);
            if(retval == kSOSCCViewNotMember) updateCircle = true;
        } else {
            retval = currentStatus;
        }

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

    retval = SOSFullPeerInfoViewStatus(account->my_identity, viewname, error);

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
        secnotice("circleChange", "%@ list: %@", label, views);
    } else {
        secnotice("circleChange", "No %@ list provided.", label);
    }
}

bool SOSAccountUpdateViewSets(SOSAccountRef account, CFSetRef enabledViews, CFSetRef disabledViews) {
    bool updateCircle = false;
    dumpViewSet(CFSTR("Enabled"), enabledViews);
    dumpViewSet(CFSTR("Disabled"), disabledViews);
    
    require_action_quiet(account->trusted_circle, errOut, secnotice("views", "Attempt to set viewsets with no trusted circle"));
    require_action_quiet(account->my_identity, errOut, secnotice("views", "Attempt to set viewsets with no fullPeerInfo"));
    require_action_quiet(enabledViews || disabledViews, errOut, secnotice("views", "No work to do"));
    
    // Copy my views
    SOSFullPeerInfoRef fpi = SOSAccountGetMyFullPeerInfo(account);
    SOSPeerInfoRef  pi = SOSPeerInfoCreateCopy(kCFAllocatorDefault, SOSFullPeerInfoGetPeerInfo(fpi), NULL);
    
    require_action_quiet(pi, errOut, secnotice("views", "Couldn't copy PeerInfoRef"));
    
    
    if(!SOSPeerInfoVersionIsCurrent(pi)) {
        if(!SOSPeerInfoUpdateToV2(pi, NULL)) {
            secnotice("views", "Unable to update peer to V2- can't update views");
            return false;
        }
    }
    
    if(enabledViews) updateCircle = SOSViewSetEnable(pi, enabledViews);
    if(disabledViews) updateCircle |= SOSViewSetDisable(pi, disabledViews);
    
    /* UPDATE FULLPEERINFO VIEWS */
    
    if (updateCircle && SOSFullPeerInfoUpdateToThisPeer(fpi, pi, NULL)) {
        SOSAccountModifyCircle(account, NULL, ^(SOSCircleRef circle_to_change) {
            secnotice("circleChange", "Calling SOSCircleUpdatePeerInfo for views change");
            return SOSCircleUpdatePeerInfo(circle_to_change, SOSFullPeerInfoGetPeerInfo(account->my_identity));
        });
    }
    
errOut:
    return updateCircle;
}


SOSAccountRef SOSAccountCreate(CFAllocatorRef allocator,
                               CFDictionaryRef gestalt,
                               SOSDataSourceFactoryRef factory) {
    SOSAccountRef a = SOSAccountCreateBasic(allocator, gestalt, factory);

    SOSAccountEnsureFactoryCircles(a);

    SOSUpdateKeyInterest(a);

    return a;
}

static void SOSAccountDestroy(CFTypeRef aObj) {
    SOSAccountRef a = (SOSAccountRef) aObj;

    // We don't own the factory, merely have a reference to the singleton
    //    Don't free it.
    //   a->factory

    SOSAccountCleanupNotificationForAllPeers(a);

    SOSEngineRef engine = SOSDataSourceFactoryGetEngineForDataSourceName(a->factory, SOSCircleGetName(a->trusted_circle), NULL);

    if (engine)
        SOSEngineSetSyncCompleteListenerQueue(engine, NULL);

    dispatch_sync(a->queue, ^{
        CFReleaseNull(a->gestalt);

        CFReleaseNull(a->my_identity);
        CFReleaseNull(a->trusted_circle);
        CFReleaseNull(a->trusted_rings);
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
        CFReleaseNull(a->notification_cleanups);

        dispatch_release(a->user_private_timer);
        CFReleaseNull(a->change_blocks);
        CFReleaseNull(a->waitForInitialSync_blocks);
        CFReleaseNull(a->expansion);

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
    CFReleaseNull(a->trusted_rings);
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
    
    /* remove all syncable items */
    result = do_keychain_delete_aks_bags();
    secdebug("set to new", "result for deleting aks bags: %d", result);

    result = do_keychain_delete_identities();
    secdebug("set to new", "result for deleting identities: %d", result);
 
    result = do_keychain_delete_lakitu();
    secdebug("set to new", "result for deleting lakitu: %d", result);
    
    result = do_keychain_delete_sbd();
    secdebug("set to new", "result for deleting sbd: %d", result);

    a->user_public_trusted = false;
    a->departure_code = kSOSNeverAppliedToCircle;
    a->user_private_timer = 0;
    a->lock_notification_token = 0;

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

    a->trusted_rings = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
    a->backups = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);

    a->retirees = CFSetCreateMutableForSOSPeerInfosByID(kCFAllocatorDefault);
    a->expansion = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);

    SOSAccountEnsureFactoryCircles(a); // Does rings too

    SOSUpdateKeyInterest(a);
}


static CFStringRef SOSAccountCopyFormatDescription(CFTypeRef aObj, CFDictionaryRef formatOptions) {
    SOSAccountRef a = (SOSAccountRef) aObj;
    
    CFStringRef gestaltDescription = CFDictionaryCopyCompactDescription(a->gestalt);

    CFStringRef result = CFStringCreateWithFormat(NULL, NULL, CFSTR("<SOSAccount@%p: Gestalt: %@ Circle: %@ Me: %@>"), a, gestaltDescription, a->trusted_circle, a->my_identity);

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
        && CFEqualSafe(laccount->trusted_rings, raccount->trusted_rings)
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

static bool SOSAccountThisDeviceCanSyncWithCircle(SOSAccountRef account) {
    bool ok = false;
    __block CFErrorRef error = NULL;

    if (!SOSAccountHasPublicKey(account, &error)) {
        CFReleaseSafe(error);
        return false;
    }
    
    bool hasID = true;
    
    require_action_quiet(account->my_identity, xit,
                         SOSCreateError(kSOSErrorBadFormat, CFSTR("Account identity not set"), NULL, &error));
    
    SOSTransportMessageIDSGetIDSDeviceID(account);
    
    require_action_quiet(account->trusted_circle, xit,
                         SOSCreateError(kSOSErrorBadFormat, CFSTR("Account trusted circle not set"), NULL, &error));
    
    require_action_quiet(hasID, xit,
                         SOSCreateError(kSOSErrorBadFormat, CFSTR("Missing IDS device ID"), NULL, &error));
    ok = SOSCircleHasPeerWithID(account->trusted_circle,
                                SOSPeerInfoGetPeerID(SOSFullPeerInfoGetPeerInfo(account->my_identity)), &error);
xit:
    if (!ok) {
        secerror("sync with device failure: %@", error);
    }
    CFReleaseSafe(error);
    return ok;
}

static bool SOSAccountIsThisPeerIDMe(SOSAccountRef account, CFStringRef peerID) {
    SOSPeerInfoRef mypi = SOSFullPeerInfoGetPeerInfo(account->my_identity);
    CFStringRef myPeerID = SOSPeerInfoGetPeerID(mypi);

    return myPeerID && CFEqualSafe(myPeerID, peerID);
}

bool SOSAccountSyncWithAllPeers(SOSAccountRef account, CFErrorRef *error)
{
    bool result = true;
    __block bool SyncingCompletedOverIDS = true;
    __block bool SyncingCompletedOverKVS = true;
    __block CFErrorRef localError = NULL;
    SOSCircleRef circle  = SOSAccountGetCircle(account, error);
    CFMutableDictionaryRef circleToPeerIDs = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
    CFMutableArrayRef peerIds = CFArrayCreateMutableForCFTypes(kCFAllocatorDefault);
    
    require_action_quiet(SOSAccountThisDeviceCanSyncWithCircle(account), xit,
                         SOSCreateError(kSOSErrorNoCircle, CFSTR("This device cannot sync with circle"),
                                        NULL, &localError));

    SOSCircleForEachValidPeer(circle, account->user_public, ^(SOSPeerInfoRef peer) {
        if (!SOSAccountIsThisPeerIDMe(account, SOSPeerInfoGetPeerID(peer))) {
            if (SOSPeerInfoShouldUseIDSTransport(SOSFullPeerInfoGetPeerInfo(account->my_identity), peer)) {
                secdebug("IDS Transport", "Syncing with IDS capable peers using IDS!");
                CFMutableDictionaryRef circleToIdsId = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
                CFMutableArrayRef ids = CFArrayCreateMutableForCFTypes(kCFAllocatorDefault);
                CFArrayAppendValue(ids, SOSPeerInfoGetPeerID(peer));
                CFDictionaryAddValue(circleToIdsId, SOSCircleGetName(circle), ids);
                SyncingCompletedOverIDS = SOSTransportMessageSyncWithPeers(account->ids_message_transport, circleToIdsId, &localError);
                if(!SyncingCompletedOverIDS){
                    secerror("Failed to sync over IDS, falling back to KVS");
                    SyncingCompletedOverIDS = SOSTransportMessageSyncWithPeers(account->kvs_message_transport, circleToIdsId, &localError);
                }
                CFReleaseNull(circleToIdsId);
            } else {
                CFArrayAppendValue(peerIds, SOSPeerInfoGetPeerID(peer));
            }
        }
    });
    if (CFArrayGetCount(peerIds)) {
        secnotice("KVS", "Syncing with KVS capable peers");
        CFDictionarySetValue(circleToPeerIDs, SOSCircleGetName(circle), peerIds);
        SyncingCompletedOverKVS &= SOSTransportMessageSyncWithPeers(account->kvs_message_transport, circleToPeerIDs, &localError);
    }

    SOSEngineRef engine = SOSTransportMessageGetEngine(account->kvs_message_transport);
    result = SOSEngineSyncWithPeers(engine, account->ids_message_transport, account->kvs_message_transport, &localError);

    result &= ((SyncingCompletedOverIDS) &&
               (SyncingCompletedOverKVS || (CFDictionaryGetCount(circleToPeerIDs) == 0)));

    if (result)
        SetCloudKeychainTraceValueForKey(kCloudKeychainNumberOfTimesSyncedWithPeers, 1);

xit:
    CFReleaseNull(circleToPeerIDs);

    if (!result) {
        secdebug("Account", "Could not sync with all peers: %@", localError);
        // Tell account to update SOSEngine with current trusted peers
        if (isSOSErrorCoded(localError, kSOSErrorPeerNotFound)) {
            secnotice("Account", "Arming account to update SOSEngine with current trusted peers");
            account->circle_rings_retirements_need_attention = true;
        }
        CFErrorPropagate(localError, error);
        localError = NULL;
    }
    CFReleaseNull(peerIds);
    CFReleaseSafe(localError);
    return result;
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
    if(!new_circle) return NULL;

    if (account->retirees) {
        CFSetForEach(account->retirees, ^(const void* value) {
            SOSPeerInfoRef pi = (SOSPeerInfoRef) value;
            if (isSOSPeerInfo(pi)) {
                SOSCircleUpdatePeerInfo(new_circle, pi);
            }
        });
    }

    if(SOSCircleCountPeers(new_circle) == 0) {
        SOSCircleResetToEmpty(new_circle, NULL);
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

bool SOSAccountIsInCircle(SOSAccountRef account, CFErrorRef *error) {
    SOSCCStatus result = SOSAccountGetCircleStatus(account, error);
    
    if (result != kSOSCCInCircle && result != kSOSCCError) {
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

static bool SOSAccountResetCircleToOffering(SOSAccountRef account, SecKeyRef user_key, CFErrorRef *error) {
    bool result = false;

    require(SOSAccountHasCircle(account, error), fail);
    require(SOSAccountEnsureFullPeerAvailable(account, error), fail);
    
    (void) SOSAccountResetAllRings(account, error);
    
    SOSAccountModifyCircle(account, error, ^(SOSCircleRef circle) {
        bool result = false;
        SOSFullPeerInfoRef cloud_identity = NULL;
        CFErrorRef localError = NULL;

        require_quiet(SOSCircleResetToOffering(circle, user_key, account->my_identity, &localError), err_out);

        {
            SOSPeerInfoRef cloud_peer = GenerateNewCloudIdentityPeerInfo(error);
            require_quiet(cloud_peer, err_out);
            cloud_identity = CopyCloudKeychainIdentity(cloud_peer, error);
            CFReleaseNull(cloud_peer);
            require_quiet(cloud_identity, err_out);
        }

        account->departure_code = kSOSNeverLeftCircle;
        require_quiet(SOSAccountAddEscrowToPeerInfo(account, SOSAccountGetMyFullPeerInfo(account), error), err_out);
        require_quiet(SOSCircleRequestAdmission(circle, user_key, cloud_identity, &localError), err_out);
        require_quiet(SOSCircleAcceptRequest(circle, user_key, account->my_identity, SOSFullPeerInfoGetPeerInfo(cloud_identity), &localError), err_out);
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

    result = true;

fail:
    return result;
}


bool SOSAccountResetToOffering(SOSAccountRef account, CFErrorRef* error) {
    SecKeyRef user_key = SOSAccountGetPrivateCredential(account, error);
    if (!user_key)
        return false;

    CFReleaseNull(account->my_identity);

    return user_key && SOSAccountResetCircleToOffering(account, user_key, error);
}

bool SOSAccountResetToEmpty(SOSAccountRef account, CFErrorRef* error) {
    if (!SOSAccountHasPublicKey(account, error))
        return false;
    __block bool result = true;

    result &= SOSAccountResetAllRings(account, error);

    CFReleaseNull(account->my_identity);

    account->departure_code = kSOSWithdrewMembership;
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

bool SOSAccountEnsureBackupStarts(SOSAccountRef account) {
    
    __block bool result = false;
    __block CFErrorRef error = NULL;
    secnotice("backup", "Starting new backups");

    CFDataRef backupKey = SOSPeerInfoV2DictionaryCopyData(SOSAccountGetMyPeerInfo(account), sBackupKeyKey);

    if (CFEqualSafe(backupKey, account->backup_key)){
        CFReleaseNull(backupKey);
        return true;
    }

    if(account->backup_key != NULL){
        require_quiet(SOSBSKBIsGoodBackupPublic(account->backup_key, &error), exit);
        require_quiet(SOSAccountUpdatePeerInfo(account, CFSTR("Backup public key"), &error,
                                               ^bool(SOSFullPeerInfoRef fpi, CFErrorRef *error) {
                                                   return SOSFullPeerInfoUpdateBackupKey(fpi, account->backup_key, error);
                                               }), exit);
        CFErrorRef localError = NULL;
        if (!SOSDeleteV0Keybag(&localError)) {
            secerror("Failed to delete v0 keybag: %@", localError);
        }
        CFReleaseNull(localError);
        
        result = true;

        SOSAccountForEachBackupView(account, ^(const void *value) {
            CFStringRef viewName = (CFStringRef)value;
            result &= SOSAccountStartNewBackup(account, viewName, &error);
        });
    }
    else{
        if(account->backup_key == NULL){
            secerror("account backup key is NULL!");
        }
    }
    
exit:
    if (!result) {
        secnotice("backupkey", "Failed to setup backup public key: %@", error ? (CFTypeRef) error : (CFTypeRef) CFSTR("No error space provided"));
    }
    CFReleaseNull(backupKey);
    return result;
}

//
// MARK: Waiting for in-sync
//

static bool SOSAccountHasBeenInSync(SOSAccountRef account) {
    CFTypeRef unsyncedObject = SOSAccountGetValue(account, kSOSUnsyncedViewsKey, NULL);
    CFSetRef unsynced = asSet(unsyncedObject, NULL);

    return !(unsyncedObject == kCFBooleanTrue || (unsynced && (CFSetGetCount(unsynced) > 0)));
}

static bool SOSAccountUpdateOutOfSyncViews(SOSAccountRef account, CFSetRef viewsInSync) {
    bool notifyOfChange = false;

    SOSCCStatus circleStatus = SOSAccountGetCircleStatus(account, NULL);
    bool inOrApplying = (circleStatus == kSOSCCInCircle) || (circleStatus == kSOSCCRequestPending);

    CFTypeRef unsyncedObject = SOSAccountGetValue(account, kSOSUnsyncedViewsKey, NULL);

    if (!inOrApplying) {
        if (unsyncedObject != NULL) {
            SOSAccountClearValue(account, kSOSUnsyncedViewsKey, NULL);
            secnotice("initial-sync", "in sync, clearing pending");
            notifyOfChange = true;
        }
    } else if (circleStatus == kSOSCCInCircle) {
        __block CFMutableSetRef viewsToSync = CFSetCreateMutableForCFTypes(kCFAllocatorDefault);
        SOSAccountForEachCirclePeerExceptMe(account, ^(SOSPeerInfoRef peer) {
            SOSPeerInfoWithEnabledViewSet(peer, ^(CFSetRef enabled) {
                CFSetUnion(viewsToSync, enabled);
            });
        });

        if (viewsInSync) {
            CFSetSubtract(viewsToSync, viewsInSync);

        }

        if (unsyncedObject == kCFBooleanTrue) {
            if (CFSetGetCount(viewsToSync) == 0) {
                secnotice("initial-sync", "No views to wait for");
                SOSAccountClearValue(account, kSOSUnsyncedViewsKey, NULL);
            } else {
                __block CFSetRef newViews = NULL;
                SOSPeerInfoWithEnabledViewSet(SOSAccountGetMyPeerInfo(account), ^(CFSetRef enabled) {
                    newViews = CFSetCreateIntersection(kCFAllocatorDefault, enabled, viewsToSync);
                });
                secnotice("initial-sync", "Pending views set from True: %@", newViews);
                SOSAccountSetValue(account, kSOSUnsyncedViewsKey, newViews, NULL);
                CFReleaseNull(newViews);
            }
            notifyOfChange = true;
        } else if (isSet(unsyncedObject)) {
            CFSetRef waiting = (CFMutableSetRef) unsyncedObject;
            CFSetRef newViews = CFSetCreateIntersection(kCFAllocatorDefault, waiting, viewsToSync);
            if (!CFEqualSafe(waiting, newViews)) {
                if (CFSetGetCount(newViews) == 0) {
                    secnotice("initial-sync", "No views left to wait for.");
                    SOSAccountClearValue(account, kSOSUnsyncedViewsKey, NULL);
                } else {
                    secnotice("initial-sync", "Pending views updated: %@", newViews);
                    SOSAccountSetValue(account, kSOSUnsyncedViewsKey, newViews, NULL);
                }
                notifyOfChange = true;
            }
            CFReleaseNull(newViews);
        }

        CFReleaseNull(viewsToSync);
    }

    if (notifyOfChange) {
        if(SOSAccountGetValue(account, kSOSUnsyncedViewsKey, NULL) == NULL){
            CFDictionaryRef syncBlocks = account->waitForInitialSync_blocks;
            account->waitForInitialSync_blocks = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);

            CFDictionaryForEach(syncBlocks, ^(const void *key, const void *value) {
                secnotice("updates", "calling in sync block [%@]", key);
                ((SOSAccountWaitForInitialSyncBlock)value)(account);
            });
            
            CFReleaseNull(syncBlocks);
        }
        
        // Make sure we update the engine
        account->circle_rings_retirements_need_attention = true;
    }
    
    return SOSAccountHasBeenInSync(account);
}

static void SOSAccountPeerGotInSync(SOSAccountRef account, CFStringRef peerID) {
    secnotice("initial-sync", "Heard PeerID is in sync: %@", peerID);

    if (account->trusted_circle) {
        SOSPeerInfoRef peer = SOSCircleCopyPeerWithID(account->trusted_circle, peerID, NULL);
        if (peer) {
            CFSetRef views = SOSPeerInfoCopyEnabledViews(peer);
            SOSAccountUpdateOutOfSyncViews(account, views);
            CFReleaseNull(views);
        }
        CFReleaseNull(peer);
    }
}

void SOSAccountCleanupNotificationForAllPeers(SOSAccountRef account) {
    SOSEngineRef engine = SOSDataSourceFactoryGetEngineForDataSourceName(account->factory, SOSCircleGetName(account->trusted_circle), NULL);

    CFDictionaryForEach(account->notification_cleanups, ^(const void *key, const void *value) {
        if (engine) {
            SOSEngineSetSyncCompleteListener(engine, key, NULL);
        }
        dispatch_async(account->queue, value);
    });

    CFDictionaryRemoveAllValues(account->notification_cleanups);
}

static void SOSAccountCleanupNotificationForPeer(SOSAccountRef account, CFStringRef peerID) {
    dispatch_block_t cleanup = CFDictionaryGetValue(account->notification_cleanups, peerID);

    if (cleanup) {
        SOSEngineRef engine = SOSDataSourceFactoryGetEngineForDataSourceName(account->factory, SOSCircleGetName(account->trusted_circle), NULL);

        if (engine) {
            SOSEngineSetSyncCompleteListener(engine, peerID, NULL);
        }

        dispatch_async(account->queue, cleanup);
    }

    CFDictionaryRemoveValue(account->notification_cleanups, peerID);

}

static void SOSAccountRegisterCleanupBlock(SOSAccountRef account, CFStringRef peerID, dispatch_block_t block) {
    dispatch_block_t copy = Block_copy(block);
    CFDictionarySetValue(account->notification_cleanups, peerID, copy);
    CFReleaseNull(copy);
}

void SOSAccountEnsureSyncChecking(SOSAccountRef account) {
    if (CFDictionaryGetCount(account->notification_cleanups) == 0) {
        secnotice("initial-sync", "Setting up notifications to monitor in-sync");
        SOSEngineRef engine = SOSDataSourceFactoryGetEngineForDataSourceName(account->factory, SOSCircleGetName(account->trusted_circle), NULL);

        SOSEngineSetSyncCompleteListenerQueue(engine, account->queue);

        if (engine) {
            SOSAccountForEachCirclePeerExceptMe(account, ^(SOSPeerInfoRef peer) {
                CFStringRef peerID = CFStringCreateCopy(kCFAllocatorDefault, SOSPeerInfoGetPeerID(peer));

                secnotice("initial-sync", "Setting up monitoring for peer: %@", peerID);
                SOSAccountRegisterCleanupBlock(account, peerID, ^{
                    CFReleaseSafe(peerID);
                });

                SOSEngineSetSyncCompleteListener(engine, peerID, ^{
                    SOSAccountPeerGotInSync(account, peerID);
                    SOSAccountCleanupNotificationForPeer(account, peerID);
                    SOSAccountFinishTransaction(account);
                });
            });
        } else {
            secerror("Couldn't find engine to setup notifications!!!");
        }
    }
}

void SOSAccountCancelSyncChecking(SOSAccountRef account) {
    SOSAccountCleanupNotificationForAllPeers(account);
    SOSAccountUpdateOutOfSyncViews(account, NULL);
}

bool SOSAccountCheckHasBeenInSync(SOSAccountRef account) {
    bool hasBeenInSync = false;

    if (!SOSAccountIsInCircle(account, NULL)) {
        SOSAccountCancelSyncChecking(account);
    } else {
        hasBeenInSync = SOSAccountHasBeenInSync(account);
        if (!hasBeenInSync) {
            hasBeenInSync = SOSAccountUpdateOutOfSyncViews(account, NULL);
            if (hasBeenInSync) {
                // Cancel and declare victory

                SOSAccountCancelSyncChecking(account);
            } else {
                // Make sure we're watching in case this is the fist attempt
                SOSAccountEnsureSyncChecking(account);
            }
        }
    }

    return hasBeenInSync;
}

//
// MARK: Joining
//

static bool SOSAccountJoinCircle(SOSAccountRef account, SecKeyRef user_key,
                                bool use_cloud_peer, CFErrorRef* error) {
    __block bool result = false;
    __block SOSFullPeerInfoRef cloud_full_peer = NULL;

    require_action_quiet(account->trusted_circle, fail, SOSCreateErrorWithFormat(kSOSErrorPeerNotFound, NULL, error, NULL, CFSTR("Don't have circle when joining???")));
    require_quiet(SOSAccountEnsureFullPeerAvailable(account, error), fail);

    SOSFullPeerInfoRef myCirclePeer = account->my_identity;

    if (use_cloud_peer) {
        cloud_full_peer = SOSCircleCopyiCloudFullPeerInfoRef(account->trusted_circle, NULL);
    } else {
        SOSAccountSetValue(account, kSOSUnsyncedViewsKey, kCFBooleanTrue, NULL);
    }

    if (SOSCircleCountPeers(account->trusted_circle) == 0) {
        result = SOSAccountResetCircleToOffering(account, user_key, error);
    } else {
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
    }

fail:
    CFReleaseNull(cloud_full_peer);
    return result;
}

static bool SOSAccountJoinCircles_internal(SOSAccountRef account, bool use_cloud_identity, CFErrorRef* error) {
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

    success = SOSAccountJoinCircle(account, user_key, use_cloud_identity, error);

    require_quiet(success, done);
       
    account->departure_code = kSOSNeverLeftCircle;

done:
    return success;
}

bool SOSAccountJoinCircles(SOSAccountRef account, CFErrorRef* error) {
    return SOSAccountJoinCircles_internal(account, false, error);
}

CFStringRef SOSAccountCopyDeviceID(SOSAccountRef account, CFErrorRef *error){
    CFStringRef result = NULL;

    require_action_quiet(account->my_identity, fail, SOSErrorCreate(kSOSErrorPeerNotFound, error, NULL, CFSTR("No peer for me")));

    result = SOSPeerInfoCopyDeviceID(SOSFullPeerInfoGetPeerInfo(account->my_identity));

fail:
    return result;
}

bool SOSAccountSetMyDSID(SOSAccountRef account, CFStringRef IDS, CFErrorRef* error){
    bool result = true;

    if(whichTransportType == kSOSTransportIDS || whichTransportType == kSOSTransportFuture){
        secdebug("IDS Transport", "We are setting our device ID: %@", IDS);
        if(IDS != NULL && (CFStringGetLength(IDS) > 0)){
            require_action_quiet(account->my_identity, fail, SOSErrorCreate(kSOSErrorPeerNotFound, error, NULL, CFSTR("No peer for me")));
            
            result = SOSAccountModifyCircle(account, error, ^bool(SOSCircleRef circle) {
                
                SOSFullPeerInfoUpdateDeviceID(account->my_identity, IDS, error);
                SOSFullPeerInfoUpdateTransportType(account->my_identity, SOSTransportMessageTypeIDS, error);
                SOSFullPeerInfoUpdateTransportPreference(account->my_identity, kCFBooleanTrue, error);
                
                return SOSCircleHasPeer(circle, SOSFullPeerInfoGetPeerInfo(account->my_identity), NULL);
            });
        }
        else
            result = false;
    }
    else{
        secdebug("IDS Transport", "We are setting our device ID: %@", IDS);
        if(IDS != NULL && (CFStringGetLength(IDS) > 0)){
            require_action_quiet(account->my_identity, fail, SOSErrorCreate(kSOSErrorPeerNotFound, error, NULL, CFSTR("No peer for me")));
            
            result = SOSAccountModifyCircle(account, error, ^bool(SOSCircleRef circle) {
                
                SOSFullPeerInfoUpdateDeviceID(account->my_identity, IDS, error);
                SOSFullPeerInfoUpdateTransportType(account->my_identity, SOSTransportMessageTypeKVS, error);
                SOSFullPeerInfoUpdateTransportPreference(account->my_identity, kCFBooleanTrue, error);
                
                return SOSCircleHasPeer(circle, SOSFullPeerInfoGetPeerInfo(account->my_identity), NULL);
            });
        }
        else
            result = false;

    }
   
    SOSCCSyncWithAllPeers();
    
fail:
    return result;
}


bool SOSAccountSendIDSTestMessage(SOSAccountRef account, CFStringRef message, CFErrorRef *error){
    bool result = true;
    if(whichTransportType == kSOSTransportIDS || whichTransportType == kSOSTransportFuture || whichTransportType == kSOSTransportPresent){
        //construct message dictionary, circle -> peerID -> message
        
        CFMutableDictionaryRef circleToPeerMessages = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
        CFMutableDictionaryRef peerToMessage = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
        
        char *messageCharStar;
        asprintf(&messageCharStar, "%d", kIDSSendOneMessage);
        CFStringRef messageString = CFStringCreateWithCString(kCFAllocatorDefault, messageCharStar, kCFStringEncodingUTF8);
        
        CFMutableDictionaryRef mutableDictionary = CFDictionaryCreateMutableForCFTypesWith(kCFAllocatorDefault, messageString, CFSTR("send IDS test message"), NULL);
        
        SOSCircleForEachPeer(account->trusted_circle, ^(SOSPeerInfoRef peer) {
            if(!CFEqualSafe(peer, SOSAccountGetMyPeerInfo(account)))
            CFDictionaryAddValue(peerToMessage, SOSPeerInfoGetPeerID(peer), mutableDictionary);
        });
        
        CFDictionaryAddValue(circleToPeerMessages, SOSCircleGetName(account->trusted_circle), peerToMessage);
        result = SOSTransportMessageSendMessages(account->ids_message_transport, circleToPeerMessages, error);
        
        CFReleaseNull(mutableDictionary);
        CFReleaseNull(peerToMessage);
        CFReleaseNull(circleToPeerMessages);
        CFReleaseNull(messageString);
        free(messageCharStar);
    }
    return result;
}

bool SOSAccountStartPingTest(SOSAccountRef account, CFStringRef message, CFErrorRef *error){
    bool result = false;
    //construct message dictionary, circle -> peerID -> message
    
    if(account->ids_message_transport == NULL)
        account->ids_message_transport = (SOSTransportMessageRef)SOSTransportMessageIDSCreate(account, SOSCircleGetName(account->trusted_circle), error);
    
    require_quiet(account->ids_message_transport, fail);
    CFMutableDictionaryRef circleToPeerMessages = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
    CFMutableDictionaryRef peerToMessage = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
    
    char *messageCharStar;
    asprintf(&messageCharStar, "%d", kIDSStartPingTestMessage);
    CFStringRef messageString = CFStringCreateWithCString(kCFAllocatorDefault, messageCharStar, kCFStringEncodingUTF8);
    
    CFMutableDictionaryRef mutableDictionary = CFDictionaryCreateMutableForCFTypesWith(kCFAllocatorDefault, messageString, CFSTR("send IDS test message"), NULL);
    
    SOSCircleForEachPeer(account->trusted_circle, ^(SOSPeerInfoRef peer) {
        if(CFStringCompare(SOSAccountGetMyPeerID(account), SOSPeerInfoGetPeerID(peer), 0) != 0)
            CFDictionaryAddValue(peerToMessage, SOSPeerInfoGetPeerID(peer), mutableDictionary);
    });
    
    CFDictionaryAddValue(circleToPeerMessages, SOSCircleGetName(account->trusted_circle), peerToMessage);
    result = SOSTransportMessageSendMessages(account->ids_message_transport, circleToPeerMessages, error);
    
    CFReleaseNull(mutableDictionary);
    CFReleaseNull(peerToMessage);
    CFReleaseNull(circleToPeerMessages);
    CFReleaseNull(messageString);
    free(messageCharStar);
fail:
    return result;
}

bool SOSAccountRetrieveDeviceIDFromIDSKeychainSyncingProxy(SOSAccountRef account, CFErrorRef *error){
    bool result = true;
    if(whichTransportType == kSOSTransportIDS || whichTransportType == kSOSTransportFuture || whichTransportType == kSOSTransportPresent){
        
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
            secerror("Could not ask IDSKeychainSyncingProxy for Device ID: %@", localError);
            *error = localError;
        }
        else{
            secdebug("IDS Transport", "Attempting to retrieve the IDS Device ID");
        }
    }
    return result;
}

bool SOSAccountJoinCirclesAfterRestore(SOSAccountRef account, CFErrorRef* error) {
    return SOSAccountJoinCircles_internal(account, true, error);
}


bool SOSAccountLeaveCircle(SOSAccountRef account, CFErrorRef* error)
{
    bool result = true;

    result &= SOSAccountModifyCircle(account, error, ^(SOSCircleRef circle) {
        return sosAccountLeaveCircle(account, circle, error);
    });

    account->departure_code = kSOSWithdrewMembership;

    return result;
}

bool SOSAccountRemovePeersFromCircle(SOSAccountRef account, CFArrayRef peers, CFErrorRef* error)
{
    SecKeyRef user_key = SOSAccountGetPrivateCredential(account, error);
    if (!user_key)
        return false;

    bool result = true;

    CFMutableSetRef peersToRemove = CFSetCreateMutableForSOSPeerInfosByIDWithArray(kCFAllocatorDefault, peers);

    bool leaveCircle = CFSetContainsValue(peersToRemove, SOSAccountGetMyPeerInfo(account));

    CFSetRemoveValue(peersToRemove, SOSAccountGetMyPeerInfo(account));

    result &= SOSAccountModifyCircle(account, error, ^(SOSCircleRef circle) {
        bool success = false;

        if(CFSetGetCount(peersToRemove) != 0) {
            require_quiet(SOSCircleRemovePeers(circle, user_key, SOSAccountGetMyFullPeerInfo(account), peersToRemove, error), done);
            success = SOSAccountGenerationSignatureUpdate(account, error);
        } else success = true;

        if (success && leaveCircle) {
            success = sosAccountLeaveCircle(account, circle, error);
        }

    done:
        return success;

    });

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
    // If we are not in the circle, there is no point in setting up peers
    require_quiet(SOSAccountIsMyPeerActive(account, NULL), done);

    // This code only uses the SOSFullPeerInfoRef for two things:
    //  - Finding out if this device is in the trusted circle
    //  - Using the peerID for this device to see if the current peer is "me"
    //  - It is used indirectly by passing account->my_identity to SOSPeerCoderInitializeForPeer
    
    CFStringRef my_id = SOSPeerInfoGetPeerID(SOSFullPeerInfoGetPeerInfo(account->my_identity));

    SOSCircleForEachPeer(account->trusted_circle, ^(SOSPeerInfoRef peer) {
        if (!SOSPeerInfoPeerIDEqual(peer, my_id)) {
            CFErrorRef localError = NULL;
            SOSTransportMessageRef messageTransport = NULL;
            
            if(whichTransportType == kSOSTransportIDS || whichTransportType == kSOSTransportFuture || whichTransportType == kSOSTransportPresent){
                 messageTransport = SOSPeerInfoHasDeviceID(peer) ? account->ids_message_transport : account->kvs_message_transport;
            }
            else
                messageTransport = account->kvs_message_transport;
            
            SOSPeerCoderInitializeForPeer(messageTransport->engine, account->my_identity, peer, &localError);
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

static inline bool SOSAccountEnsureExpansion(SOSAccountRef account, CFErrorRef *error) {
    if (!account->expansion) {
        account->expansion = CFDictionaryCreateMutableForCFTypes(NULL);
    }

    return SecAllocationError(account->expansion, error, CFSTR("Can't Alloc Account Expansion dictionary"));
}

bool SOSAccountClearValue(SOSAccountRef account, const void *key, CFErrorRef *error) {
    bool success = SOSAccountEnsureExpansion(account, error);
    require_quiet(success, errOut);

    CFDictionaryRemoveValue(account->expansion, key);
errOut:
    return success;
}

bool SOSAccountSetValue(SOSAccountRef account, const void *key, const void *value, CFErrorRef *error) {
    bool success = SOSAccountEnsureExpansion(account, error);
    require_quiet(success, errOut);

    CFDictionarySetValue(account->expansion, key, value);
errOut:
    return success;
}


const void *SOSAccountGetValue(SOSAccountRef account, const void *key, CFErrorRef *error) {
    if (!account->expansion) {
        return NULL;
    }
    return CFDictionaryGetValue(account->expansion, key);
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
    CFMutableDictionaryRef circleToPeerMessages = NULL;
    CFStringRef messageString = NULL;
    CFMutableDictionaryRef mutableDictionary = NULL;
    CFMutableSetRef peers = NULL;
    CFMutableDictionaryRef peerList = NULL;
    char* message = NULL;
    bool result = false;
    if(account->ids_message_transport == NULL)
        account->ids_message_transport = (SOSTransportMessageRef)SOSTransportMessageIDSCreate(account, SOSCircleGetName(account->trusted_circle), error);
    
    require_quiet(account->ids_message_transport, fail);
    circleToPeerMessages = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
    
    //adding message type kIDSPeerAvailability so IDSKeychainSyncingProxy does not send this message as a keychain item
  
    asprintf(&message, "%d", kIDSPeerAvailability);
    messageString = CFStringCreateWithCString(kCFAllocatorDefault, message, kCFStringEncodingUTF8);
    
    mutableDictionary = CFDictionaryCreateMutableForCFTypesWith(kCFAllocatorDefault, messageString, CFSTR("checking peers"), NULL);
    
    //make sure there are peers in the circle
    peers = SOSCircleCopyPeers(account->trusted_circle, kCFAllocatorDefault);
    require_quiet(CFSetGetCount(peers) > 0, fail);
    CFReleaseNull(peers);
    
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
                    CFDictionaryAddValue(peerList, SOSPeerInfoGetPeerID(peer), mutableDictionary);
                CFReleaseNull(deviceID);
            }
            CFReleaseNull(peerViews);
            CFReleaseNull(intersectSets);
        }
    });
        
    require_quiet(CFDictionaryGetCount(peerList) > 0 , fail);
    CFDictionaryAddValue(circleToPeerMessages, SOSCircleGetName(account->trusted_circle), peerList);
    result = SOSTransportMessageSendMessages(account->ids_message_transport, circleToPeerMessages, error);
   
fail:
    CFReleaseNull(mutableDictionary);
    CFReleaseNull(messageString);
    CFReleaseNull(peerList);
    CFReleaseNull(circleToPeerMessages);
    CFReleaseNull(peers);
    free(message);
    return result;
}


static void SOSAccountRecordRetiredPeersInCircle(SOSAccountRef account) {
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

void SOSAccountFinishTransaction(SOSAccountRef account) {
    if(account->circle_rings_retirements_need_attention){
        SOSAccountRecordRetiredPeersInCircle(account);

        CFErrorRef localError = NULL;
        if(!SOSTransportCircleFlushChanges(account->circle_transport, &localError)) {
            secerror("flush circle failed %@", localError);
        }
        CFReleaseSafe(localError);
        
        SOSAccountNotifyEngines(account); // For now our only rings are backup rings.
    }
    
    SOSAccountCheckHasBeenInSync(account);

    account->circle_rings_retirements_need_attention = false;
}


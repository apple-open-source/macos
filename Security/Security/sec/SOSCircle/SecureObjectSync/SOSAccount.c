/*
 * Copyright (c) 2012-2014 Apple Inc. All Rights Reserved.
 */

/*
 * SOSAccount.c -  Implementation of the secure object syncing account.
 * An account contains a SOSCircle for each protection domain synced.
 */

#include "SOSAccountPriv.h"
#include <SecureObjectSync/SOSPeerInfoCollections.h>
#include <SecureObjectSync/SOSTransportCircle.h>
#include <SecureObjectSync/SOSTransportMessage.h>
#include <SecureObjectSync/SOSKVSKeys.h>
#include <SecureObjectSync/SOSTransportKeyParameter.h>
#include <SecureObjectSync/SOSTransportKeyParameterKVS.h>
#include <SecureObjectSync/SOSEngine.h>
#include <SecureObjectSync/SOSPeerCoder.h>

CFGiblisWithCompareFor(SOSAccount);


bool SOSAccountEnsureFactoryCircles(SOSAccountRef a)
{
    bool result = false;
    if (a)
    {
        require(a->factory, xit);
        CFArrayRef circle_names = a->factory->copy_names(a->factory);
        require(circle_names, xit);
        CFArrayForEach(circle_names, ^(const void*name) {
            if (isString(name))
                SOSAccountEnsureCircle(a, (CFStringRef)name, NULL);
        });

        CFReleaseNull(circle_names);
        result = true;
    }
xit:
    return result;
}


SOSAccountRef SOSAccountCreateBasic(CFAllocatorRef allocator,
                                           CFDictionaryRef gestalt,
                                           SOSDataSourceFactoryRef factory) {
    SOSAccountRef a = CFTypeAllocate(SOSAccount, struct __OpaqueSOSAccount, allocator);

    a->queue = dispatch_queue_create("Account Queue", DISPATCH_QUEUE_SERIAL);

    a->gestalt = CFRetainSafe(gestalt);

    a->circles = CFDictionaryCreateMutableForCFTypes(allocator);
    a->circle_identities = CFDictionaryCreateMutableForCFTypes(allocator);

    a->factory = factory; // We adopt the factory. kthanksbai.

    a->change_blocks = CFArrayCreateMutableForCFTypes(allocator);

    a->departure_code = kSOSNeverAppliedToCircle;

    a->key_transport = (SOSTransportKeyParameterRef)SOSTransportKeyParameterKVSCreate(a, NULL);
    a->circle_transports = CFDictionaryCreateMutableForCFTypes(allocator);
    a->message_transports = CFDictionaryCreateMutableForCFTypes(allocator);

    return a;
}




bool SOSAccountUpdateGestalt(SOSAccountRef account, CFDictionaryRef new_gestalt)
{
    if (CFEqual(new_gestalt, account->gestalt))
        return false;
    SOSAccountForEachKnownCircle(account, NULL, NULL, ^(SOSCircleRef circle, SOSFullPeerInfoRef full_peer) {
        if (SOSFullPeerInfoUpdateGestalt(full_peer, new_gestalt, NULL)) {
            SOSAccountModifyCircle(account, SOSCircleGetName(circle),
                                   NULL, ^(SOSCircleRef circle_to_change) {
                                       secnotice("circleChange", "Calling SOSCircleUpdatePeerInfo for gestalt change");
                                       return SOSCircleUpdatePeerInfo(circle_to_change, SOSFullPeerInfoGetPeerInfo(full_peer));
                                   });
        };
    });

    CFRetainAssign(account->gestalt, new_gestalt);
    return true;
}

SOSAccountRef SOSAccountCreate(CFAllocatorRef allocator,
                               CFDictionaryRef gestalt,
                               SOSDataSourceFactoryRef factory) {
    SOSAccountRef a = SOSAccountCreateBasic(allocator, gestalt, factory);

    a->retired_peers = CFDictionaryCreateMutableForCFTypes(allocator);

    SOSAccountEnsureFactoryCircles(a);

    return a;
}

static void SOSAccountDestroy(CFTypeRef aObj) {
    SOSAccountRef a = (SOSAccountRef) aObj;

    // We don't own the factory, meerly have a reference to the singleton
    //    don't free it.
    //   a->factory
    
    CFReleaseNull(a->gestalt);
    CFReleaseNull(a->circle_identities);
    CFReleaseNull(a->circles);
    CFReleaseNull(a->retired_peers);

    a->user_public_trusted = false;
    CFReleaseNull(a->user_public);
    CFReleaseNull(a->user_key_parameters);

    SOSAccountPurgePrivateCredential(a);
    CFReleaseNull(a->previous_public);

    a->departure_code = kSOSNeverAppliedToCircle;
    CFReleaseNull(a->message_transports);
    CFReleaseNull(a->key_transport);
    CFReleaseNull(a->circle_transports);
    dispatch_release(a->queue);
}

void SOSAccountSetToNew(SOSAccountRef a) {
    secnotice("accountChange", "Setting Account to New");
    CFAllocatorRef allocator = CFGetAllocator(a);
    CFReleaseNull(a->circle_identities);
    CFReleaseNull(a->circles);
    CFReleaseNull(a->retired_peers);

    CFReleaseNull(a->user_key_parameters);
    CFReleaseNull(a->user_public);
    CFReleaseNull(a->previous_public);
    CFReleaseNull(a->_user_private);
    
    CFReleaseNull(a->key_transport);
    CFReleaseNull(a->circle_transports);
    CFReleaseNull(a->message_transports);
    
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
    
    a->circles = CFDictionaryCreateMutableForCFTypes(allocator);
    a->circle_identities = CFDictionaryCreateMutableForCFTypes(allocator);
    a->retired_peers = CFDictionaryCreateMutableForCFTypes(allocator);
    
    a->key_transport = (SOSTransportKeyParameterRef)SOSTransportKeyParameterKVSCreate(a, NULL);
    a->circle_transports = (CFMutableDictionaryRef)CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
    a->message_transports = (CFMutableDictionaryRef)CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
    
    SOSAccountEnsureFactoryCircles(a);
}


static CFStringRef SOSAccountCopyFormatDescription(CFTypeRef aObj, CFDictionaryRef formatOptions) {
    SOSAccountRef a = (SOSAccountRef) aObj;
    
    return CFStringCreateWithFormat(NULL, NULL, CFSTR("<SOSAccount@%p: Gestalt: %@\n Circles: %@ CircleIDs: %@>"), a, a->gestalt, a->circles, a->circle_identities);
}

static Boolean SOSAccountCompare(CFTypeRef lhs, CFTypeRef rhs)
{
    SOSAccountRef laccount = (SOSAccountRef) lhs;
    SOSAccountRef raccount = (SOSAccountRef) rhs;

    return CFEqual(laccount->gestalt, raccount->gestalt)
        && CFEqual(laccount->circles, raccount->circles)
        && CFEqual(laccount->circle_identities, raccount->circle_identities);
    //  ??? retired_peers
}

dispatch_queue_t SOSAccountGetQueue(SOSAccountRef account) {
    return account->queue;
}

CFDictionaryRef SOSAccountGetMessageTransports(SOSAccountRef account){
    return account->message_transports;
}


void SOSAccountSetUserPublicTrustedForTesting(SOSAccountRef account){
    account->user_public_trusted = true;
}

CFArrayRef SOSAccountCopyAccountIdentityPeerInfos(SOSAccountRef account, CFAllocatorRef allocator, CFErrorRef* error)
{
    CFMutableArrayRef result = CFArrayCreateMutableForCFTypes(allocator);

    CFDictionaryForEach(account->circle_identities, ^(const void *key, const void *value) {
        SOSFullPeerInfoRef fpi = (SOSFullPeerInfoRef) value;
        
        CFArrayAppendValue(result, SOSFullPeerInfoGetPeerInfo(fpi));
    });
    
    return result;
}

static bool SOSAccountThisDeviceCanSyncWithCircle(SOSAccountRef account, SOSCircleRef circle) {
    CFErrorRef error = NULL;

    if (!SOSAccountHasPublicKey(account, &error))
        return false;

    SOSFullPeerInfoRef myfpi = SOSAccountGetMyFullPeerInCircleNamedIfPresent(account, SOSCircleGetName(circle), &error);
    SOSPeerInfoRef mypi = SOSFullPeerInfoGetPeerInfo(myfpi);
    CFStringRef myPeerID = SOSPeerInfoGetPeerID(mypi);
    return SOSCircleHasPeerWithID(circle, myPeerID, &error);
}

static bool SOSAccountIsThisPeerIDMe(SOSAccountRef account, CFStringRef circleName, CFStringRef peerID) {
    CFErrorRef error = NULL;
    SOSFullPeerInfoRef myfpi = SOSAccountGetMyFullPeerInCircleNamedIfPresent(account, circleName, &error);
    if (!myfpi) {
        return false;
    }
    SOSPeerInfoRef mypi = SOSFullPeerInfoGetPeerInfo(myfpi);
    CFStringRef myPeerID = SOSPeerInfoGetPeerID(mypi);
    return CFEqualSafe(myPeerID, peerID);
}

bool SOSAccountSyncWithAllPeers(SOSAccountRef account, CFErrorRef *error)
{
    __block bool result = true;
    
    SOSAccountForEachCircle(account, ^(SOSCircleRef circle) {
        
        if (SOSAccountThisDeviceCanSyncWithCircle(account, circle)) {
            CFStringRef circleName = SOSCircleGetName(circle);
            SOSTransportMessageRef thisPeerTransport = (SOSTransportMessageRef)CFDictionaryGetValue(account->message_transports, SOSCircleGetName(circle));
;
            CFMutableDictionaryRef circleToPeerIDs = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);

            SOSCircleForEachPeer(circle, ^(SOSPeerInfoRef peer) {
                // Figure out transport for peer; for now we always use KVS
                CFStringRef peerID = SOSPeerInfoGetPeerID(peer);
                if (!SOSAccountIsThisPeerIDMe(account, circleName, peerID)) {
                    CFArrayAppendValue(CFDictionaryEnsureCFArrayAndGetCurrentValue(circleToPeerIDs, circleName), peerID);
                }
            });
            
            result &= SOSTransportMessageSyncWithPeers(thisPeerTransport, circleToPeerIDs, error);
            
            CFReleaseNull(circleToPeerIDs);
        }
    });
    
    // Tell each transport to sync with its collection of peers we know we should sync with.


    if (result)
        SetCloudKeychainTraceValueForKey(kCloudKeychainNumberOfTimesSyncedWithPeers, 1);

    
    return result;
}

bool SOSAccountCleanupAfterPeer(SOSAccountRef account, size_t seconds, SOSCircleRef circle,
                                SOSPeerInfoRef cleanupPeer, CFErrorRef* error)
{
    bool success = false;
    if(!SOSAccountIsMyPeerActiveInCircle(account, circle, NULL)) return true;
    
    SOSPeerInfoRef myPeerInfo = SOSAccountGetMyPeerInCircle(account, circle, error);
    require(myPeerInfo, xit);
    
    CFStringRef cleanupPeerID = SOSPeerInfoGetPeerID(cleanupPeer);
    CFStringRef circle_name = SOSCircleGetName(circle);
    
    if (CFEqual(cleanupPeerID, SOSPeerInfoGetPeerID(myPeerInfo))) {
        CFErrorRef destroyError = NULL;
        if (!SOSAccountDestroyCirclePeerInfo(account, circle, &destroyError)) {
            secerror("Unable to destroy peer info: %@", destroyError);
        }
        CFReleaseSafe(destroyError);
        
        account->departure_code = kSOSWithdrewMembership;
        
        return true;
    }
    
    CFMutableDictionaryRef circleToPeerIDs = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
    CFArrayAppendValue(CFDictionaryEnsureCFArrayAndGetCurrentValue(circleToPeerIDs, circle_name), cleanupPeerID);

    
    CFErrorRef localError = NULL;
    SOSTransportMessageRef tMessage = (SOSTransportMessageRef)CFDictionaryGetValue(account->message_transports, SOSCircleGetName(circle));
    if (!SOSTransportMessageCleanupAfterPeerMessages(tMessage, circleToPeerIDs, &localError)) {
        secnotice("account", "Failed to cleanup after peer %@ messages: %@", cleanupPeerID, localError);
    }
    CFReleaseNull(localError);
    SOSTransportCircleRef tCircle = (SOSTransportCircleRef)CFDictionaryGetValue(account->circle_transports, SOSCircleGetName(circle));
    if(SOSPeerInfoRetireRetirementTicket(seconds, cleanupPeer)) {
        if (!SOSTransportCircleExpireRetirementRecords(tCircle, circleToPeerIDs, &localError)) {
            secnotice("account", "Failed to cleanup after peer %@ retirement: %@", cleanupPeerID, localError);
        }
    }
    CFReleaseNull(localError);
    
    CFReleaseNull(circleToPeerIDs);
    
xit:
    return success;
}

bool SOSAccountCleanupRetirementTickets(SOSAccountRef account, size_t seconds, CFErrorRef* error) {
    CFMutableDictionaryRef retirements_to_remove = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
    CFDictionaryRef original_retired_peers = account->retired_peers;
    __block bool success = true;
    account->retired_peers = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);

    CFDictionaryForEach(original_retired_peers, ^(const void *key, const void *value) {
        if (isString(key) && isDictionary(value)) {
            CFStringRef circle_name = key;
            __block CFMutableDictionaryRef still_active_circle_retirements = NULL;
            CFDictionaryForEach((CFMutableDictionaryRef) value, ^(const void *key, const void *value) {
                if (isString(key) && isData(value)) {
                    CFStringRef retired_peer_id = (CFStringRef) key;
                    SOSPeerInfoRef retired_peer = SOSPeerInfoCreateFromData(kCFAllocatorDefault, NULL, (CFDataRef) value);
                    if (retired_peer && SOSPeerInfoIsRetirementTicket(retired_peer) && CFEqual(retired_peer_id, SOSPeerInfoGetPeerID(retired_peer))) {
                        // He's a retired peer all right, if he's active or not yet expired we keep a record of his retirement.
                        // if not, clear any recordings of his retirement from our transport.
                        if (SOSAccountIsActivePeerInCircleNamed(account, circle_name, retired_peer_id, NULL) ||
                            !SOSPeerInfoRetireRetirementTicket(seconds, retired_peer)) {
                            // He's still around or not expired. Keep record.
                            if (still_active_circle_retirements == NULL) {
                                still_active_circle_retirements = CFDictionaryEnsureCFDictionaryAndGetCurrentValue(account->retired_peers, circle_name);
                            }
                            CFDictionarySetValue(still_active_circle_retirements, retired_peer_id, value);
                        } else {
                            CFMutableArrayRef retirements = CFDictionaryEnsureCFArrayAndGetCurrentValue(retirements_to_remove, circle_name);
                            CFArrayAppendValue(retirements, retired_peer_id);
                        }
                    }
                    CFReleaseNull(retired_peer);
                }
            });
            
            SOSTransportCircleRef tCircle = (SOSTransportCircleRef)CFDictionaryGetValue(account->circle_transports, circle_name);
            success &= SOSTransportCircleExpireRetirementRecords(tCircle, retirements_to_remove, error);
        }
    });

    CFReleaseNull(original_retired_peers);
    CFReleaseNull(retirements_to_remove);

    return success;
}

bool SOSAccountScanForRetired(SOSAccountRef account, SOSCircleRef circle, CFErrorRef *error) {
    __block CFMutableDictionaryRef circle_retirees = (CFMutableDictionaryRef) CFDictionaryGetValue(account->retired_peers, SOSCircleGetName(circle));

    SOSCircleForEachRetiredPeer(circle, ^(SOSPeerInfoRef peer) {
        CFStringRef peer_id = SOSPeerInfoGetPeerID(peer);
        if(!circle_retirees || !CFDictionaryGetValueIfPresent(circle_retirees, peer_id, NULL)) {
            if (!circle_retirees) {
                circle_retirees = CFDictionaryEnsureCFDictionaryAndGetCurrentValue(account->retired_peers, SOSCircleGetName(circle));
            }
            CFDataRef value = SOSPeerInfoCopyEncodedData(peer, NULL, NULL);
            if(value) {
                CFDictionarySetValue(circle_retirees, peer_id, value);
                SOSAccountCleanupAfterPeer(account, RETIREMENT_FINALIZATION_SECONDS, circle, peer, error);
            }
            CFReleaseSafe(value);
        }
    });
    return true;
}

SOSCircleRef SOSAccountCloneCircleWithRetirement(SOSAccountRef account, SOSCircleRef starting_circle, CFErrorRef *error) {
    CFStringRef circle_to_mod = SOSCircleGetName(starting_circle);

    SOSCircleRef new_circle = SOSCircleCopyCircle(NULL, starting_circle, error);
    if(!new_circle) return NULL;

    CFDictionaryRef circle_retirements = CFDictionaryGetValue(account->retired_peers, circle_to_mod);

    if (isDictionary(circle_retirements)) {
        CFDictionaryForEach(circle_retirements, ^(const void* id, const void* value) {
            if (isData(value)) {
                SOSPeerInfoRef pi = SOSPeerInfoCreateFromData(NULL, error, (CFDataRef) value);
                if (pi && CFEqualSafe(id, SOSPeerInfoGetPeerID(pi))) {
                    SOSCircleUpdatePeerInfo(new_circle, pi);
                }
                CFReleaseSafe(pi);
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
    CFArrayAppendValue(a->change_blocks, changeBlock);
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

        SOSPeerInfoRef myPi = SOSAccountGetMyPeerInCircle(a, new_circle, NULL);
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
    SOSAccountAddChangeBlock(a, Block_copy(block_to_register));

    CFSetRef empty = CFSetCreateMutableForSOSPeerInfosByID(kCFAllocatorDefault);
    SOSCircleRef circle = (SOSCircleRef) CFDictionaryGetValue(a->circles, ds_name);
    if (circle) {
        block_to_register(circle, empty, empty, empty, empty);
    }
    CFReleaseSafe(empty);
}


bool sosAccountLeaveCircle(SOSAccountRef account, SOSCircleRef circle, CFErrorRef* error) {
    SOSFullPeerInfoRef fpi = SOSAccountGetMyFullPeerInCircle(account, circle, NULL);
    if(!fpi) return false;
    if(!SOSFullPeerInfoValidate(fpi, NULL)) return false;
	CFErrorRef localError = NULL;
	SOSPeerInfoRef retire_peer = SOSFullPeerInfoPromoteToRetiredAndCopy(fpi, &localError);
    CFStringRef retire_id = SOSPeerInfoGetPeerID(retire_peer);
    
    // Account should move away from a dictionary of KVS keys to a Circle -> Peer -> Retirement ticket storage soonish.
	CFStringRef retire_key = SOSRetirementKeyCreateWithCircleAndPeer(circle, retire_id);
	CFDataRef retire_value = NULL;
    bool retval = false;
    bool writeCircle = false;
    
    // Create a Retirement Ticket and store it in the retired_peers of the account.
    require_action_quiet(retire_peer, errout, secerror("Create ticket failed for peer %@: %@", fpi, localError));
	retire_value = SOSPeerInfoCopyEncodedData(retire_peer, NULL, &localError);
    require_action_quiet(retire_value, errout, secerror("Failed to encode retirement peer %@: %@", retire_peer, localError));

    // See if we need to repost the circle we could either be an applicant or a peer already in the circle
    if(SOSCircleHasApplicant(circle, retire_peer, NULL)) {
	    // Remove our application if we have one.
	    SOSCircleWithdrawRequest(circle, retire_peer, NULL);
        writeCircle = true;
    } else if (SOSCircleHasPeer(circle, retire_peer, NULL)) {
        if (SOSCircleUpdatePeerInfo(circle, retire_peer)) {
            CFErrorRef cleanupError = NULL;
            if (!SOSAccountCleanupAfterPeer(account, RETIREMENT_FINALIZATION_SECONDS, circle, retire_peer, &cleanupError))
                secerror("Error cleanup up after peer (%@): %@", retire_peer, cleanupError);
            CFReleaseSafe(cleanupError);
        }
        writeCircle = true;
    }
    
    // Store the retirement record locally.
    CFDictionarySetValue(account->retired_peers, retire_key, retire_value);

    // Write retirement to Transport
    SOSTransportCircleRef tCircle = (SOSTransportCircleRef)CFDictionaryGetValue(account->circle_transports, SOSCircleGetName(circle));
    SOSTransportCirclePostRetirement(tCircle, SOSCircleGetName(circle), retire_id, retire_value, NULL); // TODO: Handle errors?
    
    // Kill peer key but don't return error if we can't.
    if(!SOSAccountDestroyCirclePeerInfo(account, circle, &localError))
        secerror("Couldn't purge key for peer %@ on retirement: %@", fpi, localError);

    if (writeCircle) {
        CFDataRef circle_data = SOSCircleCopyEncodedData(circle, kCFAllocatorDefault, error);
        
        if (circle_data) {
            SOSTransportCirclePostCircle(tCircle, SOSCircleGetName(circle), circle_data, NULL); // TODO: Handle errors?
        }
        CFReleaseNull(circle_data);
    }
    retval = true;

errout:
    CFReleaseNull(localError);
    CFReleaseNull(retire_peer);
    CFReleaseNull(retire_key);
    CFReleaseNull(retire_value);
    return retval;
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

static SOSCCStatus SOSCCCircleStatus(SOSCircleRef circle) {
    if (SOSCircleCountPeers(circle) == 0)
        return kSOSCCCircleAbsent;

    return kSOSCCNotInCircle;
}

static SOSCCStatus SOSCCThisDeviceStatusInCircle(SOSCircleRef circle, SOSPeerInfoRef this_peer) {
    if (SOSCircleCountPeers(circle) == 0)
        return kSOSCCCircleAbsent;

    if (SOSCircleHasPeer(circle, this_peer, NULL))
        return kSOSCCInCircle;

    if (SOSCircleHasApplicant(circle, this_peer, NULL))
        return kSOSCCRequestPending;

    return kSOSCCNotInCircle;
}

static SOSCCStatus UnionStatus(SOSCCStatus accumulated_status, SOSCCStatus additional_circle_status) {
    switch (additional_circle_status) {
        case kSOSCCInCircle:
            return accumulated_status;
        case kSOSCCRequestPending:
            return (accumulated_status == kSOSCCInCircle) ?
            kSOSCCRequestPending :
            accumulated_status;
        case kSOSCCNotInCircle:
            return (accumulated_status == kSOSCCInCircle ||
                    accumulated_status == kSOSCCRequestPending) ?
            kSOSCCNotInCircle :
            accumulated_status;
        case kSOSCCCircleAbsent:
            return (accumulated_status == kSOSCCInCircle ||
                    accumulated_status == kSOSCCRequestPending ||
                    accumulated_status == kSOSCCNotInCircle) ?
            kSOSCCCircleAbsent :
            accumulated_status;
        default:
            return additional_circle_status;
    };

}

SOSCCStatus SOSAccountIsInCircles(SOSAccountRef account, CFErrorRef* error) {
    if (!SOSAccountHasPublicKey(account, error)) {
        return kSOSCCError;
    }

    __block bool set_once = false;
    __block SOSCCStatus status = kSOSCCInCircle;

    SOSAccountForEachKnownCircle(account, ^(CFStringRef name) {
        set_once = true;
        status = UnionStatus(status, kSOSCCNotInCircle);
    }, ^(SOSCircleRef circle) {
        set_once = true;
        status = UnionStatus(status, SOSCCCircleStatus(circle));
    }, ^(SOSCircleRef circle, SOSFullPeerInfoRef full_peer) {
        set_once = true;
        SOSCCStatus circle_status = SOSCCThisDeviceStatusInCircle(circle, SOSFullPeerInfoGetPeerInfo(full_peer));
        status = UnionStatus(status, circle_status);
    });

    if (!set_once)
        status = kSOSCCCircleAbsent;

    return status;
}

//
// MARK: Account Reset Circles
//

static bool SOSAccountResetThisCircleToOffering(SOSAccountRef account, SOSCircleRef circle, SecKeyRef user_key, CFErrorRef *error) {
    SOSFullPeerInfoRef myCirclePeer = SOSAccountMakeMyFullPeerInCircleNamed(account, SOSCircleGetName(circle), error);
    if (!myCirclePeer)
        return false;
    if(!SOSFullPeerInfoValidate(myCirclePeer, NULL)) return false;

    
    SOSAccountModifyCircle(account, SOSCircleGetName(circle), error, ^(SOSCircleRef circle) {
        bool result = false;
        SOSFullPeerInfoRef cloud_identity = NULL;
        CFErrorRef localError = NULL;

        require_quiet(SOSCircleResetToOffering(circle, user_key, myCirclePeer, &localError), err_out);

        {
            SOSPeerInfoRef cloud_peer = GenerateNewCloudIdentityPeerInfo(error);
            require_quiet(cloud_peer, err_out);
            cloud_identity = CopyCloudKeychainIdentity(cloud_peer, error);
            CFReleaseNull(cloud_peer);
            require_quiet(cloud_identity, err_out);
        }

        account->departure_code = kSOSNeverLeftCircle;
        require_quiet(SOSCircleRequestAdmission(circle, user_key, cloud_identity, &localError), err_out);
        require_quiet(SOSCircleAcceptRequest(circle, user_key, myCirclePeer, SOSFullPeerInfoGetPeerInfo(cloud_identity), &localError), err_out);
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

    return true;
}


bool SOSAccountResetToOffering(SOSAccountRef account, CFErrorRef* error) {
    SecKeyRef user_key = SOSAccountGetPrivateCredential(account, error);
    if (!user_key)
        return false;
    
    __block bool result = true;
    
    SOSAccountForEachKnownCircle(account, ^(CFStringRef name) {
        SOSCircleRef circle = SOSCircleCreate(NULL, name, NULL);
        if (circle)
            CFDictionaryAddValue(account->circles, name, circle);
        
        SOSAccountResetThisCircleToOffering(account, circle, user_key, error);
    }, ^(SOSCircleRef circle) {
        SOSAccountResetThisCircleToOffering(account, circle, user_key, error);
    }, ^(SOSCircleRef circle, SOSFullPeerInfoRef full_peer) {
        SOSAccountResetThisCircleToOffering(account, circle, user_key, error);
    });
    
    return result;
}

bool SOSAccountResetToEmpty(SOSAccountRef account, CFErrorRef* error) {
    if (!SOSAccountHasPublicKey(account, error))
        return false;
    
    __block bool result = true;
    SOSAccountForEachCircle(account, ^(SOSCircleRef circle) {
        SOSAccountModifyCircle(account, SOSCircleGetName(circle), error, ^(SOSCircleRef circle) {
            if (!SOSCircleResetToEmpty(circle, error))
            {
                secerror("error: %@", *error);
                result = false;
            }
            account->departure_code = kSOSWithdrewMembership;
            return result;
        });
    });
    
    return result;
}


//
// MARK: Joining
//

static bool SOSAccountJoinThisCircle(SOSAccountRef account, SecKeyRef user_key,
                                     SOSCircleRef circle, bool use_cloud_peer, CFErrorRef* error) {
    __block bool result = false;
    __block SOSFullPeerInfoRef cloud_full_peer = NULL;

    SOSFullPeerInfoRef myCirclePeer = SOSAccountMakeMyFullPeerInCircleNamed(account, SOSCircleGetName(circle), error);
    
    require_action_quiet(myCirclePeer, fail,
                         SOSCreateErrorWithFormat(kSOSErrorPeerNotFound, NULL, error, NULL, CFSTR("Can't find/create peer for circle: %@"), circle));
    if (use_cloud_peer) {
        cloud_full_peer = SOSCircleGetiCloudFullPeerInfoRef(circle);
    }
    
    if (SOSCircleCountPeers(circle) == 0) {
        result = SOSAccountResetThisCircleToOffering(account, circle, user_key, error);
    } else {
        SOSAccountModifyCircle(account, SOSCircleGetName(circle), error, ^(SOSCircleRef circle) {
            result = SOSCircleRequestAdmission(circle, user_key, myCirclePeer, error);
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
    SecKeyRef user_key = SOSAccountGetPrivateCredential(account, error);
    if (!user_key)
        return false;

    __block bool success = true;
    
    SOSAccountForEachKnownCircle(account, ^(CFStringRef name) { // Incompatible
        success = false;
        SOSCreateError(kSOSErrorIncompatibleCircle, CFSTR("Incompatible circle"), NULL, error);
    }, ^(SOSCircleRef circle) { //no peer
        success = SOSAccountJoinThisCircle(account, user_key, circle, use_cloud_identity, error) && success;
    }, ^(SOSCircleRef circle, SOSFullPeerInfoRef full_peer) {   // Have Peer
        SOSPeerInfoRef myPeer = SOSFullPeerInfoGetPeerInfo(full_peer);
        if(SOSCircleHasPeer(circle, myPeer, NULL)) goto already_present;
        if(SOSCircleHasApplicant(circle, myPeer, NULL))  goto already_applied;
        if(SOSCircleHasRejectedApplicant(circle, myPeer, NULL)) {
            SOSCircleRemoveRejectedPeer(circle, myPeer, NULL);
        }
        
        secerror("Resetting my peer (ID: %@) for circle '%@' during application", SOSPeerInfoGetPeerID(myPeer), SOSCircleGetName(circle));
        CFErrorRef localError = NULL;
        if (!SOSAccountDestroyCirclePeerInfo(account, circle, &localError)) {
            secerror("Failed to destroy peer (%@) during application, error=%@", myPeer, localError);
            CFReleaseNull(localError);
        }
    already_applied:
        success = SOSAccountJoinThisCircle(account, user_key, circle, use_cloud_identity, error) && success;
        return;
    already_present:
        success = true;
        return;
    });

    if(success) account->departure_code = kSOSNeverLeftCircle;
    return success;
}

bool SOSAccountJoinCircles(SOSAccountRef account, CFErrorRef* error) {
    return SOSAccountJoinCircles_internal(account, false, error);
}

CFStringRef SOSAccountGetDeviceID(SOSAccountRef account, CFErrorRef *error){
    __block CFStringRef result = NULL;
    __block CFStringRef temp = NULL;
    SOSAccountForEachKnownCircle(account, NULL, NULL, ^(SOSCircleRef circle, SOSFullPeerInfoRef myCirclePeer) {
            SOSFullPeerInfoRef fpi = SOSAccountGetMyFullPeerInCircle(account, circle, error);
            if(fpi){
                SOSPeerInfoRef myPeer = SOSFullPeerInfoGetPeerInfo(fpi);
                if(myPeer){
                    temp = SOSPeerInfoGetDeviceID(myPeer);
                    if(!isNull(temp)){
                        result = CFStringCreateCopy(kCFAllocatorDefault, temp);
                    }
                }
                else{
                    secnotice("circle", "Could not acquire my peer info in circle: %@", SOSCircleGetName(circle));
                }
            }
            else{
                secnotice("circle", "Could not acquire my full peer info in circle: %@", SOSCircleGetName(circle));
            }
    });
    return result;
}

bool SOSAccountSetMyDSID(SOSAccountRef account, CFStringRef IDS, CFErrorRef* error){
    __block bool result = false;
    
    SOSAccountForEachKnownCircle(account, NULL, NULL, ^(SOSCircleRef circle, SOSFullPeerInfoRef myCirclePeer) {
        SOSAccountModifyCircle(account, SOSCircleGetName(circle), error, ^(SOSCircleRef circle) {
            SOSFullPeerInfoRef fpi = SOSAccountGetMyFullPeerInCircle(account, circle, error);
            if(fpi){
                SOSPeerInfoRef myPeer = SOSFullPeerInfoGetPeerInfo(fpi);
                if(myPeer){
                    SOSPeerInfoSetDeviceID(myPeer, IDS);
                    result = true;
                }
                else{
                    secnotice("circle", "Could not acquire my peer info in circle: %@", SOSCircleGetName(circle));
                    result = false;
                }
            }
            else{
                secnotice("circle", "Could not acquire my full peer info in circle: %@", SOSCircleGetName(circle));
                result = false;
            }
            return result;
        });
    });
    return result;
    
}
bool SOSAccountJoinCirclesAfterRestore(SOSAccountRef account, CFErrorRef* error) {
    return SOSAccountJoinCircles_internal(account, true, error);
}


bool SOSAccountLeaveCircles(SOSAccountRef account, CFErrorRef* error)
{
    __block bool result = true;
    SOSAccountForEachKnownCircle(account, NULL, NULL, ^(SOSCircleRef circle, SOSFullPeerInfoRef myCirclePeer) {
        SOSAccountModifyCircle(account, SOSCircleGetName(circle), error, ^(SOSCircleRef circle) {
            result = sosAccountLeaveCircle(account, circle, error); // TODO: What about multiple errors!
            return result;
		});
    });

    account->departure_code = kSOSWithdrewMembership;
    return result;
}

bool SOSAccountBail(SOSAccountRef account, uint64_t limit_in_seconds, CFErrorRef* error) {
    dispatch_queue_t queue = dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0);
    dispatch_group_t group = dispatch_group_create();
    __block bool result = false;
    secnotice("circle", "Attempting to leave circle - best effort - in %llu seconds\n", limit_in_seconds);
    // Add a task to the group
    dispatch_group_async(group, queue, ^{
        SOSAccountForEachKnownCircle(account, NULL, NULL, ^(SOSCircleRef circle, SOSFullPeerInfoRef myCirclePeer) {
            SOSAccountModifyCircle(account, SOSCircleGetName(circle), error, ^(SOSCircleRef circle) {
                result = sosAccountLeaveCircle(account, circle, error); // TODO: What about multiple errors!
                return result;
            });
        });
        
        account->departure_code = kSOSWithdrewMembership;
    });
    dispatch_time_t milestone = dispatch_time(DISPATCH_TIME_NOW, limit_in_seconds * NSEC_PER_SEC);

    dispatch_group_wait(group, milestone);
    dispatch_release(group);
    return result;
}


//
// MARK: Application
//

static void for_each_applicant_in_each_circle(SOSAccountRef account, CFArrayRef peer_infos,
                                              bool (^action)(SOSCircleRef circle, SOSFullPeerInfoRef myCirclePeer, SOSPeerInfoRef peer)) {

    SOSAccountForEachKnownCircle(account, NULL, NULL, ^(SOSCircleRef circle, SOSFullPeerInfoRef full_peer) {
        SOSPeerInfoRef me = SOSFullPeerInfoGetPeerInfo(full_peer);
        CFErrorRef peer_error = NULL;
        if (SOSCircleHasPeer(circle, me, &peer_error)) {
            CFArrayForEach(peer_infos, ^(const void *value) {
                SOSPeerInfoRef peer = (SOSPeerInfoRef) value;
                if (SOSCircleHasApplicant(circle, peer, NULL)) {
                    SOSAccountModifyCircle(account, SOSCircleGetName(circle), NULL, ^(SOSCircleRef circle) {
                        return action(circle, full_peer, peer);
                    });
                }
            });
        }
        if (peer_error)
            secerror("Got error in SOSCircleHasPeer: %@", peer_error);
        CFReleaseSafe(peer_error); // TODO: We should be accumulating errors here.
    });
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


CFArrayRef SOSAccountCopyGeneration(SOSAccountRef account, CFErrorRef *error) {
    if (!SOSAccountHasPublicKey(account, error))
        return NULL;
    CFMutableArrayRef generations = CFArrayCreateMutableForCFTypes(kCFAllocatorDefault);
    
    SOSAccountForEachCircle(account, ^(SOSCircleRef circle) {
        CFNumberRef generation = (CFNumberRef)SOSCircleGetGeneration(circle);
        CFArrayAppendValue(generations, SOSCircleGetName(circle));
        CFArrayAppendValue(generations, generation);
    });
    
    return generations;
    
}

bool SOSValidateUserPublic(SOSAccountRef account, CFErrorRef *error) {
    if (!SOSAccountHasPublicKey(account, error))
        return NULL;

    return account->user_public_trusted;
}


bool SOSAccountEnsurePeerRegistration(SOSAccountRef account, CFErrorRef *error) {
    __block bool result = true;
    
    secnotice("updates", "Ensuring peer registration.");
    
    SOSAccountForEachKnownCircle(account, ^(CFStringRef name) {
    }, ^(SOSCircleRef circle) {
    }, ^(SOSCircleRef circle, SOSFullPeerInfoRef full_peer) {
        SOSFullPeerInfoRef fpi = SOSAccountGetMyFullPeerInCircle(account, circle, error);
        SOSPeerInfoRef me = SOSFullPeerInfoGetPeerInfo(fpi);
        CFMutableArrayRef trusted_peer_ids = NULL;
        CFMutableArrayRef untrusted_peer_ids = NULL;
        CFStringRef my_id = NULL;
        if (SOSCircleHasPeer(circle, me, NULL)) {
            my_id = SOSPeerInfoGetPeerID(me);
            trusted_peer_ids = CFArrayCreateMutableForCFTypes(kCFAllocatorDefault);
            untrusted_peer_ids = CFArrayCreateMutableForCFTypes(kCFAllocatorDefault);
            SOSCircleForEachPeer(circle, ^(SOSPeerInfoRef peer) {
                CFMutableArrayRef arrayToAddTo = SOSPeerInfoApplicationVerify(peer, account->user_public, NULL) ? trusted_peer_ids : untrusted_peer_ids;

                CFArrayAppendValueIfNot(arrayToAddTo, SOSPeerInfoGetPeerID(peer), my_id);
            });
        }

        SOSEngineRef engine = SOSDataSourceFactoryGetEngineForDataSourceName(account->factory, SOSCircleGetName(circle), NULL);
        if (engine)
            SOSEngineCircleChanged(engine, my_id, trusted_peer_ids, untrusted_peer_ids);

        CFReleaseNull(trusted_peer_ids);
        CFReleaseNull(untrusted_peer_ids);

        SOSTransportMessageRef transport = (SOSTransportMessageRef)CFDictionaryGetValue(account->message_transports, SOSCircleGetName(circle));
        SOSCircleForEachPeer(circle, ^(SOSPeerInfoRef peer) {
            if (!CFEqualSafe(me, peer)) {
                CFErrorRef localError = NULL;
                SOSPeerCoderInitializeForPeer(transport, full_peer, peer, &localError);
                if (localError)
                    secnotice("updates", "can't initialize transport for peer %@ with %@ (%@)", peer, full_peer, localError);
                CFReleaseSafe(localError);
            }
        });
    });
    
    return result;
}

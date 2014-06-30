/*
 * Created by Michael Brouwer on 6/22/12.
 * Copyright 2012 Apple Inc. All Rights Reserved.
 */

/*
 * SOSAccount.c -  Implementation of the secure object syncing account.
 * An account contains a SOSCircle for each protection domain synced.
 */

#include <SecureObjectSync/SOSInternal.h>
#include <SecureObjectSync/SOSAccount.h>
#include <SecureObjectSync/SOSCircle.h>
#include <SecureObjectSync/SOSCloudCircle.h>
#include <SecureObjectSync/SOSEngine.h>
#include <SecureObjectSync/SOSPeer.h>
#include <SecureObjectSync/SOSFullPeerInfo.h>
#include <SecureObjectSync/SOSPeerInfo.h>
#include <SecureObjectSync/SOSPeerInfoInternal.h>
#include <SecureObjectSync/SOSUserKeygen.h>
#include <Security/SecKeyPriv.h>
#include <Security/SecItemPriv.h>
#include <CoreFoundation/CFArray.h>
#include <dispatch/dispatch.h>
#include <stdlib.h>
#include <assert.h>
#include <AssertMacros.h>
#include <utilities/SecCFWrappers.h>

#include <utilities/der_plist.h>
#include <utilities/der_plist_internal.h>
#include <utilities/iOSforOSX.h>

#include <utilities/SecAKSWrappers.h>

#include <corecrypto/ccder.h>

#include <securityd/SOSCloudCircleServer.h>
#include <securityd/SecDbItem.h> // For SecError

#include <utilities/debugging.h>
#include <utilities/iCloudKeychainTrace.h>

#include <notify.h>

static CFStringRef kicloud_identity_name = CFSTR("Cloud Identity");

//
// Forward statics.
//

static bool SOSAccountHandleUpdateCircle(SOSAccountRef account, SOSCircleRef newCircle, bool writeUpdate, bool initialSync, CFErrorRef *error);

//
// DER Encoding utilities
//

//
// Encodes data or a zero length data
//
static size_t der_sizeof_data_or_null(CFDataRef data, CFErrorRef* error)
{
	if (data) {
		return der_sizeof_data(data, error);
	} else {
		return der_sizeof_null(kCFNull, error);
	}
}

static uint8_t* der_encode_data_or_null(CFDataRef data, CFErrorRef* error, const uint8_t* der, uint8_t* der_end)
{
    if (data) {
		return der_encode_data(data, error, der, der_end);
	} else {
		return der_encode_null(kCFNull, error, der, der_end);
	}
}


static const uint8_t* der_decode_data_or_null(CFAllocatorRef allocator, CFDataRef* data,
                                              CFErrorRef* error,
                                              const uint8_t* der, const uint8_t* der_end)
{
    CFTypeRef value = NULL;
	der = der_decode_plist(allocator, 0, &value, error, der, der_end);
	if (value && CFGetTypeID(value) != CFDataGetTypeID()) {
		CFReleaseNull(value);
	}
	if (data) {
		*data = value;
	}
	return der;
}


//
// Mark: public_bytes encode/decode
//

static size_t der_sizeof_public_bytes(SecKeyRef publicKey, CFErrorRef* error)
{
    CFDataRef publicData = NULL;

    if (publicKey)
        SecKeyCopyPublicBytes(publicKey, &publicData);

    size_t size = der_sizeof_data_or_null(publicData, error);

    CFReleaseNull(publicData);

    return size;
}

static uint8_t* der_encode_public_bytes(SecKeyRef publicKey, CFErrorRef* error, const uint8_t* der, uint8_t* der_end)
{
    CFDataRef publicData = NULL;

    if (publicKey)
        SecKeyCopyPublicBytes(publicKey, &publicData);

    uint8_t *result = der_encode_data_or_null(publicData, error, der, der_end);

    CFReleaseNull(publicData);

    return result;
}

static const uint8_t* der_decode_public_bytes(CFAllocatorRef allocator, CFIndex algorithmID, SecKeyRef* publicKey, CFErrorRef* error, const uint8_t* der, const uint8_t* der_end)
{
    CFDataRef dataFound = NULL;
    der = der_decode_data_or_null(allocator, &dataFound, error, der, der_end);

    if (der && dataFound && publicKey) {
        *publicKey = SecKeyCreateFromPublicData(allocator, algorithmID, dataFound);
    }
    CFReleaseNull(dataFound);

    return der;
}


//
// Cloud Paramters encode/decode
//

static size_t der_sizeof_cloud_parameters(SecKeyRef publicKey, CFDataRef paramters, CFErrorRef* error)
{
    size_t public_key_size = der_sizeof_public_bytes(publicKey, error);
    size_t parameters_size = der_sizeof_data_or_null(paramters, error);

    return ccder_sizeof(CCDER_CONSTRUCTED_SEQUENCE, public_key_size + parameters_size);
}

static uint8_t* der_encode_cloud_parameters(SecKeyRef publicKey, CFDataRef paramters, CFErrorRef* error,
                                            const uint8_t* der, uint8_t* der_end)
{
    uint8_t* original_der_end = der_end;

    return ccder_encode_constructed_tl(CCDER_CONSTRUCTED_SEQUENCE, original_der_end, der,
                                       der_encode_public_bytes(publicKey, error, der,
                                                               der_encode_data_or_null(paramters, error, der, der_end)));
}

static const uint8_t* der_decode_cloud_parameters(CFAllocatorRef allocator,
                                                  CFIndex algorithmID, SecKeyRef* publicKey,
                                                  CFDataRef *parameters,
                                                  CFErrorRef* error,
                                                  const uint8_t* der, const uint8_t* der_end)
{
    const uint8_t *sequence_end;
    der = ccder_decode_sequence_tl(&sequence_end, der, der_end);
    der = der_decode_public_bytes(allocator, algorithmID, publicKey, error, der, sequence_end);
    der = der_decode_data_or_null(allocator, parameters, error, der, sequence_end);

    return der;
}


//
// bool encoding/decoding
//


static const uint8_t* ccder_decode_bool(bool* boolean, const uint8_t* der, const uint8_t *der_end)
{
    if (NULL == der)
        return NULL;

    size_t payload_size = 0;
    const uint8_t *payload = ccder_decode_tl(CCDER_BOOLEAN, &payload_size, der, der_end);

    if (NULL == payload || (der_end - payload) < 1 || payload_size != 1) {
        return NULL;
    }

    if (boolean)
        *boolean = (*payload != 0);

    return payload + payload_size;
}


static size_t ccder_sizeof_bool(bool value __unused, CFErrorRef *error)
{
    return ccder_sizeof(CCDER_BOOLEAN, 1);
}


static uint8_t* ccder_encode_bool(bool value, const uint8_t *der, uint8_t *der_end)
{
    uint8_t value_byte = value;

    return ccder_encode_tl(CCDER_BOOLEAN, 1, der,
           ccder_encode_body(1, &value_byte, der, der_end));
}

struct __OpaqueSOSAccount {
    CFRuntimeBase           _base;

    dispatch_queue_t        queue;

    CFDictionaryRef         gestalt;

    CFMutableDictionaryRef  circle_identities;
    CFMutableDictionaryRef  circles;
    CFMutableDictionaryRef  retired_peers;

    bool      user_public_trusted;
    CFDataRef user_key_parameters;
    SecKeyRef user_public;
    SecKeyRef previous_public;
    enum DepartureReason    departure_code;

    // Non-persistent data

    SOSDataSourceFactoryRef factory;
    SecKeyRef _user_private;
    dispatch_source_t user_private_timer;
    int               lock_notification_token;

    // Live Notification
    CFMutableArrayRef       change_blocks;

    SOSAccountKeyInterestBlock        update_interest_block;
    SOSAccountDataUpdateBlock         update_block;
    SOSAccountMessageProcessedBlock   processed_message_block;

    CFMutableDictionaryRef  deferred_updates;

    CFMutableDictionaryRef  pending_changes;
};

CFGiblisWithCompareFor(SOSAccount);

static inline bool SOSAccountHasLeft(SOSAccountRef account) {
    switch(account->departure_code) {
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

// Private static functions.

static bool SOSUpdateKeyInterest(SOSAccountRef account, bool getNewKeysOnly, CFErrorRef *error);

static bool SOSAccountEnsureFactoryCircles(SOSAccountRef a)
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

static SOSAccountRef SOSAccountCreateBasic(CFAllocatorRef allocator,
                                           CFDictionaryRef gestalt,
                                           SOSDataSourceFactoryRef factory,
                                           SOSAccountKeyInterestBlock interest_block,
                                           SOSAccountDataUpdateBlock update_block) {
    SOSAccountRef a = CFTypeAllocate(SOSAccount, struct __OpaqueSOSAccount, allocator);

    a->queue = dispatch_queue_create("Account Queue", DISPATCH_QUEUE_SERIAL);

    a->gestalt = gestalt;
    CFRetain(a->gestalt);

    a->circles = CFDictionaryCreateMutableForCFTypes(allocator);
    a->circle_identities = CFDictionaryCreateMutableForCFTypes(allocator);
    a->retired_peers = CFDictionaryCreateMutableForCFTypes(allocator);
    
    a->factory = factory; // We adopt the factory. kthanksbai.
    
    a->change_blocks = CFArrayCreateMutableForCFTypes(allocator);
    
    a->update_interest_block = Block_copy(interest_block);
    a->update_block = Block_copy(update_block);

    a->pending_changes = CFDictionaryCreateMutableForCFTypes(allocator);
    a->departure_code = kSOSNeverAppliedToCircle;

    return a;
}


static SOSFullPeerInfoRef SOSAccountGetMyFullPeerInCircleNamedIfPresent(SOSAccountRef account, CFStringRef name, CFErrorRef *error) {
    if (CFDictionaryGetValue(account->circles, name) == NULL) {
        SOSCreateErrorWithFormat(kSOSErrorNoCircle, NULL, error, NULL, CFSTR("No circle named '%@'"), name);
        return NULL;
    }
    
    return (SOSFullPeerInfoRef) CFDictionaryGetValue(account->circle_identities, name);
}


static void SOSAccountForEachKnownCircle(SOSAccountRef account,
                                         void (^handle_incompatible)(CFStringRef name),
                                         void (^handle_no_peer)(SOSCircleRef circle),
                                         void (^handle_peer)(SOSCircleRef circle, SOSFullPeerInfoRef full_peer)) {
    CFDictionaryForEach(account->circles, ^(const void *key, const void *value) {
        if (isNull(value)) {
            if (handle_incompatible)
                handle_incompatible((CFStringRef)key);
        } else {
            SOSCircleRef circle = (SOSCircleRef) value;
            CFRetainSafe(circle);
            SOSFullPeerInfoRef fpi = SOSAccountGetMyFullPeerInCircleNamedIfPresent(account, SOSCircleGetName(circle), NULL);
            if (!fpi) {
                if (handle_no_peer)
                    handle_no_peer(circle);
            } else {
                CFRetainSafe(fpi);
                if (handle_peer)
                    handle_peer(circle, fpi);
                CFReleaseSafe(fpi);
            }
            CFReleaseSafe(circle);
        }
    });
}


bool SOSAccountUpdateGestalt(SOSAccountRef account, CFDictionaryRef new_gestalt)
{
    if (CFEqual(new_gestalt, account->gestalt))
        return false;
    
    SOSAccountForEachKnownCircle(account, NULL, NULL, ^(SOSCircleRef circle, SOSFullPeerInfoRef full_peer) {
        if (SOSFullPeerInfoUpdateGestalt(full_peer, new_gestalt, NULL)) {
            SOSAccountModifyCircle(account, SOSCircleGetName(circle),
                                   NULL, ^(SOSCircleRef circle_to_change) {
                                       (void) SOSCircleUpdatePeerInfo(circle_to_change, SOSFullPeerInfoGetPeerInfo(full_peer));
                                   });
        };
    });

    CFReleaseNull(account->gestalt);
    account->gestalt = new_gestalt;
    CFRetain(account->gestalt);
    
    return true;
}

SOSAccountRef SOSAccountCreate(CFAllocatorRef allocator,
                               CFDictionaryRef gestalt,
                               SOSDataSourceFactoryRef factory,
                               SOSAccountKeyInterestBlock interest_block,
                               SOSAccountDataUpdateBlock update_block) {
    SOSAccountRef a = SOSAccountCreateBasic(allocator, gestalt, factory, interest_block, update_block);

    SOSAccountEnsureFactoryCircles(a);

    return a;
}

static void SOSAccountDestroy(CFTypeRef aObj) {
    SOSAccountRef a = (SOSAccountRef) aObj;

    if (a->factory)
        a->factory->release(a->factory);

    CFReleaseNull(a->gestalt);
    CFReleaseNull(a->circle_identities);
    CFReleaseNull(a->circles);
    CFReleaseNull(a->retired_peers);

    a->user_public_trusted = false;
    CFReleaseNull(a->user_public);
    CFReleaseNull(a->user_key_parameters);

    SOSAccountPurgePrivateCredential(a);
    CFReleaseNull(a->previous_public);

    CFReleaseNull(a->change_blocks);
    Block_release(a->update_interest_block);
    Block_release(a->update_block);
    CFReleaseNull(a->processed_message_block);
    CFReleaseNull(a->pending_changes);
    CFReleaseNull(a->deferred_updates);
    a->departure_code = kSOSNeverAppliedToCircle;

    dispatch_release(a->queue);
}

static void SOSAccountSetToNew(SOSAccountRef a) {
    CFAllocatorRef allocator = CFGetAllocator(a);
    CFReleaseNull(a->circle_identities);
    CFReleaseNull(a->circles);
    CFReleaseNull(a->retired_peers);

    CFReleaseNull(a->user_key_parameters);
    CFReleaseNull(a->user_public);
    CFReleaseNull(a->previous_public);
    CFReleaseNull(a->_user_private);
    
    CFReleaseNull(a->pending_changes);
    CFReleaseNull(a->deferred_updates);
    
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
    a->pending_changes = CFDictionaryCreateMutableForCFTypes(allocator);

    SOSAccountEnsureFactoryCircles(a);
}


static CFStringRef SOSAccountCopyDescription(CFTypeRef aObj) {
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

#if OLD_CODERS_SUPPORTED

//
// MARK: Persistent Encode decode
//
SOSAccountRef SOSAccountCreateFromDER_V1(CFAllocatorRef allocator,
                                      SOSDataSourceFactoryRef factory,
                                      SOSAccountKeyInterestBlock interest_block,
                                      SOSAccountDataUpdateBlock update_block,
                                      CFErrorRef* error,
                                      const uint8_t** der_p, const uint8_t *der_end)
{
    SOSAccountRef account = NULL;

    const uint8_t *sequence_end;
    *der_p = ccder_decode_constructed_tl(CCDER_CONSTRUCTED_SEQUENCE, &sequence_end, *der_p, der_end);

    {
        CFDictionaryRef decoded_gestalt = NULL;
        *der_p = der_decode_dictionary(kCFAllocatorDefault, kCFPropertyListImmutable, &decoded_gestalt, error,
                                       *der_p, der_end);

        if (*der_p == 0)
            return NULL;

        account = SOSAccountCreateBasic(allocator, decoded_gestalt, factory, interest_block, update_block);
        CFReleaseNull(decoded_gestalt);
    }

    CFArrayRef array = NULL;
    *der_p = der_decode_array(kCFAllocatorDefault, 0, &array, error, *der_p, sequence_end);

    *der_p = ccder_decode_bool(&account->user_public_trusted, *der_p, sequence_end);
    *der_p = der_decode_public_bytes(kCFAllocatorDefault, kSecECDSAAlgorithmID, &account->user_public, error, *der_p, sequence_end);
    *der_p = der_decode_data_or_null(kCFAllocatorDefault, &account->user_key_parameters, error, *der_p, sequence_end);
    *der_p = der_decode_dictionary(kCFAllocatorDefault, kCFPropertyListMutableContainers, (CFDictionaryRef *) &account->retired_peers, error, *der_p, sequence_end);
    if (*der_p != sequence_end)
        *der_p = NULL;

    __block bool success = true;

    require_quiet(array && *der_p, fail);
    
    CFArrayForEach(array, ^(const void *value) {
        if (success) {
            if (isString(value)) {
                CFDictionaryAddValue(account->circles, value, kCFNull);
            } else {
                CFDataRef circleData = NULL;
                CFDataRef fullPeerInfoData = NULL;

                if (isData(value)) {
                    circleData = (CFDataRef) value;
                } else if (isArray(value)) {
                    CFArrayRef pair = (CFArrayRef) value;
                    
                    CFTypeRef circleObject = CFArrayGetValueAtIndex(pair, 0);
                    CFTypeRef fullPeerInfoObject = CFArrayGetValueAtIndex(pair, 1);
                    
                    if (CFArrayGetCount(pair) == 2 && isData(circleObject) && isData(fullPeerInfoObject)) {
                        circleData = (CFDataRef) circleObject;
                        fullPeerInfoData = (CFDataRef) fullPeerInfoObject;
                    }
                }
                
                if (circleData) {
                    SOSCircleRef circle = SOSCircleCreateFromData(kCFAllocatorDefault, circleData, error);
                    require_action_quiet(circle, fail, success = false);
                    
                    CFStringRef circleName = SOSCircleGetName(circle);
                    CFDictionaryAddValue(account->circles, circleName, circle);

                    if (fullPeerInfoData) {
                        SOSFullPeerInfoRef full_peer = SOSFullPeerInfoCreateFromData(kCFAllocatorDefault, fullPeerInfoData, error);
                        require_action_quiet(full_peer, fail, success = false);
                        
                        CFDictionaryAddValue(account->circle_identities, circleName, full_peer);
                        CFReleaseNull(full_peer);
                    }
                fail:
                    CFReleaseNull(circle);
                }
            }
        }
    });
    CFReleaseNull(array);
    
    require_quiet(success, fail);
    require_action_quiet(SOSAccountEnsureFactoryCircles(account), fail,
                         SOSCreateError(kSOSErrorBadFormat, CFSTR("Cannot EnsureFactoryCircles"), (error != NULL) ? *error : NULL, error));

    return account;
    
fail:
    // Create a default error if we don't have one:
    SOSCreateError(kSOSErrorBadFormat, CFSTR("Bad Account DER"), NULL, error);
    CFReleaseNull(account);
    return NULL;
}

SOSAccountRef SOSAccountCreateFromDER_V2(CFAllocatorRef allocator,
                                         SOSDataSourceFactoryRef factory,
                                         SOSAccountKeyInterestBlock interest_block,
                                         SOSAccountDataUpdateBlock update_block,
                                         CFErrorRef* error,
                                         const uint8_t** der_p, const uint8_t *der_end)
{
    SOSAccountRef account = NULL;
    const uint8_t *dersave = *der_p;
    const uint8_t *derend = der_end;

    const uint8_t *sequence_end;
    *der_p = ccder_decode_constructed_tl(CCDER_CONSTRUCTED_SEQUENCE, &sequence_end, *der_p, der_end);

    {
        CFDictionaryRef decoded_gestalt = NULL;
        *der_p = der_decode_dictionary(kCFAllocatorDefault, kCFPropertyListImmutable, &decoded_gestalt, error,
                                       *der_p, der_end);

        if (*der_p == 0)
            return NULL;

        account = SOSAccountCreateBasic(allocator, decoded_gestalt, factory, interest_block, update_block);
        CFReleaseNull(decoded_gestalt);
    }

    CFArrayRef array = NULL;
    *der_p = der_decode_array(kCFAllocatorDefault, 0, &array, error, *der_p, sequence_end);

    uint64_t tmp_departure_code = kSOSNeverAppliedToCircle;
    *der_p = ccder_decode_uint64(&tmp_departure_code, *der_p, sequence_end);
    *der_p = ccder_decode_bool(&account->user_public_trusted, *der_p, sequence_end);
    *der_p = der_decode_public_bytes(kCFAllocatorDefault, kSecECDSAAlgorithmID, &account->user_public, error, *der_p, sequence_end);
    *der_p = der_decode_data_or_null(kCFAllocatorDefault, &account->user_key_parameters, error, *der_p, sequence_end);
    *der_p = der_decode_dictionary(kCFAllocatorDefault, kCFPropertyListMutableContainers, (CFDictionaryRef *) &account->retired_peers, error, *der_p, sequence_end);
    if (*der_p != sequence_end)
        *der_p = NULL;
    account->departure_code = (enum DepartureReason) tmp_departure_code;

    __block bool success = true;

    require_quiet(array && *der_p, fail);

    CFArrayForEach(array, ^(const void *value) {
        if (success) {
            if (isString(value)) {
                CFDictionaryAddValue(account->circles, value, kCFNull);
            } else {
                CFDataRef circleData = NULL;
                CFDataRef fullPeerInfoData = NULL;

                if (isData(value)) {
                    circleData = (CFDataRef) value;
                } else if (isArray(value)) {
                    CFArrayRef pair = (CFArrayRef) value;

                    CFTypeRef circleObject = CFArrayGetValueAtIndex(pair, 0);
                    CFTypeRef fullPeerInfoObject = CFArrayGetValueAtIndex(pair, 1);

                    if (CFArrayGetCount(pair) == 2 && isData(circleObject) && isData(fullPeerInfoObject)) {
                        circleData = (CFDataRef) circleObject;
                        fullPeerInfoData = (CFDataRef) fullPeerInfoObject;
                    }
                }

                if (circleData) {
                    SOSCircleRef circle = SOSCircleCreateFromData(kCFAllocatorDefault, circleData, error);
                    require_action_quiet(circle, fail, success = false);

                    CFStringRef circleName = SOSCircleGetName(circle);
                    CFDictionaryAddValue(account->circles, circleName, circle);

                    if (fullPeerInfoData) {
                        SOSFullPeerInfoRef full_peer = SOSFullPeerInfoCreateFromData(kCFAllocatorDefault, fullPeerInfoData, error);
                        require_action_quiet(full_peer, fail, success = false);

                        CFDictionaryAddValue(account->circle_identities, circleName, full_peer);
                    }
                fail:
                    CFReleaseNull(circle);
                }
            }
        }
    });
    CFReleaseNull(array);

    require_quiet(success, fail);
    require_action_quiet(SOSAccountEnsureFactoryCircles(account), fail,
                         SOSCreateError(kSOSErrorBadFormat, CFSTR("Cannot EnsureFactoryCircles"), (error != NULL) ? *error : NULL, error));

    return account;

fail:
    // Create a default error if we don't have one:
    account->factory = NULL; // give the factory back.
    CFReleaseNull(account);
    // Try the der inflater from the previous release.
    account = SOSAccountCreateFromDER_V1(allocator, factory, interest_block, update_block, error, &dersave, derend);
    if(account) account->departure_code = kSOSNeverAppliedToCircle;
    return account;
}

#endif /* OLD_CODERS_SUPPORTED */

#define CURRENT_ACCOUNT_PERSISTENT_VERSION 6

SOSAccountRef SOSAccountCreateFromDER(CFAllocatorRef allocator,
                                         SOSDataSourceFactoryRef factory,
                                         SOSAccountKeyInterestBlock interest_block,
                                         SOSAccountDataUpdateBlock update_block,
                                         CFErrorRef* error,
                                         const uint8_t** der_p, const uint8_t *der_end)
{
    SOSAccountRef account = NULL;
#if UPGRADE_FROM_PREVIOUS_VERSION
    const uint8_t *dersave = *der_p;
    const uint8_t *derend = der_end;
#endif
    uint64_t version = 0;
    
    const uint8_t *sequence_end;
    *der_p = ccder_decode_constructed_tl(CCDER_CONSTRUCTED_SEQUENCE, &sequence_end, *der_p, der_end);
    *der_p = ccder_decode_uint64(&version, *der_p, sequence_end);
    if(!(*der_p) || version < CURRENT_ACCOUNT_PERSISTENT_VERSION) {
#if UPGRADE_FROM_PREVIOUS_VERSION
        return SOSAccountCreateFromDER_V3(allocator, factory, interest_block, update_block, error, &dersave, derend);
#else
        return NULL;
#endif
    }
    
    {
        CFDictionaryRef decoded_gestalt = NULL;
        *der_p = der_decode_dictionary(kCFAllocatorDefault, kCFPropertyListImmutable, &decoded_gestalt, error,
                                       *der_p, der_end);
        
        if (*der_p == 0)
            return NULL;
        
        account = SOSAccountCreateBasic(allocator, decoded_gestalt, factory, interest_block, update_block);
        CFReleaseNull(decoded_gestalt);
    }
    
    CFArrayRef array = NULL;
    *der_p = der_decode_array(kCFAllocatorDefault, 0, &array, error, *der_p, sequence_end);
    
    uint64_t tmp_departure_code = kSOSNeverAppliedToCircle;
    *der_p = ccder_decode_uint64(&tmp_departure_code, *der_p, sequence_end);
    *der_p = ccder_decode_bool(&account->user_public_trusted, *der_p, sequence_end);
    *der_p = der_decode_public_bytes(kCFAllocatorDefault, kSecECDSAAlgorithmID, &account->user_public, error, *der_p, sequence_end);
    *der_p = der_decode_public_bytes(kCFAllocatorDefault, kSecECDSAAlgorithmID, &account->previous_public, error, *der_p, sequence_end);
    *der_p = der_decode_data_or_null(kCFAllocatorDefault, &account->user_key_parameters, error, *der_p, sequence_end);
    *der_p = der_decode_dictionary(kCFAllocatorDefault, kCFPropertyListMutableContainers, (CFDictionaryRef *) &account->retired_peers, error, *der_p, sequence_end);
    if (*der_p != sequence_end)
        *der_p = NULL;
    account->departure_code = (enum DepartureReason) tmp_departure_code;
    
    __block bool success = true;
    
    require_quiet(array && *der_p, fail);
    
    CFArrayForEach(array, ^(const void *value) {
        if (success) {
            if (isString(value)) {
                CFDictionaryAddValue(account->circles, value, kCFNull);
            } else {
                CFDataRef circleData = NULL;
                CFDataRef fullPeerInfoData = NULL;
                
                if (isData(value)) {
                    circleData = (CFDataRef) value;
                } else if (isArray(value)) {
                    CFArrayRef pair = (CFArrayRef) value;
                    
                    CFTypeRef circleObject = CFArrayGetValueAtIndex(pair, 0);
                    CFTypeRef fullPeerInfoObject = CFArrayGetValueAtIndex(pair, 1);
                    
                    if (CFArrayGetCount(pair) == 2 && isData(circleObject) && isData(fullPeerInfoObject)) {
                        circleData = (CFDataRef) circleObject;
                        fullPeerInfoData = (CFDataRef) fullPeerInfoObject;
                    }
                }
                
                if (circleData) {
                    SOSCircleRef circle = SOSCircleCreateFromData(kCFAllocatorDefault, circleData, error);
                    require_action_quiet(circle, fail, success = false);
                    
                    CFStringRef circleName = SOSCircleGetName(circle);
                    CFDictionaryAddValue(account->circles, circleName, circle);
                    
                    if (fullPeerInfoData) {
                        SOSFullPeerInfoRef full_peer = SOSFullPeerInfoCreateFromData(kCFAllocatorDefault, fullPeerInfoData, error);
                        require_action_quiet(full_peer, fail, success = false);
                        
                        CFDictionaryAddValue(account->circle_identities, circleName, full_peer);
                    }
                fail:
                    CFReleaseNull(circle);
                }
            }
        }
    });
    CFReleaseNull(array);
    
    require_quiet(success, fail);
    require_action_quiet(SOSAccountEnsureFactoryCircles(account), fail,
                         SOSCreateError(kSOSErrorBadFormat, CFSTR("Cannot EnsureFactoryCircles"), (error != NULL) ? *error : NULL, error));
    
    return account;
    
fail:
    account->factory = NULL; // give the factory back.
    CFReleaseNull(account);
    return NULL;
}


SOSAccountRef SOSAccountCreateFromDER_V3(CFAllocatorRef allocator,
                                         SOSDataSourceFactoryRef factory,
                                         SOSAccountKeyInterestBlock interest_block,
                                         SOSAccountDataUpdateBlock update_block,
                                         CFErrorRef* error,
                                         const uint8_t** der_p, const uint8_t *der_end)
{
    SOSAccountRef account = NULL;
    uint64_t version = 0;
    
    const uint8_t *sequence_end;
    *der_p = ccder_decode_constructed_tl(CCDER_CONSTRUCTED_SEQUENCE, &sequence_end, *der_p, der_end);
    *der_p = ccder_decode_uint64(&version, *der_p, sequence_end);
    if(!(*der_p) || version != 3) {
        // In this case we want to silently fail so that an account gets newly created.
        return NULL;
    }

    {
        CFDictionaryRef decoded_gestalt = NULL;
        *der_p = der_decode_dictionary(kCFAllocatorDefault, kCFPropertyListImmutable, &decoded_gestalt, error,
                                       *der_p, der_end);
        
        if (*der_p == 0)
            return NULL;
        
        account = SOSAccountCreateBasic(allocator, decoded_gestalt, factory, interest_block, update_block);
        CFReleaseNull(decoded_gestalt);
    }
    
    CFArrayRef array = NULL;
    *der_p = der_decode_array(kCFAllocatorDefault, 0, &array, error, *der_p, sequence_end);
    
    uint64_t tmp_departure_code = kSOSNeverAppliedToCircle;
    *der_p = ccder_decode_uint64(&tmp_departure_code, *der_p, sequence_end);
    *der_p = ccder_decode_bool(&account->user_public_trusted, *der_p, sequence_end);
    *der_p = der_decode_public_bytes(kCFAllocatorDefault, kSecECDSAAlgorithmID, &account->user_public, error, *der_p, sequence_end);
    *der_p = der_decode_data_or_null(kCFAllocatorDefault, &account->user_key_parameters, error, *der_p, sequence_end);
    *der_p = der_decode_dictionary(kCFAllocatorDefault, kCFPropertyListMutableContainers, (CFDictionaryRef *) &account->retired_peers, error, *der_p, sequence_end);
    if (*der_p != sequence_end)
        *der_p = NULL;
    account->departure_code = (enum DepartureReason) tmp_departure_code;
    
    __block bool success = true;
    
    require_quiet(array && *der_p, fail);
    
    CFArrayForEach(array, ^(const void *value) {
        if (success) {
            if (isString(value)) {
                CFDictionaryAddValue(account->circles, value, kCFNull);
            } else {
                CFDataRef circleData = NULL;
                CFDataRef fullPeerInfoData = NULL;
                
                if (isData(value)) {
                    circleData = (CFDataRef) value;
                } else if (isArray(value)) {
                    CFArrayRef pair = (CFArrayRef) value;
                    
                    CFTypeRef circleObject = CFArrayGetValueAtIndex(pair, 0);
                    CFTypeRef fullPeerInfoObject = CFArrayGetValueAtIndex(pair, 1);
                    
                    if (CFArrayGetCount(pair) == 2 && isData(circleObject) && isData(fullPeerInfoObject)) {
                        circleData = (CFDataRef) circleObject;
                        fullPeerInfoData = (CFDataRef) fullPeerInfoObject;
                    }
                }
                
                if (circleData) {
                    SOSCircleRef circle = SOSCircleCreateFromData(kCFAllocatorDefault, circleData, error);
                    require_action_quiet(circle, fail, success = false);
                    
                    CFStringRef circleName = SOSCircleGetName(circle);
                    CFDictionaryAddValue(account->circles, circleName, circle);
                    
                    if (fullPeerInfoData) {
                        SOSFullPeerInfoRef full_peer = SOSFullPeerInfoCreateFromData(kCFAllocatorDefault, fullPeerInfoData, error);
                        require_action_quiet(full_peer, fail, success = false);
                        
                        CFDictionaryAddValue(account->circle_identities, circleName, full_peer);
                    }
                fail:
                    CFReleaseNull(circle);
                }
            }
        }
    });
    CFReleaseNull(array);
    
    require_quiet(success, fail);
    require_action_quiet(SOSAccountEnsureFactoryCircles(account), fail,
                         SOSCreateError(kSOSErrorBadFormat, CFSTR("Cannot EnsureFactoryCircles"), (error != NULL) ? *error : NULL, error));
    
    return account;
    
fail:
    // Create a default error if we don't have one:
    account->factory = NULL; // give the factory back.
    CFReleaseNull(account);
    // Don't try the der inflater from the previous release.
    // account = SOSAccountCreateFromDER_V2(allocator, factory, interest_block, update_block, error, &dersave, derend);
    if(account) account->departure_code = kSOSNeverAppliedToCircle;
    return account;
}

SOSAccountRef SOSAccountCreateFromData(CFAllocatorRef allocator, CFDataRef circleData,
                                       SOSDataSourceFactoryRef factory,
                                       SOSAccountKeyInterestBlock interest_block,
                                       SOSAccountDataUpdateBlock update_block,
                                       CFErrorRef* error)
{
    size_t size = CFDataGetLength(circleData);
    const uint8_t *der = CFDataGetBytePtr(circleData);
    SOSAccountRef account = SOSAccountCreateFromDER(allocator, factory, interest_block, update_block,
                                                    error,
                                                    &der, der + size);
    return account;
}

static CFMutableArrayRef SOSAccountCopyCircleArrayToEncode(SOSAccountRef account)
{
    CFMutableArrayRef arrayToEncode = CFArrayCreateMutableForCFTypes(kCFAllocatorDefault);

    CFDictionaryForEach(account->circles, ^(const void *key, const void *value) {
        if (isNull(value)) {
            CFArrayAppendValue(arrayToEncode, key); // Encode the name of the circle that's out of date.
        } else {
            SOSCircleRef circle = (SOSCircleRef) value;
            CFDataRef encodedCircle = SOSCircleCopyEncodedData(circle, kCFAllocatorDefault, NULL);
            CFTypeRef arrayEntry = encodedCircle;
            CFRetainSafe(arrayEntry);
            
            SOSFullPeerInfoRef full_peer = (SOSFullPeerInfoRef) CFDictionaryGetValue(account->circle_identities, key);
        
            if (full_peer) {
                CFDataRef encodedPeer = SOSFullPeerInfoCopyEncodedData(full_peer, kCFAllocatorDefault, NULL);
                CFTypeRef originalArrayEntry = arrayEntry;
                arrayEntry = CFArrayCreateForCFTypes(kCFAllocatorDefault, encodedCircle, encodedPeer, NULL);
                
                CFReleaseSafe(originalArrayEntry);
                CFReleaseNull(encodedPeer);
            }
            
            CFArrayAppendValue(arrayToEncode, arrayEntry);

            CFReleaseSafe(arrayEntry);
            CFReleaseNull(encodedCircle);
        }
		
    });

    return arrayToEncode;
}

size_t SOSAccountGetDEREncodedSize(SOSAccountRef account, CFErrorRef *error)
{
    size_t sequence_size = 0;
    CFMutableArrayRef arrayToEncode = SOSAccountCopyCircleArrayToEncode(account);
    uint64_t version = CURRENT_ACCOUNT_PERSISTENT_VERSION;
    
    require_quiet(accumulate_size(&sequence_size, ccder_sizeof_uint64(version)),                                    fail);
    require_quiet(accumulate_size(&sequence_size, der_sizeof_dictionary(account->gestalt, error)),                  fail);
    require_quiet(accumulate_size(&sequence_size, der_sizeof_array(arrayToEncode, error)),                          fail);
    require_quiet(accumulate_size(&sequence_size, ccder_sizeof_uint64(account->departure_code)),                    fail);
    require_quiet(accumulate_size(&sequence_size, ccder_sizeof_bool(account->user_public_trusted, error)),          fail);
    require_quiet(accumulate_size(&sequence_size, der_sizeof_public_bytes(account->user_public, error)),            fail);
    require_quiet(accumulate_size(&sequence_size, der_sizeof_public_bytes(account->previous_public, error)),        fail);
    require_quiet(accumulate_size(&sequence_size, der_sizeof_data_or_null(account->user_key_parameters, error)),    fail);
    require_quiet(accumulate_size(&sequence_size, der_sizeof_dictionary(account->retired_peers, error)),            fail);
    
    CFReleaseNull(arrayToEncode);
    return ccder_sizeof(CCDER_CONSTRUCTED_SEQUENCE, sequence_size);
    
fail:
    CFReleaseNull(arrayToEncode);
    SecCFDERCreateError(kSecDERErrorUnknownEncoding, CFSTR("don't know how to encode"), NULL, error);
    return 0;
}

uint8_t* SOSAccountEncodeToDER(SOSAccountRef account, CFErrorRef* error, const uint8_t* der, uint8_t* der_end)
{
    CFMutableArrayRef arrayToEncode = SOSAccountCopyCircleArrayToEncode(account);
    uint64_t version = CURRENT_ACCOUNT_PERSISTENT_VERSION;
    der_end =  ccder_encode_constructed_tl(CCDER_CONSTRUCTED_SEQUENCE, der_end, der,
        ccder_encode_uint64(version, der,
        der_encode_dictionary(account->gestalt, error, der,
        der_encode_array(arrayToEncode, error, der,
        ccder_encode_uint64(account->departure_code, der,
        ccder_encode_bool(account->user_public_trusted, der,
        der_encode_public_bytes(account->user_public, error, der,
        der_encode_public_bytes(account->previous_public, error, der,
        der_encode_data_or_null(account->user_key_parameters, error, der,
        der_encode_dictionary(account->retired_peers, error, der, der_end))))))))));

    CFReleaseNull(arrayToEncode);
    
    return der_end;
}



size_t SOSAccountGetDEREncodedSize_V3(SOSAccountRef account, CFErrorRef *error)
{
    size_t sequence_size = 0;
    CFMutableArrayRef arrayToEncode = SOSAccountCopyCircleArrayToEncode(account);
    uint64_t version = CURRENT_ACCOUNT_PERSISTENT_VERSION;

    require_quiet(accumulate_size(&sequence_size, ccder_sizeof_uint64(version)),                                    fail);
    require_quiet(accumulate_size(&sequence_size, der_sizeof_dictionary(account->gestalt, error)),                  fail);
    require_quiet(accumulate_size(&sequence_size, der_sizeof_array(arrayToEncode, error)),                          fail);
    require_quiet(accumulate_size(&sequence_size, ccder_sizeof_uint64(account->departure_code)),                    fail);
    require_quiet(accumulate_size(&sequence_size, ccder_sizeof_bool(account->user_public_trusted, error)),          fail);
    require_quiet(accumulate_size(&sequence_size, der_sizeof_public_bytes(account->user_public, error)),            fail);
    require_quiet(accumulate_size(&sequence_size, der_sizeof_data_or_null(account->user_key_parameters, error)),    fail);
    require_quiet(accumulate_size(&sequence_size, der_sizeof_dictionary(account->retired_peers, error)),            fail);
    
    CFReleaseNull(arrayToEncode);
    return ccder_sizeof(CCDER_CONSTRUCTED_SEQUENCE, sequence_size);
    
fail:
    CFReleaseNull(arrayToEncode);
    SecCFDERCreateError(kSecDERErrorUnknownEncoding, CFSTR("don't know how to encode"), NULL, error);
    return 0;
}

uint8_t* SOSAccountEncodeToDER_V3(SOSAccountRef account, CFErrorRef* error, const uint8_t* der, uint8_t* der_end)
{
    CFMutableArrayRef arrayToEncode = SOSAccountCopyCircleArrayToEncode(account);
    uint64_t version = 3;
    der_end =  ccder_encode_constructed_tl(CCDER_CONSTRUCTED_SEQUENCE, der_end, der,
        ccder_encode_uint64(version, der,
        der_encode_dictionary(account->gestalt, error, der,
        der_encode_array(arrayToEncode, error, der,
        ccder_encode_uint64(account->departure_code, der,
        ccder_encode_bool(account->user_public_trusted, der,
        der_encode_public_bytes(account->user_public, error, der,
        der_encode_data_or_null(account->user_key_parameters, error, der,
        der_encode_dictionary(account->retired_peers, error, der, der_end)))))))));
    
    CFReleaseNull(arrayToEncode);
    
    return der_end;
}

#if OLD_CODERS_SUPPORTED

/* Original V2 encoders */

size_t SOSAccountGetDEREncodedSize_V2(SOSAccountRef account, CFErrorRef *error)
{
    size_t sequence_size = 0;
    CFMutableArrayRef arrayToEncode = SOSAccountCopyCircleArrayToEncode(account);

    require_quiet(accumulate_size(&sequence_size, der_sizeof_dictionary(account->gestalt, error)),                  fail);
    require_quiet(accumulate_size(&sequence_size, der_sizeof_array(arrayToEncode, error)),                          fail);
    require_quiet(accumulate_size(&sequence_size, ccder_sizeof_uint64(account->departure_code)), fail);
    require_quiet(accumulate_size(&sequence_size, ccder_sizeof_bool(account->user_public_trusted, error)),          fail);
    require_quiet(accumulate_size(&sequence_size, der_sizeof_public_bytes(account->user_public, error)),            fail);
    require_quiet(accumulate_size(&sequence_size, der_sizeof_data_or_null(account->user_key_parameters, error)),    fail);
    require_quiet(accumulate_size(&sequence_size, der_sizeof_dictionary(account->retired_peers, error)),            fail);

    CFReleaseNull(arrayToEncode);
    return ccder_sizeof(CCDER_CONSTRUCTED_SEQUENCE, sequence_size);

fail:
    CFReleaseNull(arrayToEncode);
    SecCFDERCreateError(kSecDERErrorUnknownEncoding, CFSTR("don't know how to encode"), NULL, error);
    return 0;
}

uint8_t* SOSAccountEncodeToDER_V2(SOSAccountRef account, CFErrorRef* error, const uint8_t* der, uint8_t* der_end)
{
    CFMutableArrayRef arrayToEncode = SOSAccountCopyCircleArrayToEncode(account);

    der_end =  ccder_encode_constructed_tl(CCDER_CONSTRUCTED_SEQUENCE, der_end, der,
               der_encode_dictionary(account->gestalt, error, der,
               der_encode_array(arrayToEncode, error, der,
               ccder_encode_uint64(account->departure_code, der,
               ccder_encode_bool(account->user_public_trusted, der,
               der_encode_public_bytes(account->user_public, error, der,
               der_encode_data_or_null(account->user_key_parameters, error, der,
               der_encode_dictionary(account->retired_peers, error, der, der_end))))))));

    CFReleaseNull(arrayToEncode);

    return der_end;
}


/* Original V1 encoders */


size_t SOSAccountGetDEREncodedSize_V1(SOSAccountRef account, CFErrorRef *error)
{
    size_t sequence_size = 0;
    CFMutableArrayRef arrayToEncode = SOSAccountCopyCircleArrayToEncode(account);

    require_quiet(accumulate_size(&sequence_size, der_sizeof_dictionary(account->gestalt, error)),                  fail);
    require_quiet(accumulate_size(&sequence_size, der_sizeof_array(arrayToEncode, error)),                          fail);
    require_quiet(accumulate_size(&sequence_size, ccder_sizeof_bool(account->user_public_trusted, error)),          fail);
    require_quiet(accumulate_size(&sequence_size, der_sizeof_public_bytes(account->user_public, error)),            fail);
    require_quiet(accumulate_size(&sequence_size, der_sizeof_data_or_null(account->user_key_parameters, error)),    fail);
    require_quiet(accumulate_size(&sequence_size, der_sizeof_dictionary(account->retired_peers, error)),            fail);

    CFReleaseNull(arrayToEncode);
    return ccder_sizeof(CCDER_CONSTRUCTED_SEQUENCE, sequence_size);

fail:
    CFReleaseNull(arrayToEncode);
    SecCFDERCreateError(kSecDERErrorUnknownEncoding, CFSTR("don't know how to encode"), NULL, error);
    return 0;
}

uint8_t* SOSAccountEncodeToDER_V1(SOSAccountRef account, CFErrorRef* error, const uint8_t* der, uint8_t* der_end)
{
    CFMutableArrayRef arrayToEncode = SOSAccountCopyCircleArrayToEncode(account);

    der_end =  ccder_encode_constructed_tl(CCDER_CONSTRUCTED_SEQUENCE, der_end, der,
        der_encode_dictionary(account->gestalt, error, der,
        der_encode_array(arrayToEncode, error, der,
        ccder_encode_bool(account->user_public_trusted, der,
        der_encode_public_bytes(account->user_public, error, der,
        der_encode_data_or_null(account->user_key_parameters, error, der,
               der_encode_dictionary(account->retired_peers, error, der, der_end)))))));

    CFReleaseNull(arrayToEncode);

    return der_end;
}
#endif /* OLD_CODERS_SUPPORTED */

/************************/

CFDataRef SOSAccountCopyEncodedData(SOSAccountRef account, CFAllocatorRef allocator, CFErrorRef *error)
{
    size_t size = SOSAccountGetDEREncodedSize(account, error);
    if (size == 0)
        return NULL;
    uint8_t buffer[size];
    uint8_t* start = SOSAccountEncodeToDER(account, error, buffer, buffer + sizeof(buffer));
    CFDataRef result = CFDataCreate(kCFAllocatorDefault, start, size);
    return result;
}

dispatch_queue_t SOSAccountGetQueue(SOSAccountRef account) {
    return account->queue;
}

//
// MARK: User Credential management
//

void SOSAccountPurgePrivateCredential(SOSAccountRef account)
{
    CFReleaseNull(account->_user_private);
    if (account->user_private_timer) {
        dispatch_source_cancel(account->user_private_timer);
        dispatch_release(account->user_private_timer);
        account->user_private_timer = NULL;
        xpc_transaction_end();
    }
    if (account->lock_notification_token) {
        notify_cancel(account->lock_notification_token);
        account->lock_notification_token = 0;
    }
}

static void SOSAccountSetPrivateCredential(SOSAccountRef account, SecKeyRef private) {
    if (!private)
        return SOSAccountPurgePrivateCredential(account);

    CFRetain(private);
    CFReleaseSafe(account->_user_private);
    account->_user_private = private;

    bool resume_timer = false;
    if (!account->user_private_timer) {
        xpc_transaction_begin();
        resume_timer = true;
        account->user_private_timer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, account->queue);
        dispatch_source_set_event_handler(account->user_private_timer, ^{
            SOSAccountPurgePrivateCredential(account);
        });

        notify_register_dispatch(kUserKeybagStateChangeNotification, &account->lock_notification_token, account->queue, ^(int token) {
            bool locked = false;
            CFErrorRef lockCheckError = NULL;

            if (!SecAKSGetIsLocked(&locked, &lockCheckError)) {
                secerror("Checking for locked after change failed: %@", lockCheckError);
            }

            if (locked) {
                SOSAccountPurgePrivateCredential(account);
            }
        });
    }

    // (Re)set the timer's fire time to now + 120 seconds with a 5 second fuzz factor.
    dispatch_time_t purgeTime = dispatch_time(DISPATCH_TIME_NOW, (int64_t)(10 * 60 * NSEC_PER_SEC));
    dispatch_source_set_timer(account->user_private_timer, purgeTime, DISPATCH_TIME_FOREVER, (int64_t)(5 * NSEC_PER_SEC));
    if (resume_timer)
        dispatch_resume(account->user_private_timer);
}

SecKeyRef SOSAccountGetPrivateCredential(SOSAccountRef account, CFErrorRef* error)
{
    if (account->_user_private == NULL) {
        SOSCreateError(kSOSErrorPrivateKeyAbsent, CFSTR("Private Key not available - failed to prompt user recently"), NULL, error);
    }
    return account->_user_private;
}

static bool SOSAccountHasPublicKey(SOSAccountRef account, CFErrorRef* error)
{
    if (account->user_public == NULL || account->user_public_trusted == false) {
        SOSCreateError(kSOSErrorPublicKeyAbsent, CFSTR("Public Key not available - failed to register before call"), NULL, error);
        return false;
    }

    return true;
}

static bool SOSAccountIsMyPeerActiveInCircle(SOSAccountRef account, SOSCircleRef circle, CFErrorRef* error);

static void SOSAccountGenerationSignatureUpdate(SOSAccountRef account, SecKeyRef privKey) {
    SOSAccountForEachCircle(account, ^(SOSCircleRef circle) {
        SOSFullPeerInfoRef fpi = SOSAccountGetMyFullPeerInCircle(account, circle, NULL);
        if(SOSCircleHasPeer(circle, SOSFullPeerInfoGetPeerInfo(fpi), NULL) &&
           !SOSCircleVerify(circle, account->user_public, NULL)) {
            SOSAccountModifyCircle(account, SOSCircleGetName(circle), NULL, ^(SOSCircleRef circle) {
                SOSFullPeerInfoRef cloud_fpi = SOSCircleGetiCloudFullPeerInfoRef(circle);
                require_quiet(cloud_fpi != NULL, gen_sign);
                require_quiet(SOSFullPeerInfoUpgradeSignatures(cloud_fpi, privKey, NULL), gen_sign);
                if(!SOSCircleUpdatePeerInfo(circle, SOSFullPeerInfoGetPeerInfo(cloud_fpi))) {
                }
            gen_sign: // finally generation sign this.
                SOSCircleGenerationSign(circle, privKey, fpi, NULL);
                account->departure_code = kSOSNeverLeftCircle;
            });
        }
    });
}

/* this one is meant to be local - not published over KVS. */
static void SOSAccountPeerSignatureUpdate(SOSAccountRef account, SecKeyRef privKey) {
    SOSAccountForEachCircle(account, ^(SOSCircleRef circle) {
        SOSFullPeerInfoRef fpi = SOSAccountGetMyFullPeerInCircle(account, circle, NULL);
        SOSFullPeerInfoUpgradeSignatures(fpi, privKey, NULL);
    });
}

static void SOSAccountSetPreviousPublic(SOSAccountRef account) {
    CFReleaseNull(account->previous_public);
    account->previous_public = account->user_public;
    CFRetain(account->previous_public);
}

static void SOSAccountSetTrustedUserPublicKey(SOSAccountRef account, bool public_was_trusted, SecKeyRef privKey)
{
    if (!privKey) return;
    SecKeyRef publicKey = SecKeyCreatePublicFromPrivate(privKey);

    if (account->user_public && account->user_public_trusted && CFEqual(publicKey, account->user_public)) return;

    if(public_was_trusted && account->user_public) {
        CFReleaseNull(account->previous_public);
        account->previous_public = account->user_public;
        CFRetain(account->previous_public);
    }
    
    CFReleaseNull(account->user_public);
    account->user_public = publicKey;
    account->user_public_trusted = true;
    
    if(!account->previous_public) {
        account->previous_public = account->user_public;
        CFRetain(account->previous_public);
    }

	secnotice("trust", "trusting new public key: %@", account->user_public);
}

static void SOSAccountProcessDeferredUpdates(SOSAccountRef account) {
    CFErrorRef error = NULL;
    if (account->deferred_updates && !SOSAccountHandleUpdates(account, account->deferred_updates, &error))
        secerror("Failed to handle updates when setting public key (%@)", error);

    CFReleaseNull(account->deferred_updates);
}


bool SOSAccountTryUserCredentials(SOSAccountRef account, CFStringRef user_account __unused, CFDataRef user_password, CFErrorRef *error)
{
    bool success = false;

    if (!SOSAccountHasPublicKey(account, error))
        return false;
    
    if (account->user_key_parameters) {
        SecKeyRef new_key = SOSUserKeygen(user_password, account->user_key_parameters, error);
        if (new_key) {
            SecKeyRef new_public_key = SecKeyCreatePublicFromPrivate(new_key);

            if (CFEqualSafe(new_public_key, account->user_public)) {
                SOSAccountSetPrivateCredential(account, new_key);
                success = true;
            } else {
                SOSCreateError(kSOSErrorWrongPassword, CFSTR("Password passed in incorrect: ▇█████▇▇██"), NULL, error);
            }
            CFReleaseSafe(new_public_key);
            CFReleaseSafe(new_key);
        }
    } else {
        SOSCreateError(kSOSErrorProcessingFailure, CFSTR("Have public key but no parameters??"), NULL, error);
    }

    return success;
}

static bool SOSAccountPublishCloudParameters(SOSAccountRef account, CFErrorRef* error)
{
    bool success = false;
    CFMutableDataRef cloudParameters = CFDataCreateMutableWithScratch(kCFAllocatorDefault,
                                                                      der_sizeof_cloud_parameters(account->user_public,
                                                                                                  account->user_key_parameters,
                                                                                                  error));
    if (der_encode_cloud_parameters(account->user_public, account->user_key_parameters, error,
                                    CFDataGetMutableBytePtr(cloudParameters),
                                    CFDataGetMutablePastEndPtr(cloudParameters)) != NULL) {

        CFDictionaryRef changes = CFDictionaryCreateForCFTypes(kCFAllocatorDefault,
                                                               kSOSKVSKeyParametersKey, cloudParameters,
                                                               NULL);

        CFErrorRef changeError = NULL;
        if (account->update_block(changes, &changeError)) {
            success = true;
        } else {
            SOSCreateErrorWithFormat(kSOSErrorSendFailure, changeError, error, NULL,
                                     CFSTR("update parameters key failed [%@]"), changes);
        }
        CFReleaseSafe(changes);
        CFReleaseSafe(changeError);
    } else {
        SOSCreateError(kSOSErrorEncodeFailure, CFSTR("Encoding parameters failed"), NULL, error);
    }

    CFReleaseNull(cloudParameters);

    return success;
}

bool SOSAccountAssertUserCredentials(SOSAccountRef account, CFStringRef user_account __unused, CFDataRef user_password, CFErrorRef *error)
{
    bool public_was_trusted = account->user_public_trusted;
    account->user_public_trusted = false;
    SecKeyRef user_private = NULL;

    if (account->user_public && account->user_key_parameters) {
        // We have an untrusted public key – see if our generation makes the same key:
        // if so we trust it and we have the private key.
        // if not we still don't trust it.
        require_quiet(user_private = SOSUserKeygen(user_password, account->user_key_parameters, error), exit);
        SecKeyRef public_candidate = SecKeyCreatePublicFromPrivate(user_private);
        if (!CFEqualSafe(account->user_public, public_candidate)) {
            secnotice("trust", "Public keys don't match:  calculated: %@, expected: %@",
                      account->user_public, public_candidate);
            debugDumpUserParameters(CFSTR("params"), account->user_key_parameters);
            CFReleaseNull(user_private);
        } else {
            SOSAccountPeerSignatureUpdate(account, user_private);
            SOSAccountSetTrustedUserPublicKey(account, public_was_trusted, user_private);
        }
        CFReleaseSafe(public_candidate);
    }

    if (!account->user_public_trusted) {
        // We may or may not have parameters here.
        // In any case we tried using them and they didn't match
        // So forget all that and start again, assume we're the first to push anything useful.
        
        CFReleaseNull(account->user_key_parameters);
        account->user_key_parameters = SOSUserKeyCreateGenerateParameters(error);
        require_quiet(user_private = SOSUserKeygen(user_password, account->user_key_parameters, error), exit);

        SOSAccountPeerSignatureUpdate(account, user_private);
        SOSAccountSetTrustedUserPublicKey(account, public_was_trusted, user_private);

        CFErrorRef publishError = NULL;
        if (!SOSAccountPublishCloudParameters(account, &publishError))
            secerror("Failed to publish new cloud parameters: %@", publishError);
        CFReleaseSafe(publishError);
    }

    SOSAccountProcessDeferredUpdates(account);
    SOSAccountGenerationSignatureUpdate(account, user_private);
    SOSAccountSetPrivateCredential(account, user_private);
exit:
    CFReleaseSafe(user_private);

    return account->user_public_trusted;
}

//
// MARK: Circle management
//

int SOSAccountCountCircles(SOSAccountRef a) {
    assert(a);
    assert(a->circle_identities);
    assert(a->circles);
    return (int)CFDictionaryGetCount(a->circles);
}

static SecKeyRef GeneratePermanentFullECKey_internal(int keySize, CFStringRef name, CFTypeRef accessibility, CFBooleanRef sync,  CFErrorRef* error)
{    
    SecKeyRef full_key = NULL;
    
    CFNumberRef key_size_num = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &keySize);

    CFDictionaryRef priv_key_attrs = CFDictionaryCreateForCFTypes(kCFAllocatorDefault,
                                                                  kSecAttrIsPermanent,    kCFBooleanTrue,
                                                                  NULL);
    
    CFDictionaryRef keygen_parameters = CFDictionaryCreateForCFTypes(kCFAllocatorDefault,
        kSecAttrKeyType,        kSecAttrKeyTypeEC,
        kSecAttrKeySizeInBits,  key_size_num,
        kSecPrivateKeyAttrs,    priv_key_attrs,
        kSecAttrAccessible,     accessibility,
        kSecAttrAccessGroup,    kSOSInternalAccessGroup,
        kSecAttrLabel,          name,
        kSecAttrSynchronizable, sync,
        kSecUseTombstones,      kCFBooleanTrue,
        NULL);

    CFReleaseNull(priv_key_attrs);

    CFReleaseNull(key_size_num);
    OSStatus status = SecKeyGeneratePair(keygen_parameters, NULL, &full_key);
    CFReleaseNull(keygen_parameters);
    
    if (status)
        secerror("status: %ld", (long)status);
    if (status != errSecSuccess && error != NULL && *error == NULL) {
        *error = CFErrorCreate(kCFAllocatorDefault, kCFErrorDomainOSStatus, status, NULL);
    }
    
    return full_key;
}

static SecKeyRef GeneratePermanentFullECKey(int keySize, CFStringRef name, CFErrorRef* error) {
    return GeneratePermanentFullECKey_internal(keySize, name, kSecAttrAccessibleWhenUnlockedThisDeviceOnly, kCFBooleanFalse, error);
}

static SecKeyRef GeneratePermanentFullECKeyForCloudIdentity(int keySize, CFStringRef name, CFErrorRef* error) {
    return GeneratePermanentFullECKey_internal(keySize, name, kSecAttrAccessibleWhenUnlocked, kCFBooleanTrue, error);
}


SOSFullPeerInfoRef SOSAccountGetMyFullPeerInCircleNamed(SOSAccountRef account, CFStringRef name, CFErrorRef *error) {
    if (CFDictionaryGetValue(account->circles, name) == NULL) {
        SOSCreateErrorWithFormat(kSOSErrorNoCircle, NULL, error, NULL, CFSTR("No circle named '%@'"), name);
        return NULL;
    }
    SOSFullPeerInfoRef circle_full_peer_info = (SOSFullPeerInfoRef) CFDictionaryGetValue(account->circle_identities, name);
    

    if (circle_full_peer_info == NULL) {
        CFStringRef keyName = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("ID for %@-%@"), SOSPeerGestaltGetName(account->gestalt), name);
        SecKeyRef full_key = GeneratePermanentFullECKey(256, keyName, error);
        CFReleaseNull(keyName);

        if (full_key) {
            circle_full_peer_info = SOSFullPeerInfoCreate(kCFAllocatorDefault, account->gestalt, full_key, error);
    
            CFReleaseNull(full_key);

            if (!circle_full_peer_info) {
                secerror("Can't make FullPeerInfo for %@-%@ (%@) - is AKS ok?", SOSPeerGestaltGetName(account->gestalt), name, error ? (void*)*error : (void*)CFSTR("-"));
                return circle_full_peer_info;
            }
            
            CFDictionarySetValue(account->circle_identities, name, circle_full_peer_info);
            CFReleaseNull(circle_full_peer_info);
            circle_full_peer_info = (SOSFullPeerInfoRef) CFDictionaryGetValue(account->circle_identities, name);
        }
        else
            secerror("No full_key: %@:", error ? *error : NULL);
    }
    
    return circle_full_peer_info;
}

static bool SOSAccountDestroyCirclePeerInfoNamed(SOSAccountRef account, CFStringRef name, CFErrorRef* error) {
    if (CFDictionaryGetValue(account->circles, name) == NULL) {
        SOSCreateErrorWithFormat(kSOSErrorNoCircle, NULL, error, NULL, CFSTR("No circle named '%@'"), name);
        return false;
    }
    
    SOSFullPeerInfoRef circle_full_peer_info = (SOSFullPeerInfoRef) CFDictionaryGetValue(account->circle_identities, name);
    
    if (circle_full_peer_info) {
        SOSPeerPurgeAllFor(SOSPeerInfoGetPeerID(SOSFullPeerInfoGetPeerInfo(circle_full_peer_info)));
        
        SOSFullPeerInfoPurgePersistentKey(circle_full_peer_info, NULL);
    }
    
    CFDictionaryRemoveValue(account->circle_identities, name);
    
    return true;
}

static bool SOSAccountDestroyCirclePeerInfo(SOSAccountRef account, SOSCircleRef circle, CFErrorRef* error) {    
    return SOSAccountDestroyCirclePeerInfoNamed(account, SOSCircleGetName(circle), error);
}

SOSFullPeerInfoRef SOSAccountGetMyFullPeerInCircle(SOSAccountRef account, SOSCircleRef circle, CFErrorRef* error) {
    return SOSAccountGetMyFullPeerInCircleNamed(account, SOSCircleGetName(circle), error);
}

SOSPeerInfoRef SOSAccountGetMyPeerInCircle(SOSAccountRef account, SOSCircleRef circle, CFErrorRef* error) {
    SOSFullPeerInfoRef fpi =  SOSAccountGetMyFullPeerInCircleNamed(account, SOSCircleGetName(circle), error);
    
    return fpi ? SOSFullPeerInfoGetPeerInfo(fpi) : NULL;
}

SOSPeerInfoRef SOSAccountGetMyPeerInCircleNamed(SOSAccountRef account, CFStringRef name, CFErrorRef *error)
{
    SOSFullPeerInfoRef fpi =  SOSAccountGetMyFullPeerInCircleNamed(account, name, error);
    
    return fpi ? SOSFullPeerInfoGetPeerInfo(fpi) : NULL;
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

bool SOSAccountIsAccountIdentity(SOSAccountRef account, SOSPeerInfoRef peer_info, CFErrorRef *error)
{
    __block bool matches = false;
    CFDictionaryForEach(account->circle_identities, ^(const void *key, const void *value) {
        if (!matches) {
            matches = CFEqual(peer_info, SOSFullPeerInfoGetPeerInfo((SOSFullPeerInfoRef) value));
        }
    });
    
    return matches;
}

bool SOSAccountSyncWithAllPeers(SOSAccountRef account, CFErrorRef *error)
{
    __block bool result = true;
    SOSAccountForEachCircle(account, ^(SOSCircleRef circle) {
        if (!SOSAccountSyncWithAllPeersInCircle(account, circle, error))
            result = false;
    });
    
    return result;
}

bool SOSAccountSyncWithAllPeersInCircle(SOSAccountRef account, SOSCircleRef circle,
                                CFErrorRef *error)
{
    SOSPeerInfoRef my_peer = SOSAccountGetMyPeerInCircle(account, circle, error);
    if (!my_peer)
        return false;
    
    __block bool didSync = false;
    __block bool result = true;
    
    if (SOSCircleHasPeer(circle, my_peer, NULL)) {
        SOSCircleForEachPeer(circle, ^(SOSPeerInfoRef peer) {
            if (!CFEqual(SOSPeerInfoGetPeerID(my_peer), SOSPeerInfoGetPeerID(peer)))
            {
                bool local_didSync = false;
                if (!SOSAccountSyncWithPeer(account, circle, peer, &local_didSync, error))
                    result = false;
                if (!didSync && local_didSync)
                {
                    didSync = true;
                }
            }
        });
        
        if (didSync)
        {
            SetCloudKeychainTraceValueForKey(kCloudKeychainNumberOfTimesSyncedWithPeers, 1);
        }
    }
    
    return result;
}

bool SOSAccountSyncWithPeer(SOSAccountRef account, SOSCircleRef circle,
                            SOSPeerInfoRef thisPeer, bool* didSendData, CFErrorRef* error)
{
    CFStringRef peer_id = SOSPeerInfoGetPeerID(thisPeer);
    CFStringRef peer_write_key = SOSMessageKeyCreateWithAccountAndPeer(account, circle, peer_id);
    SOSFullPeerInfoRef myRef = SOSAccountGetMyFullPeerInCircle(account, circle, error);
    
    __block bool sentData = false;


    SOSPeerSendBlock writeToKVSKey = ^bool (CFDataRef data, CFErrorRef* error) {
        secnotice("account", "writing data of size %ld:", data?CFDataGetLength(data):0);
        sentData = (NULL != data);
        CFDictionaryRef writeToDo = CFDictionaryCreateForCFTypes(kCFAllocatorDefault, peer_write_key, data, NULL);
        bool written = account->update_block(writeToDo, error);
        if (account->processed_message_block)
            account->processed_message_block(circle, NULL, data);
        CFRelease(writeToDo);
        return written;
    };
    
    if (NULL != didSendData)
    {
        *didSendData = sentData;
    }
    
    bool result = SOSCircleSyncWithPeer(myRef, circle, account->factory, writeToKVSKey, peer_id, error);
    CFReleaseNull(peer_write_key);
    return result;
}

static bool SOSAccountIsActivePeerInCircleNamed(SOSAccountRef account, CFStringRef circle_name, CFStringRef peerid, CFErrorRef* error) {
    SOSCircleRef circle = SOSAccountFindCircle(account, circle_name, error);
    if(!circle) return false;
    return SOSCircleHasActivePeerWithID(circle, peerid, error);
}

static bool SOSAccountIsMyPeerActiveInCircle(SOSAccountRef account, SOSCircleRef circle, CFErrorRef* error) {
    SOSFullPeerInfoRef fpi = SOSAccountGetMyFullPeerInCircleNamedIfPresent(account, SOSCircleGetName(circle), NULL);
    if(!fpi) return false;
    return SOSCircleHasActivePeer(circle, SOSFullPeerInfoGetPeerInfo(fpi), error);
}

bool SOSAccountCleanupAfterPeer(SOSAccountRef account, size_t seconds, SOSCircleRef circle,
                                SOSPeerInfoRef cleanupPeer, CFErrorRef* error)
{
    if(!SOSAccountIsMyPeerActiveInCircle(account, circle, NULL)) return true;
    
    CFMutableDictionaryRef keysToWrite = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);

    SOSCircleForEachPeer(circle, ^(SOSPeerInfoRef circlePeer) {
        CFStringRef from_key = SOSMessageKeyCreateWithCircleAndPeerInfos(circle, cleanupPeer, circlePeer);
        CFStringRef to_key = SOSMessageKeyCreateWithCircleAndPeerInfos(circle, circlePeer, cleanupPeer);

        CFDictionaryAddValue(keysToWrite, from_key, kCFNull);
        CFDictionaryAddValue(keysToWrite, to_key, kCFNull);

        CFReleaseNull(from_key);
        CFReleaseNull(to_key);
    });
    
    if(SOSPeerInfoRetireRetirementTicket(seconds, cleanupPeer)) {
        CFStringRef resignationKey = SOSRetirementKeyCreateWithCircleAndPeer(circle, SOSPeerInfoGetPeerID(cleanupPeer));
        CFDictionarySetValue(keysToWrite, resignationKey, kCFNull);
        CFDictionaryRemoveValue(account->retired_peers, resignationKey);
        CFReleaseNull(resignationKey);
    }

    bool success = account->update_block(keysToWrite, error);

    CFReleaseNull(keysToWrite);

    return success;
}

bool SOSAccountCleanupRetirementTickets(SOSAccountRef account, size_t seconds, CFErrorRef* error) {
    CFMutableDictionaryRef keysToWrite = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
    
    CFDictionaryRef copyToIterate = CFDictionaryCreateCopy(kCFAllocatorDefault, account->retired_peers);

    CFDictionaryForEach(copyToIterate, ^(const void* resignationKey, const void* value) {
        CFStringRef circle_name = NULL;
        CFStringRef retiree_peerid = NULL;
        SOSPeerInfoRef pi = NULL;
        SOSKVSKeyType keytype = SOSKVSKeyGetKeyTypeAndParse(resignationKey, &circle_name, &retiree_peerid, NULL);
        require_quiet(keytype == kRetirementKey && circle_name && retiree_peerid && isData(value), forget);
        pi = SOSPeerInfoCreateFromData(NULL, error, (CFDataRef) value);
        require_quiet(pi && CFEqualSafe(retiree_peerid, SOSPeerInfoGetPeerID(pi)), forget);

        require_quiet(!SOSAccountIsActivePeerInCircleNamed(account, circle_name, retiree_peerid, NULL), keep);
        require_quiet(SOSPeerInfoRetireRetirementTicket(seconds, pi), keep);
        
        // Happy day, it's time and it's a ticket we should eradicate from KVS.
        CFDictionarySetValue(keysToWrite, resignationKey, kCFNull);

    forget:
        CFDictionaryRemoveValue(account->retired_peers, resignationKey);
    keep:
        CFReleaseSafe(pi);
        CFReleaseSafe(circle_name);
        CFReleaseSafe(retiree_peerid);
    });
    CFReleaseNull(copyToIterate);

    bool success = true;
    if(CFDictionaryGetCount(keysToWrite)) {
        success = account->update_block(keysToWrite, error);
    }
    CFReleaseNull(keysToWrite);
        
    return success;
}

bool SOSAccountScanForRetired(SOSAccountRef account, SOSCircleRef circle, CFErrorRef *error) {
    SOSCircleForEachRetiredPeer(circle, ^(SOSPeerInfoRef peer) {
        CFStringRef key = SOSRetirementKeyCreateWithCircleAndPeer(circle, SOSPeerInfoGetPeerID(peer));
        if(key && !CFDictionaryGetValueIfPresent(account->retired_peers, key, NULL)) {
            CFDataRef value = SOSPeerInfoCopyEncodedData(peer, NULL, NULL);
            if(value) {
                CFDictionarySetValue(account->retired_peers, key, value);
                SOSAccountCleanupAfterPeer(account, RETIREMENT_FINALIZATION_SECONDS, circle, peer, error);
            }
            CFReleaseSafe(value);
        }
        CFReleaseSafe(key);
    });
    return true;
}

SOSCircleRef SOSAccountCloneCircleWithRetirement(SOSAccountRef account, SOSCircleRef starting_circle, CFErrorRef *error) {
    CFStringRef circle_to_mod = SOSCircleGetName(starting_circle);
    SOSCircleRef new_circle = SOSCircleCopyCircle(NULL, starting_circle, error);
    if(!new_circle) return NULL;
    
    CFDictionaryForEach(account->retired_peers, ^(const void* resignationKey, const void* value) {
        CFStringRef circle_name = NULL;
        CFStringRef retiree_peerid = NULL;
        
        SOSKVSKeyType keytype = SOSKVSKeyGetKeyTypeAndParse(resignationKey, &circle_name, &retiree_peerid, NULL);
        if(keytype == kRetirementKey && CFEqualSafe(circle_name, circle_to_mod) && SOSCircleHasPeerWithID(new_circle, retiree_peerid, NULL)) {
            if(isData(value)) {
                SOSPeerInfoRef pi = SOSPeerInfoCreateFromData(NULL, error, (CFDataRef) value);
                SOSCircleUpdatePeerInfo(new_circle, pi);
                CFReleaseSafe(pi);
            }
        }
        CFReleaseSafe(circle_name);
        CFReleaseSafe(retiree_peerid);
    });
    
    if(SOSCircleCountPeers(new_circle) == 0) {
        SOSCircleResetToEmpty(new_circle, NULL);
    }
    
    return new_circle;
}


//
// Circle Finding
//
SOSCircleRef SOSAccountFindCompatibleCircle(SOSAccountRef a, CFStringRef name)
{
    CFTypeRef entry = CFDictionaryGetValue(a->circles, name);
    
    if (CFGetTypeID(entry) == SOSCircleGetTypeID())
        return (SOSCircleRef) entry;
    
    return NULL;
}

SOSCircleRef SOSAccountFindCircle(SOSAccountRef a, CFStringRef name, CFErrorRef *error)
{
    CFTypeRef entry = CFDictionaryGetValue(a->circles, name);
    
    require_action_quiet(!isNull(entry), fail,
                         SOSCreateError(kSOSErrorIncompatibleCircle, CFSTR("Incompatible circle in KVS"), NULL, error));

    require_action_quiet(entry, fail,
                         SOSCreateError(kSOSErrorNoCircle, CFSTR("No circle found"), NULL, error));

    
    return (SOSCircleRef) entry;

fail:
    return NULL;
}

SOSCircleRef SOSAccountEnsureCircle(SOSAccountRef a, CFStringRef name, CFErrorRef *error)
{
    CFErrorRef localError = NULL;

    SOSCircleRef circle = SOSAccountFindCircle(a, name, &localError);
    
    require_action_quiet(circle || !isSOSErrorCoded(localError, kSOSErrorIncompatibleCircle), fail,
                         if (error) { *error = localError; localError = NULL; });
    

    if (NULL == circle) {
        circle = SOSCircleCreate(NULL, name, NULL);
        if (circle){
            CFDictionaryAddValue(a->circles, name, circle);
            CFRelease(circle);
            circle = SOSAccountFindCircle(a, name, &localError);
        }
        SOSUpdateKeyInterest(a, false, NULL);
    }

fail:
    CFReleaseNull(localError);
    return circle;
}


void SOSAccountAddChangeBlock(SOSAccountRef a, SOSAccountCircleMembershipChangeBlock changeBlock)
{
    CFArrayAppendValue(a->change_blocks, changeBlock);
}

void SOSAccountRemoveChangeBlock(SOSAccountRef a, SOSAccountCircleMembershipChangeBlock changeBlock)
{
    CFArrayRemoveAllValue(a->change_blocks, changeBlock);
}

static void DifferenceAndCall(CFArrayRef old_members, CFArrayRef new_members, void (^updatedCircle)(CFArrayRef additions, CFArrayRef removals))
{
    CFMutableArrayRef additions = CFArrayCreateMutableCopy(kCFAllocatorDefault, 0, new_members);
    CFMutableArrayRef removals = CFArrayCreateMutableCopy(kCFAllocatorDefault, 0, old_members);
    

    CFArrayForEach(old_members, ^(const void * value) {
        CFArrayRemoveAllValue(additions, value);
    });
    
    CFArrayForEach(new_members, ^(const void * value) {
        CFArrayRemoveAllValue(removals, value);
    });

    updatedCircle(additions, removals);
    
    CFReleaseSafe(additions);
    CFReleaseSafe(removals);
}

static void SOSAccountNotifyOfChange(SOSAccountRef account, SOSCircleRef oldCircle, SOSCircleRef newCircle)
{
    CFMutableArrayRef old_members = SOSCircleCopyPeers(oldCircle, kCFAllocatorDefault);
    CFMutableArrayRef new_members = SOSCircleCopyPeers(newCircle, kCFAllocatorDefault);
    
    CFMutableArrayRef old_applicants = SOSCircleCopyApplicants(oldCircle, kCFAllocatorDefault);
    CFMutableArrayRef new_applicants = SOSCircleCopyApplicants(newCircle, kCFAllocatorDefault);
    
    DifferenceAndCall(old_members, new_members, ^(CFArrayRef added_members, CFArrayRef removed_members) {
        DifferenceAndCall(old_applicants, new_applicants, ^(CFArrayRef added_applicants, CFArrayRef removed_applicants) {
            CFArrayForEach(account->change_blocks, ^(const void * notificationBlock) {
                ((SOSAccountCircleMembershipChangeBlock) notificationBlock)(newCircle, added_members, removed_members, added_applicants, removed_applicants);
            });
        });
    });
    
    CFReleaseNull(old_applicants);
    CFReleaseNull(new_applicants);

    CFReleaseNull(old_members);
    CFReleaseNull(new_members);
}

void SOSAccountForEachCircle(SOSAccountRef account, void (^process)(SOSCircleRef circle))
{
    CFDictionaryForEach(account->circles, ^(const void* key, const void* value) {
        assert(value);
        process((SOSCircleRef)value);
    });
}

static void AppendCircleKeyName(CFMutableArrayRef array, CFStringRef name) {
    CFStringRef circle_key = SOSCircleKeyCreateWithName(name, NULL);
    CFArrayAppendValue(array, circle_key);
    CFReleaseNull(circle_key);
}

static inline void AppendCircleInterests(CFMutableArrayRef circle_keys, CFMutableArrayRef retiree_keys, CFMutableArrayRef message_keys, SOSCircleRef circle, SOSFullPeerInfoRef me) {
    CFStringRef my_peer_id = NULL;
    
    if (me) {
        SOSPeerInfoRef my_peer = me ? SOSFullPeerInfoGetPeerInfo(me) : NULL;
        my_peer_id = SOSPeerInfoGetPeerID(my_peer);
    }
    
    if (circle_keys) {
        CFStringRef circleName = SOSCircleGetName(circle);
        AppendCircleKeyName(circle_keys, circleName);
    }

    SOSCircleForEachPeer(circle, ^(SOSPeerInfoRef peer) {
        if (!CFEqualSafe(my_peer_id, SOSPeerInfoGetPeerID(peer))) {
            CFStringRef peer_name = SOSPeerInfoGetPeerID(peer);
            if (retiree_keys) {
                CFStringRef retirementKey = SOSRetirementKeyCreateWithCircleAndPeer(circle, peer_name);
                CFArrayAppendValue(retiree_keys, retirementKey);
                CFReleaseNull(retirementKey);
            }
            
            if (my_peer_id && message_keys) {
                CFStringRef messageKey = SOSMessageKeyCreateWithCircleAndPeerNames(circle, peer_name, my_peer_id);
                CFArrayAppendValue(message_keys, messageKey);
                CFRelease(messageKey);
            }
        }
    });
}

static void SOSAccountCopyKeyInterests(SOSAccountRef account,
                                       CFMutableArrayRef alwaysKeys,
                                       CFMutableArrayRef afterFirstUnlockKeys,
                                       CFMutableArrayRef whenUnlockedKeys)
{
    CFArrayAppendValue(afterFirstUnlockKeys, kSOSKVSKeyParametersKey);
    
    SOSAccountForEachKnownCircle(account, ^(CFStringRef name) {
        AppendCircleKeyName(afterFirstUnlockKeys, name);
    }, ^(SOSCircleRef circle) {
        AppendCircleInterests(afterFirstUnlockKeys, afterFirstUnlockKeys, whenUnlockedKeys, circle, NULL);
    }, ^(SOSCircleRef circle, SOSFullPeerInfoRef full_peer) {
        bool inCircle = SOSCircleHasPeer(circle, SOSFullPeerInfoGetPeerInfo(full_peer), NULL);

        AppendCircleInterests(afterFirstUnlockKeys, afterFirstUnlockKeys, inCircle ? whenUnlockedKeys : NULL, circle, full_peer);
    });
}

static bool SOSUpdateKeyInterest(SOSAccountRef account, bool getNewKeysOnly, CFErrorRef *error)
{
    if (account->update_interest_block) {

        CFMutableArrayRef alwaysKeys = CFArrayCreateMutableForCFTypes(kCFAllocatorDefault);
        CFMutableArrayRef afterFirstUnlockKeys = CFArrayCreateMutableForCFTypes(kCFAllocatorDefault);
        CFMutableArrayRef whenUnlockedKeys = CFArrayCreateMutableForCFTypes(kCFAllocatorDefault);

        SOSAccountCopyKeyInterests(account, alwaysKeys, afterFirstUnlockKeys, whenUnlockedKeys);

        account->update_interest_block(getNewKeysOnly, alwaysKeys, afterFirstUnlockKeys, whenUnlockedKeys);
    
        CFReleaseNull(alwaysKeys);
        CFReleaseNull(afterFirstUnlockKeys);
        CFReleaseNull(whenUnlockedKeys);
    }
    
    return true;
}

static bool SOSAccountSendPendingChanges(SOSAccountRef account, CFErrorRef *error) {
    CFErrorRef changeError = NULL;
    
    if (CFDictionaryGetCount(account->pending_changes) == 0)
        return true;
    
    bool success = account->update_block(account->pending_changes, &changeError);
    if (success) {
        CFDictionaryRemoveAllValues(account->pending_changes);
    } else {
        SOSCreateErrorWithFormat(kSOSErrorSendFailure, changeError, error, NULL,
                                 CFSTR("Send changes block failed [%@]"), account->pending_changes);
    }
    
    return success;
}

static bool SOSAccountAddCircleToPending(SOSAccountRef account, SOSCircleRef circle, CFErrorRef *error)
{
    bool success = false;
    CFDataRef circle_data = SOSCircleCopyEncodedData(circle, kCFAllocatorDefault, error);
    
    if (circle_data) {
        CFStringRef circle_key = SOSCircleKeyCreateWithCircle(circle, NULL);

        CFDictionarySetValue(account->pending_changes, circle_key, circle_data);
        success = true;
        
        CFReleaseNull(circle_data);
        CFReleaseNull(circle_key);
    }
    
    return success;
}


static void SOSAccountRecordRetiredPeerInCircleNamed(SOSAccountRef account, CFStringRef circleName, SOSPeerInfoRef retiree)
{
    // Replace Peer with RetiredPeer, if were a peer.
    SOSAccountModifyCircle(account, circleName, NULL, ^(SOSCircleRef circle) {
        if (SOSCircleUpdatePeerInfo(circle, retiree)) {
            CFErrorRef cleanupError = NULL;
            SOSAccountCleanupAfterPeer(account, RETIREMENT_FINALIZATION_SECONDS, circle, retiree, &cleanupError);
            secerror("Error cleanup up after peer (%@): %@", retiree, cleanupError);
            CFReleaseSafe(cleanupError);
        }
    });
}

static bool sosAccountLeaveCircle(SOSAccountRef account, SOSCircleRef circle, CFErrorRef* error) {
    SOSFullPeerInfoRef fpi = SOSAccountGetMyFullPeerInCircle(account, circle, NULL);
    if(!fpi) return false;
	CFErrorRef localError = NULL;
	SOSPeerInfoRef retire_peer = SOSFullPeerInfoPromoteToRetiredAndCopy(fpi, &localError);;
	CFStringRef retire_key = SOSRetirementKeyCreateWithCircleAndPeer(circle, SOSPeerInfoGetPeerID(retire_peer));
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
            SOSAccountCleanupAfterPeer(account, RETIREMENT_FINALIZATION_SECONDS, circle, retire_peer, &cleanupError);
            secerror("Error cleanup up after peer (%@): %@", retire_peer, cleanupError);
            CFReleaseSafe(cleanupError);
        }
        writeCircle = true;
    }
    
    // Store the retirement record locally.
    CFDictionarySetValue(account->retired_peers, retire_key, retire_value);

    // Write pending change to KVS
    CFDictionarySetValue(account->pending_changes, retire_key, retire_value);
    
    // Kill peer key but don't return error if we can't.
    if(!SOSAccountDestroyCirclePeerInfo(account, circle, &localError))
        secerror("Couldn't purge key for peer %@ on retirement: %@", fpi, localError);

    if (writeCircle) {
        SOSAccountAddCircleToPending(account, circle, NULL);
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
// MARK: Handle Circle Updates
//


static bool SOSAccountHandleUpdateCircle(SOSAccountRef account, SOSCircleRef prospective_circle, bool writeUpdate, bool initialSync, CFErrorRef *error)
{
    bool success = true;

    secnotice("signing", "start: %@", prospective_circle);
    if (!account->user_public || !account->user_public_trusted) {
        SOSCreateError(kSOSErrorPublicKeyAbsent, CFSTR("Can't handle updates with no trusted public key here"), NULL, error);
        return false;
    }
    
    if (!prospective_circle) {
        secerror("##### Can't update to a NULL circle ######");
        return false; // Can't update one we don't have.
    }
    
    CFStringRef newCircleName = SOSCircleGetName(prospective_circle);
    SOSCircleRef oldCircle = SOSAccountFindCompatibleCircle(account, newCircleName);
    SOSFullPeerInfoRef me_full = SOSAccountGetMyFullPeerInCircle(account, oldCircle, NULL);
    SOSPeerInfoRef me = SOSFullPeerInfoGetPeerInfo(me_full);
    
    if (initialSync)
        secerror("##### Processing initial sync. Old (local) circle: %@, New (cloud) circle: %@", oldCircle, prospective_circle);

    if (!oldCircle)
        return false; // Can't update one we don't have.
    
    SOSAccountScanForRetired(account, prospective_circle, error);
    SOSCircleRef newCircle = SOSAccountCloneCircleWithRetirement(account, prospective_circle, error);
    if(!newCircle) return false;
    
    SOSCircleUpdatePeerInfo(newCircle, me);
    
    typedef enum {
        accept,
        countersign,
        leave,
        revert,
        ignore
    } circle_action_t;
    
    circle_action_t circle_action = ignore;
    enum DepartureReason leave_reason = kSOSNeverLeftCircle;
    
    SecKeyRef old_circle_key = NULL;
    if(SOSCircleVerify(oldCircle, account->user_public, NULL)) old_circle_key = account->user_public;
    else if(account->previous_public && SOSCircleVerify(oldCircle, account->previous_public, NULL)) old_circle_key = account->previous_public;
    bool userTrustedOldCircle = (old_circle_key != NULL);
    
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
            secerror("##### No trusted peer signature found, accepting hoping for concordance later %@", newCircle);
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

    secnotice("signing", "Decided on action %d based on concordance state %d and %s circle.", circle_action, concstat, userTrustedOldCircle ? "trusted" : "untrusted");

    SOSCircleRef circleToPush = NULL;

    if (circle_action == leave) {
        circle_action = ignore;

        if (me && SOSCircleHasPeer(oldCircle, me, NULL)) {
            if (sosAccountLeaveCircle(account, newCircle, error)) {
                account->departure_code = leave_reason;
                circleToPush = newCircle;
                circle_action = accept;
                me = NULL;
                me_full = NULL;
            }
        }
        else {
            // We are not in this circle, but we need to update account with it, since we got it from cloud
            secnotice("updatecircle", "We are not in this circle, but we need to update account with it");
            circle_action = accept;
        }
    }

    if (circle_action == countersign) {
        if (me && SOSCircleHasPeer(newCircle, me, NULL) && !SOSCircleVerifyPeerSigned(newCircle, me, NULL)) {
            CFErrorRef signing_error = NULL;

            if (me_full && SOSCircleConcordanceSign(newCircle, me_full, &signing_error)) {
                circleToPush = newCircle;
                secnotice("signing", "Concurred with: %@", newCircle);
            } else {
                secerror("Failed to concurrence sign, error: %@  Old: %@ New: %@", signing_error, oldCircle, newCircle);
            }
            CFReleaseSafe(signing_error);
        }
        circle_action = accept;
    }

    if (circle_action == accept) {
        if (me && SOSCircleHasActivePeer(oldCircle, me, NULL) && !SOSCircleHasPeer(newCircle, me, NULL)) {
            //  Don't destroy evidence of other code determining reason for leaving.
            if(!SOSAccountHasLeft(account)) account->departure_code = kSOSMembershipRevoked;
        }

        if (me
            && SOSCircleHasActivePeer(oldCircle, me, NULL)
            && !(SOSCircleCountPeers(oldCircle) == 1 && SOSCircleHasPeer(oldCircle, me, NULL)) // If it was our offering, don't change ID to avoid ghosts
            && !SOSCircleHasPeer(newCircle, me, NULL) && !SOSCircleHasApplicant(newCircle, me, NULL)) {
            secnotice("circle", "Purging my peer (ID: %@) for circle '%@'!!!", SOSPeerInfoGetPeerID(me), SOSCircleGetName(oldCircle));
            SOSAccountDestroyCirclePeerInfo(account, oldCircle, NULL);
            me = NULL;
            me_full = NULL;
        }
        
        if (me && SOSCircleHasRejectedApplicant(newCircle, me, NULL)) {
            SOSPeerInfoRef  reject = SOSCircleCopyRejectedApplicant(newCircle, me, NULL);
            if(CFEqualSafe(reject, me) && SOSPeerInfoApplicationVerify(me, account->user_public, NULL)) {
                secnotice("circle", "Rejected, Purging my applicant peer (ID: %@) for circle '%@'", SOSPeerInfoGetPeerID(me), SOSCircleGetName(oldCircle));
                SOSAccountDestroyCirclePeerInfo(account, oldCircle, NULL);
                me = NULL;
                me_full = NULL;
            } else {
                SOSCircleRequestReadmission(newCircle, account->user_public, me_full, NULL);
                writeUpdate = true;
            }
        }
        
        CFRetain(oldCircle); // About to replace the oldCircle
        CFDictionarySetValue(account->circles, newCircleName, newCircle);
        SOSAccountSetPreviousPublic(account);
        
        secnotice("signing", "%@, Accepting circle: %@", concStr, newCircle);
        
        if (me_full && account->user_public_trusted
            && SOSCircleHasApplicant(oldCircle, me, NULL)
            && SOSCircleCountPeers(newCircle) > 0
            && !SOSCircleHasPeer(newCircle, me, NULL) && !SOSCircleHasApplicant(newCircle, me, NULL)) {
            // We weren't rejected (above would have set me to NULL.
            // We were applying and we weren't accepted.
            // Our application is declared lost, let us reapply.
            
            if (SOSCircleRequestReadmission(newCircle, account->user_public, me_full, NULL))
                writeUpdate = true;
        }
        
        if (me && SOSCircleHasActivePeer(oldCircle, me, NULL)) {
            SOSAccountCleanupRetirementTickets(account, RETIREMENT_FINALIZATION_SECONDS, NULL);
        }

        SOSAccountNotifyOfChange(account, oldCircle, newCircle);

        CFReleaseNull(oldCircle);

        if (writeUpdate)
            circleToPush = newCircle;

        success = SOSUpdateKeyInterest(account, true, error);
    }

    if (circle_action == revert) {
        secnotice("signing", "%@, Rejecting: %@ re-publishing %@", concStr, newCircle, oldCircle);
        
        circleToPush = oldCircle;
    }


    if (circleToPush != NULL) {
        success = (success
                   && SOSAccountAddCircleToPending(account, circleToPush, error)
                   && SOSAccountSendPendingChanges(account, error));
    }

    CFReleaseSafe(newCircle);

    return success;
}

static bool SOSAccountUpdateCircleFromRemote(SOSAccountRef account, SOSCircleRef newCircle, bool initialSync, CFErrorRef *error)
{
    return SOSAccountHandleUpdateCircle(account, newCircle, false, initialSync, error);
}

bool SOSAccountUpdateCircle(SOSAccountRef account, SOSCircleRef newCircle, CFErrorRef *error)
{
    return SOSAccountHandleUpdateCircle(account, newCircle, true, false, error);
}

bool SOSAccountModifyCircle(SOSAccountRef account,
                            CFStringRef circleName,
                            CFErrorRef* error,
                            void (^action)(SOSCircleRef circle))
{
    bool success = false;
    
    SOSCircleRef circle = NULL;
    SOSCircleRef accountCircle = SOSAccountFindCircle(account, circleName, error);
    require_quiet(accountCircle, fail);

    circle = SOSCircleCopyCircle(kCFAllocatorDefault, accountCircle, error);
    require_quiet(circle, fail);

    action(circle);
    success = SOSAccountUpdateCircle(account, circle, error);
    
fail:
    CFReleaseSafe(circle);
    return success;
}

static SOSCircleRef SOSAccountCreateCircleFrom(CFStringRef circleName, CFTypeRef value, CFErrorRef *error) {
    if (value && !isData(value) && !isNull(value)) {
        CFStringRef description = CFCopyTypeIDDescription(CFGetTypeID(value));
        SOSCreateErrorWithFormat(kSOSErrorUnexpectedType, NULL, error, NULL,
                                 CFSTR("Expected data or NULL got %@"), description);
        CFReleaseSafe(description);
        return NULL;
    }

    SOSCircleRef circle = NULL;
    if (!value || isNull(value)) {
        circle = SOSCircleCreate(kCFAllocatorDefault, circleName, error);
    } else {
        circle = SOSCircleCreateFromData(NULL, (CFDataRef) value, error);
        if (circle) {
            CFStringRef name = SOSCircleGetName(circle);
            if (!CFEqualSafe(name, circleName)) {
                SOSCreateErrorWithFormat(kSOSErrorNameMismatch, NULL, error, NULL,
                                     CFSTR("Expected circle named %@, got %@"), circleName, name);
                CFReleaseNull(circle);
            }
        }
    }
    return circle;
}

static SOSCCStatus SOSCCCircleStatus(SOSCircleRef circle)
{
    if (SOSCircleCountPeers(circle) == 0)
        return kSOSCCCircleAbsent;

    return kSOSCCNotInCircle;
}

static SOSCCStatus SOSCCThisDeviceStatusInCircle(SOSCircleRef circle, SOSPeerInfoRef this_peer)
{
    if (SOSCircleCountPeers(circle) == 0)
        return kSOSCCCircleAbsent;

    if (SOSCircleHasPeer(circle, this_peer, NULL))
        return kSOSCCInCircle;

    if (SOSCircleHasApplicant(circle, this_peer, NULL))
        return kSOSCCRequestPending;

    return kSOSCCNotInCircle;
}

static SOSCCStatus UnionStatus(SOSCCStatus accumulated_status, SOSCCStatus additional_circle_status)
{
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

SOSCCStatus SOSAccountIsInCircles(SOSAccountRef account, CFErrorRef* error)
{
    if (!SOSAccountHasPublicKey(account, error)) {
        return kSOSCCError;
    }

    __block bool set_once = false;
    __block SOSCCStatus status = kSOSCCInCircle;

    SOSAccountForEachKnownCircle(account, ^(CFStringRef name) {
        set_once = true;
        status = kSOSCCError;
        SOSCreateError(kSOSErrorIncompatibleCircle, CFSTR("Incompatible circle"), NULL, error);
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

static SOSPeerInfoRef GenerateNewCloudIdentityPeerInfo(CFErrorRef *error) {
    SecKeyRef cloud_key = GeneratePermanentFullECKeyForCloudIdentity(256, kicloud_identity_name, error);
    SOSPeerInfoRef cloud_peer = NULL;
    CFDictionaryRef query = NULL;
    CFDictionaryRef change = NULL;
    CFStringRef new_name = NULL;

    CFDictionaryRef gestalt = CFDictionaryCreateForCFTypes(kCFAllocatorDefault,
                                                           kPIUserDefinedDeviceName, CFSTR("iCloud"),
                                                           NULL);
    require_action_quiet(gestalt, fail, SecError(errSecAllocate, error, CFSTR("Can't allocate gestalt")));

    cloud_peer = SOSPeerInfoCreateCloudIdentity(kCFAllocatorDefault, gestalt, cloud_key, error);

    require(cloud_peer, fail);

    query = CFDictionaryCreateForCFTypes(kCFAllocatorDefault,
                                         kSecClass,             kSecClassKey,
                                         kSecAttrSynchronizable,kCFBooleanTrue,
                                         kSecUseTombstones,     kCFBooleanTrue,
                                         kSecValueRef,          cloud_key,
                                         NULL);

    new_name = CFStringCreateWithFormat(kCFAllocatorDefault, NULL,
                                        CFSTR("Cloud Identity - '%@'"), SOSPeerInfoGetPeerID(cloud_peer));
    
    change = CFDictionaryCreateForCFTypes(kCFAllocatorDefault,
                                          kSecAttrLabel,        new_name,
                                          NULL);
    
    SecError(SecItemUpdate(query, change), error, CFSTR("Couldn't update name"));

fail:
    CFReleaseNull(new_name);
    CFReleaseNull(query);
    CFReleaseNull(change);
    CFReleaseNull(gestalt);
    CFReleaseNull(cloud_key);

    return cloud_peer;
}

static SOSFullPeerInfoRef CopyCloudKeychainIdentity(SOSPeerInfoRef cloudPeer, CFErrorRef *error) {    
    return SOSFullPeerInfoCreateCloudIdentity(NULL, cloudPeer, error);
}

static bool SOSAccountResetThisCircleToOffering(SOSAccountRef account, SOSCircleRef circle, SecKeyRef user_key, CFErrorRef *error) {
    SOSFullPeerInfoRef myCirclePeer = SOSAccountGetMyFullPeerInCircle(account, circle, error);
    if (!myCirclePeer)
        return false;

    SOSAccountModifyCircle(account, SOSCircleGetName(circle), error, ^(SOSCircleRef circle) {
        bool result = false;
        SOSFullPeerInfoRef cloud_identity = NULL;
        CFErrorRef localError = NULL;

        require_quiet(SOSCircleResetToOffering(circle, user_key, myCirclePeer, &localError), err_out);

        {
            SOSPeerInfoRef cloud_peer = GenerateNewCloudIdentityPeerInfo(error);
            require_quiet(cloud_peer, err_out);
            cloud_identity = CopyCloudKeychainIdentity(cloud_peer, error);
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
    });

    return true;
}

static bool SOSAccountJoinThisCircle(SOSAccountRef account, SecKeyRef user_key,
                                     SOSCircleRef circle, bool use_cloud_peer, CFErrorRef* error) {
    __block bool result = false;
    __block SOSFullPeerInfoRef cloud_full_peer = NULL;

    SOSFullPeerInfoRef myCirclePeer = SOSAccountGetMyFullPeerInCircle(account, circle, error);
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
    }, ^(SOSCircleRef circle) {                                 // No Peer
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


bool SOSAccountJoinCirclesAfterRestore(SOSAccountRef account, CFErrorRef* error) {
    return SOSAccountJoinCircles_internal(account, true, error);
}


bool SOSAccountLeaveCircles(SOSAccountRef account, CFErrorRef* error)
{
    __block bool result = true;
    
    SOSAccountForEachKnownCircle(account, NULL, NULL, ^(SOSCircleRef circle, SOSFullPeerInfoRef myCirclePeer) {
        SOSAccountModifyCircle(account, SOSCircleGetName(circle), error, ^(SOSCircleRef circle) {
		result = sosAccountLeaveCircle(account, circle, error); // TODO: What about multiple errors!
		});
    });

    account->departure_code = kSOSWithdrewMembership;

    return SOSAccountSendPendingChanges(account, error) && result;
}

bool SOSAccountBail(SOSAccountRef account, uint64_t limit_in_seconds, CFErrorRef* error)
{
    dispatch_queue_t queue = dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0);
    dispatch_group_t group = dispatch_group_create();
    __block bool result = false;
    
    secnotice("circle", "Attempting to leave circle - best effort - in %llu seconds\n", limit_in_seconds);
    // Add a task to the group
    dispatch_group_async(group, queue, ^{
        SOSAccountForEachKnownCircle(account, NULL, NULL, ^(SOSCircleRef circle, SOSFullPeerInfoRef myCirclePeer) {
            SOSAccountModifyCircle(account, SOSCircleGetName(circle), error, ^(SOSCircleRef circle) {
                result = sosAccountLeaveCircle(account, circle, error); // TODO: What about multiple errors!
            });
        });
        
        account->departure_code = kSOSWithdrewMembership;
        if(result) result = SOSAccountSendPendingChanges(account, error);
    });
    dispatch_time_t milestone = dispatch_time(DISPATCH_TIME_NOW, limit_in_seconds * NSEC_PER_SEC);

    dispatch_group_wait(group, milestone);
    dispatch_release(group);
    return result;
}


static void for_each_applicant_in_each_circle(SOSAccountRef account, CFArrayRef peer_infos,
                                              void (^action)(SOSCircleRef circle, SOSFullPeerInfoRef myCirclePeer, SOSPeerInfoRef peer)) {
    SOSAccountForEachKnownCircle(account, NULL, NULL, ^(SOSCircleRef circle, SOSFullPeerInfoRef full_peer) {
        SOSPeerInfoRef me = SOSFullPeerInfoGetPeerInfo(full_peer);
        CFErrorRef peer_error = NULL;
        if (SOSCircleHasPeer(circle, me, &peer_error)) {
            CFArrayForEach(peer_infos, ^(const void *value) {
                SOSPeerInfoRef peer = (SOSPeerInfoRef) value;
                if (SOSCircleHasApplicant(circle, peer, NULL)) {
                    SOSAccountModifyCircle(account, SOSCircleGetName(circle), NULL, ^(SOSCircleRef circle) {
                        action(circle, full_peer, peer);
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
        if (!SOSCircleAcceptRequest(circle, user_key, myCirclePeer, peer, error))
            success = false;
		else
			num_peers = MAX(num_peers, SOSCircleCountPeers(circle));
    });
	
    return success;
}

bool SOSAccountRejectApplicants(SOSAccountRef account, CFArrayRef applicants, CFErrorRef* error) {
    __block bool success = true;
	__block int64_t num_peers = 0;

    for_each_applicant_in_each_circle(account, applicants, ^(SOSCircleRef circle, SOSFullPeerInfoRef myCirclePeer, SOSPeerInfoRef peer) {
        if (!SOSCircleRejectRequest(circle, myCirclePeer, peer, error))
            success = false;
		else
			num_peers = MAX(num_peers, SOSCircleCountPeers(circle));
    });

    return success;
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
        });
    });

    return result;
}

CFArrayRef SOSAccountCopyApplicants(SOSAccountRef account, CFErrorRef *error) {
    if (!SOSAccountHasPublicKey(account, error))
        return NULL;
    CFMutableArrayRef applicants = CFArrayCreateMutableForCFTypes(kCFAllocatorDefault);
 
    SOSAccountForEachCircle(account, ^(SOSCircleRef circle) {
        SOSCircleForEachApplicant(circle, ^(SOSPeerInfoRef peer) {
            CFArrayAppendValue(applicants, peer);
        });
    });

    return applicants;
}

CFArrayRef SOSAccountCopyPeers(SOSAccountRef account, CFErrorRef *error) {
    if (!SOSAccountHasPublicKey(account, error))
        return NULL;

    CFMutableArrayRef peers = CFArrayCreateMutableForCFTypes(kCFAllocatorDefault);

    SOSAccountForEachCircle(account, ^(SOSCircleRef circle) {
        SOSCircleForEachPeer(circle, ^(SOSPeerInfoRef peer) {
            CFArrayAppendValue(peers, peer);
        });
    });

    return peers;
}

CFArrayRef SOSAccountCopyActivePeers(SOSAccountRef account, CFErrorRef *error) {
    if (!SOSAccountHasPublicKey(account, error))
        return NULL;
    
    CFMutableArrayRef peers = CFArrayCreateMutableForCFTypes(kCFAllocatorDefault);
    
    SOSAccountForEachCircle(account, ^(SOSCircleRef circle) {
        SOSCircleForEachActivePeer(circle, ^(SOSPeerInfoRef peer) {
            CFArrayAppendValue(peers, peer);
        });
    });
    
    return peers;
}

CFArrayRef SOSAccountCopyActiveValidPeers(SOSAccountRef account, CFErrorRef *error) {
    if (!SOSAccountHasPublicKey(account, error))
        return NULL;
    
    CFMutableArrayRef peers = CFArrayCreateMutableForCFTypes(kCFAllocatorDefault);
    
    SOSAccountForEachCircle(account, ^(SOSCircleRef circle) {
        SOSCircleForEachActiveValidPeer(circle, account->user_public, ^(SOSPeerInfoRef peer) {
            CFArrayAppendValue(peers, peer);
        });
    });
    
    return peers;
}


CFArrayRef SOSAccountCopyConcurringPeers(SOSAccountRef account, CFErrorRef *error)
{
    if (!SOSAccountHasPublicKey(account, error))
        return NULL;

    CFMutableArrayRef concurringPeers = CFArrayCreateMutableForCFTypes(kCFAllocatorDefault);

    SOSAccountForEachCircle(account, ^(SOSCircleRef circle) {
        CFMutableArrayRef circleConcurring = SOSCircleCopyConcurringPeers(circle, NULL);
        CFArrayAppendArray(concurringPeers, circleConcurring, CFRangeMake(0, CFArrayGetCount(circleConcurring)));
        CFReleaseSafe(circleConcurring);
    });

    return concurringPeers;
}

CFStringRef SOSAccountCopyIncompatibilityInfo(SOSAccountRef account, CFErrorRef* error)
{
    return CFSTR("We're compatible, go away");
}

enum DepartureReason SOSAccountGetLastDepartureReason(SOSAccountRef account, CFErrorRef* error)
{
    return account->departure_code;
}

//
// TODO: Handle '|' and "¬" in other strings.
//
const CFStringRef kSOSKVSKeyParametersKey = CFSTR(">KeyParameters");
const CFStringRef kSOSKVSInitialSyncKey = CFSTR("^InitialSync");
const CFStringRef kSOSKVSAccountChangedKey = CFSTR("^AccountChanged");

const CFStringRef sWarningPrefix = CFSTR("!");
const CFStringRef sAncientCirclePrefix = CFSTR("@");
const CFStringRef sCirclePrefix = CFSTR("o");
const CFStringRef sRetirementPrefix = CFSTR("-");
const CFStringRef sCircleSeparator = CFSTR("|");
const CFStringRef sFromToSeparator = CFSTR(":");

static CFStringRef stringEndingIn(CFMutableStringRef in, CFStringRef token) {
    if(token == NULL) return CFStringCreateCopy(NULL, in);
    CFRange tokenAt = CFStringFind(in, token, 0);
    if(tokenAt.location == kCFNotFound) return NULL;
    CFStringRef retval = CFStringCreateWithSubstring(NULL, in, CFRangeMake(0, tokenAt.location));
    CFStringDelete(in, CFRangeMake(0, tokenAt.location+1));
    return retval;
}

SOSKVSKeyType SOSKVSKeyGetKeyTypeAndParse(CFStringRef key, CFStringRef *circle, CFStringRef *from, CFStringRef *to)
{
    SOSKVSKeyType retval = kUnknownKey;
    
    if(CFStringHasPrefix(key, sCirclePrefix)) retval = kCircleKey;
    else if(CFStringHasPrefix(key, sRetirementPrefix)) retval = kRetirementKey;
    else if(CFStringHasPrefix(key, kSOSKVSKeyParametersKey)) retval = kParametersKey;
    else if(CFStringHasPrefix(key, kSOSKVSInitialSyncKey)) retval = kInitialSyncKey;
    else if(CFStringHasPrefix(key, kSOSKVSAccountChangedKey)) retval = kAccountChangedKey;
    else retval = kMessageKey;
    
    switch(retval) {
        case kCircleKey:
            if (circle) {
                CFRange fromRange = CFRangeMake(1, CFStringGetLength(key)-1);
                *circle = CFStringCreateWithSubstring(NULL, key, fromRange);
            }
            break;
        case kMessageKey: {
                CFStringRef mCircle = NULL;
                CFStringRef mFrom = NULL;
                CFStringRef mTo = NULL;
                CFMutableStringRef keycopy = CFStringCreateMutableCopy(NULL, 128, key);

                if( ((mCircle = stringEndingIn(keycopy, sCircleSeparator)) != NULL) &&
                    ((mFrom = stringEndingIn(keycopy, sFromToSeparator)) != NULL) &&
                    (CFStringGetLength(mFrom) > 0)  ) {
                        mTo = stringEndingIn(keycopy, NULL);                
                        if (circle) *circle = CFStringCreateCopy(NULL, mCircle);
                        if (from) *from = CFStringCreateCopy(NULL, mFrom);
                        if (to && mTo) *to = CFStringCreateCopy(NULL, mTo);
                } else {
                    retval = kUnknownKey;
                }
                CFReleaseNull(mCircle);
                CFReleaseNull(mFrom);
                CFReleaseNull(mTo);
                CFReleaseNull(keycopy);
            }
            break;
        case kRetirementKey: {
                CFStringRef mCircle = NULL;
                CFStringRef mPeer = NULL;
                CFMutableStringRef keycopy = CFStringCreateMutableCopy(NULL, 128, key);
                CFStringDelete(keycopy, CFRangeMake(0, 1));
                if( ((mCircle = stringEndingIn(keycopy, sCircleSeparator)) != NULL) &&
                    ((mPeer = stringEndingIn(keycopy, NULL)) != NULL)) {
                    if (circle) *circle = CFStringCreateCopy(NULL, mCircle);
                    if (from) *from = CFStringCreateCopy(NULL, mPeer);
                } else {
                    retval = kUnknownKey;
                }
                // TODO - Update our circle
                CFReleaseNull(mCircle);
                CFReleaseNull(mPeer);
                CFReleaseNull(keycopy);
            }
            break;
        case kAccountChangedKey:
        case kParametersKey:
        case kInitialSyncKey:
        case kUnknownKey:
            break;
    }
    
    return retval;
}


SOSKVSKeyType SOSKVSKeyGetKeyType(CFStringRef key)
{
    return SOSKVSKeyGetKeyTypeAndParse(key, NULL, NULL, NULL);
}

CFStringRef SOSCircleKeyCreateWithCircle(SOSCircleRef circle, CFErrorRef *error)
{
    return SOSCircleKeyCreateWithName(SOSCircleGetName(circle), error);
}


CFStringRef SOSCircleKeyCreateWithName(CFStringRef circleName, CFErrorRef *error)
{
    return CFStringCreateWithFormat(NULL, NULL, CFSTR("%@%@"), sCirclePrefix, circleName);
}

CFStringRef SOSCircleKeyCopyCircleName(CFStringRef key, CFErrorRef *error)
{
    CFStringRef circleName = NULL;
    
    if (kCircleKey != SOSKVSKeyGetKeyTypeAndParse(key, &circleName, NULL, NULL)) {
        SOSCreateErrorWithFormat(kSOSErrorNoCircleName, NULL, error, NULL, CFSTR("Couldn't find circle name in key '%@'"), key);
        
        CFReleaseNull(circleName);
    }
    
    return circleName;
}

CFStringRef SOSMessageKeyCopyCircleName(CFStringRef key, CFErrorRef *error)
{
    CFStringRef circleName = NULL;
    
    if (SOSKVSKeyGetKeyTypeAndParse(key, &circleName, NULL, NULL) != kMessageKey) {
        SOSCreateErrorWithFormat(kSOSErrorNoCircleName, NULL, error, NULL, CFSTR("Couldn't find circle name in key '%@'"), key);
        
        CFReleaseNull(circleName);
    }        
    return circleName;
}

CFStringRef SOSMessageKeyCopyFromPeerName(CFStringRef messageKey, CFErrorRef *error)
{
    CFStringRef fromPeer = NULL;
    
    if (SOSKVSKeyGetKeyTypeAndParse(messageKey, NULL, &fromPeer, NULL) != kMessageKey) {
        SOSCreateErrorWithFormat(kSOSErrorNoCircleName, NULL, error, NULL, CFSTR("Couldn't find from peer in key '%@'"), messageKey);
        
        CFReleaseNull(fromPeer);
    }
    return fromPeer;
}

CFStringRef SOSMessageKeyCreateWithCircleAndPeerNames(SOSCircleRef circle, CFStringRef from_peer_name, CFStringRef to_peer_name)
{
    return CFStringCreateWithFormat(NULL, NULL, CFSTR("%@%@%@%@%@"),
                                    SOSCircleGetName(circle), sCircleSeparator, from_peer_name, sFromToSeparator, to_peer_name);
}

CFStringRef SOSMessageKeyCreateWithCircleAndPeerInfos(SOSCircleRef circle, SOSPeerInfoRef from_peer, SOSPeerInfoRef to_peer)
{
    return SOSMessageKeyCreateWithCircleAndPeerNames(circle, SOSPeerInfoGetPeerID(from_peer), SOSPeerInfoGetPeerID(to_peer));
}

CFStringRef SOSMessageKeyCreateWithAccountAndPeer(SOSAccountRef account, SOSCircleRef circle, CFStringRef peer_name) {
    // TODO: Handle errors!
    CFErrorRef error = NULL;

    SOSFullPeerInfoRef me = SOSAccountGetMyFullPeerInCircle(account, circle, &error);
    SOSPeerInfoRef my_pi = SOSFullPeerInfoGetPeerInfo(me);
    CFStringRef result = SOSMessageKeyCreateWithCircleAndPeerNames(circle, SOSPeerInfoGetPeerID(my_pi), peer_name);
    CFReleaseSafe(error);
    return result;
}

CFStringRef SOSRetirementKeyCreateWithCircleAndPeer(SOSCircleRef circle, CFStringRef retirement_peer_name)
{
     return CFStringCreateWithFormat(NULL, NULL, CFSTR("%@%@%@%@"),
                                    sRetirementPrefix, SOSCircleGetName(circle), sCircleSeparator, retirement_peer_name);
}


static SOSPeerCoderStatus SOSAccountHandlePeerMessage(SOSAccountRef account,
                                        CFStringRef circle_id,
                                        CFStringRef peer_name,
                                        CFDataRef message,
                                        SOSAccountSendBlock send_block,
                                        CFErrorRef *error)
{
    bool success = false;
    CFStringRef peer_key = NULL;

    SOSCircleRef circle = SOSAccountFindCircle(account, circle_id, error);
    require_quiet(circle, fail);
    SOSFullPeerInfoRef myFullPeer = SOSAccountGetMyFullPeerInCircle(account, circle, error);
    SOSPeerInfoRef myPeer = SOSFullPeerInfoGetPeerInfo(myFullPeer);
    require_action_quiet(SOSCircleHasPeer(circle, myPeer, NULL), fail, SOSCreateErrorWithFormat(kSOSErrorNotReady, NULL, error, NULL, CFSTR("Not in circle, can't handle message")));
    
    peer_key = SOSMessageKeyCreateWithAccountAndPeer(account, circle, peer_name);

    SOSPeerSendBlock peer_send_block = ^bool (CFDataRef message, CFErrorRef *error) {
        return send_block(circle, peer_key, message, error);
    };

    success = SOSCircleHandlePeerMessage(circle, myFullPeer, account->factory, peer_send_block, peer_name, message, error);

fail:
    CFReleaseNull(peer_key);
    return success;
}

bool SOSAccountHandleUpdates(SOSAccountRef account,
                             CFDictionaryRef updates,
                             CFErrorRef *error) {
    
    if(CFDictionaryGetValue(updates, kSOSKVSAccountChangedKey) != NULL) {
        SOSAccountSetToNew(account);
    }

    CFTypeRef parameters = CFDictionaryGetValue(updates, kSOSKVSKeyParametersKey);
    if (isData(parameters)) {
        SecKeyRef newKey = NULL;
        CFDataRef newParameters = NULL;
        const uint8_t *parse_end = der_decode_cloud_parameters(kCFAllocatorDefault, kSecECDSAAlgorithmID,
                                                               &newKey, &newParameters, error,
                                                               CFDataGetBytePtr(parameters), CFDataGetPastEndPtr(parameters));

        if (parse_end == CFDataGetPastEndPtr(parameters)) {
            if (CFEqualSafe(account->user_public, newKey)) {
                secnotice("updates", "Got same public key sent our way. Ignoring.");
            } else if (CFEqualSafe(account->previous_public, newKey)) {
                secnotice("updates", "Got previous public key repeated. Ignoring.");
            } else {
                CFReleaseNull(account->user_public);
                SOSAccountPurgePrivateCredential(account);
                CFReleaseNull(account->user_key_parameters);

                account->user_public_trusted = false;

                account->user_public = newKey;
                newKey = NULL;

                account->user_key_parameters = newParameters;
                newParameters = NULL;

                secnotice("updates", "Got new parameters for public key: %@", account->user_public);
                debugDumpUserParameters(CFSTR("params"), account->user_key_parameters);
            }
        }

        CFReleaseNull(newKey);
        CFReleaseNull(newParameters);
    }

    if (!account->user_public_trusted) {
        if (!account->deferred_updates) {
            account->deferred_updates = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
        }
        
        CFDictionaryForEach(updates, ^(const void *key, const void *value) {
            if (!CFEqualSafe(key, kSOSKVSKeyParametersKey) && !CFEqualSafe(key, kSOSKVSAccountChangedKey))
                CFDictionarySetValue(account->deferred_updates, key, value);
        });
        secnotice("updates", "No public peer key, deferring updates: %@", updates);
        return true;
    }

    // Iterate though keys in updates.  Perform circle change update.
    // Then instantiate circles and engines and peers for all peers that
    // are receiving a message in updates.
    __block bool is_initial_sync = CFDictionaryContainsKey(updates, kSOSKVSInitialSyncKey);

    CFDictionaryForEach(updates, ^(const void *key, const void *value) {
        CFStringRef circle_name = NULL;
        CFErrorRef localError = NULL;
        SOSCircleRef circle = NULL;
        
        if (SOSKVSKeyGetKeyTypeAndParse(key, &circle_name, NULL, NULL) == kCircleKey) {
            circle = SOSAccountCreateCircleFrom(circle_name, value, &localError);
            if (!circle) {
                if (isSOSErrorCoded(localError, kSOSErrorIncompatibleCircle)) {
                    SOSAccountDestroyCirclePeerInfoNamed(account, circle_name, NULL);
                    CFDictionarySetValue(account->circles, circle_name, kCFNull);
                } else {
                    SOSCreateErrorWithFormat(kSOSErrorNameMismatch, localError, error, NULL,
                                         CFSTR("Bad key for message, no circle '%@'"), key);
                    goto circle_done;
                }
            }

            if (!SOSAccountUpdateCircleFromRemote(account, circle, is_initial_sync, &localError)) {
                SOSCreateErrorWithFormat(kSOSErrorProcessingFailure, localError, error, NULL,
                                         CFSTR("Error handling circle change '%@'"), key);
                secnotice("update", "Error updating circle '%@': %@", key, circle);
                goto circle_done;
            }
        }
        circle_done:
        CFReleaseSafe(circle_name);
        CFReleaseNull(circle);
        CFReleaseNull(localError);
    });
    
    CFDictionaryForEach(updates, ^(const void *key, const void *value) {
        CFErrorRef localError = NULL;
        CFStringRef circle_name = NULL;
        CFStringRef from_name = NULL;
        CFStringRef to_name = NULL;
        switch (SOSKVSKeyGetKeyTypeAndParse(key, &circle_name, &from_name, &to_name)) {
            case kParametersKey:
            case kInitialSyncKey:
            case kCircleKey:
                break;
            case kMessageKey:
            {
                SOSFullPeerInfoRef my_peer = NULL;

                require_action_quiet(isData(value), message_error, SOSCreateErrorWithFormat(kSOSErrorUnexpectedType, localError, error, NULL, CFSTR("Non-Data for message(%@) from '%@'"), value, key));
                require_quiet(my_peer = SOSAccountGetMyFullPeerInCircleNamedIfPresent(account, circle_name, &localError), message_error);
                
                CFStringRef my_id = SOSPeerInfoGetPeerID(SOSFullPeerInfoGetPeerInfo(my_peer));
                require_quiet(SOSAccountIsActivePeerInCircleNamed(account, circle_name, my_id, &localError), skip);
                require_quiet(CFEqual(my_id, to_name), skip);
                require_quiet(!CFEqual(my_id, from_name), skip);

                SOSAccountSendBlock cacheInDictionary = ^ bool (SOSCircleRef circle, CFStringRef key, CFDataRef new_message, CFErrorRef* error) {
                    CFDictionarySetValue(account->pending_changes, key, new_message);
                    
                    if (account->processed_message_block) {
                        account->processed_message_block(circle, value, new_message);
                    }
                    
                    return true;
                };
                
                if (SOSAccountHandlePeerMessage(account, circle_name, from_name, value, cacheInDictionary, &localError) == kSOSPeerCoderFailure) {
                    SOSCreateErrorWithFormat(kSOSErrorNameMismatch, localError, error, NULL,
                                             CFSTR("Error handling peer message from '%@'"), key);
                    localError = NULL; // Released by SOSCreateErrorWithFormat
                    goto message_error;
                }

            message_error:
            skip:
                break;
            }
            case kRetirementKey:
                if(isData(value)) {                
                    SOSPeerInfoRef pi = SOSPeerInfoCreateFromData(NULL, error, (CFDataRef) value);
                    if(pi && CFEqual(from_name, SOSPeerInfoGetPeerID(pi)) && SOSPeerInfoInspectRetirementTicket(pi, error)) {
                        CFDictionarySetValue(account->retired_peers, key, value);
                        SOSAccountRecordRetiredPeerInCircleNamed(account, circle_name, pi);
                    }
                    CFReleaseSafe(pi);
                }
                break;
            
            case kAccountChangedKey: // Handled at entry to function to make sure these are processed first.
                break;
                
            case kUnknownKey:
                secnotice("updates", "Unknown key '%@', ignoring", key);
                break;
            
        }
        
        CFReleaseNull(circle_name);
        CFReleaseNull(from_name);
        CFReleaseNull(to_name);

        if (error && *error)
            secerror("Peer message processing error for: %@ -> %@ (%@)", key, value, *error);
        if (localError)
            secerror("Peer message local processing error for: %@ -> %@ (%@)", key, value, localError);

        CFReleaseNull(localError);
    });
    
    return SOSAccountSendPendingChanges(account, error);
}

void SOSAccountSetMessageProcessedBlock(SOSAccountRef account, SOSAccountMessageProcessedBlock processedBlock)
{
    CFRetainSafe(processedBlock);
    CFReleaseNull(account->processed_message_block);
    account->processed_message_block = processedBlock;
}

CFStringRef SOSInterestListCopyDescription(CFArrayRef interests)
{
    CFMutableStringRef description = CFStringCreateMutable(kCFAllocatorDefault, 0);
    CFStringAppendFormat(description, NULL, CFSTR("<Interest: "));
    
    CFArrayForEach(interests, ^(const void* string) {
        if (isString(string))
            CFStringAppendFormat(description, NULL, CFSTR(" '%@'"), string);
    });
    CFStringAppend(description, CFSTR(">"));

    return description;
}

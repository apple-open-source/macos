/*
 * Copyright (c) 2013-2014 Apple Inc. All Rights Reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */


#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <AssertMacros.h>
#include "SOSAccountPriv.h"
#include "SOSViews.h"

#include <utilities/SecCFWrappers.h>
#include <utilities/SecCoreCrypto.h>
#include <utilities/SecBuffer.h>

#include <Security/SecureObjectSync/SOSKVSKeys.h>
#include <SOSPeerInfoDER.h>

#include <Security/SecureObjectSync/SOSTransport.h>

#include <Security/SecureObjectSync/SOSPeerInfoCollections.h>
#include <os/state_private.h>


static SOSAccountRef SOSAccountCreateFromRemainingDER_v6(CFAllocatorRef allocator,
                                                         SOSDataSourceFactoryRef factory,
                                                         CFErrorRef* error,
                                                         const uint8_t** der_p, const uint8_t *der_end)
{
    SOSAccountRef result = NULL;
    SOSAccountRef account = NULL;
    CFArrayRef array = NULL;
    CFDictionaryRef retiredPeers = NULL;
    CFStringRef circle_name = factory->copy_name(factory);
    
    {
        CFDictionaryRef decoded_gestalt = NULL;
        *der_p = der_decode_dictionary(kCFAllocatorDefault, kCFPropertyListImmutable, &decoded_gestalt, error,
                                       *der_p, der_end);
        
        if (*der_p == 0)
            return NULL;
        
        account = SOSAccountCreateBasic(allocator, decoded_gestalt, factory);
        CFReleaseNull(decoded_gestalt);
    }
    
    *der_p = der_decode_array(kCFAllocatorDefault, 0, &array, error, *der_p, der_end);
    
    uint64_t tmp_departure_code = kSOSNeverAppliedToCircle;
    *der_p = ccder_decode_uint64(&tmp_departure_code, *der_p, der_end);
    *der_p = ccder_decode_bool(&account->user_public_trusted, *der_p, der_end);
    *der_p = der_decode_public_bytes(kCFAllocatorDefault, kSecECDSAAlgorithmID, &account->user_public, error, *der_p, der_end);
    *der_p = der_decode_public_bytes(kCFAllocatorDefault, kSecECDSAAlgorithmID, &account->previous_public, error, *der_p, der_end);
    *der_p = der_decode_data_or_null(kCFAllocatorDefault, &account->user_key_parameters, error, *der_p, der_end);
    
    *der_p = der_decode_dictionary(kCFAllocatorDefault, kCFPropertyListMutableContainers, (CFDictionaryRef *) &retiredPeers, error, *der_p, der_end);
    require_action_quiet(*der_p == der_end, fail, *der_p = NULL);
    
    account->departure_code = (enum DepartureReason) tmp_departure_code;
    
    CFDictionaryForEach(retiredPeers, ^(const void *key, const void *value) {
        if (isData(value)) {
            SOSPeerInfoRef retiree = SOSPeerInfoCreateFromData(kCFAllocatorDefault, NULL, (CFDataRef) value);
            
            CFSetAddValue(account->retirees, retiree);
            
            CFReleaseNull(retiree);
        }
    });

    require_quiet(array && *der_p, fail);
    
    CFArrayForEach(array, ^(const void *value) {
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
            require_quiet(circle && CFEqualSafe(circle_name, SOSCircleGetName(circle)), fail);
            
            account->trusted_circle = CFRetainSafe(circle);

	    if(fullPeerInfoData) {
	      	account->my_identity = SOSFullPeerInfoCreateFromData(kCFAllocatorDefault, fullPeerInfoData, NULL);
	    }

        fail:
            CFReleaseNull(circle);
        }
    });
    CFReleaseNull(array);

    require_action_quiet(SOSAccountEnsureFactoryCircles(account), fail,
                         SOSCreateError(kSOSErrorBadFormat, CFSTR("Cannot EnsureFactoryCircles"), (error != NULL) ? *error : NULL, error));

    result = CFRetainSafe(account);

fail:
    CFReleaseNull(account);
    return result;
}

static size_t der_sizeof_data_optional(CFDataRef data)
{
    return data ? der_sizeof_data(data, NULL) : 0;

}

static uint8_t* der_encode_data_optional(CFDataRef data, CFErrorRef *error,
                                         const uint8_t *der, uint8_t *der_end)
{
    return data ? der_encode_data(data, error, der, der_end) : der_end;

}

static const uint8_t* der_decode_data_optional(CFAllocatorRef allocator, CFOptionFlags mutability,
                                        CFDataRef* data, CFErrorRef *error,
                                        const uint8_t* der, const uint8_t *der_end)
{
    const uint8_t *dt_end = der_decode_data(allocator, mutability, data, NULL, der, der_end);

    return dt_end ? dt_end : der;
}

static SOSAccountRef SOSAccountCreateFromRemainingDER_v7(CFAllocatorRef allocator,
                                                         SOSDataSourceFactoryRef factory,
                                                         CFErrorRef* error,
                                                         const uint8_t** der_p, const uint8_t *der_end)
{
    SOSAccountRef account = NULL;
    SOSAccountRef result = NULL;
    
    {
        CFDictionaryRef decoded_gestalt = NULL;
        *der_p = der_decode_dictionary(kCFAllocatorDefault, kCFPropertyListImmutable, &decoded_gestalt, error,
                                       *der_p, der_end);
        
        if (*der_p == 0)
            return NULL;
        
        account = SOSAccountCreateBasic(allocator, decoded_gestalt, factory);
        CFReleaseNull(decoded_gestalt);
    }
    
    account->trusted_circle = SOSCircleCreateFromDER(kCFAllocatorDefault, error, der_p, der_end);
    *der_p = der_decode_fullpeer_or_null(kCFAllocatorDefault, &account->my_identity, error, *der_p, der_end);
    
    uint64_t tmp_departure_code = kSOSNeverAppliedToCircle;
    *der_p = ccder_decode_uint64(&tmp_departure_code, *der_p, der_end);
    *der_p = ccder_decode_bool(&account->user_public_trusted, *der_p, der_end);
    *der_p = der_decode_public_bytes(kCFAllocatorDefault, kSecECDSAAlgorithmID, &account->user_public, error, *der_p, der_end);
    *der_p = der_decode_public_bytes(kCFAllocatorDefault, kSecECDSAAlgorithmID, &account->previous_public, error, *der_p, der_end);
    *der_p = der_decode_data_or_null(kCFAllocatorDefault, &account->user_key_parameters, error, *der_p, der_end);
    account->retirees = SOSPeerInfoSetCreateFromArrayDER(kCFAllocatorDefault, &kSOSPeerSetCallbacks, error, der_p, der_end);
    *der_p = der_decode_data_optional(kCFAllocatorDefault, kCFPropertyListImmutable, &account->backup_key, error, *der_p, der_end);
    
    account->departure_code = (enum DepartureReason) tmp_departure_code;
    
    require_action_quiet(*der_p && *der_p == der_end, fail,
                         SOSCreateError(kSOSErrorBadFormat, CFSTR("Didn't consume all bytes v7"), (error != NULL) ? *error : NULL, error));
    
    result = CFRetainSafe(account);
    
fail:
    CFReleaseNull(account);
    return result;
}


static SOSAccountRef SOSAccountCreateFromRemainingDER_v8(CFAllocatorRef allocator,
                                                         SOSDataSourceFactoryRef factory,
                                                         CFErrorRef* error,
                                                         const uint8_t** der_p, const uint8_t *der_end)
{
    SOSAccountRef account = NULL;
    SOSAccountRef result = NULL;
    
    {
        CFDictionaryRef decoded_gestalt = NULL;
        *der_p = der_decode_dictionary(kCFAllocatorDefault, kCFPropertyListImmutable, &decoded_gestalt, error,
                                       *der_p, der_end);
        
        if (*der_p == 0)
            return NULL;
        
        account = SOSAccountCreateBasic(allocator, decoded_gestalt, factory);
        CFReleaseNull(decoded_gestalt);
    }
    CFReleaseNull(account->trusted_circle);
    account->trusted_circle = SOSCircleCreateFromDER(kCFAllocatorDefault, error, der_p, der_end);
    CFReleaseNull(account->my_identity);
    *der_p = der_decode_fullpeer_or_null(kCFAllocatorDefault, &account->my_identity, error, *der_p, der_end);
    
    uint64_t tmp_departure_code = kSOSNeverAppliedToCircle;
    *der_p = ccder_decode_uint64(&tmp_departure_code, *der_p, der_end);
    *der_p = ccder_decode_bool(&account->user_public_trusted, *der_p, der_end);
    *der_p = der_decode_public_bytes(kCFAllocatorDefault, kSecECDSAAlgorithmID, &account->user_public, error, *der_p, der_end);
    *der_p = der_decode_public_bytes(kCFAllocatorDefault, kSecECDSAAlgorithmID, &account->previous_public, error, *der_p, der_end);
    *der_p = der_decode_data_or_null(kCFAllocatorDefault, &account->user_key_parameters, error, *der_p, der_end);
    CFReleaseNull(account->retirees);
    account->retirees = SOSPeerInfoSetCreateFromArrayDER(kCFAllocatorDefault, &kSOSPeerSetCallbacks, error, der_p, der_end);
    *der_p = der_decode_data_optional(kCFAllocatorDefault, kCFPropertyListImmutable, &account->backup_key, error, *der_p, der_end);
    {
        CFDictionaryRef expansion = NULL;
        *der_p = der_decode_dictionary(kCFAllocatorDefault, kCFPropertyListImmutable, &expansion, error,
                                       *der_p, der_end);
        
        if (*der_p == 0)
            return NULL;
        
        CFReleaseNull(account->expansion);
        account->expansion = CFDictionaryCreateMutableCopy(kCFAllocatorDefault, 0, expansion);
        CFReleaseNull(expansion);
    }

    account->departure_code = (enum DepartureReason) tmp_departure_code;
    
    require_action_quiet(*der_p && *der_p == der_end, fail,
                         SOSCreateError(kSOSErrorBadFormat, CFSTR("Didn't consume all bytes v7"), (error != NULL) ? *error : NULL, error));
    
    result = CFRetainSafe(account);
    
fail:
    CFReleaseNull(account);
    return result;
}

//
// Version History for Account
//
// 1-5 - InnsbruckTaos/Cab; Never supported even for upgrading.
// 6   - First version used in the field.
// 7   - One Circle version
// 8   - Adding expansion dictionary
//

#define CURRENT_ACCOUNT_PERSISTENT_VERSION 8

SOSAccountRef SOSAccountCreateFromDER(CFAllocatorRef allocator,
                                      SOSDataSourceFactoryRef factory,
                                      CFErrorRef* error,
                                      const uint8_t** der_p, const uint8_t *der_end)
{
    SOSAccountRef account = NULL;
    SOSAccountRef result = NULL;
    uint64_t version = 0;

    const uint8_t *sequence_end;
    *der_p = ccder_decode_constructed_tl(CCDER_CONSTRUCTED_SEQUENCE, &sequence_end, *der_p, der_end);
    *der_p = ccder_decode_uint64(&version, *der_p, sequence_end);
    require_action_quiet(*der_p, errOut,
                         SOSCreateError(kSOSErrorBadFormat, CFSTR("Version parsing failed"), (error != NULL) ? *error : NULL, error));
    
    switch (version) {
        case CURRENT_ACCOUNT_PERSISTENT_VERSION:
            account = SOSAccountCreateFromRemainingDER_v8(allocator, factory, error, der_p, sequence_end);
            break;
            
        case 7:
            account = SOSAccountCreateFromRemainingDER_v7(allocator, factory, error, der_p, sequence_end);
            break;
            
        case 6:
            account = SOSAccountCreateFromRemainingDER_v6(allocator, factory, error, der_p, sequence_end);
            break;
            
        default:
            SOSCreateErrorWithFormat(kSOSErrorBadFormat, (error != NULL) ? *error : NULL, error,
                                     NULL, CFSTR("Unsupported version (%llu)"), version);
            break;
    }

    require_quiet(account, errOut);
    
    require_quiet(*der_p && *der_p == sequence_end, errOut);
    
    /* I may not always have an identity, but when I do, it has a private key */
    if(account->my_identity) {
        require_action_quiet(SOSFullPeerInfoPrivKeyExists(account->my_identity), errOut, secnotice("account", "No private key associated with my_identity, resetting"));
        notify_post(kSecServerPeerInfoAvailable);
        if(account->deviceID)
            SOSFullPeerInfoUpdateDeviceID(account->my_identity, account->deviceID, error);
    }
    
    require_action_quiet(SOSAccountEnsureFactoryCircles(account), errOut,
                         SOSCreateError(kSOSErrorBadFormat, CFSTR("Cannot EnsureFactoryCircles"), (error != NULL) ? *error : NULL, error));

    SOSPeerInfoRef myPI = SOSAccountGetMyPeerInfo(account);
    if (myPI) {
        SOSAccountCheckForAlwaysOnViews(account);
        SOSPeerInfoRef oldPI = myPI;
        // if UpdateFullPeerInfo did something - we need to make sure we have the right Ref
        myPI = SOSAccountGetMyPeerInfo(account);
        if(oldPI != myPI) secnotice("canary", "Caught spot where PIs differ in account setup");
        CFStringRef transportTypeInflatedFromDER = SOSPeerInfoCopyTransportType(myPI);
        if (CFStringCompare(transportTypeInflatedFromDER, CFSTR("IDS"), 0) == 0 || CFStringCompare(transportTypeInflatedFromDER, CFSTR("KVS"), 0) == 0)
            SOSFullPeerInfoUpdateTransportType(account->my_identity, SOSTransportMessageTypeIDSV2, NULL); //update the transport type to the current IDS V2 type
               
        CFReleaseNull(transportTypeInflatedFromDER);
    }

    SOSAccountEnsureRecoveryRing(account);
    
    SOSAccountWithTransactionSync(account, ^(SOSAccountRef account, SOSAccountTransactionRef txn) {
        account->key_interests_need_updating = true;
    });

    SOSAccountEnsureUUID(account);

    result = CFRetainSafe(account);

errOut:
    CFReleaseNull(account);
    return result;
}

SOSAccountRef SOSAccountCreateFromData(CFAllocatorRef allocator, CFDataRef circleData,
                                       SOSDataSourceFactoryRef factory,
                                       CFErrorRef* error)
{
    size_t size = CFDataGetLength(circleData);
    const uint8_t *der = CFDataGetBytePtr(circleData);
    SOSAccountRef account = SOSAccountCreateFromDER(allocator, factory,
                                                    error,
                                                    &der, der + size);
    return account;
}

CFMutableSetRef SOSPeerInfoSetCreateFromArrayDER(CFAllocatorRef allocator, const CFSetCallBacks *callbacks, CFErrorRef* error,
                                                 const uint8_t** der_p, const uint8_t *der_end);
size_t SOSPeerInfoSetGetDEREncodedArraySize(CFSetRef pia, CFErrorRef *error);
uint8_t* SOSPeerInfoSetEncodeToArrayDER(CFSetRef pia, CFErrorRef* error, const uint8_t* der, uint8_t* der_end);

size_t SOSAccountGetDEREncodedSize(SOSAccountRef account, CFErrorRef *error)
{
    size_t sequence_size = 0;
    uint64_t version = CURRENT_ACCOUNT_PERSISTENT_VERSION;
    
    require_quiet(accumulate_size(&sequence_size, ccder_sizeof_uint64(version)),                                    fail);
    require_quiet(accumulate_size(&sequence_size, der_sizeof_dictionary(account->gestalt, error)),                  fail);
    require_quiet(accumulate_size(&sequence_size, SOSCircleGetDEREncodedSize(account->trusted_circle, error)),      fail);
    require_quiet(accumulate_size(&sequence_size, der_sizeof_fullpeer_or_null(account->my_identity, error)),        fail);
    require_quiet(accumulate_size(&sequence_size, ccder_sizeof_uint64(account->departure_code)),                    fail);
    require_quiet(accumulate_size(&sequence_size, ccder_sizeof_bool(account->user_public_trusted, error)),          fail);
    require_quiet(accumulate_size(&sequence_size, der_sizeof_public_bytes(account->user_public, error)),            fail);
    require_quiet(accumulate_size(&sequence_size, der_sizeof_public_bytes(account->previous_public, error)),        fail);
    require_quiet(accumulate_size(&sequence_size, der_sizeof_data_or_null(account->user_key_parameters, error)),    fail);
    require_quiet(accumulate_size(&sequence_size, SOSPeerInfoSetGetDEREncodedArraySize(account->retirees, error)),  fail);
    accumulate_size(&sequence_size, der_sizeof_data_optional(account->backup_key));
    require_quiet(accumulate_size(&sequence_size, der_sizeof_dictionary(account->expansion, error)),  fail);

    return ccder_sizeof(CCDER_CONSTRUCTED_SEQUENCE, sequence_size);
    
fail:
    SecCFDERCreateError(kSecDERErrorUnknownEncoding, CFSTR("don't know how to encode"), NULL, error);
    return 0;
}

uint8_t* SOSAccountEncodeToDER(SOSAccountRef account, CFErrorRef* error, const uint8_t* der, uint8_t* der_end)
{
    uint64_t version = CURRENT_ACCOUNT_PERSISTENT_VERSION;
    der_end =  ccder_encode_constructed_tl(CCDER_CONSTRUCTED_SEQUENCE, der_end, der,
               ccder_encode_uint64(version, der,
               der_encode_dictionary(account->gestalt, error, der,
               SOSCircleEncodeToDER(account->trusted_circle, error, der,
               der_encode_fullpeer_or_null(account->my_identity, error, der,
               ccder_encode_uint64(account->departure_code, der,
               ccder_encode_bool(account->user_public_trusted, der,
               der_encode_public_bytes(account->user_public, error, der,
               der_encode_public_bytes(account->previous_public, error, der,
               der_encode_data_or_null(account->user_key_parameters, error, der,
               SOSPeerInfoSetEncodeToArrayDER(account->retirees, error, der,
               der_encode_data_optional(account->backup_key, error, der,
               der_encode_dictionary(account->expansion, error, der,
              

    der_end)))))))))))));

    return der_end;
}

/************************/

CFDataRef SOSAccountCopyEncodedData(SOSAccountRef account, CFAllocatorRef allocator, CFErrorRef *error)
{
    return CFDataCreateWithDER(kCFAllocatorDefault, SOSAccountGetDEREncodedSize(account, error), ^uint8_t*(size_t size, uint8_t *buffer) {
        return SOSAccountEncodeToDER(account, error, buffer, (uint8_t *) buffer + size);
    });
}



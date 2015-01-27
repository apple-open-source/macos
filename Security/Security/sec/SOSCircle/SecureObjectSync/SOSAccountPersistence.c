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

#include <utilities/SecCFWrappers.h>
#include <SecureObjectSync/SOSKVSKeys.h>

SOSAccountRef SOSAccountCreateFromDER_V1(CFAllocatorRef allocator,
                                         SOSDataSourceFactoryRef factory,
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
        
        account = SOSAccountCreateBasic(allocator, decoded_gestalt, factory);
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
        
        account = SOSAccountCreateBasic(allocator, decoded_gestalt, factory);
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
                        CFReleaseSafe(full_peer);
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
    account = SOSAccountCreateFromDER_V1(allocator, factory, error, &dersave, derend);
    if(account) account->departure_code = kSOSNeverAppliedToCircle;
    return account;
}

static void SOSAccountConvertKVSDictionaryToRetirementDictionary(SOSAccountRef account)
{
    CFMutableDictionaryRef old_retired_peers = account->retired_peers;
    account->retired_peers = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);

    CFDictionaryForEach(old_retired_peers, ^(const void *key, const void *value) {
        if (isDictionary(value)) {
            CFDictionaryAddValue(account->retired_peers, key, value);
        } else if (isString(key) && isData(value)) {
            CFDataRef retired_peer_data = (CFDataRef) value;
            CFStringRef circle_name = NULL;
            CFStringRef retired_peer_id = NULL;

            if (kRetirementKey == SOSKVSKeyGetKeyTypeAndParse(key, &circle_name, &retired_peer_id, NULL)) {
                CFMutableDictionaryRef circle_retirees = CFDictionaryEnsureCFDictionaryAndGetCurrentValue(account->retired_peers, circle_name);

                CFDictionarySetValue(circle_retirees, retired_peer_id, retired_peer_data);
            }

            CFReleaseSafe(circle_name);
            CFReleaseSafe(retired_peer_id);
        }
    });
    CFReleaseSafe(old_retired_peers);
}


#define CURRENT_ACCOUNT_PERSISTENT_VERSION 6

SOSAccountRef SOSAccountCreateFromDER(CFAllocatorRef allocator,
                                      SOSDataSourceFactoryRef factory,
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
        return SOSAccountCreateFromDER_V3(allocator, factory, error, &dersave, derend);
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
        
        account = SOSAccountCreateBasic(allocator, decoded_gestalt, factory);
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
    
    SOSAccountConvertKVSDictionaryToRetirementDictionary(account);

    return account;
    
fail:
    account->factory = NULL; // give the factory back.
    CFReleaseNull(account);
    return NULL;
}


SOSAccountRef SOSAccountCreateFromDER_V3(CFAllocatorRef allocator,
                                         SOSDataSourceFactoryRef factory,
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
        
        account = SOSAccountCreateBasic(allocator, decoded_gestalt, factory);
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
    
    SOSAccountConvertKVSDictionaryToRetirementDictionary(account);

    return account;
    
fail:
    // Create a default error if we don't have one:
    account->factory = NULL; // give the factory back.
    CFReleaseNull(account);
    // Don't try the der inflater from the previous release.
    // account = SOSAccountCreateFromDER_V2(allocator, transport, factory, error, &dersave, derend);
    if(account) account->departure_code = kSOSNeverAppliedToCircle;
    return account;
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


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
#include "SOSViews.h"

#include <utilities/SecCFWrappers.h>
#include <utilities/SecNSAdditions.h>

#include <utilities/SecCoreCrypto.h>
#include <utilities/SecBuffer.h>

#include <Security/SecureObjectSync/SOSKVSKeys.h>
#include <SOSPeerInfoDER.h>

#include <Security/SecureObjectSync/SOSTransport.h>

#include <Security/SecureObjectSync/SOSPeerInfoCollections.h>
#include <os/state_private.h>

#import <Security/SecureObjectSync/SOSAccountPriv.h>
#import <Security/SecureObjectSync/SOSAccountTrust.h>
#import <Security/SecureObjectSync/SOSCircleDer.h>
#import "Security/SecureObjectSync/SOSAccountTrustClassic.h"

@implementation SOSAccount (Persistence)


static SOSAccount* SOSAccountCreateFromRemainingDER_v6(CFAllocatorRef allocator,
                                                         SOSDataSourceFactoryRef factory,
                                                         CFErrorRef* error,
                                                         const uint8_t** der_p, const uint8_t *der_end)
{
    SOSAccount* result = NULL;
    SOSAccount* account = NULL;

    CFArrayRef array = NULL;
    CFDictionaryRef retiredPeers = NULL;

    CFStringRef circle_name = factory->copy_name(factory);
    
    {
        CFDictionaryRef decoded_gestalt = NULL;
        *der_p = der_decode_dictionary(kCFAllocatorDefault, kCFPropertyListImmutable, &decoded_gestalt, error,
                                       *der_p, der_end);
        
        if (*der_p == 0)
            return NULL;

        account = SOSAccountCreate(allocator, decoded_gestalt, factory);

        CFReleaseNull(decoded_gestalt);
    }
    SOSAccountTrustClassic *trust = account.trust;

    *der_p = der_decode_array(kCFAllocatorDefault, 0, &array, error, *der_p, der_end);
    
    uint64_t tmp_departure_code = kSOSNeverAppliedToCircle;
    *der_p = ccder_decode_uint64(&tmp_departure_code, *der_p, der_end);
    [trust setDepartureCode:(enum DepartureReason)tmp_departure_code];

    bool userPublicTrusted;
    *der_p = ccder_decode_bool(&userPublicTrusted, *der_p, der_end);
    account.accountKeyIsTrusted = userPublicTrusted;

    SecKeyRef userPublic = NULL;
    SecKeyRef previousUserPublic = NULL;
    *der_p = der_decode_public_bytes(kCFAllocatorDefault, kSecECDSAAlgorithmID, &userPublic, error, *der_p, der_end);
    *der_p = der_decode_public_bytes(kCFAllocatorDefault, kSecECDSAAlgorithmID, &previousUserPublic, error, *der_p, der_end);
    account.accountKey = userPublic;
    account.previousAccountKey = previousUserPublic;
    CFReleaseNull(userPublic);
    CFReleaseNull(previousUserPublic);

    {
        CFDataRef parms = NULL;
        *der_p = der_decode_data_or_null(kCFAllocatorDefault, &parms, error, *der_p, der_end);
        [account setAccountKeyDerivationParamters:(__bridge_transfer NSData*) parms];
    }

    *der_p = der_decode_dictionary(kCFAllocatorDefault, kCFPropertyListMutableContainers, (CFDictionaryRef *) &retiredPeers, error, *der_p, der_end);

    if(*der_p != der_end) {
        *der_p = NULL;
        return result;
    }
    
    {
        CFMutableSetRef retirees = CFSetCreateMutableForCFTypes(kCFAllocatorDefault);

        CFDictionaryForEach(retiredPeers, ^(const void *key, const void *value) {
            if (isData(value)) {
                SOSPeerInfoRef retiree = SOSPeerInfoCreateFromData(kCFAllocatorDefault, NULL, (CFDataRef) value);
                
                CFSetAddValue(retirees, retiree);
                
                CFReleaseNull(retiree);
            }
        });

        [trust setRetirees:(__bridge NSMutableSet *)retirees];
        CFReleaseNull(retirees);
    }

    if(!array || !*der_p)
        return result;
    
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
            [trust setTrustedCircle:circle];
            CFReleaseNull(circle);

            if(fullPeerInfoData) {
                SOSFullPeerInfoRef identity = SOSFullPeerInfoCreateFromData(kCFAllocatorDefault, fullPeerInfoData, NULL);
                trust.fullPeerInfo = identity;
                CFReleaseNull(identity);
            }

        fail:
            CFReleaseNull(circle);
        }
    });
    CFReleaseNull(array);
    require_action_quiet([account ensureFactoryCircles], fail,
                         SOSCreateError(kSOSErrorBadFormat, CFSTR("Cannot EnsureFactoryCircles"), (error != NULL) ? *error : NULL, error));
    result = account;

fail:

    return result;
}

static const uint8_t* der_decode_data_optional(CFAllocatorRef allocator, CFOptionFlags mutability,
                                        CFDataRef* data, CFErrorRef *error,
                                        const uint8_t* der, const uint8_t *der_end)
{
    const uint8_t *dt_end = der_decode_data(allocator, mutability, data, NULL, der, der_end);

    return dt_end ? dt_end : der;
}

static SOSAccount* SOSAccountCreateFromRemainingDER_v7(CFAllocatorRef allocator,
                                                         SOSDataSourceFactoryRef factory,
                                                         CFErrorRef* error,
                                                         const uint8_t** der_p, const uint8_t *der_end)
{
    SOSAccount* account = NULL;
    SOSAccount* result = NULL;
    
    {
        CFDictionaryRef decoded_gestalt = NULL;
        *der_p = der_decode_dictionary(kCFAllocatorDefault, kCFPropertyListImmutable, &decoded_gestalt, error,
                                       *der_p, der_end);
        
        if (*der_p == 0)
            return NULL;
        
        account =  SOSAccountCreate(kCFAllocatorDefault, decoded_gestalt, factory);
        CFReleaseNull(decoded_gestalt);
    }

    enum DepartureReason departure_code = 0;
    SOSAccountTrustClassic *trust = account.trust;
    
    {
        SOSCircleRef circle = SOSCircleCreateFromDER(kCFAllocatorDefault, error, der_p, der_end);
        [trust setTrustedCircle:circle];
        CFReleaseNull(circle);
    }

    {
        SOSFullPeerInfoRef identity = NULL;
        *der_p = der_decode_fullpeer_or_null(kCFAllocatorDefault, &identity, error, *der_p, der_end);
        trust.fullPeerInfo = identity;
        CFReleaseNull(identity);
    }

    uint64_t tmp_departure_code = kSOSNeverAppliedToCircle;
    *der_p = ccder_decode_uint64(&tmp_departure_code, *der_p, der_end);
    bool userPublicTrusted;

    *der_p = ccder_decode_bool(&userPublicTrusted, *der_p, der_end);
    account.accountKeyIsTrusted = userPublicTrusted;

    {
        SecKeyRef userPublic = NULL;
        *der_p = der_decode_public_bytes(kCFAllocatorDefault, kSecECDSAAlgorithmID, &userPublic, error, *der_p, der_end);
        account.accountKey = userPublic;
        CFReleaseNull(userPublic);
    }

    {
        SecKeyRef previousUserPublic = NULL;
        *der_p = der_decode_public_bytes(kCFAllocatorDefault, kSecECDSAAlgorithmID, &previousUserPublic, error, *der_p, der_end);
        account.previousAccountKey = previousUserPublic;
        CFReleaseNull(previousUserPublic);
    }

    {
        CFDataRef parms = NULL;
        *der_p = der_decode_data_or_null(kCFAllocatorDefault, &parms, error, *der_p, der_end);
        account.accountKeyDerivationParamters = (__bridge_transfer NSData*)parms;
    }

    {
        CFSetRef retirees = SOSPeerInfoSetCreateFromArrayDER(kCFAllocatorDefault, &kSOSPeerSetCallbacks, error, der_p, der_end);
        [trust setRetirees:(__bridge NSMutableSet *)retirees];
    }

    {
        CFDataRef bKey = NULL;
        *der_p = der_decode_data_optional(kCFAllocatorDefault, kCFPropertyListImmutable, &bKey, error, *der_p, der_end);
        if(bKey != NULL)
            [account setBackup_key:(__bridge_transfer NSData *)bKey];
    }

    departure_code = (enum DepartureReason) tmp_departure_code;

    [trust setDepartureCode:departure_code];
    require_action_quiet(*der_p && *der_p == der_end, fail,
                         SOSCreateError(kSOSErrorBadFormat, CFSTR("Didn't consume all bytes v7"), (error != NULL) ? *error : NULL, error));
    
    result = account;
    
fail:
    return result;
}


static SOSAccount* SOSAccountCreateFromRemainingDER_v8(CFAllocatorRef allocator,
                                                         SOSDataSourceFactoryRef factory,
                                                         CFErrorRef* error,
                                                         const uint8_t** der_p, const uint8_t *der_end)
{
    SOSAccount* account = NULL;
    SOSAccount* result = NULL;

    {
        CFDictionaryRef decoded_gestalt = NULL;
        *der_p = der_decode_dictionary(kCFAllocatorDefault, kCFPropertyListImmutable, &decoded_gestalt, error,
                                       *der_p, der_end);
        
        if (*der_p == 0) {
            CFReleaseNull(decoded_gestalt);
            return NULL;
        }

        account =  SOSAccountCreate(kCFAllocatorDefault, decoded_gestalt, factory);
        CFReleaseNull(decoded_gestalt);
    }

    SOSAccountTrustClassic *trust = account.trust;

    {
        SOSCircleRef circle = SOSCircleCreateFromDER(kCFAllocatorDefault, error, der_p, der_end);
        [trust setTrustedCircle:circle];
        CFReleaseNull(circle);
    }

    {
        SOSFullPeerInfoRef identity = NULL;
        *der_p = der_decode_fullpeer_or_null(kCFAllocatorDefault, &identity, error, *der_p, der_end);
        trust.fullPeerInfo = identity;
        CFReleaseNull(identity);
    }

    uint64_t tmp_departure_code = kSOSNeverAppliedToCircle;
    *der_p = ccder_decode_uint64(&tmp_departure_code, *der_p, der_end);
    [trust setDepartureCode:(enum DepartureReason) tmp_departure_code];

    bool userPublicTrusted;
    *der_p = ccder_decode_bool(&userPublicTrusted, *der_p, der_end);
    account.accountKeyIsTrusted = userPublicTrusted;

    {
        SecKeyRef userPublic = NULL;
        *der_p = der_decode_public_bytes(kCFAllocatorDefault, kSecECDSAAlgorithmID, &userPublic, error, *der_p, der_end);
        account.accountKey = userPublic;
        CFReleaseNull(userPublic);
    }

    {
        SecKeyRef previousUserPublic = NULL;
        *der_p = der_decode_public_bytes(kCFAllocatorDefault, kSecECDSAAlgorithmID, &previousUserPublic, error, *der_p, der_end);
        account.previousAccountKey = previousUserPublic;
        CFReleaseNull(previousUserPublic);
    }

    {
        CFDataRef parms;
        *der_p = der_decode_data_or_null(kCFAllocatorDefault, &parms, error, *der_p, der_end);
        account.accountKeyDerivationParamters = (__bridge_transfer NSData*)parms;
        parms = NULL;
    }

    {
        CFMutableSetRef retirees = SOSPeerInfoSetCreateFromArrayDER(kCFAllocatorDefault, &kSOSPeerSetCallbacks, error, der_p, der_end);
        [trust setRetirees:(__bridge NSMutableSet *)retirees];
        CFReleaseNull(retirees);
    }

    CFDataRef bKey = NULL;
    *der_p = der_decode_data_optional(kCFAllocatorDefault, kCFPropertyListImmutable, &bKey, error, *der_p, der_end);
    {
        CFDictionaryRef expansion = NULL;
        *der_p = der_decode_dictionary(kCFAllocatorDefault, kCFPropertyListImmutable, &expansion, error,
                                       *der_p, der_end);
        
        if (*der_p == 0) {
            CFReleaseNull(bKey);
            CFReleaseNull(expansion);
            return NULL;
        }

        if(expansion) {
            [trust setExpansion:(__bridge NSMutableDictionary *)(expansion)];
        }
        CFReleaseNull(expansion);
    }
    if(bKey) {
        [account setBackup_key:[[NSData alloc] initWithData:(__bridge NSData * _Nonnull)(bKey)]];
    }
    CFReleaseNull(bKey);

    require_action_quiet(*der_p && *der_p == der_end, fail,
                         SOSCreateError(kSOSErrorBadFormat, CFSTR("Didn't consume all bytes v7"), (error != NULL) ? *error : NULL, error));
    
    result = account;
    
fail:
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

static SOSAccount* SOSAccountCreateFromDER(CFAllocatorRef allocator,
                                      SOSDataSourceFactoryRef factory,
                                      CFErrorRef* error,
                                      const uint8_t** der_p, const uint8_t *der_end)
{
    SOSAccount* account = NULL;
    uint64_t version = 0;

    SOSFullPeerInfoRef identity = NULL;

    const uint8_t *sequence_end;
    *der_p = ccder_decode_constructed_tl(CCDER_CONSTRUCTED_SEQUENCE, &sequence_end, *der_p, der_end);
    *der_p = ccder_decode_uint64(&version, *der_p, sequence_end);
    if (*der_p == NULL) {
        SOSCreateError(kSOSErrorBadFormat, CFSTR("Version parsing failed"), (error != NULL) ? *error : NULL, error);
        return nil;
    }

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

    if (!account) {
        // Error should have been filled in above.
        return nil;
    }

    if (*der_p != sequence_end) {
        SOSCreateError(kSOSErrorBadFormat, CFSTR("Extra data at the end of saved acount"), (error != NULL) ? *error : NULL, error);
        return nil;
    }

    identity = account.fullPeerInfo;
    /* I may not always have an identity, but when I do, it has a private key */
    if(identity) {
        if(!(SOSFullPeerInfoPrivKeyExists(identity)))
        {
            SOSUnregisterTransportKeyParameter(account.key_transport);
            SOSUnregisterTransportCircle((SOSCircleStorageTransport*)account.circle_transport);
            SOSUnregisterTransportMessage(account.kvs_message_transport);

            secnotice("account", "No private key associated with my_identity, resetting");
            return nil;
        }
    }

    if (![account ensureFactoryCircles]) {
        SOSCreateError(kSOSErrorBadFormat, CFSTR("Cannot EnsureFactoryCircles"), (error != NULL) ? *error : NULL, error);
        return nil;
    }

    SOSPeerInfoRef oldPI = CFRetainSafe(account.peerInfo);
    if (oldPI) {
        SOSAccountCheckForAlwaysOnViews(account);
    }
    CFReleaseNull(oldPI);

    SOSAccountEnsureRecoveryRing(account);

    [account performTransaction:^(SOSAccountTransaction * _Nonnull txn) {
        secnotice("circleop", "Setting account.key_interests_need_updating to true in SOSAccountCreateFromDER");
        account.key_interests_need_updating = true;
    }];

    SOSAccountEnsureUUID(account);

    return account;
}

+(instancetype) accountFromDER: (const uint8_t**) der
                           end: (const uint8_t*) der_end
                       factory: (SOSDataSourceFactoryRef) factory
                         error: (NSError**) error {
    CFErrorRef failure = NULL;
    SOSAccount* result = SOSAccountCreateFromDER(kCFAllocatorDefault, factory, &failure, der, der_end);

    if (result == nil) {
        if (error) {
            *error = (__bridge_transfer NSError*) failure;
            failure = NULL;
        }
    }
    CFReleaseNull(failure);

    return result;
}

+(instancetype) accountFromData: (NSData*) data
                        factory: (SOSDataSourceFactoryRef) factory
                          error: (NSError**) error {
    size_t size = [data length];
    const uint8_t *der = [data bytes];
    return [self accountFromDER: &der end: der+size factory: factory error: error];
}

CFMutableSetRef SOSPeerInfoSetCreateFromArrayDER(CFAllocatorRef allocator, const CFSetCallBacks *callbacks, CFErrorRef* error,
                                                 const uint8_t** der_p, const uint8_t *der_end);
size_t SOSPeerInfoSetGetDEREncodedArraySize(CFSetRef pia, CFErrorRef *error);
uint8_t* SOSPeerInfoSetEncodeToArrayDER(CFSetRef pia, CFErrorRef* error, const uint8_t* der, uint8_t* der_end);

/************************/


- (NSData*) encodedData: (NSError* __autoreleasing *) error {
    NSUInteger expected = [self.trust getDEREncodedSize: self err:error];
    if (expected == 0) return nil;

    return [NSMutableData dataWithSpace:expected
                              DEREncode:^uint8_t *(size_t size, uint8_t *buffer) {
                                  return [self.trust encodeToDER:self
                                                             err:error
                                                           start:buffer
                                                             end:buffer + size];
                              }];
}

@end

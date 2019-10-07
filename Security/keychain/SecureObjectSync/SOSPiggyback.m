/*
 * Copyright (c) 2015 Apple Inc. All Rights Reserved.
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

#include <Security/SecureObjectSync/SOSCloudCircle.h>
#include "keychain/SecureObjectSync/SOSInternal.h"
#include "keychain/SecureObjectSync/SOSCircleDer.h"
#include "keychain/SecureObjectSync/SOSAccountPriv.h"

#include <Security/Security.h>
#include <Security/SecKeyPriv.h>

#include <securityd/SecItemSchema.h>
#include <Security/SecItem.h>
#include <Security/SecItemPriv.h>

#include "SOSPiggyback.h"

#include "utilities/der_date.h"
#include "utilities/der_plist.h"
#include <utilities/der_plist_internal.h>
#include <corecrypto/ccder.h>

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
    CFReleaseNull(publicBytes);
    return ccder_sizeof(CCDER_CONSTRUCTED_SEQUENCE, total_payload);

errOut:
    SecCFDERCreateError(kSecDERErrorUnknownEncoding, CFSTR("don't know how to encode"), NULL, error);
    CFReleaseNull(publicBytes);
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

    CFReleaseNull(publicBytes);

    return der_end;
}

CFDataRef SOSPiggyBackBlobCopyEncodedData(SOSGenCountRef gencount, SecKeyRef pubKey, CFDataRef signature, CFErrorRef *error)
{
    return CFDataCreateWithDER(kCFAllocatorDefault, SOSPiggyBackBlobGetDEREncodedSize(gencount, pubKey, signature, error), ^uint8_t*(size_t size, uint8_t *buffer) {
        return SOSPiggyBackBlobEncodeToDER(gencount, pubKey, signature, error, buffer, (uint8_t *) buffer + size);
    });
}

bool SOSPiggyBackAddToKeychain(NSArray<NSData*>* identities, NSArray<NSDictionary*>*  tlks)
{
    [tlks enumerateObjectsUsingBlock:^(NSDictionary* item, NSUInteger idx, BOOL * _Nonnull stop) {

        NSData* v_data = item[(__bridge NSString*)kSecValueData];
        NSString *acct = item[(__bridge NSString*)kSecAttrAccount];
        NSString *srvr = item[(__bridge NSString*)kSecAttrServer];

        NSData* base64EncodedVData = [v_data base64EncodedDataWithOptions:0];

        NSMutableDictionary* query = [@{
                                        (id)kSecClass : (id)kSecClassInternetPassword,
                                        (id)kSecUseDataProtectionKeychain : @YES,
                                        (id)kSecAttrAccessGroup: @"com.apple.security.ckks",
                                        (id)kSecAttrDescription: @"tlk-piggy",
                                        (id)kSecAttrSynchronizable : (id)kCFBooleanFalse,
                                        (id)kSecAttrSyncViewHint : (id)kSecAttrViewHintPCSMasterKey,
                                        (id)kSecAttrServer: srvr,
                                        (id)kSecAttrAccount: [NSString stringWithFormat: @"%@-piggy", acct],
                                        (id)kSecAttrPath: acct,
                                        (id)kSecAttrIsInvisible: @YES,
                                        (id)kSecValueData : base64EncodedVData,
                                        } mutableCopy];

        OSStatus status = SecItemAdd((__bridge CFDictionaryRef) query, NULL);

        if(status == errSecDuplicateItem) {
            // Sure, okay, fine, we'll update.
            NSMutableDictionary* update = [@{(id)kSecValueData: v_data,
                                             } mutableCopy];

            status = SecItemUpdate((__bridge CFDictionaryRef) query, (__bridge CFDictionaryRef)update);
        }

        if(status) {
            secerror("Couldn't save tlks to keychain %d", (int)status);
        }
    }];
    [identities enumerateObjectsUsingBlock:^(NSData *v_data, NSUInteger idx, BOOL *stop) {
        SecKeyRef publicKey = NULL;
        SecKeyRef privKey = NULL;
        CFDataRef public_key_hash = NULL;
        NSMutableDictionary* query = NULL;
        CFStringRef peerid = NULL;
        OSStatus status;

        NSDictionary *keyAttributes = @{
                                        (__bridge id)kSecAttrKeyClass : (__bridge id)kSecAttrKeyClassPrivate,
                                        (__bridge id)kSecAttrKeyType : (__bridge id)kSecAttrKeyTypeEC,
                                        };

        privKey = SecKeyCreateWithData((__bridge CFDataRef)v_data, (__bridge CFDictionaryRef)keyAttributes, NULL);
        require_action_quiet(privKey, exit, secnotice("piggy","privKey failed to be created"));

        publicKey = SecKeyCreatePublicFromPrivate(privKey);
        require_action_quiet(publicKey, exit, secnotice("piggy","public key failed to be created"));

        public_key_hash = SecKeyCopyPublicKeyHash(publicKey);
        require_action_quiet(public_key_hash, exit, secnotice("piggy","can't create public key hash"));

        peerid = SOSCopyIDOfKey(publicKey, NULL);

        query = [@{
            (id)kSecClass : (id)kSecClassKey,
            (id)kSecUseDataProtectionKeychain : @YES,
            (id)kSecAttrAccessGroup: @"com.apple.security.sos",
            (id)kSecAttrApplicationLabel : (__bridge NSData*)public_key_hash,
            (id)kSecAttrLabel : [NSString stringWithFormat: @"Cloud Identity-piggy-%@", peerid],
            (id)kSecAttrSynchronizable : (id)kCFBooleanTrue,
            (id)kSecUseTombstones : (id)kCFBooleanTrue,
            (id)kSecValueData : v_data,
        } mutableCopy];

        status = SecItemAdd((__bridge CFDictionaryRef) query, NULL);

        if(status == errSecDuplicateItem) {
            // Sure, okay, fine, we'll update.
            NSMutableDictionary* update = [@{
                                             (id)kSecValueData: v_data,
                                             } mutableCopy];
            query[(id)kSecValueData] = nil;
            status = SecItemUpdate((__bridge CFDictionaryRef) query, (__bridge CFDictionaryRef)update);
        }

        if(status) {
            secerror("Couldn't save backupV0 to keychain %d", (int)status);
        }

    exit:
        CFReleaseNull(publicKey);
        CFReleaseNull(privKey);
        CFReleaseNull(peerid);
        CFReleaseNull(public_key_hash);
        secnotice("piggy","key not available");
    }];

    return true;
}

static const uint8_t *
piggy_decode_data(const uint8_t *der, const uint8_t *der_end, NSData **data)
{
    size_t body_length = 0;
    const uint8_t *body = ccder_decode_tl(CCDER_OCTET_STRING, &body_length, der, der_end);
    if(body == NULL)
        return NULL;
    *data = [NSData dataWithBytes:body length:body_length];
    return body + body_length;

}

static NSMutableArray *
parse_identies(const uint8_t *der, const uint8_t *der_end)
{
    NSMutableArray<NSData *>* array = [NSMutableArray array];

    while (der != der_end) {
        NSData *data = NULL;

        der = piggy_decode_data(der, der_end, &data);
        if (der == NULL)
            return NULL;
        if (data)
            [array addObject:data];
    }

    return array;
}

static NSMutableArray *
SOSPiggyCreateDecodedTLKs(const uint8_t *der, const uint8_t *der_end)
{
    NSMutableArray *array = [NSMutableArray array];

    while (der != der_end) {
        NSMutableDictionary<NSString *,id> *item = [NSMutableDictionary dictionary];
        NSData *data = NULL;
        size_t item_size = 0;

        const uint8_t *item_der = ccder_decode_tl(CCDER_CONSTRUCTED_SEQUENCE, &item_size, der, der_end);
        if (item_der == NULL)
            return NULL;
        const uint8_t *end_item_der = item_der + item_size;

        item_der = piggy_decode_data(item_der, end_item_der, &data);
        if (der == NULL)
            return NULL;

        item[(__bridge id)kSecValueData] = data;
        data = NULL;

        item_der = piggy_decode_data(item_der, end_item_der, &data);
        if (item_der == NULL)
            return NULL;
        if ([data length] != sizeof(uuid_t)) {
            return NULL;
        }

        NSString *uuidString = [[[NSUUID alloc] initWithUUIDBytes:[data bytes]] UUIDString];
        item[(__bridge id)kSecAttrAccount] = uuidString;

        NSString *view = NULL;
        uint64_t r = 0;
        const uint8_t *choice_der = NULL;
        choice_der = ccder_decode_uint64(&r, item_der, end_item_der);
        if (choice_der == NULL) {
            /* try other branch of CHOICE, a string */
            CFErrorRef localError = NULL;
            CFStringRef string = NULL;

            choice_der = der_decode_string(NULL, 0, &string, &localError, item_der, end_item_der);
            if (choice_der == NULL || string == NULL) {
                CFReleaseNull(string);
                secnotice("piggy", "Failed to parse view name");
                return NULL;
            }
            CFReleaseNull(localError);
            item_der = choice_der;
            view = CFBridgingRelease(string);
        } else {
            if (r == kTLKManatee)
                view = @"Manatee";
            else if (r == kTLKEngram)
                view = @"Engram";
            else if (r == kTLKAutoUnlock)
                view = @"AutoUnlock";
            else if (r == kTLKHealth)
                view = @"Health";
            else {
                secnotice("piggy", "unexpected view number: %d", (int)r);
                return NULL;
            }
            item_der = choice_der;
        }
        item[(__bridge id)kSecAttrServer] = view;

        if (item_der != end_item_der) {
            return NULL;
        }
        secnotice("piggy", "Adding %@ %@", view, uuidString);

        [array addObject:item];

        der = end_item_der;
    }
    return array;
}

NSDictionary *
SOSPiggyCopyInitialSyncData(const uint8_t** der, const uint8_t *der_end)
{
    NSMutableDictionary *results = [NSMutableDictionary dictionary];
    size_t seq_size;

    const uint8_t *topSeq = ccder_decode_tl(CCDER_CONSTRUCTED_SEQUENCE, &seq_size, *der, der_end);
    if(topSeq == NULL){
        secnotice("piggy", "Failed to parse CONS SEQ");
        return NULL;
    }

    /* parse idents */
    const uint8_t *ider = ccder_decode_tl(CCDER_CONSTRUCTED_SEQUENCE, &seq_size, topSeq, der_end);
    if (ider == NULL){
        secnotice("piggy", "Failed to parse CONS SEQ of ident");
        return NULL;
    }
    NSArray *idents = parse_identies(ider, ider + seq_size);
    if (idents)
        results[@"idents"]  = idents;
    topSeq = ider + seq_size;

    /* parse tlks */
    const uint8_t *tder = ccder_decode_tl(CCDER_CONSTRUCTED_SEQUENCE, &seq_size, topSeq, der_end);
    if (tder == NULL){
        secnotice("piggy", "Failed to parse CONS SEQ of TLKs");
        return NULL;
    }
    NSMutableArray *tlks = SOSPiggyCreateDecodedTLKs(tder, tder + seq_size);
    if (tlks)
        results[@"tlks"]  = tlks;
    *der = tder + seq_size;

    /* Don't check length here so we can add more data */

    if(results.count == 0 || tlks.count == 0){
        secnotice("piggy","NO DATA, falling back to waiting 5 minutes for initial sync to finish");
        results = NULL;
    }
    
    return results;
}

bool
SOSPiggyBackBlobCreateFromDER(SOSGenCountRef  *retGencount,
                              SecKeyRef *retPubKey,
                              CFDataRef *retSignature,
                              const uint8_t** der_p, const uint8_t *der_end,
                              PiggyBackProtocolVersion version,
                              bool *setInitialSyncTimeoutToV0,
                              CFErrorRef *error)
{
    const uint8_t *sequence_end;
    SOSGenCountRef gencount = NULL;
    CFDataRef signature = NULL;
    CFDataRef publicBytes = NULL;
    
    bool res = true;

    *setInitialSyncTimeoutToV0 = true;

    *der_p = ccder_decode_constructed_tl(CCDER_CONSTRUCTED_SEQUENCE, &sequence_end, *der_p, der_end);
    require_action_quiet(sequence_end != NULL, errOut,
                         SOSCreateError(kSOSErrorBadFormat, CFSTR("Bad Blob DER"), (error != NULL) ? *error : NULL, error));
    *der_p = der_decode_number(kCFAllocatorDefault, 0, &gencount, error, *der_p, sequence_end);
    *der_p = der_decode_data_or_null(kCFAllocatorDefault, &publicBytes, error, *der_p, sequence_end);
    *der_p = der_decode_data_or_null(kCFAllocatorDefault, &signature, error, *der_p, sequence_end);
    
    if(version == kPiggyV1){
        NSDictionary* initialSyncDict = SOSPiggyCopyInitialSyncData(der_p, der_end);
        if (initialSyncDict) {
            NSArray* idents = initialSyncDict[@"idents"];
            NSArray* tlks = initialSyncDict[@"tlks"];
            SOSPiggyBackAddToKeychain(idents, tlks);
            *setInitialSyncTimeoutToV0 = false;
        }
        /* Don't check length here so we can add more data */
    }
    else{ //V0
        secnotice("piggy","Piggybacking version 0, setting initial sync timeout to 5 minutes");
        *setInitialSyncTimeoutToV0 = true;
        require_action_quiet(*der_p && *der_p == der_end, errOut,
                             SOSCreateError(kSOSErrorBadFormat, CFSTR("Didn't consume all bytes for pbblob"), (error != NULL) ? *error : NULL, error));
    }

    *retPubKey = SecKeyCreateFromPublicData(kCFAllocatorDefault, kSecECDSAAlgorithmID, publicBytes);
    require_quiet(*retPubKey, errOut);
    *retGencount = gencount;
    *retSignature = signature;

    res = true;

errOut:
    if(!res) {
        CFReleaseNull(gencount);
        CFReleaseNull(signature);
    }
    CFReleaseNull(publicBytes);
    
    return res;
}

bool
SOSPiggyBackBlobCreateFromData(SOSGenCountRef *gencount,
                               SecKeyRef *pubKey,
                               CFDataRef *signature,
                               CFDataRef blobData,
                               PiggyBackProtocolVersion version,
                               bool *setInitialSyncTimeoutToV0,
                               CFErrorRef *error)
{
    size_t size = CFDataGetLength(blobData);
    const uint8_t *der = CFDataGetBytePtr(blobData);
    return SOSPiggyBackBlobCreateFromDER(gencount, pubKey, signature, &der, der + size, version, setInitialSyncTimeoutToV0, error);
}

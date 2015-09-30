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


#include "SOSAccountPriv.h"

//
// DER Encoding utilities
//

static const uint8_t* ccder_decode_null(const uint8_t* der, const uint8_t *der_end)
{
    if (NULL == der)
        return NULL;

    size_t payload_size = 0;
    const uint8_t *payload = ccder_decode_tl(CCDER_NULL, &payload_size, der, der_end);

    if (NULL == payload || payload_size != 0) {
        return NULL;
    }

    return payload + payload_size;
}


static size_t ccder_sizeof_null(void)
{
    return ccder_sizeof(CCDER_NULL, 0);
}


static uint8_t* ccder_encode_null(const uint8_t *der, uint8_t *der_end)
{
    return ccder_encode_tl(CCDER_NULL, 0, der, der_end);
}


//
// Encodes data or a zero length data
//
size_t der_sizeof_data_or_null(CFDataRef data, CFErrorRef* error)
{
    if (data) {
        return der_sizeof_data(data, error);
    } else {
        return der_sizeof_null(kCFNull, error);
    }
}

uint8_t* der_encode_data_or_null(CFDataRef data, CFErrorRef* error, const uint8_t* der, uint8_t* der_end)
{
    if (data) {
        return der_encode_data(data, error, der, der_end);
    } else {
        return der_encode_null(kCFNull, error, der, der_end);
    }
}


const uint8_t* der_decode_data_or_null(CFAllocatorRef allocator, CFDataRef* data,
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
// Encodes data or a zero length data
//
size_t der_sizeof_fullpeer_or_null(SOSFullPeerInfoRef full_peer, CFErrorRef* error)
{
    if (full_peer) {
        return SOSFullPeerInfoGetDEREncodedSize(full_peer, error);
    } else {
        return ccder_sizeof_null();
    }
}

uint8_t* der_encode_fullpeer_or_null(SOSFullPeerInfoRef full_peer, CFErrorRef* error, const uint8_t* der, uint8_t* der_end)
{
    if (full_peer) {
        return SOSFullPeerInfoEncodeToDER(full_peer, error, der, der_end);
    } else {
        return ccder_encode_null(der, der_end);
    }
}


const uint8_t* der_decode_fullpeer_or_null(CFAllocatorRef allocator, SOSFullPeerInfoRef* full_peer,
                                           CFErrorRef* error,
                                           const uint8_t* der, const uint8_t* der_end)
{
    ccder_tag tag;

    require_action_quiet(ccder_decode_tag(&tag, der, der_end), fail, der = NULL);

    require_action_quiet(full_peer, fail, der = NULL);

    if (tag == CCDER_NULL) {
        der = ccder_decode_null(der, der_end);
    } else  {
        *full_peer = SOSFullPeerInfoCreateFromDER(kCFAllocatorDefault, error, &der, der_end);
    }

fail:
    return der;
}


//
// Mark: public_bytes encode/decode
//

size_t der_sizeof_public_bytes(SecKeyRef publicKey, CFErrorRef* error)
{
    CFDataRef publicData = NULL;
    
    if (publicKey)
        SecKeyCopyPublicBytes(publicKey, &publicData);
    
    size_t size = der_sizeof_data_or_null(publicData, error);
    
    CFReleaseNull(publicData);
    
    return size;
}

uint8_t* der_encode_public_bytes(SecKeyRef publicKey, CFErrorRef* error, const uint8_t* der, uint8_t* der_end)
{
    CFDataRef publicData = NULL;
    
    if (publicKey)
        SecKeyCopyPublicBytes(publicKey, &publicData);
    
    uint8_t *result = der_encode_data_or_null(publicData, error, der, der_end);
    
    CFReleaseNull(publicData);
    
    return result;
}

const uint8_t* der_decode_public_bytes(CFAllocatorRef allocator, CFIndex algorithmID, SecKeyRef* publicKey, CFErrorRef* error, const uint8_t* der, const uint8_t* der_end)
{
    CFDataRef dataFound = NULL;
    der = der_decode_data_or_null(allocator, &dataFound, error, der, der_end);
    
    if (der && dataFound && publicKey) {
        *publicKey = SecKeyCreateFromPublicData(allocator, algorithmID, dataFound);
    }
    CFReleaseNull(dataFound);
    
    return der;
}


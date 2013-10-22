//
//  der_data.c
//  utilities
//
//  Created by Mitch Adler on 6/18/12.
//  Copyright (c) 2012 Apple Inc. All rights reserved.
//

#include <stdio.h>

#include "utilities/SecCFRelease.h"
#include "utilities/der_plist.h"
#include "utilities/der_plist_internal.h"

#include <corecrypto/ccder.h>
#include <CoreFoundation/CoreFoundation.h>


const uint8_t* der_decode_data_mutable(CFAllocatorRef allocator, CFOptionFlags mutability,
                                       CFMutableDataRef* data, CFErrorRef *error,
                                       const uint8_t* der, const uint8_t *der_end)
{
    if (NULL == der)
        return NULL;

    size_t payload_size = 0;
    const uint8_t *payload = ccder_decode_tl(CCDER_OCTET_STRING, &payload_size, der, der_end);

    if (NULL == payload || (der_end - payload) < payload_size) {
        SecCFDERCreateError(kSecDERErrorUnknownEncoding, CFSTR("Unknown data encoding"), NULL, error);
        return NULL;
    }

    *data = CFDataCreateMutable(allocator, 0);

    if (NULL == *data) {
        SecCFDERCreateError(kSecDERErrorUnderlyingError, CFSTR("Failed to create data"), NULL, error);
        return NULL;
    }

    CFDataAppendBytes(*data, payload, payload_size);

    return payload + payload_size;
}


const uint8_t* der_decode_data(CFAllocatorRef allocator, CFOptionFlags mutability,
                               CFDataRef* data, CFErrorRef *error,
                               const uint8_t* der, const uint8_t *der_end)
{
    if (NULL == der)
        return NULL;

    size_t payload_size = 0;
    const uint8_t *payload = ccder_decode_tl(CCDER_OCTET_STRING, &payload_size, der, der_end);

    if (NULL == payload || (der_end - payload) < payload_size) {
        SecCFDERCreateError(kSecDERErrorUnknownEncoding, CFSTR("Unknown data encoding"), NULL, error);
        return NULL;
    }
    
    *data = CFDataCreate(allocator, payload, payload_size);

    if (NULL == *data) {
        SecCFDERCreateError(kSecDERErrorUnderlyingError, CFSTR("Failed to create data"), NULL, error);
        return NULL;
    }

    return payload + payload_size;
}


size_t der_sizeof_data(CFDataRef data, CFErrorRef *error)
{
    return ccder_sizeof_raw_octet_string(CFDataGetLength(data));
}


uint8_t* der_encode_data(CFDataRef data, CFErrorRef *error,
                         const uint8_t *der, uint8_t *der_end)
{
    const CFIndex data_length = CFDataGetLength(data);

    return ccder_encode_tl(CCDER_OCTET_STRING, data_length, der,
           ccder_encode_body(data_length, CFDataGetBytePtr(data), der, der_end));

}

//
//  der_string.c
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


const uint8_t* der_decode_string(CFAllocatorRef allocator, CFOptionFlags mutability,
                                 CFStringRef* string, CFErrorRef *error,
                                 const uint8_t* der, const uint8_t *der_end)
{
    if (NULL == der)
        return NULL;

    size_t payload_size = 0;
    const uint8_t *payload = ccder_decode_tl(CCDER_UTF8_STRING, &payload_size, der, der_end);

    if (NULL == payload || (der_end - payload) < payload_size){
        SecCFDERCreateError(kSecDERErrorUnknownEncoding, CFSTR("Unknown string encoding"), NULL, error);
        return NULL;
    }

    *string = CFStringCreateWithBytes(allocator, payload, payload_size, kCFStringEncodingUTF8, false);

    if (NULL == *string) {
        SecCFDERCreateError(kSecDERErrorAllocationFailure, CFSTR("String allocation failed"), NULL, error);
        return NULL;
    }

    return payload + payload_size;
}


size_t der_sizeof_string(CFStringRef str, CFErrorRef *error)
{
    const CFIndex str_length    = CFStringGetLength(str);
    const CFIndex maximum       = CFStringGetMaximumSizeForEncoding(str_length, kCFStringEncodingUTF8);

    CFIndex encodedLen = 0;
    CFIndex converted = CFStringGetBytes(str, CFRangeMake(0, str_length), kCFStringEncodingUTF8, 0, false, NULL, maximum, &encodedLen);

    return ccder_sizeof(CCDER_UTF8_STRING, (converted == str_length) ? encodedLen : 0);
}


uint8_t* der_encode_string(CFStringRef string, CFErrorRef *error,
                           const uint8_t *der, uint8_t *der_end)
{
    // Obey the NULL allowed rules.
    if (!der_end)
        return NULL;

    const CFIndex str_length = CFStringGetLength(string);

    ptrdiff_t der_space = der_end - der;
    CFIndex bytes_used = 0;
    uint8_t *buffer = der_end - der_space;
    CFIndex converted = CFStringGetBytes(string, CFRangeMake(0, str_length), kCFStringEncodingUTF8, 0, false, buffer, der_space, &bytes_used);
    if (converted != str_length){
        SecCFDERCreateError(kSecDERErrorUnderlyingError, CFSTR("String extraction failed"), NULL, error);
        return NULL;
    }

    return ccder_encode_tl(CCDER_UTF8_STRING, bytes_used, der,
           ccder_encode_body(bytes_used, buffer, der, der_end));

}

//
//  der_boolean.c
//  utilities
//
//  Created by Mitch Adler on 6/19/12.
//  Copyright (c) 2012 Apple Inc. All rights reserved.
//

#include <stdio.h>

#include "utilities/SecCFRelease.h"
#include "utilities/der_plist.h"
#include "utilities/der_plist_internal.h"

#include <corecrypto/ccder.h>
#include <CoreFoundation/CoreFoundation.h>


const uint8_t* der_decode_boolean(CFAllocatorRef allocator, CFOptionFlags mutability,
                                  CFBooleanRef* boolean, CFErrorRef *error,
                                  const uint8_t* der, const uint8_t *der_end)
{
    if (NULL == der)
        return NULL;

    size_t payload_size = 0;
    const uint8_t *payload = ccder_decode_tl(CCDER_BOOLEAN, &payload_size, der, der_end);

    if (NULL == payload || (der_end - payload) < payload_size || payload_size != 1) {
        SecCFDERCreateError(kSecDERErrorUnknownEncoding, CFSTR("Unknown boolean encoding"), NULL, error);
        return NULL;
    }

    *boolean = *payload ? kCFBooleanTrue : kCFBooleanFalse;

    return payload + payload_size;
}


size_t der_sizeof_boolean(CFBooleanRef data __unused, CFErrorRef *error)
{
    return ccder_sizeof(CCDER_BOOLEAN, 1);
}


uint8_t* der_encode_boolean(CFBooleanRef boolean, CFErrorRef *error,
                            const uint8_t *der, uint8_t *der_end)
{
    uint8_t value = CFBooleanGetValue(boolean);

    return ccder_encode_tl(CCDER_BOOLEAN, 1, der,
           ccder_encode_body(1, &value, der, der_end));

}

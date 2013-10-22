//
//  der_null.c
//  utilities
//
//  Created by Josh Osborne on 4/30/13.
//  Copyright (c) 2013 Apple Inc. All rights reserved.
//

#include <stdio.h>

#include "utilities/SecCFRelease.h"
#include "utilities/der_plist.h"
#include "utilities/der_plist_internal.h"

#include <corecrypto/ccder.h>
#include <CoreFoundation/CoreFoundation.h>


const uint8_t* der_decode_null(CFAllocatorRef allocator, CFOptionFlags mutability,
                                  CFNullRef* nul, CFErrorRef *error,
                                  const uint8_t* der, const uint8_t *der_end)
{
    if (NULL == der)
        return NULL;
	
    size_t payload_size = 0;
    const uint8_t *payload = ccder_decode_tl(CCDER_NULL, &payload_size, der, der_end);
	
	if (NULL == payload || payload_size != 0) {
        SecCFDERCreateError(kSecDERErrorUnknownEncoding, CFSTR("Unknown null encoding"), NULL, error);
        return NULL;
    }
	
    *nul = kCFNull;
	
    return payload + payload_size;
}


size_t der_sizeof_null(CFNullRef data __unused, CFErrorRef *error)
{
    return ccder_sizeof(CCDER_NULL, 0);
}


uint8_t* der_encode_null(CFNullRef boolean __unused, CFErrorRef *error,
                            const uint8_t *der, uint8_t *der_end)
{
	return ccder_encode_tl(CCDER_NULL, 0, der, der_end);
}

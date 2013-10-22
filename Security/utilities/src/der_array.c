//
//  der_array.c
//  utilities
//
//  Created by Mitch Adler on 7/2/12.
//  Copyright (c) 2012 Apple Inc. All rights reserved.
//

#include <stdio.h>

#include "utilities/SecCFRelease.h"
#include "utilities/der_plist.h"
#include "utilities/der_plist_internal.h"

#include <corecrypto/ccder.h>
#include <CoreFoundation/CoreFoundation.h>


const uint8_t* der_decode_array(CFAllocatorRef allocator, CFOptionFlags mutability,
                                CFArrayRef* array, CFErrorRef *error,
                                const uint8_t* der, const uint8_t *der_end)
{
    if (NULL == der)
        return NULL;

    CFMutableArrayRef result = CFArrayCreateMutable(allocator, 0, &kCFTypeArrayCallBacks);

    const uint8_t *elements_end;
    const uint8_t *current_element = ccder_decode_sequence_tl(&elements_end, der, der_end);
    
    while (current_element != NULL && current_element < elements_end) {
        CFPropertyListRef element = NULL;
        current_element = der_decode_plist(allocator, mutability, &element, error, current_element, elements_end);
        if (current_element) {
            CFArrayAppendValue(result, element);
            CFReleaseNull(element);
        }
    }

    if (current_element) {
        *array = result;
        result = NULL;
    }

    CFReleaseNull(result);
    return current_element;
}


size_t der_sizeof_array(CFArrayRef data, CFErrorRef *error)
{
    size_t body_size = 0;
    for(CFIndex position = CFArrayGetCount(data) - 1;
        position >= 0;
        --position)
    {
        body_size += der_sizeof_plist(CFArrayGetValueAtIndex(data, position), error);
    }
    return ccder_sizeof(CCDER_CONSTRUCTED_SEQUENCE, body_size);
}


uint8_t* der_encode_array(CFArrayRef array, CFErrorRef *error,
                          const uint8_t *der, uint8_t *der_end)
{
    uint8_t* original_der_end = der_end;
    for(CFIndex position = CFArrayGetCount(array) - 1;
        position >= 0;
        --position)
    {
        der_end = der_encode_plist(CFArrayGetValueAtIndex(array, position), error, der, der_end);
    }
    
    return ccder_encode_constructed_tl(CCDER_CONSTRUCTED_SEQUENCE, original_der_end, der, der_end);
}

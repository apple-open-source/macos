//
//  der_plist_internal.h
//  utilities
//
//  Created by Mitch Adler on 7/2/12.
//  Copyright (c) 2012 Apple Inc. All rights reserved.
//

#ifndef _DER_PLIST_INTERNAL_H_
#define _DER_PLIST_INTERNAL_H_

#include <CoreFoundation/CoreFoundation.h>

void SecCFDERCreateError(CFIndex errorCode, CFStringRef descriptionString, CFErrorRef previousError, CFErrorRef *newError);


// CFArray <-> DER
size_t der_sizeof_array(CFArrayRef array, CFErrorRef *error);

uint8_t* der_encode_array(CFArrayRef array, CFErrorRef *error,
                          const uint8_t *der, uint8_t *der_end);

const uint8_t* der_decode_array(CFAllocatorRef allocator, CFOptionFlags mutability,
                                CFArrayRef* array, CFErrorRef *error,
                                const uint8_t* der, const uint8_t *der_end);

// CFNull <-> DER
size_t der_sizeof_null(CFNullRef	nul, CFErrorRef *error);

uint8_t* der_encode_null(CFNullRef	nul, CFErrorRef *error,
                            const uint8_t *der, uint8_t *der_end);

const uint8_t* der_decode_null(CFAllocatorRef allocator, CFOptionFlags mutability,
                                  CFNullRef	*nul, CFErrorRef *error,
                                  const uint8_t* der, const uint8_t *der_end);


// CFBoolean <-> DER
size_t der_sizeof_boolean(CFBooleanRef boolean, CFErrorRef *error);

uint8_t* der_encode_boolean(CFBooleanRef boolean, CFErrorRef *error,
                            const uint8_t *der, uint8_t *der_end);

const uint8_t* der_decode_boolean(CFAllocatorRef allocator, CFOptionFlags mutability,
                                  CFBooleanRef* boolean, CFErrorRef *error,
                                  const uint8_t* der, const uint8_t *der_end);

// CFData <-> DER
size_t der_sizeof_data(CFDataRef data, CFErrorRef *error);

uint8_t* der_encode_data(CFDataRef data, CFErrorRef *error,
                         const uint8_t *der, uint8_t *der_end);

const uint8_t* der_decode_data(CFAllocatorRef allocator, CFOptionFlags mutability,
                               CFDataRef* data, CFErrorRef *error,
                               const uint8_t* der, const uint8_t *der_end);

const uint8_t* der_decode_data_mutable(CFAllocatorRef allocator, CFOptionFlags mutability,
                                       CFMutableDataRef* data, CFErrorRef *error,
                                       const uint8_t* der, const uint8_t *der_end);


// CFDate <-> DER
size_t der_sizeof_date(CFDateRef date, CFErrorRef *error);

uint8_t* der_encode_date(CFDateRef date, CFErrorRef *error,
                         const uint8_t *der, uint8_t *der_end);

const uint8_t* der_decode_date(CFAllocatorRef allocator, CFOptionFlags mutability,
                               CFDateRef* date, CFErrorRef *error,
                               const uint8_t* der, const uint8_t *der_end);


// CFDictionary <-> DER
size_t der_sizeof_dictionary(CFDictionaryRef dictionary, CFErrorRef *error);

uint8_t* der_encode_dictionary(CFDictionaryRef dictionary, CFErrorRef *error,
                               const uint8_t *der, uint8_t *der_end);

const uint8_t* der_decode_dictionary(CFAllocatorRef allocator, CFOptionFlags mutability,
                                     CFDictionaryRef* dictionary, CFErrorRef *error,
                                     const uint8_t* der, const uint8_t *der_end);

// CFNumber <-> DER
// Currently only supports signed 64 bit values. No floating point.
size_t der_sizeof_number(CFNumberRef number, CFErrorRef *error);

uint8_t* der_encode_number(CFNumberRef number, CFErrorRef *error,
                           const uint8_t *der, uint8_t *der_end);

const uint8_t* der_decode_number(CFAllocatorRef allocator, CFOptionFlags mutability,
                                 CFNumberRef* number, CFErrorRef *error,
                                 const uint8_t* der, const uint8_t *der_end);

// CFString <-> DER
size_t der_sizeof_string(CFStringRef string, CFErrorRef *error);

uint8_t* der_encode_string(CFStringRef string, CFErrorRef *error,
                           const uint8_t *der, uint8_t *der_end);

const uint8_t* der_decode_string(CFAllocatorRef allocator, CFOptionFlags mutability,
                                 CFStringRef* string, CFErrorRef *error,
                                 const uint8_t* der, const uint8_t *der_end);

#endif

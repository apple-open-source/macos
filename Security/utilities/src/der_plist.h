//
//  der_plist.h
//  utilities
//
//  Created by Mitch Adler on 6/18/12.
//  Copyright (c) 2012 Apple Inc. All rights reserved.
//

#ifndef _DER_PLIST_H_
#define _DER_PLIST_H_

#include <CoreFoundation/CoreFoundation.h>

//
// Error Codes for PropertyList <-> DER
//

static const CFIndex kSecDERErrorUnknownEncoding = -1;
static const CFIndex kSecDERErrorUnsupportedCFObject = -2;
static const CFIndex kSecDERErrorUnsupportedDERType = -2;
static const CFIndex kSecDERErrorAllocationFailure = -3;
static const CFIndex kSecDERErrorUnsupportedNumberType = -4;

static const CFIndex kSecDERErrorUnderlyingError = -100;

extern CFStringRef sSecDERErrorDomain;

// PropertyList <-> DER Functions

size_t der_sizeof_plist(CFPropertyListRef pl, CFErrorRef *error);

uint8_t* der_encode_plist(CFPropertyListRef pl, CFErrorRef *error,
                           const uint8_t *der, uint8_t *der_end);

const uint8_t* der_decode_plist(CFAllocatorRef pl, CFOptionFlags mutability,
                                CFPropertyListRef* cf, CFErrorRef *error,
                                const uint8_t* der, const uint8_t *der_end);

#endif

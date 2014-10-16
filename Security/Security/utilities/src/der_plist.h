/*
 * Copyright (c) 2012-2014 Apple Inc. All Rights Reserved.
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


#ifndef _DER_PLIST_H_
#define _DER_PLIST_H_

#include <CoreFoundation/CoreFoundation.h>

//
// Error Codes for PropertyList <-> DER
//

static const CFIndex kSecDERErrorUnknownEncoding = -1;
static const CFIndex kSecDERErrorUnsupportedDERType = -2;
static const CFIndex kSecDERErrorAllocationFailure = -3;
static const CFIndex kSecDERErrorUnsupportedNumberType = -4;
static const CFIndex kSecDERErrorUnsupportedCFObject = -5;

extern CFStringRef sSecDERErrorDomain;

// PropertyList <-> DER Functions

size_t der_sizeof_plist(CFPropertyListRef pl, CFErrorRef *error);

uint8_t* der_encode_plist(CFPropertyListRef pl, CFErrorRef *error,
                           const uint8_t *der, uint8_t *der_end);

const uint8_t* der_decode_plist(CFAllocatorRef pl, CFOptionFlags mutability,
                                CFPropertyListRef* cf, CFErrorRef *error,
                                const uint8_t* der, const uint8_t *der_end);

#endif

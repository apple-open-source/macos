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


#include <stdio.h>

#include "utilities/SecCFRelease.h"
#include "utilities/der_plist.h"
#include "utilities/der_plist_internal.h"

#include <corecrypto/ccder.h>
#include <CoreFoundation/CoreFoundation.h>

//
// der..CFPropertyList
//
// We support:
//
//   CFBoolean
//   CFData
//   CFDate
//   CFString
//   CFNumber
//   CFNull
//
//   CFArray
//   CFDictionary
//
//

const uint8_t* der_decode_plist(CFAllocatorRef allocator, CFOptionFlags mutability,
                                CFPropertyListRef* pl, CFErrorRef *error,
                                const uint8_t* der, const uint8_t *der_end)
{    if (NULL == der)
    return NULL;

    ccder_tag tag;
    if (NULL == ccder_decode_tag(&tag, der, der_end))
        return NULL;

    switch (tag) {
		case CCDER_NULL:
			return der_decode_null(allocator, mutability, (CFNullRef*)pl, error, der, der_end);
        case CCDER_BOOLEAN:
            return der_decode_boolean(allocator, mutability, (CFBooleanRef*)pl, error, der, der_end);
        case CCDER_OCTET_STRING:
            return der_decode_data(allocator, mutability, (CFDataRef*)pl, error, der, der_end);
        case CCDER_GENERALIZED_TIME:
            return der_decode_date(allocator, mutability, (CFDateRef*)pl, error, der, der_end);
        case CCDER_CONSTRUCTED_SEQUENCE:
            return der_decode_array(allocator, mutability, (CFArrayRef*)pl, error, der, der_end);
        case CCDER_UTF8_STRING:
            return der_decode_string(allocator, mutability, (CFStringRef*)pl, error, der, der_end);
        case CCDER_INTEGER:
            return der_decode_number(allocator, mutability, (CFNumberRef*)pl, error, der, der_end);
        case CCDER_CONSTRUCTED_SET:
            return der_decode_dictionary(allocator, mutability, (CFDictionaryRef*)pl, error, der, der_end);
        default:
            SecCFDERCreateError(kSecDERErrorUnsupportedDERType, CFSTR("Unsupported DER Type"), NULL, error);
            return NULL;
    }
}


size_t der_sizeof_plist(CFPropertyListRef pl, CFErrorRef *error)
{
    if (!pl) {
        SecCFDERCreateError(kSecDERErrorUnsupportedCFObject, CFSTR("Null CFType"), NULL, error);
        return 0;
    }

    CFTypeID  dataType = CFGetTypeID(pl);

    if (CFArrayGetTypeID() == dataType)
        return der_sizeof_array((CFArrayRef) pl, error);
    else if (CFBooleanGetTypeID() == dataType)
        return der_sizeof_boolean((CFBooleanRef) pl, error);
    else if (CFDataGetTypeID() == dataType)
        return der_sizeof_data((CFDataRef) pl, error);
    else if (CFDateGetTypeID() == dataType)
        return der_sizeof_date((CFDateRef) pl, error);
    else if (CFDictionaryGetTypeID() == dataType)
        return der_sizeof_dictionary((CFDictionaryRef) pl, error);
    else if (CFStringGetTypeID() == dataType)
        return der_sizeof_string((CFStringRef) pl, error);
    else if (CFNumberGetTypeID() == dataType)
        return der_sizeof_number((CFNumberRef) pl, error);
	if (CFNullGetTypeID() == dataType)
		return der_sizeof_null((CFNullRef) pl, error);
	else {
        SecCFDERCreateError(kSecDERErrorUnsupportedCFObject, CFSTR("Unsupported CFType"), NULL, error);
        return 0;
    }
}


uint8_t* der_encode_plist(CFPropertyListRef pl, CFErrorRef *error,
                          const uint8_t *der, uint8_t *der_end)
{
    if (!pl) {
        SecCFDERCreateError(kSecDERErrorUnsupportedCFObject, CFSTR("Null CFType"), NULL, error);
        return NULL;
    }

    CFTypeID  dataType = CFGetTypeID(pl);

    if (CFArrayGetTypeID() == dataType)
        return der_encode_array((CFArrayRef) pl, error, der, der_end);
    else if (CFBooleanGetTypeID() == dataType)
        return der_encode_boolean((CFBooleanRef) pl, error, der, der_end);
    else if (CFDataGetTypeID() == dataType)
        return der_encode_data((CFDataRef) pl, error, der, der_end);
    else if (CFDateGetTypeID() == dataType)
        return der_encode_date((CFDateRef) pl, error, der, der_end);
    else if (CFDictionaryGetTypeID() == dataType)
        return der_encode_dictionary((CFDictionaryRef) pl, error, der, der_end);
    else if (CFStringGetTypeID() == dataType)
        return der_encode_string((CFStringRef) pl, error, der, der_end);
    else if (CFNumberGetTypeID() == dataType)
        return der_encode_number((CFNumberRef) pl, error, der, der_end);
	else if (CFNullGetTypeID() == dataType)
		return der_encode_null((CFNullRef) pl, error, der, der_end);
    else {
        SecCFDERCreateError(kSecDERErrorUnsupportedCFObject, CFSTR("Unsupported CFType"), NULL, error);
        return NULL;
    }
}

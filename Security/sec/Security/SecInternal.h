/*
 * Copyright (c) 2007,2009-2010 Apple Inc. All Rights Reserved.
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

/*!
    @header SecInternal
    SecInternal defines common internal constants macros and SPI functions.
*/

#ifndef _SECURITY_SECINTERNAL_H_
#define _SECURITY_SECINTERNAL_H_

#include <assert.h>
#include <CoreFoundation/CFNumber.h>
#include <CoreFoundation/CFString.h>

#if defined(__cplusplus)
extern "C" {
#endif

#define CFReleaseSafe(CF) { CFTypeRef _cf = (CF); if (_cf) CFRelease(_cf); }
#define CFReleaseNull(CF) { CFTypeRef _cf = (CF); \
	if (_cf) { (CF) = NULL; CFRelease(_cf); } }
#define CFRetainSafe(CF) { CFTypeRef _cf = (CF); if (_cf) CFRetain(_cf); }

#define DICT_DECLARE(MAXVALUES) \
	CFIndex numValues = 0, maxValues = (MAXVALUES); \
	const void *keys[maxValues]; \
	const void *values[maxValues];

#define DICT_ADDPAIR(KEY,VALUE) do { \
	if (numValues < maxValues) { \
		keys[numValues] = (KEY); \
		values[numValues] = (VALUE); \
		numValues++; \
	} else \
		assert(false); \
} while(0)

#define DICT_CREATE(ALLOCATOR) CFDictionaryCreate((ALLOCATOR), keys, values, \
	numValues, NULL, &kCFTypeDictionaryValueCallBacks)

/* Non valid CFTimeInterval or CFAbsoluteTime. */
#define NULL_TIME	0.0


static inline CFIndex getIntValue(CFTypeRef cf) {
    if (cf) {
        if (CFGetTypeID(cf) == CFNumberGetTypeID()) {
            CFIndex value;
            CFNumberGetValue(cf, kCFNumberCFIndexType, &value);
            return value;
        } else if (CFGetTypeID(cf) == CFStringGetTypeID()) {
            return CFStringGetIntValue(cf);
        }
    }
    return -1;
}


#if defined(__cplusplus)
}
#endif

#endif /* !_SECURITY_SECINTERNAL_H_ */

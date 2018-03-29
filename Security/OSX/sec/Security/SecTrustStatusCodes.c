/*
 * Copyright (c) 2017 Apple Inc. All Rights Reserved.
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
 *
 * SecTrustStatusCodes.c - map trust result details to status codes
 *
 */

#include <Security/SecTrustPriv.h>
#include <Security/SecInternal.h>
#include <Security/SecTrustStatusCodes.h>
#include <CoreFoundation/CoreFoundation.h>

struct resultmap_entry_s {
    const CFStringRef checkstr;
    const int32_t resultcode;
};
typedef struct resultmap_entry_s resultmap_entry_t;

const resultmap_entry_t resultmap[] = {
#undef POLICYCHECKMACRO
#define POLICYCHECKMACRO(NAME, TRUSTRESULT, SUBTYPE, LEAFCHECK, PATHCHECK, LEAFONLY, CSSMERR, OSSTATUS) \
{ CFSTR(#NAME), CSSMERR },
#include "SecPolicyChecks.list"
};

//
// Returns a malloced array of SInt32 values, with the length in numStatusCodes,
// for the certificate specified by chain index in the given SecTrustRef.
//
// To match legacy behavior, the array actually allocates one element more than the
// value of numStatusCodes; if the certificate is revoked, the additional element
// at the end contains the CrlReason value.
//
// Caller must free the returned pointer.
//
SInt32 *SecTrustCopyStatusCodes(SecTrustRef trust,
    CFIndex index, CFIndex *numStatusCodes)
{
    if (!trust || !numStatusCodes) {
        return NULL;
    }
    *numStatusCodes = 0;
    CFArrayRef details = SecTrustCopyFilteredDetails(trust);
    CFIndex chainLength = (details) ? CFArrayGetCount(details) : 0;
    if (!(index < chainLength)) {
        CFReleaseSafe(details);
        return NULL;
    }
    CFDictionaryRef detail = (CFDictionaryRef)CFArrayGetValueAtIndex(details, index);
    CFIndex ix, detailCount = CFDictionaryGetCount(detail);
    *numStatusCodes = (unsigned int)detailCount;

    // Allocate one more entry than we need; this is used to store a CrlReason
    // at the end of the array.
    SInt32 *statusCodes = (SInt32*)malloc((detailCount+1) * sizeof(SInt32));
    statusCodes[*numStatusCodes] = 0;

    const unsigned int resultmaplen = sizeof(resultmap) / sizeof(resultmap_entry_t);
    const void *keys[detailCount];
    CFDictionaryGetKeysAndValues(detail, &keys[0], NULL);
    for (ix = 0; ix < detailCount; ix++) {
        CFStringRef key = (CFStringRef)keys[ix];
        SInt32 statusCode = 0;
        for (unsigned int mapix = 0; mapix < resultmaplen; mapix++) {
            CFStringRef str = (CFStringRef) resultmap[mapix].checkstr;
            if (CFStringCompare(str, key, 0) == kCFCompareEqualTo) {
                statusCode = (SInt32) resultmap[mapix].resultcode;
                break;
            }
        }
        if (statusCode == (SInt32)0x8001210C) {  /* CSSMERR_TP_CERT_REVOKED */
            SInt32 reason;
            CFNumberRef number = (CFNumberRef)CFDictionaryGetValue(detail, key);
            if (number && CFNumberGetValue(number, kCFNumberSInt32Type, &reason)) {
                statusCodes[*numStatusCodes] = (SInt32)reason;
            }
        }
        statusCodes[ix] = statusCode;
    }

    CFReleaseSafe(details);
    return statusCodes;
}

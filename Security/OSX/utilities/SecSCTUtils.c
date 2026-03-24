//
//  SecSCTUtils.c
//  utilities
/*
 * Copyright (c) 2015 Apple Inc. All Rights Reserved.
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

#include <AssertMacros.h>
#include <utilities/SecCFWrappers.h>
#include "SecSCTUtils.h"

static size_t SSLDecodeSize(const uint8_t *p)
{
    return (p[0]<<8 | p[1]);
}

CFArrayRef SecCreateSignedCertificateTimestampsArrayFromSerializedSCTList(const uint8_t *p, size_t listLen)
{
    size_t encodedListLen;
    CFMutableArrayRef sctArray = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
    __Require_Quiet(sctArray, out);

    __Require(listLen > 2 , out);
    encodedListLen = SSLDecodeSize(p); p+=2; listLen-=2;

    __Require(encodedListLen==listLen, out);

    while (listLen > 0)
    {
        size_t itemLen;
        __Require(listLen >= 2, out);
        itemLen = SSLDecodeSize(p); p += 2; listLen-=2;
        __Require(itemLen <= listLen, out);
        CFDataRef sctData = CFDataCreate(kCFAllocatorDefault, p, itemLen);
        p += itemLen; listLen -= itemLen;
        __Require(sctData, out);
        CFArrayAppendValue(sctArray, sctData);
        CFReleaseSafe(sctData);
    }

    return sctArray;

out:
    CFReleaseSafe(sctArray);
    return NULL;
}

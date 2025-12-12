/*
 * Copyright (c) 2025 Apple Inc. All Rights Reserved.
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
 */

#import "SecRevocationCRLite.h"

#include <utilities/SecCFWrappers.h>

CFGiblisFor(SecCRLiteInfo)

static void SecCRLiteInfoDestroy(CFTypeRef cf) {
    // NOP
}

static CFStringRef SecCRLiteInfoCopyFormatDescription(CFTypeRef cf, CFDictionaryRef formatOptions) {
    SecCRLiteInfoRef crliteInfo = (SecCRLiteInfoRef)cf;
    CFStringRef desc = CFStringCreateWithFormat(NULL, formatOptions, CFSTR("crliteInfo<%p> isRevoked:%@ generationUsed:%u versionUsed:%u"), crliteInfo, crliteInfo->isRevoked ? CFSTR("YES") : CFSTR("NO"), crliteInfo->generationUsed, crliteInfo->versionUsed );
    return desc;
}

CF_RETURNS_RETAINED SecCRLiteInfoRef SecCRLiteInfoCreate(bool isRevoked, uint32_t generationUsed, uint32_t versionUsed) {
    SecCRLiteInfoRef crliteInfo = CFTypeAllocate(SecCRLiteInfo, struct OpaqueSecCRLiteInfo, kCFAllocatorDefault);
    if (crliteInfo == NULL) {
        return NULL; 
    }

    crliteInfo->isRevoked = isRevoked;
    crliteInfo->generationUsed = generationUsed;
    crliteInfo->versionUsed = versionUsed;
    
    return crliteInfo;
}

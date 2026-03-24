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
 */

/*!
 @header SecRevocationCRLite
 The functions provided in SecRevocationCRLite.h provide an interface to
 the trust evaluation engine for dealing with certificate revocation using CRLite.
 */

#ifndef _SECURITY_SECREVOCATIONCRLITE_H_
#define _SECURITY_SECREVOCATIONCRLITE_H_

#include <CoreFoundation/CFRuntime.h>

typedef struct OpaqueSecCRLiteInfo *SecCRLiteInfoRef;

struct OpaqueSecCRLiteInfo {
    CFRuntimeBase _base;

    /* Whether the certificate is revoked according to CRLite */
    bool isRevoked;

    /* The Valid generation number at time of evaluation */
    uint32_t generationUsed;

    /* The Valid version number at time of evaluation */
    uint32_t versionUsed;
};

CF_RETURNS_RETAINED SecCRLiteInfoRef SecCRLiteInfoCreate(bool isRevoked, uint32_t generationUsed, uint32_t versionUsed);

CF_RETURNS_RETAINED CFDictionaryRef SecCRLiteInfoCopyInfo(SecCRLiteInfoRef validInfo);


#endif /* _SECURITY_SECREVOCATIONCRLITE_H_ */

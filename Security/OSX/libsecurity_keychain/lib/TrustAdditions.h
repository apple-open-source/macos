/*
 * Copyright (c) 2002-2012,2014 Apple Inc. All Rights Reserved.
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

//
// TrustAdditions.h
//

#ifndef	_TRUST_ADDITIONS_H_
#define _TRUST_ADDITIONS_H_

#include <CoreFoundation/CFArray.h>
#include <Security/cssmtype.h>
#include <Security/SecTrust.h>

#ifdef __cplusplus
extern "C" {
#endif

CFArrayRef CF_RETURNS_RETAINED potentialEVChainWithCertificates(CFArrayRef certificates);
CFArrayRef CF_RETURNS_RETAINED allowedEVRootsForLeafCertificate(CFArrayRef certificates);
bool isOCSPStatusCode(CSSM_RETURN statusCode);
bool isCRLStatusCode(CSSM_RETURN statusCode);
bool isRevocationStatusCode(CSSM_RETURN statusCode);
bool isRevocationServerMetaError(CSSM_RETURN statusCode);
bool ignorableRevocationStatusCode(CSSM_RETURN statusCode);
CFDictionaryRef extendedTrustResults(CFArrayRef certChain, SecTrustResultType trustResult, OSStatus tpResult, bool isEVCandidate);

#ifdef __cplusplus
}
#endif

#endif	/* _TRUST_ADDITIONS_H_ */

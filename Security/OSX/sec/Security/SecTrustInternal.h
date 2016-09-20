/*
 * Copyright (c) 2015-2016 Apple Inc. All Rights Reserved.
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
    @header SecTrustInternal
    This header provides the interface to internal functions used by SecTrust.
*/

#ifndef _SECURITY_SECTRUSTINTERNAL_H_
#define _SECURITY_SECTRUSTINTERNAL_H_

#include <Security/SecTrust.h>

__BEGIN_DECLS

/* args_in keys. */
#define kSecTrustCertificatesKey "certificates"
#define kSecTrustAnchorsKey "anchors"
#define kSecTrustAnchorsOnlyKey "anchorsOnly"
#define kSecTrustKeychainsAllowedKey "keychainsAllowed"
#define kSecTrustPoliciesKey "policies"
#define kSecTrustResponsesKey "responses"
#define kSecTrustSCTsKey "scts"
#define kSecTrustTrustedLogsKey "trustedLogs"
#define kSecTrustVerifyDateKey "verifyDate"
#define kSecTrustExceptionsKey "exceptions"

/* args_out keys. */
#define kSecTrustDetailsKey "details"
#define kSecTrustChainKey "chain"
#define kSecTrustResultKey "result"
#define kSecTrustInfoKey "info"

#if TARGET_OS_MAC && !TARGET_OS_IPHONE
SecKeyRef SecTrustCopyPublicKey_ios(SecTrustRef trust);
CFArrayRef SecTrustCopyProperties_ios(SecTrustRef trust);
#endif

__END_DECLS

#endif /* !_SECURITY_SECTRUSTINTERNAL_H_ */

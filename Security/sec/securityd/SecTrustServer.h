/*
 * Copyright (c) 2008-2009 Apple Inc. All Rights Reserved.
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
 * SecTrustServer.h - certificate trust evaluation engine
 *
 *  Created by Michael Brouwer on 12/12/08.
 *
 */

#ifndef _SECURITY_SECTRUSTSERVER_H_
#define _SECURITY_SECTRUSTSERVER_H_

#include <CoreFoundation/CFString.h>

#include <Security/SecCertificatePath.h>
#include <Security/SecTrust.h>
#include <Security/SecBasePriv.h> /* For errSecWaitForCallback. */
#include <mach/port.h>


__BEGIN_DECLS


/* args_in keys. */
#define kSecTrustCertificatesKey "certificates"
#define kSecTrustAnchorsKey "anchors"
#define kSecTrustAnchorsOnlyKey "anchorsOnly"
#define kSecTrustPoliciesKey "policies"
#define kSecTrustVerifyDateKey "verifyDate"

/* args_out keys. */
#define kSecTrustDetailsKey "details"
#define kSecTrustChainKey "chain"
#define kSecTrustResultKey "result"
#define kSecTrustInfoKey "info"

typedef struct SecPathBuilder *SecPathBuilderRef;

/* Completion callback.  You should call SecTrustSessionDestroy from this. */
typedef void(*SecPathBuilderCompleted)(const void *userData,
    SecCertificatePathRef chain, CFArrayRef details, CFDictionaryRef info,
    SecTrustResultType result);

/* Returns a new trust path builder and policy evaluation engine instance. */
SecPathBuilderRef SecPathBuilderCreate(CFArrayRef certificates,
    CFArrayRef anchors, bool anchorsOnly, CFArrayRef policies,
    CFAbsoluteTime verifyTime, CFArrayRef accessGroups,
    SecPathBuilderCompleted completed, const void *userData);

/* Returns true if it's ok to perform network operations for this builder. */
bool SecPathBuilderCanAccessNetwork(SecPathBuilderRef builder);

/* Disable or enable network access for this builder if allow is false
   network access will be disabled. */
void SecPathBuilderSetCanAccessNetwork(SecPathBuilderRef builder, bool allow);

/* Core of the trust evaluation engine, this will invoke the completed
   callback and return false if the evaluation completed, or return true if
   the evaluation is still waiting for some external event (usually the
   network). */
bool SecPathBuilderStep(SecPathBuilderRef builder);

/* Return the dispatch queue to be used by this builder. */
dispatch_queue_t SecPathBuilderGetQueue(SecPathBuilderRef builder);

/* Evaluate trust and call evaluated when done. */
void SecTrustServerEvaluateBlock(CFArrayRef certificates, CFArrayRef anchors, bool anchorsOnly, CFArrayRef policies, CFAbsoluteTime verifyTime, __unused CFArrayRef accessGroups, void (^evaluated)(SecTrustResultType tr, CFArrayRef details, CFDictionaryRef info, SecCertificatePathRef chain, CFErrorRef error));

/* Synchronously invoke SecTrustServerEvaluateBlock. */
SecTrustResultType SecTrustServerEvaluate(CFArrayRef certificates, CFArrayRef anchors, bool anchorsOnly, CFArrayRef policies, CFAbsoluteTime verifyTime, __unused CFArrayRef accessGroups, CFArrayRef *details, CFDictionaryRef *info, SecCertificatePathRef *chain, CFErrorRef *error);

void InitializeAnchorTable(void);

__END_DECLS

#endif /* !_SECURITY_SECTRUSTSERVER_H_ */

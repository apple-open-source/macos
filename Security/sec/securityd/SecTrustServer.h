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


#if defined(__cplusplus)
extern "C" {
#endif


/* args_in keys. */
#define kSecTrustCertificatesKey CFSTR("certificates")
#define kSecTrustAnchorsKey CFSTR("anchors")
#define kSecTrustAnchorsOnlyKey CFSTR("anchorsOnly")
#define kSecTrustPoliciesKey CFSTR("policies")
#define kSecTrustVerifyDateKey CFSTR("verifyDate")

/* args_out keys. */
#define kSecTrustDetailsKey CFSTR("details")
#define kSecTrustChainKey CFSTR("chain")
#define kSecTrustResultKey CFSTR("result")
#define kSecTrustInfoKey CFSTR("info")

typedef struct SecPathBuilder *SecPathBuilderRef;

/* Completion callback.  You should call SecTrustSessionDestroy from this. */
typedef void(*SecPathBuilderCompleted)(const void *userData,
    SecCertificatePathRef chain, CFArrayRef details, CFDictionaryRef info,
    SecTrustResultType result);

/* Returns a new trust path builder and policy evaluation engine instance. */
SecPathBuilderRef SecPathBuilderCreate(CFArrayRef certificates,
    CFArrayRef anchors, bool anchorsOnly, CFArrayRef policies,
    CFAbsoluteTime verifyTime,
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


/* Returns noErr if the operation has completed synchronously.  Returns
   errSecWaitForCallback if the operation is running in the background. Can
   also return paramErr if one or more of the inputs aren't correct.  Upon
   completion the completed callback is called. */
OSStatus SecTrustServerEvaluateAsync(CFDictionaryRef args_in,
    SecPathBuilderCompleted completed, const void *userData);

/* Synchronously invoke SecTrustServerEvaluateAsync. */
OSStatus SecTrustServerEvaluate(CFDictionaryRef args_in, CFTypeRef *args_out);

#if defined(__cplusplus)
}
#endif

#endif /* !_SECURITY_SECTRUSTSERVER_H_ */

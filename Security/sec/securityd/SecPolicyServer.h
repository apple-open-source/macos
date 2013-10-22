/*
 * Copyright (c) 2008-2010 Apple Inc. All Rights Reserved.
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
    @header SecPolicyServer
    The functions provided in SecPolicyServer.h provide an interface to
    trust policies dealing with certificate revocation.
*/

#ifndef _SECURITY_SECPOLICYSERVER_H_
#define _SECURITY_SECPOLICYSERVER_H_

#include <Security/SecPolicyInternal.h>

#include <securityd/policytree.h>
#include <securityd/SecTrustServer.h>

__BEGIN_DECLS

typedef struct OpaqueSecPVC *SecPVCRef;

struct OpaqueSecPVC {
    SecPathBuilderRef builder;

    /* @@@ Duplicated from builder, remove. */
	CFArrayRef policies;
	CFAbsoluteTime verifyTime;

	SecCertificatePathRef path;
	CFArrayRef details;
    CFMutableDictionaryRef info;
    policy_tree_t valid_policy_tree;
    CFDictionaryRef callbacks;
    CFIndex policyIX;

    void *rvcs;
    unsigned int asyncJobCount;

    bool check_revocation;
    bool optionally_ev;
    bool is_ev;
	bool result;
};

void SecPVCInit(SecPVCRef pvc, SecPathBuilderRef builder, CFArrayRef policies,
    CFAbsoluteTime verifyTime);
void SecPVCDelete(SecPVCRef pvc);
void SecPVCSetPath(SecPVCRef pvc, SecCertificatePathRef path,
    CFArrayRef details);
SecPolicyRef SecPVCGetPolicy(SecPVCRef pv);
CFAbsoluteTime SecPVCGetVerifyTime(SecPVCRef pv);
CFIndex SecPVCGetCertificateCount(SecPVCRef pv);
SecCertificateRef SecPVCGetCertificateAtIndex(SecPVCRef pv, CFIndex ix);
bool SecPVCIsCertificateAtIndexSelfSigned(SecPVCRef pvc, CFIndex ix);
bool SecPVCIsAnchored(SecPVCRef pvc);

/* Set the string result as the reason for the sub policy check key
   failing.  The policy check function should continue processing if
   this function returns true. */
bool SecPVCSetResult(SecPVCRef pv, CFStringRef key, CFIndex ix,
	CFTypeRef result);
bool SecPVCSetResultForced(SecPVCRef pvc,
	CFStringRef key, CFIndex ix, CFTypeRef result, bool force);

/* Enable revocation checking if the rest of the policy checks succeed. */
void SecPVCSetCheckRevocation(SecPVCRef pvc);

/* Run static leaf checks on the path in pvc. */
bool SecPVCLeafChecks(SecPVCRef pvc);

/* Run static parent checks on the path in pvc. */
bool SecPVCParentCertificateChecks(SecPVCRef pvc, CFIndex ix);

/* Check whether an intermediate certificates key has been blacklisted. */
bool SecPVCBlackListedKeyChecks(SecPVCRef pvc, CFIndex ix);
 
/* Check whether an intermediate certificates key has been gray listed. */
bool SecPVCGrayListedKeyChecks(SecPVCRef pvc, CFIndex ix);

/* Run dynamic checks on the complete path in pvc.  Return true if the
   operation is complete, returns false if an async backgroup request was
   scheduled.  Upon completion of the async background job
   SecPathBuilderStep() should be called. */
bool SecPVCPathChecks(SecPVCRef pvc);

/* Return 0 if any certs revocation checking failed, the earliest date on
   which one of the used revocation validation tokens (ocsp response or
   crl) expires.  */
CFAbsoluteTime SecPVCGetEarliestNextUpdate(SecPVCRef pvc);

typedef void (*SecPolicyCheckFunction)(SecPVCRef pv, CFStringRef key);

/*
    Used by SecTrust to verify if a particular certificate chain matches
	this policy.  Returns true if the policy accepts the certificate chain.
*/
bool SecPolicyValidate(SecPolicyRef policy, SecPVCRef pvc, CFStringRef key);

CFArrayRef SecPolicyArrayDeserialize(CFArrayRef serializedPolicies);

void SecPolicyServerInitalize(void);

/* True iff certificate could be an extended validation (EV) certificate. */
bool SecPolicySubscriberCertificateCouldBeEV(SecCertificateRef certificate);

void SecEVPolicyToAnchorDigestsInit(void);

__END_DECLS

#endif /* !_SECURITY_SECPOLICYSERVER_H_ */

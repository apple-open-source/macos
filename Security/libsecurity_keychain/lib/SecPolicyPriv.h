/*
 * Copyright (c) 2003-2010 Apple Inc. All Rights Reserved.
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
	@header SecPolicy
	The functions provided in SecPolicy implement a representation of a particular trust policy.
*/

#ifndef _SECURITY_SECPOLICYPRIV_H_
#define _SECURITY_SECPOLICYPRIV_H_

#include <Security/SecPolicy.h>
#include <CoreFoundation/CFArray.h>


#if defined(__cplusplus)
extern "C" {
#endif

/*!
	@function SecPolicyCopy
	@abstract Returns a copy of a policy reference based on certificate type and OID.
	@param certificateType A certificate type.
	@param policyOID The OID of the policy you want to find. This is a required parameter. See oidsalg.h to see a list of policy OIDs.
	@param policy The returned policy reference. This is a required parameter.
	@result A result code.  See "Security Error Codes" (SecBase.h).
	@discussion This function is deprecated in Mac OS X 10.7 and later;
	to obtain a policy reference, use one of the SecPolicyCreate* functions in SecPolicy.h.
*/
OSStatus SecPolicyCopy(CSSM_CERT_TYPE certificateType, const CSSM_OID *policyOID, SecPolicyRef* policy)
	DEPRECATED_IN_MAC_OS_X_VERSION_10_7_AND_LATER;

/*!
	@function SecPolicyCopyAll
	@abstract Returns an array of all known policies based on certificate type.
    @param certificateType A certificate type. This is a optional parameter. Pass CSSM_CERT_UNKNOWN if the certificate type is unknown.
    @param policies The returned array of policies. This is a required parameter.
    @result A result code.  See "Security Error Codes" (SecBase.h).
	@discussion This function is deprecated in Mac OS X 10.7 and later;
	to obtain a policy reference, use one of the SecPolicyCreate* functions in SecPolicy.h. (Note: there is normally
	no reason to iterate over multiple disjointed policies, except to provide a way to edit trust settings for each
	policy, as is done in certain certificate UI views. In that specific case, your code should call SecPolicyCreateWithOID
	for each desired policy from the list of supported OID constants in SecPolicy.h.)
*/
OSStatus SecPolicyCopyAll(CSSM_CERT_TYPE certificateType, CFArrayRef* policies)
	DEPRECATED_IN_MAC_OS_X_VERSION_10_7_AND_LATER;

#if defined(__cplusplus)
}
#endif

#endif /* !_SECURITY_SECPOLICYPRIV_H_ */

/*
 * Copyright (c) 2003 Apple Computer, Inc. All Rights Reserved.
 * 
 * The contents of this file constitute Original Code as defined in and are
 * subject to the Apple Public Source License Version 1.2 (the 'License').
 * You may not use this file except in compliance with the License. Please obtain
 * a copy of the License at http://www.apple.com/publicsource and read it before
 * using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESS
 * OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES, INCLUDING WITHOUT
 * LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. Please see the License for the
 * specific language governing rights and limitations under the License.
 */

/*!
	@header SecPolicy
	The functions provided in SecPolicy implement a representation of a particular trust policy.
*/

#ifndef _SECURITY_SECPOLICYPRIV_H_
#define _SECURITY_SECPOLICYPRIV_H_

#include <Security/SecPolicy.h>


#if defined(__cplusplus)
extern "C" {
#endif


/*!
	@function SecPolicySetValue
	@abstract Sets a policy's value.
    @param policyRef A policy reference.
    @param value The value to be set into the policy object, replacing any previous value.
    @result A result code.  See "Security Error Codes" (SecBase.h).
*/
OSStatus SecPolicySetValue(SecPolicyRef policyRef, const CSSM_DATA *value);


#if defined(__cplusplus)
}
#endif

#endif /* !_SECURITY_SECPOLICYPRIV_H_ */

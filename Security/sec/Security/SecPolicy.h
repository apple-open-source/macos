/*
 * Copyright (c) 2007-2010 Apple Inc. All Rights Reserved.
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
    The functions provided in SecPolicy.h provide an interface to various
	X.509 certificate trust policies.
*/

#ifndef _SECURITY_SECPOLICY_H_
#define _SECURITY_SECPOLICY_H_

#include <Security/SecBase.h>
#include <CoreFoundation/CFBase.h>

#if defined(__cplusplus)
extern "C" {
#endif

/*!
	@typedef SecPolicyRef
	@abstract CFType representing a X.509 certificate trust policy.
*/
typedef struct __SecPolicy *SecPolicyRef;

/*!
    @function SecPolicyGetTypeID
    @abstract Returns the type identifier of SecPolicy instances.
    @result The CFTypeID of SecPolicy instances.
*/
CFTypeID SecPolicyGetTypeID(void)
    __OSX_AVAILABLE_STARTING(__MAC_10_3, __IPHONE_2_0);

/*!
    @function SecPolicyCreateBasicX509
    @abstract Returns a policy object for the default X.509 policy.
    @result A policy object. The caller is responsible for calling CFRelease
	on this when it is no longer needed.
*/
SecPolicyRef SecPolicyCreateBasicX509(void)
    __OSX_AVAILABLE_STARTING(__MAC_10_6, __IPHONE_2_0);

/*!
    @function SecPolicyCreateSSL
    @abstract Returns a policy object for evaluating SSL certificate chains.
	@param server Passing true for this parameter create a policy for SSL
	server certificates.
	@param hostname Optional; if present, the policy will require the specified
	hostname to match the hostname in the leaf certificate.
    @result A policy object. The caller is responsible for calling CFRelease
	on this when it is no longer needed.
*/
SecPolicyRef SecPolicyCreateSSL(Boolean server, CFStringRef hostname)
    __OSX_AVAILABLE_STARTING(__MAC_10_6, __IPHONE_2_0);

#if defined(__cplusplus)
}
#endif

#endif /* !_SECURITY_SECPOLICY_H_ */

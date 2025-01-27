/*
 * Copyright (c) 2023 Apple Inc. All Rights Reserved.
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
	@header SecRequirementPrivLWCR
	SecRequirementPrivLWCR is a private counter-part to SecRequirement. Its contents are not
	official API, and are subject to change without notice.
*/
#ifndef _H_SECREQUIREMENTPRIVLWCR
#define _H_SECREQUIREMENTPRIVLWCR

#include <Security/SecRequirement.h>
#include <Security/SecCertificate.h>


#ifdef __cplusplus
extern "C" {
#endif

/*!
	@function SecRequirementCreateWithLightweightCodeRequirementData
	Create a SecRequirement object based on DER encoded LightweightCodeRequirement Data.
 
	@param lwcr A CFDataRef containing the DER encoded LightweightCodeRequirement Data.
	@param flags Optional flags. (Not used)
	@param result Upon success a SecRequirementRef for the requirement.
	@param errors An optional pointer to a CFErrorRef variable. If the call fails and something
	other than errSecSuccess is returned, then this argument is non-NULL and contains more
	information on the error. The caller must CFRelease() this error object when done.
	@result Upoon success, errSecSucces. Upon error, an OSStatus value documented in
	CSCommon.h or other headers.
*/
OSStatus SecRequirementCreateWithLightweightCodeRequirementData(CFDataRef lwcr, SecCSFlags flags,
																SecRequirementRef *result, CFErrorRef *errors);
#ifdef __cplusplus
}
#endif

#endif //_H_SECREQUIREMENTPRIV

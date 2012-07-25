/*
 * Copyright (c) 2000-2009 Apple Inc. All Rights Reserved.
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
	@header SecBase
	SecBase contains common declarations for the Security functions. 
*/

#ifndef _SECURITY_SECBASEP_H_
#define _SECURITY_SECBASEP_H_

#include <Availability.h>

#if defined(__cplusplus)
extern "C" {
#endif

/*!
    @typedef SecCertificateRef
    @abstract CFType representing a X.509 certificate, see
    SecCertificate.h for details.
*/
typedef struct __SecCertificate *SecCertificateRefP;

/*!
    @typedef SecIdentityRef
    @abstract CFType representing an identity, which contains
    a SecKeyRef and an ascociated SecCertificateRef, see
    SecIdentity.h for details.
*/
typedef struct __SecIdentity *SecIdentityRefP;

/*!
    @typedef SecKeyRef
    @abstract CFType representing an asymetric key, see
    SecKey.h for details.
*/
typedef struct __SecKey *SecKeyRefP;

/***********************************************
 *** OSStatus values unique to Security APIs ***
 ***********************************************/

/*
    Note: the comments that appear after these errors are used to create
    SecErrorMessages.strings. The comments must not be multi-line, and
    should be in a form meaningful to an end user. If a different or
    additional comment is needed, it can be put in the header doc format,
    or on a line that does not start with errZZZ.
*/

#if 0
enum
{
    errSecSuccess                = 0,       /* No error. */
    errSecUnimplemented          = -4,      /* Function or operation not implemented. */
    errSecParam                  = -50,     /* One or more parameters passed to a function where not valid. */
    errSecAllocate               = -108,    /* Failed to allocate memory. */
    errSecNotAvailable           = -25291,	/* No keychain is available. You may need to restart your computer. */
    errSecDuplicateItem          = -25299,	/* The specified item already exists in the keychain. */
    errSecItemNotFound           = -25300,	/* The specified item could not be found in the keychain. */
    errSecInteractionNotAllowed  = -25308,	/* User interaction is not allowed. */
    errSecDecode                 = -26275,  /* Unable to decode the provided data. */
};
#endif

#if defined(__cplusplus)
}
#endif

#endif /* !_SECURITY_SECBASEP_H_ */

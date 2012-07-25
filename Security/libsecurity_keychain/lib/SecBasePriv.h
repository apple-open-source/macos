/*
 * Copyright (c) 2003-2008 Apple Inc. All Rights Reserved.
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
 @header SecBasePriv
 SecBasePriv contains private error codes for the Security framework. 
 */
#ifndef _SECURITY_SECBASEPRIV_H_
#define _SECURITY_SECBASEPRIV_H_

#include <CoreFoundation/CFBase.h>
#include <Security/cssmtype.h>

#if defined(__cplusplus)
extern "C" {
#endif

/*******************************************************
 *** Private OSStatus values unique to Security APIs ***
 *******************************************************/

/*
    Note: the comments that appear after these errors are used to create SecErrorMessages.strings.
    The comments must not be multi-line, and should be in a form meaningful to an end user. If
    a different or additional comment is needed, it can be put in the header doc format, or on a
    line that does not start with errZZZ.
*/

enum
{
	priv_errSecUnimplemented     = -4,		/* Private version of errSecUnimplemented constant. */
	priv_errSecParam             = -50,		/* Private version of errSecParam constant. */
	priv_errSecDecode            = -26275,	/* Private version of errSecDecode constant. */
};

enum
{
    errSecInvalidCertificate     = priv_errSecDecode, // -26265,  /* This certificate could not be decoded. */
    errSecPolicyDenied			 = -26270,  /* The certificate chain was not trusted due to a policy not accepting it. */
    errSecInvalidKey             = priv_errSecDecode, // -26274,  /* The provided key material was not valid. */
    errSecInternal               = -26276,  /* An internal error occured in the Security framework. */
    errSecUnsupportedAlgorithm   = priv_errSecUnimplemented, // -26268,  /* An unsupported algorithm was encountered. */
    errSecUnsupportedOperation   = priv_errSecUnimplemented, // -26271,  /* The operation you requested is not supported by this key. */
    errSecUnsupportedPadding     = priv_errSecParam, // -26273,  /* The padding you requested is not supported. */
    errSecItemInvalidKey         = priv_errSecParam, // -34000,  /* A string key in dictionary is not one of the supported keys. */
    errSecItemInvalidKeyType     = priv_errSecParam, // -34001,  /* A key in a dictionary is neither a CFStringRef nor a CFNumberRef. */
    errSecItemInvalidValue       = priv_errSecParam, // -34002,  /* A value in a dictionary is an invalid (or unsupported) CF type. */
    errSecItemClassMissing       = priv_errSecParam, // -34003,  /* No kSecItemClass key was specified in a dictionary. */
    errSecItemMatchUnsupported   = priv_errSecParam, // -34004,  /* The caller passed one or more kSecMatch keys to a function which does not support matches. */
    errSecUseItemListUnsupported = priv_errSecParam, // -34005,  /* The caller passed in a kSecUseItemList key to a function which does not support it. */
    errSecUseKeychainUnsupported = priv_errSecParam, // -34006,  /* The caller passed in a kSecUseKeychain key to a function which does not support it. */
    errSecUseKeychainListUnsupported = priv_errSecParam, // -34007,  /* The caller passed in a kSecUseKeychainList key to a function which does not support it. */
    errSecReturnDataUnsupported  = priv_errSecParam, // -34008,  /* The caller passed in a kSecReturnData key to a function which does not support it. */
    errSecReturnAttributesUnsupported = priv_errSecParam, // -34009,  /* The caller passed in a kSecReturnAttributes key to a function which does not support it. */
    errSecReturnRefUnsupported   = priv_errSecParam, // -34010,  /* The caller passed in a kSecReturnRef key to a function which does not support it. */
    errSecValueRefUnsupported    = priv_errSecParam, // -34012,  /* The caller passed in a kSecValueRef key to a function which does not support it. */
    errSecValuePersistentRefUnsupported = priv_errSecParam, // -34013,  /* The caller passed in a kSecValuePersistentRef key to a function which does not support it. */
    errSecReturnMissingPointer   = priv_errSecParam, // -34014,  /* The caller passed asked for something to be returned but did not pass in a result pointer. */
	errSecMatchLimitUnsupported  = priv_errSecParam, // -34015,  /* The caller passed in a kSecMatchLimit key to a call which does not support limits. */
	errSecItemIllegalQuery       = priv_errSecParam, // -34016,  /* The caller passed in a query which contained too many keys. */
};

const char *cssmErrorString(CSSM_RETURN error);

OSStatus SecKeychainErrFromOSStatus(OSStatus osStatus);

#if defined(__cplusplus)
}
#endif

#endif /* !_SECURITY_SECBASEPRIV_H_ */

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
	@header SecTrustPriv
	Private part of SecTrust.h
*/

#ifndef _SECURITY_SECTRUST_PRIV_H_
#define _SECURITY_SECTRUST_PRIV_H_

#include <Security/SecTrust.h>
#include <CoreFoundation/CFString.h>
#include <CoreFoundation/CFDictionary.h>


#if defined(__cplusplus)
extern "C" {
#endif

/*
	unique keychain item attributes for user trust records.
*/
enum {
    kSecTrustCertAttr 				 = 'tcrt',
    kSecTrustPolicyAttr				 = 'tpol',
	/* Leopard and later */
	kSecTrustPubKeyAttr				 = 'tpbk',
	kSecTrustSignatureAttr			 = 'tsig'
};

/*!
	@function SecTrustGetUserTrust
	@abstract Gets the user-specified trust settings of a certificate and policy.
	@param certificate A reference to a certificate.
	@param policy A reference to a policy.
	@param trustSetting On return, a pointer to the user specified trust settings.
	@result A result code. See "Security Error Codes" (SecBase.h).
	@availability Mac OS X version 10.4. Deprecated in Mac OS X version 10.5.
*/
OSStatus SecTrustGetUserTrust(SecCertificateRef certificate, SecPolicyRef policy, SecTrustUserSetting *trustSetting)
	/*DEPRECATED_IN_MAC_OS_X_VERSION_10_5_AND_LATER*/;

/*!
	@function SecTrustSetUserTrust
	@abstract Sets the user-specified trust settings of a certificate and policy.
	@param certificate A reference to a certificate.
	@param policy A reference to a policy.
	@param trustSetting The user-specified trust settings.
	@result A result code. See "Security Error Codes" (SecBase.h).
	@availability Mac OS X version 10.4. Deprecated in Mac OS X version 10.5.
	@discussion as of Mac OS version 10.5, this will result in a call to 
	 SecTrustSettingsSetTrustSettings(). 
*/
OSStatus SecTrustSetUserTrust(SecCertificateRef certificate, SecPolicyRef policy, SecTrustUserSetting trustSetting)
	/*DEPRECATED_IN_MAC_OS_X_VERSION_10_5_AND_LATER*/;

/*!
	@function SecTrustSetUserTrustLegacy
	@abstract Sets the user-specified trust settings of a certificate and policy.
	@param certificate A reference to a certificate.
	@param policy A reference to a policy.
	@param trustSetting The user-specified trust settings.
	@result A result code.  See "Security Error Codes" (SecBase.h).

	@This is the private version of what used to be SecTrustSetUserTrust(); it operates
	 on UserTrust entries as that function used to. The current SecTrustSetUserTrust()
	 function operated on Trust Settings. 
*/
OSStatus SecTrustSetUserTrustLegacy(SecCertificateRef certificate, SecPolicyRef policy, SecTrustUserSetting trustSetting);

/*!
	@function SecTrustGetCSSMAnchorCertificates
	@abstract Retrieves the CSSM anchor certificates.
	@param cssmAnchors A pointer to an array of anchor certificates.
	@param cssmAnchorCount A pointer to the number of certificates in anchors.
	@result A result code. See "Security Error Codes" (SecBase.h).
	@availability Mac OS X version 10.4. Deprecated in Mac OS X version 10.5.
*/
OSStatus SecTrustGetCSSMAnchorCertificates(const CSSM_DATA **cssmAnchors, uint32 *cssmAnchorCount)
	/*DEPRECATED_IN_MAC_OS_X_VERSION_10_5_AND_LATER*/;

/*!
	@function SecTrustCopyExtendedResult
	@abstract Gets the extended trust result after an evaluation has been performed.
	@param trust A trust reference.
	@param result On return, result points to a CFDictionaryRef containing extended trust results (if no error occurred).
	The caller is responsible for releasing this dictionary with CFRelease when finished with it.
	@result A result code. See "Security Error Codes" (SecBase.h).
	@discussion This function may only be used after SecTrustEvaluate has been called for the trust reference, otherwise
	errSecTrustNotAvailable is returned. If the certificate is not an extended validation certificate, there is
	no extended result data and errSecDataNotAvailable is returned. Currently, only one dictionary key is defined
	(kSecEVOrganizationName).
*/
OSStatus SecTrustCopyExtendedResult(SecTrustRef trust, CFDictionaryRef *result);


/*
 * Preference-related strings for Revocation policies.
 */
 
/* 
 * Preference domain, i.e., the name of a plist in ~/Library/Preferences or in
 * /Library/Preferences
 */
#define kSecRevocationDomain		"com.apple.security.revocation"

/* OCSP and CRL style keys, followed by values used for both of them */
#define kSecRevocationOcspStyle				CFSTR("OCSPStyle")
#define kSecRevocationCrlStyle				CFSTR("CRLStyle")
  #define kSecRevocationOff					CFSTR("None")	/* default for each one */
  #define kSecRevocationBestAttempt			CFSTR("BestAttempt")
  #define kSecRevocationRequireIfPresent	CFSTR("RequireIfPresent")
  #define kSecRevocationRequireForAll		CFSTR("RequireForAll")
  
/* Which first if both enabled? */
#define kSecRevocationWhichFirst			CFSTR("RevocationFirst")
  #define kSecRevocationOcspFirst			CFSTR("OCSP")
  #define kSecRevocationCrlFirst			CFSTR("CRL")
  
/* boolean: A "this policy is sufficient per cert" for each */
#define kSecRevocationOCSPSufficientPerCert	CFSTR("OCSPSufficientPerCert")
#define kSecRevocationCRLSufficientPerCert	CFSTR("CRLSufficientPerCert")

/* local OCSP responder URI, value arbitrary string value */
#define kSecOCSPLocalResponder				CFSTR("OCSPLocalResponder")

#define kSecEVOrganizationName				CFSTR("Organization")

#if defined(__cplusplus)
}
#endif

#endif /* !_SECURITY_SECTRUST_PRIV_H_ */

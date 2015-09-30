/*
 * Copyright (c) 2007-2011,2013-2015 Apple Inc. All Rights Reserved.
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

/*
   SecCertificateInternal.h
*/

#ifndef _SECURITY_SECCERTIFICATEINTERNAL_H_
#define _SECURITY_SECCERTIFICATEINTERNAL_H_

//#include <Security/SecCertificatePrivP.h>
#include "SecCertificatePrivP.h"
#include "certextensionsP.h"
#include <libDER/DER_Keys.h>

#if defined(__cplusplus)
extern "C" {
#endif

CFDataRef SecCertificateGetAuthorityKeyIDP(SecCertificateRefP certificate);
CFDataRef SecCertificateGetSubjectKeyIDP(SecCertificateRefP certificate);

/* Return an array of CFURLRefs each of which is an crl distribution point for
   this certificate. */
CFArrayRef SecCertificateGetCRLDistributionPointsP(SecCertificateRefP certificate);

/* Return an array of CFURLRefs each of which is an ocspResponder for this
   certificate. */
CFArrayRef SecCertificateGetOCSPRespondersP(SecCertificateRefP certificate);

/* Return an array of CFURLRefs each of which is an caIssuer for this
   certificate. */
CFArrayRef SecCertificateGetCAIssuersP(SecCertificateRefP certificate);

/* Dump certificate for debugging. */
void SecCertificateShowP(SecCertificateRefP certificate);

/* Return the DER encoded issuer sequence for the receiving certificates issuer. */
CFDataRef SecCertificateCopyIssuerSequenceP(SecCertificateRefP certificate);

/* Return the DER encoded subject sequence for the receiving certificates subject. */
CFDataRef SecCertificateCopySubjectSequenceP(SecCertificateRefP certificate);

/* Return the content of a DER encoded X.501 name (without the tag and length
   fields) for the receiving certificates issuer. */
CFDataRef SecCertificateGetNormalizedIssuerContentP(SecCertificateRefP certificate);

/* Return the content of a DER encoded X.501 name (without the tag and length
   fields) for the receiving certificates subject. */
CFDataRef SecCertificateGetNormalizedSubjectContentP(SecCertificateRefP certificate);

CFDataRef SecDERItemCopySequenceP(DERItem *content);

/* Return true iff the certificate has a subject. */
bool SecCertificateHasSubjectP(SecCertificateRefP certificate);

/* Return true iff the certificate has a critical subject alt name. */
bool SecCertificateHasCriticalSubjectAltNameP(SecCertificateRefP certificate);

/* Return true if certificate contains one or more critical extensions we
   are unable to parse. */
bool SecCertificateHasUnknownCriticalExtensionP(SecCertificateRefP certificate);

/* Return true iff certificate is valid as of verifyTime. */
bool SecCertificateIsValidP(SecCertificateRefP certificate,
	CFAbsoluteTime verifyTime);

/* Return an attribute dictionary used to store this item in a keychain. */
CFDictionaryRef SecCertificateCopyAttributeDictionaryP(
	SecCertificateRefP certificate);

/* Return a certificate from the attribute dictionary that was used to store
   this item in a keychain. */
SecCertificateRefP SecCertificateCreateFromAttributeDictionaryP(
	CFDictionaryRef refAttributes);

/* Return a SecKeyRef for the public key embedded in the cert. */
SecKeyRefP SecCertificateCopyPublicKeyP(SecCertificateRefP certificate);

/* Return the SecCEBasicConstraints extension for this certificate if it
   has one. */
const SecCEBasicConstraints *
SecCertificateGetBasicConstraintsP(SecCertificateRefP certificate);

/* Return the SecCEPolicyConstraints extension for this certificate if it
   has one. */
const SecCEPolicyConstraints *
SecCertificateGetPolicyConstraintsP(SecCertificateRefP certificate);

/* Return a dictionary from CFDataRef to CFArrayRef of CFDataRef
   representing the policyMapping extension of this certificate. */
CFDictionaryRef
SecCertificateGetPolicyMappingsP(SecCertificateRefP certificate);

/* Return the SecCECertificatePolicies extension for this certificate if it
   has one. */
const SecCECertificatePolicies *
SecCertificateGetCertificatePoliciesP(SecCertificateRefP certificate);

/* Returns UINT32_MAX if InhibitAnyPolicy extension is not present or invalid,
   returns the value of the SkipCerts field of the InhibitAnyPolicy extension
   otherwise. */
uint32_t
SecCertificateGetInhibitAnyPolicySkipCertsP(SecCertificateRefP certificate);

/* Return the public key algorithm and parameters for certificate.  */
const DERAlgorithmId *SecCertificateGetPublicKeyAlgorithmP(
	SecCertificateRefP certificate);

/* Return the raw public key data for certificate.  */
const DERItem *SecCertificateGetPublicKeyDataP(SecCertificateRefP certificate);

#pragma mark -
#pragma mark Certificate Operations

OSStatus SecCertificateIsSignedByP(SecCertificateRefP certificate,
    SecKeyRefP issuerKey);

#pragma mark -
#pragma mark Certificate Creation

#ifdef OPTIONAL_METHODS
/* Return a certificate for the PEM representation of this certificate.
   Return NULL the passed in der_certificate is not a valid DER encoded X.509
   certificate, and return a CFError by reference.  It is the
   responsibility of the caller to release the CFError. */
SecCertificateRefP SecCertificateCreateWithPEMP(CFAllocatorRef allocator,
	CFStringRef pem_certificate);

/* Return a CFStringRef containing the the pem representation of this
   certificate. */
CFStringRef SecCertificateGetPEMP(SecCertificateRefP der_certificate);

#endif /* OPTIONAL_METHODS */

#if 0
/* Complete the certificate chain of this certificate, setting the parent
   certificate for each certificate along they way.  Return 0 if the
   system is able to find all the certificates to complete the certificate
   chain either in the passed in other_certificates array or in the user or
   the systems keychain(s).
   If the certificate's issuer chain can not be completed, this function
   will return an error status code.
   NOTE: This function does not verify whether the certificate is trusted it's
   main use is just to ensure that anyone using this certificate upstream will
   have access to a complete (or as complete as possible in the case of
   something going wrong) certificate chain.  */
OSStatus SecCertificateCompleteChainP(SecCertificateRefP certificate,
	CFArrayRef other_certificates);
#endif

#if 0

/*!
	@function SecCertificateGetVersionNumberP
	@abstract Retrieves the version of a given certificate as a CFNumberRef.
    @param certificate A reference to the certificate from which to obtain the certificate version.
	@result A CFNumberRef representing the certificate version.  The following values are currently known to be returned, but more may be added in the future:
        1: X509v1
        2: X509v2
        3: X509v3
*/
CFNumberRef SecCertificateGetVersionNumberP(SecCertificateRefP certificate);

/*!
	@function SecCertificateGetSerialDERP
	@abstract Retrieves the serial number of a given certificate in DER encoding.
    @param certificate A reference to the certificate from which to obtain the serial number.
	@result A CFDataRef containing the DER encoded serial number of the certificate, minus the tag and length fields.
*/
CFDataRef SecCertificateGetSerialDERP(SecCertificateRefP certificate);


/*!
	@function SecCertificateGetSerialStringP
	@abstract Retrieves the serial number of a given certificate in human readable form.
    @param certificate A reference to the certificate from which to obtain the serial number.
	@result A CFStringRef containing the human readable serial number of the certificate in decimal form.
*/
CFStringRef SecCertificateGetSerialStringP(SecCertificateRefP certificate);



CFDataRef SecCertificateGetPublicKeyDERP(SecCertificateRefP certificate);
CFDataRef SecCertificateGetPublicKeySHA1FingerPrintP(SecCertificateRefP certificate);
CFDataRef SecCertificateGetPublicKeyMD5FingerPrintP(SecCertificateRefP certificate);
CFDataRef SecCertificateGetSignatureAlgorithmDERP(SecCertificateRefP certificate);
CFDataRef SecCertificateGetSignatureAlgorithmNameP(SecCertificateRefP certificate);
CFStringRef SecCertificateGetSignatureAlgorithmOIDP(SecCertificateRefP certificate);
CFDataRef SecCertificateGetSignatureDERP(SecCertificateRefP certificate);
CFDataRef SecCertificateGetSignatureAlgorithmParametersDERP(SecCertificateRefP certificate);

/* plist top level array is orderd list of key/value pairs */
CFArrayRef SecCertificateGetSignatureAlgorithmParametersArrayP(SecCertificateRefP certificate);

#if 0
/* This cert is signed by its parent? */
bool SecCertificateIsSignatureValidP(SecCertificateRefP certificate);

/* This cert is signed by its parent and so on until no parent certificate can be found? */
bool SecCertificateIsIssuerChainValidP(SecCertificateRefP certificate, CFArrayRef additionalCertificatesToSearch);

/* This cert is signed by its parent and so on until no parent certificate can be found? */
bool SecCertificateIsSignatureChainValidP(SecCertificateRefP certificate);

/* This cert is signed by its parent and so on until a certiicate in anchors can be found. */
bool SecCertificateIssuerChainHasAnchorInP(SecCertificateRefP certificate, CFArrayRef anchors);

/* This cert is signed by its parent and so on until a certiicate in anchors can be found. */
bool SecCertificateSignatureChainHasAnchorInP(SecCertificateRefP certificate, CFArrayRef anchors);

bool SecCertificateIsSelfSignedP(SecCertificateRefP certificate);
#endif


/* The entire certificate in DER encoding including the outer tag and length fields. */
CFDataRef SecCertificateGetDERP(SecCertificateRefP certificate);

/* Returns the status code of the last failed call for this certificate on this thread. */
OSStatus SecCertificateGetStatusP(SecCertificateRefP certificate);

CFDataRef SecCertificateGetIssuerDERP(SecCertificateRefP certificate);
CFDataRef SecCertificateGetNormalizedIssuerDERP(SecCertificateRefP certificate);

/* Return the issuer as an X509 name encoded in an array.  Each element in this array is an array.  Each inner array has en even number of elements.  Each pair of elements in the inner array represents a key and a value.  The key is a string and the value is also a string.  Elements in the outer array should be considered ordered while pairs in the inner array should not. */
CFArrayRef SecCertificateGetIssuerArrayP(SecCertificateRefP certificate);


CFDataRef SecCertificateGetSubjectDERP(SecCertificateRefP certificate);
CFDataRef SecCertificateGetNormalizedSubjectDERP(SecCertificateRefP certificate);
/* See SecCertificateGetIssuerArray for a description of the returned array. */
CFArrayRef SecCertificateGetSubjectArrayP(SecCertificateRefP certificate);

CFDateRef SecCertificateGetNotValidBeforeDateP(SecCertificateRefP certificate);
CFDateRef SecCertificateGetNotValidDateP(SecCertificateRefP certificate);


#if 0

CFIndex SecCertificateGetExtensionCountP(SecCertificateRefP certificate,  index);
CFDataRef SecCertificateGetExtensionAtIndexDERP(SecCertificateRefP certificate, CFIndex index);
bool SecCertificateIsExtensionAtIndexCriticalP(SecCertificateRefP certificate, CFIndex index);

/* array see email example. */
CFArrayRef SecCertificateGetExtensionAtIndexParamsArrayP(SecCertificateRefP certificate, CFIndex index);

CFStringRef SecCertificateGetExtensionAtIndexNameP(SecCertificateRefP certificate, CFIndex index);
CFStringRef SecCertificateGetExtensionAtIndexOIDP(SecCertificateRefP certificate, CFIndex index);

#else

/* Return an array with all of this certificates SecCertificateExtensionRefs. */
CFArrayRef SecCertificateGetExtensionsP(SecCertificateRefP certificate);

/* Return the SecCertificateExtensionRef for the extension with the given oid.  Return NULL if it does not exist or if an error occours call SecCertificateGetStatus() to see if an error occured or not. */
SecCertificateExtensionRef SecCertificateGetExtensionWithOIDP(SecCertificateRefP certificate, CFDataRef oid);

CFDataRef SecCertificateExtensionGetDERP(SecCertificateExtensionRef extension, CFDataRef oid);
CFStringRef SecCertificateExtensionNameP(SecCertificateExtensionRef extension);
CFDataRef SecCertificateExtensionGetOIDDERP(SecCertificateExtensionRef extension, CFDataRef oid);
CFStringRef SecCertificateExtensionGetOIDStringP(SecCertificateExtensionRef extension, CFDataRef oid);
bool SecCertificateExtensionIsCriticalP(SecCertificateExtensionRef extension);
CFArrayRef SecCertificateExtensionGetContentDERP(SecCertificateExtensionRef extension);

/* Return the content of extension as an array.  The array has en even number of elements.  Each pair of elements in the array represents a key and a value.  The key is a string and the value is either a string, or dictionary or an array of key value pairs like the outer array.  */
CFArrayRef SecCertificateExtensionGetContentArrayP(SecCertificateExtensionRef extension);

#endif /* 0 */

#endif /* 0 */


void appendPropertyP(CFMutableArrayRef properties,
    CFStringRef propertyType, CFStringRef label, CFTypeRef value);

/* Utility functions. */
CFStringRef SecDERItemCopyOIDDecimalRepresentation(CFAllocatorRef allocator,
    const DERItem *oid);
CFDataRef createNormalizedX501Name(CFAllocatorRef allocator,
	const DERItem *x501name);

/* Decode a choice of UTCTime or GeneralizedTime to a CFAbsoluteTime. Return
   an absoluteTime if the date was valid and properly decoded.  Return
   NULL_TIME otherwise. */
CFAbsoluteTime SecAbsoluteTimeFromDateContent(DERTag tag, const uint8_t *bytes,
    size_t length);

#if defined(__cplusplus)
}
#endif

#endif /* !_SECURITY_SECCERTIFICATEINTERNAL_H_ */

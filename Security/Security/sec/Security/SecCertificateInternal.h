/*
 * Copyright (c) 2007-2014 Apple Inc. All Rights Reserved.
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
   SecCertificateInternal
*/

#ifndef _SECURITY_SECCERTIFICATEINTERNAL_H_
#define _SECURITY_SECCERTIFICATEINTERNAL_H_

#include <Security/SecCertificatePriv.h>
#include <Security/certextensions.h>
#include <libDER/DER_Keys.h>

__BEGIN_DECLS

CFDataRef SecCertificateGetAuthorityKeyID(SecCertificateRef certificate);
CFDataRef SecCertificateGetSubjectKeyID(SecCertificateRef certificate);

/* Return an array of CFURLRefs each of which is an crl distribution point for
   this certificate. */
CFArrayRef SecCertificateGetCRLDistributionPoints(SecCertificateRef certificate);

/* Return an array of CFURLRefs each of which is an ocspResponder for this
   certificate. */
CFArrayRef SecCertificateGetOCSPResponders(SecCertificateRef certificate);

/* Return an array of CFURLRefs each of which is an caIssuer for this
   certificate. */
CFArrayRef SecCertificateGetCAIssuers(SecCertificateRef certificate);

/* Dump certificate for debugging. */
void SecCertificateShow(SecCertificateRef certificate);

/* Return the DER encoded issuer sequence for the receiving certificates issuer. */
CFDataRef SecCertificateCopyIssuerSequence(SecCertificateRef certificate);

/* Return the DER encoded subject sequence for the receiving certificates subject. */
CFDataRef SecCertificateCopySubjectSequence(SecCertificateRef certificate);

/* Return the content of a DER encoded X.501 name (without the tag and length
   fields) for the receiving certificates issuer. */
CFDataRef SecCertificateGetNormalizedIssuerContent(SecCertificateRef certificate);

/* Return the content of a DER encoded X.501 name (without the tag and length
   fields) for the receiving certificates subject. */
CFDataRef SecCertificateGetNormalizedSubjectContent(SecCertificateRef certificate);

/* Return the normalized name or NULL if it fails to parse */
CFDataRef SecDistinguishedNameCopyNormalizedContent(CFDataRef distinguished_name);

/* Return true iff the certificate has a subject. */
bool SecCertificateHasSubject(SecCertificateRef certificate);
/* Return true iff the certificate has a critical subject alt name. */
bool SecCertificateHasCriticalSubjectAltName(SecCertificateRef certificate);

/* Return true if certificate contains one or more critical extensions we
   are unable to parse. */
bool SecCertificateHasUnknownCriticalExtension(SecCertificateRef certificate);

/* Return true iff certificate is valid as of verifyTime. */
bool SecCertificateIsValid(SecCertificateRef certificate,
	CFAbsoluteTime verifyTime);

/* Return an attribute dictionary used to store this item in a keychain. */
CFDictionaryRef SecCertificateCopyAttributeDictionary(
	SecCertificateRef certificate);

/* Return a certificate from the attribute dictionary that was used to store
   this item in a keychain. */
SecCertificateRef SecCertificateCreateFromAttributeDictionary(
	CFDictionaryRef refAttributes);

/* Return a SecKeyRef for the public key embedded in the cert. */
SecKeyRef SecCertificateCopyPublicKey(SecCertificateRef certificate);

/* Return the SecCEBasicConstraints extension for this certificate if it
   has one. */
const SecCEBasicConstraints *
SecCertificateGetBasicConstraints(SecCertificateRef certificate);

/* Return the SecCEPolicyConstraints extension for this certificate if it
   has one. */
const SecCEPolicyConstraints *
SecCertificateGetPolicyConstraints(SecCertificateRef certificate);

/* Return a dictionary from CFDataRef to CFArrayRef of CFDataRef
   representing the policyMapping extension of this certificate. */
CFDictionaryRef
SecCertificateGetPolicyMappings(SecCertificateRef certificate);

/* Return the SecCECertificatePolicies extension for this certificate if it
   has one. */
const SecCECertificatePolicies *
SecCertificateGetCertificatePolicies(SecCertificateRef certificate);

/* Returns UINT32_MAX if InhibitAnyPolicy extension is not present or invalid,
   returns the value of the SkipCerts field of the InhibitAnyPolicy extension
   otherwise. */
uint32_t
SecCertificateGetInhibitAnyPolicySkipCerts(SecCertificateRef certificate);

/* Return the public key algorithm and parameters for certificate.  */
const DERAlgorithmId *SecCertificateGetPublicKeyAlgorithm(
	SecCertificateRef certificate);

/* Return the raw public key data for certificate.  */
const DERItem *SecCertificateGetPublicKeyData(SecCertificateRef certificate);

// MARK: -
// MARK: Certificate Operations

OSStatus SecCertificateIsSignedBy(SecCertificateRef certificate,
    SecKeyRef issuerKey);

// MARK: -
// MARK: Certificate Creation

#ifdef OPTIONAL_METHODS
/* Return a certificate for the PEM representation of this certificate.
   Return NULL the passed in der_certificate is not a valid DER encoded X.509
   certificate, and return a CFError by reference.  It is the
   responsibility of the caller to release the CFError. */
SecCertificateRef SecCertificateCreateWithPEM(CFAllocatorRef allocator,
	CFStringRef pem_certificate);

/* Return a CFStringRef containing the the pem representation of this
   certificate. */
CFStringRef SecCertificateGetPEM(SecCertificateRef der_certificate);

#endif /* OPTIONAL_METHODS */

#if 0
/* Complete the certificate chain of this certificate, setting the parent
   certificate for each certificate along they way.  Return 0 if the
   system is able to find all the certificates to complete the certificate
   chain either in the passed in other_certificates array or in the user or
   the systems keychain(s).
   If the certifcates issuer chain can not be completed, this function
   will return an error status code.
   NOTE: This function does not verify whether the certificate is trusted it's
   main use is just to ensure that anyone using this certificate upstream will
   have access to a complete (or as complete as possible in the case of
   something going wrong) certificate chain.  */
OSStatus SecCertificateCompleteChain(SecCertificateRef certificate,
	CFArrayRef other_certificates);
#endif

#if 0

/*!
	@function SecCertificateGetVersionNumber
	@abstract Retrieves the version of a given certificate as a CFNumberRef.
    @param certificate A reference to the certificate from which to obtain the certificate version.
	@result A CFNumberRef representing the certificate version.  The following values are currently known to be returned, but more may be added in the future:
        1: X509v1
        2: X509v2
        3: X509v3
*/
CFNumberRef SecCertificateGetVersionNumber(SecCertificateRef certificate);

/*!
	@function SecCertificateGetSerialDER
	@abstract Retrieves the serial number of a given certificate in DER encoding.
    @param certificate A reference to the certificate from which to obtain the serial number.
	@result A CFDataRef containing the DER encoded serial number of the certificate, minus the tag and length fields.
*/
CFDataRef SecCertificateGetSerialDER(SecCertificateRef certificate);


/*!
	@function SecCertificateGetSerialString
	@abstract Retrieves the serial number of a given certificate in human readable form.
    @param certificate A reference to the certificate from which to obtain the serial number.
	@result A CFStringRef containing the human readable serial number of the certificate in decimal form.
*/
CFStringRef SecCertificateGetSerialString(SecCertificateRef certificate);



CFDataRef SecCertificateGetPublicKeyDER(SecCertificateRef certificate);
CFDataRef SecCertificateGetPublicKeySHA1FingerPrint(SecCertificateRef certificate);
CFDataRef SecCertificateGetPublicKeyMD5FingerPrint(SecCertificateRef certificate);
CFDataRef SecCertificateGetSignatureAlgorithmDER(SecCertificateRef certificate);
CFDataRef SecCertificateGetSignatureAlgorithmName(SecCertificateRef certificate);
CFStringRef SecCertificateGetSignatureAlgorithmOID(SecCertificateRef certificate);
CFDataRef SecCertificateGetSignatureDER(SecCertificateRef certificate);
CFDataRef SecCertificateGetSignatureAlgorithmParametersDER(SecCertificateRef certificate);

/* plist top level array is orderd list of key/value pairs */
CFArrayRef SecCertificateGetSignatureAlgorithmParametersArray(SecCertificateRef certificate);

#if 0
/* This cert is signed by it's parent? */
bool SecCertificateIsSignatureValid(SecCertificateRef certificate);

/* This cert is signed by it's parent and so on until no parent certificate can be found? */
bool SecCertificateIsIssuerChainValid(SecCertificateRef certificate, CFArrayRef additionalCertificatesToSearch);

/* This cert is signed by it's parent and so on until no parent certificate can be found? */
bool SecCertificateIsSignatureChainValid(SecCertificateRef certificate);

/* This cert is signed by it's parent and so on until a certiicate in anchors can be found. */
bool SecCertificateIssuerChainHasAnchorIn(SecCertificateRef certificate, CFArrayRef anchors);

/* This cert is signed by it's parent and so on until a certiicate in anchors can be found. */
bool SecCertificateSignatureChainHasAnchorIn(SecCertificateRef certificate, CFArrayRef anchors);

bool SecCertificateIsSelfSigned(SecCertificateRef certificate);
#endif


/* The entire certificate in DER encoding including the outer tag and length fields. */
CFDataRef SecCertificateGetDER(SecCertificateRef certificate);

/* Returns the status code of the last failed call for this certificate on this thread. */
OSStatus SecCertificateGetStatus(SecCertificateRef certificate);

CFDataRef SecCertificateGetIssuerDER(SecCertificateRef certificate);
CFDataRef SecCertificateGetNormalizedIssuerDER(SecCertificateRef certificate);

/* Return the issuer as an X509 name encoded in an array.  Each element in this array is an array.  Each inner array has en even number of elements.  Each pair of elements in the inner array represents a key and a value.  The key is a string and the value is also a string.  Elements in the outer array should be considered ordered while pairs in the inner array should not. */
CFArrayRef SecCertificateGetIssuerArray(SecCertificateRef certificate);


CFDataRef SecCertificateGetSubjectDER(SecCertificateRef certificate);
CFDataRef SecCertificateGetNormalizedSubjectDER(SecCertificateRef certificate);
/* See SecCertificateGetIssuerArray for a description of the returned array. */
CFArrayRef SecCertificateGetSubjectArray(SecCertificateRef certificate);

CFDateRef SecCertificateGetNotValidBeforeDate(SecCertificateRef certificate);
CFDateRef SecCertificateGetNotValidDateDate(SecCertificateRef certificate);


#if 0

CFIndex SecCertificateGetExtensionCount(SecCertificateRef certificate,  index);
CFDataRef SecCertificateGetExtensionAtIndexDER(SecCertificateRef certificate, CFIndex index);
bool SecCertificateIsExtensionAtIndexCritical(SecCertificateRef certificate, CFIndex index);

/* array see email example. */
CFArrayRef SecCertificateGetExtensionAtIndexParamsArray(SecCertificateRef certificate, CFIndex index);

CFStringRef SecCertificateGetExtensionAtIndexName(SecCertificateRef certificate, CFIndex index);
CFStringRef SecCertificateGetExtensionAtIndexOID(SecCertificateRef certificate, CFIndex index);

#else

/* Return an array with all of this certificates SecCertificateExtensionRefs. */
CFArrayRef SecCertificateGetExtensions(SecCertificateRef certificate);

/* Return the SecCertificateExtensionRef for the extension with the given oid.  Return NULL if it does not exist or if an error occours call SecCertificateGetStatus() to see if an error occured or not. */
SecCertificateExtensionRef SecCertificateGetExtensionWithOID(SecCertificateRef certificate, CFDataRef oid);

CFDataRef SecCertificateExtensionGetDER(SecCertificateExtensionRef extension, CFDataRef oid);
CFStringRef SecCertificateExtensionName(SecCertificateExtensionRef extension);
CFDataRef SecCertificateExtensionGetOIDDER(SecCertificateExtensionRef extension, CFDataRef oid);
CFStringRef SecCertificateExtensionGetOIDString(SecCertificateExtensionRef extension, CFDataRef oid);
bool SecCertificateExtensionIsCritical(SecCertificateExtensionRef extension);
CFArrayRef SecCertificateExtensionGetContentDER(SecCertificateExtensionRef extension);

/* Return the content of extension as an array.  The array has en even number of elements.  Each pair of elements in the array represents a key and a value.  The key is a string and the value is either a string, or dictionary or an array of key value pairs like the outer array.  */
CFArrayRef SecCertificateExtensionGetContentArray(SecCertificateExtensionRef extension);

#endif /* 0 */

#endif /* 0 */


void appendProperty(CFMutableArrayRef properties, CFStringRef propertyType,
    CFStringRef label, CFStringRef localizedLabel, CFTypeRef value);

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

bool SecCertificateHasMarkerExtension(SecCertificateRef certificate, CFTypeRef oid);

__END_DECLS

#endif /* !_SECURITY_SECCERTIFICATEINTERNAL_H_ */

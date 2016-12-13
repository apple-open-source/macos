/*
 * Copyright (c) 2006-2015 Apple Inc. All Rights Reserved.
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
	@header SecCertificatePriv
	The functions provided in SecCertificatePriv.h implement and manage a particular
	type of keychain item that represents a certificate.  You can store a
	certificate in a keychain, but a certificate can also be a transient
	object.

	You can use a certificate as a keychain item in most functions.
	Certificates are able to compute their parent certificates, and much more.
*/

#ifndef _SECURITY_SECCERTIFICATEPRIV_H_
#define _SECURITY_SECCERTIFICATEPRIV_H_

#include <Security/SecCertificate.h>
#include <CoreFoundation/CFArray.h>
#include <CoreFoundation/CFData.h>
#include <CoreFoundation/CFDate.h>
#include <CoreFoundation/CFDictionary.h>
#include <CoreFoundation/CFError.h>
#include <stdbool.h>
#include <xpc/xpc.h>

__BEGIN_DECLS

typedef CF_OPTIONS(uint32_t, SecKeyUsage) {
    kSecKeyUsageUnspecified      = 0,
    kSecKeyUsageDigitalSignature = 1 << 0,
    kSecKeyUsageNonRepudiation   = 1 << 1,
    kSecKeyUsageContentCommitment= 1 << 1,
    kSecKeyUsageKeyEncipherment  = 1 << 2,
    kSecKeyUsageDataEncipherment = 1 << 3,
    kSecKeyUsageKeyAgreement     = 1 << 4,
    kSecKeyUsageKeyCertSign      = 1 << 5,
    kSecKeyUsageCRLSign          = 1 << 6,
    kSecKeyUsageEncipherOnly     = 1 << 7,
    kSecKeyUsageDecipherOnly     = 1 << 8,
    kSecKeyUsageCritical         = 1 << 31,
    kSecKeyUsageAll              = 0x7FFFFFFF
};

typedef CF_ENUM(uint32_t, SecCertificateEscrowRootType) {
    kSecCertificateBaselineEscrowRoot = 0,
    kSecCertificateProductionEscrowRoot = 1,
    kSecCertificateBaselinePCSEscrowRoot = 2,
    kSecCertificateProductionPCSEscrowRoot = 3,
    kSecCertificateBaselineEscrowBackupRoot = 4,        // v100 and v101
    kSecCertificateProductionEscrowBackupRoot = 5,
    kSecCertificateBaselineEscrowEnrollmentRoot = 6,    // v101 only
    kSecCertificateProductionEscrowEnrollmentRoot = 7,
};

/* The names of the files that contain the escrow certificates */
extern const CFStringRef kSecCertificateProductionEscrowKey;
extern const CFStringRef kSecCertificateProductionPCSEscrowKey;
extern const CFStringRef kSecCertificateEscrowFileName;


/* Return a certificate for the DER representation of this certificate.
   Return NULL if the passed-in data is not a valid DER-encoded X.509
   certificate. */
SecCertificateRef SecCertificateCreateWithBytes(CFAllocatorRef allocator,
	const UInt8 *bytes, CFIndex length);

/* Return the length of the DER representation of this certificate. */
CFIndex SecCertificateGetLength(SecCertificateRef certificate);

/* Return the bytes of the DER representation of this certificate. */
const UInt8 *SecCertificateGetBytePtr(SecCertificateRef certificate);

// MARK: -
// MARK: Certificate Accessors

CFDataRef SecCertificateGetSHA1Digest(SecCertificateRef certificate);

CFDataRef SecCertificateCopyIssuerSHA1Digest(SecCertificateRef certificate);

CFDataRef SecCertificateCopyPublicKeySHA1Digest(SecCertificateRef certificate);

CFDataRef SecCertificateCopySubjectPublicKeyInfoSHA1Digest(SecCertificateRef certificate);

CFDataRef SecCertificateCopySubjectPublicKeyInfoSHA256Digest(SecCertificateRef certificate);

CFDataRef SecCertificateCopySHA256Digest(SecCertificateRef certificate);

SecKeyRef SecCertificateCopyPublicKey(SecCertificateRef certificate);

SecCertificateRef SecCertificateCreateWithKeychainItem(CFAllocatorRef allocator,
	CFDataRef der_certificate, CFTypeRef keychainItem);

OSStatus SecCertificateSetKeychainItem(SecCertificateRef certificate,
	CFTypeRef keychain_item);

CFTypeRef SecCertificateCopyKeychainItem(SecCertificateRef certificate);

/*!
	@function SecCertificateCopyIssuerSummary
	@abstract Return a simple string which hopefully represents a human understandable issuer.
    @param certificate SecCertificate object created with SecCertificateCreateWithData().
    @discussion All the data in this string comes from the certificate itself
    and thus it's in whatever language the certificate itself is in.
	@result A CFStringRef which the caller should CFRelease() once it's no longer needed.
*/
CFStringRef SecCertificateCopyIssuerSummary(SecCertificateRef certificate);

/*!
	@function SecCertificateCopyProperties
	@abstract Return a property array for this trust certificate.
	@param certificate A reference to the certificate to evaluate.
    @result A property array. It is the caller's responsability to CFRelease
    the returned array when it is no longer needed.
    See SecTrustCopySummaryPropertiesAtIndex on how to intepret this array.
	Unlike that function call this function returns a detailed description
    of the certificate in question.
*/
CFArrayRef SecCertificateCopyProperties(SecCertificateRef certificate);

CFMutableArrayRef SecCertificateCopySummaryProperties(
    SecCertificateRef certificate, CFAbsoluteTime verifyTime);

/* Return the content of a DER-encoded integer (without the tag and length
   fields) for this certificate's serial number.   The caller must CFRelease
   the value returned.  */
#if (TARGET_OS_MAC && !(TARGET_OS_EMBEDDED || TARGET_OS_IPHONE))
CFDataRef SecCertificateCopySerialNumber(SecCertificateRef certificate, CFErrorRef *error);
#else
CFDataRef SecCertificateCopySerialNumber(SecCertificateRef certificate);
#endif

/* Return the content of a DER encoded X.501 name (without the tag and length
 fields) for the receiving certificates issuer. */
CFDataRef SecCertificateGetNormalizedIssuerContent(SecCertificateRef certificate);

/* Return the content of a DER encoded X.501 name (without the tag and length
 fields) for the receiving certificates subject. */
CFDataRef SecCertificateGetNormalizedSubjectContent(SecCertificateRef certificate);

/* Return an array of CFStringRefs representing the ip addresses in the
   certificate if any. */
CFArrayRef SecCertificateCopyIPAddresses(SecCertificateRef certificate);

/* Return an array of CFStringRefs representing the dns addresses in the
   certificate if any. */
CFArrayRef SecCertificateCopyDNSNames(SecCertificateRef certificate);

/* Return an array of CFStringRefs representing the email addresses in the
   certificate if any. */
CFArrayRef SecCertificateCopyRFC822Names(SecCertificateRef certificate);

/* Return an array of CFStringRefs representing the common names in the
   certificates subject if any. */
CFArrayRef SecCertificateCopyCommonNames(SecCertificateRef certificate);

/* Return an array of CFStringRefs representing the organization in the
   certificate's subject if any. */
CFArrayRef SecCertificateCopyOrganization(SecCertificateRef certificate);

/* Return an array of CFStringRefs representing the organizational unit in the
   certificate's subject if any. */
CFArrayRef SecCertificateCopyOrganizationalUnit(SecCertificateRef certificate);

/* Return an array of CFStringRefs representing the NTPrincipalNames in the
   certificate if any. */
CFArrayRef SecCertificateCopyNTPrincipalNames(SecCertificateRef certificate);

/* Return a string formatted according to RFC 2253 representing the complete
   subject of certificate. */
CFStringRef SecCertificateCopySubjectString(SecCertificateRef certificate);

/* Return a string with the company name of an ev leaf certificate. */
CFStringRef SecCertificateCopyCompanyName(SecCertificateRef certificate);

/* X.509 Certificate Version: 1, 2 or 3. */
CFIndex SecCertificateVersion(SecCertificateRef certificate);
CFAbsoluteTime SecCertificateNotValidBefore(SecCertificateRef certificate);
CFAbsoluteTime SecCertificateNotValidAfter(SecCertificateRef certificate);

/* Return true in isSelfSigned output parameter if certificate is self-signed.
   Function result is a non-zero status if the answer cannot be determined
   (e.g. certRef is invalid), otherwise errSecSuccess. */
OSStatus SecCertificateIsSelfSigned(SecCertificateRef certRef, Boolean *isSelfSigned);

/* Return true iff certificate is self signed and has a basic constraints
   extension indicating that it's a certificate authority. */
bool SecCertificateIsSelfSignedCA(SecCertificateRef certificate);

/* Return true if certificate has a basic constraints extension
   indicating that it's a certificate authority. */
bool SecCertificateIsCA(SecCertificateRef certificate);

SecKeyUsage SecCertificateGetKeyUsage(SecCertificateRef certificate);

/* Returns an array of CFDataRefs for all extended key usage oids or NULL */
CFArrayRef SecCertificateCopyExtendedKeyUsage(SecCertificateRef certificate);

/* Returns an array of CFDataRefs for all embedded SCTs */
CFArrayRef SecCertificateCopySignedCertificateTimestamps(SecCertificateRef certificate);

/* Returns a certificate from a pem blob */
SecCertificateRef SecCertificateCreateWithPEM(CFAllocatorRef allocator,
	CFDataRef pem_certificate);

/* Append certificate to xpc_certificates. */
bool SecCertificateAppendToXPCArray(SecCertificateRef certificate, xpc_object_t xpc_certificates, CFErrorRef *error);

/* Decode certificate from xpc_certificates[index] as encoded by SecCertificateAppendToXPCArray(). */
SecCertificateRef SecCertificateCreateWithXPCArrayAtIndex(xpc_object_t xpc_certificates, size_t index, CFErrorRef *error);

/* Retrieve the array of valid Escrow certificates for a given root type */
CFArrayRef SecCertificateCopyEscrowRoots(SecCertificateEscrowRootType escrowRootType);

/* Return an xpc_array of data from an array of SecCertificateRefs. */
xpc_object_t SecCertificateArrayCopyXPCArray(CFArrayRef certificates, CFErrorRef *error);

/* Return an array of SecCertificateRefs from a xpc_object array of datas. */
CFArrayRef SecCertificateXPCArrayCopyArray(xpc_object_t xpc_certificates, CFErrorRef *error);

/* Return the precert TBSCertificate DER data - used for Certificate Transparency */
CFDataRef SecCertificateCopyPrecertTBS(SecCertificateRef certificate);

/* Return an attribute dictionary used to store this item in a keychain. */
CFDictionaryRef SecCertificateCopyAttributeDictionary(SecCertificateRef certificate);

/*
 * Enumerated constants for signature hash algorithms.
 */
typedef CF_ENUM(uint32_t, SecSignatureHashAlgorithm){
    kSecSignatureHashAlgorithmUnknown = 0,
    kSecSignatureHashAlgorithmMD2 = 1,
    kSecSignatureHashAlgorithmMD4 = 2,
    kSecSignatureHashAlgorithmMD5 = 3,
    kSecSignatureHashAlgorithmSHA1 = 4,
    kSecSignatureHashAlgorithmSHA224 = 5,
    kSecSignatureHashAlgorithmSHA256 = 6,
    kSecSignatureHashAlgorithmSHA384 = 7,
    kSecSignatureHashAlgorithmSHA512 = 8
};

/*!
	@function SecCertificateGetSignatureHashAlgorithm
	@abstract Determine the hash algorithm used in a certificate's signature.
	@param certificate A certificate reference.
	@result Returns an enumerated value indicating the signature hash algorithm
	used in a certificate. If the hash algorithm is unsupported or cannot be
	obtained (e.g. because the supplied certificate reference is invalid), a
	value of 0 (kSecSignatureHashAlgorithmUnknown) is returned.
*/
SecSignatureHashAlgorithm SecCertificateGetSignatureHashAlgorithm(SecCertificateRef certificate)
    __OSX_AVAILABLE_STARTING(__MAC_10_11, __IPHONE_9_0);

/* Return the auth capabilities bitmask from the iAP marker extension */
CF_RETURNS_RETAINED CFDataRef SecCertificateCopyiAPAuthCapabilities(SecCertificateRef certificate)
    __OSX_AVAILABLE_STARTING(__MAC_10_12, __IPHONE_10_0);

typedef CF_ENUM(uint32_t, SeciAuthVersion) {
    kSeciAuthInvalid = 0,
    kSeciAuthVersion1 = 1, /* unused */
    kSeciAuthVersion2 = 2,
    kSeciAuthVersion3 = 3,
} __OSX_AVAILABLE_STARTING(__MAC_10_12, __IPHONE_10_0);

/* Return the iAuth version indicated by the certificate. This function does
 * not guarantee that the certificate is valid, so the caller must still call
 * SecTrustEvaluate to guarantee that the certificate was properly issued */
SeciAuthVersion SecCertificateGetiAuthVersion(SecCertificateRef certificate)
    __OSX_AVAILABLE_STARTING(__MAC_10_12, __IPHONE_10_0);

__END_DECLS

#endif /* !_SECURITY_SECCERTIFICATEPRIV_H_ */

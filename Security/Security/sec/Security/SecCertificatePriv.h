/*
 * Copyright (c) 2006-2014 Apple Inc. All Rights Reserved.
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

typedef uint32_t SecKeyUsage;
enum {
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

typedef uint32_t SecCertificateEscrowRootType;
enum {
    kSecCertificateBaselineEscrowRoot = 0,
    kSecCertificateProductionEscrowRoot = 1,
    kSecCertificateBaselinePCSEscrowRoot = 2,
    kSecCertificateProductionPCSEscrowRoot = 3,
};

/* The names of the files that contain the escrow certificates */
extern CFTypeRef kSecCertificateProductionEscrowKey;
extern CFTypeRef kSecCertificateProductionPCSEscrowKey;
extern CFTypeRef kSecCertificateEscrowFileName;


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
CFDataRef SecCertificateCopySerialNumber(SecCertificateRef certificate);

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

/* Return true iff certificate is self signed and has a basic constraints
   extension indicating that it's a certificate authority. */
bool SecCertificateIsSelfSignedCA(SecCertificateRef certificate);

SecKeyUsage SecCertificateGetKeyUsage(SecCertificateRef certificate);

/* Returns an array of CFDataRefs for all extended key usage oids or NULL */
CFArrayRef SecCertificateCopyExtendedKeyUsage(SecCertificateRef certificate);

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

__END_DECLS

#endif /* !_SECURITY_SECCERTIFICATEPRIV_H_ */

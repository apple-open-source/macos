/*
 * Copyright (c) 2006-2010,2013 Apple Inc. All Rights Reserved.
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

#ifndef _SECURITY_SECCERTIFICATEPRIVP_H_
#define _SECURITY_SECCERTIFICATEPRIVP_H_

//#include <Security/SecCertificate.h>
#include "SecCertificateP.h"
#include <CoreFoundation/CFArray.h>
#include <CoreFoundation/CFData.h>
#include <CoreFoundation/CFDate.h>
#include <CoreFoundation/CFDictionary.h>
#include <stdbool.h>

#if defined(__cplusplus)
extern "C" {
#endif

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

/* Return a certificate for the DER representation of this certificate.
   Return NULL if the passed-in data is not a valid DER-encoded X.509
   certificate. */
SecCertificateRefP SecCertificateCreateWithBytesP(CFAllocatorRef allocator,
	const UInt8 *bytes, CFIndex length);

/* Return the length of the DER representation of this certificate. */
CFIndex SecCertificateGetLengthP(SecCertificateRefP certificate);

/* Return the bytes of the DER representation of this certificate. */
const UInt8 *SecCertificateGetBytePtrP(SecCertificateRefP certificate);

#pragma mark -
#pragma mark Certificate Accessors

CFDataRef SecCertificateGetSHA1DigestP(SecCertificateRefP certificate);

CFDataRef SecCertificateCopyIssuerSHA1Digest(SecCertificateRefP certificate);

CFDataRef SecCertificateCopyPublicKeySHA1Digest(SecCertificateRefP certificate);

CFStringRef SecCertificateCopyIssuerSummaryP(SecCertificateRefP certificate);

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
CFArrayRef SecCertificateCopyProperties(SecCertificateRefP certificate);

CFMutableArrayRef SecCertificateCopySummaryProperties(
    SecCertificateRefP certificate, CFAbsoluteTime verifyTime);

/* Return the content of a DER-encoded integer (without the tag and length
   fields) for this certificate's serial number.   The caller must CFRelease
   the value returned.  */
CFDataRef SecCertificateCopySerialNumberP(SecCertificateRefP certificate);

/* Return an array of CFStringRefs representing the ip addresses in the
   certificate if any. */
CFArrayRef SecCertificateCopyIPAddresses(SecCertificateRefP certificate);

/* Return an array of CFStringRefs representing the dns addresses in the
   certificate if any. */
CFArrayRef SecCertificateCopyDNSNamesP(SecCertificateRefP certificate);

/* Return an array of CFStringRefs representing the email addresses in the
   certificate if any. */
CFArrayRef SecCertificateCopyRFC822Names(SecCertificateRefP certificate);

/* Return an array of CFStringRefs representing the common names in the
   certificates subject if any. */
CFArrayRef SecCertificateCopyCommonNames(SecCertificateRefP certificate);

/* Return an array of CFStringRefs representing the organization in the
   certificate's subject if any. */
CFArrayRef SecCertificateCopyOrganization(SecCertificateRefP certificate);

/* Return an array of CFStringRefs representing the NTPrincipalNames in the
   certificate if any. */
CFArrayRef SecCertificateCopyNTPrincipalNames(SecCertificateRefP certificate);

/* Return a string formatted according to RFC 2253 representing the complete
   subject of certificate. */
CFStringRef SecCertificateCopySubjectString(SecCertificateRefP certificate);

/* Return a string with the company name of an ev leaf certificate. */
CFStringRef SecCertificateCopyCompanyName(SecCertificateRefP certificate);

/* X.509 Certificate Version: 1, 2 or 3. */
CFIndex SecCertificateVersion(SecCertificateRefP certificate);

CFAbsoluteTime SecCertificateNotValidBeforeP(SecCertificateRefP certificate);
CFAbsoluteTime SecCertificateNotValidAfterP(SecCertificateRefP certificate);

/* Return true iff certificate is self signed and has a basic constraints
   extension indicating that it's a certificate authority. */
bool SecCertificateIsSelfSignedCA(SecCertificateRefP certificate);

SecKeyUsage SecCertificateGetKeyUsage(SecCertificateRefP certificate);

/* Returns an array of CFDataRefs for all extended key usage oids or NULL */
CFArrayRef SecCertificateCopyExtendedKeyUsage(SecCertificateRefP certificate);

/* Returns a certificate from a pem blob */
SecCertificateRefP SecCertificateCreateWithPEM(CFAllocatorRef allocator,
	CFDataRef pem_certificate);

/* Return an array of CFDataRefs from an array of SecCertificateRefPs. */
CFArrayRef SecCertificateArrayCopyDataArray(CFArrayRef certificates);

/* Return an array of SecCertificateRefPs from an array of CFDataRefs. */
CFArrayRef SecCertificateDataArrayCopyArray(CFArrayRef certificates);

CFDataRef SecCertificateGetNormalizedIssuerContent(SecCertificateRefP certificate);
CFDataRef SecCertificateGetNormalizedSubjectContent(SecCertificateRefP certificate);

CFDataRef SecCertificateCopyNormalizedIssuerSequence(SecCertificateRefP certificate);
CFDataRef SecCertificateCopyNormalizedSubjectSequence(SecCertificateRefP certificate);

#if defined(__cplusplus)
}
#endif

#endif /* !_SECURITY_SECCERTIFICATEPRIVP_H_ */

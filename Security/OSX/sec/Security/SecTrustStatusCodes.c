/*
 * Copyright (c) 2017 Apple Inc. All Rights Reserved.
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
 *
 * SecTrustStatusCodes.c - map trust result details to status codes
 *
 */

#include <Security/SecTrustPriv.h>
#include <Security/SecInternal.h>
#include <Security/SecTrustStatusCodes.h>
#include <CoreFoundation/CoreFoundation.h>

struct resultmap_entry_s {
    const CFStringRef checkstr;
    const int32_t resultcode;
};
typedef struct resultmap_entry_s resultmap_entry_t;

const resultmap_entry_t resultmap[] = {
    { CFSTR("SSLHostname"), 0x80012400, /* CSSMERR_APPLETP_HOSTNAME_MISMATCH */},
    { CFSTR("email"), 0x80012418, /* CSSMERR_APPLETP_SMIME_EMAIL_ADDRS_NOT_FOUND */},
    { CFSTR("IssuerCommonName"), 0x8001243B /* CSSMERR_APPLETP_IDENTIFIER_MISSING */},
    { CFSTR("SubjectCommonName"), 0x8001243B /* CSSMERR_APPLETP_IDENTIFIER_MISSING */},
    { CFSTR("SubjectCommonNamePrefix"), 0x8001243B /* CSSMERR_APPLETP_IDENTIFIER_MISSING */},
    { CFSTR("SubjectCommonNameTEST"), 0x8001243B /* CSSMERR_APPLETP_IDENTIFIER_MISSING */},
    { CFSTR("SubjectOrganization"), 0x8001243B /* CSSMERR_APPLETP_IDENTIFIER_MISSING */},
    { CFSTR("SubjectOrganizationalUnit"), 0x8001243B /* CSSMERR_APPLETP_IDENTIFIER_MISSING */},
    { CFSTR("EAPTrustedServerNames"), 0x80012400 /* CSSMERR_APPLETP_HOSTNAME_MISMATCH */},
    { CFSTR("CertificatePolicy"), 0x80012439 /* CSSMERR_APPLETP_MISSING_REQUIRED_EXTENSION */},
    { CFSTR("KeyUsage"), 0x80012406 /* CSSMERR_APPLETP_INVALID_KEY_USAGE */},
    { CFSTR("ExtendedKeyUsage"), 0x80012407 /* CSSMERR_APPLETP_INVALID_EXTENDED_KEY_USAGE */},
    { CFSTR("BasicConstraints"), 0x80012402 /* CSSMERR_APPLETP_NO_BASIC_CONSTRAINTS */},
    { CFSTR("QualifiedCertStatements"), 0x80012438 /* CSSMERR_APPLETP_UNKNOWN_QUAL_CERT_STATEMENT */},
    { CFSTR("IntermediateSPKISHA256"), 0x8001243B /* CSSMERR_APPLETP_IDENTIFIER_MISSING */},
    { CFSTR("IntermediateEKU"), 0x80012407 /* CSSMERR_APPLETP_INVALID_EXTENDED_KEY_USAGE */},
    { CFSTR("AnchorSHA1"), 0x8001212A /* CSSMERR_TP_NOT_TRUSTED */},
    { CFSTR("AnchorSHA256"), 0x8001212A /* CSSMERR_TP_NOT_TRUSTED */},
    { CFSTR("AnchorTrusted"), 0x8001212A /* CSSMERR_TP_NOT_TRUSTED */},
    { CFSTR("AnchorApple"), 0x8001243C /* CSSMERR_APPLETP_CA_PIN_MISMATCH */},
    { CFSTR("NonEmptySubject"), 0x80012437 /* CSSMERR_APPLETP_INVALID_EMPTY_SUBJECT */},
    { CFSTR("IdLinkage"), 0x80012404 /* CSSMERR_APPLETP_INVALID_AUTHORITY_ID */},
    { CFSTR("WeakIntermediates"), 0x80012115 /* CSSMERR_TP_INVALID_CERTIFICATE */},
    { CFSTR("WeakLeaf"), 0x80012115 /* CSSMERR_TP_INVALID_CERTIFICATE */},
    { CFSTR("WeakRoot"), 0x80012115 /* CSSMERR_TP_INVALID_CERTIFICATE */},
    { CFSTR("KeySize"), 0x80010918 /* CSSMERR_CSP_UNSUPPORTED_KEY_SIZE */},
    { CFSTR("SignatureHashAlgorithms"), 0x80010913 /* CSSMERR_CSP_ALGID_MISMATCH */},
    { CFSTR("SystemTrustedWeakHash"), 0x80010955 /* CSSMERR_CSP_INVALID_DIGEST_ALGORITHM */},
    { CFSTR("CriticalExtensions"), 0x80012401 /* CSSMERR_APPLETP_UNKNOWN_CRITICAL_EXTEN */},
    { CFSTR("ChainLength"), 0x80012409 /* CSSMERR_APPLETP_PATH_LEN_CONSTRAINT */},
    { CFSTR("BasicCertificateProcessing"), 0x80012115 /* CSSMERR_TP_INVALID_CERTIFICATE */},
    { CFSTR("ExtendedValidation"), 0x8001212A /* CSSMERR_TP_NOT_TRUSTED */},
    { CFSTR("Revocation"), 0x8001210C /* CSSMERR_TP_CERT_REVOKED */},
    { CFSTR("RevocationResponseRequired"), 0x80012423 /* CSSMERR_APPLETP_INCOMPLETE_REVOCATION_CHECK */},
    { CFSTR("CertificateTransparency"), 0x8001212A /* CSSMERR_TP_NOT_TRUSTED */},
    { CFSTR("BlackListedLeaf"), 0x8001210C /* CSSMERR_TP_CERT_REVOKED */},
    { CFSTR("GrayListedLeaf"), 0x8001212A /* CSSMERR_TP_NOT_TRUSTED */},
    { CFSTR("GrayListedKey"), 0x8001212A /* CSSMERR_TP_NOT_TRUSTED */},
    { CFSTR("BlackListedKey"), 0x8001210C /* CSSMERR_TP_CERT_REVOKED */},
    { CFSTR("CheckLeafMarkerOid"), 0x80012439 /* CSSMERR_APPLETP_MISSING_REQUIRED_EXTENSION */},
    { CFSTR("CheckLeafMarkerOidNoValueCheck"), 0x80012439 /* CSSMERR_APPLETP_MISSING_REQUIRED_EXTENSION */},
    { CFSTR("CheckIntermediateMarkerOid"), 0x80012439 /* CSSMERR_APPLETP_MISSING_REQUIRED_EXTENSION */},
    { CFSTR("UsageConstraints"), 0x80012436 /* CSSMERR_APPLETP_TRUST_SETTING_DENY */},
    { CFSTR("NotValidBefore"), 0x8001210B /* CSSMERR_TP_CERT_NOT_VALID_YET */},
    { CFSTR("ValidIntermediates"), 0x8001210A /* CSSMERR_TP_CERT_EXPIRED */},
    { CFSTR("ValidLeaf"), 0x8001210A /* CSSMERR_TP_CERT_EXPIRED */},
    { CFSTR("ValidRoot"), 0x8001210A /* CSSMERR_TP_CERT_EXPIRED */},
};

//
// Returns a malloced array of SInt32 values, with the length in numStatusCodes,
// for the certificate specified by chain index in the given SecTrustRef.
//
// To match legacy behavior, the array actually allocates one element more than the
// value of numStatusCodes; if the certificate is revoked, the additional element
// at the end contains the CrlReason value.
//
// Caller must free the returned pointer.
//
SInt32 *SecTrustCopyStatusCodes(SecTrustRef trust,
    CFIndex index, CFIndex *numStatusCodes)
{
    if (!trust || !numStatusCodes) {
        return NULL;
    }
    *numStatusCodes = 0;
    CFArrayRef details = SecTrustCopyFilteredDetails(trust);
    CFIndex chainLength = (details) ? CFArrayGetCount(details) : 0;
    if (!(index < chainLength)) {
        CFReleaseSafe(details);
        return NULL;
    }
    CFDictionaryRef detail = (CFDictionaryRef)CFArrayGetValueAtIndex(details, index);
    CFIndex ix, detailCount = CFDictionaryGetCount(detail);
    *numStatusCodes = (unsigned int)detailCount;

    // Allocate one more entry than we need; this is used to store a CrlReason
    // at the end of the array.
    SInt32 *statusCodes = (SInt32*)malloc((detailCount+1) * sizeof(SInt32));
    statusCodes[*numStatusCodes] = 0;

    const unsigned int resultmaplen = sizeof(resultmap) / sizeof(resultmap_entry_t);
    const void *keys[detailCount];
    CFDictionaryGetKeysAndValues(detail, &keys[0], NULL);
    for (ix = 0; ix < detailCount; ix++) {
        CFStringRef key = (CFStringRef)keys[ix];
        SInt32 statusCode = 0;
        for (unsigned int mapix = 0; mapix < resultmaplen; mapix++) {
            CFStringRef str = (CFStringRef) resultmap[mapix].checkstr;
            if (CFStringCompare(str, key, 0) == kCFCompareEqualTo) {
                statusCode = (SInt32) resultmap[mapix].resultcode;
                break;
            }
        }
        if (statusCode == (SInt32)0x8001210C) {  /* CSSMERR_TP_CERT_REVOKED */
            SInt32 reason;
            CFNumberRef number = (CFNumberRef)CFDictionaryGetValue(detail, key);
            if (number && CFNumberGetValue(number, kCFNumberSInt32Type, &reason)) {
                statusCodes[*numStatusCodes] = (SInt32)reason;
            }
        }
        statusCodes[ix] = statusCode;
    }

    CFReleaseSafe(details);
    return statusCodes;
}

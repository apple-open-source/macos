/*
 * Copyright (c) 2006-2016 Apple Inc. All Rights Reserved.
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
 * SecCertificate.c - CoreFoundation based certificate object
 */

#ifdef STANDALONE
/* Allows us to build genanchors against the BaseSDK. */
#undef __ENVIRONMENT_IPHONE_OS_VERSION_MIN_REQUIRED__
#undef __ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__
#endif

#include <Security/SecCertificateInternal.h>
#include <utilities/SecIOFormat.h>
#include <CommonCrypto/CommonDigest.h>
#include <CoreFoundation/CFRuntime.h>
#include <CoreFoundation/CFString.h>
#include <CoreFoundation/CFBundle.h>
#include <CoreFoundation/CFDictionary.h>
#include <CoreFoundation/CFNumber.h>
#include <CoreFoundation/CFCalendar.h>
#include <CoreFoundation/CFTimeZone.h>
#include <CoreFoundation/CFXPCBridge.h>
#include <string.h>
#include <AssertMacros.h>
#include <libDER/DER_CertCrl.h>
#include <libDER/DER_Encode.h>
#include <libDER/DER_Keys.h>
#include <libDER/asn1Types.h>
#include <libDER/oidsPriv.h>
#include "SecBasePriv.h"
#include "SecRSAKey.h"
#include "SecFramework.h"
#include "SecItem.h"
#include "SecItemPriv.h"
#include "SecSignatureVerificationSupport.h"
#include <stdbool.h>
#include <utilities/debugging.h>
#include <utilities/SecCFWrappers.h>
#include <utilities/SecCFError.h>
#include <utilities/SecSCTUtils.h>
#include <utilities/array_size.h>
#include <stdlib.h>
#include <libkern/OSByteOrder.h>
#include <ctype.h>
#include <Security/SecInternal.h>
#include <Security/SecFrameworkStrings.h>
#include "SecBase64.h"
#include "AppleBaselineEscrowCertificates.h"
#include <ipc/securityd_client.h>
#include <Security/SecKeyInternal.h>

/* The minimum key sizes necessary to not be considered "weak" */
#define MIN_RSA_KEY_SIZE    128     // 1024-bit
#define MIN_EC_KEY_SIZE     20      // 160-bit

typedef struct SecCertificateExtension {
	DERItem extnID;
    bool critical;
    DERItem extnValue;
} SecCertificateExtension;

enum {
    kSecSelfSignedUnknown = 0,
    kSecSelfSignedFalse,
    kSecSelfSignedTrue,
};

struct __SecCertificate {
    CFRuntimeBase       _base;

    DERItem             _der;           /* Entire certificate in DER form. */
    DERItem             _tbs;           /* To Be Signed cert DER bytes. */
    DERAlgorithmId      _sigAlg;        /* Top level signature algorithm. */
    DERItem             _signature;     /* The content of the sig bit string. */

    UInt8               _version;
    DERItem             _serialNum;     /* Integer. */
    DERAlgorithmId      _tbsSigAlg;     /* sig alg MUST be same as _sigAlg. */
    DERItem             _issuer;        /* Sequence of RDN. */
    CFAbsoluteTime      _notBefore;
    CFAbsoluteTime      _notAfter;
    DERItem             _subject;       /* Sequence of RDN. */
    DERItem             _subjectPublicKeyInfo; /* SPKI */
    DERAlgorithmId      _algId;         /* oid and params of _pubKeyDER. */
    DERItem             _pubKeyDER;     /* contents of bit string */
    DERItem             _issuerUniqueID;    /* bit string, optional */
    DERItem             _subjectUniqueID;   /* bit string, optional */

    bool                _foundUnknownCriticalExtension;

    /* Well known certificate extensions. */
    SecCEBasicConstraints       _basicConstraints;
    SecCEPolicyConstraints      _policyConstraints;
    SecCEPolicyMappings         _policyMappings;
    SecCECertificatePolicies    _certificatePolicies;
    SecCEInhibitAnyPolicy       _inhibitAnyPolicySkipCerts;

    /* If KeyUsage extension is not present this is 0, otherwise it's
       the value of the extension. */
    SecKeyUsage _keyUsage;

    /* OCTETS of SubjectKeyIdentifier extensions KeyIdentifier.
       Length = 0 if not present. */
    DERItem             _subjectKeyIdentifier;

    /* OCTETS of AuthorityKeyIdentifier extensions KeyIdentifier.
       Length = 0 if not present. */
    DERItem             _authorityKeyIdentifier;
    /* AuthorityKeyIdentifier extension _authorityKeyIdentifierIssuer and
       _authorityKeyIdentifierSerialNumber have non zero length if present.
       Both are either present or absent together.  */
    DERItem             _authorityKeyIdentifierIssuer;
    DERItem             _authorityKeyIdentifierSerialNumber;

    /* Subject alt name extension, if present.  Not malloced, it's just a
       pointer to an element in the _extensions array. */
    const SecCertificateExtension  *_subjectAltName;

    /* Parsed extension values. */

    /* Array of CFURLRefs containing the URI values of crlDistributionPoints. */
    CFMutableArrayRef   _crlDistributionPoints;

    /* Array of CFURLRefs containing the URI values of accessLocations of each
       id-ad-ocsp AccessDescription in the Authority Information Access
       extension. */
    CFMutableArrayRef   _ocspResponders;

    /* Array of CFURLRefs containing the URI values of accessLocations of each
       id-ad-caIssuers AccessDescription in the Authority Information Access
       extension. */
    CFMutableArrayRef   _caIssuers;

    /* Array of CFDataRefs containing the generalNames for permittedSubtrees
       Name Constraints.*/
    CFArrayRef          _permittedSubtrees;

    /* Array of CFDataRefs containing the generalNames for excludedSubtrees
     Name Constraints. */
    CFArrayRef          _excludedSubtrees;

    CFMutableArrayRef   _embeddedSCTs;

    /* All other (non known) extensions.   The _extensions array is malloced. */
    CFIndex             _extensionCount;
    SecCertificateExtension *_extensions;

    /* Optional cached fields. */
    SecKeyRef           _pubKey;
    CFDataRef           _der_data;
    CFArrayRef          _properties;
    CFDataRef           _serialNumber;
    CFDataRef           _normalizedIssuer;
    CFDataRef           _normalizedSubject;
    CFDataRef           _authorityKeyID;
    CFDataRef           _subjectKeyID;

    CFDataRef           _sha1Digest;
    CFTypeRef           _keychain_item;
    uint8_t             _isSelfSigned;

};

#define SEC_CONST_DECL(k,v) const CFStringRef k = CFSTR(v);

SEC_CONST_DECL (kSecCertificateProductionEscrowKey, "ProductionEscrowKey");
SEC_CONST_DECL (kSecCertificateProductionPCSEscrowKey, "ProductionPCSEscrowKey");
SEC_CONST_DECL (kSecCertificateEscrowFileName, "AppleESCertificates");

/* Public Constants for property list keys. */
SEC_CONST_DECL (kSecPropertyKeyType, "type");
SEC_CONST_DECL (kSecPropertyKeyLabel, "label");
SEC_CONST_DECL (kSecPropertyKeyLocalizedLabel, "localized label");
SEC_CONST_DECL (kSecPropertyKeyValue, "value");

/* Public Constants for property list values. */
SEC_CONST_DECL (kSecPropertyTypeWarning, "warning");
SEC_CONST_DECL (kSecPropertyTypeError, "error");
SEC_CONST_DECL (kSecPropertyTypeSuccess, "success");
SEC_CONST_DECL (kSecPropertyTypeTitle, "title");
SEC_CONST_DECL (kSecPropertyTypeSection, "section");
SEC_CONST_DECL (kSecPropertyTypeData, "data");
SEC_CONST_DECL (kSecPropertyTypeString, "string");
SEC_CONST_DECL (kSecPropertyTypeURL, "url");
SEC_CONST_DECL (kSecPropertyTypeDate, "date");

/* Extension parsing routine. */
typedef void (*SecCertificateExtensionParser)(SecCertificateRef certificate,
	const SecCertificateExtension *extn);

/* Mapping from extension OIDs (as a DERItem *) to
   SecCertificateExtensionParser extension parsing routines. */
static CFDictionaryRef sExtensionParsers;

/* Forward declarations of static functions. */
static CFStringRef SecCertificateCopyDescription(CFTypeRef cf);
static void SecCertificateDestroy(CFTypeRef cf);
static bool derDateGetAbsoluteTime(const DERItem *dateChoice,
    CFAbsoluteTime *absTime) __attribute__((__nonnull__));

/* Static functions. */
static CFStringRef SecCertificateCopyDescription(CFTypeRef cf) {
    SecCertificateRef certificate = (SecCertificateRef)cf;
    CFStringRef subject = SecCertificateCopySubjectSummary(certificate);
    CFStringRef issuer = SecCertificateCopyIssuerSummary(certificate);
    CFStringRef desc = CFStringCreateWithFormat(kCFAllocatorDefault, NULL,
        CFSTR("<cert(%p) s: %@ i: %@>"), certificate, subject, issuer);
    CFReleaseSafe(issuer);
    CFReleaseSafe(subject);
    return desc;
}

static void SecCertificateDestroy(CFTypeRef cf) {
    SecCertificateRef certificate = (SecCertificateRef)cf;
    if (certificate->_certificatePolicies.policies)
        free(certificate->_certificatePolicies.policies);
    if (certificate->_policyMappings.mappings) {
        free(certificate->_policyMappings.mappings);
    }
    CFReleaseSafe(certificate->_crlDistributionPoints);
    CFReleaseSafe(certificate->_ocspResponders);
    CFReleaseSafe(certificate->_caIssuers);
    if (certificate->_extensions) {
        free(certificate->_extensions);
    }
    CFReleaseSafe(certificate->_pubKey);
    CFReleaseSafe(certificate->_der_data);
    CFReleaseSafe(certificate->_properties);
    CFReleaseSafe(certificate->_serialNumber);
    CFReleaseSafe(certificate->_normalizedIssuer);
    CFReleaseSafe(certificate->_normalizedSubject);
    CFReleaseSafe(certificate->_authorityKeyID);
    CFReleaseSafe(certificate->_subjectKeyID);
    CFReleaseSafe(certificate->_sha1Digest);
    CFReleaseSafe(certificate->_keychain_item);
    CFReleaseSafe(certificate->_permittedSubtrees);
    CFReleaseSafe(certificate->_excludedSubtrees);
}

static Boolean SecCertificateEqual(CFTypeRef cf1, CFTypeRef cf2) {
    SecCertificateRef cert1 = (SecCertificateRef)cf1;
    SecCertificateRef cert2 = (SecCertificateRef)cf2;
    if (cert1 == cert2)
        return true;
    if (!cert2 || cert1->_der.length != cert2->_der.length)
        return false;
    return !memcmp(cert1->_der.data, cert2->_der.data, cert1->_der.length);
}

/* Hash of the certificate is der length + signature length + last 4 bytes
   of signature. */
static CFHashCode SecCertificateHash(CFTypeRef cf) {
    SecCertificateRef certificate = (SecCertificateRef)cf;
	size_t der_length = certificate->_der.length;
	size_t sig_length = certificate->_signature.length;
	size_t ix = (sig_length > 4) ? sig_length - 4 : 0;
	CFHashCode hashCode = 0;
	for (; ix < sig_length; ++ix)
		hashCode = (hashCode << 8) + certificate->_signature.data[ix];

	return (hashCode + der_length + sig_length);
}

#if 1

/************************************************************************/
/************************* General Name Parsing *************************/
/************************************************************************/
/*
      GeneralName ::= CHOICE {
           otherName                       [0]     OtherName,
           rfc822Name                      [1]     IA5String,
           dNSName                         [2]     IA5String,
           x400Address                     [3]     ORAddress,
           directoryName                   [4]     Name,
           ediPartyName                    [5]     EDIPartyName,
           uniformResourceIdentifier       [6]     IA5String,
           iPAddress                       [7]     OCTET STRING,
           registeredID                    [8]     OBJECT IDENTIFIER}

      OtherName ::= SEQUENCE {
           type-id    OBJECT IDENTIFIER,
           value      [0] EXPLICIT ANY DEFINED BY type-id }

      EDIPartyName ::= SEQUENCE {
           nameAssigner            [0]     DirectoryString OPTIONAL,
           partyName               [1]     DirectoryString }
 */
OSStatus SecCertificateParseGeneralNameContentProperty(DERTag tag,
	const DERItem *generalNameContent,
	void *context, parseGeneralNameCallback callback) {
	switch (tag) {
	case ASN1_CONTEXT_SPECIFIC | ASN1_CONSTRUCTED | 0:
		return callback(context, GNT_OtherName, generalNameContent);
	case ASN1_CONTEXT_SPECIFIC | 1:
		return callback(context, GNT_RFC822Name, generalNameContent);
	case ASN1_CONTEXT_SPECIFIC | 2:
		return callback(context, GNT_DNSName, generalNameContent);
	case ASN1_CONTEXT_SPECIFIC | ASN1_CONSTRUCTED | 3:
		return callback(context, GNT_X400Address, generalNameContent);
	case ASN1_CONTEXT_SPECIFIC | ASN1_CONSTRUCTED | 4:
		return callback(context, GNT_DirectoryName, generalNameContent);
	case ASN1_CONTEXT_SPECIFIC | ASN1_CONSTRUCTED | 5:
		return callback(context, GNT_EdiPartyName, generalNameContent);
	case ASN1_CONTEXT_SPECIFIC | ASN1_CONSTRUCTED | 6:
	{
		/* Technically I don't think this is valid, but there are certs out
		   in the wild that use a constructed IA5String.   In particular the
		   VeriSign Time Stamping Authority CA.cer does this.  */
		DERDecodedInfo uriContent;
		require_noerr(DERDecodeItem(generalNameContent, &uriContent), badDER);
		require(uriContent.tag == ASN1_IA5_STRING, badDER);
		return callback(context, GNT_URI, &uriContent.content);
	}
	case ASN1_CONTEXT_SPECIFIC | 6:
		return callback(context, GNT_URI, generalNameContent);
	case ASN1_CONTEXT_SPECIFIC | 7:
		return callback(context, GNT_IPAddress, generalNameContent);
	case ASN1_CONTEXT_SPECIFIC | 8:
		return callback(context, GNT_RegisteredID, generalNameContent);
	default:
		goto badDER;
	}
badDER:
	return errSecInvalidCertificate;
}

static OSStatus parseGeneralNamesContent(const DERItem *generalNamesContent,
	void *context, parseGeneralNameCallback callback) {
    DERSequence gnSeq;
    DERReturn drtn = DERDecodeSeqContentInit(generalNamesContent, &gnSeq);
    require_noerr_quiet(drtn, badDER);
    DERDecodedInfo generalNameContent;
    while ((drtn = DERDecodeSeqNext(&gnSeq, &generalNameContent)) ==
		DR_Success) {
		OSStatus status = SecCertificateParseGeneralNameContentProperty(
			generalNameContent.tag, &generalNameContent.content, context,
				callback);
		if (status)
			return status;
	}
    require_quiet(drtn == DR_EndOfSequence, badDER);
	return errSecSuccess;

badDER:
	return errSecInvalidCertificate;
}

OSStatus SecCertificateParseGeneralNames(const DERItem *generalNames, void *context,
	parseGeneralNameCallback callback) {
    DERDecodedInfo generalNamesContent;
    DERReturn drtn = DERDecodeItem(generalNames, &generalNamesContent);
    require_noerr_quiet(drtn, badDER);
    require_quiet(generalNamesContent.tag == ASN1_CONSTR_SEQUENCE, badDER);
    return parseGeneralNamesContent(&generalNamesContent.content, context,
		callback);
badDER:
	return errSecInvalidCertificate;
}

#else

/*
      GeneralName ::= CHOICE {
           otherName                       [0]     OtherName,
           rfc822Name                      [1]     IA5String,
           dNSName                         [2]     IA5String,
           x400Address                     [3]     ORAddress,
           directoryName                   [4]     Name,
           ediPartyName                    [5]     EDIPartyName,
           uniformResourceIdentifier       [6]     IA5String,
           iPAddress                       [7]     OCTET STRING,
           registeredID                    [8]     OBJECT IDENTIFIER}

      EDIPartyName ::= SEQUENCE {
           nameAssigner            [0]     DirectoryString OPTIONAL,
           partyName               [1]     DirectoryString }
 */
static OSStatus parseGeneralNameContentProperty(DERTag tag,
	const DERItem *generalNameContent, SecCEGeneralName *generalName) {
	switch (tag) {
	case ASN1_CONTEXT_SPECIFIC | ASN1_CONSTRUCTED | 0:
		generalName->nameType = GNT_OtherName;
		generalName->berEncoded = true;
		generalName->name = *generalNameContent;
		break;
	case ASN1_CONTEXT_SPECIFIC | 1:
		/* IA5String. */
		generalName->nameType = GNT_RFC822Name;
		generalName->berEncoded = false;
		generalName->name = *generalNameContent;
		break;
	case ASN1_CONTEXT_SPECIFIC | 2:
		/* IA5String. */
		generalName->nameType = GNT_DNSName;
		generalName->berEncoded = false;
		generalName->name = *generalNameContent;
		break;
	case ASN1_CONTEXT_SPECIFIC | ASN1_CONSTRUCTED | 3:
		generalName->nameType = GNT_X400Address;
		generalName->berEncoded = true;
		generalName->name = *generalNameContent;
		break;
	case ASN1_CONTEXT_SPECIFIC | ASN1_CONSTRUCTED | 4:
		generalName->nameType = GNT_DirectoryName;
		generalName->berEncoded = true;
		generalName->name = *generalNameContent;
		break;
	case ASN1_CONTEXT_SPECIFIC | ASN1_CONSTRUCTED | 5:
		generalName->nameType = GNT_EdiPartyName;
		generalName->berEncoded = true;
		generalName->name = *generalNameContent;
		break;
	case ASN1_CONTEXT_SPECIFIC | ASN1_CONSTRUCTED | 6:
	{
		/* Technically I don't think this is valid, but there are certs out
		   in the wild that use a constructed IA5String.   In particular the
		   VeriSign Time Stamping Authority CA.cer does this.  */
		DERDecodedInfo decoded;
		require_noerr(DERDecodeItem(generalNameContent, &decoded), badDER);
		require(decoded.tag == ASN1_IA5_STRING, badDER);
		generalName->nameType = GNT_URI;
		generalName->berEncoded = false;
		generalName->name = decoded.content;
		break;
	}
	case ASN1_CONTEXT_SPECIFIC | 6:
		generalName->nameType = GNT_URI;
		generalName->berEncoded = false;
		generalName->name = *generalNameContent;
		break;
	case ASN1_CONTEXT_SPECIFIC | 7:
		/* @@@ This is the IP Address as an OCTECT STRING. For IPv4 it's
		   8 octects, addr/mask for ipv6 it's 32.  */
		generalName->nameType = GNT_IPAddress;
		generalName->berEncoded = false;
		generalName->name = *generalNameContent;
		break;
	case ASN1_CONTEXT_SPECIFIC | 8:
		/* name is the content of an OID. */
		generalName->nameType = GNT_RegisteredID;
		generalName->berEncoded = false;
		generalName->name = *generalNameContent;
		break;
	default:
		goto badDER;
		break;
	}
	return errSecSuccess;
badDER:
	return errSecInvalidCertificate;
}

/*
      GeneralNames ::= SEQUENCE SIZE (1..MAX) OF GeneralName
 */
static OSStatus parseGeneralNamesContent(const DERItem *generalNamesContent,
	CFIndex *count, SecCEGeneralName **name) {
	SecCEGeneralName *generalNames = NULL;
    DERSequence gnSeq;
    DERReturn drtn = DERDecodeSeqContentInit(generalNamesContent, &gnSeq);
    require_noerr_quiet(drtn, badDER);
    DERDecodedInfo generalNameContent;
	CFIndex generalNamesCount = 0;
    while ((drtn = DERDecodeSeqNext(&gnSeq, &generalNameContent)) ==
		DR_Success) {
		++generalNamesCount;
	}
    require_quiet(drtn == DR_EndOfSequence, badDER);

	require(generalNames = calloc(generalNamesCount, sizeof(SecCEGeneralName)),
		badDER);
    DERDecodeSeqContentInit(generalNamesContent, &gnSeq);
	CFIndex ix = 0;
    while ((drtn = DERDecodeSeqNext(&gnSeq, &generalNameContent)) ==
		DR_Success) {
		if (!parseGeneralNameContentProperty(generalNameContent.tag,
			&generalNameContent.content, &generalNames[ix])) {
			goto badDER;
		}
		++ix;
    }
	*count = generalNamesCount;
	*name = generalNames;
	return errSecSuccess;

badDER:
	if (generalNames)
		free(generalNames);
	return errSecInvalidCertificate;
}

static OSStatus parseGeneralNames(const DERItem *generalNames,
	CFIndex *count, SecCEGeneralName **name) {
    DERDecodedInfo generalNamesContent;
    DERReturn drtn = DERDecodeItem(generalNames, &generalNamesContent);
    require_noerr_quiet(drtn, badDER);
    require_quiet(generalNamesContent.tag == ASN1_CONSTR_SEQUENCE,
        badDER);
    parseGeneralNamesContent(&generalNamesContent.content, count, name);
    return errSecSuccess;
badDER:
	return errSecInvalidCertificate;
}
#endif

/************************************************************************/
/************************** X.509 Name Parsing **************************/
/************************************************************************/

typedef OSStatus (*parseX501NameCallback)(void *context, const DERItem *type,
	const DERItem *value, CFIndex rdnIX);

static OSStatus parseRDNContent(const DERItem *rdnSetContent, void *context,
	parseX501NameCallback callback) {
	DERSequence rdn;
	DERReturn drtn = DERDecodeSeqContentInit(rdnSetContent, &rdn);
	require_noerr_quiet(drtn, badDER);
	DERDecodedInfo atvContent;
	CFIndex rdnIX = 0;
	while ((drtn = DERDecodeSeqNext(&rdn, &atvContent)) == DR_Success) {
		require_quiet(atvContent.tag == ASN1_CONSTR_SEQUENCE, badDER);
		DERAttributeTypeAndValue atv;
		drtn = DERParseSequenceContent(&atvContent.content,
			DERNumAttributeTypeAndValueItemSpecs,
			DERAttributeTypeAndValueItemSpecs,
			&atv, sizeof(atv));
		require_noerr_quiet(drtn, badDER);
		require_quiet(atv.type.length != 0, badDER);
		OSStatus status = callback(context, &atv.type, &atv.value, rdnIX++);
		if (status)
			return status;
	}
	require_quiet(drtn == DR_EndOfSequence, badDER);

	return errSecSuccess;
badDER:
	return errSecInvalidCertificate;
}

static OSStatus parseX501NameContent(const DERItem *x501NameContent, void *context,
	parseX501NameCallback callback) {
	DERSequence derSeq;
	DERReturn drtn = DERDecodeSeqContentInit(x501NameContent, &derSeq);
	require_noerr_quiet(drtn, badDER);
	DERDecodedInfo currDecoded;
	while ((drtn = DERDecodeSeqNext(&derSeq, &currDecoded)) == DR_Success) {
		require_quiet(currDecoded.tag == ASN1_CONSTR_SET, badDER);
		OSStatus status = parseRDNContent(&currDecoded.content, context,
			callback);
		if (status)
			return status;
	}
	require_quiet(drtn == DR_EndOfSequence, badDER);

	return errSecSuccess;

badDER:
	return errSecInvalidCertificate;
}

static OSStatus parseX501Name(const DERItem *x501Name, void *context,
	parseX501NameCallback callback) {
	DERDecodedInfo x501NameContent;
	if (DERDecodeItem(x501Name, &x501NameContent) ||
        x501NameContent.tag != ASN1_CONSTR_SEQUENCE) {
		return errSecInvalidCertificate;
    } else {
        return parseX501NameContent(&x501NameContent.content, context,
			callback);
    }
}

/************************************************************************/
/********************** Extension Parsing Routines **********************/
/************************************************************************/

static void SecCEPSubjectKeyIdentifier(SecCertificateRef certificate,
	const SecCertificateExtension *extn) {
	secdebug("cert", "critical: %s", extn->critical ? "yes" : "no");
    DERDecodedInfo keyIdentifier;
	DERReturn drtn = DERDecodeItem(&extn->extnValue, &keyIdentifier);
	require_noerr_quiet(drtn, badDER);
	require_quiet(keyIdentifier.tag == ASN1_OCTET_STRING, badDER);
	certificate->_subjectKeyIdentifier = keyIdentifier.content;

	return;
badDER:
	secwarning("Invalid SubjectKeyIdentifier Extension");
}

static void SecCEPKeyUsage(SecCertificateRef certificate,
	const SecCertificateExtension *extn) {
	secdebug("cert", "critical: %s", extn->critical ? "yes" : "no");
    SecKeyUsage keyUsage = extn->critical ? kSecKeyUsageCritical : 0;
    DERDecodedInfo bitStringContent;
    DERReturn drtn = DERDecodeItem(&extn->extnValue, &bitStringContent);
    require_noerr_quiet(drtn, badDER);
    require_quiet(bitStringContent.tag == ASN1_BIT_STRING, badDER);
    DERSize len = bitStringContent.content.length - 1;
    require_quiet(len == 1 || len == 2, badDER);
    DERByte numUnusedBits = bitStringContent.content.data[0];
    require_quiet(numUnusedBits < 8, badDER);
    /* Flip the bits in the bit string so the first bit in the lsb. */
    uint_fast16_t bits = 8 * len - numUnusedBits;
    uint_fast16_t value = bitStringContent.content.data[1];
    uint_fast16_t mask;
    if (len > 1) {
        value = (value << 8) + bitStringContent.content.data[2];
        mask = 0x8000;
    } else {
        mask = 0x80;
    }
    uint_fast16_t ix;
    for (ix = 0; ix < bits; ++ix) {
        if (value & mask) {
            keyUsage |= 1 << ix;
        }
        mask >>= 1;
    }
    certificate->_keyUsage = keyUsage;
    return;
badDER:
    certificate->_keyUsage = kSecKeyUsageUnspecified;
}

static void SecCEPPrivateKeyUsagePeriod(SecCertificateRef certificate,
	const SecCertificateExtension *extn) {
	secdebug("cert", "critical: %s", extn->critical ? "yes" : "no");
}

static void SecCEPSubjectAltName(SecCertificateRef certificate,
	const SecCertificateExtension *extn) {
	secdebug("cert", "critical: %s", extn->critical ? "yes" : "no");
	certificate->_subjectAltName = extn;
}

static void SecCEPIssuerAltName(SecCertificateRef certificate,
	const SecCertificateExtension *extn) {
	secdebug("cert", "critical: %s", extn->critical ? "yes" : "no");
}

static void SecCEPBasicConstraints(SecCertificateRef certificate,
	const SecCertificateExtension *extn) {
	secdebug("cert", "critical: %s", extn->critical ? "yes" : "no");
	DERBasicConstraints basicConstraints;
	require_noerr_quiet(DERParseSequence(&extn->extnValue,
        DERNumBasicConstraintsItemSpecs, DERBasicConstraintsItemSpecs,
        &basicConstraints, sizeof(basicConstraints)), badDER);
    require_noerr_quiet(DERParseBooleanWithDefault(&basicConstraints.cA, false,
		&certificate->_basicConstraints.isCA), badDER);
    if (basicConstraints.pathLenConstraint.length != 0) {
        require_noerr_quiet(DERParseInteger(
            &basicConstraints.pathLenConstraint,
            &certificate->_basicConstraints.pathLenConstraint), badDER);
		certificate->_basicConstraints.pathLenConstraintPresent = true;
	}
    certificate->_basicConstraints.present = true;
	certificate->_basicConstraints.critical = extn->critical;
    return;
badDER:
    certificate->_basicConstraints.present = false;
	secwarning("Invalid BasicConstraints Extension");
}


/*
 * id-ce-nameConstraints OBJECT IDENTIFIER ::=  { id-ce 30 }
 *
 * NameConstraints ::= SEQUENCE {
 * permittedSubtrees       [0]     GeneralSubtrees OPTIONAL,
 * excludedSubtrees        [1]     GeneralSubtrees OPTIONAL }
 *
 * GeneralSubtrees ::= SEQUENCE SIZE (1..MAX) OF GeneralSubtree
 *
 * GeneralSubtree ::= SEQUENCE {
 * base                    GeneralName,
 * minimum         [0]     BaseDistance DEFAULT 0,
 * maximum         [1]     BaseDistance OPTIONAL }
 *
 * BaseDistance ::= INTEGER (0..MAX)
 */
static DERReturn parseGeneralSubtrees(DERItem *derSubtrees, CFArrayRef *generalSubtrees) {
    CFMutableArrayRef gs = NULL;
    DERSequence gsSeq;
    DERReturn drtn = DERDecodeSeqContentInit(derSubtrees, &gsSeq);
    require_noerr_quiet(drtn, badDER);
    DERDecodedInfo gsContent;
    require_quiet(gs = CFArrayCreateMutable(kCFAllocatorDefault, 0,
                  &kCFTypeArrayCallBacks),
                  badDER);
    while ((drtn = DERDecodeSeqNext(&gsSeq, &gsContent)) == DR_Success) {
        DERGeneralSubtree derGS;
        require_quiet(gsContent.tag==ASN1_CONSTR_SEQUENCE, badDER);
        drtn = DERParseSequenceContent(&gsContent.content,
                                       DERNumGeneralSubtreeItemSpecs,
                                       DERGeneralSubtreeItemSpecs,
                                       &derGS, sizeof(derGS));
        require_noerr_quiet(drtn, badDER);
        /*
         * RFC 5280 4.2.1.10
         * Within this profile, the minimum and maximum fields are not used with
         * any name forms, thus, the minimum MUST be zero, and maximum MUST be
         * absent.
         *
         * Because minimum DEFAULT 0, absence equivalent to present and 0.
         */
        if (derGS.minimum.length) {
            uint32_t minimum;
            require_noerr_quiet(DERParseInteger(&derGS.minimum, &minimum),
                                badDER);
            require_quiet(minimum == 0, badDER);
        }
        require_quiet(derGS.maximum.length == 0, badDER);
        require_quiet(derGS.generalName.length != 0, badDER);

        CFDataRef generalName = NULL;
        require_quiet(generalName = CFDataCreate(kCFAllocatorDefault,
                                             derGS.generalName.data,
                                             derGS.generalName.length),
                                             badDER);
        CFArrayAppendValue(gs, generalName);
        CFReleaseNull(generalName);
    }
    require_quiet(drtn == DR_EndOfSequence, badDER);

    // since generalSubtrees is a pointer to an instance variable,
    // make sure we release the existing array before assignment.
    CFReleaseSafe(*generalSubtrees);
    *generalSubtrees = gs;

    return DR_Success;

badDER:
    CFReleaseNull(gs);
    secdebug("cert","failed to parse GeneralSubtrees");
    return drtn;
}

static void SecCEPNameConstraints(SecCertificateRef certificate,
    const SecCertificateExtension *extn) {
    secdebug("cert", "critical: %s", extn->critical ? "yes" : "no");
    DERNameConstraints nc;
    DERReturn drtn;
    drtn = DERParseSequence(&extn->extnValue,
                            DERNumNameConstraintsItemSpecs,
                            DERNameConstraintsItemSpecs,
                            &nc, sizeof(nc));
    require_noerr_quiet(drtn, badDER);
    if (nc.permittedSubtrees.length) {
        require_noerr_quiet(parseGeneralSubtrees(&nc.permittedSubtrees, &certificate->_permittedSubtrees), badDER);
    }
    if (nc.excludedSubtrees.length) {
        require_noerr_quiet(parseGeneralSubtrees(&nc.excludedSubtrees, &certificate->_excludedSubtrees), badDER);
    }

    return;
badDER:
    secdebug("cert", "failed to parse Name Constraints extension");
}

static OSStatus appendCRLDPFromGeneralNames(void *context, SecCEGeneralNameType type,
                                                   const DERItem *value) {
    CFMutableArrayRef *crlDPs = (CFMutableArrayRef *)context;
    if (type == GNT_URI) {
        CFURLRef url = NULL;
        url = CFURLCreateWithBytes(NULL, value->data, value->length, kCFStringEncodingASCII, NULL);
        if (url) {
            if (!*crlDPs) {
                *crlDPs = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
            }
            CFArrayAppendValue(*crlDPs, url);
            CFRelease(url);
        }
    }
    return errSecSuccess;
}

/*
 id-ce-cRLDistributionPoints OBJECT IDENTIFIER ::=  { id-ce 31 }

 CRLDistributionPoints ::= SEQUENCE SIZE (1..MAX) OF DistributionPoint

 DistributionPoint ::= SEQUENCE {
    distributionPoint       [0]     DistributionPointName OPTIONAL,
    reasons                 [1]     ReasonFlags OPTIONAL,
    cRLIssuer               [2]     GeneralNames OPTIONAL }

 DistributionPointName ::= CHOICE {
    fullName                [0]     GeneralNames,
    nameRelativeToCRLIssuer [1]     RelativeDistinguishedName }
 */
static void SecCEPCrlDistributionPoints(SecCertificateRef certificate,
	const SecCertificateExtension *extn) {
	secdebug("cert", "critical: %s", extn->critical ? "yes" : "no");
    DERSequence crlDPSeq;
    DERTag tag;
    DERReturn drtn = DERDecodeSeqInit(&extn->extnValue, &tag, &crlDPSeq);
    require_noerr_quiet(drtn, badDER);
    require_quiet(tag == ASN1_CONSTR_SEQUENCE, badDER);
    DERDecodedInfo dpContent;
    while ((drtn = DERDecodeSeqNext(&crlDPSeq, &dpContent)) == DR_Success) {
        require_quiet(dpContent.tag == ASN1_CONSTR_SEQUENCE, badDER);
        DERDistributionPoint dp;
        drtn = DERParseSequenceContent(&dpContent.content, DERNumDistributionPointItemSpecs,
                                       DERDistributionPointItemSpecs, &dp, sizeof(dp));
        require_noerr_quiet(drtn, badDER);
        require_quiet(dp.distributionPoint.data || dp.cRLIssuer.data, badDER);
        if (dp.distributionPoint.data) {
            DERDecodedInfo dpName;
            drtn = DERDecodeItem(&dp.distributionPoint, &dpName);
            require_noerr_quiet(drtn, badDER);
            switch (dpName.tag) {
                case ASN1_CONTEXT_SPECIFIC | ASN1_CONSTRUCTED | 0:
                    drtn = parseGeneralNamesContent(&dpName.content, &certificate->_crlDistributionPoints,
                                                    appendCRLDPFromGeneralNames);
                    require_noerr_quiet(drtn, badDER);
                    break;
                case ASN1_CONTEXT_SPECIFIC | ASN1_CONSTRUCTED | 1:
                    /* RelativeDistinguishName. Nothing we can do with that. */
                    break;
                default:
                    goto badDER;
            }
        }
        if (dp.cRLIssuer.data) {
            drtn = SecCertificateParseGeneralNames(&dp.cRLIssuer, &certificate->_crlDistributionPoints,
                                                   appendCRLDPFromGeneralNames);
            require_noerr_quiet(drtn, badDER);
        }
    }
    require_quiet(drtn == DR_EndOfSequence, badDER);
    return;
badDER:
    secdebug("cert", "failed to parse CRL Distribution Points extension");
}

/*
   certificatePolicies ::= SEQUENCE SIZE (1..MAX) OF PolicyInformation

   PolicyInformation ::= SEQUENCE {
        policyIdentifier   CertPolicyId,
        policyQualifiers   SEQUENCE SIZE (1..MAX) OF
                                PolicyQualifierInfo OPTIONAL }

   CertPolicyId ::= OBJECT IDENTIFIER

   PolicyQualifierInfo ::= SEQUENCE {
        policyQualifierId  PolicyQualifierId,
        qualifier          ANY DEFINED BY policyQualifierId }
*/
/* maximum number of policies of 8192 seems more than adequate */
#define MAX_CERTIFICATE_POLICIES 8192
static void SecCEPCertificatePolicies(SecCertificateRef certificate,
	const SecCertificateExtension *extn) {
	secdebug("cert", "critical: %s", extn->critical ? "yes" : "no");
    DERTag tag;
    DERSequence piSeq;
    SecCEPolicyInformation *policies = NULL;
    DERReturn drtn = DERDecodeSeqInit(&extn->extnValue, &tag, &piSeq);
    require_noerr_quiet(drtn, badDER);
    require_quiet(tag == ASN1_CONSTR_SEQUENCE, badDER);
    DERDecodedInfo piContent;
    DERSize policy_count = 0;
    while ((policy_count < MAX_CERTIFICATE_POLICIES) &&
           (drtn = DERDecodeSeqNext(&piSeq, &piContent)) == DR_Success) {
        require_quiet(piContent.tag == ASN1_CONSTR_SEQUENCE, badDER);
        policy_count++;
    }
    require_quiet(drtn == DR_EndOfSequence, badDER);
    policies = (SecCEPolicyInformation *)malloc(sizeof(SecCEPolicyInformation)
                                                * (policy_count > 0 ? policy_count : 1));
    drtn = DERDecodeSeqInit(&extn->extnValue, &tag, &piSeq);
    require_noerr_quiet(drtn, badDER);
    DERSize policy_ix = 0;
    while ((policy_ix < (policy_count > 0 ? policy_count : 1)) &&
           (drtn = DERDecodeSeqNext(&piSeq, &piContent)) == DR_Success) {
        DERPolicyInformation pi;
        drtn = DERParseSequenceContent(&piContent.content,
            DERNumPolicyInformationItemSpecs,
            DERPolicyInformationItemSpecs,
            &pi, sizeof(pi));
        require_noerr_quiet(drtn, badDER);
        policies[policy_ix].policyIdentifier = pi.policyIdentifier;
        policies[policy_ix++].policyQualifiers = pi.policyQualifiers;
    }
    certificate->_certificatePolicies.present = true;
    certificate->_certificatePolicies.critical = extn->critical;
    certificate->_certificatePolicies.numPolicies = policy_count;
    certificate->_certificatePolicies.policies = policies;
	return;
badDER:
    if (policies)
        free(policies);
    certificate->_certificatePolicies.present = false;
	secwarning("Invalid CertificatePolicies Extension");
}

/*
   id-ce-policyMappings OBJECT IDENTIFIER ::=  { id-ce 33 }

   PolicyMappings ::= SEQUENCE SIZE (1..MAX) OF SEQUENCE {
        issuerDomainPolicy      CertPolicyId,
        subjectDomainPolicy     CertPolicyId }
*/
#define MAX_POLICY_MAPPINGS 8192
static void SecCEPPolicyMappings(SecCertificateRef certificate,
	const SecCertificateExtension *extn) {
	secdebug("cert", "critical: %s", extn->critical ? "yes" : "no");
    DERTag tag;
    DERSequence pmSeq;
    SecCEPolicyMapping *mappings = NULL;
    DERReturn drtn = DERDecodeSeqInit(&extn->extnValue, &tag, &pmSeq);
    require_noerr_quiet(drtn, badDER);
    require_quiet(tag == ASN1_CONSTR_SEQUENCE, badDER);
    DERDecodedInfo pmContent;
    DERSize mapping_count = 0;
    while ((mapping_count < MAX_POLICY_MAPPINGS) &&
           (drtn = DERDecodeSeqNext(&pmSeq, &pmContent)) == DR_Success) {
        require_quiet(pmContent.tag == ASN1_CONSTR_SEQUENCE, badDER);
        mapping_count++;
    }
    require_quiet(drtn == DR_EndOfSequence, badDER);
    mappings = (SecCEPolicyMapping *)malloc(sizeof(SecCEPolicyMapping)
                                            * (mapping_count > 0 ? mapping_count : 1));
    drtn = DERDecodeSeqInit(&extn->extnValue, &tag, &pmSeq);
    require_noerr_quiet(drtn, badDER);
    DERSize mapping_ix = 0;
    while ((mapping_ix < (mapping_count > 0 ? mapping_count : 1)) &&
           (drtn = DERDecodeSeqNext(&pmSeq, &pmContent)) == DR_Success) {
        require_quiet(pmContent.tag == ASN1_CONSTR_SEQUENCE, badDER);
        DERPolicyMapping pm;
        drtn = DERParseSequenceContent(&pmContent.content,
            DERNumPolicyMappingItemSpecs,
            DERPolicyMappingItemSpecs,
            &pm, sizeof(pm));
        require_noerr_quiet(drtn, badDER);
        mappings[mapping_ix].issuerDomainPolicy = pm.issuerDomainPolicy;
        mappings[mapping_ix++].subjectDomainPolicy = pm.subjectDomainPolicy;
    }
    certificate->_policyMappings.present = true;
    certificate->_policyMappings.critical = extn->critical;
    certificate->_policyMappings.numMappings = mapping_count;
    certificate->_policyMappings.mappings = mappings;
	return;
badDER:
    if (mappings) {
        free(mappings);
    }
    certificate->_policyMappings.present = false;
	secwarning("Invalid CertificatePolicies Extension");
}

/*
AuthorityKeyIdentifier ::= SEQUENCE {
    keyIdentifier             [0] KeyIdentifier            OPTIONAL,
    authorityCertIssuer       [1] GeneralNames             OPTIONAL,
    authorityCertSerialNumber [2] CertificateSerialNumber  OPTIONAL }
    -- authorityCertIssuer and authorityCertSerialNumber MUST both
    -- be present or both be absent

KeyIdentifier ::= OCTET STRING
*/
static void SecCEPAuthorityKeyIdentifier(SecCertificateRef certificate,
	const SecCertificateExtension *extn) {
	secdebug("cert", "critical: %s", extn->critical ? "yes" : "no");
	DERAuthorityKeyIdentifier akid;
	DERReturn drtn;
	drtn = DERParseSequence(&extn->extnValue,
		DERNumAuthorityKeyIdentifierItemSpecs,
		DERAuthorityKeyIdentifierItemSpecs,
		&akid, sizeof(akid));
	require_noerr_quiet(drtn, badDER);
	if (akid.keyIdentifier.length) {
		certificate->_authorityKeyIdentifier = akid.keyIdentifier;
	}
	if (akid.authorityCertIssuer.length ||
		akid.authorityCertSerialNumber.length) {
		require_quiet(akid.authorityCertIssuer.length &&
			akid.authorityCertSerialNumber.length, badDER);
		/* Perhaps put in a subsection called Authority Certificate Issuer. */
		certificate->_authorityKeyIdentifierIssuer = akid.authorityCertIssuer;
		certificate->_authorityKeyIdentifierSerialNumber = akid.authorityCertSerialNumber;
	}

	return;
badDER:
	secwarning("Invalid AuthorityKeyIdentifier Extension");
}

static void SecCEPPolicyConstraints(SecCertificateRef certificate,
	const SecCertificateExtension *extn) {
	secdebug("cert", "critical: %s", extn->critical ? "yes" : "no");
	DERPolicyConstraints pc;
	DERReturn drtn;
	drtn = DERParseSequence(&extn->extnValue,
		DERNumPolicyConstraintsItemSpecs,
		DERPolicyConstraintsItemSpecs,
		&pc, sizeof(pc));
	require_noerr_quiet(drtn, badDER);
	if (pc.requireExplicitPolicy.length) {
        require_noerr_quiet(DERParseInteger(
            &pc.requireExplicitPolicy,
            &certificate->_policyConstraints.requireExplicitPolicy), badDER);
        certificate->_policyConstraints.requireExplicitPolicyPresent = true;
	}
	if (pc.inhibitPolicyMapping.length) {
        require_noerr_quiet(DERParseInteger(
            &pc.inhibitPolicyMapping,
            &certificate->_policyConstraints.inhibitPolicyMapping), badDER);
        certificate->_policyConstraints.inhibitPolicyMappingPresent = true;
	}

    certificate->_policyConstraints.present = true;
    certificate->_policyConstraints.critical = extn->critical;

    return;
badDER:
    certificate->_policyConstraints.present = false;
	secwarning("Invalid PolicyConstraints Extension");
}

static void SecCEPExtendedKeyUsage(SecCertificateRef certificate,
	const SecCertificateExtension *extn) {
	secdebug("cert", "critical: %s", extn->critical ? "yes" : "no");
}

/*
   InhibitAnyPolicy ::= SkipCerts

   SkipCerts ::= INTEGER (0..MAX)
*/
static void SecCEPInhibitAnyPolicy(SecCertificateRef certificate,
	const SecCertificateExtension *extn) {
	secdebug("cert", "critical: %s", extn->critical ? "yes" : "no");
    DERDecodedInfo iapContent;
    require_noerr_quiet(DERDecodeItem(&extn->extnValue, &iapContent), badDER);
    require_quiet(iapContent.tag == ASN1_INTEGER, badDER);
    require_noerr_quiet(DERParseInteger(
        &iapContent.content,
        &certificate->_inhibitAnyPolicySkipCerts.skipCerts), badDER);

    certificate->_inhibitAnyPolicySkipCerts.present = true;
    certificate->_inhibitAnyPolicySkipCerts.critical = extn->critical;
    return;
badDER:
    certificate->_inhibitAnyPolicySkipCerts.present = false;
	secwarning("Invalid InhibitAnyPolicy Extension");
}

/*
   id-pe-authorityInfoAccess OBJECT IDENTIFIER ::= { id-pe 1 }

   AuthorityInfoAccessSyntax  ::=
           SEQUENCE SIZE (1..MAX) OF AccessDescription

   AccessDescription  ::=  SEQUENCE {
           accessMethod          OBJECT IDENTIFIER,
           accessLocation        GeneralName  }

   id-ad OBJECT IDENTIFIER ::= { id-pkix 48 }

   id-ad-caIssuers OBJECT IDENTIFIER ::= { id-ad 2 }

   id-ad-ocsp OBJECT IDENTIFIER ::= { id-ad 1 }
 */
static void SecCEPAuthorityInfoAccess(SecCertificateRef certificate,
	const SecCertificateExtension *extn) {
	secdebug("cert", "critical: %s", extn->critical ? "yes" : "no");
    DERTag tag;
    DERSequence adSeq;
    DERReturn drtn = DERDecodeSeqInit(&extn->extnValue, &tag, &adSeq);
    require_noerr_quiet(drtn, badDER);
    require_quiet(tag == ASN1_CONSTR_SEQUENCE, badDER);
    DERDecodedInfo adContent;
    while ((drtn = DERDecodeSeqNext(&adSeq, &adContent)) == DR_Success) {
        require_quiet(adContent.tag == ASN1_CONSTR_SEQUENCE, badDER);
		DERAccessDescription ad;
		drtn = DERParseSequenceContent(&adContent.content,
			DERNumAccessDescriptionItemSpecs,
			DERAccessDescriptionItemSpecs,
			&ad, sizeof(ad));
		require_noerr_quiet(drtn, badDER);
        CFMutableArrayRef *urls;
        if (DEROidCompare(&ad.accessMethod, &oidAdOCSP))
            urls = &certificate->_ocspResponders;
        else if (DEROidCompare(&ad.accessMethod, &oidAdCAIssuer))
            urls = &certificate->_caIssuers;
        else
            continue;

        DERDecodedInfo generalNameContent;
        drtn = DERDecodeItem(&ad.accessLocation, &generalNameContent);
        require_noerr_quiet(drtn, badDER);
        switch (generalNameContent.tag) {
#if 0
        case ASN1_CONTEXT_SPECIFIC | ASN1_CONSTRUCTED | 6:
            /* Technically I don't think this is valid, but there are certs out
               in the wild that use a constructed IA5String.   In particular the
               VeriSign Time Stamping Authority CA.cer does this.  */
#endif
        case ASN1_CONTEXT_SPECIFIC | 6:
        {
            CFURLRef url = CFURLCreateWithBytes(kCFAllocatorDefault,
                generalNameContent.content.data, generalNameContent.content.length,
                kCFStringEncodingASCII, NULL);
            if (url) {
                if (!*urls)
                    *urls = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
                CFArrayAppendValue(*urls, url);
                CFRelease(url);
            }
            break;
        }
        default:
            secdebug("cert", "bad general name for id-ad-ocsp AccessDescription t: 0x%02llx v: %.*s",
                generalNameContent.tag, (int) generalNameContent.content.length, generalNameContent.content.data);
            goto badDER;
            break;
        }
    }
    require_quiet(drtn == DR_EndOfSequence, badDER);
	return;
badDER:
    secdebug("cert", "failed to parse Authority Information Access extension");
}

/* Apple Worldwide Developer Relations Certificate Authority subject name.
 * This is a DER sequence with the leading tag and length bytes removed,
 * to match what tbsCert.issuer contains.
 */
static const unsigned char Apple_WWDR_CA_Subject_Name[]={
                 0x31,0x0B,0x30,0x09,0x06,0x03,0x55,0x04,0x06,0x13,0x02,0x55,0x53,
  0x31,0x13,0x30,0x11,0x06,0x03,0x55,0x04,0x0A,0x0C,0x0A,0x41,0x70,0x70,0x6C,0x65,
  0x20,0x49,0x6E,0x63,0x2E,0x31,0x2C,0x30,0x2A,0x06,0x03,0x55,0x04,0x0B,0x0C,0x23,
  0x41,0x70,0x70,0x6C,0x65,0x20,0x57,0x6F,0x72,0x6C,0x64,0x77,0x69,0x64,0x65,0x20,
  0x44,0x65,0x76,0x65,0x6C,0x6F,0x70,0x65,0x72,0x20,0x52,0x65,0x6C,0x61,0x74,0x69,
  0x6F,0x6E,0x73,0x31,0x44,0x30,0x42,0x06,0x03,0x55,0x04,0x03,0x0C,0x3B,0x41,0x70,
  0x70,0x6C,0x65,0x20,0x57,0x6F,0x72,0x6C,0x64,0x77,0x69,0x64,0x65,0x20,0x44,0x65,
  0x76,0x65,0x6C,0x6F,0x70,0x65,0x72,0x20,0x52,0x65,0x6C,0x61,0x74,0x69,0x6F,0x6E,
  0x73,0x20,0x43,0x65,0x72,0x74,0x69,0x66,0x69,0x63,0x61,0x74,0x69,0x6F,0x6E,0x20,
  0x41,0x75,0x74,0x68,0x6F,0x72,0x69,0x74,0x79
};

static void checkForMissingRevocationInfo(SecCertificateRef certificate) {
	if (!certificate ||
		certificate->_crlDistributionPoints ||
		certificate->_ocspResponders) {
		/* We already have an OCSP or CRL URI (or no cert) */
		return;
	}
	/* Specify an appropriate OCSP responder if we recognize the issuer. */
	CFURLRef url = NULL;
	if (sizeof(Apple_WWDR_CA_Subject_Name) == certificate->_issuer.length &&
		!memcmp(certificate->_issuer.data, Apple_WWDR_CA_Subject_Name,
				sizeof(Apple_WWDR_CA_Subject_Name))) {
		const char *WWDR_OCSP_URI = "http://ocsp.apple.com/ocsp-wwdr01";
		url = CFURLCreateWithBytes(kCFAllocatorDefault,
				(const UInt8*)WWDR_OCSP_URI, strlen(WWDR_OCSP_URI),
				kCFStringEncodingASCII, NULL);
	}
	if (url) {
		CFMutableArrayRef *urls = &certificate->_ocspResponders;
		*urls = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
		CFArrayAppendValue(*urls, url);
		CFRelease(url);
	}
}

static void SecCEPSubjectInfoAccess(SecCertificateRef certificate,
	const SecCertificateExtension *extn) {
	secdebug("cert", "critical: %s", extn->critical ? "yes" : "no");
}

static void SecCEPNetscapeCertType(SecCertificateRef certificate,
	const SecCertificateExtension *extn) {
	secdebug("cert", "critical: %s", extn->critical ? "yes" : "no");
}

static void SecCEPEntrustVersInfo(SecCertificateRef certificate,
	const SecCertificateExtension *extn) {
	secdebug("cert", "critical: %s", extn->critical ? "yes" : "no");
}

static void SecCEPEscrowMarker(SecCertificateRef certificate,
                               const SecCertificateExtension *extn) {
	secdebug("cert", "critical: %s", extn->critical ? "yes" : "no");
}


/* Dictionary key callback for comparing to DERItems. */
static Boolean SecDERItemEqual(const void *value1, const void *value2) {
	return DEROidCompare((const DERItem *)value1, (const DERItem *)value2);
}

/* Dictionary key callback calculating the hash of a DERItem. */
static CFHashCode SecDERItemHash(const void *value) {
	const DERItem *derItem = (const DERItem *)value;
	CFHashCode hash = derItem->length;
	DERSize ix = derItem->length > 8 ? derItem->length - 8 : 0;
	for (; ix < derItem->length; ++ix) {
		hash = (hash << 9) + (hash >> 23) + derItem->data[ix];
	}

	return hash;
}

/* Dictionary key callbacks using the above 2 functions. */
static const CFDictionaryKeyCallBacks SecDERItemKeyCallBacks = {
	0,					/* version */
	NULL,				/* retain */
	NULL,				/* release */
	NULL,				/* copyDescription */
	SecDERItemEqual,	/* equal */
	SecDERItemHash		/* hash */
};

static void SecCertificateInitializeExtensionParsers(void) {
	/* Build a dictionary that maps from extension OIDs to callback functions
     which can parse the extension of the type given. */
	static const void *extnOIDs[] = {
		&oidSubjectKeyIdentifier,
		&oidKeyUsage,
		&oidPrivateKeyUsagePeriod,
		&oidSubjectAltName,
		&oidIssuerAltName,
		&oidBasicConstraints,
		&oidNameConstraints,
		&oidCrlDistributionPoints,
		&oidCertificatePolicies,
		&oidPolicyMappings,
		&oidAuthorityKeyIdentifier,
		&oidPolicyConstraints,
		&oidExtendedKeyUsage,
		&oidInhibitAnyPolicy,
		&oidAuthorityInfoAccess,
		&oidSubjectInfoAccess,
		&oidNetscapeCertType,
		&oidEntrustVersInfo,
        &oidApplePolicyEscrowService
	};
	static const void *extnParsers[] = {
		SecCEPSubjectKeyIdentifier,
		SecCEPKeyUsage,
		SecCEPPrivateKeyUsagePeriod,
		SecCEPSubjectAltName,
		SecCEPIssuerAltName,
		SecCEPBasicConstraints,
		SecCEPNameConstraints,
		SecCEPCrlDistributionPoints,
		SecCEPCertificatePolicies,
		SecCEPPolicyMappings,
		SecCEPAuthorityKeyIdentifier,
		SecCEPPolicyConstraints,
		SecCEPExtendedKeyUsage,
        SecCEPInhibitAnyPolicy,
		SecCEPAuthorityInfoAccess,
		SecCEPSubjectInfoAccess,
		SecCEPNetscapeCertType,
		SecCEPEntrustVersInfo,
        SecCEPEscrowMarker,
	};
	sExtensionParsers = CFDictionaryCreate(kCFAllocatorDefault, extnOIDs,
                                           extnParsers, array_size(extnOIDs),
                                           &SecDERItemKeyCallBacks, NULL);
}

CFGiblisWithFunctions(SecCertificate, NULL, NULL, SecCertificateDestroy, SecCertificateEqual, SecCertificateHash, NULL, SecCertificateCopyDescription, NULL, NULL, ^{
    SecCertificateInitializeExtensionParsers();
})

static bool isAppleExtensionOID(const DERItem *extnID)
{
	static const uint8_t appleExtension[8] = { 0x2a,0x86,0x48,0x86,0xf7,0x63,0x64,0x06 };
	return (extnID && extnID->data &&
			extnID->length > sizeof(appleExtension) &&
			!memcmp(extnID->data, appleExtension, sizeof(appleExtension)));
}

/* Given the contents of an X.501 Name return the contents of a normalized
   X.501 name. */
CFDataRef createNormalizedX501Name(CFAllocatorRef allocator,
	const DERItem *x501name) {
    CFMutableDataRef result = CFDataCreateMutable(allocator, x501name->length);
    CFIndex length = x501name->length;
    CFDataSetLength(result, length);
    UInt8 *base = CFDataGetMutableBytePtr(result);

	DERSequence rdnSeq;
	DERReturn drtn = DERDecodeSeqContentInit(x501name, &rdnSeq);

	require_noerr_quiet(drtn, badDER);
	DERDecodedInfo rdn;

    /* Always points to last rdn tag. */
    const DERByte *rdnTag = rdnSeq.nextItem;
    /* Offset relative to base of current rdn set tag. */
    CFIndex rdnTagLocation = 0;
	while ((drtn = DERDecodeSeqNext(&rdnSeq, &rdn)) == DR_Success) {
		require_quiet(rdn.tag == ASN1_CONSTR_SET, badDER);
		/* We don't allow empty RDNs. */
		require_quiet(rdn.content.length != 0, badDER);
        /* Length of the tag and length of the current rdn. */
        CFIndex rdnTLLength = rdn.content.data - rdnTag;
        CFIndex rdnContentLength = rdn.content.length;
        /* Copy the tag and length of the RDN. */
        memcpy(base + rdnTagLocation, rdnTag, rdnTLLength);

		DERSequence atvSeq;
		drtn = DERDecodeSeqContentInit(&rdn.content, &atvSeq);
        require_quiet(drtn == DR_Success, badDER);

        DERDecodedInfo atv;
        /* Always points to tag of current atv sequence. */
        const DERByte *atvTag = atvSeq.nextItem;
        /* Offset relative to base of current atv sequence tag. */
        CFIndex atvTagLocation = rdnTagLocation + rdnTLLength;
		while ((drtn = DERDecodeSeqNext(&atvSeq, &atv)) == DR_Success) {
			require_quiet(atv.tag == ASN1_CONSTR_SEQUENCE, badDER);
            /* Length of the tag and length of the current atv. */
            CFIndex atvTLLength = atv.content.data - atvTag;
            CFIndex atvContentLength = atv.content.length;
            /* Copy the tag and length of the atv and the atv itself. */
            memcpy(base + atvTagLocation, atvTag,
                atvTLLength + atv.content.length);

            /* Now decode the atv sequence. */
			DERAttributeTypeAndValue atvPair;
			drtn = DERParseSequenceContent(&atv.content,
				DERNumAttributeTypeAndValueItemSpecs,
				DERAttributeTypeAndValueItemSpecs,
				&atvPair, sizeof(atvPair));
			require_noerr_quiet(drtn, badDER);
			require_quiet(atvPair.type.length != 0, badDER);
            DERDecodedInfo value;
            drtn = DERDecodeItem(&atvPair.value, &value);
			require_noerr_quiet(drtn, badDER);

            /* (c) attribute values in PrintableString are not case sensitive
               (e.g., "Marianne Swanson" is the same as "MARIANNE SWANSON"); and

               (d) attribute values in PrintableString are compared after
               removing leading and trailing white space and converting internal
               substrings of one or more consecutive white space characters to a
               single space. */
            if (value.tag == ASN1_PRINTABLE_STRING) {
                /* Offset relative to base of current value tag. */
                CFIndex valueTagLocation = atvTagLocation + atvPair.value.data - atvTag;
                CFIndex valueTLLength = value.content.data - atvPair.value.data;
                CFIndex valueContentLength = value.content.length;

                /* Now copy all the bytes, but convert to upper case while
                   doing so and convert multiple whitespace chars into a
                   single space. */
                bool lastWasBlank = false;
                CFIndex valueLocation = valueTagLocation + valueTLLength;
                CFIndex valueCurrentLocation = valueLocation;
                CFIndex ix;
                for (ix = 0; ix < valueContentLength; ++ix) {
                    UInt8 ch = value.content.data[ix];
                    if (isblank(ch)) {
                        if (lastWasBlank) {
                            continue;
                        } else {
                            /* Don't insert a space for first character
                               we encounter. */
                            if (valueCurrentLocation > valueLocation) {
                                base[valueCurrentLocation++] = ' ';
                            }
                            lastWasBlank = true;
                        }
                    } else {
                        lastWasBlank = false;
                        if ('a' <= ch && ch <= 'z') {
                            base[valueCurrentLocation++] = ch + 'A' - 'a';
                        } else {
                            base[valueCurrentLocation++] = ch;
                        }
                    }
                }
                /* Finally if lastWasBlank remove the trailing space. */
                if (lastWasBlank && valueCurrentLocation > valueLocation) {
                    valueCurrentLocation--;
                }
                /* Adjust content length to normalized length. */
                valueContentLength = valueCurrentLocation - valueLocation;

                /* Number of bytes by which the length should be shorted. */
                CFIndex lengthDiff = value.content.length - valueContentLength;
                if (lengthDiff == 0) {
                    /* Easy case no need to adjust lengths. */
                } else {
                    /* Hard work we need to go back and fix up length fields
                       for:
                           1) The value itself.
                           2) The ATV Sequence containing type/value
                           3) The RDN Set containing one or more atv pairs.
                           4) The result.
                       */

                    /* Step 1 fix up length of value. */
                    /* Length of value tag and length minus the tag. */
                    DERSize newValueTLLength = valueTLLength - 1;
                    drtn = DEREncodeLength(valueContentLength,
                        base + valueTagLocation + 1, &newValueTLLength);
                    require(drtn == DR_Success, badDER);
                    /* Add the length of the tag back in. */
                    newValueTLLength++;
                    CFIndex valueLLDiff = valueTLLength - newValueTLLength;
                    if (valueLLDiff) {
                        /* The size of the length field changed, let's slide
                           the value back by valueLLDiff bytes. */
                        memmove(base + valueTagLocation + newValueTLLength,
                            base + valueTagLocation + valueTLLength,
                            valueContentLength);
                        /* The length diff for the enclosing object. */
                        lengthDiff += valueLLDiff;
                    }

                    /* Step 2 fix up length of the enclosing ATV Sequence. */
                    atvContentLength -= lengthDiff;
                    DERSize newATVTLLength = atvTLLength - 1;
                    drtn = DEREncodeLength(atvContentLength,
                        base + atvTagLocation + 1, &newATVTLLength);
                    require(drtn == DR_Success, badDER);
                    /* Add the length of the tag back in. */
                    newATVTLLength++;
                    CFIndex atvLLDiff = atvTLLength - newATVTLLength;
                    if (atvLLDiff) {
                        /* The size of the length field changed, let's slide
                           the value back by valueLLDiff bytes. */
                        memmove(base + atvTagLocation + newATVTLLength,
                            base + atvTagLocation + atvTLLength,
                            atvContentLength);
                        /* The length diff for the enclosing object. */
                        lengthDiff += atvLLDiff;
                        atvTLLength = newATVTLLength;
                    }

                    /* Step 3 fix up length of enclosing RDN Set. */
                    rdnContentLength -= lengthDiff;
                    DERSize newRDNTLLength = rdnTLLength - 1;
                    drtn = DEREncodeLength(rdnContentLength,
                        base + rdnTagLocation + 1, &newRDNTLLength);
                    require_quiet(drtn == DR_Success, badDER);
                    /* Add the length of the tag back in. */
                    newRDNTLLength++;
                    CFIndex rdnLLDiff = rdnTLLength - newRDNTLLength;
                    if (rdnLLDiff) {
                        /* The size of the length field changed, let's slide
                           the value back by valueLLDiff bytes. */
                        memmove(base + rdnTagLocation + newRDNTLLength,
                            base + rdnTagLocation + rdnTLLength,
                            rdnContentLength);
                        /* The length diff for the enclosing object. */
                        lengthDiff += rdnLLDiff;
                        rdnTLLength = newRDNTLLength;

                        /* Adjust the locations that might have changed due to
                           this slide. */
                        atvTagLocation -= rdnLLDiff;
                    }
                    (void) lengthDiff; // No next object, silence analyzer
                }
            }
            atvTagLocation += atvTLLength + atvContentLength;
            atvTag = atvSeq.nextItem;
		}
        rdnTagLocation += rdnTLLength + rdnContentLength;
        rdnTag = rdnSeq.nextItem;
	}
	require_quiet(drtn == DR_EndOfSequence, badDER);
    /* Truncate the result to the proper length. */
    CFDataSetLength(result, rdnTagLocation);

	return result;

badDER:
    CFRelease(result);
    return NULL;
}

CFDataRef SecDistinguishedNameCopyNormalizedContent(CFDataRef distinguished_name)
{
    const DERItem name = { (unsigned char *)CFDataGetBytePtr(distinguished_name), CFDataGetLength(distinguished_name) };
    DERDecodedInfo content;
    /* Decode top level sequence into DERItem */
    if (!DERDecodeItem(&name, &content) && (content.tag == ASN1_CONSTR_SEQUENCE))
        return createNormalizedX501Name(kCFAllocatorDefault, &content.content);
    return NULL;
}

/* AUDIT[securityd]:
   certificate->_der is a caller provided data of any length (might be 0).

   Top level certificate decode.
 */
static bool SecCertificateParse(SecCertificateRef certificate)
{
	DERReturn drtn;

    check(certificate);
    require_quiet(certificate, badCert);
    CFAllocatorRef allocator = CFGetAllocator(certificate);

	/* top level decode */
	DERSignedCertCrl signedCert;
	drtn = DERParseSequence(&certificate->_der, DERNumSignedCertCrlItemSpecs,
		DERSignedCertCrlItemSpecs, &signedCert,
		sizeof(signedCert));
	require_noerr_quiet(drtn, badCert);
	/* Store tbs since we need to digest it for verification later on. */
	certificate->_tbs = signedCert.tbs;

	/* decode the TBSCert - it was saved in full DER form */
    DERTBSCert tbsCert;
	drtn = DERParseSequence(&signedCert.tbs,
		DERNumTBSCertItemSpecs, DERTBSCertItemSpecs,
		&tbsCert, sizeof(tbsCert));
	require_noerr_quiet(drtn, badCert);

	/* sequence we're given: decode the signedCerts Signature Algorithm. */
	/* This MUST be the same as the certificate->_tbsSigAlg with the exception
	   of the params field. */
	drtn = DERParseSequenceContent(&signedCert.sigAlg,
		DERNumAlgorithmIdItemSpecs, DERAlgorithmIdItemSpecs,
		&certificate->_sigAlg, sizeof(certificate->_sigAlg));
	require_noerr_quiet(drtn, badCert);

	/* The contents of signedCert.sig is a bit string whose contents
	   are the signature itself. */
    DERByte numUnusedBits;
	drtn = DERParseBitString(&signedCert.sig,
        &certificate->_signature, &numUnusedBits);
	require_noerr_quiet(drtn, badCert);

    /* Now decode the tbsCert. */

    /* First we turn the optional version into an int. */
    if (tbsCert.version.length) {
        DERDecodedInfo decoded;
        drtn = DERDecodeItem(&tbsCert.version, &decoded);
        require_noerr_quiet(drtn, badCert);
        require_quiet(decoded.tag == ASN1_INTEGER, badCert);
        require_quiet(decoded.content.length == 1, badCert);
        certificate->_version = decoded.content.data[0];
        if (certificate->_version > 2) {
            secwarning("Invalid certificate version (%d), must be 0..2",
                certificate->_version);
        }
        require_quiet(certificate->_version > 0, badCert);
        require_quiet(certificate->_version < 3, badCert);
    } else {
        certificate->_version = 0;
    }

	/* The serial number is in the tbsCert.serialNum - it was saved in
       INTEGER form without the tag and length. */
	certificate->_serialNum = tbsCert.serialNum;
	certificate->_serialNumber = CFDataCreate(allocator,
		tbsCert.serialNum.data, tbsCert.serialNum.length);
    /* RFC5280 4.1.2.2 limits serial number values to 20 octets.
       %%% At some point, this should be treated as a hard error.*/
    if (tbsCert.serialNum.length < 1 || tbsCert.serialNum.length > 20) {
        secwarning("Invalid serial number length (%ld), must be 1..20",
            tbsCert.serialNum.length);
    }

	/* sequence we're given: decode the tbsCerts TBS Signature Algorithm. */
	drtn = DERParseSequenceContent(&tbsCert.tbsSigAlg,
		DERNumAlgorithmIdItemSpecs, DERAlgorithmIdItemSpecs,
		&certificate->_tbsSigAlg, sizeof(certificate->_tbsSigAlg));
	require_noerr_quiet(drtn, badCert);

	/* The issuer is in the tbsCert.issuer - it's a sequence without the tag
       and length fields. */
	certificate->_issuer = tbsCert.issuer;
    certificate->_normalizedIssuer = createNormalizedX501Name(allocator,
        &tbsCert.issuer);

	/* sequence we're given: decode the tbsCerts Validity sequence. */
    DERValidity validity;
	drtn = DERParseSequenceContent(&tbsCert.validity,
		DERNumValidityItemSpecs, DERValidityItemSpecs,
		&validity, sizeof(validity));
	require_noerr_quiet(drtn, badCert);
    require_quiet(derDateGetAbsoluteTime(&validity.notBefore,
        &certificate->_notBefore), badCert);
    require_quiet(derDateGetAbsoluteTime(&validity.notAfter,
        &certificate->_notAfter), badCert);

	/* The subject is in the tbsCert.subject - it's a sequence without the tag
       and length fields. */
	certificate->_subject = tbsCert.subject;
    certificate->_normalizedSubject = createNormalizedX501Name(allocator,
        &tbsCert.subject);

    /* Keep the SPKI around for CT */
    certificate->_subjectPublicKeyInfo = tbsCert.subjectPubKey;

	/* sequence we're given: encoded DERSubjPubKeyInfo - it was saved in full DER form */
	DERSubjPubKeyInfo pubKeyInfo;
	drtn = DERParseSequence(&tbsCert.subjectPubKey,
		DERNumSubjPubKeyInfoItemSpecs, DERSubjPubKeyInfoItemSpecs,
		&pubKeyInfo, sizeof(pubKeyInfo));
	require_noerr_quiet(drtn, badCert);

	/* sequence we're given: decode the pubKeyInfos DERAlgorithmId */
	drtn = DERParseSequenceContent(&pubKeyInfo.algId,
		DERNumAlgorithmIdItemSpecs, DERAlgorithmIdItemSpecs,
		&certificate->_algId, sizeof(certificate->_algId));
	require_noerr_quiet(drtn, badCert);

	/* Now we can figure out the key's algorithm id and params based on
	   certificate->_algId.oid. */

	/* The contents of pubKeyInfo.pubKey is a bit string whose contents
	   are a PKCS1 format RSA key. */
	drtn = DERParseBitString(&pubKeyInfo.pubKey,
        &certificate->_pubKeyDER, &numUnusedBits);
	require_noerr_quiet(drtn, badCert);

	/* The contents of tbsCert.issuerID is a bit string. */
	certificate->_issuerUniqueID = tbsCert.issuerID;

	/* The contents of tbsCert.subjectID is a bit string. */
	certificate->_subjectUniqueID = tbsCert.subjectID;

	/* Extensions. */
    if (tbsCert.extensions.length) {
        CFIndex extensionCount = 0;
        DERSequence derSeq;
        DERTag tag;
        drtn = DERDecodeSeqInit(&tbsCert.extensions, &tag,
            &derSeq);
        require_noerr_quiet(drtn, badCert);
        require_quiet(tag == ASN1_CONSTR_SEQUENCE, badCert);
        DERDecodedInfo currDecoded;
        while ((drtn = DERDecodeSeqNext(&derSeq, &currDecoded)) == DR_Success) {
#if 0
/* ! = MUST recognize ? = SHOULD recognize
*/

    KnownExtension      _subjectKeyID;          /* ?SubjectKeyIdentifier     id-ce 14 */
    KnownExtension      _keyUsage;              /* !KeyUsage                 id-ce 15 */
    KnownExtension      _subjectAltName;        /* !SubjectAltName           id-ce 17 */
    KnownExtension      _basicConstraints;      /* !BasicConstraints         id-ce 19 */
    KnownExtension      _authorityKeyID;        /* ?AuthorityKeyIdentifier   id-ce 35 */
    KnownExtension      _extKeyUsage;           /* !ExtKeyUsage              id-ce 37 */
    KnownExtension      _netscapeCertType;      /* 2.16.840.1.113730.1.1 netscape 1 1 */
    KnownExtension      _qualCertStatements;    /* QCStatements             id-pe 3 */

    KnownExtension      _issuerAltName;         /* IssuerAltName            id-ce 18 */
    KnownExtension      _nameConstraints;       /* !NameConstraints          id-ce 30 */
    KnownExtension      _cRLDistributionPoints; /* CRLDistributionPoints    id-ce 31 */
    KnownExtension      _certificatePolicies;   /* !CertificatePolicies      id-ce 32 */
    KnownExtension      _policyMappings;        /* ?PolicyMappings           id-ce 33 */
    KnownExtension      _policyConstraints;     /* !PolicyConstraints        id-ce 36 */
    KnownExtension      _freshestCRL;           /* FreshestCRL              id-ce 46 */
    KnownExtension      _inhibitAnyPolicy;      /* !InhibitAnyPolicy         id-ce 54 */

    KnownExtension      _authorityInfoAccess;   /* AuthorityInfoAccess      id-pe 1 */
    KnownExtension      _subjectInfoAccess;     /* SubjectInfoAccess        id-pe 11 */
#endif

            extensionCount++;
        }
        require_quiet(drtn == DR_EndOfSequence, badCert);

        /* Put some upper limit on the number of extensions allowed. */
        require_quiet(extensionCount < 10000, badCert);
        certificate->_extensionCount = extensionCount;
        certificate->_extensions =
        malloc(sizeof(SecCertificateExtension) * (extensionCount > 0 ? extensionCount : 1));

        CFIndex ix = 0;
        drtn = DERDecodeSeqInit(&tbsCert.extensions, &tag, &derSeq);
        require_noerr_quiet(drtn, badCert);
        for (ix = 0; ix < extensionCount; ++ix) {
            drtn = DERDecodeSeqNext(&derSeq, &currDecoded);
            require_quiet(drtn == DR_Success ||
                (ix == extensionCount - 1 && drtn == DR_EndOfSequence), badCert);
            require_quiet(currDecoded.tag == ASN1_CONSTR_SEQUENCE, badCert);
            DERExtension extn;
            drtn = DERParseSequenceContent(&currDecoded.content,
                DERNumExtensionItemSpecs, DERExtensionItemSpecs,
                &extn, sizeof(extn));
            require_noerr_quiet(drtn, badCert);
            /* Copy stuff into certificate->extensions[ix]. */
            certificate->_extensions[ix].extnID = extn.extnID;
            require_noerr_quiet(drtn = DERParseBooleanWithDefault(&extn.critical, false,
                &certificate->_extensions[ix].critical), badCert);
            certificate->_extensions[ix].extnValue = extn.extnValue;

			SecCertificateExtensionParser parser =
				(SecCertificateExtensionParser)CFDictionaryGetValue(
				sExtensionParsers, &certificate->_extensions[ix].extnID);
			if (parser) {
				/* Invoke the parser. */
				parser(certificate, &certificate->_extensions[ix]);
			} else if (certificate->_extensions[ix].critical) {
				if (isAppleExtensionOID(&extn.extnID)) {
					continue;
				}
				secdebug("cert", "Found unknown critical extension");
				certificate->_foundUnknownCriticalExtension = true;
			} else {
				secdebug("cert", "Found unknown non critical extension");
			}
		}
	}
	checkForMissingRevocationInfo(certificate);

	return true;

badCert:
	return false;
}


/* Public API functions. */
SecCertificateRef SecCertificateCreateWithBytes(CFAllocatorRef allocator,
	const UInt8 *der_bytes, CFIndex der_length) {
	if (der_bytes == NULL) return NULL;
    if (der_length == 0) return NULL;

    CFIndex size = sizeof(struct __SecCertificate) + der_length;
    SecCertificateRef result = (SecCertificateRef)_CFRuntimeCreateInstance(
		allocator, SecCertificateGetTypeID(), size - sizeof(CFRuntimeBase), 0);
	if (result) {
		memset((char*)result + sizeof(result->_base), 0,
			sizeof(*result) - sizeof(result->_base));
		result->_der.data = ((DERByte *)result + sizeof(*result));
		result->_der.length = der_length;
		memcpy(result->_der.data, der_bytes, der_length);
		if (!SecCertificateParse(result)) {
			CFRelease(result);
			return NULL;
		}
    }
    return result;
}

/* @@@ Placeholder until <rdar://problem/5701851> iap submits a binary is fixed. */
SecCertificateRef SecCertificateCreate(CFAllocatorRef allocator,
	const UInt8 *der_bytes, CFIndex der_length);

SecCertificateRef SecCertificateCreate(CFAllocatorRef allocator,
	const UInt8 *der_bytes, CFIndex der_length) {
    return SecCertificateCreateWithBytes(allocator, der_bytes, der_length);
}
/* @@@ End of placeholder. */

/* AUDIT[securityd](done):
   der_certificate is a caller provided data of any length (might be 0), only
   its cf type has been checked.
 */
SecCertificateRef SecCertificateCreateWithData(CFAllocatorRef allocator,
	CFDataRef der_certificate) {
	if (!der_certificate) {
		return NULL;
	}
	CFIndex size = sizeof(struct __SecCertificate);
	SecCertificateRef result = (SecCertificateRef)_CFRuntimeCreateInstance(
		allocator, SecCertificateGetTypeID(), size - sizeof(CFRuntimeBase), 0);
	if (result) {
		memset((char*)result + sizeof(result->_base), 0, size - sizeof(result->_base));
		result->_der_data = CFDataCreateCopy(allocator, der_certificate);
		result->_der.data = (DERByte *)CFDataGetBytePtr(result->_der_data);
		result->_der.length = CFDataGetLength(result->_der_data);
		if (!SecCertificateParse(result)) {
			CFRelease(result);
			return NULL;
		}
	}
	return result;
}

SecCertificateRef SecCertificateCreateWithKeychainItem(CFAllocatorRef allocator,
	CFDataRef der_certificate,
	CFTypeRef keychain_item)
{
	SecCertificateRef result = SecCertificateCreateWithData(allocator, der_certificate);
	if (result) {
		CFRetainSafe(keychain_item);
		result->_keychain_item = keychain_item;
	}
	return result;
}

OSStatus SecCertificateSetKeychainItem(SecCertificateRef certificate,
	CFTypeRef keychain_item)
{
	if (!certificate) {
		return errSecParam;
	}
	CFRetainSafe(keychain_item);
	CFReleaseSafe(certificate->_keychain_item);
	certificate->_keychain_item = keychain_item;
	return errSecSuccess;
}

CFDataRef SecCertificateCopyData(SecCertificateRef certificate) {
	check(certificate);
	CFDataRef result = NULL;
	if (!certificate) {
		return result;
	}
	if (certificate->_der_data) {
        CFRetain(certificate->_der_data);
        result = certificate->_der_data;
    } else {
		result = CFDataCreate(CFGetAllocator(certificate),
            certificate->_der.data, certificate->_der.length);
#if 0
		/* FIXME: If we wish to cache result we need to lock the certificate.
           Also this create 2 copies of the certificate data which is somewhat
           suboptimal. */
        CFRetain(result);
        certificate->_der_data = result;
#endif
	}

	return result;
}

CFIndex SecCertificateGetLength(SecCertificateRef certificate) {
	return certificate->_der.length;
}

const UInt8 *SecCertificateGetBytePtr(SecCertificateRef certificate) {
	return certificate->_der.data;
}

/* Used to recreate preCert from cert for Certificate Transparency */
CFDataRef SecCertificateCopyPrecertTBS(SecCertificateRef certificate)
{
    CFDataRef outData = NULL;
    DERItem tbsIn = certificate->_tbs;
    DERItem tbsOut = {0,};
    DERItem extensionsOut = {0,};
    DERItem *extensionsList = malloc(sizeof(DERItem)*certificate->_extensionCount); /* This maybe one too many */
    DERItemSpec *extensionsListSpecs = malloc(sizeof(DERItemSpec)*certificate->_extensionCount);
    DERTBSCert tbsCert;
    DERReturn drtn;

    /* decode the TBSCert - it was saved in full DER form */
    drtn = DERParseSequence(&tbsIn,
                            DERNumTBSCertItemSpecs, DERTBSCertItemSpecs,
                            &tbsCert, sizeof(tbsCert));
    require_noerr_quiet(drtn, out);

    /* Go over extensions and filter any SCT extension */
    CFIndex extensionsCount = 0;

    if (tbsCert.extensions.length) {
        DERSequence derSeq;
        DERTag tag;
        drtn = DERDecodeSeqInit(&tbsCert.extensions, &tag,
                                &derSeq);
        require_noerr_quiet(drtn, out);
        require_quiet(tag == ASN1_CONSTR_SEQUENCE, out);
        DERDecodedInfo currDecoded;
        while ((drtn = DERDecodeSeqNext(&derSeq, &currDecoded)) == DR_Success) {

            require_quiet(currDecoded.tag == ASN1_CONSTR_SEQUENCE, out);
            DERExtension extn;
            drtn = DERParseSequenceContent(&currDecoded.content,
                                           DERNumExtensionItemSpecs, DERExtensionItemSpecs,
                                           &extn, sizeof(extn));
            require_noerr_quiet(drtn, out);

            if (extn.extnID.length == oidGoogleEmbeddedSignedCertificateTimestamp.length &&
                !memcmp(extn.extnID.data, oidGoogleEmbeddedSignedCertificateTimestamp.data, extn.extnID.length))
                continue;

            extensionsList[extensionsCount] = currDecoded.content;
            extensionsListSpecs[extensionsCount].offset = sizeof(DERItem)*extensionsCount;
            extensionsListSpecs[extensionsCount].options = 0;
            extensionsListSpecs[extensionsCount].tag = ASN1_CONSTR_SEQUENCE;

            extensionsCount++;
        }

        require_quiet(drtn == DR_EndOfSequence, out);

    }

    /* Encode extensions */
    extensionsOut.length = DERLengthOfEncodedSequence(ASN1_CONSTR_SEQUENCE, extensionsList, extensionsCount, extensionsListSpecs);
    extensionsOut.data = malloc(extensionsOut.length);
    require_quiet(extensionsOut.data, out);
    drtn = DEREncodeSequence(ASN1_CONSTR_SEQUENCE, extensionsList, extensionsCount, extensionsListSpecs, extensionsOut.data, &extensionsOut.length);
    require_noerr_quiet(drtn, out);

    tbsCert.extensions = extensionsOut;

    tbsOut.length = DERLengthOfEncodedSequence(ASN1_CONSTR_SEQUENCE, &tbsCert, DERNumTBSCertItemSpecs, DERTBSCertItemSpecs);
    tbsOut.data = malloc(tbsOut.length);
    require_quiet(tbsOut.data, out);
    drtn = DEREncodeSequence(ASN1_CONSTR_SEQUENCE, &tbsCert, DERNumTBSCertItemSpecs, DERTBSCertItemSpecs, tbsOut.data, &tbsOut.length);
    require_noerr_quiet(drtn, out);

    outData = CFDataCreate(kCFAllocatorDefault, tbsOut.data, tbsOut.length);

out:
    free(extensionsOut.data);
    free(tbsOut.data);
    free(extensionsList);
    free(extensionsListSpecs);
    return outData;

}

/* From rfc3280 - Appendix B.  ASN.1 Notes

   Object Identifiers (OIDs) are used throughout this specification to
   identify certificate policies, public key and signature algorithms,
   certificate extensions, etc.  There is no maximum size for OIDs.
   This specification mandates support for OIDs which have arc elements
   with values that are less than 2^28, that is, they MUST be between 0
   and 268,435,455, inclusive.  This allows each arc element to be
   represented within a single 32 bit word.  Implementations MUST also
   support OIDs where the length of the dotted decimal (see [RFC 2252],
   section 4.1) string representation can be up to 100 bytes
   (inclusive).  Implementations MUST be able to handle OIDs with up to
   20 elements (inclusive).  CAs SHOULD NOT issue certificates which
   contain OIDs that exceed these requirements.  Likewise, CRL issuers
   SHOULD NOT issue CRLs which contain OIDs that exceed these
   requirements.
*/

/* Oids longer than this are considered invalid. */
#define MAX_OID_SIZE				32

CFStringRef SecDERItemCopyOIDDecimalRepresentation(CFAllocatorRef allocator,
    const DERItem *oid) {

	if (oid->length == 0) {
        return SecCopyCertString(SEC_NULL_KEY);
    }
	if (oid->length > MAX_OID_SIZE) {
        return SecCopyCertString(SEC_OID_TOO_LONG_KEY);
    }

    CFMutableStringRef result = CFStringCreateMutable(allocator, 0);

	// The first two levels are encoded into one byte, since the root level
	// has only 3 nodes (40*x + y).  However if x = joint-iso-itu-t(2) then
	// y may be > 39, so we have to add special-case handling for this.
	uint32_t x = oid->data[0] / 40;
	uint32_t y = oid->data[0] % 40;
	if (x > 2)
	{
		// Handle special case for large y if x = 2
		y += (x - 2) * 40;
		x = 2;
	}
    CFStringAppendFormat(result, NULL, CFSTR("%u.%u"), x, y);

	uint32_t value = 0;
	for (x = 1; x < oid->length; ++x)
	{
		value = (value << 7) | (oid->data[x] & 0x7F);
        /* @@@ value may not span more than 4 bytes. */
        /* A max number of 20 values is allowed. */
		if (!(oid->data[x] & 0x80))
		{
            CFStringAppendFormat(result, NULL, CFSTR(".%" PRIu32), value);
			value = 0;
		}
	}
	return result;
}

static CFStringRef copyLocalizedOidDescription(CFAllocatorRef allocator,
    const DERItem *oid) {
	if (oid->length == 0) {
        return SecCopyCertString(SEC_NULL_KEY);
    }

    /* Build the key we use to lookup the localized OID description. */
    CFMutableStringRef oidKey = CFStringCreateMutable(allocator,
        oid->length * 3 + 5);
    CFStringAppendFormat(oidKey, NULL, CFSTR("06 %02lX"), oid->length);
    DERSize ix;
    for (ix = 0; ix < oid->length; ++ix)
        CFStringAppendFormat(oidKey, NULL, CFSTR(" %02X"), oid->data[ix]);

    CFStringRef name = SecFrameworkCopyLocalizedString(oidKey, CFSTR("OID"));
    if (CFEqual(oidKey, name)) {
        CFRelease(name);
        name = SecDERItemCopyOIDDecimalRepresentation(allocator, oid);
    }
    CFRelease(oidKey);

    return name;
}

/* Return the ipAddress as a dotted quad for ipv4 or as 8 colon separated
   4 digit hex strings for ipv6.  Return NULL if the passed in IP doesn't
   have a length of exactly 4 or 16 octects.  */
static CFStringRef copyIPAddressContentDescription(CFAllocatorRef allocator,
	const DERItem *ip) {
	/* @@@ This is the IP Address as an OCTECT STRING. For IPv4 it's
	   4 octects addr, or 8 octects, addr/mask for ipv6 it's
	   16 octects addr, or 32 octects addr/mask.  */
	CFStringRef value = NULL;
	if (ip->length == 4) {
		value = CFStringCreateWithFormat(allocator, NULL,
			CFSTR("%u.%u.%u.%u"),
			ip->data[0], ip->data[1], ip->data[2], ip->data[3]);
	} else if (ip->length == 16) {
		value = CFStringCreateWithFormat(allocator, NULL,
			CFSTR("%02x%02x:%02x%02x:%02x%02x:%02x%02x:"
			"%02x%02x:%02x%02x:%02x%02x:%02x%02x"),
			ip->data[0], ip->data[1], ip->data[2], ip->data[3],
			ip->data[4], ip->data[5], ip->data[6], ip->data[7],
			ip->data[8], ip->data[9], ip->data[10], ip->data[11],
			ip->data[12], ip->data[13], ip->data[14], ip->data[15]);
	}

	return value;
}

void appendProperty(CFMutableArrayRef properties, CFStringRef propertyType,
    CFStringRef label, CFStringRef localizedLabel, CFTypeRef value) {
    CFDictionaryRef property;
    if (label) {
        CFStringRef ll;
        if (localizedLabel) {
            ll = NULL;
        } else {
            ll = localizedLabel = SecCopyCertString(label);
        }
        const void *all_keys[4];
        all_keys[0] = kSecPropertyKeyType;
        all_keys[1] = kSecPropertyKeyLabel;
        all_keys[2] = kSecPropertyKeyLocalizedLabel;
        all_keys[3] = kSecPropertyKeyValue;
        const void *property_values[] = {
            propertyType,
            label,
            localizedLabel,
            value,
        };
        property = CFDictionaryCreate(CFGetAllocator(properties),
            all_keys, property_values, value ? 4 : 3,
            &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        CFReleaseSafe(ll);
    } else {
        const void *nolabel_keys[2];
        nolabel_keys[0] = kSecPropertyKeyType;
        nolabel_keys[1] = kSecPropertyKeyValue;
        const void *property_values[] = {
            propertyType,
            value,
        };
        property = CFDictionaryCreate(CFGetAllocator(properties),
            nolabel_keys, property_values, 2,
            &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    }

    CFArrayAppendValue(properties, property);
    CFRelease(property);
}

/* YYMMDDhhmmZ */
#define UTC_TIME_NOSEC_ZULU_LEN			11
/* YYMMDDhhmmssZ */
#define UTC_TIME_ZULU_LEN				13
/* YYMMDDhhmmssThhmm */
#define UTC_TIME_LOCALIZED_LEN			17
/* YYYYMMDDhhmmssZ */
#define GENERALIZED_TIME_ZULU_LEN		15
/* YYYYMMDDhhmmssThhmm */
#define GENERALIZED_TIME_LOCALIZED_LEN	19

/* Parse 2 digits at (*p)[0] and (*p)[1] and return the result.  Also
   advance *p by 2. */
static inline int parseDecimalPair(const DERByte **p) {
    const DERByte *cp = *p;
    *p += 2;
    return 10 * (cp[0] - '0') + cp[1] - '0';
}

/* Decode a choice of UTCTime or GeneralizedTime to a CFAbsoluteTime.
   Return a CFErrorRef in the error parameter if decoding fails.
   Note that this is needed to distinguish an error condition from a
   valid time which specifies 2001-01-01 00:00:00 (i.e. a value of 0).
*/
static CFAbsoluteTime SecAbsoluteTimeFromDateContentWithError(DERTag tag,
	const uint8_t *bytes,
	size_t length,
	CFErrorRef *error) {
	if (error) {
		*error = NULL;
	}
	if (NULL == bytes || 0 == length) {
		goto decodeErr;
	}

	bool isUtcLength = false;
	bool isLocalized = false;
	bool noSeconds = false;
	switch (length) {
		case UTC_TIME_NOSEC_ZULU_LEN:       /* YYMMDDhhmmZ */
			isUtcLength = true;
			noSeconds = true;
			break;
		case UTC_TIME_ZULU_LEN:             /* YYMMDDhhmmssZ */
			isUtcLength = true;
			break;
		case GENERALIZED_TIME_ZULU_LEN:     /* YYYYMMDDhhmmssZ */
			break;
		case UTC_TIME_LOCALIZED_LEN:        /* YYMMDDhhmmssThhmm (where T=[+,-]) */
			isUtcLength = true;
			/*DROPTHROUGH*/
		case GENERALIZED_TIME_LOCALIZED_LEN:/* YYYYMMDDhhmmssThhmm (where T=[+,-]) */
			isLocalized = true;
			break;
		default:                            /* unknown format */
			goto decodeErr;
	}

	/* Make sure the der tag fits the thing inside it. */
	if (tag == ASN1_UTC_TIME) {
		if (!isUtcLength) {
			goto decodeErr;
		}
	} else if (tag == ASN1_GENERALIZED_TIME) {
		if (isUtcLength) {
			goto decodeErr;
		}
	} else {
		goto decodeErr;
	}

	const DERByte *cp = bytes;
	/* Check that all characters are digits, except if localized the timezone
	   indicator or if not localized the 'Z' at the end.  */
	DERSize ix;
	for (ix = 0; ix < length; ++ix) {
		if (!(isdigit(cp[ix]))) {
			if ((isLocalized && ix == length - 5 &&
				 (cp[ix] == '+' || cp[ix] == '-')) ||
				(!isLocalized && ix == length - 1 && cp[ix] == 'Z')) {
				continue;
			}
			goto decodeErr;
		}
	}

	/* Parse the date and time fields. */
	int year, month, day, hour, minute, second;
	if (isUtcLength) {
		year = parseDecimalPair(&cp);
		if (year < 50) {
			/* 0  <= year <  50 : assume century 21 */
			year += 2000;
		} else if (year < 70) {
			/* 50 <= year <  70 : illegal per PKIX */
			return false;
		} else {
			/* 70 <  year <= 99 : assume century 20 */
			year += 1900;
		}
	} else {
		year = 100 * parseDecimalPair(&cp) + parseDecimalPair(&cp);
	}
	month = parseDecimalPair(&cp);
	day = parseDecimalPair(&cp);
	hour = parseDecimalPair(&cp);
	minute = parseDecimalPair(&cp);
	if (noSeconds) {
		second = 0;
	} else {
		second = parseDecimalPair(&cp);
	}

	CFTimeInterval timeZoneOffset;
	if (isLocalized) {
		/* ZONE INDICATOR */
        int multiplier = *cp++ == '+' ? 60 : -60;
        timeZoneOffset = multiplier *
            (parseDecimalPair(&cp) * 60 + parseDecimalPair(&cp));
	} else {
		timeZoneOffset = 0;
	}

    secdebug("dateparse",
        "date %.*s year: %04d%02d%02d%02d%02d%02d%+05g",
        (int) length, bytes, year, month,
        day, hour, minute, second,
        timeZoneOffset / 60);

    static int mdays[13] = { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365 };
    int is_leap_year = year % 4 == 0 && (year % 100 != 0 || year % 400 == 0) ? 1 : 0;
    if (month < 1 || month > 12 || day < 1 || day > 31 || hour > 23 || minute > 59 || second > 59
        || (month == 2 && day > mdays[month] - mdays[month - 1] + is_leap_year)
        || (month != 2 && day > mdays[month] - mdays[month - 1])) {
        /* Invalid date. */
        goto decodeErr;
    }

    int dy = year - 2001;
    if (dy < 0) {
        dy += 1;
        day -= 1;
    }
    int leap_days = dy / 4 - dy / 100 + dy / 400;
    day += ((year - 2001) * 365 + leap_days) + mdays[month - 1] - 1;
    if (month > 2)
        day += is_leap_year;

    CFAbsoluteTime absTime = (CFAbsoluteTime)((day * 24.0 + hour) * 60.0 + minute) * 60.0 + second;
    return absTime - timeZoneOffset;

decodeErr:
    if (error) {
        *error = CFErrorCreate(NULL, kCFErrorDomainOSStatus, errSecInvalidCertificate, NULL);
    }
    return NULL_TIME;
}

CFAbsoluteTime SecAbsoluteTimeFromDateContent(DERTag tag, const uint8_t *bytes,
    size_t length) {
    return SecAbsoluteTimeFromDateContentWithError(tag, bytes, length, NULL);
}

__attribute__((__nonnull__)) static bool derDateContentGetAbsoluteTime(DERTag tag, const DERItem *date,
    CFAbsoluteTime *pabsTime)  {
    CFErrorRef error = NULL;
    CFAbsoluteTime absTime = SecAbsoluteTimeFromDateContentWithError(tag, date->data,
        date->length, &error);
    if (error) {
        secwarning("Invalid date specification in certificate (see RFC5280 4.1.2.5)");
        CFRelease(error);
        return false;
    }

    *pabsTime = absTime;
    return true;
}

/* Decode a choice of UTCTime or GeneralizedTime to a CFAbsoluteTime. Return
   true if the date was valid and properly decoded, also return the result in
   absTime.  Return false otherwise. */
__attribute__((__nonnull__)) static bool derDateGetAbsoluteTime(const DERItem *dateChoice,
	CFAbsoluteTime *absTime) {
	if (dateChoice->length == 0) return false;

	DERDecodedInfo decoded;
	if (DERDecodeItem(dateChoice, &decoded))
		return false;

    return derDateContentGetAbsoluteTime(decoded.tag, &decoded.content,
        absTime);
}

static void appendDataProperty(CFMutableArrayRef properties,
    CFStringRef label, CFStringRef localizedLabel, const DERItem *der_data) {
    CFDataRef data = CFDataCreate(CFGetAllocator(properties),
        der_data->data, der_data->length);
    appendProperty(properties, kSecPropertyTypeData, label, localizedLabel,
                   data);
    CFRelease(data);
}

static void appendRelabeledProperty(CFMutableArrayRef properties,
                                    CFStringRef label,
                                    CFStringRef localizedLabel,
                                    const DERItem *der_data,
                                    CFStringRef labelFormat) {
    CFStringRef newLabel =
        CFStringCreateWithFormat(CFGetAllocator(properties), NULL,
                                 labelFormat, label);
    CFStringRef ll;
    if (localizedLabel) {
        ll = NULL;
    } else {
        ll = localizedLabel = SecCopyCertString(label);
    }
    CFStringRef localizedLabelFormat = SecCopyCertString(labelFormat);
    CFStringRef newLocalizedLabel =
        CFStringCreateWithFormat(CFGetAllocator(properties), NULL,
                                 localizedLabelFormat, localizedLabel);
    CFReleaseSafe(ll);
    CFReleaseSafe(localizedLabelFormat);
    appendDataProperty(properties, newLabel, newLocalizedLabel, der_data);
    CFReleaseSafe(newLabel);
    CFReleaseSafe(newLocalizedLabel);
}


static void appendUnparsedProperty(CFMutableArrayRef properties,
    CFStringRef label, CFStringRef localizedLabel, const DERItem *der_data) {
    appendRelabeledProperty(properties, label, localizedLabel, der_data,
                            SEC_UNPARSED_KEY);
}

static void appendInvalidProperty(CFMutableArrayRef properties,
    CFStringRef label, const DERItem *der_data) {
    appendRelabeledProperty(properties, label, NULL, der_data, SEC_INVALID_KEY);
}

static void appendDateContentProperty(CFMutableArrayRef properties,
    CFStringRef label, DERTag tag,
    const DERItem *dateContent) {
	CFAbsoluteTime absTime;
	if (!derDateContentGetAbsoluteTime(tag, dateContent, &absTime)) {
		/* Date decode failure insert hex bytes instead. */
		return appendInvalidProperty(properties, label, dateContent);
	}
    CFDateRef date = CFDateCreate(CFGetAllocator(properties), absTime);
    appendProperty(properties, kSecPropertyTypeDate, label, NULL, date);
    CFRelease(date);
}

static void appendDateProperty(CFMutableArrayRef properties,
    CFStringRef label, CFAbsoluteTime absTime) {
    CFDateRef date = CFDateCreate(CFGetAllocator(properties), absTime);
    appendProperty(properties, kSecPropertyTypeDate, label, NULL, date);
    CFRelease(date);
}

static void appendValidityPeriodProperty(CFMutableArrayRef parent, CFStringRef label,
                                         SecCertificateRef certificate) {
    CFAllocatorRef allocator = CFGetAllocator(parent);
    CFMutableArrayRef properties = CFArrayCreateMutable(allocator, 0, &kCFTypeArrayCallBacks);

    appendDateProperty(properties, SEC_NOT_VALID_BEFORE_KEY,
                       certificate->_notBefore);
    appendDateProperty(properties, SEC_NOT_VALID_AFTER_KEY,
                       certificate->_notAfter);

    appendProperty(parent, kSecPropertyTypeSection, label, NULL, properties);
    CFReleaseNull(properties);
}

static void appendIPAddressContentProperty(CFMutableArrayRef properties,
    CFStringRef label, const DERItem *ip) {
	CFStringRef value =
		copyIPAddressContentDescription(CFGetAllocator(properties), ip);
	if (value) {
        appendProperty(properties, kSecPropertyTypeString, label, NULL, value);
		CFRelease(value);
	} else {
		appendUnparsedProperty(properties, label, NULL, ip);
	}
}

static void appendURLContentProperty(CFMutableArrayRef properties,
    CFStringRef label, const DERItem *urlContent) {
    CFURLRef url = CFURLCreateWithBytes(CFGetAllocator(properties),
        urlContent->data, urlContent->length, kCFStringEncodingASCII, NULL);
    if (url) {
        appendProperty(properties, kSecPropertyTypeURL, label, NULL, url);
        CFRelease(url);
    } else {
		appendInvalidProperty(properties, label, urlContent);
    }
}

static void appendURLProperty(CFMutableArrayRef properties,
    CFStringRef label, const DERItem *url) {
	DERDecodedInfo decoded;
	DERReturn drtn;

	drtn = DERDecodeItem(url, &decoded);
    if (drtn || decoded.tag != ASN1_IA5_STRING) {
		appendInvalidProperty(properties, label, url);
    } else {
        appendURLContentProperty(properties, label, &decoded.content);
    }
}

static void appendOIDProperty(CFMutableArrayRef properties,
    CFStringRef label, CFStringRef llabel, const DERItem *oid) {
    CFStringRef oid_string =
        copyLocalizedOidDescription(CFGetAllocator(properties), oid);
    appendProperty(properties, kSecPropertyTypeString, label, llabel,
                   oid_string);
    CFRelease(oid_string);
}

static void appendAlgorithmProperty(CFMutableArrayRef properties,
    CFStringRef label, const DERAlgorithmId *algorithm) {
    CFMutableArrayRef alg_props =
        CFArrayCreateMutable(CFGetAllocator(properties), 0,
            &kCFTypeArrayCallBacks);
    appendOIDProperty(alg_props, SEC_ALGORITHM_KEY, NULL, &algorithm->oid);
    if (algorithm->params.length) {
        if (algorithm->params.length == 2 &&
            algorithm->params.data[0] == ASN1_NULL &&
            algorithm->params.data[1] == 0) {
            CFStringRef value = SecCopyCertString(SEC_NONE_KEY);
            appendProperty(alg_props, kSecPropertyTypeString,
                           SEC_PARAMETERS_KEY, NULL, value);
            CFRelease(value);
        } else {
            appendUnparsedProperty(alg_props, SEC_PARAMETERS_KEY, NULL,
                                   &algorithm->params);
        }
    }
    appendProperty(properties, kSecPropertyTypeSection, label, NULL, alg_props);
    CFRelease(alg_props);
}

static void appendPublicKeyProperty(CFMutableArrayRef parent, CFStringRef label,
                                    SecCertificateRef certificate) {
    CFAllocatorRef allocator = CFGetAllocator(parent);
    CFMutableArrayRef properties = CFArrayCreateMutable(allocator, 0, &kCFTypeArrayCallBacks);

    /* Public key algorithm. */
    appendAlgorithmProperty(properties, SEC_PUBLIC_KEY_ALG_KEY,
                            &certificate->_algId);

    /* Public Key Size */
#if TARGET_OS_IPHONE
    SecKeyRef publicKey = SecCertificateCopyPublicKey(certificate);
#else
    SecKeyRef publicKey = SecCertificateCopyPublicKey_ios(certificate);
#endif
    if (publicKey) {
        size_t sizeInBytes = SecKeyGetBlockSize(publicKey);
        CFStringRef sizeInBitsString = CFStringCreateWithFormat(allocator, NULL,
                                                                CFSTR("%ld"), (sizeInBytes*8));
        if (sizeInBitsString) {
            appendProperty(properties, kSecPropertyTypeString, SEC_PUBLIC_KEY_SIZE_KEY,
                           NULL, sizeInBitsString);
        }
        CFReleaseNull(sizeInBitsString);
    }
    CFReleaseNull(publicKey);

    /* Consider breaking down an RSA public key into modulus and
     exponent? */
    appendDataProperty(properties, SEC_PUBLIC_KEY_DATA_KEY, NULL,
                       &certificate->_pubKeyDER);

    appendProperty(parent, kSecPropertyTypeSection, label, NULL, properties);
    CFReleaseNull(properties);
}

static void appendSignatureProperty(CFMutableArrayRef parent, CFStringRef label,
                                    SecCertificateRef certificate) {
    CFAllocatorRef allocator = CFGetAllocator(parent);
    CFMutableArrayRef properties = CFArrayCreateMutable(allocator, 0, &kCFTypeArrayCallBacks);

    appendAlgorithmProperty(properties, SEC_SIGNATURE_ALGORITHM_KEY,
                            &certificate->_tbsSigAlg);

    appendDataProperty(properties, SEC_SIGNATURE_DATA_KEY, NULL,
                       &certificate->_signature);

    appendProperty(parent, kSecPropertyTypeSection, label, NULL, properties);
    CFReleaseNull(properties);
}

static void appendFingerprintsProperty(CFMutableArrayRef parent, CFStringRef label, SecCertificateRef certificate) {
    CFAllocatorRef allocator = CFGetAllocator(parent);
    CFMutableArrayRef properties = CFArrayCreateMutable(allocator, 0, &kCFTypeArrayCallBacks);

    CFDataRef sha256Fingerprint = SecCertificateCopySHA256Digest(certificate);
    if (sha256Fingerprint) {
        appendProperty(properties, kSecPropertyTypeData, SEC_SHA2_FINGERPRINT_KEY,
                       NULL, sha256Fingerprint);
    }
    CFReleaseNull(sha256Fingerprint);

    appendProperty(properties, kSecPropertyTypeData, SEC_SHA1_FINGERPRINT_KEY,
                   NULL, SecCertificateGetSHA1Digest(certificate));

    appendProperty(parent, kSecPropertyTypeSection, label, NULL, properties);
    CFReleaseNull(properties);
}

static CFStringRef copyHexDescription(CFAllocatorRef allocator,
    const DERItem *blob) {
    CFIndex ix, length = blob->length /* < 24 ? blob->length : 24 */;
    CFMutableStringRef string = CFStringCreateMutable(allocator,
        blob->length * 3 - 1);
    for (ix = 0; ix < length; ++ix)
        if (ix == 0)
            CFStringAppendFormat(string, NULL, CFSTR("%02X"), blob->data[ix]);
        else
            CFStringAppendFormat(string, NULL, CFSTR(" %02X"), blob->data[ix]);

    return string;
}

/* Returns a (localized) blob string. */
static CFStringRef copyBlobString(CFAllocatorRef allocator,
    CFStringRef blobType, CFStringRef quanta, const DERItem *blob) {
    CFStringRef localizedBlobType = SecCopyCertString(blobType);
    CFStringRef localizedQuanta = SecCopyCertString(quanta);
    /*  "format string for encoded field data (e.g. Sequence; 128 bytes; "
        "data = 00 00 ...)" */
    CFStringRef blobFormat = SecCopyCertString(SEC_BLOB_KEY);
    CFStringRef hex = copyHexDescription(allocator, blob);
    CFStringRef result = CFStringCreateWithFormat(allocator, NULL,
        blobFormat, localizedBlobType, blob->length, localizedQuanta, hex);
    CFRelease(hex);
    CFRelease(blobFormat);
    CFReleaseSafe(localizedQuanta);
    CFReleaseSafe(localizedBlobType);

    return result;
}

/* Return a string verbatim (unlocalized) from a DER field. */
static CFStringRef copyContentString(CFAllocatorRef allocator,
	const DERItem *string, CFStringEncoding encoding,
    bool printableOnly) {
    /* Strip potential bogus trailing zero from printable strings. */
    DERSize length = string->length;
    if (length && string->data[length - 1] == 0) {
        /* Don't mess with the length of UTF16 strings though. */
        if (encoding != kCFStringEncodingUTF16)
            length--;
    }
    /* A zero length string isn't considered printable. */
    if (!length && printableOnly)
        return NULL;

    /* Passing true for the 5th paramater to CFStringCreateWithBytes() makes
       it treat kCFStringEncodingUTF16 as big endian by default, whereas
       passing false makes it treat it as native endian by default.  */
    CFStringRef result = CFStringCreateWithBytes(allocator, string->data,
        length, encoding, encoding == kCFStringEncodingUTF16);
    if (result)
        return result;

    return printableOnly ? NULL : copyHexDescription(allocator, string);
}

/* From rfc3280 - Appendix B.  ASN.1 Notes

   CAs MUST force the serialNumber to be a non-negative integer, that
   is, the sign bit in the DER encoding of the INTEGER value MUST be
   zero - this can be done by adding a leading (leftmost) `00'H octet if
   necessary.  This removes a potential ambiguity in mapping between a
   string of octets and an integer value.

   As noted in section 4.1.2.2, serial numbers can be expected to
   contain long integers.  Certificate users MUST be able to handle
   serialNumber values up to 20 octets in length.  Conformant CAs MUST
   NOT use serialNumber values longer than 20 octets.
*/

/* Return the given numeric data as a string: decimal up to 64 bits,
   hex otherwise. */
static CFStringRef copyIntegerContentDescription(CFAllocatorRef allocator,
	const DERItem *integer) {
	uint64_t value = 0;
	CFIndex ix, length = integer->length;

	if (length == 0 || length > 8)
		return copyHexDescription(allocator, integer);

	for(ix = 0; ix < length; ++ix) {
		value <<= 8;
		value += integer->data[ix];
	}

    return CFStringCreateWithFormat(allocator, NULL, CFSTR("%llu"), value);
}

static CFStringRef copyDERThingContentDescription(CFAllocatorRef allocator,
	DERTag tag, const DERItem *derThing, bool printableOnly) {
	switch(tag) {
    case ASN1_INTEGER:
    case ASN1_BOOLEAN:
        return printableOnly ? NULL : copyIntegerContentDescription(allocator, derThing);
    case ASN1_PRINTABLE_STRING:
    case ASN1_IA5_STRING:
        return copyContentString(allocator, derThing, kCFStringEncodingASCII, printableOnly);
    case ASN1_UTF8_STRING:
    case ASN1_GENERAL_STRING:
    case ASN1_UNIVERSAL_STRING:
        return copyContentString(allocator, derThing, kCFStringEncodingUTF8, printableOnly);
    case ASN1_T61_STRING:		// 20, also BER_TAG_TELETEX_STRING
    case ASN1_VIDEOTEX_STRING:   // 21
    case ASN1_VISIBLE_STRING:		// 26
        return copyContentString(allocator, derThing, kCFStringEncodingISOLatin1, printableOnly);
    case ASN1_BMP_STRING:   // 30
        return copyContentString(allocator, derThing, kCFStringEncodingUTF16, printableOnly);
    case ASN1_OCTET_STRING:
        return printableOnly ? NULL :
            copyBlobString(allocator, SEC_BYTE_STRING_KEY, SEC_BYTES_KEY,
                           derThing);
        //return copyBlobString(BYTE_STRING_STR, BYTES_STR, derThing);
    case ASN1_BIT_STRING:
        return printableOnly ? NULL :
            copyBlobString(allocator, SEC_BIT_STRING_KEY, SEC_BITS_KEY,
                           derThing);
    case ASN1_CONSTR_SEQUENCE:
        return printableOnly ? NULL :
            copyBlobString(allocator, SEC_SEQUENCE_KEY, SEC_BYTES_KEY,
                           derThing);
    case ASN1_CONSTR_SET:
        return printableOnly ? NULL :
            copyBlobString(allocator, SEC_SET_KEY, SEC_BYTES_KEY, derThing);
    case ASN1_OBJECT_ID:
        return printableOnly ? NULL : copyLocalizedOidDescription(allocator, derThing);
    default:
        if (printableOnly) {
            return NULL;
        } else {
            CFStringRef fmt = SecCopyCertString(SEC_NOT_DISPLAYED_KEY);
            CFStringRef result = CFStringCreateWithFormat(allocator, NULL, fmt,
                tag, derThing->length);
            CFRelease(fmt);
            return result;
        }
	}
}

static CFStringRef copyDERThingDescription(CFAllocatorRef allocator,
	const DERItem *derThing, bool printableOnly) {
	DERDecodedInfo decoded;
	DERReturn drtn;

	drtn = DERDecodeItem(derThing, &decoded);
    if (drtn) {
        /* TODO: Perhaps put something in the label saying we couldn't parse
           the DER? */
        return printableOnly ? NULL : copyHexDescription(allocator, derThing);
    } else {
        return copyDERThingContentDescription(allocator, decoded.tag,
            &decoded.content, false);
    }
}

static void appendDERThingProperty(CFMutableArrayRef properties,
    CFStringRef label, CFStringRef localizedLabel, const DERItem *derThing) {
    CFStringRef value = copyDERThingDescription(CFGetAllocator(properties),
        derThing, false);
    appendProperty(properties, kSecPropertyTypeString, label, localizedLabel,
                   value);
    CFReleaseSafe(value);
}

static OSStatus appendRDNProperty(void *context, const DERItem *rdnType,
	const DERItem *rdnValue, CFIndex rdnIX) {
	CFMutableArrayRef properties = (CFMutableArrayRef)context;
	if (rdnIX > 0) {
		/* If there is more than one value pair we create a subsection for the
		   second pair, and append things to the subsection for subsequent
		   pairs. */
		CFIndex lastIX = CFArrayGetCount(properties) - 1;
		CFTypeRef lastValue = CFArrayGetValueAtIndex(properties, lastIX);
		if (rdnIX == 1) {
			/* Since this is the second rdn pair for a given rdn, we setup a
			   new subsection for this rdn.  We remove the first property
			   from the properties array and make it the first element in the
			   subsection instead. */
			CFMutableArrayRef rdn_props = CFArrayCreateMutable(
				CFGetAllocator(properties), 0, &kCFTypeArrayCallBacks);
			CFArrayAppendValue(rdn_props, lastValue);
			CFArrayRemoveValueAtIndex(properties, lastIX);
			appendProperty(properties, kSecPropertyTypeSection, NULL, NULL,
                           rdn_props);
			properties = rdn_props;
		} else {
			/* Since this is the third or later rdn pair we have already
			   created a subsection in the top level properties array.  Instead
			   of appending to that directly we append to the array inside the
			   subsection. */
			properties = (CFMutableArrayRef)CFDictionaryGetValue(
				(CFDictionaryRef)lastValue, kSecPropertyKeyValue);
		}
	}

	/* Finally we append the new rdn value to the property array. */
	CFStringRef label = SecDERItemCopyOIDDecimalRepresentation(
        CFGetAllocator(properties), rdnType);
	CFStringRef localizedLabel =
        copyLocalizedOidDescription(CFGetAllocator(properties), rdnType);
    appendDERThingProperty(properties, label, localizedLabel, rdnValue);
    CFReleaseSafe(label);
    CFReleaseSafe(localizedLabel);
    return errSecSuccess;
}

static CFArrayRef createPropertiesForRDNContent(CFAllocatorRef allocator,
	const DERItem *rdnSetContent) {
	CFMutableArrayRef properties = CFArrayCreateMutable(allocator, 0,
		&kCFTypeArrayCallBacks);
	OSStatus status = parseRDNContent(rdnSetContent, properties,
		appendRDNProperty);
	if (status) {
        CFArrayRemoveAllValues(properties);
		appendInvalidProperty(properties, SEC_RDN_KEY, rdnSetContent);
	}

	return properties;
}

/*
    From rfc3739 - 3.1.2.  Subject

    When parsing the subject here are some tips for a short name of the cert.
      Choice   I:  commonName
      Choice  II:  givenName
      Choice III:  pseudonym

      The commonName attribute value SHALL, when present, contain a name
      of the subject.  This MAY be in the subject's preferred
      presentation format, or a format preferred by the CA, or some
      other format.  Pseudonyms, nicknames, and names with spelling
      other than defined by the registered name MAY be used.  To
      understand the nature of the name presented in commonName,
      complying applications MAY have to examine present values of the
      givenName and surname attributes, or the pseudonym attribute.

*/
static CFArrayRef createPropertiesForX501NameContent(CFAllocatorRef allocator,
	const DERItem *x501NameContent) {
	CFMutableArrayRef properties = CFArrayCreateMutable(allocator, 0,
		&kCFTypeArrayCallBacks);
	OSStatus status = parseX501NameContent(x501NameContent, properties,
		appendRDNProperty);
	if (status) {
        CFArrayRemoveAllValues(properties);
        appendInvalidProperty(properties, SEC_X501_NAME_KEY, x501NameContent);
	}

	return properties;
}

static CFArrayRef createPropertiesForX501Name(CFAllocatorRef allocator,
	const DERItem *x501Name) {
	CFMutableArrayRef properties = CFArrayCreateMutable(allocator, 0,
		&kCFTypeArrayCallBacks);
	OSStatus status = parseX501Name(x501Name, properties, appendRDNProperty);
	if (status) {
        CFArrayRemoveAllValues(properties);
        appendInvalidProperty(properties, SEC_X501_NAME_KEY, x501Name);
	}

	return properties;
}

static void appendIntegerProperty(CFMutableArrayRef properties,
    CFStringRef label, const DERItem *integer) {
    CFStringRef string = copyIntegerContentDescription(
        CFGetAllocator(properties), integer);
    appendProperty(properties, kSecPropertyTypeString, label, NULL, string);
    CFRelease(string);
}

static void appendBoolProperty(CFMutableArrayRef properties,
    CFStringRef label, bool boolean) {
    CFStringRef value = SecCopyCertString(boolean ? SEC_YES_KEY : SEC_NO_KEY);
    appendProperty(properties, kSecPropertyTypeString, label, NULL, value);
    CFRelease(value);
}

static void appendBooleanProperty(CFMutableArrayRef properties,
    CFStringRef label, const DERItem *boolean, bool defaultValue) {
    bool result;
    DERReturn drtn = DERParseBooleanWithDefault(boolean, defaultValue, &result);
    if (drtn) {
        /* Couldn't parse boolean; dump the raw unparsed data as hex. */
        appendInvalidProperty(properties, label, boolean);
    } else {
        appendBoolProperty(properties, label, result);
    }
}

static void appendSerialNumberProperty(CFMutableArrayRef parent, CFStringRef label,
                                       DERItem *serialNum) {
    CFAllocatorRef allocator = CFGetAllocator(parent);
    CFMutableArrayRef properties = CFArrayCreateMutable(allocator, 0, &kCFTypeArrayCallBacks);

    if (serialNum->length) {
        appendIntegerProperty(properties, SEC_SERIAL_NUMBER_KEY,
                              serialNum);
        appendProperty(parent, kSecPropertyTypeSection, label, NULL, properties);
    }

    CFReleaseNull(properties);
}

static void appendBitStringContentNames(CFMutableArrayRef properties,
    CFStringRef label, const DERItem *bitStringContent,
    const CFStringRef *names, CFIndex namesCount) {
    DERSize len = bitStringContent->length - 1;
    require_quiet(len == 1 || len == 2, badDER);
    DERByte numUnusedBits = bitStringContent->data[0];
    require_quiet(numUnusedBits < 8, badDER);
    uint_fast16_t bits = 8 * len - numUnusedBits;
    require_quiet(bits <= (uint_fast16_t)namesCount, badDER);
    uint_fast16_t value = bitStringContent->data[1];
    uint_fast16_t mask;
    if (len > 1) {
        value = (value << 8) + bitStringContent->data[2];
        mask = 0x8000;
    } else {
        mask = 0x80;
    }
    uint_fast16_t ix;
    CFStringRef fmt = SecCopyCertString(SEC_STRING_LIST_KEY);
    CFStringRef string = NULL;
    for (ix = 0; ix < bits; ++ix) {
        if (value & mask) {
            if (string) {
                CFStringRef s =
                    CFStringCreateWithFormat(CFGetAllocator(properties),
                                             NULL, fmt, string, names[ix]);
                CFRelease(string);
                string = s;
            } else {
                string = names[ix];
                CFRetain(string);
            }
        }
        mask >>= 1;
    }
    CFRelease(fmt);
    appendProperty(properties, kSecPropertyTypeString, label, NULL,
                   string ? string : CFSTR(""));
    CFReleaseSafe(string);
    return;
badDER:
    appendInvalidProperty(properties, label, bitStringContent);
}

static void appendBitStringNames(CFMutableArrayRef properties,
    CFStringRef label, const DERItem *bitString,
    const CFStringRef *names, CFIndex namesCount) {
    DERDecodedInfo bitStringContent;
    DERReturn drtn = DERDecodeItem(bitString, &bitStringContent);
    require_noerr_quiet(drtn, badDER);
    require_quiet(bitStringContent.tag == ASN1_BIT_STRING, badDER);
    appendBitStringContentNames(properties, label, &bitStringContent.content,
        names, namesCount);
    return;
badDER:
    appendInvalidProperty(properties, label, bitString);
}

static void appendKeyUsage(CFMutableArrayRef properties,
    const DERItem *extnValue) {
    static const CFStringRef usageNames[] = {
        SEC_DIGITAL_SIGNATURE_KEY,
        SEC_NON_REPUDIATION_KEY,
        SEC_KEY_ENCIPHERMENT_KEY,
        SEC_DATA_ENCIPHERMENT_KEY,
        SEC_KEY_AGREEMENT_KEY,
        SEC_CERT_SIGN_KEY,
        SEC_CRL_SIGN_KEY,
        SEC_ENCIPHER_ONLY_KEY,
        SEC_DECIPHER_ONLY_KEY
    };
    appendBitStringNames(properties, SEC_USAGE_KEY, extnValue,
        usageNames, array_size(usageNames));
}

static void appendPrivateKeyUsagePeriod(CFMutableArrayRef properties,
    const DERItem *extnValue) {
    DERPrivateKeyUsagePeriod pkup;
	DERReturn drtn = DERParseSequence(extnValue,
        DERNumPrivateKeyUsagePeriodItemSpecs, DERPrivateKeyUsagePeriodItemSpecs,
        &pkup, sizeof(pkup));
	require_noerr_quiet(drtn, badDER);
    if (pkup.notBefore.length) {
        appendDateContentProperty(properties, SEC_NOT_VALID_BEFORE_KEY,
            ASN1_GENERALIZED_TIME, &pkup.notBefore);
    }
    if (pkup.notAfter.length) {
        appendDateContentProperty(properties, SEC_NOT_VALID_AFTER_KEY,
            ASN1_GENERALIZED_TIME, &pkup.notAfter);
    }
    return;
badDER:
    appendInvalidProperty(properties, SEC_PRIVATE_KU_PERIOD_KEY, extnValue);
}

static void appendStringContentProperty(CFMutableArrayRef properties,
	CFStringRef label, const DERItem *stringContent,
	CFStringEncoding encoding) {
    CFStringRef string = CFStringCreateWithBytes(CFGetAllocator(properties),
		stringContent->data, stringContent->length, encoding, FALSE);
    if (string) {
		appendProperty(properties, kSecPropertyTypeString, label, NULL, string);
        CFRelease(string);
	} else {
		appendInvalidProperty(properties, label, stringContent);
	}
}

/*
      OtherName ::= SEQUENCE {
           type-id    OBJECT IDENTIFIER,
           value      [0] EXPLICIT ANY DEFINED BY type-id }
*/
static void appendOtherNameContentProperty(CFMutableArrayRef properties,
	const DERItem *otherNameContent) {
    DEROtherName on;
	DERReturn drtn = DERParseSequenceContent(otherNameContent,
        DERNumOtherNameItemSpecs, DEROtherNameItemSpecs,
        &on, sizeof(on));
	require_noerr_quiet(drtn, badDER);
	CFAllocatorRef allocator = CFGetAllocator(properties);
	CFStringRef label =
        SecDERItemCopyOIDDecimalRepresentation(allocator, &on.typeIdentifier);
	CFStringRef localizedLabel =
        copyLocalizedOidDescription(allocator, &on.typeIdentifier);
	CFStringRef value_string = copyDERThingDescription(allocator, &on.value, false);
	if (value_string)
		appendProperty(properties, kSecPropertyTypeString, label,
                       localizedLabel, value_string);
	else
        appendUnparsedProperty(properties, label, localizedLabel, &on.value);

    CFReleaseSafe(value_string);
    CFReleaseSafe(label);
    CFReleaseSafe(localizedLabel);
    return;
badDER:
    appendInvalidProperty(properties, SEC_OTHER_NAME_KEY, otherNameContent);
}

/*
      GeneralName ::= CHOICE {
           otherName                       [0]     OtherName,
           rfc822Name                      [1]     IA5String,
           dNSName                         [2]     IA5String,
           x400Address                     [3]     ORAddress,
           directoryName                   [4]     Name,
           ediPartyName                    [5]     EDIPartyName,
           uniformResourceIdentifier       [6]     IA5String,
           iPAddress                       [7]     OCTET STRING,
           registeredID                    [8]     OBJECT IDENTIFIER}

      EDIPartyName ::= SEQUENCE {
           nameAssigner            [0]     DirectoryString OPTIONAL,
           partyName               [1]     DirectoryString }
 */
static bool appendGeneralNameContentProperty(CFMutableArrayRef properties,
    DERTag tag, const DERItem *generalName) {
	switch (tag) {
	case ASN1_CONTEXT_SPECIFIC | ASN1_CONSTRUCTED | 0:
		appendOtherNameContentProperty(properties, generalName);
		break;
	case ASN1_CONTEXT_SPECIFIC | 1:
		/* IA5String. */
		appendStringContentProperty(properties, SEC_EMAIL_ADDRESS_KEY,
			generalName, kCFStringEncodingASCII);
		break;
	case ASN1_CONTEXT_SPECIFIC | 2:
		/* IA5String. */
		appendStringContentProperty(properties, SEC_DNS_NAME_KEY, generalName,
			kCFStringEncodingASCII);
		break;
	case ASN1_CONTEXT_SPECIFIC | ASN1_CONSTRUCTED | 3:
		appendUnparsedProperty(properties, SEC_X400_ADDRESS_KEY, NULL,
			generalName);
		break;
	case ASN1_CONTEXT_SPECIFIC | ASN1_CONSTRUCTED | 4:
	{
		CFArrayRef directory_plist =
			createPropertiesForX501Name(CFGetAllocator(properties),
				generalName);
		appendProperty(properties, kSecPropertyTypeSection,
			SEC_DIRECTORY_NAME_KEY, NULL, directory_plist);
		CFRelease(directory_plist);
		break;
	}
	case ASN1_CONTEXT_SPECIFIC | ASN1_CONSTRUCTED | 5:
		appendUnparsedProperty(properties, SEC_EDI_PARTY_NAME_KEY, NULL,
			generalName);
		break;
	case ASN1_CONTEXT_SPECIFIC | ASN1_CONSTRUCTED | 6:
		/* Technically I don't think this is valid, but there are certs out
		   in the wild that use a constructed IA5String.   In particular the
		   VeriSign Time Stamping Authority CA.cer does this.  */
		appendURLProperty(properties, SEC_URI_KEY, generalName);
		break;
	case ASN1_CONTEXT_SPECIFIC | 6:
		appendURLContentProperty(properties, SEC_URI_KEY, generalName);
		break;
	case ASN1_CONTEXT_SPECIFIC | 7:
		appendIPAddressContentProperty(properties, SEC_IP_ADDRESS_KEY,
			generalName);
		break;
	case ASN1_CONTEXT_SPECIFIC | 8:
		appendOIDProperty(properties, SEC_REGISTERED_ID_KEY, NULL, generalName);
		break;
	default:
		goto badDER;
		break;
	}
	return true;
badDER:
	return false;
}

static void appendGeneralNameProperty(CFMutableArrayRef properties,
    const DERItem *generalName) {
    DERDecodedInfo generalNameContent;
	DERReturn drtn = DERDecodeItem(generalName, &generalNameContent);
	require_noerr_quiet(drtn, badDER);
	if (appendGeneralNameContentProperty(properties, generalNameContent.tag,
		&generalNameContent.content))
		return;
badDER:
    appendInvalidProperty(properties, SEC_GENERAL_NAME_KEY, generalName);
}


/*
      GeneralNames ::= SEQUENCE SIZE (1..MAX) OF GeneralName
 */
static void appendGeneralNamesContent(CFMutableArrayRef properties,
    const DERItem *generalNamesContent) {
    DERSequence gnSeq;
    DERReturn drtn = DERDecodeSeqContentInit(generalNamesContent, &gnSeq);
    require_noerr_quiet(drtn, badDER);
    DERDecodedInfo generalNameContent;
    while ((drtn = DERDecodeSeqNext(&gnSeq, &generalNameContent)) ==
		DR_Success) {
		if (!appendGeneralNameContentProperty(properties,
			generalNameContent.tag, &generalNameContent.content)) {
			goto badDER;
		}
    }
    require_quiet(drtn == DR_EndOfSequence, badDER);
	return;
badDER:
    appendInvalidProperty(properties, SEC_GENERAL_NAMES_KEY,
        generalNamesContent);
}

static void appendGeneralNames(CFMutableArrayRef properties,
    const DERItem *generalNames) {
    DERDecodedInfo generalNamesContent;
    DERReturn drtn = DERDecodeItem(generalNames, &generalNamesContent);
    require_noerr_quiet(drtn, badDER);
    require_quiet(generalNamesContent.tag == ASN1_CONSTR_SEQUENCE,
        badDER);
    appendGeneralNamesContent(properties, &generalNamesContent.content);
    return;
badDER:
    appendInvalidProperty(properties, SEC_GENERAL_NAMES_KEY, generalNames);
}

/*
BasicConstraints ::= SEQUENCE {
     cA                      BOOLEAN DEFAULT FALSE,
     pathLenConstraint       INTEGER (0..MAX) OPTIONAL }
*/
static void appendBasicConstraints(CFMutableArrayRef properties,
    const DERItem *extnValue) {
	DERBasicConstraints basicConstraints;
	DERReturn drtn = DERParseSequence(extnValue,
        DERNumBasicConstraintsItemSpecs, DERBasicConstraintsItemSpecs,
        &basicConstraints, sizeof(basicConstraints));
	require_noerr_quiet(drtn, badDER);

    appendBooleanProperty(properties, SEC_CERT_AUTHORITY_KEY,
        &basicConstraints.cA, false);

    if (basicConstraints.pathLenConstraint.length != 0) {
        appendIntegerProperty(properties, SEC_PATH_LEN_CONSTRAINT_KEY,
            &basicConstraints.pathLenConstraint);
    }
    return;
badDER:
    appendInvalidProperty(properties, SEC_BASIC_CONSTRAINTS_KEY, extnValue);
}

/*
 * id-ce-nameConstraints OBJECT IDENTIFIER ::=  { id-ce 30 }
 *
 * NameConstraints ::= SEQUENCE {
 * permittedSubtrees       [0]     GeneralSubtrees OPTIONAL,
 * excludedSubtrees        [1]     GeneralSubtrees OPTIONAL }
 *
 * GeneralSubtrees ::= SEQUENCE SIZE (1..MAX) OF GeneralSubtree
 *
 * GeneralSubtree ::= SEQUENCE {
 * base                    GeneralName,
 * minimum         [0]     BaseDistance DEFAULT 0,
 * maximum         [1]     BaseDistance OPTIONAL }
 *
 * BaseDistance ::= INTEGER (0..MAX)
 */
static void appendNameConstraints(CFMutableArrayRef properties,
    const DERItem *extnValue) {
    CFAllocatorRef allocator = CFGetAllocator(properties);
    DERNameConstraints nc;
    DERReturn drtn;
    drtn = DERParseSequence(extnValue,
                            DERNumNameConstraintsItemSpecs,
                            DERNameConstraintsItemSpecs,
                            &nc, sizeof(nc));
    require_noerr_quiet(drtn, badDER);
    if (nc.permittedSubtrees.length) {
        DERSequence gsSeq;
        require_noerr_quiet(DERDecodeSeqContentInit(&nc.permittedSubtrees, &gsSeq), badDER);
        DERDecodedInfo gsContent;
        while ((drtn = DERDecodeSeqNext(&gsSeq, &gsContent)) == DR_Success) {
            DERGeneralSubtree derGS;
            require_quiet(gsContent.tag==ASN1_CONSTR_SEQUENCE, badDER);
            drtn = DERParseSequenceContent(&gsContent.content,
                                           DERNumGeneralSubtreeItemSpecs,
                                           DERGeneralSubtreeItemSpecs,
                                           &derGS, sizeof(derGS));
            require_noerr_quiet(drtn, badDER);
            if (derGS.minimum.length) {
                appendIntegerProperty(properties, SEC_PERMITTED_MINIMUM_KEY, &derGS.minimum);
            }
            if (derGS.maximum.length) {
                appendIntegerProperty(properties, SEC_PERMITTED_MAXIMUM_KEY, &derGS.maximum);
            }
            if (derGS.generalName.length) {
                CFMutableArrayRef base = CFArrayCreateMutable(allocator, 0,
                                                                   &kCFTypeArrayCallBacks);
                appendProperty(properties, kSecPropertyTypeSection,
                               SEC_PERMITTED_NAME_KEY, NULL, base);
                appendGeneralNameProperty(base, &derGS.generalName);
                CFRelease(base);
            }
        }
        require_quiet(drtn == DR_EndOfSequence, badDER);
    }
    if (nc.excludedSubtrees.length) {
        DERSequence gsSeq;
        require_noerr_quiet(DERDecodeSeqContentInit(&nc.excludedSubtrees, &gsSeq), badDER);
        DERDecodedInfo gsContent;
        while ((drtn = DERDecodeSeqNext(&gsSeq, &gsContent)) == DR_Success) {
            DERGeneralSubtree derGS;
            require_quiet(gsContent.tag==ASN1_CONSTR_SEQUENCE, badDER);
            drtn = DERParseSequenceContent(&gsContent.content,
                                           DERNumGeneralSubtreeItemSpecs,
                                           DERGeneralSubtreeItemSpecs,
                                           &derGS, sizeof(derGS));
            require_noerr_quiet(drtn, badDER);
            if (derGS.minimum.length) {
                appendIntegerProperty(properties, SEC_EXCLUDED_MINIMUM_KEY, &derGS.minimum);
            }
            if (derGS.maximum.length) {
                appendIntegerProperty(properties, SEC_EXCLUDED_MAXIMUM_KEY, &derGS.maximum);
            }
            if (derGS.generalName.length) {
                CFMutableArrayRef base = CFArrayCreateMutable(allocator, 0,
                                                              &kCFTypeArrayCallBacks);
                appendProperty(properties, kSecPropertyTypeSection,
                               SEC_EXCLUDED_NAME_KEY, NULL, base);
                appendGeneralNameProperty(base, &derGS.generalName);
                CFRelease(base);
            }
        }
        require_quiet(drtn == DR_EndOfSequence, badDER);
    }

    return;
badDER:
    appendInvalidProperty(properties, SEC_NAME_CONSTRAINTS_KEY, extnValue);
}

/*
   CRLDistPointsSyntax ::= SEQUENCE SIZE (1..MAX) OF DistributionPoint

   DistributionPoint ::= SEQUENCE {
        distributionPoint       [0]     DistributionPointName OPTIONAL,
        reasons                 [1]     ReasonFlags OPTIONAL,
        cRLIssuer               [2]     GeneralNames OPTIONAL }

   DistributionPointName ::= CHOICE {
        fullName                [0]     GeneralNames,
        nameRelativeToCRLIssuer [1]     RelativeDistinguishedName }

   ReasonFlags ::= BIT STRING {
        unused                  (0),
        keyCompromise           (1),
        cACompromise            (2),
        affiliationChanged      (3),
        superseded              (4),
        cessationOfOperation    (5),
        certificateHold         (6),
        privilegeWithdrawn      (7),
        aACompromise            (8) }
*/
static void appendCrlDistributionPoints(CFMutableArrayRef properties,
    const DERItem *extnValue) {
    CFAllocatorRef allocator = CFGetAllocator(properties);
    DERTag tag;
    DERSequence dpSeq;
    DERReturn drtn = DERDecodeSeqInit(extnValue, &tag, &dpSeq);
    require_noerr_quiet(drtn, badDER);
    require_quiet(tag == ASN1_CONSTR_SEQUENCE, badDER);
    DERDecodedInfo dpSeqContent;
    while ((drtn = DERDecodeSeqNext(&dpSeq, &dpSeqContent)) == DR_Success) {
        require_quiet(dpSeqContent.tag == ASN1_CONSTR_SEQUENCE, badDER);
        DERDistributionPoint dp;
        drtn = DERParseSequenceContent(&dpSeqContent.content,
            DERNumDistributionPointItemSpecs,
            DERDistributionPointItemSpecs,
            &dp, sizeof(dp));
        require_noerr_quiet(drtn, badDER);
        if (dp.distributionPoint.length) {
            DERDecodedInfo distributionPointName;
            drtn = DERDecodeItem(&dp.distributionPoint, &distributionPointName);
            require_noerr_quiet(drtn, badDER);
            if (distributionPointName.tag ==
                (ASN1_CONTEXT_SPECIFIC | ASN1_CONSTRUCTED | 0)) {
                /* Full Name */
                appendGeneralNamesContent(properties,
                    &distributionPointName.content);
            } else if (distributionPointName.tag ==
                (ASN1_CONTEXT_SPECIFIC | ASN1_CONSTRUCTED | 1)) {
				CFArrayRef rdn_props = createPropertiesForRDNContent(allocator,
					&dp.reasons);
				appendProperty(properties, kSecPropertyTypeSection,
					SEC_NAME_REL_CRL_ISSUER_KEY, NULL, rdn_props);
				CFRelease(rdn_props);
            } else {
                goto badDER;
            }
        }
        if (dp.reasons.length) {
            static const CFStringRef reasonNames[] = {
                SEC_UNUSED_KEY,
                SEC_KEY_COMPROMISE_KEY,
                SEC_CA_COMPROMISE_KEY,
                SEC_AFFILIATION_CHANGED_KEY,
                SEC_SUPERSEDED_KEY,
                SEC_CESSATION_OF_OPER_KEY,
                SEC_CERTIFICATE_HOLD_KEY,
                SEC_PRIV_WITHDRAWN_KEY,
                SEC_AA_COMPROMISE_KEY
            };
            appendBitStringContentNames(properties, SEC_REASONS_KEY,
                &dp.reasons,
                reasonNames, array_size(reasonNames));
        }
        if (dp.cRLIssuer.length) {
            CFMutableArrayRef crlIssuer = CFArrayCreateMutable(allocator, 0,
                &kCFTypeArrayCallBacks);
            appendProperty(properties, kSecPropertyTypeSection,
                SEC_CRL_ISSUER_KEY, NULL, crlIssuer);
            CFRelease(crlIssuer);
            appendGeneralNames(crlIssuer, &dp.cRLIssuer);
        }
    }
    require_quiet(drtn == DR_EndOfSequence, badDER);
	return;
badDER:
    appendInvalidProperty(properties, SEC_CRL_DISTR_POINTS_KEY, extnValue);
}

/* Decode a sequence of integers into a comma separated list of ints. */
static void appendIntegerSequenceContent(CFMutableArrayRef properties,
    CFStringRef label, const DERItem *intSequenceContent) {
    CFAllocatorRef allocator = CFGetAllocator(properties);
	DERSequence intSeq;
    CFStringRef fmt = NULL, value = NULL, intDesc = NULL, v = NULL;
	DERReturn drtn = DERDecodeSeqContentInit(intSequenceContent, &intSeq);
	require_noerr_quiet(drtn, badDER);
	DERDecodedInfo intContent;
    fmt = SecCopyCertString(SEC_STRING_LIST_KEY);
	while ((drtn = DERDecodeSeqNext(&intSeq, &intContent)) == DR_Success) {
		require_quiet(intContent.tag == ASN1_INTEGER, badDER);
		intDesc = copyIntegerContentDescription(
			allocator, &intContent.content);
		if (value) {
            v = CFStringCreateWithFormat(allocator, NULL, fmt, value, intDesc);
            CFReleaseNull(value);
            value = v;
            CFReleaseNull(intDesc);
		} else {
			value = intDesc;
		}
	}
    CFReleaseNull(fmt);
	require_quiet(drtn == DR_EndOfSequence, badDER);
	if (value) {
		appendProperty(properties, kSecPropertyTypeString, label, NULL, value);
		CFRelease(value);
		return;
	}
	/* DROPTHOUGH if !value. */
badDER:
    CFReleaseNull(fmt);
    CFReleaseNull(intDesc);
    CFReleaseNull(v);
	appendInvalidProperty(properties, label, intSequenceContent);
}

static void appendCertificatePolicies(CFMutableArrayRef properties,
    const DERItem *extnValue) {
    CFAllocatorRef allocator = CFGetAllocator(properties);
    DERTag tag;
    DERSequence piSeq;
    DERReturn drtn = DERDecodeSeqInit(extnValue, &tag, &piSeq);
    require_noerr_quiet(drtn, badDER);
    require_quiet(tag == ASN1_CONSTR_SEQUENCE, badDER);
    DERDecodedInfo piContent;
    int pin = 1;
    while ((drtn = DERDecodeSeqNext(&piSeq, &piContent)) == DR_Success) {
        require_quiet(piContent.tag == ASN1_CONSTR_SEQUENCE, badDER);
        DERPolicyInformation pi;
        drtn = DERParseSequenceContent(&piContent.content,
            DERNumPolicyInformationItemSpecs,
            DERPolicyInformationItemSpecs,
            &pi, sizeof(pi));
        require_noerr_quiet(drtn, badDER);
        CFStringRef piLabel = CFStringCreateWithFormat(allocator, NULL,
            SEC_POLICY_IDENTIFIER_KEY, pin);
        CFStringRef piFmt = SecCopyCertString(SEC_POLICY_IDENTIFIER_KEY);
        CFStringRef lpiLabel = CFStringCreateWithFormat(allocator, NULL,
            piFmt, pin++);
        CFRelease(piFmt);
        appendOIDProperty(properties, piLabel, lpiLabel, &pi.policyIdentifier);
        CFRelease(piLabel);
        CFRelease(lpiLabel);
        if (pi.policyQualifiers.length == 0)
            continue;

        DERSequence pqSeq;
        drtn = DERDecodeSeqContentInit(&pi.policyQualifiers, &pqSeq);
        require_noerr_quiet(drtn, badDER);
        DERDecodedInfo pqContent;
        int pqn = 1;
        while ((drtn = DERDecodeSeqNext(&pqSeq, &pqContent)) == DR_Success) {
            DERPolicyQualifierInfo pqi;
            drtn = DERParseSequenceContent(&pqContent.content,
                DERNumPolicyQualifierInfoItemSpecs,
                DERPolicyQualifierInfoItemSpecs,
                &pqi, sizeof(pqi));
            require_noerr_quiet(drtn, badDER);
            DERDecodedInfo qualifierContent;
            drtn = DERDecodeItem(&pqi.qualifier, &qualifierContent);
            require_noerr_quiet(drtn, badDER);
            CFStringRef pqLabel = CFStringCreateWithFormat(allocator, NULL,
                SEC_POLICY_QUALIFIER_KEY, pqn);
            CFStringRef pqFmt = SecCopyCertString(SEC_POLICY_QUALIFIER_KEY);
            CFStringRef lpqLabel = CFStringCreateWithFormat(allocator, NULL,
                pqFmt, pqn++);
            CFRelease(pqFmt);
            appendOIDProperty(properties, pqLabel, lpqLabel,
                              &pqi.policyQualifierID);
            CFRelease(pqLabel);
            CFRelease(lpqLabel);
            if (DEROidCompare(&oidQtCps, &pqi.policyQualifierID)) {
                require_quiet(qualifierContent.tag == ASN1_IA5_STRING, badDER);
                appendURLContentProperty(properties, SEC_CPS_URI_KEY,
                                         &qualifierContent.content);
            } else if (DEROidCompare(&oidQtUNotice, &pqi.policyQualifierID)) {
                require_quiet(qualifierContent.tag == ASN1_CONSTR_SEQUENCE, badDER);
                DERUserNotice un;
                drtn = DERParseSequenceContent(&qualifierContent.content,
                    DERNumUserNoticeItemSpecs,
                    DERUserNoticeItemSpecs,
                    &un, sizeof(un));
                require_noerr_quiet(drtn, badDER);
                if (un.noticeRef.length) {
                    DERNoticeReference nr;
                    drtn = DERParseSequenceContent(&un.noticeRef,
                        DERNumNoticeReferenceItemSpecs,
                        DERNoticeReferenceItemSpecs,
                        &nr, sizeof(nr));
                    require_noerr_quiet(drtn, badDER);
                    appendDERThingProperty(properties,
                        SEC_ORGANIZATION_KEY, NULL,
                        &nr.organization);
					appendIntegerSequenceContent(properties,
						SEC_NOTICE_NUMBERS_KEY, &nr.noticeNumbers);
                }
                if (un.explicitText.length) {
                    appendDERThingProperty(properties, SEC_EXPLICIT_TEXT_KEY,
                        NULL, &un.explicitText);
                }
            } else {
                appendUnparsedProperty(properties, SEC_QUALIFIER_KEY, NULL,
                    &pqi.qualifier);
            }
        }
    }
    require_quiet(drtn == DR_EndOfSequence, badDER);
	return;
badDER:
    appendInvalidProperty(properties, SEC_CERT_POLICIES_KEY, extnValue);
}

static void appendSubjectKeyIdentifier(CFMutableArrayRef properties,
    const DERItem *extnValue) {
	DERReturn drtn;
    DERDecodedInfo keyIdentifier;
	drtn = DERDecodeItem(extnValue, &keyIdentifier);
	require_noerr_quiet(drtn, badDER);
	require_quiet(keyIdentifier.tag == ASN1_OCTET_STRING, badDER);
	appendDataProperty(properties, SEC_KEY_IDENTIFIER_KEY, NULL,
		&keyIdentifier.content);

	return;
badDER:
    appendInvalidProperty(properties, SEC_SUBJ_KEY_ID_KEY,
        extnValue);
}

/*
AuthorityKeyIdentifier ::= SEQUENCE {
    keyIdentifier             [0] KeyIdentifier            OPTIONAL,
    authorityCertIssuer       [1] GeneralNames             OPTIONAL,
    authorityCertSerialNumber [2] CertificateSerialNumber  OPTIONAL }
    -- authorityCertIssuer and authorityCertSerialNumber MUST both
    -- be present or both be absent

KeyIdentifier ::= OCTET STRING
*/
static void appendAuthorityKeyIdentifier(CFMutableArrayRef properties,
    const DERItem *extnValue) {
	DERAuthorityKeyIdentifier akid;
	DERReturn drtn;
	drtn = DERParseSequence(extnValue,
		DERNumAuthorityKeyIdentifierItemSpecs,
		DERAuthorityKeyIdentifierItemSpecs,
		&akid, sizeof(akid));
	require_noerr_quiet(drtn, badDER);
	if (akid.keyIdentifier.length) {
		appendDataProperty(properties, SEC_KEY_IDENTIFIER_KEY, NULL,
			&akid.keyIdentifier);
	}
	if (akid.authorityCertIssuer.length ||
		akid.authorityCertSerialNumber.length) {
		require_quiet(akid.authorityCertIssuer.length &&
			akid.authorityCertSerialNumber.length, badDER);
		/* Perhaps put in a subsection called Authority Certificate Issuer. */
		appendGeneralNamesContent(properties,
			&akid.authorityCertIssuer);
		appendIntegerProperty(properties, SEC_AUTH_CERT_SERIAL_KEY,
			&akid.authorityCertSerialNumber);
	}

	return;
badDER:
    appendInvalidProperty(properties, SEC_AUTHORITY_KEY_ID_KEY, extnValue);
}

/*
   PolicyConstraints ::= SEQUENCE {
        requireExplicitPolicy           [0] SkipCerts OPTIONAL,
        inhibitPolicyMapping            [1] SkipCerts OPTIONAL }

   SkipCerts ::= INTEGER (0..MAX)
*/
static void appendPolicyConstraints(CFMutableArrayRef properties,
    const DERItem *extnValue) {
	DERPolicyConstraints pc;
	DERReturn drtn;
	drtn = DERParseSequence(extnValue,
		DERNumPolicyConstraintsItemSpecs,
		DERPolicyConstraintsItemSpecs,
		&pc, sizeof(pc));
	require_noerr_quiet(drtn, badDER);
	if (pc.requireExplicitPolicy.length) {
		appendIntegerProperty(properties, SEC_REQUIRE_EXPL_POLICY_KEY,
                              &pc.requireExplicitPolicy);
	}
	if (pc.inhibitPolicyMapping.length) {
		appendIntegerProperty(properties, SEC_INHIBIT_POLICY_MAP_KEY,
                              &pc.inhibitPolicyMapping);
	}

	return;

badDER:
	appendInvalidProperty(properties, SEC_POLICY_CONSTRAINTS_KEY, extnValue);
}

/*
extendedKeyUsage EXTENSION ::= {
        SYNTAX SEQUENCE SIZE (1..MAX) OF KeyPurposeId
        IDENTIFIED BY id-ce-extKeyUsage }

KeyPurposeId ::= OBJECT IDENTIFIER
*/
static void appendExtendedKeyUsage(CFMutableArrayRef properties,
    const DERItem *extnValue) {
    DERTag tag;
    DERSequence derSeq;
    DERReturn drtn = DERDecodeSeqInit(extnValue, &tag, &derSeq);
    require_noerr_quiet(drtn, badDER);
    require_quiet(tag == ASN1_CONSTR_SEQUENCE, badDER);
    DERDecodedInfo currDecoded;
    while ((drtn = DERDecodeSeqNext(&derSeq, &currDecoded)) == DR_Success) {
        require_quiet(currDecoded.tag == ASN1_OBJECT_ID, badDER);
        appendOIDProperty(properties, SEC_PURPOSE_KEY, NULL,
            &currDecoded.content);
    }
    require_quiet(drtn == DR_EndOfSequence, badDER);
	return;
badDER:
    appendInvalidProperty(properties, SEC_EXTENDED_KEY_USAGE_KEY, extnValue);
}

/*
   id-pe-authorityInfoAccess OBJECT IDENTIFIER ::= { id-pe 1 }

   AuthorityInfoAccessSyntax  ::=
           SEQUENCE SIZE (1..MAX) OF AccessDescription

   AccessDescription  ::=  SEQUENCE {
           accessMethod          OBJECT IDENTIFIER,
           accessLocation        GeneralName  }

   id-ad OBJECT IDENTIFIER ::= { id-pkix 48 }

   id-ad-caIssuers OBJECT IDENTIFIER ::= { id-ad 2 }

   id-ad-ocsp OBJECT IDENTIFIER ::= { id-ad 1 }
*/
static void appendInfoAccess(CFMutableArrayRef properties,
    const DERItem *extnValue) {
    DERTag tag;
    DERSequence adSeq;
    DERReturn drtn = DERDecodeSeqInit(extnValue, &tag, &adSeq);
    require_noerr_quiet(drtn, badDER);
    require_quiet(tag == ASN1_CONSTR_SEQUENCE, badDER);
    DERDecodedInfo adContent;
    while ((drtn = DERDecodeSeqNext(&adSeq, &adContent)) == DR_Success) {
        require_quiet(adContent.tag == ASN1_CONSTR_SEQUENCE, badDER);
		DERAccessDescription ad;
		drtn = DERParseSequenceContent(&adContent.content,
			DERNumAccessDescriptionItemSpecs,
			DERAccessDescriptionItemSpecs,
			&ad, sizeof(ad));
		require_noerr_quiet(drtn, badDER);
        appendOIDProperty(properties, SEC_ACCESS_METHOD_KEY, NULL,
                          &ad.accessMethod);
		//TODO: Do something with SEC_ACCESS_LOCATION_KEY
        appendGeneralNameProperty(properties, &ad.accessLocation);
    }
    require_quiet(drtn == DR_EndOfSequence, badDER);
	return;
badDER:
    appendInvalidProperty(properties, SEC_AUTH_INFO_ACCESS_KEY, extnValue);
}

static void appendNetscapeCertType(CFMutableArrayRef properties,
    const DERItem *extnValue) {
    static const CFStringRef certTypes[] = {
        SEC_SSL_CLIENT_KEY,
        SEC_SSL_SERVER_KEY,
        SEC_SMIME_KEY,
        SEC_OBJECT_SIGNING_KEY,
        SEC_RESERVED_KEY,
        SEC_SSL_CA_KEY,
        SEC_SMIME_CA_KEY,
        SEC_OBJECT_SIGNING_CA_KEY
    };
    appendBitStringNames(properties, SEC_USAGE_KEY, extnValue,
        certTypes, array_size(certTypes));
}

static bool appendPrintableDERSequence(CFMutableArrayRef properties,
    CFStringRef label, const DERItem *sequence) {
    DERTag tag;
    DERSequence derSeq;
    DERReturn drtn = DERDecodeSeqInit(sequence, &tag, &derSeq);
    require_noerr_quiet(drtn, badSequence);
    require_quiet(tag == ASN1_CONSTR_SEQUENCE, badSequence);
    DERDecodedInfo currDecoded;
    bool appendedSomething = false;
    while ((drtn = DERDecodeSeqNext(&derSeq, &currDecoded)) == DR_Success) {
		switch (currDecoded.tag)
		{
			case 0:                             // 0
			case ASN1_SEQUENCE:                 // 16
			case ASN1_SET:                      // 17
				// skip constructed object lengths
				break;

			case ASN1_UTF8_STRING:              // 12
			case ASN1_NUMERIC_STRING:           // 18
			case ASN1_PRINTABLE_STRING:         // 19
			case ASN1_T61_STRING:               // 20, also ASN1_TELETEX_STRING
			case ASN1_VIDEOTEX_STRING:          // 21
			case ASN1_IA5_STRING:               // 22
			case ASN1_GRAPHIC_STRING:           // 25
			case ASN1_VISIBLE_STRING:           // 26, also ASN1_ISO646_STRING
			case ASN1_GENERAL_STRING:           // 27
			case ASN1_UNIVERSAL_STRING:         // 28
			{
                CFStringRef string =
                    copyDERThingContentDescription(CFGetAllocator(properties),
                        currDecoded.tag, &currDecoded.content, false);
                //CFStringRef cleanString = copyStringRemovingPercentEscapes(string);

                appendProperty(properties, kSecPropertyTypeString, label, NULL,
                    string);
                CFRelease(string);
				appendedSomething = true;
                break;
			}
			default:
				break;
		}
    }
    require_quiet(drtn == DR_EndOfSequence, badSequence);
	return appendedSomething;
badSequence:
    return false;
}

static void appendExtension(CFMutableArrayRef parent,
    const SecCertificateExtension *extn) {
    CFAllocatorRef allocator = CFGetAllocator(parent);
    CFMutableArrayRef properties = CFArrayCreateMutable(allocator, 0,
        &kCFTypeArrayCallBacks);
    const DERItem
        *extnID = &extn->extnID,
        *extnValue = &extn->extnValue;
    CFStringRef label = NULL;
    CFStringRef localizedLabel = NULL;

    appendBoolProperty(properties, SEC_CRITICAL_KEY, extn->critical);
    require_quiet(extnID, xit);

	bool handled = true;
	/* Extensions that we know how to handle ourselves... */
	if (extnID->length == oidSubjectKeyIdentifier.length &&
		!memcmp(extnID->data, oidSubjectKeyIdentifier.data, extnID->length - 1))
	{
		switch (extnID->data[extnID->length - 1]) {
		case 14: /* SubjectKeyIdentifier     id-ce 14 */
			appendSubjectKeyIdentifier(properties, extnValue);
			break;
		case 15: /* KeyUsage                 id-ce 15 */
			appendKeyUsage(properties, extnValue);
			break;
		case 16: /* PrivateKeyUsagePeriod    id-ce 16 */
			appendPrivateKeyUsagePeriod(properties, extnValue);
			break;
		case 17: /* SubjectAltName           id-ce 17 */
		case 18: /* IssuerAltName            id-ce 18 */
			appendGeneralNames(properties, extnValue);
			break;
		case 19: /* BasicConstraints         id-ce 19 */
			appendBasicConstraints(properties, extnValue);
			break;
		case 30: /* NameConstraints          id-ce 30 */
			appendNameConstraints(properties, extnValue);
			break;
		case 31: /* CRLDistributionPoints    id-ce 31 */
			appendCrlDistributionPoints(properties, extnValue);
			break;
		case 32: /* CertificatePolicies      id-ce 32 */
			appendCertificatePolicies(properties, extnValue);
			break;
		case 33: /* PolicyMappings           id-ce 33 */
			handled = false;
			break;
		case 35: /* AuthorityKeyIdentifier   id-ce 35 */
			appendAuthorityKeyIdentifier(properties, extnValue);
			break;
		case 36: /* PolicyConstraints        id-ce 36 */
			appendPolicyConstraints(properties, extnValue);
			break;
		case 37: /* ExtKeyUsage              id-ce 37 */
			appendExtendedKeyUsage(properties, extnValue);
			break;
		case 46: /* FreshestCRL              id-ce 46 */
			handled = false;
			break;
		case 54: /* InhibitAnyPolicy         id-ce 54 */
			handled = false;
			break;
		default:
			handled = false;
			break;
		}
	} else if (extnID->length == oidAuthorityInfoAccess.length &&
		!memcmp(extnID->data, oidAuthorityInfoAccess.data, extnID->length - 1))
	{
		switch (extnID->data[extnID->length - 1]) {
		case  1: /* AuthorityInfoAccess      id-pe 1 */
			appendInfoAccess(properties, extnValue);
			break;
		case  3: /* QCStatements             id-pe 3 */
			handled = false;
			break;
		case 11: /* SubjectInfoAccess        id-pe 11 */
			appendInfoAccess(properties, extnValue);
			break;
		default:
			handled = false;
			break;
		}
	} else if (DEROidCompare(extnID, &oidNetscapeCertType)) {
		/* 2.16.840.1.113730.1.1 netscape 1 1 */
		appendNetscapeCertType(properties, extnValue);
	} else {
		handled = false;
	}

	if (!handled) {
		/* Try to parse and display printable string(s). */
		if (appendPrintableDERSequence(properties, SEC_DATA_KEY, extnValue)) {
			/* Nothing to do here appendPrintableDERSequence did the work. */
		} else {
			/* Couldn't parse extension; dump the raw unparsed data as hex. */
			appendUnparsedProperty(properties, SEC_DATA_KEY, NULL, extnValue);
		}
	}
    label = SecDERItemCopyOIDDecimalRepresentation(allocator, extnID);
    localizedLabel = copyLocalizedOidDescription(allocator, extnID);
    appendProperty(parent, kSecPropertyTypeSection, label, localizedLabel, properties);

xit:
    CFReleaseSafe(localizedLabel);
    CFReleaseSafe(label);
    CFReleaseSafe(properties);
}

/* Different types of summary types from least desired to most desired. */
enum SummaryType {
    kSummaryTypeNone,
    kSummaryTypePrintable,
    kSummaryTypeOrganizationName,
    kSummaryTypeOrganizationalUnitName,
    kSummaryTypeCommonName,
};

struct Summary {
    enum SummaryType type;
    CFStringRef summary;
    CFStringRef description;
};

static OSStatus obtainSummaryFromX501Name(void *context,
	const DERItem *type, const DERItem *value, CFIndex rdnIX) {
    struct Summary *summary = (struct Summary *)context;
    enum SummaryType stype = kSummaryTypeNone;
    CFStringRef string = NULL;
    if (DEROidCompare(type, &oidCommonName)) {
        stype = kSummaryTypeCommonName;
    } else if (DEROidCompare(type, &oidOrganizationalUnitName)) {
        stype = kSummaryTypeOrganizationalUnitName;
    } else if (DEROidCompare(type, &oidOrganizationName)) {
        stype = kSummaryTypeOrganizationName;
    } else if (DEROidCompare(type, &oidDescription)) {
        string = copyDERThingDescription(kCFAllocatorDefault, value, true);
        if (string) {
            if (summary->description) {
                CFStringRef fmt = SecCopyCertString(SEC_STRING_LIST_KEY);
                CFStringRef newDescription = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, fmt, string, summary->description);
                CFRelease(fmt);
                CFRelease(summary->description);
                summary->description = newDescription;
            } else {
                summary->description = string;
                CFRetain(string);
            }
            stype = kSummaryTypePrintable;
        }
    } else {
        stype = kSummaryTypePrintable;
    }

    /* Build a string with all instances of the most desired
       component type in reverse order encountered comma separated list,
       The order of desirability is defined by enum SummaryType. */
    if (summary->type <= stype) {
        if (!string)
            string = copyDERThingDescription(kCFAllocatorDefault, value, true);

        if (string) {
            if (summary->type == stype) {
                CFStringRef fmt = SecCopyCertString(SEC_STRING_LIST_KEY);
                CFStringRef newSummary = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, fmt, string, summary->summary);
                CFRelease(fmt);
                CFRelease(string);
                string = newSummary;
            } else {
                summary->type = stype;
            }
            CFReleaseSafe(summary->summary);
            summary->summary = string;
        }
    } else {
        CFReleaseSafe(string);
    }

	return errSecSuccess;
}

CFStringRef SecCertificateCopySubjectSummary(SecCertificateRef certificate) {
    struct Summary summary = {};
	parseX501NameContent(&certificate->_subject, &summary, obtainSummaryFromX501Name);
    /* If we found a description and a common name we change the summary to
       CommonName (Description). */
    if (summary.description) {
        if (summary.type == kSummaryTypeCommonName) {
            CFStringRef fmt = SecCopyCertString(SEC_COMMON_NAME_DESC_KEY);
            CFStringRef newSummary = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, fmt, summary.summary, summary.description);
            CFRelease(fmt);
            CFRelease(summary.summary);
            summary.summary = newSummary;
        }
        CFRelease(summary.description);
    }

    if (!summary.summary) {
        /* If we didn't find a suitable printable string in the subject at all, we try
           the first email address in the certificate instead. */
        CFArrayRef names = SecCertificateCopyRFC822Names(certificate);
        if (!names) {
            /* If we didn't find any email addresses in the certificate, we try finding
               a DNS name instead. */
            names = SecCertificateCopyDNSNames(certificate);
        }
        if (names) {
            summary.summary = CFArrayGetValueAtIndex(names, 0);
            CFRetain(summary.summary);
            CFRelease(names);
        }
    }

	return summary.summary;
}

CFStringRef SecCertificateCopyIssuerSummary(SecCertificateRef certificate) {
    struct Summary summary = {};
	parseX501NameContent(&certificate->_issuer, &summary, obtainSummaryFromX501Name);
    /* If we found a description and a common name we change the summary to
       CommonName (Description). */
    if (summary.description) {
        if (summary.type == kSummaryTypeCommonName) {
            CFStringRef fmt = SecCopyCertString(SEC_COMMON_NAME_DESC_KEY);
            CFStringRef newSummary = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, fmt, summary.summary, summary.description);
            CFRelease(fmt);
            CFRelease(summary.summary);
            summary.summary = newSummary;
        }
        CFRelease(summary.description);
    }

	return summary.summary;
}

/* Return the earliest date on which all certificates in this chain are still
   valid. */
static CFAbsoluteTime SecCertificateGetChainsLastValidity(
    SecCertificateRef certificate) {
    CFAbsoluteTime earliest = certificate->_notAfter;
#if 0
    while (certificate->_parent) {
        certificate = certificate->_parent;
        if (earliest > certificate->_notAfter)
            earliest = certificate->_notAfter;
    }
#endif

    return earliest;
}

/* Return the latest date on which all certificates in this chain will be
   valid. */
static CFAbsoluteTime SecCertificateGetChainsFirstValidity(
    SecCertificateRef certificate) {
    CFAbsoluteTime latest = certificate->_notBefore;
#if 0
    while (certificate->_parent) {
        certificate = certificate->_parent;
        if (latest < certificate->_notBefore)
            latest = certificate->_notBefore;
    }
#endif

    return latest;
}

bool SecCertificateIsValid(SecCertificateRef certificate,
	CFAbsoluteTime verifyTime) {
    return certificate && certificate->_notBefore <= verifyTime &&
		verifyTime <= certificate->_notAfter;
}

CFIndex SecCertificateVersion(SecCertificateRef certificate) {
	return certificate->_version + 1;
}

CFAbsoluteTime SecCertificateNotValidBefore(SecCertificateRef certificate) {
	return certificate->_notBefore;
}

CFAbsoluteTime SecCertificateNotValidAfter(SecCertificateRef certificate) {
	return certificate->_notAfter;
}

CFMutableArrayRef SecCertificateCopySummaryProperties(
    SecCertificateRef certificate, CFAbsoluteTime verifyTime) {
    CFAllocatorRef allocator = CFGetAllocator(certificate);
    CFMutableArrayRef summary = CFArrayCreateMutable(allocator, 0,
        &kCFTypeArrayCallBacks);

    /* First we put the subject summary name. */
    CFStringRef ssummary = SecCertificateCopySubjectSummary(certificate);
    if (ssummary) {
        appendProperty(summary, kSecPropertyTypeTitle,
            NULL, NULL, ssummary);
        CFRelease(ssummary);
    }

    /* Let see if this certificate is currently valid. */
    CFStringRef label;
    CFAbsoluteTime when;
    CFStringRef message;
    CFStringRef ptype;
    if (verifyTime > certificate->_notAfter) {
        label = SEC_EXPIRED_KEY;
        when = certificate->_notAfter;
        ptype = kSecPropertyTypeError;
        message = SEC_CERT_EXPIRED_KEY;
    } else if (certificate->_notBefore > verifyTime) {
        label = SEC_VALID_FROM_KEY;
        when = certificate->_notBefore;
        ptype = kSecPropertyTypeError;
        message = SEC_CERT_NOT_YET_VALID_KEY;
    } else {
        CFAbsoluteTime last = SecCertificateGetChainsLastValidity(certificate);
        CFAbsoluteTime first = SecCertificateGetChainsFirstValidity(certificate);
        if (verifyTime > last) {
            label = SEC_EXPIRED_KEY;
            when = last;
            ptype = kSecPropertyTypeError;
            message = SEC_ISSUER_EXPIRED_KEY;
        } else if (verifyTime < first) {
            label = SEC_VALID_FROM_KEY;
            when = first;
            ptype = kSecPropertyTypeError;
            message = SEC_ISSR_NOT_YET_VALID_KEY;
        } else {
            label = SEC_EXPIRES_KEY;
            when = certificate->_notAfter;
            ptype = kSecPropertyTypeSuccess;
            message = SEC_CERT_VALID_KEY;
        }
    }

    appendDateProperty(summary, label, when);
    CFStringRef lmessage = SecCopyCertString(message);
    appendProperty(summary, ptype, NULL, NULL, lmessage);
    CFRelease(lmessage);

	return summary;
}

CFArrayRef SecCertificateCopyProperties(SecCertificateRef certificate) {
	if (!certificate->_properties) {
		CFAllocatorRef allocator = CFGetAllocator(certificate);
		CFMutableArrayRef properties = CFArrayCreateMutable(allocator, 0,
			&kCFTypeArrayCallBacks);

        /* First we put the Subject Name in the property list. */
		CFArrayRef subject_plist = createPropertiesForX501NameContent(allocator,
                &certificate->_subject);
        appendProperty(properties, kSecPropertyTypeSection,
            SEC_SUBJECT_NAME_KEY, NULL, subject_plist);
		CFRelease(subject_plist);

		/* Next we put the Issuer Name in the property list. */
		CFArrayRef issuer_plist = createPropertiesForX501NameContent(allocator,
			&certificate->_issuer);
        appendProperty(properties, kSecPropertyTypeSection,
            SEC_ISSUER_NAME_KEY, NULL, issuer_plist);
		CFRelease(issuer_plist);

		/* Version */
        CFStringRef fmt = SecCopyCertString(SEC_CERT_VERSION_VALUE_KEY);
        CFStringRef versionString = CFStringCreateWithFormat(allocator,
            NULL, fmt, certificate->_version + 1);
        CFRelease(fmt);
        appendProperty(properties, kSecPropertyTypeString,
            SEC_VERSION_KEY, NULL, versionString);
        CFRelease(versionString);

		/* Serial Number */
        appendSerialNumberProperty(properties, SEC_SERIAL_NUMBER_KEY, &certificate->_serialNum);

        /* Validity dates. */
        appendValidityPeriodProperty(properties, SEC_VALIDITY_PERIOD_KEY, certificate);

        if (certificate->_subjectUniqueID.length) {
            appendDataProperty(properties, SEC_SUBJECT_UNIQUE_ID_KEY, NULL,
                &certificate->_subjectUniqueID);
        }
        if (certificate->_issuerUniqueID.length) {
            appendDataProperty(properties, SEC_ISSUER_UNIQUE_ID_KEY, NULL,
                &certificate->_issuerUniqueID);
        }

        appendPublicKeyProperty(properties, SEC_PUBLIC_KEY_KEY, certificate);

        CFIndex ix;
        for (ix = 0; ix < certificate->_extensionCount; ++ix) {
            appendExtension(properties, &certificate->_extensions[ix]);
        }

        /* Signature */
        appendSignatureProperty(properties, SEC_SIGNATURE_KEY, certificate);

        appendFingerprintsProperty(properties, SEC_FINGERPRINTS_KEY, certificate);

		certificate->_properties = properties;
	}

    CFRetain(certificate->_properties);
	return certificate->_properties;
}

#if TARGET_OS_OSX
/* On OS X, the SecCertificateCopySerialNumber API takes two arguments. */
CFDataRef SecCertificateCopySerialNumber(
	SecCertificateRef certificate,
	CFErrorRef *error) {
	if (!certificate) {
		if (error) {
			*error = CFErrorCreate(NULL, kCFErrorDomainOSStatus, errSecInvalidCertificate, NULL);
		}
		return NULL;
	}
	if (certificate->_serialNumber) {
		CFRetain(certificate->_serialNumber);
	}
	return certificate->_serialNumber;
}
#else
/* On iOS, the SecCertificateCopySerialNumber API takes one argument. */
CFDataRef SecCertificateCopySerialNumber(
		SecCertificateRef certificate) {
	if (certificate->_serialNumber) {
		CFRetain(certificate->_serialNumber);
	}
	return certificate->_serialNumber;
}
#endif

CFDataRef SecCertificateGetNormalizedIssuerContent(
    SecCertificateRef certificate) {
    return certificate->_normalizedIssuer;
}

CFDataRef SecCertificateGetNormalizedSubjectContent(
    SecCertificateRef certificate) {
    return certificate->_normalizedSubject;
}

/* Verify that certificate was signed by issuerKey. */
OSStatus SecCertificateIsSignedBy(SecCertificateRef certificate,
    SecKeyRef issuerKey) {
    /* Setup algId in SecAsn1AlgId format. */
    SecAsn1AlgId algId;
    algId.algorithm.Length = certificate->_tbsSigAlg.oid.length;
    algId.algorithm.Data = certificate->_tbsSigAlg.oid.data;
    algId.parameters.Length = certificate->_tbsSigAlg.params.length;
    algId.parameters.Data = certificate->_tbsSigAlg.params.data;

    /* RFC5280 4.1.1.2, 4.1.2.3 requires the actual signature algorithm
       must match the specified algorithm in the TBSCertificate. */
	bool sigAlgMatch = DEROidCompare(&certificate->_sigAlg.oid,
                         &certificate->_tbsSigAlg.oid);
    if (!sigAlgMatch) {
        secwarning("Signature algorithm mismatch in certificate (see RFC5280 4.1.1.2)");
    }

    CFErrorRef error = NULL;
    if (!sigAlgMatch ||
        !SecVerifySignatureWithPublicKey(issuerKey, &algId,
        certificate->_tbs.data, certificate->_tbs.length,
        certificate->_signature.data, certificate->_signature.length, &error))
    {
#if !defined(NDEBUG)
        secdebug("verify", "signature verify failed: %" PRIdOSStatus, (error) ? (OSStatus)CFErrorGetCode(error) : errSecNotSigner);
#endif
        CFReleaseSafe(error);
        return errSecNotSigner;
    }

    return errSecSuccess;
}

const DERItem * SecCertificateGetSubjectAltName(SecCertificateRef certificate) {
    if (!certificate->_subjectAltName) {
        return NULL;
    }
    return &certificate->_subjectAltName->extnValue;
}

static OSStatus appendIPAddressesFromGeneralNames(void *context,
	SecCEGeneralNameType gnType, const DERItem *generalName) {
	CFMutableArrayRef ipAddresses = (CFMutableArrayRef)context;
	if (gnType == GNT_IPAddress) {
		CFStringRef string = copyIPAddressContentDescription(
			kCFAllocatorDefault, generalName);
		if (string) {
			CFArrayAppendValue(ipAddresses, string);
			CFRelease(string);
		} else {
			return errSecInvalidCertificate;
		}
	}
	return errSecSuccess;
}

CFArrayRef SecCertificateCopyIPAddresses(SecCertificateRef certificate) {
	/* These can only exist in the subject alt name. */
	if (!certificate->_subjectAltName)
		return NULL;

	CFMutableArrayRef ipAddresses = CFArrayCreateMutable(kCFAllocatorDefault,
		0, &kCFTypeArrayCallBacks);
	OSStatus status = SecCertificateParseGeneralNames(&certificate->_subjectAltName->extnValue,
		ipAddresses, appendIPAddressesFromGeneralNames);
	if (status || CFArrayGetCount(ipAddresses) == 0) {
		CFRelease(ipAddresses);
		ipAddresses = NULL;
	}
	return ipAddresses;
}

static OSStatus appendDNSNamesFromGeneralNames(void *context, SecCEGeneralNameType gnType,
	const DERItem *generalName) {
	CFMutableArrayRef dnsNames = (CFMutableArrayRef)context;
	if (gnType == GNT_DNSName) {
		CFStringRef string = CFStringCreateWithBytes(kCFAllocatorDefault,
			generalName->data, generalName->length,
			kCFStringEncodingUTF8, FALSE);
		if (string) {
			CFArrayAppendValue(dnsNames, string);
			CFRelease(string);
		} else {
			return errSecInvalidCertificate;
		}
	}
	return errSecSuccess;
}

/* Return true if the passed in string matches the
   Preferred name syntax from sections 2.3.1. in RFC 1035.
   With the added check that we disallow empty dns names.
   Also in order to support wildcard DNSNames we allow for the '*'
   character anywhere in a dns component where we currently allow
   a letter.

	<domain> ::= <subdomain> | " "

	<subdomain> ::= <label> | <subdomain> "." <label>

	<label> ::= <letter> [ [ <ldh-str> ] <let-dig> ]

	<ldh-str> ::= <let-dig-hyp> | <let-dig-hyp> <ldh-str>

	<let-dig-hyp> ::= <let-dig> | "-"

	<let-dig> ::= <letter> | <digit>

	<letter> ::= any one of the 52 alphabetic characters A through Z in
	upper case and a through z in lower case

	<digit> ::= any one of the ten digits 0 through 9
   */
static bool isDNSName(CFStringRef string) {
    CFStringInlineBuffer buf = {};
	CFIndex ix, labelLength = 0, length = CFStringGetLength(string);
	/* From RFC 1035 2.3.4. Size limits:
	   labels          63 octets or less
	   names           255 octets or less */
	require_quiet(length <= 255, notDNS);
	CFRange range = { 0, length };
	CFStringInitInlineBuffer(string, &buf, range);
	enum {
		kDNSStateInital,
		kDNSStateAfterDot,
		kDNSStateAfterAlpha,
		kDNSStateAfterDigit,
		kDNSStateAfterDash,
	} state = kDNSStateInital;

	for (ix = 0; ix < length; ++ix) {
		UniChar ch = CFStringGetCharacterFromInlineBuffer(&buf, ix);
		labelLength++;
		if (ch == '.') {
			require_quiet(labelLength <= 64 &&
				(state == kDNSStateAfterAlpha || state == kDNSStateAfterDigit),
				notDNS);
			state = kDNSStateAfterDot;
			labelLength = 0;
		} else if (('A' <= ch && ch <= 'Z') || ('a' <= ch && ch <= 'z')  ||
			ch == '*') {
			state = kDNSStateAfterAlpha;
		} else if ('0' <= ch && ch <= '9') {
#if 0
			/* The requirement for labels to start with a letter was
			   dropped so we don't check this anymore.  */
			require_quiet(state == kDNSStateAfterAlpha ||
				state == kDNSStateAfterDigit ||
				state == kDNSStateAfterDash, notDNS);
#endif
			state = kDNSStateAfterDigit;
		} else if (ch == '-') {
			require_quiet(state == kDNSStateAfterAlpha ||
				state == kDNSStateAfterDigit ||
				state == kDNSStateAfterDash, notDNS);
			state = kDNSStateAfterDash;
		} else {
			goto notDNS;
		}
	}

	/* We don't allow a dns name to end in a dot or dash.  */
	require_quiet(labelLength <= 63 &&
		(state == kDNSStateAfterAlpha || state == kDNSStateAfterDigit),
		notDNS);

	return true;
notDNS:
	return false;
}

static OSStatus appendDNSNamesFromX501Name(void *context, const DERItem *type,
	const DERItem *value, CFIndex rdnIX) {
	CFMutableArrayRef dnsNames = (CFMutableArrayRef)context;
	if (DEROidCompare(type, &oidCommonName)) {
		CFStringRef string = copyDERThingDescription(kCFAllocatorDefault,
			value, true);
		if (string) {
			if (isDNSName(string)) {
				/* We found a common name that is formatted like a valid
				   dns name. */
				CFArrayAppendValue(dnsNames, string);
			}
			CFRelease(string);
		} else {
			return errSecInvalidCertificate;
		}
	}
	return errSecSuccess;
}

/* Not everything returned by this function is going to be a proper DNS name,
   we also return the certificates common name entries from the subject,
   assuming they look like dns names as specified in RFC 1035. */
CFArrayRef SecCertificateCopyDNSNames(SecCertificateRef certificate) {
	/* These can exist in the subject alt name or in the subject. */
	CFMutableArrayRef dnsNames = CFArrayCreateMutable(kCFAllocatorDefault,
		0, &kCFTypeArrayCallBacks);
	OSStatus status = errSecSuccess;
	if (certificate->_subjectAltName) {
		status = SecCertificateParseGeneralNames(&certificate->_subjectAltName->extnValue,
			dnsNames, appendDNSNamesFromGeneralNames);
	}
	/* RFC 2818 section 3.1.  Server Identity
	  [...]
	  If a subjectAltName extension of type dNSName is present, that MUST
	  be used as the identity. Otherwise, the (most specific) Common Name
	  field in the Subject field of the certificate MUST be used. Although
	  the use of the Common Name is existing practice, it is deprecated and
	  Certification Authorities are encouraged to use the dNSName instead.
	  [...]

	  This implies that if we found 1 or more DNSNames in the
	  subjectAltName, we should not use the Common Name of the subject as
	  a DNSName.
	*/
	if (!status && CFArrayGetCount(dnsNames) == 0) {
		status = parseX501NameContent(&certificate->_subject, dnsNames,
			appendDNSNamesFromX501Name);
	}
	if (status || CFArrayGetCount(dnsNames) == 0) {
		CFRelease(dnsNames);
		dnsNames = NULL;
	}
	return dnsNames;
}

static OSStatus appendRFC822NamesFromGeneralNames(void *context,
	SecCEGeneralNameType gnType, const DERItem *generalName) {
	CFMutableArrayRef dnsNames = (CFMutableArrayRef)context;
	if (gnType == GNT_RFC822Name) {
		CFStringRef string = CFStringCreateWithBytes(kCFAllocatorDefault,
			generalName->data, generalName->length,
			kCFStringEncodingASCII, FALSE);
		if (string) {
			CFArrayAppendValue(dnsNames, string);
			CFRelease(string);
		} else {
			return errSecInvalidCertificate;
		}
	}
	return errSecSuccess;
}

static OSStatus appendRFC822NamesFromX501Name(void *context, const DERItem *type,
	const DERItem *value, CFIndex rdnIX) {
	CFMutableArrayRef dnsNames = (CFMutableArrayRef)context;
	if (DEROidCompare(type, &oidEmailAddress)) {
		CFStringRef string = copyDERThingDescription(kCFAllocatorDefault,
			value, true);
		if (string) {
			CFArrayAppendValue(dnsNames, string);
			CFRelease(string);
		} else {
			return errSecInvalidCertificate;
		}
	}
	return errSecSuccess;
}

CFArrayRef SecCertificateCopyRFC822Names(SecCertificateRef certificate) {
	/* These can exist in the subject alt name or in the subject. */
	CFMutableArrayRef rfc822Names = CFArrayCreateMutable(kCFAllocatorDefault,
		0, &kCFTypeArrayCallBacks);
	OSStatus status = errSecSuccess;
	if (certificate->_subjectAltName) {
		status = SecCertificateParseGeneralNames(&certificate->_subjectAltName->extnValue,
			rfc822Names, appendRFC822NamesFromGeneralNames);
	}
	if (!status) {
		status = parseX501NameContent(&certificate->_subject, rfc822Names,
			appendRFC822NamesFromX501Name);
	}
	if (status || CFArrayGetCount(rfc822Names) == 0) {
		CFRelease(rfc822Names);
		rfc822Names = NULL;
	}
	return rfc822Names;
}

OSStatus SecCertificateCopyEmailAddresses(SecCertificateRef certificate, CFArrayRef * __nonnull CF_RETURNS_RETAINED emailAddresses) {
    if (!certificate || !emailAddresses) {
        return errSecParam;
    }
    *emailAddresses = SecCertificateCopyRFC822Names(certificate);
    return errSecSuccess;
}

static OSStatus appendCommonNamesFromX501Name(void *context,
    const DERItem *type, const DERItem *value, CFIndex rdnIX) {
	CFMutableArrayRef commonNames = (CFMutableArrayRef)context;
	if (DEROidCompare(type, &oidCommonName)) {
		CFStringRef string = copyDERThingDescription(kCFAllocatorDefault,
			value, true);
		if (string) {
            CFArrayAppendValue(commonNames, string);
			CFRelease(string);
		} else {
			return errSecInvalidCertificate;
		}
	}
	return errSecSuccess;
}

CFArrayRef SecCertificateCopyCommonNames(SecCertificateRef certificate) {
	CFMutableArrayRef commonNames = CFArrayCreateMutable(kCFAllocatorDefault,
		0, &kCFTypeArrayCallBacks);
	OSStatus status;
    status = parseX501NameContent(&certificate->_subject, commonNames,
        appendCommonNamesFromX501Name);
	if (status || CFArrayGetCount(commonNames) == 0) {
		CFRelease(commonNames);
		commonNames = NULL;
	}
	return commonNames;
}

OSStatus SecCertificateCopyCommonName(SecCertificateRef certificate, CFStringRef *commonName)
{
    if (!certificate) {
        return errSecParam;
    }
    CFArrayRef commonNames = SecCertificateCopyCommonNames(certificate);
    if (!commonNames) {
        return errSecInternal;
    }

    if (commonName) {
        CFIndex count = CFArrayGetCount(commonNames);
        *commonName = CFRetainSafe(CFArrayGetValueAtIndex(commonNames, count-1));
    }
    CFReleaseSafe(commonNames);
    return errSecSuccess;
}

static OSStatus appendOrganizationFromX501Name(void *context,
	const DERItem *type, const DERItem *value, CFIndex rdnIX) {
	CFMutableArrayRef organization = (CFMutableArrayRef)context;
	if (DEROidCompare(type, &oidOrganizationName)) {
		CFStringRef string = copyDERThingDescription(kCFAllocatorDefault,
			value, true);
		if (string) {
			CFArrayAppendValue(organization, string);
			CFRelease(string);
		} else {
			return errSecInvalidCertificate;
		}
	}
	return errSecSuccess;
}

CFArrayRef SecCertificateCopyOrganization(SecCertificateRef certificate) {
	CFMutableArrayRef organization = CFArrayCreateMutable(kCFAllocatorDefault,
		0, &kCFTypeArrayCallBacks);
	OSStatus status;
	status = parseX501NameContent(&certificate->_subject, organization,
        appendOrganizationFromX501Name);
	if (status || CFArrayGetCount(organization) == 0) {
		CFRelease(organization);
		organization = NULL;
	}
	return organization;
}

static OSStatus appendOrganizationalUnitFromX501Name(void *context,
	const DERItem *type, const DERItem *value, CFIndex rdnIX) {
	CFMutableArrayRef organizationalUnit = (CFMutableArrayRef)context;
	if (DEROidCompare(type, &oidOrganizationalUnitName)) {
		CFStringRef string = copyDERThingDescription(kCFAllocatorDefault,
			value, true);
		if (string) {
			CFArrayAppendValue(organizationalUnit, string);
			CFRelease(string);
		} else {
			return errSecInvalidCertificate;
		}
	}
	return errSecSuccess;
}

CFArrayRef SecCertificateCopyOrganizationalUnit(SecCertificateRef certificate) {
	CFMutableArrayRef organizationalUnit = CFArrayCreateMutable(kCFAllocatorDefault,
		0, &kCFTypeArrayCallBacks);
	OSStatus status;
	status = parseX501NameContent(&certificate->_subject, organizationalUnit,
        appendOrganizationalUnitFromX501Name);
	if (status || CFArrayGetCount(organizationalUnit) == 0) {
		CFRelease(organizationalUnit);
		organizationalUnit = NULL;
	}
	return organizationalUnit;
}

static OSStatus appendCountryFromX501Name(void *context,
                                                     const DERItem *type, const DERItem *value, CFIndex rdnIX) {
    CFMutableArrayRef countries = (CFMutableArrayRef)context;
    if (DEROidCompare(type, &oidCountryName)) {
        CFStringRef string = copyDERThingDescription(kCFAllocatorDefault,
                                                     value, true);
        if (string) {
            CFArrayAppendValue(countries, string);
            CFRelease(string);
        } else {
            return errSecInvalidCertificate;
        }
    }
    return errSecSuccess;
}

CFArrayRef SecCertificateCopyCountry(SecCertificateRef certificate) {
    CFMutableArrayRef countries = CFArrayCreateMutable(kCFAllocatorDefault,
                                                                0, &kCFTypeArrayCallBacks);
    OSStatus status;
    status = parseX501NameContent(&certificate->_subject, countries,
                                  appendCountryFromX501Name);
    if (status || CFArrayGetCount(countries) == 0) {
        CFRelease(countries);
        countries = NULL;
    }
    return countries;
}

const SecCEBasicConstraints *
SecCertificateGetBasicConstraints(SecCertificateRef certificate) {
	if (certificate->_basicConstraints.present)
		return &certificate->_basicConstraints;
	else
		return NULL;
}

CFArrayRef SecCertificateGetPermittedSubtrees(SecCertificateRef certificate) {
    return (certificate->_permittedSubtrees);
}

CFArrayRef SecCertificateGetExcludedSubtrees(SecCertificateRef certificate) {
    return (certificate->_excludedSubtrees);
}

const SecCEPolicyConstraints *
SecCertificateGetPolicyConstraints(SecCertificateRef certificate) {
	if (certificate->_policyConstraints.present)
		return &certificate->_policyConstraints;
	else
		return NULL;
}

const SecCEPolicyMappings *
SecCertificateGetPolicyMappings(SecCertificateRef certificate) {
    if (certificate->_policyMappings.present) {
        return &certificate->_policyMappings;
    } else {
        return NULL;
    }
}

const SecCECertificatePolicies *
SecCertificateGetCertificatePolicies(SecCertificateRef certificate) {
	if (certificate->_certificatePolicies.present)
		return &certificate->_certificatePolicies;
	else
		return NULL;
}

const SecCEInhibitAnyPolicy *
SecCertificateGetInhibitAnyPolicySkipCerts(SecCertificateRef certificate) {
    if (certificate->_inhibitAnyPolicySkipCerts.present) {
        return &certificate->_inhibitAnyPolicySkipCerts;
    } else {
        return NULL;
    }
}

static OSStatus appendNTPrincipalNamesFromGeneralNames(void *context,
	SecCEGeneralNameType gnType, const DERItem *generalName) {
	CFMutableArrayRef ntPrincipalNames = (CFMutableArrayRef)context;
	if (gnType == GNT_OtherName) {
        DEROtherName on;
        DERReturn drtn = DERParseSequenceContent(generalName,
            DERNumOtherNameItemSpecs, DEROtherNameItemSpecs,
            &on, sizeof(on));
        require_noerr_quiet(drtn, badDER);
        if (DEROidCompare(&on.typeIdentifier, &oidMSNTPrincipalName)) {
            CFStringRef string;
            require_quiet(string = copyDERThingDescription(kCFAllocatorDefault,
                &on.value, true), badDER);
            CFArrayAppendValue(ntPrincipalNames, string);
            CFRelease(string);
		}
	}
	return errSecSuccess;

badDER:
    return errSecInvalidCertificate;

}

CFArrayRef SecCertificateCopyNTPrincipalNames(SecCertificateRef certificate) {
	CFMutableArrayRef ntPrincipalNames = CFArrayCreateMutable(kCFAllocatorDefault,
		0, &kCFTypeArrayCallBacks);
	OSStatus status = errSecSuccess;
	if (certificate->_subjectAltName) {
		status = SecCertificateParseGeneralNames(&certificate->_subjectAltName->extnValue,
			ntPrincipalNames, appendNTPrincipalNamesFromGeneralNames);
	}
	if (status || CFArrayGetCount(ntPrincipalNames) == 0) {
		CFRelease(ntPrincipalNames);
		ntPrincipalNames = NULL;
	}
	return ntPrincipalNames;
}

static OSStatus appendToRFC2253String(void *context,
	const DERItem *type, const DERItem *value, CFIndex rdnIX) {
	CFMutableStringRef string = (CFMutableStringRef)context;
    /*
                    CN      commonName
                    L       localityName
                    ST      stateOrProvinceName
                    O       organizationName
                    OU      organizationalUnitName
                    C       countryName
                    STREET  streetAddress
                    DC      domainComponent
                    UID     userid
    */
    /* Prepend a + if this is not the first RDN in an RDN set.
       Otherwise prepend a , if this is not the first RDN. */
    if (rdnIX > 0)
        CFStringAppend(string, CFSTR("+"));
    else if (CFStringGetLength(string)) {
        CFStringAppend(string, CFSTR(","));
    }

    CFStringRef label, oid = NULL;
    /* @@@ Consider changing this to a dictionary lookup keyed by the
       decimal representation. */
	if (DEROidCompare(type, &oidCommonName)) {
        label = CFSTR("CN");
    } else if (DEROidCompare(type, &oidLocalityName)) {
        label = CFSTR("L");
    } else if (DEROidCompare(type, &oidStateOrProvinceName)) {
        label = CFSTR("ST");
    } else if (DEROidCompare(type, &oidOrganizationName)) {
        label = CFSTR("O");
    } else if (DEROidCompare(type, &oidOrganizationalUnitName)) {
        label = CFSTR("OU");
    } else if (DEROidCompare(type, &oidCountryName)) {
        label = CFSTR("C");
#if 0
    } else if (DEROidCompare(type, &oidStreetAddress)) {
        label = CFSTR("STREET");
    } else if (DEROidCompare(type, &oidDomainComponent)) {
        label = CFSTR("DC");
    } else if (DEROidCompare(type, &oidUserID)) {
        label = CFSTR("UID");
#endif
    } else {
        label = oid = SecDERItemCopyOIDDecimalRepresentation(kCFAllocatorDefault, type);
    }

    CFStringAppend(string, label);
    CFStringAppend(string, CFSTR("="));
    CFStringRef raw = NULL;
    if (!oid)
        raw = copyDERThingDescription(kCFAllocatorDefault, value, true);

    if (raw) {
        /* Append raw to string while escaping:
           a space or "#" character occurring at the beginning of the string
           a space character occurring at the end of the string
           one of the characters ",", "+", """, "\", "<", ">" or ";"
        */
        CFStringInlineBuffer buffer = {};
        CFIndex ix, length = CFStringGetLength(raw);
        CFRange range = { 0, length };
        CFStringInitInlineBuffer(raw, &buffer, range);
        for (ix = 0; ix < length; ++ix) {
            UniChar ch = CFStringGetCharacterFromInlineBuffer(&buffer, ix);
            if (ch < 0x20) {
                CFStringAppendFormat(string, NULL, CFSTR("\\%02X"), ch);
            } else if (ch == ',' || ch == '+' || ch == '"' || ch == '\\' ||
                ch == '<' || ch == '>' || ch == ';' ||
                (ch == ' ' && (ix == 0 || ix == length - 1)) ||
                (ch == '#' && ix == 0)) {
                UniChar chars[] = { '\\', ch };
                CFStringAppendCharacters(string, chars, 2);
            } else {
                CFStringAppendCharacters(string, &ch, 1);
            }
        }
        CFRelease(raw);
    } else {
        /* Append the value in hex. */
        CFStringAppend(string, CFSTR("#"));
        DERSize ix;
        for (ix = 0; ix < value->length; ++ix)
            CFStringAppendFormat(string, NULL, CFSTR("%02X"), value->data[ix]);
    }

    CFReleaseSafe(oid);

	return errSecSuccess;
}

CFStringRef SecCertificateCopySubjectString(SecCertificateRef certificate) {
	CFMutableStringRef string = CFStringCreateMutable(kCFAllocatorDefault, 0);
	OSStatus status = parseX501NameContent(&certificate->_subject, string, appendToRFC2253String);
	if (status || CFStringGetLength(string) == 0) {
		CFRelease(string);
		string = NULL;
	}
	return string;
}

static OSStatus appendToCompanyNameString(void *context,
	const DERItem *type, const DERItem *value, CFIndex rdnIX) {
	CFMutableStringRef string = (CFMutableStringRef)context;
    if (CFStringGetLength(string) != 0)
        return errSecSuccess;

    if (!DEROidCompare(type, &oidOrganizationName))
        return errSecSuccess;

    CFStringRef raw;
    raw = copyDERThingDescription(kCFAllocatorDefault, value, true);
    if (!raw)
        return errSecSuccess;
    CFStringAppend(string, raw);
    CFRelease(raw);

	return errSecSuccess;
}

CFStringRef SecCertificateCopyCompanyName(SecCertificateRef certificate) {
	CFMutableStringRef string = CFStringCreateMutable(kCFAllocatorDefault, 0);
	OSStatus status = parseX501NameContent(&certificate->_subject, string,
        appendToCompanyNameString);
	if (status || CFStringGetLength(string) == 0) {
		CFRelease(string);
		string = NULL;
	}
	return string;
}

static CFDataRef SecDERItemCopySequence(DERItem *content) {
    DERSize seq_len_length = DERLengthOfLength(content->length);
    size_t sequence_length = 1 + seq_len_length + content->length;
	CFMutableDataRef sequence = CFDataCreateMutable(kCFAllocatorDefault,
        sequence_length);
	CFDataSetLength(sequence, sequence_length);
	uint8_t *sequence_ptr = CFDataGetMutableBytePtr(sequence);
    *sequence_ptr++ = ONE_BYTE_ASN1_CONSTR_SEQUENCE;
    require_noerr_quiet(DEREncodeLength(content->length,
        sequence_ptr, &seq_len_length), out);
    sequence_ptr += seq_len_length;
    memcpy(sequence_ptr, content->data, content->length);
	return sequence;
out:
    CFReleaseSafe(sequence);
    return NULL;
}

CFDataRef SecCertificateCopyIssuerSequence(
    SecCertificateRef certificate) {
    return SecDERItemCopySequence(&certificate->_issuer);
}

CFDataRef SecCertificateCopySubjectSequence(
    SecCertificateRef certificate) {
    return SecDERItemCopySequence(&certificate->_subject);
}

CFDataRef SecCertificateCopyNormalizedIssuerSequence(SecCertificateRef certificate) {
    if (!certificate || !certificate->_normalizedIssuer) {
        return NULL;
    }
    DERItem tmpItem;
    tmpItem.data = (void *)CFDataGetBytePtr(certificate->_normalizedIssuer);
    tmpItem.length = CFDataGetLength(certificate->_normalizedIssuer);

    return SecDERItemCopySequence(&tmpItem);
}

CFDataRef SecCertificateCopyNormalizedSubjectSequence(SecCertificateRef certificate) {
    if (!certificate || !certificate->_normalizedSubject) {
        return NULL;
    }
    DERItem tmpItem;
    tmpItem.data = (void *)CFDataGetBytePtr(certificate->_normalizedSubject);
    tmpItem.length = CFDataGetLength(certificate->_normalizedSubject);

    return SecDERItemCopySequence(&tmpItem);
}

const DERAlgorithmId *SecCertificateGetPublicKeyAlgorithm(
	SecCertificateRef certificate) {
	return &certificate->_algId;
}

const DERItem *SecCertificateGetPublicKeyData(SecCertificateRef certificate) {
	return &certificate->_pubKeyDER;
}

#if TARGET_OS_OSX
/* There is already a SecCertificateCopyPublicKey with different args on OS X,
   so we will refer to this one internally as SecCertificateCopyPublicKey_ios.
 */
__nullable SecKeyRef SecCertificateCopyPublicKey_ios(SecCertificateRef certificate)
#else
__nullable SecKeyRef SecCertificateCopyPublicKey(SecCertificateRef certificate)
#endif
{
    if (certificate->_pubKey == NULL) {
        const DERAlgorithmId *algId =
        SecCertificateGetPublicKeyAlgorithm(certificate);
        const DERItem *keyData = SecCertificateGetPublicKeyData(certificate);
        const DERItem *params = NULL;
        if (algId->params.length != 0) {
            params = &algId->params;
        }
        SecAsn1Oid oid1 = { .Data = algId->oid.data, .Length = algId->oid.length };
        SecAsn1Item params1 = {
            .Data = params ? params->data : NULL,
            .Length = params ? params->length : 0
        };
        SecAsn1Item keyData1 = {
            .Data = keyData ? keyData->data : NULL,
            .Length = keyData ? keyData->length : 0
        };
        certificate->_pubKey = SecKeyCreatePublicFromDER(kCFAllocatorDefault, &oid1, &params1,
                                                         &keyData1);
    }

    return CFRetainSafe(certificate->_pubKey);
}

bool SecCertificateIsWeakKey(SecCertificateRef certificate) {
    bool weak = true;
    SecKeyRef pubKey = NULL;
#if TARGET_OS_OSX
    require_quiet(pubKey = SecCertificateCopyPublicKey_ios(certificate), out);
#else
    require_quiet(pubKey = SecCertificateCopyPublicKey(certificate) ,out);
#endif
    size_t size = SecKeyGetBlockSize(pubKey);
    switch (SecKeyGetAlgorithmIdentifier(pubKey)) {
        case kSecRSAAlgorithmID:
            if (MIN_RSA_KEY_SIZE <= size) weak = false;
            break;
        case kSecECDSAAlgorithmID:
            if (MIN_EC_KEY_SIZE <= size) weak = false;
            break;
        default:
            weak = true;
    }

out:
    CFReleaseSafe(pubKey);
    return weak;
}

bool SecCertificateIsWeakHash(SecCertificateRef certificate) {
    SecSignatureHashAlgorithm certAlg = 0;
    certAlg = SecCertificateGetSignatureHashAlgorithm(certificate);
    if (certAlg == kSecSignatureHashAlgorithmUnknown ||
        certAlg == kSecSignatureHashAlgorithmMD2 ||
        certAlg == kSecSignatureHashAlgorithmMD4 ||
        certAlg == kSecSignatureHashAlgorithmMD5 ||
        certAlg == kSecSignatureHashAlgorithmSHA1) {
        return true;
    }
    return false;
}

bool SecCertificateIsAtLeastMinKeySize(SecCertificateRef certificate,
                                       CFDictionaryRef keySizes) {
    bool goodSize = false;
    SecKeyRef pubKey = NULL;
#if TARGET_OS_OSX
    require_quiet(pubKey = SecCertificateCopyPublicKey_ios(certificate), out);
#else
    require_quiet(pubKey = SecCertificateCopyPublicKey(certificate) ,out);
#endif
    size_t size = SecKeyGetBlockSize(pubKey);
    CFNumberRef minSize;
    size_t minSizeInBits;
    switch (SecKeyGetAlgorithmIdentifier(pubKey)) {
        case kSecRSAAlgorithmID:
            if(CFDictionaryGetValueIfPresent(keySizes, kSecAttrKeyTypeRSA, (const void**)&minSize)
               && minSize && CFNumberGetValue(minSize, kCFNumberLongType, &minSizeInBits)) {
                if (size >= (size_t)(minSizeInBits+7)/8) goodSize = true;
            }
            break;
        case kSecECDSAAlgorithmID:
            if(CFDictionaryGetValueIfPresent(keySizes, kSecAttrKeyTypeEC, (const void**)&minSize)
               && minSize && CFNumberGetValue(minSize, kCFNumberLongType, &minSizeInBits)) {
                if (size >= (size_t)(minSizeInBits+7)/8) goodSize = true;
            }
            break;
        default:
            goodSize = false;
    }
out:
    CFReleaseSafe(pubKey);
    return goodSize;
}

CFDataRef SecCertificateGetSHA1Digest(SecCertificateRef certificate) {
    if (!certificate || !certificate->_der.data) {
        return NULL;
    }
    if (!certificate->_sha1Digest) {
        certificate->_sha1Digest =
            SecSHA1DigestCreate(CFGetAllocator(certificate),
                certificate->_der.data, certificate->_der.length);
    }
    return certificate->_sha1Digest;
}

CFDataRef SecCertificateCopySHA256Digest(SecCertificateRef certificate) {
    if (!certificate || !certificate->_der.data) {
        return NULL;
    }
    return SecSHA256DigestCreate(CFGetAllocator(certificate),
                                 certificate->_der.data, certificate->_der.length);
}

CFDataRef SecCertificateCopyIssuerSHA1Digest(SecCertificateRef certificate) {
    CFDataRef digest = NULL;
    CFDataRef issuer = SecCertificateCopyIssuerSequence(certificate);
    if (issuer) {
        digest = SecSHA1DigestCreate(kCFAllocatorDefault,
            CFDataGetBytePtr(issuer), CFDataGetLength(issuer));
        CFRelease(issuer);
    }
    return digest;
}

CFDataRef SecCertificateCopyPublicKeySHA1Digest(SecCertificateRef certificate) {
    if (!certificate || !certificate->_pubKeyDER.data) {
        return NULL;
    }
    return SecSHA1DigestCreate(CFGetAllocator(certificate),
        certificate->_pubKeyDER.data, certificate->_pubKeyDER.length);
}

CFDataRef SecCertificateCopySubjectPublicKeyInfoSHA1Digest(SecCertificateRef certificate) {
    if (!certificate || !certificate->_subjectPublicKeyInfo.data) {
        return NULL;
    }
    return SecSHA1DigestCreate(CFGetAllocator(certificate),
                               certificate->_subjectPublicKeyInfo.data, certificate->_subjectPublicKeyInfo.length);
}

CFDataRef SecCertificateCopySubjectPublicKeyInfoSHA256Digest(SecCertificateRef certificate) {
    if (!certificate || !certificate->_subjectPublicKeyInfo.data) {
        return NULL;
    }
    return SecSHA256DigestCreate(CFGetAllocator(certificate),
                                 certificate->_subjectPublicKeyInfo.data, certificate->_subjectPublicKeyInfo.length);
}

CFTypeRef SecCertificateCopyKeychainItem(SecCertificateRef certificate)
{
	if (!certificate) {
		return NULL;
	}
	CFRetainSafe(certificate->_keychain_item);
	return certificate->_keychain_item;
}

CFDataRef SecCertificateGetAuthorityKeyID(SecCertificateRef certificate) {
	if (!certificate) {
		return NULL;
	}
	if (!certificate->_authorityKeyID &&
		certificate->_authorityKeyIdentifier.length) {
		certificate->_authorityKeyID = CFDataCreate(kCFAllocatorDefault,
			certificate->_authorityKeyIdentifier.data,
			certificate->_authorityKeyIdentifier.length);
	}

    return certificate->_authorityKeyID;
}

CFDataRef SecCertificateGetSubjectKeyID(SecCertificateRef certificate) {
	if (!certificate) {
		return NULL;
	}
	if (!certificate->_subjectKeyID &&
		certificate->_subjectKeyIdentifier.length) {
		certificate->_subjectKeyID = CFDataCreate(kCFAllocatorDefault,
			certificate->_subjectKeyIdentifier.data,
			certificate->_subjectKeyIdentifier.length);
	}

    return certificate->_subjectKeyID;
}

CFArrayRef SecCertificateGetCRLDistributionPoints(SecCertificateRef certificate) {
    if (!certificate) {
        return NULL;
    }
    return certificate->_crlDistributionPoints;
}

CFArrayRef SecCertificateGetOCSPResponders(SecCertificateRef certificate) {
    if (!certificate) {
        return NULL;
    }
    return certificate->_ocspResponders;
}

CFArrayRef SecCertificateGetCAIssuers(SecCertificateRef certificate) {
    if (!certificate) {
        return NULL;
    }
    return certificate->_caIssuers;
}

bool SecCertificateHasCriticalSubjectAltName(SecCertificateRef certificate) {
    if (!certificate) {
        return false;
    }
    return certificate->_subjectAltName &&
        certificate->_subjectAltName->critical;
}

bool SecCertificateHasSubject(SecCertificateRef certificate) {
    if (!certificate) {
        return false;
    }
	/* Since the _subject field is the content of the subject and not the
	   whole thing, we can simply check for a 0 length subject here. */
	return certificate->_subject.length != 0;
}

bool SecCertificateHasUnknownCriticalExtension(SecCertificateRef certificate) {
    if (!certificate) {
        return false;
    }
    return certificate->_foundUnknownCriticalExtension;
}

/* Private API functions. */
void SecCertificateShow(SecCertificateRef certificate) {
	check(certificate);
	fprintf(stderr, "SecCertificate instance %p:\n", certificate);
		fprintf(stderr, "\n");
}

#ifndef STANDALONE
CFDictionaryRef SecCertificateCopyAttributeDictionary(
	SecCertificateRef certificate) {
	CFAllocatorRef allocator = CFGetAllocator(certificate);
	CFNumberRef certificateType, certificateEncoding;
	CFStringRef label, alias;
	CFDataRef skid, pubKeyDigest, certData;
	CFDictionaryRef dict = NULL;

	DICT_DECLARE(11);

	/* CSSM_CERT_X_509v1, CSSM_CERT_X_509v2 or CSSM_CERT_X_509v3 */
	SInt32 ctv = certificate->_version + 1;
	SInt32 cev = 3; /* CSSM_CERT_ENCODING_DER */
	certificateType = CFNumberCreate(allocator, kCFNumberSInt32Type, &ctv);
	certificateEncoding = CFNumberCreate(allocator, kCFNumberSInt32Type, &cev);
	certData = SecCertificateCopyData(certificate);
	skid = SecCertificateGetSubjectKeyID(certificate);
	pubKeyDigest = SecSHA1DigestCreate(allocator, certificate->_pubKeyDER.data,
		certificate->_pubKeyDER.length);
#if 0
	/* We still need to figure out how to deal with multi valued attributes. */
	alias = SecCertificateCopyRFC822Names(certificate);
	label = SecCertificateCopySubjectSummary(certificate);
#else
	alias = NULL;
	label = NULL;
#endif

	DICT_ADDPAIR(kSecClass, kSecClassCertificate);
	DICT_ADDPAIR(kSecAttrCertificateType, certificateType);
	DICT_ADDPAIR(kSecAttrCertificateEncoding, certificateEncoding);
	if (label)
		DICT_ADDPAIR(kSecAttrLabel, label);
	if (alias)
		DICT_ADDPAIR(kSecAttrAlias, alias);
	DICT_ADDPAIR(kSecAttrSubject, certificate->_normalizedSubject);
	DICT_ADDPAIR(kSecAttrIssuer, certificate->_normalizedIssuer);
	DICT_ADDPAIR(kSecAttrSerialNumber, certificate->_serialNumber);
	if (skid)
		DICT_ADDPAIR(kSecAttrSubjectKeyID, skid);
	DICT_ADDPAIR(kSecAttrPublicKeyHash, pubKeyDigest);
	DICT_ADDPAIR(kSecValueData, certData);
    dict = DICT_CREATE(allocator);

	CFReleaseSafe(label);
	CFReleaseSafe(pubKeyDigest);
	CFReleaseSafe(certData);
	CFReleaseSafe(certificateEncoding);
	CFReleaseSafe(certificateType);

	return dict;
}

SecCertificateRef SecCertificateCreateFromAttributeDictionary(
	CFDictionaryRef refAttributes) {
	/* @@@ Support having an allocator in refAttributes. */
 	CFAllocatorRef allocator = NULL;
	CFDataRef data = CFDictionaryGetValue(refAttributes, kSecValueData);
	return data ? SecCertificateCreateWithData(allocator, data) : NULL;
}
#endif

static bool _SecCertificateIsSelfSigned(SecCertificateRef certificate) {
    if (certificate->_isSelfSigned == kSecSelfSignedUnknown) {
        certificate->_isSelfSigned = kSecSelfSignedFalse;
        SecKeyRef publicKey = NULL;
        require(certificate && (CFGetTypeID(certificate) == SecCertificateGetTypeID()), out);
#if TARGET_OS_OSX
        require(publicKey = SecCertificateCopyPublicKey_ios(certificate), out);
#else
        require(publicKey = SecCertificateCopyPublicKey(certificate), out);
#endif
        CFDataRef normalizedIssuer =
        SecCertificateGetNormalizedIssuerContent(certificate);
        CFDataRef normalizedSubject =
        SecCertificateGetNormalizedSubjectContent(certificate);
        require_quiet(normalizedIssuer && normalizedSubject &&
                      CFEqual(normalizedIssuer, normalizedSubject), out);

        CFDataRef authorityKeyID = SecCertificateGetAuthorityKeyID(certificate);
        CFDataRef subjectKeyID = SecCertificateGetSubjectKeyID(certificate);
        if (authorityKeyID) {
            require_quiet(subjectKeyID && CFEqual(subjectKeyID, authorityKeyID), out);
        }

        require_noerr_quiet(SecCertificateIsSignedBy(certificate, publicKey), out);

        certificate->_isSelfSigned = kSecSelfSignedTrue;
    out:
        CFReleaseSafe(publicKey);
    }

    return (certificate->_isSelfSigned == kSecSelfSignedTrue);
}

bool SecCertificateIsCA(SecCertificateRef certificate) {
    bool result = false;
    require(certificate && (CFGetTypeID(certificate) == SecCertificateGetTypeID()), out);
    if (SecCertificateVersion(certificate) >= 3) {
        const SecCEBasicConstraints *basicConstraints = SecCertificateGetBasicConstraints(certificate);
        result = (basicConstraints && basicConstraints->isCA);
    }
    else {
        result = _SecCertificateIsSelfSigned(certificate);
    }
out:
    return result;
}

bool SecCertificateIsSelfSignedCA(SecCertificateRef certificate) {
    return (_SecCertificateIsSelfSigned(certificate) && SecCertificateIsCA(certificate));
}

OSStatus SecCertificateIsSelfSigned(SecCertificateRef certificate, Boolean *isSelfSigned) {
    if (!certificate || (CFGetTypeID(certificate) != SecCertificateGetTypeID())) {
        return errSecInvalidCertificate;
    }
    if (!isSelfSigned) {
        return errSecParam;
    }
    *isSelfSigned = _SecCertificateIsSelfSigned(certificate);
    return errSecSuccess;
}

SecKeyUsage SecCertificateGetKeyUsage(SecCertificateRef certificate) {
    if (!certificate) {
        return kSecKeyUsageUnspecified;
    }
    return certificate->_keyUsage;
}

CFArrayRef SecCertificateCopyExtendedKeyUsage(SecCertificateRef certificate)
{
    CFMutableArrayRef extended_key_usage_oids =
        CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
    require_quiet(certificate && extended_key_usage_oids, out);
    int ix;
    for (ix = 0; ix < certificate->_extensionCount; ++ix) {
        const SecCertificateExtension *extn = &certificate->_extensions[ix];
        if (extn->extnID.length == oidExtendedKeyUsage.length &&
            !memcmp(extn->extnID.data, oidExtendedKeyUsage.data, extn->extnID.length)) {
            DERTag tag;
            DERSequence derSeq;
            DERReturn drtn = DERDecodeSeqInit(&extn->extnValue, &tag, &derSeq);
            require_noerr_quiet(drtn, out);
            require_quiet(tag == ASN1_CONSTR_SEQUENCE, out);
            DERDecodedInfo currDecoded;

            while ((drtn = DERDecodeSeqNext(&derSeq, &currDecoded)) == DR_Success) {
                require_quiet(currDecoded.tag == ASN1_OBJECT_ID, out);
                CFDataRef oid = CFDataCreate(kCFAllocatorDefault,
                    currDecoded.content.data, currDecoded.content.length);
                if (oid) {
                    CFArrayAppendValue(extended_key_usage_oids, oid);
                    CFRelease(oid);
                }
            }
            require_quiet(drtn == DR_EndOfSequence, out);
            return extended_key_usage_oids;
        }
    }
out:
    CFReleaseSafe(extended_key_usage_oids);
    return NULL;
}

CFArrayRef SecCertificateCopySignedCertificateTimestamps(SecCertificateRef certificate)
{
    require_quiet(certificate, out);
    int ix;

    for (ix = 0; ix < certificate->_extensionCount; ++ix) {
        const SecCertificateExtension *extn = &certificate->_extensions[ix];
        if (extn->extnID.length == oidGoogleEmbeddedSignedCertificateTimestamp.length &&
            !memcmp(extn->extnID.data, oidGoogleEmbeddedSignedCertificateTimestamp.data, extn->extnID.length)) {
            /* Got the SCT oid */
            DERDecodedInfo sctList;
            DERReturn drtn = DERDecodeItem(&extn->extnValue, &sctList);
            require_noerr_quiet(drtn, out);
            require_quiet(sctList.tag == ASN1_OCTET_STRING, out);
            return SecCreateSignedCertificateTimestampsArrayFromSerializedSCTList(sctList.content.data, sctList.content.length);
        }
    }
out:
    return NULL;
}


static bool matches_expected(DERItem der, CFTypeRef expected) {
    if (der.length > 1) {
        DERDecodedInfo decoded;
        DERDecodeItem(&der, &decoded);
        switch (decoded.tag) {
            case ASN1_NULL:
            {
                return decoded.content.length == 0 && expected == NULL;
            }
            break;

            case ASN1_IA5_STRING:
            case ASN1_UTF8_STRING: {
                if (isString(expected)) {
                    CFStringRef expectedString = (CFStringRef) expected;
                    CFStringRef itemString = CFStringCreateWithBytesNoCopy(kCFAllocatorDefault, decoded.content.data, decoded.content.length, kCFStringEncodingUTF8, false, kCFAllocatorNull);

                    bool result = (kCFCompareEqualTo == CFStringCompare(expectedString, itemString, 0));
                    CFReleaseNull(itemString);
                    return result;
                }
            }
            break;

            case ASN1_OCTET_STRING: {
                if (isData(expected)) {
                    CFDataRef expectedData = (CFDataRef) expected;
                    CFDataRef itemData = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, decoded.content.data, decoded.content.length, kCFAllocatorNull);

                    bool result = CFEqual(expectedData, itemData);
                    CFReleaseNull(itemData);
                    return result;
                }
            }
            break;

            case ASN1_INTEGER: {
                SInt32 expected_value = 0;
                if (isString(expected))
                {
                    CFStringRef aStr = (CFStringRef)expected;
                    expected_value = CFStringGetIntValue(aStr);
                }
                else if (isNumber(expected))
                {
                    CFNumberGetValue(expected, kCFNumberSInt32Type, &expected_value);
                }

                uint32_t num_value = 0;
                if (!DERParseInteger(&decoded.content, &num_value))
                {
                    return ((uint32_t)expected_value == num_value);
                }
            }
            break;

            default:
                break;
        }
    }

    return false;
}

static bool cert_contains_marker_extension_value(SecCertificateRef certificate, CFDataRef oid, CFTypeRef expectedValue)
{
    CFIndex ix;
    const uint8_t *oid_data = CFDataGetBytePtr(oid);
    size_t oid_len = CFDataGetLength(oid);

    for (ix = 0; ix < certificate->_extensionCount; ++ix) {
        const SecCertificateExtension *extn = &certificate->_extensions[ix];
        if (extn->extnID.length == oid_len
            && !memcmp(extn->extnID.data, oid_data, extn->extnID.length))
        {
			return matches_expected(extn->extnValue, expectedValue);
        }
    }
    return false;
}

static bool cert_contains_marker_extension(SecCertificateRef certificate, CFTypeRef oid)
{
    return cert_contains_marker_extension_value(certificate, oid, NULL);
}

struct search_context {
    bool found;
    SecCertificateRef certificate;
};

static bool GetDecimalValueOfString(CFStringRef string, uint32_t* value)
{
    CFCharacterSetRef nonDecimalDigit = CFCharacterSetCreateInvertedSet(NULL, CFCharacterSetGetPredefined(kCFCharacterSetDecimalDigit));
    bool result = false;

    if ( CFStringGetLength(string) > 0
      && !CFStringFindCharacterFromSet(string, nonDecimalDigit, CFRangeMake(0, CFStringGetLength(string)), kCFCompareForcedOrdering, NULL))
    {
        if (value)
            *value = CFStringGetIntValue(string);
        result = true;
    }

    CFReleaseNull(nonDecimalDigit);

    return result;
}

bool SecCertificateIsOidString(CFStringRef oid)
{
    if (!oid) return false;
    if (2 >= CFStringGetLength(oid)) return false;
    bool result = true;

    /* oid string only has the allowed characters */
    CFCharacterSetRef decimalOid = CFCharacterSetCreateWithCharactersInString(NULL, CFSTR("0123456789."));
    CFCharacterSetRef nonDecimalOid = CFCharacterSetCreateInvertedSet(NULL, decimalOid);
    if (CFStringFindCharacterFromSet(oid, nonDecimalOid, CFRangeMake(0, CFStringGetLength(oid)), kCFCompareForcedOrdering, NULL)) {
        result = false;
    }

    /* first arc is allowed */
    UniChar firstArc[2];
    CFRange firstTwo = {0, 2};
    CFStringGetCharacters(oid, firstTwo, firstArc);
    if (firstArc[1] != '.' ||
        (firstArc[0] != '0' && firstArc[0] != '1' && firstArc[0] != '2')) {
        result = false;
    }

    CFReleaseNull(decimalOid);
    CFReleaseNull(nonDecimalOid);

    return result;
}

CFDataRef SecCertificateCreateOidDataFromString(CFAllocatorRef allocator, CFStringRef string)
{
    CFMutableDataRef currentResult = NULL;
    CFDataRef encodedResult = NULL;

    CFArrayRef parts = NULL;
    CFIndex count = 0;

    if (!string || !SecCertificateIsOidString(string))
        goto exit;

    parts = CFStringCreateArrayBySeparatingStrings(NULL, string, CFSTR("."));

    if (!parts)
        goto exit;

    count = CFArrayGetCount(parts);
    if (count == 0)
        goto exit;

    // assume no more than 5 bytes needed to represent any part of the oid,
    // since we limit parts to 32-bit values,
    // but the first two parts only need 1 byte
    currentResult = CFDataCreateMutable(allocator, 1+(count-2)*5);

    CFStringRef part;
    uint32_t x;
    uint8_t firstByte;

    part = CFArrayGetValueAtIndex(parts, 0);

    if (!GetDecimalValueOfString(part, &x) || x > 6)
        goto exit;

    firstByte = x * 40;


    if (count > 1) {
        part = CFArrayGetValueAtIndex(parts, 1);

        if (!GetDecimalValueOfString(part, &x) || x > 39)
            goto exit;

        firstByte += x;
    }

    CFDataAppendBytes(currentResult, &firstByte, 1);

    for (CFIndex i = 2; i < count && GetDecimalValueOfString(CFArrayGetValueAtIndex(parts, i), &x); ++i) {
        uint8_t b[5] = {0, 0, 0, 0, 0};
        b[4] = (x & 0x7F);
        b[3] = 0x80 | ((x >> 7) & 0x7F);
        b[2] = 0x80 | ((x >> 14) & 0x7F);
        b[1] = 0x80 | ((x >> 21) & 0x7F);
        b[0] = 0x80 | ((x >> 28) & 0x7F);

        // Skip the unused extension bytes.
        size_t skipBytes = 0;
        while (b[skipBytes] == 0x80)
            ++skipBytes;

        CFDataAppendBytes(currentResult, b + skipBytes, sizeof(b) - skipBytes);
    }

    encodedResult = currentResult;
    currentResult = NULL;

exit:
    CFReleaseNull(parts);
    CFReleaseNull(currentResult);

    return encodedResult;
}

static void check_for_marker(const void *key, const void *value, void *context)
{
    struct search_context * search_ctx = (struct search_context *) context;
    CFStringRef key_string = (CFStringRef) key;
    CFTypeRef value_ref = (CFTypeRef) value;

    // If we could have short circuted the iteration
    // we would have, but the best we can do
    // is not waste time comparing once a match
    // was found.
    if (search_ctx->found)
        return;

    if (CFGetTypeID(key_string) != CFStringGetTypeID())
        return;

    CFDataRef key_data = SecCertificateCreateOidDataFromString(NULL, key_string);

    if (NULL == key_data)
        return;

    if (cert_contains_marker_extension_value(search_ctx->certificate, key_data, value_ref))
        search_ctx->found = true;

    CFReleaseNull(key_data);
}

//
// CFType Ref is either:
//
// CFData - OID to match with no data permitted
// CFString - decimal OID to match
// CFDictionary - OID -> Value table for expected values Single Object or Array
// CFArray - Array of the above.
//
// This returns true if any of the requirements are met.
bool SecCertificateHasMarkerExtension(SecCertificateRef certificate, CFTypeRef oids)
{
    if (CFGetTypeID(oids) == CFArrayGetTypeID()) {
        CFIndex ix, length = CFArrayGetCount(oids);
        for (ix = 0; ix < length; ix++)
            if (SecCertificateHasMarkerExtension(certificate, CFArrayGetValueAtIndex((CFArrayRef)oids, ix)))
                return true;
    } else if (CFGetTypeID(oids) == CFDictionaryGetTypeID()) {
        struct search_context context = { .found = false, .certificate = certificate };
        CFDictionaryApplyFunction((CFDictionaryRef) oids, &check_for_marker, &context);
        return context.found;
    } else if (CFGetTypeID(oids) == CFDataGetTypeID()) {
        return cert_contains_marker_extension(certificate, oids);
    } else if (CFGetTypeID(oids) == CFStringGetTypeID()) {
        CFDataRef dataOid = SecCertificateCreateOidDataFromString(NULL, oids);
        if (dataOid == NULL) return false;
        bool result = cert_contains_marker_extension(certificate, dataOid);
        CFReleaseNull(dataOid);
        return result;
    }
    return false;
}

static DERItem *cert_extension_value_for_marker(SecCertificateRef certificate, CFDataRef oid) {
    CFIndex ix;
    const uint8_t *oid_data = CFDataGetBytePtr(oid);
    size_t oid_len = CFDataGetLength(oid);

    for (ix = 0; ix < certificate->_extensionCount; ++ix) {
        const SecCertificateExtension *extn = &certificate->_extensions[ix];
        if (extn->extnID.length == oid_len
            && !memcmp(extn->extnID.data, oid_data, extn->extnID.length))
        {
            return (DERItem *)&extn->extnValue;
        }
    }
    return NULL;
}

//
// CFType Ref is either:
//
// CFData - OID to match with no data permitted
// CFString - decimal OID to match
//
DERItem *SecCertificateGetExtensionValue(SecCertificateRef certificate, CFTypeRef oid) {
    if (!certificate || !oid) {
        return NULL;
    }

    if(CFGetTypeID(oid) == CFDataGetTypeID()) {
        return cert_extension_value_for_marker(certificate, oid);
    } else if (CFGetTypeID(oid) == CFStringGetTypeID()) {
        CFDataRef dataOid = SecCertificateCreateOidDataFromString(NULL, oid);
        if (dataOid == NULL) return NULL;
        DERItem *result = cert_extension_value_for_marker(certificate, dataOid);
        CFReleaseNull(dataOid);
        return result;
    }

    return NULL;
}

CFDataRef SecCertificateCopyiAPAuthCapabilities(SecCertificateRef certificate) {
    if (!certificate) {
        return NULL;
    }
    CFDataRef extensionData = NULL;
    DERItem *extensionValue = NULL;
    extensionValue = SecCertificateGetExtensionValue(certificate,
                                                     CFSTR("1.2.840.113635.100.6.36"));
    require_quiet(extensionValue, out);
    /* The extension is a octet string containing the DER-encoded 32-byte octet string */
    require_quiet(extensionValue->length == 34, out);
    DERDecodedInfo decodedValue;
    require_noerr_quiet(DERDecodeItem(extensionValue, &decodedValue), out);
    if (decodedValue.tag == ASN1_OCTET_STRING) {
        require_quiet(decodedValue.content.length == 32, out);
        extensionData = CFDataCreate(NULL, decodedValue.content.data,
                                     decodedValue.content.length);
    } else {
        require_quiet(extensionValue->data[33] == 0x00 &&
                      extensionValue->data[32] == 0x00, out);
        extensionData = CFDataCreate(NULL, extensionValue->data, 32);
    }
out:
    return extensionData;
}

#if 0
/* From iapd IAPAuthenticationTypes.h */
typedef struct  IapCertSerialNumber
{
    uint8_t   xservID;            // Xserver ID
    uint8_t   hsmID;              // Hardware security module ID (generated cert)
    uint8_t   delimiter01;        // Field delimiter (IAP_CERT_FIELD_DELIMITER)
    uint8_t   dateYear;           // Date year  cert was issued
    uint8_t   dateMonth;          // Date month cert was issued
    uint8_t   dateDay;            // Date day   cert was issued
    uint8_t   delimiter02;        // Field delimiter (IAP_CERT_FIELD_DELIMITER)
    uint8_t   devClass;           // iAP device class (maps to lingo permissions)
    uint8_t   delimiter03;        // Field delimiter (IAP_CERT_FIELD_DELIMITER)
    uint8_t   batchNumHi;         // Batch number high byte (15:08)
    uint8_t   batchNumLo;         // Batch number low  byte (07:00)
    uint8_t   delimiter04;        // Field delimiter (IAP_CERT_FIELD_DELIMITER)
    uint8_t   serialNumHi;        // Serial number high   byte (23:16)
    uint8_t   serialNumMid;       // Serial number middle byte (15:08)
    uint8_t   serialNumLo;        // Serial number low    byte (07:00)

}   IapCertSerialNumber_t, *pIapCertSerialNumber_t;
#endif

#define IAP_CERT_FIELD_DELIMITER        0xAA    // "Apple_Accessory" delimiter
SeciAuthVersion SecCertificateGetiAuthVersion(SecCertificateRef certificate) {
    if (!certificate) {
        return kSeciAuthInvalid;
    }
    if (NULL != SecCertificateGetExtensionValue(certificate,
                                                CFSTR("1.2.840.113635.100.6.36"))) {
        return kSeciAuthVersion3;
    }
    DERItem serialNumber = certificate->_serialNum;
    require_quiet(serialNumber.data, out);
    require_quiet(serialNumber.length == 15, out);
    require_quiet(serialNumber.data[2] == IAP_CERT_FIELD_DELIMITER &&
                  serialNumber.data[6] == IAP_CERT_FIELD_DELIMITER &&
                  serialNumber.data[8] == IAP_CERT_FIELD_DELIMITER &&
                  serialNumber.data[11] == IAP_CERT_FIELD_DELIMITER, out);
    return kSeciAuthVersion2;
out:
    return kSeciAuthInvalid;
}

SecCertificateRef SecCertificateCreateWithPEM(CFAllocatorRef allocator,
	CFDataRef pem_certificate)
{
    static const char begin_cert[] = "-----BEGIN CERTIFICATE-----\n";
    static const char end_cert[] = "-----END CERTIFICATE-----\n";
    uint8_t *base64_data = NULL;
    SecCertificateRef cert = NULL;
    const unsigned char *data = CFDataGetBytePtr(pem_certificate);
    //const size_t length = CFDataGetLength(pem_certificate);
    char *begin = strstr((const char *)data, begin_cert);
    char *end = strstr((const char *)data, end_cert);
    if (!begin || !end)
        return NULL;
    begin += sizeof(begin_cert) - 1;
    size_t base64_length = SecBase64Decode(begin, end - begin, NULL, 0);
    if (base64_length) {
        require_quiet(base64_data = calloc(1, base64_length), out);
        require_quiet(base64_length = SecBase64Decode(begin, end - begin, base64_data, base64_length), out);
        cert = SecCertificateCreateWithBytes(kCFAllocatorDefault, base64_data, base64_length);
        free(base64_data);
    }
out:
    return cert;
}


//
// -- MARK -- XPC encoding/decoding
//

bool SecCertificateAppendToXPCArray(SecCertificateRef certificate, xpc_object_t xpc_certificates, CFErrorRef *error) {
    if (!certificate)
        return true; // NOOP

    size_t length = SecCertificateGetLength(certificate);
    const uint8_t *bytes = SecCertificateGetBytePtr(certificate);
#if SECTRUST_VERBOSE_DEBUG
	secerror("cert=0x%lX length=%d bytes=0x%lX", (uintptr_t)certificate, (int)length, (uintptr_t)bytes);
#endif
    if (!length || !bytes) {
        return SecError(errSecParam, error, CFSTR("failed to der encode certificate"));
	}
    xpc_array_set_data(xpc_certificates, XPC_ARRAY_APPEND, bytes, length);
    return true;
}

SecCertificateRef SecCertificateCreateWithXPCArrayAtIndex(xpc_object_t xpc_certificates, size_t index, CFErrorRef *error) {
    SecCertificateRef certificate = NULL;
    size_t length = 0;
    const uint8_t *bytes = xpc_array_get_data(xpc_certificates, index, &length);
    if (bytes) {
        certificate = SecCertificateCreateWithBytes(kCFAllocatorDefault, bytes, length);
    }
    if (!certificate) {
        SecError(errSecParam, error, CFSTR("certificates[%zu] failed to decode"), index);
	}
    return certificate;
}

xpc_object_t SecCertificateArrayCopyXPCArray(CFArrayRef certificates, CFErrorRef *error) {
    xpc_object_t xpc_certificates;
    require_action_quiet(xpc_certificates = xpc_array_create(NULL, 0), exit,
                         SecError(errSecAllocate, error, CFSTR("failed to create xpc_array")));
    CFIndex ix, count = CFArrayGetCount(certificates);
    for (ix = 0; ix < count; ++ix) {
		SecCertificateRef certificate = (SecCertificateRef) CFArrayGetValueAtIndex(certificates, ix);
    #if SECTRUST_VERBOSE_DEBUG
		CFIndex length = SecCertificateGetLength(certificate);
		const UInt8 *bytes = SecCertificateGetBytePtr(certificate);
		secerror("idx=%d of %d; cert=0x%lX length=%ld bytes=0x%lX", (int)ix, (int)count, (uintptr_t)certificate, (size_t)length, (uintptr_t)bytes);
    #endif
        if (!SecCertificateAppendToXPCArray(certificate, xpc_certificates, error)) {
            xpc_release(xpc_certificates);
            xpc_certificates = NULL;
			break;
        }
    }

exit:
    return xpc_certificates;
}

CFArrayRef SecCertificateXPCArrayCopyArray(xpc_object_t xpc_certificates, CFErrorRef *error) {
    CFMutableArrayRef certificates = NULL;
    require_action_quiet(xpc_get_type(xpc_certificates) == XPC_TYPE_ARRAY, exit,
                         SecError(errSecParam, error, CFSTR("certificates xpc value is not an array")));
    size_t count = xpc_array_get_count(xpc_certificates);
    require_action_quiet(certificates = CFArrayCreateMutable(kCFAllocatorDefault, count, &kCFTypeArrayCallBacks), exit,
                         SecError(errSecAllocate, error, CFSTR("failed to create CFArray of capacity %zu"), count));

    size_t ix;
    for (ix = 0; ix < count; ++ix) {
        SecCertificateRef cert = SecCertificateCreateWithXPCArrayAtIndex(xpc_certificates, ix, error);
        if (!cert) {
            CFRelease(certificates);
            return NULL;
        }
        CFArraySetValueAtIndex(certificates, ix, cert);
        CFRelease(cert);
    }

exit:
    return certificates;
}

#define do_if_registered(sdp, ...) if (gSecurityd && gSecurityd->sdp) { return gSecurityd->sdp(__VA_ARGS__); }


static CFArrayRef CopyEscrowCertificates(SecCertificateEscrowRootType escrowRootType, CFErrorRef* error)
{
	__block CFArrayRef result = NULL;

	do_if_registered(ota_CopyEscrowCertificates, escrowRootType, error);

	securityd_send_sync_and_do(kSecXPCOpOTAGetEscrowCertificates, error,
		^bool(xpc_object_t message, CFErrorRef *error)
		{
			xpc_dictionary_set_uint64(message, "escrowType", (uint64_t)escrowRootType);
			return true;
		},
		^bool(xpc_object_t response, CFErrorRef *error)
		{
			xpc_object_t xpc_array = xpc_dictionary_get_value(response, kSecXPCKeyResult);

			if (response && (NULL != xpc_array)) {
				result = (CFArrayRef)_CFXPCCreateCFObjectFromXPCObject(xpc_array);
			}
			else {
				return SecError(errSecInternal, error, CFSTR("Did not get the Escrow certificates"));
			}
			return result != NULL;
		});
	return result;
}

CFArrayRef SecCertificateCopyEscrowRoots(SecCertificateEscrowRootType escrowRootType)
{
	CFArrayRef result = NULL;
	int iCnt;
	CFDataRef certData = NULL;
	int numRoots = 0;

	if (kSecCertificateBaselineEscrowRoot == escrowRootType ||
		kSecCertificateBaselinePCSEscrowRoot == escrowRootType ||
		kSecCertificateBaselineEscrowBackupRoot == escrowRootType ||
		kSecCertificateBaselineEscrowEnrollmentRoot == escrowRootType)
	{
		// The request is for the base line certificates.
		// Use the hard coded data to generate the return array.
		struct RootRecord** pEscrowRoots;
		switch (escrowRootType) {
			case kSecCertificateBaselineEscrowRoot:
				numRoots = kNumberOfBaseLineEscrowRoots;
				pEscrowRoots = kBaseLineEscrowRoots;
				break;
			case kSecCertificateBaselinePCSEscrowRoot:
				numRoots = kNumberOfBaseLinePCSEscrowRoots;
				pEscrowRoots = kBaseLinePCSEscrowRoots;
				break;
			case kSecCertificateBaselineEscrowBackupRoot:
				numRoots = kNumberOfBaseLineEscrowBackupRoots;
				pEscrowRoots = kBaseLineEscrowBackupRoots;
				break;
			case kSecCertificateBaselineEscrowEnrollmentRoot:
			default:
				numRoots = kNumberOfBaseLineEscrowEnrollmentRoots;
				pEscrowRoots = kBaseLineEscrowEnrollmentRoots;
				break;
		}

		// Get the hard coded set of roots
		SecCertificateRef baseLineCerts[numRoots];
		struct RootRecord* pRootRecord = NULL;

		for (iCnt = 0; iCnt < numRoots; iCnt++) {
			pRootRecord = pEscrowRoots[iCnt];
			if (NULL != pRootRecord && pRootRecord->_length > 0 && NULL != pRootRecord->_bytes) {
				certData = CFDataCreate(kCFAllocatorDefault, pRootRecord->_bytes, pRootRecord->_length);
				if (NULL != certData) {
					baseLineCerts[iCnt] = SecCertificateCreateWithData(kCFAllocatorDefault, certData);
					CFRelease(certData);
				}
			}
		}
		result = CFArrayCreate(kCFAllocatorDefault, (const void **)baseLineCerts, numRoots, &kCFTypeArrayCallBacks);
		for (iCnt = 0; iCnt < numRoots; iCnt++) {
			if (NULL != baseLineCerts[iCnt]) {
				CFRelease(baseLineCerts[iCnt]);
			}
		}
	}
	else {
		// The request is for the current certificates.
		CFErrorRef error = NULL;
		CFArrayRef cert_datas = CopyEscrowCertificates(escrowRootType, &error);
		if (NULL != error || NULL == cert_datas) {
			if (NULL != error) {
				CFRelease(error);
			}
			if (NULL != cert_datas) {
				CFRelease(cert_datas);
			}
			return result;
		}

		numRoots = (int)(CFArrayGetCount(cert_datas));

		SecCertificateRef assetCerts[numRoots];
		for (iCnt = 0; iCnt < numRoots; iCnt++) {
			certData = (CFDataRef)CFArrayGetValueAtIndex(cert_datas, iCnt);
			if (NULL != certData) {
				SecCertificateRef aCertRef = SecCertificateCreateWithData(kCFAllocatorDefault, certData);
				assetCerts[iCnt] = aCertRef;
			}
			else {
				assetCerts[iCnt] = NULL;
			}
		}

		if (numRoots > 0) {
			result = CFArrayCreate(kCFAllocatorDefault, (const void **)assetCerts, numRoots, &kCFTypeArrayCallBacks);
			for (iCnt = 0; iCnt < numRoots; iCnt++) {
				if (NULL != assetCerts[iCnt]) {
					CFRelease(assetCerts[iCnt]);
				}
			}
		}
		CFReleaseSafe(cert_datas);
	}
	return result;
}

SEC_CONST_DECL (kSecSignatureDigestAlgorithmUnknown, "SignatureDigestUnknown");
SEC_CONST_DECL (kSecSignatureDigestAlgorithmMD2, "SignatureDigestMD2");
SEC_CONST_DECL (kSecSignatureDigestAlgorithmMD4, "SignatureDigestMD4");
SEC_CONST_DECL (kSecSignatureDigestAlgorithmMD5, "SignatureDigestMD5");
SEC_CONST_DECL (kSecSignatureDigestAlgorithmSHA1, "SignatureDigestSHA1");
SEC_CONST_DECL (kSecSignatureDigestAlgorithmSHA224, "SignatureDigestSHA224");
SEC_CONST_DECL (kSecSignatureDigestAlgorithmSHA256, "SignatureDigestSHA256");
SEC_CONST_DECL (kSecSignatureDigestAlgorithmSHA384, "SignatureDigestSHA284");
SEC_CONST_DECL (kSecSignatureDigestAlgorithmSHA512, "SignatureDigestSHA512");

SecSignatureHashAlgorithm SecCertificateGetSignatureHashAlgorithm(SecCertificateRef certificate)
{
	SecSignatureHashAlgorithm result = kSecSignatureHashAlgorithmUnknown;
	DERAlgorithmId *algId = (certificate) ? &certificate->_tbsSigAlg : NULL;
	const DERItem *algOid = (algId) ? &algId->oid : NULL;
	while (algOid) {
		if (!algOid->data || !algOid->length) {
			break;
		}
		/* classify the signature algorithm OID into one of our known types */
		if (DEROidCompare(algOid, &oidSha512Ecdsa) ||
			DEROidCompare(algOid, &oidSha512Rsa) ||
			DEROidCompare(algOid, &oidSha512)) {
			result = kSecSignatureHashAlgorithmSHA512;
			break;
		}
		if (DEROidCompare(algOid, &oidSha384Ecdsa) ||
			DEROidCompare(algOid, &oidSha384Rsa) ||
			DEROidCompare(algOid, &oidSha384)) {
			result = kSecSignatureHashAlgorithmSHA384;
			break;
		}
		if (DEROidCompare(algOid, &oidSha256Ecdsa) ||
			DEROidCompare(algOid, &oidSha256Rsa) ||
			DEROidCompare(algOid, &oidSha256)) {
			result = kSecSignatureHashAlgorithmSHA256;
			break;
		}
		if (DEROidCompare(algOid, &oidSha224Ecdsa) ||
			DEROidCompare(algOid, &oidSha224Rsa) ||
			DEROidCompare(algOid, &oidSha224)) {
			result = kSecSignatureHashAlgorithmSHA224;
			break;
		}
		if (DEROidCompare(algOid, &oidSha1Ecdsa) ||
			DEROidCompare(algOid, &oidSha1Rsa) ||
			DEROidCompare(algOid, &oidSha1Dsa) ||
			DEROidCompare(algOid, &oidSha1DsaOIW) ||
			DEROidCompare(algOid, &oidSha1DsaCommonOIW) ||
			DEROidCompare(algOid, &oidSha1RsaOIW) ||
			DEROidCompare(algOid, &oidSha1Fee) ||
			DEROidCompare(algOid, &oidSha1)) {
			result = kSecSignatureHashAlgorithmSHA1;
			break;
		}
		if (DEROidCompare(algOid, &oidMd5Rsa) ||
			DEROidCompare(algOid, &oidMd5Fee) ||
			DEROidCompare(algOid, &oidMd5)) {
			result = kSecSignatureHashAlgorithmMD5;
			break;
		}
		if (DEROidCompare(algOid, &oidMd4Rsa) ||
			DEROidCompare(algOid, &oidMd4)) {
			result = kSecSignatureHashAlgorithmMD4;
			break;
		}
		if (DEROidCompare(algOid, &oidMd2Rsa) ||
			DEROidCompare(algOid, &oidMd2)) {
			result = kSecSignatureHashAlgorithmMD2;
			break;
		}
		break;
	}

	return result;
}


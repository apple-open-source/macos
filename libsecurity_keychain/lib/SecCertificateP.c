/*
 * Copyright (c) 2006-2010 Apple Inc. All Rights Reserved.
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


//#include <Security/SecCertificateInternal.h>
#include "SecCertificateInternalP.h"

#include <CommonCrypto/CommonDigest.h>
#include <CoreFoundation/CFRuntime.h>
#include <CoreFoundation/CFString.h>
#include <CoreFoundation/CFBundle.h>
#include <CoreFoundation/CFDictionary.h>
#include <CoreFoundation/CFNumber.h>
#include <CoreFoundation/CFTimeZone.h>
#include <pthread.h>
#include <string.h>
#include <AssertMacros.h>
#include <libDER/libDER.h>
#include <libDER/DER_CertCrl.h>
#include <libDER/DER_Encode.h>
#include <libDER/DER_Keys.h>
#include <libDER/asn1Types.h>
#include <libDER/oids.h>

#include "SecBasePriv.h"

#include "SecRSAKeyP.h"
#include "SecFrameworkP.h"
#include "SecItem.h"
#include "SecItemPriv.h"
#include <stdbool.h>
#include "debuggingP.h"
#include <stdlib.h>
#include <libkern/OSByteOrder.h>
#include <ctype.h>
#include "SecInternalP.h"
#include "SecBase64P.h"

typedef struct SecCertificateExtension {
	DERItem extnID;
    bool critical;
    DERItem extnValue;
} SecCertificateExtension;

#if 0
typedef struct KnownExtension {
    bool critical;
    DERItem extnValue;
} KnownExtension;

enum {
    kSecSelfSignedUnknown = 0,
    kSecSelfSignedFalse,
    kSecSelfSignedTrue,
};
#endif

struct __SecCertificate {
    CFRuntimeBase		_base;

	DERItem				_der;			/* Entire certificate in DER form. */
	DERItem				_tbs;			/* To Be Signed cert DER bytes. */
    DERAlgorithmId      _sigAlg;		/* Top level signature algorithm. */
    DERItem				_signature;		/* The content of the sig bit string. */

    UInt8               _version;
	DERItem				_serialNum;		/* Integer. */
    DERAlgorithmId      _tbsSigAlg;		/* sig alg MUST be same as _sigAlg. */
	DERItem				_issuer;		/* Sequence of RDN. */
	CFAbsoluteTime      _notBefore;
	CFAbsoluteTime      _notAfter;
	DERItem				_subject;		/* Sequence of RDN. */
	DERAlgorithmId		_algId;			/* oid and params of _pubKeyDER. */
    DERItem             _pubKeyDER;		/* contents of bit string */
	DERItem				_issuerUniqueID;		/* bit string, optional */
	DERItem				_subjectUniqueID;		/* bit string, optional */

#if 0
    /* Known extensions if the certificate contains them,
       extnValue.length will be > 0. */
    KnownExtension      _authorityKeyID;

    /* This extension is used to uniquely identify a certificate from among
       several that have the same subject name. If the extension is not
       present, its value is calculated by performing a SHA-1 hash of the
       certificate's DER encoded subjectPublicKeyInfo, as recommended by
       PKIX. */
    KnownExtension      _subjectKeyID;
    KnownExtension      _keyUsage;
    KnownExtension      _extendedKeyUsage;
    KnownExtension      _basicConstraints;
    KnownExtension      _netscapeCertType;
    KnownExtension      _subjectAltName;
    KnownExtension      _qualCertStatements;

#endif
    bool                _foundUnknownCriticalExtension;

	/* Well known certificate extensions. */
    SecCEBasicConstraints       _basicConstraints;
    SecCEPolicyConstraints      _policyConstraints;
    CFDictionaryRef             _policyMappings;
    SecCECertificatePolicies    _certificatePolicies;

    /* If InhibitAnyPolicy extension is not present or invalid UINT32_MAX,
       value of the SkipCerts field of the InhibitAnyPolicy extension
       otherwise. */
    uint32_t _inhibitAnyPolicySkipCerts;

    /* If KeyUsage extension is not present this is 0, otherwise it's
       the value of the extension. */
    SecKeyUsage _keyUsage;

	/* OCTECTS of SubjectKeyIdentifier extensions KeyIdentifier.
	   Length = 0 if not present. */
    DERItem				_subjectKeyIdentifier;

	/* OCTECTS of AuthorityKeyIdentifier extensions KeyIdentifier.
	   Length = 0 if not present. */
    DERItem				_authorityKeyIdentifier;
	/* AuthorityKeyIdentifier extension _authorityKeyIdentifierIssuer and
	   _authorityKeyIdentifierSerialNumber have non zero length if present.
	   Both are either present or absent together.  */
    DERItem				_authorityKeyIdentifierIssuer;
    DERItem				_authorityKeyIdentifierSerialNumber;

	/* Subject alt name extension, if present.  Not malloced, it's just a
	   pointer to an element in the _extensions array. */
	const SecCertificateExtension	*_subjectAltName;

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

    /* All other (non known) extensions.   The _extensions array is malloced. */
    CFIndex             _extensionCount;
    SecCertificateExtension *_extensions;

	/* Optional cached fields. */
	SecKeyRef			_pubKey;
	CFDataRef			_der_data;
	CFArrayRef			_properties;
    CFDataRef			_serialNumber;
    CFDataRef			_normalizedIssuer;
	CFDataRef			_normalizedSubject;
	CFDataRef			_authorityKeyID;
	CFDataRef			_subjectKeyID;

	CFDataRef			_sha1Digest;
    uint8_t             _isSelfSigned;

};

/* Public Constants for property list keys. */
CFStringRef kSecPropertyKeyType             = CFSTR("type");
CFStringRef kSecPropertyKeyLabel            = CFSTR("label");
CFStringRef kSecPropertyKeyLocalizedLabel   = CFSTR("localized label");
CFStringRef kSecPropertyKeyValue            = CFSTR("value");

/* Public Constants for property list values. */
CFStringRef kSecPropertyTypeWarning         = CFSTR("warning");
CFStringRef kSecPropertyTypeError           = CFSTR("error");
CFStringRef kSecPropertyTypeSuccess         = CFSTR("success");
CFStringRef kSecPropertyTypeTitle           = CFSTR("title");
CFStringRef kSecPropertyTypeSection         = CFSTR("section");
CFStringRef kSecPropertyTypeData            = CFSTR("data");
CFStringRef kSecPropertyTypeString          = CFSTR("string");
CFStringRef kSecPropertyTypeURL             = CFSTR("url");
CFStringRef kSecPropertyTypeDate            = CFSTR("date");

/* Extension parsing routine. */
typedef void (*SecCertificateExtensionParser)(SecCertificateRefP certificate,
	const SecCertificateExtension *extn);

/* CFRuntime regsitration data. */
static pthread_once_t kSecCertificateRegisterClass = PTHREAD_ONCE_INIT;
static CFTypeID kSecCertificateTypeID = _kCFRuntimeNotATypeID;

/* Mapping from extension OIDs (as a DERItem *) to
   SecCertificateExtensionParser extension parsing routines. */
static CFDictionaryRef gExtensionParsers;

/* Forward declartions of static functions. */
static CFStringRef SecCertificateDescribe(CFTypeRef cf);
static void SecCertificateDestroy(CFTypeRef cf);
static bool derDateGetAbsoluteTime(const DERItem *dateChoice,
    CFAbsoluteTime *absTime);

/* Static functions. */
static CFStringRef SecCertificateDescribe(CFTypeRef cf) {
    SecCertificateRefP certificate = (SecCertificateRefP)cf;
    return CFStringCreateWithFormat(kCFAllocatorDefault, NULL,
        CFSTR("<cert(%p) s: %@ i: %@>"), certificate,
        SecCertificateCopySubjectSummaryP(certificate),
        SecCertificateCopyIssuerSummary(certificate));
}

static void SecCertificateDestroy(CFTypeRef cf) {
    SecCertificateRefP certificate = (SecCertificateRefP)cf;
    if (certificate->_certificatePolicies.policies)
        free(certificate->_certificatePolicies.policies);
    CFReleaseSafe(certificate->_policyMappings);
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
}

static Boolean SecCertificateEqual(CFTypeRef cf1, CFTypeRef cf2) {
    SecCertificateRefP cert1 = (SecCertificateRefP)cf1;
    SecCertificateRefP cert2 = (SecCertificateRefP)cf2;
    if (cert1 == cert2)
        return true;
    if (!cert2 || cert1->_der.length != cert2->_der.length)
        return false;
    return !memcmp(cert1->_der.data, cert2->_der.data, cert1->_der.length);
}

/* Hash of the certificate is der length + signature length + last 4 bytes
   of signature. */
static CFHashCode SecCertificateHash(CFTypeRef cf) {
    SecCertificateRefP certificate = (SecCertificateRefP)cf;
	uint32_t der_length = certificate->_der.length;
	uint32_t sig_length = certificate->_signature.length;
	uint32_t ix = (sig_length > 4) ? sig_length - 4 : 0;
	CFHashCode hashCode = 0;
	for (; ix < sig_length; ++ix)
		hashCode = (hashCode << 8) + certificate->_signature.data[ix];

	return (hashCode + der_length + sig_length);
}

#if 1

/************************************************************************/
/************************* General Name Parsing *************************/
/************************************************************************/

typedef OSStatus (*parseGeneralNameCallback)(void *context,
	SecCEGeneralNameType type, const DERItem *value);


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
static OSStatus parseGeneralNameContentProperty(DERTag tag,
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
		OSStatus status = parseGeneralNameContentProperty(
			generalNameContent.tag, &generalNameContent.content, context,
				callback);
		if (status)
			return status;
	}
    require_quiet(drtn == DR_EndOfSequence, badDER);
	return noErr;

badDER:
	return errSecInvalidCertificate;
}

static OSStatus parseGeneralNames(const DERItem *generalNames, void *context,
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
	return noErr;
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
	return noErr;

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
    return noErr;
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

	return noErr;
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

	return noErr;

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

static void SecCEPSubjectKeyIdentifier(SecCertificateRefP certificate,
	const SecCertificateExtension *extn) {
	secdebug("cert", "critical: %s", extn->critical ? "yes" : "no");
    DERDecodedInfo keyIdentifier;
	DERReturn drtn = DERDecodeItem(&extn->extnValue, &keyIdentifier);
	require_noerr_quiet(drtn, badDER);
	require_quiet(keyIdentifier.tag == ASN1_OCTET_STRING, badDER);
	certificate->_subjectKeyIdentifier = keyIdentifier.content;

	return;
badDER:
	secdebug(NULL, "Invalid SubjectKeyIdentifier Extension");
}

static void SecCEPKeyUsage(SecCertificateRefP certificate,
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

static void SecCEPPrivateKeyUsagePeriod(SecCertificateRefP certificate,
	const SecCertificateExtension *extn) {
	secdebug("cert", "critical: %s", extn->critical ? "yes" : "no");
}

static void SecCEPSubjectAltName(SecCertificateRefP certificate,
	const SecCertificateExtension *extn) {
	secdebug("cert", "critical: %s", extn->critical ? "yes" : "no");
	certificate->_subjectAltName = extn;
}

static void SecCEPIssuerAltName(SecCertificateRefP certificate,
	const SecCertificateExtension *extn) {
	secdebug("cert", "critical: %s", extn->critical ? "yes" : "no");
}

static void SecCEPBasicConstraints(SecCertificateRefP certificate,
	const SecCertificateExtension *extn) {
	secdebug("cert", "critical: %s", extn->critical ? "yes" : "no");
	DERBasicConstraints basicConstraints;
	require_noerr_quiet(DERParseSequence(&extn->extnValue,
        DERNumBasicConstraintsItemSpecs, DERBasicConstraintsItemSpecs,
        &basicConstraints, sizeof(basicConstraints)), badDER);
    require_noerr_quiet(DERParseBoolean(&basicConstraints.cA, false,
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
	secdebug(NULL, "Invalid BasicConstraints Extension");
}

static void SecCEPCrlDistributionPoints(SecCertificateRefP certificate,
	const SecCertificateExtension *extn) {
	secdebug("cert", "critical: %s", extn->critical ? "yes" : "no");
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
static void SecCEPCertificatePolicies(SecCertificateRefP certificate,
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
    while ((drtn = DERDecodeSeqNext(&piSeq, &piContent)) == DR_Success) {
        require_quiet(piContent.tag == ASN1_CONSTR_SEQUENCE, badDER);
        policy_count++;
    }
    require_quiet(drtn == DR_EndOfSequence, badDER);
    policies = (SecCEPolicyInformation *)malloc(sizeof(SecCEPolicyInformation)
        * policy_count);
    DERDecodeSeqInit(&extn->extnValue, &tag, &piSeq);
    DERSize policy_ix = 0;
    while ((drtn = DERDecodeSeqNext(&piSeq, &piContent)) == DR_Success) {
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
	secdebug(NULL, "Invalid CertificatePolicies Extension");
}

/*
   id-ce-policyMappings OBJECT IDENTIFIER ::=  { id-ce 33 }

   PolicyMappings ::= SEQUENCE SIZE (1..MAX) OF SEQUENCE {
        issuerDomainPolicy      CertPolicyId,
        subjectDomainPolicy     CertPolicyId }
*/
#if 0
static void SecCEPPolicyMappings(SecCertificateRefP certificate,
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
    while ((drtn = DERDecodeSeqNext(&pmSeq, &pmContent)) == DR_Success) {
        require_quiet(pmContent.tag == ASN1_CONSTR_SEQUENCE, badDER);
        mapping_count++;
    }
    mappings = (SecCEPolicyMapping *)malloc(sizeof(SecCEPolicyMapping)
        * mapping_count);
    DERDecodeSeqInit(&extn->extnValue, &tag, &pmSeq);
    DERSize mapping_ix = 0;
    while ((drtn = DERDecodeSeqNext(&pmSeq, &pmContent)) == DR_Success) {
        DERPolicyMapping pm;
        drtn = DERParseSequenceContent(&pmContent.content,
            DERNumPolicyMappingItemSpecs,
            DERPolicyMappingItemSpecs,
            &pm, sizeof(pm));
        require_noerr_quiet(drtn, badDER);
        mappings[mapping_ix].issuerDomainPolicy = pm.issuerDomainPolicy;
        mappings[mapping_ix++].subjectDomainPolicy = pm.subjectDomainPolicy;
    }
    require_quiet(drtn == DR_EndOfSequence, badDER);
    certificate->_policyMappings.present = true;
    certificate->_policyMappings.critical = extn->critical;
    certificate->_policyMappings.numMappings = mapping_count;
    certificate->_policyMappings.mappings = mappings;
	return;
badDER:
    if (mappings)
        free(mappings);
    CFReleaseSafe(mappings);
    certificate->_policyMappings.present = false;
	secdebug(NULL, "Invalid CertificatePolicies Extension");
}
#else
static void SecCEPPolicyMappings(SecCertificateRefP certificate,
	const SecCertificateExtension *extn) {
	secdebug("cert", "critical: %s", extn->critical ? "yes" : "no");
    DERTag tag;
    DERSequence pmSeq;
    CFMutableDictionaryRef mappings = NULL;
    DERReturn drtn = DERDecodeSeqInit(&extn->extnValue, &tag, &pmSeq);
    require_noerr_quiet(drtn, badDER);
    require_quiet(tag == ASN1_CONSTR_SEQUENCE, badDER);
    DERDecodedInfo pmContent;
    require_quiet(mappings = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
        &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks),
        badDER);;
    while ((drtn = DERDecodeSeqNext(&pmSeq, &pmContent)) == DR_Success) {
        require_quiet(pmContent.tag == ASN1_CONSTR_SEQUENCE, badDER);
        DERPolicyMapping pm;
        drtn = DERParseSequenceContent(&pmContent.content,
            DERNumPolicyMappingItemSpecs,
            DERPolicyMappingItemSpecs,
            &pm, sizeof(pm));
        require_noerr_quiet(drtn, badDER);
        CFDataRef idp, sdp;
        require_quiet(idp = CFDataCreate(kCFAllocatorDefault,
            pm.issuerDomainPolicy.data, pm.issuerDomainPolicy.length), badDER);
        require_quiet(sdp = CFDataCreate(kCFAllocatorDefault,
            pm.subjectDomainPolicy.data, pm.subjectDomainPolicy.length), badDER);
        CFMutableArrayRef sdps =
            (CFMutableArrayRef)CFDictionaryGetValue(mappings, idp);
        if (sdps) {
            CFArrayAppendValue(sdps, sdp);
        } else {
            require_quiet(sdps = CFArrayCreateMutable(kCFAllocatorDefault, 0,
                &kCFTypeArrayCallBacks), badDER);
            CFDictionarySetValue(mappings, idp, sdps);
            CFRelease(sdps);
        }
    }
    require_quiet(drtn == DR_EndOfSequence, badDER);
    certificate->_policyMappings = mappings;
	return;
badDER:
    CFReleaseSafe(mappings);
    certificate->_policyMappings = NULL;
	secdebug(NULL, "Invalid CertificatePolicies Extension");
}
#endif

/*
AuthorityKeyIdentifier ::= SEQUENCE {
    keyIdentifier             [0] KeyIdentifier            OPTIONAL,
    authorityCertIssuer       [1] GeneralNames             OPTIONAL,
    authorityCertSerialNumber [2] CertificateSerialNumber  OPTIONAL }
    -- authorityCertIssuer and authorityCertSerialNumber MUST both
    -- be present or both be absent

KeyIdentifier ::= OCTET STRING
*/
static void SecCEPAuthorityKeyIdentifier(SecCertificateRefP certificate,
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
	secdebug(NULL, "Invalid AuthorityKeyIdentifier Extension");
}

static void SecCEPPolicyConstraints(SecCertificateRefP certificate,
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
	secdebug(NULL, "Invalid PolicyConstraints Extension");
}

static void SecCEPExtendedKeyUsage(SecCertificateRefP certificate,
	const SecCertificateExtension *extn) {
	secdebug("cert", "critical: %s", extn->critical ? "yes" : "no");
}

/*
   InhibitAnyPolicy ::= SkipCerts

   SkipCerts ::= INTEGER (0..MAX)
*/
static void SecCEPInhibitAnyPolicy(SecCertificateRefP certificate,
	const SecCertificateExtension *extn) {
	secdebug("cert", "critical: %s", extn->critical ? "yes" : "no");
    require_noerr_quiet(DERParseInteger(
        &extn->extnValue,
        &certificate->_inhibitAnyPolicySkipCerts), badDER);
    return;
badDER:
    certificate->_inhibitAnyPolicySkipCerts = UINT32_MAX;
	secdebug(NULL, "Invalid InhibitAnyPolicy Extension");
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
static void SecCEPAuthorityInfoAccess(SecCertificateRefP certificate,
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
            secdebug("cert", "bad general name for id-ad-ocsp AccessDescription t: 0x%02x v: %.*s",
                generalNameContent.tag, generalNameContent.content.length, generalNameContent.content.data);
            goto badDER;
            break;
        }
    }
    require_quiet(drtn == DR_EndOfSequence, badDER);
	return;
badDER:
    secdebug("cert", "failed to parse Authority Information Access extension");
}

static void SecCEPSubjectInfoAccess(SecCertificateRefP certificate,
	const SecCertificateExtension *extn) {
	secdebug("cert", "critical: %s", extn->critical ? "yes" : "no");
}

static void SecCEPNetscapeCertType(SecCertificateRefP certificate,
	const SecCertificateExtension *extn) {
	secdebug("cert", "critical: %s", extn->critical ? "yes" : "no");
}

static void SecCEPEntrustVersInfo(SecCertificateRefP certificate,
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

static void SecCertificateRegisterClass(void) {
	static const CFRuntimeClass kSecCertificateClass = {
		0,												/* version */
        "SecCertificate",					     		/* class name */
		NULL,											/* init */
		NULL,											/* copy */
		SecCertificateDestroy,                          /* dealloc */
		SecCertificateEqual,							/* equal */
		SecCertificateHash,								/* hash */
		NULL,											/* copyFormattingDesc */
		SecCertificateDescribe                          /* copyDebugDesc */
	};

    kSecCertificateTypeID = _CFRuntimeRegisterClass(&kSecCertificateClass);

	/* Build a dictionary that maps from extension OIDs to callback functions
	   which can parse the extension of the type given. */
	static const void *extnOIDs[] = {
		&oidSubjectKeyIdentifier,
		&oidKeyUsage,
		&oidPrivateKeyUsagePeriod,
		&oidSubjectAltName,
		&oidIssuerAltName,
		&oidBasicConstraints,
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
		&oidEntrustVersInfo
	};
	static const void *extnParsers[] = {
		SecCEPSubjectKeyIdentifier,
		SecCEPKeyUsage,
		SecCEPPrivateKeyUsagePeriod,
		SecCEPSubjectAltName,
		SecCEPIssuerAltName,
		SecCEPBasicConstraints,
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
		SecCEPEntrustVersInfo
	};
	gExtensionParsers = CFDictionaryCreate(kCFAllocatorDefault, extnOIDs,
		extnParsers, sizeof(extnOIDs) / sizeof(*extnOIDs),
		&SecDERItemKeyCallBacks, NULL);
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

/* AUDIT[securityd]:
   certificate->_der is a caller provided data of any length (might be 0).

   Top level certificate decode.
 */
static bool SecCertificateParse(SecCertificateRefP certificate)
{
	DERReturn drtn;

    check(certificate);
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

	/* sequence we're given: encoded DERSubjPubKeyInfo */
	DERSubjPubKeyInfo pubKeyInfo;
	drtn = DERParseSequenceContent(&tbsCert.subjectPubKey,
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

        /* Put some upper limit on the number of extentions allowed. */
        require_quiet(extensionCount < 10000, badCert);
        certificate->_extensionCount = extensionCount;
        certificate->_extensions =
            malloc(sizeof(SecCertificateExtension) * extensionCount);

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
            require_noerr_quiet(drtn = DERParseBoolean(&extn.critical, false,
                &certificate->_extensions[ix].critical), badCert);
            certificate->_extensions[ix].extnValue = extn.extnValue;

			SecCertificateExtensionParser parser =
				(SecCertificateExtensionParser)CFDictionaryGetValue(
				gExtensionParsers, &certificate->_extensions[ix].extnID);
			if (parser) {
				/* Invoke the parser. */
				parser(certificate, &certificate->_extensions[ix]);
			} else if (certificate->_extensions[ix].critical) {
				secdebug("cert", "Found unknown critical extension");
				certificate->_foundUnknownCriticalExtension = true;
			} else {
				secdebug("cert", "Found unknown non critical extension");
			}
        }
    }

	return true;

badCert:
	return false;
}


/* Public API functions. */
CFTypeID SecCertificateGetTypeIDP(void) {
    pthread_once(&kSecCertificateRegisterClass, SecCertificateRegisterClass);
    return kSecCertificateTypeID;
}

SecCertificateRefP SecCertificateCreateWithBytes(CFAllocatorRef allocator,
	const UInt8 *der_bytes, CFIndex der_length) {
	check(der_bytes);
	check(der_length);
    CFIndex size = sizeof(struct __SecCertificate) + der_length;
    SecCertificateRefP result = (SecCertificateRefP)_CFRuntimeCreateInstance(
		allocator, SecCertificateGetTypeIDP(), size - sizeof(CFRuntimeBase), 0);
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
SecCertificateRefP SecCertificateCreate(CFAllocatorRef allocator,
	const UInt8 *der_bytes, CFIndex der_length);

SecCertificateRefP SecCertificateCreate(CFAllocatorRef allocator,
	const UInt8 *der_bytes, CFIndex der_length) {
    return SecCertificateCreateWithBytes(allocator, der_bytes, der_length);
}
/* @@@ End of placeholder. */

/* AUDIT[securityd](done):
   der_certificate is a caller provided data of any length (might be 0), only
   its cf type has been checked.
 */
SecCertificateRefP SecCertificateCreateWithDataP(CFAllocatorRef allocator,
	CFDataRef der_certificate) {
	check(der_certificate);
    CFIndex size = sizeof(struct __SecCertificate);
    SecCertificateRefP result = (SecCertificateRefP)_CFRuntimeCreateInstance(
		allocator, SecCertificateGetTypeIDP(), size - sizeof(CFRuntimeBase), 0);
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

CFDataRef SecCertificateCopyDataP(SecCertificateRefP certificate) {
	check(certificate);
    CFDataRef result;
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

CFIndex SecCertificateGetLength(SecCertificateRefP certificate) {
	return certificate->_der.length;
}

const UInt8 *SecCertificateGetBytePtr(SecCertificateRefP certificate) {
	return certificate->_der.data;
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
        return SecFrameworkCopyLocalizedString(CFSTR("<NULL>"),
            CFSTR("SecCertificate"));
    }
	if (oid->length > MAX_OID_SIZE) {
        return SecFrameworkCopyLocalizedString(CFSTR("Oid too long"),
            CFSTR("SecCertificate"));
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
            CFStringAppendFormat(result, NULL, CFSTR(".%lu"), value);
			value = 0;
		}
	}
	return result;
}

static CFStringRef copyLocalizedOidDescription(CFAllocatorRef allocator,
    const DERItem *oid) {
	if (oid->length == 0) {
        return SecFrameworkCopyLocalizedString(CFSTR("<NULL>"),
            CFSTR("SecCertificate"));
    }

    /* Build the key we use to lookup the localized OID description. */
    CFMutableStringRef oidKey = CFStringCreateMutable(allocator,
        oid->length * 3 + 5);
    CFStringAppendFormat(oidKey, NULL, CFSTR("06 %02X"), oid->length);
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

#if 0
static CFStringRef copyFullOidDescription(CFAllocatorRef allocator,
    const DERItem *oid) {
    CFStringRef decimal = SecDERItemCopyOIDDecimalRepresentation(allocator, oid);
    CFStringRef name = copyLocalizedOidDescription(allocator, oid);
    CFStringRef oid_string = CFStringCreateWithFormat(allocator, NULL,
        CFSTR("%@ (%@)"), name, decimal);
    CFRelease(name);
    CFRelease(decimal);
    return oid_string;
}
#endif

void appendProperty(CFMutableArrayRef properties,
    CFStringRef propertyType, CFStringRef label, CFTypeRef value) {
    CFDictionaryRef property;
    if (label) {
        CFStringRef localizedLabel = SecFrameworkCopyLocalizedString(label,
            CFSTR("SecCertificate"));
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
        CFRelease(localizedLabel);
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
static inline SInt32 parseDecimalPair(const DERByte **p) {
    const DERByte *cp = *p;
    *p += 2;
    return 10 * (cp[0] - '0') + cp[1] - '0';
}

/* Decode a choice of UTCTime or GeneralizedTime to a CFAbsoluteTime. Return
   true if the date was valid and properly decoded, also return the result in
   absTime.  Return false otherwise. */
CFAbsoluteTime SecAbsoluteTimeFromDateContent(DERTag tag, const uint8_t *bytes,
    size_t length) {
	check(bytes);
	if (length == 0)
		return NULL_TIME;

	bool isUtcLength = false;
	bool isLocalized = false;
	bool noSeconds = false;
  	switch (length) {
		case UTC_TIME_NOSEC_ZULU_LEN:		/* YYMMDDhhmmZ */
			isUtcLength = true;
			noSeconds = true;
  			break;
 		case UTC_TIME_ZULU_LEN:				/* YYMMDDhhmmssZ */
			isUtcLength = true;
  			break;
  		case GENERALIZED_TIME_ZULU_LEN:		/* YYYYMMDDhhmmssZ */
  			break;
		case UTC_TIME_LOCALIZED_LEN:		/* YYMMDDhhmmssThhmm (where T=[+,-]) */
			isUtcLength = true;
			/*DROPTHROUGH*/
		case GENERALIZED_TIME_LOCALIZED_LEN:/* YYYYMMDDhhmmssThhmm (where T=[+,-]) */
			isLocalized = true;
			break;
  		default:							/* unknown format */
            return NULL_TIME;
  	}

	/* Make sure the der tag fits the thing inside it. */
	if (tag == ASN1_UTC_TIME) {
		if (!isUtcLength)
            return NULL_TIME;
	} else if (tag == ASN1_GENERALIZED_TIME) {
		if (isUtcLength)
            return NULL_TIME;
	} else {
		return NULL_TIME;
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
			return NULL_TIME;
		}
	}

	/* Initialize the fields in a gregorian date struct. */
    CFGregorianDate gdate;
	if (isUtcLength) {
		SInt32 year = parseDecimalPair(&cp);
		if (year < 50) {
			/* 0  <= year <  50 : assume century 21 */
			gdate.year = 2000 + year;
		} else if (year < 70) {
			/* 50 <= year <  70 : illegal per PKIX */
			return false;
		} else {
			/* 70 <  year <= 99 : assume century 20 */
			gdate.year = 1900 + year;
		}
	} else {
        gdate.year = 100 * parseDecimalPair(&cp) + parseDecimalPair(&cp);
	}
	gdate.month = parseDecimalPair(&cp);
	gdate.day = parseDecimalPair(&cp);
	gdate.hour = parseDecimalPair(&cp);
	gdate.minute = parseDecimalPair(&cp);
	if (noSeconds) {
		gdate.second = 0;
	} else {
		gdate.second = parseDecimalPair(&cp);
	}

	CFTimeInterval timeZoneOffset = 0;
	if (isLocalized) {
		/* ZONE INDICATOR */
        SInt32 multiplier = *cp++ == '+' ? 60 : -60;
        timeZoneOffset = multiplier *
            (parseDecimalPair(&cp) + 60 * parseDecimalPair(&cp));
	} else {
		timeZoneOffset = 0;
	}

    secdebug("dateparse",
        "date %.*s year: %04d-%02d-%02d %02d:%02d:%02.f %+05.f",
        length, bytes, gdate.year, gdate.month,
        gdate.day, gdate.hour, gdate.minute, gdate.second,
        timeZoneOffset / 60);

    if (!CFGregorianDateIsValid(gdate, kCFGregorianAllUnits))
		return false;
	CFTimeZoneRef timeZone = CFTimeZoneCreateWithTimeIntervalFromGMT(NULL,
		timeZoneOffset);
	if (!timeZone)
		return NULL_TIME;
	CFAbsoluteTime absTime = CFGregorianDateGetAbsoluteTime(gdate, timeZone);
	CFRelease(timeZone);
	return absTime;
}

static bool derDateContentGetAbsoluteTime(DERTag tag, const DERItem *date,
    CFAbsoluteTime *pabsTime) {
    CFAbsoluteTime absTime = SecAbsoluteTimeFromDateContent(tag, date->data,
        date->length);
    if (absTime == NULL_TIME)
        return false;

    *pabsTime = absTime;
    return true;
}

/* Decode a choice of UTCTime or GeneralizedTime to a CFAbsoluteTime. Return
   true if the date was valid and properly decoded, also return the result in
   absTime.  Return false otherwise. */
static bool derDateGetAbsoluteTime(const DERItem *dateChoice,
	CFAbsoluteTime *absTime) {
	check(dateChoice);
	check(absTime);
	if (dateChoice->length == 0)
		return false;

	DERDecodedInfo decoded;
	if (DERDecodeItem(dateChoice, &decoded))
		return false;

    return derDateContentGetAbsoluteTime(decoded.tag, &decoded.content,
        absTime);
}

static void appendDataProperty(CFMutableArrayRef properties,
    CFStringRef label, const DERItem *der_data) {
    CFDataRef data = CFDataCreate(CFGetAllocator(properties),
        der_data->data, der_data->length);
    appendProperty(properties, kSecPropertyTypeData, label, data);
    CFRelease(data);
}

static void appendUnparsedProperty(CFMutableArrayRef properties,
    CFStringRef label, const DERItem *der_data) {
    CFStringRef newLabel = CFStringCreateWithFormat(CFGetAllocator(properties),
		NULL, CFSTR("Unparsed %@"), label);
    appendDataProperty(properties, newLabel, der_data);
    CFRelease(newLabel);
}

static void appendInvalidProperty(CFMutableArrayRef properties,
    CFStringRef label, const DERItem *der_data) {
    CFStringRef newLabel = CFStringCreateWithFormat(CFGetAllocator(properties),
		NULL, CFSTR("Invalid %@"), label);
    appendDataProperty(properties, newLabel, der_data);
    CFRelease(newLabel);
}

static void appendDateContentProperty(CFMutableArrayRef properties,
    CFStringRef label, DERTag tag, const DERItem *dateContent) {
	CFAbsoluteTime absTime;
	if (!derDateContentGetAbsoluteTime(tag, dateContent, &absTime)) {
		/* Date decode failure insert hex bytes instead. */
		return appendInvalidProperty(properties, label, dateContent);
	}
    CFDateRef date = CFDateCreate(CFGetAllocator(properties), absTime);
    appendProperty(properties, kSecPropertyTypeDate, label, date);
    CFRelease(date);
}

static void appendDateProperty(CFMutableArrayRef properties,
    CFStringRef label, CFAbsoluteTime absTime) {
    CFDateRef date = CFDateCreate(CFGetAllocator(properties), absTime);
    appendProperty(properties, kSecPropertyTypeDate, label, date);
    CFRelease(date);
}

static void appendIPAddressContentProperty(CFMutableArrayRef properties,
    CFStringRef label, const DERItem *ip) {
	CFStringRef value =
		copyIPAddressContentDescription(CFGetAllocator(properties), ip);
	if (value) {
        appendProperty(properties, kSecPropertyTypeString, label, value);
		CFRelease(value);
	} else {
		appendUnparsedProperty(properties, label, ip);
	}
}

static void appendURLContentProperty(CFMutableArrayRef properties,
    CFStringRef label, const DERItem *urlContent) {
    CFURLRef url = CFURLCreateWithBytes(CFGetAllocator(properties),
        urlContent->data, urlContent->length, kCFStringEncodingASCII, NULL);
    if (url) {
        appendProperty(properties, kSecPropertyTypeURL, label, url);
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
    CFStringRef label, const DERItem *oid) {
    CFStringRef oid_string = copyLocalizedOidDescription(CFGetAllocator(properties),
        oid);
    appendProperty(properties, kSecPropertyTypeString, label, oid_string);
    CFRelease(oid_string);
}

static void appendAlgorithmProperty(CFMutableArrayRef properties,
    CFStringRef label, const DERAlgorithmId *algorithm) {
    CFMutableArrayRef alg_props =
        CFArrayCreateMutable(CFGetAllocator(properties), 0,
            &kCFTypeArrayCallBacks);
    appendOIDProperty(alg_props, CFSTR("Algorithm"), &algorithm->oid);
    if (algorithm->params.length) {
        if (algorithm->params.length == 2 &&
            algorithm->params.data[0] == ASN1_NULL &&
            algorithm->params.data[1] == 0) {
            /* @@@ Localize <NULL> or perhaps skip it? */
            appendProperty(alg_props, kSecPropertyTypeString,
                CFSTR("Parameters"), CFSTR("none"));
        } else {
            appendUnparsedProperty(alg_props, CFSTR("Parameters"),
				&algorithm->params);
        }
    }
    appendProperty(properties, kSecPropertyTypeSection, label, alg_props);
    CFRelease(alg_props);
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

static CFStringRef copyBlobString(CFAllocatorRef allocator,
    CFStringRef blobType, CFStringRef quanta, const DERItem *blob) {
    CFStringRef blobFormat = SecFrameworkCopyLocalizedString(
        CFSTR("%@; %d %@; data = %@"), CFSTR("SecCertificate")
        /*, "format string for encoded field data (e.g. Sequence; 128 bytes; "
            "data = 00 00 ...)" */);
    CFStringRef hex = copyHexDescription(allocator, blob);
    CFStringRef result = CFStringCreateWithFormat(allocator, NULL,
        blobFormat, blobType, blob->length, quanta, hex);
    CFRelease(hex);
    CFRelease(blobFormat);

    return result;
}

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
        return printableOnly ? NULL : copyBlobString(allocator, CFSTR("Byte string"), CFSTR("bytes"),
            derThing);
        //return copyBlobString(BYTE_STRING_STR, BYTES_STR, derThing);
    case ASN1_BIT_STRING:
        return printableOnly ? NULL : copyBlobString(allocator, CFSTR("Bit string"), CFSTR("bits"),
            derThing);
    case (DERByte)ASN1_CONSTR_SEQUENCE:
        return printableOnly ? NULL : copyBlobString(allocator, CFSTR("Sequence"), CFSTR("bytes"),
            derThing);
    case (DERByte)ASN1_CONSTR_SET:
        return printableOnly ? NULL : copyBlobString(allocator, CFSTR("Set"), CFSTR("bytes"),
            derThing);
    case ASN1_OBJECT_ID:
        return printableOnly ? NULL : copyLocalizedOidDescription(allocator, derThing);
    default:
        /* @@@ Localize. */
        /* "format string for undisplayed field data with a given DER tag" */
        return printableOnly ? NULL : CFStringCreateWithFormat(allocator, NULL,
            CFSTR("not displayed (tag = %d; length %d)"),
            tag, derThing->length);
	}
}

static CFStringRef copyDERThingDescription(CFAllocatorRef allocator,
	const DERItem *derThing, bool printableOnly) {
	DERDecodedInfo decoded;
	DERReturn drtn;

	drtn = DERDecodeItem(derThing, &decoded);
    if (drtn) {
        return printableOnly ? NULL : copyHexDescription(allocator, derThing);
    } else {
        return copyDERThingContentDescription(allocator, decoded.tag,
            &decoded.content, false);
    }
}

static void appendDERThingProperty(CFMutableArrayRef properties,
    CFStringRef label, const DERItem *derThing) {
    CFStringRef value = copyDERThingDescription(CFGetAllocator(properties),
        derThing, false);
    appendProperty(properties, kSecPropertyTypeString, label, value);
    CFRelease(value);
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
			appendProperty(properties, kSecPropertyTypeSection, NULL, rdn_props);
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
	CFStringRef label = copyLocalizedOidDescription(CFGetAllocator(properties),
		rdnType);
	if (label) {
		appendDERThingProperty(properties, label, rdnValue);
		CFRelease(label);
		return noErr;
	} else {
		return errSecInvalidCertificate;
	}
}

static CFArrayRef createPropertiesForRDNContent(CFAllocatorRef allocator,
	const DERItem *rdnSetContent) {
	CFMutableArrayRef properties = CFArrayCreateMutable(allocator, 0,
		&kCFTypeArrayCallBacks);
	OSStatus status = parseRDNContent(rdnSetContent, properties,
		appendRDNProperty);
	if (status) {
        CFArrayRemoveAllValues(properties);
		appendInvalidProperty(properties, CFSTR("RDN"), rdnSetContent);
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
        appendInvalidProperty(properties, CFSTR("X.501 Name"), x501NameContent);
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
        appendInvalidProperty(properties, CFSTR("X.501 Name"), x501Name);
	}

	return properties;
}

static void appendIntegerProperty(CFMutableArrayRef properties,
    CFStringRef label, const DERItem *integer) {
    CFStringRef string = copyIntegerContentDescription(
        CFGetAllocator(properties), integer);
    appendProperty(properties, kSecPropertyTypeString, label, string);
    CFRelease(string);
}

static void appendBoolProperty(CFMutableArrayRef properties,
    CFStringRef label, bool boolean) {
    appendProperty(properties, kSecPropertyTypeString,
        label, boolean ? CFSTR("Yes") : CFSTR("No"));
}

static void appendBooleanProperty(CFMutableArrayRef properties,
    CFStringRef label, const DERItem *boolean, bool defaultValue) {
    bool result;
    DERReturn drtn = DERParseBoolean(boolean, defaultValue, &result);
    if (drtn) {
        /* Couldn't parse boolean; dump the raw unparsed data as hex. */
        appendInvalidProperty(properties, label, boolean);
    } else {
        appendBoolProperty(properties, label, result);
    }
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
    bool didOne = false;
    CFMutableStringRef string =
        CFStringCreateMutable(CFGetAllocator(properties), 0);
    for (ix = 0; ix < bits; ++ix) {
        if (value & mask) {
            if (didOne) {
                CFStringAppend(string, CFSTR(", "));
            } else {
                didOne = true;
            }
            CFStringAppend(string, names[ix]);
        }
        mask >>= 1;
    }
    appendProperty(properties, kSecPropertyTypeString, label, string);
    CFRelease(string);
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

#if 0
typedef uint16_t SecKeyUsage;

#define kSecKeyUsageDigitalSignature    0x8000
#define kSecKeyUsageNonRepudiation      0x4000
#define kSecKeyUsageKeyEncipherment     0x2000
#define kSecKeyUsageDataEncipherment    0x1000
#define kSecKeyUsageKeyAgreement        0x0800
#define kSecKeyUsageKeyCertSign         0x0400
#define kSecKeyUsageCRLSign             0x0200
#define kSecKeyUsageEncipherOnly        0x0100
#define kSecKeyUsageDecipherOnly        0x0080

/*
      KeyUsage ::= BIT STRING {
           digitalSignature        (0),
           nonRepudiation          (1),
           keyEncipherment         (2),
           dataEncipherment        (3),
           keyAgreement            (4),
           keyCertSign             (5),
           cRLSign                 (6),
           encipherOnly            (7),
           decipherOnly            (8) }
 */
static void appendKeyUsage(CFMutableArrayRef properties,
    const DERItem *extnValue) {
    if ((extnValue->length != 4 && extnValue->length != 5)  ||
        extnValue->data[0] !=  ASN1_BIT_STRING ||
        extnValue->data[1] < 2 || extnValue->data[1] > 3 ||
        extnValue->data[2] > 7) {
        appendInvalidProperty(properties, CFSTR("KeyUsage Extension"),
            extnValue);
    } else {
        CFMutableStringRef string =
            CFStringCreateMutable(CFGetAllocator(properties), 0);
        SecKeyUsage usage = (extnValue->data[3] << 8);
        if (extnValue->length == 5)
            usage += extnValue->data[4];
        secdebug("keyusage", "keyusage: %04X", usage);
        static const CFStringRef usageNames[] = {
            CFSTR("Digital Signature"),
            CFSTR("Non-Repudiation"),
            CFSTR("Key Encipherment"),
            CFSTR("Data Encipherment"),
            CFSTR("Key Agreement"),
            CFSTR("Cert Sign"),
            CFSTR("CRL Sign"),
            CFSTR("Encipher"),
            CFSTR("Decipher"),
        };
        bool didOne = false;
        SecKeyUsage mask = kSecKeyUsageDigitalSignature;
        CFIndex ix, bits = (extnValue->data[1] - 1) * 8 - extnValue->data[2];
        for (ix = 0; ix < bits; ++ix) {
            if (usage & mask) {
                if (didOne) {
                    CFStringAppend(string, CFSTR(", "));
                } else {
                    didOne = true;
                }
                /* @@@ Localize usageNames[ix]. */
                CFStringAppend(string, usageNames[ix]);
            }
            mask >>= 1;
        }
        appendProperty(properties, kSecPropertyTypeString, CFSTR("Usage"),
            string);
        CFRelease(string);
    }
}
#else
static void appendKeyUsage(CFMutableArrayRef properties,
    const DERItem *extnValue) {
    static const CFStringRef usageNames[] = {
        CFSTR("Digital Signature"),
        CFSTR("Non-Repudiation"),
        CFSTR("Key Encipherment"),
        CFSTR("Data Encipherment"),
        CFSTR("Key Agreement"),
        CFSTR("Cert Sign"),
        CFSTR("CRL Sign"),
        CFSTR("Encipher Only"),
        CFSTR("Decipher Only")
    };
    appendBitStringNames(properties, CFSTR("Usage"), extnValue,
        usageNames, sizeof(usageNames) / sizeof(*usageNames));
}
#endif

static void appendPrivateKeyUsagePeriod(CFMutableArrayRef properties,
    const DERItem *extnValue) {
    DERPrivateKeyUsagePeriod pkup;
	DERReturn drtn = DERParseSequence(extnValue,
        DERNumPrivateKeyUsagePeriodItemSpecs, DERPrivateKeyUsagePeriodItemSpecs,
        &pkup, sizeof(pkup));
	require_noerr_quiet(drtn, badDER);
    if (pkup.notBefore.length) {
        appendDateContentProperty(properties, CFSTR("Not Valid Before"),
            ASN1_GENERALIZED_TIME, &pkup.notBefore);
    }
    if (pkup.notAfter.length) {
        appendDateContentProperty(properties, CFSTR("Not Valid After"),
            ASN1_GENERALIZED_TIME, &pkup.notAfter);
    }
    return;
badDER:
    appendInvalidProperty(properties, CFSTR("Private Key Usage Period"),
        extnValue);
}

static void appendStringContentProperty(CFMutableArrayRef properties,
	CFStringRef label, const DERItem *stringContent,
	CFStringEncoding encoding) {
    CFStringRef string = CFStringCreateWithBytes(CFGetAllocator(properties),
		stringContent->data, stringContent->length, encoding, FALSE);
    if (string) {
		appendProperty(properties, kSecPropertyTypeString, label, string);
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
    CFStringRef oid_string = copyLocalizedOidDescription(allocator,
		&on.typeIdentifier);
	CFStringRef value_string = copyDERThingDescription(allocator, &on.value, false);
	if (value_string)
		appendProperty(properties, kSecPropertyTypeString, oid_string,
			value_string);
	else
        appendUnparsedProperty(properties, oid_string, &on.value);

    return;
badDER:
    appendInvalidProperty(properties, CFSTR("Other Name"), otherNameContent);
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
		appendStringContentProperty(properties, CFSTR("Email Address"),
			generalName, kCFStringEncodingASCII);
		break;
	case ASN1_CONTEXT_SPECIFIC | 2:
		/* IA5String. */
		appendStringContentProperty(properties, CFSTR("DNS Name"), generalName,
			kCFStringEncodingASCII);
		break;
	case ASN1_CONTEXT_SPECIFIC | ASN1_CONSTRUCTED | 3:
		appendUnparsedProperty(properties, CFSTR("X.400 Address"),
			generalName);
		break;
	case ASN1_CONTEXT_SPECIFIC | ASN1_CONSTRUCTED | 4:
	{
		CFArrayRef directory_plist =
			createPropertiesForX501Name(CFGetAllocator(properties),
				generalName);
		appendProperty(properties, kSecPropertyTypeSection,
			CFSTR("Directory Name"), directory_plist);
		CFRelease(directory_plist);
		break;
	}
	case ASN1_CONTEXT_SPECIFIC | ASN1_CONSTRUCTED | 5:
		appendUnparsedProperty(properties, CFSTR("EDI Party Name"),
			generalName);
		break;
	case ASN1_CONTEXT_SPECIFIC | ASN1_CONSTRUCTED | 6:
		/* Technically I don't think this is valid, but there are certs out
		   in the wild that use a constructed IA5String.   In particular the
		   VeriSign Time Stamping Authority CA.cer does this.  */
		appendURLProperty(properties, CFSTR("URI"), generalName);
		break;
	case ASN1_CONTEXT_SPECIFIC | 6:
		appendURLContentProperty(properties, CFSTR("URI"), generalName);
		break;
	case ASN1_CONTEXT_SPECIFIC | 7:
		appendIPAddressContentProperty(properties, CFSTR("IP Address"),
			generalName);
		break;
	case ASN1_CONTEXT_SPECIFIC | 8:
		appendOIDProperty(properties, CFSTR("Registered ID"), generalName);
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
    appendInvalidProperty(properties, CFSTR("General Name"), generalName);
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
    appendInvalidProperty(properties, CFSTR("General Names"),
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
    appendInvalidProperty(properties, CFSTR("General Names"), generalNames);
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

    appendBooleanProperty(properties, CFSTR("Certificate Authority"),
        &basicConstraints.cA, false);

    if (basicConstraints.pathLenConstraint.length != 0) {
        appendIntegerProperty(properties, CFSTR("Path Length Constraint"),
            &basicConstraints.pathLenConstraint);
    }
    return;
badDER:
    appendInvalidProperty(properties, CFSTR("Basic Constraints"), extnValue);
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
					CFSTR("Name Relative To CRL Issuer"), rdn_props);
				CFRelease(rdn_props);
            } else {
                goto badDER;
            }
        }
        if (dp.reasons.length) {
            static const CFStringRef reasonNames[] = {
                CFSTR("Unused"),
                CFSTR("Key Compromise"),
                CFSTR("CA Compromise"),
                CFSTR("Affiliation Changed"),
                CFSTR("Superseded"),
                CFSTR("Cessation Of Operation"),
                CFSTR("Certificate Hold"),
                CFSTR("Priviledge Withdrawn"),
                CFSTR("AA Compromise")
            };
            appendBitStringContentNames(properties, CFSTR("Reasons"),
                &dp.reasons,
                reasonNames, sizeof(reasonNames) / sizeof(*reasonNames));
        }
        if (dp.cRLIssuer.length) {
            CFMutableArrayRef crlIssuer = CFArrayCreateMutable(allocator, 0,
                &kCFTypeArrayCallBacks);
            appendProperty(properties, kSecPropertyTypeSection,
                CFSTR("CRL Issuer"), crlIssuer);
            CFRelease(crlIssuer);
            appendGeneralNames(crlIssuer, &dp.cRLIssuer);
        }
    }
    require_quiet(drtn == DR_EndOfSequence, badDER);
	return;
badDER:
    appendInvalidProperty(properties, CFSTR("Crl Distribution Points"),
		extnValue);
}

/* Decode a sequence of integers into a comma separated list of ints. */
static void appendIntegerSequenceContent(CFMutableArrayRef properties,
    CFStringRef label, const DERItem *intSequenceContent) {
    CFAllocatorRef allocator = CFGetAllocator(properties);
	DERSequence intSeq;
	DERReturn drtn = DERDecodeSeqContentInit(intSequenceContent, &intSeq);
	require_noerr_quiet(drtn, badDER);
	DERDecodedInfo intContent;
	CFMutableStringRef value = NULL;
	while ((drtn = DERDecodeSeqNext(&intSeq, &intContent))
		== DR_Success) {
		require_quiet(intContent.tag == ASN1_INTEGER, badDER);
		CFStringRef intDesc = copyIntegerContentDescription(
			allocator, &intContent.content);
		if (value) {
			CFStringAppendFormat(value, NULL, CFSTR(", %@"), intDesc);
		} else {
			value = CFStringCreateMutableCopy(allocator, 0, intDesc);
		}
		CFRelease(intDesc);
	}
	require_quiet(drtn == DR_EndOfSequence, badDER);
	if (value) {
		appendProperty(properties, kSecPropertyTypeString,
			CFSTR("Notice Numbers"), value);
		CFRelease(value);
		return;
	}
	/* DROPTHOUGH if !value. */
badDER:
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
            CFSTR("Policy Identifier #%d"), pin++);
        appendOIDProperty(properties, piLabel, &pi.policyIdentifier);
        CFRelease(piLabel);
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
                CFSTR("Policy Qualifier #%d"), pqn++);
            appendOIDProperty(properties, pqLabel, &pqi.policyQualifierID);
            CFRelease(pqLabel);
            if (DEROidCompare(&oidQtCps, &pqi.policyQualifierID)) {
                require_quiet(qualifierContent.tag == ASN1_IA5_STRING, badDER);
                appendURLContentProperty(properties,
                    CFSTR("CPS URI"),
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
                        CFSTR("Organization"),
                        &nr.organization);
					appendIntegerSequenceContent(properties,
						CFSTR("Notice Numbers"), &nr.noticeNumbers);
                }
                if (un.explicitText.length) {
                    appendDERThingProperty(properties, CFSTR("Explicit Text"),
                        &un.explicitText);
                }
            } else {
                appendUnparsedProperty(properties, CFSTR("Qualifier"),
                    &pqi.qualifier);
            }
        }
    }
    require_quiet(drtn == DR_EndOfSequence, badDER);
	return;
badDER:
    appendInvalidProperty(properties, CFSTR("Certificate Policies"),
        extnValue);
}

static void appendSubjectKeyIdentifier(CFMutableArrayRef properties,
    const DERItem *extnValue) {
	DERReturn drtn;
    DERDecodedInfo keyIdentifier;
	drtn = DERDecodeItem(extnValue, &keyIdentifier);
	require_noerr_quiet(drtn, badDER);
	require_quiet(keyIdentifier.tag == ASN1_OCTET_STRING, badDER);
	appendDataProperty(properties, CFSTR("Key Identifier"),
		&keyIdentifier.content);

	return;
badDER:
    appendInvalidProperty(properties, CFSTR("Invalid Subject Key Identifier"),
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
		appendDataProperty(properties, CFSTR("Key Identifier"),
			&akid.keyIdentifier);
	}
	if (akid.authorityCertIssuer.length ||
		akid.authorityCertSerialNumber.length) {
		require_quiet(akid.authorityCertIssuer.length &&
			akid.authorityCertSerialNumber.length, badDER);
		/* Perhaps put in a subsection called Authority Certificate Issuer. */
		appendGeneralNamesContent(properties,
			&akid.authorityCertIssuer);
		appendIntegerProperty(properties,
			CFSTR("Authority Certificate Serial Number"),
			&akid.authorityCertSerialNumber);
	}

	return;
badDER:
    appendInvalidProperty(properties, CFSTR("Authority Key Identifier"),
        extnValue);
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
		appendIntegerProperty(properties,
			CFSTR("Require Explicit Policy"), &pc.requireExplicitPolicy);
	}
	if (pc.inhibitPolicyMapping.length) {
		appendIntegerProperty(properties,
			CFSTR("Inhibit Policy Mapping"), &pc.inhibitPolicyMapping);
	}

	return;

badDER:
	appendInvalidProperty(properties, CFSTR("Policy Constraints"), extnValue);
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
        appendOIDProperty(properties, CFSTR("Purpose"),
            &currDecoded.content);
    }
    require_quiet(drtn == DR_EndOfSequence, badDER);
	return;
badDER:
    appendInvalidProperty(properties, CFSTR("Extended Key Usage"), extnValue);
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
        appendOIDProperty(properties, CFSTR("Access Method"),
            &ad.accessMethod);
		//CFSTR("Access Location");
        appendGeneralNameProperty(properties, &ad.accessLocation);
    }
    require_quiet(drtn == DR_EndOfSequence, badDER);
	return;
badDER:
    appendInvalidProperty(properties, CFSTR("Authority Information Access"),
        extnValue);
}

static void appendNetscapeCertType(CFMutableArrayRef properties,
    const DERItem *extnValue) {
    static const CFStringRef certTypes[] = {
        CFSTR("SSL client"),
        CFSTR("SSL server"),
        CFSTR("S/MIME"),
        CFSTR("Object Signing"),
        CFSTR("Reserved"),
        CFSTR("SSL CA"),
        CFSTR("S/MIME CA"),
        CFSTR("Object Signing CA")
    };
    appendBitStringNames(properties, CFSTR("Usage"), extnValue,
        certTypes, sizeof(certTypes) / sizeof(*certTypes));
}

#if 0
static void appendEntrustVersInfo(CFMutableArrayRef properties,
    const DERItem *extnValue) {
}

/*
 * The list of Qualified Cert Statement statementIds we understand, even though
 * we don't actually do anything with them; if these are found in a Qualified
 * Cert Statement that's critical, we can truthfully say "yes we understand this".
 */
static const CSSM_OID_PTR knownQualifiedCertStatements[] =
{
    /* id-qcs := { id-pkix 11 } */
	(const CSSM_OID_PTR)&CSSMOID_OID_QCS_SYNTAX_V1, /* id-qcs 1 */
	(const CSSM_OID_PTR)&CSSMOID_OID_QCS_SYNTAX_V2, /* id-qcs 2 */
	(const CSSM_OID_PTR)&CSSMOID_ETSI_QCS_QC_COMPLIANCE,
	(const CSSM_OID_PTR)&CSSMOID_ETSI_QCS_QC_LIMIT_VALUE,
	(const CSSM_OID_PTR)&CSSMOID_ETSI_QCS_QC_RETENTION,
	(const CSSM_OID_PTR)&CSSMOID_ETSI_QCS_QC_SSCD
};
#define NUM_KNOWN_QUAL_CERT_STATEMENTS (sizeof(knownQualifiedCertStatements) / sizeof(CSSM_OID_PTR))
*/
static void appendQCCertStatements(CFMutableArrayRef properties,
    const DERItem *extnValue) {
}

#endif

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

                appendProperty(properties, kSecPropertyTypeString, label,
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

    appendBoolProperty(properties, CFSTR("Critical"), extn->critical);

#if 1
	bool handeled = true;
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
			handeled = false;
			break;
		case 31: /* CRLDistributionPoints    id-ce 31 */
			appendCrlDistributionPoints(properties, extnValue);
			break;
		case 32: /* CertificatePolicies      id-ce 32 */
			appendCertificatePolicies(properties, extnValue);
			break;
		case 33: /* PolicyMappings           id-ce 33 */
			handeled = false;
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
			handeled = false;
			break;
		case 54: /* InhibitAnyPolicy         id-ce 54 */
			handeled = false;
			break;
		default:
			handeled = false;
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
			handeled = false;
			break;
		case 11: /* SubjectInfoAccess        id-pe 11 */
			appendInfoAccess(properties, extnValue);
			break;
		default:
			handeled = false;
			break;
		}
	} else if (DEROidCompare(extnID, &oidNetscapeCertType)) {
		/* 2.16.840.1.113730.1.1 netscape 1 1 */
		appendNetscapeCertType(properties, extnValue);
	} else {
		handeled = false;
	}

	if (!handeled) {
		/* Try to parse and display printable string(s). */
		if (appendPrintableDERSequence(properties, CFSTR("Data"), extnValue)) {
			/* Nothing to do here appendPrintableDERSequence did the work. */
		} else {
			/* Couldn't parse extension; dump the raw unparsed data as hex. */
			appendUnparsedProperty(properties, CFSTR("Data"), extnValue);
		}
	}
#else
	/* Extensions that we know how to handle ourselves... */
	if (DEROidCompare(extnID, &oidSubjectKeyIdentifier)) {
		appendSubjectKeyIdentifier(properties, extnValue);
	} else if (DEROidCompare(extnID, &oidKeyUsage)) {
		appendKeyUsage(properties, extnValue);
	} else if (DEROidCompare(extnID, &oidPrivateKeyUsagePeriod)) {
		appendPrivateKeyUsagePeriod(properties, extnValue);
	} else if (DEROidCompare(extnID, &oidSubjectAltName)) {
		appendGeneralNames(properties, extnValue);
	} else if (DEROidCompare(extnID, &oidIssuerAltName)) {
		appendGeneralNames(properties, extnValue);
	} else if (DEROidCompare(extnID, &oidBasicConstraints)) {
		appendBasicConstraints(properties, extnValue);
	} else if (DEROidCompare(extnID, &oidCrlDistributionPoints)) {
		appendCrlDistributionPoints(properties, extnValue);
	} else if (DEROidCompare(extnID, &oidCertificatePolicies)) {
		appendCertificatePolicies(properties, extnValue);
	} else if (DEROidCompare(extnID, &oidAuthorityKeyIdentifier)) {
		appendAuthorityKeyIdentifier(properties, extnValue);
	} else if (DEROidCompare(extnID, &oidPolicyConstraints)) {
		appendPolicyConstraints(properties, extnValue);
	} else if (DEROidCompare(extnID, &oidExtendedKeyUsage)) {
		appendExtendedKeyUsage(properties, extnValue);
	} else if (DEROidCompare(extnID, &oidAuthorityInfoAccess)) {
		appendInfoAccess(properties, extnValue);
	} else if (DEROidCompare(extnID, &oidSubjectInfoAccess)) {
		appendInfoAccess(properties, extnValue);
	} else if (DEROidCompare(extnID, &oidNetscapeCertType)) {
		appendNetscapeCertType(properties, extnValue);
#if 0
	} else if (DEROidCompare(extnID, &oidEntrustVersInfo)) {
		appendEntrustVersInfo(properties, extnValue);
#endif
	} else
	/* Try to parse and display printable string(s). */
    if (appendPrintableDERSequence(properties, CFSTR("Data"), extnValue)) {
        /* Nothing to do here appendPrintableDERSequence did the work. */
    } else {
        /* Couldn't parse extension; dump the raw unparsed data as hex. */
        appendUnparsedProperty(properties, CFSTR("Data"), extnValue);
    }
#endif
    CFStringRef oid_string = copyLocalizedOidDescription(allocator, extnID);
    appendProperty(parent, kSecPropertyTypeSection, oid_string, properties);
    CFRelease(oid_string);
    CFRelease(properties);
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
        /* We skip Common Names that have generic values. */
        const char tfm[] = "Thawte Freemail Member";
        if ((value->length == sizeof(tfm) + 1) &&
              !memcmp(value->data + 2, tfm, sizeof(tfm) - 1)) {
            return noErr;
        }
        stype = kSummaryTypeCommonName;
    } else if (DEROidCompare(type, &oidOrganizationalUnitName)) {
        stype = kSummaryTypeOrganizationalUnitName;
    } else if (DEROidCompare(type, &oidOrganizationName)) {
        stype = kSummaryTypeOrganizationName;
    } else if (DEROidCompare(type, &oidDescription)) {
        if (!summary->description) {
            summary->description = string = copyDERThingDescription(kCFAllocatorDefault, value, true);
            CFRetain(string);
        }
        stype = kSummaryTypePrintable;
    } else {
        stype = kSummaryTypePrintable;
    }

    /* Use the first field we encounter of the highest priority type. */
    if (summary->type < stype) {
        if (!string) {
            string = copyDERThingDescription(kCFAllocatorDefault, value, true);
        }

        if (string) {
            CFReleaseSafe(summary->summary);
            summary->summary = string;
            summary->type = stype;
        }
    } else {
        CFReleaseSafe(string);
    }

	return noErr;
}

CFStringRef SecCertificateCopySubjectSummaryP(SecCertificateRefP certificate) {
    struct Summary summary = {};
	parseX501NameContent(&certificate->_subject, &summary, obtainSummaryFromX501Name);
    /* If we found a description and a common name we change the summary to
       CommonName (Description). */
    if (summary.description) {
        if (summary.type == kSummaryTypeCommonName) {
            CFStringRef newSummary = CFStringCreateWithFormat(kCFAllocatorDefault, NULL,
                CFSTR("%@ (%@)"), summary.summary, summary.description);
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

CFStringRef SecCertificateCopyIssuerSummary(SecCertificateRefP certificate) {
    struct Summary summary = {};
	parseX501NameContent(&certificate->_issuer, &summary, obtainSummaryFromX501Name);
    /* If we found a description and a common name we change the summary to
       CommonName (Description). */
    if (summary.description) {
        if (summary.type == kSummaryTypeCommonName) {
            CFStringRef newSummary = CFStringCreateWithFormat(kCFAllocatorDefault, NULL,
                CFSTR("%@ (%@)"), summary.summary, summary.description);
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
    SecCertificateRefP certificate) {
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
    SecCertificateRefP certificate) {
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

bool SecCertificateIsValid(SecCertificateRefP certificate,
	CFAbsoluteTime verifyTime) {
	check(certificate);
    return certificate->_notBefore <= verifyTime &&
		verifyTime <= certificate->_notAfter;
}

CFIndex SecCertificateVersion(SecCertificateRefP certificate) {
	return certificate->_version + 1;
}

CFAbsoluteTime SecCertificateNotValidBefore(SecCertificateRefP certificate) {
	return certificate->_notBefore;
}

CFAbsoluteTime SecCertificateNotValidAfter(SecCertificateRefP certificate) {
	return certificate->_notAfter;
}

CFMutableArrayRef SecCertificateCopySummaryProperties(
    SecCertificateRefP certificate, CFAbsoluteTime verifyTime) {
    CFAllocatorRef allocator = CFGetAllocator(certificate);
    CFMutableArrayRef summary = CFArrayCreateMutable(allocator, 0,
        &kCFTypeArrayCallBacks);

    /* First we put the subject summary name. */
    CFStringRef ssummary = SecCertificateCopySubjectSummaryP(certificate);
    if (ssummary) {
        appendProperty(summary, kSecPropertyTypeTitle,
            NULL, ssummary);
        CFRelease(ssummary);
    }
#if 0
    CFStringRef isummary = CFSTR("Issuer Summary");
    appendProperty(summary, kSecPropertyTypeString,
        CFSTR("Issued By"), isummary);
    CFRelease(isummary);
#endif

    /* Let see if this certificate is currently valid. */
    CFStringRef label;
    CFAbsoluteTime when;
    CFStringRef message;
    CFStringRef ptype;
    if (verifyTime > certificate->_notAfter) {
        label = CFSTR("Expired");
        when = certificate->_notAfter;
        ptype = kSecPropertyTypeError;
        message = CFSTR("This certificate has expired");
    } else if (certificate->_notBefore > verifyTime) {
        label = CFSTR("Valid from");
        when = certificate->_notBefore;
        ptype = kSecPropertyTypeError;
        message = CFSTR("This certificate is not yet valid");
    } else {
        CFAbsoluteTime last = SecCertificateGetChainsLastValidity(certificate);
        CFAbsoluteTime first = SecCertificateGetChainsFirstValidity(certificate);
        if (verifyTime > last) {
            label = CFSTR("Expired");
            when = last;
            ptype = kSecPropertyTypeError;
            message = CFSTR("This certificate has an issuer that has expired");
        } else if (verifyTime < first) {
            label = CFSTR("Valid from");
            when = first;
            ptype = kSecPropertyTypeError;
            message = CFSTR("This certificate has an issuer that is not yet valid");
        } else {
            label = CFSTR("Expires");
            when = certificate->_notAfter;
            ptype = kSecPropertyTypeSuccess;
            message = CFSTR("This certificate is valid");
        }
    }

    appendDateProperty(summary, label, when);
    appendProperty(summary, ptype, NULL, message);

	return summary;
}

CFArrayRef SecCertificateCopyProperties(SecCertificateRefP certificate) {
	if (!certificate->_properties) {
		CFAllocatorRef allocator = CFGetAllocator(certificate);
		CFMutableArrayRef properties = CFArrayCreateMutable(allocator, 0,
			&kCFTypeArrayCallBacks);

        /* First we put the Subject Name in the property list. */
		CFArrayRef subject_plist = createPropertiesForX501NameContent(allocator,
                &certificate->_subject);
        appendProperty(properties, kSecPropertyTypeSection,
            CFSTR("Subject Name"), subject_plist);
		CFRelease(subject_plist);

#if 0
        /* Put Normalized subject in for testing. */
		if (certificate->_normalizedSubject) {
			DERItem nsubject = {
				(DERByte *)CFDataGetBytePtr(certificate->_normalizedSubject),
				CFDataGetLength(certificate->_normalizedSubject)
			};
			CFArrayRef nsubject_plist = createPropertiesForX501NameContent(allocator,
					&nsubject);
			appendProperty(properties, kSecPropertyTypeSection,
				CFSTR("Normalized Subject Name"), nsubject_plist);
			CFRelease(nsubject_plist);
		}
#endif

		/* Next we put the Issuer Name in the property list. */
		CFArrayRef issuer_plist = createPropertiesForX501NameContent(allocator,
			&certificate->_issuer);
        appendProperty(properties, kSecPropertyTypeSection,
            CFSTR("Issuer Name"), issuer_plist);
		CFRelease(issuer_plist);

#if 0
        /* Certificate version/type. */
        bool isRoot = false;
        CFStringRef typeString = CFStringCreateWithFormat(allocator, NULL,
            CFSTR("X.509 version %d %scertificate"),
                certificate->_version + 1, isRoot ? "root " : "");
        appendProperty(properties, kSecPropertyTypeString,
            CFSTR("Certificate Type"), typeString);
        CFRelease(typeString);
#endif

		/* Version */
        CFStringRef versionString = CFStringCreateWithFormat(allocator,
            NULL, CFSTR("%d"), certificate->_version + 1);
        appendProperty(properties, kSecPropertyTypeString,
            CFSTR("Version"), versionString);
        CFRelease(versionString);

		/* Serial Number */
        if (certificate->_serialNum.length) {
            appendIntegerProperty(properties, CFSTR("Serial Number"),
                &certificate->_serialNum);
        }

        /* Signature algorithm. */
#if 0
        appendAlgorithmProperty(properties, CFSTR("Signature Algorithm"),
            &certificate->_sigAlg);
#endif
        appendAlgorithmProperty(properties, CFSTR("Signature Algorithm"),
            &certificate->_tbsSigAlg);


        /* Validity dates. */
        appendDateProperty(properties, CFSTR("Not Valid Before"),
            certificate->_notBefore);
        appendDateProperty(properties, CFSTR("Not Valid After"),
            certificate->_notAfter);

        if (certificate->_subjectUniqueID.length) {
            appendDataProperty(properties, CFSTR("Subject Unique ID"),
                &certificate->_subjectUniqueID);
        }
        if (certificate->_issuerUniqueID.length) {
            appendDataProperty(properties, CFSTR("Issuer Unique ID"),
                &certificate->_issuerUniqueID);
        }

        /* Public key algorithm. */
        appendAlgorithmProperty(properties, CFSTR("Public Key Algorithm"),
            &certificate->_algId);

        /* Consider breaking down an RSA public key into modulus and
           exponent? */
        appendDataProperty(properties, CFSTR("Public Key Data"),
            &certificate->_pubKeyDER);
		/* @@@ Key Size. */
		/* @@@ Key Usage. */

        appendDataProperty(properties, CFSTR("Signature"),
            &certificate->_signature);

        CFIndex ix;
        for (ix = 0; ix < certificate->_extensionCount; ++ix) {
            appendExtension(properties, &certificate->_extensions[ix]);
        }

		/* @@@ Key Fingerprints. */

		certificate->_properties = properties;
	}

    CFRetain(certificate->_properties);
	return certificate->_properties;
}

CFDataRef SecCertificateCopySerialNumberP(
    SecCertificateRefP certificate) {
	if (certificate->_serialNumber) {
		CFRetain(certificate->_serialNumber);
	}
    return certificate->_serialNumber;
}

CFDataRef SecCertificateGetNormalizedIssuerContent(
    SecCertificateRefP certificate) {
    return certificate->_normalizedIssuer;
}

CFDataRef SecCertificateGetNormalizedSubjectContent(
    SecCertificateRefP certificate) {
	DERItem tmpdi;
	tmpdi.data = (DERByte *)CFDataGetBytePtr(certificate->_normalizedSubject);
	tmpdi.length = CFDataGetLength(certificate->_normalizedSubject);

    return SecDERItemCopySequence(&tmpdi);
}

CFDataRef SecCertificateGetNormalizedIssuer(
    SecCertificateRefP certificate) {
	DERItem tmpdi;
	tmpdi.data = (DERByte *)CFDataGetBytePtr(certificate->_normalizedIssuer);
	tmpdi.length = CFDataGetLength(certificate->_normalizedIssuer);

    return SecDERItemCopySequence(&tmpdi);
}

CFDataRef SecCertificateGetNormalizedSubject(
    SecCertificateRefP certificate) {
    return certificate->_normalizedSubject;
}

/* Verify that certificate was signed by issuerKey. */
OSStatus SecCertificateIsSignedByP(SecCertificateRefP certificate,
	SecKeyRefP issuerKey) {
    /* Setup algId in SecAsn1AlgId format. */
    SecAsn1AlgId algId;
    algId.algorithm.Length = certificate->_tbsSigAlg.oid.length;
    algId.algorithm.Data = certificate->_tbsSigAlg.oid.data;
    algId.parameters.Length = certificate->_tbsSigAlg.params.length;
    algId.parameters.Data = certificate->_tbsSigAlg.params.data;

#warning implementation empty
#if 0
    OSStatus status = SecKeyDigestAndVerify(issuerKey, &algId,
        certificate->_tbs.data, certificate->_tbs.length,
        certificate->_signature.data, certificate->_signature.length);
	if (status) {
		secdebug("verify", "signature verify failed: %d", status);
		return errSecNotSigner;
	}
#endif

    return noErr;
}

#if 0
static OSStatus SecCertificateIsIssuedBy(SecCertificateRefP certificate,
	SecCertificateRefP issuer, bool signatureCheckOnly) {
    if (!signatureCheckOnly) {
        /* It turns out we don't actually need to use normalized subject and
           issuer according to rfc2459.  */

        /* If present we should check issuerID against the issuer subjectID. */

        /* If we have an AuthorityKeyIdentifier extension that has a keyIdentifier
           then we should look for a SubjectKeyIdentifier in the issuer
           certificate.
           If we have a authorityCertSerialNumber we can use that for chaining.
           If we have a authorityCertIssuer we can use that? (or not)  */

        /* Verify that this cert was issued by issuer. Do so by chaining
           either issuerID to subjectID or normalized issuer to normalized
           subject. */
        CFDataRef normalizedIssuer =
            SecCertificateGetNormalizedIssuerContent(certificate);
        CFDataRef normalizedIssuerSubject =
            SecCertificateGetNormalizedSubjectContent(issuer);
        if (normalizedIssuer && normalizedIssuerSubject &&
            !CFEqual(normalizedIssuer, normalizedIssuerSubject))
            return errSecIssuerMismatch;
    }

	/* Next verify that this cert was signed by issuer. */
	SecKeyRef issuerKey = SecCertificateGetPublicKey(issuer);

	/* Get the encodedDigestInfo from the digest of the subject's TBSCert */
	/* FIXME: We sould cache this (or at least the digest) until we find
	   a suitable issuer. */
	uint8_t signedData[DER_SHA1_DIGEST_INFO_LEN];
	CFIndex signedDataLength;
	CertVerifyReturn crtn;
	if (DEROidCompare(&certificate->_tbsSigAlg.oid, &oidSha1Rsa)) {
		signedDataLength = DER_SHA1_DIGEST_INFO_LEN;
		crtn = sha1DigestInfo(&certificate->_tbs, signedData);
	} else if(DEROidCompare(&certificate->_tbsSigAlg.oid, &oidMd5Rsa)) {
		signedDataLength = DER_MD_DIGEST_INFO_LEN;
		crtn = mdDigestInfo(WD_MD5, &certificate->_tbs, signedData);
	} else if(DEROidCompare(&certificate->_tbsSigAlg.oid, &oidMd2Rsa)) {
		signedDataLength = DER_MD_DIGEST_INFO_LEN;
		crtn = mdDigestInfo(WD_MD2, &certificate->_tbs, signedData);
	} else {
		secdebug("verify", "unsupported algorithm");
		return errSecUnsupportedAlgorithm;
	}
	if (crtn) {
		secdebug("verify", "*DigestInfo returned: %d", crtn);
        /* FIXME: Do proper error code translation. */
		return errSecUnsupportedAlgorithm;
	}

	OSStatus status = SecKeyRawVerify(issuerKey, kSecPaddingPKCS1,
		signedData, signedDataLength,
		certificate->_signature.data, certificate->_signature.length);
	if (status) {
		secdebug("verify", "signature verify failed: %d", status);
		return errSecNotSigner;
	}

    return noErr;
}

static OSStatus _SecCertificateSetParent(SecCertificateRefP certificate,
	SecCertificateRefP issuer, bool signatureCheckOnly) {
	check(issuer);
    if (certificate->_parent) {
		/* Setting a certificates issuer twice is only allowed if the new
		   issuer is equal to the current one. */
        return issuer && CFEqual(certificate->_parent, issuer);
    }

#if 0
    OSStatus status = SecCertificateIsIssuedBy(certificate, issuer,
        signatureCheckOnly);
#else
	OSStatus status = noErr;
#endif
    if (!status) {
        if (CFEqual(certificate, issuer)) {
            /* We don't retain ourselves cause that would be bad mojo,
               however we do record that we are properly self signed. */
            certificate->_isSelfSigned = kSecSelfSignedTrue;
            secdebug("cert", "set self as parent");
            return noErr;
        }

        CFRetain(issuer);
        certificate->_parent = issuer;
        certificate->_isSelfSigned = kSecSelfSignedFalse;
    }

    return status;
}

static bool SecCertificateIsSelfSigned(SecCertificateRefP certificate) {
    if (certificate->_isSelfSigned == kSecSelfSignedUnknown) {
        certificate->_isSelfSigned =
            (SecCertificateIsIssuedBy(certificate, certificate, false) ?
             kSecSelfSignedTrue : kSecSelfSignedFalse);
    }

    return certificate->_isSelfSigned == kSecSelfSignedTrue;
}

/* Return true iff we were able to set our own parent from one of the
   certificates in other_certificates, return false otherwise.   If
   signatureCheckOnly is true, we can skip the subject == issuer or
   authorityKeyIdentifier tests. */
static bool SecCertificateSetParentFrom(SecCertificateRefP certificate,
    CFArrayRef other_certificates, bool signatureCheckOnly) {
    CFIndex count = CFArrayGetCount(other_certificates);
    CFIndex ix;
    for (ix = 0; ix < count; ++ix) {
        SecCertificateRefP candidate = (SecCertificateRefP)
            CFArrayGetValueAtIndex(other_certificates, ix);
        if (_SecCertificateSetParent(certificate, candidate,
            signatureCheckOnly))
            return true;
    }
    return false;
}

/* Lookup the parent of certificate in the keychain and set it. */
static bool SecCertificateFindParent(SecCertificateRefP certificate) {
    /* FIXME: Search for things other than just subject of our issuer if we
       have a subjectID or authorityKeyIdentifier. */
    CFDataRef normalizedIssuer =
        SecCertificateGetNormalizedIssuerContent(certificate);
    const void *keys[] = {
        kSecClass,
        kSecReturnRef,
        kSecMatchLimit,
        kSecAttrSubject
    },
    *values[] = {
        kSecClassCertificate,
        kCFBooleanTrue,
        kSecMatchLimitAll,
        normalizedIssuer
    };
    CFDictionaryRef query = CFDictionaryCreate(NULL, keys, values, 4,
        &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFTypeRef results;
    OSStatus status = SecItemCopyMatching(query, &results);
    CFRelease(query);
    if (status) {
		secdebug("cert", "SecCertificateFindParent: SecItemCopyMatching: %d",
            status);
        return false;
    }
    CFArrayRef certs = (CFArrayRef)results;
    /* Since we already know the certificates we are providing as candidates
       have been checked for subject matching, we can ask
       SecCertificateSetParentFrom to skip everything except the signature
       checks. */
    bool result = SecCertificateSetParentFrom(certificate, certs, true);
    CFRelease(certs);
    return result;
}

OSStatus SecCertificateCompleteChain(SecCertificateRefP certificate,
	CFArrayRef other_certificates) {
    for (;;) {
        if (certificate->_parent == NULL) {
            if (SecCertificateIsSelfSigned(certificate))
                return noErr;
            if (!other_certificates ||
                !SecCertificateSetParentFrom(certificate, other_certificates,\
                    false)) {
                if (!SecCertificateFindParent(certificate))
                    return errSecIssuerNotFound;
            }
        }
        certificate = certificate->_parent;
    }
}
#endif

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
	return noErr;
}

CFArrayRef SecCertificateCopyIPAddresses(SecCertificateRefP certificate) {
	/* These can only exist in the subject alt name. */
	if (!certificate->_subjectAltName)
		return NULL;

	CFMutableArrayRef ipAddresses = CFArrayCreateMutable(kCFAllocatorDefault,
		0, &kCFTypeArrayCallBacks);
	OSStatus status = parseGeneralNames(&certificate->_subjectAltName->extnValue,
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
	return noErr;
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
	CFStringInlineBuffer buf;
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

	bool nonAlpha = false;
	for (ix = 0; ix < length; ++ix) {
		UniChar ch = CFStringGetCharacterFromInlineBuffer(&buf, ix);
		labelLength++;
		if (ch == '.') {
			require_quiet(labelLength <= 64 &&
				(state == kDNSStateAfterAlpha || state == kDNSStateAfterDigit),
				notDNS);
			state = kDNSStateAfterDot;
			labelLength = 0;
			nonAlpha = false;
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
			nonAlpha = true;
		} else if (ch == '-') {
			require_quiet(state == kDNSStateAfterAlpha ||
				state == kDNSStateAfterDigit ||
				state == kDNSStateAfterDash, notDNS);
			state = kDNSStateAfterDash;
			nonAlpha = true;
		} else {
			goto notDNS;
		}
	}

	/* We don't allow a dns name to end in a dot, and we require the
	   final name component to only have alphanumeric chars.  */
	require_quiet(!nonAlpha && labelLength <= 63 &&
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
	return noErr;
}

/* Not everything returned by this function is going to be a proper DNS name,
   we also return the certificates common name entries from the subject,
   assuming they look like dns names as specified in RFC 1035. */
CFArrayRef SecCertificateCopyDNSNames(SecCertificateRefP certificate) {
	/* These can exist in the subject alt name or in the subject. */
	CFMutableArrayRef dnsNames = CFArrayCreateMutable(kCFAllocatorDefault,
		0, &kCFTypeArrayCallBacks);
	OSStatus status = noErr;
	if (certificate->_subjectAltName) {
		status = parseGeneralNames(&certificate->_subjectAltName->extnValue,
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
	return noErr;
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
	return noErr;
}

CFArrayRef SecCertificateCopyRFC822Names(SecCertificateRefP certificate) {
	/* These can exist in the subject alt name or in the subject. */
	CFMutableArrayRef rfc822Names = CFArrayCreateMutable(kCFAllocatorDefault,
		0, &kCFTypeArrayCallBacks);
	OSStatus status = noErr;
	if (certificate->_subjectAltName) {
		status = parseGeneralNames(&certificate->_subjectAltName->extnValue,
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
	return noErr;
}

CFArrayRef SecCertificateCopyCommonNames(SecCertificateRefP certificate) {
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
	return noErr;
}

CFArrayRef SecCertificateCopyOrganization(SecCertificateRefP certificate) {
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

const SecCEBasicConstraints *
SecCertificateGetBasicConstraints(SecCertificateRefP certificate) {
	if (certificate->_basicConstraints.present)
		return &certificate->_basicConstraints;
	else
		return NULL;
}

const SecCEPolicyConstraints *
SecCertificateGetPolicyConstraints(SecCertificateRefP certificate) {
	if (certificate->_policyConstraints.present)
		return &certificate->_policyConstraints;
	else
		return NULL;
}

CFDictionaryRef
SecCertificateGetPolicyMappings(SecCertificateRefP certificate) {
    return certificate->_policyMappings;
}

const SecCECertificatePolicies *
SecCertificateGetCertificatePolicies(SecCertificateRefP certificate) {
	if (certificate->_certificatePolicies.present)
		return &certificate->_certificatePolicies;
	else
		return NULL;
}

uint32_t
SecCertificateGetInhibitAnyPolicySkipCerts(SecCertificateRefP certificate) {
    return certificate->_inhibitAnyPolicySkipCerts;
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
	return noErr;

badDER:
    return errSecInvalidCertificate;

}

CFArrayRef SecCertificateCopyNTPrincipalNames(SecCertificateRefP certificate) {
	CFMutableArrayRef ntPrincipalNames = CFArrayCreateMutable(kCFAllocatorDefault,
		0, &kCFTypeArrayCallBacks);
	OSStatus status = noErr;
	if (certificate->_subjectAltName) {
		status = parseGeneralNames(&certificate->_subjectAltName->extnValue,
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
#if 0 // represent all labels as oids
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
    } else
#endif
	 {
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
        CFStringInlineBuffer buffer;
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

	return noErr;
}

CFStringRef SecCertificateCopySubjectString(SecCertificateRefP certificate) {
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
        return noErr;

    if (!DEROidCompare(type, &oidOrganizationName))
        return noErr;

    CFStringRef raw;
    raw = copyDERThingDescription(kCFAllocatorDefault, value, true);
    if (!raw)
        return noErr;
    CFStringAppend(string, raw);
    CFRelease(raw);

	return noErr;
}

CFStringRef SecCertificateCopyCompanyName(SecCertificateRefP certificate) {
	CFMutableStringRef string = CFStringCreateMutable(kCFAllocatorDefault, 0);
	OSStatus status = parseX501NameContent(&certificate->_subject, string,
        appendToCompanyNameString);
	if (status || CFStringGetLength(string) == 0) {
		CFRelease(string);
		string = NULL;
	}
	return string;
}

CFDataRef SecDERItemCopySequence(DERItem *content) {
    DERSize seq_len_length = DERLengthOfLength(content->length);
    size_t sequence_length = 1 + seq_len_length + content->length;
	CFMutableDataRef sequence = CFDataCreateMutable(kCFAllocatorDefault,
        sequence_length);
	CFDataSetLength(sequence, sequence_length);
	uint8_t *sequence_ptr = CFDataGetMutableBytePtr(sequence);
    *sequence_ptr++ = 0x30; /* ASN1_CONSTR_SEQUENCE */
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
    SecCertificateRefP certificate) {
    return SecDERItemCopySequence(&certificate->_issuer);
}

CFDataRef SecCertificateCopySubjectSequence(
    SecCertificateRefP certificate) {
    return SecDERItemCopySequence(&certificate->_subject);
}

const DERAlgorithmId *SecCertificateGetPublicKeyAlgorithm(
	SecCertificateRefP certificate) {
	return &certificate->_algId;
}

const DERItem *SecCertificateGetPublicKeyData(SecCertificateRefP certificate) {
	return &certificate->_pubKeyDER;
}

SecKeyRefP SecCertificateCopyPublicKeyP(SecCertificateRefP certificate) {
	SecKeyRefP publicKey = NULL;
#warning implementation empty
#if 0
	const DERAlgorithmId *algId =
		SecCertificateGetPublicKeyAlgorithm(certificate);
	const DERItem *keyData = SecCertificateGetPublicKeyData(certificate);
	if (DEROidCompare(&algId->oid, &oidRsa)) {
		publicKey = SecKeyCreateRSAPublicKey(kCFAllocatorDefault,
			keyData->data, keyData->length, kSecKeyEncodingPkcs1);
    } else {
		secdebug(NULL, "Unsupported algorithm oid");
	}
#endif

    return publicKey;
}

CFDataRef SecCertificateGetSHA1Digest(SecCertificateRefP certificate) {
    if (!certificate->_sha1Digest) {
		certificate->_sha1Digest =
			SecSHA1DigestCreate(CFGetAllocator(certificate),
				certificate->_der.data, certificate->_der.length);
	}

	return certificate->_sha1Digest;
}

CFDataRef SecCertificateCopyIssuerSHA1Digest(SecCertificateRefP certificate) {
    CFDataRef digest = NULL;
    CFDataRef issuer = SecCertificateCopyIssuerSequence(certificate);
    if (issuer) {
        digest = SecSHA1DigestCreate(kCFAllocatorDefault,
            CFDataGetBytePtr(issuer), CFDataGetLength(issuer));
        CFRelease(issuer);
    }
	return digest;
}

CFDataRef SecCertificateCopyPublicKeySHA1Digest(SecCertificateRefP certificate) {
    return SecSHA1DigestCreate(CFGetAllocator(certificate),
        certificate->_pubKeyDER.data, certificate->_pubKeyDER.length);
}

CFDataRef SecCertificateGetAuthorityKeyID(SecCertificateRefP certificate) {
	if (!certificate->_authorityKeyID &&
		certificate->_authorityKeyIdentifier.length) {
		certificate->_authorityKeyID = CFDataCreate(kCFAllocatorDefault,
			certificate->_authorityKeyIdentifier.data,
			certificate->_authorityKeyIdentifier.length);
	}

    return certificate->_authorityKeyID;
}

CFDataRef SecCertificateGetSubjectKeyID(SecCertificateRefP certificate) {
	if (!certificate->_subjectKeyID &&
		certificate->_subjectKeyIdentifier.length) {
		certificate->_subjectKeyID = CFDataCreate(kCFAllocatorDefault,
			certificate->_subjectKeyIdentifier.data,
			certificate->_subjectKeyIdentifier.length);
	}

    return certificate->_subjectKeyID;
}

CFArrayRef SecCertificateGetCRLDistributionPoints(SecCertificateRefP certificate) {
    return certificate->_crlDistributionPoints;
}

CFArrayRef SecCertificateGetOCSPResponders(SecCertificateRefP certificate) {
    return certificate->_ocspResponders;
}

CFArrayRef SecCertificateGetCAIssuers(SecCertificateRefP certificate) {
    return certificate->_caIssuers;
}

bool SecCertificateHasCriticalSubjectAltName(SecCertificateRefP certificate) {
	return certificate->_subjectAltName &&
		certificate->_subjectAltName->critical;
}

bool SecCertificateHasSubject(SecCertificateRefP certificate) {
	/* Since the _subject field is the content of the subject and not the
	   whole thing, we can simply check for a 0 length subject here. */
	return certificate->_subject.length != 0;
}

bool SecCertificateHasUnknownCriticalExtension(SecCertificateRefP certificate) {
	return certificate->_foundUnknownCriticalExtension;
}

/* Private API functions. */
void SecCertificateShow(SecCertificateRefP certificate) {
	check(certificate);
	fprintf(stderr, "SecCertificate instance %p:\n", certificate);
		fprintf(stderr, "\n");
}

CFDictionaryRef SecCertificateCopyAttributeDictionary(
	SecCertificateRefP certificate) {
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
	certData = SecCertificateCopyDataP(certificate);
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

SecCertificateRefP SecCertificateCreateFromAttributeDictionary(
	CFDictionaryRef refAttributes) {
	/* @@@ Support having an allocator in refAttributes. */
 	CFAllocatorRef allocator = NULL;
	CFDataRef data = CFDictionaryGetValue(refAttributes, kSecValueData);
	return SecCertificateCreateWithDataP(allocator, data);
}

bool SecCertificateIsSelfSignedCA(SecCertificateRefP certificate) {
    bool result = false;
    SecKeyRefP publicKey;
    require(publicKey = SecCertificateCopyPublicKeyP(certificate), out);
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

    if (SecCertificateVersion(certificate) >= 3) {
        const SecCEBasicConstraints *basicConstraints = SecCertificateGetBasicConstraints(certificate);
        require_quiet(basicConstraints && basicConstraints->isCA, out);
        require_noerr_quiet(SecCertificateIsSignedByP(certificate, publicKey), out);
    }

    result = true;
out:
    CFReleaseSafe(publicKey);
    return result;
}

SecKeyUsage SecCertificateGetKeyUsage(SecCertificateRefP certificate) {
    return certificate->_keyUsage;
}

CFArrayRef SecCertificateCopyExtendedKeyUsage(SecCertificateRefP certificate)
{
    CFMutableArrayRef extended_key_usage_oids =
        CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
    require_quiet(extended_key_usage_oids, out);
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

SecCertificateRefP SecCertificateCreateWithPEM(CFAllocatorRef allocator,
	CFDataRef pem_certificate)
{
    static const char begin_cert[] = "-----BEGIN CERTIFICATE-----\n";
    static const char end_cert[] = "-----END CERTIFICATE-----\n";
    uint8_t *base64_data = NULL;
    SecCertificateRefP cert = NULL;
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

static void convertCertificateToCFData(const void *value, void *context) {
    CFMutableArrayRef result = (CFMutableArrayRef)context;
    SecCertificateRefP certificate = (SecCertificateRefP)value;
    CFDataRef data = SecCertificateCopyDataP(certificate);
    CFArrayAppendValue(result, data);
    CFRelease(data);
}

/* Return an array of CFDataRefs from an array of SecCertificateRefPs. */
CFArrayRef SecCertificateArrayCopyDataArray(CFArrayRef certificates) {
    CFIndex count = CFArrayGetCount(certificates);
    CFMutableArrayRef result = CFArrayCreateMutable(kCFAllocatorDefault, count, &kCFTypeArrayCallBacks);
    CFRange all_certs = { 0, count };
    CFArrayApplyFunction(certificates, all_certs, convertCertificateToCFData, result);
    return result;
}

/* AUDIT[securityd](done):
   value (ok) is an element in a caller provided array.
 */
static void convertCFDataToCertificate(const void *value, void *context) {
    CFMutableArrayRef result = (CFMutableArrayRef)context;
    CFDataRef data = (CFDataRef)value;
    if (data && CFGetTypeID(data) == CFDataGetTypeID()) {
        SecCertificateRefP certificate = SecCertificateCreateWithDataP(kCFAllocatorDefault, data);
        if (certificate) {
            CFArrayAppendValue(result, certificate);
            CFRelease(certificate);
        }
    }
}

/* AUDIT[securityd](done):
   certificates (ok) is a caller provided array, only its cf type has
   been checked.
 */
CFArrayRef SecCertificateDataArrayCopyArray(CFArrayRef certificates) {
    CFIndex count = CFArrayGetCount(certificates);
    CFMutableArrayRef result = CFArrayCreateMutable(NULL, count, &kCFTypeArrayCallBacks);
    CFRange all_certs = { 0, count };
    CFArrayApplyFunction(certificates, all_certs, convertCFDataToCertificate, result);
    return result;
}

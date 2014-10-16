/*
 * Copyright (c) 2000-2009,2012 Apple Inc. All Rights Reserved.
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
 * CertExtensions.h -- X.509 Cert Extensions as C structs
 */

#ifndef	_CERT_EXTENSIONS_H_
#define _CERT_EXTENSIONS_H_

#include <stdbool.h>
#include <libDER/libDER.h>
//#include <Security/x509defs.h>

/***
 *** Structs for declaring extension-specific data. 
 ***/

/*
 * GeneralName, used in AuthorityKeyID, SubjectAltName, and 
 * IssuerAltName. 
 *
 * For now, we just provide explicit support for the types which are
 * represented as IA5Strings, OIDs, and octet strings. Constructed types
 * such as EDIPartyName and x400Address are not explicitly handled
 * right now and must be encoded and decoded by the caller. (See exception
 * for Name and OtherName, below). In those cases the SecCEGeneralName.name.Data field 
 * represents the BER contents octets; SecCEGeneralName.name.Length is the 
 * length of the contents; the tag of the field is not needed - the BER 
 * encoding uses context-specific implicit tagging. The berEncoded field 
 * is set to true in these case. Simple types have berEncoded = false. 
 *
 * In the case of a GeneralName in the form of a Name, we parse the Name
 * into a CSSM_X509_NAME and place a pointer to the CSSM_X509_NAME in the
 * SecCEGeneralName.name.Data field. SecCEGeneralName.name.Length is set to 
 * sizeof(CSSM_X509_NAME). In this case berEncoded is false. 
 *
 * In the case of a GeneralName in the form of a OtherName, we parse the fields
 * into a SecCEOtherName and place a pointer to the SecCEOtherName in the
 * SecCEGeneralName.name.Data field. SecCEGeneralName.name.Length is set to 
 * sizeof(SecCEOtherName). In this case berEncoded is false. 
 *
 *      GeneralNames ::= SEQUENCE SIZE (1..MAX) OF GeneralName
 *
 *      GeneralName ::= CHOICE {
 *           otherName                       [0]     OtherName
 *           rfc822Name                      [1]     IA5String,
 *           dNSName                         [2]     IA5String,
 *           x400Address                     [3]     ORAddress,
 *           directoryName                   [4]     Name,
 *           ediPartyName                    [5]     EDIPartyName,
 *           uniformResourceIdentifier       [6]     IA5String,
 *           iPAddress                       [7]     OCTET STRING,
 *           registeredID                    [8]     OBJECT IDENTIFIER}
 *
 *      OtherName ::= SEQUENCE {
 *           type-id    OBJECT IDENTIFIER,
 *           value      [0] EXPLICIT ANY DEFINED BY type-id }
 *
 *      EDIPartyName ::= SEQUENCE {
 *           nameAssigner            [0]     DirectoryString OPTIONAL,
 *           partyName               [1]     DirectoryString }
 */
typedef enum {
	GNT_OtherName = 0,
	GNT_RFC822Name,
	GNT_DNSName,
	GNT_X400Address,
	GNT_DirectoryName,
	GNT_EdiPartyName,
	GNT_URI,
	GNT_IPAddress,
	GNT_RegisteredID
} SecCEGeneralNameType;

typedef struct {
	DERItem                 typeId;
	DERItem                 value;		// unparsed, BER-encoded
} SecCEOtherName;

typedef struct {
	SecCEGeneralNameType		nameType;	// GNT_RFC822Name, etc.
	bool                    berEncoded;
	DERItem                 name; 
} SecCEGeneralName;

typedef struct {
	uint32_t					numNames;
	SecCEGeneralName			*generalName;		
} SecCEGeneralNames;	

/*
 * id-ce-authorityKeyIdentifier OBJECT IDENTIFIER ::=  { id-ce 35 }
 *
 *   AuthorityKeyIdentifier ::= SEQUENCE {
 *     keyIdentifier             [0] KeyIdentifier           OPTIONAL,
 *     authorityCertIssuer       [1] GeneralNames            OPTIONAL,
 *     authorityCertSerialNumber [2] CertificateSerialNumber OPTIONAL  }
 *
 *   KeyIdentifier ::= OCTET STRING
 *
 * CSSM OID = CSSMOID_AuthorityKeyIdentifier
 */
typedef struct {
	bool                keyIdentifierPresent;
	DERItem             keyIdentifier;
	bool                generalNamesPresent;
	SecCEGeneralNames		*generalNames;
	bool                serialNumberPresent;
	DERItem             serialNumber;
} SecCEAuthorityKeyID;

/*
 * id-ce-subjectKeyIdentifier OBJECT IDENTIFIER ::=  { id-ce 14 }
 *   SubjectKeyIdentifier ::= KeyIdentifier
 *
 * CSSM OID = CSSMOID_SubjectKeyIdentifier
 */
typedef DERItem SecCESubjectKeyID;

/*
 * id-ce-keyUsage OBJECT IDENTIFIER ::=  { id-ce 15 }
 *
 *     KeyUsage ::= BIT STRING {
 *          digitalSignature        (0),
 *          nonRepudiation          (1),
 *          keyEncipherment         (2),
 *          dataEncipherment        (3),
 *          keyAgreement            (4),
 *          keyCertSign             (5),
 *          cRLSign                 (6),
 *          encipherOnly            (7),
 *          decipherOnly            (8) }
 *
 * CSSM OID = CSSMOID_KeyUsage
 *
 */
typedef uint16_t SecCEKeyUsage;

#define SecCEKU_DigitalSignature	0x8000
#define SecCEKU_NonRepudiation	0x4000
#define SecCEKU_KeyEncipherment	0x2000
#define SecCEKU_DataEncipherment	0x1000
#define SecCEKU_KeyAgreement		0x0800
#define SecCEKU_KeyCertSign	 	0x0400
#define SecCEKU_CRLSign			0x0200
#define SecCEKU_EncipherOnly	 	0x0100
#define SecCEKU_DecipherOnly	 	0x0080

/*
 *  id-ce-cRLReason OBJECT IDENTIFIER ::= { id-ce 21 }
 *
 *   -- reasonCode ::= { CRLReason }
 *
 *   CRLReason ::= ENUMERATED {
 *  	unspecified             (0),
 *      keyCompromise           (1),
 *     	cACompromise            (2),
 *    	affiliationChanged      (3),
 *   	superseded              (4),
 *  	cessationOfOperation    (5),
 * 		certificateHold         (6),
 *		removeFromCRL           (8) }
 *
 * CSSM OID = CSSMOID_CrlReason
 *
 */
typedef uint32_t SecCECrlReason;

#define SecCECR_Unspecified			0
#define SecCECR_KeyCompromise			1
#define SecCECR_CACompromise			2
#define SecCECR_AffiliationChanged	3
#define SecCECR_Superseded			4
#define SecCECR_CessationOfOperation	5
#define SecCECR_CertificateHold		6
#define SecCECR_RemoveFromCRL	 		8

/*
 * id-ce-subjectAltName OBJECT IDENTIFIER ::=  { id-ce 17 }
 *
 *      SubjectAltName ::= GeneralNames
 *
 * CSSM OID = CSSMOID_SubjectAltName
 *
 * GeneralNames defined above.
 */

/*
 *  id-ce-extKeyUsage OBJECT IDENTIFIER ::= {id-ce 37}
 *
 *   ExtKeyUsageSyntax ::= SEQUENCE SIZE (1..MAX) OF KeyPurposeId*
 *
 *  KeyPurposeId ::= OBJECT IDENTIFIER
 *
 * CSSM OID = CSSMOID_ExtendedKeyUsage
 */
typedef struct {
	uint32_t		numPurposes;
	DERItem         *purposes;		// in Intel pre-encoded format
} SecCEExtendedKeyUsage;

/*
 * id-ce-basicConstraints OBJECT IDENTIFIER ::=  { id-ce 19 }
 *
 * BasicConstraints ::= SEQUENCE {
 *       cA                      BOOLEAN DEFAULT FALSE,
 *       pathLenConstraint       INTEGER (0..MAX) OPTIONAL }
 *
 * CSSM OID = CSSMOID_BasicConstraints
 */
typedef struct {
	bool                present;
	bool                critical;
	bool                isCA;
	bool                pathLenConstraintPresent;
	uint32_t			pathLenConstraint;
} SecCEBasicConstraints;	

typedef struct {
	bool                present;
	bool                critical;
	bool                requireExplicitPolicyPresent;
	uint32_t			requireExplicitPolicy;
	bool                inhibitPolicyMappingPresent;
	uint32_t			inhibitPolicyMapping;
} SecCEPolicyConstraints;

/*
 * id-ce-certificatePolicies OBJECT IDENTIFIER ::=  { id-ce 32 }
 *
 *   certificatePolicies ::= SEQUENCE SIZE (1..MAX) OF PolicyInformation
 *
 *   PolicyInformation ::= SEQUENCE {
 *        policyIdentifier   CertPolicyId,
 *        policyQualifiers   SEQUENCE SIZE (1..MAX) OF
 *                                PolicyQualifierInfo OPTIONAL }
 *
 *   CertPolicyId ::= OBJECT IDENTIFIER
 *
 *   PolicyQualifierInfo ::= SEQUENCE {
 *        policyQualifierId  PolicyQualifierId,
 *        qualifier          ANY DEFINED BY policyQualifierId } 
 *
 *   -- policyQualifierIds for Internet policy qualifiers
 *
 *   id-qt          OBJECT IDENTIFIER ::=  { id-pkix 2 }
 *   id-qt-cps      OBJECT IDENTIFIER ::=  { id-qt 1 }
 *   id-qt-unotice  OBJECT IDENTIFIER ::=  { id-qt 2 }
 *
 *   PolicyQualifierId ::=
 *        OBJECT IDENTIFIER ( id-qt-cps | id-qt-unotice )
 *
 *   Qualifier ::= CHOICE {
 *        cPSuri           CPSuri,
 *        userNotice       UserNotice }
 *
 *   CPSuri ::= IA5String
 *
 *   UserNotice ::= SEQUENCE {
 *        noticeRef        NoticeReference OPTIONAL,
 *        explicitText     DisplayText OPTIONAL}
 *
 *   NoticeReference ::= SEQUENCE {
 *        organization     DisplayText,
 *        noticeNumbers    SEQUENCE OF INTEGER }
 *
 *   DisplayText ::= CHOICE {
 *        visibleString    VisibleString  (SIZE (1..200)),
 *        bmpString        BMPString      (SIZE (1..200)),
 *        utf8String       UTF8String     (SIZE (1..200)) }
 *
 *  CSSM OID = CSSMOID_CertificatePolicies
 *
 * We only support down to the level of Qualifier, and then only the CPSuri
 * choice. UserNotice is transmitted to and from this library as a raw
 * CSSM_DATA containing the BER-encoded UserNotice sequence. 
 */
#if 0
typedef struct {
	DERItem     policyQualifierId;			// CSSMOID_QT_CPS, CSSMOID_QT_UNOTICE
	DERItem     qualifier;					// CSSMOID_QT_CPS: IA5String contents
											// CSSMOID_QT_UNOTICE : Sequence contents
} SecCEPolicyQualifierInfo;
#endif

typedef struct {
    DERItem policyIdentifier;
    DERItem policyQualifiers;
} SecCEPolicyInformation;

typedef struct {
	bool                    present;
	bool                    critical;
	size_t                  numPolicies;			// size of *policies;
	SecCEPolicyInformation  *policies;
} SecCECertificatePolicies;

typedef struct {
    DERItem issuerDomainPolicy;
    DERItem subjectDomainPolicy;
} SecCEPolicyMapping;

/*
   PolicyMappings ::= SEQUENCE SIZE (1..MAX) OF SEQUENCE {
        issuerDomainPolicy      CertPolicyId,
        subjectDomainPolicy     CertPolicyId }
*/
typedef struct {
	bool                present;
	bool                critical;
	uint32_t            numMappings;			// size of *mappings;
	SecCEPolicyMapping  *mappings;
} SecCEPolicyMappings;

#if 0
typedef struct {
	bool                    present;
	bool                    critical;
	uint32_t                skipCerts;
} SecCEInhibitAnyPolicy;

/*
 * netscape-cert-type, a bit string.
 *
 * CSSM OID = CSSMOID_NetscapeCertType
 *
 * Bit fields defined in oidsattr.h: SecCENCT_SSL_Client, etc.
 */
typedef uint16_t SecCENetscapeCertType;

/*
 * CRLDistributionPoints.
 *
 *   id-ce-cRLDistributionPoints OBJECT IDENTIFIER ::=  { id-ce 31 }
 *
 *   cRLDistributionPoints ::= {
 *        CRLDistPointsSyntax }
 *
 *   CRLDistPointsSyntax ::= SEQUENCE SIZE (1..MAX) OF DistributionPoint
 *
 *   NOTE: RFC 2459 claims that the tag for the optional DistributionPointName
 *   is IMPLICIT as shown here, but in practice it is EXPLICIT. It has to be -
 *   because the underlying type also uses an implicit tag for distinguish
 *   between CHOICEs.
 *
 *   DistributionPoint ::= SEQUENCE {
 *        distributionPoint       [0]     DistributionPointName OPTIONAL,
 *        reasons                 [1]     ReasonFlags OPTIONAL,
 *        cRLIssuer               [2]     GeneralNames OPTIONAL }
 *
 *   DistributionPointName ::= CHOICE {
 *        fullName                [0]     GeneralNames,
 *        nameRelativeToCRLIssuer [1]     RelativeDistinguishedName }
 *
 *   ReasonFlags ::= BIT STRING {
 *        unused                  (0),
 *        keyCompromise           (1),
 *        cACompromise            (2),
 *        affiliationChanged      (3),
 *        superseded              (4),
 *        cessationOfOperation    (5),
 *        certificateHold         (6) }
 *
 * CSSM OID = CSSMOID_CrlDistributionPoints
 */
 
/*
 * Note that this looks similar to SecCECrlReason, but that's an enum and this
 * is an OR-able bit string.
 */
typedef uint8_t SecCECrlDistReasonFlags;

#define SecCECD_Unspecified			0x80
#define SecCECD_KeyCompromise			0x40
#define SecCECD_CACompromise			0x20
#define SecCECD_AffiliationChanged	0x10
#define SecCECD_Superseded			0x08
#define SecCECD_CessationOfOperation	0x04
#define SecCECD_CertificateHold		0x02

typedef enum {
	SecCECDNT_FullName,
	SecCECDNT_NameRelativeToCrlIssuer
} SecCECrlDistributionPointNameType;

typedef struct {
	SecCECrlDistributionPointNameType		nameType;
	union {
		SecCEGeneralNames					*fullName;
		CSSM_X509_RDN_PTR				rdn;
	} dpn;
} SecCEDistributionPointName;

/*
 * The top-level CRLDistributionPoint.
 * All fields are optional; NULL pointers indicate absence. 
 */
typedef struct {
	SecCEDistributionPointName			*distPointName;
	bool                                reasonsPresent;
	SecCECrlDistReasonFlags				reasons;
	SecCEGeneralNames						*crlIssuer;
} SecCECRLDistributionPoint;

typedef struct {
	uint32_t							numDistPoints;
	SecCECRLDistributionPoint				*distPoints;
} SecCECRLDistPointsSyntax;

/* 
 * Authority Information Access and Subject Information Access.
 *
 * CSSM OID = CSSMOID_AuthorityInfoAccess
 * CSSM OID = CSSMOID_SubjectInfoAccess
 *
 * SubjAuthInfoAccessSyntax  ::=
 *		SEQUENCE SIZE (1..MAX) OF AccessDescription
 * 
 * AccessDescription  ::=  SEQUENCE {
 *		accessMethod          OBJECT IDENTIFIER,
 *		accessLocation        GeneralName  }
 */
typedef struct {
	DERItem                 accessMethod;
	SecCEGeneralName			accessLocation;
} SecCEAccessDescription;

typedef struct {
	uint32_t				numAccessDescriptions;
	SecCEAccessDescription	*accessDescriptions;
} SecCEAuthorityInfoAccess;

/*** CRL extensions ***/

/*
 * cRLNumber, an integer.
 *
 * CSSM OID = CSSMOID_CrlNumber
 */
typedef uint32_t SecCECrlNumber;

/*
 * deltaCRLIndicator, an integer.
 *
 * CSSM OID = CSSMOID_DeltaCrlIndicator
 */
typedef uint32_t SecCEDeltaCrl;

/*
 * IssuingDistributionPoint
 *
 * id-ce-issuingDistributionPoint OBJECT IDENTIFIER ::= { id-ce 28 }
 *
 * issuingDistributionPoint ::= SEQUENCE {
 *      distributionPoint       [0] DistributionPointName OPTIONAL,
 *		onlyContainsUserCerts   [1] BOOLEAN DEFAULT FALSE,
 *      onlyContainsCACerts     [2] BOOLEAN DEFAULT FALSE,
 *      onlySomeReasons         [3] ReasonFlags OPTIONAL,
 *      indirectCRL             [4] BOOLEAN DEFAULT FALSE }
 *
 * CSSM OID = CSSMOID_IssuingDistributionPoint
 */
typedef struct {
	SecCEDistributionPointName	*distPointName;		// optional
	bool                        onlyUserCertsPresent;
	bool                        onlyUserCerts;
	bool                        onlyCACertsPresent;
	bool                        onlyCACerts;
	bool                        onlySomeReasonsPresent;
	SecCECrlDistReasonFlags		onlySomeReasons;
	bool                        indirectCrlPresent;
	bool                        indirectCrl;
} SecCEIssuingDistributionPoint;

/*
 * An enumerated list identifying one of the above per-extension
 * structs.
 */
typedef enum {
	DT_AuthorityKeyID,			// SecCEAuthorityKeyID
	DT_SubjectKeyID,			// SecCESubjectKeyID
	DT_KeyUsage,				// SecCEKeyUsage
	DT_SubjectAltName,			// implies SecCEGeneralName
	DT_IssuerAltName,			// implies SecCEGeneralName
	DT_ExtendedKeyUsage,		// SecCEExtendedKeyUsage
	DT_BasicConstraints,		// SecCEBasicConstraints
	DT_CertPolicies,			// SecCECertPolicies
	DT_NetscapeCertType,		// SecCENetscapeCertType
	DT_CrlNumber,				// SecCECrlNumber
	DT_DeltaCrl,				// SecCEDeltaCrl
	DT_CrlReason,				// SecCECrlReason
	DT_CrlDistributionPoints,	// SecCECRLDistPointsSyntax
	DT_IssuingDistributionPoint,// SecCEIssuingDistributionPoint
	DT_AuthorityInfoAccess,		// SecCEAuthorityInfoAccess
	DT_Other					// unknown, raw data as a CSSM_DATA
} SecCEDataType;

/*
 * One unified representation of all the cert adn CRL extensions we know about.
 */
typedef union {
	SecCEAuthorityKeyID				authorityKeyID;
	SecCESubjectKeyID				subjectKeyID;
	SecCEKeyUsage					keyUsage;
	SecCEGeneralNames				subjectAltName;
	SecCEGeneralNames				issuerAltName;
	SecCEExtendedKeyUsage			extendedKeyUsage;
	SecCEBasicConstraints			basicConstraints;
	SecCECertPolicies				certPolicies;
	SecCENetscapeCertType			netscapeCertType;
	SecCECrlNumber					crlNumber;
	SecCEDeltaCrl					deltaCrl;
	SecCECrlReason					crlReason;
	SecCECRLDistPointsSyntax		crlDistPoints;
	SecCEIssuingDistributionPoint	issuingDistPoint;
	SecCEAuthorityInfoAccess		authorityInfoAccess;
	DERItem							rawData;			// unknown, not decoded
} SecCEData;

typedef struct {
	SecCEDataType				type;
	SecCEData					extension;
	bool						critical;
} SecCEDataAndType;
#endif /* 0 */

#endif	/* _CERT_EXTENSIONS_H_ */

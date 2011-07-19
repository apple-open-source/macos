/*
 * Copyright (c) 2003-2004 Apple Computer, Inc. All Rights Reserved.
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
 * certExtensionTemplates.h - libnssasn1 structs and templates for cert and 
 *                            CRL extensions
 *
 */
 
#ifndef	_CERT_EXTENSION_TEMPLATES_H_
#define _CERT_EXTENSION_TEMPLATES_H_

#include <Security/secasn1t.h>
#include <Security/X509Templates.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Basic Constraints
 * NSS struct  : NSS_BasicConstraints
 * CDSA struct : CE_BasicConstraints
 */
typedef struct {
	CSSM_DATA		cA;					// BOOL
	CSSM_DATA		pathLenConstraint;	// INTEGER optional
} NSS_BasicConstraints;

extern const SecAsn1Template kSecAsn1BasicConstraintsTemplate[];

/* 
 * Key Usage
 * NSS struct  : CSSM_DATA, BIT STRING - length in bits
 * CDSA struct : CE_KeyUsage
 */
#define kSecAsn1KeyUsageTemplate		kSecAsn1BitStringTemplate

/*
 * Extended Key Usage
 * NSS struct  : NSS_ExtKeyUsage
 * CDSA struct : CE_ExtendedKeyUsage
 */
typedef struct {
	CSSM_OID	**purposes;
} NSS_ExtKeyUsage;
#define kSecAsn1ExtKeyUsageTemplate		kSecAsn1SequenceOfObjectIDTemplate

/*
 * Subject Key Identifier
 * NSS struct  : CSSM_DATA
 * CDSA struct : CE_SubjectKeyID, typedef'd to a CSSM_DATA
 */
#define kSecAsn1SubjectKeyIdTemplate	kSecAsn1OctetStringTemplate

/*
 * Authority Key Identifier
 * NSS struct  : NSS_AuthorityKeyId
 * CDSA struct : CE_AuthorityKeyID
 *
 * All fields are optional.
 * NOTE: due to an anomaly in the encoding module, if the first field
 * of a sequence is optional, it has to be a POINTER type. 
 */
typedef struct {
	CSSM_DATA			*keyIdentifier;		// octet string
	NSS_GeneralNames	genNames;	
	CSSM_DATA			serialNumber;		// integer
} NSS_AuthorityKeyId;

extern const SecAsn1Template kSecAsn1AuthorityKeyIdTemplate[];

/*
 * Certificate policies. 
 * NSS struct  : NSS_CertPolicies
 * CDSA struct : CE_CertPolicies
 */
typedef struct {
	CSSM_OID		policyQualifierId;	// CSSMOID_QT_CPS, CSSMOID_QT_UNOTICE
	CSSM_DATA		qualifier;			// ASN_ANY, not interpreted here
} NSS_PolicyQualifierInfo;

extern const SecAsn1Template kSecAsn1PolicyQualifierTemplate[];

typedef struct {
	CSSM_OID				certPolicyId;
	NSS_PolicyQualifierInfo	**policyQualifiers;	// SEQUENCE OF
} NSS_PolicyInformation;

extern const SecAsn1Template kSecAsn1PolicyInformationTemplate[];

typedef struct {
	NSS_PolicyInformation	**policies;			// SEQUENCE OF
} NSS_CertPolicies;

extern const SecAsn1Template kSecAsn1CertPoliciesTemplate[];

/* 
 * netscape-cert-type
 * NSS struct  : CSSM_DATA, BIT STRING - length in bits
 * CDSA struct : CE_NetscapeCertType (a uint16)
 */
#define kSecAsn1NetscapeCertTypeTemplate		kSecAsn1BitStringTemplate

/*
 * CRL Distribution Points. 
 * NSS struct  : NSS_DistributionPoint, NSS_DistributionPoints
 * CDSA struct : CE_CRLDistributionPoint, CE_CRLDistributionPointSyntax
 */

typedef struct {
	CSSM_DATA			*distPointName;		// ASN_ANY, optional
	CSSM_DATA			reasons;			// BIT_STRING, optional
	NSS_GeneralNames	crlIssuer;			// optional
} NSS_DistributionPoint;

typedef struct {
	NSS_DistributionPoint	**distPoints;	// SEQUENCE OF
} NSS_CRLDistributionPoints;

extern const SecAsn1Template kSecAsn1DistributionPointTemplate[];
extern const SecAsn1Template kSecAsn1CRLDistributionPointsTemplate[];

/*
 * Resolving the NSS_DistributionPoint.distributionPoint option
 * involves inspecting the tag of the ASN_ANY and using one of
 * these templates. One the CDSA side the corresponding struct is
 * a CE_DistributionPointName.
 *
 * This one resolves to an NSS_GeneralNames:
 */
#define NSS_DIST_POINT_FULL_NAME_TAG	0
extern const SecAsn1Template kSecAsn1DistPointFullNameTemplate[];
 
/*
 * This one resolves to an NSS_RDN.
 */
#define NSS_DIST_POINT_RDN_TAG			1
extern const SecAsn1Template kSecAsn1DistPointRDNTemplate[];

/*
 * Issuing distribution point.
 *
 * NSS Struct  : NSS_IssuingDistributionPoint
 * CDSA struct : CE_IssuingDistributionPoint
 *
 * All fields optional; default for ASN_BOOLs is false.
 */
typedef struct {
	/* manually decode to a CE_DistributionPointName */
	CSSM_DATA			*distPointName;		// ASN_ANY, optional
	
	CSSM_DATA			*onlyUserCerts;		// ASN_BOOL
	CSSM_DATA			*onlyCACerts;		// ASN_BOOL
	CSSM_DATA			*onlySomeReasons;	// BIT STRING
	CSSM_DATA			*indirectCRL;		// ASN_BOOL
} NSS_IssuingDistributionPoint;

extern const SecAsn1Template kSecAsn1IssuingDistributionPointTemplate[];

/* 
 * Authority Information Access, Subject Information Access.
 *
 * NSS Struct  : NSS_AuthorityInfoAccess
 * CDSA struct : CE_AuthorityInfoAccess
 */
typedef struct {
	CSSM_DATA				accessMethod;
	
	/* NSS encoder just can't handle direct inline of an NSS_GeneralName here.
	 * After decode and prior to encode this is an encoded GeneralName. 
	 */
	CSSM_DATA				encodedAccessLocation;
} NSS_AccessDescription;

typedef struct {
	NSS_AccessDescription	**accessDescriptions;
} NSS_AuthorityInfoAccess;

extern const SecAsn1Template kSecAsn1AccessDescriptionTemplate[];
extern const SecAsn1Template kSecAsn1AuthorityInfoAccessTemplate[];

/*
 * Qualified Certificate Statements support
 */
typedef struct {
	CSSM_OID				*semanticsIdentifier;			/* optional */
	NSS_GeneralNames		*nameRegistrationAuthorities;	/* optional */
} NSS_SemanticsInformation;

typedef struct {
	CSSM_OID				statementId;
	CSSM_DATA				info;		/* optional, ANY */
} NSS_QC_Statement;

typedef struct {
	NSS_QC_Statement		**qcStatements;
} NSS_QC_Statements; 

extern const SecAsn1Template kSecAsn1SemanticsInformationTemplate[];
extern const SecAsn1Template kSecAsn1QC_StatementTemplate[];
extern const SecAsn1Template kSecAsn1QC_StatementsTemplate[];

/*
 * NameConstraints support
 */
typedef struct {
	NSS_GeneralNames		base;
	CSSM_DATA				minimum;	// INTEGER default=0
	CSSM_DATA				maximum;	// INTEGER optional
} NSS_GeneralSubtree;

typedef struct {
	NSS_GeneralSubtree		**subtrees; // SEQUENCE OF
} NSS_GeneralSubtrees; 

typedef struct {
	NSS_GeneralSubtrees		*permittedSubtrees; // optional
	NSS_GeneralSubtrees		*excludedSubtrees;  // optional
} NSS_NameConstraints; 

extern const SecAsn1Template kSecAsn1NameConstraintsTemplate[];

/*
 * PolicyMappings support
 */
typedef struct {
	CSSM_OID				issuerDomainPolicy;
	CSSM_OID				subjectDomainPolicy;
} NSS_PolicyMapping;

typedef struct {
	NSS_PolicyMapping		**policyMappings; // SEQUENCE OF
} NSS_PolicyMappings; 

extern const SecAsn1Template kSecAsn1PolicyMappingsTemplate[];

/*
 * PolicyConstraints support
 */
typedef struct {
	CSSM_DATA				requireExplicitPolicy;	// INTEGER optional
	CSSM_DATA				inhibitPolicyMapping;	// INTEGER optional
} NSS_PolicyConstraints;

extern const SecAsn1Template kSecAsn1PolicyConstraintsTemplate[];

/*
 * InhibitAnyPolicy support
 */
#define kSecAsn1InhibitAnyPolicyTemplate	kSecAsn1IntegerTemplate;

#ifdef	__cplusplus
}
#endif

#endif	/* _CERT_EXTENSION_TEMPLATES_H_ */

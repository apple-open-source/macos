/*
 * Copyright (c) 2003 Apple Computer, Inc. All Rights Reserved.
 * 
 * The contents of this file constitute Original Code as defined in and are
 * subject to the Apple Public Source License Version 1.2 (the 'License').
 * You may not use this file except in compliance with the License. Please 
 * obtain a copy of the License at http://www.apple.com/publicsource and 
 * read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES, 
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, 
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and 
 * limitations under the License.
 */
/*
 * certExtensionTemplates.h - libnssasn1 structs and templates for cert and 
 *                            CRL extensions
 *
 * Created 2/6/03 by Doug Mitchell. 
 * Copyright (c) 2003 by Apple Computer. 
 */
 
#ifndef	_CERT_EXTENSION_TEMPLATES_H_
#define _CERT_EXTENSION_TEMPLATES_H_

#include <SecurityNssAsn1/X509Templates.h>

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

extern const SEC_ASN1Template NSS_BasicConstraintsTemplate[];

/* 
 * Key Usage
 * NSS struct  : CSSM_DATA, BIT STRING - length in bits
 * CDSA struct : CE_KeyUsage
 */
#define NSS_KeyUsageTemplate		SEC_BitStringTemplate

/*
 * Extended Key Usage
 * NSS struct  : NSS_ExtKeyUsage
 * CDSA struct : CE_ExtendedKeyUsage
 */
typedef struct {
	CSSM_OID	**purposes;
} NSS_ExtKeyUsage;
#define NSS_ExtKeyUsageTemplate		SEC_SequenceOfObjectIDTemplate

/*
 * Subject Key Identifier
 * NSS struct  : CSSM_DATA
 * CDSA struct : CE_SubjectKeyID, typedef'd to a CSSM_DATA
 */
#define NSS_SubjectKeyIdTemplate	SEC_OctetStringTemplate

/*
 * Authority Key Identifier
 * NSS struct  : NSS_AuthorityKeyId
 * CDSA struct : CE_AuthorityKeyID
 *
 * All fields are optional.
 * NOTE: sue to an anomaly in the encoding module, if the first field
 * of a sequence is optional, it has to be a POINTER type. 
 */
typedef struct {
	CSSM_DATA			*keyIdentifier;		// octet string
	NSS_GeneralNames	genNames;	
	CSSM_DATA			serialNumber;		// integer
} NSS_AuthorityKeyId;

extern const SEC_ASN1Template NSS_AuthorityKeyIdTemplate[];

/*
 * Certificate policies. 
 * NSS struct  : NSS_CertPolicies
 * CDSA struct : CE_CertPolicies
 */
typedef struct {
	CSSM_OID		policyQualifierId;	// CSSMOID_QT_CPS, CSSMOID_QT_UNOTICE
	CSSM_DATA		qualifier;			// ASN_ANY, not interpreted here
} NSS_PolicyQualifierInfo;

extern const SEC_ASN1Template NSS_PolicyQualifierTemplate[];

typedef struct {
	CSSM_OID				certPolicyId;
	NSS_PolicyQualifierInfo	**policyQualifiers;	// SEQUENCE OF
} NSS_PolicyInformation;

extern const SEC_ASN1Template NSS_PolicyInformationTemplate[];

typedef struct {
	NSS_PolicyInformation	**policies;			// SEQUENCE OF
} NSS_CertPolicies;

extern const SEC_ASN1Template NSS_CertPoliciesTemplate[];

/* 
 * netscape-cert-type
 * NSS struct  : CSSM_DATA, BIT STRING - length in bits
 * CDSA struct : CE_NetscapeCertType (a uint16)
 */
#define NSS_NetscapeCertTypeTemplate		SEC_BitStringTemplate

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

extern const SEC_ASN1Template NSS_DistributionPointTemplate[];
extern const SEC_ASN1Template NSS_CRLDistributionPointsTemplate[];

/*
 * Resolving the NSS_DistributionPoint.distributionPoint option
 * involves inspecting the tag of the ASN_ANY and using one of
 * these templates. One the CDSA side the corresponding struct is
 * a CE_DistributionPointName.
 *
 * This one resolves to an NSS_GeneralNames:
 */
#define NSS_DIST_POINT_FULL_NAME_TAG	0
extern const SEC_ASN1Template NSS_DistPointFullNameTemplate[];
 
/*
 * This one resolves to an NSS_RDN.
 */
#define NSS_DIST_POINT_RDN_TAG			1
extern const SEC_ASN1Template NSS_DistPointRDNTemplate[];

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

extern const SEC_ASN1Template NSS_IssuingDistributionPointTemplate[];

#ifdef	__cplusplus
}
#endif

#endif	/* _CERT_EXTENSION_TEMPLATES_H_ */

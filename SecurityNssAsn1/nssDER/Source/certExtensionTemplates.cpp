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
 * Copyright (c) 2000-2001 Apple Computer, Inc. All Rights Reserved.
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
 * certExtensionTemplates.cpp - libnssasn1 structs and templates for cert and 
 *                              CRL extensions
 *
 * Created 2/6/03 by Doug Mitchell. 
 * Copyright (c) 2003 by Apple Computer. 
 */
 
#include "certExtensionTemplates.h"

/* Basic Constraints */
const SEC_ASN1Template NSS_BasicConstraintsTemplate[] = {
    { SEC_ASN1_SEQUENCE,
	  0, NULL, sizeof(NSS_BasicConstraints) },
    { SEC_ASN1_BOOLEAN | SEC_ASN1_OPTIONAL, 
	  offsetof(NSS_BasicConstraints,cA) },
    { SEC_ASN1_INTEGER | SEC_ASN1_OPTIONAL,
	  offsetof(NSS_BasicConstraints, pathLenConstraint) },
    { 0, }
};

/* Authority Key Identifier */

/* signed integer - SEC_ASN1_SIGNED_INT state gets lost
 * in SEC_ASN1_CONTEXT_SPECIFIC processing */
const SEC_ASN1Template NSS_SignedIntegerTemplate[] = {
    { SEC_ASN1_INTEGER | SEC_ASN1_SIGNED_INT, 0, NULL, sizeof(SECItem) }
};

const SEC_ASN1Template NSS_AuthorityKeyIdTemplate[] = {
    { SEC_ASN1_SEQUENCE,
	  0, NULL, sizeof(NSS_AuthorityKeyId) },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_CONTEXT_SPECIFIC | 
	  SEC_ASN1_POINTER | 0, 
	  offsetof(NSS_AuthorityKeyId,keyIdentifier),
	  SEC_OctetStringTemplate },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_CONSTRUCTED | 
	                      SEC_ASN1_CONTEXT_SPECIFIC | 1, 
	  offsetof(NSS_AuthorityKeyId,genNames), 
	  NSS_GeneralNamesTemplate },
	/* serial number is SIGNED integer */
    { SEC_ASN1_OPTIONAL | SEC_ASN1_CONTEXT_SPECIFIC | 2,
	  offsetof(NSS_AuthorityKeyId,serialNumber), 
	  NSS_SignedIntegerTemplate},
	{ 0 }
};

/* Certificate policies */
const SEC_ASN1Template NSS_PolicyQualifierTemplate[] = {
    { SEC_ASN1_SEQUENCE,
	  0, NULL, sizeof(NSS_PolicyQualifierInfo) },
    { SEC_ASN1_OBJECT_ID,  
	  offsetof(NSS_PolicyQualifierInfo,policyQualifierId) },
	{ SEC_ASN1_ANY, offsetof(NSS_PolicyQualifierInfo, qualifier) },
	{ 0 }
};

const SEC_ASN1Template NSS_PolicyInformationTemplate[] = {
    { SEC_ASN1_SEQUENCE,
	  0, NULL, sizeof(NSS_PolicyInformation) },
    { SEC_ASN1_OBJECT_ID,  
	  offsetof(NSS_PolicyInformation,certPolicyId) },
	{ SEC_ASN1_SEQUENCE_OF | SEC_ASN1_OPTIONAL,
	  offsetof(NSS_PolicyInformation,policyQualifiers),
	  NSS_PolicyQualifierTemplate },
	{ 0 }
};

const SEC_ASN1Template NSS_CertPoliciesTemplate[] = {
	{ SEC_ASN1_SEQUENCE_OF,
	  offsetof(NSS_CertPolicies,policies),
	  NSS_PolicyInformationTemplate },
	{ 0 }
};

/* CRL Distribution Points */

/*
 * NOTE WELL: RFC2459, and all the documentation I can find, claims that
 * the tag for the DistributionPointName option (tag 0) of a 
 * DistributionPoint is IMPLICIT and context-specific. However this
 * is IMPOSSIBLE - since the underlying type (DistributionPointName)
 * also relies upon context-specific tags to resolve a CHOICE. 
 * The real world indicates that the tag for the DistributionPoint option
 * is indeed EXPLICIT. Examination of many certs' cRLDistributionPoints
 * extensions shows this, and the NSS reference code also specifies
 * an EXPLICIT tag for this field. 
 */
const SEC_ASN1Template NSS_DistributionPointTemplate[] = {
    { SEC_ASN1_SEQUENCE,
	  0, NULL, sizeof(NSS_DistributionPoint) },
	{ SEC_ASN1_OPTIONAL | SEC_ASN1_CONTEXT_SPECIFIC |
	  SEC_ASN1_CONSTRUCTED | SEC_ASN1_EXPLICIT | 0,
	  offsetof(NSS_DistributionPoint,distPointName), 
	  SEC_PointerToAnyTemplate },
	{ SEC_ASN1_OPTIONAL | SEC_ASN1_CONTEXT_SPECIFIC | 1,
	  offsetof(NSS_DistributionPoint,reasons), SEC_BitStringTemplate},
	{ SEC_ASN1_OPTIONAL | SEC_ASN1_CONTEXT_SPECIFIC |
						  SEC_ASN1_CONSTRUCTED | 2,
	  offsetof(NSS_DistributionPoint, crlIssuer), 
	  NSS_GeneralNamesTemplate
	},
	{ 0 }
};

const SEC_ASN1Template NSS_CRLDistributionPointsTemplate[] = {
	{ SEC_ASN1_SEQUENCE_OF,
	  offsetof(NSS_CRLDistributionPoints,distPoints),
	  NSS_DistributionPointTemplate },
	{ 0 }
};


/*
 * These are the context-specific targets of the DistributionPointName
 * option.
 */
const SEC_ASN1Template NSS_DistPointFullNameTemplate[] = {
    {SEC_ASN1_CONTEXT_SPECIFIC | SEC_ASN1_CONSTRUCTED | 0,
	offsetof (NSS_GeneralNames,names), NSS_GeneralNamesTemplate}
};

const SEC_ASN1Template NSS_DistPointRDNTemplate[] = {
    {SEC_ASN1_CONTEXT_SPECIFIC | SEC_ASN1_CONSTRUCTED | 1, 
	offsetof (NSS_RDN,atvs), NSS_RDNTemplate}
};
	 
/*
 * Issuing distribution points
 *
 * Although the spec says that the DistributionPointName element
 * is context-specific, it must be explicit because the underlying
 * type - a DistributionPointName - also relies on a context-specific
 * tags to resolve a CHOICE.
 */

/* kludge: ASN decoder doesn't handle 
 * SEC_ASN1_OPTIONAL | SEC_ASN1_CONTEXT_SPECIFIC | SEC_ASN1_POINTER
 * very well... */
static const SEC_ASN1Template NSS_OptBooleanTemplate[] = {
    { SEC_ASN1_BOOLEAN | SEC_ASN1_OPTIONAL, 0, NULL, sizeof(SECItem) }
};

static const SEC_ASN1Template NSS_OptBitStringTemplate[] = {
    { SEC_ASN1_BIT_STRING | SEC_ASN1_OPTIONAL, 0, NULL, sizeof(SECItem) }
};

const SEC_ASN1Template NSS_IssuingDistributionPointTemplate[] = {
    { SEC_ASN1_SEQUENCE,
	  0, NULL, sizeof(NSS_IssuingDistributionPoint) },
	{ SEC_ASN1_OPTIONAL | SEC_ASN1_CONTEXT_SPECIFIC | 
	  SEC_ASN1_CONSTRUCTED | SEC_ASN1_EXPLICIT | 0,
	  offsetof(NSS_IssuingDistributionPoint,distPointName), 
	  SEC_PointerToAnyTemplate },
	{ SEC_ASN1_OPTIONAL | SEC_ASN1_CONTEXT_SPECIFIC | SEC_ASN1_POINTER | 1,
	  offsetof(NSS_IssuingDistributionPoint,onlyUserCerts), 
	  NSS_OptBooleanTemplate},
	{ SEC_ASN1_OPTIONAL | SEC_ASN1_CONTEXT_SPECIFIC | SEC_ASN1_POINTER | 2,
	  offsetof(NSS_IssuingDistributionPoint,onlyCACerts), 
	  NSS_OptBooleanTemplate},
	{ SEC_ASN1_OPTIONAL | SEC_ASN1_CONTEXT_SPECIFIC | SEC_ASN1_POINTER | 3,
	  offsetof(NSS_IssuingDistributionPoint,onlySomeReasons), 
	  NSS_OptBitStringTemplate},
	{ SEC_ASN1_OPTIONAL | SEC_ASN1_CONTEXT_SPECIFIC | SEC_ASN1_POINTER | 4,
	  offsetof(NSS_IssuingDistributionPoint,indirectCRL), 
	  NSS_OptBooleanTemplate},
	{ 0 }
};

/*
 * Copyright (c) 2000-2004,2008-2013 Apple Inc. All Rights Reserved.
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

 File:      oidscert.cpp

 Contains:  Object Identifiers for X509 Certificate Library

 Copyright (c) 1999,2001-2004 Apple Computer, Inc. All Rights Reserved.

 */

#include "oidsbase.h"
#include "oidscert.h"

/* required until PR-3347430 Security/cdsa/cdsa/oidscert.h is checked
 * into TOT - pending public API review */
extern "C" {
	extern const CSSM_OID CSSMOID_X509V1IssuerNameStd,
		CSSMOID_X509V1SubjectNameStd;
}

static const uint8

	/* Certificate OID Fields */
	X509V3SignedCertificate[]					= {INTEL_X509V3_CERT_R08, 0},
	X509V3SignedCertificateCStruct[]			= {INTEL_X509V3_CERT_R08, 0, INTEL_X509_C_DATATYPE},
	X509V3Certificate[]							= {INTEL_X509V3_CERT_R08, 1},
	X509V3CertificateCStruct[]					= {INTEL_X509V3_CERT_R08, 1, INTEL_X509_C_DATATYPE},
	X509V1Version[]								= {INTEL_X509V3_CERT_R08, 2},
	X509V1SerialNumber[]						= {INTEL_X509V3_CERT_R08, 3},
	X509V1IssuerName[]							= {INTEL_X509V3_CERT_R08, 5},
	X509V1IssuerNameCStruct[]					= {INTEL_X509V3_CERT_R08, 5, INTEL_X509_C_DATATYPE},
	X509V1IssuerNameLDAP[]						= {INTEL_X509V3_CERT_R08, 5, INTEL_X509_LDAPSTRING_DATATYPE},
	X509V1ValidityNotBefore[]					= {INTEL_X509V3_CERT_R08, 6},
	X509V1ValidityNotAfter[]					= {INTEL_X509V3_CERT_R08, 7},
	X509V1SubjectName[]							= {INTEL_X509V3_CERT_R08, 8},
	X509V1SubjectNameCStruct[]					= {INTEL_X509V3_CERT_R08, 8, INTEL_X509_C_DATATYPE},
	X509V1SubjectNameLDAP[]						= {INTEL_X509V3_CERT_R08, 8, INTEL_X509_LDAPSTRING_DATATYPE},
	X509V1SubjectPublicKeyAlgorithm[]			= {INTEL_X509V3_CERT_R08, 9},
	X509V1SubjectPublicKey[]					= {INTEL_X509V3_CERT_R08, 10},
	X509V1CertificateIssuerUniqueId[]			= {INTEL_X509V3_CERT_R08, 11},
	X509V1CertificateSubjectUniqueId[]			= {INTEL_X509V3_CERT_R08, 12},
	X509V3CertificateExtensionStruct[]			= {INTEL_X509V3_CERT_R08, 13},
	X509V3CertificateExtensionCStruct[]			= {INTEL_X509V3_CERT_R08, 13, INTEL_X509_C_DATATYPE},
	X509V3CertificateNumberOfExtensions[]		= {INTEL_X509V3_CERT_R08, 14},
	X509V3CertificateExtensionId[]				= {INTEL_X509V3_CERT_R08, 15},
	X509V3CertificateExtensionCritical[]		= {INTEL_X509V3_CERT_R08, 16},
	X509V3CertificateExtensionValue[]			= {INTEL_X509V3_CERT_R08, 17},
	X509V1SubjectPublicKeyAlgorithmParameters[]	= {INTEL_X509V3_CERT_R08, 18},
	X509V3CertificateExtensionType[]			= {INTEL_X509V3_CERT_R08, 19},
	CSSMKeyStruct[]								= {INTEL_X509V3_CERT_R08, 20},
	X509V1SubjectPublicKeyCStruct[]				= {INTEL_X509V3_CERT_R08, 20, INTEL_X509_C_DATATYPE},
	X509V3CertificateExtensionsStruct[]			= {INTEL_X509V3_CERT_R08, 21},
	X509V3CertificateExtensionsCStruct[]		= {INTEL_X509V3_CERT_R08, 21, INTEL_X509_C_DATATYPE},
	X509V1SubjectNameStd[]						= {INTEL_X509V3_CERT_R08, 22},
	X509V1IssuerNameStd[]						= {INTEL_X509V3_CERT_R08, 23},

	/* Signature OID Fields */
	X509V1SignatureStruct[]						= {INTEL_X509V3_SIGN_R08, 0},
	X509V1SignatureCStruct[]					= {INTEL_X509V3_SIGN_R08, 0, INTEL_X509_C_DATATYPE},
	/* for the algorithm ID in the cert proper */
	X509V1SignatureAlgorithm[]					= {INTEL_X509V3_SIGN_R08, 1},
	/* for the one in TBSCert */
	X509V1SignatureAlgorithmTBS[]				= {INTEL_X509V3_SIGN_R08, 10},
	X509V1SignatureAlgorithmParameters[]		= {INTEL_X509V3_SIGN_R08, 3},
	X509V1Signature[]							= {INTEL_X509V3_SIGN_R08, 2},

	/* Extension OID Fields */
	SubjectSignatureBitmap[]					= {INTEL_X509V3_CERT_PRIVATE_EXTENSIONS, 1},
	SubjectPicture[]							= {INTEL_X509V3_CERT_PRIVATE_EXTENSIONS, 2},
	SubjectEmailAddress[]						= {INTEL_X509V3_CERT_PRIVATE_EXTENSIONS, 3},
	UseExemptions[]								= {INTEL_X509V3_CERT_PRIVATE_EXTENSIONS, 4};


const CSSM_OID

	/* Certificate OIDS */
	CSSMOID_X509V3SignedCertificate  			= {INTEL_X509V3_CERT_R08_LENGTH+1,  (uint8 *)X509V3SignedCertificate},
	CSSMOID_X509V3SignedCertificateCStruct  	= {INTEL_X509V3_CERT_R08_LENGTH+2,
													(uint8 *)X509V3SignedCertificateCStruct},
	CSSMOID_X509V3Certificate  					= {INTEL_X509V3_CERT_R08_LENGTH+1,  (uint8 *)X509V3Certificate},
	CSSMOID_X509V3CertificateCStruct  			= {INTEL_X509V3_CERT_R08_LENGTH+2,  (uint8 *)X509V3CertificateCStruct},
	CSSMOID_X509V1Version  						= {INTEL_X509V3_CERT_R08_LENGTH+1,  (uint8 *)X509V1Version},
	CSSMOID_X509V1SerialNumber  				= {INTEL_X509V3_CERT_R08_LENGTH+1,  (uint8 *)X509V1SerialNumber},
	CSSMOID_X509V1IssuerName  					= {INTEL_X509V3_CERT_R08_LENGTH+1,  (uint8 *)X509V1IssuerName},
	CSSMOID_X509V1IssuerNameStd  				= {INTEL_X509V3_CERT_R08_LENGTH+1,  (uint8 *)X509V1IssuerNameStd},
	CSSMOID_X509V1IssuerNameCStruct  			= {INTEL_X509V3_CERT_R08_LENGTH+2,  (uint8 *)X509V1IssuerNameCStruct},
	CSSMOID_X509V1IssuerNameLDAP  				= {INTEL_X509V3_CERT_R08_LENGTH+2,  (uint8 *)X509V1IssuerNameLDAP},
	CSSMOID_X509V1ValidityNotBefore  			= {INTEL_X509V3_CERT_R08_LENGTH+1,  (uint8 *)X509V1ValidityNotBefore},
	CSSMOID_X509V1ValidityNotAfter  			= {INTEL_X509V3_CERT_R08_LENGTH+1,  (uint8 *)X509V1ValidityNotAfter},
	CSSMOID_X509V1SubjectName  					= {INTEL_X509V3_CERT_R08_LENGTH+1,  (uint8 *)X509V1SubjectName},
	CSSMOID_X509V1SubjectNameStd  				= {INTEL_X509V3_CERT_R08_LENGTH+1,  (uint8 *)X509V1SubjectNameStd},
	CSSMOID_X509V1SubjectNameCStruct  			= {INTEL_X509V3_CERT_R08_LENGTH+2,  (uint8 *)X509V1SubjectNameCStruct},
	CSSMOID_X509V1SubjectNameLDAP  				= {INTEL_X509V3_CERT_R08_LENGTH+2,  (uint8 *)X509V1SubjectNameLDAP},
	CSSMOID_CSSMKeyStruct  						= {INTEL_X509V3_CERT_R08_LENGTH+1,  (uint8 *)CSSMKeyStruct},
	CSSMOID_X509V1SubjectPublicKeyCStruct  		= {INTEL_X509V3_CERT_R08_LENGTH+2,
													(uint8 *)X509V1SubjectPublicKeyCStruct},
	CSSMOID_X509V1SubjectPublicKeyAlgorithm  	= {INTEL_X509V3_CERT_R08_LENGTH+1,
													(uint8 *)X509V1SubjectPublicKeyAlgorithm},
	CSSMOID_X509V1SubjectPublicKeyAlgorithmParameters = {INTEL_X509V3_CERT_R08_LENGTH+1,
													(uint8 *)X509V1SubjectPublicKeyAlgorithmParameters},
	CSSMOID_X509V1SubjectPublicKey  			= {INTEL_X509V3_CERT_R08_LENGTH+1,  (uint8 *)X509V1SubjectPublicKey},
	CSSMOID_X509V1CertificateIssuerUniqueId  	= {INTEL_X509V3_CERT_R08_LENGTH+1,
													(uint8 *)X509V1CertificateIssuerUniqueId},
	CSSMOID_X509V1CertificateSubjectUniqueId  	= {INTEL_X509V3_CERT_R08_LENGTH+1,
													(uint8 *)X509V1CertificateSubjectUniqueId},
	CSSMOID_X509V3CertificateExtensionsStruct  	= {INTEL_X509V3_CERT_R08_LENGTH+1,
													(uint8 *)X509V3CertificateExtensionsStruct},
	CSSMOID_X509V3CertificateExtensionsCStruct  = {INTEL_X509V3_CERT_R08_LENGTH+2,
													(uint8 *)X509V3CertificateExtensionsCStruct},
	CSSMOID_X509V3CertificateNumberOfExtensions = {INTEL_X509V3_CERT_R08_LENGTH+1,
													(uint8 *)X509V3CertificateNumberOfExtensions},
	CSSMOID_X509V3CertificateExtensionStruct  	= {INTEL_X509V3_CERT_R08_LENGTH+1,
													(uint8 *)X509V3CertificateExtensionStruct},
	CSSMOID_X509V3CertificateExtensionCStruct  	= {INTEL_X509V3_CERT_R08_LENGTH+2,
													(uint8 *)X509V3CertificateExtensionCStruct},
	CSSMOID_X509V3CertificateExtensionId  		= {INTEL_X509V3_CERT_R08_LENGTH+1,
													(uint8 *)X509V3CertificateExtensionId},
	CSSMOID_X509V3CertificateExtensionCritical  = {INTEL_X509V3_CERT_R08_LENGTH+1,
													(uint8 *)X509V3CertificateExtensionCritical},
	CSSMOID_X509V3CertificateExtensionType  	= {INTEL_X509V3_CERT_R08_LENGTH+1,
													(uint8 *)X509V3CertificateExtensionType},
	CSSMOID_X509V3CertificateExtensionValue  	= {INTEL_X509V3_CERT_R08_LENGTH+1,
													(uint8 *)X509V3CertificateExtensionValue},

	/* Signature OID Fields */
	CSSMOID_X509V1SignatureStruct  				= {INTEL_X509V3_SIGN_R08_LENGTH+1,  (uint8 *)X509V1SignatureStruct},
	CSSMOID_X509V1SignatureCStruct  			= {INTEL_X509V3_SIGN_R08_LENGTH+2,  (uint8 *)X509V1SignatureCStruct},
	CSSMOID_X509V1SignatureAlgorithm  			= {INTEL_X509V3_SIGN_R08_LENGTH+1,  (uint8 *)X509V1SignatureAlgorithm},
	CSSMOID_X509V1SignatureAlgorithmTBS  		= {INTEL_X509V3_SIGN_R08_LENGTH+1,  (uint8 *)X509V1SignatureAlgorithmTBS},
	CSSMOID_X509V1SignatureAlgorithmParameters 	= {INTEL_X509V3_SIGN_R08_LENGTH+1,
													(uint8 *)X509V1SignatureAlgorithmParameters},
	CSSMOID_X509V1Signature  					= {INTEL_X509V3_SIGN_R08_LENGTH+1,  (uint8 *)X509V1Signature},

	/* Extension OID Fields */
	CSSMOID_SubjectSignatureBitmap  			= {INTEL_X509V3_CERT_PRIVATE_EXTENSIONS_LENGTH+1,  (uint8 *)SubjectSignatureBitmap},
	CSSMOID_SubjectPicture  					= {INTEL_X509V3_CERT_PRIVATE_EXTENSIONS_LENGTH+1,  (uint8 *)SubjectPicture},
	CSSMOID_SubjectEmailAddress 				= {INTEL_X509V3_CERT_PRIVATE_EXTENSIONS_LENGTH+1,  (uint8 *)SubjectEmailAddress},
	CSSMOID_UseExemptions  						= {INTEL_X509V3_CERT_PRIVATE_EXTENSIONS_LENGTH+1, (uint8 *)UseExemptions};


/***
 *** Apple addenda.
 ***/

/*
 * Standard Cert extensions.
 */
static const uint8
	OID_SubjectDirectoryAttributes[]	= { OID_EXTENSION, 9 },
	OID_SubjectKeyIdentifier[] 		 	= { OID_EXTENSION, 14 },
	OID_KeyUsage[]             		 	= { OID_EXTENSION, 15 },
	OID_PrivateKeyUsagePeriod[] 	 	= { OID_EXTENSION, 16 },
	OID_SubjectAltName[]       			= { OID_EXTENSION, 17 },
	OID_IssuerAltName[]         		= { OID_EXTENSION, 18 },
	OID_BasicConstraints[]      		= { OID_EXTENSION, 19 },
	OID_CrlNumber[]             		= { OID_EXTENSION, 20 },
	OID_CrlReason[]             		= { OID_EXTENSION, 21 },
	OID_HoldInstructionCode[]   		= { OID_EXTENSION, 23 },
	OID_InvalidityDate[]        		= { OID_EXTENSION, 24 },
	OID_DeltaCrlIndicator[]     		= { OID_EXTENSION, 27 },
	OID_IssuingDistributionPoint[]      = { OID_EXTENSION, 28 },
	OID_CertIssuer[] 				    = { OID_EXTENSION, 29 },
	OID_NameConstraints[]       		= { OID_EXTENSION, 30 },
	OID_CrlDistributionPoints[] 		= { OID_EXTENSION, 31 },
	OID_CertificatePolicies[]   		= { OID_EXTENSION, 32 },
	OID_PolicyMappings[]        		= { OID_EXTENSION, 33 },
	OID_AuthorityKeyIdentifier[]		= { OID_EXTENSION, 35 },
	OID_PolicyConstraints[]     		= { OID_EXTENSION, 36 },
	OID_ExtendedKeyUsage[] 				= { OID_EXTENSION, 37 },
	OID_InhibitAnyPolicy[] 				= { OID_EXTENSION, 54 },
	OID_AuthorityInfoAccess[]			= { OID_PE, 1 },
	OID_BiometricInfo[]					= { OID_PE, 2 },
	OID_QC_Statements[]					= { OID_PE, 3 },
	OID_SubjectInfoAccess[]				= { OID_PE, 11 },

	/* Individual OIDS appearing in an ExtendedKeyUsage extension */
	OID_ExtendedKeyUsageAny[] 			= { OID_EXTENSION, 37, 0 },
	OID_KP_ServerAuth[]					= { OID_KP, 1 },
	OID_KP_ClientAuth[]					= { OID_KP, 2 },
	OID_KP_ExtendedUseCodeSigning[]		= { OID_KP, 3 },
	OID_KP_EmailProtection[]			= { OID_KP, 4 },
	OID_KP_TimeStamping[]				= { OID_KP, 8 },
	OID_KP_OCSPSigning[]				= { OID_KP, 9 },
	/* Kerberos PKINIT Extended Key Use values */
	OID_KERBv5_PKINIT_KP_CLIENT_AUTH[]	= { OID_KERBv5_PKINIT, 4 },
	OID_KERBv5_PKINIT_KP_KDC[]			= { OID_KERBv5_PKINIT, 5 },
	/* IPSec */
	OID_EKU_IPSec[]						= { 0x2B, 0x06, 0x01, 0x05, 0x05, 0x08, 0x02, 0x02 },

	/* .mac Certificate Extended Key Use values */
	OID_DOTMAC_CERT_EXTENSION[]		= { APPLE_DOTMAC_CERT_EXTEN_OID },
	OID_DOTMAC_CERT_IDENTITY[]		= { APPLE_DOTMAC_CERT_EXTEN_OID, 1 },
	OID_DOTMAC_CERT_EMAIL_SIGN[]	= { APPLE_DOTMAC_CERT_EXTEN_OID, 2 },
	OID_DOTMAC_CERT_EMAIL_ENCRYPT[]	= { APPLE_DOTMAC_CERT_EXTEN_OID, 3 },
	/* Other Apple extended key usage values */
	OID_APPLE_EKU_CODE_SIGNING[]		= { APPLE_EKU_CODE_SIGNING },
	OID_APPLE_EKU_CODE_SIGNING_DEV[]	= { APPLE_EKU_CODE_SIGNING, 1 },
	OID_APPLE_EKU_RESOURCE_SIGNING[]	= { APPLE_EKU_CODE_SIGNING, 4 },
	OID_APPLE_EKU_ICHAT_SIGNING[]		= { APPLE_EKU_OID, 2 },
	OID_APPLE_EKU_ICHAT_ENCRYPTION[]	= { APPLE_EKU_OID, 3 },
	OID_APPLE_EKU_SYSTEM_IDENTITY[]		= { APPLE_EKU_OID, 4 },
	OID_APPLE_EKU_PASSBOOK_SIGNING[]	= { APPLE_EKU_OID, 14 },
	OID_APPLE_EKU_PROFILE_SIGNING[]		= { APPLE_EKU_OID, 16 },
	OID_APPLE_EKU_QA_PROFILE_SIGNING[]	= { APPLE_EKU_OID, 17 },
	/* Apple cert policies */
	OID_APPLE_CERT_POLICY[]				= { APPLE_CERT_POLICIES, 1 },
	OID_DOTMAC_CERT_POLICY[]			= { APPLE_CERT_POLICIES, 2 },
	OID_ADC_CERT_POLICY[]				= { APPLE_CERT_POLICIES, 3 },
	OID_APPLE_CERT_POLICY_MACAPPSTORE[] = { APPLE_CERT_POLICIES_MACAPPSTORE },
	OID_APPLE_CERT_POLICY_MACAPPSTORE_RECEIPT[] = { APPLE_CERT_POLICIES_MACAPPSTORE_RECEIPT },
	OID_APPLE_CERT_POLICY_APPLEID[] = { APPLE_CERT_POLICIES_APPLEID },
	OID_APPLE_CERT_POLICY_APPLEID_SHARING[] = { APPLE_CERT_POLICIES_APPLEID_SHARING },
	OID_APPLE_CERT_POLICY_MOBILE_STORE_SIGNING[] = { APPLE_CERT_POLICIES_MOBILE_STORE_SIGNING },
	OID_APPLE_CERT_POLICY_TEST_MOBILE_STORE_SIGNING[] = { APPLE_CERT_POLICIES_TEST_MOBILE_STORE_SIGNING },

    /* Apple-specific extensions */
    OID_APPLE_EXTENSION[]				= { APPLE_EXTENSION_OID },
    OID_APPLE_EXTENSION_CODE_SIGNING[]		= { APPLE_EXTENSION_CODE_SIGNING },
    OID_APPLE_EXTENSION_APPLE_SIGNING[]		= { APPLE_EXTENSION_CODE_SIGNING, 1 },
    OID_APPLE_EXTENSION_ADC_DEV_SIGNING[]	= { APPLE_EXTENSION_CODE_SIGNING, 2 },
    OID_APPLE_EXTENSION_ADC_APPLE_SIGNING[]	= { APPLE_EXTENSION_CODE_SIGNING, 3 },
    OID_APPLE_EXTENSION_PASSBOOK_SIGNING[]	= { APPLE_EXTENSION_CODE_SIGNING, 16 },
	OID_APPLE_EXTENSION_MACAPPSTORE_RECEIPT[] = { APPLE_EXTENSION_MACAPPSTORE_RECEIPT },
	OID_APPLE_EXTENSION_INTERMEDIATE_MARKER[] = { APPLE_EXTENSION_INTERMEDIATE_MARKER },
	OID_APPLE_EXTENSION_WWDR_INTERMEDIATE[] = { APPLE_EXTENSION_WWDR_INTERMEDIATE },
	OID_APPLE_EXTENSION_ITMS_INTERMEDIATE[] = { APPLE_EXTENSION_ITMS_INTERMEDIATE },
	OID_APPLE_EXTENSION_AAI_INTERMEDIATE[] = { APPLE_EXTENSION_AAI_INTERMEDIATE },
	OID_APPLE_EXTENSION_APPLEID_INTERMEDIATE[] = { APPLE_EXTENSION_APPLEID_INTERMEDIATE },
	OID_APPLE_EXTENSION_APPLEID_SHARING[]   = { APPLE_EXTENSION_APPLEID_SHARING },
	OID_APPLE_EXTENSION_SYSINT2_INTERMEDIATE[] = { APPLE_EXTENSION_SYSINT2_INTERMEDIATE },
	OID_APPLE_EXTENSION_ESCROW_SERVICE[] = { APPLE_EXTENSION_ESCROW_SERVICE }
;

#define OID_PKCS_CE_LENGTH	OID_EXTENSION_LENGTH + 1

const CSSM_OID
CSSMOID_SubjectDirectoryAttributes = { OID_PKCS_CE_LENGTH, (uint8 *)OID_SubjectDirectoryAttributes},
CSSMOID_SubjectKeyIdentifier 	= { OID_PKCS_CE_LENGTH, (uint8 *)OID_SubjectKeyIdentifier},
CSSMOID_KeyUsage  				= { OID_PKCS_CE_LENGTH, (uint8 *)OID_KeyUsage},
CSSMOID_PrivateKeyUsagePeriod  	= { OID_PKCS_CE_LENGTH, (uint8 *)OID_PrivateKeyUsagePeriod},
CSSMOID_SubjectAltName  		= { OID_PKCS_CE_LENGTH, (uint8 *)OID_SubjectAltName},
CSSMOID_IssuerAltName  			= { OID_PKCS_CE_LENGTH, (uint8 *)OID_IssuerAltName},
CSSMOID_BasicConstraints  		= { OID_PKCS_CE_LENGTH, (uint8 *)OID_BasicConstraints},
CSSMOID_CrlNumber  				= { OID_PKCS_CE_LENGTH, (uint8 *)OID_CrlNumber},
CSSMOID_CrlReason  				= { OID_PKCS_CE_LENGTH, (uint8 *)OID_CrlReason},
CSSMOID_HoldInstructionCode  	= { OID_PKCS_CE_LENGTH, (uint8 *)OID_HoldInstructionCode},
CSSMOID_InvalidityDate  		= { OID_PKCS_CE_LENGTH, (uint8 *)OID_InvalidityDate},
CSSMOID_DeltaCrlIndicator  		= { OID_PKCS_CE_LENGTH, (uint8 *)OID_DeltaCrlIndicator},
CSSMOID_IssuingDistributionPoint = { OID_PKCS_CE_LENGTH, (uint8 *)OID_IssuingDistributionPoint},
/* for backwards compatibility... */
CSSMOID_IssuingDistributionPoints = { OID_PKCS_CE_LENGTH, (uint8 *)OID_IssuingDistributionPoint},
CSSMOID_CertIssuer				= { OID_PKCS_CE_LENGTH, (uint8 *)OID_CertIssuer},
CSSMOID_NameConstraints  		= { OID_PKCS_CE_LENGTH, (uint8 *)OID_NameConstraints},
CSSMOID_CrlDistributionPoints  	= { OID_PKCS_CE_LENGTH, (uint8 *)OID_CrlDistributionPoints},
CSSMOID_CertificatePolicies  	= { OID_PKCS_CE_LENGTH, (uint8 *)OID_CertificatePolicies},
CSSMOID_PolicyMappings  		= { OID_PKCS_CE_LENGTH, (uint8 *)OID_PolicyMappings},
CSSMOID_PolicyConstraints  		= { OID_PKCS_CE_LENGTH, (uint8 *)OID_PolicyConstraints},
CSSMOID_AuthorityKeyIdentifier  = { OID_PKCS_CE_LENGTH, (uint8 *)OID_AuthorityKeyIdentifier},
CSSMOID_ExtendedKeyUsage  		= { OID_PKCS_CE_LENGTH, (uint8 *)OID_ExtendedKeyUsage},
CSSMOID_InhibitAnyPolicy  		= { OID_PKCS_CE_LENGTH, (uint8 *)OID_InhibitAnyPolicy},
CSSMOID_AuthorityInfoAccess		= { OID_PE_LENGTH+1, (uint8 *)OID_AuthorityInfoAccess},
CSSMOID_BiometricInfo			= { OID_PE_LENGTH+1, (uint8 *)OID_BiometricInfo},
CSSMOID_QC_Statements			= { OID_PE_LENGTH+1, (uint8 *)OID_QC_Statements},
CSSMOID_SubjectInfoAccess		= { OID_PE_LENGTH+1, (uint8 *)OID_SubjectInfoAccess},
CSSMOID_ExtendedKeyUsageAny		= { OID_PKCS_CE_LENGTH+1, (uint8 *)OID_ExtendedKeyUsageAny},
CSSMOID_ServerAuth				= { OID_KP_LENGTH+1, (uint8 *)OID_KP_ServerAuth},
CSSMOID_ClientAuth				= { OID_KP_LENGTH+1, (uint8 *)OID_KP_ClientAuth},
CSSMOID_ExtendedUseCodeSigning	= { OID_KP_LENGTH+1, (uint8 *)OID_KP_ExtendedUseCodeSigning},
CSSMOID_EmailProtection			= { OID_KP_LENGTH+1, (uint8 *)OID_KP_EmailProtection},
CSSMOID_TimeStamping			= { OID_KP_LENGTH+1, (uint8 *)OID_KP_TimeStamping},
CSSMOID_OCSPSigning				= { OID_KP_LENGTH+1, (uint8 *)OID_KP_OCSPSigning},
CSSMOID_KERBv5_PKINIT_KP_CLIENT_AUTH = { OID_KERBv5_PKINIT_LEN + 1,
										(uint8 *)OID_KERBv5_PKINIT_KP_CLIENT_AUTH },
CSSMOID_KERBv5_PKINIT_KP_KDC		= { OID_KERBv5_PKINIT_LEN + 1,
										(uint8 *)OID_KERBv5_PKINIT_KP_KDC },
CSSMOID_EKU_IPSec					= { 8, (uint8 *)OID_EKU_IPSec },
CSSMOID_DOTMAC_CERT_EXTENSION		= { APPLE_DOTMAC_CERT_EXTEN_OID_LENGTH,
										(uint8 *)OID_DOTMAC_CERT_EXTENSION },
CSSMOID_DOTMAC_CERT_IDENTITY		= { APPLE_DOTMAC_CERT_EXTEN_OID_LENGTH + 1,
										(uint8 *)OID_DOTMAC_CERT_IDENTITY },
CSSMOID_DOTMAC_CERT_EMAIL_SIGN		= { APPLE_DOTMAC_CERT_EXTEN_OID_LENGTH + 1,
										(uint8 *)OID_DOTMAC_CERT_EMAIL_SIGN },
CSSMOID_DOTMAC_CERT_EMAIL_ENCRYPT	= { APPLE_DOTMAC_CERT_EXTEN_OID_LENGTH + 1,
										(uint8 *)OID_DOTMAC_CERT_EMAIL_ENCRYPT },
CSSMOID_APPLE_CERT_POLICY			= { APPLE_CERT_POLICIES_LENGTH + 1,
										(uint8 *)OID_APPLE_CERT_POLICY },
CSSMOID_DOTMAC_CERT_POLICY			= { APPLE_CERT_POLICIES_LENGTH + 1,
										(uint8 *)OID_DOTMAC_CERT_POLICY },
CSSMOID_ADC_CERT_POLICY				= { APPLE_CERT_POLICIES_LENGTH + 1,
										(uint8 *)OID_ADC_CERT_POLICY },
CSSMOID_MACAPPSTORE_CERT_POLICY		= { APPLE_CERT_POLICIES_MACAPPSTORE_LENGTH,
										(uint8 *)OID_APPLE_CERT_POLICY_MACAPPSTORE },
CSSMOID_MACAPPSTORE_RECEIPT_CERT_POLICY	= { APPLE_CERT_POLICIES_MACAPPSTORE_RECEIPT_LENGTH,
										(uint8 *)OID_APPLE_CERT_POLICY_MACAPPSTORE_RECEIPT },
CSSMOID_APPLEID_CERT_POLICY			= { APPLE_CERT_POLICIES_APPLEID_LENGTH,
										(uint8 *)OID_APPLE_CERT_POLICY_APPLEID },
CSSMOID_APPLEID_SHARING_CERT_POLICY	= { APPLE_CERT_POLICIES_APPLEID_SHARING_LENGTH,
										(uint8 *)OID_APPLE_CERT_POLICY_APPLEID_SHARING },
CSSMOID_MOBILE_STORE_SIGNING_POLICY = { APPLE_CERT_POLICIES_MOBILE_STORE_SIGNING_LENGTH,
										(uint8 *)OID_APPLE_CERT_POLICY_MOBILE_STORE_SIGNING },
CSSMOID_TEST_MOBILE_STORE_SIGNING_POLICY	= { APPLE_CERT_POLICIES_TEST_MOBILE_STORE_SIGNING_LENGTH,
										(uint8 *)OID_APPLE_CERT_POLICY_TEST_MOBILE_STORE_SIGNING },
CSSMOID_APPLE_EKU_CODE_SIGNING		= { APPLE_EKU_CODE_SIGNING_LENGTH,
										(uint8 *)OID_APPLE_EKU_CODE_SIGNING },
CSSMOID_APPLE_EKU_CODE_SIGNING_DEV	= { APPLE_EKU_CODE_SIGNING_LENGTH + 1,
										(uint8 *)OID_APPLE_EKU_CODE_SIGNING_DEV },
CSSMOID_APPLE_EKU_RESOURCE_SIGNING	= { APPLE_EKU_CODE_SIGNING_LENGTH + 1,
										(uint8 *)OID_APPLE_EKU_RESOURCE_SIGNING },
CSSMOID_APPLE_EKU_ICHAT_SIGNING		= { APPLE_EKU_OID_LENGTH + 1,
										(uint8 *)OID_APPLE_EKU_ICHAT_SIGNING },
CSSMOID_APPLE_EKU_ICHAT_ENCRYPTION	= { APPLE_EKU_OID_LENGTH + 1,
										(uint8 *)OID_APPLE_EKU_ICHAT_ENCRYPTION },
CSSMOID_APPLE_EKU_SYSTEM_IDENTITY	= { APPLE_EKU_OID_LENGTH + 1,
										(uint8 *)OID_APPLE_EKU_SYSTEM_IDENTITY },
CSSMOID_APPLE_EKU_PASSBOOK_SIGNING	= { APPLE_EKU_OID_LENGTH + 1,
										(uint8 *)OID_APPLE_EKU_PASSBOOK_SIGNING },
CSSMOID_APPLE_EKU_PROFILE_SIGNING	= { APPLE_EKU_OID_LENGTH + 1,
										(uint8 *)OID_APPLE_EKU_PROFILE_SIGNING },
CSSMOID_APPLE_EKU_QA_PROFILE_SIGNING	= { APPLE_EKU_OID_LENGTH + 1,
										(uint8 *)OID_APPLE_EKU_QA_PROFILE_SIGNING },
CSSMOID_APPLE_EXTENSION				= { APPLE_EXTENSION_OID_LENGTH,
										(uint8 *)OID_APPLE_EXTENSION },
CSSMOID_APPLE_EXTENSION_CODE_SIGNING		= { APPLE_EXTENSION_CODE_SIGNING_LENGTH,
												(uint8 *)OID_APPLE_EXTENSION_CODE_SIGNING },
CSSMOID_APPLE_EXTENSION_APPLE_SIGNING		= { APPLE_EXTENSION_CODE_SIGNING_LENGTH + 1,
												(uint8 *)OID_APPLE_EXTENSION_APPLE_SIGNING },
CSSMOID_APPLE_EXTENSION_ADC_DEV_SIGNING		= { APPLE_EXTENSION_CODE_SIGNING_LENGTH + 2,
												(uint8 *)OID_APPLE_EXTENSION_ADC_DEV_SIGNING },
CSSMOID_APPLE_EXTENSION_ADC_APPLE_SIGNING	= { APPLE_EXTENSION_CODE_SIGNING_LENGTH + 3,
												(uint8 *)OID_APPLE_EXTENSION_ADC_DEV_SIGNING },
CSSMOID_APPLE_EXTENSION_PASSBOOK_SIGNING	= { APPLE_EXTENSION_CODE_SIGNING_LENGTH + 1,
												(uint8 *)OID_APPLE_EXTENSION_PASSBOOK_SIGNING },
CSSMOID_APPLE_EXTENSION_MACAPPSTORE_RECEIPT    = { APPLE_EXTENSION_MACAPPSTORE_RECEIPT_LENGTH,
												(uint8 *)OID_APPLE_EXTENSION_MACAPPSTORE_RECEIPT },
CSSMOID_APPLE_EXTENSION_INTERMEDIATE_MARKER   = { APPLE_EXTENSION_INTERMEDIATE_MARKER_LENGTH,
												(uint8 *)OID_APPLE_EXTENSION_INTERMEDIATE_MARKER },
CSSMOID_APPLE_EXTENSION_WWDR_INTERMEDIATE     = { APPLE_EXTENSION_WWDR_INTERMEDIATE_LENGTH,
												(uint8 *)OID_APPLE_EXTENSION_WWDR_INTERMEDIATE },
CSSMOID_APPLE_EXTENSION_ITMS_INTERMEDIATE     = { APPLE_EXTENSION_ITMS_INTERMEDIATE_LENGTH,
												(uint8 *)OID_APPLE_EXTENSION_ITMS_INTERMEDIATE },
CSSMOID_APPLE_EXTENSION_AAI_INTERMEDIATE      = { APPLE_EXTENSION_AAI_INTERMEDIATE_LENGTH,
												(uint8 *)OID_APPLE_EXTENSION_AAI_INTERMEDIATE },
CSSMOID_APPLE_EXTENSION_APPLEID_INTERMEDIATE    = { APPLE_EXTENSION_APPLEID_INTERMEDIATE_LENGTH,
												(uint8 *)OID_APPLE_EXTENSION_APPLEID_INTERMEDIATE },
CSSMOID_APPLE_EXTENSION_APPLEID_SHARING         = { APPLE_EXTENSION_APPLEID_SHARING_LENGTH + 1,
												(uint8 *)OID_APPLE_EXTENSION_APPLEID_SHARING },
CSSMOID_APPLE_EXTENSION_SYSINT2_INTERMEDIATE    = { APPLE_EXTENSION_SYSINT2_INTERMEDIATE_LENGTH,
												(uint8 *)OID_APPLE_EXTENSION_SYSINT2_INTERMEDIATE },
CSSMOID_APPLE_EXTENSION_ESCROW_SERVICE          = { APPLE_EXTENSION_ESCROW_SERVICE_LENGTH + 1,
												(uint8 *)OID_APPLE_EXTENSION_ESCROW_SERVICE }
;

/* Apple Intermediate Marker OIDs */
#define APPLE_CERT_EXT_INTERMEDIATE_MARKER APPLE_CERT_EXT, 2
/* Apple Apple ID Intermediate Marker */
#define APPLE_CERT_EXT_INTERMEDIATE_MARKER_APPLEID APPLE_CERT_EXT_INTERMEDIATE_MARKER, 3
/*
 *  Apple Apple ID Intermediate Marker (New subCA, no longer shared with push notification server cert issuer
 *
 *  appleCertificateExtensionAppleIDIntermediate ::=
 *    { appleCertificateExtensionIntermediateMarker 7 }
 *    { 1 2 840 113635 100 6 2 7 }
 */
#define APPLE_CERT_EXT_INTERMEDIATE_MARKER_APPLEID_2 APPLE_CERT_EXT_INTERMEDIATE_MARKER, 7

/*
 * Netscape extensions.
 *
 *  netscape-cert-type OBJECT IDENTIFIER ::=
 * 		{ 2 16 840 1 113730 1 1 }
 *
 *	BER = 06 08 60 86 48 01 86 F8 42 01 01
 */
static const uint8 	OID_NetscapeCertType[] 		= {NETSCAPE_CERT_EXTEN, 1};
const CSSM_OID	CSSMOID_NetscapeCertType 	=
	{NETSCAPE_CERT_EXTEN_LENGTH + 1, (uint8 *)OID_NetscapeCertType};

/*
 * netscape-cert-sequence ::= { 2 16 840 1 113730 2 5 }
 *
 * BER = 06 09 60 86 48 01 86 F8 42 02 05
 */
static const uint8  OID_NetscapeCertSequence[]  =  { NETSCAPE_BASE_OID, 2, 5 };
const CSSM_OID CSSMOID_NetscapeCertSequence		=
	{ NETSCAPE_BASE_OID_LEN + 2, (uint8 *)OID_NetscapeCertSequence };

/*
 * Netscape version of ServerGatedCrypto ExtendedKeyUse.
 * OID { 2 16 840 1 113730 4 1 }
 */
static const uint8 OID_Netscape_SGC[] = {NETSCAPE_CERT_POLICY, 1};
const CSSM_OID CSSMOID_NetscapeSGC 	=
	{NETSCAPE_CERT_POLICY_LENGTH + 1, (uint8 *)OID_Netscape_SGC};

/*
 * Microsoft version of ServerGatedCrypto ExtendedKeyUse.
 * OID { 1 3 6 1 4 1 311 10 3 3 }
 */
static const uint8 OID_Microsoft_SGC[] = {0x2B, 0x06, 0x01, 0x04, 0x01, 0x82, 0x37, 0x0A, 0x03, 0x03};
const CSSM_OID CSSMOID_MicrosoftSGC 	=
	{10, (uint8 *)OID_Microsoft_SGC};

/*
 * .mac Certificate Extended Key Use values.
 */

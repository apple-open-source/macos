/*
 * Copyright (c) 2000-2001 Apple Computer, Inc. All Rights Reserved.
 * 
 * The contents of this file constitute Original Code as defined in and are
 * subject to the Apple Public Source License Version 1.2 (the 'License').
 * You may not use this file except in compliance with the License. Please obtain
 * a copy of the License at http://www.apple.com/publicsource and read it before
 * using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESS
 * OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES, INCLUDING WITHOUT
 * LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. Please see the License for the
 * specific language governing rights and limitations under the License.
 */


/*

 File:      oidscert.cpp

 Contains:  Object Identifiers for X509 Certificate Library

 Copyright: (c) 1999 Apple Computer, Inc., all rights reserved.

 */

#include <Security/oidscert.h>
 
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
	CSSMOID_X509V3SignedCertificate  						= {INTEL_X509V3_CERT_R08_LENGTH+1,  (uint8 *)X509V3SignedCertificate},
	CSSMOID_X509V3SignedCertificateCStruct  				= {INTEL_X509V3_CERT_R08_LENGTH+2,  (uint8 *)X509V3SignedCertificateCStruct},
	CSSMOID_X509V3Certificate  							= {INTEL_X509V3_CERT_R08_LENGTH+1,  (uint8 *)X509V3Certificate},
	CSSMOID_X509V3CertificateCStruct  						= {INTEL_X509V3_CERT_R08_LENGTH+2,  (uint8 *)X509V3CertificateCStruct},
	CSSMOID_X509V1Version  								= {INTEL_X509V3_CERT_R08_LENGTH+1,  (uint8 *)X509V1Version},
	CSSMOID_X509V1SerialNumber  							= {INTEL_X509V3_CERT_R08_LENGTH+1,  (uint8 *)X509V1SerialNumber},
	CSSMOID_X509V1IssuerName  							= {INTEL_X509V3_CERT_R08_LENGTH+1,  (uint8 *)X509V1IssuerName},
	CSSMOID_X509V1IssuerNameCStruct  					= {INTEL_X509V3_CERT_R08_LENGTH+2,  (uint8 *)X509V1IssuerNameCStruct},
	CSSMOID_X509V1IssuerNameLDAP  						= {INTEL_X509V3_CERT_R08_LENGTH+2,  (uint8 *)X509V1IssuerNameLDAP},
	CSSMOID_X509V1ValidityNotBefore  						= {INTEL_X509V3_CERT_R08_LENGTH+1,  (uint8 *)X509V1ValidityNotBefore},
	CSSMOID_X509V1ValidityNotAfter  						= {INTEL_X509V3_CERT_R08_LENGTH+1,  (uint8 *)X509V1ValidityNotAfter},
	CSSMOID_X509V1SubjectName  							= {INTEL_X509V3_CERT_R08_LENGTH+1,  (uint8 *)X509V1SubjectName},
	CSSMOID_X509V1SubjectNameCStruct  					= {INTEL_X509V3_CERT_R08_LENGTH+2,  (uint8 *)X509V1SubjectNameCStruct},
	CSSMOID_X509V1SubjectNameLDAP  						= {INTEL_X509V3_CERT_R08_LENGTH+2,  (uint8 *)X509V1SubjectNameLDAP},
	CSSMOID_CSSMKeyStruct  								= {INTEL_X509V3_CERT_R08_LENGTH+1,  (uint8 *)CSSMKeyStruct},
	CSSMOID_X509V1SubjectPublicKeyCStruct  				= {INTEL_X509V3_CERT_R08_LENGTH+2,  (uint8 *)X509V1SubjectPublicKeyCStruct},
	CSSMOID_X509V1SubjectPublicKeyAlgorithm  				= {INTEL_X509V3_CERT_R08_LENGTH+1,  (uint8 *)X509V1SubjectPublicKeyAlgorithm},
	CSSMOID_X509V1SubjectPublicKeyAlgorithmParameters  	= {INTEL_X509V3_CERT_R08_LENGTH+1,  (uint8 *)X509V1SubjectPublicKeyAlgorithmParameters},
	CSSMOID_X509V1SubjectPublicKey  						= {INTEL_X509V3_CERT_R08_LENGTH+1,  (uint8 *)X509V1SubjectPublicKey},
	CSSMOID_X509V1CertificateIssuerUniqueId  				= {INTEL_X509V3_CERT_R08_LENGTH+1,  (uint8 *)X509V1CertificateIssuerUniqueId},
	CSSMOID_X509V1CertificateSubjectUniqueId  				= {INTEL_X509V3_CERT_R08_LENGTH+1,  (uint8 *)X509V1CertificateSubjectUniqueId},
	CSSMOID_X509V3CertificateExtensionsStruct  				= {INTEL_X509V3_CERT_R08_LENGTH+1,  (uint8 *)X509V3CertificateExtensionsStruct},
	CSSMOID_X509V3CertificateExtensionsCStruct  				= {INTEL_X509V3_CERT_R08_LENGTH+2,  (uint8 *)X509V3CertificateExtensionsCStruct},
	CSSMOID_X509V3CertificateNumberOfExtensions  			= {INTEL_X509V3_CERT_R08_LENGTH+1,  (uint8 *)X509V3CertificateNumberOfExtensions},
	CSSMOID_X509V3CertificateExtensionStruct  				= {INTEL_X509V3_CERT_R08_LENGTH+1,  (uint8 *)X509V3CertificateExtensionStruct},
	CSSMOID_X509V3CertificateExtensionCStruct  				= {INTEL_X509V3_CERT_R08_LENGTH+2,  (uint8 *)X509V3CertificateExtensionCStruct},
	CSSMOID_X509V3CertificateExtensionId  					= {INTEL_X509V3_CERT_R08_LENGTH+1,  (uint8 *)X509V3CertificateExtensionId},
	CSSMOID_X509V3CertificateExtensionCritical  				= {INTEL_X509V3_CERT_R08_LENGTH+1,  (uint8 *)X509V3CertificateExtensionCritical},
	CSSMOID_X509V3CertificateExtensionType  				= {INTEL_X509V3_CERT_R08_LENGTH+1,  (uint8 *)X509V3CertificateExtensionType},
	CSSMOID_X509V3CertificateExtensionValue  				= {INTEL_X509V3_CERT_R08_LENGTH+1,  (uint8 *)X509V3CertificateExtensionValue},

	/* Signature OID Fields */
	CSSMOID_X509V1SignatureStruct  						= {INTEL_X509V3_SIGN_R08_LENGTH+1,  (uint8 *)X509V1SignatureStruct},
	CSSMOID_X509V1SignatureCStruct  						= {INTEL_X509V3_SIGN_R08_LENGTH+2,  (uint8 *)X509V1SignatureCStruct},
	CSSMOID_X509V1SignatureAlgorithm  					= {INTEL_X509V3_SIGN_R08_LENGTH+1,  (uint8 *)X509V1SignatureAlgorithm},
	CSSMOID_X509V1SignatureAlgorithmTBS  					= {INTEL_X509V3_SIGN_R08_LENGTH+1,  (uint8 *)X509V1SignatureAlgorithmTBS},
	CSSMOID_X509V1SignatureAlgorithmParameters  			= {INTEL_X509V3_SIGN_R08_LENGTH+1,  (uint8 *)X509V1SignatureAlgorithmParameters},
	CSSMOID_X509V1Signature  							= {INTEL_X509V3_SIGN_R08_LENGTH+1,  (uint8 *)X509V1Signature},
	
	/* Extension OID Fields */
	CSSMOID_SubjectSignatureBitmap  						= {INTEL_X509V3_CERT_PRIVATE_EXTENSIONS_LENGTH+1,  (uint8 *)SubjectSignatureBitmap},
	CSSMOID_SubjectPicture  								= {INTEL_X509V3_CERT_PRIVATE_EXTENSIONS_LENGTH+1,  (uint8 *)SubjectPicture},
	CSSMOID_SubjectEmailAddress 							= {INTEL_X509V3_CERT_PRIVATE_EXTENSIONS_LENGTH+1,  (uint8 *)SubjectEmailAddress},
	CSSMOID_UseExemptions  								= {INTEL_X509V3_CERT_PRIVATE_EXTENSIONS_LENGTH+1, (uint8 *)UseExemptions};

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
	OID_IssuingDistributionPoints[]     = { OID_EXTENSION, 28 },
	OID_NameConstraints[]       		= { OID_EXTENSION, 30 },
	OID_CrlDistributionPoints[] 		= { OID_EXTENSION, 31 },
	OID_CertificatePolicies[]   		= { OID_EXTENSION, 32 },
	OID_PolicyMappings[]        		= { OID_EXTENSION, 33 },
	OID_AuthorityKeyIdentifier[]		= { OID_EXTENSION, 35 },
	OID_PolicyConstraints[]     		= { OID_EXTENSION, 36 },
	OID_ExtendedKeyUsage[] 				= { OID_EXTENSION, 37 },
	OID_ExtendedUseCodeSigning[]		= { OID_EXTENSION, 37, 3 }
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
CSSMOID_IssuingDistributionPoints = { OID_PKCS_CE_LENGTH, (uint8 *)OID_IssuingDistributionPoints},
CSSMOID_NameConstraints  		= { OID_PKCS_CE_LENGTH, (uint8 *)OID_NameConstraints},
CSSMOID_CrlDistributionPoints  	= { OID_PKCS_CE_LENGTH, (uint8 *)OID_CrlDistributionPoints},
CSSMOID_CertificatePolicies  	= { OID_PKCS_CE_LENGTH, (uint8 *)OID_CertificatePolicies},
CSSMOID_PolicyMappings  		= { OID_PKCS_CE_LENGTH, (uint8 *)OID_PolicyMappings},
CSSMOID_PolicyConstraints  		= { OID_PKCS_CE_LENGTH, (uint8 *)OID_PolicyConstraints},
CSSMOID_AuthorityKeyIdentifier  = { OID_PKCS_CE_LENGTH, (uint8 *)OID_AuthorityKeyIdentifier},
CSSMOID_ExtendedKeyUsage  		= { OID_PKCS_CE_LENGTH, (uint8 *)OID_ExtendedKeyUsage},
CSSMOID_ExtendedUseCodeSigning	= { OID_PKCS_CE_LENGTH+1, (uint8 *)OID_ExtendedUseCodeSigning};


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


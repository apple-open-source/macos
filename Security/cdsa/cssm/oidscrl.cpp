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

#include <Security/oidscrl.h>
 
static const uint8

	/* CRL OIDs */
	X509V2CRLSignedCrlStruct[]					= {INTEL_X509V2_CRL_R08, 0},
	X509V2CRLSignedCrlCStruct	[]				= {INTEL_X509V2_CRL_R08, 0, INTEL_X509_C_DATATYPE},
	X509V2CRLTbsCertListStruct	[]				= {INTEL_X509V2_CRL_R08, 1},
	X509V2CRLTbsCertListCStruct[]				= {INTEL_X509V2_CRL_R08, 1, INTEL_X509_C_DATATYPE},
	X509V2CRLVersion	[]						= {INTEL_X509V2_CRL_R08, 2},
	X509V1CRLIssuerStruct[]						= {INTEL_X509V2_CRL_R08, 3},
	X509V1CRLIssuerNameCStruct[]				= {INTEL_X509V2_CRL_R08, 3, INTEL_X509_C_DATATYPE},
	X509V1CRLIssuerNameLDAP[]					= {INTEL_X509V2_CRL_R08, 3, 
													INTEL_X509_LDAPSTRING_DATATYPE},
	X509V1CRLThisUpdate[]						= {INTEL_X509V2_CRL_R08, 4},
	X509V1CRLNextUpdate[]						= {INTEL_X509V2_CRL_R08, 5},
	
	/* CRL Entry (CRL CertList) OIDS */
	X509V1CRLRevokedCertificatesStruct[]		= {INTEL_X509V2_CRL_R08, 7},
	X509V1CRLRevokedCertificatesCStruct[]		= {INTEL_X509V2_CRL_R08, 7, INTEL_X509_C_DATATYPE},
	X509V1CRLNumberOfRevokedCertEntries[]		= {INTEL_X509V2_CRL_R08, 6},
	X509V1CRLRevokedEntryStruct[]				= {INTEL_X509V2_CRL_R08, 15},
	X509V1CRLRevokedEntryCStruct[]				= {INTEL_X509V2_CRL_R08, 15, INTEL_X509_C_DATATYPE},
	X509V1CRLRevokedEntrySerialNumber[]			= {INTEL_X509V2_CRL_R08, 16},
	X509V1CRLRevokedEntryRevocationDate[]		= {INTEL_X509V2_CRL_R08, 17},
	
	/* CRL Entry (CRL CertList) Extension OIDs */
	X509V2CRLRevokedEntryAllExtensionsStruct[]	= {INTEL_X509V2_CRL_R08, 18},
	X509V2CRLRevokedEntryAllExtensionsCStruct[]	= {INTEL_X509V2_CRL_R08, 18, INTEL_X509_C_DATATYPE},
	X509V2CRLRevokedEntryNumberOfExtensions[]	= {INTEL_X509V2_CRL_R08, 20},
	X509V2CRLRevokedEntrySingleExtensionStruct[]= {INTEL_X509V2_CRL_R08, 19},
	X509V2CRLRevokedEntrySingleExtensionCStruct[]= {INTEL_X509V2_CRL_R08, 19, INTEL_X509_C_DATATYPE},
	X509V2CRLRevokedEntryExtensionId[]			= {INTEL_X509V2_CRL_R08, 21},
	X509V2CRLRevokedEntryExtensionCritical[]	= {INTEL_X509V2_CRL_R08, 22},
	X509V2CRLRevokedEntryExtensionType[]		= {INTEL_X509V2_CRL_R08, 23},
	X509V2CRLRevokedEntryExtensionValue[]		= {INTEL_X509V2_CRL_R08, 24},
	
	/* CRL Extension OIDs */
	X509V2CRLAllExtensionsStruct[]				= {INTEL_X509V2_CRL_R08, 8},
	X509V2CRLAllExtensionsCStruct[]				= {INTEL_X509V2_CRL_R08, 8, INTEL_X509_C_DATATYPE},
	X509V2CRLNumberOfExtensions[]				= {INTEL_X509V2_CRL_R08, 10},
	X509V2CRLSingleExtensionStruct[]			= {INTEL_X509V2_CRL_R08, 9},
	X509V2CRLSingleExtensionCStruct[]			= {INTEL_X509V2_CRL_R08, 9, INTEL_X509_C_DATATYPE},
	X509V2CRLExtensionId[]						= {INTEL_X509V2_CRL_R08, 11},
	X509V2CRLExtensionCritical[]				= {INTEL_X509V2_CRL_R08, 12},
	X509V2CRLExtensionType[]					= {INTEL_X509V2_CRL_R08, 13};

const CSSM_OID

	/* CRL OIDs */
	CSSMOID_X509V2CRLSignedCrlStruct 			= {INTEL_X509V2_CRL_R08_LENGTH+1, 
													(uint8 *)X509V2CRLSignedCrlStruct},
	CSSMOID_X509V2CRLSignedCrlCStruct 			= {INTEL_X509V2_CRL_R08_LENGTH+2, 
													(uint8 *)X509V2CRLSignedCrlCStruct},
	CSSMOID_X509V2CRLTbsCertListStruct 			= {INTEL_X509V2_CRL_R08_LENGTH+1, 
													(uint8 *)X509V2CRLTbsCertListStruct},
	CSSMOID_X509V2CRLTbsCertListCStruct 		= {INTEL_X509V2_CRL_R08_LENGTH+2, 
													(uint8 *)X509V2CRLTbsCertListCStruct},
	CSSMOID_X509V2CRLVersion 					= {INTEL_X509V2_CRL_R08_LENGTH+1, 
													(uint8 *)X509V2CRLVersion},
	CSSMOID_X509V1CRLIssuerStruct 				= {INTEL_X509V2_CRL_R08_LENGTH+1, 
													(uint8 *)X509V1CRLIssuerStruct},
	CSSMOID_X509V1CRLIssuerNameCStruct 			= {INTEL_X509V2_CRL_R08_LENGTH+2, 
													(uint8 *)X509V1CRLIssuerNameCStruct},
	CSSMOID_X509V1CRLIssuerNameLDAP 			= {INTEL_X509V2_CRL_R08_LENGTH+2, 
													(uint8 *)X509V1CRLIssuerNameLDAP},
	CSSMOID_X509V1CRLThisUpdate 				= {INTEL_X509V2_CRL_R08_LENGTH+1, 
													(uint8 *)X509V1CRLThisUpdate},
	CSSMOID_X509V1CRLNextUpdate 				= {INTEL_X509V2_CRL_R08_LENGTH+1, 
													(uint8 *)X509V1CRLNextUpdate},

	/* CRL Entry (CRL CertList) OIDS */
	CSSMOID_X509V1CRLRevokedCertificatesStruct 	= {INTEL_X509V2_CRL_R08_LENGTH+1, 
													(uint8 *)X509V1CRLRevokedCertificatesStruct},
	CSSMOID_X509V1CRLRevokedCertificatesCStruct	= {INTEL_X509V2_CRL_R08_LENGTH+2, 
													(uint8 *)X509V1CRLRevokedCertificatesCStruct},
	CSSMOID_X509V1CRLNumberOfRevokedCertEntries	= {INTEL_X509V2_CRL_R08_LENGTH+1, 
													(uint8 *)X509V1CRLNumberOfRevokedCertEntries},
	CSSMOID_X509V1CRLRevokedEntryStruct 		= {INTEL_X509V2_CRL_R08_LENGTH+1, 
													(uint8 *)X509V1CRLRevokedEntryStruct},
	CSSMOID_X509V1CRLRevokedEntryCStruct 		= {INTEL_X509V2_CRL_R08_LENGTH+2, 
													(uint8 *)X509V1CRLRevokedEntryCStruct},
	CSSMOID_X509V1CRLRevokedEntrySerialNumber 	= {INTEL_X509V2_CRL_R08_LENGTH+1, 
													(uint8 *)X509V1CRLRevokedEntrySerialNumber},
	CSSMOID_X509V1CRLRevokedEntryRevocationDate	= {INTEL_X509V2_CRL_R08_LENGTH+1, 
													(uint8 *)X509V1CRLRevokedEntryRevocationDate},

	/* CRL Entry (CRL CertList) Extension OIDs */
	CSSMOID_X509V2CRLRevokedEntryAllExtensionsStruct 	= {INTEL_X509V2_CRL_R08_LENGTH+1, 
													(uint8 *)X509V2CRLRevokedEntryAllExtensionsStruct},
	CSSMOID_X509V2CRLRevokedEntryAllExtensionsCStruct 	= {INTEL_X509V2_CRL_R08_LENGTH+2, 
													(uint8 *)X509V2CRLRevokedEntryAllExtensionsCStruct},
	CSSMOID_X509V2CRLRevokedEntryNumberOfExtensions 	= {INTEL_X509V2_CRL_R08_LENGTH+1, 
													(uint8 *)X509V2CRLRevokedEntryNumberOfExtensions},
	CSSMOID_X509V2CRLRevokedEntrySingleExtensionStruct 	= {INTEL_X509V2_CRL_R08_LENGTH+1, 
													(uint8 *)X509V2CRLRevokedEntrySingleExtensionStruct},
	CSSMOID_X509V2CRLRevokedEntrySingleExtensionCStruct = {INTEL_X509V2_CRL_R08_LENGTH+2, 
													(uint8 *)X509V2CRLRevokedEntrySingleExtensionCStruct},
	CSSMOID_X509V2CRLRevokedEntryExtensionId 			= {INTEL_X509V2_CRL_R08_LENGTH+1, 
													(uint8 *)X509V2CRLRevokedEntryExtensionId},
	CSSMOID_X509V2CRLRevokedEntryExtensionCritical 		= {INTEL_X509V2_CRL_R08_LENGTH+1, 
													(uint8 *)X509V2CRLRevokedEntryExtensionCritical},
	CSSMOID_X509V2CRLRevokedEntryExtensionType 			= {INTEL_X509V2_CRL_R08_LENGTH+1, 
													(uint8 *)X509V2CRLRevokedEntryExtensionType},
	CSSMOID_X509V2CRLRevokedEntryExtensionValue 		= {INTEL_X509V2_CRL_R08_LENGTH+1, 
													 (uint8 *)X509V2CRLRevokedEntryExtensionValue},

	/* CRL Extension OIDs */
	CSSMOID_X509V2CRLAllExtensionsStruct 		= {INTEL_X509V2_CRL_R08_LENGTH+1, 
													(uint8 *)X509V2CRLAllExtensionsStruct},
	CSSMOID_X509V2CRLAllExtensionsCStruct 		= {INTEL_X509V2_CRL_R08_LENGTH+2, 
													(uint8 *)X509V2CRLAllExtensionsCStruct},
	CSSMOID_X509V2CRLNumberOfExtensions 		= {INTEL_X509V2_CRL_R08_LENGTH+1, 
													(uint8 *)X509V2CRLNumberOfExtensions},
	CSSMOID_X509V2CRLSingleExtensionStruct 		= {INTEL_X509V2_CRL_R08_LENGTH+1, 
													(uint8 *)X509V2CRLSingleExtensionStruct},
	CSSMOID_X509V2CRLSingleExtensionCStruct 	= {INTEL_X509V2_CRL_R08_LENGTH+2, 
													(uint8 *)X509V2CRLSingleExtensionCStruct},
	CSSMOID_X509V2CRLExtensionId 				= {INTEL_X509V2_CRL_R08_LENGTH+1, 
													(uint8 *)X509V2CRLExtensionId},
	CSSMOID_X509V2CRLExtensionCritical 			= {INTEL_X509V2_CRL_R08_LENGTH+1, 
													(uint8 *)X509V2CRLExtensionCritical},
	CSSMOID_X509V2CRLExtensionType 				= {INTEL_X509V2_CRL_R08_LENGTH+1, 
													(uint8 *)X509V2CRLExtensionType};

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
 * oidsattr.c - Cert/CRL related OIDs.
 */

#include <Security/oidsattr.h>


/* 
 * Directory name component identifiers.
 */
static const uint8
	OID_ObjectClass[]           		= { OID_ATTR_TYPE, 0 },
	OID_AliasedEntryName[]      		= { OID_ATTR_TYPE, 1 },
	OID_KnowledgeInformation[]  		= { OID_ATTR_TYPE, 2 },
	OID_CommonName[]            		= { OID_ATTR_TYPE, 3 },
	OID_Surname[]              			= { OID_ATTR_TYPE, 4 },
	OID_SerialNumber[]         			= { OID_ATTR_TYPE, 5 },
	OID_CountryName[]           		= { OID_ATTR_TYPE, 6 },
	OID_LocalityName[]          		= { OID_ATTR_TYPE, 7 },
	OID_StateProvinceName[]     		= { OID_ATTR_TYPE, 8 },
	OID_CollectiveStateProvinceName[] 	= { OID_ATTR_TYPE, 8, 1 },
	OID_StreetAddress[]         		= { OID_ATTR_TYPE, 9 },
	OID_CollectiveStreetAddress[]     	= { OID_ATTR_TYPE, 9, 1 },
	OID_OrganizationName[]      		= { OID_ATTR_TYPE, 10 },
	OID_CollectiveOrganizationName[]  	= { OID_ATTR_TYPE, 10, 1 },
	OID_OrganizationalUnitName[]		= { OID_ATTR_TYPE, 11 },
	OID_CollectiveOrganizationalUnitName[]
										= { OID_ATTR_TYPE, 11, 1 },
	OID_Title[]                 		= { OID_ATTR_TYPE, 12 },
	OID_Description[]           		= { OID_ATTR_TYPE, 13 },
	OID_SearchGuide[]           		= { OID_ATTR_TYPE, 14 },
	OID_BusinessCategory[]      		= { OID_ATTR_TYPE, 15 },
	OID_PostalAddress[]         		= { OID_ATTR_TYPE, 16 },
	OID_CollectivePostalAddress[]     	= { OID_ATTR_TYPE, 16, 1 },
	OID_PostalCode[]            		= { OID_ATTR_TYPE, 17 },
	OID_CollectivePostalCode[]  		= { OID_ATTR_TYPE, 17, 1 },
	OID_PostOfficeBox[]         		= { OID_ATTR_TYPE, 18 },
	OID_CollectivePostOfficeBox[]     	= { OID_ATTR_TYPE, 18, 1 },
	OID_PhysicalDeliveryOfficeName[]  	= { OID_ATTR_TYPE, 19 },
	OID_CollectivePhysicalDeliveryOfficeName[]
										= { OID_ATTR_TYPE, 19, 1 },
	OID_TelephoneNumber[]       		= { OID_ATTR_TYPE, 20 },
	OID_CollectiveTelephoneNumber[]  	= { OID_ATTR_TYPE, 20, 1 },
	OID_TelexNumber[]           		= { OID_ATTR_TYPE, 21 },
	OID_CollectiveTelexNumber[] 		= { OID_ATTR_TYPE, 21, 1 },
	OID_TelexTerminalIdentifier[]     	= { OID_ATTR_TYPE, 22 },
	OID_CollectiveTelexTerminalIdentifier[] 
										= { OID_ATTR_TYPE, 22, 1 },
	OID_FacsimileTelephoneNumber[]    	= { OID_ATTR_TYPE, 23 },
	OID_CollectiveFacsimileTelephoneNumber[]
										= { OID_ATTR_TYPE, 23, 1 },
	OID_X_121Address[]          		= { OID_ATTR_TYPE, 24 },
	OID_InternationalISDNNumber[]     	= { OID_ATTR_TYPE, 25 },
	OID_CollectiveInternationalISDNNumber[] 
										= { OID_ATTR_TYPE, 25, 1 },
	OID_RegisteredAddress[]     		= { OID_ATTR_TYPE, 26 },
	OID_DestinationIndicator[]  		= { OID_ATTR_TYPE, 27 },
	OID_PreferredDeliveryMethod[] 		= { OID_ATTR_TYPE, 28 },
	OID_PresentationAddress[]   		= { OID_ATTR_TYPE, 29 },
	OID_SupportedApplicationContext[] 	= { OID_ATTR_TYPE, 30 },
	OID_Member[]                		= { OID_ATTR_TYPE, 31 },
	OID_Owner[]                			= { OID_ATTR_TYPE, 32 },
	OID_RoleOccupant[]          		= { OID_ATTR_TYPE, 33 },
	OID_SeeAlso[]               		= { OID_ATTR_TYPE, 34 },
	OID_UserPassword[]          		= { OID_ATTR_TYPE, 35 },
	OID_UserCertificate[]       		= { OID_ATTR_TYPE, 36 },
	OID_CACertificate[]         		= { OID_ATTR_TYPE, 37 },
	OID_AuthorityRevocationList[] 		= { OID_ATTR_TYPE, 38 },
	OID_CertificateRevocationList[] 	= { OID_ATTR_TYPE, 39 },
	OID_CrossCertificatePair[]  		= { OID_ATTR_TYPE, 40 },
	OID_Name[]                  		= { OID_ATTR_TYPE, 41 },
	OID_GivenName[]             		= { OID_ATTR_TYPE, 42 },
	OID_Initials[]              		= { OID_ATTR_TYPE, 43 },
	OID_GenerationQualifier[]   		= { OID_ATTR_TYPE, 44 },
	OID_UniqueIdentifier[]     			= { OID_ATTR_TYPE, 45 },
	OID_DNQualifier[]           		= { OID_ATTR_TYPE, 46 },
	OID_EnhancedSearchGuide[]   		= { OID_ATTR_TYPE, 47 },
	OID_ProtocolInformation[]   		= { OID_ATTR_TYPE, 48 },
	OID_DistinguishedName[]     		= { OID_ATTR_TYPE, 49 },
	OID_UniqueMember[]          		= { OID_ATTR_TYPE, 50 },
	OID_HouseIdentifier[]       		= { OID_ATTR_TYPE, 51 }
;

const CSSM_OID
CSSMOID_ObjectClass        		= { OID_ATTR_TYPE_LENGTH+1, (uint8 *)OID_ObjectClass},
CSSMOID_AliasedEntryName    	= { OID_ATTR_TYPE_LENGTH+1, (uint8 *)OID_AliasedEntryName},
CSSMOID_KnowledgeInformation	= { OID_ATTR_TYPE_LENGTH+1, (uint8 *)OID_KnowledgeInformation},
CSSMOID_CommonName          	= { OID_ATTR_TYPE_LENGTH+1, (uint8 *)OID_CommonName},
CSSMOID_Surname             	= { OID_ATTR_TYPE_LENGTH+1, (uint8 *)OID_Surname},
CSSMOID_SerialNumber       		= { OID_ATTR_TYPE_LENGTH+1, (uint8 *)OID_SerialNumber},
CSSMOID_CountryName         	= { OID_ATTR_TYPE_LENGTH+1, (uint8 *)OID_CountryName},
CSSMOID_LocalityName        	= { OID_ATTR_TYPE_LENGTH+1, (uint8 *)OID_LocalityName},
CSSMOID_StateProvinceName   	= { OID_ATTR_TYPE_LENGTH+1, (uint8 *)OID_StateProvinceName},
CSSMOID_CollectiveStateProvinceName
								= { OID_ATTR_TYPE_LENGTH+2, (uint8 *)OID_CollectiveStateProvinceName},
CSSMOID_StreetAddress       	= { OID_ATTR_TYPE_LENGTH+1, (uint8 *)OID_StreetAddress},
CSSMOID_CollectiveStreetAddress = { OID_ATTR_TYPE_LENGTH+2, (uint8 *)OID_CollectiveStreetAddress},
CSSMOID_OrganizationName    	= { OID_ATTR_TYPE_LENGTH+1, (uint8 *)OID_OrganizationName},
CSSMOID_CollectiveOrganizationName  
								= { OID_ATTR_TYPE_LENGTH+2, (uint8 *)OID_CollectiveOrganizationName},
CSSMOID_OrganizationalUnitName  = { OID_ATTR_TYPE_LENGTH+1, (uint8 *)OID_OrganizationalUnitName},
CSSMOID_CollectiveOrganizationalUnitName
								= { OID_ATTR_TYPE_LENGTH+2, (uint8 *)OID_CollectiveOrganizationalUnitName},
CSSMOID_Title              		= { OID_ATTR_TYPE_LENGTH+1, (uint8 *)OID_Title},
CSSMOID_Description        		= { OID_ATTR_TYPE_LENGTH+1, (uint8 *)OID_Description},
CSSMOID_SearchGuide         	= { OID_ATTR_TYPE_LENGTH+1, (uint8 *)OID_SearchGuide},
CSSMOID_BusinessCategory    	= { OID_ATTR_TYPE_LENGTH+1, (uint8 *)OID_BusinessCategory},
CSSMOID_PostalAddress       	= { OID_ATTR_TYPE_LENGTH+1, (uint8 *)OID_PostalAddress},
CSSMOID_CollectivePostalAddress = { OID_ATTR_TYPE_LENGTH+2, (uint8 *)OID_CollectivePostalAddress},
CSSMOID_PostalCode          	= { OID_ATTR_TYPE_LENGTH+1, (uint8 *)OID_PostalCode},
CSSMOID_CollectivePostalCode	= { OID_ATTR_TYPE_LENGTH+2, (uint8 *)OID_CollectivePostalCode},
CSSMOID_PostOfficeBox       	= { OID_ATTR_TYPE_LENGTH+1, (uint8 *)OID_PostOfficeBox},
CSSMOID_CollectivePostOfficeBox = { OID_ATTR_TYPE_LENGTH+2, (uint8 *)OID_CollectivePostOfficeBox},
CSSMOID_PhysicalDeliveryOfficeName  
								= { OID_ATTR_TYPE_LENGTH+1, (uint8 *)OID_PhysicalDeliveryOfficeName},
CSSMOID_CollectivePhysicalDeliveryOfficeName 
								= { OID_ATTR_TYPE_LENGTH+2, (uint8 *)OID_CollectivePhysicalDeliveryOfficeName},
CSSMOID_TelephoneNumber     	= { OID_ATTR_TYPE_LENGTH+1, (uint8 *)OID_TelephoneNumber},
CSSMOID_CollectiveTelephoneNumber   
								= { OID_ATTR_TYPE_LENGTH+2, (uint8 *)OID_CollectiveTelephoneNumber},
CSSMOID_TelexNumber         	= { OID_ATTR_TYPE_LENGTH+1, (uint8 *)OID_TelexNumber},
CSSMOID_CollectiveTelexNumber   = { OID_ATTR_TYPE_LENGTH+2, (uint8 *)OID_CollectiveTelexNumber},
CSSMOID_TelexTerminalIdentifier = { OID_ATTR_TYPE_LENGTH+1, (uint8 *)OID_TelexTerminalIdentifier},
CSSMOID_CollectiveTelexTerminalIdentifier 
								= { OID_ATTR_TYPE_LENGTH+2, (uint8 *)OID_CollectiveTelexTerminalIdentifier},
CSSMOID_FacsimileTelephoneNumber= { OID_ATTR_TYPE_LENGTH+1, (uint8 *)OID_FacsimileTelephoneNumber},
CSSMOID_CollectiveFacsimileTelephoneNumber
								= { OID_ATTR_TYPE_LENGTH+2, (uint8 *)OID_CollectiveFacsimileTelephoneNumber},
CSSMOID_X_121Address        	= { OID_ATTR_TYPE_LENGTH+1, (uint8 *)OID_X_121Address},
CSSMOID_InternationalISDNNumber = { OID_ATTR_TYPE_LENGTH+1, (uint8 *)OID_InternationalISDNNumber},
CSSMOID_CollectiveInternationalISDNNumber 
								= { OID_ATTR_TYPE_LENGTH+2, (uint8 *)OID_CollectiveInternationalISDNNumber},
CSSMOID_RegisteredAddress   	= { OID_ATTR_TYPE_LENGTH+1, (uint8 *)OID_RegisteredAddress},
CSSMOID_DestinationIndicator	= { OID_ATTR_TYPE_LENGTH+1, (uint8 *)OID_DestinationIndicator},
CSSMOID_PreferredDeliveryMethod = { OID_ATTR_TYPE_LENGTH+1, (uint8 *)OID_PreferredDeliveryMethod},
CSSMOID_PresentationAddress 	= { OID_ATTR_TYPE_LENGTH+1, (uint8 *)OID_PresentationAddress},
CSSMOID_SupportedApplicationContext 
								= { OID_ATTR_TYPE_LENGTH+1, (uint8 *)OID_SupportedApplicationContext},
CSSMOID_Member              	= { OID_ATTR_TYPE_LENGTH+1, (uint8 *)OID_Member},
CSSMOID_Owner               	= { OID_ATTR_TYPE_LENGTH+1, (uint8 *)OID_Owner},
CSSMOID_RoleOccupant        	= { OID_ATTR_TYPE_LENGTH+1, (uint8 *)OID_RoleOccupant},
CSSMOID_SeeAlso             	= { OID_ATTR_TYPE_LENGTH+1, (uint8 *)OID_SeeAlso},
CSSMOID_UserPassword        	= { OID_ATTR_TYPE_LENGTH+1, (uint8 *)OID_UserPassword},
CSSMOID_UserCertificate     	= { OID_ATTR_TYPE_LENGTH+1, (uint8 *)OID_UserCertificate},
CSSMOID_CACertificate       	= { OID_ATTR_TYPE_LENGTH+1, (uint8 *)OID_CACertificate},
CSSMOID_AuthorityRevocationList = { OID_ATTR_TYPE_LENGTH+1, (uint8 *)OID_AuthorityRevocationList},
CSSMOID_CertificateRevocationList
								= { OID_ATTR_TYPE_LENGTH+1, (uint8 *)OID_CertificateRevocationList},
CSSMOID_CrossCertificatePair	= { OID_ATTR_TYPE_LENGTH+1, (uint8 *)OID_CrossCertificatePair},
CSSMOID_Name                	= { OID_ATTR_TYPE_LENGTH+1, (uint8 *)OID_Name},
CSSMOID_GivenName           	= { OID_ATTR_TYPE_LENGTH+1, (uint8 *)OID_GivenName},
CSSMOID_Initials            	= { OID_ATTR_TYPE_LENGTH+1, (uint8 *)OID_Initials},
CSSMOID_GenerationQualifier 	= { OID_ATTR_TYPE_LENGTH+1, (uint8 *)OID_GenerationQualifier},
CSSMOID_UniqueIdentifier    	= { OID_ATTR_TYPE_LENGTH+1, (uint8 *)OID_UniqueIdentifier},
CSSMOID_DNQualifier         	= { OID_ATTR_TYPE_LENGTH+1, (uint8 *)OID_DNQualifier},
CSSMOID_EnhancedSearchGuide 	= { OID_ATTR_TYPE_LENGTH+1, (uint8 *)OID_EnhancedSearchGuide},
CSSMOID_ProtocolInformation 	= { OID_ATTR_TYPE_LENGTH+1, (uint8 *)OID_ProtocolInformation},
CSSMOID_DistinguishedName   	= { OID_ATTR_TYPE_LENGTH+1, (uint8 *)OID_DistinguishedName},
CSSMOID_UniqueMember        	= { OID_ATTR_TYPE_LENGTH+1, (uint8 *)OID_UniqueMember},
CSSMOID_HouseIdentifier     	= { OID_ATTR_TYPE_LENGTH+1, (uint8 *)OID_HouseIdentifier}
;


/* From PKCS 9 */
static const uint8
	OID_EmailAddress[]          = { OID_PKCS_9, 1 },
	OID_UnstructuredName[]      = { OID_PKCS_9, 2 },
	OID_ContentType[]           = { OID_PKCS_9, 3 },
	OID_MessageDigest[]         = { OID_PKCS_9, 4 },
	OID_SigningTime[]           = { OID_PKCS_9, 5 },
	OID_CounterSignature[]      = { OID_PKCS_9, 6 },
	OID_ChallengePassword[]     = { OID_PKCS_9, 7 },
	OID_UnstructuredAddress[]   = { OID_PKCS_9, 8 },
	OID_ExtendedCertificateAttributes[] = { OID_PKCS_9, 9 }
;

const CSSM_OID
CSSMOID_EmailAddress        = {OID_PKCS_9_LENGTH+1, (uint8 *)OID_EmailAddress},
CSSMOID_UnstructuredName    = {OID_PKCS_9_LENGTH+1, (uint8 *)OID_UnstructuredName},
CSSMOID_ContentType         = {OID_PKCS_9_LENGTH+1, (uint8 *)OID_ContentType},
CSSMOID_MessageDigest       = {OID_PKCS_9_LENGTH+1, (uint8 *)OID_MessageDigest},
CSSMOID_SigningTime         = {OID_PKCS_9_LENGTH+1, (uint8 *)OID_SigningTime},
CSSMOID_CounterSignature    = {OID_PKCS_9_LENGTH+1, (uint8 *)OID_CounterSignature},
CSSMOID_ChallengePassword   = {OID_PKCS_9_LENGTH+1, (uint8 *)OID_ChallengePassword},
CSSMOID_UnstructuredAddress = {OID_PKCS_9_LENGTH+1, (uint8 *)OID_UnstructuredAddress},
CSSMOID_ExtendedCertificateAttributes = {OID_PKCS_9_LENGTH+1, (uint8 *)OID_ExtendedCertificateAttributes};

/* PKIX */
static const uint8
	OID_QT_CPS[]			= { OID_QT, 1},
	OID_QT_UNOTICE[]		= { OID_QT, 2}
;

const CSSM_OID
CSSMOID_QT_CPS				= {OID_QT_LENGTH+1, (uint8 *)OID_QT_CPS},
CSSMOID_QT_UNOTICE			= {OID_QT_LENGTH+1, (uint8 *)OID_QT_UNOTICE}
;

#define OID_PKCS12_BagTypes			OID_PKCS_12,10,1
#define OID_PKCS12_BagTypesLength	OID_PKCS_12_LENGTH+2

#define ID_PKCS9_CertTypes			OID_PKCS_9, 22
#define OID_PKCS9_CertTypesLength	OID_PKCS_9_LENGTH+1
#define ID_PKCS9_CrlTypes			OID_PKCS_9, 23
#define OID_PKCS9_CrlTypesLength	OID_PKCS_9_LENGTH+1

static const uint8
	OID_PKCS7_Data[] = 						{ OID_PKCS_7, 1},	
	OID_PKCS7_SignedData[] = 				{ OID_PKCS_7, 2},	
	OID_PKCS7_EnvelopedData[] = 			{ OID_PKCS_7, 3},	
	OID_PKCS7_SignedAndEnvelopedData[] = 	{ OID_PKCS_7, 4},
	OID_PKCS7_DigestedData[] =		 		{ OID_PKCS_7, 5},
	OID_PKCS7_EncryptedData[] = 			{ OID_PKCS_7, 6},
	OID_PKCS7_DataWithAttributes[] = 		{ OID_PKCS_7, 7},
	OID_PKCS7_EncryptedPrivateKeyInfo[] = 	{ OID_PKCS_7, 8},
	
	OID_PKCS9_FriendlyName[] =				{ OID_PKCS_9, 20},
	OID_PKCS9_LocalKeyId[] =				{ OID_PKCS_9, 21},
	OID_PKCS9_CertTypes[] =					{ ID_PKCS9_CertTypes },
	OID_PKCS9_CrlTypes[] =					{ ID_PKCS9_CrlTypes },
	OID_PKCS9_X509Certificate[] =			{ ID_PKCS9_CertTypes, 1 },
	OID_PKCS9_SdsiCertificate[] =			{ ID_PKCS9_CertTypes, 2 },
	OID_PKCS9_X509Crl[] =					{ ID_PKCS9_CrlTypes, 1 },
	
	OID_PKCS12_keyBag[] =					{ OID_PKCS12_BagTypes, 1},
	OID_PKCS12_shroundedKeyBag[] =			{ OID_PKCS12_BagTypes, 2},
	OID_PKCS12_certBag[] =					{ OID_PKCS12_BagTypes, 3},
	OID_PKCS12_crlBag[] =					{ OID_PKCS12_BagTypes, 4},
	OID_PKCS12_secretBag[] =				{ OID_PKCS12_BagTypes, 5},
	OID_PKCS12_safeContentsBag[] =			{ OID_PKCS12_BagTypes, 6}
;

const CSSM_OID 
CSSMOID_PKCS7_Data = {OID_PKCS_7_LENGTH + 1,
					  (uint8 *)OID_PKCS7_Data },
CSSMOID_PKCS7_SignedData = {OID_PKCS_7_LENGTH + 1,
					  (uint8 *)OID_PKCS7_SignedData },
CSSMOID_PKCS7_EnvelopedData = {OID_PKCS_7_LENGTH + 1,
					  (uint8 *)OID_PKCS7_EnvelopedData },
CSSMOID_PKCS7_SignedAndEnvelopedData = {OID_PKCS_7_LENGTH + 1,
					  (uint8 *)OID_PKCS7_SignedAndEnvelopedData },
CSSMOID_PKCS7_DigestedData = {OID_PKCS_7_LENGTH + 1,
					  (uint8 *)OID_PKCS7_DigestedData },
CSSMOID_PKCS7_EncryptedData = {OID_PKCS_7_LENGTH + 1,
					  (uint8 *)OID_PKCS7_EncryptedData },
CSSMOID_PKCS7_DataWithAttributes = {OID_PKCS_7_LENGTH + 1,
					  (uint8 *)OID_PKCS7_DataWithAttributes },
CSSMOID_PKCS7_EncryptedPrivateKeyInfo = {OID_PKCS_7_LENGTH + 1,
					  (uint8 *)OID_PKCS7_EncryptedPrivateKeyInfo },
					  
CSSMOID_PKCS9_FriendlyName = {OID_PKCS_9_LENGTH + 1,
						(uint8 *)OID_PKCS9_FriendlyName },
CSSMOID_PKCS9_LocalKeyId = {OID_PKCS_9_LENGTH + 1,
						(uint8 *)OID_PKCS9_LocalKeyId },
CSSMOID_PKCS9_CertTypes = {OID_PKCS_9_LENGTH + 1,
						(uint8 *)OID_PKCS9_CertTypes },
CSSMOID_PKCS9_CrlTypes = {OID_PKCS_9_LENGTH + 1,
						(uint8 *)OID_PKCS9_CrlTypes },
CSSMOID_PKCS9_X509Certificate = {OID_PKCS9_CertTypesLength + 1,
						(uint8 *)OID_PKCS9_X509Certificate },
CSSMOID_PKCS9_SdsiCertificate = {OID_PKCS9_CertTypesLength + 1,
						(uint8 *)OID_PKCS9_SdsiCertificate },
CSSMOID_PKCS9_X509Crl = {OID_PKCS9_CrlTypesLength + 1,
						(uint8 *)OID_PKCS9_X509Crl },
	

CSSMOID_PKCS12_keyBag = {OID_PKCS12_BagTypesLength + 1,
					(uint8 *)OID_PKCS12_keyBag },
CSSMOID_PKCS12_shroudedKeyBag = {OID_PKCS12_BagTypesLength + 1,
					(uint8 *)OID_PKCS12_shroundedKeyBag },
CSSMOID_PKCS12_certBag = {OID_PKCS12_BagTypesLength + 1,
					(uint8 *)OID_PKCS12_certBag },
CSSMOID_PKCS12_crlBag = {OID_PKCS12_BagTypesLength + 1,
					(uint8 *)OID_PKCS12_crlBag },
CSSMOID_PKCS12_secretBag = {OID_PKCS12_BagTypesLength + 1,
					(uint8 *)OID_PKCS12_secretBag },
CSSMOID_PKCS12_safeContentsBag = {OID_PKCS12_BagTypesLength + 1,
					(uint8 *)OID_PKCS12_safeContentsBag }

;



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
 * oidsalg.c - OIDs defining crypto algorithms
 */

#include <Security/oidsbase.h>
#include <Security/cssmtype.h>
#include <Security/cssmapple.h>
#include <string.h>

static const uint8
	OID_MD2[]   	       	= { OID_RSA_HASH, 2 },
	OID_MD4[]       	   	= { OID_RSA_HASH, 4 },
	OID_MD5[]          		= { OID_RSA_HASH, 5 },
	OID_RSAEncryption[]		= { OID_PKCS_1, 1 },	
	OID_MD2WithRSA[]   		= { OID_PKCS_1, 2 },
	OID_MD4WithRSA[]   		= { OID_PKCS_1, 3 },
	OID_MD5WithRSA[]   		= { OID_PKCS_1, 4 },
	OID_SHA1WithRSA[]  		= { OID_PKCS_1, 5 },
	OID_PKCS_3_ARC[]		= { OID_PKCS_3 },
	OID_DHKeyAgreement[]    = { OID_PKCS_3, 1 },
	/* BSAFE-specific DSA */
	OID_OIW_DSA[]     		= { OID_OIW_ALGORITHM, 12  },  
	OID_OIW_DSAWithSHA1[] 	= { OID_OIW_ALGORITHM, 27  },
	/* DSA from CMS */
	OID_CMS_DSA[]			= { 0x2A, 0x86, 0x48, 0xCE, 0x38, 4, 1 },
	OID_CMS_DSAWithSHA1[]	= { 0x2A, 0x86, 0x48, 0xCE, 0x38, 4, 3 },
	/* DSA from JDK 1.1 */
	OID_JDK_DSA[]			= { 0x2B, 0x0E, 0x03, 0x02, 0x0c },
	OID_JDK_DSAWithSHA1[]	= { 0x2B, 0x0E, 0x03, 0x02, 0x0D },
	
	OID_OIW_SHA1[]			= { OID_OIW_ALGORITHM, 26  },
	OID_OIW_RSAWithSHA1[]   = { OID_OIW_ALGORITHM, 29  },
	
	/* ANSI X9.42 */
	OID_ANSI_DH_PUB_NUMBER[]= { OID_ANSI_X9_42, 1 },
	OID_ANSI_DH_STATIC[] 	= { OID_ANSI_X9_42_SCHEME, 1 },
	OID_ANSI_DH_EPHEM[] 	= { OID_ANSI_X9_42_SCHEME, 2 },
	OID_ANSI_DH_ONE_FLOW[] 	= { OID_ANSI_X9_42_SCHEME, 3 },
	OID_ANSI_DH_HYBRID1[] 	= { OID_ANSI_X9_42_SCHEME, 4 },
	OID_ANSI_DH_HYBRID2[] 	= { OID_ANSI_X9_42_SCHEME, 5 },
	OID_ANSI_DH_HYBRID_ONEFLOW[] 	= { OID_ANSI_X9_42_SCHEME, 6 },
	/* sic - enumerated in reverse order in the spec */
	OID_ANSI_MQV1[] 		= { OID_ANSI_X9_42_SCHEME, 8 },
	OID_ANSI_MQV2[] 		= { OID_ANSI_X9_42_SCHEME, 7 },

	OID_ANSI_DH_STATIC_SHA1[] 	= { OID_ANSI_X9_42_NAMED_SCHEME, 1 },
	OID_ANSI_DH_EPHEM_SHA1[] 	= { OID_ANSI_X9_42_NAMED_SCHEME, 2 },
	OID_ANSI_DH_ONE_FLOW_SHA1[] = { OID_ANSI_X9_42_NAMED_SCHEME, 3 },
	OID_ANSI_DH_HYBRID1_SHA1[] 	= { OID_ANSI_X9_42_NAMED_SCHEME, 4 },
	OID_ANSI_DH_HYBRID2_SHA1[] 	= { OID_ANSI_X9_42_NAMED_SCHEME, 5 },
	OID_ANSI_DH_HYBRID_ONEFLOW_SHA1[] 	= { OID_ANSI_X9_42_NAMED_SCHEME, 6 },
	/* sic - enumerated in reverse order in the spec */
	OID_ANSI_MQV1_SHA1[] 		= { OID_ANSI_X9_42_NAMED_SCHEME, 8 },
	OID_ANSI_MQV2_SHA1[] 		= { OID_ANSI_X9_42_NAMED_SCHEME, 7 };
	
const CSSM_OID
	CSSMOID_MD2     		= {OID_RSA_HASH_LENGTH+1, (uint8 *)OID_MD2},
	CSSMOID_MD4     		= {OID_RSA_HASH_LENGTH+1, (uint8 *)OID_MD4},
	CSSMOID_MD5     		= {OID_RSA_HASH_LENGTH+1, (uint8 *)OID_MD5},
	CSSMOID_RSA     		= {OID_PKCS_1_LENGTH+1, (uint8 *)OID_RSAEncryption},
	CSSMOID_MD2WithRSA  	= {OID_PKCS_1_LENGTH+1, (uint8 *)OID_MD2WithRSA},
	CSSMOID_MD4WithRSA  	= {OID_PKCS_1_LENGTH+1, (uint8 *)OID_MD4WithRSA},
	CSSMOID_MD5WithRSA  	= {OID_PKCS_1_LENGTH+1, (uint8 *)OID_MD5WithRSA},
	CSSMOID_SHA1WithRSA 	= {OID_PKCS_1_LENGTH+1, (uint8 *)OID_SHA1WithRSA},
	CSSMOID_PKCS3			= {OID_PKCS_3_LENGTH,   (uint8 *)OID_PKCS_3_ARC},
	CSSMOID_DH      		= {OID_PKCS_3_LENGTH+1, (uint8 *)OID_DHKeyAgreement},
	CSSMOID_DSA     		= {OID_OIW_ALGORITHM_LENGTH+1, (uint8 *)OID_OIW_DSA},
	CSSMOID_DSA_CMS			= { 7, (uint8 *)OID_CMS_DSA},
	CSSMOID_DSA_JDK			= { 5, (uint8 *)OID_JDK_DSA},
	CSSMOID_SHA1WithDSA 	= {OID_OIW_ALGORITHM_LENGTH+1, (uint8 *)OID_OIW_DSAWithSHA1},
	CSSMOID_SHA1WithDSA_CMS = { 7, (uint8 *)OID_CMS_DSAWithSHA1},
	CSSMOID_SHA1WithDSA_JDK = { 5, (uint8 *)OID_JDK_DSAWithSHA1},
	CSSMOID_SHA1			= {OID_OIW_ALGORITHM_LENGTH+1, (uint8 *)OID_OIW_SHA1},
	CSSMOID_SHA1WithRSA_OIW = {OID_OIW_ALGORITHM_LENGTH+1, (uint8 *)OID_OIW_RSAWithSHA1},
	CSSMOID_ANSI_DH_PUB_NUMBER = {OID_ANSI_X9_42_LEN + 1, (uint8 *)OID_ANSI_DH_PUB_NUMBER},
	CSSMOID_ANSI_DH_STATIC 	   = {OID_ANSI_X9_42_SCHEME_LEN + 1, (uint8 *)OID_ANSI_DH_STATIC},
	CSSMOID_ANSI_DH_ONE_FLOW   = {OID_ANSI_X9_42_SCHEME_LEN + 1, (uint8 *)OID_ANSI_DH_ONE_FLOW},
	CSSMOID_ANSI_DH_EPHEM 	   = {OID_ANSI_X9_42_SCHEME_LEN + 1, (uint8 *)OID_ANSI_DH_EPHEM},
	CSSMOID_ANSI_DH_HYBRID1	   = {OID_ANSI_X9_42_SCHEME_LEN + 1, (uint8 *)OID_ANSI_DH_HYBRID1},
	CSSMOID_ANSI_DH_HYBRID2	   = {OID_ANSI_X9_42_SCHEME_LEN + 1, (uint8 *)OID_ANSI_DH_HYBRID2},
	CSSMOID_ANSI_DH_HYBRID_ONEFLOW = {OID_ANSI_X9_42_SCHEME_LEN + 1, 
									  (uint8 *)OID_ANSI_DH_HYBRID_ONEFLOW},
	CSSMOID_ANSI_DH_MQV1 	   = {OID_ANSI_X9_42_SCHEME_LEN + 1, (uint8 *)OID_ANSI_MQV1},
	CSSMOID_ANSI_DH_MQV2 	   = {OID_ANSI_X9_42_SCHEME_LEN + 1, (uint8 *)OID_ANSI_MQV2},
	CSSMOID_ANSI_DH_STATIC_SHA1 	= {OID_ANSI_X9_42_NAMED_SCHEME_LEN + 1, 
										(uint8 *)OID_ANSI_DH_STATIC_SHA1},
	CSSMOID_ANSI_DH_ONE_FLOW_SHA1   = {OID_ANSI_X9_42_NAMED_SCHEME_LEN + 1, 
										(uint8 *)OID_ANSI_DH_ONE_FLOW_SHA1},
	CSSMOID_ANSI_DH_EPHEM_SHA1 	   	= {OID_ANSI_X9_42_NAMED_SCHEME_LEN + 1, 
										(uint8 *)OID_ANSI_DH_EPHEM_SHA1},
	CSSMOID_ANSI_DH_HYBRID1_SHA1	= {OID_ANSI_X9_42_NAMED_SCHEME_LEN + 1, 
										(uint8 *)OID_ANSI_DH_HYBRID1_SHA1},
	CSSMOID_ANSI_DH_HYBRID2_SHA1	= {OID_ANSI_X9_42_NAMED_SCHEME_LEN + 1, 
										(uint8 *)OID_ANSI_DH_HYBRID2_SHA1},
	CSSMOID_ANSI_DH_HYBRID_ONEFLOW_SHA1 = {OID_ANSI_X9_42_NAMED_SCHEME_LEN + 1, 
										(uint8 *)OID_ANSI_DH_HYBRID_ONEFLOW_SHA1},
	CSSMOID_ANSI_MQV1_SHA1 	 	  	= {OID_ANSI_X9_42_NAMED_SCHEME_LEN + 1, 
										(uint8 *)OID_ANSI_MQV1_SHA1},
	CSSMOID_ANSI_MQV2_SHA1 	 	  	= {OID_ANSI_X9_42_NAMED_SCHEME_LEN + 1, 
										(uint8 *)OID_ANSI_MQV2_SHA1};

	
/*	iSignTP OBJECT IDENTIFIER ::=
 *		{ appleTrustPolicy 1 }
 *      { 1 2 840 113635 100 1 1 }
 *
 * BER =  06 09 2A 86 48 86 F7 63 64 01 01
 */
static const uint8
APPLE_TP_ISIGN[]		= {APPLE_TP_OID, 1},

/*	AppleX509Basic OBJECT IDENTIFIER ::=
 *		{ appleTrustPolicy 2 }
 *      { 1 2 840 113635 100 1 2 }
 *
 * BER =  06 09 2A 86 48 86 F7 63 64 01 01
 */
APPLE_TP_X509_BASIC[]	= {APPLE_TP_OID, 2},

/* AppleSSLPolicy := {appleTrustPolicy 3 } */
APPLE_TP_SSL[]			= {APPLE_TP_OID, 3},

/* AppleLocalCertGenPolicy := {appleTrustPolicy 4 } */
APPLE_TP_LOCAL_CERT_GEN[]	= {APPLE_TP_OID, 4},

/* AppleCSRGenPolicy := {appleTrustPolicy 5 } */
APPLE_TP_CSR_GEN[]			= {APPLE_TP_OID, 5},

/* Apple CRL-based revocation policy := {appleTrustPolicy 6 } */
APPLE_TP_REVOCATION_CRL[]	= {APPLE_TP_OID, 6},

/* Apple OCSP-based revocation policy := {appleTrustPolicy 7 } */
APPLE_TP_REVOCATION_OCSP[]	= {APPLE_TP_OID, 7},

/* Apple S/MIME trust policy := {appleTrustPolicy 8 } */
APPLE_TP_SMIME[]			= {APPLE_TP_OID, 8},

/* Apple EAP trust policy := {appleTrustPolicy 9 } */
APPLE_TP_EAP[]				= {APPLE_TP_OID, 9},

/*
 *	fee OBJECT IDENTIFIER ::=
 *		{ appleSecurityAlgorithm 1 }
 *      { 1 2 840 113635 100 2 1 }
 *
 * BER = 06 09 2A 86 48 86 F7 63 64 02 01
 */
APPLE_FEE[]			= {APPLE_ALG_OID, 1},

/*
 *	asc OBJECT IDENTIFIER ::=
 *		{ appleSecurityAlgorithm 2 }
 *      { 1 2 840 113635 100 2 2 }
 *
 * BER = 06 09 2A 86 48 86 F7 63 64 02 02
 */
APPLE_ASC[]			= {APPLE_ALG_OID, 2},

/*
 *	fee_MD5 OBJECT IDENTIFIER ::=
 *		{ appleSecurityAlgorithm 3 }
 *      { 1 2 840 113635 100 2 3 }
 *
 * BER = 06 09 2A 86 48 86 F7 63 64 02 03
 */
APPLE_FEE_MD5[]		= {APPLE_ALG_OID, 3},

/*
 *	fee_SHA1 OBJECT IDENTIFIER ::=
 *		{ appleSecurityAlgorithm 4 }
 *      { 1 2 840 113635 100 2 4 }
 *
 * BER = 06 09 2A 86 48 86 F7 63 64 02 04
 */
APPLE_FEE_SHA1[]	= {APPLE_ALG_OID, 4},

/*
 *	feed OBJECT IDENTIFIER ::=
 *		{ appleSecurityAlgorithm 5 }
 *      { 1 2 840 113635 100 2 5 }
 *
 * BER = 06 09 2A 86 48 86 F7 63 64 02 05
 */
APPLE_FEED[]		= {APPLE_ALG_OID, 5},

/*
 *	feedExp OBJECT IDENTIFIER ::=
 *		{ appleSecurityAlgorithm 6 }
 *      { 1 2 840 113635 100 2 6 }
 *
 * BER = 06 09 2A 86 48 86 F7 63 64 02 06
 */
APPLE_FEEDEXP[]		= {APPLE_ALG_OID, 6},

/*
 *	AppleECDSA OBJECT IDENTIFIER ::=
 *		{ appleSecurityAlgorithm 7 }
 *      { 1 2 840 113635 100 2 7 }
 *
 * BER = 06 09 2A 86 48 86 F7 63 64 02 07
 */
APPLE_ECDSA[]		= {APPLE_ALG_OID, 7};

const CSSM_OID	

CSSMOID_APPLE_ISIGN      = {APPLE_TP_OID_LENGTH+1,  (uint8 *)APPLE_TP_ISIGN},
CSSMOID_APPLE_X509_BASIC = {APPLE_TP_OID_LENGTH+1,  (uint8 *)APPLE_TP_X509_BASIC},
CSSMOID_APPLE_TP_SSL	 = {APPLE_TP_OID_LENGTH+1,  (uint8 *)APPLE_TP_SSL},
CSSMOID_APPLE_TP_LOCAL_CERT_GEN	= 
						   {APPLE_TP_OID_LENGTH+1,  (uint8 *)APPLE_TP_LOCAL_CERT_GEN},
CSSMOID_APPLE_TP_CSR_GEN = {APPLE_TP_OID_LENGTH+1,  (uint8 *)APPLE_TP_CSR_GEN},
CSSMOID_APPLE_TP_REVOCATION_CRL = 
						   {APPLE_TP_OID_LENGTH+1,  (uint8 *)APPLE_TP_REVOCATION_CRL},
CSSMOID_APPLE_TP_REVOCATION_OCSP = 
						   {APPLE_TP_OID_LENGTH+1,  (uint8 *)APPLE_TP_REVOCATION_OCSP},
CSSMOID_APPLE_TP_SMIME	 = {APPLE_TP_OID_LENGTH+1,  (uint8 *)APPLE_TP_SMIME},
CSSMOID_APPLE_TP_EAP	 = {APPLE_TP_OID_LENGTH+1,  (uint8 *)APPLE_TP_EAP},
CSSMOID_APPLE_FEE        = {APPLE_ALG_OID_LENGTH+1, (uint8 *)APPLE_FEE},
CSSMOID_APPLE_ASC        = {APPLE_ALG_OID_LENGTH+1, (uint8 *)APPLE_ASC},
CSSMOID_APPLE_FEE_MD5    = {APPLE_ALG_OID_LENGTH+1, (uint8 *)APPLE_FEE_MD5},
CSSMOID_APPLE_FEE_SHA1   = {APPLE_ALG_OID_LENGTH+1, (uint8 *)APPLE_FEE_SHA1},
CSSMOID_APPLE_FEED       = {APPLE_ALG_OID_LENGTH+1, (uint8 *)APPLE_FEED},
CSSMOID_APPLE_FEEDEXP    = {APPLE_ALG_OID_LENGTH+1, (uint8 *)APPLE_FEEDEXP},
CSSMOID_APPLE_ECDSA      = {APPLE_ALG_OID_LENGTH+1, (uint8 *)APPLE_ECDSA};

/* PKCS12 algorithms */
#define OID_PKCS12_PbeIds 			OID_PKCS_12,1
#define OID_PKCS12_PbeIds_Length	OID_PKCS_12_LENGTH+1

static const uint8
	OID_PKCS12_pbeWithSHAAnd128BitRC4[] = 	{ OID_PKCS12_PbeIds, 1 },
	OID_PKCS12_pbeWithSHAAnd40BitRC4[] = 	{ OID_PKCS12_PbeIds, 2 },
	OID_PKCS12_pbeWithSHAAnd3Key3DESCBC[] = { OID_PKCS12_PbeIds, 3 },
	OID_PKCS12_pbeWithSHAAnd2Key3DESCBC[] =	{ OID_PKCS12_PbeIds, 4 },
	OID_PKCS12_pbeWithSHAAnd128BitRC2CBC[] ={ OID_PKCS12_PbeIds, 5 },
	OID_PKCS12_pbewithSHAAnd40BitRC2CBC[] = { OID_PKCS12_PbeIds, 6 };
	

const CSSM_OID	
CSSMOID_PKCS12_pbeWithSHAAnd128BitRC4 = {OID_PKCS12_PbeIds_Length + 1,
					(uint8 *)OID_PKCS12_pbeWithSHAAnd128BitRC4 },
CSSMOID_PKCS12_pbeWithSHAAnd40BitRC4 = {OID_PKCS12_PbeIds_Length + 1,
					(uint8 *)OID_PKCS12_pbeWithSHAAnd40BitRC4 },
CSSMOID_PKCS12_pbeWithSHAAnd3Key3DESCBC = {OID_PKCS12_PbeIds_Length + 1,
					(uint8 *)OID_PKCS12_pbeWithSHAAnd3Key3DESCBC },
CSSMOID_PKCS12_pbeWithSHAAnd2Key3DESCBC = {OID_PKCS12_PbeIds_Length + 1,
					(uint8 *)OID_PKCS12_pbeWithSHAAnd2Key3DESCBC },
CSSMOID_PKCS12_pbeWithSHAAnd128BitRC2CBC = {OID_PKCS12_PbeIds_Length + 1,
					(uint8 *)OID_PKCS12_pbeWithSHAAnd128BitRC2CBC },
CSSMOID_PKCS12_pbewithSHAAnd40BitRC2CBC = {OID_PKCS12_PbeIds_Length + 1,
					(uint8 *)OID_PKCS12_pbewithSHAAnd40BitRC2CBC };
					

#pragma mark ----- CSSM_OID <--> CSSM_ALGORITHMS -----

typedef struct {
	const CSSM_OID 	*oid;
	CSSM_ALGORITHMS	alg;
} OidToAlgEnt;

static const OidToAlgEnt oidToAlgMap[] = 
{
	{&CSSMOID_RSA, CSSM_ALGID_RSA },
	{&CSSMOID_MD2WithRSA, CSSM_ALGID_MD2WithRSA },
	{&CSSMOID_MD5WithRSA, CSSM_ALGID_MD5WithRSA },
	{&CSSMOID_SHA1WithRSA, CSSM_ALGID_SHA1WithRSA },
	{&CSSMOID_SHA1WithRSA_OIW, CSSM_ALGID_SHA1WithRSA },
	{&CSSMOID_SHA1, CSSM_ALGID_SHA1},
	{&CSSMOID_MD5, CSSM_ALGID_MD5 },
	/* 
	 * These OIDs have three variants - one for BSAFE, CMS, and JDK 1.1.
	 * On the oid-to-alg map, we'll handle either one, mapping to 
	 * the same CSSM alg. When we map from alg to OID, we'll use
	 * the CMS variant (being first in the list).
	 */
	{&CSSMOID_DSA_CMS, CSSM_ALGID_DSA },
	{&CSSMOID_DSA, CSSM_ALGID_DSA },
	{&CSSMOID_DSA_JDK, CSSM_ALGID_DSA },
	{&CSSMOID_SHA1WithDSA_CMS, CSSM_ALGID_SHA1WithDSA },
	{&CSSMOID_SHA1WithDSA, CSSM_ALGID_SHA1WithDSA },
	{&CSSMOID_SHA1WithDSA_JDK, CSSM_ALGID_SHA1WithDSA },
	/*
	 * Multiple entries for Diffie-Hellman. We favor the PKCS3 version for
	 * mapping alg to OID.
	 */
	{&CSSMOID_DH, CSSM_ALGID_DH},
	{&CSSMOID_ANSI_DH_PUB_NUMBER, CSSM_ALGID_DH},
	{&CSSMOID_ANSI_DH_STATIC, CSSM_ALGID_DH},
	{&CSSMOID_ANSI_DH_ONE_FLOW, CSSM_ALGID_DH},
	{&CSSMOID_ANSI_DH_EPHEM, CSSM_ALGID_DH},
	{&CSSMOID_ANSI_DH_HYBRID1, CSSM_ALGID_DH},
	{&CSSMOID_ANSI_DH_HYBRID2, CSSM_ALGID_DH},
	{&CSSMOID_ANSI_DH_HYBRID_ONEFLOW, CSSM_ALGID_DH},
	{&CSSMOID_APPLE_FEE, CSSM_ALGID_FEE },
	{&CSSMOID_APPLE_ASC, CSSM_ALGID_ASC },
	{&CSSMOID_APPLE_FEE_MD5, CSSM_ALGID_FEE_MD5 },
	{&CSSMOID_APPLE_FEE_SHA1, CSSM_ALGID_FEE_SHA1 },
	{&CSSMOID_APPLE_FEED, CSSM_ALGID_FEED },
	{&CSSMOID_APPLE_FEEDEXP, CSSM_ALGID_FEEDEXP },
	{&CSSMOID_APPLE_ECDSA, CSSM_ALGID_SHA1WithECDSA },
	{NULL, 0}
};

#define NUM_OID_TO_ALGS	(sizeof(oidToAlgMap) / sizeof(oidToAlgMap[0]))

/*
 * Compare two CSSM_DATAs (or two CSSM_OIDs), return true if identical.
 */
static bool compareCssmData(
	const CSSM_DATA *data1,
	const CSSM_DATA *data2)
{	
	if((data1 == NULL) || (data1->Data == NULL) || 
	   (data2 == NULL) || (data2->Data == NULL) ||
	   (data1->Length != data2->Length)) {
		return false;
	}
	if(data1->Length != data2->Length) {
		return false;
	}
	if(memcmp(data1->Data, data2->Data, data1->Length) == 0) {
		return true;
	}
	else {
		return false;
	}
}

bool cssmOidToAlg(
	const CSSM_OID *oid,
	CSSM_ALGORITHMS *alg)		// RETURNED
{
	const OidToAlgEnt *ent;
	
	for(ent=oidToAlgMap; ent->oid; ent++) {
		if(compareCssmData(ent->oid, oid)) {
			*alg = ent->alg;
			return true;
		}
	}
	return false;
}

const CSSM_OID *cssmAlgToOid(
	CSSM_ALGORITHMS algId)
{
	const OidToAlgEnt *ent;
	
	for(ent=oidToAlgMap; ent->oid; ent++) {
		if(ent->alg == algId) {
			return ent->oid;
		}
	}
	return NULL;
}



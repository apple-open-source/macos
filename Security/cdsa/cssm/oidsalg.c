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

static const uint8
	OID_MD2[]   	       	= { OID_RSA_HASH, 2 },
	OID_MD4[]       	   	= { OID_RSA_HASH, 4 },
	OID_MD5[]          		= { OID_RSA_HASH, 5 },
	OID_RSAEncryption[]		= { OID_PKCS_1, 1 },	
	OID_MD2WithRSA[]   		= { OID_PKCS_1, 2 },
	OID_MD4WithRSA[]   		= { OID_PKCS_1, 3 },
	OID_MD5WithRSA[]   		= { OID_PKCS_1, 4 },
	OID_SHA1WithRSA[]  		= { OID_PKCS_1, 5 },
	OID_DHKeyAgreement[]    = { OID_PKCS_3, 1 },
	OID_OIW_DSA[]     		= { OID_OIW_ALGORITHM, 12  },  
	OID_OIW_DSAWithSHA1[] 	= { OID_OIW_ALGORITHM, 27  },
	OID_OIW_SHA1[]			= { OID_OIW_ALGORITHM, 26  };
	
const CSSM_OID
	CSSMOID_MD2     		= {OID_RSA_HASH_LENGTH+1, (uint8 *)OID_MD2},
	CSSMOID_MD4     		= {OID_RSA_HASH_LENGTH+1, (uint8 *)OID_MD4},
	CSSMOID_MD5     		= {OID_RSA_HASH_LENGTH+1, (uint8 *)OID_MD5},
	CSSMOID_RSA     		= {OID_PKCS_1_LENGTH+1, (uint8 *)OID_RSAEncryption},
	CSSMOID_MD2WithRSA  	= {OID_PKCS_1_LENGTH+1, (uint8 *)OID_MD2WithRSA},
	CSSMOID_MD4WithRSA  	= {OID_PKCS_1_LENGTH+1, (uint8 *)OID_MD4WithRSA},
	CSSMOID_MD5WithRSA  	= {OID_PKCS_1_LENGTH+1, (uint8 *)OID_MD5WithRSA},
	CSSMOID_SHA1WithRSA 	= {OID_PKCS_1_LENGTH+1, (uint8 *)OID_SHA1WithRSA},
	CSSMOID_DH      		= {OID_PKCS_3_LENGTH+1, (uint8 *)OID_DHKeyAgreement},
	CSSMOID_DSA     		= {OID_OIW_ALGORITHM_LENGTH+1, (uint8 *)OID_OIW_DSA},
	CSSMOID_SHA1WithDSA 	= {OID_OIW_ALGORITHM_LENGTH+1, (uint8 *)OID_OIW_DSAWithSHA1},
	CSSMOID_SHA1			= {OID_OIW_ALGORITHM_LENGTH+1, (uint8 *)OID_OIW_SHA1};

	
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
CSSMOID_APPLE_FEE        = {APPLE_ALG_OID_LENGTH+1, (uint8 *)APPLE_FEE},
CSSMOID_APPLE_ASC        = {APPLE_ALG_OID_LENGTH+1, (uint8 *)APPLE_ASC},
CSSMOID_APPLE_FEE_MD5    = {APPLE_ALG_OID_LENGTH+1, (uint8 *)APPLE_FEE_MD5},
CSSMOID_APPLE_FEE_SHA1   = {APPLE_ALG_OID_LENGTH+1, (uint8 *)APPLE_FEE_SHA1},
CSSMOID_APPLE_FEED       = {APPLE_ALG_OID_LENGTH+1, (uint8 *)APPLE_FEED},
CSSMOID_APPLE_FEEDEXP    = {APPLE_ALG_OID_LENGTH+1, (uint8 *)APPLE_FEEDEXP},
CSSMOID_APPLE_ECDSA      = {APPLE_ALG_OID_LENGTH+1, (uint8 *)APPLE_ECDSA};



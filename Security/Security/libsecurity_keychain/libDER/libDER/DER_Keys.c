/*
 * Copyright (c) 2005-2007,2011,2014 Apple Inc. All Rights Reserved.
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
 * DER_Cert.c - support for decoding RSA keys
 *
 */
 
#include <libDER/DER_Decode.h>
#include <libDER/DER_Encode.h>
#include <libDER/DER_Keys.h>
#include <libDER/asn1Types.h>
#include <libDER/libDER_config.h>

#ifndef	DER_DECODE_ENABLE
#error Please define DER_DECODE_ENABLE.
#endif
#if		DER_DECODE_ENABLE

/* 
 * DERItemSpecs for decoding RSA keys. 
 */
 
/* Algorithm Identifier */
const DERItemSpec DERAlgorithmIdItemSpecs[] = 
{
	{ DER_OFFSET(DERAlgorithmId, oid),
			ASN1_OBJECT_ID,
			DER_DEC_NO_OPTS },
	{ DER_OFFSET(DERAlgorithmId, params),
			0,				/* no tag - any */
			DER_DEC_ASN_ANY | DER_DEC_OPTIONAL | DER_DEC_SAVE_DER }
};
const DERSize DERNumAlgorithmIdItemSpecs = 
	sizeof(DERAlgorithmIdItemSpecs) / sizeof(DERItemSpec);

/* X509 SubjectPublicKeyInfo */
const DERItemSpec DERSubjPubKeyInfoItemSpecs[] = 
{
	{ DER_OFFSET(DERSubjPubKeyInfo, algId),
			ASN1_CONSTR_SEQUENCE,	
			DER_DEC_NO_OPTS },		
	{ DER_OFFSET(DERSubjPubKeyInfo, pubKey),
			ASN1_BIT_STRING,	
			DER_DEC_NO_OPTS },		

};
const DERSize DERNumSubjPubKeyInfoItemSpecs = 
	sizeof(DERSubjPubKeyInfoItemSpecs) / sizeof(DERItemSpec);

/* 
 * RSA private key in CRT format
 */
const DERItemSpec DERRSAPrivKeyCRTItemSpecs[] = 
{
	/* version, n, e, d - skip */
	{ 0,
			ASN1_INTEGER,
			DER_DEC_SKIP },
	{ 0,
			ASN1_INTEGER,
			DER_DEC_SKIP },
	{ 0,
			ASN1_INTEGER,
			DER_DEC_SKIP },
	{ 0,
			ASN1_INTEGER,
			DER_DEC_SKIP },
	{ DER_OFFSET(DERRSAPrivKeyCRT, p),
			ASN1_INTEGER,	
			DER_DEC_NO_OPTS },		
	{ DER_OFFSET(DERRSAPrivKeyCRT, q),
			ASN1_INTEGER,	
			DER_DEC_NO_OPTS },		
	{ DER_OFFSET(DERRSAPrivKeyCRT, dp),
			ASN1_INTEGER,	
			DER_DEC_NO_OPTS },		
	{ DER_OFFSET(DERRSAPrivKeyCRT, dq),
			ASN1_INTEGER,	
			DER_DEC_NO_OPTS },		
	{ DER_OFFSET(DERRSAPrivKeyCRT, qInv),
			ASN1_INTEGER,	
			DER_DEC_NO_OPTS },		
	/* ignore the (optional) rest */
};
const DERSize DERNumRSAPrivKeyCRTItemSpecs = 
	sizeof(DERRSAPrivKeyCRTItemSpecs) / sizeof(DERItemSpec);

#endif	/* DER_DECODE_ENABLE */

#if		DER_DECODE_ENABLE || DER_ENCODE_ENABLE

/* RSA public key in PKCS1 format - encode and decode */
const DERItemSpec DERRSAPubKeyPKCS1ItemSpecs[] = 
{
	{ DER_OFFSET(DERRSAPubKeyPKCS1, modulus),
			ASN1_INTEGER,	
			DER_DEC_NO_OPTS | DER_ENC_SIGNED_INT },		
	{ DER_OFFSET(DERRSAPubKeyPKCS1, pubExponent),
			ASN1_INTEGER,	
			DER_DEC_NO_OPTS | DER_ENC_SIGNED_INT },		
};
const DERSize DERNumRSAPubKeyPKCS1ItemSpecs = 
	sizeof(DERRSAPubKeyPKCS1ItemSpecs) / sizeof(DERItemSpec);

/* RSA public key in Apple custome format with reciprocal - encode and decode */
const DERItemSpec DERRSAPubKeyAppleItemSpecs[] = 
{
	{ DER_OFFSET(DERRSAPubKeyApple, modulus),
			ASN1_INTEGER,	
			DER_DEC_NO_OPTS | DER_ENC_SIGNED_INT },		
	{ DER_OFFSET(DERRSAPubKeyApple, reciprocal),
			ASN1_INTEGER,	
			DER_DEC_NO_OPTS | DER_ENC_SIGNED_INT },		
	{ DER_OFFSET(DERRSAPubKeyApple, pubExponent),
			ASN1_INTEGER,	
			DER_DEC_NO_OPTS | DER_ENC_SIGNED_INT },		
};
const DERSize DERNumRSAPubKeyAppleItemSpecs = 
	sizeof(DERRSAPubKeyAppleItemSpecs) / sizeof(DERItemSpec);


#endif		/* DER_DECODE_ENABLE || DER_ENCODE_ENABLE */

#ifndef	DER_ENCODE_ENABLE
#error Please define DER_ENCODE_ENABLE.
#endif

#if		DER_ENCODE_ENABLE

/* RSA Key Pair, encode only */
const DERItemSpec DERRSAKeyPairItemSpecs[] = 
{
	{ DER_OFFSET(DERRSAKeyPair, version),
			ASN1_INTEGER,	
			DER_ENC_SIGNED_INT },		
	{ DER_OFFSET(DERRSAKeyPair, n),
			ASN1_INTEGER,	
			DER_ENC_SIGNED_INT },		
	{ DER_OFFSET(DERRSAKeyPair, e),
			ASN1_INTEGER,	
			DER_ENC_SIGNED_INT },		
	{ DER_OFFSET(DERRSAKeyPair, d),
			ASN1_INTEGER,	
			DER_ENC_SIGNED_INT },		
	{ DER_OFFSET(DERRSAKeyPair, p),
			ASN1_INTEGER,	
			DER_ENC_SIGNED_INT },		
	{ DER_OFFSET(DERRSAKeyPair, q),
			ASN1_INTEGER,	
			DER_ENC_SIGNED_INT },		
	{ DER_OFFSET(DERRSAKeyPair, dp),
			ASN1_INTEGER,	
			DER_ENC_SIGNED_INT },		
	{ DER_OFFSET(DERRSAKeyPair, dq),
			ASN1_INTEGER,	
			DER_ENC_SIGNED_INT },		
	{ DER_OFFSET(DERRSAKeyPair, qInv),
			ASN1_INTEGER,	
			DER_ENC_SIGNED_INT },		
};

const DERSize DERNumRSAKeyPairItemSpecs = 
	sizeof(DERRSAKeyPairItemSpecs) / sizeof(DERItemSpec);

#endif	/* DER_ENCODE_ENABLE */


/* Copyright (c) 1997,2003-2006,2008,2010,2013 Apple Inc.
 *
 * cspwrap.c - wrappers to simplify access to CDSA
 *
 * Revision History
 * ----------------
 *   3 May 2000 Doug Mitchell
 *		Ported to X/CDSA2.
 *  12 Aug 1997	Doug Mitchell at Apple
 *		Created.
 */
 
#include <Security/cssmapple.h>
#include <Security/cssm.h>
#include "cspwrap.h"
#include "common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* MCF hack */
// #include <CarbonCore/MacTypes.h>
#include <MacTypes.h>
/* end MCF */

#ifndef	NULL
#define NULL ((void *)0)
#endif	/* NULL */
#ifndef	MAX
#define MAX(a,b)	((a > b) ? a : b)
#define MIN(a,b)	((a < b) ? a : b)
#endif

#pragma mark --------- Key Generation ---------

/*
 * Key generation
 */
#define FEE_PRIV_DATA_SIZE	20
/*
 * Debug/test only. BsafeCSP only (long since disabled, in Puma).
 * This results in quicker but less secure RSA key generation.
 */
#define RSA_WEAK_KEYS		0

/*
 * Force bad data in KeyData prior to generating, deriving, or
 * wrapping key to ensure that the CSP ignores incoming
 * KeyData.
 */
static void setBadKeyData(
	CSSM_KEY_PTR key)
{
	key->KeyData.Data = (uint8 *)0xeaaaeaaa;	// bad ptr
	key->KeyData.Length = 1;	// no key can fit here
}

/*
 * Generate key pair of arbitrary algorithm. 
 * FEE keys will have random private data.
 */
CSSM_RETURN cspGenKeyPair(CSSM_CSP_HANDLE cspHand,
	uint32 algorithm,
	const char *keyLabel,
	unsigned keyLabelLen,
	uint32 keySize,					// in bits
	CSSM_KEY_PTR pubKey,			// mallocd by caller
	CSSM_BOOL pubIsRef,				// true - reference key, false - data
	uint32 pubKeyUsage,				// CSSM_KEYUSE_ENCRYPT, etc.
	CSSM_KEYBLOB_FORMAT pubFormat,	// Optional. Specify 0 or CSSM_KEYBLOB_RAW_FORMAT_NONE
									//   to get the default format. 
	CSSM_KEY_PTR privKey,			// mallocd by caller
	CSSM_BOOL privIsRef,			// true - reference key, false - data
	uint32 privKeyUsage,			// CSSM_KEYUSE_DECRYPT, etc.
	CSSM_KEYBLOB_FORMAT privFormat,	// optional 0 ==> default
	CSSM_BOOL genSeed)				// FEE only. True: we generate seed and CSP
									// will hash it. False: CSP generates random 
									// seed. 
{
	CSSM_RETURN				crtn;
	CSSM_CC_HANDLE 			ccHand;
	CSSM_DATA				privData = {0, NULL};		// mallocd for FEE
	CSSM_CRYPTO_DATA		privCData;
	CSSM_CRYPTO_DATA_PTR	privCDataPtr = NULL;
	CSSM_DATA				keyLabelData;
	uint32					pubAttr;
	uint32					privAttr;
	CSSM_RETURN 			ocrtn = CSSM_OK;
	
	if(keySize == CSP_KEY_SIZE_DEFAULT) {
		keySize = cspDefaultKeySize(algorithm);
	}
	
	/* pre-context-create algorithm-specific stuff */
	switch(algorithm) {
		case CSSM_ALGID_FEE:
			if(genSeed) {
				/* cook up random privData */
				privData.Data = (uint8 *)CSSM_MALLOC(FEE_PRIV_DATA_SIZE);
				privData.Length = FEE_PRIV_DATA_SIZE;
				appGetRandomBytes(privData.Data, FEE_PRIV_DATA_SIZE);
				privCData.Param = privData;
				privCData.Callback = NULL;
				privCDataPtr = &privCData;
			}
			/* else CSP generates random seed/key */
			break;
		case CSSM_ALGID_RSA:
			break;
		case CSSM_ALGID_DSA:
			break;
		case CSSM_ALGID_ECDSA:
			break;
		default:
			printf("cspGenKeyPair: Unknown algorithm\n");
			/* but what the hey */
			privCDataPtr = NULL;
			break;
	}
	keyLabelData.Data        = (uint8 *)keyLabel,
	keyLabelData.Length      = keyLabelLen;
	memset(pubKey, 0, sizeof(CSSM_KEY));
	memset(privKey, 0, sizeof(CSSM_KEY));
	setBadKeyData(pubKey);
	setBadKeyData(privKey);
	
	crtn = CSSM_CSP_CreateKeyGenContext(cspHand,
		algorithm,
		keySize,
		privCDataPtr,			// Seed
		NULL,					// Salt
		NULL,					// StartDate
		NULL,					// EndDate
		NULL,					// Params
		&ccHand);
	if(crtn) {
		printError("CSSM_CSP_CreateKeyGenContext", crtn);
		ocrtn = crtn;
		goto abort;
	}
	/* cook up attribute bits */
	if(pubIsRef) {
		pubAttr = CSSM_KEYATTR_RETURN_REF | CSSM_KEYATTR_EXTRACTABLE;
	}
	else {
		pubAttr = CSSM_KEYATTR_RETURN_DATA | CSSM_KEYATTR_EXTRACTABLE;
	}
	if(privIsRef) {
		privAttr = CSSM_KEYATTR_RETURN_REF | CSSM_KEYATTR_EXTRACTABLE;
	}
	else {
		privAttr = CSSM_KEYATTR_RETURN_DATA | CSSM_KEYATTR_EXTRACTABLE;
	}

	/* post-context-create algorithm-specific stuff */
	switch(algorithm) {
		case CSSM_ALGID_RSA:
		
			#if	RSA_WEAK_KEYS
			{
				/* for testing, speed up key gen by using the
				* undocumented "CUSTOM" key gen mode. This
				* results in the CSP using AI_RsaKeyGen instead of
				* AI_RSAStrongKeyGen.
				*/
				crtn = AddContextAttribute(ccHand,
					CSSM_ATTRIBUTE_MODE,
					sizeof(uint32),		
					CAT_Uint32,
					NULL,
					CSSM_ALGMODE_CUSTOM);
				if(crtn) {
					printError("CSSM_UpdateContextAttributes", crtn);
					return crtn;
				}
			}
			#endif	// RSA_WEAK_KEYS
			break;
		 
		 case CSSM_ALGID_DSA:
			/* 
			 * extra step - generate params - this just adds some
			 * info to the context
			 */
			{
				CSSM_DATA dummy = {0, NULL};
				crtn = CSSM_GenerateAlgorithmParams(ccHand, 
					keySize, &dummy);
				if(crtn) {
					printError("CSSM_GenerateAlgorithmParams", crtn);
					return crtn;
				}
				appFreeCssmData(&dummy, CSSM_FALSE);
			}
			break;
		default:
			break;
	}
	
	/* optional format specifiers */
	if(!pubIsRef && (pubFormat != CSSM_KEYBLOB_RAW_FORMAT_NONE)) {
		crtn = AddContextAttribute(ccHand,
			CSSM_ATTRIBUTE_PUBLIC_KEY_FORMAT,
			sizeof(uint32),	
			CAT_Uint32,
			NULL,
			pubFormat);
		if(crtn) {
			printError("AddContextAttribute(CSSM_ATTRIBUTE_PUBLIC_KEY_FORMAT)", crtn);
			return crtn;
		}
	}
	if(!privIsRef && (privFormat != CSSM_KEYBLOB_RAW_FORMAT_NONE)) {
		crtn = AddContextAttribute(ccHand,
			CSSM_ATTRIBUTE_PRIVATE_KEY_FORMAT,
			sizeof(uint32),			// currently sizeof CSSM_DATA
			CAT_Uint32,
			NULL,
			privFormat);
		if(crtn) {
			printError("AddContextAttribute(CSSM_ATTRIBUTE_PRIVATE_KEY_FORMAT)", crtn);
			return crtn;
		}
	}
	crtn = CSSM_GenerateKeyPair(ccHand,
		pubKeyUsage,
		pubAttr,
		&keyLabelData,
		pubKey,
		privKeyUsage,
		privAttr,
		&keyLabelData,			// same labels
		NULL,					// CredAndAclEntry
		privKey);
	if(crtn) {
		printError("CSSM_GenerateKeyPair", crtn);
		ocrtn = crtn;
		goto abort;
	}
	/* basic checks...*/
	if(privIsRef) {
		if(privKey->KeyHeader.BlobType != CSSM_KEYBLOB_REFERENCE) {
			printf("privKey blob type: exp %u got %u\n",
				CSSM_KEYBLOB_REFERENCE, (unsigned)privKey->KeyHeader.BlobType);
			ocrtn = CSSM_ERRCODE_INTERNAL_ERROR;
			goto abort;
		}
	}
	else {
		switch(privKey->KeyHeader.BlobType) {
			case CSSM_KEYBLOB_RAW:
				break;
			default:
				printf("privKey blob type: exp raw, got %u\n",
					(unsigned)privKey->KeyHeader.BlobType);
				ocrtn = CSSM_ERRCODE_INTERNAL_ERROR;
				goto abort;
		}
	}
	if(pubIsRef) {
		if(pubKey->KeyHeader.BlobType != CSSM_KEYBLOB_REFERENCE) {
			printf("pubKey blob type: exp %u got %u\n",
				CSSM_KEYBLOB_REFERENCE, (unsigned)pubKey->KeyHeader.BlobType);
			ocrtn = CSSM_ERRCODE_INTERNAL_ERROR;
			goto abort;
		}
	}
	else {
		switch(pubKey->KeyHeader.BlobType) {
			case CSSM_KEYBLOB_RAW:
				break;
			default:
				printf("pubKey blob type: exp raw or raw_berder, got %u\n",
					(unsigned)pubKey->KeyHeader.BlobType);
				ocrtn = CSSM_ERRCODE_INTERNAL_ERROR;
				goto abort;
		}
	}
abort:
	if(ccHand != 0) {
		crtn = CSSM_DeleteContext(ccHand);
		if(crtn) {
			printError("CSSM_DeleteContext", crtn);
			ocrtn = CSSM_ERRCODE_INTERNAL_ERROR;
		}
	}
	if(privData.Data != NULL) {
		CSSM_FREE(privData.Data);
	}
	return ocrtn;
}

/*
 * Generate FEE key pair with optional primeType, curveType, and seed (password) data.
 */
CSSM_RETURN cspGenFEEKeyPair(CSSM_CSP_HANDLE cspHand,
	const char *keyLabel,
	unsigned keyLabelLen,
	uint32 keySize,					// in bits
	uint32 primeType,				// CSSM_FEE_PRIME_TYPE_MERSENNE, etc.
	uint32 curveType,				// CSSM_FEE_CURVE_TYPE_MONTGOMERY, etc.
	CSSM_KEY_PTR pubKey,			// mallocd by caller
	CSSM_BOOL pubIsRef,				// true - reference key, false - data
	uint32 pubKeyUsage,				// CSSM_KEYUSE_ENCRYPT, etc.
	CSSM_KEYBLOB_FORMAT pubFormat,	// Optional. Specify 0 or CSSM_KEYBLOB_RAW_FORMAT_NONE
									//   to get the default format. 
	CSSM_KEY_PTR privKey,			// mallocd by caller
	CSSM_BOOL privIsRef,			// true - reference key, false - data
	uint32 privKeyUsage,			// CSSM_KEYUSE_DECRYPT, etc.
	CSSM_KEYBLOB_FORMAT privFormat,	// optional 0 ==> default
	const CSSM_DATA *seedData)		// Present: CSP will hash this for private data.
									// NULL: CSP generates random seed. 
{
	CSSM_RETURN				crtn;
	CSSM_CC_HANDLE 			ccHand;
	CSSM_CRYPTO_DATA		privCData;
	CSSM_CRYPTO_DATA_PTR	privCDataPtr = NULL;
	CSSM_DATA				keyLabelData;
	uint32					pubAttr;
	uint32					privAttr;
	CSSM_RETURN 			ocrtn = CSSM_OK;
	
	/* pre-context-create algorithm-specific stuff */
	if(seedData) {
		privCData.Param = *((CSSM_DATA_PTR)seedData);
		privCData.Callback = NULL;
		privCDataPtr = &privCData;
	}
	/* else CSP generates random seed/key */
	
	if(keySize == CSP_KEY_SIZE_DEFAULT) {
		keySize = CSP_FEE_KEY_SIZE_DEFAULT;
	}

	keyLabelData.Data        = (uint8 *)keyLabel,
	keyLabelData.Length      = keyLabelLen;
	memset(pubKey, 0, sizeof(CSSM_KEY));
	memset(privKey, 0, sizeof(CSSM_KEY));
	setBadKeyData(pubKey);
	setBadKeyData(privKey);
	
	crtn = CSSM_CSP_CreateKeyGenContext(cspHand,
		CSSM_ALGID_FEE,
		keySize,
		privCDataPtr,			// Seed
		NULL,					// Salt
		NULL,					// StartDate
		NULL,					// EndDate
		NULL,					// Params
		&ccHand);
	if(crtn) {
		printError("CSSM_CSP_CreateKeyGenContext", crtn);
		ocrtn = crtn;
		goto abort;
	}
	/* cook up attribute bits */
	if(pubIsRef) {
		pubAttr = CSSM_KEYATTR_RETURN_REF | CSSM_KEYATTR_EXTRACTABLE;
	}
	else {
		pubAttr = CSSM_KEYATTR_RETURN_DATA | CSSM_KEYATTR_EXTRACTABLE;
	}
	if(privIsRef) {
		privAttr = CSSM_KEYATTR_RETURN_REF | CSSM_KEYATTR_EXTRACTABLE;
	}
	else {
		privAttr = CSSM_KEYATTR_RETURN_DATA | CSSM_KEYATTR_EXTRACTABLE;
	}

	/* optional post-context-create stuff */
	if(primeType != CSSM_FEE_PRIME_TYPE_DEFAULT) {
		crtn = AddContextAttribute(ccHand,
			CSSM_ATTRIBUTE_FEE_PRIME_TYPE,
			sizeof(uint32),		
			CAT_Uint32,
			NULL,
			primeType);
		if(crtn) {
			printError("AddContextAttribute(CSSM_ATTRIBUTE_FEE_PRIME_TYPE)", crtn);
			return crtn;
		}
	}
	if(curveType != CSSM_FEE_CURVE_TYPE_DEFAULT) {
		crtn = AddContextAttribute(ccHand,
			CSSM_ATTRIBUTE_FEE_CURVE_TYPE,
			sizeof(uint32),		
			CAT_Uint32,
			NULL,
			curveType);
		if(crtn) {
			printError("AddContextAttribute(CSSM_ATTRIBUTE_FEE_CURVE_TYPE)", crtn);
			return crtn;
		}
	}
	
	if(pubFormat != CSSM_KEYBLOB_RAW_FORMAT_NONE) {
		crtn = AddContextAttribute(ccHand,
			CSSM_ATTRIBUTE_PUBLIC_KEY_FORMAT,
			sizeof(uint32),		
			CAT_Uint32,
			NULL,
			pubFormat);
		if(crtn) {
			printError("AddContextAttribute(CSSM_ATTRIBUTE_PUBLIC_KEY_FORMAT)", crtn);
			return crtn;
		}
	}
	if(privFormat != CSSM_KEYBLOB_RAW_FORMAT_NONE) {
		crtn = AddContextAttribute(ccHand,
			CSSM_ATTRIBUTE_PRIVATE_KEY_FORMAT,
			sizeof(uint32),			// currently sizeof CSSM_DATA
			CAT_Uint32,
			NULL,
			pubFormat);
		if(crtn) {
			printError("AddContextAttribute(CSSM_ATTRIBUTE_PRIVATE_KEY_FORMAT)", crtn);
			return crtn;
		}
	}
	crtn = CSSM_GenerateKeyPair(ccHand,
		pubKeyUsage,
		pubAttr,
		&keyLabelData,
		pubKey,
		privKeyUsage,
		privAttr,
		&keyLabelData,			// same labels
		NULL,					// CredAndAclEntry
		privKey);
	if(crtn) {
		printError("CSSM_GenerateKeyPair", crtn);
		ocrtn = crtn;
		goto abort;
	}
	/* basic checks...*/
	if(privIsRef) {
		if(privKey->KeyHeader.BlobType != CSSM_KEYBLOB_REFERENCE) {
			printf("privKey blob type: exp %u got %u\n",
				CSSM_KEYBLOB_REFERENCE, (unsigned)privKey->KeyHeader.BlobType);
			ocrtn = CSSM_ERRCODE_INTERNAL_ERROR;
			goto abort;
		}
	}
	else {
		switch(privKey->KeyHeader.BlobType) {
			case CSSM_KEYBLOB_RAW:
				break;
			default:
				printf("privKey blob type: exp raw, got %u\n",
					(unsigned)privKey->KeyHeader.BlobType);
				ocrtn = CSSM_ERRCODE_INTERNAL_ERROR;
				goto abort;
		}
	}
	if(pubIsRef) {
		if(pubKey->KeyHeader.BlobType != CSSM_KEYBLOB_REFERENCE) {
			printf("pubKey blob type: exp %u got %u\n",
				CSSM_KEYBLOB_REFERENCE, (unsigned)pubKey->KeyHeader.BlobType);
			ocrtn = CSSM_ERRCODE_INTERNAL_ERROR;
			goto abort;
		}
	}
	else {
		switch(pubKey->KeyHeader.BlobType) {
			case CSSM_KEYBLOB_RAW:
				break;
			default:
				printf("pubKey blob type: exp raw or raw_berder, got %u\n",
					(unsigned)pubKey->KeyHeader.BlobType);
				ocrtn = CSSM_ERRCODE_INTERNAL_ERROR;
				goto abort;
		}
	}
abort:
	if(ccHand != 0) {
		crtn = CSSM_DeleteContext(ccHand);
		if(crtn) {
			printError("CSSM_DeleteContext", crtn);
			ocrtn = CSSM_ERRCODE_INTERNAL_ERROR;
		}
	}
	return ocrtn;
}

/*
 * Generate DSA key pair with optional generateAlgParams and optional
 * incoming parameters.
 */
CSSM_RETURN cspGenDSAKeyPair(CSSM_CSP_HANDLE cspHand,
	const char *keyLabel,
	unsigned keyLabelLen,
	uint32 keySize,					// in bits
	CSSM_KEY_PTR pubKey,			// mallocd by caller
	CSSM_BOOL pubIsRef,				// true - reference key, false - data
	uint32 pubKeyUsage,				// CSSM_KEYUSE_ENCRYPT, etc.
	CSSM_KEYBLOB_FORMAT pubFormat,	// Optional. Specify 0 or CSSM_KEYBLOB_RAW_FORMAT_NONE
									//   to get the default format. 
	CSSM_KEY_PTR privKey,			// mallocd by caller
	CSSM_BOOL privIsRef,			// true - reference key, false - data
	uint32 privKeyUsage,			// CSSM_KEYUSE_DECRYPT, etc.
	CSSM_KEYBLOB_FORMAT privFormat,	// Optional. Specify 0 or CSSM_KEYBLOB_RAW_FORMAT_NONE
									//   to get the default format. 
	CSSM_BOOL genParams,
	CSSM_DATA_PTR paramData)		// optional	
{
	CSSM_RETURN				crtn;
	CSSM_CC_HANDLE 			ccHand;
	CSSM_DATA				keyLabelData;
	uint32					pubAttr;
	uint32					privAttr;
	CSSM_RETURN 			ocrtn = CSSM_OK;
	
	if(keySize == CSP_KEY_SIZE_DEFAULT) {
		keySize = CSP_DSA_KEY_SIZE_DEFAULT;
	}
	keyLabelData.Data        = (uint8 *)keyLabel,
	keyLabelData.Length      = keyLabelLen;
	memset(pubKey, 0, sizeof(CSSM_KEY));
	memset(privKey, 0, sizeof(CSSM_KEY));
	setBadKeyData(pubKey);
	setBadKeyData(privKey);
	
	crtn = CSSM_CSP_CreateKeyGenContext(cspHand,
		CSSM_ALGID_DSA,
		keySize,
		NULL,					// Seed
		NULL,					// Salt
		NULL,					// StartDate
		NULL,					// EndDate
		paramData,
		&ccHand);
	if(crtn) {
		printError("CSSM_CSP_CreateKeyGenContext", crtn);
		ocrtn = crtn;
		goto abort;
	}
	
	/* cook up attribute bits */
	if(pubIsRef) {
		pubAttr = CSSM_KEYATTR_RETURN_REF | CSSM_KEYATTR_EXTRACTABLE;
	}
	else {
		pubAttr = CSSM_KEYATTR_RETURN_DATA | CSSM_KEYATTR_EXTRACTABLE;
	}
	if(privIsRef) {
		privAttr = CSSM_KEYATTR_RETURN_REF | CSSM_KEYATTR_EXTRACTABLE;
	}
	else {
		privAttr = CSSM_KEYATTR_RETURN_DATA | CSSM_KEYATTR_EXTRACTABLE;
	}

	if(genParams) {
		/* 
		 * extra step - generate params - this just adds some
		 * info to the context
		 */
		CSSM_DATA dummy = {0, NULL};
		crtn = CSSM_GenerateAlgorithmParams(ccHand, 
			keySize, &dummy);
		if(crtn) {
			printError("CSSM_GenerateAlgorithmParams", crtn);
			return crtn;
		}
		appFreeCssmData(&dummy, CSSM_FALSE);
	}
	
	/* optional format specifiers */
	if(!pubIsRef && (pubFormat != CSSM_KEYBLOB_RAW_FORMAT_NONE)) {
		crtn = AddContextAttribute(ccHand,
			CSSM_ATTRIBUTE_PUBLIC_KEY_FORMAT,
			sizeof(uint32),	
			CAT_Uint32,
			NULL,
			pubFormat);
		if(crtn) {
			printError("AddContextAttribute(CSSM_ATTRIBUTE_PUBLIC_KEY_FORMAT)", crtn);
			return crtn;
		}
	}
	if(!privIsRef && (privFormat != CSSM_KEYBLOB_RAW_FORMAT_NONE)) {
		crtn = AddContextAttribute(ccHand,
			CSSM_ATTRIBUTE_PRIVATE_KEY_FORMAT,
			sizeof(uint32),			// currently sizeof CSSM_DATA
			CAT_Uint32,
			NULL,
			privFormat);
		if(crtn) {
			printError("AddContextAttribute(CSSM_ATTRIBUTE_PRIVATE_KEY_FORMAT)", crtn);
			return crtn;
		}
	}

	crtn = CSSM_GenerateKeyPair(ccHand,
		pubKeyUsage,
		pubAttr,
		&keyLabelData,
		pubKey,
		privKeyUsage,
		privAttr,
		&keyLabelData,			// same labels
		NULL,					// CredAndAclEntry
		privKey);
	if(crtn) {
		printError("CSSM_GenerateKeyPair", crtn);
		ocrtn = crtn;
		goto abort;
	}
	/* basic checks...*/
	if(privIsRef) {
		if(privKey->KeyHeader.BlobType != CSSM_KEYBLOB_REFERENCE) {
			printf("privKey blob type: exp %u got %u\n",
				CSSM_KEYBLOB_REFERENCE, (unsigned)privKey->KeyHeader.BlobType);
			ocrtn = CSSM_ERRCODE_INTERNAL_ERROR;
			goto abort;
		}
	}
	else {
		switch(privKey->KeyHeader.BlobType) {
			case CSSM_KEYBLOB_RAW:
				break;
			default:
				printf("privKey blob type: exp raw, got %u\n",
					(unsigned)privKey->KeyHeader.BlobType);
				ocrtn = CSSM_ERRCODE_INTERNAL_ERROR;
				goto abort;
		}
	}
	if(pubIsRef) {
		if(pubKey->KeyHeader.BlobType != CSSM_KEYBLOB_REFERENCE) {
			printf("pubKey blob type: exp %u got %u\n",
				CSSM_KEYBLOB_REFERENCE, (unsigned)pubKey->KeyHeader.BlobType);
			ocrtn = CSSM_ERRCODE_INTERNAL_ERROR;
			goto abort;
		}
	}
	else {
		switch(pubKey->KeyHeader.BlobType) {
			case CSSM_KEYBLOB_RAW:
				break;
			default:
				printf("pubKey blob type: exp raw or raw_berder, got %u\n",
					(unsigned)pubKey->KeyHeader.BlobType);
				ocrtn = CSSM_ERRCODE_INTERNAL_ERROR;
				goto abort;
		}
	}
abort:
	if(ccHand != 0) {
		crtn = CSSM_DeleteContext(ccHand);
		if(crtn) {
			printError("CSSM_DeleteContext", crtn);
			ocrtn = CSSM_ERRCODE_INTERNAL_ERROR;
		}
	}
	return ocrtn;
}


uint32 cspDefaultKeySize(uint32 alg)
{
	uint32 keySizeInBits;
	switch(alg) {
		case CSSM_ALGID_DES:
			keySizeInBits = CSP_DES_KEY_SIZE_DEFAULT;
			break;
		case CSSM_ALGID_3DES_3KEY:
		case CSSM_ALGID_DESX:
			keySizeInBits = CSP_DES3_KEY_SIZE_DEFAULT;
			break;
		case CSSM_ALGID_RC2:
			keySizeInBits = CSP_RC2_KEY_SIZE_DEFAULT;
			break;
		case CSSM_ALGID_RC4:
			keySizeInBits = CSP_RC4_KEY_SIZE_DEFAULT;
			break;
		case CSSM_ALGID_RC5:
			keySizeInBits = CSP_RC5_KEY_SIZE_DEFAULT;
			break;
		case CSSM_ALGID_ASC:
			keySizeInBits = CSP_ASC_KEY_SIZE_DEFAULT;
			break;
		case CSSM_ALGID_BLOWFISH:
			keySizeInBits = CSP_BFISH_KEY_SIZE_DEFAULT;
			break;
		case CSSM_ALGID_CAST:
			keySizeInBits = CSP_CAST_KEY_SIZE_DEFAULT;
			break;
		case CSSM_ALGID_IDEA:
			keySizeInBits = CSP_IDEA_KEY_SIZE_DEFAULT;
			break;
		case CSSM_ALGID_AES:
			keySizeInBits = CSP_AES_KEY_SIZE_DEFAULT;
			break;
		case CSSM_ALGID_SHA1HMAC:
			keySizeInBits = CSP_HMAC_SHA_KEY_SIZE_DEFAULT;
			break;
		case CSSM_ALGID_MD5HMAC:
			keySizeInBits = CSP_HMAC_MD5_KEY_SIZE_DEFAULT;
			break;
		case CSSM_ALGID_FEE:
			keySizeInBits = CSP_FEE_KEY_SIZE_DEFAULT;
			break;
		case CSSM_ALGID_RSA:
			keySizeInBits = CSP_RSA_KEY_SIZE_DEFAULT;
			break;
		case CSSM_ALGID_DSA:
			keySizeInBits = CSP_DSA_KEY_SIZE_DEFAULT;
			break;
		case CSSM_ALGID_ECDSA:
			keySizeInBits = CSP_ECDSA_KEY_SIZE_DEFAULT;
			break;
		case CSSM_ALGID_NONE:
			keySizeInBits = CSP_NULL_CRYPT_KEY_SIZE_DEF;
			break;
		default:
			printf("***cspDefaultKeySize: Unknown symmetric algorithm\n");
			keySizeInBits = 0;
			break;
	}
	return keySizeInBits;
}

/*
 * Create a random symmetric key.
 */
CSSM_KEY_PTR cspGenSymKey(CSSM_CSP_HANDLE cspHand,
		uint32 				alg,
		const char 			*keyLabel,
		unsigned 			keyLabelLen,
		uint32 				keyUsage,		// CSSM_KEYUSE_ENCRYPT, etc.
		uint32 				keySizeInBits,
		CSSM_BOOL			refKey)
{
	CSSM_KEY_PTR 		symKey = (CSSM_KEY_PTR)CSSM_MALLOC(sizeof(CSSM_KEY));
	CSSM_RETURN			crtn;
	CSSM_CC_HANDLE 		ccHand;
	uint32				keyAttr;
	CSSM_DATA			dummyLabel;
	
	if(symKey == NULL) {
		printf("Insufficient heap space\n");
		return NULL;
	}
	memset(symKey, 0, sizeof(CSSM_KEY));
	setBadKeyData(symKey);
	if(keySizeInBits == CSP_KEY_SIZE_DEFAULT) {
		keySizeInBits = cspDefaultKeySize(alg);
	}
	crtn = CSSM_CSP_CreateKeyGenContext(cspHand,
		alg,
		keySizeInBits,	// keySizeInBits
		NULL,			// Seed
		NULL,			// Salt
		NULL,			// StartDate
		NULL,			// EndDate
		NULL,			// Params
		&ccHand);
	if(crtn) {
		printError("CSSM_CSP_CreateKeyGenContext", crtn);
		goto errorOut;
	}
	if(refKey) {
		keyAttr = CSSM_KEYATTR_RETURN_REF | CSSM_KEYATTR_EXTRACTABLE;
	}
	else {
		keyAttr = CSSM_KEYATTR_RETURN_DATA | CSSM_KEYATTR_EXTRACTABLE;
	}
	dummyLabel.Length = keyLabelLen;
	dummyLabel.Data = (uint8 *)keyLabel;

	crtn = CSSM_GenerateKey(ccHand,
		keyUsage,
		keyAttr,
		&dummyLabel,
		NULL,			// ACL
		symKey);
	if(crtn) {
		printError("CSSM_GenerateKey", crtn);
		goto errorOut;
	}
	crtn = CSSM_DeleteContext(ccHand);
	if(crtn) {
		printError("CSSM_DeleteContext", crtn);
		goto errorOut;
	}
	return symKey;
errorOut:
	CSSM_FREE(symKey);
	return NULL;
}

/*
 * Derive symmetric key.
 * Note in the X CSP, we never return an IV. 
 */
CSSM_KEY_PTR cspDeriveKey(CSSM_CSP_HANDLE cspHand,
		uint32 				deriveAlg,		// CSSM_ALGID_PKCS5_PBKDF2, etc.
		uint32				keyAlg,			// CSSM_ALGID_RC5, etc.
		const char 			*keyLabel,
		unsigned 			keyLabelLen,
		uint32 				keyUsage,		// CSSM_KEYUSE_ENCRYPT, etc.
		uint32 				keySizeInBits,
		CSSM_BOOL			isRefKey,
		CSSM_DATA_PTR		password,		// in PKCS-5 lingo
		CSSM_DATA_PTR		salt,			// ditto
		uint32				iterationCnt,	// ditto
		CSSM_DATA_PTR		initVector)		// mallocd & RETURNED
{
	CSSM_KEY_PTR 				symKey = (CSSM_KEY_PTR)
									CSSM_MALLOC(sizeof(CSSM_KEY));
	CSSM_RETURN					crtn;
	CSSM_CC_HANDLE 				ccHand;
	uint32						keyAttr;
	CSSM_DATA					dummyLabel;
	CSSM_PKCS5_PBKDF2_PARAMS 	pbeParams;
	CSSM_DATA					pbeData;
	CSSM_ACCESS_CREDENTIALS		creds;
	
	if(symKey == NULL) {
		printf("Insufficient heap space\n");
		return NULL;
	}
	memset(symKey, 0, sizeof(CSSM_KEY));
	setBadKeyData(symKey);
	memset(&creds, 0, sizeof(CSSM_ACCESS_CREDENTIALS));
	if(keySizeInBits == CSP_KEY_SIZE_DEFAULT) {
		keySizeInBits = cspDefaultKeySize(keyAlg);
	}
	crtn = CSSM_CSP_CreateDeriveKeyContext(cspHand,
		deriveAlg,
		keyAlg,
		keySizeInBits,
		&creds,
		NULL,			// BaseKey
		iterationCnt,
		salt,
		NULL,			// seed
		&ccHand);
	if(crtn) {
		printError("CSSM_CSP_CreateDeriveKeyContext", crtn);
		goto errorOut;
	}
	keyAttr = CSSM_KEYATTR_EXTRACTABLE;
	if(isRefKey) {
		keyAttr |= (CSSM_KEYATTR_RETURN_REF | CSSM_KEYATTR_SENSITIVE);
	}
	else {
		keyAttr |= CSSM_KEYATTR_RETURN_DATA;
	}
	dummyLabel.Length = keyLabelLen;
	dummyLabel.Data = (uint8 *)keyLabel;
	
	/* passing in password is pretty strange....*/
	pbeParams.Passphrase = *password;
	pbeParams.PseudoRandomFunction = 
			CSSM_PKCS5_PBKDF2_PRF_HMAC_SHA1;
	pbeData.Data = (uint8 *)&pbeParams;
	pbeData.Length = sizeof(pbeParams);
	crtn = CSSM_DeriveKey(ccHand,
		&pbeData,
		keyUsage,
		keyAttr,
		&dummyLabel,
		NULL,			// cred and acl
		symKey);
	if(crtn) {
		printError("CSSM_DeriveKey", crtn);
		goto errorOut;
	}
	/* copy IV back to caller */
	/* Nope, not supported */
	#if 0
	if(pbeParams.InitVector.Data != NULL) {
		if(initVector->Data != NULL) {
			if(initVector->Length < pbeParams.InitVector.Length) {
				printf("***Insufficient InitVector\n");
				goto errorOut;
			}
		}
		else {
			initVector->Data = 
				(uint8 *)CSSM_MALLOC(pbeParams.InitVector.Length);
		}
		memmove(initVector->Data, pbeParams.InitVector.Data,
				pbeParams.InitVector.Length);
		initVector->Length = pbeParams.InitVector.Length;
		CSSM_FREE(pbeParams.InitVector.Data);
	}
	else {
		printf("***Warning: CSSM_DeriveKey, no InitVector\n");
	}
	#endif
	crtn = CSSM_DeleteContext(ccHand);
	if(crtn) {
		printError("CSSM_DeleteContext", crtn);
		goto errorOut;
	}
	return symKey;
errorOut:
	CSSM_FREE(symKey);
	return NULL;
}

/*
 * Cook up a symmetric key with specified key bits and other
 * params. Currently the CSPDL can only deal with reference keys except when
 * doing wrap/unwrap, so we manually cook up a raw key, then we null-unwrap it. 
 */
CSSM_RETURN cspGenSymKeyWithBits(
	CSSM_CSP_HANDLE		cspHand,
	CSSM_ALGORITHMS		keyAlg,
	CSSM_KEYUSE			keyUsage,
	const CSSM_DATA		*keyBits,
	unsigned			keySizeInBytes,
	CSSM_KEY_PTR		refKey)				// init'd and RETURNED
{
	CSSM_KEY			rawKey;
	CSSM_KEYHEADER_PTR	hdr = &rawKey.KeyHeader;
	CSSM_RETURN			crtn;
	
	/* set up a raw key the CSP will accept */
	memset(&rawKey, 0, sizeof(CSSM_KEY));
	hdr->HeaderVersion = CSSM_KEYHEADER_VERSION;
	hdr->BlobType = CSSM_KEYBLOB_RAW;
	hdr->Format = CSSM_KEYBLOB_RAW_FORMAT_OCTET_STRING;
	hdr->AlgorithmId = keyAlg;
	hdr->KeyClass = CSSM_KEYCLASS_SESSION_KEY;
	hdr->LogicalKeySizeInBits = keySizeInBytes * 8;
	hdr->KeyAttr = CSSM_KEYATTR_EXTRACTABLE;
	hdr->KeyUsage = keyUsage;
	appSetupCssmData(&rawKey.KeyData, keySizeInBytes);
	memmove(rawKey.KeyData.Data, keyBits->Data, keySizeInBytes);
	
	/* convert to a ref key */
	crtn = cspRawKeyToRef(cspHand, &rawKey, refKey);
	appFreeCssmData(&rawKey.KeyData, CSSM_FALSE);
	return crtn;
}

/*
 * Free a key. This frees a CSP's resources associated with the key if
 * the key is a reference key. It also frees key->KeyData. The CSSM_KEY
 * struct itself is not freed.
 * Note this has no effect on the CSP or DL cached keys unless the incoming
 * key is a reference key.
 */
CSSM_RETURN	cspFreeKey(CSSM_CSP_HANDLE cspHand,
	CSSM_KEY_PTR key)
{
	CSSM_RETURN crtn;
	crtn = CSSM_FreeKey(cspHand, 
		NULL,		// access cred
		key,
		CSSM_FALSE);	// delete - OK? maybe should parameterize?
	if(crtn) {
		printError("CSSM_FreeKey", crtn);
	}
	return crtn;
}

/* generate a random and reasonable key size in bits for specified CSSM algorithm */
uint32 randKeySizeBits(uint32 alg, 
	opType op)			// OT_Encrypt, etc.
{
	uint32 minSize;
	uint32 maxSize;
	uint32 size;
	
	switch(alg) {
		case CSSM_ALGID_DES:
			return CSP_DES_KEY_SIZE_DEFAULT;
		case CSSM_ALGID_3DES_3KEY:
		case CSSM_ALGID_DESX:
			return CSP_DES3_KEY_SIZE_DEFAULT;
		case CSSM_ALGID_ASC:
		case CSSM_ALGID_RC2:
		case CSSM_ALGID_RC4:
		case CSSM_ALGID_RC5:
			minSize = 5 * 8;
			maxSize = MAX_KEY_SIZE_RC245_BYTES * 8 ;	// somewhat arbitrary
			break;
		case CSSM_ALGID_BLOWFISH:
			minSize = 32;
			maxSize = 448;
			break;
		case CSSM_ALGID_CAST:
			minSize = 40;
			maxSize = 128;
			break;
		case CSSM_ALGID_IDEA:
			return CSP_IDEA_KEY_SIZE_DEFAULT;
		case CSSM_ALGID_RSA:
			minSize = CSP_RSA_KEY_SIZE_DEFAULT;
			maxSize = 1024;
			break;
		case CSSM_ALGID_DSA:
			/* signature only, no export restriction */
			minSize = 512;
			maxSize = 1024;
			break;
		case CSSM_ALGID_SHA1HMAC:
			minSize = 20 * 8;
			maxSize = 256 * 8;
			break;
		case CSSM_ALGID_MD5HMAC:
			minSize = 16 * 8;
			maxSize = 256 * 8;
			break;
		case CSSM_ALGID_FEE:
			/* FEE requires discrete sizes */
			size = genRand(1,4);
			switch(size) {
				case 1:
					return 31;
				case 2:
					if(alg == CSSM_ALGID_FEE) {
						return 127;
					}
					else {
						return 128;
					}
				case 3:
					return 161;
				case 4:
					return 192;
				default:
					printf("randKeySizeBits: internal error\n");
					return 0;
			}
		case CSSM_ALGID_ECDSA:
		case CSSM_ALGID_SHA1WithECDSA:
			/* ECDSA require discrete sizes */
			size = genRand(1,4);
			switch(size) {
				case 1:
					return 192;
				case 2:
					return 256;
				case 3:
					return 384;
				case 4:
				default:
					return 521;
			}
		case CSSM_ALGID_AES:
			size = genRand(1, 3);
			switch(size) {
				case 1:
					return 128;
				case 2:
					return 192;
				case 3:
					return 256;
			}
		case CSSM_ALGID_NONE:
			return CSP_NULL_CRYPT_KEY_SIZE_DEF;
		default:
			printf("randKeySizeBits: unknown alg\n");
			return CSP_KEY_SIZE_DEFAULT;
	}
	size = genRand(minSize, maxSize);
	
	/* per-alg postprocessing.... */
	if(alg != CSSM_ALGID_RC2) {
		size &= ~0x7;
	}
	switch(alg) {
		case CSSM_ALGID_RSA:
			// new for X - strong keys */
			size &= ~(16 - 1);
			break;
		case CSSM_ALGID_DSA:
			/* size mod 64 == 0 */
			size &= ~(64 - 1);
			break;
		default:
			break;
	}
	return size;
}

#pragma mark --------- Encrypt/Decrypt ---------

/*
 * Encrypt/Decrypt
 */
/*
 * Common routine for encrypt/decrypt - cook up an appropriate context handle
 */
/*
 * When true, effectiveKeySizeInBits is passed down via the Params argument.
 * Otherwise, we add a customized context attribute.
 * Setting this true works with the stock Intel CSSM; this may well change.
 * Note this overloading prevent us from specifying RC5 rounds....
 */
#define EFFECTIVE_SIZE_VIA_PARAMS		0
CSSM_CC_HANDLE genCryptHandle(CSSM_CSP_HANDLE cspHand,
		uint32 algorithm,					// CSSM_ALGID_FEED, etc.
		uint32 mode,						// CSSM_ALGMODE_CBC, etc. - only for symmetric algs
		CSSM_PADDING padding,				// CSSM_PADDING_PKCS1, etc. 
		const CSSM_KEY *key0,
		const CSSM_KEY *key1,				// for CSSM_ALGID_FEED only - must be the 
											// public key
		const CSSM_DATA *iv,				// optional
		uint32 effectiveKeySizeInBits,		// 0 means skip this attribute
		uint32 rounds)						// ditto
{
	CSSM_CC_HANDLE cryptHand = 0;
	uint32 params;
	CSSM_RETURN crtn;
	CSSM_ACCESS_CREDENTIALS	creds;
	
	memset(&creds, 0, sizeof(CSSM_ACCESS_CREDENTIALS));
	#if	EFFECTIVE_SIZE_VIA_PARAMS
	params = effectiveKeySizeInBits;
	#else
	params = 0;
	#endif
	switch(algorithm) {
		case CSSM_ALGID_DES:
		case CSSM_ALGID_3DES_3KEY_EDE:
		case CSSM_ALGID_DESX:
		case CSSM_ALGID_ASC:
		case CSSM_ALGID_RC2:
		case CSSM_ALGID_RC4:
		case CSSM_ALGID_RC5:
		case CSSM_ALGID_AES:
		case CSSM_ALGID_BLOWFISH:
		case CSSM_ALGID_CAST:
		case CSSM_ALGID_IDEA:
		case CSSM_ALGID_NONE:		// used for wrapKey()
			crtn = CSSM_CSP_CreateSymmetricContext(cspHand,
				algorithm,
				mode,
				NULL,			// access cred
				key0,
				iv,				// InitVector
				padding,	
				NULL,			// Params
				&cryptHand);
			if(crtn) {
				printError("CSSM_CSP_CreateSymmetricContext", crtn);
				return 0;
			}
			break;
		case CSSM_ALGID_FEED:
		case CSSM_ALGID_FEEDEXP:
		case CSSM_ALGID_FEECFILE:
		case CSSM_ALGID_RSA:
			 crtn = CSSM_CSP_CreateAsymmetricContext(cspHand,
				algorithm,
				&creds,			// access
				key0,
				padding,
				&cryptHand);
			if(crtn) {
				printError("CSSM_CSP_CreateAsymmetricContext", crtn);
				return 0;
			}
			if(key1 != NULL) {
				/*
				 * FEED, some CFILE. Add (non-standard) second key attribute.
				 */
				crtn = AddContextAttribute(cryptHand,
						CSSM_ATTRIBUTE_PUBLIC_KEY,
						sizeof(CSSM_KEY),			// currently sizeof CSSM_DATA
						CAT_Ptr,
						key1,
						0);
				if(crtn) {
					printError("AddContextAttribute", crtn);
					return 0;
				}
			}
			if(mode != CSSM_ALGMODE_NONE) {
				/* special case, e.g., CSSM_ALGMODE_PUBLIC_KEY */
				crtn = AddContextAttribute(cryptHand,
						CSSM_ATTRIBUTE_MODE,
						sizeof(uint32),
						CAT_Uint32,
						NULL,
						mode);
				if(crtn) {
					printError("AddContextAttribute", crtn);
					return 0;
				}
			}
			break;
		default:
			printf("genCryptHandle: bogus algorithm\n");
			return 0;
	}
	#if		!EFFECTIVE_SIZE_VIA_PARAMS
	/* add optional EffectiveKeySizeInBits and rounds attributes */
	if(effectiveKeySizeInBits != 0) {
		CSSM_CONTEXT_ATTRIBUTE attr;
		attr.AttributeType = CSSM_ATTRIBUTE_EFFECTIVE_BITS;
		attr.AttributeLength = sizeof(uint32);
		attr.Attribute.Uint32 = effectiveKeySizeInBits;
		crtn = CSSM_UpdateContextAttributes(
			cryptHand,
			1,
			&attr);
		if(crtn) {
			printError("CSSM_UpdateContextAttributes", crtn);
			return crtn;
		}
	}
	#endif
	
	if(rounds != 0) {
		CSSM_CONTEXT_ATTRIBUTE attr;
		attr.AttributeType = CSSM_ATTRIBUTE_ROUNDS;
		attr.AttributeLength = sizeof(uint32);
		attr.Attribute.Uint32 = rounds;
		crtn = CSSM_UpdateContextAttributes(
			cryptHand,
			1,
			&attr);
		if(crtn) {
			printError("CSSM_UpdateContextAttributes", crtn);
			return crtn;
		}
	}

	return cryptHand;
}

CSSM_RETURN cspEncrypt(CSSM_CSP_HANDLE cspHand,
		uint32 algorithm,					// CSSM_ALGID_FEED, etc.
		uint32 mode,						// CSSM_ALGMODE_CBC, etc. - only for symmetric algs
		CSSM_PADDING padding,				// CSSM_PADDING_PKCS1, etc. 
		const CSSM_KEY *key,				// public or session key
		const CSSM_KEY *pubKey,				// for CSSM_ALGID_FEED, CSSM_ALGID_FEECFILE only
		uint32 effectiveKeySizeInBits,		// 0 means skip this attribute
		uint32 rounds,						// ditto
		const CSSM_DATA *iv,				// init vector, optional
		const CSSM_DATA *ptext,
		CSSM_DATA_PTR ctext,				// RETURNED
		CSSM_BOOL mallocCtext)				// if true, and ctext empty, malloc
											// by getting size from CSP
{
	CSSM_CC_HANDLE 	cryptHand;
	CSSM_RETURN		crtn;
	CSSM_SIZE		bytesEncrypted;
	CSSM_DATA		remData = {0, NULL};
	CSSM_RETURN		ocrtn = CSSM_OK;
	unsigned		origCtextLen;			// the amount we malloc, if any
	CSSM_RETURN		savedErr = CSSM_OK;
	CSSM_BOOL		restoreErr = CSSM_FALSE;
	
	cryptHand = genCryptHandle(cspHand, 
		algorithm, 
		mode, 
		padding,
		key, 
		pubKey, 
		iv, 
		effectiveKeySizeInBits,
		rounds);
	if(cryptHand == 0) {
		return CSSMERR_CSSM_INTERNAL_ERROR;
	}
	if(mallocCtext && (ctext->Length == 0)) {
		CSSM_QUERY_SIZE_DATA querySize;
		querySize.SizeInputBlock = ptext->Length;
		crtn = CSSM_QuerySize(cryptHand,
			CSSM_TRUE,						// encrypt
			1,
			&querySize);
		if(crtn) {
			printError("CSSM_QuerySize", crtn);
			ocrtn = crtn;
			goto abort;
		}
		if(querySize.SizeOutputBlock == 0) {
			/* CSP couldn't figure this out; skip our malloc */
			printf("***cspEncrypt: warning: cipherTextSize unknown; "
				"skipping malloc\n");
			origCtextLen = 0;
		}
		else {
			ctext->Data = (uint8 *)
				appMalloc(querySize.SizeOutputBlock, NULL);
			if(ctext->Data == NULL) {
				printf("Insufficient heap space\n");
				ocrtn = CSSM_ERRCODE_MEMORY_ERROR;
				goto abort;
			}
			ctext->Length = origCtextLen = querySize.SizeOutputBlock;
			memset(ctext->Data, 0, ctext->Length);
		}
	}
	else {
		origCtextLen = ctext->Length;
	}
	crtn = CSSM_EncryptData(cryptHand,
		ptext,
		1,
		ctext,
		1,
		&bytesEncrypted,
		&remData);
	if(crtn == CSSM_OK) {
		/*
		 * Deal with remData - its contents are included in bytesEncrypted.
		 */
		if((remData.Length != 0) && mallocCtext) {
			/* shouldn't happen - right? */
			if(bytesEncrypted > origCtextLen) {
				/* malloc and copy a new one */
				uint8 *newCdata = (uint8 *)appMalloc(bytesEncrypted, NULL);
				printf("**Warning: app malloced cipherBuf, but got nonzero "
					"remData!\n");
				if(newCdata == NULL) {
					printf("Insufficient heap space\n");
					ocrtn = CSSM_ERRCODE_MEMORY_ERROR;
					goto abort;
				}
				memmove(newCdata, ctext->Data, ctext->Length);
				memmove(newCdata+ctext->Length, remData.Data, remData.Length);
				CSSM_FREE(ctext->Data);
				ctext->Data = newCdata;
			}
			else {
				/* there's room left over */
				memmove(ctext->Data+ctext->Length, remData.Data, remData.Length);
			}
			ctext->Length = bytesEncrypted;
		}
		// NOTE: We return the proper length in ctext....
		ctext->Length = bytesEncrypted;
	}
	else {
		savedErr = crtn;
		restoreErr = CSSM_TRUE;
		printError("CSSM_EncryptData", crtn);
	}
abort:
	crtn = CSSM_DeleteContext(cryptHand);
	if(crtn) {
		printError("CSSM_DeleteContext", crtn);
		ocrtn = crtn;
	}
	if(restoreErr) {
		ocrtn = savedErr;
	}
	return ocrtn;
}

#define PAD_IMPLIES_RAND_PTEXTSIZE	1
#define LOG_STAGED_OPS				0
#if		LOG_STAGED_OPS
#define soprintf(s)	printf s
#else
#define soprintf(s)
#endif

CSSM_RETURN cspStagedEncrypt(CSSM_CSP_HANDLE cspHand,
		uint32 algorithm,					// CSSM_ALGID_FEED, etc.
		uint32 mode,						// CSSM_ALGMODE_CBC, etc. - only for symmetric algs
		CSSM_PADDING padding,				// CSSM_PADDING_PKCS1, etc. 
		const CSSM_KEY *key,				// public or session key
		const CSSM_KEY *pubKey,				// for CSSM_ALGID_FEED, CSSM_ALGID_FEECFILE only
		uint32 effectiveKeySizeInBits,		// 0 means skip this attribute
		uint32 cipherBlockSize,				// ditto
		uint32 rounds,						// ditto
		const CSSM_DATA *iv,				// init vector, optional
		const CSSM_DATA *ptext,
		CSSM_DATA_PTR ctext,				// RETURNED, we malloc
		CSSM_BOOL multiUpdates)				// false:single update, true:multi updates
{
	CSSM_CC_HANDLE 	cryptHand;
	CSSM_RETURN		crtn;
	CSSM_SIZE		bytesEncrypted;			// per update
	CSSM_SIZE		bytesEncryptedTotal = 0;
	CSSM_RETURN		ocrtn = CSSM_OK;		// 'our' crtn
	unsigned		toMove;					// remaining
	unsigned		thisMove;				// bytes to encrypt on this update
	CSSM_DATA		thisPtext;				// running ptr into ptext
	CSSM_DATA		ctextWork;				// per update, mallocd by CSP
	CSSM_QUERY_SIZE_DATA querySize;
	uint8			*origCtext;				// initial ctext->Data
	unsigned		origCtextLen;			// amount we mallocd
	CSSM_BOOL		restoreErr = CSSM_FALSE;
	CSSM_RETURN		savedErr = CSSM_OK;
	
	
	cryptHand = genCryptHandle(cspHand, 
		algorithm, 
		mode, 
		padding,
		key, 
		pubKey, 
		iv,
		effectiveKeySizeInBits,
		rounds);
	if(cryptHand == 0) {
		return CSSMERR_CSP_INTERNAL_ERROR;
	}
	if(cipherBlockSize) {
		crtn = AddContextAttribute(cryptHand,
			CSSM_ATTRIBUTE_BLOCK_SIZE,
			sizeof(uint32),
			CAT_Uint32,
			NULL,
			cipherBlockSize);
		if(crtn) {
			printError("CSSM_UpdateContextAttributes", crtn);
			goto abort;
		}
	}
	
	/* obtain total required ciphertext size and block size */
	querySize.SizeInputBlock = ptext->Length;
	crtn = CSSM_QuerySize(cryptHand,
		CSSM_TRUE,						// encrypt
		1,
		&querySize);
	if(crtn) {
		printError("CSSM_QuerySize(1)", crtn);
		ocrtn = CSSMERR_CSP_INTERNAL_ERROR;
		goto abort;
	}
	if(querySize.SizeOutputBlock == 0) {
		/* CSP couldn't figure this out; skip our malloc - caller is taking its
		 * chances */
		printf("***cspStagedEncrypt: warning: cipherTextSize unknown; aborting\n");
		ocrtn = CSSMERR_CSP_INTERNAL_ERROR;
		goto abort;
	}
	else {
		origCtextLen = querySize.SizeOutputBlock;
		if(algorithm == CSSM_ALGID_ASC) {
			/* ASC is weird - the more chunks we do, the bigger the
			 * resulting ctext...*/
			origCtextLen *= 2;
		}
		ctext->Length = origCtextLen;
		ctext->Data   = origCtext = (uint8 *)appMalloc(origCtextLen, NULL);
		if(ctext->Data == NULL) {
			printf("Insufficient heap space\n");
			ocrtn = CSSMERR_CSP_MEMORY_ERROR;
			goto abort;
		}
		memset(ctext->Data, 0, ctext->Length);
	}

	crtn = CSSM_EncryptDataInit(cryptHand);
	if(crtn) {
		printError("CSSM_EncryptDataInit", crtn);
		ocrtn = crtn;
		goto abort;
	}
	
	toMove = ptext->Length;
	thisPtext.Data = ptext->Data;
	while(toMove) {
		if(multiUpdates) {
			thisMove = genRand(1, toMove);
		}
		else {
			/* just do one pass thru this loop */
			thisMove = toMove;
		}
		thisPtext.Length = thisMove;
		/* let CSP do the individual mallocs */
		ctextWork.Data = NULL;
		ctextWork.Length = 0;
		soprintf(("*** EncryptDataUpdate: ptextLen 0x%x\n", thisMove));
		crtn = CSSM_EncryptDataUpdate(cryptHand,
			&thisPtext,
			1,
			&ctextWork,
			1,
			&bytesEncrypted);
		if(crtn) {
			printError("CSSM_EncryptDataUpdate", crtn);
			ocrtn = crtn;
			goto abort;
		}
		// NOTE: We return the proper length in ctext....
		ctextWork.Length = bytesEncrypted;
		soprintf(("*** EncryptDataUpdate: ptextLen 0x%x  bytesEncrypted 0x%x\n",
			thisMove, bytesEncrypted));
		thisPtext.Data += thisMove;
		toMove         -= thisMove;
		if(bytesEncrypted > ctext->Length) {
			printf("cspStagedEncrypt: ctext overflow!\n");
			ocrtn = crtn;
			goto abort;
		}
		if(bytesEncrypted != 0) {
			memmove(ctext->Data, ctextWork.Data, bytesEncrypted);
			bytesEncryptedTotal += bytesEncrypted;
			ctext->Data         += bytesEncrypted;
			ctext->Length       -= bytesEncrypted;
		}
		if(ctextWork.Data != NULL) {
			CSSM_FREE(ctextWork.Data);
		}
	}
	/* OK, one more */
	ctextWork.Data = NULL;
	ctextWork.Length = 0;
	crtn = CSSM_EncryptDataFinal(cryptHand, &ctextWork);
	if(crtn) {
		printError("CSSM_EncryptDataFinal", crtn);
		savedErr = crtn;
		restoreErr = CSSM_TRUE;
		goto abort;
	}
	if(ctextWork.Length != 0) {
		bytesEncryptedTotal += ctextWork.Length;
		if(ctextWork.Length > ctext->Length) {
			printf("cspStagedEncrypt: ctext overflow (2)!\n");
			ocrtn = CSSMERR_CSP_INTERNAL_ERROR;
			goto abort;
		}
		memmove(ctext->Data, ctextWork.Data, ctextWork.Length);
	}
	if(ctextWork.Data) {
		/* this could have gotten mallocd and Length still be zero */
		CSSM_FREE(ctextWork.Data);
	}

	/* retweeze ctext */
	ctext->Data   = origCtext;
	ctext->Length = bytesEncryptedTotal;
abort:
	crtn = CSSM_DeleteContext(cryptHand);
	if(crtn) {
		printError("CSSM_DeleteContext", crtn);
		ocrtn = crtn;
	}
	if(restoreErr) {
		/* give caller the error from the encrypt */
		ocrtn = savedErr;
	}
	return ocrtn;
}

CSSM_RETURN cspDecrypt(CSSM_CSP_HANDLE cspHand,
		uint32 algorithm,					// CSSM_ALGID_FEED, etc.
		uint32 mode,						// CSSM_ALGMODE_CBC, etc. - only for symmetric algs
		CSSM_PADDING padding,				// CSSM_PADDING_PKCS1, etc. 
		const CSSM_KEY *key,				// public or session key
		const CSSM_KEY *pubKey,				// for CSSM_ALGID_FEED, CSSM_ALGID_FEECFILE only
		uint32 effectiveKeySizeInBits,		// 0 means skip this attribute
		uint32 rounds,						// ditto
		const CSSM_DATA *iv,				// init vector, optional
		const CSSM_DATA *ctext,
		CSSM_DATA_PTR ptext,				// RETURNED
		CSSM_BOOL mallocPtext)				// if true and ptext->Length = 0,
											//   we'll malloc
{
	CSSM_CC_HANDLE 	cryptHand;
	CSSM_RETURN		crtn;
	CSSM_RETURN		ocrtn = CSSM_OK;
	CSSM_SIZE		bytesDecrypted;
	CSSM_DATA		remData = {0, NULL};
	unsigned		origPtextLen;			// the amount we malloc, if any

	cryptHand = genCryptHandle(cspHand, 
		algorithm, 
		mode, 
		padding,
		key, 
		pubKey, 
		iv,
		effectiveKeySizeInBits,
		rounds);
	if(cryptHand == 0) {
		return CSSMERR_CSP_INTERNAL_ERROR;
	}
	if(mallocPtext && (ptext->Length == 0)) {
		CSSM_QUERY_SIZE_DATA querySize;
		querySize.SizeInputBlock = ctext->Length;
		crtn = CSSM_QuerySize(cryptHand,
			CSSM_FALSE,						// encrypt
			1,
			&querySize);
		if(crtn) {
			printError("CSSM_QuerySize", crtn);
			ocrtn = crtn;
			goto abort;
		}
		if(querySize.SizeOutputBlock == 0) {
			/* CSP couldn't figure this one out; skip our malloc */
			printf("***cspDecrypt: warning: plainTextSize unknown; "
				"skipping malloc\n");
			origPtextLen = 0;
		}
		else {
			ptext->Data = 
				(uint8 *)appMalloc(querySize.SizeOutputBlock, NULL);
			if(ptext->Data == NULL) {
				printf("Insufficient heap space\n");
				ocrtn = CSSMERR_CSP_MEMORY_ERROR;
				goto abort;
			}
			ptext->Length = origPtextLen = querySize.SizeOutputBlock;
			memset(ptext->Data, 0, ptext->Length);
		}
	}
	else {
		origPtextLen = ptext->Length;
	}
	crtn = CSSM_DecryptData(cryptHand,
		ctext,
		1,
		ptext,
		1,
		&bytesDecrypted,
		&remData);
	if(crtn == CSSM_OK) {
		/*
		 * Deal with remData - its contents are included in bytesDecrypted.
		 */
		if((remData.Length != 0) && mallocPtext) {
			/* shouldn't happen - right? */
			if(bytesDecrypted > origPtextLen) {
				/* malloc and copy a new one */
				uint8 *newPdata = (uint8 *)appMalloc(bytesDecrypted, NULL);
				printf("**Warning: app malloced ClearBuf, but got nonzero "
					"remData!\n");
				if(newPdata == NULL) {
					printf("Insufficient heap space\n");
					ocrtn = CSSMERR_CSP_MEMORY_ERROR;
					goto abort;
				}
				memmove(newPdata, ptext->Data, ptext->Length);
				memmove(newPdata + ptext->Length,
					remData.Data, remData.Length);
				CSSM_FREE(ptext->Data);
				ptext->Data = newPdata;
			}
			else {
				/* there's room left over */
				memmove(ptext->Data + ptext->Length,
					remData.Data, remData.Length);
			}
			ptext->Length = bytesDecrypted;
		}
		// NOTE: We return the proper length in ptext....
		ptext->Length = bytesDecrypted;
		
		// FIXME - sometimes get mallocd RemData here, but never any valid data
		// there...side effect of CSPFullPluginSession's buffer handling logic;
		// but will we ever actually see valid data in RemData? So far we never
		// have....
		if(remData.Data != NULL) {
			appFree(remData.Data, NULL);
		}
	}
	else {
		printError("CSSM_DecryptData", crtn);
		ocrtn = crtn;
	}
abort:
	crtn = CSSM_DeleteContext(cryptHand);
	if(crtn) {
		printError("CSSM_DeleteContext", crtn);
		ocrtn = crtn;
	}
	return ocrtn;
}

CSSM_RETURN cspStagedDecrypt(CSSM_CSP_HANDLE cspHand,
		uint32 algorithm,					// CSSM_ALGID_FEED, etc.
		uint32 mode,						// CSSM_ALGMODE_CBC, etc. - only for symmetric algs
		CSSM_PADDING padding,				// CSSM_PADDING_PKCS1, etc. 
		const CSSM_KEY *key,				// public or session key
		const CSSM_KEY *pubKey,				// for CSSM_ALGID_FEED, CSSM_ALGID_FEECFILE only
		uint32 effectiveKeySizeInBits,		// 0 means skip this attribute
		uint32 cipherBlockSize,				// ditto
		uint32 rounds,						// ditto
		const CSSM_DATA *iv,				// init vector, optional
		const CSSM_DATA *ctext,
		CSSM_DATA_PTR ptext,				// RETURNED, we malloc
		CSSM_BOOL multiUpdates)				// false:single update, true:multi updates
{
	CSSM_CC_HANDLE 	cryptHand;
	CSSM_RETURN		crtn;
	CSSM_SIZE		bytesDecrypted;			// per update
	CSSM_SIZE		bytesDecryptedTotal = 0;
	CSSM_RETURN		ocrtn = CSSM_OK;		// 'our' crtn
	unsigned		toMove;					// remaining
	unsigned		thisMove;				// bytes to encrypt on this update
	CSSM_DATA		thisCtext;				// running ptr into ptext
	CSSM_DATA		ptextWork;				// per update, mallocd by CSP
	CSSM_QUERY_SIZE_DATA querySize;
	uint8			*origPtext;				// initial ptext->Data
	unsigned		origPtextLen;			// amount we mallocd
	
	cryptHand = genCryptHandle(cspHand, 
		algorithm, 
		mode, 
		padding,
		key, 
		pubKey, 
		iv,
		effectiveKeySizeInBits,
		rounds);
	if(cryptHand == 0) {
		return CSSMERR_CSP_INTERNAL_ERROR;
	}
	if(cipherBlockSize) {
		crtn = AddContextAttribute(cryptHand,
			CSSM_ATTRIBUTE_BLOCK_SIZE,
			sizeof(uint32),
			CAT_Uint32,
			NULL,
			cipherBlockSize);
		if(crtn) {
			printError("CSSM_UpdateContextAttributes", crtn);
			goto abort;
		}
	}
	
	/* obtain total required ciphertext size and block size */
	querySize.SizeInputBlock = ctext->Length;
	crtn = CSSM_QuerySize(cryptHand,
		CSSM_FALSE,						// encrypt
		1,
		&querySize);
	if(crtn) {
		printError("CSSM_QuerySize(1)", crtn);
		ocrtn = crtn;
		goto abort;
	}
	
	/* required ptext size should be independent of number of chunks */
	if(querySize.SizeOutputBlock == 0) {
		printf("***warning: cspStagedDecrypt: plainTextSize unknown; aborting\n");
		ocrtn = CSSMERR_CSP_INTERNAL_ERROR;
		goto abort;
	}
	else {
		// until exit, ptext->Length indicates remaining bytes of usable data in
		// ptext->Data
		ptext->Length = origPtextLen = querySize.SizeOutputBlock;
		ptext->Data   = origPtext    = 
			(uint8 *)appMalloc(origPtextLen, NULL);
		if(ptext->Data == NULL) {
			printf("Insufficient heap space\n");
			ocrtn = CSSMERR_CSP_INTERNAL_ERROR;
			goto abort;
		}
		memset(ptext->Data, 0, ptext->Length);
	}
	
	crtn = CSSM_DecryptDataInit(cryptHand);
	if(crtn) {
		printError("CSSM_DecryptDataInit", crtn);
		ocrtn = crtn;
		goto abort;
	}
	toMove = ctext->Length;
	thisCtext.Data = ctext->Data;
	while(toMove) {
		if(multiUpdates) {
			thisMove = genRand(1, toMove);
		}
		else {
			/* just do one pass thru this loop */
			thisMove = toMove;
		}
		thisCtext.Length = thisMove;
		/* let CSP do the individual mallocs */
		ptextWork.Data = NULL;
		ptextWork.Length = 0;
		soprintf(("*** DecryptDataUpdate: ctextLen 0x%x\n", thisMove));
		crtn = CSSM_DecryptDataUpdate(cryptHand,
			&thisCtext,
			1,
			&ptextWork,
			1,
			&bytesDecrypted);
		if(crtn) {
			printError("CSSM_DecryptDataUpdate", crtn);
			ocrtn = crtn;
			goto abort;
		}
		//
		// NOTE: We return the proper length in ptext....
		ptextWork.Length = bytesDecrypted;
		thisCtext.Data += thisMove;
		toMove         -= thisMove;
		if(bytesDecrypted > ptext->Length) {
			printf("cspStagedDecrypt: ptext overflow!\n");
			ocrtn = CSSMERR_CSP_INTERNAL_ERROR;
			goto abort;
		}
		if(bytesDecrypted != 0) {
			memmove(ptext->Data, ptextWork.Data, bytesDecrypted);
			bytesDecryptedTotal += bytesDecrypted;
			ptext->Data         += bytesDecrypted;
			ptext->Length       -= bytesDecrypted;
		}
		if(ptextWork.Data != NULL) {
			CSSM_FREE(ptextWork.Data);
		}
	}
	/* OK, one more */
	ptextWork.Data = NULL;
	ptextWork.Length = 0;
	crtn = CSSM_DecryptDataFinal(cryptHand, &ptextWork);
	if(crtn) {
		printError("CSSM_DecryptDataFinal", crtn);
		ocrtn = crtn;
		goto abort;
	}
	if(ptextWork.Length != 0) {
		bytesDecryptedTotal += ptextWork.Length;
		if(ptextWork.Length > ptext->Length) {
			printf("cspStagedDecrypt: ptext overflow (2)!\n");
			ocrtn = CSSMERR_CSP_INTERNAL_ERROR;
			goto abort;
		}
		memmove(ptext->Data, ptextWork.Data, ptextWork.Length);
	}
	if(ptextWork.Data) {
		/* this could have gotten mallocd and Length still be zero */
		CSSM_FREE(ptextWork.Data);
	}
	
	/* retweeze ptext */
	ptext->Data   = origPtext;
	ptext->Length = bytesDecryptedTotal;
abort:
	crtn = CSSM_DeleteContext(cryptHand);
	if(crtn) {
		printError("CSSM_DeleteContext", crtn);
		ocrtn = crtn;
	}
	return ocrtn;
}

#pragma mark --------- sign/verify/MAC ---------

/*
 * Signature routines
 * This all-in-one sign op has a special case for RSA keys. If the requested
 * alg is MD5 or SHA1, we'll do a manual digest op followed by raw RSA sign. 
 * Likewise, if it's CSSM_ALGID_DSA, we'll do manual SHA1 digest followed by 
 * raw DSA sign.
 */

CSSM_RETURN cspSign(CSSM_CSP_HANDLE cspHand,
		uint32 algorithm,					// CSSM_ALGID_FEE_MD5, etc.
		CSSM_KEY_PTR key,					// private key
		const CSSM_DATA *text,
		CSSM_DATA_PTR sig)					// RETURNED
{
	CSSM_CC_HANDLE	sigHand;
	CSSM_RETURN		crtn;
	CSSM_RETURN		ocrtn = CSSM_OK;
	const CSSM_DATA	*ptext;
	CSSM_DATA		digest = {0, NULL};
	CSSM_ALGORITHMS	digestAlg = CSSM_ALGID_NONE;

	/* handle special cases for raw sign */
	switch(algorithm) {
		case CSSM_ALGID_SHA1:
			digestAlg = CSSM_ALGID_SHA1;
			algorithm = CSSM_ALGID_RSA;
			break;
		case CSSM_ALGID_MD5:
			digestAlg = CSSM_ALGID_MD5;
			algorithm = CSSM_ALGID_RSA;
			break;
		case CSSM_ALGID_DSA:
			digestAlg = CSSM_ALGID_SHA1;
			algorithm = CSSM_ALGID_DSA;
			break;
		default:
			break;
	}
	if(digestAlg != CSSM_ALGID_NONE) {
		crtn = cspDigest(cspHand,
			digestAlg,
			CSSM_FALSE,			// mallocDigest
			text,
			&digest);
		if(crtn) {
			return crtn;
		}	
		/* sign digest with raw RSA/DSA */
		ptext = &digest;
	}
	else {
		ptext = text;
	}
	crtn = CSSM_CSP_CreateSignatureContext(cspHand,
		algorithm,
		NULL,				// passPhrase
		key,
		&sigHand);
	if(crtn) {
		printError("CSSM_CSP_CreateSignatureContext (1)", crtn);
		return crtn;
	}
	crtn = CSSM_SignData(sigHand,
		ptext,
		1,
		digestAlg,
		sig);
	if(crtn) {
		printError("CSSM_SignData", crtn);
		ocrtn = crtn;
	}
	crtn = CSSM_DeleteContext(sigHand);
	if(crtn) {
		printError("CSSM_DeleteContext", crtn);
		ocrtn = crtn;
	}
	if(digest.Data != NULL) {
		CSSM_FREE(digest.Data);
	}
	return ocrtn;
}

/*
 * Staged sign. Each update does a random number of bytes 'till through.
 */
CSSM_RETURN cspStagedSign(CSSM_CSP_HANDLE cspHand,
		uint32 algorithm,					// CSSM_ALGID_FEE_MD5, etc.
		CSSM_KEY_PTR key,					// private key
		const CSSM_DATA *text,
		CSSM_BOOL multiUpdates,				// false:single update, true:multi updates
		CSSM_DATA_PTR sig)					// RETURNED
{
	CSSM_CC_HANDLE	sigHand;
	CSSM_RETURN		crtn;
	CSSM_RETURN		ocrtn = CSSM_OK;
	unsigned		thisMove;				// this update
	unsigned		toMove;					// total to go
	CSSM_DATA		thisText;				// actaully passed to update
	crtn = CSSM_CSP_CreateSignatureContext(cspHand,
		algorithm,
		NULL,				// passPhrase
		key,
		&sigHand);
	if(crtn) {
		printError("CSSM_CSP_CreateSignatureContext (1)", crtn);
		return crtn;
	}
	crtn = CSSM_SignDataInit(sigHand);
	if(crtn) {
		printError("CSSM_SignDataInit", crtn);
		ocrtn = crtn;
		goto abort;
	}
	toMove = text->Length;
	thisText.Data = text->Data;
	while(toMove) {
		if(multiUpdates) {
			thisMove = genRand(1, toMove);
		}
		else {
			thisMove = toMove;
		}
		thisText.Length = thisMove;
		crtn = CSSM_SignDataUpdate(sigHand,
			&thisText,
			1);
		if(crtn) {
			printError("CSSM_SignDataUpdate", crtn);
			ocrtn = crtn;
			goto abort;
		}
		thisText.Data += thisMove;
		toMove -= thisMove;
	}
	crtn = CSSM_SignDataFinal(sigHand, sig);
	if(crtn) {
		printError("CSSM_SignDataFinal", crtn);
		ocrtn = crtn;
		goto abort;
	}
abort:
	crtn = CSSM_DeleteContext(sigHand);
	if(crtn) {
		printError("CSSM_DeleteContext", crtn);
		ocrtn = crtn;
	}
	return ocrtn;
}

/*
 * This all-in-one verify op has a special case for RSA keys. If the requested
 * alg is MD5 or SHA1, we'll do a manual digest op followed by raw RSA verify.
 * Likewise, if it's CSSM_ALGID_DSA, we'll do manual SHA1 digest followed by 
 * raw DSA sign.
 */ 
 
CSSM_RETURN cspSigVerify(CSSM_CSP_HANDLE cspHand,
		uint32 algorithm,					// CSSM_ALGID_FEE_MD5, etc.
		CSSM_KEY_PTR key,					// public key
		const CSSM_DATA *text,
		const CSSM_DATA *sig,
		CSSM_RETURN expectResult)			// expected result is verify failure
											// CSSM_OK - expect success
{
	CSSM_CC_HANDLE	sigHand;
	CSSM_RETURN		ocrtn = CSSM_OK;
	CSSM_RETURN		crtn;
	const CSSM_DATA	*ptext;
	CSSM_DATA		digest = {0, NULL};
	CSSM_ALGORITHMS	digestAlg = CSSM_ALGID_NONE;
	
	/* handle special cases for raw sign */
	switch(algorithm) {
		case CSSM_ALGID_SHA1:
			digestAlg = CSSM_ALGID_SHA1;
			algorithm = CSSM_ALGID_RSA;
			break;
		case CSSM_ALGID_MD5:
			digestAlg = CSSM_ALGID_MD5;
			algorithm = CSSM_ALGID_RSA;
			break;
		case CSSM_ALGID_DSA:
			digestAlg = CSSM_ALGID_SHA1;
			algorithm = CSSM_ALGID_DSA;
			break;
		default:
			break;
	}
	if(digestAlg != CSSM_ALGID_NONE) {
		crtn = cspDigest(cspHand,
			digestAlg,
			CSSM_FALSE,			// mallocDigest
			text,
			&digest);
		if(crtn) {
			return crtn;
		}	
		/* sign digest with raw RSA/DSA */
		ptext = &digest;
	}
	else {
		ptext = text;
	}
	crtn = CSSM_CSP_CreateSignatureContext(cspHand,
		algorithm,
		NULL,				// passPhrase
		key,
		&sigHand);
	if(crtn) {
		printError("CSSM_CSP_CreateSignatureContext (3)", crtn);
		return crtn;
	}
	
	crtn = CSSM_VerifyData(sigHand,
		ptext,
		1,
		digestAlg,
		sig);
	if(crtn != expectResult) {
		if(!crtn) {
			printf("Unexpected good Sig Verify\n");
		}
		else {
			printError("CSSM_VerifyData", crtn);
		}
		ocrtn = CSSMERR_CSSM_INTERNAL_ERROR;
	}
	crtn = CSSM_DeleteContext(sigHand);
	if(crtn) {
		printError("CSSM_DeleteContext", crtn);
		ocrtn = crtn;
	}
	if(digest.Data != NULL) {
		CSSM_FREE(digest.Data);
	}
	return ocrtn;
}

/*
 * Staged verify. Each update does a random number of bytes 'till through.
 */
CSSM_RETURN cspStagedSigVerify(CSSM_CSP_HANDLE cspHand,
		uint32 algorithm,					// CSSM_ALGID_FEE_MD5, etc.
		CSSM_KEY_PTR key,					// private key
		const CSSM_DATA *text,
		const CSSM_DATA *sig,
		CSSM_BOOL multiUpdates,				// false:single update, true:multi updates
		CSSM_RETURN expectResult)			// expected result is verify failure
											// CSSM_TRUE - expect success
{
	CSSM_CC_HANDLE	sigHand;
	CSSM_RETURN		crtn;
	CSSM_RETURN		ocrtn = CSSM_OK;
	unsigned		thisMove;				// this update
	unsigned		toMove;					// total to go
	CSSM_DATA		thisText;				// actaully passed to update
	crtn = CSSM_CSP_CreateSignatureContext(cspHand,
		algorithm,
		NULL,				// passPhrase
		key,
		&sigHand);
	if(crtn) {
		printError("CSSM_CSP_CreateSignatureContext (4)", crtn);
		return crtn;
	}
	crtn = CSSM_VerifyDataInit(sigHand);
	if(crtn) {
		printError("CSSM_VerifyDataInit", crtn);
		ocrtn = crtn;
		goto abort;
	}
	toMove = text->Length;
	thisText.Data = text->Data;
	while(toMove) {
		if(multiUpdates) {
			thisMove = genRand(1, toMove);
		}
		else {
			thisMove = toMove;
		}
		thisText.Length = thisMove;
		crtn = CSSM_VerifyDataUpdate(sigHand,
			&thisText,
			1);
		if(crtn) {
			printError("CSSM_VerifyDataUpdate", crtn);
			ocrtn = crtn;
			goto abort;
		}
		thisText.Data += thisMove;
		toMove -= thisMove;
	}
	crtn = CSSM_VerifyDataFinal(sigHand, sig);
	if(crtn != expectResult) {
		if(crtn) {
			printError("CSSM_VerifyDataFinal", crtn);
		}
		else {
			printf("Unexpected good Staged Sig Verify\n");
		}
		ocrtn = CSSMERR_CSSM_INTERNAL_ERROR;
	}
abort:
	crtn = CSSM_DeleteContext(sigHand);
	if(crtn) {
		printError("CSSM_DeleteContext", crtn);
		ocrtn = crtn;
	}
	return ocrtn;
}

/*
 * MAC routines
 */
CSSM_RETURN cspGenMac(CSSM_CSP_HANDLE cspHand,
		uint32 algorithm,					// CSSM_ALGID_FEE_MD5, etc.
		CSSM_KEY_PTR key,					// session key
		const CSSM_DATA *text,
		CSSM_DATA_PTR mac)					// RETURNED
{
	CSSM_CC_HANDLE	macHand;
	CSSM_RETURN		crtn;
	CSSM_RETURN		ocrtn = CSSM_OK;
	crtn = CSSM_CSP_CreateMacContext(cspHand,
		algorithm,
		key,
		&macHand);
	if(crtn) {
		printError("CSSM_CSP_CreateMacContext (1)", crtn);
		return crtn;
	}
	crtn = CSSM_GenerateMac(macHand,
		text,
		1,
		mac);
	if(crtn) {
		printError("CSSM_GenerateMac", crtn);
		ocrtn = crtn;
	}
	crtn = CSSM_DeleteContext(macHand);
	if(crtn) {
		printError("CSSM_DeleteContext", crtn);
		ocrtn = crtn;
	}
	return ocrtn;
}

/*
 * Staged generate mac. 
 */
CSSM_RETURN cspStagedGenMac(CSSM_CSP_HANDLE cspHand,
		uint32 algorithm,					// CSSM_ALGID_FEE_MD5, etc.
		CSSM_KEY_PTR key,					// private key
		const CSSM_DATA *text,
		CSSM_BOOL mallocMac,				// if true and digest->Length = 0, we'll 
											//		malloc
		CSSM_BOOL multiUpdates,				// false:single update, true:multi updates
		CSSM_DATA_PTR mac)					// RETURNED
{
	CSSM_CC_HANDLE	macHand;
	CSSM_RETURN		crtn;
	CSSM_RETURN		ocrtn = CSSM_OK;
	unsigned		thisMove;				// this update
	unsigned		toMove;					// total to go
	CSSM_DATA		thisText;				// actaully passed to update
	
	crtn = CSSM_CSP_CreateMacContext(cspHand,
		algorithm,
		key,
		&macHand);
	if(crtn) {
		printError("CSSM_CSP_CreateMacContext (2)", crtn);
		return crtn;
	}

	if(mallocMac && (mac->Length == 0)) {
		/* malloc mac - ask CSP for size */
		CSSM_QUERY_SIZE_DATA	querySize = {0, 0};
		crtn = CSSM_QuerySize(macHand,
			CSSM_TRUE,						// encrypt
			1,
			&querySize);
		if(crtn) {
			printError("CSSM_QuerySize(mac)", crtn);
			ocrtn = crtn;
			goto abort;
		}
		if(querySize.SizeOutputBlock == 0) {
			printf("Unknown mac size\n");
			ocrtn = CSSMERR_CSSM_INTERNAL_ERROR;
			goto abort;
		}
		mac->Data = (uint8 *)appMalloc(querySize.SizeOutputBlock, NULL);
		if(mac->Data == NULL) {
			printf("malloc failure\n");
			ocrtn = CSSMERR_CSSM_MEMORY_ERROR;
			goto abort;
		}
		mac->Length = querySize.SizeOutputBlock;
	}

	crtn = CSSM_GenerateMacInit(macHand);
	if(crtn) {
		printError("CSSM_GenerateMacInit", crtn);
		ocrtn = crtn;
		goto abort;
	}
	toMove = text->Length;
	thisText.Data = text->Data;
	
	while(toMove) {
		if(multiUpdates) {
			thisMove = genRand(1, toMove);
		}
		else {
			thisMove = toMove;
		}
		thisText.Length = thisMove;
		crtn = CSSM_GenerateMacUpdate(macHand,
			&thisText,
			1);
		if(crtn) {
			printError("CSSM_GenerateMacUpdate", crtn);
			ocrtn = crtn;
			goto abort;
		}
		thisText.Data += thisMove;
		toMove -= thisMove;
	}
	crtn = CSSM_GenerateMacFinal(macHand, mac);
	if(crtn) {
		printError("CSSM_GenerateMacFinal", crtn);
		ocrtn = crtn;
		goto abort;
	}
abort:
	crtn = CSSM_DeleteContext(macHand);
	if(crtn) {
		printError("CSSM_DeleteContext", crtn);
		ocrtn = crtn;
	}
	return ocrtn;
}

CSSM_RETURN cspMacVerify(CSSM_CSP_HANDLE cspHand,
		uint32 algorithm,					// CSSM_ALGID_FEE_MD5, etc.
		CSSM_KEY_PTR key,					// public key
		const CSSM_DATA *text,
		const CSSM_DATA_PTR mac,
		CSSM_RETURN expectResult)			// expected result 
											// CSSM_OK - expect success
{
	CSSM_CC_HANDLE	macHand;
	CSSM_RETURN		ocrtn = CSSM_OK;
	CSSM_RETURN		crtn;
	crtn = CSSM_CSP_CreateMacContext(cspHand,
		algorithm,
		key,
		&macHand);
	if(crtn) {
		printError("CSSM_CSP_CreateMacContext (3)", crtn);
		return crtn;
	}
	crtn = CSSM_VerifyMac(macHand,
		text,
		1,
		mac);
	if(crtn != expectResult) {
		if(crtn) {
			printError("CSSM_VerifyMac", crtn);
		}
		else {
			printf("Unexpected good Mac Verify\n");
		}
		ocrtn = CSSMERR_CSSM_INTERNAL_ERROR;
	}
	crtn = CSSM_DeleteContext(macHand);
	if(crtn) {
		printError("CSSM_DeleteContext", crtn);
		ocrtn = crtn;
	}
	return ocrtn;
}

/*
 * Staged mac verify. Each update does a random number of bytes 'till through.
 */
CSSM_RETURN cspStagedMacVerify(CSSM_CSP_HANDLE cspHand,
		uint32 algorithm,					// CSSM_ALGID_FEE_MD5, etc.
		CSSM_KEY_PTR key,					// private key
		const CSSM_DATA *text,
		const CSSM_DATA_PTR mac,
		CSSM_BOOL multiUpdates,				// false:single update, true:multi updates
		CSSM_RETURN expectResult)			// expected result is verify failure
											// CSSM_OK - expect success
{
	CSSM_CC_HANDLE	macHand;
	CSSM_RETURN		crtn;
	CSSM_RETURN		ocrtn = CSSM_OK;
	unsigned		thisMove;				// this update
	unsigned		toMove;					// total to go
	CSSM_DATA		thisText;				// actaully passed to update

	crtn = CSSM_CSP_CreateMacContext(cspHand,
		algorithm,
		key,
		&macHand);
	if(crtn) {
		printError("CSSM_CSP_CreateMacContext (4)", crtn);
		return crtn;
	}
	crtn = CSSM_VerifyMacInit(macHand);
	if(crtn) {
		printError("CSSM_VerifyMacInit", crtn);
		ocrtn = crtn;
		goto abort;
	}
	toMove = text->Length;
	thisText.Data = text->Data;
	
	while(toMove) {
		if(multiUpdates) {
			thisMove = genRand(1, toMove);
		}
		else {
			thisMove = toMove;
		}
		thisText.Length = thisMove;
		crtn = CSSM_VerifyMacUpdate(macHand,
			&thisText,
			1);
		if(crtn) {
			printError("CSSM_VerifyMacUpdate", crtn);
			ocrtn = crtn;
			goto abort;
		}
		thisText.Data += thisMove;
		toMove -= thisMove;
	}
	crtn = CSSM_VerifyMacFinal(macHand, mac);
	if(crtn != expectResult) {
		if(crtn) {
			printError("CSSM_VerifyMacFinal", crtn);
		}
		else {
			printf("Unexpected good Staged Mac Verify\n");
		}
		ocrtn = CSSMERR_CSSM_INTERNAL_ERROR;
	}
abort:
	crtn = CSSM_DeleteContext(macHand);
	if(crtn) {
		printError("CSSM_DeleteContext", crtn);
		ocrtn = crtn;
	}
	return ocrtn;
}

#pragma mark --------- Digest ---------

/*
 * Digest functions
 */
CSSM_RETURN cspDigest(CSSM_CSP_HANDLE cspHand,
		uint32 algorithm,					// CSSM_ALGID_MD5, etc.
		CSSM_BOOL mallocDigest,				// if true and digest->Length = 0, we'll malloc
		const CSSM_DATA *text,
		CSSM_DATA_PTR digest)
{
	CSSM_CC_HANDLE	digestHand;
	CSSM_RETURN		crtn;
	CSSM_RETURN		ocrtn = CSSM_OK;
	
	crtn = CSSM_CSP_CreateDigestContext(cspHand,
		algorithm,
		&digestHand);
	if(crtn) {
		printError("CSSM_CSP_CreateDIgestContext (1)", crtn);
		return crtn;
	}
	if(mallocDigest && (digest->Length == 0)) {
		/* malloc digest - ask CSP for size */
		CSSM_QUERY_SIZE_DATA	querySize = {0, 0};
		crtn = CSSM_QuerySize(digestHand,
			CSSM_FALSE,						// encrypt
			1,
			&querySize);
		if(crtn) {
			printError("CSSM_QuerySize(3)", crtn);
			ocrtn = crtn;
			goto abort;
		}
		if(querySize.SizeOutputBlock == 0) {
			printf("Unknown digest size\n");
			ocrtn = CSSMERR_CSSM_INTERNAL_ERROR;
			goto abort;
		}
		digest->Data = (uint8 *)appMalloc(querySize.SizeOutputBlock, NULL);
		if(digest->Data == NULL) {
			printf("malloc failure\n");
			ocrtn = CSSMERR_CSSM_MEMORY_ERROR;
			goto abort;
		}
		digest->Length = querySize.SizeOutputBlock;
	}
	crtn = CSSM_DigestData(digestHand,
		text,
		1,
		digest);
	if(crtn) {
		printError("CSSM_DigestData", crtn);
		ocrtn = crtn;
	}
abort:
	crtn = CSSM_DeleteContext(digestHand);
	if(crtn) {
		printError("CSSM_DeleteContext", crtn);
		ocrtn = crtn;
	}
	return ocrtn;
}

CSSM_RETURN cspStagedDigest(CSSM_CSP_HANDLE cspHand,
		uint32 algorithm,					// CSSM_ALGID_MD5, etc.
		CSSM_BOOL mallocDigest,				// if true and digest->Length = 0, we'll 
											//		malloc
		CSSM_BOOL multiUpdates,				// false:single update, true:multi updates
		const CSSM_DATA *text,
		CSSM_DATA_PTR digest)
{
	CSSM_CC_HANDLE	digestHand;
	CSSM_RETURN		crtn;
	CSSM_RETURN		ocrtn = CSSM_OK;
	unsigned		thisMove;				// this update
	unsigned		toMove;					// total to go
	CSSM_DATA		thisText;				// actually passed to update
	
	crtn = CSSM_CSP_CreateDigestContext(cspHand,
		algorithm,
		&digestHand);
	if(crtn) {
		printError("CSSM_CSP_CreateDigestContext (2)", crtn);
		return crtn;
	}
	if(mallocDigest && (digest->Length == 0)) {
		/* malloc digest - ask CSP for size */
		CSSM_QUERY_SIZE_DATA	querySize = {0, 0};
		crtn = CSSM_QuerySize(digestHand,
			CSSM_FALSE,						// encrypt
			1,
			&querySize);
		if(crtn) {
			printError("CSSM_QuerySize(4)", crtn);
			ocrtn = crtn;
			goto abort;
		}
		if(querySize.SizeOutputBlock == 0) {
			printf("Unknown digest size\n");
			ocrtn = CSSMERR_CSSM_INTERNAL_ERROR;
			goto abort;
		}
		digest->Data = (uint8 *)appMalloc(querySize.SizeOutputBlock, NULL);
		if(digest->Data == NULL) {
			printf("malloc failure\n");
			ocrtn = CSSMERR_CSSM_MEMORY_ERROR;
			goto abort;
		}
		digest->Length = querySize.SizeOutputBlock;
	}
	crtn = CSSM_DigestDataInit(digestHand);
	if(crtn) {
		printError("CSSM_DigestDataInit", crtn);
		ocrtn = crtn;
		goto abort;
	}
	toMove = text->Length;
	thisText.Data = text->Data;
	while(toMove) {
		if(multiUpdates) {
			thisMove = genRand(1, toMove);
		}
		else {
			thisMove = toMove;
		}
		thisText.Length = thisMove;
		crtn = CSSM_DigestDataUpdate(digestHand,
			&thisText,
			1);
		if(crtn) {
			printError("CSSM_DigestDataUpdate", crtn);
			ocrtn = crtn;
			goto abort;
		}
		thisText.Data += thisMove;
		toMove -= thisMove;
	}
	crtn = CSSM_DigestDataFinal(digestHand, digest);
	if(crtn) {
		printError("CSSM_DigestDataFinal", crtn);
		ocrtn = crtn;
		goto abort;
	}
abort:
	crtn = CSSM_DeleteContext(digestHand);
	if(crtn) {
		printError("CSSM_DeleteContext", crtn);
		ocrtn = crtn;
	}
	return ocrtn;
}

#pragma mark --------- wrap/unwrap ---------

/* wrap key function. */
CSSM_RETURN cspWrapKey(CSSM_CSP_HANDLE cspHand,
	const CSSM_KEY			*unwrappedKey,	
	const CSSM_KEY			*wrappingKey,
	CSSM_ALGORITHMS			wrapAlg,
	CSSM_ENCRYPT_MODE		wrapMode,
	CSSM_KEYBLOB_FORMAT		wrapFormat,			// NONE, PKCS7, PKCS8
	CSSM_PADDING			wrapPad,
	CSSM_DATA_PTR			initVector,			// for some wrapping algs
	CSSM_DATA_PTR			descrData,			// optional 
	CSSM_KEY_PTR			wrappedKey)			// RETURNED
{
	CSSM_CC_HANDLE		ccHand;
	CSSM_RETURN			crtn;
	CSSM_ACCESS_CREDENTIALS	creds;
	
	memset(wrappedKey, 0, sizeof(CSSM_KEY));
	setBadKeyData(wrappedKey);
	memset(&creds, 0, sizeof(CSSM_ACCESS_CREDENTIALS));
	/* special case for NULL wrap - no wrapping key */
	if((wrappingKey == NULL) ||
	   (wrappingKey->KeyHeader.KeyClass == CSSM_KEYCLASS_SESSION_KEY)) {
		crtn = CSSM_CSP_CreateSymmetricContext(cspHand,
				wrapAlg,
				wrapMode,
				&creds,			// passPhrase,
				wrappingKey,
				initVector,
				wrapPad,		// Padding
				0,				// Params
				&ccHand);
	}
	else {
		crtn = CSSM_CSP_CreateAsymmetricContext(cspHand,
				wrapAlg,
				&creds,	
				wrappingKey,
				wrapPad,		// padding
				&ccHand);
		if(crtn) {
			printError("cspWrapKey/CreateContext", crtn);
			return crtn;
		}
		if(initVector) {
			/* manually add IV for CMS. The actual low-level encrypt doesn't
			 * use it (and must ignore it). */
			crtn = AddContextAttribute(ccHand,
				CSSM_ATTRIBUTE_INIT_VECTOR,
				sizeof(CSSM_DATA),
				CAT_Ptr,
				initVector,
				0);
			if(crtn) {
				printError("CSSM_UpdateContextAttributes", crtn);
				return crtn;
			}
		}
	}
	if(crtn) {
		printError("cspWrapKey/CreateContext", crtn);
		return crtn;
	}
	if(wrapFormat != CSSM_KEYBLOB_WRAPPED_FORMAT_NONE) {
		/* only add this attribute if it's not the default */
		CSSM_CONTEXT_ATTRIBUTE attr;
		attr.AttributeType = CSSM_ATTRIBUTE_WRAPPED_KEY_FORMAT;
		attr.AttributeLength = sizeof(uint32);
		attr.Attribute.Uint32 = wrapFormat;
		crtn = CSSM_UpdateContextAttributes(
			ccHand,
			1,
			&attr);
		if(crtn) {
			printError("CSSM_UpdateContextAttributes", crtn);
			return crtn;
		}
	}
	crtn = CSSM_WrapKey(ccHand,
		&creds,
		unwrappedKey,
		descrData,			// DescriptiveData
		wrappedKey);
	if(crtn != CSSM_OK) {
		printError("CSSM_WrapKey", crtn);
	}
	if(CSSM_DeleteContext(ccHand)) {
		printf("CSSM_DeleteContext failure\n");
	}
	return crtn;
}

/* unwrap key function. */
CSSM_RETURN cspUnwrapKey(CSSM_CSP_HANDLE cspHand,
	const CSSM_KEY			*wrappedKey,
	const CSSM_KEY			*unwrappingKey,
	CSSM_ALGORITHMS			unwrapAlg,
	CSSM_ENCRYPT_MODE		unwrapMode,
	CSSM_PADDING 			unwrapPad,
	CSSM_DATA_PTR			initVector,			// for some wrapping algs
	CSSM_KEY_PTR			unwrappedKey,		// RETURNED
	CSSM_DATA_PTR			descrData,			// required
	const char 				*keyLabel,
	unsigned 				keyLabelLen)
{
	CSSM_CC_HANDLE		ccHand;
	CSSM_RETURN			crtn;
	CSSM_DATA			labelData;
	uint32				keyAttr;
	CSSM_ACCESS_CREDENTIALS	creds;
	
	memset(unwrappedKey, 0, sizeof(CSSM_KEY));
	setBadKeyData(unwrappedKey);
	memset(&creds, 0, sizeof(CSSM_ACCESS_CREDENTIALS));
	if((unwrappingKey == NULL) ||
	   (unwrappingKey->KeyHeader.KeyClass == CSSM_KEYCLASS_SESSION_KEY)) {
		crtn = CSSM_CSP_CreateSymmetricContext(cspHand,
				unwrapAlg,
				unwrapMode,
				&creds,
				unwrappingKey,
				initVector,
				unwrapPad,
				0,				// Params
				&ccHand);
	}
	else {
		crtn = CSSM_CSP_CreateAsymmetricContext(cspHand,
				unwrapAlg,
				&creds,			// passPhrase,
				unwrappingKey,
				unwrapPad,		// Padding
				&ccHand);
		if(crtn) {
			printError("cspUnwrapKey/CreateContext", crtn);
			return crtn;
		}
		if(initVector) {
			/* manually add IV for CMS. The actual low-level encrypt doesn't
			 * use it (and must ignore it). */
			crtn = AddContextAttribute(ccHand,
				CSSM_ATTRIBUTE_INIT_VECTOR,
				sizeof(CSSM_DATA),
				CAT_Ptr,
				initVector,
				0);
			if(crtn) {
				printError("CSSM_UpdateContextAttributes", crtn);
				return crtn;
			}
		}
	}
	if(crtn) {
		printError("cspUnwrapKey/CreateContext", crtn);
		return crtn;
	}
	labelData.Data = (uint8 *)keyLabel;
	labelData.Length = keyLabelLen;
	
	/*
	 * New keyAttr - clear some old bits, make sure we ask for ref key
	 */
	keyAttr = wrappedKey->KeyHeader.KeyAttr;
	keyAttr &= ~(CSSM_KEYATTR_ALWAYS_SENSITIVE | CSSM_KEYATTR_NEVER_EXTRACTABLE);
	keyAttr |= CSSM_KEYATTR_RETURN_REF;
	crtn = CSSM_UnwrapKey(ccHand,
		NULL,				// PublicKey
		wrappedKey,
		wrappedKey->KeyHeader.KeyUsage,
		keyAttr,
		&labelData,
		NULL,				// CredAndAclEntry
		unwrappedKey,
		descrData);			// required
	if(crtn != CSSM_OK) {
		printError("CSSM_UnwrapKey", crtn);
	}
	if(CSSM_DeleteContext(ccHand)) {
		printf("CSSM_DeleteContext failure\n");
	}
	return crtn;
}

/*
 * Simple NULL wrap to convert a reference key to a raw key.
 */
CSSM_RETURN cspRefKeyToRaw(
	CSSM_CSP_HANDLE cspHand,
	const CSSM_KEY *refKey,
	CSSM_KEY_PTR rawKey)		// init'd and RETURNED
{
	CSSM_DATA descData = {0, 0};
	
	memset(rawKey, 0, sizeof(CSSM_KEY));
	return cspWrapKey(cspHand,
		refKey,
		NULL,					// unwrappingKey
		CSSM_ALGID_NONE,
		CSSM_ALGMODE_NONE,
		CSSM_KEYBLOB_WRAPPED_FORMAT_NONE,
		CSSM_PADDING_NONE,
		NULL,					// IV
		&descData,
		rawKey);
}

/* 
 * Convert ref key to raw key with specified format.
 */
CSSM_RETURN cspRefKeyToRawWithFormat(
	CSSM_CSP_HANDLE cspHand,
	const CSSM_KEY *refKey,
	CSSM_KEYBLOB_FORMAT format,
	CSSM_KEY_PTR rawKey)		// init'd and RETURNED
{
	memset(rawKey, 0, sizeof(CSSM_KEY));
	CSSM_ATTRIBUTE_TYPE attrType;	
	
	switch(refKey->KeyHeader.KeyClass) {
		case CSSM_KEYCLASS_PUBLIC_KEY:
			attrType = CSSM_ATTRIBUTE_PUBLIC_KEY_FORMAT;
			break;
		case CSSM_KEYCLASS_PRIVATE_KEY:
			attrType = CSSM_ATTRIBUTE_PRIVATE_KEY_FORMAT;
			break;
		case CSSM_KEYCLASS_SESSION_KEY:
			attrType = CSSM_ATTRIBUTE_SYMMETRIC_KEY_FORMAT;
			break;
		default:
			printf("***Unknown key class\n");
			return CSSMERR_CSP_INVALID_KEY;
	}
	
	CSSM_DATA descData = {0, 0};
	CSSM_CC_HANDLE		ccHand;
	CSSM_RETURN			crtn;
//	uint32				keyAttr;
	CSSM_ACCESS_CREDENTIALS	creds;
	
	memset(rawKey, 0, sizeof(CSSM_KEY));
	memset(&creds, 0, sizeof(CSSM_ACCESS_CREDENTIALS));
	crtn = CSSM_CSP_CreateSymmetricContext(cspHand,
				CSSM_ALGID_NONE,
				CSSM_ALGMODE_NONE,
				&creds,
				NULL,			// unwrappingKey
				NULL,			// initVector
				CSSM_PADDING_NONE,
				NULL,			// Reserved
				&ccHand);
	if(crtn) {
		printError("cspRefKeyToRawWithFormat/CreateContext", crtn);
		return crtn;
	}
	
	/* Add the spec for the resulting format */
	crtn = AddContextAttribute(ccHand,
		attrType,
		sizeof(uint32),		
		CAT_Uint32,
		NULL,
		format);

	crtn = CSSM_WrapKey(ccHand,
		&creds,
		refKey,
		&descData,			// DescriptiveData
		rawKey);
	if(crtn != CSSM_OK) {
		printError("CSSM_WrapKey", crtn);
	}
	if(rawKey->KeyHeader.Format != format) {
		printf("***cspRefKeyToRawWithFormat format scewup\n");
		crtn = CSSMERR_CSP_INTERNAL_ERROR;
	}
	if(CSSM_DeleteContext(ccHand)) {
		printf("CSSM_DeleteContext failure\n");
	}
	return crtn;
}

/* unwrap raw key --> ref */
CSSM_RETURN cspRawKeyToRef(
	CSSM_CSP_HANDLE cspHand,
	const CSSM_KEY *rawKey,
	CSSM_KEY_PTR refKey)				// init'd and RETURNED
{
	CSSM_DATA descData = {0, 0};

	memset(refKey, 0, sizeof(CSSM_KEY));
	return cspUnwrapKey(cspHand,
		rawKey,
		NULL,		// unwrappingKey
		CSSM_ALGID_NONE,
		CSSM_ALGMODE_NONE,
		CSSM_PADDING_NONE,
		NULL,		// init vector
		refKey,
		&descData,
		"noLabel",
		7);
}


#pragma mark --------- FEE key/curve support ---------

/*
 * Generate random key size, primeType, curveType for FEE key for specified op.
 *
 * First just enumerate the curves we know about, with ECDSA-INcapable first
 */
 
typedef struct {
	uint32	keySizeInBits;
	uint32 	primeType;				// CSSM_FEE_PRIME_TYPE_xxx
	uint32 	curveType;				// CSSM_FEE_CURVE_TYPE_xxx
} feeCurveParams;

#define FEE_PROTOTYPE_CURVES	0
#if 	FEE_PROTOTYPE_CURVES
/* obsolete as of 4/9/2001 */
static feeCurveParams feeCurves[] = {
	{	31,		CSSM_FEE_PRIME_TYPE_MERSENNE,	CSSM_FEE_CURVE_TYPE_MONTGOMERY },
	{	127,	CSSM_FEE_PRIME_TYPE_MERSENNE,	CSSM_FEE_CURVE_TYPE_MONTGOMERY },
	{	127,	CSSM_FEE_PRIME_TYPE_GENERAL,	CSSM_FEE_CURVE_TYPE_MONTGOMERY },
	#define NUM_NON_ECDSA_CURVES	3
	
	/* start of Weierstrass, IEEE P1363-capable curves */
	{	31,		CSSM_FEE_PRIME_TYPE_MERSENNE,	CSSM_FEE_CURVE_TYPE_WEIERSTRASS },
	{	40,		CSSM_FEE_PRIME_TYPE_FEE,		CSSM_FEE_CURVE_TYPE_WEIERSTRASS },
	{	127,	CSSM_FEE_PRIME_TYPE_MERSENNE,	CSSM_FEE_CURVE_TYPE_WEIERSTRASS },
	{	160,	CSSM_FEE_PRIME_TYPE_FEE,		CSSM_FEE_CURVE_TYPE_WEIERSTRASS },
	{	160,	CSSM_FEE_PRIME_TYPE_GENERAL,	CSSM_FEE_CURVE_TYPE_WEIERSTRASS },
	{	192,	CSSM_FEE_PRIME_TYPE_FEE,		CSSM_FEE_CURVE_TYPE_WEIERSTRASS },
};
#else	/* FEE_PROTOTYPE_CURVES */
static feeCurveParams feeCurves[] = {
	{	31,		CSSM_FEE_PRIME_TYPE_MERSENNE,	CSSM_FEE_CURVE_TYPE_MONTGOMERY },
	{	127,	CSSM_FEE_PRIME_TYPE_MERSENNE,	CSSM_FEE_CURVE_TYPE_MONTGOMERY },
	#define NUM_NON_ECDSA_CURVES	2
	
	/* start of Weierstrass, IEEE P1363-capable curves */
	{	31,		CSSM_FEE_PRIME_TYPE_MERSENNE,	CSSM_FEE_CURVE_TYPE_WEIERSTRASS },
	{	128,	CSSM_FEE_PRIME_TYPE_FEE,		CSSM_FEE_CURVE_TYPE_WEIERSTRASS },
	{	161,	CSSM_FEE_PRIME_TYPE_FEE,		CSSM_FEE_CURVE_TYPE_WEIERSTRASS },
	{	161,	CSSM_FEE_PRIME_TYPE_GENERAL,	CSSM_FEE_CURVE_TYPE_WEIERSTRASS },
	{	192,	CSSM_FEE_PRIME_TYPE_GENERAL,	CSSM_FEE_CURVE_TYPE_WEIERSTRASS },
};
#endif	/* FEE_PROTOTYPE_CURVES */
#define NUM_FEE_CURVES	(sizeof(feeCurves) / sizeof(feeCurveParams))

void randFeeKeyParams(
	CSSM_ALGORITHMS	alg,			// ALGID_FEED, CSSM_ALGID_FEE_MD5, etc.
	uint32			*keySizeInBits,	// RETURNED
	uint32 			*primeType,		// CSSM_FEE_PRIME_TYPE_xxx, RETURNED
	uint32 			*curveType)		// CSSM_FEE_CURVE_TYPE_xxx, RETURNED
{
	unsigned minParams;
	unsigned die;
	feeCurveParams *feeParams;
	
	switch(alg) {
		case CSSM_ALGID_SHA1WithECDSA:
			minParams = NUM_NON_ECDSA_CURVES;
			break;
		default:
			minParams = 0;
			break;
	}
	die = genRand(minParams, (NUM_FEE_CURVES - 1));
	feeParams = &feeCurves[die];
	*keySizeInBits = feeParams->keySizeInBits;
	*primeType = feeParams->primeType;
	*curveType = feeParams->curveType;
}

/*
 * Obtain strings for primeType and curveType.
 */
const char *primeTypeStr(uint32 primeType)
{
	const char *p;
	switch(primeType) {
		case CSSM_FEE_PRIME_TYPE_MERSENNE:
			p = "Mersenne";
			break;
		case CSSM_FEE_PRIME_TYPE_FEE:
			p = "FEE";
			break;
		case CSSM_FEE_PRIME_TYPE_GENERAL:
			p = "General";
			break;
		case CSSM_FEE_PRIME_TYPE_DEFAULT:
			p = "Default";
			break;
		default:
			p = "***UNKNOWN***";
			break;
	}
	return p;
}

const char *curveTypeStr(uint32 curveType)
{
	const char *c;
	switch(curveType) {
		case CSSM_FEE_CURVE_TYPE_DEFAULT:
			c = "Default";
			break;
		case CSSM_FEE_CURVE_TYPE_MONTGOMERY:
			c = "Montgomery";
			break;
		case CSSM_FEE_CURVE_TYPE_WEIERSTRASS:
			c = "Weierstrass";
			break;
		default:
			c = "***UNKNOWN***";
			break;
	}
	return c;
}

/*
 * Perform FEE Key exchange via CSSM_DeriveKey. 
 */
#if 0
/* Not implemented in OS X */
CSSM_RETURN cspFeeKeyExchange(CSSM_CSP_HANDLE cspHand,
	CSSM_KEY_PTR 	privKey,
	CSSM_KEY_PTR 	pubKey,
	CSSM_KEY_PTR 	derivedKey,		// mallocd by caller
	
	/* remaining fields apply to derivedKey */
	uint32 			keyAlg,
	const char 		*keyLabel,
	unsigned 		keyLabelLen,
	uint32 			keyUsage,		// CSSM_KEYUSE_ENCRYPT, etc.
	uint32 			keySizeInBits)
{
	CSSM_CC_HANDLE	dkHand;
	CSSM_RETURN 	crtn;
	CSSM_DATA		labelData;
	
	if(derivedKey == NULL) {
		printf("cspFeeKeyExchange: no derivedKey\n");
		return CSSMERR_CSSM_INTERNAL_ERROR;
	}
	if((pubKey == NULL) ||
	   (pubKey->KeyHeader.KeyClass != CSSM_KEYCLASS_PUBLIC_KEY) ||
	   (pubKey->KeyHeader.BlobType != CSSM_KEYBLOB_RAW)) {
	 	printf("cspFeeKeyExchange: bad pubKey\n");
	 	return CSSMERR_CSSM_INTERNAL_ERROR;
	}
	if((privKey == NULL) ||
	   (privKey->KeyHeader.KeyClass != CSSM_KEYCLASS_PRIVATE_KEY) ||
	   (privKey->KeyHeader.BlobType != CSSM_KEYBLOB_REFERENCE)) {
	 	printf("cspFeeKeyExchange: bad privKey\n");
	 	return CSSMERR_CSSM_INTERNAL_ERROR;
	}
	memset(derivedKey, 0, sizeof(CSSM_KEY));
	
	crtn = CSSM_CSP_CreateDeriveKeyContext(cspHand,
		CSSM_ALGID_FEE_KEYEXCH,			// AlgorithmID
		keyAlg,							// alg of the derived key
		keySizeInBits,
		NULL,							// access creds
		// FIXME
		0,								// IterationCount
		NULL,							// Salt
		NULL,							// Seed
		NULL);							// PassPhrase
	if(dkHand == 0) {
		printError("CSSM_CSP_CreateDeriveKeyContext");
		return CSSM_FAIL;
	} 
	labelData.Length = keyLabelLen;
	labelData.Data = (uint8 *)keyLabel;
	crtn = CSSM_DeriveKey(dkHand,
		privKey,
		&pubKey->KeyData,		// Param - pub key blob
		keyUsage,
		CSSM_KEYATTR_RETURN_REF | CSSM_KEYATTR_EXTRACTABLE |
				  CSSM_KEYATTR_SENSITIVE,
		&labelData,
		derivedKey);
	
	/* FIXME - save/restore error */
	CSSM_DeleteContext(dkHand);
	if(crtn) {
		printError("CSSM_DeriveKey");
	}
	return crtn;
}
#endif

#pragma mark --------- Key/DL/DB support ---------

/*
 * Add a DL/DB handle to a crypto context.
 */
CSSM_RETURN cspAddDlDbToContext(
	CSSM_CC_HANDLE ccHand,
	CSSM_DL_HANDLE dlHand,
	CSSM_DB_HANDLE dbHand)
{
	CSSM_DL_DB_HANDLE dlDb = { dlHand, dbHand };
	return AddContextAttribute(ccHand, 
		CSSM_ATTRIBUTE_DL_DB_HANDLE,
		sizeof(CSSM_ATTRIBUTE_DL_DB_HANDLE),
		CAT_Ptr,
		&dlDb,
		0);
}
	
/* 
 * Common routine to do a basic DB lookup by label and key type.
 * Query is aborted prior to exit.
 */
static CSSM_DB_UNIQUE_RECORD_PTR dlLookup(
	CSSM_DL_DB_HANDLE	dlDbHand,
	const CSSM_DATA		*keyLabel,
	CT_KeyType 			keyType,
	CSSM_HANDLE 		*resultHand,			// RETURNED
	CSSM_DATA_PTR		theData,				// RETURED
	CSSM_DB_RECORDTYPE	*recordType)			// RETURNED
{
	CSSM_QUERY						query;
	CSSM_SELECTION_PREDICATE		predicate;
	CSSM_DB_UNIQUE_RECORD_PTR		record = NULL;
	CSSM_RETURN						crtn;
	
	switch(keyType) {
		case CKT_Public:
			query.RecordType = *recordType = CSSM_DL_DB_RECORD_PUBLIC_KEY;
			break;
		case CKT_Private:
			query.RecordType = *recordType = CSSM_DL_DB_RECORD_PRIVATE_KEY;
			break;
		case CKT_Session:
			query.RecordType = *recordType = CSSM_DL_DB_RECORD_SYMMETRIC_KEY;
			break;
		default:
			printf("Hey bozo! Give me a valid key type!\n");
			return NULL;
	}
	query.Conjunctive = CSSM_DB_NONE;
	query.NumSelectionPredicates = 1;
	predicate.DbOperator = CSSM_DB_EQUAL;
	
	predicate.Attribute.Info.AttributeNameFormat = 
		CSSM_DB_ATTRIBUTE_NAME_AS_STRING;
	predicate.Attribute.Info.Label.AttributeName = (char *) "Label";
	predicate.Attribute.Info.AttributeFormat = CSSM_DB_ATTRIBUTE_FORMAT_BLOB;
	/* hope this cast is OK */
	predicate.Attribute.Value = (CSSM_DATA_PTR)keyLabel;
	query.SelectionPredicate = &predicate;
	
	query.QueryLimits.TimeLimit = 0;	// FIXME - meaningful?
	query.QueryLimits.SizeLimit = 1;	// FIXME - meaningful?
	query.QueryFlags = CSSM_QUERY_RETURN_DATA;	// FIXME - used?
	
	crtn = CSSM_DL_DataGetFirst(dlDbHand,
		&query,
		resultHand,
		NULL,
		theData,
		&record);
	/* abort only on success */
	if(crtn == CSSM_OK) {
		crtn = CSSM_DL_DataAbortQuery(dlDbHand, *resultHand);
		if(crtn) {
			printError("CSSM_DL_AbortQuery", crtn);
			return NULL;
		}
	}
	return record;
}

/*
 * Look up a key by label and type.
 */
CSSM_KEY_PTR cspLookUpKeyByLabel(
	CSSM_DL_HANDLE dlHand, 
	CSSM_DB_HANDLE dbHand, 
	const CSSM_DATA *labelData, 
	CT_KeyType keyType)
{
	CSSM_DB_UNIQUE_RECORD_PTR	record;
	CSSM_HANDLE					resultHand;
	CSSM_DATA					theData;
	CSSM_KEY_PTR				key;
	CSSM_DB_RECORDTYPE 			recordType;
	CSSM_DL_DB_HANDLE			dlDbHand;
	
	dlDbHand.DLHandle = dlHand;
	dlDbHand.DBHandle = dbHand;
	
	theData.Length = 0;
	theData.Data = NULL;
	
	record = dlLookup(dlDbHand,
		labelData,
		keyType,
		&resultHand,
		&theData,
		&recordType);
	if(record == NULL) {
		//printf("cspLookUpKeyByLabel: key not found\n");
		return NULL;
	}
	key = (CSSM_KEY_PTR)theData.Data;
	CSSM_DL_FreeUniqueRecord(dlDbHand, record);
	return key;
}

/*
 * Delete and free a key 
 */
CSSM_RETURN cspDeleteKey(
	CSSM_CSP_HANDLE		cspHand,		// for free
	CSSM_DL_HANDLE		dlHand,			// for delete
	CSSM_DB_HANDLE		dbHand,			// ditto
	const CSSM_DATA 	*labelData, 
	CSSM_KEY_PTR		key)
{
	CSSM_DB_UNIQUE_RECORD_PTR	record;
	CSSM_HANDLE					resultHand;
	CT_KeyType					keyType;
	CSSM_RETURN					crtn = CSSM_OK;
	CSSM_DB_RECORDTYPE 			recordType;
	CSSM_DL_DB_HANDLE			dlDbHand;
	
	if(key->KeyHeader.KeyAttr & CSSM_KEYATTR_PERMANENT) {
		/* first do a lookup based in this key's fields */
		switch(key->KeyHeader.KeyClass) {
			case CSSM_KEYCLASS_PUBLIC_KEY:
				keyType = CKT_Public;
				break;
			case CSSM_KEYCLASS_PRIVATE_KEY:
				keyType = CKT_Private;
				break;
			case CSSM_KEYCLASS_SESSION_KEY:
				keyType = CKT_Session;
				break;
			default:
				printf("Hey bozo! Give me a valid key type!\n");
				return -1;
		}

		dlDbHand.DLHandle = dlHand;
		dlDbHand.DBHandle = dbHand;
		
		record = dlLookup(dlDbHand,
			labelData,
			keyType,
			&resultHand,
			NULL,			// don't want actual data
			&recordType);
		if(record == NULL) {
			printf("cspDeleteKey: key not found in DL\n");
			return CSSMERR_DL_RECORD_NOT_FOUND;
		}
		
		/* OK, nuke it */
		crtn = CSSM_DL_DataDelete(dlDbHand, record);
		if(crtn) {
			printError("CSSM_DL_DataDelete", crtn);
		}
		CSSM_DL_FreeUniqueRecord(dlDbHand, record);
	}
		
	/* CSSM_FreeKey() should fail due to the delete, but it will
	 * still free KeyData....
	 * FIXME - we should be able to do this in this one single call - right?
	 */
	CSSM_FreeKey(cspHand, NULL, key, CSSM_FALSE);

	return crtn;
}

/*
 * Given any key in either blob or reference format,
 * obtain the associated SHA-1 hash. 
 */
CSSM_RETURN cspKeyHash(
	CSSM_CSP_HANDLE		cspHand,	
	const CSSM_KEY_PTR	key,			/* public key */
	CSSM_DATA_PTR		*hashData)		/* hash mallocd and RETURNED here */
{
	CSSM_CC_HANDLE		ccHand;
	CSSM_RETURN			crtn;
	CSSM_DATA_PTR		dp;
	
	*hashData = NULL;
	
	/* validate input params */
	if((key == NULL) ||
	   (hashData == NULL)) {
	   	printf("cspKeyHash: bogus args\n");
		return CSSMERR_CSSM_INTERNAL_ERROR;				
	}
	
	/* cook up a context for a passthrough op */
	crtn = CSSM_CSP_CreatePassThroughContext(cspHand,
	 	key,
		&ccHand);
	if(ccHand == 0) {
		printError("CSSM_CSP_CreatePassThroughContext", crtn);
		return crtn;
	}
	
	/* now it's up to the CSP */
	crtn = CSSM_CSP_PassThrough(ccHand,
		CSSM_APPLECSP_KEYDIGEST,
		NULL,
		(void **)&dp);
	if(crtn) {
		printError("CSSM_CSP_PassThrough(PUBKEYHASH)", crtn);
	}
	else {
		*hashData = dp;
		crtn = CSSM_OK;
	}
	CSSM_DeleteContext(ccHand);
	return crtn;
}


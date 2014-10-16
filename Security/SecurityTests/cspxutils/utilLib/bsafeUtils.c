/*
 * bsafeUtils.c - common routines for CDSA/BSAFE compatibility testing
 */


#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
//#include <security_bsafe/bsafe.h>
//#include <security_bsafe/aglobal.h>
#include "bsafeUtils.h"
#include <Security/cssmerr.h>
#include "common.h"

/*
 * Convert between BSAFE ITEM and CSSM_DATA
 */
#if 0
static inline void buItemToCssmData(
	const ITEM 		*item,
	CSSM_DATA_PTR	cdata)
{
	cdata->Data   = item->data;
	cdata->Length = item->len;
}

static inline void buCssmDataToItem(
	const CSSM_DATA		*cdata,
	ITEM 				*item)
{
	item->data = cdata->Data;
	item->len  = cdata->Length;
}

/*
 * BSafe's Chooser table - all we'll ever need.
 */
/*static*/ B_ALGORITHM_METHOD *BSAFE_ALGORITHM_CHOOSER[] = {
    // digests
    &AM_SHA,
    &AM_MD5,
	&AM_MD2,

    // organizational
    &AM_CBC_ENCRYPT,
    &AM_CBC_DECRYPT,
    &AM_ECB_ENCRYPT,
    &AM_ECB_DECRYPT,
    &AM_OFB_ENCRYPT,
    &AM_OFB_DECRYPT,

    // DES & variants
    &AM_DES_ENCRYPT,
    &AM_DES_DECRYPT,
    &AM_DESX_ENCRYPT,
    &AM_DESX_DECRYPT,
    &AM_DES_EDE_ENCRYPT,
    &AM_DES_EDE_DECRYPT,

    // RCn stuff
    &AM_RC2_CBC_ENCRYPT,
    &AM_RC2_CBC_DECRYPT,
    &AM_RC2_ENCRYPT,
    &AM_RC2_DECRYPT,
    &AM_RC4_ENCRYPT,
    &AM_RC4_DECRYPT,
    &AM_RC5_ENCRYPT,
    &AM_RC5_DECRYPT,
    &AM_RC5_CBC_ENCRYPT,
    &AM_RC5_CBC_DECRYPT,

    // RSA
    &AM_RSA_STRONG_KEY_GEN,
    &AM_RSA_KEY_GEN,
    &AM_RSA_CRT_ENCRYPT_BLIND,
    &AM_RSA_CRT_DECRYPT_BLIND,
    &AM_RSA_ENCRYPT,
    &AM_RSA_DECRYPT,

    // DSA
    &AM_DSA_PARAM_GEN,
    &AM_DSA_KEY_GEN,

    // signatures
    &AM_DSA_SIGN,
    &AM_DSA_VERIFY,

    // random number generation
    &AM_MD5_RANDOM,
    &AM_SHA_RANDOM,

    // sentinel
    (B_ALGORITHM_METHOD *)NULL_PTR
};

/* 
 * Convert a BSAFE return to a CSSM error and optionally print the error msg with 
 * the op in which the error occurred.
 */
static CSSM_RETURN buBsafeErrToCssm(
	int brtn, 
	const char *op)
{
	const char *errStr = NULL;
	CSSM_RETURN crtn;
	
    switch (brtn) {
		case 0:
			return CSSM_OK;
		case BE_ALLOC:
			crtn = CSSMERR_CSSM_MEMORY_ERROR;
			errStr = "BE_ALLOC";
			break;
		case BE_SIGNATURE:
			crtn = CSSMERR_CSP_VERIFY_FAILED;
			errStr = "BE_SIGNATURE";
			break;
		case BE_OUTPUT_LEN:
			crtn = CSSMERR_CSP_OUTPUT_LENGTH_ERROR;
			errStr = "BE_OUTPUT_LEN";
			break;
		case BE_INPUT_LEN:
			crtn = CSSMERR_CSP_INPUT_LENGTH_ERROR;
			errStr = "BE_INPUT_LEN";
			break;
		case BE_EXPONENT_EVEN:
			crtn = CSSMERR_CSP_INVALID_KEY;
			errStr = "BE_EXPONENT_EVEN";
			break;
		case BE_EXPONENT_LEN:
			crtn = CSSMERR_CSP_INVALID_KEY;
			errStr = "BE_EXPONENT_LEN";
			break;
		case BE_EXPONENT_ONE:
			crtn = CSSMERR_CSP_INVALID_KEY;
			errStr = "BE_EXPONENT_ONE";
			break;
		case BE_DATA:
			crtn = CSSMERR_CSP_INVALID_DATA;
			errStr = "BE_DATA";
			break;
		case BE_INPUT_DATA:
			crtn = CSSMERR_CSP_INVALID_DATA;
			errStr = "BE_INPUT_DATA";
			break;
		case BE_WRONG_KEY_INFO:
			crtn = CSSMERR_CSP_INVALID_KEY;
			errStr = "BE_WRONG_KEY_INFO";
			break;
        default:
			//@@@ translate BSafe errors intelligently
 			crtn = CSSM_ERRCODE_INTERNAL_ERROR;
			errStr = "Other BSAFE error";
			break;
    }
	if(op != NULL) {
		printf("%s: BSAFE error %d (%s)\n", op, brtn, errStr);
	}
	return crtn;
}

/*
 * Non-thread-safe global random B_ALGORITHM_OBJ and a reusable init for it.
 */
static B_ALGORITHM_OBJ 	bsafeRng = NULL;
#define BSAFE_RANDSIZE	64

static B_ALGORITHM_OBJ buGetRng()
{
	int brtn;
	uint8 seed[BSAFE_RANDSIZE];
	
	if(bsafeRng != NULL) {
		return bsafeRng;
	}
	brtn = B_CreateAlgorithmObject(&bsafeRng);
	if(brtn) {
		buBsafeErrToCssm(brtn, "B_CreateAlgorithmObject(&bsafeRng)");
		return NULL;
	}
	brtn = B_SetAlgorithmInfo(bsafeRng, AI_X962Random_V0, NULL_PTR);
	if(brtn) {
		buBsafeErrToCssm(brtn, "B_SetAlgorithmInfo(bsafeRng)");
		return NULL;
	}
	brtn = B_RandomInit(bsafeRng, BSAFE_ALGORITHM_CHOOSER, NULL);
 	if(brtn) {
		buBsafeErrToCssm(brtn, "B_SetAlgorithmInfo(bsafeRng)");
		return NULL;
	}
	appGetRandomBytes(seed, BSAFE_RANDSIZE);
	brtn = B_RandomUpdate(bsafeRng, seed, BSAFE_RANDSIZE, NULL);
	if(brtn) {
		buBsafeErrToCssm(brtn, "B_RandomUpdate");
		return NULL;
	}
	return bsafeRng;
}
#endif

/*
 * Create a symmetric key.
 */
CSSM_RETURN  buGenSymKey(
	uint32			keySizeInBits,
	const CSSM_DATA	*keyData,
	BU_KEY			*key)			// RETURNED
{
#if 0
	int				brtn;
	B_KEY_OBJ		bkey = NULL;
	ITEM			item;
	unsigned		keyBytes = (keySizeInBits + 7) / 8;
	
	if(keyBytes > keyData->Length) {
		/* note it's OK to give us too much key data */
		printf("***buGenSymKey: Insufficient keyData\n");
		return CSSM_ERRCODE_INTERNAL_ERROR;
	}

	/* create a BSAFE key */
	brtn = B_CreateKeyObject(&bkey);
	if(brtn) {
		return buBsafeErrToCssm(brtn, "B_CreateKeyObject");
	}
	
	/* assign data to the key */
	item.data = keyData->Data;
	item.len = keyBytes;
	brtn = B_SetKeyInfo(bkey, KI_Item, (POINTER)&item);
	if(brtn) {
		return buBsafeErrToCssm(brtn, "B_SetKeyInfo");
	}
	else {
		*key = bkey;
		return CSSM_OK;
	}
#endif
    return 0;
}

/*
 * Create asymmetric key pair.
 * FIXME - additional params (e.g. DSA params, RSA exponent)?
 */
CSSM_RETURN buGenKeyPair(
	uint32			keySizeInBits,
	CSSM_ALGORITHMS	keyAlg,			// CSSM_ALGID_{RSA,DSA}
	BU_KEY			*pubKey,		// RETURNED
	BU_KEY			*privKey)		// RETURNED
{
#if 0 // NO MORE BSAFE
	int						brtn;
	B_KEY_OBJ				bPubkey = NULL;
	B_KEY_OBJ				bPrivkey = NULL;
	B_ALGORITHM_OBJ			keypairGen = NULL;
	const char				*op = NULL;
	A_RSA_KEY_GEN_PARAMS	params;
	unsigned char 			exp[1] = { 3 };
    B_ALGORITHM_OBJ 		genDsaAlg = NULL;
    B_ALGORITHM_OBJ 		dsaResult = NULL;
	B_DSA_PARAM_GEN_PARAMS 	dsaParams;
	A_DSA_PARAMS 			*kParams = NULL;
	
	/* create algorithm object */
	brtn = B_CreateAlgorithmObject(&keypairGen);
	if(brtn) {
		return CSSMERR_CSSM_MEMORY_ERROR;
	}
	
	/* create two BSAFE keys */
	brtn = B_CreateKeyObject(&bPubkey);
	if(brtn) {
		op ="B_CreateKeyObject";
		goto abort;
	}
	brtn = B_CreateKeyObject(&bPrivkey);
	if(brtn) {
		op ="B_CreateKeyObject";
		goto abort;
	}
	switch(keyAlg) {
		case CSSM_ALGID_RSA:
		{
			/* set RSA-specific params */
			params.modulusBits = keySizeInBits;
			/* hack - parameterize? */
			params.publicExponent.data = exp;
			params.publicExponent.len = 1;
			brtn = B_SetAlgorithmInfo(keypairGen, AI_RSAKeyGen, 
				(POINTER)&params);
			if(brtn) {
				op ="B_SetAlgorithmInfo(AI_RSAKeyGen)";
			}
			break;
		}
		case CSSM_ALGID_DSA:	
		{
			/* jump through hoops generating parameters */
			brtn = B_CreateAlgorithmObject(&genDsaAlg);
			if(brtn) {
				op ="B_CreateAlgorithmObject";
				break;
			}
			dsaParams.primeBits = keySizeInBits;
        	brtn = B_SetAlgorithmInfo(genDsaAlg, AI_DSAParamGen, (POINTER)&dsaParams);
			if(brtn) {
				op = "B_SetAlgorithmInfo(AI_DSAParamGen)";
				break;
			}
			brtn = B_GenerateInit(genDsaAlg, BSAFE_ALGORITHM_CHOOSER, NULL);
			if(brtn) {
				op = "B_GenerateInit(AI_DSAParamGen)";
				break;
			}
        	brtn = B_CreateAlgorithmObject(&dsaResult);
			if(brtn) {
				op = "B_CreateAlgorithmObject";
				break;
			}
        	brtn = B_GenerateParameters(genDsaAlg, dsaResult, buGetRng(), NULL);
			if(brtn) {
				op = "B_GenerateParameters";
				break;
			}
			
			/* dsaResult now has the parameters, which we must extract and then
			 * apply to the keypairGen object. Cool, huh? */
			brtn = B_GetAlgorithmInfo((POINTER *)&kParams, dsaResult, AI_DSAKeyGen);
			if(brtn) {
				op = "B_GetAlgorithmInfo(AI_DSAKeyGen)";
				break;
			}
			brtn = B_SetAlgorithmInfo(keypairGen, AI_DSAKeyGen, (POINTER)kParams);
			if(brtn) {
				op ="B_SetAlgorithmInfo(AI_DSAKeyGen)";
			}
			break;
		}
		default:
			printf("buGenKeyPair: algorithm not supported\n");
			return CSSMERR_CSSM_FUNCTION_NOT_IMPLEMENTED;
	}
	if(brtn) {
		goto abort;
	}
	
	/* keypairGen all set to go. */
	brtn = B_GenerateInit(keypairGen, 
		BSAFE_ALGORITHM_CHOOSER,
		(A_SURRENDER_CTX *)NULL);
	if(brtn) {
		op = "B_GenerateInit";
		goto abort;
	}
	brtn = B_GenerateKeypair(keypairGen,
		bPubkey,
		bPrivkey,
		buGetRng(),
		NULL);
	if(brtn) {
		op = "B_GenerateInit";
	}
abort:
	B_DestroyAlgorithmObject(&keypairGen);
	B_DestroyAlgorithmObject(&genDsaAlg);
	B_DestroyAlgorithmObject(&dsaResult);
	if(brtn) {
		B_DestroyKeyObject(&bPubkey);
		B_DestroyKeyObject(&bPrivkey);
		return buBsafeErrToCssm(brtn, op);
	}
	else {
		*pubKey = bPubkey;
		*privKey = bPrivkey;
		return CSSM_OK;
	}
#endif
	return CSSM_OK;
}

/*
 * Free a key created in buGenSymKey or buGenKeyPair
 */
CSSM_RETURN buFreeKey(
	BU_KEY			key)
{
#if 0 // NO MORE BSAFE
	B_KEY_OBJ bkey = (B_KEY_OBJ)key;
	B_DestroyKeyObject(&bkey);
#endif
	return CSSM_OK;
}

/*
 * encrypt/decrypt
 */
CSSM_RETURN buEncryptDecrypt(
	BU_KEY				key,
	CSSM_BOOL			forEncrypt,
	CSSM_ALGORITHMS		encrAlg,
	CSSM_ENCRYPT_MODE	mode,				// CSSM_ALGMODE_CBC, etc.
	const CSSM_DATA		*iv,				//Êoptional per mode
	uint32				effectiveKeyBits,	// optional per key alg (actually just RC2)
											// for RSA, key size in bits
	uint32				rounds,				// optional, RC5 only
	const CSSM_DATA		*inData,
	CSSM_DATA_PTR		outData)			// mallocd and RETURNED
{
#if 0 // NO MORE BSAFE
	B_ALGORITHM_OBJ		alg;
	int 				brtn;
	char				fbCipher = 1;
	uint32				blockSize = 0;
	unsigned			outBufLen;
	unsigned			bytesMoved;
	CSSM_RETURN			crtn;
	char				useIv;
	
	// these variables are used in the switch below and need to 
	// live until after setAlgorithm()
	ITEM	 			bsIv;
    B_BLK_CIPHER_W_FEEDBACK_PARAMS spec;
	A_RC5_PARAMS		rc5Params;
	A_RC2_PARAMS		rc2Params;
	
	brtn = B_CreateAlgorithmObject(&alg);
	if(brtn) {
		return buBsafeErrToCssm(brtn, "B_CreateAlgorithmObject");
	}
	
	/* per-alg setup */
	switch(encrAlg) {
		case CSSM_ALGID_RC4:
			/* the easy one */
			brtn = B_SetAlgorithmInfo(alg, AI_RC4, NULL);
			if(brtn) {
				crtn = buBsafeErrToCssm(brtn, "B_SetAlgorithmInfo");
				goto abort;
			}
			fbCipher = 0;
			break;
			
		case CSSM_ALGID_RSA:
			/* assume encrypt via publicm decrypt via private */
			if(forEncrypt) {
				brtn = B_SetAlgorithmInfo(alg, AI_PKCS_RSAPublic, NULL);
			}
			else {
				brtn = B_SetAlgorithmInfo(alg, AI_PKCS_RSAPrivate, NULL);
			}
			if(brtn) {
				crtn = buBsafeErrToCssm(brtn, "B_SetAlgorithmInfo(RSA)");
				goto abort;
			}
			blockSize = (effectiveKeyBits + 7) / 8;
			fbCipher = 0;
			break;
			
		/* common code using AI_FeebackCipher */
        case CSSM_ALGID_DES:
            spec.encryptionMethodName = (POINTER)"des";
			blockSize = 8;
            break;
        case CSSM_ALGID_DESX:
            spec.encryptionMethodName = (POINTER)"desx";
 			blockSize = 8;
			break;
        case CSSM_ALGID_3DES_3KEY_EDE:
            spec.encryptionMethodName = (POINTER)"des_ede";
			blockSize = 8;
            break;
        case CSSM_ALGID_RC5:
            spec.encryptionMethodName = (POINTER)"rc5";
			spec.encryptionParams = (POINTER)&rc5Params;
			rc5Params.version = 0x10;
			rc5Params.rounds = rounds;
			rc5Params.wordSizeInBits = 32;
			blockSize = 8;
            break;
        case CSSM_ALGID_RC2:
            spec.encryptionMethodName = (POINTER)"rc2";
			spec.encryptionParams = (POINTER)&rc2Params;
			rc2Params.effectiveKeyBits = effectiveKeyBits;
 			blockSize = 8;
           break;
		/* add other non-AI_FeebackCipher algorithms here */
		default:
			printf("buEncryptDecrypt: unknown algorithm\n");
			return CSSM_ERRCODE_INTERNAL_ERROR;
	}
	if(fbCipher) {
		useIv = 1;		// default, except for ECB
		switch(mode) {
			case CSSM_ALGMODE_CBCPadIV8:
				spec.feedbackMethodName = (POINTER)"cbc";
				spec.paddingMethodName = (POINTER)"pad";
				break;
			case CSSM_ALGMODE_CBC_IV8: 
				spec.feedbackMethodName = (POINTER)"cbc";
				spec.paddingMethodName = (POINTER)"nopad";
				break;
			case CSSM_ALGMODE_OFB_IV8: 
				spec.feedbackMethodName = (POINTER)"cbc";
				spec.paddingMethodName = (POINTER)"nopad";
				break;
			case CSSM_ALGMODE_ECB: 
				/* this does not seem to work yet - need info from 
				 * RSA. Specify block size as the feedbackParams (per manual)
				 * and get a memmove error trying to copy from address 8; specify
				 * an IV and get BSAFE error 524 (BE_INPUT_DATA) error on the
				 * EncryptInit.
				 */
				spec.feedbackMethodName = (POINTER)"ecb";
				spec.paddingMethodName = (POINTER)"nopad";
				//useIv = 0;
				//spec.feedbackParams = (POINTER)8;
				break;
			default:
				printf("buEncryptDecrypt: unknown mode\n");
				return CSSM_ERRCODE_INTERNAL_ERROR;
		}
		if(useIv && (iv != NULL)) {
			buCssmDataToItem(iv, &bsIv);
			spec.feedbackParams = (POINTER)&bsIv;
		}
		
		brtn = B_SetAlgorithmInfo(alg, AI_FeedbackCipher, (POINTER)&spec);
		if(brtn) {
			crtn = buBsafeErrToCssm(brtn, "B_SetAlgorithmInfo");
			goto abort;
		}
	}
	
	/*
	 * OK, one way or another we have an algorithm object. Set up
	 * output buffer.
	 */
	if(forEncrypt) {
		outBufLen = inData->Length + blockSize;
	}
	else {
		outBufLen = inData->Length;
	}
	outData->Length = 0;
	outData->Data = NULL;
	crtn = appSetupCssmData(outData, outBufLen);
	if(crtn) {
		goto abort;
	}
	if(forEncrypt) {
		brtn = B_EncryptInit(alg, 
			(B_KEY_OBJ)key,
			BSAFE_ALGORITHM_CHOOSER,
			(A_SURRENDER_CTX *)NULL);
		if(brtn) {
			crtn = buBsafeErrToCssm(brtn, "B_EncryptInit");
			goto abort;
		}
		brtn = B_EncryptUpdate(alg,
			outData->Data,
			&bytesMoved,
			outBufLen,
			inData->Data,
			inData->Length,
			buGetRng(),		// randAlg
			NULL);			// surrender
		if(brtn) {
			crtn = buBsafeErrToCssm(brtn, "B_EncryptInit");
			goto abort;
		}
		outData->Length = bytesMoved;
		brtn = B_EncryptFinal(alg,
			outData->Data + bytesMoved,
			&bytesMoved,
			outBufLen - outData->Length,
			buGetRng(),		// randAlg
			NULL);			// surrender
		if(brtn) {
			crtn = buBsafeErrToCssm(brtn, "B_EncryptFinal");
			goto abort;
		}
		outData->Length += bytesMoved;
		crtn = CSSM_OK;
	}
	else {
		brtn = B_DecryptInit(alg, 
			(B_KEY_OBJ)key,
			BSAFE_ALGORITHM_CHOOSER,
			(A_SURRENDER_CTX *)NULL);
		if(brtn) {
			crtn = buBsafeErrToCssm(brtn, "B_DecryptInit");
			goto abort;
		}
		brtn = B_DecryptUpdate(alg,
			outData->Data,
			&bytesMoved,
			outBufLen,
			inData->Data,
			inData->Length,
			NULL,			// randAlg
			NULL);			// surrender
		if(brtn) {
			crtn = buBsafeErrToCssm(brtn, "B_DecryptUpdate");
			goto abort;
		}
		outData->Length = bytesMoved;
		brtn = B_DecryptFinal(alg,
			outData->Data + bytesMoved,
			&bytesMoved,
			outBufLen - outData->Length,
			NULL,			// randAlg
			NULL);			// surrender
		if(brtn) {
			crtn = buBsafeErrToCssm(brtn, "B_DecryptFinal");
			goto abort;
		}
		outData->Length += bytesMoved;
		crtn = CSSM_OK;
	}
abort:
	B_DestroyAlgorithmObject(&alg);
#endif
	return 0;   //crtn;
}

#if 0
/* CSSM sig alg --> B_INFO_TYPE */
static CSSM_RETURN cssmSigAlgToInfoType(
	CSSM_ALGORITHMS cssmAlg,
	B_INFO_TYPE		*infoType)
{
	switch(cssmAlg) {
		case CSSM_ALGID_SHA1WithRSA:
			*infoType = AI_SHA1WithRSAEncryption;
			break;
		case CSSM_ALGID_MD5WithRSA:
			*infoType = AI_MD5WithRSAEncryption;
			break;
		case CSSM_ALGID_SHA1WithDSA:
			*infoType = AI_DSAWithSHA1;
			break;
		default:
			printf("cssmSigAlgToInfoType: unknown algorithm\n");
			return CSSMERR_CSSM_FUNCTION_NOT_IMPLEMENTED;
	}
	return CSSM_OK;
}
#endif

/*
 * Sign/verify
 */
CSSM_RETURN buSign(
	BU_KEY				key,
	CSSM_ALGORITHMS		sigAlg,
	const CSSM_DATA		*ptext,
	uint32				keySizeInBits,		// to set up sig
	CSSM_DATA_PTR		sig)				// mallocd and RETURNED
{
#if 0 // NO MORE BSAFE
	B_ALGORITHM_OBJ		alg = NULL;
	int 				brtn;
	B_INFO_TYPE			infoType;
	CSSM_RETURN			crtn;
	unsigned			sigBytes;
	
	brtn = B_CreateAlgorithmObject(&alg);
	if(brtn) {
		return buBsafeErrToCssm(brtn, "B_CreateAlgorithmObject");
	}
	crtn = cssmSigAlgToInfoType(sigAlg, &infoType);
	if(crtn) {
		return crtn;
	}
	brtn = B_SetAlgorithmInfo(alg, infoType, NULL);
	if(brtn) {
		crtn = buBsafeErrToCssm(brtn, "B_SetAlgorithmInfo");
		goto abort;
	}
	brtn = B_SignInit(alg, (B_KEY_OBJ)key, BSAFE_ALGORITHM_CHOOSER, NULL);
	if(brtn) {
		crtn = buBsafeErrToCssm(brtn, "B_SignInit");
		goto abort;
	}
	brtn = B_SignUpdate(alg, ptext->Data, ptext->Length, NULL);
	if(brtn) {
		crtn = buBsafeErrToCssm(brtn, "B_SignUpdate");
		goto abort;
	}
	
	/* prepare for sig, size of key */
	sigBytes = (keySizeInBits + 7) / 8;
	sig->Data = (uint8 *)CSSM_MALLOC(sigBytes);
	sig->Length = sigBytes;
	
	brtn = B_SignFinal(alg, sig->Data, &sigBytes, sigBytes, buGetRng(), NULL);
	if(brtn) {
		crtn = buBsafeErrToCssm(brtn, "B_SignFinal");
		goto abort;
	}
	sig->Length = sigBytes;
	crtn = CSSM_OK;
abort:
	B_DestroyAlgorithmObject(&alg);
#endif
	return 0;//;
}

CSSM_RETURN buVerify(
	BU_KEY				key,
	CSSM_ALGORITHMS		sigAlg,
	const CSSM_DATA		*ptext,
	const CSSM_DATA		*sig)				// mallocd and RETURNED
{
#if 0 // NO MORE BSAFE
	B_ALGORITHM_OBJ		alg = NULL;
	int 				brtn;
	B_INFO_TYPE			infoType;
	CSSM_RETURN			crtn;
	
	brtn = B_CreateAlgorithmObject(&alg);
	if(brtn) {
		return buBsafeErrToCssm(brtn, "B_CreateAlgorithmObject");
	}
	crtn = cssmSigAlgToInfoType(sigAlg, &infoType);
	if(crtn) {
		return crtn;
	}
	brtn = B_SetAlgorithmInfo(alg, infoType, NULL);
	if(brtn) {
		crtn = buBsafeErrToCssm(brtn, "B_SetAlgorithmInfo");
		goto abort;
	}
	brtn = B_VerifyInit(alg, (B_KEY_OBJ)key, BSAFE_ALGORITHM_CHOOSER, NULL);
	if(brtn) {
		crtn = buBsafeErrToCssm(brtn, "B_VerifyInit");
		goto abort;
	}
	brtn = B_VerifyUpdate(alg, ptext->Data, ptext->Length, NULL);
	if(brtn) {
		crtn = buBsafeErrToCssm(brtn, "B_VerifyUpdate");
		goto abort;
	}
	brtn = B_VerifyFinal(alg, sig->Data, sig->Length, buGetRng(), NULL);
	if(brtn) {
		crtn = buBsafeErrToCssm(brtn, "B_VerifyFinal");
		goto abort;
	}
	crtn = CSSM_OK;
abort:
	B_DestroyAlgorithmObject(&alg);
	return crtn;
#endif
    return 0;
}

/* 
 * generate MAC either one update (updateSizes == NULL) or 
 * specified set of update sizes.
 */
#define MAX_MAC_SIZE	20

CSSM_RETURN buGenMac(
	BU_KEY				key,				// any key, any size
	CSSM_ALGORITHMS		macAlg,				// only CSSM_ALGID_SHA1HMAC for now
	const CSSM_DATA		*ptext,
	unsigned			*updateSizes,		// NULL --> random updates
											// else null-terminated list of sizes
	CSSM_DATA_PTR		mac)				// mallocd and RETURNED 
{
#if 0 // NO MORE BSAFE
	B_ALGORITHM_OBJ		alg = NULL;
	int 				brtn;
	CSSM_RETURN			crtn;
	B_DIGEST_SPECIFIER	digestInfo;
	B_INFO_TYPE			infoType;
	unsigned			macBytes;
	
	brtn = B_CreateAlgorithmObject(&alg);
	if(brtn) {
		return buBsafeErrToCssm(brtn, "B_CreateAlgorithmObject");
	}
	switch(macAlg) {
		case CSSM_ALGID_SHA1HMAC:
		case CSSM_ALGID_SHA1HMAC_LEGACY:
			digestInfo.digestInfoType = AI_SHA1;
			infoType = AI_HMAC;
			break;
		default:
			printf("buGenMac: alg not supported\n");
			return CSSMERR_CSSM_FUNCTION_NOT_IMPLEMENTED;
	}
	digestInfo.digestInfoParams = NULL;
	brtn = B_SetAlgorithmInfo(alg, infoType, (POINTER)&digestInfo);
	if(brtn) {
		crtn = buBsafeErrToCssm(brtn, "B_SetAlgorithmInfo");
		goto abort;
	}
	brtn = B_DigestInit(alg, (B_KEY_OBJ)key, BSAFE_ALGORITHM_CHOOSER, NULL);
	if(brtn) {
		crtn = buBsafeErrToCssm(brtn, "B_DigestInit");
		goto abort;
	}
	if(updateSizes) {
		uint8 *currData = ptext->Data;
		while(*updateSizes) {
			brtn = B_DigestUpdate(alg, currData, *updateSizes, NULL);
			if(brtn) {
				crtn = buBsafeErrToCssm(brtn, "B_DigestUpdate");
				goto abort;
			}
			currData += *updateSizes;
			updateSizes++;
		}
	}
	else {
		/* one-shot */
		brtn = B_DigestUpdate(alg, ptext->Data, ptext->Length, NULL);
		if(brtn) {
			crtn = buBsafeErrToCssm(brtn, "B_DigestUpdate");
			goto abort;
		}
	}
	/* prepare for mac, magically gleaned max size */
	macBytes = MAX_MAC_SIZE;
	mac->Data = (uint8 *)CSSM_MALLOC(macBytes);
	mac->Length = macBytes;
	
	brtn = B_DigestFinal(alg, mac->Data, &macBytes, macBytes, NULL);
	if(brtn) {
		crtn = buBsafeErrToCssm(brtn, "B_DigestFinal");
		goto abort;
	}
	mac->Length = macBytes;
	crtn = CSSM_OK;
abort:
	B_DestroyAlgorithmObject(&alg);
	return crtn;
#endif
    return 0;
}

/* generate digest */
#define MAX_DIGEST_SIZE		20

CSSM_RETURN buGenDigest(
	CSSM_ALGORITHMS		macAlg,				// CSSM_ALGID_SHA1, etc. */
	const CSSM_DATA		*ptext,
	CSSM_DATA_PTR		digest)				// mallocd and RETURNED 
{
#if 0 // NO MORE BSAFE
	B_ALGORITHM_OBJ		alg = NULL;
	int 				brtn;
	CSSM_RETURN			crtn;
	B_INFO_TYPE			infoType;
	unsigned			hashBytes;
	
	brtn = B_CreateAlgorithmObject(&alg);
	if(brtn) {
		return buBsafeErrToCssm(brtn, "B_CreateAlgorithmObject");
	}
	switch(macAlg) {
		case CSSM_ALGID_SHA1:
			infoType = AI_SHA1;
			break;
		case CSSM_ALGID_MD5:
			infoType = AI_MD5;
			break;
		case CSSM_ALGID_MD2:
			infoType = AI_MD2;
			break;
		default:
			printf("buGenDigest: alg not supported\n");
			return CSSMERR_CSSM_FUNCTION_NOT_IMPLEMENTED;
	}
	brtn = B_SetAlgorithmInfo(alg, infoType, NULL);
	if(brtn) {
		crtn = buBsafeErrToCssm(brtn, "B_SetAlgorithmInfo");
		goto abort;
	}
	brtn = B_DigestInit(alg, NULL, BSAFE_ALGORITHM_CHOOSER, NULL);
	if(brtn) {
		crtn = buBsafeErrToCssm(brtn, "B_DigestInit");
		goto abort;
	}
	brtn = B_DigestUpdate(alg, ptext->Data, ptext->Length, NULL);
	if(brtn) {
		crtn = buBsafeErrToCssm(brtn, "B_DigestUpdate");
		goto abort;
	}
	
	/* prepare for digest, magically gleaned max size */
	hashBytes = MAX_DIGEST_SIZE;
	digest->Data = (uint8 *)CSSM_MALLOC(hashBytes);
	digest->Length = hashBytes;
	
	brtn = B_DigestFinal(alg, digest->Data, &hashBytes, hashBytes, NULL);
	if(brtn) {
		crtn = buBsafeErrToCssm(brtn, "B_DigestFinal");
		goto abort;
	}
	digest->Length = hashBytes;
	crtn = CSSM_OK;
abort:
	B_DestroyAlgorithmObject(&alg);
	return crtn;
#else
    return 0;
#endif
}

/*
 * Convert between BSAFE and CDSA private keys
 */
CSSM_RETURN buBsafePrivKeyToCdsa(
	CSSM_ALGORITHMS		keyAlg,
	uint32				keySizeInBits,
	BU_KEY				bsafePrivKey,
	CSSM_KEY_PTR		cdsaPrivKey)
{
#if 0 // NO MORE BSAFE
	B_INFO_TYPE			infoType;
	ITEM				*keyBlob;
	int					brtn;
	CSSM_KEYBLOB_FORMAT	format;
	CSSM_KEYHEADER_PTR	hdr = &cdsaPrivKey->KeyHeader;
	
	/* what kind of info? */
	switch(keyAlg) {
		case CSSM_ALGID_RSA:
			infoType = KI_PKCS_RSAPrivateBER;
			format = CSSM_KEYBLOB_RAW_FORMAT_PKCS8;
			break;
		case CSSM_ALGID_DSA:
			infoType = KI_DSAPrivateBER;
			format = CSSM_KEYBLOB_RAW_FORMAT_FIPS186;
			break;
		default:
			printf("***buBsafePrivKeyToCdsa: bogus keyAlg\n");
			return CSSMERR_CSSM_FUNCTION_NOT_IMPLEMENTED;
	}
	
	/* get the blob */
	brtn = B_GetKeyInfo((POINTER *)&keyBlob,
		(B_KEY_OBJ)bsafePrivKey,
		infoType);
	if(brtn) {
		return buBsafeErrToCssm(brtn, "B_GetKeyInfo");
	}
	
	/* copy blob to CDSA key */
	cdsaPrivKey->KeyData.Data = (uint8 *)CSSM_MALLOC(keyBlob->len);
	cdsaPrivKey->KeyData.Length = keyBlob->len;
	memmove(cdsaPrivKey->KeyData.Data, keyBlob->data, keyBlob->len);
	
	/* set up CSSM key header */
	memset(hdr, 0, sizeof(CSSM_KEYHEADER));
	hdr->HeaderVersion = CSSM_KEYHEADER_VERSION;
	hdr->BlobType = CSSM_KEYBLOB_RAW;
	hdr->Format = format;
	hdr->AlgorithmId = keyAlg;
	hdr->KeyClass = CSSM_KEYCLASS_PRIVATE_KEY;
	hdr->LogicalKeySizeInBits = keySizeInBits;
	hdr->KeyAttr = CSSM_KEYATTR_EXTRACTABLE;
	hdr->KeyUsage = CSSM_KEYUSE_ANY;
#endif
	return CSSM_OK;
}

CSSM_RETURN buCdsaPrivKeyToBsafe(
	CSSM_KEY_PTR		cdsaPrivKey,
	BU_KEY				*bsafePrivKey)
{
#if 0 // NO MORE BSAFE
	int 		brtn;
	B_KEY_OBJ	privKey = NULL;
	ITEM		keyBlob;
	B_INFO_TYPE	infoType;
	
	/* what kind of info? */
	switch(cdsaPrivKey->KeyHeader.AlgorithmId) {
		case CSSM_ALGID_RSA:
			infoType = KI_PKCS_RSAPrivateBER;
			break;
		case CSSM_ALGID_DSA:
			infoType = KI_DSAPrivateBER;
			break;
		default:
			printf("***buCdsaPrivKeyToCssm: bogus keyAlg\n");
			return CSSMERR_CSSM_FUNCTION_NOT_IMPLEMENTED;
	}
	
	/* create caller's key, assign blob to it */
	brtn = B_CreateKeyObject(&privKey);
	if(brtn) {
		return buBsafeErrToCssm(brtn, "B_CreateKeyObject");
	}
	buCssmDataToItem(&cdsaPrivKey->KeyData, &keyBlob);
	brtn = B_SetKeyInfo(privKey, infoType, (POINTER)&keyBlob);
	if(brtn) {
		return buBsafeErrToCssm(brtn, "B_SetKeyInfo");
	}
	*bsafePrivKey = privKey;
#endif
	return CSSM_OK;
}


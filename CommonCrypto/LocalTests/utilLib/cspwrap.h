/* Copyright 1997 Apple Computer, Inc.
 *
 * cspwrap.h - wrappers to simplify access to CDSA
 *
 * Revision History
 * ----------------
 *   3 May 2000 Doug Mitchell
 *		Ported to X/CDSA2.
 *  12 Aug 1997	Doug Mitchell at Apple
 *		Created.
 */
 
#ifndef	_CSPWRAP_H_
#define _CSPWRAP_H_
#include <Security/cssm.h>

#ifdef	__cplusplus
extern "C" {
#endif

/* 
 * Bug/feature workaround flags
 */
 
/* 
 * Doing a WrapKey requires Access Creds, which should be 
 * optional. Looks like this is not a bug.
 */
#define WRAP_KEY_REQUIRES_CREDS	1

/*
 * encrypt/decrypt - cook up a context handle
 */
CSSM_CC_HANDLE genCryptHandle(CSSM_CSP_HANDLE cspHand,
		uint32 algorithm,					// CSSM_ALGID_FEED, etc.
		uint32 mode,						// CSSM_ALGMODE_CBC, etc. - only for symmetric algs
		CSSM_PADDING padding,				// CSSM_PADDING_PKCS1, etc. 
		const CSSM_KEY *key0,
		const CSSM_KEY *key1,				// for CSSM_ALGID_FEED only - must be the 
											// public key
		const CSSM_DATA *iv,				// optional
		uint32 effectiveKeySizeInBits,		// 0 means skip this attribute
		uint32 rounds);						// ditto
/*
 * Key generation
 */
/*
 * Specifying a keySize of CSP_KEY_SIZE_DEFAULT results in using the default
 * key size for the specified algorithm.
 */
#define CSP_KEY_SIZE_DEFAULT		0

/* symmetric key sizes in bits */
#define CSP_ASC_KEY_SIZE_DEFAULT	(16 * 8)
#define CSP_DES_KEY_SIZE_DEFAULT	(8 * 8)
#define CSP_DES3_KEY_SIZE_DEFAULT	(24 * 8)
#define CSP_RC2_KEY_SIZE_DEFAULT	(10 * 8)
#define CSP_RC4_KEY_SIZE_DEFAULT	(10 * 8)
#define CSP_RC5_KEY_SIZE_DEFAULT	(10 * 8)
#define CSP_AES_KEY_SIZE_DEFAULT	128
#define CSP_BFISH_KEY_SIZE_DEFAULT	128
#define CSP_CAST_KEY_SIZE_DEFAULT	128
#define CSP_IDEA_KEY_SIZE_DEFAULT	128				/* fixed */
#define CSP_HMAC_SHA_KEY_SIZE_DEFAULT	(20 * 8)
#define CSP_HMAC_MD5_KEY_SIZE_DEFAULT	(16 * 8)
#define CSP_NULL_CRYPT_KEY_SIZE_DEF	(16 * 8)

/* asymmetric key sizes in bits */
/* note: we now use AI_RSAStrongKeyGen for RSA key pair 
 * generate; this requires at least 512 bits and also that
 * the key size be a multiple of 16. */
#define CSP_FEE_KEY_SIZE_DEFAULT	128

#define CSP_RSA_KEY_SIZE_DEFAULT	1024		/* min for SHA512/RSA */
#define CSP_DSA_KEY_SIZE_DEFAULT	512

/*
 * Generate key pair of arbitrary algorithm. 
 */
extern CSSM_RETURN cspGenKeyPair(CSSM_CSP_HANDLE cspHand,	
	uint32 algorithm,
	const char *keyLabel,
	unsigned keyLabelLen,
	uint32 keySizeInBits,
	CSSM_KEY_PTR pubKey,			// mallocd by caller
	CSSM_BOOL pubIsRef,				// true - reference key, false - data
	uint32 pubKeyUsage,				// CSSM_KEYUSE_ENCRYPT, etc.
	CSSM_KEYBLOB_FORMAT pubFormat,	// Optional. Specify 0 or CSSM_KEYBLOB_RAW_FORMAT_NONE
									//   to get the default format. 
	CSSM_KEY_PTR privKey,			// mallocd by caller - always returned as ref
	CSSM_BOOL privIsRef,			// true - reference key, false - data
	uint32 privKeyUsage,			// CSSM_KEYUSE_DECRYPT, etc.
	CSSM_KEYBLOB_FORMAT privFormat,	// optional 0 ==> default
	CSSM_BOOL genSeed);				// FEE only. True: we generate seed and CSP
									//   will hash it. False: CSP generates random 
									//   seed. 

/*
 * Generate FEE key pair with optional primeType, curveType, and seed (password) data.
 */
extern CSSM_RETURN cspGenFEEKeyPair(CSSM_CSP_HANDLE cspHand,
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
	const CSSM_DATA *seedData);		// Present: CSP will hash this for private data.
									// NULL: CSP generates random seed. 

/*
 * Generate DSA key pair with optional generateAlgParams.
 */
extern CSSM_RETURN cspGenDSAKeyPair(CSSM_CSP_HANDLE cspHand,
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
	CSSM_DATA_PTR paramData);		// optional

/*
 * Create a symmetric key.
 */
extern CSSM_KEY_PTR cspGenSymKey(CSSM_CSP_HANDLE cspHand,
		uint32 				alg,
		const char 			*keyLabel,
		unsigned 			keyLabelLen,
		uint32 				keyUsage,		// CSSM_KEYUSE_ENCRYPT, etc.
		uint32 				keySizeInBits,
		CSSM_BOOL			refKey); // true - reference key, false - data

/*
 * Derive symmetric key using PBE.
 */
CSSM_KEY_PTR cspDeriveKey(CSSM_CSP_HANDLE cspHand,
		uint32 				deriveAlg,		// CSSM_ALGID_MD5_PBE, etc.
		uint32				keyAlg,			// CSSM_ALGID_RC5, etc.
		const char 			*keyLabel,
		unsigned 			keyLabelLen,
		uint32 				keyUsage,		// CSSM_KEYUSE_ENCRYPT, etc.
		uint32 				keySizeInBits,
		CSSM_BOOL			isRefKey,
		CSSM_DATA_PTR		password,		// in PKCS-5 lingo
		CSSM_DATA_PTR		salt,			// ditto
		uint32				iterationCnt,	// ditto
		CSSM_DATA_PTR		initVector);	// mallocd & RETURNED

/*
 * Encrypt/Decrypt - these work for both symmetric and asymmetric algorithms.
 */
CSSM_RETURN cspEncrypt(CSSM_CSP_HANDLE cspHand,
		uint32 algorithm,					// CSSM_ALGID_FEED, etc.
		uint32 mode,						// CSSM_ALGMODE_CBC, etc. - only for 
											//    symmetric algs
		CSSM_PADDING padding,				// CSSM_PADDING_PKCS1, etc. 
		const CSSM_KEY *key,				// public or session key
		const CSSM_KEY *pubKey,				// for CSSM_ALGID_{FEED,FEECFILE} only
		uint32 effectiveKeySizeInBits,		// 0 means skip this attribute
		uint32 rounds,						// ditto
		const CSSM_DATA *iv,				// init vector, optional
		const CSSM_DATA *ptext,
		CSSM_DATA_PTR ctext,				// RETURNED
		CSSM_BOOL mallocCtext);
		
CSSM_RETURN cspStagedEncrypt(CSSM_CSP_HANDLE cspHand,
		uint32 algorithm,					// CSSM_ALGID_FEED, etc.
		uint32 mode,						// CSSM_ALGMODE_CBC, etc. - only for 
											//    symmetric algs
		CSSM_PADDING padding,				// CSSM_PADDING_PKCS1, etc. 
		const CSSM_KEY *key,				// public or session key
		const CSSM_KEY *pubKey,				// for CSSM_ALGID_{FEED,FEECFILE} only
		uint32 effectiveKeySizeInBits,		// 0 means skip this attribute
		uint32 cipherBlockSize,				// ditto, block size in bytes
		uint32 rounds,						// ditto
		const CSSM_DATA *iv,				// init vector, optional
		const CSSM_DATA *ptext,
		CSSM_DATA_PTR ctext,				// RETURNED, we malloc
		CSSM_BOOL multiUpdates);			// false:single update, true:multi updates
		
CSSM_RETURN cspDecrypt(CSSM_CSP_HANDLE cspHand,
		uint32 algorithm,					// CSSM_ALGID_FEED, etc.
		uint32 mode,						// CSSM_ALGMODE_CBC, etc. - only for 
											//    symmetric algs
		CSSM_PADDING padding,				// CSSM_PADDING_PKCS1, etc. 
		const CSSM_KEY *key,				// private or session key
		const CSSM_KEY *pubKey,				// for CSSM_ALGID_{FEED,FEECFILE} only
		uint32 effectiveKeySizeInBits,		// 0 means skip this attribute
		uint32 rounds,						// ditto
		const CSSM_DATA *iv,				// init vector, optional
		const CSSM_DATA *ctext,
		CSSM_DATA_PTR ptext,				// RETURNED
		CSSM_BOOL mallocPtext);
		
CSSM_RETURN cspStagedDecrypt(CSSM_CSP_HANDLE cspHand,
		uint32 algorithm,					// CSSM_ALGID_FEED, etc.
		uint32 mode,						// CSSM_ALGMODE_CBC, etc. - only for 
											//    symmetric algs
		CSSM_PADDING padding,				// CSSM_PADDING_PKCS1, etc. 
		const CSSM_KEY *key,				// private or session key
		const CSSM_KEY *pubKey,				// for CSSM_ALGID_{FEED,FEECFILE} only
		uint32 effectiveKeySizeInBits,		// 0 means skip this attribute
		uint32 cipherBlockSize,				// ditto, block size in bytes
		uint32 rounds,						// ditto
		const CSSM_DATA *iv,				// init vector, optional
		const CSSM_DATA *ctext,
		CSSM_DATA_PTR ptext,				// RETURNED, we malloc
		CSSM_BOOL multiUpdates);			// false:single update, true:multi updates

/*
 * Signature routines
 */
CSSM_RETURN cspSign(CSSM_CSP_HANDLE cspHand,
		uint32 algorithm,					// CSSM_ALGID_FEE_MD5, etc.
		CSSM_KEY_PTR key,					// private key
		const CSSM_DATA *text,
		CSSM_DATA_PTR sig);					// RETURNED
CSSM_RETURN cspStagedSign(CSSM_CSP_HANDLE cspHand,
		uint32 algorithm,					// CSSM_ALGID_FEE_MD5, etc.
		CSSM_KEY_PTR key,					// private key
		const CSSM_DATA *text,
		CSSM_BOOL multiUpdates,				// false:single update, true:multi updates
		CSSM_DATA_PTR sig);					// RETURNED
CSSM_RETURN cspSigVerify(CSSM_CSP_HANDLE cspHand,
		uint32 algorithm,					// CSSM_ALGID_FEE_MD5, etc.
		CSSM_KEY_PTR key,					// public key
		const CSSM_DATA *text,
		const CSSM_DATA *sig,
		CSSM_RETURN expectResult);			// expected result is verify failure
											// CSSM_OK - expect success
CSSM_RETURN cspStagedSigVerify(CSSM_CSP_HANDLE cspHand,
		uint32 algorithm,					// CSSM_ALGID_FEE_MD5, etc.
		CSSM_KEY_PTR key,					// private key
		const CSSM_DATA *text,
		const CSSM_DATA *sig,
		CSSM_BOOL multiUpdates,				// false:single update, true:multi updates
		CSSM_RETURN expectResult);			// expected result is verify failure
											// CSSM_OK - expect success

/*
 * MAC routines
 */
CSSM_RETURN cspGenMac(CSSM_CSP_HANDLE cspHand,
		uint32 algorithm,					// CSSM_ALGID_DES, etc.
		CSSM_KEY_PTR key,					// session key
		const CSSM_DATA *text,
		CSSM_DATA_PTR mac);					// RETURNED
CSSM_RETURN cspStagedGenMac(CSSM_CSP_HANDLE cspHand,
		uint32 algorithm,					// CSSM_ALGID_FEE_MD5, etc.
		CSSM_KEY_PTR key,					// private key
		const CSSM_DATA *text,
		CSSM_BOOL mallocMac,				// if true and digest->Length = 0, we'll 
											//		malloc
		CSSM_BOOL multiUpdates,				// false:single update, true:multi updates
		CSSM_DATA_PTR mac);					// RETURNED
CSSM_RETURN cspMacVerify(CSSM_CSP_HANDLE cspHand,
		uint32 algorithm,					
		CSSM_KEY_PTR key,					// public key
		const CSSM_DATA *text,
		const CSSM_DATA_PTR mac,
		CSSM_RETURN expectResult);		
CSSM_RETURN cspStagedMacVerify(CSSM_CSP_HANDLE cspHand,
		uint32 algorithm,					
		CSSM_KEY_PTR key,					// private key
		const CSSM_DATA *text,
		const CSSM_DATA_PTR mac,
		CSSM_BOOL multiUpdates,				// false:single update, true:multi updates
		CSSM_RETURN expectResult);			

/*
 * Digest functions
 */
CSSM_RETURN cspDigest(CSSM_CSP_HANDLE cspHand,
		uint32 algorithm,					// CSSM_ALGID_MD5, etc.
		CSSM_BOOL mallocDigest,				// if true and digest->Length = 0, we'll malloc
		const CSSM_DATA *text,
		CSSM_DATA_PTR digest);
CSSM_RETURN cspStagedDigest(CSSM_CSP_HANDLE cspHand,
		uint32 algorithm,					// CSSM_ALGID_MD5, etc.
		CSSM_BOOL mallocDigest,				// if true and digest->Length = 0, we'll malloc
		CSSM_BOOL multiUpdates,				// false:single update, true:multi updates
		const CSSM_DATA *text,
		CSSM_DATA_PTR digest);
CSSM_RETURN	cspFreeKey(CSSM_CSP_HANDLE cspHand,
	CSSM_KEY_PTR key);

/*
 * Perform FEE Key exchange via CSSM_DeriveKey. 
 */
CSSM_RETURN cspFeeKeyExchange(CSSM_CSP_HANDLE cspHand,
	CSSM_KEY_PTR 	privKey,
	CSSM_KEY_PTR 	pubKey,
	CSSM_KEY_PTR 	derivedKey,		// mallocd by caller
	
	/* remaining fields apply to derivedKey */
	uint32 			keyAlg,
	const char 		*keyLabel,
	unsigned 		keyLabelLen,
	uint32 			keyUsage,		// CSSM_KEYUSE_ENCRYPT, etc.
	uint32 			keySizeInBits);

/* 
 * wrap/unwrap key functions. 
 */
CSSM_RETURN cspWrapKey(CSSM_CSP_HANDLE cspHand,
	const CSSM_KEY			*unwrappedKey,	
	const CSSM_KEY			*wrappingKey,
	CSSM_ALGORITHMS			wrapAlg,
	CSSM_ENCRYPT_MODE		wrapMode,
	CSSM_KEYBLOB_FORMAT		wrapFormat,			// NONE, PKCS7, PKCS8
	CSSM_PADDING			wrapPad,
	CSSM_DATA_PTR			initVector,			// for some wrapping algs
	CSSM_DATA_PTR			descrData,			// optional 
	CSSM_KEY_PTR			wrappedKey);		// RETURNED
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
	unsigned 				keyLabelLen);

/* generate a random and reasonable key size in bits for specified CSSM algorithm */
typedef enum {
	OT_Sign,
	OT_Encrypt,
	OT_KeyExch
} opType;

#define MAX_KEY_SIZE_RC245_BYTES		64	/* max bytes, RC2, RC4, RC5 */

uint32 randKeySizeBits(uint32 alg, opType op);
uint32 cspDefaultKeySize(uint32 alg);

/*
 * Generate random key size, primeType, curveType for FEE key for specified op.
 */
void randFeeKeyParams(
	CSSM_ALGORITHMS	alg,			// ALGID_FEED, CSSM_ALGID_FEE_MD5, etc.
	uint32			*keySizeInBits,	// RETURNED
	uint32 			*primeType,		// CSSM_FEE_PRIME_TYPE_xxx, RETURNED
	uint32 			*curveType);	// CSSM_FEE_CURVE_TYPE_xxx, RETURNED

/*
 * Obtain strings for primeType and curveType.
 */
const char *primeTypeStr(uint32 primeType);
const char *curveTypeStr(uint32 curveType);

/*
 * Given any key in either blob or reference format,
 * obtain the associated SHA-1 hash. 
 */
CSSM_RETURN cspKeyHash(
	CSSM_CSP_HANDLE		cspHand,	
	const CSSM_KEY_PTR	key,			/* public key */
	CSSM_DATA_PTR		*hashData);		/* hash mallocd and RETURNED here */

/* wrap ref key --> raw key */
CSSM_RETURN cspRefKeyToRaw(
	CSSM_CSP_HANDLE cspHand,
	const CSSM_KEY *refKey,
	CSSM_KEY_PTR rawKey);				// init'd and RETURNED

/* unwrap raw key --> ref */
CSSM_RETURN cspRawKeyToRef(
	CSSM_CSP_HANDLE cspHand,
	const CSSM_KEY *rawKey,
	CSSM_KEY_PTR refKey);				// init'd and RETURNED

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
	CSSM_KEY_PTR		refKey);			// init'd and RETURNED

/*
 * Add a DL/DB handle to a crypto context.
 */
CSSM_RETURN cspAddDlDbToContext(
	CSSM_CC_HANDLE ccHand,
	CSSM_DL_HANDLE dlHand,
	CSSM_DB_HANDLE dbHand);

/*
 * Look up a key by label and type.
 */
typedef enum {
	CKT_Public = 1,
	CKT_Private = 2,
	CKT_Session = 3
	/* any others? */
} CT_KeyType;

CSSM_KEY_PTR cspLookUpKeyByLabel(
	CSSM_DL_HANDLE dlHand, 
	CSSM_DB_HANDLE dbHand, 
	const CSSM_DATA *labelData, 
	CT_KeyType keyType);

/*
 * Delete and free a key 
 */
CSSM_RETURN cspDeleteKey(
	CSSM_CSP_HANDLE		cspHand,		// for free
	CSSM_DL_HANDLE		dlHand,			// for delete
	CSSM_DB_HANDLE		dbHand,			// ditto
	const CSSM_DATA 	*labelData, 
	CSSM_KEY_PTR		key);

// temp hack
#define	CSSM_ALGID_FEECFILE		(CSSM_ALGID_VENDOR_DEFINED + 102)

#ifdef	__cplusplus
}
#endif
#endif	/* _CSPWRAP_H_ */

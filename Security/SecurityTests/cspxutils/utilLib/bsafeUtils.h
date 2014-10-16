/*
 * bsafeUtils.h - common routines for CDSA/BSAFE compatibility testing
 */

/*
 * Clients of this module do not need to know about or see anything from the 
 * BSAFE headers. 
 */
#ifndef	_BSAFE_UTILS_H_
#define _BSAFE_UTILS_H_
#include <Security/cssmtype.h>

#ifdef	__cplusplus
extern "C" {
#endif

/* Actually the same as a B_KEY_OBJ, but our callers don't need to know that */
typedef void *BU_KEY;

/*
 * Create a symmetric key.
 */
CSSM_RETURN  buGenSymKey(
	uint32			keySizeInBits,
	const CSSM_DATA	*keyData,
	BU_KEY			*key);			// RETURNED

/*
 * Create asymmetric key pair.
 * FIXME - additional params (e.g. DSA params, RSA exponent)?
 */
CSSM_RETURN buGenKeyPair(
	uint32			keySizeInBits,
	CSSM_ALGORITHMS	keyAlg,			// CSSM_ALGID_{RSA,DSA}
	BU_KEY			*pubKey,		// RETURNED
	BU_KEY			*privKey);		// RETURNED
	
/*
 * Free a key created in buGenSymKey or buGenKeyPair
 */
CSSM_RETURN buFreeKey(
	BU_KEY			key);

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
	CSSM_DATA_PTR		outData);			// mallocd and RETURNED

/*
 * Sign/verify
 */
CSSM_RETURN buSign(
	BU_KEY				key,
	CSSM_ALGORITHMS		sigAlg,
	const CSSM_DATA		*ptext,
	uint32				keySizeInBits,		// to set up sig
	CSSM_DATA_PTR		sig);				// mallocd and RETURNED

CSSM_RETURN buVerify(
	BU_KEY				key,
	CSSM_ALGORITHMS		sigAlg,
	const CSSM_DATA		*ptext,
	const CSSM_DATA		*sig);				// mallocd and RETURNED

/* 
 * generate MAC either one update (updateSizes == NULL) or 
 * specified set of update sizes.
 */
CSSM_RETURN buGenMac(
	BU_KEY				key,				// any key, any size
	CSSM_ALGORITHMS		macAlg,				// only CSSM_ALGID_SHA1HMAC for now
	const CSSM_DATA		*ptext,
	unsigned			*updateSizes,		// NULL --> random updates
											// else null-terminated list of sizes
	CSSM_DATA_PTR		mac);				// mallocd and RETURNED 
	
/* generate digest */
CSSM_RETURN buGenDigest(
	CSSM_ALGORITHMS		macAlg,				// CSSM_ALGID_SHA1, etc. */
	const CSSM_DATA		*ptext,
	CSSM_DATA_PTR		digest);			// mallocd and RETURNED 
	
/*
 * Convert between BSAFE and CDSA private keys
 */
CSSM_RETURN buBsafePrivKeyToCdsa(
	CSSM_ALGORITHMS		keyAlg,
	uint32				keySizeInBits,
	BU_KEY				bsafePrivKey,
	CSSM_KEY_PTR		cdsaPrivKey);
CSSM_RETURN buCdsaPrivKeyToBsafe(
	CSSM_KEY_PTR		cdsaPrivKey,
	BU_KEY				*bsafePrivKey);

#ifdef	__cplusplus
}
#endif

#endif	/* _BSAFE_UTILS_H_ */

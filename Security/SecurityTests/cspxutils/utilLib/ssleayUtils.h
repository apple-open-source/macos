/*
 * ssleayUtils.h - common routines for CDSA/openssl compatibility testing
 */

/*
 * Clients of this module do not need to know about or see anything from the 
 * libcrypt headers. 
 */
#ifndef	_SSLEAY_UTILS_H_
#define _SSLEAY_UTILS_H_
#include <Security/cssmtype.h>

typedef void *EAY_KEY;

/*
 * Create a symmetric key.
 */
CSSM_RETURN  eayGenSymKey(
	CSSM_ALGORITHMS alg,
	CSSM_BOOL		forEncr,
	const CSSM_DATA	*keyData,
	EAY_KEY			*key);			// RETURNED

/*
 * Free a key created in eayGenSymKey
 */
CSSM_RETURN eayFreeKey(
	EAY_KEY			key);

/*
 * encrypt/decrypt
 */
CSSM_RETURN eayEncryptDecrypt(
	EAY_KEY				key,
	CSSM_BOOL			forEncrypt,
	CSSM_ALGORITHMS		encrAlg,
	CSSM_ENCRYPT_MODE	mode,				// CSSM_ALGMODE_CBC ONLY!
	const CSSM_DATA		*iv,				//Êoptional per mode
	const CSSM_DATA		*inData,
	CSSM_DATA_PTR		outData);			// mallocd and RETURNED

/*** EVP-based encrypt/decrypt ***/

int evpEncryptDecrypt(
	CSSM_ALGORITHMS		alg,				// AES 128 only for now 
	CSSM_BOOL			forEncr,
	const CSSM_DATA		*keyData,			// may be larger than the key size we use
	unsigned			keyLengthInBits,
	CSSM_ENCRYPT_MODE	mode,				// CSSM_ALGMODE_CBC_IV8, ECB, always padding
	const CSSM_DATA		*iv,				// optional per mode
	const CSSM_DATA		*inData,
	CSSM_DATA_PTR		outData);			// CSSM_MALLOCd and RETURNED

#endif	/* _EAY_UTILS_H_ */

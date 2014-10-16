#include <Security/cssmtype.h>
#include <Security/cssmerr.h>

/* 
 * encrypt/decrypt using test implementation.
 */
CSSM_RETURN encryptDecryptTest(
	CSSM_BOOL			forEncrypt,
	uint32				keyBits,
	uint32				blockSizeInBits,	
	const uint8			*key,				// raw key bytes
	const uint8			*inText,
	uint32				inTextLen,			// in bytes
	uint8 				*outText);

/* 
 * encrypt/decrypt using reference AES.
 */
CSSM_RETURN encryptDecryptRef(
	CSSM_BOOL			forEncrypt,
	uint32				keyBits,
	uint32				blockSizeInBits,	
	const uint8			*key,				// raw key bytes
	const uint8			*inText,
	uint32				inTextLen,			// in bytes
	uint8 				*outText);

/* 
 * encrypt/decrypt using CSP.
 */
CSSM_RETURN encryptDecryptCsp(
	CSSM_BOOL			forEncrypt,
	uint32				keyBits,
	uint32				blockSizeInBits,	
	const uint8			*key,				// raw key bytes
	const uint8			*inText,
	uint32				inTextLen,			// in bytes
	uint8 				*outText);

typedef CSSM_RETURN (*encrDecrFcn) (
	CSSM_BOOL			forEncrypt,
	uint32				keyBits,
	uint32				blockSizeInBits,	
	const uint8			*key,				// raw key bytes
	const uint8			*inText,
	uint32				inTextLen,			// in bytes
	uint8 				*outText);

#include "enDecrypt.h"
#include "rijndaelApi.h"		/* reference */

/* 
 * encrypt/decrypt using reference AES.
 */
CSSM_RETURN encryptDecryptRef(
	CSSM_BOOL			forEncrypt,
	uint32				keySizeInBits,
	uint32				blockSizeInBits,	
	const uint8			*key,				// raw key bytes
	const uint8			*inText,
	uint32				inTextLen,
	uint8 				*outText)
{
	keyInstance 	aesKey;
	cipherInstance 	aesCipher;
	int 			artn;
	
	artn = _makeKey(&aesKey, 
		forEncrypt ? DIR_ENCRYPT : DIR_DECRYPT,
		keySizeInBits,
		blockSizeInBits,
		(BYTE *)key);
	if(artn <= 0) {
		printf("***AES makeKey returned %d\n", artn);
		return CSSM_ERRCODE_INTERNAL_ERROR;
	}
	artn = _cipherInit(&aesCipher,
		MODE_ECB,
		blockSizeInBits,
		NULL);
	if(artn <= 0) {
		printf("***AES cipherInit returned %d\n", artn);
		return CSSM_ERRCODE_INTERNAL_ERROR;
	}
	if(forEncrypt) {
		artn = _blockEncrypt(&aesCipher,
			&aesKey,
			(BYTE *)inText,
			inTextLen * 8,
			(BYTE *)outText);
	}
	else {
		artn = _blockDecrypt(&aesCipher,
			&aesKey,
			(BYTE *)inText,
			inTextLen * 8,
			(BYTE *)outText);
	}
	if(artn <= 0) {
		printf("***AES Reference encrypt/decrypt returned %d\n", artn);
		return CSSM_ERRCODE_INTERNAL_ERROR;
	}
	return CSSM_OK;
}

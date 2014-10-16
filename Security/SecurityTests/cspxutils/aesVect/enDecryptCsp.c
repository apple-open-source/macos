/* 
 * encrypt/decrypt using CSP implementation of AES.
 */
#include "enDecrypt.h"
#include <Security/cssm.h>
#include "cspwrap.h"
#include "common.h"
#include <strings.h>

static 	CSSM_CSP_HANDLE cspHand = 0;

CSSM_RETURN encryptDecryptCsp(
	CSSM_BOOL			forEncrypt,
	uint32				keySizeInBits,
	uint32				blockSizeInBits,	
	const uint8			*key,				// raw key bytes
	const uint8			*inText,
	uint32				inTextLen,
	uint8 				*outText)
{
	CSSM_KEY_PTR		symKey;				// mallocd by cspGenSymKey or a ptr
											// to refKey
	CSSM_RETURN 		crtn;
	CSSM_DATA			inData;
	CSSM_DATA			outData;
	
	if(cspHand == 0) {
		/* attach first time thru */
		cspHand = cspDlDbStartup(CSSM_TRUE, NULL);
		if(cspHand == 0) {
			return CSSMERR_CSSM_MODULE_NOT_LOADED;
		}
	}
	
	/* cook up a raw symmetric key */
	symKey = cspGenSymKey(cspHand,
		CSSM_ALGID_AES,
		"noLabel",
		8,
		CSSM_KEYUSE_ENCRYPT | CSSM_KEYUSE_DECRYPT,
		keySizeInBits,
		CSSM_FALSE);			// ref key
	if(symKey == NULL) {
		return CSSM_ERRCODE_INTERNAL_ERROR;
	}
	memmove(symKey->KeyData.Data, key, keySizeInBits / 8);

	inData.Data = (uint8 *)inText;
	inData.Length = inTextLen;
	outData.Data = outText;
	outData.Length = inTextLen;

	if(forEncrypt) {
		crtn = cspEncrypt(cspHand,
			CSSM_ALGID_AES,
			CSSM_ALGMODE_ECB,
			CSSM_PADDING_NONE,
			symKey,
			NULL,			// no second key
			0,				// effectiveKeyBits
			0,				// rounds
			NULL,			// iv
			&inData,
			&outData,
			CSSM_FALSE);	// mallocCtext
	}
	else {
		crtn = cspDecrypt(cspHand,
			CSSM_ALGID_AES,
			CSSM_ALGMODE_ECB,
			CSSM_PADDING_NONE,
			symKey,
			NULL,			// no second key
			0,				// effectiveKeyBits
			0,				// rounds
			NULL,			// iv
			&inData,
			&outData,
			CSSM_FALSE);	// mallocPtext
	}
	cspFreeKey(cspHand, symKey);
	return crtn;

}

#include "enDecrypt.h"
#include "std_defs.h"	
#include <strings.h>
#include <stdio.h>

/* 
 * encrypt/decrypt using Gladman version of AES.
 */

CSSM_RETURN encryptDecryptTest(
	CSSM_BOOL			forEncrypt,
	uint32				keySizeInBits,
	uint32				blockSizeInBits,	
	const uint8			*key,				// raw key bytes
	const uint8			*inText,
	uint32				inTextLen,
	uint8 				*outText)
{
	u4byte			aesKey[8];
	uint8			*inPtr = (uint8 *)inText;
	uint8			*outPtr = outText;
	uint32			blockSizeInBytes = blockSizeInBits / 8;
	uint32			blocks = inTextLen / blockSizeInBytes;
	
	if(blockSizeInBits != 128) {
		printf("***This AES implementation supports only 128 bit blocks.\n");
		return CSSM_ERRCODE_INTERNAL_ERROR;
	}
	memmove(aesKey, key, keySizeInBits / 8);
	set_key(aesKey, keySizeInBits);
	for( ; blocks > 0; blocks--) {
		if(forEncrypt) {
			rEncrypt((u4byte *)inPtr, (u4byte *)outPtr);
		}
		else {
			rDecrypt((u4byte *)inPtr, (u4byte *)outPtr);
		}
		inPtr += blockSizeInBytes;
		outPtr += blockSizeInBytes;
	}
	return CSSM_OK;
}

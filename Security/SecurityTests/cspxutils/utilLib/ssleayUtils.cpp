/*
 * ssleayUtils.c - common routines for CDSA/openssl compatibility testing
 */
 
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <openssl/blowfish.h>
#include <openssl/cast.h>
#include <openssl/evp.h>
#include "ssleayUtils.h"
#include <Security/cssmerr.h>
#include "common.h"

/*
 * Caller sees EAY_KEY, we see a pointer to this.
 */
typedef struct {
	CSSM_ALGORITHMS alg;
	union {
		BF_KEY				bf;		// blowfish
		CAST_KEY			cast;
	} key;
} EayKeyPriv;

/*
 * Create a symmetric key.
 */
CSSM_RETURN  eayGenSymKey(
	CSSM_ALGORITHMS alg,
	CSSM_BOOL		forEncr,
	const CSSM_DATA	*keyData,
	EAY_KEY			*key)			// RETURNED
{
	EayKeyPriv *ekp = (EayKeyPriv *)malloc(sizeof(EayKeyPriv));
	memset(ekp, 0, sizeof(*ekp));
	switch(alg) {
		case CSSM_ALGID_BLOWFISH:
			BF_set_key(&ekp->key.bf, keyData->Length, keyData->Data);
			break;
		case CSSM_ALGID_CAST:		// cast128 only
			CAST_set_key(&ekp->key.cast, keyData->Length, keyData->Data);
			break;
		default:
			printf("***eayGenSymKey: bad alg\n");
			return -1;
	}
	ekp->alg = alg;
	*key = (EAY_KEY)ekp;
	return CSSM_OK;
}

/*
 * Free a key created in eayGenSymKey
 */
CSSM_RETURN eayFreeKey(
	EAY_KEY			key)
{
	memset(key, 0, sizeof(EayKeyPriv));
	free(key);
	return CSSM_OK;
}

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
	CSSM_DATA_PTR		outData)			// CSSM_MALLOCd and RETURNED
{
	EayKeyPriv *ekp = (EayKeyPriv *)key;
	if((mode != CSSM_ALGMODE_CBC_IV8) && (mode != CSSM_ALGMODE_ECB)) {
		printf("***eayEncryptDecrypt only does CBC_IV8, ECB\n");
		return -1;
	}
	
	bool cbc = (mode == CSSM_ALGMODE_ECB) ? false : true;
	
	outData->Data = (uint8 *)CSSM_MALLOC(inData->Length);
	outData->Length = inData->Length;
	
	/* BF_cbc_encrypt actually writes to IV */
	CSSM_DATA ivc = {0, NULL};
	if(cbc) {
		ivc.Data = (uint8 *)malloc(iv->Length);
		ivc.Length = iv->Length;
		memmove(ivc.Data, iv->Data, ivc.Length);
	}
	switch(encrAlg) {
		case CSSM_ALGID_BLOWFISH:
			if(cbc) {
				BF_cbc_encrypt(inData->Data,
					outData->Data,
					inData->Length,
					&ekp->key.bf,
					ivc.Data,
					forEncrypt ? BF_ENCRYPT : BF_DECRYPT);
			}
			else {
				CSSM_DATA intext = *inData;
				CSSM_DATA outtext = *outData;
				while(intext.Length) {
					BF_ecb_encrypt(intext.Data,
						outtext.Data,
						&ekp->key.bf,
						forEncrypt ? BF_ENCRYPT : BF_DECRYPT);
					intext.Data   += 8;
					outtext.Data  += 8;
					intext.Length -= 8;
				}
			}
			break;
		case CSSM_ALGID_CAST:		// cast128 only
			CAST_cbc_encrypt(inData->Data,
				outData->Data,
				inData->Length,
				&ekp->key.cast,
				ivc.Data,
				forEncrypt ? CAST_ENCRYPT : CAST_DECRYPT);
			break;
		default:
			printf("***eayEncryptDecrypt: bad alg\n");
			return -1;
	}
	if(ivc.Data) {
		free(ivc.Data);
	}
	return CSSM_OK;
}

/*** EVP-based encrypt/decrypt ***/

int evpEncryptDecrypt(
	CSSM_ALGORITHMS		alg,				// AES 128 only for now 
	CSSM_BOOL			forEncr,
	const CSSM_DATA		*keyData,			// may be larger than the key size we use
	unsigned			keyLengthInBits,
	CSSM_ENCRYPT_MODE	mode,				// CSSM_ALGMODE_CBC_IV8, ECB, always padding
	const CSSM_DATA		*iv,				// optional per mode
	const CSSM_DATA		*inData,
	CSSM_DATA_PTR		outData)			// CSSM_MALLOCd and RETURNED
{
	EVP_CIPHER_CTX ctx;
	const EVP_CIPHER *cipher;
	unsigned blockSize;
	unsigned outLen = inData->Length;
	bool noPad = false;
	
	switch(alg) {
		case CSSM_ALGID_AES:
			switch(mode) {
				case CSSM_ALGMODE_CBCPadIV8:
					switch(keyLengthInBits) {
						case 128:
							cipher = EVP_aes_128_cbc();
							break;
						case 192:
							cipher = EVP_aes_192_cbc();
							break;
						case 256:
							cipher = EVP_aes_256_cbc();
							break;
						default:
							printf("***Bad AES key length (%u)\n", keyLengthInBits);
							return -1;
					}
					break;
				case CSSM_ALGMODE_ECB:
					switch(keyLengthInBits) {
						case 128:
							cipher = EVP_aes_128_ecb();
							break;
						case 192:
							cipher = EVP_aes_192_ecb();
							break;
						case 256:
							cipher = EVP_aes_256_ecb();
							break;
						default:
							printf("***Bad AES key length (%u)\n", keyLengthInBits);
							return -1;
					}
					noPad = true;
					break;
				default:
					printf("***evpEncryptDecrypt only does CBC and ECB for now\n");
					return -1;
			}
			blockSize = 16;
			break;
		case CSSM_ALGID_DES:
			switch(mode) {
				case CSSM_ALGMODE_CBCPadIV8:
					cipher = EVP_des_cbc();
					break;
				case CSSM_ALGMODE_ECB:
					cipher = EVP_des_ecb();
					noPad = true;
					break;
				default:
					printf("***evpEncryptDecrypt only does CBC and ECB for now\n");
					return -1;
			}
			blockSize = 8;
			break;
		default:
			printf("***evpEncryptDecrypt only does DES and AES 128 for now\n");
			return -1;
	}
	outLen += blockSize;
	unsigned char *outp = (uint8 *)CSSM_MALLOC(outLen);
	int outl = outLen;
	outData->Data = outp;
	
	if(forEncr) {
		int rtn = EVP_EncryptInit(&ctx, cipher, keyData->Data, iv ? iv->Data : NULL);
		if(!rtn) {
			printf("EVP_EncryptInit error\n");
			return -1;
		}
		if(noPad) {
			EVP_CIPHER_CTX_set_padding(&ctx, 0);
		}
		if(!EVP_EncryptUpdate(&ctx, outp, &outl, inData->Data, inData->Length)) {
			printf("EVP_EncryptUpdate error\n");
			return -1;
		}
	}
	else {
		int rtn = EVP_DecryptInit(&ctx, cipher, keyData->Data, iv ? iv->Data : NULL);
		if(!rtn) {
			printf("EVP_DecryptInit error\n");
			return -1;
		}
		if(noPad) {
			EVP_CIPHER_CTX_set_padding(&ctx, 0);
		}
		
		if(!EVP_DecryptUpdate(&ctx, outp, &outl, inData->Data, inData->Length)) {
			printf("EVP_DecryptUpdate error\n");
			return -1;
		}
	}
	outData->Length = outl;
	outp += outl;
	outl = outLen - outl;
	if(forEncr) {
		if(!EVP_EncryptFinal(&ctx, outp, &outl)) {
			printf("EVP_EncryptFinal error\n");
			return -1;
		}
	}
	else {
		if(!EVP_DecryptFinal(&ctx, outp, &outl)) {
			printf("EVP_DecryptFinal error\n");
			return -1;
		}
	}
	outData->Length += outl;
	return 0;
}

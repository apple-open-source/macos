/*
 * Copyright (c) 2000-2001 Apple Computer, Inc. All Rights Reserved.
 * 
 * The contents of this file constitute Original Code as defined in and are
 * subject to the Apple Public Source License Version 1.2 (the 'License').
 * You may not use this file except in compliance with the License. Please obtain
 * a copy of the License at http://www.apple.com/publicsource and read it before
 * using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESS
 * OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES, INCLUDING WITHOUT
 * LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. Please see the License for the
 * specific language governing rights and limitations under the License.
 */


/*
	File:		symCipher.c

	Contains:	CDSA-based symmetric cipher module

	Written by:	Doug Mitchell

	Copyright: (c) 1999 by Apple Computer, Inc., all rights reserved.

*/

#include "sslContext.h"
#include "cryptType.h"
#include "sslDebug.h"
#include "sslMemory.h"
#include "appleCdsa.h"
#include "symCipher.h"

#include <Security/cssm.h>

#include <string.h>

/* dispose of dynamically allocated resources in a CipherContext */
static void disposeCipherCtx(
	CipherContext *cipherCtx)
{
	assert(cipherCtx != NULL);
	if(cipherCtx->symKey != NULL) {
		assert(cipherCtx->cspHand != 0);
		CSSM_FreeKey(cipherCtx->cspHand, NULL, cipherCtx->symKey, CSSM_FALSE);
		sslFree(cipherCtx->symKey);
		cipherCtx->symKey = NULL;
	}
	cipherCtx->cspHand = 0;
	if(cipherCtx->ccHand != 0) {
		CSSM_DeleteContext(cipherCtx->ccHand);
		cipherCtx->ccHand = 0;
	}
}

OSStatus CDSASymmInit(
	uint8 *key, 
	uint8* iv, 
	CipherContext *cipherCtx, 
	SSLContext *ctx)
{
	/*
	 * Cook up a symmetric key and a CCSM_CC_HANDLE. Assumes:
	 * 		cipherCtx->symCipher.keyAlg
	 * 		ctx->cspHand
	 * 		key (raw key bytes)
	 * On successful exit:
	 * 		Resulting CSSM_KEY_PTR --> cipherCtx->symKey
	 * 	 	Resulting CSSM_CC_HANDLE --> cipherCtx->ccHand
	 *      (Currently) a copy of ctx->cspHand --> cipherCtx->cspHand
	 *
	 * FIXME - for now we assume that ctx->cspHand is capable of 
	 * using the specified algorithm, keysize, and mode. This
	 * may need revisiting.
	 */
	
	OSStatus				serr = errSSLInternal;
	CSSM_RETURN			crtn;
	const SSLSymmetricCipher  *symCipher;
	CSSM_DATA			ivData;
	CSSM_DATA_PTR		ivDataPtr = NULL;
	CSSM_KEY_PTR		symKey = NULL;
	CSSM_CC_HANDLE		ccHand = 0;
	char				*op;
	
	assert(cipherCtx != NULL);
	assert(cipherCtx->symCipher != NULL);
	assert(ctx != NULL);
	if(ctx->cspHand == 0) {
		sslErrorLog("CDSASymmInit: NULL cspHand!\n");
		return errSSLInternal;
	}
	
	/* clean up cipherCtx  */
	disposeCipherCtx(cipherCtx);

	/* cook up a raw key */
	symKey = (CSSM_KEY_PTR)sslMalloc(sizeof(CSSM_KEY));
	if(symKey == NULL) {
		return memFullErr;
	}
	serr = sslSetUpSymmKey(symKey, cipherCtx->symCipher->keyAlg, 
		CSSM_KEYUSE_ENCRYPT | CSSM_KEYUSE_DECRYPT, CSSM_TRUE,
		key, cipherCtx->symCipher->keySize);
	if(serr) {
		sslFree(symKey);
		return serr;
	}
	
	cipherCtx->symKey = symKey;
	
	/* now the crypt handle */
	symCipher = cipherCtx->symCipher;
	if(symCipher->ivSize != 0) {
		ivData.Data = iv;
		ivData.Length = symCipher->ivSize;
		ivDataPtr = &ivData;
	}
	crtn = CSSM_CSP_CreateSymmetricContext(ctx->cspHand,
		symCipher->encrAlg,
		symCipher->encrMode,
		NULL, 
		symKey,
		ivDataPtr,
		symCipher->encrPad,
		0,						// Params
		&ccHand);
	if(crtn) {
		stPrintCdsaError("CSSM_CSP_CreateSymmetricContext", crtn);
		serr = errSSLCrypto;
		goto errOut;
	}
	cipherCtx->ccHand = ccHand;
	
	/* after this, each en/decrypt is merely an update */
	if(cipherCtx->encrypting) {
		crtn = CSSM_EncryptDataInit(ccHand);
		op = "CSSM_EncryptDataInit";
	}
	else {
		crtn = CSSM_DecryptDataInit(ccHand);
		op = "CSSM_DecryptDataInit";
	}
	if(crtn) {
		stPrintCdsaError("CSSM_CSP_EncryptDataInit", crtn);
		serr = errSSLCrypto;
		goto errOut;
	}
	
	/* success */
	cipherCtx->cspHand = ctx->cspHand;
	serr = noErr;
	
errOut:
	if(serr) {
		/* dispose of the stuff we created */
		disposeCipherCtx(cipherCtx);
	}
	return serr;
}

#define REDECRYPT_DATA		0

#define LOG_SYMM_DATA		0
#if		LOG_SYMM_DATA
static void logSymmData(
	char *field,
	SSLBuffer *data, 
	int maxLen)
{
	int i;
	
	printf("%s: ", field);
	for(i=0; i<data->length; i++) {
		if(i == maxLen) {
			break;
		}
		printf("%02X", data->data[i]);
		if((i % 4) == 3) {
			printf(" ");
		}
	}
	printf("\n");
}
#else	/* LOG_SYMM_DATA */
#define logSymmData(f, d, l)
#endif	/* LOG_SYMM_DATA */

#define IS_ALIGNED(count, blockSize)	((count % blockSize) == 0)

OSStatus CDSASymmEncrypt(
	SSLBuffer src, 
	SSLBuffer dest, 
	CipherContext *cipherCtx, 
	SSLContext *ctx)
{
	CSSM_RETURN			crtn;
	CSSM_DATA			ptextData;
	CSSM_DATA			ctextData;
	uint32				bytesEncrypted;
	OSStatus				serr = errSSLInternal;
	uint32				origLen = dest.length;
	
	/*
	 * Valid on entry:
	 * cipherCtx->ccHand
	 * cipherCtx->cspHand
	 */
	assert(ctx != NULL);
	assert(cipherCtx != NULL);
	logSymmData("Symm encrypt ptext", &src, 48);
	
	/* this requirement allows us to avoid a malloc and copy */
	assert(dest.length >= src.length);

	#if	SSL_DEBUG
	{
		unsigned blockSize = cipherCtx->symCipher->blockSize;
		if(blockSize) {
			if(!IS_ALIGNED(src.length, blockSize)) {
				sslErrorLog("CDSASymmEncrypt: unaligned ptext (len %ld bs %d)\n",
					src.length, blockSize);
				return errSSLInternal;
			}
			if(!IS_ALIGNED(dest.length, blockSize)) {
				sslErrorLog("CDSASymmEncrypt: unaligned ctext (len %ld bs %d)\n",
					dest.length, blockSize);
				return errSSLInternal;
			}
		}
	}
	#endif
	
	if((cipherCtx->ccHand == 0) || (cipherCtx->cspHand == 0)) {
		sslErrorLog("CDSASymmEncrypt: null args\n");
		return errSSLInternal;
	}
	SSLBUF_TO_CSSM(&src, &ptextData);
	SSLBUF_TO_CSSM(&dest, &ctextData);
	crtn = CSSM_EncryptDataUpdate(cipherCtx->ccHand,
		&ptextData,
		1,
		&ctextData,
		1,
		&bytesEncrypted);
	if(crtn) {
		stPrintCdsaError("CSSM_EncryptDataUpdate", crtn);
		serr = errSSLCrypto;
		goto errOut;
	}
	
	if(bytesEncrypted > origLen) {
		/* should never happen, callers always give us block-aligned
		 * plaintext and CSP padding is disabled. */
		sslErrorLog("Symmetric encrypt overflow: bytesEncrypted %ld destLen %ld\n",
			bytesEncrypted, dest.length);
		serr = errSSLCrypto;
		goto errOut;
	}
	dest.length = bytesEncrypted;
	logSymmData("Symm encrypt ctext", &dest, 48);
	serr = noErr;
	
errOut:
	return serr;
}

OSStatus CDSASymmDecrypt(
	SSLBuffer src, 
	SSLBuffer dest, 
	CipherContext *cipherCtx, 
	SSLContext *ctx)
{
	CSSM_RETURN			crtn;
	CSSM_DATA			ptextData = {0, NULL};
	CSSM_DATA			ctextData;
	uint32				bytesDecrypted;
	OSStatus				serr = errSSLInternal;
	uint32				origLen = dest.length;
	
	/*
	 * Valid on entry:
	 * cipherCtx->cspHand
	 * cipherCtx->ccHand
	 */
	assert(ctx != NULL);
	assert(cipherCtx != NULL);
	if((cipherCtx->ccHand == 0) || (cipherCtx->cspHand == 0)) {
		sslErrorLog("CDSASymmDecrypt: null args\n");
		return errSSLInternal;
	}
	/* this requirement allows us to avoid a malloc and copy */
	assert(dest.length >= src.length);
	
	#if	SSL_DEBUG
	{
		unsigned blockSize = cipherCtx->symCipher->blockSize;
		if(blockSize) {
			if(!IS_ALIGNED(src.length, blockSize)) {
				sslErrorLog("CDSASymmDecrypt: unaligned ctext (len %ld bs %d)\n",
					src.length, blockSize);
				return errSSLInternal;
			}
			if(!IS_ALIGNED(dest.length, blockSize)) {
				sslErrorLog("CDSASymmDecrypt: unaligned ptext (len %ld bs %d)\n",
					dest.length, blockSize);
				return errSSLInternal;
			}
		}
	}
	#endif

	SSLBUF_TO_CSSM(&src, &ctextData);
	SSLBUF_TO_CSSM(&dest, &ptextData);
	crtn = CSSM_DecryptDataUpdate(cipherCtx->ccHand,
		&ctextData,
		1,
		&ptextData,
		1,
		&bytesDecrypted);
	if(crtn) {
		stPrintCdsaError("CSSM_DecryptDataUpdate", crtn);
		serr = errSSLCrypto;
		goto errOut;
	}
	
	if(bytesDecrypted > origLen) {
		/* FIXME - can this happen? Should we remalloc? */
		sslErrorLog("Symmetric decrypt overflow: bytesDecrypted %ld destLen %ld\n",
			bytesDecrypted, dest.length);
		serr = errSSLCrypto;
		goto errOut;
	}
	dest.length = bytesDecrypted;
	serr = noErr;
	logSymmData("Symm decrypt ptext(1)", &dest, 48);
errOut:
	return serr;
}

OSStatus CDSASymmFinish(
	CipherContext *cipherCtx, 
	SSLContext *ctx)
{
	/* dispose of cipherCtx->{symKey,cspHand,ccHand} */
	disposeCipherCtx(cipherCtx);
	return noErr;
}


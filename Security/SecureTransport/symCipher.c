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

	Written by:	Doug Mitchell, based on Netscape RSARef 3.0

	Copyright: (c) 1999 by Apple Computer, Inc., all rights reserved.

*/
/*  *********************************************************************
    File: ciphers.c

    SSLRef 3.0 Final -- 11/19/96

    Copyright (c)1996 by Netscape Communications Corp.

    By retrieving this software you are bound by the licensing terms
    disclosed in the file "LICENSE.txt". Please read it, and if you don't
    accept the terms, delete this software.

    SSLRef 3.0 was developed by Netscape Communications Corp. of Mountain
    View, California <http://home.netscape.com/> and Consensus Development
    Corporation of Berkeley, California <http://www.consensus.com/>.

    *********************************************************************

    File: ciphers.c    Data structures for handling supported ciphers

    Contains a table mapping cipherSuite values to the ciphers, MAC
    algorithms, key exchange procedures and so on that are used for that
    algorithm, in order of preference.

    ****************************************************************** */

#include "sslctx.h"
#include "cryptType.h"
#include "sslDebug.h"
#include "sslalloc.h"
#include "appleCdsa.h"
#include "symCipher.h"

#include <Security/cssm.h>

#include <string.h>

/* dispose of dynamically allocated resources in a CipherContext */
static void disposeCipherCtx(
	CipherContext *cipherCtx)
{
	CASSERT(cipherCtx != NULL);
	if(cipherCtx->symKey != NULL) {
		CASSERT(cipherCtx->cspHand != 0);
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

SSLErr CDSASymmInit(
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
	
	SSLErr				serr = SSLInternalError;
	CSSM_RETURN			crtn;
	const SSLSymmetricCipher  *symCipher;
	CSSM_DATA			ivData;
	CSSM_DATA_PTR		ivDataPtr = NULL;
	CSSM_KEY_PTR		symKey = NULL;
	CSSM_CC_HANDLE		ccHand = 0;
	CSSM_KEYHEADER_PTR	hdr;
	char				*op;
	
	CASSERT(cipherCtx != NULL);
	CASSERT(cipherCtx->symCipher != NULL);
	CASSERT(ctx != NULL);
	if(ctx->cspHand == 0) {
		errorLog0("CDSASymmInit: NULL cspHand!\n");
		return SSLInternalError;
	}
	
	/* clean up cipherCtx  */
	disposeCipherCtx(cipherCtx);

	/* cook up a raw key */
	symKey = sslMalloc(sizeof(CSSM_KEY));
	if(symKey == NULL) {
		return SSLMemoryErr;
	}
	memset(symKey, 0, sizeof(CSSM_KEY));
	serr = stSetUpCssmData(&symKey->KeyData, cipherCtx->symCipher->keySize);
	if(serr) {
		sslFree(symKey);
		return serr;
	}
	memmove(symKey->KeyData.Data, key, cipherCtx->symCipher->keySize);
	
	/* set up the header */
	hdr = &symKey->KeyHeader;
	hdr->BlobType = CSSM_KEYBLOB_RAW;
	hdr->Format = CSSM_KEYBLOB_RAW_FORMAT_OCTET_STRING;
	hdr->AlgorithmId = cipherCtx->symCipher->keyAlg;
	hdr->KeyClass = CSSM_KEYCLASS_SESSION_KEY;
	hdr->LogicalKeySizeInBits = cipherCtx->symCipher->keySize * 8;
	hdr->KeyAttr = CSSM_KEYATTR_MODIFIABLE | CSSM_KEYATTR_EXTRACTABLE;
	hdr->KeyUsage = CSSM_KEYUSE_ENCRYPT | CSSM_KEYUSE_DECRYPT;
	hdr->WrapAlgorithmId = CSSM_ALGID_NONE;
	
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
		serr = SSLCryptoError;
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
		serr = SSLCryptoError;
		goto errOut;
	}
	
	/* success */
	cipherCtx->cspHand = ctx->cspHand;
	serr = SSLNoErr;
	
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

SSLErr CDSASymmEncrypt(
	SSLBuffer src, 
	SSLBuffer dest, 
	CipherContext *cipherCtx, 
	SSLContext *ctx)
{
	CSSM_RETURN			crtn;
	CSSM_DATA			ptextData;
	CSSM_DATA			ctextData = {0, NULL};
	uint32				bytesEncrypted;
	SSLErr				serr = SSLInternalError;
	
	/*
	 * Valid on entry:
	 * cipherCtx->ccHand
	 * cipherCtx->cspHand
	 */
	CASSERT(ctx != NULL);
	CASSERT(cipherCtx != NULL);
	logSymmData("Symm encrypt ptext", &src, 48);
	
	#if	SSL_DEBUG
	{
		unsigned blockSize = cipherCtx->symCipher->blockSize;
		if(blockSize) {
			if(!IS_ALIGNED(src.length, blockSize)) {
				errorLog2("CDSASymmEncrypt: unaligned ptext (len %ld bs %d)\n",
					src.length, blockSize);
				return SSLInternalError;
			}
			if(!IS_ALIGNED(dest.length, blockSize)) {
				errorLog2("CDSASymmEncrypt: unaligned ctext (len %ld bs %d)\n",
					dest.length, blockSize);
				return SSLInternalError;
			}
		}
	}
	#endif
	
	if((cipherCtx->ccHand == 0) || (cipherCtx->cspHand == 0)) {
		errorLog0("CDSASymmEncrypt: null args\n");
		return SSLInternalError;
	}
	SSLBUF_TO_CSSM(&src, &ptextData);
	crtn = CSSM_EncryptDataUpdate(cipherCtx->ccHand,
		&ptextData,
		1,
		&ctextData,
		1,
		&bytesEncrypted);
	if(crtn) {
		stPrintCdsaError("CSSM_EncryptDataUpdate", crtn);
		serr = SSLCryptoError;
		goto errOut;
	}
	
	if(bytesEncrypted > dest.length) {
		/* FIXME - can this happen? Should we remalloc? */
		errorLog2("Symmetric encrypt overflow: bytesEncrypted %ld destLen %ld\n",
			bytesEncrypted, dest.length);
		serr = SSLDataOverflow;
		goto errOut;
	}
	if(bytesEncrypted) {
		memmove(dest.data, ctextData.Data, bytesEncrypted);
	}
	dest.length = bytesEncrypted;
	
	/* CSP mallocd ctext  */
	/* FIXME - once we're really sure that the caller always mallocs
	 * dest.data, we should avoid this malloc/copy */
	stFreeCssmData(&ctextData, CSSM_FALSE);
	
	/* FIXME - sure we don't need to do Final()? */
	
	logSymmData("Symm encrypt ctext", &dest, 48);
	serr = SSLNoErr;
	
errOut:
	return serr;
}

SSLErr CDSASymmDecrypt(
	SSLBuffer src, 
	SSLBuffer dest, 
	CipherContext *cipherCtx, 
	SSLContext *ctx)
{
	CSSM_RETURN			crtn;
	CSSM_DATA			ptextData = {0, NULL};
	CSSM_DATA			ctextData;
	uint32				bytesDecrypted;
	SSLErr				serr = SSLInternalError;
		
	/*
	 * Valid on entry:
	 * cipherCtx->cspHand
	 * cipherCtx->ccHand
	 */
	CASSERT(ctx != NULL);
	CASSERT(cipherCtx != NULL);
	if((cipherCtx->ccHand == 0) || (cipherCtx->cspHand == 0)) {
		errorLog0("CDSASymmDecrypt: null args\n");
		return SSLInternalError;
	}
	
	#if	SSL_DEBUG
	{
		unsigned blockSize = cipherCtx->symCipher->blockSize;
		if(blockSize) {
			if(!IS_ALIGNED(src.length, blockSize)) {
				errorLog2("CDSASymmDecrypt: unaligned ctext (len %ld bs %d)\n",
					src.length, blockSize);
				return SSLInternalError;
			}
			if(!IS_ALIGNED(dest.length, blockSize)) {
				errorLog2("CDSASymmDecrypt: unaligned ptext (len %ld bs %d)\n",
					dest.length, blockSize);
				return SSLInternalError;
			}
		}
	}
	#endif

	SSLBUF_TO_CSSM(&src, &ctextData);
	crtn = CSSM_DecryptDataUpdate(cipherCtx->ccHand,
		&ctextData,
		1,
		&ptextData,
		1,
		&bytesDecrypted);
	if(crtn) {
		stPrintCdsaError("CSSM_DecryptDataUpdate", crtn);
		serr = SSLCryptoError;
		goto errOut;
	}
	
	if(bytesDecrypted > dest.length) {
		/* FIXME - can this happen? Should we remalloc? */
		errorLog2("Symmetric decrypt overflow: bytesDecrypted %ld destLen %ld\n",
			bytesDecrypted, dest.length);
		serr = SSLDataOverflow;
		goto errOut;
	}
	
	if(bytesDecrypted) {
		memmove(dest.data, ptextData.Data, bytesDecrypted);
	}

	/* CSP mallocd ptext, remData */
	stFreeCssmData(&ptextData, CSSM_FALSE);

	dest.length = bytesDecrypted;
	serr = SSLNoErr;
	logSymmData("Symm decrypt ptext(1)", &dest, 48);
errOut:
	return serr;
}

SSLErr CDSASymmFinish(
	CipherContext *cipherCtx, 
	SSLContext *ctx)
{
	/* dispose of cipherCtx->{symKey,cspHand,ccHand} */
	disposeCipherCtx(cipherCtx);
	return SSLNoErr;
}


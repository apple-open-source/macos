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
	File:		appleCdsa.c

	Contains:	interface between SSL and CDSA

	Written by:	Doug Mitchell

	Copyright: (c) 1999 by Apple Computer, Inc., all rights reserved.

*/

#include "ssl.h"
#include "sslctx.h"
#include "sslalloc.h"
#include "appleCdsa.h"
#include "sslerrs.h"
#include "sslutil.h"
#include "sslDebug.h"
#include "sslBER.h"
#include "ModuleAttacher.h"

#ifndef	_SSL_KEYCHAIN_H_
#include "sslKeychain.h"
#endif

#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include <Security/cssm.h>
#include <Security/cssmapple.h>

/* X.509 includes, from cssmapi */
#include <Security/x509defs.h>         /* x.509 function and type defs */
#include <Security/oidsalg.h>
#include <Security/oidscert.h>

#pragma mark *** Utilities ***

/*
 * Set up a Raw symmetric key with specified algorithm and key bits.
 */
SSLErr sslSetUpSymmKey(
	CSSM_KEY_PTR	symKey,
	CSSM_ALGORITHMS	alg,
	CSSM_KEYUSE		keyUse, 		// CSSM_KEYUSE_ENCRYPT, etc.
	CSSM_BOOL		copyKey,		// true: copy keyData   false: set by reference
	uint8 			*keyData,
	uint32			keyDataLen)		// in bytes
{
	SSLErr serr;
	CSSM_KEYHEADER *hdr;
	
	memset(symKey, 0, sizeof(CSSM_KEY));
	if(copyKey) {
		serr = stSetUpCssmData(&symKey->KeyData, keyDataLen);
		if(serr) {
			return serr;
		}
		memmove(symKey->KeyData.Data, keyData, keyDataLen);
	}
	else {
		symKey->KeyData.Data = keyData;
		symKey->KeyData.Length = keyDataLen;
	}
	
	/* set up the header */
	hdr = &symKey->KeyHeader;
	hdr->BlobType = CSSM_KEYBLOB_RAW;
	hdr->Format = CSSM_KEYBLOB_RAW_FORMAT_OCTET_STRING;
	hdr->AlgorithmId = alg;
	hdr->KeyClass = CSSM_KEYCLASS_SESSION_KEY;
	hdr->LogicalKeySizeInBits = keyDataLen * 8;
	hdr->KeyAttr = CSSM_KEYATTR_MODIFIABLE | CSSM_KEYATTR_EXTRACTABLE;
	hdr->KeyUsage = keyUse;
	hdr->WrapAlgorithmId = CSSM_ALGID_NONE;
	return SSLNoErr;
}

/*
 * Free a CSSM_KEY - its CSP resources, KCItemRef, and the key itself.
 */
SSLErr sslFreeKey(
	CSSM_CSP_HANDLE		cspHand,
	CSSM_KEY_PTR		*key,		/* so we can null it out */
	#if		ST_KEYCHAIN_ENABLE && ST_KC_KEYS_NEED_REF
	SecKeychainRef	*kcItem)
	#else	
	void			*kcItem) 
	#endif
{
	CASSERT(key != NULL);
	
	if(*key != NULL) {
		if(cspHand != 0) {
			CSSM_FreeKey(cspHand, NULL, *key, CSSM_FALSE);
		}
		sslFree(*key);
		*key = NULL;
	}
	#if		ST_KEYCHAIN_ENABLE && ST_KC_KEYS_NEED_REF
	if((kcItem != NULL) && (*kcItem != NULL)) {
		KCReleaseItem(kcItem);		/* does this NULL the referent? */
		*kcItem = NULL;
	}
	#endif
	return SSLNoErr;
}

/*
 * Standard app-level memory functions required by CDSA.
 */
void * stAppMalloc (uint32 size, void *allocRef) {
	return( malloc(size) );
}
void stAppFree (void *mem_ptr, void *allocRef) {
	free(mem_ptr);
 	return;
}
void * stAppRealloc (void *ptr, uint32 size, void *allocRef) {
	return( realloc( ptr, size ) );
}
void * stAppCalloc (uint32 num, uint32 size, void *allocRef) {
	return( calloc( num, size ) );
}

/*
 * Ensure there's a connection to ctx->cspHand. If there 
 * already is one, fine.
 * Note that as of 12/18/00, we assume we're connected to 
 * all modules all the time (since we do an attachToAll() in 
 * SSLNewContext()).
 */
SSLErr attachToCsp(SSLContext *ctx)
{
	CASSERT(ctx != NULL);
	if(ctx->cspHand != 0) {
		return SSLNoErr;
	}	
	else {
		return SSLAttachFailure;
	}
}

/* 
 * Connect to TP, CL; reusable.
 */
SSLErr attachToCl(SSLContext *ctx)
{
	CASSERT(ctx != NULL);
	if(ctx->clHand != 0) {
		return SSLNoErr;
	}
	else {
		return SSLAttachFailure;
	}
}

SSLErr attachToTp(SSLContext *ctx)
{
	CASSERT(ctx != NULL);
	if(ctx->tpHand != 0) {
		return SSLNoErr;
	}
	else {
		return SSLAttachFailure;
	}
}

/*
 * Convenience function - attach to CSP, CL, TP. Reusable. 
 */
SSLErr attachToAll(SSLContext *ctx)
{
	CSSM_RETURN crtn;
	
	CASSERT(ctx != NULL);
	crtn = attachToModules(&ctx->cspHand, &ctx->clHand, 
		&ctx->tpHand
		#if ST_FAKE_KEYCHAIN || ST_FAKE_GET_CSPDL_HANDLE
		,
		&ctx->cspDlHand
		#endif
		);
	if(crtn) {
	   return SSLAttachFailure;
	}
	else {
		return SSLNoErr;
	}
}

SSLErr detachFromAll(SSLContext *ctx)
{
	#if	0
	/* No more, attachments are kept on a global basis */
	CASSERT(ctx != NULL);
	if(ctx->cspHand != 0) {
		CSSM_ModuleDetach(ctx->cspHand);
		ctx->cspHand = 0;
	}
	if(ctx->tpHand != 0) {
		CSSM_ModuleDetach(ctx->tpHand);
		ctx->tpHand = 0;
	}
	if(ctx->clHand != 0) {
		CSSM_ModuleDetach(ctx->clHand);
		ctx->clHand = 0;
	}
	#endif	/* 0 */
	return SSLNoErr;
}

#pragma mark -
#pragma mark *** CSSM_DATA routines ***

CSSM_DATA_PTR stMallocCssmData(
	uint32 size)
{
	CSSM_DATA_PTR rtn = (CSSM_DATA_PTR)stAppMalloc(sizeof(CSSM_DATA), NULL);

	if(rtn == NULL) {
		return NULL;
	}
	rtn->Length = size;
	if(size == 0) {
		rtn->Data = NULL;
	}
	else {
		rtn->Data = (uint8 *)stAppMalloc(size, NULL);
	}
	return rtn;
}

void stFreeCssmData(
	CSSM_DATA_PTR data,
	CSSM_BOOL freeStruct)
{
	if(data == NULL) {
		return;
	}
	if(data->Data != NULL) {
		stAppFree(data->Data, NULL);
		data->Data   = NULL;
	}
	data->Length = 0;
	if(freeStruct) {
		stAppFree(data, NULL);
	}
}

/*
 * Ensure that indicated CSSM_DATA_PTR can handle 'length' bytes of data.
 * Malloc the Data ptr if necessary.
 */
SSLErr stSetUpCssmData(
	CSSM_DATA_PTR 	data,
	uint32 			length)
{
	CASSERT(data != NULL);
	if(data->Length == 0) {
		data->Data = (uint8 *)stAppMalloc(length, NULL);
		if(data->Data == NULL) {
			return SSLMemoryErr;
		}
	}
	else if(data->Length < length) {
		errorLog0("stSetUpCssmData: length too small\n");
		return SSLMemoryErr;
	}
	data->Length = length;
	return SSLNoErr;
}

#pragma mark -
#pragma mark *** Public CSP Functions ***

/*
 * Common RNG function; replaces SSLRef's SSLRandomFunc.
 * FIXME - just use /dev/random.
 */
SSLErr sslRand(SSLContext *ctx, SSLBuffer *buf)
{
	CSSM_RETURN 	crtn;
	CSSM_CC_HANDLE 	rngHand;
	CSSM_DATA		randData;
	SSLErr			serr;
	
	CASSERT(ctx != NULL);
	CASSERT(buf != NULL);
	CASSERT(buf->data != NULL);
	
	serr = attachToCsp(ctx);
	if(serr) {
		return serr;
	}
	if(buf->length == 0) {
		dprintf0("sslRand: zero buf->length\n");
		return SSLNoErr;
	}
	
	/*
	 * We happen to know that the CSP has a really good RNG
	 * seed if we don't specify anything; let's use it
	 */
	crtn = CSSM_CSP_CreateRandomGenContext(ctx->cspHand,
			CSSM_ALGID_APPLE_YARROW,
			NULL,				/* seed */
			buf->length,
			&rngHand);
	if(crtn) {
		stPrintCdsaError("CSSM_CSP_CreateRandomGenContext", crtn);
		return SSLCryptoError;
	}
	SSLBUF_TO_CSSM(buf, &randData);	
	crtn = CSSM_GenerateRandom(rngHand, &randData);
	if(crtn) {
		stPrintCdsaError("CSSM_GenerateRandom", crtn);
		serr = SSLCryptoError;
	}
	CSSM_DeleteContext(rngHand);
	return serr;
}

/*
 * Raw RSA sign/verify.
 *
 * Initial X port: CSP doesns't support this, so we'll do sign/verify via
 * raw RSA encrypt/decrypt here. 
 */
#define SIGN_VFY_VIA_ENCR_DECR	0

#if		SIGN_VFY_VIA_ENCR_DECR

SSLErr sslRsaRawSign(
	SSLContext			*ctx,
	const CSSM_KEY		*privKey,
	CSSM_CSP_HANDLE		cspHand,
	const UInt8			*plainText,
	UInt32				plainTextLen,
	UInt8				*sig,			// mallocd by caller; RETURNED
	UInt32				sigLen,			// available
	UInt32				*actualBytes)	// RETURNED
{
	/* Raw RSA sign with no digest is the same as raw RSA encrypt. */
	/* Force CSSM_KEYUSE_ANY in case CL provided keyuse bits more specific 
	 * than we really want */
	SSLErr serr;
	CSSM_KEYUSE savedKeyUse = privKey->KeyHeader.KeyUsage;
	privKey->KeyHeader.KeyUsage = CSSM_KEYUSE_ANY;
	serr = sslRsaEncrypt(ctx,
		privKey,
		cspHand,
		plainText,
		plainTextLen,
		sig,	
		sigLen,
		actualBytes);
	privKey->KeyHeader.KeyUsage = savedKeyUse;
	return serr;
}

SSLErr sslRsaRawVerify(
	SSLContext			*ctx,
	const CSSM_KEY		*pubKey,
	CSSM_CSP_HANDLE		cspHand,
	const UInt8			*plainText,
	UInt32				plainTextLen,
	const UInt8			*sig,
	UInt32				sigLen)	
{	
	/* 
	 * Raw RSA verify with no digest is just a comparison of the incoming
	 * plaintext with (signature, decrypted via raw RSA decrypt). 
	 */
	 
	UInt32 actualBytes;
	SSLErr serr;
	UInt8  *digest;
	
	/* Force CSSM_KEYUSE_ANY in case CL provided keyuse bits more specific 
	 * than we really want */
	CSSM_KEYUSE savedKeyUse = pubKey->KeyHeader.KeyUsage;
	pubKey->KeyHeader.KeyUsage = CSSM_KEYUSE_ANY;
	
	/* malloc space for decrypting the signature */
	digest = sslMalloc(plainTextLen);
	if(digest == NULL) {
		return SSLMemoryErr;
	}
	
	/* decrypt signature */
	serr = sslRsaDecrypt(ctx,
		pubKey,
		cspHand,
		sig,
		sigLen,		
		digest,			
		plainTextLen,	
		&actualBytes);
	pubKey->KeyHeader.KeyUsage = savedKeyUse;
	if(serr) {
		goto errOut;
	}
	if((actualBytes != plainTextLen) ||
	   (memcmp(plainText, digest, plainTextLen))) {
		errorLog0("sslRsaRawVerify: sig miscompare\n");
		serr = SSLCryptoError;
	}
	else {
		serr = SSLNoErr;
	}
errOut:
	sslFree(digest);
	return serr;
}

#else	/* OS9 and future post-cheetah version */

SSLErr sslRsaRawSign(
	SSLContext			*ctx,
	const CSSM_KEY		*privKey,
	CSSM_CSP_HANDLE		cspHand,
	const UInt8			*plainText,
	UInt32				plainTextLen,
	UInt8				*sig,			// mallocd by caller; RETURNED
	UInt32				sigLen,			// available
	UInt32				*actualBytes)	// RETURNED
{
	CSSM_CC_HANDLE			sigHand = 0;
	CSSM_RETURN				crtn;
	SSLErr					serr;
	CSSM_DATA				sigData;
	CSSM_DATA				ptextData;
	
	CASSERT(ctx != NULL);
	if((privKey == NULL) 	|| 
	   (cspHand == 0) 		|| 
	   (plainText == NULL)	|| 
	   (sig == NULL)		||
	   (actualBytes == NULL)) {
		errorLog0("sslRsaRawSign: bad arguments\n");
		return SSLInternalError;
	}
	*actualBytes = 0;
	
	crtn = CSSM_CSP_CreateSignatureContext(cspHand,
		CSSM_ALGID_RSA,
		NULL,				// passPhrase
		privKey,
		&sigHand);
	if(crtn) {
		stPrintCdsaError("CSSM_CSP_CreateSignatureContext (1)", crtn);
		return SSLCryptoError;
	}
	
	ptextData.Data = (uint8 *)plainText;
	ptextData.Length = plainTextLen;
	
	/* caller better get this right, or the SignData will fail */
	sigData.Data = sig;
	sigData.Length = sigLen;
	
	crtn = CSSM_SignData(sigHand,
		&ptextData,
		1,
		CSSM_ALGID_NONE,	// digestAlg 
		&sigData);
	if(crtn) {
		stPrintCdsaError("CSSM_SignData", crtn);
		serr = SSLCryptoError;
	}
	else {
		*actualBytes = sigData.Length;
		serr = SSLNoErr;
	}
	if(sigHand != 0) {
		CSSM_DeleteContext(sigHand);
	}
	return serr;
}

SSLErr sslRsaRawVerify(
	SSLContext			*ctx,
	const CSSM_KEY		*pubKey,
	CSSM_CSP_HANDLE		cspHand,
	const UInt8			*plainText,
	UInt32				plainTextLen,
	const UInt8			*sig,
	UInt32				sigLen)	
{
	CSSM_CC_HANDLE			sigHand = 0;
	CSSM_RETURN				crtn;
	SSLErr					serr;
	CSSM_DATA				sigData;
	CSSM_DATA				ptextData;
	
	CASSERT(ctx != NULL);
	if((pubKey == NULL) 	|| 
	   (cspHand == 0) 		|| 
	   (plainText == NULL)	|| 
	   (sig == NULL)) {
		errorLog0("sslRsaRawVerify: bad arguments\n");
		return SSLInternalError;
	}
	
	crtn = CSSM_CSP_CreateSignatureContext(cspHand,
		CSSM_ALGID_RSA,
		NULL,				// passPhrase
		pubKey,
		&sigHand);
	if(sigHand == 0) {
		stPrintCdsaError("CSSM_CSP_CreateSignatureContext (2)", crtn);
		return SSLCryptoError;
	}
	
	ptextData.Data = (uint8 *)plainText;
	ptextData.Length = plainTextLen;
	sigData.Data = (uint8 *)sig;
	sigData.Length = sigLen;
	
	crtn = CSSM_VerifyData(sigHand,
		&ptextData,
		1,
		CSSM_ALGID_NONE,		// digestAlg
		&sigData);
	if(crtn) {
		stPrintCdsaError("CSSM_VerifyData", crtn);
		serr = SSLCryptoError;
	}
	else {
		serr = SSLNoErr;
	}
	if(sigHand != 0) {
		CSSM_DeleteContext(sigHand);
	}
	return serr;
}
#endif	/* SIGN_VFY_VIA_ENCR_DECR */

/*
 * Encrypt/Decrypt
 */
#if		APPLE_DOMESTIC_CSP_REQUIRED

/*
 * Mucho work needed to get this functionality out of export CSP....
 */

SSLErr sslRsaEncrypt(
	SSLContext			*ctx,
	const CSSM_KEY		*pubKey,
	CSSM_CSP_HANDLE		cspHand,
	const UInt8			*plainText,
	UInt32				plainTextLen,
	UInt8				*cipherText,		// mallocd by caller; RETURNED 
	UInt32				cipherTextLen,		// available
	UInt32				*actualBytes)		// RETURNED
{
	CSSM_DATA 		ctextData = {0, NULL};
	CSSM_DATA 		ptextData;
	CSSM_DATA		remData = {0, NULL};
	CSSM_CC_HANDLE 	cryptHand = 0;
	SSLErr			serr = SSLInternalError;
	CSSM_RETURN		crtn;
	uint32			bytesMoved = 0;
	CSSM_ACCESS_CREDENTIALS	creds;
	
	CASSERT(ctx != NULL);
	CASSERT(actualBytes != NULL);
	*actualBytes = 0;
	
	if((pubKey == NULL) || (cspHand == 0)) {
		errorLog0("sslRsaEncrypt: bad pubKey/cspHand\n");
		return SSLInternalError;
	}
	
	#if		RSA_PUB_KEY_USAGE_HACK
	((CSSM_KEY_PTR)pubKey)->KeyHeader.KeyUsage |= CSSM_KEYUSE_ENCRYPT;
	#endif
	memset(&creds, 0, sizeof(CSSM_ACCESS_CREDENTIALS));
	
	crtn = CSSM_CSP_CreateAsymmetricContext(cspHand,
		CSSM_ALGID_RSA,
		&creds,
		pubKey,
		CSSM_PADDING_PKCS1,
		&cryptHand);
	if(crtn) {
		stPrintCdsaError("CSSM_CSP_CreateAsymmetricContext", crtn);
		return SSLCryptoError;
	}
	ptextData.Data = (uint8 *)plainText;
	ptextData.Length = plainTextLen;
	
	if(pubKey->KeyHeader.KeyClass == CSSM_KEYCLASS_PRIVATE_KEY) {
		/* 
		 * Special case, encrypting with private key (i.e., raw sign). Add
		 * the required context attr.
		 */
		CSSM_CONTEXT_ATTRIBUTE	modeAttr;
		
		modeAttr.AttributeType     = CSSM_ATTRIBUTE_MODE;
		modeAttr.AttributeLength   = sizeof(uint32);
		modeAttr.Attribute.Uint32  = CSSM_ALGMODE_PRIVATE_KEY;
		crtn = CSSM_UpdateContextAttributes(cryptHand, 1, &modeAttr);
		if(crtn) {
			stPrintCdsaError("CSSM_UpdateContextAttributes", crtn);
			CSSM_DeleteContext(cryptHand);
			return SSLCryptoError;
		}
	}
	
	/* 
	 * Have CSP malloc ciphertext 
	 */
	crtn = CSSM_EncryptData(cryptHand,
		&ptextData,
		1,
		&ctextData,
		1,
		&bytesMoved,
		&remData);
	if(crtn == CSSM_OK) {
		/* 
		 * ciphertext in both ctextData and remData; ensure it'll fit
		 * in caller's buf & copy 
		 */
		if(bytesMoved > cipherTextLen) {
			errorLog2("sslRsaEncrypt overflow; cipherTextLen %ld bytesMoved %ld\n",
				cipherTextLen, bytesMoved);
			serr = SSLDataOverflow;
		}
		else {
			UInt32 toMoveCtext;
			UInt32 toMoveRem;
			
			*actualBytes = bytesMoved;
			/* 
			 * Snag valid data from ctextData - its length or bytesMoved, 
			 * whichever is less
			 */
			if(ctextData.Length > bytesMoved) {
				/* everything's in ctext */
				toMoveCtext = bytesMoved;
				toMoveRem = 0;
			}
			else {
				/* must be some in remData too */
				toMoveCtext = ctextData.Length;
				toMoveRem = bytesMoved - toMoveCtext;		// remainder 
			}
			if(toMoveCtext) {
				memmove(cipherText, ctextData.Data, toMoveCtext);
			}
			if(toMoveRem) {
				memmove(cipherText + toMoveCtext, remData.Data,
					toMoveRem);
			}
			serr = SSLNoErr;
		}
	}
	else {
		stPrintCdsaError("CSSM_EncryptData", crtn);
		serr = SSLCryptoError;
	}
	if(cryptHand != 0) {
		CSSM_DeleteContext(cryptHand);
	}

	/* free data mallocd by CSP */
	stFreeCssmData(&ctextData, CSSM_FALSE);
	stFreeCssmData(&remData, CSSM_FALSE);
	return serr;
}

SSLErr sslRsaDecrypt(
	SSLContext			*ctx,
	const CSSM_KEY		*privKey,
	CSSM_CSP_HANDLE		cspHand,
	const UInt8			*cipherText,
	UInt32				cipherTextLen,		
	UInt8				*plainText,			// mallocd by caller; RETURNED
	UInt32				plainTextLen,		// available
	UInt32				*actualBytes)		// RETURNED
{
	CSSM_DATA 		ptextData = {0, NULL};
	CSSM_DATA 		ctextData;
	CSSM_DATA		remData = {0, NULL};
	CSSM_CC_HANDLE 	cryptHand = 0;
	SSLErr			serr = SSLInternalError;
	CSSM_RETURN		crtn;
	uint32			bytesMoved = 0;
	CSSM_ACCESS_CREDENTIALS	creds;
		
	CASSERT(ctx != NULL);
	CASSERT(actualBytes != NULL);
	*actualBytes = 0;
	
	if((privKey == NULL) || (cspHand == 0)) {
		errorLog0("sslRsaDecrypt: bad privKey/cspHand\n");
		return SSLInternalError;
	}
	memset(&creds, 0, sizeof(CSSM_ACCESS_CREDENTIALS));
	crtn = CSSM_CSP_CreateAsymmetricContext(cspHand,
		CSSM_ALGID_RSA,
		&creds,
		privKey,
		CSSM_PADDING_PKCS1,
		&cryptHand);
	if(crtn) {
		stPrintCdsaError("CSSM_CSP_CreateAsymmetricContext", crtn);
		return SSLCryptoError;
	}
	ctextData.Data = (uint8 *)cipherText;
	ctextData.Length = cipherTextLen;
	
	if(privKey->KeyHeader.KeyClass == CSSM_KEYCLASS_PUBLIC_KEY) {
		/* 
		 * Special case, decrypting with public key (i.e., raw verify). Add
		 * the required context attr.
		 */
		CSSM_CONTEXT_ATTRIBUTE	modeAttr;
		
		modeAttr.AttributeType     = CSSM_ATTRIBUTE_MODE;
		modeAttr.AttributeLength   = sizeof(uint32);
		modeAttr.Attribute.Uint32  = CSSM_ALGMODE_PUBLIC_KEY;
		crtn = CSSM_UpdateContextAttributes(cryptHand, 1, &modeAttr);
		if(crtn) {
			stPrintCdsaError("CSSM_UpdateContextAttributes", crtn);
			CSSM_DeleteContext(cryptHand);
			return SSLCryptoError;
		}
	}

	/* 
	 * Have CSP malloc plaintext 
	 */
	crtn = CSSM_DecryptData(cryptHand,
		&ctextData,
		1,
		&ptextData,
		1,
		&bytesMoved,
		&remData);
	if(crtn == CSSM_OK) {
		/* 
		 * plaintext in both ptextData and remData; ensure it'll fit
		 * in caller's buf & copy 
		 */
		if(bytesMoved > plainTextLen) {
			errorLog2("sslRsaDecrypt overflow; plainTextLen %ld bytesMoved %ld\n",
				plainTextLen, bytesMoved);
			serr = SSLDataOverflow;
		}
		else {
			UInt32 toMovePtext;
			UInt32 toMoveRem;
			
			*actualBytes = bytesMoved;
			/* 
			 * Snag valid data from ptextData - its length or bytesMoved, 
			 * whichever is less
			 */
			if(ptextData.Length > bytesMoved) {
				/* everything's in ptext */
				toMovePtext = bytesMoved;
				toMoveRem = 0;
			}
			else {
				/* must be some in remData too */
				toMovePtext = ptextData.Length;
				toMoveRem = bytesMoved - toMovePtext;		// remainder 
			}
			if(toMovePtext) {
				memmove(plainText, ptextData.Data, toMovePtext);
			}
			if(toMoveRem) {
				memmove(plainText + toMovePtext, remData.Data,
					toMoveRem);
			}
			serr = SSLNoErr;
		}
	}
	else {
		stPrintCdsaError("CSSM_DecryptData", crtn);
		serr = SSLCryptoError;
	}
	if(cryptHand != 0) {
		CSSM_DeleteContext(cryptHand);
	}
	
	/* free data mallocd by CSP */
	stFreeCssmData(&ptextData, CSSM_FALSE);
	stFreeCssmData(&remData, CSSM_FALSE);
	return serr;
}

#endif	/* APPLE_DOMESTIC_CSP_REQUIRED */

/*
 * Obtain size of key in bytes.
 */
UInt32 sslKeyLengthInBytes(const CSSM_KEY *key)
{
	CASSERT(key != NULL);
	return (((key->KeyHeader.LogicalKeySizeInBits) + 7) / 8);
}

/*
 * Get raw key bits from an RSA public key.
 */
SSLErr sslGetPubKeyBits(
	SSLContext			*ctx,
	const CSSM_KEY		*pubKey,
	CSSM_CSP_HANDLE		cspHand,
	SSLBuffer			*modulus,		// data mallocd and RETURNED
	SSLBuffer			*exponent)		// data mallocd and RETURNED
{
	CSSM_KEY			wrappedKey;
	CSSM_BOOL			didWrap = CSSM_FALSE;
	const CSSM_KEYHEADER *hdr;
	CSSM_CC_HANDLE 		ccHand;
	CSSM_RETURN			crtn;
	SSLBuffer			pubKeyBlob;
	SSLErr				srtn;
	CSSM_ACCESS_CREDENTIALS	creds;
	
	CASSERT(ctx != NULL);
	CASSERT(modulus != NULL);
	CASSERT(exponent != NULL);
	CASSERT(pubKey != NULL);
	
	hdr = &pubKey->KeyHeader;
	if(hdr->KeyClass != CSSM_KEYCLASS_PUBLIC_KEY) {
		errorLog1("sslGetPubKeyBits: bad keyClass (%ld)\n", hdr->KeyClass);
		return SSLInternalError;
	}
	if(hdr->AlgorithmId != CSSM_ALGID_RSA) {
		errorLog1("sslGetPubKeyBits: bad AlgorithmId (%ld)\n", hdr->AlgorithmId);
		return SSLInternalError;
	}
	
	/* 
	 * Handle possible reference format - I think it should be in
	 * blob form since it came from the DL, but conversion is 
	 * simple.
	 */
	switch(hdr->BlobType) {
		case CSSM_KEYBLOB_RAW:
			/* easy case */
			CSSM_TO_SSLBUF(&pubKey->KeyData, &pubKeyBlob);
			break;

		case CSSM_KEYBLOB_REFERENCE:
			/* 
			 * Convert to a blob via "NULL wrap"; no wrapping key, 
			 * ALGID_NONE 
			 */ 
			srtn = attachToCsp(ctx);
			if(srtn) {
				return srtn;
			}
			memset(&creds, 0, sizeof(CSSM_ACCESS_CREDENTIALS));
			crtn = CSSM_CSP_CreateSymmetricContext(ctx->cspHand,
					CSSM_ALGID_NONE,
					CSSM_ALGMODE_NONE,
					&creds,			// creds
					pubKey,
					NULL,			// InitVector
					CSSM_PADDING_NONE,
					0,			// reserved
					&ccHand);
			if(crtn) {
				stPrintCdsaError("sslGetPubKeyBits: CreateSymmetricContext failure", crtn); 
				return SSLMemoryErr;
			}
			memset(&wrappedKey, 0, sizeof(CSSM_KEY));
			crtn = CSSM_WrapKey(ccHand,
				&creds,
				pubKey,
				NULL,			// descriptiveData
				&wrappedKey);
			CSSM_DeleteContext(ccHand);
			if(crtn) {
				stPrintCdsaError("CSSM_WrapKey", crtn);
				return SSLCryptoError;
			}
			hdr = &wrappedKey.KeyHeader;
			if(hdr->BlobType != CSSM_KEYBLOB_RAW) {
				errorLog1("sslGetPubKeyBits: bad BlobType (%ld) after WrapKey\n", 
					hdr->BlobType);
				return SSLCryptoError;
			}
			didWrap = CSSM_TRUE;
			CSSM_TO_SSLBUF(&wrappedKey.KeyData, &pubKeyBlob);
			break;

		default:
			errorLog1("sslGetPubKeyBits: bad BlobType (%ld)\n", 
				hdr->BlobType);
			return SSLInternalError;
	
	}	/* switch BlobType */

	CASSERT(hdr->BlobType == CSSM_KEYBLOB_RAW); 
	srtn = sslDecodeRsaBlob(&pubKeyBlob, modulus, exponent);
	if(didWrap) {
		CSSM_FreeKey(ctx->cspHand, NULL, &wrappedKey, CSSM_FALSE);
	}
	return srtn;
}

/*
 * Given raw RSA key bits, cook up a CSSM_KEY_PTR. Used in 
 * Server-initiated key exchange. 
 */
SSLErr sslGetPubKeyFromBits(
	SSLContext			*ctx,
	const SSLBuffer		*modulus,	
	const SSLBuffer		*exponent,	
	CSSM_KEY_PTR		*pubKey,		// mallocd and RETURNED
	CSSM_CSP_HANDLE		*cspHand)		// RETURNED
{
	CSSM_KEY_PTR		key = NULL;
	SSLErr				serr;
	SSLBuffer			blob;
	CSSM_KEYHEADER_PTR	hdr;
	CSSM_KEY_SIZE		keySize;
	CSSM_RETURN			crtn;
	
	CASSERT((ctx != NULL) && (modulus != NULL) && (exponent != NULL));
	CASSERT((pubKey != NULL) && (cspHand != NULL));
	
	*pubKey = NULL;
	*cspHand = 0;
	
	serr = attachToCsp(ctx);
	if(serr) {
		return serr;
	}
	serr = sslEncodeRsaBlob(modulus, exponent, &blob);
	if(serr) {
		return serr;
	}
	
	/* the rest is boilerplate, cook up a good-looking public key */
	key = sslMalloc(sizeof(CSSM_KEY));
	if(key == NULL) {
		return SSLMemoryErr;
	}
	memset(key, 0, sizeof(CSSM_KEY));
	hdr = &key->KeyHeader;
	
    hdr->HeaderVersion = CSSM_KEYHEADER_VERSION;
    /* key_ptr->KeyHeader.CspId is unknown (remains 0) */
    hdr->BlobType = CSSM_KEYBLOB_RAW;
    hdr->AlgorithmId = CSSM_ALGID_RSA;
    hdr->Format = CSSM_KEYBLOB_RAW_FORMAT_PKCS1;
    hdr->KeyClass = CSSM_KEYCLASS_PUBLIC_KEY;
    /* comply with ASA requirements */
    hdr->KeyUsage = CSSM_KEYUSE_VERIFY;
    hdr->KeyAttr = CSSM_KEYATTR_EXTRACTABLE;
    /* key_ptr->KeyHeader.StartDate is unknown  (remains 0) */
    /* key_ptr->KeyHeader.EndDate is unknown  (remains 0) */
    hdr->WrapAlgorithmId = CSSM_ALGID_NONE;
    hdr->WrapMode = CSSM_ALGMODE_NONE;

	/* blob->data was mallocd by sslEncodeRsaBlob, pass it over to 
	 * actual key */
	SSLBUF_TO_CSSM(&blob, &key->KeyData);
	
	/*
	 * Get keySizeInBits. This also serves to validate the key blob
	 * we just cooked up.
	 */
    crtn = CSSM_QueryKeySizeInBits(ctx->cspHand, CSSM_INVALID_HANDLE, key, &keySize);
	if(crtn) {	
    	stPrintCdsaError("sslGetPubKeyFromBits: QueryKeySizeInBits\n", crtn);
		serr = SSLCryptoError;
    	goto abort;
	}
	
	/* success */
    hdr->LogicalKeySizeInBits = keySize.EffectiveKeySizeInBits;
    *pubKey = key;
    *cspHand = ctx->cspHand;
	return SSLNoErr;
	
abort:
	/* note this frees the blob */
	sslFreeKey(ctx->cspHand, &key, NULL);
	return serr;
}

#pragma mark -
#pragma mark *** Public Certificate Functions ***

/*
 * Given a DER-encoded cert, obtain its public key as a CSSM_KEY_PTR.
 * Caller must CSSM_FreeKey and free the CSSM_KEY_PTR itself. 
 *
 * For now, the returned cspHand is a copy of ctx->cspHand, so it
 * doesn't have to be detached later - this may change.
 *
 * Update: since CSSM_CL_CertGetKeyInfo() doesn't provide a means for
 * us to tell the CL what CSP to use, we really have no way of knowing 
 * what is going on here...we return the process-wide (bare) cspHand,
 * which is currently always able to deal with this raw public key. 
 */
SSLErr sslPubKeyFromCert(
	SSLContext 			*ctx,
	const SSLBuffer		*derCert,
	CSSM_KEY_PTR		*pubKey,		// RETURNED
	CSSM_CSP_HANDLE		*cspHand)		// RETURNED
{
	SSLErr 			serr;
	CSSM_DATA		certData;
	CSSM_RETURN		crtn;
	
	CASSERT(ctx != NULL);
	CASSERT(derCert != NULL);
	CASSERT(pubKey != NULL);
	CASSERT(cspHand != NULL);
	
	*pubKey = NULL;
	*cspHand = 0;
	
	serr = attachToCl(ctx);
	if(serr) {
		return serr;
	}
	serr = attachToCsp(ctx);
	if(serr) {
		return serr;
	}
	SSLBUF_TO_CSSM(derCert, &certData);
	crtn = CSSM_CL_CertGetKeyInfo(ctx->clHand, &certData, pubKey);
	if(crtn) {
		return SSLBadCert;
	}
	else {
		*cspHand = ctx->cspHand; 
		return SSLNoErr;
	}
}

#if		0

#include <Files.h>
#include <Errors.h>

/* for writing root cert to a file */

static OSErr writeBlob(const CSSM_DATA_PTR blob, 
	const char *fileName)
{
	OSErr	err = noErr;
	FSSpec	fsp;
	short	fileRef;
	long	count = blob->Length;
	int		len = strlen(fileName);
	
	fsp.vRefNum = 0;
	fsp.parID = 0;
	fsp.name[0] = len;
	memmove(&fsp.name[1], fileName, len);

	err = FSpCreate(&fsp, 0, 0, 0);
	if(err && (err != dupFNErr)) {
		dprintf1("***FSpCreate() returned %d\n", err);
		return err;
	}
	err = FSpOpenDF(&fsp, fsRdWrPerm, &fileRef);
	if(err) {
		dprintf1("***FSpOpenDF() returned %d\n", err);
		return err;
	}
	err = FSWrite(fileRef, &count, blob->Data);
	if(err) {
		dprintf1("***FSWrite() returned %d\n", err);
		return err;
	}
	err = FSClose(fileRef);
	if(err) {
		dprintf1("***FSClose() returned %d\n", err);
		return err;
	}
	return 0;
}

void writeBufBlob(const SSLBuffer *blob, 
	const char *fileName)
{
	CSSM_DATA	d;
	
	SSLBUF_TO_CSSM(blob, &d)
	writeBlob(&d, fileName);
}

#endif	/* 0 */

#if		ST_KEYCHAIN_ENABLE && ST_MANAGES_TRUSTED_ROOTS

/*
 * Given a CSSM_CERTGROUP which fails due to CSSM_TP_INVALID_ANCHOR
 * (chain verifies to an unknown root):
 *
 * -- find the root cert
 * -- add it to newRootCertKc if present (else error)
 * -- add it to trustedCerts
 * -- re-verify certgroup, demand full success
 */
static SSLErr sslHandleNewRoot(
	SSLContext				*ctx,
	CSSM_CERTGROUP_PTR		certGroup)
{
	int 			i;
	CSSM_DATA_PTR	rootCert;
	CSSM_BOOL		expired;
	SSLErr			serr;
	CSSM_BOOL		brtn;
	
	CASSERT(ctx != NULL);
	CASSERT(certGroup != NULL);
	
	if(ctx->newRootCertKc == NULL) {
		/* no place to add this; done */
		return SSLUnknownRootCert;
	}
	
	/*
	 * The root cert "should" be at the end of the chain, but 
	 * let's not assume that. (We are assuming that there is 
	 * only one root in the cert group...)
	 */
	for(i=0; i<certGroup->NumCerts; i++) {
		rootCert = &certGroup->CertList[i];
		if(sslVerifyCert(ctx, rootCert, rootCert, ctx->cspHand, &expired)) {
			break;
		}
	}
	if(i == certGroup->NumCerts) {
		/* Huh! no root cert!? We should not have been called! */
		errorLog0("sslHandleNewRoot: no root cert!\n");
		return SSLInternalError;
	}
	
	/*
	 * Add to newRootCertKc. This may well fail due to user interaction.	
	 */
	serr = sslAddNewRoot(ctx, rootCert);
	if(serr) {
		return serr;
	}
	
	/*
	 * Just to be sure...reverify the whole cert chain. 
	 */
	brtn = CSSM_TP_CertGroupVerify(
		ctx->tpHand,
		ctx->clHand,
		ctx->cspHand,
		NULL,					// DBList
		NULL, 					// PolicyIdentifiers
		0,						// NumberofPolicyIdentifiers
		CSSM_TP_STOP_ON_POLICY, 
		certGroup,
		ctx->trustedCerts,		// AnchorCerts
		ctx->numTrustedCerts, 
		NULL,					// VerifyScope
		0,						// ScopeSize
		0,						// Action
		0,						// Data
		NULL,					// evidence
		NULL);					// evidenceSize
	if(brtn == CSSM_FALSE) {
		errorLog0("sslHandleNewRoot: adding new root did not help!\n");
		return SSLUnknownRootCert;
	}
	return SSLNoErr;
}

#endif	/* ST_KEYCHAIN_ENABLE && ST_MANAGES_TRUSTED_ROOTS */

/* free a CSSM_CERT_GROUP */ 
static void sslFreeCertGroup(
	CSSM_CERTGROUP_PTR	certGroup,
	CSSM_BOOL			freeCerts,	// free individual cert fields
	CSSM_BOOL			freeStruct)	// free the overall CSSM_CERTGROUP
{
	unsigned dex;
	
	if(certGroup == NULL) {
		return;	
	}
	
	/* free the individual cert Data fields */
	if(certGroup->GroupList.CertList) {
		if(freeCerts) {
			for(dex=0; dex<certGroup->NumCerts; dex++) {
				stFreeCssmData(&certGroup->GroupList.CertList[dex], CSSM_FALSE);
			}
		}
		/* and the array of CSSM_DATAs */
		stAppFree(certGroup->GroupList.CertList, NULL);
	}
	if(freeStruct) {
		stAppFree(certGroup, NULL);
	}
}

/*
 * Verify a chain of DER-encoded certs.
 * First cert in a chain is root; this must also be present
 * in ctx->trustedCerts. 
 */
SSLErr sslVerifyCertChain(
	SSLContext				*ctx,
	const SSLCertificate	*certChain)
{
	UInt32 						numCerts;
	CSSM_CERTGROUP				certGroup;
	int 						i;
	SSLErr						serr;
	SSLCertificate				*c = (SSLCertificate *)certChain;
	CSSM_RETURN					crtn;
	CSSM_TP_VERIFY_CONTEXT		vfyCtx;
	CSSM_TP_CALLERAUTH_CONTEXT	authCtx;
	CSSM_FIELD					policyId;
	CSSM_DL_DB_LIST				dbList;
	CSSM_APPLE_TP_SSL_OPTIONS	sslOpts;
	CSSM_APPLE_TP_ACTION_DATA	actionData;
	
	/* FIXME - allowAnyRoot should probably mean "return success" with 
	 * no checking */
	 
	numCerts = SSLGetCertificateChainLength(certChain);
	if(numCerts == 0) {
		/* nope */
		return SSLBadCert;
	}
	#if 0
	serr = attachToAll(ctx);
	if(serr) {
		return serr;
	}
	#endif
	
	/* 
	 * SSLCertificate chain --> CSSM TP cert group.
	 * TP Cert group has root at the end, opposite of 
	 * SSLCertificate chain. 
	 */
	certGroup.GroupList.CertList = 
		(CSSM_DATA_PTR)sslMalloc(numCerts * sizeof(CSSM_DATA));
	if(certGroup.GroupList.CertList == NULL) {
		return SSLMemoryErr;
	}
	certGroup.CertGroupType = CSSM_CERTGROUP_DATA;
	certGroup.CertType = CSSM_CERT_X_509v3;
	certGroup.CertEncoding = CSSM_CERT_ENCODING_DER; 
	certGroup.NumCerts = numCerts;
	
	memset(certGroup.GroupList.CertList, 0, numCerts * sizeof(CSSM_DATA));
	
	for(i=numCerts-1; i>=0; i--) {
		SSLBUF_TO_CSSM(&c->derCert, &certGroup.GroupList.CertList[i]);
		c = c->next;
	}
	
	memset(&vfyCtx, 0, sizeof(CSSM_TP_VERIFY_CONTEXT));
	vfyCtx.Action = CSSM_TP_ACTION_DEFAULT;
	vfyCtx.Cred = &authCtx;
	
	/* CSSM_TP_CALLERAUTH_CONTEXT components */
	/* 
		typedef struct cssm_tp_callerauth_context {
			CSSM_TP_POLICYINFO Policy;
			CSSM_TIMESTRING VerifyTime;
			CSSM_TP_STOP_ON VerificationAbortOn;
			CSSM_TP_VERIFICATION_RESULTS_CALLBACK CallbackWithVerifiedCert;
			uint32 NumberOfAnchorCerts;
			CSSM_DATA_PTR AnchorCerts;
			CSSM_DL_DB_LIST_PTR DBList;
			CSSM_ACCESS_CREDENTIALS_PTR CallerCredentials;
		} CSSM_TP_CALLERAUTH_CONTEXT, *CSSM_TP_CALLERAUTH_CONTEXT_PTR;
	*/
	
	/* SSL-specific FieldValue */
	sslOpts.Version = CSSM_APPLE_TP_SSL_OPTS_VERSION;
	sslOpts.ServerNameLen = ctx->peerDomainNameLen;
	sslOpts.ServerName = ctx->peerDomainName;
	
	/* TP-wide ActionData */
	actionData.Version = CSSM_APPLE_TP_ACTION_VERSION;
	actionData.ActionFlags = 0x80000000;	// @@@ secret root-cert-enable 
	if(ctx->allowExpiredCerts) {
		actionData.ActionFlags |= CSSM_TP_ACTION_ALLOW_EXPIRED;
	}
	vfyCtx.ActionData.Data = (uint8 *)&actionData;
	vfyCtx.ActionData.Length = sizeof(actionData);
	
	/* zero or one policy here */
	policyId.FieldOid = CSSMOID_APPLE_TP_SSL;
	policyId.FieldValue.Data = (uint8 *)&sslOpts;
	policyId.FieldValue.Length = sizeof(sslOpts);
	authCtx.Policy.NumberOfPolicyIds = 1;
	authCtx.Policy.PolicyIds = &policyId;
	
	authCtx.VerifyTime = NULL;
	authCtx.VerificationAbortOn = CSSM_TP_STOP_ON_POLICY;
	authCtx.CallbackWithVerifiedCert = NULL;
	authCtx.NumberOfAnchorCerts = ctx->numTrustedCerts;
	authCtx.AnchorCerts = ctx->trustedCerts;	
	memset(&dbList, 0, sizeof(CSSM_DL_DB_LIST));
	authCtx.DBList = &dbList;
	authCtx.CallerCredentials = NULL;

	/*
	 * Here we go; hand it over to TP. Note trustedCerts are our
	 * known good Anchor certs; they're already formatted properly. 
	 * Unlike most other Apple code, we demand full success here, 
	 * implying that the last cert in the chain is indeed an Anchor
	 * cert. We already know that all of our anchor certs are
	 * roots, so on successful return, we'll know the incoming 
	 * chain has a root, it verifies to that root, and that that
	 * root is in trustedCerts.  
	 */
	crtn = CSSM_TP_CertGroupVerify(ctx->tpHand,
		ctx->clHand,
		ctx->cspHand,
		&certGroup,
		&vfyCtx,
		NULL);			// no evidence needed

	serr = SSLNoErr;
	if(crtn) {	
		/* get some detailed error info */
		switch(crtn) {
			case CSSMERR_TP_INVALID_ANCHOR_CERT: 
				/* root found but we don't trust it */
				if(ctx->allowAnyRoot) {
					dprintf0("***Warning: accepting unknown root cert\n");
					break;
				}
				#if		ST_KEYCHAIN_ENABLE && ST_MANAGES_TRUSTED_ROOTS
				if(ctx->newRootCertKc != NULL) {
					/* see if user wants to handle new root */
					serr = sslHandleNewRoot(ctx, &certGroup);
				}
				else {
					serr = SSLUnknownRootCert;
				}
				#else
				serr = SSLUnknownRootCert;
				#endif	/* ST_KEYCHAIN_ENABLE && ST_MANAGES_TRUSTED_ROOTS */
				break;
			case CSSMERR_TP_NOT_TRUSTED:
				/* no root, not even in implicit SSL roots */
				if(ctx->allowAnyRoot) {
					dprintf0("***Warning: accepting unverified cert chain\n");
					break;
				}
				serr = SSLNoRootCert;
				break;
			case CSSMERR_TP_CERT_EXPIRED:
				assert(!ctx->allowExpiredCerts);
				serr = SSLCertExpired;
				break;
			case CSSMERR_TP_CERT_NOT_VALID_YET:
				serr = SSLCertNotYetValid;
				break;
			default:
				stPrintCdsaError(
					"sslVerifyCertChain: CSSM_TP_CertGroupVerify returned", 					crtn);
				serr = X509CertChainInvalidErr;
				break;
		}
	} 	/* brtn FALSE */

	/* 
	 * don't free individual certs - caller still owns them
	 * don't free struct - on stack 
	 */
	sslFreeCertGroup(&certGroup, CSSM_FALSE, CSSM_FALSE);
	return serr;
}


#if 0
/* not needed in X */

/*
 * Given two certs, verify subjectCert with issuerCert. Returns 
 * CSSM_TRUE on successful verify.
 * Only special case on error is "subject cert expired", indicated by
 * *subjectExpired returned as CSSM_TRUE.
 */
CSSM_BOOL sslVerifyCert(
	SSLContext				*ctx,
	const CSSM_DATA_PTR		subjectCert,
	const CSSM_DATA_PTR		issuerCert,
	CSSM_CSP_HANDLE			cspHand,			// can verify with issuerCert
	CSSM_BOOL				*subjectExpired)	// RETURNED
{
	CSSM_KEY_PTR		issuerPubKey = NULL;
	CSSM_DATA_PTR		sigOid = NULL;
	CSSM_HANDLE 		ResultsHandle;
	uint32 				NumberOfFields;
	CSSM_ERROR_PTR 		pErr = NULL;
	CSSM_BOOL			brtn;
	uint32				*algId = NULL;		// mallocd by CL_Passthrough
	CSSM_CC_HANDLE		ccHand = 0;
	
	*subjectExpired = CSSM_FALSE;

	/* ensure connection to CL, TP */
	if(attachToCl(ctx)) {
		return CSSM_FALSE;
	}
	if(attachToTp(ctx)) {
		return CSSM_FALSE;
	}
	
	/* public key from issuer cert */	
	issuerPubKey = CSSM_CL_CertGetKeyInfo(ctx->clHand, issuerCert);
	if(issuerPubKey == NULL) {
		return CSSM_FALSE;
	}
	/* subsequent errors to abort: */
	
	/* signature alg from subject cert */
    sigOid = CSSM_CL_CertGetFirstFieldValue(ctx->clHand, 
    	subjectCert,
    	&CSSMOID_X509V1SignatureAlgorithm,
		&ResultsHandle, 
		&NumberOfFields);
	if(sigOid == NULL) {
		stPrintCdsaError("CSSM_CL_CertGetFirstFieldValue");
		brtn = CSSM_FALSE;
    	CSSM_CL_CertAbortQuery(ctx->clHand, ResultsHandle);
		goto abort;
	}
    /* cleanup query state */
    CSSM_CL_CertAbortQuery(ctx->clHand, ResultsHandle);

	/* convert: alg OID to CSSM_ALGID_xxx */
	algId = (uint32 *)CSSM_CL_PassThrough(ctx->clHand,
			0,			// no handle needed
			INTEL_X509V3_PASSTHROUGH_ALGOID_TO_ALGID,
			sigOid);
	if(*algId == CSSM_ALGID_NONE) {
		brtn = CSSM_FALSE;
		goto abort;
	}

	/* set up a sign context with obtained pub key and algorithm */		
	ccHand = CSSM_CSP_CreateSignatureContext(cspHand,
		*algId,
		NULL,		// no passphrase
		issuerPubKey);
	if(ccHand == 0) {
		brtn = CSSM_FALSE;
		goto abort;
	}

	/* go for it - CL takes over from here */
    brtn = CSSM_CL_CertVerify(ctx->clHand, 
    	ccHand, 
    	subjectCert,
    	issuerCert,
    	NULL,				// VerifyScope
    	0);					// ScopeSize
	if(!brtn && (CSSM_GetError()->error == CSSM_CL_CERT_EXPIRED)) {
		*subjectExpired = CSSM_TRUE;
	}
	
abort:
	if(issuerPubKey != NULL) {
		CSSM_Free(issuerPubKey->KeyData.Data);
		CSSM_Free(issuerPubKey);
	}
	if(sigOid != NULL) {
		CSSM_Free(sigOid->Data);
		CSSM_Free(sigOid);
	}
	if(ccHand != 0) {
		CSSM_DeleteContext(ccHand);
	} 
	if(algId != NULL) {
		CSSM_Free(algId);
	}
	return brtn;
}
#endif	/* 0 - not needed */

#if		ST_KEYCHAIN_ENABLE 
/* no cert parsing in this version */

/*
 * Given a DER-encoded cert, obtain its DER-encoded subject name.
 */
CSSM_DATA_PTR sslGetCertSubjectName( 
	SSLContext			*ctx,
    const CSSM_DATA_PTR cert)
{
	uint32 			NumberOfFields = 0;
	CSSM_HANDLE 	ResultsHandle = 0;
	CSSM_DATA_PTR 	pEncodedName = NULL;
	CSSM_RETURN		crtn;
	
	/* ensure connection to CL */
	if(attachToCl(ctx)) {
		return NULL;
	}
	crtn = CSSM_CL_CertGetFirstFieldValue(
		ctx->clHand,
		cert,
	    &CSSMOID_X509V1SubjectName,
	    &ResultsHandle,
	    &NumberOfFields, 
		&pEncodedName);
	if(crtn) {
		stPrintCdsaError("CertGetFirstFieldValue", crtn);
	}
  	CSSM_CL_CertAbortQuery(ctx->clHand, ResultsHandle);
	return pEncodedName;
}
#endif	ST_KEYCHAIN_ENABLE 

#if		(SSL_DEBUG && ST_KEYCHAIN_ENABLE && ST_MANAGES_TRUSTED_ROOTS)
void verifyTrustedRoots(SSLContext *ctx,
	CSSM_DATA_PTR	certs,
	unsigned		numCerts)
{	
	int i;
	CSSM_DATA_PTR cert;
	CSSM_BOOL	expired;
	
	for(i=0; i<numCerts; i++) {
		cert = &certs[i];
		if(!sslVerifyCert(ctx,
				cert,
				cert,
				ctx->cspHand,
				&expired)) {
			sslPanic("Bad trusted cert!\n");
		}
	}
}
#endif



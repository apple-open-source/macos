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
	File:		appleCdsa.cpp

	Contains:	interface between SSL and CDSA

	Written by:	Doug Mitchell

	Copyright: (c) 1999 by Apple Computer, Inc., all rights reserved.

*/

#include "ssl.h"
#include "sslContext.h"
#include "sslMemory.h"
#include "appleCdsa.h"
#include "sslUtils.h"
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
#include <Security/cssmerrno.h>

/* X.509 includes, from cssmapi */
#include <Security/x509defs.h>         /* x.509 function and type defs */
#include <Security/oidsalg.h>
#include <Security/oidscert.h>

#pragma mark *** Utilities ***

/*
 * Set up a Raw symmetric key with specified algorithm and key bits.
 */
OSStatus sslSetUpSymmKey(
	CSSM_KEY_PTR	symKey,
	CSSM_ALGORITHMS	alg,
	CSSM_KEYUSE		keyUse, 		// CSSM_KEYUSE_ENCRYPT, etc.
	CSSM_BOOL		copyKey,		// true: copy keyData   false: set by reference
	uint8 			*keyData,
	uint32			keyDataLen)		// in bytes
{
	OSStatus serr;
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
	return noErr;
}

/*
 * Free a CSSM_KEY - its CSP resources, KCItemRef, and the key itself.
 */
OSStatus sslFreeKey(
	CSSM_CSP_HANDLE		cspHand,
	CSSM_KEY_PTR		*key,		/* so we can null it out */
	#if		ST_KC_KEYS_NEED_REF
	SecKeychainRef	*kcItem)
	#else	
	void			*kcItem) 
	#endif
{
	assert(key != NULL);
	
	if(*key != NULL) {
		if(cspHand != 0) {
			CSSM_FreeKey(cspHand, NULL, *key, CSSM_FALSE);
		}
		stAppFree(*key, NULL);		// key mallocd by CL using our callback
		*key = NULL;
	}
	#if		ST_KC_KEYS_NEED_REF
	if((kcItem != NULL) && (*kcItem != NULL)) {
		KCReleaseItem(kcItem);		/* does this NULL the referent? */
		*kcItem = NULL;
	}
	#endif
	return noErr;
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
OSStatus attachToCsp(SSLContext *ctx)
{
	assert(ctx != NULL);
	if(ctx->cspHand != 0) {
		return noErr;
	}	
	else {
		return errSSLModuleAttach;
	}
}

/* 
 * Connect to TP, CL; reusable.
 */
OSStatus attachToCl(SSLContext *ctx)
{
	assert(ctx != NULL);
	if(ctx->clHand != 0) {
		return noErr;
	}
	else {
		return errSSLModuleAttach;
	}
}

OSStatus attachToTp(SSLContext *ctx)
{
	assert(ctx != NULL);
	if(ctx->tpHand != 0) {
		return noErr;
	}
	else {
		return errSSLModuleAttach;
	}
}

/*
 * Convenience function - attach to CSP, CL, TP. Reusable. 
 */
OSStatus attachToAll(SSLContext *ctx)
{
	CSSM_RETURN crtn;
	
	assert(ctx != NULL);
	crtn = attachToModules(&ctx->cspHand, &ctx->clHand, &ctx->tpHand);
	if(crtn) {
	   return errSSLModuleAttach;
	}
	else {
		return noErr;
	}
}

OSStatus detachFromAll(SSLContext *ctx)
{
	#if	0
	/* No more, attachments are kept on a global basis */
	assert(ctx != NULL);
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
	return noErr;
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
OSStatus stSetUpCssmData(
	CSSM_DATA_PTR 	data,
	uint32 			length)
{
	assert(data != NULL);
	if(data->Length == 0) {
		data->Data = (uint8 *)stAppMalloc(length, NULL);
		if(data->Data == NULL) {
			return memFullErr;
		}
	}
	else if(data->Length < length) {
		sslErrorLog("stSetUpCssmData: length too small\n");
		return memFullErr;
	}
	data->Length = length;
	return noErr;
}

#pragma mark -
#pragma mark *** Public CSP Functions ***

/*
 * Raw RSA sign/verify.
 *
 * Initial X port: CSP doesns't support this, so we'll do sign/verify via
 * raw RSA encrypt/decrypt here. 
 */
#define SIGN_VFY_VIA_ENCR_DECR	0

#if		SIGN_VFY_VIA_ENCR_DECR

OSStatus sslRsaRawSign(
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
	OSStatus serr;
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

OSStatus sslRsaRawVerify(
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
	OSStatus serr;
	UInt8  *digest;
	
	/* Force CSSM_KEYUSE_ANY in case CL provided keyuse bits more specific 
	 * than we really want */
	CSSM_KEYUSE savedKeyUse = pubKey->KeyHeader.KeyUsage;
	pubKey->KeyHeader.KeyUsage = CSSM_KEYUSE_ANY;
	
	/* malloc space for decrypting the signature */
	digest = sslMalloc(plainTextLen);
	if(digest == NULL) {
		return memFullErr;
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
		sslErrorLog("sslRsaRawVerify: sig miscompare\n");
		serr = errSSLCrypto;
	}
	else {
		serr = noErr;
	}
errOut:
	sslFree(digest);
	return serr;
}

#else	/* OS9 and future post-cheetah version */

OSStatus sslRsaRawSign(
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
	OSStatus					serr;
	CSSM_DATA				sigData;
	CSSM_DATA				ptextData;
	
	assert(ctx != NULL);
	if((privKey == NULL) 	|| 
	   (cspHand == 0) 		|| 
	   (plainText == NULL)	|| 
	   (sig == NULL)		||
	   (actualBytes == NULL)) {
		sslErrorLog("sslRsaRawSign: bad arguments\n");
		return errSSLInternal;
	}
	*actualBytes = 0;
	
	crtn = CSSM_CSP_CreateSignatureContext(cspHand,
		CSSM_ALGID_RSA,
		NULL,				// passPhrase
		privKey,
		&sigHand);
	if(crtn) {
		stPrintCdsaError("CSSM_CSP_CreateSignatureContext (1)", crtn);
		return errSSLCrypto;
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
		serr = errSSLCrypto;
	}
	else {
		*actualBytes = sigData.Length;
		serr = noErr;
	}
	if(sigHand != 0) {
		CSSM_DeleteContext(sigHand);
	}
	return serr;
}

OSStatus sslRsaRawVerify(
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
	OSStatus					serr;
	CSSM_DATA				sigData;
	CSSM_DATA				ptextData;
	
	assert(ctx != NULL);
	if((pubKey == NULL) 	|| 
	   (cspHand == 0) 		|| 
	   (plainText == NULL)	|| 
	   (sig == NULL)) {
		sslErrorLog("sslRsaRawVerify: bad arguments\n");
		return errSSLInternal;
	}
	
	crtn = CSSM_CSP_CreateSignatureContext(cspHand,
		CSSM_ALGID_RSA,
		NULL,				// passPhrase
		pubKey,
		&sigHand);
	if(sigHand == 0) {
		stPrintCdsaError("CSSM_CSP_CreateSignatureContext (2)", crtn);
		return errSSLCrypto;
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
		serr = errSSLCrypto;
	}
	else {
		serr = noErr;
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
OSStatus sslRsaEncrypt(
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
	OSStatus			serr = errSSLInternal;
	CSSM_RETURN		crtn;
	uint32			bytesMoved = 0;
	CSSM_ACCESS_CREDENTIALS	creds;
	
	assert(ctx != NULL);
	assert(actualBytes != NULL);
	*actualBytes = 0;
	
	if((pubKey == NULL) || (cspHand == 0)) {
		sslErrorLog("sslRsaEncrypt: bad pubKey/cspHand\n");
		return errSSLInternal;
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
		return errSSLCrypto;
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
			return errSSLCrypto;
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
			sslErrorLog("sslRsaEncrypt overflow; cipherTextLen %ld bytesMoved %ld\n",
				cipherTextLen, bytesMoved);
			serr = errSSLCrypto;
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
			serr = noErr;
		}
	}
	else {
		stPrintCdsaError("CSSM_EncryptData", crtn);
		serr = errSSLCrypto;
	}
	if(cryptHand != 0) {
		CSSM_DeleteContext(cryptHand);
	}

	/* free data mallocd by CSP */
	stFreeCssmData(&ctextData, CSSM_FALSE);
	stFreeCssmData(&remData, CSSM_FALSE);
	return serr;
}

OSStatus sslRsaDecrypt(
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
	OSStatus			serr = errSSLInternal;
	CSSM_RETURN		crtn;
	uint32			bytesMoved = 0;
	CSSM_ACCESS_CREDENTIALS	creds;
		
	assert(ctx != NULL);
	assert(actualBytes != NULL);
	*actualBytes = 0;
	
	if((privKey == NULL) || (cspHand == 0)) {
		sslErrorLog("sslRsaDecrypt: bad privKey/cspHand\n");
		return errSSLInternal;
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
		return errSSLCrypto;
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
			return errSSLCrypto;
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
			sslErrorLog("sslRsaDecrypt overflow; plainTextLen %ld bytesMoved %ld\n",
				plainTextLen, bytesMoved);
			serr = errSSLCrypto;
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
			serr = noErr;
		}
	}
	else {
		stPrintCdsaError("CSSM_DecryptData", crtn);
		serr = errSSLCrypto;
	}
	if(cryptHand != 0) {
		CSSM_DeleteContext(cryptHand);
	}
	
	/* free data mallocd by CSP */
	stFreeCssmData(&ptextData, CSSM_FALSE);
	stFreeCssmData(&remData, CSSM_FALSE);
	return serr;
}

/*
 * Obtain size of key in bytes.
 */
UInt32 sslKeyLengthInBytes(const CSSM_KEY *key)
{
	assert(key != NULL);
	return (((key->KeyHeader.LogicalKeySizeInBits) + 7) / 8);
}

/*
 * Get raw key bits from an RSA public key.
 */
OSStatus sslGetPubKeyBits(
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
	OSStatus				srtn;
	CSSM_ACCESS_CREDENTIALS	creds;
	
	assert(ctx != NULL);
	assert(modulus != NULL);
	assert(exponent != NULL);
	assert(pubKey != NULL);
	
	hdr = &pubKey->KeyHeader;
	if(hdr->KeyClass != CSSM_KEYCLASS_PUBLIC_KEY) {
		sslErrorLog("sslGetPubKeyBits: bad keyClass (%ld)\n", hdr->KeyClass);
		return errSSLInternal;
	}
	if(hdr->AlgorithmId != CSSM_ALGID_RSA) {
		sslErrorLog("sslGetPubKeyBits: bad AlgorithmId (%ld)\n", hdr->AlgorithmId);
		return errSSLInternal;
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
				return errSSLCrypto;
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
				return errSSLCrypto;
			}
			hdr = &wrappedKey.KeyHeader;
			if(hdr->BlobType != CSSM_KEYBLOB_RAW) {
				sslErrorLog("sslGetPubKeyBits: bad BlobType (%ld) after WrapKey\n", 
					hdr->BlobType);
				return errSSLCrypto;
			}
			didWrap = CSSM_TRUE;
			CSSM_TO_SSLBUF(&wrappedKey.KeyData, &pubKeyBlob);
			break;

		default:
			sslErrorLog("sslGetPubKeyBits: bad BlobType (%ld)\n", 
				hdr->BlobType);
			return errSSLInternal;
	
	}	/* switch BlobType */

	assert(hdr->BlobType == CSSM_KEYBLOB_RAW); 
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
OSStatus sslGetPubKeyFromBits(
	SSLContext			*ctx,
	const SSLBuffer		*modulus,	
	const SSLBuffer		*exponent,	
	CSSM_KEY_PTR		*pubKey,		// mallocd and RETURNED
	CSSM_CSP_HANDLE		*cspHand)		// RETURNED
{
	CSSM_KEY_PTR		key = NULL;
	OSStatus				serr;
	SSLBuffer			blob;
	CSSM_KEYHEADER_PTR	hdr;
	CSSM_KEY_SIZE		keySize;
	CSSM_RETURN			crtn;
	
	assert((ctx != NULL) && (modulus != NULL) && (exponent != NULL));
	assert((pubKey != NULL) && (cspHand != NULL));
	
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
	key = (CSSM_KEY_PTR)sslMalloc(sizeof(CSSM_KEY));
	if(key == NULL) {
		return memFullErr;
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
		serr = errSSLCrypto;
    	goto abort;
	}
	
	/* success */
    hdr->LogicalKeySizeInBits = keySize.EffectiveKeySizeInBits;
    *pubKey = key;
    *cspHand = ctx->cspHand;
	return noErr;
	
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
OSStatus sslPubKeyFromCert(
	SSLContext 			*ctx,
	const SSLBuffer		&derCert,
	CSSM_KEY_PTR		*pubKey,		// RETURNED
	CSSM_CSP_HANDLE		*cspHand)		// RETURNED
{
	OSStatus 			serr;
	CSSM_DATA		certData;
	CSSM_RETURN		crtn;
	
	assert(ctx != NULL);
	assert(pubKey != NULL);
	assert(cspHand != NULL);
	
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
	SSLBUF_TO_CSSM(&derCert, &certData);
	crtn = CSSM_CL_CertGetKeyInfo(ctx->clHand, &certData, pubKey);
	if(crtn) {
		return errSSLBadCert;
	}
	else {
		*cspHand = ctx->cspHand; 
		return noErr;
	}
}

#if		ST_MANAGES_TRUSTED_ROOTS

/*
 * Given a CSSM_CERTGROUP which fails due to CSSM_TP_INVALID_ANCHOR
 * (chain verifies to an unknown root):
 *
 * -- find the root cert
 * -- add it to newRootCertKc if present (else error)
 * -- add it to trustedCerts
 * -- re-verify certgroup, demand full success
 */
static OSStatus sslHandleNewRoot(
	SSLContext				*ctx,
	CSSM_CERTGROUP_PTR		certGroup)
{
	int 			i;
	CSSM_DATA_PTR	rootCert;
	CSSM_BOOL		expired;
	OSStatus			serr;
	CSSM_BOOL		brtn;
	
	assert(ctx != NULL);
	assert(certGroup != NULL);
	
	if(ctx->newRootCertKc == NULL) {
		/* no place to add this; done */
		return errSSLUnknownRootCert;
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
		sslErrorLog("sslHandleNewRoot: no root cert!\n");
		return errSSLInternal;
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
		sslErrorLog("sslHandleNewRoot: adding new root did not help!\n");
		return errSSLUnknownRootCert;
	}
	return noErr;
}

#endif	/* ST_MANAGES_TRUSTED_ROOTS */

/*
 * Verify a chain of DER-encoded certs.
 * First cert in a chain is root; this must also be present
 * in ctx->trustedCerts. 
 */
OSStatus sslVerifyCertChain(
	SSLContext				*ctx,
	const SSLCertificate	&certChain,
	bool					verifyHostName /* = true */) 
{
	UInt32 						numCerts;
	CSSM_CERTGROUP				certGroup;
	int 						i;
	OSStatus						serr;
	SSLCertificate				*c = (SSLCertificate *)&certChain;
	CSSM_RETURN					crtn;
	CSSM_TP_VERIFY_CONTEXT		vfyCtx;
	CSSM_TP_CALLERAUTH_CONTEXT	authCtx;
	CSSM_FIELD					policyId;
	CSSM_DL_DB_LIST				dbList;
	CSSM_APPLE_TP_SSL_OPTIONS	sslOpts;
	CSSM_APPLE_TP_ACTION_DATA	actionData;
	
	if(!ctx->enableCertVerify) {
		/* trivial case, this is caller's responsibility */
		return noErr;
	}
	numCerts = SSLGetCertificateChainLength(&certChain);
	if(numCerts == 0) {
		/* nope */
		return errSSLBadCert;
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
		return memFullErr;
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
	if(verifyHostName) {
		sslOpts.ServerNameLen = ctx->peerDomainNameLen;
		sslOpts.ServerName = ctx->peerDomainName;
	}
	else {
		sslOpts.ServerNameLen = 0;
		sslOpts.ServerName = NULL;
	}
	
	/* TP-wide ActionData */
	actionData.Version = CSSM_APPLE_TP_ACTION_VERSION;
	if(ctx->numTrustedCerts != 0) {
		/* use our anchors */
		actionData.ActionFlags = 0;
	}
	else {
		/* secret root-cert-enable */
		actionData.ActionFlags = 0x80000000;
	}
	if(ctx->allowExpiredCerts) {
		actionData.ActionFlags |= CSSM_TP_ACTION_ALLOW_EXPIRED;
	}
	if(ctx->allowExpiredRoots) {
		actionData.ActionFlags |= CSSM_TP_ACTION_ALLOW_EXPIRED_ROOT;
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

	serr = noErr;
	if(crtn) {	
		/* get some detailed error info */
		switch(crtn) {
			case CSSMERR_TP_INVALID_ANCHOR_CERT: 
				/* root found but we don't trust it */
				if(ctx->allowAnyRoot) {
					sslErrorLog("***Warning: accepting unknown root cert\n");
					break;
				}
				#if		ST_MANAGES_TRUSTED_ROOTS
				if(ctx->newRootCertKc != NULL) {
					/* see if user wants to handle new root */
					serr = sslHandleNewRoot(ctx, &certGroup);
				}
				else {
					serr = errSSLUnknownRootCert;
				}
				#else
				serr = errSSLUnknownRootCert;
				#endif	/* ST_MANAGES_TRUSTED_ROOTS */
				break;
			case CSSMERR_TP_NOT_TRUSTED:
				/* no root, not even in implicit SSL roots */
				if(ctx->allowAnyRoot) {
					sslErrorLog("***Warning: accepting unverified cert chain\n");
					break;
				}
				serr = errSSLNoRootCert;
				break;
			case CSSMERR_TP_CERT_EXPIRED:
				assert(!ctx->allowExpiredCerts);
				serr = errSSLCertExpired;
				break;
			case CSSMERR_TP_CERT_NOT_VALID_YET:
				serr = errSSLCertNotYetValid;
				break;
			default:
				stPrintCdsaError(
					"sslVerifyCertChain: CSSM_TP_CertGroupVerify returned", 					crtn);
				serr = errSSLXCertChainInvalid;
				break;
		}
	} 	/* brtn FALSE */

	/* 
	 * don't free individual certs - caller still owns them
	 * don't free struct - on stack 
	 */
	sslFree(certGroup.GroupList.CertList);
	return serr;
}

#if		ST_MANAGES_TRUSTED_ROOTS

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
#endif	/* ST_MANAGES_TRUSTED_ROOTS */

#if		(SSL_DEBUG && ST_MANAGES_TRUSTED_ROOTS)
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
			sslErrorLog("Bad trusted cert!\n");
		}
	}
}
#endif

#ifndef	NDEBUG
void stPrintCdsaError(const char *op, CSSM_RETURN crtn)
{
	cssmPerror(op, crtn);
}

char *stCssmErrToStr(CSSM_RETURN err)
{
	string errStr = cssmErrorString(err);
	return const_cast<char *>(errStr.c_str());
}
#endif



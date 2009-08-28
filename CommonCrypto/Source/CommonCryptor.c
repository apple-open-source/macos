/* 
 * Copyright (c) 2006 Apple Computer, Inc. All Rights Reserved.
 * 
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

/*
 * CommonCryptor.c - common crypto context.
 *
 * Created 3/27/06 by Doug Mitchell. 
 */

#include <CommonCrypto/CommonCryptor.h>
#include "CommonCryptorPriv.h"
#include "BlockCipher.h"
#include "StreamCipher.h"
#include <stdlib.h>
#include <strings.h>
#include <stddef.h>			/* for offsetof() */

/* 
 * ComonCryptor's portion of a CCCryptorRef. 
 */
typedef struct _CCCryptor {
	/* if true, we mallocd this and must free it in CCCryptorRelease() */
	bool						weMallocd;
	/* total size of context (ours plus SPI-specific) */
	size_t						contextSize;
	/* op info */
	CCOperation					op;
	CCAlgorithm					alg;
	/* SPI info */
	const CCCryptSpiCallouts	*callouts;
	/* start of SPI-specific context */
	char						spiCtx[1];
} CCCryptor;


static const CCCryptSpiCallouts *ccSpiCallouts(
	CCOperation op,	
	CCAlgorithm alg)
{
	switch(alg) {
		case kCCAlgorithmAES128:
		case kCCAlgorithmDES:
		case kCCAlgorithm3DES:
		case kCCAlgorithmCAST:
		case kCCAlgorithmRC2:
			return &ccBlockCipherCallouts;
		case kCCAlgorithmRC4:
			return &ccRC4Callouts;
		default:
			return NULL;
	}
}

CCCryptorStatus CCCryptorCreate(
	CCOperation op,             /* kCCEncrypt, etc. */
	CCAlgorithm alg,            /* kCCAlgorithmDES, etc. */
	CCOptions options,          /* kCCOptionPKCS7Padding, etc. */
	const void *key,            /* raw key material */
	size_t keyLength,	
	const void *iv,             /* optional initialization vector */
	CCCryptorRef *cryptorRef)  /* RETURNED */
{
	const CCCryptSpiCallouts *callouts;
	CCCryptorStatus crtn;
	CCCryptor *cryptor = NULL;
	size_t requiredLen;
	
	if(cryptorRef == NULL) {
		return kCCParamError;
	}
	callouts = ccSpiCallouts(op, alg);
	if(callouts == NULL) {
		return kCCParamError;
	}
	
	/* how much space do we and the SPI need? */
	crtn = callouts->contextSize(op, alg, &requiredLen);
	if(crtn) {
		return crtn;
	}
	requiredLen += offsetof(CCCryptor, spiCtx);
	
	/* alloc and init our portion */
	cryptor = (CCCryptor *)malloc(requiredLen);
	if(cryptor == NULL) {
		return kCCMemoryFailure;
	}
	cryptor->weMallocd = true;
	cryptor->contextSize = requiredLen;
	cryptor->op = op;
	cryptor->alg = alg;
	cryptor->callouts = callouts;

	/* SPI-specific init */
	crtn = callouts->init(cryptor->spiCtx, op, alg, options, 
		key, keyLength, iv);
	if(crtn) {
		free(cryptor);
		return crtn;
	}
	*cryptorRef = cryptor;
	return kCCSuccess;
}

CCCryptorStatus CCCryptorCreateFromData(
	CCOperation op,             /* kCCEncrypt, etc. */
	CCAlgorithm alg,            /* kCCAlgorithmDES, etc. */
	CCOptions options,          /* kCCOptionPKCS7Padding, etc. */
	const void *key,            /* raw key material */
	size_t keyLength,	
	const void *iv,             /* optional initialization vector */
	const void *data,			/* caller-supplied memory */
	size_t dataLength,			/* length of data in bytes */
	CCCryptorRef *cryptorRef,   /* RETURNED */
	size_t *dataUsed)			/* optional, RETURNED */
{
	const CCCryptSpiCallouts *callouts;
	CCCryptorStatus crtn;
	CCCryptor *cryptor = NULL;
	size_t requiredLen;
	
	if((data == NULL) || (cryptorRef == NULL)) {
		return kCCParamError;
	}
	callouts = ccSpiCallouts(op, alg);
	if(callouts == NULL) {
		return kCCParamError;
	}
	
	/* how much space do we and the SPI need? */
	crtn = callouts->contextSize(op, alg, &requiredLen);
	if(crtn) {
		return crtn;
	}
	requiredLen += offsetof(CCCryptor, spiCtx);
	if(dataLength < requiredLen) {
		if(dataUsed != NULL) {
			*dataUsed = requiredLen;
		}
		return kCCBufferTooSmall;
	}
	cryptor = (CCCryptor *)data;

	/* init our portion */
	cryptor->weMallocd = false;
	cryptor->contextSize = requiredLen;
	cryptor->op = op;
	cryptor->alg = alg;
	cryptor->callouts = callouts;

	/* SPI-specific init */
	crtn = callouts->init(cryptor->spiCtx, op, alg, options, 
		key, keyLength, iv);
	if(crtn) {
		return crtn;
	}
	*cryptorRef = cryptor;
	if(dataUsed != NULL) {
		*dataUsed = requiredLen;
	}
	return kCCSuccess;
}

CCCryptorStatus CCCryptorRelease(
	CCCryptorRef cryptor)
{
	bool weMallocd;
	size_t zeroSize;
	
	if(cryptor == NULL) {
		return kCCParamError;
	}
	if(cryptor->callouts->release) {
		cryptor->callouts->release(cryptor->spiCtx);
		zeroSize = offsetof(CCCryptor, spiCtx);
	}
	else {
		/* provider says: "just zero everything" */
		zeroSize = cryptor->contextSize;
	}
	weMallocd = cryptor->weMallocd;
	memset(cryptor, 0, zeroSize);
	if(weMallocd) {
		free(cryptor);
	}
	return kCCSuccess;
}

CCCryptorStatus CCCryptorUpdate(
	CCCryptorRef cryptor,
	const void *dataIn,
	size_t dataInLength,
	void *dataOut,				/* data RETURNED here */
	size_t dataOutAvailable,
	size_t *dataOutMoved)		/* number of bytes written */
{
	if(cryptor == NULL) {
		return kCCParamError;
	}
	return cryptor->callouts->update(cryptor->spiCtx, 
		dataIn, dataInLength, 
		dataOut, dataOutAvailable, dataOutMoved);
}

CCCryptorStatus CCCryptorFinal(
	CCCryptorRef cryptor,
	void *dataOut,			/* data RETURNED here */
	size_t dataOutAvailable,
	size_t *dataOutMoved)		/* number of bytes written */
{
	if(cryptor == NULL) {
		return kCCParamError;
	}
	return cryptor->callouts->final(cryptor->spiCtx, 
		dataOut, dataOutAvailable, dataOutMoved);
}

size_t CCCryptorGetOutputLength(
	CCCryptorRef cryptor,
	size_t inputLength,
	bool final)
{
	if(cryptor == NULL) {
		return 0;
	}
	return cryptor->callouts->outputSize(cryptor->spiCtx, 
		inputLength, final);
}

CCCryptorStatus CCCryptorReset(
	CCCryptorRef cryptor,
	const void *iv)
{
	if(cryptor == NULL) {
		return kCCParamError;
	}
	if(cryptor->callouts->reset == NULL) {
		return kCCUnimplemented;
	}
	return cryptor->callouts->reset(cryptor->spiCtx, iv);
}


/* 
 * One-shot is mostly service provider independent, except for the
 * dataOutLength check.
 */
CCCryptorStatus CCCrypt(
	CCOperation op,			/* kCCEncrypt, etc. */
	CCAlgorithm alg,		/* kCCAlgorithmAES128, etc. */
	CCOptions options,		/* kCCOptionPKCS7Padding, etc. */
	const void *key,
	size_t keyLength,
	const void *iv,			/* optional initialization vector */
	const void *dataIn,		/* optional per op and alg */
	size_t dataInLength,
	void *dataOut,			/* data RETURNED here */
	size_t dataOutAvailable,
	size_t *dataOutMoved)	
{
	const CCCryptSpiCallouts *callouts;
	size_t outputSize;
	CCCryptorRef cryptor = NULL;
	CCCryptorStatus crtn;
	size_t totalMoved = 0;
	size_t remaining;
	size_t thisMove;
	char *outp;

	if(dataOutMoved == NULL) {
		return kCCParamError;
	}
	/* remaining fields technically legal, here */
	
	/* the only SPI-specific task is validating dataOutLength */
	callouts = ccSpiCallouts(op, alg);
	if(callouts == NULL) {
		return kCCParamError;
	}
	crtn = callouts->oneShotSize(op, alg, options, dataInLength, &outputSize);
	if(crtn) {
		return crtn;
	}
	if(outputSize > dataOutAvailable) {
		*dataOutMoved = outputSize;
		return kCCBufferTooSmall;
	}
	
	/* looks like it should be legal */
	crtn = CCCryptorCreate(op, alg, options,
		key, keyLength, iv, &cryptor);
	if(crtn) {
		return crtn;
	}
	remaining = dataOutAvailable;
	outp = (char *)dataOut;
	if((dataIn != NULL) && (dataInLength != 0)) {
		crtn = CCCryptorUpdate(cryptor, dataIn, dataInLength,
			outp, remaining, &thisMove);
		if(crtn) {
			goto errOut;
		}
		outp       += thisMove;
		totalMoved += thisMove;
		remaining  -= thisMove;
	}
	crtn = CCCryptorFinal(cryptor, outp, remaining, &thisMove);
	if(crtn == kCCSuccess) {
		totalMoved += thisMove;
		*dataOutMoved = totalMoved;
	}
	
errOut:
	if(cryptor) {
		CCCryptorRelease(cryptor);
	}
	return crtn;
}

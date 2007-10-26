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

#include "CommonCryptorPriv.h"
#include "StreamCipher.h"
#include <CommonCrypto/rc4.h>

/* 
 * RC4 stream cipher.
 * SPI-specific context is just a RC4_KEY.
 */
static CCCryptorStatus CCRC4ContextSize(
	CCOperation op, 
	CCAlgorithm alg, 
	size_t *ctxSize)
{
	*ctxSize = sizeof(RC4_KEY);
	return kCCSuccess;
}

static CCCryptorStatus CCRC4Init(
	void *ctx,
	CCOperation op,			/* kCCEncrypt, kCCDecrypt */
	CCAlgorithm alg,		/* kCCAlgDES, etc. */
	CCOptions options,		/* kCCOptionPKCS7Padding, etc. */
	const void *key,		/* raw key material */
	size_t keyLength,	
	const void *iv)			/* optional initialization vector */
{
	RC4_KEY *rc4Key = (RC4_KEY *)ctx;
	
	if(key == NULL) {
		return kCCParamError;
	}
	RC4_set_key(rc4Key, keyLength, key);
	return kCCSuccess;
}

/* no release - just zero our memory */

static CCCryptorStatus CCRC4Update(
   void *ctx,
   const void *dataIn,
   size_t dataInLen,
   void *dataOut,           /* data RETURNED here */
	size_t dataOutAvailable,
	size_t *dataOutMoved)		/* number of bytes written */
{
	RC4_KEY *rc4Key = (RC4_KEY *)ctx;
	
	if((dataIn == NULL) || (dataOut == NULL) || (dataOutMoved == NULL)) {
		return kCCParamError;
	}
	if(dataOutAvailable < dataInLen) {
		return kCCBufferTooSmall;
	}
	RC4(rc4Key, dataInLen, dataIn, dataOut);
	*dataOutMoved = dataInLen;
	return kCCSuccess;
}

static CCCryptorStatus CCRC4Final(
   void *ctx,
   void *dataOut,           /* data RETURNED here */
	size_t dataOutAvailable,
	size_t *dataOutMoved)	/* number of bytes written */
{
	if(dataOutMoved) {
		*dataOutMoved = 0;
	}
	return kCCSuccess;
}

/* no reset - not supported */

/* normal OutputSize */
static size_t CCRC4OutputSize(
	void *ctx, size_t inputLength, bool final)
{
	return inputLength;
}

/* one-shot size */
static CCCryptorStatus CCRC4OneShotSize(
	CCOperation op,
	CCAlgorithm alg,
	CCOptions options,
	size_t inputLen,
	size_t *outputLen)
{
	*outputLen = inputLen;
	return kCCSuccess;
}

/* 
 * Callouts used by CommonCryptor.
 */
const CCCryptSpiCallouts ccRC4Callouts = 
{
	CCRC4ContextSize,
	CCRC4Init,
	NULL,			/* release */
	CCRC4Update,
	CCRC4Final,
	NULL,			/* reset */
	CCRC4OutputSize,
	CCRC4OneShotSize,
};

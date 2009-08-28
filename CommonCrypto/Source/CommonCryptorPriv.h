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
 * CommonCryptorPriv.h - interface between CommonCryptor and operation- and
 *						 algorithm-specific service providers. 
 */

#ifndef	_CC_COMMON_CRYPTOR_PRIV_
#define	_CC_COMMON_CRYPTOR_PRIV_

#include <CommonCrypto/CommonCryptoPriv.h>
#include <CommonCrypto/CommonCryptor.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Service provider callouts, called from the public functions declared
 * in CommonCryptor.h. Semantics are the same as the corresponding functions
 * in that header, except for the void * passed to *ccCryptSpiInitFcn (which 
 * otherwise corresponds to CCCryptorCreate()), the allocation of which is 
 * handled at the CCCryptor layer. 
 */

/*
 * Return the size in bytes of SPI-specific context required for
 * specified op and algorithm.
 */
typedef CCCryptorStatus (*ccCryptorSpiContextSize)(CCOperation op, CCAlgorithm alg, size_t *ctxSize);

/* remainder mirror the public functions in CCCryptor.h */
typedef CCCryptorStatus (*ccCryptorSpiInitFcn)(
	void *ctx,
	CCOperation op,
	CCAlgorithm alg,
	CCOptions options,
	const void *key,
	size_t keyLength,
	const void *iv);
/* release is optional - if not present, SPI-specific context is zeroed */
typedef CCCryptorStatus (*ccCryptorSpiRelease)(void *ctx);
typedef CCCryptorStatus (*ccCryptorSpiUpdate)(void *ctx,
	const void *dataIn,
	size_t dataInLength,
	void *dataOut,			/* data RETURNED here */
	size_t dataOutAvailable,
	size_t *dataOutMoved);
typedef CCCryptorStatus (*ccCryptorSpiFinal)(void *ctx,
	void *dataOut,			/* data RETURNED here */
	size_t dataOutAvailable,
	size_t *dataOutMoved);
/* reset is optional - if not present, kCCUnimplemented */
typedef CCCryptorStatus (*ccCryptorSpiReset)(void *ctx,
	const void *iv);
typedef size_t (*ccCryptorSpiOutputSize)(void *ctx,
	size_t inputLength,
	bool final);	
typedef CCCryptorStatus (*ccCryptorSpiOneShotSize)(
	CCOperation op,
	CCAlgorithm alg,
	CCOptions options,
	size_t inputLen,
	size_t *outputLen);
	
/* 
 * Callouts for one service provider. 
 */
typedef struct {
	ccCryptorSpiContextSize	contextSize;
	ccCryptorSpiInitFcn		init;
	ccCryptorSpiRelease		release;
	ccCryptorSpiUpdate		update;
	ccCryptorSpiFinal		final;
	ccCryptorSpiReset		reset;		/* optional: NULL --> kCCUnimplemented */
	ccCryptorSpiOutputSize	outputSize;
	ccCryptorSpiOneShotSize	oneShotSize;
} CCCryptSpiCallouts;


#ifdef __cplusplus
}
#endif

#endif	/* _CC_COMMON_CRYPTOR_PRIV_ */

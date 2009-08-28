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
 * BlockCipher.c - CommonCryptor service provider for block ciphers.
 *
 * Created 3/20/2006 by Doug Mitchell.
 */

#include <CommonCrypto/CommonCryptor.h>
#include "CommonCryptorPriv.h"
#include <stdlib.h>
#include <strings.h>
#include <CommonCrypto/opensslDES.h>	/* SPI */
#include <CommonCrypto/ccCast.h>		/* SPI */
#include <CommonCrypto/ccRC2.h>		/* SPI */
#include <CommonCrypto/aesopt.h>

#include <stddef.h>					/* for offsetof() */
#include <stdbool.h>

/* select an AES implementation */
#define AES_GLADMAN_STD		0
#define AES_GLADMAN_NEW		1

#if		AES_GLADMAN_STD
#include <AES/ccRijndaelGladman.h>
#elif	AES_GLADMAN_NEW
#include <CommonCrypto/aesopt.h>
#endif

#define CC_DEBUG	0
#if		CC_DEBUG
#include <stdio.h>

#define dprintf(args...)	printf(args)
#else
#define dprintf(args...)
#endif	/* CC_DEBUG */

/*
 * To avoid dynamic allocation of buffers, we just hard-code the 
 * knowledge of the max block size here. This must be kept up to date
 * with block sizes of algortihms that are added to this module.
 */
#define CC_MAX_BLOCK_SIZE	kCCBlockSizeAES128

static void ccBlockCipherProcessOneBlock(
	void *ctx,
	const void *blockIn,
	void *blockOut);

/* 
 * The functions a symmetric encryption algorithm must implement
 * to work with this module. Single-block encrypt and decrypt are of the form
 * bcProcessBlockFcn. Multi-block encrypt and decrypt are of the form
 * bcProcessBlocksFcn. Exactly one pair of (bcProcessBlockFcn, 
 * bcProcessBlocksFcn) functions is avaialble for a given cipher. 
 * The keyLength arg to bcSetKeyFcn is in bytes.
 */
typedef int (*bcSetKeyFcn)(
	void *ctx, 
	const void *rawKey, 
	size_t keyLength, 
	int forEncrypt);
/* 
 * This is used for algorithms that perform their own CBC. If this is not called,
 * ECB is assumed.
 */
typedef void (*bcSetIvFcn)(
	void *ctx,
	int forEncrypt, 
	const void *iv);
	
/* process one block */
typedef void (*bcProcessBlockFcn)(
	void *ctx, 
	const void *blockIn, 
	void *blockOut);
	
/* process multiple blocks */
typedef void (*bcProcessBlocksFcn)(
	void *ctx, 
	const void *blocksIn, 
	size_t numBlocks, 
	void *blocksOut);

/*
 * Everything we need to know about an algorithm.
 */
typedef struct {
	CCAlgorithm			alg;
	size_t				blockSize;
	size_t				minKeySize;
	size_t				maxKeySize;
	bool				algDoesCbc;	/* algorithm does CBC itself */
	bcSetKeyFcn			setKey;
	bcSetIvFcn			setIv;
	bcProcessBlockFcn	encrypt;
	bcProcessBlockFcn	decrypt;
	bcProcessBlocksFcn	encryptBlocks;
	bcProcessBlocksFcn	decryptBlocks;
	size_t				ctxSize;	/* size of alg-specific context */
} CCAlgInfo;

/* 
 * The list of algorithms we know about.
 * The casts for the function pointers are necessary to allow
 * implementations to keep meaningful context pointers (and,
 * possibly, char * in/out pointers) in their interfaces. 
 */
static const CCAlgInfo bcAlgInfos[] = 
{
	/* AES with 128-bit blocks */
	{ kCCAlgorithmAES128, kCCBlockSizeAES128, kCCKeySizeAES128, kCCKeySizeAES256,
	  true,
	  (bcSetKeyFcn)aes_cc_set_key,
	  (bcSetIvFcn)aes_cc_set_iv,
	  ccBlockCipherProcessOneBlock, 
	  ccBlockCipherProcessOneBlock,
	  (bcProcessBlocksFcn)aes_cc_encrypt,
	  (bcProcessBlocksFcn)aes_cc_decrypt,
	  sizeof(aes_cc_ctx)
	},
	/* DES */
	{ kCCAlgorithmDES, kCCBlockSizeDES, kCCKeySizeDES, kCCKeySizeDES, 
	  false, 
	  (bcSetKeyFcn)osDesSetkey,
	  NULL,
	  (bcProcessBlockFcn)osDesEncrypt,
	  (bcProcessBlockFcn)osDesDecrypt,
	  NULL, NULL,
	  sizeof(DES_key_schedule)
	},
	/* Triple DES EDE */
	{ kCCAlgorithm3DES, kCCBlockSize3DES, kCCKeySize3DES, kCCKeySize3DES, 
	  false,
	  (bcSetKeyFcn)osDes3Setkey,
	  NULL,
	  (bcProcessBlockFcn)osDes3Encrypt,
	  (bcProcessBlockFcn)osDes3Decrypt,
	  NULL, NULL,
	  sizeof(DES3_Schedule)
	},
	/* CAST */
	{ kCCAlgorithmCAST, kCCBlockSizeCAST, kCCKeySizeMinCAST, kCCKeySizeMaxCAST,
	  false,
	  (bcSetKeyFcn)cast_cc_set_key,
	  NULL,
	  (bcProcessBlockFcn)cast_cc_encrypt,
	  (bcProcessBlockFcn)cast_cc_decrypt,
	  NULL, NULL,
	  sizeof(CAST_KEY)
	},
	/* RC2 */
	{ kCCAlgorithmRC2, kCCBlockSizeRC2, kCCKeySizeMinRC2, kCCKeySizeMaxRC2,
	  false,
	  (bcSetKeyFcn)rc2_cc_set_key,
	  NULL,
	  (bcProcessBlockFcn)rc2_cc_encrypt,
	  (bcProcessBlockFcn)rc2_cc_decrypt,
	  NULL, NULL,
	  sizeof(RC2_Schedule)
	}
};
#define NUM_CC_ALG_INFOS	(sizeof(bcAlgInfos) / sizeof(bcAlgInfos[0]))

/* 
 * Runtime context. This follows CommonCryptor's CCCryptor struct, but
 * we don't need to know that here. 
 */
struct _CCBlockCipherContext {
	const CCAlgInfo		*algInfo;
	bcProcessBlockFcn	update;	
	bcProcessBlocksFcn	updateBlocks;	
	uint8_t				inBuf[CC_MAX_BLOCK_SIZE];		/* for buffering input */
	size_t				inBufSize;						/* valid bytes in inBuf */
	uint8_t				chainBuf[CC_MAX_BLOCK_SIZE];	/* for CBC */
	bool				encrypting;
	bool				pkcsPad;
	bool				cbc;					/* what caller asked for */
	bool				doCbc;					/* cbc & !algInfo->algDoesCbc */
	char				algCtx[1];				/* start of alg-specific context */
};
typedef struct _CCBlockCipherContext *CCBlockCipherContext;

#define MAC_BLOCK_SIZE	32

/* set IV per ctx->encrypting */
static void ccBlockCipherSetIV(
	CCBlockCipherContext ctx,
	const void *iv)
{
	uint8_t *buf;
	uint8_t blockSize;
	
	if(ctx->algInfo->algDoesCbc) {
		uint8_t nullIv[MAC_BLOCK_SIZE];
		
		if(iv == NULL) {
			/* NULL IV semantics does not apply at the CCAlgInfo layer */
			memset(nullIv, 0, sizeof(nullIv));
			iv = nullIv;
		}
		ctx->algInfo->setIv(ctx->algCtx, ctx->encrypting, iv);
		return;
	}
	
	/* IV ==> inBuf for encrypt, chainBuf for decrypt */
	blockSize = ctx->algInfo->blockSize;
	if(ctx->encrypting) {
		buf = ctx->inBuf;
	}
	else {
		buf = ctx->chainBuf;
	}
	
	/* actual IV is optional */
	if(iv == NULL) {
		memset(buf, 0, blockSize);
	}
	else {
		memmove(buf, iv, blockSize);
	}
}

/* locate CCAlgInfo for a given algorithm */
static const CCAlgInfo *ccBlockCipherFindAlgInfo(
	CCAlgorithm alg)
{
	const CCAlgInfo *algInfo = bcAlgInfos;
	unsigned dex;

	for(dex=0; dex<NUM_CC_ALG_INFOS; dex++) {
		if(algInfo->alg == alg) {
			return algInfo;
		}
		algInfo++;
	}
	return NULL;
}

/*
 * One-block encrypt/decrypt for algorithms that don't provide it 
 * themselves.
 */
static void ccBlockCipherProcessOneBlock(
	void *ctx,
	const void *blockIn,
	void *blockOut)
{
	/* The ctx we've been given is the algCtx; we need a CCBlockCipherContext...*/
	char *ourCtx = (char *)ctx - offsetof(struct _CCBlockCipherContext, algCtx);
	CCBlockCipherContext cryptCtx = (CCBlockCipherContext)ourCtx;
	cryptCtx->updateBlocks(ctx, blockIn, 1, blockOut);
}

/* service provider interface */

/* 
 * Determine SPI-specific context size, including algorithm-specific
 * context.
 */
static CCCryptorStatus CCBlockCipherContextSize(
	CCOperation op, 
	CCAlgorithm alg, 
	size_t *ctxSize)
{
	const CCAlgInfo *algInfo = ccBlockCipherFindAlgInfo(alg);
	
	if(algInfo == NULL) {
		return kCCParamError;
	}
	*ctxSize = offsetof(struct _CCBlockCipherContext, algCtx) + algInfo->ctxSize;
	return kCCSuccess;
}

static CCCryptorStatus CCBlockCipherInit(
	void *ctx,
	CCOperation op,			/* kCCEncrypt, kCCDecrypt */
	CCAlgorithm alg,		/* kCCAlgorithmDES, etc. */
	CCOptions options,		/* kCCOptionPKCS7Padding, etc. */
	const void *key,		/* raw key material */
	size_t keyLength,	
	const void *iv)			/* optional initialization vector */
{
	const CCAlgInfo *algInfo = ccBlockCipherFindAlgInfo(alg);
	CCBlockCipherContext cryptCtx = (CCBlockCipherContext)ctx;
	
	if((algInfo == NULL) || (key == NULL)) {
		return kCCParamError;
	}
	if((keyLength < algInfo->minKeySize) || 
	   (keyLength > algInfo->maxKeySize)) {
		return kCCParamError;
	}
	cryptCtx->algInfo = algInfo;
	switch(op) {
		case kCCEncrypt:
			cryptCtx->update = algInfo->encrypt;
			cryptCtx->updateBlocks = algInfo->encryptBlocks;
			cryptCtx->encrypting = true;
			break;
		case kCCDecrypt:
			cryptCtx->update = algInfo->decrypt;
			cryptCtx->updateBlocks = algInfo->decryptBlocks;
			cryptCtx->encrypting = false;
			break;
		default:
			return kCCParamError;
	}
	cryptCtx->pkcsPad = (options & kCCOptionPKCS7Padding) ? true : false;
	if(!(options & kCCOptionECBMode)) {
		cryptCtx->cbc = true;
		if(algInfo->algDoesCbc) {
			cryptCtx->doCbc = false;
		}
		else {
			cryptCtx->doCbc = true;
		}
	}
	else {
		cryptCtx->cbc = false;
		cryptCtx->doCbc = false;
	}
	cryptCtx->inBufSize = 0;
	
	/* perform key schedule */
	if(algInfo->setKey(cryptCtx->algCtx, key, keyLength, cryptCtx->encrypting)) {
		/* assume only error is bad key length */
		return kCCParamError;
	}
	/* then IV if necessary */
	if(cryptCtx->cbc) {
		ccBlockCipherSetIV(cryptCtx, iv);
	}
	return kCCSuccess;
}

static CCCryptorStatus CCBlockCipherRelease(
	void *ctx)
{
	CCBlockCipherContext cryptCtx = (CCBlockCipherContext)ctx;

	memset(cryptCtx, 0, 
		offsetof(struct _CCBlockCipherContext, algCtx) + cryptCtx->algInfo->ctxSize - 1);
	return kCCSuccess;
}

/* 
 * Update, arbitrary input size.
 * This always leaves data in ctx->inBuf if we're decrypting and 
 * padding is enabled. 
 */
static CCCryptorStatus CCBlockCipherUpdate(
	void *ctx,
	const void *dataIn,
	size_t dataInLen,
	void *dataOut,           /* data RETURNED here */
	size_t dataOutAvailable,
	size_t *dataOutMoved)		/* number of bytes written */
{
	CCBlockCipherContext cryptCtx = (CCBlockCipherContext)ctx;
	uint8_t		*uInp = (uint8_t *)dataIn;
	uint8_t		*uOutp = (uint8_t *)dataOut;
	size_t	 	uInSize = dataInLen;	// input bytes to go
	size_t 		uOutSize = 0;			// ouput bytes generated
	size_t		uOutLeft = dataOutAvailable;	// bytes remaining in outp
	size_t 		toMove;
	size_t		blocks;
	unsigned	i;
	bool		needLeftOver;
	const CCAlgInfo	*algInfo;
	size_t		blockSize;
	unsigned	leftOver;
	
	if((dataIn == NULL) || (dataOut == NULL) || (dataOutMoved == NULL)) {
		return kCCParamError;
	}
	needLeftOver = !cryptCtx->encrypting && cryptCtx->pkcsPad;
	algInfo = cryptCtx->algInfo;
	blockSize = algInfo->blockSize;
	
	/* 
	 * First make sure the caller has provided enough output buffer. 
	 * This routine only outputs complete blocks, and each output
	 * block requires a full block of input. 
	 */
	size_t totalInBytes = dataInLen + cryptCtx->inBufSize;	
	size_t totalBlocks = totalInBytes / blockSize;		/* truncated! */
	if(needLeftOver && (totalBlocks > 0)) {
		/* subtract one block that we keep for Final(), but only if 
		 * totalInBytes is well aligned - if it's not, we process
		 * the bufferred block */
		if((totalBlocks * blockSize) == totalInBytes) {
			totalBlocks--;
		}
	}
	size_t totalOutBytes = totalBlocks * blockSize;
	
	#if 0
	dprintf("dataInLen %lu inBufSize %lu totalBlocks %lu dataOutAvailable %lu\n",
		(unsigned long)dataInLen, (unsigned long)cryptCtx->inBufSize,
		(unsigned long)totalBlocks, (unsigned long)dataOutAvailable);
	#endif
	
	if(totalOutBytes > dataOutAvailable) {
		dprintf("CCBlockCipherUpdate: o/f encr %d totalOutBytes %lu dataOutAvailable %lu\n",
			cryptCtx->encrypting, totalOutBytes, dataOutAvailable);
		return kCCBufferTooSmall;
	}
	
	/* first deal with pending data */
	if(cryptCtx->inBufSize) {
		/* attempt to fill inBuf from inp */
		toMove = blockSize - cryptCtx->inBufSize;
		if(toMove > uInSize) {
			toMove = uInSize;
		}
		if(cryptCtx->encrypting && cryptCtx->doCbc) {
			/* xor into last cipherblock or IV */
			uint8_t *dst = &cryptCtx->inBuf[cryptCtx->inBufSize];
			for(i=0; i<toMove; i++) {
				*dst ^= *uInp++;
				dst++;
			}
		}
		else {
			/* use incoming data as is */
			memmove(cryptCtx->inBuf+cryptCtx->inBufSize, uInp, toMove);
			uInp += toMove;
		}
		uInSize             -= toMove;
		cryptCtx->inBufSize += toMove;
		
		/* 
		 * Process inBuf if it's full, but skip if no more data in uInp and
		 * inBuf might be needed for unpadding on decrypt.
		 */
		if((cryptCtx->inBufSize == blockSize) && !((uInSize == 0) && needLeftOver)) {
			if(uOutLeft < blockSize) {
				/* output overflow - shouldn't happen (we checked) */
				dprintf("kCCBufferTooSmall 3: uOutLeft %lu\n", 
					(unsigned long)uOutLeft);
				return kCCBufferTooSmall;
			}
			cryptCtx->update(cryptCtx->algCtx, cryptCtx->inBuf, uOutp);
			if(cryptCtx->doCbc) {
				if(cryptCtx->encrypting) {
					/* save ciphertext for chaining next block */
					memmove(cryptCtx->inBuf, uOutp, blockSize);
				}
				else {
					/* xor in last ciphertext */
					uint8_t *src = cryptCtx->chainBuf;
					for(i=0; i<blockSize; i++) {
						uOutp[i] ^= *src++;
					}
					/* save this ciphertext for next chain */
					memmove(cryptCtx->chainBuf, cryptCtx->inBuf, blockSize);
				}
			}
			uOutSize += blockSize;
			uOutp    += blockSize;
			uOutLeft -= blockSize;
			cryptCtx->inBufSize = 0;
		}
	}	/* processing inBuf */
	
	if(uInSize == 0) {
		/* done */
		*dataOutMoved = uOutSize;
		return kCCSuccess;
	}
	
	/* 
	 * en/decrypt even blocks in (remaining) inp.  
	 */
	leftOver = uInSize % blockSize;
	if((leftOver == 0) && needLeftOver) {
		/* 
		 * Even blocks coming in, but we really need to leave some data
		 * in the buffer because we're decrypting with PKCS padding). 
		 * Save one block for inBuf.
		 */
		leftOver = blockSize; 
	}
	toMove = uInSize - leftOver;
	blocks = toMove / blockSize;
	if(cryptCtx->updateBlocks && !cryptCtx->doCbc && (blocks != 0)) {
		/* optimized multi block processing */
		size_t thisMove = blocks * blockSize;
		cryptCtx->updateBlocks(cryptCtx->algCtx, uInp, blocks, uOutp);
		uOutSize += thisMove;
		uOutp    += thisMove;
		uInp	 += thisMove;
		uOutLeft -= thisMove;
		toMove   -= thisMove; 
	}
	else if(cryptCtx->encrypting) {
		/* encrypt a block at a time */
		while(toMove) {
			if(uOutLeft < blockSize) {
				/* output overflow - shouldn't happen (we checked) */
				dprintf("kCCBufferTooSmall 1: uOutLeft %lu\n", 
					(unsigned long)uOutLeft);
				return kCCBufferTooSmall;
			}
			if(!cryptCtx->doCbc) {
				/* encrypt directly from input to output */
				cryptCtx->update(cryptCtx->algCtx, uInp, uOutp);
			}
			else {
				/* xor into last ciphertext, encrypt the result */
				uint8_t *dst = cryptCtx->inBuf;
				for(i=0; i<blockSize; i++) {
					*dst ^= uInp[i];
					dst++;
				}
				cryptCtx->update(cryptCtx->algCtx, cryptCtx->inBuf, uOutp);
				
				/* save new ciphertext for next chain */
				memmove(cryptCtx->inBuf, uOutp, blockSize);
			}
			uOutSize += blockSize;
			uOutp    += blockSize;
			uInp	 += blockSize;
			uOutLeft -= blockSize;
			toMove   -= blockSize; 
		}	/* main encrypt loop */
	}	
	else {
		/* decrypt a block at a time */
		while(toMove) {
			if(uOutLeft < blockSize) {
				/* output overflow - we already checked */
				dprintf("kCCBufferTooSmall 2: uOutLeft %lu toMove %lu\n", 
					(unsigned long)uOutLeft, (unsigned long)toMove);
				return kCCBufferTooSmall;
			}
			if(cryptCtx->doCbc) {
				uint8_t *src = cryptCtx->chainBuf;
				
				/* save this ciphertext for chain; don't assume in != out */
				memmove(cryptCtx->inBuf, uInp, blockSize);
				cryptCtx->update(cryptCtx->algCtx, uInp, uOutp);
				
				/* chain in previous ciphertext */
				for(i=0; i<blockSize; i++) {
					uOutp[i] ^= *src++;
				}
				
				/* save current ciphertext for next block */
				memmove(cryptCtx->chainBuf, cryptCtx->inBuf, blockSize);
			}
			else {
				/* ECB */
				cryptCtx->update(cryptCtx->algCtx, uInp, uOutp);
			}
			uOutSize += blockSize;
			uOutp    += blockSize;
			uInp	 += blockSize;
			uOutLeft -= blockSize;
			toMove   -= blockSize; 
		}	/* main decrypt loop */
	}
	
	/* leftover bytes from inp --> inBuf */
	if(leftOver) {
		if(cryptCtx->encrypting && cryptCtx->doCbc) {
			/* xor into last cipherblock or IV */
			uint8_t *dst = cryptCtx->inBuf;
			for(i=0; i<leftOver; i++) {
				*dst ^= *uInp++;
				dst++;
			}
		}
		else {
			memmove(cryptCtx->inBuf, uInp, leftOver);
		}
	}
	cryptCtx->inBufSize = leftOver;
	*dataOutMoved = uOutSize;
	return kCCSuccess;
}

static CCCryptorStatus CCBlockCipherFinal(
   void *ctx,
   void *dataOut,           /* data RETURNED here */
	size_t dataOutAvailable,
	size_t *dataOutMoved)		/* number of bytes written */
{
	size_t			uOutSize = 0;			// ouput bytes generated
	size_t			actMoved;
	unsigned		i;
	const CCAlgInfo	*algInfo;
	size_t			blockSize;
	CCCryptorStatus	ourRtn = kCCSuccess;
	CCBlockCipherContext	cryptCtx = (CCBlockCipherContext)ctx;
	
	if((dataOut == NULL) || (dataOutMoved == NULL)) {
		return kCCParamError;
	}
	algInfo = cryptCtx->algInfo;
	blockSize = algInfo->blockSize;
	
	if(cryptCtx->encrypting) {
		/* 
		 * First ensure that the caller provided sufficient output
		 * buffer space. 
		 * If we have any bufferred input, or we are doing padding, 
		 * the output is exactly one block. Otherwise the output
		 * is zero.
		 */
		size_t required = 0;
		if((cryptCtx->inBufSize != 0) || (cryptCtx->pkcsPad)) {
			required = blockSize;
		}
		if(required > dataOutAvailable) {
			dprintf("CCBlockCipherFinal: o/f (1): required %lu dataOutAvailable %lu\n",
				required, dataOutAvailable);
			return kCCBufferTooSmall;
		}
		
		if(cryptCtx->pkcsPad) {
			/* 
			 * PKCS5/7 padding: pad byte = size of padding. 
			 */
			size_t padSize = blockSize - cryptCtx->inBufSize;
			uint8_t *padPtr  = cryptCtx->inBuf + cryptCtx->inBufSize;
			if(!cryptCtx->doCbc) {
				for(i=0; i<padSize; i++) {
					*padPtr++ = padSize;
				}
			}
			else {
				for(i=0; i<padSize; i++) {
					*padPtr++ ^= padSize;
				}
			}
			cryptCtx->inBufSize = blockSize;
		}	/* PKCS padding */
		
		/*
		 * Encrypt final inBuf. Abort if not full (meaning, padding 
		 * is disabled and caller gave us unaligned plaintext).
		 */
		if(cryptCtx->inBufSize) {
			if(cryptCtx->inBufSize != blockSize) {
				ourRtn = kCCParamError;
				goto errOut;
			}
			cryptCtx->update(cryptCtx->algCtx, cryptCtx->inBuf, dataOut);
			uOutSize += blockSize;
			cryptCtx->inBufSize = 0;
		}
		*dataOutMoved = uOutSize;
	}	/* encrypting */
	
	else {
		if(cryptCtx->inBufSize == 0) {
			if(cryptCtx->pkcsPad) {
				/* we must have never gotten a block's worth of ciphertext */
				ourRtn = kCCParamError;
				goto errOut;
			}
			else {
				/* simple decrypt op complete */
				*dataOutMoved = 0;
				goto errOut;
			}
		}
		
		/*
		 * Decrypt - must have exactly one block of ciphertext.
		 */
		if(cryptCtx->inBufSize != blockSize) {
			ourRtn = kCCParamError;
			goto errOut;
		}
		if(dataOutAvailable < blockSize) {
			dprintf("CCBlockCipherFinal: o/f (2): dataOutAvailable %lu\n", 
				(unsigned long)dataOutAvailable);
			return kCCBufferTooSmall;
		}

		cryptCtx->update(cryptCtx->algCtx, cryptCtx->inBuf, dataOut);
		if(cryptCtx->doCbc) {
			/* chain in previous ciphertext one more time */
			uint8_t *src = cryptCtx->chainBuf;
			uint8_t *dst = dataOut;
			for(i=0; i<blockSize; i++) {
				*dst ^= *src++;
				dst++;
			}
		}
		actMoved = blockSize;
		if(cryptCtx->pkcsPad) {
			/* ensure integrity of padding byte(s) */
			unsigned char *cp = (unsigned char *)dataOut;
			unsigned padSize = cp[blockSize - 1];
			if(padSize > blockSize) {
				/* result of garbled ciphertext or wrong key */
				ourRtn = kCCDecodeError;
				goto errOut;
			}
			uint8_t *padPtr = cp + blockSize - padSize;
			unsigned i;
			for(i=0; i<padSize; i++) {
				if(*padPtr++ != padSize) {
					ourRtn = kCCDecodeError;
					goto errOut;
				}
			}
			actMoved -= padSize;
		}
		*dataOutMoved = actMoved;
	}	/* decrypting */
	
errOut:
	return ourRtn;
}

static CCCryptorStatus CCBlockCipherReset(
	void *ctx,
	const void *iv)
{
	CCBlockCipherContext cryptCtx = (CCBlockCipherContext)ctx;

	if(cryptCtx->cbc) {
		ccBlockCipherSetIV(cryptCtx, iv);
	}
	cryptCtx->inBufSize = 0;
	return kCCSuccess;
}

/* normal OutputSize */
static size_t CCBlockCipherOutputSize(
	void *ctx, size_t inputLength, bool final)
{
	CCBlockCipherContext cryptCtx = (CCBlockCipherContext)ctx;
	
	size_t blockSize = cryptCtx->algInfo->blockSize;
	size_t totalInBytes = inputLength + cryptCtx->inBufSize;	
	size_t blocks = totalInBytes / blockSize;		/* truncated! */
	
	if(final && cryptCtx->encrypting && cryptCtx->pkcsPad) {
		/* one extra block for padding as appropriate */
		blocks++;
	}
	
	/* 
	 * Note we ignore the needLeftOver corner case calculated in Update();
	 * we just take outputSize := inputSize, in full blocks.
	 */
	return blocks * blockSize;
}

/* one-shot output size */
static CCCryptorStatus CCBlockCipherOneShotSize(
	CCOperation op,
	CCAlgorithm alg,
	CCOptions options,
	size_t inputLen,
	size_t *outputLen)
{
	const CCAlgInfo *algInfo = ccBlockCipherFindAlgInfo(alg);
	size_t totalBlocks;	
	size_t blockSize;

	if(algInfo == NULL) {
		return kCCParamError;
	}
	blockSize = algInfo->blockSize;
	totalBlocks = (inputLen + blockSize - 1) / blockSize;
	if((op == kCCEncrypt) && (options & kCCOptionPKCS7Padding)) {
		if((totalBlocks * blockSize) == inputLen) {
			/* encrypting, padding, well-aligned input: add another block */
			totalBlocks++;
		}
	}
	*outputLen = totalBlocks * blockSize;
	return kCCSuccess;
}

/* 
 * Callouts used by CommonCryptor.
 */
const CCCryptSpiCallouts ccBlockCipherCallouts = 
{
	CCBlockCipherContextSize,
	CCBlockCipherInit,
	CCBlockCipherRelease,
	CCBlockCipherUpdate,
	CCBlockCipherFinal,
	CCBlockCipherReset,
	CCBlockCipherOutputSize,
	CCBlockCipherOneShotSize
};

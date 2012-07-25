/*
 * Copyright (c) 2012 Apple Inc. All Rights Reserved.
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
 */

// #define COMMON_CRYPTOR_FUNCTIONS

#include "ccMemory.h"
#include "ccdebug.h"
#include "CommonCryptor.h"
#include "CommonCryptorSPI.h"
#include "CommonCryptorPriv.h"
#include <dispatch/dispatch.h>
#include <dispatch/queue.h>

/* 
 * CommonCryptor's portion of a CCCryptorRef. 
 */


typedef struct cipherMode_t {
    dispatch_once_t init;
    struct ccmode_ecb* ecb;
    struct ccmode_cbc* cbc;
    struct ccmode_cfb* cfb;
    struct ccmode_cfb8* cfb8;
    struct ccmode_ctr* ctr;
    struct ccmode_ofb* ofb;
    struct ccmode_xts* xts;
    struct ccmode_gcm* gcm;
} cipherMode;

static cipherMode cipherModeTab[7][2];

static inline size_t ccGetCipherBlockSize(CCCryptor *ref)
{
    switch(ref->cipher) {
        case kCCAlgorithmAES128:    return kCCBlockSizeAES128;
        case kCCAlgorithmDES:       return kCCBlockSizeDES;  
        case kCCAlgorithm3DES:      return kCCBlockSize3DES;       
        case kCCAlgorithmCAST:      return kCCBlockSizeCAST;      
        case kCCAlgorithmRC4:       return 1;
        case kCCAlgorithmRC2:       return kCCBlockSizeRC2;
        case kCCAlgorithmBlowfish:  return kCCBlockSizeBlowfish;
        default: return kCCBlockSizeAES128;
    }
}


corecryptoMode getCipherMode(CCAlgorithm cipher, CCMode mode, CCOperation direction)
{
    for(int i = 0; i<2; i++) {
        dispatch_once(&(cipherModeTab[cipher][i].init), ^{
            cipherModeTab[cipher][i].ecb = ccmodeList[cipher][i].ecb();
            cipherModeTab[cipher][i].cbc = ccmodeList[cipher][i].cbc();
            cipherModeTab[cipher][i].cfb = ccmodeList[cipher][i].cfb();
            cipherModeTab[cipher][i].cfb8 = ccmodeList[cipher][i].cfb8();
            cipherModeTab[cipher][i].ctr = ccmodeList[cipher][i].ctr();
            cipherModeTab[cipher][i].ofb = ccmodeList[cipher][i].ofb();
#if defined(__i386__) || defined(__x86_64__)
            cipherModeTab[cipher][i].xts = ccmodeList[cipher][i].xts();
#else
            cipherModeTab[cipher][i].xts = NULL;
#endif
            cipherModeTab[cipher][i].gcm = ccmodeList[cipher][i].gcm();
        });
    }
    // printf("%lu Size %lu Blocksize\n\n", cipherModeTab[cipher][direction].ecb->size, cipherModeTab[cipher][direction].ecb->block_size);
    switch(mode) {
        case kCCModeECB: return (corecryptoMode) cipherModeTab[cipher][direction].ecb;
        case kCCModeCBC: return (corecryptoMode) cipherModeTab[cipher][direction].cbc;
        case kCCModeCFB: return (corecryptoMode) cipherModeTab[cipher][direction].cfb;
        case kCCModeCFB8: return (corecryptoMode) cipherModeTab[cipher][direction].cfb8;
        case kCCModeCTR: return (corecryptoMode) cipherModeTab[cipher][direction].ctr;
        case kCCModeOFB: return (corecryptoMode) cipherModeTab[cipher][direction].ofb;
        case kCCModeXTS: return (corecryptoMode) cipherModeTab[cipher][direction].xts;
        case kCCModeGCM: return (corecryptoMode) cipherModeTab[cipher][direction].gcm;
    }
    return (corecryptoMode) (struct ccmode_ecb*) NULL;
}

static CCCryptorStatus setCryptorCipherMode(CCCryptor *ref, CCAlgorithm cipher, CCMode mode, CCOperation direction)
{
    switch(mode) {
        case kCCModeECB: if((ref->symMode[direction].ecb = getCipherMode(cipher, mode, direction).ecb) == NULL) return kCCUnimplemented;
            ref->modeDesc = &ccecb_mode; break;
        case kCCModeCBC: if((ref->symMode[direction].cbc = getCipherMode(cipher, mode, direction).cbc) == NULL) return kCCUnimplemented;
            ref->modeDesc = &cccbc_mode; break;
        case kCCModeCFB: if((ref->symMode[direction].cfb = getCipherMode(cipher, mode, direction).cfb) == NULL) return kCCUnimplemented;
            ref->modeDesc = &cccfb_mode; break;
        case kCCModeCFB8: if((ref->symMode[direction].cfb8 = getCipherMode(cipher, mode, direction).cfb8) == NULL) return kCCUnimplemented;
            ref->modeDesc = &cccfb8_mode; break;
        case kCCModeCTR: if((ref->symMode[direction].ctr = getCipherMode(cipher, mode, direction).ctr) == NULL) return kCCUnimplemented;
            ref->modeDesc = &ccctr_mode; break;
        case kCCModeOFB: if((ref->symMode[direction].ofb = getCipherMode(cipher, mode, direction).ofb) == NULL) return kCCUnimplemented;
            ref->modeDesc = &ccofb_mode; break;
        case kCCModeXTS: if((ref->symMode[direction].xts = getCipherMode(cipher, mode, direction).xts) == NULL) return kCCUnimplemented;
            ref->modeDesc = &ccxts_mode; break;
        case kCCModeGCM: if((ref->symMode[direction].gcm = getCipherMode(cipher, mode, direction).gcm) == NULL) return kCCUnimplemented;
            ref->modeDesc = &ccgcm_mode; break;
        default: return kCCParamError;
    }
    return kCCSuccess;

}


static inline CCCryptorStatus ccSetupCryptor(CCCryptor *ref, CCAlgorithm cipher, CCMode mode, CCOperation direction, CCPadding padding)
{
    CCCryptorStatus retval;
    
    if(cipher > 6) return kCCParamError;
    if(direction > kCCBoth) return kCCParamError;
    if(cipher == kCCAlgorithmRC4) mode = kCCModeOFB;
    
    ref->mode = mode;
    CCOperation op = direction;
    if(ref->mode == kCCModeXTS || ref->mode == kCCModeECB || ref->mode == kCCModeCBC) op = kCCBoth;

    // printf("Cryptor setup - cipher %d mode %d direction %d padding %d\n", cipher, mode, direction, padding);
    switch(op) {
        case kCCEncrypt:
        case kCCDecrypt:
            if((retval = setCryptorCipherMode(ref, cipher, mode, op)) != kCCSuccess) return retval;
            if((ref->ctx[op].data = CC_XMALLOC(ref->modeDesc->mode_get_ctx_size(ref->symMode[op]))) == NULL) return kCCMemoryFailure;
            break;
        case kCCBoth:
            if((retval = setCryptorCipherMode(ref, cipher, mode, kCCEncrypt)) != kCCSuccess) return retval;
            if((ref->ctx[kCCEncrypt].data = CC_XMALLOC(ref->modeDesc->mode_get_ctx_size(ref->symMode[kCCEncrypt]))) == NULL) return kCCMemoryFailure;
            if((retval = setCryptorCipherMode(ref, cipher, mode, kCCDecrypt)) != kCCSuccess) return retval;
            if((ref->ctx[kCCDecrypt].data = CC_XMALLOC(ref->modeDesc->mode_get_ctx_size(ref->symMode[kCCDecrypt]))) == NULL) return kCCMemoryFailure;
            break;
    }
    
    switch(padding) {
        case ccNoPadding:
            ref->padptr = &ccnopad_pad;
            break;
        case ccPKCS7Padding:
            if(mode == kCCModeCBC)
                ref->padptr = &ccpkcs7_pad;
            else
                ref->padptr = &ccpkcs7_ecb_pad;
            break;
        case ccCBCCTS1:
            ref->padptr = &cccts1_pad;
            break;
        case ccCBCCTS2:
            ref->padptr = &cccts2_pad;
            break;
        case ccCBCCTS3:
            ref->padptr = &cccts3_pad;
            break;
        default:
            ref->padptr = &ccnopad_pad;
    }
    ref->cipher = cipher;
    ref->cipherBlocksize = ccGetCipherBlockSize(ref);
    ref->op = direction;
    ref->bufferPos = 0;
    ref->bytesProcessed = 0;
    return kCCSuccess;
}

#define OP4INFO(X) (((X)->op == 3) ? 0: (X)->op)

static inline size_t ccGetBlockSize(CCCryptor *ref)
{
    return ref->modeDesc->mode_get_block_size(ref->symMode[OP4INFO(ref)]);
}

static inline bool ccIsStreaming(CCCryptor *ref)
{
    
    return ref->modeDesc->mode_get_block_size(ref->symMode[ref->op]) == 1;
}

static inline CCCryptorStatus ccInitCryptor(CCCryptor *ref, const void *key, unsigned long key_len,
                                            const void *tweak_key, 
                                            const void *iv)
{
    size_t blocksize = ccGetCipherBlockSize(ref);
    if(!ref->modeDesc) return kCCParamError;
    uint8_t defaultIV[blocksize];
    
    if(iv == NULL) {
        CC_XZEROMEM(defaultIV, blocksize);
        iv = defaultIV;
    }
    
    CCOperation op = ref->op;
    
    // This will create both sides of the context/mode pairs for now.
    if(ref->mode == kCCModeXTS || ref->mode == kCCModeECB || ref->mode == kCCModeCBC) op = kCCBoth;
    
    switch(op) {
        case kCCEncrypt:
        case kCCDecrypt:
            ref->modeDesc->mode_setup(ref->symMode[ref->op], iv, key, key_len, tweak_key, 0, 0, ref->ctx[ref->op]);
            break;
        case kCCBoth:
            ref->modeDesc->mode_setup(ref->symMode[kCCEncrypt], iv, key, key_len, tweak_key, 0, 0, ref->ctx[kCCEncrypt]);
            ref->modeDesc->mode_setup(ref->symMode[kCCDecrypt], iv, key, key_len, tweak_key, 0, 0, ref->ctx[kCCDecrypt]);
            break;
    }
    return kCCSuccess;    
}

static inline CCCryptorStatus ccDoEnCrypt(CCCryptor *ref, const void *dataIn, size_t dataInLength, void *dataOut)
{
    if(!ref->modeDesc->mode_encrypt) return kCCParamError;
    ref->modeDesc->mode_encrypt(ref->symMode[kCCEncrypt], dataIn, dataOut, dataInLength, ref->ctx[kCCEncrypt]);
    return kCCSuccess;
}

static inline CCCryptorStatus ccDoDeCrypt(CCCryptor *ref, const void *dataIn, size_t dataInLength, void *dataOut)
{
    if(!ref->modeDesc->mode_decrypt) return kCCParamError;
    ref->modeDesc->mode_decrypt(ref->symMode[kCCDecrypt], dataIn, dataOut, dataInLength, ref->ctx[kCCDecrypt]);
    return kCCSuccess;
}

static inline CCCryptorStatus ccDoEnCryptTweaked(CCCryptor *ref, const void *dataIn, size_t dataInLength, void *dataOut, const void *tweak)
{
    if(!ref->modeDesc->mode_encrypt_tweaked) return kCCParamError;
    ref->modeDesc->mode_encrypt_tweaked(ref->symMode[kCCEncrypt], dataIn, dataOut, dataInLength, tweak, ref->ctx[kCCEncrypt]);
    return kCCSuccess;
}

static inline CCCryptorStatus ccDoDeCryptTweaked(CCCryptor *ref, const void *dataIn, size_t dataInLength, void *dataOut, const void *tweak)
{
    if(!ref->modeDesc->mode_decrypt_tweaked) return kCCParamError;
    ref->modeDesc->mode_decrypt_tweaked(ref->symMode[kCCDecrypt], dataIn, dataOut, dataInLength, tweak, ref->ctx[kCCDecrypt]);
    return kCCSuccess;
}



static inline CCCryptorStatus ccGetIV(CCCryptor *ref, void *iv, size_t *ivLen)
{
    if(ref->modeDesc->mode_getiv == NULL) return kCCParamError;
    if(ref->modeDesc->mode_getiv(ref->symMode[OP4INFO(ref)], iv, ivLen, ref->ctx[OP4INFO(ref)]) != 0) return kCCMemoryFailure;
    return kCCSuccess;
}

static inline CCCryptorStatus ccSetIV(CCCryptor *ref, void *iv, size_t ivLen)
{
    if(ref->modeDesc->mode_setiv == NULL) return kCCParamError;
    if(ref->modeDesc->mode_setiv(ref->symMode[OP4INFO(ref)], iv, ivLen, ref->ctx[OP4INFO(ref)]) != 0) return kCCMemoryFailure;
    return kCCSuccess;
}



static inline void ccClearCryptor(CCCryptor *ref)
{
    CC_XZEROMEM(ref->buffptr, sizeof(ref->buffptr));
    CCOperation op = ref->op;
    
    // This will clear both sides of the context/mode pairs for now.
    if(ref->mode == kCCModeXTS || ref->mode == kCCModeECB || ref->mode == kCCModeCBC) op = kCCBoth;
    switch(op) {
        case kCCEncrypt:
        case kCCDecrypt:
            CC_XZEROMEM(ref->ctx[ref->op].data, ref->modeDesc->mode_get_ctx_size(ref->symMode[ref->op]));
            CC_XFREE(ref->ctx[ref->op].data, ref->modeDesc->mode_get_ctx_size(ref->symMode[ref->op]));
            break;
        case kCCBoth:
            for(int i = 0; i<2; i++) {
                CC_XZEROMEM(ref->ctx[i].data, ref->modeDesc->mode_get_ctx_size(ref->symMode[i]));
                CC_XFREE(ref->ctx[i].data, ref->modeDesc->mode_get_ctx_size(ref->symMode[i]));
            }
            break;
    }
    ref->cipher = 0;
    ref->mode = 0;
    ref->op = 0;
    ref->bufferPos = 0;
    ref->bytesProcessed = 0;
}

static inline CCCryptorStatus ccEncryptPad(CCCryptor	*cryptor, void *buf, size_t *moved)
{
    if(cryptor->padptr->encrypt_pad(cryptor->ctx[cryptor->op], cryptor->modeDesc, cryptor->symMode[cryptor->op], cryptor->buffptr, cryptor->bufferPos, buf, moved)) return kCCDecodeError;
    return kCCSuccess;
}

static inline CCCryptorStatus ccDecryptPad(CCCryptor	*cryptor, void *buf, size_t *moved)
{
    if(cryptor->padptr->decrypt_pad(cryptor->ctx[cryptor->op], cryptor->modeDesc, cryptor->symMode[cryptor->op], cryptor->buffptr, cryptor->bufferPos, buf, moved)) return kCCDecodeError;
    return kCCSuccess;
}

static inline size_t ccGetReserve(CCCryptor *cryptor)
{
    return cryptor->padptr->padreserve(cryptor->op == kCCEncrypt, cryptor->modeDesc, cryptor->symMode[cryptor->op]);
}

static inline size_t ccGetPadlen(CCCryptor *cryptor)
{
    return cryptor->padptr->padlen(cryptor->op == kCCEncrypt, cryptor->modeDesc, cryptor->symMode[cryptor->op], cryptor->buffptr);
}



static uint8_t *
ccGetBytesAligned64(uint8_t *fromptr, uint32_t *bytesused)
{
	uint8_t *retval;
    
	retval = fromptr;
	if((uintptr_t) fromptr % 8) retval = (uint8_t *)(((uintptr_t) fromptr / 8) * 8 + 8);
	*bytesused = retval - fromptr;
	return retval;
}














static int ccAddBuff(CCCryptor *cryptor, const void *dataIn, size_t dataInLength)
{
    CC_XMEMCPY((char *) cryptor->buffptr + cryptor->bufferPos, dataIn, dataInLength);
    cryptor->bufferPos += dataInLength;
    return dataInLength;
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
	CCCryptorStatus err;
	CCCompatCryptor *compat_cryptor = NULL;
	size_t dataUsed = 0;
	
    CC_DEBUG_LOG(ASL_LEVEL_ERR, "Entering\n");
	if((compat_cryptor = (CCCompatCryptor *)CC_XMALLOC(sizeof(CCCompatCryptor))) == NULL) return kCCMemoryFailure;
	err = CCCryptorCreateFromData( op, alg,  options, key,  keyLength, iv, compat_cryptor, sizeof(CCCompatCryptor), cryptorRef, &dataUsed); 
	if(err != kCCSuccess)  CC_XFREE(compat_cryptor, sizeof(CCCompatCryptor));
	else compat_cryptor->weMallocd = true;
	return err;
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
	CCMode			mode;
    CCPadding		padding;		
	const void 		*tweak;
	size_t 			tweakLength;	
	int				numRounds;
	CCModeOptions 	modeOptions;
    
    CC_DEBUG_LOG(ASL_LEVEL_ERR, "Entering\n");
	/* Determine mode from options - old call only supported ECB and CBC 
       we treat RC4 as a "mode" in that it's the only streaming cipher
       currently supported 
    */
    if(alg == kCCAlgorithmRC4) mode = kCCModeRC4;
    else if(options & kCCOptionECBMode) mode = kCCModeECB;
	else mode = kCCModeCBC;
    
	/* Determine padding from options - only PKCS7 was available */
    padding = ccNoPadding;
	if(options & kCCOptionPKCS7Padding) padding = ccPKCS7Padding;
   
	/* No tweak was ever used */
   	tweak = NULL;
    tweakLength = 0;
    
	/* default rounds */
    numRounds = 0;
    
	/* No mode options needed */
    modeOptions = 0;
    
	return CCCryptorCreateFromDataWithMode(op, mode, alg, padding, iv, key, keyLength, tweak, tweakLength, numRounds, modeOptions, data, dataLength, cryptorRef, dataUsed);
}

/* This version mallocs the CCCryptorRef */

CCCryptorStatus CCCryptorCreateWithMode(
	CCOperation 	op,				/* kCCEncrypt, kCCEncrypt, kCCBoth (default for BlockMode) */
	CCMode			mode,
	CCAlgorithm		alg,
	CCPadding		padding,		
	const void 		*iv,			/* optional initialization vector */
	const void 		*key,			/* raw key material */
	size_t 			keyLength,	
	const void 		*tweak,			/* raw tweak material */
	size_t 			tweakLength,	
	int				numRounds,		/* 0 == default */
	CCModeOptions 	options,
	CCCryptorRef	*cryptorRef)	/* RETURNED */
{
	CCCryptorStatus err;
	CCCompatCryptor *compat_cryptor = NULL;
	size_t dataUsed = 0;
	
    CC_DEBUG_LOG(ASL_LEVEL_ERR, "Entering\n");
	if((compat_cryptor = (CCCompatCryptor *)CC_XMALLOC(sizeof(CCCompatCryptor))) == NULL) return kCCMemoryFailure;
	err = CCCryptorCreateFromDataWithMode( op, mode, alg, padding, iv, key,  keyLength, tweak, tweakLength, numRounds, options, compat_cryptor, DEFAULT_CRYPTOR_MALLOC, cryptorRef, &dataUsed); 
	if(err != kCCSuccess)  CC_XFREE(compat_cryptor, sizeof(CCCompatCryptor));
	else compat_cryptor->weMallocd = true;
	return err;
}

#define KEYALIGNMENT (sizeof(int)-1)

CCCryptorStatus CCCryptorCreateFromDataWithMode(
	CCOperation 	op,				/* kCCEncrypt, kCCEncrypt, kCCBoth (default for BlockMode) */
	CCMode			mode,
	CCAlgorithm		alg,
	CCPadding		padding,		
	const void 		*iv,			/* optional initialization vector */
	const void 		*key,			/* raw key material */
	size_t 			keyLength,	
	const void 		*tweak,			/* raw tweak material */
	size_t 			tweakLength,	
	int				numRounds,
	CCModeOptions 	options,
	const void		*data,			/* caller-supplied memory */
	size_t			dataLength,		/* length of data in bytes */
	CCCryptorRef	*cryptorRef,	/* RETURNED */
	size_t			*dataUsed)		/* optional, RETURNED */
{
	CCCryptorStatus retval = kCCSuccess;
	CCCryptor *cryptor = NULL;
    CCCompatCryptor *compat_cryptor = NULL;
	int	needed, needed2aligncryptor;
    uint8_t *alignedKey = NULL;

    CC_DEBUG_LOG(ASL_LEVEL_ERR, "Entering Op: %d Mode: %d Cipher: %d Padding: %d\n", op, mode, alg, padding);

    // For now we're mapping these two AES selectors to the stock one.
    if(alg == kCCAlgorithmAES128NoHardware || alg == kCCAlgorithmAES128WithHardware) 
        alg = kCCAlgorithmAES128;
    
    /* corecrypto only implements CTR_BE.  No use of CTR_LE was found so we're marking
       this as unimplemented for now.  Also in Lion this was defined in reverse order.
       See <rdar://problem/10306112> */
    
    if(mode == kCCModeCTR && options != kCCModeOptionCTR_BE) {
        CC_DEBUG_LOG(ASL_LEVEL_ERR, "Mode is CTR, but options isn't BE\n", op, mode, alg, padding);
        return kCCUnimplemented;
    }

    // validate pointers
	if((data == NULL) || (cryptorRef == NULL) || (key == NULL)) {
		CC_DEBUG_LOG(ASL_LEVEL_ERR, "bad arguments\n", 0);
		return kCCParamError;
	}
    
    /*
     * Some implementations are sensitive to keys not being 4 byte aligned.
     * We'll move the key into an aligned buffer for the call to setup
     * the key schedule.
     */
    
    if((intptr_t) key & KEYALIGNMENT) {
        if((alignedKey = CC_XMALLOC(keyLength)) == NULL) {
            return kCCMemoryFailure;
        }
        CC_XMEMCPY(alignedKey, key, keyLength);
        key = alignedKey;
    }
    

	/* Get Space for Cryptor Structure */	
	compat_cryptor = (CCCompatCryptor *) ccGetBytesAligned64((uint8_t *)data, &needed2aligncryptor);
    needed = needed2aligncryptor + sizeof(CCCompatCryptor);
    if(dataUsed != NULL) *dataUsed = needed;

	if (needed > dataLength) {
		if(dataUsed != NULL) *dataUsed += 16; /* in case it's not on a doubleword boundary */
		CC_DEBUG_LOG(ASL_LEVEL_ERR, "Needed %d Have %d\n", needed, dataLength);
        retval = kCCBufferTooSmall;
        goto out;
	}
    compat_cryptor->weMallocd = false;
    
    if((cryptor = (CCCryptor *)CC_XMALLOC(DEFAULT_CRYPTOR_MALLOC)) == NULL) {
        retval = kCCMemoryFailure;
        goto out;
    }
	compat_cryptor->cryptor = cryptor;
		
	*cryptorRef = compat_cryptor;
        
    if((retval = ccSetupCryptor(cryptor, alg, mode, op, padding)) != kCCSuccess) {
        printf("Failed to setup Cryptor struct with alg/mode %d\n", retval);
        goto out;
    }
    
    if((retval = ccInitCryptor(cryptor, key, keyLength, tweak, iv)) != kCCSuccess) {
        printf("Failed to init Cryptor %d\n", retval);
        goto out;
    }


out:
    // Things to destroy if setup failed
    if(retval) {
        *cryptorRef = NULL;
        if(compat_cryptor) compat_cryptor->cryptor = NULL;
        if(cryptor) {
            CC_XZEROMEM(cryptor, DEFAULT_CRYPTOR_MALLOC);
            CC_XFREE(cryptor, DEFAULT_CRYPTOR_MALLOC);
        }
    } else {
        // printf("Blocksize = %d mode = %d pad = %d\n", ccGetBlockSize(cryptor), cryptor->mode, padding);
    }
    
    // Things to destroy all the time
    if(alignedKey) {
        CC_XZEROMEM(alignedKey, keyLength);
        CC_XFREE(alignedKey, keyLength);
    }
    
    return retval;
}





CCCryptorStatus CCCryptorRelease(
	CCCryptorRef cryptorRef)
{
	uint32_t err;
    CCCompatCryptor *compat_cryptor = cryptorRef;
    CCCryptor *cryptor;
	uint32_t weMallocd;
    
    CC_DEBUG_LOG(ASL_LEVEL_ERR, "Entering\n");
	if(cryptorRef == NULL) return kCCSuccess;
    cryptor = compat_cryptor->cryptor;
	weMallocd = compat_cryptor->weMallocd;
    
    ccClearCryptor(cryptor);
    
	CC_XMEMSET(cryptor, 0, CCCRYPTOR_SIZE);
    CC_XFREE(cryptor, DEFAULT_CRYPTOR_MALLOC);
	if(weMallocd)  CC_XFREE(compat_cryptor, sizeof(CCCompatCryptor));
	return kCCSuccess;
}

#define FULLBLOCKSIZE(X,BLOCKSIZE) (((X)/(BLOCKSIZE))*BLOCKSIZE)
#define FULLBLOCKREMAINDER(X,BLOCKSIZE) ((X)%(BLOCKSIZE))

static CCCryptorStatus ccSimpleUpdate(CCCryptor *cryptor, const void *dataIn, size_t dataInLength, void **dataOut, size_t *dataOutAvailable, size_t *dataOutMoved)
{		
	CCCryptorStatus	retval;
    if(dataOutAvailable < dataInLength) return kCCBufferTooSmall;
    if(cryptor->op == kCCEncrypt) {
        if((retval = ccDoEnCrypt(cryptor, dataIn, dataInLength, *dataOut)) != kCCSuccess) return retval;
    } else {
        if((retval = ccDoDeCrypt(cryptor, dataIn, dataInLength, *dataOut)) != kCCSuccess) return retval;
    }

    cryptor->bytesProcessed += dataInLength;
    if(dataOutMoved) *dataOutMoved += dataInLength;
    *dataOut += dataInLength;
    *dataOutAvailable -= dataInLength;
    
    return kCCSuccess;
}


static CCCryptorStatus ccBlockUpdate(CCCryptor *cryptor, const void *dataIn, size_t dataInLength, void *dataOut, size_t *dataOutAvailable, size_t *dataOutMoved)
{
    CCCryptorStatus retval;
	uint32_t encrypting = (cryptor->op == kCCEncrypt);
    size_t blocksize = ccGetCipherBlockSize(cryptor);
    size_t reserve = ccGetReserve(cryptor);
    size_t buffsize = (reserve) ? reserve: blocksize; /* minimum buffering is a block */
    size_t dataCount = cryptor->bufferPos + dataInLength; /* Total amount of data we have "in hand" to process now. */
    size_t dataCountToHold, dataCountToProcess;
    size_t remainder, movecnt;
    
    /* This is a simple optimization */
    if(reserve == 0 && cryptor->bufferPos == 0 && (dataInLength % blocksize) == 0) { // No Padding, not buffering, even blocks
        // printf("simple processing\n");
    	return ccSimpleUpdate(cryptor, dataIn, dataInLength, &dataOut, &dataOutAvailable, dataOutMoved);
    }
    
    /* From this point on we're dealing with a Block Cipher with Block oriented I/O
     
     We always fallback to buffering once we're processing non-block aligned data.
     If the data inputs result in data becoming block aligned once again we can 
     move back to block aligned I/O - even if it's only for partial processing
     of the data supplied to this routine.
     
     */
    
    if(dataCount <= reserve) {
    	dataCountToHold = dataCount;
    } else {
    	remainder = FULLBLOCKREMAINDER(dataCount, blocksize);
		dataCountToHold = buffsize - blocksize + remainder;
        dataCountToHold = (remainder) ? dataCountToHold: reserve;
    }
    
    dataCountToProcess = dataCount - dataCountToHold;
    // printf("DataCount %d Processing %d Holding %d\n", dataCount, dataCountToProcess, dataCountToHold);
    
    if(dataCountToProcess > 0) {
    	if(cryptor->bufferPos == 0) {
        	// printf("CCCryptorUpdate checkpoint 0\n");
        	/* nothing to do yet */
    	} else if(cryptor->bufferPos < dataCountToProcess) {
        	// printf("CCCryptorUpdate checkpoint 1\n");
            movecnt = blocksize - (cryptor->bufferPos % blocksize);
            ccAddBuff(cryptor, dataIn, movecnt);
            dataIn += movecnt; dataInLength -= movecnt;
            
         	// printf("CCCryptorUpdate checkpoint 1.1 bufpos = %d\n", (int) cryptor->bufferPos);
           	if((retval = ccSimpleUpdate(cryptor, cryptor->buffptr, cryptor->bufferPos, &dataOut, &dataOutAvailable, dataOutMoved)) != kCCSuccess) {
                return retval;
        	}
			// printf("CCCryptorUpdate checkpoint 1.2\n");
            
			dataCountToProcess -= cryptor->bufferPos;
        	cryptor->bufferPos = 0;
        } else if(cryptor->bufferPos == dataCountToProcess) {
        	// printf("CCCryptorUpdate checkpoint 2\n");
			if((retval = ccSimpleUpdate(cryptor, cryptor->buffptr, cryptor->bufferPos, &dataOut, &dataOutAvailable, dataOutMoved)) != kCCSuccess) {
                return retval;
        	}
			dataCountToProcess -= cryptor->bufferPos;
        	cryptor->bufferPos = 0;
        } else /* (cryptor->bufferPos > dataCountToProcess) */ {
         	// printf("CCCryptorUpdate checkpoint 3\n");
       		if(dataCountToHold) {
            	// printf("CCCryptorUpdate bad calculation 1\n");
                return kCCDecodeError;
            }
			if((retval = ccSimpleUpdate(cryptor, cryptor->buffptr, dataCountToProcess, &dataOut, &dataOutAvailable, dataOutMoved)) != kCCSuccess) {
                return retval;
        	}
            cryptor->bufferPos = reserve - dataCountToProcess;
            memmove(cryptor->buffptr, ((uint8_t *) cryptor->buffptr)+ dataCountToProcess, cryptor->bufferPos);
            return kCCSuccess;
        }
        
        if(dataCountToProcess > 0) {
         	// printf("CCCryptorUpdate checkpoint 4\n");
   			movecnt = FULLBLOCKREMAINDER(dataCountToProcess, blocksize);
            if(movecnt) {
            	// printf("CCCryptorUpdate bad calculation 2\n");
                return kCCDecodeError;
            }
        	if((retval = ccSimpleUpdate(cryptor, dataIn, dataCountToProcess, &dataOut, &dataOutAvailable, dataOutMoved)) != kCCSuccess) return retval;
        	dataIn += dataCountToProcess; dataInLength -= dataCountToProcess;
        }
    }
    
    if(dataCountToHold) {
		// printf("CCCryptorUpdate checkpoint 1\n");
    	movecnt = dataCountToHold - cryptor->bufferPos;
        if(movecnt) {
        	if(movecnt != dataInLength) {
            	// printf("CCCryptorUpdate bad calculation 3\n");
                return kCCDecodeError;
            }
            ccAddBuff(cryptor, dataIn, movecnt);
        	dataIn += movecnt; dataInLength -= movecnt;
        }
    }
    
    if(dataInLength) {
        // printf("CCCryptorUpdate bad calculation 4\n");
        return kCCDecodeError;
    }
    return kCCSuccess;

}

CCCryptorStatus CCCryptorUpdate(CCCryptorRef cryptorRef, const void *dataIn, size_t dataInLength, void *dataOut, size_t dataOutAvailable, size_t *dataOutMoved)
{
	CCCryptorStatus     retval;
    CCCompatCryptor     *compat_cryptor = cryptorRef;
    CCCryptor           *cryptor;


    
    CC_DEBUG_LOG(ASL_LEVEL_ERR, "Entering\n");
	if(dataOutMoved) *dataOutMoved = 0;
    if(compat_cryptor == NULL)  return kCCParamError;
    cryptor = compat_cryptor->cryptor;
    
	if(dataInLength == 0) return kCCSuccess;

	if(ccIsStreaming(cryptor)) {
    	return ccSimpleUpdate(cryptor, dataIn, dataInLength, &dataOut, &dataOutAvailable, dataOutMoved);
	}
	
    retval = ccBlockUpdate(cryptor, dataIn, dataInLength, dataOut, dataOutAvailable, dataOutMoved);
        
	return retval;
}



CCCryptorStatus CCCryptorFinal(
	CCCryptorRef cryptorRef,
	void *dataOut,					/* data RETURNED here */
	size_t dataOutAvailable,
	size_t *dataOutMoved)		/* number of bytes written */
{
    CCCompatCryptor *compat_cryptor = cryptorRef;
    CC_DEBUG_LOG(ASL_LEVEL_ERR, "Entering\n");
    if(compat_cryptor == NULL)  return kCCParamError;
    CCCryptor	*cryptor = compat_cryptor->cryptor;
    if(cryptor == NULL) return kCCSuccess; // Some old behavior .. CDSA? has zapped the Cryptor.
    
    
	CCCryptorStatus	retval;
    int encrypting = (cryptor->op == kCCEncrypt);
	uint8_t *bufp = cryptor->buffptr;
	uint32_t blocksize = ccGetCipherBlockSize(cryptor);
    
    size_t moved;
	uint8_t padval;
	char tmpbuf[blocksize*2];

    
    
	if(dataOutMoved) *dataOutMoved = 0;

    if(ccIsStreaming(cryptor)) return kCCSuccess;

	if(encrypting) {
        retval = ccEncryptPad(cryptor, tmpbuf, &moved);
        if(retval != kCCSuccess) return retval;
		if(dataOutAvailable < moved) {
            return kCCBufferTooSmall;
        }
        if(dataOut) {
            CC_XMEMCPY(dataOut, tmpbuf, moved);
            if(dataOutMoved) *dataOutMoved = moved;
        }
		cryptor->bufferPos = 0;
	} else {
		if(ccGetReserve(cryptor) != 0) {
            retval = ccDecryptPad(cryptor, tmpbuf, &moved);
            if(retval != kCCSuccess) return retval;
            if(dataOutAvailable < moved) {
                return kCCBufferTooSmall;
            }
            if(dataOut) {
                CC_XMEMCPY(dataOut, tmpbuf, moved);
                if(dataOutMoved) *dataOutMoved = moved;
            }
			cryptor->bytesProcessed += *dataOutMoved;
            cryptor->bufferPos = 0;
		}
	}
	return kCCSuccess;
}

size_t CCCryptorGetOutputLength(
	CCCryptorRef cryptorRef,
	size_t inputLength,
	bool final)
{
	size_t retval;
    CCCompatCryptor *compat_cryptor = cryptorRef;
    CCCryptor	*cryptor;
	
    CC_DEBUG_LOG(ASL_LEVEL_ERR, "Entering\n");
    if(compat_cryptor == NULL)  return kCCParamError;
    cryptor = compat_cryptor->cryptor;
    
	retval = cryptor->bufferPos + inputLength;

    if(ccIsStreaming(cryptor))
    	return retval;
    
    if(cryptor->op == kCCEncrypt) {
        retval = FULLBLOCKSIZE(retval, ccGetCipherBlockSize(cryptor));        
        if(final) retval += ccGetPadlen(cryptor);
        return retval;
    } else {
        if(final) return retval;
        else return FULLBLOCKSIZE(retval, ccGetCipherBlockSize(cryptor));
    }
}

CCCryptorStatus CCCryptorReset(
	CCCryptorRef cryptorRef,
	const void *iv)
{
    CCCompatCryptor *compat_cryptor = cryptorRef;
    CCCryptor	*cryptor;
    CCCryptorStatus retval;
    
    
    CC_DEBUG_LOG(ASL_LEVEL_ERR, "Entering\n");
    if(compat_cryptor == NULL)  return kCCParamError;
    cryptor = compat_cryptor->cryptor;
    /* 
    	This routine resets all buffering and sets or clears the IV.  It is
    	documented to throw away any in-flight buffer data.
    */
    
    cryptor->bytesProcessed = cryptor->bufferPos = 0;
    
    /* 
    	Call the common routine to reset the IV - this will copy in the new 
       	value. There is now always space for an IV in the cryptor.
    */
    
    if(iv) {
        retval = ccSetIV(cryptor, iv, ccGetCipherBlockSize(cryptor));
    } else {
        uint8_t ivzero[ccGetCipherBlockSize(cryptor)];
        CC_XZEROMEM(ivzero, ccGetCipherBlockSize(cryptor));
        retval = ccSetIV(cryptor, ivzero, ccGetCipherBlockSize(cryptor));
    }
    if(retval == kCCParamError) return kCCSuccess;
    return retval;
}

CCCryptorStatus
CCCryptorGetIV(CCCryptorRef cryptorRef, void *iv)
{
    uint32_t blocklen;
    CCCompatCryptor *compat_cryptor = cryptorRef;
    CCCryptor	*cryptor;
    
    
    CC_DEBUG_LOG(ASL_LEVEL_ERR, "Entering\n");
    if(compat_cryptor == NULL)  return kCCParamError;
    cryptor = compat_cryptor->cryptor;
    
    if(ccIsStreaming(cryptor)) return kCCParamError;
    return ccGetIV(cryptor, iv, ccGetCipherBlockSize(cryptor));
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
	CCCryptorRef cryptor = NULL;
	CCCryptorStatus retval;
	size_t outputSize;
	size_t used = 0;
	size_t moved;
            
    CC_DEBUG_LOG(ASL_LEVEL_ERR, "Entering\n");
	if(retval = CCCryptorCreate(op, alg, options, key, keyLength, iv, &cryptor)) {
		return retval;
	}

	if((outputSize = CCCryptorGetOutputLength(cryptor, dataInLength, true)) > dataOutAvailable) {
		CCCryptorRelease(cryptor);
		if(dataOutMoved != NULL) *dataOutMoved = outputSize;
		return kCCBufferTooSmall;
	}

	if(retval = CCCryptorUpdate(cryptor, dataIn, dataInLength, dataOut, dataOutAvailable, &moved)) {
		CCCryptorRelease(cryptor);
		return retval;
	}
	dataOut += moved;
	used += moved;
	dataOutAvailable  -= moved;

	if(retval = CCCryptorFinal(cryptor, dataOut, dataOutAvailable, &moved)) {
		CC_DEBUG_LOG(ASL_LEVEL_ERR, "Final Error\n", 0);
        // printf("Failing on final\n");
	} else {
		used += moved;
		if(dataOutMoved != NULL) *dataOutMoved = used;
	}

	CCCryptorRelease(cryptor);
	return retval;
}

CCCryptorStatus CCCryptorEncryptDataBlock(
	CCCryptorRef cryptorRef,
	const void *iv,
	const void *dataIn,
	size_t dataInLength,
	void *dataOut)
{
    CCCompatCryptor *compat_cryptor = cryptorRef;
    CCCryptor	*cryptor;
    
    CC_DEBUG_LOG(ASL_LEVEL_ERR, "Entering\n");
    if(compat_cryptor == NULL)  return kCCParamError;
    cryptor = compat_cryptor->cryptor;
    
    if(ccIsStreaming(cryptor)) return kCCParamError;
    if(!iv) return ccDoEnCrypt(cryptor, dataIn, dataInLength, dataOut);
    return ccDoEnCryptTweaked(cryptor, dataIn, dataInLength, dataOut, iv);    
}


CCCryptorStatus CCCryptorDecryptDataBlock(
	CCCryptorRef cryptorRef,
	const void *iv,
	const void *dataIn,
	size_t dataInLength,
	void *dataOut)
{
    CCCompatCryptor *compat_cryptor = cryptorRef;
    CCCryptor	*cryptor;
    
    CC_DEBUG_LOG(ASL_LEVEL_ERR, "Entering\n");
    if(compat_cryptor == NULL)  return kCCParamError;
    cryptor = compat_cryptor->cryptor;
    
    if(ccIsStreaming(cryptor)) return kCCParamError;
    if(!iv) return ccDoDeCrypt(cryptor, dataIn, dataInLength, dataOut);
    return ccDoDeCryptTweaked(cryptor, dataIn, dataInLength, dataOut, iv);    
}


CCCryptorStatus CCDesIsWeakKey( void *key, size_t length)
{
    CC_DEBUG_LOG(ASL_LEVEL_ERR, "Entering\n");
    return ccdes_key_is_weak(key, length);
}

void CCDesSetOddParity(void *key, size_t Length)
{
    CC_DEBUG_LOG(ASL_LEVEL_ERR, "Entering\n");
    ccdes_key_set_odd_parity(key, Length);
}

uint32_t CCDesCBCCksum(void *in, void *out, size_t length,
                       void *key, size_t keylen, void *ivec)
{
    CC_DEBUG_LOG(ASL_LEVEL_ERR, "Entering\n");
    return ccdes_cbc_cksum(in, out, length, key, keylen, ivec);
}

// Legacy SPI
#include <corecrypto/ccrc4.h>

typedef struct current_rc4_key_st
{
	uint32_t x,y;
	uint32_t data[256];
} RC4_KEY;

#ifndef	NDEBUG
#define ASSERT(s)
#else
#define ASSERT(s)	assert(s)
#endif

void CC_RC4_set_key(void *ctx, int len, const unsigned char *data)
{
    CC_DEBUG_LOG(ASL_LEVEL_ERR, "Entering\n");
    ASSERT(sizeof(RC4_KEY) == ccrc4_eay.size);
    ccrc4_eay.init(ctx, len, data);
}

void CC_RC4(void *ctx, unsigned long len, const unsigned char *indata,
            unsigned char *outdata)
{
    CC_DEBUG_LOG(ASL_LEVEL_ERR, "Entering\n");
    ccrc4_eay.crypt(ctx, len, indata, outdata);
}



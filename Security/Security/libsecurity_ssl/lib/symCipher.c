/*
 * Copyright (c) 1999-2001,2005-2008,2010-2014 Apple Inc. All Rights Reserved.
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
 * symCipher.c - CommonCrypto-based symmetric cipher module
 */

/* THIS FILE CONTAINS KERNEL CODE */

#include "sslBuildFlags.h"
#include "sslDebug.h"
#include "sslMemory.h"
#include "symCipher.h"
#include "cipherSpecs.h"
#include "SSLRecordInternal.h"

#ifdef KERNEL

#include <corecrypto/ccaes.h>
#include <corecrypto/ccdes.h>
#include <corecrypto/ccmode.h>

#include <AssertMacros.h>

struct SymCipherContext {
    const struct ccmode_cbc *cbc;
    cccbc_ctx u[]; /* this will have the key and iv */
};

/* Macros for accessing the content of a SymCipherContext */
 
/*
 SymCipherContext looks like this in memory:
 
 {
    const struct ccmode_cbc *cbc;
    cccbc_ctx key[n];
    cccbc_iv  iv[m];
 }
 
 cccbc_ctx and cccbc_iv are typedef-ined as aligned opaque struct, the actual contexts are arrays
 of those types normally declared with a cc_ctx_decl macro.
 The cc_ctx_n macros gives the number of elements in those arrays that are needed to store the
 contexts.
 The size of the context depends on the actual cbc implementation used.
*/


/* CTX_SIZE: Total size of the SymCipherContext struct for a cbc implementation */
static inline
size_t CTX_SIZE(const struct ccmode_cbc *cbc)
{
#ifdef __CC_HAS_FIX_FOR_11468135__
    return (sizeof(SymCipherContext) + (sizeof(cccbc_ctx) * (cc_ctx_n(cccbc_ctx, cbc->size) + cc_ctx_n(cccbc_iv, cbc->block_size))));
#else
    /* This is approximate, but will work in that case, this code will go away after we transition */
    return (sizeof(SymCipherContext) + sizeof(cccbc_ctx) + cbc->size);
#endif
}

/* CTX_KEY: Address of the key context in the SymCipherContext struct */
static inline
cccbc_ctx *CTX_KEY(struct SymCipherContext *ctx)
{
    return &ctx->u[0];
}


/* CTX_IV: Address of the iv context in the SymCipherContext struct */
#ifdef __CC_HAS_FIX_FOR_11468135__
static inline
cccbc_iv *CTX_IV(struct SymCipherContext *ctx)
{
    return (cccbc_iv *)&ctx->u[cc_ctx_n(cccbc_ctx, ctx->cbc->size)];
}
#endif

static
const void *ccmode(SSL_CipherAlgorithm alg, int enc)
{
    switch(alg) {
        case SSL_CipherAlgorithmAES_128_CBC:
        case SSL_CipherAlgorithmAES_256_CBC:
            return enc?ccaes_cbc_encrypt_mode():ccaes_cbc_decrypt_mode();
        case SSL_CipherAlgorithm3DES_CBC:
            return enc?ccdes3_cbc_encrypt_mode():ccdes3_cbc_decrypt_mode();
        case SSL_CipherAlgorithmAES_128_GCM:
        case SSL_CipherAlgorithmAES_256_GCM:
        case SSL_CipherAlgorithmRC4_128:
            /* TODO: we should do RC4 for TLS, but we dont need it for DTLS */
        default:
            check(0);
            return NULL; /* This will cause CCCryptorCreate to return an error */
    }
}

static
int CCSymmInit(
    const SSLSymmetricCipherParams *params,
    int encrypting,
    uint8_t *key,
    uint8_t* iv,
    SymCipherContext *cipherCtx)
{
    check(cipherCtx!=NULL);
    check(params);
    SymCipherContext ctx = *cipherCtx;

	/*
	 * Cook up a cccbx_ctx object. Assumes:
	 * 		cipherCtx->symCipher.keyAlg
	 *		cipherCtx->encrypting
	 * 		key (raw key bytes)
	 *		iv (raw bytes)
	 * On successful exit:
	 * 		Resulting ccmode --> cipherCtx
	 */

    /* FIXME: this should not be needed as long as CCSymFinish is called */
	if(ctx) {
        sslFree(ctx);
        ctx = NULL;
	}

    const struct ccmode_cbc *cbc = ccmode(params->keyAlg, encrypting);

    ctx = sslMalloc(CTX_SIZE(cbc));

    if(ctx==NULL) {
        sslErrorLog("CCSymmInit: Can't allocate context\n");
        return errSSLRecordInternal;
    }

    ctx->cbc = cbc;

#ifdef __CC_HAS_FIX_FOR_11468135__
    cccbc_init(cbc, CTX_KEY(ctx), params->keySize, key);
    cccbc_set_iv(cbc, CTX_IV(ctx), iv);
#else
    cccbc_init(cbc, CTX_KEY(ctx), params->keySize, key, iv);
#endif

    *cipherCtx = ctx;
	return 0;
}

/* same for en/decrypt */
static
int CCSymmEncryptDecrypt(
                         const uint8_t *src,
                         uint8_t *dest,
                         size_t len,
                         SymCipherContext cipherCtx)
{

	ASSERT(cipherCtx != NULL);
    ASSERT(cipherCtx->cbc != NULL);

	if(cipherCtx == NULL || cipherCtx->cbc == NULL) {
		sslErrorLog("CCSymmEncryptDecrypt: NULL cipherCtx\n");
		return errSSLRecordInternal;
	}

    ASSERT((len%cipherCtx->cbc->block_size)==0);

    if(len%cipherCtx->cbc->block_size) {
        sslErrorLog("CCSymmEncryptDecrypt: Invalid size\n");
        return errSSLRecordInternal;
    }

    unsigned long nblocks = len/cipherCtx->cbc->block_size;

#ifdef __CC_HAS_FIX_FOR_11468135__
    cccbc_update(cipherCtx->cbc, CTX_KEY(cipherCtx), CTX_IV(cipherCtx), nblocks, src, dest);
#else
    cipherCtx->cbc->cbc(CTX_KEY(cipherCtx), nblocks, src, dest);
#endif
	return 0;
}

static
int CCSymmFinish(
                 SymCipherContext cipherCtx)
{
	if(cipherCtx) {
        sslFree(cipherCtx);
	}
	return 0;
}


#else

#define ENABLE_RC4      1
#define ENABLE_3DES     1
#define ENABLE_AES      1
#define ENABLE_AES256   1

/*
 * CommonCrypto-based symmetric cipher callouts
 */
#include <CommonCrypto/CommonCryptor.h>
#include <CommonCrypto/CommonCryptorSPI.h>
#include <assert.h>

static
CCAlgorithm CCAlg(SSL_CipherAlgorithm alg)
{
    switch(alg) {
        case SSL_CipherAlgorithmAES_128_CBC:
        case SSL_CipherAlgorithmAES_256_CBC:
        case SSL_CipherAlgorithmAES_128_GCM:
        case SSL_CipherAlgorithmAES_256_GCM:
            return kCCAlgorithmAES128;          /* AES128 here means 128bit block size, not key size */
        case SSL_CipherAlgorithm3DES_CBC:
            return kCCAlgorithm3DES;
        case SSL_CipherAlgorithmDES_CBC:
            return kCCAlgorithmDES;
        case SSL_CipherAlgorithmRC4_128:
            return kCCAlgorithmRC4;
        case SSL_CipherAlgorithmRC2_128:
            return kCCAlgorithmRC2;
        default:
            assert(0);
            return (CCAlgorithm)(-1); /* This will cause CCCryptorCreate to return an error */
    }
}

static CCOptions CCOpt(CipherType cipherType)
{
#if 0
    if(cipherType==aeadCipherType) return kCCModeGCM;
#endif
    return 0;
}

static
int CCSymmInit(
    const SSLSymmetricCipherParams *params,
    int encrypting,
	uint8_t *key, 
	uint8_t* iv, 
	SymCipherContext *cipherCtx)
{
    assert(cipherCtx!=NULL);

	/*
	 * Cook up a CCCryptorRef. Assumes:
	 * 		cipherCtx->symCipher.keyAlg
	 *		cipherCtx->encrypting
	 * 		key (raw key bytes)
	 *		iv (raw bytes)
	 * On successful exit:
	 * 		Resulting CCCryptorRef --> cipherCtx->cryptorRef
	 */
	CCCryptorStatus ccrtn;
	CCOperation op = encrypting ? kCCEncrypt : kCCDecrypt;
    CCCryptorRef cryptorRef = (CCCryptorRef)*cipherCtx;

    /* FIXME: this should not be needed as long as CCSymFinish is called */
	if(cryptorRef) {
		CCCryptorRelease(cryptorRef);
		cryptorRef = NULL;
	}

	ccrtn = CCCryptorCreate(op, CCAlg(params->keyAlg),
        /* options - gcm or no padding, default CBC */
        CCOpt(params->cipherType),
		key, params->keySize,
		iv,
		&cryptorRef);
	if(ccrtn) {
		sslErrorLog("CCCryptorCreate returned %d\n", (int)ccrtn);
		return errSSLRecordInternal;
	}
    *cipherCtx = (SymCipherContext)cryptorRef;
	return 0;
}

/* same for en/decrypt */
static
int CCSymmEncryptDecrypt(
                              const uint8_t *src,
                              uint8_t *dest,
                              size_t len,
                              SymCipherContext cipherCtx)
{
	CCCryptorStatus ccrtn;
	CCCryptorRef cryptorRef = (CCCryptorRef)cipherCtx;
	ASSERT(cryptorRef != NULL);
	if(cryptorRef == NULL) {
		sslErrorLog("CCSymmEncryptDecrypt: NULL cryptorRef\n");
		return errSSLRecordInternal;
	}
    size_t data_moved;
	ccrtn = CCCryptorUpdate(cryptorRef, src, len,
                            dest, len, &data_moved);
    assert(data_moved == len);
#if SSL_DEBUG
	if(ccrtn) {
		sslErrorLog("CCSymmEncryptDecrypt: returned %d\n", (int)ccrtn);
		return errSSLRecordInternal;
	}
#endif
	return 0;
}

#if ENABLE_AES_GCM

/* same for en/decrypt */
static
int CCSymmAEADSetIV(
    const uint8_t *iv,
    size_t len,
    SymCipherContext cipherCtx)
{
	CCCryptorStatus ccrtn;
	CCCryptorRef cryptorRef = (CCCryptorRef)cipherCtx;

	ASSERT(cryptorRef != NULL);
	if(cryptorRef == NULL) {
		sslErrorLog("CCSymmAEADAddIV: NULL cryptorRef\n");
		return errSecInternalComponent;
	}
	ccrtn = CCCryptorGCMAddIV(cryptorRef, iv, len);
#if SSL_DEBUG
	if(ccrtn) {
		sslErrorLog("CCSymmAEADAddIV: returned %d\n", (int)ccrtn);
		return errSSLRecordInternal;
	}
#endif
	return 0;
}

/* same for en/decrypt */
static
int CCSymmAddADD(
    const uint8_t *src,
    size_t len,
    SymCipherContext cipherCtx)
{
	CCCryptorStatus ccrtn;
	CCCryptorRef cryptorRef = (CCCryptorRef)cipherCtx;

	ASSERT(cryptorRef != NULL);
	if(cryptorRef == NULL) {
		sslErrorLog("CCSymmAddADD: NULL cryptorRef\n");
		return errSSLRecordInternal;
	}
	ccrtn = CCCryptorGCMAddADD(cryptorRef, src, len);
#if SSL_DEBUG
	if(ccrtn) {
		sslErrorLog("CCSymmAddADD: returned %d\n", (int)ccrtn);
		return errSSLRecordInternal;
	}
#endif
	return 0;
}

static
int CCSymmAEADEncrypt(
    const uint8_t *src,
    uint8_t *dest,
    size_t len,
    SymCipherContext cipherCtx)
{
	CCCryptorStatus ccrtn;
	CCCryptorRef cryptorRef = (CCCryptorRef)cipherCtx;

	ASSERT(cryptorRef != NULL);
	if(cryptorRef == NULL) {
		sslErrorLog("CCSymmAEADEncrypt: NULL cryptorRef\n");
		return errSSLRecordInternal;
	}
	ccrtn = CCCryptorGCMEncrypt(cryptorRef, src, len, dest);
#if SSL_DEBUG
	if(ccrtn) {
		sslErrorLog("CCSymmAEADEncrypt: returned %d\n", (int)ccrtn);
		return errSSLRecordInternal;
	}
#endif
	return 0;
}


static
int CCSymmAEADDecrypt(
    const uint8_t *src,
    uint8_t *dest,
    size_t len,
    SymCipherContext cipherCtx)
{
	CCCryptorStatus ccrtn;
	CCCryptorRef cryptorRef = (CCCryptorRef)cipherCtx;

	ASSERT(cipherCtx != NULL);
	ASSERT(cipherCtx->cryptorRef != NULL);
	if(cipherCtx->cryptorRef == NULL) {
		sslErrorLog("CCSymmAEADDecrypt: NULL cryptorRef\n");
		return errSSLRecordInternal;
	}
	ccrtn = CCCryptorGCMDecrypt(cryptorRef, src, len, dest);
#if SSL_DEBUG
	if(ccrtn) {
		sslErrorLog("CCSymmAEADDecrypt: returned %d\n", (int)ccrtn);
		return errSSLRecordInternal;
	}
#endif
	return 0;
}


static
int CCSymmAEADDone(
    uint8_t *mac,
    size_t *macLen,
    SymCipherContext cipherCtx)
{
	CCCryptorStatus ccrtn;
	CCCryptorRef cryptorRef = (CCCryptorRef)cipherCtx;

	ASSERT(cipherCtx != NULL);
	ASSERT(cipherCtx->cryptorRef != NULL);
	if(cipherCtx->cryptorRef == NULL) {
		sslErrorLog("CCSymmAEADDone: NULL cryptorRef\n");
		return errSSLRecordInternal;
	}
	ccrtn = CCCryptorGCMFinal(cipherCtx->cryptorRef, mac, macLen);
    CCCryptorStatus ccrtn2 = CCCryptorGCMReset(cipherCtx->cryptorRef);
    if (ccrtn == kCCSuccess)
        ccrtn = ccrtn2;
#if SSL_DEBUG
	if(ccrtn) {
		sslErrorLog("CCSymmAEADDone: returned %d\n", (int)ccrtn);
		return errSSLRecordInternal;
	}
#endif
	return 0;
}
#endif

static
int CCSymmFinish(
	SymCipherContext cipherCtx)
{
	CCCryptorRef cryptorRef = (CCCryptorRef)cipherCtx;

	if(cryptorRef) {
		CCCryptorRelease(cryptorRef);
	}
	return 0;
}

#endif /* KERNEL */

#if	ENABLE_DES
const SSLSymmetricCipher SSLCipherDES_CBC = {
    .params = &SSLCipherDES_CBCParams,
    .c.cipher = {
        .initialize = CCSymmInit,
        .encrypt = CCSymmEncryptDecrypt,
        .decrypt = CCSymmEncryptDecrypt
    },
    .finish = CCSymmFinish
};
#endif	/* ENABLE_DES */

#if	ENABLE_3DES
const SSLSymmetricCipher SSLCipher3DES_CBC = {
    .params = &SSLCipher3DES_CBCParams,
    .c.cipher = {
        .initialize = CCSymmInit,
        .encrypt = CCSymmEncryptDecrypt,
        .decrypt = CCSymmEncryptDecrypt
    },
    .finish = CCSymmFinish
};
#endif	/* ENABLE_3DES */

#if		ENABLE_RC4
const SSLSymmetricCipher SSLCipherRC4_128 = {
    .params = &SSLCipherRC4_128Params,
    .c.cipher = {
        .initialize = CCSymmInit,
        .encrypt = CCSymmEncryptDecrypt,
        .decrypt = CCSymmEncryptDecrypt
    },
    .finish = CCSymmFinish
};
#endif	/* ENABLE_RC4 */

#if		ENABLE_RC2
const SSLSymmetricCipher SSLCipherRC2_128 = {
    .params = &SSLCipherRC2_128Params,
    .c.cipher = {
        .initialize = CCSymmInit,
        .encrypt = CCSymmEncryptDecrypt,
        .decrypt = CCSymmEncryptDecrypt
    },
    .finish = CCSymmFinish
};
#endif	/* ENABLE_RC2*/

#if		ENABLE_AES
const SSLSymmetricCipher SSLCipherAES_128_CBC = {
    .params = &SSLCipherAES_128_CBCParams,
    .c.cipher = {
        .initialize = CCSymmInit,
        .encrypt = CCSymmEncryptDecrypt,
        .decrypt = CCSymmEncryptDecrypt
    },
    .finish = CCSymmFinish
};
#endif	/* ENABLE_AES */

#if ENABLE_AES256
const SSLSymmetricCipher SSLCipherAES_256_CBC = {
    .params = &SSLCipherAES_256_CBCParams,
    .c.cipher = {
        .initialize = CCSymmInit,
        .encrypt = CCSymmEncryptDecrypt,
        .decrypt = CCSymmEncryptDecrypt
    },
    .finish = CCSymmFinish
};
#endif /* ENABLE_AES256 */

#if ENABLE_AES_GCM
const SSLSymmetricCipher SSLCipherAES_128_GCM = {
    .params = &SSLCipherAES_128_GCMParams,
    .c.aead = {
        .initialize = CCSymmInit,
        .setIV = CCSymmAEADSetIV,
        .update = CCSymmAddADD,
        .encrypt = CCSymmAEADEncrypt,
        .decrypt = CCSymmAEADDecrypt,
        .done =  CCSymmAEADDone
    },
    .finish = CCSymmFinish
};

const SSLSymmetricCipher SSLCipherAES_256_GCM = {
    .params = &SSLCipherAES_256_GCMParams,
    .c.aead = {
        .initialize = CCSymmInit,
        .setIV = CCSymmAEADSetIV,
        .update = CCSymmAddADD,
        .encrypt = CCSymmAEADEncrypt,
        .decrypt = CCSymmAEADDecrypt,
        .done =  CCSymmAEADDone
    },
    .finish = CCSymmFinish
};
#endif /* ENABLE_AES_GCM */

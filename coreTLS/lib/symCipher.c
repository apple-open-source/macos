/*
 * Copyright (c) 1999-2001,2005-2008,2010-2012 Apple Inc. All Rights Reserved.
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
 * symCipher.c - corecrypto-based symmetric cipher module
 */

/* THIS FILE CONTAINS KERNEL CODE */

#include <tls_ciphersuites.h>

#include "sslBuildFlags.h"
#include "sslDebug.h"
#include "sslMemory.h"
#include "symCipher.h"
#include "tls_types_private.h"

#include <corecrypto/ccaes.h>
#include <corecrypto/ccdes.h>
#include <corecrypto/ccmode.h>

#include <AssertMacros.h>

struct SymCipherContext {
    const struct ccmode_cbc *cbc;
    cccbc_ctx u[]; /* this will have the key and iv */
};

/* Macros for accessing the content of a struct SymCipherContext */
 
/*
 struct SymCipherContext looks like this in memory:
 
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
    return (sizeof(struct SymCipherContext) + (sizeof(cccbc_ctx) * (cc_ctx_n(cccbc_ctx, cbc->size) + cc_ctx_n(cccbc_iv, cbc->block_size))));
}

/* CTX_KEY: Address of the key context in the SymCipherContext struct */
static inline
cccbc_ctx *CTX_KEY(struct SymCipherContext *ctx)
{
    return &ctx->u[0];
}

/* CTX_IV: Address of the iv context in the SymCipherContext struct */
static inline
cccbc_iv *CTX_IV(struct SymCipherContext *ctx)
{
    return (cccbc_iv *)&ctx->u[cc_ctx_n(cccbc_ctx, ctx->cbc->size)];
}

static
const void *ccmode(SSL_CipherAlgorithm alg, int enc)
{
    switch(alg) {
        case SSL_CipherAlgorithmAES_128_CBC:
        case SSL_CipherAlgorithmAES_256_CBC:
            return enc?ccaes_cbc_encrypt_mode():ccaes_cbc_decrypt_mode();
        case SSL_CipherAlgorithm3DES_CBC:
            return enc?ccdes3_cbc_encrypt_mode():ccdes3_cbc_decrypt_mode();
        case SSL_CipherAlgorithmRC4_128:
            /* TODO: we should do RC4 for TLS, but we dont need it for DTLS */
        case SSL_CipherAlgorithmAES_128_GCM:
        case SSL_CipherAlgorithmAES_256_GCM:
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
    struct ccrng_state *rng,
    SymCipherContext *cipherCtx)
{
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
	if (ctx) {
        sslFree(ctx);
        ctx = NULL;
	}

    const struct ccmode_cbc *cbc = ccmode(params->keyAlg, encrypting);

    ctx = sslMalloc(CTX_SIZE(cbc));

    if (ctx == NULL) {
        sslErrorLog("CCSymmInit: Can't allocate context\n");
        return errSSLRecordInternal;
    }

    ctx->cbc = cbc;

    int error = 0;
    require_noerr((error = cccbc_init(cbc, CTX_KEY(ctx), params->keySize, key)), cleanup);
    require_noerr((error = cccbc_set_iv(cbc, CTX_IV(ctx), iv)), cleanup);

    *cipherCtx = ctx;
	return 0;

cleanup:
    sslFree(ctx);
    return error;
}

/* same for en/decrypt */
static
int CCSymmEncryptDecrypt(
                         const uint8_t *src,
                         uint8_t *dest,
                         size_t len,
                         SymCipherContext cipherCtx)
{
    if (cipherCtx == NULL || cipherCtx->cbc == NULL) {
        sslErrorLog("CCSymmEncryptDecrypt: NULL cipherCtx\n");
        return errSSLRecordInternal;
    }

    ASSERT((len%cipherCtx->cbc->block_size)==0);

    if (len%cipherCtx->cbc->block_size) {
        sslErrorLog("CCSymmEncryptDecrypt: Invalid size\n");
        return errSSLRecordInternal;
    }

    unsigned long nblocks = len/cipherCtx->cbc->block_size;

    return cccbc_update(cipherCtx->cbc, CTX_KEY(cipherCtx), CTX_IV(cipherCtx), nblocks, src, dest);
}

static
int CCSymmFinish(
                 SymCipherContext cipherCtx)
{
	if (cipherCtx) {
        cc_clear(CTX_SIZE(cipherCtx->cbc), cipherCtx);
        sslFree(cipherCtx);
	}
	return 0;
}

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


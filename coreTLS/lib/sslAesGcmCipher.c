/*
 * Copyright (c) 2014 Apple Inc. All Rights Reserved.
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

/* THIS FILE CONTAINS KERNEL CODE */
#include <tls_ciphersuites.h>

#include "sslBuildFlags.h"
#include "sslDebug.h"
#include "sslMemory.h"
#include "symCipher.h"
#include "tls_types_private.h"

#include <string.h>
#include <AssertMacros.h>
#include <stddef.h>

#include <corecrypto/ccaes.h>
#include <corecrypto/ccmode.h>
#include <corecrypto/ccrng.h>

struct SymCipherContext {
    const struct ccmode_gcm *gcm;
    ccgcm_ctx *gcmCtx;
    uint8_t gcmIV[TLS_AES_GCM_IMPLICIT_IV_SIZE+TLS_AES_GCM_EXPLICIT_IV_SIZE];
    uint8_t *key;
    size_t keySize;
};

static
int CCGCMSymmInit(const SSLSymmetricCipherParams *params,
                  int encrypting,
                  uint8_t *key,
                  uint8_t* iv,
                  struct ccrng_state *rng,
                  SymCipherContext *cipherCtx)
{
    SymCipherContext ctx = *cipherCtx;

    /* FIXME: this should not be needed as long as CCSymFinish is called */
    if (ctx) {
        sslFree(ctx);
        ctx = NULL;
    }

    const struct ccmode_gcm *gcm = encrypting ? ccaes_gcm_encrypt_mode() : ccaes_gcm_decrypt_mode();
    ctx = sslMalloc(sizeof(struct SymCipherContext));

    if (ctx == NULL) {
        sslErrorLog("CCSymmInit: Can't allocate context\n");
        return errSSLRecordInternal;
    }

    ctx->gcm = gcm;
    ctx->gcmCtx = sslMalloc(gcm->size);

    // Generate the initial value for the implicit and explicit nonces.
    // The initial nonce is the 4 byte packet counter and the explicit nonce
    // is a 8 byte random value.
    memcpy(ctx->gcmIV, iv, TLS_AES_GCM_IMPLICIT_IV_SIZE);
    ccrng_generate(rng, TLS_AES_GCM_EXPLICIT_IV_SIZE, &ctx->gcmIV[TLS_AES_GCM_IMPLICIT_IV_SIZE]);

    // We may need to re-initialize the cipher later on (if the explicit IV changes). So, for now, we
    // have to save the key and defer initialization until later on.
    ctx->keySize = params->keySize;
    ctx->key = sslMalloc(ctx->keySize);
    memcpy(ctx->key, key, ctx->keySize);

    *cipherCtx = ctx;
    return 0;
}

static
int CCSymmAEADSetIV(const uint8_t *srcIV,
                    SymCipherContext cipherCtx)
{
    int err = 0;

    if (cipherCtx == NULL || cipherCtx->gcm == NULL) {
        sslErrorLog("CCSymmAEADSetIV: NULL cipherCtx\n");
        return errSSLRecordInternal;
    }

    /* Save the explicit IV in our local buffer and then finish initializing the cipher, if needed. */
    memcpy(&cipherCtx->gcmIV[TLS_AES_GCM_IMPLICIT_IV_SIZE], srcIV, TLS_AES_GCM_EXPLICIT_IV_SIZE);
    err = ccgcm_init_with_iv(cipherCtx->gcm, cipherCtx->gcmCtx, cipherCtx->keySize, cipherCtx->key, cipherCtx->gcmIV);

    return err;
}

static
int CCSymmAEADGetIV(uint8_t *destIV,
                    SymCipherContext cipherCtx)
{
    if (cipherCtx == NULL || cipherCtx->gcm == NULL) {
        sslErrorLog("CCSymmAEADSetIV: NULL cipherCtx\n");
        return errSSLRecordInternal;
    }

    /* copy the explicit IV */
    memcpy(destIV, &cipherCtx->gcmIV[TLS_AES_GCM_IMPLICIT_IV_SIZE], TLS_AES_GCM_EXPLICIT_IV_SIZE);

    return 0;
}

static
int CCSymmAEADEncrypt(const uint8_t *src,
                      uint8_t *dest,
                      size_t len,
                      SymCipherContext cipherCtx)
{
    int err = 0;

    if (cipherCtx == NULL || cipherCtx->gcm == NULL) {
        sslErrorLog("CCSymmAEADEncrypt: NULL cipherCtx\n");
        return errSSLRecordInternal;
    }

    err = ccgcm_update(cipherCtx->gcm, cipherCtx->gcmCtx, len-(TLS_AES_GCM_EXPLICIT_IV_SIZE+TLS_AES_GCM_TAG_SIZE),
                       src+TLS_AES_GCM_EXPLICIT_IV_SIZE, dest+TLS_AES_GCM_EXPLICIT_IV_SIZE);
    err |= ccgcm_finalize(cipherCtx->gcm, cipherCtx->gcmCtx, TLS_AES_GCM_TAG_SIZE, &dest[len-TLS_AES_GCM_TAG_SIZE]);

    /* Reset the GCM for another operation and then bump up the IV */
    err |= ccgcm_reset(cipherCtx->gcm, cipherCtx->gcmCtx);
    err |= ccgcm_inc_iv(cipherCtx->gcm, cipherCtx->gcmCtx, cipherCtx->gcmIV);

    return err;
}

static
int CCSymmAEADDecrypt(const uint8_t *src,
                      uint8_t *dest,
                      size_t len,
                      SymCipherContext cipherCtx)
{
    int err = 0;

    if (cipherCtx == NULL || cipherCtx->gcm == NULL) {
        printf("CCSymmAEADDecrypt: NULL cipherCtx\n");
        return -1;
    }

    /* Make a temporary copy of the tag to avoid breaking the const signature */
    uint8_t tagCopy[TLS_AES_GCM_TAG_SIZE];
    memcpy(tagCopy, &src[len-TLS_AES_GCM_TAG_SIZE], TLS_AES_GCM_TAG_SIZE);

    err = ccgcm_update(cipherCtx->gcm, cipherCtx->gcmCtx, len-TLS_AES_GCM_TAG_SIZE, src, dest);
    err |= ccgcm_finalize(cipherCtx->gcm, cipherCtx->gcmCtx, TLS_AES_GCM_TAG_SIZE, tagCopy);

    /* Reset the GCM for another operation and then bump up the IV */
    err |= ccgcm_reset(cipherCtx->gcm, cipherCtx->gcmCtx);
    err |= ccgcm_inc_iv(cipherCtx->gcm, cipherCtx->gcmCtx, cipherCtx->gcmIV);

    /* If an error occurred then scrub the plaintext destination */
    if (err != 0) {
        cc_clear(len-TLS_AES_GCM_TAG_SIZE, dest);
    }

    return err;
}

static
int CCSymmAddADD(const uint8_t *src,
                 size_t  len,
                 SymCipherContext cipherCtx)
{
    if (cipherCtx == NULL || cipherCtx->gcm == NULL) {
        sslErrorLog("CCSymmAddADD: NULL cipherCtx\n");
        return errSSLRecordInternal;
    }

    return ccgcm_gmac(cipherCtx->gcm, cipherCtx->gcmCtx, len, src);
}

static
int CCSymmFinish(SymCipherContext cipherCtx)
{
    if (cipherCtx) {
        ccgcm_ctx_clear(cipherCtx->gcm->size, cipherCtx->gcmCtx);
        sslFree(cipherCtx->gcmCtx);

        // Scrub the key from memory
        cc_clear(cipherCtx->keySize, cipherCtx->key);
        sslFree(cipherCtx->key);

        sslFree(cipherCtx);
    }
    return 0;
}

const SSLSymmetricCipher SSLCipherAES_128_GCM = {
    .params = &SSLCipherAES_128_GCMParams,
    .c.aead = {
        .initialize = CCGCMSymmInit,
        .setIV = CCSymmAEADSetIV,
        .getIV = CCSymmAEADGetIV,
        .update = CCSymmAddADD,
        .encrypt = CCSymmAEADEncrypt,
        .decrypt = CCSymmAEADDecrypt,
    },
    .finish = CCSymmFinish
};

const SSLSymmetricCipher SSLCipherAES_256_GCM = {
    .params = &SSLCipherAES_256_GCMParams,
    .c.aead = {
        .initialize = CCGCMSymmInit,
        .setIV = CCSymmAEADSetIV,
        .getIV = CCSymmAEADGetIV,
        .update = CCSymmAddADD,
        .encrypt = CCSymmAEADEncrypt,
        .decrypt = CCSymmAEADDecrypt,
    },
    .finish = CCSymmFinish
};

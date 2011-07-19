/*
 * Copyright (c) 1995 - 2010 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 *
 * Portions Copyright (c) 2010 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <config.h>

#ifndef HAVE_CCDIGESTCREATE

#define HC_DEPRECATED
#define HC_DEPRECATED_CRYPTO

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <krb5-types.h>

#if __APPLE_TARGET_EMBEDDED__

#include <CommonCrypto/CommonDigest.h>
#include <CommonCrypto/CommonCryptor.h>
#include "CCDGlue.h"

#else

#include "sha.h"
#include "md5.h"
#include "md4.h"
#include "md2.h"

#include "CCDGlue.h"

#endif

struct hc_md_ctx;

typedef int (*hc_cc_md_init)(struct hc_md_ctx *);
typedef int (*hc_cc_md_update)(struct hc_md_ctx *,const void *, size_t);
typedef int (*hc_cc_md_final)(void *, struct hc_md_ctx *);
typedef int (*hc_cc_md_cleanup)(struct hc_md_ctx *);

struct CCDigest_s {
    int hash_size;
    int block_size;
    int ctx_size;
    hc_cc_md_init init;
    hc_cc_md_update update;
    hc_cc_md_final final;
    hc_cc_md_cleanup cleanup;
};

#define HC_CC_MD(name, ctxname, outputsize, blocksize)	\
const struct CCDigest_s hc_kCCDigest##name##_s = {	\
	outputsize,					\
	blocksize,					\
	sizeof(CC_##ctxname##_CTX),			\
	(hc_cc_md_init)CC_##name##_Init,		\
	(hc_cc_md_update)CC_##name##_Update,		\
	(hc_cc_md_final)CC_##name##_Final,		\
	NULL						\
    }

HC_CC_MD(MD2, MD2, 16, 16);
HC_CC_MD(MD4, MD4, 16, 64);
HC_CC_MD(MD5, MD5, 16, 64);
HC_CC_MD(SHA1, SHA1, 20, 64);
HC_CC_MD(SHA256, SHA256, 32, 64);
#if 0
HC_CC_MD(SHA384, SHA384, 48, 128);
HC_CC_MD(SHA512, SHA512, 32, 128);
#endif

int
CCDigest(CCDigestAlg algorithm, const void *data, size_t length, void *output)
{
    CCDigestRef d = CCDigestCreate(algorithm);
    if (d == NULL)
	return ENOMEM;
    CCDigestUpdate(d, data, length);
    CCDigestFinal(d, output);
    CCDigestDestroy(d);

    return 0;
}

struct CCDigestCtx_s {
    CCDigestAlg alg;
};


CCDigestRef
CCDigestCreate(CCDigestAlg alg)
{
    CCDigestRef ctx = calloc(1, sizeof(*ctx) + alg->ctx_size);
    if (ctx == NULL)
	return NULL;

    ctx->alg = alg;
    ctx->alg->init((struct hc_md_ctx *)&ctx[1]);

    return ctx;
    
}

int
CCDigestUpdate(CCDigestRef ctx, const void *data, size_t length)
{
    ctx->alg->update((struct hc_md_ctx *)&ctx[1], data, length);
    return 0;
}

int
CCDigestFinal(CCDigestRef ctx, void *output)
{
    ctx->alg->final(output, (struct hc_md_ctx *)&ctx[1]);
    return 0;
}

void
CCDigestDestroy(CCDigestRef ctx)
{
    CCDigestAlg alg = ctx->alg;
    memset(ctx, 0, sizeof(*ctx) + alg->ctx_size);
    free(ctx);
}

void
CCDigestReset(CCDigestRef ctx)
{ 
    ctx->alg->init((struct hc_md_ctx *)&ctx[1]);
}

size_t
CCDigestBlockSize(CCDigestRef ctx)
{
    return ctx->alg->block_size;
}

size_t
CCDigestOutputSize(CCDigestRef ctx)
{
    return ctx->alg->hash_size;
}

#endif /* HAVE_CCDIGESTCREATE */

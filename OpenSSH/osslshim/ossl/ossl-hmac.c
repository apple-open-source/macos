/*
 * Copyright (c) 2011-12 Apple Inc. All Rights Reserved.
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
 * Copyright (c) 2006 - 2007 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
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

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ossl-hmac.h"

void
HMAC_CTX_init(HMAC_CTX *ctx)
{
	memset(ctx, 0, sizeof(*ctx));
}


void
HMAC_CTX_cleanup(HMAC_CTX *ctx)
{
	EVP_MD_CTX_cleanup(&ctx->md_ctx);
	EVP_MD_CTX_cleanup(&ctx->i_ctx);
	EVP_MD_CTX_cleanup(&ctx->o_ctx);
}


size_t
HMAC_size(const HMAC_CTX *ctx)
{
	return (EVP_MD_size(ctx->md));
}


void
HMAC_Init_ex(HMAC_CTX *ctx,
    const void *key,
    size_t keylen,
    const EVP_MD *md,
    ENGINE *engine)
{
	int i, blksize, reset = 0;
	unsigned char pad[HMAC_MAX_MD_CBLOCK];

	if (md != NULL) {
		reset = 1;
		ctx->md = md;
	} else{
		md = ctx->md;
	}

	if (key != NULL) {
		reset = 1;
		blksize = EVP_MD_block_size(md);

		/* assert(j <= (int)sizeof(ctx->key)); */
		if (blksize < keylen) {
			EVP_DigestInit_ex(&ctx->md_ctx, md, engine);
			EVP_DigestUpdate(&ctx->md_ctx, key, keylen);
			EVP_DigestFinal_ex(&(ctx->md_ctx), ctx->key,
			    &ctx->key_length);
		} else {
			/* assert(keylen>=0 && keylen<=(int)sizeof(ctx->buf)); */
			memcpy(ctx->key, key, keylen);
			ctx->key_length = keylen;
		}
		if (ctx->key_length != HMAC_MAX_MD_CBLOCK) {
			memset(&ctx->key[ctx->key_length], 0,
			    HMAC_MAX_MD_CBLOCK - ctx->key_length);
		}
	}

	if (reset) {
		for (i = 0; i < HMAC_MAX_MD_CBLOCK; i++) {
			pad[i] = 0x36 ^ ctx->key[i];
		}
		EVP_DigestInit_ex(&ctx->i_ctx, md, engine);
		EVP_DigestUpdate(&ctx->i_ctx, pad, EVP_MD_block_size(md));

		for (i = 0; i < HMAC_MAX_MD_CBLOCK; i++) {
			pad[i] = 0x5c ^ ctx->key[i];
		}
		EVP_DigestInit_ex(&ctx->o_ctx, md, engine);
		EVP_DigestUpdate(&ctx->o_ctx, pad, EVP_MD_block_size(md));
	}

	EVP_MD_CTX_copy_ex(&ctx->md_ctx, &ctx->i_ctx);
}


void
HMAC_Init(HMAC_CTX *ctx, const void *key, size_t keylen, const EVP_MD *md)
{
	if (key && md) {
		HMAC_CTX_init(ctx);
	}
	HMAC_Init_ex(ctx, key, keylen, md, NULL);
}


void
HMAC_Update(HMAC_CTX *ctx, const void *data, size_t len)
{
	EVP_DigestUpdate(&ctx->md_ctx, data, len);
}


void
HMAC_Final(HMAC_CTX *ctx, void *md, unsigned int *len)
{
	unsigned int i;
	unsigned char buf[EVP_MAX_MD_SIZE];

	EVP_DigestFinal_ex(&ctx->md_ctx, buf, &i);
	EVP_MD_CTX_copy_ex(&ctx->md_ctx, &ctx->o_ctx);
	EVP_DigestUpdate(&ctx->md_ctx, buf, i);
	EVP_DigestFinal_ex(&ctx->md_ctx, md, len);
}


void *
HMAC(const EVP_MD *md,
    const void *key, size_t key_size,
    const void *data, size_t data_size,
    void *hash, unsigned int *hash_len)
{
	HMAC_CTX ctx;

	HMAC_CTX_init(&ctx);
	HMAC_Init_ex(&ctx, key, key_size, md, NULL);
	HMAC_Update(&ctx, data, data_size);
	HMAC_Final(&ctx, hash, hash_len);
	HMAC_CTX_cleanup(&ctx);
	return (hash);
}

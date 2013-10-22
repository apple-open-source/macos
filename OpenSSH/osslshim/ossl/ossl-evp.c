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
 * Copyright (c) 2006 - 2008 Kungliga Tekniska Högskolan
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

#include "ossl-config.h"

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "ossl-evp.h"
#include "ossl-objects.h"
#include "ossl-evp-cc.h"
#include "ossl-ui.h"

#ifdef HAVE_COMMONCRYPTO_COMMONRANDOMSPI_H
#include <CommonCrypto/CommonRandomSPI.h>
#endif

#include "krb5-types.h"

#ifndef OSSL_DEF_PROVIDER
#define OSSL_DEF_PROVIDER    cc
#endif

#define OSSL_CONCAT4(x, y, z, aa)	x ## y ## z ## aa


#define EVP_DEF_OP(_prov, _op)		OSSL_CONCAT4(EVP_, _prov, _, _op) ()

/**
 * @page page_evp EVP - generic crypto interface
 *
 * @section evp_cipher EVP Cipher
 *
 * The use of EVP_CipherInit_ex() and EVP_Cipher() is pretty easy to
 * understand forward, then EVP_CipherUpdate() and
 * EVP_CipherFinal_ex() really needs an example to explain @ref
 * example_evp_cipher.c .
 *
 * @example example_evp_cipher.c
 *
 * This is an example how to use EVP_CipherInit_ex(),
 * EVP_CipherUpdate() and EVP_CipherFinal_ex().
 */


/**
 * Return the output size of the message digest function.
 *
 * @param md the evp message
 *
 * @return size output size of the message digest function.
 */
size_t
EVP_MD_size(const EVP_MD *md)
{
	return (md->hash_size);
}


/**
 * Return the blocksize of the message digest function.
 *
 * @param md the evp message
 *
 * @return size size of the message digest block size
 */
size_t
EVP_MD_block_size(const EVP_MD *md)
{
	return (md->block_size);
}


/**
 * Allocate a messsage digest context object. Free with
 * EVP_MD_CTX_destroy().
 *
 * @return a newly allocated message digest context object.
 */
EVP_MD_CTX *
EVP_MD_CTX_create(void)
{
	return (calloc(1, sizeof(EVP_MD_CTX)));
}


/**
 * Initiate a messsage digest context object. Deallocate with
 * EVP_MD_CTX_cleanup(). Please use EVP_MD_CTX_create() instead.
 *
 * @param ctx variable to initiate.
 */
void
EVP_MD_CTX_init(EVP_MD_CTX *ctx)
{
	memset(ctx, 0, sizeof(*ctx));
}


/**
 * Free a messsage digest context object.
 *
 * @param ctx context to free.
 */
void
EVP_MD_CTX_destroy(EVP_MD_CTX *ctx)
{
	EVP_MD_CTX_cleanup(ctx);
	free(ctx);
}


/**
 * Free the resources used by the EVP_MD context.
 *
 * @param ctx the context to free the resources from.
 *
 * @return 1 on success.
 */
int
EVP_MD_CTX_cleanup(EVP_MD_CTX *ctx)
{
	if (ctx->md && ctx->md->cleanup) {
		(ctx->md->cleanup)(ctx);
	} else if (ctx->md) {
		memset(ctx->ptr, 0, ctx->md->ctx_size);
	}
	ctx->md = NULL;
	ctx->engine = NULL;
	free(ctx->ptr);
	memset(ctx, 0, sizeof(*ctx));
	return (1);
}


/**
 * Get the EVP_MD use for a specified context.
 *
 * @param ctx the EVP_MD context to get the EVP_MD for.
 *
 * @return the EVP_MD used for the context.
 */
const EVP_MD *
EVP_MD_CTX_md(EVP_MD_CTX *ctx)
{
	return (ctx->md);
}


/**
 * Return the output size of the message digest function.
 *
 * @param ctx the evp message digest context
 *
 * @return size output size of the message digest function.
 */
size_t
EVP_MD_CTX_size(EVP_MD_CTX *ctx)
{
	return (EVP_MD_size(ctx->md));
}


/**
 * Return the blocksize of the message digest function.
 *
 * @param ctx the evp message digest context
 *
 * @return size size of the message digest block size
 */
size_t
EVP_MD_CTX_block_size(EVP_MD_CTX *ctx)
{
	return (EVP_MD_block_size(ctx->md));
}


int
EVP_MD_CTX_copy_ex(EVP_MD_CTX *out, const EVP_MD_CTX *in)
{
	if ((NULL == in) || (NULL == in->md)) {
		/* EVPerr */
		return (0);
	}

	EVP_MD_CTX_cleanup(out);
	memcpy(out, in, sizeof(*out));

	if (out->md->ctx_size) {
		out->ptr = malloc(in->md->ctx_size);
		if (!out->ptr) {
			/* EVPerr */
			return (0);
		}
		memcpy(out->ptr, in->ptr, out->md->ctx_size);
	}

	if (out->engine) {
		ENGINE_up_ref(out->engine);
	}

	/*
	 * if (out->md->copy)
	 *      return out->md->copy(out, in);
	 */

	return (1);
}


/**
 * Init a EVP_MD_CTX for use a specific message digest and engine.
 *
 * @param ctx the message digest context to init.
 * @param md the message digest to use.
 * @param engine the engine to use, NULL to use the default engine.
 *
 * @return 1 on success.
 */
int
EVP_DigestInit_ex(EVP_MD_CTX *ctx, const EVP_MD *md, ENGINE *engine)
{
	if (NULL == ctx)
		return (0);
	if ((ctx->md != md) || (ctx->engine != engine)) {
		EVP_MD_CTX_cleanup(ctx);
		ctx->md = md;
		ctx->engine = engine;

		ctx->ptr = calloc(1, md->ctx_size);
		if (ctx->ptr == NULL) {
			return (0);
		}
	}
	(ctx->md->init)(ctx->ptr);
	return (1);
}


int
EVP_DigestInit(EVP_MD_CTX *ctx, const EVP_MD *md)
{
	if (ctx) {
		EVP_MD_CTX_init(ctx);
	}
	return (EVP_DigestInit_ex(ctx, md, NULL));
}


/**
 * Update the digest with some data.
 *
 * @param ctx the context to update
 * @param data the data to update the context with
 * @param size length of data
 *
 * @return 1 on success.
 */
int
EVP_DigestUpdate(EVP_MD_CTX *ctx, const void *data, size_t size)
{
	(ctx->md->update)(ctx->ptr, data, size);
	return (1);
}


/**
 * Complete the message digest.
 *
 * @param ctx the context to complete.
 * @param hash the output of the message digest function. At least
 * EVP_MD_size().
 * @param size the output size of hash.
 *
 * @return 1 on success.
 */
int
EVP_DigestFinal_ex(EVP_MD_CTX *ctx, void *hash, unsigned int *size)
{
	(ctx->md->final)(hash, ctx->ptr);
	if (size) {
		*size = ctx->md->hash_size;
	}
	return (1);
}


int
EVP_DigestFinal(EVP_MD_CTX *ctx, void *hash, unsigned int *size)
{
	int rv = EVP_DigestFinal_ex(ctx, hash, size);

	EVP_MD_CTX_cleanup(ctx);
	return (rv);
}


/**
 * Do the whole EVP_MD_CTX_create(), EVP_DigestInit_ex(),
 * EVP_DigestUpdate(), EVP_DigestFinal_ex(), EVP_MD_CTX_destroy()
 * dance in one call.
 *
 * @param data the data to update the context with
 * @param dsize length of data
 * @param hash output data of at least EVP_MD_size() length.
 * @param hsize output length of hash.
 * @param md message digest to use
 * @param engine engine to use, NULL for default engine.
 *
 * @return 1 on success.
 */
int
EVP_Digest(const void *data, size_t dsize, void *hash, unsigned int *hsize,
    const EVP_MD *md, ENGINE *engine)
{
	EVP_MD_CTX ctx;
	int ret;

	EVP_MD_CTX_init(&ctx);
	ret = EVP_DigestInit_ex(&ctx, md, engine) &&
	    EVP_DigestUpdate(&ctx, data, dsize) &&
	    EVP_DigestFinal_ex(&ctx, hash, hsize);
	EVP_MD_CTX_cleanup(&ctx);

	return (ret);
}


/**
 * The message digest rmd128
 *
 * @return the message digest type.
 */
const EVP_MD *
EVP_rmd128(void)
{
	return (EVP_DEF_OP(OSSL_DEF_PROVIDER, rmd128));
}


const EVP_MD *
EVP_ripemd128(void)
{
	return (EVP_rmd128());
}


/**
 * The message digest rmd160
 *
 * @return the message digest type.
 */
const EVP_MD *
EVP_rmd160(void)
{
	return (EVP_DEF_OP(OSSL_DEF_PROVIDER, rmd160));
}


const EVP_MD *
EVP_ripemd160(void)
{
	return (EVP_rmd160());
}


/**
 * The message digest rmd256
 *
 * @return the message digest type.
 */
const EVP_MD *
EVP_rmd256(void)
{
	return (EVP_DEF_OP(OSSL_DEF_PROVIDER, rmd256));
}


const EVP_MD *
EVP_ripemd256(void)
{
	return (EVP_rmd256());
}


/**
 * The message digest rmd320
 *
 * @return the message digest type.
 */
const EVP_MD *
EVP_rmd320(void)
{
	return (EVP_DEF_OP(OSSL_DEF_PROVIDER, rmd320));
}


const EVP_MD *
EVP_ripemd320(void)
{
	return (EVP_rmd320());
}


/**
 * The message digest sha224
 *
 * @return the message digest type.
 */
const EVP_MD *
EVP_sha224(void)
{
	return (EVP_DEF_OP(OSSL_DEF_PROVIDER, sha224));
}


/**
 * The message digest SHA256
 *
 * @return the message digest type.
 */
const EVP_MD *
EVP_sha256(void)
{
	return (EVP_DEF_OP(OSSL_DEF_PROVIDER, sha256));
}


/**
 * The message digest SHA384
 *
 * @return the message digest type.
 */
const EVP_MD *
EVP_sha384(void)
{
	return (EVP_DEF_OP(OSSL_DEF_PROVIDER, sha384));
}


/**
 * The message digest SHA512
 *
 * @return the message digest type.
 */
const EVP_MD *
EVP_sha512(void)
{
	return (EVP_DEF_OP(OSSL_DEF_PROVIDER, sha512));
}


/**
 * The message digest SHA1
 *
 * @return the message digest type.
 */
const EVP_MD *
EVP_sha1(void)
{
	return (EVP_DEF_OP(OSSL_DEF_PROVIDER, sha1));
}


/**
 * The message digest SHA1
 *
 * @return the message digest type.
 */
const EVP_MD *
EVP_sha(void)

{
	return (EVP_sha1());
}


/**
 * The message digest MD5
 *
 * @return the message digest type.
 */
const EVP_MD *
EVP_md5(void)
{
	return (EVP_DEF_OP(OSSL_DEF_PROVIDER, md5));
}


/**
 * The message digest MD4
 *
 * @return the message digest type.
 */
const EVP_MD *
EVP_md4(void)
{
	return (EVP_DEF_OP(OSSL_DEF_PROVIDER, md4));
}


/**
 * The message digest MD2
 *
 * @return the message digest type.
 */
const EVP_MD *
EVP_md2(void)
{
	return (EVP_DEF_OP(OSSL_DEF_PROVIDER, md2));
}


/*
 * Null MD
 */
static void
null_Init(void *m)
{
}


static void
null_Update(void *m, const void *data, size_t size)
{
}


static void
null_Final(void *res, void *m)
{
}


/**
 * The null message digest
 *
 * @return the message digest type.
 *
 * @ingroup hcrypto_evp
 */
const EVP_MD *
EVP_md_null(void)
{
	static const struct ossl_evp_md null =
	{
		.hash_size	=			    0,
		.block_size	=			    0,
		.ctx_size	=			    0,
		.init		= (ossl_evp_md_init)null_Init,
		.update		= (ossl_evp_md_update)null_Update,
		.final		= (ossl_evp_md_final)null_Final,
		.cleanup	= NULL
	};

	return (&null);
}


const EVP_MD *
EVP_get_digestbynid(int nid)
{
	switch (nid) {
	case NID_md2:
		return (EVP_md2());

	case NID_md4:
		return (EVP_md4());

	case NID_md5:
		return (EVP_md5());

	case NID_sha1:
		return (EVP_sha1());

	case NID_sha256:
		return (EVP_sha256());

	case NID_sha384:
		return (EVP_sha384());

	case NID_sha512:
		return (EVP_sha512());

	default:
		return (NULL);
	}
}


int
EVP_CIPHER_nid(const EVP_CIPHER *c)
{
	return (c->nid);
}


/**
 * Return the block size of the cipher.
 *
 * @param c cipher to get the block size from.
 *
 * @return the block size of the cipher.
 */
size_t
EVP_CIPHER_block_size(const EVP_CIPHER *c)
{
	return (c->block_size);
}


/**
 * Return the key size of the cipher.
 *
 * @param c cipher to get the key size from.
 *
 * @return the key size of the cipher.
 */
size_t
EVP_CIPHER_key_length(const EVP_CIPHER *c)
{
	return (c->key_len);
}


/**
 * Return the IV size of the cipher.
 *
 * @param c cipher to get the IV size from.
 *
 * @return the IV size of the cipher.
 */
size_t
EVP_CIPHER_iv_length(const EVP_CIPHER *c)
{
	return (c->iv_len);
}


/**
 * Initiate a EVP_CIPHER_CTX context. Clean up with
 * EVP_CIPHER_CTX_cleanup().
 *
 * @param c the cipher initiate.
 */
void
EVP_CIPHER_CTX_init(EVP_CIPHER_CTX *c)
{
	memset(c, 0, sizeof(*c));
}


/**
 * Clean up the EVP_CIPHER_CTX context.
 *
 * @param c the cipher to clean up.
 *
 * @return 1 on success.
 */
int
EVP_CIPHER_CTX_cleanup(EVP_CIPHER_CTX *c)
{
	if (c->cipher) {
		if (c->cipher->cleanup) {
			c->cipher->cleanup(c);
		}

		if (c->cipher_data) {
			memset(c->cipher_data, 0, c->cipher->ctx_size);
			free(c->cipher_data);
			c->cipher_data = NULL;
		}
		EVP_CIPHER_CTX_init(c);
	}
	return (1);
}


/**
 * If the cipher type supports it, change the key length
 *
 * @param c the cipher context to change the key length for
 * @param length new key length
 *
 * @return 1 on success.
 */
int
EVP_CIPHER_CTX_set_key_length(EVP_CIPHER_CTX *c, int length)
{
	if ((c->cipher->flags & EVP_CIPH_VARIABLE_LENGTH) && (length > 0)) {
		c->key_len = length;
		return (1);
	}
	return (0);
}


#if 0
int
EVP_CIPHER_CTX_set_padding(EVP_CIPHER_CTX *c, int pad)
{
	return (0);
}


#endif

/**
 * Return the EVP_CIPHER for a EVP_CIPHER_CTX context.
 *
 * @param ctx the context to get the cipher type from.
 *
 * @return the EVP_CIPHER pointer.
 */
const EVP_CIPHER *
EVP_CIPHER_CTX_cipher(EVP_CIPHER_CTX *ctx)
{
	return (ctx->cipher);
}


/**
 * Return the block size of the cipher context.
 *
 * @param ctx cipher context to get the block size from.
 *
 * @return the block size of the cipher context.
 */
size_t
EVP_CIPHER_CTX_block_size(const EVP_CIPHER_CTX *ctx)
{
	return (EVP_CIPHER_block_size(ctx->cipher));
}


/**
 * Return the key size of the cipher context.
 *
 * @param ctx cipher context to get the key size from.
 *
 * @return the key size of the cipher context.
 */
size_t
EVP_CIPHER_CTX_key_length(const EVP_CIPHER_CTX *ctx)
{
	/* return EVP_CIPHER_key_length(ctx->cipher); */
	return (ctx->key_len);
}


/**
 * Return the IV size of the cipher context.
 *
 * @param ctx cipher context to get the IV size from.
 *
 * @return the IV size of the cipher context.
 */
size_t
EVP_CIPHER_CTX_iv_length(const EVP_CIPHER_CTX *ctx)
{
	return (EVP_CIPHER_iv_length(ctx->cipher));
}


/**
 * Get the flags for an EVP_CIPHER_CTX context.
 *
 * @param ctx the EVP_CIPHER_CTX to get the flags from
 *
 * @return the flags for an EVP_CIPHER_CTX.
 */
unsigned long
EVP_CIPHER_CTX_flags(const EVP_CIPHER_CTX *ctx)
{
	return (ctx->cipher->flags);
}


/**
 * Get the mode for an EVP_CIPHER_CTX context.
 *
 * @param ctx the EVP_CIPHER_CTX to get the mode from
 *
 * @return the mode for an EVP_CIPHER_CTX.
 */
int
EVP_CIPHER_CTX_mode(const EVP_CIPHER_CTX *ctx)
{
	return (EVP_CIPHER_CTX_flags(ctx) & EVP_CIPH_MODE);
}


/**
 * Get the app data for an EVP_CIPHER_CTX context.
 *
 * @param ctx the EVP_CIPHER_CTX to get the app data from
 *
 * @return the app data for an EVP_CIPHER_CTX.
 */
void *
EVP_CIPHER_CTX_get_app_data(EVP_CIPHER_CTX *ctx)
{
	return (ctx->app_data);
}


/**
 * Set the app data for an EVP_CIPHER_CTX context.
 *
 * @param ctx the EVP_CIPHER_CTX to set the app data for
 * @param data the app data to set for an EVP_CIPHER_CTX.
 */
void
EVP_CIPHER_CTX_set_app_data(EVP_CIPHER_CTX *ctx, void *data)
{
	ctx->app_data = data;
}


/**
 * Initiate the EVP_CIPHER_CTX context to encrypt or decrypt data.
 * Clean up with EVP_CIPHER_CTX_cleanup().
 *
 * @param ctx context to initiate
 * @param c cipher to use.
 * @param engine crypto engine to use, NULL to select default.
 * @param key the crypto key to use, NULL will use the previous value.
 * @param iv the IV to use, NULL will use the previous value.
 * @param encp non zero will encrypt, -1 use the previous value.
 *
 * @return 1 on success.
 */
int
EVP_CipherInit_ex(EVP_CIPHER_CTX *ctx, const EVP_CIPHER *c, ENGINE *engine,
    const void *key, const void *iv, int encp)
{
	ctx->buf_len = 0;

	if (encp == -1) {
		encp = ctx->encrypt;
	} else{
		ctx->encrypt = (encp ? 1 : 0);
	}

	if (c && (c != ctx->cipher)) {
		EVP_CIPHER_CTX_cleanup(ctx);
		ctx->cipher = c;
		ctx->key_len = c->key_len;

		ctx->cipher_data = calloc(1, c->ctx_size);
		if ((ctx->cipher_data == NULL) && (c->ctx_size != 0)) {
			return (0);
		}

		/* assume block size is a multiple of 2 */
		ctx->block_mask = EVP_CIPHER_block_size(c) - 1;
	} else if (ctx->cipher == NULL) {
		/* reuse of cipher, but not any cipher ever set! */
		return (0);
	}

	switch (EVP_CIPHER_CTX_mode(ctx)) {
	case EVP_CIPH_CBC_MODE:

		assert(EVP_CIPHER_CTX_iv_length(ctx) <= sizeof(ctx->iv));

		if (iv) {
			memcpy(ctx->oiv, iv, EVP_CIPHER_CTX_iv_length(ctx));
		}
		memcpy(ctx->iv, ctx->oiv, EVP_CIPHER_CTX_iv_length(ctx));
		break;

	case EVP_CIPH_ECB_MODE:
	case EVP_CIPH_STREAM_CIPHER:
	case EVP_CIPH_CFB8_MODE:
		if (iv) {
			memcpy(ctx->iv, iv, EVP_CIPHER_CTX_iv_length(ctx));
		}
		break;

	default:
		return (0);
	}

	if (key || (ctx->cipher->flags & EVP_CIPH_ALWAYS_CALL_INIT)) {
		ctx->cipher->init(ctx, key, iv, encp);
	}

	return (1);
}


int
EVP_CipherInit(EVP_CIPHER_CTX *ctx, const EVP_CIPHER *c, const void *key,
    const void *iv, int encp)
{
	return (EVP_CipherInit_ex(ctx, c, NULL, key, iv, encp));
}


/**
 * Encipher/decipher partial data
 *
 * @param ctx the cipher context.
 * @param out output data from the operation.
 * @param outlen output length
 * @param in input data to the operation.
 * @param inlen length of data.
 *
 * The output buffer length should at least be EVP_CIPHER_block_size()
 * byte longer then the input length.
 *
 * See @ref evp_cipher for an example how to use this function.
 *
 * @return 1 on success.
 */
int
EVP_CipherUpdate(EVP_CIPHER_CTX *ctx, void *out, int *outlen,
    void *in, size_t inlen)
{
	int ret, left, blocksize;

	*outlen = 0;

	/**
	 * If there in no spare bytes in the left from last Update and the
	 * input length is on the block boundery, the EVP_CipherUpdate()
	 * function can take a shortcut (and preformance gain) and
	 * directly encrypt the data, otherwise we hav to fix it up and
	 * store extra it the EVP_CIPHER_CTX.
	 */
	if ((ctx->buf_len == 0) && ((inlen & ctx->block_mask) == 0)) {
		ret = (*ctx->cipher->do_cipher)(ctx, out, in, inlen);
		if (ret == 1) {
			*outlen = inlen;
		} else{
			*outlen = 0;
		}
		return (ret);
	}


	blocksize = EVP_CIPHER_CTX_block_size(ctx);
	left = blocksize - ctx->buf_len;
	assert(left > 0);

	if (ctx->buf_len) {
		/* if total buffer is smaller then input, store locally */
		if (inlen < left) {
			memcpy(ctx->buf + ctx->buf_len, in, inlen);
			ctx->buf_len += inlen;
			return (1);
		}

		/* fill in local buffer and encrypt */
		memcpy(ctx->buf + ctx->buf_len, in, left);
		ret = (*ctx->cipher->do_cipher)(ctx, out, ctx->buf, blocksize);
		memset(ctx->buf, 0, blocksize);
		if (ret != 1) {
			return (ret);
		}

		*outlen += blocksize;
		inlen -= left;
		in = ((unsigned char *)in) + left;
		out = ((unsigned char *)out) + blocksize;
		ctx->buf_len = 0;
	}

	if (inlen) {
		ctx->buf_len = (inlen & ctx->block_mask);
		inlen &= ~ctx->block_mask;

		ret = (*ctx->cipher->do_cipher)(ctx, out, in, inlen);
		if (ret != 1) {
			return (ret);
		}

		*outlen += inlen;

		in = ((unsigned char *)in) + inlen;
		memcpy(ctx->buf, in, ctx->buf_len);
	}

	return (1);
}


/**
 * Encipher/decipher final data
 *
 * @param ctx the cipher context.
 * @param out output data from the operation.
 * @param outlen output length
 *
 * The input length needs to be at least EVP_CIPHER_block_size() bytes
 * long.
 *
 * See @ref evp_cipher for an example how to use this function.
 *
 * @return 1 on success.
 */
int
EVP_CipherFinal_ex(EVP_CIPHER_CTX *ctx, void *out, int *outlen)
{
	*outlen = 0;

	// Love say to always output a block even if we have ctx->buf_len is 0... that is a whole block of pad bytes.
	int ret, left, blocksize;

	blocksize = EVP_CIPHER_CTX_block_size(ctx);
	
	if (blocksize == 1) {
		*outlen = 0;
		return (1);
	}

	left = blocksize - ctx->buf_len;
	assert(left > 0);

	/* zero fill local buffer */
	// use PKCS11 padding with pad value of number of pad byes.
	memset(ctx->buf + ctx->buf_len, left, left);
	
	ret = (*ctx->cipher->do_cipher)(ctx, out, ctx->buf, blocksize);
	memset(ctx->buf, 0, blocksize);
	
	if (ret != 1) {
		return (ret);
	}

	*outlen += blocksize;

	return (1);
}


/**
 * Encipher/decipher data
 *
 * @param ctx the cipher context.
 * @param out out data from the operation.
 * @param in in data to the operation.
 * @param size length of data.
 */
int
EVP_Cipher(EVP_CIPHER_CTX *ctx, void *out, const void *in, size_t size)
{
	return (ctx->cipher->do_cipher(ctx, out, in, size));
}


/*
 *
 */
static int
enc_null_init(EVP_CIPHER_CTX *ctx,
    const unsigned char *key,
    const unsigned char *iv,
    int encp)
{
	return (1);
}


static int
enc_null_do_cipher(EVP_CIPHER_CTX *ctx,
    unsigned char *out,
    const unsigned char *in,
    unsigned int size)
{
	memmove(out, in, size);
	return (1);
}


static int
enc_null_cleanup(EVP_CIPHER_CTX *ctx)
{
	return (1);
}


/**
 * The NULL cipher type, does no encryption/decryption.
 *
 * @return the null EVP_CIPHER pointer.
 *
 * @ingroup hcrypto_evp
 */
const EVP_CIPHER *
EVP_enc_null(void)
{
	static const EVP_CIPHER enc_null =
	{
		.nid			=		0,
		.block_size		=		0,
		.key_len		=		0,
		.iv_len			=		0,
		.flags			= EVP_CIPH_CBC_MODE,
		.init			= enc_null_init,
		.do_cipher		= enc_null_do_cipher,
		.cleanup		= enc_null_cleanup,
		.ctx_size		=		0,
		.set_asn1_parameters	= NULL,
		.get_asn1_parameters	= NULL,
		.ctrl			= NULL,
		.app_data		= NULL
	};

	return (&enc_null);
}


/**
 * The RC2 cipher type
 *
 * @return the RC2 EVP_CIPHER pointer.
 */
const EVP_CIPHER *
EVP_rc2_cbc(void)
{
	return (EVP_DEF_OP(OSSL_DEF_PROVIDER, rc2_cbc));
}


/**
 * The RC2 cipher type
 *
 * @return the RC2 EVP_CIPHER pointer.
 */
const EVP_CIPHER *
EVP_rc2_40_cbc(void)
{
	return (EVP_DEF_OP(OSSL_DEF_PROVIDER, rc2_40_cbc));
}


/**
 * The RC2 cipher type
 *
 * @return the RC2 EVP_CIPHER pointer.
 */
const EVP_CIPHER *
EVP_rc2_64_cbc(void)
{
	return (EVP_DEF_OP(OSSL_DEF_PROVIDER, rc2_64_cbc));
}


/**
 * The RC4 cipher type
 *
 * @return the RC4 EVP_CIPHER pointer.
 */
const EVP_CIPHER *
EVP_rc4(void)
{
	return (EVP_DEF_OP(OSSL_DEF_PROVIDER, rc4));
}


/**
 * The RC4-40 cipher type
 *
 * @return the RC4-40 EVP_CIPHER pointer.
 */
const EVP_CIPHER *
EVP_rc4_40(void)
{
	return (EVP_DEF_OP(OSSL_DEF_PROVIDER, rc4_40));
}


/**
 * The DES cipher type
 *
 * @return the DES-ECB EVP_CIPHER pointer.
 */
const EVP_CIPHER *
EVP_des_ecb(void)
{
	return (EVP_DEF_OP(OSSL_DEF_PROVIDER, des_ecb));
}


/**
 * The DES cipher type
 *
 * @return the DES-CBC EVP_CIPHER pointer.
 */
const EVP_CIPHER *
EVP_des_cbc(void)
{
	return (EVP_DEF_OP(OSSL_DEF_PROVIDER, des_cbc));
}


/**
 * The tripple DES cipher type
 *
 * @return the DES-EDE3-CBC EVP_CIPHER pointer.
 */
const EVP_CIPHER *
EVP_des_ede3_cbc(void)
{
	return (EVP_DEF_OP(OSSL_DEF_PROVIDER, des_ede3_cbc));
}


/**
 * The tripple DES cipher type
 *
 * @return the DES-EDE3-ECB EVP_CIPHER pointer.
 */
const EVP_CIPHER *
EVP_des_ede3_ecb(void)
{
	return (EVP_DEF_OP(OSSL_DEF_PROVIDER, des_ede3_ecb));
}


/**
 * The AES-128 cipher type
 *
 * @return the AES-128 EVP_CIPHER pointer.
 */
const EVP_CIPHER *
EVP_aes_128_cbc(void)
{
	return (EVP_DEF_OP(OSSL_DEF_PROVIDER, aes_128_cbc));
}


/**
 * The AES-128 cipher type
 *
 * @return the AES-128 EVP_CIPHER pointer.
 */
const EVP_CIPHER *
EVP_aes_128_ecb(void)
{
	return (EVP_DEF_OP(OSSL_DEF_PROVIDER, aes_128_ecb));
}


/**
 * The AES-192 cipher type
 *
 * @return the AES-192 EVP_CIPHER pointer.
 */
const EVP_CIPHER *
EVP_aes_192_cbc(void)
{
	return (EVP_DEF_OP(OSSL_DEF_PROVIDER, aes_192_cbc));
}


/**
 * The AES-192 cipher type
 *
 * @return the AES-192 EVP_CIPHER pointer.
 */
const EVP_CIPHER *
EVP_aes_192_ecb(void)
{
	return (EVP_DEF_OP(OSSL_DEF_PROVIDER, aes_192_ecb));
}


/**
 * The AES-256 cipher type
 *
 * @return the AES-256 EVP_CIPHER pointer.
 */
const EVP_CIPHER *
EVP_aes_256_cbc(void)
{
	return (EVP_DEF_OP(OSSL_DEF_PROVIDER, aes_256_cbc));
}


/**
 * The AES-256 cipher type
 *
 * @return the AES-256 EVP_CIPHER pointer.
 */
const EVP_CIPHER *
EVP_aes_256_ecb(void)
{
	return (EVP_DEF_OP(OSSL_DEF_PROVIDER, aes_256_ecb));
}


/**
 * The AES-128 CFB8 cipher type
 *
 * @return the AES-128 EVP_CIPHER pointer.
 */
const EVP_CIPHER *
EVP_aes_128_cfb8(void)
{
#ifndef __APPLE_TARGET_EMBEDDED__
	return (EVP_DEF_OP(OSSL_DEF_PROVIDER, aes_128_cfb8));

#else
	return (NULL);
#endif
}


/**
 * The AES-192 CFB8 cipher type
 *
 * @return the AES-192 EVP_CIPHER pointer.
 *
 * @ingroup hcrypto_evp
 */
const EVP_CIPHER *
EVP_aes_192_cfb8(void)
{
#ifndef __APPLE_TARGET_EMBEDDED__
	return (EVP_DEF_OP(OSSL_DEF_PROVIDER, aes_192_cfb8));

#else
	return (NULL);
#endif
}


/**
 * The AES-256 CFB8 cipher type
 *
 * @return the AES-256 EVP_CIPHER pointer.
 *
 * @ingroup hcrypto_evp
 */
const EVP_CIPHER *
EVP_aes_256_cfb8(void)
{
#ifndef __APPLE_TARGET_EMBEDDED__
	return (EVP_DEF_OP(OSSL_DEF_PROVIDER, aes_256_cfb8));

#else
	return (NULL);
#endif
}


/**
 * The Camellia-128 cipher type
 *
 * @return the Camellia-128 EVP_CIPHER pointer.
 */
const EVP_CIPHER *
EVP_camellia_128_cbc(void)
{
	return (EVP_DEF_OP(OSSL_DEF_PROVIDER, camellia_128_cbc));
}


/**
 * The Camellia-198 cipher type
 *
 * @return the Camellia-198 EVP_CIPHER pointer.
 */
const EVP_CIPHER *
EVP_camellia_192_cbc(void)
{
	return (EVP_DEF_OP(OSSL_DEF_PROVIDER, camellia_192_cbc));
}


/**
 * The Camellia-256 cipher type
 *
 * @return the Camellia-256 EVP_CIPHER pointer.
 */
const EVP_CIPHER *
EVP_camellia_256_cbc(void)
{
	return (EVP_DEF_OP(OSSL_DEF_PROVIDER, camellia_256_cbc));
}


/**
 * The blowfish ecb cipher type
 *
 * @return the bf_ecb EVP_CIPHER pointer.
 */
const EVP_CIPHER *
EVP_bf_ecb(void)
{
	return (EVP_DEF_OP(OSSL_DEF_PROVIDER, bf_ecb));
}


/**
 * The blowfish cbc cipher type
 *
 * @return the bf_ecb EVP_CIPHER pointer.
 */
const EVP_CIPHER *
EVP_bf_cbc(void)
{
	return (EVP_DEF_OP(OSSL_DEF_PROVIDER, bf_cbc));
}


/**
 * The cast5 ecb cipher type
 *
 * @return the bf_ecb EVP_CIPHER pointer.
 */
const EVP_CIPHER *
EVP_cast5_ecb(void)
{
	return (EVP_DEF_OP(OSSL_DEF_PROVIDER, cast5_ecb));
}


/**
 * The cast5 cbc cipher type
 *
 * @return the bf_ecb EVP_CIPHER pointer.
 */
const EVP_CIPHER *
EVP_cast5_cbc(void)
{
	return (EVP_DEF_OP(OSSL_DEF_PROVIDER, cast5_cbc));
}


/*
 *
 */

static const struct cipher_name {
	const char *		name;
	const EVP_CIPHER *	(*func)(void);
}
cipher_name[] =
{
	{ "des-ecb",	      EVP_des_ecb	   },
	{ "des-cbc",	      EVP_des_cbc	   },
	{ "des-ede3-ecb",     EVP_des_ede3_ecb	   },
	{ "des-ede3-cbc",     EVP_des_ede3_cbc	   },
	{ "aes-128-ecb",      EVP_aes_128_ecb	   },
	{ "aes-192-ecb",      EVP_aes_192_ecb	   },
	{ "aes-256-ecb",      EVP_aes_256_ecb	   },
	{ "aes-128-cbc",      EVP_aes_128_cbc	   },
	{ "aes-192-cbc",      EVP_aes_192_cbc	   },
	{ "aes-256-cbc",      EVP_aes_256_cbc	   },
	{ "aes-128-cfb8",     EVP_aes_128_cfb8	   },
	{ "aes-192-cfb8",     EVP_aes_192_cfb8	   },
	{ "aes-256-cfb8",     EVP_aes_256_cfb8	   },
	{ "camellia-128-cbc", EVP_camellia_128_cbc },
	{ "camellia-192-cbc", EVP_camellia_192_cbc },
	{ "camellia-256-cbc", EVP_camellia_256_cbc },
	{ "bf-ecb",	      EVP_bf_ecb	   },
	{ "bf-cbc",	      EVP_bf_cbc	   },
	{ "cast5-ecb",	      EVP_cast5_ecb	   },
	{ "cast5-cbc",	      EVP_cast5_cbc	   },
};

/**
 * Get the cipher type using their name.
 *
 * @param name the name of the cipher.
 *
 * @return the selected EVP_CIPHER pointer or NULL if not found.
 *
 * @ingroup hcrypto_evp
 */
const EVP_CIPHER *
EVP_get_cipherbyname(const char *name)
{
	int i;

	for (i = 0; i < sizeof(cipher_name)/sizeof(cipher_name[0]); i++) {
		if (strcasecmp(cipher_name[i].name, name) == 0) {
			return ((*cipher_name[i].func)());
		}
	}
	return (NULL);
}


/*
 *
 */

#ifndef min
#define min(a, b)    (((a) > (b)) ? (b) : (a))
#endif

/**
 * Provides a legancy string to key function, used in PEM files.
 *
 * New protocols should use new string to key functions like NIST
 * SP56-800A or PKCS#5 v2.0 (see PKCS5_PBKDF2_HMAC_SHA1()).
 *
 * @param type type of cipher to use
 * @param md message digest to use
 * @param salt salt salt string, should be an binary 8 byte buffer.
 * @param data the password/input key string.
 * @param datalen length of data parameter.
 * @param count iteration counter.
 * @param keydata output keydata, needs to of the size EVP_CIPHER_key_length().
 * @param ivdata output ivdata, needs to of the size EVP_CIPHER_block_size().
 *
 * @return the size of derived key.
 */
int
EVP_BytesToKey(const EVP_CIPHER *type,
    const EVP_MD *md,
    const void *salt,
    const void *data, size_t datalen,
    unsigned int count,
    void *keydata,
    void *ivdata)
{
	unsigned int ivlen, keylen;
	int first = 0;
	unsigned int mds = 0, i;
	unsigned char *key = keydata;
	unsigned char *iv = ivdata;
	unsigned char *buf;
	EVP_MD_CTX c;

	keylen = EVP_CIPHER_key_length(type);
	ivlen = EVP_CIPHER_iv_length(type);

	if (data == NULL) {
		return (keylen);
	}

	buf = malloc(EVP_MD_size(md));
	if (buf == NULL) {
		return (-1);
	}

	EVP_MD_CTX_init(&c);

	first = 1;
	while (1) {
		EVP_DigestInit_ex(&c, md, NULL);
		if (!first) {
			EVP_DigestUpdate(&c, buf, mds);
		}
		first = 0;
		EVP_DigestUpdate(&c, data, datalen);

#define PKCS5_SALT_LEN    8

		if (salt) {
			EVP_DigestUpdate(&c, salt, PKCS5_SALT_LEN);
		}

		EVP_DigestFinal_ex(&c, buf, &mds);
		assert(mds == EVP_MD_size(md));

		for (i = 1; i < count; i++) {
			EVP_DigestInit_ex(&c, md, NULL);
			EVP_DigestUpdate(&c, buf, mds);
			EVP_DigestFinal_ex(&c, buf, &mds);
			assert(mds == EVP_MD_size(md));
		}

		i = 0;
		if (keylen) {
			size_t sz = min(keylen, mds);
			if (key) {
				memcpy(key, buf, sz);
				key += sz;
			}
			keylen -= sz;
			i += sz;
		}
		if (ivlen && (mds > i)) {
			size_t sz = min(ivlen, (mds - i));
			if (iv) {
				memcpy(iv, &buf[i], sz);
				iv += sz;
			}
			ivlen -= sz;
		}
		if ((keylen == 0) && (ivlen == 0)) {
			break;
		}
	}

	EVP_MD_CTX_cleanup(&c);
	free(buf);

	return (EVP_CIPHER_key_length(type));
}


static char prompt_string[80];

void
EVP_set_pw_prompt(const char *prompt)
{
	if (prompt == NULL) {
		prompt_string[0] = '\0';
	} else {
		strncpy(prompt_string, prompt, 79);
		prompt_string[79] = '\0';
	}
}


char *
EVP_get_pw_prompt(void)
{
	if (prompt_string[0] == '\0') {
		return (NULL);
	} else{
		return (prompt_string);
	}
}


int
EVP_read_pw_string(char *buf, int len, const char *prompt, int verify)
{
	if ((prompt == NULL) && (prompt_string[0] != '\0')) {
		prompt = prompt_string;
	}

	return (UI_UTIL_read_pw_string(buf, len, prompt, verify));
}


/**
 * Generate a random key for the specificed EVP_CIPHER.
 *
 * @param ctx EVP_CIPHER_CTX type to build the key for.
 * @param key return key, must be at least EVP_CIPHER_key_length() byte long.
 *
 * @return 1 for success, 0 for failure.
 */
int
EVP_CIPHER_CTX_rand_key(EVP_CIPHER_CTX *ctx, void *key)
{
	if (ctx->cipher->flags & EVP_CIPH_RAND_KEY) {
		return (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_RAND_KEY, 0, key));
	}
	if (CCRandomCopyBytes(kCCRandomDefault, key, ctx->key_len) != 0) {
		return (0);
	}
	return (1);
}


/**
 * Perform a operation on a ctx
 *
 * @param ctx context to perform operation on.
 * @param type type of operation.
 * @param arg argument to operation.
 * @param data addition data to operation.
 *
 * @return 1 for success, 0 for failure.
 */
int
EVP_CIPHER_CTX_ctrl(EVP_CIPHER_CTX *ctx, int type, int arg, void *data)
{
	if ((ctx->cipher == NULL) || (ctx->cipher->ctrl == NULL)) {
		return (0);
	}
	return ((*ctx->cipher->ctrl)(ctx, type, arg, data));
}


static void ossl_EVP_PKEY_free_it(EVP_PKEY *x);

#define EVP_PKEY_assign_RSA(pkey, rsa)		EVP_PKEY_assign((pkey), EVP_PKEY_RSA, (char *)(rsa))
#define EVP_PKEY_assign_DSA(pkey, dsa)		EVP_PKEY_assign((pkey), EVP_PKEY_DSA, (char *)(dsa))
#define EVP_PKEY_assign_DH(pkey, dh)		EVP_PKEY_assign((pkey), EVP_PKEY_DH, (char *)(dh))

int
EVP_PKEY_assign(EVP_PKEY *pkey, int type, char *key)
{
	if (pkey == NULL) {
		return (0);
	}
	if (pkey->pkey.ptr != NULL) {
		ossl_EVP_PKEY_free_it(pkey);
	}
	pkey->type = EVP_PKEY_type(type);
	pkey->save_type = type;
	pkey->pkey.ptr = key;
	return (key != NULL);
}


RSA *
EVP_PKEY_get1_RSA(EVP_PKEY *pkey)
{
	if (pkey->type != EVP_PKEY_RSA) {
		return (NULL);
	}

	RSA_up_ref(pkey->pkey.rsa);
	return (pkey->pkey.rsa);
}


int
EVP_PKEY_set1_RSA(EVP_PKEY *pkey, RSA *key)
{
	int ret = EVP_PKEY_assign_RSA(pkey, key);

	if (ret) {
		RSA_up_ref(key);
	}

	return (ret);
}


DSA *
EVP_PKEY_get1_DSA(EVP_PKEY *pkey)
{
	if (pkey->type != EVP_PKEY_DSA) {
		return (NULL);
	}

	DSA_up_ref(pkey->pkey.dsa);
	return (pkey->pkey.dsa);
}


int
EVP_PKEY_set1_DSA(EVP_PKEY *pkey, DSA *key)
{
	int ret = EVP_PKEY_assign_DSA(pkey, key);

	if (ret) {
		DSA_up_ref(key);
	}

	return (ret);
}


int EVP_PKEY_type(int type)
{
	switch (type) {
	case EVP_PKEY_RSA:
	case EVP_PKEY_RSA2:
		return (EVP_PKEY_RSA);

	case EVP_PKEY_DSA:
	case EVP_PKEY_DSA1:
	case EVP_PKEY_DSA2:
	case EVP_PKEY_DSA3:
	case EVP_PKEY_DSA4:
		return (EVP_PKEY_DSA);

	case EVP_PKEY_DH:
		return (EVP_PKEY_DH);

	default:
		return (NID_undef);
	}
}


EVP_PKEY *
EVP_PKEY_new(void)
{
	EVP_PKEY *ret = (EVP_PKEY *)malloc(sizeof(EVP_PKEY));

	if (ret == NULL) {
		/* EVPerr(EVP_F_EVP_PKEY_NEW,ERR_R_MALLOC_FAILURE); */
		return (NULL);
	}
	ret->type = EVP_PKEY_NONE;
	ret->references = 1;
	ret->pkey.ptr = NULL;
#if 0
	ret->attributes = NULL;
#endif
	ret->save_parameters = 1;
	return (ret);
}


static void
ossl_EVP_PKEY_free_it(EVP_PKEY *x)
{
	switch (x->type) {
	case EVP_PKEY_RSA:
	case EVP_PKEY_RSA2:
		if (x->pkey.rsa) {
			RSA_free(x->pkey.rsa);
		}
		break;

	case EVP_PKEY_DSA:
	case EVP_PKEY_DSA2:
	case EVP_PKEY_DSA3:
	case EVP_PKEY_DSA4:
		if (x->pkey.dsa) {
			DSA_free(x->pkey.dsa);
		}
		break;

	case EVP_PKEY_DH:
		if (x->pkey.dh) {
			DH_free(x->pkey.dh);
		}
		break;
	}
}


void
EVP_PKEY_free(EVP_PKEY *x)
{
	int i;

	if (x == NULL) {
		return;
	}

	/* XXX not thread safe. */
	x->references--;
	if (i > 0) {
		return;
	}
	ossl_EVP_PKEY_free_it(x);
#if 0
	if (x->attributes) {
		sk_X509_ATTRIBUTE_pop_free(x->attributes, X509_ATTRIBUTE_free);
	}
#endif
	free(x);
}


/*
 *
 */

#define conv_bin2ascii(a)	(data_bin2ascii[(a) & 0x3f])
#define conv_ascii2bin(a)	(data_ascii2bin[(a) & 0x7f])

/* 64 char lines
 * pad input with 0
 * left over chars are set to =
 * 1 byte  => xx==
 * 2 bytes => xxx=
 * 3 bytes => xxxx
 */
#define BIN_PER_LINE		(64 / 4 * 3)
#define CHUNKS_PER_LINE		(64 / 4)
#define CHAR_PER_LINE		(64 + 1)

static unsigned char data_bin2ascii[65] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ\
abcdefghijklmnopqrstuvwxyz0123456789+/";

/* 0xF0 is a EOLN
 * 0xF1 is ignore but next needs to be 0xF0 (for \r\n processing).
 * 0xF2 is EOF
 * 0xE0 is ignore at start of line.
 * 0xFF is error
 */

#define B64_EOLN	0xF0
#define B64_CR		0xF1
#define B64_EOF		0xF2
#define B64_WS		0xE0
#define B64_ERROR	0xFF
#define B64_NOT_BASE64(a)    (((a)|0x13) == 0xF3)

static unsigned char data_ascii2bin[128] =
{
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xE0, 0xF0, 0xFF, 0xFF, 0xF1, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xE0, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0x3E, 0xFF, 0xF2, 0xFF, 0x3F,
	0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B,
	0x3C, 0x3D, 0xFF, 0xFF, 0xFF, 0x00, 0xFF, 0xFF,
	0xFF, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
	0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E,
	0x0F, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16,
	0x17, 0x18, 0x19, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20,
	0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28,
	0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F, 0x30,
	0x31, 0x32, 0x33, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
};

void
EVP_EncodeInit(EVP_ENCODE_CTX *ctx)
{
	ctx->length = 48;
	ctx->num = 0;
	ctx->line_num = 0;
}


void
EVP_EncodeUpdate(EVP_ENCODE_CTX *ctx, unsigned char *out, int *outl,
    const unsigned char *in, int inl)
{
	int i, j;
	unsigned int total = 0;

	*outl = 0;
	if (inl == 0) {
		return;
	}
	/* assert(ctx->length <= (int)sizeof(ctx->enc_data)); */
	if ((ctx->num+inl) < ctx->length) {
		memcpy(&(ctx->enc_data[ctx->num]), in, inl);
		ctx->num += inl;
		return;
	}
	if (ctx->num != 0) {
		i = ctx->length-ctx->num;
		memcpy(&(ctx->enc_data[ctx->num]), in, i);
		in += i;
		inl -= i;
		j = EVP_EncodeBlock(out, ctx->enc_data, ctx->length);
		ctx->num = 0;
		out += j;
		*(out++) = '\n';
		*out = '\0';
		total = j + 1;
	}
	while (inl >= ctx->length) {
		j = EVP_EncodeBlock(out, in, ctx->length);
		in += ctx->length;
		inl -= ctx->length;
		out += j;
		*(out++) = '\n';
		*out = '\0';
		total += j + 1;
	}
	if (inl != 0) {
		memcpy(&(ctx->enc_data[0]), in, inl);
	}
	ctx->num = inl;
	*outl = total;
}


void
EVP_EncodeFinal(EVP_ENCODE_CTX *ctx, unsigned char *out, int *outl)
{
	unsigned int ret = 0;

	if (ctx->num != 0) {
		ret = EVP_EncodeBlock(out, ctx->enc_data, ctx->num);
		out[ret++] = '\n';
		out[ret] = '\0';
		ctx->num = 0;
	}
	*outl = ret;
}


int
EVP_EncodeBlock(unsigned char *t, const unsigned char *f, int dlen)
{
	int i, ret = 0;
	unsigned long l;

	for (i = dlen; i > 0; i -= 3) {
		if (i >= 3) {
			l = (((unsigned long)f[0])<<16L) |
			    (((unsigned long)f[1])<< 8L)|f[2];
			*(t++) = conv_bin2ascii(l>>18L);
			*(t++) = conv_bin2ascii(l>>12L);
			*(t++) = conv_bin2ascii(l>> 6L);
			*(t++) = conv_bin2ascii(l);
		} else {
			l = ((unsigned long)f[0])<<16L;
			if (i == 2) {
				l |= ((unsigned long)f[1] << 8L);
			}

			*(t++) = conv_bin2ascii(l>>18L);
			*(t++) = conv_bin2ascii(l>>12L);
			*(t++) = (i == 1) ? '=' : conv_bin2ascii(l>>6L);
			*(t++) = '=';
		}
		ret += 4;
		f += 3;
	}

	*t = '\0';
	return (ret);
}


void
EVP_DecodeInit(EVP_ENCODE_CTX *ctx)
{
	ctx->length = 30;
	ctx->num = 0;
	ctx->line_num = 0;
	ctx->expect_nl = 0;
}


/* -1 for error
 *  0 for last line
 *  1 for full line
 */
int
EVP_DecodeUpdate(EVP_ENCODE_CTX *ctx, unsigned char *out, int *outl,
    const unsigned char *in, int inl)
{
	int seof = -1, eof = 0, rv = -1, ret = 0, i, v, tmp, n, ln, exp_nl;
	unsigned char *d;

	n = ctx->num;
	d = ctx->enc_data;
	ln = ctx->line_num;
	exp_nl = ctx->expect_nl;

	/* last line of input. */
	if ((inl == 0) || ((n == 0) && (conv_ascii2bin(in[0]) == B64_EOF))) {
		rv = 0;
		goto end;
	}

	/* We parse the input data */
	for (i = 0; i < inl; i++) {
		/* If the current line is > 80 characters, scream alot */
		if (ln >= 80) {
			rv = -1;
			goto end;
		}

		/* Get char and put it into the buffer */
		tmp = *(in++);
		v = conv_ascii2bin(tmp);
		/* only save the good data :-) */
		if (!B64_NOT_BASE64(v)) {
			/* assert(n < (int)sizeof(ctx->enc_data)); */
			d[n++] = tmp;
			ln++;
		} else if (v == B64_ERROR) {
			rv = -1;
			goto end;
		}

		/* have we seen a '=' which is 'definitly' the last
		 * input line.  seof will point to the character that
		 * holds it. and eof will hold how many characters to
		 * chop off. */
		if (tmp == '=') {
			if (seof == -1) {
				seof = n;
			}
			eof++;
		}

		if (v == B64_CR) {
			ln = 0;
			if (exp_nl) {
				continue;
			}
		}

		/* eoln */
		if (v == B64_EOLN) {
			ln = 0;
			if (exp_nl) {
				exp_nl = 0;
				continue;
			}
		}
		exp_nl = 0;

		/* If we are at the end of input and it looks like a
		 * line, process it. */
		if (((i+1) == inl) && (((n&3) == 0) || eof)) {
			v = B64_EOF;

			/* In case things were given us in really small
			 * records (so two '=' were given in separate
			 * updates), eof may contain the incorrect number
			 * of ending bytes to skip, so let's redo the count */
			eof = 0;
			if (d[n-1] == '=') {
				eof++;
			}
			if (d[n-2] == '=') {
				eof++;
			}
			/* There will never be more than two '=' */
		}

		if (((v == B64_EOF) && ((n&3) == 0)) || (n >= 64)) {
			/* This is needed to work correctly on 64 byte input
			 * lines.  We process the line and then need to
			 * accept the '\n' */
			if ((v != B64_EOF) && (n >= 64)) {
				exp_nl = 1;
			}
			if (n > 0) {
				v = EVP_DecodeBlock(out, d, n);
				n = 0;
				if (v < 0) {
					rv = 0;
					goto end;
				}
				ret += (v-eof);
			} else {
				eof = 1;
				v = 0;
			}

			/* This is the case where we have had a short
			 * but valid input line */
			if ((v < ctx->length) && eof) {
				rv = 0;
				goto end;
			} else{
				ctx->length = v;
			}

			if (seof >= 0) {
				rv = 0;
				goto end;
			}
			out += v;
		}
	}
	rv = 1;
end:
	*outl = ret;
	ctx->num = n;
	ctx->line_num = ln;
	ctx->expect_nl = exp_nl;
	return (rv);
}


int
EVP_DecodeBlock(unsigned char *t, const unsigned char *f, int n)
{
	int i, ret = 0, a, b, c, d;
	unsigned long l;

	/* trim white space from the start of the line. */
	while ((conv_ascii2bin(*f) == B64_WS) && (n > 0)) {
		f++;
		n--;
	}

	/* strip off stuff at the end of the line
	 * ascii2bin values B64_WS, B64_EOLN, B64_EOLN and B64_EOF */
	while ((n > 3) && (B64_NOT_BASE64(conv_ascii2bin(f[n-1])))) {
		n--;
	}

	if (n % 4 != 0) {
		return (-1);
	}

	for (i = 0; i < n; i += 4) {
		a = conv_ascii2bin(*(f++));
		b = conv_ascii2bin(*(f++));
		c = conv_ascii2bin(*(f++));
		d = conv_ascii2bin(*(f++));
		if ((a & 0x80) || (b & 0x80) || (c & 0x80) || (d & 0x80)) {
			return (-1);
		}
		l = ((((unsigned long)a) << 18L) |
		    (((unsigned long)b) << 12L) |
		    (((unsigned long)c) << 6L)  |
		    (((unsigned long)d)));
		*(t++) = (unsigned char)(l >> 16L) & 0xff;
		*(t++) = (unsigned char)(l >> 8L) & 0xff;
		*(t++) = (unsigned char)(l) & 0xff;
		ret += 3;
	}
	return (ret);
}


int
EVP_DecodeFinal(EVP_ENCODE_CTX *ctx, unsigned char *out, int *outl)
{
	int i;

	*outl = 0;
	if (ctx->num != 0) {
		i = EVP_DecodeBlock(out, ctx->enc_data, ctx->num);
		if (i < 0) {
			return (-1);
		}
		ctx->num = 0;
		*outl = i;
		return (1);
	} else{
		return (1);
	}
}

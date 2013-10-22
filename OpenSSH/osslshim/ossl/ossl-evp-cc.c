/*
 * Copyright (c) 2011 Apple Inc. All Rights Reserved.
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
 * Copyright (c) 2008 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 *
 * Portions Copyright (c) 2009 Apple Inc. All rights reserved.
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

/* CommonCrypto shim provider */

#ifdef __APPLE__

#include "ossl-config.h"

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#ifdef HAVE_COMMONCRYPTO_COMMONDIGEST_H
#include <CommonCrypto/CommonDigest.h>
#endif
#ifdef HAVE_COMMONCRYPTO_COMMONDIGESTSPI_H
#include <CommonCrypto/CommonDigestSPI.h>
#endif
#ifdef HAVE_COMMONCRYPTO_COMMONCRYPTOR_H
#include <CommonCrypto/CommonCryptor.h>
#endif

#include "ossl-objects.h"
#include "ossl-evp.h"
#include "ossl-evp-cc.h"
#include "ossl-engine.h"

/*
 *
 */

#ifdef HAVE_COMMONCRYPTO_COMMONCRYPTOR_H

struct cc_key {
	CCCryptorRef href;
};

static int
cc_do_cipher(EVP_CIPHER_CTX *ctx,
    unsigned char *out,
    const unsigned char *in,
    unsigned int size)
{
	struct cc_key *cc = ctx->cipher_data;
	CCCryptorStatus ret;
	size_t moved;

	memcpy(out, in, size);

	ret = CCCryptorUpdate(cc->href, in, size, out, size, &moved);
	if (ret) {
		return (0);
	}

	if (moved != size) {
		return (0);
	}

	return (1);
}


static int
init_cc_key(int encp, CCAlgorithm alg, CCOptions opts, const void *key,
    size_t keylen, const void *iv, CCCryptorRef *ref)
{
	CCOperation op = encp ? kCCEncrypt : kCCDecrypt;
	CCCryptorStatus ret;

	if (*ref) {
		if ((key == NULL) && iv) {
			CCCryptorReset(*ref, iv);
			return (1);
		}
		CCCryptorRelease(*ref);
	}

	ret = CCCryptorCreate(op, alg, opts, key, keylen, iv, ref);
	if (ret) {
		return (0);
	}
	return (1);
}


#ifdef USE_COMMONCRYPTO_CBC_MODE
static int
cc_do_cbc_cipher(EVP_CIPHER_CTX *ctx,
    unsigned char *out,
    const unsigned char *in,
    unsigned int size)
{
	return (cc_do_cipher(ctx, out, in, size));
}


#else

/*
 * We implement our own CBC mode so OpenSSH can get at the IV state.
 */
static int
cc_do_cbc_cipher(EVP_CIPHER_CTX *ctx,
    unsigned char *out,
    const unsigned char *in,
    unsigned int size)
{
	unsigned int n;
	unsigned int len = size;
	unsigned char *iv = ctx->iv;
	unsigned int block_size = (unsigned int)ctx->cipher->block_size;
	size_t iv_len = (size_t)ctx->cipher->iv_len;
	unsigned char tmp[EVP_MAX_BLOCK_LENGTH];

	if (ctx->encrypt) {
		/* Encrypt case */
		while (len >= block_size) {
			for (n = 0; n < block_size; ++n) {
				out[n] = in[n] ^ iv[n];
			}
			if (cc_do_cipher(ctx, out, out, block_size) == 0) {
				return (0);
			}
			iv = out;
			len -= block_size;
			in += block_size;
			out += block_size;
		}
		if (len) {
			for (n = 0; n < len; ++n) {
				out[n] = in[n] ^ iv[n];
			}
			for (n = len; n < block_size; ++n) {
				out[n] = iv[n];
			}
			if (cc_do_cipher(ctx, out, out, block_size) == 0) {
				return (0);
			}
			iv = out;
		}
		memcpy(ctx->iv, iv, iv_len);
	} else if (in != out) {
		/* Decrypt case, in/out buffers are different */
		while (len >= block_size) {
			if (cc_do_cipher(ctx, out, in, block_size) == 0) {
				return (0);
			}
			for (n = 0; n < block_size; ++n) {
				out[n] ^= iv[n];
			}
			iv = (unsigned char *)in;
			len -= block_size;
			in += block_size;
			out += block_size;
		}
		if (len) {
			if (cc_do_cipher(ctx, tmp, in, block_size) == 0) {
				return (0);
			}
			for (n = 0; n < len; ++n) {
				out[n] = tmp[n] ^ iv[n];
			}
			iv = (unsigned char *)in;
		}
		memcpy(ctx->iv, iv, iv_len);
	} else {
		/* Decrypt case, in/out buffers are same */
		while (len >= block_size) {
			memcpy(tmp, in, block_size);
			if (cc_do_cipher(ctx, out, tmp, block_size) == 0) {
				return (0);
			}
			for (n = 0; n < block_size; ++n) {
				out[n] ^= iv[n];
			}
			memcpy(iv, tmp, iv_len);
			len -= block_size;
			in += block_size;
			out += block_size;
		}
		if (len) {
			memcpy(tmp, in, block_size);
			if (cc_do_cipher(ctx, out, tmp, block_size) == 0) {
				return (0);
			}
			for (n = 0; n < len; ++n) {
				out[n] ^= iv[n];
			}
			for (n = len; n < block_size; ++n) {
				out[n] = tmp[n];
			}
			memcpy(iv, tmp, iv_len);
		}
		memcpy(ctx->iv, iv, iv_len);
	}

	return (1);
}


#endif /* ! USE_COMMONCRYPTO_CBC_MODE */

static int
cc_do_cfb8_cipher(EVP_CIPHER_CTX *ctx,
    unsigned char *out,
    const unsigned char *in,
    unsigned int size)
{
	struct cc_key *cc = ctx->cipher_data;
	CCCryptorStatus ret;
	size_t moved;
	unsigned int i;

	for (i = 0; i < size; i++) {
		unsigned char oiv[EVP_MAX_IV_LENGTH + 1];

		assert(ctx->cipher->iv_len + 1 <= sizeof(oiv));
		memcpy(oiv, ctx->iv, ctx->cipher->iv_len);

		ret = CCCryptorUpdate(cc->href, ctx->iv, ctx->cipher->iv_len,
			ctx->iv, ctx->cipher->iv_len, &moved);
		if (ret) {
			return (0);
		}

		if (moved != ctx->cipher->iv_len) {
			return (0);
		}

		if (!ctx->encrypt) {
			oiv[ctx->cipher->iv_len] = in[i];
		}
		out[i] = in[i] ^ ctx->iv[0];
		if (ctx->encrypt) {
			oiv[ctx->cipher->iv_len] = out[i];
		}

		memcpy(ctx->iv, &oiv[1], ctx->cipher->iv_len);
	}

	return (1);
}


static int
cc_cleanup(EVP_CIPHER_CTX *ctx)
{
	struct cc_key *cc = ctx->cipher_data;

	if (cc->href) {
		CCCryptorRelease(cc->href);
	}
	return (1);
}


static int
cc_des_ede3_ecb_init(EVP_CIPHER_CTX *ctx,
    const unsigned char *key,
    const unsigned char *iv,
    int encp)
{
	struct cc_key *cc = ctx->cipher_data;

	return (init_cc_key(encp, kCCAlgorithm3DES, kCCOptionECBMode, key, kCCKeySize3DES, iv, &cc->href));
}


#ifdef USE_COMMONCRYPTO_CBC_MODE
static int
cc_des_ede3_cbc_init(EVP_CIPHER_CTX *ctx,
    const unsigned char *key,
    const unsigned char *iv,
    int encp)
{
	struct cc_key *cc = ctx->cipher_data;

	return (init_cc_key(encp, kCCAlgorithm3DES, 0, key, kCCKeySize3DES, iv, &cc->href));
}


#else

static int
cc_des_ede3_cbc_init(EVP_CIPHER_CTX *ctx,
    const unsigned char *key,
    const unsigned char *iv,
    int encp)
{
	return (cc_des_ede3_ecb_init(ctx, key, NULL, encp));
}


#endif  /* ! USE_COMMONCRYPTO_CBC_MODE */

#endif  /* HAVE_COMMONCRYPTO_COMMONCRYPTOR_H */

/**
 * The tripple DES cipher type (Apple CommonCrypto provider)
 *
 * @return the DES-EDE3-ECB EVP_CIPHER pointer.
 *
 * @ingroup hcrypto_evp
 */
const EVP_CIPHER *
EVP_cc_des_ede3_ecb(void)
{
#ifdef HAVE_COMMONCRYPTO_COMMONCRYPTOR_H
	static const EVP_CIPHER c =
	{
		.nid			= NID_des_ede3_ecb,
		.block_size		= kCCBlockSize3DES,
		.key_len		= kCCKeySize3DES,
		.iv_len			= kCCBlockSize3DES,
		.flags			= EVP_CIPH_ECB_MODE | EVP_CIPH_ALWAYS_CALL_INIT,
		.init			= cc_des_ede3_ecb_init,
		.do_cipher		= cc_do_cipher,
		.cleanup		= cc_cleanup,
		.ctx_size		= sizeof(struct cc_key),
		.set_asn1_parameters	= NULL,
		.get_asn1_parameters	= NULL,
		.ctrl			= NULL,
		.app_data		= NULL
	};
	return (&c);

#else
	return (NULL);
#endif
}


/**
 * The tripple DES cipher type (Apple CommonCrypto provider)
 *
 * @return the DES-EDE3-CBC EVP_CIPHER pointer.
 *
 * @ingroup hcrypto_evp
 */
const EVP_CIPHER *
EVP_cc_des_ede3_cbc(void)
{
#ifdef HAVE_COMMONCRYPTO_COMMONCRYPTOR_H
	static const EVP_CIPHER c =
	{
		.nid			= NID_des_ede3_cbc,
		.block_size		= kCCBlockSize3DES,
		.key_len		= kCCKeySize3DES,
		.iv_len			= kCCBlockSize3DES,
		.flags			= EVP_CIPH_CBC_MODE | EVP_CIPH_ALWAYS_CALL_INIT | EVP_CIPH_CUSTOM_IV,
		.init			= cc_des_ede3_cbc_init,
		.do_cipher		= cc_do_cbc_cipher,
		.cleanup		= cc_cleanup,
		.ctx_size		= sizeof(struct cc_key),
		.set_asn1_parameters	= NULL,
		.get_asn1_parameters	= NULL,
		.ctrl			= NULL,
		.app_data		= NULL
	};
	return (&c);

#else
	return (NULL);
#endif
}


#ifdef HAVE_COMMONCRYPTO_COMMONCRYPTOR_H

/*
 *
 */
static int
cc_des_ecb_init(EVP_CIPHER_CTX *ctx,
    const unsigned char *key,
    const unsigned char *iv,
    int encp)
{
	struct cc_key *cc = ctx->cipher_data;

	return (init_cc_key(encp, kCCAlgorithmDES, kCCOptionECBMode, key, kCCBlockSizeDES, iv, &cc->href));
}


#ifdef USE_COMMONCRYPTO_CBC_MODE
static int
cc_des_cbc_init(EVP_CIPHER_CTX *ctx,
    const unsigned char *key,
    const unsigned char *iv,
    int encp)
{
	struct cc_key *cc = ctx->cipher_data;

	return (init_cc_key(encp, kCCAlgorithmDES, 0, key, kCCBlockSizeDES, iv, &cc->href));
}


#else

static int
cc_des_cbc_init(EVP_CIPHER_CTX *ctx,
    const unsigned char *key,
    const unsigned char *iv,
    int encp)
{
	return (cc_des_ecb_init(ctx, key, NULL, encp));
}


#endif  /* USE_COMMONCRYPTO_CBC_MODE */
#endif  /* HAVE_COMMONCRYPTO_COMMONCRYPTOR_H */

/**
 * The DES cipher type (Apple CommonCrypto provider)
 *
 * @return the DES-ECB EVP_CIPHER pointer.
 *
 * @ingroup hcrypto_evp
 */
const EVP_CIPHER *
EVP_cc_des_ecb(void)
{
#ifdef HAVE_COMMONCRYPTO_COMMONCRYPTOR_H
	static const EVP_CIPHER c =
	{
		.nid			= NID_des_ecb,
		.block_size		= kCCBlockSizeDES,
		.key_len		= kCCKeySizeDES,
		.iv_len			= kCCBlockSizeDES,
		.flags			= EVP_CIPH_ECB_MODE | EVP_CIPH_ALWAYS_CALL_INIT,
		.init			= cc_des_ecb_init,
		.do_cipher		= cc_do_cipher,
		.cleanup		= cc_cleanup,
		.ctx_size		= sizeof(struct cc_key),
		.set_asn1_parameters	= NULL,
		.get_asn1_parameters	= NULL,
		.ctrl			= NULL,
		.app_data		= NULL
	};
	return (&c);

#else
	return (NULL);
#endif
}


/**
 * The DES cipher type (Apple CommonCrypto provider)
 *
 * @return the DES-CBC EVP_CIPHER pointer.
 *
 * @ingroup hcrypto_evp
 */
const EVP_CIPHER *
EVP_cc_des_cbc(void)
{
#ifdef HAVE_COMMONCRYPTO_COMMONCRYPTOR_H
	static const EVP_CIPHER c =
	{
		.nid			= NID_des_cbc,
		.block_size		= kCCBlockSizeDES,
		.key_len		= kCCKeySizeDES,
		.iv_len			= kCCBlockSizeDES,
		.flags			= EVP_CIPH_CBC_MODE|EVP_CIPH_ALWAYS_CALL_INIT|EVP_CIPH_CUSTOM_IV,
		.init			= cc_des_cbc_init,
		.do_cipher		= cc_do_cbc_cipher,
		.cleanup		= cc_cleanup,
		.ctx_size		= sizeof(struct cc_key),
		.set_asn1_parameters	= NULL,
		.get_asn1_parameters	= NULL,
		.ctrl			= NULL,
		.app_data		= NULL
	};
	return (&c);

#else
	return (NULL);
#endif
}


#ifdef HAVE_COMMONCRYPTO_COMMONCRYPTOR_H
static int
cc_bf_ecb_init(EVP_CIPHER_CTX *ctx,
    const unsigned char *key,
    const unsigned char *iv,
    int encp)
{
	struct cc_key *cc = ctx->cipher_data;

	return (init_cc_key(encp, kCCAlgorithmBlowfish, kCCOptionECBMode, key, ctx->cipher->key_len, iv, &cc->href));
}


#ifdef USE_COMMONCRYPTO_CBC_MODE
static int
cc_bf_cbc_init(EVP_CIPHER_CTX *ctx,
    const unsigned char *key,
    const unsigned char *iv,
    int encp)
{
	struct cc_key *cc = ctx->cipher_data;

	return (init_cc_key(encp, kCCAlgorithmBlowfish, 0, key, ctx->cipher->key_len, iv, &cc->href));
}


#else

static int
cc_bf_cbc_init(EVP_CIPHER_CTX *ctx,
    const unsigned char *key,
    const unsigned char *iv,
    int encp)
{
	return (cc_bf_ecb_init(ctx, key, NULL, encp));
}


#endif  /* ! USE_COMMONCRYPTO_CBC_MODE */

#endif  /* HAVE_COMMONCRYPTO_COMMONCRYPTOR_H */

const EVP_CIPHER *
EVP_cc_bf_cbc(void)
{
#ifdef HAVE_COMMONCRYPTO_COMMONCRYPTOR_H
	static const EVP_CIPHER c =
	{
		.nid			= NID_bf_cbc,
		.block_size		= 8 /* kCCBlockSizeBlowfish */,
		.key_len		= 16,
		.iv_len			= 8 /* kCCBlockSizeBlowfish */,
		.flags			= EVP_CIPH_VARIABLE_LENGTH | EVP_CIPH_ALWAYS_CALL_INIT |
					    EVP_CIPH_CUSTOM_IV | EVP_CIPH_CBC_MODE,
		.init			= cc_bf_cbc_init,
		.do_cipher		= cc_do_cbc_cipher,
		.cleanup		= cc_cleanup,
		.ctx_size		= sizeof(struct cc_key),
		.set_asn1_parameters	= NULL,
		.get_asn1_parameters	= NULL,
		.ctrl			= NULL,
		.app_data		= NULL
	};
	return (&c);

#else
	return (NULL);
#endif
}


const EVP_CIPHER *
EVP_cc_bf_ecb(void)
{
#ifdef HAVE_COMMONCRYPTO_COMMONCRYPTOR_H
	static const EVP_CIPHER c =
	{
		.nid			= NID_bf_ecb,
		.block_size		= kCCBlockSizeBlowfish,
		.key_len		= 8,
		.iv_len			= kCCBlockSizeBlowfish,
		.flags			= EVP_CIPH_VARIABLE_LENGTH | EVP_CIPH_ALWAYS_CALL_INIT | EVP_CIPH_ECB_MODE,
		.init			= cc_bf_ecb_init,
		.do_cipher		= cc_do_cipher,
		.cleanup		= cc_cleanup,
		.ctx_size		= sizeof(struct cc_key),
		.set_asn1_parameters	= NULL,
		.get_asn1_parameters	= NULL,
		.ctrl			= NULL,
		.app_data		= NULL
	};
	return (&c);

#else
	return (NULL);
#endif
}


#ifdef HAVE_COMMONCRYPTO_COMMONCRYPTOR_H
static int
cc_cast5_ecb_init(EVP_CIPHER_CTX *ctx,
    const unsigned char *key,
    const unsigned char *iv,
    int encp)
{
	struct cc_key *cc = ctx->cipher_data;

	return (init_cc_key(encp, kCCAlgorithmCAST, kCCOptionECBMode, key, ctx->key_len, iv, &cc->href));
}


#ifdef USE_COMMONCRYPTO_CBC_MODE
static int
cc_cast5_cbc_init(EVP_CIPHER_CTX *ctx,
    const unsigned char *key,
    const unsigned char *iv,
    int encp)
{
	struct cc_key *cc = ctx->cipher_data;

	return (init_cc_key(encp, kCCAlgorithmCAST, 0, key, ctx->key_len, iv, &cc->href));
}


#else

static int
cc_cast5_cbc_init(EVP_CIPHER_CTX *ctx,
    const unsigned char *key,
    const unsigned char *iv,
    int encp)
{
	return (cc_cast5_ecb_init(ctx, key, NULL, encp));
}


#endif  /* ! USE_COMMONCRYPTO_CBC_MODE */

#endif  /* HAVE_COMMONCRYPTO_COMMONCRYPTOR_H */

const EVP_CIPHER *
EVP_cc_cast5_ecb(void)
{
#ifdef HAVE_COMMONCRYPTO_COMMONCRYPTOR_H
	static const EVP_CIPHER c =
	{
		.nid			= NID_cast5_ecb,
		.block_size		= kCCBlockSizeCAST,
		.key_len		= 16,
		.iv_len			= kCCBlockSizeCAST,
		.flags			= EVP_CIPH_VARIABLE_LENGTH | EVP_CIPH_ALWAYS_CALL_INIT | EVP_CIPH_ECB_MODE,
		.init			= cc_cast5_ecb_init,
		.do_cipher		= cc_do_cipher,
		.cleanup		= cc_cleanup,
		.ctx_size		= sizeof(struct cc_key),
		.set_asn1_parameters	= NULL,
		.get_asn1_parameters	= NULL,
		.ctrl			= NULL,
		.app_data		= NULL
	};
	return (&c);

#else
	return (NULL);
#endif
}


const EVP_CIPHER *
EVP_cc_cast5_cbc(void)
{
#ifdef HAVE_COMMONCRYPTO_COMMONCRYPTOR_H
	static const EVP_CIPHER c =
	{
		.nid			= NID_cast5_cbc,
		.block_size		= kCCBlockSizeCAST,
		.key_len		= 16,
		.iv_len			= kCCBlockSizeCAST,
		.flags			= EVP_CIPH_VARIABLE_LENGTH | EVP_CIPH_ALWAYS_CALL_INIT |
					    EVP_CIPH_CUSTOM_IV | EVP_CIPH_CBC_MODE,
		.init			= cc_cast5_cbc_init,
		.do_cipher		= cc_do_cbc_cipher,
		.cleanup		= cc_cleanup,
		.ctx_size		= sizeof(struct cc_key),
		.set_asn1_parameters	= NULL,
		.get_asn1_parameters	= NULL,
		.ctrl			= NULL,
		.app_data		= NULL
	};
	return (&c);

#else
	return (NULL);
#endif
}


#ifdef HAVE_COMMONCRYPTO_COMMONCRYPTOR_H

/*
 *
 */
static int
cc_aes_ecb_init(EVP_CIPHER_CTX *ctx,
    const unsigned char *key,
    const unsigned char *iv,
    int encp)
{
	struct cc_key *cc = ctx->cipher_data;

	return (init_cc_key(encp, kCCAlgorithmAES128, kCCOptionECBMode, key, ctx->cipher->key_len, iv, &cc->href));
}


#ifdef USE_COMMONCRYPTO_CBC_MODE
static int
cc_aes_cbc_init(EVP_CIPHER_CTX *ctx,
    const unsigned char *key,
    const unsigned char *iv,
    int encp)
{
	struct cc_key *cc = ctx->cipher_data;

	return (init_cc_key(encp, kCCAlgorithmAES128, 0, key, ctx->cipher->key_len, iv, &cc->href));
}


#else

static int
cc_aes_cbc_init(EVP_CIPHER_CTX *ctx,
    const unsigned char *key,
    const unsigned char *iv,
    int encp)
{
	return (cc_aes_ecb_init(ctx, key, NULL, encp));
}


#endif  /* ! USE_COMMONCRYPTO_CBC_MODE */

#endif  /* HAVE_COMMONCRYPTO_COMMONCRYPTOR_H */

/**
 * The AES-128 cipher type (Apple CommonCrypto provider)
 *
 * @return the AES-128-ECB EVP_CIPHER pointer.
 */
const EVP_CIPHER *
EVP_cc_aes_128_ecb(void)
{
#ifdef HAVE_COMMONCRYPTO_COMMONCRYPTOR_H
	static const EVP_CIPHER c =
	{
		.nid			= NID_aes_128_ecb,
		.block_size		= kCCBlockSizeAES128,
		.key_len		= kCCKeySizeAES128,
		.iv_len			= kCCBlockSizeAES128,
		.flags			= EVP_CIPH_ECB_MODE | EVP_CIPH_ALWAYS_CALL_INIT,
		.init			= cc_aes_ecb_init,
		.do_cipher		= cc_do_cipher,
		.cleanup		= cc_cleanup,
		.ctx_size		= sizeof(struct cc_key),
		.set_asn1_parameters	= NULL,
		.get_asn1_parameters	= NULL,
		.ctrl			= NULL,
		.app_data		= NULL
	};
	return (&c);

#else
	return (NULL);
#endif
}


/**
 * The AES-128 cipher type (Apple CommonCrypto provider)
 *
 * @return the AES-128-CBC EVP_CIPHER pointer.
 */
const EVP_CIPHER *
EVP_cc_aes_128_cbc(void)
{
#ifdef HAVE_COMMONCRYPTO_COMMONCRYPTOR_H
	static const EVP_CIPHER c =
	{
		.nid			= NID_aes_128_cbc,
		.block_size		= kCCBlockSizeAES128,
		.key_len		= kCCKeySizeAES128,
		.iv_len			= kCCBlockSizeAES128,
		.flags			= EVP_CIPH_CBC_MODE|EVP_CIPH_ALWAYS_CALL_INIT|EVP_CIPH_CUSTOM_IV,
		.init			= cc_aes_cbc_init,
		.do_cipher		= cc_do_cbc_cipher,
		.cleanup		= cc_cleanup,
		.ctx_size		= sizeof(struct cc_key),
		.set_asn1_parameters	= NULL,
		.get_asn1_parameters	= NULL,
		.ctrl			= NULL,
		.app_data		= NULL
	};
	return (&c);

#else
	return (NULL);
#endif
}


/**
 * The AES-192 cipher type (Apple CommonCrypto provider)
 *
 * @return the AES-192-ECB EVP_CIPHER pointer.
 */
const EVP_CIPHER *
EVP_cc_aes_192_ecb(void)
{
#ifdef HAVE_COMMONCRYPTO_COMMONCRYPTOR_H
	static const EVP_CIPHER c =
	{
		.nid			= NID_aes_192_ecb,
		.block_size		= kCCBlockSizeAES128,
		.key_len		= kCCKeySizeAES192,
		.iv_len			= kCCBlockSizeAES128,
		.flags			= EVP_CIPH_ECB_MODE|EVP_CIPH_ALWAYS_CALL_INIT,
		.init			= cc_aes_ecb_init,
		.do_cipher		= cc_do_cipher,
		.cleanup		= cc_cleanup,
		.ctx_size		= sizeof(struct cc_key),
		.set_asn1_parameters	= NULL,
		.get_asn1_parameters	= NULL,
		.ctrl			= NULL,
		.app_data		= NULL
	};
	return (&c);

#else
	return (NULL);
#endif
}


/**
 * The AES-192 cipher type (Apple CommonCrypto provider)
 *
 * @return the AES-192-CBC EVP_CIPHER pointer.
 */
const EVP_CIPHER *
EVP_cc_aes_192_cbc(void)
{
#ifdef HAVE_COMMONCRYPTO_COMMONCRYPTOR_H
	static const EVP_CIPHER c =
	{
		.nid			= NID_aes_192_cbc,
		.block_size		= kCCBlockSizeAES128,
		.key_len		= kCCKeySizeAES192,
		.iv_len			= kCCBlockSizeAES128,
		.flags			= EVP_CIPH_CBC_MODE|EVP_CIPH_ALWAYS_CALL_INIT|EVP_CIPH_CUSTOM_IV,
		.init			= cc_aes_cbc_init,
		.do_cipher		= cc_do_cbc_cipher,
		.cleanup		= cc_cleanup,
		.ctx_size		= sizeof(struct cc_key),
		.set_asn1_parameters	= NULL,
		.get_asn1_parameters	= NULL,
		.ctrl			= NULL,
		.app_data		= NULL
	};
	return (&c);

#else
	return (NULL);
#endif
}


/**
 * The AES-256 cipher type (Apple CommonCrypto provider)
 *
 * @return the AES-256-ECB EVP_CIPHER pointer.
 */
const EVP_CIPHER *
EVP_cc_aes_256_ecb(void)
{
#ifdef HAVE_COMMONCRYPTO_COMMONCRYPTOR_H
	static const EVP_CIPHER c =
	{
		.nid			= NID_aes_256_ecb,
		.block_size		= kCCBlockSizeAES128,
		.key_len		= kCCKeySizeAES256,
		.iv_len			= kCCBlockSizeAES128,
		.flags			= EVP_CIPH_ECB_MODE|EVP_CIPH_ALWAYS_CALL_INIT,
		.init			= cc_aes_ecb_init,
		.do_cipher		= cc_do_cipher,
		.cleanup		= cc_cleanup,
		.ctx_size		= sizeof(struct cc_key),
		.set_asn1_parameters	= NULL,
		.get_asn1_parameters	= NULL,
		.ctrl			= NULL,
		.app_data		= NULL
	};
	return (&c);

#else
	return (NULL);
#endif
}


/**
 * The AES-256 cipher type (Apple CommonCrypto provider)
 *
 * @return the AES-256-CBC EVP_CIPHER pointer.
 */
const EVP_CIPHER *
EVP_cc_aes_256_cbc(void)
{
#ifdef HAVE_COMMONCRYPTO_COMMONCRYPTOR_H
	static const EVP_CIPHER c =
	{
		.nid			= NID_aes_256_cbc,
		.block_size		= kCCBlockSizeAES128,
		.key_len		= kCCKeySizeAES256,
		.iv_len			= kCCBlockSizeAES128,
		.flags			= EVP_CIPH_CBC_MODE|EVP_CIPH_ALWAYS_CALL_INIT|EVP_CIPH_CUSTOM_IV,
		.init			= cc_aes_cbc_init,
		.do_cipher		= cc_do_cbc_cipher,
		.cleanup		= cc_cleanup,
		.ctx_size		= sizeof(struct cc_key),
		.set_asn1_parameters	= NULL,
		.get_asn1_parameters	= NULL,
		.ctrl			= NULL,
		.app_data		= NULL
	};
	return (&c);

#else
	return (NULL);
#endif
}


#ifdef HAVE_COMMONCRYPTO_COMMONCRYPTOR_H
static int
cc_aes_cfb8_init(EVP_CIPHER_CTX *ctx,
    const unsigned char *key,
    const unsigned char *iv,
    int encp)
{
	struct cc_key *cc = ctx->cipher_data;

	if (iv) {
		memcpy(ctx->iv, iv, ctx->cipher->iv_len);
	} else{
		memset(ctx->iv, 0, ctx->cipher->iv_len);
	}
	return (init_cc_key(1, kCCAlgorithmAES128, kCCOptionECBMode,
	       key, ctx->cipher->key_len, NULL, &cc->href));
}


#endif

/**
 * The AES-128 CFB8 cipher type (Apple CommonCrypto provider)
 *
 * @return the AES-128-CFB8 EVP_CIPHER pointer.
 */
const EVP_CIPHER *
EVP_cc_aes_128_cfb8(void)
{
#ifdef HAVE_COMMONCRYPTO_COMMONCRYPTOR_H
	static const EVP_CIPHER c =
	{
		.nid			= NID_aes_128_cfb1,
		.block_size		= 1,
		.key_len		= kCCKeySizeAES128,
		.iv_len			= kCCBlockSizeAES128,
		.flags			= EVP_CIPH_CFB8_MODE | EVP_CIPH_ALWAYS_CALL_INIT,
		.init			= cc_aes_cfb8_init,
		.do_cipher		= cc_do_cfb8_cipher,
		.cleanup		= cc_cleanup,
		.ctx_size		= sizeof(struct cc_key),
		.set_asn1_parameters	= NULL,
		.get_asn1_parameters	= NULL,
		.ctrl			= NULL,
		.app_data		= NULL
	};
	return (&c);

#else
	return (NULL);
#endif
}


/**
 * The AES-192 CFB8 cipher type (Apple CommonCrypto provider)
 *
 * @return the AES-192-CFB8 EVP_CIPHER pointer.
 */
const EVP_CIPHER *
EVP_cc_aes_192_cfb8(void)
{
#ifdef HAVE_COMMONCRYPTO_COMMONCRYPTOR_H
	static const EVP_CIPHER c =
	{
		.nid			= NID_aes_192_cfb1,
		.block_size		= 1,
		.key_len		= kCCKeySizeAES192,
		.iv_len			= kCCBlockSizeAES128,
		.flags			= EVP_CIPH_CFB8_MODE|EVP_CIPH_ALWAYS_CALL_INIT,
		.init			= cc_aes_cfb8_init,
		.do_cipher		= cc_do_cfb8_cipher,
		.cleanup		= cc_cleanup,
		.ctx_size		= sizeof(struct cc_key),
		.set_asn1_parameters	= NULL,
		.get_asn1_parameters	= NULL,
		.ctrl			= NULL,
		.app_data		= NULL
	};
	return (&c);

#else
	return (NULL);
#endif
}


/**
 * The AES-256 CFB8 cipher type (Apple CommonCrypto provider)
 *
 * @return the AES-256-CFB8 EVP_CIPHER pointer.
 */
const EVP_CIPHER *
EVP_cc_aes_256_cfb8(void)
{
#ifdef HAVE_COMMONCRYPTO_COMMONCRYPTOR_H
	static const EVP_CIPHER c =
	{
		.nid			= NID_aes_256_cfb1,
		.block_size		= 1,
		.key_len		= kCCKeySizeAES256,
		.iv_len			= kCCBlockSizeAES128,
		.flags			= EVP_CIPH_CFB8_MODE|EVP_CIPH_ALWAYS_CALL_INIT,
		.init			= cc_aes_cfb8_init,
		.do_cipher		= cc_do_cfb8_cipher,
		.cleanup		= cc_cleanup,
		.ctx_size		= sizeof(struct cc_key),
		.set_asn1_parameters	= NULL,
		.get_asn1_parameters	= NULL,
		.ctrl			= NULL,
		.app_data		= NULL
	};
	return (&c);

#else
	return (NULL);
#endif
}


/*
 *
 */

#ifdef COMMONCRYPTO_SUPPORTS_RC2
static int
cc_rc2_cbc_init(EVP_CIPHER_CTX *ctx,
    const unsigned char *key,
    const unsigned char *iv,
    int encp)
{
	struct cc_key *cc = ctx->cipher_data;

	return (init_cc_key(encp, kCCAlgorithmRC2, 0, key, ctx->cipher->key_len, iv, &cc->href));
}


#endif

/**
 * The RC2 cipher type - common crypto
 *
 * @return the RC2 EVP_CIPHER pointer.
 */
const EVP_CIPHER *
EVP_cc_rc2_cbc(void)
{
#ifdef COMMONCRYPTO_SUPPORTS_RC2
	static const EVP_CIPHER rc2_cbc =
	{
		.nid			= NID_rc2_cbc,
		.block_size		= kCCBlockSizeRC2,
		.key_len		= 16,
		.iv_len			= kCCBlockSizeRC2,
		.flags			= EVP_CIPH_CBC_MODE|EVP_CIPH_ALWAYS_CALL_INIT,
		.init			= cc_rc2_cbc_init,
		.do_cipher		= cc_do_cipher,
		.cleanup		= cc_cleanup,
		.ctx_size		= sizeof(struct cc_key),
		.set_asn1_parameters	= NULL,
		.get_asn1_parameters	= NULL,
		.ctrl			= NULL,
		.app_data		= NULL
	};
	return (&rc2_cbc);

#else
	return (NULL);
#endif
}


/**
 * The RC2-40 cipher type - common crypto
 *
 * @return the RC2-40 EVP_CIPHER pointer.
 *
 */
const EVP_CIPHER *
EVP_cc_rc2_40_cbc(void)
{
#ifdef COMMONCRYPTO_SUPPORTS_RC2
	static const EVP_CIPHER rc2_40_cbc =
	{
		.nid			= NID_rc2_40_cbc,
		.block_size		= kCCBlockSizeRC2,
		.key_len		= 5,
		.iv_len			= kCCBlockSizeRC2,
		.flags			= EVP_CIPH_CBC_MODE|EVP_CIPH_ALWAYS_CALL_INIT,
		.init			= cc_rc2_cbc_init,
		.do_cipher		= cc_do_cipher,
		.cleanup		= cc_cleanup,
		.ctx_size		= sizeof(struct cc_key),
		.set_asn1_parameters	= NULL,
		.get_asn1_parameters	= NULL,
		.ctrl			= NULL,
		.app_data		= NULL
	};
	return (&rc2_40_cbc);

#else
	return (NULL);
#endif
}


/**
 * The RC2-64 cipher type - common crypto
 *
 * @return the RC2-64 EVP_CIPHER pointer.
 *
 */
const EVP_CIPHER *
EVP_cc_rc2_64_cbc(void)
{
#ifdef COMMONCRYPTO_SUPPORTS_RC2
	static const EVP_CIPHER rc2_64_cbc =
	{
		.nid			= NID_rc2_64_cbc,
		.block_size		= kCCBlockSizeRC2,
		.key_len		= 8,
		.iv_len			= kCCBlockSizeRC2,
		.flags			= EVP_CIPH_CBC_MODE|EVP_CIPH_ALWAYS_CALL_INIT,
		.init			= cc_rc2_cbc_init,
		.do_cipher		= cc_do_cipher,
		.cleanup		= cc_cleanup,
		.ctx_size		= sizeof(struct cc_key),
		.set_asn1_parameters	= NULL,
		.get_asn1_parameters	= NULL,
		.ctrl			= NULL,
		.app_data		= NULL
	};
	return (&rc2_64_cbc);

#else
	return (NULL);
#endif
}


/**
 * The CommonCrypto md2 provider
 *
 * @ingroup hcrypto_evp
 */
const EVP_MD *
EVP_cc_md2(void)
{
#ifdef HAVE_COMMONCRYPTO_COMMONDIGEST_H
	static const struct ossl_evp_md md2 =
	{
		.hash_size	= CC_MD2_DIGEST_LENGTH,
		.block_size	= CC_MD2_BLOCK_BYTES,
		.ctx_size	= sizeof(CC_MD2_CTX),
		.init		= (ossl_evp_md_init)CC_MD2_Init,
		.update		= (ossl_evp_md_update)CC_MD2_Update,
		.final		= (ossl_evp_md_final)CC_MD2_Final,
		.cleanup	= (ossl_evp_md_cleanup)NULL
	};
	return (&md2);

#else
	return (NULL);
#endif
}


/**
 * The CommonCrypto md4 provider
 *
 * @ingroup hcrypto_evp
 */
const EVP_MD *
EVP_cc_md4(void)
{
#ifdef HAVE_COMMONCRYPTO_COMMONDIGEST_H
	static const struct ossl_evp_md md4 =
	{
		.hash_size	= CC_MD4_DIGEST_LENGTH,
		.block_size	= CC_MD4_BLOCK_BYTES,
		.ctx_size	= sizeof(CC_MD4_CTX),
		.init		= (ossl_evp_md_init)CC_MD4_Init,
		.update		= (ossl_evp_md_update)CC_MD4_Update,
		.final		= (ossl_evp_md_final)CC_MD4_Final,
		.cleanup	= (ossl_evp_md_cleanup)NULL
	};
	return (&md4);

#else
	return (NULL);
#endif
}


/**
 * The CommonCrypto md5 provider
 *
 * @ingroup hcrypto_evp
 */
const EVP_MD *
EVP_cc_md5(void)
{
#ifdef HAVE_COMMONCRYPTO_COMMONDIGEST_H
	static const struct ossl_evp_md md5 =
	{
		.hash_size	= CC_MD5_DIGEST_LENGTH,
		.block_size	= CC_MD5_BLOCK_BYTES,
		.ctx_size	= sizeof(CC_MD5_CTX),
		.init		= (ossl_evp_md_init)CC_MD5_Init,
		.update		= (ossl_evp_md_update)CC_MD5_Update,
		.final		= (ossl_evp_md_final)CC_MD5_Final,
		.cleanup	= (ossl_evp_md_cleanup)NULL
	};
	return (&md5);

#else
	return (NULL);
#endif
}


/**
 * The CommonCrypto sha1 provider
 *
 * @ingroup hcrypto_evp
 */
const EVP_MD *
EVP_cc_sha1(void)
{
#ifdef HAVE_COMMONCRYPTO_COMMONDIGEST_H
	static const struct ossl_evp_md sha1 =
	{
		.hash_size	= CC_SHA1_DIGEST_LENGTH,
		.block_size	= CC_SHA1_BLOCK_BYTES,
		.ctx_size	= sizeof(CC_SHA1_CTX),
		.init		= (ossl_evp_md_init)CC_SHA1_Init,
		.update		= (ossl_evp_md_update)CC_SHA1_Update,
		.final		= (ossl_evp_md_final)CC_SHA1_Final,
		.cleanup	= (ossl_evp_md_cleanup)NULL
	};
	return (&sha1);

#else
	return (NULL);
#endif
}


/**
 * The CommonCrypto sha224 provider
 *
 * @ingroup hcrypto_evp
 */
const EVP_MD *
EVP_cc_sha224(void)
{
#ifdef HAVE_COMMONCRYPTO_COMMONDIGEST_H
	static const struct ossl_evp_md sha224 =
	{
		.hash_size	= CC_SHA224_DIGEST_LENGTH,
		.block_size	= CC_SHA224_BLOCK_BYTES,
		.ctx_size	= sizeof(CC_SHA256_CTX),
		.init		= (ossl_evp_md_init)CC_SHA224_Init,
		.update		= (ossl_evp_md_update)CC_SHA224_Update,
		.final		= (ossl_evp_md_final)CC_SHA224_Final,
		.cleanup	= (ossl_evp_md_cleanup)NULL
	};
	return (&sha224);

#else
	return (NULL);
#endif
}


/**
 * The CommonCrypto sha256 provider
 *
 * @ingroup hcrypto_evp
 */
const EVP_MD *
EVP_cc_sha256(void)
{
#ifdef HAVE_COMMONCRYPTO_COMMONDIGEST_H
	static const struct ossl_evp_md sha256 =
	{
		.hash_size	= CC_SHA256_DIGEST_LENGTH,
		.block_size	= CC_SHA256_BLOCK_BYTES,
		.ctx_size	= sizeof(CC_SHA256_CTX),
		.init		= (ossl_evp_md_init)CC_SHA256_Init,
		.update		= (ossl_evp_md_update)CC_SHA256_Update,
		.final		= (ossl_evp_md_final)CC_SHA256_Final,
		.cleanup	= (ossl_evp_md_cleanup)NULL
	};
	return (&sha256);

#else
	return (NULL);
#endif
}


/**
 * The CommonCrypto sha384 provider
 *
 * @ingroup hcrypto_evp
 */

/**
 * The CommonCrypto sha384 provider
 *
 */
const EVP_MD *
EVP_cc_sha384(void)
{
#ifdef HAVE_COMMONCRYPTO_COMMONDIGEST_H
	static const struct ossl_evp_md sha384 =
	{
		.hash_size	= CC_SHA384_DIGEST_LENGTH,
		.block_size	= CC_SHA384_BLOCK_BYTES,
		.ctx_size	= sizeof(CC_SHA512_CTX),
		.init		= (ossl_evp_md_init)CC_SHA384_Init,
		.update		= (ossl_evp_md_update)CC_SHA384_Update,
		.final		= (ossl_evp_md_final)CC_SHA384_Final,
		.cleanup	= (ossl_evp_md_cleanup)NULL
	};
	return (&sha384);

#else
	return (NULL);
#endif
}

/**
 * The CommonCrypto sha512 provider
 *
 */
const EVP_MD *
EVP_cc_sha512(void)
{
#ifdef HAVE_COMMONCRYPTO_COMMONDIGEST_H
	static const struct ossl_evp_md sha512 =
	{
		.hash_size	= CC_SHA512_DIGEST_LENGTH,
		.block_size	= CC_SHA512_BLOCK_BYTES,
		.ctx_size	= sizeof(CC_SHA512_CTX),
		.init		= (ossl_evp_md_init)CC_SHA512_Init,
		.update		= (ossl_evp_md_update)CC_SHA512_Update,
		.final		= (ossl_evp_md_final)CC_SHA512_Final,
		.cleanup	= (ossl_evp_md_cleanup)NULL
	};
	return (&sha512);

#else
	return (NULL);
#endif
}

/**
 * The CommonCrypto rmd128 provider
 *
 */
#ifdef HAVE_COMMONCRYPTO_COMMONDIGESTSPI_H
static int
CC_RMD128_Init(EVP_MD_CTX *c)
{
	return (CCDigestInit(kCCDigestRMD128, (CCDigestRef)c));
}


static int
CC_RMD_Update(EVP_MD_CTX *c, const void *data, size_t len)
{
	return (CCDigestUpdate((CCDigestRef)c, data, len));
}


static int
CC_RMD_Final(void *out, EVP_MD_CTX *c)
{
	return (CCDigestFinal((CCDigestRef)c, (uint8_t *)out));
}


#endif /* HAVE_COMMONCRYPTO_COMMONDIGESTSPI_H */

/**
 * The CommonCrypto rmd128 provider
 *
 * @ingroup hcrypto_evp
 */
const EVP_MD *
EVP_cc_rmd128(void)
{
#ifdef HAVE_COMMONCRYPTO_COMMONDIGESTSPI_H
	static const struct ossl_evp_md rmd128 =
	{
		.hash_size	= CC_RMD128_DIGEST_LENGTH,
		.block_size	= CC_RMD128_BLOCK_BYTES,
		.ctx_size	= sizeof(CCDigestCtx),
		.init		= (ossl_evp_md_init)CC_RMD128_Init,
		.update		= (ossl_evp_md_update)CC_RMD_Update,
		.final		= (ossl_evp_md_final)CC_RMD_Final,
		.cleanup	= (ossl_evp_md_cleanup)NULL
	};
	return (&rmd128);

#else
	return (NULL);
#endif
}


/**
 * The CommonCrypto rmd160 provider
 *
 * @ingroup hcrypto_evp
 */
#ifdef HAVE_COMMONCRYPTO_COMMONDIGESTSPI_H
static int
CC_RMD160_Init(EVP_MD_CTX *c)
{
	return (CCDigestInit(kCCDigestRMD160, (CCDigestRef)c));
}

#endif /* HAVE_COMMONCRYPTO_COMMONDIGESTSPI_H */

const EVP_MD *
EVP_cc_rmd160(void)
{
#ifdef HAVE_COMMONCRYPTO_COMMONDIGESTSPI_H
	static const struct ossl_evp_md rmd160 =
	{
		.hash_size	= CC_RMD160_DIGEST_LENGTH,
		.block_size	= CC_RMD160_BLOCK_BYTES,
		.ctx_size	= sizeof(CCDigestCtx),
		.init		= (ossl_evp_md_init)CC_RMD160_Init,
		.update		= (ossl_evp_md_update)CC_RMD_Update,
		.final		= (ossl_evp_md_final)CC_RMD_Final,
		.cleanup	= (ossl_evp_md_cleanup)NULL
	};
	return (&rmd160);

#else
	return (NULL);
#endif
}


/**
 * The CommonCrypto rmd256 provider
 */
#ifdef HAVE_COMMONCRYPTO_COMMONDIGESTSPI_H
static int
CC_RMD256_Init(EVP_MD_CTX *c)
{
	return (CCDigestInit(kCCDigestRMD256, (CCDigestRef)c));
}


#endif /* HAVE_COMMONCRYPTO_COMMONDIGESTSPI_H */

const EVP_MD *
EVP_cc_rmd256(void)
{
#ifdef HAVE_COMMONCRYPTO_COMMONDIGESTSPI_H
	static const struct ossl_evp_md rmd256 =
	{
		.hash_size	= CC_RMD256_DIGEST_LENGTH,
		.block_size	= CC_RMD256_BLOCK_BYTES,
		.ctx_size	= sizeof(CCDigestCtx),
		.init		= (ossl_evp_md_init)CC_RMD256_Init,
		.update		= (ossl_evp_md_update)CC_RMD_Update,
		.final		= (ossl_evp_md_final)CC_RMD_Final,
		.cleanup	= (ossl_evp_md_cleanup)NULL
	};
	return (&rmd256);

#else
	return (NULL);
#endif
}


/**
 * The CommonCrypto rmd256 provider
 */
#ifdef HAVE_COMMONCRYPTO_COMMONDIGESTSPI_H
static int
CC_RMD320_Init(EVP_MD_CTX *c)
{
	return (CCDigestInit(kCCDigestRMD320, (CCDigestRef)c));
}


#endif /* HAVE_COMMONCRYPTO_COMMONDIGESTSPI_H */

const EVP_MD *
EVP_cc_rmd320(void)
{
#ifdef HAVE_COMMONCRYPTO_COMMONDIGESTSPI_H
	static const struct ossl_evp_md rmd320 =
	{
		.hash_size	= CC_RMD320_DIGEST_LENGTH,
		.block_size	= CC_RMD320_BLOCK_BYTES,
		.ctx_size	= sizeof(CCDigestCtx),
		.init		= (ossl_evp_md_init)CC_RMD320_Init,
		.update		= (ossl_evp_md_update)CC_RMD_Update,
		.final		= (ossl_evp_md_final)CC_RMD_Final,
		.cleanup	= (ossl_evp_md_cleanup)NULL
	};
	return (&rmd320);

#else
	return (NULL);
#endif
}


/**
 * The Camellia-128 cipher type - CommonCrypto
 *
 * @return the Camellia-128 EVP_CIPHER pointer.
 *
 * @ingroup hcrypto_evp
 */
const EVP_CIPHER *
EVP_cc_camellia_128_cbc(void)
{
	return (NULL);
}


/**
 * The Camellia-198 cipher type - CommonCrypto
 *
 * @return the Camellia-198 EVP_CIPHER pointer.
 *
 * @ingroup hcrypto_evp
 */
const EVP_CIPHER *
EVP_cc_camellia_192_cbc(void)
{
	return (NULL);
}


/**
 * The Camellia-256 cipher type - CommonCrypto
 *
 * @return the Camellia-256 EVP_CIPHER pointer.
 *
 * @ingroup hcrypto_evp
 */
const EVP_CIPHER *
EVP_cc_camellia_256_cbc(void)
{
	return (NULL);
}


#ifdef HAVE_COMMONCRYPTO_COMMONCRYPTOR_H

/*
 *
 */

void CC_RC4_set_key(void *ks, int len, const unsigned char *data);
void CC_RC4(void *ks, unsigned long len, const unsigned char *indata,
    unsigned char *outdata);

#define data(ctx)    ((ctx)->cipher_data)

static int
cc_rc4_init(EVP_CIPHER_CTX *ctx,
    const unsigned char *key,
    const unsigned char *iv __unused,
    int encp __unused)
{
	void *ks = ctx->cipher_data;

	CC_RC4_set_key(ks, (int)EVP_CIPHER_CTX_key_length(ctx), key);
	return (1);
}


static int
cc_rc4_cipher(EVP_CIPHER_CTX *ctx, unsigned char *out,
    const unsigned char *in, unsigned int inl)
{
	void *ks = ctx->cipher_data;

	CC_RC4(ks, (unsigned long)inl, in, out);
	return (1);
}


#endif

/**
 *
 * The RC4 cipher type (Apple CommonCrypto provider)
 *
 * @return the RC4 EVP_CIPHER pointer.
 *
 * @ingroup hcrypto_evp
 */
const EVP_CIPHER *
EVP_cc_rc4(void)
{
#ifdef HAVE_COMMONCRYPTO_COMMONCRYPTOR_H
	static const EVP_CIPHER c =
	{
		.nid			= NID_rc4,
		.block_size		= 1,
		.key_len		= 16,
		.iv_len			= 0,
		.flags			= EVP_CIPH_STREAM_CIPHER | EVP_CIPH_VARIABLE_LENGTH,
		.init			= cc_rc4_init,
		.do_cipher		= cc_rc4_cipher,
		.cleanup		= NULL,
		.ctx_size		= kCCContextSizeRC4,
		.set_asn1_parameters	= NULL,
		.get_asn1_parameters	= NULL,
		.ctrl			= NULL,
		.app_data		= NULL
	};
	return (&c);

#else
	return (NULL);
#endif
}


/**
 * The RC4-40 cipher type (Apple CommonCrypto provider)
 *
 * @return the RC4 EVP_CIPHER pointer.
 *
 * @ingroup hcrypto_evp
 */
const EVP_CIPHER *
EVP_cc_rc4_40(void)
{
#ifdef HAVE_COMMONCRYPTO_COMMONCRYPTOR_H
	static const EVP_CIPHER c =
	{
		.nid			= NID_rc4_40,
		.block_size		= 1,
		.key_len		= 5 /* 40 bit */,
		.iv_len			= 0,
		.flags			= EVP_CIPH_STREAM_CIPHER | EVP_CIPH_VARIABLE_LENGTH,
		.init			= cc_rc4_init,
		.do_cipher		= cc_rc4_cipher,
		.cleanup		= NULL,
		.ctx_size		= kCCContextSizeRC4,
		.set_asn1_parameters	= NULL,
		.get_asn1_parameters	= NULL,
		.ctrl			= NULL,
		.app_data		= NULL
	};
	return (&c);

#else
	return (NULL);
#endif
}


#endif /* __APPLE__ */

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
 * Copyright (c) 2006 Kungliga Tekniska Högskolan
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

#ifndef _OSSL_RSA_H_
#define _OSSL_RSA_H_			1

/* symbol renaming */
#define RSA_null_method			ossl_RSA_null_method
#define RSA_imath_method		ossl_RSA_imath_method
#define RSA_cdsa_method			ossl_RSA_cdsa_method
#define RSA_tfm_method			ossl_RSA_tfm_method
#define RSA_ltm_method			ossl_RSA_ltm_method
#define RSA_gmp_method			ossl_RSA_gmp_method
#define RSA_tfm_method			ossl_RSA_tfm_method
#define RSA_new				ossl_RSA_new
#define RSA_new_method			ossl_RSA_new_method
#define RSA_free			ossl_RSA_free
#define RSA_up_ref			ossl_RSA_up_ref
#define RSA_set_default_method		ossl_RSA_set_default_method
#define RSA_get_default_method		ossl_RSA_get_default_method
#define RSA_set_method			ossl_RSA_set_method
#define RSA_get_method			ossl_RSA_get_method
#define RSA_set_app_data		ossl_RSA_set_app_data
#define RSA_get_app_data		ossl_RSA_get_app_data
#define RSA_check_key			ossl_RSA_check_key
#define RSA_size			ossl_RSA_size
#define RSA_public_encrypt		ossl_RSA_public_encrypt
#define RSA_public_decrypt		ossl_RSA_public_decrypt
#define RSA_private_encrypt		ossl_RSA_private_encrypt
#define RSA_private_decrypt		ossl_RSA_private_decrypt
#define RSA_sign			ossl_RSA_sign
#define RSA_verify			ossl_RSA_verify
#define RSA_generate_key_ex		ossl_RSA_generate_key_ex
#define d2i_RSAPrivateKey		ossl_d2i_RSAPrivateKey
#define i2d_RSAPrivateKey		ossl_i2d_RSAPrivateKey
#define i2d_RSAPublicKey		ossl_i2d_RSAPublicKey
#define d2i_RSAPublicKey		ossl_d2i_RSAPublicKey
#define RSAPublicKey_dup		ossl_RSAPublicKey_dup
#define RSAPrivateKey_dup		ossl_RSAPrivateKey_dup
#define RSA_blinding_on			ossl_RSA_blinding_on

/*
 *
 */

typedef struct RSA		RSA;
typedef struct RSA_METHOD	RSA_METHOD;

#include "ossl-bn.h"
#include "ossl-engine.h"

struct RSA_METHOD {
	const char *	name;
	int		(*rsa_pub_enc)(int, const unsigned char *, unsigned char *, RSA *, int);
	int		(*rsa_pub_dec)(int, const unsigned char *, unsigned char *, RSA *, int);
	int		(*rsa_priv_enc)(int, const unsigned char *, unsigned char *, RSA *, int);
	int		(*rsa_priv_dec)(int, const unsigned char *, unsigned char *, RSA *, int);
	int		(*rsa_mod_exp)(BIGNUM *, const BIGNUM *, RSA *, BN_CTX *);
	int		(*bn_mod_exp)(BIGNUM *, const BIGNUM *, const BIGNUM *, const BIGNUM *, BN_CTX *, BN_MONT_CTX *);
	int		(*init)(RSA *rsa);
	int		(*finish)(RSA *rsa);
	int		flags;
	char *		app_data;
	int		(*rsa_sign)(int, const unsigned char *, unsigned int,
	    unsigned char *, unsigned int *, const RSA *);
	int		(*rsa_verify)(int, const unsigned char *, unsigned int,
	    unsigned char *, unsigned int, const RSA *);
	int		(*rsa_keygen)(RSA *, int, BIGNUM *, BN_GENCB *);
};

struct RSA {
	int			pad;
	long			version;
	const RSA_METHOD *	meth;
	void *			engine;
	BIGNUM *		n;
	BIGNUM *		e;
	BIGNUM *		d;
	BIGNUM *		p;
	BIGNUM *		q;
	BIGNUM *		dmp1;
	BIGNUM *		dmq1;
	BIGNUM *		iqmp;
	struct rsa_CRYPTO_EX_DATA {
		void *	sk;
		int	dummy;
	}
	ex_data;
	int			references;
	int			flags;

	BN_MONT_CTX *		_method_mod_n;
	BN_MONT_CTX *		_method_mod_p;
	BN_MONT_CTX *		_method_mod_q;

	char *			bignum_data;
	void *			blinding;
	void *			mt_blinding;
};

#define RSA_FLAG_NO_BLINDING		0x0080
#define RSA_FLAG_EXT_PKEY		0x0020
#define RSA_FLAG_NO_CONSTTIME		0x0100

#define RSA_PKCS1_PADDING		1
#define RSA_PKCS1_OAEP_PADDING		4
#define RSA_PKCS1_PADDING_SIZE		11

#define RSA_F4				0x10001

/*
 *
 */

const RSA_METHOD *RSA_null_method(void);
const RSA_METHOD *RSA_gmp_method(void);
const RSA_METHOD *RSA_cdsa_method(void);
const RSA_METHOD *RSA_tfm_method(void);
const RSA_METHOD *RSA_ltm_method(void);

/*
 *
 */

RSA *RSA_new(void);
RSA *RSA_new_method(ENGINE *);
void RSA_free(RSA *);
int RSA_up_ref(RSA *);

void RSA_set_default_method(const RSA_METHOD *);
const RSA_METHOD *RSA_get_default_method(void);

const RSA_METHOD *RSA_get_method(const RSA *);
int RSA_set_method(RSA *, const RSA_METHOD *);

int RSA_set_app_data(RSA *, void *arg);
void *RSA_get_app_data(const RSA *);

int RSA_check_key(const RSA *);
int RSA_size(const RSA *);

int RSA_public_encrypt(int, const unsigned char *, unsigned char *, RSA *, int);
int RSA_private_encrypt(int, const unsigned char *, unsigned char *, RSA *, int);
int RSA_public_decrypt(int, const unsigned char *, unsigned char *, RSA *, int);
int RSA_private_decrypt(int, const unsigned char *, unsigned char *, RSA *, int);

int RSA_sign(int, const unsigned char *, unsigned int,
		    unsigned char *, unsigned int *, RSA *);
int RSA_verify(int, const unsigned char *, unsigned int,
		    unsigned char *, unsigned int, RSA *);

int RSA_generate_key_ex(RSA *, int, BIGNUM *, BN_GENCB *);

RSA *d2i_RSAPrivateKey(RSA **, const unsigned char **, long);
int i2d_RSAPrivateKey(const RSA *, unsigned char **);

int i2d_RSAPublicKey(const RSA *, unsigned char **);
RSA *d2i_RSAPublicKey(RSA **, const unsigned char **, long);
RSA *RSAPublicKey_dup(RSA *rsa);
RSA *RSAPrivateKey_dup(RSA *rsa);

int RSA_blinding_on(RSA *rsa, BN_CTX *ctx);

#endif /* _OSSL_RSA_H_ */

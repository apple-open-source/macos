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

/* Copyright (C) 1995-1998 Eric Young (eay@cryptsoft.com)
 * All rights reserved.
 *
 * This package is an SSL implementation written
 * by Eric Young (eay@cryptsoft.com).
 * The implementation was written so as to conform with Netscapes SSL.
 *
 * This library is free for commercial and non-commercial use as long as
 * the following conditions are aheared to.  The following conditions
 * apply to all code found in this distribution, be it the RC4, RSA,
 * lhash, DES, etc., code; not just the SSL code.  The SSL documentation
 * included with this distribution is covered by the same copyright terms
 * except that the holder is Tim Hudson (tjh@cryptsoft.com).
 *
 * Copyright remains Eric Young's, and as such any Copyright notices in
 * the code are not to be removed.
 * If this package is used in a product, Eric Young should be given attribution
 * as the author of the parts of the library used.
 * This can be in the form of a textual message at program startup or
 * in documentation (online or textual) provided with the package.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    "This product includes cryptographic software written by
 *     Eric Young (eay@cryptsoft.com)"
 *    The word 'cryptographic' can be left out if the rouines from the library
 *    being used are not cryptographic related :-).
 * 4. If you include any Windows specific code (or a derivative thereof) from
 *    the apps directory (application code) you must include an acknowledgement:
 *    "This product includes software written by Tim Hudson (tjh@cryptsoft.com)"
 *
 * THIS SOFTWARE IS PROVIDED BY ERIC YOUNG ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * The licence and distribution terms for any publically available version or
 * derivative of this code cannot be changed.  i.e. this code cannot simply be
 * copied and put under another distribution licence
 * [including the GNU Public Licence.]
 */


#include "ossl-config.h"


#include <stdio.h>

#include "ossl-bn.h"
#include "ossl-rand.h"
#include "ossl-dh.h"

#if !defined(PR_10771223_FIXED) || !defined(PR_10771188_FIXED)

/*
 * EAY Diffie-Hellman key exchange
 */

#define  DH_MAX_MODULUS_BITS    10000

static int eay_dh_generate_key(DH *dh);
static int eay_dh_compute_key(unsigned char *key, const BIGNUM *pub_key, DH *dh);
static int eay_dh_bn_mod_exp(const DH *dh, BIGNUM *r,
    const BIGNUM *a, const BIGNUM *p,
    const BIGNUM *m, BN_CTX *ctx
    /*,  BN_MONT_CTX *m_ctx */);
static int eay_dh_init(DH *dh);
static int eay_dh_finish(DH *dh);

static int
eay_dh_generate_key(DH *dh)
{
	int ok = 0;
	int generate_new_key = 0;
	unsigned l;
	BN_CTX *ctx;

#if 0
	BN_MONT_CTX *mont = NULL;
#endif
	BIGNUM *pub_key = NULL, *priv_key = NULL;

	ctx = BN_CTX_new();
	if (ctx == NULL) {
		goto err;
	}

	if (dh->priv_key == NULL) {
		priv_key = BN_new();
		if (priv_key == NULL) {
			goto err;
		}
		generate_new_key = 1;
	} else{
		priv_key = dh->priv_key;
	}

	if (dh->pub_key == NULL) {
		pub_key = BN_new();
		if (pub_key == NULL) {
			goto err;
		}
	} else{
		pub_key = dh->pub_key;
	}


#if 0
	if (dh->flags & DH_FLAG_CACHE_MONT_P) {
		mont = BN_MONT_CTX_set_locked(&dh->method_mont_p,
			CRYPTO_LOCK_DH, dh->p, ctx);
		if (!mont) {
			goto err;
		}
	}
#endif

	if (generate_new_key) {
		l = dh->length ? dh->length : BN_num_bits(dh->p) - 1; /* secret exponent length */
		if (!BN_rand(priv_key, l, 0, 0)) {
			goto err;
		}
	}

	{
		BIGNUM local_prk;
		BIGNUM *prk;

#if 0
		if ((dh->flags & DH_FLAG_NO_EXP_CONSTTIME) == 0) {
			BN_init(&local_prk);
			prk = &local_prk;
			BN_with_flags(prk, priv_key, BN_FLG_CONSTTIME);
		} else
#endif
		prk = priv_key;

		if (!dh->meth->bn_mod_exp(dh, pub_key, dh->g, prk, dh->p, ctx /* , mont */)) {
			goto err;
		}
	}

	dh->pub_key = pub_key;
	dh->priv_key = priv_key;
	ok = 1;
err:

	/*
	 * if (ok != 1)
	 *      DHerr(DH_F_GENERATE_KEY,ERR_R_BN_LIB);
	 */

	if ((pub_key != NULL) && (dh->pub_key == NULL)) {
		BN_clear_free(pub_key);
	}
	if ((priv_key != NULL) && (dh->priv_key == NULL)) {
		BN_clear_free(priv_key);
	}
	BN_CTX_free(ctx);
	return (ok);
}


static int
eay_dh_compute_key(unsigned char *key, const BIGNUM *pub_key, DH *dh)
{
	BN_CTX *ctx = NULL;

#if 0
	BN_MONT_CTX *mont = NULL;
#endif
	BIGNUM *tmp;
	int ret = -1;
	int check_result;

	if (BN_num_bits(dh->p) > DH_MAX_MODULUS_BITS) {
		/* DHerr(DH_F_COMPUTE_KEY,DH_R_MODULUS_TOO_LARGE); */
		goto err;
	}

	ctx = BN_CTX_new();
	if (ctx == NULL) {
		goto err;
	}
	BN_CTX_start(ctx);
	tmp = BN_CTX_get(ctx);

	if (dh->priv_key == NULL) {
		/* DHerr(DH_F_COMPUTE_KEY,DH_R_NO_PRIVATE_VALUE); */
		goto err;
	}

#if 0
	if (dh->flags & DH_FLAG_CACHE_MONT_P) {
		mont = BN_MONT_CTX_set_locked(&dh->method_mont_p,
			CRYPTO_LOCK_DH, dh->p, ctx);
		if ((dh->flags & DH_FLAG_NO_EXP_CONSTTIME) == 0) {
			/* XXX */
			BN_set_flags(dh->priv_key, BN_FLG_CONSTTIME);
		}
		if (!mont) {
			goto err;
		}
	}
#endif

	if (!DH_check_pubkey(dh, pub_key, &check_result) || check_result) {
		/* DHerr(DH_F_COMPUTE_KEY,DH_R_INVALID_PUBKEY); */
		goto err;
	}

	if (!dh->meth->bn_mod_exp(dh, tmp, pub_key, dh->priv_key, dh->p, ctx /*, mont */)) {
		/* DHerr(DH_F_COMPUTE_KEY,ERR_R_BN_LIB); */
		goto err;
	}

	ret = BN_bn2bin(tmp, key);

err:
	if (ctx != NULL) {
		BN_CTX_end(ctx);
		BN_CTX_free(ctx);
	}
	return (ret);
}


static int
eay_dh_bn_mod_exp(const DH *dh, BIGNUM *r,
    const BIGNUM *a, const BIGNUM *p,
    const BIGNUM *m, BN_CTX *ctx
    /*,  BN_MONT_CTX *m_ctx */)
{
	/* If a is only one word long and constant time is false, use the faster
	 * exponenentiation function.
	 */
	if ((a->top == 1) && ((dh->flags & DH_FLAG_NO_EXP_CONSTTIME) != 0)) {
		BN_ULONG A = a->d[0];
		/* return BN_mod_exp_mont_word(r,A,p,m,ctx,m_ctx); */
		return (BN_mod_exp_mont_word(r, A, p, m, ctx, NULL));
	} else {
		/* return BN_mod_exp_mont(r,a,p,m,ctx,m_ctx); */
		return (BN_mod_exp(r, a, p, m, ctx));
	}
}


static int
eay_dh_generate_params(DH *dh, int a, int b, BN_GENCB *callback)
{
	/* groups should already be known, we don't care about this */
	return (0);
}


static int
eay_dh_init(DH *dh)
{
#if 0
	dh->flags |= DH_FLAG_CACHE_MONT_P;
#endif
	return (1);
}


static int
eay_dh_finish(DH *dh)
{
#if 0
	if (dh->method_mont_p) {
		BN_MONT_CTX_free(dh->method_mont_p);
	}
#endif
	return (1);
}


const DH_METHOD _ossl_dh_eay_method =
{
	.name			= "EAY DH",
	.generate_key		= eay_dh_generate_key,
	.compute_key		= eay_dh_compute_key,
	.bn_mod_exp		= eay_dh_bn_mod_exp,
	.init			= eay_dh_init,
	.finish			= eay_dh_finish,
	.flags			=	     0,
	.app_data		= NULL,
	.generate_params	= eay_dh_generate_params
};
#endif /*  ! defined(PR_10771223_FIXED) || ! defined(PR_10771188_FIXED) */

const DH_METHOD *
DH_eay_method(void)
{
#if  !defined(PR_10771223_FIXED) || !defined(PR_10771188_FIXED)
	return (&_ossl_dh_eay_method);

#else
	return (NULL);
#endif
}

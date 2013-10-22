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

/* Original version from Steven Schoch <schoch@sheba.arc.nasa.gov> */

#include "ossl-config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ossl-bn.h"
#include "ossl-dsa.h"
#include "ossl-rand.h"

#if !defined(PR_10488503_FIXED)

#define DSA_MAX_MODULUS_BITS    10000

static DSA_SIG *eay_dsa_do_sign(const unsigned char *dgst, int dlen, DSA *dsa);
static int eay_dsa_sign_setup(DSA *dsa, BN_CTX *ctx_in, BIGNUM **kinvp, BIGNUM **rp);
static int eay_dsa_do_verify(const unsigned char *dgst, int dgst_len, DSA_SIG *sig,
    DSA *dsa);
static int eay_dsa_init(DSA *dsa);
static int eay_dsa_finish(DSA *dsa);

/* These macro wrappers replace attempts to use the dsa_mod_exp() and
 * bn_mod_exp() handlers in the DSA_METHOD structure. We avoid the problem of
 * having a the macro work as an expression by bundling an "err_instr". So;
 *
 *     if (!dsa->meth->bn_mod_exp(dsa, r,dsa->g,&k,dsa->p,ctx,
 *                 dsa->method_mont_p)) goto err;
 *
 * can be replaced by;
 *
 *     DSA_BN_MOD_EXP(goto err, dsa, r, dsa->g, &k, dsa->p, ctx,
 *                 dsa->method_mont_p);
 */
#define DSA_MOD_EXP(err_instr, dsa, rr, a1, p1, a2, p2, m, ctx, in_mont)	       \
	do {									       \
		int _tmp_res53;							       \
		if ((dsa)->meth->dsa_mod_exp) {					       \
			_tmp_res53 = (dsa)->meth->dsa_mod_exp((dsa), (rr), (a1), (p1), \
				(a2), (p2), (m), (ctx), (in_mont)); }		       \
		else{								       \
			_tmp_res53 = BN_mod_exp2_mont((rr), (a1), (p1), (a2), (p2),    \
				(m), (ctx), (in_mont)); }			       \
		if (!_tmp_res53) { err_instr; }					       \
	}									       \
	while (0)
#define DSA_BN_MOD_EXP(err_instr, dsa, r, a, p, m, ctx, m_ctx)				    \
	do {										    \
		int _tmp_res53;								    \
		if ((dsa)->meth->bn_mod_exp) {						    \
			_tmp_res53 = (dsa)->meth->bn_mod_exp((dsa), (r), (a), (p),	    \
				(m), (ctx), (m_ctx)); }					    \
		else{									    \
			_tmp_res53 = BN_mod_exp_mont((r), (a), (p), (m), (ctx), (m_ctx)); } \
		if (!_tmp_res53) { err_instr; }						    \
	}										    \
	while (0)

static DSA_SIG *
eay_dsa_do_sign(const unsigned char *dgst, int dlen, DSA *dsa)
{
	BIGNUM *kinv = NULL, *r = NULL, *s = NULL;
	BIGNUM m;
	BIGNUM xr;
	BN_CTX *ctx = NULL;
	int i /* , reason = ERR_R_BN_LIB */;
	DSA_SIG *ret = NULL;

	BN_init(&m);
	BN_init(&xr);

	if (!dsa->p || !dsa->q || !dsa->g) {
		/* reason = DSA_R_MISSING_PARAMETERS; */
		goto err;
	}

	s = BN_new();
	if (s == NULL) {
		goto err;
	}

	i = BN_num_bytes(dsa->q); /* should be 20 */
	if ((dlen > i) || (dlen > 50)) {
		/* reason=DSA_R_DATA_TOO_LARGE_FOR_KEY_SIZE; */
		goto err;
	}

	ctx = BN_CTX_new();
	if (ctx == NULL) {
		goto err;
	}

	if ((dsa->kinv == NULL) || (dsa->r == NULL)) {
		if (!DSA_sign_setup(dsa, ctx, &kinv, &r)) {
			goto err;
		}
	} else {
		kinv = dsa->kinv;
		dsa->kinv = NULL;
		r = dsa->r;
		dsa->r = NULL;
	}

	if (BN_bin2bn(dgst, dlen, &m) == NULL) {
		goto err;
	}

	/* Compute  s = inv(k) (m + xr) mod q */
	if (!BN_mod_mul(&xr, dsa->priv_key, r, dsa->q, ctx)) {
		goto err;                                             /* s = xr */
	}
	if (!BN_add(s, &xr, &m)) {
		goto err;                               /* s = m + xr */
	}
	if (BN_cmp(s, dsa->q) > 0) {
		if (!BN_sub(s, s, dsa->q)) {
			goto err;
		}
	}
	if (!BN_mod_mul(s, s, kinv, dsa->q, ctx)) {
		goto err;
	}

	ret = DSA_SIG_new();
	if (ret == NULL) {
		goto err;
	}
	ret->r = r;
	ret->s = s;

err:
	if (!ret) {
		/* DSAerr(DSA_F_DSA_DO_SIGN,reason); */
		BN_free(r);
		BN_free(s);
	}
	if (ctx != NULL) {
		BN_CTX_free(ctx);
	}
	BN_clear_free(&m);
	BN_clear_free(&xr);
	if (kinv != NULL) { /* dsa->kinv is NULL now if we used it */
		BN_clear_free(kinv);
	}
	return (ret);
}


static int
eay_dsa_sign_setup(DSA *dsa, BN_CTX *ctx_in, BIGNUM **kinvp, BIGNUM **rp)
{
	BN_CTX *ctx;
	BIGNUM k, kq, *K, *kinv = NULL, *r = NULL;
	int ret = 0;

	if (!dsa->p || !dsa->q || !dsa->g) {
		/* DSAerr(DSA_F_DSA_SIGN_SETUP,DSA_R_MISSING_PARAMETERS); */
		return (0);
	}

	BN_init(&k);
	BN_init(&kq);

	if (ctx_in == NULL) {
		if ((ctx = BN_CTX_new()) == NULL) {
			goto err;
		}
	} else{
		ctx = ctx_in;
	}

	if ((r = BN_new()) == NULL) {
		goto err;
	}

	/* Get random k */
	do {
		if (!BN_rand_range(&k, dsa->q)) {
			goto err;
		}
	} while (BN_is_zero(&k));
	if ((dsa->flags & DSA_FLAG_NO_EXP_CONSTTIME) == 0) {
		BN_set_flags(&k, BN_FLG_CONSTTIME);
	}

#if 0
	if (dsa->flags & DSA_FLAG_CACHE_MONT_P) {
		if (!BN_MONT_CTX_set_locked(&dsa->method_mont_p,
		    CRYPTO_LOCK_DSA,
		    dsa->p, ctx)) {
			goto err;
		}
	}
#endif

	/* Compute r = (g^k mod p) mod q */

	if ((dsa->flags & DSA_FLAG_NO_EXP_CONSTTIME) == 0) {
		if (!BN_copy(&kq, &k)) {
			goto err;
		}

		/* We do not want timing information to leak the length of k,
		 * so we compute g^k using an equivalent exponent of fixed length.
		 *
		 * (This is a kludge that we need because the BN_mod_exp_mont()
		 * does not let us specify the desired timing behaviour.) */

		if (!BN_add(&kq, &kq, dsa->q)) {
			goto err;
		}
		if (BN_num_bits(&kq) <= BN_num_bits(dsa->q)) {
			if (!BN_add(&kq, &kq, dsa->q)) {
				goto err;
			}
		}

		K = &kq;
	} else {
		K = &k;
	}
#if 1
	DSA_BN_MOD_EXP(goto err, dsa, r, dsa->g, K, dsa->p, ctx,
	    dsa->method_mont_p);
#else
	if (!BN_mod_exp(r, dsa->g, K, dsa->p, ctx)) {
		goto err;
	}
#endif
	if (!BN_mod(r, r, dsa->q, ctx)) {
		goto err;
	}

	/* Compute  part of 's = inv(k) (m + xr) mod q' */
	if ((kinv = BN_mod_inverse(NULL, &k, dsa->q, ctx)) == NULL) {
		goto err;
	}

	if (*kinvp != NULL) {
		BN_clear_free(*kinvp);
	}
	*kinvp = kinv;
	kinv = NULL;
	if (*rp != NULL) {
		BN_clear_free(*rp);
	}
	*rp = r;
	ret = 1;
err:
	if (!ret) {
		/* DSAerr(DSA_F_DSA_SIGN_SETUP,ERR_R_BN_LIB); */
		if (kinv != NULL) {
			BN_clear_free(kinv);
		}
		if (r != NULL) {
			BN_clear_free(r);
		}
	}
	if (ctx_in == NULL) {
		BN_CTX_free(ctx);
	}
	if (kinv != NULL) {
		BN_clear_free(kinv);
	}
	BN_clear_free(&k);
	BN_clear_free(&kq);

	return (ret);
}


static int
eay_dsa_do_verify(const unsigned char *dgst, int dgst_len, DSA_SIG *sig, DSA *dsa)
{
	BN_CTX *ctx;
	BIGNUM u1, u2, t1;
	BN_MONT_CTX *mont = NULL;
	int ret = -1;

	if (!dsa->p || !dsa->q || !dsa->g) {
		/* DSAerr(DSA_F_DSA_DO_VERIFY,DSA_R_MISSING_PARAMETERS); */
		return (-1);
	}

	if (BN_num_bits(dsa->q) != 160) {
		/* DSAerr(DSA_F_DSA_DO_VERIFY,DSA_R_BAD_Q_VALUE); */
		return (-1);
	}

	if (BN_num_bits(dsa->p) > DSA_MAX_MODULUS_BITS) {
		/* DSAerr(DSA_F_DSA_DO_VERIFY,DSA_R_MODULUS_TOO_LARGE); */
		return (-1);
	}

	BN_init(&u1);
	BN_init(&u2);
	BN_init(&t1);

	if ((ctx = BN_CTX_new()) == NULL) {
		goto err;
	}

	if (BN_is_zero(sig->r) || BN_is_negative(sig->r) ||
	    (BN_ucmp(sig->r, dsa->q) >= 0)) {
		ret = 0;
		goto err;
	}
	if (BN_is_zero(sig->s) || BN_is_negative(sig->s) ||
	    (BN_ucmp(sig->s, dsa->q) >= 0)) {
		ret = 0;
		goto err;
	}

	/* Calculate W = inv(S) mod Q
	 * save W in u2 */
	if ((BN_mod_inverse(&u2, sig->s, dsa->q, ctx)) == NULL) {
		goto err;
	}

	/* save M in u1 */
	if (BN_bin2bn(dgst, dgst_len, &u1) == NULL) {
		goto err;
	}

	/* u1 = M * w mod q */
	if (!BN_mod_mul(&u1, &u1, &u2, dsa->q, ctx)) {
		goto err;
	}

	/* u2 = r * w mod q */
	if (!BN_mod_mul(&u2, sig->r, &u2, dsa->q, ctx)) {
		goto err;
	}


#if 0
	if (dsa->flags & DSA_FLAG_CACHE_MONT_P) {
		mont = BN_MONT_CTX_set_locked(&dsa->method_mont_p,
			CRYPTO_LOCK_DSA, dsa->p, ctx);
		if (!mont) {
			goto err;
		}
	}
#endif

	/* v = g^u1 * y^u2 mod p mod q */
#if 1
	DSA_MOD_EXP(goto err, dsa, &t1, dsa->g, &u1, dsa->pub_key, &u2, dsa->p, ctx, mont);
#else
	/* u1 = g^u1 mod p */
	if (!BN_mod_exp(&u1, dsa->g, &u1, dsa->p, ctx)) {
		goto err;
	}
	/* u2 = y^u2 mod p */
	if (!BN_mod_exp(&u2, dsa->pub_key, &u2, dsa->p, ctx)) {
		goto err;
	}
	/* t1 = u1 * u2 mod p */
	if (!BN_mod_mul(&t1, &u1, &u2, dsa->p, ctx)) {
		goto err;
	}
#endif
	/* BN_copy(&u1,&t1); */
	/* let u1 = u1 mod q */
	if (!BN_mod(&u1, &t1, dsa->q, ctx)) {
		goto err;
	}

	/* V is now in u1.  If the signature is correct, it will be
	 * equal to R. */
	ret = (BN_ucmp(&u1, sig->r) == 0);

err:

	/* XXX: surely this is wrong - if ret is 0, it just didn't verify;
	 * there is no error in BN. Test should be ret == -1 (Ben) */
	/* if (ret != 1) DSAerr(DSA_F_DSA_DO_VERIFY,ERR_R_BN_LIB); */
	if (ctx != NULL) {
		BN_CTX_free(ctx);
	}
	BN_free(&u1);
	BN_free(&u2);
	BN_free(&t1);
	return (ret);
}


static int dsa_init(DSA *dsa)
{
	/* dsa->flags|=DSA_FLAG_CACHE_MONT_P; */
	return (1);
}


static int dsa_finish(DSA *dsa)
{
	if (dsa->method_mont_p) {
		BN_MONT_CTX_free(dsa->method_mont_p);
	}
	return (1);
}


const DSA_METHOD _ossl_dsa_eay_method =
{
	.name		= "EAY DSA",
	.dsa_do_sign	= eay_dsa_do_sign,
	.dsa_sign_setup = eay_dsa_sign_setup,
	.dsa_do_verify	= eay_dsa_do_verify,
	.dsa_mod_exp	= NULL,                 /* dsa_mod_exp, */
	.bn_mod_exp	= NULL,                 /* dsa_bn_mod_exp, */
	.init		= dsa_init,
	.finish		= dsa_finish,
	.flags		=		   0,
	.app_data	= NULL,
	.dsa_paramgen	= NULL,
	.dsa_keygen	= NULL
};

#endif /* ! defined(PR_10488503_FIXED) */

const DSA_METHOD *
DSA_eay_method(void)
{
#if !defined(PR_10488503_FIXED)
	return (&_ossl_dsa_eay_method);

#else
	return (NULL);
#endif
}

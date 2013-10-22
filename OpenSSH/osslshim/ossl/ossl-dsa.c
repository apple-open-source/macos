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

#include "ossl-config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "ossl-evp.h"
#include "ossl-dsa.h"

/**
 * Same as DSA_new_method() using NULL as engine.
 *
 * @return a newly allocated DSA object. Free with DSA_free().
 */
DSA *
DSA_new(void)
{
	return (DSA_new_method(NULL));
}


/**
 * Allocate a new DSA object using the engine, if NULL is specified as
 * the engine, use the default DSA engine as returned by
 * ENGINE_get_default_DSA().
 *
 * @param engine Specific what ENGINE DSA provider should be used.
 *
 * @return a newly allocated DSA object. Free with DSA_free().
 *
 */
DSA *
DSA_new_method(ENGINE *engine)
{
	DSA *dsa;

	dsa = calloc(1, sizeof(*dsa));
	if (dsa == NULL) {
		return (NULL);
	}

	dsa->write_params = 1;
	dsa->references = 1;

	if (engine) {
		ENGINE_up_ref(engine);
		dsa->engine = engine;
	} else {
		dsa->engine = ENGINE_get_default_DSA();
	}

	if (dsa->engine) {
		dsa->meth = ENGINE_get_DSA(dsa->engine);
		if (dsa->meth == NULL) {
			ENGINE_finish(engine);
			free(dsa);
			return (0);
		}
	}

	if (dsa->meth == NULL) {
		dsa->meth = DSA_get_default_method();
	}

	(*dsa->meth->init)(dsa);

	return (dsa);
}


/**
 * Free an allocation DSA object.
 *
 * @param dsa the DSA object to free.
 */
void
DSA_free(DSA *dsa)
{
	if (dsa->references <= 0) {
		abort();
	}

	if (--dsa->references > 0) { /* XXX use atomic op */
		return;
	}

	(*dsa->meth->finish)(dsa);

	if (dsa->engine) {
		ENGINE_finish(dsa->engine);
	}

#define free_if(f)    if (f) { BN_clear_free(f); }
	free_if(dsa->p);
	free_if(dsa->q);
	free_if(dsa->g);
	free_if(dsa->pub_key);
	free_if(dsa->priv_key);
	free_if(dsa->kinv);
	free_if(dsa->r);
#undef free_if

	memset(dsa, 0, sizeof(*dsa));
	free(dsa);
}


/**
 * Add an extra reference to the DSA object. The object should be free
 * with DSA_free() to drop the reference.
 *
 * @param dsa the object to add reference counting too.
 *
 * @return the current reference count, can't safely be used except
 * for debug printing.
 *
 */
int
DSA_up_ref(DSA *dsa)
{
	return (++dsa->references); /* XXX use atomic op */
}


#if 0

/**
 * Return the DSA_METHOD used for this DSA object.
 *
 * @param dsa the object to get the method from.
 *
 * @return the method used for this DSA object.
 *
 */
const DSA_METHOD *
DSA_get_method(const DSA *dsa)
{
	return (dsa->meth);
}


#endif

/**
 * Set a new method for the DSA keypair.
 *
 * @param dsa dsa parameter.
 * @param method the new method for the DSA parameter.
 *
 * @return 1 on success.
 *
 */
int
DSA_set_method(DSA *dsa, const DSA_METHOD *method)
{
	(*dsa->meth->finish)(dsa);

	if (dsa->engine) {
		ENGINE_finish(dsa->engine);
		dsa->engine = NULL;
	}

	dsa->meth = method;
	(*dsa->meth->init)(dsa);
	return (1);
}


#if 0

/**
 * Set the application data for the DSA object.
 *
 * @param dsa the dsa object to set the parameter for
 * @param arg the data object to store
 *
 * @return 1 on success.
 *
 */
int
DSA_set_app_data(DSA *dsa, void *arg)
{
	dsa->ex_data.sk = arg;
	return (1);
}


/**
 * Get the application data for the DSA object.
 *
 * @param dsa the dsa object to get the parameter for
 *
 * @return the data object
 *
 */
void *
DSA_get_app_data(const DSA *dsa)
{
	return (dsa->ex_data.sk);
}


#endif

DSA_SIG *
DSA_do_sign(const unsigned char *dgst, unsigned int dlen, DSA *dsa)
{
	if (dsa && dsa->meth && dsa->meth->dsa_do_sign) {
		return (dsa->meth->dsa_do_sign(dgst, dlen, dsa));
	} else{
		return (NULL);
	}
}


int
DSA_sign_setup(DSA *dsa, BN_CTX *ctx_in, BIGNUM **kinvp, BIGNUM **rp)
{
	if (dsa && dsa->meth->dsa_sign_setup) {
		return (dsa->meth->dsa_sign_setup(dsa, ctx_in, kinvp, rp));
	} else{
		return (0);
	}
}


int
DSA_do_verify(const unsigned char *dgst, int dgst_len, DSA_SIG *sig, DSA *dsa)
{
	if (dsa && dsa->meth && dsa->meth->dsa_do_verify) {
		return (dsa->meth->dsa_do_verify(dgst, dgst_len, sig, dsa));
	} else{
		return (0);
	}
}


/*
 * default key generation
 */
static int
dsa_builtin_keygen(DSA *dsa)
{
	int ok = 0;
	BN_CTX *ctx = NULL;
	BIGNUM *pub_key = NULL, *priv_key = NULL;

	if ((ctx = BN_CTX_new()) == NULL) {
		goto err;
	}

	if (dsa->priv_key == NULL) {
		if ((priv_key = BN_new()) == NULL) {
			goto err;
		}
	} else{
		priv_key = dsa->priv_key;
	}

	do {
		if (!BN_rand_range(priv_key, dsa->q)) {
			goto err;
		}
	} while (BN_is_zero(priv_key));

	if (dsa->pub_key == NULL) {
		if ((pub_key = BN_new()) == NULL) {
			goto err;
		}
	} else{
		pub_key = dsa->pub_key;
	}

	{
		BIGNUM local_prk;
		BIGNUM *prk;

		if ((dsa->flags & DSA_FLAG_NO_EXP_CONSTTIME) == 0) {
			BN_init(&local_prk);
			prk = &local_prk;
			BN_with_flags(prk, priv_key, BN_FLG_CONSTTIME);
		} else{
			prk = priv_key;
		}

		if (!BN_mod_exp(pub_key, dsa->g, prk, dsa->p, ctx)) {
			goto err;
		}
	}

	dsa->priv_key = priv_key;
	dsa->pub_key = pub_key;
	ok = 1;

err:
	if ((pub_key != NULL) && (dsa->pub_key == NULL)) {
		BN_free(pub_key);
	}
	if ((priv_key != NULL) && (dsa->priv_key == NULL)) {
		BN_free(priv_key);
	}
	if (ctx != NULL) {
		BN_CTX_free(ctx);
	}
	return (ok);
}


int
DSA_generate_key(DSA *dsa)
{
	if (dsa && dsa->meth && dsa->meth->dsa_keygen) {
		return (dsa->meth->dsa_keygen(dsa));
	} else{
		return (dsa_builtin_keygen(dsa));
	}
}


#define HASH			EVP_sha()
#ifndef SHA_DIGEST_LENGTH
#define SHA_DIGEST_LENGTH	20
#endif
#define DSS_prime_checks	50

static int
dsa_builtin_paramgen(DSA *ret, int bits,
    unsigned char *seed_in, int seed_len,
    int *counter_ret, unsigned long *h_ret, BN_GENCB *cb)
{
	int ok = 0;
	unsigned char seed[SHA_DIGEST_LENGTH];
	unsigned char md[SHA_DIGEST_LENGTH];
	unsigned char buf[SHA_DIGEST_LENGTH], buf2[SHA_DIGEST_LENGTH];
	BIGNUM *r0, *W, *X, *c, *test;
	BIGNUM *g = NULL, *q = NULL, *p = NULL;
	BN_MONT_CTX *mont = NULL;
	int k, n = 0, i, m = 0;
	int counter = 0;
	int r = 0;
	BN_CTX *ctx = NULL;
	unsigned int h = 2;

	if (bits < 512) {
		bits = 512;
	}
	bits = (bits + 63) / 64 * 64;

	/* NB: seed_len == 0 is special case: copy generated seed to
	 * seed_in if it is not NULL.
	 */
	if (seed_len && (seed_len < 20)) {
		seed_in = NULL; /* seed buffer too small -- ignore */
	}
	if (seed_len > 20) {
		seed_len = 20; /* App. 2.2 of FIPS PUB 186 allows larger SEED,
	                        * but our internal buffers are restricted to 160 bits*/
	}
	if ((seed_in != NULL) && (seed_len == 20)) {
		memcpy(seed, seed_in, seed_len);
		/* set seed_in to NULL to avoid it being copied back */
		seed_in = NULL;
	}

	if ((ctx = BN_CTX_new()) == NULL) {
		goto err;
	}

	if ((mont = BN_MONT_CTX_new()) == NULL) {
		goto err;
	}

	BN_CTX_start(ctx);
	r0 = BN_CTX_get(ctx);
	g = BN_CTX_get(ctx);
	W = BN_CTX_get(ctx);
	q = BN_CTX_get(ctx);
	X = BN_CTX_get(ctx);
	c = BN_CTX_get(ctx);
	p = BN_CTX_get(ctx);
	test = BN_CTX_get(ctx);

	if (!BN_lshift(test, BN_value_one(), bits - 1)) {
		goto err;
	}

	for ( ; ; ) {
		for ( ; ; ) {
			/* find q */
			int seed_is_random;

			/* step 1 */
			if (!BN_GENCB_call(cb, 0, m++)) {
				goto err;
			}

			if (!seed_len) {
				RAND_pseudo_bytes(seed, SHA_DIGEST_LENGTH);
				seed_is_random = 1;
			} else {
				seed_is_random = 0;
				seed_len = 0; /* use random seed if 'seed_in' turns out to be bad*/
			}
			memcpy(buf, seed, SHA_DIGEST_LENGTH);
			memcpy(buf2, seed, SHA_DIGEST_LENGTH);
			/* precompute "SEED + 1" for step 7: */
			for (i = SHA_DIGEST_LENGTH - 1; i >= 0; i--) {
				buf[i]++;
				if (buf[i] != 0) {
					break;
				}
			}

			/* step 2 */
			EVP_Digest(seed, SHA_DIGEST_LENGTH, md, NULL, HASH, NULL);
			EVP_Digest(buf, SHA_DIGEST_LENGTH, buf2, NULL, HASH, NULL);
			for (i = 0; i < SHA_DIGEST_LENGTH; i++) {
				md[i] ^= buf2[i];
			}

			/* step 3 */
			md[0] |= 0x80;
			md[SHA_DIGEST_LENGTH - 1] |= 0x01;
			if (!BN_bin2bn(md, SHA_DIGEST_LENGTH, q)) {
				goto err;
			}

			/* step 4 */
			r = BN_is_prime_fasttest_ex(q, DSS_prime_checks, ctx,
				seed_is_random, cb);
			if (r > 0) {
				break;
			}
			if (r != 0) {
				goto err;
			}

			/* do a callback call */
			/* step 5 */
		}

		if (!BN_GENCB_call(cb, 2, 0)) {
			goto err;
		}
		if (!BN_GENCB_call(cb, 3, 0)) {
			goto err;
		}

		/* step 6 */
		counter = 0;
		/* "offset = 2" */

		n = (bits - 1) / 160;

		for ( ; ; ) {
			if ((counter != 0) && !BN_GENCB_call(cb, 0, counter)) {
				goto err;
			}

			/* step 7 */
			BN_zero(W);
			/* now 'buf' contains "SEED + offset - 1" */
			for (k = 0; k <= n; k++) {
				/* obtain "SEED + offset + k" by incrementing: */
				for (i = SHA_DIGEST_LENGTH-1; i >= 0; i--) {
					buf[i]++;
					if (buf[i] != 0) {
						break;
					}
				}

				EVP_Digest(buf, SHA_DIGEST_LENGTH, md, NULL, HASH, NULL);

				/* step 8 */
				if (!BN_bin2bn(md, SHA_DIGEST_LENGTH, r0)) {
					goto err;
				}
				if (!BN_lshift(r0, r0, 160*k)) {
					goto err;
				}
				if (!BN_add(W, W, r0)) {
					goto err;
				}
			}

			/* more of step 8 */
			if (!BN_mask_bits(W, bits-1)) {
				goto err;
			}
			if (!BN_copy(X, W)) {
				goto err;
			}
			if (!BN_add(X, X, test)) {
				goto err;
			}

			/* step 9 */
			if (!BN_lshift1(r0, q)) {
				goto err;
			}
			if (!BN_mod(c, X, r0, ctx)) {
				goto err;
			}
			if (!BN_sub(r0, c, BN_value_one())) {
				goto err;
			}
			if (!BN_sub(p, X, r0)) {
				goto err;
			}

			/* step 10 */
			if (BN_cmp(p, test) >= 0) {
				/* step 11 */
				r = BN_is_prime_fasttest_ex(p, DSS_prime_checks,
					ctx, 1, cb);
				if (r > 0) {
					goto end;         /* found it */
				}
				if (r != 0) {
					goto err;
				}
			}

			/* step 13 */
			counter++;
			/* "offset = offset + n + 1" */

			/* step 14 */
			if (counter >= 4096) {
				break;
			}
		}
	}
end:
	if (!BN_GENCB_call(cb, 2, 1)) {
		goto err;
	}

	/* We now need to generate g */
	/* Set r0=(p-1)/q */
	if (!BN_sub(test, p, BN_value_one())) {
		goto err;
	}
	if (!BN_div(r0, NULL, test, q, ctx)) {
		goto err;
	}

	if (!BN_set_word(test, h)) {
		goto err;
	}
	if (!BN_MONT_CTX_set(mont, p, ctx)) {
		goto err;
	}

	for ( ; ; ) {
		/* g=test^r0%p */
		if (!BN_mod_exp_mont(g, test, r0, p, ctx, mont)) {
			goto err;
		}
		if (!BN_is_one(g)) {
			break;
		}
		if (!BN_add(test, test, BN_value_one())) {
			goto err;
		}
		h++;
	}

	if (!BN_GENCB_call(cb, 3, 1)) {
		goto err;
	}

	ok = 1;
err:
	if (ok) {
		if (ret->p) {
			BN_free(ret->p);
		}
		if (ret->q) {
			BN_free(ret->q);
		}
		if (ret->g) {
			BN_free(ret->g);
		}
		ret->p = BN_dup(p);
		ret->q = BN_dup(q);
		ret->g = BN_dup(g);
		if ((ret->p == NULL) || (ret->q == NULL) || (ret->g == NULL)) {
			ok = 0;
			goto err;
		}
		if (seed_in != NULL) {
			memcpy(seed_in, seed, 20);
		}
		if (counter_ret != NULL) {
			*counter_ret = counter;
		}
		if (h_ret != NULL) {
			*h_ret = h;
		}
	}
	if (ctx) {
		BN_CTX_end(ctx);
		BN_CTX_free(ctx);
	}
	if (mont != NULL) {
		BN_MONT_CTX_free(mont);
	}
	return (ok);
}


int
DSA_generate_parameters_ex(DSA *ret, int bits,
    unsigned char *seed_in, int seed_len,
    int *counter_ret, unsigned long *h_ret, BN_GENCB *cb)
{
	if (ret && ret->meth->dsa_paramgen) {
		return (ret->meth->dsa_paramgen(ret, bits, seed_in, seed_len,
		       counter_ret, h_ret, cb));
	}
	return (dsa_builtin_paramgen(ret, bits, seed_in, seed_len,
	       counter_ret, h_ret, cb));
}


/*
 * DSA Null method
 */
static DSA_SIG *
null_dsa_do_sign(const unsigned char *dgst, int dlen, DSA *dsa)
{
	return (NULL);
}


static int
null_dsa_sign_setup(DSA *dsa, BN_CTX *ctx_in, BIGNUM **kinvp, BIGNUM **rp)
{
	return (0);
}


static int
null_dsa_do_verify(const unsigned char *dgst, int dgst_len, DSA_SIG *sig, DSA *dsa)
{
	return (0);
}


static int
null_dsa_init(DSA *dsa)
{
	return (0);
}


static int
null_dsa_finish(DSA *dsa)
{
	return (0);
}


static int
null_dsa_paramgen(DSA *dsa, int bits, unsigned char *seed, int seed_len,
    int *counter_ret, unsigned long *h_ret, BN_GENCB *cb)
{
	return (0);
}


static int
null_dsa_keygen(DSA *dsa)
{
	return (0);
}


static const DSA_METHOD dsa_null_method =
{
	.name		= "cryptoshim null DSA",
	.dsa_do_sign	= null_dsa_do_sign,
	.dsa_sign_setup = null_dsa_sign_setup,
	.dsa_do_verify	= null_dsa_do_verify,
	.init		= null_dsa_init,
	.finish		= null_dsa_finish,
	.flags		=		      0,
	.app_data	= NULL,
	.dsa_paramgen	= null_dsa_paramgen,
	.dsa_keygen	= null_dsa_keygen
};

const DSA_METHOD *
DSA_null_method(void)
{
	return (&dsa_null_method);
}


#if !defined(PR_10488503_FIXED)
extern const DSA_METHOD _ossl_dsa_eay_method;
static const DSA_METHOD *default_dsa_method = &_ossl_dsa_eay_method;
#elif HAVE_CDSA
extern const DSA_METHOD _ossl_dsa_cdsa_method;
static const DSA_METHOD *default_dsa_method = &_ossl_dsa_cdsa_method;
#elif defined(__APPLE_TARGET_EMBEDDED__)
static const DSA_METHOD *default_dsa_method = &dsa_null_method;
#elif defined(HAVE_GMP)
extern const DSA_METHOD _ossl_dsa_gmp_method;
static const DSA_METHOD *default_dsa_method = &_ossl_dsa_gmp_method;
#elif defined(HEIM_HC_LTM)
extern const DSA_METHOD _ossl_dsa_ltm_method;
static const DSA_METHOD *default_dsa_method = &_ossl_dsa_ltm_method;
#else
static const DSA_METHOD *default_dsa_method = &dsa_null_method;
#endif

const DSA_METHOD *
DSA_get_default_method(void)
{
	return (default_dsa_method);
}


void
DSA_set_default_method(const DSA_METHOD *meth)
{
	default_dsa_method = meth;
}


DSA_SIG *
DSA_SIG_new(void)
{
	DSA_SIG *sig;

	sig = malloc(sizeof(DSA_SIG));
	if (NULL == sig) {
		return (NULL);
	}
	sig->r = NULL;
	sig->s = NULL;

	return (sig);
}


void
DSA_SIG_free(DSA_SIG *sig)
{
	if (sig != NULL) {
		if (sig->r != NULL) {
			BN_free(sig->r);
		}
		if (sig->s) {
			BN_free(sig->s);
		}
		free(sig);
	}
}


#if defined(USE_HEIMDAL_ASN1)

/*
 * HEIMDAL ASN1
 */

#include "krb5-types.h"
#include "rfc2459_asn1.h"
#include "der.h"

#include "ossl-common.h"

int
DSA_size(const DSA *dsa)
{
	DSAPublicKey data;
	size_t size;

	if (NULL == dsa) {
		return (0);
	}

	if (_cs_BN_to_integer(dsa->q, &data)) {
		return (0);
	}

	size = length_DSAPublicKey(&data);
	free_DSAPublicKey(&data);

	return (size);
}


DSA *
d2i_DSAPrivateKey(DSA **dsa, const unsigned char **pp, long len)
{
	DSAPrivateKey data;
	DSA *k = NULL;
	size_t size;
	int ret;

	ret = decode_DSAPrivateKey(*pp, len, &data, &size);
	if (ret) {
		return (NULL);
	}

	*pp += size;

	if (dsa != NULL) {
		k = *dsa;
	}

	if (NULL == k) {
		k = DSA_new();
		if (NULL == k) {
			free_DSAPrivateKey(&data);
			return (NULL);
		}
	}

	k->p = _cs_integer_to_BN(&data.p, NULL);
	k->q = _cs_integer_to_BN(&data.q, NULL);
	k->g = _cs_integer_to_BN(&data.g, NULL);
	k->pub_key = _cs_integer_to_BN(&data.pub_key, NULL);
	k->priv_key = _cs_integer_to_BN(&data.priv_key, NULL);
	free_DSAPrivateKey(&data);

	if ((NULL == k->p) || (NULL == k->q) || (NULL == k->g) ||
	    (NULL == k->pub_key) || (NULL == k->priv_key)) {
		DSA_free(k);
		return (NULL);
	}

	if (dsa != NULL) {
		*dsa = k;
	}

	return (k);
}


DSA *
d2i_DSAPublicKey(DSA **dsa, const unsigned char **pp, long len)
{
	DSAPublicKey data;
	DSA *k = NULL;
	size_t size;
	int ret;

	ret = decode_DSAPublicKey(*pp, len, &data, &size);
	if (ret) {
		return (NULL);
	}

	*pp += size;

	if (dsa != NULL) {
		k = *dsa;
	}

	if (NULL == k) {
		k = DSA_new();
		if (NULL == k) {
			free_DSAPublicKey(&data);
			return (NULL);
		}
	}

	k->pub_key = _cs_integer_to_BN(&data, NULL);
	free_DSAPublicKey(&data);

	if (NULL == k->pub_key) {
		DSA_free(k);
		return (NULL);
	}

	if (dsa != NULL) {
		*dsa = k;
	}

	return (k);
}


int
i2d_DSAPrivateKey(const DSA *dsa, unsigned char **pp)
{
	DSAPrivateKey data;
	size_t size;
	int ret;

	if ((NULL == dsa->p) || (NULL == dsa->q) || (NULL == dsa->g) ||
	    (NULL == dsa->pub_key) || (NULL == dsa->priv_key)) {
		return (-1);
	}

	memset(&data, 0, sizeof(data));

	ret = _cs_BN_to_integer(dsa->p, &data.p);
	ret |= _cs_BN_to_integer(dsa->q, &data.q);
	ret |= _cs_BN_to_integer(dsa->g, &data.g);
	ret |= _cs_BN_to_integer(dsa->pub_key, &data.pub_key);
	ret |= _cs_BN_to_integer(dsa->priv_key, &data.priv_key);
	if (ret) {
		free_DSAPrivateKey(&data);
		return (-1);
	}

	if (NULL == pp) {
		size = length_DSAPrivateKey(&data);
		free_DSAPrivateKey(&data);
	} else {
		void *p;
		size_t len;

		ASN1_MALLOC_ENCODE(DSAPrivateKey, p, len, &data, &size, ret);
		free_DSAPrivateKey(&data);
		if (ret) {
			return (-1);
		}
		if (len != size) {
			abort();
		}

		memcpy(*pp, p, size);
		free(p);

		*pp += size;
	}

	return (size);
}


int
i2d_DSAPublicKey(const DSA *dsa, unsigned char **pp)
{
	DSAPublicKey data;
	size_t size;
	int ret;

	if (NULL == dsa->pub_key) {
		return (-1);
	}

	memset(&data, 0, sizeof(data));
	ret = _cs_BN_to_integer(dsa->pub_key, &data);
	if (ret) {
		free_DSAPublicKey(&data);
		return (-1);
	}

	if (NULL == pp) {
		size = length_DSAPublicKey(&data);
		free_DSAPublicKey(&data);
	} else {
		void *p;
		size_t len;

		ASN1_MALLOC_ENCODE(DSAPublicKey, p, len, &data, &size, ret);
		free_DSAPublicKey(&data);
		if (ret) {
			return (-1);
		}
		if (len != size) {
			abort();
		}

		memcpy(*pp, p, size);
		free(p);

		*pp += size;
	}

	return (size);
}


#elif defined(USE_EAY_ASN1)

/*
 * ossl eay asn1
 */

#include "ossl-asn1.h"
#include "ossl-asn1t.h"

/* Override the default new methods */
static int
sig_cb(int operation, ASN1_VALUE **pval, const ASN1_ITEM *it)
{
	if (operation == ASN1_OP_NEW_PRE) {
		DSA_SIG *sig;
		sig = malloc(sizeof(DSA_SIG));
		sig->r = NULL;
		sig->s = NULL;
		*pval = (ASN1_VALUE *)sig;
		if (sig) {
			return (2);
		}
		/* DSAerr(DSA_F_SIG_CB, ERR_R_MALLOC_FAILURE); */
		return (0);
	}
	return (1);
}


ASN1_SEQUENCE_cb(DSA_SIG, sig_cb) =
{
	ASN1_SIMPLE(DSA_SIG, r, CBIGNUM),
	ASN1_SIMPLE(DSA_SIG, s, CBIGNUM)
}


ASN1_SEQUENCE_END_cb(DSA_SIG, DSA_SIG)

IMPLEMENT_ASN1_ENCODE_FUNCTIONS_const_fname(DSA_SIG, DSA_SIG, DSA_SIG)

/* Override the default free and new methods */
static int
dsa_cb(int operation, ASN1_VALUE **pval, const ASN1_ITEM *it)
{
	if (operation == ASN1_OP_NEW_PRE) {
		*pval = (ASN1_VALUE *)DSA_new();
		if (*pval) {
			return (2);
		}
		return (0);
	} else if (operation == ASN1_OP_FREE_PRE) {
		DSA_free((DSA *)*pval);
		*pval = NULL;
		return (2);
	}
	return (1);
}


ASN1_SEQUENCE_cb(DSAPrivateKey, dsa_cb) =
{
	ASN1_SIMPLE(DSA, version,  LONG),
	ASN1_SIMPLE(DSA, p,	   BIGNUM),
	ASN1_SIMPLE(DSA, q,	   BIGNUM),
	ASN1_SIMPLE(DSA, g,	   BIGNUM),
	ASN1_SIMPLE(DSA, pub_key,  BIGNUM),
	ASN1_SIMPLE(DSA, priv_key, BIGNUM)
}


ASN1_SEQUENCE_END_cb(DSA, DSAPrivateKey)

IMPLEMENT_ASN1_ENCODE_FUNCTIONS_const_fname(DSA, DSAPrivateKey, DSAPrivateKey)

ASN1_SEQUENCE_cb(DSAparams, dsa_cb) =
{
	ASN1_SIMPLE(DSA, p, BIGNUM),
	ASN1_SIMPLE(DSA, q, BIGNUM),
	ASN1_SIMPLE(DSA, g, BIGNUM),
}


ASN1_SEQUENCE_END_cb(DSA, DSAparams)

IMPLEMENT_ASN1_ENCODE_FUNCTIONS_const_fname(DSA, DSAparams, DSAparams)

/* DSA public key is a bit trickier... its effectively a CHOICE type
 * decided by a field called write_params which can either write out
 * just the public key as an INTEGER or the parameters and public key
 * in a SEQUENCE
 */

ASN1_SEQUENCE(dsa_pub_internal) =
{
	ASN1_SIMPLE(DSA, pub_key, BIGNUM),
	ASN1_SIMPLE(DSA, p,	  BIGNUM),
	ASN1_SIMPLE(DSA, q,	  BIGNUM),
	ASN1_SIMPLE(DSA, g,	  BIGNUM)
}


ASN1_SEQUENCE_END_name(DSA, dsa_pub_internal)

ASN1_CHOICE_cb(DSAPublicKey, dsa_cb) =
{
	ASN1_SIMPLE(DSA,   pub_key, BIGNUM),
	ASN1_EX_COMBINE(0,	 0, dsa_pub_internal)
}


ASN1_CHOICE_END_cb(DSA, DSAPublicKey, write_params)

IMPLEMENT_ASN1_ENCODE_FUNCTIONS_const_fname(DSA, DSAPublicKey, DSAPublicKey)

int
DSA_sign(int type, const unsigned char *dgst, int dlen, unsigned char *sig,
    unsigned int *siglen, DSA *dsa)
{
	DSA_SIG *s;

	RAND_seed(dgst, dlen);
	s = DSA_do_sign(dgst, dlen, dsa);
	if (s == NULL) {
		*siglen = 0;
		return (0);
	}
	*siglen = i2d_DSA_SIG(s, &sig);
	DSA_SIG_free(s);
	return (1);
}


int
DSA_size(const DSA *r)
{
	int ret, i;
	ASN1_INTEGER bs;
	unsigned char buf[4];   /* 4 bytes looks really small.
	                         * However, i2d_ASN1_INTEGER() will not look
	                         * beyond the first byte, as long as the second
	                         * parameter is NULL. */

	i = BN_num_bits(r->q);
	bs.length = (i + 7) / 8;
	bs.data = buf;
	bs.type = V_ASN1_INTEGER;
	/* If the top bit is set the asn1 encoding is 1 larger. */
	buf[0] = 0xff;

	i = i2d_ASN1_INTEGER(&bs, NULL);
	i += i; /* r and s */
	ret = ASN1_object_size(1, i, V_ASN1_SEQUENCE);
	return (ret);
}


/* data has already been hashed (probably with SHA or SHA-1). */

/* returns
 *      1: correct signature
 *      0: incorrect signature
 *     -1: error
 */
int
DSA_verify(int type, const unsigned char *dgst, int dgst_len,
    const unsigned char *sigbuf, int siglen, DSA *dsa)
{
	DSA_SIG *s;

	int ret = -1;

	s = DSA_SIG_new();
	if (s == NULL) {
		return (ret);
	}
	if (d2i_DSA_SIG(&s, &sigbuf, siglen) == NULL) {
		goto err;
	}
	ret = DSA_do_verify(dgst, dgst_len, s, dsa);
err:
	DSA_SIG_free(s);
	return (ret);
}


#else

#error "Unknown ASN1 implementation"

#endif /* ASN1 */

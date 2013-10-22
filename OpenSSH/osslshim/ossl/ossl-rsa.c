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

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ossl-objects.h"
#include "ossl-evp.h"
#include "ossl-rsa.h"
#include "ossl-bn.h"


/**
 * @page page_rsa RSA - public-key cryptography
 *
 * RSA is named by its inventors (Ron Rivest, Adi Shamir, and Leonard
 * Adleman) (published in 1977), patented expired in 21 September 2000.
 *
 *
 * Speed for RSA in seconds
 *   no key blinding
 *   1000 iteration,
 *   same rsa keys (1024 and 2048)
 *   operation performed each eteration sign, verify, encrypt, decrypt on a random bit pattern
 *
 * name		1024	2048	4098
 * =================================
 * gmp:          0.73	  6.60	 44.80
 * tfm:          2.45	    --	    --
 * ltm:		 3.79	 20.74	105.41	(default in hcrypto)
 * openssl:	 4.04	 11.90	 82.59
 * cdsa:	15.89	102.89	721.40
 * imath:       40.62	    --	    --
 *
 * See the library functions here: @ref hcrypto_rsa
 */

/**
 * Same as RSA_new_method() using NULL as engine.
 *
 * @return a newly allocated RSA object. Free with RSA_free().
 */
RSA *
RSA_new(void)
{
	return (RSA_new_method(NULL));
}


/**
 * Allocate a new RSA object using the engine, if NULL is specified as
 * the engine, use the default RSA engine as returned by
 * ENGINE_get_default_RSA().
 *
 * @param engine Specific what ENGINE RSA provider should be used.
 *
 * @return a newly allocated RSA object. Free with RSA_free().
 */
RSA *
RSA_new_method(ENGINE *engine)
{
	RSA *rsa;

	rsa = calloc(1, sizeof(*rsa));
	if (rsa == NULL) {
		return (NULL);
	}

	rsa->references = 1;

	if (engine) {
		ENGINE_up_ref(engine);
		rsa->engine = engine;
	} else {
		rsa->engine = ENGINE_get_default_RSA();
	}

	if (rsa->engine) {
		rsa->meth = ENGINE_get_RSA(rsa->engine);
		if (rsa->meth == NULL) {
			ENGINE_finish(engine);
			free(rsa);
			return (0);
		}
	}

	if (rsa->meth == NULL) {
		rsa->meth = RSA_get_default_method();
	}

	(*rsa->meth->init)(rsa);

	return (rsa);
}


/**
 * Free an allocation RSA object.
 *
 * @param rsa the RSA object to free.
 */
void
RSA_free(RSA *rsa)
{
	if (rsa->references <= 0) {
		abort();
	}

	if (--rsa->references > 0) { /* XXX replace w/ atomic op */
		return;
	}

	(*rsa->meth->finish)(rsa);

	if (rsa->engine) {
		ENGINE_finish(rsa->engine);
	}

#define free_if(f)    if (f) { BN_clear_free(f); }
	free_if(rsa->n);
	free_if(rsa->e);
	free_if(rsa->d);
	free_if(rsa->p);
	free_if(rsa->q);
	free_if(rsa->dmp1);
	free_if(rsa->dmq1);
	free_if(rsa->iqmp);
#undef free_if

	memset(rsa, 0, sizeof(*rsa));
	free(rsa);
}


/**
 * Add an extra reference to the RSA object. The object should be free
 * with RSA_free() to drop the reference.
 *
 * @param rsa the object to add reference counting too.
 *
 * @return the current reference count, can't safely be used except
 * for debug printing.
 */
int
RSA_up_ref(RSA *rsa)
{
	return (++rsa->references); /* XXX replace w/ atomic op */
}


/**
 * Return the RSA_METHOD used for this RSA object.
 *
 * @param rsa the object to get the method from.
 *
 * @return the method used for this RSA object.
 */
const RSA_METHOD *
RSA_get_method(const RSA *rsa)
{
	return (rsa->meth);
}


/**
 * Set a new method for the RSA keypair.
 *
 * @param rsa rsa parameter.
 * @param method the new method for the RSA parameter.
 *
 * @return 1 on success.
 */
int
RSA_set_method(RSA *rsa, const RSA_METHOD *method)
{
	(*rsa->meth->finish)(rsa);

	if (rsa->engine) {
		ENGINE_finish(rsa->engine);
		rsa->engine = NULL;
	}

	rsa->meth = method;
	(*rsa->meth->init)(rsa);
	return (1);
}


/**
 * Set the application data for the RSA object.
 *
 * @param rsa the rsa object to set the parameter for
 * @param arg the data object to store
 *
 * @return 1 on success.
 */
int
RSA_set_app_data(RSA *rsa, void *arg)
{
	rsa->ex_data.sk = arg;
	return (1);
}


/**
 * Get the application data for the RSA object.
 *
 * @param rsa the rsa object to get the parameter for
 *
 * @return the data object
 */
void *
RSA_get_app_data(const RSA *rsa)
{
	return (rsa->ex_data.sk);
}


int
RSA_check_key(const RSA *key)
{
	BIGNUM *i, *j, *k, *l, *m;
	BN_CTX *ctx;
	int r;
	int ret = 1;

	i = BN_new();
	j = BN_new();
	k = BN_new();
	l = BN_new();
	m = BN_new();
	ctx = BN_CTX_new();
	if ((i == NULL) || (j == NULL) || (k == NULL) || (l == NULL) ||
	    (m == NULL) || (ctx == NULL)) {
		ret = -1;
		/* RSAerr(RSA_F_RSA_CHECK_KEY, ERR_R_MALLOC_FAILURE); */
		goto err;
	}

	/* p prime? */
	r = BN_is_prime_ex(key->p, BN_prime_checks, NULL, NULL);
	if (r != 1) {
		ret = r;
		if (r != 0) {
			goto err;
		}
		/* RSAerr(RSA_F_RSA_CHECK_KEY, RSA_R_P_NOT_PRIME); */
	}

	/* q prime? */
	r = BN_is_prime_ex(key->q, BN_prime_checks, NULL, NULL);
	if (r != 1) {
		ret = r;
		if (r != 0) {
			goto err;
		}
		/* RSAerr(RSA_F_RSA_CHECK_KEY, RSA_R_Q_NOT_PRIME); */
	}

	/* n = p*q? */
	r = BN_mul(i, key->p, key->q, ctx);
	if (!r) {
		ret = -1;
		goto err;
	}

	if (BN_cmp(i, key->n) != 0) {
		ret = 0;
		/* RSAerr(RSA_F_RSA_CHECK_KEY, RSA_R_N_DOES_NOT_EQUAL_P_Q); */
	}

	/* d*e = 1  mod lcm(p-1,q-1)? */

	r = BN_sub(i, key->p, BN_value_one());
	if (!r) {
		ret = -1;
		goto err;
	}
	r = BN_sub(j, key->q, BN_value_one());
	if (!r) {
		ret = -1;
		goto err;
	}

	/* now compute k = lcm(i,j) */
	r = BN_mul(l, i, j, ctx);
	if (!r) {
		ret = -1;
		goto err;
	}
	r = BN_gcd(m, i, j, ctx);
	if (!r) {
		ret = -1;
		goto err;
	}
	r = BN_div(k, NULL, l, m, ctx); /* remainder is 0 */
	if (!r) {
		ret = -1;
		goto err;
	}

	r = BN_mod_mul(i, key->d, key->e, k, ctx);
	if (!r) {
		ret = -1;
		goto err;
	}

	if (!BN_is_one(i)) {
		ret = 0;
		/* RSAerr(RSA_F_RSA_CHECK_KEY, RSA_R_D_E_NOT_CONGRUENT_TO_1); */
	}

	if ((key->dmp1 != NULL) && (key->dmq1 != NULL) && (key->iqmp != NULL)) {
		/* dmp1 = d mod (p-1)? */
		r = BN_sub(i, key->p, BN_value_one());
		if (!r) {
			ret = -1;
			goto err;
		}

		r = BN_mod(j, key->d, i, ctx);
		if (!r) {
			ret = -1;
			goto err;
		}

		if (BN_cmp(j, key->dmp1) != 0) {
			ret = 0;

			/*	RSAerr(RSA_F_RSA_CHECK_KEY,
			 *      RSA_R_DMP1_NOT_CONGRUENT_TO_D); */
		}

		/* dmq1 = d mod (q-1)? */
		r = BN_sub(i, key->q, BN_value_one());
		if (!r) {
			ret = -1;
			goto err;
		}

		r = BN_mod(j, key->d, i, ctx);
		if (!r) {
			ret = -1;
			goto err;
		}

		if (BN_cmp(j, key->dmq1) != 0) {
			ret = 0;

			/* RSAerr(RSA_F_RSA_CHECK_KEY,
			 *      RSA_R_DMQ1_NOT_CONGRUENT_TO_D); */
		}

		/* iqmp = q^-1 mod p? */
		if (!BN_mod_inverse(i, key->q, key->p, ctx)) {
			ret = -1;
			goto err;
		}

		if (BN_cmp(i, key->iqmp) != 0) {
			ret = 0;

			/* RSAerr(RSA_F_RSA_CHECK_KEY,
			 *      RSA_R_IQMP_NOT_INVERSE_OF_Q); */
		}
	}

err:
	if (i != NULL) {
		BN_free(i);
	}
	if (j != NULL) {
		BN_free(j);
	}
	if (k != NULL) {
		BN_free(k);
	}
	if (l != NULL) {
		BN_free(l);
	}
	if (m != NULL) {
		BN_free(m);
	}
	if (ctx != NULL) {
		BN_CTX_free(ctx);
	}

	return (ret);
}


int
RSA_size(const RSA *rsa)
{
	return ((BN_num_bits(rsa->n) + 7) / 8);
}


#define RSAFUNC(name, body)							  \
	int									  \
	name(int flen, const unsigned char *f, unsigned char *t, RSA* r, int p) { \
		return body;							  \
	}

RSAFUNC(RSA_public_encrypt, (r)->meth->rsa_pub_enc(flen, f, t, r, p))
RSAFUNC(RSA_public_decrypt, (r)->meth->rsa_pub_dec(flen, f, t, r, p))
RSAFUNC(RSA_private_encrypt, (r)->meth->rsa_priv_enc(flen, f, t, r, p))
RSAFUNC(RSA_private_decrypt, (r)->meth->rsa_priv_dec(flen, f, t, r, p))

/*
 * A NULL RSA_METHOD that returns failure for all operations. This is
 * used as the default RSA method if we don't have any native
 * support.
 */

static RSAFUNC(null_rsa_public_encrypt, -1)
static RSAFUNC(null_rsa_public_decrypt, -1)
static RSAFUNC(null_rsa_private_encrypt, -1)
static RSAFUNC(null_rsa_private_decrypt, -1)

/*
 *
 */
int
RSA_generate_key_ex(RSA *r, int bits, BIGNUM *e, BN_GENCB *cb)
{
	if (r->meth->rsa_keygen) {
		return ((*r->meth->rsa_keygen)(r, bits, e, cb));
	}
	return (0);
}


/*
 * NULL rsa
 */
static int
null_rsa_init(RSA *rsa)
{
	return (1);
}


static int
null_rsa_finish(RSA *rsa)
{
	return (1);
}


static const RSA_METHOD rsa_null_method =
{
	.name		= "cryptoshim null RSA",
	.rsa_pub_enc	= null_rsa_public_encrypt,
	.rsa_pub_dec	= null_rsa_public_decrypt,
	.rsa_priv_enc	= null_rsa_private_encrypt,
	.rsa_priv_dec	= null_rsa_private_decrypt,
	.rsa_mod_exp	= NULL,
	.bn_mod_exp	= NULL,
	.init		= null_rsa_init,
	.finish		= null_rsa_finish,
	.flags		= 0,
	.app_data	= NULL,
	.rsa_sign	= NULL,
	.rsa_verify	= NULL,
	.rsa_keygen	= NULL
};

const RSA_METHOD *
RSA_null_method(void)
{
	return (&rsa_null_method);
}


/*
 * XXX <radr://problem/107832242> and <rda://problem/8174874> are
 * blocking the use of CommonCrypto's RSA code for OpenSSH.
 */
#if !defined(PR_10783242_FIXED) || !defined(PR_8174774_FIXED)
extern const RSA_METHOD _ossl_rsa_eay_method;
static const RSA_METHOD *default_rsa_method = &_ossl_rsa_eay_method;
#elif HAVE_COMMONCRYPTO_COMMONRSACRYPTOR_H
extern const RSA_METHOD _ossl_rsa_cc_method;
static const RSA_METHOD *default_rsa_method = &_ossl_rsa_cc_method;
#elif difend(HAVE_CDSA)
extern const RSA_METHOD _ossl_rsa_cdsa_method;
static const RSA_METHOD *default_rsa_method = &_ossl_rsa_cdsa_method;
#elif defined(__APPLE_TARGET_EMBEDDED__)
static const RSA_METHOD *default_rsa_method = &rsa_null_method;
#elif defined(HAVE_GMP)
extern const RSA_METHOD _ossl_rsa_gmp_method;
static const RSA_METHOD *default_rsa_method = &_ossl_rsa_gmp_method;
#elif defined(HEIM_HC_LTM)
extern const RSA_METHOD _ossl_rsa_ltm_method;
static const RSA_METHOD *default_rsa_method = &_ossl_rsa_ltm_method;
#else
static const RSA_METHOD *default_rsa_method = &rsa_null_method;
#endif

const RSA_METHOD *
RSA_get_default_method(void)
{
	return (default_rsa_method);
}


void
RSA_set_default_method(const RSA_METHOD *meth)
{
	default_rsa_method = meth;
}


int
RSA_blinding_on(RSA *rsa __unused, BN_CTX *ctx __unused)
{
	/* XXX this needs to be fixed */
	return (1);
}


/*
 * ASN1 dependent code
 */
#if defined(USE_HEIMDAL_ASN1)

/*
 * Use heimdal asn1 compiler generated code
 */

#include "krb5-types.h"
#include "rfc2459_asn1.h"
#include "der.h"
#include "ossl-common.h"

static heim_octet_string null_entry_oid = { 2, "\x05\x00" };

static unsigned sha1_oid_tree[] = { 1, 3, 14, 3, 2, 26 };
static const AlgorithmIdentifier _signature_sha1_data =
{
	{ 6, sha1_oid_tree }, &null_entry_oid
};
static unsigned sha256_oid_tree[] = { 2, 16, 840, 1, 101, 3, 4, 2, 1 };
static const AlgorithmIdentifier _signature_sha256_data =
{
	{ 9, sha256_oid_tree }, &null_entry_oid
};
static unsigned sha384_oid_tree[] = { 2, 16, 840, 1, 101, 3, 4, 2, 2 };
const AlgorithmIdentifier _signature_sha384_data =
{
	{ 9, sha384_oid_tree }, &null_entry_oid
};
static unsigned sha512_oid_tree[] = { 2, 16, 840, 1, 101, 3, 4, 2, 3 };
const AlgorithmIdentifier _signature_sha512_data =
{
	{ 9, sha512_oid_tree }, &null_entry_oid
};
static unsigned md5_oid_tree[] = { 1, 2, 840, 113549, 2, 5 };
static const AlgorithmIdentifier _signature_md5_data =
{
	{ 6, md5_oid_tree }, &null_entry_oid
};

int
RSA_sign(int type, const unsigned char *from, unsigned int flen,
    unsigned char *to, unsigned int *tlen, RSA *rsa)
{
	if (rsa->meth->rsa_sign) {
		return (rsa->meth->rsa_sign(type, from, flen, to, tlen, rsa));
	}

	if (rsa->meth->rsa_priv_enc) {
		heim_octet_string indata;
		DigestInfo di;
		size_t size;
		int ret;

		memset(&di, 0, sizeof(di));

		if (type == NID_sha1) {
			di.digestAlgorithm = _signature_sha1_data;
		} else if (type == NID_md5) {
			di.digestAlgorithm = _signature_md5_data;
		} else if (type == NID_sha256) {
			di.digestAlgorithm = _signature_sha256_data;
		} else if (type == NID_sha384) {
			di.digestAlgorithm = _signature_sha384_data;
		} else if (type == NID_sha512) {
			di.digestAlgorithm = _signature_sha512_data;
		} else{
			return (-1);
		}

		di.digest.data = (void *)from;
		di.digest.length = flen;

		ASN1_MALLOC_ENCODE(DigestInfo,
		    indata.data,
		    indata.length,
		    &di,
		    &size,
		    ret);
		if (ret) {
			return (ret);
		}
		if (indata.length != size) {
			abort();
		}

		ret = rsa->meth->rsa_priv_enc(indata.length, indata.data, to,
			rsa, RSA_PKCS1_PADDING);
		free(indata.data);
		if (ret > 0) {
			*tlen = ret;
			ret = 1;
		} else{
			ret = 0;
		}

		return (ret);
	}

	return (0);
}


int
RSA_verify(int type, const unsigned char *from, unsigned int flen,
    unsigned char *sigbuf, unsigned int siglen, RSA *rsa)
{
	if (rsa->meth->rsa_verify) {
		return (rsa->meth->rsa_verify(type, from, flen, sigbuf, siglen, rsa));
	}

	if (rsa->meth->rsa_pub_dec) {
		const AlgorithmIdentifier *digest_alg;
		void *data;
		DigestInfo di;
		size_t size;
		int ret, ret2;

		data = malloc(RSA_size(rsa));
		if (data == NULL) {
			return (-1);
		}

		memset(&di, 0, sizeof(di));

		ret = rsa->meth->rsa_pub_dec(siglen, sigbuf, data, rsa, RSA_PKCS1_PADDING);
		if (ret <= 0) {
			free(data);
			return (-2);
		}

		ret2 = decode_DigestInfo(data, ret, &di, &size);
		free(data);
		if (ret2 != 0) {
			return (-3);
		}
		if (ret != size) {
			free_DigestInfo(&di);
			return (-4);
		}

		if ((flen != di.digest.length) || (memcmp(di.digest.data, from, flen) != 0)) {
			free_DigestInfo(&di);
			return (-5);
		}

		if (type == NID_sha1) {
			digest_alg = &_signature_sha1_data;
		} else if (type == NID_md5) {
			digest_alg = &_signature_md5_data;
		} else if (type == NID_sha256) {
			digest_alg = &_signature_sha256_data;
		} else {
			free_DigestInfo(&di);
			return (-1);
		}

		ret = der_heim_oid_cmp(&digest_alg->algorithm,
			&di.digestAlgorithm.algorithm);
		free_DigestInfo(&di);

		if (ret != 0) {
			return (0);
		}
		return (1);
	}

	return (0);
}


RSA *
d2i_RSAPrivateKey(RSA **rsa, const unsigned char **pp, long len)
{
	RSAPrivateKey data;
	RSA *k = NULL;
	size_t size;
	int ret;

	if (rsa != NULL) {
		k = *rsa;
	}

	ret = decode_RSAPrivateKey(*pp, len, &data, &size);
	if (ret) {
		return (NULL);
	}

	*pp += size;

	if (k == NULL) {
		k = RSA_new();
		if (k == NULL) {
			free_RSAPrivateKey(&data);
			return (NULL);
		}
	}

	k->n = _cs_integer_to_BN(&data.modulus, NULL);
	k->e = _cs_integer_to_BN(&data.publicExponent, NULL);
	k->d = _cs_integer_to_BN(&data.privateExponent, NULL);
	k->p = _cs_integer_to_BN(&data.prime1, NULL);
	k->q = _cs_integer_to_BN(&data.prime2, NULL);
	k->dmp1 = _cs_integer_to_BN(&data.exponent1, NULL);
	k->dmq1 = _cs_integer_to_BN(&data.exponent2, NULL);
	k->iqmp = _cs_integer_to_BN(&data.coefficient, NULL);
	free_RSAPrivateKey(&data);

	if ((k->n == NULL) || (k->e == NULL) || (k->d == NULL) || (k->p == NULL) ||
	    (k->q == NULL) || (k->dmp1 == NULL) || (k->dmq1 == NULL) || (k->iqmp == NULL)) {
		RSA_free(k);
		return (NULL);
	}

	if (rsa != NULL) {
		*rsa = k;
	}

	return (k);
}


int
i2d_RSAPrivateKey(const RSA *rsa, unsigned char **pp)
{
	RSAPrivateKey data;
	size_t size;
	int ret;

	if ((rsa->n == NULL) || (rsa->e == NULL) || (rsa->d == NULL) || (rsa->p == NULL) ||
	    (rsa->q == NULL) || (rsa->dmp1 == NULL) || (rsa->dmq1 == NULL) ||
	    (rsa->iqmp == NULL)) {
		return (-1);
	}

	memset(&data, 0, sizeof(data));

	ret = _cs_BN_to_integer(rsa->n, &data.modulus);
	ret |= _cs_BN_to_integer(rsa->e, &data.publicExponent);
	ret |= _cs_BN_to_integer(rsa->d, &data.privateExponent);
	ret |= _cs_BN_to_integer(rsa->p, &data.prime1);
	ret |= _cs_BN_to_integer(rsa->q, &data.prime2);
	ret |= _cs_BN_to_integer(rsa->dmp1, &data.exponent1);
	ret |= _cs_BN_to_integer(rsa->dmq1, &data.exponent2);
	ret |= _cs_BN_to_integer(rsa->iqmp, &data.coefficient);
	if (ret) {
		free_RSAPrivateKey(&data);
		return (-1);
	}

	if (pp == NULL) {
		size = length_RSAPrivateKey(&data);
		free_RSAPrivateKey(&data);
	} else {
		void *p;
		size_t len;

		ASN1_MALLOC_ENCODE(RSAPrivateKey, p, len, &data, &size, ret);
		free_RSAPrivateKey(&data);
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
i2d_RSAPublicKey(const RSA *rsa, unsigned char **pp)
{
	RSAPublicKey data;
	size_t size;
	int ret;

	memset(&data, 0, sizeof(data));

	if (_cs_BN_to_integer(rsa->n, &data.modulus) ||
	    _cs_BN_to_integer(rsa->e, &data.publicExponent)) {
		free_RSAPublicKey(&data);
		return (-1);
	}

	if (pp == NULL) {
		size = length_RSAPublicKey(&data);
		free_RSAPublicKey(&data);
	} else {
		void *p;
		size_t len;

		ASN1_MALLOC_ENCODE(RSAPublicKey, p, len, &data, &size, ret);
		free_RSAPublicKey(&data);
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


RSA *
d2i_RSAPublicKey(RSA **rsa, const unsigned char **pp, long len)
{
	RSAPublicKey data;
	RSA *k = NULL;
	size_t size;
	int ret;

	if (rsa != NULL) {
		k = *rsa;
	}

	ret = decode_RSAPublicKey(*pp, len, &data, &size);
	if (ret) {
		return (NULL);
	}

	*pp += size;

	if (k == NULL) {
		k = RSA_new();
		if (k == NULL) {
			free_RSAPublicKey(&data);
			return (NULL);
		}
	}

	k->n = _cs_integer_to_BN(&data.modulus, NULL);
	k->e = _cs_integer_to_BN(&data.publicExponent, NULL);

	free_RSAPublicKey(&data);

	if ((k->n == NULL) || (k->e == NULL)) {
		RSA_free(k);
		return (NULL);
	}

	if (rsa != NULL) {
		*rsa = k;
	}

	return (k);
}


#elif defined(USE_EAY_ASN1)

/*
 * Use ossl asn1 bits
 */

#include "cs-asn1.h"
#include "cs-asn1t.h"
#include "cs-stack.h"

static ASN1_METHOD method =
{
	(I2D_OF(void))i2d_RSAPrivateKey,
	(D2I_OF(void))d2i_RSAPrivateKey,
	(void *(*)(void))RSA_new,
	(void (*)(void *))RSA_free
};

ASN1_METHOD *
RSAPrivateKey_asn1_meth(void)
{
	return (&method);
}


/* Override the default free and new methods */
static int
rsa_cb(int operation, ASN1_VALUE **pval, const ASN1_ITEM *it)
{
	if (operation == ASN1_OP_NEW_PRE) {
		*pval = (ASN1_VALUE *)RSA_new();
		if (*pval) {
			return (2);
		}
		return (0);
	} else if (operation == ASN1_OP_FREE_PRE) {
		RSA_free((RSA *)*pval);
		*pval = NULL;
		return (2);
	}
	return (1);
}


ASN1_SEQUENCE_cb(RSAPrivateKey, rsa_cb) =
{
	ASN1_SIMPLE(RSA, version, LONG),
	ASN1_SIMPLE(RSA, n,	  BIGNUM),
	ASN1_SIMPLE(RSA, e,	  BIGNUM),
	ASN1_SIMPLE(RSA, d,	  BIGNUM),
	ASN1_SIMPLE(RSA, p,	  BIGNUM),
	ASN1_SIMPLE(RSA, q,	  BIGNUM),
	ASN1_SIMPLE(RSA, dmp1,	  BIGNUM),
	ASN1_SIMPLE(RSA, dmq1,	  BIGNUM),
	ASN1_SIMPLE(RSA, iqmp,	  BIGNUM)
}


ASN1_SEQUENCE_END_cb(RSA, RSAPrivateKey)


ASN1_SEQUENCE_cb(RSAPublicKey, rsa_cb) =
{
	ASN1_SIMPLE(RSA, n, BIGNUM),
	ASN1_SIMPLE(RSA, e, BIGNUM),
}


ASN1_SEQUENCE_END_cb(RSA, RSAPublicKey)

IMPLEMENT_ASN1_ENCODE_FUNCTIONS_const_fname(RSA, RSAPrivateKey, RSAPrivateKey)

IMPLEMENT_ASN1_ENCODE_FUNCTIONS_const_fname(RSA, RSAPublicKey, RSAPublicKey)

#if 0
RSA *
RSAPublicKey_dup(RSA *rsa)
{
	return (ASN1_item_dup(ASN1_ITEM_rptr(RSAPublicKey), rsa));
}

RSA *
RSAPrivateKey_dup(RSA *rsa)
{
	return (ASN1_item_dup(ASN1_ITEM_rptr(RSAPrivateKey), rsa));
}


#endif

/* #include <openssl/x509.h> */

/* Size of an SSL signature: MD5+SHA1 */
#define SSL_SIG_LENGTH    36

typedef struct X509_algor_st {
	ASN1_OBJECT *	algorithm;
	ASN1_TYPE *	parameter;
} X509_ALGOR;

typedef struct X509_sig_st {
	X509_ALGOR *		algor;
	ASN1_OCTET_STRING *	digest;
} X509_SIG;

ASN1_SEQUENCE(X509_ALGOR) =
{
	ASN1_SIMPLE(X509_ALGOR, algorithm, ASN1_OBJECT),
	ASN1_OPT(X509_ALGOR,	parameter, ASN1_ANY)
}


ASN1_SEQUENCE_END(X509_ALGOR)

DECLARE_ASN1_SET_OF(X509_ALGOR)
DECLARE_ASN1_ITEM(X509_ALGOR)

ASN1_SEQUENCE(X509_SIG) =
{
	ASN1_SIMPLE(X509_SIG, algor,  X509_ALGOR),
	ASN1_SIMPLE(X509_SIG, digest, ASN1_OCTET_STRING)
}


ASN1_SEQUENCE_END(X509_SIG)

IMPLEMENT_ASN1_FUNCTIONS(X509_SIG)

int
RSA_sign(int type, const unsigned char *m, unsigned int m_len,
    unsigned char *sigret, unsigned int *siglen, RSA *rsa)
{
	X509_SIG sig;
	ASN1_TYPE parameter;
	int i, j, ret = 1;
	unsigned char *p, *tmps = NULL;
	const unsigned char *s = NULL;
	X509_ALGOR algor;
	ASN1_OCTET_STRING digest;

	/*
	 * if((rsa->flags & RSA_FLAG_SIGN_VER) && rsa->meth->rsa_sign) {
	 *      return rsa->meth->rsa_sign(type, m, m_len,
	 *              sigret, siglen, rsa);
	 * }
	 */
	/* Special case: SSL signature, just check the length */
	if (type == NID_md5_sha1) {
		if (m_len != SSL_SIG_LENGTH) {
			/* RSAerr(RSA_F_RSA_SIGN,RSA_R_INVALID_MESSAGE_LENGTH); */
			return (0);
		}
		i = SSL_SIG_LENGTH;
		s = m;
	} else {
		sig.algor = &algor;
		sig.algor->algorithm = OBJ_nid2obj(type);
		if (sig.algor->algorithm == NULL) {
			/* RSAerr(RSA_F_RSA_SIGN,RSA_R_UNKNOWN_ALGORITHM_TYPE); */
			return (0);
		}
		if (sig.algor->algorithm->length == 0) {
			/* RSAerr(RSA_F_RSA_SIGN,RSA_R_THE_ASN1_OBJECT_IDENTIFIER_IS_NOT_KNOWN_FOR_THIS_MD); */
			return (0);
		}
		parameter.type = V_ASN1_NULL;
		parameter.value.ptr = NULL;
		sig.algor->parameter = &parameter;

		sig.digest = &digest;
		sig.digest->data = (unsigned char *)m; /* TMP UGLY CAST */
		sig.digest->length = m_len;

		i = i2d_X509_SIG(&sig, NULL);
	}
	j = RSA_size(rsa);
	if (i > (j-RSA_PKCS1_PADDING_SIZE)) {
		/* RSAerr(RSA_F_RSA_SIGN,RSA_R_DIGEST_TOO_BIG_FOR_RSA_KEY); */
		return (0);
	}
	if (type != NID_md5_sha1) {
		tmps = (unsigned char *)malloc((unsigned int)j+1);
		if (tmps == NULL) {
			/* RSAerr(RSA_F_RSA_SIGN,ERR_R_MALLOC_FAILURE); */
			return (0);
		}
		p = tmps;
		i2d_X509_SIG(&sig, &p);
		s = tmps;
	}
	i = RSA_private_encrypt(i, s, sigret, rsa, RSA_PKCS1_PADDING);
	if (i <= 0) {
		ret = 0;
	} else{
		*siglen = i;
	}

	if (type != NID_md5_sha1) {
		memset(tmps, 0, (unsigned int)j + 1);
		free(tmps);
	}
	return (ret);
}


int
RSA_verify(int dtype, const unsigned char *m, unsigned int m_len,
    unsigned char *sigbuf, unsigned int siglen, RSA *rsa)
{
	int i, ret = 0, sigtype;
	unsigned char *s;
	X509_SIG *sig = NULL;

	if (siglen != (unsigned int)RSA_size(rsa)) {
		/* RSAerr(RSA_F_RSA_VERIFY,RSA_R_WRONG_SIGNATURE_LENGTH); */
		return (0);
	}

	/*
	 * if ((rsa->flags & RSA_FLAG_SIGN_VER) && rsa->meth->rsa_verify) {
	 *      return rsa->meth->rsa_verify(dtype, m, m_len,
	 *              sigbuf, siglen, rsa);
	 * }
	 */

	s = (unsigned char *)malloc((unsigned int)siglen);
	if (s == NULL) {
		/* RSAerr(RSA_F_RSA_VERIFY,ERR_R_MALLOC_FAILURE); */
		goto err;
	}
	if (dtype == NID_md5_sha1) {
		if (m_len != SSL_SIG_LENGTH) {
			/* RSAerr(RSA_F_RSA_VERIFY,RSA_R_INVALID_MESSAGE_LENGTH); */
			goto err;
		}
	}

	i = RSA_public_decrypt((int)siglen, sigbuf, s, rsa, RSA_PKCS1_PADDING);

	if (i <= 0) {
		goto err;
	}

	/* Special case: SSL signature */
	if (dtype == NID_md5_sha1) {
		if ((i != SSL_SIG_LENGTH) || memcmp(s, m, SSL_SIG_LENGTH)) {
			/* RSAerr(RSA_F_RSA_VERIFY,RSA_R_BAD_SIGNATURE); */
		} else{
			ret = 1;
		}
	} else {
		const unsigned char *p = s;
		sig = d2i_X509_SIG(NULL, &p, (long)i);

		if (sig == NULL) {
			goto err;
		}

		/* Excess data can be used to create forgeries */
		if (p != s+i) {
			/* RSAerr(RSA_F_RSA_VERIFY,RSA_R_BAD_SIGNATURE); */
			goto err;
		}

		/* Parameters to the signature algorithm can also be used to
		 * create forgeries */
		if (sig->algor->parameter &&
		    (ASN1_TYPE_get(sig->algor->parameter) != V_ASN1_NULL)) {
			/* RSAerr(RSA_F_RSA_VERIFY,RSA_R_BAD_SIGNATURE); */
			goto err;
		}

		sigtype = OBJ_obj2nid(sig->algor->algorithm);


#ifdef RSA_DEBUG
		/* put a backward compatibility flag in EAY */
		fprintf(stderr, "in(%s) expect(%s)\n", OBJ_nid2ln(sigtype),
		    OBJ_nid2ln(dtype));
#endif
		if (sigtype != dtype) {
			if (((dtype == NID_md5) &&
			    (sigtype == NID_md5WithRSAEncryption)) ||
			    ((dtype == NID_md2) &&
			    (sigtype == NID_md2WithRSAEncryption))) {
				/* ok, we will let it through */
				fprintf(stderr, "signature has problems, re-make with post SSLeay045\n");
			} else {
				/* RSAerr(RSA_F_RSA_VERIFY,
				 *              RSA_R_ALGORITHM_MISMATCH); */
				goto err;
			}
		}
		if (((unsigned int)sig->digest->length != m_len) ||
		    (memcmp(m, sig->digest->data, m_len) != 0)) {
			/* RSAerr(RSA_F_RSA_VERIFY,RSA_R_BAD_SIGNATURE); */
		} else{
			ret = 1;
		}
	}
err:
	if (sig != NULL) {
		X509_SIG_free(sig);
	}
	if (s != NULL) {
		memset(s, 0, siglen);
		free(s);
	}
	return (ret);
}


#else

#error "Unknown ASN1 implementation"

#endif /* ASN1 implementation */

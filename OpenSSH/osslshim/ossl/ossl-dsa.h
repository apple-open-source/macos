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
#ifndef _OSSL_DSA_H_
#define _OSSL_DSA_H_			1

/* symbol renaming */
#define DSA_null_method			ossl_DSA_null_method
#define DSA_cdsa_method			ossl_DSA_cdsa_method
#define DSA_SIG_free			ossl_DSA_SIG_free
#define DSA_SIG_new			ossl_DSA_SIG_new
#define DSA_do_sign			ossl_DSA_do_sign
#define DSA_do_verify			ossl_DSA_do_verify
#define DSA_free			ossl_DSA_free
#define DSA_generate_key		ossl_DSA_generate_key
#define DSA_generate_parameters_ex	ossl_DSA_generate_parameters_ex
#define DSA_new				ossl_DSA_new
#define DSA_new_method			ossl_DSA_new_new_method
#define DSA_up_ref			ossl_DSA_up_ref
#define DSA_get_default_method		ossl_DSA_get_default_method
#define DSA_set_method			ossl_DSA_set_method
#define DSA_sign_setup			ossl_DSA_sign_setup
#define DSA_sign			ossl_DSA_sign
#define DSA_verify			ossl_DSA_verify
#define DSA_up_ref			ossl_DSA_up_ref

#define d2i_DSAPrivateKey		ossl_d2i_DSAPrivateKey
#define i2d_DSAPrivateKey		ossl_i2d_DSAPrivateKey

#define DSA_PKCS1_PADDING		1
#define DSA_PKCS1_PADDING_SIZE		11

#define DSA_FLAG_NO_EXP_CONSTTIME	0x02

/*
 *
 */
typedef struct DSA_SIG_st	DSA_SIG;
typedef struct dsa_method	DSA_METHOD;
typedef struct dsa_st		DSA;

#include "ossl-bn.h"
#include "ossl-engine.h"

struct DSA_SIG_st {
	BIGNUM *r;
	BIGNUM *s;
};

struct dsa_method {
	const char *	name;
	DSA_SIG *	(*dsa_do_sign)(const unsigned char *dgst, int dlen, DSA *dsa);
	int		(*dsa_sign_setup)(DSA *dsa, BN_CTX *ctx_in, BIGNUM **kinvp,
			    BIGNUM **rp);
	int		(*dsa_do_verify)(const unsigned char *dgst, int dgst_len,
			    DSA_SIG *sig, DSA *dsa);
	int		(*dsa_mod_exp)(DSA *dsa, BIGNUM *rr, BIGNUM *a1, BIGNUM *p1,
			    BIGNUM *a2, BIGNUM *p2, BIGNUM *m, BN_CTX *ctx,
			    BN_MONT_CTX *in_mont);
	int		(*bn_mod_exp)(DSA *dsa, BIGNUM *r, BIGNUM *a, const BIGNUM *p,
			    const BIGNUM *m, BN_CTX *ctx, BN_MONT_CTX *m_ctx);
	int		(*init)(DSA *dsa);
	int		(*finish)(DSA *dsa);
	int		flags;
	char *		app_data;
	int		(*dsa_paramgen)(DSA *dsa, int bits, unsigned char *seed, int seed_len,
			    int *counter_ret, unsigned long *h_ret, BN_GENCB *cb);
	int		(*dsa_keygen)(DSA *dsa);
};

struct dsa_st {
	int			pad;
	long			version;
	int			write_params;
	BIGNUM *		p;
	BIGNUM *		q;
	BIGNUM *		g;

	BIGNUM *		pub_key;
	BIGNUM *		priv_key;

	BIGNUM *		kinv;   /* Signing pre-calc */
	BIGNUM *		r;      /* Signing pre-calc */

	int			flags;
	BN_MONT_CTX *		method_mont_p;
	int			references;
	struct dsa_CRYPTO_EX_DATA {
		void *	sk;
		int	dummy;
	}
	ex_data;
	const DSA_METHOD *	meth;
	ENGINE *		engine;
};


/*
 *
 */

const DSA_METHOD *DSA_null_method(void);
const DSA_METHOD *DSA_eay_method(void);

/*
 *
 */

DSA *DSA_new(void);
DSA *DSA_new_method(ENGINE *);
void DSA_free(DSA *);
int DSA_up_ref(DSA *);

DSA_SIG *DSA_SIG_new(void);
void DSA_SIG_free(DSA_SIG *a);

DSA_SIG *DSA_do_sign(const unsigned char *dgst, unsigned int dlen, DSA *dsa);
int DSA_do_verify(const unsigned char *dgst, int dgst_len,
    DSA_SIG *sig, DSA *dsa);
int DSA_sign_setup(DSA *dsa, BN_CTX *ctx_in, BIGNUM **kinvp, BIGNUM **rp);
int DSA_sign(int type, const unsigned char *dgst, int dlen,
    unsigned char *sig, unsigned int *siglen, DSA *dsa);
int DSA_verify(int type, const unsigned char *dgst, int dgst_len,
    const unsigned char *sigbuf, int siglen, DSA *dsa);

int DSA_generate_parameters_ex(DSA *dsa, int bits,
    unsigned char *seed, int seed_len,
    int *counter_ret, unsigned long *h_ret, BN_GENCB *cb);
int DSA_generate_key(DSA *a);

const DSA_METHOD *DSA_get_default_method(void);
void DSA_set_default_method(const DSA_METHOD *meth);
int DSA_set_method(DSA *dsa, const DSA_METHOD *method);

DSA *d2i_DSAPrivateKey(DSA **, const unsigned char **, long len);
int i2d_DSAPrivateKey(const DSA *, unsigned char **);

int i2d_DSAPublicKey(const DSA *, unsigned char **);
DSA *d2i_DSAPublicKey(DSA **, const unsigned char **, long);

#endif /* _OSSL_DSA_H_ */

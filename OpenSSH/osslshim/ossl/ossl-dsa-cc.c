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
#include <assert.h>

#include "krb5-types.h"
#include "rfc2459_asn1.h"

#include "ossl-dsa.h"
#include "ossl-common.h"

#ifdef HAVE_COMMONCRYPTO_COMMONDSACRYPTOR_H

/*
 * CommonCrypto DSA/DSS shim
 */

#if 0

static int
load_key(CSSM_CSP_HANDLE cspHandle, DSA *dsa, int use_public, CSSM_KEY_PTR key, size_t *keysize)
{
	CSSM_KEY_SIZE keySize;
	CSSM_RETURN ret;
	size_t size;

	memset(key, 0, sizeof(*key));

	if (use_public) {
		DSAPublicKey k;

		memset(&k, 0, sizeof(k));

		ret = _cs_BN_to_integer(rsa->n, &k.modulus);
		if (ret == 0) {
			ret = _cs_BN_to_integer(rsa->e, &k.publicExponent);
		}
		if (ret) {
			free_RSAPublicKey(&k);
			return (0);
		}

		ASN1_MALLOC_ENCODE(RSAPublicKey, key->KeyData.Data, key->KeyData.Length,
		    &k, &size, ret);
		free_RSAPublicKey(&k);
		if (ret) {
			return (1);
		}
		if (size != key->KeyData.Length) {
			abort();
		}
	} else {
		RSAPrivateKey k;

		memset(&k, 0, sizeof(k));

		k.version = 1;
		ret = _cs_BN_to_integer(rsa->n, &k.modulus);
		if (ret == 0) {
			ret = _cs_BN_to_integer(rsa->e, &k.publicExponent);
		}
		if (ret == 0) {
			ret = _cs_BN_to_integer(rsa->d, &k.privateExponent);
		}
		if (ret == 0) {
			ret = _cs_BN_to_integer(rsa->p, &k.prime1);
		}
		if (ret == 0) {
			ret = _cs_BN_to_integer(rsa->q, &k.prime2);
		}
		if (ret == 0) {
			ret = _cs_BN_to_integer(rsa->dmp1, &k.exponent1);
		}
		if (ret == 0) {
			ret = _cs_BN_to_integer(rsa->dmq1, &k.exponent2);
		}
		if (ret == 0) {
			ret = _cs_BN_to_integer(rsa->iqmp, &k.coefficient);
		}
		if (ret) {
			free_RSAPrivateKey(&k);
			return (1);
		}

		ASN1_MALLOC_ENCODE(RSAPrivateKey, key->KeyData.Data, key->KeyData.Length,
		    &k, &size, ret);
		free_RSAPrivateKey(&k);
		if (ret) {
			return (1);
		}
		if (size != key->KeyData.Length) {
			abort();
		}
	}

	key->KeyHeader.HeaderVersion = CSSM_KEYHEADER_VERSION;
	key->KeyHeader.BlobType = CSSM_KEYBLOB_RAW;
	key->KeyHeader.Format = CSSM_KEYBLOB_RAW_FORMAT_PKCS1;
	key->KeyHeader.AlgorithmId = CSSM_ALGID_RSA;
	key->KeyHeader.KeyClass = use_public ?
	    CSSM_KEYCLASS_PUBLIC_KEY :
	    CSSM_KEYCLASS_PRIVATE_KEY;
	key->KeyHeader.KeyAttr = CSSM_KEYATTR_EXTRACTABLE;
	key->KeyHeader.KeyUsage = CSSM_KEYUSE_ANY;

	ret = CSSM_QueryKeySizeInBits(cspHandle, 0, key, &keySize);
	if (ret) {
		return (1);
	}

	key->KeyHeader.LogicalKeySizeInBits = keySize.LogicalKeySizeInBits;

	*keysize = (keySize.LogicalKeySizeInBits + 7) / 8;

	return (0);
}


static void
unload_key(CSSM_KEY_PTR key)
{
	free(key->KeyData.Data);
	memset(key, 0, sizeof(*key));
}


typedef CSSM_RETURN (*op)(CSSM_CC_HANDLE, const CSSM_DATA *,
    uint32, CSSM_DATA_PTR, uint32,
    CSSM_SIZE *, CSSM_DATA_PTR);


static int
perform_rsa_op(int flen, const unsigned char *from,
    unsigned char *to, RSA *rsa, int padding,
    CSSM_ENCRYPT_MODE algMode, op func)
{
	CSSM_CSP_HANDLE cspHandle = _cs_get_cdsa_csphandle();
	CSSM_RETURN cret;
	CSSM_ACCESS_CREDENTIALS creds;
	CSSM_KEY cssmKey;
	CSSM_CC_HANDLE handle = 0;
	CSSM_DATA out, in, rem;
	int fret = 0;
	CSSM_SIZE outlen = 0;
	char remdata[1024];
	size_t keysize;

	if (padding != RSA_PKCS1_PADDING) {
		return (-1);
	}

	memset(&creds, 0, sizeof(creds));

	fret = load_key(cspHandle, rsa, (algMode == CSSM_ALGMODE_PUBLIC_KEY),
		&cssmKey, &keysize);
	if (fret) {
		return (-2);
	}

	fret = CSSM_CSP_CreateAsymmetricContext(cspHandle,
		CSSM_ALGID_RSA,
		&creds,
		&cssmKey,
		CSSM_PADDING_PKCS1,
		&handle);
	if (fret) {
		abort();
	}

	{
		CSSM_CONTEXT_ATTRIBUTE attr;

		attr.AttributeType = CSSM_ATTRIBUTE_MODE;
		attr.AttributeLength = sizeof(attr.Attribute.Uint32);
		attr.Attribute.Uint32 = algMode;

		fret = CSSM_UpdateContextAttributes(handle, 1, &attr);
		if (fret) {
			abort();
		}
	}

	in.Data = (uint8 *)from;
	in.Length = flen;

	out.Data = (uint8 *)to;
	out.Length = keysize;

	rem.Data = (uint8 *)remdata;
	rem.Length = sizeof(remdata);

	cret = func(handle, &in, 1, &out, 1, &outlen, &rem);
	if (cret) {
		/* cssmErrorString(cret); */
		fret = -1;
	} else{
		fret = outlen;
	}

	if (handle) {
		CSSM_DeleteContext(handle);
	}
	unload_key(&cssmKey);

	return (fret);
}


#endif /* #if 0 */

/*
 *
 */

#ifdef PR_10488503_FIXED

/* CommonCrypto doesn't implement DSA/DSS. See <rdar://problem/10488503> */

#else

#include "tommath.h"

/* int dsa_make_key(CCRNGRef rng, int group_size, int modulus_size, dsa_key *key) */
static int
cc_dsa_generate_key(DSA *rsa, int bits, BIGNUM *e, BN_GENCB *cb)
{
	void *tmp, *tmp2;
	int err, res;
	unsigned char *buf;

	LTC_ARGCHK(key != NULL);
	LTC_ARGCHK(ltc_mp.name != NULL);
	LTC_ARGCHK(rng != NULL);


	/* check size */
	if ((group_size >= LTC_MDSA_MAX_GROUP) || (group_size <= 15) ||
	    (group_size >= modulus_size) || ((modulus_size - group_size) >= LTC_MDSA_DELTA)) {
		return (CRYPT_INVALID_ARG);
	}

	/* allocate ram */
	buf = CC_XMALLOC(LTC_MDSA_DELTA);
	if (buf == NULL) {
		return (CRYPT_MEM);
	}

	/* init mp_ints  */
	if ((err = mp_init_multi(&tmp, &tmp2, &key->g, &key->q, &key->p, &key->x, &key->y, NULL)) != CRYPT_OK) {
		CC_XFREE(buf, LTC_MDSA_DELTA);
		return (err);
	}

	/* make our prime q */
	if ((err = rand_prime(key->q, group_size, rng)) != CRYPT_OK) {
		goto error;
	}

	/* double q  */
	if ((err = mp_add(key->q, key->q, tmp)) != CRYPT_OK) {
		goto error;
	}

	/* now make a random string and multply it against q */
	if (CCRNGGetBytes(rng, buf+1, modulus_size - group_size)) {
		err = CRYPT_ERROR_READPRNG;
		goto error;
	}

	/* force magnitude */
	buf[0] |= 0xC0;

	/* force even */
	buf[modulus_size - group_size - 1] &= ~1;

	if ((err = mp_read_unsigned_bin(tmp2, buf, modulus_size - group_size)) != CRYPT_OK) {
		goto error;
	}
	if ((err = mp_mul(key->q, tmp2, key->p)) != CRYPT_OK) {
		goto error;
	}
	if ((err = mp_add_d(key->p, 1, key->p)) != CRYPT_OK) {
		goto error;
	}

	/* now loop until p is prime */
	for ( ; ; ) {
		if ((err = mp_prime_is_prime(key->p, 8, &res)) != CRYPT_OK) {
			goto error;
		}
		if (res == LTC_MP_YES) {
			break;
		}

		/* add 2q to p and 2 to tmp2 */
		if ((err = mp_add(tmp, key->p, key->p)) != CRYPT_OK) {
			goto error;
		}
		if ((err = mp_add_d(tmp2, 2, tmp2)) != CRYPT_OK) {
			goto error;
		}
	}

	/* now p = (q * tmp2) + 1 is prime, find a value g for which g^tmp2 != 1 */
	mp_set(key->g, 1);

	do {
		if ((err = mp_add_d(key->g, 1, key->g)) != CRYPT_OK) {
			goto error;
		}
		if ((err = mp_exptmod(key->g, tmp2, key->p, tmp)) != CRYPT_OK) {
			goto error;
		}
	} while (mp_cmp_d(tmp, 1) == LTC_MP_EQ);

	/* at this point tmp generates a group of order q mod p */
	mp_exch(tmp, key->g);

	/* so now we have our DH structure, generator g, order q, modulus p
	 * Now we need a random exponent [mod q] and it's power g^x mod p
	 */
	do {
		if (CCRNGGetBytes(rng, buf, group_size)) {
			err = CRYPT_ERROR_READPRNG;
			goto error;
		}
		if ((err = mp_read_unsigned_bin(key->x, buf, group_size)) != CRYPT_OK) {
			goto error;
		}
	} while (mp_cmp_d(key->x, 1) != LTC_MP_GT);
	if ((err = mp_exptmod(key->g, key->x, key->p, key->y)) != CRYPT_OK) {
		goto error;
	}

	key->type = PK_PRIVATE;
	key->qord = group_size;

#ifdef LTC_CLEAN_STACK
	zeromem(buf, LTC_MDSA_DELTA);
#endif

	err = CRYPT_OK;
	goto done;
error:
	mp_clear_multi(key->g, key->q, key->p, key->x, key->y, NULL);
done:
	mp_clear_multi(tmp, tmp2, NULL);
	CC_XFREE(buf, LTC_MDSA_DELTA);
	return (err);
}


/**
 * Sign a hash with DSA
 * @param in       The hash to sign
 * @param inlen    The length of the hash to sign
 * @param r        The "r" integer of the signature (caller must initialize with mp_init() first)
 * @param s        The "s" integer of the signature (caller must initialize with mp_init() first)
 * @param prng     An active PRNG state
 * @param wprng    The index of the PRNG desired
 * @param key      A private DSA key
 * @return CRYPT_OK if successful
 */

/* int dsa_sign_hash_raw(const unsigned char *in,  unsigned long inlen,
 *                                 void   *r,   void *s,
 *                             CCRNGRef rng, dsa_key *key) */
static DSA_SIG *
cc_dsa_do_sign(const unsigned char *dgst, int dlen, DSA *dsa)
{
	void *k, *kinv, *tmp;
	unsigned char *buf;
	int err;

	LTC_ARGCHK(in != NULL);
	LTC_ARGCHK(r != NULL);
	LTC_ARGCHK(s != NULL);
	LTC_ARGCHK(key != NULL);
	LTC_ARGCHK(rng != NULL);

	if (key->type != PK_PRIVATE) {
		return (CRYPT_PK_NOT_PRIVATE);
	}

	/* check group order size  */
	if (key->qord >= LTC_MDSA_MAX_GROUP) {
		return (CRYPT_INVALID_ARG);
	}

	buf = CC_XMALLOC(LTC_MDSA_MAX_GROUP);
	if (buf == NULL) {
		return (CRYPT_MEM);
	}

	/* Init our temps */
	if ((err = mp_init_multi(&k, &kinv, &tmp, NULL)) != CRYPT_OK) {
		goto ERRBUF;
	}

retry:

	do {
		/* gen random k */

		if (CCRNGGetBytes(rng, buf, key->qord)) {
			err = CRYPT_ERROR_READPRNG;
			goto error;
		}

		/* read k */
		if ((err = mp_read_unsigned_bin(k, buf, key->qord)) != CRYPT_OK) {
			goto error;
		}

		/* k > 1 ? */
		if (mp_cmp_d(k, 1) != LTC_MP_GT) {
			goto retry;
		}

		/* test gcd */
		if ((err = mp_gcd(k, key->q, tmp)) != CRYPT_OK) {
			goto error;
		}
	} while (mp_cmp_d(tmp, 1) != LTC_MP_EQ);

	/* now find 1/k mod q */
	if ((err = mp_invmod(k, key->q, kinv)) != CRYPT_OK) {
		goto error;
	}

	/* now find r = g^k mod p mod q */
	if ((err = mp_exptmod(key->g, k, key->p, r)) != CRYPT_OK) {
		goto error;
	}
	if ((err = mp_mod(r, key->q, r)) != CRYPT_OK) {
		goto error;
	}

	if (mp_iszero(r) == LTC_MP_YES) {
		goto retry;
	}

	/* now find s = (in + xr)/k mod q */
	if ((err = mp_read_unsigned_bin(tmp, (unsigned char *)in, inlen)) != CRYPT_OK) {
		goto error;
	}
	if ((err = mp_mul(key->x, r, s)) != CRYPT_OK) {
		goto error;
	}
	if ((err = mp_add(s, tmp, s)) != CRYPT_OK) {
		goto error;
	}
	if ((err = mp_mulmod(s, kinv, key->q, s)) != CRYPT_OK) {
		goto error;
	}

	if (mp_iszero(s) == LTC_MP_YES) {
		goto retry;
	}

	err = CRYPT_OK;
error:
	mp_clear_multi(k, kinv, tmp, NULL);
ERRBUF:
#ifdef LTC_CLEAN_STACK
	zeromem(buf, LTC_MDSA_MAX_GROUP);
#endif
	CC_XFREE(buf, X);
	return (err);
}


/**
 * Sign a hash with DSA
 * @param in       The hash to sign
 * @param inlen    The length of the hash to sign
 * @param out      [out] Where to store the signature
 * @param outlen   [in/out] The max size and resulting size of the signature
 * @param prng     An active PRNG state
 * @param wprng    The index of the PRNG desired
 * @param key      A private DSA key
 * @return CRYPT_OK if successful
 */

/*
 * int dsa_sign_hash(const unsigned char *in,  unsigned long inlen,
 *                      unsigned char *out, unsigned long *outlen,
 *                      CCRNGRef rng, dsa_key *key)
 */
static int
cc_dsa_sign_setup(DSA *dsa, BN_CTX *ctx_in, BIGNUM **kinvp, BIGNUM **rp)
{
	void *r, *s;
	int err;

	LTC_ARGCHK(in != NULL);
	LTC_ARGCHK(out != NULL);
	LTC_ARGCHK(outlen != NULL);
	LTC_ARGCHK(key != NULL);

	if (mp_init_multi(&r, &s, NULL) != CRYPT_OK) {
		return (CRYPT_MEM);
	}

	if ((err = dsa_sign_hash_raw(in, inlen, r, s, rng, key)) != CRYPT_OK) {
		goto error;
	}

	err = der_encode_sequence_multi(out, outlen,
		LTC_ASN1_INTEGER, 1UL, r,
		LTC_ASN1_INTEGER, 1UL, s,
		LTC_ASN1_EOL, 0UL, NULL);

error:
	mp_clear_multi(r, s, NULL);
	return (err);
}


/**
 * Verify a DSA signature
 * @param r        DSA "r" parameter
 * @param s        DSA "s" parameter
 * @param hash     The hash that was signed
 * @param hashlen  The length of the hash that was signed
 * @param stat     [out] The result of the signature verification, 1==valid, 0==invalid
 * @param key      The corresponding public DH key
 * @return CRYPT_OK if successful (even if the signature is invalid)
 */

/*
 * int dsa_verify_hash_raw(         void   *r,          void   *s,
 *                  const unsigned char *hash, unsigned long hashlen,
 *                                  int *stat,      dsa_key *key)
 */
static int
cc_dsa_do_verify(const unsigned char *dgst, int dgst_len,
    DSA_SIG *sig, DSA *dsa)
{
	void *w, *v, *u1, *u2;
	int err;

	LTC_ARGCHK(r != NULL);
	LTC_ARGCHK(s != NULL);
	LTC_ARGCHK(stat != NULL);
	LTC_ARGCHK(key != NULL);

	/* default to invalid signature */
	*stat = 0;

	/* init our variables */
	if ((err = mp_init_multi(&w, &v, &u1, &u2, NULL)) != CRYPT_OK) {
		return (err);
	}

	/* neither r or s can be null or >q*/
	if ((mp_iszero(r) == LTC_MP_YES) || (mp_iszero(s) == LTC_MP_YES) || (mp_cmp(r, key->q) != LTC_MP_LT) || (mp_cmp(s, key->q) != LTC_MP_LT)) {
		err = CRYPT_INVALID_PACKET;
		goto error;
	}

	/* w = 1/s mod q */
	if ((err = mp_invmod(s, key->q, w)) != CRYPT_OK) {
		goto error;
	}

	/* u1 = m * w mod q */
	if ((err = mp_read_unsigned_bin(u1, (unsigned char *)hash, hashlen)) != CRYPT_OK) {
		goto error;
	}
	if ((err = mp_mulmod(u1, w, key->q, u1)) != CRYPT_OK) {
		goto error;
	}

	/* u2 = r*w mod q */
	if ((err = mp_mulmod(r, w, key->q, u2)) != CRYPT_OK) {
		goto error;
	}

	/* v = g^u1 * y^u2 mod p mod q */
	if ((err = mp_exptmod(key->g, u1, key->p, u1)) != CRYPT_OK) {
		goto error;
	}
	if ((err = mp_exptmod(key->y, u2, key->p, u2)) != CRYPT_OK) {
		goto error;
	}
	if ((err = mp_mulmod(u1, u2, key->p, v)) != CRYPT_OK) {
		goto error;
	}
	if ((err = mp_mod(v, key->q, v)) != CRYPT_OK) {
		goto error;
	}

	/* if r = v then we're set */
	if (mp_cmp(r, v) == LTC_MP_EQ) {
		*stat = 1;
	}

	err = CRYPT_OK;
error:
	mp_clear_multi(w, v, u1, u2, NULL);
	return (err);
}


/**
 * Verify a DSA signature
 * @param sig      The signature
 * @param siglen   The length of the signature (octets)
 * @param hash     The hash that was signed
 * @param hashlen  The length of the hash that was signed
 * @param stat     [out] The result of the signature verification, 1==valid, 0==invalid
 * @param key      The corresponding public DH key
 * @return CRYPT_OK if successful (even if the signature is invalid)
 */

/*
 * int dsa_verify_hash(const unsigned char *sig, unsigned long siglen,
 *                  const unsigned char *hash, unsigned long hashlen,
 *                  int *stat, dsa_key *key)
 */
static int
cc_dsa_verify(int type, const unsigned char *dgst, int len,
    unsigned char *sigbuf, int siglen, DSA *dsa)
{
	int err;
	void *r, *s;

	if ((err = mp_init_multi(&r, &s, NULL)) != CRYPT_OK) {
		return (CRYPT_MEM);
	}

	/* decode the sequence */
	if ((err = der_decode_sequence_multi(sig, siglen,
	    LTC_ASN1_INTEGER, 1UL, r,
	    LTC_ASN1_INTEGER, 1UL, s,
	    LTC_ASN1_EOL, 0UL, NULL)) != CRYPT_OK) {
		goto LBL_ERR;
	}

	/* do the op */
	err = dsa_verify_hash_raw(r, s, hash, hashlen, stat, key);

LBL_ERR:
	mp_clear_multi(r, s, NULL);
	return (err);
}


#endif /* PR_XXX_FIXED */

static int
cc_dsa_init(DSA *dsa)
{
	return (1);
}


static int
cc_dsa_finish(DSA *dsa)
{
	return (1);
}


static int
cc_dsa_paramgen(DSA *dsa, int bits, unsigned char *seed, int seed_len,
    int *counter_ret, unsigned long *h_ret, BN_GENCB *cb)
{
	return (1);
}


static int
cc_dsa_keygen(DSA *dsa)
{
	return (1);
}


const DSA_METHOD _cs_dsa_cc_method =
{
	.name		= "CommonCrypto(LTC) DSA",
	.dsa_do_sign	= cc_dsa_do_sign,
	.dsa_sign_setup = cc_dsa_sign_setup,
	.dsa_do_verify	= cc_dsa_do_verify,
	.init		= cdsa_dsa_init,
	.finish		= cdsa_dsa_finish,
	0,
	NULL,
	.dsa_paramgen	= cc_dsa_paramgen,
	.dsa_keygen	= cc_dsa_keygen
};
#endif /* HAVE_COMMONCRYPTO_COMMONDSACRYPTOR_H */

const DSA_METHOD *
DSA_cc_method(void)
{
#ifdef HAVE_COMMONCRYPTO_COMMONDSACRYPTOR_H
	return (&_cs_dsa_cc_method);

#else
	return (NULL);
#endif  /* HAVE_COMMONCRYPTO_COMMONDSACRYPTOR_H */
}

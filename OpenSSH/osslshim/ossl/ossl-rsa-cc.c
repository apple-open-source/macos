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


#include "ossl-config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "ossl-bn.h"
#include "ossl-rand.h"
#include "ossl-rsa.h"

#ifdef HAVE_COMMONCRYPTO_COMMONRSACRYPTOR_H

#include <CommonCrypto/CommonRSACryptor.h>

static int cc_rsa_public_encrypt(int, const unsigned char *, unsigned char *, RSA *, int);
static int cc_rsa_private_decrypt(int, const unsigned char *, unsigned char *, RSA *, int);

static int cc_rsa_private_encrypt(int flen, const unsigned char *from,
				    unsigned char *to, RSA *rsa, int padding);
static int cc_rsa_public_decrypt(int flen, const unsigned char *from,
				    unsigned char *to, RSA *rsa, int padding);

static int cc_rsa_mod_exp(BIGNUM *r0, const BIGNUM *i, RSA *rsa, BN_CTX *ctx);

static int cc_rsa_init(RSA *rsa);
static int cc_rsa_finish(RSA *rsa);

static int cc_rsa_sign(int, const unsigned char *, unsigned int, unsigned char *,
			    unsigned int *, const RSA *);
static int cc_rsa_verify(int, const unsigned char *, unsigned int, unsigned char *,
			    unsigned int, const RSA *);

static int cc_rsa_generate_key(RSA *rsa, int bits, BIGNUM *e_value, BN_GENCB *cb);

static int
cc_rsa_init(RSA *rsa __unused)
{
	/* XXX set any flags needed */
	return (1);
}


static int
cc_rsa_finish(RSA *rsa __unused)
{
	/* XXX free resources */
	return (1);
}


static int
cc_status_to_openssl_err(CCCryptorStatus status)
{
	switch (status) {
	case kCCSuccess:
		return (0);

	case -1:
		/* XXX RSAerr( , UNKNOWN_NEG1); */
		fprintf(stderr, "unknown (-1) error");
		return (-1);

	case kCCParamError:
		/* XXX RSAerr( , PARAM_ERROR ); */
		fprintf(stderr, "parameter error");
		return (1);

	case kCCBufferTooSmall:
		/* XXX RSAerr( , BUFFER_OVERFLOW ); */
		fprintf(stderr, "buffer overflow error");
		return (2);

	case kCCMemoryFailure:
		/* XXX RSAerr(  , ERR_R_MALLOC_FAILURE); */
		fprintf(stderr, "memory failure");
		return (3);

	case kCCDecodeError:
		/* XXX RSAerr( , PK_INVALID_SIZE); */
		fprintf(stderr, "der_encode_int_error");
		return (4);

	default:
		/* XXX RSAerr( , UNKNOWN); */
		fprintf(stderr, "unknown error");
		return (666);
	}
}


/* DER encoding for importing keys to CommonCrypto.  */
static unsigned long
der_length_seq_hdr(unsigned long blksz)
{
	unsigned long len = 0;

	if (blksz < 128UL) {
		/* 0x30 LL */
		len = 2UL;
	} else if (blksz < 256UL) {
		/* 0x30 0x81 LL */
		len = 3UL;
	} else if (blksz < 65536UL) {
		/* 0x30 0x81 LL LL */
		len = 4UL;
	} else if (blksz < 16777216UL) {
		/*  0x30 0x83 LL LL LL */
		len = 5UL;
	} else {
		fprintf(stderr, "Invalid DER Sequence size.");
		abort();
	}

	return (len);
}


static int
der_encode_seq_hdr(unsigned long blksz, unsigned char *out, unsigned long *outlen)
{
	*out++ = 0x30;
	if (blksz < 128UL) {
		/* 0x30 LL */
		*out++ = (unsigned char)(blksz & 0xFF);
		*outlen = 2UL;
	} else if (blksz < 256) {
		/* 0x30 0x81 LL */
		*out++ = 0x81;
		*out++ = (unsigned char)(blksz & 0xFF);
		*outlen = 3UL;
	} else if (blksz < 65536UL) {
		/* 0x30 0x81 LL LL */
		*out++ = 0x82;
		*out++ = (unsigned char)((blksz >> 8UL) & 0xFF);
		*out++ = (unsigned char)(blksz & 0xFF);
		*outlen = 4UL;
	} else if (blksz < 16777216UL) {
		/*  0x30 0x83 LL LL LL */
		*out++ = 0x83;
		*out++ = (unsigned char)((blksz >> 16UL) & 0xFF);
		*out++ = (unsigned char)((blksz >> 8UL) & 0xFF);
		*out++ = (unsigned char)(blksz & 0xFF);
		*outlen = 5UL;
	} else {
		*outlen = 0UL;
		return (-1);
	}

	return (0);
}


static unsigned long
der_length_int(const BIGNUM *num)
{
	unsigned long len, z;
	int leading_zero;

	if (((BN_num_bits(num) & 0x7) == 0) || BN_is_zero(num)) {
		leading_zero = 1;
	} else{
		leading_zero = 0;
	}

	z = len = leading_zero + BN_num_bytes(num);

	if (z < 128) {
		/* short form */
		++len;
	} else {
		/* long form (z != 0), len bytes < 128 */
		++len;
		while (z) {
			++len;
			z >>= 8;
		}
	}
	/* add the 0x02 header byte */
	++len;

	return (len);
}


static int
der_encode_int(const BIGNUM *num, unsigned char *out, unsigned long *outlen)
{
	unsigned long tmplen, y;
	int err, leading_zero;

	if ((tmplen = der_length_int(num)) > *outlen) {
		return (-1);
	}

	if (BN_is_negative(num)) {
		return (-2);
	}

	/* We only need a leading zero if the msb of the first byte is one */
	if (((BN_num_bits(num) & 0x7) == 0) || BN_is_zero(num)) {
		leading_zero = 1;
	} else{
		leading_zero = 0;
	}

	y = BN_num_bytes(num) + leading_zero;

	/* header info */
	*out++ = 0x02;
	if (y < 128) {
		/* short form */
		*out++ = (unsigned char)y;
	} else if (y < 256) {
		*out++ = 0x81;
		*out++ = (unsigned char)y;
	} else if (y < 65536UL) {
		*out++ = 0x82;
		*out++ = (unsigned char)((y >> 8) & 0xFF);
		*out++ = (unsigned char)y;
	} else if (y < 16777216UL) {
		*out++ = 0x83;
		*out++ = (unsigned char)((y >> 16) & 0xFF);
		*out++ = (unsigned char)((y >> 8) & 0xFF);
		*out++ = (unsigned char)y;
	} else {
		return (-3);
	}

	/* now store msbyte of zero if num is non-zero */
	if (leading_zero) {
		*out++ = 0x00;
	}

	(void)BN_bn2bin(num, out);

	*outlen = tmplen;

	return (0);
}


/* keytype = { ccRSAKeyPublic or ccRSAKeyPrivate } */
static CCRSACryptorRef
get_CCRSACryptorRef(CCRSAKeyType keytype, const RSA *rsakey)
{
	CCCryptorStatus status = kCCDecodeError;
	CCRSACryptorRef ref = NULL;
	unsigned long length, offset, outlen, blksz;
	unsigned char *derbuf = NULL;
	BIGNUM *zero = BN_new();

	BN_clear(zero);

	switch (keytype) {
	case ccRSAKeyPublic:
		blksz = der_length_int(rsakey->n) + der_length_int(rsakey->e);
		length = der_length_seq_hdr(blksz) + blksz;
		derbuf = malloc(length);
		offset = 0L;
		outlen = length;

		/* Add sequence header first */
		if (der_encode_seq_hdr(blksz, derbuf + offset, &outlen) != 0) {
			goto outerr;
		}
		offset += outlen;
		outlen = length - offset;
		/* And then integers */
		if (der_encode_int(rsakey->n, derbuf + offset, &outlen) != 0) {
			goto outerr;
		}
		offset += outlen;
		outlen = length - offset;
		if (der_encode_int(rsakey->e, derbuf + offset, &outlen) != 0) {
			goto outerr;
		}

		break;

	case ccRSAKeyPrivate:
		blksz = der_length_int(zero) + der_length_int(rsakey->n) +
		    der_length_int(rsakey->e) + der_length_int(rsakey->d) +
		    der_length_int(rsakey->p) + der_length_int(rsakey->q) +
		    der_length_int(rsakey->dmp1) + der_length_int(rsakey->dmq1) +
		    der_length_int(rsakey->iqmp);
		length = der_length_seq_hdr(blksz) + blksz;
		derbuf = malloc(length);
		offset = 0L;
		outlen = length;

		/* Add sequence header first */
		if (der_encode_seq_hdr(blksz, derbuf + offset, &outlen) != 0) {
			goto outerr;
		}
		offset += outlen;
		outlen = length - offset;
		/* And then integers */
		if (der_encode_int(zero, derbuf + offset, &outlen) != 0) {
			goto outerr;
		}
		offset += outlen;
		outlen = length - offset;
		if (der_encode_int(rsakey->n, derbuf + offset, &outlen) != 0) {
			goto outerr;
		}
		offset += outlen;
		outlen = length - offset;
		if (der_encode_int(rsakey->e, derbuf + offset, &outlen) != 0) {
			goto outerr;
		}
		offset += outlen;
		outlen = length - offset;
		if (der_encode_int(rsakey->d, derbuf + offset, &outlen) != 0) {
			goto outerr;
		}
		offset += outlen;
		outlen = length - offset;
		if (der_encode_int(rsakey->p, derbuf + offset, &outlen) != 0) {
			goto outerr;
		}
		offset += outlen;
		outlen = length - offset;
		if (der_encode_int(rsakey->q, derbuf + offset, &outlen) != 0) {
			goto outerr;
		}
		offset += outlen;
		outlen = length - offset;
		if (der_encode_int(rsakey->dmp1, derbuf + offset, &outlen) != 0) {
			goto outerr;
		}
		offset += outlen;
		outlen = length - offset;
		if (der_encode_int(rsakey->dmq1, derbuf + offset, &outlen) != 0) {
			goto outerr;
		}
		offset += outlen;
		outlen = length - offset;
		if (der_encode_int(rsakey->iqmp, derbuf + offset, &outlen) != 0) {
			goto outerr;
		}

		break;

	default:
		goto outerr;
	}

	status = CCRSACryptorImport(derbuf, length, &ref);

outerr:

	BN_free(zero);
	if (derbuf != NULL) {
		free(derbuf);
	}

	if (cc_status_to_openssl_err(status) == 0) {
		return (ref);
	} else{
		return (NULL);
	}
}


/* RSASSA-PKCS1-v1_5 (PKCS #1 v2.0 signature) with SHA1 (default) or MD5 */
static int
cc_rsa_sign(int nid, const unsigned char *digest, unsigned int dlen,
    unsigned char *sig, unsigned int *len, const RSA *rsa)
{
	CCRSACryptorRef cc_key = get_CCRSACryptorRef(ccRSAKeyPrivate, rsa);
	CCCryptorStatus status;

	if (NULL == cc_key) {
		/* XXX RSAerr( , ); */
		return (0);
	}

	status = CCRSACryptorSign(cc_key, ccPKCS1Padding, (const void *)digest,
		(size_t)dlen, (nid == NID_md5) ? kCCDigestMD5 : kCCDigestSHA1,
		0, (void *)sig, (size_t *)len);

	CCRSACryptorRelease(cc_key);

	return (cc_status_to_openssl_err(status) == 0);
}


/* RSASSA-PKCS1-v1_5 (PKCS #1 v2.0 signature verify) with SHA1 (default) or MD5 */
static int
cc_rsa_verify(int nid, const unsigned char *digest, unsigned int dlen,
    unsigned char *sigblob, unsigned int len, const RSA *rsa)
{
	CCRSACryptorRef cc_key = get_CCRSACryptorRef(ccRSAKeyPublic, rsa);
	CCCryptorStatus status;

	if (NULL == cc_key) {
		/* XXX RSAerr( , ); */
		return (0);
	}

	status = CCRSACryptorVerify(cc_key, ccPKCS1Padding, (const void *)digest, (size_t)dlen,
		(nid == NID_md5) ? kCCDigestMD5 : kCCDigestSHA1, 0, (const void *)sigblob,
		(size_t)len);
	CCRSACryptorRelease(cc_key);

	return (cc_status_to_openssl_err(status) == 0);
}


int RSA_public_encrypt(int flen, const unsigned char *from, unsigned char *to, RSA *rsa, int padding);

static int
openssl_to_cc_padding(int padding)
{
	switch (padding) {
	case RSA_PKCS1_PADDING:
		return (ccPKCS1Padding);

	case RSA_PKCS1_OAEP_PADDING:
		return (ccOAEPPadding);

#if 0
	case RSA_X931_PADDING:
		return (ccX931Padding);

	case RSA_NO_PADDING:
		return (ccPaddingNone);
#endif
	default:
		/* XXX RSAerr( , ); */
		return (-1);
	}
}


static int
cc_rsa_public_encrypt(int ilen, const unsigned char *inbuf, unsigned char *outbuf,
    RSA *key, int padding)
{
	CCRSACryptorRef cc_key = get_CCRSACryptorRef(ccRSAKeyPublic, key);
	CCCryptorStatus status;
	size_t olen = (size_t)BN_num_bytes(key->n);
	int cc_padding;

	if ((cc_padding = openssl_to_cc_padding(padding)) == -1) {
		return (-1);
	}


	if (NULL == cc_key) {
		/* XXX RSAerr( , ); */
		return (-1);
	}

	status = CCRSACryptorEncrypt(cc_key, cc_padding, (const void *)inbuf,
		(size_t)ilen, (void *)outbuf, (size_t *)&olen, NULL, 0, kCCDigestSHA1);

	CCRSACryptorRelease(cc_key);
	/* See if CCRSACryptorEncrypt() returns the undocumented -1 error. */
	if (-1 == status) {
		/* couldn't open /dev/random?  Use OpenSSL's RSA encrypt for now. */
		olen = RSA_public_encrypt(ilen, inbuf, outbuf, key, padding);
		return ((int)olen);
	}

	if (cc_status_to_openssl_err(status) != 0) {
		return (-1);
	} else {
		return ((int)olen);
	}
}


#include "tommath.h"
#include "rk-roken.h"
typedef struct Rsa_key {
	/** Type of key, PK_PRIVATE or PK_PUBLIC */
	int	type;
	/** The public exponent */
	void *	e;
	/** The private exponent */
	void *	d;
	/** The modulus */
	void *	N;
	/** The p factor of N */
	void *	p;
	/** The q factor of N */
	void *	q;
	/** The 1/q mod p CRT param */
	void *	qP;
	/** The d mod (p - 1) CRT param */
	void *	dP;
	/** The d mod (q - 1) CRT param */
	void *	dQ;
} rsa_key;
typedef struct _CCRSACryptor {
	rsa_key		key;
	CCRSAKeyType	keyType;
} CCRSACryptor;

static int
cc_rsa_private_decrypt(int ilen, const unsigned char *inbuf, unsigned char *outbuf,
    RSA *key, int padding)
{
	CCRSACryptorRef cc_key = get_CCRSACryptorRef(ccRSAKeyPrivate, key);
	CCCryptorStatus status;
	size_t olen = (size_t)BN_num_bytes(key->n);
	int cc_padding;

	if ((cc_padding = openssl_to_cc_padding(padding)) == -1) {
		return (-1);
	}

	if (NULL == cc_key) {
		/* XXX RSAerr( , ); */
		return (-1);
	}

	status = CCRSACryptorDecrypt(cc_key, cc_padding, (const void *)inbuf, (size_t)ilen,
		(void *)outbuf, (size_t * )&olen, NULL, 0, kCCDigestSHA1);
	CCRSACryptorRelease(cc_key);

	if (cc_status_to_openssl_err(status) != 0) {
		return (-1);
	} else {
		return ((int)olen);
	}
}


#ifndef PR_10783242_FIXED

/*
 * XXX The following code is needed because CommonCrypto doesn't provide RSA private key
 * encryption and RSA public key decryption.  See <rdar://problem/10783242>
 */

#ifndef RSA_MAX_MODULUS_BITS
# define RSA_MAX_MODULUS_BITS		16384
#endif
#ifndef RSA_SMALL_MODULUS_BITS
# define RSA_SMALL_MODULUS_BITS		3072
#endif
#ifndef RSA_MAX_PUBEXP_BITS
# define RSA_MAX_PUBEXP_BITS		64 /* exponent limit enforced for "large" modulus only */
#endif

static int
RSA_padding_add_PKCS1_type_1(unsigned char *to, int tlen,
    const unsigned char *from, int flen)
{
	int j;
	unsigned char *p;

	if (flen > (tlen - RSA_PKCS1_PADDING_SIZE)) {
		/* RSAerr(RSA_F_RSA_PADDING_ADD_PKCS1_TYPE_1,RSA_R_DATA_TOO_LARGE_FOR_KEY_SIZE); */
		return (0);
	}

	p = (unsigned char *)to;

	*(p++) = 0;
	*(p++) = 1; /* Private Key BT (Block Type) */

	/* pad out with 0xff data */
	j = tlen - 3 - flen;
	memset(p, 0xff, j);
	p += j;
	*(p++) = '\0';
	memcpy(p, from, (unsigned int)flen);
	return (1);
}


static int
RSA_padding_check_PKCS1_type_1(unsigned char *to, int tlen,
    const unsigned char *from, int flen, int num)
{
	int i, j;
	const unsigned char *p;

	p = from;
	if ((num != (flen+1)) || (*(p++) != 01)) {
		/* RSAerr(RSA_F_RSA_PADDING_CHECK_PKCS1_TYPE_1,RSA_R_BLOCK_TYPE_IS_NOT_01); */
		return (-1);
	}

	/* scan over padding data */
	j = flen - 1; /* one for type. */
	for (i = 0; i < j; i++) {
		if (*p != 0xff) {
			/* should decrypt to 0xff */
			if (*p == 0) {
				p++;
				break;
			} else {
				/* RSAerr(RSA_F_RSA_PADDING_CHECK_PKCS1_TYPE_1,RSA_R_BAD_FIXED_HEADER_DECRYPT); */
				return (-1);
			}
		}
		p++;
	}

	if (i == j) {
		/* RSAerr(RSA_F_RSA_PADDING_CHECK_PKCS1_TYPE_1,RSA_R_NULL_BEFORE_BLOCK_MISSING); */
		return (-1);
	}

	if (i < 8) {
		/* RSAerr(RSA_F_RSA_PADDING_CHECK_PKCS1_TYPE_1,RSA_R_BAD_PAD_BYTE_COUNT); */
		return (-1);
	}
	i++; /* Skip over the '\0' */
	j -= i;
	if (j > tlen) {
		/* RSAerr(RSA_F_RSA_PADDING_CHECK_PKCS1_TYPE_1,RSA_R_DATA_TOO_LARGE); */
		return (-1);
	}
	memcpy(to, p, (unsigned int)j);

	return (j);
}


static int
RSA_padding_add_PKCS1_type_2(unsigned char *to, int tlen,
    const unsigned char *from, int flen)
{
	int i, j;
	unsigned char *p;

	if (flen > (tlen-11)) {
		/* RSAerr(RSA_F_RSA_PADDING_ADD_PKCS1_TYPE_2,RSA_R_DATA_TOO_LARGE_FOR_KEY_SIZE); */
		return (0);
	}

	p = (unsigned char *)to;

	*(p++) = 0;
	*(p++) = 2; /* Public Key BT (Block Type) */

	/* pad out with non-zero random data */
	j = tlen - 3 - flen;

	if (RAND_bytes(p, j) <= 0) {
		return (0);
	}
	for (i = 0; i < j; i++) {
		if (*p == '\0') {
			do {
				if (RAND_bytes(p, 1) <= 0) {
					return (0);
				}
			} while (*p == '\0');
		}
		p++;
	}

	*(p++) = '\0';

	memcpy(p, from, (unsigned int)flen);

	return (1);
}


static int
RSA_padding_check_PKCS1_type_2(unsigned char *to, int tlen,
    const unsigned char *from, int flen, int num)
{
	int i, j;
	const unsigned char *p;

	p = from;
	if ((num != (flen+1)) || (*(p++) != 02)) {
		/* RSAerr(RSA_F_RSA_PADDING_CHECK_PKCS1_TYPE_2,RSA_R_BLOCK_TYPE_IS_NOT_02); */
		return (-1);
	}
#ifdef PKCS1_CHECK
	return (num - 11);
#endif

	/* scan over padding data */
	j = flen - 1; /* one for type. */
	for (i = 0; i < j; i++) {
		if (*(p++) == 0) {
			break;
		}
	}

	if (i == j) {
		/* RSAerr(RSA_F_RSA_PADDING_CHECK_PKCS1_TYPE_2,RSA_R_NULL_BEFORE_BLOCK_MISSING); */
		return (-1);
	}

	if (i < 8) {
		/* RSAerr(RSA_F_RSA_PADDING_CHECK_PKCS1_TYPE_2,RSA_R_BAD_PAD_BYTE_COUNT); */
		return (-1);
	}
	i++; /* Skip over the '\0' */
	j -= i;
	if (j > tlen) {
		/* RSAerr(RSA_F_RSA_PADDING_CHECK_PKCS1_TYPE_2,RSA_R_DATA_TOO_LARGE); */
		return (-1);
	}
	memcpy(to, p, (unsigned int)j);

	return (j);
}


/* signing */
static int
cc_rsa_private_encrypt(int flen, const unsigned char *from,
    unsigned char *to, RSA *rsa, int padding)
{
	BIGNUM *f, *ret, *br, *res;
	int i, j, k, num = 0, r = -1;
	unsigned char *buf = NULL;
	BN_CTX *ctx = NULL;

#if 0
	int local_blinding = 0;
	BN_BLINDING *blinding = NULL;
#endif

	if ((ctx = BN_CTX_new()) == NULL) {
		goto err;
	}
	BN_CTX_start(ctx);
	f = BN_CTX_get(ctx);
	br = BN_CTX_get(ctx);
	ret = BN_CTX_get(ctx);
	num = BN_num_bytes(rsa->n);
	buf = malloc(num);
	if (!f || !ret || !buf) {
		/* RSAerr(RSA_F_RSA_EAY_PRIVATE_ENCRYPT,ERR_R_MALLOC_FAILURE); */
		goto err;
	}

	switch (padding) {
	case RSA_PKCS1_PADDING:
		i = RSA_padding_add_PKCS1_type_1(buf, num, from, flen);
		break;

#if 0
	case RSA_X931_PADDING:
		i = RSA_padding_add_X931(buf, num, from, flen);
		break;

	case RSA_NO_PADDING:
		i = RSA_padding_add_none(buf, num, from, flen);
		break;

	case RSA_SSLV23_PADDING:
#endif
	default:
		/* RSAerr(RSA_F_RSA_EAY_PRIVATE_ENCRYPT,RSA_R_UNKNOWN_PADDING_TYPE); */
		goto err;
	}
	if (i <= 0) {
		goto err;
	}

	if (BN_bin2bn(buf, num, f) == NULL) {
		goto err;
	}

	if (BN_ucmp(f, rsa->n) >= 0) {
		/* usually the padding functions would catch this */
		/* RSAerr(RSA_F_RSA_EAY_PRIVATE_ENCRYPT,RSA_R_DATA_TOO_LARGE_FOR_MODULUS); */
		goto err;
	}

#if 0
	if (!(rsa->flags & RSA_FLAG_NO_BLINDING)) {
		blinding = rsa_get_blinding(rsa, &local_blinding, ctx);
		if (blinding == NULL) {
			/* RSAerr(RSA_F_RSA_EAY_PRIVATE_ENCRYPT, ERR_R_INTERNAL_ERROR); */
			goto err;
		}
	}

	if (blinding != NULL) {
		if (!rsa_blinding_convert(blinding, local_blinding, f, br, ctx)) {
			goto err;
		}
	}
#endif

	if ((rsa->flags & RSA_FLAG_EXT_PKEY) ||
	    ((rsa->p != NULL) &&
	    (rsa->q != NULL) &&
	    (rsa->dmp1 != NULL) &&
	    (rsa->dmq1 != NULL) &&
	    (rsa->iqmp != NULL))) {
		if (!rsa->meth->rsa_mod_exp(ret, f, rsa, ctx)) {
			goto err;
		}
	} else {
		BIGNUM local_d;
		BIGNUM *d = NULL;

		if (!(rsa->flags & RSA_FLAG_NO_CONSTTIME)) {
			BN_init(&local_d);
			d = &local_d;
			BN_with_flags(d, rsa->d, BN_FLG_CONSTTIME);
		} else{
			d = rsa->d;
		}

#if 0
		if (rsa->flags & RSA_FLAG_CACHE_PUBLIC) {
			if (!BN_MONT_CTX_set_locked(&rsa->_method_mod_n, CRYPTO_LOCK_RSA, rsa->n, ctx)) {
				goto err;
			}
		}
#endif

		if (!rsa->meth->bn_mod_exp(ret, f, d, rsa->n, ctx, rsa->_method_mod_n)) {
			goto err;
		}
	}

#if 0
	if (blinding) {
		if (!rsa_blinding_invert(blinding, local_blinding, ret, br, ctx)) {
			goto err;
		}
	}
#endif

#if 0
	if (padding == RSA_X931_PADDING) {
		BN_sub(f, rsa->n, ret);
		if (BN_cmp(ret, f)) {
			res = f;
		} else{
			res = ret;
		}
	} else
#endif
	res = ret;

	/* put in leading 0 bytes if the number is less than the
	 * length of the modulus */
	j = BN_num_bytes(res);
	i = BN_bn2bin(res, &(to[num-j]));
	for (k = 0; k < (num-i); k++) {
		to[k] = 0;
	}

	r = num;
err:
	if (ctx != NULL) {
		BN_CTX_end(ctx);
		BN_CTX_free(ctx);
	}
	if (buf != NULL) {
		memset(buf, 0, num);
		free(buf);
	}
	return (r);
}


/* signature verification */
static int
cc_rsa_public_decrypt(int flen, const unsigned char *from,
    unsigned char *to, RSA *rsa, int padding)
{
	BIGNUM *f, *ret;
	int i, num = 0, r = -1;
	unsigned char *p;
	unsigned char *buf = NULL;
	BN_CTX *ctx = NULL;

	if (BN_num_bits(rsa->n) > RSA_MAX_MODULUS_BITS) {
		/* RSAerr(RSA_F_RSA_EAY_PUBLIC_DECRYPT, RSA_R_MODULUS_TOO_LARGE); */
		return (-1);
	}

	if (BN_ucmp(rsa->n, rsa->e) <= 0) {
		/* RSAerr(RSA_F_RSA_EAY_PUBLIC_DECRYPT, RSA_R_BAD_E_VALUE); */
		return (-1);
	}

	/* for large moduli, enforce exponent limit */
	if (BN_num_bits(rsa->n) > RSA_SMALL_MODULUS_BITS) {
		if (BN_num_bits(rsa->e) > RSA_MAX_PUBEXP_BITS) {
			/* RSAerr(RSA_F_RSA_EAY_PUBLIC_DECRYPT, RSA_R_BAD_E_VALUE); */
			return (-1);
		}
	}

	if ((ctx = BN_CTX_new()) == NULL) {
		goto err;
	}
	BN_CTX_start(ctx);
	f = BN_CTX_get(ctx);
	ret = BN_CTX_get(ctx);
	num = BN_num_bytes(rsa->n);
	buf = malloc(num);
	if (!f || !ret || !buf) {
		/* RSAerr(RSA_F_RSA_EAY_PUBLIC_DECRYPT,ERR_R_MALLOC_FAILURE); */
		goto err;
	}

	/* This check was for equality but PGP does evil things
	 * and chops off the top '0' bytes */
	if (flen > num) {
		/* RSAerr(RSA_F_RSA_EAY_PUBLIC_DECRYPT,RSA_R_DATA_GREATER_THAN_MOD_LEN); */
		goto err;
	}

	if (BN_bin2bn(from, flen, f) == NULL) {
		goto err;
	}

	if (BN_ucmp(f, rsa->n) >= 0) {
		/* RSAerr(RSA_F_RSA_EAY_PUBLIC_DECRYPT,RSA_R_DATA_TOO_LARGE_FOR_MODULUS); */
		goto err;
	}

#if 0
	if (rsa->flags & RSA_FLAG_CACHE_PUBLIC) {
		if (!BN_MONT_CTX_set_locked(&rsa->_method_mod_n, CRYPTO_LOCK_RSA, rsa->n, ctx)) {
			goto err;
		}
	}
#endif

	if (!rsa->meth->bn_mod_exp(ret, f, rsa->e, rsa->n, ctx, rsa->_method_mod_n)) {
		goto err;
	}

#if 0
	if ((padding == RSA_X931_PADDING) && ((ret->d[0] & 0xf) != 12)) {
		if (!BN_sub(ret, rsa->n, ret)) {
			goto err;
		}
	}
#endif

	p = buf;
	i = BN_bn2bin(ret, p);

	switch (padding) {
	case RSA_PKCS1_PADDING:
		r = RSA_padding_check_PKCS1_type_1(to, num, buf, i, num);
		break;

#if 0
	case RSA_X931_PADDING:
		r = RSA_padding_check_X931(to, num, buf, i, num);
		break;

	case RSA_NO_PADDING:
		r = RSA_padding_check_none(to, num, buf, i, num);
		break;
#endif
	default:
		/* RSAerr(RSA_F_RSA_EAY_PUBLIC_DECRYPT,RSA_R_UNKNOWN_PADDING_TYPE); */
		goto err;
	}

	/*
	 * if (r < 0)
	 *      RSAerr(RSA_F_RSA_EAY_PUBLIC_DECRYPT,RSA_R_PADDING_CHECK_FAILED);
	 */

err:
	if (ctx != NULL) {
		BN_CTX_end(ctx);
		BN_CTX_free(ctx);
	}
	if (buf != NULL) {
		memset(buf, 0, num);
		free(buf);
	}

	return (r);
}


static int
cc_rsa_mod_exp(BIGNUM *r0, const BIGNUM *I, RSA *rsa, BN_CTX *ctx)
{
	BIGNUM *r1, *m1, *vrfy;
	BIGNUM local_dmp1, local_dmq1, local_c, local_r1;
	BIGNUM *dmp1, *dmq1, *c, *pr1;
	int ret = 0;

	BN_CTX_start(ctx);
	r1 = BN_CTX_get(ctx);
	m1 = BN_CTX_get(ctx);
	vrfy = BN_CTX_get(ctx);

	{
		BIGNUM local_p, local_q;
		BIGNUM *p = NULL, *q = NULL;

		/* Make sure BN_mod_inverse in Montgomery intialization uses the
		 * BN_FLG_CONSTTIME flag (unless RSA_FLAG_NO_CONSTTIME is set)
		 */
		if (!(rsa->flags & RSA_FLAG_NO_CONSTTIME)) {
			BN_init(&local_p);
			p = &local_p;
			BN_with_flags(p, rsa->p, BN_FLG_CONSTTIME);

			BN_init(&local_q);
			q = &local_q;
			BN_with_flags(q, rsa->q, BN_FLG_CONSTTIME);
		} else {
			p = rsa->p;
			q = rsa->q;
		}

#if 0
		if (rsa->flags & RSA_FLAG_CACHE_PRIVATE) {
			if (!BN_MONT_CTX_set_locked(&rsa->_method_mod_p, CRYPTO_LOCK_RSA, p, ctx)) {
				goto err;
			}
			if (!BN_MONT_CTX_set_locked(&rsa->_method_mod_q, CRYPTO_LOCK_RSA, q, ctx)) {
				goto err;
			}
		}
#endif
	}

#if 0
	if (rsa->flags & RSA_FLAG_CACHE_PUBLIC) {
		if (!BN_MONT_CTX_set_locked(&rsa->_method_mod_n, CRYPTO_LOCK_RSA, rsa->n, ctx)) {
			goto err;
		}
	}
#endif

	/* compute I mod q */
	if (!(rsa->flags & RSA_FLAG_NO_CONSTTIME)) {
		c = &local_c;
		BN_with_flags(c, I, BN_FLG_CONSTTIME);
		if (!BN_mod(r1, c, rsa->q, ctx)) {
			goto err;
		}
	} else {
		if (!BN_mod(r1, I, rsa->q, ctx)) {
			goto err;
		}
	}

	/* compute r1^dmq1 mod q */
	if (!(rsa->flags & RSA_FLAG_NO_CONSTTIME)) {
		dmq1 = &local_dmq1;
		BN_with_flags(dmq1, rsa->dmq1, BN_FLG_CONSTTIME);
	} else{
		dmq1 = rsa->dmq1;
	}

	if (!rsa->meth->bn_mod_exp(m1, r1, dmq1, rsa->q, ctx, rsa->_method_mod_q)) {
		goto err;
	}

	/* compute I mod p */
	if (!(rsa->flags & RSA_FLAG_NO_CONSTTIME)) {
		c = &local_c;
		BN_with_flags(c, I, BN_FLG_CONSTTIME);
		if (!BN_mod(r1, c, rsa->p, ctx)) {
			goto err;
		}
	} else {
		if (!BN_mod(r1, I, rsa->p, ctx)) {
			goto err;
		}
	}

	/* compute r1^dmp1 mod p */
	if (!(rsa->flags & RSA_FLAG_NO_CONSTTIME)) {
		dmp1 = &local_dmp1;
		BN_with_flags(dmp1, rsa->dmp1, BN_FLG_CONSTTIME);
	} else{
		dmp1 = rsa->dmp1;
	}
	if (!rsa->meth->bn_mod_exp(r0, r1, dmp1, rsa->p, ctx, rsa->_method_mod_p)) {
		goto err;
	}

	if (!BN_sub(r0, r0, m1)) {
		goto err;
	}

	/* This will help stop the size of r0 increasing, which does
	 * affect the multiply if it optimised for a power of 2 size */
	if (BN_is_negative(r0)) {
		if (!BN_add(r0, r0, rsa->p)) {
			goto err;
		}
	}

	if (!BN_mul(r1, r0, rsa->iqmp, ctx)) {
		goto err;
	}

	/* Turn BN_FLG_CONSTTIME flag on before division operation */
	if (!(rsa->flags & RSA_FLAG_NO_CONSTTIME)) {
		pr1 = &local_r1;
		BN_with_flags(pr1, r1, BN_FLG_CONSTTIME);
	} else{
		pr1 = r1;
	}
	if (!BN_mod(r0, pr1, rsa->p, ctx)) {
		goto err;
	}

	/* If p < q it is occasionally possible for the correction of
	 * adding 'p' if r0 is negative above to leave the result still
	 * negative. This can break the private key operations: the following
	 * second correction should *always* correct this rare occurrence.
	 * This will *never* happen with OpenSSL generated keys because
	 * they ensure p > q [steve]
	 */
	if (BN_is_negative(r0)) {
		if (!BN_add(r0, r0, rsa->p)) {
			goto err;
		}
	}
	if (!BN_mul(r1, r0, rsa->q, ctx)) {
		goto err;
	}
	if (!BN_add(r0, r1, m1)) {
		goto err;
	}

	if (rsa->e && rsa->n) {
		if (!rsa->meth->bn_mod_exp(vrfy, r0, rsa->e, rsa->n, ctx,
		    rsa->_method_mod_n)) {
			goto err;
		}

		/* If 'I' was greater than (or equal to) rsa->n, the operation
		 * will be equivalent to using 'I mod n'. However, the result of
		 * the verify will *always* be less than 'n' so we don't check
		 * for absolute equality, just congruency. */
		if (!BN_sub(vrfy, vrfy, I)) {
			goto err;
		}
		if (!BN_mod(vrfy, vrfy, rsa->n, ctx)) {
			goto err;
		}
		if (BN_is_negative(vrfy)) {
			if (!BN_add(vrfy, vrfy, rsa->n)) {
				goto err;
			}
		}
		if (!BN_is_zero(vrfy)) {
			/* 'I' and 'vrfy' aren't congruent mod n. Don't leak
			 * miscalculated CRT output, just do a raw (slower)
			 * mod_exp and return that instead. */

			BIGNUM local_d;
			BIGNUM *d = NULL;

			if (!(rsa->flags & RSA_FLAG_NO_CONSTTIME)) {
				d = &local_d;
				BN_with_flags(d, rsa->d, BN_FLG_CONSTTIME);
			} else{
				d = rsa->d;
			}
			if (!rsa->meth->bn_mod_exp(r0, I, d, rsa->n, ctx,
			    rsa->_method_mod_n)) {
				goto err;
			}
		}
	}
	ret = 1;
err:
	BN_CTX_end(ctx);
	return (ret);
}


#endif /* ! PR_10783242_FIXED */

/*
 * RSA key generation.
 */
#if 0
int
CC_RSA_generate_key_ex(RSA *rsa, int keybits, BIGNUM *bn_e, void *cb)
{
	CCCryptorStatus status;
	unsigned long eword = BN_get_word(bn_e);
	CCRSACryptorRef public = NULL, private = NULL;
	uint8_t *npublic = NULL, *nprivate = NULL, *e = NULL, *d = NULL, *p = NULL, *q = NULL;
	size_t npubliclen, nprivatelen, elen, dlen, plen, qlen;
	size_t keybytes = ((keybits + 7) / 8) + 1;

	if (0xffffffffL == eword) {
		/* XXX Need to set OpenSSL error code here on failure. */
		return (0);
	}

	status = CCRSACryptorGeneratePair((size_t)keybits, (uint32_t)eword, &public, &private);
	if (kCCSuccess != status) {
		/* XXX Need to set OpenSSL error code here on failure. */
		return (0);
	}

	if ((npublic = malloc(keybytes)) == NULL) {
		goto outerr;
	}
	if ((nprivate = malloc(keybytes)) == NULL) {
		goto outerr;
	}
	if ((e = malloc(keybytes)) == NULL) {
		goto outerr;
	}
	if ((d = malloc(keybytes)) == NULL) {
		goto outerr;
	}
	if ((p = malloc(keybytes)) == NULL) {
		goto outerr;
	}
	if ((q = malloc(keybytes)) == NULL) {
		goto outerr;
	}

	npubliclen = elen = keybytes;
	dlen = plen = qlen = 0;
	status = CCRSAGetKeyComponents(public, npublic, &npubliclen, e, &elen, p, &plen, q, &qlen);
	if (kCCSuccess != status) {
		/* XXX Need to set OpenSSL error code here on failure. */
		return (0);
	}
	if ((rsa->n = BN_bin2bn((const unsigned char *)npublic, (int)npubliclen, NULL)) == NULL) {
		goto outerr;
	}
	if ((rsa->e = BN_bin2bn((const unsigned char *)e, (int)elen, NULL)) == NULL) {
		goto outerr;
	}

	/* NOTE: d is returned in the exponent (e) for the private key */
	nprivatelen = dlen = plen = qlen = keybytes;
	status = CCRSAGetKeyComponents(private, nprivate, &nprivatelen, d, &dlen, p, &plen, q, &qlen);
	if (kCCSuccess != status) {
		/* XXX Need to set OpenSSL error code here on failure. */
		return (0);
	}

	if ((npubliclen != nprivatelen) || (memcmp(npublic, nprivate, nprivatelen) != 0)) {
		goto outerr;
	}

	/* We need the RSA components non-NULL */
	if (!rsa->n && ((rsa->n = BN_new()) == NULL)) {
		goto outerr;
	}
	if (!rsa->d && ((rsa->d = BN_new()) == NULL)) {
		goto outerr;
	}
	if (!rsa->e && ((rsa->e = BN_new()) == NULL)) {
		goto outerr;
	}
	if (!rsa->p && ((rsa->p = BN_new()) == NULL)) {
		goto outerr;
	}
	if (!rsa->q && ((rsa->q = BN_new()) == NULL)) {
		goto outerr;
	}
	if (!rsa->dmp1 && ((rsa->dmp1 = BN_new()) == NULL)) {
		goto outerr;
	}
	if (!rsa->dmq1 && ((rsa->dmq1 = BN_new()) == NULL)) {
		goto outerr;
	}
	if (!rsa->iqmp && ((rsa->iqmp = BN_new()) == NULL)) {
		goto outerr;
	}

	if (BN_bin2bn((const unsigned char *)npublic, (int)npubliclen, rsa->n) == NULL) {
		goto outerr;
	}
	if (BN_bin2bn((const unsigned char *)e, (int)elen, rsa->e) == NULL) {
		goto outerr;
	}
	if (BN_bin2bn((const unsigned char *)d, (int)dlen, rsa->d) == NULL) {
		goto outerr;
	}
	if (BN_bin2bn((const unsigned char *)p, (int)plen, rsa->p) == NULL) {
		goto outerr;
	}
	if (BN_bin2bn((const unsigned char *)q, (int)qlen, rsa->q) == NULL) {
		goto outerr;
	}

	if (RSA_check_key(rsa) != 1) {
		char buf[1024];
		unsigned long err = ERR_get_error();
		ERR_error_string_n(err, buf, 1024);
		printf("RSA is invalid!! %s\n", buf);
		return (0);
	}

	BIGNUM local_d, local_p;
	BIGNUM *dbn, *pbn;
	BIGNUM *r1 = NULL, *r2 = NULL;
	BN_CTX *ctx = NULL;

	ctx = BN_CTX_new();
	if (ctx == NULL) {
		goto outerr;
	}
	BN_CTX_start(ctx);
	if ((r1 = BN_CTX_get(ctx)) == NULL) {
		goto outerr;
	}
	if ((r2 = BN_CTX_get(ctx)) == NULL) {
		goto outerr;
	}

	/* set up d for correct BN_FLG_CONSTTIME flag */
	if (!(rsa->flags & RSA_FLAG_NO_CONSTTIME)) {
		dbn = &local_d;
		BN_with_flags(dbn, rsa->d, BN_FLG_CONSTTIME);
	}else          {
		dbn = rsa->d;
	}

	/* calculate d mod (p-1) */
	if (!BN_sub(r1, rsa->p, BN_value_one())) {
		goto outerr;                                    /* p-1 */
	}
	if (!BN_mod(rsa->dmp1, dbn, r1, ctx)) {
		goto outerr;
	}

	/* calculate d mod (q-1) */
	if (!BN_sub(r2, rsa->q, BN_value_one())) {
		goto outerr;                                    /* q-1 */
	}
	if (!BN_mod(rsa->dmq1, dbn, r2, ctx)) {
		goto outerr;
	}

	/* calculate inverse of q mod p */
	if (!(rsa->flags & RSA_FLAG_NO_CONSTTIME)) {
		pbn = &local_p;
		BN_with_flags(pbn, rsa->p, BN_FLG_CONSTTIME);
	}else          {
		pbn = rsa->p;
	}
	if (!BN_mod_inverse(rsa->iqmp, rsa->q, pbn, ctx)) {
		goto outerr;
	}

	CCRSACryptorRelease(public);
	CCRSACryptorRelease(private);
	free(npublic);
	free(nprivate);
	free(e);
	free(d);
	free(p);
	free(q);
	BN_CTX_end(ctx);
	BN_CTX_free(ctx);

	return (1);

outerr:
	if (ctx != NULL) {
		BN_CTX_end(ctx);
		BN_CTX_free(ctx);
	}
	if (public != NULL) {
		CCRSACryptorRelease(public);
	}
	if (private != NULL) {
		CCRSACryptorRelease(private);
	}

	if (rsa->n != NULL) {
		BN_clear_free(rsa->n);
	}
	if (rsa->e != NULL) {
		BN_clear_free(rsa->e);
	}
	if (rsa->d != NULL) {
		BN_clear_free(rsa->d);
	}
	if (rsa->p != NULL) {
		BN_clear_free(rsa->p);
	}
	if (rsa->q != NULL) {
		BN_clear_free(rsa->q);
	}
	if (rsa->dmp1 != NULL) {
		BN_clear_free(rsa->dmp1);
	}
	if (rsa->dmq1 != NULL) {
		BN_clear_free(rsa->dmq1);
	}
	if (rsa->iqmp != NULL) {
		BN_clear_free(rsa->iqmp);
	}

	if (NULL == npublic) {
		free(npublic);
	}
	if (NULL == npublic) {
		free(nprivate);
	}
	if (NULL == e) {
		free(e);
	}
	if (NULL == d) {
		free(d);
	}
	if (NULL == p) {
		free(p);
	}
	if (NULL == q) {
		free(q);
	}

	/* XXX Need to set OpenSSL error code here on failure. */
	return (0);
}


#else

static int
cc_rsa_builtin_keygen(RSA *rsa, int bits, BIGNUM *e_value, void *cb)
{
	BIGNUM *r0 = NULL, *r1 = NULL, *r2 = NULL, *r3 = NULL, *tmp;
	BIGNUM local_r0, local_d, local_p;
	BIGNUM *pr0, *d, *p;
	int bitsp, bitsq, ok = -1, n = 0;
	BN_CTX *ctx = NULL;

	ctx = BN_CTX_new();
	if (ctx == NULL) {
		goto err;
	}
	BN_CTX_start(ctx);
	r0 = BN_CTX_get(ctx);
	r1 = BN_CTX_get(ctx);
	r2 = BN_CTX_get(ctx);
	r3 = BN_CTX_get(ctx);
	if (r3 == NULL) {
		goto err;
	}

	bitsp = (bits + 1) / 2;
	bitsq = bits - bitsp;

	/* We need the RSA components to be non-NULL */
	if (!rsa->n && ((rsa->n = BN_new()) == NULL)) {
		goto err;
	}
	if (!rsa->d && ((rsa->d = BN_new()) == NULL)) {
		goto err;
	}
	if (!rsa->e && ((rsa->e = BN_new()) == NULL)) {
		goto err;
	}
	if (!rsa->p && ((rsa->p = BN_new()) == NULL)) {
		goto err;
	}
	if (!rsa->q && ((rsa->q = BN_new()) == NULL)) {
		goto err;
	}
	if (!rsa->dmp1 && ((rsa->dmp1 = BN_new()) == NULL)) {
		goto err;
	}
	if (!rsa->dmq1 && ((rsa->dmq1 = BN_new()) == NULL)) {
		goto err;
	}
	if (!rsa->iqmp && ((rsa->iqmp = BN_new()) == NULL)) {
		goto err;
	}

	BN_copy(rsa->e, e_value);

	/* generate p and q */
	for ( ; ; ) {
		if (!BN_generate_prime_ex(rsa->p, bitsp, 0, NULL, NULL, NULL)) {
			goto err;
		}
		if (!BN_sub(r2, rsa->p, BN_value_one())) {
			goto err;
		}
		if (!BN_gcd(r1, r2, rsa->e, ctx)) {
			goto err;
		}
		if (BN_is_one(r1)) {
			break;
		}
	}
	for ( ; ; ) {
		/* When generating ridiculously small keys, we can get stuck
		 * continually regenerating the same prime values. Check for
		 * this and bail if it happens 3 times. */
		unsigned int degenerate = 0;
		do {
			if (!BN_generate_prime_ex(rsa->q, bitsq, 0, NULL, NULL, NULL)) {
				goto err;
			}
		} while ((BN_cmp(rsa->p, rsa->q) == 0) && (++degenerate < 3));
		if (degenerate == 3) {
			ok = 0; /* we set our own err */
			/* RSAerr(RSA_F_RSA_BUILTIN_KEYGEN,RSA_R_KEY_SIZE_TOO_SMALL); */
			goto err;
		}
		if (!BN_sub(r2, rsa->q, BN_value_one())) {
			goto err;
		}
		if (!BN_gcd(r1, r2, rsa->e, ctx)) {
			goto err;
		}
		if (BN_is_one(r1)) {
			break;
		}
	}
	if (BN_cmp(rsa->p, rsa->q) < 0) {
		tmp = rsa->p;
		rsa->p = rsa->q;
		rsa->q = tmp;
	}

	/* calculate n */
	if (!BN_mul(rsa->n, rsa->p, rsa->q, ctx)) {
		goto err;
	}

	/* calculate d */
	if (!BN_sub(r1, rsa->p, BN_value_one())) {
		goto err;                                       /* p-1 */
	}
	if (!BN_sub(r2, rsa->q, BN_value_one())) {
		goto err;                                       /* q-1 */
	}
	if (!BN_mul(r0, r1, r2, ctx)) {
		goto err;                       /* (p-1)(q-1) */
	}
	if (!(rsa->flags & RSA_FLAG_NO_CONSTTIME)) {
		pr0 = &local_r0;
		BN_with_flags(pr0, r0, BN_FLG_CONSTTIME);
	} else{
		pr0 = r0;
	}

	if (!BN_mod_inverse(rsa->d, rsa->e, pr0, ctx)) {
		goto err;                                               /* d */
	}
	/* set up d for correct BN_FLG_CONSTTIME flag */
	if (!(rsa->flags & RSA_FLAG_NO_CONSTTIME)) {
		d = &local_d;
		BN_with_flags(d, rsa->d, BN_FLG_CONSTTIME);
	} else{
		d = rsa->d;
	}

	/* calculate d mod (p-1) */
	if (!BN_mod(rsa->dmp1, d, r1, ctx)) {
		goto err;
	}

	/* calculate d mod (q-1) */
	if (!BN_mod(rsa->dmq1, d, r2, ctx)) {
		goto err;
	}

	/* calculate inverse of q mod p */
	if (!(rsa->flags & RSA_FLAG_NO_CONSTTIME)) {
		p = &local_p;
		BN_with_flags(p, rsa->p, BN_FLG_CONSTTIME);
	} else{
		p = rsa->p;
	}
	if (!BN_mod_inverse(rsa->iqmp, rsa->q, p, ctx)) {
		goto err;
	}

	ok = 1;
err:
	if (ok == -1) {
		/* RSAerr(RSA_F_RSA_BUILTIN_KEYGEN,ERR_LIB_BN); */
		ok = 0;
	}
	if (ctx != NULL) {
		BN_CTX_end(ctx);
		BN_CTX_free(ctx);
	}

	return (ok);
}


#endif /* ! #if 0 */


static int cc_rsa_generate_key(RSA *rsa, int bits, BIGNUM *e_value, BN_GENCB *cb)
{
	return (cc_rsa_builtin_keygen(rsa, bits, e_value, cb));
}


const RSA_METHOD _ossl_rsa_cc_method =
{
	.name		= "CommonCrypto RSA",
	.rsa_pub_enc	= cc_rsa_public_encrypt,
	.rsa_pub_dec	= cc_rsa_public_decrypt,
	.rsa_priv_enc	= cc_rsa_private_encrypt,
	.rsa_priv_dec	= cc_rsa_private_decrypt,
	.rsa_mod_exp	= cc_rsa_mod_exp,
	.bn_mod_exp	= BN_mod_exp_mont,
	.init		= cc_rsa_init,
	.finish		= cc_rsa_finish,
	.flags		=		     0,
	.app_data	= NULL,
	.rsa_sign	= cc_rsa_sign,
	.rsa_verify	= cc_rsa_verify,
	.rsa_keygen	= cc_rsa_generate_key
};
#endif /* HAVE_COMMONCRYPTO_COMMONRSACRYPTOR_H */

const RSA_METHOD *
RSA_cc_method(void)
{
#ifdef HAVE_COMMONCRYPTO_COMMONRSACRYPTOR_H
	return (&_ossl_rsa_cc_method);

#else
	return (NULL);
#endif
}

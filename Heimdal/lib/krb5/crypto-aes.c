/*
 * Copyright (c) 1997 - 2008 Kungliga Tekniska Högskolan
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

#include "krb5_locl.h"

/*
 * AES
 */

#ifdef __APPLE_TARGET_EMBEDDED__

#include <CommonCrypto/CommonCryptor.h>
#include <CommonCrypto/CommonCryptorSPI.h>

/*
 * CommonCrypto based
 */

struct cc_schedule {
    CCCryptorRef enc;
    CCCryptorRef dec;
    struct _krb5_key_type *kt;
};

static void
_krb5_cc_schedule(krb5_context context,
		  struct _krb5_key_type *kt,
		  struct _krb5_key_data *kd)
{
    struct cc_schedule *key = kd->schedule->data;
    CCAlgorithm alg = (CCAlgorithm)kt->evp;

    key->kt = kt;

    if (CCCryptorCreateWithMode(kCCEncrypt,
				kCCModeCBC,
				alg,
				ccCBCCTS3,
				NULL,
				kd->key->keyvalue.data,
				kd->key->keyvalue.length,
				NULL,
				0,
				0,
				0,
				&key->enc) != 0)
	abort();

    if (CCCryptorCreateWithMode(kCCDecrypt,
				kCCModeCBC,
				alg,
				ccCBCCTS3,
				NULL,
				kd->key->keyvalue.data,
				kd->key->keyvalue.length,
				NULL,
				0,
				0,
				0,
				&key->dec) != 0)
	abort();
}

static void
_krb5_cc_cleanup(krb5_context context, struct _krb5_key_data *kd)
{
    struct cc_schedule *key = kd->schedule->data;
    CCCryptorRelease(key->enc);
    CCCryptorRelease(key->dec);
}

static struct _krb5_key_type keytype_aes128 = {
    ENCTYPE_AES128_CTS_HMAC_SHA1_96,
    "aes-128",
    128,
    16,
    sizeof(struct cc_schedule),
    NULL,
    _krb5_cc_schedule,
    _krb5_AES_salt,
    NULL,
    _krb5_cc_cleanup,
    (void *)kCCAlgorithmAES128
};

static struct _krb5_key_type keytype_aes256 = {
    ENCTYPE_AES256_CTS_HMAC_SHA1_96,
    "aes-256",
    256,
    32,
    sizeof(struct cc_schedule),
    NULL,
    _krb5_cc_schedule,
    _krb5_AES_salt,
    NULL,
    _krb5_cc_cleanup,
    (void *)kCCAlgorithmAES128
};

struct _krb5_checksum_type _krb5_checksum_hmac_sha1_aes128 = {
    CKSUMTYPE_HMAC_SHA1_96_AES_128,
    "hmac-sha1-96-aes128",
    64,
    12,
    F_KEYED | F_CPROOF | F_DERIVED,
    _krb5_SP_HMAC_SHA1_checksum,
    NULL
};

struct _krb5_checksum_type _krb5_checksum_hmac_sha1_aes256 = {
    CKSUMTYPE_HMAC_SHA1_96_AES_256,
    "hmac-sha1-96-aes256",
    64,
    12,
    F_KEYED | F_CPROOF | F_DERIVED,
    _krb5_SP_HMAC_SHA1_checksum,
    NULL
};

static krb5_error_code
cc_AES_PRF(krb5_context context,
	   krb5_crypto crypto,
	   const krb5_data *in,
	   krb5_data *out)
{
    struct _krb5_checksum_type *ct = crypto->et->checksum;
    krb5_error_code ret;
    Checksum result;
    krb5_keyblock *derived;

    result.cksumtype = ct->type;
    ret = krb5_data_alloc(&result.checksum, ct->checksumsize);
    if (ret) {
	krb5_set_error_message(context, ret, N_("malloc: out memory", ""));
	return ret;
    }

    ret = (*ct->checksum)(context, NULL, in->data, in->length, 0, &result);
    if (ret) {
	krb5_data_free(&result.checksum);
	return ret;
    }

    if (result.checksum.length < crypto->et->blocksize)
	krb5_abortx(context, "internal prf error");

    derived = NULL;
    ret = krb5_derive_key(context, crypto->key.key,
			  crypto->et->type, "prf", 3, &derived);
    if (ret)
	krb5_abortx(context, "krb5_derive_key");

    ret = krb5_data_alloc(out, crypto->et->blocksize);
    if (ret)
	krb5_abortx(context, "malloc failed");

    {
	CCAlgorithm alg = (CCAlgorithm)crypto->et->keytype->evp;
	CCCryptorStatus s;
	size_t moved;
	
	s = CCCrypt(kCCEncrypt,
		    alg,
		    0,
		    derived->keyvalue.data,
		    crypto->et->keytype->size,
		    NULL,
		    result.checksum.data,
		    crypto->et->blocksize,
		    out->data,
		    crypto->et->blocksize,
		    &moved);
	if (s)
	    krb5_abortx(context, "encrypt failed");
	if (moved != crypto->et->blocksize)
	    krb5_abortx(context, "encrypt failed");

    }

    krb5_data_free(&result.checksum);
    krb5_free_keyblock(context, derived);

    return ret;
}

static const unsigned char zero_ivec[EVP_MAX_BLOCK_LENGTH] = { 0 };

static krb5_error_code
_krb5_cc_encrypt_cts(krb5_context context,
		     struct _krb5_key_data *key,
		     void *data,
		     size_t len,
		     krb5_boolean encryptp,
		     int usage,
		     void *ivec)
{
    struct cc_schedule *ctx = key->schedule->data;
    size_t blocksize;
    unsigned char *p, *p0;
    size_t moved, plen = len;
    CCCryptorStatus s;
    CCCryptorRef c;

    c = encryptp ? ctx->enc : ctx->dec;

    p0 = p = malloc(len);
    if (p0 == NULL)
	return ENOMEM;

    blocksize = 16; /* XXX only for aes now */

    if (len < blocksize) {
	krb5_set_error_message(context, EINVAL,
			       "message block too short");
	return EINVAL;
    } else if (len == blocksize) {
	struct _krb5_key_type *kt = ctx->kt;
	CCAlgorithm alg = (CCAlgorithm)kt->evp;
	
	s = CCCrypt(encryptp ? kCCEncrypt : kCCDecrypt,
		    alg,
		    0,
		    key->key->keyvalue.data,
		    key->key->keyvalue.length,
		    NULL,
		    data,
		    len,
		    data,
		    len,
		    &moved);
	heim_assert(s == 0, "CCCrypt failed");
	heim_assert(moved == len, "moved == len");

	return 0;
    }

    if (ivec)
	CCCryptorReset(c, ivec);
    else
	CCCryptorReset(c, zero_ivec);
    
    s = CCCryptorUpdate(c, data, len, p, plen, &moved);
    heim_assert(s == 0, "CCCryptorUpdate failed");
    plen -= moved;
    p += moved;
    s = CCCryptorFinal(c, p, plen, &moved);
    heim_assert(s == 0, "CCCryptorFinal failed");
    plen -= moved;
    heim_assert(plen == 0, "plen == 0");

    memcpy(data, p0, len);
    free(p0);

    return 0;
}


struct _krb5_encryption_type _krb5_enctype_aes128_cts_hmac_sha1 = {
    ETYPE_AES128_CTS_HMAC_SHA1_96,
    "aes128-cts-hmac-sha1-96",
    16,
    1,
    16,
    &keytype_aes128,
    &_krb5_checksum_sha1,
    &_krb5_checksum_hmac_sha1_aes128,
    F_DERIVED,
    _krb5_cc_encrypt_cts,
    16,
    cc_AES_PRF
};

struct _krb5_encryption_type _krb5_enctype_aes256_cts_hmac_sha1 = {
    ETYPE_AES256_CTS_HMAC_SHA1_96,
    "aes256-cts-hmac-sha1-96",
    16,
    1,
    16,
    &keytype_aes256,
    &_krb5_checksum_sha1,
    &_krb5_checksum_hmac_sha1_aes256,
    F_DERIVED,
    _krb5_cc_encrypt_cts,
    16,
    cc_AES_PRF
};

#else

/*
 * EVP based
 */

static struct _krb5_key_type keytype_aes128 = {
    KRB5_ENCTYPE_AES128_CTS_HMAC_SHA1_96,
    "aes-128",
    128,
    16,
    sizeof(struct _krb5_evp_schedule),
    NULL,
    _krb5_evp_schedule,
    _krb5_AES_salt,
    NULL,
    _krb5_evp_cleanup,
    EVP_aes_128_cbc
};

static struct _krb5_key_type keytype_aes256 = {
    KRB5_ENCTYPE_AES256_CTS_HMAC_SHA1_96,
    "aes-256",
    256,
    32,
    sizeof(struct _krb5_evp_schedule),
    NULL,
    _krb5_evp_schedule,
    _krb5_AES_salt,
    NULL,
    _krb5_evp_cleanup,
    EVP_aes_256_cbc
};

struct _krb5_checksum_type _krb5_checksum_hmac_sha1_aes128 = {
    CKSUMTYPE_HMAC_SHA1_96_AES_128,
    "hmac-sha1-96-aes128",
    64,
    12,
    F_KEYED | F_CPROOF | F_DERIVED,
    _krb5_SP_HMAC_SHA1_checksum,
    NULL
};

struct _krb5_checksum_type _krb5_checksum_hmac_sha1_aes256 = {
    CKSUMTYPE_HMAC_SHA1_96_AES_256,
    "hmac-sha1-96-aes256",
    64,
    12,
    F_KEYED | F_CPROOF | F_DERIVED,
    _krb5_SP_HMAC_SHA1_checksum,
    NULL
};

static krb5_error_code
AES_PRF(krb5_context context,
	krb5_crypto crypto,
	const krb5_data *in,
	krb5_data *out)
{
    struct _krb5_checksum_type *ct = crypto->et->checksum;
    krb5_error_code ret;
    Checksum result;
    krb5_keyblock *derived;

    result.cksumtype = ct->type;
    ret = krb5_data_alloc(&result.checksum, ct->checksumsize);
    if (ret) {
	krb5_set_error_message(context, ret, N_("malloc: out memory", ""));
	return ret;
    }

    ret = (*ct->checksum)(context, NULL, in->data, in->length, 0, &result);
    if (ret) {
	krb5_data_free(&result.checksum);
	return ret;
    }

    if (result.checksum.length < crypto->et->blocksize)
	krb5_abortx(context, "internal prf error");

    derived = NULL;
    ret = krb5_derive_key(context, crypto->key.key,
			  crypto->et->type, "prf", 3, &derived);
    if (ret)
	krb5_abortx(context, "krb5_derive_key");

    ret = krb5_data_alloc(out, crypto->et->blocksize);
    if (ret)
	krb5_abortx(context, "malloc failed");

    {
	const EVP_CIPHER *c = (*crypto->et->keytype->evp)();
	EVP_CIPHER_CTX ctx;

	EVP_CIPHER_CTX_init(&ctx); /* ivec all zero */
	EVP_CipherInit_ex(&ctx, c, NULL, derived->keyvalue.data, NULL, 1);
	EVP_Cipher(&ctx, out->data, result.checksum.data,
		   crypto->et->blocksize);
	EVP_CIPHER_CTX_cleanup(&ctx);
    }

    krb5_data_free(&result.checksum);
    krb5_free_keyblock(context, derived);

    return ret;
}

struct _krb5_encryption_type _krb5_enctype_aes128_cts_hmac_sha1 = {
    ETYPE_AES128_CTS_HMAC_SHA1_96,
    "aes128-cts-hmac-sha1-96",
    16,
    1,
    16,
    &keytype_aes128,
    &_krb5_checksum_sha1,
    &_krb5_checksum_hmac_sha1_aes128,
    F_DERIVED,
    _krb5_evp_encrypt_cts,
    16,
    AES_PRF
};

struct _krb5_encryption_type _krb5_enctype_aes256_cts_hmac_sha1 = {
    ETYPE_AES256_CTS_HMAC_SHA1_96,
    "aes256-cts-hmac-sha1-96",
    16,
    1,
    16,
    &keytype_aes256,
    &_krb5_checksum_sha1,
    &_krb5_checksum_hmac_sha1_aes256,
    F_DERIVED,
    _krb5_evp_encrypt_cts,
    16,
    AES_PRF
};

#endif

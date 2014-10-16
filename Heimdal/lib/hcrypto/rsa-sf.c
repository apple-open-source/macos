/*
 * Copyright (c) 2006 - 2007 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 *
 * Portions Copyright (c) 2010 Apple Inc. All rights reserved.
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

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <krb5-types.h>
#include <assert.h>

#include <rsa.h>

#ifdef HEIM_HC_SF

#include <Security/Security.h> 
#include <Security/SecRSAKey.h> 

#include <rfc2459_asn1.h>

#include "common.h"

/*
 *
 */

static SecKeyRef
CreateKeyFromRSA(RSA *rsa, int use_public)
{
    SecKeyRef (*CreateMethod)(CFAllocatorRef, const uint8_t *, CFIndex, SecKeyEncoding) = NULL;
    size_t size = 0, keylength;
    void *keydata;
    int ret;
	
    if (use_public) {
	RSAPublicKey k;

	CreateMethod = SecKeyCreateRSAPublicKey;

	memset(&k, 0, sizeof(k));

	ret = _hc_BN_to_integer(rsa->n, &k.modulus);
	if (ret == 0)
	    ret = _hc_BN_to_integer(rsa->e, &k.publicExponent);
	if (ret) {
	    free_RSAPublicKey(&k);
	    return NULL;
	}

	ASN1_MALLOC_ENCODE(RSAPublicKey, keydata, keylength, &k, &size, ret);
	free_RSAPublicKey(&k);
	if (ret)
	    return NULL;
	if (size != keylength)
	    abort();

    } else {
	RSAPrivateKey k;

	CreateMethod = SecKeyCreateRSAPrivateKey;

	memset(&k, 0, sizeof(k));

	k.version = 1;
	ret = _hc_BN_to_integer(rsa->n, &k.modulus);
	if (ret == 0)
	    ret = _hc_BN_to_integer(rsa->e, &k.publicExponent);
	if (ret == 0)
	    ret = _hc_BN_to_integer(rsa->d, &k.privateExponent);
	if (ret == 0)
	    ret = _hc_BN_to_integer(rsa->p, &k.prime1);
	if (ret == 0)
	    ret = _hc_BN_to_integer(rsa->q, &k.prime2);
	if (ret == 0)
	    ret = _hc_BN_to_integer(rsa->dmp1, &k.exponent1);
	if (ret == 0)
	    ret = _hc_BN_to_integer(rsa->dmq1, &k.exponent2);
	if (ret == 0)
	    ret = _hc_BN_to_integer(rsa->iqmp, &k.coefficient);
	if (ret) {
	    free_RSAPrivateKey(&k);
	    return NULL;
	}

	ASN1_MALLOC_ENCODE(RSAPrivateKey, keydata, keylength, &k, &size, ret);
	free_RSAPrivateKey(&k);
	if (ret)
	    return NULL;
	if (size != keylength)
	    abort();
    }

    SecKeyRef key = CreateMethod(NULL, keydata, keylength, kSecKeyEncodingPkcs1);
    free(keydata);
    return key;
}

/*
 *
 */

static int
sf_rsa_public_encrypt(int flen, const unsigned char* from,
		      unsigned char* to, RSA* rsa, int padding)
{
    SecKeyRef key = CreateKeyFromRSA(rsa, 1);
    OSStatus status;
    size_t tlen = RSA_size(rsa);

    if (key == NULL)
	return -1;

    if (padding != RSA_PKCS1_PADDING)
	return -1;

    status = SecKeyEncrypt(key, kSecPaddingPKCS1, from, flen, to, &tlen);
    CFRelease(key);
    if (status)
	return -1;
    if (tlen > (size_t)RSA_size(rsa))
	abort();
    return tlen;
}

static int
sf_rsa_public_decrypt(int flen, const unsigned char* from,
		      unsigned char* to, RSA* rsa, int padding)
{
    SecKeyRef key = CreateKeyFromRSA(rsa, 1);
    OSStatus status;
    size_t tlen = RSA_size(rsa);

    if (key == NULL)
	return -1;

    if (padding != RSA_PKCS1_PADDING)
	return -1;

    /* SecKeyDecrypt gets decrytion wrong for public keys in the PKCS1 case (14322412), lets do PKCS1 (un)padding ourself */
    status = SecKeyDecrypt(key, kSecPaddingNone, from, flen, to, &tlen);
    CFRelease(key);
    if (status)
	return -1;
    if (tlen > (size_t)RSA_size(rsa))
	abort();

    unsigned char *p = to;

    if (tlen < 1)
	return -1;
    if (*p != 1)
	return -1;
    tlen--; p++;
    while (tlen && *p == 0xff) {
	tlen--; p++;
    }
    if (tlen == 0 || *p != 0)
	return -1;
    tlen--; p++;

    memmove(to, p, tlen);

    return tlen;
}

static int
sf_rsa_private_encrypt(int flen, const unsigned char* from,
		       unsigned char* to, RSA* rsa, int padding)
{
    SecKeyRef key = CreateKeyFromRSA(rsa, 0);
    OSStatus status;
    size_t tlen = RSA_size(rsa);

    if (key == NULL)
	return -1;

    if (padding != RSA_PKCS1_PADDING)
	return -1;

    status = SecKeyEncrypt(key, kSecPaddingPKCS1, from, flen, to, &tlen);
    CFRelease(key);
    if (status)
	return -1;
    if (tlen > (size_t)RSA_size(rsa))
	abort();
    return tlen;
}

static int
sf_rsa_private_decrypt(int flen, const unsigned char* from,
		       unsigned char* to, RSA* rsa, int padding)
{
    SecKeyRef key = CreateKeyFromRSA(rsa, 0);
    OSStatus status;
    size_t tlen = RSA_size(rsa);

    if (key == NULL)
	return -1;

    if (padding != RSA_PKCS1_PADDING)
	return -1;

    /* SecKeyDecrypt gets kSecPaddingPKCS1 wrong (14322412), lets inline pkcs1 padding here ourself */

    status = SecKeyDecrypt(key, kSecPaddingNone, from, flen, to, &tlen);
    CFRelease(key);
    if (status)
	return -1;
    if (tlen > (size_t)RSA_size(rsa))
	abort();

    unsigned char *p = to;

    if (tlen < 1) return -1;

    if (*p != 1)
	return -1;
    tlen--; p++;
    while (tlen && *p == 0xff) {
	tlen--; p++;
    }
    if (tlen == 0 || *p != 0)
	return -1;
    tlen--; p++;

    memmove(to, p, tlen);

    return tlen;
}


static int
sf_rsa_generate_key(RSA *rsa, int bits, BIGNUM *e, BN_GENCB *cb)
{
    return -1;
}

static int
sf_rsa_init(RSA *rsa)
{
    return 1;
}

static int
sf_rsa_finish(RSA *rsa)
{
    return 1;
}

const RSA_METHOD _hc_rsa_sf_method = {
    "hcrypto sf RSA",
    sf_rsa_public_encrypt,
    sf_rsa_public_decrypt,
    sf_rsa_private_encrypt,
    sf_rsa_private_decrypt,
    NULL,
    NULL,
    sf_rsa_init,
    sf_rsa_finish,
    0,
    NULL,
    NULL,
    NULL,
    sf_rsa_generate_key
};
#endif

const RSA_METHOD *
RSA_sf_method(void)
{
#ifdef HEIM_HC_SF
    return &_hc_rsa_sf_method;
#else
    return NULL;
#endif
}

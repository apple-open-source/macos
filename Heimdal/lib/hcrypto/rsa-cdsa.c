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

#ifdef HAVE_CDSA
#define NEED_CDSA 1

#include <rfc2459_asn1.h>

#include "common.h"

#include <roken.h>

/*
 *
 */


static int
load_key(CSSM_CSP_HANDLE cspHandle, RSA *rsa, int use_public, CSSM_KEY_PTR key, size_t *keysize)
{
    CSSM_KEY_SIZE keySize;
    CSSM_RETURN ret;
    size_t size;
	
    memset(key, 0, sizeof(*key));

    if (use_public) {
	RSAPublicKey k;

	memset(&k, 0, sizeof(k));

	ret = _hc_BN_to_integer(rsa->n, &k.modulus);
	if (ret == 0)
	    ret = _hc_BN_to_integer(rsa->e, &k.publicExponent);
	if (ret) {
	    free_RSAPublicKey(&k);
	    return 0;
	}

	ASN1_MALLOC_ENCODE(RSAPublicKey, key->KeyData.Data, key->KeyData.Length,
			   &k, &size, ret);
	free_RSAPublicKey(&k);
	if (ret)
	    return 1;
	if (size != key->KeyData.Length)
	    abort();

    } else {
	RSAPrivateKey k;

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
	    return 1;
	}

	ASN1_MALLOC_ENCODE(RSAPrivateKey, key->KeyData.Data, key->KeyData.Length,
			   &k, &size, ret);
	free_RSAPrivateKey(&k);
	if (ret)
	    return 1;
	if (size != key->KeyData.Length)
	    abort();
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
    if(ret)
	return 1;

    key->KeyHeader.LogicalKeySizeInBits = keySize.LogicalKeySizeInBits;

    *keysize = (keySize.LogicalKeySizeInBits + 7) / 8;

    return 0;
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
perform_rsa_op(int flen, const unsigned char* from,
	       unsigned char* to, RSA* rsa, int padding,
	       CSSM_ENCRYPT_MODE algMode, op func)
{
    CSSM_CSP_HANDLE cspHandle = _hc_get_cdsa_csphandle();
    CSSM_RETURN cret;
    CSSM_ACCESS_CREDENTIALS creds;
    CSSM_KEY cssmKey;
    CSSM_CC_HANDLE handle = 0;
    CSSM_DATA out, in, rem;
    int fret = 0;
    CSSM_SIZE outlen = 0;
    char remdata[1024];
    size_t keysize;

    if (padding != RSA_PKCS1_PADDING)
	return -1;

    memset(&creds, 0, sizeof(creds));

    fret = load_key(cspHandle, rsa, (algMode == CSSM_ALGMODE_PUBLIC_KEY),
		    &cssmKey, &keysize);
    if (fret)
	return -2;

    fret = CSSM_CSP_CreateAsymmetricContext(cspHandle,
					    CSSM_ALGID_RSA,
					    &creds,
					    &cssmKey,
					    CSSM_PADDING_PKCS1,
					    &handle);
    if(fret) abort();

    {
	CSSM_CONTEXT_ATTRIBUTE attr;	
	
	attr.AttributeType = CSSM_ATTRIBUTE_MODE;
	attr.AttributeLength = sizeof(attr.Attribute.Uint32);
	attr.Attribute.Uint32 = algMode;

	fret = CSSM_UpdateContextAttributes(handle, 1, &attr);
	if (fret) abort();
    }

    in.Data = (uint8 *)from;
    in.Length = flen;

    out.Data = (uint8 *)to;
    out.Length = keysize;

    rem.Data = (uint8 *)remdata;
    rem.Length = sizeof(remdata);

    cret = func(handle, &in, 1, &out, 1, &outlen, &rem);
    if(cret) {
	/* cssmErrorString(cret); */
	fret = -1;
    } else
	fret = outlen;

    if(handle)
	CSSM_DeleteContext(handle);
    unload_key(&cssmKey);

    return fret;
}

/*
 *
 */


static int
cdsa_rsa_public_encrypt(int flen, const unsigned char* from,
			unsigned char* to, RSA* rsa, int padding)
{
    return perform_rsa_op(flen, from, to, rsa, padding, CSSM_ALGMODE_PUBLIC_KEY, CSSM_EncryptData);
}

static int
cdsa_rsa_public_decrypt(int flen, const unsigned char* from,
			 unsigned char* to, RSA* rsa, int padding)
{
    return perform_rsa_op(flen, from, to, rsa, padding, CSSM_ALGMODE_PUBLIC_KEY, CSSM_DecryptData);
}

static int
cdsa_rsa_private_encrypt(int flen, const unsigned char* from,
			  unsigned char* to, RSA* rsa, int padding)
{
    return perform_rsa_op(flen, from, to, rsa, padding, CSSM_ALGMODE_PRIVATE_KEY, CSSM_EncryptData);
}

static int
cdsa_rsa_private_decrypt(int flen, const unsigned char* from,
			  unsigned char* to, RSA* rsa, int padding)
{
    return perform_rsa_op(flen, from, to, rsa, padding, CSSM_ALGMODE_PRIVATE_KEY, CSSM_DecryptData);
}


static int
cdsa_rsa_generate_key(RSA *rsa, int bits, BIGNUM *e, BN_GENCB *cb)
{
    CSSM_CSP_HANDLE cspHandle = _hc_get_cdsa_csphandle();
    uint32_t pubAttr = CSSM_KEYATTR_EXTRACTABLE | CSSM_KEYATTR_RETURN_DATA;
    uint32_t privAttr = CSSM_KEYATTR_EXTRACTABLE | CSSM_KEYATTR_RETURN_DATA;
    CSSM_CC_HANDLE handle;
    CSSM_RETURN ret;
    size_t len;
    const unsigned char *p;
    CSSM_KEY priv_key, pub_key;
    AlgorithmIdentifier ai;

    memset(&priv_key, 0, sizeof(priv_key));
    memset(&pub_key, 0, sizeof(pub_key));
    memset(&ai, 0, sizeof(ai));

    ret = CSSM_CSP_CreateKeyGenContext(cspHandle,
				       CSSM_ALGID_RSA,
				       bits,
				       NULL,
				       NULL,
				       NULL,
				       NULL,
				       NULL,
				       &handle);
    if (ret)
	return 0;

    {
	CSSM_CONTEXT_ATTRIBUTE attr;	
	
	attr.AttributeType = CSSM_ATTRIBUTE_PRIVATE_KEY_FORMAT;
	attr.AttributeLength = sizeof(attr.Attribute.Uint32);
	attr.Attribute.Uint32 = CSSM_KEYBLOB_RAW_FORMAT_PKCS1;

	ret = CSSM_UpdateContextAttributes(handle, 1, &attr);
	if (ret) abort();

	attr.AttributeType = CSSM_ATTRIBUTE_PUBLIC_KEY_FORMAT;
	ret = CSSM_UpdateContextAttributes(handle, 1, &attr);
	if (ret) abort();
    }

    ret = CSSM_GenerateKeyPair(handle,
			       /* pubkey */
			       CSSM_KEYUSE_DERIVE,
			       pubAttr,
			       &_hc_labelData,
			       &pub_key,
			       /* private key */
			       CSSM_KEYUSE_DERIVE,
			       privAttr,
			       &_hc_labelData,
			       NULL,
			       &priv_key);

    CSSM_DeleteContext(handle);
    if (ret)
	return 0;

    ret = decode_AlgorithmIdentifier(priv_key.KeyData.Data,
				     priv_key.KeyData.Length, &ai, NULL);
    if (ret || ai.parameters == NULL) {
	p = priv_key.KeyData.Data;
	len = priv_key.KeyData.Length;
    } else {
	p = ai.parameters->data;
	len = ai.parameters->length;
    }

    ret = (d2i_RSAPrivateKey(rsa, &p, len) == NULL) ? 0 : 1;
    free_AlgorithmIdentifier(&ai);

    CSSM_FreeKey(cspHandle, NULL, &pub_key, CSSM_FALSE);
    CSSM_FreeKey(cspHandle, NULL, &priv_key, CSSM_FALSE);

    return ret;
}

static int
cdsa_rsa_init(RSA *rsa)
{
    return 1;
}

static int
cdsa_rsa_finish(RSA *rsa)
{
    return 1;
}

const RSA_METHOD _hc_rsa_cdsa_method = {
    "hcrypto cdsa RSA",
    cdsa_rsa_public_encrypt,
    cdsa_rsa_public_decrypt,
    cdsa_rsa_private_encrypt,
    cdsa_rsa_private_decrypt,
    NULL,
    NULL,
    cdsa_rsa_init,
    cdsa_rsa_finish,
    0,
    NULL,
    NULL,
    NULL,
    cdsa_rsa_generate_key
};
#endif

const RSA_METHOD *
RSA_cdsa_method(void)
{
#ifdef HAVE_CDSA
    return &_hc_rsa_cdsa_method;
#else
    return NULL;
#endif
}

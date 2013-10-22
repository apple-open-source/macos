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

/* #include "rk-roken.h" */

#ifdef HAVE_CDSA

/*
 *
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


/*
 *
 */
static int
cdsa_rsa_public_encrypt(int flen, const unsigned char *from,
    unsigned char *to, RSA *rsa, int padding)
{
	return (perform_rsa_op(flen, from, to, rsa, padding, CSSM_ALGMODE_PUBLIC_KEY, CSSM_EncryptData));
}


static int
cdsa_rsa_public_decrypt(int flen, const unsigned char *from,
    unsigned char *to, RSA *rsa, int padding)
{
	return (perform_rsa_op(flen, from, to, rsa, padding, CSSM_ALGMODE_PUBLIC_KEY, CSSM_DecryptData));
}


static int
cdsa_rsa_private_encrypt(int flen, const unsigned char *from,
    unsigned char *to, RSA *rsa, int padding)
{
	return (perform_rsa_op(flen, from, to, rsa, padding, CSSM_ALGMODE_PRIVATE_KEY, CSSM_EncryptData));
}


static int
cdsa_rsa_private_decrypt(int flen, const unsigned char *from,
    unsigned char *to, RSA *rsa, int padding)
{
	return (perform_rsa_op(flen, from, to, rsa, padding, CSSM_ALGMODE_PRIVATE_KEY, CSSM_DecryptData));
}


static int
cdsa_dsa_generate_key(DSA *rsa, int bits, BIGNUM *e, BN_GENCB *cb)
{
	CSSM_CSP_HANDLE cspHandle = _cs_get_cdsa_csphandle();
	uint32_t pubAttr = CSSM_KEYATTR_EXTRACTABLE | CSSM_KEYATTR_RETURN_DATA;
	uint32_t privAttr = CSSM_KEYATTR_EXTRACTABLE | CSSM_KEYATTR_RETURN_DATA;
	CSSM_CC_HANDLE handle = 0;
	CSSM_RETURN ret;
	size_t len;
	const unsigned char *p;
	CSSM_KEY priv_key, pub_key;
	AlgorithmIdentifier ai;

	memset(&priv_key, 0, sizeof(priv_key));
	memset(&pub_key, 0, sizeof(pub_key));
	memset(&ai, 0, sizeof(ai));

	ret = CSSM_CSP_CreateKeyGenContext(cspHandle,
		CSSM_ALGID_DSA,
		bits,
		NULL,                           /* Seed */
		NULL,                           /* Salt */
		NULL,                           /* StartDate */
		NULL,                           /* EndDate */
		NULL,                           /* Params */
		&handle);
	if (ret) {
		return (0);
	}

	{
		CSSM_CONTEXT_ATTRIBUTE attr;

		/* optional format specifiers */
		attr.AttributeType = CSSM_ATTRIBUTE_PRIVATE_KEY_FORMAT;
		attr.AttributeLength = sizeof(attr.Attribute.Uint32);
		attr.Attribute.Uint32 = CSSM_KEYBLOB_RAW_FORMAT_PKCS1;

		ret = CSSM_UpdateContextAttributes(handle, 1, &attr);
		if (ret) {
			abort();
		}

		attr.AttributeType = CSSM_ATTRIBUTE_PUBLIC_KEY_FORMAT;
		ret = CSSM_UpdateContextAttributes(handle, 1, &attr);
		if (ret) {
			abort();
		}
	}

	ret = CSSM_GenerateKeyPair(handle,
	        /* pubkey */
		CSSM_KEYUSE_DERIVE,
		pubAttr,
		&_cs_labelData,
		&pub_key,

	        /* private key */
		CSSM_KEYUSE_DERIVE,
		privAttr,
		&_cs_labelData,
		NULL,                           /* CredAndAclEntry */
		&priv_key);

	CSSM_DeleteContext(handle);
	if (ret) {
		return (0);
	}

	ret = decode_AlgorithmIdentifier(priv_key.KeyData.Data,
		priv_key.KeyData.Length, &ai, NULL);
	if (ret || (ai.parameters == NULL)) {
		p = priv_key.KeyData.Data;
		len = priv_key.KeyData.Length;
	} else {
		p = ai.parameters->data;
		len = ai.parameters->length;
	}

	ret = (d2i_DSAPrivateKey(dsa, &p, len) == NULL) ? 0 : 1;
	free_AlgorithmIdentifier(&ai);

	CSSM_FreeKey(cspHandle, NULL, &pub_key, CSSM_FALSE);
	CSSM_FreeKey(cspHandle, NULL, &priv_key, CSSM_FALSE);

	return (ret);
}


#endif /* #if 0 */

static DSA_SIG *
cdsa_dsa_do_sign(const unsigned char *dgst, int dlen, DSA *dsa)
{
	return (NULL);
}


static int
cdsa_dsa_sign_setup(DSA *dsa, BN_CTX *ctx_in, BIGNUM **kinvp, BIGNUM **rp)
{
	return (1);
}


static int
cdsa_dsa_do_verify(const unsigned char *dgst, int dgst_len,
    DSA_SIG *sig, DSA *dsa)
{
	return (1);
}


static int
cdsa_dsa_init(DSA *dsa)
{
	return (1);
}


static int
cdsa_dsa_finish(DSA *dsa)
{
	return (1);
}


static int
cdsa_dsa_paramgen(DSA *dsa, int bits, unsigned char *seed, int seed_len,
    int *counter_ret, unsigned long *h_ret, BN_GENCB *cb)
{
	return (1);
}


static int
cdsa_dsa_keygen(DSA *dsa)
{
	return (1);
}


const DSA_METHOD _ossl_dsa_cdsa_method =
{
	.name		= "cryptoshim cdsa DSA",
	.dsa_do_sign	= cdsa_dsa_do_sign,
	.dsa_sign_setup = cdsa_dsa_sign_setup,
	.dsa_do_verify	= cdsa_dsa_do_verify,
	.init		= cdsa_dsa_init,
	.finish		= cdsa_dsa_finish,
	0,
	NULL,
	.dsa_paramgen	= cdsa_dsa_paramgen,
	.dsa_keygen	= cdsa_dsa_keygen
};
#endif /* HAVE_CDSA */

const DSA_METHOD *
DSA_cdsa_method(void)
{
#ifdef HAVE_CDSA
	return (&_ossl_dsa_cdsa_method);

#else
	return (NULL);
#endif
}

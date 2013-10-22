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
 * Copyright (c) 2006 Kungliga Tekniska Högskolan
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

#include "ossl-config.h"

#ifdef HAVE_CDSA

#include <stdio.h>
#include <stdlib.h>

#include "ossl-common.h"
#include "ossl-dh.h"

#include "rfc2459_asn1.h"



struct dh_cdsa {
	CSSM_KEY	priv_key;
	CSSM_KEY	pub_key;
};

/*
 * shims for CSSM/CDSA DH
 */
static int
dh_generate_key(DH *dh)
{
	CSSM_CSP_HANDLE cspHandle = _cs_get_cdsa_csphandle();
	struct dh_cdsa *cdsa = DH_get_ex_data(dh, 0);
	uint32_t pubAttr = CSSM_KEYATTR_EXTRACTABLE | CSSM_KEYATTR_RETURN_DATA;
	uint32_t privAttr = CSSM_KEYATTR_RETURN_REF;
	DHParameter dp;
	CSSM_CC_HANDLE handle;
	CSSM_DATA param;
	CSSM_RETURN ret;
	size_t size;

	if ((dh->p == NULL) || (dh->g == NULL)) {
		return (0);
	}

	CSSM_FreeKey(cspHandle, NULL, &cdsa->pub_key, CSSM_FALSE);
	CSSM_FreeKey(cspHandle, NULL, &cdsa->priv_key, CSSM_FALSE);

	memset(&dp, 0, sizeof(dp));

	ret = _cs_BN_to_integer(dh->p, &dp.prime);
	if (ret == 0) {
		ret = _cs_BN_to_integer(dh->g, &dp.base);
	}
	if (ret) {
		free_DHParameter(&dp);
		return (0);
	}
	dp.privateValueLength = NULL;

	ASN1_MALLOC_ENCODE(DHParameter, param.Data, param.Length,
	    &dp, &size, ret);
	free_DHParameter(&dp);
	if (ret) {
		return (0);
	}
	if (size != param.Length) {
		abort();
	}

	ret = CSSM_CSP_CreateKeyGenContext(cspHandle,
		CSSM_ALGID_DH,
		BN_num_bits(dh->p) - 1,
		NULL,
		NULL,
		NULL,
		NULL,
		&param,
		&handle);
	free(param.Data);
	if (ret) {
		return (0);
	}

	ret = CSSM_GenerateKeyPair(handle,
	        /* pubkey */
		CSSM_KEYUSE_DERIVE,
		pubAttr,
		&_cs_labelData,
		&cdsa->pub_key,
	        /* private key */
		CSSM_KEYUSE_DERIVE,
		privAttr,
		&_cs_labelData,
		NULL,
		&cdsa->priv_key);

	CSSM_DeleteContext(handle);
	if (ret) {
		return (0);
	}

	dh->pub_key = BN_bin2bn(cdsa->pub_key.KeyData.Data, cdsa->pub_key.KeyData.Length, NULL);
	if (dh->pub_key == NULL) {
		return (0);
	}

	dh->priv_key = BN_bin2bn(cdsa->priv_key.KeyData.Data, cdsa->priv_key.KeyData.Length, NULL);
	if (dh->priv_key == NULL) {
		return (0);
	}

	return (1);
}


static int
dh_compute_key(unsigned char *shared, const BIGNUM *pub, DH *dh)
{
	CSSM_CSP_HANDLE cspHandle = _cs_get_cdsa_csphandle();
	struct dh_cdsa *cdsa = DH_get_ex_data(dh, 0);
	CSSM_ACCESS_CREDENTIALS creds;
	CSSM_CC_HANDLE handle;
	CSSM_KEY derivedKey;
	CSSM_DATA param;
	CSSM_RETURN ret;
	int sharedlen = -1;

	memset(&creds, 0, sizeof(creds));
	memset(&derivedKey, 0, sizeof(derivedKey));

	ret = CSSM_CSP_CreateDeriveKeyContext(cspHandle,
		CSSM_ALGID_DH,
		CSSM_ALGID_RC5,                                 /* will give us plenty of bits */
		DH_size(dh) * 8,                                /* size in bits */
		&creds,
		&cdsa->priv_key,
		0,
		0,
		0,
		&handle);
	if (ret) {
		return (-1);
	}

	param.Length = BN_num_bytes(pub);
	param.Data = malloc(param.Length);
	if (param.Data == NULL) {
		CSSM_DeleteContext(handle);
		return (-1);
	}
	BN_bn2bin(pub, param.Data);

	ret = CSSM_DeriveKey(handle,
		&param,
		CSSM_KEYUSE_ENCRYPT | CSSM_KEYUSE_DECRYPT,
		CSSM_KEYATTR_RETURN_DATA | CSSM_KEYATTR_EXTRACTABLE,
		&_cs_labelData,
		NULL,
		&derivedKey);
	free(param.Data);
	if (ret) {
		/* cssmPerror("CSSM_DeriveKey", ret); */
		return (-1);
	}

	memcpy(shared, derivedKey.KeyData.Data, derivedKey.KeyData.Length);
	if (derivedKey.KeyData.Length > DH_size(dh)) {
		CSSM_FreeKey(cspHandle, NULL, &derivedKey, CSSM_FALSE);
		CSSM_DeleteContext(handle);
		return (-1);
	}

	sharedlen = derivedKey.KeyData.Length;

	CSSM_FreeKey(cspHandle, NULL, &derivedKey, CSSM_FALSE);
	CSSM_DeleteContext(handle);

	return (sharedlen);
}


static int
dh_generate_params(DH *dh, int a, int b, BN_GENCB *callback)
{
	/* groups should already be known, we don't care about this */
	return (0);
}


static int
dh_init(DH *dh)
{
	struct dh_cdsa *cdsa;

	cdsa = calloc(1, sizeof(*cdsa));
	if (cdsa == NULL) {
		return (0);
	}

	DH_set_ex_data(dh, 0, cdsa);

	return (1);
}


static int
dh_finish(DH *dh)
{
	struct dh_cdsa *cdsa = DH_get_ex_data(dh, 0);

	if (cdsa) {
		CSSM_CSP_HANDLE cspHandle = _cs_get_cdsa_csphandle();

		CSSM_FreeKey(cspHandle, NULL, &cdsa->pub_key, CSSM_FALSE);
		CSSM_FreeKey(cspHandle, NULL, &cdsa->priv_key, CSSM_FALSE);
		free(cdsa);
	}
	return (1);
}


/*
 *
 */

const DH_METHOD _ossl_dh_cdsa_method =
{
	.name = "cryptoshim cdsa DH",
	.generate_key = dh_generate_key,
	.compute_key = dh_compute_key,
	.bn_mod_exp = NULL,
	.init = dh_init,
	.finish = dh_finish,
	.flags = 0,
	.app_data = NULL,
	.generate_params = dh_generate_params
};

#endif /* HAVE_CDSA */

/**
 * DH implementation using cdsa.
 *
 * @return the DH_METHOD for the DH implementation using libcdsa.
 *
 * @ingroup hcrypto_dh
 */
const DH_METHOD *
DH_cdsa_method(void)
{
#ifdef HAVE_CDSA
	return (&_ossl_dh_cdsa_method);

#else
	return (NULL);
#endif  /* HAVE_CDSA */
}

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

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <dh.h>
#include <roken.h>

#ifdef HAVE_CDSA
#define NEED_CDSA 1

#include <rfc2459_asn1.h>

#include "common.h"

struct dh_cdsa {
    CSSM_KEY priv_key;
    CSSM_KEY pub_key;
};

/*
 *
 */

static int
dh_generate_key(DH *dh)
{
    CSSM_CSP_HANDLE cspHandle = _hc_get_cdsa_csphandle();
    struct dh_cdsa *cdsa = DH_get_ex_data(dh, 0);
    uint32_t pubAttr = CSSM_KEYATTR_EXTRACTABLE | CSSM_KEYATTR_RETURN_DATA;
    uint32_t privAttr = CSSM_KEYATTR_RETURN_REF;
    DHParameter dp;
    CSSM_CC_HANDLE handle;
    CSSM_DATA param;
    CSSM_RETURN ret;
    size_t size = 0;

    if (dh->p == NULL || dh->g == NULL)
	return 0;

    CSSM_FreeKey(cspHandle, NULL, &cdsa->pub_key, CSSM_FALSE);
    CSSM_FreeKey(cspHandle, NULL, &cdsa->priv_key, CSSM_FALSE);

    memset(&dp, 0, sizeof(dp));

    ret = _hc_BN_to_integer(dh->p, &dp.prime);
    if (ret == 0)
	ret = _hc_BN_to_integer(dh->g, &dp.base);
    if (ret) {
	free_DHParameter(&dp);
	return 0;
    }
    dp.privateValueLength = NULL;

    ASN1_MALLOC_ENCODE(DHParameter, param.Data, param.Length,
		       &dp, &size, ret);
    free_DHParameter(&dp);
    if (ret)
	return 0;
    if (size != param.Length)
	abort();

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
    if (ret)
	return 0;

    ret = CSSM_GenerateKeyPair(handle,
			       /* pubkey */
			       CSSM_KEYUSE_DERIVE,
			       pubAttr,
			       &_hc_labelData,
			       &cdsa->pub_key,
			       /* private key */
			       CSSM_KEYUSE_DERIVE,
			       privAttr,
			       &_hc_labelData,
			       NULL,
			       &cdsa->priv_key);

    CSSM_DeleteContext(handle);
    if (ret)
	return 0;

    dh->pub_key = BN_bin2bn(cdsa->pub_key.KeyData.Data, cdsa->pub_key.KeyData.Length, NULL);
    if (dh->pub_key == NULL)
	return 0;

    return 1;
}

static int
dh_compute_key(unsigned char *shared, const BIGNUM * pub, DH *dh)
{
    CSSM_CSP_HANDLE cspHandle = _hc_get_cdsa_csphandle();
    struct dh_cdsa *cdsa = DH_get_ex_data(dh, 0);
    CSSM_ACCESS_CREDENTIALS creds;
    CSSM_CC_HANDLE handle;
    CSSM_KEY derivedKey;
    CSSM_DATA param;
    CSSM_RETURN ret;
    int size;

    memset(&creds, 0, sizeof(creds));
    memset(&derivedKey, 0, sizeof(derivedKey));
	
    ret = CSSM_CSP_CreateDeriveKeyContext(cspHandle,
					  CSSM_ALGID_DH,
					  CSSM_ALGID_RC5, /* will give us plenty of bits */
					  DH_size(dh) * 8,
					  &creds,
					  &cdsa->priv_key,
					  0,
					  0,
					  0,
					  &handle);
    if(ret)
	return 0;

    param.Length = BN_num_bytes(pub);
    param.Data = malloc(param.Length);
    if (param.Data == NULL) {
	CSSM_DeleteContext(handle);
	return 0;
    }
    BN_bn2bin(pub, param.Data);

    ret = CSSM_DeriveKey(handle,
			 &param,
			 CSSM_KEYUSE_ENCRYPT | CSSM_KEYUSE_DECRYPT,
			 CSSM_KEYATTR_RETURN_DATA | CSSM_KEYATTR_EXTRACTABLE,
			 &_hc_labelData,
			 NULL,
			 &derivedKey);
    free(param.Data);
    if(ret)
	return 0;
    
    size = derivedKey.KeyData.Length;

    if (size > DH_size(dh)) {
	CSSM_FreeKey(cspHandle, NULL, &derivedKey, CSSM_FALSE);
	CSSM_DeleteContext(handle);
	return 0;
    }

    memcpy(shared, derivedKey.KeyData.Data, size);

    CSSM_FreeKey(cspHandle, NULL, &derivedKey, CSSM_FALSE);
    CSSM_DeleteContext(handle);

    return size;
}

static int
dh_generate_params(DH *dh, int a, int b, BN_GENCB *callback)
{
    /* groups should already be known, we don't care about this */
    return 0;
}

static int
dh_init(DH *dh)
{
    struct dh_cdsa *cdsa;

    cdsa = calloc(1, sizeof(*cdsa));
    if (cdsa == NULL)
	return 0;

    DH_set_ex_data(dh, 0, cdsa);

    return 1;
}

static int
dh_finish(DH *dh)
{
    struct dh_cdsa *cdsa = DH_get_ex_data(dh, 0);
    if (cdsa) {
	CSSM_CSP_HANDLE cspHandle = _hc_get_cdsa_csphandle();

	CSSM_FreeKey(cspHandle, NULL, &cdsa->pub_key, CSSM_FALSE);
	CSSM_FreeKey(cspHandle, NULL, &cdsa->priv_key, CSSM_FALSE);
	free(cdsa);
    }
    return 1;
}


/*
 *
 */

const DH_METHOD _hc_dh_cdsa_method = {
    "hcrypto cdsa DH",
    dh_generate_key,
    dh_compute_key,
    NULL,
    dh_init,
    dh_finish,
    0,
    NULL,
    dh_generate_params
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
    return &_hc_dh_cdsa_method;
#else
    return NULL;
#endif /* HAVE_CDSA */
}

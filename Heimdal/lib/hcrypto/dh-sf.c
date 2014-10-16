/*
 * Copyright (c) 2006 - 2010 Kungliga Tekniska Högskolan
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

#ifdef HEIM_HC_SF

#include <stdio.h>
#include <stdlib.h>
#include <dh.h>

#include <roken.h>

#include <rfc2459_asn1.h>

#include <Security/SecDH.h>

#include "common.h"

struct dh_sf {
    SecDHContext secdh;
};

#define DH_NUM_TRIES 10

static int
sf_dh_generate_key(DH *dh)
{
    struct dh_sf *sf = DH_get_ex_data(dh, 0);
    size_t length, size = 0;
    DHParameter dp;
    void *data = NULL;
    int ret;

    if (dh->p == NULL || dh->g == NULL)
	return 0;

    memset(&dp, 0, sizeof(dp));

    ret = _hc_BN_to_integer(dh->p, &dp.prime);
    if (ret == 0)
	ret = _hc_BN_to_integer(dh->g, &dp.base);
    if (ret) {
	free_DHParameter(&dp);
	return 0;
    }
    dp.privateValueLength = NULL;

    ASN1_MALLOC_ENCODE(DHParameter, data, length, &dp, &size, ret);
    free_DHParameter(&dp);
    if (ret)
	return 0;
    if (size != length)
	abort();
    
    if (sf->secdh) {
	SecDHDestroy(sf->secdh);
	sf->secdh = NULL;
    }

    ret = SecDHCreateFromParameters(data, length, &sf->secdh);
    if (ret)
	goto error;

    free(data);
    data = NULL;

    length = BN_num_bytes(dh->p);
    data = malloc(size);
    if (data == NULL)
	goto error;

    ret = SecDHGenerateKeypair(sf->secdh, data, &length);
    if (ret)
	goto error;

    dh->pub_key = BN_bin2bn(data, length, NULL);
    if (dh->pub_key == NULL)
	goto error;

    free(data);

    return 1;

 error:
    if (data)
	free(data);
    if (sf->secdh)
	SecDHDestroy(sf->secdh);
    sf->secdh = NULL;

    return 1;
}

static int
sf_dh_compute_key(unsigned char *shared, const BIGNUM * pub, DH *dh)
{
    struct dh_sf *sf = DH_get_ex_data(dh, 0);
    size_t length, shared_length;
    OSStatus ret;
    void *data;

    shared_length = BN_num_bytes(dh->p);

    length = BN_num_bytes(pub);
    data = malloc(length);
    if (data == NULL)
	return 0;

    BN_bn2bin(pub, data);

    ret = SecDHComputeKey(sf->secdh, data, length, shared, &shared_length);
    free(data);
    if (ret)
	return 0;

    return shared_length;
}


static int
sf_dh_generate_params(DH *dh, int a, int b, BN_GENCB *callback)
{
    return 0;
}

static int
sf_dh_init(DH *dh)
{
    struct dh_sf *sf;

    sf = calloc(1, sizeof(*sf));
    if (sf == NULL)
	return 0;

    DH_set_ex_data(dh, 0, sf);

    return 1;
}

static int
sf_dh_finish(DH *dh)
{
    struct dh_sf *sf = DH_get_ex_data(dh, 0);

    if (sf->secdh)
	SecDHDestroy(sf->secdh);
    free(sf);

    return 1;
}


/*
 *
 */

const DH_METHOD _hc_dh_sf_method = {
    "hcrypto sf DH",
    sf_dh_generate_key,
    sf_dh_compute_key,
    NULL,
    sf_dh_init,
    sf_dh_finish,
    0,
    NULL,
    sf_dh_generate_params
};

/**
 * DH implementation using SecurityFramework
 *
 * @return the DH_METHOD for the DH implementation using SecurityFramework.
 *
 * @ingroup hcrypto_dh
 */

const DH_METHOD *
DH_sf_method(void)
{
    return &_hc_dh_sf_method;
}
#endif /* HEIM_HC_SF */

/*
 * Copyright (c) 2006 - 2008 Kungliga Tekniska Högskolan
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

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <krb5-types.h>
#include <rfc2459_asn1.h>
#include <ecdsa.h>
#include "common.h"
#include <roken.h>

ECDSA *
ECDSA_new(void)
{
    return ECDSA_new_method(NULL);
}

/**
 * Allocate a new ECDSA object using the engine, if NULL is specified as
 * the engine, use the default ECDSA engine as returned by
 * ENGINE_get_default_ECDSA().
 *
 * @param engine Specific what ENGINE ECDSA provider should be used.
 *
 * @return a newly allocated ECDSA object. Free with ECDSA_free().
 *
 */

ECDSA *
ECDSA_new_method(ENGINE *engine)
{
    ECDSA *ecdsa;

    ecdsa = calloc(1, sizeof(*ecdsa));
    if (ecdsa == NULL)
	return NULL;

    ecdsa->references = 1;

    if (engine) {
	ENGINE_up_ref(engine);
	ecdsa->engine = engine;
    } else {
	ecdsa->engine = ENGINE_get_default_ECDSA();
    }

    if (ecdsa->engine) {
	ecdsa->meth = ENGINE_get_ECDSA(ecdsa->engine);
	if (ecdsa->meth == NULL) {
	    ENGINE_finish(engine);
	    free(ecdsa);
	    return 0;
	}
    }

    if (ecdsa->meth == NULL)
	ecdsa->meth = rk_UNCONST(ECDSA_get_default_method());

    (*ecdsa->meth->init)(ecdsa);

    return ecdsa;
}

/**
 * Free an allocation ECDSA object.
 *
 * @param ecdsa the ECDSA object to free.
 */

void
ECDSA_free(ECDSA *ecdsa)
{
    if (ecdsa->references <= 0)
	abort();

    if (--ecdsa->references > 0)
	return;

    (*ecdsa->meth->finish)(ecdsa);

    if (ecdsa->engine)
	ENGINE_finish(ecdsa->engine);

    memset(ecdsa, 0, sizeof(*ecdsa));
    free(ecdsa);
}

/**
 * Add an extra reference to the ECDSA object. The object should be free
 * with ECDSA_free() to drop the reference.
 *
 * @param ecdsa the object to add reference counting too.
 *
 * @return the current reference count, can't safely be used except
 * for debug printing.
 *
 */

int
ECDSA_up_ref(ECDSA *ecdsa)
{
    return ++ecdsa->references;
}

/**
 * Return the ECDSA_METHOD used for this ECDSA object.
 *
 * @param ecdsa the object to get the method from.
 *
 * @return the method used for this ECDSA object.
 *
 */

const ECDSA_METHOD *
ECDSA_get_method(const ECDSA *ecdsa)
{
    return ecdsa->meth;
}

/**
 * Set a new method for the ECDSA keypair.
 *
 * @param ecdsa ecdsa parameter.
 * @param method the new method for the ECDSA parameter.
 *
 * @return 1 on success.
 *
 */

int
ECDSA_set_method(ECDSA *ecdsa, const ECDSA_METHOD *method)
{
    (*ecdsa->meth->finish)(ecdsa);

    if (ecdsa->engine) {
	ENGINE_finish(ecdsa->engine);
	ecdsa->engine = NULL;
    }

    ecdsa->meth = method;
    (*ecdsa->meth->init)(ecdsa);
    return 1;
}

/**
 * Set the application data for the ECDSA object.
 *
 * @param ecdsa the ecdsa object to set the parameter for
 * @param arg the data object to store
 *
 * @return 1 on success.
 *
 */

int
ECDSA_set_app_data(ECDSA *ecdsa, void *arg)
{
    ecdsa->ex_data.sk = arg;
    return 1;
}

/**
 * Get the application data for the ECDSA object.
 *
 * @param ecdsa the ecdsa object to get the parameter for
 *
 * @return the data object
 *
 */

void *
ECDSA_get_app_data(const ECDSA *ecdsa)
{
    return ecdsa->ex_data.sk;
}

#define ECDSAFUNC(name, body) \
int \
name(int flen,const unsigned char* f, unsigned char* t, ECDSA* r, int p){\
    return body; \
}

int
ECDSA_sign(int type, const unsigned char *from, unsigned int flen,
	 unsigned char *to, unsigned int *tlen, ECDSA *ecdsa)
{
    if (ecdsa->meth->ecdsa_sign)
	return ecdsa->meth->ecdsa_sign(type, from, flen, to, tlen, ecdsa);

    return 0;
}

int
ECDSA_verify(int type, const unsigned char *from, unsigned int flen,
	   unsigned char *sigbuf, unsigned int siglen, ECDSA *ecdsa)
{
    if (ecdsa->meth->ecdsa_verify)
	return ecdsa->meth->ecdsa_verify(type, from, flen, sigbuf, siglen, ecdsa);

    return 0;
}

int
ECDSA_size(EC_KEY *ecdsa)
{
    return ECDSA_KEY_SIZE;
}


/*
 * A NULL ECDSA_METHOD that returns failure for all operations. This is
 * used as the default ECDSA method if we don't have any native
 * support.
 */

static ECDSAFUNC(null_ecdsa_public_encrypt, -1)
static ECDSAFUNC(null_ecdsa_public_decrypt, -1)
static ECDSAFUNC(null_ecdsa_private_encrypt, -1)
static ECDSAFUNC(null_ecdsa_private_decrypt, -1)


/*
 *
 */

static int
null_ecdsa_init(ECDSA *ecdsa)
{
    return 1;
}

static int
null_ecdsa_finish(ECDSA *ecdsa)
{
    return 1;
}

static const ECDSA_METHOD ecdsa_null_method = {
    "hcrypto null ECDSA",
    null_ecdsa_public_encrypt,
    null_ecdsa_public_decrypt,
    null_ecdsa_private_encrypt,
    null_ecdsa_private_decrypt,
    null_ecdsa_init,
    null_ecdsa_finish,
    0,
    NULL,
    NULL
};

const ECDSA_METHOD *
ECDSA_null_method(void)
{
    return &ecdsa_null_method;
}

static const ECDSA_METHOD *default_ecdsa_method = &ecdsa_null_method;

const ECDSA_METHOD *
ECDSA_get_default_method(void)
{
    return default_ecdsa_method;
}

void
ECDSA_set_default_method(const ECDSA_METHOD *meth)
{
    default_ecdsa_method = meth;
}

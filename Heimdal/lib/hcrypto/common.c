/*
 * Copyright (c) 2010 Kungliga Tekniska Högskolan
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

#include <errno.h>

#include <stdio.h>
#include <stdlib.h>

#include <krb5-types.h>
#include <rfc2459_asn1.h>
#include <hcrypto/bn.h>

#ifdef HAVE_CDSA
#define NEED_CDSA 1
#endif

#include "common.h"

#ifdef HAVE_CDSA

static CSSM_CSP_HANDLE cspHandle;

static CSSM_VERSION vers = {2, 0 };
static const CSSM_GUID guid = { 0xFADE, 0, 0, { 1, 2, 3, 4, 5, 6, 7, 0 } };

const CSSM_DATA _hc_labelData = { 7, (void *)"noLabel" };

static void * cssmMalloc(CSSM_SIZE size, void *alloc)           { return malloc(size); }
static void   cssmFree(void *ptr, void *alloc)                  { free(ptr); }
static void * cssmRealloc(void *ptr, CSSM_SIZE size, void *alloc)       { return realloc(ptr, size); }
static void * cssmCalloc(uint32 num, CSSM_SIZE size, void *alloc)       { return calloc(num, size); }


static CSSM_API_MEMORY_FUNCS cssm_memory_funcs = {
    cssmMalloc,
    cssmFree,
    cssmRealloc,
    cssmCalloc,
    NULL
};
 
CSSM_CSP_HANDLE
_hc_get_cdsa_csphandle(void)
{
    CSSM_PVC_MODE pvcPolicy = CSSM_PVC_NONE;
    CSSM_RETURN ret;

    if (cspHandle)
        return cspHandle;
        
    ret = CSSM_Init(&vers, CSSM_PRIVILEGE_SCOPE_NONE,
                    &guid, CSSM_KEY_HIERARCHY_NONE,
                    &pvcPolicy, NULL);
    if (ret != CSSM_OK) 
        abort();

    ret = CSSM_ModuleLoad(&gGuidAppleCSP, CSSM_KEY_HIERARCHY_NONE, NULL, NULL);
    if (ret)
        abort();

    ret = CSSM_ModuleAttach(&gGuidAppleCSP, &vers, &cssm_memory_funcs,
                            0, CSSM_SERVICE_CSP, 0,
                            CSSM_KEY_HIERARCHY_NONE,
                            NULL, 0, NULL, &cspHandle);
    if (ret)
        abort();

    return cspHandle;
}
#endif


int
_hc_BN_to_integer(BIGNUM *bn, heim_integer *integer)
{
    integer->length = BN_num_bytes(bn);
    integer->data = malloc(integer->length);
    if (integer->data == NULL)
	return ENOMEM;
    BN_bn2bin(bn, integer->data);
    integer->negative = BN_is_negative(bn);
    return 0;
}

BIGNUM *
_hc_integer_to_BN(const heim_integer *i, BIGNUM *bn)
{
    bn = BN_bin2bn(i->data, i->length, bn);
    if (bn)
	BN_set_negative(bn, i->negative);
    return bn;
}

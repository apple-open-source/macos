/*
 * Copyright (c) 2013 Apple Inc. All Rights Reserved.
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

/* THIS FILE CONTAINS KERNEL CODE */
#include <tls_ciphersuites.h>

#include "sslBuildFlags.h"
#include "sslDebug.h"
#include "sslMemory.h"
#include "symCipher.h"
#include "tls_types_private.h"

#include <corecrypto/ccrc4.h>

#include <string.h>
#include <AssertMacros.h>
#include <stddef.h>

#if		ENABLE_RC4

struct SymCipherContext {
    const struct ccrc4_info *rc4;
    ccrc4_ctx key[];
};

static int RC4Init(
    const SSLSymmetricCipherParams *params,
    int encrypting,
	uint8_t *key,
	uint8_t *iv,
    struct ccrng_state *rng,
	SymCipherContext *cipherCtx)
{
    SymCipherContext ctx = *cipherCtx;

	/*
	 * Cook up a ccrc4_ctx object. Assumes:
	 *		cipherCtx->encrypting
	 * 		key (raw key bytes)
	 *		iv (raw bytes)
	 * On successful exit:
	 * 		Resulting ccmode --> cipherCtx
	 */

    /* FIXME: this should not be needed as long as CCSymFinish is called */
	if (ctx) {
        sslFree(ctx);
        ctx = NULL;
	}

    const struct ccrc4_info *rc4 = ccrc4();

    ctx = sslMalloc(offsetof(struct SymCipherContext, key) + rc4->size);

    if (ctx==NULL) {
        sslErrorLog("RC4Init: Can't allocate context\n");
        return errSSLRecordInternal;
    }

    rc4->init(ctx->key, params->keySize, key);
    ctx->rc4 = rc4;

    *cipherCtx = ctx;
	return 0;
}

static int RC4Crypt(
	const uint8_t *src, 
	uint8_t *dest,
    size_t len,
	SymCipherContext cipherCtx)
{
    cipherCtx->rc4->crypt(cipherCtx->key, len, src, dest);

    return 0;
}

static int RC4Finish(
	SymCipherContext cipherCtx)
{
	if (cipherCtx) {
        ccrc4_ctx_clear(cipherCtx->rc4->size, cipherCtx->key);
        sslFree(cipherCtx);
	}
	return 0;
}

const SSLSymmetricCipher SSLCipherRC4_128 = {
    .params = &SSLCipherRC4_128Params,
    .c.cipher = {
        .initialize = RC4Init,
        .encrypt = RC4Crypt,
        .decrypt = RC4Crypt
    },
    .finish = RC4Finish
};

#endif	/* ENABLE_RC4 */


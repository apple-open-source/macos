/*
 * Copyright (c) 2010 Apple Inc. All Rights Reserved.
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

/* Adapted from LibTomCrypt, modular cryptographic library -- Tom St Denis
 *
 * LibTomCrypt is a library that provides various cryptographic
 * algorithms in a highly modular and flexible manner.
 *
 * The library is free for all purposes without any express
 * guarantee it works.
 *
 * Tom St Denis, tomstdenis@gmail.com, http://libtom.org
 */
/**
   @param sha224.c
   vng_neon_SHA-224 new NIST standard based off of vng_neon_SHA-256 truncated to 224 bits (Tom St Denis)
*/

const ccDescriptor vng_neon_sha224_desc =
{
    .implementation_info = &cc_sha224_impinfo,
	.dtype.digest.hashsize = CC_SHA224_DIGEST_LENGTH,
	.dtype.digest.blocksize = CC_SHA224_BLOCK_BYTES,
    .dtype.digest.digest_info = NULL,
	.dtype.digest.init = &vng_neon_sha224_init,
	.dtype.digest.process = &vng_neon_sha256_process,
	.dtype.digest.done = &vng_neon_sha224_done,
};


/* init the sha256 er... sha224 state ;-) */
/**
   Initialize the hash state
   @param md   The hash state you wish to initialize
   @return CRYPT_OK if successful
*/
int vng_neon_sha224_init(vng_neon_sha256_ctx *ctx)
{
    LTC_ARGCHK(ctx != NULL);

    // ctx->curlen = 0;
    ctx->length = 0;
    ctx->state[0] = 0xc1059ed8UL;
    ctx->state[1] = 0x367cd507UL;
    ctx->state[2] = 0x3070dd17UL;
    ctx->state[3] = 0xf70e5939UL;
    ctx->state[4] = 0xffc00b31UL;
    ctx->state[5] = 0x68581511UL;
    ctx->state[6] = 0x64f98fa7UL;
    ctx->state[7] = 0xbefa4fa4UL;

    return CRYPT_OK;
}

/**
   Terminate the hash to get the digest
   @param md  The hash state
   @param out [out] The destination of the hash (28 bytes)
   @return CRYPT_OK if successful
*/
int vng_neon_sha224_done(vng_neon_sha256_ctx *ctx, unsigned char *out)
{
    unsigned char buf[32];
    int err;

    LTC_ARGCHK(ctx  != NULL);
    LTC_ARGCHK(out != NULL);

    err = vng_neon_sha256_done(ctx, buf);
    CC_XMEMCPY(out, buf, 28);
    return err;
}


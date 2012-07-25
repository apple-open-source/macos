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

/* LibTomCrypt, modular cryptographic library -- Tom St Denis
 *
 * LibTomCrypt is a library that provides various cryptographic
 * algorithms in a highly modular and flexible manner.
 *
 * The library is free for all purposes without any express
 * guarantee it works.
 *
 * Tom St Denis, tomstdenis@gmail.com, http://libtom.org
 */
 

#if defined (__x86_64__) || defined(__i386__)		// x86_64 or i386 architectures

#include <stdio.h>
#include "vng_x86_sha1.h"
#include "ltc_hashcommon.h"
#include "tomcrypt_cfg.h"
#include "tomcrypt_macros.h"
#include "tomcrypt_argchk.h"
#include "ccDescriptors.h"
#include "ccErrors.h"
#include "ccMemory.h"
#include "CommonDigest.h"

/**
  @file vng_x86_sha1.c
  original LTC_SHA1 code by Tom St Denis 
  optimized compress function by the Vector and Numerics Group
*/


const ccDescriptor vng_x86_sha1_desc =
{
    .implementation_info = &cc_sha1_impinfo,
	.dtype.digest.hashsize = CC_SHA1_DIGEST_LENGTH,
	.dtype.digest.blocksize = CC_SHA1_BLOCK_BYTES,
    .dtype.digest.digest_info = NULL,
	.dtype.digest.init = &vng_x86_sha1_init,
	.dtype.digest.process = &vng_x86_sha1_process,
	.dtype.digest.done = &vng_x86_sha1_done,
};


/**
   Initialize the hash state
   @param md   The hash state you wish to initialize
   @return CRYPT_OK if successful
*/
int vng_x86_sha1_init(vng_x86_sha1_ctx *ctx)
{
   LTC_ARGCHK(ctx != NULL);
   
   ctx->state[0] = 0x67452301UL;
   ctx->state[1] = 0xefcdab89UL;
   ctx->state[2] = 0x98badcfeUL;
   ctx->state[3] = 0x10325476UL;
   ctx->state[4] = 0xc3d2e1f0UL;
    CC_XZEROMEM(ctx->buf, CC_SHA1_BLOCK_BYTES);
    ctx->curlen = 0;
   ctx->length = 0;
   
   return CRYPT_OK;
}

/**
   Process a block of memory though the hash
   @param md     The hash state
   @param in     The data to hash
   @param inlen  The length of the data (octets)
   @return CRYPT_OK if successful
*/
int vng_x86_sha1_process(vng_x86_sha1_ctx *ctx, const unsigned char *in, unsigned long inlen)
{
    unsigned long n; 
    int err;
    int fullblocks, remainder, processed;
    
    LTC_ARGCHK(ctx != NULL); 
    LTC_ARGCHK(in != NULL);
        
    if (ctx->curlen > sizeof(ctx->buf)) {
        return CRYPT_INVALID_ARG;
    } 
    if ((ctx->length + inlen) < ctx->length) { 
        return CRYPT_HASH_OVERFLOW; 
    }    
    
    while (inlen > 0) { 
        if (ctx->curlen == 0 && inlen >= VNG_X86_SHA1_BLOCKSIZE && CC_XALIGNED(in, 4)) {
            fullblocks = inlen / VNG_X86_SHA1_BLOCKSIZE;
            // remainder = inlen % VNG_X86_SHA1_BLOCKSIZE;
            processed = fullblocks * VNG_X86_SHA1_BLOCKSIZE;
            sha1_x86_compress_data_order (ctx->state, in, fullblocks);
            ctx->length += processed * 8; 
            in += processed;
            inlen -= processed; 
        } else {
            n = MIN(inlen, (VNG_X86_SHA1_BLOCKSIZE - ctx->curlen)); 
            CC_XMEMCPY(ctx->buf + ctx->curlen, in, n); 
            ctx->curlen += n; in += n; inlen -= n; 
            if (ctx->curlen == VNG_X86_SHA1_BLOCKSIZE) {
                sha1_x86_compress_data_order (ctx->state, ctx->buf, 1);
                ctx->length += 8*VNG_X86_SHA1_BLOCKSIZE;
                ctx->curlen = 0; 
            } 
        } 
    }
    
    return CRYPT_OK;
}

/**
   Terminate the hash to get the digest
   @param md  The hash state
   @param out [out] The destination of the hash (20 bytes)
   @return CRYPT_OK if successful
*/
int vng_x86_sha1_done(vng_x86_sha1_ctx *ctx, unsigned char *out)
{
    int i;

    LTC_ARGCHK(ctx  != NULL);
    LTC_ARGCHK(out != NULL);

    if (ctx->curlen >= sizeof(ctx->buf)) {
       return CRYPT_INVALID_ARG;
    }
    
    /* increase the length of the message */
    ctx->length += ctx->curlen * 8;
    
    /* append the '1' bit */
    ctx->buf[ctx->curlen++] = (unsigned char)0x80;

    /* if the length is currently above 56 bytes we append zeros
     * then compress.  Then we can fall back to padding zeros and length
     * encoding like normal.
     */
    if (ctx->curlen > 56) {
        while (ctx->curlen < 64) {
            ctx->buf[ctx->curlen++] = (unsigned char)0;
        }
        sha1_x86_compress_data_order(ctx->state, ctx->buf, 1);
        ctx->curlen = 0;
    }

    /* pad upto 56 bytes of zeroes */
    while (ctx->curlen < 56) {
        ctx->buf[ctx->curlen++] = (unsigned char)0;
    }

    /* store length */
    CC_XSTORE64H(ctx->length, ctx->buf+56);
    sha1_x86_compress_data_order(ctx->state, ctx->buf, 1);

    /* copy output */
    for(i=0; i<5; i++, out+=4) CC_XSTORE32H(ctx->state[i], out);
    
    return CRYPT_OK;
}
#endif /* x86 */

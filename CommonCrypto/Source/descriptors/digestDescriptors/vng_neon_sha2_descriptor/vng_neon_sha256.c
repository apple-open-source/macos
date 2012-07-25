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
#include <arm/arch.h>

#if defined(_ARM_ARCH_7)

#include <stdio.h>
#include "vng_neon_sha256.h"
#include "vng_neon_sha224.h"
#include "ltc_hashcommon.h"
#include "tomcrypt_cfg.h"
#include "tomcrypt_macros.h"
#include "tomcrypt_argchk.h"
#include "ccDescriptors.h"
#include "ccErrors.h"
#include "ccMemory.h"
#include "CommonDigest.h"

/**
  @file vng_neon_sha256.c
  vng_neon_SHA256 by Tom St Denis 
*/

const ccDescriptor vng_neon_sha256_desc =
{
    .implementation_info = &cc_sha256_impinfo,
	.dtype.digest.hashsize = CC_SHA256_DIGEST_LENGTH,
	.dtype.digest.blocksize = CC_SHA256_BLOCK_BYTES,
    .dtype.digest.digest_info = NULL,
	.dtype.digest.init = &vng_neon_sha256_init,
	.dtype.digest.process = &vng_neon_sha256_process,
	.dtype.digest.done = &vng_neon_sha256_done,
};


#ifdef LTC_SMALL_CODE
/* the K array */
static const ulong32 K[64] = {
    0x428a2f98UL, 0x71374491UL, 0xb5c0fbcfUL, 0xe9b5dba5UL, 0x3956c25bUL,
    0x59f111f1UL, 0x923f82a4UL, 0xab1c5ed5UL, 0xd807aa98UL, 0x12835b01UL,
    0x243185beUL, 0x550c7dc3UL, 0x72be5d74UL, 0x80deb1feUL, 0x9bdc06a7UL,
    0xc19bf174UL, 0xe49b69c1UL, 0xefbe4786UL, 0x0fc19dc6UL, 0x240ca1ccUL,
    0x2de92c6fUL, 0x4a7484aaUL, 0x5cb0a9dcUL, 0x76f988daUL, 0x983e5152UL,
    0xa831c66dUL, 0xb00327c8UL, 0xbf597fc7UL, 0xc6e00bf3UL, 0xd5a79147UL,
    0x06ca6351UL, 0x14292967UL, 0x27b70a85UL, 0x2e1b2138UL, 0x4d2c6dfcUL,
    0x53380d13UL, 0x650a7354UL, 0x766a0abbUL, 0x81c2c92eUL, 0x92722c85UL,
    0xa2bfe8a1UL, 0xa81a664bUL, 0xc24b8b70UL, 0xc76c51a3UL, 0xd192e819UL,
    0xd6990624UL, 0xf40e3585UL, 0x106aa070UL, 0x19a4c116UL, 0x1e376c08UL,
    0x2748774cUL, 0x34b0bcb5UL, 0x391c0cb3UL, 0x4ed8aa4aUL, 0x5b9cca4fUL,
    0x682e6ff3UL, 0x748f82eeUL, 0x78a5636fUL, 0x84c87814UL, 0x8cc70208UL,
    0x90befffaUL, 0xa4506cebUL, 0xbef9a3f7UL, 0xc67178f2UL
};
#endif

/* Various logical functions */
#define Ch(x,y,z)       (z ^ (x & (y ^ z)))
#define Maj(x,y,z)      (((x | y) & z) | (x & y)) 
#define S(x, n)         LTC_RORc((x),(n))
#define R(x, n)         (((x)&0xFFFFFFFFUL)>>(n))
#define Sigma0(x)       (S(x, 2) ^ S(x, 13) ^ S(x, 22))
#define Sigma1(x)       (S(x, 6) ^ S(x, 11) ^ S(x, 25))
#define Gamma0(x)       (S(x, 7) ^ S(x, 18) ^ R(x, 3))
#define Gamma1(x)       (S(x, 17) ^ S(x, 19) ^ R(x, 10))


#define FULLLENGTH_MASK 0xffffffffffffffc0
#define BUFFLENGTH_MASK 0x3f
/**
   Initialize the hash state
   @param md   The hash state you wish to initialize
   @return CRYPT_OK if successful
*/
int vng_neon_sha256_init(vng_neon_sha256_ctx *ctx)
{
    LTC_ARGCHK(ctx != NULL);

    ctx->length = 0;
    ctx->state[0] = 0x6A09E667UL;
    ctx->state[1] = 0xBB67AE85UL;
    ctx->state[2] = 0x3C6EF372UL;
    ctx->state[3] = 0xA54FF53AUL;
    ctx->state[4] = 0x510E527FUL;
    ctx->state[5] = 0x9B05688CUL;
    ctx->state[6] = 0x1F83D9ABUL;
    ctx->state[7] = 0x5BE0CD19UL;

    return CRYPT_OK;
}

/**
   Process a block of memory though the hash
   @param md     The hash state
   @param in     The data to hash
   @param inlen  The length of the data (octets)
   @return CRYPT_OK if successful
*/
int vng_neon_sha256_process(vng_neon_sha256_ctx *ctx, const unsigned char *in, unsigned long inlen)
{
    unsigned long n; 
    int err;
    int fullblocks, remainder, processed;
    uint64_t curlen;
    
    LTC_ARGCHK(ctx != NULL); 
    LTC_ARGCHK(in != NULL);
            
    curlen = ctx->length & BUFFLENGTH_MASK;
    
    if ((ctx->length + inlen) < ctx->length) { 
        return CRYPT_HASH_OVERFLOW; 
    }
    
    while (inlen > 0) { 
        if (curlen == 0 && inlen >= VNG_NEON_SHA256_BLOCKSIZE && CC_XALIGNED(in, 4)) {
            fullblocks = inlen / VNG_NEON_SHA256_BLOCKSIZE;
            remainder = inlen % VNG_NEON_SHA256_BLOCKSIZE;
            processed = fullblocks * VNG_NEON_SHA256_BLOCKSIZE;
            vng_armv7neon_sha256_compress (ctx->state, fullblocks, in);
            ctx->length += VNG_NEON_SHA256_BLOCKSIZE * 8 * fullblocks; 
            in += processed;
            inlen -= processed; 
        } else {
            n = MIN(inlen, (VNG_NEON_SHA256_BLOCKSIZE - curlen)); 
            memcpy(ctx->buf + curlen, in, (size_t)n); 
            curlen += n; in += n; inlen -= n; 
            if (curlen == VNG_NEON_SHA256_BLOCKSIZE) {
                vng_armv7neon_sha256_compress (ctx->state, 1, ctx->buf);
                ctx->length += 8*VNG_NEON_SHA256_BLOCKSIZE;
                curlen = 0; 
            } 
        } 
    }
    
    ctx->length = (ctx->length & FULLLENGTH_MASK) + curlen;
    
    return CRYPT_OK;
}

/**
   Terminate the hash to get the digest
   @param md  The hash state
   @param out [out] The destination of the hash (32 bytes)
   @return CRYPT_OK if successful
*/
int vng_neon_sha256_done(vng_neon_sha256_ctx *ctx, unsigned char *out)
{
    int i;
    uint64_t curlen;

    LTC_ARGCHK(ctx  != NULL);
    LTC_ARGCHK(out != NULL);

    curlen = ctx->length & BUFFLENGTH_MASK;
    ctx->length &= FULLLENGTH_MASK;

    /* increase the length of the message */
    ctx->length += curlen * 8;

    /* append the '1' bit */
    ctx->buf[curlen++] = (unsigned char)0x80;

    /* if the length is currently above 56 bytes we append zeros
     * then compress.  Then we can fall back to padding zeros and length
     * encoding like normal.
     */
    if (curlen > 56) {
        while (curlen < 64) {
            ctx->buf[curlen++] = (unsigned char)0;
        }
        vng_armv7neon_sha256_compress (ctx->state, 1, ctx->buf);

        curlen = 0;
    }

    /* pad upto 56 bytes of zeroes */
    while (curlen < 56) {
        ctx->buf[curlen++] = (unsigned char)0;
    }

    /* store length */
    LTC_STORE64H(ctx->length, ctx->buf+56);
    vng_armv7neon_sha256_compress (ctx->state, 1, ctx->buf);

    /* copy output */
    for (i = 0; i < 8; i++) {
        LTC_STORE32H(ctx->state[i], out+(4*i));
    }
#ifdef LTC_CLEAN_STACK
    ltc_zeromem(ctx, sizeof(hash_state));
#endif
    return CRYPT_OK;
}


#include "vng_neon_sha224.c"

#endif /* _ARM_ARCH_7 */

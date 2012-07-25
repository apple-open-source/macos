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
#include "vng_x86_sha256.h"
#include "vng_x86_sha224.h"
#include "ltc_hashcommon.h"
#include "tomcrypt_cfg.h"
#include "tomcrypt_macros.h"
#include "tomcrypt_argchk.h"
#include "ccDescriptors.h"
#include "ccErrors.h"
#include "ccMemory.h"
#include "CommonDigest.h"

/**
  @file vng_x86_sha256.c
  vng_x86_SHA256 by Tom St Denis 
*/

const ccDescriptor vng_x86_sha256_desc =
{
    .implementation_info = &cc_sha256_impinfo,
	.dtype.digest.hashsize = CC_SHA256_DIGEST_LENGTH,
	.dtype.digest.blocksize = CC_SHA256_BLOCK_BYTES,
    .dtype.digest.digest_info = NULL,
	.dtype.digest.init = &vng_x86_sha256_init,
	.dtype.digest.process = &vng_x86_sha256_process,
	.dtype.digest.done = &vng_x86_sha256_done,
};

extern const uint32_t K256[64] =
{   0x428a2f98ul, 0x71374491ul, 0xb5c0fbcful, 0xe9b5dba5ul,
    0x3956c25bul, 0x59f111f1ul, 0x923f82a4ul, 0xab1c5ed5ul,
    0xd807aa98ul, 0x12835b01ul, 0x243185beul, 0x550c7dc3ul,
    0x72be5d74ul, 0x80deb1feul, 0x9bdc06a7ul, 0xc19bf174ul,
    0xe49b69c1ul, 0xefbe4786ul, 0x0fc19dc6ul, 0x240ca1ccul,
    0x2de92c6ful, 0x4a7484aaul, 0x5cb0a9dcul, 0x76f988daul,
    0x983e5152ul, 0xa831c66dul, 0xb00327c8ul, 0xbf597fc7ul,
    0xc6e00bf3ul, 0xd5a79147ul, 0x06ca6351ul, 0x14292967ul,
    0x27b70a85ul, 0x2e1b2138ul, 0x4d2c6dfcul, 0x53380d13ul,
    0x650a7354ul, 0x766a0abbul, 0x81c2c92eul, 0x92722c85ul,
    0xa2bfe8a1ul, 0xa81a664bul, 0xc24b8b70ul, 0xc76c51a3ul,
    0xd192e819ul, 0xd6990624ul, 0xf40e3585ul, 0x106aa070ul,
    0x19a4c116ul, 0x1e376c08ul, 0x2748774cul, 0x34b0bcb5ul,
    0x391c0cb3ul, 0x4ed8aa4aul, 0x5b9cca4ful, 0x682e6ff3ul,
    0x748f82eeul, 0x78a5636ful, 0x84c87814ul, 0x8cc70208ul,
    0x90befffaul, 0xa4506cebul, 0xbef9a3f7ul, 0xc67178f2ul,
};


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
int vng_x86_sha256_init(vng_x86_sha256_ctx *ctx)
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
int vng_x86_sha256_process(vng_x86_sha256_ctx *ctx, const unsigned char *in, unsigned long inlen)
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
        if (curlen == 0 && inlen >= VNG_X86_SHA256_BLOCKSIZE && CC_XALIGNED(in, 4)) {
            fullblocks = inlen / VNG_X86_SHA256_BLOCKSIZE;
            remainder = inlen % VNG_X86_SHA256_BLOCKSIZE;
            processed = fullblocks * VNG_X86_SHA256_BLOCKSIZE;
            vng_x86_sha256_compress (ctx->state, in, fullblocks);
            ctx->length += VNG_X86_SHA256_BLOCKSIZE * 8 * fullblocks; 
            in += processed;
            inlen -= processed; 
        } else {
            n = MIN(inlen, (VNG_X86_SHA256_BLOCKSIZE - curlen)); 
            memcpy(ctx->buf + curlen, in, (size_t)n); 
            curlen += n; in += n; inlen -= n; 
            if (curlen == VNG_X86_SHA256_BLOCKSIZE) {
                vng_x86_sha256_compress (ctx->state, ctx->buf, 1);
                ctx->length += 8*VNG_X86_SHA256_BLOCKSIZE;
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
int vng_x86_sha256_done(vng_x86_sha256_ctx *ctx, unsigned char *out)
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
        vng_x86_sha256_compress (ctx->state, ctx->buf, 1);

        curlen = 0;
    }

    /* pad upto 56 bytes of zeroes */
    while (curlen < 56) {
        ctx->buf[curlen++] = (unsigned char)0;
    }

    /* store length */
    LTC_STORE64H(ctx->length, ctx->buf+56);
    vng_x86_sha256_compress (ctx->state, ctx->buf, 1);

    /* copy output */
    for (i = 0; i < 8; i++) {
        LTC_STORE32H(ctx->state[i], out+(4*i));
    }
#ifdef LTC_CLEAN_STACK
    ltc_zeromem(ctx, sizeof(hash_state));
#endif
    return CRYPT_OK;
}


#include "vng_x86_sha224.c"

#endif /* x86 */

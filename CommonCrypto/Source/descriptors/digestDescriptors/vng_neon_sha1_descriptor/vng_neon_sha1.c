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
#include "vng_neon_sha1.h"
#include "ltc_hashcommon.h"
#include "tomcrypt_cfg.h"
#include "tomcrypt_macros.h"
#include "tomcrypt_argchk.h"
#include "ccDescriptors.h"
#include "ccErrors.h"
#include "ccMemory.h"
#include "CommonDigest.h"

/**
  @file vng_neon_sha1.c
  original LTC_SHA1 code by Tom St Denis 
  optimized compress function by the Vector and Numerics Group
*/


const ccDescriptor vng_neon_sha1_desc =
{
    .implementation_info = &cc_sha1_impinfo,
	.dtype.digest.hashsize = CC_SHA1_DIGEST_LENGTH,
	.dtype.digest.blocksize = CC_SHA1_BLOCK_BYTES,
    .dtype.digest.digest_info = NULL,
	.dtype.digest.init = &vng_neon_sha1_init,
	.dtype.digest.process = &vng_neon_sha1_process,
	.dtype.digest.done = &vng_neon_sha1_done,
};


/**
   Initialize the hash state
   @param md   The hash state you wish to initialize
   @return CRYPT_OK if successful
*/
int vng_neon_sha1_init(vng_neon_sha1_ctx *ctx)
{
   LTC_ARGCHK(ctx != NULL);
   
   ctx->state[0] = 0x67452301UL;
   ctx->state[1] = 0xefcdab89UL;
   ctx->state[2] = 0x98badcfeUL;
   ctx->state[3] = 0x10325476UL;
   ctx->state[4] = 0xc3d2e1f0UL;
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
int vng_neon_sha1_process(vng_neon_sha1_ctx *ctx, const unsigned char *in, unsigned long inlen)
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
        if (ctx->curlen == 0 && inlen >= VNG_NEON_SHA1_BLOCKSIZE && CC_XALIGNED(in, 4)) {
            fullblocks = inlen / VNG_NEON_SHA1_BLOCKSIZE;
            remainder = inlen % VNG_NEON_SHA1_BLOCKSIZE;
            processed = fullblocks * VNG_NEON_SHA1_BLOCKSIZE;
            sha1_vng_armv7neon_compress (ctx->state, fullblocks, in);
            ctx->length += VNG_NEON_SHA1_BLOCKSIZE * 8 * fullblocks; 
            in += processed;
            inlen -= processed; 
        } else {
            n = MIN(inlen, (VNG_NEON_SHA1_BLOCKSIZE - ctx->curlen)); 
            memcpy(ctx->buf + ctx->curlen, in, (size_t)n); 
            ctx->curlen += n; in += n; inlen -= n; 
            if (ctx->curlen == VNG_NEON_SHA1_BLOCKSIZE) {
                sha1_vng_armv7neon_compress (ctx->state, 1, ctx->buf);
                ctx->length += 8*VNG_NEON_SHA1_BLOCKSIZE;
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
int vng_neon_sha1_done(vng_neon_sha1_ctx *ctx, unsigned char *out)
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
        sha1_vng_armv7neon_compress(ctx->state, 1, ctx->buf);
        ctx->curlen = 0;
    }

    /* pad upto 56 bytes of zeroes */
    while (ctx->curlen < 56) {
        ctx->buf[ctx->curlen++] = (unsigned char)0;
    }

    /* store length */
    LTC_STORE64H(ctx->length, ctx->buf+56);
    sha1_vng_armv7neon_compress(ctx->state, 1, ctx->buf);

    /* copy output */
    for (i = 0; i < 5; i++) {
        LTC_STORE32H(ctx->state[i], out+(4*i));
    }
#ifdef LTC_CLEAN_STACK
    ltc_zeromem(md, sizeof(ltc_hash_state));
#endif
    return CRYPT_OK;
}

/**
  Self-test the hash
  @return CRYPT_OK if successful, CRYPT_NOP if self-tests have been disabled
*/  
int  vng_neon_test(void)
{
 #ifndef LTC_TEST
    return CRYPT_NOP;
 #else    
  static const struct {
      const char *msg;
      unsigned char hash[20];
  } tests[] = {
    { "abc",
      { 0xa9, 0x99, 0x3e, 0x36, 0x47, 0x06, 0x81, 0x6a,
        0xba, 0x3e, 0x25, 0x71, 0x78, 0x50, 0xc2, 0x6c,
        0x9c, 0xd0, 0xd8, 0x9d }
    },
    { "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq",
      { 0x84, 0x98, 0x3E, 0x44, 0x1C, 0x3B, 0xD2, 0x6E,
        0xBA, 0xAE, 0x4A, 0xA1, 0xF9, 0x51, 0x29, 0xE5,
        0xE5, 0x46, 0x70, 0xF1 }
    }
  };

  int i;
  unsigned char tmp[20];
  ltc_hash_state md;

  for (i = 0; i < (int)(sizeof(tests) / sizeof(tests[0]));  i++) {
      vng_neon_sha1_init(&md);
      vng_neon_sha1_process(&md, (const unsigned char*)tests[i].msg, (unsigned long)strlen(tests[i].msg));
      vng_neon_sha1_done(&md, tmp);
      if (LTC_XMEMCMP(tmp, tests[i].hash, 20) != 0) {
         return CRYPT_FAIL_TESTVECTOR;
      }
  }
  return CRYPT_OK;
  #endif
}

#endif /* _ARM_ARCH_7 */

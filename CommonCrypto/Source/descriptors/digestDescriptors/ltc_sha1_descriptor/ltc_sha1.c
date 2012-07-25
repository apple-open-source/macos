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
#include <stdio.h>
#include "ltc_sha1.h"
#include "ltc_hashcommon.h"
#include "tomcrypt_cfg.h"
#include "tomcrypt_macros.h"
#include "tomcrypt_argchk.h"
#include "ccDescriptors.h"
#include "ccErrors.h"
#include "ccMemory.h"
#include "CommonDigest.h"

/**
  @file ltc_sha1.c
  LTC_SHA1 code by Tom St Denis 
*/


const ccDescriptor ltc_sha1_desc =
{
    .implementation_info = &cc_sha1_impinfo,
	.dtype.digest.hashsize = CC_SHA1_DIGEST_LENGTH,
	.dtype.digest.blocksize = CC_SHA1_BLOCK_BYTES,
    .dtype.digest.digest_info = NULL,
	.dtype.digest.init = &ltc_sha1_init,
	.dtype.digest.process = &ltc_sha1_process,
	.dtype.digest.done = &ltc_sha1_done,
};


#define F0(x,y,z)  (z ^ (x & (y ^ z)))
#define F1(x,y,z)  (x ^ y ^ z)
#define F2(x,y,z)  ((x & y) | (z & (x | y)))
#define F3(x,y,z)  (x ^ y ^ z)

#ifdef LTC_CLEAN_STACK
static int _sha1_compress(ltc_sha1_ctx *ctx, const unsigned char *buf)
#else
static int  sha1_compress(ltc_sha1_ctx *ctx, const unsigned char *buf)
#endif
{
    ulong32 a,b,c,d,e,W[80],i;
#ifdef LTC_SMALL_CODE
    ulong32 t;
#endif

    /* copy the state into 512-bits into W[0..15] */
    for (i = 0; i < 16; i++) {
        LTC_LOAD32H(W[i], buf + (4*i));
    }

    /* copy state */
    a = ctx->state[0];
    b = ctx->state[1];
    c = ctx->state[2];
    d = ctx->state[3];
    e = ctx->state[4];

    /* expand it */
    for (i = 16; i < 80; i++) {
        W[i] = LTC_ROL(W[i-3] ^ W[i-8] ^ W[i-14] ^ W[i-16], 1); 
    }

    /* compress */
    /* round one */
    #define FF0(a,b,c,d,e,i) e = (LTC_ROLc(a, 5) + F0(b,c,d) + e + W[i] + 0x5a827999UL); b = LTC_ROLc(b, 30);
    #define FF1(a,b,c,d,e,i) e = (LTC_ROLc(a, 5) + F1(b,c,d) + e + W[i] + 0x6ed9eba1UL); b = LTC_ROLc(b, 30);
    #define FF2(a,b,c,d,e,i) e = (LTC_ROLc(a, 5) + F2(b,c,d) + e + W[i] + 0x8f1bbcdcUL); b = LTC_ROLc(b, 30);
    #define FF3(a,b,c,d,e,i) e = (LTC_ROLc(a, 5) + F3(b,c,d) + e + W[i] + 0xca62c1d6UL); b = LTC_ROLc(b, 30);
 
#ifdef LTC_SMALL_CODE
 
    for (i = 0; i < 20; ) {
       FF0(a,b,c,d,e,i++); t = e; e = d; d = c; c = b; b = a; a = t;
    }

    for (; i < 40; ) {
       FF1(a,b,c,d,e,i++); t = e; e = d; d = c; c = b; b = a; a = t;
    }

    for (; i < 60; ) {
       FF2(a,b,c,d,e,i++); t = e; e = d; d = c; c = b; b = a; a = t;
    }

    for (; i < 80; ) {
       FF3(a,b,c,d,e,i++); t = e; e = d; d = c; c = b; b = a; a = t;
    }

#else

    for (i = 0; i < 20; ) {
       FF0(a,b,c,d,e,i++);
       FF0(e,a,b,c,d,i++);
       FF0(d,e,a,b,c,i++);
       FF0(c,d,e,a,b,i++);
       FF0(b,c,d,e,a,i++);
    }

    /* round two */
    for (; i < 40; )  { 
       FF1(a,b,c,d,e,i++);
       FF1(e,a,b,c,d,i++);
       FF1(d,e,a,b,c,i++);
       FF1(c,d,e,a,b,i++);
       FF1(b,c,d,e,a,i++);
    }

    /* round three */
    for (; i < 60; )  { 
       FF2(a,b,c,d,e,i++);
       FF2(e,a,b,c,d,i++);
       FF2(d,e,a,b,c,i++);
       FF2(c,d,e,a,b,i++);
       FF2(b,c,d,e,a,i++);
    }

    /* round four */
    for (; i < 80; )  { 
       FF3(a,b,c,d,e,i++);
       FF3(e,a,b,c,d,i++);
       FF3(d,e,a,b,c,i++);
       FF3(c,d,e,a,b,i++);
       FF3(b,c,d,e,a,i++);
    }
#endif

    #undef FF0
    #undef FF1
    #undef FF2
    #undef FF3

    /* store */
    ctx->state[0] = ctx->state[0] + a;
    ctx->state[1] = ctx->state[1] + b;
    ctx->state[2] = ctx->state[2] + c;
    ctx->state[3] = ctx->state[3] + d;
    ctx->state[4] = ctx->state[4] + e;

    return CRYPT_OK;
}

#ifdef LTC_CLEAN_STACK
static int sha1_compress(ltc_sha1_ctx *ctx, const unsigned char *buf)
{
   int err;
   err = _sha1_compress(ctx, buf);

   ltc_burn_stack(sizeof(ulong32) * 87);
   return err;
}
#endif

/**
   Initialize the hash state
   @param md   The hash state you wish to initialize
   @return CRYPT_OK if successful
*/
int ltc_sha1_init(ltc_sha1_ctx *ctx)
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
LTC_HASH_PROCESS(ltc_sha1_process, sha1_compress, ltc_sha1_ctx, sha1, 64)

/**
   Terminate the hash to get the digest
   @param md  The hash state
   @param out [out] The destination of the hash (20 bytes)
   @return CRYPT_OK if successful
*/
int ltc_sha1_done(ltc_sha1_ctx *ctx, unsigned char *out)
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
        sha1_compress(ctx, ctx->buf);
        ctx->curlen = 0;
    }

    /* pad upto 56 bytes of zeroes */
    while (ctx->curlen < 56) {
        ctx->buf[ctx->curlen++] = (unsigned char)0;
    }

    /* store length */
    LTC_STORE64H(ctx->length, ctx->buf+56);
    sha1_compress(ctx, ctx->buf);

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
int  ltc_sha1_test(void)
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
      ltc_sha1_init(&md);
      ltc_sha1_process(&md, (const unsigned char*)tests[i].msg, (unsigned long)strlen(tests[i].msg));
      ltc_sha1_done(&md, tmp);
      if (LTC_XMEMCMP(tmp, tests[i].hash, 20) != 0) {
         return CRYPT_FAIL_TESTVECTOR;
      }
  }
  return CRYPT_OK;
  #endif
}


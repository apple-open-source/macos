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
#include "ltc_sha512.h"
#include "ltc_sha384.h"
#include "ltc_hashcommon.h"
#include "tomcrypt_cfg.h"
#include "tomcrypt_macros.h"
#include "tomcrypt_argchk.h"
#include "ccDescriptors.h"
#include "ccErrors.h"
#include "ccMemory.h"
#include "CommonDigest.h"

/**
   @param ltc_sha512.c
   LTC_SHA512 by Tom St Denis 
*/

const ccDescriptor ltc_sha512_desc =
{
    .implementation_info = &cc_sha512_impinfo,
	.dtype.digest.hashsize = CC_SHA512_DIGEST_LENGTH,
	.dtype.digest.blocksize = CC_SHA512_BLOCK_BYTES,
    .dtype.digest.digest_info = NULL,
	.dtype.digest.init = &ltc_sha512_init,
	.dtype.digest.process = &ltc_sha512_process,
	.dtype.digest.done = &ltc_sha512_done,
};


/* the K array */
static const ulong64 K[80] = {
LTC_CONST64(0x428a2f98d728ae22), LTC_CONST64(0x7137449123ef65cd), 
LTC_CONST64(0xb5c0fbcfec4d3b2f), LTC_CONST64(0xe9b5dba58189dbbc),
LTC_CONST64(0x3956c25bf348b538), LTC_CONST64(0x59f111f1b605d019), 
LTC_CONST64(0x923f82a4af194f9b), LTC_CONST64(0xab1c5ed5da6d8118),
LTC_CONST64(0xd807aa98a3030242), LTC_CONST64(0x12835b0145706fbe), 
LTC_CONST64(0x243185be4ee4b28c), LTC_CONST64(0x550c7dc3d5ffb4e2),
LTC_CONST64(0x72be5d74f27b896f), LTC_CONST64(0x80deb1fe3b1696b1), 
LTC_CONST64(0x9bdc06a725c71235), LTC_CONST64(0xc19bf174cf692694),
LTC_CONST64(0xe49b69c19ef14ad2), LTC_CONST64(0xefbe4786384f25e3), 
LTC_CONST64(0x0fc19dc68b8cd5b5), LTC_CONST64(0x240ca1cc77ac9c65),
LTC_CONST64(0x2de92c6f592b0275), LTC_CONST64(0x4a7484aa6ea6e483), 
LTC_CONST64(0x5cb0a9dcbd41fbd4), LTC_CONST64(0x76f988da831153b5),
LTC_CONST64(0x983e5152ee66dfab), LTC_CONST64(0xa831c66d2db43210), 
LTC_CONST64(0xb00327c898fb213f), LTC_CONST64(0xbf597fc7beef0ee4),
LTC_CONST64(0xc6e00bf33da88fc2), LTC_CONST64(0xd5a79147930aa725), 
LTC_CONST64(0x06ca6351e003826f), LTC_CONST64(0x142929670a0e6e70),
LTC_CONST64(0x27b70a8546d22ffc), LTC_CONST64(0x2e1b21385c26c926), 
LTC_CONST64(0x4d2c6dfc5ac42aed), LTC_CONST64(0x53380d139d95b3df),
LTC_CONST64(0x650a73548baf63de), LTC_CONST64(0x766a0abb3c77b2a8), 
LTC_CONST64(0x81c2c92e47edaee6), LTC_CONST64(0x92722c851482353b),
LTC_CONST64(0xa2bfe8a14cf10364), LTC_CONST64(0xa81a664bbc423001),
LTC_CONST64(0xc24b8b70d0f89791), LTC_CONST64(0xc76c51a30654be30),
LTC_CONST64(0xd192e819d6ef5218), LTC_CONST64(0xd69906245565a910), 
LTC_CONST64(0xf40e35855771202a), LTC_CONST64(0x106aa07032bbd1b8),
LTC_CONST64(0x19a4c116b8d2d0c8), LTC_CONST64(0x1e376c085141ab53), 
LTC_CONST64(0x2748774cdf8eeb99), LTC_CONST64(0x34b0bcb5e19b48a8),
LTC_CONST64(0x391c0cb3c5c95a63), LTC_CONST64(0x4ed8aa4ae3418acb), 
LTC_CONST64(0x5b9cca4f7763e373), LTC_CONST64(0x682e6ff3d6b2b8a3),
LTC_CONST64(0x748f82ee5defb2fc), LTC_CONST64(0x78a5636f43172f60), 
LTC_CONST64(0x84c87814a1f0ab72), LTC_CONST64(0x8cc702081a6439ec),
LTC_CONST64(0x90befffa23631e28), LTC_CONST64(0xa4506cebde82bde9), 
LTC_CONST64(0xbef9a3f7b2c67915), LTC_CONST64(0xc67178f2e372532b),
LTC_CONST64(0xca273eceea26619c), LTC_CONST64(0xd186b8c721c0c207), 
LTC_CONST64(0xeada7dd6cde0eb1e), LTC_CONST64(0xf57d4f7fee6ed178),
LTC_CONST64(0x06f067aa72176fba), LTC_CONST64(0x0a637dc5a2c898a6), 
LTC_CONST64(0x113f9804bef90dae), LTC_CONST64(0x1b710b35131c471b),
LTC_CONST64(0x28db77f523047d84), LTC_CONST64(0x32caab7b40c72493), 
LTC_CONST64(0x3c9ebe0a15c9bebc), LTC_CONST64(0x431d67c49c100d4c),
LTC_CONST64(0x4cc5d4becb3e42b6), LTC_CONST64(0x597f299cfc657e2a), 
LTC_CONST64(0x5fcb6fab3ad6faec), LTC_CONST64(0x6c44198c4a475817)
};

/* Various logical functions */
#define Ch(x,y,z)       (z ^ (x & (y ^ z)))
#define Maj(x,y,z)      (((x | y) & z) | (x & y)) 
#define S(x, n)         LTC_ROR64c(x, n)
#define R(x, n)         (((x) & LTC_CONST64(0xFFFFFFFFFFFFFFFF) )>>((ulong64)n))
#define Sigma0(x)       (S(x, 28) ^ S(x, 34) ^ S(x, 39))
#define Sigma1(x)       (S(x, 14) ^ S(x, 18) ^ S(x, 41))
#define Gamma0(x)       (S(x, 1) ^ S(x, 8) ^ R(x, 7))
#define Gamma1(x)       (S(x, 19) ^ S(x, 61) ^ R(x, 6))

/* compress 1024-bits */
#ifdef LTC_CLEAN_STACK
static int _sha512_compress(ltc_sha512_ctx *ctx, const unsigned char *buf)
#else
static int  sha512_compress(ltc_sha512_ctx *ctx, const unsigned char *buf)
#endif
{
    ulong64 S[8], W[80], t0, t1;
    int i;

    /* copy state into S */
    for (i = 0; i < 8; i++) {
        S[i] = ctx->state[i];
    }

    /* copy the state into 1024-bits into W[0..15] */
    for (i = 0; i < 16; i++) {
        LTC_LOAD64H(W[i], buf + (8*i));
    }

    /* fill W[16..79] */
    for (i = 16; i < 80; i++) {
        W[i] = Gamma1(W[i - 2]) + W[i - 7] + Gamma0(W[i - 15]) + W[i - 16];
    }        

    /* Compress */
#ifdef LTC_SMALL_CODE
    for (i = 0; i < 80; i++) {
        t0 = S[7] + Sigma1(S[4]) + Ch(S[4], S[5], S[6]) + K[i] + W[i];
        t1 = Sigma0(S[0]) + Maj(S[0], S[1], S[2]);
        S[7] = S[6];
        S[6] = S[5];
        S[5] = S[4];
        S[4] = S[3] + t0;
        S[3] = S[2];
        S[2] = S[1];
        S[1] = S[0];
        S[0] = t0 + t1;
    }
#else
#define RND(a,b,c,d,e,f,g,h,i)                    \
     t0 = h + Sigma1(e) + Ch(e, f, g) + K[i] + W[i];   \
     t1 = Sigma0(a) + Maj(a, b, c);                  \
     d += t0;                                        \
     h  = t0 + t1;

     for (i = 0; i < 80; i += 8) {
         RND(S[0],S[1],S[2],S[3],S[4],S[5],S[6],S[7],i+0);
         RND(S[7],S[0],S[1],S[2],S[3],S[4],S[5],S[6],i+1);
         RND(S[6],S[7],S[0],S[1],S[2],S[3],S[4],S[5],i+2);
         RND(S[5],S[6],S[7],S[0],S[1],S[2],S[3],S[4],i+3);
         RND(S[4],S[5],S[6],S[7],S[0],S[1],S[2],S[3],i+4);
         RND(S[3],S[4],S[5],S[6],S[7],S[0],S[1],S[2],i+5);
         RND(S[2],S[3],S[4],S[5],S[6],S[7],S[0],S[1],i+6);
         RND(S[1],S[2],S[3],S[4],S[5],S[6],S[7],S[0],i+7);
     }
#endif     


    /* feedback */
    for (i = 0; i < 8; i++) {
        ctx->state[i] = ctx->state[i] + S[i];
    }

    return CRYPT_OK;
}

/* compress 1024-bits */
#ifdef LTC_CLEAN_STACK
static int sha512_compress(ltc_sha512_ctx *ctx, const unsigned char *buf)
{
    int err;
    err = _sha512_compress(md, buf);

    ltc_burn_stack(sizeof(ulong64) * 90 + sizeof(int));
    return err;
}
#endif

/**
   Initialize the hash state
   @param md   The hash state you wish to initialize
   @return CRYPT_OK if successful
*/
int ltc_sha512_init(ltc_sha512_ctx *ctx)
{
    LTC_ARGCHK(ctx != NULL);

    ctx->curlen = 0;
    ctx->length = 0;
    ctx->state[0] = LTC_CONST64(0x6a09e667f3bcc908);
    ctx->state[1] = LTC_CONST64(0xbb67ae8584caa73b);
    ctx->state[2] = LTC_CONST64(0x3c6ef372fe94f82b);
    ctx->state[3] = LTC_CONST64(0xa54ff53a5f1d36f1);
    ctx->state[4] = LTC_CONST64(0x510e527fade682d1);
    ctx->state[5] = LTC_CONST64(0x9b05688c2b3e6c1f);
    ctx->state[6] = LTC_CONST64(0x1f83d9abfb41bd6b);
    ctx->state[7] = LTC_CONST64(0x5be0cd19137e2179);
    
    return CRYPT_OK;
}

/**
   Process a block of memory though the hash
   @param md     The hash state
   @param in     The data to hash
   @param inlen  The length of the data (octets)
   @return CRYPT_OK if successful
*/
LTC_HASH_PROCESS(ltc_sha512_process, sha512_compress, ltc_sha512_ctx, sha512, 128)

/**
   Terminate the hash to get the digest
   @param md  The hash state
   @param out [out] The destination of the hash (64 bytes)
   @return CRYPT_OK if successful
*/
int ltc_sha512_done(ltc_sha512_ctx *ctx, unsigned char *out)
{
    int i;
    LTC_ARGCHK(ctx  != NULL);
    LTC_ARGCHK(out != NULL);

    if (ctx->curlen >= sizeof(ctx->buf)) {
       return CRYPT_INVALID_ARG;
    }

    /* increase the length of the message */
    ctx->length += ctx->curlen * LTC_CONST64(8);

    /* append the '1' bit */
    ctx->buf[ctx->curlen++] = (unsigned char)0x80;

    /* if the length is currently above 112 bytes we append zeros
     * then compress.  Then we can fall back to padding zeros and length
     * encoding like normal.
     */
    if (ctx->curlen > 112) {
        while (ctx->curlen < 128) {
            ctx->buf[ctx->curlen++] = (unsigned char)0;
        }
        sha512_compress(ctx, ctx->buf);
        ctx->curlen = 0;
    }

    /* pad upto 120 bytes of zeroes 
     * note: that from 112 to 120 is the 64 MSB of the length.  We assume that
     * you won't hash > 2^64 bits of data... :-)
     */
    while (ctx->curlen < 120) {
        ctx->buf[ctx->curlen++] = (unsigned char)0;
    }

    /* store length */
    LTC_STORE64H(ctx->length, ctx->buf+120);
    sha512_compress(ctx, ctx->buf);

    /* copy output */
    for (i = 0; i < 8; i++) {
        LTC_STORE64H(ctx->state[i], out+(8*i));
    }
#ifdef LTC_CLEAN_STACK
    ltc_zeromem(md, sizeof(hash_state));
#endif
    return CRYPT_OK;
}

/**
  Self-test the hash
  @return CRYPT_OK if successful, CRYPT_NOP if self-tests have been disabled
*/  
int  ltc_sha512_test(void)
{
 #ifndef LTC_TEST
    return CRYPT_NOP;
 #else    
  static const struct {
      const char *msg;
      unsigned char hash[64];
  } tests[] = {
    { "abc",
     { 0xdd, 0xaf, 0x35, 0xa1, 0x93, 0x61, 0x7a, 0xba,
       0xcc, 0x41, 0x73, 0x49, 0xae, 0x20, 0x41, 0x31,
       0x12, 0xe6, 0xfa, 0x4e, 0x89, 0xa9, 0x7e, 0xa2,
       0x0a, 0x9e, 0xee, 0xe6, 0x4b, 0x55, 0xd3, 0x9a,
       0x21, 0x92, 0x99, 0x2a, 0x27, 0x4f, 0xc1, 0xa8,
       0x36, 0xba, 0x3c, 0x23, 0xa3, 0xfe, 0xeb, 0xbd,
       0x45, 0x4d, 0x44, 0x23, 0x64, 0x3c, 0xe8, 0x0e,
       0x2a, 0x9a, 0xc9, 0x4f, 0xa5, 0x4c, 0xa4, 0x9f }
    },
    { "abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmnhijklmnoijklmnopjklmnopqklmnopqrlmnopqrsmnopqrstnopqrstu",
     { 0x8e, 0x95, 0x9b, 0x75, 0xda, 0xe3, 0x13, 0xda,
       0x8c, 0xf4, 0xf7, 0x28, 0x14, 0xfc, 0x14, 0x3f,
       0x8f, 0x77, 0x79, 0xc6, 0xeb, 0x9f, 0x7f, 0xa1,
       0x72, 0x99, 0xae, 0xad, 0xb6, 0x88, 0x90, 0x18,
       0x50, 0x1d, 0x28, 0x9e, 0x49, 0x00, 0xf7, 0xe4,
       0x33, 0x1b, 0x99, 0xde, 0xc4, 0xb5, 0x43, 0x3a,
       0xc7, 0xd3, 0x29, 0xee, 0xb6, 0xdd, 0x26, 0x54,
       0x5e, 0x96, 0xe5, 0x5b, 0x87, 0x4b, 0xe9, 0x09 }
    },
  };

  int i;
  unsigned char tmp[64];
  ltc_hash_state md;

  for (i = 0; i < (int)(sizeof(tests) / sizeof(tests[0])); i++) {
      ltc_sha512_init(&md);
      ltc_sha512_process(&md, (const unsigned char *)tests[i].msg,
	  (unsigned long)strlen(tests[i].msg));
      ltc_sha512_done(&md, tmp);
      if (LTC_XMEMCMP(tmp, tests[i].hash, 64) != 0) {
         return CRYPT_FAIL_TESTVECTOR;
      }
  }
  return CRYPT_OK;
  #endif
}

#include "ltc_sha384.c"


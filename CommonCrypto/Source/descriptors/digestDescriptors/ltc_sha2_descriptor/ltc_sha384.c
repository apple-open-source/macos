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
/** 
   @param sha384.c
   LTC_SHA384 hash included in sha512.c, Tom St Denis
*/

const ccDescriptor ltc_sha384_desc =
{
    .implementation_info = &cc_sha384_impinfo,
	.dtype.digest.hashsize = CC_SHA384_DIGEST_LENGTH,
	.dtype.digest.blocksize = CC_SHA384_BLOCK_BYTES,
    .dtype.digest.digest_info = NULL,
	.dtype.digest.init = &ltc_sha384_init,
	.dtype.digest.process = &ltc_sha512_process,
	.dtype.digest.done = &ltc_sha384_done,
};


/**
   Initialize the hash state
   @param md   The hash state you wish to initialize
   @return CRYPT_OK if successful
*/
int ltc_sha384_init(ltc_sha512_ctx *ctx)
{
    LTC_ARGCHK(ctx != NULL);

    ctx->curlen = 0;
    ctx->length = 0;
    ctx->state[0] = LTC_CONST64(0xcbbb9d5dc1059ed8);
    ctx->state[1] = LTC_CONST64(0x629a292a367cd507);
    ctx->state[2] = LTC_CONST64(0x9159015a3070dd17);
    ctx->state[3] = LTC_CONST64(0x152fecd8f70e5939);
    ctx->state[4] = LTC_CONST64(0x67332667ffc00b31);
    ctx->state[5] = LTC_CONST64(0x8eb44a8768581511);
    ctx->state[6] = LTC_CONST64(0xdb0c2e0d64f98fa7);
    ctx->state[7] = LTC_CONST64(0x47b5481dbefa4fa4);

    return CRYPT_OK;
}

/**
   Terminate the hash to get the digest
   @param md  The hash state
   @param out [out] The destination of the hash (48 bytes)
   @return CRYPT_OK if successful
*/
int ltc_sha384_done(ltc_sha512_ctx *ctx, unsigned char *out)
{
   unsigned char buf[64];

   LTC_ARGCHK(ctx  != NULL);
   LTC_ARGCHK(out != NULL);

    if (ctx->curlen >= sizeof(ctx->buf)) {
       return CRYPT_INVALID_ARG;
    }

   ltc_sha512_done(ctx, buf);
   CC_XMEMCPY(out, buf, 48);
#ifdef LTC_CLEAN_STACK
   ltc_zeromem(buf, sizeof(buf));
#endif
   return CRYPT_OK;
}

/**
  Self-test the hash
  @return CRYPT_OK if successful, CRYPT_NOP if self-tests have been disabled
*/  
int ltc_sha384_test(void)
{
 #ifndef LTC_TEST
    return CRYPT_NOP;
 #else    
  static const struct {
      const char *msg;
      unsigned char hash[48];
  } tests[] = {
    { "abc",
      { 0xcb, 0x00, 0x75, 0x3f, 0x45, 0xa3, 0x5e, 0x8b,
        0xb5, 0xa0, 0x3d, 0x69, 0x9a, 0xc6, 0x50, 0x07,
        0x27, 0x2c, 0x32, 0xab, 0x0e, 0xde, 0xd1, 0x63,
        0x1a, 0x8b, 0x60, 0x5a, 0x43, 0xff, 0x5b, 0xed,
        0x80, 0x86, 0x07, 0x2b, 0xa1, 0xe7, 0xcc, 0x23,
        0x58, 0xba, 0xec, 0xa1, 0x34, 0xc8, 0x25, 0xa7 }
    },
    { "abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmnhijklmnoijklmnopjklmnopqklmnopqrlmnopqrsmnopqrstnopqrstu",
      { 0x09, 0x33, 0x0c, 0x33, 0xf7, 0x11, 0x47, 0xe8,
        0x3d, 0x19, 0x2f, 0xc7, 0x82, 0xcd, 0x1b, 0x47,
        0x53, 0x11, 0x1b, 0x17, 0x3b, 0x3b, 0x05, 0xd2,
        0x2f, 0xa0, 0x80, 0x86, 0xe3, 0xb0, 0xf7, 0x12,
        0xfc, 0xc7, 0xc7, 0x1a, 0x55, 0x7e, 0x2d, 0xb9,
        0x66, 0xc3, 0xe9, 0xfa, 0x91, 0x74, 0x60, 0x39 }
    },
  };

  int i;
  unsigned char tmp[48];
  ltc_hash_state md;

  for (i = 0; i < (int)(sizeof(tests) / sizeof(tests[0])); i++) {
      ltc_sha384_init(&md);
      ltc_sha384_process(&md, (const unsigned char*)tests[i].msg,
	  (unsigned long)strlen(tests[i].msg));
      ltc_sha384_done(&md, tmp);
      if (LTC_XMEMCMP(tmp, tests[i].hash, 48) != 0) {
         return CRYPT_FAIL_TESTVECTOR;
      }
  }
  return CRYPT_OK;
 #endif
}

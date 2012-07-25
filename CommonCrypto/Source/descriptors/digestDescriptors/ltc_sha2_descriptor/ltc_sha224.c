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
   @param sha224.c
   LTC_SHA-224 new NIST standard based off of LTC_SHA-256 truncated to 224 bits (Tom St Denis)
*/

const ccDescriptor ltc_sha224_desc =
{
    .implementation_info = &cc_sha224_impinfo,
	.dtype.digest.hashsize = CC_SHA224_DIGEST_LENGTH,
	.dtype.digest.blocksize = CC_SHA224_BLOCK_BYTES,
    .dtype.digest.digest_info = NULL,
	.dtype.digest.init = &ltc_sha224_init,
	.dtype.digest.process = &ltc_sha256_process,
	.dtype.digest.done = &ltc_sha224_done,
};


/* init the sha256 er... sha224 state ;-) */
/**
   Initialize the hash state
   @param md   The hash state you wish to initialize
   @return CRYPT_OK if successful
*/
int ltc_sha224_init(ltc_sha256_ctx *ctx)
{
    LTC_ARGCHK(ctx != NULL);

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
int ltc_sha224_done(ltc_sha256_ctx *ctx, unsigned char *out)
{
    unsigned char buf[32];
    int err;

    LTC_ARGCHK(ctx  != NULL);
    LTC_ARGCHK(out != NULL);

    err = ltc_sha256_done(ctx, buf);
    CC_XMEMCPY(out, buf, 28);
#ifdef LTC_CLEAN_STACK
    ltc_zeromem(buf, sizeof(buf));
#endif 
    return err;
}

/**
  Self-test the hash
  @return CRYPT_OK if successful, CRYPT_NOP if self-tests have been disabled
*/  
int  ltc_sha224_test(void)
{
 #ifndef LTC_TEST
    return CRYPT_NOP;
 #else    
  static const struct {
      const char *msg;
      unsigned char hash[28];
  } tests[] = {
    { "abc",
      { 0x23, 0x09, 0x7d, 0x22, 0x34, 0x05, 0xd8,
        0x22, 0x86, 0x42, 0xa4, 0x77, 0xbd, 0xa2,
        0x55, 0xb3, 0x2a, 0xad, 0xbc, 0xe4, 0xbd,
        0xa0, 0xb3, 0xf7, 0xe3, 0x6c, 0x9d, 0xa7 }
    },
    { "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq",
      { 0x75, 0x38, 0x8b, 0x16, 0x51, 0x27, 0x76,
        0xcc, 0x5d, 0xba, 0x5d, 0xa1, 0xfd, 0x89,
        0x01, 0x50, 0xb0, 0xc6, 0x45, 0x5c, 0xb4,
        0xf5, 0x8b, 0x19, 0x52, 0x52, 0x25, 0x25 }
    },
  };

  int i;
  unsigned char tmp[28];
  ltc_hash_state md;

  for (i = 0; i < (int)(sizeof(tests) / sizeof(tests[0])); i++) {
      ltc_sha224_init(&md);
      ltc_sha224_process(&md, (const unsigned char*)tests[i].msg,
	  (unsigned long)strlen(tests[i].msg));
      ltc_sha224_done(&md, tmp);
      if (LTC_XMEMCMP(tmp, tests[i].hash, 28) != 0) {
         return CRYPT_FAIL_TESTVECTOR;
      }
  }
  return CRYPT_OK;
 #endif
}

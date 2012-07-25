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
#include "ltc_md2.h"
#include "tomcrypt_cfg.h"
#include "tomcrypt_macros.h"
#include "tomcrypt_argchk.h"
#include "ccDescriptors.h"
#include "ccErrors.h"
#include "ccMemory.h"
#include "CommonDigest.h"

/**
 @param ltc_md2.c
 LTC_MD2 (RFC 1319) hash function implementation by Tom St Denis 
 */


const ccDescriptor ltc_md2_desc = {
    .implementation_info = &cc_md2_impinfo,
	.dtype.digest.hashsize = CC_MD2_DIGEST_LENGTH,
	.dtype.digest.blocksize = CC_MD2_BLOCK_BYTES,
    .dtype.digest.digest_info = NULL,
	.dtype.digest.init = &ltc_md2_init,
	.dtype.digest.process = &ltc_md2_process,
	.dtype.digest.done = &ltc_md2_done,
};

static const unsigned char PI_SUBST[256] = {
    41, 46, 67, 201, 162, 216, 124, 1, 61, 54, 84, 161, 236, 240, 6,
    19, 98, 167, 5, 243, 192, 199, 115, 140, 152, 147, 43, 217, 188,
    76, 130, 202, 30, 155, 87, 60, 253, 212, 224, 22, 103, 66, 111, 24,
    138, 23, 229, 18, 190, 78, 196, 214, 218, 158, 222, 73, 160, 251,
    245, 142, 187, 47, 238, 122, 169, 104, 121, 145, 21, 178, 7, 63,
    148, 194, 16, 137, 11, 34, 95, 33, 128, 127, 93, 154, 90, 144, 50,
    39, 53, 62, 204, 231, 191, 247, 151, 3, 255, 25, 48, 179, 72, 165,
    181, 209, 215, 94, 146, 42, 172, 86, 170, 198, 79, 184, 56, 210,
    150, 164, 125, 182, 118, 252, 107, 226, 156, 116, 4, 241, 69, 157,
    112, 89, 100, 113, 135, 32, 134, 91, 207, 101, 230, 45, 168, 2, 27,
    96, 37, 173, 174, 176, 185, 246, 28, 70, 97, 105, 52, 64, 126, 15,
    85, 71, 163, 35, 221, 81, 175, 58, 195, 92, 249, 206, 186, 197,
    234, 38, 44, 83, 13, 110, 133, 40, 132, 9, 211, 223, 205, 244, 65,
    129, 77, 82, 106, 220, 55, 200, 108, 193, 171, 250, 36, 225, 123,
    8, 12, 189, 177, 74, 120, 136, 149, 139, 227, 99, 232, 109, 233,
    203, 213, 254, 59, 0, 29, 57, 242, 239, 183, 14, 102, 88, 208, 228,
    166, 119, 114, 248, 235, 117, 75, 10, 49, 68, 80, 180, 143, 237,
    31, 26, 219, 153, 141, 51, 159, 17, 131, 20
};

/* adds 16 bytes to the checksum */
static void md2_update_chksum(ltc_md2_ctx *ctx)
{
    int j;
    unsigned char L;

    LTC_ARGCHKVD(ctx != NULL);
    
    L = ctx->chksum[15];
    for (j = 0; j < 16; j++) {
        
        /* caution, the RFC says its "C[j] = S[M[i*16+j] xor L]" but the
	 * reference source code [and test vectors] say otherwise.
         */
        L = (ctx->chksum[j] ^= PI_SUBST[(int)(ctx->buf[j] ^ L)] & 255);
    }
}

static void md2_compress(ltc_md2_ctx *ctx)
{
    int j, k;
    unsigned char t;

    LTC_ARGCHKVD(ctx != NULL);

    /* copy block */
    for (j = 0; j < 16; j++) {
        ctx->X[16+j] = ctx->buf[j];
        ctx->X[32+j] = ctx->X[j] ^ ctx->X[16+j];
    }
    
    t = (unsigned char)0;
    
    /* do 18 rounds */
    for (j = 0; j < 18; j++) {
        for (k = 0; k < 48; k++) {
            t = (ctx->X[k] ^= PI_SUBST[(int)(t & 255)]);
        }
        t = (t + (unsigned char)j) & 255;
    }
}

/**
 Initialize the hash state
 @param md   The hash state you wish to initialize
 @return CRYPT_OK if successful
 */
int ltc_md2_init(ltc_md2_ctx *ctx)
{
    
    LTC_ARGCHK(ctx != NULL);

    /* LTC_MD2 uses a zero'ed state... */
    CC_XZEROMEM(ctx->X, sizeof(ctx->X));
    CC_XZEROMEM(ctx->chksum, sizeof(ctx->chksum));
    CC_XZEROMEM(ctx->buf, sizeof(ctx->buf));
    ctx->curlen = 0;
    return CRYPT_OK;
}

/**
 Process a block of memory though the hash
 @param md     The hash state
 @param in     The data to hash
 @param inlen  The length of the data (octets)
 @return CRYPT_OK if successful
 */
int ltc_md2_process(ltc_md2_ctx *ctx, const unsigned char *in,
    unsigned long inlen)
{
    unsigned long n;

    LTC_ARGCHK(ctx != NULL);
    LTC_ARGCHK(in != NULL);

    if (ctx->curlen > sizeof(ctx->buf)) {                            
        return CRYPT_INVALID_ARG; 
    }                            

    while (inlen > 0) {
        n = MIN(inlen, (16 - ctx->curlen));
        CC_XMEMCPY(ctx->buf + ctx->curlen, in, (size_t)n);
        ctx->curlen += n;
        in             += n;
        inlen          -= n;
        
        /* is 16 bytes full? */
        if (ctx->curlen == 16) {
            md2_compress(ctx);
            md2_update_chksum(ctx);
            ctx->curlen = 0;
        }
    }
    return CRYPT_OK;
}

/**
 Terminate the hash to get the digest
 @param md  The hash state
 @param out [out] The destination of the hash (16 bytes)
 @return CRYPT_OK if successful
 */
int ltc_md2_done(ltc_md2_ctx *ctx, unsigned char *out)
{
    unsigned long i, k;
    
    LTC_ARGCHK(ctx  != NULL);
    LTC_ARGCHK(out != NULL);
        
    if (ctx->curlen >= sizeof(ctx->buf)) {
        return CRYPT_INVALID_ARG;
    }
    
    
    /* pad the message */
    k = 16 - ctx->curlen;
    for (i = ctx->curlen; i < 16; i++) {
        ctx->buf[i] = (unsigned char)k;
    }
    
    /* hash and update */
    md2_compress(ctx);
    md2_update_chksum(ctx);
    
    /* hash checksum */
    CC_XMEMCPY(ctx->buf, ctx->chksum, 16);
    md2_compress(ctx);
    
    /* output is lower 16 bytes of X */
    CC_XMEMCPY(out, ctx->X, 16);
    
#ifdef LTC_CLEAN_STACK
    CC_XZEROMEM(ctx, sizeof(hash_state));
#endif
    return CRYPT_OK;
}

/**
 Self-test the hash
 @return CRYPT_OK if successful, CRYPT_NOP if self-tests have been disabled
 */  
int ltc_md2_test(void)
{
#ifndef LTC_TEST
    return CRYPT_NOP;
#else    
    static const struct {
        const char *msg;
        unsigned char md[16];
    } tests[] = {
        { "",
            {0x83,0x50,0xe5,0xa3,0xe2,0x4c,0x15,0x3d,
                0xf2,0x27,0x5c,0x9f,0x80,0x69,0x27,0x73
            }
        },
        { "a",
            {0x32,0xec,0x01,0xec,0x4a,0x6d,0xac,0x72,
                0xc0,0xab,0x96,0xfb,0x34,0xc0,0xb5,0xd1
            }
        },
        { "message digest",
            {0xab,0x4f,0x49,0x6b,0xfb,0x2a,0x53,0x0b,
                0x21,0x9f,0xf3,0x30,0x31,0xfe,0x06,0xb0
            }
        },
        { "abcdefghijklmnopqrstuvwxyz",
            {0x4e,0x8d,0xdf,0xf3,0x65,0x02,0x92,0xab,
                0x5a,0x41,0x08,0xc3,0xaa,0x47,0x94,0x0b
            }
        },
        { "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789",
            {0xda,0x33,0xde,0xf2,0xa4,0x2d,0xf1,0x39,
                0x75,0x35,0x28,0x46,0xc3,0x03,0x38,0xcd
            }
        },
        { "12345678901234567890123456789012345678901234567890123456789012345678901234567890",
            {0xd5,0x97,0x6f,0x79,0xd8,0x3d,0x3a,0x0d,
                0xc9,0x80,0x6c,0x3c,0x66,0xf3,0xef,0xd8
            }
        }
    };
    int i;
    ltc_hash_state md;
    unsigned char buf[16];
    
    for (i = 0; i < (int)(sizeof(tests) / sizeof(tests[0])); i++) {
        ltc_md2_init(&md);
        ltc_md2_process(&md, (const unsigned char*)tests[i].msg, (unsigned long)strlen(tests[i].msg));
        ltc_md2_done(&md, buf);
        if (LTC_XMEMCMP(buf, tests[i].md, 16) != 0) {
            return CRYPT_FAIL_TESTVECTOR;
        }
    }
    return CRYPT_OK;        
#endif /* LTC_TEST */
}


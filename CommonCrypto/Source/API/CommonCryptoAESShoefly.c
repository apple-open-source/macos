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
/*
 *  CommonCryptoAESShoefly.c
 *  CommonCrypto
 *
 *  Shim for Diskimages to bridge to a non-hw based aes-cbc
 *
 */

// #define COMMON_AESSHOEFLY_FUNCTIONS
#define CC_Building
#include "aes.h"

#include "ccdebug.h"


void aes_encrypt_key128(const unsigned char *in_key, aes_encrypt_ctx cx[1])
{
    CCCryptorRef encCryptorRef;
    aes_encrypt_ctx *ctx = cx;
    size_t dataUsed;
    
    CC_DEBUG_LOG(ASL_LEVEL_ERR, "Entering\n");

    (void) CCCryptorCreateFromDataWithMode(kCCEncrypt, kCCModeCBC, kCCAlgorithmAES128NoHardware, ccNoPadding, NULL, in_key, 16, NULL, 0, 0, 0, 
    	&ctx->ctx, kCCContextSizeGENERIC, &ctx->cref, &dataUsed);
}

void aes_encrypt_key256(const unsigned char *in_key, aes_encrypt_ctx cx[1])
{
    CCCryptorRef encCryptorRef;
    aes_encrypt_ctx *ctx = cx;
    size_t dataUsed;
    
    CC_DEBUG_LOG(ASL_LEVEL_ERR, "Entering\n");
    (void) CCCryptorCreateFromDataWithMode(kCCEncrypt, kCCModeCBC, kCCAlgorithmAES128NoHardware, ccNoPadding, NULL, in_key, 32, NULL, 0, 0, 0, 
                                           &ctx->ctx, kCCContextSizeGENERIC, &ctx->cref, &dataUsed);
}

void aes_decrypt_key128(const unsigned char *in_key, aes_decrypt_ctx cx[1])
{
    CCCryptorRef encCryptorRef;
    aes_encrypt_ctx *ctx = cx;
    size_t dataUsed;
    
    CC_DEBUG_LOG(ASL_LEVEL_ERR, "Entering\n");
    (void) CCCryptorCreateFromDataWithMode(kCCDecrypt, kCCModeCBC, kCCAlgorithmAES128NoHardware, ccNoPadding, NULL, in_key, 16, NULL, 0, 0, 0, 
                                           &ctx->ctx, kCCContextSizeGENERIC, &ctx->cref, &dataUsed);
}

void aes_decrypt_key256(const unsigned char *in_key, aes_decrypt_ctx cx[1])
{
    CCCryptorRef encCryptorRef;
    aes_encrypt_ctx *ctx = cx;
    size_t dataUsed;
    
    CC_DEBUG_LOG(ASL_LEVEL_ERR, "Entering\n");
    (void) CCCryptorCreateFromDataWithMode(kCCDecrypt, kCCModeCBC, kCCAlgorithmAES128NoHardware, ccNoPadding, NULL, in_key, 32, NULL, 0, 0, 0, 
                                           &ctx->ctx, kCCContextSizeGENERIC, &ctx->cref, &dataUsed);
}


void aes_encrypt_cbc(const unsigned char *in_blk, const unsigned char *in_iv, unsigned int num_blk,
                     unsigned char *out_blk, aes_encrypt_ctx cx[1])
{
    aes_encrypt_ctx *ctx = cx;
    CC_DEBUG_LOG(ASL_LEVEL_ERR, "Entering\n");
	(void) CCCryptorEncryptDataBlock(ctx->cref, in_iv, in_blk, num_blk * AES_BLOCK_SIZE, out_blk);
}


void aes_decrypt_cbc(const unsigned char *in_blk, const unsigned char *in_iv, unsigned int num_blk,
                     unsigned char *out_blk, aes_decrypt_ctx cx[1])
{
    aes_encrypt_ctx *ctx = cx;
    CC_DEBUG_LOG(ASL_LEVEL_ERR, "Entering\n");
	(void) CCCryptorDecryptDataBlock(ctx->cref, in_iv, in_blk, num_blk * AES_BLOCK_SIZE, out_blk);
}


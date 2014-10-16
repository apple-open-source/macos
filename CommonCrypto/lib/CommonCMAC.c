/* 
 * Copyright (c) 2011 Apple Computer, Inc. All Rights Reserved.
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

// #define COMMON_CMAC_FUNCTIONS

#include "CommonCMACSPI.h"
#include "CommonCryptorPriv.h"
#include <corecrypto/ccaes.h>
#include "ccdebug.h"
#include "ccMemory.h"

#if 0

/* Internal functions to support one-shot */

const uint8_t const_Rb[16] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x87
};



static void leftshift_onebit(uint8_t *input, uint8_t *output)
{
    int		i;
    uint8_t	overflow = 0;
    
    for ( i=15; i>=0; i-- ) {
        output[i] = input[i] << 1;
        output[i] |= overflow;
        overflow = (input[i] & 0x80)?1:0;
    }
    return;
}

static void xor_128(const uint8_t *a, const uint8_t *b, uint8_t *out)
{
    int i;
    for (i=0;i<16; i++) out[i] = a[i] ^ b[i];
}


static void ccGenAESSubKey(const struct ccmode_ecb *aesmode, ccecb_ctx *ctx, void *key1, void *key2)
{
    uint8_t L[16];
    uint8_t Z[16];
    uint8_t tmp[16];
    
	memset(Z, 0, 16);
    
    aesmode->ecb(ctx, 1, Z, L);
    
    if ( (L[0] & 0x80) == 0 ) { /* If MSB(L) = 0, then K1 = L << 1 */
        leftshift_onebit(L, key1);
    } else {    /* Else K1 = ( L << 1 ) (+) Rb */
        leftshift_onebit(L, tmp);
        xor_128(tmp,const_Rb, key1);
    }
    
    if ( (((uint8_t *)key1)[0] & 0x80) == 0 ) {
        leftshift_onebit(key1, key2);
    } else {
        leftshift_onebit(key1, tmp);
        xor_128(tmp,const_Rb, key2);
    }
    return;
    
}

static void ccAESCMacPadding (const uint8_t *lastb, uint8_t *pad, int length)
{
    int         j;
    
    for ( j=0; j<16; j++ ) {
        if ( j < length ) pad[j] = lastb[j];
        else if ( j == length ) pad[j] = 0x80;
        else pad[j] = 0x00;
    }
}

/* This would be the one-shot CMAC interface */

void CCAESCmac(const void *key,
               const uint8_t *data,
               size_t dataLength,			/* length of data in bytes */
               void *macOut)				/* MAC written here */
{
    uint8_t       X[16],Y[16], M_last[16], padded[16];
    uint8_t       K1[16], K2[16];
    int         flag;
    size_t      n;
    const struct ccmode_ecb *aesmode = getCipherMode(kCCAlgorithmAES128, kCCModeECB, kCCEncrypt).ecb;
    ccecb_ctx_decl(aesmode->size, ctx);

    CC_DEBUG_LOG(ASL_LEVEL_ERR, "Entering\n");

	// CMacInit
    
    aesmode->init(aesmode, ctx, 16, key);
    aesmode->ecb(ctx, 1, Y, X);
    ccGenAESSubKey(aesmode, ctx, K1, K2);
    
    // CMacUpdates (all in this case)
    
    n = (dataLength+15) / 16;       /* n is number of rounds */
    
    if ( 0 == n ) {
        n = 1;
        flag = 0;
    } else {
        if ( (dataLength%16) == 0 ) flag = 1;
        else  flag = 0;
    }
    
    if ( flag ) { /* last block is complete block */
        xor_128(&data[16*(n-1)],K1,M_last);
    } else {
        ccAESCMacPadding(&data[16*(n-1)],padded,dataLength%16);
        xor_128(padded,K2,M_last);
    }
    
    memset(X, 0, 16);
    for (size_t i=0; i<n-1; i++ ) {
        xor_128(X,&data[16*i],Y); /* Y := Mi (+) X  */
        aesmode->ecb(ctx, 1, Y, X);
    }
    
    // CMacFinal
    
    xor_128(X,M_last,Y);
    aesmode->ecb(ctx, 1, Y, X);
    
    memcpy(macOut, X, 16);
}

#else
#include <corecrypto/cccmac.h>
#include <corecrypto/ccaes.h>

void CCAESCmac(const void *key,
               const uint8_t *data,
               size_t dataLength,			/* length of data in bytes */
               void *macOut)				/* MAC written here */
{
    cccmac(ccaes_cbc_encrypt_mode(), key, dataLength, data, macOut);
}

struct CCCmacContext {
    const struct ccmode_cbc *cbc;
    cccmac_ctx_t ctxptr;
    size_t pos;
    uint8_t buf[16];
};

CCCmacContextPtr
CCAESCmacCreate(const void *key, size_t keyLength)
{
    CCCmacContextPtr retval = (CCCmacContextPtr) CC_XMALLOC(sizeof(struct CCCmacContext));
    if(!retval) return NULL;
    retval->cbc = ccaes_cbc_encrypt_mode();
    retval->ctxptr.b = CC_XMALLOC(cccmac_ctx_size(retval->cbc));
    retval->pos = 0;
    if(!retval->ctxptr.b) {
        CC_XFREE(retval, cccmac_ctx_size(retval->cbc));
        return NULL;
    }
    cccmac_init(retval->cbc, retval->ctxptr, key);
    
    return retval;
}

void CCAESCmacUpdate(CCCmacContextPtr ctx, const void *data, size_t dataLength) {
    size_t blocksize = ctx->cbc->block_size;
    // Need to have some data for final - so don't process all available data - even if it's even blocks
    while(dataLength) {
        if(ctx->pos == blocksize) { // flush what we have - there's more
            cccmac_block_update(ctx->cbc, ctx->ctxptr, 1, ctx->buf);
            ctx->pos = 0;
        } else if (ctx->pos == 0 && dataLength > blocksize) {
            size_t fullblocks = ((dataLength + blocksize - 1) / blocksize) - 1;
            cccmac_block_update(ctx->cbc, ctx->ctxptr, fullblocks, data);
            size_t nbytes = fullblocks * blocksize;
            dataLength -= nbytes; data += nbytes;
        } else {
            size_t n = CC_XMIN(dataLength, (blocksize - ctx->pos));
            CC_XMEMCPY(&ctx->buf[ctx->pos], data, n);
            ctx->pos += n; dataLength -= n; data += n;
        }
    }
}

void CCAESCmacFinal(CCCmacContextPtr ctx, void *macOut) {
    cccmac_final(ctx->cbc, ctx->ctxptr, ctx->pos, ctx->buf, macOut);
}

void CCAESCmacDestroy(CCCmacContextPtr ctx) {
    if(ctx) {
        CC_BZERO(ctx->buf, 16);
        if(!ctx->ctxptr.b) CC_XFREE(ctx->ctxptr.b, cccmac_ctx_size(retval->cbc));
        CC_XFREE(ctx, sizeof(struct CCCmacContext));
    }
}

size_t
CCAESCmacOutputSizeFromContext(CCCmacContextPtr ctx) {
    return ctx->cbc->block_size;
}

#endif

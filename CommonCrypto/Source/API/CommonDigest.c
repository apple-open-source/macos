/*
 * Copyright (c) 2012 Apple Inc. All Rights Reserved.
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

// #define COMMON_DIGEST_FUNCTIONS
#define COMMON_DIGEST_FOR_RFC_1321

#include "CommonDigest.h"
#include "CommonDigestPriv.h"
#include "CommonDigestSPI.h"
#include "ccErrors.h"
#include "ccMemory.h"
#include "ccdebug.h"
#include <stdio.h>
#include <dispatch/dispatch.h>
#include <dispatch/queue.h>
#include <corecrypto/ccmd2.h>
#include <corecrypto/ccmd4.h>
#include <corecrypto/ccmd5.h>
#include <corecrypto/ccripemd.h>
#include <corecrypto/ccsha1.h>
#include <corecrypto/ccsha2.h>

#define NDEBUG
#ifndef	NDEBUG
#define ASSERT(s)
#else
#define ASSERT(s)	assert(s)
#endif

static const size_t diMax = kCCDigestSkein512+1;
static struct ccdigest_info *di[diMax];

// This returns a pointer to the corecrypto "di" structure for a digest.
// It's used for all functions that need a di (HMac, Key Derivation, etc).

struct ccdigest_info *
CCDigestGetDigestInfo(CCDigestAlgorithm algorithm) {
    static dispatch_once_t di_init;

    dispatch_once(&di_init, ^{
        di[kCCDigestNone] = NULL;
        di[kCCDigestMD2] = &ccmd2_di;
        di[kCCDigestMD4] = &ccmd4_di;
        di[kCCDigestMD5] = ccmd5_di();
        di[kCCDigestRMD128] = &ccrmd128_di;
        di[kCCDigestRMD160] = &ccrmd160_di;
        di[kCCDigestRMD256] = &ccrmd256_di;
        di[kCCDigestRMD320] = &ccrmd320_di;
        di[kCCDigestSHA1] = ccsha1_di();
        di[kCCDigestSHA224] = ccsha224_di();
        di[kCCDigestSHA256] = ccsha256_di();
        di[kCCDigestSHA384] = ccsha384_di();
        di[kCCDigestSHA512] = ccsha512_di();
        di[kCCDigestSkein128] = NULL;
        di[kCCDigestSkein160] = NULL;
        di[15] = NULL; // gap
        di[kCCDigestSkein224] = NULL;
        di[kCCDigestSkein256] = NULL;
        di[kCCDigestSkein384] = NULL;
        di[kCCDigestSkein512] = NULL;
    });
    return di[algorithm];
}

    
int 
CCDigestInit(CCDigestAlgorithm alg, CCDigestRef c)
{
    if(alg == 0 || alg >= diMax) return kCCParamError;
    if(!c) return kCCParamError;
    
    CC_DEBUG_LOG(ASL_LEVEL_ERR, "Entering Algorithm: %d\n", alg);
    CCDigestCtxPtr p = (CCDigestCtxPtr) c;

    if(p->di = CCDigestGetDigestInfo(alg)) {
        ccdigest_init(p->di, (struct ccdigest_ctx *) p->md);
		return 0;
    } else {
        return kCCUnimplemented;
    }
}

int
CCDigestUpdate(CCDigestRef c, const void *data, size_t len)
{
    // CC_DEBUG_LOG(ASL_LEVEL_ERR, "Entering\n");
    if(c == NULL) return kCCParamError;
    if(len == 0) return kCCSuccess;
    if(data == NULL) return kCCParamError; /* this is only a problem if len > 0 */
    CCDigestCtxPtr p = (CCDigestCtxPtr) c;
    if(p->di) {
        ccdigest_update(p->di, (struct ccdigest_ctx *) p->md, len, data);
        return kCCSuccess;
    }
    return kCCUnimplemented;
}

int
CCDigestFinal(CCDigestRef c, uint8_t *out)
{
    // CC_DEBUG_LOG(ASL_LEVEL_ERR, "Entering\n");
	if(c == NULL || out == NULL) return kCCParamError;
	CCDigestCtxPtr p = (CCDigestCtxPtr) c;
    if(p->di) {
        ccdigest_final(p->di, (struct ccdigest_ctx *) p->md, out);
        return 0;
    }
    return kCCUnimplemented;
}

int
CCDigest(CCDigestAlgorithm alg, const uint8_t *data, size_t len, uint8_t *out)
{
	CCDigestCtx_t c;
    int retval;
    struct ccdigest_info *di;
    
    CC_DEBUG_LOG(ASL_LEVEL_ERR, "Entering Algorithm: %d\n", alg);
    if(di = CCDigestGetDigestInfo(alg)) {
        ccdigest(di, len, data, out);
        return 0;
    }
    return kCCUnimplemented;
}

size_t
CCDigestGetBlockSize(CCDigestAlgorithm algorithm) 
{
    CC_DEBUG_LOG(ASL_LEVEL_ERR, "Entering Algorithm: %d\n", algorithm);
    struct ccdigest_info *di = CCDigestGetDigestInfo(algorithm);
    if(di) return di->block_size;
    return kCCUnimplemented;
}

size_t
CCDigestGetOutputSize(CCDigestAlgorithm algorithm)
{
    CC_DEBUG_LOG(ASL_LEVEL_ERR, "Entering Algorithm: %d\n", algorithm);
    struct ccdigest_info *di = CCDigestGetDigestInfo(algorithm);
    if(di) return di->output_size;
    return kCCUnimplemented;
}

size_t
CCDigestGetBlockSizeFromRef(CCDigestRef ctx) 
{
    // CC_DEBUG_LOG(ASL_LEVEL_ERR, "Entering\n");
    CCDigestCtxPtr p = (CCDigestCtxPtr) ctx;
    if(p->di) return p->di->block_size;
    return kCCUnimplemented;
}

size_t
CCDigestBlockSize(CCDigestRef ctx)
{
    // CC_DEBUG_LOG(ASL_LEVEL_ERR, "Entering\n");
    return CCDigestGetBlockSizeFromRef(ctx);
}

size_t
CCDigestOutputSize(CCDigestRef ctx)
{
    // CC_DEBUG_LOG(ASL_LEVEL_ERR, "Entering\n");
    return CCDigestGetOutputSizeFromRef(ctx);
}

size_t
CCDigestGetOutputSizeFromRef(CCDigestRef ctx)
{
    // CC_DEBUG_LOG(ASL_LEVEL_ERR, "Entering\n");
    CCDigestCtxPtr p = (CCDigestCtxPtr) ctx;
    if(p->di) return p->di->output_size;
    return kCCUnimplemented;
}





CCDigestRef
CCDigestCreate(CCDigestAlgorithm alg)
{
	CCDigestCtxPtr retval = CC_XMALLOC(sizeof(CCDigestCtx_t));
    
    // CC_DEBUG_LOG(ASL_LEVEL_ERR, "Entering\n");
    if(!retval) return NULL;
    if(CCDigestInit(alg, retval)) {
    	CC_XFREE(retval, sizeof(CCDigestCtx_t));
    	return NULL;
    }
    return retval;
}


uint8_t *
CCDigestOID(CCDigestRef ctx)
{
    CC_DEBUG_LOG(ASL_LEVEL_ERR, "Entering\n");
    CCDigestCtxPtr p = (CCDigestCtxPtr) ctx;
	return p->di->oid;
}

size_t
CCDigestOIDLen(CCDigestRef ctx)
{
    CC_DEBUG_LOG(ASL_LEVEL_ERR, "Entering\n");
    CCDigestCtxPtr p = (CCDigestCtxPtr) ctx;
	return p->di->oid_size;
}

CCDigestRef
CCDigestCreateByOID(uint8_t *OID, size_t OIDlen)
{    
    CC_DEBUG_LOG(ASL_LEVEL_ERR, "Entering\n");
    for(int i=kCCDigestMD2; i<diMax; i++) {
        struct ccdigest_info *di = CCDigestGetDigestInfo(i);
        if(di && (OIDlen == di->oid_size) && (CC_XMEMCMP(OID, di->oid, OIDlen) == 0))
            return CCDigestCreate(i);
    }
    return NULL;
}

void
CCDigestReset(CCDigestRef ctx)
{
    CC_DEBUG_LOG(ASL_LEVEL_ERR, "Entering\n");
    CCDigestCtxPtr p = (CCDigestCtxPtr) ctx;
    if(p->di) ccdigest_init(p->di, (struct ccdigest_ctx *) p->md);
}


void
CCDigestDestroy(CCDigestRef ctx)
{
    // CC_DEBUG_LOG(ASL_LEVEL_ERR, "Entering\n");
	if(ctx) {
		CC_XZEROMEM(ctx, sizeof(CCDigestCtx_t));
		CC_XFREE(ctx, sizeof(CCDigestCtx_t));
    }
}
/*
 * Legacy CommonDigest API shims.
 */

#define CC_COMPAT_DIGEST_RETURN 1

#define DIGEST_SHIMS(_name_,_constant_) \
\
static size_t CC_##_name_##_Len = 0; \
static size_t CC_##_name_##_Ctr = 0; \
\
int CC_##_name_##_Init(CC_##_name_##_CTX *c) { \
    struct ccdigest_info *di = CCDigestGetDigestInfo(_constant_); \
    ASSERT(sizeof(CC_##_name_##_CTX) <= ccdigest_di_size(di)); \   
    ccdigest_init(CCDigestGetDigestInfo(_constant_), (struct ccdigest_ctx *) c); \
	return 1; \
} \
 \
int \
CC_##_name_##_Update(CC_##_name_##_CTX *c, const void *data, CC_LONG len) \
{ \
    CC_##_name_##_Len += len; \
    ccdigest_update(CCDigestGetDigestInfo(_constant_), (struct ccdigest_ctx *) c, len, data); \
	return 1; \
} \
 \
int \
CC_##_name_##_Final(unsigned char *md, CC_##_name_##_CTX *c) \
{ \
    if(((++CC_##_name_##_Ctr) % 50) == 0) CC_DEBUG_LOG(ASL_LEVEL_ERR, "Len = %lu\n", CC_##_name_##_Len); \
    ccdigest_final(CCDigestGetDigestInfo(_constant_), (struct ccdigest_ctx *) c, md); \
	return 1; \
} \
 \
unsigned char * \
CC_##_name_ (const void *data, CC_LONG len, unsigned char *md) \
{ \
	(void) CCDigest(_constant_, data, len, md); \
	return md; \
}


#define DIGEST_FINAL_SHIMS(_name_,_constant_) \
unsigned char * \
CC_##_name_ (const void *data, CC_LONG len, unsigned char *md) \
{ \
(void) CCDigest(_constant_, data, len, md); \
return md; \
}



DIGEST_FINAL_SHIMS(MD2, kCCDigestMD2)
DIGEST_SHIMS(MD4, kCCDigestMD4)
DIGEST_SHIMS(MD5, kCCDigestMD5)
DIGEST_SHIMS(SHA1, kCCDigestSHA1)
DIGEST_FINAL_SHIMS(SHA224, kCCDigestSHA224)
DIGEST_FINAL_SHIMS(SHA256, kCCDigestSHA256)
DIGEST_FINAL_SHIMS(SHA384, kCCDigestSHA384)
DIGEST_FINAL_SHIMS(SHA512, kCCDigestSHA512)


#define MD5_CTX                     CC_MD5_CTX
void MD5Final(unsigned char md[16], MD5_CTX *c)
{
    CC_DEBUG_LOG(ASL_LEVEL_ERR, "Entering\n");
    (void) CC_MD5_Final(md, c);
}

static void
ccdigest_process(struct ccdigest_info *di, uint8_t *bufptr, ccdigest_state_t state,
                 uint64_t curlen, size_t len, uint8_t *data)
{
    while(len) { 
        if (curlen == 0 && len >= di->block_size) {
            uint64_t fullblocks = len / di->block_size;
            di->compress(state, fullblocks, data);
            uint64_t nbytes = fullblocks * di->block_size;
            len -= nbytes; data += nbytes;
        } else {
            uint64_t n = CC_XMIN(len, (di->block_size - curlen)); 
            CC_XMEMCPY(bufptr + curlen, data, n); 
            curlen += n; len -= n; data += n;
            if (curlen == di->block_size) {
                di->compress(state, 1, bufptr);
                curlen = 0; 
            }
        } 
    }
}

static void
ccdigest_finalize(struct ccdigest_info *di, uint8_t *bufptr, ccdigest_state_t state, 
                  uint64_t curlen, uint64_t totalLen)
{
    bufptr[curlen++] = (unsigned char)0x80;
    int reserve = 8;
    if(di->block_size == 128) reserve = 16; // SHA384/512 reserves 16 bytes below.
    
    /* if the length is currently above block_size - reserve bytes we append zeros
     * then compress.  Then we can fall back to padding zeros and length
     * encoding like normal.
     */
    
    if (curlen > (di->block_size - reserve)) {
        while (curlen < di->block_size) bufptr[curlen++] = (unsigned char)0;
        di->compress(state, 1, bufptr);        
        curlen = 0;
    }
    
    /* pad out with zeros, but store length in last 8 bytes (sizeof uint64_t) */
    while (curlen < (di->block_size - 8))  bufptr[curlen++] = (unsigned char)0;
    totalLen *= 8; // size in bits
    CC_XSTORE64H(totalLen, bufptr+(di->block_size - 8));
    di->compress(state, 1, bufptr);
}

/*
 #define CC_MD2_DIGEST_LENGTH    16
 #define CC_MD2_BLOCK_BYTES      64
 #define CC_MD2_BLOCK_LONG       (CC_MD2_BLOCK_BYTES / sizeof(CC_LONG))


 typedef struct CC_MD2state_st
 {
 int num;
 unsigned char data[CC_MD2_DIGEST_LENGTH];
 CC_LONG cksm[CC_MD2_BLOCK_LONG];
 CC_LONG state[CC_MD2_BLOCK_LONG];
 } CC_MD2_CTX;
 */

static inline void md2in(struct ccdigest_info *di, ccdigest_ctx_t ctx, CC_MD2_CTX *c)
{
    CC_XMEMCPY(ccdigest_state_u8(di, ctx)+48, c->cksm, CC_MD2_BLOCK_LONG);    
    CC_XMEMCPY(ccdigest_state_u8(di, ctx), c->state, CC_MD2_BLOCK_LONG);    
    CC_XMEMCPY(ccdigest_data(di, ctx), c->data, CC_MD2_DIGEST_LENGTH);
    ccdigest_num(di, ctx) = c->num;    
}

static inline void md2out(struct ccdigest_info *di, CC_MD2_CTX *c, ccdigest_ctx_t ctx)
{
    CC_XMEMCPY(c->cksm, ccdigest_state_u8(di, ctx)+48, CC_MD2_BLOCK_LONG);    
    CC_XMEMCPY(c->state, ccdigest_state_u8(di, ctx), CC_MD2_BLOCK_LONG);    
    CC_XMEMCPY(c->data, ccdigest_data(di, ctx), CC_MD2_DIGEST_LENGTH);
    c->num = (int) ccdigest_num(di, ctx);    
}

int CC_MD2_Init(CC_MD2_CTX *c)
{
    struct ccdigest_info *di = CCDigestGetDigestInfo(kCCDigestMD2);
    ccdigest_di_decl(di, ctx);
    ccdigest_init(di, ctx);
    md2out(di, c, ctx);
    return CC_COMPAT_DIGEST_RETURN;
}

int CC_MD2_Update(CC_MD2_CTX *c, const void *data, CC_LONG len)
{
    struct ccdigest_info *di = CCDigestGetDigestInfo(kCCDigestMD2);
    ccdigest_di_decl(di, ctx);
    md2in(di, ctx, c);
    ccdigest_update(di, ctx, len, data);
    md2out(di, c, ctx);
    return CC_COMPAT_DIGEST_RETURN;
}

extern int CC_MD2_Final(unsigned char *md, CC_MD2_CTX *c)
{
    struct ccdigest_info *di = CCDigestGetDigestInfo(kCCDigestMD2);
    ccdigest_di_decl(di, ctx);
    md2in(di, ctx, c);
    ccdigest_final(di, ctx, md);
    md2out(di, c, ctx);
    return CC_COMPAT_DIGEST_RETURN;
}





/*
 typedef struct CC_SHA256state_st
 {   
     CC_LONG count[2];
     CC_LONG hash[8];
     CC_LONG wbuf[16];
 } CC_SHA256_CTX;
 
 */

typedef struct CC_SHA256state_x
{   
    uint64_t count;
    uint32_t hash[8];
    uint32_t wbuf[16];
} CC_SHA256_CTX_X;



int
CC_SHA256_Init(CC_SHA256_CTX *x)
{
    CC_SHA256_CTX_X *c = (CC_SHA256_CTX_X *) x;
    CC_DEBUG_LOG(ASL_LEVEL_ERR, "Entering\n");
    ASSERT(sizeof(CC_SHA256_CTX) == sizeof(CC_SHA256_CTX_X));
    struct ccdigest_info *di = CCDigestGetDigestInfo(kCCDigestSHA256);
    ASSERT(di->state_size == CC_SHA256_DIGEST_LENGTH);
    CC_XZEROMEM(c->hash, CC_SHA256_DIGEST_LENGTH);
    ASSERT(di->block_size == CC_SHA256_BLOCK_BYTES);
    CC_XZEROMEM(c->wbuf, CC_SHA256_BLOCK_BYTES);
    c->count = 0;
    CC_XMEMCPY(c->hash, di->initial_state, di->state_size);
	return CC_COMPAT_DIGEST_RETURN;
}

int
CC_SHA256_Update(CC_SHA256_CTX *x, const void *data, CC_LONG len)
{
    struct ccdigest_info *di = CCDigestGetDigestInfo(kCCDigestSHA256);
    CC_SHA256_CTX_X *c = (CC_SHA256_CTX_X *) x;
    uint64_t totalLen = c->count;
	uint64_t curlen = totalLen % di->block_size;
	uint8_t *bufptr = (uint8_t *) c->wbuf;
    struct ccdigest_state *state = c->hash;
    
    CC_DEBUG_LOG(ASL_LEVEL_ERR, "Entering\n");
   	if(!len || !data) return CC_COMPAT_DIGEST_RETURN;
    c->count += len;
    
    ccdigest_process(di, bufptr, state, curlen, len, data);

    return CC_COMPAT_DIGEST_RETURN;
}    

int
CC_SHA256_Final(unsigned char *md, CC_SHA256_CTX *x)
{
    struct ccdigest_info *di = CCDigestGetDigestInfo(kCCDigestSHA256);
    CC_SHA256_CTX_X *c = (CC_SHA256_CTX_X *) x;
    uint64_t totalLen = c->count;
	uint64_t curlen = totalLen % di->block_size;
	uint8_t *bufptr = (uint8_t *) c->wbuf;
    struct ccdigest_state *state = c->hash;
    
    CC_DEBUG_LOG(ASL_LEVEL_ERR, "Entering\n");
    if(!md) return CC_COMPAT_DIGEST_RETURN;
    
    ccdigest_finalize(di, bufptr, state, curlen, totalLen);
    
    /* copy output */
    for (int i = 0; i < 8; i++)  CC_XSTORE32H(c->hash[i], md+(4*i));

	return CC_COMPAT_DIGEST_RETURN;
}


/*
typedef struct CC_SHA512state_st
{   CC_LONG64 count[2];
    CC_LONG64 hash[8];
    CC_LONG64 wbuf[16];
} CC_SHA512_CTX;
*/

typedef struct CC_SHA512state_x
{   
    uint64_t count;
    uint64_t countx;
    uint64_t hash[8];
    uint64_t wbuf[16];
} CC_SHA512_CTX_X;


int
CC_SHA512_Init(CC_SHA512_CTX *x)
{
    CC_DEBUG_LOG(ASL_LEVEL_ERR, "Entering\n");
    struct ccdigest_info *di = CCDigestGetDigestInfo(kCCDigestSHA512);
    CC_SHA512_CTX_X *c = (CC_SHA512_CTX_X *) x;
    ASSERT(di->state_size == CC_SHA512_DIGEST_LENGTH);
    CC_XZEROMEM(c->hash, CC_SHA512_DIGEST_LENGTH);
    ASSERT(di->block_size == CC_SHA512_BLOCK_BYTES);
    CC_XZEROMEM(c->wbuf, CC_SHA512_BLOCK_BYTES);
    c->count = 0;
    CC_XMEMCPY(c->hash, di->initial_state, di->state_size);
	return CC_COMPAT_DIGEST_RETURN;
}

int
CC_SHA512_Update(CC_SHA512_CTX *x, const void *data, CC_LONG len)
{
    struct ccdigest_info *di = CCDigestGetDigestInfo(kCCDigestSHA512);
    CC_SHA512_CTX_X *c = (CC_SHA512_CTX_X *) x;
    uint64_t totalLen = c->count;
	uint64_t curlen = totalLen % di->block_size;
	uint8_t *bufptr = (uint8_t *) c->wbuf;
    struct ccdigest_state *state = c->hash;
    
    CC_DEBUG_LOG(ASL_LEVEL_ERR, "Entering\n");
    
   	if(!len || !data) return CC_COMPAT_DIGEST_RETURN;
    
    c->count += len;
    ccdigest_process(di, bufptr, state, curlen, len, data);
    return CC_COMPAT_DIGEST_RETURN;
}    

int
CC_SHA512_Final(unsigned char *md, CC_SHA512_CTX *x)
{
    struct ccdigest_info *di = CCDigestGetDigestInfo(kCCDigestSHA512);
    CC_SHA512_CTX_X *c = (CC_SHA512_CTX_X *) x;
    uint64_t totalLen = c->count;
	uint64_t curlen = totalLen % di->block_size;
	uint8_t *bufptr = (uint8_t *) c->wbuf;
    struct ccdigest_state *state = c->hash;
    
    CC_DEBUG_LOG(ASL_LEVEL_ERR, "Entering\n");
    if(!md) return CC_COMPAT_DIGEST_RETURN;
    
    ccdigest_finalize(di, bufptr, state, curlen, totalLen);

    /* copy output */
    for (int i = 0; i < di->output_size/8; i++)  CC_XSTORE64H(c->hash[i], md+(8*i));
    
	return CC_COMPAT_DIGEST_RETURN;
}

/*
 * Dependent sets of routines (SHA224 and SHA384)
 */

int
CC_SHA224_Init(CC_SHA256_CTX *c)
{
    CC_DEBUG_LOG(ASL_LEVEL_ERR, "Entering\n");
    struct ccdigest_info *di = CCDigestGetDigestInfo(kCCDigestSHA224);
    ASSERT(di->state_size == CC_SHA256_DIGEST_LENGTH);
    CC_XZEROMEM(c->hash, CC_SHA256_DIGEST_LENGTH);
    ASSERT(di->block_size == CC_SHA256_BLOCK_BYTES);
    CC_XZEROMEM(c->wbuf, CC_SHA256_BLOCK_BYTES);
    c->count[0] = c->count[1] = 0;
    CC_XMEMCPY(c->hash, di->initial_state, di->state_size);
	return CC_COMPAT_DIGEST_RETURN;
}

int
CC_SHA224_Update(CC_SHA256_CTX *c, const void *data, CC_LONG len)
{
    CC_DEBUG_LOG(ASL_LEVEL_ERR, "Entering\n");
	return CC_SHA256_Update(c, data, len);
}

int
CC_SHA224_Final(unsigned char *md, CC_SHA256_CTX *c)
{
    uint32_t buf[CC_SHA256_DIGEST_LENGTH/4];
    CC_DEBUG_LOG(ASL_LEVEL_ERR, "Entering\n");
    
    CC_SHA256_Final(buf, c);
    CC_XMEMCPY(md, buf, CC_SHA224_DIGEST_LENGTH);
	return CC_COMPAT_DIGEST_RETURN;
}


int
CC_SHA384_Init(CC_SHA512_CTX *c)
{
    CC_DEBUG_LOG(ASL_LEVEL_ERR, "Entering\n");
    struct ccdigest_info *di = CCDigestGetDigestInfo(kCCDigestSHA384);
    ASSERT(di->state_size == CC_SHA512_DIGEST_LENGTH);
    CC_XZEROMEM(c->hash, CC_SHA512_DIGEST_LENGTH);
    ASSERT(di->block_size == CC_SHA512_BLOCK_BYTES);
    CC_XZEROMEM(c->wbuf, CC_SHA512_BLOCK_BYTES);
    c->count[0] = c->count[1] = 0;
    CC_XMEMCPY(c->hash, di->initial_state, di->state_size);
	return CC_COMPAT_DIGEST_RETURN;
}


int
CC_SHA384_Update(CC_SHA512_CTX *c, const void *data, CC_LONG len)
{
    CC_DEBUG_LOG(ASL_LEVEL_ERR, "Entering\n");
	return CC_SHA512_Update(c, data, len);
}

int
CC_SHA384_Final(unsigned char *md, CC_SHA512_CTX *c)
{
    uint64_t buf[CC_SHA512_DIGEST_LENGTH/8];
    
    CC_DEBUG_LOG(ASL_LEVEL_ERR, "Entering\n");
    CC_SHA512_Final(buf, c);
    CC_XMEMCPY(md, buf, CC_SHA384_DIGEST_LENGTH);
	return CC_COMPAT_DIGEST_RETURN;
}


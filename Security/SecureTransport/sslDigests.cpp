/*
 * Copyright (c) 2000-2001 Apple Computer, Inc. All Rights Reserved.
 * 
 * The contents of this file constitute Original Code as defined in and are
 * subject to the Apple Public Source License Version 1.2 (the 'License').
 * You may not use this file except in compliance with the License. Please obtain
 * a copy of the License at http://www.apple.com/publicsource and read it before
 * using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESS
 * OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES, INCLUDING WITHOUT
 * LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. Please see the License for the
 * specific language governing rights and limitations under the License.
 */


/*
	File:		sslDigests.cpp

	Contains:	interface between SSL and SHA, MD5 digest implementations

	Written by:	Doug Mitchell

	Copyright: (c) 1999 by Apple Computer, Inc., all rights reserved.

*/

#include "sslContext.h"
#include "cryptType.h"
#include "sslMemory.h"
#include "sslDigests.h"
#include "sslDebug.h"
#include "appleCdsa.h"
#include <Security/cssm.h>
#include <string.h>

#define DIGEST_PRINT		0
#if		DIGEST_PRINT
#define dgprintf(s)	printf s
#else
#define dgprintf(s)
#endif

/*
 * Common digest context. The SSLBuffer.data pointer in a "digest state" argument
 * casts to one of these.
 */
typedef struct {
	CSSM_CC_HANDLE	hashHand;
} cdsaHashContext;

const UInt8 SSLMACPad1[MAX_MAC_PADDING] = 
{
	0x36,0x36,0x36,0x36,0x36,0x36,0x36,0x36,
	0x36,0x36,0x36,0x36,0x36,0x36,0x36,0x36,
	0x36,0x36,0x36,0x36,0x36,0x36,0x36,0x36,
	0x36,0x36,0x36,0x36,0x36,0x36,0x36,0x36,
	0x36,0x36,0x36,0x36,0x36,0x36,0x36,0x36,
	0x36,0x36,0x36,0x36,0x36,0x36,0x36,0x36
};

const UInt8 SSLMACPad2[MAX_MAC_PADDING] = 
{
	0x5C,0x5C,0x5C,0x5C,0x5C,0x5C,0x5C,0x5C,
	0x5C,0x5C,0x5C,0x5C,0x5C,0x5C,0x5C,0x5C,
	0x5C,0x5C,0x5C,0x5C,0x5C,0x5C,0x5C,0x5C,
	0x5C,0x5C,0x5C,0x5C,0x5C,0x5C,0x5C,0x5C,
	0x5C,0x5C,0x5C,0x5C,0x5C,0x5C,0x5C,0x5C,
	0x5C,0x5C,0x5C,0x5C,0x5C,0x5C,0x5C,0x5C
};

/*
 * Public general hash functions 
 */

/* 
 * A convenience wrapper for HashReference.clone, which has the added benefit of
 * allocating the state buffer for the caller.
 */
OSStatus
CloneHashState(
	const HashReference &ref, 
	const SSLBuffer &state, 
	SSLBuffer &newState, 
	SSLContext *ctx)
{   
	OSStatus      err;
    if ((err = SSLAllocBuffer(newState, ref.contextSize, ctx)) != 0)
        return err;
	return ref.clone(state, newState);
}

/* 
 * Wrapper for HashReference.init.
 */
OSStatus
ReadyHash(const HashReference &ref, SSLBuffer &state, SSLContext *ctx)
{   
	OSStatus      err;
    if ((err = SSLAllocBuffer(state, ref.contextSize, ctx)) != 0)
        return err;
    return ref.init(state, ctx);
}

/*
 * Wrapper for HashReference.clone. Tolerates NULL digestCtx and frees it if it's
 * there.
 */
OSStatus CloseHash(const HashReference &ref, SSLBuffer &state, SSLContext *ctx)
{
	OSStatus serr;
	
	if(state.data == NULL) {
		return noErr;
	}
	serr = ref.close(state, ctx);
	if(serr) {
		return serr;
	}
	return SSLFreeBuffer(state, ctx);
}

static OSStatus HashNullInit(SSLBuffer &digestCtx, SSLContext *sslCtx);
static OSStatus HashNullUpdate(SSLBuffer &digestCtx, const SSLBuffer &data);
static OSStatus HashNullFinal(SSLBuffer &digestCtx, SSLBuffer &digest);
static OSStatus HashNullClose(SSLBuffer &digestCtx, SSLContext *sslCtx);
static OSStatus HashNullClone(const SSLBuffer &src, SSLBuffer &dst);

static OSStatus HashMD5Init(SSLBuffer &digestCtx, SSLContext *sslCtx);
static OSStatus HashSHA1Init(SSLBuffer &digestCtx, SSLContext *sslCtx);
static OSStatus cdsaHashInit(SSLBuffer &digestCtx, SSLContext *sslCtx,
	CSSM_ALGORITHMS digestAlg);
static OSStatus cdsaHashUpdate(SSLBuffer &digestCtx, const SSLBuffer &data);
static OSStatus cdsaHashFinal(SSLBuffer &digestCtx, SSLBuffer &digest); 
static OSStatus cdsaHashClose(SSLBuffer &digestCtx, SSLContext *sslCtx);
static OSStatus cdsaHashClone(const SSLBuffer &src, SSLBuffer &dest);

/*
 * These are the handles by which the bulk of digesting work
 * is done.
 */
const HashReference SSLHashNull = 
	{ 	
		0, 
	  	0, 
	  	0, 
	  	HashNullInit, 
	  	HashNullUpdate, 
	  	HashNullFinal, 
		HashNullClose,
	  	HashNullClone 
	};
	
const HashReference SSLHashMD5 = 
	{ 
		sizeof(cdsaHashContext), 
		16, 
		48, 
		HashMD5Init, 
		cdsaHashUpdate, 
		cdsaHashFinal, 
		cdsaHashClose,
		cdsaHashClone 
	};

const HashReference SSLHashSHA1 = 
	{ 
		sizeof(cdsaHashContext), 
		20, 
		40, 
		HashSHA1Init, 
		cdsaHashUpdate, 
		cdsaHashFinal, 
		cdsaHashClose,
		cdsaHashClone 
	};

/*** NULL ***/
static OSStatus HashNullInit(SSLBuffer &digestCtx, SSLContext *sslCtx) { 
	return noErr; 
}

static OSStatus HashNullUpdate(SSLBuffer &digestCtx, const SSLBuffer &data) { 
	return noErr; 
}

static OSStatus HashNullFinal(SSLBuffer &digestCtx, SSLBuffer &digest) { 
	return noErr; 
}
static OSStatus HashNullClose(SSLBuffer &digestCtx, SSLContext *sslCtx) {
	return noErr; 
}
static OSStatus HashNullClone(const SSLBuffer &src, SSLBuffer &dest) { 
	return noErr; 
}

static OSStatus HashMD5Init(SSLBuffer &digestCtx, SSLContext *sslCtx)
{   
	assert(digestCtx.length >= sizeof(cdsaHashContext));
	return cdsaHashInit(digestCtx, sslCtx, CSSM_ALGID_MD5);
}

static OSStatus HashSHA1Init(SSLBuffer &digestCtx, SSLContext *sslCtx)
{   
	assert(digestCtx.length >= sizeof(cdsaHashContext));
	return cdsaHashInit(digestCtx, sslCtx, CSSM_ALGID_SHA1);
}

/* common digest functions via CDSA */
static OSStatus cdsaHashInit(SSLBuffer &digestCtx, 
	SSLContext *sslCtx,
	CSSM_ALGORITHMS digestAlg)
{
	OSStatus serr;
	cdsaHashContext *cdsaCtx;
	CSSM_CC_HANDLE hashHand = 0;
	CSSM_RETURN crtn;
	
	assert(digestCtx.length >= sizeof(cdsaHashContext));
	serr = attachToCsp(sslCtx);		// should be a nop
	if(serr) {
		return serr;
	}
	cdsaCtx = (cdsaHashContext *)digestCtx.data;
	cdsaCtx->hashHand = 0;
	dgprintf(("###cdsaHashInit  cdsaCtx %p\n", cdsaCtx));
	
	/* cook up a digest context, initialize it */
	crtn = CSSM_CSP_CreateDigestContext(sslCtx->cspHand,
		digestAlg,
		&hashHand);
	if(crtn) {
		sslErrorLog("CSSM_CSP_CreateDigestContext failure\n");
		return errSSLCrypto;
	}
	crtn = CSSM_DigestDataInit(hashHand);
	if(crtn) {
		CSSM_DeleteContext(hashHand);
		sslErrorLog("CSSM_DigestDataInit failure\n");
		return errSSLCrypto;
	}
	cdsaCtx->hashHand = hashHand;
    return noErr;
}

static OSStatus cdsaHashUpdate(SSLBuffer &digestCtx, const SSLBuffer &data)
{   
	cdsaHashContext *cdsaCtx;
	CSSM_RETURN crtn;
	CSSM_DATA cdata;
	
	assert(digestCtx.length >= sizeof(cdsaHashContext));
	cdsaCtx = (cdsaHashContext *)digestCtx.data;
	//dgprintf(("###cdsaHashUpdate  cdsaCtx %p\n", cdsaCtx));
	
	SSLBUF_TO_CSSM(&data, &cdata);
	crtn = CSSM_DigestDataUpdate(cdsaCtx->hashHand, &cdata, 1);
	if(crtn) {
		sslErrorLog("CSSM_DigestDataUpdate failure\n");
		return errSSLCrypto;
	}
	else {
		return noErr;
	}
}

static OSStatus cdsaHashFinal(SSLBuffer &digestCtx, SSLBuffer &digest)
{	
	cdsaHashContext *cdsaCtx;
	CSSM_RETURN crtn;
	CSSM_DATA cdata;
	OSStatus srtn = noErr;
	
	assert(digestCtx.length >= sizeof(cdsaHashContext));
	cdsaCtx = (cdsaHashContext *)digestCtx.data;
	dgprintf(("###cdsaHashFinal  cdsaCtx %p\n", cdsaCtx));
	SSLBUF_TO_CSSM(&digest, &cdata);
	crtn = CSSM_DigestDataFinal(cdsaCtx->hashHand, &cdata);
	if(crtn) {
		sslErrorLog("CSSM_DigestDataFinal failure\n");
		srtn = errSSLCrypto;
	}
	else {
		digest.length = cdata.Length;
	}
	CSSM_DeleteContext(cdsaCtx->hashHand);
	cdsaCtx->hashHand = 0;
    return srtn;
}

static OSStatus cdsaHashClose(SSLBuffer &digestCtx, SSLContext *sslCtx)
{
	cdsaHashContext *cdsaCtx;
	
	assert(digestCtx.length >= sizeof(cdsaHashContext));
	cdsaCtx = (cdsaHashContext *)digestCtx.data;
	dgprintf(("###cdsaHashClose  cdsaCtx %p\n", cdsaCtx));
	if(cdsaCtx->hashHand != 0) {
		CSSM_DeleteContext(cdsaCtx->hashHand);
		cdsaCtx->hashHand = 0;
	}
	return noErr;
}

static OSStatus cdsaHashClone(const SSLBuffer &src, SSLBuffer &dst)
{   
	cdsaHashContext *srcCtx;
	cdsaHashContext *dstCtx;
	CSSM_RETURN crtn;

	assert(src.length >= sizeof(cdsaHashContext));
	assert(dst.length >= sizeof(cdsaHashContext));
	srcCtx = (cdsaHashContext *)src.data;
	dstCtx = (cdsaHashContext *)dst.data;
	dgprintf(("###cdsaHashClone  srcCtx %p  dstCtx %p\n", srcCtx, dstCtx));

	crtn = CSSM_DigestDataClone(srcCtx->hashHand, &dstCtx->hashHand);
	if(crtn) {
		sslErrorLog("CSSM_DigestDataClone failure\n");
		return errSSLCrypto;
	}
	else {
		return noErr;
	}
}   


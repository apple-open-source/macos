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
	File:		digests.c

	Contains:	interface between SSL and SHA, MD5 digest libraries

	Written by:	Doug Mitchell, based on Netscape SSLRef 3.0

	Copyright: (c) 1999 by Apple Computer, Inc., all rights reserved.

*/
/*  *********************************************************************
    File: digests.c

    SSLRef 3.0 Final -- 11/19/96

    Copyright (c)1996 by Netscape Communications Corp.

    By retrieving this software you are bound by the licensing terms
    disclosed in the file "LICENSE.txt". Please read it, and if you don't
    accept the terms, delete this software.

    SSLRef 3.0 was developed by Netscape Communications Corp. of Mountain
    View, California <http://home.netscape.com/> and Consensus Development
    Corporation of Berkeley, California <http://www.consensus.com/>.

    *********************************************************************

    File: digests.c    Hashing support functions and data structures

    Contains interface functions which generalize hashing support for MD5
    and SHA1 and a dummy null hash implementation (used before MACing is
    turned on). Also, utility functions for using the hashes.

    ****************************************************************** */

#include "sslctx.h"
#include "cryptType.h"
#include "sslalloc.h"
#include "digests.h"
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

uint8   SSLMACPad1[MAX_MAC_PADDING], SSLMACPad2[MAX_MAC_PADDING];

/*
 * Public general hash functions 
 */
void
SSLInitMACPads(void)
{   int     i;
    
    for (i = 0; i < MAX_MAC_PADDING; i++)
    {   SSLMACPad1[i] = 0x36;
        SSLMACPad2[i] = 0x5C;
    }
}

/* 
 * A convenience wrapper for HashReference.clone, which has the added benefit of
 * allocating the state buffer for the caller.
 */
SSLErr
CloneHashState(const HashReference *ref, SSLBuffer state, SSLBuffer *newState, SSLContext *ctx)
{   
	SSLErr      err;
    if ((err = SSLAllocBuffer(newState, ref->contextSize, &ctx->sysCtx)) != 0)
        return err;
	return ref->clone(state, *newState);
}

/* 
 * Wrapper for HashReference.init.
 */
SSLErr
ReadyHash(const HashReference *ref, SSLBuffer *state, SSLContext *ctx)
{   
	SSLErr      err;
    if ((err = SSLAllocBuffer(state, ref->contextSize, &ctx->sysCtx)) != 0)
        return err;
    return ref->init(*state, ctx);
}

/*
 * Wrapper for HashReference.clone. Tolerates NULL digestCtx and frees it if it's
 * there.
 */
SSLErr CloseHash(const HashReference *ref, SSLBuffer *state, SSLContext *ctx)
{
	SSLErr serr;
	
	if((state == NULL) || (state->data == NULL)) {
		return SSLNoErr;
	}
	serr = ref->close(*state, ctx);
	if(serr) {
		return serr;
	}
	return SSLFreeBuffer(state, &ctx->sysCtx);
}

static SSLErr HashNullInit(SSLBuffer digestCtx, SSLContext *sslCtx);
static SSLErr HashNullUpdate(SSLBuffer,SSLBuffer);
static SSLErr HashNullFinal(SSLBuffer,SSLBuffer);
static SSLErr HashNullClose(SSLBuffer digestCtx, SSLContext *sslCtx);
static SSLErr HashNullClone(SSLBuffer,SSLBuffer);

static SSLErr HashMD5Init(SSLBuffer digestCtx, SSLContext *sslCtx);
static SSLErr HashSHA1Init(SSLBuffer digestCtx, SSLContext *sslCtx);
static SSLErr cdsaHashInit(SSLBuffer digestCtx, SSLContext *sslCtx,
	CSSM_ALGORITHMS digestAlg);
static SSLErr cdsaHashUpdate(SSLBuffer digestCtx, SSLBuffer data);
static SSLErr cdsaHashFinal(SSLBuffer digestCtx, SSLBuffer digest);
static SSLErr cdsaHashClose(SSLBuffer digestCtx, SSLContext *sslCtx);
static SSLErr cdsaHashClone(SSLBuffer src, SSLBuffer dest);

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
static SSLErr HashNullInit(SSLBuffer digestCtx, SSLContext *sslCtx) { 
	return SSLNoErr; 
}

static SSLErr HashNullUpdate(SSLBuffer digestCtx, SSLBuffer data) { 
	return SSLNoErr; 
}

static SSLErr HashNullFinal(SSLBuffer digestCtx, SSLBuffer digest) { 
	return SSLNoErr; 
}
static SSLErr HashNullClose(SSLBuffer digestCtx, SSLContext *sslCtx) {
	return SSLNoErr; 
}
static SSLErr HashNullClone(SSLBuffer src, SSLBuffer dest) { 
	return SSLNoErr; 
}

static SSLErr HashMD5Init(SSLBuffer digestCtx, SSLContext *sslCtx)
{   
	CASSERT(digestCtx.length >= sizeof(cdsaHashContext));
	return cdsaHashInit(digestCtx, sslCtx, CSSM_ALGID_MD5);
}

static SSLErr HashSHA1Init(SSLBuffer digestCtx, SSLContext *sslCtx)
{   
	CASSERT(digestCtx.length >= sizeof(cdsaHashContext));
	return cdsaHashInit(digestCtx, sslCtx, CSSM_ALGID_SHA1);
}

/* common digest functions via CDSA */
static SSLErr cdsaHashInit(SSLBuffer digestCtx, 
	SSLContext *sslCtx,
	CSSM_ALGORITHMS digestAlg)
{
	SSLErr serr;
	cdsaHashContext *cdsaCtx;
	CSSM_CC_HANDLE hashHand = 0;
	CSSM_RETURN crtn;
	
	CASSERT(digestCtx.length >= sizeof(cdsaHashContext));
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
		errorLog0("CSSM_CSP_CreateDigestContext failure\n");
		return SSLCryptoError;
	}
	crtn = CSSM_DigestDataInit(hashHand);
	if(crtn) {
		CSSM_DeleteContext(hashHand);
		errorLog0("CSSM_DigestDataInit failure\n");
		return SSLCryptoError;
	}
	cdsaCtx->hashHand = hashHand;
    return SSLNoErr;
}

static SSLErr cdsaHashUpdate(SSLBuffer digestCtx, SSLBuffer data)
{   
	cdsaHashContext *cdsaCtx;
	CSSM_RETURN crtn;
	CSSM_DATA cdata;
	
	CASSERT(digestCtx.length >= sizeof(cdsaHashContext));
	cdsaCtx = (cdsaHashContext *)digestCtx.data;
	//dgprintf(("###cdsaHashUpdate  cdsaCtx %p\n", cdsaCtx));
	
	SSLBUF_TO_CSSM(&data, &cdata);
	crtn = CSSM_DigestDataUpdate(cdsaCtx->hashHand, &cdata, 1);
	if(crtn) {
		errorLog0("CSSM_DigestDataUpdate failure\n");
		return SSLCryptoError;
	}
	else {
		return SSLNoErr;
	}
}

static SSLErr cdsaHashFinal(SSLBuffer digestCtx, SSLBuffer digest)
{	
	cdsaHashContext *cdsaCtx;
	CSSM_RETURN crtn;
	CSSM_DATA cdata;
	SSLErr srtn = SSLNoErr;
	
	CASSERT(digestCtx.length >= sizeof(cdsaHashContext));
	cdsaCtx = (cdsaHashContext *)digestCtx.data;
	dgprintf(("###cdsaHashFinal  cdsaCtx %p\n", cdsaCtx));
	SSLBUF_TO_CSSM(&digest, &cdata);
	crtn = CSSM_DigestDataFinal(cdsaCtx->hashHand, &cdata);
	if(crtn) {
		errorLog0("CSSM_DigestDataFinal failure\n");
		srtn = SSLCryptoError;
	}
	else {
		digest.length = cdata.Length;
	}
	CSSM_DeleteContext(cdsaCtx->hashHand);
	cdsaCtx->hashHand = 0;
    return srtn;
}

static SSLErr cdsaHashClose(SSLBuffer digestCtx, SSLContext *sslCtx)
{
	cdsaHashContext *cdsaCtx;
	
	CASSERT(digestCtx.length >= sizeof(cdsaHashContext));
	cdsaCtx = (cdsaHashContext *)digestCtx.data;
	dgprintf(("###cdsaHashClose  cdsaCtx %p\n", cdsaCtx));
	if(cdsaCtx->hashHand != 0) {
		CSSM_DeleteContext(cdsaCtx->hashHand);
		cdsaCtx->hashHand = 0;
	}
	return SSLNoErr;
}

static SSLErr cdsaHashClone(SSLBuffer src, SSLBuffer dst)
{   
	cdsaHashContext *srcCtx;
	cdsaHashContext *dstCtx;
	CSSM_RETURN crtn;

	CASSERT(src.length >= sizeof(cdsaHashContext));
	CASSERT(dst.length >= sizeof(cdsaHashContext));
	srcCtx = (cdsaHashContext *)src.data;
	dstCtx = (cdsaHashContext *)dst.data;
	dgprintf(("###cdsaHashClone  srcCtx %p  dstCtx %p\n", srcCtx, dstCtx));

	crtn = CSSM_DigestDataClone(srcCtx->hashHand, &dstCtx->hashHand);
	if(crtn) {
		errorLog0("CSSM_DigestDataClone failure\n");
		return SSLCryptoError;
	}
	else {
		return SSLNoErr;
	}
}   


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
	File:		sslDigests.c

	Contains:	interface between SSL and SHA, MD5 digest implementations

	Written by:	Doug Mitchell

	Copyright: (c) 1999 by Apple Computer, Inc., all rights reserved.

*/

#include "sslContext.h"
#include "cryptType.h"
#include "sslMemory.h"
#include "sslDigests.h"
#include "sslDebug.h"
#include <CommonCrypto/CommonDigest.h>
#include <string.h>

#define DIGEST_PRINT		0
#if		DIGEST_PRINT
#define dgprintf(s)	printf s
#else
#define dgprintf(s)
#endif

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
	const HashReference *ref, 
	const SSLBuffer *state, 
	SSLBuffer *newState, 
	SSLContext *ctx)
{   
	OSStatus      err;
    if ((err = SSLAllocBuffer(newState, ref->contextSize, ctx)) != 0)
        return err;
	return ref->clone(state, newState);
}

/* 
 * Wrapper for HashReference.init.
 */
OSStatus
ReadyHash(const HashReference *ref, SSLBuffer *state, SSLContext *ctx)
{   
	OSStatus      err;
    if ((err = SSLAllocBuffer(state, ref->contextSize, ctx)) != 0)
        return err;
    return ref->init(state, ctx);
}

/*
 * Wrapper for HashReference.clone. Tolerates NULL digestCtx and frees it if it's
 * there.
 */
OSStatus CloseHash(const HashReference *ref, SSLBuffer *state, SSLContext *ctx)
{
	OSStatus serr;
	
	if(state->data == NULL) {
		return noErr;
	}
	serr = ref->close(state, ctx);
	if(serr) {
		return serr;
	}
	return SSLFreeBuffer(state, ctx);
}

/*** NULL ***/
static OSStatus HashNullInit(SSLBuffer *digestCtx, SSLContext *sslCtx) { 
	return noErr; 
}
static OSStatus HashNullUpdate(SSLBuffer *digestCtx, const SSLBuffer *data) { 
	return noErr; 
}
static OSStatus HashNullFinal(SSLBuffer *digestCtx, SSLBuffer *digest) { 
	return noErr; 
}
static OSStatus HashNullClose(SSLBuffer *digestCtx, SSLContext *sslCtx) {
	return noErr; 
}
static OSStatus HashNullClone(const SSLBuffer *src, SSLBuffer *dest) { 
	return noErr; 
}

/* MD5 */
static OSStatus HashMD5Init(SSLBuffer *digestCtx, SSLContext *sslCtx) 
{ 
	ASSERT(digestCtx->length >= sizeof(CC_MD5_CTX));
	CC_MD5_Init((CC_MD5_CTX *)digestCtx->data);
	return noErr; 
}

static OSStatus HashMD5Update(SSLBuffer *digestCtx, const SSLBuffer *data) 
{ 
	ASSERT(digestCtx->length >= sizeof(CC_MD5_CTX));
	CC_MD5_Update((CC_MD5_CTX *)digestCtx->data, data->data, data->length);
	return noErr; 
}

static OSStatus HashMD5Final(SSLBuffer *digestCtx, SSLBuffer *digest) 
{ 
	ASSERT(digestCtx->length >= sizeof(CC_MD5_CTX));
	CC_MD5_Final(digest->data, (CC_MD5_CTX *)digestCtx->data);
	digest->length = CC_MD5_DIGEST_LENGTH;
	return noErr; 
}

static OSStatus HashMD5Close(SSLBuffer *digestCtx, SSLContext *sslCtx) 
{
	/* nop */
	return noErr; 
}
static OSStatus HashMD5Clone(const SSLBuffer *src, SSLBuffer *dest) 
{ 
	ASSERT(src->length >= sizeof(CC_MD5_CTX));
	ASSERT(dest->length >= sizeof(CC_MD5_CTX));
	memmove(dest->data, src->data, sizeof(CC_MD5_CTX));
	return noErr;
}

/* SHA1 */
static OSStatus HashSHA1Init(SSLBuffer *digestCtx, SSLContext *sslCtx) 
{ 
	ASSERT(digestCtx->length >= sizeof(CC_SHA1_CTX));
	CC_SHA1_Init((CC_SHA1_CTX *)digestCtx->data);
	return noErr; 
}

static OSStatus HashSHA1Update(SSLBuffer *digestCtx, const SSLBuffer *data) 
{ 
	ASSERT(digestCtx->length >= sizeof(CC_SHA1_CTX));
	CC_SHA1_Update((CC_SHA1_CTX *)digestCtx->data, data->data, data->length);
	return noErr; 
}

static OSStatus HashSHA1Final(SSLBuffer *digestCtx, SSLBuffer *digest) 
{ 
	ASSERT(digestCtx->length >= sizeof(CC_SHA1_CTX));
	CC_SHA1_Final(digest->data, (CC_SHA1_CTX *)digestCtx->data);
	digest->length = CC_SHA1_DIGEST_LENGTH;
	return noErr; 
}

static OSStatus HashSHA1Close(SSLBuffer *digestCtx, SSLContext *sslCtx) 
{
	/* nop */
	return noErr; 
}
static OSStatus HashSHA1Clone(const SSLBuffer *src, SSLBuffer *dest) 
{ 
	ASSERT(src->length >= sizeof(CC_SHA1_CTX));
	ASSERT(dest->length >= sizeof(CC_SHA1_CTX));
	memmove(dest->data, src->data, sizeof(CC_SHA1_CTX));
	return noErr;
}

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
		sizeof(CC_MD5_CTX), 
		CC_MD5_DIGEST_LENGTH, 
		48, 
		HashMD5Init, 
		HashMD5Update, 
		HashMD5Final, 
		HashMD5Close,
		HashMD5Clone 
	};

const HashReference SSLHashSHA1 = 
	{ 
		sizeof(CC_SHA1_CTX), 
		CC_SHA1_DIGEST_LENGTH, 
		40, 
		HashSHA1Init, 
		HashSHA1Update, 
		HashSHA1Final, 
		HashSHA1Close,
		HashSHA1Clone 
	};


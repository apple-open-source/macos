/*
 * Copyright (c) 2000-2001,2005-2007,2010-2012 Apple Inc. All Rights Reserved.
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
 * sslDigests.c - Interface between SSL and SHA, MD5 digest implementations
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
 * Wrapper for HashReference.close. Tolerates NULL state and frees it if it's
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

/*** MD5 ***/
static OSStatus HashMD5Init(SSLBuffer *digestCtx, SSLContext *sslCtx)
{
	assert(digestCtx->length >= sizeof(CC_MD5_CTX));
	CC_MD5_CTX *ctx = (CC_MD5_CTX *)digestCtx->data;
	CC_MD5_Init(ctx);
	dgprintf(("###HashMD5Init  ctx %p\n", ctx));
    return noErr;
}

static OSStatus HashMD5Update(SSLBuffer *digestCtx, const SSLBuffer *data)
{
    /* 64 bits cast: safe, SSL records are always smaller than 2^32 bytes */
	assert(digestCtx->length >= sizeof(CC_MD5_CTX));
	CC_MD5_CTX *ctx = (CC_MD5_CTX *)digestCtx->data;
	CC_MD5_Update(ctx, data->data, (CC_LONG)data->length);
    return noErr;
}

static OSStatus HashMD5Final(SSLBuffer *digestCtx, SSLBuffer *digest)
{
	assert(digestCtx->length >= sizeof(CC_MD5_CTX));
	CC_MD5_CTX *ctx = (CC_MD5_CTX *)digestCtx->data;
	dgprintf(("###HashMD5Final  ctx %p\n", ctx));
	assert(digest->length >= CC_MD5_DIGEST_LENGTH);
	//if (digest->length < CC_MD5_DIGEST_LENGTH)
	//	return errSSLCrypto;
	CC_MD5_Final(digest->data, ctx);
	digest->length = CC_MD5_DIGEST_LENGTH;
    return noErr;
}

static OSStatus HashMD5Close(SSLBuffer *digestCtx, SSLContext *sslCtx)
{
	assert(digestCtx->length >= sizeof(CC_MD5_CTX));
    return noErr;
}

static OSStatus HashMD5Clone(const SSLBuffer *src, SSLBuffer *dst)
{
	CC_MD5_CTX *srcCtx;
	CC_MD5_CTX *dstCtx;

	assert(src->length >= sizeof(CC_MD5_CTX));
	assert(dst->length >= sizeof(CC_MD5_CTX));

	srcCtx = (CC_MD5_CTX *)src->data;
	dstCtx = (CC_MD5_CTX *)dst->data;
	dgprintf(("###HashMD5Clone  srcCtx %p  dstCtx %p\n", srcCtx, dstCtx));

	memcpy(dstCtx, srcCtx, sizeof(CC_MD5_CTX));
	return noErr;
}

/*** SHA1 ***/
static OSStatus HashSHA1Init(SSLBuffer *digestCtx, SSLContext *sslCtx)
{
	assert(digestCtx->length >= sizeof(CC_SHA1_CTX));
	CC_SHA1_CTX *ctx = (CC_SHA1_CTX *)digestCtx->data;
	CC_SHA1_Init(ctx);
	dgprintf(("###HashSHA1Init  ctx %p\n", ctx));
    return noErr;
}

static OSStatus HashSHA1Update(SSLBuffer *digestCtx, const SSLBuffer *data)
{
    /* 64 bits cast: safe, SSL records are always smaller than 2^32 bytes */
	assert(digestCtx->length >= sizeof(CC_SHA1_CTX));
	CC_SHA1_CTX *ctx = (CC_SHA1_CTX *)digestCtx->data;
	CC_SHA1_Update(ctx, data->data, (CC_LONG)data->length);
    return noErr;
}

static OSStatus HashSHA1Final(SSLBuffer *digestCtx, SSLBuffer *digest)
{
	assert(digestCtx->length >= sizeof(CC_SHA1_CTX));
	CC_SHA1_CTX *ctx = (CC_SHA1_CTX *)digestCtx->data;
	dgprintf(("###HashSHA1Final  ctx %p\n", ctx));
	assert(digest->length >= CC_SHA1_DIGEST_LENGTH);
	//if (digest->length < CC_SHA1_DIGEST_LENGTH)
	//	return errSSLCrypto;
	CC_SHA1_Final(digest->data, ctx);
	digest->length = CC_SHA1_DIGEST_LENGTH;
    return noErr;
}

static OSStatus HashSHA1Close(SSLBuffer *digestCtx, SSLContext *sslCtx)
{
	assert(digestCtx->length >= sizeof(CC_SHA1_CTX));
    return noErr;
}

static OSStatus HashSHA1Clone(const SSLBuffer *src, SSLBuffer *dst)
{
	CC_SHA1_CTX *srcCtx;
	CC_SHA1_CTX *dstCtx;

	assert(src->length >= sizeof(CC_SHA1_CTX));
	assert(dst->length >= sizeof(CC_SHA1_CTX));

	srcCtx = (CC_SHA1_CTX *)src->data;
	dstCtx = (CC_SHA1_CTX *)dst->data;
	dgprintf(("###HashSHA1Clone  srcCtx %p  dstCtx %p\n", srcCtx, dstCtx));

	memcpy(dstCtx, srcCtx, sizeof(CC_SHA1_CTX));
	return noErr;
}

/*** SHA256 ***/
static OSStatus HashSHA256Init(SSLBuffer *digestCtx, SSLContext *sslCtx)
{
	assert(digestCtx->length >= sizeof(CC_SHA256_CTX));
	CC_SHA256_CTX *ctx = (CC_SHA256_CTX *)digestCtx->data;
	CC_SHA256_Init(ctx);
	dgprintf(("###HashSHA256Init  ctx %p\n", ctx));
    return noErr;
}

static OSStatus HashSHA256Update(SSLBuffer *digestCtx, const SSLBuffer *data)
{
    /* 64 bits cast: safe, SSL records are always smaller than 2^32 bytes */
    assert(digestCtx->length >= sizeof(CC_SHA256_CTX));
	CC_SHA256_CTX *ctx = (CC_SHA256_CTX *)digestCtx->data;
	CC_SHA256_Update(ctx, data->data, (CC_LONG)data->length);
    return noErr;
}

static OSStatus HashSHA256Final(SSLBuffer *digestCtx, SSLBuffer *digest)
{
	assert(digestCtx->length >= sizeof(CC_SHA256_CTX));
	CC_SHA256_CTX *ctx = (CC_SHA256_CTX *)digestCtx->data;
	dgprintf(("###HashSHA256Final  ctx %p\n", ctx));
	assert(digest->length >= CC_SHA256_DIGEST_LENGTH);
	//if (digest->length < CC_SHA256_DIGEST_LENGTH)
	//	return errSSLCrypto;
	CC_SHA256_Final(digest->data, ctx);
	digest->length = CC_SHA256_DIGEST_LENGTH;
    return noErr;
}

static OSStatus HashSHA256Close(SSLBuffer *digestCtx, SSLContext *sslCtx)
{
	assert(digestCtx->length >= sizeof(CC_SHA256_CTX));
    return noErr;
}

static OSStatus HashSHA256Clone(const SSLBuffer *src, SSLBuffer *dst)
{
	CC_SHA256_CTX *srcCtx;
	CC_SHA256_CTX *dstCtx;

	assert(src->length >= sizeof(CC_SHA256_CTX));
	assert(dst->length >= sizeof(CC_SHA256_CTX));

	srcCtx = (CC_SHA256_CTX *)src->data;
	dstCtx = (CC_SHA256_CTX *)dst->data;
	dgprintf(("###HashSHA256Clone  srcCtx %p  dstCtx %p\n", srcCtx, dstCtx));

	memcpy(dstCtx, srcCtx, sizeof(CC_SHA256_CTX));
	return noErr;
}

/*** SHA384 ***/
static OSStatus HashSHA384Init(SSLBuffer *digestCtx, SSLContext *sslCtx)
{
	assert(digestCtx->length >= sizeof(CC_SHA512_CTX));
	CC_SHA512_CTX *ctx = (CC_SHA512_CTX *)digestCtx->data;
	CC_SHA384_Init(ctx);
	dgprintf(("###HashSHA384Init  ctx %p\n", ctx));
    return noErr;
}

static OSStatus HashSHA384Update(SSLBuffer *digestCtx, const SSLBuffer *data)
{
    /* 64 bits cast: safe, SSL records are always smaller than 2^32 bytes */
    assert(digestCtx->length >= sizeof(CC_SHA512_CTX));
	CC_SHA512_CTX *ctx = (CC_SHA512_CTX *)digestCtx->data;
	CC_SHA384_Update(ctx, data->data, (CC_LONG)data->length);
    return noErr;
}

static OSStatus HashSHA384Final(SSLBuffer *digestCtx, SSLBuffer *digest)
{
	assert(digestCtx->length >= sizeof(CC_SHA512_CTX));
	CC_SHA512_CTX *ctx = (CC_SHA512_CTX *)digestCtx->data;
	dgprintf(("###HashSHA384Final  ctx %p\n", ctx));
	assert(digest->length >= CC_SHA384_DIGEST_LENGTH);
	//if (digest->length < CC_SHA384_DIGEST_LENGTH)
	//	return errSSLCrypto;
	CC_SHA384_Final(digest->data, ctx);
	digest->length = CC_SHA384_DIGEST_LENGTH;
    return noErr;
}

static OSStatus HashSHA384Close(SSLBuffer *digestCtx, SSLContext *sslCtx)
{
	assert(digestCtx->length >= sizeof(CC_SHA512_CTX));
    return noErr;
}

static OSStatus HashSHA384Clone(const SSLBuffer *src, SSLBuffer *dst)
{
	CC_SHA512_CTX *srcCtx;
	CC_SHA512_CTX *dstCtx;

	assert(src->length >= sizeof(CC_SHA512_CTX));
	assert(dst->length >= sizeof(CC_SHA512_CTX));

	srcCtx = (CC_SHA512_CTX *)src->data;
	dstCtx = (CC_SHA512_CTX *)dst->data;
	dgprintf(("###HashSHA384Clone  srcCtx %p  dstCtx %p\n", srcCtx, dstCtx));

	memcpy(dstCtx, srcCtx, sizeof(CC_SHA512_CTX));
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

const HashReference SSLHashSHA256 =
    {
        sizeof(CC_SHA256_CTX),
        CC_SHA256_DIGEST_LENGTH,
        CC_SHA256_BLOCK_BYTES,
        HashSHA256Init,
        HashSHA256Update,
        HashSHA256Final,
        HashSHA256Close,
        HashSHA256Clone
    };

const HashReference SSLHashSHA384 =
    {
        sizeof(CC_SHA512_CTX),
        CC_SHA384_DIGEST_LENGTH,
        CC_SHA384_BLOCK_BYTES,
        HashSHA384Init,
        HashSHA384Update,
        HashSHA384Final,
        HashSHA384Close,
        HashSHA384Clone
    };

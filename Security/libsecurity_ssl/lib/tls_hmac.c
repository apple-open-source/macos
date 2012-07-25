/*
 * Copyright (c) 2002,2005-2008,2010-2012 Apple Inc. All Rights Reserved.
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
 * tls_hmac.c - HMAC routines used by TLS
 */

#include "tls_hmac.h"


#include "sslMemory.h"
#include "cryptType.h"
#include "sslDigests.h"
#include "sslDebug.h"
#include <strings.h>
#include <assert.h>

#ifdef USE_CDSA_CRYPTO
#include "sslCrypto.h"
#include <CommonCrypto/CommonHMAC.h>

/* Per-session state, opaque to callers; all fields set at alloc time */
struct HMACContext {
	SSLContext					*ctx;

 	/* this one is set once with the key, and it then cloned
 	 * for each init() */
 	CCHmacContext				ccHmacTemplate;

 	/* the one we actually feed data to */
 	CCHmacContext				ccHmac;
 	size_t						macSize;

 	/* FIXME not sure if we need this */
	const struct HMACReference	*hmac;
};

#pragma mark -
#pragma mark CommonCryptor HMAC routines

/* Create an HMAC session */
static OSStatus HMAC_Alloc(
	const struct HMACReference	*hmac,
	SSLContext 					*ctx,
	const void					*keyPtr,
	unsigned					keyLen,
	HMACContextRef				*hmacCtxOut)		// RETURNED
{
	CCHmacAlgorithm	ccAlg;

	HMACContextRef hmacCtx = (HMACContextRef)sslMalloc(sizeof(struct HMACContext));

	if(hmacCtx == NULL) {
		return memFullErr;
	}
	hmacCtx->ctx = ctx;
	hmacCtx->hmac = hmac;

	switch(hmac->alg) {
		case HA_SHA384:
			ccAlg = kCCHmacAlgSHA384;
			hmacCtx->macSize = CC_SHA384_DIGEST_LENGTH;
			break;
		case HA_SHA256:
			ccAlg = kCCHmacAlgSHA256;
			hmacCtx->macSize = CC_SHA256_DIGEST_LENGTH;
			break;
		case HA_SHA1:
			ccAlg = kCCHmacAlgSHA1;
			hmacCtx->macSize = CC_SHA1_DIGEST_LENGTH;
			break;
		case HA_MD5:
			ccAlg = kCCHmacAlgMD5;
			hmacCtx->macSize = CC_MD5_DIGEST_LENGTH;
			break;
		default:
			ASSERT(0);
			return errSSLInternal;
	}

	/* create the template from which individual record MAC-ers are cloned */
	CCHmacInit(&hmacCtx->ccHmacTemplate, ccAlg, keyPtr, keyLen);
	*hmacCtxOut = hmacCtx;
	return noErr;
}

/* free a session */
static OSStatus HMAC_Free(
	HMACContextRef	hmacCtx)
{
	if(hmacCtx != NULL) {
		memset(hmacCtx, 0, sizeof(*hmacCtx));
		sslFree(hmacCtx);
	}
	return noErr;
}

/* Reusable init - clone from template */
static OSStatus HMAC_Init(
	HMACContextRef	hmacCtx)
{
	if(hmacCtx == NULL) {
		return errSSLInternal;
	}
	hmacCtx->ccHmac = hmacCtx->ccHmacTemplate;
	return noErr;
}

/* normal crypt ops */
static OSStatus HMAC_Update(
	HMACContextRef	hmacCtx,
	const void		*data,
	unsigned		dataLen)
{
	CCHmacUpdate(&hmacCtx->ccHmac, data, dataLen);
	return noErr;
}

static OSStatus HMAC_Final(
	HMACContextRef	hmacCtx,
	void			*hmac,			// mallocd by caller
	unsigned		*hmacLen)		// IN/OUT
{
	if(*hmacLen < hmacCtx->macSize) {
		return errSSLInternal;
	}
	CCHmacFinal(&hmacCtx->ccHmac, hmac);
	*hmacLen = hmacCtx->macSize;
	return noErr;
}

/* one-shot */
static OSStatus HMAC_Hmac (
	HMACContextRef	hmacCtx,
	const void		*data,
	unsigned		dataLen,
	void			*hmac,			// mallocd by caller
	unsigned		*hmacLen)		// IN/OUT
{
	OSStatus serr;
	const HMACReference	*hmacRef;

	if(hmacCtx == NULL) {
		return errSSLInternal;
	}
	hmacRef = hmacCtx->hmac;
	assert(hmacRef != NULL);
	serr = hmacRef->init(hmacCtx);
	if(serr) {
		return serr;
	}
	serr = hmacRef->update(hmacCtx, data, dataLen);
	if(serr) {
		return serr;
	}
	return hmacRef->final(hmacCtx, hmac, hmacLen);
}

#else

/* Per-session state, opaque to callers; all fields set at alloc time */
struct HMACContext {
	SSLContext                  *ctx;
	const HashReference         *digest;
	SSLBuffer				    outerHashCtx;
	SSLBuffer				    innerHashCtx;
	SSLBuffer				    currentHashCtx;
};

#pragma mark -
#pragma mark Common HMAC routines

/* Create an HMAC session */
static OSStatus HMAC_Alloc(
	const struct HMACReference	*hmac,
	SSLContext 					*ctx,
	const void					*keyPtr,
	size_t                      keyLen,
	HMACContextRef				*hmacCtx)			// RETURNED
{
	const HashReference	*digest;
	HMACContextRef href;
	size_t ix;
	uint8_t *context;
	const uint8_t *key;
    size_t digest_block_size;

	switch(hmac->alg) {
		case HA_SHA384:
			digest = &SSLHashSHA384;
            digest_block_size = 128;
			break;
		case HA_SHA256:
			digest = &SSLHashSHA256;
            digest_block_size = 64;
			break;
		case HA_SHA1:
			digest = &SSLHashSHA1;
            digest_block_size = 64;
			break;
		case HA_MD5:
			digest = &SSLHashMD5;
            digest_block_size = 64;
			break;
		default:
			assert(0);
			return errSSLInternal;
	}

	context = (uint8_t *)sslMalloc(sizeof(struct HMACContext) +
                                   3 * digest->contextSize);
	if(context == NULL)
		return memFullErr;
	href = (HMACContextRef)context;
	href->ctx = ctx;
    href->digest = digest;
	href->outerHashCtx.data = context + sizeof(*href);
	href->outerHashCtx.length = digest->contextSize;
	href->innerHashCtx.data = href->outerHashCtx.data + digest->contextSize;
	href->innerHashCtx.length = digest->contextSize;
	href->currentHashCtx.data = href->innerHashCtx.data + digest->contextSize;
	href->currentHashCtx.length = digest->contextSize;

	digest->init(&href->outerHashCtx, ctx);
	digest->init(&href->innerHashCtx, ctx);

    uint8_t tmpkey[digest->digestSize];
	uint8_t pad[digest_block_size];
	SSLBuffer kpad = { digest_block_size, pad };

	/* If the key is longer than digest_block_size, reset it to key=digest(key) */
	if (keyLen <= digest_block_size) {
    	key = (const uint8_t *)keyPtr;
    } else {
        SSLBuffer keyBuffer = { keyLen, (uint8_t *)keyPtr };
        SSLBuffer outBuffer = { digest->digestSize, tmpkey };
    	digest->update(&href->innerHashCtx, &keyBuffer);
    	digest->final(&href->innerHashCtx, &outBuffer);
        key = outBuffer.data;
        keyLen = outBuffer.length;
        /* Re-initialize the inner context. */
        digest->init(&href->innerHashCtx, ctx);
	}

	/* Copy the key into k_opad while doing the XOR. */
	for (ix = 0; ix < keyLen; ++ix)
		pad[ix] = key[ix] ^ 0x5c;
	memset(pad + keyLen, 0x5c, digest_block_size - keyLen);
	digest->update(&href->outerHashCtx, &kpad);

	/* Copy the key into k_ipad while doing the XOR. */
	for (ix = 0; ix < keyLen; ++ix)
		pad[ix] = key[ix] ^ 0x36;
	memset(pad + keyLen, 0x36, digest_block_size - keyLen);
	digest->update(&href->innerHashCtx, &kpad);

	/* Clear out the key bits in pad. */
	bzero(pad, keyLen);

	/* Now clone the inner digest so we are ready to receive an update. */
	/* @@@ If init is always called before update we could skip this step. */
	digest->clone(&href->innerHashCtx, &href->currentHashCtx);

	/* success */
	*hmacCtx = href;
	return noErr;
}

/* free a session */
static OSStatus HMAC_Free(
	HMACContextRef	hmacCtx)
{
	if(hmacCtx != NULL) {
		hmacCtx->digest->close(&hmacCtx->outerHashCtx, hmacCtx->ctx);
		hmacCtx->digest->close(&hmacCtx->innerHashCtx, hmacCtx->ctx);
		hmacCtx->digest->close(&hmacCtx->currentHashCtx, hmacCtx->ctx);

		/* Clear out any key material left in the digest contexts. */
		bzero(hmacCtx->outerHashCtx.data, hmacCtx->outerHashCtx.length);
		bzero(hmacCtx->innerHashCtx.data, hmacCtx->innerHashCtx.length);
		bzero(hmacCtx->currentHashCtx.data, hmacCtx->currentHashCtx.length);

		sslFree(hmacCtx);
	}
	return noErr;
}

/* Reusable init */
static OSStatus HMAC_Init(
	HMACContextRef	hmacCtx)
{
	if(hmacCtx == NULL)
		return errSSLInternal;

	assert(hmacCtx->digest != NULL);

	hmacCtx->digest->close(&hmacCtx->currentHashCtx, hmacCtx->ctx);
	hmacCtx->digest->clone(&hmacCtx->innerHashCtx, &hmacCtx->currentHashCtx);

	return noErr;
}

/* normal crypt ops */
static OSStatus HMAC_Update(
	HMACContextRef	hmacCtx,
	const void		*data,
	size_t          dataLen)
{
	SSLBuffer       cdata = { dataLen, (uint8_t *)data };
	if(hmacCtx == NULL)
		return errSSLInternal;

	assert(hmacCtx->digest != NULL);

	hmacCtx->digest->update(&hmacCtx->currentHashCtx, &cdata);

	return noErr;
}

static OSStatus HMAC_Final(
	HMACContextRef	hmacCtx,
	void			*hmac,			// mallocd by caller
	size_t          *hmacLen)		// IN/OUT
{
	uint8_t			bytes[TLS_HMAC_MAX_SIZE];
	SSLBuffer       digest = { TLS_HMAC_MAX_SIZE, bytes };
	SSLBuffer       cdata;

	if(hmacCtx == NULL) {
		return errSSLInternal;
	}
	if((hmac == NULL) || (hmacLen == NULL)) {
		return errSSLInternal;
	}
	assert(hmacCtx->digest != NULL);
	assert(*hmacLen >= hmacCtx->digest->digestSize);

	cdata.length = *hmacLen;
	cdata.data = (uint8_t *)hmac;

	hmacCtx->digest->final(&hmacCtx->currentHashCtx, &digest);
	hmacCtx->digest->clone(&hmacCtx->outerHashCtx, &hmacCtx->currentHashCtx);
	hmacCtx->digest->update(&hmacCtx->currentHashCtx, &digest);
	bzero(bytes, hmacCtx->digest->digestSize);
	hmacCtx->digest->final(&hmacCtx->currentHashCtx, &cdata);
	*hmacLen = hmacCtx->digest->digestSize;

	return noErr;
}

/* one-shot */
static OSStatus HMAC_Hmac (
	HMACContextRef	hmacCtx,
	const void		*data,
	size_t          dataLen,
	void			*hmac,			// mallocd by caller
	size_t          *hmacLen)		// IN/OUT
{
	OSStatus serr;

	if(hmacCtx == NULL) {
		return errSSLInternal;
	}
	serr = HMAC_Init(hmacCtx);
	if(serr) {
		return serr;
	}
	serr = HMAC_Update(hmacCtx, data, dataLen);
	if(serr) {
		return serr;
	}
	return HMAC_Final(hmacCtx, hmac, hmacLen);
}

#endif /* !USE_CDSA_CRYPTO */

#pragma mark -
#pragma mark Null HMAC

static OSStatus HMAC_AllocNull(
	const struct HMACReference	*hmac,
	SSLContext 					*ctx,
	const void					*keyPtr,
	size_t                      keyLen,
	HMACContextRef				*hmacCtx)			// RETURNED
{
	*hmacCtx = NULL;
	return noErr;
}

static OSStatus HMAC_FreeNull(
	HMACContextRef	hmacCtx)
{
	return noErr;
}

static OSStatus HMAC_InitNull(
	HMACContextRef	hmacCtx)
	{
	return noErr;
}

static OSStatus HMAC_UpdateNull(
	HMACContextRef	hmacCtx,
	const void		*data,
	size_t          dataLen)
{
	return noErr;
}

static OSStatus HMAC_FinalNull(
	HMACContextRef	hmacCtx,
	void			*hmac,			// mallocd by caller
	size_t          *hmacLen)		// IN/OUT
{
	return noErr;
}

static OSStatus HMAC_HmacNull (
	HMACContextRef	hmacCtx,
	const void		*data,
	size_t          dataLen,
	void			*hmac,			// mallocd by caller
	size_t          *hmacLen)
{
	return noErr;
}

const HMACReference TlsHmacNull = {
	0,
	HA_Null,
	HMAC_AllocNull,
	HMAC_FreeNull,
	HMAC_InitNull,
	HMAC_UpdateNull,
	HMAC_FinalNull,
	HMAC_HmacNull
};

const HMACReference TlsHmacMD5 = {
	16,
	HA_MD5,
	HMAC_Alloc,
	HMAC_Free,
	HMAC_Init,
	HMAC_Update,
	HMAC_Final,
	HMAC_Hmac
};

const HMACReference TlsHmacSHA1 = {
	20,
	HA_SHA1,
	HMAC_Alloc,
	HMAC_Free,
	HMAC_Init,
	HMAC_Update,
	HMAC_Final,
	HMAC_Hmac
};

const HMACReference TlsHmacSHA256 = {
	32,
	HA_SHA256,
	HMAC_Alloc,
	HMAC_Free,
	HMAC_Init,
	HMAC_Update,
	HMAC_Final,
	HMAC_Hmac
};

const HMACReference TlsHmacSHA384 = {
	48,
	HA_SHA384,
	HMAC_Alloc,
	HMAC_Free,
	HMAC_Init,
	HMAC_Update,
	HMAC_Final,
	HMAC_Hmac
};

const HashHmacReference HashHmacNull = {
	&SSLHashNull,
	&TlsHmacNull
};

const HashHmacReference HashHmacMD5 = {
	&SSLHashMD5,
	&TlsHmacMD5
};

const HashHmacReference HashHmacSHA1 = {
	&SSLHashSHA1,
	&TlsHmacSHA1
};

const HashHmacReference HashHmacSHA256 = {
	&SSLHashSHA256,
	&TlsHmacSHA256
};

const HashHmacReference HashHmacSHA384 = {
	&SSLHashSHA384,
	&TlsHmacSHA384
};

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

/* THIS FILE CONTAINS KERNEL CODE */

#include "tls_hmac.h"
#include "tls_digest.h"
#include "sslMemory.h"
#include "sslDebug.h"
#include <string.h>
#include <AssertMacros.h>


/* Per-session state, opaque to callers; all fields set at alloc time */
struct HMACContext {
	const HashReference         *digest;
	SSLBuffer				    outerHashCtx;
	SSLBuffer				    innerHashCtx;
	SSLBuffer				    currentHashCtx;
};

// MARK: -
// MARK: Common HMAC routines

/* Create an HMAC session */
static int HMAC_Alloc(
	const struct HMACReference	*hmac,
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
			check(0);
			return -1;
	}

	context = (uint8_t *)sslMalloc(sizeof(struct HMACContext) +
                                   3 * digest->contextSize);
	if(context == NULL) {
        check(0);
		return -1;
    }
	href = (HMACContextRef)context;
    href->digest = digest;
	href->outerHashCtx.data = context + sizeof(*href);
	href->outerHashCtx.length = digest->contextSize;
	href->innerHashCtx.data = href->outerHashCtx.data + digest->contextSize;
	href->innerHashCtx.length = digest->contextSize;
	href->currentHashCtx.data = href->innerHashCtx.data + digest->contextSize;
	href->currentHashCtx.length = digest->contextSize;

	digest->init(&href->outerHashCtx);
	digest->init(&href->innerHashCtx);

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
        digest->init(&href->innerHashCtx);
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
	return 0;
}

/* free a session */
static int HMAC_Free(
	HMACContextRef	hmacCtx)
{
	if(hmacCtx != NULL) {
		hmacCtx->digest->close(&hmacCtx->outerHashCtx);
		hmacCtx->digest->close(&hmacCtx->innerHashCtx);
		hmacCtx->digest->close(&hmacCtx->currentHashCtx);

		/* Clear out any key material left in the digest contexts. */
		bzero(hmacCtx->outerHashCtx.data, hmacCtx->outerHashCtx.length);
		bzero(hmacCtx->innerHashCtx.data, hmacCtx->innerHashCtx.length);
		bzero(hmacCtx->currentHashCtx.data, hmacCtx->currentHashCtx.length);

		sslFree(hmacCtx);
	}
	return 0;
}

/* Reusable init */
static int HMAC_Init(
	HMACContextRef	hmacCtx)
{
	if(hmacCtx == NULL) {
        check(0);
		return -1;
    }

	check(hmacCtx->digest != NULL);

	hmacCtx->digest->close(&hmacCtx->currentHashCtx);
	hmacCtx->digest->clone(&hmacCtx->innerHashCtx, &hmacCtx->currentHashCtx);

	return 0;
}

/* normal crypt ops */
static int HMAC_Update(
	HMACContextRef	hmacCtx,
	const void		*data,
	size_t          dataLen)
{
	SSLBuffer       cdata = { dataLen, (uint8_t *)data };
	if(hmacCtx == NULL) {
        check(0);
		return -1;
    }

	check(hmacCtx->digest != NULL);

	hmacCtx->digest->update(&hmacCtx->currentHashCtx, &cdata);

	return 0;
}
	
static int HMAC_Final(
	HMACContextRef	hmacCtx,
	void			*hmac,			// mallocd by caller
	size_t          *hmacLen)		// IN/OUT
{
	uint8_t			bytes[TLS_HMAC_MAX_SIZE];
	SSLBuffer       digest = { TLS_HMAC_MAX_SIZE, bytes };
	SSLBuffer       cdata;

	if(hmacCtx == NULL) {
        check(0);
		return -1;
	}
	if((hmac == NULL) || (hmacLen == NULL)) {
        check(0);
		return -1;
	}
	check(hmacCtx->digest != NULL);
	check(*hmacLen >= hmacCtx->digest->digestSize);

	cdata.length = *hmacLen;
	cdata.data = (uint8_t *)hmac;

	hmacCtx->digest->final(&hmacCtx->currentHashCtx, &digest);
	hmacCtx->digest->clone(&hmacCtx->outerHashCtx, &hmacCtx->currentHashCtx);
	hmacCtx->digest->update(&hmacCtx->currentHashCtx, &digest);
	bzero(bytes, hmacCtx->digest->digestSize);
	hmacCtx->digest->final(&hmacCtx->currentHashCtx, &cdata);
	*hmacLen = hmacCtx->digest->digestSize;

	return 0;
}

/* one-shot */
static int HMAC_Hmac (
	HMACContextRef	hmacCtx,
	const void		*data,
	size_t          dataLen,
	void			*hmac,			// mallocd by caller
	size_t          *hmacLen)		// IN/OUT
{
	int serr;
	
	if(hmacCtx == NULL) {
        check(0);
		return -1;
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


// MARK: -
// MARK: Null HMAC

static int HMAC_AllocNull(
	const struct HMACReference	*hmac,
	const void					*keyPtr,
	size_t                      keyLen,
	HMACContextRef				*hmacCtx)			// RETURNED
{
	*hmacCtx = NULL;
	return 0;
}

static int HMAC_FreeNull(
	HMACContextRef	hmacCtx)
{
	return 0;
}

static int HMAC_InitNull(
	HMACContextRef	hmacCtx)
	{
	return 0;
}

static int HMAC_UpdateNull(
	HMACContextRef	hmacCtx,
	const void		*data,
	size_t          dataLen)
{
	return 0;
}

static int HMAC_FinalNull(
	HMACContextRef	hmacCtx,
	void			*hmac,			// mallocd by caller
	size_t          *hmacLen)		// IN/OUT
{
	return 0;
}

static int HMAC_HmacNull (
	HMACContextRef	hmacCtx,
	const void		*data,
	size_t          dataLen,
	void			*hmac,			// mallocd by caller
	size_t          *hmacLen)
{
	return 0;
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

/*
 * Copyright (c) 2002 Apple Computer, Inc. All Rights Reserved.
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
	File:		tls_hmac.c

	Contains:	HMAC routines used by TLS

	Written by:	Doug Mitchell
*/

#include "tls_hmac.h"
#include "appleCdsa.h"
#include "sslMemory.h"
#include "cryptType.h"
#include "sslDigests.h"
#include "sslDebug.h"
#include <strings.h>
#include <assert.h>
#include <CommonCrypto/CommonHMAC.h>
#include <CoreServices/../Frameworks/CarbonCore.framework/Headers/MacErrors.h>


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

#pragma mark *** CommonCryptor HMAC routines ***

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

#pragma mark *** Null HMAC ***

static OSStatus HMAC_AllocNull(
	const struct HMACReference	*hmac,
	SSLContext 					*ctx,
	const void					*keyPtr,
	unsigned					keyLen,
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
	unsigned		dataLen)
{
	return noErr;
}

static OSStatus HMAC_FinalNull(
	HMACContextRef	hmacCtx,
	void			*hmac,			// mallocd by caller
	unsigned		*hmacLen)		// IN/OUT
{
	return noErr;
}

static OSStatus HMAC_HmacNull (
	HMACContextRef	hmacCtx,
	const void		*data,
	unsigned		dataLen,
	void			*hmac,			// mallocd by caller
	unsigned		*hmacLen)
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

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
#include "sslalloc.h"
#include "sslerrs.h"
#include "cryptType.h"
#include "digests.h"
#include <strings.h>
#include <assert.h>
#include <Security/cssm.h>

/* Per-session state, opaque to callers; all fields set at alloc time */
struct HMACContext {
	SSLContext					*ctx;
	CSSM_CC_HANDLE				ccHand;
	const struct HMACReference	*hmac;
};

#pragma mark *** Common CDSA_based HMAC routines ***

/* Create an HMAC session */
static SSLErr HMAC_Alloc(
	const struct HMACReference	*hmac,
	SSLContext 					*ctx,
	const void					*keyPtr,
	unsigned					keyLen,
	HMACContextRef				*hmacCtx)			// RETURNED
{
	CSSM_RETURN 	crtn;
	CSSM_KEY		cssmKey;
	SSLErr			serr;
	CSSM_ALGORITHMS	calg;
	HMACContextRef 	href = sslMalloc(sizeof(struct HMACContext));
	
	if(href == NULL) {
		return SSLMemoryErr;
	}
	href->ctx = ctx;
	href->ccHand = 0;
	href->hmac = hmac;
	
	/*
	 * Since the key is present in the CDSA context, we cook up the context now.
	 * Currently we can't reuse an HMAC context if the key changes. 
	 */
	switch(hmac->alg) {
		case HA_SHA1:
			calg = CSSM_ALGID_SHA1HMAC;
			break;
		case HA_MD5:
			calg = CSSM_ALGID_MD5HMAC;
			break;
		default:
			assert(0);
			return SSLInternalError;
	}
	serr = sslSetUpSymmKey(&cssmKey,
		calg,
		CSSM_KEYUSE_SIGN | CSSM_KEYUSE_VERIFY,
		CSSM_FALSE,			/* don't malloc/copy key */
		(uint8 *)keyPtr,
		keyLen);
	if(serr) {
		return serr;
	}
	if(attachToCsp(ctx)) {
		return serr;
	}
	crtn = CSSM_CSP_CreateMacContext(ctx->cspHand,
		calg,
		&cssmKey,
		&href->ccHand);
	if(crtn) {
		return SSLCryptoError;
	}
	
	/* success */
	*hmacCtx = href;
	return SSLNoErr;
}

/* free a session */
static SSLErr HMAC_Free(
	HMACContextRef	hmacCtx)
{
	if(hmacCtx != NULL) {
		if(hmacCtx->ccHand != 0) {
			CSSM_DeleteContext(hmacCtx->ccHand);
			hmacCtx->ccHand = 0;
		}
		sslFree(hmacCtx);
	}
	return SSLNoErr;
}

/* Reusable init */
static SSLErr HMAC_Init(
	HMACContextRef	hmacCtx)
{
	CSSM_RETURN crtn;
	
	if(hmacCtx == NULL) {
		return SSLInternalError;
	}
	assert(hmacCtx->ctx != NULL);
	assert(hmacCtx->hmac != NULL);
	assert(hmacCtx->ccHand != 0);
	
	crtn = CSSM_GenerateMacInit(hmacCtx->ccHand);
	if(crtn) {
		return SSLCryptoError;
	}
	return SSLNoErr;
}

/* normal crypt ops */
static SSLErr HMAC_Update(
	HMACContextRef	hmacCtx,
	const void		*data,
	unsigned		dataLen)
{
	CSSM_RETURN crtn;
	CSSM_DATA	cdata;
	
	if(hmacCtx == NULL) {
		return SSLInternalError;
	}
	assert(hmacCtx->ctx != NULL);
	assert(hmacCtx->hmac != NULL);
	assert(hmacCtx->ccHand != 0);
	cdata.Data = (uint8 *)data;
	cdata.Length = dataLen;
	crtn = CSSM_GenerateMacUpdate(hmacCtx->ccHand, &cdata, 1);
	if(crtn) {
		return SSLCryptoError;
	}
	return SSLNoErr;
}
	
static SSLErr HMAC_Final(
	HMACContextRef	hmacCtx,
	void			*hmac,			// mallocd by caller
	unsigned		*hmacLen)		// IN/OUT
{
	CSSM_RETURN crtn;
	CSSM_DATA	cdata;
	
	if(hmacCtx == NULL) {
		return SSLInternalError;
	}
	if((hmac == NULL) || (hmacLen == 0)) {
		return SSLInternalError;
	}
	assert(hmacCtx->ctx != NULL);
	assert(hmacCtx->hmac != NULL);
	assert(hmacCtx->ccHand != 0);
	cdata.Data = (uint8 *)hmac;
	cdata.Length = *hmacLen;
	crtn = CSSM_GenerateMacFinal(hmacCtx->ccHand, &cdata);
	if(crtn) {
		return SSLCryptoError;
	}
	*hmacLen = cdata.Length;
	return SSLNoErr;
}

/* one-shot */
static SSLErr HMAC_Hmac (
	HMACContextRef	hmacCtx,
	const void		*data,
	unsigned		dataLen,
	void			*hmac,			// mallocd by caller
	unsigned		*hmacLen)		// IN/OUT
{
	SSLErr serr;
	const HMACReference	*hmacRef;
	
	if(hmacCtx == NULL) {
		return SSLInternalError;
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

static SSLErr HMAC_AllocNull(
	const struct HMACReference	*hmac,
	SSLContext 					*ctx,
	const void					*keyPtr,
	unsigned					keyLen,
	HMACContextRef				*hmacCtx)			// RETURNED
{
	*hmacCtx = NULL;
	return SSLNoErr;
}

static SSLErr HMAC_FreeNull(
	HMACContextRef	hmacCtx)
{
	return SSLNoErr;
}

static SSLErr HMAC_InitNull(
	HMACContextRef	hmacCtx)
	{
	return SSLNoErr;
}

static SSLErr HMAC_UpdateNull(
	HMACContextRef	hmacCtx,
	const void		*data,
	unsigned		dataLen)
{
	return SSLNoErr;
}

static SSLErr HMAC_FinalNull(
	HMACContextRef	hmacCtx,
	void			*hmac,			// mallocd by caller
	unsigned		*hmacLen)		// IN/OUT
{
	return SSLNoErr;
}

static SSLErr HMAC_HmacNull (
	HMACContextRef	hmacCtx,
	const void		*data,
	unsigned		dataLen,
	void			*hmac,			// mallocd by caller
	unsigned		*hmacLen)
{
	return SSLNoErr;
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

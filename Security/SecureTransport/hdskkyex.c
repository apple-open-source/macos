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
	File:		hdskkyex.c

	Contains:	Support for key exchange and server key exchange

	Written by:	Doug Mitchell, based on Netscape SSLRef 3.0

	Copyright: (c) 1999 by Apple Computer, Inc., all rights reserved.

*/
/*  *********************************************************************
    File: hdskkyex.c

    SSLRef 3.0 Final -- 11/19/96

    Copyright (c)1996 by Netscape Communications Corp.

    By retrieving this software you are bound by the licensing terms
    disclosed in the file "LICENSE.txt". Please read it, and if you don't
    accept the terms, delete this software.

    SSLRef 3.0 was developed by Netscape Communications Corp. of Mountain
    View, California <http://home.netscape.com/> and Consensus Development
    Corporation of Berkeley, California <http://www.consensus.com/>.

    *********************************************************************

    File: hdskkyex.c   Support for key exchange and server key exchange

    Encoding and decoding of key exchange and server key exchange
    messages in both the Diffie-Hellman and RSA variants; also, includes
    the necessary crypto library calls to support this negotiation.

    ****************************************************************** */

#ifndef _SSLCTX_H_
#include "sslctx.h"
#endif

#ifndef _SSLHDSHK_H_
#include "sslhdshk.h"
#endif

#ifndef _SSLALLOC_H_
#include "sslalloc.h"
#endif

#ifndef	_SSL_DEBUG_H_
#include "sslDebug.h"
#endif

#ifndef _SSLUTIL_H_
#include "sslutil.h"
#endif

#ifndef	_APPLE_CDSA_H_
#include "appleCdsa.h"
#endif

#ifndef	_DIGESTS_H_
#include "digests.h"
#endif

#include <assert.h>
#include <string.h>

/*
 * Client RSA Key Exchange msgs actually start with a two-byte
 * length field, contrary to the first version of RFC 2246, dated
 * January 1999. See RFC 2246, March 2002, section 7.4.7.1 for 
 * updated requirements. 
 */
#define RSA_CLIENT_KEY_ADD_LENGTH		1

typedef	CSSM_KEY_PTR	SSLRSAPrivateKey;

static SSLErr SSLEncodeRSAServerKeyExchange(SSLRecord *keyExch, SSLContext *ctx);
static SSLErr SSLEncodeRSAKeyParams(SSLBuffer *keyParams, SSLRSAPrivateKey *key, SSLContext *ctx);
static SSLErr SSLProcessRSAServerKeyExchange(SSLBuffer message, SSLContext *ctx);
static SSLErr SSLDecodeRSAKeyExchange(SSLBuffer keyExchange, SSLContext *ctx);
static SSLErr SSLEncodeRSAKeyExchange(SSLRecord *keyExchange, SSLContext *ctx);
#if	APPLE_DH
static SSLErr SSLEncodeDHanonServerKeyExchange(SSLRecord *keyExch, SSLContext *ctx);
static SSLErr SSLEncodeDHanonKeyExchange(SSLRecord *keyExchange, SSLContext *ctx);
static SSLErr SSLDecodeDHanonKeyExchange(SSLBuffer keyExchange, SSLContext *ctx);
static SSLErr SSLProcessDHanonServerKeyExchange(SSLBuffer message, SSLContext *ctx);
#endif

SSLErr
SSLEncodeServerKeyExchange(SSLRecord *keyExch, SSLContext *ctx)
{   SSLErr      err;
    
    switch (ctx->selectedCipherSpec->keyExchangeMethod)
    {   case SSL_RSA:
        case SSL_RSA_EXPORT:
            if (ERR(err = SSLEncodeRSAServerKeyExchange(keyExch, ctx)) != 0)
                return err;
            break;
        #if		APPLE_DH
        case SSL_DH_anon:
            if (ERR(err = SSLEncodeDHanonServerKeyExchange(keyExch, ctx)) != 0)
                return err;
            break;
        #endif
        default:
            return ERR(SSLUnsupportedErr);
    }
    
    return SSLNoErr;
}

static SSLErr
SSLEncodeRSAServerKeyExchange(SSLRecord *keyExch, SSLContext *ctx)
{   SSLErr          err;
    UInt8           *progress;
    int             length;
    UInt32	    	outputLen, localKeyModulusLen;
    UInt8           hashes[36];
    SSLBuffer       exportKey,clientRandom,serverRandom,hashCtx, hash;
    
    exportKey.data = 0;
    hashCtx.data = 0;
    
    /* we have a public key here... */
    CASSERT(ctx->encryptPubKey != NULL);
    CASSERT(ctx->protocolSide == SSL_ServerSide);
    
    if ((err = SSLEncodeRSAKeyParams(&exportKey, &ctx->encryptPubKey, ctx)) != 0)
        goto fail;
    
	CASSERT(ctx->signingPubKey != NULL);
	localKeyModulusLen = sslKeyLengthInBytes(ctx->signingPubKey);
    
    length = exportKey.length + 2 + localKeyModulusLen;     /* RSA ouputs a block as long as the modulus */
    
 	assert((ctx->negProtocolVersion == SSL_Version_3_0) ||
		   (ctx->negProtocolVersion == TLS_Version_1_0));
    keyExch->protocolVersion = ctx->negProtocolVersion;
    keyExch->contentType = SSL_handshake;
    if (ERR(err = SSLAllocBuffer(&keyExch->contents, length+4, &ctx->sysCtx)) != 0)
        goto fail;
    
    progress = keyExch->contents.data;
    *progress++ = SSL_server_key_exchange;
    progress = SSLEncodeInt(progress, length, 3);
    
    memcpy(progress, exportKey.data, exportKey.length);
    progress += exportKey.length;
    
    clientRandom.data = ctx->clientRandom;
    clientRandom.length = SSL_CLIENT_SRVR_RAND_SIZE;
    serverRandom.data = ctx->serverRandom;
    serverRandom.length = SSL_CLIENT_SRVR_RAND_SIZE;
    
    hash.data = &hashes[0];
    hash.length = 16;
    if (ERR(err = ReadyHash(&SSLHashMD5, &hashCtx, ctx)) != 0)
        goto fail;
    if (ERR(err = SSLHashMD5.update(hashCtx, clientRandom)) != 0)
        goto fail;
    if (ERR(err = SSLHashMD5.update(hashCtx, serverRandom)) != 0)
        goto fail;
    if (ERR(err = SSLHashMD5.update(hashCtx, exportKey)) != 0)
        goto fail;
    if (ERR(err = SSLHashMD5.final(hashCtx, hash)) != 0)
        goto fail;
    if (ERR(err = SSLFreeBuffer(&hashCtx, &ctx->sysCtx)) != 0)
        goto fail;
    
    hash.data = &hashes[16];
    hash.length = 20;
    if (ERR(err = ReadyHash(&SSLHashSHA1, &hashCtx, ctx)) != 0)
        goto fail;
    if (ERR(err = SSLHashSHA1.update(hashCtx, clientRandom)) != 0)
        goto fail;
    if (ERR(err = SSLHashSHA1.update(hashCtx, serverRandom)) != 0)
        goto fail;
    if (ERR(err = SSLHashSHA1.update(hashCtx, exportKey)) != 0)
        goto fail;
    if (ERR(err = SSLHashSHA1.final(hashCtx, hash)) != 0)
        goto fail;
    if (ERR(err = SSLFreeBuffer(&hashCtx, &ctx->sysCtx)) != 0)
        goto fail;
    
    progress = SSLEncodeInt(progress, localKeyModulusLen, 2);
	err = sslRsaRawSign(ctx,
		ctx->signingPrivKey,
		ctx->signingKeyCsp,
		hashes,
		36,
		progress,
		length,
		&outputLen);
	if(err) {
		goto fail;
	}
    CASSERT(outputLen == localKeyModulusLen);
    
    err = SSLNoErr;
    
fail:
    ERR(SSLFreeBuffer(&hashCtx, &ctx->sysCtx));
    ERR(SSLFreeBuffer(&exportKey, &ctx->sysCtx));
    
    return err;
}

static SSLErr
SSLEncodeRSAKeyParams(SSLBuffer *keyParams, SSLRSAPrivateKey *key, SSLContext *ctx)
{   SSLErr      err;
    SSLBuffer   modulus, exponent;
    UInt8       *progress;
    
	err = sslGetPubKeyBits(ctx,
		*key,
		ctx->encryptKeyCsp,
		&modulus,
		&exponent);
	if(err) {
		SSLFreeBuffer(&modulus, &ctx->sysCtx);
		SSLFreeBuffer(&exponent, &ctx->sysCtx);
		return err;
	}
    
    if (ERR(err = SSLAllocBuffer(keyParams, modulus.length + exponent.length + 4, &ctx->sysCtx)) != 0)
        return err;
    progress = keyParams->data;
    progress = SSLEncodeInt(progress, modulus.length, 2);
    memcpy(progress, modulus.data, modulus.length);
    progress += modulus.length;
    progress = SSLEncodeInt(progress, exponent.length, 2);
    memcpy(progress, exponent.data, exponent.length);

	/* these were mallocd by sslGetPubKeyBits() */
	SSLFreeBuffer(&modulus, &ctx->sysCtx);
	SSLFreeBuffer(&exponent, &ctx->sysCtx);
    return SSLNoErr;
}

#if		APPLE_DH
static SSLErr
SSLEncodeDHanonServerKeyExchange(SSLRecord *keyExch, SSLContext *ctx)
{   SSLErr              err;
    UInt32        		length;
    UInt8               *progress;
    SSLRandomCtx        random;
    int                 rsaErr;
    
#if RSAREF
    length = 6 + ctx->dhAnonParams.primeLen + ctx->dhAnonParams.generatorLen +
                    ctx->dhExchangePublic.length;
    
 	assert((ctx->negProtocolVersion == SSL_Version_3_0) ||
		   (ctx->negProtocolVersion == TLS_Version_1_0));
    keyExch->protocolVersion = ctx->negProtocolVersion;
    keyExch->contentType = SSL_handshake;
    if (ERR(err = SSLAllocBuffer(&keyExch->contents, length+4, &ctx->sysCtx)) != 0)
        return err;
    
    progress = keyExch->contents.data;
    *progress++ = SSL_server_key_exchange;
    progress = SSLEncodeInt(progress, length, 3);
    
    progress = SSLEncodeInt(progress, ctx->dhAnonParams.primeLen, 2);
    memcpy(progress, ctx->dhAnonParams.prime, ctx->dhAnonParams.primeLen);
    progress += ctx->dhAnonParams.primeLen;
    
    progress = SSLEncodeInt(progress, ctx->dhAnonParams.generatorLen, 2);
    memcpy(progress, ctx->dhAnonParams.generator, ctx->dhAnonParams.generatorLen);
    progress += ctx->dhAnonParams.generatorLen;
    
    if (ERR(err = SSLAllocBuffer(&ctx->dhExchangePublic, ctx->peerDHParams.primeLen, &ctx->sysCtx)) != 0)
        return err;
    if (ERR(err = SSLAllocBuffer(&ctx->dhPrivate, ctx->dhExchangePublic.length - 16, &ctx->sysCtx)) != 0)
        return err;

    if (ERR(err = ReadyRandom(&random, ctx)) != 0)
        return err;
    
    if ((rsaErr = R_SetupDHAgreement(ctx->dhExchangePublic.data, ctx->dhPrivate.data,
                    ctx->dhPrivate.length, &ctx->dhAnonParams, &random)) != 0)
    {   err = SSLUnknownErr;
        return err;
    }
    
    progress = SSLEncodeInt(progress, ctx->dhExchangePublic.length, 2);
    memcpy(progress, ctx->dhExchangePublic.data, ctx->dhExchangePublic.length);
    progress += ctx->dhExchangePublic.length;
    
#elif BSAFE
    {   A_DH_KEY_AGREE_PARAMS   *params;
        unsigned int            outputLen;
        
        if ((rsaErr = B_GetAlgorithmInfo((POINTER*)&params, ctx->dhAnonParams, AI_DHKeyAgree)) != 0)
            return SSLUnknownErr;
        if (ERR(err = ReadyRandom(&random, ctx)) != 0)
            return err;
        if (ERR(err = SSLAllocBuffer(&ctx->dhExchangePublic, 128, &ctx->sysCtx)) != 0)
            return err;
        if ((rsaErr = B_KeyAgreePhase1(ctx->dhAnonParams, ctx->dhExchangePublic.data,
                            &outputLen, 128, random, NO_SURR)) != 0)
        {   err = SSLUnknownErr;
            return err;
        }
        ctx->dhExchangePublic.length = outputLen;
        
        length = 6 + params->prime.len + params->base.len + ctx->dhExchangePublic.length;
        
		assert((ctx->negProtocolVersion == SSL_Version_3_0) ||
			   (ctx->negProtocolVersion == TLS_Version_1_0));
        keyExch->protocolVersion = ctx->negProtocolVersion;
        keyExch->contentType = SSL_handshake;
        if (ERR(err = SSLAllocBuffer(&keyExch->contents, length+4, &ctx->sysCtx)) != 0)
            return err;
        
        progress = keyExch->contents.data;
        *progress++ = SSL_server_key_exchange;
        progress = SSLEncodeInt(progress, length, 3);
        
        progress = SSLEncodeInt(progress, params->prime.len, 2);
        memcpy(progress, params->prime.data, params->prime.len);
        progress += params->prime.len;
        
        progress = SSLEncodeInt(progress, params->base.len, 2);
        memcpy(progress, params->base.data, params->base.len);
        progress += params->base.len;
        
        progress = SSLEncodeInt(progress, ctx->dhExchangePublic.length, 2);
        memcpy(progress, ctx->dhExchangePublic.data, ctx->dhExchangePublic.length);
        progress += ctx->dhExchangePublic.length;
    }
#endif /* RSAREF / BSAFE */
        
    ASSERT(progress == keyExch->contents.data + keyExch->contents.length);
    
    return SSLNoErr;
}

#endif	/* APPLE_DH */

SSLErr
SSLProcessServerKeyExchange(SSLBuffer message, SSLContext *ctx)
{   SSLErr      err;
    
    switch (ctx->selectedCipherSpec->keyExchangeMethod)
    {   case SSL_RSA:
        case SSL_RSA_EXPORT:
            if (ERR(err = SSLProcessRSAServerKeyExchange(message, ctx)) != 0)
                return err;
            break;
        #if		APPLE_DH
        case SSL_DH_anon:
            if (ERR(err = SSLProcessDHanonServerKeyExchange(message, ctx)) != 0)
                return err;
            break;
        #endif
        default:
            return ERR(SSLUnsupportedErr);
    }
    
    return SSLNoErr;
}

static SSLErr
SSLProcessRSAServerKeyExchange(SSLBuffer message, SSLContext *ctx)
{   
	SSLErr          err;
    SSLBuffer       tempPubKey, hashOut, hashCtx, clientRandom, serverRandom;
    UInt16          modulusLen, exponentLen, signatureLen;
    UInt8           *progress, *modulus, *exponent, *signature;
    UInt8           hash[36];
    SSLBuffer       signedHashes;
    
    signedHashes.data = 0;
    hashCtx.data = 0;
    
    if (message.length < 2) {
    	errorLog0("SSLProcessRSAServerKeyExchange: msg len error 2\n");
        return ERR(SSLProtocolErr);
    }
    progress = message.data;
    modulusLen = SSLDecodeInt(progress, 2);
    modulus = progress + 2;
    progress += 2+modulusLen;
    if (message.length < 4 + modulusLen) {
    	errorLog0("SSLProcessRSAServerKeyExchange: msg len error 2\n");
        return ERR(SSLProtocolErr);
    }
    exponentLen = SSLDecodeInt(progress, 2);
    exponent = progress + 2;
    progress += 2+exponentLen;
    if (message.length < 6 + modulusLen + exponentLen) {
    	errorLog0("SSLProcessRSAServerKeyExchange: msg len error 2\n");
        return ERR(SSLProtocolErr);
    }
    signatureLen = SSLDecodeInt(progress, 2);
    signature = progress + 2;
    if (message.length != 6 + modulusLen + exponentLen + signatureLen) {
    	errorLog0("SSLProcessRSAServerKeyExchange: msg len error 3\n");
        return ERR(SSLProtocolErr);
    }
    
    clientRandom.data = ctx->clientRandom;
    clientRandom.length = SSL_CLIENT_SRVR_RAND_SIZE;
    serverRandom.data = ctx->serverRandom;
    serverRandom.length = SSL_CLIENT_SRVR_RAND_SIZE;
    tempPubKey.data = message.data;
    tempPubKey.length = modulusLen + exponentLen + 4;
    hashOut.data = hash;
    
    hashOut.length = 16;
    if (ERR(err = ReadyHash(&SSLHashMD5, &hashCtx, ctx)) != 0)
        goto fail;
    if (ERR(err = SSLHashMD5.update(hashCtx, clientRandom)) != 0)
        goto fail;
    if (ERR(err = SSLHashMD5.update(hashCtx, serverRandom)) != 0)
        goto fail;
    if (ERR(err = SSLHashMD5.update(hashCtx, tempPubKey)) != 0)
        goto fail;
    if (ERR(err = SSLHashMD5.final(hashCtx, hashOut)) != 0)
        goto fail;
        
    /* 
     * SHA hash goes right after the MD5 hash 
     */
    hashOut.data = hash + 16; 
    hashOut.length = 20;
    if (ERR(err = SSLFreeBuffer(&hashCtx, &ctx->sysCtx)) != 0)
        goto fail;
    
    if (ERR(err = ReadyHash(&SSLHashSHA1, &hashCtx, ctx)) != 0)
        goto fail;
    if (ERR(err = SSLHashSHA1.update(hashCtx, clientRandom)) != 0)
        goto fail;
    if (ERR(err = SSLHashSHA1.update(hashCtx, serverRandom)) != 0)
        goto fail;
    if (ERR(err = SSLHashSHA1.update(hashCtx, tempPubKey)) != 0)
        goto fail;
    if (ERR(err = SSLHashSHA1.final(hashCtx, hashOut)) != 0)
        goto fail;

	err = sslRsaRawVerify(ctx,
		ctx->peerPubKey,
		ctx->peerPubKeyCsp,
		hash,					/* plaintext */
		36,						/* plaintext length */
		signature,
		signatureLen);
	if(err) {
		errorLog1("SSLProcessRSAServerKeyExchange: sslRsaRawVerify returned %d\n",
			err);
		goto fail;
	}
    
	/* Signature matches; now replace server key with new key */
	{
		SSLBuffer modBuf;
		SSLBuffer expBuf;
		
		/* first free existing peerKey */
		sslFreeKey(ctx->peerPubKeyCsp, 
			&ctx->peerPubKey,
			NULL);					/* no KCItem */
			
		/* and cook up a new one from raw bits */
		modBuf.data = modulus;
		modBuf.length = modulusLen;
		expBuf.data = exponent;
		expBuf.length = exponentLen;
		err = sslGetPubKeyFromBits(ctx,
			&modBuf,
			&expBuf,
			&ctx->peerPubKey,
			&ctx->peerPubKeyCsp);
	}
fail:
    ERR(SSLFreeBuffer(&signedHashes, &ctx->sysCtx));
    ERR(SSLFreeBuffer(&hashCtx, &ctx->sysCtx));
    return err;
}

#if		APPLE_DH
static SSLErr
SSLProcessDHanonServerKeyExchange(SSLBuffer message, SSLContext *ctx)
{   SSLErr          err;
    UInt8           *progress;
    unsigned int    totalLength;
    
    if (message.length < 6) {
    	errorLog1("SSLProcessDHanonServerKeyExchange error: msg len %d\n",
    		message.length);
        return ERR(SSLProtocolErr);
    }
    progress = message.data;
    totalLength = 0;
    
#if RSAREF
    {   SSLBuffer       alloc;
        UInt8           *prime, *generator, *publicVal;
        
        ctx->peerDHParams.primeLen = SSLDecodeInt(progress, 2);
        progress += 2;
        prime = progress;
        progress += ctx->peerDHParams.primeLen;
        totalLength += ctx->peerDHParams.primeLen;
        if (message.length < 6 + totalLength)
            return ERR(SSLProtocolErr);
        
        ctx->peerDHParams.generatorLen = SSLDecodeInt(progress, 2);
        progress += 2;
        generator = progress;
        progress += ctx->peerDHParams.generatorLen;
        totalLength += ctx->peerDHParams.generatorLen;
        if (message.length < 6 + totalLength)
            return ERR(SSLProtocolErr);
            
        ctx->dhPeerPublic.length = SSLDecodeInt(progress, 2);
        progress += 2;
        publicVal = progress;
        progress += ctx->dhPeerPublic.length;
        totalLength += ctx->dhPeerPublic.length;
        if (message.length != 6 + totalLength)
            return ERR(SSLProtocolErr);
        
        ASSERT(progress == message.data + message.length);
        
        if (ERR(err = SSLAllocBuffer(&alloc, ctx->peerDHParams.primeLen +
                                    ctx->peerDHParams.generatorLen, &ctx->sysCtx)) != 0)
            return err;
        
        ctx->peerDHParams.prime = alloc.data;
        memcpy(ctx->peerDHParams.prime, prime, ctx->peerDHParams.primeLen);
        ctx->peerDHParams.generator = alloc.data + ctx->peerDHParams.primeLen;
        memcpy(ctx->peerDHParams.generator, generator, ctx->peerDHParams.generatorLen);
        
        if (ERR(err = SSLAllocBuffer(&ctx->dhPeerPublic,
                                ctx->dhPeerPublic.length, &ctx->sysCtx)) != 0)
            return err;
        
        memcpy(ctx->dhPeerPublic.data, publicVal, ctx->dhPeerPublic.length);
    }
#elif BSAFE
    {   int                     rsaErr;
        unsigned char           *publicVal;
        A_DH_KEY_AGREE_PARAMS   params;
        B_ALGORITHM_METHOD      *chooser[] = { &AM_DH_KEY_AGREE, 0 };

        params.prime.len = SSLDecodeInt(progress, 2);
        progress += 2;
        params.prime.data = progress;
        progress += params.prime.len;
        totalLength += params.prime.len;
        if (message.length < 6 + totalLength)
            return ERR(SSLProtocolErr);
        
        params.base.len = SSLDecodeInt(progress, 2);
        progress += 2;
        params.base.data = progress;
        progress += params.base.len;
        totalLength += params.base.len;
        if (message.length < 6 + totalLength)
            return ERR(SSLProtocolErr);
        
        ctx->dhPeerPublic.length = SSLDecodeInt(progress, 2);
        if (ERR(err = SSLAllocBuffer(&ctx->dhPeerPublic, ctx->dhPeerPublic.length, &ctx->sysCtx)) != 0)
            return err;
        
        progress += 2;
        publicVal = progress;
        progress += ctx->dhPeerPublic.length;
        totalLength += ctx->dhPeerPublic.length;
        memcpy(ctx->dhPeerPublic.data, publicVal, ctx->dhPeerPublic.length);
        if (message.length != 6 + totalLength)
            return ERR(SSLProtocolErr);
        
        params.exponentBits = 8 * ctx->dhPeerPublic.length - 1;
        
        if ((rsaErr = B_CreateAlgorithmObject(&ctx->peerDHParams)) != 0)
            return SSLUnknownErr;
        if ((rsaErr = B_SetAlgorithmInfo(ctx->peerDHParams, AI_DHKeyAgree, (POINTER)&params)) != 0)
            return SSLUnknownErr;
        if ((rsaErr = B_KeyAgreeInit(ctx->peerDHParams, (B_KEY_OBJ) 0, chooser, NO_SURR)) != 0)
            return SSLUnknownErr;
    }
#endif
        
    return SSLNoErr;
}

#endif

SSLErr
SSLProcessKeyExchange(SSLBuffer keyExchange, SSLContext *ctx)
{   SSLErr      err;
    
    switch (ctx->selectedCipherSpec->keyExchangeMethod)
    {   case SSL_RSA:
        case SSL_RSA_EXPORT:
            if (ERR(err = SSLDecodeRSAKeyExchange(keyExchange, ctx)) != 0)
                return err;
            break;
		#if		APPLE_DH
        case SSL_DH_anon:
            if (ERR(err = SSLDecodeDHanonKeyExchange(keyExchange, ctx)) != 0)
                return err;
            break;
        #endif
        default:
            return ERR(SSLUnsupportedErr);
    }
    
    return SSLNoErr;
}

static SSLErr
SSLDecodeRSAKeyExchange(SSLBuffer keyExchange, SSLContext *ctx)
{   SSLErr              err;
    SSLBuffer           result;
    UInt32        		outputLen, localKeyModulusLen;
    CSSM_KEY_PTR    	*key;
    SSLProtocolVersion  version;
    Boolean				useEncryptKey = false;
	UInt8				*src = NULL;
	
    
	/* different key names, also need CSP handle */
	CSSM_CSP_HANDLE		cspHand;
	
	CASSERT(ctx->protocolSide == SSL_ServerSide);
	
	/* 
	 * FIXME - The original SSLRef looked at 
	 * ctx->selectedCipherSpec->keyExchangeMethod to decide which 
	 * key to use (exportKey or localKey). I really don't think we 
	 * want to use that - it's constant. We need to look at 
	 * whether the app specified encrypting certs, right?
	 */
	#if		SSL_SERVER_KEYEXCH_HACK
		/* 
		 * the way we work with Netscape.
		 * FIXME - maybe we should *require* an encryptPrivKey in this
		 * situation?
		 */
		if((ctx->selectedCipherSpec->keyExchangeMethod == SSL_RSA_EXPORT) &&
			(ctx->encryptPrivKey != NULL)) {
			useEncryptKey = true;
		}
		
	#else	/* !SSL_SERVER_KEYEXCH_HACK */
		/* The "correct" way, I think, which doesn't work with Netscape */
		if (ctx->encryptPrivKey) {
			useEncryptKey = true;
		}
	#endif	/* SSL_SERVER_KEYEXCH_HACK */
	if (useEncryptKey) {
		key = &ctx->encryptPrivKey;
		cspHand = ctx->encryptKeyCsp;
	} 
	else {
		key = &ctx->signingPrivKey;
		cspHand = ctx->signingKeyCsp;
	}
    
	localKeyModulusLen = sslKeyLengthInBytes(*key);

	/* 
	 * We have to tolerate incoming key exchange msgs with and without the 
	 * two-byte "encrypted length" field.
	 */
    if (keyExchange.length == localKeyModulusLen) {
		/* no length encoded */
		src = keyExchange.data;
	}
	else if((keyExchange.length == (localKeyModulusLen + 2)) &&
		(ctx->negProtocolVersion >= TLS_Version_1_0)) {
		/* TLS only - skip the length bytes */
		src = keyExchange.data + 2;
	}
	else {
    	errorLog2("SSLDecodeRSAKeyExchange: length error (exp %u got %u)\n",
			(unsigned)localKeyModulusLen, (unsigned)keyExchange.length);
        return ERR(SSLProtocolErr);
	}
    err = SSLAllocBuffer(&result, localKeyModulusLen, &ctx->sysCtx);
	if(err != 0) {
        return err;
	}
	
	err = sslRsaDecrypt(ctx,
		*key,
		cspHand,
		src, 
		localKeyModulusLen,
		result.data,
		48,
		&outputLen);
	if(err) {
		goto fail;
	}
    
    if (outputLen != 48)
    {   
		errorLog0("SSLDecodeRSAKeyExchange: outputLen error\n");
    	ERR(err = SSLProtocolErr);
        goto fail;
    }
    result.length = outputLen;
    
    version = (SSLProtocolVersion)SSLDecodeInt(result.data, 2);
/* Modify this check to check against our maximum version with protocol revisions */
    if (version > ctx->negProtocolVersion && version < SSL_Version_3_0) {
		errorLog0("SSLDecodeRSAKeyExchange: version error\n");
    	ERR(err = SSLProtocolErr);
        goto fail;
    }
    if (ERR(err = SSLAllocBuffer(&ctx->preMasterSecret, 
			SSL_RSA_PREMASTER_SECRET_SIZE, &ctx->sysCtx)) != 0)
        goto fail;
    memcpy(ctx->preMasterSecret.data, result.data, 
		SSL_RSA_PREMASTER_SECRET_SIZE);
    
    err = SSLNoErr;
fail:
    ERR(SSLFreeBuffer(&result, &ctx->sysCtx));
    return err;
}

#if		APPLE_DH
static SSLErr
SSLDecodeDHanonKeyExchange(SSLBuffer keyExchange, SSLContext *ctx)
{   SSLErr          err;
    unsigned int    publicLen;
    int             rsaResult;

    publicLen = SSLDecodeInt(keyExchange.data, 2);
    
#if RSAREF
    if (keyExchange.length != publicLen + 2 ||
        publicLen != ctx->dhAnonParams.primeLen)
        return ERR(SSLProtocolErr);
    
    if (ERR(err = SSLAllocBuffer(&ctx->preMasterSecret, ctx->dhAnonParams.primeLen, &ctx->sysCtx)) != 0)
        return err;
    
    if ((rsaResult = R_ComputeDHAgreedKey (ctx->preMasterSecret.data, ctx->dhPeerPublic.data,
                        ctx->dhPrivate.data, ctx->dhPrivate.length,  &ctx->dhAnonParams)) != 0)
    {   err = SSLUnknownErr;
        return err;
    }
    
#elif BSAFE
    {   unsigned int    amount;
        if (keyExchange.length != publicLen + 2)
            return ERR(SSLProtocolErr);
    
        if (ERR(err = SSLAllocBuffer(&ctx->preMasterSecret, 128, &ctx->sysCtx)) != 0)
            return err;
        
        if ((rsaResult = B_KeyAgreePhase2(ctx->dhAnonParams, ctx->preMasterSecret.data,
            &amount, 128, keyExchange.data+2, publicLen, NO_SURR)) != 0)
            return err;
        
        ctx->preMasterSecret.length = amount;
    }   
#endif
        
    return SSLNoErr;
}
#endif	/* APPLE_DH */

SSLErr
SSLEncodeKeyExchange(SSLRecord *keyExchange, SSLContext *ctx)
{   SSLErr      err;
    
    CASSERT(ctx->protocolSide == SSL_ClientSide);
    
    switch (ctx->selectedCipherSpec->keyExchangeMethod)
    {   case SSL_RSA:
        case SSL_RSA_EXPORT:
            if (ERR(err = SSLEncodeRSAKeyExchange(keyExchange, ctx)) != 0)
                return err;
            break;
        #if		APPLE_DH
        case SSL_DH_anon:
            if (ERR(err = SSLEncodeDHanonKeyExchange(keyExchange, ctx)) != 0)
                return err;
            break;
        #endif
        default:
            return ERR(SSLUnsupportedErr);
    }
    
    return SSLNoErr;
}

static SSLErr
SSLEncodeRSAKeyExchange(SSLRecord *keyExchange, SSLContext *ctx)
{   SSLErr              err;
    UInt32        		outputLen, peerKeyModulusLen;
    UInt32				bufLen;
	UInt8				*dst;
	bool				encodeLen = false;
	
    if (ERR(err = SSLEncodeRSAPremasterSecret(ctx)) != 0)
        return err;
    
    keyExchange->contentType = SSL_handshake;
	assert((ctx->negProtocolVersion == SSL_Version_3_0) ||
			(ctx->negProtocolVersion == TLS_Version_1_0));
    keyExchange->protocolVersion = ctx->negProtocolVersion;
        
	peerKeyModulusLen = sslKeyLengthInBytes(ctx->peerPubKey);
	bufLen = peerKeyModulusLen + 4;
	#if 	RSA_CLIENT_KEY_ADD_LENGTH
	if(ctx->negProtocolVersion >= TLS_Version_1_0) {
		bufLen += 2;
		encodeLen = true;
	}
	#endif
    if (ERR(err = SSLAllocBuffer(&keyExchange->contents, 
		bufLen,&ctx->sysCtx)) != 0)
    {   
        return err;
    }
	dst = keyExchange->contents.data + 4;
	if(encodeLen) {
		dst += 2;
	}
    keyExchange->contents.data[0] = SSL_client_key_exchange;
	
	/* this is the record payload length */
    SSLEncodeInt(keyExchange->contents.data + 1, bufLen - 4, 3);
	if(encodeLen) {
		/* the length of the encrypted pre_master_secret */
		SSLEncodeInt(keyExchange->contents.data + 4, 			
			peerKeyModulusLen, 2);
	}
	err = sslRsaEncrypt(ctx,
		ctx->peerPubKey,
		/* FIXME - maybe this should be ctx->cspHand */
		ctx->peerPubKeyCsp,
		ctx->preMasterSecret.data, 
		SSL_RSA_PREMASTER_SECRET_SIZE,
		dst,
		peerKeyModulusLen,
		&outputLen);
	if(err) {
		return err;
	}
    
    CASSERT(outputLen == encodeLen ? 
		keyExchange->contents.length - 6 :
		keyExchange->contents.length - 4 );
    
    return SSLNoErr;
}

#if		APPLE_DH
static SSLErr
SSLEncodeDHanonKeyExchange(SSLRecord *keyExchange, SSLContext *ctx)
{   SSLErr              err;
    unsigned int        outputLen;
    
    if (ERR(err = SSLEncodeDHPremasterSecret(ctx)) != 0)
        return err;
    
    outputLen = ctx->dhExchangePublic.length + 2;
    
    keyExchange->contentType = SSL_handshake;
	assert((ctx->negProtocolVersion == SSL_Version_3_0) ||
			(ctx->negProtocolVersion == TLS_Version_1_0));
    keyExchange->protocolVersion = ctx->negProtocolVersion;
    
    if (ERR(err = SSLAllocBuffer(&keyExchange->contents,outputLen + 4,&ctx->sysCtx)) != 0)
        return err;
    
    keyExchange->contents.data[0] = SSL_client_key_exchange;
    SSLEncodeInt(keyExchange->contents.data+1, ctx->dhExchangePublic.length+2, 3);
    
    SSLEncodeInt(keyExchange->contents.data+4, ctx->dhExchangePublic.length, 2);
    memcpy(keyExchange->contents.data+6, ctx->dhExchangePublic.data, ctx->dhExchangePublic.length);
    
    return SSLNoErr;
}
#endif


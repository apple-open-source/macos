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
	File:		sslKeyExchange.c

	Contains:	Support for key exchange and server key exchange

	Written by:	Doug Mitchell

	Copyright: (c) 1999 by Apple Computer, Inc., all rights reserved.

*/

#include "sslContext.h"
#include "sslHandshake.h"
#include "sslMemory.h"
#include "sslDebug.h"
#include "sslUtils.h"
#include "appleCdsa.h"
#include "sslDigests.h"

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

static OSStatus SSLEncodeRSAServerKeyExchange(SSLRecord &keyExch, SSLContext *ctx);
static OSStatus SSLEncodeRSAKeyParams(SSLBuffer *keyParams, SSLRSAPrivateKey *key, SSLContext *ctx);
static OSStatus SSLProcessRSAServerKeyExchange(SSLBuffer message, SSLContext *ctx);
static OSStatus SSLDecodeRSAKeyExchange(SSLBuffer keyExchange, SSLContext *ctx);
static OSStatus SSLEncodeRSAKeyExchange(SSLRecord &keyExchange, SSLContext *ctx);
#if	APPLE_DH
static OSStatus SSLEncodeDHanonServerKeyExchange(SSLRecord &keyExch, SSLContext *ctx);
static OSStatus SSLEncodeDHanonKeyExchange(SSLRecord &keyExchange, SSLContext *ctx);
static OSStatus SSLDecodeDHanonKeyExchange(SSLBuffer keyExchange, SSLContext *ctx);
static OSStatus SSLProcessDHanonServerKeyExchange(SSLBuffer message, SSLContext *ctx);
#endif

OSStatus
SSLEncodeServerKeyExchange(SSLRecord &keyExch, SSLContext *ctx)
{   OSStatus      err;
    
    switch (ctx->selectedCipherSpec->keyExchangeMethod)
    {   case SSL_RSA:
        case SSL_RSA_EXPORT:
            if ((err = SSLEncodeRSAServerKeyExchange(keyExch, ctx)) != 0)
                return err;
            break;
        #if		APPLE_DH
        case SSL_DH_anon:
            if ((err = SSLEncodeDHanonServerKeyExchange(keyExch, ctx)) != 0)
                return err;
            break;
        #endif
        default:
            return unimpErr;
    }
    
    return noErr;
}

static OSStatus
SSLEncodeRSAServerKeyExchange(SSLRecord &keyExch, SSLContext *ctx)
{   OSStatus        err;
    UInt8           *charPtr;
    int             length;
    UInt32	    	outputLen, localKeyModulusLen;
    UInt8           hashes[36];
    SSLBuffer       exportKey,clientRandom,serverRandom,hashCtx, hash;
    
    exportKey.data = 0;
    hashCtx.data = 0;
    
    /* we have a public key here... */
    assert(ctx->encryptPubKey != NULL);
    assert(ctx->protocolSide == SSL_ServerSide);
    
    if ((err = SSLEncodeRSAKeyParams(&exportKey, &ctx->encryptPubKey, ctx)) != 0)
        goto fail;
    
	assert(ctx->signingPubKey != NULL);
	localKeyModulusLen = sslKeyLengthInBytes(ctx->signingPubKey);
    
    length = exportKey.length + 2 + localKeyModulusLen;     
								/* RSA ouputs a block as long as the modulus */
    
 	assert((ctx->negProtocolVersion == SSL_Version_3_0) ||
		   (ctx->negProtocolVersion == TLS_Version_1_0));
    keyExch.protocolVersion = ctx->negProtocolVersion;
    keyExch.contentType = SSL_RecordTypeHandshake;
    if ((err = SSLAllocBuffer(keyExch.contents, length+4, ctx)) != 0)
        goto fail;
    
    charPtr = keyExch.contents.data;
    *charPtr++ = SSL_HdskServerKeyExchange;
    charPtr = SSLEncodeInt(charPtr, length, 3);
    
    memcpy(charPtr, exportKey.data, exportKey.length);
    charPtr += exportKey.length;
    
    clientRandom.data = ctx->clientRandom;
    clientRandom.length = SSL_CLIENT_SRVR_RAND_SIZE;
    serverRandom.data = ctx->serverRandom;
    serverRandom.length = SSL_CLIENT_SRVR_RAND_SIZE;
    
    hash.data = &hashes[0];
    hash.length = 16;
    if ((err = ReadyHash(SSLHashMD5, hashCtx, ctx)) != 0)
        goto fail;
    if ((err = SSLHashMD5.update(hashCtx, clientRandom)) != 0)
        goto fail;
    if ((err = SSLHashMD5.update(hashCtx, serverRandom)) != 0)
        goto fail;
    if ((err = SSLHashMD5.update(hashCtx, exportKey)) != 0)
        goto fail;
    if ((err = SSLHashMD5.final(hashCtx, hash)) != 0)
        goto fail;
    if ((err = SSLFreeBuffer(hashCtx, ctx)) != 0)
        goto fail;
    
    hash.data = &hashes[16];
    hash.length = 20;
    if ((err = ReadyHash(SSLHashSHA1, hashCtx, ctx)) != 0)
        goto fail;
    if ((err = SSLHashSHA1.update(hashCtx, clientRandom)) != 0)
        goto fail;
    if ((err = SSLHashSHA1.update(hashCtx, serverRandom)) != 0)
        goto fail;
    if ((err = SSLHashSHA1.update(hashCtx, exportKey)) != 0)
        goto fail;
    if ((err = SSLHashSHA1.final(hashCtx, hash)) != 0)
        goto fail;
    if ((err = SSLFreeBuffer(hashCtx, ctx)) != 0)
        goto fail;
    
    charPtr = SSLEncodeInt(charPtr, localKeyModulusLen, 2);
	err = sslRsaRawSign(ctx,
		ctx->signingPrivKey,
		ctx->signingKeyCsp,
		hashes,
		36,
		charPtr,
		length,
		&outputLen);
	if(err) {
		goto fail;
	}
    assert(outputLen == localKeyModulusLen);
    
    err = noErr;
    
fail:
    SSLFreeBuffer(hashCtx, ctx);
    SSLFreeBuffer(exportKey, ctx);
    
    return err;
}

static OSStatus
SSLEncodeRSAKeyParams(SSLBuffer *keyParams, SSLRSAPrivateKey *key, SSLContext *ctx)
{   OSStatus    err;
    SSLBuffer   modulus, exponent;
    UInt8       *charPtr;
    
	err = sslGetPubKeyBits(ctx,
		*key,
		ctx->encryptKeyCsp,
		&modulus,
		&exponent);
	if(err) {
		SSLFreeBuffer(modulus, ctx);
		SSLFreeBuffer(exponent, ctx);
		return err;
	}
    
    if ((err = SSLAllocBuffer(*keyParams, modulus.length + exponent.length + 4, ctx)) != 0)
        return err;
    charPtr = keyParams->data;
    charPtr = SSLEncodeInt(charPtr, modulus.length, 2);
    memcpy(charPtr, modulus.data, modulus.length);
    charPtr += modulus.length;
    charPtr = SSLEncodeInt(charPtr, exponent.length, 2);
    memcpy(charPtr, exponent.data, exponent.length);

	/* these were mallocd by sslGetPubKeyBits() */
	SSLFreeBuffer(modulus, ctx);
	SSLFreeBuffer(exponent, ctx);
    return noErr;
}

#if		APPLE_DH
static OSStatus
SSLEncodeDHanonServerKeyExchange(SSLRecord &keyExch, SSLContext *ctx)
{   OSStatus            err;
    UInt32        		length;
    UInt8               *charPtr;
    SSLRandomCtx        random;
    int                 rsaErr;
    
#if RSAREF
    length = 6 + ctx->dhAnonParams.primeLen + ctx->dhAnonParams.generatorLen +
                    ctx->dhExchangePublic.length;
    
 	assert((ctx->negProtocolVersion == SSL_Version_3_0) ||
		   (ctx->negProtocolVersion == TLS_Version_1_0));
    keyExch.protocolVersion = ctx->negProtocolVersion;
    keyExch.contentType = SSL_RecordTypeHandshake;
    if ((err = SSLAllocBuffer(keyExch.contents, length+4, ctx)) != 0)
        return err;
    
    charPtr = keyExch.contents.data;
    *charPtr++ = SSL_HdskServerKeyExchange;
    charPtr = SSLEncodeInt(charPtr, length, 3);
    
    charPtr = SSLEncodeInt(charPtr, ctx->dhAnonParams.primeLen, 2);
    memcpy(charPtr, ctx->dhAnonParams.prime, ctx->dhAnonParams.primeLen);
    charPtr += ctx->dhAnonParams.primeLen;
    
    charPtr = SSLEncodeInt(charPtr, ctx->dhAnonParams.generatorLen, 2);
    memcpy(charPtr, ctx->dhAnonParams.generator, ctx->dhAnonParams.generatorLen);
    charPtr += ctx->dhAnonParams.generatorLen;
    
    if ((err = SSLAllocBuffer(ctx->dhExchangePublic, 
			ctx->peerDHParams.primeLen, ctx)) != 0)
        return err;
    if ((err = SSLAllocBuffer(ctx->dhPrivate, 
			ctx->dhExchangePublic.length - 16, ctx)) != 0)
        return err;

    if ((err = ReadyRandom(&random, ctx)) != 0)
        return err;
    
    if ((rsaErr = R_SetupDHAgreement(ctx->dhExchangePublic.data, ctx->dhPrivate.data,
                    ctx->dhPrivate.length, &ctx->dhAnonParams, &random)) != 0)
    {   err = SSLUnknownErr;
        return err;
    }
    
    charPtr = SSLEncodeInt(charPtr, ctx->dhExchangePublic.length, 2);
    memcpy(charPtr, ctx->dhExchangePublic.data, ctx->dhExchangePublic.length);
    charPtr += ctx->dhExchangePublic.length;
    
#elif BSAFE
    {   A_DH_KEY_AGREE_PARAMS   *params;
        unsigned int            outputLen;
        
        if ((rsaErr = B_GetAlgorithmInfo((POINTER*)&params, ctx->dhAnonParams, AI_DHKeyAgree)) != 0)
            return SSLUnknownErr;
        if ((err = ReadyRandom(&random, ctx)) != 0)
            return err;
        if ((err = SSLAllocBuffer(ctx->dhExchangePublic, 128, ctx)) != 0)
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
        keyExch.protocolVersion = ctx->negProtocolVersion;
        keyExch.contentType = SSL_RecordTypeHandshake;
        if ((err = SSLAllocBuffer(keyExch.contents, length+4, ctx)) != 0)
            return err;
        
        charPtr = keyExch.contents.data;
        *charPtr++ = SSL_HdskServerKeyExchange;
        charPtr = SSLEncodeInt(charPtr, length, 3);
        
        charPtr = SSLEncodeInt(charPtr, params->prime.len, 2);
        memcpy(charPtr, params->prime.data, params->prime.len);
        charPtr += params->prime.len;
        
        charPtr = SSLEncodeInt(charPtr, params->base.len, 2);
        memcpy(charPtr, params->base.data, params->base.len);
        charPtr += params->base.len;
        
        charPtr = SSLEncodeInt(charPtr, ctx->dhExchangePublic.length, 2);
        memcpy(charPtr, ctx->dhExchangePublic.data, ctx->dhExchangePublic.length);
        charPtr += ctx->dhExchangePublic.length;
    }
#endif /* RSAREF / BSAFE */
        
    assert(charPtr == keyExch.contents.data + keyExch.contents.length);
    
    return noErr;
}

#endif	/* APPLE_DH */

OSStatus
SSLProcessServerKeyExchange(SSLBuffer message, SSLContext *ctx)
{   OSStatus      err;
    
    switch (ctx->selectedCipherSpec->keyExchangeMethod)
    {   case SSL_RSA:
        case SSL_RSA_EXPORT:
            if ((err = SSLProcessRSAServerKeyExchange(message, ctx)) != 0)
                return err;
            break;
        #if		APPLE_DH
        case SSL_DH_anon:
            if ((err = SSLProcessDHanonServerKeyExchange(message, ctx)) != 0)
                return err;
            break;
        #endif
        default:
            return unimpErr;
    }
    
    return noErr;
}

static OSStatus
SSLProcessRSAServerKeyExchange(SSLBuffer message, SSLContext *ctx)
{   
	OSStatus        err;
    SSLBuffer       tempPubKey, hashOut, hashCtx, clientRandom, serverRandom;
    UInt16          modulusLen, exponentLen, signatureLen;
    UInt8           *charPtr, *modulus, *exponent, *signature;
    UInt8           hash[36];
    SSLBuffer       signedHashes;
    
    signedHashes.data = 0;
    hashCtx.data = 0;
    
    if (message.length < 2) {
    	sslErrorLog("SSLProcessRSAServerKeyExchange: msg len error 2\n");
        return errSSLProtocol;
    }
    charPtr = message.data;
    modulusLen = SSLDecodeInt(charPtr, 2);
    modulus = charPtr + 2;
    charPtr += 2+modulusLen;
    if (message.length < (unsigned)(4 + modulusLen)) {
    	sslErrorLog("SSLProcessRSAServerKeyExchange: msg len error 2\n");
        return errSSLProtocol;
    }
    exponentLen = SSLDecodeInt(charPtr, 2);
    exponent = charPtr + 2;
    charPtr += 2+exponentLen;
    if (message.length < (unsigned)(6 + modulusLen + exponentLen)) {
    	sslErrorLog("SSLProcessRSAServerKeyExchange: msg len error 2\n");
        return errSSLProtocol;
    }
    signatureLen = SSLDecodeInt(charPtr, 2);
    signature = charPtr + 2;
    if (message.length != (unsigned)(6 + modulusLen + exponentLen + signatureLen)) {
    	sslErrorLog("SSLProcessRSAServerKeyExchange: msg len error 3\n");
        return errSSLProtocol;
    }
    
    clientRandom.data = ctx->clientRandom;
    clientRandom.length = SSL_CLIENT_SRVR_RAND_SIZE;
    serverRandom.data = ctx->serverRandom;
    serverRandom.length = SSL_CLIENT_SRVR_RAND_SIZE;
    tempPubKey.data = message.data;
    tempPubKey.length = modulusLen + exponentLen + 4;
    hashOut.data = hash;
    
    hashOut.length = 16;
    if ((err = ReadyHash(SSLHashMD5, hashCtx, ctx)) != 0)
        goto fail;
    if ((err = SSLHashMD5.update(hashCtx, clientRandom)) != 0)
        goto fail;
    if ((err = SSLHashMD5.update(hashCtx, serverRandom)) != 0)
        goto fail;
    if ((err = SSLHashMD5.update(hashCtx, tempPubKey)) != 0)
        goto fail;
    if ((err = SSLHashMD5.final(hashCtx, hashOut)) != 0)
        goto fail;
        
    /* 
     * SHA hash goes right after the MD5 hash 
     */
    hashOut.data = hash + 16; 
    hashOut.length = 20;
    if ((err = SSLFreeBuffer(hashCtx, ctx)) != 0)
        goto fail;
    
    if ((err = ReadyHash(SSLHashSHA1, hashCtx, ctx)) != 0)
        goto fail;
    if ((err = SSLHashSHA1.update(hashCtx, clientRandom)) != 0)
        goto fail;
    if ((err = SSLHashSHA1.update(hashCtx, serverRandom)) != 0)
        goto fail;
    if ((err = SSLHashSHA1.update(hashCtx, tempPubKey)) != 0)
        goto fail;
    if ((err = SSLHashSHA1.final(hashCtx, hashOut)) != 0)
        goto fail;

	err = sslRsaRawVerify(ctx,
		ctx->peerPubKey,
		ctx->peerPubKeyCsp,
		hash,					/* plaintext */
		36,						/* plaintext length */
		signature,
		signatureLen);
	if(err) {
		sslErrorLog("SSLProcessRSAServerKeyExchange: sslRsaRawVerify returned %d\n",
			(int)err);
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
    SSLFreeBuffer(signedHashes, ctx);
    SSLFreeBuffer(hashCtx, ctx);
    return err;
}

#if		APPLE_DH
static OSStatus
SSLProcessDHanonServerKeyExchange(SSLBuffer message, SSLContext *ctx)
{   OSStatus        err;
    UInt8           *charPtr;
    unsigned int    totalLength;
    
    if (message.length < 6) {
    	sslErrorLog("SSLProcessDHanonServerKeyExchange error: msg len %d\n",
    		message.length);
        return errSSLProtocol;
    }
    charPtr = message.data;
    totalLength = 0;
    
#if RSAREF
    {   SSLBuffer       alloc;
        UInt8           *prime, *generator, *publicVal;
        
        ctx->peerDHParams.primeLen = SSLDecodeInt(charPtr, 2);
        charPtr += 2;
        prime = charPtr;
        charPtr += ctx->peerDHParams.primeLen;
        totalLength += ctx->peerDHParams.primeLen;
        if (message.length < 6 + totalLength)
            return errSSLProtocol;
        
        ctx->peerDHParams.generatorLen = SSLDecodeInt(charPtr, 2);
        charPtr += 2;
        generator = charPtr;
        charPtr += ctx->peerDHParams.generatorLen;
        totalLength += ctx->peerDHParams.generatorLen;
        if (message.length < 6 + totalLength)
            return errSSLProtocol;
            
        ctx->dhPeerPublic.length = SSLDecodeInt(charPtr, 2);
        charPtr += 2;
        publicVal = charPtr;
        charPtr += ctx->dhPeerPublic.length;
        totalLength += ctx->dhPeerPublic.length;
        if (message.length != 6 + totalLength)
            return errSSLProtocol;
        
        assert(charPtr == message.data + message.length);
        
        if ((err = SSLAllocBuffer(alloc, ctx->peerDHParams.primeLen +
                                    ctx->peerDHParams.generatorLen, ctx)) != 0)
            return err;
        
        ctx->peerDHParams.prime = alloc.data;
        memcpy(ctx->peerDHParams.prime, prime, ctx->peerDHParams.primeLen);
        ctx->peerDHParams.generator = alloc.data + ctx->peerDHParams.primeLen;
        memcpy(ctx->peerDHParams.generator, generator, ctx->peerDHParams.generatorLen);
        
        if ((err = SSLAllocBuffer(ctx->dhPeerPublic,
                                ctx->dhPeerPublic.length, ctx)) != 0)
            return err;
        
        memcpy(ctx->dhPeerPublic.data, publicVal, ctx->dhPeerPublic.length);
    }
#elif BSAFE
    {   int                     rsaErr;
        unsigned char           *publicVal;
        A_DH_KEY_AGREE_PARAMS   params;
        B_ALGORITHM_METHOD      *chooser[] = { &AM_DH_KEY_AGREE, 0 };

        params.prime.len = SSLDecodeInt(charPtr, 2);
        charPtr += 2;
        params.prime.data = charPtr;
        charPtr += params.prime.len;
        totalLength += params.prime.len;
        if (message.length < 6 + totalLength)
            return errSSLProtocol;
        
        params.base.len = SSLDecodeInt(charPtr, 2);
        charPtr += 2;
        params.base.data = charPtr;
        charPtr += params.base.len;
        totalLength += params.base.len;
        if (message.length < 6 + totalLength)
            return errSSLProtocol;
        
        ctx->dhPeerPublic.length = SSLDecodeInt(charPtr, 2);
        if ((err = SSLAllocBuffer(ctx->dhPeerPublic, ctx->dhPeerPublic.length, ctx)) != 0)
            return err;
        
        charPtr += 2;
        publicVal = charPtr;
        charPtr += ctx->dhPeerPublic.length;
        totalLength += ctx->dhPeerPublic.length;
        memcpy(ctx->dhPeerPublic.data, publicVal, ctx->dhPeerPublic.length);
        if (message.length != 6 + totalLength)
            return errSSLProtocol;
        
        params.exponentBits = 8 * ctx->dhPeerPublic.length - 1;
        
        if ((rsaErr = B_CreateAlgorithmObject(&ctx->peerDHParams)) != 0)
            return SSLUnknownErr;
        if ((rsaErr = B_SetAlgorithmInfo(ctx->peerDHParams, AI_DHKeyAgree, (POINTER)&params)) != 0)
            return SSLUnknownErr;
        if ((rsaErr = B_KeyAgreeInit(ctx->peerDHParams, (B_KEY_OBJ) 0, chooser, NO_SURR)) != 0)
            return SSLUnknownErr;
    }
#endif
        
    return noErr;
}

#endif

OSStatus
SSLProcessKeyExchange(SSLBuffer keyExchange, SSLContext *ctx)
{   OSStatus      err;
    
    switch (ctx->selectedCipherSpec->keyExchangeMethod)
    {   case SSL_RSA:
        case SSL_RSA_EXPORT:
            if ((err = SSLDecodeRSAKeyExchange(keyExchange, ctx)) != 0)
                return err;
            break;
		#if		APPLE_DH
        case SSL_DH_anon:
            if ((err = SSLDecodeDHanonKeyExchange(keyExchange, ctx)) != 0)
                return err;
            break;
        #endif
        default:
            return unimpErr;
    }
    
    return noErr;
}

static OSStatus
SSLDecodeRSAKeyExchange(SSLBuffer keyExchange, SSLContext *ctx)
{   OSStatus            err;
    SSLBuffer           result;
    UInt32        		outputLen, localKeyModulusLen;
    CSSM_KEY_PTR    	*key;
    SSLProtocolVersion  version;
    Boolean				useEncryptKey = false;
	UInt8				*src = NULL;
	
    
	/* different key names, also need CSP handle */
	CSSM_CSP_HANDLE		cspHand;
	
	assert(ctx->protocolSide == SSL_ServerSide);
	
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
    	sslErrorLog("SSLDecodeRSAKeyExchange: length error (exp %u got %u)\n",
			(unsigned)localKeyModulusLen, (unsigned)keyExchange.length);
        return errSSLProtocol;
	}
    err = SSLAllocBuffer(result, localKeyModulusLen, ctx);
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
		sslErrorLog("SSLDecodeRSAKeyExchange: outputLen error\n");
    	err = errSSLProtocol;
        goto fail;
    }
    result.length = outputLen;
    
    version = (SSLProtocolVersion)SSLDecodeInt(result.data, 2);
	/* Modify this check to check against our maximum version with 
	 * protocol revisions */
    if (version > ctx->negProtocolVersion && version < SSL_Version_3_0) {
		sslErrorLog("SSLDecodeRSAKeyExchange: version error\n");
    	err = errSSLProtocol;
        goto fail;
    }
    if ((err = SSLAllocBuffer(ctx->preMasterSecret, 
			SSL_RSA_PREMASTER_SECRET_SIZE, ctx)) != 0)
        goto fail;
    memcpy(ctx->preMasterSecret.data, result.data, 
		SSL_RSA_PREMASTER_SECRET_SIZE);
    
    err = noErr;
fail:
    SSLFreeBuffer(result, ctx);
    return err;
}

#if		APPLE_DH
static OSStatus
SSLDecodeDHanonKeyExchange(SSLBuffer keyExchange, SSLContext *ctx)
{   OSStatus        err;
    unsigned int    publicLen;
    int             rsaResult;

    publicLen = SSLDecodeInt(keyExchange.data, 2);
    
#if RSAREF
    if (keyExchange.length != publicLen + 2 ||
        publicLen != ctx->dhAnonParams.primeLen)
        return errSSLProtocol;
    
    if ((err = SSLAllocBuffer(ctx->preMasterSecret, ctx->dhAnonParams.primeLen, ctx)) != 0)
        return err;
    
    if ((rsaResult = R_ComputeDHAgreedKey (ctx->preMasterSecret.data, ctx->dhPeerPublic.data,
                        ctx->dhPrivate.data, ctx->dhPrivate.length,  &ctx->dhAnonParams)) != 0)
    {   err = SSLUnknownErr;
        return err;
    }
    
#elif BSAFE
    {   unsigned int    amount;
        if (keyExchange.length != publicLen + 2)
            return errSSLProtocol;
    
        if ((err = SSLAllocBuffer(ctx->preMasterSecret, 128, ctx)) != 0)
            return err;
        
        if ((rsaResult = B_KeyAgreePhase2(ctx->dhAnonParams, ctx->preMasterSecret.data,
            &amount, 128, keyExchange.data+2, publicLen, NO_SURR)) != 0)
            return err;
        
        ctx->preMasterSecret.length = amount;
    }   
#endif
        
    return noErr;
}
#endif	/* APPLE_DH */

OSStatus
SSLEncodeKeyExchange(SSLRecord &keyExchange, SSLContext *ctx)
{   OSStatus      err;
    
    assert(ctx->protocolSide == SSL_ClientSide);
    
    switch (ctx->selectedCipherSpec->keyExchangeMethod)
    {   case SSL_RSA:
        case SSL_RSA_EXPORT:
            if ((err = SSLEncodeRSAKeyExchange(keyExchange, ctx)) != 0)
                return err;
            break;
        #if		APPLE_DH
        case SSL_DH_anon:
            if ((err = SSLEncodeDHanonKeyExchange(keyExchange, ctx)) != 0)
                return err;
            break;
        #endif
        default:
            return unimpErr;
    }
    
    return noErr;
}

static OSStatus
SSLEncodeRSAKeyExchange(SSLRecord &keyExchange, SSLContext *ctx)
{   OSStatus            err;
    UInt32        		outputLen, peerKeyModulusLen;
    UInt32				bufLen;
	UInt8				*dst;
	bool				encodeLen = false;
	
    if ((err = SSLEncodeRSAPremasterSecret(ctx)) != 0)
        return err;
    
    keyExchange.contentType = SSL_RecordTypeHandshake;
	assert((ctx->negProtocolVersion == SSL_Version_3_0) ||
			(ctx->negProtocolVersion == TLS_Version_1_0));
    keyExchange.protocolVersion = ctx->negProtocolVersion;
        
	peerKeyModulusLen = sslKeyLengthInBytes(ctx->peerPubKey);
	bufLen = peerKeyModulusLen + 4;
	#if 	RSA_CLIENT_KEY_ADD_LENGTH
	if(ctx->negProtocolVersion >= TLS_Version_1_0) {
		bufLen += 2;
		encodeLen = true;
	}
	#endif
    if ((err = SSLAllocBuffer(keyExchange.contents, 
		bufLen,ctx)) != 0)
    {   
        return err;
    }
	dst = keyExchange.contents.data + 4;
	if(encodeLen) {
		dst += 2;
	}
    keyExchange.contents.data[0] = SSL_HdskClientKeyExchange;
	
	/* this is the record payload length */
    SSLEncodeInt(keyExchange.contents.data + 1, bufLen - 4, 3);
	if(encodeLen) {
		/* the length of the encrypted pre_master_secret */
		SSLEncodeInt(keyExchange.contents.data + 4, 			
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
    
    assert(outputLen == encodeLen ? 
		keyExchange.contents.length - 6 :
		keyExchange.contents.length - 4 );
    
    return noErr;
}

#if		APPLE_DH
static OSStatus
SSLEncodeDHanonKeyExchange(SSLRecord &keyExchange, SSLContext *ctx)
{   OSStatus            err;
    unsigned int        outputLen;
    
    if ((err = SSLEncodeDHPremasterSecret(ctx)) != 0)
        return err;
    
    outputLen = ctx->dhExchangePublic.length + 2;
    
    keyExchange.contentType = SSL_RecordTypeHandshake;
	assert((ctx->negProtocolVersion == SSL_Version_3_0) ||
			(ctx->negProtocolVersion == TLS_Version_1_0));
    keyExchange.protocolVersion = ctx->negProtocolVersion;
    
    if ((err = SSLAllocBuffer(keyExchange.contents,outputLen + 4,ctx)) != 0)
        return err;
    
    keyExchange.contents.data[0] = SSL_HdskClientKeyExchange;
    SSLEncodeInt(keyExchange.contents.data+1, ctx->dhExchangePublic.length+2, 3);
    
    SSLEncodeInt(keyExchange.contents.data+4, ctx->dhExchangePublic.length, 2);
    memcpy(keyExchange.contents.data+6, ctx->dhExchangePublic.data, ctx->dhExchangePublic.length);
    
    return noErr;
}
#endif

OSStatus
SSLEncodeRSAPremasterSecret(SSLContext *ctx)
{   SSLBuffer           randData;
    OSStatus            err;
    
    if ((err = SSLAllocBuffer(ctx->preMasterSecret, 
			SSL_RSA_PREMASTER_SECRET_SIZE, ctx)) != 0)
        return err;
    
	assert((ctx->negProtocolVersion == SSL_Version_3_0) ||
		   (ctx->negProtocolVersion == TLS_Version_1_0));
    SSLEncodeInt(ctx->preMasterSecret.data, ctx->maxProtocolVersion, 2);
    randData.data = ctx->preMasterSecret.data+2;
    randData.length = SSL_RSA_PREMASTER_SECRET_SIZE - 2;
    if ((err = sslRand(ctx, &randData)) != 0)
        return err;
    return noErr;
}

#if	APPLE_DH

OSStatus
SSLEncodeDHPremasterSecret(SSLContext *ctx)
{   
	#if		!APPLE_DH
	return unimpErr;
	#else
	
	OSStatus            err;
    int                 rsaResult;
    SSLRandomCtx        rsaRandom;

/* Given the server's Diffie-Hellman parameters, prepare a public & private value,
 *  then use the public value provided by the server and our private value to
 *  generate a shared key (the premaster secret). Save our public value in
 *  ctx->dhExchangePublic to send to the server so it can calculate the matching
 *  key on its end
 */
    if ((err = ReadyRandom(&rsaRandom, ctx)) != 0)
        return err;
    
#if RSAREF
    {   privateValue.data = 0;
        
        if ((err = SSLAllocBuffer(ctx->dhExchangePublic, ctx->peerDHParams.primeLen, ctx)) != 0)
            goto fail;
        if ((err = SSLAllocBuffer(privateValue, ctx->dhExchangePublic.length - 16, ctx)) != 0)
            goto fail;
        
        if ((rsaResult = R_SetupDHAgreement(ctx->dhExchangePublic.data, privateValue.data,
                            privateValue.length, &ctx->peerDHParams, &rsaRandom)) != 0)
        {   err = SSLUnknownErr;
            goto fail;
        }
        
        if ((err = SSLAllocBuffer(ctx->preMasterSecret, ctx->peerDHParams.primeLen, ctx)) != 0)
            goto fail;
        
        if ((rsaResult = R_ComputeDHAgreedKey (ctx->preMasterSecret.data, ctx->dhPeerPublic.data,
                            privateValue.data, privateValue.length,  &ctx->peerDHParams)) != 0)
        {   err = SSLUnknownErr;
            goto fail;
        }
    }
#elif BSAFE
    {   unsigned int    outputLen;
        
        if ((err = SSLAllocBuffer(ctx->dhExchangePublic, 128, ctx)) != 0)
            goto fail;
        if ((rsaResult = B_KeyAgreePhase1(ctx->peerDHParams, ctx->dhExchangePublic.data,
                            &outputLen, 128, rsaRandom, NO_SURR)) != 0)
        {   err = SSLUnknownErr;
            goto fail;
        }
        ctx->dhExchangePublic.length = outputLen;
        if ((err = SSLAllocBuffer(ctx->preMasterSecret, 128, ctx)) != 0)
            goto fail;
        if ((rsaResult = B_KeyAgreePhase2(ctx->peerDHParams, ctx->preMasterSecret.data,
                            &outputLen, 128, ctx->dhPeerPublic.data, ctx->dhPeerPublic.length,
                            NO_SURR)) != 0)
        {   err = SSLUnknownErr;
            goto fail;
        }
        ctx->preMasterSecret.length = outputLen;
    }
 #endif
    
    err = noErr;
fail:
#if RSAREF
    SSLFreeBuffer(privateValue, ctx);
    R_RandomFinal(&rsaRandom);
#elif BSAFE
    B_DestroyAlgorithmObject(&rsaRandom);
#endif  
    return err;
    #endif
}

#endif	/* APPLE_DH */

OSStatus
SSLInitPendingCiphers(SSLContext *ctx)
{   OSStatus        err;
    SSLBuffer       key;
    UInt8           *keyDataProgress, *keyPtr, *ivPtr;
    int             keyDataLen;
    CipherContext   *serverPending, *clientPending;
        
    key.data = 0;
    
    ctx->readPending.macRef = ctx->selectedCipherSpec->macAlgorithm;
    ctx->writePending.macRef = ctx->selectedCipherSpec->macAlgorithm;
    ctx->readPending.symCipher = ctx->selectedCipherSpec->cipher;
    ctx->writePending.symCipher = ctx->selectedCipherSpec->cipher;
    ctx->readPending.sequenceNum.high = ctx->readPending.sequenceNum.low = 0;
    ctx->writePending.sequenceNum.high = ctx->writePending.sequenceNum.low = 0;
    
    keyDataLen = ctx->selectedCipherSpec->macAlgorithm->hash->digestSize +
                     ctx->selectedCipherSpec->cipher->secretKeySize;
    if (ctx->selectedCipherSpec->isExportable == NotExportable)
        keyDataLen += ctx->selectedCipherSpec->cipher->ivSize;
    keyDataLen *= 2;        /* two of everything */
    
    if ((err = SSLAllocBuffer(key, keyDataLen, ctx)) != 0)
        return err;
	assert(ctx->sslTslCalls != NULL);
    if ((err = ctx->sslTslCalls->generateKeyMaterial(key, ctx)) != 0)
        goto fail;
    
    if (ctx->protocolSide == SSL_ServerSide)
    {   serverPending = &ctx->writePending;
        clientPending = &ctx->readPending;
    }
    else
    {   serverPending = &ctx->readPending;
        clientPending = &ctx->writePending;
    }
    
    keyDataProgress = key.data;
    memcpy(clientPending->macSecret, keyDataProgress, 
		ctx->selectedCipherSpec->macAlgorithm->hash->digestSize);
    keyDataProgress += ctx->selectedCipherSpec->macAlgorithm->hash->digestSize;
    memcpy(serverPending->macSecret, keyDataProgress, 
		ctx->selectedCipherSpec->macAlgorithm->hash->digestSize);
    keyDataProgress += ctx->selectedCipherSpec->macAlgorithm->hash->digestSize;
    
	/* init the reusable-per-record MAC contexts */
	err = ctx->sslTslCalls->initMac(clientPending, ctx);
	if(err) {
		goto fail;
	}
	err = ctx->sslTslCalls->initMac(serverPending, ctx);
	if(err) {
		goto fail;
	}
	
    if (ctx->selectedCipherSpec->isExportable == NotExportable)
    {   keyPtr = keyDataProgress;
        keyDataProgress += ctx->selectedCipherSpec->cipher->secretKeySize;
        /* Skip server write key to get to IV */
        ivPtr = keyDataProgress + ctx->selectedCipherSpec->cipher->secretKeySize;
        if ((err = ctx->selectedCipherSpec->cipher->initialize(keyPtr, ivPtr,
                                    clientPending, ctx)) != 0)
            goto fail;
        keyPtr = keyDataProgress;
        keyDataProgress += ctx->selectedCipherSpec->cipher->secretKeySize;
        /* Skip client write IV to get to server write IV */
        ivPtr = keyDataProgress + ctx->selectedCipherSpec->cipher->ivSize;
        if ((err = ctx->selectedCipherSpec->cipher->initialize(keyPtr, ivPtr,
                                    serverPending, ctx)) != 0)
            goto fail;
    }
    else {
        UInt8		clientExportKey[16], serverExportKey[16], 
					clientExportIV[16],  serverExportIV[16];
        SSLBuffer   clientWrite, serverWrite;
        SSLBuffer	finalClientWrite, finalServerWrite;
		SSLBuffer	finalClientIV, finalServerIV;
		
        assert(ctx->selectedCipherSpec->cipher->keySize <= 16);
        assert(ctx->selectedCipherSpec->cipher->ivSize <= 16);
        
		/* Inputs to generateExportKeyAndIv are clientRandom, serverRandom,
		 *    clientWriteKey, serverWriteKey. The first two are already present
		 *    in ctx.
		 * Outputs are a key and IV for each of {server, client}.
		 */
        clientWrite.data = keyDataProgress;
        clientWrite.length = ctx->selectedCipherSpec->cipher->secretKeySize;
        serverWrite.data = keyDataProgress + clientWrite.length;
        serverWrite.length = ctx->selectedCipherSpec->cipher->secretKeySize;
		finalClientWrite.data = clientExportKey;
		finalServerWrite.data   = serverExportKey;
		finalClientIV.data      = clientExportIV;
		finalServerIV.data      = serverExportIV;
		finalClientWrite.length = 16;
		finalServerWrite.length = 16;
		/* these can be zero */
		finalClientIV.length    = ctx->selectedCipherSpec->cipher->ivSize;
		finalServerIV.length    = ctx->selectedCipherSpec->cipher->ivSize;

		assert(ctx->sslTslCalls != NULL);
		err = ctx->sslTslCalls->generateExportKeyAndIv(ctx, clientWrite, serverWrite,
			finalClientWrite, finalServerWrite, finalClientIV, finalServerIV);
		if(err) {
			goto fail;
		}
        if ((err = ctx->selectedCipherSpec->cipher->initialize(clientExportKey, 
				clientExportIV, clientPending, ctx)) != 0)
            goto fail;
        if ((err = ctx->selectedCipherSpec->cipher->initialize(serverExportKey, 
				serverExportIV, serverPending, ctx)) != 0)
            goto fail;
    }
    
	/* Ciphers are ready for use */
    ctx->writePending.ready = 1;
    ctx->readPending.ready = 1;
    
	/* Ciphers get swapped by sending or receiving a change cipher spec message */
    
    err = noErr;
fail:
    SSLFreeBuffer(key, ctx);
    return err;
}


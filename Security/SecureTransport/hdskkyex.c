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

#include <string.h>

#if	_APPLE_CDSA_
/*
 * For this config, just for this file, we'll do this typedef....
 */
typedef	CSSM_KEY_PTR	SSLRSAPrivateKey;
#endif

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
    
    #if	_APPLE_CDSA_
    /* we have a public key here... */
    CASSERT(ctx->encryptPubKey != NULL);
    CASSERT(ctx->protocolSide == SSL_ServerSide);
    
    if ((err = SSLEncodeRSAKeyParams(&exportKey, &ctx->encryptPubKey, ctx)) != 0)
    #else
    if (ERR(err = SSLEncodeRSAKeyParams(&exportKey, &ctx->exportKey, ctx)) != 0)
    #endif
        goto fail;
    
#if RSAREF
    localKeyModulusLen = (ctx->localKey.bits + 7)/8;
#elif BSAFE
    {   A_RSA_KEY   *keyInfo;
        int         rsaResult;
        
        if ((rsaResult = B_GetKeyInfo((POINTER*)&keyInfo, ctx->localKey, KI_RSAPublic)) != 0)
            return SSLUnknownErr;
        localKeyModulusLen = keyInfo->modulus.len;
    }
#elif	_APPLE_CDSA_
	CASSERT(ctx->signingPubKey != NULL);
	localKeyModulusLen = sslKeyLengthInBytes(ctx->signingPubKey);
#else
#error No Asymmetric crypto specified 
#endif /* RSAREF / BSAFE */
    
    length = exportKey.length + 2 + localKeyModulusLen;     /* RSA ouputs a block as long as the modulus */
    
    keyExch->protocolVersion = SSL_Version_3_0;
    keyExch->contentType = SSL_handshake;
    if (ERR(err = SSLAllocBuffer(&keyExch->contents, length+4, &ctx->sysCtx)) != 0)
        goto fail;
    
    progress = keyExch->contents.data;
    *progress++ = SSL_server_key_exchange;
    progress = SSLEncodeInt(progress, length, 3);
    
    memcpy(progress, exportKey.data, exportKey.length);
    progress += exportKey.length;
    
    clientRandom.data = ctx->clientRandom;
    clientRandom.length = 32;
    serverRandom.data = ctx->serverRandom;
    serverRandom.length = 32;
    
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
#if RSAREF
    if (RSAPrivateEncrypt(progress, &outputLen, hashes, 36, &ctx->localKey) != 0)   /* Sign the structure */
        return ERR(SSLUnknownErr);
#elif BSAFE
    {   B_ALGORITHM_OBJ     rsa;
        B_ALGORITHM_METHOD  *chooser[] = { &AM_RSA_ENCRYPT, &AM_RSA_CRT_ENCRYPT, 0 };
        int                 rsaResult;
        UInt32        		encryptedOut;
        
        if ((rsaResult = B_CreateAlgorithmObject(&rsa)) != 0)
            return SSLUnknownErr;
        if ((rsaResult = B_SetAlgorithmInfo(rsa, AI_PKCS_RSAPrivate, 0)) != 0)
            return SSLUnknownErr;
        if ((rsaResult = B_EncryptInit(rsa, ctx->localKey, chooser, NO_SURR)) != 0)
            return SSLUnknownErr;
        if ((rsaResult = B_EncryptUpdate(rsa, progress,
                    &encryptedOut, localKeyModulusLen, hashes, 36, 0, NO_SURR)) != 0)
            return SSLUnknownErr;
        outputLen = encryptedOut;
        if ((rsaResult = B_EncryptFinal(rsa, progress+outputLen,
                    &encryptedOut, localKeyModulusLen-outputLen, 0, NO_SURR)) != 0)
            return SSLUnknownErr;
        outputLen += encryptedOut;
        B_DestroyAlgorithmObject(&rsa);
    }
#elif	_APPLE_CDSA_
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
#endif /* RSAREF / BSAFE */
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
    
#if RSAREF
    keyParams->data = 0;
    modulus.length = (key->bits + 7) / 8;
    modulus.data = key->modulus + MAX_RSA_MODULUS_LEN - modulus.length;
    
    exponent.length = MAX_RSA_MODULUS_LEN;
    exponent.data = key->publicExponent;            /* Point at first byte */
    
    while (*exponent.data == 0)
    {   ++exponent.data;
        --exponent.length;
    }
#elif BSAFE
    {   A_RSA_KEY   *keyInfo;
        int         rsaResult;
        
        if ((rsaResult = B_GetKeyInfo((POINTER*)&keyInfo, *key, KI_RSAPublic)) != 0)
            return SSLUnknownErr;
        modulus.data = keyInfo->modulus.data;
        modulus.length = keyInfo->modulus.len;
        exponent.data = keyInfo->exponent.data;
        exponent.length = keyInfo->exponent.len;
    }   
#elif	_APPLE_CDSA_
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
#else
#error No assymetric crypto specified
#endif /* RSAREF / BSAFE */
    
    if (ERR(err = SSLAllocBuffer(keyParams, modulus.length + exponent.length + 4, &ctx->sysCtx)) != 0)
        return err;
    progress = keyParams->data;
    progress = SSLEncodeInt(progress, modulus.length, 2);
    memcpy(progress, modulus.data, modulus.length);
    progress += modulus.length;
    progress = SSLEncodeInt(progress, exponent.length, 2);
    memcpy(progress, exponent.data, exponent.length);

#if	_APPLE_CDSA_
	/* these were mallocd by sslGetPubKeyBits() */
	SSLFreeBuffer(&modulus, &ctx->sysCtx);
	SSLFreeBuffer(&exponent, &ctx->sysCtx);
#endif
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
    
    keyExch->protocolVersion = SSL_Version_3_0;
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
        
        keyExch->protocolVersion = SSL_Version_3_0;
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
	#if	_APPLE_CDSA_
    UInt8           hash[36];
    #else
    UInt8           hash[20];
    UInt32    		outputLen;
    #endif	/* _APPLE_CDSA_ */
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
    
#if RSAREF
    {   /* Allocate room for the signed hashes; RSA can encrypt data
            as long as the modulus */
        if (ERR(err = SSLAllocBuffer(&signedHashes, (ctx->peerKey.bits + 7)/8, &ctx->sysCtx)) != 0)
            return err;

        if ((RSAPublicDecrypt(signedHashes.data, &outputLen, signature, signatureLen,
                            &ctx->peerKey)) != 0)
        {   ERR(err = SSLUnknownErr);
            goto fail;
        }
    }
#elif BSAFE
    {   B_ALGORITHM_OBJ     rsa;
        B_ALGORITHM_METHOD  *chooser[] = { &AM_MD2, &AM_MD5, &AM_RSA_DECRYPT, 0 };
        int                 rsaResult;
        unsigned int        decryptLen;
        
        /* Allocate room for the signed hashes; BSAFE makes sure we don't decode too much data */
        if (ERR(err = SSLAllocBuffer(&signedHashes, 36, &ctx->sysCtx)) != 0)
            return err; 
    
        if ((rsaResult = B_CreateAlgorithmObject(&rsa)) != 0)
            return SSLUnknownErr;
        if ((rsaResult = B_SetAlgorithmInfo(rsa, AI_PKCS_RSAPublic, 0)) != 0)
            return SSLUnknownErr;
        if ((rsaResult = B_DecryptInit(rsa, ctx->peerKey, chooser, NO_SURR)) != 0)
            return SSLUnknownErr;
        if ((rsaResult = B_DecryptUpdate(rsa, signedHashes.data, &decryptLen, 36,
                    signature, signatureLen, 0, NO_SURR)) != 0)
            return SSLUnknownErr;
        outputLen = decryptLen;
        if ((rsaResult = B_DecryptFinal(rsa, signedHashes.data+outputLen,
                    &decryptLen, 36-outputLen, 0, NO_SURR)) != 0)
            return SSLUnknownErr;
        outputLen += decryptLen;
        B_DestroyAlgorithmObject(&rsa);
    }
#elif	_APPLE_CDSA_
	
	/* not yet - calculate the hashes and then do a sig verify */
		
#else
#error No Asymmetric crypto module
#endif

	#ifndef	_APPLE_CDSA_
    if (outputLen != 36)
    {   ERR(err = SSLProtocolErr);
        goto fail;
    }
    #endif
    
    clientRandom.data = ctx->clientRandom;
    clientRandom.length = 32;
    serverRandom.data = ctx->serverRandom;
    serverRandom.length = 32;
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
        
    #if		_APPLE_CDSA_
    /* 
     * SHA hash goes right after the MD5 hash 
     */
    hashOut.data = hash + 16; 
    #else
    if ((memcmp(hash, signedHashes.data, 16)) != 0)
    {   ERR(err = SSLProtocolErr);
        goto fail;
    }
    #endif	/* _APPLE_CDSA_ */

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

	#if	_APPLE_CDSA_

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
	
	#else	/* old BSAFE/RSAREF */
	
    if ((memcmp(hash, signedHashes.data + 16, 20)) != 0)
    {   ERR(err = SSLProtocolErr);
        goto fail;
    }

	#endif
    
/* Signature matches; now replace server key with new key */
#if RSAREF
    memset(&ctx->peerKey, 0, sizeof(R_RSA_PUBLIC_KEY));
    memcpy(ctx->peerKey.modulus + (MAX_RSA_MODULUS_LEN - modulusLen),
            modulus, modulusLen);
    memcpy(ctx->peerKey.exponent + (MAX_RSA_MODULUS_LEN - exponentLen),
            exponent, exponentLen);
    
/* Adjust bit length for leading zeros in value; assume no more than 8 leading zero bits */
    {   unsigned int    bitAdjust;
        UInt8           c;
        
        c = modulus[0];
        
        bitAdjust = 8;
        while (c != 0)
        {   --bitAdjust;
            c >>= 1;
        }
        ctx->peerKey.bits = modulusLen * 8 - bitAdjust;
    }
    err = SSLNoErr;
#elif BSAFE
    {   A_RSA_KEY   pubKeyInfo;
        int         rsaErr;
        
        pubKeyInfo.modulus.data = modulus;
        pubKeyInfo.modulus.len = modulusLen;
        pubKeyInfo.exponent.data = exponent;
        pubKeyInfo.exponent.len = exponentLen;
        
        if ((rsaErr = B_CreateKeyObject(&ctx->peerKey)) != 0)
            return SSLUnknownErr;
        if ((rsaErr = B_SetKeyInfo(ctx->peerKey, KI_RSAPublic, (POINTER)&pubKeyInfo)) != 0)
            return SSLUnknownErr;
    }
    err = SSLNoErr;
#elif _APPLE_CDSA_
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
#else
#error No Assymmetric crypto module
#endif /* RSAREF / BSAFE */
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
    SSLRSAPrivateKey    *key;
    SSLProtocolVersion  version;
    Boolean				useEncryptKey = false;
    
    #if	_APPLE_CDSA_
    
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
    #else	/* original SSLRef3 */
	    if (ctx->selectedCipherSpec->keyExchangeMethod == SSL_RSA_EXPORT)
	        key = &ctx->exportKey;
	    else
	        key = &ctx->localKey;
    #endif	/* _APPLE_CDSA_ */
    result.data = 0;
    
#if RSAREF
    localKeyModulusLen = (key->bits + 7)/8;
#elif BSAFE
    {   A_RSA_KEY   *keyInfo;
        int         rsaResult;
        
        if ((rsaResult = B_GetKeyInfo((POINTER*)&keyInfo, *key, KI_RSAPublic)) != 0)
            return SSLUnknownErr;
        localKeyModulusLen = keyInfo->modulus.len;
    }
#elif	_APPLE_CDSA_
	localKeyModulusLen = sslKeyLengthInBytes(*key);
#else
#error No assymetric crypto module
#endif /* RSAREF / BSAFE */
    
    if (keyExchange.length != localKeyModulusLen) {
    	errorLog0("SSLDecodeRSAKeyExchange: length error\n");
        return ERR(SSLProtocolErr);
	}
	
#if RSAREF
    if (ERR(err = SSLAllocBuffer(&result, localKeyModulusLen, &ctx->sysCtx)) != 0)
        return err;
    if ((RSAPrivateDecrypt(result.data, &outputLen, keyExchange.data, keyExchange.length, key)) != 0)
    {   ERR(err = SSLUnknownErr);
        goto fail;
    }
#elif BSAFE
    {   B_ALGORITHM_OBJ     rsa;
        B_ALGORITHM_METHOD  *chooser[] = { &AM_RSA_DECRYPT, &AM_RSA_CRT_DECRYPT, 0 };
        int                 rsaResult;
        unsigned int        decryptLen;
        
        /* Allocate room for the premaster secret; BSAFE makes sure we don't decode too much data */
        if (ERR(err = SSLAllocBuffer(&result, 48, &ctx->sysCtx)) != 0)
            return err; 
    
        if ((rsaResult = B_CreateAlgorithmObject(&rsa)) != 0)
            return SSLUnknownErr;
        if ((rsaResult = B_SetAlgorithmInfo(rsa, AI_PKCS_RSAPrivate, 0)) != 0)
            return SSLUnknownErr;
        #ifdef	macintosh
        /* 
         * I think this is an SSLRef bug - we need to use the right key here,
         * as the RSAREF case above does!
         */
         if ((rsaResult = B_DecryptInit(rsa, *key, chooser, NO_SURR)) != 0)
            return SSLUnknownErr;
       #else	/* the SSLRef way */
        if ((rsaResult = B_DecryptInit(rsa, ctx->localKey, chooser, NO_SURR)) != 0)
            return SSLUnknownErr;
        #endif	/* mac/SSLREF */
        if ((rsaResult = B_DecryptUpdate(rsa, result.data, &decryptLen, 48,
                    keyExchange.data, keyExchange.length, 0, NO_SURR)) != 0)
            return SSLUnknownErr;
        outputLen = decryptLen;
        if ((rsaResult = B_DecryptFinal(rsa, result.data+outputLen,
                    &decryptLen, 48-outputLen, 0, NO_SURR)) != 0)
            return SSLUnknownErr;
        outputLen += decryptLen;
        B_DestroyAlgorithmObject(&rsa);
    }
#elif	_APPLE_CDSA_
	err = sslRsaDecrypt(ctx,
		*key,
		cspHand,
		keyExchange.data, 
		keyExchange.length,
		result.data,
		48,
		&outputLen);
	if(err) {
		goto fail;
	}
#endif
    
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
    if (ERR(err = SSLAllocBuffer(&ctx->preMasterSecret, 48, &ctx->sysCtx)) != 0)
        goto fail;
    memcpy(ctx->preMasterSecret.data, result.data, 48);
    
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
    #if	!_APPLE_CDSA_
    SSLRandomCtx        rsaRandom;
    int                 rsaResult;
    #endif
    
    if (ERR(err = SSLEncodeRSAPremasterSecret(ctx)) != 0)
        return err;
    
    #if	!_APPLE_CDSA_
    if (ERR(err = ReadyRandom(&rsaRandom, ctx)) != 0)
        return err;
    #endif
    
    keyExchange->contentType = SSL_handshake;
    keyExchange->protocolVersion = SSL_Version_3_0;
        
#if RSAREF
    peerKeyModulusLen = (ctx->peerKey.bits + 7)/8;
#elif BSAFE
    {   A_RSA_KEY   *keyInfo;
        
        if ((rsaResult = B_GetKeyInfo((POINTER*)&keyInfo, ctx->peerKey, KI_RSAPublic)) != 0)
            return SSLUnknownErr;
        peerKeyModulusLen = keyInfo->modulus.len;
    }
#elif	_APPLE_CDSA_
	peerKeyModulusLen = sslKeyLengthInBytes(ctx->peerPubKey);
#else
#error No Assymetric Crypto
#endif /* RSAREF / BSAFE */
    if (ERR(err = SSLAllocBuffer(&keyExchange->contents,peerKeyModulusLen + 4,&ctx->sysCtx)) != 0)
    {   
#if RSAREF
        R_RandomFinal(&rsaRandom);
#elif BSAFE
        B_DestroyAlgorithmObject(&rsaRandom);
#endif
        return err;
    }
    keyExchange->contents.data[0] = SSL_client_key_exchange;
    SSLEncodeInt(keyExchange->contents.data + 1, peerKeyModulusLen, 3);
#if RSAREF
    if ((rsaResult = RSAPublicEncrypt(keyExchange->contents.data+4, &outputLen,
                                ctx->preMasterSecret.data, 48,
                                &ctx->peerKey,&rsaRandom)) != 0)
    {   R_RandomFinal(&rsaRandom);
        return ERR(SSLUnknownErr);
    }
    
    R_RandomFinal(&rsaRandom);

#elif BSAFE
    {   B_ALGORITHM_OBJ     rsa;
        B_ALGORITHM_METHOD  *chooser[] = { &AM_RSA_ENCRYPT, 0 };
        int                 rsaResult;
        unsigned int        encryptedOut;
        
        if ((rsaResult = B_CreateAlgorithmObject(&rsa)) != 0)
            return SSLUnknownErr;
        if ((rsaResult = B_SetAlgorithmInfo(rsa, AI_PKCS_RSAPublic, 0)) != 0)
            return SSLUnknownErr;
        if ((rsaResult = B_EncryptInit(rsa, ctx->peerKey, chooser, NO_SURR)) != 0)
            return SSLUnknownErr;
        if ((rsaResult = B_EncryptUpdate(rsa, keyExchange->contents.data+4,
                    &encryptedOut, peerKeyModulusLen, ctx->preMasterSecret.data, 48, rsaRandom, NO_SURR)) != 0)
            return SSLUnknownErr;
        outputLen = encryptedOut;
        if ((rsaResult = B_EncryptFinal(rsa, keyExchange->contents.data+4+outputLen,
                    &encryptedOut, peerKeyModulusLen-outputLen, rsaRandom, NO_SURR)) != 0)
            return SSLUnknownErr;
        outputLen += encryptedOut;
        B_DestroyAlgorithmObject(&rsa);
    }
    
    B_DestroyAlgorithmObject(&rsaRandom);
#elif _APPLE_CDSA_
	err = sslRsaEncrypt(ctx,
		ctx->peerPubKey,
		/* FIXME - maybe this should be ctx->cspHand */
		ctx->peerPubKeyCsp,
		ctx->preMasterSecret.data, 
		48,
		keyExchange->contents.data+4,
		peerKeyModulusLen,
		&outputLen);
	if(err) {
		return err;
	}
#endif  
    
    CASSERT(outputLen + 4 == keyExchange->contents.length);
    
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
    keyExchange->protocolVersion = SSL_Version_3_0;
    
    if (ERR(err = SSLAllocBuffer(&keyExchange->contents,outputLen + 4,&ctx->sysCtx)) != 0)
        return err;
    
    keyExchange->contents.data[0] = SSL_client_key_exchange;
    SSLEncodeInt(keyExchange->contents.data+1, ctx->dhExchangePublic.length+2, 3);
    
    SSLEncodeInt(keyExchange->contents.data+4, ctx->dhExchangePublic.length, 2);
    memcpy(keyExchange->contents.data+6, ctx->dhExchangePublic.data, ctx->dhExchangePublic.length);
    
    return SSLNoErr;
}
#endif


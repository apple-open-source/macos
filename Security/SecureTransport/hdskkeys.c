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
	File:		hdskkeys.c

	Contains:	Key calculation and encoding

	Written by:	Doug Mitchell, based on Netscape SSLRef 3.0

	Copyright: (c) 1999 by Apple Computer, Inc., all rights reserved.

*/
/*  *********************************************************************
    File: hdskkeys.c

    SSLRef 3.0 Final -- 11/19/96

    Copyright (c)1996 by Netscape Communications Corp.

    By retrieving this software you are bound by the licensing terms
    disclosed in the file "LICENSE.txt". Please read it, and if you don't
    accept the terms, delete this software.

    SSLRef 3.0 was developed by Netscape Communications Corp. of Mountain
    View, California <http://home.netscape.com/> and Consensus Development
    Corporation of Berkeley, California <http://www.consensus.com/>.

    *********************************************************************

    File: hdskkeys.c   Key calculation and encoding

    Contains code for encoding premaster secrets, generating master
    secrets from premaster secrets & key data generation from master
    secrets and following initialization of ciphers.

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

static SSLErr SSLGenerateKeyMaterial(SSLBuffer key, SSLContext *ctx);

SSLErr
SSLEncodeRSAPremasterSecret(SSLContext *ctx)
{   SSLBuffer           randData;
    SSLErr              err;
    
    if (ERR(err = SSLAllocBuffer(&ctx->preMasterSecret, 48, &ctx->sysCtx)) != 0)
        return err;
    
    SSLEncodeInt(ctx->preMasterSecret.data, SSL_Version_3_0, 2);
    randData.data = ctx->preMasterSecret.data+2;
    randData.length = 46;
    #ifdef	_APPLE_CDSA_
    if ((err = sslRand(ctx, &randData)) != 0)
    #else
    if ((err = ctx->sysCtx.random(randData, ctx->sysCtx.randomRef)) != 0)
    #endif
        return err;
    
    DUMP_BUFFER_NAME("premaster secret", ctx->preMasterSecret);
    
    return SSLNoErr;
}

#if	APPLE_DH

SSLErr
SSLEncodeDHPremasterSecret(SSLContext *ctx)
{   
	#if		!APPLE_DH
	return SSLUnsupportedErr;
	#else
	
	SSLErr              err;
    int                 rsaResult;
    SSLRandomCtx        rsaRandom;
#if RSAREF
    SSLBuffer           privateValue;
#endif

/* Given the server's Diffie-Hellman parameters, prepare a public & private value,
 *  then use the public value provided by the server and our private value to
 *  generate a shared key (the premaster secret). Save our public value in
 *  ctx->dhExchangePublic to send to the server so it can calculate the matching
 *  key on its end
 */
    if (ERR(err = ReadyRandom(&rsaRandom, ctx)) != 0)
        return err;
    
#if RSAREF
    {   privateValue.data = 0;
        
        if (ERR(err = SSLAllocBuffer(&ctx->dhExchangePublic, ctx->peerDHParams.primeLen, &ctx->sysCtx)) != 0)
            goto fail;
        if (ERR(err = SSLAllocBuffer(&privateValue, ctx->dhExchangePublic.length - 16, &ctx->sysCtx)) != 0)
            goto fail;
        
        if ((rsaResult = R_SetupDHAgreement(ctx->dhExchangePublic.data, privateValue.data,
                            privateValue.length, &ctx->peerDHParams, &rsaRandom)) != 0)
        {   err = SSLUnknownErr;
            goto fail;
        }
        
        if (ERR(err = SSLAllocBuffer(&ctx->preMasterSecret, ctx->peerDHParams.primeLen, &ctx->sysCtx)) != 0)
            goto fail;
        
        if ((rsaResult = R_ComputeDHAgreedKey (ctx->preMasterSecret.data, ctx->dhPeerPublic.data,
                            privateValue.data, privateValue.length,  &ctx->peerDHParams)) != 0)
        {   err = SSLUnknownErr;
            goto fail;
        }
    }
#elif BSAFE
    {   unsigned int    outputLen;
        
        if (ERR(err = SSLAllocBuffer(&ctx->dhExchangePublic, 128, &ctx->sysCtx)) != 0)
            goto fail;
        if ((rsaResult = B_KeyAgreePhase1(ctx->peerDHParams, ctx->dhExchangePublic.data,
                            &outputLen, 128, rsaRandom, NO_SURR)) != 0)
        {   err = SSLUnknownErr;
            goto fail;
        }
        ctx->dhExchangePublic.length = outputLen;
        if (ERR(err = SSLAllocBuffer(&ctx->preMasterSecret, 128, &ctx->sysCtx)) != 0)
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
    
    DUMP_BUFFER_NAME("premaster secret", ctx->preMasterSecret);
    
    err = SSLNoErr;
fail:
#if RSAREF
    ERR(SSLFreeBuffer(&privateValue, &ctx->sysCtx));
    R_RandomFinal(&rsaRandom);
#elif BSAFE
    B_DestroyAlgorithmObject(&rsaRandom);
#endif  
    return err;
    #endif
}

#endif	/* APPLE_DH */

SSLErr
SSLCalculateMasterSecret(SSLContext *ctx)
{   SSLErr      err;
    SSLBuffer   shaState, md5State, clientRandom,
                serverRandom, shaHash, md5Hash, leader;
    UInt8       *masterProgress, shaHashData[20], leaderData[3];
    int         i;
    
    md5State.data = shaState.data = 0;
    if ((err = SSLAllocBuffer(&md5State, SSLHashMD5.contextSize, &ctx->sysCtx)) != 0)
        goto fail;
    if ((err = SSLAllocBuffer(&shaState, SSLHashSHA1.contextSize, &ctx->sysCtx)) != 0)
        goto fail;
    
    clientRandom.data = ctx->clientRandom;
    clientRandom.length = 32;
    serverRandom.data = ctx->serverRandom;
    serverRandom.length = 32;
    shaHash.data = shaHashData;
    shaHash.length = 20;
    
    masterProgress = ctx->masterSecret;
    
    for (i = 1; i <= 3; i++)
    {   if ((err = SSLHashMD5.init(md5State)) != 0)
            goto fail;
        if ((err = SSLHashSHA1.init(shaState)) != 0)
            goto fail;
        
        leaderData[0] = leaderData[1] = leaderData[2] = 0x40 + i;   /* 'A', 'B', etc. */
        leader.data = leaderData;
        leader.length = i;
        
        if ((err = SSLHashSHA1.update(shaState, leader)) != 0)
            goto fail;
        if ((err = SSLHashSHA1.update(shaState, ctx->preMasterSecret)) != 0)
            goto fail;
        if ((err = SSLHashSHA1.update(shaState, clientRandom)) != 0)
            goto fail;
        if ((err = SSLHashSHA1.update(shaState, serverRandom)) != 0)
            goto fail;
        if ((err = SSLHashSHA1.final(shaState, shaHash)) != 0)
            goto fail;
        if ((err = SSLHashMD5.update(md5State, ctx->preMasterSecret)) != 0)
            goto fail;
        if ((err = SSLHashMD5.update(md5State, shaHash)) != 0)
            goto fail;
        md5Hash.data = masterProgress;
        md5Hash.length = 16;
        if ((err = SSLHashMD5.final(md5State, md5Hash)) != 0)
            goto fail;
        masterProgress += 16;
    }
    
    DUMP_DATA_NAME("master secret",ctx->masterSecret, 48);
    
    err = SSLNoErr;
fail:
    SSLFreeBuffer(&shaState, &ctx->sysCtx);
    SSLFreeBuffer(&md5State, &ctx->sysCtx);
    return err;
}

SSLErr
SSLInitPendingCiphers(SSLContext *ctx)
{   SSLErr          err;
    SSLBuffer       key, hashCtx;
    UInt8           *keyDataProgress, *keyPtr, *ivPtr;
    int             keyDataLen;
    CipherContext   *serverPending, *clientPending;
        
    key.data = hashCtx.data = 0;
    
    ctx->readPending.hash = ctx->selectedCipherSpec->macAlgorithm;
    ctx->writePending.hash = ctx->selectedCipherSpec->macAlgorithm;
    ctx->readPending.symCipher = ctx->selectedCipherSpec->cipher;
    ctx->writePending.symCipher = ctx->selectedCipherSpec->cipher;
    ctx->readPending.sequenceNum.high = ctx->readPending.sequenceNum.low = 0;
    ctx->writePending.sequenceNum.high = ctx->writePending.sequenceNum.low = 0;
    
    keyDataLen = ctx->selectedCipherSpec->macAlgorithm->digestSize +
                     ctx->selectedCipherSpec->cipher->secretKeySize;
    if (ctx->selectedCipherSpec->isExportable == NotExportable)
        keyDataLen += ctx->selectedCipherSpec->cipher->ivSize;
    keyDataLen *= 2;        /* two of everything */
    
    if ((err = SSLAllocBuffer(&key, keyDataLen, &ctx->sysCtx)) != 0)
        return err;
    if ((err = SSLGenerateKeyMaterial(key, ctx)) != 0)
        goto fail;
    DUMP_BUFFER_NAME("key data",key);
    
    if (ctx->protocolSide == SSL_ServerSide)
    {   serverPending = &ctx->writePending;
        clientPending = &ctx->readPending;
    }
    else
    {   serverPending = &ctx->readPending;
        clientPending = &ctx->writePending;
    }
    
    keyDataProgress = key.data;
    memcpy(clientPending->macSecret, keyDataProgress, ctx->selectedCipherSpec->macAlgorithm->digestSize);
    DUMP_DATA_NAME("client write mac secret", keyDataProgress, ctx->selectedCipherSpec->macAlgorithm->digestSize);
    keyDataProgress += ctx->selectedCipherSpec->macAlgorithm->digestSize;
    memcpy(serverPending->macSecret, keyDataProgress, ctx->selectedCipherSpec->macAlgorithm->digestSize);
    DUMP_DATA_NAME("server write mac secret", keyDataProgress, ctx->selectedCipherSpec->macAlgorithm->digestSize);
    keyDataProgress += ctx->selectedCipherSpec->macAlgorithm->digestSize;
    
    if (ctx->selectedCipherSpec->isExportable == NotExportable)
    {   keyPtr = keyDataProgress;
        keyDataProgress += ctx->selectedCipherSpec->cipher->secretKeySize;
        /* Skip server write key to get to IV */
        ivPtr = keyDataProgress + ctx->selectedCipherSpec->cipher->secretKeySize;
        /* APPLE_CDSA changes to all symmetric cipher routines.....*/
        if ((err = ctx->selectedCipherSpec->cipher->initialize(keyPtr, ivPtr,
                                    clientPending, ctx)) != 0)
            goto fail;
        DUMP_DATA_NAME("client write key", keyPtr, ctx->selectedCipherSpec->cipher->secretKeySize);
        DUMP_DATA_NAME("client write iv", ivPtr, ctx->selectedCipherSpec->cipher->ivSize);
        keyPtr = keyDataProgress;
        keyDataProgress += ctx->selectedCipherSpec->cipher->secretKeySize;
        /* Skip client write IV to get to server write IV */
        ivPtr = keyDataProgress + ctx->selectedCipherSpec->cipher->ivSize;
        if ((err = ctx->selectedCipherSpec->cipher->initialize(keyPtr, ivPtr,
                                    serverPending, ctx)) != 0)
            goto fail;
        DUMP_DATA_NAME("server write key", keyPtr, ctx->selectedCipherSpec->cipher->secretKeySize);
        DUMP_DATA_NAME("server write iv", ivPtr, ctx->selectedCipherSpec->cipher->ivSize);
    }
    else
    {   UInt8           exportKey[16], exportIV[16];
        SSLBuffer       hashOutput, clientWrite, serverWrite, clientRandom,
                        serverRandom;
        
        CASSERT(ctx->selectedCipherSpec->cipher->keySize <= 16);
        CASSERT(ctx->selectedCipherSpec->cipher->ivSize <= 16);
        
        clientWrite.data = keyDataProgress;
        clientWrite.length = ctx->selectedCipherSpec->cipher->secretKeySize;
        serverWrite.data = keyDataProgress + clientWrite.length;
        serverWrite.length = ctx->selectedCipherSpec->cipher->secretKeySize;
        clientRandom.data = ctx->clientRandom;
        clientRandom.length = 32;
        serverRandom.data = ctx->serverRandom;
        serverRandom.length = 32;
        
        if ((err = SSLAllocBuffer(&hashCtx, SSLHashMD5.contextSize, &ctx->sysCtx)) != 0)
            goto fail;
        if ((err = SSLHashMD5.init(hashCtx)) != 0)
            goto fail;
        if ((err = SSLHashMD5.update(hashCtx, clientWrite)) != 0)
            goto fail;
        if ((err = SSLHashMD5.update(hashCtx, clientRandom)) != 0)
            goto fail;
        if ((err = SSLHashMD5.update(hashCtx, serverRandom)) != 0)
            goto fail;
        hashOutput.data = exportKey;
        hashOutput.length = 16;
        if ((err = SSLHashMD5.final(hashCtx, hashOutput)) != 0)
            goto fail;
        
        if (ctx->selectedCipherSpec->cipher->ivSize > 0)
        {   if ((err = SSLHashMD5.init(hashCtx)) != 0)
                goto fail;
            if ((err = SSLHashMD5.update(hashCtx, clientRandom)) != 0)
                goto fail;
            if ((err = SSLHashMD5.update(hashCtx, serverRandom)) != 0)
                goto fail;
            hashOutput.data = exportIV;
            hashOutput.length = 16;
            if ((err = SSLHashMD5.final(hashCtx, hashOutput)) != 0)
                goto fail;
        }
        if ((err = ctx->selectedCipherSpec->cipher->initialize(exportKey, exportIV,
                                    clientPending, ctx)) != 0)
            goto fail;
        
        if ((err = SSLHashMD5.init(hashCtx)) != 0)
            goto fail;
        if ((err = SSLHashMD5.update(hashCtx, serverWrite)) != 0)
            goto fail;
        if ((err = SSLHashMD5.update(hashCtx, serverRandom)) != 0)
            goto fail;
        if ((err = SSLHashMD5.update(hashCtx, clientRandom)) != 0)
            goto fail;
        hashOutput.data = exportKey;
        hashOutput.length = 16;
        if ((err = SSLHashMD5.final(hashCtx, hashOutput)) != 0)
            goto fail;
        
        if (ctx->selectedCipherSpec->cipher->ivSize > 0)
        {   if ((err = SSLHashMD5.init(hashCtx)) != 0)
                goto fail;
            if ((err = SSLHashMD5.update(hashCtx, serverRandom)) != 0)
                goto fail;
            if ((err = SSLHashMD5.update(hashCtx, clientRandom)) != 0)
                goto fail;
            hashOutput.data = exportIV;
            hashOutput.length = 16;
            if ((err = SSLHashMD5.final(hashCtx, hashOutput)) != 0)
                goto fail;
        }
        if ((err = ctx->selectedCipherSpec->cipher->initialize(exportKey, exportIV,
                                    serverPending, ctx)) != 0)
            goto fail;
    }
    
/* Ciphers are ready for use */
    ctx->writePending.ready = 1;
    ctx->readPending.ready = 1;
    
/* Ciphers get swapped by sending or receiving a change cipher spec message */
    
    err = SSLNoErr;
fail:
    SSLFreeBuffer(&key, &ctx->sysCtx);
    SSLFreeBuffer(&hashCtx, &ctx->sysCtx);
    return err;
}

static SSLErr
SSLGenerateKeyMaterial(SSLBuffer key, SSLContext *ctx)
{   SSLErr      err;
    UInt8       leaderData[10];     /* Max of 10 hashes (* 16 bytes/hash = 160 bytes of key) */
    UInt8       shaHashData[20], md5HashData[16];
    SSLBuffer   shaContext, md5Context;
    UInt8       *keyProgress;
    int         i,j,remaining, satisfied;
    SSLBuffer   leader, masterSecret, serverRandom, clientRandom, shaHash, md5Hash;
    
    CASSERT(key.length <= 16 * sizeof(leaderData));
    
    leader.data = leaderData;
    masterSecret.data = ctx->masterSecret;
    masterSecret.length = 48;
    serverRandom.data = ctx->serverRandom;
    serverRandom.length = 32;
    clientRandom.data = ctx->clientRandom;
    clientRandom.length = 32;
    shaHash.data = shaHashData;
    shaHash.length = 20;
    md5Hash.data = md5HashData;
    md5Hash.length = 20;
    
    md5Context.data = 0;
    shaContext.data = 0;
    if ((err = ReadyHash(&SSLHashMD5, &md5Context, ctx)) != 0)
        goto fail;
    if ((err = ReadyHash(&SSLHashSHA1, &shaContext, ctx)) != 0)
        goto fail;  
    
    keyProgress = key.data;
    remaining = key.length;
    
    for (i = 0; remaining > 0; ++i)
    {   for (j = 0; j <= i; j++)
            leaderData[j] = 0x41 + i;   /* 'A', 'BB', 'CCC', etc. */
        leader.length = i+1;
        
        if ((err = SSLHashSHA1.update(shaContext, leader)) != 0)
            goto fail;
        if ((err = SSLHashSHA1.update(shaContext, masterSecret)) != 0)
            goto fail;
        if ((err = SSLHashSHA1.update(shaContext, serverRandom)) != 0)
            goto fail;
        if ((err = SSLHashSHA1.update(shaContext, clientRandom)) != 0)
            goto fail;
        if ((err = SSLHashSHA1.final(shaContext, shaHash)) != 0)
            goto fail;
        if ((err = SSLHashMD5.update(md5Context, masterSecret)) != 0)
            goto fail;
        if ((err = SSLHashMD5.update(md5Context, shaHash)) != 0)
            goto fail;
        if ((err = SSLHashMD5.final(md5Context, md5Hash)) != 0)
            goto fail;
        
        satisfied = 16;
        if (remaining < 16)
            satisfied = remaining;
        memcpy(keyProgress, md5HashData, satisfied);
        remaining -= satisfied;
        keyProgress += satisfied;
        
        if ((err = SSLHashMD5.init(md5Context)) != 0)
            goto fail;
        if ((err = SSLHashSHA1.init(shaContext)) != 0)
            goto fail;
    }
    
    CASSERT(remaining == 0 && keyProgress == (key.data + key.length));
    err = SSLNoErr;
fail:
    SSLFreeBuffer(&md5Context, &ctx->sysCtx);
    SSLFreeBuffer(&shaContext, &ctx->sysCtx);
    
    return err;
}

#ifndef	_APPLE_CDSA_
/* I'm not sure what this is for */
SSLErr
ReadyRandom(SSLRandomCtx *rsaRandom, SSLContext *ctx)
{   SSLErr              err;
    SSLBuffer           randomSeedBuf;
    UInt8               randomSeed[32];
    int                 rsaResult;
#if RSAREF
    unsigned int        bytesNeeded;
    
    if (R_RandomInit(rsaRandom) != 0)
        return ERR(SSLUnknownErr);
    if (R_GetRandomBytesNeeded(&bytesNeeded, rsaRandom) != 0)
        return ERR(SSLUnknownErr);
    
    randomSeedBuf.data = randomSeed;
    randomSeedBuf.length = 32;
    
    while (bytesNeeded > 0)
    {   if (ERR(err = ctx->sysCtx.random(randomSeedBuf, ctx->sysCtx.randomRef)) != 0)
            return err;
        if ((rsaResult = R_RandomUpdate(rsaRandom, randomSeed, 32)) != 0)
            return ERR(SSLUnknownErr);
        
        if (bytesNeeded >= 32)
            bytesNeeded -= 32;
        else
            bytesNeeded = 0;
    }
#elif BSAFE
    static B_ALGORITHM_OBJ  random;
    B_ALGORITHM_METHOD      *chooser[] = { &AM_MD5_RANDOM, 0 };
    
    if ((rsaResult = B_CreateAlgorithmObject(rsaRandom)) != 0)
        return ERR(SSLUnknownErr);
    if ((rsaResult = B_SetAlgorithmInfo(*rsaRandom, AI_MD5Random, 0)) != 0)
        return ERR(SSLUnknownErr);
    if ((rsaResult = B_RandomInit(*rsaRandom, chooser, NO_SURR)) != 0)
        return ERR(SSLUnknownErr);
    randomSeedBuf.data = randomSeed;
    randomSeedBuf.length = 32;
    if (ERR(err = ctx->sysCtx.random(randomSeedBuf, ctx->sysCtx.randomRef)) != 0)
        return err;
    if ((rsaResult = B_RandomUpdate(*rsaRandom, randomSeedBuf.data, randomSeedBuf.length, NO_SURR)) != 0)
        return ERR(SSLUnknownErr);
#endif /* RSAREF / BSAFE */
        
    return SSLNoErr;
}
#endif	/* APPLE_CDSA */

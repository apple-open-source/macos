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
#include <assert.h>

SSLErr
SSLEncodeRSAPremasterSecret(SSLContext *ctx)
{   SSLBuffer           randData;
    SSLErr              err;
    
    if (ERR(err = SSLAllocBuffer(&ctx->preMasterSecret, 
			SSL_RSA_PREMASTER_SECRET_SIZE, &ctx->sysCtx)) != 0)
        return err;
    
	assert((ctx->negProtocolVersion == SSL_Version_3_0) ||
		   (ctx->negProtocolVersion == TLS_Version_1_0));
    SSLEncodeInt(ctx->preMasterSecret.data, ctx->maxProtocolVersion, 2);
    randData.data = ctx->preMasterSecret.data+2;
    randData.length = SSL_RSA_PREMASTER_SECRET_SIZE - 2;
    if ((err = sslRand(ctx, &randData)) != 0)
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
SSLInitPendingCiphers(SSLContext *ctx)
{   SSLErr          err;
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
    
    if ((err = SSLAllocBuffer(&key, keyDataLen, &ctx->sysCtx)) != 0)
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
        /* APPLE_CDSA changes to all symmetric cipher routines.....*/
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
    
    err = SSLNoErr;
fail:
    SSLFreeBuffer(&key, &ctx->sysCtx);
    return err;
}


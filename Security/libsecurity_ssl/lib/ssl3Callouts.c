/*
 * Copyright (c) 2002,2005-2007,2010-2012 Apple Inc. All Rights Reserved.
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
 * ssl3Callouts.c - SSLv3-specific routines for SslTlsCallouts.
 */

#include "sslMemory.h"
#include "tls_ssl.h"
#include "sslUtils.h"
#include "sslDigests.h"
#include "sslDebug.h"
#include "sslAlertMessage.h"

#include <assert.h>
#include <strings.h>
#include <stddef.h>

#define LOG_GEN_KEY 	0

/*
 * On input, the following are valid:
 *		MasterSecret[48]
 *		ClientHello.random[32]
 *      ServerHello.random[32]
 *
 *      key_block =
 *      	 MD5(master_secret + SHA(`A' + master_secret +
 *                              ServerHello.random +
 *                              ClientHello.random)) +
 *      	MD5(master_secret + SHA(`BB' + master_secret +
 *                              ServerHello.random +
 *                              ClientHello.random)) +
 *      	MD5(master_secret + SHA(`CCC' + master_secret +
 *                              ServerHello.random +
 *                              ClientHello.random)) + [...];
 */
static OSStatus ssl3GenerateKeyMaterial (
	SSLBuffer key, 					// caller mallocs and specifies length of
									//   required key material here
	SSLContext *ctx)
{
	OSStatus    err;
    UInt8       leaderData[10];     /* Max of 10 hashes
									 * (* 16 bytes/hash = 160 bytes of key) */
    UInt8       shaHashData[20], md5HashData[16];
    SSLBuffer   shaContext, md5Context;
    UInt8       *keyProgress;
    size_t      i,j,remaining, satisfied;
    SSLBuffer   leader, masterSecret, serverRandom, clientRandom, shaHash, md5Hash;

 	#if	LOG_GEN_KEY
	printf("GenerateKey: master ");
	for(i=0; i<SSL_MASTER_SECRET_SIZE; i++) {
		printf("%02X ", ctx->masterSecret[i]);
	}
	printf("\n");
	#endif

    assert(key.length <= 16 * sizeof(leaderData));

    leader.data = leaderData;
    masterSecret.data = ctx->masterSecret;
    masterSecret.length = SSL_MASTER_SECRET_SIZE;
    serverRandom.data = ctx->serverRandom;
    serverRandom.length = SSL_CLIENT_SRVR_RAND_SIZE;
    clientRandom.data = ctx->clientRandom;
    clientRandom.length = SSL_CLIENT_SRVR_RAND_SIZE;
    shaHash.data = shaHashData;
    shaHash.length = 20;
    md5Hash.data = md5HashData;
    md5Hash.length = 16;

    md5Context.data = 0;
    shaContext.data = 0;
    if ((err = ReadyHash(&SSLHashMD5, &md5Context)) != 0)
        goto fail;
    if ((err = ReadyHash(&SSLHashSHA1, &shaContext)) != 0)
        goto fail;  
    
    keyProgress = key.data;
    remaining = key.length;

    for (i = 0; remaining > 0; ++i)
    {   for (j = 0; j <= i; j++)
            leaderData[j] = 0x41 + i;   /* 'A', 'BB', 'CCC', etc. */
        leader.length = i+1;

        if ((err = SSLHashSHA1.update(&shaContext, &leader)) != 0)
            goto fail;
        if ((err = SSLHashSHA1.update(&shaContext, &masterSecret)) != 0)
            goto fail;
        if ((err = SSLHashSHA1.update(&shaContext, &serverRandom)) != 0)
            goto fail;
        if ((err = SSLHashSHA1.update(&shaContext, &clientRandom)) != 0)
            goto fail;
        if ((err = SSLHashSHA1.final(&shaContext, &shaHash)) != 0)
            goto fail;
        if ((err = SSLHashMD5.update(&md5Context, &masterSecret)) != 0)
            goto fail;
        if ((err = SSLHashMD5.update(&md5Context, &shaHash)) != 0)
            goto fail;
        if ((err = SSLHashMD5.final(&md5Context, &md5Hash)) != 0)
            goto fail;

        satisfied = 16;
        if (remaining < 16)
            satisfied = remaining;
        memcpy(keyProgress, md5HashData, satisfied);
        remaining -= satisfied;
        keyProgress += satisfied;

		if(remaining > 0) {
			/* at top of loop, this was done in ReadyHash() */
			if ((err = SSLHashMD5.init(&md5Context)) != 0)
				goto fail;
			if ((err = SSLHashSHA1.init(&shaContext)) != 0)
				goto fail;
		}
    }

    assert(remaining == 0 && keyProgress == (key.data + key.length));
    err = errSecSuccess;
fail:
    SSLFreeBuffer(&md5Context);
    SSLFreeBuffer(&shaContext);
    
 	#if	LOG_GEN_KEY
	printf("GenerateKey: DONE\n");
	#endif
    return err;
}

/*
 * On entry: clientRandom, serverRandom, preMasterSecret valid
 * On return: masterSecret valid
 */
static OSStatus ssl3GenerateMasterSecret (
	SSLContext *ctx)
{
	OSStatus    err;
    SSLBuffer   shaState, md5State, clientRandom,
                serverRandom, shaHash, md5Hash, leader;
    UInt8       *masterProgress, shaHashData[20], leaderData[3];
    int         i;

    md5State.data = shaState.data = 0;
    if ((err = SSLAllocBuffer(&md5State, SSLHashMD5.contextSize)))
        goto fail;
    if ((err = SSLAllocBuffer(&shaState, SSLHashSHA1.contextSize)))
        goto fail;

    clientRandom.data = ctx->clientRandom;
    clientRandom.length = SSL_CLIENT_SRVR_RAND_SIZE;
    serverRandom.data = ctx->serverRandom;
    serverRandom.length = SSL_CLIENT_SRVR_RAND_SIZE;
    shaHash.data = shaHashData;
    shaHash.length = 20;

    masterProgress = ctx->masterSecret;

    for (i = 1; i <= 3; i++)
    {   if ((err = SSLHashMD5.init(&md5State)) != 0)
            goto fail;
        if ((err = SSLHashSHA1.init(&shaState)) != 0)
            goto fail;

        leaderData[0] = leaderData[1] = leaderData[2] = 0x40 + i;   /* 'A', 'B', etc. */
        leader.data = leaderData;
        leader.length = i;

        if ((err = SSLHashSHA1.update(&shaState, &leader)) != 0)
            goto fail;
        if ((err = SSLHashSHA1.update(&shaState, &ctx->preMasterSecret)) != 0)
            goto fail;
        if ((err = SSLHashSHA1.update(&shaState, &clientRandom)) != 0)
            goto fail;
        if ((err = SSLHashSHA1.update(&shaState, &serverRandom)) != 0)
            goto fail;
        if ((err = SSLHashSHA1.final(&shaState, &shaHash)) != 0)
            goto fail;
        if ((err = SSLHashMD5.update(&md5State, &ctx->preMasterSecret)) != 0)
            goto fail;
        if ((err = SSLHashMD5.update(&md5State, &shaHash)) != 0)
            goto fail;
        md5Hash.data = masterProgress;
        md5Hash.length = 16;
        if ((err = SSLHashMD5.final(&md5State, &md5Hash)) != 0)
            goto fail;
        masterProgress += 16;
    }

    err = errSecSuccess;
fail:
    SSLFreeBuffer(&shaState);
    SSLFreeBuffer(&md5State);
    return err;
}

/* common routine to compute a Mac for finished message and cert verify message */
static OSStatus
ssl3CalculateFinishedMessage(
	SSLContext *ctx,
	SSLBuffer finished, 		// mallocd by caller
	SSLBuffer shaMsgState,		// running total
	SSLBuffer md5MsgState, 		// ditto
	UInt32 senderID) 			// optional, nonzero for finished message
{
	OSStatus        err;
    SSLBuffer       hash, input;
    UInt8           sender[4], md5Inner[16], shaInner[20];

    // assert(finished.length == 36);

    if (senderID != 0) {
		SSLEncodeInt(sender, senderID, 4);
        input.data = sender;
        input.length = 4;
        if ((err = SSLHashMD5.update(&md5MsgState, &input)) != 0)
            goto fail;
        if ((err = SSLHashSHA1.update(&shaMsgState, &input)) != 0)
            goto fail;
    }
    input.data = ctx->masterSecret;
    input.length = SSL_MASTER_SECRET_SIZE;
    if ((err = SSLHashMD5.update(&md5MsgState, &input)) != 0)
        goto fail;
    if ((err = SSLHashSHA1.update(&shaMsgState, &input)) != 0)
        goto fail;
    input.data = (UInt8 *)SSLMACPad1;
    input.length = SSLHashMD5.macPadSize;
    if ((err = SSLHashMD5.update(&md5MsgState, &input)) != 0)
        goto fail;
    input.length = SSLHashSHA1.macPadSize;
    if ((err = SSLHashSHA1.update(&shaMsgState, &input)) != 0)
        goto fail;
    hash.data = md5Inner;
    hash.length = 16;
    if ((err = SSLHashMD5.final(&md5MsgState, &hash)) != 0)
        goto fail;
    hash.data = shaInner;
    hash.length = 20;
    if ((err = SSLHashSHA1.final(&shaMsgState, &hash)) != 0)
        goto fail;
    if ((err = SSLHashMD5.init(&md5MsgState)) != 0)
        goto fail;
    if ((err = SSLHashSHA1.init(&shaMsgState)) != 0)
        goto fail;
    input.data = ctx->masterSecret;
    input.length = SSL_MASTER_SECRET_SIZE;
    if ((err = SSLHashMD5.update(&md5MsgState, &input)) != 0)
        goto fail;
    if ((err = SSLHashSHA1.update(&shaMsgState, &input)) != 0)
        goto fail;
    input.data = (UInt8 *)SSLMACPad2;
    input.length = SSLHashMD5.macPadSize;
    if ((err = SSLHashMD5.update(&md5MsgState, &input)) != 0)
        goto fail;
    input.length = SSLHashSHA1.macPadSize;
    if ((err = SSLHashSHA1.update(&shaMsgState, &input)) != 0)
        goto fail;
    input.data = md5Inner;
    input.length = 16;
    if ((err = SSLHashMD5.update(&md5MsgState, &input)) != 0)
        goto fail;
    hash.data = finished.data;
    hash.length = 16;
    if ((err = SSLHashMD5.final(&md5MsgState, &hash)) != 0)
        goto fail;
    input.data = shaInner;
    input.length = 20;
    if ((err = SSLHashSHA1.update(&shaMsgState, &input)) != 0)
        goto fail;
    hash.data = finished.data + 16;
    hash.length = 20;
    if ((err = SSLHashSHA1.final(&shaMsgState, &hash)) != 0)
        goto fail;

fail:
    return err;
}


static OSStatus ssl3ComputeFinishedMac (
	SSLContext *ctx,
	SSLBuffer finished, 		// output - mallocd by caller
	Boolean isServer)			// refers to message, not us
{
	OSStatus serr;
    SSLBuffer shaMsgState, md5MsgState;

    shaMsgState.data = 0;
    md5MsgState.data = 0;
    if ((serr = CloneHashState(&SSLHashSHA1, &ctx->shaState, &shaMsgState)) != 0)
        goto fail;
    if ((serr = CloneHashState(&SSLHashMD5, &ctx->md5State, &md5MsgState)) != 0)
        goto fail;

	serr = ssl3CalculateFinishedMessage(ctx, finished, shaMsgState, md5MsgState,
		isServer ? SSL_Finished_Sender_Server : SSL_Finished_Sender_Client);

fail:
    SSLFreeBuffer(&shaMsgState);
    SSLFreeBuffer(&md5MsgState);

    return serr;
}

/* TODO: Factor this and ssl3ComputeFinishedMac to share more common code. */
static OSStatus ssl3ComputeCertVfyMac (
	SSLContext *ctx,
	SSLBuffer *finished, 		// output - mallocd by caller
    SSL_HashAlgorithm hash)     //unused in this one
{
	OSStatus serr;
    SSLBuffer shaMsgState, md5MsgState;

    shaMsgState.data = 0;
    md5MsgState.data = 0;
    if ((serr = CloneHashState(&SSLHashSHA1, &ctx->shaState, &shaMsgState)) != 0)
        goto fail;
    if ((serr = CloneHashState(&SSLHashMD5, &ctx->md5State, &md5MsgState)) != 0)
        goto fail;

    assert(finished->length >= SSL_MD5_DIGEST_LEN + SSL_SHA1_DIGEST_LEN);
    finished->length = SSL_MD5_DIGEST_LEN + SSL_SHA1_DIGEST_LEN;

	serr = ssl3CalculateFinishedMessage(ctx, *finished, shaMsgState, md5MsgState, 0);

fail:
    SSLFreeBuffer(&shaMsgState);
    SSLFreeBuffer(&md5MsgState);

    return serr;
}

const SslTlsCallouts Ssl3Callouts = {
	ssl3GenerateKeyMaterial,
	ssl3GenerateMasterSecret,
	ssl3ComputeFinishedMac,
	ssl3ComputeCertVfyMac
};

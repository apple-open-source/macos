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
	File:		ssl3Callouts.c

	Contains:	SSLv3-specific routines for SslTlsCallouts. 

	Written by:	Doug Mitchell
*/

#include "sslMemory.h"
#include "tls_ssl.h"
#include "sslUtils.h"
#include "sslDigests.h"
#include "ssl2.h"
#include "sslDebug.h"
#include "sslAlertMessage.h"

#include <assert.h>
#include <strings.h>

/*  
 * ssl3WriteRecord does not send alerts on failure, out of the assumption/fear
 * that this might result in a loop (since sending an alert causes ssl3WriteRecord
 * to be called).
 *
 * As far as I can tell, we can use this same routine for SSLv3 and TLSv1, as long 
 * as we're not trying to use the "variable length padding" feature of TLSv1.
 * OpenSSL doesn't use that feature; for now, neither will we. Thus this routine
 * is used for the SslTlsCallouts.writeRecord function for both protocols. 
 */
OSStatus ssl3WriteRecord(
	SSLRecord rec, 
	SSLContext *ctx)
{   
	OSStatus        err;
    int             padding = 0, i;
    WaitingRecord   *out = NULL, *queue;
    SSLBuffer       payload, mac;
    UInt8           *charPtr;
    UInt16          payloadSize,blockSize;
    
	switch(rec.protocolVersion) {
		case SSL_Version_2_0:
			return SSL2WriteRecord(&rec, ctx);
		case SSL_Version_3_0:
		case TLS_Version_1_0:
			break;
		default:
			assert(0);
			return errSSLInternal;
	}
    assert(rec.contents.length <= 16384);
    
    /* Allocate enough room for the transmitted record, which will be:
     *  5 bytes of header +
     *  encrypted contents +
     *  macLength +
     *  padding [block ciphers only] +
     *  padding length field (1 byte) [block ciphers only]
     */
    payloadSize = (UInt16) (rec.contents.length + ctx->writeCipher.macRef->hash->digestSize);
    blockSize = ctx->writeCipher.symCipher->blockSize;
    if (blockSize > 0)
    {   padding = blockSize - (payloadSize % blockSize) - 1;
        payloadSize += padding + 1;
    }
	
	out = (WaitingRecord *)sslMalloc(offsetof(WaitingRecord, data) +
		5 + payloadSize);
	out->next = NULL;
	out->sent = 0;
	out->length = 5 + payloadSize;
    
    charPtr = out->data;
    *(charPtr++) = rec.contentType;
    charPtr = SSLEncodeInt(charPtr, rec.protocolVersion, 2);
    charPtr = SSLEncodeInt(charPtr, payloadSize, 2);
    
    /* Copy the contents into the output buffer */
    memcpy(charPtr, rec.contents.data, rec.contents.length);
    payload.data = charPtr;
    payload.length = rec.contents.length;
    
    charPtr += rec.contents.length;
    /* MAC immediately follows data */
    mac.data = charPtr;
    mac.length = ctx->writeCipher.macRef->hash->digestSize;
    charPtr += mac.length;
    
    /* MAC the data */
    if (mac.length > 0)     /* Optimize away null case */
    {   
		assert(ctx->sslTslCalls != NULL);
        if ((err = ctx->sslTslCalls->computeMac(rec.contentType, 
				payload, 
				mac, 
				&ctx->writeCipher,
				ctx->writeCipher.sequenceNum, 
				ctx)) != 0)
            goto fail;
    }
    
    /* Update payload to reflect encrypted data: contents, mac & padding */
    payload.length = payloadSize;
    
    /* Fill in the padding bytes & padding length field with the padding value; the
     *  protocol only requires the last byte,
     *  but filling them all in avoids leaking data
     */
    if (ctx->writeCipher.symCipher->blockSize > 0)
        for (i = 1; i <= padding + 1; ++i)
            payload.data[payload.length - i] = padding;
    
    /* Encrypt the data */
    if ((err = ctx->writeCipher.symCipher->encrypt(payload, 
    		payload, 
    		&ctx->writeCipher, 
    		ctx)) != 0)
        goto fail;
    
    /* Enqueue the record to be written from the idle loop */
    if (ctx->recordWriteQueue == 0)
        ctx->recordWriteQueue = out;
    else
    {   queue = ctx->recordWriteQueue;
        while (queue->next != 0)
            queue = queue->next;
        queue->next = out;
    }
    
    /* Increment the sequence number */
    IncrementUInt64(&ctx->writeCipher.sequenceNum);
    
    return noErr;
    
fail:  
	/* 
	 * Only for if we fail between when the WaitingRecord is allocated and when 
	 * it is queued 
	 */
	sslFree(out);
    return err;
}

static OSStatus ssl3DecryptRecord(
	UInt8 type, 
	SSLBuffer *payload, 
	SSLContext *ctx)
{   
	OSStatus    err;
    SSLBuffer   content;
    
    if ((ctx->readCipher.symCipher->blockSize > 0) &&
        ((payload->length % ctx->readCipher.symCipher->blockSize) != 0))
    {   SSLFatalSessionAlert(SSL_AlertUnexpectedMsg, ctx);
        return errSSLProtocol;
    }

    /* Decrypt in place */
    if ((err = ctx->readCipher.symCipher->decrypt(*payload, 
    		*payload, 
    		&ctx->readCipher, 
    		ctx)) != 0)
    {   SSLFatalSessionAlert(SSL_AlertDecryptionFail, ctx);
        return err;
    }
    
	/* Locate content within decrypted payload */
    content.data = payload->data;
    content.length = payload->length - ctx->readCipher.macRef->hash->digestSize;
    if (ctx->readCipher.symCipher->blockSize > 0)
    {   /* padding can't be equal to or more than a block */
        if (payload->data[payload->length - 1] >= ctx->readCipher.symCipher->blockSize)
        {   SSLFatalSessionAlert(SSL_AlertDecryptionFail, ctx);
        	sslErrorLog("DecryptSSLRecord: bad padding length (%d)\n", 
        		(unsigned)payload->data[payload->length - 1]);
            return errSSLDecryptionFail;
        }
        content.length -= 1 + payload->data[payload->length - 1];  
						/* Remove block size padding */
    }

	/* Verify MAC on payload */
    if (ctx->readCipher.macRef->hash->digestSize > 0)       
		/* Optimize away MAC for null case */
        if ((err = SSLVerifyMac(type, &content, 
				payload->data + content.length, ctx)) != 0)
        {   SSLFatalSessionAlert(SSL_AlertBadRecordMac, ctx);
            return errSSLBadRecordMac;
        }
    
    *payload = content;     /* Modify payload buffer to indicate content length */
    
    return noErr;
}

/* initialize a per-CipherContext HashHmacContext for use in MACing each record */
static OSStatus ssl3InitMac (
	CipherContext *cipherCtx,		// macRef, macSecret valid on entry
									// macCtx valid on return
	SSLContext *ctx)
{
	const HashReference *hash;
	SSLBuffer *hashCtx;
	OSStatus serr;
	
	assert(cipherCtx->macRef != NULL);
	hash = cipherCtx->macRef->hash;
	assert(hash != NULL);
	
	hashCtx = &cipherCtx->macCtx.hashCtx;
	if(hashCtx->data != NULL) {
		SSLFreeBuffer(hashCtx, ctx);
	}
	serr = SSLAllocBuffer(hashCtx, hash->contextSize, ctx);
	if(serr) {
		return serr;
	}
	return noErr;
}

static OSStatus ssl3FreeMac (
	CipherContext *cipherCtx)
{
	SSLBuffer *hashCtx;
	
	assert(cipherCtx != NULL);
	/* this can be called on a completely zeroed out CipherContext... */
	if(cipherCtx->macRef == NULL) {
		return noErr;
	}
	hashCtx = &cipherCtx->macCtx.hashCtx;
	if(hashCtx->data != NULL) {
		sslFree(hashCtx->data);
		hashCtx->data = NULL;
	}
	hashCtx->length = 0;
	return noErr;
}

static OSStatus ssl3ComputeMac (
	UInt8 type, 
	SSLBuffer data, 			
	SSLBuffer mac, 					// caller mallocs data
	CipherContext *cipherCtx,		// assumes macCtx, macRef
	sslUint64 seqNo, 
	SSLContext *ctx)
{   
	OSStatus        err;
    UInt8           innerDigestData[SSL_MAX_DIGEST_LEN];
    UInt8           scratchData[11], *charPtr;
    SSLBuffer       digest, digestCtx, scratch;
	SSLBuffer		secret;
	
    const HashReference	*hash;
	
	assert(cipherCtx != NULL);
	assert(cipherCtx->macRef != NULL);
	hash = cipherCtx->macRef->hash;
	assert(hash != NULL);
    assert(hash->macPadSize <= MAX_MAC_PADDING);
    assert(hash->digestSize <= SSL_MAX_DIGEST_LEN);
	digestCtx = cipherCtx->macCtx.hashCtx;		// may be NULL, for null cipher
	secret.data = cipherCtx->macSecret;
	secret.length = hash->digestSize;
	
	/* init'd early in SSLNewContext() */
    assert(SSLMACPad1[0] == 0x36 && SSLMACPad2[0] == 0x5C);
    
	/*
	 * MAC = hash( MAC_write_secret + pad_2 + 
	 *		       hash( MAC_write_secret + pad_1 + seq_num + type + 
	 *			         length + content ) 
	 *			 ) 
	 */
    if ((err = hash->init(&digestCtx, ctx)) != 0)
        goto exit;
    if ((err = hash->update(&digestCtx, &secret)) != 0)    /* MAC secret */
        goto exit;
    scratch.data = (UInt8 *)SSLMACPad1;
    scratch.length = hash->macPadSize;
    if ((err = hash->update(&digestCtx, &scratch)) != 0)   /* pad1 */
        goto exit;
    charPtr = scratchData;
    charPtr = SSLEncodeUInt64(charPtr, seqNo);
    *charPtr++ = type;
    charPtr = SSLEncodeInt(charPtr, data.length, 2);
    scratch.data = scratchData;
    scratch.length = 11;
    assert(charPtr = scratchData+11);
    if ((err = hash->update(&digestCtx, &scratch)) != 0)   
										/* sequenceNo, type & length */
        goto exit;
    if ((err = hash->update(&digestCtx, &data)) != 0)      /* content */
        goto exit;
    digest.data = innerDigestData;
    digest.length = hash->digestSize;
    if ((err = hash->final(&digestCtx, &digest)) != 0) 	/* figure inner digest */
        goto exit;
    
    if ((err = hash->init(&digestCtx, ctx)) != 0)
        goto exit;
    if ((err = hash->update(&digestCtx, &secret)) != 0)    /* MAC secret */
        goto exit;
    scratch.data = (UInt8 *)SSLMACPad2;
    scratch.length = hash->macPadSize;
    if ((err = hash->update(&digestCtx, &scratch)) != 0)   /* pad2 */
        goto exit;
    if ((err = hash->update(&digestCtx, &digest)) != 0)    /* inner digest */
        goto exit;  
    if ((err = hash->final(&digestCtx, &mac)) != 0)   	 /* figure the mac */
        goto exit;
    
    err = noErr; /* redundant, I know */
    
exit:
    return err;
}

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
    int         i,j,remaining, satisfied;
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
			if ((err = SSLHashMD5.init(&md5Context, ctx)) != 0)
				goto fail;
			if ((err = SSLHashSHA1.init(&shaContext, ctx)) != 0)
				goto fail;
		}
    }
    
    assert(remaining == 0 && keyProgress == (key.data + key.length));
    err = noErr;
fail:
    SSLFreeBuffer(&md5Context, ctx);
    SSLFreeBuffer(&shaContext, ctx);
    
 	#if	LOG_GEN_KEY
	printf("GenerateKey: DONE\n");
	#endif
    return err;
}

static OSStatus ssl3GenerateExportKeyAndIv (
	SSLContext *ctx,				// clientRandom, serverRandom valid
	const SSLBuffer clientWriteKey,
	const SSLBuffer serverWriteKey,
	SSLBuffer finalClientWriteKey,	// RETURNED, mallocd by caller
	SSLBuffer finalServerWriteKey,	// RETURNED, mallocd by caller
	SSLBuffer finalClientIV,		// RETURNED, mallocd by caller
	SSLBuffer finalServerIV)		// RETURNED, mallocd by caller
{
	OSStatus err;
	SSLBuffer hashCtx, serverRandom, clientRandom; 
	
	/* random blobs are 32 bytes */
	serverRandom.data = ctx->serverRandom;
	serverRandom.length = SSL_CLIENT_SRVR_RAND_SIZE;
	clientRandom.data = ctx->clientRandom;
	clientRandom.length = SSL_CLIENT_SRVR_RAND_SIZE;
	
	if ((err = SSLAllocBuffer(&hashCtx, SSLHashMD5.contextSize, ctx)) != 0)
		return err;
	/* client write key */
	if ((err = SSLHashMD5.init(&hashCtx, ctx)) != 0)
		goto fail;
	if ((err = SSLHashMD5.update(&hashCtx, &clientWriteKey)) != 0)
		goto fail;
	if ((err = SSLHashMD5.update(&hashCtx, &clientRandom)) != 0)
		goto fail;
	if ((err = SSLHashMD5.update(&hashCtx, &serverRandom)) != 0)
		goto fail;
	finalClientWriteKey.length = 16;
	if ((err = SSLHashMD5.final(&hashCtx, &finalClientWriteKey)) != 0)
		goto fail;

	/* optional client IV */
	if (ctx->selectedCipherSpec->cipher->ivSize > 0)
	{   if ((err = SSLHashMD5.init(&hashCtx, ctx)) != 0)
			goto fail;
		if ((err = SSLHashMD5.update(&hashCtx, &clientRandom)) != 0)
			goto fail;
		if ((err = SSLHashMD5.update(&hashCtx, &serverRandom)) != 0)
			goto fail;
		finalClientIV.length = 16;
		if ((err = SSLHashMD5.final(&hashCtx, &finalClientIV)) != 0)
			goto fail;
	}

	/* server write key */
	if ((err = SSLHashMD5.init(&hashCtx, ctx)) != 0)
		goto fail;
	if ((err = SSLHashMD5.update(&hashCtx, &serverWriteKey)) != 0)
		goto fail;
	if ((err = SSLHashMD5.update(&hashCtx, &serverRandom)) != 0)
		goto fail;
	if ((err = SSLHashMD5.update(&hashCtx, &clientRandom)) != 0)
		goto fail;
	finalServerWriteKey.length = 16;
	if ((err = SSLHashMD5.final(&hashCtx, &finalServerWriteKey)) != 0)
		goto fail;
	
	/* optional server IV */
	if (ctx->selectedCipherSpec->cipher->ivSize > 0)
	{   if ((err = SSLHashMD5.init(&hashCtx, ctx)) != 0)
			goto fail;
		if ((err = SSLHashMD5.update(&hashCtx, &serverRandom)) != 0)
			goto fail;
		if ((err = SSLHashMD5.update(&hashCtx, &clientRandom)) != 0)
			goto fail;
		finalServerIV.length = 16;
		if ((err = SSLHashMD5.final(&hashCtx, &finalServerIV)) != 0)
			goto fail;
	}

    err = noErr;
fail:
    SSLFreeBuffer(&hashCtx, ctx);
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
    if ((err = SSLAllocBuffer(&md5State, SSLHashMD5.contextSize, ctx)) != 0)
        goto fail;
    if ((err = SSLAllocBuffer(&shaState, SSLHashSHA1.contextSize, ctx)) != 0)
        goto fail;
    
    clientRandom.data = ctx->clientRandom;
    clientRandom.length = SSL_CLIENT_SRVR_RAND_SIZE;
    serverRandom.data = ctx->serverRandom;
    serverRandom.length = SSL_CLIENT_SRVR_RAND_SIZE;
    shaHash.data = shaHashData;
    shaHash.length = 20;
    
    masterProgress = ctx->masterSecret;
    
    for (i = 1; i <= 3; i++)
    {   if ((err = SSLHashMD5.init(&md5State, ctx)) != 0)
            goto fail;
        if ((err = SSLHashSHA1.init(&shaState, ctx)) != 0)
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
    
    err = noErr;
fail:
    SSLFreeBuffer(&shaState, ctx);
    SSLFreeBuffer(&md5State, ctx);
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
            return err;
        if ((err = SSLHashSHA1.update(&shaMsgState, &input)) != 0)
            return err;
    }
    input.data = ctx->masterSecret;
    input.length = SSL_MASTER_SECRET_SIZE;
    if ((err = SSLHashMD5.update(&md5MsgState, &input)) != 0)
        return err;
    if ((err = SSLHashSHA1.update(&shaMsgState, &input)) != 0)
        return err;
    input.data = (UInt8 *)SSLMACPad1;
    input.length = SSLHashMD5.macPadSize;
    if ((err = SSLHashMD5.update(&md5MsgState, &input)) != 0)
        return err;
    input.length = SSLHashSHA1.macPadSize;
    if ((err = SSLHashSHA1.update(&shaMsgState, &input)) != 0)
        return err;
    hash.data = md5Inner;
    hash.length = 16;
    if ((err = SSLHashMD5.final(&md5MsgState, &hash)) != 0)
        return err;
    hash.data = shaInner;
    hash.length = 20;
    if ((err = SSLHashSHA1.final(&shaMsgState, &hash)) != 0)
        return err;
    if ((err = SSLHashMD5.init(&md5MsgState, ctx)) != 0)
        return err;
    if ((err = SSLHashSHA1.init(&shaMsgState, ctx)) != 0)
        return err;
    input.data = ctx->masterSecret;
    input.length = SSL_MASTER_SECRET_SIZE;
    if ((err = SSLHashMD5.update(&md5MsgState, &input)) != 0)
        return err;
    if ((err = SSLHashSHA1.update(&shaMsgState, &input)) != 0)
        return err;
    input.data = (UInt8 *)SSLMACPad2;
    input.length = SSLHashMD5.macPadSize;
    if ((err = SSLHashMD5.update(&md5MsgState, &input)) != 0)
        return err;
    input.length = SSLHashSHA1.macPadSize;
    if ((err = SSLHashSHA1.update(&shaMsgState, &input)) != 0)
        return err;
    input.data = md5Inner;
    input.length = 16;
    if ((err = SSLHashMD5.update(&md5MsgState, &input)) != 0)
        return err;
    hash.data = finished.data;
    hash.length = 16;
    if ((err = SSLHashMD5.final(&md5MsgState, &hash)) != 0)
        return err;
    input.data = shaInner;
    input.length = 20;
    if ((err = SSLHashSHA1.update(&shaMsgState, &input)) != 0)
        return err;
    hash.data = finished.data + 16;
    hash.length = 20;
    if ((err = SSLHashSHA1.final(&shaMsgState, &hash)) != 0)
        return err;
    return noErr;
}


static OSStatus ssl3ComputeFinishedMac (
	SSLContext *ctx,
	SSLBuffer finished, 		// output - mallocd by caller 
	SSLBuffer shaMsgState,		// clone of running digest of all handshake msgs
	SSLBuffer md5MsgState, 		// ditto
	Boolean isServer)			// refers to message, not us
{
	return ssl3CalculateFinishedMessage(ctx, finished, shaMsgState, md5MsgState, 
		isServer ? SSL_Finished_Sender_Server : SSL_Finished_Sender_Client);
}

static OSStatus ssl3ComputeCertVfyMac (
	SSLContext *ctx,
	SSLBuffer finished, 		// output - mallocd by caller 
	SSLBuffer shaMsgState,		// clone of running digest of all handshake msgs
	SSLBuffer md5MsgState) 		// ditto
{
	return ssl3CalculateFinishedMessage(ctx, finished, shaMsgState, md5MsgState, 0);
}

const SslTlsCallouts Ssl3Callouts = {
	ssl3DecryptRecord,
	ssl3WriteRecord,
	ssl3InitMac,
	ssl3FreeMac,
	ssl3ComputeMac,
	ssl3GenerateKeyMaterial,
	ssl3GenerateExportKeyAndIv,
	ssl3GenerateMasterSecret,
	ssl3ComputeFinishedMac,
	ssl3ComputeCertVfyMac
};

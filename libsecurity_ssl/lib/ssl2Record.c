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
	File:		ssl2Record.c

	Contains:	Record encrypting/decrypting/MACing for SSL 2

	Written by:	Doug Mitchell

	Copyright: (c) 1999 by Apple Computer, Inc., all rights reserved.

*/

#include "ssl2.h"
#include "sslRecord.h"
#include "sslMemory.h"
#include "sslContext.h"
#include "sslAlertMessage.h"
#include "sslDebug.h"
#include "sslUtils.h"
#include "sslDigests.h"

#include <string.h>

static OSStatus SSL2DecryptRecord(
	SSLBuffer *payload, 
	SSLContext *ctx);
static OSStatus SSL2VerifyMAC(
	SSLBuffer *content, 
	uint8 *compareMAC, 
	SSLContext *ctx);
static OSStatus SSL2CalculateMAC(
	SSLBuffer *secret, 
	SSLBuffer *content, 
	UInt32 seqNo, 
	const HashReference *hash,
	SSLBuffer *mac,
	SSLContext *ctx);


OSStatus
SSL2ReadRecord(SSLRecord *rec, SSLContext *ctx)
{   OSStatus        err;
	size_t			len, contentLen;
    int             padding, headerSize;
    uint8           *charPtr;
    SSLBuffer       readData, cipherFragment;
    
    switch (ctx->negProtocolVersion)
    {   case SSL_Version_Undetermined:
        case SSL_Version_2_0:
            break;
        case SSL_Version_3_0:           /* We've negotiated a 3.0 session; 
										 * we can send an alert */
		case TLS_Version_1_0:
            SSLFatalSessionAlert(SSL_AlertUnexpectedMsg, ctx);
            return errSSLProtocol;
        default:
            sslErrorLog("bad protocolVersion in ctx->protocolVersion");
			return errSSLInternal;
    }
    
    if (!ctx->partialReadBuffer.data || ctx->partialReadBuffer.length < 3)
    {   if (ctx->partialReadBuffer.data)
            if ((err = SSLFreeBuffer(&ctx->partialReadBuffer, ctx)) != 0)
            {   SSL2SendError(SSL2_ErrNoCipher, ctx);
                return err;
            }
        if ((err = SSLAllocBuffer(&ctx->partialReadBuffer, DEFAULT_BUFFER_SIZE, ctx)) != 0)
        {   SSL2SendError(SSL2_ErrNoCipher, ctx);
            return err;
        }
    }
    
    if (ctx->amountRead < 3)
    {   readData.length = 3 - ctx->amountRead;
        readData.data = ctx->partialReadBuffer.data + ctx->amountRead;
        len = readData.length;
        err = sslIoRead(readData, &len, ctx);
        if(err != 0)
        {   if (err == errSSLWouldBlock)
                ctx->amountRead += len;
            if (err == ioErr && ctx->amountRead == 0)    /* If the session closes on a record boundary, it's graceful */
                err = errSSLClosedGraceful;
            return err;
        }
        ctx->amountRead += len;
    }
    
    rec->contentType = SSL_RecordTypeV2_0;
    rec->protocolVersion = SSL_Version_2_0;
    charPtr = ctx->partialReadBuffer.data;
    
    if (((*charPtr) & 0x80) != 0)       /* High bit on -> specifies 2-byte header */
    {   headerSize = 2;
        contentLen = ((charPtr[0] & 0x7F) << 8) | charPtr[1];
        padding = 0;
    }
    else if (((*charPtr) & 0x40) != 0)	/* Bit 6 on -> specifies security escape */
    {   return errSSLProtocol;          /* No security escapes are defined */
    }
    else                                /* 3-byte header */
    {   headerSize = 3;
        contentLen = ((charPtr[0] & 0x3F) << 8) | charPtr[1];
        padding = charPtr[2];
    }
    
    /* 
     * FIXME - what's the max record size?
     * and why doesn't SSLReadRecord parse the 2 or 3 byte header?
	 * Note: I see contentLen of 0 coming back from www.cduniverse.com when
	 * it's only been given SSL_RSA_EXPORT_WITH_DES40_CBC_SHA.
     */
    if((contentLen == 0) || (contentLen > 0xffff)) {
    	return errSSLProtocol;
    }
    
    charPtr += headerSize;
    
    if (ctx->partialReadBuffer.length < headerSize + contentLen)
    {   if ((err = SSLReallocBuffer(&ctx->partialReadBuffer, 5 + contentLen, ctx)) != 0)
            return err;
    }
    
    if (ctx->amountRead < headerSize + contentLen)
    {   readData.length = headerSize + contentLen - ctx->amountRead;
        readData.data = ctx->partialReadBuffer.data + ctx->amountRead;
        len = readData.length;
        err = sslIoRead(readData, &len, ctx);
        if(err != 0)
        {   if (err == errSSLWouldBlock)
                ctx->amountRead += len;
            return err;
        }
        ctx->amountRead += len;
    }
    
    cipherFragment.data = ctx->partialReadBuffer.data + headerSize;
    cipherFragment.length = contentLen;
    if ((err = SSL2DecryptRecord(&cipherFragment, ctx)) != 0)
        return err;
    
    cipherFragment.length -= padding;       /* Remove padding; MAC was removed 
											 * by SSL2DecryptRecord */
    
    IncrementUInt64(&ctx->readCipher.sequenceNum);
    
	/* Allocate a buffer to return the plaintext in and return it */
    if ((err = SSLAllocBuffer(&rec->contents, cipherFragment.length, ctx)) != 0)
        return err;
    memcpy(rec->contents.data, cipherFragment.data, cipherFragment.length);
    
    ctx->amountRead = 0;        /* We've used all the data in the cache */
    
    return noErr;
}

OSStatus
SSL2WriteRecord(SSLRecord *rec, SSLContext *ctx)
{   OSStatus        err;
    int             padding = 0, i, headerSize;
    WaitingRecord   *out = NULL, *queue;
    SSLBuffer       content, payload, secret, mac;
    uint8           *charPtr;
    UInt16          payloadSize, blockSize;
    
    assert(rec->contents.length < 16384);
    
    payloadSize = (UInt16) 
		(rec->contents.length + ctx->writeCipher.macRef->hash->digestSize);
    blockSize = ctx->writeCipher.symCipher->blockSize;
    if (blockSize > 0)
    {   
		padding = blockSize - (payloadSize % blockSize);
        if (padding == blockSize)
            padding = 0;
        payloadSize += padding;
        headerSize = 3;
    }
    else
    {   padding = 0;
        headerSize = 2;
    }
	
	out = (WaitingRecord *)sslMalloc(offsetof(WaitingRecord, data) +
		headerSize + payloadSize);
	out->next = NULL;
	out->sent = 0;
	out->length = headerSize + payloadSize;

    charPtr = out->data;
    
    if (headerSize == 2)
        charPtr = SSLEncodeInt(charPtr, payloadSize | 0x8000, 2);
    else
    {   charPtr = SSLEncodeInt(charPtr, payloadSize, 2);
        *charPtr++ = padding;
    }
    
    payload.data = charPtr;
    payload.length = payloadSize;
    
    mac.data = charPtr;
    mac.length = ctx->writeCipher.macRef->hash->digestSize;
    charPtr += mac.length;
    
    content.data = charPtr;
    content.length = rec->contents.length + padding;
    memcpy(charPtr, rec->contents.data, rec->contents.length);
    charPtr += rec->contents.length;
    i = padding;
    while (i--)
        *charPtr++ = padding;

    assert(charPtr == out->data + out->length);
    
    secret.data = ctx->writeCipher.macSecret;
    secret.length = ctx->writeCipher.symCipher->keySize;
    if (mac.length > 0)
        if ((err = SSL2CalculateMAC(&secret, &content, 
				ctx->writeCipher.sequenceNum.low,
                ctx->writeCipher.macRef->hash, &mac, ctx)) != 0)
            goto fail;
    
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
	 * Only for if we fail between when the WaitingRecord is allocated and 
	 * when it is queued 
	 */
	sslFree(out);
    return err;
}

static OSStatus
SSL2DecryptRecord(SSLBuffer *payload, SSLContext *ctx)
{   OSStatus        err;
    SSLBuffer       content;
    
    if (ctx->readCipher.symCipher->blockSize > 0)
        if (payload->length % ctx->readCipher.symCipher->blockSize != 0)
            return errSSLProtocol;
    
	/* Decrypt in place */
    if ((err = ctx->readCipher.symCipher->decrypt(*payload, 
    		*payload, 
    		&ctx->readCipher, 
    		ctx)) != 0)
        return err;
    
    if (ctx->readCipher.macRef->hash->digestSize > 0)       
		/* Optimize away MAC for null case */
    {   content.data = payload->data + ctx->readCipher.macRef->hash->digestSize;        		/* Data is after MAC */
        content.length = payload->length - ctx->readCipher.macRef->hash->digestSize;
        if ((err = SSL2VerifyMAC(&content, payload->data, ctx)) != 0)
            return err;
		/* Adjust payload to remove MAC; caller is still responsible 
		 * for removing padding [if any] */
        *payload = content;
    }
    
    return noErr;
}

#define IGNORE_MAC_FAILURE	0

static OSStatus
SSL2VerifyMAC(SSLBuffer *content, uint8 *compareMAC, SSLContext *ctx)
{   OSStatus    err;
    uint8       calculatedMAC[SSL_MAX_DIGEST_LEN];
    SSLBuffer   secret, mac;
    
    secret.data = ctx->readCipher.macSecret;
    secret.length = ctx->readCipher.symCipher->keySize;
    mac.data = calculatedMAC;
    mac.length = ctx->readCipher.macRef->hash->digestSize;
    if ((err = SSL2CalculateMAC(&secret, content, ctx->readCipher.sequenceNum.low,
                                ctx->readCipher.macRef->hash, &mac, ctx)) != 0)
        return err;
    if (memcmp(mac.data, compareMAC, mac.length) != 0) {
		#if	IGNORE_MAC_FAILURE
		sslErrorLog("SSL2VerifyMAC: Mac verify failure\n");
		return noErr;
		#else
		sslErrorLog("SSL2VerifyMAC: Mac verify failure\n");
        return errSSLProtocol;
        #endif
    }
    return noErr;
}

#define LOG_MAC_DATA		0
#if		LOG_MAC_DATA
static void logMacData(
	char *field,
	SSLBuffer *data)
{
	int i;
	
	printf("%s: ", field);
	for(i=0; i<data->length; i++) {
		printf("%02X", data->data[i]);
		if((i % 4) == 3) {
			printf(" ");
		}
	}
	printf("\n");
}
#else	/* LOG_MAC_DATA */
#define logMacData(f, d)
#endif	/* LOG_MAC_DATA */

/* For SSL 2, the MAC is hash ( secret || content || sequence# )
 *  where secret is the decryption key for the message, content is
 *  the record data plus any padding used to round out the record
 *  size to an even multiple of the block size and sequence# is
 *  a monotonically increasing 32-bit unsigned integer.
 */
static OSStatus
SSL2CalculateMAC(
	SSLBuffer *secret, 
	SSLBuffer *content, 
	UInt32 seqNo, 
	const HashReference *hash, 
	SSLBuffer *mac, 
	SSLContext *ctx)
{   OSStatus    err;
    uint8       sequenceNum[4];
    SSLBuffer   seqData, hashContext;
    
    SSLEncodeInt(sequenceNum, seqNo, 4);
    seqData.data = sequenceNum;
    seqData.length = 4;
	
    hashContext.data = 0;
    if ((err = ReadyHash(hash, &hashContext, ctx)) != 0)
        return err;
    if ((err = hash->update(&hashContext, secret)) != 0)
        goto fail;
    if ((err = hash->update(&hashContext, content)) != 0)
        goto fail;
    if ((err = hash->update(&hashContext, &seqData)) != 0)
        goto fail;
    if ((err = hash->final(&hashContext, mac)) != 0)
        goto fail;

	logMacData("secret ", secret);
	logMacData("seqData", &seqData);
	logMacData("mac    ", mac);

    err = noErr;
fail:
    SSLFreeBuffer(&hashContext, ctx);
    return err;
}

OSStatus
SSL2SendError(SSL2ErrorCode error, SSLContext *ctx)
{   OSStatus        err;
    SSLRecord       rec;
    uint8           errorData[3];
    
    rec.contentType = SSL_RecordTypeV2_0;
    rec.protocolVersion = SSL_Version_2_0;
    rec.contents.data = errorData;
    rec.contents.length = 3;
    errorData[0] = SSL2_MsgError;
    SSLEncodeInt(errorData + 1, error, 2);
    
    err = SSL2WriteRecord(&rec, ctx);
    return err;
}

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
	File:		ssl2rec.c

	Contains:	Record encrypting/decrypting/MACing for SSL 2

	Written by:	Doug Mitchell, based on Netscape SSLRef 3.0

	Copyright: (c) 1999 by Apple Computer, Inc., all rights reserved.

*/
/*  *********************************************************************
    File: ssl2rec.c

    SSLRef 3.0 Final -- 11/19/96

    Copyright (c)1996 by Netscape Communications Corp.

    By retrieving this software you are bound by the licensing terms
    disclosed in the file "LICENSE.txt". Please read it, and if you don't
    accept the terms, delete this software.

    SSLRef 3.0 was developed by Netscape Communications Corp. of Mountain
    View, California <http://home.netscape.com/> and Consensus Development
    Corporation of Berkeley, California <http://www.consensus.com/>.

    *********************************************************************

    File: ssl2rec.c    Record encrypting/decrypting/MACing for SSL 2


    ****************************************************************** */

#ifndef _SSL2_H_
#include "ssl2.h"
#endif

#ifndef _SSLREC_H_
#include "sslrec.h"
#endif

#ifndef _SSLALLOC_H_
#include "sslalloc.h"
#endif

#ifndef _SSLCTX_H_
#include "sslctx.h"
#endif

#ifndef _SSLALERT_H_
#include "sslalert.h"
#endif

#ifndef	_SSL_DEBUG_H_
#include "sslDebug.h"
#endif

#ifndef _SSLUTIL_H_
#include "sslutil.h"
#endif

#ifndef	_DIGESTS_H_
#include "digests.h"
#endif

#ifndef	_APPLE_GLUE_H_
#include "appleGlue.h"
#endif

#include <string.h>

static SSLErr SSL2DecryptRecord(SSLBuffer *payload, SSLContext *ctx);
static SSLErr SSL2VerifyMAC(SSLBuffer content, UInt8 *compareMAC, SSLContext *ctx);
static SSLErr SSL2CalculateMAC(SSLBuffer secret, SSLBuffer content, UInt32 seqNo, const HashReference *hash, SSLBuffer mac, SSLContext *ctx);


SSLErr
SSL2ReadRecord(SSLRecord *rec, SSLContext *ctx)
{   SSLErr          err;
    UInt32          len, contentLen;
    int             padding, headerSize;
    UInt8           *progress;
    SSLBuffer       readData, cipherFragment;
    
    switch (ctx->negProtocolVersion)
    {   case SSL_Version_Undetermined:
        case SSL_Version_3_0_With_2_0_Hello:
        case SSL_Version_2_0:
            break;
        case SSL_Version_3_0:           /* We've negotiated a 3.0 session; we can send an alert */
		case TLS_Version_1_0:
            SSLFatalSessionAlert(alert_unexpected_message, ctx);
            return SSLProtocolErr;
        case SSL_Version_3_0_Only:      /* We haven't yet negotiated, but we don't want to support 2.0; just die without an alert */
            return SSLProtocolErr;
        default:
            sslPanic("bad protocolVersion in ctx->protocolVersion");
    }
    
    if (!ctx->partialReadBuffer.data || ctx->partialReadBuffer.length < 3)
    {   if (ctx->partialReadBuffer.data)
            if (ERR(err = SSLFreeBuffer(&ctx->partialReadBuffer, &ctx->sysCtx)) != 0)
            {   SSLFatalSessionAlert(alert_close_notify, ctx);
                return err;
            }
        if (ERR(err = SSLAllocBuffer(&ctx->partialReadBuffer, DEFAULT_BUFFER_SIZE, &ctx->sysCtx)) != 0)
        {   SSLFatalSessionAlert(alert_close_notify, ctx);
            return err;
        }
    }
    
    if (ctx->amountRead < 3)
    {   readData.length = 3 - ctx->amountRead;
        readData.data = ctx->partialReadBuffer.data + ctx->amountRead;
        len = readData.length;
        err = sslIoRead(readData, &len, ctx);
        if(err != 0)
        {   if (err == SSLWouldBlockErr)
                ctx->amountRead += len;
            if (err == SSLIOErr && ctx->amountRead == 0)    /* If the session closes on a record boundary, it's graceful */
                err = SSLConnectionClosedGraceful;
            return err;
        }
        ctx->amountRead += len;
    }
    
    rec->contentType = SSL_version_2_0_record;
    rec->protocolVersion = SSL_Version_2_0;
    progress = ctx->partialReadBuffer.data;
    
    if (((*progress) & 0x80) != 0)          /* High bit on -> specifies 2-byte header */
    {   headerSize = 2;
        contentLen = ((progress[0] & 0x7F) << 8) | progress[1];
        padding = 0;
    }
    else if (((*progress) & 0x40) != 0)     /* Bit 6 on -> specifies security escape */
    {   return ERR(SSLProtocolErr);                 /* No security escapes are defined */
    }
    else                                    /* 3-byte header */
    {   headerSize = 3;
        contentLen = ((progress[0] & 0x3F) << 8) | progress[1];
        padding = progress[2];
    }
    
    /* 
     * FIXME - what's the max record size?
     * and why doesn't SSLReadRecord parse the 2 or 3 byte header?
	 * Note: I see contentLen of 0 coming back from www.cduniverse.com when
	 * it's only been given SSL_RSA_EXPORT_WITH_DES40_CBC_SHA.
     */
    if((contentLen == 0) || (contentLen > 0xffff)) {
    	return SSLProtocolErr;
    }
    
    progress += headerSize;
    
    if (ctx->partialReadBuffer.length < headerSize + contentLen)
    {   if (ERR(err = SSLReallocBuffer(&ctx->partialReadBuffer, 5 + contentLen, &ctx->sysCtx)) != 0)
            return err;
    }
    
    if (ctx->amountRead < headerSize + contentLen)
    {   readData.length = headerSize + contentLen - ctx->amountRead;
        readData.data = ctx->partialReadBuffer.data + ctx->amountRead;
        len = readData.length;
        err = sslIoRead(readData, &len, ctx);
        if(err != 0)
        {   if (err == SSLWouldBlockErr)
                ctx->amountRead += len;
            return err;
        }
        ctx->amountRead += len;
    }
    
    cipherFragment.data = ctx->partialReadBuffer.data + headerSize;
    cipherFragment.length = contentLen;
    if (ERR(err = SSL2DecryptRecord(&cipherFragment, ctx)) != 0)
        return err;
    
    cipherFragment.length -= padding;       /* Remove padding; MAC was removed by SSL2DecryptRecord */
    
    IncrementUInt64(&ctx->readCipher.sequenceNum);
    
/* Allocate a buffer to return the plaintext in and return it */
    if (ERR(err = SSLAllocBuffer(&rec->contents, cipherFragment.length, &ctx->sysCtx)) != 0)
        return err;
    memcpy(rec->contents.data, cipherFragment.data, cipherFragment.length);
    
    ctx->amountRead = 0;        /* We've used all the data in the cache */
    
    return SSLNoErr;
}

SSLErr
SSL2WriteRecord(SSLRecord rec, SSLContext *ctx)
{   SSLErr          err;
    int             padding = 0, i, headerSize;
    WaitingRecord   *out, *queue;
    SSLBuffer       buf, content, payload, secret, mac;
    UInt8           *progress;
    UInt16          payloadSize, blockSize;
    
    CASSERT(rec.contents.length < 16384);
    
    out = 0;
    /* Allocate a WaitingRecord to store our ready-to-send record in */
    if (ERR(err = SSLAllocBuffer(&buf, sizeof(WaitingRecord), &ctx->sysCtx)) != 0)
        return err;
    out = (WaitingRecord*)buf.data;
    out->next = 0;
    out->sent = 0;
        
    payloadSize = (UInt16) 
		(rec.contents.length + ctx->writeCipher.macRef->hash->digestSize);
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
    out->data.data = 0;
    if (ERR(err = SSLAllocBuffer(&out->data, headerSize + payloadSize, &ctx->sysCtx)) != 0)
        goto fail;
    progress = out->data.data;
    
    if (headerSize == 2)
        progress = SSLEncodeInt(progress, payloadSize | 0x8000, 2);
    else
    {   progress = SSLEncodeInt(progress, payloadSize, 2);
        *progress++ = padding;
    }
    
    payload.data = progress;
    payload.length = payloadSize;
    
    mac.data = progress;
    mac.length = ctx->writeCipher.macRef->hash->digestSize;
    progress += mac.length;
    
    content.data = progress;
    content.length = rec.contents.length + padding;
    memcpy(progress, rec.contents.data, rec.contents.length);
    progress += rec.contents.length;
    i = padding;
    while (i--)
        *progress++ = padding;

    CASSERT(progress == out->data.data + out->data.length);
    
    secret.data = ctx->writeCipher.macSecret;
    secret.length = ctx->writeCipher.symCipher->keySize;
    if (mac.length > 0)
        if (ERR(err = SSL2CalculateMAC(secret, content, 
				ctx->writeCipher.sequenceNum.low,
                ctx->writeCipher.macRef->hash, mac, ctx)) != 0)
            goto fail;
    
    /* APPLE_CDSA change...*/
    if (ERR(err = ctx->writeCipher.symCipher->encrypt(payload, 
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
    
    return SSLNoErr;
    
fail:   /* Only for if we fail between when the WaitingRecord is allocated and when it is queued */
    SSLFreeBuffer(&out->data, 0);
    buf.data = (UInt8*)out;
    buf.length = sizeof(WaitingRecord);
    SSLFreeBuffer(&buf, &ctx->sysCtx);
    return err;
}

static SSLErr
SSL2DecryptRecord(SSLBuffer *payload, SSLContext *ctx)
{   SSLErr          err;
    SSLBuffer       content;
    
    if (ctx->readCipher.symCipher->blockSize > 0)
        if (payload->length % ctx->readCipher.symCipher->blockSize != 0)
            return ERR(SSLProtocolErr);
    
/* Decrypt in place */
	/* APPLE_CDSA change...*/
    if (ERR(err = ctx->readCipher.symCipher->decrypt(*payload, 
    		*payload, 
    		&ctx->readCipher, 
    		ctx)) != 0)
        return err;
    
    if (ctx->readCipher.macRef->hash->digestSize > 0)       /* Optimize away MAC for null case */
    {   content.data = payload->data + ctx->readCipher.macRef->hash->digestSize;        /* Data is after MAC */
        content.length = payload->length - ctx->readCipher.macRef->hash->digestSize;
        if (ERR(err = SSL2VerifyMAC(content, payload->data, ctx)) != 0)
            return err;
    /* Adjust payload to remove MAC; caller is still responsible for removing padding [if any] */
        *payload = content;
    }
    
    return SSLNoErr;
}

#define IGNORE_MAC_FAILURE	0

static SSLErr
SSL2VerifyMAC(SSLBuffer content, UInt8 *compareMAC, SSLContext *ctx)
{   SSLErr      err;
    UInt8       calculatedMAC[MAX_DIGEST_SIZE];
    SSLBuffer   secret, mac;
    
    secret.data = ctx->readCipher.macSecret;
    secret.length = ctx->readCipher.symCipher->keySize;
    mac.data = calculatedMAC;
    mac.length = ctx->readCipher.macRef->hash->digestSize;
    if (ERR(err = SSL2CalculateMAC(secret, content, ctx->readCipher.sequenceNum.low,
                                ctx->readCipher.macRef->hash, mac, ctx)) != 0)
        return err;
    if (memcmp(mac.data, compareMAC, mac.length) != 0) {
		#if	IGNORE_MAC_FAILURE
		dprintf0("SSL2VerifyMAC: Mac verify failure\n");
		return SSLNoErr;
		#else
		errorLog0("SSL2VerifyMAC: Mac verify failure\n");
        return ERR(SSLProtocolErr);
        #endif
    }
    return SSLNoErr;
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
static SSLErr
SSL2CalculateMAC(SSLBuffer secret, SSLBuffer content, UInt32 seqNo, const HashReference *hash, SSLBuffer mac, SSLContext *ctx)
{   SSLErr      err;
    UInt8       sequenceNum[4];
    SSLBuffer   seqData, hashContext;
    
    SSLEncodeInt(sequenceNum, seqNo, 4);
    seqData.data = sequenceNum;
    seqData.length = 4;
	
    hashContext.data = 0;
    if (ERR(err = ReadyHash(hash, &hashContext, ctx)) != 0)
        return err;
    if (ERR(err = hash->update(hashContext, secret)) != 0)
        goto fail;
    if (ERR(err = hash->update(hashContext, content)) != 0)
        goto fail;
    if (ERR(err = hash->update(hashContext, seqData)) != 0)
        goto fail;
    if (ERR(err = hash->final(hashContext, mac)) != 0)
        goto fail;

	logMacData("secret ", &secret);
	logMacData("seqData", &seqData);
	logMacData("mac    ", &mac);
    
    err = SSLNoErr;
fail:
    ERR(SSLFreeBuffer(&hashContext, &ctx->sysCtx));
    return err;
}

SSLErr
SSL2SendError(SSL2ErrorCode error, SSLContext *ctx)
{   SSLErr          err;
    SSLRecord       rec;
    UInt8           errorData[3];
    
    rec.contentType = SSL_version_2_0_record;
    rec.protocolVersion = SSL_Version_2_0;
    rec.contents.data = errorData;
    rec.contents.length = 3;
    errorData[0] = ssl2_mt_error;
    SSLEncodeInt(errorData + 1, error, 2);
    
    ERR(err = SSL2WriteRecord(rec, ctx));
    return err;
}

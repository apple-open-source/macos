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
	File:		sslrec.c

	Contains:	Encryption, decryption and MACing of data

	Written by:	Doug Mitchell, based on Netscape RSARef 3.0

	Copyright: (c) 1999 by Apple Computer, Inc., all rights reserved.

*/
/*  *********************************************************************
    File: sslrec.c

    SSLRef 3.0 Final -- 11/19/96

    Copyright (c)1996 by Netscape Communications Corp.

    By retrieving this software you are bound by the licensing terms
    disclosed in the file "LICENSE.txt". Please read it, and if you don't
    accept the terms, delete this software.

    SSLRef 3.0 was developed by Netscape Communications Corp. of Mountain
    View, California <http://home.netscape.com/> and Consensus Development
    Corporation of Berkeley, California <http://www.consensus.com/>.

    *********************************************************************

    File: sslrec.c     Encryption, decryption and MACing of data

    All the transformations which occur between plaintext and the
    secured, authenticated data that goes out over the wire. Also,
    detects incoming SSL 2 hello messages and hands them off to the SSL 2
    record layer (and hands all SSL 2 reading & writing off to the SSL 2
    layer).

    ****************************************************************** */


#include "ssl.h"

#ifndef _SSLREC_H_
#include "sslrec.h"
#endif

#ifndef _SSLALLOC_H_
#include "sslalloc.h"
#endif

#ifndef _CRYPTTYPE_H_
#include "cryptType.h"
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

#ifndef _SSL2_H_
#include "ssl2.h"
#endif

#ifndef _SSLUTIL_H_
#include "sslutil.h"
#endif

#ifdef	_APPLE_CDSA_
#ifndef	_APPLE_GLUE_H_
#include "appleGlue.h"
#endif
#endif

#include <string.h>

/*
 * Lots of servers fail to provide closure alerts when they disconnect. 
 * For now we'll just accept it as long as it occurs on a clean record boundary
 * (and the handshake is complete). 
 */
#define SSL_ALLOW_UNNOTICED_DISCONNECT	1

static SSLErr DecryptSSLRecord(UInt8 type, SSLBuffer *payload, SSLContext *ctx);
static SSLErr VerifyMAC(UInt8 type, SSLBuffer data, UInt8 *compareMAC, SSLContext *ctx);
static SSLErr ComputeMAC(UInt8 type, SSLBuffer data, SSLBuffer mac, sslUint64 seqNo, SSLBuffer secret, const HashReference *macHash, SSLContext *ctx);
static UInt8* SSLEncodeUInt64(UInt8 *p, sslUint64 value);

/* ReadSSLRecord
 *  Attempt to read & decrypt an SSL record.
 */
SSLErr
SSLReadRecord(SSLRecord *rec, SSLContext *ctx)
{   SSLErr          err;
    UInt32          len, contentLen;
    UInt8           *progress;
    SSLBuffer       readData, cipherFragment;
    
    if (!ctx->partialReadBuffer.data || ctx->partialReadBuffer.length < 5)
    {   if (ctx->partialReadBuffer.data)
            if ((err = SSLFreeBuffer(&ctx->partialReadBuffer, &ctx->sysCtx)) != 0)
            {   SSLFatalSessionAlert(alert_close_notify, ctx);
                return ERR(err);
            }
        if ((err = SSLAllocBuffer(&ctx->partialReadBuffer, DEFAULT_BUFFER_SIZE, &ctx->sysCtx)) != 0)
        {   SSLFatalSessionAlert(alert_close_notify, ctx);
            return ERR(err);
        }
    }
    
    if (ctx->negProtocolVersion == SSL_Version_Undetermined ||
        ctx->negProtocolVersion == SSL_Version_3_0_With_2_0_Hello)
        if (ctx->amountRead < 1)
        {   readData.length = 1 - ctx->amountRead;
            readData.data = ctx->partialReadBuffer.data + ctx->amountRead;
            len = readData.length;
            #ifdef	_APPLE_CDSA_
            err = sslIoRead(readData, &len, ctx);
            if(err != 0)
			#else
            if (ERR(err = ctx->ioCtx.read(readData, &len, ctx->ioCtx.ioRef)) != 0)
            #endif
            {   if (err == SSLWouldBlockErr)
                    ctx->amountRead += len;
                else
                    SSLFatalSessionAlert(alert_close_notify, ctx);
                return err;
            }
            ctx->amountRead += len;
        }
    
/* In undetermined cases, if the first byte isn't in the range of SSL 3.0
 *  record types, this is an SSL 2.0 record
 */
    switch (ctx->negProtocolVersion)
    {   case SSL_Version_Undetermined:
        case SSL_Version_3_0_With_2_0_Hello:
            if (ctx->partialReadBuffer.data[0] < SSL_smallest_3_0_type ||
                ctx->partialReadBuffer.data[0] > SSL_largest_3_0_type)
                return SSL2ReadRecord(rec, ctx);
            else
                break;
        case SSL_Version_2_0:
            return SSL2ReadRecord(rec, ctx);
        default:
            break;
    }
    
    if (ctx->amountRead < 5)
    {   readData.length = 5 - ctx->amountRead;
        readData.data = ctx->partialReadBuffer.data + ctx->amountRead;
        len = readData.length;
        #ifdef	_APPLE_CDSA_
        err = sslIoRead(readData, &len, ctx);
        if(err != 0)
		#else
        if (ERR(err = ctx->ioCtx.read(readData, &len, ctx->ioCtx.ioRef)) != 0)
        #endif
        {   
			switch(err) {
				case SSLWouldBlockErr:
					ctx->amountRead += len;
					break;
				#if	SSL_ALLOW_UNNOTICED_DISCONNECT
				case SSLConnectionClosedGraceful:
					/* legal if we're on record boundary and we've gotten past 
					 * the handshake */
					if((ctx->amountRead == 0) && 				/* nothing pending */
					   (len == 0) &&							/* nothing new */
					   (ctx->state == HandshakeClientReady)) {	/* handshake done */
					    /*
						 * This means that the server has discionected without 
						 * sending a closure alert notice. This is technically
						 * illegal per the SSL3 spec, but about half of the 
						 * servers out there do it, so we report it as a separate
						 * error which most clients - including (currently)
						 * URLAccess - ignore by treating it the same as
						 * a SSLConnectionClosedGraceful error. Paranoid
						 * clients can detect it and handle it however they
						 * want to.
						 */
						SSLChangeHdskState(ctx, SSLNoNotifyClose);
						err = SSLConnectionClosedNoNotify;
						break;
					}
					else {
						/* illegal disconnect */
						err = SSLConnectionClosedError;
						/* and drop thru to default: fatal alert */
					}
				#endif	/* SSL_ALLOW_UNNOTICED_DISCONNECT */
				default:
					SSLFatalSessionAlert(alert_close_notify, ctx);
					break;
				}
            return err;
        }
        ctx->amountRead += len;
    }
    
    CASSERT(ctx->amountRead >= 5);
    
    progress = ctx->partialReadBuffer.data;
    rec->contentType = *progress++;
    if (rec->contentType < SSL_smallest_3_0_type || 
        rec->contentType > SSL_largest_3_0_type)
        return ERR(SSLProtocolErr);
    
    rec->protocolVersion = (SSLProtocolVersion)SSLDecodeInt(progress, 2);
    progress += 2;
    contentLen = SSLDecodeInt(progress, 2);
    progress += 2;
    if (contentLen > (16384 + 2048))    /* Maximum legal length of an SSLCipherText payload */
    {   SSLFatalSessionAlert(alert_unexpected_message, ctx);
        return ERR(SSLProtocolErr);
    }
    
    if (ctx->partialReadBuffer.length < 5 + contentLen)
    {   if ((err = SSLReallocBuffer(&ctx->partialReadBuffer, 5 + contentLen, &ctx->sysCtx)) != 0)
        {   SSLFatalSessionAlert(alert_close_notify, ctx);
            return ERR(err);
        }
    }
    
    if (ctx->amountRead < 5 + contentLen)
    {   readData.length = 5 + contentLen - ctx->amountRead;
        readData.data = ctx->partialReadBuffer.data + ctx->amountRead;
        len = readData.length;
        #ifdef	_APPLE_CDSA_
        err = sslIoRead(readData, &len, ctx);
        if(err != 0)
		#else
        if (ERR(err = ctx->ioCtx.read(readData, &len, ctx->ioCtx.ioRef)) != 0)
        #endif
        {   if (err == SSLWouldBlockErr)
                ctx->amountRead += len;
            else
                SSLFatalSessionAlert(alert_close_notify, ctx);
            return err;
        }
        ctx->amountRead += len;
    }
    
    CASSERT(ctx->amountRead >= 5 + contentLen);
    
    cipherFragment.data = ctx->partialReadBuffer.data + 5;
    cipherFragment.length = contentLen;
    
/* Decrypt the payload & check the MAC, modifying the length of the buffer to indicate the
 *  amount of plaintext data after adjusting for the block size and removing the MAC
 *  (this function generates its own alerts)
 */
    if ((err = DecryptSSLRecord(rec->contentType, &cipherFragment, ctx)) != 0)
        return err;
    
/* We appear to have sucessfully received a record; increment the sequence number */
    IncrementUInt64(&ctx->readCipher.sequenceNum);
    
/* Allocate a buffer to return the plaintext in and return it */
    if ((err = SSLAllocBuffer(&rec->contents, cipherFragment.length, &ctx->sysCtx)) != 0)
    {   SSLFatalSessionAlert(alert_close_notify, ctx);
        return ERR(err);
    }
    memcpy(rec->contents.data, cipherFragment.data, cipherFragment.length);
    
    ctx->amountRead = 0;        /* We've used all the data in the cache */
    
    return SSLNoErr;
}

/* SSLWriteRecord does not send alerts on failure, out of the assumption/fear
 *  that this might result in a loop (since sending an alert causes SSLWriteRecord
 *  to be called).
 */
SSLErr
SSLWriteRecord(SSLRecord rec, SSLContext *ctx)
{   SSLErr          err;
    int             padding = 0, i;
    WaitingRecord   *out, *queue;
    SSLBuffer       buf, payload, secret, mac;
    UInt8           *progress;
    UInt16          payloadSize,blockSize;
    
    if (rec.protocolVersion == SSL_Version_2_0)
        return SSL2WriteRecord(rec, ctx);
    
    CASSERT(rec.protocolVersion == SSL_Version_3_0);
    CASSERT(rec.contents.length <= 16384);
    
    out = 0;
    /* Allocate a WaitingRecord to store our ready-to-send record in */
    if ((err = SSLAllocBuffer(&buf, sizeof(WaitingRecord), &ctx->sysCtx)) != 0)
        return ERR(err);
    out = (WaitingRecord*)buf.data;
    out->next = 0;
    out->sent = 0;
    /* Allocate enough room for the transmitted record, which will be:
     *  5 bytes of header +
     *  encrypted contents +
     *  macLength +
     *  padding [block ciphers only] +
     *  padding length field (1 byte) [block ciphers only]
     */
    payloadSize = (UInt16) (rec.contents.length + ctx->writeCipher.hash->digestSize);
    blockSize = ctx->writeCipher.symCipher->blockSize;
    if (blockSize > 0)
    {   padding = blockSize - (payloadSize % blockSize) - 1;
        payloadSize += padding + 1;
    }
    out->data.data = 0;
    if ((err = SSLAllocBuffer(&out->data, 5 + payloadSize, &ctx->sysCtx)) != 0)
        goto fail;
    
    progress = out->data.data;
    *(progress++) = rec.contentType;
    progress = SSLEncodeInt(progress, rec.protocolVersion, 2);
    progress = SSLEncodeInt(progress, payloadSize, 2);
    
    /* Copy the contents into the output buffer */
    memcpy(progress, rec.contents.data, rec.contents.length);
    payload.data = progress;
    payload.length = rec.contents.length;
    
    progress += rec.contents.length;
    /* MAC immediately follows data */
    mac.data = progress;
    mac.length = ctx->writeCipher.hash->digestSize;
    progress += mac.length;
    
    /* MAC the data */
    if (mac.length > 0)     /* Optimize away null case */
    {   secret.data = ctx->writeCipher.macSecret;
        secret.length = ctx->writeCipher.hash->digestSize;
        if ((err = ComputeMAC(rec.contentType, payload, mac, ctx->writeCipher.sequenceNum, secret, ctx->writeCipher.hash, ctx)) != 0)
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
    DUMP_BUFFER_NAME("cleartext data", payload);
    /* _APPLE_CDSA_ change */
    if ((err = ctx->writeCipher.symCipher->encrypt(payload, 
    		payload, 
    		&ctx->writeCipher, 
    		ctx)) != 0)
        goto fail;
    DUMP_BUFFER_NAME("encrypted data", payload);
    
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
    SSLFreeBuffer(&out->data, &ctx->sysCtx);
    buf.data = (UInt8*)out;
    buf.length = sizeof(WaitingRecord);
    SSLFreeBuffer(&buf, &ctx->sysCtx);
    return ERR(err);
}

static SSLErr
DecryptSSLRecord(UInt8 type, SSLBuffer *payload, SSLContext *ctx)
{   SSLErr      err;
    SSLBuffer   content;
    
    if ((ctx->readCipher.symCipher->blockSize > 0) &&
        ((payload->length % ctx->readCipher.symCipher->blockSize) != 0))
    {   SSLFatalSessionAlert(alert_unexpected_message, ctx);
        return ERR(SSLProtocolErr);
    }

    /* Decrypt in place */
    DUMP_BUFFER_NAME("encrypted data", (*payload));
    /* _APPLE_CDSA_ change */
    if ((err = ctx->readCipher.symCipher->decrypt(*payload, 
    		*payload, 
    		&ctx->readCipher, 
    		ctx)) != 0)
    {   SSLFatalSessionAlert(alert_close_notify, ctx);
        return ERR(err);
    }
    DUMP_BUFFER_NAME("decrypted data", (*payload));
    
/* Locate content within decrypted payload */
    content.data = payload->data;
    content.length = payload->length - ctx->readCipher.hash->digestSize;
    if (ctx->readCipher.symCipher->blockSize > 0)
    {   /* padding can't be equal to or more than a block */
        if (payload->data[payload->length - 1] >= ctx->readCipher.symCipher->blockSize)
        {   SSLFatalSessionAlert(alert_unexpected_message, ctx);
        	errorLog1("DecryptSSLRecord: bad padding length (%d)\n", 
        		(unsigned)payload->data[payload->length - 1]);
            return ERR(SSLProtocolErr);
        }
        content.length -= 1 + payload->data[payload->length - 1];   /* Remove block size padding */
    }

/* Verify MAC on payload */
    if (ctx->readCipher.hash->digestSize > 0)       /* Optimize away MAC for null case */
        if ((err = VerifyMAC(type, content, payload->data + content.length, ctx)) != 0)
        {   SSLFatalSessionAlert(alert_bad_record_mac, ctx);
            return ERR(err);
        }
    
    *payload = content;     /* Modify payload buffer to indicate content length */
    
    return SSLNoErr;
}

static UInt8*
SSLEncodeUInt64(UInt8 *p, sslUint64 value)
{   p = SSLEncodeInt(p, value.high, 4);
    return SSLEncodeInt(p, value.low, 4);
}

static SSLErr
VerifyMAC(UInt8 type, SSLBuffer data, UInt8 *compareMAC, SSLContext *ctx)
{   SSLErr          err;
    UInt8           macData[MAX_DIGEST_SIZE];
    SSLBuffer       secret, mac;
    
    secret.data = ctx->readCipher.macSecret;
    secret.length = ctx->readCipher.hash->digestSize;
    mac.data = macData;
    mac.length = ctx->readCipher.hash->digestSize;
    
    if ((err = ComputeMAC(type, data, mac, ctx->readCipher.sequenceNum, secret, ctx->readCipher.hash, ctx)) != 0)
        return ERR(err);
    
    if ((memcmp(mac.data, compareMAC, mac.length)) != 0) {
		errorLog0("VerifyMAC: Mac verify failure\n");
        return ERR(SSLProtocolErr);
    }
    return SSLNoErr;
}

static SSLErr
ComputeMAC(UInt8 type, SSLBuffer data, SSLBuffer mac, sslUint64 seqNo, SSLBuffer secret,
            const HashReference *macHash, SSLContext *ctx)
{   SSLErr          err;
    UInt8           innerDigestData[MAX_DIGEST_SIZE];
    UInt8           scratchData[11], *progress;
    SSLBuffer       digest,digestCtx,scratch;
    
    CASSERT(macHash->macPadSize <= MAX_MAC_PADDING);
    CASSERT(macHash->digestSize <= MAX_DIGEST_SIZE);
    CASSERT(SSLMACPad1[0] == 0x36 && SSLMACPad2[0] == 0x5C);
    
    digestCtx.data = 0;
    if ((err = SSLAllocBuffer(&digestCtx, macHash->contextSize, &ctx->sysCtx)) != 0)
        goto exit;
    
/* MAC = hash( MAC_write_secret + pad_2 + hash( MAC_write_secret + pad_1 + seq_num + type + length + content ) ) */
    if ((err = macHash->init(digestCtx)) != 0)
        goto exit;
    if ((err = macHash->update(digestCtx, secret)) != 0)    /* MAC secret */
        goto exit;
    scratch.data = SSLMACPad1;
    scratch.length = macHash->macPadSize;
    if ((err = macHash->update(digestCtx, scratch)) != 0)   /* pad1 */
        goto exit;
    progress = scratchData;
    progress = SSLEncodeUInt64(progress, seqNo);
    *progress++ = type;
    progress = SSLEncodeInt(progress, data.length, 2);
    scratch.data = scratchData;
    scratch.length = 11;
    CASSERT(progress = scratchData+11);
    if ((err = macHash->update(digestCtx, scratch)) != 0)   /* sequenceNo, type & length */
        goto exit;
    if ((err = macHash->update(digestCtx, data)) != 0)      /* content */
        goto exit;
    digest.data = innerDigestData;
    digest.length = macHash->digestSize;
    if ((err = macHash->final(digestCtx, digest)) != 0) /* figure inner digest */
        goto exit;
    
    if ((err = macHash->init(digestCtx)) != 0)
        goto exit;
    if ((err = macHash->update(digestCtx, secret)) != 0)    /* MAC secret */
        goto exit;
    scratch.data = SSLMACPad2;
    scratch.length = macHash->macPadSize;
    if ((err = macHash->update(digestCtx, scratch)) != 0)   /* pad2 */
        goto exit;
    if ((err = macHash->update(digestCtx, digest)) != 0)    /* inner digest */
        goto exit;  
    if ((err = macHash->final(digestCtx, mac)) != 0)    /* figure the mac */
        goto exit;
    
    err = SSLNoErr; /* redundant, I know */
    
exit:
    SSLFreeBuffer(&digestCtx, &ctx->sysCtx);
    return ERR(err);
}

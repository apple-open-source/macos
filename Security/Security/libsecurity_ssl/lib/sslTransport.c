/*
 * Copyright (c) 1999-2001,2005-2014 Apple Inc. All Rights Reserved.
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
 * sslTransport.c - SSL transport layer
 */

#include "ssl.h"
#include "sslMemory.h"
#include "sslContext.h"
#include "sslRecord.h"
#include "sslAlertMessage.h"
#include "sslSession.h"
#include "sslDebug.h"
#include "sslCipherSpecs.h"
#include "sslUtils.h"

#include <assert.h>
#include <string.h>

#include <utilities/SecIOFormat.h>

#ifndef	NDEBUG
static inline void sslIoTrace(
    SSLContext *ctx,
	const char *op,
	size_t req,
	size_t moved,
	OSStatus stat)
{
	sslLogRecordIo("[%p] ===%s: req %4lu moved %4lu status %d",
		ctx, op, req, moved, (int)stat);
}
#else
#define sslIoTrace(ctx, op, req, moved, stat)
#endif	/* NDEBUG */

extern int kSplitDefaultValue;

static OSStatus SSLProcessProtocolMessage(SSLRecord *rec, SSLContext *ctx);
static OSStatus SSLHandshakeProceed(SSLContext *ctx);
//static OSStatus SSLInitConnection(SSLContext *ctx);

OSStatus 
SSLWrite(
	SSLContext			*ctx,
	const void *		data,
	size_t				dataLength,
	size_t 				*bytesWritten)	/* RETURNED */
{
	OSStatus        err;
    SSLRecord       rec;
    size_t          dataLen, processed;

	sslLogRecordIo("[%p] SSLWrite top", ctx);
    if((ctx == NULL) || (bytesWritten == NULL)) {
    	return errSecParam;
    }
    dataLen = dataLength;
    processed = 0;        /* Initialize in case we return with errSSLWouldBlock */
    *bytesWritten = 0;

    switch(ctx->state) {
    	case SSL_HdskStateGracefulClose:
        	err = errSSLClosedGraceful;
			goto abort;
        case SSL_HdskStateErrorClose:
        	err = errSSLClosedAbort;
			goto abort;
	    case SSL_HdskStateReady:
			break;
        case SSL_HdskStateUninit:
            /* not ready for I/O, and handshake not in progress */
            sslIoTrace(ctx, "SSLWrite(1)", dataLength, 0, errSecBadReq);
            return errSecBadReq;
        default:
			/* handshake in progress or done. Will call SSLHandshakeProceed below if necessary */
			break;
	}

    /* First, we have to wait until the session is ready to send data,
        so the encryption keys and such have been established. */
    err = errSecSuccess;
    while (!(ctx->writeCipher_ready))
    {   if ((err = SSLHandshakeProceed(ctx)) != 0)
            goto exit;
    }

    /* Attempt to empty the write queue before queueing more data */
    if ((err = SSLServiceWriteQueue(ctx)) != 0)
        goto abort;

    processed = 0;

    /* Skip empty writes, fragmentation is done at the coreTLS layer */
    if(dataLen) {
        rec.contentType = SSL_RecordTypeAppData;
        rec.protocolVersion = ctx->negProtocolVersion;
        rec.contents.data = ((uint8_t *)data) + processed;
        rec.contents.length = dataLen;
        if ((err = SSLWriteRecord(rec, ctx)) != 0)
            goto exit;
        processed += rec.contents.length;
    }

    /* All the data has been advanced to the write queue */
    *bytesWritten = processed;
    if ((err = SSLServiceWriteQueue(ctx)) == 0) {
		err = errSecSuccess;
	}
exit:
	switch(err) {
		case errSecSuccess:
		case errSSLWouldBlock:
        case errSSLUnexpectedRecord:
		case errSSLServerAuthCompleted: /* == errSSLClientAuthCompleted */
		case errSSLClientCertRequested:
		case errSSLClosedGraceful:
			break;
		default:
			sslErrorLog("SSLWrite: going to state errorClose due to err %d\n",
				(int)err);
			SSLChangeHdskState(ctx, SSL_HdskStateErrorClose);
			break;
    }
abort:
	sslIoTrace(ctx, "SSLWrite(2)", dataLength, *bytesWritten, err);
    return err;
}

OSStatus
SSLRead	(
	SSLContext			*ctx,
	void *				data,
	size_t				dataLength,
	size_t 				*processed)		/* RETURNED */
{
	OSStatus        err;
    uint8_t         *charPtr;
    size_t          bufSize, remaining, count;
    SSLRecord       rec;

	sslLogRecordIo("[%p] SSLRead top (dataLength=%ld)", ctx, dataLength);
    if((ctx == NULL) || (data == NULL) || (processed == NULL)) {
    	return errSecParam;
    }
    bufSize = dataLength;
    *processed = 0;        /* Initialize in case we return with errSSLWouldBlock */

readRetry:
	/* first handle cases in which we know we're finished */
	switch(ctx->state) {
		case SSL_HdskStateGracefulClose:
			err = errSSLClosedGraceful;
			goto abort;
		case SSL_HdskStateErrorClose:
			err = errSSLClosedAbort;
			goto abort;
		case SSL_HdskStateNoNotifyClose:
			err = errSSLClosedNoNotify;
			goto abort;
		default:
			break;
	}

    /* First, we have to wait until the session is ready to receive data,
        so the encryption keys and such have been established. */
    err = errSecSuccess;
    while (ctx->readCipher_ready == 0) {
		if ((err = SSLHandshakeProceed(ctx)) != 0) {
            goto exit;
		}
    }

    /* Attempt to service the write queue */
    if ((err = SSLServiceWriteQueue(ctx)) != 0) {
		if (err != errSSLWouldBlock) {
            goto exit;
		}
        err = errSecSuccess; /* Write blocking shouldn't stop attempts to read */
    }

    remaining = bufSize;
    charPtr = (uint8_t *)data;
    if (ctx->receivedDataBuffer.data)
    {   count = ctx->receivedDataBuffer.length - ctx->receivedDataPos;
        if (count > bufSize)
            count = bufSize;
        memcpy(data, ctx->receivedDataBuffer.data + ctx->receivedDataPos, count);
        remaining -= count;
        charPtr += count;
        *processed += count;
        ctx->receivedDataPos += count;
    }

    assert(ctx->receivedDataPos <= ctx->receivedDataBuffer.length);
    assert(*processed + remaining == bufSize);
    assert(charPtr == ((uint8_t *)data) + *processed);

    if (ctx->receivedDataBuffer.data != 0 &&
        ctx->receivedDataPos >= ctx->receivedDataBuffer.length)
    {   SSLFreeBuffer(&ctx->receivedDataBuffer);
        ctx->receivedDataBuffer.data = 0;
        ctx->receivedDataPos = 0;
    }

	/*
	 * This while statement causes a hang when using nonblocking low-level I/O!
    while (remaining > 0 && ctx->state != SSL_HdskStateGracefulClose)
	 ..what we really have to do is just return as soon as we read one
	   record. A performance hit in the nonblocking case, but that is
	   the only way this code can work in both modes...
	 */
    if (remaining > 0 && ctx->state != SSL_HdskStateGracefulClose)
    {   assert(ctx->receivedDataBuffer.data == 0);
        if ((err = SSLReadRecord(&rec, ctx)) != 0) {
            goto exit;
        }
        if (rec.contentType == SSL_RecordTypeAppData ||
            rec.contentType == SSL_RecordTypeV2_0)
        {   if (rec.contents.length <= remaining)
            {   memcpy(charPtr, rec.contents.data, rec.contents.length);
                remaining -= rec.contents.length;
                charPtr += rec.contents.length;
                *processed += rec.contents.length;
                {
                    if ((err = SSLFreeRecord(rec, ctx))) {
                    	goto exit;
                    }
                }
            }
            else
            {   memcpy(charPtr, rec.contents.data, remaining);
                charPtr += remaining;
                *processed += remaining;
                ctx->receivedDataBuffer = rec.contents;
                ctx->receivedDataPos = remaining;
                remaining = 0;
            }
        }
        else {
            if ((err = SSLProcessProtocolMessage(&rec, ctx)) != 0) {
                /* This may not make much sense, but this is required so that we
                 process the write queue. This replicate exactly the behavior 
                 before the coreTLS adoption */
                if(err == errSSLClosedGraceful) {
                    err = SSLClose(ctx);
                } else {
                    goto exit;
                }
			}
            if ((err = SSLFreeRecord(rec, ctx))) {
                goto exit;
			}
        }
    }

    err = errSecSuccess;

exit:
	/* test for renegotiate: loop until something useful happens */
	if(((err == errSecSuccess)  && (*processed == 0) && dataLength) || (err == errSSLUnexpectedRecord)) {
		sslLogNegotiateDebug("SSLRead recursion");
		goto readRetry;
	}
	/* shut down on serious errors */
	switch(err) {
		case errSecSuccess:
		case errSSLWouldBlock:
        case errSSLUnexpectedRecord:
		case errSSLServerAuthCompleted: /* == errSSLClientAuthCompleted */
		case errSSLClientCertRequested:
		case errSSLClosedGraceful:
		case errSSLClosedNoNotify:
			break;
		default:
			sslErrorLog("SSLRead: going to state errorClose due to err %d\n",
				(int)err);
			SSLChangeHdskState(ctx, SSL_HdskStateErrorClose);
			break;
    }
abort:
	sslIoTrace(ctx, "SSLRead returns", dataLength, *processed, err);
    return err;
}

#if	SSL_DEBUG
#include "sslCrypto.h"
#endif

OSStatus
SSLHandshake(SSLContext *ctx)
{
	OSStatus  err;

	if(ctx == NULL) {
		return errSecParam;
	}
    if (ctx->state == SSL_HdskStateGracefulClose)
        return errSSLClosedGraceful;
    if (ctx->state == SSL_HdskStateErrorClose)
        return errSSLClosedAbort;

    if(ctx->validCipherSuites == NULL) {
    	/* build list of legal cipherSpecs */
        err = sslBuildCipherSuiteArray(ctx);
    	if(err) {
    		return err;
    	}
    }

    err = errSecSuccess;

    if(ctx->isDTLS && ctx->timeout_deadline) {
        CFAbsoluteTime current = CFAbsoluteTimeGetCurrent();

        if (ctx->timeout_deadline<current) {
            sslDebugLog("%p, retransmition deadline expired\n", ctx);
            err = tls_handshake_retransmit_timer_expired(ctx->hdsk);
            if(err) {
                return err;
            }
        }
    }

    while (ctx->readCipher_ready == 0 || ctx->writeCipher_ready == 0)
    {
        err = SSLHandshakeProceed(ctx);
        if((err != 0) && (err != errSSLUnexpectedRecord))
            return err;
    }

	/* one more flush at completion of successful handshake */
    if ((err = SSLServiceWriteQueue(ctx)) != 0) {
		return err;
	}

    return errSecSuccess;
}


static OSStatus
SSLHandshakeProceed(SSLContext *ctx)
{
    OSStatus  err;


    if(ctx->state==SSL_HdskStateUninit) {
        /* If we are the client, we start the negotiation */
        if(ctx->protocolSide == kSSLClientSide) {
            err = tls_handshake_negotiate(ctx->hdsk, &ctx->peerID);
            if(err)
                return err;
        }
        SSLChangeHdskState(ctx, SSL_HdskStatePending);
    }

    if ((err = tls_handshake_continue(ctx->hdsk)) != 0)
        return err;

    if ((err = SSLServiceWriteQueue(ctx)) != 0)
        return err;

    SSLRecord rec;

    err = SSLReadRecord(&rec, ctx);

    if(!err) {
        sslDebugLog("%p going to process a record (rec.len=%zd, ct=%d)\n", ctx, rec.contents.length, rec.contentType);
        err = tls_handshake_process(ctx->hdsk, rec.contents, rec.contentType);
        sslDebugLog("%p processed a record (rec.len=%zd, ct=%d, err=%d)\n", ctx, rec.contents.length, rec.contentType, (int)err);
        SSLFreeRecord(rec, ctx);
    } else if(err!=errSSLWouldBlock) {
        sslDebugLog("%p Read error err=%d\n\n", ctx, (int)err);
    }

    return err;
}

static OSStatus
SSLProcessProtocolMessage(SSLRecord *rec, SSLContext *ctx)
{
    return tls_handshake_process(ctx->hdsk, rec->contents, rec->contentType);
}

OSStatus
SSLClose(SSLContext *ctx)
{
	OSStatus      err = errSecSuccess;

	sslHdskStateDebug("SSLClose");
	if(ctx == NULL) {
		return errSecParam;
	}

    err = tls_handshake_close(ctx->hdsk);

    if (err == 0)
        err = SSLServiceWriteQueue(ctx);

    SSLChangeHdskState(ctx, SSL_HdskStateGracefulClose);
    if (err == errSecIO)
        err = errSecSuccess;     /* Ignore errors related to closed streams */
    return err;
}

/*
 * Determine how much data the client can be guaranteed to
 * obtain via SSLRead() without blocking or causing any low-level
 * read operations to occur.
 *
 * Implemented here because the relevant info in SSLContext (receivedDataBuffer
 * and receivedDataPos) are only used in this file.
 */
OSStatus
SSLGetBufferedReadSize(SSLContextRef ctx,
	size_t *bufSize)      			/* RETURNED */
{
	if(ctx == NULL) {
		return errSecParam;
	}
	if(ctx->receivedDataBuffer.data == NULL) {
		*bufSize = 0;
	}
	else {
		assert(ctx->receivedDataBuffer.length >= ctx->receivedDataPos);
		*bufSize = ctx->receivedDataBuffer.length - ctx->receivedDataPos;
	}
	return errSecSuccess;
}

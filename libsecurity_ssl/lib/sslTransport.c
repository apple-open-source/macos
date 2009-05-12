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
	File:		sslTransport.c

	Contains:	SSL transport layer

	Written by:	Doug Mitchell

	Copyright: (c) 1999 by Apple Computer, Inc., all rights reserved.

*/

#include "sslMemory.h"
#include "sslContext.h"
#include "sslRecord.h"
#include "sslAlertMessage.h"
#include "sslSession.h"
#include "ssl2.h"
#include "sslDebug.h"
#include "cipherSpecs.h"
#include "sslUtils.h"

#include <CoreServices/../Frameworks/CarbonCore.framework/Headers/MacErrors.h>

#include <assert.h>
#include <string.h>

#ifndef	NDEBUG
static void inline sslIoTrace(
	const char *op,
	UInt32 req, 
	UInt32 moved,
	OSStatus stat)
{
	sslLogRecordIo("===%s: req %4lu moved %4lu status %ld", 
		op, req, moved, stat);
}
#else
#define sslIoTrace(op, req, moved, stat)
#endif	/* NDEBUG */

static OSStatus SSLProcessProtocolMessage(SSLRecord *rec, SSLContext *ctx);
static OSStatus SSLHandshakeProceed(SSLContext *ctx);
static OSStatus SSLInitConnection(SSLContext *ctx);
static OSStatus SSLServiceWriteQueue(SSLContext *ctx);

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
    
	sslLogRecordIo("SSLWrite top");
    if((ctx == NULL) || (bytesWritten == NULL)) {
    	return paramErr;
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
        default:
	    	/* FIXME - original code didn't check for pending handshake - 
	    	 * should we? 
	    	 */
			sslIoTrace("SSLWrite", dataLength, 0, badReqErr);
	    	return badReqErr;
	    case SSL_HdskStateServerReady:
	    case SSL_HdskStateClientReady:
			break;
	}
	
    /* First, we have to wait until the session is ready to send data,
        so the encryption keys and such have been established. */
    err = noErr;
    while (ctx->writeCipher.ready == 0)
    {   if ((err = SSLHandshakeProceed(ctx)) != 0)
            goto exit;
    }
    
    /* Attempt to empty the write queue before queueing more data */
    if ((err = SSLServiceWriteQueue(ctx)) != 0)
        goto abort;
    
    processed = 0;
    /* 
	 * Fragment, package and encrypt the data and queue the resulting data 
	 * for sending 
	 */
    while (dataLen > 0)
    {   rec.contentType = SSL_RecordTypeAppData;
        rec.protocolVersion = ctx->negProtocolVersion;
        rec.contents.data = ((uint8*)data) + processed;
        
        if (dataLen < MAX_RECORD_LENGTH)
            rec.contents.length = dataLen;
        else
            rec.contents.length = MAX_RECORD_LENGTH;
        
		assert(ctx->sslTslCalls != NULL);
       if ((err = ctx->sslTslCalls->writeRecord(rec, ctx)) != 0) 
            goto exit;
        processed += rec.contents.length;
        dataLen -= rec.contents.length;
    }
    
    /* All the data has been advanced to the write queue */
    *bytesWritten = processed;
    if ((err = SSLServiceWriteQueue(ctx)) == 0) {
		err = noErr;
	}
exit:
    if (err != 0 && err != errSSLWouldBlock && err != errSSLClosedGraceful) {
		sslErrorLog("SSLWrite: going to state errorCLose due to err %d\n",
			(int)err);
        SSLChangeHdskState(ctx, SSL_HdskStateErrorClose);
    }
abort:
	sslIoTrace("SSLWrite", dataLength, *bytesWritten, err);
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
    uint8           *charPtr;
    size_t          bufSize, remaining, count;
    SSLRecord       rec;
    
	sslLogRecordIo("SSLRead top");
    if((ctx == NULL) || (processed == NULL)) {
    	return paramErr;
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
    err = noErr;
    while (ctx->readCipher.ready == 0) {   
		if ((err = SSLHandshakeProceed(ctx)) != 0) {
            goto exit;
		}
    }
    
    /* Attempt to service the write queue */
    if ((err = SSLServiceWriteQueue(ctx)) != 0) {
		if (err != errSSLWouldBlock) {
            goto exit;
		}
        err = noErr; /* Write blocking shouldn't stop attempts to read */
    }
    
    remaining = bufSize;
    charPtr = (uint8*)data;
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
    assert(charPtr == ((uint8*)data) + *processed);
    
    if (ctx->receivedDataBuffer.data != 0 &&
        ctx->receivedDataPos >= ctx->receivedDataBuffer.length)
    {   SSLFreeBuffer(&ctx->receivedDataBuffer, ctx);
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
                /* COMPILER BUG!
                 * This:
                 * if ((err = SSLFreeBuffer(rec.contents, ctx)) != 0)
                 * passes the address of rec to SSLFreeBuffer, not the address
                 * of the contents field (which should be offset 8 from the start
                 * of rec).
                 */
                {
                	SSLBuffer *b = &rec.contents;
                	if ((err = SSLFreeBuffer(b, ctx)) != 0) {
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
                goto exit;
			}
            if ((err = SSLFreeBuffer(&rec.contents, ctx)) != 0) {
                goto exit;
			}
        }
    }
    
    err = noErr;
    
exit:
	/* test for renegotiate: loop until something useful happens */
	if((err == noErr) && (*processed == 0)) {
		sslLogNegotiateDebug("SSLRead recursion");
		goto readRetry;
	}
	/* shut down on serious errors */
	switch(err) {
		case noErr:
		case errSSLWouldBlock:
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
	sslIoTrace("SSLRead ", dataLength, *processed, err);
    return err;
}

#if	SSL_DEBUG
#include "appleCdsa.h"
#endif

OSStatus
SSLHandshake(SSLContext *ctx)
{   
	OSStatus  err;

	if(ctx == NULL) {
		return paramErr;
	}
    if (ctx->state == SSL_HdskStateGracefulClose)
        return errSSLClosedGraceful;
    if (ctx->state == SSL_HdskStateErrorClose)
        return errSSLClosedAbort;
    
    if(ctx->validCipherSpecs == NULL) {
    	/* build list of legal cipherSpecs */
    	err = sslBuildCipherSpecArray(ctx);
    	if(err) {
    		return err;
    	}
    }
    err = noErr;
    while (ctx->readCipher.ready == 0 || ctx->writeCipher.ready == 0)
    {   if ((err = SSLHandshakeProceed(ctx)) != 0)
            return err;
    }
    
	/* one more flush at completion of successful handshake */ 
    if ((err = SSLServiceWriteQueue(ctx)) != 0) {
		return err;
	}
    return noErr;
}


static OSStatus
SSLHandshakeProceed(SSLContext *ctx)
{   OSStatus    err;
    SSLRecord   rec;
    
    if (ctx->state == SSL_HdskStateUninit)
        if ((err = SSLInitConnection(ctx)) != 0)
            return err;

	/* This is our cue that we dropped out of the handshake
	 * to let the client pick an identity, move state back
	 * to server hello done and continue processing.
	 */
	if ((ctx->protocolSide == SSL_ClientSide) &&
		(ctx->state == SSL_HdskStateClientCert))
		if ((err = SSLAdvanceHandshake(SSL_HdskServerHelloDone, ctx)) != 0)
			return err;
	
    if ((err = SSLServiceWriteQueue(ctx)) != 0)
        return err;
    assert(ctx->readCipher.ready == 0);
    if ((err = SSLReadRecord(&rec, ctx)) != 0)
        return err;
    if ((err = SSLProcessProtocolMessage(&rec, ctx)) != 0)
    {   SSLFreeBuffer(&rec.contents, ctx);
        return err;
    }
    if ((err = SSLFreeBuffer(&rec.contents, ctx)) != 0)
        return err;
        
    return noErr;
}

static OSStatus
SSLInitConnection(SSLContext *ctx)
{   OSStatus      err = noErr;
    
    if (ctx->protocolSide == SSL_ClientSide) {
        SSLChangeHdskState(ctx, SSL_HdskStateClientUninit);
    }
    else
    {   assert(ctx->protocolSide == SSL_ServerSide);
        SSLChangeHdskState(ctx, SSL_HdskStateServerUninit);
    }
    
    if (ctx->peerID.data != 0) 
    {   SSLGetSessionData(&ctx->resumableSession, ctx);
        /* Ignore errors; just treat as uncached session */
    }
    
	/* 
	 * If we have a cached resumable session, blow it off if it's a version
	 * which is not currently enabled.
	 */
	Boolean cachedV3OrTls1 = false;
	
    if (ctx->resumableSession.data != 0) {
    
		SSLProtocolVersion savedVersion;
		Boolean enable;
		
		if ((err = SSLRetrieveSessionProtocolVersion(ctx->resumableSession,
				&savedVersion, ctx)) != 0) {
            return err;
		}
		switch(savedVersion) {
			case SSL_Version_2_0:
				enable = ctx->versionSsl2Enable;
				break;
			case SSL_Version_3_0:
				enable = ctx->versionSsl3Enable;
				cachedV3OrTls1 = true;		// avoid V2 hello
				break;
			case TLS_Version_1_0:
				enable = ctx->versionTls1Enable;
				cachedV3OrTls1 = true;	
				break;
			default:
				assert(0);
				return errSSLInternal;
		}
		if(!enable) {
			sslLogResumSessDebug("===Resumable session protocol mismatch");
			SSLFreeBuffer(&ctx->resumableSession, ctx);
			cachedV3OrTls1 = false;
		} 
		else {
			sslLogResumSessDebug("===attempting to resume session");
		}
    }
    
	/* 
	 * If we're the client & handshake hasn't yet begun, start it by
	 *  pretending we just received a hello request
	 */
    if (ctx->state == SSL_HdskStateClientUninit && ctx->writeCipher.ready == 0)
    {   
		assert(ctx->negProtocolVersion == SSL_Version_Undetermined);
		if(ctx->versionSsl2Enable && !cachedV3OrTls1) {
			/* SSL2 client hello with possible upgrade */
			err = SSL2AdvanceHandshake(SSL2_MsgKickstart, ctx);
		}
		else {
			err = SSLAdvanceHandshake(SSL_HdskHelloRequest, ctx);
		}
    }
    
    return err;
}

static OSStatus
SSLServiceWriteQueue(SSLContext *ctx)
{   OSStatus        err = noErr, werr = noErr;
    size_t          written = 0;
    SSLBuffer       buf;
    WaitingRecord   *rec;

    while (!werr && ((rec = ctx->recordWriteQueue) != 0))
    {   buf.data = rec->data + rec->sent;
        buf.length = rec->length - rec->sent;
        werr = sslIoWrite(buf, &written, ctx);
        rec->sent += written;
        if (rec->sent >= rec->length)
        {   assert(rec->sent == rec->length);
            assert(err == 0);
            ctx->recordWriteQueue = rec->next;
			sslFree(rec);
        }
        if (err)
            return err;
    }

    return werr;
}

static OSStatus
SSLProcessProtocolMessage(SSLRecord *rec, SSLContext *ctx)
{   OSStatus      err;
    
    switch (rec->contentType)
    {   case SSL_RecordTypeHandshake:
			sslLogRxProtocolDebug("Handshake");
            err = SSLProcessHandshakeRecord(*rec, ctx);
            break;
        case SSL_RecordTypeAlert:
 			sslLogRxProtocolDebug("Alert");
            err = SSLProcessAlert(*rec, ctx);
            break;
        case SSL_RecordTypeChangeCipher:
			sslLogRxProtocolDebug("ChangeCipher");
            err = SSLProcessChangeCipherSpec(*rec, ctx);
            break;
        case SSL_RecordTypeV2_0:
			sslLogRxProtocolDebug("RecordTypeV2_0");
            err = SSL2ProcessMessage(rec, ctx);
            break;
        default:
			sslLogRxProtocolDebug("Bad msg");
            return errSSLProtocol;
    }
    
    return err;
}

OSStatus
SSLClose(SSLContext *ctx)
{   
	OSStatus      err = noErr;	
    
	sslHdskStateDebug("SSLClose");
	if(ctx == NULL) {
		return paramErr;
	}
    if (ctx->negProtocolVersion >= SSL_Version_3_0)
        err = SSLSendAlert(SSL_AlertLevelWarning, SSL_AlertCloseNotify, ctx);
    if (err == 0)
        err = SSLServiceWriteQueue(ctx);
    SSLChangeHdskState(ctx, SSL_HdskStateGracefulClose);
    if (err == ioErr)
        err = noErr;     /* Ignore errors related to closed streams */
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
		return paramErr;
	}
	if(ctx->receivedDataBuffer.data == NULL) {
		*bufSize = 0;
	}
	else {
		assert(ctx->receivedDataBuffer.length >= ctx->receivedDataPos);
		*bufSize = ctx->receivedDataBuffer.length - ctx->receivedDataPos;
	}
	return noErr;
}

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
	File:		ssltrspt.c

	Contains:	SSLContext transport layer

	Written by:	Doug Mitchell, based on Netscape SSLRef 3.0

	Copyright: (c) 1999 by Apple Computer, Inc., all rights reserved.

*/
/*  *********************************************************************
    File: ssltrspt.c

    SSLRef 3.0 Final -- 11/19/96

    Copyright (c)1996 by Netscape Communications Corp.

    By retrieving this software you are bound by the licensing terms
    disclosed in the file "LICENSE.txt". Please read it, and if you don't
    accept the terms, delete this software.

    SSLRef 3.0 was developed by Netscape Communications Corp. of Mountain
    View, California <http://home.netscape.com/> and Consensus Development
    Corporation of Berkeley, California <http://www.consensus.com/>.

    *********************************************************************

    File: ssltrspt.c   Data transportation functionality

    Transports data between the application and the record layer; also
    hands off handshake, alert, and change cipher spec messages to their
    handlers. Also, ensures that negotiation happens before application
    data goes out on the wire.

    ****************************************************************** */

#ifndef _SSLTRSPT_H_
#include "ssltrspt.h"
#endif

#ifndef _SSLALLOC_H_
#include "sslalloc.h"
#endif

#ifndef _SSLCTX_H_
#include "sslctx.h"
#endif

#ifndef _SSLCTX_H_
#include "sslrec.h"
#endif

#ifndef _SSLALERT_H_
#include "sslalert.h"
#endif

#ifndef _SSLSESS_H_
#include "sslsess.h"
#endif

#ifndef _SSL2_H_
#include "ssl2.h"
#endif

#ifndef	_APPLE_GLUE_H_
#include "appleGlue.h"
#endif

#ifndef	_SSL_DEBUG_H_
#include "sslDebug.h"
#endif

#ifndef	_CIPHER_SPECS_H_
#include "cipherSpecs.h"
#endif

#include <CoreServices/../Frameworks/CarbonCore.framework/Headers/MacErrors.h>

#include <assert.h>
#include <string.h>

#define SSL_IO_TRACE	0
#if		SSL_IO_TRACE
static void sslIoTrace(
	const char *op,
	UInt32 req, 
	UInt32 moved,
	OSStatus stat)
{
	printf("===%s: req %4d moved %4d status %d\n", 
		op, req, moved, stat);
}

#else
#define sslIoTrace(op, req, moved, stat)
#endif	/* SSL_IO_TRACE */

static SSLErr SSLProcessProtocolMessage(SSLRecord rec, SSLContext *ctx);
static SSLErr SSLHandshakeProceed(SSLContext *ctx);
static SSLErr SSLInitConnection(SSLContext *ctx);
static SSLErr SSLServiceWriteQueue(SSLContext *ctx);

OSStatus 
SSLWrite(
	SSLContext			*ctx,
	const void *		data,
	UInt32				dataLength,
	UInt32 				*bytesWritten)	/* RETURNED */ 
{   
	SSLErr          err;
    SSLRecord       rec;
    UInt32          dataLen, processed;
    
    if((ctx == NULL) || (bytesWritten == NULL)) {
    	return paramErr;
    }
    dataLen = dataLength;
    processed = 0;        /* Initialize in case we return with SSLWouldBlockErr */
    *bytesWritten = 0;
    
    switch(ctx->state) {
    	case SSLGracefulClose:
        	err = SSLConnectionClosedGraceful;
			goto abort;
        case SSLErrorClose:
        	err = SSLConnectionClosedError;
			goto abort;
        default:
	    	/* FIXME - original code didn't check for handshake in progress - 
	    	 * should we? 
	    	 */
			sslIoTrace("SSLWrite", dataLength, 0, badReqErr);
	    	return badReqErr;
	    case HandshakeServerReady:
	    case HandshakeClientReady:
			break;
	}
	
    /* First, we have to wait until the session is ready to send data,
        so the encryption keys and such have been established. */
    err = SSLNoErr;
    while (ctx->writeCipher.ready == 0)
    {   if ((err = SSLHandshakeProceed(ctx)) != 0)
            goto exit;
    }
    
    /* Attempt to empty the write queue before queueing more data */
    if ((err = SSLServiceWriteQueue(ctx)) != 0)
        goto abort;
    
    processed = 0;
    /* Fragment, package and encrypt the data and queue the resulting data for sending */
    while (dataLen > 0)
    {   rec.contentType = SSL_application_data;
        rec.protocolVersion = ctx->negProtocolVersion;
        rec.contents.data = ((UInt8*)data) + processed;
        
        if (dataLen < MAX_RECORD_LENGTH)
            rec.contents.length = dataLen;
        else
            rec.contents.length = MAX_RECORD_LENGTH;
        
		assert(ctx->sslTslCalls != NULL);
       if (ERR(err = ctx->sslTslCalls->writeRecord(rec, ctx)) != 0)
            goto exit;
        
        processed += rec.contents.length;
        dataLen -= rec.contents.length;
    }
    
    /* All the data has been advanced to the write queue */
    *bytesWritten = processed;
    if (ERR(err = SSLServiceWriteQueue(ctx)) != 0)
        goto exit;
    
    err = SSLNoErr;
exit:
    if (err != 0 && err != SSLWouldBlockErr && err != SSLConnectionClosedGraceful) {
		dprintf1("SSLWrite: going to state errorCLose due to err %d\n",
			err);
        SSLChangeHdskState(ctx, SSLErrorClose);
    }
abort:
	sslIoTrace("SSLWrite", dataLength, *bytesWritten, sslErrToOsStatus(err));
    return sslErrToOsStatus(err);
}

OSStatus 
SSLRead	(
	SSLContext			*ctx,
	void *				data,
	UInt32				dataLength,
	UInt32 				*processed)		/* RETURNED */ 
{   
	SSLErr          err;
    UInt8           *progress;
    UInt32          bufSize, remaining, count;
    SSLRecord       rec;
    
    if((ctx == NULL) || (processed == NULL)) {
    	return paramErr;
    }
    bufSize = dataLength;
    *processed = 0;        /* Initialize in case we return with SSLWouldBlockErr */

	/* first handle cases in which we know we're finished */
	switch(ctx->state) {
		case SSLGracefulClose:
			err = SSLConnectionClosedGraceful;
			goto abort;
		case SSLErrorClose:
			err = SSLConnectionClosedError;
			goto abort;
		case SSLNoNotifyClose:
			err = SSLConnectionClosedNoNotify;
			goto abort;
		default:
			break;
	}
    
    /* First, we have to wait until the session is ready to receive data,
        so the encryption keys and such have been established. */
    err = SSLNoErr;
    while (ctx->readCipher.ready == 0)
    {   if (ERR(err = SSLHandshakeProceed(ctx)) != 0)
            goto exit;
    }
    
    /* Attempt to service the write queue */
    if (ERR(err = SSLServiceWriteQueue(ctx)) != 0)
    {   if (err != SSLWouldBlockErr)
            goto exit;
        err = SSLNoErr; /* Write blocking shouldn't stop attempts to read */
    }
    
    remaining = bufSize;
    progress = (UInt8*)data;
    if (ctx->receivedDataBuffer.data)
    {   count = ctx->receivedDataBuffer.length - ctx->receivedDataPos;
        if (count > bufSize)
            count = bufSize;
        memcpy(data, ctx->receivedDataBuffer.data + ctx->receivedDataPos, count);
        remaining -= count;
        progress += count;
        *processed += count;
        ctx->receivedDataPos += count;
    }
    
    CASSERT(ctx->receivedDataPos <= ctx->receivedDataBuffer.length);
    CASSERT(*processed + remaining == bufSize);
    CASSERT(progress == ((UInt8*)data) + *processed);
    
    if (ctx->receivedDataBuffer.data != 0 &&
        ctx->receivedDataPos >= ctx->receivedDataBuffer.length)
    {   SSLFreeBuffer(&ctx->receivedDataBuffer, &ctx->sysCtx);
        ctx->receivedDataBuffer.data = 0;
        ctx->receivedDataPos = 0;
    }
    
	/*
	 * This while statement causes a hang when using nonblocking low-level I/O!
    while (remaining > 0 && ctx->state != SSLGracefulClose)
	 ..what we really have to do is just return as soon as we read one 
	   record. A performance hit in the nonblocking case, but that is 
	   the only way this code can work in both modes...
	 */
    if (remaining > 0 && ctx->state != SSLGracefulClose)
    {   CASSERT(ctx->receivedDataBuffer.data == 0);
        if (ERR(err = SSLReadRecord(&rec, ctx)) != 0)
            goto exit;
        
        if (rec.contentType == SSL_application_data ||
            rec.contentType == SSL_version_2_0_record)
        {   if (rec.contents.length <= remaining)
            {   memcpy(progress, rec.contents.data, rec.contents.length);
                remaining -= rec.contents.length;
                progress += rec.contents.length;
                *processed += rec.contents.length;
                /* COMPILER BUG!
                 * This:
                 * if (ERR(err = SSLFreeBuffer(&rec.contents, &ctx->sysCtx)) != 0)
                 * passes the address of rec to SSLFreeBuffer, not the address
                 * of the contents field (which should be offset 8 from the start
                 * of rec).
                 */
                {
                	SSLBuffer *b = &rec.contents;
                	if (ERR(err = SSLFreeBuffer(b, &ctx->sysCtx)) != 0) {
                    	goto exit;
                    }
                }
            }
            else
            {   memcpy(progress, rec.contents.data, remaining);
                progress += remaining;
                *processed += remaining;
                ctx->receivedDataBuffer = rec.contents;
                ctx->receivedDataPos = remaining;
                remaining = 0;
            }
        }
        else
        {   if (ERR(err = SSLProcessProtocolMessage(rec, ctx)) != 0)
                goto exit;
            if (ERR(err = SSLFreeBuffer(&rec.contents, &ctx->sysCtx)) != 0)
                goto exit;
        }
    }
    
    err = SSLNoErr;
    
exit:
	/* shut down on serious errors */
	switch(err) {
		case SSLNoErr:
		case SSLWouldBlockErr:
		case SSLConnectionClosedGraceful:
		case SSLConnectionClosedNoNotify:
			break;
		default:
			dprintf1("SSLRead: going to state errorClose due to err %d\n",
				err);
			SSLChangeHdskState(ctx, SSLErrorClose);
			break;
    }
abort:
	sslIoTrace("SSLRead ", dataLength, *processed, sslErrToOsStatus(err));
    return sslErrToOsStatus(err);
}

#if	SSL_DEBUG
#include "appleCdsa.h"
#endif

OSStatus
SSLHandshake(SSLContext *ctx)
{   
	SSLErr  err;

	if(ctx == NULL) {
		return paramErr;
	}
    if (ctx->state == SSLGracefulClose)
        return sslErrToOsStatus(SSLConnectionClosedGraceful);
    if (ctx->state == SSLErrorClose)
        return sslErrToOsStatus(SSLConnectionClosedError);
    
    if(ctx->protocolSide == SSL_ServerSide) {
    	/* some things the caller really has to have done by now... */
    	if((ctx->localCert == NULL) ||
    	   (ctx->signingPrivKey == NULL) ||
    	   (ctx->signingPubKey == NULL) ||
    	   (ctx->signingKeyCsp == 0)) {
    	 	errorLog0("SSLHandshake: insufficient init\n");
    	 	return badReqErr;
    	}
    }
    if(ctx->validCipherSpecs == NULL) {
    	/* build list of legal cipherSpecs */
    	err = sslBuildCipherSpecArray(ctx);
    	if(err) {
    		return err;
    	}
    }
    err = SSLNoErr;
    while (ctx->readCipher.ready == 0 || ctx->writeCipher.ready == 0)
    {   if (ERR(err = SSLHandshakeProceed(ctx)) != 0)
            return sslErrToOsStatus(err);
    }
    
	/* one more flush at completion of successful handshake */ 
    if ((err = SSLServiceWriteQueue(ctx)) != 0) {
		return sslErrToOsStatus(err);
	}
    return noErr;
}


static SSLErr
SSLHandshakeProceed(SSLContext *ctx)
{   SSLErr      err;
    SSLRecord   rec;
    
    if (ctx->state == SSLUninitialized)
        if (ERR(err = SSLInitConnection(ctx)) != 0)
            return err;
    if (ERR(err = SSLServiceWriteQueue(ctx)) != 0)
        return err;
    CASSERT(ctx->readCipher.ready == 0);
    if (ERR(err = SSLReadRecord(&rec, ctx)) != 0)
        return err;
    if (ERR(err = SSLProcessProtocolMessage(rec, ctx)) != 0)
    {   SSLFreeBuffer(&rec.contents, &ctx->sysCtx);
        return err;
    }
    if (ERR(err = SSLFreeBuffer(&rec.contents, &ctx->sysCtx)) != 0)
        return err;
        
    return SSLNoErr;
}

static SSLErr
SSLInitConnection(SSLContext *ctx)
{   SSLErr      err;
    
    if (ctx->protocolSide == SSL_ClientSide) {
        SSLChangeHdskState(ctx, HandshakeClientUninit);
    }
    else
    {   CASSERT(ctx->protocolSide == SSL_ServerSide);
        SSLChangeHdskState(ctx, HandshakeServerUninit);
    }
    
    if (ctx->peerID.data != 0)
    {   ERR(SSLGetSessionData(&ctx->resumableSession, ctx));
        /* Ignore errors; just treat as uncached session */
    }
    
	/* 
	 * If we have a cached resumable session, blow it off if it's a higher
	 * version than the max currently allowed. Note that this means that once
	 * a process negotiates a given version with a given server/port, it won't
	 * be able to negotiate a higher version. We might want to revisit this.
	 */
    if (ctx->resumableSession.data != 0) {
    
		SSLProtocolVersion savedVersion;
		
		if (ERR(err = SSLRetrieveSessionProtocolVersion(ctx->resumableSession,
				&savedVersion, ctx)) != 0) {
            return err;
		}
		if(savedVersion > ctx->maxProtocolVersion) {
			SSLLogResumSess("===Resumable session protocol mismatch\n");
			SSLFreeBuffer(&ctx->resumableSession, &ctx->sysCtx);
		} 
		else {
			SSLLogResumSess("===attempting to resume session\n");
			/*
			 * A bit of a special case for server side here. If currently 
			 * configged to allow for SSL3/TLS1 with an SSL2 hello, we 
			 * don't want to preclude the possiblity of an SSL2 hello...
			 * so we'll just leave the negProtocolVersion alone in the server case.
			 */
			if(ctx->protocolSide == SSL_ClientSide) {
				ctx->negProtocolVersion = savedVersion;
			}
		}
    }
    
/* If we're the client & handshake hasn't yet begun, start it by
 *  pretending we just received a hello request
 */
    if (ctx->state == HandshakeClientUninit && ctx->writeCipher.ready == 0)
    {   switch (ctx->negProtocolVersion)
        {   case SSL_Version_Undetermined:
            case SSL_Version_3_0_With_2_0_Hello:
            case SSL_Version_2_0:
                if (ERR(err = SSL2AdvanceHandshake(ssl2_mt_kickstart_handshake, ctx)) != 0)
                    return err;
                break;
            case SSL_Version_3_0_Only:
            case SSL_Version_3_0:
            case TLS_Version_1_0_Only:
            case TLS_Version_1_0:
                if (ERR(err = SSLAdvanceHandshake(SSL_hello_request, ctx)) != 0)
                    return err;
                break;
            default:
                sslPanic("Bad protocol version");
                break;
        }
    }
    
    return SSLNoErr;
}

static SSLErr
SSLServiceWriteQueue(SSLContext *ctx)
{   SSLErr          err = SSLNoErr, werr = SSLNoErr;
    UInt32          written = 0;
    SSLBuffer       buf, recBuf;
    WaitingRecord   *rec;

    while (!werr && ((rec = ctx->recordWriteQueue) != 0))
    {   buf.data = rec->data.data + rec->sent;
        buf.length = rec->data.length - rec->sent;
        werr = sslIoWrite(buf, &written, ctx);
        rec->sent += written;
        if (rec->sent >= rec->data.length)
        {   CASSERT(rec->sent == rec->data.length);
            CASSERT(err == 0);
            err = SSLFreeBuffer(&rec->data, &ctx->sysCtx);
            CASSERT(err == 0);
            recBuf.data = (UInt8*)rec;
            recBuf.length = sizeof(WaitingRecord);
            ctx->recordWriteQueue = rec->next;
            err = SSLFreeBuffer(&recBuf, &ctx->sysCtx);
            CASSERT(err == 0);
        }
        if (ERR(err))
            return err;
        CASSERT(ctx->recordWriteQueue == 0 || ctx->recordWriteQueue->sent == 0);
    }

    return werr;
}

#if		LOG_RX_PROTOCOL
static void sslLogRxProto(const char *msgType)
{
	printf("---received protoMsg %s\n", msgType);
}
#else
#define sslLogRxProto(msgType)
#endif	/* LOG_RX_PROTOCOL */

static SSLErr
SSLProcessProtocolMessage(SSLRecord rec, SSLContext *ctx)
{   SSLErr      err;
    
    switch (rec.contentType)
    {   case SSL_handshake:
			sslLogRxProto("SSL_handshake");
            ERR(err = SSLProcessHandshakeRecord(rec, ctx));
            break;
        case SSL_alert:
 			sslLogRxProto("SSL_alert");
            ERR(err = SSLProcessAlert(rec, ctx));
            break;
        case SSL_change_cipher_spec:
			sslLogRxProto("SSL_change_cipher_spec");
            ERR(err = SSLProcessChangeCipherSpec(rec, ctx));
            break;
        case SSL_version_2_0_record:
			sslLogRxProto("SSL_version_2_0_record");
            ERR(err = SSL2ProcessMessage(rec, ctx));
            break;
        default:
			sslLogRxProto("Bad msg");
            return ERR(SSLProtocolErr);
    }
    
    return err;
}

OSStatus
SSLClose(SSLContext *ctx)
{   
	SSLErr      err = SSLNoErr;	
    
	if(ctx == NULL) {
		return paramErr;
	}
    if (ctx->negProtocolVersion >= SSL_Version_3_0)
        ERR(err = SSLSendAlert(alert_warning, alert_close_notify, ctx));
    if (err == 0)
        ERR(err = SSLServiceWriteQueue(ctx));
    SSLChangeHdskState(ctx, SSLGracefulClose);
    if (err == SSLIOErr)
        err = SSLNoErr;     /* Ignore errors related to closed streams */
    return sslErrToOsStatus(err);
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
		CASSERT(ctx->receivedDataBuffer.length >= ctx->receivedDataPos);
		*bufSize = ctx->receivedDataBuffer.length - ctx->receivedDataPos;
	}
	return noErr;
}

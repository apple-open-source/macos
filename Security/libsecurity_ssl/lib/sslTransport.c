/*
 * Copyright (c) 1999-2001,2005-2012 Apple Inc. All Rights Reserved.
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
	const char *op,
	size_t req,
	size_t moved,
	OSStatus stat)
{
	sslLogRecordIo("===%s: req %4lu moved %4lu status %d",
		op, req, moved, (int)stat);
}
#else
#define sslIoTrace(op, req, moved, stat)
#endif	/* NDEBUG */

extern int kSplitDefaultValue;

static OSStatus SSLProcessProtocolMessage(SSLRecord *rec, SSLContext *ctx);
static OSStatus SSLHandshakeProceed(SSLContext *ctx);
static OSStatus SSLInitConnection(SSLContext *ctx);

static Boolean isFalseStartAllowed(SSLContext *ctx)
{
    SSL_CipherAlgorithm c=sslCipherSuiteGetSymmetricCipherAlgorithm(ctx->selectedCipher);
    KeyExchangeMethod kem=sslCipherSuiteGetKeyExchangeMethod(ctx->selectedCipher);
    
    
    /* Whitelisting allowed ciphers, kem and client auth type */
    return
        (
            (c==SSL_CipherAlgorithmAES_128_CBC) ||
            (c==SSL_CipherAlgorithmAES_128_GCM) ||
            (c==SSL_CipherAlgorithmAES_256_CBC) ||
            (c==SSL_CipherAlgorithmAES_256_GCM) ||
            (c==SSL_CipherAlgorithmRC4_128)
        ) && (
            (kem==SSL_ECDHE_ECDSA) ||
            (kem==SSL_ECDHE_RSA) ||
            (kem==SSL_DHE_RSA) ||
            (kem==SSL_DHE_DSS)
        ) && (
            (ctx->negAuthType==SSLClientAuthNone) ||
            (ctx->negAuthType==SSLClientAuth_DSSSign) ||
            (ctx->negAuthType==SSLClientAuth_RSASign) ||
            (ctx->negAuthType==SSLClientAuth_ECDSASign)
        );
}


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
    Boolean         split;

	sslLogRecordIo("SSLWrite top");
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
	    case SSL_HdskStateServerReady:
	    case SSL_HdskStateClientReady:
			break;
        default:
			if(ctx->state < SSL_HdskStateServerHello) {
				/* not ready for I/O, and handshake not in progress */
				sslIoTrace("SSLWrite", dataLength, 0, errSecBadReq);
				return errSecBadReq;
			}
			/* handshake in progress; will call SSLHandshakeProceed below */
			break;
	}

    /* First, we have to wait until the session is ready to send data,
        so the encryption keys and such have been established. */
    err = errSecSuccess; 
    while (!(
              (ctx->state==SSL_HdskStateServerReady) ||
              (ctx->state==SSL_HdskStateClientReady) ||
              (ctx->writeCipher_ready &&  ctx->falseStartEnabled && isFalseStartAllowed(ctx))
            ))
    {   if ((err = SSLHandshakeProceed(ctx)) != 0)
            goto exit;
    }

    /* Attempt to empty the write queue before queueing more data */
    if ((err = SSLServiceWriteQueue(ctx)) != 0)
        goto abort;

	/* Check if we should split the data into 1/n-1 to avoid known-IV issue */
	split = (ctx->oneByteRecordEnable && ctx->negProtocolVersion <= TLS_Version_1_0);
	if (split) {
		/* Only split if cipher algorithm uses CBC mode */
		SSL_CipherAlgorithm cipherAlg = sslCipherSuiteGetSymmetricCipherAlgorithm(ctx->selectedCipher);
		split = (cipherAlg > SSL_CipherAlgorithmRC4_128 &&
		         cipherAlg < SSL_CipherAlgorithmAES_128_GCM);
		if (split) {
			/* Determine whether we start splitting on second write */
			split = (kSplitDefaultValue == 2 && !ctx->wroteAppData) ? false : true;
		}
	}
    processed = 0;

    /*
	 * Fragment, package and encrypt the data and queue the resulting data
	 * for sending
	 */
    while (dataLen > 0)
    {   rec.contentType = SSL_RecordTypeAppData;
        rec.protocolVersion = ctx->negProtocolVersion;
        rec.contents.data = ((uint8_t *)data) + processed;

        if (processed == 0 && split)
            rec.contents.length = 1;
        else if (dataLen < MAX_RECORD_LENGTH)
            rec.contents.length = dataLen;
        else
            rec.contents.length = MAX_RECORD_LENGTH;
        
        if ((err = SSLWriteRecord(rec, ctx)) != 0)
            goto exit;
        processed += rec.contents.length;
        dataLen -= rec.contents.length;
        ctx->wroteAppData = 1;
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
    uint8_t         *charPtr;
    size_t          bufSize, remaining, count;
    SSLRecord       rec;

	sslLogRecordIo("SSLRead top");
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
                goto exit;
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
	sslIoTrace("SSLRead ", dataLength, *processed, err);
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

	#if SSL_ECDSA_HACK
	/* Because of Radar 6133465, we have to disable this to allow the sending
	 of client hello extensions for ECDSA... */
	ctx->versionSsl2Enable = false;
	#endif

    if(ctx->validCipherSuites == NULL) {
    	/* build list of legal cipherSpecs */
	err = sslBuildCipherSuiteArray(ctx);
    	if(err) {
    		return err;
    	}
    }
    err = errSecSuccess;

    if(ctx->isDTLS) {
        if (ctx->timeout_deadline<CFAbsoluteTimeGetCurrent()) {
            DTLSRetransmit(ctx);
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
{   OSStatus    err;
    SSLRecord   rec;

    if (ctx->signalServerAuth) {
        ctx->signalServerAuth = false;
        return errSSLServerAuthCompleted;
    }
    if (ctx->signalCertRequest) {
        ctx->signalCertRequest = false;
        return errSSLClientCertRequested;
    }
    if (ctx->signalClientAuth) {
        ctx->signalClientAuth = false;
        return errSSLClientAuthCompleted;
    }

    if (ctx->state == SSL_HdskStateUninit)
        if ((err = SSLInitConnection(ctx)) != 0)
            return err;

	/* This is our cue that we dropped out of the handshake
	 * to let the client pick an identity, move state back
	 * to server hello done and continue processing.
	 */
	if ((ctx->protocolSide == kSSLClientSide) &&
		(ctx->state == SSL_HdskStateClientCert))
		if ((err = SSLAdvanceHandshake(SSL_HdskServerHelloDone, ctx)) != 0)
			return err;

    if ((err = SSLServiceWriteQueue(ctx)) != 0)
        return err;

    assert(ctx->readCipher_ready == 0);
    if ((err = SSLReadRecord(&rec, ctx)) != 0)
        return err;
    if ((err = SSLProcessProtocolMessage(&rec, ctx)) != 0)
    {
        SSLFreeRecord(rec, ctx);
        return err;
    }
    if ((err = SSLFreeRecord(rec, ctx)))
        return err;

    return errSecSuccess;
}

static OSStatus
SSLInitConnection(SSLContext *ctx)
{   OSStatus      err = errSecSuccess;

    if (ctx->protocolSide == kSSLClientSide) {
        SSLChangeHdskState(ctx, SSL_HdskStateClientUninit);
    }
    else
    {   assert(ctx->protocolSide == kSSLServerSide);
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
	Boolean cachedV3OrTls1 = ctx->isDTLS;

    if (ctx->resumableSession.data != 0) {
        SSLProtocolVersion savedVersion;
		if ((err = SSLRetrieveSessionProtocolVersion(ctx->resumableSession,
                                                     &savedVersion, ctx)) != 0)
            return err;

            if (ctx->isDTLS
                ? (savedVersion <= ctx->minProtocolVersion &&
                   savedVersion >= ctx->maxProtocolVersion)
                : (savedVersion >= ctx->minProtocolVersion &&
                   savedVersion <= ctx->maxProtocolVersion)) {
            cachedV3OrTls1 = savedVersion != SSL_Version_2_0;
			sslLogResumSessDebug("===attempting to resume session");
		} else {
			sslLogResumSessDebug("===Resumable session protocol mismatch");
			SSLFreeBuffer(&ctx->resumableSession);
		}
    }

	/*
	 * If we're the client & handshake hasn't yet begun, start it by
	 *  pretending we just received a hello request
	 */
    if (ctx->state == SSL_HdskStateClientUninit && ctx->writeCipher_ready == 0)
    {   
		assert(ctx->negProtocolVersion == SSL_Version_Undetermined);
#if ENABLE_SSLV2
		if(!cachedV3OrTls1) {
			/* SSL2 client hello with possible upgrade */
			err = SSL2AdvanceHandshake(SSL2_MsgKickstart, ctx);
		}
		else
#endif
        {
			err = SSLAdvanceHandshake(SSL_HdskHelloRequest, ctx);
		}
    }

    return err;
}


static OSStatus
SSLProcessProtocolMessage(SSLRecord *rec, SSLContext *ctx)
{   OSStatus      err;

    switch (rec->contentType)
    {   case SSL_RecordTypeHandshake:
			sslLogRxProtocolDebug("Handshake");
            if(ctx->isDTLS)
                err = DTLSProcessHandshakeRecord(*rec, ctx);
            else
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
#if ENABLE_SSLV2
        case SSL_RecordTypeV2_0:
			sslLogRxProtocolDebug("RecordTypeV2_0");
            err = SSL2ProcessMessage(rec, ctx);
            break;
#endif
        default:
			sslLogRxProtocolDebug("Bad msg");
            return errSSLProtocol;
    }

    return err;
}

OSStatus
SSLClose(SSLContext *ctx)
{
	OSStatus      err = errSecSuccess;

	sslHdskStateDebug("SSLClose");
	if(ctx == NULL) {
		return errSecParam;
	}
    if (ctx->negProtocolVersion >= SSL_Version_3_0)
        err = SSLSendAlert(SSL_AlertLevelWarning, SSL_AlertCloseNotify, ctx);

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

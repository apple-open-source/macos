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
#include "sslDebug.h"
#include "sslCipherSpecs.h"

#include <assert.h>
#include <string.h>

#include <utilities/SecIOFormat.h>
#include <utilities/SecCFWrappers.h>

#include <CommonCrypto/CommonDigest.h>
#include <Security/SecCertificatePriv.h>

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
        case errSSLClientHelloReceived:
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
    while (ctx->readCipher_ready == 0) {
		if ((err = SSLHandshakeProceed(ctx)) != 0) {
            goto exit;
		}
    }

    /* Need this to handle the case were SSLRead returned
       errSSLClientHelloReceived as readCipher_ready is not set yet in that case */
    if ((err = tls_handshake_continue(ctx->hdsk)) != 0)
        return err;

    /* Attempt to service the write queue */
    if ((err = SSLServiceWriteQueue(ctx)) != 0) {
		if (err != errSSLWouldBlock) {
            goto exit;
		}
    }

    remaining = bufSize;
    charPtr = (uint8_t *)data;

    /* If we have data in the buffer, use that first */
    if (ctx->receivedDataBuffer.data)
    {
        count = ctx->receivedDataBuffer.length - ctx->receivedDataPos;
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
    {
        SSLFreeBuffer(&ctx->receivedDataBuffer);
        ctx->receivedDataBuffer.data = 0;
        ctx->receivedDataPos = 0;
    }

	/*
     * If we didnt fill up the users buffer, get some more data
	 */
    if (remaining > 0 && ctx->state != SSL_HdskStateGracefulClose)
    {
        assert(ctx->receivedDataBuffer.data == 0);
        if ((err = SSLReadRecord(&rec, ctx)) != 0) {
            goto exit;
        }
        if (rec.contentType == SSL_RecordTypeAppData ||
            rec.contentType == SSL_RecordTypeV2_0)
        {
            if (rec.contents.length <= remaining)
            {   /* Copy all we got in the user's buffer */
                memcpy(charPtr, rec.contents.data, rec.contents.length);
                *processed += rec.contents.length;
                {
                    if ((err = SSLFreeRecord(rec, ctx))) {
                    	goto exit;
                    }
                }
            }
            else
            {   /* Copy what we can in the user's buffer, keep the rest for next SSLRead. */
                memcpy(charPtr, rec.contents.data, remaining);
                *processed += remaining;
                ctx->receivedDataBuffer = rec.contents;
                ctx->receivedDataPos = remaining;
            }
        }
        else {
            if ((err = SSLProcessProtocolMessage(&rec, ctx)) != 0) {
                /* This may not make much sense, but this is required so that we
                 process the write queue. This replicate exactly the behavior 
                 before the coreTLS adoption */
                if(err == errSSLClosedGraceful) {
                    SSLClose(ctx);
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
        case errSSLClientHelloReceived:
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



static void get_extended_peer_id(SSLContext *ctx, tls_buffer *extended_peer_id)
{
    uint8_t md[CC_SHA256_DIGEST_LENGTH];
    __block CC_SHA256_CTX hash_ctx;

    CC_SHA256_Init(&hash_ctx);

    CC_SHA256_Update(&hash_ctx, &ctx->allowAnyRoot, sizeof(ctx->allowAnyRoot));

#if !TARGET_OS_IPHONE
    if(ctx->trustedLeafCerts) {
        CFArrayForEach(ctx->trustedLeafCerts, ^(const void *value) {
            SecCertificateRef cert = (SecCertificateRef) value;
            CC_SHA256_Update(&hash_ctx, SecCertificateGetBytePtr(cert), (CC_LONG)SecCertificateGetLength(cert));
        });
    }
#endif

    CC_SHA256_Update(&hash_ctx, &ctx->trustedCertsOnly, sizeof(ctx->trustedCertsOnly));


    if(ctx->trustedCerts) {
        CFArrayForEach(ctx->trustedCerts, ^(const void *value) {
            SecCertificateRef cert = (SecCertificateRef) value;
            CC_SHA256_Update(&hash_ctx, SecCertificateGetBytePtr(cert), (CC_LONG)SecCertificateGetLength(cert));
        });
    }

    CC_SHA256_Final(md, &hash_ctx);

    extended_peer_id->length = ctx->peerID.length + sizeof(md);
    extended_peer_id->data = sslMalloc(extended_peer_id->length);
    memcpy(extended_peer_id->data, ctx->peerID.data, ctx->peerID.length);
    memcpy(extended_peer_id->data+ctx->peerID.length, md, sizeof(md));
}

/* Send the initial client hello */
static OSStatus
SSLHandshakeStart(SSLContext *ctx)
{
    int err;
    tls_buffer extended_peer_id;
    get_extended_peer_id(ctx, &extended_peer_id);
    err = tls_handshake_negotiate(ctx->hdsk, &extended_peer_id);
    free(extended_peer_id.data);
    if(err)
        return err;

    ctx->readCipher_ready = 0;
    ctx->writeCipher_ready = 0;
    SSLChangeHdskState(ctx, SSL_HdskStatePending);

    return noErr;
}

OSStatus
SSLReHandshake(SSLContext *ctx)
{
    if(ctx == NULL) {
        return errSecParam;
    }

    if (ctx->state == SSL_HdskStateGracefulClose)
        return errSSLClosedGraceful;
    if (ctx->state == SSL_HdskStateErrorClose)
        return errSSLClosedAbort;
    if (ctx->state == SSL_HdskStatePending)
        return errSecBadReq;

    /* If we are the client, we start the negotiation */
    if(ctx->protocolSide == kSSLClientSide) {
        return SSLHandshakeStart(ctx);
    } else {
        return tls_handshake_request_renegotiation(ctx->hdsk);
    }
}

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

    /* Initial Client Hello */
    if(ctx->state==SSL_HdskStateUninit) {
        /* If we are the client, we start the negotiation */
        if(ctx->protocolSide == kSSLClientSide) {
            err = SSLHandshakeStart(ctx);
            if(err) {
                return err;
            }
        }
        SSLChangeHdskState(ctx, SSL_HdskStatePending);
    }

    do {
        err = SSLHandshakeProceed(ctx);
        if((err != 0) && (err != errSSLUnexpectedRecord))
            return err;
    } while (ctx->readCipher_ready == 0 || ctx->writeCipher_ready == 0);

	/* one more flush at completion of successful handshake */
    if ((err = SSLServiceWriteQueue(ctx)) != 0) {
		return err;
	}

    return errSecSuccess;
}

#if (TARGET_OS_IPHONE && !TARGET_IPHONE_SIMULATOR)

#include "SecADWrapper.h"

static void ad_log_SecureTransport_early_fail(long signature)
{
    CFStringRef key = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("com.apple.SecureTransport.early_fail.%ld"), signature);

    if (key) {
        SecADAddValueForScalarKey(key, 1);
        CFRelease(key);
    }
}

#endif


#if (!TARGET_OS_IPHONE && !TARGET_OS_SIMULATOR)

#include <msgtracer_client.h>

static void mt_log_SecureTransport_early_fail(long signature)
{
    char signature_string[16];

    snprintf(signature_string, sizeof(signature_string), "%ld", signature);

    msgtracer_log_with_keys("com.apple.SecureTransport.early_fail", ASL_LEVEL_NOTICE,
                            "com.apple.message.signature", signature_string,
                            "com.apple.message.summarize", "YES",
                            NULL);
}

#endif


static void log_SecureTransport_early_fail(long signature)
{
#if (!TARGET_OS_IPHONE && !TARGET_OS_SIMULATOR)
    mt_log_SecureTransport_early_fail(signature);
#endif

#if (TARGET_OS_IPHONE && !TARGET_OS_SIMULATOR)
    ad_log_SecureTransport_early_fail(signature);
#endif
}


static OSStatus
SSLHandshakeProceed(SSLContext *ctx)
{
    OSStatus  err;

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

    if(ctx->protocolSide == kSSLClientSide &&
       ctx->dheEnabled == false &&
       !ctx->serverHelloReceived &&
       err && err != errSSLWouldBlock)
    {
        log_SecureTransport_early_fail(err);
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

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
 * sslHandshake.c - SSL 3.0 handshake state machine.
 */

#include <tls_ciphersuites.h>
#include "tls_handshake_priv.h"
#include "tls_metrics.h"

#include "sslHandshake.h"
#include "sslMemory.h"
#include "sslAlertMessage.h"
#include "sslSession.h"
#include "sslUtils.h"
#include "sslDebug.h"
#include "sslCrypto.h"
#include "sslDigests.h"
#include "sslCipherSpecs.h"


#include <AssertMacros.h>
#include <string.h>
#include <assert.h>
#include <inttypes.h>

#define REQUEST_CERT_CORRECT        0

#if __LP64__
#define PRIstatus "d"
#else
#define PRIstatus "ld"
#endif


uint8_t *
SSLEncodeHandshakeHeader(tls_handshake_t ctx, tls_buffer *rec, SSLHandshakeType type, size_t msglen)
{
    uint8_t *charPtr;

    charPtr = rec->data;
    *charPtr++ = type;
    charPtr = SSLEncodeSize(charPtr, msglen, 3);

    if(ctx->isDTLS) {
        charPtr = SSLEncodeInt(charPtr, ctx->hdskMessageSeq, 2);
        /* fragmentation -- we encode header as if unfragmented,
         actual fragmentation happens at lower layer. */
        charPtr = SSLEncodeInt(charPtr, 0, 3);
        charPtr = SSLEncodeSize(charPtr, msglen, 3);
    }

    return charPtr;
}

static int SSLProcessHandshakeMessage(SSLHandshakeMsg message, tls_handshake_t ctx);

static int
SSLUpdateHandshakeMacs(const tls_buffer *messageData, tls_handshake_t ctx)
{
    int err = errSSLInternal;
    bool do_md5 = false;
    bool do_sha1 = false;
    bool do_sha256 = false;
    bool do_sha384 = false;
    bool do_sha512 = false;

    //TODO: We can stop updating the unecessary hashes once the CertVerify message is processed in case where we do Client Side Auth.

    if(ctx->negProtocolVersion == tls_protocol_version_Undertermined)
    {
        // we dont know yet, so we might need MD5 & SHA1 - Server should always call in with known protocol version.
        assert(!ctx->isServer);
        do_md5 = do_sha1 = true;
        if(ctx->isDTLS
           ? ctx->maxProtocolVersion < tls_protocol_version_DTLS_1_0
           : ctx->maxProtocolVersion >= tls_protocol_version_TLS_1_2)
        {
            // We wil need those too, unless we are sure we wont end up doing TLS 1.2
            do_sha256 = do_sha384 = do_sha512 = true;
        }
    } else {
        // we know which version we use at this point
        if(sslVersionIsLikeTls12(ctx)) {
            do_sha1 = do_sha256 = do_sha384 = do_sha512 = true;
        } else {
            do_md5 = do_sha1 = true;
        }
    }

    if (do_md5 &&
        (err = SSLHashMD5.update(&ctx->md5State, messageData)) != 0)
        goto done;
    if (do_sha1 &&
        (err = SSLHashSHA1.update(&ctx->shaState, messageData)) != 0)
        goto done;
    if (do_sha256 &&
        (err = SSLHashSHA256.update(&ctx->sha256State, messageData)) != 0)
        goto done;
    if (do_sha384 &&
        (err = SSLHashSHA384.update(&ctx->sha384State, messageData)) != 0)
        goto done;
    if (do_sha512 &&
        (err = SSLHashSHA512.update(&ctx->sha512State, messageData)) != 0)
        goto done;

    sslLogNegotiateDebug("%s protocol: %02X max: %02X cipher: %02X%s%s%s%s",
                         ctx->isServer ? "server" : "client",
                         ctx->negProtocolVersion,
                         ctx->maxProtocolVersion,
                         ctx->selectedCipher,
                         do_md5 ? " md5" : "",
                         do_sha1 ? " sha1" : "",
                         do_sha256 ? " sha256" : "",
                         do_sha384 ? " sha384" : "",
                         do_sha512 ? " sha512" : "");
done:
    return err;
}


#define SSL2_RECORD_HEADER_SIZE 3
int
SSLProcessSSL2Message(tls_buffer rec, tls_handshake_t ctx)
{
    int err;

    uint16_t msg_length;
    uint8_t msg_type;

    err = errSSLParam;

    require(rec.length>=SSL2_RECORD_HEADER_SIZE, fail);

    msg_length = SSLDecodeInt(rec.data, 2);
    msg_type = rec.data[2];


    // msg_length, First bit must be 1, must match record length.
    require(msg_length&0x8000, fail);
    require(((msg_length&0x7FFF)+2)==rec.length, fail);

    // msg_type must be 1 for Client Hello - anything else throw away
    require(msg_type==1, fail);

    tls_buffer message;
    message.data = rec.data+3;
    message.length = rec.length-3;

    require_noerr((err=SSLProcessSSL2ClientHello(message, ctx)), fail);

    /* The msg_length field is not hashed */
    tls_buffer hashedData;
    hashedData.data = rec.data+2;
    hashedData.length = rec.length-2;

    if ((err = SSLUpdateHandshakeMacs(&hashedData, ctx)) != 0) {
        SSLFatalSessionAlert(SSL_AlertInternalError, ctx);
        return err;
    }

    require_noerr((err = SSLAdvanceHandshake(SSL_HdskClientHello, ctx)), fail);

    return errSSLSuccess;
fail:
    return err;

}

int
SSLProcessHandshakeRecordInner(tls_buffer rec, tls_handshake_t ctx)
{
    int             err;
    const size_t    head = 4;
    uint8_t         *p;
    size_t          remaining;
    SSLHandshakeMsg message = {};
    tls_buffer       messageData;
    int             event_err = 0;

    p = rec.data;
    remaining = rec.length;

    while (remaining > 0)
    {
        if (remaining < head)
            break;  /* we must have at least a header */

        messageData.data = p;
        message.type = (SSLHandshakeType)p[0];
        message.contents.length = SSLDecodeSize(p+1, 3);

        if ((message.contents.length + head) > remaining)
            break;

        p += head;
        message.contents.data = p;
        p += message.contents.length;
        messageData.length = head + message.contents.length;
        assert(p == messageData.data + messageData.length);

        /* message fragmentation */
        remaining -= messageData.length;
        if ((err = SSLProcessHandshakeMessage(message, ctx)) != 0)
            return err;

        if (message.type != SSL_HdskHelloRequest)

        {   if ((err = SSLUpdateHandshakeMacs(&messageData, ctx)) != 0)
        {   SSLFatalSessionAlert(SSL_AlertInternalError, ctx);
            return err;
        }
        }

        /* If message callback failed, don't call Advance Handshake, but save the remaing data */
        /* when calling continue, we will first call SSLAdvanceHandshake, then process the remainder of the cache. */
        ctx->advanceHandshake = true;
        ctx->currentMessage = message.type;
        event_err = ctx->callbacks->message(ctx->callback_ctx, message.type);
        if(event_err)
            break;

        if ((err = SSLAdvanceHandshake(message.type, ctx)) != 0)
            return err;
    }

    if (remaining > 0)      /* Fragmented handshake message or event pending */
    {   /* If there isn't a cache, allocate one */
        if (ctx->fragmentedMessageCache.data == 0)
        {   if ((err = SSLAllocBuffer(&ctx->fragmentedMessageCache, remaining)))
        {   SSLFatalSessionAlert(SSL_AlertInternalError, ctx);
            return err;
        }
        }
        /* Copy the remainder at the start of the message cache, unless nothing was processed */
        if(p!=ctx->fragmentedMessageCache.data)
        {
            /* possibly overlapping, use memmove not memcpy */
            memmove(ctx->fragmentedMessageCache.data, p, remaining);
            ctx->fragmentedMessageCache.length = remaining;
        }
    }
    else if (ctx->fragmentedMessageCache.data != 0)
    {   if ((err = SSLFreeBuffer(&ctx->fragmentedMessageCache)))
    {   SSLFatalSessionAlert(SSL_AlertInternalError, ctx);
        return err;
    }
    }

    /* If a callback returned an error, we return it, unless another error happened */
    return event_err;
}

/* TODO: there is probably some room to refactor DTLSProcessHandshakeRecordInner amd SSLProcessHandshakeRecordInner */
static int
DTLSProcessHandshakeRecordInner(tls_buffer rec, tls_handshake_t ctx)
{
    int             err = errSSLParam;
    size_t          remaining;
    UInt8           *p;
    int             event_err = 0;

    const UInt32    head = 12;

    assert(ctx->isDTLS);

    remaining = rec.length;
    p = rec.data;

    while (remaining > 0)
    {
        UInt8 msgtype;
        UInt32 msglen;
        UInt32 msgseq;
        UInt32 fraglen;
        UInt32 fragofs;

        if (remaining < head) {
            /* flush it - record is too small */
            sslErrorLog("DTLSProcessHandshakeRecord: remaining too small (%lu out of %lu)\n", remaining, rec.length);
            assert(0); // keep this assert until we find a test case that triggers it
            err = errSSLUnexpectedRecord;
            goto flushit;
        }

        /* Thats the 12 bytes of header : */
        msgtype = (SSLHandshakeType)*p++;
        msglen = SSLDecodeInt(p, 3); p+=3;
        msgseq = SSLDecodeInt(p, 2); p+=2;
        fragofs = SSLDecodeInt(p, 3); p+=3;
        fraglen = SSLDecodeInt(p, 3); p+=3;

        remaining -= head;

        SSLLogHdskMsg(msgtype, 0);
        sslHdskMsgDebug("DTLS Hdsk Record: type=%u, len=%u, seq=%u (%u), f_ofs=%u, f_len=%u, remaining=%u",
                             msgtype, (int)msglen, (int)msgseq, (int)ctx->hdskMessageSeqNext, (int)fragofs, (int)fraglen, (int)remaining);

        if(
           ((fraglen+fragofs) > msglen)
           || (fraglen > remaining)
           || (msgseq!=ctx->hdskMessageSeqNext)
           || (fragofs!=ctx->hdskMessageCurrentOfs)
           || (fragofs && (msgtype!=ctx->hdskMessageCurrent.type))
           || (fragofs && (msglen != ctx->hdskMessageCurrent.contents.length))
           )
        {
            sslErrorLog("DTLSProcessHandshakeRecord: wrong fragment (fl=%d, fo=%d, ml=%d, rm=%d | ms=%d/%d | mt=%d/%d, ml=%d/%d\n",
                        fraglen, fragofs, msglen, remaining, msgtype, ctx->hdskMessageCurrent.type, msglen, ctx->hdskMessageCurrent.contents.length);
            // assert(0); // keep this assert until we find a test case that triggers it
            // This is a recoverable error, we just drop this fragment.
            // TODO: this should probably trigger a retransmit
            err = errSSLUnexpectedRecord;
            goto flushit;
        }

        /* First fragment - allocate */
        if(fragofs==0) {
            sslHdskMsgDebug("Allocating hdsk buf for msg type %d", msgtype);
            assert(ctx->hdskMessageCurrent.contents.data==NULL);
            assert(ctx->hdskMessageCurrent.contents.length==0);
            if((err=SSLAllocBuffer(&(ctx->hdskMessageCurrent.contents), msglen))) {
                SSLFatalSessionAlert(SSL_AlertInternalError, ctx);
                return err;
            }
            ctx->hdskMessageCurrent.type = msgtype;
        }

        /* We have the next fragment, lets save it */
        memcpy(ctx->hdskMessageCurrent.contents.data + ctx->hdskMessageCurrentOfs, p, fraglen);
        ctx->hdskMessageCurrentOfs+=fraglen;
        p+=fraglen;
        remaining-=fraglen;

        /* This was the last fragment, lets process the message */
        if(ctx->hdskMessageCurrentOfs == ctx->hdskMessageCurrent.contents.length) {
            err = SSLProcessHandshakeMessage(ctx->hdskMessageCurrent, ctx);
            if(err)
                goto flushit;

            if ((msgtype != SSL_HdskHelloRequest) && (msgtype != SSL_HdskHelloVerifyRequest))
            {
                /* We need to hash a fake header as if no fragmentation */
                uint8_t pseudo_header[head];
                tls_buffer header;
                header.data=pseudo_header;
                header.length=head;

                pseudo_header[0]=msgtype;
                SSLEncodeInt(pseudo_header+1, msglen, 3);
                SSLEncodeInt(pseudo_header+4, msgseq, 2);
                SSLEncodeInt(pseudo_header+6, 0, 3);
                SSLEncodeInt(pseudo_header+9, msglen, 3);

                if ((err = SSLHashSHA1.update(&ctx->shaState, &header)) != 0 ||
                    (err = SSLHashMD5.update(&ctx->md5State, &header)) != 0)
                {
                    SSLFatalSessionAlert(SSL_AlertInternalError, ctx);
                    goto flushit;
                }

                tls_buffer *messageData=&ctx->hdskMessageCurrent.contents;
                if ((err = SSLHashSHA1.update(&ctx->shaState, messageData)) != 0 ||
                    (err = SSLHashMD5.update(&ctx->md5State, messageData)) != 0)
                {
                    SSLFatalSessionAlert(SSL_AlertInternalError, ctx);
                    goto flushit;
                }

                sslHdskMsgDebug("Hashing %d bytes of msg seq %d\n", (int)messageData->length, (int)msgseq);
            }

            sslHdskMsgDebug("processed message of type %d", msgtype);

            /* Free the buffer for current message, and reset offset */
            SSLFreeBuffer(&(ctx->hdskMessageCurrent.contents));
            ctx->hdskMessageCurrentOfs=0;

            /* If we successfully processed a message, we wait for the next one */
            ctx->hdskMessageSeqNext++;


            /* If message callback failed, don't call Advance Handshake, but save the remaing data */
            /* when calling continue, we will first call SSLAdvanceHandshake, then process the remainder of the cache. */
            ctx->advanceHandshake = true;
            ctx->currentMessage = msgtype;
            event_err = ctx->callbacks->message(ctx->callback_ctx, msgtype);
            if(event_err)
                break;

            if ((err = SSLAdvanceHandshake(msgtype, ctx)) != 0)
            {
                sslErrorLog("AdvanceHandshake error: %d\n", err);
                goto flushit;
            }

        }

        sslHdskMsgDebug("remaining = %ld", remaining);
    }

    if (remaining > 0)      /* Fragmented handshake message or event pending */
    {   /* If there isn't a cache, allocate one */
        if (ctx->fragmentedMessageCache.data == 0)
        {   if ((err = SSLAllocBuffer(&ctx->fragmentedMessageCache, remaining)))
        {   SSLFatalSessionAlert(SSL_AlertInternalError, ctx);
            return err;
        }
        }
        /* Copy the remainder at the start of the message cache, unless nothing was processed */
        if(p!=ctx->fragmentedMessageCache.data)
        {
            /* possibly overlapping, use memmove not memcpy */
            memmove(ctx->fragmentedMessageCache.data, p, remaining);
            ctx->fragmentedMessageCache.length = remaining;
        }
    }
    else if (ctx->fragmentedMessageCache.data != 0)
    {   if ((err = SSLFreeBuffer(&ctx->fragmentedMessageCache)))
    {   SSLFatalSessionAlert(SSL_AlertInternalError, ctx);
        return err;
    }
    }

    return event_err;

flushit:
    sslErrorLog("DTLSProcessHandshakeRecord: flushing record (err=%"PRIstatus")\n", err);

    /* This will flush the current handshake message */
    SSLFreeBuffer(&(ctx->hdskMessageCurrent.contents));
    ctx->hdskMessageCurrentOfs=0;

    return err;
}



int
SSLProcessHandshakeRecord(tls_buffer rec, tls_handshake_t ctx)
{
    int             err;

    /* This code is a bit complicated.
     There is a message cache that is allocated and used only if needed. It is needed in several situations:
     - The record contains a incomplete message
     - The record contains several messages and we need to break out to the client between messages.
     If there is already a message cache, we append the current record before processing the message cache.
     Otherwise we just process the current record without a copy.
     */

    if (ctx->fragmentedMessageCache.data != 0)
    {
		size_t origLen = ctx->fragmentedMessageCache.length;
		if ((err = SSLReallocBuffer(&ctx->fragmentedMessageCache,
                                    ctx->fragmentedMessageCache.length + rec.length)) != 0)
        {   SSLFatalSessionAlert(SSL_AlertInternalError, ctx);
            return err;
        }
        memcpy(ctx->fragmentedMessageCache.data + origLen,
               rec.data, rec.length);

        rec = ctx->fragmentedMessageCache;
    }

    if(ctx->isDTLS)
        return DTLSProcessHandshakeRecordInner(rec, ctx);
    else
        return SSLProcessHandshakeRecordInner(rec, ctx);
}

int
DTLSRetransmit(tls_handshake_t ctx)
{
    sslHdskMsgDebug("DTLSRetransmit in state %s. Last Sent = %d, Last Recv=%d, attempt=%f\n",
                hdskStateToStr(ctx->state), ctx->hdskMessageSeq, ctx->hdskMessageSeqNext, ctx->hdskMessageRetryCount);

    /* go back to previous cipher if retransmitting a flight including changecipherspec */
    if(ctx->messageQueueContainsChangeCipherSpec) {
        int err;
        err = ctx->callbacks->rollback_write_cipher(ctx->callback_ctx);
        if(err)
            return err;
    }

    /* Lets resend the last flight */
    return SSLSendFlight(ctx);
}

static int
SSLProcessHandshakeMessage(SSLHandshakeMsg message, tls_handshake_t ctx)
{   int      err;

    err = errSSLSuccess;
    SSLLogHdskMsg(message.type, 0);
    switch (message.type)
    {   case SSL_HdskHelloRequest:
            if (ctx->isServer)
                goto wrongMessage;
            if (message.contents.length > 0)
                err = errSSLProtocol;
            break;
        case SSL_HdskClientHello:
            if ((ctx->state != SSL_HdskStateServerUninit) && (ctx->state != SSL_HdskStateServerReady))
                goto wrongMessage;
            if ((ctx->state == SSL_HdskStateServerReady) && !ctx->allowRenegotiation) {
                err = SSLSendAlert(SSL_AlertLevelWarning, SSL_AlertNoRenegotiation, ctx);
            } else {
                err = SSLProcessClientHello(message.contents, ctx);
            }
            break;
        case SSL_HdskServerHello:
            if (ctx->state != SSL_HdskStateServerHello)
                goto wrongMessage;
            err = SSLProcessServerHello(message.contents, ctx);
            break;
#if ENABLE_DTLS
        case SSL_HdskHelloVerifyRequest:
            if (ctx->isServer)
                goto wrongMessage;
            if(ctx->state != SSL_HdskStateServerHello)
                goto wrongMessage;
            /* TODO: Do we need to check the client state here ? */
            err = SSLProcessServerHelloVerifyRequest(message.contents, ctx);
            break;
#endif
        case SSL_HdskCert:
            if (ctx->state != SSL_HdskStateCert &&
                ctx->state != SSL_HdskStateClientCert)
                goto wrongMessage;
			err = SSLProcessCertificate(message.contents, ctx);
			/*
			 * Note that cert evaluation can now be performed asynchronously,
			 * so SSLProcessCertificate may return errSSLWouldBlock here.
			 */
            break;
        case SSL_HdskCertificateStatus:
            if ((ctx->state != SSL_HdskStateHelloDone) &&
                (ctx->state != SSL_HdskStateKeyExchange))
                goto wrongMessage;
            err = SSLProcessCertificateStatus(message.contents, ctx);
            break;
        case SSL_HdskServerKeyExchange:
            /*
             * Since this message is optional for some key exchange
             * mechanisms, and completely at the server's discretion,
             * we need to be able to handle this in one of two states...
             */
            switch(ctx->state) {
                case SSL_HdskStateKeyExchange:	/* explicitly waiting for this */
                case SSL_HdskStateHelloDone:
                    break;
                default:
                    goto wrongMessage;
            }
            err = SSLProcessServerKeyExchange(message.contents, ctx);
            break;
        case SSL_HdskCertRequest:
            if ((ctx->state != SSL_HdskStateHelloDone) || ctx->certRequested)
                goto wrongMessage;
            err = SSLProcessCertificateRequest(message.contents, ctx);
            break;
        case SSL_HdskServerHelloDone:
            if (ctx->state != SSL_HdskStateHelloDone)
                goto wrongMessage;
            err = SSLProcessServerHelloDone(message.contents, ctx);
            break;
        case SSL_HdskCertVerify:
            if (ctx->state != SSL_HdskStateClientCertVerify)
                goto wrongMessage;
            err = SSLProcessCertificateVerify(message.contents, ctx);
			assert(ctx->isServer);
			if(err) {
				ctx->clientCertState = kSSLClientCertRejected;
			}
            break;
        case SSL_HdskClientKeyExchange:
            if (ctx->state != SSL_HdskStateClientKeyExchange)
                goto wrongMessage;
            err = SSLProcessKeyExchange(message.contents, ctx);
            break;
        case SSL_HdskNPNEncryptedExtension:
            if (ctx->state != SSL_HdskStateFinished)
                goto wrongMessage;
            err = SSLProcessEncryptedExtension(message.contents, ctx);
            break;
        case SSL_HdskFinished:
            if (ctx->state != SSL_HdskStateFinished)
                goto wrongMessage;
            err = SSLProcessFinished(message.contents, ctx);
            break;
        case SSL_HdskNewSessionTicket:
            if(ctx->state != SSL_HdskStateNewSessionTicket)
                goto wrongMessage;
            err = SSLProcessNewSessionTicket(message.contents, ctx);
            break;
        default:
            goto wrongMessage;
            break;
    }

    if (err && !ctx->sentFatalAlert)
    {   if (err == errSSLProtocol)
            SSLFatalSessionAlert(SSL_AlertIllegalParam, ctx);
        else if (err == errSSLNegotiation)
            SSLFatalSessionAlert(SSL_AlertHandshakeFail, ctx);
        else if (err == errSSLInternal)
            SSLFatalSessionAlert(SSL_AlertInternalError, ctx);
        else if (err != errSSLWouldBlock &&
				 err != errSSLPeerAuthCompleted /* == errSSLClientAuthCompleted */ &&
				 err != errSSLClientCertRequested)
            SSLFatalSessionAlert(SSL_AlertCloseNotify, ctx); 
    }
    return err;

wrongMessage:
    SSLFatalSessionAlert(SSL_AlertUnexpectedMsg, ctx);
    return errSSLProtocol;
}

/*
 * Given a server-side SSLContext that's fully restored for a resumed session,
 * queue up the remaining outgoing messages to finish the handshake.
 */
static int
SSLResumeServerSide(
	tls_handshake_t ctx)
{
	int err;
	if ((err = SSLPrepareAndQueueMessage(SSLEncodeServerHello, tls_record_type_Handshake, ctx)) != 0)
		return err;
	if ((err = SSLInitPendingCiphers(ctx)) != 0)
	{   SSLFatalSessionAlert(SSL_AlertInternalError, ctx);
		return err;
	}
	if ((err = SSLPrepareAndQueueMessage(SSLEncodeChangeCipherSpec, tls_record_type_ChangeCipher,
				ctx)) != 0)
		return err;

	if ((err = SSLPrepareAndQueueMessage(SSLEncodeFinishedMessage, tls_record_type_Handshake,
			ctx)) != 0)
		return err;

	SSLChangeHdskState(ctx, SSL_HdskStateChangeCipherSpec);

	return errSSLSuccess;

}

int
SSLAdvanceHandshake(SSLHandshakeType processed, tls_handshake_t ctx)
{   int        err;
    // tls_buffer       sessionIdentifier;

    SSLResetFlight(ctx);

    ctx->advanceHandshake = false;

    switch (processed)
    {
        case SSL_HdskHelloRequest:
            if((ctx->state != SSL_HdskStateClientUninit) && (ctx->state != SSL_HdskStateClientReady)) {
                return 0; /* Ignore this */
            }
            /* Just fall through */

#if ENABLE_DTLS
        case SSL_HdskHelloVerifyRequest:
#endif
			/*
			 * Reset the client auth state machine in case this is
			 * a renegotiation.
			 */
			ctx->certRequested = 0;
			ctx->certSent = 0;
			ctx->certReceived = 0;
			ctx->x509Requested = 0;
			ctx->clientCertState = kSSLClientCertNone;
            sslReadReady(ctx, false);
            sslWriteReady(ctx, false);
            if ((err = SSLPrepareAndQueueMessage(SSLEncodeClientHello, tls_record_type_Handshake, ctx)) != 0)
                return err;
            SSLChangeHdskState(ctx, SSL_HdskStateServerHello);
            break;
        case SSL_HdskClientHello:
            assert(ctx->isServer);
            /* Reset the client auth state in case this is a renegotiation - Required ?*/
            ctx->sessionMatch = false;
            ctx->certRequested = 0;
			ctx->certSent = 0;
			ctx->certReceived = 0;
			ctx->x509Requested = 0;
			ctx->clientCertState = kSSLClientCertNone;
            sslReadReady(ctx, false);
            sslWriteReady(ctx, false);

            if(ctx->tls_fallback_scsv && (ctx->isDTLS
                                          ? ctx->maxProtocolVersion < ctx->clientReqProtocol
                                          : ctx->maxProtocolVersion > ctx->clientReqProtocol))
            {
                SSLFatalSessionAlert(SSL_AlertInappropriateFallback, ctx);
                return errSSLPeerProtocolVersion;
            }

            if((ctx->isDTLS) && (ctx->cookieVerified==false))
            {   /* Send Hello Verify Request */
                if((err=SSLPrepareAndQueueMessage(SSLEncodeServerHelloVerifyRequest, tls_record_type_Handshake, ctx)) !=0 )
                    return err;
                break;
            }

            if (ctx->proposedSessionID.data != 0)
            /* If proposed session ID != 0, client is trying to resume */
            {
                tls_buffer resumableSession;

                sslLogResumSessDebug("Trying to RESUME TLS server-side session ID=%02x...", ctx->proposedSessionID.data[0]);

                err = ctx->callbacks->load_session_data(ctx->callback_ctx, ctx->proposedSessionID, &resumableSession);

                if (!err)
                {
                    sslLogResumSessDebug("Session data FOUND");

                    err = SSLServerValidateSessionData(resumableSession, ctx);
                }

                if (!err)
                {

                    sslLogResumSessDebug("Session data VALID");
                    sslLogResumSessDebug("RESUMING TLS server-side session");
                    if ((err = SSLInstallSessionFromData(resumableSession,
                                                             ctx)) != 0)
                    {   // FIXME: Move this somewhere else ? SSLFatalSessionAlert(SSL_AlertInternalError, ctx);
                        return err;
                    }
                    ctx->sessionMatch = true;

                } else if (err == errSSLFatalAlert) {
                    return err;
                }else {
                    sslLogResumSessDebug("FAILED TO RESUME TLS server-side session");
                }
            }

            sslLogResumSessDebug("Session match = %d", ctx->sessionMatch);

            if (ctx->sessionMatch)
            {
                SSLFreeBuffer(&ctx->sessionID);
                SSLCopyBuffer(&ctx->proposedSessionID, &ctx->sessionID);
                /* queue up remaining messages to finish handshake */
                if((err = SSLResumeServerSide(ctx)) != 0)
                    return err;
                break;
            }

            /*
             * If we get here, we're not resuming;
             * 1) Select a ciphersuite
             * 2) free the received sessionID and generate a new one
             *    unless we explicitely do not want this session to
             *    be cached
             */

            if ((err = SelectNewCiphersuite(ctx)) != 0) {
                return err;
            };

            SSLFreeBuffer(&ctx->sessionID);

            if(ctx->allowResumption)
            {   /* Ignore errors; just treat as uncached session */
                err = SSLAllocBuffer(&ctx->sessionID, SSL_SESSION_ID_LEN);
                if (err == 0)
                {   
                    if((err = sslRand(&ctx->sessionID)) != 0)
                    {   SSLFatalSessionAlert(SSL_AlertInternalError, ctx);
                        return err;
                    }
                }
            }

            if ((err = SSLPrepareAndQueueMessage(SSLEncodeServerHello, tls_record_type_Handshake, ctx)) != 0)
                return err;

            switch (ctx->selectedCipherSpecParams.keyExchangeMethod)
            {
                case SSL_NULL_auth:
                case TLS_PSK:
#if		APPLE_DH
                case SSL_DH_anon:
                case SSL_ECDH_anon:
                    check(!ctx->tryClientAuth);
                    if(ctx->tryClientAuth) {
                        /* This is illegal, see RFC 5246, section 7.4.4. */
                        sslErrorLog("SSLAdvanceHandshake: Attempting Client Auth with Anonyous CipherSuite!\n");
                        SSLFatalSessionAlert(SSL_AlertInternalError, ctx);
                        return errSSLBadConfiguration;
                    }
                	ctx->tryClientAuth = false;
					/* DH server side needs work */
#endif	/* APPLE_DH */
                    break;
                case SSL_RSA:
                case SSL_DHE_RSA:
                case SSL_ECDHE_ECDSA:
                case SSL_ECDHE_RSA:
					if(ctx->localCert == NULL) {
						/* no cert but configured for, and negotiated, a
						 * ciphersuite which requires one */
						sslErrorLog("SSLAdvanceHandshake: No server cert!\n");
                        SSLFatalSessionAlert(SSL_AlertInternalError, ctx);
						return errSSLBadConfiguration;
					}
                    if ((err = SSLPrepareAndQueueMessage(SSLEncodeCertificate, tls_record_type_Handshake,
                                                         ctx)) != 0)
                        return err;
                    if (ctx->ocsp_peer_enabled && ctx->ocsp_response.length) {
                        if ((err = SSLPrepareAndQueueMessage(SSLEncodeCertificateStatus, tls_record_type_Handshake,
                                                             ctx)) != 0)
                            return err;
                    }
                    break;
                default:        /* everything else */
                    sslErrorLog("SSLAdvanceHandshake: Unsupported KEM!\n");
                    SSLFatalSessionAlert(SSL_AlertInternalError, ctx);
                    return errSSLInternal;
            }
			/*
			 * At this point we decide whether to send a server key exchange
			 * method.
			 */
			{
				bool doServerKeyExch = false;
				switch(ctx->selectedCipherSpecParams.keyExchangeMethod) {
					case SSL_DH_anon:
					case SSL_DHE_RSA:
                    case SSL_ECDH_anon:
                    case SSL_ECDHE_RSA:
                    case SSL_ECDHE_ECDSA:
						doServerKeyExch = true;
						break;
#if ALLOW_RSA_SERVER_KEY_EXCHANGE
                    case SSL_RSA:
                        if(ctx->rsaEncryptPubKey.rsa!=NULL) {
                            doServerKeyExch = true;
                        }
                        break;
#endif
					default: /* In all other cases, we don't send a ServerkeyExchange message */
						break;
				}
				if(doServerKeyExch) {
					err = SSLPrepareAndQueueMessage(SSLEncodeServerKeyExchange, tls_record_type_Handshake, ctx);
					if(err) {
						return err;
					}
				}
			}
            if (ctx->tryClientAuth)
            {
                if ((err = SSLPrepareAndQueueMessage(SSLEncodeCertificateRequest, tls_record_type_Handshake,
						ctx)) != 0)
                    return err;
                ctx->certRequested = 1;
				ctx->clientCertState = kSSLClientCertRequested;
            }
            if ((err = SSLPrepareAndQueueMessage(SSLEncodeServerHelloDone, tls_record_type_Handshake, ctx)) != 0)
                return err;
            if (ctx->certRequested) {
                SSLChangeHdskState(ctx, SSL_HdskStateClientCert);
            } else {
                SSLChangeHdskState(ctx, SSL_HdskStateClientKeyExchange);
            }
            break;
        case SSL_HdskServerHello:
			ctx->sessionMatch = 0;
            if (ctx->resumableSession.data != 0 && ctx->sessionID.data != 0)
            {
                tls_buffer sessionID;
                SSLRetrieveSessionID(ctx->resumableSession, &sessionID);
                if((sessionID.length == ctx->sessionID.length) &&
                   (memcmp(sessionID.data, ctx->sessionID.data, sessionID.length) == 0))
                {
                    sslLogResumSessDebug("Server willing to RESUME TLS client-side session ID=%02x...", ctx->sessionID.data[0]);

                    if(SSLClientValidateSessionDataAfter(ctx->resumableSession, ctx) == 0)
                    {   /* Everything matches; resume the session */
                        sslLogResumSessDebug("RESUMING TLS client-side session");
                        if ((err = SSLInstallSessionFromData(ctx->resumableSession, ctx)) != 0 ||
                            (err = SSLInitPendingCiphers(ctx)) != 0)
                        {   SSLFatalSessionAlert(SSL_AlertInternalError, ctx);
                            return err;
                        }
                        ctx->sessionMatch = 1;
                        if(ctx->sessionTicket_confirmed) {
                            SSLChangeHdskState(ctx, SSL_HdskStateNewSessionTicket);
                        } else {
                            SSLChangeHdskState(ctx, SSL_HdskStateChangeCipherSpec);
                        }
                        break;
                    } else {
                        sslLogResumSessDebug("FAILED TO RESUME TLS client-side session: ");
                        SSLFatalSessionAlert(SSL_AlertIllegalParam, ctx);
                        return errSSLProtocol;
                    }
                }
            }
            switch (ctx->selectedCipherSpecParams.keyExchangeMethod)
            {   
            	/* these require a key exchange message */
            	case SSL_NULL_auth:
                case SSL_DH_anon:
                case SSL_ECDH_anon:
                    SSLChangeHdskState(ctx, SSL_HdskStateKeyExchange);
                    break;
                case SSL_RSA:
                case SSL_DHE_RSA:
				case SSL_ECDHE_ECDSA:
				case SSL_ECDHE_RSA:
                    SSLChangeHdskState(ctx, SSL_HdskStateCert);
                    break;
                case TLS_PSK:
                    SSLChangeHdskState(ctx, SSL_HdskStateHelloDone);
                    break;
                default:
                    assert("Unknown key exchange method");
                    break;
            }
            break;
        case SSL_HdskCert:
            if(ctx->isServer && ctx->peerCert)
                ctx->clientCertState = kSSLClientCertSent;

            if (ctx->state == SSL_HdskStateCert)
                switch (ctx->selectedCipherSpecParams.keyExchangeMethod)
                {
                    case SSL_RSA:
                        SSLChangeHdskState(ctx, SSL_HdskStateHelloDone);
                        break;
                    case SSL_DHE_RSA:
					case SSL_ECDHE_ECDSA:
					case SSL_ECDHE_RSA:
                        SSLChangeHdskState(ctx, SSL_HdskStateKeyExchange);
                        break;
                    default:
                        assert("Unknown or unexpected key exchange method");
                        break;
                }
            else if (ctx->state == SSL_HdskStateClientCert)
            {
                SSLChangeHdskState(ctx, SSL_HdskStateClientKeyExchange);
                if (ctx->peerCert != 0)
                    ctx->certReceived = 1;
            }
            break;
        case SSL_HdskCertificateStatus:
            assert(!ctx->isServer);
            break;
        case SSL_HdskCertRequest:
			/* state stays in SSL_HdskStateHelloDone; distinction is in
			 *  ctx->certRequested */
            /* Why this ? Can server ask for cert without having one ? */
            if (ctx->peerCert == 0)
            {   SSLFatalSessionAlert(SSL_AlertHandshakeFail, ctx);
                return errSSLProtocol;
            }
            assert(!ctx->isServer);
            ctx->certRequested = 1;
            ctx->clientCertState = kSSLClientCertRequested;
            break;
        case SSL_HdskServerKeyExchange:
            SSLChangeHdskState(ctx, SSL_HdskStateHelloDone);
            break;
        case SSL_HdskServerHelloDone:
            /* Did the app trust this cert ? */
            if(ctx->peerCert && ctx->peerTrust)
            {
                AlertDescription desc;
                switch(ctx->peerTrust) {
                    case tls_handshake_trust_unknown:
                        err = errSSLInternal;
                        desc = SSL_AlertInternalError;
                        break;
                    case tls_handshake_trust_unknown_root:
                        err = errSSLUnknownRootCert;
                        desc = SSL_AlertUnknownCA;
                        break;
                    case tls_handshake_trust_cert_expired:
                        err = errSSLCertExpired;
                        desc = SSL_AlertCertExpired;
                        break;
                    case tls_handshake_trust_cert_invalid:
                    default:
                        err = errSSLXCertChainInvalid;
                        desc = SSL_AlertCertUnknown;
                        break;
                }
                SSLFatalSessionAlert(desc, ctx);
                if(ctx->isServer && ctx->peerCert) {
                    ctx->clientCertState = kSSLClientCertRejected;
                }
                return err;
            }
            if (ctx->clientCertState == kSSLClientCertRequested) {
                /*
                 * Server wants a client authentication cert.
                 * If we have one, we send it even if it does not match one of
                 * the requested types.
                 */

                if (ctx->localCert == 0 || ctx->signingPrivKeyRef==NULL) {
                    ctx->negAuthType = tls_client_auth_type_None;
                } else {
                    switch(ctx->signingPrivKeyRef->desc.type) {
                        case tls_private_key_type_ecdsa:
                            ctx->negAuthType = tls_client_auth_type_ECDSASign;
                            break;
                        case tls_private_key_type_rsa:
                            ctx->negAuthType = tls_client_auth_type_RSASign;
                            break;
                        default:
                            ctx->negAuthType = tls_client_auth_type_None;
                            break;
                    }
                }

                if (ctx->negAuthType != tls_client_auth_type_None) {
					if ((err = SSLPrepareAndQueueMessage(SSLEncodeCertificate, tls_record_type_Handshake,
							ctx)) != 0) {
						return err;
					}
                } else {
					/* response for no cert depends on protocol version */
					if(ctx->negProtocolVersion >= tls_protocol_version_TLS_1_0) {
						/* TLS: send empty cert msg */
						if ((err = SSLPrepareAndQueueMessage(SSLEncodeCertificate, tls_record_type_Handshake,
								ctx)) != 0) {
							return err;
						}
					}
					else {
						/* SSL3: "no cert" alert */
						if ((err = SSLSendAlert(SSL_AlertLevelWarning, SSL_AlertNoCert_RESERVED,
								ctx)) != 0) {
							return err;
						}
					}
                }	/* no cert to send */
            }	/* server requested a cert */
            if ((err = SSLPrepareAndQueueMessage(SSLEncodeKeyExchange, tls_record_type_Handshake, ctx)) != 0)
                return err;
			assert(ctx->sslTslCalls != NULL);
            if ((err = ctx->sslTslCalls->generateMasterSecret(ctx)) != 0 ||
                (err = SSLInitPendingCiphers(ctx)) != 0)
            {   SSLFatalSessionAlert(SSL_AlertInternalError, ctx);
                return err;
            }
			memset(ctx->preMasterSecret.data, 0, ctx->preMasterSecret.length);
            if ((err = SSLFreeBuffer(&ctx->preMasterSecret))) {
                return err;
			}
            if (ctx->certSent) {
                /* All client auth mechanisms we support do require a cert verify message */
                if ((err = SSLPrepareAndQueueMessage(SSLEncodeCertificateVerify, tls_record_type_Handshake,
                        ctx)) != 0) {
                    return err;
                }
			}
            if ((err = SSLPrepareAndQueueMessage(SSLEncodeChangeCipherSpec, tls_record_type_ChangeCipher,
					ctx)) != 0) {
                return err;
			}
            if (!ctx->isServer && ctx->npn_received && !ctx->npn_confirmed) {
                if ((err = SSLPrepareAndQueueMessage(SSLEncodeNPNEncryptedExtensionMessage, tls_record_type_Handshake, ctx)) != 0)
                    return err;
            }
            if ((err = SSLPrepareAndQueueMessage(SSLEncodeFinishedMessage, tls_record_type_Handshake, ctx)) != 0)
                return err;
            if(ctx->sessionTicket_confirmed) {
                SSLChangeHdskState(ctx, SSL_HdskStateNewSessionTicket);
            } else {
                SSLChangeHdskState(ctx, SSL_HdskStateChangeCipherSpec);
            }
            break;
        case SSL_HdskCertVerify:
            SSLChangeHdskState(ctx, SSL_HdskStateChangeCipherSpec);
            break;
        case SSL_HdskClientKeyExchange:
 			assert(ctx->sslTslCalls != NULL);
			if ((err = ctx->sslTslCalls->generateMasterSecret(ctx)) != 0 ||
                (err = SSLInitPendingCiphers(ctx)) != 0)
            {   SSLFatalSessionAlert(SSL_AlertInternalError, ctx);
                return err;
            }
			memset(ctx->preMasterSecret.data, 0, ctx->preMasterSecret.length);
            if ((err = SSLFreeBuffer(&ctx->preMasterSecret)))
                return err;
            if (ctx->certReceived) {
                SSLChangeHdskState(ctx, SSL_HdskStateClientCertVerify);
            }
            else {
                SSLChangeHdskState(ctx, SSL_HdskStateChangeCipherSpec);
            }
            break;
        case SSL_HdskNewSessionTicket:
            SSLChangeHdskState(ctx, SSL_HdskStateChangeCipherSpec);
            break;
        case SSL_HdskFinished:
            if(ctx->isServer && ctx->certRequested && ctx->peerTrust)
            {
                AlertDescription desc;
                switch(ctx->peerTrust) {
                    case tls_handshake_trust_unknown:
                        err = errSSLInternal;
                        desc = SSL_AlertInternalError;
                        break;
                    case tls_handshake_trust_unknown_root:
                        err = errSSLUnknownRootCert;
                        desc = SSL_AlertUnknownCA;
                        break;
                    case tls_handshake_trust_cert_expired:
                        err = errSSLCertExpired;
                        desc = SSL_AlertCertExpired;
                        break;
                    case tls_handshake_trust_cert_invalid:
                    default:
                        err = errSSLXCertChainInvalid;
                        desc = SSL_AlertCertUnknown;
                        break;
                }
                SSLFatalSessionAlert(desc, ctx);
                if(ctx->isServer && ctx->peerCert) {
                    ctx->clientCertState = kSSLClientCertRejected;
                }
                return err;
            }

            /* We successfully processed the peer Finished message: we can now trust data received */
            sslReadReady(ctx, true);
            /* If writePending is set, we haven't yet sent a finished message;
			 * send it */
            /* Note: If using session resumption, the client will hit this, otherwise the server will */
            if (ctx->writePending_ready)
            {   if ((err = SSLPrepareAndQueueMessage(SSLEncodeChangeCipherSpec, tls_record_type_ChangeCipher,
						ctx)) != 0)
                    return err;

                if (!ctx->isServer && ctx->npn_received && !ctx->npn_confirmed) {
                    if ((err = SSLPrepareAndQueueMessage(SSLEncodeNPNEncryptedExtensionMessage, tls_record_type_Handshake, ctx)) != 0)
                        return err;
                }
                if ((err = SSLPrepareAndQueueMessage(SSLEncodeFinishedMessage, tls_record_type_Handshake,
							ctx)) != 0)
                    return err;
            } else {
                /* We already sent our finished message, and received the Peer finished message,
                 We are ready to write data */
                sslWriteReady(ctx, true);
            }
            if (ctx->isServer) {
                SSLChangeHdskState(ctx, SSL_HdskStateServerReady);
            }
            else {
                SSLChangeHdskState(ctx, SSL_HdskStateClientReady);
                if(!ctx->sessionMatch) {
                    tls_metric_client_finished(ctx);
                }
            }
            if (ctx->allowResumption && (!ctx->sessionMatch || ctx->sessionTicket.data)) {  // Only store the session if this is not resumed or if we have a new ticket from the server
                SSLAddSessionData(ctx);
			}
            break;
        default:
            assert(0);
            break;
    }

    /* We should have a full flight when we reach here, sending it for the first time */
    return SSLSendFlight(ctx);
}

int
SSLPrepareAndQueueMessage(EncodeMessageFunc msgFunc, uint8_t contentType, tls_handshake_t ctx)
{   int        err;
    tls_buffer       rec = {0, NULL};
    WaitingMessage  *out;
    WaitingMessage  *queue;

    if ((err = msgFunc(&rec, ctx)) != 0)
    {   SSLFatalSessionAlert(SSL_AlertCloseNotify, ctx);
        goto fail;
    }

    if (contentType==tls_record_type_Handshake) {
        if(msgFunc!=SSLEncodeServerHelloRequest) {
            if ((err = SSLUpdateHandshakeMacs(&rec, ctx)) != 0) {
                SSLFatalSessionAlert(SSL_AlertInternalError, ctx);
                goto fail;
            }
        }
        SSLLogHdskMsg((SSLHandshakeType)rec.data[0], 1);
        ctx->hdskMessageSeq++;
    }

    err=errSSLInternal;
    out = (WaitingMessage *)sslMalloc(sizeof(WaitingMessage));
    if(out==NULL) goto fail;

    out->next = NULL;
	out->rec = rec;
    out->contentType = contentType;

    queue=ctx->messageWriteQueue;
    if (queue == NULL) {
        sslHdskMsgDebug("Queuing first message in flight\n");
        ctx->messageWriteQueue = out;
    } else {
        int n=1;
        while (queue->next != 0) {
            queue = queue->next;
            n++;
        }
        sslHdskMsgDebug("Queuing message %d in flight\n", n);
        queue->next = out;
    }

    return errSSLSuccess;
fail:
    SSLFreeBuffer(&rec);
    return err;
}

static
int SSLSendMessage(tls_buffer rec, uint8_t contentType, tls_handshake_t ctx)
{
    int err;


    if ((err = ctx->callbacks->write(ctx->callback_ctx, rec, contentType)) != 0)
        return err;
    if(contentType == tls_record_type_ChangeCipher) {
        /* TODO: if we want to enabled App data during most of handshake,
           we need to disable write channel here, until Finished is sent 
         sslWriteReady(ctx, false);
         */
        /* Install new cipher spec on write side */
        if ((err = ctx->callbacks->advance_write_cipher(ctx->callback_ctx)) != 0)
        {   SSLFatalSessionAlert(SSL_AlertInternalError, ctx);
            return err;
        }
        /* pending cipher is invalid now - this is currently used to figure out if we need
           to send out the last flight */
        ctx->writePending_ready = 0;
    }

    return errSSLSuccess;
}

static
int DTLSSendMessage(tls_buffer rec, uint8_t contentType, tls_handshake_t ctx)
{
    int err=errSSLSuccess;

    if(contentType != tls_record_type_Handshake) {
        sslHdskMsgDebug("Not fragmenting message type=%d len=%d\n", (int)contentType, (int)rec.length);
        if ((err = ctx->callbacks->write(ctx->callback_ctx, rec, contentType)) != 0)
            return err;
        if(contentType == tls_record_type_ChangeCipher) {
            /* TODO: if we want to enabled App data during most of handshake,
             we need to disable write channel here, until Finished is sent
             sslWriteReady(ctx, false);
             */
            /* Install new cipher spec on write side */
            if ((err = ctx->callbacks->advance_write_cipher(ctx->callback_ctx)) != 0)
            {
                SSLFatalSessionAlert(SSL_AlertInternalError, ctx);
                return err;
            }

            /* pending cipher is invalid now - this is currently used to figure out if we need
             to send out the last flight */
            ctx->writePending_ready = 0;
        }
    } else {
    /* fragmenting */
        tls_buffer fragrec;

        int msghead = 12;  /* size of message header in DTLS */
        size_t fraglen;
        size_t len = rec.length-msghead;
        UInt32 seq = SSLDecodeInt(rec.data+4, 2);
        (void) seq; // Suppress warnings
        size_t ofs = 0;

        sslHdskMsgDebug("Fragmenting msg seq %d (rl=%d, ml=%d)", (int)seq, (int)rec.length,
                        SSLDecodeInt(rec.data+1, 3));


        //SSLGetDatagramWriteSize(ctx, &fraglen);
        fraglen=getMaxDataGramSize(ctx);

        fraglen -=  msghead;

        if((err=SSLAllocBuffer(&fragrec, fraglen + msghead)))
            return err;

        /* copy the constant part of the header */
        memcpy(fragrec.data,rec.data, 6);

        while(len>fraglen) {

            sslHdskMsgDebug("Fragmenting msg seq %d (o=%d,l=%d)", (int)seq, (int)ofs, (int)fraglen);

            /* fragment offset and fragment length */
            SSLEncodeSize(fragrec.data+6, ofs, 3);
            SSLEncodeSize(fragrec.data+9, fraglen, 3);
            /* copy the payload */
            memcpy(fragrec.data+msghead, rec.data+msghead+ofs, fraglen);
            if ((err = ctx->callbacks->write(ctx->callback_ctx, fragrec, contentType)) != 0)
                goto cleanup;
            len-=fraglen;
            ofs+=fraglen;
        }

        sslHdskMsgDebug("Fragmenting msg seq %d - Last Fragment (o=%d,l=%d)", (int)seq, (int)ofs, (int)len);

        /* last fragment */
        /* fragment offset and fragment length */
        SSLEncodeSize(fragrec.data+6, ofs, 3);
        SSLEncodeSize(fragrec.data+9, len, 3);
        /* copy the payload */
        memcpy(fragrec.data+msghead, rec.data+msghead+ofs, len);
        fragrec.length=len+msghead;
        err = ctx->callbacks->write(ctx->callback_ctx, fragrec, contentType);

    cleanup:
        /* Free the allocated fragment buffer */
        SSLFreeBuffer(&fragrec);

    }

    return err;
}


int SSLResetFlight(tls_handshake_t ctx)
{
    int err;
    WaitingMessage *queue;
    WaitingMessage *next;
    int n=0;

    ctx->hdskMessageRetryCount = 0;
    if(ctx->isDTLS) {
        ctx->callbacks->set_retransmit_timer(ctx->callback_ctx, 0);
    }
    
    queue=ctx->messageWriteQueue;
    ctx->messageQueueContainsChangeCipherSpec=false;

    while(queue) {
        n++;
        err = SSLFreeBuffer(&queue->rec);
        if (err != 0)
            goto fail;
        next=queue->next;
        sslFree(queue);
        queue=next;
    }

    ctx->messageWriteQueue=NULL;

    return errSSLSuccess;
fail:
    check_noerr(err);
    return err;
}



static bool isFalseStartAllowed(tls_handshake_t ctx)
{
    SSL_CipherAlgorithm c=sslCipherSuiteGetSymmetricCipherAlgorithm(ctx->selectedCipher);
    KeyExchangeMethod kem=sslCipherSuiteGetKeyExchangeMethod(ctx->selectedCipher);

    /* Whitelisting allowed ciphers, kem and client auth type */
    return ctx->falseStartEnabled &&
    ((c==SSL_CipherAlgorithmAES_128_CBC) ||
     (c==SSL_CipherAlgorithmAES_128_GCM) ||
     (c==SSL_CipherAlgorithmAES_256_CBC) ||
     (c==SSL_CipherAlgorithmAES_256_GCM)) &&
    ((kem==SSL_ECDHE_ECDSA) ||
     (kem==SSL_ECDHE_RSA) ||
     (kem==SSL_DHE_RSA)) &&
    ((ctx->negAuthType==tls_client_auth_type_None) ||
     (ctx->negAuthType==tls_client_auth_type_RSASign) ||
     (ctx->negAuthType==tls_client_auth_type_ECDSASign));
}

int SSLSendFlight(tls_handshake_t ctx)
{
    int err;
    WaitingMessage  *queue;
    int n=0;

    if(ctx->isDTLS) {
        ctx->hdskMessageRetryCount++;
        if(ctx->hdskMessageRetryCount>10)
            return errSSLConnectionRefused;
        ctx->callbacks->set_retransmit_timer(ctx->callback_ctx, ctx->hdskMessageRetryCount);
    }

    queue=ctx->messageWriteQueue;

    while(queue) {
        if (ctx->isDTLS) {
            err=DTLSSendMessage(queue->rec, queue->contentType, ctx);
        } else {
            err=SSLSendMessage(queue->rec, queue->contentType, ctx);
        }
        if (err != 0)
            goto fail;
        queue=queue->next;
        n++;
    }

    if(ctx->messageQueueContainsChangeCipherSpec) {
        /* We just send a ChangeCipherSpecs and a Finished message, 
           if we already sucessfully processed the peer Finished message (readCipher_ready==1),
           or if we can use FalseStart, we are ready to send some data */
        if(isFalseStartAllowed(ctx) || ctx->readCipher_ready)
        {
            sslWriteReady(ctx, true);
        }
    }

    return errSSLSuccess;
fail:
    return err;
}

/*
 * Determine max enabled protocol, i.e., the one we try to negotiate for.
 * Only returns an error (errSecParam) if NO protocols are enabled, which can
 * in fact happen by malicious or ignorant use of SSLSetProtocolVersionEnabled().
 */
int sslGetMaxProtVersion(tls_handshake_t ctx,
                         tls_protocol_version	 *version)	// RETURNED
{
    /* This check is here until SSLSetProtocolVersionEnabled() is gone .*/
    if (ctx->maxProtocolVersion == tls_protocol_version_Undertermined)
        return errSSLParam;

    *version = ctx->maxProtocolVersion;
    return errSSLSuccess;
}


/* log changes in handshake state */
#ifndef	NDEBUG
#include <stdio.h>

char *hdskStateToStr(SSLHandshakeState state)
{
	static char badStr[100];

	switch(state) {
		case SSL_HdskStateUninit:
			return "Uninit";
		case SSL_HdskStateServerUninit:
			return "ServerUninit";
		case SSL_HdskStateClientUninit:
			return "ClientUninit";
		case SSL_HdskStateGracefulClose:
			return "GracefulClose";
		case SSL_HdskStateErrorClose:
			return "ErrorClose";
		case SSL_HdskStateNoNotifyClose:
			return "NoNotifyClose";
		case SSL_HdskStateServerHello:
			return "ServerHello";
		case SSL_HdskStateKeyExchange:
			return "KeyExchange";
		case SSL_HdskStateCert:
			return "Cert";
		case SSL_HdskStateHelloDone:
			return "HelloDone";
		case SSL_HdskStateClientCert:
			return "ClientCert";
		case SSL_HdskStateClientKeyExchange:
			return "ClientKeyExchange";
		case SSL_HdskStateClientCertVerify:
			return "ClientCertVerify";
        case SSL_HdskStateNewSessionTicket:
            return "NewSessionTicket";
		case SSL_HdskStateChangeCipherSpec:
			return "ChangeCipherSpec";
		case SSL_HdskStateFinished:
			return "Finished";
		case SSL_HdskStateServerReady:
			return "SSL_ServerReady";
		case SSL_HdskStateClientReady:
			return "SSL_ClientReady";
		default:
			sprintf(badStr, "Unknown state (%d(d)", state);
			return badStr;
	}
}

/* This is a macro in Release mode */
void SSLChangeHdskState(tls_handshake_t ctx, SSLHandshakeState newState)
{
	sslHdskStateDebug("...hdskState = %s", hdskStateToStr(newState));
	ctx->state = newState;
}

/* log handshake messages */
#if SSL_DEBUG
static char *hdskMsgToStr(SSLHandshakeType msg)
{
	static char badStr[100];

	switch(msg) {
		case SSL_HdskHelloRequest:
			return "SSL_HdskHelloRequest";
		case SSL_HdskClientHello:
			return "SSL_HdskClientHello";
		case SSL_HdskServerHello:
			return "SSL_HdskServerHello";
        case SSL_HdskHelloVerifyRequest:
			return "SSL_HdskHelloVerifyRequest";
        case SSL_HdskNewSessionTicket:
            return "SSL_HdskNewSessionTicket";
		case SSL_HdskCert:
			return "SSL_HdskCert";
		case SSL_HdskServerKeyExchange:
			return "SSL_HdskServerKeyExchange";
		case SSL_HdskCertRequest:
			return "SSL_HdskCertRequest";
		case SSL_HdskServerHelloDone:
			return "SSL_HdskServerHelloDone";
		case SSL_HdskCertVerify:
			return "SSL_HdskCertVerify";
		case SSL_HdskClientKeyExchange:
			return "SSL_HdskClientKeyExchange";
		case SSL_HdskFinished:
			return "SSL_HdskFinished";
        case SSL_HdskCertificateStatus:
            return "SSL_HdskCertificateStatus";
        case SSL_HdskNPNEncryptedExtension:
            return "SSL_HdskNPNEncryptedExtension";
		default:
			sprintf(badStr, "Unknown msg (%d(d))", msg);
			return badStr;
	}
}
#endif

void SSLLogHdskMsg(SSLHandshakeType msg, char sent)
{
	sslHdskMsgDebug("---%s handshake msg %s",
		hdskMsgToStr(msg), (sent ? "sent" : "recv"));
}

#endif	/* NDEBUG */


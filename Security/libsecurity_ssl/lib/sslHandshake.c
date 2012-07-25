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

#include "sslContext.h"
#include "sslHandshake.h"
#include "sslMemory.h"
#include "sslAlertMessage.h"
#include "sslSession.h"
#include "sslUtils.h"
#include "sslDebug.h"
#include "sslCrypto.h"

#include "sslDigests.h"
#include "cipherSpecs.h"

#include <string.h>
#include <assert.h>
#include <inttypes.h>

#define REQUEST_CERT_CORRECT        0

#if __LP64__
#define PRIstatus "d"
#else
#define PRIstatus "ld"
#endif

static OSStatus SSLProcessHandshakeMessage(SSLHandshakeMsg message, SSLContext *ctx);

static OSStatus
SSLUpdateHandshakeMacs(const SSLBuffer *messageData, SSLContext *ctx)
{
    OSStatus err = errSecParam;
    bool do_md5 = false;
    bool do_sha1 = false;
    bool do_sha256 = false;
    bool do_sha384 = false;

    //TODO: We can stop updating the unecessary hashes once the CertVerify message is processed in case where we do Client Side Auth, or .

    if(ctx->negProtocolVersion == SSL_Version_Undetermined)
    {
        // we dont know yet, so we might need MD5 & SHA1 - Server should always call in with known protocol version.
        assert(ctx->protocolSide==kSSLClientSide);
        do_md5 = do_sha1 = true;
        if(ctx->isDTLS
           ? ctx->maxProtocolVersion < DTLS_Version_1_0
           : ctx->maxProtocolVersion >= TLS_Version_1_2)
        {
            // We wil need those too, unless we are sure we wont end up doing TLS 1.2
            do_sha256 = do_sha384 = true;
        }
    } else {
        // we know which version we use at this point
        if(sslVersionIsLikeTls12(ctx)) {
            do_sha1 = do_sha256 = do_sha384 = true;
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
        (err = SSLHashSHA384.update(&ctx->sha512State, messageData)) != 0)
        goto done;

    sslLogNegotiateDebug("%s protocol: %02X max: %02X cipher: %02X%s%s",
                         ctx->protocolSide == kSSLClientSide ? "client" : "server",
                         ctx->negProtocolVersion,
                         ctx->maxProtocolVersion,
                         ctx->selectedCipher,
                         do_md5 ? " md5" : "",
                         do_sha1 ? " sha1" : "",
                         do_sha256 ? " sha256" : "",
                         do_sha384 ? " sha384" : "");
done:
    return err;
}

OSStatus
SSLProcessHandshakeRecord(SSLRecord rec, SSLContext *ctx)
{   OSStatus        err;
    size_t          remaining;
    UInt8           *p;
	UInt8			*startingP;		// top of record we're parsing
    SSLHandshakeMsg message = {};
    SSLBuffer       messageData;

    if (ctx->fragmentedMessageCache.data != 0)
    {
		size_t origLen = ctx->fragmentedMessageCache.length;
		if ((err = SSLReallocBuffer(&ctx->fragmentedMessageCache,
                    ctx->fragmentedMessageCache.length + rec.contents.length,
                    ctx)) != 0)
        {   SSLFatalSessionAlert(SSL_AlertInternalError, ctx);
            return err;
        }
        memcpy(ctx->fragmentedMessageCache.data + origLen,
            rec.contents.data, rec.contents.length);
        remaining = ctx->fragmentedMessageCache.length;
        p = ctx->fragmentedMessageCache.data;
    }
    else
    {   remaining = rec.contents.length;
        p = rec.contents.data;
    }
	startingP = p;

    size_t head = 4;

    while (remaining > 0)
    {
        if (remaining < head)
            break;  /* we must have at least a header */

        messageData.data = p;
        message.type = (SSLHandshakeType)*p++;
        message.contents.length = SSLDecodeSize(p, 3);


        p += 3;

        if ((message.contents.length + head) > remaining)
            break;

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

        if ((err = SSLAdvanceHandshake(message.type, ctx)) != 0)
            return err;
    }

    if (remaining > 0)      /* Fragmented handshake message */
    {   /* If there isn't a cache, allocate one */
        if (ctx->fragmentedMessageCache.data == 0)
        {   if ((err = SSLAllocBuffer(&ctx->fragmentedMessageCache, remaining, ctx)) != 0)
            {   SSLFatalSessionAlert(SSL_AlertInternalError, ctx);
                return err;
            }
        }
        if (startingP != ctx->fragmentedMessageCache.data)
        {   memcpy(ctx->fragmentedMessageCache.data, startingP, remaining);
            ctx->fragmentedMessageCache.length = remaining;
        }
    }
    else if (ctx->fragmentedMessageCache.data != 0)
    {   if ((err = SSLFreeBuffer(&ctx->fragmentedMessageCache, ctx)) != 0)
        {   SSLFatalSessionAlert(SSL_AlertInternalError, ctx);
            return err;
        }
    }

    return noErr;
}

OSStatus
DTLSProcessHandshakeRecord(SSLRecord rec, SSLContext *ctx)
{   OSStatus        err = errSecParam;
    size_t          remaining;
    UInt8           *p;
	UInt8			*startingP;		// top of record we're parsing

    const UInt32 head = 12;

    assert(ctx->isDTLS);

    remaining = rec.contents.length;
    p = rec.contents.data;
	startingP = p;

    while (remaining > 0)
    {
        UInt8 msgtype;
        UInt32 msglen;
        UInt32 msgseq;
        UInt32 fraglen;
        UInt32 fragofs;

        if (remaining < head) {
            /* flush it - record is too small */
            sslErrorLog("DTLSProcessHandshakeRecord: remaining too small (%lu out of %lu)\n", remaining, rec.contents.length);
            assert(0); // keep this assert until we find a test case that triggers it
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
        sslHdskMsgDebug("DTLS Hdsk Record: type=%lu, len=%lu, seq=%lu (%lu), f_ofs=%lu, f_len=%lu, remaining=%lu",
                             msgtype, msglen, msgseq, ctx->hdskMessageSeqNext, fragofs, fraglen, remaining);

        if(
           ((fraglen+fragofs) > msglen)
           || (fraglen > remaining)
           || (msgseq!=ctx->hdskMessageSeqNext)
           || (fragofs!=ctx->hdskMessageCurrentOfs)
           || (fragofs && (msgtype!=ctx->hdskMessageCurrent.type))
           || (fragofs && (msglen != ctx->hdskMessageCurrent.contents.length))
           )
        {
            sslErrorLog("DTLSProcessHandshakeRecord: wrong fragment\n");
            // assert(0); // keep this assert until we find a test case that triggers it
            // This is a recoverable error, we just drop this fragment.
            // TODO: this should probably trigger a retransmit
            err = noErr;
            goto flushit;
        }

        /* First fragment - allocate */
        if(fragofs==0) {
            sslHdskMsgDebug("Allocating hdsk buf for msg type %d", msgtype);
            assert(ctx->hdskMessageCurrent.contents.data==NULL);
            assert(ctx->hdskMessageCurrent.contents.length==0);
            if((err=SSLAllocBuffer(&(ctx->hdskMessageCurrent.contents), msglen, ctx))!=0) {
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
                SSLBuffer header;
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

                SSLBuffer *messageData=&ctx->hdskMessageCurrent.contents;
                if ((err = SSLHashSHA1.update(&ctx->shaState, messageData)) != 0 ||
                    (err = SSLHashMD5.update(&ctx->md5State, messageData)) != 0)
                {
                    SSLFatalSessionAlert(SSL_AlertInternalError, ctx);
                    goto flushit;
                }

                sslHdskMsgDebug("Hashing %d bytes of msg seq %ld\n", messageData->length, msgseq);
            }

            sslHdskMsgDebug("processed message of type %d", msgtype);

            if ((err = SSLAdvanceHandshake(msgtype, ctx)) != 0)
            {
                sslErrorLog("AdvanceHandshake error: %"PRIstatus"\n", err);
                goto flushit;
            }

            /* Free the buffer for current message, and reset offset */
            SSLFreeBuffer(&(ctx->hdskMessageCurrent.contents), ctx);
            ctx->hdskMessageCurrentOfs=0;

            /* If we successfully processed a message, we wait for the next one */
            ctx->hdskMessageSeqNext++;

        }

        sslHdskMsgDebug("remaining = %ld", remaining);
    }

    return noErr;

flushit:
    sslErrorLog("DTLSProcessHandshakeRecord: flusing record (err=%"PRIstatus")\n", err);

    /* This will flush the current handshake message */
    SSLFreeBuffer(&(ctx->hdskMessageCurrent.contents), ctx);
    ctx->hdskMessageCurrentOfs=0;

    return err;
}

OSStatus
DTLSRetransmit(SSLContext *ctx)
{
    sslHdskMsgDebug("DTLSRetransmit in state %s. Last Sent = %d, Last Recv=%d, timeout=%f\n",
                hdskStateToStr(ctx->state), ctx->hdskMessageSeq, ctx->hdskMessageSeqNext, ctx->timeout_duration);

    /* Too many retransmits, just give up!! */
    if(ctx->hdskMessageRetryCount>10)
        return errSSLConnectionRefused;

    /* go back to previous cipher if retransmitting a flight including changecipherspec */
    if(ctx->messageQueueContainsChangeCipherSpec) {
        ctx->writePending = ctx->writeCipher;
        ctx->writeCipher = ctx->prevCipher;
    }

    /* set timeout deadline */
    ctx->hdskMessageRetryCount++;
    ctx->timeout_deadline = CFAbsoluteTimeGetCurrent()+((1<<ctx->hdskMessageRetryCount)*ctx->timeout_duration);

    /* Lets resend the last flight */
    return SSLSendFlight(ctx);
}

static OSStatus
SSLProcessHandshakeMessage(SSLHandshakeMsg message, SSLContext *ctx)
{   OSStatus      err;

    err = noErr;
    SSLLogHdskMsg(message.type, 0);
    switch (message.type)
    {   case SSL_HdskHelloRequest:
            if (ctx->protocolSide != kSSLClientSide)
                goto wrongMessage;
            if (message.contents.length > 0)
                err = errSSLProtocol;
            break;
        case SSL_HdskClientHello:
            if (ctx->state != SSL_HdskStateServerUninit)
                goto wrongMessage;
            err = SSLProcessClientHello(message.contents, ctx);
            break;
        case SSL_HdskServerHello:
            if (ctx->state != SSL_HdskStateServerHello &&
                ctx->state != SSL_HdskStateServerHelloUnknownVersion)
                goto wrongMessage;
            err = SSLProcessServerHello(message.contents, ctx);
            break;
#if ENABLE_DTLS
        case SSL_HdskHelloVerifyRequest:
            if (ctx->protocolSide != kSSLClientSide)
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
        case SSL_HdskCertRequest:
            if (((ctx->state != SSL_HdskStateHelloDone) &&
			     (ctx->state != SSL_HdskStateKeyExchange))
                 || ctx->certRequested)
                goto wrongMessage;
            err = SSLProcessCertificateRequest(message.contents, ctx);
            if (ctx->breakOnCertRequest)
                ctx->signalCertRequest = true;
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
        case SSL_HdskServerHelloDone:
            if (ctx->state != SSL_HdskStateHelloDone)
                goto wrongMessage;
            err = SSLProcessServerHelloDone(message.contents, ctx);
            break;
        case SSL_HdskCertVerify:
            if (ctx->state != SSL_HdskStateClientCertVerify)
                goto wrongMessage;
            err = SSLProcessCertificateVerify(message.contents, ctx);
			assert(ctx->protocolSide == kSSLServerSide);
			if(err) {
				ctx->clientCertState = kSSLClientCertRejected;
			}
            break;
        case SSL_HdskClientKeyExchange:
            if (ctx->state != SSL_HdskStateClientKeyExchange)
                goto wrongMessage;
            err = SSLProcessKeyExchange(message.contents, ctx);
            break;
        case SSL_HdskFinished:
            if (ctx->state != SSL_HdskStateFinished)
                goto wrongMessage;
            err = SSLProcessFinished(message.contents, ctx);
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
        else if (err != errSSLWouldBlock &&
				 err != errSSLServerAuthCompleted /* == errSSLClientAuthCompleted */ &&
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
static OSStatus
SSLResumeServerSide(
	SSLContext *ctx)
{
	OSStatus err;
	if ((err = SSLPrepareAndQueueMessage(SSLEncodeServerHello, ctx)) != 0)
		return err;
	if ((err = SSLInitPendingCiphers(ctx)) != 0)
	{   SSLFatalSessionAlert(SSL_AlertInternalError, ctx);
		return err;
	}
	if ((err = SSLPrepareAndQueueMessage(SSLEncodeChangeCipherSpec,
				ctx)) != 0)
		return err;

	if ((err = SSLPrepareAndQueueMessage(SSLEncodeFinishedMessage,
			ctx)) != 0)
		return err;

	SSLChangeHdskState(ctx, SSL_HdskStateChangeCipherSpec);

	return noErr;

}

OSStatus
SSLAdvanceHandshake(SSLHandshakeType processed, SSLContext *ctx)
{   OSStatus        err;
    SSLBuffer       sessionIdentifier;

    SSLResetFlight(ctx);

    switch (processed)
    {
#if ENABLE_DTLS
        case SSL_HdskHelloVerifyRequest:
            /* Just fall through */
#endif
        case SSL_HdskHelloRequest:
			/*
			 * Reset the client auth state machine in case this is
			 * a renegotiation.
			 */
			ctx->certRequested = 0;
			ctx->certSent = 0;
			ctx->certReceived = 0;
			ctx->x509Requested = 0;
			ctx->clientCertState = kSSLClientCertNone;
			ctx->readCipher.ready = 0;
			ctx->writeCipher.ready = 0;
			ctx->wroteAppData = 0;
            if ((err = SSLPrepareAndQueueMessage(SSLEncodeClientHello, ctx)) != 0)
                return err;
            SSLChangeHdskState(ctx, SSL_HdskStateServerHello);
            break;
        case SSL_HdskClientHello:
            assert(ctx->protocolSide == kSSLServerSide);
			ctx->sessionMatch = 0;

            if((ctx->negProtocolVersion==DTLS_Version_1_0) && (ctx->cookieVerified==false))
            {   /* Send Hello Verify Request */
                if((err=SSLPrepareAndQueueMessage(SSLEncodeServerHelloVerifyRequest, ctx)) !=0 )
                    return err;
                break;
            }

			#if 	SSL_PAC_SERVER_ENABLE
			if((ctx->sessionTicket.data != NULL) &&
			   (ctx->masterSecretCallback != NULL)) {
				/*
				 * Client sent us a session ticket and we know how to ask
				 * the app for master secret. Go for it.
				 */
				size_t secretLen = SSL_MASTER_SECRET_SIZE;
				sslEapDebug("Server side resuming based on masterSecretCallback");

				/* the master secret callback requires serverRandom, now... */
			    if ((err = SSLEncodeRandom(ctx->serverRandom, ctx)) != 0)
       				 return err;
				ctx->serverRandomValid = 1;

				ctx->masterSecretCallback(ctx, ctx->masterSecretArg,
					ctx->masterSecret, &secretLen);
				ctx->sessionMatch = 1;
				/* set up selectedCipherSpec */
				if ((err = FindCipherSpec(ctx)) != 0) {
					return err;
				}
				/* queue up remaining messages to finish handshake */
				if((err = SSLResumeServerSide(ctx)) != 0)
					return err;
				break;
			}
			#endif	/* SSL_PAC_SERVER_ENABLE */
            if (ctx->sessionID.data != 0)
			/* If session ID != 0, client is trying to resume */
            {   if (ctx->resumableSession.data != 0)
                {
					SSLProtocolVersion sessionProt;
					if ((err = SSLRetrieveSessionID(ctx->resumableSession,
								&sessionIdentifier, ctx)) != 0)
                        return err;
					if ((err = SSLRetrieveSessionProtocolVersion(ctx->resumableSession,
							&sessionProt, ctx)) != 0)
					{   SSLFatalSessionAlert(SSL_AlertInternalError, ctx);
						return err;
					}
					if ((sessionIdentifier.length == ctx->sessionID.length) &&
                        (memcmp(sessionIdentifier.data, ctx->sessionID.data,
							ctx->sessionID.length) == 0) &&
					    (sessionProt == ctx->negProtocolVersion))
                    {   /* Everything matches; resume the session */
						sslLogResumSessDebug("===RESUMING SSL3 server-side session");
                        if ((err = SSLInstallSessionFromData(ctx->resumableSession,
								ctx)) != 0)
                        {   SSLFatalSessionAlert(SSL_AlertInternalError, ctx);
                            return err;
                        }
						ctx->sessionMatch = 1;
						SSLFreeBuffer(&sessionIdentifier, ctx);

						/* queue up remaining messages to finish handshake */
						if((err = SSLResumeServerSide(ctx)) != 0)
							return err;
                        break;
                    }
					else {
						sslLogResumSessDebug(
							"===FAILED TO RESUME SSL3 server-side session");
					}
                    if ((err = SSLFreeBuffer(&sessionIdentifier, ctx)) != 0 ||
                        (err = SSLDeleteSessionData(ctx)) != 0)
                    {   SSLFatalSessionAlert(SSL_AlertInternalError, ctx);
                        return err;
                    }
                }
                if ((err = SSLFreeBuffer(&ctx->sessionID, ctx)) != 0)
                {   SSLFatalSessionAlert(SSL_AlertInternalError, ctx);
                    return err;
                }
            }

            /*
			 * If we get here, we're not resuming; generate a new session ID
			 * if we know our peer
			 */
            if (ctx->peerID.data != 0)
            {   /* Ignore errors; just treat as uncached session */
                assert(ctx->sessionID.data == 0);
                err = SSLAllocBuffer(&ctx->sessionID, SSL_SESSION_ID_LEN, ctx);
                if (err == 0)
                {
                	if((err = sslRand(ctx, &ctx->sessionID)) != 0)
                    {   SSLFatalSessionAlert(SSL_AlertInternalError, ctx);
                        return err;
                    }
                }
            }

            if ((err = SSLPrepareAndQueueMessage(SSLEncodeServerHello, ctx)) != 0)
                return err;
            switch (ctx->selectedCipherSpec.keyExchangeMethod)
            {   case SSL_NULL_auth:
            	#if		APPLE_DH
                case SSL_DH_anon:
                case SSL_DH_anon_EXPORT:
                	if(ctx->clientAuth == kAlwaysAuthenticate) {
                		/* app requires this; abort */
                		SSLFatalSessionAlert(SSL_AlertHandshakeFail, ctx);
                		return errSSLNegotiation;
                	}
                	ctx->tryClientAuth = false;
					/* DH server side needs work */
                    break;
                #endif	/* APPLE_DH */
                default:        /* everything else */
					if(ctx->localCert == NULL) {
						/* no cert but configured for, and negotiated, a
						 * ciphersuite which requires one */
						sslErrorLog("SSLAdvanceHandshake: No server key!\n");
						return errSSLBadConfiguration;
					}
                    if ((err = SSLPrepareAndQueueMessage(SSLEncodeCertificate,
							ctx)) != 0)
                        return err;
                    break;
            }
			/*
			 * At this point we decide whether to send a server key exchange
			 * method. For Apple servers, I think we'll ALWAYS do this, because
			 * of key usage restrictions (can't decrypt and sign with the same
			 * private key), but conceptually in this code, we do it if
			 * enabled by the presence of encryptPrivKey.
			 */
			{
				bool doServerKeyExch = false;
				switch(ctx->selectedCipherSpec.keyExchangeMethod) {
					case SSL_RSA_EXPORT:
					#if !SSL_SERVER_KEYEXCH_HACK
					/* the "proper" way - app decides. */
					case SSL_RSA:
					#endif
						if(ctx->encryptPrivKeyRef != NULL) {
							doServerKeyExch = true;
						}
						break;
					case SSL_DH_anon:
					case SSL_DH_anon_EXPORT:
					case SSL_DHE_RSA:
					case SSL_DHE_RSA_EXPORT:
					case SSL_DHE_DSS:
					case SSL_DHE_DSS_EXPORT:
						doServerKeyExch = true;
						break;
					default:
						break;
				}
				if(doServerKeyExch) {
					err = SSLPrepareAndQueueMessage(SSLEncodeServerKeyExchange, ctx);
					if(err) {
						return err;
					}
				}
			}
            if (ctx->tryClientAuth)
            {   if ((err = SSLPrepareAndQueueMessage(SSLEncodeCertificateRequest,
						ctx)) != 0)
                    return err;
                ctx->certRequested = 1;
				ctx->clientCertState = kSSLClientCertRequested;
            }
            if ((err = SSLPrepareAndQueueMessage(SSLEncodeServerHelloDone, ctx)) != 0)
                return err;
            if (ctx->certRequested) {
                SSLChangeHdskState(ctx, SSL_HdskStateClientCert);
            }
            else {
                SSLChangeHdskState(ctx, SSL_HdskStateClientKeyExchange);
            }
            break;
        case SSL_HdskServerHello:
			ctx->sessionMatch = 0;
            if (ctx->resumableSession.data != 0 && ctx->sessionID.data != 0)
            {
				SSLProtocolVersion sessionProt;
				if ((err = SSLRetrieveSessionID(ctx->resumableSession,
						&sessionIdentifier, ctx)) != 0)
                {   SSLFatalSessionAlert(SSL_AlertInternalError, ctx);
                    return err;
                }
				if ((err = SSLRetrieveSessionProtocolVersion(ctx->resumableSession,
						&sessionProt, ctx)) != 0)
                {   SSLFatalSessionAlert(SSL_AlertInternalError, ctx);
                    return err;
                }
                if ((sessionIdentifier.length == ctx->sessionID.length) &&
                    (memcmp(sessionIdentifier.data, ctx->sessionID.data,
							ctx->sessionID.length) == 0) &&
					(sessionProt == ctx->negProtocolVersion))
                {   /* Everything matches; resume the session */
					sslLogResumSessDebug("===RESUMING SSL3 client-side session");
                    if ((err = SSLInstallSessionFromData(ctx->resumableSession,
							ctx)) != 0 ||
                        (err = SSLInitPendingCiphers(ctx)) != 0 ||
                        (err = SSLFreeBuffer(&sessionIdentifier, ctx)) != 0)
                    {   SSLFatalSessionAlert(SSL_AlertInternalError, ctx);
                        return err;
                    }
					ctx->sessionMatch = 1;
                    SSLChangeHdskState(ctx, SSL_HdskStateChangeCipherSpec);
                    break;
                }
				else {
					sslLogResumSessDebug("===FAILED TO RESUME SSL3 client-side "
							"session");
				}
                if ((err = SSLFreeBuffer(&sessionIdentifier, ctx)) != 0)
                {   SSLFatalSessionAlert(SSL_AlertInternalError, ctx);
                    return err;
                }
            }
            switch (ctx->selectedCipherSpec.keyExchangeMethod)
            {
            	/* these require a key exchange message */
            	case SSL_NULL_auth:
                case SSL_DH_anon:
                case SSL_DH_anon_EXPORT:
                    SSLChangeHdskState(ctx, SSL_HdskStateKeyExchange);
                    break;
                case SSL_RSA:
                case SSL_DH_DSS:
                case SSL_DH_DSS_EXPORT:
                case SSL_DH_RSA:
                case SSL_DH_RSA_EXPORT:
                case SSL_RSA_EXPORT:
                case SSL_DHE_DSS:
                case SSL_DHE_DSS_EXPORT:
                case SSL_DHE_RSA:
                case SSL_DHE_RSA_EXPORT:
                case SSL_Fortezza:
				case SSL_ECDH_ECDSA:
				case SSL_ECDHE_ECDSA:
				case SSL_ECDH_RSA:
				case SSL_ECDHE_RSA:
                    SSLChangeHdskState(ctx, SSL_HdskStateCert);
                    break;
                default:
                    assert("Unknown key exchange method");
                    break;
            }
            break;
        case SSL_HdskCert:
            if (ctx->state == SSL_HdskStateCert)
                switch (ctx->selectedCipherSpec.keyExchangeMethod)
                {   case SSL_RSA:
                 	/*
                	 * I really think the two RSA cases should be
                	 * handled the same here - the server key exchange is
                	 * optional, and is up to the server.
                	 * Note this isn't the same as SSL_SERVER_KEYEXCH_HACK;
                	 * we're a client here.
                	 */
                	case SSL_RSA_EXPORT:
                    case SSL_DH_DSS:
                    case SSL_DH_DSS_EXPORT:
                    case SSL_DH_RSA:
                    case SSL_DH_RSA_EXPORT:
					case SSL_ECDH_ECDSA:
					case SSL_ECDH_RSA:
                        SSLChangeHdskState(ctx, SSL_HdskStateHelloDone);
                        break;
                    case SSL_DHE_DSS:
                    case SSL_DHE_DSS_EXPORT:
                    case SSL_DHE_RSA:
                    case SSL_DHE_RSA_EXPORT:
                    case SSL_Fortezza:
					case SSL_ECDHE_ECDSA:
					case SSL_ECDHE_RSA:
                        SSLChangeHdskState(ctx, SSL_HdskStateKeyExchange);
                        break;
                    default:
                        assert("Unknown or unexpected key exchange method");
                        break;
                }
            else if (ctx->state == SSL_HdskStateClientCert)
            {   SSLChangeHdskState(ctx, SSL_HdskStateClientKeyExchange);
                if (ctx->peerCert != 0)
                    ctx->certReceived = 1;
            }
            break;
        case SSL_HdskCertRequest:
			/* state stays in SSL_HdskStateHelloDone; distinction is in
			 *  ctx->certRequested */
            if (ctx->peerCert == 0)
            {   SSLFatalSessionAlert(SSL_AlertHandshakeFail, ctx);
                return errSSLProtocol;
            }
            assert(ctx->protocolSide == kSSLClientSide);
            ctx->certRequested = 1;
            ctx->clientCertState = kSSLClientCertRequested;
            break;
        case SSL_HdskServerKeyExchange:
            SSLChangeHdskState(ctx, SSL_HdskStateHelloDone);
            break;
        case SSL_HdskServerHelloDone:
			/*
             * Waiting until server has sent hello done to interrupt and allow
             * setting client cert, so we can send certificate, keyexchange and
             * cert verify message together
			 */
            if (ctx->state != SSL_HdskStateClientCert) {
                if (ctx->signalServerAuth) {
                    ctx->signalServerAuth = false;
                    SSLChangeHdskState(ctx, SSL_HdskStateClientCert);
                    return errSSLServerAuthCompleted;
                } else if (ctx->signalCertRequest) {
                    ctx->signalCertRequest = false;
                    SSLChangeHdskState(ctx, SSL_HdskStateClientCert);
                    return errSSLClientCertRequested;
                } else if (ctx->signalClientAuth) {
                    ctx->signalClientAuth = false;
                    return errSSLClientAuthCompleted;
                }
            }

			if (ctx->clientCertState == kSSLClientCertRequested) {
				/*
				 * Server wants a client authentication cert - do
				 * we have one?
				 */
                if (ctx->localCert != 0 && ctx->x509Requested) {
					if ((err = SSLPrepareAndQueueMessage(SSLEncodeCertificate,
							ctx)) != 0) {
						return err;
					}
                }
                else {
					/* response for no cert depends on protocol version */
					if(ctx->negProtocolVersion >= TLS_Version_1_0) {
						/* TLS: send empty cert msg */
						if ((err = SSLPrepareAndQueueMessage(SSLEncodeCertificate,
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
            if ((err = SSLPrepareAndQueueMessage(SSLEncodeKeyExchange, ctx)) != 0)
                return err;
			assert(ctx->sslTslCalls != NULL);
            if ((err = ctx->sslTslCalls->generateMasterSecret(ctx)) != 0 ||
                (err = SSLInitPendingCiphers(ctx)) != 0)
            {   SSLFatalSessionAlert(SSL_AlertInternalError, ctx);
                return err;
            }
			memset(ctx->preMasterSecret.data, 0, ctx->preMasterSecret.length);
            if ((err = SSLFreeBuffer(&ctx->preMasterSecret, ctx)) != 0) {
                return err;
			}
            if (ctx->certSent) {
				/* Not all client auth mechanisms require a cert verify message */
				switch(ctx->negAuthType) {
					case SSLClientAuth_RSASign:
					case SSLClientAuth_ECDSASign:
						if ((err = SSLPrepareAndQueueMessage(SSLEncodeCertificateVerify,
								ctx)) != 0) {
							return err;
						}
						break;
					default:
						break;
				}
			}
            if ((err = SSLPrepareAndQueueMessage(SSLEncodeChangeCipherSpec,
					ctx)) != 0) {
                return err;
			}
            if ((err = SSLPrepareAndQueueMessage(SSLEncodeFinishedMessage, ctx)) != 0)
                return err;
            SSLChangeHdskState(ctx, SSL_HdskStateChangeCipherSpec);
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
            if ((err = SSLFreeBuffer(&ctx->preMasterSecret, ctx)) != 0)
                return err;
            if (ctx->certReceived) {
                SSLChangeHdskState(ctx, SSL_HdskStateClientCertVerify);
            }
            else {
                SSLChangeHdskState(ctx, SSL_HdskStateChangeCipherSpec);
            }
            break;
        case SSL_HdskFinished:
            /* Handshake is over; enable data transfer on read channel */
            ctx->readCipher.ready = 1;
            /* If writePending is set, we haven't yet sent a finished message;
			 * send it */
            if (ctx->writePending.ready != 0)
            {   if ((err = SSLPrepareAndQueueMessage(SSLEncodeChangeCipherSpec,
						ctx)) != 0)
                    return err;
                if ((err = SSLPrepareAndQueueMessage(SSLEncodeFinishedMessage,
							ctx)) != 0)
                    return err;
            }
            if (ctx->protocolSide == kSSLServerSide) {
                SSLChangeHdskState(ctx, SSL_HdskStateServerReady);
            }
            else {
                SSLChangeHdskState(ctx, SSL_HdskStateClientReady);
            }
            if ((ctx->peerID.data != 0) && (ctx->sessionTicket.data == NULL)) {
				/* note we avoid caching session data for PAC-style resumption */
                SSLAddSessionData(ctx);
			}
            break;
        default:
            assert(0);
            break;
    }

    /* We should have a full flight when we reach here, sending it for the first time */
    ctx->hdskMessageRetryCount = 0;
    ctx->timeout_deadline = CFAbsoluteTimeGetCurrent() + ctx->timeout_duration;
    return SSLSendFlight(ctx);
}

OSStatus
SSLPrepareAndQueueMessage(EncodeMessageFunc msgFunc, SSLContext *ctx)
{   OSStatus        err;
    SSLRecord       rec = {0, 0, {0, NULL}};
    WaitingMessage  *out;
    WaitingMessage  *queue;

    if ((err = msgFunc(&rec, ctx)) != 0)
    {   SSLFatalSessionAlert(SSL_AlertCloseNotify, ctx);
        goto fail;
    }

    if (rec.contentType == SSL_RecordTypeHandshake)
    {
        if ((err = SSLUpdateHandshakeMacs(&rec.contents, ctx)) != 0)
        {   SSLFatalSessionAlert(SSL_AlertInternalError, ctx);
            goto fail;
        }
        SSLLogHdskMsg((SSLHandshakeType)rec.contents.data[0], 1);
        ctx->hdskMessageSeq++;
    }

    err=errSSLInternal;
    out = (WaitingMessage *)sslMalloc(sizeof(WaitingMessage));
    if(out==NULL) goto fail;

    out->next = NULL;
	out->rec = rec;

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

    return noErr;
fail:
    SSLFreeBuffer(&rec.contents, ctx);
    return err;
}

static
OSStatus SSLSendMessage(SSLRecord rec, SSLContext *ctx)
{
    OSStatus err;

    assert(ctx->sslTslCalls != NULL);

    if ((err = ctx->sslTslCalls->writeRecord(rec, ctx)) != 0)
        return err;
    if(rec.contentType == SSL_RecordTypeChangeCipher) {
        /* Install new cipher spec on write side */
        if ((err = SSLDisposeCipherSuite(&ctx->writeCipher, ctx)) != 0)
        {   SSLFatalSessionAlert(SSL_AlertInternalError, ctx);
            return err;
        }
        ctx->prevCipher = ctx->writeCipher;
        ctx->writeCipher = ctx->writePending;
        /* Can't send data until Finished is sent */
        ctx->writeCipher.ready = 0;
		ctx->wroteAppData = 0;

        /* Zero out old data */
        memset(&ctx->writePending, 0, sizeof(CipherContext));
        ctx->writePending.encrypting = 1;

        /* TODO: that should only happen after Finished message is sent. <rdar://problem/9682471> */
        ctx->writeCipher.ready = 1;
    }

    return noErr;
}

static
OSStatus DTLSSendMessage(SSLRecord rec, SSLContext *ctx)
{
    OSStatus err=noErr;

    assert(ctx->sslTslCalls != NULL);

    if(rec.contentType != SSL_RecordTypeHandshake) {
        sslHdskMsgDebug("Not fragmenting message type=%d len=%d\n", rec.contentType, rec.contents.length);
        if ((err = ctx->sslTslCalls->writeRecord(rec, ctx)) != 0)
            return err;
        if(rec.contentType == SSL_RecordTypeChangeCipher) {
            /* Install new cipher spec on write side */
            if ((err = SSLDisposeCipherSuite(&ctx->writeCipher, ctx)) != 0)
            {   SSLFatalSessionAlert(SSL_AlertInternalError, ctx);
                return err;
            }
            ctx->prevCipher = ctx->writeCipher;
            ctx->writeCipher = ctx->writePending;
			/* Can't send data until Finished is sent */
            ctx->writeCipher.ready = 0;
			ctx->wroteAppData = 0;

			/* Zero out old data */
            memset(&ctx->writePending, 0, sizeof(CipherContext));
			ctx->writePending.encrypting = 1;

            /* TODO: that should only happen after Finished message is sent. See <rdar://problem/9682471> */
            ctx->writeCipher.ready = 1;

        }
    } else {
    /* fragmenting */
        SSLRecord fragrec;

        int msghead = 12;  /* size of message header in DTLS */
        size_t fraglen;
        size_t len = rec.contents.length-msghead;
        UInt32 seq = SSLDecodeInt(rec.contents.data+4, 2);
        (void) seq; // Suppress warnings
        size_t ofs = 0;

        sslHdskMsgDebug("Fragmenting msg seq %ld (rl=%d, ml=%d)", seq, rec.contents.length,
                        SSLDecodeInt(rec.contents.data+1, 3));


        SSLGetDatagramWriteSize(ctx, &fraglen);
        fraglen -=  msghead;

        fragrec.contentType = rec.contentType;
        fragrec.protocolVersion = rec.protocolVersion;
        if((err=SSLAllocBuffer(&fragrec.contents, fraglen + msghead, ctx))!=0)
            return err;

        /* copy the constant part of the header */
        memcpy(fragrec.contents.data,rec.contents.data, 6);

        while(len>fraglen) {

            sslHdskMsgDebug("Fragmenting msg seq %ld (o=%d,l=%d)", seq, ofs, fraglen);

            /* fragment offset and fragment length */
            SSLEncodeSize(fragrec.contents.data+6, ofs, 3);
            SSLEncodeSize(fragrec.contents.data+9, fraglen, 3);
            /* copy the payload */
            memcpy(fragrec.contents.data+msghead, rec.contents.data+msghead+ofs, fraglen);
            if ((err = ctx->sslTslCalls->writeRecord(fragrec, ctx)) != 0)
                goto cleanup;
            len-=fraglen;
            ofs+=fraglen;
        }

        sslHdskMsgDebug("Fragmenting msg seq %ld - Last Fragment (o=%d,l=%d)", seq, ofs, len);

        /* last fragment */
        /* fragment offset and fragment length */
        SSLEncodeSize(fragrec.contents.data+6, ofs, 3);
        SSLEncodeSize(fragrec.contents.data+9, len, 3);
        /* copy the payload */
        memcpy(fragrec.contents.data+msghead, rec.contents.data+msghead+ofs, len);
        fragrec.contents.length=len+msghead;
        err = ctx->sslTslCalls->writeRecord(fragrec, ctx);

    cleanup:
        /* Free the allocated fragment buffer */
        SSLFreeBuffer(&fragrec.contents, ctx);

    }

    return err;
}


OSStatus SSLResetFlight(SSLContext *ctx)
{
    OSStatus err;
    WaitingMessage *queue;
    WaitingMessage *next;
    int n=0;

    assert(ctx->sslTslCalls != NULL);

    queue=ctx->messageWriteQueue;
    ctx->messageQueueContainsChangeCipherSpec=false;

    while(queue) {
        n++;
        err = SSLFreeBuffer(&queue->rec.contents, ctx);
        if (err != 0)
            goto fail;
        next=queue->next;
        sslFree(queue);
        queue=next;
    }

    ctx->messageWriteQueue=NULL;

    return noErr;
fail:
    assert(0);
    return err;
}


OSStatus SSLSendFlight(SSLContext *ctx)
{
    OSStatus err;
    WaitingMessage  *queue;
    int n=0;

    assert(ctx->sslTslCalls != NULL);

    queue=ctx->messageWriteQueue;

    while(queue) {
        if (ctx->isDTLS) {
            err=DTLSSendMessage(queue->rec, ctx);
        } else {
            err=SSLSendMessage(queue->rec, ctx);
        }
        if (err != 0)
            goto fail;
        queue=queue->next;
        n++;
    }

    return noErr;
fail:
    assert(0);
    return err;
}

OSStatus
SSL3ReceiveSSL2ClientHello(SSLRecord rec, SSLContext *ctx)
{   OSStatus      err;

    if ((err = SSLInitMessageHashes(ctx)) != 0)
        return err;

    if ((err = SSLHashSHA1.update(&ctx->shaState, &rec.contents)) != 0 ||
        (err = SSLHashMD5.update(&ctx->md5State, &rec.contents)) != 0)
    {   SSLFatalSessionAlert(SSL_AlertInternalError, ctx);
        return err;
    }

    if ((err = SSLAdvanceHandshake(SSL_HdskClientHello, ctx)) != 0)
        return err;

    return noErr;
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
		case SSL_HdskStateServerHelloUnknownVersion:
			return "ServerHelloUnknownVersion";
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
		case SSL_HdskStateChangeCipherSpec:
			return "ChangeCipherSpec";
		case SSL_HdskStateFinished:
			return "Finished";
		case SSL2_HdskStateClientMasterKey:
			return "SSL2_ClientMasterKey";
		case SSL2_HdskStateClientFinished:
			return "SSL2_ClientFinished";
		case SSL2_HdskStateServerHello:
			return "SSL2_ServerHello";
		case SSL2_HdskStateServerVerify:
			return "SSL2_ServerVerify";
		case SSL2_HdskStateServerFinished:
			return "SSL2_ServerFinished";
		case SSL_HdskStateServerReady:
			return "SSL_ServerReady";
		case SSL_HdskStateClientReady:
			return "SSL_ClientReady";
		default:
			sprintf(badStr, "Unknown state (%d(d)", state);
			return badStr;
	}
}

void SSLChangeHdskState(SSLContext *ctx, SSLHandshakeState newState)
{
	/* FIXME - this ifndef should not be necessary */
	#ifndef	NDEBUG
	sslHdskStateDebug("...hdskState = %s", hdskStateToStr(newState));
	#endif
	ctx->state = newState;
}


/* log handshake messages */

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
		default:
			sprintf(badStr, "Unknown msg (%d(d))", msg);
			return badStr;
	}
}

void SSLLogHdskMsg(SSLHandshakeType msg, char sent)
{
	sslHdskMsgDebug("---%s handshake msg %s",
		hdskMsgToStr(msg), (sent ? "sent" : "recv"));
}

#endif	/* NDEBUG */


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
	File:		sslhdshk.c

	Contains:	SSL 3.0 handshake state machine. 

	Written by:	Doug Mitchell, based on Netscape RSARef 3.0

	Copyright: (c) 1999 by Apple Computer, Inc., all rights reserved.

*/
/*  *********************************************************************
    File: sslhdshk.c

    SSLRef 3.0 Final -- 11/19/96

    Copyright (c)1996 by Netscape Communications Corp.

    By retrieving this software you are bound by the licensing terms
    disclosed in the file "LICENSE.txt". Please read it, and if you don't
    accept the terms, delete this software.

    SSLRef 3.0 was developed by Netscape Communications Corp. of Mountain
    View, California <http://home.netscape.com/> and Consensus Development
    Corporation of Berkeley, California <http://www.consensus.com/>.

    *********************************************************************

    File: sslhdshk.c   SSL 3.0 handshake state machine

    Support for SSL Handshake messages, including extracting handshake
    messages from record layer records, processing those messages
    (including verifying their appropriateness) and then advancing the
    handshake by generating response messages and/or changing the state
    such that different messages are expected. In addition, controls when
    keys are generated.

    ****************************************************************** */

#ifndef _SSLCTX_H_
#include "sslctx.h"
#endif

#ifndef _SSLHDSHK_H_
#include "sslhdshk.h"
#endif

#ifndef _SSLALLOC_H_
#include "sslalloc.h"
#endif

#ifndef _SSLALERT_H_
#include "sslalert.h"
#endif

#ifndef _SSLSESS_H_
#include "sslsess.h"
#endif

#ifndef _SSLUTIL_H_
#include "sslutil.h"
#endif

#ifndef	_SSL_DEBUG_H_
#include "sslDebug.h"
#endif

#ifndef	_APPLE_CDSA_H_
#include "appleCdsa.h"
#endif

#include <string.h>

#define REQUEST_CERT_CORRECT        0

static SSLErr SSLProcessHandshakeMessage(SSLHandshakeMsg message, SSLContext *ctx);

SSLErr
SSLProcessHandshakeRecord(SSLRecord rec, SSLContext *ctx)
{   SSLErr          err;
    sint32          remaining;
    UInt8           *p;
    SSLHandshakeMsg message;
    SSLBuffer       messageData;
    
    if (ctx->fragmentedMessageCache.data != 0)
    {   if ((err = SSLReallocBuffer(&ctx->fragmentedMessageCache,
                    ctx->fragmentedMessageCache.length + rec.contents.length,
                    &ctx->sysCtx)) != 0)
        {   ERR(SSLFatalSessionAlert(alert_close_notify, ctx));
            return ERR(err);
        }
        memcpy(ctx->fragmentedMessageCache.data + ctx->fragmentedMessageCache.length,
            rec.contents.data, rec.contents.length);
        remaining = ctx->fragmentedMessageCache.length;
        p = ctx->fragmentedMessageCache.data;
    }
    else
    {   remaining = rec.contents.length;
        p = rec.contents.data;
    }

    while (remaining > 0)
    {   if (remaining < 4)
            break;  /* we must have at least a header */
        
        messageData.data = p;
        message.type = (SSLHandshakeType)*p++;
        message.contents.length = SSLDecodeInt(p, 3);
        if ((message.contents.length + 4) > remaining)
            break;
        
        p += 3;
        message.contents.data = p;
        p += message.contents.length;
        messageData.length = 4 + message.contents.length;
        CASSERT(p == messageData.data + messageData.length);
        
        /* message fragmentation */
        remaining -= messageData.length;
        if (ERR(err = SSLProcessHandshakeMessage(message, ctx)) != 0)
            return err;
        
        if (message.type != SSL_hello_request)
        {   if (ERR(err = SSLHashSHA1.update(ctx->shaState, messageData)) != 0 ||
                ERR(err = SSLHashMD5.update(ctx->md5State, messageData)) != 0)
            {   ERR(SSLFatalSessionAlert(alert_close_notify, ctx));
                return err;
            }
        }
        
        if (ERR(err = SSLAdvanceHandshake(message.type, ctx)) != 0)
            return err;
    }
    
    if (remaining > 0)      /* Fragmented handshake message */
    {   /* If there isn't a cache, allocate one */
        if (ctx->fragmentedMessageCache.data == 0)
        {   if (ERR(err = SSLAllocBuffer(&ctx->fragmentedMessageCache, remaining, &ctx->sysCtx)) != 0)
            {   ERR(SSLFatalSessionAlert(alert_close_notify, ctx));
                return err;
            }
        }
        if (p != ctx->fragmentedMessageCache.data)
        {   memcpy(ctx->fragmentedMessageCache.data, p, remaining);
            ctx->fragmentedMessageCache.length = remaining;
        }
    }
    else if (ctx->fragmentedMessageCache.data != 0)
    {   if (ERR(err = SSLFreeBuffer(&ctx->fragmentedMessageCache, &ctx->sysCtx)) != 0)
        {   ERR(SSLFatalSessionAlert(alert_close_notify, ctx));
            return err;
        }
    }
    
    return SSLNoErr;
}

static SSLErr
SSLProcessHandshakeMessage(SSLHandshakeMsg message, SSLContext *ctx)
{   SSLErr      err;
    
    err = SSLNoErr;
    SSLLogHdskMsg(message.type, 0);
    switch (message.type)
    {   case SSL_hello_request:
            if (ctx->protocolSide != SSL_ClientSide)
                goto wrongMessage;
            if (message.contents.length > 0)
                err = ERR(SSLProtocolErr);
            break;
        case SSL_client_hello:
            if (ctx->state != HandshakeServerUninit)
                goto wrongMessage;
            ERR(err = SSLProcessClientHello(message.contents, ctx));
            break;
        case SSL_server_hello:
            if (ctx->state != HandshakeServerHello &&
                ctx->state != HandshakeServerHelloUnknownVersion)
                goto wrongMessage;
            ERR(err = SSLProcessServerHello(message.contents, ctx));
            break;
        case SSL_certificate:
            if (ctx->state != HandshakeCertificate &&
                ctx->state != HandshakeClientCertificate)
                goto wrongMessage;
            ERR(err = SSLProcessCertificate(message.contents, ctx));
            break;
        case SSL_certificate_request:
            if ((ctx->state != HandshakeHelloDone && ctx->state != HandshakeKeyExchange)
                 || ctx->certRequested)
                goto wrongMessage;
            ERR(err = SSLProcessCertificateRequest(message.contents, ctx));
            break;
        case SSL_server_key_exchange:
             #if _APPLE_CDSA_
       		/* 
        	 * Since this message is optional, and completely at the
        	 * server's discretion, we need to be able to handle this
        	 * in one of two states...
        	 */
        	switch(ctx->state) {
        		case HandshakeKeyExchange:	/* explicitly waiting for this */
        		case HandshakeHelloDone:
        			break;
        		default:
                	goto wrongMessage;
        	}
        	#else
            if (ctx->state != HandshakeKeyExchange)
                goto wrongMessage;
            #endif	/* _APPLE_CDSA_ */
            ERR(err = SSLProcessServerKeyExchange(message.contents, ctx));
            break;
        case SSL_server_hello_done:
            if (ctx->state != HandshakeHelloDone)
                goto wrongMessage;
            ERR(err = SSLProcessServerHelloDone(message.contents, ctx));
            break;
        case SSL_certificate_verify:
            if (ctx->state != HandshakeClientCertVerify)
                goto wrongMessage;
            ERR(err = SSLProcessCertificateVerify(message.contents, ctx));
            break;
        case SSL_client_key_exchange:
            if (ctx->state != HandshakeClientKeyExchange)
                goto wrongMessage;
            ERR(err = SSLProcessKeyExchange(message.contents, ctx));
            break;
        case SSL_finished:
            if (ctx->state != HandshakeFinished)
                goto wrongMessage;
            ERR(err = SSLProcessFinished(message.contents, ctx));
            break;
        default:
            goto wrongMessage;
            break;
    }
    
    if (err)
    {   if (err == SSLProtocolErr)
            ERR(SSLFatalSessionAlert(alert_illegal_parameter, ctx));
        else if (err == SSLNegotiationErr)
            ERR(SSLFatalSessionAlert(alert_handshake_failure, ctx));
        else
            ERR(SSLFatalSessionAlert(alert_close_notify, ctx));
    }
    return ERR(err);
    
wrongMessage:
    ERR(SSLFatalSessionAlert(alert_unexpected_message, ctx));
    return ERR(SSLProtocolErr);
}

SSLErr
SSLAdvanceHandshake(SSLHandshakeType processed, SSLContext *ctx)
{   SSLErr          err;
    SSLBuffer       sessionIdentifier;
    
    switch (processed)
    {   case SSL_hello_request:
            if (ERR(err = SSLPrepareAndQueueMessage(SSLEncodeClientHello, ctx)) != 0)
                return err;
            SSLChangeHdskState(ctx, HandshakeServerHello);
            break;
        case SSL_client_hello:
            CASSERT(ctx->protocolSide == SSL_ServerSide);
            if (ctx->sessionID.data != 0)   /* If session ID != 0, client is trying to resume */
            {   if (ctx->resumableSession.data != 0)
                {   if (ERR(err = SSLRetrieveSessionIDIdentifier(ctx->resumableSession, &sessionIdentifier, ctx)) != 0)
                        return err;
                    if (sessionIdentifier.length == ctx->sessionID.length &&
                        memcmp(sessionIdentifier.data, ctx->sessionID.data, ctx->sessionID.length) == 0)
                    {   /* Everything matches; resume the session */
                        //DEBUGMSG("Using resumed SSL3 Session");
                        if (ERR(err = SSLInstallSessionID(ctx->resumableSession, ctx)) != 0)
                        {   ERR(SSLFatalSessionAlert(alert_close_notify, ctx));
                            return err;
                        }
                        if (ERR(err = SSLPrepareAndQueueMessage(SSLEncodeServerHello, ctx)) != 0)
                            return err;
                        if (ERR(err = SSLInitPendingCiphers(ctx)) != 0 ||
                            ERR(err = SSLFreeBuffer(&sessionIdentifier, &ctx->sysCtx)) != 0)
                        {   ERR(SSLFatalSessionAlert(alert_close_notify, ctx));
                            return err;
                        }
                        if (ERR(err = SSLPrepareAndQueueMessage(SSLEncodeChangeCipherSpec, ctx)) != 0)
                            return err;
                        /* Install new cipher spec on write side */
                        if (ERR(err = SSLDisposeCipherSuite(&ctx->writeCipher, ctx)) != 0)
                        {   ERR(SSLFatalSessionAlert(alert_close_notify, ctx));
                            return err;
                        }
                        ctx->writeCipher = ctx->writePending;
                        ctx->writeCipher.ready = 0;     /* Can't send data until Finished is sent */
                        memset(&ctx->writePending, 0, sizeof(CipherContext));       /* Zero out old data */
                        if (ERR(err = SSLPrepareAndQueueMessage(SSLEncodeFinishedMessage, ctx)) != 0)
                            return err;
                        /* Finished has been sent; enable data dransfer on write channel */
                        ctx->writeCipher.ready = 1;
                        SSLChangeHdskState(ctx, HandshakeChangeCipherSpec);
                        break;
                    }
                    if (ERR(err = SSLFreeBuffer(&sessionIdentifier, &ctx->sysCtx)) != 0 ||
                        ERR(err = SSLDeleteSessionID(ctx)) != 0)
                    {   ERR(SSLFatalSessionAlert(alert_close_notify, ctx));
                        return err;
                    }
                }
                if (ERR(err = SSLFreeBuffer(&ctx->sessionID, &ctx->sysCtx)) != 0)
                {   ERR(SSLFatalSessionAlert(alert_close_notify, ctx));
                    return err;
                }
            }
            
            /* If we get here, we're not resuming; generate a new session ID if we know our peer */
            if (ctx->peerID.data != 0)
            {   /* Ignore errors; just treat as uncached session */
                CASSERT(ctx->sessionID.data == 0);
                ERR(err = SSLAllocBuffer(&ctx->sessionID, SSL_SESSION_ID_LEN, &ctx->sysCtx));
                if (err == 0)
                {   
                	#ifdef	_APPLE_CDSA_
                	if((err = sslRand(ctx, &ctx->sessionID)) != 0)
                	#else
                	if (ERR(err = ctx->sysCtx.random(ctx->sessionID, ctx->sysCtx.randomRef)) != 0)
                	#endif
                    {   ERR(SSLFatalSessionAlert(alert_close_notify, ctx));
                        return err;
                    }
                }
            }
            
            if (ERR(err = SSLPrepareAndQueueMessage(SSLEncodeServerHello, ctx)) != 0)
                return err;
            switch (ctx->selectedCipherSpec->keyExchangeMethod)
            {   case SSL_NULL_auth:
            	#if		APPLE_DH
                case SSL_DH_anon:
                #endif
                case SSL_DH_anon_EXPORT:
					#if		ST_SERVER_MODE_ENABLE
                	if(ctx->clientAuth == kAlwaysAuthenticate) {
                		/* APPLE_CDSA change: app requires this; abort */
                		SSLFatalSessionAlert(alert_handshake_failure, ctx);
                		return SSLNegotiationErr;
                	}
                	ctx->tryClientAuth = false;
					#else	/* ST_SERVER_MODE_ENABLE */
					/* server side needs work */
					#endif	/* ST_SERVER_MODE_ENABLE*/
                    break;
                default:        /* everything else */
                    if (ERR(err = SSLPrepareAndQueueMessage(SSLEncodeCertificate, ctx)) != 0)
                        return err;
                    break;
            }
            #ifdef	_APPLE_CDSA_
		        /*
	             * At this point we decide whether to send a server key exchange
	             * method. For Apple servers, I think we'll ALWAYS do this, because
	             * of key usage restrictions (can't decrypt and sign with the same
	             * private key), but conceptually in this code, we do it if 
	             * enabled by the presence of encryptPrivKey. 
	             */
	            #if		SSL_SERVER_KEYEXCH_HACK	
	            	/*
	            	 * This is currently how we work with Netscape. It requires
	            	 * a CSP which can handle private keys which can both
	            	 * sign and decrypt. 
	            	 */
	            	if((ctx->selectedCipherSpec->keyExchangeMethod != SSL_RSA) &&
	            	   (ctx->encryptPrivKey != NULL)) {
			        	err = SSLPrepareAndQueueMessage(SSLEncodeServerKeyExchange, ctx);
			        	if(err) {
							return err;
						}
	            	}
	            #else	/* !SSL_SERVER_KEYEXCH_HACK */
	            	/*
	            	 * This is, I believe the "right" way, but Netscape doesn't
	            	 * work this way.
	            	 */
		            if (ctx->encryptPrivKey != NULL) {
			        	err = SSLPrepareAndQueueMessage(SSLEncodeServerKeyExchange, ctx);
			        	if(err) {
							return err;
						}
					}
				#endif	/* SSL_SERVER_KEYEXCH_HACK */
            #else	/* !_APPLE_CDSA_ */
	            /* original SSLRef3.... */
	            if (ctx->selectedCipherSpec->keyExchangeMethod != SSL_RSA)
	                if (ERR(err = SSLPrepareAndQueueMessage(SSLEncodeServerKeyExchange, ctx)) != 0)
	                    return err;
            #endif	/* _APPLE_CDSA_ */
			#if	ST_SERVER_MODE_ENABLE
            if (ctx->tryClientAuth)
            {   if (ERR(err = SSLPrepareAndQueueMessage(SSLEncodeCertificateRequest, ctx)) != 0)
                    return err;
                ctx->certRequested = 1;
            }
			#else	/* !ST_SERVER_MODE_ENABLE */
			/* disabled for now */
			#endif	/* ST_SERVER_MODE_ENABLE */
            if (ERR(err = SSLPrepareAndQueueMessage(SSLEncodeServerHelloDone, ctx)) != 0)
                return err;
            if (ctx->certRequested) {
                SSLChangeHdskState(ctx, HandshakeClientCertificate);
            }
            else {
                SSLChangeHdskState(ctx, HandshakeClientKeyExchange);
            }
            break;
        case SSL_server_hello:
            if (ctx->resumableSession.data != 0 && ctx->sessionID.data != 0)
            {   if (ERR(err = SSLRetrieveSessionIDIdentifier(ctx->resumableSession, &sessionIdentifier, ctx)) != 0)
                {   ERR(SSLFatalSessionAlert(alert_close_notify, ctx));
                    return err;
                }
                if (sessionIdentifier.length == ctx->sessionID.length &&
                    memcmp(sessionIdentifier.data, ctx->sessionID.data, ctx->sessionID.length) == 0)
                {   /* Everything matches; resume the session */
                    if (ERR(err = SSLInstallSessionID(ctx->resumableSession, ctx)) != 0 ||
                        ERR(err = SSLInitPendingCiphers(ctx)) != 0 ||
                        ERR(err = SSLFreeBuffer(&sessionIdentifier, &ctx->sysCtx)) != 0)
                    {   ERR(SSLFatalSessionAlert(alert_close_notify, ctx));
                        return err;
                    }
                    SSLChangeHdskState(ctx, HandshakeChangeCipherSpec);
                    break;
                }
                if (ERR(err = SSLFreeBuffer(&sessionIdentifier, &ctx->sysCtx)) != 0)
                {   ERR(SSLFatalSessionAlert(alert_close_notify, ctx));
                    return err;
                }
            }
            switch (ctx->selectedCipherSpec->keyExchangeMethod)
            {   
            	/* these require a key exchange message */
            	case SSL_NULL_auth:
                case SSL_DH_anon:
                case SSL_DH_anon_EXPORT:
                    SSLChangeHdskState(ctx, HandshakeKeyExchange);
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
                    SSLChangeHdskState(ctx, HandshakeCertificate);
                    break;
                default:
                    ASSERTMSG("Unknown key exchange method");
                    break;
            }
            break;
        case SSL_certificate:
            if (ctx->state == HandshakeCertificate)
                switch (ctx->selectedCipherSpec->keyExchangeMethod)
                {   case SSL_RSA:
                	#ifdef	_APPLE_CDSA_
                 	/*
                	 * I really think the two RSA cases should be
                	 * handled the same here - the server key exchange is
                	 * optional, and is up to the server.
                	 * Note this isn't the same as SSL_SERVER_KEYEXCH_HACK;
                	 * we're a client here.
                	 */                   
                	case SSL_RSA_EXPORT:
                    #endif
                    case SSL_DH_DSS:
                    case SSL_DH_DSS_EXPORT:
                    case SSL_DH_RSA:
                    case SSL_DH_RSA_EXPORT:
                        SSLChangeHdskState(ctx, HandshakeHelloDone);
                        break;
                	#ifndef	_APPLE_CDSA_
                    case SSL_RSA_EXPORT:
                    #endif
                    case SSL_DHE_DSS:
                    case SSL_DHE_DSS_EXPORT:
                    case SSL_DHE_RSA:
                    case SSL_DHE_RSA_EXPORT:
                    case SSL_Fortezza:
                        SSLChangeHdskState(ctx, HandshakeKeyExchange);
                        break;
                    default:
                        ASSERTMSG("Unknown or unexpected key exchange method");
                        break;
                }
            else if (ctx->state == HandshakeClientCertificate)
            {   SSLChangeHdskState(ctx, HandshakeClientKeyExchange);
                if (ctx->peerCert != 0)
                    ctx->certReceived = 1;
            }
            break;
        case SSL_certificate_request:   /* state stays in HandshakeHelloDone; distinction is in ctx->certRequested */
            if (ctx->peerCert == 0)
            {   ERR(SSLFatalSessionAlert(alert_handshake_failure, ctx));
                return ERR(SSLProtocolErr);
            }
            ctx->certRequested = 1;
            break;
        case SSL_server_key_exchange:
            SSLChangeHdskState(ctx, HandshakeHelloDone);
            break;
        case SSL_server_hello_done:
            if (ctx->certRequested)
            {   if (ctx->localCert != 0 && ctx->x509Requested)
                {   if (ERR(err = SSLPrepareAndQueueMessage(SSLEncodeCertificate, ctx)) != 0)
                        return err;
                }
                else
                {   if (ERR(err = SSLSendAlert(alert_warning, alert_no_certificate, ctx)) != 0)
                        return err;
                }
            }
            if (ERR(err = SSLPrepareAndQueueMessage(SSLEncodeKeyExchange, ctx)) != 0)
                return err;
            if (ERR(err = SSLCalculateMasterSecret(ctx)) != 0 ||
                ERR(err = SSLInitPendingCiphers(ctx)) != 0)
            {   ERR(SSLFatalSessionAlert(alert_close_notify, ctx));
                return err;
            }
            if (ERR(err = SSLFreeBuffer(&ctx->preMasterSecret, &ctx->sysCtx)) != 0)
                return err;
            if (ctx->certSent)
                if (ERR(err = SSLPrepareAndQueueMessage(SSLEncodeCertificateVerify, ctx)) != 0)
                    return err;
            if (ERR(err = SSLPrepareAndQueueMessage(SSLEncodeChangeCipherSpec, ctx)) != 0)
                return err;
            /* Install new cipher spec on write side */
            if (ERR(err = SSLDisposeCipherSuite(&ctx->writeCipher, ctx)) != 0)
            {   ERR(SSLFatalSessionAlert(alert_close_notify, ctx));
                return err;
            }
            ctx->writeCipher = ctx->writePending;
            ctx->writeCipher.ready = 0;     /* Can't send data until Finished is sent */
            memset(&ctx->writePending, 0, sizeof(CipherContext));       /* Zero out old data */
            if (ERR(err = SSLPrepareAndQueueMessage(SSLEncodeFinishedMessage, ctx)) != 0)
                return err;
            /* Finished has been sent; enable data dransfer on write channel */
            ctx->writeCipher.ready = 1;
            SSLChangeHdskState(ctx, HandshakeChangeCipherSpec);
            break;
        case SSL_certificate_verify:
            SSLChangeHdskState(ctx, HandshakeChangeCipherSpec);
            break;
        case SSL_client_key_exchange:
            if (ERR(err = SSLCalculateMasterSecret(ctx)) != 0 ||
                ERR(err = SSLInitPendingCiphers(ctx)) != 0)
            {   ERR(SSLFatalSessionAlert(alert_close_notify, ctx));
                return err;
            }
            if (ERR(err = SSLFreeBuffer(&ctx->preMasterSecret, &ctx->sysCtx)) != 0)
                return err;
            if (ctx->certReceived) {
                SSLChangeHdskState(ctx, HandshakeClientCertVerify);
            }
            else {
                SSLChangeHdskState(ctx, HandshakeChangeCipherSpec);
            }
            break;
        case SSL_finished:
            /* Handshake is over; enable data transfer on read channel */
            ctx->readCipher.ready = 1;
            /* If writePending is set, we haven't yet sent a finished message; send it */
            if (ctx->writePending.ready != 0)
            {   if (ERR(err = SSLPrepareAndQueueMessage(SSLEncodeChangeCipherSpec, ctx)) != 0)
                    return err;
                
                /* Install new cipher spec on write side */
                if (ERR(err = SSLDisposeCipherSuite(&ctx->writeCipher, ctx)) != 0)
                {   SSLFatalSessionAlert(alert_close_notify, ctx);
                    return err;
                }
                ctx->writeCipher = ctx->writePending;
                ctx->writeCipher.ready = 0;     /* Can't send data until Finished is sent */
                memset(&ctx->writePending, 0, sizeof(CipherContext));       /* Zero out old data */
                if (ERR(err = SSLPrepareAndQueueMessage(SSLEncodeFinishedMessage, ctx)) != 0)
                    return err;
                ctx->writeCipher.ready = 1;
            }
            if (ctx->protocolSide == SSL_ServerSide) {
                SSLChangeHdskState(ctx, HandshakeServerReady);
            }
            else {
                SSLChangeHdskState(ctx, HandshakeClientReady);
            }
            if (ctx->peerID.data != 0)
                ERR(SSLAddSessionID(ctx));
            break;
        default:
            ASSERTMSG("Unknown State");
            break;
    }
    
    return SSLNoErr;
}

SSLErr
SSLPrepareAndQueueMessage(EncodeMessageFunc msgFunc, SSLContext *ctx)
{   SSLErr          err;
    SSLRecord       rec;
    
    if (ERR(err = msgFunc(&rec, ctx)) != 0)
    {   ERR(SSLFatalSessionAlert(alert_close_notify, ctx));
        goto fail;
    }
    
    if (rec.contentType == SSL_handshake)
    {   if (ERR(err = SSLHashSHA1.update(ctx->shaState, rec.contents)) != 0 ||
            ERR(err = SSLHashMD5.update(ctx->md5State, rec.contents)) != 0)
        {   ERR(SSLFatalSessionAlert(alert_close_notify, ctx));
            goto fail;
        }
        SSLLogHdskMsg((SSLHandshakeType)rec.contents.data[0], 1);
    }
    
    if (ERR(err = SSLWriteRecord(rec, ctx)) != 0)
        goto fail;
    
    err = SSLNoErr;
fail:
    SSLFreeBuffer(&rec.contents, &ctx->sysCtx);
    
    return err;
}

SSLErr
SSL3ReceiveSSL2ClientHello(SSLRecord rec, SSLContext *ctx)
{   SSLErr      err;
    
    if (ERR(err = SSLInitMessageHashes(ctx)) != 0)
        return err;
    
    if (ERR(err = SSLHashSHA1.update(ctx->shaState, rec.contents)) != 0 ||
        ERR(err = SSLHashMD5.update(ctx->md5State, rec.contents)) != 0)
    {   ERR(SSLFatalSessionAlert(alert_close_notify, ctx));
        return err;
    }
    
    if (ERR(err = SSLAdvanceHandshake(SSL_client_hello, ctx)) != 0)
        return err;
    
    return SSLNoErr;
}

/* log changes in handshake state */
#if		LOG_HDSK_STATE

#include <stdio.h>

char *hdskStateToStr(SSLHandshakeState state)
{
	static char badStr[100];
	
	switch(state) {
		case SSLUninitialized:
			return "SSLUninitialized";	
		case HandshakeServerUninit:
			return "HandshakeServerUninit";	
		case HandshakeClientUninit:
			return "HandshakeClientUninit";	
		case SSLGracefulClose:
			return "SSLGracefulClose";	
		case SSLErrorClose:
			return "SSLErrorClose";		
		case SSLNoNotifyClose:
			return "SSLNoNotifyClose";
		case HandshakeServerHello:
			return "HandshakeServerHello";	
		case HandshakeServerHelloUnknownVersion:
			return "HandshakeServerHelloUnknownVersion";	
		case HandshakeKeyExchange:
			return "HandshakeKeyExchange";	
		case HandshakeCertificate:
			return "HandshakeCertificate";	
		case HandshakeHelloDone:
			return "HandshakeHelloDone";	
		case HandshakeClientCertificate:
			return "HandshakeClientCertificate";	
		case HandshakeClientKeyExchange:
			return "HandshakeClientKeyExchange";	
		case HandshakeClientCertVerify:
			return "HandshakeClientCertVerify";	
		case HandshakeChangeCipherSpec:
			return "HandshakeChangeCipherSpec";	
		case HandshakeFinished:
			return "HandshakeFinished";	
		case HandshakeSSL2ClientMasterKey:
			return "HandshakeSSL2ClientMasterKey";
		case HandshakeSSL2ClientFinished:
			return "HandshakeSSL2ClientFinished";	
		case HandshakeSSL2ServerHello:
			return "HandshakeSSL2ServerHello";	
		case HandshakeSSL2ServerVerify:
			return "HandshakeSSL2ServerVerify";	
		case HandshakeSSL2ServerFinished:
			return "HandshakeSSL2ServerFinished";	
		case HandshakeServerReady:
			return "HandshakeServerReady";	
		case HandshakeClientReady:
			return "HandshakeClientReady";
		default:
			sprintf(badStr, "Unknown state (%d(d)", state);
			return badStr;
	}
}

void SSLChangeHdskState(SSLContext *ctx, SSLHandshakeState newState)
{
	printf("...hdskState = %s\n", hdskStateToStr(newState));
	ctx->state = newState;
}

#endif	/* LOG_HDSK_STATE */

/* log handshake messages */

#if		LOG_HDSK_MSG

#include <stdio.h>

static char *hdskMsgToStr(SSLHandshakeType msg)
{
	static char badStr[100];
	
	switch(msg) {
		case SSL_hello_request:
			return "SSL_hello_request";	
		case SSL_client_hello:
			return "SSL_client_hello";	
		case SSL_server_hello:
			return "SSL_server_hello";	
		case SSL_certificate:
			return "SSL_certificate";	
		case SSL_server_key_exchange:
			return "SSL_server_key_exchange";	
		case SSL_certificate_request:
			return "SSL_certificate_request";	
		case SSL_server_hello_done:
			return "SSL_server_hello_done";	
		case SSL_certificate_verify:
			return "SSL_certificate_verify";	
		case SSL_client_key_exchange:
			return "SSL_client_key_exchange";	
		case SSL_finished:
			return "SSL_finished";	
		case SSL_MAGIC_no_certificate_alert:
			return "SSL_MAGIC_no_certificate_alert";	
		default:
			sprintf(badStr, "Unknown state (%d(d)", msg);
			return badStr;
	}
}

void SSLLogHdskMsg(SSLHandshakeType msg, char sent)
{
	printf("---%s handshake msg %s\n", 
		hdskMsgToStr(msg), (sent ? "sent" : "recv"));
}

#endif	/* LOG_HDSK_MSG */
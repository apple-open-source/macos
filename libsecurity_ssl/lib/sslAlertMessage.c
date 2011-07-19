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


/*  *********************************************************************
    File: sslAlertMessage.c - SSL3 Alert protocol
    ****************************************************************** */

#include "ssl.h"
#include "sslAlertMessage.h"
#include "sslMemory.h"
#include "sslContext.h"
#include "sslSession.h"
#include "sslDebug.h"
#include "sslUtils.h"

#include <assert.h>

#ifdef	NDEBUG
#define SSLLogAlertMsg(msg,sent)
#else
static void SSLLogAlertMsg(AlertDescription msg, bool sent);
#endif

static OSStatus SSLEncodeAlert(
	SSLRecord *rec, 
	AlertLevel level, 
	AlertDescription desc, 
	SSLContext *ctx);

/*
 * If a peer sends us any kind of a bad cert alert, we may need to adjust
 * ctx->clientCertState accordingly.
 */
static void 
SSLDetectCertRejected(
	SSLContext 			*ctx,
	AlertDescription	desc)
{
	if(ctx->protocolSide == SSL_ServerSide) {
		return;
	}
	if(ctx->clientCertState != kSSLClientCertSent) {
		return;
	}
	switch(desc) {
		case SSL_AlertBadCert:
		case SSL_AlertUnsupportedCert:
		case SSL_AlertCertRevoked:
		case SSL_AlertCertExpired:
		case SSL_AlertCertUnknown:
		case SSL_AlertUnknownCA:
			ctx->clientCertState = kSSLClientCertRejected;
			break;
		default:
			break;
	}
}

OSStatus
SSLProcessAlert(SSLRecord rec, SSLContext *ctx)
{   OSStatus            err = noErr;
    AlertLevel          level;
    AlertDescription    desc;
    uint8               *charPtr;
    uint32              remaining;
    
    if (rec.contents.length % 2 != 0)
    {   
		err = SSLFatalSessionAlert(SSL_AlertIllegalParam, ctx);
        if (!err) {
            err = errSSLProtocol;
		}
        return err;
    }
    
    charPtr = rec.contents.data;
    remaining = rec.contents.length;
	bool fatal = false;
	
    while (remaining > 0)
    {   level = (AlertLevel)*charPtr++;
        desc = (AlertDescription)*charPtr++;
		sslHdskMsgDebug("alert msg received level %d   desc %d",
			(int)level, (int)desc);
        remaining -= 2;
        SSLLogAlertMsg(desc, false);
		
        if (level == SSL_AlertLevelFatal) {
			/* explicit fatal errror */
			fatal = true;
            sslHdskMsgDebug("***Fatal alert %d received\n", desc);
        }
        SSLDetectCertRejected(ctx, desc);
		
        switch (desc) {
			/* A number of these are fatal by implication */
            case SSL_AlertUnexpectedMsg:
				err = errSSLPeerUnexpectedMsg;
				fatal = true;
				break;
            case SSL_AlertBadRecordMac:
 				err = errSSLPeerBadRecordMac;
				fatal = true;
				break;
			case SSL_AlertDecryptionFail:
 				err = errSSLPeerDecryptionFail;
				fatal = true;
				break;
            case SSL_AlertRecordOverflow:
 				err = errSSLPeerRecordOverflow;
				fatal = true;
				break;
            case SSL_AlertDecompressFail:
 				err = errSSLPeerDecompressFail;
				fatal = true;
				break;
            case SSL_AlertHandshakeFail:
 				err = errSSLPeerHandshakeFail;
				fatal = true;
				break;
            case SSL_AlertIllegalParam:
 				err = errSSLIllegalParam;
				fatal = true;
				break;
            case SSL_AlertBadCert:
				err = errSSLPeerBadCert;
				break;
            case SSL_AlertUnsupportedCert:
				err = errSSLPeerUnsupportedCert;
				break;
            case SSL_AlertCertRevoked:
				err = errSSLPeerCertRevoked;
				break;
            case SSL_AlertCertExpired:
				err = errSSLPeerCertExpired;
				break;
            case SSL_AlertCertUnknown:
                err = errSSLPeerCertUnknown;
                break;
            case SSL_AlertUnknownCA:
                err = errSSLPeerUnknownCA;
                break;
            case SSL_AlertAccessDenied:
                err = errSSLPeerAccessDenied;
                break;
            case SSL_AlertDecodeError:
                err = errSSLPeerDecodeError;
                break;
            case SSL_AlertDecryptError:
                err = errSSLPeerDecryptError;
                break;
            case SSL_AlertExportRestriction:
                err = errSSLPeerExportRestriction;
                break;
            case SSL_AlertProtocolVersion:
                err = errSSLPeerProtocolVersion;
                break;
            case SSL_AlertInsufficientSecurity:
                err = errSSLPeerInsufficientSecurity;
                break;
            case SSL_AlertInternalError:
                err = errSSLPeerInternalError;
                break;
            case SSL_AlertUserCancelled:
                err = errSSLPeerUserCancelled;
                break;
            case SSL_AlertNoRenegotiation:
                err = errSSLPeerNoRenegotiation;
                break;
			/* unusual cases.... */
            case SSL_AlertCloseNotify:
				/* the clean "we're done" case */
                SSLClose(ctx);
                err = noErr;
                break;
            case SSL_AlertNoCert:
                if((ctx->state == SSL_HdskStateClientCert) &&
				   (ctx->protocolSide == SSL_ServerSide) &&
				   (ctx->clientAuth != kAlwaysAuthenticate)) {
					/*
					 * Tolerate this unless we're configured to  
					 * *require* a client cert. If a client cert is
					 * required, we'll catch the error at the next 
					 * handshake msg we receive - which will probably
					 * be a client key exchange msg, which is illegal
					 * when we're in state SSL_HdskStateClientCert.
					 * If the client cert is optional, advance to 
					 * state ClientKeyExchange by pretending we
					 * just got a client cert msg. 
					 */
                    if ((err = SSLAdvanceHandshake(SSL_HdskCert, 
							ctx)) != 0) {
                        return err;
					}
				}
                break;
            default:
                /* Unknown alert, ignore if not fatal */
				if(level == SSL_AlertLevelFatal) {
					err = errSSLFatalAlert;
				}
				else {
					err = noErr;
				}
                break;
        }
		if(fatal) {
			/* don't bother processing any more */
			break;
		}
    }
    if(fatal) {
       	SSLDeleteSessionData(ctx);
	}
    return err;
}

OSStatus
SSLSendAlert(AlertLevel level, AlertDescription desc, SSLContext *ctx)
{   SSLRecord       rec;
    OSStatus        err;
    
	switch(ctx->negProtocolVersion) {
		case SSL_Version_Undetermined:
			/* Too early in negotiation to send an alert */
			return noErr;
		case SSL_Version_2_0:
			/* shouldn't be here */
			assert(0);
			return errSSLInternal;
		default:
			break;
	}
	if(ctx->sentFatalAlert) {
		/* no more alerts allowed */
		return noErr;
	}
    if ((err = SSLEncodeAlert(&rec, level, desc, ctx)) != 0)
        return err;
	assert(ctx->sslTslCalls != NULL);
	SSLLogAlertMsg(desc, true);
    if ((err = ctx->sslTslCalls->writeRecord(rec, ctx)) != 0)
        return err;
    if ((err = SSLFreeBuffer(&rec.contents, ctx)) != 0)
        return err;
    if(desc == SSL_AlertCloseNotify) {
		/* no more alerts allowed */
		ctx->sentFatalAlert = true;
	}
    return noErr;
}

static OSStatus
SSLEncodeAlert(SSLRecord *rec, AlertLevel level, AlertDescription desc, SSLContext *ctx)
{   OSStatus          err;
    
	rec->protocolVersion = ctx->negProtocolVersion;
	rec->contentType = SSL_RecordTypeAlert;
    rec->contents.length = 2;
    if ((err = SSLAllocBuffer(&rec->contents, 2, ctx)) != 0)
        return err;
    rec->contents.data[0] = level;
    rec->contents.data[1] = desc;
    
    return noErr;
}

OSStatus
SSLFatalSessionAlert(AlertDescription desc, SSLContext *ctx)
{   OSStatus          err1, err2;
    
    if(desc != SSL_AlertCloseNotify) {
    	sslHdskMsgDebug("SSLFatalSessionAlert: desc %d\n", desc);
    }
    SSLChangeHdskState(ctx, SSL_HdskStateErrorClose);
    
	if(ctx->negProtocolVersion < TLS_Version_1_0) {
		/* translate to SSL3 if necessary */
		switch(desc) {
			case SSL_AlertDecryptionFail:
			case SSL_AlertRecordOverflow:
			case SSL_AlertAccessDenied:
			case SSL_AlertDecodeError:
			case SSL_AlertDecryptError:
			case SSL_AlertExportRestriction:
			case SSL_AlertProtocolVersion:
			case SSL_AlertInsufficientSecurity:
			case SSL_AlertUserCancelled:
			case SSL_AlertNoRenegotiation:
				desc = SSL_AlertHandshakeFail;
				break;
			case SSL_AlertUnknownCA:
				desc = SSL_AlertUnsupportedCert;
				break;
			case SSL_AlertInternalError:
				desc = SSL_AlertCloseNotify;
				break;
			default:
				/* send as is */
				break;
		}
	}
    /* Make session unresumable; I'm not stopping if I get an error,
        because I'd like to attempt to send the alert anyway */
    err1 = SSLDeleteSessionData(ctx);
    
    /* Second, send the alert */
    err2 = SSLSendAlert(SSL_AlertLevelFatal, desc, ctx);
    
	ctx->sentFatalAlert = true;
	
    /* If they both returned errors, arbitrarily return the first */
    return err1 != 0 ? err1 : err2;
}

#ifndef	NDEBUG

/* log alert messages */

static char *alertMsgToStr(AlertDescription msg)
{
	static char badStr[100];
	
	switch(msg) {
		case SSL_AlertCloseNotify:
			return "SSL_AlertCloseNotify";	
		case SSL_AlertUnexpectedMsg:
			return "SSL_AlertUnexpectedMsg";	
		case SSL_AlertBadRecordMac:
			return "SSL_AlertBadRecordMac";	
		case SSL_AlertDecryptionFail:
			return "SSL_AlertDecryptionFail";	
		case SSL_AlertRecordOverflow:
			return "SSL_AlertRecordOverflow";	
		case SSL_AlertDecompressFail:
			return "SSL_AlertDecompressFail";	
		case SSL_AlertHandshakeFail:
			return "SSL_AlertHandshakeFail";	
		case SSL_AlertNoCert:
			return "SSL_AlertNoCert";	
		case SSL_AlertBadCert:
			return "SSL_AlertBadCert";	
		case SSL_AlertUnsupportedCert:
			return "SSL_AlertUnsupportedCert";	
		case SSL_AlertCertRevoked:
			return "SSL_AlertCertRevoked";	
			
		case SSL_AlertCertExpired:
			return "SSL_AlertCertExpired";	
		case SSL_AlertCertUnknown:
			return "SSL_AlertCertUnknown";	
		case SSL_AlertIllegalParam:
			return "SSL_AlertIllegalParam";	
		case SSL_AlertUnknownCA:
			return "SSL_AlertUnknownCA";	
		case SSL_AlertAccessDenied:
			return "SSL_AlertAccessDenied";	
		case SSL_AlertDecodeError:
			return "SSL_AlertDecodeError";	
		case SSL_AlertDecryptError:
			return "SSL_AlertDecryptError";	

		case SSL_AlertExportRestriction:
			return "SSL_AlertExportRestriction";	
		case SSL_AlertProtocolVersion:
			return "SSL_AlertProtocolVersion";	
		case SSL_AlertInsufficientSecurity:
			return "SSL_AlertInsufficientSecurity";	
		case SSL_AlertInternalError:
			return "SSL_AlertInternalError";	
		case SSL_AlertUserCancelled:
			return "SSL_AlertUserCancelled";	
		case SSL_AlertNoRenegotiation:
			return "SSL_AlertNoRenegotiation";	

		default:
			sprintf(badStr, "Unknown state (%d(d)", msg);
			return badStr;
	}
}

static void SSLLogAlertMsg(AlertDescription msg, bool sent)
{
	sslHdskMsgDebug("---%s alert msg %s", 
		alertMsgToStr(msg), (sent ? "sent" : "recv"));
}

#endif	/* NDEBUG */

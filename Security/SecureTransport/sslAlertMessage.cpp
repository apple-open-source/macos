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
    File: sslAlertMessage.cpp - SSL3 Alert protocol
    ****************************************************************** */

#include "ssl.h"
#include "sslAlertMessage.h"
#include "sslMemory.h"
#include "sslContext.h"
#include "sslSession.h"
#include "sslDebug.h"

#include <assert.h>

#ifdef	NDEBUG
#define SSLLogAlertMsg(msg,sent)
#else
static void SSLLogAlertMsg(AlertDescription msg, bool sent);
#endif

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
    while (remaining > 0)
    {   level = (AlertLevel)*charPtr++;
        desc = (AlertDescription)*charPtr++;
		sslHdskMsgDebug("alert msg recieved level %d   desc %d\n",
			(int)level, (int)desc);
        remaining -= 2;
        SSLLogAlertMsg(desc, false);
		
        /* 
         * Ignore sessionID-related failures here;
         * the important thing is the alert. 
         */
        if (level == SSL_AlertLevelFatal)
        {   
        	SSLDeleteSessionData(ctx);
            sslErrorLog("***Fatal alert %d received\n", desc);
            return errSSLFatalAlert;
        }
        
        switch (desc)
        {   case SSL_AlertUnexpectedMsg:
            case SSL_AlertBadRecordMac:
            case SSL_AlertDecompressFail:
            case SSL_AlertHandshakeFail:
            case SSL_AlertIllegalParam:
                /* These must always be fatal; if we got here, the level is warning;
                 *  die anyway
                 */
                SSLDeleteSessionData(ctx);
                err = errSSLFatalAlert;
                break;
            case SSL_AlertCloseNotify:
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
            case SSL_AlertBadCert:
            case SSL_AlertUnsupportedCert:
            case SSL_AlertCertRevoked:
            case SSL_AlertCertExpired:
            case SSL_AlertCertUnknown:
                err = noErr;
                break;
            default:
                /* Unknown alert, but not fatal; ignore it */
                break;
        }
    }
    
    return err;
}

OSStatus
SSLSendAlert(AlertLevel level, AlertDescription desc, SSLContext *ctx)
{   SSLRecord       rec;
    OSStatus        err;
    
    assert((ctx->negProtocolVersion != SSL_Version_2_0));
    
    if ((err = SSLEncodeAlert(rec, level, desc, ctx)) != 0)
        return err;
	assert(ctx->sslTslCalls != NULL);
	SSLLogAlertMsg(desc, true);
    if ((err = ctx->sslTslCalls->writeRecord(rec, ctx)) != 0)
        return err;
    if ((err = SSLFreeBuffer(rec.contents, ctx)) != 0)
        return err;
    
    return noErr;
}

OSStatus
SSLEncodeAlert(SSLRecord &rec, AlertLevel level, AlertDescription desc, SSLContext *ctx)
{   OSStatus          err;
    
    rec.contentType = SSL_RecordTypeAlert;
    assert((ctx->negProtocolVersion != SSL_Version_2_0));
	if(ctx->negProtocolVersion == SSL_Version_Undetermined) {
		/* error while negotiating */
		rec.protocolVersion = ctx->maxProtocolVersion;
	}
	else {
		rec.protocolVersion = ctx->negProtocolVersion;
	}
    rec.contents.length = 2;
    if ((err = SSLAllocBuffer(rec.contents, 2, ctx)) != 0)
        return err;
    rec.contents.data[0] = level;
    rec.contents.data[1] = desc;
    
    return noErr;
}

OSStatus
SSLFatalSessionAlert(AlertDescription desc, SSLContext *ctx)
{   OSStatus          err1, err2;
    
    if(desc != SSL_AlertCloseNotify) {
    	sslErrorLog("SSLFatalSessionAlert: desc %d\n", desc);
    }
    SSLChangeHdskState(ctx, SSL_HdskStateErrorClose);
    
    /* Make session unresumable; I'm not stopping if I get an error,
        because I'd like to attempt to send the alert anyway */
    err1 = SSLDeleteSessionData(ctx);
    
    /* Second, send the alert */
    err2 = SSLSendAlert(SSL_AlertLevelFatal, desc, ctx);
    
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

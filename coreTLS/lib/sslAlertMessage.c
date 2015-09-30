/*
 * Copyright (c) 2000-2001,2005-2007,2010-2012 Apple Inc. All Rights Reserved.
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
 * sslAlertMessage.c - SSL3 Alert protocol
 */

#include "tls_handshake_priv.h"
#include "sslAlertMessage.h"
#include "sslMemory.h"
#include "sslSession.h"
#include "sslDebug.h"
#include "sslUtils.h"

#include <assert.h>

#if !SSL_DEBUG
#define SSLLogAlertMsg(msg,sent)
#else
static void SSLLogAlertMsg(AlertDescription msg, bool sent);
#endif

static int SSLEncodeAlert(
	tls_buffer *rec,
	AlertLevel level,
	AlertDescription desc,
	tls_handshake_t ctx);

/*
 * If a peer sends us any kind of a bad cert alert, we may need to adjust
 * ctx->clientCertState accordingly.
 */
static void
SSLDetectCertRejected(
	tls_handshake_t ctx,
	AlertDescription	desc)
{
	if(ctx->isServer) {
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

int
SSLProcessAlert(tls_buffer rec, tls_handshake_t ctx)
{   int            err = errSSLSuccess;
    AlertLevel          level;
    AlertDescription    desc;
    uint8_t             *charPtr;
    size_t              remaining;

    if (rec.length % 2 != 0)
    {
		err = SSLFatalSessionAlert(SSL_AlertIllegalParam, ctx);
        if (!err) {
            err = errSSLProtocol;
		}
        return err;
    }

    charPtr = rec.data;
    remaining = rec.length;
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
            sslErrorLog("***Fatal alert %d received\n", desc);
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
			case SSL_AlertDecryptionFail_RESERVED:
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
            case SSL_AlertExportRestriction_RESERVED:
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
                tls_handshake_close(ctx);
                err = errSSLClosedGraceful;
                break;
            case SSL_AlertNoCert_RESERVED:
                if((ctx->state == SSL_HdskStateClientCert) &&
				   (ctx->isServer)) {
					/*
					 * Advance to
					 * state ClientKeyExchange by pretending we
					 * just got an empty client cert msg.
					 */
                    if ((err = SSLAdvanceHandshake(SSL_HdskCert,
							ctx)) != 0) {
                        return err;
					}
				}
                break;
            case SSL_AlertUnsupportedExtension:
                err = errSSLFatalAlert;
                fatal = true;
                break;

            default:
                /* Unknown alert, ignore if not fatal */
				if(level == SSL_AlertLevelFatal) {
					err = errSSLFatalAlert;
				}
				else {
					err = errSSLSuccess;
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

int
SSLSendAlert(AlertLevel level, AlertDescription desc, tls_handshake_t ctx)
{
    tls_buffer  rec;
    int        err;

	switch(ctx->negProtocolVersion) {
		case tls_protocol_version_Undertermined:
			/* Too early in negotiation to send an alert */
			return errSSLSuccess;
		default:
			break;
	}
	if(ctx->sentFatalAlert) {
		/* no more alerts allowed */
		return errSSLSuccess;
	}
    if ((err = SSLEncodeAlert(&rec, level, desc, ctx)) != 0)
        return err;
	SSLLogAlertMsg(desc, true);
    err = ctx->callbacks->write(ctx->callback_ctx, rec, tls_record_type_Alert);
    SSLFreeBuffer(&rec);
    if(desc == SSL_AlertCloseNotify) {
		/* no more alerts allowed */
		ctx->sentFatalAlert = true;
	}
    return err;
}

static int
SSLEncodeAlert(tls_buffer *rec, AlertLevel level, AlertDescription desc, tls_handshake_t ctx)
{   int          err;

    rec->length = 2;
    if ((err = SSLAllocBuffer(rec, 2)))
        return err;
    rec->data[0] = level;
    rec->data[1] = desc;

    return errSSLSuccess;
}

int
SSLFatalSessionAlert(AlertDescription desc, tls_handshake_t ctx)
{   int          err1, err2;

    if(desc != SSL_AlertCloseNotify) {
    	sslHdskMsgDebug("SSLFatalSessionAlert: desc %d\n", desc);
    }
    SSLChangeHdskState(ctx, SSL_HdskStateErrorClose);

	if(ctx->negProtocolVersion < tls_protocol_version_TLS_1_0) {
		/* translate to SSL3 if necessary */
		switch(desc) {
			//case SSL_AlertDecryptionFail_RESERVED:
			case SSL_AlertRecordOverflow:
			case SSL_AlertAccessDenied:
			case SSL_AlertDecodeError:
			case SSL_AlertDecryptError:
			//case SSL_AlertExportRestriction_RESERVED:
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

#if SSL_DEBUG

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
		case SSL_AlertDecryptionFail_RESERVED:
			return "SSL_AlertDecryptionFail";
		case SSL_AlertRecordOverflow:
			return "SSL_AlertRecordOverflow";
		case SSL_AlertDecompressFail:
			return "SSL_AlertDecompressFail";
		case SSL_AlertHandshakeFail:
			return "SSL_AlertHandshakeFail";
		case SSL_AlertNoCert_RESERVED:
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

		case SSL_AlertExportRestriction_RESERVED:
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
			sprintf(badStr, "Unknown state (%d)", msg);
			return badStr;
	}
}

static void SSLLogAlertMsg(AlertDescription msg, bool sent)
{
	sslHdskMsgDebug("---%s alert msg %s",
		alertMsgToStr(msg), (sent ? "sent" : "recv"));
}

#endif	/* SSL_DEBUG */

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
    File: sslalert.c

    SSLRef 3.0 Final -- 11/19/96

    Copyright (c)1996 by Netscape Communications Corp.

    By retrieving this software you are bound by the licensing terms
    disclosed in the file "LICENSE.txt". Please read it, and if you don't
    accept the terms, delete this software.

    SSLRef 3.0 was developed by Netscape Communications Corp. of Mountain
    View, California <http://home.netscape.com/> and Consensus Development
    Corporation of Berkeley, California <http://www.consensus.com/>.

    *********************************************************************

    File: sslalert.c   Support for alert protocol in SSL 3

    Encoding, decoding and processing for the SSL alert protocol. Also,
    support for sending fatal alerts, which also closes down our
    connection, including invalidating our cached session.

    ****************************************************************** */

#include "ssl.h"

#ifndef _SSLALERT_H_
#include "sslalert.h"
#endif

#ifndef _SSLALLOC_H_
#include "sslalloc.h"
#endif

#ifndef _SSLCTX_H_
#include "sslctx.h"
#endif

#ifndef _SSLSESS_H_
#include "sslsess.h"
#endif

#ifndef	_SSL_DEBUG_H_
#include "sslDebug.h"
#endif

#include <assert.h>

SSLErr
SSLProcessAlert(SSLRecord rec, SSLContext *ctx)
{   SSLErr              err = SSLNoErr;
    AlertLevel          level;
    AlertDescription    desc;
    uint8               *progress;
    uint32              remaining;
    
    if (rec.contents.length % 2 != 0)
    {   ERR(err = SSLFatalSessionAlert(alert_illegal_parameter, ctx));
        if (!err)
            ERR(err = SSLProtocolErr);
        return err;
    }
    
    progress = rec.contents.data;
    remaining = rec.contents.length;
    while (remaining > 0)
    {   level = (AlertLevel)*progress++;
        desc = (AlertDescription)*progress++;
        remaining -= 2;
        
        /* 
         * APPLE_CDSA changes: ignore sessionID-related failures here;
         * the important thing is the alert. 
         */
        if (level == alert_fatal)
        {   
        	SSLDeleteSessionData(ctx);
            dprintf1("***Fatal alert %d received", desc);
            return SSLFatalAlert;
        }
        
        switch (desc)
        {   case alert_unexpected_message:
            case alert_bad_record_mac:
            case alert_decompression_failure:
            case alert_handshake_failure:
            case alert_illegal_parameter:
                /* These must always be fatal; if we got here, the level is warning;
                 *  die anyway
                 */
                SSLDeleteSessionData(ctx);
                err = SSLFatalAlert;
                break;
            case alert_close_notify:
                ERR(SSLClose(ctx));
                err = SSLNoErr;
                break;
            case alert_no_certificate:
                if (ctx->state == HandshakeClientCertificate)
                    if (ERR(err = SSLAdvanceHandshake(SSL_certificate, ctx)) != 0)
                        return err;
                break;
            case alert_bad_certificate:
            case alert_unsupported_certificate:
            case alert_certificate_revoked:
            case alert_certificate_expired:
            case alert_certificate_unknown:
                err = SSLNoErr;
                break;
            default:
                /* Unknown alert, but not fatal; ignore it */
                break;
        }
    }
    
    return err;
}

SSLErr
SSLSendAlert(AlertLevel level, AlertDescription desc, SSLContext *ctx)
{   SSLRecord       rec;
    SSLErr          err;
    
    CASSERT((ctx->negProtocolVersion != SSL_Version_2_0));
    
    if ((err = SSLEncodeAlert(&rec, level, desc, ctx)) != 0)
        return err;
	assert(ctx->sslTslCalls != NULL);
    if ((err = ctx->sslTslCalls->writeRecord(rec, ctx)) != 0)
        return err;
    if ((err = SSLFreeBuffer(&rec.contents, &ctx->sysCtx)) != 0)
        return err;
    
    return SSLNoErr;
}

SSLErr
SSLEncodeAlert(SSLRecord *rec, AlertLevel level, AlertDescription desc, SSLContext *ctx)
{   SSLErr          err;
    
    rec->contentType = SSL_alert;
    CASSERT((ctx->negProtocolVersion != SSL_Version_2_0));
	if(ctx->negProtocolVersion == SSL_Version_Undetermined) {
		/* error while negotiating */
		rec->protocolVersion = ctx->maxProtocolVersion;
	}
	else {
		rec->protocolVersion = ctx->negProtocolVersion;
	}
    rec->contents.length = 2;
    if ((err = SSLAllocBuffer(&rec->contents, 2, &ctx->sysCtx)) != 0)
        return err;
    rec->contents.data[0] = level;
    rec->contents.data[1] = desc;
    
    return SSLNoErr;
}

SSLErr
SSLFatalSessionAlert(AlertDescription desc, SSLContext *ctx)
{   SSLErr          err1, err2;
    
    if(desc != alert_close_notify) {
    	errorLog1("SSLFatalSessionAlert: desc %d\n", desc);
    }
	//dprintf0("SSLFatalSessionAlert: going to state ErrorClose\n");
    SSLChangeHdskState(ctx, SSLErrorClose);
    
    /* Make session unresumable; I'm not stopping if I get an error,
        because I'd like to attempt to send the alert anyway */
    err1 = SSLDeleteSessionData(ctx);
    
    /* Second, send the alert */
    err2 = SSLSendAlert(alert_fatal, desc, ctx);
    
    /* If they both returned errors, arbitrarily return the first */
    return err1 != 0 ? err1 : err2;
}

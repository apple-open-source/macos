/*
 * Copyright (c) 1999-2001,2005-2007,2010-2012 Apple Inc. All Rights Reserved.
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
 * sslChangeCipher.c - support for change cipher spec messages
 */

#include "tls_handshake_priv.h"
#include "sslHandshake.h"
#include "sslHandshake_priv.h"
#include "sslMemory.h"
#include "sslAlertMessage.h"
#include "sslDebug.h"
#include "sslCipherSpecs.h"

#include <assert.h>
#include <string.h>

int
SSLEncodeChangeCipherSpec(tls_buffer *rec, tls_handshake_t ctx)
{   int          err;
    
    assert(ctx->writePending_ready);
    
    sslLogNegotiateDebug("===Sending changeCipherSpec msg");
	assert(ctx->negProtocolVersion >= tls_protocol_version_SSL_3);
    rec->length = 1;
    if ((err = SSLAllocBuffer(rec, 1)))
        return err;
    rec->data[0] = 1;

    ctx->messageQueueContainsChangeCipherSpec=true;

    return errSSLSuccess;
}

int
SSLProcessChangeCipherSpec(tls_buffer rec, tls_handshake_t ctx)
{   int          err;

    if (rec.length != 1 || rec.data[0] != 1)
    {
#warning This is fishy:
        if(ctx->isDTLS)
            return errSSLUnexpectedRecord;

        SSLFatalSessionAlert(SSL_AlertUnexpectedMsg, ctx);
    	sslErrorLog("***bad changeCipherSpec msg: length %d data 0x%x\n",
    		(unsigned)rec.length, (unsigned)rec.data[0]);
        return errSSLProtocol;
    }

	/*
	 * Handle PAC-style session resumption, client side only.
	 * In that case, the handshake state was left in either KeyExchange or
	 * Cert.
     * Other client side resumption cases are handled in SSLAdvanceHandshake
     * (case SSL_HdskServerHello:).
	 */
	if((!ctx->isServer) &&
	   (ctx->externalSessionTicket.length != 0) &&
	   ((ctx->state == SSL_HdskStateKeyExchange) || (ctx->state == SSL_HdskStateCert)) &&
	   (ctx->masterSecretCallback != NULL)) {
		size_t secretLen = SSL_MASTER_SECRET_SIZE;
		sslEapDebug("Client side resuming based on masterSecretCallback");
		ctx->masterSecretCallback(ctx->masterSecretArg,
			ctx->masterSecret, &secretLen);
		ctx->sessionMatch = 1;

		/* set up selectedCipherSpec */
		if ((err = ValidateSelectedCiphersuite(ctx)) != 0) {
			return err;
		}
		if((err = SSLInitPendingCiphers(ctx)) != 0) {
			SSLFatalSessionAlert(SSL_AlertInternalError, ctx);
			return err;
		}
		SSLChangeHdskState(ctx, SSL_HdskStateChangeCipherSpec);
	}

    if (!ctx->readPending_ready || ctx->state != SSL_HdskStateChangeCipherSpec)
    {
        // This mean we received a ChangeCipherSpec message when we didnt expect it.
        // Just drop silently.
        if(ctx->isDTLS)
            return errSSLUnexpectedRecord;

        SSLFatalSessionAlert(SSL_AlertUnexpectedMsg, ctx);
    	sslErrorLog("***bad changeCipherSpec msg: readPending.ready %d state %d\n",
		(unsigned)ctx->readPending_ready, (unsigned)ctx->state);
        return errSSLProtocol;
    }

    sslLogNegotiateDebug("===Processing changeCipherSpec msg");

    /* Install new cipher spec on read side */
    /* TODO: if we want to enable app data during most of handshake, 
       we should disable read channel here:
        sslReadReady(ctx, false);
    */
    if ((err = ctx->callbacks->advance_read_cipher(ctx->callback_ctx)) != 0)
    {
        SSLFatalSessionAlert(SSL_AlertInternalError, ctx);
        return err;
    }
    /* make the pending read cipher invalid */
    ctx->readPending_ready = 0;
    SSLChangeHdskState(ctx, SSL_HdskStateFinished);
    return errSSLSuccess;
}


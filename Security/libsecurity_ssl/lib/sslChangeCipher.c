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

#include "sslContext.h"
#include "sslHandshake.h"
#include "sslMemory.h"
#include "sslAlertMessage.h"
#include "sslDebug.h"
#include "cipherSpecs.h"

#include <assert.h>
#include <string.h>

OSStatus
SSLEncodeChangeCipherSpec(SSLRecord *rec, SSLContext *ctx)
{   OSStatus          err;

    assert(ctx->writePending.ready);

    sslLogNegotiateDebug("===Sending changeCipherSpec msg");
    rec->contentType = SSL_RecordTypeChangeCipher;
	assert(ctx->negProtocolVersion >= SSL_Version_3_0);
    rec->protocolVersion = ctx->negProtocolVersion;
    rec->contents.length = 1;
    if ((err = SSLAllocBuffer(&rec->contents, 1, ctx)) != 0)
        return err;
    rec->contents.data[0] = 1;

    ctx->messageQueueContainsChangeCipherSpec=true;

    return noErr;
}

OSStatus
SSLProcessChangeCipherSpec(SSLRecord rec, SSLContext *ctx)
{   OSStatus          err;

    if (rec.contents.length != 1 || rec.contents.data[0] != 1)
    {
        if(ctx->isDTLS)
            return errSSLWouldBlock;

        SSLFatalSessionAlert(SSL_AlertUnexpectedMsg, ctx);
    	sslErrorLog("***bad changeCipherSpec msg: length %d data 0x%x\n",
    		(unsigned)rec.contents.length, (unsigned)rec.contents.data[0]);
        return errSSLProtocol;
    }

	/*
	 * Handle PAC-style session resumption, client side only.
	 * In that case, the handshake state was left in either KeyExchange or
	 * Cert.
	 */
	if((ctx->protocolSide == kSSLClientSide) &&
	   (ctx->sessionTicket.length != 0) &&
	   ((ctx->state == SSL_HdskStateKeyExchange) || (ctx->state == SSL_HdskStateCert)) &&
	   (ctx->masterSecretCallback != NULL)) {
		size_t secretLen = SSL_MASTER_SECRET_SIZE;
		sslEapDebug("Client side resuming based on masterSecretCallback");
		ctx->masterSecretCallback(ctx, ctx->masterSecretArg,
			ctx->masterSecret, &secretLen);
		ctx->sessionMatch = 1;

		/* set up selectedCipherSpec */
		if ((err = FindCipherSpec(ctx)) != 0) {
			return err;
		}
		if((err = SSLInitPendingCiphers(ctx)) != 0) {
			SSLFatalSessionAlert(SSL_AlertInternalError, ctx);
			return err;
		}
		SSLChangeHdskState(ctx, SSL_HdskStateChangeCipherSpec);
	}

    if (!ctx->readPending.ready || ctx->state != SSL_HdskStateChangeCipherSpec)
    {
        if(ctx->isDTLS)
            return errSSLWouldBlock;

        SSLFatalSessionAlert(SSL_AlertUnexpectedMsg, ctx);
    	sslErrorLog("***bad changeCipherSpec msg: readPending.ready %d state %d\n",
    		(unsigned)ctx->readPending.ready, (unsigned)ctx->state);
        return errSSLProtocol;
    }

    sslLogNegotiateDebug("===Processing changeCipherSpec msg");

    /* Install new cipher spec on read side */
    if ((err = SSLDisposeCipherSuite(&ctx->readCipher, ctx)) != 0)
    {   SSLFatalSessionAlert(SSL_AlertInternalError, ctx);
        return err;
    }
    ctx->readCipher = ctx->readPending;
    ctx->readCipher.ready = 0;      /* Can't send data until Finished is sent */
    SSLChangeHdskState(ctx, SSL_HdskStateFinished);
    memset(&ctx->readPending, 0, sizeof(CipherContext)); 	/* Zero out old data */
    return noErr;
}

OSStatus
SSLDisposeCipherSuite(CipherContext *cipher, SSLContext *ctx)
{   OSStatus      err;

	/* symmetric encryption context */
	if(cipher->symCipher) {
		if ((err = cipher->symCipher->finish(cipher, ctx)) != 0) {
			return err;
		}
	}

	/* per-record hash/hmac context */
	ctx->sslTslCalls->freeMac(cipher);

    return noErr;
}

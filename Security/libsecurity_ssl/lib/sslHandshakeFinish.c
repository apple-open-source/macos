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
 * sslHandshakeFinish.c - Finished and server hello done messages.
 */

#include "sslContext.h"
#include "sslHandshake.h"
#include "sslMemory.h"
#include "sslDebug.h"
#include "sslUtils.h"
#include "sslDigests.h"

#include <string.h>
#include <assert.h>

OSStatus
SSLEncodeFinishedMessage(SSLRecord *finished, SSLContext *ctx)
{   OSStatus        err;
    SSLBuffer       finishedMsg;
    Boolean         isServerMsg;
    unsigned		finishedSize;
	UInt8           *p;
    int             head;

	/* size and version depend on negotiatedProtocol */
	switch(ctx->negProtocolVersion) {
		case SSL_Version_3_0:
			finishedSize = 36;
			break;
        case DTLS_Version_1_0:
		case TLS_Version_1_0:
        case TLS_Version_1_1:
        case TLS_Version_1_2: /* TODO: Support variable finishedSize. */
			finishedSize = 12;
			break;
		default:
			assert(0);
			return errSSLInternal;
	}
    finished->protocolVersion = ctx->negProtocolVersion;

    finished->contentType = SSL_RecordTypeHandshake;
	/* msg = type + 3 bytes len + finishedSize */
    head = SSLHandshakeHeaderSize(finished);
    if ((err = SSLAllocBuffer(&finished->contents, finishedSize + head,
			ctx)) != 0)
        return err;

    p = SSLEncodeHandshakeHeader(ctx, finished, SSL_HdskFinished, finishedSize);

    finishedMsg.data = p;
    finishedMsg.length = finishedSize;

    isServerMsg = (ctx->protocolSide == kSSLServerSide) ? true : false;
    err = ctx->sslTslCalls->computeFinishedMac(ctx, finishedMsg, isServerMsg);

    if(err)
        return err;

    /* Keep this around for secure renegotiation */
    SSLFreeBuffer(&ctx->ownVerifyData, ctx);
    return SSLCopyBuffer(&finishedMsg, &ctx->ownVerifyData);
}

OSStatus
SSLProcessFinished(SSLBuffer message, SSLContext *ctx)
{   OSStatus        err;
    SSLBuffer       expectedFinished;
    Boolean         isServerMsg;
    unsigned		finishedSize;

	switch(ctx->negProtocolVersion) {
		case SSL_Version_3_0:
			finishedSize = 36;
			break;
		case DTLS_Version_1_0:
		case TLS_Version_1_0:
        case TLS_Version_1_1:
        case TLS_Version_1_2: /* TODO: Support variable finishedSize. */
			finishedSize = 12;
			break;
		default:
			assert(0);
			return errSSLInternal;
	}
    if (message.length != finishedSize) {
		sslErrorLog("SSLProcessFinished: msg len error 1\n");
        return errSSLProtocol;
    }
    expectedFinished.data = 0;
    if ((err = SSLAllocBuffer(&expectedFinished, finishedSize, ctx)) != 0)
        return err;
    isServerMsg = (ctx->protocolSide == kSSLServerSide) ? false : true;
    if ((err = ctx->sslTslCalls->computeFinishedMac(ctx, expectedFinished, isServerMsg)) != 0)
        goto fail;

    if (memcmp(expectedFinished.data, message.data, finishedSize) != 0)
    {
   		sslErrorLog("SSLProcessFinished: memcmp failure\n");
   	 	err = errSSLProtocol;
        goto fail;
    }

    /* Keep this around for secure renegotiation */
    SSLFreeBuffer(&ctx->peerVerifyData, ctx);
    err = SSLCopyBuffer(&expectedFinished, &ctx->peerVerifyData);

fail:
    SSLFreeBuffer(&expectedFinished, ctx);
    return err;
}

OSStatus
SSLEncodeServerHelloDone(SSLRecord *helloDone, SSLContext *ctx)
{   OSStatus          err;
    int               head;

    helloDone->contentType = SSL_RecordTypeHandshake;
	assert(ctx->negProtocolVersion >= SSL_Version_3_0);
    helloDone->protocolVersion = ctx->negProtocolVersion;
    head = SSLHandshakeHeaderSize(helloDone);
    if ((err = SSLAllocBuffer(&helloDone->contents, head, ctx)) != 0)
        return err;

    SSLEncodeHandshakeHeader(ctx, helloDone, SSL_HdskServerHelloDone, 0); /* Message has 0 length */

    return noErr;
}

OSStatus
SSLProcessServerHelloDone(SSLBuffer message, SSLContext *ctx)
{   assert(ctx->protocolSide == kSSLClientSide);
    if (message.length != 0) {
    	sslErrorLog("SSLProcessServerHelloDone: nonzero msg len\n");
        return errSSLProtocol;
    }
    return noErr;
}

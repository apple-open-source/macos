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

// #include "sslContext.h"
#include "tls_handshake_priv.h"

#include "sslHandshake.h"
#include "sslMemory.h"
#include "sslDebug.h"
#include "sslUtils.h"
#include "sslDigests.h"

#include <string.h>
#include <assert.h>

/* Define the following to 1 if you want http://tools.ietf.org/html/draft-agl-tls-nextprotoneg-04,
 otherwise you get http://tools.ietf.org/html/draft-agl-tls-nextprotoneg-03 */
#define NPN_MSG_HAS_EXT_TYPE 0

int
SSLEncodeNPNEncryptedExtensionMessage(tls_buffer *npn, tls_handshake_t ctx)
{
    OSStatus        err;
    unsigned        msgSize;
    unsigned        extnSize;
    unsigned        paddingSize;
    UInt8           *p;
    int             head;

    if(ctx->npnOwnData.data == NULL)
        return errSSLBadConfiguration;
    assert(ctx->npnOwnData.length < 256);

    /* Total Size = Header (handshake message type) + Extension Data (Selected Protocol + Padding) */
    paddingSize = 32 - ((ctx->npnOwnData.length + 2) % 32);
    /* The selected protocol and padding opaque byte strings each start with a 1-byte length */
    extnSize = 1 + (unsigned)ctx->npnOwnData.length + 1 + paddingSize;

    msgSize = extnSize;
#if NPN_MSG_HAS_EXT_TYPE
    msgSize += 4;
#endif

    head = SSLHandshakeHeaderSize(ctx);
    if ((err = SSLAllocBuffer(npn, msgSize + head)) != 0) {
        return err;
    }

    /* Populate the record with the header */
    p = SSLEncodeHandshakeHeader(ctx, npn, SSL_HdskNPNEncryptedExtension, msgSize);

#if NPN_MSG_HAS_EXT_TYPE
    /* Latest draft require this - google does not actually accept this */
    /* Extension type */
    p = SSLEncodeInt(p, SSL_HE_NPN, 2);

    /* Extension data len */
    p = SSLEncodeInt(p, extnSize, 2);
#endif

    /* Now include the length of the selected protocol */
    p = SSLEncodeInt(p, ctx->npnOwnData.length, 1);

    /* Fill the record with the selected protocol */
    memcpy(p, ctx->npnOwnData.data, ctx->npnOwnData.length);
    p += ctx->npnOwnData.length;

    /* Now include the length of the padding */
    p = SSLEncodeInt(p, paddingSize, 1);

    /* Finish the record with the padding */
	memset(p, 0, paddingSize);

    ctx->npn_confirmed = true;

    return errSSLSuccess;
}

static int
SSLProcessEncryptedNPNExtension(const uint8_t *p, size_t extLen, tls_handshake_t ctx)
{
    int err;
    if (!ctx->npn_announced)
        return errSSLProtocol;

    ctx->npn_received = true;
    if ((err = SSLAllocBuffer(&ctx->npnPeerData, extLen)) != 0)
        return err;
    memcpy(ctx->npnPeerData.data, p, ctx->npnPeerData.length);
    return 0;
}

int
SSLProcessEncryptedExtension(tls_buffer message, tls_handshake_t ctx)
{
    const uint8_t *p = message.data;

#if NPN_MSG_HAS_EXT_TYPE
    int err;
    while (message.length) {
        unsigned int extName, extLen;

        if (message.length < 4)
            return errSSLProtocol;

        extName = SSLDecodeInt(p, 2);
        p += 2;
        extLen = SSLDecodeInt(p, 2);
        p += 2;

        message.length -= 4;

        if (message.length < extLen)
            return errSSLProtocol;

        switch (extName) {
            case SSL_HE_NPN:
                if((err = SSLProcessEncryptedNPNExtension(p, extLen, ctx)))
                    return err;
                p += extLen;
                break;
            default:
                p += extLen;
                break;
        }
        message.length -= extLen;
    }
    return errSSLSuccess;
#else
    return SSLProcessEncryptedNPNExtension(p, message.length, ctx);
#endif
}

int
SSLEncodeFinishedMessage(tls_buffer *finished,  tls_handshake_t ctx)
{
    int        err;
    tls_buffer       finishedMsg;
    unsigned		finishedSize;
	UInt8           *p;
    int             head;

	/* size and version depend on negotiatedProtocol */
	switch(ctx->negProtocolVersion) {
		case tls_protocol_version_SSL_3:
			finishedSize = 36;
			break;
        case tls_protocol_version_DTLS_1_0:
		case tls_protocol_version_TLS_1_0:
        case tls_protocol_version_TLS_1_1:
        case tls_protocol_version_TLS_1_2: /* TODO: Support variable finishedSize. */
			finishedSize = 12;
			break;
		default:
			assert(0);
			return errSSLInternal;
	}
	/* msg = type + 3 bytes len + finishedSize */
    head = SSLHandshakeHeaderSize(ctx);
    if ((err = SSLAllocBuffer(finished, finishedSize + head)) != 0)
        return err;

    p = SSLEncodeHandshakeHeader(ctx, finished, SSL_HdskFinished, finishedSize);

    finishedMsg.data = p;
    finishedMsg.length = finishedSize;

    err = ctx->sslTslCalls->computeFinishedMac(ctx, finishedMsg, ctx->isServer);

    if(err)
        return err;

    /* Keep this around for secure renegotiation */
    SSLFreeBuffer(&ctx->ownVerifyData);
    return SSLCopyBuffer(&finishedMsg, &ctx->ownVerifyData);
}

int
SSLProcessFinished(tls_buffer message, tls_handshake_t ctx)
{   int        err;
    tls_buffer       expectedFinished;
    unsigned		finishedSize;

	switch(ctx->negProtocolVersion) {
		case tls_protocol_version_SSL_3:
			finishedSize = 36;
			break;
		case tls_protocol_version_DTLS_1_0:
		case tls_protocol_version_TLS_1_0:
        case tls_protocol_version_TLS_1_1:
        case tls_protocol_version_TLS_1_2: /* TODO: Support variable finishedSize. */
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
    if ((err = SSLAllocBuffer(&expectedFinished, finishedSize)))
        return err;
    if ((err = ctx->sslTslCalls->computeFinishedMac(ctx, expectedFinished, !ctx->isServer)) != 0)
        goto fail;

    if (memcmp(expectedFinished.data, message.data, finishedSize) != 0)
    {
   		sslErrorLog("SSLProcessFinished: memcmp failure\n");
   	 	err = errSSLProtocol;
        goto fail;
    }

    /* Keep this around for secure renegotiation */
    SSLFreeBuffer(&ctx->peerVerifyData);
    err = SSLCopyBuffer(&expectedFinished, &ctx->peerVerifyData);

fail:
    SSLFreeBuffer(&expectedFinished);
    return err;
}

int
SSLEncodeServerHelloDone(tls_buffer *helloDone, tls_handshake_t ctx)
{   int          err;
    int               head;

	assert(ctx->negProtocolVersion >= tls_protocol_version_SSL_3);
    head = SSLHandshakeHeaderSize(ctx);
    if ((err = SSLAllocBuffer(helloDone, head)))
        return err;

    SSLEncodeHandshakeHeader(ctx, helloDone, SSL_HdskServerHelloDone, 0); /* Message has 0 length */

    return errSSLSuccess;
}

int
SSLProcessServerHelloDone(tls_buffer message, tls_handshake_t ctx)
{
    assert(!ctx->isServer);
    if (message.length != 0) {
    	sslErrorLog("SSLProcessServerHelloDone: nonzero msg len\n");
        return errSSLProtocol;
    }
    return errSSLSuccess;
}

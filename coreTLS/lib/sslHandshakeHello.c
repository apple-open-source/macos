/*
 * Copyright (c) 1999-2001,2005-2008,2010-2012 Apple Inc. All Rights Reserved.
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
 * sslHandshakeHello.c - Support for client hello and server hello messages.
 */

#include "tls_handshake_priv.h"
#include "sslHandshake.h"
#include "sslMemory.h"
#include "sslSession.h"
#include "sslUtils.h"
#include "sslDebug.h"
#include "sslCrypto.h"
#include "sslDigests.h"
#include "sslCipherSpecs.h"

#include <string.h>
#include <time.h>
#include <assert.h>
#include <stddef.h>

#include <inttypes.h>

int
SSLEncodeServerHelloRequest(tls_buffer *helloDone, tls_handshake_t ctx)
{   int          err;
    int               head;

	assert(ctx->negProtocolVersion >= tls_protocol_version_SSL_3);
    head = SSLHandshakeHeaderSize(ctx);
    if ((err = SSLAllocBuffer(helloDone, head)))
        return err;

    SSLEncodeHandshakeHeader(ctx, helloDone, SSL_HdskHelloRequest, 0); /* Message has 0 length */

    return errSSLSuccess;
}

/*
 * Given a protocol version sent by peer, determine if we accept that version
 * and downgrade if appropriate (which can not be done for the client side).
 */
static
int sslVerifyProtVersion(
    tls_handshake_t ctx,
    tls_protocol_version	peerVersion,	// sent by peer
    tls_protocol_version 	*negVersion)	// final negotiated version if return success
{
    if ((ctx->isDTLS)
        ? peerVersion > ctx->minProtocolVersion
        : peerVersion < ctx->minProtocolVersion) {
        return errSSLNegotiation;
    }
    if ((ctx->isDTLS)
        ? peerVersion < ctx->maxProtocolVersion
        : peerVersion > ctx->maxProtocolVersion) {
        if (!ctx->isServer) {
            return errSSLNegotiation;
        }
        *negVersion = ctx->maxProtocolVersion;
    } else {
        *negVersion = peerVersion;
    }

    return errSSLSuccess;
}


/* IE treats null session id as valid; two consecutive sessions with NULL ID
 * are considered a match. Workaround: when resumable sessions are disabled,
 * send a random session ID. */
#define SSL_IE_NULL_RESUME_BUG		1
#if		SSL_IE_NULL_RESUME_BUG
#define SSL_NULL_ID_LEN				32	/* length of bogus session ID */
#endif

int
SSLEncodeServerHello(tls_buffer *serverHello, tls_handshake_t ctx)
{   int        err;
    UInt8           *charPtr;
    int             sessionIDLen;
    size_t          msglen;
    int             head;
	size_t			extnsLen = 0;
    size_t          sctLen = 0;

    sessionIDLen = 0;
    if (ctx->sessionID.data != 0)
        sessionIDLen = (UInt8)ctx->sessionID.length;
	#if 	SSL_IE_NULL_RESUME_BUG
	if(sessionIDLen == 0) {
		sessionIDLen = SSL_NULL_ID_LEN;
	}
	#endif	/* SSL_IE_NULL_RESUME_BUG */

    msglen = 38 + sessionIDLen;

    /* We send a extension if:
        - NPN was enabled by the client app,
        - The client sent an NPN extension,
        - We didnt send the extension in a previous handshake.
       The list of protocols sent is optional */
    if (ctx->npn_enabled && ctx->npn_announced && !ctx->npn_confirmed) {
		extnsLen +=
			2 + 2 + /* ext hdr + ext len */
			ctx->npnOwnData.length;
        ctx->npn_confirmed = true;
    }

    /* We send a extension if:
     - A selected protocol was provided
     - The client sent an ALPN extension,
     The list of protocols sent is optional */
    if (ctx->alpnOwnData.length && ctx->alpn_announced) {
		extnsLen +=
        2 + 2 + 2 + /* ext hdr + ext len + list len */
        ctx->alpnOwnData.length;
        ctx->alpn_confirmed = true;
    }

    /* We send this (empty) extension if:
     - ocsp stapling was enabled
     - We received an ocsp stapling extension from the client */
    if (ctx->ocsp_peer_enabled && ctx->ocsp_enabled) {
        extnsLen += 2 + 2;
    }

    /* We send the SCT extension if:
      - the peer sent it
      - we have an SCT list set 
      - We are not resuming */
    if (ctx->sct_peer_enabled && ctx->sct_list && !ctx->sessionMatch) {
        sctLen = SSLEncodedBufferListSize(ctx->sct_list, 2);
        extnsLen += 2 + 2 + 2  /* ext hdr + ext len + list len */
                    + sctLen;
    }

    /* We send this empty SNI extension if:
     - the client sent it */
    if (ctx->peerDomainName.data != NULL) {
        extnsLen += 2 + 2; /* ext hdr + ext len */
    }

    if (ctx->secure_renegotiation) {
        extnsLen += 2 + 2 + 1 /* ext hdr + ext len + data len */
                    + ctx->ownVerifyData.length+ctx->peerVerifyData.length;
    }

    /* If we received a extended master secret in client hello */
    if (ctx->extMSEnabled && ctx->extMSReceived) {
        extnsLen += 2 + 2; /* ext hdr + ext len */
    }
    if (extnsLen) {
		msglen +=
			2 /* ServerHello extns length */
			+ extnsLen;
	}

	/* this was set to a known quantity in SSLProcessClientHello */
	check(ctx->negProtocolVersion != tls_protocol_version_Undertermined);
	sslLogNegotiateDebug("===SSL3 server: sending version %d_%d",
		ctx->negProtocolVersion >> 8, ctx->negProtocolVersion & 0xff);
	sslLogNegotiateDebug("...sessionIDLen = %d", sessionIDLen);
    head = SSLHandshakeHeaderSize(ctx);
    if ((err = SSLAllocBuffer(serverHello, msglen + head)))
        return err;

    charPtr = SSLEncodeHandshakeHeader(ctx, serverHello, SSL_HdskServerHello, msglen);

    charPtr = SSLEncodeInt(charPtr, ctx->negProtocolVersion, 2);

#if		SSL_PAC_SERVER_ENABLE
	/* serverRandom might have already been set, in SSLAdvanceHandshake() */
	if(!ctx->serverRandomValid) {
    	if ((err = SSLEncodeRandom(ctx->serverRandom, ctx)) != 0) {
       		return err;
		}
	}
#else
	/* This is the normal production code path */
    if ((err = SSLEncodeRandom(ctx->serverRandom, ctx)) != 0)
        return err;
#endif	/* SSL_PAC_SERVER_ENABLE */

	memcpy(charPtr, ctx->serverRandom, SSL_CLIENT_SRVR_RAND_SIZE);

    charPtr += SSL_CLIENT_SRVR_RAND_SIZE;
	*(charPtr++) = (UInt8)sessionIDLen;
	#if 	SSL_IE_NULL_RESUME_BUG
	if(ctx->sessionID.data != NULL) {
		/* normal path for enabled resumable session */
		memcpy(charPtr, ctx->sessionID.data, sessionIDLen);
	}
	else {
		/* IE workaround */
		tls_buffer rb;
		rb.data = charPtr;
		rb.length = SSL_NULL_ID_LEN;
		sslRand(&rb);
	}
	#else
    if (sessionIDLen > 0)
        memcpy(charPtr, ctx->sessionID.data, sessionIDLen);
	#endif	/* SSL_IE_NULL_RESUME_BUG */
	charPtr += sessionIDLen;
    charPtr = SSLEncodeInt(charPtr, ctx->selectedCipher, 2);
    *(charPtr++) = 0;      /* Null compression */

    sslLogNegotiateDebug("TLS server specifying cipherSuite 0x%04x",
		(unsigned long)ctx->selectedCipher);

    /* Encoding extensions */
	if (extnsLen) {
		charPtr = SSLEncodeInt(charPtr, extnsLen, 2);

        if (ctx->peerDomainName.data != NULL) {
            charPtr = SSLEncodeInt(charPtr, SSL_HE_ServerName, 2);
            charPtr = SSLEncodeInt(charPtr, 0, 2);
        }

        /* We are confirming we support NPN, and sending a list of protocols */
        if (ctx->npn_confirmed) {
            charPtr = SSLEncodeInt(charPtr, SSL_HE_NPN, 2);
            charPtr = SSLEncodeInt(charPtr, ctx->npnOwnData.length, 2);
            memcpy(charPtr, ctx->npnOwnData.data, ctx->npnOwnData.length);
            charPtr += ctx->npnOwnData.length;
		}

        if(ctx->alpn_confirmed) {
            charPtr = SSLEncodeInt(charPtr, SSL_HE_ALPN, 2);
			charPtr = SSLEncodeInt(charPtr, ctx->alpnOwnData.length + 2, 2);
			charPtr = SSLEncodeInt(charPtr, ctx->alpnOwnData.length, 2);

			memcpy(charPtr, ctx->alpnOwnData.data, ctx->alpnOwnData.length);
			charPtr += ctx->alpnOwnData.length;
        }

        if(ctx->ocsp_enabled && ctx->ocsp_peer_enabled) {
            charPtr = SSLEncodeInt(charPtr, SSL_HE_StatusReguest, 2);
            charPtr = SSLEncodeInt(charPtr, 0, 2);
        }

        if(sctLen) {
            charPtr = SSLEncodeInt(charPtr, SSL_HE_SCT, 2);
            charPtr = SSLEncodeInt(charPtr, sctLen + 2, 2);
            charPtr = SSLEncodeInt(charPtr, sctLen, 2);
            charPtr = SSLEncodeBufferList(ctx->sct_list, 2, charPtr);
        }

        /* RFC 5746: Secure Renegotiation */
        if(ctx->secure_renegotiation) {
            charPtr = SSLEncodeInt(charPtr, SSL_HE_SecureRenegotation, 2);
            charPtr = SSLEncodeInt(charPtr, ctx->ownVerifyData.length+ctx->peerVerifyData.length+1, 2);
            charPtr = SSLEncodeInt(charPtr, ctx->ownVerifyData.length+ctx->peerVerifyData.length, 1);
            memcpy(charPtr, ctx->peerVerifyData.data, ctx->peerVerifyData.length);
            charPtr += ctx->peerVerifyData.length;
            memcpy(charPtr, ctx->ownVerifyData.data, ctx->ownVerifyData.length);
            charPtr += ctx->ownVerifyData.length;
        }

        if (ctx->extMSEnabled && ctx->extMSReceived) {
            charPtr = SSLEncodeInt(charPtr, SSL_HE_ExtendedMasterSecret, 2);
            charPtr = SSLEncodeInt(charPtr, 0, 2);
        }
    }

    // This will catch inconsistency between encoded and actual length.
    if(charPtr != serverHello->data + serverHello->length) {
        assert(0);
        return errSSLInternal;
    } else {
        return errSSLSuccess;
    }
}

int
SSLEncodeServerHelloVerifyRequest(tls_buffer *helloVerifyRequest, tls_handshake_t ctx)
{   int        err;
    UInt8           *charPtr;
    size_t          msglen;
    int             head;

    assert(ctx->isServer);
    assert(ctx->isDTLS);
    assert(ctx->dtlsCookie.length);

    msglen = 3 + ctx->dtlsCookie.length;

    head = SSLHandshakeHeaderSize(ctx);
    if ((err = SSLAllocBuffer(helloVerifyRequest, msglen + head)))
        return err;

    charPtr = SSLEncodeHandshakeHeader(ctx, helloVerifyRequest, SSL_HdskHelloVerifyRequest, msglen);

    charPtr = SSLEncodeInt(charPtr, ctx->negProtocolVersion, 2);

    *charPtr++ = ctx->dtlsCookie.length;
    memcpy(charPtr, ctx->dtlsCookie.data, ctx->dtlsCookie.length);
    charPtr += ctx->dtlsCookie.length;

    if(charPtr != (helloVerifyRequest->data + helloVerifyRequest->length)) {
        assert(0);
        return errSSLInternal;
    } else {
        return errSSLSuccess;
    }
}


int
SSLProcessServerHelloVerifyRequest(tls_buffer message, tls_handshake_t ctx)
{   int            err;
    tls_protocol_version  protocolVersion;
    unsigned int        cookieLen;
    UInt8               *p;

    assert(!ctx->isServer);

    /* TODO: those length values should not be hardcoded */
    /* 3 bytes at least with empty cookie */
    if (message.length < 3 ) {
	sslErrorLog("SSLProcessServerHelloVerifyRequest: msg len error\n");
        return errSSLProtocol;
    }
    p = message.data;

    protocolVersion = (tls_protocol_version)SSLDecodeInt(p, 2);
    p += 2;

    /* TODO: Not clear what else to do with protocol version here */
    if(protocolVersion != tls_protocol_version_DTLS_1_0) {
        sslErrorLog("SSLProcessServerHelloVerifyRequest: protocol version error\n");
        return errSSLProtocol;
    }

    cookieLen = *p++;
    sslLogNegotiateDebug("cookieLen = %d, msglen=%d\n", (int)cookieLen, (int)message.length);
    /* TODO: hardcoded '15' again */
    if (message.length < (3 + cookieLen)) {
	sslErrorLog("SSLProcessServerHelloVerifyRequest: msg len error 2\n");
        return errSSLProtocol;
    }

    err = SSLAllocBuffer(&ctx->dtlsCookie, cookieLen);
    if (err == 0)
        memcpy(ctx->dtlsCookie.data, p, cookieLen);

    return err;
}

static void
SSLProcessServerHelloExtension_SecureRenegotiation(tls_handshake_t ctx, UInt16 extLen, UInt8 *p)
{
    if(extLen!= (1 + ctx->ownVerifyData.length + ctx->peerVerifyData.length))
        return;

    if(*p!=ctx->ownVerifyData.length + ctx->peerVerifyData.length)
        return;
    p++;

    if(memcmp(p, ctx->ownVerifyData.data, ctx->ownVerifyData.length))
        return;
    p+=ctx->ownVerifyData.length;

    if(memcmp(p, ctx->peerVerifyData.data, ctx->peerVerifyData.length))
        return;

    ctx->secure_renegotiation_received = true;
}

static int
SSLProcessServerHelloExtension_NPN(tls_handshake_t ctx, UInt16 extLen, UInt8 *p)
{
    if (!ctx->npn_announced)
        return errSSLProtocol;

	ctx->npn_received = true;

    /* We already have an NPN extension */
    if(ctx->npnPeerData.data)
        return errSSLProtocol;

    return SSLCopyBufferFromData(p, extLen, &ctx->npnPeerData);
}

static int
SSLProcessServerHelloExtension_ALPN(tls_handshake_t ctx, UInt16 extLen, UInt8 *p)
{
    size_t plen;

    if (!ctx->alpn_announced)
        return errSSLProtocol;

    /* Need at least 4 bytes */
    if (extLen<=3)
        return errSSLProtocol;

	ctx->alpn_received = true;

    /* We already have an ALPN extension */
    if(ctx->alpnPeerData.data)
        return errSSLProtocol;

    plen = SSLDecodeSize(p, 2); p+=2;

    if (plen!=extLen-2)
        return errSSLProtocol;

    return SSLCopyBufferFromData(p, plen, &ctx->alpnPeerData);
}

static int
SSLProcessServerHelloExtension_StatusRequest(tls_handshake_t ctx, UInt16 extLen, UInt8 *p)
{
    if (!ctx->ocsp_enabled)
        return errSSLProtocol;

    if(extLen!=0)
        return errSSLProtocol;

    ctx->ocsp_peer_enabled = true;

    return 0;
}

static int
SSLProcessServerHelloExtension_SCT(tls_handshake_t ctx, uint16_t extLen, uint8_t *p)
{
    size_t listLen;

    if (!ctx->sct_enabled)
        return errSSLProtocol;

    if(extLen<2) {
        sslErrorLog("SSLProcessClientHelloExtension_SCT: length decode error 1\n");
        return errSSLProtocol;
    }

    listLen = SSLDecodeSize(p, 2); p+=2; extLen-=2;

    if(extLen!=listLen) {
        sslErrorLog("SSLProcessClientHelloExtension_SCT: length decode error 2\n");
        return errSSLProtocol;
    }

    return SSLDecodeBufferList(p, listLen, 2, &ctx->sct_list);
}


static int
SSLProcessServerHelloExtension_SessionTicket(tls_handshake_t ctx, UInt16 extLen, UInt8 *p)
{
    if (!ctx->sessionTicket_announced)
        return errSSLProtocol;

    if(extLen!=0)
        return errSSLProtocol;

    ctx->sessionTicket_confirmed = true;

    return 0;
}

static int
SSLProcessServerHelloExtensions(tls_handshake_t ctx, UInt16 extensionsLen, UInt8 *p)
{
    Boolean got_secure_renegotiation = false;
    UInt16 remaining;
    OSStatus err;

    if(extensionsLen<2) {
        sslErrorLog("SSLProcessHelloExtensions: need a least 2 bytes\n");
        return errSSLProtocol;
    }

    /* Reset state of Server Hello extensions */
    ctx->ocsp_peer_enabled = false;
    ctx->sct_peer_enabled = false;
    ctx->secure_renegotiation_received = false;
    tls_free_buffer_list(ctx->sct_list);
    ctx->extMSReceived = false;

    remaining = SSLDecodeInt(p, 2); p+=2;
    extensionsLen -=2;

    /* remaining = number of bytes remaining to process according to buffer data */
    /* extensionsLen = number of bytes in the buffer */

    if(remaining>extensionsLen) {
        sslErrorLog("SSLProcessHelloExtensions: ext len error 1\n");
        return errSSLProtocol;
    }

    if(remaining<extensionsLen) {
        sslErrorLog("Warning: SSLProcessServerHelloExtensions: Too many bytes\n");
    }

    while(remaining) {
        UInt16 extType;
        UInt16 extLen;

        if (remaining<4) {
            sslErrorLog("SSLProcessHelloExtensions: ext len error\n");
            return errSSLProtocol;
        }

        extType = SSLDecodeInt(p, 2); p+=2;
        extLen = SSLDecodeInt(p, 2); p+=2;

        if (remaining<(4+extLen)) {
            sslErrorLog("SSLProcessHelloExtension: ext len error 2\n");
            return errSSLProtocol;
        }
        remaining -= (4+extLen);

        switch (extType) {
            case SSL_HE_SecureRenegotation:
                if(got_secure_renegotiation)
                    return errSSLProtocol;            /* Fail if we already processed one */
                got_secure_renegotiation = true;
                SSLProcessServerHelloExtension_SecureRenegotiation(ctx, extLen, p);
                break;
            case SSL_HE_NPN:
                if ((err = SSLProcessServerHelloExtension_NPN(ctx, extLen, p)) != 0)
                    return err;
                break;
            case SSL_HE_ALPN:
                if ((err = SSLProcessServerHelloExtension_ALPN(ctx, extLen, p)) != 0)
                    return err;
                break;
            case SSL_HE_SCT:
                if ((err = SSLProcessServerHelloExtension_SCT(ctx, extLen, p)) != 0)
                    return err;
                break;
            case SSL_HE_StatusReguest:
                if ((err = SSLProcessServerHelloExtension_StatusRequest(ctx, extLen, p)) != 0)
                    return err;
                break;
            case SSL_HE_SessionTicket:
                if ((err = SSLProcessServerHelloExtension_SessionTicket(ctx, extLen, p)) != 0)
                    return err;
                break;
            case SSL_HE_ExtendedMasterSecret:
                ctx->extMSReceived = true;
                break;
            default:
                /*
                 Do nothing for other extensions. Per RFC 5246, we should (MUST) error
                 if we received extensions we didnt specify in the Client Hello.
                 Client should also abort handshake if multiple extensions of the same
                 type are found
                 */
                break;
        }
        p+=extLen;
    }

    return errSSLSuccess;
}



int
SSLProcessServerHello(tls_buffer message, tls_handshake_t ctx)
{   int            err;
    tls_protocol_version  protocolVersion, negVersion;
    size_t              sessionIDLen;
    size_t              extensionsLen;
    UInt8               *p;

    assert(!ctx->isServer);

    if (message.length < 38) {
    	sslErrorLog("SSLProcessServerHello: msg len error\n");
        return errSSLProtocol;
    }
    p = message.data;

    protocolVersion = (tls_protocol_version)SSLDecodeInt(p, 2);
    p += 2;
	/* FIXME this should probably send appropriate alerts */
	err = sslVerifyProtVersion(ctx, protocolVersion, &negVersion);
	if(err) {
		return err;
	}
    ctx->negProtocolVersion = negVersion;
	switch(negVersion) {
		case tls_protocol_version_SSL_3:
			ctx->sslTslCalls = &Ssl3Callouts;
			break;
		case tls_protocol_version_TLS_1_0:
        case tls_protocol_version_TLS_1_1:
        case tls_protocol_version_DTLS_1_0:
 			ctx->sslTslCalls = &Tls1Callouts;
			break;
        case tls_protocol_version_TLS_1_2:
			ctx->sslTslCalls = &Tls12Callouts;
			break;
		default:
			return errSSLNegotiation;
	}
    err = ctx->callbacks->set_protocol_version(ctx->callback_ctx, negVersion);
    if(err) {
        return err;
    }

    sslLogNegotiateDebug("SSLProcessServerHello: negVersion is %d_%d",
		(negVersion >> 8) & 0xff, negVersion & 0xff);

    memcpy(ctx->serverRandom, p, 32);
    p += 32;

    sessionIDLen = *p++;
    if (message.length < (38 + sessionIDLen)) {
    	sslErrorLog("SSLProcessServerHello: msg len error 2\n");
        return errSSLProtocol;
    }
    SSLFreeBuffer(&ctx->sessionID);
    if (sessionIDLen > 0 && ctx->peerID.data != 0)
    {   /* Don't die on error; just treat it as an uncached session */
        err = SSLAllocBuffer(&ctx->sessionID, sessionIDLen);
        if (err == 0)
            memcpy(ctx->sessionID.data, p, sessionIDLen);
    }
    p += sessionIDLen;

    ctx->selectedCipher = (UInt16)SSLDecodeInt(p,2);
    sslLogNegotiateDebug("SSLProcessServerHello: server selected ciphersuite %04x",
    	(unsigned)ctx->selectedCipher);
    p += 2;
    if ((err = ValidateSelectedCiphersuite(ctx)) != 0) {
        return err;
    }

    if (*p++ != 0)      /* Compression */
        return errSSLUnimplemented;

    /* Process ServerHello extensions */
    extensionsLen = message.length - (38 + sessionIDLen);

    if(extensionsLen) {
        err = SSLProcessServerHelloExtensions(ctx, extensionsLen, p);
        if(err)
            return err;
    }

    /* RFC 5746: Make sure the renegotiation is secure */
    if(ctx->secure_renegotiation && !ctx->secure_renegotiation_received)
        return errSSLNegotiation;

    if(ctx->secure_renegotiation_received)
        ctx->secure_renegotiation = true;

    
	/*
	 * Note: the server MAY send a SSL_HE_EC_PointFormats extension if
	 * we've negotiated an ECDSA ciphersuite...but
	 * a) the provided format list MUST contain SSL_PointFormatUncompressed per
	 *    RFC 4492 5.2; and
	 * b) The uncompressed format is the only one we support.
	 *
	 * Thus we drop a possible incoming SSL_HE_EC_PointFormats extension here.
	 * IF we ever support other point formats, we have to parse the extension
	 * to see what the server supports.
	 */
    return errSSLSuccess;
}

int
SSLEncodeClientHello(tls_buffer *clientHello, tls_handshake_t ctx)
{
	size_t          length;
    unsigned        i;
    int             err;
    unsigned char   *p;
    tls_buffer      sessionIdentifier = { 0, NULL };
    tls_buffer      sessionTicket = { 0, NULL };
    size_t          sessionIDLen;
	size_t			sessionTicketLen = 0;
	size_t			serverNameLen = 0;
	size_t			pointFormatLen = 0;
	size_t			suppCurveLen = 0;
	size_t			signatureAlgorithmsLen = 0;
    size_t          npnLen = 0;  /* NPN support for SPDY */
    size_t          alpnLen = 0; /* ALPN support for SPDY, HTTP 2.0, ... */
    size_t          ocspLen = 0; /* OCSP Stapling extension */
    size_t          sctLen = 0;  /* SCT extension */
    size_t          extMasterSecretLen = 0;
    size_t          paddingLen = 0;  /* Padding extension */
	size_t			totalExtenLen = 0;
    UInt16          numCipherSuites = 0;
    int             head;

    assert(!ctx->isServer);

	clientHello->length = 0;
	clientHello->data = NULL;

    sessionIDLen = 0;

    head = SSLHandshakeHeaderSize(ctx);

    if (ctx->externalSessionTicket.length)
    {
        sessionTicket = ctx->externalSessionTicket;
    }
    else if ((ctx->resumableSession.data != 0) && (SSLClientValidateSessionDataBefore(ctx->resumableSession, ctx) == 0))
    {

        /* We should always have a sessionID, even when using tickets. */
        err = SSLRetrieveSessionID(ctx->resumableSession, &sessionIdentifier);
        if(err != 0) {
            goto err_exit; // error means something really wrong.
        }

        err = SSLRetrieveSessionTicket(ctx->resumableSession, &sessionTicket);
        if (err != 0) {
            goto err_exit; // error means something really wrong.
        }

        /* We resume if this is not a ticket, or if tickets are enabled 
           and if other session parameters are valid for the current context */
        if ((sessionTicket.length == 0) || (ctx->sessionTicket_enabled))
        {
            sessionIDLen = sessionIdentifier.length;
            SSLCopyBuffer(&sessionIdentifier, &ctx->proposedSessionID);
        }
    }

    /* Count valid ciphersuites */
    for(i=0;i<ctx->numEnabledCipherSuites; i++) {
        if(tls_handshake_ciphersuite_is_valid(ctx, ctx->enabledCipherSuites[i])) {
            numCipherSuites++;
        }
    }

    /* RFC 5746 : add the fake ciphersuite unless we are including the extension */
    if(!ctx->secure_renegotiation)
        numCipherSuites+=1;
    /* https://tools.ietf.org/html/draft-ietf-tls-downgrade-scsv-00 */
    if(ctx->fallback)
        numCipherSuites+=1;

    length = 39 + 2*numCipherSuites + sessionIDLen;

    /* We always use the max enabled version in the ClientHello.client_version,
       even in the renegotiation case. This value is saved in the context so it
       can be used in the RSA key exchange */
	err = sslGetMaxProtVersion(ctx, &ctx->clientReqProtocol);
	if(err) {
		/* we don't have a protocol enabled */
		goto err_exit;
	}

    /* RFC 5746: If are starting a new handshake, so we didnt received this yet */
    ctx->secure_renegotiation_received = false;

    /* This is the protocol version used at the record layer, If we already
     negotiated the protocol version previously, we should just use that,
     otherwise we use the the minimum supported version.
     We do not always use the minimum version because some TLS only servers
     will reject an SSL 3 version in client_hello.
    */
    tls_protocol_version pv;
    if(ctx->negProtocolVersion != tls_protocol_version_Undertermined) {
        pv = ctx->negProtocolVersion;
    } else {
        if(ctx->minProtocolVersion<tls_protocol_version_TLS_1_0 && ctx->maxProtocolVersion>=tls_protocol_version_TLS_1_0)
            pv = tls_protocol_version_TLS_1_0;
        else
            pv = ctx->minProtocolVersion;
    }

    //Initialize the record layer protocol version
    //FIXME: error handling.
    ctx->callbacks->set_protocol_version(ctx->callback_ctx, pv);


#if ENABLE_DTLS
    if(ctx->isDTLS) {
        /* extra space for cookie */
        /* TODO: cookie len - 0 for now */
        length += 1 + ctx->dtlsCookie.length;
        sslLogNegotiateDebug("DTLS Hello: len=%lu\n", length);
    }
    /* Because of the way the version number for DTLS is encoded,
     the following code mean that you can use extensions with DTLS... */
#endif /* ENABLE_DTLS */

    /* RFC 5746: We add the extension only for renegotiation ClientHello */
    if(ctx->secure_renegotiation) {
        totalExtenLen += 2 + /* extension type */
                         2 + /* extension length */
                         1 + /* lenght of renegotiated_conection (client verify data) */
                         ctx->ownVerifyData.length;
    }

    /* prepare for optional ClientHello extensions */
	if((ctx->clientReqProtocol >= tls_protocol_version_TLS_1_0) &&
	   (ctx->peerDomainName.data != NULL) &&
	   (ctx->peerDomainName.length != 0)) {
		serverNameLen = 2 +	/* extension type */
						2 + /* 2-byte vector length, extension_data */
						2 + /* length of server_name_list */
						1 +	/* length of name_type */
						2 + /* length of HostName */
						ctx->peerDomainName.length;
		totalExtenLen += serverNameLen;
	}
	if(ctx->sessionTicket_enabled || ctx->externalSessionTicket.length) {
		sessionTicketLen = 2 +	/* extension type */
						   2 + /* 2-byte vector length, extension_data */
						   sessionTicket.length;
		totalExtenLen += sessionTicketLen;
        ctx->sessionTicket_announced = true;
	}
	if((ctx->clientReqProtocol >= tls_protocol_version_TLS_1_0) &&
	   (ctx->ecdsaEnable)) {
		/* Two more extensions: point format, supported curves */
		pointFormatLen = 2 +	/* extension type */
						 2 +	/* 2-byte vector length, extension_data */
						 1 +    /* length of the ec_point_format_list */
						 1;		/* the single format we support */
		suppCurveLen   = 2 +	/* extension type */
						 2 +	/* 2-byte vector length, extension_data */
						 2 +    /* length of the elliptic_curve_list */
						(2 * ctx->ecdhNumCurves);	/* each curve is 2 bytes */
		totalExtenLen += (pointFormatLen + suppCurveLen);
	}
    if(ctx->isDTLS
       ? ctx->clientReqProtocol < tls_protocol_version_DTLS_1_0
       : ctx->clientReqProtocol >= tls_protocol_version_TLS_1_2) {
        signatureAlgorithmsLen = 2 +	/* extension type */
                                 2 +	/* 2-byte vector length, extension_data */
                                 2 +    /* length of signatureAlgorithms list */
                                 2 * ctx->numLocalSigAlgs;
		totalExtenLen += signatureAlgorithmsLen;
    }

    /* Only send NPN extension if:
        - Requested protocol is appropriate
        - Client app enabled it.
        - We didnt send it in a previous handshake
     */
    if (ctx->clientReqProtocol >= tls_protocol_version_TLS_1_0  && ctx->npn_enabled && !ctx->npn_announced) {
        npnLen = 2 +    /* extension type */
                 2 +    /* extension length */
                 0;     /* empty extension data */
        totalExtenLen += npnLen;
    }

    /* Only send ALPN extension if:
     - Requested protocol is appropriate
     - Client app set a list of protocols.
     - We didnt send it in a previous handshake
     */
    if (ctx->clientReqProtocol >= tls_protocol_version_TLS_1_0 && ctx->alpnOwnData.length) {
        alpnLen = 2 +               /* extension type */
        2 +                         /* extension length */
        2 +                         /* protocol list len */
        ctx->alpnOwnData.length;    /* protocol list data */
        totalExtenLen += alpnLen;
    }

    /* Send the status request extension (OCSP stapling) RFC 6066 if enabled */
    if (ctx->clientReqProtocol >= tls_protocol_version_TLS_1_0 && ctx->ocsp_enabled) {
        ocspLen = 2 +   /* extension type */
        2 +             /* extension lenght */
        1 +             /* status_type */
        2 + SSLEncodedBufferListSize(ctx->ocsp_responder_id_list, 2) +            /* responder ID list len */
        2 + ctx->ocsp_request_extensions.length;              /* extensions len */
        totalExtenLen += ocspLen;
    }

    /* Send the SCT extension (RFC 6962) if enabled */
    if (ctx->clientReqProtocol >= tls_protocol_version_TLS_1_0 && ctx->sct_enabled) {
        sctLen = 2 +    /* extension type */
        2 +             /* extension lenght */
        0;              /* empty extension data */
        totalExtenLen += sctLen;
    }

    /* Extended Master Secret */
    if (ctx->clientReqProtocol >= tls_protocol_version_TLS_1_0 && ctx->extMSEnabled) {
        extMasterSecretLen = 2 +
        2 +
        0;
        totalExtenLen += extMasterSecretLen;
    }

    /* Last extension is the padding one: 
       Some F5 load balancers (and maybe other TLS implementations) will hang
       if the ClientHello size is between 256 and 511 (inclusive). So we make
       sure our ClientHello is always smaller than 256 or bigger than 512.
       See section 4 in https://tools.ietf.org/html/draft-ietf-tls-padding-01
     */
    if(((length + totalExtenLen + 2 + head) >= 256) &&
       ((length + totalExtenLen + 2 + head) < 512)) {
        paddingLen = (512 - (length + totalExtenLen + 2 + head));
        if(paddingLen<4) paddingLen = 4;
        totalExtenLen += paddingLen;
    }

	if(totalExtenLen != 0) {
		/*
		 * Total length extensions have to fit in a 16 bit field...
		 */
		if(totalExtenLen > 0xffff) {
			sslErrorLog("Total extensions length EXCEEDED\n");
			totalExtenLen = 0;
			sessionTicketLen = 0;
			serverNameLen = 0;
			pointFormatLen = 0;
			suppCurveLen = 0;
            signatureAlgorithmsLen = 0;
		}
		else {
			/* add length of total length plus lengths of extensions */
			length += (totalExtenLen + 2);
		}
	}

    if ((err = SSLAllocBuffer(clientHello, length + head)))
        goto err_exit;

    p = SSLEncodeHandshakeHeader(ctx, clientHello, SSL_HdskClientHello, length);

    p = SSLEncodeInt(p, ctx->clientReqProtocol, 2);

	sslLogNegotiateDebug("Requesting protocol version %04x", ctx->clientReqProtocol);
   if ((err = SSLEncodeRandom(p, ctx)) != 0)
    {   goto err_exit;
    }
    memcpy(ctx->clientRandom, p, SSL_CLIENT_SRVR_RAND_SIZE);
    p += 32;
    *p++ = sessionIDLen;    				/* 1 byte vector length */
    sslLogNegotiateDebug("SessionIDLen = %lu\n", sessionIDLen);

    if (sessionIDLen > 0)
    {
        memcpy(p, sessionIdentifier.data, sessionIDLen);
        sslLogNegotiateDebug("SessionID = %02x...\n", p[0]);
    }
    p += sessionIDLen;
#if ENABLE_DTLS
    if (ctx->isDTLS) {
        /* TODO: Add the cookie ! Currently: size=0 -> no cookie */
        *p++ = ctx->dtlsCookie.length;
        if(ctx->dtlsCookie.length) {
            memcpy(p, ctx->dtlsCookie.data, ctx->dtlsCookie.length);
            p+=ctx->dtlsCookie.length;
        }
        sslLogNegotiateDebug("DTLS Hello: cookie len = %d\n",(int)ctx->dtlsCookie.length);
    }
#endif


    p = SSLEncodeInt(p, 2*numCipherSuites, 2);
    /* 2 byte long vector length */

    /* RFC 5746 : add the fake ciphersuite unless we are including the extension */
    if(!ctx->secure_renegotiation) {
        p = SSLEncodeInt(p, TLS_EMPTY_RENEGOTIATION_INFO_SCSV, 2);
    }
    
    /* https://tools.ietf.org/html/draft-ietf-tls-downgrade-scsv-00 */
    if(ctx->fallback) {
        p = SSLEncodeInt(p, TLS_FALLBACK_SCSV, 2);
    }

    for (i = 0; i<ctx->numEnabledCipherSuites; ++i) {
        if(tls_handshake_ciphersuite_is_valid(ctx, ctx->enabledCipherSuites[i])) {
            sslLogNegotiateDebug("Sending ciphersuite %04x",
                        (unsigned)ctx->enabledCipherSuites[i]);
            p = SSLEncodeInt(p, ctx->enabledCipherSuites[i], 2);
        }
	}

    *p++ = 1;                               /* 1 byte long vector */
    *p++ = 0;                               /* null compression */

	/*
	 * Append ClientHello extensions.
	 */
	if (totalExtenLen != 0) {
		/* first, total length of all extensions */
		p = SSLEncodeSize(p, totalExtenLen, 2);
	}

    if (ctx->secure_renegotiation && p != NULL){
        assert(ctx->ownVerifyData.length<=255);
        p = SSLEncodeInt(p, SSL_HE_SecureRenegotation, 2);
        p = SSLEncodeSize(p, ctx->ownVerifyData.length+1, 2);
        p = SSLEncodeSize(p, ctx->ownVerifyData.length, 1);
        memcpy(p, ctx->ownVerifyData.data, ctx->ownVerifyData.length);
        p += ctx->ownVerifyData.length;
    }

	if (sessionTicketLen > 0 && p != NULL) {
		sslLogNegotiateDebug("Adding %lu bytes of sessionTicket to ClientHello",
                             sessionTicket.length);
   		p = SSLEncodeInt(p, SSL_HE_SessionTicket, 2);
		p = SSLEncodeSize(p, sessionTicket.length, 2);
		memcpy(p, sessionTicket.data, sessionTicket.length);
		p += sessionTicket.length;
	}

	if (serverNameLen > 0 && p != NULL) {
		sslLogNegotiateDebug("Specifying ServerNameIndication");
		p = SSLEncodeInt(p, SSL_HE_ServerName, 2);
		p = SSLEncodeSize(p, ctx->peerDomainName.length + 5, 2);
		p = SSLEncodeSize(p, ctx->peerDomainName.length + 3, 2);
		p = SSLEncodeInt(p, SSL_NT_HostName, 1);
		p = SSLEncodeSize(p, ctx->peerDomainName.length, 2);
		memcpy(p, ctx->peerDomainName.data, ctx->peerDomainName.length);
		p += ctx->peerDomainName.length;
	}

	if (suppCurveLen > 0 && p != NULL) {
		UInt32 len = 2 * ctx->ecdhNumCurves;
		unsigned dex;
		p = SSLEncodeInt(p, SSL_HE_EllipticCurves, 2);
		p = SSLEncodeSize(p, len+2, 2);		/* length of extension data */
		p = SSLEncodeSize(p, len, 2);		/* length of elliptic_curve_list */
		for(dex=0; dex<ctx->ecdhNumCurves; dex++) {
			sslEcdsaDebug("+++ adding supported curves %u to ClientHello",
				(unsigned)ctx->ecdhCurves[dex]);
			p = SSLEncodeInt(p, ctx->ecdhCurves[dex], 2);
		}
	}

	if (pointFormatLen > 0 && p != NULL) {
		sslEcdsaDebug("+++ adding point format to ClientHello");
		p = SSLEncodeInt(p, SSL_HE_EC_PointFormats, 2);
		p = SSLEncodeSize(p, 2, 2);		/* length of extension data */
		p = SSLEncodeSize(p, 1, 1);		/* length of ec_point_format_list */
		p = SSLEncodeInt(p, SSL_PointFormatUncompressed, 1);
	}
    
    if (signatureAlgorithmsLen > 0 && p != NULL) {
        sslEcdsaDebug("+++ adding signature algorithms to ClientHello");
        UInt32 len = 2 * ctx->numLocalSigAlgs;
        p = SSLEncodeInt(p, SSL_HE_SignatureAlgorithms, 2);
        p = SSLEncodeSize(p, len+2, 2);		/* length of extension data */
        p = SSLEncodeSize(p, len, 2);		/* length of extension data */

        for (int i = 0; i < ctx->numLocalSigAlgs; i++) {
            p = SSLEncodeInt(p, ctx->localSigAlgs[i].hash, 1);
            p = SSLEncodeInt(p, ctx->localSigAlgs[i].signature, 1);
        }
    }

    if (npnLen > 0 && p != NULL) {
        ctx->npn_announced = true;
        p = SSLEncodeInt(p, SSL_HE_NPN, 2);
        p = SSLEncodeSize(p, 0, 2);
    }

    if (alpnLen > 0 && p != NULL) {
        ctx->alpn_announced = true;
        p = SSLEncodeInt(p, SSL_HE_ALPN, 2);
        p = SSLEncodeSize(p, ctx->alpnOwnData.length + 2, 2);
        p = SSLEncodeSize(p, ctx->alpnOwnData.length, 2);
        memcpy(p, ctx->alpnOwnData.data, ctx->alpnOwnData.length);
        p += ctx->alpnOwnData.length;
    }

    if (ocspLen > 0 && p != NULL) {
        p = SSLEncodeInt(p, SSL_HE_StatusReguest, 2);
        p = SSLEncodeSize(p, ocspLen-4, 2);
        *p++ = SSL_CST_Ocsp;
        p = SSLEncodeSize(p, SSLEncodedBufferListSize(ctx->ocsp_responder_id_list, 2), 2);
        p = SSLEncodeBufferList(ctx->ocsp_responder_id_list, 2, p);
        p = SSLEncodeSize(p, ctx->ocsp_request_extensions.length, 2);
        memcpy(p, ctx->ocsp_request_extensions.data, ctx->ocsp_request_extensions.length);
        p += ctx->ocsp_request_extensions.length;
    }

    if (sctLen > 0 && p != NULL) {
        p = SSLEncodeInt(p, SSL_HE_SCT, 2);
        p = SSLEncodeSize(p, 0, 2);
    }

    if (extMasterSecretLen > 0 && p != NULL) {
        p = SSLEncodeInt(p, SSL_HE_ExtendedMasterSecret, 2);
        p = SSLEncodeSize(p, 0, 2);
    }

    if (paddingLen > 0 && p != NULL) {
        p = SSLEncodeInt(p, SSL_HE_Padding, 2);
        p = SSLEncodeSize(p, paddingLen-4, 2);
        memset(p, 0, paddingLen-4);
        p += paddingLen-4;
    }

    sslLogNegotiateDebug("Client Hello : data=%p p=%p len=%08lx\n", clientHello->data, p, (unsigned long)clientHello->length);

    assert((clientHello->length<256) || (clientHello->length>=512));

    if (p != clientHello->data + clientHello->length) {
        assert(0);
        err = errSSLInternal;
        goto err_exit;
    }

    if ((err = SSLInitMessageHashes(ctx)) != 0) {
        goto err_exit;
    }

err_exit:
	if (err != 0) {
		SSLFreeBuffer(clientHello);
	}

	return err;
}

static int
SSLProcessClientHelloExtension_NPN(tls_handshake_t ctx, uint16_t extenLen, uint8_t *charPtr)
{
    if (ctx->npn_announced)
        return errSSLProtocol;

    if (extenLen!=0) {
        sslErrorLog("SSLProcessClientHelloExtension_NPN: length decode error 1\n");
        return errSSLProtocol;
    }

    /* if the client sent npn extension, let the server know so it can set the NPN data */
    ctx->npn_announced = true;
    return 0;
}

static int
SSLProcessClientHelloExtension_SCT(tls_handshake_t ctx, uint16_t extenLen, uint8_t *charPtr)
{
    if (extenLen!=0) {
        sslErrorLog("SSLProcessClientHelloExtension_SCT: length decode error 1\n");
        return errSSLProtocol;
    }

    ctx->sct_peer_enabled = true;
    return 0;
}

static int
SSLProcessClientHelloExtension_StatusRequest(tls_handshake_t ctx, uint16_t extenLen, uint8_t *charPtr)
{
    int err;
    uint8_t status_type;

    if(extenLen<1) {
        sslErrorLog("SSLProcessClientHelloExtension_StatusRequest: length decode error 1\n");
        return errSSLProtocol;
    }

    status_type  = *charPtr++;
    extenLen-=1;

    if(ctx->ocsp_enabled && (status_type==SSL_CST_Ocsp)) {

        size_t len;
        if(extenLen<2) {
            sslErrorLog("SSLProcessClientHelloExtension_StatusRequest: length decode error 2\n");
            return errSSLProtocol;
        }

        len = SSLDecodeSize(charPtr,2); charPtr += 2; extenLen -= 2;
        if (len > extenLen) {
            sslErrorLog("SSLProcessClientHelloExtension_StatusRequest: length decode error 3\n");
            return errSSLProtocol;
        }

        if((err = SSLDecodeBufferList(charPtr, len, 2, &ctx->ocsp_responder_id_list))) {
            return err;
        }
        charPtr+=len; extenLen-=len;

        if (extenLen < 2) {
            sslErrorLog("SSLProcessClientHelloExtension_StatusRequest: length decode error 4\n");
            return errSSLProtocol;
        }

        len = SSLDecodeSize(charPtr,2); charPtr += 2; extenLen -= 2;

        if (len != extenLen) {
            sslErrorLog("SSLProcessClientHelloExtension_StatusRequest: length decode error 5\n");
            return errSSLProtocol;
        }

        SSLCopyBufferFromData(charPtr, len, &ctx->ocsp_request_extensions);

        ctx->ocsp_peer_enabled = true;
    }

    return 0;
}

static int
SSLProcessClientHelloExtension_ServerName(tls_handshake_t ctx, uint16_t extenLen, uint8_t *charPtr)
{
    size_t listLen;
    if(extenLen<2) {
        sslErrorLog("SSLProcessClientHelloExtension_ServerName: length decode error 1\n");
        return errSSLProtocol;
    }
    listLen = SSLDecodeSize(charPtr, 2); charPtr+=2; extenLen-=2;

    if(listLen!=extenLen) {
        sslErrorLog("SSLProcessClientHelloExtension_ServerName: length decode error 2\n");
        return errSSLProtocol;
    }

    while(extenLen) {
        uint8_t nameType;
        size_t nameLen;

        if(extenLen<3) {
            sslErrorLog("SSLProcessClientHelloExtension_ServerName: length decode error 3\n");
            return errSSLProtocol;
        }

        nameType = *charPtr++; extenLen--;
        nameLen = SSLDecodeSize(charPtr, 2); charPtr+=2; extenLen-=2;

        if(extenLen<nameLen) {
            sslErrorLog("SSLProcessClientHelloExtension_ServerName: length decode error 4\n");
            return errSSLProtocol;
        }

        if(nameType==0) {
            SSLFreeBuffer(&ctx->peerDomainName);
            SSLAllocBuffer(&ctx->peerDomainName, nameLen+1);
            memcpy(ctx->peerDomainName.data, charPtr, nameLen);
            ctx->peerDomainName.data[nameLen]=0;  // Make it a NULL terminated string
        }
        charPtr+=nameLen; extenLen-=nameLen;
    }

    return 0;
}

#if		SSL_PAC_SERVER_ENABLE
static int
SSLProcessClientHelloExtension_SessionTicket(tls_handshake_t ctx, uint16_t extenLen, uint8_t *charPtr)
{
    SSLFreeBuffer(&ctx->sessionTicket);
    SSLCopyBufferFromData(charPtr, extenLen, &ctx->sessionTicket);
    sslLogNegotiateDebug("Saved %lu bytes of sessionTicket from ClientHello",
                         (unsigned long)extenLen);
    return 0;
}
#endif

static int
SSLProcessClientHelloExtension_SignatureAlgorithms(tls_handshake_t ctx, uint16_t extenLen, uint8_t *charPtr)
{
    UInt8 *cp = charPtr;
#ifndef NDEBUG
    UInt8 *end = charPtr + extenLen;
#endif
    size_t sigAlgsSize;

    if(extenLen<2) {
        sslErrorLog("SSLProcessClientHelloExtension_SignatureAlgorithms: length decode error 1\n");
        return errSSLProtocol;
    }

    sigAlgsSize = SSLDecodeInt(cp, 2);
    cp += 2; extenLen -= 2;

    if ((extenLen != sigAlgsSize) || (extenLen & 1) || (sigAlgsSize & 1)) {
        sslLogNegotiateDebug("SSL_HE_SignatureAlgorithms: odd length of signature algorithms list %lu %lu",
                             (unsigned long)extenLen, (unsigned long)sigAlgsSize);
        return errSSLProtocol;
    }

    ctx->numPeerSigAlgs = (unsigned)(sigAlgsSize / 2);
    sslFree(ctx->peerSigAlgs);
    ctx->peerSigAlgs = (tls_signature_and_hash_algorithm *)
    sslMalloc((ctx->numPeerSigAlgs) * sizeof(tls_signature_and_hash_algorithm));
    for(int i=0; i<ctx->numPeerSigAlgs; i++) {
        /* Just store, will validate when needed */
        ctx->peerSigAlgs[i].hash = *cp++;
        ctx->peerSigAlgs[i].signature = *cp++;
        sslLogNegotiateDebug("===Client specifies sigAlg %d %d",
                             ctx->peerSigAlgs[i].hash,
                             ctx->peerSigAlgs[i].signature);
    }
    assert(cp==end);

    return 0;
}

static int
SSLProcessClientHelloExtension_EllipticCurves(tls_handshake_t ctx, uint16_t extenLen, uint8_t *charPtr)
{
    UInt8 *cp = charPtr;
    if(extenLen<2) {
        sslErrorLog("SSLProcessClientHelloExtension_SignatureAlgorithms: length decode error 1\n");
        return errSSLProtocol;
    }

    size_t suppCurveLen;

    suppCurveLen = SSLDecodeInt(cp, 2);
    cp += 2; extenLen -= 2;

    if (extenLen != suppCurveLen) {
        sslLogNegotiateDebug("SSL_HE_EllipticCurves: extension length & no. of EC curves don't match %lu %lu",
                             (unsigned long)extenLen, (unsigned long)suppCurveLen);
        return errSSLProtocol;
    }
    if (suppCurveLen < 2 || (suppCurveLen % 2) != 0) {
        sslLogNegotiateDebug("SSL_HE_EllipticCurves: invalid extensions length sent by client %lu",
                             (unsigned long)suppCurveLen);
        return errSSLProtocol;
    }

    ctx->num_ec_curves = (unsigned)(suppCurveLen / 2);
    sslFree(ctx->requested_ecdh_curves);
    ctx->requested_ecdh_curves = sslMalloc(ctx->num_ec_curves*sizeof(uint16_t));
    for(int i = 0; i<ctx->num_ec_curves; i++) {
        ctx->requested_ecdh_curves[i] = SSLDecodeInt(cp, 2);
        cp+=2;
    }
    return 0;

}

static int
SSLProcessClientHelloExtension_SecureRenegotiation(tls_handshake_t ctx, uint16_t extenLen, uint8_t *charPtr)
{
    ctx->secure_renegotiation_received = false;

    if(extenLen!= (1 + ctx->peerVerifyData.length))
        return errSSLNegotiation;

    if(*charPtr != ctx->peerVerifyData.length)
        return errSSLNegotiation;
    charPtr++;

    if(memcmp(charPtr, ctx->peerVerifyData.data, ctx->peerVerifyData.length))
        return errSSLNegotiation;

    ctx->secure_renegotiation_received = true;

    return 0;
}

static int
SSLProcessClientHelloExtensions(tls_handshake_t ctx, uint16_t extensionsLen, uint8_t *p)
{
    UInt16 remaining;
    OSStatus err;

    if(extensionsLen<2) {
        sslErrorLog("SSLProcessClientHelloExtensions: need a least 2 bytes\n");
        return errSSLProtocol;
    }

    ctx->ocsp_peer_enabled = false;
    SSLFreeBuffer(&ctx->peerDomainName);
    ctx->extMSReceived = false;

    remaining = SSLDecodeInt(p, 2); p+=2;
    extensionsLen -=2;

    /* remaining = number of bytes remaining to process according to buffer data */
    /* extensionsLen = number of bytes in the buffer */

    if(remaining>extensionsLen) {
        sslErrorLog("SSLProcessClientHelloExtensions: ext len error 1\n");
        return errSSLProtocol;
    }

    if(remaining<extensionsLen) {
        sslErrorLog("Warning: SSLProcessClientHelloExtensions: Too many bytes\n");
    }

    while(remaining) {
        UInt16 extType;
        UInt16 extLen;

        if (remaining<4) {
            sslErrorLog("SSLProcessClientHelloExtensions: ext len error\n");
            return errSSLProtocol;
        }

        extType = SSLDecodeInt(p, 2); p+=2;
        extLen = SSLDecodeInt(p, 2); p+=2;

        if (remaining<(4+extLen)) {
            sslErrorLog("SSLProcessClientHelloExtension: ext len error 2\n");
            return errSSLProtocol;
        }
        remaining -= (4+extLen);

        switch(extType) {
#if		SSL_PAC_SERVER_ENABLE
            case SSL_HE_SessionTicket:
                if((err=SSLProcessClientHelloExtension_SessionTicket(ctx, extLen, p))!=0)
                    return err;
                break;
#endif
            case SSL_HE_ServerName:
                if((err=SSLProcessClientHelloExtension_ServerName(ctx, extLen, p))!=0)
                    return err;
                break;
            case SSL_HE_StatusReguest:
                if((err=SSLProcessClientHelloExtension_StatusRequest(ctx, extLen, p))!=0)
                    return err;
                break;
            case SSL_HE_SignatureAlgorithms:
                if((err=SSLProcessClientHelloExtension_SignatureAlgorithms(ctx, extLen, p))!=0)
                    return err;
                break;
            case SSL_HE_NPN:
                if((err=SSLProcessClientHelloExtension_NPN(ctx, extLen, p))!=0)
                    return err;
                break;
            case SSL_HE_SCT:
                if((err=SSLProcessClientHelloExtension_SCT(ctx, extLen, p))!=0)
                    return err;
                break;
            case SSL_HE_EllipticCurves:
                if ((err = SSLProcessClientHelloExtension_EllipticCurves(ctx, extLen, p)) != 0)
                    return err;
                break;
            case SSL_HE_SecureRenegotation:
                if((err=SSLProcessClientHelloExtension_SecureRenegotiation(ctx, extLen, p))!=0)
                    return err;
                break;
            case SSL_HE_ExtendedMasterSecret:
                ctx->extMSReceived = true;
                break;
            default:
                sslLogNegotiateDebug("SSLProcessClientHelloExtensions: unknown extenType (%lu)",
                                     (unsigned long)extType);
                break;
        }
        p+=extLen;
    }

    return errSSLSuccess;
}


int
SSLProcessClientHello(tls_buffer message, tls_handshake_t ctx)
{   int            err;
    tls_protocol_version  negVersion;
    UInt16              cipherListLen;
    UInt8               sessionIDLen, compressionCount;
    UInt8               *charPtr;
    UInt8				*eom;		/* end of message */

    if (message.length < 41) {
    	sslErrorLog("SSLProcessClientHello: msg len error 1\n");
        return errSSLProtocol;
    }
    charPtr = message.data;
	eom = charPtr + message.length;
    ctx->clientReqProtocol = (tls_protocol_version)SSLDecodeInt(charPtr, 2);
    charPtr += 2;
	err = sslVerifyProtVersion(ctx, ctx->clientReqProtocol, &negVersion);
	if(err) {
        sslErrorLog("SSLProcessClientHello: protocol version error %04x\n", ctx->clientReqProtocol);
		return err;
	}
	switch(negVersion) {
		case tls_protocol_version_SSL_3:
			ctx->sslTslCalls = &Ssl3Callouts;
			break;
		case tls_protocol_version_TLS_1_0:
        case tls_protocol_version_TLS_1_1:
		case tls_protocol_version_DTLS_1_0:
 			ctx->sslTslCalls = &Tls1Callouts;
			break;
        case tls_protocol_version_TLS_1_2:
			ctx->sslTslCalls = &Tls12Callouts;
			break;
		default:
			return errSSLNegotiation;
	}
	ctx->negProtocolVersion = negVersion;
    err = ctx->callbacks->set_protocol_version(ctx->callback_ctx, negVersion);
    if(err) {
        return err;
    }
    sslLogNegotiateDebug("TLS server: negotiated protocol version is %04x", negVersion);

    memcpy(ctx->clientRandom, charPtr, SSL_CLIENT_SRVR_RAND_SIZE);
    charPtr += 32;
    sessionIDLen = *(charPtr++);
    if (message.length < (unsigned)(41 + sessionIDLen)) {
    	sslErrorLog("SSLProcessClientHello: msg len error 2\n");
        return errSSLProtocol;
    }

    SSLFreeBuffer(&ctx->proposedSessionID); //in case of renegotiation.
    if (sessionIDLen > 0) {
        if(SSLCopyBufferFromData(charPtr, sessionIDLen, &ctx->proposedSessionID)) {
            return errSSLAllocate;
        }
    }
    charPtr += sessionIDLen;

#if ENABLE_DTLS
    /* TODO: actually do something with this cookie */
    if(ctx->isDTLS) {
        UInt8 cookieLen = *charPtr++;

        sslLogNegotiateDebug("cookieLen=%d\n", cookieLen);

        if((charPtr + cookieLen)>eom) {
            sslErrorLog("SSLProcessClientHello: msg len error 3\n");
            return errSSLProtocol;
        }

        if((ctx->dtlsCookie.length==0) || ((cookieLen==ctx->dtlsCookie.length) && (memcmp(ctx->dtlsCookie.data, charPtr, cookieLen)==0)))
        {
            ctx->cookieVerified=true;
        } else {
            ctx->cookieVerified=false;
        }

        charPtr+=cookieLen;
    }

    /* TODO: if we are about to send a HelloVerifyRequest, we probably dont need to process the cipherspecs */
#endif

    cipherListLen = (UInt16)SSLDecodeInt(charPtr, 2);
								/* Count of cipherSuites, must be even & >= 2 */
    charPtr += 2;
	if((charPtr + cipherListLen) > eom) {
        sslErrorLog("SSLProcessClientHello: msg len error 4\n");
        return errSSLProtocol;
	}
    if ((cipherListLen & 1) ||
	    (cipherListLen < 2) ||
		(message.length < (unsigned)(39 + sessionIDLen + cipherListLen))) {
        sslErrorLog("SSLProcessClientHello: msg len error 5\n");
        return errSSLProtocol;
    }

    sslFree(ctx->requestedCipherSuites);
    ctx->requestedCipherSuites = sslMalloc(cipherListLen);
    if(ctx->requestedCipherSuites==NULL)
        return errSSLAllocate;

    ctx->empty_renegotation_info_scsv = false;
    ctx->tls_fallback_scsv = false;

    ctx->numRequestedCipherSuites = cipherListLen/2;
    for(int i = 0; i<ctx->numRequestedCipherSuites; i++) {
        ctx->requestedCipherSuites[i] = SSLDecodeInt(charPtr, 2);
        if(ctx->requestedCipherSuites[i] == TLS_EMPTY_RENEGOTIATION_INFO_SCSV) {
            ctx->empty_renegotation_info_scsv = true;
        }
        if(ctx->requestedCipherSuites[i] == TLS_FALLBACK_SCSV) {
            ctx->tls_fallback_scsv = true;
        }
        charPtr+=2;
    }

    compressionCount = *(charPtr++);
    if ((compressionCount < 1) ||
	    (message.length <
		    (unsigned)(38 + sessionIDLen + cipherListLen + compressionCount))) {
        sslErrorLog("SSLProcessClientHello: msg len error 6\n");
        return errSSLProtocol;
    }
    /* Ignore list; we're doing null */

	/* skip compression list */
	charPtr += compressionCount;

    /*
     * Handle ClientHello extensions.
     */

	if(charPtr < eom) {
		ptrdiff_t remLen = eom - charPtr;
        SSLProcessClientHelloExtensions(ctx, remLen, charPtr);
	}

    /* RFC 5746: Secure Renegotiation */
    if(ctx->peerVerifyData.length) {
        // This is a renegotiation.
        if(ctx->secure_renegotiation) { //client supports RFC 5746
            if(ctx->empty_renegotation_info_scsv) {
                return errSSLNegotiation;
            }
            if(!ctx->secure_renegotiation_received) {
                return errSSLNegotiation;
            }
        }
    } else {
        // This is an initial handshake.
        if(ctx->empty_renegotation_info_scsv || ctx->secure_renegotiation_received) {
            ctx->secure_renegotiation = true;
        } else {
            ctx->secure_renegotiation = false;
        }
    }


    if ((err = SSLInitMessageHashes(ctx)) != 0)
        return err;

    return errSSLSuccess;
}


#define SSL2_CLIENT_HELLO_HEADER_SIZE 8
int
SSLProcessSSL2ClientHello(tls_buffer message, tls_handshake_t ctx)
{
    int err;

    uint16_t version;
    uint16_t cipher_spec_length;
    uint16_t session_id_length;
    uint16_t challenge_length;

    //Sanity checks first:

    err = errSSLParam;

    // length must be big enough.
    require(message.length>=SSL2_CLIENT_HELLO_HEADER_SIZE, fail);

    version = SSLDecodeInt(message.data, 2);
    cipher_spec_length = SSLDecodeInt(message.data+2, 2);
    session_id_length = SSLDecodeInt(message.data+4, 2);
    challenge_length = SSLDecodeInt(message.data+6, 2);

    // session_id_length must be 0
    require(session_id_length==0, fail);

    // total lenght must match
    require(message.length==SSL2_CLIENT_HELLO_HEADER_SIZE+cipher_spec_length+challenge_length, fail);

    // cipher_spec_length must be multiple of 3
    require(cipher_spec_length%3==0, fail);

    // require a 32 byte challenge, this is stricter than required, but client SHOULD use 32 bytes,
    // so we'll enforce it, assuming only bogus clients are not doing that already.
    require(challenge_length==SSL_CLIENT_SRVR_RAND_SIZE, fail);

    // Sanity checks done,

    //lets process the version...
    tls_protocol_version negVersion;
    // TODO: refactor with SSLProcessClientHello
    ctx->clientReqProtocol = (tls_protocol_version)version;
    require_noerr((err = sslVerifyProtVersion(ctx, ctx->clientReqProtocol, &negVersion)), fail);

    switch(negVersion) {
        case tls_protocol_version_SSL_3:
            ctx->sslTslCalls = &Ssl3Callouts;
            break;
        case tls_protocol_version_TLS_1_0:
        case tls_protocol_version_TLS_1_1:
        case tls_protocol_version_DTLS_1_0:
            ctx->sslTslCalls = &Tls1Callouts;
            break;
        case tls_protocol_version_TLS_1_2:
            ctx->sslTslCalls = &Tls12Callouts;
            break;
        default:
            return errSSLNegotiation;
    }
    ctx->negProtocolVersion = negVersion;
    require_noerr((err = ctx->callbacks->set_protocol_version(ctx->callback_ctx, negVersion)), fail);

    sslLogNegotiateDebug("negVersion is %d_%d", negVersion >> 8, negVersion & 0xff);

    uint8_t *p = message.data+SSL2_CLIENT_HELLO_HEADER_SIZE;

    // Save the desired ciphersuites

    unsigned cipherCount = cipher_spec_length/3;
    int i, j;
    ctx->numRequestedCipherSuites = 0;
    for(i = 0; i<cipherCount; i++) {
        // SSLv3/TLS ciphersuites start with 0x00.
        if(p[i*3]==0) ctx->numRequestedCipherSuites++;
    }

    sslFree(ctx->requestedCipherSuites);
    ctx->requestedCipherSuites = sslMalloc(ctx->numRequestedCipherSuites*sizeof(uint16_t));
    if(ctx->requestedCipherSuites==NULL)
        return errSSLAllocate;
    j = 0;
    for(i = 0; i<cipherCount; i++) {
        assert(j<ctx->numRequestedCipherSuites);
        if(p[0]==0) ctx->requestedCipherSuites[j++] = SSLDecodeInt(p+1, 2);
        p+=3;
    }

    memcpy(ctx->clientRandom, message.data+SSL2_CLIENT_HELLO_HEADER_SIZE+cipher_spec_length, SSL_CLIENT_SRVR_RAND_SIZE);

    require_noerr((err = SSLInitMessageHashes(ctx)), fail);

    return errSSLSuccess;
fail:
    return err;
}

int
SSLProcessNewSessionTicket(tls_buffer message, tls_handshake_t ctx)
{
    int err = errSSLProtocol;
    uint8_t *p;
    size_t len;

    p = message.data;
    len = message.length;

    require(message.length>=6, fail);

    uint32_t lifetime = SSLDecodeInt(p, 4); p+=4; len-=4;
    uint16_t sessionTicketLen = SSLDecodeInt(p, 2); p+=2; len-=2;

    require(len == sessionTicketLen, fail);

    ctx->sessionTicket_lifetime = lifetime;
    SSLFreeBuffer(&ctx->sessionTicket);
    err = SSLCopyBufferFromData(p, len, &ctx->sessionTicket);

fail:
    return err;
}



static
int sslTime(uint32_t *tim)
{
	time_t t;
	time(&t);
	*tim = (uint32_t)t;
	return errSSLSuccess;
}

int
SSLEncodeRandom(unsigned char *p, tls_handshake_t ctx)
{   tls_buffer   randomData;
    int    err;
    uint32_t    now;

    if ((err = sslTime(&now)) != 0)
        return err;
    SSLEncodeInt(p, now, 4);
    randomData.data = p+4;
    randomData.length = 28;
	if((err = sslRand(&randomData)) != 0)
        return err;
    return errSSLSuccess;
}

int
SSLInitMessageHashes(tls_handshake_t ctx)
{   int          err;

    if ((err = CloseHash(&SSLHashSHA1, &ctx->shaState)) != 0)
        return err;
    if ((err = CloseHash(&SSLHashMD5,  &ctx->md5State)) != 0)
        return err;
    if ((err = CloseHash(&SSLHashSHA256,  &ctx->sha256State)) != 0)
        return err;
    if ((err = CloseHash(&SSLHashSHA384,  &ctx->sha384State)) != 0)
        return err;
    if ((err = CloseHash(&SSLHashSHA512,  &ctx->sha512State)) != 0)
        return err;
    if ((err = ReadyHash(&SSLHashSHA1, &ctx->shaState)) != 0)
        return err;
    if ((err = ReadyHash(&SSLHashMD5,  &ctx->md5State)) != 0)
        return err;
    if ((err = ReadyHash(&SSLHashSHA256,  &ctx->sha256State)) != 0)
        return err;
    if ((err = ReadyHash(&SSLHashSHA384,  &ctx->sha384State)) != 0)
        return err;
    if ((err = ReadyHash(&SSLHashSHA512,  &ctx->sha512State)) != 0)
        return err;
    return errSSLSuccess;
}

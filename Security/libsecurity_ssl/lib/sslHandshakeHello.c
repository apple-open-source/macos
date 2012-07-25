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

#include "sslContext.h"
#include "sslHandshake.h"
#include "sslMemory.h"
#include "sslSession.h"
#include "sslUtils.h"
#include "sslDebug.h"
#include "sslCrypto.h"

#include "sslDigests.h"
#include "cipherSpecs.h"

#include <string.h>

/* IE treats null session id as valid; two consecutive sessions with NULL ID
 * are considered a match. Workaround: when resumable sessions are disabled,
 * send a random session ID. */
#define SSL_IE_NULL_RESUME_BUG		1
#if		SSL_IE_NULL_RESUME_BUG
#define SSL_NULL_ID_LEN				32	/* length of bogus session ID */
#endif

OSStatus
SSLEncodeServerHello(SSLRecord *serverHello, SSLContext *ctx)
{   OSStatus        err;
    UInt8           *charPtr;
    int             sessionIDLen;
    size_t          msglen;
    int             head;

    sessionIDLen = 0;
    if (ctx->sessionID.data != 0)
        sessionIDLen = (UInt8)ctx->sessionID.length;
	#if 	SSL_IE_NULL_RESUME_BUG
	if(sessionIDLen == 0) {
		sessionIDLen = SSL_NULL_ID_LEN;
	}
	#endif	/* SSL_IE_NULL_RESUME_BUG */

    msglen = 38 + sessionIDLen;

	/* this was set to a known quantity in SSLProcessClientHello */
	assert(ctx->negProtocolVersion != SSL_Version_Undetermined);
	/* should not be here in this case */
	assert(ctx->negProtocolVersion != SSL_Version_2_0);
	sslLogNegotiateDebug("===SSL3 server: sending version %d_%d",
		ctx->negProtocolVersion >> 8, ctx->negProtocolVersion & 0xff);
	sslLogNegotiateDebug("...sessionIDLen = %d", sessionIDLen);
    serverHello->protocolVersion = ctx->negProtocolVersion;
    serverHello->contentType = SSL_RecordTypeHandshake;
    head = SSLHandshakeHeaderSize(serverHello);
    if ((err = SSLAllocBuffer(&serverHello->contents, msglen + head, ctx)) != 0)
        return err;

    charPtr = SSLEncodeHandshakeHeader(ctx, serverHello, SSL_HdskServerHello, msglen);

    charPtr = SSLEncodeInt(charPtr, serverHello->protocolVersion, 2);

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
		SSLBuffer rb;
		rb.data = charPtr;
		rb.length = SSL_NULL_ID_LEN;
		sslRand(ctx, &rb);
	}
	#else
    if (sessionIDLen > 0)
        memcpy(charPtr, ctx->sessionID.data, sessionIDLen);
	#endif	/* SSL_IE_NULL_RESUME_BUG */
	charPtr += sessionIDLen;
    charPtr = SSLEncodeInt(charPtr, ctx->selectedCipher, 2);
    *(charPtr++) = 0;      /* Null compression */

    sslLogNegotiateDebug("ssl3: server specifying cipherSuite 0x%lx",
		(UInt32)ctx->selectedCipher);

    assert(charPtr == serverHello->contents.data + serverHello->contents.length);

    return noErr;
}

OSStatus
SSLEncodeServerHelloVerifyRequest(SSLRecord *helloVerifyRequest, SSLContext *ctx)
{   OSStatus        err;
    UInt8           *charPtr;
    size_t          msglen;
    int             head;

    assert(ctx->protocolSide == kSSLServerSide);
    assert(ctx->negProtocolVersion == DTLS_Version_1_0);
    assert(ctx->dtlsCookie.length);

    msglen = 3 + ctx->dtlsCookie.length;

    helloVerifyRequest->protocolVersion = DTLS_Version_1_0;
    helloVerifyRequest->contentType = SSL_RecordTypeHandshake;
    head = SSLHandshakeHeaderSize(helloVerifyRequest);
    if ((err = SSLAllocBuffer(&helloVerifyRequest->contents, msglen + head, ctx)) != 0)
        return err;

    charPtr = SSLEncodeHandshakeHeader(ctx, helloVerifyRequest, SSL_HdskHelloVerifyRequest, msglen);

    charPtr = SSLEncodeInt(charPtr, helloVerifyRequest->protocolVersion, 2);

    *charPtr++ = ctx->dtlsCookie.length;
    memcpy(charPtr, ctx->dtlsCookie.data, ctx->dtlsCookie.length);
    charPtr += ctx->dtlsCookie.length;

    assert(charPtr == (helloVerifyRequest->contents.data + helloVerifyRequest->contents.length));

    return noErr;
}


OSStatus
SSLProcessServerHelloVerifyRequest(SSLBuffer message, SSLContext *ctx)
{   OSStatus            err;
    SSLProtocolVersion  protocolVersion;
    unsigned int        cookieLen;
    UInt8               *p;

    assert(ctx->protocolSide == kSSLClientSide);

    /* TODO: those length values should not be hardcoded */
    /* 3 bytes at least with empty cookie */
    if (message.length < 3 ) {
	sslErrorLog("SSLProcessServerHelloVerifyRequest: msg len error\n");
        return errSSLProtocol;
    }
    p = message.data;

    protocolVersion = (SSLProtocolVersion)SSLDecodeInt(p, 2);
    p += 2;

    /* TODO: Not clear what else to do with protocol version here */
    if(protocolVersion != DTLS_Version_1_0) {
        sslErrorLog("SSLProcessServerHelloVerifyRequest: protocol version error\n");
        return errSSLProtocol;
    }

    cookieLen = *p++;
    sslLogNegotiateDebug("cookieLen = %d, msglen=%d\n", cookieLen, message.length);
    /* TODO: hardcoded '15' again */
    if (message.length < (3 + cookieLen)) {
	sslErrorLog("SSLProcessServerHelloVerifyRequest: msg len error 2\n");
        return errSSLProtocol;
    }

    err = SSLAllocBuffer(&ctx->dtlsCookie, cookieLen, ctx);
    if (err == 0)
        memcpy(ctx->dtlsCookie.data, p, cookieLen);

    return err;
}

static void
SSLProcessServerHelloExtension_SecureRenegotiation(SSLContext *ctx, UInt16 extLen, UInt8 *p)
{
    if(extLen!= (1 + ctx->ownVerifyData.length + ctx->peerVerifyData.length))
        return;

    if(*p!=ctx->ownVerifyData.length + ctx->ownVerifyData.length)
        return;
    p++;

    if(memcmp(p, ctx->ownVerifyData.data, ctx->ownVerifyData.length))
        return;
    p+=ctx->ownVerifyData.length;

    if(memcmp(p, ctx->peerVerifyData.data, ctx->peerVerifyData.length))
        return;

    ctx->secure_renegotiation_received = true;
}


static OSStatus
SSLProcessServerHelloExtensions(SSLContext *ctx, UInt16 extensionsLen, UInt8 *p)
{
    Boolean got_secure_renegotiation = false;
    UInt16 remaining;

    if(extensionsLen<2) {
        sslErrorLog("SSLProcessHelloExtensions: need a least 2 bytes\n");
        return errSSLProtocol;
    }

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

    return noErr;
}

OSStatus
SSLProcessServerHello(SSLBuffer message, SSLContext *ctx)
{   OSStatus            err;
    SSLProtocolVersion  protocolVersion, negVersion;
    size_t              sessionIDLen;
    size_t              extensionsLen;
    UInt8               *p;

    assert(ctx->protocolSide == kSSLClientSide);

    if (message.length < 38) {
    	sslErrorLog("SSLProcessServerHello: msg len error\n");
        return errSSLProtocol;
    }
    p = message.data;

    protocolVersion = (SSLProtocolVersion)SSLDecodeInt(p, 2);
    p += 2;
	/* FIXME this should probably send appropriate alerts */
	err = sslVerifyProtVersion(ctx, protocolVersion, &negVersion);
	if(err) {
		return err;
	}
    ctx->negProtocolVersion = negVersion;
	switch(negVersion) {
		case SSL_Version_3_0:
			ctx->sslTslCalls = &Ssl3Callouts;
			break;
		case TLS_Version_1_0:
        case TLS_Version_1_1:
        case DTLS_Version_1_0:
 			ctx->sslTslCalls = &Tls1Callouts;
			break;
        case TLS_Version_1_2:
			ctx->sslTslCalls = &Tls12Callouts;
			break;
		default:
			return errSSLNegotiation;
	}
    sslLogNegotiateDebug("===SSL3 client: negVersion is %d_%d",
		(negVersion >> 8) & 0xff, negVersion & 0xff);

    memcpy(ctx->serverRandom, p, 32);
    p += 32;

    sessionIDLen = *p++;
    if (message.length < (38 + sessionIDLen)) {
    	sslErrorLog("SSLProcessServerHello: msg len error 2\n");
        return errSSLProtocol;
    }
    if (sessionIDLen > 0 && ctx->peerID.data != 0)
    {   /* Don't die on error; just treat it as an uncached session */
        if (ctx->sessionID.data)
            SSLFreeBuffer(&ctx->sessionID, ctx);
        err = SSLAllocBuffer(&ctx->sessionID, sessionIDLen, ctx);
        if (err == 0)
            memcpy(ctx->sessionID.data, p, sessionIDLen);
    }
    p += sessionIDLen;

    ctx->selectedCipher = (UInt16)SSLDecodeInt(p,2);
    sslLogNegotiateDebug("===ssl3: server requests cipherKind %x",
    	(unsigned)ctx->selectedCipher);
    p += 2;
    if ((err = FindCipherSpec(ctx)) != 0) {
        return err;
    }

    if (*p++ != 0)      /* Compression */
        return unimpErr;

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
    return noErr;
}

OSStatus
SSLEncodeClientHello(SSLRecord *clientHello, SSLContext *ctx)
{
	size_t          length;
    unsigned        i;
    OSStatus        err;
    unsigned char   *p;
    SSLBuffer       sessionIdentifier = { 0, NULL };
    size_t          sessionIDLen;
	size_t			sessionTicketLen = 0;
	size_t			serverNameLen = 0;
	size_t			pointFormatLen = 0;
	size_t			suppCurveLen = 0;
	size_t			signatureAlgorithmsLen = 0;
	size_t			totalExtenLen = 0;
    UInt16          numCipherSuites;
    int             head;

    assert(ctx->protocolSide == kSSLClientSide);

	clientHello->contents.length = 0;
	clientHello->contents.data = NULL;

    sessionIDLen = 0;
    if (ctx->resumableSession.data != 0)
    {   if ((err = SSLRetrieveSessionID(ctx->resumableSession,
				&sessionIdentifier, ctx)) != 0)
        {   return err;
        }
        sessionIDLen = sessionIdentifier.length;
    }

	/*
	 * Since we're not in SSLv2 compatibility mode, only count non-SSLv2 ciphers.
	 */
#if ENABLE_SSLV2
    numCipherSuites = ctx->numValidNonSSLv2Specs;
#else
    numCipherSuites = ctx->numValidCipherSuites;
#endif

    /* RFC 5746 : add the fake ciphersuite unless we are including the extension */
    if(!ctx->secure_renegotiation)
        numCipherSuites+=1;

    length = 39 + 2*numCipherSuites + sessionIDLen;

	err = sslGetMaxProtVersion(ctx, &clientHello->protocolVersion);
	if(err) {
		/* we don't have a protocol enabled */
		goto err_exit;
	}

    /* RFC 5746: If are starting a new handshake, so we didnt received this yet */
    ctx->secure_renegotiation_received = false;

    /* If we already negotiated the protocol version previously,
     we should just use that */
    if(ctx->negProtocolVersion != SSL_Version_Undetermined) {
        clientHello->protocolVersion = ctx->negProtocolVersion;
    }

#if ENABLE_DTLS
    if(clientHello->protocolVersion == DTLS_Version_1_0) {
        /* extra space for cookie */
        /* TODO: cookie len - 0 for now */
        length += 1 + ctx->dtlsCookie.length;
        sslLogNegotiateDebug("==DTLS Hello: len=%lu\n", length);
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
	if((clientHello->protocolVersion >= TLS_Version_1_0) &&
	   (ctx->peerDomainName != NULL) &&
	   (ctx->peerDomainNameLen != 0)) {
		serverNameLen = 2 +	/* extension type */
						2 + /* 2-byte vector length, extension_data */
						2 + /* length of server_name_list */
						1 +	/* length of name_type */
						2 + /* length of HostName */
						ctx->peerDomainNameLen;
		totalExtenLen += serverNameLen;
	}
	if(ctx->sessionTicket.length) {
		sessionTicketLen = 2 +	/* extension type */
						   2 + /* 2-byte vector length, extension_data */
						   ctx->sessionTicket.length;
		totalExtenLen += sessionTicketLen;
	}
	if((clientHello->protocolVersion >= TLS_Version_1_0) &&
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
       ? clientHello->protocolVersion < DTLS_Version_1_0
       : clientHello->protocolVersion >= TLS_Version_1_2) {
        signatureAlgorithmsLen = 2 +	/* extension type */
                                 2 +	/* 2-byte vector length, extension_data */
                                 2 +    /* length of signatureAlgorithms list */
                                 2 * (ctx->ecdsaEnable ? 5 : 3); //FIXME: 5:3 should not be hardcoded here.
		totalExtenLen += signatureAlgorithmsLen;
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

    clientHello->contentType = SSL_RecordTypeHandshake;
    head = SSLHandshakeHeaderSize(clientHello);
    if ((err = SSLAllocBuffer(&clientHello->contents, length + head, ctx)) != 0)
        goto err_exit;

    p = SSLEncodeHandshakeHeader(ctx, clientHello, SSL_HdskClientHello, length);

    p = SSLEncodeInt(p, clientHello->protocolVersion, 2);

	sslLogNegotiateDebug("===SSL3 client: proclaiming max protocol "
		"%d_%d capable ONLY",
		clientHello->protocolVersion >> 8, clientHello->protocolVersion & 0xff);
   if ((err = SSLEncodeRandom(p, ctx)) != 0)
    {   goto err_exit;
    }
    memcpy(ctx->clientRandom, p, SSL_CLIENT_SRVR_RAND_SIZE);
    p += 32;
    *p++ = sessionIDLen;    				/* 1 byte vector length */
    if (sessionIDLen > 0)
    {   memcpy(p, sessionIdentifier.data, sessionIDLen);
    }
    p += sessionIDLen;
#if ENABLE_DTLS
    if (clientHello->protocolVersion == DTLS_Version_1_0) {
        /* TODO: Add the cookie ! Currently: size=0 -> no cookie */
        *p++ = ctx->dtlsCookie.length;
        if(ctx->dtlsCookie.length) {
            memcpy(p, ctx->dtlsCookie.data, ctx->dtlsCookie.length);
            p+=ctx->dtlsCookie.length;
        }
        sslLogNegotiateDebug("==DTLS Hello: cookie len = %d\n",ctx->dtlsCookie.length);
    }
#endif


    p = SSLEncodeInt(p, 2*numCipherSuites, 2);
    /* 2 byte long vector length */

    /* RFC 5746 : add the fake ciphersuite unless we are including the extension */
    if(!ctx->secure_renegotiation)
        p = SSLEncodeInt(p, TLS_EMPTY_RENEGOTIATION_INFO_SCSV, 2);

    for (i = 0; i<ctx->numValidCipherSuites; ++i) {
#if ENABLE_SSLV2
		if(CIPHER_SUITE_IS_SSLv2(ctx->validCipherSuites[i])) {
			continue;
		}
#endif
		sslLogNegotiateDebug("ssl3EncodeClientHello sending suite %x",
					(unsigned)ctx->validCipherSuites[i]);
        p = SSLEncodeInt(p, ctx->validCipherSuites[i], 2);
	}
    *p++ = 1;                               /* 1 byte long vector */
    *p++ = 0;                               /* null compression */

	/*
	 * Append ClientHello extensions.
	 */
	if(totalExtenLen != 0) {
		/* first, total length of all extensions */
		p = SSLEncodeSize(p, totalExtenLen, 2);
	}
    if(ctx->secure_renegotiation){
        assert(ctx->ownVerifyData.length<=255);
        p = SSLEncodeInt(p, SSL_HE_SecureRenegotation, 2);
        p = SSLEncodeSize(p, ctx->ownVerifyData.length+1, 2);
        p = SSLEncodeSize(p, ctx->ownVerifyData.length, 1);
        memcpy(p, ctx->ownVerifyData.data, ctx->ownVerifyData.length);
        p += ctx->ownVerifyData.length;
    }
	if(sessionTicketLen) {
		sslEapDebug("Adding %lu bytes of sessionTicket to ClientHello",
			ctx->sessionTicket.length);
   		p = SSLEncodeInt(p, SSL_HE_SessionTicket, 2);
		p = SSLEncodeSize(p, ctx->sessionTicket.length, 2);
		memcpy(p, ctx->sessionTicket.data, ctx->sessionTicket.length);
		p += ctx->sessionTicket.length;
	}
	if(serverNameLen) {
		sslEapDebug("Specifying ServerNameIndication");
		p = SSLEncodeInt(p, SSL_HE_ServerName, 2);
		p = SSLEncodeSize(p, ctx->peerDomainNameLen + 5, 2);
		p = SSLEncodeSize(p, ctx->peerDomainNameLen + 3, 2);
		p = SSLEncodeInt(p, SSL_NT_HostName, 1);
		p = SSLEncodeSize(p, ctx->peerDomainNameLen, 2);
		memcpy(p, ctx->peerDomainName, ctx->peerDomainNameLen);
		p += ctx->peerDomainNameLen;
	}
	if(suppCurveLen) {
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
	if(pointFormatLen) {
		sslEcdsaDebug("+++ adding point format to ClientHello");
		p = SSLEncodeInt(p, SSL_HE_EC_PointFormats, 2);
		p = SSLEncodeSize(p, 2, 2);		/* length of extension data */
		p = SSLEncodeSize(p, 1, 1);		/* length of ec_point_format_list */
		p = SSLEncodeInt(p, SSL_PointFormatUncompressed, 1);
	}
    if (signatureAlgorithmsLen) {
		sslEcdsaDebug("+++ adding signature algorithms to ClientHello");
        /* TODO: Don't hardcode this */
        /* We dont support SHA512 or SHA224 because we didnot implement the digest abstraction for those
         and we dont keep a running hash for those.
         We dont support SHA384/ECDSA because corecrypto ec does not support it with 256 bits curves */
		UInt32 len = 2 * (ctx->ecdsaEnable ? 5 : 3); //FIXME: 5:3 should not be hardcoded here.
		p = SSLEncodeInt(p, SSL_HE_SignatureAlgorithms, 2);
		p = SSLEncodeSize(p, len+2, 2);		/* length of extension data */
		p = SSLEncodeSize(p, len, 2);		/* length of extension data */
        // p = SSLEncodeInt(p, SSL_HashAlgorithmSHA512, 1);
        // p = SSLEncodeInt(p, SSL_SignatureAlgorithmRSA, 1);
        p = SSLEncodeInt(p, SSL_HashAlgorithmSHA384, 1);
        p = SSLEncodeInt(p, SSL_SignatureAlgorithmRSA, 1);
        p = SSLEncodeInt(p, SSL_HashAlgorithmSHA256, 1);
        p = SSLEncodeInt(p, SSL_SignatureAlgorithmRSA, 1);
        // p = SSLEncodeInt(p, SSL_HashAlgorithmSHA224, 1);
        // p = SSLEncodeInt(p, SSL_SignatureAlgorithmRSA, 1);
        p = SSLEncodeInt(p, SSL_HashAlgorithmSHA1, 1);
        p = SSLEncodeInt(p, SSL_SignatureAlgorithmRSA, 1);
        if (ctx->ecdsaEnable) {
            // p = SSLEncodeInt(p, SSL_HashAlgorithmSHA512, 1);
            // p = SSLEncodeInt(p, SSL_SignatureAlgorithmECDSA, 1);
            // p = SSLEncodeInt(p, SSL_HashAlgorithmSHA384, 1);
            // p = SSLEncodeInt(p, SSL_SignatureAlgorithmECDSA, 1);
            p = SSLEncodeInt(p, SSL_HashAlgorithmSHA256, 1);
            p = SSLEncodeInt(p, SSL_SignatureAlgorithmECDSA, 1);
            // p = SSLEncodeInt(p, SSL_HashAlgorithmSHA224, 1);
            // p = SSLEncodeInt(p, SSL_SignatureAlgorithmECDSA, 1);
            p = SSLEncodeInt(p, SSL_HashAlgorithmSHA1, 1);
            p = SSLEncodeInt(p, SSL_SignatureAlgorithmECDSA, 1);
        }
    }

    sslLogNegotiateDebug("Client Hello : data=%p p=%p len=%08x\n", clientHello->contents.data, p, clientHello->contents.length);

    assert(p == clientHello->contents.data + clientHello->contents.length);

    if ((err = SSLInitMessageHashes(ctx)) != 0)
        goto err_exit;

err_exit:
	if (err != 0) {
		SSLFreeBuffer(&clientHello->contents, ctx);
	}
	SSLFreeBuffer(&sessionIdentifier, ctx);

	return err;
}

OSStatus
SSLProcessClientHello(SSLBuffer message, SSLContext *ctx)
{   OSStatus            err;
    SSLProtocolVersion  negVersion;
    UInt16              cipherListLen, cipherCount, desiredSuite, cipherSuite;
    UInt8               sessionIDLen, compressionCount;
    UInt8               *charPtr;
    unsigned            i;
    UInt8				*eom;		/* end of message */

    if (message.length < 41) {
    	sslErrorLog("SSLProcessClientHello: msg len error 1\n");
        return errSSLProtocol;
    }
    charPtr = message.data;
	eom = charPtr + message.length;
    ctx->clientReqProtocol = (SSLProtocolVersion)SSLDecodeInt(charPtr, 2);
    charPtr += 2;
	err = sslVerifyProtVersion(ctx, ctx->clientReqProtocol, &negVersion);
	if(err) {
        sslErrorLog("SSLProcessClientHello: protocol version error %04x - %04x\n", ctx->clientReqProtocol, negVersion);
		return err;
	}
	switch(negVersion) {
		case SSL_Version_3_0:
			ctx->sslTslCalls = &Ssl3Callouts;
			break;
		case TLS_Version_1_0:
        case TLS_Version_1_1:
		case DTLS_Version_1_0:
 			ctx->sslTslCalls = &Tls1Callouts;
			break;
        case TLS_Version_1_2:
			ctx->sslTslCalls = &Tls12Callouts;
			break;
		default:
			return errSSLNegotiation;
	}
	ctx->negProtocolVersion = negVersion;
    sslLogNegotiateDebug("===SSL3 server: negVersion is %d_%d",
		negVersion >> 8, negVersion & 0xff);

    memcpy(ctx->clientRandom, charPtr, SSL_CLIENT_SRVR_RAND_SIZE);
    charPtr += 32;
    sessionIDLen = *(charPtr++);
    if (message.length < (unsigned)(41 + sessionIDLen)) {
    	sslErrorLog("SSLProcessClientHello: msg len error 2\n");
        return errSSLProtocol;
    }
	/* FIXME peerID is never set on server side.... */
    if (sessionIDLen > 0 && ctx->peerID.data != 0)
    {   /* Don't die on error; just treat it as an uncacheable session */
        err = SSLAllocBuffer(&ctx->sessionID, sessionIDLen, ctx);
        if (err == 0)
            memcpy(ctx->sessionID.data, charPtr, sessionIDLen);
    }
    charPtr += sessionIDLen;

#if ENABLE_DTLS
    /* TODO: actually do something with this cookie */
    if(negVersion==DTLS_Version_1_0) {
        UInt8 cookieLen = *charPtr++;

        sslLogNegotiateDebug("cookieLen=%d\n", cookieLen);

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
    	sslErrorLog("SSLProcessClientHello: msg len error 5\n");
        return errSSLProtocol;
	}
    if ((cipherListLen & 1) ||
	    (cipherListLen < 2) ||
		(message.length < (unsigned)(39 + sessionIDLen + cipherListLen))) {
    	sslErrorLog("SSLProcessClientHello: msg len error 3\n");
        return errSSLProtocol;
    }
    cipherCount = cipherListLen/2;
    cipherSuite = 0xFFFF;        /* No match marker */
    while (cipherSuite == 0xFFFF && cipherCount--)
    {   desiredSuite = (UInt16)SSLDecodeInt(charPtr, 2);
        charPtr += 2;
        for (i = 0; i <ctx->numValidCipherSuites; i++)
        {   if (ctx->validCipherSuites[i] == desiredSuite)
            {   cipherSuite = desiredSuite;
                break;
            }
        }
    }

    if (cipherSuite == 0xFFFF)
        return errSSLNegotiation;
    charPtr += 2 * cipherCount;    /* Advance past unchecked cipherCounts */
    ctx->selectedCipher = cipherSuite;
	/* validate cipher later, after we get possible sessionTicket */

    compressionCount = *(charPtr++);
    if ((compressionCount < 1) ||
	    (message.length <
		    (unsigned)(38 + sessionIDLen + cipherListLen + compressionCount))) {
    	sslErrorLog("SSLProcessClientHello: msg len error 4\n");
        return errSSLProtocol;
    }
    /* Ignore list; we're doing null */

	/*
 	 * Handle ClientHello extensions.
	 */
	/* skip compression list */
	charPtr += compressionCount;
	if(charPtr < eom) {
		ptrdiff_t remLen = eom - charPtr;
		UInt32 totalExtensLen;
		UInt32 extenType;
		UInt32 extenLen;
		if(remLen < 6) {
			/*
			 * Not enough for extension type and length, but not an error...
			 * skip it and proceed.
			 */
			sslEapDebug("SSLProcessClientHello: too small for any extension");
			goto proceed;
		}
		totalExtensLen = SSLDecodeInt(charPtr, 2);
		charPtr += 2;
		if((charPtr + totalExtensLen) > eom) {
			sslEapDebug("SSLProcessClientHello: too small for specified total_extension_length");
			goto proceed;
		}
		while(charPtr < eom) {
			extenType = SSLDecodeInt(charPtr, 2);
			charPtr += 2;
			extenLen = SSLDecodeInt(charPtr, 2);
			charPtr += 2;
			if((charPtr + extenLen) > eom) {
				sslEapDebug("SSLProcessClientHello: too small for specified extension_length");
				break;
			}
			switch(extenType) {
#if		SSL_PAC_SERVER_ENABLE

				case SSL_HE_SessionTicket:
					SSLFreeBuffer(&ctx->sessionTicket, NULL);
					SSLCopyBufferFromData(charPtr, extenLen, &ctx->sessionTicket);
					sslEapDebug("Saved %lu bytes of sessionTicket from ClientHello",
						(unsigned long)extenLen);
					break;
#endif
				case SSL_HE_ServerName:
				{
					/*
					 * This is for debug only (it's disabled for Deployment builds).
					 * Someday, I imagine we'll have a getter in the API to get this info.
					 */
					UInt8 *cp = charPtr;
					UInt32 v = SSLDecodeInt(cp, 2);
					cp += 2;
					sslEapDebug("SSL_HE_ServerName: length of server_name_list %lu",
						(unsigned long)v);
					v = SSLDecodeInt(cp, 1);
					cp++;
					sslEapDebug("SSL_HE_ServerName: name_type %lu", (unsigned long)v);
					v = SSLDecodeInt(cp, 2);
					cp += 2;
					sslEapDebug("SSL_HE_ServerName: length of HostName %lu",
						(unsigned long)v);
					char hostString[v + 1];
					memmove(hostString, cp, v);
					hostString[v] = '\0';
					sslEapDebug("SSL_HE_ServerName: ServerName '%s'", hostString);
					break;
				}
				case SSL_HE_SignatureAlgorithms:
				{
					UInt8 *cp = charPtr, *end = charPtr + extenLen;
                    UInt32 sigAlgsSize = SSLDecodeInt(cp, 2);
					cp += 2;

                    if (extenLen != sigAlgsSize + 2 || extenLen & 1 || sigAlgsSize & 1) {
                        sslEapDebug("SSL_HE_SignatureAlgorithms: odd length of signature algorithms list %lu %lu",
                            (unsigned long)extenLen, (unsigned long)sigAlgsSize);
                        break;
                    }

                    ctx->numClientSigAlgs = sigAlgsSize / 2;
                    if(ctx->clientSigAlgs != NULL) {
                        sslFree(ctx->clientSigAlgs);
                    }
                    ctx->clientSigAlgs = (SSLSignatureAndHashAlgorithm *)
                    sslMalloc((ctx->numClientSigAlgs) * sizeof(SSLSignatureAndHashAlgorithm));
                    for(i=0; i<ctx->numClientSigAlgs; i++) {
                        /* TODO: Validate hash and signature fields. */
                        ctx->clientSigAlgs[i].hash = *cp++;
                        ctx->clientSigAlgs[i].signature = *cp++;
                        sslLogNegotiateDebug("===Client specifies sigAlg %d %d",
                                             ctx->clientSigAlgs[i].hash,
                                             ctx->clientSigAlgs[i].signature);
                    }
                    assert(cp==end);
					break;
                }
				default:
					sslEapDebug("SSLProcessClientHello: unknown extenType (%lu)",
						(unsigned long)extenType);
					break;
			}
			charPtr += extenLen;
		}
	}
proceed:
    if ((err = FindCipherSpec(ctx)) != 0) {
        return err;
    }
    sslLogNegotiateDebug("ssl3 server: selecting cipherKind 0x%x", (unsigned)ctx->selectedCipher);
    if ((err = SSLInitMessageHashes(ctx)) != 0)
        return err;

    return noErr;
}

OSStatus
SSLEncodeRandom(unsigned char *p, SSLContext *ctx)
{   SSLBuffer   randomData;
    OSStatus    err;
    uint32_t    now;

    if ((err = sslTime(&now)) != 0)
        return err;
    SSLEncodeInt(p, now, 4);
    randomData.data = p+4;
    randomData.length = 28;
   	if((err = sslRand(ctx, &randomData)) != 0)
        return err;
    return noErr;
}

OSStatus
SSLInitMessageHashes(SSLContext *ctx)
{   OSStatus          err;

    if ((err = CloseHash(&SSLHashSHA1, &ctx->shaState, ctx)) != 0)
        return err;
    if ((err = CloseHash(&SSLHashMD5,  &ctx->md5State, ctx)) != 0)
        return err;
    if ((err = CloseHash(&SSLHashSHA256,  &ctx->sha256State, ctx)) != 0)
        return err;
    if ((err = CloseHash(&SSLHashSHA384,  &ctx->sha512State, ctx)) != 0)
        return err;
    if ((err = ReadyHash(&SSLHashSHA1, &ctx->shaState, ctx)) != 0)
        return err;
    if ((err = ReadyHash(&SSLHashMD5,  &ctx->md5State, ctx)) != 0)
        return err;
    if ((err = ReadyHash(&SSLHashSHA256,  &ctx->sha256State, ctx)) != 0)
        return err;
    if ((err = ReadyHash(&SSLHashSHA384,  &ctx->sha512State, ctx)) != 0)
        return err;
    return noErr;
}

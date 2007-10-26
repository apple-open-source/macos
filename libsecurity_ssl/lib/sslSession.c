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


#include "ssl.h"
#include "sslContext.h"
#include "sslSession.h"
#include "sslMemory.h"
#include "sslUtils.h"
#include "sslDebug.h"
#include "cipherSpecs.h"
#include "appleSession.h"

#include <assert.h>
#include <string.h>
#include <stddef.h>

typedef struct
{   int                 sessionIDLen;
    UInt8               sessionID[32];
    SSLProtocolVersion  protocolVersion;
    UInt16              cipherSuite;
	UInt16				padding;	/* so remainder is word aligned */
    UInt8               masterSecret[48];
    int                 certCount;
    UInt8               certs[1];   /* Actually, variable length */
} ResumableSession;

/*
 * Cook up a (private) resumable session blob, based on the
 * specified ctx, store it with ctx->peerID as the key. 
 * NOTE: This is contrary to the SSL v3 spec, which claims that
 * servers store resumable sessions using ctx->sessionID as the key.
 * I don' think this is an issue...is it?
 */
OSStatus
SSLAddSessionData(const SSLContext *ctx)
{   OSStatus            err;
    uint32              sessionIDLen;
    SSLBuffer           sessionID;
    ResumableSession    *session;
    int                 certCount;
    SSLCertificate      *cert;
    uint8               *certDest;
    
    /* If we don't know who the peer is, we can't store a session */
    if (ctx->peerID.data == 0)
        return errSSLSessionNotFound;
    
    sessionIDLen = offsetof(ResumableSession, certs);
    cert = ctx->peerCert;
    certCount = 0;
    while (cert)
    {   ++certCount;
        sessionIDLen += 4 + cert->derCert.length;
        cert = cert->next;
    }
    
    if ((err = SSLAllocBuffer(&sessionID, sessionIDLen, ctx)) != 0)
        return err;
    
    session = (ResumableSession*)sessionID.data;
    
    session->sessionIDLen = ctx->sessionID.length;
    memcpy(session->sessionID, ctx->sessionID.data, session->sessionIDLen);
    session->protocolVersion = ctx->negProtocolVersion;
    session->cipherSuite = ctx->selectedCipher;
    memcpy(session->masterSecret, ctx->masterSecret, 48);
    session->certCount = certCount;
    session->padding = 0;
	
    certDest = session->certs;
    cert = ctx->peerCert;
    while (cert)
    {   certDest = SSLEncodeInt(certDest, cert->derCert.length, 4);
        memcpy(certDest, cert->derCert.data, cert->derCert.length);
        certDest += cert->derCert.length;
        cert = cert->next;
    }
    
    err = sslAddSession(ctx->peerID, sessionID, ctx->sessionCacheTimeout);
    SSLFreeBuffer(&sessionID, ctx);
    
    return err;
}

/*
 * Retrieve resumable session data, from key ctx->peerID.
 */
OSStatus
SSLGetSessionData(SSLBuffer *sessionData, const SSLContext *ctx)
{   OSStatus      err;
    
    if (ctx->peerID.data == 0)
        return errSSLSessionNotFound;
    
    sessionData->data = 0;
    
    err = sslGetSession(ctx->peerID, sessionData);
    if (sessionData->data == 0)
        return errSSLSessionNotFound;
    
    return err;
}

OSStatus
SSLDeleteSessionData(const SSLContext *ctx)
{   OSStatus      err;
    
    if (ctx->peerID.data == 0)
        return errSSLSessionNotFound;
    
    err = sslDeleteSession(ctx->peerID);
    return err;
}

/*
 * Given a sessionData blob, obtain the associated sessionID (NOT the key...).
 */
OSStatus
SSLRetrieveSessionID(
		const SSLBuffer sessionData, 
		SSLBuffer *identifier, 
		const SSLContext *ctx)
{   OSStatus            err;
    ResumableSession    *session;
    
    session = (ResumableSession*) sessionData.data;
    if ((err = SSLAllocBuffer(identifier, session->sessionIDLen, ctx)) != 0)
        return err;
    memcpy(identifier->data, session->sessionID, session->sessionIDLen);
    return noErr;
}

/*
 * Obtain the protocol version associated with a specified resumable session blob.
 */
OSStatus
SSLRetrieveSessionProtocolVersion(
		const SSLBuffer sessionData, 
		SSLProtocolVersion *version, 
		const SSLContext *ctx)
{   ResumableSession    *session;
    
    session = (ResumableSession*) sessionData.data;
    *version = session->protocolVersion;
    return noErr;
}

/*
 * Retrieve session state from specified sessionData blob, install into
 * ctx. Presumably, ctx->sessionID and
 * ctx->negProtocolVersion are already init'd (from the above two functions). 
 */
 
/*
 * Netscape Enterprise Server is known to change cipherspecs upon session resumption.
 * For example, connecting to cdnow.com with all ciphersuites enabled results in
 * CipherSuite 4 (SSL_RSA_WITH_RC4_128_MD5) being selected on the first session,
 * and CipherSuite 10 (SSL_RSA_WITH_3DES_EDE_CBC_SHA) being selected on subsequent
 * sessions. This is contrary to the SSL3.0 spec, sesion 7.6.1.3, describing the 
 * Server Hello message. 
 *
 * This anomaly does not occur if only RC4 ciphers are enabled in the Client Hello
 * message. It also does not happen in SSL V2. 
 */
#define ALLOW_CIPHERSPEC_CHANGE		1

OSStatus
SSLInstallSessionFromData(const SSLBuffer sessionData, SSLContext *ctx)
{   OSStatus            err;
    ResumableSession    *session;
    uint8               *storedCertProgress;
    SSLCertificate      *cert, *lastCert;
    int                 certCount;
    uint32              certLen;
    
    session = (ResumableSession*)sessionData.data;
	
	/* 
	 * For SSLv3 and TLSv1, we know that selectedCipher has already been specified in 
	 * SSLProcessServerHello(). An SSLv2 server hello message with a session
	 * ID hit contains no CipherKind field so we set it here.
	 */
	if(ctx->negProtocolVersion == SSL_Version_2_0) {
		if(ctx->protocolSide == SSL_ClientSide) {
			assert(ctx->selectedCipher == 0);
			ctx->selectedCipher = session->cipherSuite;
		}
		else {
			/* 
			 * Else...what if they don't match? Could never happen, right? 
			 * Wouldn't that mean the client is trying to switch ciphers on us?
			 */
			if(ctx->selectedCipher != session->cipherSuite) {
				sslErrorLog("+++SSL2: CipherSpec change from %d to %d on session "
					"resume\n",
				session->cipherSuite, ctx->selectedCipher);
				return errSSLProtocol;
			}
		}
	}
	else {
		assert(ctx->selectedCipher != 0);
		if(ctx->selectedCipher != session->cipherSuite) {
			#if		ALLOW_CIPHERSPEC_CHANGE
			sslErrorLog("+++WARNING: CipherSpec change from %d to %d "
					"on session resume\n",
				session->cipherSuite, ctx->selectedCipher);
			#else
			sslErrorLog("+++SSL: CipherSpec change from %d to %d on session resume\n",
				session->cipherSuite, ctx->selectedCipher);
			return errSSLProtocol;
			#endif
		}
    }
    if ((err = FindCipherSpec(ctx)) != 0) {
        return err;
    }
    memcpy(ctx->masterSecret, session->masterSecret, 48);
    
    lastCert = 0;
    storedCertProgress = session->certs;
    certCount = session->certCount;

    while (certCount--)
    {   
		cert = (SSLCertificate *)sslMalloc(sizeof(SSLCertificate));
		if(cert == NULL) {
			return memFullErr;
		}
        cert->next = 0;
        certLen = SSLDecodeInt(storedCertProgress, 4);
        storedCertProgress += 4;
        if ((err = SSLAllocBuffer(&cert->derCert, certLen, ctx)) != 0)
        {   
			sslFree(cert);
            return err;
        }
        memcpy(cert->derCert.data, storedCertProgress, certLen);
        storedCertProgress += certLen;
        if (lastCert == 0)
            ctx->peerCert = cert;
        else
            lastCert->next = cert;
        lastCert = cert;
    }
    
    return noErr;
}

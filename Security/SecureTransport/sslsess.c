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
    File: sslsess.c

    SSLRef 3.0 Final -- 11/19/96

    Copyright (c)1996 by Netscape Communications Corp.

    By retrieving this software you are bound by the licensing terms
    disclosed in the file "LICENSE.txt". Please read it, and if you don't
    accept the terms, delete this software.

    SSLRef 3.0 was developed by Netscape Communications Corp. of Mountain
    View, California <http://home.netscape.com/> and Consensus Development
    Corporation of Berkeley, California <http://www.consensus.com/>.

    *********************************************************************

    File: sslsess.c    SSL Session DB interface

    This file contains functions which interact with the session database
    to store and restore sessions and retrieve information from packed
    session records.

    ****************************************************************** */

#ifndef _SSL_H_
#include "ssl.h"
#endif

#ifndef _SSLCTX_H_
#include "sslctx.h"
#endif

#ifndef _SSLSESS_H_
#include "sslsess.h"
#endif

#ifndef _SSLALLOC_H_
#include "sslalloc.h"
#endif

#ifndef _SSLUTIL_H_
#include "sslutil.h"
#endif

#ifndef	_SSL_DEBUG_H_
#include "sslDebug.h"
#endif

#ifndef	_CIPHER_SPECS_H_
#include "cipherSpecs.h"
#endif

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
 * NOTE: This is contrary to the SSLRef3 spec, which claims that
 * servers store resumable sessions using ctx->sessionID as the key.
 * I don' think this is an issue...is it?
 */
SSLErr
SSLAddSessionData(const SSLContext *ctx)
{   SSLErr              err;
    uint32              sessionIDLen;
    SSLBuffer           sessionID;
    ResumableSession    *session;
    int                 certCount;
    SSLCertificate      *cert;
    uint8               *certDest;
    
    /* If we don't know who the peer is, we can't store a session */
    if (ctx->peerID.data == 0)
        return SSLSessionNotFoundErr;
    
    sessionIDLen = offsetof(ResumableSession, certs);
    cert = ctx->peerCert;
    certCount = 0;
    while (cert)
    {   ++certCount;
        sessionIDLen += 4 + cert->derCert.length;
        cert = cert->next;
    }
    
    if ((err = SSLAllocBuffer(&sessionID, sessionIDLen, &ctx->sysCtx)) != 0)
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
    
    err = sslAddSession(ctx->peerID, sessionID);
    SSLFreeBuffer(&sessionID, &ctx->sysCtx);
    
    return err;
}

/*
 * Retrieve resumable session data, from key ctx->peerID.
 */
SSLErr
SSLGetSessionData(SSLBuffer *sessionData, const SSLContext *ctx)
{   SSLErr      err;
    
    if (ctx->peerID.data == 0)
        return ERR(SSLSessionNotFoundErr);
    
    sessionData->data = 0;
    
    err = sslGetSession(ctx->peerID, sessionData);
    if (sessionData->data == 0)
        return ERR(SSLSessionNotFoundErr);
    
    return err;
}

SSLErr
SSLDeleteSessionData(const SSLContext *ctx)
{   SSLErr      err;
    
    if (ctx->peerID.data == 0)
        return SSLSessionNotFoundErr;
    
    err = sslDeleteSession(ctx->peerID);
    return err;
}

/*
 * Given a sessionData blob, obtain the associated sessionID (NOT the key...).
 */
SSLErr
SSLRetrieveSessionID(
		const SSLBuffer sessionData, 
		SSLBuffer *identifier, 
		const SSLContext *ctx)
{   SSLErr              err;
    ResumableSession    *session;
    
    session = (ResumableSession*) sessionData.data;
    if ((err = SSLAllocBuffer(identifier, session->sessionIDLen, &ctx->sysCtx)) != 0)
        return err;
    memcpy(identifier->data, session->sessionID, session->sessionIDLen);
    return SSLNoErr;
}

/*
 * Obtain the protocol version associated with a specified resumable session blob.
 */
SSLErr
SSLRetrieveSessionProtocolVersion(
		const SSLBuffer sessionData, 
		SSLProtocolVersion *version, 
		const SSLContext *ctx)
{   ResumableSession    *session;
    
    session = (ResumableSession*) sessionData.data;
    *version = session->protocolVersion;
    return SSLNoErr;
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

SSLErr
SSLInstallSessionFromData(const SSLBuffer sessionData, SSLContext *ctx)
{   SSLErr              err;
    ResumableSession    *session;
    uint8               *storedCertProgress;
    SSLCertificate      *cert, *lastCert;
    int                 certCount;
    uint32              certLen;
    
    session = (ResumableSession*)sessionData.data;
    
    CASSERT(ctx->negProtocolVersion == session->protocolVersion);
	
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
				errorLog2("+++SSL2: CipherSpec change from %d to %d on session "
					"resume\n",
				session->cipherSuite, ctx->selectedCipher);
				return SSLProtocolErr;
			}
		}
	}
	else {
		assert(ctx->selectedCipher != 0);
		if(ctx->selectedCipher != session->cipherSuite) {
			#if		ALLOW_CIPHERSPEC_CHANGE
			dprintf2("+++WARNING: CipherSpec change from %d to %d on session resume\n",
				session->cipherSuite, ctx->selectedCipher);
			#else
			errorLog2("+++SSL: CipherSpec change from %d to %d on session resume\n",
				session->cipherSuite, ctx->selectedCipher);
			return SSLProtocolErr;
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
			return SSLMemoryErr;
		}
        cert->next = 0;
        certLen = SSLDecodeInt(storedCertProgress, 4);
        storedCertProgress += 4;
        if ((err = SSLAllocBuffer(&cert->derCert, certLen, &ctx->sysCtx)) != 0)
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
    
    return SSLNoErr;
}

/*
 * Copyright (c) 2000-2001,2005-2008,2010-2012 Apple Inc. All Rights Reserved.
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

#include "ssl.h"
#include "sslContext.h"
#include "sslSession.h"
#include "sslMemory.h"
#include "sslUtils.h"
#include "sslDebug.h"
#include "sslCipherSpecs.h"
#include "appleSession.h"

#include <assert.h>
#include <string.h>
#include <stddef.h>
#include <Security/SecCertificate.h>
#include <Security/SecCertificatePriv.h>
#include "utilities/SecCFRelease.h"

typedef struct
{   size_t              sessionIDLen;
    UInt8               sessionID[32];
    SSLProtocolVersion  protocolVersion;
    UInt16              cipherSuite;
	UInt16				padding;	/* so remainder is word aligned */
    UInt8               masterSecret[48];
    size_t              certCount;
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
	size_t              sessionIDLen;
	SSLBuffer           sessionID;
	ResumableSession    *session;
	size_t              certCount;
#ifdef USE_SSLCERTIFICATE
	SSLCertificate      *cert;
#else
	CFArrayRef			certChain;
	size_t				ix;
#endif
	uint8_t             *certDest;

	/* If we don't know who the peer is, we can't store a session */
	if (ctx->peerID.data == 0)
		return errSSLSessionNotFound;

	sessionIDLen = offsetof(ResumableSession, certs);
#ifdef USE_SSLCERTIFICATE
	cert = ctx->peerCert;
	certCount = 0;
	while (cert)
	{   ++certCount;
		sessionIDLen += 4 + cert->derCert.length;
		cert = cert->next;
	}
#else
	certChain = ctx->peerCert;
	certCount = certChain ? CFArrayGetCount(certChain) : 0;
	for (ix = 0; ix < certCount; ++ix) {
		SecCertificateRef cert = (SecCertificateRef)CFArrayGetValueAtIndex(certChain, ix);
		#if SSL_DEBUG
		sslDebugLog("SSLAddSessionData: got cert %d of %d\n", (int)ix+1, (int)certCount);
		if (!cert || CFGetTypeID(cert) != SecCertificateGetTypeID()) {
			sslErrorLog("SSLAddSessionData: non-cert in peerCert array!\n");
		}
		#endif
		sessionIDLen += 4 + (size_t)SecCertificateGetLength(cert);
	}
#endif

    if ((err = SSLAllocBuffer(&sessionID, sessionIDLen)))
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

#ifdef USE_SSLCERTIFICATE
	cert = ctx->peerCert;
	while (cert)
	{   certDest = SSLEncodeInt(certDest, cert->derCert.length, 4);
		memcpy(certDest, cert->derCert.data, cert->derCert.length);
		certDest += cert->derCert.length;
		cert = cert->next;
	}
#else
	for (ix = 0; ix < certCount; ++ix) {
		SecCertificateRef certRef = (SecCertificateRef)CFArrayGetValueAtIndex(certChain, ix);
		size_t certLength = (size_t)SecCertificateGetLength(certRef);
		const uint8_t *certBytes = SecCertificateGetBytePtr(certRef);

		#if SSL_DEBUG && !TARGET_OS_IPHONE
		/* print cert name when debugging; leave disabled otherwise */
		CFStringRef certName = NULL;
		OSStatus status = SecCertificateInferLabel(certRef, &certName);
		char buf[1024];
		if (status || !certName || !CFStringGetCString(certName, buf, 1024-1, kCFStringEncodingUTF8)) { buf[0]=0; }
		sslDebugLog("SSLAddSessionData: flattening \"%s\" (%ld bytes)\n", buf, certLength);
		CFReleaseSafe(certName);
		#endif

		if (!certBytes || !certLength) {
			sslErrorLog("SSLAddSessionData: invalid certificate at index %d of %d (length=%ld, data=%p)\n",
					(int)ix, (int)certCount-1, certLength, certBytes);
			err = errSecParam; /* if we have a bad cert, don't add session to cache */
		}
		else {
			certDest = SSLEncodeSize(certDest, certLength, 4);
			memcpy(certDest, certBytes, certLength);
			certDest += certLength;
		}
	}
#endif

    err = sslAddSession(ctx->peerID, sessionID, ctx->sessionCacheTimeout);
    SSLFreeBuffer(&sessionID);

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
    if ((err = SSLAllocBuffer(identifier, session->sessionIDLen)))
        return err;
    memcpy(identifier->data, session->sessionID, session->sessionIDLen);
    return errSecSuccess;
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
    return errSecSuccess;
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
    uint8_t             *storedCertProgress;
#ifdef USE_SSLCERTIFICATE
    SSLCertificate		*cert;
    SSLCertificate      *lastCert = NULL;
#else
    SecCertificateRef   cert;
    CFMutableArrayRef	certChain = NULL;
#endif
    size_t              certCount;
    size_t              certLen;

    session = (ResumableSession*)sessionData.data;

	/*
	 * For SSLv3 and TLSv1, we know that selectedCipher has already been specified in
	 * SSLProcessServerHello(). An SSLv2 server hello message with a session
	 * ID hit contains no CipherKind field so we set it here.
	 */
	if(ctx->negProtocolVersion == SSL_Version_2_0) {
		if(ctx->protocolSide == kSSLClientSide) {
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

    storedCertProgress = session->certs;
    certCount = session->certCount;

    while (certCount--)
    {
#ifdef USE_SSLCERTIFICATE
		cert = (SSLCertificate *)sslMalloc(sizeof(SSLCertificate));
		if(cert == NULL) {
			return errSecAllocate;
		}
        cert->next = 0;
        certLen = SSLDecodeInt(storedCertProgress, 4);
        storedCertProgress += 4;
        if ((err = SSLAllocBuffer(&cert->derCert, certLen)
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
#else
        certLen = SSLDecodeInt(storedCertProgress, 4);
        storedCertProgress += 4;
		cert = SecCertificateCreateWithBytes(NULL, storedCertProgress, certLen);
		#if SSL_DEBUG
		sslDebugLog("SSLInstallSessionFromData: creating cert with bytes=%p len=%lu\n",
			storedCertProgress, certLen);
		if (!cert || CFGetTypeID(cert) != SecCertificateGetTypeID()) {
			sslErrorLog("SSLInstallSessionFromData: SecCertificateCreateWithBytes failed\n");
		}
		#endif
		if(cert == NULL) {
			return errSecAllocate;
		}
        storedCertProgress += certLen;
		/* @@@ This is almost the same code as in sslCert.c: SSLProcessCertificate() */
		if (!certChain) {
			certChain = CFArrayCreateMutable(kCFAllocatorDefault,
				session->certCount, &kCFTypeArrayCallBacks);
            if (!certChain) {
				CFRelease(cert);
				return errSecAllocate;
			}
            if (ctx->peerCert) {
				sslDebugLog("SSLInstallSessionFromData: releasing existing cert chain\n");
				CFRelease(ctx->peerCert);
			}
			ctx->peerCert = certChain;
		}

		CFArrayAppendValue(certChain, cert);
		CFRelease(cert);
#endif
    }

    return errSecSuccess;
}

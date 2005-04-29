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


/*
	File:		sslContext.cpp

	Contains:	SSLContext accessors

	Written by:	Doug Mitchell

	Copyright: (c) 1999 by Apple Computer, Inc., all rights reserved.

*/

#include "ssl.h"
#include "sslContext.h"
#include "sslMemory.h"
#include <CoreServices/../Frameworks/CarbonCore.framework/Headers/MacErrors.h>
#include "sslDigests.h"
#include "sslDebug.h"
#include "appleCdsa.h"
#include "sslKeychain.h"
#include "sslUtils.h"
#include "cipherSpecs.h"
#include "appleSession.h"
#include "sslBER.h"
#include <string.h>
#include <Security/SecCertificate.h>
#include <Security/SecTrust.h>

static void sslFreeDnList(
	SSLContext *ctx)
{
    DNListElem      *dn, *nextDN;
    
    dn = ctx->acceptableDNList;
    while (dn)
    {   
    	SSLFreeBuffer(dn->derDN, ctx);
        nextDN = dn->next;
        sslFree(dn);
        dn = nextDN;
    }
    ctx->acceptableDNList = NULL;
}

static OSStatus sslFreeTrustedRoots(
	SSLContext *ctx)
{
	unsigned i;
	
	assert(ctx != NULL);
	if((ctx->numTrustedCerts == 0) || (ctx->trustedCerts == NULL)) {
		/* they really should both be zero, right? */
		assert((ctx->numTrustedCerts == 0) && (ctx->trustedCerts == NULL));
	}
	else {
		for(i=0; i<ctx->numTrustedCerts; i++) {
			stFreeCssmData(&ctx->trustedCerts[i], CSSM_FALSE);
		}
		sslFree(ctx->trustedCerts);
	}
	ctx->numTrustedCerts = 0;
	ctx->trustedCerts = NULL;
	sslFreeDnList(ctx);
	return noErr;
}

/*
 * Default version enables.
 */
#define DEFAULT_SSL2_ENABLE		true
#define DEFAULT_SSL3_ENABLE		true
#define DEFAULT_TLS1_ENABLE		true

OSStatus
SSLNewContext				(Boolean 			isServer,
							 SSLContextRef 		*contextPtr)	/* RETURNED */
{
	SSLContext 	*ctx;
	OSStatus	serr;
		
	if(contextPtr == NULL) {
		return paramErr;
	}
	*contextPtr = NULL;
	ctx = (SSLContext *)sslMalloc(sizeof(SSLContext));
	if(ctx == NULL) {
		return memFullErr;
	}
	/* subsequent errors to errOut: */
	
    memset(ctx, 0, sizeof(SSLContext));
    ctx->state = SSL_HdskStateUninit;
    ctx->clientCertState = kSSLClientCertNone;
	
	ctx->versionSsl2Enable = DEFAULT_SSL2_ENABLE;
	ctx->versionSsl3Enable = DEFAULT_SSL3_ENABLE;
	ctx->versionTls1Enable = DEFAULT_TLS1_ENABLE;
	ctx->negProtocolVersion = SSL_Version_Undetermined;

    if(isServer) {
    	ctx->protocolSide = SSL_ServerSide;
    }
    else {
    	ctx->protocolSide = SSL_ClientSide;
    }

	/* Default value so we can send and receive hello msgs */
	ctx->sslTslCalls = &Ssl3Callouts;
	
    /* Initialize the cipher state to NULL_WITH_NULL_NULL */
    ctx->selectedCipherSpec    = &SSL_NULL_WITH_NULL_NULL_CipherSpec;
    ctx->selectedCipher        = ctx->selectedCipherSpec->cipherSpec;
    ctx->writeCipher.macRef    = ctx->selectedCipherSpec->macAlgorithm;
    ctx->readCipher.macRef     = ctx->selectedCipherSpec->macAlgorithm;
    ctx->readCipher.symCipher  = ctx->selectedCipherSpec->cipher;
    ctx->writeCipher.symCipher = ctx->selectedCipherSpec->cipher;
	
	/* these two are invariant */
    ctx->writeCipher.encrypting = 1;
    ctx->writePending.encrypting = 1;
	
    /* this gets init'd on first call to SSLHandshake() */
    ctx->validCipherSpecs = NULL;
    ctx->numValidCipherSpecs = 0;
    
	ctx->peerDomainName = NULL;
	ctx->peerDomainNameLen = 0;

	/* attach to CSP, CL, TP */
	serr = attachToAll(ctx);
	if(serr) {
		goto errOut;
	}
	
	/* Initial cert verify state: verify with default system roots */
	ctx->enableCertVerify = true;
	
	/* Default for RSA blinding is ENABLED */
	ctx->rsaBlindingEnable = true;
	
    *contextPtr = ctx;
    return noErr;
    
errOut:
	sslFree(ctx);
	return serr;
}


/*
 * Dispose of an SSLContext.
 */
OSStatus
SSLDisposeContext				(SSLContext			*ctx)
{   
	WaitingRecord   *wait, *next;
    SSLBuffer       buf;
    
    if(ctx == NULL) {
    	return paramErr;
    }
    sslDeleteCertificateChain(ctx->localCert, ctx);
    sslDeleteCertificateChain(ctx->encryptCert, ctx);
    sslDeleteCertificateChain(ctx->peerCert, ctx);
    ctx->localCert = ctx->encryptCert = ctx->peerCert = NULL;
    SSLFreeBuffer(ctx->partialReadBuffer, ctx);
	if(ctx->peerSecTrust) {
		CFRelease(ctx->peerSecTrust);
		ctx->peerSecTrust = NULL;
	}
    wait = ctx->recordWriteQueue;
    while (wait)
    {   SSLFreeBuffer(wait->data, ctx);
        next = wait->next;
        buf.data = (uint8*)wait;
        buf.length = sizeof(WaitingRecord);
        SSLFreeBuffer(buf, ctx);
        wait = next;
    }
    
	#if APPLE_DH
    SSLFreeBuffer(ctx->dhParamsPrime, ctx);
    SSLFreeBuffer(ctx->dhParamsGenerator, ctx);
    SSLFreeBuffer(ctx->dhParamsEncoded, ctx);
    SSLFreeBuffer(ctx->dhPeerPublic, ctx);
    SSLFreeBuffer(ctx->dhExchangePublic, ctx);
	sslFreeKey(ctx->cspHand, &ctx->dhPrivate, NULL);
    #endif	/* APPLE_DH */
	
	CloseHash(SSLHashSHA1, ctx->shaState, ctx);
	CloseHash(SSLHashMD5,  ctx->md5State, ctx);
    
    SSLFreeBuffer(ctx->sessionID, ctx);
    SSLFreeBuffer(ctx->peerID, ctx);
    SSLFreeBuffer(ctx->resumableSession, ctx);
    SSLFreeBuffer(ctx->preMasterSecret, ctx);
    SSLFreeBuffer(ctx->partialReadBuffer, ctx);
    SSLFreeBuffer(ctx->fragmentedMessageCache, ctx);
    SSLFreeBuffer(ctx->receivedDataBuffer, ctx);

	if(ctx->peerDomainName) {
		sslFree(ctx->peerDomainName);
		ctx->peerDomainName = NULL;
		ctx->peerDomainNameLen = 0;
	}
    SSLDisposeCipherSuite(&ctx->readCipher, ctx);
    SSLDisposeCipherSuite(&ctx->writeCipher, ctx);
    SSLDisposeCipherSuite(&ctx->readPending, ctx);
    SSLDisposeCipherSuite(&ctx->writePending, ctx);

	sslFree(ctx->validCipherSpecs);
	ctx->validCipherSpecs = NULL;
	ctx->numValidCipherSpecs = 0;
	
	/*
	 * NOTE: currently, all public keys come from the CL via CSSM_CL_CertGetKeyInfo.
	 * We really don't know what CSP the CL used to generate a public key (in fact,
	 * it uses the raw CSP only to get LogicalKeySizeInBits, but we can't know
	 * that). Thus using e.g. signingKeyCsp (or any other CSP) to free 
	 * signingPubKey is not tecnically accurate. However, our public keys 
	 * are all raw keys, and all Apple CSPs dispose of raw keys in the same
	 * way.
	 */
	sslFreeKey(ctx->cspHand, &ctx->signingPubKey, NULL);
	sslFreeKey(ctx->cspHand, &ctx->encryptPubKey, NULL);
	sslFreeKey(ctx->peerPubKeyCsp, &ctx->peerPubKey, NULL);
	
	if(ctx->signingPrivKeyRef) {
		CFRelease(ctx->signingPrivKeyRef);
	}
	if(ctx->encryptPrivKeyRef) {
		CFRelease(ctx->encryptPrivKeyRef);
	}
	sslFreeTrustedRoots(ctx);
	
	detachFromAll(ctx);
	    
	if(ctx->localCertArray) {
		CFRelease(ctx->localCertArray);
	}
	if(ctx->encryptCertArray) {
		CFRelease(ctx->encryptCertArray);
	}

    memset(ctx, 0, sizeof(SSLContext));
    sslFree(ctx);
	sslCleanupSession();
	return noErr;
}

/*
 * Determine the state of an SSL session.
 */
OSStatus 
SSLGetSessionState			(SSLContextRef		context,
							 SSLSessionState	*state)		/* RETURNED */
{
	SSLSessionState rtnState = kSSLIdle;
	
	if(context == NULL) {
		return paramErr;
	}
	*state = rtnState;
	switch(context->state) {
		case SSL_HdskStateUninit:
		case SSL_HdskStateServerUninit:
		case SSL_HdskStateClientUninit:
			rtnState = kSSLIdle;
			break;
		case SSL_HdskStateGracefulClose:
			rtnState = kSSLClosed;
			break;
		case SSL_HdskStateErrorClose:
		case SSL_HdskStateNoNotifyClose:
			rtnState = kSSLAborted;
			break;
		case SSL_HdskStateServerReady:
		case SSL_HdskStateClientReady:
			rtnState = kSSLConnected;
			break;
		default:
			assert((context->state >= SSL_HdskStateServerHello) &&
			        (context->state <= SSL2_HdskStateServerFinished));
			rtnState = kSSLHandshake;
			break;
			
	}
	*state = rtnState;
	return noErr;
}

OSStatus 
SSLSetIOFuncs				(SSLContextRef		ctx, 
							 SSLReadFunc 		read,
							 SSLWriteFunc		write)
{
	if(ctx == NULL) {
		return paramErr;
	}
	if(sslIsSessionActive(ctx)) {
		/* can't do this with an active session */
		return badReqErr;
	}
	ctx->ioCtx.read = read;
	ctx->ioCtx.write = write;
	return noErr;
}

OSStatus
SSLSetConnection			(SSLContextRef		ctx,
							 SSLConnectionRef	connection)
{
	if(ctx == NULL) {
		return paramErr;
	}
	if(sslIsSessionActive(ctx)) {
		/* can't do this with an active session */
		return badReqErr;
	}
	ctx->ioCtx.ioRef = connection;
    return noErr;
}

OSStatus
SSLGetConnection			(SSLContextRef		ctx,
							 SSLConnectionRef	*connection)
{
	if((ctx == NULL) || (connection == NULL)) {
		return paramErr;
	}
	*connection = ctx->ioCtx.ioRef;
	return noErr;
}

OSStatus
SSLSetPeerDomainName		(SSLContextRef		ctx,
							 const char			*peerName,
							 size_t				peerNameLen)
{
	if(ctx == NULL) {
		return paramErr;
	}
	if(sslIsSessionActive(ctx)) {
		/* can't do this with an active session */
		return badReqErr;
	}
	
	/* free possible existing name */
	if(ctx->peerDomainName) {
		sslFree(ctx->peerDomainName);
	}
	
	/* copy in */
	ctx->peerDomainName = (char *)sslMalloc(peerNameLen);
	if(ctx->peerDomainName == NULL) {
		return memFullErr;
	}
	memmove(ctx->peerDomainName, peerName, peerNameLen);
	ctx->peerDomainNameLen = peerNameLen;
	return noErr;
}
		
/*
 * Determine the buffer size needed for SSLGetPeerDomainName().
 */
OSStatus 
SSLGetPeerDomainNameLength	(SSLContextRef		ctx,
							 size_t				*peerNameLen)	// RETURNED
{
	if(ctx == NULL) {
		return paramErr;
	}
	*peerNameLen = ctx->peerDomainNameLen;
	return noErr;
}

OSStatus 
SSLGetPeerDomainName		(SSLContextRef		ctx,
							 char				*peerName,		// returned here
							 size_t				*peerNameLen)	// IN/OUT
{
	if(ctx == NULL) {
		return paramErr;
	}
	if(*peerNameLen < ctx->peerDomainNameLen) {
		return errSSLBufferOverflow;
	}
	memmove(peerName, ctx->peerDomainName, ctx->peerDomainNameLen);
	*peerNameLen = ctx->peerDomainNameLen;
	return noErr;
}

/* concert between private SSLProtocolVersion and public SSLProtocol */
static SSLProtocol convertProtToExtern(SSLProtocolVersion prot)
{
	switch(prot) {
		case SSL_Version_Undetermined:
			return kSSLProtocolUnknown;
		case SSL_Version_2_0:
			return kSSLProtocol2;
		case SSL_Version_3_0:
			return kSSLProtocol3;
		case TLS_Version_1_0:
			return kTLSProtocol1;
		default:
			sslErrorLog("convertProtToExtern: bad prot\n");
			return kSSLProtocolUnknown;
	}
	/* not reached but make compiler happy */
	return kSSLProtocolUnknown;
}

OSStatus 
SSLSetProtocolVersionEnabled(SSLContextRef 		ctx,
							 SSLProtocol		protocol,
							 Boolean			enable)		/* RETURNED */
{
	if(ctx == NULL) {
		return paramErr;
	}
	if(sslIsSessionActive(ctx)) {
		/* can't do this with an active session */
		return badReqErr;
	}
	switch(protocol) {
		case kSSLProtocol2:
			ctx->versionSsl2Enable = enable;
			break;
		case kSSLProtocol3:
			ctx->versionSsl3Enable = enable;
			break;
		case kTLSProtocol1:
			ctx->versionTls1Enable = enable;
			break;
		case kSSLProtocolAll:
			ctx->versionTls1Enable = ctx->versionSsl3Enable = 
				ctx->versionSsl2Enable = enable;
			break;
		default:
			return paramErr;
	}
	return noErr;
}
							 
OSStatus 
SSLGetProtocolVersionEnabled(SSLContextRef 		ctx,
							 SSLProtocol		protocol,
							 Boolean			*enable)		/* RETURNED */
{
	if(ctx == NULL) {
		return paramErr;
	}
	switch(protocol) {
		case kSSLProtocol2:
			*enable = ctx->versionSsl2Enable;
			break;
		case kSSLProtocol3:
			*enable = ctx->versionSsl3Enable;
			break;
		case kTLSProtocol1:
			*enable = ctx->versionTls1Enable;
			break;
		case kSSLProtocolAll:
			if(ctx->versionTls1Enable && ctx->versionSsl3Enable &&
					ctx->versionSsl2Enable) {
				*enable = true;
			}
			else {
				*enable = false;
			}
			break;
		default:
			return paramErr;
	}
	return noErr;
}

/* deprecated */
OSStatus 
SSLSetProtocolVersion		(SSLContextRef 		ctx,
							 SSLProtocol		version)
{   
	if(ctx == NULL) {
		return paramErr;
	}
	if(sslIsSessionActive(ctx)) {
		/* can't do this with an active session */
		return badReqErr;
	}

	/* convert external representation to three booleans */
	switch(version) {
		case kSSLProtocolUnknown:
			ctx->versionSsl2Enable = DEFAULT_SSL2_ENABLE;
			ctx->versionSsl3Enable = DEFAULT_SSL3_ENABLE;
			ctx->versionTls1Enable = DEFAULT_TLS1_ENABLE;
			break;
		case kSSLProtocol2:
			ctx->versionSsl2Enable = true;
			ctx->versionSsl3Enable = false;
			ctx->versionTls1Enable = false;
			break;
		case kSSLProtocol3:
			/* this tells us to do our best, up to 3.0, but allows 2.0 */
			ctx->versionSsl2Enable = true;
			ctx->versionSsl3Enable = true;
			ctx->versionTls1Enable = false;
			break;
		case kSSLProtocol3Only:
			ctx->versionSsl2Enable = false;
			ctx->versionSsl3Enable = true;
			ctx->versionTls1Enable = false;
			break;
		case kTLSProtocol1:
		case kSSLProtocolAll:
			/* this tells us to do our best, up to TLS, but allows 2.0 or 3.0 */
			ctx->versionSsl2Enable = true;
			ctx->versionSsl3Enable = true;
			ctx->versionTls1Enable = true;
			break;
		case kTLSProtocol1Only:
			ctx->versionSsl2Enable = false;
			ctx->versionSsl3Enable = false;
			ctx->versionTls1Enable = true;
			break;
		default:
			return paramErr;
	}
    return noErr;
}

/* deprecated */
OSStatus 
SSLGetProtocolVersion		(SSLContextRef		ctx,
							 SSLProtocol		*protocol)		/* RETURNED */
{
	if(ctx == NULL) {
		return paramErr;
	}
	
	/* translate array of booleans to public value; not all combinations
	 * are legal (i.e., meaningful) for this call */
	if(ctx->versionTls1Enable) {
		if(ctx->versionSsl2Enable) {
			if(ctx->versionSsl3Enable) {
				/* traditional 'all enabled' */
				*protocol = kTLSProtocol1;
				return noErr;
			}
			else {
				/* SSL2 true, SSL3 false, TLS1 true - invalid here */
				return paramErr;
			}
		}
		else if(ctx->versionSsl3Enable) {
			/* SSL2 false, SSL3 true, TLS1 true - invalid here */
			return paramErr;
		}
		else {
			*protocol = kTLSProtocol1Only;
			return noErr;
		}
	}
	else {
		/* TLS1 false */
		if(ctx->versionSsl3Enable) {
			*protocol = ctx->versionSsl2Enable ?
				kSSLProtocol3 : kSSLProtocol3Only;
			return noErr;
		}
		else if(ctx->versionSsl2Enable) {
			*protocol = kSSLProtocol2;
			return noErr;
		}
		else {
			/*
			 * Bogus state - no enables - the API does provide a way
			 * to get into this state. Other than this path, the app
			 * will discover this bogon when attempting to do the 
			 * handshake; sslGetMaxProtVersion will detect this.
			 */
			return paramErr;
		}
	}
	/* NOT REACHED */
}

OSStatus 
SSLGetNegotiatedProtocolVersion		(SSLContextRef		ctx,
									 SSLProtocol		*protocol) /* RETURNED */
{
	if(ctx == NULL) {
		return paramErr;
	}
	*protocol = convertProtToExtern(ctx->negProtocolVersion);
	return noErr;
}

OSStatus
SSLSetEnableCertVerify		(SSLContextRef		ctx,
							 Boolean			enableVerify)
{
	if(ctx == NULL) {
		return paramErr;
	}
	sslCertDebug("SSLSetEnableCertVerify %s",
		enableVerify ? "true" : "false");
	if(sslIsSessionActive(ctx)) {
		/* can't do this with an active session */
		return badReqErr;
	}
	ctx->enableCertVerify = enableVerify;
	return noErr;
}

OSStatus 
SSLGetEnableCertVerify		(SSLContextRef		ctx,
							Boolean				*enableVerify)
{
	if(ctx == NULL) {
		return paramErr;
	}
	*enableVerify = ctx->enableCertVerify;
	return noErr;
}

OSStatus 
SSLSetAllowsExpiredCerts(SSLContextRef		ctx,
						 Boolean			allowExpired)
{
	if(ctx == NULL) {
		return paramErr;
	}
	sslCertDebug("SSLSetAllowsExpiredCerts %s",
		allowExpired ? "true" : "false");
	if(sslIsSessionActive(ctx)) {
		/* can't do this with an active session */
		return badReqErr;
	}
	ctx->allowExpiredCerts = allowExpired;
	return noErr;
}

OSStatus
SSLGetAllowsExpiredCerts	(SSLContextRef		ctx,
							 Boolean			*allowExpired)
{
	if(ctx == NULL) {
		return paramErr;
	}
	*allowExpired = ctx->allowExpiredCerts;
	return noErr;
}

OSStatus 
SSLSetAllowsExpiredRoots(SSLContextRef		ctx,
						 Boolean			allowExpired)
{
	if(ctx == NULL) {
		return paramErr;
	}
	sslCertDebug("SSLSetAllowsExpiredRoots %s",
		allowExpired ? "true" : "false");
	if(sslIsSessionActive(ctx)) {
		/* can't do this with an active session */
		return badReqErr;
	}
	ctx->allowExpiredRoots = allowExpired;
	return noErr;
}

OSStatus
SSLGetAllowsExpiredRoots	(SSLContextRef		ctx,
							 Boolean			*allowExpired)
{
	if(ctx == NULL) {
		return paramErr;
	}
	*allowExpired = ctx->allowExpiredRoots;
	return noErr;
}

OSStatus SSLSetAllowsAnyRoot(
	SSLContextRef	ctx,
	Boolean			anyRoot)
{
	if(ctx == NULL) {
		return paramErr;
	}
	sslCertDebug("SSLSetAllowsAnyRoot %s",	anyRoot ? "true" : "false");
	ctx->allowAnyRoot = anyRoot;
	return noErr;
}

OSStatus
SSLGetAllowsAnyRoot(
	SSLContextRef	ctx,
	Boolean			*anyRoot)
{
	if(ctx == NULL) {
		return paramErr;
	}
	*anyRoot = ctx->allowAnyRoot;
	return noErr;
}

OSStatus 
SSLSetTrustedRoots			(SSLContextRef 		ctx,
							 CFArrayRef 		trustedRoots,
							 Boolean 			replaceExisting)
{
	unsigned 			dex;
	unsigned			outDex;
	unsigned 			numIncoming;
	uint32 				numCerts;
	CSSM_DATA_PTR		newRoots = NULL;
	const CSSM_DATA 	*existAnchors = NULL;
	uint32 				numExistAnchors = 0;
	OSStatus			ortn = noErr;
	
	if(ctx == NULL) {
		return paramErr;
	}
	if(sslIsSessionActive(ctx)) {
		/* can't do this with an active session */
		return badReqErr;
	}
	numCerts = numIncoming = CFArrayGetCount(trustedRoots);
	sslCertDebug("SSLSetTrustedRoot  numCerts %d  replaceExist %s",
		(int)numCerts, replaceExisting ? "true" : "false");
	if(!replaceExisting) {
		if(ctx->trustedCerts != NULL) {
			/* adding to existing store */
			existAnchors = ctx->trustedCerts;
			numExistAnchors = ctx->numTrustedCerts;
		}
		else {
			/* adding to system roots */
			ortn = SecTrustGetCSSMAnchorCertificates(&existAnchors,
				&numExistAnchors);
			if(ortn) {
				/* should never happen */
				return ortn;
			}
		}
		numCerts += numExistAnchors;
	}
	newRoots = (CSSM_DATA_PTR)sslMalloc(numCerts * sizeof(CSSM_DATA));
	memset(newRoots, 0, numCerts * sizeof(CSSM_DATA));
	
	/* Caller's certs first */
	for(dex=0, outDex=0; dex<numIncoming; dex++, outDex++) {
		CSSM_DATA certData;
		SecCertificateRef secCert = (SecCertificateRef)
			CFArrayGetValueAtIndex(trustedRoots, dex);
		
		if(CFGetTypeID(secCert) != SecCertificateGetTypeID()) {
			/* elements of trustedRoots must be SecCertificateRefs */
			ortn = paramErr;
			goto abort;
		}
		ortn = SecCertificateGetData(secCert, &certData);
		if(ortn) {
			goto abort;
		}
		stSetUpCssmData(&newRoots[outDex], certData.Length);
		memmove(newRoots[outDex].Data, certData.Data, certData.Length);
	}
	
	/* now existing roots - either ours, or the system's */
	for(dex=0; dex<numExistAnchors; dex++, outDex++) {
		stSetUpCssmData(&newRoots[outDex], existAnchors[dex].Length);
		memmove(newRoots[outDex].Data, existAnchors[dex].Data, 
			existAnchors[dex].Length);
	}
	
	/* success - replace context values */
	sslFreeTrustedRoots(ctx);
	ctx->numTrustedCerts = numCerts;
	ctx->trustedCerts = newRoots;
	return noErr;
	
abort:
	sslFree(newRoots);
	return ortn;
}

OSStatus 
SSLGetTrustedRoots			(SSLContextRef 		ctx,
							 CFArrayRef 		*trustedRoots)	/* RETURNED */
{
	uint32	 			numCerts;
	const CSSM_DATA 	*certs;
	CFMutableArrayRef	certArray;
	unsigned			dex;
	SecCertificateRef	secCert;
	OSStatus 			ortn;
	
	if(ctx == NULL) {
		return paramErr;
	}
	if(ctx->trustedCerts != NULL) {
		/* use ours */
		certs = ctx->trustedCerts;
		numCerts = ctx->numTrustedCerts;
	}
	else {
		/* use default system roots */
		OSStatus ortn = SecTrustGetCSSMAnchorCertificates(&certs,
			&numCerts);
		if(ortn) {
			/* should never happen */
			return ortn;
		}
	}
	
	certArray = CFArrayCreateMutable(kCFAllocatorDefault,
		(CFIndex)numCerts, &kCFTypeArrayCallBacks);
	if(certArray == NULL) {
		return memFullErr;	
	}
	for(dex=0; dex<numCerts; dex++) {
		ortn = SecCertificateCreateFromData(&certs[dex],
			CSSM_CERT_X_509v3,
			CSSM_CERT_ENCODING_DER,
			&secCert);
		if(ortn) {
			CFRelease(certArray);
			return ortn;
		}
		CFArrayAppendValue(certArray, secCert);
	}
	*trustedRoots = certArray;
	return noErr;
}

OSStatus
SSLSetClientSideAuthenticate 	(SSLContext			*ctx,
								 SSLAuthenticate	auth)
{
	if(ctx == NULL) {
		return paramErr;
	}
	if(sslIsSessionActive(ctx)) {
		/* can't do this with an active session */
		return badReqErr;
	}
	ctx->clientAuth = auth;
	switch(auth) {
		case kNeverAuthenticate:
			ctx->tryClientAuth = false;
			break;
		case kAlwaysAuthenticate:
		case kTryAuthenticate:
			ctx->tryClientAuth = true;
			break;
	}
	return noErr;
}

OSStatus 
SSLGetClientCertificateState	(SSLContextRef				ctx,
								 SSLClientCertificateState	*clientState)
{
	if(ctx == NULL) {
		return paramErr;
	}
	*clientState = ctx->clientCertState;
	return noErr;
}

OSStatus
SSLSetCertificate			(SSLContextRef		ctx,
							 CFArrayRef			certRefs)
{
	/*
	 * -- free localCerts if we have any
	 * -- Get raw cert data, convert to ctx->localCert
	 * -- get pub, priv keys from certRef[0]
	 * -- validate cert chain
	 */
	if(ctx == NULL) {
		return paramErr;
	}
	if(sslIsSessionActive(ctx)) {
		/* can't do this with an active session */
		return badReqErr;
	}
	if(ctx->localCertArray) {
		CFRelease(ctx->localCertArray);
		ctx->localCertArray = NULL;
	}
	OSStatus ortn = parseIncomingCerts(ctx,
		certRefs,
		&ctx->localCert,
		&ctx->signingPubKey,
		&ctx->signingPrivKeyRef);
	if(ortn == noErr) {
		ctx->localCertArray = certRefs;
		CFRetain(certRefs);
	}
	return ortn;
}

OSStatus
SSLSetEncryptionCertificate	(SSLContextRef		ctx,
							 CFArrayRef			certRefs)
{
	/*
	 * -- free encryptCert if we have any
	 * -- Get raw cert data, convert to ctx->encryptCert
	 * -- get pub, priv keys from certRef[0]
	 * -- validate cert chain
	 */
	if(ctx == NULL) {
		return paramErr;
	}
	if(sslIsSessionActive(ctx)) {
		/* can't do this with an active session */
		return badReqErr;
	}
	if(ctx->encryptCertArray) {
		CFRelease(ctx->encryptCertArray);
		ctx->encryptCertArray = NULL;
	}
	OSStatus ortn = parseIncomingCerts(ctx,
		certRefs,
		&ctx->encryptCert,
		&ctx->encryptPubKey,
		&ctx->encryptPrivKeyRef);
	if(ortn == noErr) {
		ctx->encryptCertArray = certRefs;
		CFRetain(certRefs);
	}
	return ortn;
}

OSStatus SSLGetCertificate(SSLContextRef		ctx,
						   CFArrayRef			*certRefs)
{
	if(ctx == NULL) {
		return paramErr;
	}
	*certRefs = ctx->localCertArray;
	return noErr;
}

OSStatus SSLGetEncryptionCertificate(SSLContextRef		ctx,
								     CFArrayRef			*certRefs)
{
	if(ctx == NULL) {
		return paramErr;
	}
	*certRefs = ctx->encryptCertArray;
	return noErr;
}

OSStatus 
SSLSetPeerID				(SSLContext 		*ctx, 
							 const void 		*peerID,
							 size_t				peerIDLen)
{
	OSStatus serr;
	
	/* copy peerId to context->peerId */
	if((ctx == NULL) || 
	   (peerID == NULL) ||
	   (peerIDLen == 0)) {
		return paramErr;
	}
	if(sslIsSessionActive(ctx)) {
		/* can't do this with an active session */
		return badReqErr;
	}
	SSLFreeBuffer(ctx->peerID, ctx);
	serr = SSLAllocBuffer(ctx->peerID, peerIDLen, ctx);
	if(serr) {
		return serr;
	}
	memmove(ctx->peerID.data, peerID, peerIDLen);
	return noErr;
}

OSStatus
SSLGetPeerID				(SSLContextRef 		ctx, 
							 const void 		**peerID,
							 size_t				*peerIDLen)
{
	*peerID = ctx->peerID.data;			// may be NULL
	*peerIDLen = ctx->peerID.length;
	return noErr;
}

OSStatus 
SSLGetNegotiatedCipher		(SSLContextRef 		ctx,
							 SSLCipherSuite 	*cipherSuite)
{
	if(ctx == NULL) {
		return paramErr;
	}
	if(!sslIsSessionActive(ctx)) {
		return badReqErr;
	}
	*cipherSuite = (SSLCipherSuite)ctx->selectedCipher;
	return noErr;
}

/*
 * Add an acceptable distinguished name (client authentication only).
 */
OSStatus
SSLAddDistinguishedName( 
	SSLContextRef ctx, 
	const void *derDN,
	size_t derDNLen)
{   
    DNListElem      *dn;
    OSStatus        err;
    
	dn = (DNListElem *)sslMalloc(sizeof(DNListElem));
	if(dn == NULL) {
		return memFullErr;
	}
    if ((err = SSLAllocBuffer(dn->derDN, derDNLen, ctx)) != 0)
        return err;
    memcpy(dn->derDN.data, derDN, derDNLen);
    dn->next = ctx->acceptableDNList;
    ctx->acceptableDNList = dn;
    return noErr;
}

/*
 * Request peer certificates. Valid anytime, subsequent to
 * a handshake attempt.
 */	
OSStatus 
SSLGetPeerCertificates		(SSLContextRef 		ctx, 
							 CFArrayRef			*certs)
{
	uint32 				numCerts;
	CFMutableArrayRef	ca;
	CFIndex				i;
	SecCertificateRef	cfd;
	OSStatus			ortn;
	CSSM_DATA			certData;
	SSLCertificate		*scert;
	
	if(ctx == NULL) {
		return paramErr;
	}
	*certs = NULL;
	
	/* 
	 * Copy peerCert, a chain of SSLCertificates, to a CFArray of 
	 * CFDataRefs, each of which is one DER-encoded cert.
	 */
	numCerts = SSLGetCertificateChainLength(ctx->peerCert);
	if(numCerts == 0) {
		return noErr;
	}
	ca = CFArrayCreateMutable(kCFAllocatorDefault,
		(CFIndex)numCerts, &kCFTypeArrayCallBacks);
	if(ca == NULL) {
		return memFullErr;	
	}
	
	/*
	 * Caller gets leaf cert first, the opposite of the way we store them.
	 */
	scert = ctx->peerCert;
	for(i=0; (unsigned)i<numCerts; i++) {
		assert(scert != NULL);		/* else SSLGetCertificateChainLength 
									 * broken */
		SSLBUF_TO_CSSM(&scert->derCert, &certData);
		ortn = SecCertificateCreateFromData(&certData,
			CSSM_CERT_X_509v3,
			CSSM_CERT_ENCODING_DER,
			&cfd);
		if(ortn) {
			CFRelease(ca);
			return ortn;
		}
		/* insert at head of array */
		CFArrayInsertValueAtIndex(ca, 0, cfd);
		scert = scert->next;
	}
	*certs = ca;
	return noErr;
}							 								 
   
/*
 * Specify Diffie-Hellman parameters. Optional; if we are configured to allow
 * for D-H ciphers and a D-H cipher is negotiated, and this function has not
 * been called, a set of process-wide parameters will be calculated. However
 * that can take a long time (30 seconds). 
 */
OSStatus SSLSetDiffieHellmanParams(
	SSLContextRef	ctx,
	const void 		*dhParams,
	size_t			dhParamsLen)
{
	if(ctx == NULL) {
		return paramErr;
	}
	if(sslIsSessionActive(ctx)) {
		return badReqErr;
	}
	SSLFreeBuffer(ctx->dhParamsPrime, ctx);
	SSLFreeBuffer(ctx->dhParamsGenerator, ctx);
	SSLFreeBuffer(ctx->dhParamsEncoded, ctx);
	
	OSStatus ortn;
	ortn = SSLCopyBufferFromData(dhParams, dhParamsLen,
		ctx->dhParamsEncoded);
	if(ortn) {
		return ortn;
	}

	/* decode for use by server over the wire */
	SSLBuffer sParams;
	sParams.data = (UInt8 *)dhParams;
	sParams.length = dhParamsLen;
	return sslDecodeDhParams(&sParams, &ctx->dhParamsPrime,
		&ctx->dhParamsGenerator);
}

/*
 * Return parameter block specified in SSLSetDiffieHellmanParams.
 * Returned data is not copied and belongs to the SSLContextRef.
 */
OSStatus SSLGetDiffieHellmanParams(
	SSLContextRef	ctx,
	const void 		**dhParams,
	size_t			*dhParamsLen)
{
	if(ctx == NULL) {
		return paramErr;
	}
	*dhParams = ctx->dhParamsEncoded.data;
	*dhParamsLen = ctx->dhParamsEncoded.length;
	return noErr;
}

OSStatus SSLSetRsaBlinding(
	SSLContextRef	ctx,
	Boolean			blinding)
{
	if(ctx == NULL) {
		return paramErr;
	}
	ctx->rsaBlindingEnable = blinding;
	return noErr;
}
									 
OSStatus SSLGetRsaBlinding(
	SSLContextRef	ctx,
	Boolean			*blinding)
{
	if(ctx == NULL) {
		return paramErr;
	}
	*blinding = ctx->rsaBlindingEnable;
	return noErr;
}

OSStatus SSLGetPeerSecTrust(
	SSLContextRef	ctx,
	SecTrustRef		*secTrust)	/* RETURNED */
{
	if(ctx == NULL) {
		return paramErr;
	}
	*secTrust = ctx->peerSecTrust;
	return noErr;
}

OSStatus SSLInternalMasterSecret(
   SSLContextRef ctx,
   void *secret,        // mallocd by caller, SSL_MASTER_SECRET_SIZE 
   size_t *secretSize)  // in/out   
{
	if((ctx == NULL) || (secret == NULL) || (secretSize == NULL)) {
		return paramErr;
	}
	if(*secretSize < SSL_MASTER_SECRET_SIZE) {
		return paramErr;
	}
	memmove(secret, ctx->masterSecret, SSL_MASTER_SECRET_SIZE);
	*secretSize = SSL_MASTER_SECRET_SIZE;
	return noErr;
}

OSStatus SSLInternalServerRandom(
   SSLContextRef ctx,
   void *rand, 			// mallocd by caller, SSL_CLIENT_SRVR_RAND_SIZE
   size_t *randSize)	// in/out   
{
	if((ctx == NULL) || (rand == NULL) || (randSize == NULL)) {
		return paramErr;
	}
	if(*randSize < SSL_CLIENT_SRVR_RAND_SIZE) {
		return paramErr;
	}
	memmove(rand, ctx->serverRandom, SSL_CLIENT_SRVR_RAND_SIZE);
	*randSize = SSL_CLIENT_SRVR_RAND_SIZE;
	return noErr;
}

OSStatus SSLInternalClientRandom(
   SSLContextRef ctx,
   void *rand,  		// mallocd by caller, SSL_CLIENT_SRVR_RAND_SIZE
   size_t *randSize)	// in/out   
{
	if((ctx == NULL) || (rand == NULL) || (randSize == NULL)) {
		return paramErr;
	}
	if(*randSize < SSL_CLIENT_SRVR_RAND_SIZE) {
		return paramErr;
	}
	memmove(rand, ctx->clientRandom, SSL_CLIENT_SRVR_RAND_SIZE);
	*randSize = SSL_CLIENT_SRVR_RAND_SIZE;
	return noErr;
}

OSStatus 
SSLGetResumableSessionInfo(
	SSLContextRef	ctx,
	Boolean			*sessionWasResumed,		// RETURNED
	void			*sessionID,				// RETURNED, mallocd by caller
	size_t			*sessionIDLength)		// IN/OUT
{
	if((ctx == NULL) || (sessionWasResumed == NULL) || 
	   (sessionID == NULL) || (sessionIDLength == NULL) ||
	   (*sessionIDLength < MAX_SESSION_ID_LENGTH)) {
		return paramErr;
	}
	if(ctx->sessionMatch) {
		assert(ctx->sessionID.data != NULL);
		*sessionWasResumed = true;
		if(ctx->sessionID.length > *sessionIDLength) {
			/* really should never happen - means ID > 32 */
			return paramErr;
		}
		memmove(sessionID, ctx->sessionID.data, ctx->sessionID.length);
		*sessionIDLength = ctx->sessionID.length;
	}
	else {
		*sessionWasResumed = false;
		*sessionIDLength = 0;
	}
	return noErr;
}



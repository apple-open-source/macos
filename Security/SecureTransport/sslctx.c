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
	File:		sslctx.c

	Contains:	SSLContext accessors

	Written by:	Doug Mitchell, based on Netscape RSARef 3.0

	Copyright: (c) 1999 by Apple Computer, Inc., all rights reserved.

*/
/*  *********************************************************************
    File: sslctx.c

    SSLRef 3.0 Final -- 11/19/96

    Copyright (c)1996 by Netscape Communications Corp.

    By retrieving this software you are bound by the licensing terms
    disclosed in the file "LICENSE.txt". Please read it, and if you don't
    accept the terms, delete this software.

    SSLRef 3.0 was developed by Netscape Communications Corp. of Mountain
    View, California <http://home.netscape.com/> and Consensus Development
    Corporation of Berkeley, California <http://www.consensus.com/>.

    *********************************************************************

    File: sslctx.c     SSLContext accessors

    Functions called by the end user which configure an SSLContext
    structure or access data stored there.

    ****************************************************************** */


#include "ssl.h"
#include "sslctx.h"
#include "sslalloc.h"
#include <CoreServices/../Frameworks/CarbonCore.framework/Headers/MacErrors.h>
#include "digests.h"
#include "sslDebug.h"
#include "appleCdsa.h"
#include "appleGlue.h"
#include "sslKeychain.h"
#include "sslutil.h"
#include "cipherSpecs.h"

#include <string.h>

static void sslFreeDnList(
	SSLContext *ctx)
{
    DNListElem      *dn, *nextDN;
    SSLBuffer       buf;
    
    dn = ctx->acceptableDNList;

    while (dn)
    {   
    	SSLFreeBuffer(&dn->derDN, &ctx->sysCtx);
        nextDN = dn->next;
        buf.data = (uint8*)dn;
        buf.length = sizeof(DNListElem);
        SSLFreeBuffer(&buf, &ctx->sysCtx);
        dn = nextDN;
    }
    ctx->acceptableDNList = NULL;
}

static SSLErr sslFreeTrustedRoots(
	SSLContext *ctx)
{
	int i;
	
	CASSERT(ctx != NULL);
	if((ctx->numTrustedCerts == 0) || (ctx->trustedCerts == NULL)) {
		/* they really should both be zero, right? */
		CASSERT((ctx->numTrustedCerts == 0) && (ctx->trustedCerts == NULL));
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
	return SSLNoErr;
}

OSStatus
SSLNewContext				(Boolean 			isServer,
							 SSLContextRef 		*contextPtr)	/* RETURNED */
{
	SSLContext 	*ctx;
	OSStatus 	oerr;
	SSLErr		serr;
		
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
    ctx->state = SSLUninitialized;
    
    /* different defaults for client and server ... */
    if(isServer) {
    	ctx->protocolSide = SSL_ServerSide;
    	ctx->reqProtocolVersion = SSL_Version_3_0;
    }
    else {
    	ctx->protocolSide = SSL_ClientSide;
    	ctx->reqProtocolVersion = SSL_Version_Undetermined;
    }
    ctx->negProtocolVersion = SSL_Version_Undetermined;
	
    /* Initialize the cipher state to NULL_WITH_NULL_NULL */
    ctx->selectedCipherSpec = &SSL_NULL_WITH_NULL_NULL_CipherSpec;
    ctx->selectedCipher = ctx->selectedCipherSpec->cipherSpec;
    ctx->writeCipher.hash = ctx->selectedCipherSpec->macAlgorithm;
    ctx->readCipher.hash = ctx->selectedCipherSpec->macAlgorithm;
    ctx->readCipher.symCipher = ctx->selectedCipherSpec->cipher;
    ctx->writeCipher.symCipher = ctx->selectedCipherSpec->cipher;
	
	#if		_APPLE_CDSA_
	/* these two are invariant */
    ctx->writeCipher.encrypting = 1;
    ctx->writePending.encrypting = 1;
	#endif	/* _APPLE_CDSA_ */
	
    /* this gets init'd on first call to SSLHandshake() */
    ctx->validCipherSpecs = NULL;
    ctx->numValidCipherSpecs = 0;
    
    SSLInitMACPads();
	if(cfSetUpAllocators(ctx)) {
		oerr = memFullErr;
		goto errOut;
	}
	
	/* attach to CSP, CL, TP */
	serr = attachToAll(ctx);
	if(serr) {
		oerr = sslErrToOsStatus(serr);
		goto errOut;
	}
	
	/* snag root certs from Keychain, tolerate error */
	addBuiltInCerts(ctx);
	
    *contextPtr = ctx;
    return noErr;
    
errOut:
	sslFree(ctx);
	return oerr;
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
    SSLFreeBuffer(&ctx->partialReadBuffer, &ctx->sysCtx);
    
    wait = ctx->recordWriteQueue;
    while (wait)
    {   SSLFreeBuffer(&wait->data, &ctx->sysCtx);
        next = wait->next;
        buf.data = (uint8*)wait;
        buf.length = sizeof(WaitingRecord);
        SSLFreeBuffer(&buf, &ctx->sysCtx);
        wait = next;
    }
    
    SSLFreeBuffer(&ctx->dhPeerPublic, &ctx->sysCtx);
    SSLFreeBuffer(&ctx->dhExchangePublic, &ctx->sysCtx);
    SSLFreeBuffer(&ctx->dhPrivate, &ctx->sysCtx);
    
    SSLFreeBuffer(&ctx->shaState, &ctx->sysCtx);
    SSLFreeBuffer(&ctx->md5State, &ctx->sysCtx);
    
    SSLFreeBuffer(&ctx->sessionID, &ctx->sysCtx);
    SSLFreeBuffer(&ctx->peerID, &ctx->sysCtx);
    SSLFreeBuffer(&ctx->resumableSession, &ctx->sysCtx);
    SSLFreeBuffer(&ctx->preMasterSecret, &ctx->sysCtx);
    SSLFreeBuffer(&ctx->partialReadBuffer, &ctx->sysCtx);
    SSLFreeBuffer(&ctx->fragmentedMessageCache, &ctx->sysCtx);
    SSLFreeBuffer(&ctx->receivedDataBuffer, &ctx->sysCtx);

    SSLDisposeCipherSuite(&ctx->readCipher, ctx);
    SSLDisposeCipherSuite(&ctx->writeCipher, ctx);
    SSLDisposeCipherSuite(&ctx->readPending, ctx);
    SSLDisposeCipherSuite(&ctx->writePending, ctx);

	sslFree(ctx->validCipherSpecs);
	ctx->validCipherSpecs = NULL;
	ctx->numValidCipherSpecs = 0;
	
	/* free APPLE_CDSA stuff */
	#if		ST_KEYCHAIN_ENABLE
	sslFreeKey(ctx->signingKeyCsp, &ctx->signingPrivKey, &ctx->signingKeyRef);
	sslFreeKey(ctx->encryptKeyCsp, &ctx->encryptPrivKey, &ctx->encryptKeyRef);
	#else	
	sslFreeKey(ctx->signingKeyCsp, &ctx->signingPrivKey, NULL);
	sslFreeKey(ctx->encryptKeyCsp, &ctx->encryptPrivKey, NULL);
	#endif	/* ST_KEYCHAIN_ENABLE */
	sslFreeKey(ctx->signingKeyCsp, &ctx->signingPubKey, NULL);
	sslFreeKey(ctx->encryptKeyCsp, &ctx->encryptPubKey, NULL);
	sslFreeKey(ctx->peerPubKeyCsp, &ctx->peerPubKey, NULL);
	
	#if		SSL_DEBUG
	if(ctx->rootCertName != NULL) {
		sslFree(ctx->rootCertName);
	}
	#endif	/* SSL_DEBUG */
	
	sslFreeTrustedRoots(ctx);
	
	detachFromAll(ctx);
	    
    cfTearDownAllocators(ctx);
    memset(ctx, 0, sizeof(SSLContext));
    sslFree(ctx);
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
		case SSLUninitialized:
		case HandshakeServerUninit:
		case HandshakeClientUninit:
			rtnState = kSSLIdle;
			break;
		case SSLGracefulClose:
			rtnState = kSSLClosed;
			break;
		case SSLErrorClose:
		case SSLNoNotifyClose:
			rtnState = kSSLAborted;
			break;
		case HandshakeServerReady:
		case HandshakeClientReady:
			rtnState = kSSLConnected;
			break;
		default:
			CASSERT((context->state >= HandshakeServerHello) &&
			        (context->state <= HandshakeSSL2ServerFinished));
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
SSLSetProtocolVersion		(SSLContextRef 		ctx,
							 SSLProtocol		version)
{   
	SSLProtocolVersion	versInt;
	
	if(ctx == NULL) {
		return paramErr;
	}
	if(sslIsSessionActive(ctx)) {
		/* can't do this with an active session */
		return badReqErr;
	}

	/* convert external representation to private */
	switch(version) {
		case kSSLProtocolUnknown:
			versInt = SSL_Version_Undetermined;
			break;
		case kSSLProtocol2:
			versInt = SSL_Version_2_0;
			break;
		case kSSLProtocol3:
			/* this tells us to do our best but allows 2.0 */
			versInt = SSL_Version_Undetermined;
			break;
		case kSSLProtocol3Only:
			versInt = SSL_Version_3_0_Only;
			break;
		default:
			return paramErr;
	}
	ctx->reqProtocolVersion = ctx->negProtocolVersion = versInt;
    return noErr;
}

static SSLProtocol convertProtToExtern(SSLProtocolVersion prot)
{
	switch(prot) {
		case SSL_Version_Undetermined:
			return kSSLProtocolUnknown;
		case SSL_Version_3_0_Only:
			return kSSLProtocol3Only;
		case SSL_Version_2_0:
			return kSSLProtocol2;
		case SSL_Version_3_0:
			return kSSLProtocol3;
		case SSL_Version_3_0_With_2_0_Hello:
			sslPanic("How did we get SSL_Version_3_0_With_2_0_Hello?");
		default:
			sslPanic("convertProtToExtern: bad prot");
	}
	/* not reached but make compiler happy */
	return kSSLProtocolUnknown;
}

OSStatus 
SSLGetProtocolVersion		(SSLContextRef		ctx,
							 SSLProtocol		*protocol)		/* RETURNED */
{
	if(ctx == NULL) {
		return paramErr;
	}
	*protocol = convertProtToExtern(ctx->reqProtocolVersion);
	return noErr;
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
SSLSetAllowExpiredCerts	(SSLContextRef		ctx,
						 Boolean			allowExpired)
{
	if(ctx == NULL) {
		return paramErr;
	}
	if(sslIsSessionActive(ctx)) {
		/* can't do this with an active session */
		return badReqErr;
	}
	ctx->allowExpiredCerts = allowExpired;
	return noErr;
}

OSStatus
SSLGetAllowExpiredCerts		(SSLContextRef		ctx,
							 Boolean			*allowExpired)
{
	if(ctx == NULL) {
		return paramErr;
	}
	*allowExpired = ctx->allowExpiredCerts;
	return noErr;
}

OSStatus SSLSetAllowAnyRoot(
	SSLContextRef	ctx,
	Boolean			anyRoot)
{
	if(ctx == NULL) {
		return paramErr;
	}
	ctx->allowAnyRoot = anyRoot;
	return noErr;
}

OSStatus
SSLGetAllowAnyRoot(
	SSLContextRef	ctx,
	Boolean			*anyRoot)
{
	if(ctx == NULL) {
		return paramErr;
	}
	*anyRoot = ctx->allowAnyRoot;
	return noErr;
}

#if	ST_SERVER_MODE_ENABLE
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
			/* FIXME - needs work to distinguish these cases at
			 * handshake time */
			ctx->tryClientAuth = true;
			break;
	}
	return noErr;
}
#endif	/* ST_SERVER_MODE_ENABLE */

#if	(ST_SERVER_MODE_ENABLE || ST_CLIENT_AUTHENTICATION)

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
	return parseIncomingCerts(ctx,
		certRefs,
		&ctx->localCert,
		&ctx->signingPubKey,
		&ctx->signingPrivKey,
		&ctx->signingKeyCsp,
		&ctx->signingKeyRef);
}
#endif	/* (ST_SERVER_MODE_ENABLE || ST_CLIENT_AUTHENTICATION) */

#if	ST_SERVER_MODE_ENABLE
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
	return parseIncomingCerts(ctx,
		certRefs,
		&ctx->encryptCert,
		&ctx->encryptPubKey,
		&ctx->encryptPrivKey,
		&ctx->encryptKeyCsp,
		&ctx->encryptKeyRef);
}
#endif	/* ST_SERVER_MODE_ENABLE*/

#if		ST_KEYCHAIN_ENABLE

/*
 * Add (optional, additional) trusted root certs.
 */
OSStatus
SSLSetTrustedRootCertKC		(SSLContextRef		ctx,
							 KCRef				keyChainRef,
							 Boolean			deleteExisting)
{
	/*
	 * -- free trustedCerts if deleteExisting
	 * -- Get raw cert data, add to ctx->trustedCerts
	 * -- verify that each of these is a valid (self-verifying)
	 *    root cert
	 * -- add each subject name to acceptableDNList
	 */
	if((ctx == NULL) || (keyChainRef == nil)) {
		return paramErr;
	}
	if(sslIsSessionActive(ctx)) {
		/* can't do this with an active session */
		return badReqErr;
	}
	if(deleteExisting) {
		sslFreeTrustedRoots(ctx);
	}
	return parseTrustedKeychain(ctx, keyChainRef);
}

OSStatus 
SSLSetNewRootKC				(SSLContextRef		ctx,
							 KCRef				keyChainRef,
							 void				*accessCreds)
{
	if((ctx == NULL) || (keyChainRef == nil)) {
		return paramErr;
	}
	if(sslIsSessionActive(ctx)) {
		/* can't do this with an active session */
		return badReqErr;
	}
	if(ctx->newRootCertKc != NULL) {
		/* can't do this multiple times */
		return badReqErr;
	}
	ctx->newRootCertKc = keyChainRef;
	ctx->accessCreds = accessCreds;
	return noErr;
}
#endif	/* ST_KEYCHAIN_ENABLE */

OSStatus 
SSLSetPeerID				(SSLContext 		*ctx, 
							 CFDataRef 			peerID)
{
	SSLErr serr;
	uint32 len;
	
	/* copy peerId to context->peerId */
	if((ctx == NULL) || 
	   (peerID == NULL) ||
	   ((len = CFDataGetLength(peerID)) == 0)) {
		return paramErr;
	}
	if(sslIsSessionActive(ctx)) {
		/* can't do this with an active session */
		return badReqErr;
	}
	SSLFreeBuffer(&ctx->peerID, &ctx->sysCtx);
	serr = SSLAllocBuffer(&ctx->peerID, len, &ctx->sysCtx);
	if(serr) {
		return sslErrToOsStatus(serr);
	}
	memmove(ctx->peerID.data, CFDataGetBytePtr(peerID), len);
	ctx->peerID.length = len;
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
 * Add an acceptable distinguished name.
 * FIXME - this looks like a big hole in the SSLRef code; 
 * acceptableDNList is set here and in SSLProcessCertificateRequest();
 * it's used and sent to a client in SSLEncodeCertificateRequest();
 * but the list is never used to decide what certs to send!
 *
 * Also FIXME - this allocation of dnBufs is total horseshit. The
 * SSLBufs can never get freed. Why not just allocate the 
 * raw DNListElems? Sheesh. 
 */
#if 0
/* not used */
static SSLErr
SSLAddDistinguishedName(SSLContext *ctx, SSLBuffer derDN)
{   SSLBuffer       dnBuf;
    DNListElem      *dn;
    SSLErr          err;
    
    if ((err = SSLAllocBuffer(&dnBuf, sizeof(DNListElem), &ctx->sysCtx)) != 0)
        return err;
    dn = (DNListElem*)dnBuf.data;
    if ((err = SSLAllocBuffer(&dn->derDN, derDN.length, &ctx->sysCtx)) != 0)
    {   SSLFreeBuffer(&dnBuf, &ctx->sysCtx);
        return err;
    }
    memcpy(dn->derDN.data, derDN.data, derDN.length);
    dn->next = ctx->acceptableDNList;
    ctx->acceptableDNList = dn;
    return SSLNoErr;
}
#endif	/* not used */

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
	CFDataRef			cfd;
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
	ca = CFArrayCreateMutable(ctx->cfAllocatorRef,
		(CFIndex)numCerts, &kCFTypeArrayCallBacks);
	if(ca == NULL) {
		return memFullErr;	
	}
	
	/*
	 * We'll give the certs in the same order we store them -
	 * caller gets root first. OK?
	 */
	scert = ctx->peerCert;
	for(i=0; i<numCerts; i++) {
		CASSERT(scert != NULL);		/* else SSLGetCertificateChainLength 
									 * broken */
		cfd = CFDataCreate(ctx->cfAllocatorRef,
				scert->derCert.data,
				scert->derCert.length);
		if(cfd == NULL) {
			CFRelease(ca);
			return memFullErr;
		}
		CFArrayAppendValue(ca, cfd);
		scert = scert->next;
	}
	*certs = ca;
	return noErr;
}							 								 
   



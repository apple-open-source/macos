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

	Written by:	Doug Mitchell, based on Netscape SSLRef 3.0

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
#include "appleSession.h"
#include <string.h>
#include <Security/SecCertificate.h>

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

/*
 * Default attempted version. 
 */
#define DEFAULT_MAX_VERSION		TLS_Version_1_0	

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
    	ctx->reqProtocolVersion = DEFAULT_MAX_VERSION;
    }
    else {
    	ctx->protocolSide = SSL_ClientSide;
    	ctx->reqProtocolVersion = SSL_Version_Undetermined;
    }
    ctx->negProtocolVersion = SSL_Version_Undetermined;
	ctx->maxProtocolVersion = DEFAULT_MAX_VERSION;
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

    SSLInitMACPads();
	
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
    
	CloseHash(&SSLHashSHA1, &ctx->shaState, ctx);
	CloseHash(&SSLHashMD5,  &ctx->md5State, ctx);
    
    SSLFreeBuffer(&ctx->sessionID, &ctx->sysCtx);
    SSLFreeBuffer(&ctx->peerID, &ctx->sysCtx);
    SSLFreeBuffer(&ctx->resumableSession, &ctx->sysCtx);
    SSLFreeBuffer(&ctx->preMasterSecret, &ctx->sysCtx);
    SSLFreeBuffer(&ctx->partialReadBuffer, &ctx->sysCtx);
    SSLFreeBuffer(&ctx->fragmentedMessageCache, &ctx->sysCtx);
    SSLFreeBuffer(&ctx->receivedDataBuffer, &ctx->sysCtx);

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
	
	/* free APPLE_CDSA stuff */
	#if 0
	/* As of 5/3/02, we don't need to free these keys; they belong
	 * to SecKeychain */
	#if		ST_KEYCHAIN_ENABLE && ST_KC_KEYS_NEED_REF
	sslFreeKey(ctx->signingKeyCsp, &ctx->signingPrivKey, &ctx->signingKeyRef);
	sslFreeKey(ctx->encryptKeyCsp, &ctx->encryptPrivKey, &ctx->encryptKeyRef);
	#else	
	sslFreeKey(ctx->signingKeyCsp, (CSSM_KEY_PTR *)&ctx->signingPrivKey, NULL);
	sslFreeKey(ctx->encryptKeyCsp, (CSSM_KEY_PTR *)&ctx->encryptPrivKey, NULL);
	#endif	/* ST_KEYCHAIN_ENABLE && ST_KC_KEYS_NEED_REF */
	#endif	/* 0 */
	
	/*
	 * NOTE: currently, all public keys come from the CL via CSSM_CL_CertGetKeyInfo.
	 * We really don't know what CSP the CL used to generate a public key (in fact,
	 * it uses the raw CSP only to get LogicalKeySizeInBits, but we can't know
	 * that). Thus using e.g. signingKeyCsp (or any other CSP) to free 
	 * signingPubKey is not tecnically accurate. However, our public keys 
	 * are all raw keys, and all Apple CSPs dispose of raw keys in the same
	 * way.
	 */
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
	ctx->peerDomainName = sslMalloc(peerNameLen);
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

OSStatus 
SSLSetProtocolVersion		(SSLContextRef 		ctx,
							 SSLProtocol		version)
{   
	SSLProtocolVersion	versInt;
	SSLProtocolVersion	versMax;
	
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
			versMax = DEFAULT_MAX_VERSION;
			break;
		case kSSLProtocol2:
			versInt = versMax = SSL_Version_2_0;
			break;
		case kSSLProtocol3:
			/* this tells us to do our best but allows 2.0 */
			versInt = SSL_Version_Undetermined;
			versMax = SSL_Version_3_0;
			break;
		case kSSLProtocol3Only:
			versInt = SSL_Version_3_0_Only;
			versMax = SSL_Version_3_0;
			break;
		case kTLSProtocol1:
			/* this tells us to do our best but allows 2.0 */
			versInt = SSL_Version_Undetermined;
			versMax = TLS_Version_1_0;
			break;
		case kTLSProtocol1Only:
			versInt = TLS_Version_1_0_Only;
			versMax = TLS_Version_1_0;
			break;
		default:
			return paramErr;
	}
	ctx->reqProtocolVersion = ctx->negProtocolVersion = versInt;
	ctx->maxProtocolVersion = versMax;
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
		case TLS_Version_1_0_Only:
			return kTLSProtocol1Only;
		case TLS_Version_1_0:
			return kTLSProtocol1;
		/* this can happen in an intermediate state while negotiation
		 * is in progress...right? */
		case SSL_Version_3_0_With_2_0_Hello:
			return kSSLProtocolUnknown;
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
SSLSetAllowsExpiredCerts(SSLContextRef		ctx,
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
SSLGetAllowsExpiredCerts	(SSLContextRef		ctx,
							 Boolean			*allowExpired)
{
	if(ctx == NULL) {
		return paramErr;
	}
	*allowExpired = ctx->allowExpiredCerts;
	return noErr;
}

OSStatus SSLSetAllowsAnyRoot(
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
		&ctx->signingKeyCsp
		#if ST_KC_KEYS_NEED_REF
		,
		&ctx->signingKeyRef
		#else
		);
		#endif
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
		&ctx->encryptKeyCsp
		#if	ST_KC_KEYS_NEED_REF
		,
		&ctx->encryptKeyRef);
		#else
		);
		#endif
}
#endif	/* ST_SERVER_MODE_ENABLE*/

#if		ST_KEYCHAIN_ENABLE && ST_MANAGES_TRUSTED_ROOTS

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
#endif	/* ST_KEYCHAIN_ENABLE && ST_MANAGES_TRUSTED_ROOTS */

OSStatus 
SSLSetPeerID				(SSLContext 		*ctx, 
							 const void 		*peerID,
							 size_t				peerIDLen)
{
	SSLErr serr;
	
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
	SSLFreeBuffer(&ctx->peerID, &ctx->sysCtx);
	serr = SSLAllocBuffer(&ctx->peerID, peerIDLen, &ctx->sysCtx);
	if(serr) {
		return sslErrToOsStatus(serr);
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
 * Add an acceptable distinguished name.
 * FIXME - this looks like a big hole in the SSLRef code; 
 * acceptableDNList is set here and in SSLProcessCertificateRequest();
 * it's used and sent to a client in SSLEncodeCertificateRequest();
 * but the list is never used to decide what certs to send!
 *
 * Also FIXME - this allocation of dnBufs is preposterous. The
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
	for(i=0; i<numCerts; i++) {
		CASSERT(scert != NULL);		/* else SSLGetCertificateChainLength 
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
   



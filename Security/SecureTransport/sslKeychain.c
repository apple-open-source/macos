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
	File:		sslKeychain.c

	Contains:	Apple Keychain routines

	Written by:	Doug Mitchell, based on Netscape RSARef 3.0

	Copyright: (c) 1999 by Apple Computer, Inc., all rights reserved.

*/

#include "ssl.h"
#include "sslctx.h"
#include "sslalloc.h"
#include "appleCdsa.h"
#include "appleGlue.h"
#include "sslerrs.h"
#include "sslDebug.h"
#include "sslKeychain.h"
#include "sslutil.h"

#if		ST_KEYCHAIN_ENABLE
#include <Keychain.h>
#include <KeychainPriv.h>
#endif	/* ST_KEYCHAIN_ENABLE */

#include <string.h>

#if		ST_KEYCHAIN_ENABLE
static OSStatus
addCertData(
	SSLContext		*ctx,
	KCItemRef		kcItem,
	CSSM_DATA_PTR	certData,
	Boolean			*goodCert);		/* RETURNED */

/*
 * Given a KCItemRef: is this item a cert?
 */
static Boolean
isItemACert(KCItemRef kcItem)
{	
	KCAttribute		attr;
	FourCharCode	itemClass;
	OSStatus		ortn;
	UInt32			len;
	
	attr.tag = kClassKCItemAttr;
	attr.length = sizeof(FourCharCode);
	attr.data = &itemClass;
	
	ortn = KCGetAttribute (kcItem, &attr, &len);
	if (ortn == noErr) {
		return((itemClass == kCertificateKCItemClass) ? true : false);
	}
	else {
		errorLog1("isItemACert: KCGetAttribute returned %d\n", ortn);
		return false;
	}
}

#endif	/* ST_KEYCHAIN_ENABLE */

#if		(ST_SERVER_MODE_ENABLE || ST_CLIENT_AUTHENTICATION)
/*
 * Given an array of certs (as KCItemRefs, specified by caller
 * in SSLSetCertificate or SSLSetEncryptionCertificate) and a 
 * destination SSLCertificate:
 *
 * -- free destCerts if we have any
 * -- Get raw cert data, convert to array of SSLCertificates in *destCert 
 * -- validate cert chain
 * -- get pub, priv keys from certRef[0], store in *pubKey, *privKey
 */
OSStatus 
parseIncomingCerts(
	SSLContext		*ctx,
	CFArrayRef		certs,
	SSLCertificate	**destCert,		/* &ctx->{localCert,encryptCert} */
	CSSM_KEY_PTR	*pubKey,		/* &ctx->signingPubKey, etc. */
	CSSM_KEY_PTR	*privKey,		/* &ctx->signingPrivKey, etc. */
	CSSM_CSP_HANDLE	*cspHand,		/* &ctx->signingKeyCsp, etc. */
	KCItemRef		*privKeyRef)	/* &ctx->signingKeyRef, etc. */
{
	CFIndex			numCerts;
	CFIndex			cert;
	SSLCertificate	*certChain = NULL;
	SSLCertificate	*thisSslCert;
	KCItemRef		kcItem;
	SSLBuffer		*derSubjCert = NULL;
	UInt32			certLen;
	OSStatus		ortn;
	SSLErr			srtn;
	FromItemGetPrivateKeyParams	keyParams = {NULL, NULL};
	FromItemGetKeyInfoParams	keyInfo = {NULL, NULL, 0};
	CSSM_CSP_HANDLE				dummyCsp;
	
	CASSERT(ctx != NULL);
	CASSERT(destCert != NULL);		/* though its referent may be NULL */
	CASSERT(pubKey != NULL);
	CASSERT(privKey != NULL);
	CASSERT(cspHand != NULL);
	CASSERT(privKeyRef != NULL);
	
	sslDeleteCertificateChain(*destCert, ctx);
	*destCert = NULL;
	*pubKey   = NULL;
	*privKey  = NULL;
	*cspHand  = 0;
	
	if(certs == NULL) {
		dprintf0("parseIncomingCerts: NULL incoming cert array\n");
		return errSSLBadCert;
	}
	numCerts = CFArrayGetCount(certs);
	if(numCerts == 0) {
		dprintf0("parseIncomingCerts: empty incoming cert array\n");
		return errSSLBadCert;
	}
	
	/* 
	 * Convert: CFArray of KCItemRefs --> chain of SSLCertificates. 
	 * Incoming certs have root last; SSLCertificate chain has root
	 * first.
	 */
	for(cert=0; cert<numCerts; cert++) {
		kcItem = (KCItemRef)CFArrayGetValueAtIndex(certs, cert);
		if(kcItem == NULL) {
			errorLog0("parseIncomingCerts: bad cert array\n");
			return paramErr;
		}	
		if(!isItemACert(kcItem)) {
			/* client app error, not ours */
			return paramErr;
		}
		
		/* 
		 * OK, cook up an SSLCertificate and its associated SSLBuffer.
		 * First the size of the actual cert data...
		 */
		ortn = KCGetData(kcItem, 0,  NULL, &certLen);
		if(ortn != noErr) {
			errorLog1("parseIncomingCerts: KCGetData(1) returned %d\n", ortn);
			return ortn;
		}
		thisSslCert = sslMalloc(sizeof(SSLCertificate));
		if(thisSslCert == NULL) {
			return memFullErr;
		}
		if(SSLAllocBuffer(&thisSslCert->derCert, certLen, &ctx->sysCtx)) {
			return memFullErr;
		}
		
		/* now the data itself */
		ortn = KCGetData (kcItem, 
			certLen, 
			thisSslCert->derCert.data, 
			&certLen);
		if(ortn) {
			errorLog1("parseIncomingCerts: KCGetData(2) returned %d\n", ortn);
			SSLFreeBuffer(&thisSslCert->derCert, &ctx->sysCtx);
			return ortn;
		}
		
		/* enqueue onto head of cert chain */
		thisSslCert->next = certChain;
		certChain = thisSslCert;
		
		if(derSubjCert == NULL) {
			/* Save this ptr for obtaining public key */
			derSubjCert = &thisSslCert->derCert;
		}
	}
	
	/* validate the whole mess */
	srtn = sslVerifyCertChain(ctx, certChain);
	if(srtn) {
		ortn = sslErrToOsStatus(srtn);
		goto errOut;
	}
	
	/* 
	 * Get privKey, pubKey, KCItem of certs[0].
	 * First, the private key, from the Keychain, using crufy private API.
	 */
	keyParams.item = (KCItemRef)CFArrayGetValueAtIndex(certs, 0);
	ortn = KCDispatch(kKCFromItemGetPrivateKey, &keyParams);
	if(ortn) {
		errorLog1("KCDispatch(kKCFromItemGetPrivateKey) returned %d\n", ortn);
		goto errOut;
	}
	keyInfo.item = keyParams.privateKeyItem;
	ortn = KCDispatch(kKCFromItemGetKeyInfo, &keyInfo);
	if(ortn) {
		errorLog1("KCDispatch(kKCFromItemGetKeyInfo) returned %d\n", ortn);
		goto errOut;
	}
	*privKey = (CSSM_KEY_PTR)keyInfo.keyPtr;
	*cspHand = keyInfo.cspHandle;
	*privKeyRef = keyParams.privateKeyItem;
	
	/* now the public key, from CL */
	/* FIXME - what if this CSP differs from the one we got from KC??? */
	srtn = sslPubKeyFromCert(ctx, 
		derSubjCert, 
		pubKey,
		&dummyCsp);
	if(srtn) {
		errorLog1("sslPubKeyFromCert returned %d\n", srtn);
		ortn = sslErrToOsStatus(srtn);
		goto errOut;
	}
	
	/* SUCCESS */ 
	*destCert = certChain;
	return noErr;
	
errOut:
	/* free certChain, everything in it, other vars, return ortn */
	sslDeleteCertificateChain(certChain, ctx);
	if(keyInfo.keyPtr != NULL) {
		sslFreeKey(keyInfo.cspHandle, &keyInfo.keyPtr, NULL);
	}
	if(keyParams.privateKeyItem != NULL) {
		KCReleaseItem(&keyParams.privateKeyItem);
	}
	return ortn;
}
#endif	/* (ST_SERVER_MODE_ENABLE || ST_CLIENT_AUTHENTICATION) */

/*
 * Add Apple built-in root certs to ctx->trustedCerts.
 */
OSStatus addBuiltInCerts	(SSLContextRef		ctx)
{
	#if		ST_KEYCHAIN_ENABLE
	OSStatus 			ortn;
	KCRef				kc = nil;
	
	ortn = KCDispatch(kKCGetRootCertificateKeychain, &kc);
	if(ortn) {
		errorLog1("KCDispatch(kKCGetRootCertificateKeychain) returned %d\n", 
			ortn);
		return ortn;
	}
	return parseTrustedKeychain(ctx, kc);
	#else
	/* nothing for now */
	return noErr;
	#endif	/* ST_KEYCHAIN_ENABLE */
}

#if		ST_KEYCHAIN_ENABLE 

/*
 * Given an open Keychain:
 * -- Get raw cert data, add to array of CSSM_DATAs in 
 *    ctx->trustedCerts 
 * -- verify that each of these is a valid (self-verifying)
 *    root cert
 * -- add each subject name to acceptableDNList
 */
OSStatus
parseTrustedKeychain		(SSLContextRef		ctx,
							 KCRef				keyChainRef)
{
	CFMutableArrayRef	kcCerts = NULL;		/* all certs in one keychain */
	uint32				numGoodCerts = 0;	/* # of good root certs */
	CSSM_DATA_PTR		certData = NULL;	/* array of CSSM_DATAs */
	CFIndex				certDex;			/* index into kcCerts */
	CFIndex				certsPerKc;			/* # of certs in this KC */
	OSStatus			ortn;
	KCItemRef			kcItem;				/* one cert */
	Boolean				goodCert;
	
	CASSERT(ctx != NULL);
	if(keyChainRef == NULL) {
		return paramErr;
	}
	
	ortn = KCFindX509Certificates(keyChainRef,
		NULL,				// name, XXX
		NULL,				// emailAddress, XXX
		kCertSearchAny,		// options
		&kcCerts);			// results
	switch(ortn) {
		case noErr:
			break;					// proceed
		case errKCItemNotFound:
			return noErr;			// no certs; done
		default:
			errorLog1("parseTrustedKeychains: KCFindX509Certificates returned %d\n",
				ortn);
			return ortn;
	}
	if(kcCerts == NULL) {
		dprintf0("parseTrustedKeychains: no certs in KC\n");
		return noErr;
	}
	
	/* Note kcCerts must be released on any exit, successful or
	 * otherwise. */
	
	certsPerKc = CFArrayGetCount(kcCerts);	

	/* 
	 * This array gets allocd locally; we'll add it to 
	 * ctx->trustedCerts when we're done.
	 */
	certData = sslMalloc(certsPerKc * sizeof(CSSM_DATA));
	if(certData == NULL) {
		ortn = memFullErr;
		goto errOut;
	}
	memset(certData, 0, certsPerKc * sizeof(CSSM_DATA));
	
	/* 
	 * Build up local certData one root cert at a time. 
	 * Some certs might not pass muster, hence the numGoodCerts
	 * which may or may not increment each time thru.
	 */
	for(certDex=0; certDex<certsPerKc; certDex++) {
		kcItem = (KCItemRef)CFArrayGetValueAtIndex(kcCerts, certDex);
		if(kcItem == NULL) {
			errorLog0("parseTrustedKeychains: CF error 1\n");
			ortn = errSSLInternal;
			goto errOut;
		}
		if(!KCIsRootCertificate(kcItem)) {
			/* not root, OK, skip to next cert */
			dprintf1("parseTrustedKeychains: cert %d NOT ROOT\n",	certDex);
			continue;
		}
		ortn = addCertData(ctx,
			kcItem, 
			&certData[numGoodCerts], 
			&goodCert);
		if(ortn) {
			goto errOut;
		}
		if(goodCert) {
			/* added valid root to certData */
			numGoodCerts++;
		}
	}	/* for each cert in kcCerts */

	#if	SSL_DEBUG
	verifyTrustedRoots(ctx, certData, numGoodCerts);
	#endif

	/* Realloc ctx->trustedCerts, add new root certs */
	ctx->trustedCerts = sslRealloc(ctx->trustedCerts, 
		ctx->numTrustedCerts * sizeof(CSSM_DATA),
		(ctx->numTrustedCerts + numGoodCerts) * sizeof(CSSM_DATA));
	if(ctx->trustedCerts == NULL) {
		ortn = memFullErr;
		goto errOut;
	}
	for(certDex=0; certDex<numGoodCerts; certDex++) {
		ctx->trustedCerts[ctx->numTrustedCerts + certDex] = certData[certDex];
	}
	ctx->numTrustedCerts += numGoodCerts;
	ortn = noErr;
	
	#if	SSL_DEBUG
	verifyTrustedRoots(ctx, ctx->trustedCerts, ctx->numTrustedCerts);
	#endif
	
errOut:
	sslFree(certData);
	if(kcCerts != NULL) {
		CFRelease(kcCerts);
	}
	return ortn;
}

/*
 * Given a cert as a KCItemRef:
 * -- verify that the cert self-verifies
 * -- add its DER-encoded data *certData.
 * -- Add its subjectName to acceptableDNList.
 * -- If all is well, return True in *goodCert.
 *
 * The actual CSSM_DATA.Data is mallocd via CSSM_Malloc. 
 */
static OSStatus
addCertData(
	SSLContext		*ctx,
	KCItemRef		kcItem,
	CSSM_DATA_PTR	certData,
	Boolean			*goodCert)		/* RETURNED */
{	
	UInt32			certSize;
	OSStatus		ortn;
	SSLErr			srtn;
	CSSM_BOOL		subjectExpired;
	CSSM_DATA_PTR	dnData;
	
	CASSERT(ctx != NULL);
	CASSERT(certData != NULL);	
	CASSERT(kcItem != NULL);
	CASSERT(goodCert != NULL);
	
	*goodCert = false; 
	
	/* how big is the cert? */
	ortn = KCGetData (kcItem, 0,  NULL, &certSize);
	if(ortn != noErr) {
		errorLog1("addCertData: KCGetData(1) returned %d\n", ortn);
		return ortn;
	}

	/* Allocate the buffer. */
	srtn = stSetUpCssmData(certData, certSize);
	if(srtn) {
		return sslErrToOsStatus(srtn);
	}
	
	/* Get the data. */
	ortn = KCGetData (kcItem, certSize, certData->Data, &certSize);
	if(ortn) {
		errorLog1("addCertData: KCGetData(2) returned %d\n", ortn);
		stFreeCssmData(certData, CSSM_FALSE);
		return ortn;
	}

	/* 
	 * Do actual cert verify, which 
     * KCIsRootCertificate does not do. A failure isn't
     * fatal; we just don't add the cert to the array in
     * that case.
     *
     * FIXME - we assume here that our common cspHand can
     * do this cert verify; if not, we have some API work to 
     * do (to let the caller specify which CSP to use with 
     * trusted certs).
     */
	if(!sslVerifyCert(ctx,	
			certData,
			certData,
			ctx->cspHand,
			&subjectExpired)) {			
		dprintf0("addCertData: cert does not self-verify!\n");
		stFreeCssmData(certData, CSSM_FALSE);
		return noErr;
	}
	
	/* Add this cert's subject name to (poss. existing) acceptableDNList */
	dnData = sslGetCertSubjectName(ctx, certData);
	if(dnData) {
		DNListElem *dn = sslMalloc(sizeof(DNListElem));
		if(dn == NULL) {
			return memFullErr;
		}
		dn->next = ctx->acceptableDNList;
		ctx->acceptableDNList = dn;
		
		/* move actual data to dn; free the CSSM_DATA struct (must be
		 * via CSSM_Free()!) */
		CSSM_TO_SSLBUF(dnData, &dn->derDN);
		sslFree(dnData);
	}

	*goodCert = true;
	return noErr;
}

/*
 * Given a newly encountered root cert (obtained from a peer's cert chain),
 * add it to newRootCertKc if the user so allows, and if so, add it to 
 * trustedCerts.
 */
SSLErr
sslAddNewRoot(
	SSLContext			*ctx, 
	const CSSM_DATA_PTR	rootCert)
{
	KCRef			defaultKc;
	Boolean			bDefaultKcExists;
	KCItemRef		certRef = NULL;
	OSStatus		ortn;
	CSSM_DATA_PTR	newTrustee;
	SSLErr			serr;
	
	CASSERT(ctx != NULL);
	CASSERT(rootCert != NULL);
	CASSERT(ctx->newRootCertKc != NULL);	/* caller verifies this */
	
	/*
	 * Get default KC, temporarily set new default.
	 */
	ortn = KCGetDefaultKeychain(&defaultKc);
	if(ortn) {
		bDefaultKcExists = false;
	}
	else {
		bDefaultKcExists = true;
	}
	ortn = KCSetDefaultKeychain(ctx->newRootCertKc);
	if(ortn) {
		errorLog1("sslAddNewRoot: KCSetDefaultKeychain returned %d\n", ortn);
		return SSLUnknownRootCert;
	}

	/*	
	 * Add cert to newRootCertKc. This may well fail due to user
	 * interaction ("Do you want to add this root cert...?").
	 */
	ortn = KCAddX509Certificate(rootCert->Data, rootCert->Length, &certRef);
	
	/* restore default KC in any case */
	if(bDefaultKcExists) {
		KCSetDefaultKeychain(defaultKc);
	}
	if(ortn) {
		dprintf1("sslAddNewRoot: KCAddX509Certificate returned %d\n", ortn);
		return SSLUnknownRootCert;
	}

	/* 
	 * OK, user accepted new root. Now add to our private stash of 
	 * trusted roots. Realloc the whole pile... 
	 */
	ctx->trustedCerts = (CSSM_DATA_PTR)sslRealloc(ctx->trustedCerts,
		(ctx->numTrustedCerts * sizeof(CSSM_DATA)),
		((ctx->numTrustedCerts + 1) * sizeof(CSSM_DATA)));
	if(ctx->trustedCerts == NULL) {
		return SSLMemoryErr;
	}
	
	/* Now add a copy of the new root. */
	newTrustee = &ctx->trustedCerts[ctx->numTrustedCerts];
	newTrustee->Data = NULL;
	newTrustee->Length = 0;
	serr = stSetUpCssmData(newTrustee, rootCert->Length);
	if(serr) {
		return serr;
	}
	BlockMove(rootCert->Data, newTrustee->Data, rootCert->Length);
	(ctx->numTrustedCerts)++;
	return SSLNoErr;
}

#endif	/* ST_KEYCHAIN_ENABLE */


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

	Written by:	Doug Mitchell

	Copyright: (c) 1999 by Apple Computer, Inc., all rights reserved.

*/

#include "ssl.h"
#include "sslContext.h"
#include "sslMemory.h"
#include "appleCdsa.h"
#include "sslDebug.h"
#include "sslKeychain.h"
#include "sslUtils.h"
#include <string.h>
#include <assert.h>
#include <CoreServices/../Frameworks/CarbonCore.framework/Headers/MacErrors.h>
#include <Security/cssm.h>
/* these are to be replaced by Security/Security.h */
#include <Security/SecCertificate.h>
#include <Security/SecKeychainItem.h>
#include <Security/SecKeychain.h>
#include <Security/SecIdentity.h>
#include <Security/SecIdentitySearch.h>
#include <Security/SecKey.h>

#if		ST_MANAGES_TRUSTED_ROOTS
static OSStatus
addCertData(
	SSLContext		*ctx,
	KCItemRef		kcItem,
	CSSM_DATA_PTR	certData,
	Boolean			*goodCert);		/* RETURNED */
#endif	/* ST_MANAGES_TRUSTED_ROOTS */

/*
 * Given an array of certs (as SecIdentityRefs, specified by caller
 * in SSLSetCertificate or SSLSetEncryptionCertificate) and a 
 * destination SSLCertificate:
 *
 * -- free destCerts if we have any
 * -- Get raw cert data, convert to array of SSLCertificates in *destCert 
 * -- validate cert chain
 * -- get pub, priv keys from certRef[0], store in *pubKey, *privKey
 */
 
/* Convert a SecCertificateRef to an SSLCertificate * */
static OSStatus secCertToSslCert(
	SSLContext			*ctx,
	SecCertificateRef 	certRef,
	SSLCertificate		**sslCert)
{
	CSSM_DATA		certData;		// struct is transient, referent owned by 
									//   Sec layer
	OSStatus		ortn;
	SSLCertificate	*thisSslCert = NULL;
	
	ortn = SecCertificateGetData(certRef, &certData);
	if(ortn) {
		sslErrorLog("SecCertificateGetData() returned %d\n", (int)ortn);
		return ortn;
	}
	
	thisSslCert = (SSLCertificate *)sslMalloc(sizeof(SSLCertificate));
	if(thisSslCert == NULL) {
		return memFullErr;
	}
	if(SSLAllocBuffer(thisSslCert->derCert, certData.Length, 
			ctx)) {
		return memFullErr;
	}
	memcpy(thisSslCert->derCert.data, certData.Data, certData.Length);
	thisSslCert->derCert.length = certData.Length;
	*sslCert = thisSslCert;
	return noErr;
}

OSStatus 
parseIncomingCerts(
	SSLContext		*ctx,
	CFArrayRef		certs,
	SSLCertificate	**destCert,		/* &ctx->{localCert,encryptCert} */
	CSSM_KEY_PTR	*pubKey,		/* &ctx->signingPubKey, etc. */
	CSSM_KEY_PTR	*privKey,		/* &ctx->signingPrivKey, etc. */
	CSSM_CSP_HANDLE	*cspHand		/* &ctx->signingKeyCsp, etc. */
	#if		ST_KC_KEYS_NEED_REF
	,
	SecKeychainRef	*privKeyRef)	/* &ctx->signingKeyRef, etc. */
	#else
	)
	#endif	/* ST_KC_KEYS_NEED_REF */
{
	CFIndex				numCerts;
	CFIndex				cert;
	SSLCertificate		*certChain = NULL;
	SSLCertificate		*thisSslCert;
	SecKeychainRef		kcRef;
	OSStatus			ortn;
	SecIdentityRef 		identity;
	SecCertificateRef	certRef;
	SecKeyRef			keyRef;
	CSSM_DATA			certData;
	CSSM_CL_HANDLE		clHand;		// carefully derive from a SecCertificateRef
	CSSM_RETURN			crtn;
	
	assert(ctx != NULL);
	assert(destCert != NULL);		/* though its referent may be NULL */
	assert(pubKey != NULL);
	assert(privKey != NULL);
	assert(cspHand != NULL);
	
	sslDeleteCertificateChain(*destCert, ctx);
	*destCert = NULL;
	*pubKey   = NULL;
	*privKey  = NULL;
	*cspHand  = 0;
	
	if(certs == NULL) {
		sslErrorLog("parseIncomingCerts: NULL incoming cert array\n");
		return errSSLBadCert;
	}
	numCerts = CFArrayGetCount(certs);
	if(numCerts == 0) {
		sslErrorLog("parseIncomingCerts: empty incoming cert array\n");
		return errSSLBadCert;
	}
	
	/* 
	 * Certs[0] is an SecIdentityRef from which we extract subject cert,
	 * privKey, pubKey, and cspHand.
	 *
	 * 1. ensure the first element is a SecIdentityRef.
	 */
	identity = (SecIdentityRef)CFArrayGetValueAtIndex(certs, 0);
	if(identity == NULL) {
		sslErrorLog("parseIncomingCerts: bad cert array (1)\n");
		return paramErr;
	}	
	if(CFGetTypeID(identity) != SecIdentityGetTypeID()) {
		sslErrorLog("parseIncomingCerts: bad cert array (2)\n");
		return paramErr;
	}
	
	/* 
	 * 2. Extract cert, keys, CSP handle and convert to local format. 
	 */
	ortn = SecIdentityCopyCertificate(identity, &certRef);
	if(ortn) {
		sslErrorLog("parseIncomingCerts: bad cert array (3)\n");
		return ortn;
	}
	ortn = secCertToSslCert(ctx, certRef, &thisSslCert);
	if(ortn) {
		sslErrorLog("parseIncomingCerts: bad cert array (4)\n");
		return ortn;
	}
	/* enqueue onto head of cert chain */
	thisSslCert->next = certChain;
	certChain = thisSslCert;

	/* fetch private key from identity */
	ortn = SecIdentityCopyPrivateKey(identity, &keyRef);
	if(ortn) {
		sslErrorLog("parseIncomingCerts: SecIdentityCopyPrivateKey err %d\n",
			(int)ortn);
		return ortn;
	}
	ortn = SecKeyGetCSSMKey(keyRef, (const CSSM_KEY **)privKey);
	if(ortn) {
		sslErrorLog("parseIncomingCerts: SecKeyGetCSSMKey err %d\n",
			(int)ortn);
		return ortn;
	}
	/* FIXME = release keyRef? */
	
	/* obtain public key from cert */
	ortn = SecCertificateGetCLHandle(certRef, &clHand);
	if(ortn) {
		sslErrorLog("parseIncomingCerts: SecCertificateGetCLHandle err %d\n",
			(int)ortn);
		return ortn;
	}
	certData.Data = thisSslCert->derCert.data;
	certData.Length = thisSslCert->derCert.length;
	crtn = CSSM_CL_CertGetKeyInfo(clHand, &certData, pubKey);
	if(crtn) {
		sslErrorLog("parseIncomingCerts: CSSM_CL_CertGetKeyInfo err\n");
		return (OSStatus)crtn;
	}
	
	/* obtain keychain from key, CSP handle from keychain */
	ortn = SecKeychainItemCopyKeychain((SecKeychainItemRef)keyRef, &kcRef);
	if(ortn) {
		sslErrorLog("parseIncomingCerts: SecKeychainItemCopyKeychain err %d\n",
			(int)ortn);
		return ortn;
	}
	ortn = SecKeychainGetCSPHandle(kcRef, cspHand);
	if(ortn) {
		sslErrorLog("parseIncomingCerts: SecKeychainGetCSPHandle err %d\n",
			(int)ortn);
		return ortn;
	}
	
	/* OK, that's the subject cert. Fetch optional remaining certs. */
	/* 
	 * Convert: CFArray of SecCertificateRefs --> chain of SSLCertificates. 
	 * Incoming certs have root last; SSLCertificate chain has root
	 * first.
	 */
	for(cert=1; cert<numCerts; cert++) {
		certRef = (SecCertificateRef)CFArrayGetValueAtIndex(certs, cert);
		if(certRef == NULL) {
			sslErrorLog("parseIncomingCerts: bad cert array (5)\n");
			return paramErr;
		}	
		if(CFGetTypeID(certRef) != SecCertificateGetTypeID()) {
			sslErrorLog("parseIncomingCerts: bad cert array (6)\n");
			return paramErr;
		}
		
		/* Extract cert, convert to local format. 
		*/
		ortn = secCertToSslCert(ctx, certRef, &thisSslCert);
		if(ortn) {
			sslErrorLog("parseIncomingCerts: bad cert array (7)\n");
			return ortn;
		}
		/* enqueue onto head of cert chain */
		thisSslCert->next = certChain;
		certChain = thisSslCert;
	}
	
	/* validate the whole mess, skipping host name verify */
	ortn = sslVerifyCertChain(ctx, *certChain, false);
	if(ortn) {
		goto errOut;
	}
		
	/* SUCCESS */ 
	*destCert = certChain;
	return noErr;
	
errOut:
	/* free certChain, everything in it, other vars, return ortn */
	sslDeleteCertificateChain(certChain, ctx);
	/* FIXME - anything else? */
	return ortn;
}

/*
 * Add Apple built-in root certs to ctx->trustedCerts.
 */
OSStatus addBuiltInCerts	(SSLContextRef		ctx)
{
	#if		ST_MANAGES_TRUSTED_ROOTS
	OSStatus 			ortn;
	KCRef				kc = nil;
	
	ortn = KCDispatch(kKCGetRootCertificateKeychain, &kc);
	if(ortn) {
		sslErrorLog("KCDispatch(kKCGetRootCertificateKeychain) returned %d\n", 
			ortn);
		return ortn;
	}
	return parseTrustedKeychain(ctx, kc);
	#else
	/* nothing for now */
	return noErr;
	#endif	/* ST_MANAGES_TRUSTED_ROOTS */
}

#if		ST_MANAGES_TRUSTED_ROOTS

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
	
	assert(ctx != NULL);
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
			sslErrorLog("parseTrustedKeychains: KCFindX509Certificates returned %d\n",
				ortn);
			return ortn;
	}
	if(kcCerts == NULL) {
		sslErrorLog("parseTrustedKeychains: no certs in KC\n");
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
			sslErrorLog("parseTrustedKeychains: CF error 1\n");
			ortn = errSSLInternal;
			goto errOut;
		}
		if(!KCIsRootCertificate(kcItem)) {
			/* not root, OK, skip to next cert */
			sslErrorLog("parseTrustedKeychains: cert %d NOT ROOT\n",
					certDex);
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
 * Given a (supposedly) root cert as a KCItemRef:
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
	CSSM_BOOL		subjectExpired;
	
	assert(ctx != NULL);
	assert(certData != NULL);	
	assert(kcItem != NULL);
	assert(goodCert != NULL);
	
	*goodCert = false; 
	
	/* how big is the cert? */
	ortn = KCGetData (kcItem, 0,  NULL, &certSize);
	if(ortn != noErr) {
		sslErrorLog("addCertData: KCGetData(1) returned %d\n", ortn);
		return ortn;
	}

	/* Allocate the buffer. */
	ortn = stSetUpCssmData(certData, certSize);
	if(ortn) {
		return ortn;
	}
	
	/* Get the data. */
	ortn = KCGetData (kcItem, certSize, certData->Data, &certSize);
	if(ortn) {
		sslErrorLog("addCertData: KCGetData(2) returned %d\n", ortn);
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
		sslErrorLog("addCertData: cert does not self-verify!\n");
		stFreeCssmData(certData, CSSM_FALSE);
		return noErr;
	}
	
	/* FIXME - needs update for MANAGES_TRUSTED_ROOTS */
	/* Add this cert's subject name to (poss. existing) acceptableDNList */
	CSSM_DATA_PTR dnData = sslGetCertSubjectName(ctx, certData);
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
OSStatus
sslAddNewRoot(
	SSLContext			*ctx, 
	const CSSM_DATA_PTR	rootCert)
{
	KCRef			defaultKc;
	Boolean			bDefaultKcExists;
	KCItemRef		certRef = NULL;
	OSStatus		ortn;
	CSSM_DATA_PTR	newTrustee;
	OSStatus			serr;
	
	assert(ctx != NULL);
	assert(rootCert != NULL);
	assert(ctx->newRootCertKc != NULL);	/* caller verifies this */
	
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
		sslErrorLog("sslAddNewRoot: KCSetDefaultKeychain returned %d\n", ortn);
		return errSSLUnknownRootCert;
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
		sslErrorLog("sslAddNewRoot: KCAddX509Certificate returned %d\n", ortn);
		return errSSLUnknownRootCert;
	}

	/* 
	 * OK, user accepted new root. Now add to our private stash of 
	 * trusted roots. Realloc the whole pile... 
	 */
	ctx->trustedCerts = (CSSM_DATA_PTR)sslRealloc(ctx->trustedCerts,
		(ctx->numTrustedCerts * sizeof(CSSM_DATA)),
		((ctx->numTrustedCerts + 1) * sizeof(CSSM_DATA)));
	if(ctx->trustedCerts == NULL) {
		return memFullErr;
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
	return noErr;
}

#endif	/* ST_MANAGES_TRUSTED_ROOTS */


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

	Written by:	Doug Mitchell, based on Netscape SSLRef 3.0

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

#if		ST_KEYCHAIN_ENABLE && ST_MANAGES_TRUSTED_ROOTS
static OSStatus
addCertData(
	SSLContext		*ctx,
	KCItemRef		kcItem,
	CSSM_DATA_PTR	certData,
	Boolean			*goodCert);		/* RETURNED */
#endif	/* ST_KEYCHAIN_ENABLE && ST_MANAGES_TRUSTED_ROOTS */

#if		(ST_SERVER_MODE_ENABLE || ST_CLIENT_AUTHENTICATION)

#if		ST_FAKE_KEYCHAIN
/*
 * Routines which will be replaced by SecKeychainAPI. 
 */
 
/*
 * Given a DLDB, find the first private key in the DB. It's the application's
 * responsibility to ensure that there is only one private key. The returned
 * PrintName attribute will be used to search for an associated cert using
 * TBD.
 *
 * Caller must free returned key and PrintName.
 */
static OSStatus 
findPrivateKeyInDb(
	SSLContext			*ctx,
	CSSM_DL_DB_HANDLE	dlDbHand,
	CSSM_KEY_PTR		*privKey,		// mallocd and RETURNED
	CSSM_DATA			*printName)		// referent mallocd and RETURNED
{
	CSSM_QUERY						query;
	CSSM_DB_UNIQUE_RECORD_PTR		record = NULL;
	CSSM_RETURN						crtn;
	CSSM_HANDLE 					resultHand;
	CSSM_DB_RECORD_ATTRIBUTE_DATA	recordAttrs;
	CSSM_DB_ATTRIBUTE_DATA			theAttr;
	CSSM_DB_ATTRIBUTE_INFO_PTR		attrInfo = &theAttr.Info;
	CSSM_DATA						theData = {0, NULL};
	
	/* search by record type, no predicates (though we do want the PrintName
	 * attr returned). */
	query.RecordType = CSSM_DL_DB_RECORD_PRIVATE_KEY;
	query.Conjunctive = CSSM_DB_NONE;
	query.NumSelectionPredicates = 0;
	query.SelectionPredicate = NULL;	
	query.QueryLimits.TimeLimit = 0;	// FIXME - meaningful?
	query.QueryLimits.SizeLimit = 1;	// FIXME - meaningful?
	query.QueryFlags = CSSM_QUERY_RETURN_DATA;	// FIXME - used?

	recordAttrs.DataRecordType = CSSM_DL_DB_RECORD_PRIVATE_KEY;
	recordAttrs.SemanticInformation = 0;
	recordAttrs.NumberOfAttributes = 1;
	recordAttrs.AttributeData = &theAttr;
	
	attrInfo->AttributeNameFormat = CSSM_DB_ATTRIBUTE_NAME_AS_STRING;
	attrInfo->Label.AttributeName = "PrintName";
	attrInfo->AttributeFormat = CSSM_DB_ATTRIBUTE_FORMAT_BLOB;
	
	theAttr.NumberOfValues = 1;
	theAttr.Value = NULL;			
		
	crtn = CSSM_DL_DataGetFirst(dlDbHand,
		&query,
		&resultHand,
		&recordAttrs,
		&theData,
		&record);
	/* terminate query only on success */
	if(crtn == CSSM_OK) {
		CSSM_DL_DataAbortQuery(dlDbHand, resultHand);
		*privKey = (CSSM_KEY_PTR)theData.Data;
		/*
		 * Both the struct and the referent are mallocd by DL. Give our
		 * caller the referent; free the struct. 
		 */
		*printName = *theAttr.Value;
		stAppFree(theAttr.Value, NULL);
		return noErr;
	}
	else {
		stPrintCdsaError("CSSM_DL_DataGetFirst", crtn);
		errorLog0("findCertInDb: cert not found\n");
		return errSSLBadCert;
	}
}

static OSStatus
findCertInDb(
	SSLContext			*ctx,
	CSSM_DL_DB_HANDLE	dlDbHand,
	const CSSM_DATA		*printName,		// obtained from findPrivateKeyInDb
	CSSM_DATA			*certData)		// referent mallocd and RETURNED
{
	CSSM_QUERY						query;
	CSSM_SELECTION_PREDICATE		predicate;
	CSSM_DB_UNIQUE_RECORD_PTR		record = NULL;
	CSSM_RETURN						crtn;
	CSSM_HANDLE 					resultHand;
	
	predicate.DbOperator = CSSM_DB_EQUAL;	
	predicate.Attribute.Info.AttributeNameFormat = 
		CSSM_DB_ATTRIBUTE_NAME_AS_STRING;
	predicate.Attribute.Info.Label.AttributeName = "PrintName";
	predicate.Attribute.Info.AttributeFormat = CSSM_DB_ATTRIBUTE_FORMAT_BLOB;
	/* hope this const_cast is OK */
	predicate.Attribute.Value = (CSSM_DATA_PTR)printName;
	predicate.Attribute.NumberOfValues = 1;

	query.RecordType = CSSM_DL_DB_RECORD_X509_CERTIFICATE;
	query.Conjunctive = CSSM_DB_NONE;
	query.NumSelectionPredicates = 1;
	query.SelectionPredicate = &predicate;
	query.QueryLimits.TimeLimit = 0;	// FIXME - meaningful?
	query.QueryLimits.SizeLimit = 1;	// FIXME - meaningful?
	query.QueryFlags = 0;				// FIXME - used?
	
	crtn = CSSM_DL_DataGetFirst(dlDbHand,
		&query,
		&resultHand,
		NULL,				// no attrs returned
		certData,
		&record);
	/* terminate query only on success */
	if(crtn == CSSM_OK) {
		CSSM_DL_DataAbortQuery(dlDbHand, resultHand);
		return noErr;
	}
	else {
		stPrintCdsaError("CSSM_DL_DataGetFirst", crtn);
		errorLog0("findCertInDb: cert not found\n");
		return errSSLBadCert;
	}
}


#endif	/* ST_FAKE_KEYCHAIN */
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
 
#if		ST_FAKE_KEYCHAIN
/*
 * In this incarnation, the certs array actually holds one pointer to a 
 * CSSM_DL_DB_HANDLE. In that DL/DB is exactly one private key; that's
 * our privKey. We use the KeyLabel of that key to look up a cert with  
 * the same label. We get the public key from the cert. Other certs and 
 * public keys in the DL/DB are ignored.
 */
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
	CSSM_DL_DB_HANDLE_PTR dlDbHand = NULL;
	CFIndex			numCerts;
	CSSM_KEY_PTR	lookupPriv = NULL;
	CSSM_DATA		lookupLabel = {0, NULL};
	CSSM_DATA		lookupCert = {0, NULL};
	OSStatus 		ortn;
	SSLCertificate	*certChain = NULL;
	SSLCertificate	*thisSslCert;
	SSLErr			srtn;
	CSSM_CSP_HANDLE	dummyCsp;
	
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
		dprintf0("parseIncomingCerts: NULL incoming cert (DLDB) array\n");
		return errSSLBadCert;
	}
	numCerts = CFArrayGetCount(certs);
	if(numCerts != 1) {
		dprintf0("parseIncomingCerts: empty incoming cert (DLDB) array\n");
		return errSSLBadCert;
	}
	dlDbHand = (CSSM_DL_DB_HANDLE_PTR)CFArrayGetValueAtIndex(certs, 0);
	if(dlDbHand == NULL) {
		errorLog0("parseIncomingCerts: bad cert (DLDB) array\n");
		return paramErr;
	}	

	/* get private key - app has to ensure there is only one (for now) */
	ortn = findPrivateKeyInDb(ctx, *dlDbHand, &lookupPriv, &lookupLabel);
	if(ortn) {
		errorLog0("parseIncomingCerts: no private key\n");
		return ortn;
	}
	assert(lookupPriv->KeyHeader.BlobType == CSSM_KEYBLOB_REFERENCE);
	assert(lookupPriv->KeyHeader.KeyClass == CSSM_KEYCLASS_PRIVATE_KEY);
	
	/* get associated cert */
	ortn = findCertInDb(ctx, *dlDbHand, &lookupLabel, &lookupCert);
	if(ortn) {
		errorLog0("parseIncomingCerts: no cert\n");
		return ortn;
	}
	sslFree(lookupLabel.Data);
	assert(lookupCert.Length > 100);			// quickie check 
	
	/* 
	 * Cook up an SSLCertificate and its associated SSLBuffer.
	 */
	thisSslCert = sslMalloc(sizeof(SSLCertificate));
	if(thisSslCert == NULL) {
		return memFullErr;
	}
	if(SSLAllocBuffer(&thisSslCert->derCert, lookupCert.Length, &ctx->sysCtx)) {
		return memFullErr;
	}
	
	/* copy cert data mallocd by DL */
	memmove(thisSslCert->derCert.data, lookupCert.Data, lookupCert.Length);
	sslFree(lookupCert.Data);
	
	/* enqueue onto head of cert chain */
	thisSslCert->next = certChain;
	certChain = thisSslCert;

	/* TBD - we might fetch other certs from CFArrayRef certs here and enqueue 
	 * them on certChain */
	 
	/* now the public key of the first cert, from CL */
	srtn = sslPubKeyFromCert(ctx, 
		&certChain->derCert, 
		pubKey,
		&dummyCsp);
	if(srtn) {
		errorLog1("sslPubKeyFromCert returned %d\n", srtn);
		ortn = sslErrToOsStatus(srtn);
		goto errOut;
	}
	assert((*pubKey)->KeyHeader.BlobType == CSSM_KEYBLOB_RAW);
	assert((*pubKey)->KeyHeader.KeyClass == CSSM_KEYCLASS_PUBLIC_KEY);
	
	/*
	 * NOTE: as of 2/7/02, the size of the extracted public key will NOT
	 * always equal the size of the private key. Non-byte-aligned key sizes 
	 * for RSA keys result in the extracted public key's size to be rounded
	 * UP to the next byte boundary. 
	 */
	assert((*pubKey)->KeyHeader.LogicalKeySizeInBits == 
		  ((lookupPriv->KeyHeader.LogicalKeySizeInBits + 7) & ~7));
	
	/* SUCCESS */ 
	*destCert = certChain;
	*privKey = lookupPriv;
	
	/* we get this at context create time */
	assert(ctx->cspDlHand != 0);
	*cspHand = ctx->cspDlHand;
	*privKeyRef = NULL;				// not used 
	return noErr;
	
errOut:
	/* free certChain, everything in it, other vars, return ortn */
	sslDeleteCertificateChain(certChain, ctx);
	if(lookupPriv != NULL) {
		sslFreeKey(ctx->cspDlHand, &lookupPriv, NULL);
	}
	return ortn;
}

#else	/* !ST_FAKE_KEYCHAIN */

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
		errorLog1("SecCertificateGetData() returned %d\n", (int)ortn);
		return ortn;
	}
	
	thisSslCert = sslMalloc(sizeof(SSLCertificate));
	if(thisSslCert == NULL) {
		return memFullErr;
	}
	if(SSLAllocBuffer(&thisSslCert->derCert, certData.Length, 
			&ctx->sysCtx)) {
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
	SSLErr				srtn;
	SecIdentityRef 		identity;
	SecCertificateRef	certRef;
	SecKeyRef			keyRef;
	CSSM_DATA			certData;
	CSSM_CL_HANDLE		clHand;		// carefully derive from a SecCertificateRef
	CSSM_RETURN			crtn;
	
	CASSERT(ctx != NULL);
	CASSERT(destCert != NULL);		/* though its referent may be NULL */
	CASSERT(pubKey != NULL);
	CASSERT(privKey != NULL);
	CASSERT(cspHand != NULL);
	
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
	 * Certs[0] is an SecIdentityRef from which we extract subject cert,
	 * privKey, pubKey, and cspHand.
	 *
	 * 1. ensure the first element is a SecIdentityRef.
	 */
	identity = (SecIdentityRef)CFArrayGetValueAtIndex(certs, 0);
	if(identity == NULL) {
		errorLog0("parseIncomingCerts: bad cert array (1)\n");
		return paramErr;
	}	
	if(CFGetTypeID(identity) != SecIdentityGetTypeID()) {
		errorLog0("parseIncomingCerts: bad cert array (2)\n");
		return paramErr;
	}
	
	/* 
	 * 2. Extract cert, keys, CSP handle and convert to local format. 
	 */
	ortn = SecIdentityCopyCertificate(identity, &certRef);
	if(ortn) {
		errorLog0("parseIncomingCerts: bad cert array (3)\n");
		return ortn;
	}
	ortn = secCertToSslCert(ctx, certRef, &thisSslCert);
	if(ortn) {
		errorLog0("parseIncomingCerts: bad cert array (4)\n");
		return ortn;
	}
	/* enqueue onto head of cert chain */
	thisSslCert->next = certChain;
	certChain = thisSslCert;

	/* fetch private key from identity */
	ortn = SecIdentityCopyPrivateKey(identity, &keyRef);
	if(ortn) {
		errorLog1("parseIncomingCerts: SecIdentityCopyPrivateKey err %d\n",
			(int)ortn);
		return ortn;
	}
	ortn = SecKeyGetCSSMKey(keyRef, (const CSSM_KEY **)privKey);
	if(ortn) {
		errorLog1("parseIncomingCerts: SecKeyGetCSSMKey err %d\n",
			(int)ortn);
		return ortn;
	}
	/* FIXME = release keyRef? */
	
	/* obtain public key from cert */
	ortn = SecCertificateGetCLHandle(certRef, &clHand);
	if(ortn) {
		errorLog1("parseIncomingCerts: SecCertificateGetCLHandle err %d\n",
			(int)ortn);
		return ortn;
	}
	certData.Data = thisSslCert->derCert.data;
	certData.Length = thisSslCert->derCert.length;
	crtn = CSSM_CL_CertGetKeyInfo(clHand, &certData, pubKey);
	if(crtn) {
		errorLog0("parseIncomingCerts: CSSM_CL_CertGetKeyInfo err\n");
		return (OSStatus)crtn;
	}
	
	#if		ST_FAKE_GET_CSPDL_HANDLE
	/* we get this at context create time until SecKeychainGetCSPHandle
	 * is working */
	assert(ctx->cspDlHand != 0);
	*cspHand = ctx->cspDlHand;
	#else	/* ST_FAKE_GET_CSPDL_HANDLE */
	/* obtain keychain from key, CSP handle from keychain */
	ortn = SecKeychainItemCopyKeychain((SecKeychainItemRef)keyRef, &kcRef);
	if(ortn) {
		errorLog1("parseIncomingCerts: SecKeychainItemCopyKeychain err %d\n",
			(int)ortn);
		return ortn;
	}
	ortn = SecKeychainGetCSPHandle(kcRef, cspHand);
	if(ortn) {
		errorLog1("parseIncomingCerts: SecKeychainGetCSPHandle err %d\n",
			(int)ortn);
		return ortn;
	}
	#endif	/* ST_FAKE_GET_CSPDL_HANDLE */
	
	/* OK, that's the subject cert. Fetch optional remaining certs. */
	/* 
	 * Convert: CFArray of SecCertificateRefs --> chain of SSLCertificates. 
	 * Incoming certs have root last; SSLCertificate chain has root
	 * first.
	 */
	for(cert=1; cert<numCerts; cert++) {
		certRef = (SecCertificateRef)CFArrayGetValueAtIndex(certs, cert);
		if(certRef == NULL) {
			errorLog0("parseIncomingCerts: bad cert array (5)\n");
			return paramErr;
		}	
		if(CFGetTypeID(certRef) != SecCertificateGetTypeID()) {
			errorLog0("parseIncomingCerts: bad cert array (6)\n");
			return paramErr;
		}
		
		/* Extract cert, convert to local format. 
		*/
		ortn = secCertToSslCert(ctx, certRef, &thisSslCert);
		if(ortn) {
			errorLog0("parseIncomingCerts: bad cert array (7)\n");
			return ortn;
		}
		/* enqueue onto head of cert chain */
		thisSslCert->next = certChain;
		certChain = thisSslCert;
	}
	
	/* validate the whole mess */
	srtn = sslVerifyCertChain(ctx, certChain);
	if(srtn) {
		ortn = sslErrToOsStatus(srtn);
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
#endif	/* ST_FAKE_KEYCHAIN */
#endif	/* (ST_SERVER_MODE_ENABLE || ST_CLIENT_AUTHENTICATION) */

/*
 * Add Apple built-in root certs to ctx->trustedCerts.
 */
OSStatus addBuiltInCerts	(SSLContextRef		ctx)
{
	#if		ST_KEYCHAIN_ENABLE && ST_MANAGES_TRUSTED_ROOTS
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
	#endif	/* ST_KEYCHAIN_ENABLE && ST_MANAGES_TRUSTED_ROOTS */
}

#if		ST_KEYCHAIN_ENABLE && ST_MANAGES_TRUSTED_ROOTS

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

#endif	/* ST_KEYCHAIN_ENABLE && ST_MANAGES_TRUSTED_ROOTS */


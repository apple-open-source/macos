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
#include <Security/Security.h>
#include <Security/SecCertificatePriv.h>

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
	if(SSLAllocBuffer(&thisSslCert->derCert, certData.Length, 
			ctx)) {
		return memFullErr;
	}
	memcpy(thisSslCert->derCert.data, certData.Data, certData.Length);
	thisSslCert->derCert.length = certData.Length;
	*sslCert = thisSslCert;
	return noErr;
}

/* 
 * Determine the basic signing algorithm, without the digest, component, of
 * a cert. The returned algorithm will be RSA, DSA, or ECDSA. 
 */
static OSStatus sslCertSignerAlg(
	SecCertificateRef certRef,
	CSSM_ALGORITHMS *signerAlg)
{
	OSStatus ortn;
	CSSM_DATA_PTR fieldPtr;
	CSSM_X509_ALGORITHM_IDENTIFIER *algId;
	CSSM_ALGORITHMS sigAlg;
	
	/* 
	 * Extract the full signature algorithm OID
	 */
	*signerAlg = CSSM_ALGID_NONE;
	ortn = SecCertificateCopyFirstFieldValue(certRef, 
		&CSSMOID_X509V1SignatureAlgorithm,
		&fieldPtr);
	if(ortn) {
		return ortn;
	}
	if(fieldPtr->Length != sizeof(CSSM_X509_ALGORITHM_IDENTIFIER)) {
		sslErrorLog("sslCertSignerAlg() length error\n");
		ortn = errSSLCrypto;
		goto errOut;
	}
	algId = (CSSM_X509_ALGORITHM_IDENTIFIER *)fieldPtr->Data;
	if(!cssmOidToAlg(&algId->algorithm, &sigAlg)) {
		/* Only way this could happen is if we're given a bad cert */
		sslErrorLog("sslCertSignerAlg() bad sigAlg OID\n");
		ortn = paramErr;
		goto errOut;
	}
	
	/*
	 * OK we have the full signature algorithm as a CSSM_ALGORITHMS.
	 * Extract the core signature alg.
	 */
	switch(sigAlg) {
		case CSSM_ALGID_RSA:
		case CSSM_ALGID_MD2WithRSA:
		case CSSM_ALGID_MD5WithRSA:
		case CSSM_ALGID_SHA1WithRSA:
		case CSSM_ALGID_SHA224WithRSA:
		case CSSM_ALGID_SHA256WithRSA:
		case CSSM_ALGID_SHA384WithRSA:
		case CSSM_ALGID_SHA512WithRSA:
			*signerAlg = CSSM_ALGID_RSA;
			break;
		case CSSM_ALGID_SHA1WithECDSA:
		case CSSM_ALGID_SHA224WithECDSA:
		case CSSM_ALGID_SHA256WithECDSA:
		case CSSM_ALGID_SHA384WithECDSA:
		case CSSM_ALGID_SHA512WithECDSA:
		case CSSM_ALGID_ECDSA:
		case CSSM_ALGID_ECDSA_SPECIFIED:
			*signerAlg = CSSM_ALGID_ECDSA;
			break;
		case CSSM_ALGID_DSA:
		case CSSM_ALGID_SHA1WithDSA:
			*signerAlg = CSSM_ALGID_DSA;
			break;
		default:
			sslErrorLog("sslCertSignerAlg() unknown sigAlg\n");
			ortn = paramErr;
			break;
	}
errOut:
	SecCertificateReleaseFirstFieldValue(certRef, 
		&CSSMOID_X509V1SignatureAlgorithm, fieldPtr);
	return ortn;
}

OSStatus 
parseIncomingCerts(
	SSLContext		*ctx,
	CFArrayRef		certs,
	SSLCertificate	**destCert,		/* &ctx->{localCert,encryptCert} */
	CSSM_KEY_PTR	*pubKey,		/* &ctx->signingPubKey, etc. */
	SecKeyRef		*privKeyRef,	/* &ctx->signingPrivKeyRef, etc. */
	CSSM_ALGORITHMS	*signerAlg)		/* optional */
{
	CFIndex				numCerts;
	CFIndex				cert;
	SSLCertificate		*certChain = NULL;
	SSLCertificate		*thisSslCert;
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
	assert(privKeyRef != NULL);
	
	sslDeleteCertificateChain(*destCert, ctx);
	*destCert = NULL;
	*pubKey   = NULL;
	*privKeyRef = NULL;
	
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
	 * privKeyRef, pubKey.
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
	 * 2. Extract cert, keys and convert to local format. 
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

	if(signerAlg != NULL) {
		ortn = sslCertSignerAlg(certRef, signerAlg);
		if(ortn) {
			return ortn;
		}
	}
	
	/* fetch private key from identity */
	ortn = SecIdentityCopyPrivateKey(identity, &keyRef);
	if(ortn) {
		sslErrorLog("parseIncomingCerts: SecIdentityCopyPrivateKey err %d\n",
			(int)ortn);
		return ortn;
	}
	*privKeyRef = keyRef;
	
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
	
	/* SUCCESS */ 
	*destCert = certChain;
	return noErr;
	
	/* free certChain, everything in it, other vars, return ortn */
	sslDeleteCertificateChain(certChain, ctx);
	/* FIXME - anything else? */
	return ortn;
}



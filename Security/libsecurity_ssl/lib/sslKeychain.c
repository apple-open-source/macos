/*
 * Copyright (c) 1999-2001,2005-2008,2010-2012 Apple Inc. All Rights Reserved.
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

/*
 * sslKeychain.c - Apple Keychain routines
 */

#include "ssl.h"
#include "sslContext.h"
#include "sslMemory.h"

#include "sslCrypto.h"
#ifdef USE_CDSA_CRYPTO
#include <Security/Security.h>
#else
#include <Security/SecBase.h>
#include <Security/SecCertificate.h>
#include <Security/SecIdentity.h>
#include <Security/SecPolicy.h>
#include <Security/SecTrust.h>
#endif /* !USE_CDSA_CRYPTO */
#include <Security/SecInternal.h>

#include "sslDebug.h"
#include "sslKeychain.h"
#include "sslUtils.h"
#include <string.h>
#include <assert.h>


#ifdef USE_SSLCERTIFICATE

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
	CSSM_KEY_PTR        *pubKey;
	SecKeyRef           *privKeyRef;

	assert(ctx != NULL);
	assert(destCert != NULL);		/* though its referent may be NULL */
	assert(sslPubKey != NULL);
	assert(sslPrivKeyRef != NULL);

	pubKey = &sslPubKey->key;
	privKeyRef = &sslPrivKey->key;

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

#else /* !USE_SSLCERTIFICATE */

OSStatus
parseIncomingCerts(
	SSLContext			*ctx,
	CFArrayRef			certs,
	CFArrayRef			*destCertChain,	/* &ctx->{localCertChain,encryptCertChain} */
	SSLPubKey			**sslPubKey,	/* &ctx->signingPubKey, etc. */
	SSLPrivKey			**sslPrivKey,	/* &ctx->signingPrivKeyRef, etc. */
	CFIndex				*signerAlg)		/* optional */
{
	OSStatus			ortn;
	CFIndex				ix, numCerts;
	SecIdentityRef 		identity;
	CFMutableArrayRef	certChain = NULL;	/* Retained */
	SecCertificateRef	leafCert = NULL;	/* Retained */
	SecKeyRef			pubKey = NULL;		/* Retained */
	SecKeyRef           privKey = NULL;		/* Retained */
	SecTrustRef         trust = NULL;		/* Retained */
	SecTrustResultType	trustResult;

	assert(ctx != NULL);
	assert(destCertChain != NULL);		/* though its referent may be NULL */
	assert(sslPubKey != NULL);
	assert(sslPrivKey != NULL);

	if (certs == NULL) {
		sslErrorLog("parseIncomingCerts: NULL incoming cert array\n");
		ortn = errSSLBadCert;
		goto errOut;
	}
	numCerts = CFArrayGetCount(certs);
	if (numCerts == 0) {
		sslErrorLog("parseIncomingCerts: empty incoming cert array\n");
		ortn = errSSLBadCert;
		goto errOut;
	}

	/*
	 * Certs[0] is an SecIdentityRef from which we extract subject cert,
	 * privKey, pubKey.
	 *
	 * 1. ensure the first element is a SecIdentityRef.
	 */
	identity = (SecIdentityRef)CFArrayGetValueAtIndex(certs, 0);
	if (identity == NULL) {
		sslErrorLog("parseIncomingCerts: bad cert array (1)\n");
		ortn = paramErr;
		goto errOut;
	}
	if (CFGetTypeID(identity) != SecIdentityGetTypeID()) {
		sslErrorLog("parseIncomingCerts: bad cert array (2)\n");
		ortn = paramErr;
		goto errOut;
	}

	/*
	 * 2. Extract cert, keys and convert to local format.
	 */
	ortn = SecIdentityCopyCertificate(identity, &leafCert);
	if (ortn) {
		sslErrorLog("parseIncomingCerts: bad cert array (3)\n");
		goto errOut;
	}

	/* Fetch private key from identity */
	ortn = SecIdentityCopyPrivateKey(identity, &privKey);
	if (ortn) {
		sslErrorLog("parseIncomingCerts: SecIdentityCopyPrivateKey err %d\n",
			(int)ortn);
		goto errOut;
	}

	/* Convert the input array of SecIdentityRef at the start to an array of
	   all certificates. */
	certChain = CFArrayCreateMutable(kCFAllocatorDefault, numCerts,
		&kCFTypeArrayCallBacks);
	if (!certChain) {
		ortn = memFullErr;
		goto errOut;
	}
	CFArrayAppendValue(certChain, leafCert);
	for (ix = 1; ix < numCerts; ++ix) {
		SecCertificateRef intermediate =
			(SecCertificateRef)CFArrayGetValueAtIndex(certs, ix);
		if (intermediate == NULL) {
			sslErrorLog("parseIncomingCerts: bad cert array (5)\n");
			ortn = paramErr;
			goto errOut;
		}
		if (CFGetTypeID(intermediate) != SecCertificateGetTypeID()) {
			sslErrorLog("parseIncomingCerts: bad cert array (6)\n");
			ortn = paramErr;
			goto errOut;
		}

		CFArrayAppendValue(certChain, intermediate);
	}

	/* Obtain public key from cert */
#if TARGET_OS_IOS
	ortn = SecTrustCreateWithCertificates(certChain, NULL, &trust);
#else
	{
		SecPolicyRef policy = SecPolicyCreateBasicX509();
		ortn = SecTrustCreateWithCertificates(certChain, policy, &trust);
		CFReleaseSafe(policy);
		if (!ortn) {
			/* We are only interested in getting the public key from the leaf
			 * cert here, so for best performance, don't try to build a chain
			 * or search any keychains.
			 */
			CFArrayRef emptyArray = CFArrayCreate(NULL, NULL, 0, NULL);
			(void)SecTrustSetAnchorCertificates(trust, emptyArray);
			(void)SecTrustSetKeychains(trust, emptyArray);
			CFReleaseSafe(emptyArray);
		}
	}
#endif
	if (ortn) {
		sslErrorLog("parseIncomingCerts: SecTrustCreateWithCertificates err %d\n",
			(int)ortn);
		goto errOut;
	}
	ortn = SecTrustEvaluate(trust, &trustResult);
	if (ortn) {
		sslErrorLog("parseIncomingCerts: SecTrustEvaluate err %d\n",
			(int)ortn);
		goto errOut;
	}
	pubKey = SecTrustCopyPublicKey(trust);
	if (pubKey == NULL) {
		sslErrorLog("parseIncomingCerts: SecTrustCopyPublicKey failed\n");
		ortn = -67712; // errSecInvalidKeyRef
		goto errOut;
	}

	/* SUCCESS */
errOut:
	CFReleaseSafe(trust);
	CFReleaseSafe(leafCert);
	CFReleaseSafe(*destCertChain);
    sslFreePubKey(sslPubKey);
    sslFreePrivKey(sslPrivKey);

	if (ortn) {
		CFReleaseSafe(certChain);
		CFReleaseSafe(pubKey);
		CFReleaseSafe(privKey);

		*destCertChain = NULL;
	} else {
		*destCertChain = certChain;
		*sslPubKey = (SSLPubKey*)pubKey;
		*sslPrivKey = (SSLPrivKey*)privKey;
	}

	return ortn;
}
#endif /* !USE_SSLCERTIFICATE */

/*
 * Copyright (c) 1999-2001,2005-2008,2010-2014 Apple Inc. All Rights Reserved.
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
#include <Security/SecBase.h>
#include <Security/SecCertificate.h>
#include <Security/SecCertificatePriv.h>
#include <Security/SecIdentity.h>
#include <Security/SecPolicy.h>
#include <Security/SecTrust.h>
#include "utilities/SecCFRelease.h"

#include "sslDebug.h"
#include "sslKeychain.h"
#include "sslUtils.h"
#include <string.h>
#include <assert.h>


#include <Security/Security.h>
#include <Security/SecKeyPriv.h>
#include <AssertMacros.h>
#include <tls_handshake.h>

#if TARGET_OS_IPHONE
#include <Security/oidsalg.h>
#endif

/* Private Key operations */
static
SecAsn1Oid oidForSSLHash(SSL_HashAlgorithm hash)
{
    switch (hash) {
        case tls_hash_algorithm_SHA1:
            return CSSMOID_SHA1WithRSA;
        case tls_hash_algorithm_SHA256:
            return CSSMOID_SHA256WithRSA;
        case tls_hash_algorithm_SHA384:
            return CSSMOID_SHA384WithRSA;
        default:
            break;
    }
    // Internal error
    assert(0);
    // This guarantee failure down the line
    return CSSMOID_MD5WithRSA;
}

static
int mySSLPrivKeyRSA_sign(void *key, tls_hash_algorithm hash, const uint8_t *plaintext, size_t plaintextLen, uint8_t *sig, size_t *sigLen)
{
    SecKeyRef keyRef = key;

    if(hash == tls_hash_algorithm_None) {
        return SecKeyRawSign(keyRef, kSecPaddingPKCS1, plaintext, plaintextLen, sig, sigLen);
    } else {
        SecAsn1AlgId  algId;
        algId.algorithm = oidForSSLHash(hash);
        return SecKeySignDigest(keyRef, &algId, plaintext, plaintextLen, sig, sigLen);
    }
}

static
int mySSLPrivKeyRSA_decrypt(void *key, const uint8_t *ciphertext, size_t ciphertextLen, uint8_t *plaintext, size_t *plaintextLen)
{
    SecKeyRef keyRef = key;

    return SecKeyDecrypt(keyRef, kSecPaddingPKCS1, ciphertext, ciphertextLen, plaintext, plaintextLen);
}

void sslFreePrivKey(tls_private_key_t *sslPrivKey)
{
    assert(sslPrivKey);

    if(*sslPrivKey) {
        CFReleaseSafe(tls_private_key_get_context(*sslPrivKey));
        tls_private_key_destroy(*sslPrivKey);
        *sslPrivKey = NULL;
    }
}

OSStatus
parseIncomingCerts(
	SSLContext			*ctx,
	CFArrayRef			certs,
	SSLCertificate		**destCertChain, /* &ctx->{localCertChain,encryptCertChain} */
	tls_private_key_t   *sslPrivKey)	 /* &ctx->signingPrivKeyRef, etc. */
{
	OSStatus			ortn;
	CFIndex				ix, numCerts;
	SecIdentityRef 		identity;
	SSLCertificate      *certChain = NULL;	/* Retained */
	SecCertificateRef	leafCert = NULL;	/* Retained */
	SecKeyRef           privKey = NULL;	/* Retained */

	assert(ctx != NULL);
	assert(destCertChain != NULL);		/* though its referent may be NULL */
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

    certChain=sslMalloc(numCerts*sizeof(SSLCertificate));
    if (!certChain) {
        ortn = errSecAllocate;
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
		ortn = errSecParam;
		goto errOut;
	}
	if (CFGetTypeID(identity) != SecIdentityGetTypeID()) {
		sslErrorLog("parseIncomingCerts: bad cert array (2)\n");
		ortn = errSecParam;
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

    certChain[0].derCert.data = (uint8_t *)SecCertificateGetBytePtr(leafCert);
    certChain[0].derCert.length = SecCertificateGetLength(leafCert);
    certChain[0].next = NULL;

	for (ix = 1; ix < numCerts; ++ix) {
		SecCertificateRef intermediate =
			(SecCertificateRef)CFArrayGetValueAtIndex(certs, ix);
		if (intermediate == NULL) {
			sslErrorLog("parseIncomingCerts: bad cert array (5)\n");
			ortn = errSecParam;
			goto errOut;
		}
		if (CFGetTypeID(intermediate) != SecCertificateGetTypeID()) {
			sslErrorLog("parseIncomingCerts: bad cert array (6)\n");
			ortn = errSecParam;
			goto errOut;
		}

        certChain[ix].derCert.data = (uint8_t *)SecCertificateGetBytePtr(intermediate);
        certChain[ix].derCert.length = SecCertificateGetLength(intermediate);
        certChain[ix].next = NULL;
        certChain[ix-1].next = &certChain[ix];

	}

    if(sslPrivKeyGetAlgorithmID(privKey)!=kSecRSAAlgorithmID) {
        ortn = errSecParam;
        goto errOut;
    }

    sslFreePrivKey(sslPrivKey);
    *sslPrivKey = tls_private_key_rsa_create(privKey, SecKeyGetBlockSize(privKey), mySSLPrivKeyRSA_sign, mySSLPrivKeyRSA_decrypt);
    if(*sslPrivKey)
        ortn = errSecSuccess;
    else
        ortn = errSecAllocate;

	/* SUCCESS */
errOut:
	CFReleaseSafe(leafCert);

    sslFree(*destCertChain);

	if (ortn) {
		free(certChain);
		CFReleaseSafe(privKey);
		*destCertChain = NULL;
	} else {
		*destCertChain = certChain;
	}

	return ortn;
}

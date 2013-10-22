/*
 * Copyright (c) 2006-2012 Apple Inc. All Rights Reserved.
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
 * sslCrypto.c - interface between SSL and crypto libraries
 */

#include "sslCrypto.h"

#include "CipherSuite.h"
#include "ssl.h"
#include "sslContext.h"
#include "sslMemory.h"
#include "sslUtils.h"
#include "sslDebug.h"
#include <libDER/DER_CertCrl.h>
#include <libDER/DER_Keys.h>
#include <CoreFoundation/CFString.h>
#include <Security/SecKey.h>
#include <Security/SecKeyPriv.h>
#include <corecrypto/ccdh.h>
#include <corecrypto/ccec.h>
#include <corecrypto/ccrng.h>
#include <Security/SecCertificate.h>
#include <Security/SecPolicy.h>
#include <Security/SecTrust.h>
#include <AssertMacros.h>
#include "utilities/SecCFRelease.h"

#if TARGET_OS_IPHONE
#include <Security/SecKeyInternal.h>
#include <Security/SecRSAKey.h>
#include <Security/SecECKey.h>
#endif

#ifndef	_SSL_KEYCHAIN_H_
#include "sslKeychain.h"
#endif

#if APPLE_DH
#include <libDER/libDER.h>
#include <libDER/DER_Keys.h>
#include <libDER/DER_Encode.h>
#include <libDER/asn1Types.h>
#include <Security/SecRandom.h>
#endif

#include <string.h>
#include <stdlib.h>
#include <assert.h>

#if TARGET_OS_IPHONE
#define CCRNGSTATE ccrng_seckey
#else
/* extern struct ccrng_state *ccDRBGGetRngState(); */
#include <CommonCrypto/CommonRandomSPI.h>
#define CCRNGSTATE ccDRBGGetRngState()
#endif


/*
 * Free a pubKey object.
 */
extern OSStatus sslFreePubKey(SSLPubKey **pubKey)
{
	if (pubKey && *pubKey) {
		CFReleaseNull(SECKEYREF(*pubKey));
	}
	return errSecSuccess;
}

/*
 * Free a privKey object.
 */
extern OSStatus sslFreePrivKey(SSLPrivKey **privKey)
{
	if (privKey && *privKey) {
		CFReleaseNull(SECKEYREF(*privKey));
	}
	return errSecSuccess;
}

/*
 * Get algorithm id for a SSLPubKey object.
 */
CFIndex sslPubKeyGetAlgorithmID(SSLPubKey *pubKey)
{
#if TARGET_OS_IPHONE
	return SecKeyGetAlgorithmID(SECKEYREF(pubKey));
#else
	return SecKeyGetAlgorithmId(SECKEYREF(pubKey));
#endif
}

/*
 * Get algorithm id for a SSLPrivKey object.
 */
CFIndex sslPrivKeyGetAlgorithmID(SSLPrivKey *privKey)
{
#if TARGET_OS_IPHONE
	return SecKeyGetAlgorithmID(SECKEYREF(privKey));
#else
	return SecKeyGetAlgorithmId(SECKEYREF(privKey));
#endif
}

/*
 * Raw RSA/DSA sign/verify.
 */
OSStatus sslRawSign(
	SSLContext			*ctx,
	SSLPrivKey          *privKey,
	const uint8_t       *plainText,
	size_t              plainTextLen,
	uint8_t				*sig,			// mallocd by caller; RETURNED
	size_t              sigLen,         // available
	size_t              *actualBytes)   // RETURNED
{
#if 0
	RSAStatus rsaStatus;
#if RSA_SIG_SHARE_GIANT
	RSASignBuffer *signBuffer = (RSASignBuffer *)sig;
	assert(sigLen >= sizeof(RSASignBuffer));
#endif
	assert(actualBytes != NULL);

	/* @@@ Shouldn't need to init giSigLen according to libgRSA docs. */
	gi_uint16 giSigLen = sigLen;

	rsaStatus = RSA_Sign(&privKey->rsaKey,
		RP_PKCS1,
		plainText,
		plainTextLen,
#if RSA_SIG_SHARE_GIANT
		signBuffer,
#else
		sig,
#endif
		&giSigLen);
	*actualBytes = giSigLen;

	return rsaStatus ? rsaStatusToSSL(rsaStatus) : errSecSuccess;
#else

	size_t inOutSigLen = sigLen;

	assert(actualBytes != NULL);

    OSStatus status = SecKeyRawSign(SECKEYREF(privKey), kSecPaddingPKCS1,
        plainText, plainTextLen, sig, &inOutSigLen);

	if (status) {
		sslErrorLog("sslRawSign: SecKeyRawSign failed (error %d)\n", (int)status);
	}

    /* Since the KeyExchange already allocated modulus size bytes we'll
        use all of them.  SecureTransport has always sent that many bytes,
        so we're not going to deviate, to avoid interoperability issues. */
    if (!status && (inOutSigLen < sigLen)) {
        size_t offset = sigLen - inOutSigLen;
        memmove(sig + offset, sig, inOutSigLen);
        memset(sig, 0, offset);
        inOutSigLen = sigLen;
    }


	*actualBytes = inOutSigLen;
	return status;
#endif
}

/* TLS 1.2 RSA signature */
OSStatus sslRsaSign(
                    SSLContext			*ctx,
                    SSLPrivKey          *privKey,
                    const SecAsn1AlgId  *algId,
                    const uint8_t       *plainText,
                    size_t              plainTextLen,
                    uint8_t				*sig,			// mallocd by caller; RETURNED
                    size_t              sigLen,         // available
                    size_t              *actualBytes)   // RETURNED
{
	size_t inOutSigLen = sigLen;

	assert(actualBytes != NULL);

    OSStatus status = SecKeySignDigest(SECKEYREF(privKey), algId,
                                    plainText, plainTextLen, sig, &inOutSigLen);

	if (status) {
		sslErrorLog("sslRsaSign: SecKeySignDigest failed (error %d)\n", (int) status);
	}

    /* Since the KeyExchange already allocated modulus size bytes we'll
     use all of them.  SecureTransport has always sent that many bytes,
     so we're not going to deviate, to avoid interoperability issues. */
    if (!status && (inOutSigLen < sigLen)) {
        size_t offset = sigLen - inOutSigLen;
        memmove(sig + offset, sig, inOutSigLen);
        memset(sig, 0, offset);
        inOutSigLen = sigLen;
    }

	*actualBytes = inOutSigLen;
	return status;
}

OSStatus sslRawVerify(
	SSLContext			*ctx,
	SSLPubKey           *pubKey,
	const uint8_t       *plainText,
	size_t              plainTextLen,
	const uint8_t       *sig,
	size_t              sigLen)         // available
{
#if 0
	RSAStatus rsaStatus;

	rsaStatus = RSA_SigVerify(&pubKey->rsaKey,
		RP_PKCS1,
		plainText,
		plainTextLen,
		sig,
		sigLen);

	return rsaStatus ? rsaStatusToSSL(rsaStatus) : errSecSuccess;
#else
	OSStatus status = SecKeyRawVerify(SECKEYREF(pubKey), kSecPaddingPKCS1,
        plainText, plainTextLen, sig, sigLen);

	if (status) {
		sslErrorLog("sslRawVerify: SecKeyRawVerify failed (error %d)\n", (int) status);
	}

	return status;
#endif
}

/* TLS 1.2 RSA verify */
OSStatus sslRsaVerify(
                      SSLContext		  *ctx,
                      SSLPubKey           *pubKey,
                      const SecAsn1AlgId  *algId,
                      const uint8_t       *plainText,
                      size_t              plainTextLen,
                      const uint8_t       *sig,
                      size_t              sigLen)         // available
{
	OSStatus status = SecKeyVerifyDigest(SECKEYREF(pubKey), algId,
                           plainText, plainTextLen, sig, sigLen);

	if (status) {
		sslErrorLog("sslRsaVerify: SecKeyVerifyDigest failed (error %d)\n", (int) status);
	}

	return status;
}

/*
 * Encrypt/Decrypt
 */
OSStatus sslRsaEncrypt(
	SSLContext			*ctx,
	SSLPubKey           *pubKey,
	const uint32_t		padding,
	const uint8_t       *plainText,
	size_t              plainTextLen,
	uint8_t				*cipherText,		// mallocd by caller; RETURNED
	size_t              cipherTextLen,      // available
	size_t              *actualBytes)       // RETURNED
{
#if 0
	gi_uint16 giCipherTextLen = cipherTextLen;
	RSAStatus rsaStatus;

	assert(actualBytes != NULL);

	rsaStatus = RSA_Encrypt(&pubKey->rsaKey,
		RP_PKCS1,
		getRandomByte,
		plainText,
		plainTextLen,
		cipherText,
		&giCipherTextLen);
	*actualBytes = giCipherTextLen;

	return rsaStatus ? rsaStatusToSSL(rsaStatus) : errSecSuccess;
#else
    size_t ctlen = cipherTextLen;

	assert(actualBytes != NULL);

#if RSA_PUB_KEY_USAGE_HACK
	/* Force key usage to allow encryption with public key */
	#if (TARGET_OS_MAC && !(TARGET_OS_EMBEDDED || TARGET_OS_IPHONE))
	CSSM_KEY *cssmKey = NULL;
	if (SecKeyGetCSSMKey(SECKEYREF(pubKey), (const CSSM_KEY **)&cssmKey)==errSecSuccess && cssmKey)
		cssmKey->KeyHeader.KeyUsage |= CSSM_KEYUSE_ENCRYPT;
	#endif
#endif

    OSStatus status = SecKeyEncrypt(SECKEYREF(pubKey), padding,
        plainText, plainTextLen, cipherText, &ctlen);

	if (status) {
		sslErrorLog("sslRsaEncrypt: SecKeyEncrypt failed (error %d)\n", (int)status);
	}

    /* Since the KeyExchange already allocated modulus size bytes we'll
        use all of them.  SecureTransport has always sent that many bytes,
        so we're not going to deviate, to avoid interoperability issues. */
    if (!status && (ctlen < cipherTextLen)) {
        size_t offset = cipherTextLen - ctlen;
        memmove(cipherText + offset, cipherText, ctlen);
        memset(cipherText, 0, offset);
        ctlen = cipherTextLen;
    }

    if (actualBytes)
        *actualBytes = ctlen;

    if (status) {
        sslErrorLog("***sslRsaEncrypt: error %d\n", (int)status);
    }
    return status;
#endif
}

OSStatus sslRsaDecrypt(
	SSLContext			*ctx,
	SSLPrivKey			*privKey,
	const uint32_t		padding,
	const uint8_t       *cipherText,
	size_t              cipherTextLen,
	uint8_t				*plainText,			// mallocd by caller; RETURNED
	size_t              plainTextLen,		// available
	size_t              *actualBytes) 		// RETURNED
{
#if 0
	gi_uint16 giPlainTextLen = plainTextLen;
	RSAStatus rsaStatus;

	assert(actualBytes != NULL);

	rsaStatus = RSA_Decrypt(&privKey->rsaKey,
		RP_PKCS1,
		cipherText,
		cipherTextLen,
		plainText,
		&giPlainTextLen);
	*actualBytes = giPlainTextLen;

	return rsaStatus ? rsaStatusToSSL(rsaStatus) : errSecSuccess;
#else
	size_t ptlen = plainTextLen;

	assert(actualBytes != NULL);

    OSStatus status = SecKeyDecrypt(SECKEYREF(privKey), padding,
        cipherText, cipherTextLen, plainText, &ptlen);
	*actualBytes = ptlen;

    if (status) {
        sslErrorLog("sslRsaDecrypt: SecKeyDecrypt failed (error %d)\n", (int)status);
	}

	return status;
#endif
}

/*
 * Obtain size of the modulus of privKey in bytes.
 */
size_t sslPrivKeyLengthInBytes(SSLPrivKey *privKey)
{
#if 0
	/* Get the length of p + q (which is the size of the modulus) in bits. */
	gi_uint16 bitLen = bitlen(&privKey->rsaKey.p.g) +
		bitlen(&privKey->rsaKey.q.g);
	/* Convert it to bytes. */
	return (bitLen + 7) / 8;
#else
    return SecKeyGetBlockSize(SECKEYREF(privKey));
#endif
}

/*
 * Obtain size of the modulus of pubKey in bytes.
 */
size_t sslPubKeyLengthInBytes(SSLPubKey *pubKey)
{
#if 0
	/* Get the length of the modulus in bytes. */
	return giantNumBytes(&pubKey->rsaKey.n.g);
#else
    return SecKeyGetBlockSize(SECKEYREF(pubKey));
#endif
}


/*
 * Obtain maximum size of signature in bytes. A bit of a kludge; we could
 * ask the CSP to do this but that would be kind of expensive.
 */
OSStatus sslGetMaxSigSize(
	SSLPrivKey *privKey,
	size_t           *maxSigSize)
{
	assert(maxSigSize != NULL);

#if 0
#if RSA_SIG_SHARE_GIANT
	*maxSigSize = sizeof(RSASignBuffer);
#else
	*maxSigSize = MAX_PRIME_SIZE_BYTES;
#endif
#else
    *maxSigSize = SecKeyGetBlockSize(SECKEYREF(privKey));
#endif

	return errSecSuccess;
}

#if 0
static OSStatus sslGiantToBuffer(
	SSLContext			*ctx,			// Currently unused.
	giant g,
	SSLBuffer *buffer)
{
	gi_uint8 *chars;
	gi_uint16 ioLen;
	gi_uint16 zeroCount;
	GIReturn giReturn;
	OSStatus status;

	ioLen = serializeGiantBytes(g);
	status = SSLAllocBuffer(buffer, ioLen);
	if (status)
		return status;
	chars = buffer->data;

	/* Serialize the giant g into chars. */
	giReturn = serializeGiant(g, chars, &ioLen);
	if(giReturn) {
		SSLFreeBuffer(buffer);
		return giReturnToSSL(giReturn);
	}

	/* Trim off leading zeroes (but leave one zero if that's all there is). */
	for (zeroCount = 0; zeroCount < (ioLen - 1); ++zeroCount)
		if (chars[zeroCount])
			break;

	if (zeroCount > 0) {
		buffer->length = ioLen - zeroCount;
		memmove(chars, chars + zeroCount, buffer->length);
	}

	return status;
}

/*
 * Get raw key bits from an RSA public key.
 */
OSStatus sslGetPubKeyBits(
	SSLContext			*ctx,			// Currently unused.
	SSLPubKey           *pubKey,
	SSLBuffer			*modulus,		// data mallocd and RETURNED
	SSLBuffer			*exponent)		// data mallocd and RETURNED
{
	OSStatus status;

	status = sslGiantToBuffer(ctx, &pubKey->rsaKey.n.g, modulus);
	if(status)
		return status;

	status = sslGiantToBuffer(ctx, &pubKey->rsaKey.e.g, exponent);
	if(status) {
		SSLFreeBuffer(modulus);
		return status;
	}

	return status;
}
#endif

/*
 * Given raw RSA key bits, cook up a SSLPubKey. Used in
 * Server-initiated key exchange.
 */
OSStatus sslGetPubKeyFromBits(
	SSLContext			*ctx,
	const SSLBuffer		*modulus,
	const SSLBuffer		*exponent,
	SSLPubKey           **pubKey)        // mallocd and RETURNED
{
	if (!pubKey)
		return errSecParam;
#if 0
	SSLPubKey *key;
	RSAStatus rsaStatus;
	RSAPubKey apiKey = {
		modulus->data, modulus->length,
		NULL, 0,
		exponent->data, exponent->length
	};

	key = sslMalloc(sizeof(*key));
	rsaStatus = rsaInitPubGKey(&apiKey, &key->rsaKey);
	if (rsaStatus) {
		sslFree(key);
		return rsaStatusToSSL(rsaStatus);
	}

	*pubKey = key;
	return errSecSuccess;
#else
	check(pubKey);
	SecRSAPublicKeyParams params = {
		modulus->data, modulus->length,
		exponent->data, exponent->length
	};
#if SSL_DEBUG
	sslDebugLog("Creating RSA pub key from modulus=%p len=%lu exponent=%p len=%lu\n",
			modulus->data, modulus->length,
			exponent->data, exponent->length);
#endif
	SecKeyRef key = SecKeyCreateRSAPublicKey(NULL, (const uint8_t *)&params,
			sizeof(params), kSecKeyEncodingRSAPublicParams);
	if (!key) {
		sslErrorLog("sslGetPubKeyFromBits: SecKeyCreateRSAPublicKey failed\n");
		return errSSLCrypto;
	}
#if SSL_DEBUG
	sslDebugLog("sslGetPubKeyFromBits: RSA pub key block size=%lu\n", SecKeyGetBlockSize(key));
#endif
	*pubKey = (SSLPubKey*)key;
	return errSecSuccess;
#endif
}

// MARK: -
// MARK: Public Certificate Functions

#ifdef USE_SSLCERTIFICATE

/*
 * Given a SSLCertificate cert, obtain its public key as a SSLPubKey.
 * Caller must sslFreePubKey and free the SSLPubKey itself.
 */
OSStatus sslPubKeyFromCert(
	SSLContext 				*ctx,
	const SSLCertificate	*cert,
	SSLPubKey               **pubKey) 		// RETURNED
{
    DERItem der;
    DERSignedCertCrl signedCert;
    DERTBSCert tbsCert;
	DERSubjPubKeyInfo pubKeyInfo;
	DERByte numUnused;
    DERItem pubKeyPkcs1;
    SSLPubKey *key;
	DERReturn drtn;
    RSAStatus rsaStatus;

    assert(cert);
	assert(pubKey != NULL);

    der.data = cert->derCert.data;
    der.length = cert->derCert.length;

	/* top level decode */
	drtn = DERParseSequence(&der, DERNumSignedCertCrlItemSpecs,
		DERSignedCertCrlItemSpecs, &signedCert, sizeof(signedCert));
	if(drtn)
		return errSSLBadCert;

	/* decode the TBSCert - it was saved in full DER form */
	drtn = DERParseSequence(&signedCert.tbs,
		DERNumTBSCertItemSpecs, DERTBSCertItemSpecs,
		&tbsCert, sizeof(tbsCert));
	if(drtn)
		return errSSLBadCert;

	/* sequence we're given: encoded DERSubjPubKeyInfo */
	drtn = DERParseSequenceContent(&tbsCert.subjectPubKey,
		DERNumSubjPubKeyInfoItemSpecs, DERSubjPubKeyInfoItemSpecs,
		&pubKeyInfo, sizeof(pubKeyInfo));
	if(drtn)
		return errSSLBadCert;

	/* @@@ verify that this is an RSA key by decoding the AlgId */

	/*
	 * The contents of pubKeyInfo.pubKey is a bit string whose contents
	 * are a PKCS1 format RSA key.
	 */
	drtn = DERParseBitString(&pubKeyInfo.pubKey, &pubKeyPkcs1, &numUnused);
	if(drtn)
		return errSSLBadCert;

#if TARGET_OS_IPHONE
    /* Now we have the public key in pkcs1 format.  Let's make a public key
       object out of it. */
    key = sslMalloc(sizeof(*key));
    rsaStatus = RSA_DecodePubKey(pubKeyPkcs1.data, pubKeyPkcs1.length,
        &key->rsaKey);
	if (rsaStatus) {
		sslFree(key);
	}
#else
	SecKeyRef rsaPubKeyRef = SecKeyCreateRSAPublicKey(NULL,
		pubKeyPkcs1.data, pubKeyPkcs1.length,
		kSecKeyEncodingRSAPublicParams);
	rsaStatus = (rsaPubKeyRef) ? 0 : 1;
	key = (SSLPubKey*)rsaPubKeyRef;
#endif
	if (rsaStatus) {
		return rsaStatusToSSL(rsaStatus);
	}

	*pubKey = key;
	return errSecSuccess;
}

/*
 * Verify a chain of DER-encoded certs.
 * First cert in a chain is root; this must also be present
 * in ctx->trustedCerts.
 *
 * If arePeerCerts is true, host name verification is enabled and we
 * save the resulting SecTrustRef in ctx->peerSecTrust. Otherwise
 * we're just validating our own certs; no host name checking and
 * peerSecTrust is transient.
 */
 OSStatus sslVerifyCertChain(
	SSLContext				*ctx,
	const SSLCertificate	*certChain,
	bool					arePeerCerts)
{
	OSStatus ortn = errSecSuccess;

    assert(certChain);

    /* No point checking our own certs, our clients can do that. */
    if (!arePeerCerts)
        return errSecSuccess;

    CertVerifyReturn cvrtn;
    /* @@@ Add real cert checking. */
    if (certChain->next) {
        DERItem subject, issuer;

        issuer.data = certChain->derCert.data;
        issuer.length = certChain->derCert.length;
        subject.data = certChain->next->derCert.data;
        subject.length = certChain->next->derCert.length;
        cvrtn = certVerify(&subject, &issuer);
        if (cvrtn != CVR_Success)
            ortn = errSSLBadCert;
    }
    else
    {
		sslErrorLog("***sslVerifyCertChain: only one cert in chain\n");
    }
	return ortn;
}

#else /* !USE_SSLCERTIFICATE */

OSStatus
sslCreateSecTrust(
	SSLContext				*ctx,
	CFArrayRef				certChain,
	bool					arePeerCerts,
    SecTrustRef             *pTrust)	/* RETURNED */
{
	OSStatus status = errSecAllocate;
	CFStringRef peerDomainName = NULL;
	CFTypeRef policies = NULL;
	SecTrustRef trust = NULL;

    if (CFArrayGetCount(certChain) == 0) {
		status = errSSLBadCert;
		goto errOut;
	}

	if (arePeerCerts) {
		if (ctx->peerDomainNameLen && ctx->peerDomainName) {
			CFIndex len = ctx->peerDomainNameLen;
			if (ctx->peerDomainName[len - 1] == 0) {
				len--;
				//secwarning("peerDomainName is zero terminated!");
			}
			/* @@@ Double check that this is the correct encoding. */
			require(peerDomainName = CFStringCreateWithBytes(kCFAllocatorDefault,
				(const UInt8 *)ctx->peerDomainName, len,
				kCFStringEncodingUTF8, false), errOut);
		}
	}
    /* If we are the client, our peer certificates must satisfy the
       ssl server policy. */
    bool server = ctx->protocolSide == kSSLClientSide;
	require(policies = SecPolicyCreateSSL(server, peerDomainName), errOut);

	require_noerr(status = SecTrustCreateWithCertificates(certChain, policies,
		&trust), errOut);

	/* If we have trustedAnchors we set them here. */
    if (ctx->trustedCerts) {
        require_noerr(status = SecTrustSetAnchorCertificates(trust,
            ctx->trustedCerts), errOut);
        require_noerr(status = SecTrustSetAnchorCertificatesOnly(trust,
            ctx->trustedCertsOnly), errOut);
    }

    status = errSecSuccess;

errOut:
	CFReleaseSafe(peerDomainName);
	CFReleaseSafe(policies);

	*pTrust = trust;

	return status;
}

/* Return the first certificate reference from the supplied array
 * whose data matches the given certificate, or NULL if none match.
 */
static
SecCertificateRef
sslGetMatchingCertInArray(
	SecCertificateRef	certRef,
	CFArrayRef			certArray)
{
	SecCertificateRef matchedCert = NULL;

	if (certRef == NULL || certArray == NULL) {
		return NULL;
	}

	CFDataRef certData = SecCertificateCopyData(certRef);
	if (certData) {
		CFIndex idx, count = CFArrayGetCount(certArray);
		for(idx=0; idx<count; idx++) {
			SecCertificateRef aCert = (SecCertificateRef)CFArrayGetValueAtIndex(certArray, idx);
			CFDataRef aData = SecCertificateCopyData(aCert);
			if (aData && CFEqual(aData, certData)) {
				matchedCert = aCert;
			}
			CFReleaseSafe(aData);
			if (matchedCert)
				break;
		}
		CFReleaseSafe(certData);
	}

    return matchedCert;
}

/*
 * Verify a chain of DER-encoded certs.
 * Last cert in a chain is the leaf; this must also be present
 * in ctx->trustedCerts.
 *
 * If arePeerCerts is true, host name verification is enabled and we
 * save the resulting SecTrustRef in ctx->peerSecTrust. Otherwise
 * we're just validating our own certs; no host name checking and
 * peerSecTrust is transient.
 */
extern OSStatus sslVerifyCertChain(
	SSLContext				*ctx,
	CFArrayRef				certChain,
	bool					arePeerCerts)
{
	OSStatus status;
	SecTrustRef trust = NULL;

    assert(certChain);

    if (arePeerCerts) {
		/* renegotiate - start with a new SecTrustRef */
        CFReleaseNull(ctx->peerSecTrust);
    }

	status = sslCreateSecTrust(ctx, certChain, arePeerCerts, &trust);

	if (!ctx->enableCertVerify) {
		/* trivial case, this is caller's responsibility */
		status = errSecSuccess;
		goto errOut;
	}

	SecTrustResultType secTrustResult;
	require_noerr(status = SecTrustEvaluate(trust, &secTrustResult), errOut);
	switch (secTrustResult) {
        case kSecTrustResultUnspecified:
            /* cert chain valid, no special UserTrust assignments */
        case kSecTrustResultProceed:
            /* cert chain valid AND user explicitly trusts this */
            status = errSecSuccess;
            break;
        case kSecTrustResultDeny:
        case kSecTrustResultConfirm:
        case kSecTrustResultRecoverableTrustFailure:
        default:
            if(ctx->allowAnyRoot) {
                sslErrorLog("***Warning: accepting unverified cert chain\n");
                status = errSecSuccess;
            }
            else {
				/*
				 * If the caller provided a list of trusted leaf certs, check them here
				 */
				if(ctx->trustedLeafCerts) {
					if (sslGetMatchingCertInArray((SecCertificateRef)CFArrayGetValueAtIndex(certChain, 0),
								ctx->trustedLeafCerts)) {
						status = errSecSuccess;
						goto errOut;
					}
				}
				status = errSSLXCertChainInvalid;
            }
            /* Do we really need to return things like:
                   errSSLNoRootCert
                   errSSLUnknownRootCert
                   errSSLCertExpired
                   errSSLCertNotYetValid
                   errSSLHostNameMismatch
               for our client to see what went wrong, or should we just always
               return
                   errSSLXCertChainInvalid
               when something is wrong? */
            break;
	}

errOut:
	if (arePeerCerts)
		ctx->peerSecTrust = trust;
	else
        CFReleaseSafe(trust);

	return status;
}

/*
 * Given a SecCertificateRef cert, obtain its public key as a SSLPubKey.
 * Caller must sslFreePubKey and free the SSLPubKey itself.
 */
extern OSStatus sslCopyPeerPubKey(
	SSLContext 				*ctx,
	SSLPubKey               **pubKey)
{
    check(pubKey);
    check(ctx->peerSecTrust);

#if !TARGET_OS_IPHONE
    /* This is not required on iOS, but still required on osx */
    if (!ctx->enableCertVerify) {
        OSStatus status;
        SecTrustResultType result;
        verify_noerr_action(status = SecTrustEvaluate(ctx->peerSecTrust, &result),
            return status);
	}
#endif

    SecKeyRef key = SecTrustCopyPublicKey(ctx->peerSecTrust);
    if (!key) {
		sslErrorLog("sslCopyPeerPubKey: %s, ctx->peerSecTrust=%p\n",
			"SecTrustCopyPublicKey failed", ctx->peerSecTrust);
		return errSSLBadCert;
	}
    *pubKey = (SSLPubKey*)key;

    return errSecSuccess;
}

#endif /* !USE_SSLCERTIFICATE */

#ifndef	NDEBUG
void stPrintCdsaError(const char *op, OSStatus crtn)
{
	assert(FALSE);
}
#endif

/*
 * After ciphersuite negotiation is complete, verify that we have
 * the capability of actually performing the selected cipher.
 * Currently we just verify that we have a cert and private signing
 * key, if needed, and that the signing key's algorithm matches the
 * expected key exchange method.
 *
 * This is currently called from FindCipherSpec(), after it sets
 * ctx->selectedCipherSpec to a (supposedly) valid value, and from
 * sslBuildCipherSpecArray(), in server mode (pre-negotiation) only.
 */
OSStatus sslVerifySelectedCipher(SSLContext *ctx)
{
    if(ctx->protocolSide == kSSLClientSide) {
        return errSecSuccess;
    }
#if SSL_PAC_SERVER_ENABLE
    if((ctx->masterSecretCallback != NULL) &&
       (ctx->sessionTicket.data != NULL)) {
            /* EAP via PAC resumption; we can do it */
	return errSecSuccess;
    }
#endif	/* SSL_PAC_SERVER_ENABLE */

    CFIndex requireAlg;
    switch (ctx->selectedCipherSpecParams.keyExchangeMethod) {
        case SSL_RSA:
        case SSL_RSA_EXPORT:
	case SSL_DH_RSA:
	case SSL_DH_RSA_EXPORT:
	case SSL_DHE_RSA:
	case SSL_DHE_RSA_EXPORT:
            requireAlg = kSecRSAAlgorithmID;
            break;
 	case SSL_DHE_DSS:
	case SSL_DHE_DSS_EXPORT:
 	case SSL_DH_DSS:
	case SSL_DH_DSS_EXPORT:
            requireAlg = kSecDSAAlgorithmID;
            break;
	case SSL_DH_anon:
	case SSL_DH_anon_EXPORT:
        case TLS_PSK:
            requireAlg = kSecNullAlgorithmID; /* no signing key */
            break;
        /*
         * When SSL_ECDSA_SERVER is true and we support ECDSA on the server side,
         * we'll need to add some logic here...
         */
#if SSL_ECDSA_SERVER
        case SSL_ECDHE_ECDSA:
        case SSL_ECDHE_RSA:
        case SSL_ECDH_ECDSA:
        case SSL_ECDH_RSA:
        case SSL_ECDH_anon:
            requireAlg = kSecECDSAAlgorithmID;
            break;
#endif

	default:
            /* needs update per cipherSpecs.c */
            assert(0);
            sslErrorLog("sslVerifySelectedCipher: unknown key exchange method\n");
            return errSSLInternal;
    }

    if(requireAlg == kSecNullAlgorithmID) {
	return errSecSuccess;
    }

    /* private signing key required */
    if(ctx->signingPrivKeyRef == NULL) {
	sslErrorLog("sslVerifySelectedCipher: no signing key\n");
	return errSSLBadConfiguration;
    }

    /* Check the alg of our signing key. */
    CFIndex keyAlg = sslPrivKeyGetAlgorithmID(ctx->signingPrivKeyRef);
    if (requireAlg != keyAlg) {
	sslErrorLog("sslVerifySelectedCipher: signing key alg mismatch\n");
	return errSSLBadConfiguration;
    }

    return errSecSuccess;
}

#if APPLE_DH

/* FIXME: This is duplicated in SecDH */
typedef struct {
	DERItem				p;
	DERItem				g;
	DERItem				l;
} DER_DHParams;

static const DERItemSpec DER_DHParamsItemSpecs[] =
{
	{ DER_OFFSET(DER_DHParams, p),
        ASN1_INTEGER,
        DER_DEC_NO_OPTS | DER_ENC_SIGNED_INT },
	{ DER_OFFSET(DER_DHParams, g),
        ASN1_INTEGER,
        DER_DEC_NO_OPTS | DER_ENC_SIGNED_INT },
	{ DER_OFFSET(DER_DHParams, l),
        ASN1_INTEGER,
        DER_DEC_OPTIONAL | DER_ENC_SIGNED_INT },
};
static const DERSize DER_NumDHParamsItemSpecs =
sizeof(DER_DHParamsItemSpecs) / sizeof(DERItemSpec);

/* Max encoded size for standard (PKCS3) parameters */
#define DH_ENCODED_PARAM_SIZE(primeSizeInBytes)					\
DER_MAX_ENCODED_SIZE(										\
DER_MAX_ENCODED_SIZE(primeSizeInBytes) +		/* g */		\
DER_MAX_ENCODED_SIZE(primeSizeInBytes) +		/* p */     \
DER_MAX_ENCODED_SIZE(4))                        /* l */


OSStatus sslDecodeDhParams(
	const SSLBuffer	*blob,			/* Input - PKCS-3 encoded */
	SSLBuffer		*prime,			/* Output - wire format */
	SSLBuffer		*generator)     /* Output - wire format */
{
    OSStatus ortn = errSecSuccess;
    DERReturn drtn;
	DERItem paramItem = {(DERByte *)blob->data, blob->length};
	DER_DHParams decodedParams;

    drtn = DERParseSequence(&paramItem,
                            DER_NumDHParamsItemSpecs, DER_DHParamsItemSpecs,
                            &decodedParams, sizeof(decodedParams));
    if(drtn)
        return drtn;

    prime->data = decodedParams.p.data;
    prime->length = decodedParams.p.length;

    generator->data = decodedParams.g.data;
    generator->length = decodedParams.g.length;

    return ortn;
}


OSStatus sslEncodeDhParams(SSLBuffer        *blob,			/* data mallocd and RETURNED PKCS-3 encoded */
                           const SSLBuffer	*prime,			/* Wire format */
                           const SSLBuffer	*generator)     /* Wire format */
{
    OSStatus ortn = errSecSuccess;
    DER_DHParams derParams =
    {
        .p = {
            .length = prime->length,
            .data = prime->data,
        },
        .g = {
            .length = generator->length,
            .data = generator->data,
        },
        .l = {
            .length = 0,
            .data = NULL,
        }
    };

    DERSize ioLen = DH_ENCODED_PARAM_SIZE(derParams.p.length);
    DERByte *der = sslMalloc(ioLen);
    // FIXME: What if this fails - we should probably not have a malloc here ?
    assert(der);
    ortn = (OSStatus)DEREncodeSequence(ASN1_CONSTR_SEQUENCE,
                                       &derParams,
                                       DER_NumDHParamsItemSpecs, DER_DHParamsItemSpecs,
                                       der,
                                       &ioLen);
    // This should never fail

    blob->length=ioLen;
    blob->data=der;

    return ortn;
}

OSStatus sslDhCreateKey(SSLContext *ctx)
{
    if (ctx->secDHContext) {
        SecDHDestroy(ctx->secDHContext);
        ctx->secDHContext = NULL;
    }

    /* Server params are set using encoded dh params */
    if (!(ctx->dhParamsEncoded.length && ctx->dhParamsEncoded.data))
        return errSSLInternal;

    if (SecDHCreateFromParameters(ctx->dhParamsEncoded.data,
        ctx->dhParamsEncoded.length, &ctx->secDHContext))
            return errSSLCrypto;

    return errSecSuccess;
}

OSStatus sslDhGenerateKeyPair(SSLContext *ctx)
{
    OSStatus ortn = errSecSuccess;
    
    require_noerr(ortn = SSLAllocBuffer(&ctx->dhExchangePublic, 
        SecDHGetMaxKeyLength(ctx->secDHContext)), out);
    require_noerr(ortn = SecDHGenerateKeypair(ctx->secDHContext, 
        ctx->dhExchangePublic.data, &ctx->dhExchangePublic.length), out);

out:
    return ortn;
}


OSStatus sslDhKeyExchange(SSLContext *ctx)
{
    OSStatus ortn = errSecSuccess;

	if (ctx == NULL ||
        ctx->secDHContext == NULL ||
        ctx->dhPeerPublic.length == 0) {
		/* comes from peer, don't panic */
		sslErrorLog("sslDhKeyExchange: null peer public key\n");
		return errSSLProtocol;
	}

    require_noerr(ortn = SSLAllocBuffer(&ctx->preMasterSecret, 
        SecDHGetMaxKeyLength(ctx->secDHContext)), out);
    require_noerr(ortn = SecDHComputeKey(ctx->secDHContext, 
        ctx->dhPeerPublic.data, ctx->dhPeerPublic.length, 
        ctx->preMasterSecret.data, &ctx->preMasterSecret.length), out);

	return ortn;
out:
	sslErrorLog("sslDhKeyExchange: failed to compute key (error %d)\n", (int)ortn);
	return ortn;
}

#endif /* APPLE_DH */

/*
 * Given an ECDSA key in SecKey format, extract the SSL_ECDSA_NamedCurve
 * from its algorithm parameters.
 */
OSStatus sslEcdsaPeerCurve(
	SSLPubKey *pubKey,
	SSL_ECDSA_NamedCurve *namedCurve)
{
    /* Cast is safe because enums are kept in sync. */
    *namedCurve = (SSL_ECDSA_NamedCurve)SecECKeyGetNamedCurve(SECKEYREF(pubKey));
    if (*namedCurve == kSecECCurveNone) {
        sslErrorLog("sslEcdsaPeerCurve: no named curve for public key\n");
        return errSSLProtocol;
    }
    return errSecSuccess;
}

/*
 * Generate ECDH key pair with the given SSL_ECDSA_NamedCurve.
 * Private key, in ref form, is placed in ctx->ecdhPrivate.
 * Public key, in ECPoint form - which can NOT be used as
 * a key in any CSP ops - is placed in ecdhExchangePublic.
 */
OSStatus sslEcdhGenerateKeyPair(
	SSLContext *ctx,
	SSL_ECDSA_NamedCurve namedCurve)
{
	OSStatus ortn = errSecSuccess;

    ccec_const_cp_t cp;
	switch (namedCurve) {
    case SSL_Curve_secp256r1:
        cp = ccec_cp_256();
        break;
    case SSL_Curve_secp384r1:
        cp = ccec_cp_384();
        break;
    case SSL_Curve_secp521r1:
        cp = ccec_cp_521();
        break;
    default:
        /* should not have gotten this far */
        sslErrorLog("sslEcdhGenerateKeyPair: bad namedCurve (%u)\n",
            (unsigned)namedCurve);
        return errSSLInternal;
	}

    ccec_generate_key(cp, CCRNGSTATE, ctx->ecdhContext);
    size_t pub_size = ccec_export_pub_size(ctx->ecdhContext);
    SSLFreeBuffer(&ctx->ecdhExchangePublic);
    require_noerr(ortn = SSLAllocBuffer(&ctx->ecdhExchangePublic,
                                        pub_size), errOut);
    ccec_export_pub(ctx->ecdhContext, ctx->ecdhExchangePublic.data);

	sslDebugLog("sslEcdhGenerateKeyPair: pub key size=%ld, data=%p\n",
		pub_size, ctx->ecdhExchangePublic.data);

errOut:
	return ortn;
}

/*
 * Perform ECDH key exchange. Obtained key material is the same
 * size as our private key.
 *
 * On entry, ecdhPrivate is our private key. The peer's public key
 * is either ctx->ecdhPeerPublic for ECDHE exchange, or
 * ctx->peerPubKey for ECDH exchange.
 */
OSStatus sslEcdhKeyExchange(
	SSLContext		*ctx,
	SSLBuffer		*exchanged)
{
	OSStatus ortn = errSecSuccess;
    CFDataRef pubKeyData = NULL;
    const unsigned char *pubKeyBits;
    unsigned long pubKeyLen;

	switch(ctx->selectedCipherSpecParams.keyExchangeMethod) {
		case SSL_ECDHE_ECDSA:
		case SSL_ECDHE_RSA:
			/* public key passed in as CSSM_DATA *Param */
			if(ctx->ecdhPeerPublic.length == 0) {
				/* comes from peer, don't panic */
				sslErrorLog("sslEcdhKeyExchange: null peer public key\n");
				ortn = errSSLProtocol;
				goto errOut;
			}
            pubKeyBits = ctx->ecdhPeerPublic.data;
            pubKeyLen = ctx->ecdhPeerPublic.length;
			break;
		case SSL_ECDH_ECDSA:
		case SSL_ECDH_RSA:
			/* Use the public key provided by the peer. */
			if(ctx->peerPubKey == NULL) {
			   sslErrorLog("sslEcdhKeyExchange: no peer key\n");
			   ortn = errSSLInternal;
			   goto errOut;
			}

            pubKeyData = SecECKeyCopyPublicBits(SECKEYREF(ctx->peerPubKey));
            if (!pubKeyData) {
				sslErrorLog("sslEcdhKeyExchange: SecECKeyCopyPublicBits failed\n");
				ortn = errSSLProtocol;
				goto errOut;
            }
            pubKeyBits = CFDataGetBytePtr(pubKeyData);
            pubKeyLen = CFDataGetLength(pubKeyData);
			break;
		default:
			/* shouldn't be here */
			sslErrorLog("sslEcdhKeyExchange: unknown keyExchangeMethod (%d)\n",
				ctx->selectedCipherSpecParams.keyExchangeMethod);
			assert(0);
			ortn = errSSLInternal;
			goto errOut;
	}

    ccec_const_cp_t cp = ccec_ctx_cp(ctx->ecdhContext);
    ccec_pub_ctx_decl(ccn_sizeof(521), pubKey);
    ccec_import_pub(cp, pubKeyLen, pubKeyBits, pubKey);
    size_t len = 1 + 2 * ccec_ccn_size(cp);
    require_noerr(ortn = SSLAllocBuffer(exchanged, len), errOut);
    require_noerr(ccec_compute_key(ctx->ecdhContext, pubKey,  &exchanged->length, exchanged->data), errOut);

	sslDebugLog("sslEcdhKeyExchange: exchanged key length=%ld, data=%p\n",
		exchanged->length, exchanged->data);

errOut:
    CFReleaseSafe(pubKeyData);
	return ortn;
}

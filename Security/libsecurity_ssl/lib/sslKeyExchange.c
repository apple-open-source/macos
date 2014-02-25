/*
 * Copyright (c) 1999-2001,2005-2012 Apple Inc. All Rights Reserved.
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
 * sslKeyExchange.c - Support for key exchange and server key exchange
 */

#include "ssl.h"
#include "sslContext.h"
#include "sslHandshake.h"
#include "sslMemory.h"
#include "sslDebug.h"
#include "sslUtils.h"
#include "sslCrypto.h"
#include "sslRand.h"
#include "sslDigests.h"

#include <assert.h>
#include <string.h>

#include <stdio.h>
#include <utilities/SecCFRelease.h>
#include <corecrypto/ccdh_gp.h>

#ifdef USE_CDSA_CRYPTO
//#include <utilities/globalizer.h>
//#include <utilities/threading.h>
#include <Security/cssmapi.h>
#include <Security/SecKeyPriv.h>
#include "ModuleAttacher.h"
#else
#include <AssertMacros.h>
#include <Security/oidsalg.h>
#if APPLE_DH

#if TARGET_OS_IPHONE
#include <Security/SecRSAKey.h>
#endif

static OSStatus SSLGenServerDHParamsAndKey(SSLContext *ctx);
static size_t SSLEncodedDHKeyParamsLen(SSLContext *ctx);
static OSStatus SSLEncodeDHKeyParams(SSLContext *ctx, uint8_t *charPtr);

#endif /* APPLE_DH */
#endif /* USE_CDSA_CRYPTO */

// MARK: -
// MARK: Forward Static Declarations

#if APPLE_DH
#if USE_CDSA_CRYPTO
static OSStatus SSLGenServerDHParamsAndKey(SSLContext *ctx);
static OSStatus SSLEncodeDHKeyParams(SSLContext *ctx, uint8_t *charPtr);
#endif
static OSStatus SSLDecodeDHKeyParams(SSLContext *ctx, uint8_t **charPtr,
	size_t length);
#endif
static OSStatus SSLDecodeECDHKeyParams(SSLContext *ctx, uint8_t **charPtr,
	size_t length);

#define DH_PARAM_DUMP		0
#if 	DH_PARAM_DUMP

static void dumpBuf(const char *name, SSLBuffer *buf)
{
	printf("%s:\n", name);
	uint8_t *cp = buf->data;
	uint8_t *endCp = cp + buf->length;

	do {
		unsigned i;
		for(i=0; i<16; i++) {
			printf("%02x ", *cp++);
			if(cp == endCp) {
				break;
			}
		}
		if(cp == endCp) {
			break;
		}
		printf("\n");
	} while(cp < endCp);
	printf("\n");
}
#else
#define dumpBuf(n, b)
#endif	/* DH_PARAM_DUMP */

#if 	APPLE_DH

// MARK: -
// MARK: Local Diffie-Hellman Parameter Generator

/*
 * Process-wide server-supplied Diffie-Hellman parameters.
 * This might be overridden by some API_supplied parameters
 * in the future.
 */
struct ServerDhParams
{
	/* these two for sending over the wire */
	SSLBuffer		prime;
	SSLBuffer		generator;
	/* this one for sending to the CSP at key gen time */
	SSLBuffer		paramBlock;
};


#endif	/* APPLE_DH */

// MARK: -
// MARK: RSA Key Exchange

/*
 * Client RSA Key Exchange msgs actually start with a two-byte
 * length field, contrary to the first version of RFC 2246, dated
 * January 1999. See RFC 2246, March 2002, section 7.4.7.1 for
 * updated requirements.
 */
#define RSA_CLIENT_KEY_ADD_LENGTH		1

static OSStatus
SSLEncodeRSAKeyParams(SSLBuffer *keyParams, SSLPubKey *key, SSLContext *ctx)
{
#if 0
    SSLBuffer   modulus, exponent;
    uint8_t     *charPtr;

#ifdef USE_CDSA_CRYPTO
	if(err = attachToCsp(ctx)) {
		return err;
	}

	/* Note currently ALL public keys are raw, obtained from the CL... */
	assert((*key)->KeyHeader.BlobType == CSSM_KEYBLOB_RAW);
#endif /* USE_CDSA_CRYPTO */

	err = sslGetPubKeyBits(ctx,
		key,
		&modulus,
		&exponent);
	if(err) {
		SSLFreeBuffer(&modulus);
		SSLFreeBuffer(&exponent);
		return err;
	}

    if ((err = SSLAllocBuffer(keyParams,
			modulus.length + exponent.length + 4, ctx)) != 0) {
        return err;
	}
    charPtr = keyParams->data;
    charPtr = SSLEncodeInt(charPtr, modulus.length, 2);
    memcpy(charPtr, modulus.data, modulus.length);
    charPtr += modulus.length;
    charPtr = SSLEncodeInt(charPtr, exponent.length, 2);
    memcpy(charPtr, exponent.data, exponent.length);

	/* these were mallocd by sslGetPubKeyBits() */
	SSLFreeBuffer(&modulus);
	SSLFreeBuffer(&exponent);
    return errSecSuccess;
#else
    CFDataRef modulus = SecKeyCopyModulus(SECKEYREF(key));
    if (!modulus) {
		sslErrorLog("SSLEncodeRSAKeyParams: SecKeyCopyModulus failed\n");
        return errSSLCrypto;
	}
    CFDataRef exponent = SecKeyCopyExponent(SECKEYREF(key));
    if (!exponent) {
		sslErrorLog("SSLEncodeRSAKeyParams: SecKeyCopyExponent failed\n");
        CFRelease(modulus);
        return errSSLCrypto;
    }

    CFIndex modulusLength = CFDataGetLength(modulus);
    CFIndex exponentLength = CFDataGetLength(exponent);
	sslDebugLog("SSLEncodeRSAKeyParams: modulus len=%ld, exponent len=%ld\n",
		modulusLength, exponentLength);
    OSStatus err;
    if ((err = SSLAllocBuffer(keyParams, 
			modulusLength + exponentLength + 4)) != 0) {
        CFReleaseSafe(exponent);
        CFReleaseSafe(modulus);
        return err;
	}
    uint8_t *charPtr = keyParams->data;
    charPtr = SSLEncodeSize(charPtr, modulusLength, 2);
    memcpy(charPtr, CFDataGetBytePtr(modulus), modulusLength);
    charPtr += modulusLength;
    charPtr = SSLEncodeSize(charPtr, exponentLength, 2);
    memcpy(charPtr, CFDataGetBytePtr(exponent), exponentLength);
    CFRelease(modulus);
    CFRelease(exponent);
    return errSecSuccess;
#endif
}

static OSStatus
SSLEncodeRSAPremasterSecret(SSLContext *ctx)
{   SSLBuffer           randData;
    OSStatus            err;

    if ((err = SSLAllocBuffer(&ctx->preMasterSecret, 
			SSL_RSA_PREMASTER_SECRET_SIZE)) != 0)
        return err;

	assert(ctx->negProtocolVersion >= SSL_Version_3_0);

    SSLEncodeInt(ctx->preMasterSecret.data, ctx->clientReqProtocol, 2);
    randData.data = ctx->preMasterSecret.data+2;
    randData.length = SSL_RSA_PREMASTER_SECRET_SIZE - 2;
    if ((err = sslRand(&randData)) != 0)
        return err;
    return errSecSuccess;
}

/*
 * Generate a server key exchange message signed by our RSA or DSA private key.
 */

static OSStatus
SSLSignServerKeyExchangeTls12(SSLContext *ctx, SSLSignatureAndHashAlgorithm sigAlg, SSLBuffer exchangeParams, SSLBuffer signature, size_t *actSigLen)
{
    OSStatus        err;
    SSLBuffer       hashOut, hashCtx, clientRandom, serverRandom;
    uint8_t         hashes[SSL_MAX_DIGEST_LEN];
    SSLBuffer       signedHashes;
    uint8_t			*dataToSign;
	size_t			dataToSignLen;
    const HashReference *hashRef;
    SecAsn1AlgId        algId;

	signedHashes.data = 0;
    hashCtx.data = 0;

    clientRandom.data = ctx->clientRandom;
    clientRandom.length = SSL_CLIENT_SRVR_RAND_SIZE;
    serverRandom.data = ctx->serverRandom;
    serverRandom.length = SSL_CLIENT_SRVR_RAND_SIZE;

    switch (sigAlg.hash) {
        case SSL_HashAlgorithmSHA1:
            hashRef = &SSLHashSHA1;
            algId.algorithm = CSSMOID_SHA1WithRSA;
            break;
        case SSL_HashAlgorithmSHA256:
            hashRef = &SSLHashSHA256;
            algId.algorithm = CSSMOID_SHA256WithRSA;
            break;
        case SSL_HashAlgorithmSHA384:
            hashRef = &SSLHashSHA384;
            algId.algorithm = CSSMOID_SHA384WithRSA;
            break;
        default:
            sslErrorLog("SSLVerifySignedServerKeyExchangeTls12: unsupported hash %d\n", sigAlg.hash);
            return errSSLProtocol;
    }


    dataToSign = hashes;
    dataToSignLen = hashRef->digestSize;
    hashOut.data = hashes;
    hashOut.length = hashRef->digestSize;

    if ((err = ReadyHash(hashRef, &hashCtx)) != 0)
        goto fail;
    if ((err = hashRef->update(&hashCtx, &clientRandom)) != 0)
        goto fail;
    if ((err = hashRef->update(&hashCtx, &serverRandom)) != 0)
        goto fail;
    if ((err = hashRef->update(&hashCtx, &exchangeParams)) != 0)
        goto fail;
    if ((err = hashRef->final(&hashCtx, &hashOut)) != 0)
        goto fail;

    if(sigAlg.signature==SSL_SignatureAlgorithmRSA) {
        err = sslRsaSign(ctx,
                         ctx->signingPrivKeyRef,
                         &algId,
                         dataToSign,
                         dataToSignLen,
                         signature.data,
                         signature.length,
                         actSigLen);
    } else {
        err = sslRawSign(ctx,
                         ctx->signingPrivKeyRef,
                         dataToSign,			// one or two hashes
                         dataToSignLen,
                         signature.data,
                         signature.length,
                         actSigLen);
	}

    if(err) {
		sslErrorLog("SSLDecodeSignedServerKeyExchangeTls12: sslRawVerify "
                    "returned %d\n", (int)err);
		goto fail;
	}

fail:
    SSLFreeBuffer(&signedHashes);
    SSLFreeBuffer(&hashCtx);
    return err;
}

static OSStatus
SSLSignServerKeyExchange(SSLContext *ctx, bool isRsa, SSLBuffer exchangeParams, SSLBuffer signature, size_t *actSigLen)
{
    OSStatus        err;
    uint8_t         hashes[SSL_SHA1_DIGEST_LEN + SSL_MD5_DIGEST_LEN];
    SSLBuffer       clientRandom,serverRandom,hashCtx, hash;
	uint8_t			*dataToSign;
	size_t			dataToSignLen;

    hashCtx.data = 0;

    /* cook up hash(es) for raw sign */
    clientRandom.data   = ctx->clientRandom;
    clientRandom.length = SSL_CLIENT_SRVR_RAND_SIZE;
    serverRandom.data   = ctx->serverRandom;
    serverRandom.length = SSL_CLIENT_SRVR_RAND_SIZE;

    if(isRsa) {
        /* skip this if signing with DSA */
        dataToSign = hashes;
        dataToSignLen = SSL_SHA1_DIGEST_LEN + SSL_MD5_DIGEST_LEN;
        hash.data = &hashes[0];
        hash.length = SSL_MD5_DIGEST_LEN;

        if ((err = ReadyHash(&SSLHashMD5, &hashCtx)) != 0)
            goto fail;
        if ((err = SSLHashMD5.update(&hashCtx, &clientRandom)) != 0)
            goto fail;
        if ((err = SSLHashMD5.update(&hashCtx, &serverRandom)) != 0)
            goto fail;
        if ((err = SSLHashMD5.update(&hashCtx, &exchangeParams)) != 0)
            goto fail;
        if ((err = SSLHashMD5.final(&hashCtx, &hash)) != 0)
            goto fail;
        if ((err = SSLFreeBuffer(&hashCtx)) != 0)
            goto fail;
    }
    else {
        /* DSA - just use the SHA1 hash */
        dataToSign = &hashes[SSL_MD5_DIGEST_LEN];
        dataToSignLen = SSL_SHA1_DIGEST_LEN;
    }
    hash.data = &hashes[SSL_MD5_DIGEST_LEN];
    hash.length = SSL_SHA1_DIGEST_LEN;
    if ((err = ReadyHash(&SSLHashSHA1, &hashCtx)) != 0)
    goto fail;
    if ((err = SSLHashSHA1.update(&hashCtx, &clientRandom)) != 0)
    goto fail;
    if ((err = SSLHashSHA1.update(&hashCtx, &serverRandom)) != 0)
    goto fail;
    if ((err = SSLHashSHA1.update(&hashCtx, &exchangeParams)) != 0)
    goto fail;
    if ((err = SSLHashSHA1.final(&hashCtx, &hash)) != 0)
    goto fail;
    if ((err = SSLFreeBuffer(&hashCtx)) != 0)
    goto fail;


    err = sslRawSign(ctx,
                     ctx->signingPrivKeyRef,
                     dataToSign,			// one or two hashes
                     dataToSignLen,
                     signature.data,
                     signature.length,
                     actSigLen);
    if(err) {
        goto fail;
    }

fail:
    SSLFreeBuffer(&hashCtx);

    return err;
}

static
OSStatus FindSigAlg(SSLContext *ctx,
                    SSLSignatureAndHashAlgorithm *alg)
{
    unsigned i;

    assert(ctx->protocolSide == kSSLServerSide);
    assert(ctx->negProtocolVersion >= TLS_Version_1_2);
    assert(!ctx->isDTLS);

    if((ctx->numClientSigAlgs==0) ||(ctx->clientSigAlgs==NULL))
        return errSSLInternal;

    //FIXME: Need a better way to select here
    for(i=0; i<ctx->numClientSigAlgs; i++) {
        alg->hash = ctx->clientSigAlgs[i].hash;
        alg->signature = ctx->clientSigAlgs[i].signature;
        //We only support RSA for certs on the server side - but we should test against the cert type
        if(ctx->clientSigAlgs[i].signature != SSL_SignatureAlgorithmRSA)
            continue;
        //Let's only support SHA1 and SHA256. SHA384 does not work with 512 bits keys.
        // We should actually test against what the cert can do.
        if((alg->hash==SSL_HashAlgorithmSHA1) || (alg->hash==SSL_HashAlgorithmSHA256)) {
            return errSecSuccess;
        }
    }
    // We could not find a supported signature and hash algorithm
    return errSSLProtocol;
}

static OSStatus
SSLEncodeSignedServerKeyExchange(SSLRecord *keyExch, SSLContext *ctx)
{   OSStatus        err;
    uint8_t         *charPtr;
    size_t          outputLen;
	bool			isRsa = true;
    size_t 			maxSigLen;
    size_t	    	actSigLen;
	SSLBuffer		signature;
    int             head = 4;
    SSLBuffer       exchangeParams;

    assert(ctx->protocolSide == kSSLServerSide);
	assert(ctx->signingPubKey != NULL);
 	assert(ctx->negProtocolVersion >= SSL_Version_3_0);
    exchangeParams.data = 0;
	signature.data = 0;

#if ENABLE_DTLS
    if(ctx->negProtocolVersion == DTLS_Version_1_0) {
        head+=8;
    }
#endif


	/* Set up parameter block to hash ==> exchangeParams */
	switch(ctx->selectedCipherSpecParams.keyExchangeMethod) {
		case SSL_RSA:
        case SSL_RSA_EXPORT:
			/*
			 * Parameter block = encryption public key.
			 * If app hasn't supplied a separate encryption cert, abort.
			 */
			if(ctx->encryptPubKey == NULL) {
				sslErrorLog("RSAServerKeyExchange: no encrypt cert\n");
				return errSSLBadConfiguration;
			}
			err = SSLEncodeRSAKeyParams(&exchangeParams,
				ctx->encryptPubKey, ctx);
			break;

#if APPLE_DH

		case SSL_DHE_DSS:
		case SSL_DHE_DSS_EXPORT:
			isRsa = false;
			/* and fall through */
		case SSL_DHE_RSA:
		case SSL_DHE_RSA_EXPORT:
		{
			/*
			 * Parameter block = {prime, generator, public key}
			 * Obtain D-H parameters (if we don't have them) and a key pair.
			 */
			err = SSLGenServerDHParamsAndKey(ctx);
			if(err) {
				return err;
			}
            size_t len = SSLEncodedDHKeyParamsLen(ctx);
			err = SSLAllocBuffer(&exchangeParams, len);
			if(err) {
				goto fail;
			}
			err = SSLEncodeDHKeyParams(ctx, exchangeParams.data);
			break;
		}

#endif	/* APPLE_DH */

		default:
			/* shouldn't be here */
			assert(0);
			return errSSLInternal;
	}

    SSLSignatureAndHashAlgorithm sigAlg;


    /* preallocate a buffer for signing */
    err = sslGetMaxSigSize(ctx->signingPrivKeyRef, &maxSigLen);
    if(err) {
        goto fail;
    }
    err = SSLAllocBuffer(&signature, maxSigLen);
    if(err) {
        goto fail;
    }

    outputLen = exchangeParams.length + 2;

    if (sslVersionIsLikeTls12(ctx))
    {
        err=FindSigAlg(ctx, &sigAlg);
        if(err)
            goto fail;

        outputLen += 2;
        err = SSLSignServerKeyExchangeTls12(ctx, sigAlg, exchangeParams,
                                                    signature, &actSigLen);
    } else {
        err = SSLSignServerKeyExchange(ctx, isRsa, exchangeParams,
                                               signature, &actSigLen);
    }

    if(err)
        goto fail;

    assert(actSigLen <= maxSigLen);

    outputLen += actSigLen;

	/* package it all up */
    keyExch->protocolVersion = ctx->negProtocolVersion;
    keyExch->contentType = SSL_RecordTypeHandshake;
    if ((err = SSLAllocBuffer(&keyExch->contents, outputLen+head)) != 0)
        goto fail;

    charPtr = SSLEncodeHandshakeHeader(ctx, keyExch, SSL_HdskServerKeyExchange, outputLen);

    memcpy(charPtr, exchangeParams.data, exchangeParams.length);
    charPtr += exchangeParams.length;

    if (sslVersionIsLikeTls12(ctx))
    {
        *charPtr++=sigAlg.hash;
        *charPtr++=sigAlg.signature;
    }

    charPtr = SSLEncodeInt(charPtr, actSigLen, 2);
	memcpy(charPtr, signature.data, actSigLen);
    assert((charPtr + actSigLen) ==
		   (keyExch->contents.data + keyExch->contents.length));

    err = errSecSuccess;

fail:
    SSLFreeBuffer(&exchangeParams);
    SSLFreeBuffer(&signature);
    return err;
}

static OSStatus
SSLVerifySignedServerKeyExchange(SSLContext *ctx, bool isRsa, SSLBuffer signedParams,
                                 uint8_t *signature, UInt16 signatureLen)
{
    OSStatus        err;
    SSLBuffer       hashOut, hashCtx, clientRandom, serverRandom;
    uint8_t         hashes[SSL_SHA1_DIGEST_LEN + SSL_MD5_DIGEST_LEN];
    SSLBuffer       signedHashes;
    uint8_t			*dataToSign;
	size_t			dataToSignLen;

	signedHashes.data = 0;
    hashCtx.data = 0;

    clientRandom.data = ctx->clientRandom;
    clientRandom.length = SSL_CLIENT_SRVR_RAND_SIZE;
    serverRandom.data = ctx->serverRandom;
    serverRandom.length = SSL_CLIENT_SRVR_RAND_SIZE;


	if(isRsa) {
		/* skip this if signing with DSA */
		dataToSign = hashes;
		dataToSignLen = SSL_SHA1_DIGEST_LEN + SSL_MD5_DIGEST_LEN;
		hashOut.data = hashes;
		hashOut.length = SSL_MD5_DIGEST_LEN;
		
		if ((err = ReadyHash(&SSLHashMD5, &hashCtx)) != 0)
			goto fail;
		if ((err = SSLHashMD5.update(&hashCtx, &clientRandom)) != 0)
			goto fail;
		if ((err = SSLHashMD5.update(&hashCtx, &serverRandom)) != 0)
			goto fail;
		if ((err = SSLHashMD5.update(&hashCtx, &signedParams)) != 0)
			goto fail;
		if ((err = SSLHashMD5.final(&hashCtx, &hashOut)) != 0)
			goto fail;
	}
	else {
		/* DSA, ECDSA - just use the SHA1 hash */
		dataToSign = &hashes[SSL_MD5_DIGEST_LEN];
		dataToSignLen = SSL_SHA1_DIGEST_LEN;
	}

	hashOut.data = hashes + SSL_MD5_DIGEST_LEN;
    hashOut.length = SSL_SHA1_DIGEST_LEN;
    if ((err = SSLFreeBuffer(&hashCtx)) != 0)
        goto fail;

    if ((err = ReadyHash(&SSLHashSHA1, &hashCtx)) != 0)
        goto fail;
    if ((err = SSLHashSHA1.update(&hashCtx, &clientRandom)) != 0)
        goto fail;
    if ((err = SSLHashSHA1.update(&hashCtx, &serverRandom)) != 0)
        goto fail;
    if ((err = SSLHashSHA1.update(&hashCtx, &signedParams)) != 0)
        goto fail;
    if ((err = SSLHashSHA1.final(&hashCtx, &hashOut)) != 0)
        goto fail;

	err = sslRawVerify(ctx,
                       ctx->peerPubKey,
                       dataToSign,				/* plaintext */
                       dataToSignLen,			/* plaintext length */
                       signature,
                       signatureLen);
	if(err) {
		sslErrorLog("SSLDecodeSignedServerKeyExchange: sslRawVerify "
                    "returned %d\n", (int)err);
		goto fail;
	}

fail:
    SSLFreeBuffer(&signedHashes);
    SSLFreeBuffer(&hashCtx);
    return err;

}

static OSStatus
SSLVerifySignedServerKeyExchangeTls12(SSLContext *ctx, SSLSignatureAndHashAlgorithm sigAlg, SSLBuffer signedParams,
                                 uint8_t *signature, UInt16 signatureLen)
{
    OSStatus        err;
    SSLBuffer       hashOut, hashCtx, clientRandom, serverRandom;
    uint8_t         hashes[SSL_MAX_DIGEST_LEN];
    SSLBuffer       signedHashes;
    uint8_t			*dataToSign;
	size_t			dataToSignLen;
    const HashReference *hashRef;
    SecAsn1AlgId        algId;

	signedHashes.data = 0;
    hashCtx.data = 0;

    clientRandom.data = ctx->clientRandom;
    clientRandom.length = SSL_CLIENT_SRVR_RAND_SIZE;
    serverRandom.data = ctx->serverRandom;
    serverRandom.length = SSL_CLIENT_SRVR_RAND_SIZE;

    switch (sigAlg.hash) {
        case SSL_HashAlgorithmSHA1:
            hashRef = &SSLHashSHA1;
            algId.algorithm = CSSMOID_SHA1WithRSA;
            break;
        case SSL_HashAlgorithmSHA256:
            hashRef = &SSLHashSHA256;
            algId.algorithm = CSSMOID_SHA256WithRSA;
            break;
        case SSL_HashAlgorithmSHA384:
            hashRef = &SSLHashSHA384;
            algId.algorithm = CSSMOID_SHA384WithRSA;
            break;
        default:
            sslErrorLog("SSLVerifySignedServerKeyExchangeTls12: unsupported hash %d\n", sigAlg.hash);
            return errSSLProtocol;
    }


    dataToSign = hashes;
    dataToSignLen = hashRef->digestSize;
    hashOut.data = hashes;
    hashOut.length = hashRef->digestSize;

    if ((err = ReadyHash(hashRef, &hashCtx)) != 0)
        goto fail;
    if ((err = hashRef->update(&hashCtx, &clientRandom)) != 0)
        goto fail;
    if ((err = hashRef->update(&hashCtx, &serverRandom)) != 0)
        goto fail;
    if ((err = hashRef->update(&hashCtx, &signedParams)) != 0)
        goto fail;
    if ((err = hashRef->final(&hashCtx, &hashOut)) != 0)
        goto fail;

    if(sigAlg.signature==SSL_SignatureAlgorithmRSA) {
        err = sslRsaVerify(ctx,
                           ctx->peerPubKey,
                           &algId,
                           dataToSign,
                           dataToSignLen,
                           signature,
                           signatureLen);
    } else {
        err = sslRawVerify(ctx,
                           ctx->peerPubKey,
                           dataToSign,				/* plaintext */
                           dataToSignLen,			/* plaintext length */
                           signature,
                           signatureLen);
	}

    if(err) {
		sslErrorLog("SSLDecodeSignedServerKeyExchangeTls12: sslRawVerify "
                    "returned %d\n", (int)err);
		goto fail;
	}

fail:
    SSLFreeBuffer(&signedHashes);
    SSLFreeBuffer(&hashCtx);
    return err;

}

/*
 * Decode and verify a server key exchange message signed by server's
 * public key.
 */
static OSStatus
SSLDecodeSignedServerKeyExchange(SSLBuffer message, SSLContext *ctx)
{
	OSStatus        err;
    UInt16          modulusLen = 0, exponentLen = 0, signatureLen;
    uint8_t         *modulus = NULL, *exponent = NULL, *signature;
	bool			isRsa = true;

	assert(ctx->protocolSide == kSSLClientSide);

    if (message.length < 2) {
    	sslErrorLog("SSLDecodeSignedServerKeyExchange: msg len error 1\n");
        return errSSLProtocol;
    }

	/* first extract the key-exchange-method-specific parameters */
    uint8_t *charPtr = message.data;
	uint8_t *endCp = charPtr + message.length;
	switch(ctx->selectedCipherSpecParams.keyExchangeMethod) {
		case SSL_RSA:
        case SSL_RSA_EXPORT:
			modulusLen = SSLDecodeInt(charPtr, 2);
			charPtr += 2;
			if((charPtr + modulusLen) > endCp) {
				sslErrorLog("signedServerKeyExchange: msg len error 2\n");
				return errSSLProtocol;
			}
			modulus = charPtr;
			charPtr += modulusLen;

			exponentLen = SSLDecodeInt(charPtr, 2);
			charPtr += 2;
			if((charPtr + exponentLen) > endCp) {
				sslErrorLog("signedServerKeyExchange: msg len error 3\n");
				return errSSLProtocol;
			}
			exponent = charPtr;
			charPtr += exponentLen;
			break;
#if APPLE_DH
		case SSL_DHE_DSS:
		case SSL_DHE_DSS_EXPORT:
			isRsa = false;
			/* and fall through */
		case SSL_DHE_RSA:
		case SSL_DHE_RSA_EXPORT:
			err = SSLDecodeDHKeyParams(ctx, &charPtr, message.length);
			if(err) {
				return err;
			}
			break;
		#endif	/* APPLE_DH */

		case SSL_ECDHE_ECDSA:
			isRsa = false;
			/* and fall through */
		case SSL_ECDHE_RSA:
			err = SSLDecodeECDHKeyParams(ctx, &charPtr, message.length);
			if(err) {
				return err;
			}
			break;
		default:
			assert(0);
			return errSSLInternal;
	}

	/* this is what's hashed */
	SSLBuffer signedParams;
	signedParams.data = message.data;
	signedParams.length = charPtr - message.data;

    SSLSignatureAndHashAlgorithm sigAlg;

    if (sslVersionIsLikeTls12(ctx)) {
        /* Parse the algorithm field added in TLS1.2 */
        if((charPtr + 2) > endCp) {
            sslErrorLog("signedServerKeyExchange: msg len error 499\n");
            return errSSLProtocol;
        }
        sigAlg.hash = *charPtr++;
        sigAlg.signature = *charPtr++;
    }

	signatureLen = SSLDecodeInt(charPtr, 2);
	charPtr += 2;
	if((charPtr + signatureLen) != endCp) {
		sslErrorLog("signedServerKeyExchange: msg len error 4\n");
		return errSSLProtocol;
	}
	signature = charPtr;

    if (sslVersionIsLikeTls12(ctx))
    {
        err = SSLVerifySignedServerKeyExchangeTls12(ctx, sigAlg, signedParams,
                                                    signature, signatureLen);
    } else {
        err = SSLVerifySignedServerKeyExchange(ctx, isRsa, signedParams,
                                               signature, signatureLen);
    }

    if(err)
        goto fail;

	/* Signature matches; now replace server key with new key (RSA only) */
	switch(ctx->selectedCipherSpecParams.keyExchangeMethod) {
		case SSL_RSA:
        case SSL_RSA_EXPORT:
		{
			SSLBuffer modBuf;
			SSLBuffer expBuf;

			/* first free existing peerKey */
			sslFreePubKey(&ctx->peerPubKey);					/* no KCItem */

			/* and cook up a new one from raw bits */
			modBuf.data = modulus;
			modBuf.length = modulusLen;
			expBuf.data = exponent;
			expBuf.length = exponentLen;
			err = sslGetPubKeyFromBits(ctx,
				&modBuf,
				&expBuf,
				&ctx->peerPubKey);
			break;
		}
		case SSL_DHE_RSA:
		case SSL_DHE_RSA_EXPORT:
		case SSL_DHE_DSS:
		case SSL_DHE_DSS_EXPORT:
		case SSL_ECDHE_ECDSA:
		case SSL_ECDHE_RSA:
			break;					/* handled above */
		default:
			assert(0);
	}
fail:
    return err;
}

static OSStatus
SSLDecodeRSAKeyExchange(SSLBuffer keyExchange, SSLContext *ctx)
{   OSStatus            err;
    size_t        		outputLen, localKeyModulusLen;
    SSLProtocolVersion  version;
    Boolean				useEncryptKey = false;
	uint8_t				*src = NULL;
	SSLPrivKey			*keyRef = NULL;

	assert(ctx->protocolSide == kSSLServerSide);
    if (ctx->encryptPrivKeyRef) {
		useEncryptKey = true;
    }
	if (useEncryptKey) {
		keyRef  = ctx->encryptPrivKeyRef;
		/* FIXME: when 3420180 is implemented, pick appropriate creds here */
	}
	else {
		keyRef  = ctx->signingPrivKeyRef;
		/* FIXME: when 3420180 is implemented, pick appropriate creds here */
	}

	localKeyModulusLen = sslPrivKeyLengthInBytes(keyRef);
	if (localKeyModulusLen == 0) {
		sslErrorLog("SSLDecodeRSAKeyExchange: private key modulus is 0\n");
		return errSSLCrypto;
	}

	/*
	 * We have to tolerate incoming key exchange msgs with and without the
	 * two-byte "encrypted length" field.
	 */
    if (keyExchange.length == localKeyModulusLen) {
		/* no length encoded */
		src = keyExchange.data;
	}
	else if((keyExchange.length == (localKeyModulusLen + 2)) &&
		(ctx->negProtocolVersion >= TLS_Version_1_0)) {
		/* TLS only - skip the length bytes */
		src = keyExchange.data + 2;
	}
	else {
    	sslErrorLog("SSLDecodeRSAKeyExchange: length error (exp %u got %u)\n",
			(unsigned)localKeyModulusLen, (unsigned)keyExchange.length);
        return errSSLProtocol;
	}
    err = SSLAllocBuffer(&ctx->preMasterSecret, SSL_RSA_PREMASTER_SECRET_SIZE);
	if(err != 0) {
        return err;
	}

	/*
	 * From this point on, to defend against the Bleichenbacher attack
	 * and its Klima-Pokorny-Rosa variant, any errors we detect are *not*
	 * reported to the caller or the peer. If we detect any error during
	 * decryption (e.g., bad PKCS1 padding) or in the testing of the version
	 * number in the premaster secret, we proceed by generating a random
	 * premaster secret, with the correct version number, and tell our caller
	 * that everything is fine. This session will fail as soon as the
	 * finished messages are sent, since we will be using a bogus premaster
	 * secret (and hence bogus session and MAC keys). Meanwhile we have
	 * not provided any side channel information relating to the cause of
	 * the failure.
	 *
	 * See http://eprint.iacr.org/2003/052/ for more info.
	 */
	err = sslRsaDecrypt(ctx,
		keyRef,
#if USE_CDSA_CRYPTO
		CSSM_PADDING_PKCS1,
#else
		kSecPaddingPKCS1,
#endif
		src,
		localKeyModulusLen,				// ciphertext len
		ctx->preMasterSecret.data,
		SSL_RSA_PREMASTER_SECRET_SIZE,	// plaintext buf available
		&outputLen);

	if(err != errSecSuccess) {
		/* possible Bleichenbacher attack */
		sslLogNegotiateDebug("SSLDecodeRSAKeyExchange: RSA decrypt fail");
	}
	else if(outputLen != SSL_RSA_PREMASTER_SECRET_SIZE) {
		sslLogNegotiateDebug("SSLDecodeRSAKeyExchange: premaster secret size error");
    	err = errSSLProtocol;							// not passed back to caller
    }

	if(err == errSecSuccess) {
		/*
		 * Two legal values here - the one we actually negotiated (which is
		 * technically incorrect but not uncommon), and the one the client
		 * sent as its preferred version in the client hello msg.
		 */
		version = (SSLProtocolVersion)SSLDecodeInt(ctx->preMasterSecret.data, 2);
		if((version != ctx->negProtocolVersion) &&
		   (version != ctx->clientReqProtocol)) {
			/* possible Klima-Pokorny-Rosa attack */
			sslLogNegotiateDebug("SSLDecodeRSAKeyExchange: version error");
			err = errSSLProtocol;
		}
    }
	if(err != errSecSuccess) {
		/*
		 * Obfuscate failures for defense against Bleichenbacher and
		 * Klima-Pokorny-Rosa attacks.
		 */
		SSLEncodeInt(ctx->preMasterSecret.data, ctx->negProtocolVersion, 2);
		SSLBuffer tmpBuf;
		tmpBuf.data   = ctx->preMasterSecret.data + 2;
		tmpBuf.length = SSL_RSA_PREMASTER_SECRET_SIZE - 2;
		/* must ignore failures here */
		sslRand(&tmpBuf);
	}

	/* in any case, save premaster secret (good or bogus) and proceed */
    return errSecSuccess;
}

static OSStatus
SSLEncodeRSAKeyExchange(SSLRecord *keyExchange, SSLContext *ctx)
{   OSStatus            err;
    size_t        		outputLen, peerKeyModulusLen;
    size_t				bufLen;
	uint8_t				*dst;
	bool				encodeLen = false;
    uint8_t             *p;
    int                 head;
    size_t              msglen;

	assert(ctx->protocolSide == kSSLClientSide);
    if ((err = SSLEncodeRSAPremasterSecret(ctx)) != 0)
        return err;

    keyExchange->contentType = SSL_RecordTypeHandshake;
	assert(ctx->negProtocolVersion >= SSL_Version_3_0);
    keyExchange->protocolVersion = ctx->negProtocolVersion;

    peerKeyModulusLen = sslPubKeyLengthInBytes(ctx->peerPubKey);
	if (peerKeyModulusLen == 0) {
		sslErrorLog("SSLEncodeRSAKeyExchange: peer key modulus is 0\n");
		/* FIXME: we don't return an error here... is this condition ever expected? */
	}
#if SSL_DEBUG
	sslDebugLog("SSLEncodeRSAKeyExchange: peer key modulus length = %lu\n", peerKeyModulusLen);
#endif
    msglen = peerKeyModulusLen;
	#if 	RSA_CLIENT_KEY_ADD_LENGTH
	if(ctx->negProtocolVersion >= TLS_Version_1_0) {
        msglen += 2;
		encodeLen = true;
	}
	#endif
    head = SSLHandshakeHeaderSize(keyExchange);
    bufLen = msglen + head;
    if ((err = SSLAllocBuffer(&keyExchange->contents, 
		bufLen)) != 0)
    {   
        return err;
    }
	dst = keyExchange->contents.data + head;
	if(encodeLen) {
		dst += 2;
	}

	/* FIXME: can this line be removed? */
    p = keyExchange->contents.data;

    p = SSLEncodeHandshakeHeader(ctx, keyExchange, SSL_HdskClientKeyExchange, msglen);

	if(encodeLen) {
		/* the length of the encrypted pre_master_secret */
		SSLEncodeSize(keyExchange->contents.data + head,
			peerKeyModulusLen, 2);
	}
	err = sslRsaEncrypt(ctx,
		ctx->peerPubKey,
#if USE_CDSA_CRYPTO
		CSSM_PADDING_PKCS1,
#else
		kSecPaddingPKCS1,
#endif
		ctx->preMasterSecret.data,
		SSL_RSA_PREMASTER_SECRET_SIZE,
		dst,
		peerKeyModulusLen,
		&outputLen);
	if(err) {
		sslErrorLog("SSLEncodeRSAKeyExchange: error %d\n", (int)err);
		return err;
	}

    assert(outputLen == (encodeLen ? msglen - 2 : msglen));

    return errSecSuccess;
}


#if APPLE_DH

// MARK: -
// MARK: Diffie-Hellman Key Exchange

/*
 * Diffie-Hellman setup, server side. On successful return, the
 * following SSLContext members are valid:
 *
 *		dhParamsPrime
 *		dhParamsGenerator
 *		dhPrivate
 *		dhExchangePublic
 */
static OSStatus
SSLGenServerDHParamsAndKey(
	SSLContext *ctx)
{
	OSStatus ortn;
    assert(ctx->protocolSide == kSSLServerSide);


    /*
     * Obtain D-H parameters if we don't have them.
     */
    if(ctx->dhParamsEncoded.data == NULL) {
        /* TODO: Pick appropriate group based on cipher suite */
        ccdh_const_gp_t gp = ccdh_gp_rfc5114_MODP_2048_256();
        cc_size n = ccdh_gp_n(gp);
        size_t s = ccdh_gp_prime_size(gp);
        uint8_t p[s];
        uint8_t g[s];

        ccn_write_uint(n, ccdh_gp_prime(gp), s, p);
        ccn_write_uint(n, ccdh_gp_g(gp), s, g);

        const SSLBuffer	prime = {
            .data = p,
            .length = s,
        };
        const SSLBuffer	generator = {
            .data = g,
            .length = s,
        };

        ortn=sslEncodeDhParams(&ctx->dhParamsEncoded,			/* data mallocd and RETURNED PKCS-3 encoded */
                               &prime,			/* Wire format */
                               &generator);     /* Wire format */

        if(ortn)
            return ortn;
    }

#if USE_CDSA_CRYPTO
	/* generate per-session D-H key pair */
	sslFreeKey(ctx->cspHand, &ctx->dhPrivate, NULL);
	SSLFreeBuffer(&ctx->dhExchangePublic);
	ctx->dhPrivate = (CSSM_KEY *)sslMalloc(sizeof(CSSM_KEY));
	CSSM_KEY pubKey;
	ortn = sslDhGenerateKeyPair(ctx,
		&ctx->dhParamsEncoded,
		ctx->dhParamsPrime.length * 8,
		&pubKey, ctx->dhPrivate);
	if(ortn) {
		return ortn;
	}
	CSSM_TO_SSLBUF(&pubKey.KeyData, &ctx->dhExchangePublic);
#else
    if (!ctx->secDHContext) {
        ortn = sslDhCreateKey(ctx);
        if(ortn)
            return ortn;
    }
    return sslDhGenerateKeyPair(ctx);
#endif
	return errSecSuccess;
}

/*
 * size of DH param and public key, in wire format
 */
static size_t
SSLEncodedDHKeyParamsLen(SSLContext *ctx)
{
    SSLBuffer prime;
    SSLBuffer generator;

    sslDecodeDhParams(&ctx->dhParamsEncoded, &prime, &generator);

    return (2+prime.length+2+generator.length+2+ctx->dhExchangePublic.length);
}

/*
 * Encode DH params and public key, in wire format, in caller-supplied buffer.
 */
static OSStatus
SSLEncodeDHKeyParams(
	SSLContext *ctx,
	uint8_t *charPtr)
{
    assert(ctx->protocolSide == kSSLServerSide);
	assert(ctx->dhParamsEncoded.data != NULL);
	assert(ctx->dhExchangePublic.data != NULL);

    SSLBuffer prime;
    SSLBuffer generator;

    sslDecodeDhParams(&ctx->dhParamsEncoded, &prime, &generator);

	charPtr = SSLEncodeInt(charPtr, prime.length, 2);
	memcpy(charPtr, prime.data, prime.length);
	charPtr += prime.length;

	charPtr = SSLEncodeInt(charPtr, generator.length, 2);
	memcpy(charPtr, generator.data,
		generator.length);
	charPtr += generator.length;

    /* TODO: hum.... sounds like this one should be in the SecDHContext */
	charPtr = SSLEncodeInt(charPtr, ctx->dhExchangePublic.length, 2);
	memcpy(charPtr, ctx->dhExchangePublic.data,
		ctx->dhExchangePublic.length);

	dumpBuf("server prime", &prime);
	dumpBuf("server generator", &generator);
	dumpBuf("server pub key", &ctx->dhExchangePublic);

	return errSecSuccess;
}

/*
 * Decode DH params and server public key.
 */
static OSStatus
SSLDecodeDHKeyParams(
	SSLContext *ctx,
	uint8_t **charPtr,		// IN/OUT
	size_t length)
{
	OSStatus        err = errSecSuccess;
    SSLBuffer       prime;
    SSLBuffer       generator;

	assert(ctx->protocolSide == kSSLClientSide);
    uint8_t *endCp = *charPtr + length;

	/* Allow reuse via renegotiation */
	SSLFreeBuffer(&ctx->dhPeerPublic);
	
	/* Prime, with a two-byte length */
	UInt32 len = SSLDecodeInt(*charPtr, 2);
	(*charPtr) += 2;
	if((*charPtr + len) > endCp) {
		return errSSLProtocol;
	}

	prime.data = *charPtr;
    prime.length = len;

	(*charPtr) += len;

	/* Generator, with a two-byte length */
	len = SSLDecodeInt(*charPtr, 2);
	(*charPtr) += 2;
	if((*charPtr + len) > endCp) {
		return errSSLProtocol;
	}

    generator.data = *charPtr;
    generator.length = len;

    (*charPtr) += len;

    sslEncodeDhParams(&ctx->dhParamsEncoded, &prime, &generator);

	/* peer public key, with a two-byte length */
	len = SSLDecodeInt(*charPtr, 2);
	(*charPtr) += 2;
	err = SSLAllocBuffer(&ctx->dhPeerPublic, len);
	if(err) {
		return err;
	}
	memmove(ctx->dhPeerPublic.data, *charPtr, len);
	(*charPtr) += len;

	dumpBuf("client peer pub", &ctx->dhPeerPublic);
    //	dumpBuf("client prime", &ctx->dhParamsPrime);
	//  dumpBuf("client generator", &ctx->dhParamsGenerator);

	return err;
}

/*
 * Given the server's Diffie-Hellman parameters, generate our
 * own DH key pair, and perform key exchange using the server's
 * public key and our private key. The result is the premaster
 * secret.
 *
 * SSLContext members valid on entry:
 *		dhParamsPrime
 *		dhParamsGenerator
 *		dhPeerPublic
 *
 * SSLContext members valid on successful return:
 *		dhPrivate
 *		dhExchangePublic
 *		preMasterSecret
 */
static OSStatus
SSLGenClientDHKeyAndExchange(SSLContext *ctx)
{
	OSStatus            ortn;

#if USE_CDSA_CRYPTO

	if((ctx->dhParamsPrime.data == NULL) ||
	   (ctx->dhParamsGenerator.data == NULL) ||
	   (ctx->dhPeerPublic.data == NULL)) {
	   sslErrorLog("SSLGenClientDHKeyAndExchange: incomplete server params\n");
	   return errSSLProtocol;
	}

    /* generate two keys */
	CSSM_KEY pubKey;
	ctx->dhPrivate = (CSSM_KEY *)sslMalloc(sizeof(CSSM_KEY));
	ortn = sslDhGenKeyPairClient(ctx,
		&ctx->dhParamsPrime,
		&ctx->dhParamsGenerator,
		&pubKey, ctx->dhPrivate);
	if(ortn) {
		sslFree(ctx->dhPrivate);
		ctx->dhPrivate = NULL;
		return ortn;
	}

	/* do the exchange, size of prime */
	ortn = sslDhKeyExchange(ctx, ctx->dhParamsPrime.length * 8,
		&ctx->preMasterSecret);
	if(ortn) {
		return ortn;
	}
	CSSM_TO_SSLBUF(&pubKey.KeyData, &ctx->dhExchangePublic);
#else
    ortn=errSSLProtocol;
    require(ctx->dhParamsEncoded.data, out);
    require_noerr(ortn = sslDhCreateKey(ctx), out);
    require_noerr(ortn = sslDhGenerateKeyPair(ctx), out);
    require_noerr(ortn = sslDhKeyExchange(ctx), out);
out:
#endif
	return ortn;
}


static OSStatus
SSLEncodeDHanonServerKeyExchange(SSLRecord *keyExch, SSLContext *ctx)
{
	OSStatus            ortn = errSecSuccess;
    int                 head;

	assert(ctx->negProtocolVersion >= SSL_Version_3_0);
	assert(ctx->protocolSide == kSSLServerSide);

	/*
	 * Obtain D-H parameters (if we don't have them) and a key pair.
	 */
	ortn = SSLGenServerDHParamsAndKey(ctx);
	if(ortn) {
		return ortn;
	}

	size_t length = SSLEncodedDHKeyParamsLen(ctx);

	keyExch->protocolVersion = ctx->negProtocolVersion;
	keyExch->contentType = SSL_RecordTypeHandshake;
    head = SSLHandshakeHeaderSize(keyExch);
	if ((ortn = SSLAllocBuffer(&keyExch->contents, length+head)))
		return ortn;

    uint8_t *charPtr = SSLEncodeHandshakeHeader(ctx, keyExch, SSL_HdskServerKeyExchange, length);

	/* encode prime, generator, our public key */
	return SSLEncodeDHKeyParams(ctx, charPtr);
}

static OSStatus
SSLDecodeDHanonServerKeyExchange(SSLBuffer message, SSLContext *ctx)
{
	OSStatus        err = errSecSuccess;

	assert(ctx->protocolSide == kSSLClientSide);
    if (message.length < 6) {
    	sslErrorLog("SSLDecodeDHanonServerKeyExchange error: msg len %u\n",
    		(unsigned)message.length);
        return errSSLProtocol;
    }
    uint8_t *charPtr = message.data;
	err = SSLDecodeDHKeyParams(ctx, &charPtr, message.length);
	if(err == errSecSuccess) {
		if((message.data + message.length) != charPtr) {
			err = errSSLProtocol;
		}
	}
	return err;
}

static OSStatus
SSLDecodeDHClientKeyExchange(SSLBuffer keyExchange, SSLContext *ctx)
{
	OSStatus        ortn = errSecSuccess;
    unsigned int    publicLen;

	assert(ctx->protocolSide == kSSLServerSide);
	if(ctx->dhParamsEncoded.data == NULL) {
		/* should never happen */
		assert(0);
		return errSSLInternal;
	}

	/* this message simply contains the client's public DH key */
	uint8_t *charPtr = keyExchange.data;
    publicLen = SSLDecodeInt(charPtr, 2);
	charPtr += 2;
    /* TODO : Check the len here ? Will fail in sslDhKeyExchange anyway */
    /*
	if((keyExchange.length != publicLen + 2) ||
	   (publicLen > ctx->dhParamsPrime.length)) {
        return errSSLProtocol;
    }
    */
	SSLFreeBuffer(&ctx->dhPeerPublic);	// allow reuse via renegotiation
	ortn = SSLAllocBuffer(&ctx->dhPeerPublic, publicLen);
	if(ortn) {
		return ortn;
	}
	memmove(ctx->dhPeerPublic.data, charPtr, publicLen);

	/* DH Key exchange, result --> premaster secret */
	SSLFreeBuffer(&ctx->preMasterSecret);
#if USE_CDSA_CRYPTO
	ortn = sslDhKeyExchange(ctx, ctx->dhParamsPrime.length * 8,
		&ctx->preMasterSecret);
#else
    ortn = sslDhKeyExchange(ctx);
#endif
	dumpBuf("server peer pub", &ctx->dhPeerPublic);
	dumpBuf("server premaster", &ctx->preMasterSecret);
	return ortn;
}

static OSStatus
SSLEncodeDHClientKeyExchange(SSLRecord *keyExchange, SSLContext *ctx)
{   OSStatus            err;
    size_t				outputLen;
    int                 head;

	assert(ctx->protocolSide == kSSLClientSide);
	assert(ctx->negProtocolVersion >= SSL_Version_3_0);

    keyExchange->contentType = SSL_RecordTypeHandshake;
    keyExchange->protocolVersion = ctx->negProtocolVersion;

    if ((err = SSLGenClientDHKeyAndExchange(ctx)) != 0)
        return err;

    outputLen = ctx->dhExchangePublic.length + 2;
    head = SSLHandshakeHeaderSize(keyExchange);
    if ((err = SSLAllocBuffer(&keyExchange->contents,outputLen + head)))
        return err;

    uint8_t *charPtr = SSLEncodeHandshakeHeader(ctx, keyExchange, SSL_HdskClientKeyExchange, outputLen);

    charPtr = SSLEncodeSize(charPtr, ctx->dhExchangePublic.length, 2);
    memcpy(charPtr, ctx->dhExchangePublic.data, ctx->dhExchangePublic.length);

	dumpBuf("client pub key", &ctx->dhExchangePublic);
	dumpBuf("client premaster", &ctx->preMasterSecret);

    return errSecSuccess;
}

#endif	/* APPLE_DH */

// MARK: -
// MARK: ECDSA Key Exchange

/*
 * Given the server's ECDH curve params and public key, generate our
 * own ECDH key pair, and perform key exchange using the server's
 * public key and our private key. The result is the premaster
 * secret.
 *
 * SSLContext members valid on entry:
 *      if keyExchangeMethod == SSL_ECDHE_ECDSA or SSL_ECDHE_RSA:
 *			ecdhPeerPublic
 *			ecdhPeerCurve
 *		if keyExchangeMethod == SSL_ECDH_ECDSA or SSL_ECDH_RSA:
 *			peerPubKey, from which we infer ecdhPeerCurve
 *
 * SSLContext members valid on successful return:
 *		ecdhPrivate
 *		ecdhExchangePublic
 *		preMasterSecret
 */
static OSStatus
SSLGenClientECDHKeyAndExchange(SSLContext *ctx)
{
	OSStatus            ortn;

    assert(ctx->protocolSide == kSSLClientSide);

	switch(ctx->selectedCipherSpecParams.keyExchangeMethod) {
		case SSL_ECDHE_ECDSA:
		case SSL_ECDHE_RSA:
			/* Server sent us an ephemeral key with peer curve specified */
			if(ctx->ecdhPeerPublic.data == NULL) {
			   sslErrorLog("SSLGenClientECDHKeyAndExchange: incomplete server params\n");
			   return errSSLProtocol;
			}
			break;
		case SSL_ECDH_ECDSA:
		case SSL_ECDH_RSA:
		{
			/* No server key exchange; we have to get the curve from the key */
			if(ctx->peerPubKey == NULL) {
			   sslErrorLog("SSLGenClientECDHKeyAndExchange: no peer key\n");
			   return errSSLInternal;
			}

			/* The peer curve is in the key's CSSM_X509_ALGORITHM_IDENTIFIER... */
			ortn = sslEcdsaPeerCurve(ctx->peerPubKey, &ctx->ecdhPeerCurve);
			if(ortn) {
				return ortn;
			}
			sslEcdsaDebug("SSLGenClientECDHKeyAndExchange: derived peerCurve %u",
				(unsigned)ctx->ecdhPeerCurve);
			break;
		}
		default:
			/* shouldn't be here */
			assert(0);
			return errSSLInternal;
	}

    /* Generate our (ephemeral) pair, or extract it from our signing identity */
	if((ctx->negAuthType == SSLClientAuth_RSAFixedECDH) ||
	   (ctx->negAuthType == SSLClientAuth_ECDSAFixedECDH)) {
		/*
		 * Client auth with a fixed ECDH key in the cert. Convert private key
		 * from SecKeyRef to CSSM format. We don't need ecdhExchangePublic
		 * because the server gets that from our cert.
		 */
		assert(ctx->signingPrivKeyRef != NULL);
#if USE_CDSA_CRYPTO
		//assert(ctx->cspHand != 0);
		sslFreeKey(ctx->cspHand, &ctx->ecdhPrivate, NULL);
		SSLFreeBuffer(&ctx->ecdhExchangePublic);
		ortn = SecKeyGetCSSMKey(ctx->signingPrivKeyRef, (const CSSM_KEY **)&ctx->ecdhPrivate);
		if(ortn) {
			return ortn;
		}
		ortn = SecKeyGetCSPHandle(ctx->signingPrivKeyRef, &ctx->ecdhPrivCspHand);
		if(ortn) {
			sslErrorLog("SSLGenClientECDHKeyAndExchange: SecKeyGetCSPHandle err %d\n",
				(int)ortn);
		}
#endif
		sslEcdsaDebug("+++ Extracted ECDH private key");
	}
	else {
		/* generate a new pair */
		ortn = sslEcdhGenerateKeyPair(ctx, ctx->ecdhPeerCurve);
		if(ortn) {
			return ortn;
		}
#if USE_CDSA_CRYPTO
		sslEcdsaDebug("+++ Generated %u bit (%u byte) ECDH key pair",
			(unsigned)ctx->ecdhPrivate->KeyHeader.LogicalKeySizeInBits,
			(unsigned)((ctx->ecdhPrivate->KeyHeader.LogicalKeySizeInBits + 7) / 8));
#endif
	}


	/* do the exchange --> premaster secret */
	ortn = sslEcdhKeyExchange(ctx, &ctx->preMasterSecret);
	if(ortn) {
		return ortn;
	}
	return errSecSuccess;
}


/*
 * Decode ECDH params and server public key.
 */
static OSStatus
SSLDecodeECDHKeyParams(
	SSLContext *ctx,
	uint8_t **charPtr,		// IN/OUT
	size_t length)
{
	OSStatus        err = errSecSuccess;

	sslEcdsaDebug("+++ Decoding ECDH Server Key Exchange");

	assert(ctx->protocolSide == kSSLClientSide);
    uint8_t *endCp = *charPtr + length;

	/* Allow reuse via renegotiation */
	SSLFreeBuffer(&ctx->ecdhPeerPublic);

	/*** ECParameters - just a curveType and a named curve ***/

	/* 1-byte curveType, we only allow one type */
	uint8_t curveType = **charPtr;
	if(curveType != SSL_CurveTypeNamed) {
		sslEcdsaDebug("+++ SSLDecodeECDHKeyParams: Bad curveType (%u)\n", (unsigned)curveType);
		return errSSLProtocol;
	}
	(*charPtr)++;
	if(*charPtr > endCp) {
		return errSSLProtocol;
	}

	/* two-byte curve */
	ctx->ecdhPeerCurve = SSLDecodeInt(*charPtr, 2);
	(*charPtr) += 2;
	if(*charPtr > endCp) {
		return errSSLProtocol;
	}
	switch(ctx->ecdhPeerCurve) {
		case SSL_Curve_secp256r1:
		case SSL_Curve_secp384r1:
		case SSL_Curve_secp521r1:
			break;
		default:
			sslEcdsaDebug("+++ SSLDecodeECDHKeyParams: Bad curve (%u)\n",
				(unsigned)ctx->ecdhPeerCurve);
			return errSSLProtocol;
	}

	sslEcdsaDebug("+++ SSLDecodeECDHKeyParams: ecdhPeerCurve %u",
		(unsigned)ctx->ecdhPeerCurve);

	/*** peer public key as an ECPoint ***/

	/*
	 * The spec says the the max length of an ECPoint is 255 bytes, limiting
	 * this whole mechanism to a max modulus size of 1020 bits, which I find
	 * hard to believe...
	 */
	UInt32 len = SSLDecodeInt(*charPtr, 1);
	(*charPtr)++;
	if((*charPtr + len) > endCp) {
		return errSSLProtocol;
	}
	err = SSLAllocBuffer(&ctx->ecdhPeerPublic, len);
	if(err) {
		return err;
	}
	memmove(ctx->ecdhPeerPublic.data, *charPtr, len);
	(*charPtr) += len;

	dumpBuf("client peer pub", &ctx->ecdhPeerPublic);

	return err;
}


static OSStatus
SSLEncodeECDHClientKeyExchange(SSLRecord *keyExchange, SSLContext *ctx)
{   OSStatus            err;
    size_t				outputLen;
    int                 head;

	assert(ctx->protocolSide == kSSLClientSide);
    if ((err = SSLGenClientECDHKeyAndExchange(ctx)) != 0)
        return err;

	/*
	 * Per RFC 4492 5.7, if we're doing ECDSA_fixed_ECDH or RSA_fixed_ECDH
	 * client auth, we still send this message, but it's empty (because the
	 * server gets our public key from our cert).
	 */
	bool emptyMsg = false;
	switch(ctx->negAuthType) {
		case SSLClientAuth_RSAFixedECDH:
		case SSLClientAuth_ECDSAFixedECDH:
			emptyMsg = true;
			break;
		default:
			break;
	}
	if(emptyMsg) {
		outputLen = 0;
	}
	else {
		outputLen = ctx->ecdhExchangePublic.length + 1;
	}

    keyExchange->contentType = SSL_RecordTypeHandshake;
	assert(ctx->negProtocolVersion >= SSL_Version_3_0);
    keyExchange->protocolVersion = ctx->negProtocolVersion;
    head = SSLHandshakeHeaderSize(keyExchange);
    if ((err = SSLAllocBuffer(&keyExchange->contents,outputLen + head)))
        return err;

    uint8_t *charPtr = SSLEncodeHandshakeHeader(ctx, keyExchange, SSL_HdskClientKeyExchange, outputLen);
	if(emptyMsg) {
		sslEcdsaDebug("+++ Sending EMPTY ECDH Client Key Exchange");
	}
	else {
		/* just a 1-byte length here... */
		charPtr = SSLEncodeSize(charPtr, ctx->ecdhExchangePublic.length, 1);
		memcpy(charPtr, ctx->ecdhExchangePublic.data, ctx->ecdhExchangePublic.length);
		sslEcdsaDebug("+++ Encoded ECDH Client Key Exchange");
	}

	dumpBuf("client pub key", &ctx->ecdhExchangePublic);
	dumpBuf("client premaster", &ctx->preMasterSecret);
    return errSecSuccess;
}



static OSStatus
SSLDecodePSKClientKeyExchange(SSLBuffer keyExchange, SSLContext *ctx)
{
	OSStatus        ortn = errSecSuccess;
    unsigned int    identityLen;

	assert(ctx->protocolSide == kSSLServerSide);

	/* this message simply contains the client's PSK identity */
	uint8_t *charPtr = keyExchange.data;
    identityLen = SSLDecodeInt(charPtr, 2);
	charPtr += 2;

	SSLFreeBuffer(&ctx->pskIdentity);	// allow reuse via renegotiation
	ortn = SSLAllocBuffer(&ctx->pskIdentity, identityLen);
	if(ortn) {
		return ortn;
	}
	memmove(ctx->pskIdentity.data, charPtr, identityLen);

    /* TODO: At this point we know the identity of the PSK client,
      we should break out of the handshake, so we can select the appropriate
      PreShared secret. As this stands, the preshared secret needs to be known
      before the handshake starts. */

    size_t n=ctx->pskSharedSecret.length;

    if(n==0) return errSSLBadConfiguration;

    if ((ortn = SSLAllocBuffer(&ctx->preMasterSecret, 2*(n+2))) != 0)
        return ortn;

    uint8_t *p=ctx->preMasterSecret.data;

    p = SSLEncodeInt(p, n, 2);
    memset(p, 0, n); p+=n;
    p = SSLEncodeInt(p, n, 2);
    memcpy(p, ctx->pskSharedSecret.data, n);

    dumpBuf("server premaster (PSK)", &ctx->preMasterSecret);

	return ortn;
}


static OSStatus
SSLEncodePSKClientKeyExchange(SSLRecord *keyExchange, SSLContext *ctx)
{
    OSStatus            err;
    size_t				outputLen;
    int                 head;

	assert(ctx->protocolSide == kSSLClientSide);

	outputLen = ctx->pskIdentity.length+2;

    keyExchange->contentType = SSL_RecordTypeHandshake;
	assert(ctx->negProtocolVersion >= SSL_Version_3_0);
    keyExchange->protocolVersion = ctx->negProtocolVersion;
    head = SSLHandshakeHeaderSize(keyExchange);
    if ((err = SSLAllocBuffer(&keyExchange->contents,outputLen + head)))
        return err;

    uint8_t *charPtr = SSLEncodeHandshakeHeader(ctx, keyExchange, SSL_HdskClientKeyExchange, outputLen);

	charPtr = SSLEncodeSize(charPtr, ctx->pskIdentity.length, 2);
	memcpy(charPtr, ctx->pskIdentity.data, ctx->pskIdentity.length);


    /* We better have a pskSharedSecret already */
    size_t n=ctx->pskSharedSecret.length;

    if(n==0) return errSSLBadConfiguration;

    if ((err = SSLAllocBuffer(&ctx->preMasterSecret, 2*(n+2))) != 0)
        return err;

    uint8_t *p=ctx->preMasterSecret.data;

    p = SSLEncodeInt(p, n, 2);
    memset(p, 0, n); p+=n;
    p = SSLEncodeInt(p, n, 2);
    memcpy(p, ctx->pskSharedSecret.data, n);

    dumpBuf("client premaster (PSK)", &ctx->preMasterSecret);

    return errSecSuccess;
}


// MARK: -
// MARK: Public Functions
OSStatus
SSLEncodeServerKeyExchange(SSLRecord *keyExch, SSLContext *ctx)
{   OSStatus      err;
    
    switch (ctx->selectedCipherSpecParams.keyExchangeMethod)
    {   case SSL_RSA:
        case SSL_RSA_EXPORT:
        #if		APPLE_DH
		case SSL_DHE_RSA:
		case SSL_DHE_RSA_EXPORT:
		case SSL_DHE_DSS:
		case SSL_DHE_DSS_EXPORT:
		#endif	/* APPLE_DH */
            if ((err = SSLEncodeSignedServerKeyExchange(keyExch, ctx)) != 0)
                return err;
            break;
        #if		APPLE_DH
        case SSL_DH_anon:
		case SSL_DH_anon_EXPORT:
            if ((err = SSLEncodeDHanonServerKeyExchange(keyExch, ctx)) != 0)
                return err;
            break;
        #endif
        default:
            return errSecUnimplemented;
    }

    return errSecSuccess;
}

OSStatus
SSLProcessServerKeyExchange(SSLBuffer message, SSLContext *ctx)
{
	OSStatus      err;
    
    switch (ctx->selectedCipherSpecParams.keyExchangeMethod) {
		case SSL_RSA:
        case SSL_RSA_EXPORT:
        #if		APPLE_DH
		case SSL_DHE_RSA:
		case SSL_DHE_RSA_EXPORT:
		case SSL_DHE_DSS:
		case SSL_DHE_DSS_EXPORT:
		#endif
		case SSL_ECDHE_ECDSA:
		case SSL_ECDHE_RSA:
            err = SSLDecodeSignedServerKeyExchange(message, ctx);
            break;
        #if		APPLE_DH
        case SSL_DH_anon:
		case SSL_DH_anon_EXPORT:
            err = SSLDecodeDHanonServerKeyExchange(message, ctx);
            break;
        #endif
        default:
            err = errSecUnimplemented;
			break;
    }

    return err;
}

OSStatus
SSLEncodeKeyExchange(SSLRecord *keyExchange, SSLContext *ctx)
{   OSStatus      err;
    
    assert(ctx->protocolSide == kSSLClientSide);
    
    switch (ctx->selectedCipherSpecParams.keyExchangeMethod) {
		case SSL_RSA:
		case SSL_RSA_EXPORT:
			sslDebugLog("SSLEncodeKeyExchange: RSA method\n");
			err = SSLEncodeRSAKeyExchange(keyExchange, ctx);
			break;
#if		APPLE_DH
		case SSL_DHE_RSA:
		case SSL_DHE_RSA_EXPORT:
		case SSL_DHE_DSS:
		case SSL_DHE_DSS_EXPORT:
		case SSL_DH_anon:
		case SSL_DH_anon_EXPORT:
			sslDebugLog("SSLEncodeKeyExchange: DH method\n");
			err = SSLEncodeDHClientKeyExchange(keyExchange, ctx);
			break;
#endif
		case SSL_ECDH_ECDSA:
		case SSL_ECDHE_ECDSA:
		case SSL_ECDH_RSA:
		case SSL_ECDHE_RSA:
		case SSL_ECDH_anon:
			sslDebugLog("SSLEncodeKeyExchange: ECDH method\n");
			err = SSLEncodeECDHClientKeyExchange(keyExchange, ctx);
			break;
        case TLS_PSK:
            err = SSLEncodePSKClientKeyExchange(keyExchange, ctx);
            break;
		default:
			sslErrorLog("SSLEncodeKeyExchange: unknown method (%d)\n",
					ctx->selectedCipherSpecParams.keyExchangeMethod);
			err = errSecUnimplemented;
	}

	return err;
}

OSStatus
SSLProcessKeyExchange(SSLBuffer keyExchange, SSLContext *ctx)
{   OSStatus      err;
    
    switch (ctx->selectedCipherSpecParams.keyExchangeMethod)
    {   case SSL_RSA:
        case SSL_RSA_EXPORT:
            if ((err = SSLDecodeRSAKeyExchange(keyExchange, ctx)) != 0)
                return err;
            break;
		#if		APPLE_DH
        case SSL_DH_anon:
		case SSL_DHE_DSS:
		case SSL_DHE_DSS_EXPORT:
		case SSL_DHE_RSA:
		case SSL_DHE_RSA_EXPORT:
		case SSL_DH_anon_EXPORT:
			sslDebugLog("SSLProcessKeyExchange: processing DH key exchange (%d)\n",
					ctx->selectedCipherSpecParams.keyExchangeMethod);
			if ((err = SSLDecodeDHClientKeyExchange(keyExchange, ctx)) != 0)
				return err;
			break;
#endif
        case TLS_PSK:
			if ((err = SSLDecodePSKClientKeyExchange(keyExchange, ctx)) != 0)
				return err;
			break;
		default:
			sslErrorLog("SSLProcessKeyExchange: unknown keyExchangeMethod (%d)\n",
					ctx->selectedCipherSpecParams.keyExchangeMethod);
			return errSecUnimplemented;
	}

	return errSecSuccess;
}

OSStatus
SSLInitPendingCiphers(SSLContext *ctx)
{   OSStatus        err;
    SSLBuffer       key;
    int             keyDataLen;
        
    err = errSecSuccess;
    key.data = 0;

    keyDataLen = ctx->selectedCipherSpecParams.macSize +
                    ctx->selectedCipherSpecParams.keySize +
                    ctx->selectedCipherSpecParams.ivSize;
    keyDataLen *= 2;        /* two of everything */

    if ((err = SSLAllocBuffer(&key, keyDataLen)))
        return err;
	assert(ctx->sslTslCalls != NULL);
    if ((err = ctx->sslTslCalls->generateKeyMaterial(key, ctx)) != 0)
        goto fail;
    
    if((err = ctx->recFuncs->initPendingCiphers(ctx->recCtx, ctx->selectedCipher, (ctx->protocolSide==kSSLServerSide), key)) != 0)
        goto fail;
    
    ctx->writePending_ready = 1;
    ctx->readPending_ready = 1;
    
fail:
    SSLFreeBuffer(&key);
    return err;
}

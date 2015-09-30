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

#include "tls_handshake_priv.h"
#include "sslHandshake.h"
#include "sslMemory.h"
#include "sslDebug.h"
#include "sslUtils.h"
#include "sslDigests.h"
#include "sslCrypto.h"

#include <assert.h>
#include <string.h>

#include <corecrypto/ccdh_gp.h>

#if ALLOW_RSA_SERVER_KEY_EXCHANGE
#include <corecrypto/ccrsa.h>
/* extern struct ccrng_state *ccDRBGGetRngState(); */
#include <CommonCrypto/CommonRandomSPI.h>
#define CCRNGSTATE() ccDRBGGetRngState()
#endif

#include <AssertMacros.h>

#if APPLE_DH

static int SSLGenServerDHParamsAndKey(tls_handshake_t ctx);
static size_t SSLEncodedDHKeyParamsLen(tls_handshake_t ctx);
static int SSLEncodeDHKeyParams(tls_handshake_t ctx, uint8_t *charPtr);

static int SSLGenServerECDHParamsAndKey(tls_handshake_t ctx);
static int SSLEncodeECDHKeyParams(tls_handshake_t ctx, tls_buffer *params);


// MARK: -
// MARK: Forward Static Declarations

static int SSLDecodeDHKeyParams(tls_handshake_t ctx, uint8_t **charPtr,
	size_t length);
#endif

static int SSLDecodeECDHKeyParams(tls_handshake_t ctx, uint8_t **charPtr,
	size_t length);

#define DH_PARAM_DUMP		0
#if 	DH_PARAM_DUMP

#include <stdio.h>

static void dumpBuf(const char *name, tls_buffer *buf)
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
	tls_buffer		prime;
	tls_buffer		generator;
	/* this one for sending to the CSP at key gen time */
	tls_buffer		paramBlock;
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
#define RSA_CLIENT_KEY_ADD_LENGTH              1

static int
SSLEncodeRSAPremasterSecret(tls_handshake_t ctx)
{
    tls_buffer           randData;
    int            err;

    if ((err = SSLAllocBuffer(&ctx->preMasterSecret, 
			SSL_RSA_PREMASTER_SECRET_SIZE)) != 0)
        return err;

	assert(ctx->negProtocolVersion >= tls_protocol_version_SSL_3);

    SSLEncodeInt(ctx->preMasterSecret.data, ctx->clientReqProtocol, 2);
    randData.data = ctx->preMasterSecret.data+2;
    randData.length = SSL_RSA_PREMASTER_SECRET_SIZE - 2;
    if ((err = sslRand(&randData)) != 0)
        return err;
    return errSSLSuccess;
}


#if ALLOW_RSA_SERVER_KEY_EXCHANGE

// MARK: -
// MARK: RSA Server Key Exchange

/*
 * size of RSA public key, in wire format
 */
static size_t
SSLEncodedEphemeralRsaKeyLen(tls_handshake_t ctx)
{
    assert(ctx->isServer);
    assert(ctx->rsaEncryptPubKey.rsa.pub != NULL);

    cc_size n = ccrsa_ctx_n(ctx->rsaEncryptPubKey.rsa);

    size_t modlen = ccn_write_uint_size(n, ccrsa_ctx_m(ctx->rsaEncryptPubKey.rsa));
    size_t explen = ccn_write_uint_size(n, ccrsa_ctx_e(ctx->rsaEncryptPubKey.rsa));

    size_t len = (2+modlen+2+explen);

    return len;
}

/*
 * Encode RSA params and public key, in wire format, in caller-supplied buffer.
 */
static int
SSLEncodeEphemeralRsaKey(tls_handshake_t ctx, uint8_t *charPtr)
{
    assert(ctx->isServer);

    cc_size n = ccrsa_ctx_n(ctx->rsaEncryptPubKey.rsa);

    size_t modlen = ccn_write_uint_size(n, ccrsa_ctx_m(ctx->rsaEncryptPubKey.rsa));
    size_t explen = ccn_write_uint_size(n, ccrsa_ctx_e(ctx->rsaEncryptPubKey.rsa));

    charPtr = SSLEncodeInt(charPtr, modlen, 2);
    ccn_write_uint(n, ccrsa_ctx_m(ctx->rsaEncryptPubKey.rsa), modlen, charPtr);
    charPtr += modlen;

    charPtr = SSLEncodeInt(charPtr, explen, 2);
    ccn_write_uint(n, ccrsa_ctx_e(ctx->rsaEncryptPubKey.rsa), explen, charPtr);

    return errSSLSuccess;
}
#endif /* ALLOW_RSA_SERVER_KEY_EXCHANGE */


/*
 * Generate a server key exchange message signed by our RSA or DSA private key.
 */

static int
SSLSignServerKeyExchangeTls12(tls_handshake_t ctx, tls_signature_and_hash_algorithm sigAlg, tls_buffer exchangeParams, tls_buffer signature, size_t *actSigLen)
{
    int        err;
    tls_buffer       hashOut, hashCtx, clientRandom, serverRandom;
    uint8_t         hashes[SSL_MAX_DIGEST_LEN];
    tls_buffer       signedHashes;
    uint8_t			*dataToSign;
	size_t			dataToSignLen;
    const HashReference *hashRef;

    assert(ctx->signingPrivKeyRef);

	signedHashes.data = 0;
    hashCtx.data = 0;

    clientRandom.data = ctx->clientRandom;
    clientRandom.length = SSL_CLIENT_SRVR_RAND_SIZE;
    serverRandom.data = ctx->serverRandom;
    serverRandom.length = SSL_CLIENT_SRVR_RAND_SIZE;

    switch (sigAlg.hash) {
        case tls_hash_algorithm_SHA1:
            hashRef = &SSLHashSHA1;
            break;
        case tls_hash_algorithm_SHA256:
            hashRef = &SSLHashSHA256;
            break;
        case tls_hash_algorithm_SHA384:
            hashRef = &SSLHashSHA384;
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

    if(sigAlg.signature==tls_signature_algorithm_RSA) {
        err = sslRsaSign(ctx->signingPrivKeyRef,
                         sigAlg.hash,
                         dataToSign,
                         dataToSignLen,
                         signature.data,
                         signature.length,
                         actSigLen);
    } else {
        err = sslRawSign(ctx->signingPrivKeyRef,
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

static int
SSLSignServerKeyExchange(tls_handshake_t ctx, bool isRsa, tls_buffer exchangeParams, tls_buffer signature, size_t *actSigLen)
{
    int        err;
    uint8_t         hashes[SSL_SHA1_DIGEST_LEN + SSL_MD5_DIGEST_LEN];
    tls_buffer       clientRandom,serverRandom,hashCtx, hash;
	uint8_t			*dataToSign;
	size_t			dataToSignLen;

    assert(ctx->signingPrivKeyRef);

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

    err = sslRawSign(ctx->signingPrivKeyRef,
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
int FindSigAlg(tls_handshake_t ctx,
                    tls_signature_and_hash_algorithm *alg)
{
    unsigned i;

    assert(ctx->isServer);
    assert(ctx->negProtocolVersion >= tls_protocol_version_TLS_1_2);
    assert(!ctx->isDTLS);

    if((ctx->numClientSigAlgs==0) ||(ctx->clientSigAlgs==NULL))
        return errSSLInternal;

    switch (ctx->selectedCipherSpecParams.keyExchangeMethod) {
        case SSL_ECDHE_ECDSA:
            alg->signature = tls_signature_algorithm_ECDSA;
            break;
        case SSL_ECDHE_RSA:
        case SSL_RSA:
        case SSL_DHE_RSA:
            alg->signature = tls_signature_algorithm_RSA;
            break;
        default:
            return errSSLProtocol;
    }
    //Check for matching client signature algorithm and corresponding hash algorithm
    for(i=0; i<ctx->numClientSigAlgs; i++) {
        if (alg->signature == ctx->clientSigAlgs[i].signature) {
            alg->hash = ctx->clientSigAlgs[i].hash;
            //Let's only support SHA1 and SHA256. SHA384 does not work with 512 bits keys.
            if((alg->hash==tls_hash_algorithm_SHA1) || (alg->hash==tls_hash_algorithm_SHA256)) {
                return errSSLSuccess;
            }
        }
    }
    // We could not find a supported signature and hash algorithm
    return errSSLProtocol;
}

static int
SSLEncodeSignedServerKeyExchange(tls_buffer *keyExch, tls_handshake_t ctx)
{   int        err;
    uint8_t         *charPtr;
    size_t          outputLen;
	bool			isRsa = true;
    size_t 			maxSigLen;
    size_t	    	actSigLen;
	tls_buffer		signature;
    int             head = 4;
    tls_buffer       exchangeParams;

    assert(ctx->isServer);
 	assert(ctx->negProtocolVersion >= tls_protocol_version_SSL_3);
    exchangeParams.data = 0;
	signature.data = 0;


#if ENABLE_DTLS
    if(ctx->negProtocolVersion == tls_protocol_version_DTLS_1_0) {
        head+=8;
    }
#endif


	/* Set up parameter block to hash ==> exchangeParams */
	switch(ctx->selectedCipherSpecParams.keyExchangeMethod) {
#if APPLE_DH
		case SSL_DHE_RSA:
		{
            isRsa = true;
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
#if ALLOW_RSA_SERVER_KEY_EXCHANGE
        case SSL_RSA:
        {
            isRsa = true;
            size_t len = SSLEncodedEphemeralRsaKeyLen(ctx);
            err = SSLAllocBuffer(&exchangeParams, len);
            if(err) {
                goto fail;
            }
            err = SSLEncodeEphemeralRsaKey(ctx, exchangeParams.data);
            break;

        }
#endif
        case SSL_ECDHE_RSA:
            isRsa = true;
            require_noerr(err = SSLGenServerECDHParamsAndKey(ctx), fail);
            require_noerr(err = SSLEncodeECDHKeyParams(ctx, &exchangeParams), fail);
            break;
        case SSL_ECDHE_ECDSA:
            isRsa = false;
            require_noerr(err = SSLGenServerECDHParamsAndKey(ctx), fail);
            require_noerr(err = SSLEncodeECDHKeyParams(ctx, &exchangeParams), fail);
            break;

		default:
			/* shouldn't be here */
			assert(0);
			err = errSSLInternal;
	}

    if(err)
        goto fail;

    tls_signature_and_hash_algorithm sigAlg = {0,};


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
    if ((err = SSLAllocBuffer(keyExch, outputLen+head)) != 0)
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
		   (keyExch->data + keyExch->length));

    err = errSSLSuccess;

fail:
    SSLFreeBuffer(&exchangeParams);
    SSLFreeBuffer(&signature);
    return err;
}

static int
SSLVerifySignedServerKeyExchange(tls_handshake_t ctx, bool isRsa, tls_buffer signedParams,
                                 uint8_t *signature, UInt16 signatureLen)
{
    int        err;
    tls_buffer       hashOut, hashCtx, clientRandom, serverRandom;
    uint8_t         hashes[SSL_SHA1_DIGEST_LEN + SSL_MD5_DIGEST_LEN];
    tls_buffer       signedHashes;
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

    if(isRsa) {
		/* RSA - use MD5 + SHA1 hash */
		dataToSign = hashes;
		dataToSignLen = SSL_SHA1_DIGEST_LEN + SSL_MD5_DIGEST_LEN;
    } else {
		/* DSA, ECDSA - just use the SHA1 hash */
		dataToSign = &hashes[SSL_MD5_DIGEST_LEN];
		dataToSignLen = SSL_SHA1_DIGEST_LEN;
    }

	err = sslRawVerify(&ctx->peerPubKey,
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

static int
SSLVerifySignedServerKeyExchangeTls12(tls_handshake_t ctx, tls_signature_and_hash_algorithm sigAlg, tls_buffer signedParams,
                                 uint8_t *signature, UInt16 signatureLen)
{
    int        err;
    tls_buffer       hashOut, hashCtx, clientRandom, serverRandom;
    uint8_t         hashes[SSL_MAX_DIGEST_LEN];
    tls_buffer       signedHashes;
    uint8_t			*dataToSign;
	size_t			dataToSignLen;
    const HashReference *hashRef;

	signedHashes.data = 0;
    hashCtx.data = 0;

    clientRandom.data = ctx->clientRandom;
    clientRandom.length = SSL_CLIENT_SRVR_RAND_SIZE;
    serverRandom.data = ctx->serverRandom;
    serverRandom.length = SSL_CLIENT_SRVR_RAND_SIZE;

    switch (sigAlg.hash) {
        case tls_hash_algorithm_SHA1:
            hashRef = &SSLHashSHA1;
            break;
        case tls_hash_algorithm_SHA256:
            hashRef = &SSLHashSHA256;
            break;
        case tls_hash_algorithm_SHA384:
            hashRef = &SSLHashSHA384;
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

    if(sigAlg.signature==tls_signature_algorithm_RSA) {
        err = sslRsaVerify(&ctx->peerPubKey,
                           sigAlg.hash,
                           dataToSign,
                           dataToSignLen,
                           signature,
                           signatureLen);
    } else {
        err = sslRawVerify(&ctx->peerPubKey,
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
static int
SSLDecodeSignedServerKeyExchange(tls_buffer message, tls_handshake_t ctx)
{
	int        err;
    UInt16          signatureLen;
    uint8_t         *signature;
	bool			isRsa = true;

	assert(!ctx->isServer);

    if (message.length < 2) {
    	sslErrorLog("SSLDecodeSignedServerKeyExchange: msg len error 1\n");
        return errSSLProtocol;
    }

	/* first extract the key-exchange-method-specific parameters */
    uint8_t *charPtr = message.data;
	uint8_t *endCp = charPtr + message.length;
	switch(ctx->selectedCipherSpecParams.keyExchangeMethod) {
#if APPLE_DH
		case SSL_DHE_RSA:
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
	tls_buffer signedParams;
	signedParams.data = message.data;
	signedParams.length = charPtr - message.data;

    tls_signature_and_hash_algorithm sigAlg;

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
		case SSL_DHE_RSA:
		case SSL_ECDHE_ECDSA:
		case SSL_ECDHE_RSA:
			break;					/* handled above */
		default:
			assert(0);
	}
fail:
    return err;
}

static int
SSLDecodeRSAKeyExchange(tls_buffer keyExchange, tls_handshake_t ctx)
{   int            err;
    size_t        		outputLen, localKeyModulusLen;
    tls_protocol_version  version;
	uint8_t				*src = NULL;
	tls_private_key_t   keyRef = NULL;

	assert(ctx->isServer);

    keyRef  = ctx->signingPrivKeyRef;

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
		(ctx->negProtocolVersion >= tls_protocol_version_TLS_1_0)) {
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
	err = sslRsaDecrypt(keyRef,
		src,
		localKeyModulusLen,				// ciphertext len
		ctx->preMasterSecret.data,
		SSL_RSA_PREMASTER_SECRET_SIZE,	// plaintext buf available
		&outputLen);
	if(err != errSSLSuccess) {
		/* possible Bleichenbacher attack */
		sslLogNegotiateDebug("SSLDecodeRSAKeyExchange: RSA decrypt fail");
	}
	else if(outputLen != SSL_RSA_PREMASTER_SECRET_SIZE) {
		sslLogNegotiateDebug("SSLDecodeRSAKeyExchange: premaster secret size error");
    	err = errSSLProtocol;							// not passed back to caller
    }

	if(err == errSSLSuccess) {
		/*
		 * Two legal values here - the one we actually negotiated (which is
		 * technically incorrect but not uncommon), and the one the client
		 * sent as its preferred version in the client hello msg.
		 */
		version = (tls_protocol_version)SSLDecodeInt(ctx->preMasterSecret.data, 2);
		if((version != ctx->negProtocolVersion) &&
		   (version != ctx->clientReqProtocol)) {
			/* possible Klima-Pokorny-Rosa attack */
			sslLogNegotiateDebug("SSLDecodeRSAKeyExchange: version error");
			err = errSSLProtocol;
		}
    }
	if(err != errSSLSuccess) {
		/*
		 * Obfuscate failures for defense against Bleichenbacher and
		 * Klima-Pokorny-Rosa attacks.
		 */
		SSLEncodeInt(ctx->preMasterSecret.data, ctx->negProtocolVersion, 2);
		tls_buffer tmpBuf;
		tmpBuf.data   = ctx->preMasterSecret.data + 2;
		tmpBuf.length = SSL_RSA_PREMASTER_SECRET_SIZE - 2;
		/* must ignore failures here */
		sslRand(&tmpBuf);
	}

	/* in any case, save premaster secret (good or bogus) and proceed */
    return errSSLSuccess;
}

static int
SSLEncodeRSAKeyExchange(tls_buffer *keyExchange, tls_handshake_t ctx)
{   int            err;
    size_t        		outputLen, peerKeyModulusLen;
    size_t				bufLen;
	uint8_t				*dst;
	bool				encodeLen = false;
    int                 head;
    size_t              msglen;

	assert(!ctx->isServer);

    if(!ctx->peerPubKey.isRSA || ctx->peerPubKey.rsa.pub == NULL) {
        sslErrorLog("SSLEncodeRSAKeyExchange: no RSA peer pub key\n");
        return errSSLCrypto;
    }

    if ((err = SSLEncodeRSAPremasterSecret(ctx)) != 0)
        return err;

	assert(ctx->negProtocolVersion >= tls_protocol_version_SSL_3);

    peerKeyModulusLen = sslPubKeyLengthInBytes(&ctx->peerPubKey);
	if (peerKeyModulusLen == 0) {
		sslErrorLog("SSLEncodeRSAKeyExchange: peer key modulus is 0\n");
		/* FIXME: we don't return an error here... is this condition ever expected? */
	}
#if 0
	sslDebugLog("SSLEncodeRSAKeyExchange: peer key modulus length = %lu\n", peerKeyModulusLen);
#endif
    msglen = peerKeyModulusLen;
	#if 	RSA_CLIENT_KEY_ADD_LENGTH
	if(ctx->negProtocolVersion >= tls_protocol_version_TLS_1_0) {
        msglen += 2;
		encodeLen = true;
	}
	#endif
    head = SSLHandshakeHeaderSize(ctx);
    bufLen = msglen + head;
    if ((err = SSLAllocBuffer(keyExchange,
		bufLen)) != 0)
    {   
        return err;
    }
	dst = keyExchange->data + head;
	if(encodeLen) {
		dst += 2;
	}

    SSLEncodeHandshakeHeader(ctx, keyExchange, SSL_HdskClientKeyExchange, msglen);

	if(encodeLen) {
		/* the length of the encrypted pre_master_secret */
		SSLEncodeSize(keyExchange->data + head,
			peerKeyModulusLen, 2);
	}
	err = sslRsaEncrypt(&ctx->peerPubKey,
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

    return errSSLSuccess;
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
static int
SSLGenServerDHParamsAndKey(
	tls_handshake_t ctx)
{
    ccdh_const_gp_t gp;
    assert(ctx->isServer);

    /*
     * Obtain D-H parameters if we don't have them.
     */
    if(ctx->dhParams.gp == NULL) {
        /* TODO: Pick appropriate group based on cipher suite */
        gp = ccdh_gp_rfc5114_MODP_2048_256();
    } else {
        gp._ncgp = ctx->dhParams;
    }

    return sslDhCreateKey(gp, &ctx->dhContext);
}

/*
 * size of DH param and public key, in wire format
 */
static size_t
SSLEncodedDHKeyParamsLen(tls_handshake_t ctx)
{
    assert(ctx->isServer);
    assert(ctx->dhContext._full != NULL);

    tls_buffer prime;
    tls_buffer generator;
    tls_buffer pubKey;

    sslDecodeDhParams(ccdh_ctx_gp(ctx->dhContext._fullt), &prime, &generator);
    sslDhExportPub(ctx->dhContext, &pubKey);

    size_t len = (2+prime.length+2+generator.length+2+pubKey.length);

    SSLFreeBuffer(&prime);
    SSLFreeBuffer(&generator);
    SSLFreeBuffer(&pubKey);

    return len;
}

/*
 * Encode DH params and public key, in wire format, in caller-supplied buffer.
 */
static int
SSLEncodeDHKeyParams(
	tls_handshake_t ctx,
	uint8_t *charPtr)
{
    assert(ctx->isServer);
	assert(ctx->dhContext._full != NULL);

    //TODO: These allocations should be optimized out.
    tls_buffer prime;
    tls_buffer generator;
    tls_buffer pubKey;

    sslDecodeDhParams(ccdh_ctx_gp(ctx->dhContext._fullt), &prime, &generator);
    sslDhExportPub(ctx->dhContext, &pubKey);

	charPtr = SSLEncodeInt(charPtr, prime.length, 2);
	memcpy(charPtr, prime.data, prime.length);
	charPtr += prime.length;

	charPtr = SSLEncodeInt(charPtr, generator.length, 2);
	memcpy(charPtr, generator.data,
		generator.length);
	charPtr += generator.length;

    /* TODO: hum.... sounds like this one should be in the SecDHContext */
	charPtr = SSLEncodeInt(charPtr, pubKey.length, 2);
	memcpy(charPtr, pubKey.data, pubKey.length);

	dumpBuf("server prime", &prime);
	dumpBuf("server generator", &generator);
	dumpBuf("server pub key", &pubKey);

    //TODO: Those SSL buffers should be optimized out.
    SSLFreeBuffer(&prime);
    SSLFreeBuffer(&generator);
    SSLFreeBuffer(&pubKey);

	return errSSLSuccess;
}

/*
 * Decode DH params and server public key.
 */
static int
SSLDecodeDHKeyParams(
	tls_handshake_t ctx,
	uint8_t **charPtr,		// IN/OUT
	size_t length)
{
	int        err = errSSLSuccess;
    tls_buffer       prime;
    tls_buffer       generator;

	assert(!ctx->isServer);
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

    sslFree(ctx->dhParams.gp);
    err = sslEncodeDhParams(&ctx->dhParams, &prime, &generator);
    if(err) {
        return err;
    }

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
 *		dhParamsEncoded
 *		dhPeerPublic
 *
 * SSLContext members valid on successful return:
 *		dhPrivate
 *		dhExchangePublic
 *		preMasterSecret
 */
static int
SSLGenClientDHKeyAndExchange(tls_handshake_t ctx)
{
	int            ortn;

    ortn=errSSLProtocol;
    sslFree(ctx->dhContext._full);
    SSLFreeBuffer(&ctx->preMasterSecret);
    require(ctx->dhParams.gp, out);

    /* Note: ccdh_gp_prime_bitlen() does not return actual prime length */
    /* See: <rdar://problem/21033701> ccdh_gp_prime_bitlen does not return actual bitlen */
    if(ccn_bitlen(ccdh_gp_n(ctx->dhParams), ccdh_gp_prime(ctx->dhParams))<ctx->dhMinGroupSize) {
        return errSSLWeakPeerEphemeralDHKey;
    }

    require_noerr(ortn = sslDhCreateKey(ctx->dhParams, &ctx->dhContext), out);
    require_noerr(ortn = sslDhKeyExchange(ctx->dhContext, &ctx->dhPeerPublic, &ctx->preMasterSecret), out);
out:
	return ortn;
}


static int
SSLEncodeDHanonServerKeyExchange(tls_buffer *keyExch, tls_handshake_t ctx)
{
	int            ortn = errSSLSuccess;
    int                 head;

	assert(ctx->negProtocolVersion >= tls_protocol_version_SSL_3);
	assert(ctx->isServer);

	/*
	 * Obtain D-H parameters (if we don't have them) and a key pair.
	 */
	ortn = SSLGenServerDHParamsAndKey(ctx);
	if(ortn) {
		return ortn;
	}

	size_t length = SSLEncodedDHKeyParamsLen(ctx);

    head = SSLHandshakeHeaderSize(ctx);
	if ((ortn = SSLAllocBuffer(keyExch, length+head)))
		return ortn;

    uint8_t *charPtr = SSLEncodeHandshakeHeader(ctx, keyExch, SSL_HdskServerKeyExchange, length);

	/* encode prime, generator, our public key */
	return SSLEncodeDHKeyParams(ctx, charPtr);
}

static int
SSLDecodeDHanonServerKeyExchange(tls_buffer message, tls_handshake_t ctx)
{
	int        err = errSSLSuccess;

	assert(!ctx->isServer);
    if (message.length < 6) {
    	sslErrorLog("SSLDecodeDHanonServerKeyExchange error: msg len %u\n",
    		(unsigned)message.length);
        return errSSLProtocol;
    }
    uint8_t *charPtr = message.data;
	err = SSLDecodeDHKeyParams(ctx, &charPtr, message.length);
	if(err == errSSLSuccess) {
		if((message.data + message.length) != charPtr) {
			err = errSSLProtocol;
		}
	}
	return err;
}

static int
SSLDecodeDHClientKeyExchange(tls_buffer keyExchange, tls_handshake_t ctx)
{
	int        ortn = errSSLSuccess;
    unsigned int    publicLen;

	assert(ctx->isServer);
	if(ctx->dhContext._full == NULL) {
		/* should never happen */
		assert(0);
		return errSSLInternal;
	}

    /* this message simply contains the client's public DH key */
    uint8_t *charPtr = keyExchange.data;

    if (keyExchange.length < 2) {
        sslErrorLog("SSLDecodeDHClientKeyExchange: msg len error 1\n");
        return errSSLProtocol;
    }

    publicLen = SSLDecodeInt(charPtr, 2);
	charPtr += 2;

    if (keyExchange.length < 2 + publicLen) {
        sslErrorLog("SSLDecodeDHClientKeyExchange: msg len error 2\n");
        return errSSLProtocol;
    }

	SSLFreeBuffer(&ctx->dhPeerPublic);	// allow reuse via renegotiation
	ortn = SSLAllocBuffer(&ctx->dhPeerPublic, publicLen);
	if(ortn) {
		return ortn;
	}
	memmove(ctx->dhPeerPublic.data, charPtr, publicLen);

	/* DH Key exchange, result --> premaster secret */
	SSLFreeBuffer(&ctx->preMasterSecret);
    ortn = sslDhKeyExchange(ctx->dhContext, &ctx->dhPeerPublic, &ctx->preMasterSecret);
	dumpBuf("server peer pub", &ctx->dhPeerPublic);
	dumpBuf("server premaster", &ctx->preMasterSecret);
	return ortn;
}

static int
SSLEncodeDHClientKeyExchange(tls_buffer *keyExchange, tls_handshake_t ctx)
{   int            err;
    size_t				outputLen;
    int                 head;

	assert(!ctx->isServer);
	assert(ctx->negProtocolVersion >= tls_protocol_version_SSL_3);

    tls_buffer pubKey;

    if ((err = SSLGenClientDHKeyAndExchange(ctx)) != 0)
        return err;

    if((err = sslDhExportPub(ctx->dhContext, &pubKey)) != 0)
        return err;

    outputLen = pubKey.length + 2;
    head = SSLHandshakeHeaderSize(ctx);
    if ((err = SSLAllocBuffer(keyExchange,outputLen + head)))
        return err;
    // TODO: leak on allocation error - fix by removing the pubKey allocation.

    uint8_t *charPtr = SSLEncodeHandshakeHeader(ctx, keyExchange, SSL_HdskClientKeyExchange, outputLen);

    charPtr = SSLEncodeSize(charPtr, pubKey.length, 2);
    memcpy(charPtr, pubKey.data, pubKey.length);

	dumpBuf("client pub key", &pubKey);
	dumpBuf("client premaster", &ctx->preMasterSecret);

    SSLFreeBuffer(&pubKey);

    return errSSLSuccess;
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
static int
SSLGenClientECDHKeyAndExchange(tls_handshake_t ctx)
{
	int             ortn;
    SSLPubKey       *pubKey = NULL;
    SSLPubKey       ecdhePeerPubKey;

    assert(!ctx->isServer);

	switch(ctx->selectedCipherSpecParams.keyExchangeMethod) {
		case SSL_ECDHE_ECDSA:
		case SSL_ECDHE_RSA:
			/* Server sent us an ephemeral key with peer curve specified */
			if(ctx->ecdhPeerPublic.data == NULL) {
			   sslErrorLog("SSLGenClientECDHKeyAndExchange: incomplete server params\n");
			   return errSSLProtocol;
			}
            require_noerr((ortn=sslGetEcPubKeyFromBits(ctx->ecdhPeerCurve, &ctx->ecdhPeerPublic, &ecdhePeerPubKey)), errOut);
            pubKey = &ecdhePeerPubKey;
            break;
		case SSL_ECDH_ECDSA:
		case SSL_ECDH_RSA:
		{
			/* No server key exchange; we have to get the curve from the key */
			if(ctx->peerPubKey.ecc.pub == NULL) {
			   sslErrorLog("SSLGenClientECDHKeyAndExchange: no peer key\n");
			   return errSSLInternal;
			}

            if(ctx->peerPubKey.isRSA) {
                sslErrorLog("SSLGenClientECDHKeyAndExchange: peer key is RSA?\n");
                return errSSLProtocol;
			}
            pubKey = &ctx->peerPubKey;
            break;
		}
		default:
			/* shouldn't be here */
			assert(0);
			return errSSLInternal;
	}

    /* Generate our (ephemeral) pair, or extract it from our signing identity */
	if((ctx->negAuthType == tls_client_auth_type_RSAFixedECDH) ||
	   (ctx->negAuthType == tls_client_auth_type_ECDSAFixedECDH)) {
		/*
		 * Client auth with a fixed ECDH key in the cert. Would need to convert private key
		 * from SSLPrivKey to ccec_full_ctx. This is not supported and would need support for
         * ECDH with reference keys.
		 */
		assert(ctx->signingPrivKeyRef != NULL);
#warning TODO: This is Unsupported.
		sslEcdsaDebug("+++ Extracted ECDH private key");
        ortn = errSSLUnimplemented;
        goto errOut;
        /* TODO */
	} else {
		/* generate a new pair, using the curve from the pubkey */
        sslFree(ctx->ecdhContext._full);
        require_noerr((ortn = sslEcdhCreateKey(ccec_ctx_cp(pubKey->ecc), &ctx->ecdhContext)), errOut);
	}

	/* do the exchange --> premaster secret */
    SSLFreeBuffer(&ctx->preMasterSecret);
	require_noerr((ortn = sslEcdhKeyExchange(ctx->ecdhContext, pubKey->ecc._pub, &ctx->preMasterSecret)), errOut);

    ortn = errSSLSuccess;
errOut:
    if(pubKey == &ecdhePeerPubKey) {
        sslFreePubKey(&ecdhePeerPubKey);
    }
	return ortn;
}

static int
SSLGenServerECDHParamsAndKey(tls_handshake_t ctx)
{
    /*
     * Pick curve from what the cipher suite we are going to pick and what
     * the client supports via extension, just do P256 for now.
     */
    assert(ctx->isServer);
    ctx->ecdhPeerCurve = tls_curve_secp256r1;
    ccec_const_cp_t cp = ccec_cp_256();

    return sslEcdhCreateKey(cp, &ctx->ecdhContext);
}


/*
 * Decode ECDH params and server public key.
 */
static int
SSLEncodeECDHKeyParams(tls_handshake_t ctx, tls_buffer *keyExchange)
{
    tls_buffer pubKey = { .length = 0, .data = NULL };
    int err;
    /* curve type, curve name, curveSize, curve */

    assert(ctx->isServer);

    require_noerr(err = sslEcdhExportPub(ctx->ecdhContext, &pubKey), fail);

    size_t len = 1 + 2 + 1 + pubKey.length;

    require_noerr(err = SSLAllocBuffer(keyExchange, len), fail);

    uint8_t *p = keyExchange->data;

    p = SSLEncodeInt(p, SSL_CurveTypeNamed, 1);
    p = SSLEncodeInt(p, ctx->ecdhPeerCurve, 2);
    p = SSLEncodeInt(p, pubKey.length, 1);
    memcpy(p, pubKey.data, pubKey.length);


    err = errSSLSuccess;
fail:
    SSLFreeBuffer(&pubKey);
    return err;
}


/*
 * Decode ECDH params and server public key.
 */
static int
SSLDecodeECDHKeyParams(
	tls_handshake_t ctx,
	uint8_t **charPtr,		// IN/OUT
	size_t length)
{
	int        err = errSSLSuccess;

	sslEcdsaDebug("+++ Decoding ECDH Server Key Exchange");

	assert(!ctx->isServer);
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
		case tls_curve_secp256r1:
		case tls_curve_secp384r1:
		case tls_curve_secp521r1:
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


static int
SSLEncodeECDHClientKeyExchange(tls_buffer *keyExchange, tls_handshake_t ctx)
{   int            err;
    size_t				outputLen;
    int                 head;

	assert(!ctx->isServer);
    if ((err = SSLGenClientECDHKeyAndExchange(ctx)) != 0)
        return err;

    tls_buffer pubKey = {0,};

	/*
	 * Per RFC 4492 5.7, if we're doing ECDSA_fixed_ECDH or RSA_fixed_ECDH
	 * client auth, we still send this message, but it's empty (because the
	 * server gets our public key from our cert).
	 */
	bool emptyMsg = false;
	switch(ctx->negAuthType) {
            //TODO: cleanup: this modes are actually not supported
		case tls_client_auth_type_RSAFixedECDH:
		case tls_client_auth_type_ECDSAFixedECDH:
			emptyMsg = true;
			break;
		default:
			break;
	}
	if(emptyMsg) {
		outputLen = 0;
	}
	else {
        assert(ctx->ecdhContext.hdr);
        sslEcdhExportPub(ctx->ecdhContext, &pubKey);
		outputLen = pubKey.length + 1;
	}

	assert(ctx->negProtocolVersion >= tls_protocol_version_SSL_3);
    head = SSLHandshakeHeaderSize(ctx);
    if ((err = SSLAllocBuffer(keyExchange,outputLen + head)))
        return err;

    uint8_t *charPtr = SSLEncodeHandshakeHeader(ctx, keyExchange, SSL_HdskClientKeyExchange, outputLen);
	if(emptyMsg) {
		sslEcdsaDebug("+++ Sending EMPTY ECDH Client Key Exchange");
	}
	else {
		/* just a 1-byte length here... */
		charPtr = SSLEncodeSize(charPtr, pubKey.length, 1);
		memcpy(charPtr, pubKey.data, pubKey.length);
		sslEcdsaDebug("+++ Encoded ECDH Client Key Exchange");
	}

	dumpBuf("client pub key", &pubKey);
	dumpBuf("client premaster", &ctx->preMasterSecret);

    SSLFreeBuffer(&pubKey);
    return errSSLSuccess;
}

static int
SSLDecodeECDHClientKeyExchange(tls_buffer keyExchange, tls_handshake_t ctx)
{
    int err = errSSLSuccess;
    SSLPubKey ecdhePeerPubKey;

    assert(ctx->isServer);
    if (ctx->ecdhContext._full == NULL) {
        assert(0);
        return errSSLInternal;
    }

    if (keyExchange.length < 1) {
        return errSSLProtocol;
    }

    unsigned int publicLen = SSLDecodeInt(keyExchange.data, 1);

    if (keyExchange.length != publicLen + 1) {
        return errSSLProtocol;
    }

    SSLFreeBuffer(&ctx->ecdhPeerPublic);
    require_noerr(err = SSLAllocBuffer(&ctx->ecdhPeerPublic, publicLen), fail);
    memmove(ctx->ecdhPeerPublic.data, keyExchange.data + 1, publicLen);

    require_noerr(err = sslGetEcPubKeyFromBits(ctx->ecdhPeerCurve, &ctx->ecdhPeerPublic, &ecdhePeerPubKey), fail);

    /* DH Key exchange, result --> premaster secret */
    SSLFreeBuffer(&ctx->preMasterSecret);
    require_noerr(err = sslEcdhKeyExchange(ctx->ecdhContext, ecdhePeerPubKey.ecc._pub, &ctx->preMasterSecret), fail);

    dumpBuf("server peer pub", &ctx->ecdhPeerPublic);
    dumpBuf("server premaster", &ctx->ecpreMasterSecret);

fail:
    sslFreePubKey(&ecdhePeerPubKey);
    return err;
}


static int
SSLDecodePSKClientKeyExchange(tls_buffer keyExchange, tls_handshake_t ctx)
{
	int        ortn = errSSLSuccess;
    unsigned int    identityLen;

	assert(ctx->isServer);

	/* this message simply contains the client's PSK identity */
	uint8_t *charPtr = keyExchange.data;

    if (keyExchange.length < 2) {
        sslErrorLog("SSLDecodePSKClientKeyExchange: msg len error 1\n");
        return errSSLProtocol;
    }

    identityLen = SSLDecodeInt(charPtr, 2);
	charPtr += 2;

    if (keyExchange.length < 2 + identityLen) {
        sslErrorLog("SSLDecodePSKClientKeyExchange: msg len error 2\n");
        return errSSLProtocol;
    }

	SSLFreeBuffer(&ctx->pskIdentity);	// allow reuse via renegotiation
	ortn = SSLAllocBuffer(&ctx->pskIdentity, identityLen);
	if(ortn) {
		return ortn;
	}
	memmove(ctx->pskIdentity.data, charPtr, identityLen);

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


static int
SSLEncodePSKClientKeyExchange(tls_buffer *keyExchange, tls_handshake_t ctx)
{
    int            err;
    size_t				outputLen;
    int                 head;

	assert(!ctx->isServer);

	outputLen = ctx->pskIdentity.length+2;

	assert(ctx->negProtocolVersion >= tls_protocol_version_SSL_3);
    head = SSLHandshakeHeaderSize(ctx);
    if ((err = SSLAllocBuffer(keyExchange,outputLen + head)))
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

    return errSSLSuccess;
}

// MARK: -
// MARK: Public Functions
int
SSLEncodeServerKeyExchange(tls_buffer *keyExch, tls_handshake_t ctx)
{   int      err;
    
    switch (ctx->selectedCipherSpecParams.keyExchangeMethod)
    {   case SSL_RSA:
        #if		APPLE_DH
		case SSL_DHE_RSA:
		case SSL_DHE_DSS:
		#endif	/* APPLE_DH */
        case SSL_ECDHE_RSA:
        case SSL_ECDH_ECDSA:
        case SSL_ECDHE_ECDSA:
        case SSL_ECDH_RSA:
        case SSL_ECDH_anon:
           if ((err = SSLEncodeSignedServerKeyExchange(keyExch, ctx)) != 0)
                return err;
            break;
        #if		APPLE_DH
        case SSL_DH_anon:
            if ((err = SSLEncodeDHanonServerKeyExchange(keyExch, ctx)) != 0)
                return err;
            break;
        #endif

        default:
            return errSSLUnimplemented;
    }

    return errSSLSuccess;
}

int
SSLProcessServerKeyExchange(tls_buffer message, tls_handshake_t ctx)
{
	int      err;
    
    switch (ctx->selectedCipherSpecParams.keyExchangeMethod) {
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
#warning TODO: TLS_PSK Server Key Exchange missing.
        default:
            err = errSSLUnimplemented;
			break;
    }

    return err;
}

int
SSLEncodeKeyExchange(tls_buffer *keyExchange, tls_handshake_t ctx)
{   int      err;
    
	assert(!ctx->isServer);

    switch (ctx->selectedCipherSpecParams.keyExchangeMethod) {
		case SSL_RSA:
			sslDebugLog("SSLEncodeKeyExchange: RSA method\n");
			err = SSLEncodeRSAKeyExchange(keyExchange, ctx);
			break;
#if		APPLE_DH
		case SSL_DHE_RSA:
		case SSL_DHE_DSS:
		case SSL_DH_anon:
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
			err = errSSLUnimplemented;
	}

	return err;
}

int
SSLProcessKeyExchange(tls_buffer keyExchange, tls_handshake_t ctx)
{   int      err;
    
    switch (ctx->selectedCipherSpecParams.keyExchangeMethod)
    {
        case SSL_RSA:
            if ((err = SSLDecodeRSAKeyExchange(keyExchange, ctx)) != 0)
                return err;
            break;
#if		APPLE_DH
        case SSL_DH_anon:
		case SSL_DHE_RSA:
			sslDebugLog("SSLProcessKeyExchange: processing DH key exchange (%d)\n",
					ctx->selectedCipherSpecParams.keyExchangeMethod);
			if ((err = SSLDecodeDHClientKeyExchange(keyExchange, ctx)) != 0)
				return err;
			break;
#endif
        case SSL_ECDHE_RSA:
        case SSL_ECDHE_ECDSA:
            if ((err = SSLDecodeECDHClientKeyExchange(keyExchange, ctx)))
                return err;
            break;

        case TLS_PSK:
			if ((err = SSLDecodePSKClientKeyExchange(keyExchange, ctx)) != 0)
				return err;
			break;
		default:
			sslErrorLog("SSLProcessKeyExchange: unknown keyExchangeMethod (%d)\n",
					ctx->selectedCipherSpecParams.keyExchangeMethod);
			return errSSLUnimplemented;
	}

	return errSSLSuccess;
}

int
SSLInitPendingCiphers(tls_handshake_t ctx)
{   int        err;
    tls_buffer       key;
    int             keyDataLen;

    assert(ctx->selectedCipherSpecParams.cipherSpec == ctx->selectedCipher);

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
    
    if((err = ctx->callbacks->init_pending_cipher(ctx->callback_ctx, ctx->selectedCipher, ctx->isServer, key)) != 0)
        goto fail;
    
    ctx->writePending_ready = 1;
    ctx->readPending_ready = 1;
    
fail:
    SSLFreeBuffer(&key);
    return err;
}

/*
 * Copyright (c) 2002,2005-2007,2010-2012 Apple Inc. All Rights Reserved.
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
 * tls1Callouts.c - TLSv1-specific routines for SslTlsCallouts.
 */

#include "SecureTransport.h"

#include "tls_ssl.h"
#include "sslMemory.h"
#include "sslUtils.h"
#include "sslDigests.h"
#include "sslAlertMessage.h"
#include "sslCrypto.h"
#include "sslDebug.h"
#include "tls_hmac.h"
#include <assert.h>
#include <strings.h>

#define TLS_ENC_DEBUG		0
#if		TLS_ENC_DEBUG
#define tlsDebug(format, args...)	printf(format , ## args)
static void tlsDump(const char *name, void *b, unsigned len)
{
	unsigned char *cp = (unsigned char *)b;
	unsigned i, dex;

	printf("%s\n", name);
	for(dex=0; dex<len; dex++) {
		i = cp[dex];
		printf("%02X ", i);
		if((dex % 16) == 15) {
			printf("\n");
		}
	}
	printf("\n");
}

#else
#define tlsDebug(s, ...)
#define tlsDump(name, b, len)
#endif	/* TLS_ENC_DEBUG */

// MARK: -
// MARK: PRF label strings
/*
 * Note we could optimize away a bunch of mallocs and frees if we, like openSSL,
 * just mallocd buffers for inputs to SSLInternal_PRF() on the stack,
 * with "known" max values for all of the inputs.
 *
 * At least we hard-code string lengths here instead of calling strlen at runtime...
 */
#define PLS_MASTER_SECRET			"master secret"
#define PLS_MASTER_SECRET_LEN		13
#define PLS_KEY_EXPAND				"key expansion"
#define PLS_KEY_EXPAND_LEN			13
#define PLS_CLIENT_FINISH			"client finished"
#define PLS_CLIENT_FINISH_LEN		15
#define PLS_SERVER_FINISH			"server finished"
#define PLS_SERVER_FINISH_LEN		15
#define PLS_EXPORT_CLIENT_WRITE		"client write key"
#define PLS_EXPORT_CLIENT_WRITE_LEN	16
#define PLS_EXPORT_SERVER_WRITE		"server write key"
#define PLS_EXPORT_SERVER_WRITE_LEN	16
#define PLS_EXPORT_IV_BLOCK			"IV block"
#define PLS_EXPORT_IV_BLOCK_LEN		8

// MARK: -
// MARK: private functions

/*
 * P_Hash function defined in RFC2246, section 5.
 */
static OSStatus tlsPHash(
	SSLContext 			*ctx,
	const HMACReference *hmac,      // &TlsHmacSHA1, TlsHmacMD5
	const uint8_t       *secret,
	size_t              secretLen,
	const uint8_t       *seed,
	size_t              seedLen,
	uint8_t             *out,       // mallocd by caller, size >= outLen
	size_t              outLen)     // desired output size
{
	unsigned char aSubI[TLS_HMAC_MAX_SIZE];		/* A(i) */
	unsigned char digest[TLS_HMAC_MAX_SIZE];
	HMACContextRef hmacCtx;
	OSStatus serr;
	size_t digestLen = hmac->macSize;
	
	serr = hmac->alloc(hmac, secret, secretLen, &hmacCtx);
	if(serr) {
		return serr;
	}

	/* A(0) = seed */
	/* A(1) := HMAC_hash(secret, seed) */
	serr = hmac->hmac(hmacCtx, seed, seedLen, aSubI, &digestLen);
	if(serr) {
		goto fail;
	}
	assert(digestLen = hmac->macSize);

	/* starting at loopNum 1... */
	for (;;) {
		/*
		 * This loop's chunk = HMAC_hash(secret, A(loopNum) + seed))
		 */
		serr = hmac->init(hmacCtx);
		if(serr) {
			break;
		}
		serr = hmac->update(hmacCtx, aSubI, digestLen);
		if(serr) {
			break;
		}
		serr = hmac->update(hmacCtx, seed, seedLen);
		if(serr) {
			break;
		}
		serr = hmac->final(hmacCtx, digest, &digestLen);
		if(serr) {
			break;
		}
		assert(digestLen = hmac->macSize);

		if(outLen <= digestLen) {
			/* last time, possible partial digest */
			memmove(out, digest, outLen);
			break;
		}

		memmove(out, digest, digestLen);
		out += digestLen;
		outLen -= digestLen;

		/*
		 * A(i) = HMAC_hash(secret, A(i-1))
		 * Note there is a possible optimization involving obtaining this
		 * hmac by cloning the state of hmacCtx above after updating with
		 * aSubI, and getting the final version of that here. However CDSA
		 * does not support cloning of a MAC context (only for digest contexts).
		 */
		serr = hmac->hmac(hmacCtx, aSubI, digestLen,
			aSubI, &digestLen);
		if(serr) {
			break;
		}
		assert(digestLen = hmac->macSize);
	}
fail:
	hmac->free(hmacCtx);
	memset(aSubI, 0, TLS_HMAC_MAX_SIZE);
	memset(digest, 0, TLS_HMAC_MAX_SIZE);
	return serr;
}

/*
 * The TLS pseudorandom function, defined in RFC2246, section 5.
 * This takes as its input a secret block, a label, and a seed, and produces
 * a caller-specified length of pseudorandom data.
 *
 * Optimization TBD: make label optional, avoid malloc and two copies if it's
 * not there, so callers can take advantage of fixed-size seeds.
 */
OSStatus SSLInternal_PRF(
	SSLContext *ctx,
	const void *vsecret,
	size_t secretLen,
	const void *label,			// optional, NULL implies that seed contains
								//   the label
	size_t labelLen,
	const void *seed,
	size_t seedLen,
	void *vout,					// mallocd by caller, length >= outLen
	size_t outLen)
{
	OSStatus serr = errSSLInternal;
	const unsigned char *S1, *S2;	 	// the two seeds
	size_t sLen;					 	// effective length of each seed
	unsigned char *labelSeed = NULL;	// label + seed, passed to tlsPHash
	size_t labelSeedLen;
	unsigned char *tmpOut = NULL;		// output of P_SHA1
	size_t i;
	const unsigned char *secret = (const unsigned char *)vsecret;

	if(label != NULL) {
		/* concatenate label and seed */
		labelSeedLen = labelLen + seedLen;
		labelSeed = (unsigned char *)sslMalloc(labelSeedLen);
		if(labelSeed == NULL) {
			return errSecAllocate;
		}
		memmove(labelSeed, label, labelLen);
		memmove(labelSeed + labelLen, seed, seedLen);
	}
	else {
		/* fast track - just use seed as is */
		labelSeed = (unsigned char *)seed;
		labelSeedLen = seedLen;
	}

    unsigned char *out = (unsigned char *)vout;
    if(sslVersionIsLikeTls12(ctx)) {
        const HMACReference *mac = &TlsHmacSHA256;
        if (ctx->selectedCipherSpecParams.macAlg == HA_SHA384) {
            mac = &TlsHmacSHA384;
        }
        serr = tlsPHash(ctx, mac, secret, secretLen, labelSeed, labelSeedLen,
                        out, outLen);
        if(serr) {
            goto fail;
        }
    } else {
        /* two seeds for tlsPHash */
        sLen = secretLen / 2;			// for partitioning
        S1 = secret;
        S2 = &secret[sLen];
        sLen += (secretLen & 1);		// secret length odd, increment effective size

        /* temporary output for SHA1, to be XORd with MD5 */
        tmpOut = (unsigned char *)sslMalloc(outLen);
        if(tmpOut == NULL) {
            serr = errSecAllocate;
            goto fail;
        }

        serr = tlsPHash(ctx, &TlsHmacMD5, S1, sLen, labelSeed, labelSeedLen,
            out, outLen);
        if(serr) {
            goto fail;
        }
        serr = tlsPHash(ctx, &TlsHmacSHA1, S2, sLen, labelSeed, labelSeedLen,
            tmpOut, outLen);
        if(serr) {
            goto fail;
        }

        /* XOR together to get final result */
        for(i=0; i<outLen; i++) {
            out[i] ^= tmpOut[i];
        }
	}

    serr = errSecSuccess;
fail:
	if((labelSeed != NULL) && (label != NULL)) {
		sslFree(labelSeed);
	}
	if(tmpOut != NULL) {
		sslFree(tmpOut);
	}
	return serr;
}

/*
 * On input, the following are valid:
 *		MasterSecret[48]
 *		ClientHello.random[32]
 *      ServerHello.random[32]
 *
 *      key_block = PRF(SecurityParameters.master_secret,
 *                         "key expansion",
 *                         SecurityParameters.server_random +
 *                         SecurityParameters.client_random);
 */

#define GKM_SEED_LEN	(PLS_KEY_EXPAND_LEN + (2 * SSL_CLIENT_SRVR_RAND_SIZE))

static OSStatus tls1GenerateKeyMaterial (
	SSLBuffer key, 					// caller mallocs and specifies length of
									//   required key material here
	SSLContext *ctx)
{
	unsigned char seedBuf[GKM_SEED_LEN];
	OSStatus serr;

	/* use optimized label-less PRF */
	memmove(seedBuf, PLS_KEY_EXPAND, PLS_KEY_EXPAND_LEN);
	memmove(seedBuf + PLS_KEY_EXPAND_LEN, ctx->serverRandom,
		SSL_CLIENT_SRVR_RAND_SIZE);
	memmove(seedBuf + PLS_KEY_EXPAND_LEN + SSL_CLIENT_SRVR_RAND_SIZE,
		ctx->clientRandom, SSL_CLIENT_SRVR_RAND_SIZE);
	serr = SSLInternal_PRF(ctx,
		ctx->masterSecret,
		SSL_MASTER_SECRET_SIZE,
		NULL,						// no label
		0,
		seedBuf,
		GKM_SEED_LEN,
		key.data,					// destination
		key.length);
	tlsDump("key expansion", key.data, key.length);
	return serr;
}

/*
 * On entry: clientRandom, serverRandom, preMasterSecret valid
 * On return: masterSecret valid
 *
 * master_secret = PRF(pre_master_secret, "master secret",
 * 						ClientHello.random + ServerHello.random)
 *      [0..47];
 */

static OSStatus tls1GenerateMasterSecret (
	SSLContext *ctx)
{
	unsigned char randBuf[2 * SSL_CLIENT_SRVR_RAND_SIZE];
	OSStatus serr;

	memmove(randBuf, ctx->clientRandom, SSL_CLIENT_SRVR_RAND_SIZE);
	memmove(randBuf + SSL_CLIENT_SRVR_RAND_SIZE,
		ctx->serverRandom, SSL_CLIENT_SRVR_RAND_SIZE);
	serr = SSLInternal_PRF(ctx,
		ctx->preMasterSecret.data,
		ctx->preMasterSecret.length,
		(const unsigned char *)PLS_MASTER_SECRET,
		PLS_MASTER_SECRET_LEN,
		randBuf,
		2 * SSL_CLIENT_SRVR_RAND_SIZE,
		ctx->masterSecret,		// destination
		SSL_MASTER_SECRET_SIZE);
	tlsDump("master secret", ctx->masterSecret, SSL_MASTER_SECRET_SIZE);
	return serr;
}

/*
 * Given digests contexts representing the running total of all handshake messages,
 * calculate mac for "finished" message.
 *
 *			verify_data = 12 bytes =
 *				PRF(master_secret, finished_label, MD5(handshake_messages) +
 *					SHA-1(handshake_messages)) [0..11];
 */
static OSStatus tls1ComputeFinishedMac (
	SSLContext *ctx,
	SSLBuffer finished, 		// output - mallocd by caller
	Boolean isServer)
{
	unsigned char digests[SSL_MD5_DIGEST_LEN + SSL_SHA1_DIGEST_LEN];
	SSLBuffer digBuf;
	char *finLabel;
	unsigned finLabelLen;
	OSStatus serr;
    SSLBuffer shaMsgState, md5MsgState;

    shaMsgState.data = 0;
    md5MsgState.data = 0;
    if ((serr = CloneHashState(&SSLHashSHA1, &ctx->shaState, &shaMsgState)) != 0)
        goto fail;
    if ((serr = CloneHashState(&SSLHashMD5, &ctx->md5State, &md5MsgState)) != 0)
        goto fail;

	if(isServer) {
		finLabel = PLS_SERVER_FINISH;
		finLabelLen = PLS_SERVER_FINISH_LEN;
	}
	else {
		finLabel = PLS_CLIENT_FINISH;
		finLabelLen = PLS_CLIENT_FINISH_LEN;
	}

	/* concatenate two digest results */
	digBuf.data = digests;
	digBuf.length = SSL_MD5_DIGEST_LEN;
	serr = SSLHashMD5.final(&md5MsgState, &digBuf);
	if(serr) {
		return serr;
	}
	digBuf.data += SSL_MD5_DIGEST_LEN;
	digBuf.length = SSL_SHA1_DIGEST_LEN;
	serr = SSLHashSHA1.final(&shaMsgState, &digBuf);
	if(serr) {
		return serr;
	}
    serr = SSLInternal_PRF(ctx,
		ctx->masterSecret,
		SSL_MASTER_SECRET_SIZE,
		(const unsigned char *)finLabel,
		finLabelLen,
		digests,
		SSL_MD5_DIGEST_LEN + SSL_SHA1_DIGEST_LEN,
		finished.data,				// destination
		finished.length);

fail:
    SSLFreeBuffer(&shaMsgState);
    SSLFreeBuffer(&md5MsgState);

    return serr;
}

/*
 * Given digests contexts representing the running total of all handshake messages,
 * calculate mac for "finished" message.
 *
 *			verify_data = 12 bytes =
 *				PRF(master_secret, finished_label, SHA256(handshake_messages)) [0..11];
 */
static OSStatus tls12ComputeFinishedMac (
                                        SSLContext *ctx,
                                        SSLBuffer finished, 		// output - mallocd by caller
                                        Boolean isServer)
{
	unsigned char digest[SSL_MAX_DIGEST_LEN];
	SSLBuffer digBuf;
	char *finLabel;
	unsigned finLabelLen;
	OSStatus serr;
    SSLBuffer hashState;
    const HashReference *hashRef;
    const SSLBuffer *ctxHashState;

    /* The PRF used in the finished message is based on the cipherspec */
    if (ctx->selectedCipherSpecParams.macAlg == HA_SHA384) {
        hashRef = &SSLHashSHA384;
        ctxHashState = &ctx->sha512State;
    } else {
        hashRef = &SSLHashSHA256;
        ctxHashState = &ctx->sha256State;
    }

    hashState.data = 0;
    if ((serr = CloneHashState(hashRef, ctxHashState, &hashState)) != 0)
        goto fail;
	if(isServer) {
		finLabel = PLS_SERVER_FINISH;
		finLabelLen = PLS_SERVER_FINISH_LEN;
	}
	else {
		finLabel = PLS_CLIENT_FINISH;
		finLabelLen = PLS_CLIENT_FINISH_LEN;
	}

	/* concatenate two digest results */
	digBuf.data = digest;
	digBuf.length = hashRef->digestSize;
	if ((serr = hashRef->final(&hashState, &digBuf)) != 0)
        goto fail;
	serr = SSLInternal_PRF(ctx,
                           ctx->masterSecret,
                           SSL_MASTER_SECRET_SIZE,
                           (const unsigned char *)finLabel,
                           finLabelLen,
                           digBuf.data,
                           digBuf.length,
                           finished.data,				// destination
                           finished.length);
fail:
    SSLFreeBuffer(&hashState);
    return serr;
}

/*
 * This one is trivial.
 *
 * mac := MD5(handshake_messages) + SHA(handshake_messages);
 *
 * I don't know why this one doesn't use an HMAC or the master secret (as SSLv3
 * does).
 */
static OSStatus tls1ComputeCertVfyMac (
	SSLContext *ctx,
	SSLBuffer *finished, 		// output - mallocd by caller
    SSL_HashAlgorithm hash)     //unused in this one
{
	SSLBuffer digBuf, shaMsgState, md5MsgState;
	OSStatus serr;

    shaMsgState.data = 0;
    md5MsgState.data = 0;

    if ((serr = CloneHashState(&SSLHashSHA1, &ctx->shaState, &shaMsgState)) != 0)
        goto fail;
    if ((serr = CloneHashState(&SSLHashMD5, &ctx->md5State, &md5MsgState)) != 0)
        goto fail;

    if ((ctx->protocolSide == kSSLServerSide && sslPubKeyGetAlgorithmID(ctx->peerPubKey) == kSecECDSAAlgorithmID) ||
        (ctx->protocolSide == kSSLClientSide && ctx->negAuthType == SSLClientAuth_ECDSASign)) {
		/* Only take SHA1 regardless of TLSv1.0 or TLSv1.1 If we are the server
           and our peer signed with an ECDSA key, or if we are the client and
           are about to sign with ECDSA. */
        assert(finished->length >= SSL_SHA1_DIGEST_LEN);
        digBuf.data = finished->data;
        finished->length = SSL_SHA1_DIGEST_LEN;
    } else {
        /* Put MD5 follow by SHA1 hash in buffer. */
        assert(finished->length >= (SSL_MD5_DIGEST_LEN + SSL_SHA1_DIGEST_LEN));
        digBuf.data = finished->data;
        digBuf.length = SSL_MD5_DIGEST_LEN;
        if ((serr = SSLHashMD5.final(&md5MsgState, &digBuf)) != 0)
            goto fail;
        digBuf.data = finished->data + SSL_MD5_DIGEST_LEN;
        finished->length = SSL_MD5_DIGEST_LEN + SSL_SHA1_DIGEST_LEN;
    }

	digBuf.length = SSL_SHA1_DIGEST_LEN;
	serr = SSLHashSHA1.final(&shaMsgState, &digBuf);

fail:
    SSLFreeBuffer(&shaMsgState);
    SSLFreeBuffer(&md5MsgState);

    return serr;
}

static OSStatus tls12ComputeCertVfyMac (
    SSLContext *ctx,
    SSLBuffer *finished,		// output - mallocd by caller
    SSL_HashAlgorithm hash)
{
	const SSLBuffer *ctxHashState;
    const HashReference *hashRef;
	SSLBuffer hashState;
	OSStatus serr;

    hashState.data = 0;

    switch (hash) {
        case SSL_HashAlgorithmSHA1:
            hashRef = &SSLHashSHA1;
            ctxHashState = &ctx->shaState;
            break;
        case SSL_HashAlgorithmSHA256:
            hashRef = &SSLHashSHA256;
            ctxHashState = &ctx->sha256State;
            break;
        case SSL_HashAlgorithmSHA384:
            hashRef = &SSLHashSHA384;
            ctxHashState = &ctx->sha512State;
            break;
        default:
            return errSSLInternal;
            break;
    }

    if ((serr = CloneHashState(hashRef, ctxHashState, &hashState)) != 0)
        goto fail;

	assert(finished->length >= (hashRef->digestSize));
	finished->length = hashRef->digestSize;
	serr = hashRef->final(&hashState, finished);

fail:
    SSLFreeBuffer(&hashState);

    return serr;
}


const SslTlsCallouts Tls1Callouts = {
	tls1GenerateKeyMaterial,
	tls1GenerateMasterSecret,
	tls1ComputeFinishedMac,
	tls1ComputeCertVfyMac
};

const SslTlsCallouts Tls12Callouts = {
	tls1GenerateKeyMaterial,
	tls1GenerateMasterSecret,
	tls12ComputeFinishedMac,
	tls12ComputeCertVfyMac
};

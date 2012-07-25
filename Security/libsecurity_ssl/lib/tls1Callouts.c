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

#include "tls_ssl.h"
#include "sslMemory.h"
#include "sslUtils.h"
#include "sslDigests.h"
#include "sslAlertMessage.h"
#include "sslCrypto.h"
#include "sslDebug.h"
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

#pragma mark -
#pragma mark PRF label strings
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

#pragma mark -
#pragma mark private functions

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

	serr = hmac->alloc(hmac, ctx, secret, secretLen, &hmacCtx);
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
			return memFullErr;
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
        if (ctx->selectedCipherSpec.macAlgorithm->hmac->alg == HA_SHA384) {
            mac = ctx->selectedCipherSpec.macAlgorithm->hmac;
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
            serr = memFullErr;
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

    serr = noErr;
fail:
	if((labelSeed != NULL) && (label != NULL)) {
		sslFree(labelSeed);
	}
	if(tmpOut != NULL) {
		sslFree(tmpOut);
	}
	return serr;
}

/* not needed; encrypt/encode is the same for both protocols as long as
 * we don't use the "variable length padding" feature. */
#if 0
static OSStatus tls1WriteRecord(
	SSLRecord rec,
	SSLContext *ctx)
{
	assert(0);
	return unimpErr;
}
#endif

static OSStatus tls1DecryptRecord(
	UInt8 type,
	SSLBuffer *payload,
	SSLContext *ctx)
{
	OSStatus    err;
    SSLBuffer   content;

    if ((ctx->readCipher.symCipher->blockSize > 0) &&
        ((payload->length % ctx->readCipher.symCipher->blockSize) != 0)) {
		SSLFatalSessionAlert(SSL_AlertRecordOverflow, ctx);
        return errSSLRecordOverflow;
    }

    /* Decrypt in place */
    if ((err = ctx->readCipher.symCipher->decrypt(payload->data,
    		payload->data, payload->length,
    		&ctx->readCipher,
    		ctx)) != 0)
    {   SSLFatalSessionAlert(SSL_AlertDecryptError, ctx);
        return errSSLDecryptionFail;
    }

    /* Locate content within decrypted payload */

    /* TLS 1.1 and DTLS 1.0 block ciphers */
    if((ctx->negProtocolVersion>=TLS_Version_1_1) && (ctx->readCipher.symCipher->blockSize>0))
    {
        content.data = payload->data + ctx->readCipher.symCipher->blockSize;
        content.length = payload->length - (ctx->readCipher.macRef->hash->digestSize + ctx->readCipher.symCipher->blockSize);
    } else {
        content.data = payload->data;
        content.length = payload->length - ctx->readCipher.macRef->hash->digestSize;
    }

    if (ctx->readCipher.symCipher->blockSize > 0) {
		/* for TLSv1, padding can be anywhere from 0 to 255 bytes */
		UInt8 padSize = payload->data[payload->length - 1];
		UInt8 *padChars;

		/* verify that all padding bytes are equal - WARNING - OpenSSL code
		 * has a special case here dealing with some kind of bug related to
		 * even size packets...beware... */
		if(padSize > payload->length) {
            /* This is TLS 1.1 compliant - Do it for all protocols versions */
			SSLFatalSessionAlert(SSL_AlertBadRecordMac, ctx);
        	sslErrorLog("tls1DecryptRecord: bad padding length (%d)\n",
        		(unsigned)payload->data[payload->length - 1]);
            return errSSLDecryptionFail;
		}
		padChars = payload->data + payload->length - padSize;
		while(padChars < (payload->data + payload->length)) {
			if(*padChars++ != padSize) {
                /* This is TLS 1.1 compliant - Do it for all protocols versions */
				SSLFatalSessionAlert(SSL_AlertBadRecordMac, ctx);
				sslErrorLog("tls1DecryptRecord: bad padding value\n");
				return errSSLDecryptionFail;
			}
		}
		/* Remove block size padding and its one-byte length */
        content.length -= (1 + padSize);
    }

	/* Verify MAC on payload */
    if (ctx->readCipher.macRef->hash->digestSize > 0)
		/* Optimize away MAC for null case */
        if ((err = SSLVerifyMac(type, &content,
				content.data + content.length, ctx)) != 0)
        {   SSLFatalSessionAlert(SSL_AlertBadRecordMac, ctx);
            return errSSLBadRecordMac;
        }

    *payload = content;     /* Modify payload buffer to indicate content length */

    return noErr;
}

/* initialize a per-CipherContext HashHmacContext for use in MACing each record */
static OSStatus tls1InitMac (
	CipherContext *cipherCtx,		// macRef, macSecret valid on entry
									// macCtx valid on return
	SSLContext *ctx)
{
	const HMACReference *hmac;
	OSStatus serr;

	assert(cipherCtx->macRef != NULL);
	hmac = cipherCtx->macRef->hmac;
	assert(hmac != NULL);

	if(cipherCtx->macCtx.hmacCtx != NULL) {
		hmac->free(cipherCtx->macCtx.hmacCtx);
		cipherCtx->macCtx.hmacCtx = NULL;
	}
	serr = hmac->alloc(hmac, ctx, cipherCtx->macSecret,
		cipherCtx->macRef->hmac->macSize, &cipherCtx->macCtx.hmacCtx);

	/* mac secret now stored in macCtx.hmacCtx, delete it from cipherCtx */
	memset(cipherCtx->macSecret, 0, sizeof(cipherCtx->macSecret));
	return serr;
}

static OSStatus tls1FreeMac (
	CipherContext *cipherCtx)
{
	/* this can be called on a completely zeroed out CipherContext... */
	if(cipherCtx->macRef == NULL) {
		return noErr;
	}
	assert(cipherCtx->macRef->hmac != NULL);

	if(cipherCtx->macCtx.hmacCtx != NULL) {
		cipherCtx->macRef->hmac->free(cipherCtx->macCtx.hmacCtx);
		cipherCtx->macCtx.hmacCtx = NULL;
	}
	return noErr;
}

/*
 * mac = HMAC_hash(MAC_write_secret, seq_num + TLSCompressed.type +
 *					TLSCompressed.version + TLSCompressed.length +
 *					TLSCompressed.fragment));
 */

/* sequence, type, version, length */
#define HDR_LENGTH (8 + 1 + 2 + 2)
static OSStatus tls1ComputeMac (
	UInt8 type,
	SSLBuffer data,
	SSLBuffer mac, 					// caller mallocs data
	CipherContext *cipherCtx,		// assumes macCtx, macRef
	sslUint64 seqNo,
	SSLContext *ctx)
{
	uint8_t hdr[HDR_LENGTH];
	uint8_t *p;
	HMACContextRef hmacCtx;
	OSStatus serr;
	const HMACReference *hmac;
	size_t macLength;

	assert(cipherCtx != NULL);
	assert(cipherCtx->macRef != NULL);
	hmac = cipherCtx->macRef->hmac;
	assert(hmac != NULL);
	hmacCtx = cipherCtx->macCtx.hmacCtx;	// may be NULL, for null cipher

	serr = hmac->init(hmacCtx);
	if(serr) {
		goto fail;
	}
	p = SSLEncodeUInt64(hdr, seqNo);
	*p++ = type;
	*p++ = ctx->negProtocolVersion >> 8;
	*p++ = ctx->negProtocolVersion & 0xff;
	*p++ = data.length >> 8;
	*p   = data.length & 0xff;
	serr = hmac->update(hmacCtx, hdr, HDR_LENGTH);
	if(serr) {
		goto fail;
	}
	serr = hmac->update(hmacCtx, data.data, data.length);
	if(serr) {
		goto fail;
	}
	macLength = mac.length;
	serr = hmac->final(hmacCtx, mac.data, &macLength);
	if(serr) {
		goto fail;
	}
	mac.length = macLength;
fail:
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
 *     final_client_write_key =
 *			PRF(SecurityParameters.client_write_key,
 *                                 "client write key",
 *                                 SecurityParameters.client_random +
 *                                 SecurityParameters.server_random);
 *     final_server_write_key =
 *      	PRF(SecurityParameters.server_write_key,
 *                                 "server write key",
 *                                 SecurityParameters.client_random +
 *                                 SecurityParameters.server_random);
 *
 *     iv_block = PRF("", "IV block", SecurityParameters.client_random +
 *                      SecurityParameters.server_random);
 *
 *	   iv_block is broken up into:
 *
 *     		client_write_IV[SecurityParameters.IV_size]
 *  	   	server_write_IV[SecurityParameters.IV_size]
 */
static OSStatus tls1GenerateExportKeyAndIv (
	SSLContext *ctx,				// clientRandom, serverRandom valid
	const SSLBuffer clientWriteKey,
	const SSLBuffer serverWriteKey,
	SSLBuffer finalClientWriteKey,	// RETURNED, mallocd by caller
	SSLBuffer finalServerWriteKey,	// RETURNED, mallocd by caller
	SSLBuffer finalClientIV,		// RETURNED, mallocd by caller
	SSLBuffer finalServerIV)		// RETURNED, mallocd by caller
{
	unsigned char randBuf[2 * SSL_CLIENT_SRVR_RAND_SIZE];
	OSStatus serr;
	unsigned char *ivBlock;
	char *nullKey = "";

	/* all three PRF calls use the same seed */
	memmove(randBuf, ctx->clientRandom, SSL_CLIENT_SRVR_RAND_SIZE);
	memmove(randBuf + SSL_CLIENT_SRVR_RAND_SIZE,
		ctx->serverRandom, SSL_CLIENT_SRVR_RAND_SIZE);

	serr = SSLInternal_PRF(ctx,
		clientWriteKey.data,
		clientWriteKey.length,
		(const unsigned char *)PLS_EXPORT_CLIENT_WRITE,
		PLS_EXPORT_CLIENT_WRITE_LEN,
		randBuf,
		2 * SSL_CLIENT_SRVR_RAND_SIZE,
		finalClientWriteKey.data,		// destination
		finalClientWriteKey.length);
	if(serr) {
		return serr;
	}
	serr = SSLInternal_PRF(ctx,
		serverWriteKey.data,
		serverWriteKey.length,
		(const unsigned char *)PLS_EXPORT_SERVER_WRITE,
		PLS_EXPORT_SERVER_WRITE_LEN,
		randBuf,
		2 * SSL_CLIENT_SRVR_RAND_SIZE,
		finalServerWriteKey.data,		// destination
		finalServerWriteKey.length);
	if(serr) {
		return serr;
	}
	if((finalClientIV.length == 0) && (finalServerIV.length == 0)) {
		/* skip remainder as optimization */
		return noErr;
	}
	ivBlock = (unsigned char *)sslMalloc(finalClientIV.length + finalServerIV.length);
	if(ivBlock == NULL) {
		return memFullErr;
	}
	serr = SSLInternal_PRF(ctx,
		(const unsigned char *)nullKey,
		0,
		(const unsigned char *)PLS_EXPORT_IV_BLOCK,
		PLS_EXPORT_IV_BLOCK_LEN,
		randBuf,
		2 * SSL_CLIENT_SRVR_RAND_SIZE,
		ivBlock,					// destination
		finalClientIV.length + finalServerIV.length);
	if(serr) {
		goto done;
	}
	memmove(finalClientIV.data, ivBlock, finalClientIV.length);
	memmove(finalServerIV.data, ivBlock + finalClientIV.length, finalServerIV.length);
done:
	sslFree(ivBlock);
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
    if ((serr = CloneHashState(&SSLHashSHA1, &ctx->shaState, &shaMsgState, ctx)) != 0)
        goto fail;
    if ((serr = CloneHashState(&SSLHashMD5, &ctx->md5State, &md5MsgState, ctx)) != 0)
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
    SSLFreeBuffer(&shaMsgState, ctx);
    SSLFreeBuffer(&md5MsgState, ctx);

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
    if (ctx->selectedCipherSpec.macAlgorithm->hmac->alg == HA_SHA384) {
        hashRef = &SSLHashSHA384;
        ctxHashState = &ctx->sha512State;
    } else {
        hashRef = &SSLHashSHA256;
        ctxHashState = &ctx->sha256State;
    }

    hashState.data = 0;
    if ((serr = CloneHashState(hashRef, ctxHashState, &hashState, ctx)) != 0)
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
    SSLFreeBuffer(&hashState, ctx);
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

    if ((serr = CloneHashState(&SSLHashSHA1, &ctx->shaState, &shaMsgState, ctx)) != 0)
        goto fail;
    if ((serr = CloneHashState(&SSLHashMD5, &ctx->md5State, &md5MsgState, ctx)) != 0)
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
    SSLFreeBuffer(&shaMsgState, ctx);
    SSLFreeBuffer(&md5MsgState, ctx);

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
            break;
    }

    if ((serr = CloneHashState(hashRef, ctxHashState, &hashState, ctx)) != 0)
        goto fail;

	assert(finished->length >= (hashRef->digestSize));
	finished->length = hashRef->digestSize;
	serr = hashRef->final(&hashState, finished);

fail:
    SSLFreeBuffer(&hashState, ctx);

    return serr;
}


const SslTlsCallouts Tls1Callouts = {
	tls1DecryptRecord,
	ssl3WriteRecord,
	tls1InitMac,
	tls1FreeMac,
	tls1ComputeMac,
	tls1GenerateKeyMaterial,
	tls1GenerateExportKeyAndIv,
	tls1GenerateMasterSecret,
	tls1ComputeFinishedMac,
	tls1ComputeCertVfyMac
};


const SslTlsCallouts Tls12Callouts = {
	tls1DecryptRecord,
	ssl3WriteRecord,
	tls1InitMac,
	tls1FreeMac,
	tls1ComputeMac,
	tls1GenerateKeyMaterial,
	tls1GenerateExportKeyAndIv,
	tls1GenerateMasterSecret,
	tls12ComputeFinishedMac,
	tls12ComputeCertVfyMac
};

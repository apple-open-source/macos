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
	File:		sslKeyExchange.c

	Contains:	Support for key exchange and server key exchange

	Written by:	Doug Mitchell

	Copyright: (c) 1999 by Apple Computer, Inc., all rights reserved.

*/

#include "sslContext.h"
#include "sslHandshake.h"
#include "sslMemory.h"
#include "sslDebug.h"
#include "sslUtils.h"
#include "appleCdsa.h"
#include "sslDigests.h"
#include "ModuleAttacher.h"
#include "sslBER.h"

#include <assert.h>
#include <string.h>

//#include <security_utilities/globalizer.h>
//#include <security_utilities/threading.h>
#include <Security/cssmapi.h>
#include <Security/SecKeyPriv.h>
#include <pthread.h>

#pragma mark -
#pragma mark *** forward static declarations ***
static OSStatus SSLGenServerDHParamsAndKey(SSLContext *ctx);
static OSStatus SSLEncodeDHKeyParams(SSLContext *ctx, uint8 *charPtr);
static OSStatus SSLDecodeDHKeyParams(SSLContext *ctx, uint8 **charPtr,
	UInt32 length);
static OSStatus SSLDecodeECDHKeyParams(SSLContext *ctx, uint8 **charPtr,
	UInt32 length);

#define DH_PARAM_DUMP		0
#if 	DH_PARAM_DUMP

static void dumpBuf(const char *name, SSLBuffer *buf)
{
	printf("%s:\n", name);
	uint8 *cp = buf->data;
	uint8 *endCp = cp + buf->length;
	
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

#pragma mark -
#pragma mark *** local D-H parameter generator ***
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

static pthread_once_t serverDhParamsControl = PTHREAD_ONCE_INIT;
/* the single global thing */
static struct ServerDhParams serverDhParams = {};

static void SSLInitServerDHParams(void) {
	CSSM_CSP_HANDLE cspHand;
	CSSM_CL_HANDLE clHand;			// not used here, just for 
									//   attachToModules()
	CSSM_TP_HANDLE tpHand;			// ditto
	CSSM_RETURN crtn;

	crtn = attachToModules(&cspHand, &clHand, &tpHand);
	if(crtn)
        return /*errSSLModuleAttach*/;

	CSSM_CC_HANDLE 	ccHandle;
	CSSM_DATA cParams = {0, NULL};
	
	crtn = CSSM_CSP_CreateKeyGenContext(cspHand,
		CSSM_ALGID_DH,
		SSL_DH_DEFAULT_PRIME_SIZE,
		NULL,					// Seed
		NULL,					// Salt
		NULL,					// StartDate
		NULL,					// EndDate
		&cParams,			// Params, may be NULL
		&ccHandle);
	if(crtn) {
		stPrintCdsaError("ServerDhParams CSSM_CSP_CreateKeyGenContext", crtn);
        return /*errSSLCrypto*/;
	}
	
	/* explicitly generate params and save them */
	sslDhDebug("^^^generating Diffie-Hellman parameters...");
	crtn = CSSM_GenerateAlgorithmParams(ccHandle, 
		SSL_DH_DEFAULT_PRIME_SIZE, &cParams);
	if(crtn) {
		stPrintCdsaError("ServerDhParams CSSM_GenerateAlgorithmParams", crtn);
		CSSM_DeleteContext(ccHandle);
        return /*errSSLCrypto*/;
	}
	CSSM_TO_SSLBUF(&cParams, &serverDhParams.paramBlock);
	OSStatus ortn = sslDecodeDhParams(&serverDhParams.paramBlock,
        &serverDhParams.prime, &serverDhParams.generator);
	if(ortn) {
		sslErrorLog("ServerDhParams: param decode error\n");
        return /*ortn*/;
	}
	CSSM_DeleteContext(ccHandle);
}

#endif	/* APPLE_DH */

#pragma mark -
#pragma mark *** RSA key exchange ***

/*
 * Client RSA Key Exchange msgs actually start with a two-byte
 * length field, contrary to the first version of RFC 2246, dated
 * January 1999. See RFC 2246, March 2002, section 7.4.7.1 for 
 * updated requirements. 
 */
#define RSA_CLIENT_KEY_ADD_LENGTH		1

typedef	CSSM_KEY_PTR	SSLRSAPrivateKey;

static OSStatus
SSLEncodeRSAKeyParams(SSLBuffer *keyParams, SSLRSAPrivateKey *key, SSLContext *ctx)
{   OSStatus    err;
    SSLBuffer   modulus, exponent;
    uint8       *charPtr;

	if(err = attachToCsp(ctx)) {
		return err;
	}
	
	/* Note currently ALL public keys are raw, obtained from the CL... */
	assert((*key)->KeyHeader.BlobType == CSSM_KEYBLOB_RAW);
	err = sslGetPubKeyBits(ctx,
		*key,
		ctx->cspHand,
		&modulus,
		&exponent);
	if(err) {
		SSLFreeBuffer(&modulus, ctx);
		SSLFreeBuffer(&exponent, ctx);
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
	SSLFreeBuffer(&modulus, ctx);
	SSLFreeBuffer(&exponent, ctx);
    return noErr;
}

static OSStatus
SSLEncodeRSAPremasterSecret(SSLContext *ctx)
{   SSLBuffer           randData;
    OSStatus            err;
    SSLProtocolVersion	maxVersion;
	
    if ((err = SSLAllocBuffer(&ctx->preMasterSecret, 
			SSL_RSA_PREMASTER_SECRET_SIZE, ctx)) != 0)
        return err;
    
	assert((ctx->negProtocolVersion == SSL_Version_3_0) ||
		   (ctx->negProtocolVersion == TLS_Version_1_0));
	sslGetMaxProtVersion(ctx, &maxVersion);
    SSLEncodeInt(ctx->preMasterSecret.data, maxVersion, 2);
    randData.data = ctx->preMasterSecret.data+2;
    randData.length = SSL_RSA_PREMASTER_SECRET_SIZE - 2;
    if ((err = sslRand(ctx, &randData)) != 0)
        return err;
    return noErr;
}

/*
 * Generate a server key exchange message signed by our RSA or DSA private key. 
 */
static OSStatus
SSLEncodeSignedServerKeyExchange(SSLRecord *keyExch, SSLContext *ctx)
{   OSStatus        err;
    uint8           *charPtr;
    size_t          outputLen;
    uint8           hashes[SSL_SHA1_DIGEST_LEN + SSL_MD5_DIGEST_LEN];
    SSLBuffer       exchangeParams,clientRandom,serverRandom,hashCtx, hash;
	uint8			*dataToSign;
	size_t			dataToSignLen;
	bool			isRsa = true;
    uint32 			maxSigLen;
    size_t	    	actSigLen;
	SSLBuffer		signature;
	const CSSM_KEY	*cssmKey;
	
    assert(ctx->protocolSide == SSL_ServerSide);
	assert(ctx->signingPubKey != NULL);
 	assert((ctx->negProtocolVersion == SSL_Version_3_0) ||
		   (ctx->negProtocolVersion == TLS_Version_1_0));
    exchangeParams.data = 0;
    hashCtx.data = 0;
	signature.data = 0;
	
	/* Set up parameter block to hash ==> exchangeParams */
	switch(ctx->selectedCipherSpec->keyExchangeMethod) {
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
				&ctx->encryptPubKey, ctx);
			break;
			
		#if 	APPLE_DH
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
			UInt32 len = ctx->dhParamsPrime.length + 
				ctx->dhParamsGenerator.length + 
				ctx->dhExchangePublic.length + 6 /* 3 length fields */;
			err = SSLAllocBuffer(&exchangeParams, len, ctx);
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
	if(err) {
		goto fail;
	}
			    
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
		
		if ((err = ReadyHash(&SSLHashMD5, &hashCtx, ctx)) != 0)
			goto fail;
		if ((err = SSLHashMD5.update(&hashCtx, &clientRandom)) != 0)
			goto fail;
		if ((err = SSLHashMD5.update(&hashCtx, &serverRandom)) != 0)
			goto fail;
		if ((err = SSLHashMD5.update(&hashCtx, &exchangeParams)) != 0)
			goto fail;
		if ((err = SSLHashMD5.final(&hashCtx, &hash)) != 0)
			goto fail;
		if ((err = SSLFreeBuffer(&hashCtx, ctx)) != 0)
			goto fail;
    }
	else {
		/* DSA - just use the SHA1 hash */
		dataToSign = &hashes[SSL_MD5_DIGEST_LEN];
		dataToSignLen = SSL_SHA1_DIGEST_LEN;
	}
    hash.data = &hashes[SSL_MD5_DIGEST_LEN];
    hash.length = SSL_SHA1_DIGEST_LEN;
    if ((err = ReadyHash(&SSLHashSHA1, &hashCtx, ctx)) != 0)
        goto fail;
    if ((err = SSLHashSHA1.update(&hashCtx, &clientRandom)) != 0)
        goto fail;
    if ((err = SSLHashSHA1.update(&hashCtx, &serverRandom)) != 0)
        goto fail;
    if ((err = SSLHashSHA1.update(&hashCtx, &exchangeParams)) != 0)
        goto fail;
    if ((err = SSLHashSHA1.final(&hashCtx, &hash)) != 0)
        goto fail;
    if ((err = SSLFreeBuffer(&hashCtx, ctx)) != 0)
        goto fail;
    
	/* preallocate a buffer for signing */
	err = SecKeyGetCSSMKey(ctx->signingPrivKeyRef, &cssmKey);
	if(err) {
		sslErrorLog("SSLEncodeSignedServerKeyExchange: SecKeyGetCSSMKey err %d\n",
			(int)err);
        goto fail;
	}
	err = sslGetMaxSigSize(cssmKey, &maxSigLen);
	if(err) {
        goto fail;
	}
	err = SSLAllocBuffer(&signature, maxSigLen, ctx);
	if(err) {
		goto fail;
	}
	
	err = sslRawSign(ctx,
		ctx->signingPrivKeyRef,
		dataToSign,			// one or two hashes
		dataToSignLen,
		signature.data,
		maxSigLen,
		&actSigLen);
	if(err) {
		goto fail;
	}
	assert(actSigLen <= maxSigLen);
	
	/* package it all up */
    outputLen = exchangeParams.length + 2 + actSigLen;
    keyExch->protocolVersion = ctx->negProtocolVersion;
    keyExch->contentType = SSL_RecordTypeHandshake;
    if ((err = SSLAllocBuffer(&keyExch->contents, outputLen+4, ctx)) != 0)
        goto fail;
    
    charPtr = keyExch->contents.data;
    *charPtr++ = SSL_HdskServerKeyExchange;
    charPtr = SSLEncodeInt(charPtr, outputLen, 3);
    
    memcpy(charPtr, exchangeParams.data, exchangeParams.length);
    charPtr += exchangeParams.length;
    charPtr = SSLEncodeInt(charPtr, actSigLen, 2);
	memcpy(charPtr, signature.data, actSigLen);
    assert((charPtr + actSigLen) == 
		   (keyExch->contents.data + keyExch->contents.length));
    
    err = noErr;
    
fail:
    SSLFreeBuffer(&hashCtx, ctx);
    SSLFreeBuffer(&exchangeParams, ctx);
    SSLFreeBuffer(&signature, ctx);
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
    SSLBuffer       hashOut, hashCtx, clientRandom, serverRandom;
    UInt16          modulusLen = 0, exponentLen = 0, signatureLen;
    uint8           *modulus = NULL, *exponent = NULL, *signature;
    uint8           hashes[SSL_SHA1_DIGEST_LEN + SSL_MD5_DIGEST_LEN];
    SSLBuffer       signedHashes;
 	uint8			*dataToSign;
	size_t			dataToSignLen;
	bool			isRsa = true;
	
	assert(ctx->protocolSide == SSL_ClientSide);
	signedHashes.data = 0;
    hashCtx.data = 0;
    
    if (message.length < 2) {
    	sslErrorLog("SSLDecodeSignedServerKeyExchange: msg len error 1\n");
        return errSSLProtocol;
    }
	
	/* first extract the key-exchange-method-specific parameters */
    uint8 *charPtr = message.data;
	uint8 *endCp = charPtr + message.length;
	switch(ctx->selectedCipherSpec->keyExchangeMethod) {
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
		#if		APPLE_DH
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
	
	signatureLen = SSLDecodeInt(charPtr, 2);
	charPtr += 2;
	if((charPtr + signatureLen) != endCp) {
		sslErrorLog("signedServerKeyExchange: msg len error 4\n");
		return errSSLProtocol;
	}
	signature = charPtr;
	
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
		
		if ((err = ReadyHash(&SSLHashMD5, &hashCtx, ctx)) != 0)
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
    if ((err = SSLFreeBuffer(&hashCtx, ctx)) != 0)
        goto fail;
    
    if ((err = ReadyHash(&SSLHashSHA1, &hashCtx, ctx)) != 0)
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
		ctx->peerPubKeyCsp,
		dataToSign,				/* plaintext */
		dataToSignLen,			/* plaintext length */
		signature,
		signatureLen);
	if(err) {
		sslErrorLog("SSLDecodeSignedServerKeyExchange: sslRawVerify "
			"returned %d\n", (int)err);
		goto fail;
	}
    
	/* Signature matches; now replace server key with new key (RSA only) */
	switch(ctx->selectedCipherSpec->keyExchangeMethod) {
		case SSL_RSA:
        case SSL_RSA_EXPORT:
		{
			SSLBuffer modBuf;
			SSLBuffer expBuf;
			
			/* first free existing peerKey */
			sslFreeKey(ctx->peerPubKeyCsp, 
				&ctx->peerPubKey,
				NULL);					/* no KCItem */
				
			/* and cook up a new one from raw bits */
			modBuf.data = modulus;
			modBuf.length = modulusLen;
			expBuf.data = exponent;
			expBuf.length = exponentLen;
			err = sslGetPubKeyFromBits(ctx,
				&modBuf,
				&expBuf,
				&ctx->peerPubKey,
				&ctx->peerPubKeyCsp);
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
    SSLFreeBuffer(&signedHashes, ctx);
    SSLFreeBuffer(&hashCtx, ctx);
    return err;
}

static OSStatus
SSLDecodeRSAKeyExchange(SSLBuffer keyExchange, SSLContext *ctx)
{   OSStatus            err;
    size_t        		outputLen, localKeyModulusLen;
    SSLProtocolVersion  version;
    Boolean				useEncryptKey = false;
	uint8				*src = NULL;
	SecKeyRef			keyRef = NULL;
    const CSSM_KEY		*cssmKey;
		
	assert(ctx->protocolSide == SSL_ServerSide);
	
	#if		SSL_SERVER_KEYEXCH_HACK
		/* 
		 * the way we work with Netscape.
		 * FIXME - maybe we should *require* an encryptPrivKey in this
		 * situation?
		 */
		if((ctx->selectedCipherSpec->keyExchangeMethod == SSL_RSA_EXPORT) &&
			(ctx->encryptPrivKey != NULL)) {
			useEncryptKey = true;
		}
		
	#else	/* !SSL_SERVER_KEYEXCH_HACK */
		/* The "correct" way, I think, which doesn't work with Netscape */
		if (ctx->encryptPrivKeyRef) {
			useEncryptKey = true;
		}
	#endif	/* SSL_SERVER_KEYEXCH_HACK */
	if (useEncryptKey) {
		keyRef  = ctx->encryptPrivKeyRef;
		/* FIXME: when 3420180 is implemented, pick appropriate creds here */
	} 
	else {
		keyRef  = ctx->signingPrivKeyRef;
		/* FIXME: when 3420180 is implemented, pick appropriate creds here */
	}
	err = SecKeyGetCSSMKey(keyRef, &cssmKey);
	if(err) {
		sslErrorLog("SSLDecodeRSAKeyExchange: SecKeyGetCSSMKey err %d\n",
			(int)err);
		return err;
	}
    
	localKeyModulusLen = sslKeyLengthInBytes(cssmKey);

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
    err = SSLAllocBuffer(&ctx->preMasterSecret, SSL_RSA_PREMASTER_SECRET_SIZE, ctx);
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
		CSSM_PADDING_PKCS1,
		src, 
		localKeyModulusLen,				// ciphertext len
		ctx->preMasterSecret.data,
		SSL_RSA_PREMASTER_SECRET_SIZE,	// plaintext buf available
		&outputLen);
    
	if(err != noErr) {									
		/* possible Bleichenbacher attack */
		sslLogNegotiateDebug("SSLDecodeRSAKeyExchange: RSA decrypt fail");
	}
	else if(outputLen != SSL_RSA_PREMASTER_SECRET_SIZE) {	
		sslLogNegotiateDebug("SSLDecodeRSAKeyExchange: premaster secret size error");
    	err = errSSLProtocol;							// not passed back to caller
    }
    
	if(err == noErr) {
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
	if(err != noErr) {
		/* 
		 * Obfuscate failures for defense against Bleichenbacher and
		 * Klima-Pokorny-Rosa attacks.
		 */
		SSLEncodeInt(ctx->preMasterSecret.data, ctx->negProtocolVersion, 2);
		SSLBuffer tmpBuf;
		tmpBuf.data   = ctx->preMasterSecret.data + 2;
		tmpBuf.length = SSL_RSA_PREMASTER_SECRET_SIZE - 2;
		/* must ignore failures here */
		sslRand(ctx, &tmpBuf);
	}
	
	/* in any case, save premaster secret (good or bogus) and proceed */
    return noErr;
}

static OSStatus
SSLEncodeRSAKeyExchange(SSLRecord *keyExchange, SSLContext *ctx)
{   OSStatus            err;
    size_t        		outputLen, peerKeyModulusLen;
    size_t				bufLen;
	uint8				*dst;
	bool				encodeLen = false;
	
	assert(ctx->protocolSide == SSL_ClientSide);
    if ((err = SSLEncodeRSAPremasterSecret(ctx)) != 0)
        return err;
    
    keyExchange->contentType = SSL_RecordTypeHandshake;
	assert((ctx->negProtocolVersion == SSL_Version_3_0) ||
			(ctx->negProtocolVersion == TLS_Version_1_0));
    keyExchange->protocolVersion = ctx->negProtocolVersion;
        
	peerKeyModulusLen = sslKeyLengthInBytes(ctx->peerPubKey);
	bufLen = peerKeyModulusLen + 4;
	#if 	RSA_CLIENT_KEY_ADD_LENGTH
	if(ctx->negProtocolVersion >= TLS_Version_1_0) {
		bufLen += 2;
		encodeLen = true;
	}
	#endif
    if ((err = SSLAllocBuffer(&keyExchange->contents, 
		bufLen,ctx)) != 0)
    {   
        return err;
    }
	dst = keyExchange->contents.data + 4;
	if(encodeLen) {
		dst += 2;
	}
    keyExchange->contents.data[0] = SSL_HdskClientKeyExchange;
	
	/* this is the record payload length */
    SSLEncodeInt(keyExchange->contents.data + 1, bufLen - 4, 3);
	if(encodeLen) {
		/* the length of the encrypted pre_master_secret */
		SSLEncodeInt(keyExchange->contents.data + 4, 			
			peerKeyModulusLen, 2);
	}
	err = sslRsaEncrypt(ctx,
		ctx->peerPubKey,
		/* FIXME - maybe this should be ctx->cspHand */
		ctx->peerPubKeyCsp,
		CSSM_PADDING_PKCS1,
		ctx->preMasterSecret.data, 
		SSL_RSA_PREMASTER_SECRET_SIZE,
		dst,
		peerKeyModulusLen,
		&outputLen);
	if(err) {
		return err;
	}
    
    assert(outputLen == encodeLen ? 
		keyExchange->contents.length - 6 :
		keyExchange->contents.length - 4 );
    
    return noErr;
}


#if APPLE_DH

#pragma mark -
#pragma mark *** Diffie-Hellman key exchange ***

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
    assert(ctx->protocolSide == SSL_ServerSide);
	
	/* 
	 * Obtain D-H parameters if we don't have them.
	 */
	if(ctx->dhParamsPrime.data == NULL) {
		assert(ctx->dhParamsGenerator.data == NULL);
        int prtn = pthread_once(&serverDhParamsControl, SSLInitServerDHParams);
        if (prtn) {
            sslErrorLog("SSLGenServerDHParamsAndKey: pthread_once %d\n",
                prtn);
            return errSSLInternal;
        }
		ortn = SSLCopyBuffer(&serverDhParams.prime, &ctx->dhParamsPrime);
		if(ortn) {
			return ortn;
		}
		ortn = SSLCopyBuffer(&serverDhParams.generator, &ctx->dhParamsGenerator);
		if(ortn) {
			return ortn;
		}
		ortn = SSLCopyBuffer(&serverDhParams.paramBlock, &ctx->dhParamsEncoded);
		if(ortn) {
			return ortn;
		}
	}
	
	/* generate per-session D-H key pair */
	sslFreeKey(ctx->cspHand, &ctx->dhPrivate, NULL);
	SSLFreeBuffer(&ctx->dhExchangePublic, ctx);
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
	return noErr;
} 

/*
 * Encode DH params and public key in caller-supplied buffer. 
 */
static OSStatus 
SSLEncodeDHKeyParams(
	SSLContext *ctx,
	uint8 *charPtr)
{
    assert(ctx->protocolSide == SSL_ServerSide);
	assert(ctx->dhParamsPrime.data != NULL);
	assert(ctx->dhParamsGenerator.data != NULL);
	assert(ctx->dhExchangePublic.data != NULL);
	
	charPtr = SSLEncodeInt(charPtr, ctx->dhParamsPrime.length, 2);
	memcpy(charPtr, ctx->dhParamsPrime.data, ctx->dhParamsPrime.length);
	charPtr += ctx->dhParamsPrime.length;
	
	charPtr = SSLEncodeInt(charPtr, ctx->dhParamsGenerator.length, 2);
	memcpy(charPtr, ctx->dhParamsGenerator.data, 
		ctx->dhParamsGenerator.length);
	charPtr += ctx->dhParamsGenerator.length;
	
	charPtr = SSLEncodeInt(charPtr, ctx->dhExchangePublic.length, 2);
	memcpy(charPtr, ctx->dhExchangePublic.data, 
		ctx->dhExchangePublic.length);

	dumpBuf("server prime", &ctx->dhParamsPrime);
	dumpBuf("server generator", &ctx->dhParamsGenerator);
	dumpBuf("server pub key", &ctx->dhExchangePublic);
	return noErr;
}

/*
 * Decode DH params and server public key.
 */
static OSStatus
SSLDecodeDHKeyParams(
	SSLContext *ctx,
	uint8 **charPtr,		// IN/OUT
	UInt32 length)
{   
	OSStatus        err = noErr;
	
	assert(ctx->protocolSide == SSL_ClientSide);
    uint8 *endCp = *charPtr + length;

	/* Allow reuse via renegotiation */
    SSLFreeBuffer(&ctx->dhParamsPrime, ctx);
    SSLFreeBuffer(&ctx->dhParamsGenerator, ctx);
	SSLFreeBuffer(&ctx->dhPeerPublic, ctx);
	
	/* Prime, with a two-byte length */
	UInt32 len = SSLDecodeInt(*charPtr, 2);
	(*charPtr) += 2;
	if((*charPtr + len) > endCp) {
		return errSSLProtocol;
	}
	err = SSLAllocBuffer(&ctx->dhParamsPrime, len, ctx);
	if(err) {
		return err;
	}
	memmove(ctx->dhParamsPrime.data, *charPtr, len);
	(*charPtr) += len;
	
	/* Generator, with a two-byte length */
	len = SSLDecodeInt(*charPtr, 2);
	(*charPtr) += 2;
	if((*charPtr + len) > endCp) {
		return errSSLProtocol;
	}
	err = SSLAllocBuffer(&ctx->dhParamsGenerator, len, ctx);
	if(err) {
		return err;
	}
	memmove(ctx->dhParamsGenerator.data, *charPtr, len);
	(*charPtr) += len;
	
	/* peer public key, with a two-byte length */
	len = SSLDecodeInt(*charPtr, 2);
	(*charPtr) += 2;
	err = SSLAllocBuffer(&ctx->dhPeerPublic, len, ctx);
	if(err) {
		return err;
	}
	memmove(ctx->dhPeerPublic.data, *charPtr, len);
	(*charPtr) += len;
	
	dumpBuf("client peer pub", &ctx->dhPeerPublic);
	dumpBuf("client prime", &ctx->dhParamsPrime);
	dumpBuf("client generator", &ctx->dhParamsGenerator);
		
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

    assert(ctx->protocolSide == SSL_ClientSide);
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
	return noErr;
}


static OSStatus
SSLEncodeDHanonServerKeyExchange(SSLRecord *keyExch, SSLContext *ctx)
{   
	OSStatus            ortn = noErr;
	
	assert((ctx->negProtocolVersion == SSL_Version_3_0) ||
			(ctx->negProtocolVersion == TLS_Version_1_0));
	assert(ctx->protocolSide == SSL_ServerSide);

	/* 
	 * Obtain D-H parameters (if we don't have them) and a key pair. 
	 */
	ortn = SSLGenServerDHParamsAndKey(ctx);
	if(ortn) {
		return ortn;
	}
	
	UInt32 length = 6 + 
		ctx->dhParamsPrime.length + 
		ctx->dhParamsGenerator.length + ctx->dhExchangePublic.length;
	
	keyExch->protocolVersion = ctx->negProtocolVersion;
	keyExch->contentType = SSL_RecordTypeHandshake;
	if ((ortn = SSLAllocBuffer(&keyExch->contents, length+4, ctx)) != 0)
		return ortn;
	
	uint8 *charPtr = keyExch->contents.data;
	*charPtr++ = SSL_HdskServerKeyExchange;
	charPtr = SSLEncodeInt(charPtr, length, 3);
	
	/* encode prime, generator, our public key */
	return SSLEncodeDHKeyParams(ctx, charPtr);
}


static OSStatus
SSLDecodeDHanonServerKeyExchange(SSLBuffer message, SSLContext *ctx)
{   
	OSStatus        err = noErr;
	
	assert(ctx->protocolSide == SSL_ClientSide);
    if (message.length < 6) {
    	sslErrorLog("SSLDecodeDHanonServerKeyExchange error: msg len %u\n",
    		(unsigned)message.length);
        return errSSLProtocol;
    }
    uint8 *charPtr = message.data;
	err = SSLDecodeDHKeyParams(ctx, &charPtr, message.length);
	if(err == noErr) {
		if((message.data + message.length) != charPtr) {
			err = errSSLProtocol;
		}
	}
	return err;
}

static OSStatus
SSLDecodeDHClientKeyExchange(SSLBuffer keyExchange, SSLContext *ctx)
{   
	OSStatus        ortn = noErr;
    unsigned int    publicLen;

	assert(ctx->protocolSide == SSL_ServerSide);
	if(ctx->dhParamsPrime.data == NULL) {
		/* should never happen */
		assert(0);
		return errSSLInternal;
	}
	
	/* this message simply contains the client's public DH key */
	uint8 *charPtr = keyExchange.data;
    publicLen = SSLDecodeInt(charPtr, 2);
	charPtr += 2;
	if((keyExchange.length != publicLen + 2) ||
	   (publicLen > ctx->dhParamsPrime.length)) {
        return errSSLProtocol;
    }
	SSLFreeBuffer(&ctx->dhPeerPublic, ctx);	// allow reuse via renegotiation
	ortn = SSLAllocBuffer(&ctx->dhPeerPublic, publicLen, ctx);
	if(ortn) {
		return ortn;
	}
	memmove(ctx->dhPeerPublic.data, charPtr, publicLen);
	
	/* DH Key exchange, result --> premaster secret */
	SSLFreeBuffer(&ctx->preMasterSecret, ctx);
	ortn = sslDhKeyExchange(ctx, ctx->dhParamsPrime.length * 8, 
		&ctx->preMasterSecret);

	dumpBuf("server peer pub", &ctx->dhPeerPublic);
	dumpBuf("server premaster", &ctx->preMasterSecret);
	return ortn;
}

static OSStatus
SSLEncodeDHClientKeyExchange(SSLRecord *keyExchange, SSLContext *ctx)
{   OSStatus            err;
    size_t				outputLen;
    
	assert(ctx->protocolSide == SSL_ClientSide);
    if ((err = SSLGenClientDHKeyAndExchange(ctx)) != 0)
        return err;
    
    outputLen = ctx->dhExchangePublic.length + 2;
    
    keyExchange->contentType = SSL_RecordTypeHandshake;
	assert((ctx->negProtocolVersion == SSL_Version_3_0) ||
			(ctx->negProtocolVersion == TLS_Version_1_0));
    keyExchange->protocolVersion = ctx->negProtocolVersion;
    
    if ((err = SSLAllocBuffer(&keyExchange->contents,outputLen + 4,ctx)) != 0)
        return err;
    
    keyExchange->contents.data[0] = SSL_HdskClientKeyExchange;
    SSLEncodeInt(keyExchange->contents.data+1, 
		ctx->dhExchangePublic.length+2, 3);
    
    SSLEncodeInt(keyExchange->contents.data+4, 
		ctx->dhExchangePublic.length, 2);
    memcpy(keyExchange->contents.data+6, ctx->dhExchangePublic.data, 
		ctx->dhExchangePublic.length);

	dumpBuf("client pub key", &ctx->dhExchangePublic);
	dumpBuf("client premaster", &ctx->preMasterSecret);
    return noErr;
}

#endif	/* APPLE_DH */

#pragma mark -
#pragma mark *** ECDSA key exchange ***

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

    assert(ctx->protocolSide == SSL_ClientSide);
	
	switch(ctx->selectedCipherSpec->keyExchangeMethod) {
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
		assert(ctx->cspHand != 0);
		sslFreeKey(ctx->cspHand, &ctx->ecdhPrivate, NULL);
		SSLFreeBuffer(&ctx->ecdhExchangePublic, ctx);
		ortn = SecKeyGetCSSMKey(ctx->signingPrivKeyRef, (const CSSM_KEY **)&ctx->ecdhPrivate);
		if(ortn) {
			return ortn;
		}
		ortn = SecKeyGetCSPHandle(ctx->signingPrivKeyRef, &ctx->ecdhPrivCspHand);
		if(ortn) {
			sslErrorLog("SSLGenClientECDHKeyAndExchange: SecKeyGetCSPHandle err %d\n",
				(int)ortn);
		}
		sslEcdsaDebug("+++ Extracted ECDH private key");
	}
	else {
		/* generate a new pair */
		ortn = sslEcdhGenerateKeyPair(ctx, ctx->ecdhPeerCurve);
		if(ortn) {
			return ortn;
		}
		sslEcdsaDebug("+++ Generated %u bit (%u byte) ECDH key pair",
			(unsigned)ctx->ecdhPrivate->KeyHeader.LogicalKeySizeInBits,
			(unsigned)((ctx->ecdhPrivate->KeyHeader.LogicalKeySizeInBits + 7) / 8));
	}
	
	
	/* do the exchange --> premaster secret */
	ortn = sslEcdhKeyExchange(ctx, &ctx->preMasterSecret);
	if(ortn) {
		return ortn;
	}
	return noErr;
}


/*
 * Decode ECDH params and server public key.
 */
static OSStatus
SSLDecodeECDHKeyParams(
	SSLContext *ctx,
	uint8 **charPtr,		// IN/OUT
	UInt32 length)
{   
	OSStatus        err = noErr;
	
	sslEcdsaDebug("+++ Decoding ECDH Server Key Exchange");

	assert(ctx->protocolSide == SSL_ClientSide);
    uint8 *endCp = *charPtr + length;

	/* Allow reuse via renegotiation */
	SSLFreeBuffer(&ctx->ecdhPeerPublic, ctx);
	
	/*** ECParameters - just a curveType and a named curve ***/
	
	/* 1-byte curveType, we only allow one type */
	uint8 curveType = **charPtr;
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
	err = SSLAllocBuffer(&ctx->ecdhPeerPublic, len, ctx);
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
    
	assert(ctx->protocolSide == SSL_ClientSide);
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
	assert((ctx->negProtocolVersion == SSL_Version_3_0) ||
			(ctx->negProtocolVersion == TLS_Version_1_0));
    keyExchange->protocolVersion = ctx->negProtocolVersion;
    
    if ((err = SSLAllocBuffer(&keyExchange->contents,outputLen + 4,ctx)) != 0)
        return err;
    
    keyExchange->contents.data[0] = SSL_HdskClientKeyExchange;
	if(emptyMsg) {
		/* does "empty message" include a zero length...? */
		SSLEncodeInt(keyExchange->contents.data+1, 0, 3);
		sslEcdsaDebug("+++ Sending EMPTY ECDH Client Key Exchange");
	}
	else {
		SSLEncodeInt(keyExchange->contents.data+1, 
			ctx->ecdhExchangePublic.length+1, 3);
		
		/* just a 1-byte length here... */
		SSLEncodeInt(keyExchange->contents.data+4, 
			ctx->ecdhExchangePublic.length, 1);
		memcpy(keyExchange->contents.data+5, ctx->ecdhExchangePublic.data, 
			ctx->ecdhExchangePublic.length);
		sslEcdsaDebug("+++ Encoded ECDH Client Key Exchange");
	}
	
	dumpBuf("client pub key", &ctx->ecdhExchangePublic);
	dumpBuf("client premaster", &ctx->preMasterSecret);
    return noErr;
}

#pragma mark -
#pragma mark *** Public Functions ***
OSStatus
SSLEncodeServerKeyExchange(SSLRecord *keyExch, SSLContext *ctx)
{   OSStatus      err;
    
    switch (ctx->selectedCipherSpec->keyExchangeMethod)
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
            return unimpErr;
    }
    
    return noErr;
}

OSStatus
SSLProcessServerKeyExchange(SSLBuffer message, SSLContext *ctx)
{   
	OSStatus      err;
    
    switch (ctx->selectedCipherSpec->keyExchangeMethod) {   
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
            err = unimpErr;
			break;
    }
    
    return err;
}

OSStatus
SSLEncodeKeyExchange(SSLRecord *keyExchange, SSLContext *ctx)
{   OSStatus      err;
    
    assert(ctx->protocolSide == SSL_ClientSide);
    
    switch (ctx->selectedCipherSpec->keyExchangeMethod) {
		case SSL_RSA:
        case SSL_RSA_EXPORT:
            err = SSLEncodeRSAKeyExchange(keyExchange, ctx);
            break;
        #if		APPLE_DH
		case SSL_DHE_RSA:
		case SSL_DHE_RSA_EXPORT:
		case SSL_DHE_DSS:
		case SSL_DHE_DSS_EXPORT:
        case SSL_DH_anon:
		case SSL_DH_anon_EXPORT:
            err = SSLEncodeDHClientKeyExchange(keyExchange, ctx);
            break;
        #endif
		case SSL_ECDH_ECDSA:
		case SSL_ECDHE_ECDSA:
		case SSL_ECDH_RSA:
		case SSL_ECDHE_RSA:
		case SSL_ECDH_anon:
			err = SSLEncodeECDHClientKeyExchange(keyExchange, ctx);
            break;

		
        default:
            err = unimpErr;
    }
    
    return err;
}

OSStatus
SSLProcessKeyExchange(SSLBuffer keyExchange, SSLContext *ctx)
{   OSStatus      err;
    
    switch (ctx->selectedCipherSpec->keyExchangeMethod)
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
            if ((err = SSLDecodeDHClientKeyExchange(keyExchange, ctx)) != 0)
                return err;
            break;
        #endif
        default:
            return unimpErr;
    }
    
    return noErr;
}

OSStatus
SSLInitPendingCiphers(SSLContext *ctx)
{   OSStatus        err;
    SSLBuffer       key;
    uint8           *keyDataProgress, *keyPtr, *ivPtr;
    int             keyDataLen;
    CipherContext   *serverPending, *clientPending;
        
    key.data = 0;
    
    ctx->readPending.macRef = ctx->selectedCipherSpec->macAlgorithm;
    ctx->writePending.macRef = ctx->selectedCipherSpec->macAlgorithm;
    ctx->readPending.symCipher = ctx->selectedCipherSpec->cipher;
    ctx->writePending.symCipher = ctx->selectedCipherSpec->cipher;
    ctx->readPending.sequenceNum.high = ctx->readPending.sequenceNum.low = 0;
    ctx->writePending.sequenceNum.high = ctx->writePending.sequenceNum.low = 0;
    
    keyDataLen = ctx->selectedCipherSpec->macAlgorithm->hash->digestSize +
                     ctx->selectedCipherSpec->cipher->secretKeySize;
    if (ctx->selectedCipherSpec->isExportable == NotExportable)
        keyDataLen += ctx->selectedCipherSpec->cipher->ivSize;
    keyDataLen *= 2;        /* two of everything */
    
    if ((err = SSLAllocBuffer(&key, keyDataLen, ctx)) != 0)
        return err;
	assert(ctx->sslTslCalls != NULL);
    if ((err = ctx->sslTslCalls->generateKeyMaterial(key, ctx)) != 0)
        goto fail;
    
    if (ctx->protocolSide == SSL_ServerSide)
    {   serverPending = &ctx->writePending;
        clientPending = &ctx->readPending;
    }
    else
    {   serverPending = &ctx->readPending;
        clientPending = &ctx->writePending;
    }
    
    keyDataProgress = key.data;
    memcpy(clientPending->macSecret, keyDataProgress, 
		ctx->selectedCipherSpec->macAlgorithm->hash->digestSize);
    keyDataProgress += ctx->selectedCipherSpec->macAlgorithm->hash->digestSize;
    memcpy(serverPending->macSecret, keyDataProgress, 
		ctx->selectedCipherSpec->macAlgorithm->hash->digestSize);
    keyDataProgress += ctx->selectedCipherSpec->macAlgorithm->hash->digestSize;
    
	/* init the reusable-per-record MAC contexts */
	err = ctx->sslTslCalls->initMac(clientPending, ctx);
	if(err) {
		goto fail;
	}
	err = ctx->sslTslCalls->initMac(serverPending, ctx);
	if(err) {
		goto fail;
	}
	
    if (ctx->selectedCipherSpec->isExportable == NotExportable)
    {   keyPtr = keyDataProgress;
        keyDataProgress += ctx->selectedCipherSpec->cipher->secretKeySize;
        /* Skip server write key to get to IV */
		
		UInt8 ivSize = ctx->selectedCipherSpec->cipher->ivSize;
		
		if (ivSize == 0)
		{
			ivPtr = NULL;
		}
		else
		{
			ivPtr = keyDataProgress + ctx->selectedCipherSpec->cipher->secretKeySize;
		}
		
        if ((err = ctx->selectedCipherSpec->cipher->initialize(keyPtr, ivPtr,
                                    clientPending, ctx)) != 0)
            goto fail;
        keyPtr = keyDataProgress;
        keyDataProgress += ctx->selectedCipherSpec->cipher->secretKeySize;
		
        /* Skip client write IV to get to server write IV */
		if (ivSize == 0)
		{
			ivPtr = NULL;
		}
		else
		{
			ivPtr = keyDataProgress + ctx->selectedCipherSpec->cipher->ivSize;
		}
		
        if ((err = ctx->selectedCipherSpec->cipher->initialize(keyPtr, ivPtr,
                                    serverPending, ctx)) != 0)
            goto fail;
    }
    else {
        uint8		clientExportKey[16], serverExportKey[16], 
					clientExportIV[16],  serverExportIV[16];
        SSLBuffer   clientWrite, serverWrite;
        SSLBuffer	finalClientWrite, finalServerWrite;
		SSLBuffer	finalClientIV, finalServerIV;
		
        assert(ctx->selectedCipherSpec->cipher->keySize <= 16);
        assert(ctx->selectedCipherSpec->cipher->ivSize <= 16);
        
		/* Inputs to generateExportKeyAndIv are clientRandom, serverRandom,
		 *    clientWriteKey, serverWriteKey. The first two are already present
		 *    in ctx.
		 * Outputs are a key and IV for each of {server, client}.
		 */
        clientWrite.data = keyDataProgress;
        clientWrite.length = ctx->selectedCipherSpec->cipher->secretKeySize;
        serverWrite.data = keyDataProgress + clientWrite.length;
        serverWrite.length = ctx->selectedCipherSpec->cipher->secretKeySize;
		finalClientWrite.data = clientExportKey;
		finalServerWrite.data   = serverExportKey;
		finalClientIV.data      = clientExportIV;
		finalServerIV.data      = serverExportIV;
		finalClientWrite.length = 16;
		finalServerWrite.length = 16;
		/* these can be zero */
		finalClientIV.length    = ctx->selectedCipherSpec->cipher->ivSize;
		finalServerIV.length    = ctx->selectedCipherSpec->cipher->ivSize;

		assert(ctx->sslTslCalls != NULL);
		err = ctx->sslTslCalls->generateExportKeyAndIv(ctx, clientWrite, serverWrite,
			finalClientWrite, finalServerWrite, finalClientIV, finalServerIV);
		if(err) {
			goto fail;
		}
        if ((err = ctx->selectedCipherSpec->cipher->initialize(clientExportKey, 
				clientExportIV, clientPending, ctx)) != 0)
            goto fail;
        if ((err = ctx->selectedCipherSpec->cipher->initialize(serverExportKey, 
				serverExportIV, serverPending, ctx)) != 0)
            goto fail;
    }
    
	/* Ciphers are ready for use */
    ctx->writePending.ready = 1;
    ctx->readPending.ready = 1;
    
	/* Ciphers get swapped by sending or receiving a change cipher spec message */
    
    err = noErr;
fail:
    SSLFreeBuffer(&key, ctx);
    return err;
}


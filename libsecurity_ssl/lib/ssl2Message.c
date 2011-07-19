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

#include "ssl.h"
#include "ssl2.h"
#include "sslRecord.h"
#include "sslMemory.h"
#include "sslContext.h"
#include "sslAlertMessage.h"
#include "sslHandshake.h"
#include "sslSession.h"
#include "sslDebug.h"
#include "cipherSpecs.h"
#include "appleCdsa.h"
#include "sslUtils.h"
#include <Security/SecKeyPriv.h>
#include <string.h>
#include <assert.h>

OSStatus
SSL2ProcessClientHello(SSLBuffer msg, SSLContext *ctx)
{   OSStatus      		err;
    UInt8               *charPtr, *cipherList;
    unsigned            i, j, cipherKindCount, sessionIDLen, challengeLen;
    SSL2CipherKind      cipherKind;
    SSLCipherSuite      matchingCipher, selectedCipher;
    SSLProtocolVersion  negVersion;
    
    if (msg.length < 27) {
		sslErrorLog("SSL2ProcessClientHello: msg len error 1\n");
        return errSSLProtocol;
    }
    
    charPtr = msg.data;
    
    ctx->clientReqProtocol = (SSLProtocolVersion)SSLDecodeInt(charPtr, 2);
	err = sslVerifyProtVersion(ctx, ctx->clientReqProtocol, &negVersion);
	if(err) {
		return err;
	}
	
	/* 
	 * Note we can be here, processing a v2 client hello, even if 
	 * we don't support SSL2. That can happen if the client is 
	 * sending a v2 hello with an attempt to upgrade.
	 */
    if (ctx->negProtocolVersion == SSL_Version_Undetermined) {   
		#ifndef	NDEBUG
        sslLogNegotiateDebug("===SSL2 server: negVersion was undetermined; "
			"is %s", protocolVersStr(negVersion));
		#endif
        ctx->negProtocolVersion = negVersion;
		if(negVersion >= TLS_Version_1_0) {
			ctx->sslTslCalls = &Tls1Callouts;
		}
		else {
			/* default from context init */
			assert(ctx->sslTslCalls == &Ssl3Callouts);
		}
    }
    
    charPtr += 2;
    cipherKindCount = SSLDecodeInt(charPtr, 2);
    charPtr += 2;
    if (cipherKindCount % 3 != 0) {
		sslErrorLog("SSL2ProcessClientHello: cipherKindCount error\n");
        return errSSLProtocol;
    }
    cipherKindCount /= 3;
    sessionIDLen = SSLDecodeInt(charPtr, 2);
    charPtr += 2;
    challengeLen = SSLDecodeInt(charPtr, 2);
    charPtr += 2;
    
    if (msg.length != 8 + 3*cipherKindCount + sessionIDLen + challengeLen ||
        (sessionIDLen != 0 && sessionIDLen != 16) ||
        challengeLen < 16 || challengeLen > 32 ) {
		sslErrorLog("SSL2ProcessClientHello: msg len error 2\n");
        return errSSLProtocol;
    }
    cipherList = charPtr;
    selectedCipher = SSL_NO_SUCH_CIPHERSUITE;

	assert(ctx->negProtocolVersion >= SSL_Version_2_0);	// i.e., not undetermined
    if (ctx->negProtocolVersion >= SSL_Version_3_0) {
		/* If we're negotiating an SSL 3.0 session, use SSL 3.0 suites first */
        for (i = 0; i < cipherKindCount; i++) {
            cipherKind = (SSL2CipherKind)SSLDecodeInt(charPtr, 3);
            charPtr += 3;
            if (selectedCipher != SSL_NO_SUCH_CIPHERSUITE)
                continue;
            if ((((UInt32)cipherKind) & 0xFF0000) != 0)
                continue;       /* Skip SSL 2 suites */
            matchingCipher = (SSLCipherSuite)((UInt32)cipherKind & 0x00FFFF);
            for (j = 0; j<ctx->numValidCipherSpecs; j++) {
                if (ctx->validCipherSpecs[j].cipherSpec == matchingCipher) {
                    selectedCipher = matchingCipher;
                    break;
                }
			}	/* searching thru all our valid ciphers */
        }	/* for each client cipher */
    }	/* v3 or greater */
    
	if(selectedCipher == SSL_NO_SUCH_CIPHERSUITE) {
		/* try again using SSL2 ciphers only */
	    charPtr = cipherList;
		for (i = 0; i < cipherKindCount; i++) {
			cipherKind = (SSL2CipherKind)SSLDecodeInt(charPtr, 3);
			charPtr += 3;
			if (selectedCipher == SSL_NO_SUCH_CIPHERSUITE) {
				/* After we find one, just keep advancing ptr past 
				 * the unused ones */
				if ((((UInt32)cipherKind) & 0xFF0000) != 0) {
					/* If it's a real SSL2 spec, look for it in the list */
					matchingCipher = SSL_NO_SUCH_CIPHERSUITE;
					for (j = 0; j < SSL2CipherMapCount; j++) {
						if (cipherKind == SSL2CipherMap[j].cipherKind) {
							matchingCipher = SSL2CipherMap[j].cipherSuite;
							break;
						}
					}
				}	/* real 3-byte SSL2 suite */
				else {
					/* if the first byte is zero, it's an encoded SSL 3 CipherSuite */
					matchingCipher = (SSLCipherSuite)((UInt32)cipherKind & 0x00FFFF);
					/* 
					* One more restriction - if we've negotiated a v2 session,
					* ignore this matching cipher if it's not in the SSL2 map.
					*/
					if(ctx->negProtocolVersion < SSL_Version_3_0) {
						int isInMap = 0;
						for (j = 0; j < SSL2CipherMapCount; j++) {
							if (matchingCipher == SSL2CipherMap[j].cipherSuite) {
								isInMap = 1;
								break;
							}
						}
						if(!isInMap) {
							/* Sorry, no can do */
							matchingCipher = SSL_NO_SUCH_CIPHERSUITE;
						}
					}	/* SSL2 check */
				}	/* two-byte suite */
				
				/* now see if we are enabled for this cipher */
				if (matchingCipher != SSL_NO_SUCH_CIPHERSUITE) {
					for (j = 0; j < ctx->numValidCipherSpecs; j++) {
						if (ctx->validCipherSpecs[j].cipherSpec == matchingCipher) {
							selectedCipher = matchingCipher;
							break;
						}
					}
				}
			}	/* not ignoring this suite */
		}	/* for each suite in the hello msg */
	}		/* not found in SSL3 ciphersuites */
	
    if (selectedCipher == SSL_NO_SUCH_CIPHERSUITE)
        return errSSLNegotiation;
    
    ctx->selectedCipher = selectedCipher;
    err = FindCipherSpec(ctx);
    if(err != 0) {
        return err;
    }
    if (sessionIDLen > 0 && ctx->peerID.data != 0)
    {   /* Don't die on error; just treat it as an uncacheable session */
        err = SSLAllocBuffer(&ctx->sessionID, sessionIDLen, ctx);
        if (err == 0)
            memcpy(ctx->sessionID.data, charPtr, sessionIDLen);
    }
    charPtr += sessionIDLen;
    
    ctx->ssl2ChallengeLength = challengeLen;
    memset(ctx->clientRandom, 0, SSL_CLIENT_SRVR_RAND_SIZE);
    memcpy(ctx->clientRandom + SSL_CLIENT_SRVR_RAND_SIZE - challengeLen, 
		charPtr, challengeLen);
    charPtr += challengeLen;
    assert(charPtr == msg.data + msg.length);
    
    return noErr;
}

/*
 * The SSL v2 spec says that the challenge string sent by the client can be
 * between 16 and 32 bytes. However all Netscape enterprise servers actually
 * require a 16 byte challenge. Q.v. cdnow.com, store.apple.com. 
 * Unfortunately this means that when we're trying to do an
 * SSL2 hello with possible upgrade, we have to limit ourself to a 
 * 16-byte clientRandom, which we have to concatenate to 16 bytes of zeroes 
 * if we end up with a 3.0 or 3.1 connection. Thus we lose 16 bytes of entropy.
 */
#define SSL2_CHALLENGE_LEN	16

OSStatus
SSL2EncodeClientHello(SSLBuffer *msg, SSLContext *ctx)
{   OSStatus 	err;
    UInt8 		*charPtr;
    unsigned   	i, j;
	int		 	useSSL3Ciphers = 0;
	int			totalCipherCount;
    int			sessionIDLen;
    UInt16		version;
    SSLBuffer	sessionIdentifier, randomData;
    SSLProtocolVersion	maxVersion;
	
	assert(ctx->versionSsl2Enable);
	err = sslGetMaxProtVersion(ctx, &maxVersion);
	if(err) {
		/* we don't have a protocol enabled */
		return err;
	}
	version = maxVersion;
	if(version > SSL_Version_2_0) {
		/* see if server can handle upgrading */
		useSSL3Ciphers = 1;
	}

	#ifndef	NDEBUG
	sslLogNegotiateDebug("===SSL client: proclaiming %s capable", 
		protocolVersStr((SSLProtocolVersion)version));
    #endif
	
    if (useSSL3Ciphers != 0)
        totalCipherCount = ctx->numValidNonSSLv2Specs;
    else
        totalCipherCount = 0;
        
    for (i = 0; i < SSL2CipherMapCount; i++)
        for (j = 0; j < ctx->numValidCipherSpecs; j++)
            if (ctx->validCipherSpecs[j].cipherSpec == SSL2CipherMap[i].cipherSuite)
            {   totalCipherCount++;
                break;
            }
    
	if(totalCipherCount == 0) {
		sslErrorLog("SSL2EncodeClientHello: no valid ciphers for SSL2");
		return errSSLBadConfiguration;
	}
    sessionIDLen = 0;
    sessionIdentifier.data = 0;
    if (ctx->resumableSession.data != 0)
    {   if ((err = SSLRetrieveSessionID(ctx->resumableSession, &sessionIdentifier, ctx)) != 0)
            return err;
        sessionIDLen = sessionIdentifier.length;
    }
    
	/* msg length = 9 + 3 * totalCipherCount + sessionIDLen + 16 bytes of challenge
	 *  Use exactly 16 bytes of challenge because Netscape products have a bug
	 *  that requires this length
	 */ 
    if ((err = SSLAllocBuffer(msg, 9 + (3*totalCipherCount) + sessionIDLen + 
			SSL2_CHALLENGE_LEN, ctx)) != 0)
    {   SSLFreeBuffer(&sessionIdentifier, ctx);
        return err;
    }
    
    charPtr = msg->data;
    *charPtr++ = SSL2_MsgClientHello;
    charPtr = SSLEncodeInt(charPtr, version, 2);
    charPtr = SSLEncodeInt(charPtr, 3*totalCipherCount, 2);
    charPtr = SSLEncodeInt(charPtr, sessionIDLen, 2);
    charPtr = SSLEncodeInt(charPtr, SSL2_CHALLENGE_LEN, 2);
    
	/* 
	 * If we can send SSL3 ciphers, encode the non-SSLv2 two-byte cipher specs 
	 * into three-byte CipherKinds with a leading 0.
	 */
    if (useSSL3Ciphers != 0) {
        for (i = 0; i < ctx->numValidCipherSpecs; i++) {
			if(CIPHER_SUITE_IS_SSLv2(ctx->validCipherSpecs[i].cipherSpec)) {
				continue;
			}
			sslLogNegotiateDebug("ssl2EncodeClientHello sending spec %x", 
				(unsigned)ctx->validCipherSpecs[i].cipherSpec);
            charPtr = SSLEncodeInt(charPtr, ctx->validCipherSpecs[i].cipherSpec, 3);
    	}
	}

	/* Now send those SSL2 specs for which we have implementations */
    for (i = 0; i < SSL2CipherMapCount; i++)
        for (j = 0; j < ctx->numValidCipherSpecs; j++)
            if (ctx->validCipherSpecs[j].cipherSpec == SSL2CipherMap[i].cipherSuite) {   
				sslLogNegotiateDebug("ssl2EncodeClientHello sending spec %x", 
					SSL2CipherMap[i].cipherKind);		
				charPtr = SSLEncodeInt(charPtr, SSL2CipherMap[i].cipherKind, 3);
                break;
            }
    
    if (sessionIDLen > 0)
    {   memcpy(charPtr, sessionIdentifier.data, sessionIDLen);
        charPtr += sessionIDLen;
        SSLFreeBuffer(&sessionIdentifier, ctx);
    }
    
    randomData.data = charPtr;
    randomData.length = SSL2_CHALLENGE_LEN;
    if ((err = sslRand(ctx, &randomData)) != 0)
    {   SSLFreeBuffer(msg, ctx);
        return err;
    }
    charPtr += SSL2_CHALLENGE_LEN;
    
	/* Zero out the first 16 bytes of clientRandom, and store 
	 * the challenge in the second 16 bytes */
	#if (SSL2_CHALLENGE_LEN == SSL_CLIENT_SRVR_RAND_SIZE)
	/* this path verified to fail with Netscape Enterprise servers 1/16/02 */
    memcpy(ctx->clientRandom, randomData.data, SSL2_CHALLENGE_LEN);
	#else
    memset(ctx->clientRandom, 0, SSL_CLIENT_SRVR_RAND_SIZE - SSL2_CHALLENGE_LEN);
    memcpy(ctx->clientRandom + SSL_CLIENT_SRVR_RAND_SIZE - SSL2_CHALLENGE_LEN, 
			randomData.data, SSL2_CHALLENGE_LEN);
	#endif
    ctx->ssl2ChallengeLength = SSL2_CHALLENGE_LEN;
    
    assert(charPtr == msg->data + msg->length);
    
    return noErr;
}

OSStatus
SSL2ProcessClientMasterKey(SSLBuffer msg, SSLContext *ctx)
{   OSStatus        err;
    SSL2CipherKind  cipherKind;
    SSLBuffer       secretData;
    unsigned        clearLength, keyArgLength;
    uint32    		localKeyModulusLen;
	size_t		secretLength, encryptedLength;
    UInt8           *charPtr;
    const CSSM_KEY	*cssmKey;
	SecKeyRef		decryptKeyRef = NULL;
	
    if (msg.length < 9) {
		sslErrorLog("SSL2ProcessClientMasterKey: msg.length error 1\n");
        return errSSLProtocol;
    }
    assert(ctx->protocolSide == SSL_ServerSide);
    
    charPtr = msg.data;
    cipherKind = (SSL2CipherKind)SSLDecodeInt(charPtr, 3);
    charPtr += 3;
    clearLength = SSLDecodeInt(charPtr, 2);
    charPtr += 2;
    encryptedLength = SSLDecodeInt(charPtr, 2);
    charPtr += 2;
    keyArgLength = SSLDecodeInt(charPtr, 2);
    charPtr += 2;
    
    if (msg.length != 9 + clearLength + encryptedLength + keyArgLength) {
		sslErrorLog("SSL2ProcessClientMasterKey: msg.length error 2\n");
        return errSSLProtocol;
    }
    
	/* Master key == CLEAR_DATA || SECRET_DATA */
    memcpy(ctx->masterSecret, charPtr, clearLength);
    charPtr += clearLength;

	/* 
	 * Just as in SSL2EncodeServerHello, which key we use depends on the
	 * app's config.
	 */ 
	if(ctx->encryptPrivKeyRef) {
		decryptKeyRef = ctx->encryptPrivKeyRef;
		/* FIXME: when 3420180 is implemented, pick appropriate creds here */
	}
	else if(ctx->signingPrivKeyRef) {
		decryptKeyRef = ctx->signingPrivKeyRef;
		/* FIXME: when 3420180 is implemented, pick appropriate creds here */
	}
	else {
		/* app configuration error */
		sslErrorLog("SSL2ProcessClientMasterKey: No server key!\n");
		return errSSLBadConfiguration;
	}
	err = SecKeyGetCSSMKey(decryptKeyRef, &cssmKey);
	if(err) {
		sslErrorLog("SSL2ProcessClientMasterKey: SecKeyGetCSSMKey err %d\n", (int)err);
		return err;
	}
	localKeyModulusLen = sslKeyLengthInBytes(cssmKey);

    if (encryptedLength != localKeyModulusLen) {
		sslErrorLog("SSL2ProcessClientMasterKey: encryptedLength error 1\n");
        return errSSLProtocol;
	}
	
	/* Allocate enough room to hold any decrypted value */
    if ((err = SSLAllocBuffer(&secretData, encryptedLength, ctx)) != 0)
        return err;
    
	/* 
	 * Detect CSSM_PADDING_APPLE_SSLv2 if SSLv2 or TLSv1 enabled. If
	 * that padding style is seen, that means that the client is capable of 
	 * better than SSLv2, but it has been tricked into negotiating down
	 * to SSLv2. It shouldn't be talking SSLv2 if both it and we are capable 
	 * of better than that.
	 */
	CSSM_PADDING padding;
	if(ctx->versionSsl3Enable || ctx->versionTls1Enable) {
		sslLogNegotiateDebug("===SSL2ProcessClientMasterKey: detecting SSLv2 padding");
		padding = CSSM_PADDING_APPLE_SSLv2;
	}
	else {
		sslLogNegotiateDebug("===SSL2ProcessClientMasterKey: using PKCS1 padding");
		padding = CSSM_PADDING_PKCS1;
	}
	err = sslRsaDecrypt(ctx,
		decryptKeyRef,
		padding,
		charPtr, 
		encryptedLength,
		secretData.data,
		encryptedLength,	// same length for both...? 
		&secretLength);
	if(err) {
		SSLFreeBuffer(&secretData, ctx);
		return err;
	}
    
    charPtr += encryptedLength;
    
    if (clearLength + secretLength != ctx->selectedCipherSpec->cipher->keySize) {
    	sslErrorLog("SSL2ProcessClientMasterKey: length error 3\n");
        return errSSLProtocol;
    }
    memcpy(ctx->masterSecret + clearLength, secretData.data, secretLength);
    if ((err = SSLFreeBuffer(&secretData, ctx)) != 0)
        return err;
    
    if (keyArgLength != ctx->selectedCipherSpec->cipher->ivSize) {
    	sslErrorLog("SSL2ProcessClientMasterKey: length error 4\n");
        return errSSLProtocol;
    }
    
	/* Stash the IV after the master key in master secret storage */
    memcpy(ctx->masterSecret + ctx->selectedCipherSpec->cipher->keySize, charPtr, keyArgLength);
    charPtr += keyArgLength;
    assert(charPtr = msg.data + msg.length);
    
    return noErr;
}

OSStatus
SSL2EncodeClientMasterKey(SSLBuffer *msg, SSLContext *ctx)
{   OSStatus            err;
    unsigned            length, i, clearLen;
    size_t        	outputLen;
	uint32				peerKeyModulusLen;
    SSLBuffer           keyData;
    UInt8               *charPtr;
	
	peerKeyModulusLen = sslKeyLengthInBytes(ctx->peerPubKey);

	/* Length is 10 + clear key size + encrypted output size + iv size */
    length = 10;
    clearLen = ctx->selectedCipherSpec->cipher->keySize - ctx->selectedCipherSpec->cipher->secretKeySize;
    length += clearLen;
    length += peerKeyModulusLen;
    length += ctx->selectedCipherSpec->cipher->ivSize;
    
    if ((err = SSLAllocBuffer(msg, length, ctx)) != 0)
        return err;
    charPtr = msg->data;
    *charPtr++ = SSL2_MsgClientMasterKey;
    for (i = 0; i < SSL2CipherMapCount; i++)
        if (ctx->selectedCipher == SSL2CipherMap[i].cipherSuite)
            break;
    assert(i < SSL2CipherMapCount);
	sslLogNegotiateDebug("===SSL2EncodeClientMasterKey: sending cipherKind 0x%x", 
		SSL2CipherMap[i].cipherKind);
    charPtr = SSLEncodeInt(charPtr, SSL2CipherMap[i].cipherKind, 3);
    charPtr = SSLEncodeInt(charPtr, clearLen, 2);
    charPtr = SSLEncodeInt(charPtr, peerKeyModulusLen, 2);
    charPtr = SSLEncodeInt(charPtr, ctx->selectedCipherSpec->cipher->ivSize, 2);
    
    /* Generate the keying material; we need enough data for the key and IV */
    keyData.data = ctx->masterSecret;
    keyData.length = ctx->selectedCipherSpec->cipher->keySize + ctx->selectedCipherSpec->cipher->ivSize;
    assert(keyData.length <= 48);   /* Must be able to store it in the masterSecret array */
    if ((err = sslRand(ctx, &keyData)) != 0)
        return err;
    
    memcpy(charPtr, ctx->masterSecret, clearLen);
    charPtr += clearLen;
    
	/* 
	 * encrypt only the secret key portion of masterSecret, starting at
	 * clearLen bytes
	 */

	/* 
	 * Use CSSM_PADDING_APPLE_SSLv2 if we're not running SSLv2 ONLY. This
	 * is our way of telling an SSLv3/TLSv1 server that we wanted to negotiate
	 * a better protocol than SSLv2 (which we're in right now) but were forced
	 * down to SSLv2 during Hello negotiation. A server that supports better
	 * than SSLv2 will detect this as a man-in-the-middle rollback attack. 
	 */
	CSSM_PADDING padding;
	if(ctx->versionSsl3Enable || ctx->versionTls1Enable) {
		sslLogNegotiateDebug("===SSL2EncodeClientMasterKey: using SSLv2 padding");
		padding = CSSM_PADDING_APPLE_SSLv2;
	}
	else {
		sslLogNegotiateDebug("===SSL2EncodeClientMasterKey: using PKCS1 padding");
		padding = CSSM_PADDING_PKCS1;
	}
	err = sslRsaEncrypt(ctx,
		ctx->peerPubKey,
		ctx->peerPubKeyCsp,		// XX - maybe cspHand
		padding,
		ctx->masterSecret + clearLen,
		ctx->selectedCipherSpec->cipher->keySize - clearLen,
		charPtr, 
		peerKeyModulusLen,
		&outputLen);
	if(err) {
		return err;
	}

    charPtr += outputLen;
        
    /* copy clear IV to msg buf */
    memcpy(charPtr, ctx->masterSecret + ctx->selectedCipherSpec->cipher->keySize,
            ctx->selectedCipherSpec->cipher->ivSize);
    charPtr += ctx->selectedCipherSpec->cipher->ivSize;
    
    assert(charPtr == msg->data + msg->length);
    
    return noErr;
}

OSStatus
SSL2ProcessClientFinished(SSLBuffer msg, SSLContext *ctx)
{   if (msg.length != ctx->sessionID.length) {
    	sslErrorLog("SSL2ProcessClientFinished: length error\n");
        return errSSLProtocol;
    }
    if (memcmp(msg.data, ctx->serverRandom, ctx->ssl2ConnectionIDLength) != 0) {
    	sslErrorLog("SSL2ProcessClientFinished: data compare error\n");
        return errSSLProtocol;
	}    
    return noErr;
}

OSStatus
SSL2EncodeClientFinished(SSLBuffer *msg, SSLContext *ctx)
{   OSStatus      err;
    
    if ((err = SSLAllocBuffer(msg, ctx->ssl2ConnectionIDLength+1, ctx)) != 0)
        return err;
    msg->data[0] = SSL2_MsgClientFinished;
    memcpy(msg->data+1, ctx->serverRandom, ctx->ssl2ConnectionIDLength);
    return noErr;
}

OSStatus
SSL2ProcessServerHello(SSLBuffer msg, SSLContext *ctx)
{   OSStatus            err;
    SSL2CertTypeCode    certType;
    unsigned            sessionIDMatch, certLen, cipherSpecsLen, connectionIDLen;
    unsigned            i, j;
    SSL2CipherKind      cipherKind;
    SSLCertificate      *cert;
    SSLCipherSuite      matchingCipher = 0;		// avoid compiler warning
    SSLCipherSuite      selectedCipher;
    UInt8               *charPtr;
    SSLProtocolVersion  version;
    
    if (msg.length < 10) {
    	sslErrorLog("SSL2ProcessServerHello: length error\n");
        return errSSLProtocol;
    }
    charPtr = msg.data;
    
    sessionIDMatch = *charPtr++;
    certType = (SSL2CertTypeCode)*charPtr++;
    version = (SSLProtocolVersion)SSLDecodeInt(charPtr, 2);
    charPtr += 2;
    if (version != SSL_Version_2_0) {
    	sslErrorLog("SSL2ProcessServerHello: version error\n");
        return errSSLProtocol;
    }
    ctx->negProtocolVersion = version;
    sslLogNegotiateDebug("===SSL2 client: negVersion is 2_0");
    certLen = SSLDecodeInt(charPtr, 2);
    charPtr += 2;
    cipherSpecsLen = SSLDecodeInt(charPtr, 2);
    charPtr += 2;
    connectionIDLen = SSLDecodeInt(charPtr, 2);
    charPtr += 2;
    
    if (connectionIDLen < 16 || connectionIDLen > 32 || cipherSpecsLen % 3 != 0 ||
        (msg.length != 10 + certLen + cipherSpecsLen + connectionIDLen) )
        return errSSLProtocol;
    if (sessionIDMatch != 0)
    {   if (certLen != 0 || cipherSpecsLen != 0 /* || certType != 0 */ )
            return errSSLProtocol;
        ctx->sessionMatch = 1;
        
        ctx->ssl2ConnectionIDLength = connectionIDLen;
        memcpy(ctx->serverRandom, charPtr, connectionIDLen);
        charPtr += connectionIDLen;
    }
    else
    {   if (certType != SSL2_CertTypeX509)
            return errSSLNegotiation;
        cipherSpecsLen /= 3;
        
 		cert = (SSLCertificate *)sslMalloc(sizeof(SSLCertificate));
		if(cert == NULL) {
			return memFullErr;
		}
        cert->next = 0;
        if ((err = SSLAllocBuffer(&cert->derCert, certLen, ctx)) != 0)
        {   
			sslFree(cert);
            return err;
        }
        memcpy(cert->derCert.data, charPtr, certLen);
        charPtr += certLen;
        ctx->peerCert = cert;
    	if((err = sslVerifyCertChain(ctx, ctx->peerCert, true)) != 0) {
    		return err;
    	}
        if((err = sslPubKeyFromCert(ctx, 
        	&cert->derCert, 
        	&ctx->peerPubKey,
        	&ctx->peerPubKeyCsp)) != 0)
            return err;
        
        selectedCipher = SSL_NO_SUCH_CIPHERSUITE;
        for (i = 0; i < cipherSpecsLen; i++)
        {   cipherKind = (SSL2CipherKind)SSLDecodeInt(charPtr, 3);
            charPtr += 3;
            if (selectedCipher == SSL_NO_SUCH_CIPHERSUITE)  /* After we find one, just keep advancing charPtr past the unused ones */
            {   for (j = 0; j < SSL2CipherMapCount; j++)
                    if (cipherKind == SSL2CipherMap[j].cipherKind)
                    {   matchingCipher = SSL2CipherMap[j].cipherSuite;
                        break;
                    }
                for (j = 0; j < ctx->numValidCipherSpecs; j++)   
                    if (ctx->validCipherSpecs[j].cipherSpec == matchingCipher)
                    {   selectedCipher = matchingCipher;
                        break;
                    }
            }
        }
        if (selectedCipher == SSL_NO_SUCH_CIPHERSUITE)
            return errSSLNegotiation;
		sslLogNegotiateDebug("===SSL2 client: selectedCipher 0x%x", 
			(unsigned)selectedCipher);
        
        ctx->selectedCipher = selectedCipher;
        if ((err = FindCipherSpec(ctx)) != 0) {
            return err;
        }
        ctx->ssl2ConnectionIDLength = connectionIDLen;
        memcpy(ctx->serverRandom, charPtr, connectionIDLen);
        charPtr += connectionIDLen;
    }
    
    assert(charPtr == msg.data + msg.length);
    
    return noErr;
}

OSStatus
SSL2EncodeServerHello(SSLBuffer *msg, SSLContext *ctx)
{   OSStatus            err;
    SSLCertificate      *cert;
    SSLBuffer           randomData;
    UInt8               *charPtr;
    unsigned            i;
    
    /* Create the connection ID */
    ctx->ssl2ConnectionIDLength = SSL2_CONNECTION_ID_LENGTH;
    randomData.data = ctx->serverRandom;
    randomData.length = ctx->ssl2ConnectionIDLength;
    if ((err = sslRand(ctx, &randomData)) != 0)
        return err;
        
    if (ctx->sessionMatch != 0)
    {   if ((err = SSLAllocBuffer(msg, 11 + ctx->sessionID.length, ctx)) != 0)
            return err;
        charPtr = msg->data;
        *charPtr++ = SSL2_MsgServerHello;
        *charPtr++ = ctx->sessionMatch;
        *charPtr++ = 0;    /* cert type */
        charPtr = SSLEncodeInt(charPtr, ctx->negProtocolVersion, 2);
        charPtr = SSLEncodeInt(charPtr, 0, 2);    /* cert len */
        charPtr = SSLEncodeInt(charPtr, 0, 2);    /* cipherspecs len */
        charPtr = SSLEncodeInt(charPtr, ctx->ssl2ConnectionIDLength, 2);
        memcpy(charPtr, ctx->serverRandom, ctx->ssl2ConnectionIDLength);
        charPtr += ctx->ssl2ConnectionIDLength;
    }
    else
    {   /* First, find the last cert in the chain; it's the one we'll send */
    
    	/*
    	 * Use encryptCert if we have it, but allow for the case of app 
		 * specifying one cert which can encrypt and sign.
    	 */
    	if(ctx->encryptCert != NULL) {
			cert = ctx->encryptCert;
		}
		else if(ctx->localCert != NULL) {
			cert = ctx->localCert;
		}
		else {
			/* really should not happen... */
    		sslErrorLog("SSL2EncodeServerHello: No server cert!\n");
    		return badReqErr;
    	}
        
        while (cert->next != 0)
            cert = cert->next;
        
        if ((err = SSLAllocBuffer(msg, 11 + cert->derCert.length + 3 + ctx->sessionID.length, ctx)) != 0)
            return err;
        charPtr = msg->data;
        *charPtr++ = SSL2_MsgServerHello;
        *charPtr++ = ctx->sessionMatch;
        *charPtr++ = SSL2_CertTypeX509; /* cert type */

		#ifndef	NDEBUG
		sslLogNegotiateDebug("===SSL2 server: sending vers info %s", 
			protocolVersStr((SSLProtocolVersion)ctx->negProtocolVersion));
		#endif
		
        charPtr = SSLEncodeInt(charPtr, ctx->negProtocolVersion, 2);
        charPtr = SSLEncodeInt(charPtr, cert->derCert.length, 2);
        charPtr = SSLEncodeInt(charPtr, 3, 2);    /* cipherspecs len */
        charPtr = SSLEncodeInt(charPtr, ctx->ssl2ConnectionIDLength, 2);
        memcpy(charPtr, cert->derCert.data, cert->derCert.length);
        charPtr += cert->derCert.length;
        for (i = 0; i < SSL2CipherMapCount; i++)
            if (ctx->selectedCipher == SSL2CipherMap[i].cipherSuite)
                break;
        assert(i < SSL2CipherMapCount);
        charPtr = SSLEncodeInt(charPtr, SSL2CipherMap[i].cipherKind, 3);
    	sslLogNegotiateDebug("ssl2: server specifying cipherKind 0x%lx", 
     		(UInt32)SSL2CipherMap[i].cipherKind);
        memcpy(charPtr, ctx->serverRandom, ctx->ssl2ConnectionIDLength);
        charPtr += ctx->ssl2ConnectionIDLength;
    }
    
    assert(charPtr == msg->data + msg->length);
    return noErr;
}

OSStatus
SSL2ProcessServerVerify(SSLBuffer msg, SSLContext *ctx)
{   if (msg.length != ctx->ssl2ChallengeLength)
        return errSSLProtocol;
    
    if (memcmp(msg.data, ctx->clientRandom + SSL_CLIENT_SRVR_RAND_SIZE - 
			ctx->ssl2ChallengeLength, ctx->ssl2ChallengeLength) != 0)
        return errSSLProtocol;
    
    return noErr;
}

OSStatus
SSL2EncodeServerVerify(SSLBuffer *msg, SSLContext *ctx)
{   OSStatus      err;
    
    if ((err = SSLAllocBuffer(msg, 1 + ctx->ssl2ChallengeLength, ctx)) != 0)
        return err;
    
    msg->data[0] = SSL2_MsgServerVerify;
    memcpy(msg->data+1, ctx->clientRandom + SSL_CLIENT_SRVR_RAND_SIZE - 
			ctx->ssl2ChallengeLength, ctx->ssl2ChallengeLength);
    
    return noErr;
}

OSStatus
SSL2ProcessServerFinished(SSLBuffer msg, SSLContext *ctx)
{   OSStatus      err;
    
    if ((err = SSLAllocBuffer(&ctx->sessionID, msg.length, ctx)) != 0)
        return err;
    memcpy(ctx->sessionID.data, msg.data, msg.length);
    return noErr;
}

OSStatus
SSL2EncodeServerFinished(SSLBuffer *msg, SSLContext *ctx)
{   OSStatus      err;
    
    if ((err = SSLAllocBuffer(msg, 1 + ctx->sessionID.length, ctx)) != 0)
        return err;
    
    msg->data[0] = SSL2_MsgServerFinished;
    memcpy(msg->data+1, ctx->sessionID.data, ctx->sessionID.length);
    
    return noErr;
}

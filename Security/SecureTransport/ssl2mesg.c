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


/*  *********************************************************************
    File: ssl2mesg.c

    SSLRef 3.0 Final -- 11/19/96

    Copyright (c)1996 by Netscape Communications Corp.

    By retrieving this software you are bound by the licensing terms
    disclosed in the file "LICENSE.txt". Please read it, and if you don't
    accept the terms, delete this software.

    SSLRef 3.0 was developed by Netscape Communications Corp. of Mountain
    View, California <http://home.netscape.com/> and Consensus Development
    Corporation of Berkeley, California <http://www.consensus.com/>.

    *********************************************************************

    File: ssl2mesg.c   Message encoding and decoding functions for SSL 2

    The necessary message encoding and decoding for all SSL 2 handshake
    messages.

    ****************************************************************** */

#ifndef _SSL_H_
#include "ssl.h"
#endif

#ifndef _SSL2_H_
#include "ssl2.h"
#endif

#ifndef _SSLREC_H_
#include "sslrec.h"
#endif

#ifndef _SSLALLOC_H_
#include "sslalloc.h"
#endif

#ifndef _SSLCTX_H_
#include "sslctx.h"
#endif

#ifndef _SSLALERT_H_
#include "sslalert.h"
#endif

#ifndef _SSLHDSHK_H_
#include "sslhdshk.h"
#endif

#ifndef _SSLSESS_H_
#include "sslsess.h"
#endif

#ifndef	_SSL_DEBUG_H_
#include "sslDebug.h"
#endif

#ifndef	_CIPHER_SPECS_H_
#include "cipherSpecs.h"
#endif

#ifndef	_APPLE_CDSA_H_
#include "appleCdsa.h"
#endif

#ifndef _SSLUTIL_H_
#include "sslutil.h"
#endif

#include <string.h>
#include <assert.h>

SSLErr
SSL2ProcessClientHello(SSLBuffer msg, SSLContext *ctx)
{   SSLErr              err;
    UInt8               *progress, *cipherList;
    int                 i, j, cipherKindCount, sessionIDLen, challengeLen;
    SSL2CipherKind      cipherKind;
    SSLCipherSuite      matchingCipher, selectedCipher;
    SSLProtocolVersion  version;
    
    if (msg.length < 27) {
		errorLog0("SSL2ProcessClientHello: msg len error 1\n");
        return ERR(SSLProtocolErr);
    }
    
    progress = msg.data;
    
    version = (SSLProtocolVersion)SSLDecodeInt(progress, 2);
	if (version > ctx->maxProtocolVersion) {
		version = ctx->maxProtocolVersion;
	}
    /* FIXME - I think this needs work for a SSL_Version_2_0 server, to ensure that
	 * the client isn't establishing a v3 session. */
    if (ctx->negProtocolVersion == SSL_Version_Undetermined)
    {   
        #if LOG_NEGOTIATE
        dprintf1("===SSL2 server: negVersion was undetermined; is %s\n",
        	protocolVersStr(version));
        #endif
        ctx->negProtocolVersion = version;
		if(version >= TLS_Version_1_0) {
			ctx->sslTslCalls = &Tls1Callouts;
		}
		else {
			/* default from context init */
			assert(ctx->sslTslCalls == &Ssl3Callouts);
		}
    }
    else if (ctx->negProtocolVersion == SSL_Version_3_0_With_2_0_Hello)
    {   if (version < SSL_Version_3_0) {
			errorLog0("SSL2ProcessClientHello: version error\n");
            return ERR(SSLProtocolErr);
        }
		/* FIXME - I don't think path is ever taken - we NEVER set any
		 * protocol var to 	SSL_Version_3_0_With_2_0_Hello... */
        #if LOG_NEGOTIATE
        dprintf0("===SSL2 server: negVersion was 3_0_With_2_0_Hello; is 3_0\n");
        #endif
        ctx->negProtocolVersion = version;
    }
    
    progress += 2;
    cipherKindCount = SSLDecodeInt(progress, 2);
    progress += 2;
    if (cipherKindCount % 3 != 0) {
		errorLog0("SSL2ProcessClientHello: cipherKindCount error\n");
        return ERR(SSLProtocolErr);
    }
    cipherKindCount /= 3;
    sessionIDLen = SSLDecodeInt(progress, 2);
    progress += 2;
    challengeLen = SSLDecodeInt(progress, 2);
    progress += 2;
    
    if (msg.length != 8 + 3*cipherKindCount + sessionIDLen + challengeLen ||
        (sessionIDLen != 0 && sessionIDLen != 16) ||
        challengeLen < 16 || challengeLen > 32 ) {
		errorLog0("SSL2ProcessClientHello: msg len error 2\n");
        return ERR(SSLProtocolErr);
    }
    cipherList = progress;
    selectedCipher = SSL_NO_SUCH_CIPHERSUITE;

    if (ctx->negProtocolVersion >= SSL_Version_3_0) {
		/* If we're negotiating an SSL 3.0 session, use SSL 3.0 suites first */
        for (i = 0; i < cipherKindCount; i++) {
            cipherKind = (SSL2CipherKind)SSLDecodeInt(progress, 3);
            progress += 3;
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
	    progress = cipherList;
		for (i = 0; i < cipherKindCount; i++) {
			cipherKind = (SSL2CipherKind)SSLDecodeInt(progress, 3);
			progress += 3;
			if (selectedCipher == SSL_NO_SUCH_CIPHERSUITE) {
				/* After we find one, just keep advancing progress past 
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
        return ERR(SSLNegotiationErr);
    
    ctx->selectedCipher = selectedCipher;
    err = FindCipherSpec(ctx);
    if(err != 0) {
        return err;
    }
    if (sessionIDLen > 0 && ctx->peerID.data != 0)
    {   /* Don't die on error; just treat it as an uncacheable session */
        ERR(err = SSLAllocBuffer(&ctx->sessionID, sessionIDLen, &ctx->sysCtx));
        if (err == 0)
            memcpy(ctx->sessionID.data, progress, sessionIDLen);
    }
    progress += sessionIDLen;
    
    ctx->ssl2ChallengeLength = challengeLen;
    memset(ctx->clientRandom, 0, SSL_CLIENT_SRVR_RAND_SIZE);
    memcpy(ctx->clientRandom + SSL_CLIENT_SRVR_RAND_SIZE - challengeLen, 
		progress, challengeLen);
    progress += challengeLen;
    CASSERT(progress == msg.data + msg.length);
    
    return SSLNoErr;
}

/*
 * The SSL v2 spec says that the challenge string sent by the client can be
 * between 16 and 32 bytes. However all Netscape enterprise servers actually
 * require a 16 byte challenge. Q.v. cdnow.com, store.apple.com. 
 * Unfortunately this means that when we're trying to do a 
 * SSL_Version_3_0_With_2_0_Hello negotiation, we have to limit ourself to 
 * a 16-byte clientRandom, which we have to concatenate to 16 bytes of 
 * zeroes if we end up with a 3.0 or 3.1 connection. Thus we lose 16 bytes
 * of entropy.
 */
#define SSL2_CHALLENGE_LEN	16

SSLErr
SSL2EncodeClientHello(SSLBuffer *msg, SSLContext *ctx)
{   SSLErr          err;
    UInt8           *progress;
    int             i, j, useSSL3Ciphers, totalCipherCount;
    int             sessionIDLen;
    UInt16          version;
    SSLBuffer       sessionIdentifier, randomData;
    
    switch (ctx->negProtocolVersion)
    {   case SSL_Version_Undetermined:
        case SSL_Version_3_0_With_2_0_Hello:
        	/* go for it, see if server can handle upgrading */
           	useSSL3Ciphers = 1;
			/* could be SSLv3 or TLSv1 */
            version = ctx->maxProtocolVersion;
            break;
        case SSL_Version_2_0:
            useSSL3Ciphers = 0;
            version = SSL_Version_2_0;
            break;
        case SSL_Version_3_0_Only:
        case SSL_Version_3_0:
        case TLS_Version_1_0_Only:
        case TLS_Version_1_0:
        default:
            ASSERTMSG("Bad protocol version for sending SSL 2 Client Hello");
            break;
    }
 	#if LOG_NEGOTIATE
	dprintf1("===SSL client: proclaiming %s capable\n", 
		protocolVersStr((SSLProtocolVersion)version));
	#endif
    
    if (useSSL3Ciphers != 0)
        totalCipherCount = ctx->numValidCipherSpecs;
    else
        totalCipherCount = 0;
        
    for (i = 0; i < SSL2CipherMapCount; i++)
        for (j = 0; j < ctx->numValidCipherSpecs; j++)
            if (ctx->validCipherSpecs[j].cipherSpec == SSL2CipherMap[i].cipherSuite)
            {   totalCipherCount++;
                break;
            }
    
    sessionIDLen = 0;
    sessionIdentifier.data = 0;
    if (ctx->resumableSession.data != 0)
    {   if (ERR(err = SSLRetrieveSessionID(ctx->resumableSession, &sessionIdentifier, ctx)) != 0)
            return err;
        sessionIDLen = sessionIdentifier.length;
    }
    
	/* msg length = 9 + 3 * totalCipherCount + sessionIDLen + 16 bytes of challenge
	 *  Use exactly 16 bytes of challenge because Netscape products have a bug
	 *  that requires this length
	 */ 
    if (ERR(err = SSLAllocBuffer(msg, 9 + (3*totalCipherCount) + sessionIDLen + 
			SSL2_CHALLENGE_LEN, &ctx->sysCtx)) != 0)
    {   ERR(SSLFreeBuffer(&sessionIdentifier, &ctx->sysCtx));
        return err;
    }
    
    progress = msg->data;
    *progress++ = ssl2_mt_client_hello;
    progress = SSLEncodeInt(progress, version, 2);
    progress = SSLEncodeInt(progress, 3*totalCipherCount, 2);
    progress = SSLEncodeInt(progress, sessionIDLen, 2);
    progress = SSLEncodeInt(progress, SSL2_CHALLENGE_LEN, 2);
    
	/* If we can send SSL3 ciphers, encode the two-byte cipher specs into three-byte
	 *  CipherKinds which have a leading 0.
	 */
    if (useSSL3Ciphers != 0)
        for (i = 0; i < ctx->numValidCipherSpecs; i++)
            progress = SSLEncodeInt(progress, ctx->validCipherSpecs[i].cipherSpec, 3);
    
	/* Now send those SSL2 specs for which we have implementations */
    for (i = 0; i < SSL2CipherMapCount; i++)
        for (j = 0; j < ctx->numValidCipherSpecs; j++)
            if (ctx->validCipherSpecs[j].cipherSpec == SSL2CipherMap[i].cipherSuite)
            {   progress = SSLEncodeInt(progress, SSL2CipherMap[i].cipherKind, 3);
                break;
            }
    
    if (sessionIDLen > 0)
    {   memcpy(progress, sessionIdentifier.data, sessionIDLen);
        progress += sessionIDLen;
        ERR(SSLFreeBuffer(&sessionIdentifier, &ctx->sysCtx));
    }
    
    randomData.data = progress;
    randomData.length = SSL2_CHALLENGE_LEN;
    if ((err = sslRand(ctx, &randomData)) != 0)
    {   ERR(SSLFreeBuffer(msg, &ctx->sysCtx));
        return err;
    }
    progress += SSL2_CHALLENGE_LEN;
    
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
    
    CASSERT(progress == msg->data + msg->length);
    
    return SSLNoErr;
}

SSLErr
SSL2ProcessClientMasterKey(SSLBuffer msg, SSLContext *ctx)
{   SSLErr          err;
    SSL2CipherKind  cipherKind;
    SSLBuffer       secretData;
    int             clearLength, encryptedLength, keyArgLength;
    UInt32    		secretLength, localKeyModulusLen;
    UInt8           *progress;
    const CSSM_KEY	*decryptKey;
	CSSM_CSP_HANDLE	decryptCspHand;
	
    if (msg.length < 9) {
		errorLog0("SSL2ProcessClientMasterKey: msg.length error 1\n");
        return ERR(SSLProtocolErr);
    }
    CASSERT(ctx->protocolSide == SSL_ServerSide);
    
    progress = msg.data;
    cipherKind = (SSL2CipherKind)SSLDecodeInt(progress, 3);
    progress += 3;
    clearLength = SSLDecodeInt(progress, 2);
    progress += 2;
    encryptedLength = SSLDecodeInt(progress, 2);
    progress += 2;
    keyArgLength = SSLDecodeInt(progress, 2);
    progress += 2;
    
    if (msg.length != 9 + clearLength + encryptedLength + keyArgLength) {
		errorLog0("SSL2ProcessClientMasterKey: msg.length error 2\n");
        return ERR(SSLProtocolErr);
    }
    
	/* Master key == CLEAR_DATA || SECRET_DATA */
    memcpy(ctx->masterSecret, progress, clearLength);
    progress += clearLength;

	/* 
	 * Just as in SSL2EncodeServerHello, which key we use depends on the
	 * app's config.
	 */ 
	if(ctx->encryptPrivKey) {
		decryptKey = ctx->encryptPrivKey;
		CASSERT(ctx->encryptKeyCsp != 0);
		decryptCspHand = ctx->encryptKeyCsp;
	}
	else if(ctx->signingPrivKey) {
		decryptKey = ctx->signingPrivKey;
		CASSERT(ctx->signingKeyCsp != 0);
		decryptCspHand = ctx->signingKeyCsp;
	}
	else {
		/* really should not happen... */
		errorLog0("SSL2ProcessClientMasterKey: No server key!\n");
		return SSLBadStateErr;
	}
	localKeyModulusLen = sslKeyLengthInBytes(decryptKey);

    if (encryptedLength != localKeyModulusLen) {
		errorLog0("SSL2ProcessClientMasterKey: encryptedLength error 1\n");
        return ERR(SSLProtocolErr);
	}
	
	/* Allocate enough room to hold any decrypted value */
    if (ERR(err = SSLAllocBuffer(&secretData, encryptedLength, &ctx->sysCtx)) != 0)
        return err;
    
	err = sslRsaDecrypt(ctx,
		decryptKey,
		decryptCspHand,
		progress, 
		encryptedLength,
		secretData.data,
		encryptedLength,	// same length for both...? 
		&secretLength);
	if(err) {
		SSLFreeBuffer(&secretData, &ctx->sysCtx);
		return err;
	}
    
    progress += encryptedLength;
    
    if (clearLength + secretLength != ctx->selectedCipherSpec->cipher->keySize) {
    	errorLog0("SSL2ProcessClientMasterKey: length error 3\n");
        return ERR(SSLProtocolErr);
    }
    memcpy(ctx->masterSecret + clearLength, secretData.data, secretLength);
    if (ERR(err = SSLFreeBuffer(&secretData, &ctx->sysCtx)) != 0)
        return err;
    
    if (keyArgLength != ctx->selectedCipherSpec->cipher->ivSize) {
    	errorLog0("SSL2ProcessClientMasterKey: length error 4\n");
        return ERR(SSLProtocolErr);
    }
    
	/* Stash the IV after the master key in master secret storage */
    memcpy(ctx->masterSecret + ctx->selectedCipherSpec->cipher->keySize, progress, keyArgLength);
    progress += keyArgLength;
    CASSERT(progress = msg.data + msg.length);
    
    return SSLNoErr;
}

SSLErr
SSL2EncodeClientMasterKey(SSLBuffer *msg, SSLContext *ctx)
{   SSLErr              err;
    int                 length, i, clearLen;
    UInt32        		outputLen, peerKeyModulusLen;
    SSLBuffer           keyData;
    UInt8               *progress;
	
	peerKeyModulusLen = sslKeyLengthInBytes(ctx->peerPubKey);

	/* Length is 10 + clear key size + encrypted output size + iv size */
    length = 10;
    clearLen = ctx->selectedCipherSpec->cipher->keySize - ctx->selectedCipherSpec->cipher->secretKeySize;
    length += clearLen;
    length += peerKeyModulusLen;
    length += ctx->selectedCipherSpec->cipher->ivSize;
    
    if (ERR(err = SSLAllocBuffer(msg, length, &ctx->sysCtx)) != 0)
        return err;
    progress = msg->data;
    *progress++ = ssl2_mt_client_master_key;
    for (i = 0; i < SSL2CipherMapCount; i++)
        if (ctx->selectedCipher == SSL2CipherMap[i].cipherSuite)
            break;
    CASSERT(i < SSL2CipherMapCount);
	#if LOG_NEGOTIATE
	dprintf1("===SSL2EncodeClientMasterKey: sending cipherKind 0x%x\n", 
		SSL2CipherMap[i].cipherKind);
	#endif
    progress = SSLEncodeInt(progress, SSL2CipherMap[i].cipherKind, 3);
    progress = SSLEncodeInt(progress, clearLen, 2);
    progress = SSLEncodeInt(progress, peerKeyModulusLen, 2);
    progress = SSLEncodeInt(progress, ctx->selectedCipherSpec->cipher->ivSize, 2);
    
    /* Generate the keying material; we need enough data for the key and IV */
    keyData.data = ctx->masterSecret;
    keyData.length = ctx->selectedCipherSpec->cipher->keySize + ctx->selectedCipherSpec->cipher->ivSize;
    CASSERT(keyData.length <= 48);   /* Must be able to store it in the masterSecret array */
    if ((err = sslRand(ctx, &keyData)) != 0)
        return err;
    
    memcpy(progress, ctx->masterSecret, clearLen);
    progress += clearLen;
    
	/* Replace this with code to do encryption at lower level & set PKCS1 padding
    for rollback attack */

	/* 
	 * encrypt only the secret key portion of masterSecret, starting at
	 * clearLen bytes
	 */
	err = sslRsaEncrypt(ctx,
		ctx->peerPubKey,
		ctx->peerPubKeyCsp,		// XX - maybe cspHand
		ctx->masterSecret + clearLen,
		ctx->selectedCipherSpec->cipher->keySize - clearLen,
		progress, 
		peerKeyModulusLen,
		&outputLen);
	if(err) {
		return err;
	}

    progress += outputLen;
        
    /* copy clear IV to msg buf */
    memcpy(progress, ctx->masterSecret + ctx->selectedCipherSpec->cipher->keySize,
            ctx->selectedCipherSpec->cipher->ivSize);
    progress += ctx->selectedCipherSpec->cipher->ivSize;
    
    CASSERT(progress == msg->data + msg->length);
    
    return SSLNoErr;
}

SSLErr
SSL2ProcessClientFinished(SSLBuffer msg, SSLContext *ctx)
{   if (msg.length != ctx->sessionID.length) {
    	errorLog0("SSL2ProcessClientFinished: length error\n");
        return ERR(SSLProtocolErr);
    }
    if (memcmp(msg.data, ctx->serverRandom, ctx->ssl2ConnectionIDLength) != 0) {
    	errorLog0("SSL2ProcessClientFinished: data compare error\n");
        return ERR(SSLProtocolErr);
	}    
    return SSLNoErr;
}

SSLErr
SSL2EncodeClientFinished(SSLBuffer *msg, SSLContext *ctx)
{   SSLErr      err;
    
    if (ERR(err = SSLAllocBuffer(msg, ctx->ssl2ConnectionIDLength+1, &ctx->sysCtx)) != 0)
        return err;
    msg->data[0] = ssl2_mt_client_finished;
    memcpy(msg->data+1, ctx->serverRandom, ctx->ssl2ConnectionIDLength);
    return SSLNoErr;
}

SSLErr
SSL2ProcessServerHello(SSLBuffer msg, SSLContext *ctx)
{   SSLErr              err;
    SSL2CertTypeCode    certType;
    int                 sessionIDMatch, certLen, cipherSpecsLen, connectionIDLen;
    int                 i, j;
    SSL2CipherKind      cipherKind;
    SSLCertificate      *cert;
    SSLCipherSuite      matchingCipher = 0;		// avoid compiler warning
    SSLCipherSuite      selectedCipher;
    UInt8               *progress;
    SSLProtocolVersion  version;
    
    if (msg.length < 10) {
    	errorLog0("SSL2ProcessServerHello: length error\n");
        return ERR(SSLProtocolErr);
    }
    progress = msg.data;
    
    sessionIDMatch = *progress++;
    certType = (SSL2CertTypeCode)*progress++;
    version = (SSLProtocolVersion)SSLDecodeInt(progress, 2);
    progress += 2;
    if (version != SSL_Version_2_0) {
    	errorLog0("SSL2ProcessServerHello: version error\n");
        return ERR(SSLProtocolErr);
    }
    ctx->negProtocolVersion = version;
    #if LOG_NEGOTIATE
    dprintf0("===SSL2 client: negVersion is 2_0\n");
    #endif
    certLen = SSLDecodeInt(progress, 2);
    progress += 2;
    cipherSpecsLen = SSLDecodeInt(progress, 2);
    progress += 2;
    connectionIDLen = SSLDecodeInt(progress, 2);
    progress += 2;
    
    if (connectionIDLen < 16 || connectionIDLen > 32 || cipherSpecsLen % 3 != 0 ||
        (msg.length != 10 + certLen + cipherSpecsLen + connectionIDLen) )
        return ERR(SSLProtocolErr);
    if (sessionIDMatch != 0)
    {   if (certLen != 0 || cipherSpecsLen != 0 /* || certType != 0 */ )
            return ERR(SSLProtocolErr);
        ctx->ssl2SessionMatch = 1;
        
        ctx->ssl2ConnectionIDLength = connectionIDLen;
        memcpy(ctx->serverRandom, progress, connectionIDLen);
        progress += connectionIDLen;
    }
    else
    {   if (certType != ssl2_ct_x509_certificate)
            return ERR(SSLNegotiationErr);
        cipherSpecsLen /= 3;
        
 		cert = (SSLCertificate *)sslMalloc(sizeof(SSLCertificate));
		if(cert == NULL) {
			return SSLMemoryErr;
		}
        cert->next = 0;
        if (ERR(err = SSLAllocBuffer(&cert->derCert, certLen, &ctx->sysCtx)) != 0)
        {   
			sslFree(cert);
            return err;
        }
        memcpy(cert->derCert.data, progress, certLen);
        progress += certLen;
        ctx->peerCert = cert;
        /* This cert never gets verified in original SSLRef3 code... */
    	if((err = sslVerifyCertChain(ctx, ctx->peerCert)) != 0) {
    		return err;
    	}
        if((err = sslPubKeyFromCert(ctx, 
        	&cert->derCert, 
        	&ctx->peerPubKey,
        	&ctx->peerPubKeyCsp)) != 0)
            return err;
        
        selectedCipher = SSL_NO_SUCH_CIPHERSUITE;
        for (i = 0; i < cipherSpecsLen; i++)
        {   cipherKind = (SSL2CipherKind)SSLDecodeInt(progress, 3);
            progress += 3;
            //dprintf1("ssl2: server supports cipherKind 0x%x\n", (UInt32)cipherKind);
            if (selectedCipher == SSL_NO_SUCH_CIPHERSUITE)  /* After we find one, just keep advancing progress past the unused ones */
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
            return ERR(SSLNegotiationErr);
		#if LOG_NEGOTIATE
		dprintf1("===SSL2 client: selectedCipher 0x%x\n", 
			(unsigned)selectedCipher);
		#endif
        
        ctx->selectedCipher = selectedCipher;
        if (ERR(err = FindCipherSpec(ctx)) != 0) {
            return err;
        }
        ctx->ssl2ConnectionIDLength = connectionIDLen;
        memcpy(ctx->serverRandom, progress, connectionIDLen);
        progress += connectionIDLen;
    }
    
    CASSERT(progress == msg.data + msg.length);
    
    return SSLNoErr;
}

SSLErr
SSL2EncodeServerHello(SSLBuffer *msg, SSLContext *ctx)
{   SSLErr              err;
    SSLCertificate      *cert;
    SSLBuffer           randomData;
    UInt8               *progress;
    int                 i;
    
    /* Create the connection ID */
    ctx->ssl2ConnectionIDLength = SSL2_CONNECTION_ID_LENGTH;
    randomData.data = ctx->serverRandom;
    randomData.length = ctx->ssl2ConnectionIDLength;
    if ((err = sslRand(ctx, &randomData)) != 0)
        return err;
        
    if (ctx->ssl2SessionMatch != 0)
    {   if (ERR(err = SSLAllocBuffer(msg, 11 + ctx->sessionID.length, &ctx->sysCtx)) != 0)
            return err;
        progress = msg->data;
        *progress++ = ssl2_mt_server_hello;
        *progress++ = ctx->ssl2SessionMatch;
        *progress++ = 0;    /* cert type */
        progress = SSLEncodeInt(progress, ctx->negProtocolVersion, 2);
        progress = SSLEncodeInt(progress, 0, 2);    /* cert len */
        progress = SSLEncodeInt(progress, 0, 2);    /* cipherspecs len */
        progress = SSLEncodeInt(progress, ctx->ssl2ConnectionIDLength, 2);
        memcpy(progress, ctx->serverRandom, ctx->ssl2ConnectionIDLength);
        progress += ctx->ssl2ConnectionIDLength;
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
    		errorLog0("SSL2EncodeServerHello: No server cert!\n");
    		return SSLBadStateErr;
    	}
        
        while (cert->next != 0)
            cert = cert->next;
        
        if (ERR(err = SSLAllocBuffer(msg, 11 + cert->derCert.length + 3 + ctx->sessionID.length, &ctx->sysCtx)) != 0)
            return err;
        progress = msg->data;
        *progress++ = ssl2_mt_server_hello;
        *progress++ = ctx->ssl2SessionMatch;
        *progress++ = ssl2_ct_x509_certificate; /* cert type */
	 	#if LOG_NEGOTIATE
		dprintf1("===SSL2 server: sending vers info %s\n", 
			protocolVersStr((SSLProtocolVersion)ctx->negProtocolVersion));
		#endif
        progress = SSLEncodeInt(progress, ctx->negProtocolVersion, 2);
        progress = SSLEncodeInt(progress, cert->derCert.length, 2);
        progress = SSLEncodeInt(progress, 3, 2);    /* cipherspecs len */
        progress = SSLEncodeInt(progress, ctx->ssl2ConnectionIDLength, 2);
        memcpy(progress, cert->derCert.data, cert->derCert.length);
        progress += cert->derCert.length;
        for (i = 0; i < SSL2CipherMapCount; i++)
            if (ctx->selectedCipher == SSL2CipherMap[i].cipherSuite)
                break;
        CASSERT(i < SSL2CipherMapCount);
        progress = SSLEncodeInt(progress, SSL2CipherMap[i].cipherKind, 3);
     	dprintf1("ssl2: server specifying cipherKind 0x%lx\n", 
     		(UInt32)SSL2CipherMap[i].cipherKind);
        memcpy(progress, ctx->serverRandom, ctx->ssl2ConnectionIDLength);
        progress += ctx->ssl2ConnectionIDLength;
    }
    
    CASSERT(progress == msg->data + msg->length);
    return SSLNoErr;
}

SSLErr
SSL2ProcessServerVerify(SSLBuffer msg, SSLContext *ctx)
{   if (msg.length != ctx->ssl2ChallengeLength)
        return ERR(SSLProtocolErr);
    
    if (memcmp(msg.data, ctx->clientRandom + SSL_CLIENT_SRVR_RAND_SIZE - 
			ctx->ssl2ChallengeLength, ctx->ssl2ChallengeLength) != 0)
        return ERR(SSLProtocolErr);
    
    return SSLNoErr;
}

SSLErr
SSL2EncodeServerVerify(SSLBuffer *msg, SSLContext *ctx)
{   SSLErr      err;
    
    if (ERR(err = SSLAllocBuffer(msg, 1 + ctx->ssl2ChallengeLength, &ctx->sysCtx)) != 0)
        return err;
    
    msg->data[0] = ssl2_mt_server_verify;
    memcpy(msg->data+1, ctx->clientRandom + SSL_CLIENT_SRVR_RAND_SIZE - 
			ctx->ssl2ChallengeLength, ctx->ssl2ChallengeLength);
    
    return SSLNoErr;
}

SSLErr
SSL2ProcessServerFinished(SSLBuffer msg, SSLContext *ctx)
{   SSLErr      err;
    
    if (ERR(err = SSLAllocBuffer(&ctx->sessionID, msg.length, &ctx->sysCtx)) != 0)
        return err;
    memcpy(ctx->sessionID.data, msg.data, msg.length);
    return SSLNoErr;
}

SSLErr
SSL2EncodeServerFinished(SSLBuffer *msg, SSLContext *ctx)
{   SSLErr      err;
    
    if (ERR(err = SSLAllocBuffer(msg, 1 + ctx->sessionID.length, &ctx->sysCtx)) != 0)
        return err;
    
    msg->data[0] = ssl2_mt_server_finished;
    memcpy(msg->data+1, ctx->sessionID.data, ctx->sessionID.length);
    
    return SSLNoErr;
}

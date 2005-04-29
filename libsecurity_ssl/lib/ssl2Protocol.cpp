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
	File:		ssl2Protocol.cpp

	Contains:	Protocol engine for SSL 2

	Written by:	Doug Mitchell

	Copyright: (c) 1999 by Apple Computer, Inc., all rights reserved.

*/

#include "ssl.h"
#include "ssl2.h"
#include "sslRecord.h"
#include "sslMemory.h"
#include "sslContext.h"
#include "sslHandshake.h"
#include "sslSession.h"
#include "sslAlertMessage.h"
#include "sslDebug.h"
#include "appleCdsa.h"
#include "sslUtils.h"
#include "sslDigests.h"
#include <string.h>
#include <assert.h>

#ifndef	NDEBUG

static char *sslHdskMsgToStr(SSL2MessageType msg)
{
	static char badStr[100];
	
	switch(msg) {
		case SSL2_MsgError:
			return "SSL2_MsgError";	
		case SSL2_MsgClientHello:
			return "SSL2_MsgClientHello";	
		case SSL2_MsgClientMasterKey:
			return "SSL2_MsgClientMasterKey";	
		case SSL2_MsgClientFinished:
			return "SSL2_MsgClientFinished";	
		case SSL2_MsgServerHello:
			return "SSL2_MsgServerHello";	
		case SSL2_MsgServerVerify:
			return "SSL2_MsgServerVerify";	
		case SSL2_MsgServerFinished:
			return "SSL2_MsgServerFinished";	
		case SSL2_MsgRequestCert:
			return "SSL2_MsgRequestCert";	
		case SSL2_MsgClientCert:
			return "SSL2_MsgClientCert";	
		case SSL2_MsgKickstart:
			return "SSL2_MsgKickstart";	
		default:
			sprintf(badStr, "Unknown msg (%d(d)", msg);
			return badStr;
	}
}

static void logSsl2Msg(SSL2MessageType msg, char sent)
{
	char *ms = sslHdskMsgToStr(msg);
	sslHdskMsgDebug("...msg %s: %s", (sent ? "sent" : "recd"), ms);
}

#else

#define logSsl2Msg(m, s)

#endif		/* NDEBUG */

OSStatus
SSL2ProcessMessage(SSLRecord &rec, SSLContext *ctx)
{   OSStatus            err = 0;
    SSL2MessageType     msg;
    SSLBuffer           contents;
    
    if (rec.contents.length < 2)
        return errSSLProtocol;
    
    msg = (SSL2MessageType)rec.contents.data[0];
    contents.data = rec.contents.data + 1;
    contents.length = rec.contents.length - 1;
    
    logSsl2Msg(msg, 0);
    
    switch (msg)
    {   case SSL2_MsgError:
    		err = errSSLClosedAbort;
            break;
        case SSL2_MsgClientHello:
            if (ctx->state != SSL_HdskStateServerUninit)
                return errSSLProtocol;
            err = SSL2ProcessClientHello(contents, ctx);
            if (err == errSSLNegotiation)
                SSL2SendError(SSL2_ErrNoCipher, ctx);
            break;
        case SSL2_MsgClientMasterKey:
            if (ctx->state != SSL2_HdskStateClientMasterKey)
                return errSSLProtocol;
            err = SSL2ProcessClientMasterKey(contents, ctx);
            break;
        case SSL2_MsgClientFinished:
            if (ctx->state != SSL2_HdskStateClientFinished)
                return errSSLProtocol;
            err = SSL2ProcessClientFinished(contents, ctx);
            break;
        case SSL2_MsgServerHello:
            if (ctx->state != SSL2_HdskStateServerHello &&
                ctx->state != SSL_HdskStateServerHelloUnknownVersion)
                return errSSLProtocol;
            err = SSL2ProcessServerHello(contents, ctx);
            if (err == errSSLNegotiation)
                SSL2SendError(SSL2_ErrNoCipher, ctx);
            break;
        case SSL2_MsgServerVerify:
            if (ctx->state != SSL2_HdskStateServerVerify)
                return errSSLProtocol;
            err = SSL2ProcessServerVerify(contents, ctx);
            break;
        case SSL2_MsgServerFinished:
            if (ctx->state != SSL2_HdskStateServerFinished) {
				/* FIXME - this ifndef should not be necessary */
				#ifndef	NDEBUG
				sslHdskStateDebug("SSL2_MsgServerFinished; state %s",
            		hdskStateToStr(ctx->state));
				#endif
                return errSSLProtocol;
            }
            err = SSL2ProcessServerFinished(contents, ctx);
            break;
        case SSL2_MsgRequestCert:
            /* Don't process the request; we don't support client certification */
            break;
        case SSL2_MsgClientCert:
            return errSSLProtocol;
            break;
        default:
            return errSSLProtocol;
            break;
    }
    
    if (err == 0)
    {   	
    	if ((msg == SSL2_MsgClientHello) && 
		    (ctx->negProtocolVersion >= SSL_Version_3_0))
        {   /* Promote this message to SSL 3 protocol */
            if ((err = SSL3ReceiveSSL2ClientHello(rec, ctx)) != 0)
                return err;
        }
        else
            err = SSL2AdvanceHandshake(msg, ctx);
    }
    return err;
}

OSStatus
SSL2AdvanceHandshake(SSL2MessageType msg, SSLContext *ctx)
{   OSStatus          err;
    
    err = noErr;
    
    switch (msg)
    {   case SSL2_MsgKickstart:
			assert(ctx->negProtocolVersion == SSL_Version_Undetermined);
			assert(ctx->versionSsl2Enable);
            if (ctx->versionSsl3Enable || ctx->versionTls1Enable) {
				/* prepare for possible v3 upgrade */
                if ((err = SSLInitMessageHashes(ctx)) != 0)
                    return err;
			}
            if ((err = SSL2PrepareAndQueueMessage(SSL2EncodeClientHello, ctx)) != 0)
                return err;
            if (ctx->versionSsl3Enable || ctx->versionTls1Enable) {
				SSLChangeHdskState(ctx, SSL_HdskStateServerHelloUnknownVersion);
			}
			else {
				/* v2 only */
				SSLChangeHdskState(ctx, SSL2_HdskStateServerHello);
			}
            break;
        case SSL2_MsgClientHello:
            if ((err = SSL2CompareSessionIDs(ctx)) != 0)
                return err;
            if (ctx->sessionMatch == 0)
                if ((err = SSL2GenerateSessionID(ctx)) != 0)
                    return err;
            if ((err = SSL2PrepareAndQueueMessage(SSL2EncodeServerHello, ctx)) != 0)
                return err;
            if (ctx->sessionMatch == 0)
            {   SSLChangeHdskState(ctx, SSL2_HdskStateClientMasterKey);
                break;
            }
			sslLogResumSessDebug("===RESUMING SSL2 server-side session");
            if ((err = SSL2InstallSessionKey(ctx)) != 0)
                return err;
            /* Fall through for matching session; lame, but true */
        case SSL2_MsgClientMasterKey:
            if ((err = SSL2InitCiphers(ctx)) != 0)
                return err;
            if ((err = SSL2PrepareAndQueueMessage(SSL2EncodeServerVerify, ctx)) != 0)
                return err;
            if ((err = SSL2PrepareAndQueueMessage(SSL2EncodeServerFinished, ctx)) != 0)
                return err;
            SSLChangeHdskState(ctx, SSL2_HdskStateClientFinished);
            break;
        case SSL2_MsgServerHello:
            if (ctx->sessionMatch == 0)
            {   if ((err = SSL2PrepareAndQueueMessage(SSL2EncodeClientMasterKey, ctx)) != 0)
                    return err;
            }
            else
            {   
				sslLogResumSessDebug("===RESUMING SSL2 client-side session");
				if ((err = SSL2InstallSessionKey(ctx)) != 0)
                    return err;
            }
            if ((err = SSL2InitCiphers(ctx)) != 0)
                return err;
            if ((err = SSL2PrepareAndQueueMessage(SSL2EncodeClientFinished, ctx)) != 0)
                return err;
            SSLChangeHdskState(ctx, SSL2_HdskStateServerVerify);
            break;
        case SSL2_MsgClientFinished:
            /* Handshake is complete; turn ciphers on */
            ctx->writeCipher.ready = 1;
            ctx->readCipher.ready = 1;
            /* original code never got out of SSL2_MsgClientFinished state */
            assert(ctx->protocolSide == SSL_ServerSide);
            SSLChangeHdskState(ctx, SSL_HdskStateServerReady);
            if (ctx->peerID.data != 0)
                SSLAddSessionData(ctx);
            break;
        case SSL2_MsgServerVerify:
            SSLChangeHdskState(ctx, SSL2_HdskStateServerFinished);
            break;
        case SSL2_MsgRequestCert:
            if ((err = SSL2SendError(SSL2_ErrNoCert, ctx)) != 0)
                return err;
            break;
        case SSL2_MsgServerFinished:
            /* Handshake is complete; turn ciphers on */
            ctx->writeCipher.ready = 1;
            ctx->readCipher.ready = 1;
            /* original code never got out of SSL2_MsgServerFinished state */
            assert(ctx->protocolSide == SSL_ClientSide);
            SSLChangeHdskState(ctx, SSL_HdskStateClientReady);
            if (ctx->peerID.data != 0)
                SSLAddSessionData(ctx);
            break;
        case SSL2_MsgError:
        case SSL2_MsgClientCert:
            return errSSLProtocol;
            break;
    }
    
    return noErr;
}

OSStatus
SSL2PrepareAndQueueMessage(EncodeSSL2MessageFunc encodeFunc, SSLContext *ctx)
{   OSStatus        err;
    SSLRecord       rec;
    
    rec.contentType = SSL_RecordTypeV2_0;
    rec.protocolVersion = SSL_Version_2_0;
    if ((err = encodeFunc(rec.contents, ctx)) != 0)
        return err;

    logSsl2Msg((SSL2MessageType)rec.contents.data[0], 1);
    
	assert(ctx->sslTslCalls != NULL);
	if ((err = ctx->sslTslCalls->writeRecord(rec, ctx)) != 0)
    {   SSLFreeBuffer(rec.contents, ctx);
        return err;
    }
    
	assert((ctx->negProtocolVersion == SSL_Version_Undetermined) ||
	       (ctx->negProtocolVersion == SSL_Version_2_0));
    if((ctx->negProtocolVersion == SSL_Version_Undetermined) &&
	   (ctx->versionSsl3Enable || ctx->versionTls1Enable)) {
		/* prepare for possible V3/TLS1 upgrade */
        if ((err = SSLHashSHA1.update(ctx->shaState, rec.contents)) != 0 ||
            (err = SSLHashMD5.update(ctx->md5State, rec.contents)) != 0)
            return err;
    }
    err = SSLFreeBuffer(rec.contents, ctx);
    return err;
}

OSStatus
SSL2CompareSessionIDs(SSLContext *ctx)
{   OSStatus        err;
    SSLBuffer       sessionIdentifier;

    ctx->sessionMatch = 0;
    
    if (ctx->resumableSession.data == 0)
        return noErr;
    
    if ((err = SSLRetrieveSessionID(ctx->resumableSession,
                                    &sessionIdentifier, ctx)) != 0)
        return err;
    
    if (sessionIdentifier.length == ctx->sessionID.length &&
        memcmp(sessionIdentifier.data, ctx->sessionID.data, sessionIdentifier.length) == 0)
        ctx->sessionMatch = 1;
    
    if ((err = SSLFreeBuffer(sessionIdentifier, ctx)) != 0)
        return err;
    
    return noErr;
}

OSStatus
SSL2InstallSessionKey(SSLContext *ctx)
{   OSStatus        err;
    
    assert(ctx->sessionMatch != 0);
    assert(ctx->resumableSession.data != 0);
    if ((err = SSLInstallSessionFromData(ctx->resumableSession, ctx)) != 0)
        return err;
    return noErr;
}

OSStatus
SSL2GenerateSessionID(SSLContext *ctx)
{   OSStatus          err;
    
    if ((err = SSLFreeBuffer(ctx->sessionID, ctx)) != 0)
        return err;
    if ((err = SSLAllocBuffer(ctx->sessionID, SSL_SESSION_ID_LEN, ctx)) != 0)
        return err;
    if ((err = sslRand(ctx, &ctx->sessionID)) != 0)
        return err;
    return noErr;
}

OSStatus
SSL2InitCiphers(SSLContext *ctx)
{   OSStatus        err;
    int             keyMaterialLen;
    SSLBuffer       keyData;
    uint8           variantChar, *charPtr, *readKey, *writeKey, *iv;
    SSLBuffer       hashDigest, hashContext, masterKey, challenge, connectionID, variantData;
    
    keyMaterialLen = 2 * ctx->selectedCipherSpec->cipher->keySize;
    if ((err = SSLAllocBuffer(keyData, keyMaterialLen, ctx)) != 0)
        return err;
    
	/* Can't have % in assertion string... */
	#if	SSL_DEBUG
	{
		UInt32 keyModDigestSize = keyMaterialLen % SSLHashMD5.digestSize;
		assert(keyModDigestSize == 0);
    }
	#endif
	
    masterKey.data = ctx->masterSecret;
    masterKey.length = ctx->selectedCipherSpec->cipher->keySize;
    challenge.data = ctx->clientRandom + SSL_CLIENT_SRVR_RAND_SIZE - 
			ctx->ssl2ChallengeLength;
    challenge.length = ctx->ssl2ChallengeLength;
    connectionID.data = ctx->serverRandom;
    connectionID.length = ctx->ssl2ConnectionIDLength;
    variantData.data = &variantChar;
    variantData.length = 1;
    if ((err = SSLAllocBuffer(hashContext, SSLHashMD5.contextSize, ctx)) != 0)
    {   SSLFreeBuffer(keyData, ctx);
        return err;
    }

    variantChar = 0x30;     /* '0' */
    charPtr = keyData.data;
    while (keyMaterialLen)
    {   hashDigest.data = charPtr;
        hashDigest.length = SSLHashMD5.digestSize;
        if ((err = SSLHashMD5.init(hashContext, ctx)) != 0 ||
            (err = SSLHashMD5.update(hashContext, masterKey)) != 0 ||
            (err = SSLHashMD5.update(hashContext, variantData)) != 0 ||
            (err = SSLHashMD5.update(hashContext, challenge)) != 0 ||
            (err = SSLHashMD5.update(hashContext, connectionID)) != 0 ||
            (err = SSLHashMD5.final(hashContext, hashDigest)) != 0)
        {   SSLFreeBuffer(keyData, ctx);
            SSLFreeBuffer(hashContext, ctx);
            return err;
        }
        charPtr += hashDigest.length;
        ++variantChar;
        keyMaterialLen -= hashDigest.length;
    }
    
    assert(charPtr == keyData.data + keyData.length);
    
    if ((err = SSLFreeBuffer(hashContext, ctx)) != 0)
    {   SSLFreeBuffer(keyData, ctx);
        return err;
    }
    
    ctx->readPending.macRef = ctx->selectedCipherSpec->macAlgorithm;
    ctx->writePending.macRef = ctx->selectedCipherSpec->macAlgorithm;
    ctx->readPending.symCipher = ctx->selectedCipherSpec->cipher;
    ctx->writePending.symCipher = ctx->selectedCipherSpec->cipher;
    ctx->readPending.sequenceNum = ctx->readCipher.sequenceNum;
    ctx->writePending.sequenceNum = ctx->writeCipher.sequenceNum;
    
    if (ctx->protocolSide == SSL_ServerSide)
    {   writeKey = keyData.data;
        readKey = keyData.data + ctx->selectedCipherSpec->cipher->keySize;
    }
    else
    {   readKey = keyData.data;
        writeKey = keyData.data + ctx->selectedCipherSpec->cipher->keySize;
    }
    
    iv = ctx->masterSecret + ctx->selectedCipherSpec->cipher->keySize;
    
    if ((err = ctx->readPending.symCipher->initialize(readKey, iv,
                            &ctx->readPending, ctx)) != 0 ||
        (err = ctx->writePending.symCipher->initialize(writeKey, iv,
                            &ctx->writePending, ctx)) != 0)
    {   SSLFreeBuffer(keyData, ctx);
        return err;
    }
    
	/* 
	 * HEY! macSecret is only 20 bytes. This blows up when key size
	 * is greater than 20, e.g., 3DES. 
	 * I'll increase the size of macSecret to 24, 'cause it appears
	 * from the SSL v23 spec that the macSecret really the same size as
	 * CLIENT-WRITE-KEY and  SERVER-READ-KEY (see 1.2 of the spec).
	 */
    memcpy(ctx->readPending.macSecret, readKey, ctx->selectedCipherSpec->cipher->keySize);
    memcpy(ctx->writePending.macSecret, writeKey, ctx->selectedCipherSpec->cipher->keySize);
    
    if ((err = SSLFreeBuffer(keyData, ctx)) != 0)
        return err;
    
    ctx->readCipher = ctx->readPending;
    ctx->writeCipher = ctx->writePending;
    memset(&ctx->readPending, 0, sizeof(CipherContext));        /* Zero out old data */
    memset(&ctx->writePending, 0, sizeof(CipherContext));       /* Zero out old data */
    
    return noErr;
}

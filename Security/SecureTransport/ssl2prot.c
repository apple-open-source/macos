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
	File:		ssl2prot.c

	Contains:	Protocol engine for SSL 2

	Written by:	Doug Mitchell, based on Netscape SSLRef 3.0

	Copyright: (c) 1999 by Apple Computer, Inc., all rights reserved.

*/
/*  *********************************************************************
    File: ssl2prot.c

    SSLRef 3.0 Final -- 11/19/96

    Copyright (c)1996 by Netscape Communications Corp.

    By retrieving this software you are bound by the licensing terms
    disclosed in the file "LICENSE.txt". Please read it, and if you don't
    accept the terms, delete this software.

    SSLRef 3.0 was developed by Netscape Communications Corp. of Mountain
    View, California <http://home.netscape.com/> and Consensus Development
    Corporation of Berkeley, California <http://www.consensus.com/>.

    *********************************************************************

    File: ssl2prot.c   Protocol engine for SSL 2

    This is the heart of the SSL 2 implementation, including the state
    engine for proceeding through the handshake and the necessary code
    for installing negotiated keys and algorithms.

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

#ifndef _SSLHDSHK_H_
#include "sslhdshk.h"
#endif

#ifndef _SSLSESS_H_
#include "sslsess.h"
#endif

#ifndef _SSLALERT_H_
#include "sslalert.h"
#endif

#ifndef	_SSL_DEBUG_H_
#include "sslDebug.h"
#endif

#ifndef	_APPLE_CDSA_H_
#include "appleCdsa.h"
#endif

#include "digests.h"
#include <string.h>
#include <assert.h>

#if	LOG_HDSK_MSG

static char *sslHdskMsgToStr(SSL2MessageType msg)
{
	static char badStr[100];
	
	switch(msg) {
		case ssl2_mt_error:
			return "ssl2_mt_error";	
		case ssl2_mt_client_hello:
			return "ssl2_mt_client_hello";	
		case ssl2_mt_client_master_key:
			return "ssl2_mt_client_master_key";	
		case ssl2_mt_client_finished:
			return "ssl2_mt_client_finished";	
		case ssl2_mt_server_hello:
			return "ssl2_mt_server_hello";	
		case ssl2_mt_server_verify:
			return "ssl2_mt_server_verify";	
		case ssl2_mt_server_finished:
			return "ssl2_mt_server_finished";	
		case ssl2_mt_request_certificate:
			return "ssl2_mt_request_certificate";	
		case ssl2_mt_client_certificate:
			return "ssl2_mt_client_certificate";	
		case ssl2_mt_kickstart_handshake:
			return "ssl2_mt_kickstart_handshake";	
		default:
			sprintf(badStr, "Unknown msg (%d(d)", msg);
			return badStr;
	}
}

static void logSsl2Msg(SSL2MessageType msg, char sent)
{
	char *ms = sslHdskMsgToStr(msg);
	printf("...msg %s: %s\n", (sent ? "sent" : "recd"), ms);
}

#else	/* SSL_DEBUG */

#define logSsl2Msg(m, s)

#endif

SSLErr
SSL2ProcessMessage(SSLRecord rec, SSLContext *ctx)
{   SSLErr              err = 0;
    SSL2MessageType     msg;
    SSLBuffer           contents;
    
    if (rec.contents.length < 2)
        return ERR(SSLProtocolErr);
    
    msg = (SSL2MessageType)rec.contents.data[0];
    contents.data = rec.contents.data + 1;
    contents.length = rec.contents.length - 1;
    
    logSsl2Msg(msg, 0);
    
    switch (msg)
    {   case ssl2_mt_error:
    		err = SSLConnectionClosedError;
            break;
        case ssl2_mt_client_hello:
            if (ctx->state != HandshakeServerUninit)
                return ERR(SSLProtocolErr);
            ERR(err = SSL2ProcessClientHello(contents, ctx));
            if (err == SSLNegotiationErr)
                ERR(SSL2SendError(ssl2_pe_no_cipher, ctx));
            break;
        case ssl2_mt_client_master_key:
            if (ctx->state != HandshakeSSL2ClientMasterKey)
                return ERR(SSLProtocolErr);
            ERR(err = SSL2ProcessClientMasterKey(contents, ctx));
            break;
        case ssl2_mt_client_finished:
            if (ctx->state != HandshakeSSL2ClientFinished)
                return ERR(SSLProtocolErr);
            ERR(err = SSL2ProcessClientFinished(contents, ctx));
            break;
        case ssl2_mt_server_hello:
            if (ctx->state != HandshakeSSL2ServerHello &&
                ctx->state != HandshakeServerHelloUnknownVersion)
                return ERR(SSLProtocolErr);
            ERR(err = SSL2ProcessServerHello(contents, ctx));
            if (err == SSLNegotiationErr)
                ERR(SSL2SendError(ssl2_pe_no_cipher, ctx));
            break;
        case ssl2_mt_server_verify:
            if (ctx->state != HandshakeSSL2ServerVerify)
                return ERR(SSLProtocolErr);
            ERR(err = SSL2ProcessServerVerify(contents, ctx));
            break;
        case ssl2_mt_server_finished:
            if (ctx->state != HandshakeSSL2ServerFinished) {
            	#if	LOG_HDSK_STATE
            	errorLog1("ssl2_mt_server_finished; state %s\n",
            		hdskStateToStr(ctx->state));
            	#endif
                return ERR(SSLProtocolErr);
            }
            ERR(err = SSL2ProcessServerFinished(contents, ctx));
            break;
        case ssl2_mt_request_certificate:
            /* Don't process the request; we don't support client certification */
            break;
        case ssl2_mt_client_certificate:
            return ERR(SSLProtocolErr);
            break;
        default:
            DEBUGVAL1("Unknown message %d", msg);
            return ERR(SSLProtocolErr);
            break;
    }
    
    if (err == 0)
    {   	/* FIXME - use requested or negotiated protocol version here? */
    	if (msg == ssl2_mt_client_hello && (ctx->negProtocolVersion >= SSL_Version_3_0))
        {   /* Promote this message to SSL 3 protocol */
            if (ERR(err = SSL3ReceiveSSL2ClientHello(rec, ctx)) != 0)
                return err;
        }
        else
            ERR(err = SSL2AdvanceHandshake(msg, ctx));
    }
    return err;
}

SSLErr
SSL2AdvanceHandshake(SSL2MessageType msg, SSLContext *ctx)
{   SSLErr          err;
    
    err = SSLNoErr;
    
    switch (msg)
    {   case ssl2_mt_kickstart_handshake:
            if (ctx->negProtocolVersion == SSL_Version_3_0_With_2_0_Hello ||
                ctx->negProtocolVersion == SSL_Version_Undetermined)
                if (ERR(err = SSLInitMessageHashes(ctx)) != 0)
                    return err;
            if (ERR(err = SSL2PrepareAndQueueMessage(SSL2EncodeClientHello, ctx)) != 0)
                return err;
            switch (ctx->negProtocolVersion)
            {   case SSL_Version_Undetermined:
                    SSLChangeHdskState(ctx, HandshakeServerHelloUnknownVersion);
                    break;
                case SSL_Version_3_0_With_2_0_Hello:
					assert((ctx->reqProtocolVersion == SSL_Version_3_0) ||
						   (ctx->reqProtocolVersion == TLS_Version_1_0));
                    ctx->negProtocolVersion = ctx->reqProtocolVersion;
				    #if LOG_NEGOTIATE
				    dprintf2("===SSL client kickstart: negVersion is %d_%d\n",
						ctx->negProtocolVersion >> 8, ctx->negProtocolVersion & 0xff);
				    #endif
                  SSLChangeHdskState(ctx, HandshakeServerHello);
                    break;
                case SSL_Version_2_0:
                    SSLChangeHdskState(ctx, HandshakeSSL2ServerHello);
                    break;
                case SSL_Version_3_0_Only:
                case SSL_Version_3_0:
                case TLS_Version_1_0_Only:
                case TLS_Version_1_0:
                default:
                    ASSERTMSG("Bad protocol version for sending SSL 2 Client Hello");
                    break;
            }
            break;
        case ssl2_mt_client_hello:
            if (ERR(err = SSL2CompareSessionIDs(ctx)) != 0)
                return err;
            if (ctx->ssl2SessionMatch == 0)
                if (ERR(err = SSL2GenerateSessionID(ctx)) != 0)
                    return err;
            if (ERR(err = SSL2PrepareAndQueueMessage(SSL2EncodeServerHello, ctx)) != 0)
                return err;
            if (ctx->ssl2SessionMatch == 0)
            {   SSLChangeHdskState(ctx, HandshakeSSL2ClientMasterKey);
                break;
            }
			SSLLogResumSess("===RESUMING SSL2 server-side session\n");
            if (ERR(err = SSL2InstallSessionKey(ctx)) != 0)
                return err;
            /* Fall through for matching session; lame, but true */
        case ssl2_mt_client_master_key:
            if (ERR(err = SSL2InitCiphers(ctx)) != 0)
                return err;
            if (ERR(err = SSL2PrepareAndQueueMessage(SSL2EncodeServerVerify, ctx)) != 0)
                return err;
            if (ERR(err = SSL2PrepareAndQueueMessage(SSL2EncodeServerFinished, ctx)) != 0)
                return err;
            SSLChangeHdskState(ctx, HandshakeSSL2ClientFinished);
            break;
        case ssl2_mt_server_hello:
            if (ctx->ssl2SessionMatch == 0)
            {   if (ERR(err = SSL2PrepareAndQueueMessage(SSL2EncodeClientMasterKey, ctx)) != 0)
                    return err;
            }
            else
            {   
				SSLLogResumSess("===RESUMING SSL2 client-side session\n");
				if (ERR(err = SSL2InstallSessionKey(ctx)) != 0)
                    return err;
            }
            if (ERR(err = SSL2InitCiphers(ctx)) != 0)
                return err;
            if (ERR(err = SSL2PrepareAndQueueMessage(SSL2EncodeClientFinished, ctx)) != 0)
                return err;
            SSLChangeHdskState(ctx, HandshakeSSL2ServerVerify);
            break;
        case ssl2_mt_client_finished:
            /* Handshake is complete; turn ciphers on */
            ctx->writeCipher.ready = 1;
            ctx->readCipher.ready = 1;
            /* original code never got out of ssl2_mt_client_finished state */
            CASSERT(ctx->protocolSide == SSL_ServerSide);
            SSLChangeHdskState(ctx, HandshakeServerReady);
            if (ctx->peerID.data != 0)
                ERR(SSLAddSessionData(ctx));
            break;
        case ssl2_mt_server_verify:
            SSLChangeHdskState(ctx, HandshakeSSL2ServerFinished);
            break;
        case ssl2_mt_request_certificate:
            if (ERR(err = SSL2SendError(ssl2_pe_no_certificate, ctx)) != 0)
                return err;
            break;
        case ssl2_mt_server_finished:
            /* Handshake is complete; turn ciphers on */
            ctx->writeCipher.ready = 1;
            ctx->readCipher.ready = 1;
            /* original code never got out of ssl2_mt_server_finished state */
            CASSERT(ctx->protocolSide == SSL_ClientSide);
            SSLChangeHdskState(ctx, HandshakeClientReady);
            if (ctx->peerID.data != 0)
                ERR(SSLAddSessionData(ctx));
            break;
        case ssl2_mt_error:
        case ssl2_mt_client_certificate:
            return ERR(SSLProtocolErr);
            break;
    }
    
    return SSLNoErr;
}

SSLErr
SSL2PrepareAndQueueMessage(EncodeSSL2MessageFunc encodeFunc, SSLContext *ctx)
{   SSLErr          err;
    SSLRecord       rec;
    
    rec.contentType = SSL_version_2_0_record;
    rec.protocolVersion = SSL_Version_2_0;
    if (ERR(err = encodeFunc(&rec.contents, ctx)) != 0)
        return err;

    logSsl2Msg((SSL2MessageType)rec.contents.data[0], 1);
    
	assert(ctx->sslTslCalls != NULL);
	if (ERR(err = ctx->sslTslCalls->writeRecord(rec, ctx)) != 0)
    {   ERR(SSLFreeBuffer(&rec.contents, &ctx->sysCtx));
        return err;
    }
    
    if (ctx->negProtocolVersion == SSL_Version_3_0_With_2_0_Hello ||
        ctx->negProtocolVersion == SSL_Version_Undetermined)
        if (ERR(err = SSLHashSHA1.update(ctx->shaState, rec.contents)) != 0 ||
            ERR(err = SSLHashMD5.update(ctx->md5State, rec.contents)) != 0)
            return err;
    
    ERR(err = SSLFreeBuffer(&rec.contents, &ctx->sysCtx));
    return err;
}

SSLErr
SSL2CompareSessionIDs(SSLContext *ctx)
{   SSLErr          err;
    SSLBuffer       sessionIdentifier;

    ctx->ssl2SessionMatch = 0;
    
    if (ctx->resumableSession.data == 0)
        return SSLNoErr;
    
    if (ERR(err = SSLRetrieveSessionID(ctx->resumableSession,
                                    &sessionIdentifier, ctx)) != 0)
        return err;
    
    if (sessionIdentifier.length == ctx->sessionID.length &&
        memcmp(sessionIdentifier.data, ctx->sessionID.data, sessionIdentifier.length) == 0)
        ctx->ssl2SessionMatch = 1;
    
    if (ERR(err = SSLFreeBuffer(&sessionIdentifier, &ctx->sysCtx)) != 0)
        return err;
    
    return SSLNoErr;
}

SSLErr
SSL2InstallSessionKey(SSLContext *ctx)
{   SSLErr          err;
    
    CASSERT(ctx->ssl2SessionMatch != 0);
    CASSERT(ctx->resumableSession.data != 0);
    if (ERR(err = SSLInstallSessionFromData(ctx->resumableSession, ctx)) != 0)
        return err;
    return SSLNoErr;
}

SSLErr
SSL2GenerateSessionID(SSLContext *ctx)
{   SSLErr          err;
    
    if (ERR(err = SSLFreeBuffer(&ctx->sessionID, &ctx->sysCtx)) != 0)
        return err;
    if (ERR(err = SSLAllocBuffer(&ctx->sessionID, SSL_SESSION_ID_LEN, &ctx->sysCtx)) != 0)
        return err;
    if ((err = sslRand(ctx, &ctx->sessionID)) != 0)
        return err;
    return SSLNoErr;
}

SSLErr
SSL2InitCiphers(SSLContext *ctx)
{   SSLErr          err;
    int             keyMaterialLen;
    SSLBuffer       keyData;
    uint8           variantChar, *progress, *readKey, *writeKey, *iv;
    SSLBuffer       hashDigest, hashContext, masterKey, challenge, connectionID, variantData;
    
    keyMaterialLen = 2 * ctx->selectedCipherSpec->cipher->keySize;
    if (ERR(err = SSLAllocBuffer(&keyData, keyMaterialLen, &ctx->sysCtx)) != 0)
        return err;
    
	/* Can't have % in assertion string... */
	#if	SSL_DEBUG
	{
		UInt32 keyModDigestSize = keyMaterialLen % SSLHashMD5.digestSize;
		CASSERT(keyModDigestSize == 0);
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
    if (ERR(err = SSLAllocBuffer(&hashContext, SSLHashMD5.contextSize, &ctx->sysCtx)) != 0)
    {   ERR(SSLFreeBuffer(&keyData, &ctx->sysCtx));
        return err;
    }

    variantChar = 0x30;     /* '0' */
    progress = keyData.data;
    while (keyMaterialLen)
    {   hashDigest.data = progress;
        hashDigest.length = SSLHashMD5.digestSize;
        if (ERR(err = SSLHashMD5.init(hashContext, ctx)) != 0 ||
            ERR(err = SSLHashMD5.update(hashContext, masterKey)) != 0 ||
            ERR(err = SSLHashMD5.update(hashContext, variantData)) != 0 ||
            ERR(err = SSLHashMD5.update(hashContext, challenge)) != 0 ||
            ERR(err = SSLHashMD5.update(hashContext, connectionID)) != 0 ||
            ERR(err = SSLHashMD5.final(hashContext, hashDigest)) != 0)
        {   SSLFreeBuffer(&keyData, &ctx->sysCtx);
            SSLFreeBuffer(&hashContext, &ctx->sysCtx);
            return err;
        }
        progress += hashDigest.length;
        ++variantChar;
        keyMaterialLen -= hashDigest.length;
    }
    
    CASSERT(progress == keyData.data + keyData.length);
    
    if (ERR(err = SSLFreeBuffer(&hashContext, &ctx->sysCtx)) != 0)
    {   ERR(SSLFreeBuffer(&keyData, &ctx->sysCtx));
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
    
    /* APPLE_CDSA symmetric cipher changes....*/
    if (ERR(err = ctx->readPending.symCipher->initialize(readKey, iv,
                            &ctx->readPending, ctx)) != 0 ||
        ERR(err = ctx->writePending.symCipher->initialize(writeKey, iv,
                            &ctx->writePending, ctx)) != 0)
    {   ERR(SSLFreeBuffer(&keyData, &ctx->sysCtx));
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
    
    if (ERR(err = SSLFreeBuffer(&keyData, &ctx->sysCtx)) != 0)
        return err;
    
    ctx->readCipher = ctx->readPending;
    ctx->writeCipher = ctx->writePending;
    memset(&ctx->readPending, 0, sizeof(CipherContext));        /* Zero out old data */
    memset(&ctx->writePending, 0, sizeof(CipherContext));       /* Zero out old data */
    
    return SSLNoErr;
}

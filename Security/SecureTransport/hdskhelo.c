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
	File:		hdskhelo.c

	Contains:	Support for client hello and server hello messages. 

	Written by:	Doug Mitchell, based on Netscape SSLRef 3.0

	Copyright: (c) 1999 by Apple Computer, Inc., all rights reserved.

*/
/*  *********************************************************************
    File: hdskhelo.c

    SSLRef 3.0 Final -- 11/19/96

    Copyright (c)1996 by Netscape Communications Corp.

    By retrieving this software you are bound by the licensing terms
    disclosed in the file "LICENSE.txt". Please read it, and if you don't
    accept the terms, delete this software.

    SSLRef 3.0 was developed by Netscape Communications Corp. of Mountain
    View, California <http://home.netscape.com/> and Consensus Development
    Corporation of Berkeley, California <http://www.consensus.com/>.

    *********************************************************************

    File: hdskhelo.c   Support for client hello and server hello messages

    Also, encoding of Random structures and initializing the message
    hashes used for calculating finished and certificate verify messages.

    ****************************************************************** */

#ifndef _SSLCTX_H_
#include "sslctx.h"
#endif

#ifndef _SSLHDSHK_H_
#include "sslhdshk.h"
#endif

#ifndef _SSLALLOC_H_
#include "sslalloc.h"
#endif

#ifndef _SSLSESS_H_
#include "sslsess.h"
#endif

#ifndef _SSLUTIL_H_
#include "sslutil.h"
#endif

#ifndef	_SSL_DEBUG_H_
#include "sslDebug.h"
#endif

#ifndef	_APPLE_GLUE_H_
#include "appleGlue.h"
#endif

#ifndef	_APPLE_CDSA_H_
#include "appleCdsa.h"
#endif

#ifndef	_DIGESTS_H_
#include "digests.h"
#endif

#ifndef	_CIPHER_SPECS_H_
#include "cipherSpecs.h"
#endif

#include <string.h>

static SSLErr SSLEncodeRandom(unsigned char *p, SSLContext *ctx);

/* IE treats null session id as valid; two consecutive sessions with NULL ID
 * are considered a match. Workaround: when resumable sessions are disabled, 
 * send a random session ID. */
#define SSL_IE_NULL_RESUME_BUG		1
#if		SSL_IE_NULL_RESUME_BUG
#define SSL_NULL_ID_LEN				32	/* length of bogus session ID */
#endif

SSLErr
SSLEncodeServerHello(SSLRecord *serverHello, SSLContext *ctx)
{   SSLErr          err;
    UInt8           *progress;
    int             sessionIDLen;
    
    sessionIDLen = 0;
    if (ctx->sessionID.data != 0)
        sessionIDLen = (UInt8)ctx->sessionID.length;
	#if 	SSL_IE_NULL_RESUME_BUG
	if(sessionIDLen == 0) {
		sessionIDLen = SSL_NULL_ID_LEN;
	}	
	#endif	/* SSL_IE_NULL_RESUME_BUG */
		
 	#if LOG_NEGOTIATE
	dprintf2("===SSL3 server: sending version %d_%d\n",
		ctx->negProtocolVersion >> 8, ctx->negProtocolVersion & 0xff);
	dprintf1("...sessionIDLen = %d\n", sessionIDLen);
	#endif
    serverHello->protocolVersion = ctx->negProtocolVersion;
    serverHello->contentType = SSL_handshake;
    if ((err = SSLAllocBuffer(&serverHello->contents, 42 + sessionIDLen, &ctx->sysCtx)) != 0)
        return err;
    
    progress = serverHello->contents.data;
    *progress++ = SSL_server_hello;
    progress = SSLEncodeInt(progress, 38 + sessionIDLen, 3);
    progress = SSLEncodeInt(progress, serverHello->protocolVersion, 2);
    if ((err = SSLEncodeRandom(progress, ctx)) != 0)
        return err;
    memcpy(ctx->serverRandom, progress, SSL_CLIENT_SRVR_RAND_SIZE);
    progress += SSL_CLIENT_SRVR_RAND_SIZE;
	*(progress++) = (UInt8)sessionIDLen;
	#if 	SSL_IE_NULL_RESUME_BUG
	if(ctx->sessionID.data != NULL) {
		/* normal path for enabled resumable session */
		memcpy(progress, ctx->sessionID.data, sessionIDLen);
	}
	else {
		/* IE workaround */
		SSLBuffer rb;
		rb.data = progress;
		rb.length = SSL_NULL_ID_LEN;
		sslRand(ctx, &rb);
	}
	#else	
    if (sessionIDLen > 0)
        memcpy(progress, ctx->sessionID.data, sessionIDLen);
	#endif	/* SSL_IE_NULL_RESUME_BUG */
	progress += sessionIDLen;
    progress = SSLEncodeInt(progress, ctx->selectedCipher, 2);
    *(progress++) = 0;      /* Null compression */

 	#if LOG_NEGOTIATE
    dprintf1("ssl3: server specifying cipherSuite 0x%lx\n", (UInt32)ctx->selectedCipher);
    #endif
	
    CASSERT(progress == serverHello->contents.data + serverHello->contents.length);
    
    return SSLNoErr;
}

SSLErr
SSLProcessServerHello(SSLBuffer message, SSLContext *ctx)
{   SSLErr              err;
    SSLProtocolVersion  protocolVersion;
    unsigned int        sessionIDLen;
    UInt8               *p;
    
    CASSERT(ctx->protocolSide == SSL_ClientSide);
    
    if (message.length < 38 || message.length > 70) {
    	errorLog0("SSLProcessServerHello: msg len error\n");
        return SSLProtocolErr;
    }
    p = message.data;
    
    protocolVersion = (SSLProtocolVersion)SSLDecodeInt(p, 2);
    p += 2;
    if (protocolVersion > ctx->maxProtocolVersion) {
        return SSLNegotiationErr;
	}
    ctx->negProtocolVersion = protocolVersion;
	switch(protocolVersion) {
		case SSL_Version_3_0:
			ctx->sslTslCalls = &Ssl3Callouts;
			break;
		case TLS_Version_1_0:
 			ctx->sslTslCalls = &Tls1Callouts;
			break;
		default:
			return SSLNegotiationErr;
	}
	#if LOG_NEGOTIATE
    dprintf2("===SSL3 client: negVersion is %d_%d\n",
		(protocolVersion >> 8) & 0xff, protocolVersion & 0xff);
    #endif
    
    memcpy(ctx->serverRandom, p, 32);
    p += 32;
    
    sessionIDLen = *p++;
    if (message.length != 38 + sessionIDLen) {
    	errorLog0("SSLProcessServerHello: msg len error 2\n");
        return SSLProtocolErr;
    }
    if (sessionIDLen > 0 && ctx->peerID.data != 0)
    {   /* Don't die on error; just treat it as an uncached session */
        err = SSLAllocBuffer(&ctx->sessionID, sessionIDLen, &ctx->sysCtx);
        if (err == 0)
            memcpy(ctx->sessionID.data, p, sessionIDLen);
    }
    p += sessionIDLen;
    
    ctx->selectedCipher = (UInt16)SSLDecodeInt(p,2);
    #if	LOG_NEGOTIATE
    dprintf1("===ssl3: server requests cipherKind %d\n", 
    	(unsigned)ctx->selectedCipher);
    #endif
    p += 2;
    if ((err = FindCipherSpec(ctx)) != 0) {
        return err;
    }
    
    if (*p++ != 0)      /* Compression */
        return SSLUnsupportedErr;
    
    CASSERT(p == message.data + message.length);
    return SSLNoErr;
}

SSLErr
SSLEncodeClientHello(SSLRecord *clientHello, SSLContext *ctx)
{   int             length, i;
    SSLErr          err;
    unsigned char   *p;
    SSLBuffer       sessionIdentifier;
    UInt16          sessionIDLen;
    
    CASSERT(ctx->protocolSide == SSL_ClientSide);
    
    sessionIDLen = 0;
    if (ctx->resumableSession.data != 0)
    {   if (ERR(err = SSLRetrieveSessionID(ctx->resumableSession, &sessionIdentifier, ctx)) != 0)
        {   return err;
        }
        sessionIDLen = sessionIdentifier.length;
    }
    
    length = 39 + 2*(ctx->numValidCipherSpecs) + sessionIDLen;
    
    clientHello->protocolVersion = ctx->maxProtocolVersion;
    clientHello->contentType = SSL_handshake;
    if ((err = SSLAllocBuffer(&clientHello->contents, length + 4, &ctx->sysCtx)) != 0)
        return err;
    
    p = clientHello->contents.data;
    *p++ = SSL_client_hello;
    p = SSLEncodeInt(p, length, 3);
    p = SSLEncodeInt(p, ctx->maxProtocolVersion, 2);
  	#if LOG_NEGOTIATE
	dprintf2("===SSL3 client: proclaiming max protocol %d_%d capable ONLY\n",
		ctx->maxProtocolVersion >> 8, ctx->maxProtocolVersion & 0xff);
	#endif
   if ((err = SSLEncodeRandom(p, ctx)) != 0)
    {   SSLFreeBuffer(&clientHello->contents, &ctx->sysCtx);
        return err;
    }
    memcpy(ctx->clientRandom, p, SSL_CLIENT_SRVR_RAND_SIZE);
    p += 32;
    *p++ = sessionIDLen;    /* 1 byte vector length */
    if (sessionIDLen > 0)
    {   memcpy(p, sessionIdentifier.data, sessionIDLen);
        if ((err = SSLFreeBuffer(&sessionIdentifier, &ctx->sysCtx)) != 0)
            return err;
    }
    p += sessionIDLen;
    p = SSLEncodeInt(p, 2*(ctx->numValidCipherSpecs), 2);  /* 2 byte long vector length */
    for (i = 0; i<ctx->numValidCipherSpecs; ++i)
        p = SSLEncodeInt(p, ctx->validCipherSpecs[i].cipherSpec, 2);
    *p++ = 1;                               /* 1 byte long vector */
    *p++ = 0;                               /* null compression */
    
    CASSERT(p == clientHello->contents.data + clientHello->contents.length);
    
    if ((err = SSLInitMessageHashes(ctx)) != 0)
        return err;
    
    return SSLNoErr;
}

SSLErr
SSLProcessClientHello(SSLBuffer message, SSLContext *ctx)
{   SSLErr              err;
    SSLProtocolVersion  clientVersion;
    UInt16              cipherListLen, cipherCount, desiredSpec, cipherSpec;
    UInt8               sessionIDLen, compressionCount;
    UInt8               *progress;
    int                 i;
    
    if (message.length < 41) {
    	errorLog0("SSLProcessClientHello: msg len error 1\n");
        return SSLProtocolErr;
    }
    progress = message.data;
    clientVersion = (SSLProtocolVersion)SSLDecodeInt(progress, 2);
    progress += 2;
	#if old_way
	/* tested, works with SSLv3 */
    if (clientVersion < SSL_Version_3_0) {
        #if LOG_NEGOTIATE
        dprintf1("===SSL3 server: clientVersion %s rejected\n", clientVersion);
        #endif
        return SSLUnsupportedErr;
    }
    ctx->negProtocolVersion = SSL_Version_3_0;
	#else	
	/* Untested, for TLS */
	if(clientVersion > ctx->maxProtocolVersion) {
		clientVersion = ctx->maxProtocolVersion;
	}
	switch(clientVersion) {
		case SSL_Version_3_0:
			ctx->sslTslCalls = &Ssl3Callouts;
			break;
		case TLS_Version_1_0:
 			ctx->sslTslCalls = &Tls1Callouts;
			break;
		default:
			return SSLNegotiationErr;
	}
	ctx->negProtocolVersion = clientVersion;
	#endif	/* new_way */
    #if LOG_NEGOTIATE
    dprintf2("===SSL3 server: negVersion is %d_%d\n",
		clientVersion >> 8, clientVersion & 0xff);
    #endif
    
    memcpy(ctx->clientRandom, progress, SSL_CLIENT_SRVR_RAND_SIZE);
    progress += 32;
    sessionIDLen = *(progress++);
    if (message.length < 41 + sessionIDLen) {
    	errorLog0("SSLProcessClientHello: msg len error 2\n");
        return SSLProtocolErr;
    }
    if (sessionIDLen > 0 && ctx->peerID.data != 0)
    {   /* Don't die on error; just treat it as an uncacheable session */
        err = SSLAllocBuffer(&ctx->sessionID, sessionIDLen, &ctx->sysCtx);
        if (err == 0)
            memcpy(ctx->sessionID.data, progress, sessionIDLen);
    }
    progress += sessionIDLen;
    
    cipherListLen = (UInt16)SSLDecodeInt(progress, 2);  /* Count of cipherSpecs, must be even & >= 2 */
    progress += 2;
    if ((cipherListLen & 1) || cipherListLen < 2 || message.length < 39 + sessionIDLen + cipherListLen) {
    	errorLog0("SSLProcessClientHello: msg len error 3\n");
        return SSLProtocolErr;
    }
    cipherCount = cipherListLen/2;
    cipherSpec = 0xFFFF;        /* No match marker */
    while (cipherSpec == 0xFFFF && cipherCount--)
    {   desiredSpec = (UInt16)SSLDecodeInt(progress, 2);
        progress += 2;
        for (i = 0; i <ctx->numValidCipherSpecs; i++)
        {   if (ctx->validCipherSpecs[i].cipherSpec == desiredSpec)
            {   cipherSpec = desiredSpec;
                break;
            }
        }
    }
    
    if (cipherSpec == 0xFFFF)
        return SSLNegotiationErr;
    progress += 2 * cipherCount;    /* Advance past unchecked cipherCounts */
    ctx->selectedCipher = cipherSpec;
    if ((err = FindCipherSpec(ctx)) != 0) {
        return err;
    }
    #if	LOG_NEGOTIATE
    dprintf1("ssl3 server: selecting cipherKind 0x%x\n", (unsigned)ctx->selectedCipher);
    #endif
    
    compressionCount = *(progress++);
/* message.length restriction relaxed to allow too-long messages for future expansion
    following recommendation of TLS meeting 5/29/96 */
    if (compressionCount < 1 || message.length < 38 + sessionIDLen + cipherListLen + compressionCount) {
    	errorLog0("SSLProcessClientHello: msg len error 4\n");
        return SSLProtocolErr;
    }
    /* Ignore list; we're doing null */
    
    if ((err = SSLInitMessageHashes(ctx)) != 0)
        return err;
    
    return SSLNoErr;
}

static SSLErr
SSLEncodeRandom(unsigned char *p, SSLContext *ctx)
{   SSLBuffer   randomData;
    SSLErr      err;
    UInt32      time;
    
    if ((err = sslTime(&time)) != 0)
        return err;
    SSLEncodeInt(p, time, 4);
    randomData.data = p+4;
    randomData.length = 28;
   	if((err = sslRand(ctx, &randomData)) != 0)
        return err;
    return SSLNoErr;
}

SSLErr
SSLInitMessageHashes(SSLContext *ctx)
{   SSLErr          err;

    if ((err = CloseHash(&SSLHashSHA1, &ctx->shaState, ctx)) != 0)
        return err;
    if ((err = CloseHash(&SSLHashMD5,  &ctx->md5State, ctx)) != 0)
        return err;
    if ((err = ReadyHash(&SSLHashSHA1, &ctx->shaState, ctx)) != 0)
        return err;
    if ((err = ReadyHash(&SSLHashMD5,  &ctx->md5State, ctx)) != 0)
        return err;
    return SSLNoErr;
}

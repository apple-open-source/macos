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
	File:		hdskfini.c

	Contains:	Finished and server hello done messages. 

	Written by:	Doug Mitchell, based on Netscape SSLRef 3.0

	Copyright: (c) 1999 by Apple Computer, Inc., all rights reserved.

*/
/*  *********************************************************************
    File: hdskfini.c

    SSLRef 3.0 Final -- 11/19/96

    Copyright (c)1996 by Netscape Communications Corp.

    By retrieving this software you are bound by the licensing terms
    disclosed in the file "LICENSE.txt". Please read it, and if you don't
    accept the terms, delete this software.

    SSLRef 3.0 was developed by Netscape Communications Corp. of Mountain
    View, California <http://home.netscape.com/> and Consensus Development
    Corporation of Berkeley, California <http://www.consensus.com/>.

    *********************************************************************

    File: hdskfini.c   Finished and server hello done messages

    Support for encoding and decoding finished and server hello done
    messgages. Also includes the necessary calculations for the Finished
    message; note that the same function is used to calculate certificate
    verify message hashes (without the 'SRVR' or 'CLNT' protocol side
    identifier).

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

#ifndef	_SSL_DEBUG_H_
#include "sslDebug.h"
#endif

#ifndef _SSLUTIL_H_
#include "sslutil.h"
#endif

#ifndef	_DIGESTS_H_
#include "digests.h"
#endif

#include <string.h>
#include <assert.h>

SSLErr
SSLEncodeFinishedMessage(SSLRecord *finished, SSLContext *ctx)
{   SSLErr          err;
    SSLBuffer       finishedMsg, shaMsgState, md5MsgState;
    Boolean         isServerMsg;
    unsigned		finishedSize;
	
    shaMsgState.data = 0;
    md5MsgState.data = 0;
    
	/* size and version depend on negotiatedProtocol */
	switch(ctx->negProtocolVersion) {
		case SSL_Version_3_0:
			finished->protocolVersion = SSL_Version_3_0;
			finishedSize = 36;
			break;
		case TLS_Version_1_0:
			finished->protocolVersion = TLS_Version_1_0;
			finishedSize = 12;
			break;
		default:
			assert(0);
			return SSLInternalError;
	}
    finished->contentType = SSL_handshake;
	/* msg = type + 3 bytes len + finishedSize */
    if ((err = SSLAllocBuffer(&finished->contents, finishedSize + 4, 
			&ctx->sysCtx)) != 0)
        return err;
    
    finished->contents.data[0] = SSL_finished;
    SSLEncodeInt(finished->contents.data + 1, finishedSize, 3);
    
    finishedMsg.data = finished->contents.data + 4;
    finishedMsg.length = finishedSize;
    
    if ((err = CloneHashState(&SSLHashSHA1, ctx->shaState, &shaMsgState, ctx)) != 0)
        goto fail;
    if ((err = CloneHashState(&SSLHashMD5, ctx->md5State, &md5MsgState, ctx)) != 0)
        goto fail;
    isServerMsg = (ctx->protocolSide == SSL_ServerSide) ? true : false;
    if ((err = ctx->sslTslCalls->computeFinishedMac(ctx, finishedMsg, 
			shaMsgState, md5MsgState, isServerMsg)) != 0)
        goto fail;  
    
fail:
    SSLFreeBuffer(&shaMsgState, &ctx->sysCtx);
    SSLFreeBuffer(&md5MsgState, &ctx->sysCtx);
    return err;
}

SSLErr
SSLProcessFinished(SSLBuffer message, SSLContext *ctx)
{   SSLErr          err;
    SSLBuffer       expectedFinished, shaMsgState, md5MsgState;
    Boolean         isServerMsg;
    unsigned		finishedSize;
    
	switch(ctx->negProtocolVersion) {
		case SSL_Version_3_0:
			finishedSize = 36;
			break;
		case TLS_Version_1_0:
			finishedSize = 12;
			break;
		default:
			assert(0);
			return SSLInternalError;
	}
    if (message.length != finishedSize) {
		errorLog0("SSLProcessFinished: msg len error 1\n");
        return SSLProtocolErr;
    }
    expectedFinished.data = 0;
    if ((err = SSLAllocBuffer(&expectedFinished, finishedSize, &ctx->sysCtx)) != 0)
        return err;
    shaMsgState.data = 0;
    if ((err = CloneHashState(&SSLHashSHA1, ctx->shaState, &shaMsgState, ctx)) != 0)
        goto fail;
    md5MsgState.data = 0;
    if ((err = CloneHashState(&SSLHashMD5, ctx->md5State, &md5MsgState, ctx)) != 0)
        goto fail;
    isServerMsg = (ctx->protocolSide == SSL_ServerSide) ? false : true;
    if ((err = ctx->sslTslCalls->computeFinishedMac(ctx, expectedFinished, 
			shaMsgState, md5MsgState, isServerMsg)) != 0)
        goto fail;

    if (memcmp(expectedFinished.data, message.data, finishedSize) != 0)
    {  
   		errorLog0("SSLProcessFinished: memcmp failure\n");
   	 	err = SSLProtocolErr;
        goto fail;
    }
    
fail:
    SSLFreeBuffer(&expectedFinished, &ctx->sysCtx);
    SSLFreeBuffer(&shaMsgState, &ctx->sysCtx);
    SSLFreeBuffer(&md5MsgState, &ctx->sysCtx);
    return err;
}

SSLErr
SSLEncodeServerHelloDone(SSLRecord *helloDone, SSLContext *ctx)
{   SSLErr          err;
    
    helloDone->contentType = SSL_handshake;
	assert((ctx->negProtocolVersion == SSL_Version_3_0) ||
		   (ctx->negProtocolVersion == TLS_Version_1_0));
    helloDone->protocolVersion = ctx->negProtocolVersion;
    if ((err = SSLAllocBuffer(&helloDone->contents, 4, &ctx->sysCtx)) != 0)
        return err;
    helloDone->contents.data[0] = SSL_server_hello_done;
    SSLEncodeInt(helloDone->contents.data+1, 0, 3);     /* Message has 0 length */
    return SSLNoErr;
}

SSLErr
SSLProcessServerHelloDone(SSLBuffer message, SSLContext *ctx)
{   CASSERT(ctx->protocolSide == SSL_ClientSide);
    if (message.length != 0) {
    	errorLog0("SSLProcessServerHelloDone: nonzero msg len\n");
        return SSLProtocolErr;
    }
    return SSLNoErr;
}

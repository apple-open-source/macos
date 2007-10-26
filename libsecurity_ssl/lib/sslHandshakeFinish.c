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
	File:		sslHandshakeFinish.c

	Contains:	Finished and server hello done messages. 

	Written by:	Doug Mitchell

	Copyright: (c) 1999 by Apple Computer, Inc., all rights reserved.

*/

#include "sslContext.h"
#include "sslHandshake.h"
#include "sslMemory.h"
#include "sslDebug.h"
#include "sslUtils.h"
#include "sslDigests.h"

#include <string.h>
#include <assert.h>

OSStatus
SSLEncodeFinishedMessage(SSLRecord *finished, SSLContext *ctx)
{   OSStatus        err;
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
			return errSSLInternal;
	}
    finished->contentType = SSL_RecordTypeHandshake;
	/* msg = type + 3 bytes len + finishedSize */
    if ((err = SSLAllocBuffer(&finished->contents, finishedSize + 4, 
			ctx)) != 0)
        return err;
    
    finished->contents.data[0] = SSL_HdskFinished;
    SSLEncodeInt(finished->contents.data + 1, finishedSize, 3);
    
    finishedMsg.data = finished->contents.data + 4;
    finishedMsg.length = finishedSize;
    
    if ((err = CloneHashState(&SSLHashSHA1, &ctx->shaState, &shaMsgState, ctx)) != 0)
        goto fail;
    if ((err = CloneHashState(&SSLHashMD5, &ctx->md5State, &md5MsgState, ctx)) != 0)
        goto fail;
    isServerMsg = (ctx->protocolSide == SSL_ServerSide) ? true : false;
    if ((err = ctx->sslTslCalls->computeFinishedMac(ctx, finishedMsg, 
			shaMsgState, md5MsgState, isServerMsg)) != 0)
        goto fail;  
    
fail:
    SSLFreeBuffer(&shaMsgState, ctx);
    SSLFreeBuffer(&md5MsgState, ctx);
    return err;
}

OSStatus
SSLProcessFinished(SSLBuffer message, SSLContext *ctx)
{   OSStatus        err;
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
			return errSSLInternal;
	}
    if (message.length != finishedSize) {
		sslErrorLog("SSLProcessFinished: msg len error 1\n");
        return errSSLProtocol;
    }
    expectedFinished.data = 0;
    if ((err = SSLAllocBuffer(&expectedFinished, finishedSize, ctx)) != 0)
        return err;
    shaMsgState.data = 0;
    if ((err = CloneHashState(&SSLHashSHA1, &ctx->shaState, &shaMsgState, ctx)) != 0)
        goto fail;
    md5MsgState.data = 0;
    if ((err = CloneHashState(&SSLHashMD5, &ctx->md5State, &md5MsgState, ctx)) != 0)
        goto fail;
    isServerMsg = (ctx->protocolSide == SSL_ServerSide) ? false : true;
    if ((err = ctx->sslTslCalls->computeFinishedMac(ctx, expectedFinished, 
			shaMsgState, md5MsgState, isServerMsg)) != 0)
        goto fail;

    if (memcmp(expectedFinished.data, message.data, finishedSize) != 0)
    {  
   		sslErrorLog("SSLProcessFinished: memcmp failure\n");
   	 	err = errSSLProtocol;
        goto fail;
    }
    
fail:
    SSLFreeBuffer(&expectedFinished, ctx);
    SSLFreeBuffer(&shaMsgState, ctx);
    SSLFreeBuffer(&md5MsgState, ctx);
    return err;
}

OSStatus
SSLEncodeServerHelloDone(SSLRecord *helloDone, SSLContext *ctx)
{   OSStatus          err;
    
    helloDone->contentType = SSL_RecordTypeHandshake;
	assert((ctx->negProtocolVersion == SSL_Version_3_0) ||
		   (ctx->negProtocolVersion == TLS_Version_1_0));
    helloDone->protocolVersion = ctx->negProtocolVersion;
    if ((err = SSLAllocBuffer(&helloDone->contents, 4, ctx)) != 0)
        return err;
    helloDone->contents.data[0] = SSL_HdskServerHelloDone;
    SSLEncodeInt(helloDone->contents.data+1, 0, 3);     /* Message has 0 length */
    return noErr;
}

OSStatus
SSLProcessServerHelloDone(SSLBuffer message, SSLContext *ctx)
{   assert(ctx->protocolSide == SSL_ClientSide);
    if (message.length != 0) {
    	sslErrorLog("SSLProcessServerHelloDone: nonzero msg len\n");
        return errSSLProtocol;
    }
    return noErr;
}

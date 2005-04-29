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
	File:		sslChangeCipher.cpp

	Contains:	support for change cipher spec messages

	Written by:	Doug Mitchell

	Copyright: (c) 1999 by Apple Computer, Inc., all rights reserved.

*/

#include "sslContext.h"
#include "sslHandshake.h"
#include "sslMemory.h"
#include "sslAlertMessage.h"
#include "sslDebug.h"

#include <assert.h>
#include <string.h>

OSStatus
SSLEncodeChangeCipherSpec(SSLRecord &rec, SSLContext *ctx)
{   OSStatus          err;
    
    assert(ctx->writePending.ready);
    
    sslLogNegotiateDebug("===Sending changeCipherSpec msg");
    rec.contentType = SSL_RecordTypeChangeCipher;
	assert((ctx->negProtocolVersion == SSL_Version_3_0) ||
		   (ctx->negProtocolVersion == TLS_Version_1_0));
    rec.protocolVersion = ctx->negProtocolVersion;
    rec.contents.length = 1;
    if ((err = SSLAllocBuffer(rec.contents, 1, ctx)) != 0)
        return err;
    rec.contents.data[0] = 1;
    
    return noErr;
}

OSStatus
SSLProcessChangeCipherSpec(SSLRecord rec, SSLContext *ctx)
{   OSStatus          err;
    
    if (rec.contents.length != 1 || rec.contents.data[0] != 1)
    {   SSLFatalSessionAlert(SSL_AlertUnexpectedMsg, ctx);
    	sslErrorLog("***bad changeCipherSpec msg: length %d data 0x%x\n",
    		(unsigned)rec.contents.length, (unsigned)rec.contents.data[0]);
        return errSSLProtocol;
    }
    
    if (!ctx->readPending.ready || ctx->state != SSL_HdskStateChangeCipherSpec)
    {   SSLFatalSessionAlert(SSL_AlertUnexpectedMsg, ctx);
    	sslErrorLog("***bad changeCipherSpec msg: readPending.ready %d state %d\n",
    		(unsigned)ctx->readPending.ready, (unsigned)ctx->state);
        return errSSLProtocol;
    }
    
    sslLogNegotiateDebug("===Processing changeCipherSpec msg");
    
    /* Install new cipher spec on read side */
    if ((err = SSLDisposeCipherSuite(&ctx->readCipher, ctx)) != 0)
    {   SSLFatalSessionAlert(SSL_AlertInternalError, ctx);
        return err;
    }
    ctx->readCipher = ctx->readPending;
    ctx->readCipher.ready = 0;      /* Can't send data until Finished is sent */
    SSLChangeHdskState(ctx, SSL_HdskStateFinished);
    memset(&ctx->readPending, 0, sizeof(CipherContext)); 	/* Zero out old data */
    return noErr;    
}

OSStatus
SSLDisposeCipherSuite(CipherContext *cipher, SSLContext *ctx)
{   OSStatus      err;
    
	/* symmetric key */
    if (cipher->symKey)
    {   if ((err = cipher->symCipher->finish(cipher, ctx)) != 0)
            return err;
        cipher->symKey = 0;
    }
    
	/* per-record hash/hmac context */
	ctx->sslTslCalls->freeMac(cipher);
	
    return noErr;
}

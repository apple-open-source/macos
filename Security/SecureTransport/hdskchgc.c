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
	File:		hdskchgc.c

	Contains:	support for change cipher spec messages

	Written by:	Doug Mitchell, based on Netscape SSLRef 3.0

	Copyright: (c) 1999 by Apple Computer, Inc., all rights reserved.

*/
/*  *********************************************************************
    File: hdskchgc.c

    SSLRef 3.0 Final -- 11/19/96

    Copyright (c)1996 by Netscape Communications Corp.

    By retrieving this software you are bound by the licensing terms
    disclosed in the file "LICENSE.txt". Please read it, and if you don't
    accept the terms, delete this software.

    SSLRef 3.0 was developed by Netscape Communications Corp. of Mountain
    View, California <http://home.netscape.com/> and Consensus Development
    Corporation of Berkeley, California <http://www.consensus.com/>.

    *********************************************************************

    File: hdskchgc.c   Contains support for change cipher spec messages

    Simple support for encoding and decoding change cipher spec messages;
    the decode message also installs the pending read cipher (if it is
    ready).

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

#ifndef _SSLALERT_H_
#include "sslalert.h"
#endif

#ifndef	_SSL_DEBUG_H_
#include "sslDebug.h"
#endif

#include <string.h>

SSLErr
SSLEncodeChangeCipherSpec(SSLRecord *rec, SSLContext *ctx)
{   SSLErr          err;
    
    CASSERT(ctx->writePending.ready);
    
    #if	LOG_NEGOTIATE
    dprintf0("===Sending changeCipherSpec msg\n");
    #endif
    rec->contentType = SSL_change_cipher_spec;
    rec->protocolVersion = SSL_Version_3_0;
    rec->contents.length = 1;
    if ((err = SSLAllocBuffer(&rec->contents, 1, &ctx->sysCtx)) != 0)
        return err;
    rec->contents.data[0] = 1;
    
    return SSLNoErr;
}

SSLErr
SSLProcessChangeCipherSpec(SSLRecord rec, SSLContext *ctx)
{   SSLErr          err;
    
    if (rec.contents.length != 1 || rec.contents.data[0] != 1)
    {   SSLFatalSessionAlert(alert_unexpected_message, ctx);
    	errorLog2("***bad changeCipherSpec msg: length %d data 0x%x\n",
    		(unsigned)rec.contents.length, (unsigned)rec.contents.data[0]);
        return SSLProtocolErr;
    }
    
    if (!ctx->readPending.ready || ctx->state != HandshakeChangeCipherSpec)
    {   SSLFatalSessionAlert(alert_unexpected_message, ctx);
    	errorLog2("***bad changeCipherSpec msg: readPending.ready %d state %d\n",
    		(unsigned)ctx->readPending.ready, (unsigned)ctx->state);
        return SSLProtocolErr;
    }
    
    #if	LOG_NEGOTIATE
    dprintf0("===Processing changeCipherSpec msg\n");
    #endif
    
    /* Install new cipher spec on read side */
    if ((err = SSLDisposeCipherSuite(&ctx->readCipher, ctx)) != 0)
    {   SSLFatalSessionAlert(alert_close_notify, ctx);
        return err;
    }
    ctx->readCipher = ctx->readPending;
    ctx->readCipher.ready = 0;      /* Can't send data until Finished is sent */
    SSLChangeHdskState(ctx, HandshakeFinished);
    memset(&ctx->readPending, 0, sizeof(CipherContext));        /* Zero out old data */
    return SSLNoErr;    
}

SSLErr
SSLDisposeCipherSuite(CipherContext *cipher, SSLContext *ctx)
{   SSLErr      err;
    
    if (cipher->symKey)
    {   if ((err = cipher->symCipher->finish(cipher, ctx)) != 0)
            return err;
        cipher->symKey = 0;
    }
    
    return SSLNoErr;
}

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
	File:		sslrec.c

	Contains:	Encryption, decryption and MACing of data

	Written by:	Doug Mitchell, based on Netscape SSLRef 3.0

	Copyright: (c) 1999 by Apple Computer, Inc., all rights reserved.

*/
/*  *********************************************************************
    File: sslrec.c

    SSLRef 3.0 Final -- 11/19/96

    Copyright (c)1996 by Netscape Communications Corp.

    By retrieving this software you are bound by the licensing terms
    disclosed in the file "LICENSE.txt". Please read it, and if you don't
    accept the terms, delete this software.

    SSLRef 3.0 was developed by Netscape Communications Corp. of Mountain
    View, California <http://home.netscape.com/> and Consensus Development
    Corporation of Berkeley, California <http://www.consensus.com/>.

    *********************************************************************

    File: sslrec.c     Encryption, decryption and MACing of data

    All the transformations which occur between plaintext and the
    secured, authenticated data that goes out over the wire. Also,
    detects incoming SSL 2 hello messages and hands them off to the SSL 2
    record layer (and hands all SSL 2 reading & writing off to the SSL 2
    layer).

    ****************************************************************** */


#include "ssl.h"

#ifndef _SSLREC_H_
#include "sslrec.h"
#endif

#ifndef _SSLALLOC_H_
#include "sslalloc.h"
#endif

#ifndef _CRYPTTYPE_H_
#include "cryptType.h"
#endif

#ifndef _SSLCTX_H_
#include "sslctx.h"
#endif

#ifndef _SSLALERT_H_
#include "sslalert.h"
#endif

#ifndef	_SSL_DEBUG_H_
#include "sslDebug.h"
#endif

#ifndef _SSL2_H_
#include "ssl2.h"
#endif

#ifndef _SSLUTIL_H_
#include "sslutil.h"
#endif

#include "appleGlue.h"
#include <string.h>
#include <assert.h>

/*
 * Lots of servers fail to provide closure alerts when they disconnect. 
 * For now we'll just accept it as long as it occurs on a clean record boundary
 * (and the handshake is complete). 
 */
#define SSL_ALLOW_UNNOTICED_DISCONNECT	1

/* ReadSSLRecord
 *  Attempt to read & decrypt an SSL record.
 */
SSLErr
SSLReadRecord(SSLRecord *rec, SSLContext *ctx)
{   SSLErr          err;
    UInt32          len, contentLen;
    UInt8           *progress;
    SSLBuffer       readData, cipherFragment;
    
    if (!ctx->partialReadBuffer.data || ctx->partialReadBuffer.length < 5)
    {   if (ctx->partialReadBuffer.data)
            if ((err = SSLFreeBuffer(&ctx->partialReadBuffer, &ctx->sysCtx)) != 0)
            {   SSLFatalSessionAlert(alert_close_notify, ctx);
                return ERR(err);
            }
        if ((err = SSLAllocBuffer(&ctx->partialReadBuffer, DEFAULT_BUFFER_SIZE, &ctx->sysCtx)) != 0)
        {   SSLFatalSessionAlert(alert_close_notify, ctx);
            return ERR(err);
        }
    }
    
    if (ctx->negProtocolVersion == SSL_Version_Undetermined ||
        ctx->negProtocolVersion == SSL_Version_3_0_With_2_0_Hello)
        if (ctx->amountRead < 1)
        {   readData.length = 1 - ctx->amountRead;
            readData.data = ctx->partialReadBuffer.data + ctx->amountRead;
            len = readData.length;
            err = sslIoRead(readData, &len, ctx);
            if(err != 0)
            {   if (err == SSLWouldBlockErr)
                    ctx->amountRead += len;
                else
                    SSLFatalSessionAlert(alert_close_notify, ctx);
                return err;
            }
            ctx->amountRead += len;
        }
    
/* In undetermined cases, if the first byte isn't in the range of SSL 3.0
 *  record types, this is an SSL 2.0 record
 */
    switch (ctx->negProtocolVersion)
    {   case SSL_Version_Undetermined:
        case SSL_Version_3_0_With_2_0_Hello:
            if (ctx->partialReadBuffer.data[0] < SSL_smallest_3_0_type ||
                ctx->partialReadBuffer.data[0] > SSL_largest_3_0_type)
                return SSL2ReadRecord(rec, ctx);
            else
                break;
        case SSL_Version_2_0:
            return SSL2ReadRecord(rec, ctx);
        default:
            break;
    }
    
    if (ctx->amountRead < 5)
    {   readData.length = 5 - ctx->amountRead;
        readData.data = ctx->partialReadBuffer.data + ctx->amountRead;
        len = readData.length;
        err = sslIoRead(readData, &len, ctx);
        if(err != 0)
        {   
			switch(err) {
				case SSLWouldBlockErr:
					ctx->amountRead += len;
					break;
				#if	SSL_ALLOW_UNNOTICED_DISCONNECT
				case SSLConnectionClosedGraceful:
					/* legal if we're on record boundary and we've gotten past 
					 * the handshake */
					if((ctx->amountRead == 0) && 				/* nothing pending */
					   (len == 0) &&							/* nothing new */
					   (ctx->state == HandshakeClientReady)) {	/* handshake done */
					    /*
						 * This means that the server has disconnected without 
						 * sending a closure alert notice. This is technically
						 * illegal per the SSL3 spec, but about half of the 
						 * servers out there do it, so we report it as a separate
						 * error which most clients - including (currently)
						 * URLAccess - ignore by treating it the same as
						 * a SSLConnectionClosedGraceful error. Paranoid
						 * clients can detect it and handle it however they
						 * want to.
						 */
						SSLChangeHdskState(ctx, SSLNoNotifyClose);
						err = SSLConnectionClosedNoNotify;
						break;
					}
					else {
						/* illegal disconnect */
						err = SSLConnectionClosedError;
						/* and drop thru to default: fatal alert */
					}
				#endif	/* SSL_ALLOW_UNNOTICED_DISCONNECT */
				default:
					SSLFatalSessionAlert(alert_close_notify, ctx);
					break;
				}
            return err;
        }
        ctx->amountRead += len;
    }
    
    CASSERT(ctx->amountRead >= 5);
    
    progress = ctx->partialReadBuffer.data;
    rec->contentType = *progress++;
    if (rec->contentType < SSL_smallest_3_0_type || 
        rec->contentType > SSL_largest_3_0_type)
        return ERR(SSLProtocolErr);
    
    rec->protocolVersion = (SSLProtocolVersion)SSLDecodeInt(progress, 2);
    progress += 2;
    contentLen = SSLDecodeInt(progress, 2);
    progress += 2;
    if (contentLen > (16384 + 2048))    /* Maximum legal length of an SSLCipherText payload */
    {   SSLFatalSessionAlert(alert_unexpected_message, ctx);
        return ERR(SSLProtocolErr);
    }
    
    if (ctx->partialReadBuffer.length < 5 + contentLen)
    {   if ((err = SSLReallocBuffer(&ctx->partialReadBuffer, 5 + contentLen, &ctx->sysCtx)) != 0)
        {   SSLFatalSessionAlert(alert_close_notify, ctx);
            return ERR(err);
        }
    }
    
    if (ctx->amountRead < 5 + contentLen)
    {   readData.length = 5 + contentLen - ctx->amountRead;
        readData.data = ctx->partialReadBuffer.data + ctx->amountRead;
        len = readData.length;
        err = sslIoRead(readData, &len, ctx);
        if(err != 0)
        {   if (err == SSLWouldBlockErr)
                ctx->amountRead += len;
            else
                SSLFatalSessionAlert(alert_close_notify, ctx);
            return err;
        }
        ctx->amountRead += len;
    }
    
    CASSERT(ctx->amountRead >= 5 + contentLen);
    
    cipherFragment.data = ctx->partialReadBuffer.data + 5;
    cipherFragment.length = contentLen;
    
/* Decrypt the payload & check the MAC, modifying the length of the buffer to indicate the
 *  amount of plaintext data after adjusting for the block size and removing the MAC
 *  (this function generates its own alerts)
 */
	assert(ctx->sslTslCalls != NULL);
    if ((err = ctx->sslTslCalls->decryptRecord(rec->contentType, 
			&cipherFragment, ctx)) != 0)
        return err;
    
/* We appear to have sucessfully received a record; increment the sequence number */
    IncrementUInt64(&ctx->readCipher.sequenceNum);
    
/* Allocate a buffer to return the plaintext in and return it */
    if ((err = SSLAllocBuffer(&rec->contents, cipherFragment.length, &ctx->sysCtx)) != 0)
    {   SSLFatalSessionAlert(alert_close_notify, ctx);
        return ERR(err);
    }
    memcpy(rec->contents.data, cipherFragment.data, cipherFragment.length);
    
    ctx->amountRead = 0;        /* We've used all the data in the cache */
    
    return SSLNoErr;
}

/* common for sslv3 and tlsv1, except for the computeMac callout */
SSLErr SSLVerifyMac(
	UInt8 type, 
	SSLBuffer data, 
	UInt8 *compareMAC, 
	SSLContext *ctx)
{   
	SSLErr          err;
    UInt8           macData[MAX_DIGEST_SIZE];
    SSLBuffer       secret, mac;
    
    secret.data = ctx->readCipher.macSecret;
    secret.length = ctx->readCipher.macRef->hash->digestSize;
    mac.data = macData;
    mac.length = ctx->readCipher.macRef->hash->digestSize;
    
	assert(ctx->sslTslCalls != NULL);
    if ((err = ctx->sslTslCalls->computeMac(type, 
			data, 
			mac, 
			&ctx->readCipher,
			ctx->readCipher.sequenceNum, 
			ctx)) != 0)
        return ERR(err);
    
    if ((memcmp(mac.data, compareMAC, mac.length)) != 0) {
		errorLog0("ssl3VerifyMac: Mac verify failure\n");
        return ERR(SSLProtocolErr);
    }
    return SSLNoErr;
}



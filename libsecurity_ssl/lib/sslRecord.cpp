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
	File:		sslRecord.cpp

	Contains:	Encryption, decryption and MACing of data

	Written by:	Doug Mitchell

	Copyright: (c) 1999 by Apple Computer, Inc., all rights reserved.

*/

#include "ssl.h"

#include "sslRecord.h"
#include "sslMemory.h"
#include "cryptType.h"
#include "sslContext.h"
#include "sslAlertMessage.h"
#include "sslDebug.h"
#include "ssl2.h"
#include "sslUtils.h"
#include "sslDigests.h"

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
OSStatus
SSLReadRecord(SSLRecord &rec, SSLContext *ctx)
{   OSStatus        err;
    UInt32          len, contentLen;
    UInt8           *charPtr;
    SSLBuffer       readData, cipherFragment;
    
    if (!ctx->partialReadBuffer.data || ctx->partialReadBuffer.length < 5)
    {   if (ctx->partialReadBuffer.data)
            if ((err = SSLFreeBuffer(ctx->partialReadBuffer, ctx)) != 0)
            {   SSLFatalSessionAlert(SSL_AlertInternalError, ctx);
                return err;
            }
        if ((err = SSLAllocBuffer(ctx->partialReadBuffer, 
				DEFAULT_BUFFER_SIZE, ctx)) != 0)
        {   SSLFatalSessionAlert(SSL_AlertInternalError, ctx);
            return err;
        }
    }
    
    if (ctx->negProtocolVersion == SSL_Version_Undetermined) {
        if (ctx->amountRead < 1)
        {   readData.length = 1 - ctx->amountRead;
            readData.data = ctx->partialReadBuffer.data + ctx->amountRead;
            len = readData.length;
            err = sslIoRead(readData, &len, ctx);
            if(err != 0)
            {   if (err == errSSLWouldBlock) {
					ctx->amountRead += len;
					return err;
				}
                else {
					/* abort */
					err = errSSLClosedAbort;
					if((ctx->protocolSide == SSL_ClientSide) &&
					   (ctx->amountRead == 0) &&
					   (len == 0)) {
						/*
						 * Detect "server refused to even try to negotiate" 
						 * error, when the server drops the connection before 
						 * sending a single byte.
						 */
						switch(ctx->state) {
							case SSL_HdskStateServerHello:
							case SSL_HdskStateServerHelloUnknownVersion:
								sslHdskStateDebug("Server dropped initial connection\n");
								err = errSSLConnectionRefused;
								break;
							default:
								break;
						}
					}
                    SSLFatalSessionAlert(SSL_AlertCloseNotify, ctx);
					return err;
				}
            }
            ctx->amountRead += len;
        }
    }
	
	/* 
	 * In undetermined cases, if the first byte isn't in the range of SSL 3.0
	 * record types, this is an SSL 2.0 record
	 */
    switch (ctx->negProtocolVersion)
    {   case SSL_Version_Undetermined:
            if (ctx->partialReadBuffer.data[0] < SSL_RecordTypeV3_Smallest ||
                ctx->partialReadBuffer.data[0] > SSL_RecordTypeV3_Largest)
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
				case errSSLWouldBlock:
					ctx->amountRead += len;
					break;
				#if	SSL_ALLOW_UNNOTICED_DISCONNECT
				case errSSLClosedGraceful:
					/* legal if we're on record boundary and we've gotten past 
					 * the handshake */
					if((ctx->amountRead == 0) && 				/* nothing pending */
					   (len == 0) &&							/* nothing new */
					   (ctx->state == SSL_HdskStateClientReady)) {	/* handshake done */
					    /*
						 * This means that the server has disconnected without 
						 * sending a closure alert notice. This is technically
						 * illegal per the SSL3 spec, but about half of the 
						 * servers out there do it, so we report it as a separate
						 * error which most clients - including (currently)
						 * URLAccess - ignore by treating it the same as
						 * a errSSLClosedGraceful error. Paranoid
						 * clients can detect it and handle it however they
						 * want to.
						 */
						SSLChangeHdskState(ctx, SSL_HdskStateNoNotifyClose);
						err = errSSLClosedNoNotify;
						break;
					}
					else {
						/* illegal disconnect */
						err = errSSLClosedAbort;
						/* and drop thru to default: fatal alert */
					}
				#endif	/* SSL_ALLOW_UNNOTICED_DISCONNECT */
				default:
					SSLFatalSessionAlert(SSL_AlertCloseNotify, ctx);
					break;
				}
            return err;
        }
        ctx->amountRead += len;
    }
    
    assert(ctx->amountRead >= 5);
    
    charPtr = ctx->partialReadBuffer.data;
    rec.contentType = *charPtr++;
    if (rec.contentType < SSL_RecordTypeV3_Smallest || 
        rec.contentType > SSL_RecordTypeV3_Largest)
        return errSSLProtocol;
    
    rec.protocolVersion = (SSLProtocolVersion)SSLDecodeInt(charPtr, 2);
    charPtr += 2;
    contentLen = SSLDecodeInt(charPtr, 2);
    charPtr += 2;
    if (contentLen > (16384 + 2048))    /* Maximum legal length of an 
										 * SSLCipherText payload */
    {   SSLFatalSessionAlert(SSL_AlertRecordOverflow, ctx);
        return errSSLProtocol;
    }
    
    if (ctx->partialReadBuffer.length < 5 + contentLen)
    {   if ((err = SSLReallocBuffer(ctx->partialReadBuffer, 5 + contentLen, ctx)) != 0)
        {   SSLFatalSessionAlert(SSL_AlertInternalError, ctx);
            return err;
        }
    }
    
    if (ctx->amountRead < 5 + contentLen)
    {   readData.length = 5 + contentLen - ctx->amountRead;
        readData.data = ctx->partialReadBuffer.data + ctx->amountRead;
        len = readData.length;
        err = sslIoRead(readData, &len, ctx);
        if(err != 0)
        {   if (err == errSSLWouldBlock)
                ctx->amountRead += len;
            else
                SSLFatalSessionAlert(SSL_AlertCloseNotify, ctx);
            return err;
        }
        ctx->amountRead += len;
    }
    
    assert(ctx->amountRead >= 5 + contentLen);
    
    cipherFragment.data = ctx->partialReadBuffer.data + 5;
    cipherFragment.length = contentLen;
    
	/* 
	 * Decrypt the payload & check the MAC, modifying the length of the 
	 * buffer to indicate the amount of plaintext data after adjusting 
	 * for the block size and removing the MAC (this function generates 
	 * its own alerts).
	 */
	assert(ctx->sslTslCalls != NULL);
    if ((err = ctx->sslTslCalls->decryptRecord(rec.contentType, 
			&cipherFragment, ctx)) != 0)
        return err;
    
	/* 
	 * We appear to have sucessfully received a record; increment the 
	 * sequence number 
	 */
    IncrementUInt64(&ctx->readCipher.sequenceNum);
    
	/* Allocate a buffer to return the plaintext in and return it */
    if ((err = SSLAllocBuffer(rec.contents, cipherFragment.length, ctx)) != 0)
    {   SSLFatalSessionAlert(SSL_AlertInternalError, ctx);
        return err;
    }
    memcpy(rec.contents.data, cipherFragment.data, cipherFragment.length);
    
    ctx->amountRead = 0;        /* We've used all the data in the cache */
    
    return noErr;
}

/* common for sslv3 and tlsv1, except for the computeMac callout */
OSStatus SSLVerifyMac(
	UInt8 type, 
	SSLBuffer &data, 
	UInt8 *compareMAC, 
	SSLContext *ctx)
{   
	OSStatus        err;
    UInt8           macData[SSL_MAX_DIGEST_LEN];
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
        return err;
    
    if ((memcmp(mac.data, compareMAC, mac.length)) != 0) {
		sslErrorLog("ssl3VerifyMac: Mac verify failure\n");
        return errSSLProtocol;
    }
    return noErr;
}



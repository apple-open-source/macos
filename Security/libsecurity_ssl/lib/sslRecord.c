/*
 * Copyright (c) 1999-2001,2005-2007,2010-2012 Apple Inc. All Rights Reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

/*
 * sslRecord.c - Encryption, decryption and MACing of data
*/

#include "ssl.h"

#include "sslRecord.h"
#include "sslMemory.h"
#include "cryptType.h"
#include "sslContext.h"
#include "sslAlertMessage.h"
#include "sslDebug.h"
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
SSLReadRecord(SSLRecord *rec, SSLContext *ctx)
{   OSStatus        err;
    size_t          len, contentLen;
    UInt8           *charPtr;
    SSLBuffer       readData, cipherFragment;
    size_t          head=5;
    int             skipit=0;

#if ENABLE_DTLS
    if(ctx->isDTLS)
        head+=8;
#endif

    if (!ctx->partialReadBuffer.data || ctx->partialReadBuffer.length < head)
    {   if (ctx->partialReadBuffer.data)
            if ((err = SSLFreeBuffer(&ctx->partialReadBuffer, ctx)) != 0)
            {   SSLFatalSessionAlert(SSL_AlertInternalError, ctx);
                return err;
            }
        if ((err = SSLAllocBuffer(&ctx->partialReadBuffer,
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
					if((ctx->protocolSide == kSSLClientSide) &&
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

    if (ctx->amountRead < head)
    {   readData.length = head - ctx->amountRead;
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

    assert(ctx->amountRead >= head);

    charPtr = ctx->partialReadBuffer.data;
    rec->contentType = *charPtr++;
    if (rec->contentType < SSL_RecordTypeV3_Smallest ||
        rec->contentType > SSL_RecordTypeV3_Largest)
        return errSSLProtocol;

    rec->protocolVersion = (SSLProtocolVersion)SSLDecodeInt(charPtr, 2);
    charPtr += 2;

#if ENABLE_DTLS
    if(rec->protocolVersion == DTLS_Version_1_0)
    {
        sslUint64 seqNum;
        SSLDecodeUInt64(charPtr, 8, &seqNum);
        charPtr += 8;
        sslLogRecordIo("Read DTLS Record %08x_%08x (seq is: %08x_%08x)",
               seqNum.high, seqNum.low,
               ctx->readCipher.sequenceNum.high,ctx->readCipher.sequenceNum.low);

        /* if the epoch of the record is different of current read cipher, just drop it */
        if((seqNum.high>>8)!=(ctx->readCipher.sequenceNum.high>>8)) {
            skipit=1;
        } else {
            ctx->readCipher.sequenceNum.high=seqNum.high;
            ctx->readCipher.sequenceNum.low=seqNum.low;
        }
    }
#endif

    contentLen = SSLDecodeInt(charPtr, 2);
    charPtr += 2;
    if (contentLen > (16384 + 2048))    /* Maximum legal length of an
										 * SSLCipherText payload */
    {   SSLFatalSessionAlert(SSL_AlertRecordOverflow, ctx);
        return errSSLProtocol;
    }

    /* Dont check this if we are going to drop an old packet:
      we dont know if the digestSize is the correct one */
	if (!skipit && contentLen < ctx->readCipher.macRef->hash->digestSize)
	{
		SSLFatalSessionAlert(SSL_AlertInternalError, ctx);
		return errSSLClosedAbort;
	}

    if (ctx->partialReadBuffer.length < head + contentLen)
    {   if ((err = SSLReallocBuffer(&ctx->partialReadBuffer, head + contentLen, ctx)) != 0)
        {   SSLFatalSessionAlert(SSL_AlertInternalError, ctx);
            return err;
        }
    }

    if (ctx->amountRead < head + contentLen)
    {   readData.length = head + contentLen - ctx->amountRead;
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

    assert(ctx->amountRead >= head + contentLen);

    cipherFragment.data = ctx->partialReadBuffer.data + head;
    cipherFragment.length = contentLen;

    ctx->amountRead = 0;        /* We've used all the data in the cache */

    /* We dont decrypt if we were told to skip this record */
    if(skipit) {
        DTLSRetransmit(ctx);
        return errSSLWouldBlock;
    }
	/*
	 * Decrypt the payload & check the MAC, modifying the length of the
	 * buffer to indicate the amount of plaintext data after adjusting
	 * for the block size and removing the MAC (this function generates
	 * its own alerts).
	 */
	assert(ctx->sslTslCalls != NULL);
    if ((err = ctx->sslTslCalls->decryptRecord(rec->contentType,
			&cipherFragment, ctx)) != 0)
        return err;

	/*
	 * We appear to have sucessfully received a record; increment the
	 * sequence number
	 */
    IncrementUInt64(&ctx->readCipher.sequenceNum);

	/* Allocate a buffer to return the plaintext in and return it */
    if ((err = SSLAllocBuffer(&rec->contents, cipherFragment.length, ctx)) != 0)
    {   SSLFatalSessionAlert(SSL_AlertInternalError, ctx);
        return err;
    }
    memcpy(rec->contents.data, cipherFragment.data, cipherFragment.length);


    return noErr;
}

/* common for sslv3 and tlsv1, except for the computeMac callout */
OSStatus SSLVerifyMac(
	UInt8 type,
	SSLBuffer *data,
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
			*data,
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



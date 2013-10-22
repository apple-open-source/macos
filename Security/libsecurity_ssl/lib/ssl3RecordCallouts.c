/*
 * Copyright (c) 2002,2005-2007,2010-2011 Apple Inc. All Rights Reserved.
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
 * ssl3RecordCallouts.c - SSLv3-specific routines for SslTlsCallouts.
 */

/* THIS FILE CONTAINS KERNEL CODE */

#include <AssertMacros.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>

#ifdef KERNEL
#include <sys/types.h>
#else
#include <stddef.h>
#endif

#include "sslDebug.h"
#include "sslMemory.h"
#include "sslUtils.h"
#include "sslRand.h"

#include "tls_record.h"


/*
 * ssl3WriteRecord does not send alerts on failure, out of the assumption/fear
 * that this might result in a loop (since sending an alert causes ssl3WriteRecord
 * to be called).
 *
 * As far as I can tell, we can use this same routine for SSLv3 and TLSv1, as long
 * as we're not trying to use the "variable length padding" feature of TLSv1.
 * OpenSSL doesn't use that feature; for now, neither will we. Thus this routine
 * is used for the SslTlsCallouts.writeRecord function for both protocols.
 */
int ssl3WriteRecord(
	SSLRecord rec,
	struct SSLRecordInternalContext *ctx)
{
	int        err;
    int             padding = 0, i;
    WaitingRecord   *out = NULL, *queue;
    SSLBuffer       payload, mac;
    uint8_t           *charPtr;
    uint16_t          payloadSize,blockSize = 0;
    int             head = 5;

	switch(rec.protocolVersion) {
        case DTLS_Version_1_0:
            head += 8;
		case SSL_Version_3_0:
		case TLS_Version_1_0:
        case TLS_Version_1_1:
        case TLS_Version_1_2:
			break;
		default:
			check(0);
			return errSSLRecordInternal;
	}
    check(rec.contents.length <= 16384);

    sslLogRecordIo("type = %02x, ver = %04x, len = %ld, seq = %016llx",
                   rec.contentType, rec.protocolVersion, rec.contents.length,
                   ctx->writeCipher.sequenceNum);

    /* Allocate enough room for the transmitted record, which will be:
     *  5 bytes of header (13 for DTLS) +
     *  IV [block cipher and TLS1.1 or DTLS 1.0 only]
     *  encrypted contents +
     *  macLength +
     *  padding [block ciphers only] +
     *  padding length field (1 byte) [block ciphers only]
     */
    payloadSize = (uint16_t) rec.contents.length;
    CipherType cipherType = ctx->writeCipher.symCipher->params->cipherType;
    const Cipher *cipher = &ctx->writeCipher.symCipher->c.cipher;
    const AEADCipher *aead = &ctx->writeCipher.symCipher->c.aead;
    blockSize = ctx->writeCipher.symCipher->params->blockSize;
    switch (cipherType) {
        case blockCipherType:
            payloadSize += ctx->writeCipher.macRef->hash->digestSize;
            padding = blockSize - (payloadSize % blockSize) - 1;
            payloadSize += padding + 1;
            /* TLS 1.1, TLS1.2 and DTLS 1.0 have an extra block for IV */
            if(ctx->negProtocolVersion >= TLS_Version_1_1) {
                payloadSize += blockSize;
            }
            break;
        case streamCipherType:
            payloadSize += ctx->writeCipher.macRef->hash->digestSize;
            break;
        case aeadCipherType:
            /* AES_GCM doesn't need padding. */
            payloadSize += aead->macSize;
            break;
        default:
            check(0);
			return errSSLRecordInternal;
    }

	out = (WaitingRecord *)sslMalloc(offsetof(WaitingRecord, data) +
		head + payloadSize);
	out->next = NULL;
	out->sent = 0;
	out->length = head + payloadSize;

    charPtr = out->data;
    *(charPtr++) = rec.contentType;
    charPtr = SSLEncodeInt(charPtr, rec.protocolVersion, 2);

    /* DTLS sequence number */
    if(rec.protocolVersion == DTLS_Version_1_0)
        charPtr = SSLEncodeUInt64(charPtr,ctx->writeCipher.sequenceNum);

    charPtr = SSLEncodeInt(charPtr, payloadSize, 2);

    /* Also for DTLS */
    if((ctx->negProtocolVersion >= TLS_Version_1_1) &&
       (cipherType == blockCipherType))
    {
        SSLBuffer randomIV;
        randomIV.data = charPtr;
        randomIV.length = blockSize;
        if((err = sslRand(&randomIV)) != 0)
            return err;
        charPtr += blockSize;
    }
    if (cipherType == aeadCipherType) {
        /* Encode the explicit iv, for AES_GCM we just use the 8 byte
           sequenceNum as the explicitIV.
           Ideally this needs to be done in the algorithm itself, by an
           extra function pointer in AEADCipher.  */
        charPtr = SSLEncodeUInt64(charPtr,ctx->writeCipher.sequenceNum);
        /* TODO: If we ever add any mode other than GCM this code might have
           to be different. */
        /* TODO: Pass 4 byte implicit and 8 byte explicit IV to cipher */
        //err = ctx->writeCipher.symCipher->c.aead.setIV(charPtr, &ctx->writeCipher, ctx);
    }

    /* Copy the contents into the output buffer */
    memcpy(charPtr, rec.contents.data, rec.contents.length);
    payload.data = charPtr;
    payload.length = rec.contents.length;

    charPtr += rec.contents.length;

    /* MAC the data */
    if (cipherType != aeadCipherType) {
        /* MAC immediately follows data */
        mac.data = charPtr;
        mac.length = ctx->writeCipher.macRef->hash->digestSize;
        charPtr += mac.length;
        if (mac.length > 0)     /* Optimize away null case */
        {
            check(ctx->sslTslCalls != NULL);
            if ((err = ctx->sslTslCalls->computeMac(rec.contentType,
                    payload,
                    mac,
                    &ctx->writeCipher,
                    ctx->writeCipher.sequenceNum,
                    ctx)) != 0)
                goto fail;
        }
    }

    /* For TLS 1.1 and DTLS, we would need to specifiy the IV, but instead
     we are clever like this: since the IV is just one block in front,
     we encrypt it with the rest of the data. The actual transmitted IV
     is the result of the encryption, with whatever internal IV is used.
     This method is explained in the TLS 1.1 RFC */
    if(ctx->negProtocolVersion >= TLS_Version_1_1 &&
       cipherType == blockCipherType)
    {
            payload.data -= blockSize;
    }

    /* Update payload to reflect encrypted data: IV, contents, mac & padding */
    payload.length = payloadSize;


    switch (cipherType) {
        case blockCipherType:
            /* Fill in the padding bytes & padding length field with the
             * padding value; the protocol only requires the last byte,
             * but filling them all in avoids leaking data */
            for (i = 1; i <= padding + 1; ++i)
                payload.data[payload.length - i] = padding;
            /* DROPTRHOUGH */
        case streamCipherType:
            /* Encrypt the data */
            if ((err = cipher->encrypt(payload.data,
                payload.data, payload.length, ctx->writeCipher.cipherCtx)) != 0)
                goto fail;
            break;
        case aeadCipherType:
            check(0);
            break;
        default:
            check(0);
			return errSSLRecordInternal;
    }

    /* Enqueue the record to be written from the idle loop */
    if (ctx->recordWriteQueue == 0)
        ctx->recordWriteQueue = out;
    else
    {   queue = ctx->recordWriteQueue;
        while (queue->next != 0)
            queue = queue->next;
        queue->next = out;
    }

    /* Increment the sequence number */
    IncrementUInt64(&ctx->writeCipher.sequenceNum);

    return 0;

fail:
	/*
	 * Only for if we fail between when the WaitingRecord is allocated and when
	 * it is queued
	 */
	sslFree(out);
    return err;
}

static int ssl3DecryptRecord(
	uint8_t type,
	SSLBuffer *payload,
	struct SSLRecordInternalContext *ctx)
{
	int    err;
    SSLBuffer   content;

    CipherType cipherType = ctx->readCipher.symCipher->params->cipherType;
    const Cipher *c = &ctx->readCipher.symCipher->c.cipher;
    switch (cipherType) {
        case blockCipherType:
            if ((payload->length % ctx->readCipher.symCipher->params->blockSize) != 0)
            {
                return errSSLRecordDecryptionFail;
            }
            /* DROPTHROUGH */
        case streamCipherType:
            /* Decrypt in place */
            err = c->decrypt(payload->data, payload->data,
                             payload->length, ctx->readCipher.cipherCtx);
            break;
        case aeadCipherType:
        default:
            check(0);
			return errSSLRecordInternal;
    }
    if (err != 0)
    {
        return errSSLRecordDecryptionFail;
    }

	/* Locate content within decrypted payload */
    content.data = payload->data;
    content.length = payload->length - ctx->readCipher.macRef->hash->digestSize;
    if (cipherType == blockCipherType)
    {   /* padding can't be equal to or more than a block */
        if (payload->data[payload->length - 1] >= ctx->readCipher.symCipher->params->blockSize)
        {
		sslErrorLog("DecryptSSLRecord: bad padding length (%d)\n",
			(unsigned)payload->data[payload->length - 1]);
            return errSSLRecordDecryptionFail;
        }
        content.length -= 1 + payload->data[payload->length - 1];
						/* Remove block size padding */
    }

	/* Verify MAC on payload */
    if (ctx->readCipher.macRef->hash->digestSize > 0)
		/* Optimize away MAC for null case */
        if ((err = SSLVerifyMac(type, &content,
				payload->data + content.length, ctx)) != 0)
        {
            return errSSLRecordBadRecordMac;
        }

    *payload = content;     /* Modify payload buffer to indicate content length */

    return 0;
}

/* initialize a per-CipherContext HashHmacContext for use in MACing each record */
static int ssl3InitMac (
	CipherContext *cipherCtx)		// macRef, macSecret valid on entry
									// macCtx valid on return
{
	const HashReference *hash;
	SSLBuffer *hashCtx;
	int serr;

	check(cipherCtx->macRef != NULL);
	hash = cipherCtx->macRef->hash;
	check(hash != NULL);

	hashCtx = &cipherCtx->macCtx.hashCtx;
	if(hashCtx->data != NULL) {
		SSLFreeBuffer(hashCtx);
	}
	serr = SSLAllocBuffer(hashCtx, hash->contextSize);
	if(serr) {
		return serr;
	}
	return 0;
}

static int ssl3FreeMac (
	CipherContext *cipherCtx)
{
	SSLBuffer *hashCtx;

	check(cipherCtx != NULL);
	/* this can be called on a completely zeroed out CipherContext... */
	if(cipherCtx->macRef == NULL) {
		return 0;
	}
	hashCtx = &cipherCtx->macCtx.hashCtx;
	if(hashCtx->data != NULL) {
		sslFree(hashCtx->data);
		hashCtx->data = NULL;
	}
	hashCtx->length = 0;
	return 0;
}

static int ssl3ComputeMac (
	uint8_t type,
	SSLBuffer data,
	SSLBuffer mac, 					// caller mallocs data
	CipherContext *cipherCtx,		// assumes macCtx, macRef
	sslUint64 seqNo,
	struct SSLRecordInternalContext *ctx)
{
	int        err;
    uint8_t           innerDigestData[SSL_MAX_DIGEST_LEN];
    uint8_t           scratchData[11], *charPtr;
    SSLBuffer       digest, digestCtx, scratch;
	SSLBuffer		secret;

    const HashReference	*hash;

	check(cipherCtx != NULL);
	check(cipherCtx->macRef != NULL);
	hash = cipherCtx->macRef->hash;
	check(hash != NULL);
    check(hash->macPadSize <= MAX_MAC_PADDING);
    check(hash->digestSize <= SSL_MAX_DIGEST_LEN);
	digestCtx = cipherCtx->macCtx.hashCtx;		// may be NULL, for null cipher
	secret.data = cipherCtx->macSecret;
	secret.length = hash->digestSize;

	/* init'd early in SSLNewContext() */
    check(SSLMACPad1[0] == 0x36 && SSLMACPad2[0] == 0x5C);

	/*
	 * MAC = hash( MAC_write_secret + pad_2 +
	 *		       hash( MAC_write_secret + pad_1 + seq_num + type +
	 *			         length + content )
	 *			 )
	 */
    if ((err = hash->init(&digestCtx)) != 0)
        goto exit;
    if ((err = hash->update(&digestCtx, &secret)) != 0)    /* MAC secret */
        goto exit;
    scratch.data = (uint8_t *)SSLMACPad1;
    scratch.length = hash->macPadSize;
    if ((err = hash->update(&digestCtx, &scratch)) != 0)   /* pad1 */
        goto exit;
    charPtr = scratchData;
    charPtr = SSLEncodeUInt64(charPtr, seqNo);
    *charPtr++ = type;
    charPtr = SSLEncodeSize(charPtr, data.length, 2);
    scratch.data = scratchData;
    scratch.length = 11;
    check(charPtr = scratchData+11);
    if ((err = hash->update(&digestCtx, &scratch)) != 0)
										/* sequenceNo, type & length */
        goto exit;
    if ((err = hash->update(&digestCtx, &data)) != 0)      /* content */
        goto exit;
    digest.data = innerDigestData;
    digest.length = hash->digestSize;
    if ((err = hash->final(&digestCtx, &digest)) != 0) 	/* figure inner digest */
        goto exit;

    if ((err = hash->init(&digestCtx)) != 0)
        goto exit;
    if ((err = hash->update(&digestCtx, &secret)) != 0)    /* MAC secret */
        goto exit;
    scratch.data = (uint8_t *)SSLMACPad2;
    scratch.length = hash->macPadSize;
    if ((err = hash->update(&digestCtx, &scratch)) != 0)   /* pad2 */
        goto exit;
    if ((err = hash->update(&digestCtx, &digest)) != 0)    /* inner digest */
        goto exit;
    if ((err = hash->final(&digestCtx, &mac)) != 0)   	 /* figure the mac */
        goto exit;

    err = 0; /* redundant, I know */

exit:
    return err;
}


const SslRecordCallouts Ssl3RecordCallouts = {
	ssl3DecryptRecord,
	ssl3WriteRecord,
	ssl3InitMac,
	ssl3FreeMac,
	ssl3ComputeMac,
};

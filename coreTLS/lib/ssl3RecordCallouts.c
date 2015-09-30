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

#include "tls_record_internal.h"

static int ssl3DecryptRecord(
	uint8_t type,
	tls_buffer *payload,
	tls_record_t ctx)
{
	int    err;
    tls_buffer   content;

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
	tls_buffer *hashCtx;
	int serr;

	hash = cipherCtx->macRef->hash;
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
	tls_buffer *hashCtx;

	//check(cipherCtx != NULL);
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
	tls_buffer data,
	tls_buffer mac, 					// caller mallocs data
	CipherContext *cipherCtx,		// assumes macCtx, macRef
	uint64_t seqNo,
	tls_record_t ctx)
{
	int        err;
    uint8_t           innerDigestData[SSL_MAX_DIGEST_LEN];
    uint8_t           scratchData[11], *charPtr;
    tls_buffer       digest, digestCtx, scratch;
	tls_buffer		secret;

    const HashReference	*hash;

	//check(cipherCtx != NULL);
	//check(cipherCtx->macRef != NULL);
	hash = cipherCtx->macRef->hash;
	//check(hash != NULL);
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
    SSLEncodeSize(charPtr, data.length, 2);
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
	ssl3InitMac,
	ssl3FreeMac,
	ssl3ComputeMac,
};

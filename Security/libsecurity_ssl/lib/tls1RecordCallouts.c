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
 * tls1RecordCallouts.c - TLSv1-specific routines for SslTlsCallouts.
 */

/* THIS FILE CONTAINS KERNEL CODE */

#include "tls_record.h"
#include "sslMemory.h"
#include "sslDebug.h"
#include "sslUtils.h"

#include <AssertMacros.h>
#include <string.h>

/* not needed; encrypt/encode is the same for both protocols as long as
 * we don't use the "variable length padding" feature. */
#if 0
static int tls1WriteRecord(
	SSLRecord rec,
	SSLContext *ctx)
{
	check(0);
	return errSecUnimplemented;
}
#endif

static int tls1DecryptRecord(
	uint8_t type,
	SSLBuffer *payload,
	struct SSLRecordInternalContext *ctx)
{
	int    err;
    SSLBuffer   content;

    if ((ctx->readCipher.symCipher->params->blockSize > 0) &&
        ((payload->length % ctx->readCipher.symCipher->params->blockSize) != 0)) {
        return errSSLRecordRecordOverflow;
    }

    /* Decrypt in place */
    if ((err = ctx->readCipher.symCipher->c.cipher.decrypt(payload->data,
		payload->data, payload->length,
		ctx->readCipher.cipherCtx)) != 0)
    {
        return errSSLRecordDecryptionFail;
    }

    /* Locate content within decrypted payload */

    /* TLS 1.1 and DTLS 1.0 block ciphers */
    if((ctx->negProtocolVersion>=TLS_Version_1_1) && (ctx->readCipher.symCipher->params->blockSize>0))
    {
        content.data = payload->data + ctx->readCipher.symCipher->params->blockSize;
        content.length = payload->length - (ctx->readCipher.macRef->hash->digestSize + ctx->readCipher.symCipher->params->blockSize);
    } else {
        content.data = payload->data;
        content.length = payload->length - ctx->readCipher.macRef->hash->digestSize;
    }

    /* Test for underflow - if the record size is smaller than required */
    if(content.length > payload->length) {
            return errSSLRecordClosedAbort;
    }

    err = 0;

    if (ctx->readCipher.symCipher->params->blockSize > 0) {
		/* for TLSv1, padding can be anywhere from 0 to 255 bytes */
		uint8_t padSize = payload->data[payload->length - 1];

        /* Padding check sequence:
            1. Check that the padding size (last byte in padding) is within bound
            2. Adjust content.length accordingly
            3. Check every padding byte (except last, already checked)
           Do not return, just set the error value on failure,
           to avoid creating a padding oracle with timing.
        */
        if(padSize+1<=content.length) {
            uint8_t *padChars;
            content.length -= (1 + padSize);
            padChars = payload->data + payload->length - (padSize+1);
            while(padChars < (payload->data + payload->length - 1)) {
                if(*padChars++ != padSize) {
                    err = errSSLRecordBadRecordMac;
                }
            }
        } else {
            err = errSSLRecordBadRecordMac;
        }
    }

	/* Verify MAC on payload */
    if (ctx->readCipher.macRef->hash->digestSize > 0)
    /* Optimize away MAC for null case */
        if (SSLVerifyMac(type, &content,
                          content.data + content.length, ctx) != 0)
        {
            err = errSSLRecordBadRecordMac;
        }

    *payload = content;     /* Modify payload buffer to indicate content length */

    return err;
}

/* initialize a per-CipherContext HashHmacContext for use in MACing each record */
static int tls1InitMac (
	CipherContext *cipherCtx)		// macRef, macSecret valid on entry
									// macCtx valid on return
{
	const HMACReference *hmac;
	int serr;

    check(cipherCtx);
	check(cipherCtx->macRef != NULL);
	hmac = cipherCtx->macRef->hmac;
	check(hmac != NULL);

	if(cipherCtx->macCtx.hmacCtx != NULL) {
		hmac->free(cipherCtx->macCtx.hmacCtx);
		cipherCtx->macCtx.hmacCtx = NULL;
	}
	serr = hmac->alloc(hmac, cipherCtx->macSecret,
		cipherCtx->macRef->hmac->macSize, &cipherCtx->macCtx.hmacCtx);

	/* mac secret now stored in macCtx.hmacCtx, delete it from cipherCtx */
	memset(cipherCtx->macSecret, 0, sizeof(cipherCtx->macSecret));
	return serr;
}

static int tls1FreeMac (
	CipherContext *cipherCtx)
{
	/* this can be called on a completely zeroed out CipherContext... */
	if(cipherCtx->macRef == NULL) {
		return 0;
	}
	check(cipherCtx->macRef->hmac != NULL);

	if(cipherCtx->macCtx.hmacCtx != NULL) {
		cipherCtx->macRef->hmac->free(cipherCtx->macCtx.hmacCtx);
		cipherCtx->macCtx.hmacCtx = NULL;
	}
	return 0;
}

/*
 * mac = HMAC_hash(MAC_write_secret, seq_num + TLSCompressed.type +
 *					TLSCompressed.version + TLSCompressed.length +
 *					TLSCompressed.fragment));
 */

/* sequence, type, version, length */
#define HDR_LENGTH (8 + 1 + 2 + 2)
static int tls1ComputeMac (
	uint8_t type,
	SSLBuffer data,
	SSLBuffer mac, 					// caller mallocs data
	CipherContext *cipherCtx,		// assumes macCtx, macRef
	sslUint64 seqNo,
	struct SSLRecordInternalContext *ctx)
{
	uint8_t hdr[HDR_LENGTH];
	uint8_t *p;
	HMACContextRef hmacCtx;
	int serr;
	const HMACReference *hmac;
	size_t macLength;

	check(cipherCtx != NULL);
	check(cipherCtx->macRef != NULL);
	hmac = cipherCtx->macRef->hmac;
	check(hmac != NULL);
	hmacCtx = cipherCtx->macCtx.hmacCtx;	// may be NULL, for null cipher

	serr = hmac->init(hmacCtx);
	if(serr) {
		goto fail;
	}
	p = SSLEncodeUInt64(hdr, seqNo);
	*p++ = type;
	*p++ = ctx->negProtocolVersion >> 8;
	*p++ = ctx->negProtocolVersion & 0xff;
	*p++ = data.length >> 8;
	*p   = data.length & 0xff;
	serr = hmac->update(hmacCtx, hdr, HDR_LENGTH);
	if(serr) {
		goto fail;
	}
	serr = hmac->update(hmacCtx, data.data, data.length);
	if(serr) {
		goto fail;
	}
	macLength = mac.length;
	serr = hmac->final(hmacCtx, mac.data, &macLength);
	if(serr) {
		goto fail;
	}
	mac.length = macLength;
fail:
	return serr;
}

const SslRecordCallouts Tls1RecordCallouts = {
	tls1DecryptRecord,
	ssl3WriteRecord,
	tls1InitMac,
	tls1FreeMac,
	tls1ComputeMac,
};


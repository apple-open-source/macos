/*
 * Copyright (c) 2011-2014 Apple Inc. All Rights Reserved.
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


/* THIS FILE CONTAINS KERNEL CODE */

#include "sslBuildFlags.h"
#include "SSLRecordInternal.h"
#include "sslDebug.h"
#include "cipherSpecs.h"
#include "symCipher.h"
#include "sslUtils.h"
#include "tls_record_internal.h"

#include <AssertMacros.h>
#include <string.h>

#include <inttypes.h>
#include <stddef.h>

/* Maximum encrypted record size, defined in TLS 1.2 RFC, section 6.2.3 */
#define DEFAULT_BUFFER_SIZE (16384 + 2048)


/*
 * Redirect SSLBuffer-based I/O call to user-supplied I/O.
 */
static
int sslIoRead(SSLBuffer                        buf,
              size_t                           *actualLength,
              struct SSLRecordInternalContext  *ctx)
{
	size_t  dataLength = buf.length;
	int     ortn;

	*actualLength = 0;
	ortn = (ctx->read)(ctx->ioRef,
                       buf.data,
                       &dataLength);
	*actualLength = dataLength;

    sslLogRecordIo("sslIoRead: [%p] req %4lu actual %4lu status %d",
                   ctx, buf.length, dataLength, (int)ortn);

	return ortn;
}

static
int sslIoWrite(SSLBuffer                       buf,
               size_t                          *actualLength,
               struct SSLRecordInternalContext *ctx)
{
	size_t  dataLength = buf.length;
	int     ortn;

	*actualLength = 0;
	ortn = (ctx->write)(ctx->ioRef,
                        buf.data,
                        &dataLength);
	*actualLength = dataLength;

    sslLogRecordIo("sslIoWrite: [%p] req %4lu actual %4lu status %d",
                   ctx, buf.length, dataLength, (int)ortn);

	return ortn;
}

/* Entry points to Record Layer */

static int SSLRecordReadInternal(SSLRecordContextRef ref, SSLRecord *rec)
{
    struct SSLRecordInternalContext *ctx = ref;

    int     err;
    size_t  len, contentLen;
    SSLBuffer readData;

    size_t head=tls_record_get_header_size(ctx->filter);

    if (ctx->amountRead < head)
    {
        readData.length = head - ctx->amountRead;
        readData.data = ctx->partialReadBuffer.data + ctx->amountRead;
        len = readData.length;
        err = sslIoRead(readData, &len, ctx);
        if(err != 0)
        {
            switch(err) {
                case errSSLRecordWouldBlock:
                    ctx->amountRead += len;
                    break;
                default:
                    /* Any other error but errSSLWouldBlock is  translated to errSSLRecordClosedAbort */
                    err = errSSLRecordClosedAbort;
                    break;
            }
            return err;
        }
        ctx->amountRead += len;

        check(ctx->amountRead == head);
    }


    tls_buffer header;
    header.data=ctx->partialReadBuffer.data;
    header.length=head;

    uint8_t content_type;

    tls_record_parse_header(ctx->filter, header, &contentLen, &content_type);

    if(content_type&0x80) {
        // Looks like SSL2 record, reset expectations.
        head = 2;
        err=tls_record_parse_ssl2_header(ctx->filter, header, &contentLen, &content_type);
        if(err!=0) return errSSLRecordUnexpectedRecord;
    }

    check(ctx->partialReadBuffer.length>=head+contentLen);

    if(head+contentLen>ctx->partialReadBuffer.length)
        return errSSLRecordRecordOverflow;

    if (ctx->amountRead < head + contentLen)
    {   readData.length = head + contentLen - ctx->amountRead;
        readData.data = ctx->partialReadBuffer.data + ctx->amountRead;
        len = readData.length;
        err = sslIoRead(readData, &len, ctx);
        if(err != 0)
        {   if (err == errSSLRecordWouldBlock)
            ctx->amountRead += len;
            return err;
        }
        ctx->amountRead += len;
    }

    check(ctx->amountRead == head + contentLen);

    tls_buffer record;
    record.data = ctx->partialReadBuffer.data;
    record.length = ctx->amountRead;

    rec->contentType = content_type;

    ctx->amountRead = 0;        /* We've used all the data in the cache */

    if(content_type==tls_record_type_SSL2) {
        /* Just copy the SSL2 record, dont decrypt since this is only for SSL2 Client Hello */
        return SSLCopyBuffer(&record, &rec->contents);
    } else {
        size_t sz = tls_record_decrypted_size(ctx->filter, record.length);

        /* There was an underflow - For TLS, we return errSSLRecordClosedAbort for historical reason - see ssl-44-crashes test */
        if(sz==0) {
            sslErrorLog("underflow in SSLReadRecordInternal");
            if(ctx->dtls) {
                // For DTLS, we should just drop it.
                return errSSLRecordUnexpectedRecord;
            } else {
                // For TLS, we are going to close the connection.
                return errSSLRecordClosedAbort;
            }
        }

        /* Allocate a buffer for the plaintext */
        if ((err = SSLAllocBuffer(&rec->contents, sz)))
        {
            return err;
        }

        return tls_record_decrypt(ctx->filter, record, &rec->contents, NULL);
    }
}

static int SSLRecordWriteInternal(SSLRecordContextRef ref, SSLRecord rec)
{
    int err;
    struct SSLRecordInternalContext *ctx = ref;
    WaitingRecord *queue, *out;
    tls_buffer data;
    tls_buffer content;
    size_t len;

    err = errSSLRecordInternal; /* FIXME: allocation error */
    len=tls_record_encrypted_size(ctx->filter, rec.contentType, rec.contents.length);

    require((out = (WaitingRecord *)sslMalloc(offsetof(WaitingRecord, data) + len)), fail);
    out->next = NULL;
	out->sent = 0;
	out->length = len;

    data.data=&out->data[0];
    data.length=out->length;

    content.data = rec.contents.data;
    content.length = rec.contents.length;

    require_noerr((err=tls_record_encrypt(ctx->filter, content, rec.contentType, &data)), fail);

    out->length = data.length; // This should not be needed if tls_record_encrypted_size works properly.

    /* Enqueue the record to be written from the idle loop */
    if (ctx->recordWriteQueue == 0)
        ctx->recordWriteQueue = out;
    else
    {   queue = ctx->recordWriteQueue;
        while (queue->next != 0)
            queue = queue->next;
        queue->next = out;
    }

    return 0;
fail:
    if(out)
        sslFree(out);
    return err;
}

/* Record Layer Entry Points */

static int
SSLRollbackInternalRecordLayerWriteCipher(SSLRecordContextRef ref)
{
    struct SSLRecordInternalContext *ctx = ref;
    return tls_record_rollback_write_cipher(ctx->filter);
}

static int
SSLAdvanceInternalRecordLayerWriteCipher(SSLRecordContextRef ref)
{
    struct SSLRecordInternalContext *ctx = ref;
    return tls_record_advance_write_cipher(ctx->filter);
}

static int
SSLAdvanceInternalRecordLayerReadCipher(SSLRecordContextRef ref)
{
    struct SSLRecordInternalContext *ctx = ref;
    return tls_record_advance_read_cipher(ctx->filter);
}

static int
SSLInitInternalRecordLayerPendingCiphers(SSLRecordContextRef ref, uint16_t selectedCipher, bool isServer, SSLBuffer key)
{
    struct SSLRecordInternalContext *ctx = ref;
    return tls_record_init_pending_ciphers(ctx->filter, selectedCipher, isServer, key);
}

static int
SSLSetInternalRecordLayerProtocolVersion(SSLRecordContextRef ref, SSLProtocolVersion negVersion)
{
    struct SSLRecordInternalContext *ctx = ref;
    return tls_record_set_protocol_version(ctx->filter, negVersion);
}

static int
SSLRecordFreeInternal(SSLRecordContextRef ref, SSLRecord rec)
{
    return SSLFreeBuffer(&rec.contents);
}

static int
SSLRecordServiceWriteQueueInternal(SSLRecordContextRef ref)
{
    int             err = 0, werr = 0;
    size_t          written = 0;
    SSLBuffer       buf;
    WaitingRecord   *rec;
    struct SSLRecordInternalContext *ctx= ref;

    while (!werr && ((rec = ctx->recordWriteQueue) != 0))
    {   buf.data = rec->data + rec->sent;
        buf.length = rec->length - rec->sent;
        werr = sslIoWrite(buf, &written, ctx);
        rec->sent += written;
        if (rec->sent >= rec->length)
        {
            check(rec->sent == rec->length);
            check(err == 0);
            ctx->recordWriteQueue = rec->next;
			sslFree(rec);
        }
        if (err) {
            check_noerr(err);
            return err;
        }
    }

    return werr;
}

static int
SSLRecordSetOption(SSLRecordContextRef ref, SSLRecordOption option, bool value)
{
    struct SSLRecordInternalContext *ctx = (struct SSLRecordInternalContext *)ref;
    switch (option) {
        case kSSLRecordOptionSendOneByteRecord:
            return tls_record_set_record_splitting(ctx->filter, value);
            break;
        default:
            return 0;
            break;
    }
}

/***** Internal Record Layer APIs *****/

#include <CommonCrypto/CommonRandomSPI.h>
#define CCRNGSTATE ccDRBGGetRngState()

SSLRecordContextRef
SSLCreateInternalRecordLayer(bool dtls)
{
    struct SSLRecordInternalContext *ctx;

    ctx = sslMalloc(sizeof(struct SSLRecordInternalContext));
    if(ctx==NULL)
        return NULL;

    memset(ctx, 0, sizeof(struct SSLRecordInternalContext));

    ctx->dtls = dtls;
    require((ctx->filter=tls_record_create(dtls, CCRNGSTATE)), fail);
    require_noerr(SSLAllocBuffer(&ctx->partialReadBuffer,
                                 DEFAULT_BUFFER_SIZE), fail);

    return ctx;

fail:
    if(ctx->filter)
        tls_record_destroy(ctx->filter);
    sslFree(ctx);
    return NULL;
}

int
SSLSetInternalRecordLayerIOFuncs(
                                 SSLRecordContextRef ref,
                                 SSLIOReadFunc    readFunc,
                                 SSLIOWriteFunc   writeFunc)
{
    struct SSLRecordInternalContext *ctx = ref;

    ctx->read = readFunc;
    ctx->write = writeFunc;

    return 0;
}

int
SSLSetInternalRecordLayerConnection(
                                    SSLRecordContextRef ref,
                                    SSLIOConnectionRef ioRef)
{
    struct SSLRecordInternalContext *ctx = ref;

    ctx->ioRef = ioRef;

    return 0;
}

void
SSLDestroyInternalRecordLayer(SSLRecordContextRef ref)
{
    struct SSLRecordInternalContext *ctx = ref;
	WaitingRecord   *waitRecord, *next;

    /* RecordContext cleanup : */
    SSLFreeBuffer(&ctx->partialReadBuffer);
    waitRecord = ctx->recordWriteQueue;
    while (waitRecord)
    {   next = waitRecord->next;
        sslFree(waitRecord);
        waitRecord = next;
    }

    if(ctx->filter)
        tls_record_destroy(ctx->filter);

    sslFree(ctx);

}

struct SSLRecordFuncs SSLRecordLayerInternal =
{
    .read  = SSLRecordReadInternal,
    .write = SSLRecordWriteInternal,
    .initPendingCiphers = SSLInitInternalRecordLayerPendingCiphers,
    .advanceWriteCipher = SSLAdvanceInternalRecordLayerWriteCipher,
    .advanceReadCipher = SSLAdvanceInternalRecordLayerReadCipher,
    .rollbackWriteCipher = SSLRollbackInternalRecordLayerWriteCipher,
    .setProtocolVersion = SSLSetInternalRecordLayerProtocolVersion,
    .free = SSLRecordFreeInternal,
    .serviceWriteQueue = SSLRecordServiceWriteQueueInternal,
    .setOption = SSLRecordSetOption,
};


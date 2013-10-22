//
//  SSLRecordInternal.c
//  Security
//
//  Created by Fabrice Gautier on 10/25/11.
//  Copyright (c) 2011 Apple, Inc. All rights reserved.
//

/* THIS FILE CONTAINS KERNEL CODE */

#include "sslBuildFlags.h"
#include "SSLRecordInternal.h"
#include "sslDebug.h"
#include "cipherSpecs.h"
#include "symCipher.h"
#include "sslUtils.h"
#include "tls_record.h"

#include <AssertMacros.h>
#include <string.h>

#include <inttypes.h>

#define DEFAULT_BUFFER_SIZE 4096


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
	return ortn;
}


static int
SSLDisposeCipherSuite(CipherContext *cipher, struct SSLRecordInternalContext *ctx)
{   int      err;

	/* symmetric encryption context */
	if(cipher->symCipher) {
		if ((err = cipher->symCipher->finish(cipher->cipherCtx)) != 0) {
			return err;
		}
	}

	/* per-record hash/hmac context */
	ctx->sslTslCalls->freeMac(cipher);

    return 0;
}



/* common for sslv3 and tlsv1, except for the computeMac callout */
int SSLVerifyMac(uint8_t type,
                 SSLBuffer *data,
                 uint8_t *compareMAC,
                 struct SSLRecordInternalContext *ctx)
{
	int        err;
    uint8_t           macData[SSL_MAX_DIGEST_LEN];
    SSLBuffer       secret, mac;

    secret.data = ctx->readCipher.macSecret;
    secret.length = ctx->readCipher.macRef->hash->digestSize;
    mac.data = macData;
    mac.length = ctx->readCipher.macRef->hash->digestSize;

	check(ctx->sslTslCalls != NULL);
    if ((err = ctx->sslTslCalls->computeMac(type,
                                            *data,
                                            mac,
                                            &ctx->readCipher,
                                            ctx->readCipher.sequenceNum,
                                            ctx)) != 0)
        return err;

    if ((memcmp(mac.data, compareMAC, mac.length)) != 0) {
		sslErrorLog("SSLVerifyMac: Mac verify failure\n");
        return errSSLRecordProtocol;
    }
    return 0;
}

#include "cipherSpecs.h"
#include "symCipher.h"

static const HashHmacReference *sslCipherSuiteGetHashHmacReference(uint16_t selectedCipher)
{
    HMAC_Algs alg = sslCipherSuiteGetMacAlgorithm(selectedCipher);

    switch (alg) {
        case HA_Null:
            return &HashHmacNull;
        case HA_MD5:
            return &HashHmacMD5;
        case HA_SHA1:
            return &HashHmacSHA1;
        case HA_SHA256:
            return &HashHmacSHA256;
        case HA_SHA384:
            return &HashHmacSHA384;
        default:
            sslErrorLog("Invalid hashAlgorithm %d", alg);
            check(0);
            return &HashHmacNull;
    }
}

static const SSLSymmetricCipher *sslCipherSuiteGetSymmetricCipher(uint16_t selectedCipher)
{

    SSL_CipherAlgorithm alg = sslCipherSuiteGetSymmetricCipherAlgorithm(selectedCipher);
    switch(alg) {
        case SSL_CipherAlgorithmNull:
            return &SSLCipherNull;
#if ENABLE_RC2
        case SSL_CipherAlgorithmRC2_128:
            return &SSLCipherRC2_128;
#endif
#if ENABLE_RC4
        case SSL_CipherAlgorithmRC4_128:
            return &SSLCipherRC4_128;
#endif
#if ENABLE_DES
        case SSL_CipherAlgorithmDES_CBC:
            return &SSLCipherDES_CBC;
#endif
        case SSL_CipherAlgorithm3DES_CBC:
            return &SSLCipher3DES_CBC;
        case SSL_CipherAlgorithmAES_128_CBC:
            return &SSLCipherAES_128_CBC;
        case SSL_CipherAlgorithmAES_256_CBC:
            return &SSLCipherAES_256_CBC;
#if ENABLE_AES_GCM
        case SSL_CipherAlgorithmAES_128_GCM:
            return &SSLCipherAES_128_GCM;
        case SSL_CipherAlgorithmAES_256_GCM:
            return &SSLCipherAES_256_GCM;
#endif
        default:
            check(0);
            return &SSLCipherNull;
    }
}

static void InitCipherSpec(struct SSLRecordInternalContext *ctx, uint16_t selectedCipher)
{
    SSLRecordCipherSpec *dst = &ctx->selectedCipherSpec;

    ctx->selectedCipher = selectedCipher;
    dst->cipher = sslCipherSuiteGetSymmetricCipher(selectedCipher);
    dst->macAlgorithm = sslCipherSuiteGetHashHmacReference(selectedCipher);
};

/* Entry points to Record Layer */

static int SSLRecordReadInternal(SSLRecordContextRef ref, SSLRecord *rec)
{   int        err;
    size_t          len, contentLen;
    uint8_t           *charPtr;
    SSLBuffer       readData, cipherFragment;
    size_t          head=5;
    int             skipit=0;
    struct SSLRecordInternalContext *ctx = ref;

    if(ctx->isDTLS)
        head+=8;

    if (!ctx->partialReadBuffer.data || ctx->partialReadBuffer.length < head)
    {   if (ctx->partialReadBuffer.data)
        if ((err = SSLFreeBuffer(&ctx->partialReadBuffer)) != 0)
        {
            return err;
        }
        if ((err = SSLAllocBuffer(&ctx->partialReadBuffer,
                                  DEFAULT_BUFFER_SIZE)) != 0)
        {
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
            {   if (err == errSSLRecordWouldBlock) {
                ctx->amountRead += len;
                return err;
            }
            else {
                /* abort */
                err = errSSLRecordClosedAbort;
#if 0 // TODO: revisit this in the transport layer
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
                            sslHdskStateDebug("Server dropped initial connection\n");
                            err = errSSLConnectionRefused;
                            break;
                        default:
                            break;
                    }
                }
#endif
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
				case errSSLRecordWouldBlock:
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
					break;
            }
            return err;
        }
        ctx->amountRead += len;
    }

    check(ctx->amountRead >= head);

    charPtr = ctx->partialReadBuffer.data;
    rec->contentType = *charPtr++;
    if (rec->contentType < SSL_RecordTypeV3_Smallest ||
        rec->contentType > SSL_RecordTypeV3_Largest)
        return errSSLRecordProtocol;

    rec->protocolVersion = (SSLProtocolVersion)SSLDecodeInt(charPtr, 2);
    charPtr += 2;

    if(rec->protocolVersion == DTLS_Version_1_0)
    {
        sslUint64 seqNum;
        SSLDecodeUInt64(charPtr, 8, &seqNum);
        charPtr += 8;
        sslLogRecordIo("Read DTLS Record %016llx (seq is: %016llx)",
                       seqNum, ctx->readCipher.sequenceNum);

        /* if the epoch of the record is different of current read cipher, just drop it */
        if((seqNum>>48)!=(ctx->readCipher.sequenceNum>>48)) {
            skipit=1;
        } else {
            ctx->readCipher.sequenceNum=seqNum;
        }
    }

    contentLen = SSLDecodeInt(charPtr, 2);
    charPtr += 2;
    if (contentLen > (16384 + 2048))    /* Maximum legal length of an
										 * SSLCipherText payload */
    {
        return errSSLRecordRecordOverflow;
    }

    if (ctx->partialReadBuffer.length < head + contentLen)
    {   if ((err = SSLReallocBuffer(&ctx->partialReadBuffer, head + contentLen)) != 0)
    {
        return err;
    }
    }

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

    check(ctx->amountRead >= head + contentLen);

    cipherFragment.data = ctx->partialReadBuffer.data + head;
    cipherFragment.length = contentLen;

    ctx->amountRead = 0;        /* We've used all the data in the cache */

    /* We dont decrypt if we were told to skip this record */
    if(skipit) {
        return errSSLRecordUnexpectedRecord;
    }
	/*
	 * Decrypt the payload & check the MAC, modifying the length of the
	 * buffer to indicate the amount of plaintext data after adjusting
	 * for the block size and removing the MAC */
	check(ctx->sslTslCalls != NULL);
    if ((err = ctx->sslTslCalls->decryptRecord(rec->contentType,
                                               &cipherFragment, ctx)) != 0)
        return err;

	/*
	 * We appear to have sucessfully received a record; increment the
	 * sequence number
	 */
    IncrementUInt64(&ctx->readCipher.sequenceNum);

	/* Allocate a buffer to return the plaintext in and return it */
    if ((err = SSLAllocBuffer(&rec->contents, cipherFragment.length)) != 0)
    {
        return err;
    }
    memcpy(rec->contents.data, cipherFragment.data, cipherFragment.length);


    return 0;
}

static int SSLRecordWriteInternal(SSLRecordContextRef ref, SSLRecord rec)
{
    int err;
    struct SSLRecordInternalContext *ctx = ref;

    err=ctx->sslTslCalls->writeRecord(rec, ctx);

    check_noerr(err);

    return err;
}

/* Record Layer Entry Points */

static int
SSLRollbackInternalRecordLayerWriteCipher(SSLRecordContextRef ref)
{
    int err;
    struct SSLRecordInternalContext *ctx = ref;

    if ((err = SSLDisposeCipherSuite(&ctx->writePending, ctx)) != 0)
        return err;

    ctx->writePending = ctx->writeCipher;
    ctx->writeCipher = ctx->prevCipher;

    /* Zero out old data */
    memset(&ctx->prevCipher, 0, sizeof(CipherContext));

    return 0;
}

static int
SSLAdvanceInternalRecordLayerWriteCipher(SSLRecordContextRef ref)
{
    int err;
    struct SSLRecordInternalContext *ctx = ref;

    if ((err = SSLDisposeCipherSuite(&ctx->prevCipher, ctx)) != 0)
        return err;

    ctx->prevCipher = ctx->writeCipher;
    ctx->writeCipher = ctx->writePending;

    /* Zero out old data */
    memset(&ctx->writePending, 0, sizeof(CipherContext));

    return 0;
}

static int
SSLAdvanceInternalRecordLayerReadCipher(SSLRecordContextRef ref)
{
    struct SSLRecordInternalContext *ctx = ref;
    int err;

    if ((err = SSLDisposeCipherSuite(&ctx->readCipher, ctx)) != 0)
        return err;

    ctx->readCipher = ctx->readPending;
    memset(&ctx->readPending, 0, sizeof(CipherContext)); 	/* Zero out old data */

    return 0;
}

static int
SSLInitInternalRecordLayerPendingCiphers(SSLRecordContextRef ref, uint16_t selectedCipher, bool isServer, SSLBuffer key)
{   int        err;
    uint8_t         *keyDataProgress, *keyPtr, *ivPtr;
    CipherContext   *serverPending, *clientPending;

    struct SSLRecordInternalContext *ctx = ref;

    InitCipherSpec(ctx, selectedCipher);

    ctx->readPending.macRef = ctx->selectedCipherSpec.macAlgorithm;
    ctx->writePending.macRef = ctx->selectedCipherSpec.macAlgorithm;
    ctx->readPending.symCipher = ctx->selectedCipherSpec.cipher;
    ctx->writePending.symCipher = ctx->selectedCipherSpec.cipher;
    /* This need to be reinitialized because the whole thing is zeroed sometimes */
    ctx->readPending.encrypting = 0;
    ctx->writePending.encrypting = 1;

    if(ctx->negProtocolVersion == DTLS_Version_1_0)
    {
        ctx->readPending.sequenceNum = (ctx->readPending.sequenceNum & (0xffffULL<<48)) + (1ULL<<48);
        ctx->writePending.sequenceNum = (ctx->writePending.sequenceNum & (0xffffULL<<48)) + (1ULL<<48);
    } else {
        ctx->writePending.sequenceNum = 0;
        ctx->readPending.sequenceNum = 0;
    }

    if (isServer)
    {   serverPending = &ctx->writePending;
        clientPending = &ctx->readPending;
    }
    else
    {   serverPending = &ctx->readPending;
        clientPending = &ctx->writePending;
    }

    /* Check the size of the 'key' buffer - <rdar://problem/11204357> */
    if(key.length != ctx->selectedCipherSpec.macAlgorithm->hash->digestSize*2
                   + ctx->selectedCipherSpec.cipher->params->keySize*2
                   + ctx->selectedCipherSpec.cipher->params->ivSize*2)
    {
        return errSSLRecordInternal;
    }

    keyDataProgress = key.data;
    memcpy(clientPending->macSecret, keyDataProgress,
           ctx->selectedCipherSpec.macAlgorithm->hash->digestSize);
    keyDataProgress += ctx->selectedCipherSpec.macAlgorithm->hash->digestSize;
    memcpy(serverPending->macSecret, keyDataProgress,
           ctx->selectedCipherSpec.macAlgorithm->hash->digestSize);
    keyDataProgress += ctx->selectedCipherSpec.macAlgorithm->hash->digestSize;

    if (ctx->selectedCipherSpec.cipher->params->cipherType == aeadCipherType)
        goto skipInit;

    /* init the reusable-per-record MAC contexts */
    err = ctx->sslTslCalls->initMac(clientPending);
    if(err) {
        goto fail;
    }
    err = ctx->sslTslCalls->initMac(serverPending);
    if(err) {
        goto fail;
    }

    keyPtr = keyDataProgress;
    keyDataProgress += ctx->selectedCipherSpec.cipher->params->keySize;
    /* Skip server write key to get to IV */
    ivPtr = keyDataProgress + ctx->selectedCipherSpec.cipher->params->keySize;
    if ((err = ctx->selectedCipherSpec.cipher->c.cipher.initialize(clientPending->symCipher->params, clientPending->encrypting, keyPtr, ivPtr,
                                                                   &clientPending->cipherCtx)) != 0)
        goto fail;
    keyPtr = keyDataProgress;
    keyDataProgress += ctx->selectedCipherSpec.cipher->params->keySize;
    /* Skip client write IV to get to server write IV */
    ivPtr = keyDataProgress + ctx->selectedCipherSpec.cipher->params->ivSize;
    if ((err = ctx->selectedCipherSpec.cipher->c.cipher.initialize(serverPending->symCipher->params, serverPending->encrypting, keyPtr, ivPtr,
                                                                   &serverPending->cipherCtx)) != 0)
        goto fail;

skipInit:
    /* Ciphers are ready for use */
    ctx->writePending.ready = 1;
    ctx->readPending.ready = 1;

    /* Ciphers get swapped by sending or receiving a change cipher spec message */
    err = 0;

fail:
    return err;
}

static int
SSLSetInternalRecordLayerProtocolVersion(SSLRecordContextRef ref, SSLProtocolVersion negVersion)
{
    struct SSLRecordInternalContext *ctx = ref;

    switch(negVersion) {
        case SSL_Version_3_0:
            ctx->sslTslCalls = &Ssl3RecordCallouts;
            break;
        case TLS_Version_1_0:
        case TLS_Version_1_1:
        case DTLS_Version_1_0:
        case TLS_Version_1_2:
            ctx->sslTslCalls = &Tls1RecordCallouts;
            break;
        case SSL_Version_2_0:
        case SSL_Version_Undetermined:
        default:
            return errSSLRecordNegotiation;
    }
    ctx->negProtocolVersion = negVersion;

    return 0;
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

/***** Internal Record Layer APIs *****/

SSLRecordContextRef
SSLCreateInternalRecordLayer(bool dtls)
{
    struct SSLRecordInternalContext *ctx;

    ctx = sslMalloc(sizeof(struct SSLRecordInternalContext));
    if(ctx==NULL)
        return NULL;

    memset(ctx, 0, sizeof(struct SSLRecordInternalContext));

    ctx->negProtocolVersion = SSL_Version_Undetermined;

    ctx->sslTslCalls = &Ssl3RecordCallouts;
    ctx->recordWriteQueue = NULL;

    InitCipherSpec(ctx, TLS_NULL_WITH_NULL_NULL);

    ctx->writeCipher.macRef    = ctx->selectedCipherSpec.macAlgorithm;
    ctx->readCipher.macRef     = ctx->selectedCipherSpec.macAlgorithm;
    ctx->readCipher.symCipher  = ctx->selectedCipherSpec.cipher;
    ctx->writeCipher.symCipher = ctx->selectedCipherSpec.cipher;
    ctx->readCipher.encrypting = 0;
    ctx->writeCipher.encrypting = 1;

    ctx->isDTLS = dtls;

    return ctx;

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


    /* Cleanup cipher structs */
    SSLDisposeCipherSuite(&ctx->readCipher, ctx);
    SSLDisposeCipherSuite(&ctx->writeCipher, ctx);
    SSLDisposeCipherSuite(&ctx->readPending, ctx);
    SSLDisposeCipherSuite(&ctx->writePending, ctx);
    SSLDisposeCipherSuite(&ctx->prevCipher, ctx);

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
};


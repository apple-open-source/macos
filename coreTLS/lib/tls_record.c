//
//  tls_record_internal.c
//  Security
//
//  Created by Fabrice Gautier on 10/25/11.
//  Copyright (c) 2011,2013 Apple, Inc. All rights reserved.
//

/* THIS FILE CONTAINS KERNEL CODE */

#include <tls_record.h>
#include <tls_ciphersuites.h>
#include "sslBuildFlags.h"
#include "sslDebug.h"
#include "symCipher.h"
#include "sslUtils.h"
#include "tls_record_internal.h"

#include <AssertMacros.h>
#include <string.h>

#include <inttypes.h>

static int
SSLDisposeCipherSuite(CipherContext *cipher, tls_record_t ctx)
{   int      err;

	/* symmetric encryption context */
	if(cipher->symCipher) {
		if ((err = cipher->symCipher->finish(cipher->cipherCtx)) != 0) {
			return err;
		}
	}

    return 0;
}

#include <tls_ciphersuites.h>
#include "CipherSuite.h"
#include "symCipher.h"
#include <corecrypto/ccdigest.h>
#include <corecrypto/ccmd5.h>
#include <corecrypto/ccsha1.h>
#include <corecrypto/ccsha2.h>

static const struct ccdigest_info null_di = {0,};

static const struct ccdigest_info *sslCipherSuiteGetDigestInfo(uint16_t selectedCipher)
{
    HMAC_Algs alg = sslCipherSuiteGetMacAlgorithm(selectedCipher);

    switch (alg) {
        case HA_Null:
            return &null_di;
        case HA_MD5:
            return ccmd5_di();
        case HA_SHA1:
            return ccsha1_di();
        case HA_SHA256:
            return ccsha256_di();
        case HA_SHA384:
            return ccsha384_di();
        default:
            sslErrorLog("Invalid hashAlgorithm %d", alg);
            check(0);
            return NULL;
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

static size_t
tls_record_encrypted_size_1(tls_record_t ctx,
                                   size_t decrypted_size)
{
    /* Encrypted size is:
     *  IV [block cipher and TLS1.1 or DTLS 1.0 only]
     *  encrypted contents +
     *  macLength +
     *  padding [block ciphers only] +
     *  padding length field (1 byte) [block ciphers only]
     */

    CipherType cipherType = ctx->writeCipher.symCipher->params->cipherType;
    size_t blockSize = ctx->writeCipher.symCipher->params->blockSize;
    size_t payloadSize = decrypted_size;
    size_t padding;

    switch (cipherType) {
        case blockCipherType:
            payloadSize += ctx->writeCipher.di->output_size;
            padding = blockSize - (payloadSize % blockSize) - 1;
            payloadSize += padding + 1;
            /* TLS 1.1, TLS1.2 and DTLS 1.0 have an extra block for IV */
            if(ctx->negProtocolVersion >= tls_protocol_version_TLS_1_1) {
                payloadSize += blockSize;
            }
            break;
        case streamCipherType:
            payloadSize += ctx->writeCipher.di->output_size;
            break;
        case aeadCipherType:
            /* AES_GCM doesn't need padding. */
            payloadSize += TLS_AES_GCM_EXPLICIT_IV_SIZE+TLS_AES_GCM_TAG_SIZE;//16mac+8iv
            break;
        default:
            /* uh-oh! internal error? */
            check(0);
    }

    if(ctx->isDTLS)
        payloadSize+=DTLS_RECORD_HEADER_SIZE;
    else
        payloadSize+=TLS_RECORD_HEADER_SIZE;

    return payloadSize;
}

static bool tls_record_need_split(tls_record_t ctx, uint8_t contentType)
{
    CipherType cipherType = ctx->writeCipher.symCipher->params->cipherType;
    tls_protocol_version version = ctx->negProtocolVersion;


    return ctx->firstDataRecordEncrypted
            && ctx->splitEnabled
            && (contentType == tls_record_type_AppData)
            && (cipherType == blockCipherType)
            && (version <= tls_protocol_version_TLS_1_0);

}

size_t
tls_record_encrypted_size(tls_record_t ctx,
                                 uint8_t contentType,
                                 size_t decrypted_size)
{
    size_t out_size = 0;

    /* number of fragments: */
    size_t full_fragments;
    size_t one_byte_fragments;
    size_t remainder_size;

    /*  If we already wrote data, split is enabled and we have the right config, we have something like :
            {1|max|max|}leftover
        Otherwise we have:
            {max|max|}leftover
     */

    if(tls_record_need_split(ctx, contentType)) {
        one_byte_fragments = 1;
    } else {
        one_byte_fragments = 0;
    }

    full_fragments = (decrypted_size - one_byte_fragments) / TLS_MAX_FRAGMENT_SIZE;
    remainder_size = decrypted_size - one_byte_fragments - (full_fragments * TLS_MAX_FRAGMENT_SIZE);

    out_size = full_fragments * tls_record_encrypted_size_1(ctx, TLS_MAX_FRAGMENT_SIZE) +
                one_byte_fragments * tls_record_encrypted_size_1(ctx, 1);

    /* We never output empty records, too many implementations don't like them */
    if(remainder_size)
        out_size += tls_record_encrypted_size_1(ctx, remainder_size);

    return out_size;
}

size_t
tls_record_decrypted_size(tls_record_t ctx,
                                size_t encrypted_size)
{
    size_t overheadSize =  tls_record_get_header_size(ctx);

    /* Note: overheadSize is the *minimum* overhead */
    /* TODO (<rdar://problem/15957511>): Maybe we should cache the overheadSize, it only changes when we get the ChangeCipherSpec */

    CipherType cipherType = ctx->readCipher.symCipher->params->cipherType;
    size_t blockSize = ctx->readCipher.symCipher->params->blockSize;

    switch (cipherType) {
        case blockCipherType:
            overheadSize += 1; // at least one byte padding (padding size).
            overheadSize += ctx->readCipher.di->output_size;
            /* TLS 1.1, TLS1.2 and DTLS 1.0 have an extra block for IV */
            if(ctx->negProtocolVersion >= tls_protocol_version_TLS_1_1) {
                overheadSize += blockSize;
            }
            break;
        case streamCipherType:
            overheadSize += ctx->readCipher.di->output_size;
            break;
        case aeadCipherType:
            /* AES_GCM doesn't need padding. */
            overheadSize += TLS_AES_GCM_EXPLICIT_IV_SIZE+TLS_AES_GCM_TAG_SIZE;//16mac+8iv
            break;
        default:
            /* uh-oh! internal error? */
            check(0);
    }

    if(encrypted_size<overheadSize)
        return 0;

    return encrypted_size-overheadSize;
}


/* Entry points to Record Layer */
/* Encrypt 1 record, input.length MUST be less than the maximum record size allowed by TLS */
static int
tls_record_encrypt_1(tls_record_t ctx,
                            const tls_buffer input,
                            uint8_t contentType,
                            tls_buffer *output)
{
    int err;
    int             padding = 0, i;
    tls_buffer       payload;
    uint8_t         *charPtr;
    uint16_t        payloadSize,blockSize = 0;

    check(input.length <= TLS_MAX_FRAGMENT_SIZE);
    check(input.length > 0);

    payloadSize = tls_record_encrypted_size_1(ctx, input.length);

    check(output->length>=payloadSize);

    if(output->length<payloadSize)
    {
        check(0);
        return errSSLRecordParam; // output buffer too small
    }

    //adjust length.
    output->length=payloadSize;

    // cipher parameters:
    CipherType cipherType = ctx->writeCipher.symCipher->params->cipherType;
    const Cipher *cipher = &ctx->writeCipher.symCipher->c.cipher;
    const AEADCipher *aead = &ctx->writeCipher.symCipher->c.aead;
    blockSize = ctx->writeCipher.symCipher->params->blockSize;

    if (ctx->isDTLS)
        payloadSize-=DTLS_RECORD_HEADER_SIZE;
    else
        payloadSize-=TLS_RECORD_HEADER_SIZE;

    charPtr=output->data;
    *charPtr++=contentType;
    // We ignore the input protocol version, always use the current one
    charPtr=SSLEncodeInt(charPtr, ctx->negProtocolVersion, 2);
    if(ctx->isDTLS)
        charPtr=SSLEncodeUInt64(charPtr, ctx->writeCipher.sequenceNum);
    charPtr=SSLEncodeInt(charPtr, payloadSize, 2);

    /* Also for DTLS */
    if((ctx->negProtocolVersion >= tls_protocol_version_TLS_1_1) &&
       (cipherType == blockCipherType))
    {
        if((err = ccrng_generate(ctx->rng, blockSize, charPtr)) != 0)
            return err;
        charPtr += blockSize;
    }
    payload.data = charPtr;
    if (cipherType == aeadCipherType) {
        /* Encode the explicit iv */
        if((err = ctx->writeCipher.symCipher->c.aead.getIV(charPtr, ctx->writeCipher.cipherCtx)) != 0)
            return err;
        charPtr += TLS_AES_GCM_EXPLICIT_IV_SIZE;

        if ((err = ctx->writeCipher.symCipher->c.aead.setIV(charPtr-TLS_AES_GCM_EXPLICIT_IV_SIZE,
                                                            ctx->writeCipher.cipherCtx)) != 0)
            goto fail;
        /* TODO: If we ever add any mode other than GCM this code might have
         to be different. */
        /*
            The additional authenticated data is defined as follows:
             additional_data = seq_num + type + version + length;
             where "+" denotes concatenation.
         */
        uint8_t aad[13];
        /* First copy the 8 byte sequence number */
        SSLEncodeUInt64(aad, ctx->writeCipher.sequenceNum);
        /* Copy the 5 byte TLS header already encoded in packet to aad */
        memcpy(aad+8, charPtr-13, TLS_RECORD_HEADER_SIZE);
        /* Update length to length of plaintext after copying TLS header over */
        aad[11]=input.length>>8;
        aad[12]=input.length&0xff;
        if ((err = ctx->writeCipher.symCipher->c.aead.update(aad, 13, ctx->writeCipher.cipherCtx)) != 0)
            goto fail;
    }

    /* Copy the contents into the output buffer */
    memcpy(charPtr, input.data, input.length);
    payload.length = input.length;

    charPtr += input.length;

    /* MAC the data */
    if (cipherType != aeadCipherType) {
        if (ctx->writeCipher.di->output_size > 0)     /* Optimize away null case */
        {
            if ((err = SSLComputeMac(contentType,
                                     &payload,
                                     0,
                                     charPtr,
                                     &ctx->writeCipher,
                                     ctx->negProtocolVersion)) != 0)
                goto fail;
        }
    }

    /* For TLS 1.1 and DTLS, we would need to specifiy the IV, but instead
     we are clever like this: since the IV is just one block in front,
     we encrypt it with the rest of the data. The actual transmitted IV
     is the result of the encryption, with whatever internal IV is used.
     This method is explained in the TLS 1.1 RFC */
    if(ctx->negProtocolVersion >= tls_protocol_version_TLS_1_1 &&
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
            padding = blockSize - ((input.length+ctx->writeCipher.di->output_size) % blockSize) - 1;
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
            if ((err = aead->encrypt(payload.data,
                                       payload.data, payload.length, ctx->writeCipher.cipherCtx)) != 0)
                goto fail;
            break;

        default:
            check(0);
			return errSSLRecordInternal;
    }

    /* Increment the sequence number */
    IncrementUInt64(&ctx->writeCipher.sequenceNum);

    return 0;
    
fail:
    return err;
}

int
tls_record_encrypt(tls_record_t ctx,
                          const tls_buffer input,
                          uint8_t contentType,
                          tls_buffer *output)
{
    int err;
    tls_buffer inbuf;
    tls_buffer outbuf;
    size_t ilen = input.length;
    size_t olen = output->length;
    bool one_byte; // indicate if the next record is a one byte record, in the loop below
    CipherType cipherType = ctx->writeCipher.symCipher->params->cipherType;
    tls_protocol_version version = ctx->negProtocolVersion;


    inbuf.data = input.data;
    outbuf.data = output->data;

    /* Never output empty record, but don't fail either */
    if(ilen==0)
        return 0;

    /* This is application data packet, and we already encrypted a AppData record, we split, the first record is a 1-byte */
    one_byte = ctx->firstDataRecordEncrypted
                && ctx->splitEnabled
                && (contentType == tls_record_type_AppData)
                && (cipherType == blockCipherType)
                && (version <= tls_protocol_version_TLS_1_0);

    ctx->firstDataRecordEncrypted = contentType == tls_record_type_AppData;

    while(ilen>0)
    {
        if(one_byte) {
            inbuf.length = 1;
            one_byte = false;
        } else if (ilen>TLS_MAX_FRAGMENT_SIZE) {
            inbuf.length = TLS_MAX_FRAGMENT_SIZE;
        } else {
            inbuf.length = ilen;
        }
        outbuf.length = olen;
        require_noerr((err = tls_record_encrypt_1(ctx, inbuf, contentType, &outbuf)), errOut);
        inbuf.data += inbuf.length;
        ilen -= inbuf.length;
        outbuf.data += outbuf.length;
        olen -= outbuf.length;  // This will never underflow

    }

    output->length -= olen;
errOut:
    return err;
}


static inline size_t header_size(tls_record_t ctx)
{
    return ctx->isDTLS?DTLS_RECORD_HEADER_SIZE:TLS_RECORD_HEADER_SIZE;
}

int
tls_record_decrypt(tls_record_t ctx,
                          const tls_buffer input,
                          tls_buffer *output,
                          uint8_t *contentType)
{
    int        err;
    tls_buffer       cipherFragment;
    uint8_t         *charPtr;
    uint64_t        seqNum;
    uint8_t         ct;
    charPtr=input.data;

    check(input.length>=header_size(ctx));

    if(input.length<header_size(ctx))
        return errSSLRecordParam;

    ct = *charPtr++;
#if 0 // We dont actually check the record protocol version
    tls_protocol_version pv;
    pv = SSLDecodeInt(charPtr, 2);
#endif
    charPtr+=2;
    if(ctx->isDTLS) {
        seqNum = SSLDecodeUInt64(charPtr, 8); charPtr+=8;
    }

    cipherFragment.length = SSLDecodeInt(charPtr, 2); charPtr+=2;
    cipherFragment.data = charPtr;

#if 0 // This is too strict for the record layer.
    if (ct < tls_record_type_V3_Smallest ||
        ct > tls_record_type_V3_Largest)
        return errSSLRecordProtocol;

    if ((ctx->negProtocolVersion != tls_protocol_version_Undertermined) &&
        (pv != ctx->negProtocolVersion)) {
        sslErrorLog("invalid record protocol version, expected = %04x, received = %04x", ctx->negProtocolVersion, pv);
        return errSSLRecordProtocol; // Invalid record version ?
    }
#endif

    check(input.length>=header_size(ctx)+cipherFragment.length);

    if(input.length<header_size(ctx)+cipherFragment.length) {
        return errSSLRecordParam; // input buffer not enough data
    }

    if(ctx->isDTLS)
    {
        /* if the epoch of the record is different of current read cipher, just drop it */
        if((seqNum>>48)!=(ctx->readCipher.sequenceNum>>48)) {
            return errSSLRecordUnexpectedRecord;
        } else {
            ctx->readCipher.sequenceNum=seqNum;
        }
    }

    if (ctx->readCipher.symCipher->params->cipherType == aeadCipherType) {
        size_t overheadSize =  TLS_AES_GCM_EXPLICIT_IV_SIZE+TLS_AES_GCM_TAG_SIZE;
        if (cipherFragment.length < overheadSize)
            return errSSLRecordRecordOverflow;

        if ((err = ctx->readCipher.symCipher->c.aead.setIV(cipherFragment.data, ctx->readCipher.cipherCtx)) != 0)
            return errSSLRecordParam;
        /*
         The additional authenticated data is defined as follows:
         additional_data = seq_num + type + version + length;
         where "+" denotes concatenation.
         */
        uint8_t aad[13];
        uint8_t *seq = &aad[0];
        SSLEncodeUInt64(seq, ctx->readCipher.sequenceNum);
        memcpy(aad+8, charPtr-TLS_RECORD_HEADER_SIZE, TLS_RECORD_HEADER_SIZE);
        unsigned long len=cipherFragment.length-overheadSize;
        aad[11] = len>>8;
        aad[12] = len & 0xff;
        if ((err = ctx->readCipher.symCipher->c.aead.update(aad, 13, ctx->readCipher.cipherCtx)) != 0)
            return errSSLRecordParam;

    }

    /*
     * Decrypt the payload & check the MAC, modifying the length of the
     * buffer to indicate the amount of plaintext data after adjusting
     * for the block size and removing the MAC */
    if ((err = SSLDecryptRecord(ct, &cipherFragment, ctx)) != 0)
        return err;


    check(output->length>=cipherFragment.length);
    if(output->length<cipherFragment.length)
    {
        return errSSLRecordParam; // output buffer too small
    }

    output->length = cipherFragment.length;
    memcpy(output->data, cipherFragment.data, cipherFragment.length);

	/*
	 * We appear to have sucessfully decrypted a record; increment the
	 * sequence number
	 */
    IncrementUInt64(&ctx->readCipher.sequenceNum);

    if(contentType) {
        *contentType = ct;
    }

    return 0;
}

/* Record Layer Entry Points */
int
tls_record_rollback_write_cipher(tls_record_t ctx)
{
    int err;

    if ((err = SSLDisposeCipherSuite(&ctx->writePending, ctx)) != 0)
        return err;

    ctx->writePending = ctx->writeCipher;
    ctx->writeCipher = ctx->prevCipher;

    /* Zero out old data */
    memset(&ctx->prevCipher, 0, sizeof(CipherContext));

    return 0;
}

int
tls_record_advance_write_cipher(tls_record_t ctx)
{
    int err;

    if ((err = SSLDisposeCipherSuite(&ctx->prevCipher, ctx)) != 0)
        return err;

    ctx->prevCipher = ctx->writeCipher;
    ctx->writeCipher = ctx->writePending;
    ctx->firstDataRecordEncrypted = false;

    /* Zero out old data */
    memset(&ctx->writePending, 0, sizeof(CipherContext));

    return 0;
}

int
tls_record_advance_read_cipher(tls_record_t ctx)
{
    int err;

    if ((err = SSLDisposeCipherSuite(&ctx->readCipher, ctx)) != 0)
        return err;

    ctx->readCipher = ctx->readPending;
    memset(&ctx->readPending, 0, sizeof(CipherContext)); 	/* Zero out old data */

    return 0;
}

int
tls_record_init_pending_ciphers(tls_record_t ctx,
                                       uint16_t selectedCipher,
                                       bool isServer,
                                       tls_buffer key)
{
    int        err;
    uint8_t         *keyDataProgress, *keyPtr, *ivPtr;
    CipherContext   *serverPending, *clientPending;


    ctx->selectedCipher = selectedCipher;
    ctx->readPending.di = sslCipherSuiteGetDigestInfo(selectedCipher);
    ctx->writePending.di = sslCipherSuiteGetDigestInfo(selectedCipher);
    ctx->readPending.symCipher = sslCipherSuiteGetSymmetricCipher(selectedCipher);
    ctx->writePending.symCipher = sslCipherSuiteGetSymmetricCipher(selectedCipher);
    /* This need to be reinitialized because the whole thing is zeroed sometimes */
    ctx->readPending.encrypting = 0;
    ctx->writePending.encrypting = 1;

    if(ctx->isDTLS)
    {
        ctx->readPending.sequenceNum = (ctx->readPending.sequenceNum & (0xffffULL<<48)) + (1ULL<<48);
        ctx->writePending.sequenceNum = (ctx->writePending.sequenceNum & (0xffffULL<<48)) + (1ULL<<48);
    } else {
        ctx->writePending.sequenceNum = 0;
        ctx->readPending.sequenceNum = 0;
    }

    if (isServer)
    {
        serverPending = &ctx->writePending;
        clientPending = &ctx->readPending;
    }
    else
    {
        serverPending = &ctx->readPending;
        clientPending = &ctx->writePending;
    }

    /* Check the size of the 'key' buffer - <rdar://problem/11204357> */
    if (ctx->readPending.symCipher->params->cipherType != aeadCipherType) {
        if(key.length != ctx->readPending.di->output_size*2
                    + ctx->readPending.symCipher->params->keySize*2
                    + ctx->readPending.symCipher->params->ivSize*2)
        {
            return errSSLRecordInternal;
        }
    } else {
        if(key.length != ctx->readPending.symCipher->params->keySize*2
           + ctx->readPending.symCipher->params->ivSize*2)
        {
            return errSSLRecordInternal;
        }
    }

    keyDataProgress = key.data;
    if (ctx->readPending.symCipher->params->cipherType != aeadCipherType) {
        memcpy(clientPending->macSecret, keyDataProgress, ctx->readPending.di->output_size);
        keyDataProgress += ctx->readPending.di->output_size;
        memcpy(serverPending->macSecret, keyDataProgress, ctx->readPending.di->output_size);
        keyDataProgress += ctx->readPending.di->output_size;
    }

    keyPtr = keyDataProgress;
    keyDataProgress += ctx->readPending.symCipher->params->keySize;
    /* Skip server write key to get to IV */
    ivPtr = keyDataProgress + ctx->readPending.symCipher->params->keySize;
    if ((err = ctx->readPending.symCipher->c.cipher.initialize(clientPending->symCipher->params, clientPending->encrypting, keyPtr, ivPtr, ctx->rng,
                                                                   &clientPending->cipherCtx)) != 0)
        goto fail;
    keyPtr = keyDataProgress;
    keyDataProgress += ctx->readPending.symCipher->params->keySize;
    /* Skip client write IV to get to server write IV */
    if (ctx->readPending.symCipher->params->cipherType == aeadCipherType) {
        /* We only need the 4-byte implicit IV for GCM */
        ivPtr = keyDataProgress + ctx->readPending.symCipher->params->ivSize - TLS_AES_GCM_EXPLICIT_IV_SIZE;
    } else {
        ivPtr = keyDataProgress + ctx->readPending.symCipher->params->ivSize;
    }
    if ((err = ctx->readPending.symCipher->c.cipher.initialize(serverPending->symCipher->params, serverPending->encrypting, keyPtr, ivPtr, ctx->rng,
                                                                   &serverPending->cipherCtx)) != 0)
        goto fail;

    /* Ciphers are ready for use */
    ctx->writePending.ready = 1;
    ctx->readPending.ready = 1;

    /* Ciphers get swapped by sending or receiving a change cipher spec message */
    err = 0;

fail:
    return err;
}

int
tls_record_set_protocol_version(tls_record_t ctx,
                                       tls_protocol_version protocolVersion)
{
    switch(protocolVersion) {
        case tls_protocol_version_SSL_3:
        case tls_protocol_version_TLS_1_0:
        case tls_protocol_version_TLS_1_1:
        case tls_protocol_version_DTLS_1_0:
        case tls_protocol_version_TLS_1_2:
            ctx->negProtocolVersion = protocolVersion;
            break;
        case tls_protocol_version_Undertermined:
        default:
            return errSSLRecordNegotiation;
    }

    return 0;
}

/***** Internal tls_record APIs *****/

tls_record_t
tls_record_create(bool dtls, struct ccrng_state *rng)
{
    tls_record_t ctx;

    ctx = sslMalloc(sizeof(struct _tls_record_s));
    if(ctx==NULL)
        return NULL;

    memset(ctx, 0, sizeof(struct _tls_record_s));

    ctx->negProtocolVersion = tls_protocol_version_Undertermined;
    ctx->selectedCipher = TLS_NULL_WITH_NULL_NULL;
    ctx->writeCipher.di = sslCipherSuiteGetDigestInfo(ctx->selectedCipher);
    ctx->readCipher.di  = sslCipherSuiteGetDigestInfo(ctx->selectedCipher);
    ctx->readCipher.symCipher  = sslCipherSuiteGetSymmetricCipher(ctx->selectedCipher);
    ctx->writeCipher.symCipher = sslCipherSuiteGetSymmetricCipher(ctx->selectedCipher);
    ctx->readCipher.encrypting = 0;
    ctx->writeCipher.encrypting = 1;

    ctx->isDTLS = dtls;
    ctx->rng = rng;

    return ctx;
}

void
tls_record_destroy(tls_record_t ctx)
{
    /* Cleanup cipher structs */
    SSLDisposeCipherSuite(&ctx->readCipher, ctx);
    SSLDisposeCipherSuite(&ctx->writeCipher, ctx);
    SSLDisposeCipherSuite(&ctx->readPending, ctx);
    SSLDisposeCipherSuite(&ctx->writePending, ctx);
    SSLDisposeCipherSuite(&ctx->prevCipher, ctx);

    sslFree(ctx);
}

int
tls_record_get_header_size(tls_record_t ctx)
{
    return ctx->isDTLS?DTLS_RECORD_HEADER_SIZE:TLS_RECORD_HEADER_SIZE;
}

int
tls_record_parse_header(tls_record_t ctx, tls_buffer input, size_t *len, uint8_t *content_type)
{
    if(input.length<header_size(ctx))
        return -1;

    *len = SSLDecodeInt(input.data+header_size(ctx)-2, 2);
    if(content_type)
        *content_type = input.data[0];

    return 0;
}

int
tls_record_parse_ssl2_header(tls_record_t ctx, tls_buffer input, size_t *len, uint8_t *content_type)
{
    if(input.length<2)
        return -1;

    if(!(input.data[0] & 0x80))
        return -1;

    *len = SSLDecodeInt(input.data, 2) & 0x7fff;
    if(content_type)
        *content_type = tls_record_type_SSL2;

    return 0;
}

int
tls_record_set_record_splitting(tls_record_t ctx, bool enable)
{
    ctx->splitEnabled = enable;

    return 0;
}


void
tls_add_debug_logger(void (*function)(void *, const char *, const char *, const char *), void *ctx)
{
#if !KERNEL
    __ssl_add_debug_logger(function, ctx);
#endif
}

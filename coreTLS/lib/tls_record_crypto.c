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
 * tls_record_crypto.c - actual record layer crypto routines.
 */

/* THIS FILE CONTAINS KERNEL CODE */

#include "tls_record_internal.h"
#include "sslMemory.h"
#include "sslDebug.h"
#include "sslUtils.h"


#include <corecrypto/ccmd5.h>
#include <corecrypto/cchmac.h>

#include <AssertMacros.h>
#include <string.h>

// Borrowed from corecrypto - portable versions
#define	STORE32_LE(x, y) do {                                    \
((unsigned char *)(y))[3] = (unsigned char)(((x)>>24)&255);		\
((unsigned char *)(y))[2] = (unsigned char)(((x)>>16)&255);		\
((unsigned char *)(y))[1] = (unsigned char)(((x)>>8)&255);		\
((unsigned char *)(y))[0] = (unsigned char)((x)&255);			\
} while(0)

#define	LOAD32_LE(x, y) do {                                     \
x = ((uint32_t)(((const unsigned char *)(y))[3] & 255)<<24) |			    \
((uint32_t)(((const unsigned char *)(y))[2] & 255)<<16) |			    \
((uint32_t)(((const unsigned char *)(y))[1] & 255)<<8)  |			    \
((uint32_t)(((const unsigned char *)(y))[0] & 255));				    \
} while(0)

#define	STORE32_BE(x, y) do {                                \
((unsigned char *)(y))[0] = (unsigned char)(((x)>>24)&255);	\
((unsigned char *)(y))[1] = (unsigned char)(((x)>>16)&255);	\
((unsigned char *)(y))[2] = (unsigned char)(((x)>>8)&255);	\
((unsigned char *)(y))[3] = (unsigned char)((x)&255);       \
} while(0)

#define	LOAD32_BE(x, y) do {                             \
x = ((uint32_t)(((const unsigned char *)(y))[0] & 255)<<24) |	    \
((uint32_t)(((const unsigned char *)(y))[1] & 255)<<16) |		\
((uint32_t)(((const unsigned char *)(y))[2] & 255)<<8)  |		\
((uint32_t)(((const unsigned char *)(y))[3] & 255));          \
} while(0)


#define	STORE64_LE(x, y) do {                                    \
((unsigned char *)(y))[7] = (unsigned char)(((x)>>56)&255);     \
((unsigned char *)(y))[6] = (unsigned char)(((x)>>48)&255);		\
((unsigned char *)(y))[5] = (unsigned char)(((x)>>40)&255);		\
((unsigned char *)(y))[4] = (unsigned char)(((x)>>32)&255);		\
((unsigned char *)(y))[3] = (unsigned char)(((x)>>24)&255);		\
((unsigned char *)(y))[2] = (unsigned char)(((x)>>16)&255);		\
((unsigned char *)(y))[1] = (unsigned char)(((x)>>8)&255);		\
((unsigned char *)(y))[0] = (unsigned char)((x)&255);			\
} while(0)


#define STORE64_BE(x, y) do {                                    \
((unsigned char *)(y))[0] = (unsigned char)(((x)>>56)&255);		\
((unsigned char *)(y))[1] = (unsigned char)(((x)>>48)&255);		\
((unsigned char *)(y))[2] = (unsigned char)(((x)>>40)&255);		\
((unsigned char *)(y))[3] = (unsigned char)(((x)>>32)&255);		\
((unsigned char *)(y))[4] = (unsigned char)(((x)>>24)&255);		\
((unsigned char *)(y))[5] = (unsigned char)(((x)>>16)&255);		\
((unsigned char *)(y))[6] = (unsigned char)(((x)>>8)&255);		\
((unsigned char *)(y))[7] = (unsigned char)((x)&255);			\
} while(0)

#define	LOAD64_BE(x, y) do {                                     \
x = (((uint64_t)(((const unsigned char *)(y))[0] & 255))<<56) |           \
(((uint64_t)(((const unsigned char *)(y))[1] & 255))<<48) |           \
(((uint64_t)(((const unsigned char *)(y))[2] & 255))<<40) |           \
(((uint64_t)(((const unsigned char *)(y))[3] & 255))<<32) |           \
(((uint64_t)(((const unsigned char *)(y))[4] & 255))<<24) |           \
(((uint64_t)(((const unsigned char *)(y))[5] & 255))<<16) |           \
(((uint64_t)(((const unsigned char *)(y))[6] & 255))<<8)  |          	\
(((uint64_t)(((const unsigned char *)(y))[7] & 255)));	            \
} while(0)


uint8_t muxb(bool s, uint8_t a, uint8_t b)
{
    uint8_t cond =~((uint8_t)s-(uint8_t)1);//s?~zero:zero; see above
    uint8_t rc = (cond&a)|(~cond&b);
    return rc;
}

uint32_t mux32(bool s, uint32_t a, uint32_t b)
{
    uint32_t cond =~((uint32_t)s-(uint32_t)1);//s?~zero:zero; see above
    uint32_t rc = (cond&a)|(~cond&b);
    return rc;
}

/* constant time memory extraction - constant for a given len, src_len, dst and src */
void mem_extract(uint8_t *dst, const uint8_t *src, size_t offset, size_t dst_len, size_t src_len)
{
    for(size_t i=0; i<=src_len-dst_len; i++) {
        for(int j=0; j<dst_len; j++) {
            uint8_t b = src[i+j];
            uint8_t c = dst[j];
            dst[j]=muxb(i==offset, b, c);
        }
    }
}

/* SSL3:
 * MAC = hash( MAC_write_secret + pad_2 +
 *		       hash( MAC_write_secret + pad_1 + seq_num + type +
 *			         length + content )
 *			 )
 */
/* sequence, type, length */
#define SSL3_HDR_LENGTH (8 + 1 + 2)


/* TLS:
 * mac = HMAC_hash(MAC_write_secret, seq_num + TLSCompressed.type +
 *					TLSCompressed.version + TLSCompressed.length +
 *					TLSCompressed.fragment));
 */

/* sequence, type, version, length */
#define TLS_HDR_LENGTH (8 + 1 + 2 + 2)


/* common for sslv3 and tlsv1 */
/* Constant time no matter the value of padLen */
int SSLComputeMac(uint8_t type,
                  tls_buffer *data,
                  size_t padLen,
                  uint8_t *outputMAC,
                  CipherContext *cipherCtx,
                  tls_protocol_version pv)
{
    uint8_t *p;
    unsigned long j;
    size_t hdr_len;
    size_t ssl3_mac_pad_len = 40;
    size_t actual_data_len = data->length - padLen;

    const struct ccdigest_info *di = cipherCtx->di;
    size_t blocksize = di->block_size;
    uint8_t inner_hash[di->output_size];

    memset(inner_hash, 0, di->output_size);

    if(pv == tls_protocol_version_SSL_3) {
        if(di->output_size==CCMD5_OUTPUT_SIZE) {
            ssl3_mac_pad_len = 48;
        }
        hdr_len = di->output_size + ssl3_mac_pad_len + SSL3_HDR_LENGTH; // SSL3
    } else {
        hdr_len = blocksize + TLS_HDR_LENGTH; // TLS.
    }
    uint8_t hdr[hdr_len];
    p = hdr;


    if(pv == tls_protocol_version_SSL_3) {
        memcpy(p, cipherCtx->macSecret, di->output_size); p += di->output_size;
        memset(p, 0x36, ssl3_mac_pad_len); p += ssl3_mac_pad_len;
        p = SSLEncodeUInt64(p, cipherCtx->sequenceNum);
        *p++ = type;
        *p++ = actual_data_len >> 8;
        *p   = actual_data_len & 0xff;
    } else {
        memset(p, 0x36, blocksize); //hmac IPAD
        for(j=0;j<di->output_size;j++) {
            p[j]=p[j]^cipherCtx->macSecret[j];
        }
        p += blocksize;
        p = SSLEncodeUInt64(p, cipherCtx->sequenceNum);
        *p++ = type;
        *p++ = pv >> 8;
        *p++ = pv & 0xff;
        *p++ = actual_data_len >> 8;
        *p   = actual_data_len & 0xff;
    }

    uint8_t block[blocksize];

    unsigned long i;
    cc_ctx_decl(struct ccdigest_state, di->state_size, inner_state);
    cc_ctx_decl(struct ccdigest_state, di->state_size, final_inner_state);

    memcpy(inner_state, di->initial_state, di->state_size);
    memset(inner_hash, 0, di->output_size);


    uint64_t actual_bitlen = (hdr_len + actual_data_len) * 8;

    // Hack to identify MD5 vs SHA1/SHA256 vs SHA384
    uint8_t hash_bitlen[8] = {0,};
    size_t hash_bitlenlen;
    if(di->output_size==16) { // MD5
        hash_bitlenlen = 8;
        STORE64_LE(actual_bitlen, hash_bitlen);
    } else if (di->block_size==128) { //SHA384
        hash_bitlenlen = 16;
        STORE64_BE(actual_bitlen, hash_bitlen);
    } else { //SHA1/SHA256
        hash_bitlenlen = 8;
        STORE64_BE(actual_bitlen, hash_bitlen);
    }

    //number of blocks to process
    unsigned long nblocks = (hdr_len + data->length + 1 + hash_bitlenlen - 1)/blocksize + 1;
    // Offset of the first byte of hash padding.
    size_t hash_pad_first_byte = hdr_len + actual_data_len;
    // Actual last block
    size_t actual_last_block = (hdr_len + actual_data_len + 1 + hash_bitlenlen - 1)/blocksize;
    size_t hash_pad_last_byte = (actual_last_block+1)*blocksize-1;

    unsigned long fast_blocks;

    if(nblocks<6) {
        fast_blocks = 0;
    } else {
        fast_blocks = nblocks - 6;
    }

    for(i=0; i<fast_blocks; i++)
    {
        size_t k=i*blocksize;
        if((k+blocksize)<hdr_len) {
            di->compress(inner_state, 1, hdr+k);
        } else if (k<hdr_len) {
            memcpy(block, hdr+k, hdr_len-k);
            memcpy(block+hdr_len-k, data->data, blocksize-(hdr_len-k));
            di->compress(inner_state, 1, block);
        } else {
            di->compress(inner_state, 1, data->data+(k-hdr_len));
        }
    }

    for(i=fast_blocks;i<nblocks;i++)
    {
        for(j=0;j<blocksize;j++)
        {
            size_t k = i*blocksize+j;
            uint8_t b = 0;
            if(k<hdr_len) {
                b=hdr[k];
            } else {
                if((k-hdr_len)<data->length)
                    b = data->data[k-hdr_len];
                b = muxb(k^hash_pad_first_byte, b,  0x80);
                b = muxb((k>hash_pad_first_byte) & (k<=hash_pad_last_byte-8), 0x00, b);
                if(j>=(blocksize-8))
                    b = muxb((k>hash_pad_last_byte-8) & ((k<=hash_pad_last_byte)),hash_bitlen[j-(blocksize-8)], b);
            }
            block[j]=b;
        }

        di->compress(inner_state, 1, block);

        for(j=0; j<di->state_size/4; j++) {
            uint32_t h = ccdigest_u32(final_inner_state)[j];
            h = mux32(i==actual_last_block,ccdigest_u32(inner_state)[j],h);
            ccdigest_u32(final_inner_state)[j] = h;
        }

    }

    if(di->output_size<=16) { //MD4 or MD5
        for (unsigned int i = 0; i < di->output_size / 4; i++) {
            STORE32_LE(ccdigest_u32(final_inner_state)[i], inner_hash+(4*i));
        }
    } else if(di->output_size<=32) { // SHA1/SHA224/SHA256
        for (unsigned int i = 0; i < di->output_size / 4; i++) {
            STORE32_BE(ccdigest_u32(final_inner_state)[i], inner_hash+(4*i));
        }
    } else { //SHA384/512
        for (unsigned int i = 0; i < di->output_size / 8; i++) {
            STORE64_BE(ccdigest_u64(final_inner_state)[i], inner_hash+(8*i));
        }
    }

    memset(inner_state, 0, di->state_size);

    // Final hash with oPAD
    memset(block, 0x5c, blocksize);
    ccdigest_di_decl(di, outer);
    ccdigest_init(di, outer);

    if(pv == tls_protocol_version_SSL_3) {
        ccdigest_update(di, outer, di->output_size, cipherCtx->macSecret);
        ccdigest_update(di, outer, ssl3_mac_pad_len, block);
    } else {
        for(j=0;j<di->output_size; j++)
        {
            block[j]=block[j]^cipherCtx->macSecret[j];
        }
        ccdigest_update(di, outer, blocksize, block);
    }
    ccdigest_update(di, outer, di->output_size, inner_hash);
    ccdigest_final(di, outer, outputMAC);

    ccdigest_di_clear(di, outer);
    memset(inner_hash, 0, di->output_size);

    return 0;
}


static
int SSLVerifyMac(uint8_t type,
                 tls_buffer *data,
                 size_t padLen,
                 uint8_t *compareMAC,
                 tls_record_t ctx)
{
    int rc;
    uint8_t outMAC[ctx->readCipher.di->output_size];


    //    printf("SSLVerifyMac dataLen=%zd, padLen=%zd\n", data->length, padLen);
    SSLComputeMac(type, data, padLen, outMAC, &ctx->readCipher, ctx->negProtocolVersion);


    rc = cc_cmp_safe(ctx->readCipher.di->output_size, outMAC, compareMAC);



    return (~rc)&1; // return 0 for failed, 1 for good.
}


int SSLDecryptRecord(
	uint8_t type,
	tls_buffer *payload,
	tls_record_t ctx)
{
	int    err;
    tls_buffer   content;

    CipherType cipherType = ctx->readCipher.symCipher->params->cipherType;

    /* Decrypt in place */
    switch (cipherType) {
        case aeadCipherType:
            if ((err = ctx->readCipher.symCipher->c.aead.decrypt(payload->data+TLS_AES_GCM_EXPLICIT_IV_SIZE,
                                                                 payload->data+TLS_AES_GCM_EXPLICIT_IV_SIZE,
                                                                 payload->length-TLS_AES_GCM_EXPLICIT_IV_SIZE,
                                                                   ctx->readCipher.cipherCtx)) != 0) {
                return errSSLRecordDecryptionFail;
            }
            content.data = payload->data + TLS_AES_GCM_EXPLICIT_IV_SIZE;
            content.length = payload->length - (TLS_AES_GCM_EXPLICIT_IV_SIZE+TLS_AES_GCM_TAG_SIZE);
            /* Test for underflow - if the record size is smaller than required */
            if(content.length > payload->length) {
                return errSSLRecordClosedAbort;
            }
            err = 0;

            break;
        case blockCipherType:

            if ((payload->length % ctx->readCipher.symCipher->params->blockSize) != 0)
                return errSSLRecordRecordOverflow;

            if ((err = ctx->readCipher.symCipher->c.cipher.decrypt(payload->data,
                                                                   payload->data, payload->length,
                                                                   ctx->readCipher.cipherCtx)) != 0) {
                return errSSLRecordDecryptionFail;
            }
            /* Remove IV (optional), mac and padlen */
            if (ctx->negProtocolVersion>=tls_protocol_version_TLS_1_1) {
                /* TLS 1.1 and DTLS 1.0 block ciphers */
                content.data = payload->data + ctx->readCipher.symCipher->params->blockSize;
                content.length = payload->length - (ctx->readCipher.di->output_size + ctx->readCipher.symCipher->params->blockSize + 1)  ;
            } else {
                content.data = payload->data;
                content.length = payload->length - (ctx->readCipher.di->output_size + 1) ;
            }
            /* Test for underflow - if the record size is smaller than required */
            if(content.length > payload->length) {
                return errSSLRecordClosedAbort;
            }

            err = 0;
            /* for TLSv1, padding can be anywhere from 0 to 255 bytes, all bytes need to be the same value.
               for SSLv3, padding can be from 0 to blocksize-1 bytes,  */
            uint8_t padLen = payload->data[payload->length - 1];
            size_t maxPadLen;
            uint8_t good = 1;

            if(ctx->negProtocolVersion == tls_protocol_version_SSL_3) {
                maxPadLen = ctx->readCipher.symCipher->params->blockSize - 1;
            } else {
                maxPadLen = 255;
            }
            if(maxPadLen>content.length) maxPadLen = content.length;

            // check actual padLen is within bound.
            good &= (padLen <= maxPadLen);

            padLen = muxb(good, padLen, maxPadLen);

            if(ctx->negProtocolVersion != tls_protocol_version_SSL_3) {
                /* TLS: Need to check all padding bytes */
                for(int i=0;i<maxPadLen;i++) {
                    uint8_t b = payload->data[payload->length - (i+1)];
                    good &= muxb((b==padLen) | (i>=padLen), 1, 0);
                }
            }

            /* Verify MAC on payload - Optimize away for null case */
            if (ctx->readCipher.di->output_size > 0) {
                /* Memory access to the MAC is constant time */
                uint8_t recordMAC[ctx->readCipher.di->output_size];
                memset(recordMAC, 0, ctx->readCipher.di->output_size);
                size_t macBufferSize = ctx->readCipher.di->output_size + maxPadLen;
                uint8_t *macBuffer = payload->data + payload->length - 1 - macBufferSize;
                size_t macOffsetReal = maxPadLen - padLen;
                mem_extract(recordMAC, macBuffer, macOffsetReal, sizeof(recordMAC), macBufferSize);

                good &= SSLVerifyMac(type, &content, padLen,
                                     recordMAC, ctx);

                content.length -= padLen;
            }

            if(!good) {
                err = errSSLRecordBadRecordMac;
            }

            break;

        case streamCipherType:

            if ((err = ctx->readCipher.symCipher->c.cipher.decrypt(payload->data,
                                                                   payload->data, payload->length,
                                                                   ctx->readCipher.cipherCtx)) != 0)
            {
                return errSSLRecordDecryptionFail;
            }
            content.data = payload->data;
            content.length = payload->length - ctx->readCipher.di->output_size;
            /* Test for underflow - if the record size is smaller than required */
            if(content.length > payload->length) {
                return errSSLRecordClosedAbort;
            }

            err = 0;
            /* Verify MAC on payload */
            if (ctx->readCipher.di->output_size > 0)
            /* Optimize away MAC for null case */
            if(!SSLVerifyMac(type, &content, 0, content.data + content.length, ctx))
            {
                err = errSSLRecordBadRecordMac;
            }
            break;
    }

    *payload = content;     /* Modify payload buffer to indicate content length */

    return err;
}

//
//  tls_stream_parser.c
//  libsecurity_ssl
//
//  Created by Fabrice Gautier on 9/9/13.
//
//

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <AssertMacros.h>
#include "tls_stream_parser.h"
#include "sslMemory.h"
#include "sslUtils.h"
#include "sslDebug.h"

struct _tls_stream_parser {
    size_t currentOffset; // not used for DTLS
    uint8_t currentHeader[TLS_RECORD_HEADER_SIZE]; //not used for DTLS
    tls_buffer currentRecord; // not used for DTLS
    tls_stream_parser_ctx_t *callback_ctx;
    tls_stream_parser_process_callback_t process;
};


tls_stream_parser_t
tls_stream_parser_create(tls_stream_parser_ctx_t ctx,
                         tls_stream_parser_process_callback_t process)
{
    tls_stream_parser_t parser=(tls_stream_parser_t)malloc(sizeof(struct _tls_stream_parser));

    memset(parser, 0, sizeof(*parser));

    if(parser) {
        parser->callback_ctx=ctx;
        parser->process=process;
    }

    return parser;
}

int
tls_stream_parser_parse(tls_stream_parser_t parser,
                        tls_buffer data)
{
    size_t len=data.length;
    uint8_t *p=data.data;
    int err = 0;

    while(len) {
        if(parser->currentOffset<TLS_RECORD_HEADER_SIZE) {
            // We dont have full header yet.
            size_t l=TLS_RECORD_HEADER_SIZE-parser->currentOffset;
            if(len<l) l=len;
            memcpy(parser->currentHeader+parser->currentOffset, p, l);
            len-=l; p+=l; parser->currentOffset+=l;

            // allocate buffer once we get a full header
            if(parser->currentOffset==TLS_RECORD_HEADER_SIZE) {
                size_t rlen;
                if(parser->currentHeader[0]&0x80) {
                    // This look like an SSL2 Record
                    size_t ssl2len=SSLDecodeInt(parser->currentHeader, 2)&0x7FFF;
                    rlen=ssl2len+2;
                } else {
                    size_t plen=SSLDecodeInt(parser->currentHeader+TLS_RECORD_HEADER_SIZE-2, 2);
                    rlen=plen+TLS_RECORD_HEADER_SIZE;
                }
                require_noerr((err=SSLAllocBuffer(&parser->currentRecord, rlen)), fail);
                memcpy(parser->currentRecord.data,parser->currentHeader, TLS_RECORD_HEADER_SIZE);
            }
        } else {
            size_t l=parser->currentRecord.length-parser->currentOffset;
            if(len<l) l=len;
            memcpy(parser->currentRecord.data+parser->currentOffset, p, l);
            len-=l; p+=l; parser->currentOffset+=l;
            if(parser->currentOffset>=parser->currentRecord.length) {
                assert(parser->currentOffset==parser->currentRecord.length);
                // got a full record, process it.
                require_noerr_quiet((err = parser->process(parser->callback_ctx,parser->currentRecord)), fail); // process
                parser->currentOffset=0;
                SSLFreeBuffer(&parser->currentRecord); // TODO: ownership of buffers.
            }
        }
    } // end while

fail:
    return err;
}

void
tls_stream_parser_destroy(tls_stream_parser_t parser)
{
    SSLFreeBuffer(&parser->currentRecord);
    sslFree(parser);
}


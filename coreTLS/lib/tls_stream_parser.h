//
//  tls_stream_parser.h
//  libsecurity_ssl
//
//  Created by Fabrice Gautier on 9/9/13.
//
//

#ifndef _TLS_STREAM_PARSER_H_
#define _TLS_STREAM_PARSER_H_ 1

#include "tls_types.h"

/* module to unpack stream in SSL Records, for TLS only */

/* packer */
typedef struct _tls_stream_parser *tls_stream_parser_t;
typedef void *tls_stream_parser_ctx_t;

typedef int
(*tls_stream_parser_process_callback_t) (tls_stream_parser_ctx_t ctx, tls_buffer record);

tls_stream_parser_t
tls_stream_parser_create(tls_stream_parser_ctx_t ctx,
                         tls_stream_parser_process_callback_t process);

void
tls_stream_parser_destroy(tls_stream_parser_t parser);

/* For TLS Only:
 - Can be called with any amount of data
 - call the process callback for every record.
 - buffer the remainder if any.
 */
int
tls_stream_parser_parse(tls_stream_parser_t parser,
                        tls_buffer data);


#endif

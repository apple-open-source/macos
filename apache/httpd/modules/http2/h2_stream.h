/* Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __mod_h2__h2_stream__
#define __mod_h2__h2_stream__

#include <http_protocol.h>

#include "h2.h"
#include "h2_headers.h"

/**
 * A HTTP/2 stream, e.g. a client request+response in HTTP/1.1 terms.
 * 
 * A stream always belongs to a h2_session, the one managing the
 * connection to the client. The h2_session writes to the h2_stream,
 * adding HEADERS and DATA and finally an EOS. When headers are done,
 * h2_stream is scheduled for handling, which is expected to produce
 * h2_headers/RESPONSE buckets.
 *
 * The h2_headers may be followed by more h2_headers (interim responses) and
 * by DATA frames read from the h2_stream until EOS is reached. Trailers
 * are send when a last h2_headers is received. This always closes the stream
 * output.
 */

struct h2_mplx;
struct h2_priority;
struct h2_request;
struct h2_session;
struct h2_bucket_beam;

typedef struct h2_stream h2_stream;

typedef void h2_stream_state_cb(void *ctx, h2_stream *stream);
typedef void h2_stream_event_cb(void *ctx, h2_stream *stream, 
                                h2_stream_event_t ev);

/**
 * Callback structure for events and stream state transisitions
 */
typedef struct h2_stream_monitor {
    void *ctx;
    h2_stream_state_cb *on_state_enter;   /* called when a state is entered */
    h2_stream_state_cb *on_state_invalid; /* called when an invalid state change
                                             was detected */
    h2_stream_event_cb *on_state_event;   /* called right before the given event
                                             result in a new stream state */
    h2_stream_event_cb *on_event;         /* called for events that do not 
                                             trigger a state change */
} h2_stream_monitor;

#ifdef AP_DEBUG
#define H2_STRM_MAGIC_OK     0x5354524d
#define H2_STRM_MAGIC_SDEL   0x5344454c
#define H2_STRM_MAGIC_PDEL   0x5044454c

#define H2_STRM_ASSIGN_MAGIC(s,m)  ((s)->magic = m)
#define H2_STRM_ASSERT_MAGIC(s,m)  ap_assert((s)->magic == m)
#else
#define H2_STRM_ASSIGN_MAGIC(s,m)  ((void)0)
#define H2_STRM_ASSERT_MAGIC(s,m)  ((void)0)
#endif

struct h2_stream {
#ifdef AP_DEBUG
    uint32_t magic;
#endif
    int id;                     /* http2 stream identifier */
    int initiated_on;           /* initiating stream id (PUSH) or 0 */
    apr_pool_t *pool;           /* the memory pool for this stream */
    struct h2_session *session; /* the session this stream belongs to */
    h2_stream_state_t state;    /* state of this stream */
    
    apr_time_t created;         /* when stream was created */
    
    const struct h2_request *request; /* the request made in this stream */
    struct h2_request *rtmp;    /* request being assembled */
    apr_table_t *trailers_in;   /* optional, incoming trailers */
    int request_headers_added;  /* number of request headers added */
    int request_headers_failed; /* number of request headers failed to add */

#if AP_HAS_RESPONSE_BUCKETS
    ap_bucket_response *response; /* the final, non-interim response or NULL */
#else
    struct h2_headers *response; /* the final, non-interim response or NULL */
#endif

    struct h2_bucket_beam *input;
    apr_bucket_brigade *in_buffer;
    int in_window_size;
    apr_time_t in_last_write;
    
    struct h2_bucket_beam *output;
    apr_bucket_brigade *out_buffer;

    int rst_error;              /* stream error for RST_STREAM */
    unsigned int aborted   : 1; /* was aborted */
    unsigned int scheduled : 1; /* stream has been scheduled */
    unsigned int input_closed : 1; /* no more request data/trailers coming */
    unsigned int push_policy;   /* which push policy to use for this request */
    unsigned int sent_trailers : 1; /* trailers have been submitted */
    unsigned int output_eos : 1; /* output EOS in buffer/sent */

    conn_rec *c2;               /* connection processing stream */
    
    const h2_priority *pref_priority; /* preferred priority for this stream */
    apr_off_t out_frames;       /* # of frames sent out */
    apr_off_t out_frame_octets; /* # of RAW frame octets sent out */
    apr_off_t out_data_frames;  /* # of DATA frames sent */
    apr_off_t out_data_octets;  /* # of DATA octets (payload) sent */
    apr_off_t in_data_frames;   /* # of DATA frames received */
    apr_off_t in_data_octets;   /* # of DATA octets (payload) received */
    apr_off_t in_trailer_octets; /* # of HEADER octets (payload) received in trailers */
    
    h2_stream_monitor *monitor; /* optional monitor for stream states */
};


#define H2_STREAM_RST(s, def)    (s->rst_error? s->rst_error : (def))

/**
 * Create a stream in H2_SS_IDLE state.
 * @param id      the stream identifier
 * @param pool    the memory pool to use for this stream
 * @param session the session this stream belongs to
 * @param monitor an optional monitor to be called for events and 
 *                state transisitions
 * @param initiated_on the id of the stream this one was initiated on (PUSH)
 *
 * @return the newly opened stream
 */
h2_stream *h2_stream_create(int id, apr_pool_t *pool, 
                            struct h2_session *session,
                            h2_stream_monitor *monitor,
                            int initiated_on);

/**
 * Destroy memory pool if still owned by the stream.
 */
void h2_stream_destroy(h2_stream *stream);

/**
 * Perform any late initialization before stream starts processing.
 */
apr_status_t h2_stream_prepare_processing(h2_stream *stream);

/*
 * Set a new monitor for this stream, replacing any existing one. Can
 * be called with NULL to have no monitor installed.
 */
void h2_stream_set_monitor(h2_stream *stream, h2_stream_monitor *monitor);

/**
 * Dispatch (handle) an event on the given stream.
 * @param stream  the streama the event happened on
 * @param ev      the type of event
 */
void h2_stream_dispatch(h2_stream *stream, h2_stream_event_t ev);

/**
 * Determine if stream is at given state.
 * @param stream the stream to check
 * @param state the state to look for
 * @return != 0 iff stream is at given state.
 */
int h2_stream_is_at(const h2_stream *stream, h2_stream_state_t state);

/**
 * Determine if stream is reached given state or is past this state.
 * @param stream the stream to check
 * @param state the state to look for
 * @return != 0 iff stream is at or past given state.
 */
int h2_stream_is_at_or_past(const h2_stream *stream, h2_stream_state_t state);

/**
 * Cleanup references into requst processing.
 *
 * @param stream the stream to cleanup
 */
void h2_stream_cleanup(h2_stream *stream);

/**
 * Notify the stream that amount bytes have been consumed of its input
 * since the last invocation of this method (delta amount).
 */
apr_status_t h2_stream_in_consumed(h2_stream *stream, apr_off_t amount);

/**
 * Set complete stream headers from given h2_request.
 * 
 * @param stream stream to write request to
 * @param r the request with all the meta data
 * @param eos != 0 iff stream input is closed
 */
void h2_stream_set_request(h2_stream *stream, const h2_request *r);

/**
 * Set complete stream header from given request_rec.
 * 
 * @param stream stream to write request to
 * @param r the request with all the meta data
 * @param eos != 0 iff stream input is closed
 */
apr_status_t h2_stream_set_request_rec(h2_stream *stream, 
                                       request_rec *r, int eos);

/*
 * Add a HTTP/2 header (including pseudo headers) or trailer 
 * to the given stream, depending on stream state.
 *
 * @param stream stream to write the header to
 * @param name the name of the HTTP/2 header
 * @param nlen the number of characters in name
 * @param value the header value
 * @param vlen the number of characters in value
 */
apr_status_t h2_stream_add_header(h2_stream *stream,
                                  const char *name, size_t nlen,
                                  const char *value, size_t vlen);
                                  
/* End the construction of request headers */
apr_status_t h2_stream_end_headers(h2_stream *stream, int eos, size_t raw_bytes);


apr_status_t h2_stream_send_frame(h2_stream *stream, int frame_type, int flags, size_t frame_len);
apr_status_t h2_stream_recv_frame(h2_stream *stream, int frame_type, int flags, size_t frame_len);

/*
 * Process a frame of received DATA.
 *
 * @param stream stream to write the data to
 * @param flags the frame flags
 * @param data the beginning of the bytes to write
 * @param len the number of bytes to write
 */
apr_status_t h2_stream_recv_DATA(h2_stream *stream, uint8_t flags,
                                 const uint8_t *data, size_t len);

/**
 * Reset the stream. Stream write/reads will return errors afterwards.
 *
 * @param stream the stream to reset
 * @param error_code the HTTP/2 error code
 */
void h2_stream_rst(h2_stream *stream, int error_code);

/**
 * Stream input signals change. Take necessary actions.
 * @param stream the stream to read output for
 */
void h2_stream_on_input_change(h2_stream *stream);

/**
 * Stream output signals change. Take necessary actions.
 * @param stream the stream to read output for
 */
void h2_stream_on_output_change(h2_stream *stream);

/**
 * Read a maximum number of bytes into the bucket brigade.
 * 
 * @param stream the stream to read from
 * @param bb the brigade to append output to
 * @param plen (in-/out) max. number of bytes to append and on return actual
 *        number of bytes appended to brigade
 * @param peos (out) != 0 iff end of stream has been reached while reading
 * @return APR_SUCCESS if out information was computed successfully.
 *         APR_EAGAIN if not data is available and end of stream has not been
 *         reached yet.
 */
apr_status_t h2_stream_read_to(h2_stream *stream, apr_bucket_brigade *bb, 
                               apr_off_t *plen, int *peos);

/**
 * Get optional trailers for this stream, may be NULL. Meaningful
 * results can only be expected when the end of the response body has
 * been reached.
 *
 * @param stream to ask for trailers
 * @return trailers for NULL
 */
apr_table_t *h2_stream_get_trailers(h2_stream *stream);

/**
 * Submit any server push promises on this stream and schedule
 * the streams for these.
 *
 * @param stream the stream for which to submit
 */
#if AP_HAS_RESPONSE_BUCKETS
apr_status_t h2_stream_submit_pushes(h2_stream *stream,
                                     ap_bucket_response *response);
#else
apr_status_t h2_stream_submit_pushes(h2_stream *stream,
                                     struct h2_headers *response);
#endif

/**
 * Get priority information set for this stream.
 */
#if AP_HAS_RESPONSE_BUCKETS
const struct h2_priority *h2_stream_get_priority(h2_stream *stream,
                                                 ap_bucket_response *response);
#else
const struct h2_priority *h2_stream_get_priority(h2_stream *stream,
                                                 struct h2_headers *response);
#endif

/**
 * Return a textual representation of the stream state as in RFC 7540
 * nomenclator, all caps, underscores.
 */
const char *h2_stream_state_str(const h2_stream *stream);

/**
 * Determine if stream is ready for submitting a response or a RST
 * @param stream the stream to check
 */
int h2_stream_is_ready(h2_stream *stream);

int h2_stream_wants_send_data(h2_stream *stream);

#define H2_STRM_MSG(s, msg)     \
    "h2_stream(%d-%lu-%d,%s): "msg, s->session->child_num, \
    (unsigned long)s->session->id, s->id, h2_stream_state_str(s)

#define H2_STRM_LOG(aplogno, s, msg)    aplogno H2_STRM_MSG(s, msg)

#endif /* defined(__mod_h2__h2_stream__) */

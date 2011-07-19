/*
 * Copyright (c) 2010 Apple Inc. All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without  
 * modification, are permitted provided that the following conditions  
 * are met:
 * 
 * 1.  Redistributions of source code must retain the above copyright  
 * notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above  
 * copyright notice, this list of conditions and the following  
 * disclaimer in the documentation and/or other materials provided  
 * with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of its  
 * contributors may be used to endorse or promote products derived  
 * from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND  
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,  
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A  
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS  
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,  
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT  
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF  
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND  
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,  
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT  
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF  
 * SUCH DAMAGE.
 */

/* Implements RFC 3030 - Binary ESMTP
   Convert binary message bodies to base64.
   Rather than create a whole new output stream which filters the data,
   just interpose our own context on the same stream. */

#include <sys_defs.h>
#include <msg.h>
#include <mymalloc.h>
#include <vstring.h>
#include <vstream.h>
#include <split_at.h>
#include <mime_state.h>
#include <rec_type.h>
#include <smtpd_binary.h>

#include <string.h>

struct binary_context {
    VSTREAM *output;
    void *output_context;
    int (*output_rec)(VSTREAM *stream, int rec_type, const char *data,
		      ssize_t len);
    MIME_STATE *mime_state;
};

static void binary_head_out(void *context, int header_class_unused,
			    const HEADER_OPTS *header_info_unused,
			    VSTRING *buf, off_t offset_unused)
{
    struct binary_context *bcp = context;
    char *start, *line, *next_line;

    for (line = start = vstring_str(buf); line; line = next_line) {
	next_line = split_at(line, '\n');
	bcp->output_rec(bcp->output, REC_TYPE_NORM, line,
			next_line ? next_line - line - 1 : strlen(line));
    }
}

static void binary_body_out(void *context, int rec_type,
			    const char *buf, ssize_t len, off_t offset_unused)
{
    struct binary_context *bcp = context;

    bcp->output_rec(bcp->output, rec_type, buf, len);
}

void *binary_filter_create(void)
{
    struct binary_context *bcp;

    bcp = (struct binary_context *) mymalloc(sizeof *bcp);
    bcp->output = NULL;
    bcp->output_context = NULL;
    bcp->output_rec = NULL;
    bcp->mime_state = mime_state_alloc(MIME_OPT_DOWNGRADE_BASE64,
				       binary_head_out, NULL,
				       binary_body_out, NULL,
				       NULL, bcp);
    return bcp;
}

void binary_filter_start(void *context, VSTREAM *output,
			 int (*output_rec)(VSTREAM *stream, int rec_type,
					   const char *data, ssize_t len))
{
    struct binary_context *bcp = context;

    if (bcp->output == NULL)
	bcp->output = output;
    else if (bcp->output != output)	/* sanity check */
	msg_panic("binary_filter_start: output mismatch");

    vstream_control(output,
		    VSTREAM_CTL_CONTEXT_GET, &bcp->output_context,
		    VSTREAM_CTL_CONTEXT, bcp,
		    VSTREAM_CTL_END);
    bcp->output_rec = output_rec;
}

int binary_filter_rec_put(VSTREAM *stream, int rec_type, const char *data,
			  ssize_t len)
{
    struct binary_context *bcp = NULL;
    int err;

    vstream_control(stream,
		    VSTREAM_CTL_CONTEXT_GET, &bcp,
		    VSTREAM_CTL_END);
    if (bcp == NULL || bcp->output != stream)	/* sanity check */
	msg_panic("binary_filter_rec_put: output mismatch");

    vstream_control(stream,
		    VSTREAM_CTL_CONTEXT, bcp->output_context,
		    VSTREAM_CTL_END);
    err = mime_state_update(bcp->mime_state, rec_type, data, len);
    vstream_control(stream,
		    VSTREAM_CTL_CONTEXT, bcp,
		    VSTREAM_CTL_END);

    return err ? REC_TYPE_ERROR : rec_type;
}

int binary_filter_flush(void *context, VSTREAM *stream)
{
    struct binary_context *bcp = NULL;
    int err;

    vstream_control(stream,
		    VSTREAM_CTL_CONTEXT_GET, &bcp,
		    VSTREAM_CTL_END);
    if (bcp == NULL || bcp->output != stream)	/* sanity check */
	msg_panic("binary_filter_rec_put: output mismatch");

    vstream_control(stream,
		    VSTREAM_CTL_CONTEXT, bcp->output_context,
		    VSTREAM_CTL_END);
    err = mime_state_flush(bcp->mime_state);
    vstream_control(stream,
		    VSTREAM_CTL_CONTEXT, bcp,
		    VSTREAM_CTL_END);

    return err ? REC_TYPE_ERROR : 0;
}

void binary_filter_stop(void *context, VSTREAM *stream)
{
    struct binary_context *bcp = NULL;

    vstream_control(stream,
		    VSTREAM_CTL_CONTEXT_GET, &bcp,
		    VSTREAM_CTL_END);
    if (bcp != context || bcp->output != stream)	/* sanity check */
	msg_panic("binary_filter_deinit: output mismatch");

    vstream_control(stream,
		    VSTREAM_CTL_CONTEXT, bcp->output_context,
		    VSTREAM_CTL_END);
}

void binary_filter_destroy(void *context)
{
    struct binary_context *bcp = context;

    mime_state_free(bcp->mime_state);
    myfree((char *) bcp);
}

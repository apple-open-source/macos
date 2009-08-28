/* Copyright © 2006 Ian Osgood
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 * 
 * Except as contained in this notice, the names of the authors or their
 * institutions shall not be used in advertising or otherwise to promote the
 * sale, use or other dealings in this Software without prior written
 * authorization from the authors.
 */

#include <stdlib.h>
#include <string.h>

#include "xcb_renderutil.h"

typedef struct _glyph_header_t {
	uint8_t count;
	uint8_t pad0[3];
	int16_t dx, dy;
} _glyph_header_t;

/* implementation of the opaque stream */
struct xcb_render_util_composite_text_stream_t {
	/* state info */
	uint32_t glyph_size;		/* 0 for unset, 1/2/4 for 8/16/32 */
	xcb_render_glyphset_t initial_glyphset;
	xcb_render_glyphset_t current_glyphset;

	/* dynamically allocated stream */
	/* contents are 32-bit aligned, network byte order */
	size_t stream_len;
	uint32_t *stream;
	uint32_t *current;
};

#define CURRENT_LEN(s) (((char *)s->current - (char *)s->stream))

xcb_render_util_composite_text_stream_t *
xcb_render_util_composite_text_stream (
	xcb_render_glyphset_t initial_glyphset,
	uint32_t              total_glyphs,
	uint32_t              total_glyphset_changes )
{
	xcb_render_util_composite_text_stream_t *stream;
	size_t size = 32;

	/* assume worst case: each glyph has its own dx,dy */
	if (total_glyphs || total_glyphset_changes) {
		size = total_glyphs * 3 * sizeof(uint32_t)
		     + total_glyphset_changes * 3 * sizeof(uint32_t);
	}

	stream = malloc(sizeof(xcb_render_util_composite_text_stream_t));

	stream->glyph_size = 0;
	stream->initial_glyphset = initial_glyphset;
	stream->current_glyphset = initial_glyphset;

	stream->stream_len = size;
	stream->stream = malloc(size);
	stream->current = stream->stream;

	return stream;
}

static void
_grow_stream( xcb_render_util_composite_text_stream_t *stream, size_t increase )
{
	size_t current_len = CURRENT_LEN(stream);
	if (current_len + increase > stream->stream_len) {
		uint32_t *s = realloc(stream->stream, 2 * stream->stream_len);
		if (s != NULL) {
			stream->stream_len *= 2;
			stream->stream = s;
			stream->current = stream->stream + (current_len>>2);
		}
	}
}

void
xcb_render_util_glyphs_8 (
	xcb_render_util_composite_text_stream_t *stream,
	int16_t  dx,
	int16_t  dy,
	uint32_t count,
	const uint8_t *glyphs )
{
	_glyph_header_t header = { count, {0,0,0}, dx, dy };

	if (count > 252) return; /* FIXME */

	if (stream->glyph_size != sizeof(*glyphs)) {
		if (stream->glyph_size != 0)
			return;
		stream->glyph_size = sizeof(*glyphs);
	}
	_grow_stream(stream, sizeof(header) + count+3);

	memcpy(stream->current, &header, sizeof(header));
	stream->current += 2;

	memcpy(stream->current, glyphs, header.count);
	stream->current += ((int)header.count+3)>>2;
}

void
xcb_render_util_glyphs_16 (
	xcb_render_util_composite_text_stream_t *stream,
	int16_t  dx,
	int16_t  dy,
	uint32_t count,
	const uint16_t *glyphs )
{
	_glyph_header_t header = { count, {0,0,0}, dx, dy };

	if (count > 254) return; /* FIXME */

	if (stream->glyph_size != sizeof(*glyphs)) {
		if (stream->glyph_size != 0)
			return;
		stream->glyph_size = sizeof(*glyphs);
	}
	_grow_stream(stream, sizeof(header) + count*sizeof(*glyphs)+1);

	memcpy(stream->current, &header, sizeof(header));
	stream->current += 2;

	memcpy(stream->current, glyphs, header.count*sizeof(*glyphs));
	stream->current += ((int)header.count*sizeof(*glyphs)+3)>>2;
}

void
xcb_render_util_glyphs_32 (
	xcb_render_util_composite_text_stream_t *stream,
	int16_t  dx,
	int16_t  dy,
	uint32_t count,
	const uint32_t *glyphs )
{
	_glyph_header_t header = { count, {0,0,0}, dx, dy };

	if (count > 254) return; /* FIXME */

	if (stream->glyph_size != sizeof(*glyphs)) {
		if (stream->glyph_size != 0)
			return;
		stream->glyph_size = sizeof(*glyphs);
	}
	_grow_stream(stream, sizeof(header) + count*sizeof(*glyphs)+1);

	memcpy(stream->current, &header, sizeof(header));
	stream->current += 2;

	memcpy(stream->current, glyphs, header.count*sizeof(*glyphs));
	stream->current += header.count;
}

/* note: these glyph arrays must be swapped to network byte order */

void
xcb_render_util_change_glyphset (
	xcb_render_util_composite_text_stream_t *stream,
	xcb_render_glyphset_t glyphset )
{
	static _glyph_header_t header = { 255, {0,0,0}, 0, 0 };

	if (glyphset == stream->current_glyphset)
		return;

	_grow_stream(stream, 3*sizeof(uint32_t));

	memcpy(stream->current, &header, sizeof(header));
	stream->current += 2;

	*stream->current = glyphset;
	stream->current++;

	stream->current_glyphset = glyphset;
}
	
typedef xcb_void_cookie_t
(*xcb_render_composite_glyphs_func) (xcb_connection_t        *c,
	                             uint8_t                  op,
                                     xcb_render_picture_t     src,
                                     xcb_render_picture_t     dst,
                                     xcb_render_pictformat_t  mask_format,
                                     xcb_render_glyphset_t    glyphset,
                                     int16_t                  src_x,
                                     int16_t                  src_y,
                                     uint32_t                 glyphcmds_len,
                                     const uint8_t           *glyphcmds);


xcb_void_cookie_t
xcb_render_util_composite_text (
	xcb_connection_t        *xc,
	uint8_t                  op,
	xcb_render_picture_t     src,
	xcb_render_picture_t     dst,
	xcb_render_pictformat_t  mask_format,
	int16_t                  src_x,
	int16_t                  src_y,
	xcb_render_util_composite_text_stream_t *stream )
{
	xcb_render_composite_glyphs_func f;

	switch (stream->glyph_size)
	{
	case 1:
		f = xcb_render_composite_glyphs_8;
		break;
	case 2:
		f = xcb_render_composite_glyphs_16;
		break;
	case 4:
		f = xcb_render_composite_glyphs_32;
		break;
	default: /* uninitialized */
		return xcb_no_operation(xc);
	}
	return f(
		xc, op, src, dst, mask_format,
		stream->initial_glyphset,
		src_x, src_y,
		CURRENT_LEN(stream),
		(uint8_t *)stream->stream
	);
}

xcb_void_cookie_t
xcb_render_util_composite_text_checked (
	xcb_connection_t        *xc,
	uint8_t                  op,
	xcb_render_picture_t     src,
	xcb_render_picture_t     dst,
	xcb_render_pictformat_t  mask_format,
	int16_t                  src_x,
	int16_t                  src_y,
	xcb_render_util_composite_text_stream_t *stream )
{
	xcb_render_composite_glyphs_func f;

	switch (stream->glyph_size)
	{
	case 1:
		f = xcb_render_composite_glyphs_8_checked;
		break;
	case 2:
		f = xcb_render_composite_glyphs_16_checked;
		break;
	case 4:
		f = xcb_render_composite_glyphs_32_checked;
		break;
	default: /* uninitialized */
		return xcb_no_operation_checked(xc);
	}
	return f(
		xc, op, src, dst, mask_format,
		stream->initial_glyphset,
		src_x, src_y,
		CURRENT_LEN(stream),
		(uint8_t *)stream->stream
	);
}

void
xcb_render_util_composite_text_free (
	xcb_render_util_composite_text_stream_t *stream )
{
	free(stream->stream);
	free(stream);
}

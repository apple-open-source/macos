/*
 * Copyright © 2008 Bart Massey <bart@cs.pdx.edu>
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * Except as contained in this notice, the names of the authors or
 * their institutions shall not be used in advertising or otherwise to
 * promote the sale, use or other dealings in this Software without
 * prior written authorization from the authors.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <xcb/xcb.h>
#include "../aux/xcb_aux.h"
#include "../aux/xcb_bitops.h"
#include "xcb_image.h"

#define WIDTH 50
#define HEIGHT 50

static uint32_t
color (uint32_t depth, uint32_t x, uint32_t y)
{
	uint32_t p;

	if (depth == 1) {
		extern long random();
		int frac = random() % (WIDTH * HEIGHT);
		p = x * y >= frac;
		return p;
	}
	depth /= 3;
	p =  ((1 << depth) - 1) * x * y / WIDTH / HEIGHT;
	return (p << depth) | (p << (2 * depth));
}

static xcb_image_t *create_image(xcb_connection_t *c, int depth, int format)
{
	xcb_image_t *im;
	int x, y;
	printf("Image depth %d, format %d\n", depth, format);
	im = xcb_image_create_native(c, WIDTH, HEIGHT,
				     format, depth, 0, 0, 0);
	for(x = 0; x < WIDTH; ++x)
		for(y = 0; y < HEIGHT; ++y)
			xcb_image_put_pixel(im, x, y, color(depth, x, y));
	return im;
}

static xcb_window_t create_window(xcb_connection_t *c, xcb_screen_t *root)
{
	static const uint32_t mask = XCB_CW_EVENT_MASK;
	static const uint32_t values[] = { XCB_EVENT_MASK_EXPOSURE };
	unsigned int seq;
	xcb_window_t w = xcb_generate_id(c);
	seq = xcb_create_window(c, root->root_depth, w, root->root, 30, 30, WIDTH, HEIGHT, 0, XCB_WINDOW_CLASS_INPUT_OUTPUT, root->root_visual, mask, values).sequence;
	printf("CreateWindow sequence %d, depth %d\n", seq, root->root_depth);
	seq = xcb_map_window(c, w).sequence;
	printf("MapWindow sequence %d\n", seq);
	return w;
}

static xcb_pixmap_t create_pixmap(xcb_connection_t *c, xcb_drawable_t d, uint8_t depth)
{
	xcb_pixmap_t p = xcb_generate_id(c);
	unsigned int seq;
	seq = xcb_create_pixmap(c, depth, p, d, WIDTH, HEIGHT).sequence;
	printf("CreatePixmap sequence %d, depth %d\n", seq, depth);
	return p;
}

static xcb_gcontext_t create_gcontext(xcb_connection_t *c,
				      xcb_drawable_t d,
				      xcb_screen_t *root)
{
	static const uint32_t mask = XCB_GC_FOREGROUND | XCB_GC_BACKGROUND;
	const uint32_t values[] = { root->black_pixel, 0xffff };
	unsigned int seq;
	xcb_gcontext_t gc = xcb_generate_id(c);
	seq = xcb_create_gc(c, gc, d, mask, values).sequence;
	printf("CreateGC sequence %d\n", seq);
	return gc;
}


typedef struct {
	char *name;
	xcb_image_format_t format;
	uint8_t depth;
} format_t;

static format_t formats[] = {
	{"z-pixmap", XCB_IMAGE_FORMAT_Z_PIXMAP, 24},
	{"xy-bitmap", XCB_IMAGE_FORMAT_XY_BITMAP, 1},
	{"xy-pixmap-1", XCB_IMAGE_FORMAT_XY_PIXMAP, 1},
	{"xy-pixmap-24", XCB_IMAGE_FORMAT_XY_PIXMAP, 24},
	{0, 0, 0}
};

static format_t *
parse_format (char *name) {
	format_t *f;
	for (f = formats; f->name; f++)
		if (!strcmp(name, f->name))
			return f;
	fprintf(stderr, "%s: bad format: known formats are:\n", name);
	for (f = formats; f->name; f++)
		fprintf(stderr, "\t%s\n", f->name);
	exit(1);
}

int main(int argc, char **argv)
{
	int screen, depth;
	format_t *format = &formats[0];
	xcb_screen_t *root;
	xcb_visualtype_t *visual;
	xcb_image_t *im;
	xcb_drawable_t d, w = XCB_NONE;
	xcb_gcontext_t dgc, wgc = 0;
	xcb_generic_event_t *ev;
	xcb_connection_t *c = xcb_connect(0, &screen);
	if(!c)
	{
		printf("Connection failed.\n");
		exit(1);
	}
	root = xcb_aux_get_screen(c, screen);
	assert(root);
	visual = xcb_aux_find_visual_by_id(root, root->root_visual);
	assert(visual);
	if(argc > 1)
		format = parse_format(argv[1]);
	if (root->root_depth != 24 ||
	    visual->_class != XCB_VISUAL_CLASS_TRUE_COLOR)
	{
		printf("Only 24 bit TrueColor visuals for now\n");
		exit(1);
	}
	depth = format->depth;

	im = create_image(c, depth, format->format);
	d = create_window(c, root);
	if(format->format == XCB_IMAGE_FORMAT_XY_PIXMAP && depth == 1)
	{
		w = d;
		d = create_pixmap(c, w, depth);
	}
	dgc = create_gcontext(c, d, root);
	if (w)
	    wgc = create_gcontext(c, w, root);
	xcb_flush(c);

	if(im)
	{
		while((ev = xcb_wait_for_event(c)))
		{
			if(ev->response_type == XCB_EXPOSE && ((xcb_expose_event_t *) ev)->count == 0)
			{
				printf("ImagePut sequence %d\n", xcb_image_put(c, d, dgc, im, 0, 0, 0).sequence);
				if(w)
				{
					unsigned int seq;
					seq = xcb_copy_plane(c, d, w, wgc,
							     0, 0, 0, 0,
							     WIDTH, HEIGHT, 1).sequence;
					printf("CopyPlane sequence %d\n", seq);
				}
				xcb_flush(c);
			}
			else if(ev->response_type == 0)
			{
				xcb_generic_error_t *err = (xcb_generic_error_t *) ev;
				printf("Error: %d after sequence %d\n", err->error_code, (unsigned int) err->full_sequence);
			}
			free(ev);
		}
		xcb_image_destroy(im);
	}

	xcb_disconnect(c);
	exit(0);
}

/*
 * Copyright © 2008 Bart Massey <bart@cs.pdx.edu>
 * Copyright © 2008 Julien Danjou <julien@danjou.info>
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <xcb/xcb.h>
#include "../aux/xcb_aux.h"
#include "../event/xcb_event.h"
#include "xcb_image.h"

#include "test.xbm"

static xcb_window_t make_window(xcb_connection_t *c,
				xcb_screen_t *s,
				uint32_t bg,
				uint32_t fg,
				uint32_t width,
				uint32_t height) {
    uint32_t mask = 0;
    xcb_params_cw_t cwa;
    xcb_window_t w;
    xcb_void_cookie_t check_cookie;
    xcb_generic_error_t *error;
    xcb_visualtype_t *v = xcb_aux_find_visual_by_id(s, s->root_visual);
    assert(v);
    XCB_AUX_ADD_PARAM(&mask, &cwa, back_pixel, bg);
    XCB_AUX_ADD_PARAM(&mask, &cwa, border_pixel, fg);
    XCB_AUX_ADD_PARAM(&mask, &cwa, override_redirect, 1);
    XCB_AUX_ADD_PARAM(&mask, &cwa, event_mask,
		      XCB_EVENT_MASK_BUTTON_PRESS |
		      XCB_EVENT_MASK_EXPOSURE);
    w = xcb_generate_id(c);
    check_cookie = xcb_aux_create_window_checked(c,
      s->root_depth, w, s->root, 0, 0, width, height, 1,
      XCB_WINDOW_CLASS_INPUT_OUTPUT, v->visual_id, mask, &cwa);
    error = xcb_request_check(c, check_cookie);
    assert(!error);
    check_cookie = xcb_map_window_checked(c, w);
    error = xcb_request_check(c, check_cookie);
    assert(!error);
    return w;
}

void process_events(xcb_connection_t *c,
		    xcb_gcontext_t g,
		    xcb_window_t w,
		    xcb_pixmap_t p,
		    uint32_t width,
		    uint32_t height) {
    xcb_generic_event_t *e;
    xcb_void_cookie_t cookie;

    while ((e = xcb_wait_for_event(c))) {
	uint32_t r = XCB_EVENT_RESPONSE_TYPE(e);
	xcb_generic_error_t *err;
	
	fprintf(stderr, "event %d\n", r);
	switch (r) {
	case XCB_EXPOSE:
	case XCB_MAP_NOTIFY:
	    cookie = xcb_copy_area_checked(c, p, w, g,
					   0, 0, 0, 0,
					   width, height);
	    assert(!xcb_request_check(c, cookie));
	    break;
	case XCB_BUTTON_PRESS:
	    exit(0);
	    break;
	case 0:
	    err = (xcb_generic_error_t *) e;
	    printf("error: %d (sequence %d)\n",
		   err->error_code, (unsigned int) err->full_sequence);
	    exit(1);
	default:
	    break;
	}
	free(e);
    }
}
		       
#define INSET_X 31
#define INSET_Y 32

int main(int argc, char **argv) {
    uint32_t width = test_width - 2 * INSET_X;
    uint32_t height = test_height - 2 * INSET_Y;
    int snum;
    xcb_void_cookie_t check_cookie;
    xcb_window_t w;
    xcb_gcontext_t gc;
    xcb_pixmap_t pix;
    xcb_connection_t *c = xcb_connect(0, &snum);
    xcb_screen_t *s = xcb_aux_get_screen(c, snum);
    xcb_alloc_named_color_cookie_t bg_cookie =
	xcb_alloc_named_color(c, s->default_colormap,
			      strlen("white"), "white");
    xcb_alloc_named_color_cookie_t fg_cookie =
	xcb_alloc_named_color(c, s->default_colormap,
			      strlen("black"), "black");
    xcb_alloc_named_color_reply_t *bg_reply =
	xcb_alloc_named_color_reply(c, bg_cookie, 0);
    xcb_alloc_named_color_reply_t *fg_reply =
	xcb_alloc_named_color_reply(c, fg_cookie, 0);
    uint32_t fg, bg;
    xcb_image_t *image, *native_image, *subimage;
    uint32_t mask = 0;
    xcb_params_gc_t gcv;

    assert(bg_reply && fg_reply);
    bg = bg_reply->pixel;
    fg = fg_reply->pixel;
    free(bg_reply);
    free(fg_reply);
    w = make_window(c, s, bg, fg, width, height);
    gc = xcb_generate_id(c);
    check_cookie = xcb_create_gc_checked(c, gc, w, 0, 0);
    assert(!xcb_request_check(c, check_cookie));
    image = xcb_image_create_from_bitmap_data((uint8_t *)test_bits,
					      test_width, test_height);
    native_image = xcb_image_native(c, image, 1);
    assert(native_image);
    if (native_image != image)
	xcb_image_destroy(image);
    subimage = xcb_image_subimage(native_image, INSET_X, INSET_Y,
				  width, height,
				  0, 0, 0);
    assert(subimage);
    xcb_image_destroy(native_image);
    subimage->format = XCB_IMAGE_FORMAT_XY_BITMAP;
    pix = xcb_generate_id(c);
    xcb_create_pixmap(c, s->root_depth, pix, w,
		      subimage->width, subimage->height);
    gc = xcb_generate_id(c);
    XCB_AUX_ADD_PARAM(&mask, &gcv, foreground, fg);
    XCB_AUX_ADD_PARAM(&mask, &gcv, background, bg);
    xcb_aux_create_gc(c, gc, pix, mask, &gcv);
    xcb_image_put(c, pix, gc, subimage, 0, 0, 0);
    process_events(c, gc, w, pix, width, height);
    xcb_disconnect(c);
    return 1;
}

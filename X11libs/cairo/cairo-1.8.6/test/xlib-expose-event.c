/*
 * Copyright © 2008 Chris Wilson
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without
 * fee, provided that the above copyright notice appear in all copies
 * and that both that copyright notice and this permission notice
 * appear in supporting documentation, and that the name of
 * Chris Wilson not be used in advertising or publicity pertaining to
 * distribution of the software without specific, written prior
 * permission. Chris Wilson makes no representations about the
 * suitability of this software for any purpose.  It is provided "as
 * is" without express or implied warranty.
 *
 * CHRIS WILSON DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS
 * SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS, IN NO EVENT SHALL CHRIS WILSON BE LIABLE FOR ANY SPECIAL,
 * INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER
 * RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR
 * IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Author: Chris Wilson <chris@chris-wilson.co.uk>
 */

/* This test tries to emulate the behaviour of most toolkits; it tries
 * to simulate typical usage of a single surface with multiple exposures.
 *
 * The first goal of the test is to reproduce the XSetClipMask(NULL) bug
 * reintroduced in 1.6.2 (but was originally fixed in 40558cb15). As I've
 * made the same mistake again, it is worth adding a regression test...
 */


#include <stdio.h>
#include <stdlib.h>

#include "cairo.h"
#include "cairo-xlib.h"
#include "cairo-test.h"

#include "cairo-boilerplate-xlib.h"

#include "buffer-diff.h"

#define SIZE 160
#define NLOOPS 10

static const char	png_filename[]	= "romedalen.png";

static cairo_bool_t
check_visual (Display *dpy)
{
    Visual *visual = DefaultVisual (dpy, DefaultScreen (dpy));

    if ((visual->red_mask   == 0xff0000 &&
	 visual->green_mask == 0x00ff00 &&
	 visual->blue_mask  == 0x0000ff) ||
	(visual->red_mask   == 0x0000ff &&
	 visual->green_mask == 0x00ff00 &&
	 visual->blue_mask  == 0xff0000))
	return 1;
    else
	return 0;
}

static void
clear (cairo_surface_t *surface)
{
    cairo_t *cr = cairo_create (surface);
    cairo_set_operator (cr, CAIRO_OPERATOR_CLEAR);
    cairo_paint (cr);
    cairo_destroy (cr);
}

static void
draw_mask (cairo_t *cr)
{
    cairo_surface_t *surface;
    cairo_t *cr2;

    surface = cairo_surface_create_similar (cairo_get_group_target (cr),
	                                    CAIRO_CONTENT_ALPHA,
					    50, 50);
    cairo_boilerplate_xlib_surface_disable_render (surface);

    cr2 = cairo_create (surface);
    cairo_surface_destroy (surface);

    /* This complex clip and forcing of fallbacks is to reproduce bug
     * http://bugs.freedesktop.org/show_bug.cgi?id=10921
     */
    cairo_rectangle (cr2,
	             0, 0,
	             40, 40);
    cairo_rectangle (cr2,
	             10, 10,
	             40, 40);
    cairo_clip (cr2);

    cairo_move_to (cr2, 0, 25);
    cairo_line_to (cr2, 50, 25);
    cairo_move_to (cr2, 25, 0);
    cairo_line_to (cr2, 25, 50);
    cairo_set_source_rgb (cr2, 1, 1, 1);
    cairo_stroke (cr2);

    cairo_set_source_rgb (cr, 1, 0, 0);
    cairo_mask_surface (cr, cairo_get_target (cr2), 50, 50);
    cairo_destroy (cr2);
}

static cairo_surface_t *
clone_similar_surface (cairo_surface_t * target, cairo_surface_t *surface)
{
    cairo_t *cr;
    cairo_surface_t *similar;

    similar = cairo_surface_create_similar (target,
	                              cairo_surface_get_content (surface),
				      cairo_image_surface_get_width (surface),
				      cairo_image_surface_get_height (surface));

    cr = cairo_create (similar);
    cairo_surface_destroy (similar);

    cairo_set_source_surface (cr, surface, 0, 0);
    cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
    cairo_paint (cr);

    similar = cairo_surface_reference (cairo_get_target (cr));
    cairo_destroy (cr);

    return similar;
}

static void
draw_image (const cairo_test_context_t *ctx, cairo_t *cr)
{
    cairo_surface_t *surface, *similar;

    surface = cairo_test_create_surface_from_png (ctx, png_filename);
    similar = clone_similar_surface (cairo_get_group_target (cr), surface);
    cairo_surface_destroy (surface);

    cairo_set_source_surface (cr, similar, 0, 0);
    cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
    cairo_paint (cr);
    cairo_surface_destroy (similar);
}

static void
draw (const cairo_test_context_t *ctx,
      cairo_surface_t *surface,
      cairo_rectangle_t *region,
      int n_regions)
{
    cairo_t *cr = cairo_create (surface);
    if (region != NULL) {
	int i;
	for (i = 0; i < n_regions; i++) {
	    cairo_rectangle (cr,
			     region[i].x, region[i].y,
			     region[i].width, region[i].height);
	}
	cairo_clip (cr);
    }
    cairo_push_group (cr);
    draw_image (ctx, cr);
    draw_mask (cr);
    cairo_pop_group_to_source (cr);
    cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
    cairo_paint (cr);
    cairo_destroy (cr);
}

static cairo_test_status_t
compare (const cairo_test_context_t *ctx, cairo_surface_t *surface)
{
    cairo_t *cr;
    cairo_surface_t *image, *reference, *diff;
    cairo_status_t status;
    buffer_diff_result_t result;

    diff = cairo_image_surface_create (CAIRO_FORMAT_RGB24, SIZE, SIZE);

    /* copy the pixmap to an image buffer */
    image = cairo_image_surface_create (CAIRO_FORMAT_RGB24, SIZE, SIZE);
    cr = cairo_create (image);
    cairo_set_source_surface (cr, surface, 0, 0);
    cairo_paint (cr);
    cairo_destroy (cr);
    cairo_surface_write_to_png (image, "xlib-expose-event-out.png");

    reference = cairo_test_create_surface_from_png (ctx, "xlib-expose-event-ref.png");
    status = image_diff (ctx, reference, image, diff, &result);

    cairo_surface_destroy (reference);
    cairo_surface_destroy (image);
    cairo_surface_destroy (diff);

    return status == CAIRO_STATUS_SUCCESS && ! result.pixels_changed ?
	CAIRO_TEST_SUCCESS : CAIRO_TEST_FAILURE;
}

int
main (void)
{
    cairo_test_context_t ctx;
    Display *dpy;
    Drawable drawable;
    int screen;
    cairo_surface_t *surface;
    cairo_rectangle_t region[4];
    int i, j;
    cairo_test_status_t result = CAIRO_TEST_UNTESTED;

    cairo_test_init (&ctx, "xlib-expose-event");
    if (! cairo_test_is_target_enabled (&ctx, "xlib"))
	goto CLEANUP_TEST;

    dpy = XOpenDisplay (NULL);
    if (dpy == NULL) {
	cairo_test_log (&ctx, "xlib-expose-event: Cannot open display, skipping\n");
	goto CLEANUP_TEST;
    }

    if (! check_visual (dpy)) {
	cairo_test_log (&ctx, "xlib-expose-event: default visual is not RGB24 or BGR24, skipping\n");
	goto CLEANUP_DISPLAY;
    }

    screen = DefaultScreen (dpy);
    drawable = XCreatePixmap (dpy, DefaultRootWindow (dpy),
			      SIZE, SIZE, DefaultDepth (dpy, screen));
    surface = cairo_xlib_surface_create (dpy,
					 drawable,
					 DefaultVisual (dpy, screen),
					 SIZE, SIZE);
    clear (surface);
    draw (&ctx, surface, NULL, 0);
    for (i = 0; i < NLOOPS; i++) {
	for (j = 0; j < NLOOPS; j++) {
	    region[0].x = i * SIZE / NLOOPS;
	    region[0].y = i * SIZE / NLOOPS;
	    region[0].width = SIZE / 4;
	    region[0].height = SIZE / 4;

	    region[1].x = j * SIZE / NLOOPS;
	    region[1].y = j * SIZE / NLOOPS;
	    region[1].width = SIZE / 4;
	    region[1].height = SIZE / 4;

	    region[2].x = i * SIZE / NLOOPS;
	    region[2].y = j * SIZE / NLOOPS;
	    region[2].width = SIZE / 4;
	    region[2].height = SIZE / 4;

	    region[3].x = j * SIZE / NLOOPS;
	    region[3].y = i * SIZE / NLOOPS;
	    region[3].width = SIZE / 4;
	    region[3].height = SIZE / 4;

	    draw (&ctx, surface, region, 4);
	}
    }

    result = compare (&ctx, surface);

    cairo_surface_destroy (surface);

    XFreePixmap (dpy, drawable);

  CLEANUP_DISPLAY:
    XCloseDisplay (dpy);

  CLEANUP_TEST:
    cairo_test_fini (&ctx);

    return result;
}


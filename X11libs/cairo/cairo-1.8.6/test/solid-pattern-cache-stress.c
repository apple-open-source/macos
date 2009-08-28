/*
 * Copyright © 2007 Chris Wilson.
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without
 * fee, provided that the above copyright notice appear in all copies
 * and that both that copyright notice and this permission notice
 * appear in supporting documentation, and that the name of
 * Chris Wilson. Not be used in advertising or publicity pertaining to
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
 * Author: Chris Wilson <chris at chris-wilson.co.uk>
 */

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include "cairo-test.h"
#include <stdlib.h> /* drand48() */

#define LOOPS 10
#define NRAND 100

#ifndef HAVE_DRAND48
#define drand48() (rand () / (double) RAND_MAX)
#endif

static cairo_test_draw_function_t draw;

static const cairo_test_t test = {
    "solid-pattern-cache-stress",
    "Stress the solid pattern cache and ensure it behaves",
    1, 1,
    draw
};

static cairo_t *
_cairo_create_similar (cairo_t *cr, int width, int height)
{
    cairo_surface_t *similar;

    similar = cairo_surface_create_similar (cairo_get_target (cr),
	                                    cairo_surface_get_content (cairo_get_target (cr)),
				            width, height);
    cr = cairo_create (similar);
    cairo_surface_destroy (similar);

    return cr;
}

static cairo_t *
_cairo_create_image (cairo_t *cr, cairo_format_t format, int width, int height)
{
    cairo_surface_t *image;

    image = cairo_image_surface_create (format, width, height);
    cr = cairo_create (image);
    cairo_surface_destroy (image);

    return cr;
}

static void
_draw (cairo_t *cr,
       double red,
       double green,
       double blue)
{
    cairo_text_extents_t extents;

    cairo_set_source_rgb (cr, red, green, blue);
    cairo_paint (cr);

    cairo_move_to (cr, 0, 0);
    cairo_line_to (cr, 1, 1);
    cairo_stroke (cr);

    cairo_mask (cr, cairo_get_source (cr));

    cairo_text_extents (cr, "cairo", &extents);
    cairo_move_to (cr,
	           -extents.x_bearing - .5 * extents.width,
		   -extents.y_bearing - .5 * extents.height);
    cairo_show_text (cr, "cairo");

}

static void
use_similar (cairo_t *cr,
	    double red,
	    double green,
	    double blue)
{
    cr = _cairo_create_similar (cr, 1, 1);

    _draw (cr, red, green, blue);

    cairo_destroy (cr);
}

static void
use_image (cairo_t *cr,
	   cairo_format_t format,
	   double red,
	   double green,
	   double blue)
{
    cr = _cairo_create_image (cr, format, 1, 1);

    _draw (cr, red, green, blue);

    cairo_destroy (cr);
}

static void
use_solid (cairo_t *cr,
	   double red,
	   double green,
	   double blue)
{
    /* mix in dissimilar solids */
    use_image (cr, CAIRO_FORMAT_A1, red, green, blue);
    use_image (cr, CAIRO_FORMAT_A8, red, green, blue);
    use_image (cr, CAIRO_FORMAT_RGB24, red, green, blue);
    use_image (cr, CAIRO_FORMAT_ARGB32, red, green, blue);

    use_similar (cr, red, green, blue);

    _draw (cr, red, green, blue);
}

static cairo_test_status_t
draw (cairo_t *cr, int width, int height)
{
    int loop;
    int i;

    for (loop = 0; loop < LOOPS; loop++) {
	for (i = 0; i < LOOPS; i++) {
	    use_solid (cr, 0.0, 0.0, 0.0); /* black */
	    use_solid (cr, 1.0, 0.0, 0.0); /* red */
	    use_solid (cr, 0.0, 1.0, 0.0); /* green */
	    use_solid (cr, 1.0, 1.0, 0.0); /* yellow */
	    use_solid (cr, 0.0, 0.0, 1.0); /* blue */
	    use_solid (cr, 1.0, 0.0, 1.0); /* magenta */
	    use_solid (cr, 0.0, 1.0, 1.0); /* cyan */
	    use_solid (cr, 1.0, 1.0, 1.0); /* white */
	}

	for (i = 0; i < NRAND; i++)
	    use_solid (cr, drand48 (), drand48 (), drand48 ());
    }

    /* stress test only, so clear the surface before comparing */
    cairo_set_source_rgb (cr, 0, 0, 1);
    cairo_paint (cr);

    return CAIRO_TEST_SUCCESS;
}

int
main (void)
{
    return cairo_test (&test);
}

/*
 * Copyright © 2008 Keith Packard <keithp@keithp.com>
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
#include "xcb_image.h"

xcb_image_format_t  formats[] = {
    XCB_IMAGE_FORMAT_Z_PIXMAP,
    XCB_IMAGE_FORMAT_XY_PIXMAP,
    XCB_IMAGE_FORMAT_XY_BITMAP,
};
#define SIZE(a) (sizeof (a) / sizeof (a[0]))
#define NFORMAT SIZE(formats)

int bpps[] = {
    1, 4, 8, 16, 32
};
#define NBPP SIZE(bpps)

int units[] = {
    8, 16, 32
};
#define NUNIT SIZE(units)

xcb_image_order_t   byte_orders[] = {
    XCB_IMAGE_ORDER_LSB_FIRST,
    XCB_IMAGE_ORDER_MSB_FIRST
};
#define NBYTE_ORDER SIZE(byte_orders)

static
uint32_t pixel_mask (int bpp)
{
    if (bpp == 32)
	return 0xffffffff;
    return (1 << bpp) - 1;
}

int
compare_image(xcb_image_t *a, xcb_image_t *b)
{
    int x, y;
    uint32_t	a_pixel, b_pixel;
    uint32_t	mask = pixel_mask (a->bpp) & pixel_mask (b->bpp);

    for (y = 0; y < a->height; y++)
	for (x = 0; x < a->width; x++) {
	    a_pixel = xcb_image_get_pixel (a, x, y) & mask;
	    b_pixel = xcb_image_get_pixel (b, x, y) & mask;
	    if (a_pixel != b_pixel) {
		fprintf (stderr, "fail at %d,%d: 0x%x != 0x%x\n",
			 x, y, a_pixel, b_pixel);
		return 0;
	    }
	}
    return 1;
}

#define test_width  63
#define test_height 2

static xcb_image_t *
create_test_image (void)
{
    xcb_image_t	*test_image;
    int		x, y;
    uint32_t	pixel;
    test_image = xcb_image_create(test_width, test_height,
				  XCB_IMAGE_FORMAT_Z_PIXMAP,
				  32,
				  32,
				  32,
				  32,
				  XCB_IMAGE_ORDER_LSB_FIRST,
				  XCB_IMAGE_ORDER_LSB_FIRST,
				  NULL, 0, NULL);

    pixel = 0;
    for (y = 0; y < test_height; y++)
	for (x = 0; x < test_width; x++) {
	    xcb_image_put_pixel (test_image, x, y, pixel);
	    pixel++;
	}
    return test_image;
}

static void
convert_test (xcb_image_t *test, xcb_image_t *a)
{
    int		x, y;

    for (y = 0; y < test->height; y++)
	for (x = 0; x < test->width; x++)
	    xcb_image_put_pixel (a, x, y, xcb_image_get_pixel (test, x, y));
}

static char *
order_name (xcb_image_order_t order) {
  if (order == XCB_IMAGE_ORDER_MSB_FIRST)
    return "MSB";
  else
    return "LSB";
}

static void
print_format (xcb_image_t *image)
{
  switch (image->format) {
  case XCB_IMAGE_FORMAT_Z_PIXMAP: fprintf (stderr, "Z pixmap"); break;
  case XCB_IMAGE_FORMAT_XY_PIXMAP: fprintf (stderr, "XY pixmap"); break;
  case XCB_IMAGE_FORMAT_XY_BITMAP: fprintf (stderr, "XY bitmap"); break;
  }
  fprintf (stderr, " pad: %d bpp: %d depth: %d unit: %d planemask: 0x%08x",
	   image->scanline_pad, image->bpp, image->depth, image->unit,
	   image->plane_mask);
  fprintf (stderr, " byte order: %s bit order: %s stride: %d\n",
	   order_name (image->byte_order), order_name (image->bit_order),
	   image->stride);
}

int main (int argc, char **argv) {
  xcb_image_t	*test_image;
  xcb_image_t	*src_image;
  xcb_image_t	*dst_image;
  int		dst_format_i, dst_bpp_i, dst_unit_i, dst_byte_order_i, dst_bit_order_i;
  int		src_format_i, src_bpp_i, src_unit_i, src_byte_order_i, src_bit_order_i;
  xcb_image_format_t	dst_format, src_format;
  int		dst_bpp, src_bpp;
  int		dst_unit, src_unit;
  int		dst_byte_order, src_byte_order;
  int		dst_bit_order, src_bit_order;

  test_image = create_test_image ();

  for (dst_format_i = 0; dst_format_i < NFORMAT; dst_format_i++) {
    dst_format = formats[dst_format_i];
    for (src_format_i = 0; src_format_i < NFORMAT; src_format_i++) {
      src_format = formats[src_format_i];
      for (dst_bpp_i = 0; dst_bpp_i < NBPP; dst_bpp_i++) {
	dst_bpp = bpps[dst_bpp_i];
	for (src_bpp_i = 0; src_bpp_i < NBPP; src_bpp_i++) {
	  src_bpp = bpps[src_bpp_i];
	  for (dst_unit_i = 0; dst_unit_i < NUNIT; dst_unit_i++) {
	    dst_unit = units[dst_unit_i];
	    if (dst_format == XCB_IMAGE_FORMAT_Z_PIXMAP) {
	      if (dst_bpp == 4 && dst_unit != 8)
		continue;
	      if (dst_bpp > 4 && dst_unit != dst_bpp)
		continue;
	    }
	    if (dst_format == XCB_IMAGE_FORMAT_XY_BITMAP && dst_bpp != 1)
	      continue;
	    for (src_unit_i = 0; src_unit_i < NUNIT; src_unit_i++) {
	      src_unit = units[src_unit_i];
	      if (src_format == XCB_IMAGE_FORMAT_Z_PIXMAP) {
		if (src_bpp == 4 && src_unit != 8)
		  continue;
		if (src_bpp > 4 && src_unit != src_bpp)
		  continue;
	      }
	      if (src_format == XCB_IMAGE_FORMAT_XY_BITMAP && src_bpp != 1)
		continue;
	      for (dst_byte_order_i = 0; dst_byte_order_i < NBYTE_ORDER; dst_byte_order_i++) {
		dst_byte_order = byte_orders[dst_byte_order_i];
		for (src_byte_order_i = 0; src_byte_order_i < NBYTE_ORDER; src_byte_order_i++) {
		  src_byte_order = byte_orders[src_byte_order_i];
		  for (dst_bit_order_i = 0; dst_bit_order_i < NBYTE_ORDER; dst_bit_order_i++) {
		    dst_bit_order = byte_orders[dst_bit_order_i];
		    if (dst_format == XCB_IMAGE_FORMAT_Z_PIXMAP && dst_bit_order != dst_byte_order)
		      continue;
		    for (src_bit_order_i = 0; src_bit_order_i < NBYTE_ORDER; src_bit_order_i++) {
		      src_bit_order = byte_orders[src_bit_order_i];
		      if (src_format == XCB_IMAGE_FORMAT_Z_PIXMAP && src_bit_order != src_byte_order)
			continue;
		      src_image = xcb_image_create (test_width, test_height, src_format, 32, src_bpp, src_bpp, src_unit,
						    src_byte_order, src_bit_order, NULL, 0, NULL);
		      dst_image = xcb_image_create (test_width, test_height, dst_format, 32, dst_bpp, dst_bpp, dst_unit,
						    dst_byte_order, dst_bit_order, NULL, 0, NULL);
		      convert_test (test_image, src_image);
		      if (!compare_image (test_image, src_image)) {
			fprintf (stderr, "Initialization failure:\n");
			fprintf (stderr, "src format: "); print_format(src_image);
			exit (1);
		      }
		      xcb_image_convert (src_image, dst_image);
		      if (!compare_image (src_image, dst_image)) {
			/*
			 * Call the conversion function again so that debugging
			 * is easier
			 */
			fprintf (stderr, "Conversion failure:\n");
			fprintf (stderr, "src format: "); print_format(src_image);
			fprintf (stderr, "dst format: "); print_format(dst_image);
			exit (1);
		      }
		      xcb_image_destroy (src_image);
		      xcb_image_destroy (dst_image);
		    }
		  }
		}
	      }
	    }
	  }
	}
      }
    }
  }
  return 0;
}

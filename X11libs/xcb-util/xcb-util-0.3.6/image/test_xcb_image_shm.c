/*
 * Copyright © 2007 Bart Massey <bart@cs.pdx.edu>
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
#include <stdio.h>

#include <sys/ipc.h>
#include <sys/shm.h>

#include <xcb/xcb.h>
#include <xcb/shm.h>

#include "../aux/xcb_aux.h"
#include "xcb_image.h"

#define W_W 40
#define W_H 40



int
main (int argc, char *argv[])
{
  xcb_connection_t   *c;
  xcb_screen_t       *screen;
  xcb_drawable_t      win;
  xcb_drawable_t      rect;
  xcb_rectangle_t     rect_coord = { 0, 0, W_W, W_H};
  xcb_gcontext_t      bgcolor, fgcolor;
  xcb_point_t         points[2];
  uint32_t           mask;
  uint32_t           valgc[2];
  uint32_t           valwin[3];
  int              depth;
  int              screen_nbr;
  xcb_generic_event_t *e;
  uint8_t format;
  
  /* Open the connexion to the X server and get the first screen */
  c = xcb_connect (NULL, &screen_nbr);
  screen = xcb_aux_get_screen (c, screen_nbr);
  depth = xcb_aux_get_depth (c, screen);

  /* Create a black graphic context for drawing in the foreground */
  win = screen->root;

  fgcolor = xcb_generate_id(c);
  mask = XCB_GC_FOREGROUND | XCB_GC_GRAPHICS_EXPOSURES;
  valgc[0] = screen->black_pixel;
  valgc[1] = 0; /* no graphics exposures */
  xcb_create_gc(c, fgcolor, win, mask, valgc);

  bgcolor = xcb_generate_id(c);
  mask = XCB_GC_FOREGROUND | XCB_GC_GRAPHICS_EXPOSURES;
  valgc[0] = screen->white_pixel;
  valgc[1] = 0; /* no graphics exposures */
  xcb_create_gc(c, bgcolor, win, mask, valgc);

  /* Shm test */
  printf ("shm test begin\n");
  xcb_image_t *img = 0;
  xcb_shm_query_version_reply_t *rep;
  xcb_shm_segment_info_t shminfo;

  rep = xcb_shm_query_version_reply (c,
				 xcb_shm_query_version (c),
				 NULL);
  if (!rep || !rep->shared_pixmaps ||
      rep->major_version < 1 ||
      (rep->major_version == 1 && rep->minor_version == 0))
      {
	  printf ("No or insufficient shm support...\n");
	  exit (0);
      }
  format = rep->pixmap_format;
  img = xcb_image_create_native (c, W_W, W_H, format, depth,
				     0, ~0, 0);

  if (!img)
      {
	  printf ("Can't create image...\n");
	  exit (0);
      }

  printf ("Create image summary:\n");
  printf (" * format..........: %d\n", img->format);
  printf (" * byte order......: %d\n", img->byte_order);
  printf (" * bitmap unit.....: %d\n", img->bpp);
  printf (" * bitmap order....: %d\n", img->bit_order);
  printf (" * bitmap pad......: %d\n", img->scanline_pad);

  shminfo.shmid = shmget (IPC_PRIVATE, img->size, IPC_CREAT|0777);
  shminfo.shmaddr = shmat(shminfo.shmid, 0, 0);
  img->data = shminfo.shmaddr;

  shminfo.shmseg = xcb_generate_id (c);
  xcb_shm_attach(c, shminfo.shmseg, shminfo.shmid, 0);
  shmctl(shminfo.shmid, IPC_RMID, 0);

  /* Draw in the image */
  printf ("put the pixel\n");
  xcb_image_put_pixel (img, 20, 20, 65535);
  printf ("fin put pixel\n");

  /* Ask for our window's Id */
  win = xcb_generate_id(c);

  /* Create the window */
  mask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK | XCB_CW_DONT_PROPAGATE;
  valwin[0] = screen->white_pixel;
  valwin[1] = XCB_EVENT_MASK_KEY_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE | XCB_EVENT_MASK_EXPOSURE;
  valwin[2] = XCB_EVENT_MASK_BUTTON_PRESS;
  xcb_create_window (c,                        /* Connection          */
 		   0,                        /* depth               */
		   win,                      /* window Id           */
		   screen->root,             /* parent window       */
		   0, 0,                     /* x, y                */
		   W_W, W_H,                 /* width, height       */
		   10,                       /* border_width        */
		   XCB_WINDOW_CLASS_INPUT_OUTPUT,/* class               */
		   screen->root_visual,      /* visual              */
		   mask, valwin);            /* masks, not used yet */

  /* Map the window on the screen */
  xcb_map_window (c, win);

  /* Create a Pixmap that will fill the window */
  rect = xcb_generate_id (c);
  xcb_create_pixmap(c, depth, rect, win, W_W, W_H);
  xcb_poly_fill_rectangle(c, rect, bgcolor, 1, &rect_coord);
  points[0].x = 0;
  points[0].y = 0;
  points[1].x = 1;
  points[1].y = 1;
  xcb_poly_line(c, XCB_COORD_MODE_ORIGIN, rect, fgcolor, 2, points);

  xcb_flush (c); 

  while ((e = xcb_wait_for_event(c)))
    {
      switch (e->response_type)
	{ 
	case XCB_EXPOSE:
	  {
	    xcb_copy_area(c, rect, win, bgcolor,
			0, 0, 0, 0, W_W, W_H);
	    printf ("put image\n");
	    xcb_image_shm_put (c, win, fgcolor,
			       img, shminfo,
			       0, 0, 0, 0, W_W,W_H,
			       0);
	    xcb_flush (c);
	    break;
	  }
	}
      free (e);
    }

  return 1;
}

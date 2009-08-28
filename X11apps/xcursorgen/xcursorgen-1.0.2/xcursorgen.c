/* $XFree86: xc/programs/xcursorgen/xcursorgen.c,v 1.8 2002/11/23 02:33:20 keithp Exp $ */
/*
 * xcursorgen.c
 *
 * Copyright (C) 2002 Manish Singh
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Manish Singh not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  Manish Singh makes no
 * representations about the suitability of this software for any purpose.  It
 * is provided "as is" without express or implied warranty.
 *
 * MANISH SINGH DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL MANISH SINGH BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xcursor/Xcursor.h>

#include <png.h>

struct flist
{
  int size;
  int xhot, yhot;
  char *pngfile;
  int delay;
  struct flist *next;
};

static void
usage (char *name)
{
  fprintf (stderr, "usage: %s [-V] [--version] [-?] [--help] [-p <dir>] [--prefix <dir>] [CONFIG [OUT]]\n",
	   name);

  fprintf (stderr, "Generate an Xcursor file from a series of PNG images\n");
  fprintf (stderr, "\n");
  fprintf (stderr, "  -V, --version      display the version number and exit\n");
  fprintf (stderr, "  -?, --help         display this message and exit\n");
  fprintf (stderr, "  -p, --prefix <dir> find cursor images in <dir>\n");
  fprintf (stderr, "\n");
  fprintf (stderr, "With no CONFIG, or when CONFIG is -, read standard input. "
		   "Same with OUT and\n");
  fprintf (stderr, "standard output.\n");
}

static int
read_config_file (char *config, struct flist **list)
{
  FILE *fp;
  char line[4096], pngfile[4000];
  int size, xhot, yhot, delay;
  struct flist *start = NULL, *end = NULL, *curr;
  int count = 0;

  if (strcmp (config, "-") != 0)
    {
      fp = fopen (config, "r");
      if (!fp)
	{
	  *list = NULL;
          return 0;
	}
    }
  else
    fp = stdin;

  while (fgets (line, sizeof (line), fp) != NULL)
    {
      if (line[0] == '#')
	continue;

      switch (sscanf (line, "%d %d %d %3999s %d", &size, &xhot, &yhot, pngfile, &delay))
      {
      case 4:
	delay = 50;
	break;
      case 5:
	break;
      default:
	{
	  fprintf (stderr, "Bad config file data!\n");
	  fclose (fp);
	  return 0;
	}
      }

      curr = malloc (sizeof (struct flist));
      if (curr == NULL)
	{
          fprintf (stderr, "malloc() failed\n");
	  fclose (fp);
          return 0;
	}

      curr->size = size;
      curr->xhot = xhot;
      curr->yhot = yhot;

      curr->delay = delay;

      curr->pngfile = strdup (pngfile);
      if (curr->pngfile == NULL)
	{
          fprintf (stderr, "strdup() failed\n");
	  fclose (fp);
	  free(curr);
          return 0;
	}

      curr->next = NULL;

      if (start)
	{
	  end->next = curr;
          end = curr;
	}
      else
	{
	  start = curr; 
          end = curr;
        }

      count++;
    }

  fclose (fp);

  *list = start;
  return count;
}

#define div_255(x) (((x) + 0x80 + (((x) + 0x80) >> 8)) >> 8)

static void
premultiply_data (png_structp png, png_row_infop row_info, png_bytep data)
{
  int i;

  for (i = 0; i < row_info->rowbytes; i += 4)
    {
	unsigned char  *base = &data[i];
	unsigned char  blue = base[0];
	unsigned char  green = base[1];
	unsigned char  red = base[2];
	unsigned char  alpha = base[3];
	XcursorPixel   p;

        red = div_255((unsigned)red * (unsigned)alpha);
	green = div_255((unsigned)green * (unsigned)alpha);
	blue = div_255((unsigned)blue * (unsigned)alpha);
	p = (alpha << 24) | (red << 16) | (green << 8) | (blue << 0);
	memcpy (base, &p, sizeof (XcursorPixel));
    }
}

static XcursorImage *
load_image (struct flist *list, char *prefix)
{
  XcursorImage *image;
  png_structp png;
  png_infop info;
  png_bytepp rows;
  FILE *fp;
  int i;
  png_uint_32 width, height;
  int depth, color, interlace;
  char *file;

  png = png_create_read_struct (PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
  if (png == NULL)
    return NULL;

  info = png_create_info_struct (png);
  if (info == NULL)
    {
      png_destroy_read_struct (&png, NULL, NULL);
      return NULL;
    }

  if (setjmp (png->jmpbuf))
    {
      png_destroy_read_struct (&png, &info, NULL);
      return NULL;
    }

  if (prefix)
    {
      file = malloc (strlen (prefix) + 1 + strlen (list->pngfile) + 1);
      if (file == NULL)
	{
          fprintf (stderr, "malloc() failed\n");
	  png_destroy_read_struct (&png, &info, NULL);
          return NULL;
	}
      strcpy (file, prefix);
      strcat (file, "/");
      strcat (file, list->pngfile);
    }
  else
    file = list->pngfile;
  fp = fopen (file, "rb");
  if (prefix)
    free (file);

  if (fp == NULL)
    {
      png_destroy_read_struct (&png, &info, NULL);
      return NULL;
    }

  png_init_io (png, fp);
  png_read_info (png, info);
  png_get_IHDR (png, info, &width, &height, &depth, &color, &interlace,
		NULL, NULL);

  /* TODO: More needs to be done here maybe */

  if (color == PNG_COLOR_TYPE_PALETTE && depth <= 8)
    png_set_expand (png);

  if (color == PNG_COLOR_TYPE_GRAY && depth < 8)
    png_set_expand (png);

  if (png_get_valid (png, info, PNG_INFO_tRNS))
    png_set_expand (png);

  if (depth == 16)
    png_set_strip_16 (png);

  if (depth < 8)
    png_set_packing (png);

  if (color == PNG_COLOR_TYPE_GRAY || color == PNG_COLOR_TYPE_GRAY_ALPHA)
    png_set_gray_to_rgb (png);

  if (interlace != PNG_INTERLACE_NONE)
    png_set_interlace_handling (png);

  png_set_bgr (png);
  png_set_filler (png, 255, PNG_FILLER_AFTER);

  png_set_read_user_transform_fn (png, premultiply_data);

  png_read_update_info (png, info);

  image = XcursorImageCreate (width, height);

  image->size = list->size;
  image->xhot = list->xhot;
  image->yhot = list->yhot;
  image->delay = list->delay;

  rows = malloc (sizeof (png_bytep) * height);
  if (rows == NULL)
    {
      fclose (fp);
      png_destroy_read_struct (&png, &info, NULL);
      return NULL;
    }
  
  for (i = 0; i < height; i++)
    rows[i] = (png_bytep) (image->pixels + i * width);

  png_read_image (png, rows);
  png_read_end (png, info);

  free (rows);
  fclose (fp);
  png_destroy_read_struct (&png, &info, NULL);

  return image;
}

static int
write_cursors (int count, struct flist *list, char *filename, char *prefix)
{
  XcursorImages *cimages;
  XcursorImage *image;
  int i;
  FILE *fp;
  int ret;

  if (strcmp (filename, "-") != 0)
    {
      fp = fopen (filename, "wb");
      if (!fp)
        return 1;
    }
  else
    fp = stdout;

  cimages = XcursorImagesCreate (count);

  cimages->nimage = count;

  for (i = 0; i < count; i++, list = list->next)
    {
      image = load_image (list, prefix);
      if (image == NULL)
	{
	  fprintf (stderr, "PNG error while reading %s!\n", list->pngfile);
	  fclose(fp);
	  return 1;
	}

      cimages->images[i] = image;
    }

  ret = XcursorFileSaveImages (fp, cimages);

  fclose (fp);

  return ret ? 0 : 1;
}

static int
check_image (char *image)
{
  unsigned int width, height;
  unsigned char *data;
  int x_hot, y_hot;
  XImage ximage;
  unsigned char hash[XCURSOR_BITMAP_HASH_SIZE];
  int i;

  if (XReadBitmapFileData (image, &width, &height, &data, &x_hot, &y_hot) != BitmapSuccess)
  {
    fprintf (stderr, "Can't open bitmap file \"%s\"\n", image);
    return 1;
  }
  ximage.height = height;
  ximage.width = width;
  ximage.depth = 1;
  ximage.bits_per_pixel = 1;
  ximage.xoffset = 0;
  ximage.format = XYPixmap;
  ximage.data = (char *)data;
  ximage.byte_order = LSBFirst;
  ximage.bitmap_unit = 8;
  ximage.bitmap_bit_order = LSBFirst;
  ximage.bitmap_pad = 8;
  ximage.bytes_per_line = (width+7)/8;
  XcursorImageHash (&ximage, hash);
  printf ("%s: ", image);
  for (i = 0; i < XCURSOR_BITMAP_HASH_SIZE; i++)
    printf ("%02x", hash[i]);
  printf ("\n");
  return 0;
}

int
main (int argc, char *argv[])
{
  struct flist *list;
  int count;
  char *in = 0, *out = 0;
  char *prefix = 0;
  int i;

  for (i = 1; i < argc; i++)
    {
      if (strcmp (argv[i], "-V") == 0 || strcmp (argv[i], "--version") == 0)
        {
          printf ("xcursorgen version %s\n", PACKAGE_VERSION);
          return 0;
        }

      if (strcmp (argv[i], "-?") == 0 || strcmp (argv[i], "--help") == 0)
        {
          usage (argv[0]);
          return 0;
        }
      if (strcmp (argv[i], "-image") == 0)
        {
	  int i = 2;
	  int ret = 0;
	  while (argv[i])
	  {
	    if (check_image (argv[i]))
	      ret = i;
	    i++;
	  }
	  return ret;
	}
      if (strcmp (argv[i], "-p") == 0 || strcmp (argv[i], "--prefix") == 0)
        {
	  i++;
	  if (argv[i] == 0)
	    {
	      usage (argv[0]);
	      return 1;
	    }
	  prefix = argv[i];
	  continue;
        }

      if (!in)
	in = argv[i];
      else if (!out)
	out = argv[i];
      else
      {
	usage (argv[0]);
	return 1;
      }
    }

  if (!in)
    in = "-";
  if (!out)
    out = "-";

  count = read_config_file (in, &list);
  if (count == 0)
    {
      fprintf (stderr, "Error reading config file!\n");
      return 1;
    }

  return write_cursors (count, list, out, prefix);
}

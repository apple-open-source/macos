/*
 * "$Id: print-ps.c,v 1.1.1.1 2003/01/27 19:05:32 jlovell Exp $"
 *
 *   Print plug-in Adobe PostScript driver for the GIMP.
 *
 *   Copyright 1997-2002 Michael Sweet (mike@easysw.com) and
 *	Robert Krawitz (rlk@alum.mit.edu)
 *
 *   This program is free software; you can redistribute it and/or modify it
 *   under the terms of the GNU General Public License as published by the Free
 *   Software Foundation; either version 2 of the License, or (at your option)
 *   any later version.
 *
 *   This program is distributed in the hope that it will be useful, but
 *   WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 *   or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 *   for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

/*
 * This file must include only standard C header files.  The core code must
 * compile on generic platforms that don't support glib, gimp, gtk, etc.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <gimp-print/gimp-print.h>
#include "gimp-print-internal.h"
#include <gimp-print/gimp-print-intl-internal.h>
#include <time.h>
#include <string.h>
#include <limits.h>
#include <stdio.h>

#ifdef _MSC_VER
#define strncasecmp(s,t,n) _strnicmp(s,t,n) 
#define strcasecmp(s,t) _stricmp(s,t) 
#endif

/*
 * Local variables...
 */

static FILE	*ps_ppd = NULL;
static const char	*ps_ppd_file = NULL;


/*
 * Local functions...
 */

static void	ps_hex(const stp_vars_t, unsigned short *, int);
static void	ps_ascii85(const stp_vars_t, unsigned short *, int, int);
static char	*ppd_find(const char *, const char *, const char *, int *);


static char *
c_strdup(const char *s)
{
  char *ret = stp_malloc(strlen(s) + 1);
  strcpy(ret, s);
  return ret;
}

/*
 * 'ps_parameters()' - Return the parameter values for the given parameter.
 */

static stp_param_t *				/* O - Parameter values */
ps_parameters(const stp_printer_t printer,	/* I - Printer model */
              const char *ppd_file,		/* I - PPD file (not used) */
              const char *name,		/* I - Name of parameter */
              int  *count)		/* O - Number of values */
{
  int		i;
  char		line[1024],
		lname[255],
		loption[255],
		*ltext;
  stp_param_t	*valptrs;


  if (count == NULL)
    return (NULL);

  *count = 0;

  if (ppd_file == NULL || name == NULL)
    return (NULL);

  if (ps_ppd_file == NULL || strcmp(ps_ppd_file, ppd_file) != 0)
  {
    if (ps_ppd != NULL)
      fclose(ps_ppd);

    ps_ppd = fopen(ppd_file, "r");

    if (ps_ppd == NULL)
      ps_ppd_file = NULL;
    else
      ps_ppd_file = ppd_file;
  }

  if (ps_ppd == NULL)
    {
      if (strcmp(name, "PageSize") == 0)
	{
	  int papersizes = stp_known_papersizes();
	  valptrs = stp_zalloc(sizeof(stp_param_t) * papersizes);
	  *count = 0;
	  for (i = 0; i < papersizes; i++)
	    {
	      const stp_papersize_t pt = stp_get_papersize_by_index(i);
	      if (strlen(stp_papersize_get_name(pt)) > 0)
		{
		  valptrs[*count].name = c_strdup(stp_papersize_get_name(pt));
		  valptrs[*count].text = c_strdup(stp_papersize_get_text(pt));
		  (*count)++;
		}
	    }
	  return (valptrs);
	}
      else
	return (NULL);
    }

  rewind(ps_ppd);
  *count = 0;

  /* FIXME -- need to use realloc */
  valptrs = stp_zalloc(100 * sizeof(stp_param_t));

  while (fgets(line, sizeof(line), ps_ppd) != NULL)
  {
    if (line[0] != '*')
      continue;

    if (sscanf(line, "*%s %[^:]", lname, loption) != 2)
      continue;

    if (strcasecmp(lname, name) == 0)
    {
      if ((ltext = strchr(loption, '/')) != NULL)
        *ltext++ = '\0';
      else
        ltext = loption;

      valptrs[(*count)].name = c_strdup(loption);
      valptrs[(*count)].text = c_strdup(ltext);
      (*count) ++;
    }
  }

  if (*count == 0)
  {
    stp_free(valptrs);
    return (NULL);
  }
  else
    return (valptrs);
}

static const char *
ps_default_parameters(const stp_printer_t printer,
		      const char *ppd_file,
		      const char *name)
{
  int		i;
  char		line[1024],
		lname[255],
		loption[255],
		defname[255];

  if (ppd_file == NULL || name == NULL)
    return (NULL);

  sprintf(defname, "Default%s", name);

  if (ps_ppd_file == NULL || strcmp(ps_ppd_file, ppd_file) != 0)
  {
    if (ps_ppd != NULL)
      fclose(ps_ppd);

    ps_ppd = fopen(ppd_file, "r");

    if (ps_ppd == NULL)
      ps_ppd_file = NULL;
    else
      ps_ppd_file = ppd_file;
  }

  if (ps_ppd == NULL)
    {
      if (strcmp(name, "PageSize") == 0)
	{
	  int papersizes = stp_known_papersizes();
	  for (i = 0; i < papersizes; i++)
	    {
	      const stp_papersize_t pt = stp_get_papersize_by_index(i);
	      if (strlen(stp_papersize_get_name(pt)) > 0)
		{
		  return stp_papersize_get_name(pt);
		}
	    }
	  return NULL;
	}
      else
	return (NULL);
    }

  rewind(ps_ppd);

  while (fgets(line, sizeof(line), ps_ppd) != NULL)
  {
    if (line[0] != '*')
      continue;

    if (sscanf(line, "*%[^:]:%s", lname, loption) != 2)
      continue;

    if (strcasecmp(lname, defname) == 0)
    {
      return c_strdup(loption);
    }
  }

  if (strcmp(name, "Resolution") == 0)
    {
      return "default";
    }

  return NULL;
}


/*
 * 'ps_media_size()' - Return the size of the page.
 */

static void
ps_media_size(const stp_printer_t printer,	/* I - Printer model */
	      const stp_vars_t v,		/* I */
              int  *width,		/* O - Width in points */
              int  *height)		/* O - Height in points */
{
  char	*dimensions;			/* Dimensions of media size */

  stp_dprintf(STP_DBG_PS, v,
	      "ps_media_size(%d, \'%s\', \'%s\', %08x, %08x)\n",
	      stp_printer_get_model(printer), stp_get_ppd_file(v),
	      stp_get_media_size(v),
	      width, height);

  if ((dimensions = ppd_find(stp_get_ppd_file(v), "PaperDimension",
			     stp_get_media_size(v), NULL))
      != NULL)
    sscanf(dimensions, "%d%d", width, height);
  else
    stp_default_media_size(printer, v, width, height);
}


/*
 * 'ps_imageable_area()' - Return the imageable area of the page.
 */

static void
ps_imageable_area(const stp_printer_t printer,	/* I - Printer model */
		  const stp_vars_t v,      /* I */
                  int  *left,		/* O - Left position in points */
                  int  *right,		/* O - Right position in points */
                  int  *bottom,		/* O - Bottom position in points */
                  int  *top)		/* O - Top position in points */
{
  char	*area;				/* Imageable area of media */
  float	fleft,				/* Floating point versions */
	fright,
	fbottom,
	ftop;


  if ((area = ppd_find(stp_get_ppd_file(v), "ImageableArea",
		       stp_get_media_size(v), NULL))
      != NULL)
    {
      stp_dprintf(STP_DBG_PS, v, "area = \'%s\'\n", area);
      if (sscanf(area, "%f%f%f%f", &fleft, &fbottom, &fright, &ftop) == 4)
	{
	  *left   = (int)fleft;
	  *right  = (int)fright;
	  *bottom = (int)fbottom;
	  *top    = (int)ftop;
	}
      else
	*left = *right = *bottom = *top = 0;
    }
  else
    {
      stp_default_media_size(printer, v, right, top);
      *left   = 18;
      *right  -= 18;
      *top    -= 36;
      *bottom = 36;
    }
}

static void
ps_limit(const stp_printer_t printer,	/* I - Printer model */
	    const stp_vars_t v,  		/* I */
	    int *width,
	    int *height,
	    int *min_width,
	    int *min_height)
{
  *width =	INT_MAX;
  *height =	INT_MAX;
  *min_width =	1;
  *min_height =	1;
}

/*
 * This is really bogus...
 */
static void
ps_describe_resolution(const stp_printer_t printer,
			const char *resolution, int *x, int *y)
{
  *x = -1;
  *y = -1;
  sscanf(resolution, "%dx%d", x, y);
  return;
}

/*
 * 'ps_print()' - Print an image to a PostScript printer.
 */

static void
ps_print(const stp_printer_t printer,		/* I - Model (Level 1 or 2) */
         stp_image_t *image,		/* I - Image to print */
	 const stp_vars_t v)
{
  unsigned char *cmap = stp_get_cmap(v);
  int		model = stp_printer_get_model(printer);
  const char	*ppd_file = stp_get_ppd_file(v);
  const char	*resolution = stp_get_resolution(v);
  const char	*media_size = stp_get_media_size(v);
  const char	*media_type = stp_get_media_type(v);
  const char	*media_source = stp_get_media_source(v);
  int 		output_type = stp_get_output_type(v);
  int		orientation = stp_get_orientation(v);
  double 	scaling = stp_get_scaling(v);
  int		top = stp_get_top(v);
  int		left = stp_get_left(v);
  int		i, j;		/* Looping vars */
  int		y;		/* Looping vars */
  unsigned char	*in;		/* Input pixels from image */
  unsigned short	*out;		/* Output pixels for printer */
  int		page_left,	/* Left margin of page */
		page_right,	/* Right margin of page */
		page_top,	/* Top of page */
		page_bottom,	/* Bottom of page */
		page_width,	/* Width of page */
		page_height,	/* Height of page */
		out_width,	/* Width of image on page */
		out_height,	/* Height of image on page */
		out_bpp,	/* Output bytes per pixel */
		out_ps_height,	/* Output height (Level 2 output) */
		out_offset;	/* Output offset (Level 2 output) */
  time_t	curtime;	/* Current time of day */
  stp_convert_t	colorfunc;	/* Color conversion function... */
  int		zero_mask;
  char		*command;	/* PostScript command */
  const char	*temp;		/* Temporary string pointer */
  int		order,		/* Order of command */
		num_commands;	/* Number of commands */
  struct			/* PostScript commands... */
  {
    const char	*keyword, *choice;
    char	*command;
    int		order;
  }		commands[4];
  int           image_height,
                image_width,
                image_bpp;
  stp_vars_t	nv = stp_allocate_copy(v);

  if (!stp_get_verified(nv))
    {
      stp_eprintf(nv, "Print options not verified; cannot print.\n");
      return;
    }

 /*
  * Setup a read-only pixel region for the entire image...
  */

  image->init(image);
  image_height = image->height(image);
  image_width = image->width(image);
  image_bpp = image->bpp(image);

 /*
  * Choose the correct color conversion function...
  */

  colorfunc = stp_choose_colorfunc(output_type, image_bpp, cmap, &out_bpp, nv);

 /*
  * Compute the output size...
  */

  ps_imageable_area(printer, nv, &page_left, &page_right,
                    &page_bottom, &page_top);
  stp_compute_page_parameters(page_right, page_left, page_top, page_bottom,
			  scaling, image_width, image_height, image,
			  &orientation, &page_width, &page_height,
			  &out_width, &out_height, &left, &top);

  /*
   * Recompute the image height and width.  If the image has been
   * rotated, these will change from previously.
   */
  image_height = image->height(image);
  image_width = image->width(image);

 /*
  * Let the user know what we're doing...
  */

  image->progress_init(image);

 /*
  * Output a standard PostScript header with DSC comments...
  */

  curtime = time(NULL);

  if (left < 0)
    left = (page_width - out_width) / 2 + page_left;
  else
    left += page_left;

  if (top < 0)
    top  = (page_height + out_height) / 2 + page_bottom;
  else
    top = page_height - top + page_bottom;

  stp_dprintf(STP_DBG_PS, v,
	      "out_width = %d, out_height = %d\n", out_width, out_height);
  stp_dprintf(STP_DBG_PS, v,
	      "page_left = %d, page_right = %d, page_bottom = %d, page_top = %d\n",
	      page_left, page_right, page_bottom, page_top);
  stp_dprintf(STP_DBG_PS, v, "left = %d, top = %d\n", left, top);

  stp_puts("%!PS-Adobe-3.0\n", v);
#ifdef HAVE_CONFIG_H
  stp_zprintf(v, "%%%%Creator: %s/Gimp-Print %s (%s)\n",
	      image->get_appname(image), VERSION, RELEASE_DATE);
#else
  stp_zprintf(v, "%%%%Creator: %s/Gimp-Print\n", image->get_appname(image));
#endif
  stp_zprintf(v, "%%%%CreationDate: %s", ctime(&curtime));
  stp_puts("%Copyright: 1997-2002 by Michael Sweet (mike@easysw.com) and Robert Krawitz (rlk@alum.mit.edu)\n", v);
  stp_zprintf(v, "%%%%BoundingBox: %d %d %d %d\n",
          left, top - out_height, left + out_width, top);
  stp_puts("%%DocumentData: Clean7Bit\n", v);
  stp_zprintf(v, "%%%%LanguageLevel: %d\n", model + 1);
  stp_puts("%%Pages: 1\n", v);
  stp_puts("%%Orientation: Portrait\n", v);
  stp_puts("%%EndComments\n", v);

 /*
  * Find any printer-specific commands...
  */

  num_commands = 0;

  if ((command = ppd_find(ppd_file, "PageSize", media_size, &order)) != NULL)
  {
    commands[num_commands].keyword = "PageSize";
    commands[num_commands].choice  = media_size;
    commands[num_commands].command = stp_malloc(strlen(command) + 1);
    strcpy(commands[num_commands].command, command);
    commands[num_commands].order   = order;
    num_commands ++;
  }

  if ((command = ppd_find(ppd_file, "InputSlot", media_source, &order)) != NULL)
  {
    commands[num_commands].keyword = "InputSlot";
    commands[num_commands].choice  = media_source;
    commands[num_commands].command = stp_malloc(strlen(command) + 1);
    strcpy(commands[num_commands].command, command);
    commands[num_commands].order   = order;
    num_commands ++;
  }

  if ((command = ppd_find(ppd_file, "MediaType", media_type, &order)) != NULL)
  {
    commands[num_commands].keyword = "MediaType";
    commands[num_commands].choice  = media_type;
    commands[num_commands].command = stp_malloc(strlen(command) + 1);
    strcpy(commands[num_commands].command, command);
    commands[num_commands].order   = order;
    num_commands ++;
  }

  if ((command = ppd_find(ppd_file, "Resolution", resolution, &order)) != NULL)
  {
    commands[num_commands].keyword = "Resolution";
    commands[num_commands].choice  = resolution;
    commands[num_commands].command = stp_malloc(strlen(command) + 1);
    strcpy(commands[num_commands].command, command);
    commands[num_commands].order   = order;
    num_commands ++;
  }

 /*
  * Sort the commands using the OrderDependency value...
  */

  for (i = 0; i < (num_commands - 1); i ++)
    for (j = i + 1; j < num_commands; j ++)
      if (commands[j].order < commands[i].order)
      {
        temp                = commands[i].keyword;
        commands[i].keyword = commands[j].keyword;
        commands[j].keyword = temp;

        temp                = commands[i].choice;
        commands[i].choice  = commands[j].choice;
        commands[j].choice  = temp;

        order               = commands[i].order;
        commands[i].order   = commands[j].order;
        commands[j].order   = order;

        command             = commands[i].command;
        commands[i].command = commands[j].command;
        commands[j].command = command;
      }

 /*
  * Send the commands...
  */

  if (num_commands > 0)
  {
    stp_puts("%%BeginSetup\n", v);

    for (i = 0; i < num_commands; i ++)
    {
      stp_puts("[{\n", v);
      stp_zprintf(v, "%%%%BeginFeature: *%s %s\n", commands[i].keyword,
                  commands[i].choice);
      if (commands[i].command[0])
      {
	stp_puts(commands[i].command, v);
	if (commands[i].command[strlen(commands[i].command) - 1] != '\n')
          stp_puts("\n", v);
      }

      stp_puts("%%EndFeature\n", v);
      stp_puts("} stopped cleartomark\n", v);
      stp_free(commands[i].command);
    }

    stp_puts("%%EndSetup\n", v);
  }

 /*
  * Output the page...
  */

  stp_puts("%%Page: 1 1\n", v);
  stp_puts("gsave\n", v);

  stp_zprintf(v, "%d %d translate\n", left, top);
  stp_zprintf(v, "%.3f %.3f scale\n",
          (double)out_width / ((double)image_width),
          (double)out_height / ((double)image_height));

  in  = stp_zalloc(image_width * image_bpp);
  out = stp_zalloc((image_width * out_bpp + 3) * 2);

  stp_compute_lut(nv, 256);

  if (model == 0)
  {
    stp_zprintf(v, "/picture %d string def\n", image_width * out_bpp);

    stp_zprintf(v, "%d %d 8\n", image_width, image_height);

    stp_puts("[ 1 0 0 -1 0 1 ]\n", v);

    if (output_type == OUTPUT_GRAY || output_type == OUTPUT_MONOCHROME)
      stp_puts("{currentfile picture readhexstring pop} image\n", v);
    else
      stp_puts("{currentfile picture readhexstring pop} false 3 colorimage\n", v);

    for (y = 0; y < image_height; y ++)
    {
      if ((y & 15) == 0)
	image->note_progress(image, y, image_height);

      if (image->get_row(image, in, y) != STP_IMAGE_OK)
	break;
      (*colorfunc)(nv, in, out, &zero_mask, image_width, image_bpp, cmap,
		   NULL, NULL, NULL);

      ps_hex(v, out, image_width * out_bpp);
    }
  }
  else
  {
    if (output_type == OUTPUT_GRAY || output_type == OUTPUT_MONOCHROME)
      stp_puts("/DeviceGray setcolorspace\n", v);
    else
      stp_puts("/DeviceRGB setcolorspace\n", v);

    stp_puts("<<\n", v);
    stp_puts("\t/ImageType 1\n", v);

    stp_zprintf(v, "\t/Width %d\n", image_width);
    stp_zprintf(v, "\t/Height %d\n", image_height);
    stp_puts("\t/BitsPerComponent 8\n", v);

    if (output_type == OUTPUT_GRAY || output_type == OUTPUT_MONOCHROME)
      stp_puts("\t/Decode [ 0 1 ]\n", v);
    else
      stp_puts("\t/Decode [ 0 1 0 1 0 1 ]\n", v);

    stp_puts("\t/DataSource currentfile /ASCII85Decode filter\n", v);

    if ((image_width * 72 / out_width) < 100)
      stp_puts("\t/Interpolate true\n", v);

    stp_puts("\t/ImageMatrix [ 1 0 0 -1 0 1 ]\n", v);

    stp_puts(">>\n", v);
    stp_puts("image\n", v);

    for (y = 0, out_offset = 0; y < image_height; y ++)
    {
      if ((y & 15) == 0)
	image->note_progress(image, y, image_height);

      if (image->get_row(image, in, y) != STP_IMAGE_OK)
	break;
      (*colorfunc)(nv, in, out + out_offset, &zero_mask, image_width,
		   image_bpp, cmap, NULL, NULL, NULL);

      out_ps_height = out_offset + image_width * out_bpp;

      if (y < (image_height - 1))
      {
        ps_ascii85(v, out, out_ps_height & ~3, 0);
        out_offset = out_ps_height & 3;
      }
      else
      {
        ps_ascii85(v, out, out_ps_height, 1);
        out_offset = 0;
      }

      if (out_offset > 0)
        memcpy(out, out + out_ps_height - out_offset, out_offset);
    }
  }
  image->progress_conclude(image);

  stp_free_lut(nv);
  stp_free(in);
  stp_free(out);

  stp_puts("grestore\n", v);
  stp_puts("showpage\n", v);
  stp_puts("%%Trailer\n", v);
  stp_puts("%%EOF\n", v);
  stp_free_vars(nv);
}


/*
 * 'ps_hex()' - Print binary data as a series of hexadecimal numbers.
 */

static void
ps_hex(const stp_vars_t v,	/* I - File to print to */
       unsigned short   *data,	/* I - Data to print */
       int              length)	/* I - Number of bytes to print */
{
  int		col;		/* Current column */
  static const char	*hex = "0123456789ABCDEF";


  col = 0;
  while (length > 0)
  {
    unsigned char pixel = (*data & 0xff00) >> 8;
   /*
    * Put the hex chars out to the file; note that we don't use stp_zprintf()
    * for speed reasons...
    */

    stp_putc(hex[pixel >> 4], v);
    stp_putc(hex[pixel & 15], v);

    data ++;
    length --;

    col += 2;
    if (col >= 72)
    {
      col = 0;
      stp_putc('\n', v);
    }
  }

  if (col > 0)
    stp_putc('\n', v);
}


/*
 * 'ps_ascii85()' - Print binary data as a series of base-85 numbers.
 */

static void
ps_ascii85(const          stp_vars_t v,	/* I - File to print to */
	   unsigned short *data,	/* I - Data to print */
	   int            length,	/* I - Number of bytes to print */
	   int            last_line)	/* I - Last line of raster data? */
{
  int		i;			/* Looping var */
  unsigned	b;			/* Binary data word */
  unsigned char	c[5];			/* ASCII85 encoded chars */
  static int	column = 0;		/* Current column */


  while (length > 3)
  {
    unsigned char d0 = (data[0] & 0xff00) >> 8;
    unsigned char d1 = (data[1] & 0xff00) >> 8;
    unsigned char d2 = (data[2] & 0xff00) >> 8;
    unsigned char d3 = (data[3] & 0xff00) >> 8;
    b = (((((d0 << 8) | d1) << 8) | d2) << 8) | d3;

    if (b == 0)
    {
      stp_putc('z', v);
      column ++;
    }
    else
    {
      c[4] = (b % 85) + '!';
      b /= 85;
      c[3] = (b % 85) + '!';
      b /= 85;
      c[2] = (b % 85) + '!';
      b /= 85;
      c[1] = (b % 85) + '!';
      b /= 85;
      c[0] = b + '!';

      stp_zfwrite((const char *)c, 5, 1, v);
      column += 5;
    }

    if (column > 72)
    {
      stp_putc('\n', v);
      column = 0;
    }

    data += 4;
    length -= 4;
  }

  if (last_line)
  {
    if (length > 0)
    {
      for (b = 0, i = length; i > 0; b = (b << 8) | data[0], data ++, i --);

      c[4] = (b % 85) + '!';
      b /= 85;
      c[3] = (b % 85) + '!';
      b /= 85;
      c[2] = (b % 85) + '!';
      b /= 85;
      c[1] = (b % 85) + '!';
      b /= 85;
      c[0] = b + '!';

      stp_zfwrite((const char *)c, length + 1, 1, v);
    }

    stp_puts("~>\n", v);
    column = 0;
  }
}


/*
 * 'ppd_find()' - Find a control string with the specified name & parameters.
 */

static char *			/* O - Control string */
ppd_find(const char *ppd_file,	/* I - Name of PPD file */
         const char *name,	/* I - Name of parameter */
         const char *option,	/* I - Value of parameter */
         int  *order)		/* O - Order of the control string */
{
  char		line[1024],	/* Line from file */
		lname[255],	/* Name from line */
		loption[255],	/* Value from line */
		*opt;		/* Current control string pointer */
  static char	*value = NULL;	/* Current control string value */


  if (ppd_file == NULL || name == NULL || option == NULL)
    return (NULL);
  if (!value)
    value = stp_zalloc(32768);

  if (ps_ppd_file == NULL || strcmp(ps_ppd_file, ppd_file) != 0)
  {
    if (ps_ppd != NULL)
      fclose(ps_ppd);

    ps_ppd = fopen(ppd_file, "r");

    if (ps_ppd == NULL)
      ps_ppd_file = NULL;
    else
      ps_ppd_file = ppd_file;
  }

  if (ps_ppd == NULL)
    return (NULL);

  if (order != NULL)
    *order = 1000;

  rewind(ps_ppd);
  while (fgets(line, sizeof(line), ps_ppd) != NULL)
  {
    if (line[0] != '*')
      continue;

    if (strncasecmp(line, "*OrderDependency:", 17) == 0 && order != NULL)
    {
      sscanf(line, "%*s%d", order);
      continue;
    }
    else if (sscanf(line, "*%s %[^/:]", lname, loption) != 2)
      continue;

    if (strcasecmp(lname, name) == 0 &&
        strcasecmp(loption, option) == 0)
    {
      opt = strchr(line, ':') + 1;
      while (*opt == ' ' || *opt == '\t')
        opt ++;
      if (*opt != '\"')
        continue;

      strcpy(value, opt + 1);
      if ((opt = strchr(value, '\"')) == NULL)
      {
        while (fgets(line, sizeof(line), ps_ppd) != NULL)
        {
          strcat(value, line);
          if (strchr(line, '\"') != NULL)
          {
            strcpy(strchr(value, '\"'), "\n");
            break;
          }
        }
      }
      else
        *opt = '\0';

      return (value);
    }
  }

  return (NULL);
}

const stp_printfuncs_t stp_ps_printfuncs =
{
  ps_parameters,
  ps_media_size,
  ps_imageable_area,
  ps_limit,
  ps_print,
  ps_default_parameters,
  ps_describe_resolution,
  stp_verify_printer_params,
  stp_start_job,
  stp_end_job
};

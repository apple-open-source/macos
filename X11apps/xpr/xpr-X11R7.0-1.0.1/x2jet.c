/* $XConsortium: x2jet.c,v 1.6 94/04/17 20:44:03 rws Exp $ */

/* -*-C-*-
********************************************************************************
*
* File:         x2jet.c
* RCS:          x2jet.c,v 1.23 89/07/17 12:02:51 lori Exp
* Description:  xpr support for HP LaserJet and PaintJet printers
* Author:       Larry Rupp, HP Graphics Technology Division
* Created:      Fri Jul 15 15:22:26 1988
* Modified:     Thu Sep 15 11:59:34 1988 (Larry Rupp) ler@hpfcler
*               Tue Dec  6 10:04:43 PST 1988 (Marc Ayotte) marca@hp-pcd
* Language:     C
* Package:      N/A
* Status:       Released to MIT
*
* (c) Copyright 1988, Hewlett-Packard Company.
*
********************************************************************************
*/





/********************************************************

Copyright (c) 1988 by Hewlett-Packard Company

Permission to use, copy, modify, and distribute this software 
and its documentation for any purpose and without fee is hereby 
granted, provided that the above copyright notice appear in all 
copies and that both that copyright notice and this permission 
notice appear in supporting documentation, and that 
Hewlett-Packard not be used in advertising or publicity 
pertaining to distribution of the software without specific, written 
prior permission.

********************************************************/
/*

Copyright (c) 1988  X Consortium

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE X CONSORTIUM BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of the X Consortium shall
not be used in advertising or otherwise to promote the sale, use or
other dealings in this Software without prior written authorization
from the X Consortium.

*/


#include <stdio.h>
#include <X11/Xlib.h>
#include <X11/XWDFile.h>

#include "xpr.h"

#ifdef  NLS16
#ifndef NLS
#define	NLS
#endif
#endif

#ifndef NLS
#define catgets(i, sn,mn,s) (s)
#else /* NLS */
#define NL_SETN 2	/* set number */
#include <nl_types.h>

extern	nl_catd	nlmsg_fd;

#endif /* NLS */

#ifndef	TRUE
#  define	FALSE	0
#  define	TRUE	1
#endif

/* default printable page area (inches) */
#define STDWIDTH 8.0
#define STDHEIGHT 10.5

/* header & trailer character cell size (centipoints) */
#define CHARWIDTH 720
#define CHARHEIGHT 1200

#define XWDHEADERSIZE	(sizeof(XWDFileHeader))
#define XCOLORSIZE  	(sizeof(XColor))


typedef struct { long width, height; } Area;
typedef struct { long x, y; } Location;


static Area limit;	/* image clip limits (dots) */
static Area page;	/* printable page size (centipoints) */

static Location headerloc;	/* centipoint location of header string */
static Location trailerloc;	/* centipoint location of trailer string */
static Location imageloc;	/* centipoint location of image */

static int headerlimit;		/* number of chars which will printed for */
static int trailerlimit;	/*  the image's header/trailer strings    */

static XWDFileHeader xwd_header;

static XColor *xwd_colors;

static char *xwd_image;

static unsigned long Z_pixel_mask;

static int true_scale;

typedef struct {
  unsigned long Rmask, Gmask, Bmask;
  int Rshift, Gshift, Bshift;
} RGBshiftmask;

/* Local prototypes */
static void set_image_limits (  int scale, int density,  enum orientation orient,  Area print_area);
static void set_header_trailer_limits (char *header, char *trailer, long printwidth);
static void set_print_locations (  int scale, int density,  int top, int left,  const char *header, const char *trailer,  enum orientation orient,  int position_on_page);
static int scale_raster (
  int density,
  enum orientation orient,
  Area print_area);
static void scale_and_orient_image (
  int *scale, int *density,
  int width, int height, int left, int top,  /* in 300ths of an inch */
  const char *header, const char *trailer,
  enum orientation *orient,
  int position_on_page,
  enum device device);
static void setup_RGBshiftmask (RGBshiftmask *sm, unsigned long rmask, unsigned long gmask, unsigned long bmask);
static void swap_black_and_white (void);
static void reset_color_mapping (void);
static void prepare_color_mapping (
  int invert, int paintjet,
  unsigned int cutoff,
  FILE *out);
static
void select_grey (int level, int *r, int *g, int *b);
static int load_printer_color (
  long index,
  int nearmatch,
  enum device device);
static int lookup_color_index (long i);
static void select_printer_color (
  long index,
  int *red, int *green, int *blue,
  long *compositeRGB,
  enum device device);
static int color_already_in_printer (long compositeRGB, long *pindex);
static int program_new_printer_color (
  int red, int green, int blue,
  long compositeRGB,
  long *pindex);
static long composite_diff (long x, long y);
static long find_nearest_programmed_color (long compositeRGB);
static void add_index_to_chain (
  long cindex,
  long pindex);
static int load_printer_color_DT (
  long index,
  int nearmatch,
  enum device device);
static int load_line_colors (
  long *line,
  int length, int nearmatch,
  enum device device);
static void download_colors (
  long *line,
  int length,
  enum device device);
static void validate_visual(void);
static void read_xwd_data (FILE *in);
static void write_image_prefix (
  FILE *out,
  int scale, int density,
  const char *header,
  enum device device,
  int position_on_page, int initial_formfeed,
  enum orientation orient,
  float gamma,
  int render,
  int slide);
static void write_image_suffix (
  FILE *out,
  const char *trailer,
  int position_on_page,
  int slide, int render,
  enum device device);
static unsigned long Z_image_pixel (int x, int y);
static unsigned long XY_image_pixel (int x, int y);
static void direct_by_pixel(
  FILE *out,
  long *line,
  int length,
  enum device device);
static void index_by_pixel(
  FILE *out,
  long *line,
  int length);
static void write_raster_line (
  FILE *out,
  int scale,
  enum device device,
  long *line,
  int length);
static void write_portrait_Z_image (
  FILE *out,
  int scale,
  enum device device);
static void write_landscape_Z_image (
  FILE *out,
  int scale,
  enum device device);
static void write_portrait_XY_image (
  FILE *out,
  int scale,
  enum device device);
static void write_landscape_XY_image (
  FILE *out,
  int scale,
  enum device device);
static void write_Z_image (
  FILE *out,
  int scale,
  enum orientation orient,
  enum device device);
static void write_XY_image (
  FILE *out,
  int scale,
  enum orientation orient,
  enum device device);
static void write_image (
  FILE *out,
  int scale,
  enum orientation orient,
  enum device device);
static void fatal_err (const char *s, ...);


/* Computes the centipoint width of one printer dot. */
#define dot_centipoints(s,d)	((7200.0 * (s)) / (d))

static
void set_image_limits (
  int scale, int density,
  enum orientation orient,
  Area print_area)
{
  Area print_dots;
  double dotsize;

  /* Set dotsize to the centipoint width of one printer dot. */
  dotsize = dot_centipoints(scale, density);

  if (orient == PORTRAIT) {
    print_dots.width = print_area.width / dotsize;
    print_dots.height = print_area.height / dotsize;
  } else {
    print_dots.height = print_area.width / dotsize;
    print_dots.width = print_area.height / dotsize;
  }

  limit.width = (print_dots.width < xwd_header.pixmap_width)
		? print_dots.width : xwd_header.pixmap_width;
  limit.height = (print_dots.height < xwd_header.pixmap_height)
		? print_dots.height : xwd_header.pixmap_height;

  if ((limit.width != xwd_header.pixmap_width)
      || (limit.height != xwd_header.pixmap_height))
    fprintf(stderr,(catgets(nlmsg_fd,NL_SETN,1, "%s: Warning: %ld x %ld image clipped to %ld x %ld.\n")),
	    progname,
	    xwd_header.pixmap_width, xwd_header.pixmap_height,
	    limit.width, limit.height);
}


static
void set_header_trailer_limits (char *header, char *trailer, long printwidth)
{
  /* Determine the number of header and trailer characters
   * that will fit into the available printing area.
   */
  headerlimit = header ? (((strlen(header) * CHARWIDTH) <= printwidth)
			  ? strlen(header) : (printwidth / CHARWIDTH))
		       : 0;
  if (header && headerlimit != strlen(header)) {
    fprintf(stderr,(catgets(nlmsg_fd,NL_SETN,2,
		"%s: Warning: Header string clipped to %d characters.\n")),
		progname, headerlimit);
    header[headerlimit] = '\0';
  }

  trailerlimit = trailer ? (((strlen(trailer) * CHARWIDTH) <= printwidth)
			    ? strlen(trailer) : (printwidth / CHARWIDTH))
			 : 0;
  if (trailer && trailerlimit != strlen(trailer)) {
    fprintf(stderr,(catgets(nlmsg_fd,NL_SETN,3,
		"%s: Warning: Trailer string clipped to %d characters.\n")),
		progname, trailerlimit);
    trailer[headerlimit] = '\0';
  }
}


static
void set_print_locations (
  int scale, int density,
  int top, int left,
  const char *header, const char *trailer,
  enum orientation orient,
  int position_on_page)
{
  Area image;
  double dotsize;

  /* Set dotsize to the centipoint width of one printer dot. */
  dotsize = dot_centipoints(scale, density);

  /* Compute the centipoint size of the clipped image area. */
  if (orient == PORTRAIT) {
    image.width = limit.width * dotsize;
    image.height = limit.height * dotsize;
  } else {
    image.height = limit.width * dotsize;
    image.width = limit.height * dotsize;
  }

  if (position_on_page) {
    /* set vertical positions */
    imageloc.y = (top >= 0)
	      ? top * 24 + ((header) ? CHARHEIGHT : 0)
	      : ((page.height - ((header) ? CHARHEIGHT : 0)
		  - image.height - ((trailer) ? CHARHEIGHT : 0)) / 2)
		+ ((header) ? CHARHEIGHT : 0);
    headerloc.y = imageloc.y - CHARHEIGHT / 4;
    trailerloc.y = imageloc.y + image.height + (3 * CHARHEIGHT) / 4;

    /* set horizontal positions */
    if (left >= 0)
      headerloc.x = imageloc.x = trailerloc.x = left * 24;
    else {
      headerloc.x = (page.width - headerlimit * CHARWIDTH) / 2;
      imageloc.x = (page.width - image.width) / 2;
      trailerloc.x = (page.width - trailerlimit * CHARWIDTH) / 2;
    }
  }
}


static
int scale_raster (
  int density,
  enum orientation orient,
  Area print_area)
{
  Area image;
  int h_scale, v_scale;

  /* Set the image dimensions to the number of centipoints that would be
   * required for printing at the selected density.
   */
  if (orient == PORTRAIT) {
    image.width = xwd_header.pixmap_width * 7200 / density;
    image.height = xwd_header.pixmap_height * 7200 / density;
  } else {
    image.height = xwd_header.pixmap_width * 7200 / density;
    image.width = xwd_header.pixmap_height * 7200 / density;
  }

  /* Calculate the maximum image multiplier along
   * the horizontal and vertical dimensions.
   */
  h_scale = print_area.width / image.width;
  v_scale = print_area.height / image.height;

  /* If the image can be expanded, return the lesser of the horizontal and
   * vertical multipliers.  Otherwise, the image will not completely fit
   * the available print area, so just return 1 as the expansion factor.
   */
  return (((h_scale > 0) && (v_scale > 0))
	  ? ((h_scale<v_scale) ? h_scale : v_scale)
	  : 1);
}


static
void scale_and_orient_image (
  int *scale, int *density,
  int width, int height, int left, int top,  /* in 300ths of an inch */
  const char *header, const char *trailer,
  enum orientation *orient,
  int position_on_page,
  enum device device)
{
  Area usable;

  /* Determine printable area expressed in centipoints.  There are 7200
   * centipoints to the inch.  The width and height parameters passed in
   * are expressed in 300ths of an inch, therefore a 24x conversion factor
   * is used on the parameter values.  The default page dimensions STDWIDTH
   * and STDHEIGHT are expressed in inches so must be multiplied by 7200
   * to convert to centipoints.
   */
  page.width = (width >= 0) ? width * 24 : STDWIDTH * 7200;
  page.height = (height >= 0) ? height * 24 : STDHEIGHT * 7200;

  /* Paintjet Xl has a mechanical form feed, not a strip feed. It has
   * a slop of about 1/4 to 1/2 of an inch at the top and bottom.
   * deduct it from the page height.
   */
  if (device == PJETXL)
     page.height = page.height - 7200;

  /* Determine the area usable for the image.  This area will be smaller
   * than the total printable area if margins or header/trailer strings
   * have been specified.  Margins, like width and height discussed above,
   * are expressed in 300ths of an inch and must be converted to centipoints.
   * Header and trailer strings each reduce the available image height
   * by 1/6 inch, or 1200 centipoints (aka CHARHEIGHT).
   */
  usable.width = page.width - ((left > 0) ? (left * 24) : 0);
  usable.height = page.height - ((top > 0) ? (top * 24) : 0)
		  - ((header) ? CHARHEIGHT : 0)
		  - ((trailer) ? CHARHEIGHT : 0);

  /* Quit here if there is no usable image space. */
  if ((usable.width <= 0) || (usable.height <= 0)) {
    fatal_err((catgets(nlmsg_fd,NL_SETN,4,
				"No space available on page for image.")));
  }

  /* Determine image orientation.  The orientation will only be changed if
   * it was not specified by a command line option.  Portrait mode will be
   * used if either the usable printing area or the image area are square.
   * Portrait mode will also be used if the long dimensions of the usable
   * printing area and the image area match, otherwise landscape mode is
   * used.  Portrait mode really means "don't rotate" and landscape mode
   * means "rotate".
   */
  if (*orient == UNSPECIFIED) {
    if ((usable.width == usable.height)
	|| (xwd_header.pixmap_width == xwd_header.pixmap_height))
      *orient = PORTRAIT;
    else
      *orient = ((usable.width < usable.height)
		 == (xwd_header.pixmap_width < xwd_header.pixmap_height))
	? PORTRAIT : LANDSCAPE;
  }

  /* Set the dots-per-inch print density if it was not specified */
  if (*density <= 0) {
    switch(device) {
       case   LJET: *density = 300;
                    break;
       case   PJET: *density = 90;
                    break;
       case PJETXL: *density = 180;
                    break;
    }
  }

  /* Fit image to available area if scale was not specified */
  if (*scale <= 0)
    *scale = scale_raster(*density, *orient, usable);

  /* Determine image clipping limits */
  set_image_limits(*scale, *density, *orient, usable);

  /* Determine header/trailer string length clipping */
  set_header_trailer_limits(header, trailer, usable.width);

  /* Calculate locations for page layout */
  set_print_locations(*scale, *density, top, left,
		      header, trailer, *orient, position_on_page);

}


unsigned short fullintensity;

#define BLACK 1
#define WHITE 0
#define EMPTY -1
#define MAX_PJ_COLOR 16

#define RGBmatch(r,g,b,i)	(r == i) && (g == i) && (b == i)


/* Colormap array is used to map from the Xcolor array (xwd_colors) index
 * numbers into a pjcolor index number.  This style of mapping is done when
 * interpreting non-Direct/TrueColor visual types.
 */
long *colormap;


/* Pjcolor array is used to hold the scaled RGB triple values 
 * programmed into the printer.
 */
long pjcolor[MAX_PJ_COLOR];


static int color_warning_given = FALSE;


/* Global visual type indicator, used to select color interpretation method. */
char Direct_or_TrueColor;


/* Color index element definition, these are linked into a circular list
 * for interpretation of DirectColor or TrueColor visual types.
 */
typedef struct colorindex {
  long index;
  long pjcolor_index;
  struct colorindex *next;
} COLORINDEX;


/* Global data for color interpretation.  This structure serves as a home
 * for the color index lists (only used when processing DirectColor or
 * TrueColor visual types).  It also holds color processing switches and a	
 * pointer to the output file to reduce parameter passing overhead.
 */
struct {
  int PaintJet;
  int Invert;
  unsigned int CutOff;
  FILE *OutFile;
  COLORINDEX *indexchain, *freechain;
  RGBshiftmask sm;
} color;


static
void setup_RGBshiftmask (
  RGBshiftmask *sm,
  unsigned long rmask, unsigned long gmask, unsigned long bmask)
{
  sm->Rmask = rmask;  sm->Gmask = gmask;  sm->Bmask = bmask;
  sm->Rshift = 0;     sm->Gshift = 0;     sm->Bshift = 0;

  if (!rmask)
    fatal_err((catgets(nlmsg_fd,NL_SETN,5, "red mask for visual is zero.")));
  if (!gmask)
    fatal_err((catgets(nlmsg_fd,NL_SETN,6, "green mask for visual is zero.")));
  if (!bmask)
    fatal_err((catgets(nlmsg_fd,NL_SETN,7, "blue mask for visual is zero.")));

  for (; !(rmask & 1); sm->Rshift++)
    rmask >>= 1;
  for (; !(gmask & 1); sm->Gshift++)
    gmask >>= 1;
  for (; !(bmask & 1); sm->Bshift++)
    bmask >>= 1;
}


static
void swap_black_and_white (void)
{
  /* Reverse black and white in the Xcolor structure array. */

  XColor *color;
  int n;

  for (n=xwd_header.ncolors, color=xwd_colors;  n>0;  n--, color++)
    if (RGBmatch((color->red & fullintensity), (color->green & fullintensity),
				(color->blue & fullintensity), fullintensity))
      color->red = color->green = color->blue = 0;
    else if (RGBmatch(color->red, color->green, color->blue, 0))
      color->red = color->green = color->blue = fullintensity;
}


static
void reset_color_mapping (void)
{
  int n;
  long *cmap;
  COLORINDEX *splice;

  for (n=0; n<MAX_PJ_COLOR; n++)
    pjcolor[n] = EMPTY;

  if (!color.PaintJet) {
    /* preload for monochrome output */
    pjcolor[0] = WHITE;
    pjcolor[1] = BLACK;
  }    

  if (Direct_or_TrueColor) {
    /* move color index chain cells onto the free list */
    if (color.indexchain != NULL) {
      splice = color.indexchain->next;
      color.indexchain->next = color.freechain;
      color.freechain = splice;
      color.indexchain = NULL;
    }
  } else if (color.PaintJet)
    for (n=xwd_header.ncolors, cmap=colormap;  n>0;  n--, cmap++)
      *cmap = EMPTY;
}


#define Intensity(r,g,b)	((r) * 0.30 + (g) * 0.59 + (b) * 0.11)

static
void prepare_color_mapping (
  int invert, int paintjet,
  unsigned int cutoff,
  FILE *out)
{
  int n;
  long *cmap;
  XColor *xcolor;

  for (n = xwd_header.bits_per_rgb, fullintensity = 0; n > 0; n--)
     fullintensity = (fullintensity << 1) | 1;
  for (n = sizeof(short) * 8 - xwd_header.bits_per_rgb; n > 0; n--)
     fullintensity = (fullintensity << 1);
     
  Direct_or_TrueColor =  (xwd_header.visual_class == DirectColor
			  || xwd_header.visual_class == TrueColor);
  color.PaintJet = paintjet;
  color.Invert = invert;
  color.CutOff = cutoff;
  color.OutFile = out;
  color.indexchain = NULL;
  color.freechain = NULL;

  if (Direct_or_TrueColor)
    setup_RGBshiftmask(&color.sm, xwd_header.red_mask,
		       xwd_header.green_mask, xwd_header.blue_mask);
  else {
    if (!(colormap = (long *) malloc(xwd_header.ncolors * sizeof(long))))
      fatal_err((catgets(nlmsg_fd,NL_SETN,24,
		"Could not allocate memory for X-to-printer colormap.")));

    /* For PaintJet, color map assignment will be done one line at a time.
     * So for now just interchange the Xcolor structure's black and white
     * if the -rv command line option was specified.
     */
    if (paintjet && invert)
      swap_black_and_white();

    /* For LaserJet, map each color to black or white based upon the
     * combined intensity of the RGB components.  Note that the normal
     * non-reversed (-rv) LaserJet mapping will represent light areas
     * of the screen as black on the paper.
     */
    if (!paintjet)
      for (n=xwd_header.ncolors, xcolor=xwd_colors, cmap=colormap;  n>0;
	   n--, xcolor++, cmap++)
	*cmap = (Intensity(xcolor->red, xcolor->green, xcolor->blue) < cutoff)
		? (invert ? BLACK : WHITE)
		: (invert ? WHITE : BLACK);
  }
  reset_color_mapping();
}


/* On a PaintJet printer, the programmable color intensity ranges are:
 *
 *	red:	4..90		green:	4..88		blue:	6..85
 *
 * The following macros map the 0..65535 intensity ranges of X colors
 * into the PaintJet's ranges.
 */

#define fixred(x)	(x / 762 + 4)
#define fixgreen(x)	(x / 780 + 4)
#define fixblue(x)	(x / 829 + 6)

#define is_grey(r,g,b)	((r == g) && (r == b))


static
void select_grey (int level, int *r, int *g, int *b)
{
  /* Forced selection of a grey.  This is done since the PaintJet does
   * not do very well when picking greys, they tend to become pink!
   */
  if (level > 66) {  /* white */
    *r = 90;    *g = 88;    *b = 85;
  } else if (level > 35) {
    *r = 43;    *g = 43;    *b = 45;
  } else if (level > 21) {
    *r = 25;    *g = 25;    *b = 33;
  } else if (level > 15) {
    *r = 15;    *g = 16;    *b = 18;
  } else if (level > 11) {
    *r = 14;    *g = 14;    *b = 18;
  } else if (level > 6) {
    *r =  6;    *g =  7;    *b =  8;
  } else {  /* black */
    *r =  4;    *g =  4;    *b =  6;
  }
}


static
int load_printer_color (
  long index,
  int nearmatch,
  enum device device) /* for non Direct/TrueColor */
{
  int n, red, blue, green, xred, xgreen, xblue;
  long compositeRGB;
    
  if (colormap[index] != EMPTY)
    /* printer has already been programmed for this color index */
    return(1);	/* "success" */
  else {
    xred   = xwd_colors[index].red;
    xgreen = xwd_colors[index].green;
    xblue  = xwd_colors[index].blue;
    /* determine the scaled RGB PaintJet color values */
    if (device == PJET) {
       red   = fixred(xred);
       green = fixgreen(xgreen);
       blue  = fixblue(xblue);
       if (is_grey(xred, xgreen, xblue))  /* assist grey selection */
          select_grey(red, &red, &green, &blue);
    }
    compositeRGB = (red << 16) | (green << 8) | blue;
    /* search for a matching or unused PaintJet mapping entry */
    for (n=0; n<MAX_PJ_COLOR; n++) {
      if (pjcolor[n] == compositeRGB) {
	/* record mapping for this index */
	colormap[index] = n;
	/* return "success" */
	return(1);
      } else if (pjcolor[n] == EMPTY) {
	/* download color to printer */
	fprintf(color.OutFile,"\033*v%dA", red);
	fprintf(color.OutFile,"\033*v%dB", green);
	fprintf(color.OutFile,"\033*v%dC", blue);
	fprintf(color.OutFile,"\033*v%dI", n);
	/* record that this is now programmed */
	pjcolor[n] = compositeRGB;
	colormap[index] = n;
	/* return "success" */
	return(1);
      }
    }
    /* unable to find or program this color */
    if (nearmatch)
      colormap[index] = find_nearest_programmed_color(compositeRGB);
  }
  return(0);  /* "failure" */
}


/* Lookup the image color index on the color.indexchain list.  If found
 * return the corresponding printer color index, otherwise -1.  The index
 * chain is a singly linked circular list.  Its head pointer is left at
 * the last cell matched on the theory that this will allow faster lookup
 * for runs of color.
 */
static
int lookup_color_index (long i)
{
  COLORINDEX *start, *current;

  start = current = color.indexchain;

  if (current == NULL)
    return(-1);  /* not found */

  do {
    if (current->index == i) {
      color.indexchain = current;
      return(current->pjcolor_index);  /* found */
    }
    current = current->next;
  } while (current != start);

  return(-1);  /* not found */
}


/* Calculate the individual and composite printer RGB values.  (Only the
 * composite value is set for monochrome output.)
 */
static
void select_printer_color (
  long index,
  int *red, int *green, int *blue,
  long *compositeRGB,
  enum device device)
{
  int xred, xgreen, xblue;

  xred   = xwd_colors[((index & color.sm.Rmask) >> color.sm.Rshift)].red;
  xgreen = xwd_colors[((index & color.sm.Gmask) >> color.sm.Gshift)].green;
  xblue  = xwd_colors[((index & color.sm.Bmask) >> color.sm.Bshift)].blue;

  if (color.PaintJet) {
    if (color.Invert) {
      if (RGBmatch((xred & fullintensity), (xgreen & fullintensity),
				(xblue & fullintensity), fullintensity))
	xred = xgreen = xblue = 0;
      else if (RGBmatch(xred, xgreen, xblue, 0))
	xred = xgreen = xblue = fullintensity;
    }
    /* determine the scaled RGB PaintJet color values */
    if (device == PJET) {
       *red   = fixred(xred);
       *green = fixgreen(xgreen);
       *blue  = fixblue(xblue);
       if (is_grey(xred, xgreen, xblue))  /* assist grey selection */
         select_grey(*red, red, green, blue);
    }
    if (device == PJETXL) {
       *red   = xred >> 8;
       *green = xgreen >> 8;
       *blue  = xblue >> 8;
    }
    *compositeRGB = (*red << 16) | (*green << 8) | *blue;
  } else  /* monochrome */
    *compositeRGB = (Intensity(xred, xgreen, xblue) < color.CutOff)
		    ? (color.Invert ? BLACK : WHITE)
		    : (color.Invert ? WHITE : BLACK);
}



/* Search for a color matching the compositeRGB value in the array of
 * colors already programmed into the printer.  Returns 1 if found,
 * 0 otherwise.  The matching array index is returned in pindex.
 */
static
int color_already_in_printer (long compositeRGB, long *pindex)
{
  int n;

  for (n=0; n<MAX_PJ_COLOR; n++)
    if (pjcolor[n] == EMPTY)
      return(0);  /* not found */
    else if (pjcolor[n] == compositeRGB) {
      *pindex = n;
      return(1);  /* found */
    }
  return(0);  /* not found */
}


static
int program_new_printer_color (
  int red, int green, int blue,
  long compositeRGB,
  long *pindex)
{
  int n;

  for (n=0; n<MAX_PJ_COLOR; n++)
    if (pjcolor[n] == EMPTY) {
      /* download color to printer */
      fprintf(color.OutFile,"\033*v%dA", red);
      fprintf(color.OutFile,"\033*v%dB", green);
      fprintf(color.OutFile,"\033*v%dC", blue);
      fprintf(color.OutFile,"\033*v%dI", n);
      /* record that this is now programmed */
      pjcolor[n] = compositeRGB;
      *pindex = n;
      /* return "success" */
      return(1);
    }
  /* unable to program this color, return "failure" */
  return(0);
}


static
long composite_diff (long x, long y)
{
  long r = (x >> 16 & 0xFF) - (y >> 16 & 0xFF);
  long g = (x >> 8 & 0xFF) - (y >> 8 & 0xFF);
  long b = (x & 0xFF) - (y & 0xFF);

  return(r*r + g*g + b*b);
}


static
long find_nearest_programmed_color (long compositeRGB)
{
  int n, nearest = 0;
  long neardiff = composite_diff(pjcolor[0], compositeRGB);
  long diff;

  for (n=1; n<MAX_PJ_COLOR; n++) {
    diff = composite_diff(pjcolor[n], compositeRGB);
    if (diff < neardiff) {
      neardiff = diff;
      nearest = n;
    }
  }
  return(nearest);
}


static
void add_index_to_chain (
  long cindex,
  long pindex)
{
  COLORINDEX *new;

  /* Get a new cell for the color index chain.  Take it from the free list
   * if possible, otherwise malloc space.
   */
  if (color.freechain == NULL) {
    if (!(new = (COLORINDEX *) malloc(sizeof(COLORINDEX))))
      fatal_err((catgets(nlmsg_fd,NL_SETN,8,
			"Could not allocate memory for color translation.")));
  } else {
    new = color.freechain;
    color.freechain = color.freechain->next;
  }

  /* put index values in the new cell */
  new->index = cindex;
  new->pjcolor_index = pindex;

  /* link the new cell into the chain */
  if (color.indexchain == NULL)
    new->next = new;
  else {
    new->next = color.indexchain->next;
    color.indexchain->next = new;
  }
  /* leave head pointer at the new cell */
  color.indexchain = new;
}


static
int load_printer_color_DT (
  long index,
  int nearmatch,
  enum device device) /* for Direct/TrueColor */
{
  int pjred, pjgreen, pjblue;
  long compositeRGB;
  long pindex;

  if (lookup_color_index(index) >= 0)
    return(1);	/* "success" */
  else {
    select_printer_color(index, &pjred, &pjgreen, &pjblue, &compositeRGB,
				device);
    if (color_already_in_printer(compositeRGB, &pindex)) {
      add_index_to_chain(index, pindex);
      return(1);  /* success */
    } else if (program_new_printer_color(pjred, pjgreen, pjblue,
					 compositeRGB, &pindex)) {
      add_index_to_chain(index, pindex);
      return(1);  /* success */
    } else if (nearmatch) {
      add_index_to_chain(index, find_nearest_programmed_color(compositeRGB));
      return(0);  /* failure, sorta... */
    }
  }
  return(0); /* failure */
}


static
int load_line_colors (
  long *line,
  int length, int nearmatch,
  enum device device)
{
  int result = 1;  /* initialized to "success" */

  for (; length>0; length--, line++) {
    result &= Direct_or_TrueColor
	      ? load_printer_color_DT(*line, nearmatch, device)
	      : load_printer_color(*line, nearmatch, device);
    if (!(nearmatch || result))
      break;
  }
  return(result);
}


static
void download_colors (
  long *line,
  int length,
  enum device device)
{
  /* For the first attempt at loading the colors for a line only exact
   * color matches are accepted.  If this fails, the closest colors are
   * accepted on the second attempt.
   *
   * Note: The first "if" test below bypasses the initial color loading
   * attempt for monochrome output (which will only come here for Direct
   * or TrueColor mono).  This forces reset_color_mapping which is
   * necessary to keep the color index chain down to a tolerable length.
   */
  if (!color.PaintJet || !load_line_colors(line, length, FALSE, device)) {
    reset_color_mapping();
    if (!load_line_colors(line, length, TRUE, device) &&
					!color_warning_given) {
      fprintf(stderr,(catgets(nlmsg_fd,NL_SETN,9,
			"%s: Warning: Cannot print all image colors.\n")),
		progname);
      color_warning_given = TRUE;
    }
  }
}


static
void validate_visual(void)
{
  int depth = xwd_header.pixmap_depth;
  char *errmsg = catgets(nlmsg_fd,NL_SETN,25,
				"%d bit deep %s bitmap not supported.\n");

  switch (xwd_header.visual_class) {
  case GrayScale:
    if (depth > 8)  fatal_err(errmsg, depth, "GrayScale");    break;
  case StaticGray:
    if (depth > 8)  fatal_err(errmsg, depth, "StaticGray");    break;
  case PseudoColor:
    if (depth > 8)  fatal_err(errmsg, depth, "PseudoColor");    break;
  case StaticColor:
    if (depth > 8)  fatal_err(errmsg, depth, "StaticColor");    break;
  case DirectColor:
  case TrueColor:
    if (depth != 12 && depth != 24)
       fatal_err(errmsg, depth, (xwd_header.visual_class == DirectColor)
                               ? "DirectColor" : "TrueColor");
    break;
  default:
    fatal_err((catgets(nlmsg_fd,NL_SETN,26,
		"visual class #%d not supported.\n")), xwd_header.visual_class);
  }
}

static
void read_xwd_data (FILE *in)
{
#   define WINDOW_NAME_ALLOC	32
    unsigned long swaptest = 1;
    int window_name_size;
    int image_size;
    int n;
    char window_name [WINDOW_NAME_ALLOC];

    /* Read in XWDFileHeader structure */
    if (fread((char*) &xwd_header, 1, XWDHEADERSIZE, in) != XWDHEADERSIZE)
    	fatal_err((catgets(nlmsg_fd,NL_SETN,10,
				"Could not read xwd file's header.")));

    if (*(char *) &swaptest)
        _swaplong((char *) &xwd_header, XWDHEADERSIZE);

    validate_visual();

    /* Skip over window name */
    window_name_size = xwd_header.header_size - XWDHEADERSIZE;
    while (window_name_size > 0) {
    	n = window_name_size > WINDOW_NAME_ALLOC
    	    ? WINDOW_NAME_ALLOC : window_name_size;
    	if (fread(window_name, 1, n, in) != n)
    	    fatal_err((catgets(nlmsg_fd,NL_SETN,11,
				"Could not read xwd file's window name.")));
    	window_name_size -= n;
    }

    /* Allocate space for xwd color structures */
    if (!(xwd_colors = (XColor*) malloc(sizeof(XColor) * xwd_header.ncolors)))
    	fatal_err((catgets(nlmsg_fd,NL_SETN,12,
			"Could not allocate memory for xwdfile color table.")));

    /* Read in xwd color structures */
    for (n = 0; n < xwd_header.ncolors; n++)
    	if (fread(&xwd_colors[n], 1, XCOLORSIZE, in) != XCOLORSIZE)
    	    fatal_err((catgets(nlmsg_fd,NL_SETN,13,
				"Could not read xwd file's color table.")));

    if (*(char *) &swaptest) {
        for (n = 0; n < xwd_header.ncolors; n++) {
            _swaplong((char *) &xwd_colors[n].pixel, sizeof(long));
            _swapshort((char *) &xwd_colors[n].red, 3 * sizeof(short));
        }
     }

    /* Allocate space for xwd image */
    if (xwd_header.pixmap_format == ZPixmap)
    	image_size = 1;
    else if (xwd_header.pixmap_format == XYPixmap)
    	image_size = xwd_header.pixmap_depth;
    else
    	fatal_err((catgets(nlmsg_fd,NL_SETN,14,
			"Image in xwd file is not in Z or XY pixmap format.")));
    image_size *= xwd_header.bytes_per_line * xwd_header.pixmap_height;
    if (!(xwd_image = malloc(image_size)))
    	fatal_err((catgets(nlmsg_fd,NL_SETN,15,
			"Could not allocate memory for xwd file's image.")));

    /* Read in xwd image */
    if (fread(xwd_image, 1, image_size, in) != image_size)
    	fatal_err((catgets(nlmsg_fd,NL_SETN,16,
			"Could not read xwd file's image.")));

}


static
void write_image_prefix (
  FILE *out,
  int scale, int density,
  const char *header,
  enum device device,
  int position_on_page, int initial_formfeed,
  enum orientation orient,
  float gamma,
  int render,
  int slide)
{
  if (initial_formfeed)
    fprintf(out,"\014");

  /* Write out header & positioning commands */
  if (header) {
    if (position_on_page)
      fprintf(out,"\033&a%dH\033&a%dV",
	      /* headerloc x & y are written in decipoints */
	      (int) headerloc.x / 10, (int) headerloc.y / 10);
    fprintf(out,"%s\n", header);
  }

  /* Prepare printer for raster graphics: */

  /* Write image positioning commands */
  if (position_on_page)
    fprintf(out,"\033&a%dH\033&a%dV",
	    /* imageloc x & y are written in decipoints */
	    (int) imageloc.x / 10, (int) imageloc.y / 10);
  
  /* If doing transparencies, tell the printer before raster graphics */
  if (slide && device != LJET)
     fprintf(out, "\033&k3W");

  /* Set printer resolution */
  fprintf(out,"\033*t%dR", density);

  /*
   * do device dependent escape sequences
   */
  if (device == PJET) {
    /* Enable all four "planes" for PaintJet */
    fprintf(out,"\033*r4U");

    /* Set picture width for PaintJet */
    fprintf(out,"\033*r%dS",
	    ((int) (orient == PORTRAIT) ? limit.width : limit.height)
	    * scale);
  }

  /* Enable various options for PaintJet XL */
  if (device == PJETXL) {
    double dotsize;
    int n;

    /* Speed up printing by telling that there
     * will be no negative positioning
     */
    fprintf(out, "\033&a1N");

    if (gamma > 0.009)
       fprintf(out, "\033*t%.2fI", gamma);

    if (render > 0)
       fprintf(out, "\033*t%dJ", render);

    if (Direct_or_TrueColor)
       /* Enable direct by pixel for PaintJet XL */
       fwrite("\033*v6W\000\003\010\010\010\010", 1, 11, out);
    else {
       /* Enable index by pixel for PaintJet XL */
       fwrite("\033*v6W\000\001\010\010\010\010", 1, 11, out);

       /* Program the palette */
       for (n = 0; n < xwd_header.ncolors; n++) {
          fprintf(out,"\033*v%dA", (xwd_colors[n].red >> 8));
          fprintf(out,"\033*v%dB", (xwd_colors[n].green >> 8));
          fprintf(out,"\033*v%dC", (xwd_colors[n].blue >> 8));
          fprintf(out,"\033*v%dI", n);
       }
    }

    /****************************************
     *                                      *
     * PaintJet XL will do its own scaling  *
     *                                      *
     * Set picture width for PaintJet XL    *
     ****************************************/
    fprintf(out,"\033*r%dS",
	    ((int) (orient == PORTRAIT) ? xwd_header.pixmap_width
					: xwd_header.pixmap_height));
    fprintf(out,"\033*r%dT",
	    ((int) (orient == PORTRAIT) ? xwd_header.pixmap_height
					: xwd_header.pixmap_width));

    dotsize = dot_centipoints(scale, density);

    fprintf(out,"\033*t%dH",
		(int)(((orient == PORTRAIT) ? xwd_header.pixmap_width
                                        : xwd_header.pixmap_height)
                        * dotsize / 10));

    fprintf(out,"\033*t%dV",
		(int)(((orient == PORTRAIT) ? xwd_header.pixmap_height
                                        : xwd_header.pixmap_width)
                        * dotsize / 10));
  }

  /* Switch to raster graphics mode */
  if (device != PJETXL)
     fprintf(out,"\033*r1A");
  else
     fprintf(out,"\033*r3A");

}


static
void write_image_suffix (
  FILE *out,
  const char *trailer,
  int position_on_page,
  int slide, int render,
  enum device device)
{
  /* Exit raster graphics mode */
  if (device == PJETXL)
     fprintf(out,"\033*rC");
  else
     fprintf(out,"\033*rB");

  /* If doing transparencies, tell it to stop */
  if (slide && device != LJET)
     fprintf(out, "\033&k1W");

  if (device == PJETXL) {
     /* If selected a rendering algorithm, tell it to stop */
     if (render)
        fprintf(out, "\033*t3J");
     fprintf(out, "\033&a0N");
  }

  /* Write out trailer & positioning commands */
  if (trailer) {
    if (position_on_page)
      fprintf(out,"\033&a%dH\033&a%dV",
	      /* trailerloc x & y are written in decipoints */
	      (int) trailerloc.x / 10, (int) trailerloc.y / 10);
    fprintf(out,"%s\n", trailer);
  }
}


static
unsigned long Z_image_pixel (int x, int y)
{
  int pixel_bytes, offset;
  unsigned char *image;
  unsigned long pixel;

  pixel_bytes = xwd_header.bits_per_pixel >> 3;
  offset = ((xwd_header.bits_per_pixel == 4) ? (x / 2)
            : ((xwd_header.bits_per_pixel == 1) ? (x / 8)
               : (x * pixel_bytes)))
           + (y * xwd_header.bytes_per_line);

  image = (unsigned char *) &xwd_image[offset];

  switch (pixel_bytes) {
  case 0:  /* pixel per nibble or bit per pixel packing */
    if (xwd_header.bits_per_pixel == 1) {
      if (xwd_header.byte_order == MSBFirst)
	pixel = *image >> (7 - (x % 8));
      else
	pixel = *image >> (x % 8);
    } else {  /* xwd_header.bits_per_pixel == 4 */
      if (xwd_header.byte_order == MSBFirst)
	pixel = (x & 1) ? *image : (*image >> 4);
      else
	pixel = (x & 1) ? (*image >> 4) : *image;
    }
    break;
  case 1:
    pixel = *image;
    break;
  case 2:
    pixel = (xwd_header.byte_order == MSBFirst)
	     ? ((unsigned long)*image << 8 | *(image + 1))
	     : (*image | (unsigned long)*(image + 1) << 8);
    break;
  case 3:
    pixel = (xwd_header.byte_order == MSBFirst)
	     ? ((unsigned long)*image << 16 |
		(unsigned long)*(image + 1) << 8 |
		(unsigned long)*(image + 2))
	     : (*image |
		(unsigned long)*(image + 1) << 8 |
		(unsigned long)*(image + 2) << 16);
    break;
  case 4:
    pixel = (xwd_header.byte_order == MSBFirst)
	     ? ((unsigned long)*image << 24 |
		(unsigned long)*(image+1) << 16 |
		(unsigned long)*(image+2) << 8 |
		*(image+3))
	     : (*image |
		(unsigned long)*(image+1) << 8 |
		(unsigned long)*(image+2) << 16 |
		(unsigned long)*(image+3) << 24);
    break;
  }
  return (pixel & Z_pixel_mask);
}


static
unsigned long XY_image_pixel (int x, int y)
{
  int plane_start, line_start, bytes_per_bitmap_unit, bitmap_unit_start,
      byte_within_bitmap_unit, offset, byte_mask;

  plane_start = (xwd_header.pixmap_depth - 1) * xwd_header.pixmap_height
		* xwd_header.bytes_per_line;
  line_start = xwd_header.bytes_per_line * y;
  bytes_per_bitmap_unit = xwd_header.bitmap_unit >> 3;
  bitmap_unit_start = (x / xwd_header.bitmap_unit) * bytes_per_bitmap_unit;
  byte_within_bitmap_unit = (xwd_header.byte_order == MSBFirst)
    ? (x % xwd_header.bitmap_unit) >> 3
    :  bytes_per_bitmap_unit - ((x % xwd_header.bitmap_unit) >> 3) - 1;

  offset = plane_start + line_start + bitmap_unit_start
	   + byte_within_bitmap_unit;

  byte_mask = (xwd_header.bitmap_bit_order == MSBFirst)
      ? 0x80 >> (x % 8) : 0x01 << (x % 8);

  return(xwd_image[offset] & byte_mask ? 1 : 0);
}


static
void direct_by_pixel(
  FILE *out,
  long *line,
  int length,
  enum device device)
{
   int red, green, blue;
   long compositeRGB;

   fprintf(out, "\033*b%dW", length * 3);
   for (; length>0; length--, line++) {
      select_printer_color(*line, &red, &green, &blue, &compositeRGB, device);
      fprintf(out, "%c%c%c", (char) red, (char) green, (char) blue);
   }
}


static
void index_by_pixel(
  FILE *out,
  long *line,
  int length)
{
   register int n;
   long *lp;
   char *line_pixels;
   register char *lc;
  
  if (!(line_pixels = malloc(length)))
    fatal_err((catgets(nlmsg_fd,NL_SETN,17,
			"Could not allocate raster line memory.")));

   for (n=0, lc=line_pixels, lp=line;  n<length;  n++)
      *lc++ = (char) *lp++;

   fprintf(out, "\033*b%dW", length);
   fwrite(line_pixels, 1, length, out);

   free(line_pixels);
}


static
void write_raster_line (
  FILE *out,
  int scale,
  enum device device,
  long *line,
  int length)
{
  int planes = (device == PJET) ? 4 : 1;
  int raster_bytes = (length * scale + 7) / 8;
  register int bytebits, n, p, e, bit;
  long *lp;
  char *line_colors, *raster_line;
  register char *lc, *rl, byte = 0;
  
  if (device == PJETXL) {
     if (Direct_or_TrueColor)
        direct_by_pixel(out, line, length, device);
     else
        index_by_pixel(out, line, length);
     return;
  }

  if (!(line_colors = malloc(length))
      || !(raster_line = malloc(raster_bytes * planes)))
    fatal_err((catgets(nlmsg_fd,NL_SETN,17,
			"Could not allocate raster line memory.")));

  if (device == PJET || Direct_or_TrueColor)
    download_colors(line, length, device);

  /* Map the line's colors into output color index numbers */
  if (Direct_or_TrueColor)
    for (n=0, lc=line_colors, lp=line;  n<length;  n++)
      *lc++ = (char) lookup_color_index(*lp++);
  else
    for (n=0, lc=line_colors, lp=line;  n<length;  n++)
      *lc++ = (char) colormap[*lp++];

  for (p=0;  p<planes;  p++) {
    bytebits = 0;
    n = length;
    lc = line_colors;
    rl = &raster_line[raster_bytes * p];
    while (n-- > 0) {
      bit = (*lc++ >> p) & 0x01;
      e = scale;
      while (e--) {
	byte = (byte << 1) | bit;
	bytebits++;
	if (bytebits == 8) {
	  *rl++ = byte;
	  bytebits = 0;
	}
      }
    }
    if (bytebits)
      *rl = byte << (8 - bytebits);
  }

  e = scale;
  while (e--) {
    for (p=0;  p<planes;  p++) {
      fprintf(out,"\033*b%d%c", raster_bytes, (p+1 == planes) ? 'W' : 'V');
      fwrite(&raster_line[raster_bytes * p], 1, raster_bytes, out);
    }
  }

  free(line_colors);
  free(raster_line);
}


static
void write_portrait_Z_image (
  FILE *out,
  int scale,
  enum device device)
{
  int x, y;
  int width = limit.width;
  int height = limit.height;
  long *line, *lp;

  if (!(line = (long *) malloc(width * sizeof(long))))
    fatal_err((catgets(nlmsg_fd,NL_SETN,18,
			"Could not allocate memory for image line buffer.")));

  for (y=0;  y<height;  y++) {

    for (x=0, lp=line;  x<width;  x++)
      *lp++ = Z_image_pixel(x,y);

    write_raster_line(out, scale, device, line, width);
  }
}


static
void write_landscape_Z_image (
  FILE *out,
  int scale,
  enum device device)
{
  int x, y;
  int width = limit.height;
  int height = limit.width;
  long *line, *lp;

  if (!(line = (long *) malloc(width * sizeof(long))))
    fatal_err((catgets(nlmsg_fd,NL_SETN,19,
			"Could not allocate memory for image line buffer.")));

  for (x=0;  x<height;  x++) {

    for (y=width-1, lp=line;  y>=0;  y--)
      *lp++ = Z_image_pixel(x,y);

    write_raster_line(out, scale, device, line, width);
  }
}


static
void write_portrait_XY_image (
  FILE *out,
  int scale,
  enum device device)
{
  int x, y;
  int width = limit.width;
  int height = limit.height;
  long *line, *lp;

  if (!(line = (long *) malloc(width * sizeof(long))))
    fatal_err((catgets(nlmsg_fd,NL_SETN,20,
			"Could not allocate memory for image line buffer.")));

  for (y=0;  y<height;  y++) {

    for (x=0, lp=line;  x<width;  x++)
      *lp++ = XY_image_pixel(x,y);

    write_raster_line(out, scale, device, line, width);
  }
}


static
void write_landscape_XY_image (
  FILE *out,
  int scale,
  enum device device)
{
  int x, y;
  int width = limit.height;
  int height = limit.width;
  long *line, *lp;

  if (!(line = (long *) malloc(width * sizeof(long))))
    fatal_err((catgets(nlmsg_fd,NL_SETN,21,
			"Could not allocate memory for image line buffer.")));

  for (x=0;  x<height;  x++) {

    for (y=width-1, lp=line;  y>=0;  y--)
      *lp++ = XY_image_pixel(x,y);

    write_raster_line(out, scale, device, line, width);
  }
}


static
void write_Z_image (
  FILE *out,
  int scale,
  enum orientation orient,
  enum device device)
{
  if (orient == PORTRAIT) {
    write_portrait_Z_image(out, scale, device);
  } else
    write_landscape_Z_image(out, scale, device);
}


static
void write_XY_image (
  FILE *out,
  int scale,
  enum orientation orient,
  enum device device)
{
  if (xwd_header.pixmap_depth > 1)
    fatal_err((catgets(nlmsg_fd,NL_SETN,22,
		"XY format image, multiplane images must be Z format.")));

  if (orient == PORTRAIT) {
    write_portrait_XY_image(out, scale, device);
  } else
    write_landscape_XY_image(out, scale, device);
}


static
void write_image (
  FILE *out,
  int scale,
  enum orientation orient,
  enum device device)
{
  switch (xwd_header.pixmap_format) {
  case XYPixmap:
    write_XY_image(out, scale, orient, device);  break;
  case  ZPixmap:
    write_Z_image(out, scale, orient, device);   break;
  default: 
    fatal_err((catgets(nlmsg_fd,NL_SETN,23, "image not in XY or Z format.")));
  }
}


void x2jet(
  FILE *in, FILE *out,
  int scale, int density,
  int width, int height, int left, int top,  /* in 300ths of an inch */
  const char *header, const char *trailer,
  enum orientation orient,
  int invert, int initial_formfeed, int position_on_page, int slide,
  enum device device,
  unsigned int cutoff,
  float gamma,
  int render)
{
  int paintjet = FALSE;

  true_scale = scale;

  if (device != LJET)
     paintjet = TRUE;

  read_xwd_data(in);

  Z_pixel_mask = ~(0xFFFFFFFFL << xwd_header.pixmap_depth);

  prepare_color_mapping(invert, paintjet, cutoff, out);

  scale_and_orient_image(&scale, &density, width, height, left, top,
			 header, trailer,
			 &orient, position_on_page, device);

  write_image_prefix(out, scale, density, header, device, position_on_page,
		     initial_formfeed, orient, gamma, render, slide);
  
  write_image(out, scale, orient, device);

  write_image_suffix(out, trailer, position_on_page, slide, render, device);

  fclose(out);
}

static
void fatal_err (const char *s, ...)
{
  fprintf(stderr, "%s: %s\n", progname, s);
  exit(EXIT_FAILURE);
}


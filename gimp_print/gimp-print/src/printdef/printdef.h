/*
 * "$Id: printdef.h,v 1.1.1.1 2003/01/27 19:05:32 jlovell Exp $"
 *
 *   I18N header file for the gimp-print plugin.
 *
 *   Copyright 1997-2000 Michael Sweet (mike@easysw.com),
 *	Robert Krawitz (rlk@alum.mit.edu) and Michael Natterer (mitch@gimp.org)
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

#define OUTPUT_GRAY		0	/* Grayscale output */
#define OUTPUT_COLOR		1	/* Color output */
#define OUTPUT_GRAY_COLOR	2 	/* Grayscale output using color */

#define ORIENT_AUTO		-1	/* Best orientation */
#define ORIENT_PORTRAIT		0	/* Portrait orientation */
#define ORIENT_LANDSCAPE	1	/* Landscape orientation */
#define ORIENT_UPSIDEDOWN	2	/* Reverse portrait orientation */
#define ORIENT_SEASCAPE		3	/* Reverse landscape orientation */

#define IMAGE_LINE_ART		0
#define IMAGE_SOLID_TONE	1
#define IMAGE_CONTINUOUS	2
#define IMAGE_MONOCHROME	3
#define NIMAGE_TYPES		4

#define COLOR_MODEL_RGB		0
#define COLOR_MODEL_CMY		1

typedef struct					/* Plug-in variables */
{
  char	output_to[256],		/* Name of file or command to print to */
	driver[64],		/* Name of printer "driver" */
	ppd_file[256],		/* PPD file */
	resolution[64],		/* Resolution */
	media_size[64],		/* Media size */
	media_type[64],		/* Media type */
	media_source[64],	/* Media source */
	ink_type[64],		/* Ink or cartridge */
	dither_algorithm[64];	/* Dithering algorithm */
  int	output_type;		/* Color or grayscale output */
  float	brightness;		/* Output brightness */
  float	scaling;		/* Scaling, percent of printable area */
  int	orientation,		/* Orientation - 0 = port., 1 = land.,
				   -1 = auto */
	left,			/* Offset from lower-lefthand corner, points */
	top;			/* ... */
  float gamma;                  /* Gamma */
  float contrast,		/* Output Contrast */
	cyan,			/* Output red level */
	magenta,		/* Output green level */
	yellow;			/* Output blue level */
  float	saturation;		/* Output saturation */
  float	density;		/* Maximum output density */
  int	image_type;		/* Image type (line art etc.) */
  int	unit;			/* Units for preview area 0=Inch 1=Metric */
  float app_gamma;		/* Application gamma */
  int	page_width;		/* Width of page in points */
  int	page_height;		/* Height of page in points */
  int	input_color_model;	/* Color model for this device */
  int	output_color_model;	/* Color model for this device */
  void  *lut;			/* Look-up table */
  void  *driver_data;		/* Private data of the driver */
  unsigned char *cmap;		/* Color map */
  void (*outfunc)(void *data, const char *buffer, size_t bytes);
  void *outdata;
  void (*errfunc)(void *data, const char *buffer, size_t bytes);
  void *errdata;
} stp_vars_t;

typedef struct stp_printer
{
  const char	*long_name,			/* Long name for UI */
	*driver;			/* Short name for printrc file */
  int	model;				/* Model number */
  const char *printfuncs;
  stp_vars_t printvars;
} stp_printer_t;

typedef union yylv {
  int ival;
  double dval;
  char *sval;
} YYSTYPE;

extern YYSTYPE yylval;
extern stp_printer_t thePrinter;

#include "printdefy.h"


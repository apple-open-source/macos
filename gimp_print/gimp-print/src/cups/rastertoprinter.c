/*
 * "$Id: rastertoprinter.c,v 1.1.1.1 2003/01/27 19:05:32 jlovell Exp $"
 *
 *   GIMP-print based raster filter for the Common UNIX Printing System.
 *
 *   Copyright 1993-2002 by Easy Software Products.
 *
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License,
 *   version 2, as published by the Free Software Foundation.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, please contact Easy Software
 *   Products at:
 *
 *       Attn: CUPS Licensing Information
 *       Easy Software Products
 *       44141 Airport View Drive, Suite 204
 *       Hollywood, Maryland 20636-3111 USA
 *
 *       Voice: (301) 373-9603
 *       EMail: cups-info@cups.org
 *         WWW: http://www.cups.org
 *
 * Contents:
 *
 *   main()                    - Main entry and processing of driver.
 *   cups_writefunc()          - Write data to a file...
 *   cancel_job()              - Cancel the current job...
 *   Image_bpp()               - Return the bytes-per-pixel of an image.
 *   Image_get_appname()       - Get the application we are running.
 *   Image_get_row()           - Get one row of the image.
 *   Image_height()            - Return the height of an image.
 *   Image_init()              - Initialize an image.
 *   Image_note_progress()     - Notify the user of our progress.
 *   Image_progress_conclude() - Close the progress display.
 *   Image_progress_init()     - Initialize progress display.
 *   Image_rotate_ccw()        - Rotate the image counter-clockwise
 *                               (unsupported).
 *   Image_width()             - Return the width of an image.
 */

/*
 * Include necessary headers...
 */

#include <cups/cups.h>
#include <cups/ppd.h>
#include <cups/raster.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif
#ifdef INCLUDE_GIMP_PRINT_H
#include INCLUDE_GIMP_PRINT_H
#else
#include <gimp-print/gimp-print.h>
#endif
#include "../../lib/libprintut.h"

/* Solaris with gcc has problems because gcc's limits.h doesn't #define */
/* this */
#ifndef CHAR_BIT
#define CHAR_BIT 8
#endif

/*
 * Structure for page raster data...
 */

typedef struct
{
  cups_raster_t		*ras;		/* Raster stream to read from */
  int			page;		/* Current page number */
  int			row;		/* Current row number */
  int			left;
  int			right;
  int			bottom;
  int			top;
  int			width;
  int			height;
  cups_page_header_t	header;		/* Page header from file */
} cups_image_t;

static void	cups_writefunc(void *file, const char *buf, size_t bytes);
static void	cancel_job(int sig);
static const char *Image_get_appname(stp_image_t *image);
static void	 Image_progress_conclude(stp_image_t *image);
static void	Image_note_progress(stp_image_t *image,
				    double current, double total);
static void	Image_progress_init(stp_image_t *image);
static stp_image_status_t Image_get_row(stp_image_t *image,
					unsigned char *data, int row);
static int	Image_height(stp_image_t *image);
static int	Image_width(stp_image_t *image);
static int	Image_bpp(stp_image_t *image);
static void	Image_rotate_180(stp_image_t *image);
static void	Image_rotate_cw(stp_image_t *image);
static void	Image_rotate_ccw(stp_image_t *image);
static void	Image_init(stp_image_t *image);

static stp_image_t theImage =
{
  Image_init,
  NULL,				/* reset */
  NULL,				/* transpose */
  NULL,				/* hflip */
  NULL,				/* vflip */
  NULL,				/* crop */
  Image_rotate_ccw,
  Image_rotate_cw,
  Image_rotate_180,
  Image_bpp,
  Image_width,
  Image_height,
  Image_get_row,
  Image_get_appname,
  Image_progress_init,
  Image_note_progress,
  Image_progress_conclude,
  NULL
};

static volatile stp_image_status_t Image_status;

/*
 * 'main()' - Main entry and processing of driver.
 */

int					/* O - Exit status */
main(int  argc,				/* I - Number of command-line arguments */
     char *argv[])			/* I - Command-line arguments */
{
  int			fd;		/* File descriptor */
  cups_image_t		cups;		/* CUPS image */
  const char		*ppdfile;	/* PPD environment variable */
  ppd_file_t		*ppd;		/* PPD file */
  ppd_option_t		*option;	/* PPD option */
  stp_printer_t		printer;	/* Printer driver */
  stp_vars_t		v;		/* Printer driver variables */
  stp_papersize_t	size;		/* Paper size */
  char			*buffer;	/* Overflow buffer */
  int			num_options;	/* Number of CUPS options */
  cups_option_t		*options;	/* CUPS options */
  const char		*val;		/* CUPS option value */
  int			num_res;	/* Number of printer resolutions */
  stp_param_t		*res;		/* Printer resolutions */
  float			stp_gamma,	/* STP options */
			stp_brightness,
			stp_cyan,
			stp_magenta,
			stp_yellow,
			stp_contrast,
			stp_saturation,
			stp_density;


 /*
  * Initialise libgimpprint
  */

  theImage.rep = &cups;

  stp_init();

 /*
  * Check for valid arguments...
  */

  if (argc < 6 || argc > 7)
  {
   /*
    * We don't have the correct number of arguments; write an error message
    * and return.
    */

    fputs("ERROR: rastertoprinter job-id user title copies options [file]\n", stderr);
    return (1);
  }

  Image_status = STP_IMAGE_OK;

 /*
  * Get the PPD file...
  */

  if ((ppdfile = getenv("PPD")) == NULL)
  {
    fputs("ERROR: Fatal error: PPD environment variable not set!\n", stderr);
    return (1);
  }

  if ((ppd = ppdOpenFile(ppdfile)) == NULL)
  {
    fprintf(stderr, "ERROR: Fatal error: Unable to load PPD file \"%s\"!\n",
            ppdfile);
    return (1);
  }

  if (ppd->modelname == NULL)
  {
    fprintf(stderr, "ERROR: Fatal error: No ModelName attribute in PPD file \"%s\"!\n",
            ppdfile);
    ppdClose(ppd);
    return (1);
  }

 /*
  * Get the STP options, if any...
  */

  num_options = cupsParseOptions(argv[5], 0, &options);

  if ((val = cupsGetOption("stpGamma", num_options, options)) != NULL)
    stp_gamma = atof(val) * 0.001;
  else if ((option = ppdFindOption(ppd, "stpGamma")) != NULL)
    stp_gamma = atof(option->defchoice) * 0.001;
  else
    stp_gamma = 1.0;

  if ((val = cupsGetOption("stpBrightness", num_options, options)) != NULL)
    stp_brightness = atof(val) * 0.001;
  else if ((option = ppdFindOption(ppd, "stpBrightness")) != NULL)
    stp_brightness = atof(option->defchoice) * 0.001;
  else
    stp_brightness = 1.0;

  if ((val = cupsGetOption("stpCyan", num_options, options)) != NULL)
    stp_cyan = atof(val) * 0.001;
  else if ((option = ppdFindOption(ppd, "stpCyan")) != NULL)
    stp_cyan = atof(option->defchoice) * 0.001;
  else
    stp_cyan = 1.0;

  if ((val = cupsGetOption("stpMagenta", num_options, options)) != NULL)
    stp_magenta = atof(val) * 0.001;
  else if ((option = ppdFindOption(ppd, "stpMagenta")) != NULL)
    stp_magenta = atof(option->defchoice) * 0.001;
  else
    stp_magenta = 1.0;

  if ((val = cupsGetOption("stpYellow", num_options, options)) != NULL)
    stp_yellow = atof(val) * 0.001;
  else if ((option = ppdFindOption(ppd, "stpYellow")) != NULL)
    stp_yellow = atof(option->defchoice) * 0.001;
  else
    stp_yellow = 1.0;

  if ((val = cupsGetOption("stpContrast", num_options, options)) != NULL)
    stp_contrast = atof(val) * 0.001;
  else if ((option = ppdFindOption(ppd, "stpContrast")) != NULL)
    stp_contrast = atof(option->defchoice) * 0.001;
  else
    stp_contrast = 1.0;

  if ((val = cupsGetOption("stpSaturation", num_options, options)) != NULL)
    stp_saturation = atof(val) * 0.001;
  else if ((option = ppdFindOption(ppd, "stpSaturation")) != NULL)
    stp_saturation = atof(option->defchoice) * 0.001;
  else
    stp_saturation = 1.0;

  if ((val = cupsGetOption("stpDensity", num_options, options)) != NULL)
    stp_density = atof(val) * 0.001;
  else if ((option = ppdFindOption(ppd, "stpDensity")) != NULL)
    stp_density = atof(option->defchoice) * 0.001;
  else
    stp_density = 1.0;

 /*
  * Figure out which driver to use...
  */

  if ((printer = stp_get_printer_by_driver(ppd->modelname)) == NULL)
  {
    fprintf(stderr, "ERROR: Fatal error: Unable to find driver named \"%s\"!\n",
            ppd->modelname);
    ppdClose(ppd);
    return (1);
  }

  ppdClose(ppd);

 /*
  * Get the resolution options...
  */

  res = stp_printer_get_printfuncs(printer)->parameters(printer, NULL,
                                                        "Resolution", &num_res);

 /*
  * Open the page stream...
  */

  if (argc == 7)
  {
    if ((fd = open(argv[6], O_RDONLY)) == -1)
    {
      perror("ERROR: Unable to open raster file - ");
      sleep(1);
      return (1);
    }
  }
  else
    fd = 0;

  cups.ras = cupsRasterOpen(fd, CUPS_RASTER_READ);

 /*
  * Process pages as needed...
  */

  cups.page = 0;

  while (cupsRasterReadHeader(cups.ras, &cups.header))
  {
   /*
    * Update the current page...
    */

    cups.row = 0;

    fprintf(stderr, "PAGE: %d 1\n", cups.page);

   /*
    * Debugging info...
    */

    fprintf(stderr, "DEBUG: StartPage...\n");
    fprintf(stderr, "DEBUG: MediaClass = \"%s\"\n", cups.header.MediaClass);
    fprintf(stderr, "DEBUG: MediaColor = \"%s\"\n", cups.header.MediaColor);
    fprintf(stderr, "DEBUG: MediaType = \"%s\"\n", cups.header.MediaType);
    fprintf(stderr, "DEBUG: OutputType = \"%s\"\n", cups.header.OutputType);

    fprintf(stderr, "DEBUG: AdvanceDistance = %d\n", cups.header.AdvanceDistance);
    fprintf(stderr, "DEBUG: AdvanceMedia = %d\n", cups.header.AdvanceMedia);
    fprintf(stderr, "DEBUG: Collate = %d\n", cups.header.Collate);
    fprintf(stderr, "DEBUG: CutMedia = %d\n", cups.header.CutMedia);
    fprintf(stderr, "DEBUG: Duplex = %d\n", cups.header.Duplex);
    fprintf(stderr, "DEBUG: HWResolution = [ %d %d ]\n", cups.header.HWResolution[0],
            cups.header.HWResolution[1]);
    fprintf(stderr, "DEBUG: ImagingBoundingBox = [ %d %d %d %d ]\n",
            cups.header.ImagingBoundingBox[0], cups.header.ImagingBoundingBox[1],
            cups.header.ImagingBoundingBox[2], cups.header.ImagingBoundingBox[3]);
    fprintf(stderr, "DEBUG: InsertSheet = %d\n", cups.header.InsertSheet);
    fprintf(stderr, "DEBUG: Jog = %d\n", cups.header.Jog);
    fprintf(stderr, "DEBUG: LeadingEdge = %d\n", cups.header.LeadingEdge);
    fprintf(stderr, "DEBUG: Margins = [ %d %d ]\n", cups.header.Margins[0],
            cups.header.Margins[1]);
    fprintf(stderr, "DEBUG: ManualFeed = %d\n", cups.header.ManualFeed);
    fprintf(stderr, "DEBUG: MediaPosition = %d\n", cups.header.MediaPosition);
    fprintf(stderr, "DEBUG: MediaWeight = %d\n", cups.header.MediaWeight);
    fprintf(stderr, "DEBUG: MirrorPrint = %d\n", cups.header.MirrorPrint);
    fprintf(stderr, "DEBUG: NegativePrint = %d\n", cups.header.NegativePrint);
    fprintf(stderr, "DEBUG: NumCopies = %d\n", cups.header.NumCopies);
    fprintf(stderr, "DEBUG: Orientation = %d\n", cups.header.Orientation);
    fprintf(stderr, "DEBUG: OutputFaceUp = %d\n", cups.header.OutputFaceUp);
    fprintf(stderr, "DEBUG: PageSize = [ %d %d ]\n", cups.header.PageSize[0],
            cups.header.PageSize[1]);
    fprintf(stderr, "DEBUG: Separations = %d\n", cups.header.Separations);
    fprintf(stderr, "DEBUG: TraySwitch = %d\n", cups.header.TraySwitch);
    fprintf(stderr, "DEBUG: Tumble = %d\n", cups.header.Tumble);
    fprintf(stderr, "DEBUG: cupsWidth = %d\n", cups.header.cupsWidth);
    fprintf(stderr, "DEBUG: cupsHeight = %d\n", cups.header.cupsHeight);
    fprintf(stderr, "DEBUG: cupsMediaType = %d\n", cups.header.cupsMediaType);
    fprintf(stderr, "DEBUG: cupsBitsPerColor = %d\n", cups.header.cupsBitsPerColor);
    fprintf(stderr, "DEBUG: cupsBitsPerPixel = %d\n", cups.header.cupsBitsPerPixel);
    fprintf(stderr, "DEBUG: cupsBytesPerLine = %d\n", cups.header.cupsBytesPerLine);
    fprintf(stderr, "DEBUG: cupsColorOrder = %d\n", cups.header.cupsColorOrder);
    fprintf(stderr, "DEBUG: cupsColorSpace = %d\n", cups.header.cupsColorSpace);
    fprintf(stderr, "DEBUG: cupsCompression = %d\n", cups.header.cupsCompression);
    fprintf(stderr, "DEBUG: cupsRowCount = %d\n", cups.header.cupsRowCount);
    fprintf(stderr, "DEBUG: cupsRowFeed = %d\n", cups.header.cupsRowFeed);
    fprintf(stderr, "DEBUG: cupsRowStep = %d\n", cups.header.cupsRowStep);

   /*
    * Setup printer driver variables...
    */

    if (cups.page == 0)
      {
	v = stp_allocate_copy(stp_printer_get_printvars(printer));

	stp_set_app_gamma(v, 1.0);
	stp_set_brightness(v, stp_brightness);
	stp_set_contrast(v, stp_contrast);
	stp_set_cyan(v, stp_cyan);
	stp_set_magenta(v, stp_magenta);
	stp_set_yellow(v, stp_yellow);
	stp_set_saturation(v, stp_saturation);
	stp_set_density(v, stp_density);
	stp_set_scaling(v, 0); /* No scaling */
	stp_set_cmap(v, NULL);
	stp_set_page_width(v, cups.header.PageSize[0]);
	stp_set_page_height(v, cups.header.PageSize[1]);
	stp_set_left(v, 0);
	stp_set_top(v, 0);
	stp_set_orientation(v, ORIENT_PORTRAIT);
	stp_set_gamma(v, stp_gamma);
	stp_set_image_type(v, cups.header.cupsRowCount);
	stp_set_outfunc(v, cups_writefunc);
	stp_set_errfunc(v, cups_writefunc);
	stp_set_outdata(v, stdout);
	stp_set_errdata(v, stderr);

	switch (cups.header.cupsColorSpace)
	  {
	  case CUPS_CSPACE_W :
	    stp_set_output_type(v, OUTPUT_GRAY);
	    break;
	  case CUPS_CSPACE_K :
	    stp_set_output_type(v, OUTPUT_MONOCHROME);
	    break;
	  case CUPS_CSPACE_RGB :
	    stp_set_output_type(v, OUTPUT_COLOR);
	    break;
	  case CUPS_CSPACE_CMYK :
	    stp_set_output_type(v, OUTPUT_RAW_CMYK);
	    break;
	  default :
	    fprintf(stderr, "ERROR: Bad colorspace %d!",
		    cups.header.cupsColorSpace);
	    break;
	  }

	if (cups.header.cupsRowStep >= stp_dither_algorithm_count())
	  fprintf(stderr, "ERROR: Unable to set dither algorithm!\n");
	else
	  stp_set_dither_algorithm(v,
				   stp_dither_algorithm_name(cups.header.cupsRowStep));

	stp_set_media_source(v, cups.header.MediaClass);
	stp_set_media_type(v, cups.header.MediaType);
	stp_set_ink_type(v, cups.header.OutputType);

	fprintf(stderr, "DEBUG: PageSize = %dx%d\n", cups.header.PageSize[0],
		cups.header.PageSize[1]);

	if ((size = stp_get_papersize_by_size(cups.header.PageSize[1],
					      cups.header.PageSize[0])) != NULL)
	  stp_set_media_size(v, stp_papersize_get_name(size));
	else
	  fprintf(stderr, "ERROR: Unable to get media size!\n");

	if (cups.header.cupsCompression >= num_res)
	  fprintf(stderr, "ERROR: Unable to set printer resolution!\n");
	else
	  stp_set_resolution(v, res[cups.header.cupsCompression].name);
	stp_set_job_mode(v, STP_JOB_MODE_JOB);
	stp_merge_printvars(v, stp_printer_get_printvars(printer));
	fprintf(stderr, "DEBUG: stp_get_output_to(v) |%s|\n", stp_get_output_to(v));
	fprintf(stderr, "DEBUG: stp_get_driver(v) |%s|\n", stp_get_driver(v));
	fprintf(stderr, "DEBUG: stp_get_ppd_file(v) |%s|\n", stp_get_ppd_file(v));
	fprintf(stderr, "DEBUG: stp_get_resolution(v) |%s|\n", stp_get_resolution(v));
	fprintf(stderr, "DEBUG: stp_get_media_size(v) |%s|\n", stp_get_media_size(v));
	fprintf(stderr, "DEBUG: stp_get_media_type(v) |%s|\n", stp_get_media_type(v));
	fprintf(stderr, "DEBUG: stp_get_media_source(v) |%s|\n", stp_get_media_source(v));
	fprintf(stderr, "DEBUG: stp_get_ink_type(v) |%s|\n", stp_get_ink_type(v));
	fprintf(stderr, "DEBUG: stp_get_dither_algorithm(v) |%s|\n", stp_get_dither_algorithm(v));
	fprintf(stderr, "DEBUG: stp_get_output_type(v) |%d|\n", stp_get_output_type(v));
	fprintf(stderr, "DEBUG: stp_get_orientation(v) |%d|\n", stp_get_orientation(v));
	fprintf(stderr, "DEBUG: stp_get_left(v) |%d|\n", stp_get_left(v));
	fprintf(stderr, "DEBUG: stp_get_top(v) |%d|\n", stp_get_top(v));
	fprintf(stderr, "DEBUG: stp_get_image_type(v) |%d|\n", stp_get_image_type(v));
	fprintf(stderr, "DEBUG: stp_get_unit(v) |%d|\n", stp_get_unit(v));
	fprintf(stderr, "DEBUG: stp_get_page_width(v) |%d|\n", stp_get_page_width(v));
	fprintf(stderr, "DEBUG: stp_get_page_height(v) |%d|\n", stp_get_page_height(v));
	fprintf(stderr, "DEBUG: stp_get_input_color_model(v) |%d|\n", stp_get_input_color_model(v));
	fprintf(stderr, "DEBUG: stp_get_output_color_model(v) |%d|\n", stp_get_output_color_model(v));
	fprintf(stderr, "DEBUG: stp_get_brightness(v) |%.3f|\n", stp_get_brightness(v));
	fprintf(stderr, "DEBUG: stp_get_scaling(v) |%.3f|\n", stp_get_scaling(v));
	fprintf(stderr, "DEBUG: stp_get_gamma(v) |%.3f|\n", stp_get_gamma(v));
	fprintf(stderr, "DEBUG: stp_get_contrast(v) |%.3f|\n", stp_get_contrast(v));
	fprintf(stderr, "DEBUG: stp_get_cyan(v) |%.3f|\n", stp_get_cyan(v));
	fprintf(stderr, "DEBUG: stp_get_magenta(v) |%.3f|\n", stp_get_magenta(v));
	fprintf(stderr, "DEBUG: stp_get_yellow(v) |%.3f|\n", stp_get_yellow(v));
	fprintf(stderr, "DEBUG: stp_get_saturation(v) |%.3f|\n", stp_get_saturation(v));
	fprintf(stderr, "DEBUG: stp_get_density(v) |%.3f|\n", stp_get_density(v));
	fprintf(stderr, "DEBUG: stp_get_app_gamma(v) |%.3f|\n", stp_get_app_gamma(v));
      }
    stp_set_page_number(v, cups.page);

    (*stp_printer_get_printfuncs(printer)->media_size)
      (printer, v, &(cups.width), &(cups.height));
    (*stp_printer_get_printfuncs(printer)->imageable_area)
      (printer, v, &(cups.left), &(cups.right), &(cups.bottom), &(cups.top));
    fprintf(stderr, "DEBUG: GIMP-PRINT %d %d %d  %d %d %d\n",
	    cups.width, cups.left, cups.right, cups.height, cups.top, cups.bottom);
    cups.right = cups.width - cups.right;
    cups.width = cups.width - cups.left - cups.right;
    cups.width = cups.header.HWResolution[0] * cups.width / 72;
    cups.left = cups.header.HWResolution[0] * cups.left / 72;
    cups.right = cups.header.HWResolution[0] * cups.right / 72;

    cups.top = cups.height - cups.top;
    cups.height = cups.height - cups.top - cups.bottom;
    cups.height = cups.header.HWResolution[1] * cups.height / 72;
    cups.top = cups.header.HWResolution[1] * cups.top / 72;
    cups.bottom = cups.header.HWResolution[1] * cups.bottom / 72;
    fprintf(stderr, "DEBUG: GIMP-PRINT %d %d %d  %d %d %d\n",
	    cups.width, cups.left, cups.right, cups.height, cups.top, cups.bottom);

   /*
    * Print the page...
    */

    if (stp_printer_get_printfuncs(printer)->verify(printer, v))
    {
      signal(SIGTERM, cancel_job);
      if (cups.page == 0)
	stp_printer_get_printfuncs(printer)->start_job(printer, &theImage, v);
      stp_printer_get_printfuncs(printer)->print(printer, &theImage, v);
      fflush(stdout);
    }
    else
      fputs("ERROR: Invalid printer settings!\n", stderr);

   /*
    * Purge any remaining bitmap data...
    */

    if (cups.row < cups.header.cupsHeight)
    {
      if ((buffer = xmalloc(cups.header.cupsBytesPerLine)) == NULL)
        break;

      while (cups.row < cups.header.cupsHeight)
      {
        cupsRasterReadPixels(cups.ras, (unsigned char *)buffer,
	                     cups.header.cupsBytesPerLine);
	cups.row ++;
      }
    }
    cups.page ++;
  }

  if (cups.page > 0)
    stp_printer_get_printfuncs(printer)->end_job(printer, &theImage, v);
  stp_free_vars(v);
 /*
  * Close the raster stream...
  */

  cupsRasterClose(cups.ras);
  if (fd != 0)
    close(fd);

 /*
  * If no pages were printed, send an error message...
  */

  if (cups.page == 0)
    fputs("ERROR: No pages found!\n", stderr);
  else
    fputs("INFO: Ready to print.\n", stderr);

  return (cups.page == 0);
}


/*
 * 'cups_writefunc()' - Write data to a file...
 */

static void
cups_writefunc(void *file, const char *buf, size_t bytes)
{
  FILE *prn = (FILE *)file;
  fwrite(buf, 1, bytes, prn);
}


/*
 * 'cancel_job()' - Cancel the current job...
 */

void
cancel_job(int sig)			/* I - Signal */
{
  (void)sig;

  Image_status = STP_IMAGE_ABORT;
}


/*
 * 'Image_bpp()' - Return the bytes-per-pixel of an image.
 */

static int				/* O - Bytes per pixel */
Image_bpp(stp_image_t *image)		/* I - Image */
{
  cups_image_t	*cups;		/* CUPS image */


  if ((cups = (cups_image_t *)(image->rep)) == NULL)
    return (0);

 /*
  * For now, we only support RGB and grayscale input from the
  * raster filters.
  */

  switch (cups->header.cupsColorSpace)
  {
    default :
        return (1);
    case CUPS_CSPACE_RGB :
        return (3);
    case CUPS_CSPACE_CMYK :
        return (4);
  }
}


/*
 * 'Image_get_appname()' - Get the application we are running.
 */

static const char *				/* O - Application name */
Image_get_appname(stp_image_t *image)		/* I - Image */
{
  (void)image;

  return ("CUPS 1.1.x driver based on GIMP-print");
}


/*
 * 'Image_get_row()' - Get one row of the image.
 */

static void
throwaway_data(int amount, cups_image_t *cups)
{
  unsigned char trash[4096];	/* Throwaway */
  int block_count = amount / 4096;
  int leftover = amount % 4096;
  while (block_count > 0)
    {
      cupsRasterReadPixels(cups->ras, trash, 4096);
      block_count--;
    }
  if (leftover)
    cupsRasterReadPixels(cups->ras, trash, leftover);
}

stp_image_status_t
Image_get_row(stp_image_t   *image,	/* I - Image */
	      unsigned char *data,	/* O - Row */
	      int           row)	/* I - Row number (unused) */
{
  cups_image_t	*cups;			/* CUPS image */
  int		i;			/* Looping var */
  int 		bytes_per_line;
  int		margin;


  if ((cups = (cups_image_t *)(image->rep)) == NULL)
    return STP_IMAGE_ABORT;
  bytes_per_line = cups->width * cups->header.cupsBitsPerPixel / CHAR_BIT;
  margin = cups->header.cupsBytesPerLine - bytes_per_line;

  if (cups->row < cups->header.cupsHeight)
  {
    fprintf(stderr, "DEBUG: GIMP-PRINT reading %d %d\n",
	    bytes_per_line, cups->row);
    cupsRasterReadPixels(cups->ras, data, bytes_per_line);
    cups->row ++;
    fprintf(stderr, "DEBUG: GIMP-PRINT tossing right %d\n", margin);
    if (margin)
      throwaway_data(margin, cups);

   /*
    * Invert black data for monochrome output...
    */

    if (cups->header.cupsColorSpace == CUPS_CSPACE_K)
      for (i = bytes_per_line; i > 0; i --, data ++)
        *data = ((1 << CHAR_BIT) - 1) - *data;
  }
  else
    {
      if (cups->header.cupsColorSpace == CUPS_CSPACE_CMYK)
	memset(data, 0, bytes_per_line);
      else
	memset(data, ((1 << CHAR_BIT) - 1), bytes_per_line);
    }
  return Image_status;
}


/*
 * 'Image_height()' - Return the height of an image.
 */

static int				/* O - Height in pixels */
Image_height(stp_image_t *image)	/* I - Image */
{
  cups_image_t	*cups;		/* CUPS image */


  if ((cups = (cups_image_t *)(image->rep)) == NULL)
    return (0);

  fprintf(stderr, "DEBUG: GIMP-PRINT: Image_height %d\n", cups->height);
  return (cups->height);
}


/*
 * 'Image_init()' - Initialize an image.
 */

static void
Image_init(stp_image_t *image)		/* I - Image */
{
  (void)image;
}


/*
 * 'Image_note_progress()' - Notify the user of our progress.
 */

void
Image_note_progress(stp_image_t *image,	/* I - Image */
		    double current,	/* I - Current progress */
		    double total)	/* I - Maximum progress */
{
  cups_image_t	*cups;		/* CUPS image */


  if ((cups = (cups_image_t *)(image->rep)) == NULL)
    return;

  fprintf(stderr, "INFO: Printing page %d, %.0f%%\n",
          cups->page, 100.0 * current / total);
}


/*
 * 'Image_progress_conclude()' - Close the progress display.
 */

static void
Image_progress_conclude(stp_image_t *image)	/* I - Image */
{
  cups_image_t	*cups;		/* CUPS image */


  if ((cups = (cups_image_t *)(image->rep)) == NULL)
    return;

  fprintf(stderr, "INFO: Finished page %d...\n", cups->page);
}


/*
 * 'Image_progress_init()' - Initialize progress display.
 */

static void
Image_progress_init(stp_image_t *image)/* I - Image */
{
  cups_image_t	*cups;		/* CUPS image */


  if ((cups = (cups_image_t *)(image->rep)) == NULL)
    return;

  fprintf(stderr, "INFO: Starting page %d...\n", cups->page);
}


/*
 * 'Image_rotate_180()' - Rotate the image 180 degrees (unsupported).
 */

static void
Image_rotate_180(stp_image_t *image)	/* I - Image */
{
  (void)image;
}


/*
 * 'Image_rotate_ccw()' - Rotate the image counter-clockwise (unsupported).
 */

static void
Image_rotate_ccw(stp_image_t *image)	/* I - Image */
{
  (void)image;
}


/*
 * 'Image_rotate_cw()' - Rotate the image clockwise (unsupported).
 */

static void
Image_rotate_cw(stp_image_t *image)	/* I - Image */
{
  (void)image;
}


/*
 * 'Image_width()' - Return the width of an image.
 */

static int				/* O - Width in pixels */
Image_width(stp_image_t *image)	/* I - Image */
{
  cups_image_t	*cups;		/* CUPS image */


  if ((cups = (cups_image_t *)(image->rep)) == NULL)
    return (0);

  fprintf(stderr, "DEBUG: GIMP-PRINT: Image_width %d\n", cups->width);
  return (cups->width);
}


/*
 * End of "$Id: rastertoprinter.c,v 1.1.1.1 2003/01/27 19:05:32 jlovell Exp $".
 */

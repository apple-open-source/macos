/*

  stp driver for ghostscript

  -gs frontend derived from gdevbmp and gdevcdj


  written in January 2000 by
  Henryk Richter <buggs@comlab.uni-rostock.de>
  for ghostscript 5.x/6.x


  This program is free software; you can redistribute it and/or modify it
  under the terms of the GNU General Public License as published by the Free
  Software Foundation; either version 2 of the License, or (at your option)
  any later version.

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
  or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
  for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

*/
/*$Id: gdevstp.c,v 1.1.1.1 2003/01/27 19:05:32 jlovell Exp $ */
/* stp output driver */
#include "gdevprn.h"
#include "gdevpccm.h"
#include "gsparam.h"
#include <stdlib.h>

#ifdef DISABLE_NLS
#include "gdevstp-print.h"
#else
#include <gimp-print/gimp-print.h>
#endif

/* internal debugging output ? */

private int stp_debug = 0;

#define STP_DEBUG(x) do { if (stp_debug || getenv("STP_DEBUG")) x; } while (0)

/* ------ The device descriptors ------ */

#define X_DPI 360
#define Y_DPI 360

private dev_proc_map_rgb_color(stp_map_16m_rgb_color);
private dev_proc_map_color_rgb(stp_map_16m_color_rgb);
private dev_proc_print_page(stp_print_page);
private dev_proc_get_params(stp_get_params);
private dev_proc_put_params(stp_put_params);
private dev_proc_open_device(stp_open);

/* 24-bit color. ghostscript driver */
private const gx_device_procs stpm_procs =
prn_color_params_procs(
		       stp_open, /*gdev_prn_open,*/ /* open file, delegated */
		       gdev_prn_output_page,     /* output page, delegated */
		       gdev_prn_close,           /* close file, delegated */
		       stp_map_16m_rgb_color, /* map color (own) */
		       stp_map_16m_color_rgb, /* map color (own) */
		       stp_get_params,        /* get params (own) */
		       stp_put_params         /* put params (own) */
		       );

gx_device_printer gs_stp_device =
prn_device(stpm_procs, "stp",
	   DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS,
	   X_DPI, Y_DPI,
	   0, 0, 0, 0,		/* margins */
	   24, stp_print_page);

/* private data structure */
typedef struct
{
  int topoffset;   /* top offset in pixels */
  int bottom;
  stp_vars_t v;
} privdata_t;

/* global variables, RO for subfunctions */
private privdata_t stp_data =
{ 0, 0, NULL };

typedef struct
{
  gx_device_printer *dev;
  privdata_t *data;
  uint raster;
} stp_priv_image_t;

static const char *Image_get_appname(stp_image_t *image);
static void Image_progress_conclude(stp_image_t *image);
static void Image_note_progress(stp_image_t *image,
				double current, double total);
static void Image_progress_init(stp_image_t *image);
static stp_image_status_t Image_get_row(stp_image_t *image,
					unsigned char *data, int row);
static int Image_height(stp_image_t *image);
static int Image_width(stp_image_t *image);
static int Image_bpp(stp_image_t *image);
static void Image_rotate_180(stp_image_t *image);
static void Image_rotate_cw(stp_image_t *image);
static void Image_rotate_ccw(stp_image_t *image);
static void Image_init(stp_image_t *image);

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

/* ------ Private definitions ------ */

/***********************************************************************
* ghostscript driver function calls                                    *
***********************************************************************/

private void
stp_dbg(const char *msg, const privdata_t *stp_data)
{
  fprintf(gs_stderr,"%s Settings: c: %f  m: %f  y: %f\n",
	  msg, stp_get_cyan(stp_data->v), stp_get_magenta(stp_data->v),
	  stp_get_yellow(stp_data->v));
  fprintf(gs_stderr, "Ink type %s\n", stp_get_ink_type(stp_data->v));

  fprintf(gs_stderr,"Settings: bright: %f  contrast: %f\n",
	  stp_get_brightness(stp_data->v), stp_get_contrast(stp_data->v));

  fprintf(gs_stderr,"Settings: Gamma: %f  Saturation: %f  Density: %f\n",
	  stp_get_gamma(stp_data->v), stp_get_saturation(stp_data->v),
	  stp_get_density(stp_data->v));
  fprintf(gs_stderr, "Settings: width %d, height %d\n",
	  stp_get_page_width(stp_data->v), stp_get_page_height(stp_data->v));
  fprintf(gs_stderr, "Settings: output type %d  image type %d\n",
	  stp_get_output_type(stp_data->v), stp_get_image_type(stp_data->v));
  fprintf(gs_stderr, "Settings: Quality %s\n",
	  stp_get_resolution(stp_data->v));
  fprintf(gs_stderr, "Settings: Dither %s\n",
	  stp_get_dither_algorithm(stp_data->v));
  fprintf(gs_stderr, "Settings: InputSlot %s\n",
	  stp_get_media_source(stp_data->v));
  fprintf(gs_stderr, "Settings: MediaType %s\n",
	  stp_get_media_type(stp_data->v));
  fprintf(gs_stderr, "Settings: MediaSize %s\n",
	  stp_get_media_size(stp_data->v));
  fprintf(gs_stderr, "Settings: Model %s\n", stp_get_driver(stp_data->v));
  fprintf(gs_stderr, "Settings: InkType %s\n", stp_get_ink_type(stp_data->v));
  fprintf(gs_stderr, "Settings: OutputTo %s\n", stp_get_output_to(stp_data->v));
}

private void
stp_print_dbg(const char *msg, gx_device_printer *pdev,
	      const privdata_t *stp_data)
{
  STP_DEBUG(if (pdev)
	    fprintf(gs_stderr,"%s Image: %d x %d pixels, %f x %f dpi\n",
		    msg, pdev->width, pdev->height, pdev->x_pixels_per_inch,
		    pdev->y_pixels_per_inch));
  STP_DEBUG(stp_dbg(msg, stp_data));
}

private void
stp_print_debug(const char *msg, gx_device *pdev,
		const privdata_t *stp_data)
{
  STP_DEBUG(if (pdev)
	    fprintf(gs_stderr,"%s Image: %d x %d pixels, %f x %f dpi\n",
		    msg, pdev->width, pdev->height, pdev->x_pixels_per_inch,
		    pdev->y_pixels_per_inch));
  STP_DEBUG(stp_dbg(msg, stp_data));
}

private void
stp_init_vars(void)
{
  STP_DEBUG(fprintf(gs_stderr, "Calling "));
  if (! stp_data.v)
    {
      STP_DEBUG(fprintf(gs_stderr, "and initializing "));
      stp_init();
      stp_data.v = stp_allocate_vars();
      stp_set_driver(stp_data.v, "");
/*      stp_set_media_size(stp_data.v, "Letter"); */
    }
  STP_DEBUG(fprintf(gs_stderr, "stp_init_vars\n"));
}


private void
stp_writefunc(void *file, const char *buf, size_t bytes)
{
  FILE *prn = (FILE *)file;
  fwrite(buf, 1, bytes, prn);
}

private int
stp_print_page(gx_device_printer * pdev, FILE * file)
{
  stp_priv_image_t gsImage;
  private int printvars_merged = 0;
  int code;			/* return code */
  stp_printer_t printer = NULL;
  uint stp_raster;
  byte *stp_row;
  stp_papersize_t p;
  theImage.rep = &gsImage;

  stp_init_vars();
  stp_print_dbg("stp_print_page", pdev, &stp_data);
  code = 0;
  stp_raster = gdev_prn_raster(pdev);
  printer = stp_get_printer_by_driver(stp_get_driver(stp_data.v));
  if (printer == NULL)
    {
      fprintf(gs_stderr, "Printer %s is not a known printer model\n",
	      stp_get_driver(stp_data.v));
      return_error(gs_error_rangecheck);
    }

  if (!printvars_merged)
    {
      stp_merge_printvars(stp_data.v, stp_printer_get_printvars(printer));
      printvars_merged = 1;
    }
  stp_row = gs_alloc_bytes(pdev->memory, stp_raster, "stp file buffer");

  if (stp_row == 0)		/* can't allocate row buffer */
    return_error(gs_error_VMerror);

  if (strlen(stp_get_resolution(stp_data.v)) == 0)
    stp_set_resolution(stp_data.v,
		       ((*stp_printer_get_printfuncs(printer)->default_parameters)
			(printer, NULL, "Resolution")));
  if (strlen(stp_get_dither_algorithm(stp_data.v)) == 0)
    stp_set_dither_algorithm(stp_data.v, stp_default_dither_algorithm());

  stp_set_scaling(stp_data.v, -pdev->x_pixels_per_inch); /* resolution of image */

  /* compute lookup table: lut_t*,float dest_gamma,float app_gamma,stp_vars_t* */
  stp_set_app_gamma(stp_data.v, 1.7);

  stp_data.topoffset = 0;
  stp_set_cmap(stp_data.v, NULL);

  stp_set_page_width(stp_data.v, pdev->MediaSize[0]);
  stp_set_page_height(stp_data.v, pdev->MediaSize[1]);
  if ((p = stp_get_papersize_by_size(stp_get_page_height(stp_data.v),
				     stp_get_page_width(stp_data.v))) != NULL)
    stp_set_media_size(stp_data.v, stp_papersize_get_name(p));
  stp_print_dbg("stp_print_page", pdev, &stp_data);

  gsImage.dev = pdev;
  gsImage.data = &stp_data;
  gsImage.raster = stp_raster;
  stp_set_outfunc(stp_data.v, stp_writefunc);
  stp_set_errfunc(stp_data.v, stp_writefunc);
  stp_set_outdata(stp_data.v, file);
  stp_set_errdata(stp_data.v, gs_stderr);
  if (stp_printer_get_printfuncs(printer)->verify(printer, stp_data.v))
    (stp_printer_get_printfuncs(printer)->print)
      (printer, &theImage, stp_data.v);
  else
    code = 1;

  gs_free_object(pdev->memory, stp_row, "stp row buffer");
  stp_row = NULL;

  if (code)
    return_error(gs_error_rangecheck);
  else
    return 0;
}

/* 24-bit color mappers (taken from gdevmem2.c). */

/* Map a r-g-b color to a color index. */
private gx_color_index
stp_map_16m_rgb_color(gx_device * dev, gx_color_value r, gx_color_value g,
		  gx_color_value b)
{
  return gx_color_value_to_byte(b) +
    ((uint) gx_color_value_to_byte(g) << 8) +
    ((ulong) gx_color_value_to_byte(r) << 16);
}

/* Map a color index to a r-g-b color. */
private int
stp_map_16m_color_rgb(gx_device * dev, gx_color_index color,
		  gx_color_value prgb[3])
{
  prgb[2] = gx_color_value_from_byte(color & 0xff);
  prgb[1] = gx_color_value_from_byte((color >> 8) & 0xff);
  prgb[0] = gx_color_value_from_byte(color >> 16);
  return 0;
}

private int
stp_write_float(gs_param_list *plist, const char *name, float value)
{
  return param_write_float(plist, name, &value);
}

private int
stp_write_int(gs_param_list *plist, const char *name, int value)
{
  return param_write_int(plist, name, &value);
}

private char *
c_strdup(const char *s)
{
  char *ret;
  if (!s)
    {
      STP_DEBUG(fprintf(gs_stderr, "c_strdup null ptr\n"));
      ret = malloc(1);
      ret[0] = 0;
      return ret;
    }
  else
    {
      STP_DEBUG(fprintf(gs_stderr, "c_strdup |%s|\n", s));
      ret = malloc(strlen(s) + 1);
      strcpy(ret, s);
      return ret;
    }
}

/*
 * Get parameters.  In addition to the standard and printer
 * parameters, we supply a lot of options to play around with
 * for maximum quality out of the photo printer
*/
/* Yeah, I could have used a list for the options but... */
private int
stp_get_params(gx_device *pdev, gs_param_list *plist)
{
  int code;
  gs_param_string pinktype;
  gs_param_string pmodel;
  gs_param_string pmediatype;
  gs_param_string pInputSlot;
  gs_param_string palgorithm;
  gs_param_string pquality;
  char *pmediatypestr;
  char *pInputSlotstr;
  char *pinktypestr;
  char *pmodelstr;
  char *palgorithmstr;
  char *pqualitystr;
  stp_init_vars();

  stp_print_debug("stp_get_params(0)", pdev, &stp_data);
  code = gdev_prn_get_params(pdev, plist);
  stp_print_debug("stp_get_params(1)", pdev, &stp_data);
  pmodelstr = c_strdup(stp_get_driver(stp_data.v));
  pInputSlotstr = c_strdup(stp_get_media_source(stp_data.v));
  pmediatypestr = c_strdup(stp_get_media_type(stp_data.v));
  pinktypestr = c_strdup(stp_get_ink_type(stp_data.v));
  palgorithmstr = c_strdup(stp_get_dither_algorithm(stp_data.v));
  pqualitystr = c_strdup(stp_get_resolution(stp_data.v));

  param_string_from_string(pmodel, pmodelstr);
  param_string_from_string(pInputSlot, pInputSlotstr);
  param_string_from_string(pmediatype, pmediatypestr);
  param_string_from_string(pinktype, pinktypestr);
  param_string_from_string(palgorithm, palgorithmstr);
  param_string_from_string(pquality, pqualitystr);

  if (code < 0 ||
      (code = stp_write_float(plist, "Cyan", stp_get_cyan(stp_data.v))) < 0 ||
      (code = stp_write_float(plist, "Magenta", stp_get_magenta(stp_data.v))) < 0 ||
      (code = stp_write_float(plist, "Yellow", stp_get_yellow(stp_data.v))) < 0 ||
      (code = stp_write_float(plist, "Brightness", stp_get_brightness(stp_data.v))) < 0 ||
      (code = stp_write_float(plist, "Contrast", stp_get_contrast(stp_data.v))) < 0 ||
      (code = stp_write_int(plist, "Color", stp_get_output_type(stp_data.v))) < 0 ||
      (code = stp_write_int(plist, "ImageType", stp_get_image_type(stp_data.v))) < 0 ||
      (code = stp_write_float(plist, "Gamma", stp_get_gamma(stp_data.v))) < 0 ||
      (code = stp_write_float(plist, "Saturation", stp_get_saturation(stp_data.v))) < 0 ||
      (code = stp_write_float(plist, "Density", stp_get_density(stp_data.v))) < 0 ||
      (code = param_write_string(plist, "Model", &pmodel)) < 0 ||
      (code = param_write_string(plist, "Dither", &palgorithm)) < 0 ||
      (code = param_write_string(plist, "Quality", &pquality)) < 0 ||
      (code = param_write_string(plist, "InkType", &pinktype) < 0) ||
      (code = param_write_string(plist, "MediaType", &pmediatype)) < 0 ||
      (code = param_write_string(plist, "stpMediaType", &pmediatype)) < 0 ||
      (code = param_write_string(plist, "InputSlot", &pInputSlot)) < 0
      )
    {
      free(pmodelstr);
      free(pInputSlotstr);
      free(pmediatypestr);
      free(pinktypestr);
      free(palgorithmstr);
      free(pqualitystr);
      STP_DEBUG(fprintf(stderr, "stp_get_params returns %d\n", code));
      return code;
    }

  return 0;
}

/* Put parameters. */
/* Yeah, I could have used a list for the options but... */

#define STP_PUT_PARAM(plist, gtype, v, name, param, ecode)		    \
do {									    \
  int code;								    \
  gtype value;								    \
									    \
  code = param_read_##gtype(plist, name, &value);			    \
  switch (code)								    \
    {									    \
    case 0:								    \
      if (value < stp_get_##param(lower) || value > stp_get_##param(upper)) \
	{								    \
	  param_signal_error(plist, name, gs_error_rangecheck);		    \
	  ecode = -100;							    \
	}								    \
      else								    \
	stp_set_##param(v, value);					    \
      break;								    \
    case 1:								    \
      break;								    \
    default:								    \
      ecode = code;							    \
      break;								    \
    }									    \
} while (0)

private int
stp_put_params(gx_device *pdev, gs_param_list *plist)
{
  const stp_vars_t lower = stp_minimum_settings();
  const stp_vars_t upper = stp_maximum_settings();
  gs_param_string pmediatype;
  gs_param_string pInputSlot;
  gs_param_string pinktype;
  gs_param_string pmodel;
  gs_param_string palgorithm;
  gs_param_string pquality;
  char *pmediatypestr;
  char *pInputSlotstr;
  char *pinktypestr;
  char *pmodelstr;
  char *palgorithmstr;
  char *pqualitystr;
  int code   = 0;
  stp_printer_t printer;
  stp_init_vars();

  stp_print_debug("stp_put_params(0)", pdev, &stp_data);

  pmodelstr = c_strdup(stp_get_driver(stp_data.v));
  pInputSlotstr = c_strdup(stp_get_media_source(stp_data.v));
  pmediatypestr = c_strdup(stp_get_media_type(stp_data.v));
  pinktypestr = c_strdup(stp_get_ink_type(stp_data.v));
  palgorithmstr = c_strdup(stp_get_dither_algorithm(stp_data.v));
  pqualitystr = c_strdup(stp_get_resolution(stp_data.v));

  param_string_from_string(pmodel, pmodelstr);
  param_string_from_string(pInputSlot, pInputSlotstr);
  param_string_from_string(pmediatype, pmediatypestr);
  param_string_from_string(pinktype, pinktypestr);
  param_string_from_string(palgorithm, palgorithmstr);
  param_string_from_string(pquality, pqualitystr);

  STP_PUT_PARAM(plist, float, stp_data.v, "Cyan", cyan, code);
  STP_PUT_PARAM(plist, float, stp_data.v, "Magenta", magenta, code);
  STP_PUT_PARAM(plist, float, stp_data.v, "Yellow", yellow, code);
  STP_PUT_PARAM(plist, float, stp_data.v, "Brightness", brightness, code);
  STP_PUT_PARAM(plist, float, stp_data.v, "Contrast", contrast, code);
  STP_PUT_PARAM(plist, int, stp_data.v, "Color", output_type, code);
  STP_PUT_PARAM(plist, int, stp_data.v, "ImageType", image_type, code);
  STP_PUT_PARAM(plist, float, stp_data.v, "Gamma", gamma, code);
  STP_PUT_PARAM(plist, float, stp_data.v, "Saturation", saturation ,code);
  STP_PUT_PARAM(plist, float, stp_data.v, "Density", density, code);
  param_read_string(plist, "Quality", &pquality);
  param_read_string(plist, "Dither", &palgorithm);
  param_read_string(plist, "InputSlot", &pInputSlot);
  param_read_string(plist, "stpMediaType", &pmediatype);
  if (!pmediatype.data || pmediatype.size == 0)
    param_read_string(plist, "MediaType", &pmediatype);
  param_read_string(plist, "Model", &pmodel);
  param_read_string(plist, "InkType", &pinktype);

  if (stp_get_output_type(stp_data.v) == OUTPUT_RAW_CMYK)
    {
      param_signal_error(plist, "Color", gs_error_rangecheck);
      code = -100;
    }

  if ( code < 0 )
    {
      free(pmodelstr);
      free(pInputSlotstr);
      free(pmediatypestr);
      free(pinktypestr);
      free(palgorithmstr);
      free(pqualitystr);

      return code;
    }

  STP_DEBUG(fprintf(gs_stderr, "pmodel.size %d pmodel.data %s\n",
		    pmodel.size, pmodel.data));
  STP_DEBUG(fprintf(gs_stderr, "pinktype.size %d pinktype.data %s\n",
		    pinktype.size, pinktype.data));
  STP_DEBUG(fprintf(gs_stderr, "pquality.size %d pquality.data %s\n",
		    pquality.size, pquality.data));
  stp_set_driver_n(stp_data.v, pmodel.data, pmodel.size);
  printer = stp_get_printer_by_driver(stp_get_driver(stp_data.v));
  if (printer)
    stp_set_printer_defaults(stp_data.v, printer, NULL);

  if (pmediatype.data && pmediatype.size != 0)
    stp_set_media_type_n(stp_data.v, pmediatype.data, pmediatype.size);
  if (pInputSlot.data && pInputSlot.size != 0)
    stp_set_media_source_n(stp_data.v, pInputSlot.data, pInputSlot.size);
  if (pinktype.data && pinktype.size != 0)
    stp_set_ink_type_n(stp_data.v, pinktype.data, pinktype.size);
  if (palgorithm.data && palgorithm.size != 0)
    stp_set_dither_algorithm_n(stp_data.v, palgorithm.data, palgorithm.size);
  if (pquality.data && pquality.size != 0)
    stp_set_resolution_n(stp_data.v, pquality.data, pquality.size);
  stp_print_debug("stp_put_params(1)", pdev, &stp_data);

  code = gdev_prn_put_params(pdev, plist);

  free(pmodelstr);
  free(pInputSlotstr);
  free(pmediatypestr);
  free(pinktypestr);
  free(palgorithmstr);
  free(pqualitystr);

  return code;
}

private int
stp_open(gx_device *pdev)
{
  /* Change the margins if necessary. */
  float st[4];
  int left,right,bottom,top,width,height;
  stp_printer_t printer;
  stp_init_vars();
  stp_print_debug("stp_open", pdev, &stp_data);
  printer = stp_get_printer_by_driver(stp_get_driver(stp_data.v));
  if (!printer)
    {
      if (strlen(stp_get_driver(stp_data.v)) == 0)
	fprintf(gs_stderr, "Printer must be specified with -sModel\n");
      else
	fprintf(gs_stderr, "Printer %s is not a known model\n",
		stp_get_driver(stp_data.v));
      return_error(gs_error_undefined);
    }

  stp_set_page_width(stp_data.v, pdev->MediaSize[0]);
  stp_set_page_height(stp_data.v, pdev->MediaSize[1]);

  (*stp_printer_get_printfuncs(printer)->media_size)
    (printer, stp_data.v, &width, &height);

  (*stp_printer_get_printfuncs(printer)->imageable_area)
    (printer, stp_data.v, &left, &right, &bottom, &top);

  st[1] = (float)bottom / 72;        /* bottom margin */
  st[3] = (float)(height-top) / 72;  /* top margin    */
  st[0] = (float)left / 72;          /* left margin   */
  st[2] = (float)(width-right) / 72; /* right margin  */

  stp_set_top(stp_data.v, 0);
  stp_set_left(stp_data.v, 0);
  stp_set_orientation(stp_data.v, ORIENT_PORTRAIT);
  stp_data.bottom = bottom + height-top;

  stp_print_debug("stp_open", pdev, &stp_data);
  STP_DEBUG(fprintf(gs_stderr, "margins:  l %f  b %f  r %f  t %f\n",
		    st[0], st[1], st[2], st[3]));

  gx_device_set_margins(pdev, st, true);
  return gdev_prn_open(pdev);
}


/***********************************************************************
* driver function callback routines                                    *
***********************************************************************/

/* get one row of the image */
private stp_image_status_t
Image_get_row(stp_image_t *image, unsigned char *data, int row)
{
  stp_priv_image_t *im = (stp_priv_image_t *) (image->rep);
  memset(data, 0, im->dev->width * 3);
  if (im->dev->x_pixels_per_inch == im->dev->y_pixels_per_inch)
    {
      gdev_prn_copy_scan_lines(im->dev, im->data->topoffset+row,
			       data, im->raster);
    }
  else if (im->dev->x_pixels_per_inch > im->dev->y_pixels_per_inch)
    {
      /*
       * If xres > yres, duplicate rows
       */
      int ratio = (im->dev->x_pixels_per_inch / im->dev->y_pixels_per_inch);
      gdev_prn_copy_scan_lines(im->dev, (im->data->topoffset + row) / ratio,
			       data, im->raster);
    }
  else
    {
      /*
       * If xres < yres, skip rows
       */
      int ratio = (im->dev->y_pixels_per_inch / im->dev->x_pixels_per_inch);
      gdev_prn_copy_scan_lines(im->dev, (im->data->topoffset + row) * ratio,
			       data, im->raster);
    }
  return STP_IMAGE_OK;
}

/* return bpp of picture (24 here) */
private int
Image_bpp(stp_image_t *image)
{
  return 3;
}

/* return width of picture */
private int
Image_width(stp_image_t *image)
{
  stp_priv_image_t *im = (stp_priv_image_t *) (image->rep);
  return im->dev->width;
}

/*
  return height of picture and
  subtract margins from image size so that the
  driver only reads the correct number of lines from the
  input

*/
private int
Image_height(stp_image_t *image)
{
  stp_priv_image_t *im = (stp_priv_image_t *) (image->rep);
  float tmp,tmp2;

  /* top margin + bottom margin */
  tmp = stp_get_top(im->data->v) + im->data->bottom;

  /* calculate height in 1/72 inches */
  tmp2 = (float)(im->dev->height) / (float)(im->dev->y_pixels_per_inch) * 72.;

  tmp2 -= tmp;			/* subtract margins from sizes */

  /* calculate new image height */
  tmp2 *= (float)(im->dev->x_pixels_per_inch) / 72.;

  STP_DEBUG(fprintf(gs_stderr,"corrected page height %f\n",tmp2));

  return (int)tmp2;
}

private void
Image_rotate_ccw(stp_image_t *image)
{
 /* dummy function, Landscape printing unsupported atm */
}

private void
Image_rotate_cw(stp_image_t *image)
{
 /* dummy function, Seascape printing unsupported atm */
}

private void
Image_rotate_180(stp_image_t *image)
{
 /* dummy function,  upside down printing unsupported atm */
}

private void
Image_init(stp_image_t *image)
{
 /* dummy function */
}

private void
Image_progress_init(stp_image_t *image)
{
 /* dummy function */
}

/* progress display */
private void
Image_note_progress(stp_image_t *image, double current, double total)
{
  STP_DEBUG(fprintf(gs_stderr, "."));
}

private void
Image_progress_conclude(stp_image_t *image)
{
  STP_DEBUG(fprintf(gs_stderr, "\n"));
}

private const char *
Image_get_appname(stp_image_t *image)
{
  return "GhostScript/stp";
}

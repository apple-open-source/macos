/*
 * "$Id: print-escp2.c,v 1.1.1.1 2003/01/27 19:05:32 jlovell Exp $"
 *
 *   Print plug-in EPSON ESC/P2 driver for the GIMP.
 *
 *   Copyright 1997-2000 Michael Sweet (mike@easysw.com) and
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
#include <string.h>
#include <stdio.h>
#include "print-escp2.h"

#ifdef __GNUC__
#define inline __inline__
#endif

#ifdef TEST_UNCOMPRESSED
#define COMPRESSION (0)
#define FILLFUNC stp_fill_uncompressed
#define COMPUTEFUNC stp_compute_uncompressed_linewidth
#define PACKFUNC stp_pack_uncompressed
#else
#define COMPRESSION (1)
#define FILLFUNC stp_fill_tiff
#define COMPUTEFUNC stp_compute_tiff_linewidth
#define PACKFUNC stp_pack_tiff
#endif

#define OP_JOB_START 1
#define OP_JOB_PRINT 2
#define OP_JOB_END   4

static void flush_pass(stp_softweave_t *sw, int passno, int model, int width,
		       int hoffset, int ydpi, int xdpi, int physical_xdpi,
		       int vertical_subpass);

static const int dotidmap[] =
{ 0, 1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 10, 11, 12, 12 };

static int
resid2dotid(int resid)
{
  if (resid < 0 || resid >= RES_N)
    return -1;
  return dotidmap[resid];
}

static const escp2_printer_attr_t escp2_printer_attrs[] =
{
  { "command_mode",		0, 4 },
  { "horizontal_zero_margin",	4, 1 },
  { "rollfeed",			5, 1 },
  { "variable_mode",		6, 1 },
  { "graymode",		 	7, 1 },
  { "vacuum",			8, 1 },
  { "fast_360",			9, 1 },
};

#define INCH(x)		(72 * x)

static const res_t *escp2_find_resolution(int model, const stp_vars_t v,
					  const char *resolution);

typedef struct
{
  int undersample;
  int denominator;
  int initial_vertical_offset;
  int min_nozzles;
  int printed_something;
  int last_color;
  const physical_subchannel_t **channels;
} escp2_privdata_t;

typedef struct escp2_init
{
  int model;
  int output_type;
  int ydpi;
  int xdpi;
  int physical_xdpi;
  int use_softweave;
  int use_microweave;
  int page_true_height;
  int page_width;
  int page_top;
  int page_bottom;
  int nozzles;
  int nozzle_separation;
  int horizontal_passes;
  int vertical_passes;
  int vertical_oversample;
  int bits;
  int unidirectional;
  int resid;
  int initial_vertical_offset;
  int total_channels;
  int use_black_parameters;
  int use_fast_360;
  int print_op;
  const char *paper_type;
  const char *media_source;
  const escp2_inkname_t *inkname;
  const input_slot_t *input_slot;
  stp_vars_t v;
} escp2_init_t;


static int
escp2_has_cap(int model, escp2_model_option_t feature,
	      model_featureset_t class, const stp_vars_t v)
{
  if (feature < 0 || feature >= MODEL_LIMIT)
    return -1;
  else
    {
      model_featureset_t featureset =
	(((1ul << escp2_printer_attrs[feature].bits) - 1ul) <<
	 escp2_printer_attrs[feature].shift);
      return ((stp_escp2_model_capabilities[model].flags & featureset)==class);
    }
}

#define DEF_SIMPLE_ACCESSOR(f, t)			\
static t						\
escp2_##f(int model, const stp_vars_t v)		\
{							\
  return (stp_escp2_model_capabilities[model].f);	\
}

#define DEF_MICROWEAVE_ACCESSOR(f, t)					     \
static t								     \
escp2_##f(int model, const stp_vars_t v)				     \
{									     \
  const res_t *res = escp2_find_resolution(model, v, stp_get_resolution(v)); \
  if (res && !(res->softweave))						     \
    return (stp_escp2_model_capabilities[model].m_##f);			     \
  else									     \
    return (stp_escp2_model_capabilities[model].f);			     \
}

DEF_SIMPLE_ACCESSOR(max_hres, int)
DEF_SIMPLE_ACCESSOR(max_vres, int)
DEF_SIMPLE_ACCESSOR(min_hres, int)
DEF_SIMPLE_ACCESSOR(min_vres, int)
DEF_SIMPLE_ACCESSOR(nozzles, unsigned)
DEF_SIMPLE_ACCESSOR(black_nozzles, unsigned)
DEF_SIMPLE_ACCESSOR(fast_nozzles, unsigned)
DEF_SIMPLE_ACCESSOR(min_nozzles, unsigned)
DEF_SIMPLE_ACCESSOR(min_black_nozzles, unsigned)
DEF_SIMPLE_ACCESSOR(min_fast_nozzles, unsigned)
DEF_SIMPLE_ACCESSOR(nozzle_separation, unsigned)
DEF_SIMPLE_ACCESSOR(black_nozzle_separation, unsigned)
DEF_SIMPLE_ACCESSOR(fast_nozzle_separation, unsigned)
DEF_SIMPLE_ACCESSOR(separation_rows, unsigned)
DEF_SIMPLE_ACCESSOR(max_paper_width, unsigned)
DEF_SIMPLE_ACCESSOR(max_paper_height, unsigned)
DEF_SIMPLE_ACCESSOR(min_paper_width, unsigned)
DEF_SIMPLE_ACCESSOR(min_paper_height, unsigned)
DEF_SIMPLE_ACCESSOR(extra_feed, unsigned)
DEF_SIMPLE_ACCESSOR(pseudo_separation_rows, int)
DEF_SIMPLE_ACCESSOR(base_separation, int)
DEF_SIMPLE_ACCESSOR(base_resolution, int)
DEF_SIMPLE_ACCESSOR(enhanced_resolution, int)
DEF_SIMPLE_ACCESSOR(resolution_scale, int)
DEF_SIMPLE_ACCESSOR(initial_vertical_offset, int)
DEF_SIMPLE_ACCESSOR(black_initial_vertical_offset, int)
DEF_SIMPLE_ACCESSOR(max_black_resolution, int)
DEF_SIMPLE_ACCESSOR(zero_margin_offset, int)
DEF_SIMPLE_ACCESSOR(extra_720dpi_separation, int)
DEF_SIMPLE_ACCESSOR(paperlist, const paperlist_t *)
DEF_SIMPLE_ACCESSOR(reslist, const res_t *)
DEF_SIMPLE_ACCESSOR(inklist, const inklist_t *)
DEF_SIMPLE_ACCESSOR(input_slots, const input_slot_list_t *)
DEF_SIMPLE_ACCESSOR(preinit_sequence, const init_sequence_t *)
DEF_SIMPLE_ACCESSOR(postinit_remote_sequence, const init_sequence_t *)

DEF_MICROWEAVE_ACCESSOR(left_margin, unsigned)
DEF_MICROWEAVE_ACCESSOR(right_margin, unsigned)
DEF_MICROWEAVE_ACCESSOR(top_margin, unsigned)
DEF_MICROWEAVE_ACCESSOR(bottom_margin, unsigned)
DEF_MICROWEAVE_ACCESSOR(roll_left_margin, unsigned)
DEF_MICROWEAVE_ACCESSOR(roll_right_margin, unsigned)
DEF_MICROWEAVE_ACCESSOR(roll_top_margin, unsigned)
DEF_MICROWEAVE_ACCESSOR(roll_bottom_margin, unsigned)

static int
reslist_count(const res_t *rt)
{
  int i = 0;
  while (rt->hres)
    {
      i++;
      rt++;
    }
  return i;
}

static int
escp2_ink_type(int model, int resid, const stp_vars_t v)
{
  int dotid = resid2dotid(resid);
  return stp_escp2_model_capabilities[model].dot_sizes[dotid];
}

static double
escp2_density(int model, int resid, const stp_vars_t v)
{
  int dotid = resid2dotid(resid);
  return stp_escp2_model_capabilities[model].densities[dotid];
}

static double
escp2_bits(int model, int resid, const stp_vars_t v)
{
  int dotid = resid2dotid(resid);
  return stp_escp2_model_capabilities[model].bits[dotid];
}

static double
escp2_base_res(int model, int resid, const stp_vars_t v)
{
  int dotid = resid2dotid(resid);
  return stp_escp2_model_capabilities[model].base_resolutions[dotid];
}

static const escp2_variable_inkset_t *
escp2_inks(int model, int resid, int inkset, const stp_vars_t v)
{
  const escp2_variable_inklist_t *inks =
    stp_escp2_model_capabilities[model].inks;
  resid /= 2;
  return (*inks)[inkset][resid];
}

static const paper_t *
get_media_type(int model, const char *name, const stp_vars_t v)
{
  int i;
  const paperlist_t *p = escp2_paperlist(model, v);
  int paper_type_count = p->paper_count;
  for (i = 0; i < paper_type_count; i++)
    {
      if (!strcmp(name, p->papers[i].name))
	return &(p->papers[i]);
    }
  return NULL;
}

static int
escp2_has_advanced_command_set(int model, const stp_vars_t v)
{
  return (escp2_has_cap(model, MODEL_COMMAND, MODEL_COMMAND_PRO,v) ||
	  escp2_has_cap(model, MODEL_COMMAND, MODEL_COMMAND_1999,v) ||
	  escp2_has_cap(model, MODEL_COMMAND, MODEL_COMMAND_2000,v));
}

static int
escp2_use_extended_commands(int model, const stp_vars_t v, int use_softweave)
{
  return (escp2_has_cap(model, MODEL_COMMAND, MODEL_COMMAND_PRO, v) ||
	  (escp2_has_cap(model, MODEL_VARIABLE_DOT, MODEL_VARIABLE_YES, v) &&
	   use_softweave));
}


static char *
c_strdup(const char *s)
{
  char *ret = stp_malloc(strlen(s) + 1);
  strcpy(ret, s);
  return ret;
}

static int
verify_resolution(const res_t *res, int model, const stp_vars_t v)
{
  int nozzle_width =
    (escp2_base_separation(model, v) / escp2_nozzle_separation(model, v));
  int nozzles = escp2_nozzles(model, v);
  if (escp2_ink_type(model, res->resid, v) != -1 &&
      res->vres <= escp2_max_vres(model, v) &&
      res->hres <= escp2_max_hres(model, v) &&
      res->vres >= escp2_min_vres(model, v) &&
      res->hres >= escp2_min_hres(model, v) &&
      (nozzles == 1 ||
       ((res->vres / nozzle_width) * nozzle_width) == res->vres))
    {
      int xdpi = res->hres;
      int physical_xdpi = escp2_base_res(model, res->resid, v);
      int horizontal_passes, oversample;
      if (physical_xdpi > xdpi)
	physical_xdpi = xdpi;
      horizontal_passes = xdpi / physical_xdpi;
      oversample = horizontal_passes * res->vertical_passes
	* res->vertical_oversample;
      if (horizontal_passes < 1)
	horizontal_passes = 1;
      if (oversample < 1)
	oversample = 1;
      if (((horizontal_passes * res->vertical_passes) <= 8) &&
	  (! res->softweave || (nozzles > 1 && nozzles > oversample)))
	return 1;
    }
  return 0;
}

static int
verify_papersize(const stp_papersize_t pt, int model, const stp_vars_t v)
{
  unsigned int height_limit, width_limit;
  unsigned int min_height_limit, min_width_limit;
  unsigned int pwidth = stp_papersize_get_width(pt);
  unsigned int pheight = stp_papersize_get_height(pt);
  width_limit = escp2_max_paper_width(model, v);
  height_limit = escp2_max_paper_height(model, v);
  min_width_limit = escp2_min_paper_width(model, v);
  min_height_limit = escp2_min_paper_height(model, v);
  if (strlen(stp_papersize_get_name(pt)) > 0 &&
      pwidth <= width_limit && pheight <= height_limit &&
      (pheight >= min_height_limit || pheight == 0) &&
      (pwidth >= min_width_limit || pwidth == 0) &&
      (pwidth == 0 || pheight > 0 ||
       escp2_has_cap(model, MODEL_ROLLFEED, MODEL_ROLLFEED_YES, v)))
    return 1;
  else
    return 0;
}

/*
 * 'escp2_parameters()' - Return the parameter values for the given parameter.
 */

static stp_param_t *				/* O - Parameter values */
escp2_parameters(const stp_printer_t printer,	/* I - Printer model */
		 const char *ppd_file,	/* I - PPD file (not used) */
		 const char *name,	/* I - Name of parameter */
		 int  *count)		/* O - Number of values */
{
  int		i;
  stp_param_t	*valptrs;
  int		model = stp_printer_get_model(printer);
  const stp_vars_t v = stp_printer_get_printvars(printer);

  if (count == NULL)
    return (NULL);

  *count = 0;

  if (name == NULL)
    return (NULL);

  if (strcmp(name, "PageSize") == 0)
  {
    int papersizes = stp_known_papersizes();
    valptrs = stp_malloc(sizeof(stp_param_t) * papersizes);
    *count = 0;

    for (i = 0; i < papersizes; i++)
    {
      const stp_papersize_t pt = stp_get_papersize_by_index(i);
      if (verify_papersize(pt, model, v))
	{
	  valptrs[*count].name = c_strdup(stp_papersize_get_name(pt));
	  valptrs[*count].text = c_strdup(stp_papersize_get_text(pt));
	  (*count)++;
	}
    }

    return (valptrs);
  }
  else if (strcmp(name, "Resolution") == 0)
  {
    const res_t *res = escp2_reslist(model, v);
    valptrs = stp_malloc(sizeof(stp_param_t) * reslist_count(res));
    *count = 0;
    while (res->hres)
      {
	if (verify_resolution(res, model, v))
	  {
	    valptrs[*count].name = c_strdup(res->name);
	    valptrs[*count].text = c_strdup(_(res->text));
	    (*count)++;
	  }
	res++;
      }
    return (valptrs);
  }
  else if (strcmp(name, "InkType") == 0)
  {
    const inklist_t *inks = escp2_inklist(model, v);
    int ninktypes = inks->n_inks;
    if (ninktypes == 0)
      {
	*count = 0;
	return NULL;
      }
    valptrs = stp_malloc(sizeof(stp_param_t) * ninktypes);
    for (i = 0; i < ninktypes; i++)
    {
      valptrs[i].name = c_strdup(inks->inknames[i]->name);
      valptrs[i].text = c_strdup(_(inks->inknames[i]->text));
    }
    *count = ninktypes;
    return valptrs;
  }
  else if (strcmp(name, "MediaType") == 0)
  {
    const paperlist_t *p = escp2_paperlist(model, v);
    int nmediatypes = p->paper_count;
    valptrs = stp_malloc(sizeof(stp_param_t) * nmediatypes);
    if (nmediatypes == 0)
      {
	*count = 0;
	return NULL;
      }
    for (i = 0; i < nmediatypes; i++)
    {
      valptrs[i].name = c_strdup(p->papers[i].name);
      valptrs[i].text = c_strdup(_(p->papers[i].text));
    }
    *count = nmediatypes;
    return valptrs;
  }
  else if (strcmp(name, "InputSlot") == 0)
  {
    const input_slot_list_t *slots = escp2_input_slots(model, v);
    int ninputslots = slots->n_input_slots;
    if (ninputslots == 0)
      {
	valptrs = NULL;
	*count = 0;
	return NULL;
      }
    valptrs = stp_malloc(sizeof(stp_param_t) * ninputslots);
    for (i = 0; i < ninputslots; i++)
    {
      valptrs[i].name = c_strdup(slots->slots[i].name);
      valptrs[i].text = c_strdup(_(slots->slots[i].text));
    }
    *count = ninputslots;
    return valptrs;
  }
  else
    return (NULL);
}

static const res_t *
escp2_find_resolution(int model, const stp_vars_t v, const char *resolution)
{
  const res_t *res;
  if (!resolution || !strcmp(resolution, ""))
    return NULL;
  for (res = escp2_reslist(model, v);;res++)
    {
      if (!strcmp(resolution, res->name))
	return res;
      else if (!strcmp(res->name, ""))
	return NULL;
    }
}

/*
 * 'escp2_imageable_area()' - Return the imageable area of the page.
 */

static void
escp2_imageable_area(const stp_printer_t printer,	/* I - Printer model */
		     const stp_vars_t v,   /* I */
		     int  *left,	/* O - Left position in points */
		     int  *right,	/* O - Right position in points */
		     int  *bottom,	/* O - Bottom position in points */
		     int  *top)		/* O - Top position in points */
{
  int	width, height;			/* Size of page */
  int	rollfeed = 0;			/* Roll feed selected */
  int model = stp_printer_get_model(printer);
  const char *input_slot = stp_get_media_source(v);

  if (input_slot && strlen(input_slot) > 0)
    {
      int i;
      const input_slot_list_t *slots = escp2_input_slots(model, v);
      for (i = 0; i < slots->n_input_slots; i++)
	{
	  if (slots->slots[i].name &&
	      strcmp(input_slot, slots->slots[i].name) == 0)
	    {
	      rollfeed = slots->slots[i].is_roll_feed;
	      break;
	    }
	}
    }

  stp_default_media_size(printer, v, &width, &height);

  if (rollfeed)
    {
      *left =	escp2_roll_left_margin(model, v);
      *right =	width - escp2_roll_right_margin(model, v);
      *top =	height - escp2_roll_top_margin(model, v);
      *bottom =	escp2_roll_bottom_margin(model, v);
    }
  else
    {
      *left =	escp2_left_margin(model, v);
      *right =	width - escp2_right_margin(model, v);
      *top =	height - escp2_top_margin(model, v);
      *bottom =	escp2_bottom_margin(model, v);
    }
}

static void
escp2_limit(const stp_printer_t printer,	/* I - Printer model */
	    const stp_vars_t v,			/* I */
	    int *width,
	    int *height,
	    int *min_width,
	    int *min_height)
{
  int model = stp_printer_get_model(printer);
  *width =	escp2_max_paper_width(model, v);
  *height =	escp2_max_paper_height(model, v);
  *min_width =	escp2_min_paper_width(model, v);
  *min_height =	escp2_min_paper_height(model, v);
}

static const char *
escp2_default_parameters(const stp_printer_t printer,
			 const char *ppd_file,
			 const char *name)
{
  int i;
  int model = stp_printer_get_model(printer);
  const stp_vars_t v = stp_printer_get_printvars(printer);
  if (name == NULL)
    return NULL;
  if (strcmp(name, "PageSize") == 0)
    {
      int papersizes = stp_known_papersizes();
      for (i = 0; i < papersizes; i++)
	{
	  const stp_papersize_t pt = stp_get_papersize_by_index(i);
	  if (verify_papersize(pt, model, v))
	    return (stp_papersize_get_name(pt));
	}
      return NULL;
    }
  else if (strcmp(name, "Resolution") == 0)
    {
      int model = stp_printer_get_model(printer);
      stp_vars_t v = stp_printer_get_printvars(printer);
      const res_t *res = escp2_reslist(model, v);
      while (res->hres)
	{
	  if (res->vres >= 360 && res->hres >= 360 &&
	      verify_resolution(res, model, v))
	    return (res->name);
	  res++;
	}
      return NULL;
    }
  else if (strcmp(name, "InkType") == 0)
    {
      const inklist_t *inks = escp2_inklist(model, v);
      return inks->inknames[0]->name;
    }
  else if (strcmp(name, "MediaType") == 0)
    {
      const paperlist_t *p = escp2_paperlist(model, v);
      return (p->papers[0].name);
    }
  else if (strcmp(name, "InputSlot") == 0)
    {
      const input_slot_list_t *slots = escp2_input_slots(model, v);
      if (slots->n_input_slots)
	return slots->slots[0].name;
      return NULL;
    }
  else
    return (NULL);
}

static void
escp2_describe_resolution(const stp_printer_t printer,
			  const char *resolution, int *x, int *y)
{
  int model = stp_printer_get_model(printer);
  stp_vars_t v = stp_printer_get_printvars(printer);
  const res_t *res = escp2_reslist(model, v);
  while (res->hres)
    {
      if (!strcmp(resolution, res->name) &&
	  verify_resolution(res, model, v))
	{
	  *x = res->external_hres;
	  *y = res->external_vres;
	  return;
	}
      res++;
    }
  *x = -1;
  *y = -1;
}

static void
escp2_reset_printer(const escp2_init_t *init)
{
  /*
   * Magic initialization string that's needed to take printer out of
   * packet mode.
   */
  const init_sequence_t *inits = escp2_preinit_sequence(init->model, init->v);
  if (inits)
    stp_zfwrite(inits->data, inits->length, 1, init->v);

  stp_puts("\033@", init->v);				/* ESC/P2 reset */
}

static void
print_remote_param(const stp_vars_t v, const char *param, const char *value)
{
  unsigned bytes = 2 + strlen(param) + strlen(value);
  stp_zprintf(v, "\033(R%c%c%c%s:%s", (bytes & 0xff), ((bytes >> 8) & 0xff), 0,
	      param, value);
  stp_zprintf(v, "\033%c%c%c", 0, 0, 0);
}

static void
print_remote_int_param(const stp_vars_t v, const char *param, int value)
{
  char buf[64];
  (void) snprintf(buf, 64, "%d", value);
  print_remote_param(v, param, buf);
}

static void
print_remote_float_param(const stp_vars_t v, const char *param, double value)
{
  char buf[64];
  (void) snprintf(buf, 64, "%f", value);
  print_remote_param(v, param, buf);
}

static void
escp2_set_remote_sequence(const escp2_init_t *init)
{
  /* Magic remote mode commands, whatever they do */

  if (stp_debug_level & STP_DBG_MARK_FILE)
    {
      print_remote_param(init->v, "Package", PACKAGE);
      print_remote_param(init->v, "Version", VERSION);
      print_remote_param(init->v, "Release Date", RELEASE_DATE);
      print_remote_param(init->v, "Driver", stp_get_driver(init->v));
      print_remote_param(init->v, "Resolution", stp_get_resolution(init->v));
      print_remote_param(init->v, "Media Size", stp_get_media_size(init->v));
      print_remote_param(init->v, "Media Type", stp_get_media_type(init->v));
      print_remote_param(init->v, "Media Source", stp_get_media_source(init->v));
      print_remote_param(init->v, "Ink Type", stp_get_ink_type(init->v));
      print_remote_param(init->v, "Dither", stp_get_dither_algorithm(init->v));
      print_remote_int_param(init->v, "Output Type", stp_get_output_type(init->v));
      print_remote_int_param(init->v, "Orientation", stp_get_orientation(init->v));
      print_remote_int_param(init->v, "Left", stp_get_left(init->v));
      print_remote_int_param(init->v, "Top", stp_get_top(init->v));
      print_remote_int_param(init->v, "Image Type", stp_get_image_type(init->v));
      print_remote_int_param(init->v, "Page Width", stp_get_page_width(init->v));
      print_remote_int_param(init->v, "Page Height", stp_get_page_height(init->v));
      print_remote_int_param(init->v, "Input Model", stp_get_input_color_model(init->v));
      print_remote_int_param(init->v, "Output Model", stp_get_output_color_model(init->v));
      print_remote_float_param(init->v, "Brightness", stp_get_brightness(init->v));
      print_remote_float_param(init->v, "Scaling", stp_get_scaling(init->v));
      print_remote_float_param(init->v, "Gamma", stp_get_gamma(init->v));
      print_remote_float_param(init->v, "App Gamma", stp_get_app_gamma(init->v));
      print_remote_float_param(init->v, "Contrast", stp_get_contrast(init->v));
      print_remote_float_param(init->v, "Cyan", stp_get_cyan(init->v));
      print_remote_float_param(init->v, "Magenta", stp_get_magenta(init->v));
      print_remote_float_param(init->v, "Yellow", stp_get_yellow(init->v));
      print_remote_float_param(init->v, "Saturation", stp_get_saturation(init->v));
      print_remote_float_param(init->v, "Density", stp_get_density(init->v));
      print_remote_int_param(init->v, "Model", init->model);
      print_remote_int_param(init->v, "Output_type", init->output_type);
      print_remote_int_param(init->v, "Ydpi", init->ydpi);
      print_remote_int_param(init->v, "Xdpi", init->xdpi);
      print_remote_int_param(init->v, "Physical_xdpi", init->physical_xdpi);
      print_remote_int_param(init->v, "Use_softweave", init->use_softweave);
      print_remote_int_param(init->v, "Use_microweave", init->use_microweave);
      print_remote_int_param(init->v, "Page_true_height", init->page_true_height);
      print_remote_int_param(init->v, "Page_width", init->page_width);
      print_remote_int_param(init->v, "Page_top", init->page_top);
      print_remote_int_param(init->v, "Page_bottom", init->page_bottom);
      print_remote_int_param(init->v, "Nozzles", init->nozzles);
      print_remote_int_param(init->v, "Nozzle_separation", init->nozzle_separation);
      print_remote_int_param(init->v, "Horizontal_passes", init->horizontal_passes);
      print_remote_int_param(init->v, "Vertical_passes", init->vertical_passes);
      print_remote_int_param(init->v, "Vertical_oversample", init->vertical_oversample);
      print_remote_int_param(init->v, "Bits", init->bits);
      print_remote_int_param(init->v, "Unidirectional", init->unidirectional);
      print_remote_int_param(init->v, "Resid", init->resid);
      print_remote_int_param(init->v, "Initial_vertical_offset", init->initial_vertical_offset);
      print_remote_int_param(init->v, "Total_channels", init->total_channels);
      print_remote_int_param(init->v, "Use_black_parameters", init->use_black_parameters);
      print_remote_int_param(init->v, "Use_fast_360", init->use_fast_360);
      print_remote_param(init->v, "Ink name", init->inkname->name);
      print_remote_int_param(init->v, "  is_color", init->inkname->is_color);
      print_remote_int_param(init->v, "  inkset", init->inkname->inkset);
      stp_puts("\033@", init->v);
    }
  if (escp2_has_advanced_command_set(init->model, init->v) || init->input_slot)
    {
      int feed_sequence = 0;
      const paper_t *p =
	get_media_type(init->model, init->paper_type, init->v);
      /* Enter remote mode */
      stp_zprintf(init->v, "\033(R%c%c%cREMOTE1", 8, 0, 0);
      if (escp2_has_cap(init->model, MODEL_COMMAND,
			MODEL_COMMAND_PRO, init->v))
	{
	  if (p)
	    {
	      stp_zprintf(init->v, "PH%c%c%c%c", 2, 0, 0, p->paper_thickness);
	      if (escp2_has_cap(init->model, MODEL_VACUUM, MODEL_VACUUM_YES,
				init->v))
		stp_zprintf(init->v, "SN%c%c%c%c%c",
			    3, 0, 0, 5, p->vacuum_intensity);
	      stp_zprintf(init->v, "SN%c%c%c%c%c",
			  3, 0, 0, 4, p->feed_adjustment);
	    }
	}
      else if (escp2_has_advanced_command_set(init->model, init->v))
	{
	  if (p)
	    feed_sequence = p->paper_feed_sequence;
	  /* Function unknown */
	  stp_zprintf(init->v, "PM%c%c%c%c", 2, 0, 0, 0);
	  /* Set mechanism sequence */
	  stp_zprintf(init->v, "SN%c%c%c%c%c", 3, 0, 0, 0, feed_sequence);
	  if (escp2_has_cap(init->model, MODEL_XZEROMARGIN,
			    MODEL_XZEROMARGIN_YES, init->v))
	    stp_zprintf(init->v, "FP%c%c%c%c%c", 3, 0, 0, 0260, 0xff);
	}
      if (init->input_slot)
	{
	  int divisor = escp2_base_separation(init->model, init->v) / 360;
	  int height = init->page_true_height * 5 / divisor;
	  if (init->input_slot->init_sequence.length)
	    stp_zfwrite(init->input_slot->init_sequence.data,
			init->input_slot->init_sequence.length, 1, init->v);
	  switch (init->input_slot->roll_feed_cut_flags)
	    {
	    case ROLL_FEED_CUT_ALL:
	      stp_zprintf(init->v, "JS%c%c%c%c", 2, 0, 0, 0);
	      stp_zprintf(init->v, "CO%c%c%c%c%c%c%c%c%c%c",
			  8, 0, 0, 0, 1, 0, 0, 0, 0, 0);
	      stp_zprintf(init->v, "CO%c%c%c%c%c%c%c%c%c%c",
			  8, 0, 0, 0, 0, 0,
			  height & 0xff, (height >> 8) & 0xff,
			  (height >> 16) & 0xff, (height >> 24) & 0xff);
	      break;
	    case ROLL_FEED_CUT_LAST:
	      stp_zprintf(init->v, "CO%c%c%c%c%c%c%c%c%c%c",
			  8, 0, 0, 0, 1, 0, 0, 0, 0, 0);
	      stp_zprintf(init->v, "CO%c%c%c%c%c%c%c%c%c%c",
			  8, 0, 0, 0, 2, 0,
			  height & 0xff, (height >> 8) & 0xff,
			  (height >> 16) & 0xff, (height >> 24) & 0xff);
	      break;
	    default:
	      break;
	    }
	}

      /* Exit remote mode */
      stp_zprintf(init->v, "\033%c%c%c", 0, 0, 0);
    }
}

static void
escp2_set_graphics_mode(const escp2_init_t *init)
{
  stp_zfwrite("\033(G\001\000\001", 6, 1, init->v);
}

static void
escp2_set_resolution(const escp2_init_t *init)
{
  if (escp2_use_extended_commands(init->model, init->v, init->use_softweave))
    {
      int hres = escp2_max_hres(init->model, init->v);
      stp_zprintf(init->v, "\033(U\005%c%c%c%c%c%c", 0, hres / init->ydpi,
		  hres / init->ydpi, hres / init->xdpi,
		  hres % 256, hres / 256);
    }
  else
    stp_zprintf(init->v, "\033(U\001%c%c", 0, 3600 / init->ydpi);
}

static void
escp2_set_color(const escp2_init_t *init)
{
  if (init->use_fast_360)
    stp_zprintf(init->v, "\033(K\002%c%c%c", 0, 0, 3);
  else if (escp2_has_cap(init->model, MODEL_GRAYMODE, MODEL_GRAYMODE_YES,
			 init->v))
    stp_zprintf(init->v, "\033(K\002%c%c%c", 0, 0,
		(init->use_black_parameters ? 1 : 2));
}

static void
escp2_set_microweave(const escp2_init_t *init)
{
  stp_zprintf(init->v, "\033(i\001%c%c", 0, init->use_microweave);
}

static void
escp2_set_printhead_speed(const escp2_init_t *init)
{
  if (init->unidirectional)
    {
      stp_zprintf(init->v, "\033U%c", 1);
      if (init->xdpi > escp2_enhanced_resolution(init->model, init->v))
	stp_zprintf(init->v, "\033(s%c%c%c", 1, 0, 2);
    }
  else
    stp_zprintf(init->v, "\033U%c", 0);
}

static void
escp2_set_dot_size(const escp2_init_t *init)
{
  /* Dot size */
  int drop_size = escp2_ink_type(init->model, init->resid, init->v);
  if (drop_size >= 0)
    stp_zprintf(init->v, "\033(e\002%c%c%c", 0, 0, drop_size);
}

static void
escp2_set_page_height(const escp2_init_t *init)
{
  int l = init->ydpi * init->page_true_height / 72;
  if (escp2_use_extended_commands(init->model, init->v, init->use_softweave))
    stp_zprintf(init->v, "\033(C\004%c%c%c%c%c", 0,
		l & 0xff, (l >> 8) & 0xff, (l >> 16) & 0xff, (l >> 24) & 0xff);
  else
    stp_zprintf(init->v, "\033(C\002%c%c%c", 0, l & 255, l >> 8);
}

static void
escp2_set_margins(const escp2_init_t *init)
{
  int left = init->ydpi * (init->page_true_height - init->page_bottom) / 72;
  int top = init->ydpi * (init->page_true_height - init->page_top) / 72;

  top += init->initial_vertical_offset;
  if (escp2_use_extended_commands(init->model, init->v, init->use_softweave))
    {
      if (escp2_has_cap(init->model,MODEL_COMMAND,MODEL_COMMAND_2000,init->v)||
	  escp2_has_cap(init->model,MODEL_COMMAND,MODEL_COMMAND_PRO,init->v))
	stp_zprintf(init->v, "\033(c\010%c%c%c%c%c%c%c%c%c", 0,
		    top & 0xff, (top >> 8) & 0xff,
		    (top >> 16) & 0xff, (top >> 24) & 0xff,
		    left & 0xff, (left >> 8) & 0xff,
		    (left >> 16) & 0xff, (left >> 24) & 0xff);
      else
	stp_zprintf(init->v, "\033(c\004%c%c%c%c%c", 0,
		    top & 0xff, top >> 8, left & 0xff, left >> 8);
    }
  else
    stp_zprintf(init->v, "\033(c\004%c%c%c%c%c", 0,
		top & 0xff, top >> 8, left & 0xff, left >> 8);
}

static void
escp2_set_form_factor(const escp2_init_t *init)
{
  if (escp2_has_advanced_command_set(init->model, init->v))
    {
      int page_width = init->page_width * init->ydpi / 72;
      int page_height = init->page_true_height * init->ydpi / 72;

      if (escp2_has_cap(init->model, MODEL_XZEROMARGIN, MODEL_XZEROMARGIN_YES,
			init->v))
	/* Make the page 2/10" wider (probably ignored by the printer) */
	page_width += 144 * init->xdpi / 720;

      stp_zprintf(init->v, "\033(S\010%c%c%c%c%c%c%c%c%c", 0,
		  ((page_width >> 0) & 0xff), ((page_width >> 8) & 0xff),
		  ((page_width >> 16) & 0xff), ((page_width >> 24) & 0xff),
		  ((page_height >> 0) & 0xff), ((page_height >> 8) & 0xff),
		  ((page_height >> 16) & 0xff), ((page_height >> 24) & 0xff));
    }
}

static void
escp2_set_printhead_resolution(const escp2_init_t *init)
{
  if (escp2_use_extended_commands(init->model, init->v, init->use_softweave))
    {
      int xres;
      int yres;
      int scale = escp2_resolution_scale(init->model, init->v);

      xres = scale / init->physical_xdpi;

      if (escp2_has_cap(init->model, MODEL_COMMAND, MODEL_COMMAND_PRO,
			init->v) && !init->use_softweave)
	yres = scale / init->ydpi;
      else
	yres = (init->nozzle_separation * scale /
		escp2_base_separation(init->model, init->v));

      /* Magic resolution cookie */
      stp_zprintf(init->v, "\033(D%c%c%c%c%c%c", 4, 0,
		  scale % 256, scale / 256, yres, xres);
    }
}

static void
escp2_init_printer(const escp2_init_t *init)
{
  escp2_reset_printer(init);
  escp2_set_remote_sequence(init);
  escp2_set_graphics_mode(init);
  escp2_set_resolution(init);
  escp2_set_color(init);
  escp2_set_microweave(init);
  escp2_set_printhead_speed(init);
  escp2_set_dot_size(init);
  escp2_set_printhead_resolution(init);
  escp2_set_page_height(init);
  escp2_set_margins(init);
  escp2_set_form_factor(init);
}

static void
escp2_deinit_printer(const escp2_init_t *init)
{
  stp_puts("\033@", init->v);	/* ESC/P2 reset */
  if (escp2_has_advanced_command_set(init->model, init->v) || init->input_slot)
    {
      const init_sequence_t *deinit =
	escp2_postinit_remote_sequence(init->model, init->v);
      stp_zprintf(init->v, /* Enter remote mode */
		  "\033(R\010%c%cREMOTE1", 0, 0);
      if (init->input_slot && init->input_slot->deinit_sequence.length)
	stp_zfwrite(init->input_slot->deinit_sequence.data,
		    init->input_slot->deinit_sequence.length, 1, init->v);
      /* Load settings from NVRAM */
      stp_zprintf(init->v, "LD%c%c", 0, 0);

      /* Magic deinit sequence reported by Simone Falsini */
      if (deinit)
	stp_zfwrite(deinit->data, deinit->length, 1, init->v);
      /* Exit remote mode */
      stp_zprintf(init->v, "\033%c%c%c", 0, 0, 0);

    }
}

static void
adjust_print_quality(const escp2_init_t *init, void *dither,
		     double **lum_adjustment, double **sat_adjustment,
		     double **hue_adjustment)
{
  const paper_t *pt;
  const stp_vars_t nv = init->v;
  int i;
  const escp2_variable_inkset_t *inks;
  double k_upper, k_lower;
  double paper_k_upper;
  int		ink_spread;
  /*
   * Compute the LUT.  For now, it's 8 bit, but that may eventually
   * sometimes change.
   */
  k_lower = init->inkname->k_lower;
  k_upper = init->inkname->k_upper;

  pt = get_media_type(init->model, stp_get_media_type(nv), nv);
  if (pt)
    {
      stp_set_density(nv, stp_get_density(nv) * pt->base_density);
      if (init->total_channels >= 5)
	{
	  stp_set_cyan(nv, stp_get_cyan(nv) * pt->p_cyan);
	  stp_set_magenta(nv, stp_get_magenta(nv) * pt->p_magenta);
	  stp_set_yellow(nv, stp_get_yellow(nv) * pt->p_yellow);
	}
      else
	{
	  stp_set_cyan(nv, stp_get_cyan(nv) * pt->cyan);
	  stp_set_magenta(nv, stp_get_magenta(nv) * pt->magenta);
	  stp_set_yellow(nv, stp_get_yellow(nv) * pt->yellow);
	}
      stp_set_saturation(nv, stp_get_saturation(nv) * pt->saturation);
      stp_set_gamma(nv, stp_get_gamma(nv) * pt->gamma);
      k_lower *= pt->k_lower_scale;
      paper_k_upper = pt->k_upper;
      k_upper *= pt->k_upper;
    }
  else				/* Can't find paper type? Assume plain */
    {
      stp_set_density(nv, stp_get_density(nv) * .8);
      k_lower *= .1;
      paper_k_upper = .5;
      k_upper *= .5;
    }
  stp_set_density(nv, stp_get_density(nv) *
		  escp2_density(init->model, init->resid, nv));
  if (stp_get_density(nv) > 1.0)
    stp_set_density(nv, 1.0);
  if (init->output_type == OUTPUT_GRAY)
    stp_set_gamma(nv, stp_get_gamma(nv) / .8);
  stp_compute_lut(nv, 256);

  for (i = 0; i <= NCOLORS; i++)
    stp_dither_set_black_level(dither, i, 1.0);
  stp_dither_set_black_lower(dither, k_lower);
  stp_dither_set_black_upper(dither, k_upper);

  inks = escp2_inks(init->model, init->resid, init->inkname->inkset, nv);
  if (inks)
    for (i = 0; i < NCOLORS; i++)
      if ((*inks)[i])
	stp_dither_set_ranges(dither, i, (*inks)[i]->count, (*inks)[i]->range,
			      (*inks)[i]->density * paper_k_upper *
			      stp_get_density(nv));

  switch (stp_get_image_type(nv))
    {
    case IMAGE_LINE_ART:
      stp_dither_set_ink_spread(dither, 19);
      break;
    case IMAGE_SOLID_TONE:
      stp_dither_set_ink_spread(dither, 15);
      break;
    case IMAGE_CONTINUOUS:
      ink_spread = 13;
      if (init->ydpi > escp2_max_vres(init->model, nv))
	ink_spread++;
      if (init->bits > 1)
	ink_spread++;
      stp_dither_set_ink_spread(dither, ink_spread);
      break;
    }
  stp_dither_set_density(dither, stp_get_density(nv));
  if (init->inkname->lum_adjustment)
    {
      *lum_adjustment = stp_malloc(sizeof(double) * 49);
      for (i = 0; i < 49; i++)
	{
	  (*lum_adjustment)[i] = init->inkname->lum_adjustment[i];
	  if (pt && pt->lum_adjustment)
	    (*lum_adjustment)[i] *= pt->lum_adjustment[i];
	}
    }
  if (init->inkname->sat_adjustment)
    {
      *sat_adjustment = stp_malloc(sizeof(double) * 49);
      for (i = 0; i < 49; i++)
	{
	  (*sat_adjustment)[i] = init->inkname->sat_adjustment[i];
	  if (pt && pt->sat_adjustment)
	    (*sat_adjustment)[i] *= pt->sat_adjustment[i];
	}
    }
  if (init->inkname->hue_adjustment)
    {
      *hue_adjustment = stp_malloc(sizeof(double) * 49);
      for (i = 0; i < 49; i++)
	{
	  (*hue_adjustment)[i] = init->inkname->hue_adjustment[i];
	  if (pt && pt->hue_adjustment)
	    (*hue_adjustment)[i] += pt->hue_adjustment[i];
	}
    }
}

static int
count_channels(const escp2_inkname_t *inks)
{
  int answer = 0;
  int i;
  for (i = 0; i < NCOLORS; i++)
    if (inks->channels[i])
      answer += inks->channels[i]->n_subchannels;
  return answer;
}

static const escp2_inkname_t *
get_inktype(const stp_printer_t printer, const stp_vars_t v, int model)
{
  const char	*ink_type = stp_get_ink_type(v);
  const inklist_t *ink_list = escp2_inklist(model, v);
  int i;

  for (i = 0; i < ink_list->n_inks; i++)
    {
      if (strcmp(ink_type, ink_list->inknames[i]->name) == 0)
	return ink_list->inknames[i];
    }
  ink_type = escp2_default_parameters(printer, NULL, "InkType");
  for (i = 0; i < ink_list->n_inks; i++)
    {
      if (strcmp(ink_type, ink_list->inknames[i]->name) == 0)
	return ink_list->inknames[i];
    }
  return NULL;
}

static const physical_subchannel_t default_black_subchannels[] =
{
  { 0, 0, 0 }
};

static const ink_channel_t default_black_channels =
{
  default_black_subchannels, 1
};

static const escp2_inkname_t default_black_ink =
{
  NULL, NULL, 0, 0, 0, 0, NULL, NULL, NULL,
  {
    &default_black_channels, NULL, NULL, NULL
  }
};

static int
setup_ink_types(const escp2_inkname_t *ink_type,
		escp2_privdata_t *privdata,
		unsigned char **cols,
		int *head_offset,
		stp_dither_data_t *dt,
		int channel_limit,
		int line_length)
{
  int i;
  int channels_in_use = 0;
  for (i = 0; i < channel_limit; i++)
    {
      const ink_channel_t *channel = ink_type->channels[i];
      if (channel)
	{
	  int j;
	  for (j = 0; j < channel->n_subchannels; j++)
	    {
	      cols[channels_in_use] = stp_zalloc(line_length);
	      privdata->channels[channels_in_use] = &(channel->channels[j]);
	      stp_add_channel(dt, cols[channels_in_use], i, j);
	      head_offset[channels_in_use] = channel->channels[j].head_offset;
	      channels_in_use++;
	    }
	}
    }
  return channels_in_use;
}

/*
 * 'escp2_print()' - Print an image to an EPSON printer.
 */
static void
escp2_do_print(const stp_printer_t printer,		/* I - Model */
	       stp_image_t     *image,		/* I - Image to print */
	       const stp_vars_t    v,
	       int print_op)
{
  unsigned char *cmap = stp_get_cmap(v);
  int		model = stp_printer_get_model(printer);
  int		output_type = stp_get_output_type(v);
  int		orientation = stp_get_orientation(v);
  int		top = stp_get_top(v);
  int		left = stp_get_left(v);

  int		i;
  int		y;		/* Looping vars */

  const res_t	*res;
  int		xdpi;
  int		ydpi;	/* Resolution */
  int		physical_ydpi;
  int		physical_xdpi;
  int		undersample;
  int		resid;

  int		n;		/* Output number */
  unsigned short *out;	/* Output pixels (16-bit) */
  unsigned char	*in;		/* Input pixels */
  int		page_left,	/* Left margin of page */
		page_right,	/* Right margin of page */
		page_top,	/* Top of page */
		page_bottom,	/* Bottom of page */
		page_width,	/* Width of page */
		page_height,	/* Height of page */
		page_true_height;	/* True height of page */
  int		out_width,	/* Width of image on page */
		out_height,	/* Height of image on page */
		out_bpp,	/* Output bytes per pixel */
		length;		/* Length of raster data */
  int		errdiv,		/* Error dividend */
		errmod,		/* Error modulus */
		errval,		/* Current error value */
		errline,	/* Current raster line */
		errlast;	/* Last raster line loaded */
  stp_convert_t	colorfunc;	/* Color conversion function... */
  int   	image_height,
		image_width,
		image_bpp;

  int		nozzles;
  int		nozzle_separation;
  int		horizontal_passes;

  int		bits;
  void *	weave;
  void *	dither;
  stp_vars_t	nv = stp_allocate_copy(v);
  escp2_init_t	init;
  int		max_vres;
  unsigned char **cols;
  int 		*head_offset;
  int 		max_head_offset;
  double 	*lum_adjustment = NULL;
  double	*sat_adjustment = NULL;
  double	*hue_adjustment = NULL;
  escp2_privdata_t privdata;
  stp_dither_data_t *dt;
  const escp2_inkname_t *ink_type;
  int 		total_channels;
  int 		channels_in_use;
  int 		channel_limit;
  const char *input_slot = stp_get_media_source(v);

  if (!stp_get_verified(nv))
    {
      stp_eprintf(nv, "Print options not verified; cannot print.\n");
      return;
    }

  privdata.undersample = 1;
  privdata.denominator = 1;
  privdata.initial_vertical_offset = 0;
  privdata.printed_something = 0;
  privdata.last_color = -1;
  stp_set_driver_data(nv, &privdata);

  ink_type = get_inktype(printer, nv, model);
  total_channels = count_channels(ink_type);
  if (output_type != OUTPUT_GRAY && output_type != OUTPUT_MONOCHROME &&
      !ink_type->is_color)
    {
      output_type = OUTPUT_GRAY;
      stp_set_output_type(nv, OUTPUT_GRAY);
    }

 /*
  * Figure out the output resolution...
  */
  res = escp2_find_resolution(model, nv, stp_get_resolution(nv));
  if (res->softweave)
    max_vres = escp2_max_vres(model, nv);
  else
    max_vres = escp2_base_resolution(model, nv);
  xdpi = res->hres;
  ydpi = res->vres;
  resid = res->resid;
  undersample = res->vertical_undersample;
  privdata.undersample = res->vertical_undersample;
  privdata.denominator = res->vertical_denominator;

  physical_xdpi = escp2_base_res(model, resid, nv);
  if (physical_xdpi > xdpi)
    physical_xdpi = xdpi;

  physical_ydpi = ydpi;
  if (ydpi > max_vres)
    physical_ydpi = max_vres;

  bits = escp2_bits(model, resid, nv);

 /*
  * Compute the output size...
  */
  image->init(image);
  image_height = image->height(image);
  image_width = image->width(image);

  escp2_imageable_area(printer, nv, &page_left, &page_right,
		       &page_bottom, &page_top);

  stp_compute_page_parameters(page_right, page_left, page_top, page_bottom,
			      stp_get_scaling(nv), image_width, image_height,
			      image, &orientation, &page_width, &page_height,
			      &out_width, &out_height, &left, &top);

  /*
   * Recompute the image height and width.  If the image has been
   * rotated, these will change from previously.
   */
  image_height = image->height(image);
  image_width = image->width(image);
  stp_default_media_size(printer, nv, &n, &page_true_height);

 /*
  * Convert image size to printer resolution...
  */

  out_width  = xdpi * out_width / 72;
  out_height = ydpi * out_height / 72;
  length = (out_width + 7) / 8;

  left = physical_ydpi * undersample * left / 72 / res->vertical_denominator;

 /*
  * Adjust for zero-margin printing...
  */

  if (escp2_has_cap(model, MODEL_XZEROMARGIN, MODEL_XZEROMARGIN_YES, nv))
    {
     /*
      * In zero-margin mode, the origin is about 3/20" to the left of the
      * paper's left edge.
      */
      left += escp2_zero_margin_offset(model, nv) * physical_ydpi *
	undersample / max_vres / res->vertical_denominator;
    }


  /*
   * Set up the output channels
   */
  cols = stp_zalloc(sizeof(unsigned char *) * total_channels);
  privdata.channels =
    stp_zalloc(sizeof(physical_subchannel_t *) * total_channels);
  head_offset = stp_zalloc(sizeof(int) * total_channels);

  memset(head_offset, 0, sizeof(head_offset));
  channel_limit = NCOLORS;
  if (output_type == OUTPUT_GRAY || output_type == OUTPUT_MONOCHROME)
    channel_limit = 1;

  dt = stp_create_dither_data();

  channels_in_use = setup_ink_types(ink_type, &privdata, cols, head_offset,
				    dt, channel_limit, length * bits);
  if (channels_in_use == 0)
    {
      ink_type = &default_black_ink;
      channels_in_use = setup_ink_types(ink_type, &privdata, cols, head_offset,
					dt, channel_limit, length * bits);
    }
  if (channels_in_use == 1)
    head_offset[0] = 0;
  if (escp2_has_cap(model, MODEL_FAST_360, MODEL_FAST_360_YES, nv) &&
      (ink_type->inkset == INKSET_CMYK || channels_in_use == 1) &&
      xdpi == 360 && ydpi == 360)
    init.use_fast_360 = 1;
  else
    init.use_fast_360 = 0;

  /*
   * Set up the printer-specific parameters (weaving)
   */
  if (res->softweave)
    {
      horizontal_passes = xdpi / physical_xdpi;
      if ((output_type == OUTPUT_GRAY || output_type == OUTPUT_MONOCHROME) &&
	  channels_in_use == 1 &&
	  (ydpi >= (escp2_base_separation(model, nv) /
		    escp2_black_nozzle_separation(model, nv))) &&
	  (escp2_max_black_resolution(model, nv) < 0 ||
	   ydpi <= escp2_max_black_resolution(model, nv)) &&
	  escp2_black_nozzles(model, nv))
	init.use_black_parameters = 1;
      else
	init.use_black_parameters = 0;
      if (init.use_fast_360)
	{
	  nozzles = escp2_fast_nozzles(model, nv);
	  nozzle_separation = escp2_fast_nozzle_separation(model, nv);
	  privdata.min_nozzles = escp2_min_fast_nozzles(model, nv);
	}
      else if (init.use_black_parameters)
	{
	  nozzles = escp2_black_nozzles(model, nv);
	  nozzle_separation = escp2_black_nozzle_separation(model, nv);
	  privdata.min_nozzles = escp2_min_black_nozzles(model, nv);
	}
      else
	{
	  nozzles = escp2_nozzles(model, nv);
	  nozzle_separation = escp2_nozzle_separation(model, nv);
	  privdata.min_nozzles = escp2_min_nozzles(model, nv);
	}
      init.nozzle_separation = nozzle_separation;
      nozzle_separation =
	nozzle_separation * ydpi / escp2_base_separation(model, nv);
    }
  else
    {
      horizontal_passes = xdpi / escp2_base_resolution(model, nv);
      nozzles = 1;
      privdata.min_nozzles = 1;
      nozzle_separation = 1;
      init.nozzle_separation = nozzle_separation;
      init.use_black_parameters = 0;
    }
  init.nozzles = nozzles;

  if (horizontal_passes == 0)
    horizontal_passes = 1;

  max_head_offset = 0;
  if (channels_in_use > 1)
    for (i = 0; i < total_channels; i++)
      {
	head_offset[i] = head_offset[i] * ydpi/escp2_base_separation(model,nv);
	if (head_offset[i] > max_head_offset)
	  max_head_offset = head_offset[i];
      }

 /*
  * Send ESC/P2 initialization commands...
  */
  init.model = model;
  init.output_type = output_type;
  if (init.output_type == OUTPUT_MONOCHROME)
    init.output_type = OUTPUT_GRAY;
  init.ydpi = ydpi * undersample;
  if (init.ydpi > escp2_max_vres(init.model, init.v))
    init.ydpi = escp2_max_vres(init.model, init.v);
  init.xdpi = xdpi;
  init.physical_xdpi = physical_xdpi;
  init.use_softweave = res->softweave;
  init.use_microweave = res->microweave;
  init.page_true_height = page_true_height;
  init.page_width = page_width;
  init.page_top = page_top;
  if (init.output_type == OUTPUT_GRAY && channels_in_use == 1)
    {
      if (init.use_black_parameters)
	init.initial_vertical_offset =
	  escp2_black_initial_vertical_offset(init.model, init.v) * init.ydpi /
	  escp2_base_separation(model, nv);
      else
	init.initial_vertical_offset =
	  head_offset[0] +
	  (escp2_initial_vertical_offset(init.model, init.v) *
	   init.ydpi / escp2_base_separation(model, nv));
    }
  else
    init.initial_vertical_offset =
      escp2_initial_vertical_offset(init.model, init.v) * init.ydpi /
      escp2_base_separation(model, nv);

   /* adjust bottom margin for a 480 like head configuration */
  init.page_bottom = page_bottom - max_head_offset * 72 / ydpi;
  if ((max_head_offset * 72 % ydpi) != 0)
    init.page_bottom -= 1;
  if (init.page_bottom < 0)
    init.page_bottom = 0;

  init.horizontal_passes = horizontal_passes;
  init.vertical_passes = res->vertical_passes;
  init.vertical_oversample = res->vertical_oversample;
  init.unidirectional = res->unidirectional;
  init.resid = resid;
  init.bits = bits;
  init.paper_type = stp_get_media_type(nv);
  init.media_source =  stp_get_media_source(nv);
  init.v = nv;
  init.inkname = ink_type;
  init.total_channels = total_channels;

  init.input_slot = NULL;
  if (input_slot && strlen(input_slot) > 0)
    {
      const input_slot_list_t *slots = escp2_input_slots(model, v);
      for (i = 0; i < slots->n_input_slots; i++)
	{
	  if (slots->slots[i].name &&
	      strcmp(input_slot, slots->slots[i].name) == 0)
	    {
	      init.input_slot = &(slots->slots[i]);
	      break;
	    }
	}
    }
  if (init.input_slot && init.input_slot->roll_feed_cut_flags)
    {
      init.page_true_height += 4;
      init.page_top += 2;
      init.page_bottom += 2;
      top += 2;
      page_height += 2;
    }

  if (print_op & OP_JOB_START)
    escp2_init_printer(&init);
  if (print_op & OP_JOB_PRINT)
    {

      /*
       * Allocate memory for the raster data...
       */

      weave = stp_initialize_weave(nozzles, nozzle_separation,
				   horizontal_passes, res->vertical_passes,
				   res->vertical_oversample, total_channels,
				   bits,
				   out_width, out_height,
				   top * physical_ydpi / 72,
				   (page_height * physical_ydpi / 72 +
				    escp2_extra_feed(model, nv) *
				    physical_ydpi /
				    escp2_base_resolution(model, nv)),
				   1, head_offset, nv, flush_pass,
				   FILLFUNC, PACKFUNC, COMPUTEFUNC);

      stp_set_output_color_model(nv, COLOR_MODEL_CMY);
      image_bpp = image->bpp(image);
      colorfunc = stp_choose_colorfunc(output_type, image_bpp, cmap,
				       &out_bpp, nv);

      in  = stp_malloc(image_width * image_bpp);
      out = stp_malloc(image_width * out_bpp * 2);

      errdiv  = image_height / out_height;
      errmod  = image_height % out_height;
      errval  = 0;
      errlast = -1;
      errline  = 0;

      if (xdpi > ydpi)
	dither = stp_init_dither(image_width, out_width, 1, xdpi / ydpi, nv);
      else
	dither = stp_init_dither(image_width, out_width, ydpi / xdpi, 1, nv);

      adjust_print_quality(&init, dither,
			   &lum_adjustment, &sat_adjustment, &hue_adjustment);

      /*
       * Let the user know what we're doing...
       */

      image->progress_init(image);

      QUANT(0);
      for (y = 0; y < out_height; y ++)
	{
	  int duplicate_line = 1;
	  int zero_mask;
	  if ((y & 63) == 0)
	    image->note_progress(image, y, out_height);

	  if (errline != errlast)
	    {
	      errlast = errline;
	      duplicate_line = 0;
	      if (image->get_row(image, in, errline) != STP_IMAGE_OK)
		break;
	      (*colorfunc)(nv, in, out, &zero_mask, image_width, image_bpp,
			   cmap, hue_adjustment, lum_adjustment,
			   sat_adjustment);
	    }
	  QUANT(1);

	  stp_dither(out, y, dither, dt, duplicate_line, zero_mask);
	  QUANT(2);

	  stp_write_weave(weave, length, ydpi, model, out_width, left,
			  xdpi, physical_xdpi, cols);
	  QUANT(3);
	  errval += errmod;
	  errline += errdiv;
	  if (errval >= out_height)
	    {
	      errval -= out_height;
	      errline ++;
	    }
	  QUANT(4);
	}
      image->progress_conclude(image);
      stp_flush_all(weave, model, out_width, left, ydpi, xdpi, physical_xdpi);
      QUANT(5);

      /*
       * Cleanup...
       */
      stp_destroy_weave(weave);
      stp_free_dither(dither);
      stp_free_lut(nv);
      stp_free(in);
      stp_free(out);
      if (hue_adjustment)
	stp_free(hue_adjustment);
      if (sat_adjustment)
	stp_free(sat_adjustment);
      if (lum_adjustment)
	stp_free(lum_adjustment);
      if (!privdata.printed_something)
	stp_putc('\n', nv);
      stp_puts("\014", nv);	/* Eject page */
    }
  if (print_op & OP_JOB_END)
    escp2_deinit_printer(&init);
  
  stp_free_dither_data(dt);
  for (i = 0; i < total_channels; i++)
    if (cols[i])
      stp_free((unsigned char *) cols[i]);
  stp_free(cols);
  stp_free(head_offset);
  stp_free(privdata.channels);

#ifdef QUANTIFY
  print_timers(nv);
#endif
  stp_free_vars(nv);
}

static void
escp2_print(const stp_printer_t printer,		/* I - Model */
	    stp_image_t     *image,		/* I - Image to print */
	    const stp_vars_t    v)
{
  int op = OP_JOB_PRINT;
  if (stp_get_job_mode(v) == STP_JOB_MODE_PAGE)
    op = OP_JOB_START | OP_JOB_PRINT | OP_JOB_END;
  escp2_do_print(printer, image, v, op);
}

static int
escp2_job_start(const stp_printer_t printer,		/* I - Model */
		stp_image_t     *image,		/* I - Image to print */
		const stp_vars_t    v)
{
  if (!stp_get_verified(v))
    return 0;
  if (stp_get_job_mode(v) != STP_JOB_MODE_JOB)
    return 0;
  escp2_do_print(printer, image, v, OP_JOB_START);
  return 1;
}

static int
escp2_job_end(const stp_printer_t printer,		/* I - Model */
	      stp_image_t     *image,		/* I - Image to print */
	      const stp_vars_t    v)
{
  if (!stp_get_verified(v))
    return 0;
  if (stp_get_job_mode(v) != STP_JOB_MODE_JOB)
    return 0;
  escp2_do_print(printer, image, v, OP_JOB_END);
  return 1;
}

const stp_printfuncs_t stp_escp2_printfuncs =
{
  escp2_parameters,
  stp_default_media_size,
  escp2_imageable_area,
  escp2_limit,
  escp2_print,
  escp2_default_parameters,
  escp2_describe_resolution,
  stp_verify_printer_params,
  escp2_job_start,
  escp2_job_end
};

static void
set_vertical_position(stp_softweave_t *sw, stp_pass_t *pass, int model,
		      const stp_vars_t v)
{
  escp2_privdata_t *pd = (escp2_privdata_t *) stp_get_driver_data(v);
  int advance = pass->logicalpassstart - sw->last_pass_offset -
    (escp2_separation_rows(model, v) - 1);
  advance *= pd->undersample;
  if (pass->logicalpassstart > sw->last_pass_offset ||
      pd->initial_vertical_offset != 0)
    {
      int a0, a1, a2, a3;
      advance += pd->initial_vertical_offset;
      pd->initial_vertical_offset = 0;
      a0 = advance         & 0xff;
      a1 = (advance >> 8)  & 0xff;
      a2 = (advance >> 16) & 0xff;
      a3 = (advance >> 24) & 0xff;
      if (escp2_use_extended_commands(model, v, sw->jets > 1))
	stp_zprintf(v, "\033(v%c%c%c%c%c%c", 4, 0, a0, a1, a2, a3);
      else
	stp_zprintf(v, "\033(v%c%c%c%c", 2, 0, a0, a1);
      sw->last_pass_offset = pass->logicalpassstart;
    }
}

static void
set_color(stp_softweave_t *sw, stp_pass_t *pass, int model, const stp_vars_t v,
	  int color)
{
  escp2_privdata_t *pd = (escp2_privdata_t *) stp_get_driver_data(v);
  if (pd->last_color != color &&
      ! escp2_use_extended_commands(model, v, sw->jets > 1))
    {
      int ncolor = pd->channels[color]->color;
      int density = pd->channels[color]->density;
      if (density >= 0)
	stp_zprintf(v, "\033(r%c%c%c%c", 2, 0, density, ncolor);
      else
	stp_zprintf(v, "\033r%c", ncolor);
      pd->last_color = color;
    }
}

static void
set_horizontal_position(stp_softweave_t *sw, stp_pass_t *pass, int model,
			const stp_vars_t v, int hoffset, int ydpi,
			int xdpi, int vertical_subpass)
{
  int microoffset = vertical_subpass & (sw->horizontal_weave - 1);
  escp2_privdata_t *pd = (escp2_privdata_t *) stp_get_driver_data(v);
  if (!escp2_has_advanced_command_set(model, v) &&
      (xdpi <= escp2_base_resolution(model, v) ||
       escp2_max_hres(model, v) < 1440))
    {
      int pos = (hoffset + microoffset);
      if (pos > 0)
	stp_zprintf(v, "\033\\%c%c", pos & 255, pos >> 8);
    }
  else if (escp2_has_cap(model, MODEL_COMMAND, MODEL_COMMAND_PRO,v) ||
	   (escp2_has_advanced_command_set(model, v) &&
	    escp2_has_cap(model, MODEL_VARIABLE_DOT, MODEL_VARIABLE_YES, v)))
    {
      int pos = ((hoffset * xdpi * pd->denominator / ydpi) + microoffset);
      if (pos > 0)
	stp_zprintf(v, "\033($%c%c%c%c%c%c", 4, 0,
		    pos & 255, (pos >> 8) & 255,
		    (pos >> 16) & 255, (pos >> 24) & 255);
    }
  else
    {
      int pos = ((hoffset * escp2_max_hres(model, v) * pd->denominator / ydpi)+
		 microoffset);
      if (pos > 0)
	stp_zprintf(v, "\033(\\%c%c%c%c%c%c", 4, 0, 160, 5,
		    pos & 255, pos >> 8);
    }
}

static void
send_print_command(stp_softweave_t *sw, stp_pass_t *pass, int model, int color,
		   int lwidth, const stp_vars_t v, int hoffset, int ydpi,
		   int xdpi, int physical_xdpi, int nlines)
{
  if (!escp2_has_cap(model, MODEL_COMMAND, MODEL_COMMAND_PRO,v) &&
      sw->jets == 1 && sw->bitwidth == 1)
    {
      int ygap = 3600 / ydpi;
      int xgap = 3600 / xdpi;
      if (ydpi == 720 && escp2_extra_720dpi_separation(model, v))
	ygap *= escp2_extra_720dpi_separation(model, v);
      stp_zprintf(v, "\033.%c%c%c%c%c%c", COMPRESSION, ygap, xgap,
		  1, lwidth & 255, (lwidth >> 8) & 255);
    }
  else if (!escp2_has_cap(model, MODEL_COMMAND, MODEL_COMMAND_PRO,v) &&
	   escp2_has_cap(model, MODEL_VARIABLE_DOT, MODEL_VARIABLE_NO, v))
    {
      int ygap = 3600 / ydpi;
      int xgap = 3600 / physical_xdpi;
      if (escp2_extra_720dpi_separation(model, v))
	ygap *= escp2_extra_720dpi_separation(model, v);
      else if (escp2_pseudo_separation_rows(model, v) > 0)
	ygap *= escp2_pseudo_separation_rows(model, v);
      else
	ygap *= escp2_separation_rows(model, v);
      stp_zprintf(v, "\033.%c%c%c%c%c%c", COMPRESSION, ygap, xgap,
		  nlines, lwidth & 255, (lwidth >> 8) & 255);
    }
  else
    {
      escp2_privdata_t *pd = (escp2_privdata_t *) stp_get_driver_data(v);
      int ncolor = pd->channels[color]->color;
      int nwidth = sw->bitwidth * ((lwidth + 7) / 8);
      if (pd->channels[color]->density >= 0)
	ncolor |= (pd->channels[color]->density << 4);
      stp_zprintf(v, "\033i%c%c%c%c%c%c%c", ncolor, COMPRESSION,
		  sw->bitwidth, nwidth & 255, (nwidth >> 8) & 255,
		  nlines & 255, (nlines >> 8) & 255);
    }
}

static void
send_extra_data(stp_softweave_t *sw, stp_vars_t v, int extralines, int lwidth)
{
  int k, l;
  int bytes_to_fill = sw->bitwidth * ((lwidth + 7) / 8);
  int full_blocks = bytes_to_fill / 128;
  int leftover = bytes_to_fill % 128;
  int total_bytes = extralines * (full_blocks + 1) * 2;
  unsigned char *buf = stp_malloc(total_bytes);
  total_bytes = 0;
  for (k = 0; k < extralines; k++)
    {
      for (l = 0; l < full_blocks; l++)
	{
	  buf[total_bytes++] = 129;
	  buf[total_bytes++] = 0;
	}
      if (leftover == 1)
	{
	  buf[total_bytes++] = 1;
	  buf[total_bytes++] = 0;
	}
      else if (leftover > 0)
	{
	  buf[total_bytes++] = 257 - leftover;
	  buf[total_bytes++] = 0;
	}
    }
  stp_zfwrite((const char *) buf, total_bytes, 1, v);
  stp_free(buf);
}

static void
flush_pass(stp_softweave_t *sw, int passno, int model, int width,
	   int hoffset, int ydpi, int xdpi, int physical_xdpi,
	   int vertical_subpass)
{
  int j;
  const stp_vars_t v = (sw->v);
  escp2_privdata_t *pd = (escp2_privdata_t *) stp_get_driver_data(v);
  stp_lineoff_t *lineoffs = stp_get_lineoffsets_by_pass(sw, passno);
  stp_lineactive_t *lineactive = stp_get_lineactive_by_pass(sw, passno);
  const stp_linebufs_t *bufs = stp_get_linebases_by_pass(sw, passno);
  stp_pass_t *pass = stp_get_pass_by_pass(sw, passno);
  stp_linecount_t *linecount = stp_get_linecount_by_pass(sw, passno);
  int lwidth = (width + (sw->horizontal_weave - 1)) / sw->horizontal_weave;

  ydpi *= pd->undersample;

  if (ydpi > escp2_max_vres(model, v))
    ydpi = escp2_max_vres(model, v);
  for (j = 0; j < sw->ncolors; j++)
    {
      if (lineactive[0].v[j] > 0)
	{
	  int nlines = linecount[0].v[j];
	  int minlines = pd->min_nozzles;
	  int extralines = 0;
	  if (nlines < minlines)
	    {
	      extralines = minlines - nlines;
	      nlines = minlines;
	    }
	  set_vertical_position(sw, pass, model, v);
	  set_color(sw, pass, model, v, j);
	  set_horizontal_position(sw, pass, model, v, hoffset, ydpi, xdpi,
				  vertical_subpass);
	  send_print_command(sw, pass, model, j, lwidth, v, hoffset, ydpi,
			     xdpi, physical_xdpi, nlines);

	  /*
	   * Send the data
	   */
	  stp_zfwrite((const char *)bufs[0].v[j], lineoffs[0].v[j], 1, v);
	  if (extralines)
	    send_extra_data(sw, v, extralines, lwidth);
	  stp_putc('\r', v);
	  pd->printed_something = 1;
	}
      lineoffs[0].v[j] = 0;
      linecount[0].v[j] = 0;
    }

  sw->last_pass = pass->pass;
  pass->pass = -1;
}

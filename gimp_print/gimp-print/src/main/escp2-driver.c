/*
 * "$Id: escp2-driver.c,v 1.1.1.2 2004/12/22 23:49:38 jlovell Exp $"
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
#include <gimp-print/gimp-print-intl-internal.h>
#include "gimp-print-internal.h"
#include <string.h>
#include "print-escp2.h"

#ifdef __GNUC__
#define inline __inline__
#endif

static escp2_privdata_t *
get_privdata(stp_vars_t *v)
{
  return (escp2_privdata_t *) stp_get_component_data(v, "Driver");
}

static void
escp2_reset_printer(stp_vars_t *v)
{
  escp2_privdata_t *pd = get_privdata(v);
  /*
   * Magic initialization string that's needed to take printer out of
   * packet mode.
   */
  if (pd->init_sequence)
    stp_zfwrite(pd->init_sequence->data, pd->init_sequence->bytes, 1, v);

  stp_send_command(v, "\033@", "");
}

static void
print_remote_param(stp_vars_t *v, const char *param, const char *value)
{
  stp_send_command(v, "\033(R", "bcscs", '\0', param, ':',
		    value ? value : "NULL");
  stp_send_command(v, "\033", "ccc", 0, 0, 0);
}

static void
print_remote_int_param(stp_vars_t *v, const char *param, int value)
{
  char buf[64];
  (void) snprintf(buf, 64, "%d", value);
  print_remote_param(v, param, buf);
}

static void
print_remote_float_param(stp_vars_t *v, const char *param, double value)
{
  char buf[64];
  (void) snprintf(buf, 64, "%f", value);
  print_remote_param(v, param, buf);
}

static void
print_debug_params(stp_vars_t *v)
{
  escp2_privdata_t *pd = get_privdata(v);
  stp_parameter_list_t params = stp_get_parameter_list(v);
  int count = stp_parameter_list_count(params);
  int i;
  print_remote_param(v, "Package", PACKAGE);
  print_remote_param(v, "Version", VERSION);
  print_remote_param(v, "Release Date", RELEASE_DATE);
  print_remote_param(v, "Driver", stp_get_driver(v));
  print_remote_int_param(v, "Left", stp_get_left(v));
  print_remote_int_param(v, "Top", stp_get_top(v));
  print_remote_int_param(v, "Page Width", stp_get_page_width(v));
  print_remote_int_param(v, "Page Height", stp_get_page_height(v));
  print_remote_int_param(v, "Model", stp_get_model_id(v));
  print_remote_int_param(v, "Ydpi", pd->res->vres);
  print_remote_int_param(v, "Xdpi", pd->res->hres);
  print_remote_int_param(v, "Use_softweave", pd->res->softweave);
  print_remote_int_param(v, "Use_printer_weave", pd->res->printer_weave);
  print_remote_int_param(v, "Use_printer_weave", pd->use_printer_weave);
  print_remote_int_param(v, "Page_left", pd->page_left);
  print_remote_int_param(v, "Page_right", pd->page_right);
  print_remote_int_param(v, "Page_top", pd->page_top);
  print_remote_int_param(v, "Page_bottom", pd->page_bottom);
  print_remote_int_param(v, "Page_width", pd->page_width);
  print_remote_int_param(v, "Page_height", pd->page_height);
  print_remote_int_param(v, "Page_true_height", pd->page_true_height);
  print_remote_int_param(v, "Image_left", pd->image_left);
  print_remote_int_param(v, "Image_top", pd->image_top);
  print_remote_int_param(v, "Image_width", pd->image_width);
  print_remote_int_param(v, "Image_height", pd->image_height);
  print_remote_int_param(v, "Image_scaled_width", pd->image_scaled_width);
  print_remote_int_param(v, "Image_scaled_height", pd->image_scaled_height);
  print_remote_int_param(v, "Image_printed_width", pd->image_printed_width);
  print_remote_int_param(v, "Image_printed_height", pd->image_printed_height);
  print_remote_int_param(v, "Image_left_position", pd->image_left_position);
  print_remote_int_param(v, "Nozzles", pd->nozzles);
  print_remote_int_param(v, "Nozzle_separation", pd->nozzle_separation);
  print_remote_int_param(v, "Horizontal_passes", pd->horizontal_passes);
  print_remote_int_param(v, "Vertical_passes", pd->res->vertical_passes);
  print_remote_int_param(v, "Physical_xdpi", pd->physical_xdpi);
  print_remote_int_param(v, "Page_management_units", pd->page_management_units);
  print_remote_int_param(v, "Vertical_units", pd->vertical_units);
  print_remote_int_param(v, "Horizontal_units", pd->horizontal_units);
  print_remote_int_param(v, "Micro_units", pd->micro_units);
  print_remote_int_param(v, "Unit_scale", pd->unit_scale);
  print_remote_int_param(v, "Zero_advance", pd->send_zero_pass_advance);
  print_remote_int_param(v, "Bits", pd->bitwidth);
  print_remote_int_param(v, "Resid", pd->ink_resid);
  print_remote_int_param(v, "Drop Size", pd->drop_size);
  print_remote_int_param(v, "Initial_vertical_offset", pd->initial_vertical_offset);
  print_remote_int_param(v, "Channels_in_use", pd->channels_in_use);
  print_remote_int_param(v, "Logical_channels", pd->logical_channels);
  print_remote_int_param(v, "Physical_channels", pd->physical_channels);
  print_remote_int_param(v, "Use_black_parameters", pd->use_black_parameters);
  print_remote_int_param(v, "Use_fast_360", pd->use_fast_360);
  print_remote_int_param(v, "Command_set", pd->command_set);
  print_remote_int_param(v, "Variable_dots", pd->variable_dots);
  print_remote_int_param(v, "Has_vacuum", pd->has_vacuum);
  print_remote_int_param(v, "Has_graymode", pd->has_graymode);
  print_remote_int_param(v, "Base_separation", pd->base_separation);
  print_remote_int_param(v, "Resolution_scale", pd->resolution_scale);
  print_remote_int_param(v, "Printing_resolution", pd->printing_resolution);
  print_remote_int_param(v, "Separation_rows", pd->separation_rows);
  print_remote_int_param(v, "Pseudo_separation_rows", pd->pseudo_separation_rows);
  print_remote_int_param(v, "Extra_720dpi_separation", pd->extra_720dpi_separation);
  print_remote_param(v, "Ink name", pd->inkname->name);
  print_remote_int_param(v, "  channels", pd->inkname->channel_set->channel_count);
  print_remote_int_param(v, "  inkset", pd->inkname->inkset);
  for (i = 0; i < count; i++)
    {
      const stp_parameter_t *p = stp_parameter_list_param(params, i);
      switch (p->p_type)
	{
	case STP_PARAMETER_TYPE_DOUBLE:
	  if (stp_check_float_parameter(v, p->name, STP_PARAMETER_DEFAULTED))
	    print_remote_float_param(v, p->name,
				     stp_get_float_parameter(v, p->name));
	  break;
	case STP_PARAMETER_TYPE_INT:
	  if (stp_check_int_parameter(v, p->name, STP_PARAMETER_DEFAULTED))
	    print_remote_int_param(v, p->name,
				   stp_get_int_parameter(v, p->name));
	  break;
	case STP_PARAMETER_TYPE_DIMENSION:
	  if (stp_check_dimension_parameter(v, p->name, STP_PARAMETER_DEFAULTED))
	    print_remote_int_param(v, p->name,
				   stp_get_dimension_parameter(v, p->name));
	  break;
	case STP_PARAMETER_TYPE_BOOLEAN:
	  if (stp_check_boolean_parameter(v, p->name, STP_PARAMETER_DEFAULTED))
	    print_remote_int_param(v, p->name,
				   stp_get_boolean_parameter(v, p->name));
	  break;
	case STP_PARAMETER_TYPE_STRING_LIST:
	  if (stp_check_string_parameter(v, p->name, STP_PARAMETER_DEFAULTED))
	    print_remote_param(v, p->name,
			       stp_get_string_parameter(v, p->name));
	  break;
	case STP_PARAMETER_TYPE_CURVE:
	  if (stp_check_curve_parameter(v, p->name, STP_PARAMETER_DEFAULTED))
	    {
	      char *curve =
		stp_curve_write_string(stp_get_curve_parameter(v, p->name));
	      print_remote_param(v, p->name, curve);
	      stp_free(curve);
	    }
	  break;
	default:
	  break;
	}
    }
  stp_parameter_list_destroy(params);
  stp_send_command(v, "\033", "ccc", 0, 0, 0);
}

static void
escp2_set_remote_sequence(stp_vars_t *v)
{
  /* Magic remote mode commands, whatever they do */
  escp2_privdata_t *pd = get_privdata(v);

  if (stp_get_debug_level() & STP_DBG_MARK_FILE)
    print_debug_params(v);
  if (pd->advanced_command_set || pd->input_slot)
    {
      int feed_sequence = 0;
      /* Enter remote mode */
      stp_send_command(v, "\033(R", "bcs", 0, "REMOTE1");
      if (pd->command_set == MODEL_COMMAND_PRO)
	{
	  if (pd->paper_type)
	    {
	      stp_send_command(v, "PH", "bcc", 0,
			       pd->paper_type->paper_thickness);
	      if (pd->has_vacuum)
		stp_send_command(v, "SN", "bccc", 0, 5,
				 pd->paper_type->vacuum_intensity);
	      stp_send_command(v, "SN", "bccc", 0, 4,
			       pd->paper_type->feed_adjustment);
	    }
	}
      else if (pd->advanced_command_set)
	{
	  if (pd->paper_type)
	    feed_sequence = pd->paper_type->paper_feed_sequence;
	  /* Function unknown */
	  stp_send_command(v, "PM", "bh", 0);
	  /* Set mechanism sequence */
	  stp_send_command(v, "SN", "bccc", 0, 0, feed_sequence);
	  if (stp_get_boolean_parameter(v, "FullBleed"))
	    stp_send_command(v, "FP", "bch", 0, 0xffb0);
	}
      if (pd->input_slot)
	{
	  int divisor = pd->base_separation / 360;
	  int height = pd->page_true_height * 5 / divisor;
	  if (pd->input_slot->init_sequence.bytes)
	    stp_zfwrite(pd->input_slot->init_sequence.data,
			pd->input_slot->init_sequence.bytes, 1, v);
	  switch (pd->input_slot->roll_feed_cut_flags)
	    {
	    case ROLL_FEED_CUT_ALL:
	      stp_send_command(v, "JS", "bh", 0);
	      stp_send_command(v, "CO", "bccccl", 0, 0, 1, 0, 0);
	      stp_send_command(v, "CO", "bccccl", 0, 0, 0, 0, height);
	      break;
	    case ROLL_FEED_CUT_LAST:
	      stp_send_command(v, "CO", "bccccl", 0, 0, 1, 0, 0);
	      stp_send_command(v, "CO", "bccccl", 0, 0, 2, 0, height);
	      break;
	    default:
	      break;
	    }
	}

      /* Exit remote mode */

      stp_send_command(v, "\033", "ccc", 0, 0, 0);
    }
}

static void
escp2_set_graphics_mode(stp_vars_t *v)
{
  stp_send_command(v, "\033(G", "bc", 1);
}

static void
escp2_set_resolution(stp_vars_t *v)
{
  escp2_privdata_t *pd = get_privdata(v);
  if (pd->use_extended_commands)
    stp_send_command(v, "\033(U", "bccch",
		     pd->unit_scale / pd->page_management_units,
		     pd->unit_scale / pd->vertical_units,
		     pd->unit_scale / pd->horizontal_units,
		     pd->unit_scale);
  else
    stp_send_command(v, "\033(U", "bc",
		     pd->unit_scale / pd->page_management_units);
}

static void
escp2_set_color(stp_vars_t *v)
{
  escp2_privdata_t *pd = get_privdata(v);
  if (pd->use_fast_360)
    stp_send_command(v, "\033(K", "bcc", 0, 3);
  else if (pd->has_graymode)
    stp_send_command(v, "\033(K", "bcc", 0,
		     (pd->use_black_parameters ? 1 : 2));
}

static void
escp2_set_printer_weave(stp_vars_t *v)
{
  escp2_privdata_t *pd = get_privdata(v);
  int printer_weave_parm = 0;
  if (pd->printer_weave)
    printer_weave_parm = pd->printer_weave->value;
  else if (pd->res->printer_weave)
    printer_weave_parm = pd->res->printer_weave;
  stp_send_command(v, "\033(i", "bc", printer_weave_parm);
}

static void
escp2_set_printhead_speed(stp_vars_t *v)
{
  escp2_privdata_t *pd = get_privdata(v);
  const char *direction = stp_get_string_parameter(v, "PrintingDirection");
  int unidirectional;
  if (direction && strcmp(direction, "Unidirectional") == 0)
    unidirectional = 1;
  else if (direction && strcmp(direction, "Bidirectional") == 0)
    unidirectional = 0;
  else if (pd->res->hres >= 720 && pd->res->vres >= 720)
    unidirectional = 1;
  else
    unidirectional = 0;
  if (unidirectional)
    {
      stp_send_command(v, "\033U", "c", 1);
      if (pd->res->hres > pd->printing_resolution)
	stp_send_command(v, "\033(s", "bc", 2);
    }
  else
    stp_send_command(v, "\033U", "c", 0);
}

static void
escp2_set_dot_size(stp_vars_t *v)
{
  escp2_privdata_t *pd = get_privdata(v);
  /* Dot size */
  if (pd->drop_size >= 0)
    stp_send_command(v, "\033(e", "bcc", 0, pd->drop_size);
}

static void
escp2_set_page_height(stp_vars_t *v)
{
  escp2_privdata_t *pd = get_privdata(v);
  int l = pd->page_management_units * pd->page_true_height / 72;
  if (pd->use_extended_commands)
    stp_send_command(v, "\033(C", "bl", l);
  else
    stp_send_command(v, "\033(C", "bh", l);
}

static void
escp2_set_margins(stp_vars_t *v)
{
  escp2_privdata_t *pd = get_privdata(v);
  int bot = pd->page_management_units * pd->page_bottom / 72;
  int top = pd->page_management_units * pd->page_top / 72;

  top += pd->initial_vertical_offset;
  if (pd->use_extended_commands &&
      (pd->command_set == MODEL_COMMAND_2000 ||
       pd->command_set == MODEL_COMMAND_PRO))
    stp_send_command(v, "\033(c", "bll", top, bot);
  else
    stp_send_command(v, "\033(c", "bhh", top, bot);
}

static void
escp2_set_form_factor(stp_vars_t *v)
{
  escp2_privdata_t *pd = get_privdata(v);
  if (pd->advanced_command_set)
    {
      int w = pd->page_width * pd->page_management_units / 72;
      int h = pd->page_true_height * pd->page_management_units / 72;

      if (stp_get_boolean_parameter(v, "FullBleed"))
	/* Make the page 160/360" wider for full bleed printing. */
	/* Per the Epson manual, the margin should be expanded by 80/360" */
	/* so we need to do this on the left and the right */
	w += 320 * pd->page_management_units / 720;

      stp_send_command(v, "\033(S", "bll", w, h);
    }
}

static void
escp2_set_printhead_resolution(stp_vars_t *v)
{
  escp2_privdata_t *pd = get_privdata(v);
  if (pd->use_extended_commands)
    {
      int xres;
      int yres = pd->resolution_scale;

      xres = pd->resolution_scale / pd->physical_xdpi;

      if (pd->command_set == MODEL_COMMAND_PRO && !pd->res->softweave)
	yres = yres /  pd->res->vres;
      else
	yres = yres * pd->nozzle_separation / pd->base_separation;

      /* Magic resolution cookie */
      stp_send_command(v, "\033(D", "bhcc", pd->resolution_scale, yres, xres);
    }
}

static void
set_vertical_position(stp_vars_t *v, stp_pass_t *pass)
{
  escp2_privdata_t *pd = get_privdata(v);
  int advance = pass->logicalpassstart - pd->last_pass_offset -
    (pd->separation_rows - 1);
  advance = advance * pd->vertical_units / pd->res->vres;
  if (pass->logicalpassstart > pd->last_pass_offset ||
      (pd->send_zero_pass_advance && pass->pass > pd->last_pass) ||
      pd->printing_initial_vertical_offset != 0)
    {
      advance += pd->printing_initial_vertical_offset;
      pd->printing_initial_vertical_offset = 0;
      if (pd->use_extended_commands)
	stp_send_command(v, "\033(v", "bl", advance);
      else
	stp_send_command(v, "\033(v", "bh", advance);
      pd->last_pass_offset = pass->logicalpassstart;
      pd->last_pass = pass->pass;
    }
}

static void
set_color(stp_vars_t *v, stp_pass_t *pass, int color)
{
  escp2_privdata_t *pd = get_privdata(v);
  if (pd->last_color != color && ! pd->use_extended_commands)
    {
      int ncolor = pd->channels[color]->color;
      int subchannel = pd->channels[color]->subchannel;
      if (subchannel >= 0)
	stp_send_command(v, "\033(r", "bcc", subchannel, ncolor);
      else
	stp_send_command(v, "\033r", "c", ncolor);
      pd->last_color = color;
    }
}

static void
set_horizontal_position(stp_vars_t *v, stp_pass_t *pass, int vertical_subpass)
{
  escp2_privdata_t *pd = get_privdata(v);
  int microoffset = (vertical_subpass & (pd->horizontal_passes - 1)) *
    pd->image_scaled_width / pd->image_printed_width;
  int pos = pd->image_left_position + microoffset;

  if (pos != 0)
    {
      /* Note hard-coded 1440 -- from Epson manuals */
      if (pd->command_set == MODEL_COMMAND_PRO || pd->variable_dots)
	stp_send_command(v, "\033($", "bl", pos);
      else if (pd->advanced_command_set || pd->res->hres > 720)
	stp_send_command(v, "\033(\\", "bhh", pd->micro_units, pos);
      else
	stp_send_command(v, "\033\\", "h", pos);
    }
}

static void
send_print_command(stp_vars_t *v, stp_pass_t *pass, int color, int nlines)
{
  escp2_privdata_t *pd = get_privdata(v);
  int lwidth = (pd->image_printed_width + (pd->horizontal_passes - 1)) /
    pd->horizontal_passes;
  if (pd->command_set == MODEL_COMMAND_PRO || pd->variable_dots)
    {
      int ncolor = pd->channels[color]->color;
      int subchannel = pd->channels[color]->subchannel;
      int nwidth = pd->bitwidth * ((lwidth + 7) / 8);
      if (subchannel >= 0)
	ncolor |= (subchannel << 4);
      stp_send_command(v, "\033i", "ccchh", ncolor, COMPRESSION,
		       pd->bitwidth, nwidth, nlines);
    }
  else
    {
      int ygap = 3600 / pd->vertical_units;
      int xgap = 3600 / pd->physical_xdpi;
      if (pd->nozzles == 1)
	{
	  if (pd->vertical_units == 720 && pd->extra_720dpi_separation)
	    ygap *= pd->extra_720dpi_separation;
	}
      else if (pd->extra_720dpi_separation)
	ygap *= pd->extra_720dpi_separation;
      else if (pd->pseudo_separation_rows > 0)
	ygap *= pd->pseudo_separation_rows;
      else
	ygap *= pd->separation_rows;
      stp_send_command(v, "\033.", "cccch", COMPRESSION, ygap, xgap, nlines,
		       lwidth);
    }
}

static void
send_extra_data(stp_vars_t *v, int extralines)
{
  escp2_privdata_t *pd = get_privdata(v);
  int lwidth = (pd->image_printed_width + (pd->horizontal_passes - 1)) /
    pd->horizontal_passes;
#if TEST_UNCOMPRESSED
  int i;
  for (i = 0; i < pd->bitwidth * (lwidth + 7) / 8; i++)
    stp_putc(0, v);
#else  /* !TEST_UNCOMPRESSED */
  int k, l;
  int bytes_to_fill = pd->bitwidth * ((lwidth + 7) / 8);
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
#endif /* TEST_UNCOMPRESSED */
}

void
stpi_escp2_init_printer(stp_vars_t *v)
{
  escp2_reset_printer(v);
  escp2_set_remote_sequence(v);
  escp2_set_graphics_mode(v);
  escp2_set_resolution(v);
  escp2_set_color(v);
  escp2_set_printer_weave(v);
  escp2_set_printhead_speed(v);
  escp2_set_dot_size(v);
  escp2_set_printhead_resolution(v);
  escp2_set_page_height(v);
  escp2_set_margins(v);
  escp2_set_form_factor(v);
}

void
stpi_escp2_deinit_printer(stp_vars_t *v)
{
  escp2_privdata_t *pd = get_privdata(v);
  stp_puts("\033@", v);	/* ESC/P2 reset */
  if (pd->advanced_command_set || pd->input_slot)
    {
      stp_send_command(v, "\033(R", "bcs", 0, "REMOTE1");
      if (pd->input_slot && pd->input_slot->deinit_sequence.bytes)
	stp_zfwrite(pd->input_slot->deinit_sequence.data,
		    pd->input_slot->deinit_sequence.bytes, 1, v);
      /* Load settings from NVRAM */
      stp_send_command(v, "LD", "b");

      /* Magic deinit sequence reported by Simone Falsini */
      if (pd->deinit_sequence)
	stp_zfwrite(pd->deinit_sequence->data, pd->deinit_sequence->bytes,
		    1, v);
      /* Exit remote mode */
      stp_send_command(v, "\033", "ccc", 0, 0, 0);
    }
}

void
stpi_escp2_flush_pass(stp_vars_t *v, int passno, int vertical_subpass)
{
  int j;
  escp2_privdata_t *pd = get_privdata(v);
  stp_lineoff_t *lineoffs = stp_get_lineoffsets_by_pass(v, passno);
  stp_lineactive_t *lineactive = stp_get_lineactive_by_pass(v, passno);
  const stp_linebufs_t *bufs = stp_get_linebases_by_pass(v, passno);
  stp_pass_t *pass = stp_get_pass_by_pass(v, passno);
  stp_linecount_t *linecount = stp_get_linecount_by_pass(v, passno);
  int minlines = pd->min_nozzles;

  for (j = 0; j < pd->channels_in_use; j++)
    {
      if (lineactive[0].v[j] > 0)
	{
	  int nlines = linecount[0].v[j];
	  int extralines = 0;
	  if (nlines < minlines)
	    {
	      extralines = minlines - nlines;
	      nlines = minlines;
	    }
	  set_vertical_position(v, pass);
	  set_color(v, pass, j);
	  set_horizontal_position(v, pass, vertical_subpass);
	  send_print_command(v, pass, j, nlines);

	  /*
	   * Send the data
	   */
	  stp_zfwrite((const char *)bufs[0].v[j], lineoffs[0].v[j], 1, v);
	  if (extralines)
	    send_extra_data(v, extralines);
	  stp_send_command(v, "\r", "");
	  pd->printed_something = 1;
	}
      lineoffs[0].v[j] = 0;
      linecount[0].v[j] = 0;
    }
}

void
stpi_escp2_terminate_page(stp_vars_t *v)
{
  escp2_privdata_t *pd = get_privdata(v);
  if (!pd->input_slot ||
      pd->input_slot->roll_feed_cut_flags != ROLL_FEED_DONT_EJECT)
    {
      if (!pd->printed_something)
	stp_send_command(v, "\n", "");
      stp_send_command(v, "\f", "");	/* Eject page */
    }
}

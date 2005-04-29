/*
 * "$Id: channel.c,v 1.1.1.2 2004/12/22 23:49:37 jlovell Exp $"
 *
 *   Dither routine entrypoints
 *
 *   Copyright 2003 Robert Krawitz (rlk@alum.mit.edu)
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
 *
 * Revision History:
 *
 *   See ChangeLog
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <gimp-print/gimp-print.h>
#include "gimp-print-internal.h"
#include <gimp-print/gimp-print-intl-internal.h>
#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif
#include <math.h>
#include <string.h>

#ifdef __GNUC__
#define inline __inline__
#endif

typedef struct
{
  double value;
  double lower;
  double upper;
  double cutoff;
  unsigned short s_density;
} stpi_subchannel_t;

typedef struct
{
  unsigned subchannel_count;
  stpi_subchannel_t *sc;
  unsigned short *lut;
} stpi_channel_t;

typedef struct
{
  unsigned channel_count;
  unsigned total_channels;
  unsigned input_channels;
  size_t width;
  int initialized;
  unsigned ink_limit;
  unsigned max_density;
  stpi_channel_t *c;
  unsigned short *input_data;
  unsigned short *data;
  int black_channel;
} stpi_channel_group_t;


static void
clear_a_channel(stpi_channel_group_t *cg, int channel)
{
  if (channel < cg->channel_count)
    {
      STP_SAFE_FREE(cg->c[channel].sc);
      STP_SAFE_FREE(cg->c[channel].lut);
      cg->c[channel].subchannel_count = 0;
    }
}

static void
stpi_channel_clear(void *vc)
{
  stpi_channel_group_t *cg = (stpi_channel_group_t *) vc;
  int i;
  if (cg->channel_count > 0)
    for (i = 0; i < cg->channel_count; i++)
      clear_a_channel(cg, i);
  if (cg->data != cg->input_data)
    STP_SAFE_FREE(cg->data);
  STP_SAFE_FREE(cg->input_data);
  STP_SAFE_FREE(cg->c);
  cg->channel_count = 0;
  cg->total_channels = 0;
  cg->input_channels = 0;
  cg->initialized = 0;
}

void
stp_channel_reset(stp_vars_t *v)
{
  stpi_channel_group_t *cg =
    ((stpi_channel_group_t *) stp_get_component_data(v, "Channel"));
  if (cg)
    stpi_channel_clear(cg);
}

void
stp_channel_reset_channel(stp_vars_t *v, int channel)
{
  stpi_channel_group_t *cg =
    ((stpi_channel_group_t *) stp_get_component_data(v, "Channel"));
  if (cg)
    clear_a_channel(cg, channel);
}

static void
stpi_channel_free(void *vc)
{
  stpi_channel_clear(vc);
  stp_free(vc);
}

static stpi_subchannel_t *
get_channel(stp_vars_t *v, unsigned channel, unsigned subchannel)
{
  stpi_channel_group_t *cg =
    ((stpi_channel_group_t *) stp_get_component_data(v, "Channel"));
  if (!cg)
    return NULL;
  if (channel >= cg->channel_count)
    return NULL;
  if (subchannel >= cg->c[channel].subchannel_count)
    return NULL;
  return &(cg->c[channel].sc[subchannel]);
}

void
stp_channel_add(stp_vars_t *v, unsigned channel, unsigned subchannel,
		double value)
{
  stpi_channel_group_t *cg =
    ((stpi_channel_group_t *) stp_get_component_data(v, "Channel"));
  stpi_channel_t *chan;
  if (!cg)
    {
      cg = stp_zalloc(sizeof(stpi_channel_group_t));
      cg->black_channel = -1;
      stp_allocate_component_data(v, "Channel", NULL, stpi_channel_free, cg);
    }
  if (channel >= cg->channel_count)
    {
      unsigned oc = cg->channel_count;
      cg->c = stp_realloc(cg->c, sizeof(stpi_channel_t) * (channel + 1));
      memset(cg->c + oc, 0, sizeof(stpi_channel_t) * (channel + 1 - oc));
      if (channel >= cg->channel_count)
	cg->channel_count = channel + 1;
    }
  chan = cg->c + channel;
  if (subchannel >= chan->subchannel_count)
    {
      unsigned oc = chan->subchannel_count;
      chan->sc =
	stp_realloc(chan->sc, sizeof(stpi_subchannel_t) * (subchannel + 1));
      (void) memset
	(chan->sc + oc, 0, sizeof(stpi_subchannel_t) * (subchannel + 1 - oc));
      chan->sc[subchannel].value = value;
      if (subchannel >= chan->subchannel_count)
	chan->subchannel_count = subchannel + 1;
    }
  chan->sc[subchannel].value = value;
  chan->sc[subchannel].s_density = 65535;
  chan->sc[subchannel].cutoff = 0.75;
}

void
stp_channel_set_density_adjustment(stp_vars_t *v, int color, int subchannel,
				   double adjustment)
{
  stpi_subchannel_t *sch = get_channel(v, color, subchannel);
  if ((strcmp(stp_get_string_parameter(v, "STPIOutputType"), "Raw") == 0 &&
       strcmp(stp_get_string_parameter(v, "ColorCorrection"), "None") == 0) ||
      strcmp(stp_get_string_parameter(v, "ColorCorrection"), "Raw") == 0 ||
      strcmp(stp_get_string_parameter(v, "ColorCorrection"), "Predithered") == 0)
    {
      stp_dprintf(STP_DBG_INK, v,
		  "Ignoring channel_density channel %d subchannel %d adjustment %f\n",
		  color, subchannel, adjustment);
    }
  else
    {
      stp_dprintf(STP_DBG_INK, v,
		  "channel_density channel %d subchannel %d adjustment %f\n",
		  color, subchannel, adjustment);
      if (sch && adjustment >= 0 && adjustment <= 1)
	sch->s_density = adjustment * 65535;
    }
}

void
stp_channel_set_ink_limit(stp_vars_t *v, double limit)
{
  stpi_channel_group_t *cg =
    ((stpi_channel_group_t *) stp_get_component_data(v, "Channel"));
  stp_dprintf(STP_DBG_INK, v, "ink_limit %f\n", limit);
  if (limit > 0)
    cg->ink_limit = 65535 * limit;
}

void
stp_channel_set_black_channel(stp_vars_t *v, int channel)
{
  stpi_channel_group_t *cg =
    ((stpi_channel_group_t *) stp_get_component_data(v, "Channel"));
  stp_dprintf(STP_DBG_INK, v, "black_channel %d\n", channel);
  cg->black_channel = channel;
}

void
stp_channel_set_cutoff_adjustment(stp_vars_t *v, int color, int subchannel,
				  double adjustment)
{
  stpi_subchannel_t *sch = get_channel(v, color, subchannel);
  stp_dprintf(STP_DBG_INK, v,
	      "channel_cutoff channel %d subchannel %d adjustment %f\n",
	      color, subchannel, adjustment);
  if (sch && adjustment >= 0)
    sch->cutoff = adjustment;
}

static int
input_needs_splitting(const stp_vars_t *v)
{
  const stpi_channel_group_t *cg =
    ((const stpi_channel_group_t *) stp_get_component_data(v, "Channel"));
#if 1
  return cg->total_channels != cg->input_channels;
#else
  int i;
  if (!cg || cg->channel_count <= 0)
    return 0;
  for (i = 0; i < cg->channel_count; i++)
    {
      if (cg->c[i].subchannel_count > 1)
	return 1;
    }
  return 0;
#endif
}

void
stp_channel_initialize(stp_vars_t *v, stp_image_t *image,
		       int input_channel_count)
{
  stpi_channel_group_t *cg =
    ((stpi_channel_group_t *) stp_get_component_data(v, "Channel"));
  int width = stp_image_width(image);
  int i, j, k;
  if (!cg)
    {
      cg = stp_zalloc(sizeof(stpi_channel_group_t));
      cg->black_channel = -1;
      stp_allocate_component_data(v, "Channel", NULL, stpi_channel_free, cg);
    }
  if (cg->initialized)
    return;
  cg->initialized = 1;
  cg->max_density = 0;
  if (cg->black_channel < -1 || cg->black_channel >= cg->channel_count)
    cg->black_channel = -1;
  for (i = 0; i < cg->channel_count; i++)
    {
      stpi_channel_t *c = &(cg->c[i]);
      int sc = c->subchannel_count;
      if (sc > 1)
	{
	  int val = 0;
	  int next_breakpoint;
	  c->lut = stp_zalloc(sizeof(unsigned short) * sc * 65536);
	  next_breakpoint = c->sc[0].value * 65535 * c->sc[0].cutoff;
	  if (next_breakpoint > 65535)
	    next_breakpoint = 65535;
	  while (val <= next_breakpoint)
	    {
	      int value = (int) ((double) val / c->sc[0].value);
	      c->lut[val * sc + sc - 1] = value;
	      val++;
	    }

	  for (k = 0; k < sc - 1; k++)
	    {
	      double this_val = c->sc[k].value;
	      double next_val = c->sc[k + 1].value;
	      double this_cutoff = c->sc[k].cutoff;
	      double next_cutoff = c->sc[k + 1].cutoff;
	      int range;
	      int base = val;
	      double cutoff = sqrt(this_cutoff * next_cutoff);
	      next_breakpoint = next_val * 65535 * cutoff;
	      if (next_breakpoint > 65535)
		next_breakpoint = 65535;
	      range = next_breakpoint - val;
	      while (val <= next_breakpoint)
		{
		  double where = ((double) val - base) / (double) range;
		  double lower_val = base * (1.0 - where);
		  double lower_amount = lower_val / this_val;
		  double upper_amount = (val - lower_val) / next_val;
		  c->lut[val * sc + sc - k - 2] = upper_amount;
		  c->lut[val * sc + sc - k - 1] = lower_amount;
		  val++;
		}
	    }
	  while (val <= 65535)
	    {
	      c->lut[val * sc] = val / c->sc[sc - 1].value;
	      val++;
	    }
	}
      cg->total_channels += c->subchannel_count;
      for (j = 0; j < c->subchannel_count; j++)
	cg->max_density += c->sc[j].s_density;
    }
  cg->input_channels = input_channel_count;
  cg->width = width;
  cg->data = stp_malloc(sizeof(unsigned short) * cg->total_channels * width);
  if (!input_needs_splitting(v))
    {
      cg->input_data = cg->data;
      return;
    }
  cg->input_data =
    stp_malloc(sizeof(unsigned short) * cg->input_channels * width);
}

static void
clear_channel(unsigned short *data, unsigned width, unsigned depth)
{
  int i;
  width *= depth;
  for (i = 0; i < width; i += depth)
    data[i] = 0;
}

static int
scale_channel(unsigned short *data, unsigned width, unsigned depth,
	      unsigned short density)
{
  int i;
  int retval = 0;
  unsigned short previous_data = 0;
  unsigned short previous_value = 0;
  if (density > 65535)
    density = 65535;
  width *= depth;
  for (i = 0; i < width; i += depth)
    {
      if (data[i] == previous_data)
	data[i] = previous_value;
      else if (data[i] == (unsigned short) 65535)
	{
	  data[i] = density;
	  retval = 1;
	}
      else if (data[i] > 0)
	{
	  unsigned short tval = (32767u + data[i] * density) / 65535u;
	  previous_data = data[i];
	  if (tval)
	    retval = 1;
	  previous_value = (unsigned short) tval;
	  data[i] = (unsigned short) tval;
	}
    }
  return retval;
}

static int
scan_channel(unsigned short *data, unsigned width, unsigned depth)
{
  int i;
  width *= depth;
  for (i = 0; i < width; i += depth)
    {
      if (data[i])
	return 1;
    }
  return 0;
}

static inline unsigned
ink_sum(const unsigned short *data, int total_channels)
{
  int j;
  unsigned total_ink = 0;
  for (j = 0; j < total_channels; j++)
    total_ink += data[j];
  return total_ink;
}

static int
limit_ink(const stp_vars_t *v)
{
  int i;
  int retval = 0;
  stpi_channel_group_t *cg =
    ((stpi_channel_group_t *) stp_get_component_data(v, "Channel"));
  unsigned short *ptr = cg->data;
  if (cg->ink_limit == 0 || cg->ink_limit >= cg->max_density)
    return 0;
  for (i = 0; i < cg->width; i++)
    {
      int total_ink = ink_sum(ptr, cg->total_channels);
      if (total_ink > cg->ink_limit) /* Need to limit ink? */
	{
	  int j;
	  /*
	   * FIXME we probably should first try to convert light ink to dark
	   */
	  double ratio = (double) cg->ink_limit / (double) total_ink;
	  for (j = 0; j < cg->total_channels; j++)
	    ptr[j] *= ratio;
	  retval = 1;
	}
      ptr += cg->total_channels;
   }
  return retval;
}

static inline int
mem_eq(const unsigned short *i1, const unsigned short *i2, int count)
{
  int i;
  for (i = 0; i < count; i++)
    if (i1[i] != i2[i])
      return 0;
  return 1;
}

void
stp_channel_convert(const stp_vars_t *v, unsigned *zero_mask)
{
  stpi_channel_group_t *cg =
    ((stpi_channel_group_t *) stp_get_component_data(v, "Channel"));
  int i, j, k;
  int nz[STP_CHANNEL_LIMIT];
  int outbytes = cg->total_channels * sizeof(unsigned short);
  const unsigned short *input_cache = NULL;
  const unsigned short *output_cache = NULL;
  unsigned black_value = 0;
  unsigned l_val = 0;
  unsigned i_val = 0;
  unsigned o_val = 0;
  unsigned offset = 0;
  unsigned virtual_black = 0;
  memset(nz, 0, sizeof(nz));
  if (input_needs_splitting(v))
    {
      const unsigned short *input = cg->input_data;
      unsigned short *output = cg->data;
      const unsigned short *o_output;
      for (i = 0; i < cg->width; i++)
	{
	  int zero_ptr = 0;
	  if (input_cache && mem_eq(input_cache, input, cg->input_channels))
	    {
	      memcpy(output, output_cache, outbytes);
	      input += cg->input_channels;
	      output += cg->total_channels;
	    }
	  else
	    {
	      input_cache = input;
	      black_value = 0;
	      o_output = output;
	      if (cg->black_channel >= 0)
		black_value = input[cg->black_channel];
	      virtual_black = 65535;
	      for (j = 0; j < cg->channel_count; j++)
		{
		  if (input[j] < virtual_black && j != cg->black_channel)
		    virtual_black = input[j];
		}
	      black_value += virtual_black / 4;
	      for (j = 0; j < cg->channel_count; j++)
		{
		  stpi_channel_t *c = &(cg->c[j]);
		  int s_count = c->subchannel_count;
		  if (s_count >= 1)
		    {
		      i_val = *input++;
		      if (i_val == 0)
			{
			  for (k = 0; k < s_count; k++)
			    *(output++) = 0;
			}
		      else if (s_count == 1)
			{
			  if (c->sc[0].s_density < 65535)
			    i_val = i_val * c->sc[0].s_density / 65535;
			  nz[zero_ptr++] |= *(output++) = i_val;
			}
		      else
			{
			  l_val = i_val;
			  if (i_val > 0 && black_value &&
			      j != cg->black_channel)
			    {
			      l_val += black_value;
			      if (l_val > 65535)
				l_val = 65535;
			    }
			  offset = l_val * s_count;
			  for (k = 0; k < s_count; k++)
			    {
			      if (c->sc[k].s_density > 0)
				{
				  o_val = c->lut[offset + k];
				  if (i_val != l_val)
				    o_val = o_val * i_val / l_val;
				  if (c->sc[k].s_density < 65535)
				    o_val = o_val * c->sc[k].s_density / 65535;
				}
			      else
				o_val = 0;
			      *output++ = o_val;
			      nz[zero_ptr++] |= o_val;
			    }
			}
		    }
		}
	      output_cache = o_output;
	    }
	}
      if (zero_mask)
	{
	  *zero_mask = 0;
	  for (i = 0; i < cg->total_channels; i++)
	    if (!nz[i])
	      *zero_mask |= 1 << i;
	}
    }
  else
    {
      int physical_channel = 0;
      if (zero_mask)
	*zero_mask = 0;
      for (i = 0; i < cg->channel_count; i++)
	{
	  stpi_channel_t *ch = &(cg->c[i]);
	  if (ch->subchannel_count > 0)
	    for (j = 0; j < ch->subchannel_count; j++)
	      {
		stpi_subchannel_t *sch = &(ch->sc[j]);
		unsigned density = sch->s_density;
		unsigned short *output = cg->data + physical_channel;
		if (density == 0)
		  {
		    clear_channel(output, cg->width, cg->total_channels);
		    if (zero_mask)
		      *zero_mask |= 1 << physical_channel;
		  }
		else if (density != 65535)
		  {
		    if (scale_channel(output, cg->width, cg->total_channels,
				      density) == 0)
		      if (zero_mask)
			*zero_mask |= 1 << physical_channel;
		  }
		else if (zero_mask)
		  {
		    if (scan_channel(output, cg->width, cg->total_channels) == 0)
		      *zero_mask |= 1 << physical_channel;
		  }
		physical_channel++;
	      }
	}
    }
  (void) limit_ink(v);
}

unsigned short *
stp_channel_get_input(const stp_vars_t *v)
{
  stpi_channel_group_t *cg =
    ((stpi_channel_group_t *) stp_get_component_data(v, "Channel"));
  return (unsigned short *) cg->input_data;
}

unsigned short *
stp_channel_get_output(const stp_vars_t *v)
{
  stpi_channel_group_t *cg =
    ((stpi_channel_group_t *) stp_get_component_data(v, "Channel"));
  return cg->data;
}

/*
 * "$Id: print-color.c,v 1.1.1.1 2003/01/27 19:05:32 jlovell Exp $"
 *
 *   Print plug-in color management for the GIMP.
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
#include <math.h>
#include <limits.h>

#ifdef __GNUC__
#define inline __inline__
#endif

typedef struct
{
  unsigned steps;
  unsigned short *composite;
  unsigned short *red;
  unsigned short *green;
  unsigned short *blue;
  unsigned shiftval;
  unsigned bin_size;
  unsigned bin_shift;
} lut_t;

/*
 * RGB to grayscale luminance constants...
 */

#define LUM_RED		31
#define LUM_GREEN	61
#define LUM_BLUE	8

/* rgb/hsl conversions taken from Gimp common/autostretch_hsv.c */

#define FMAX(a, b) ((a) > (b) ? (a) : (b))
#define FMIN(a, b) ((a) < (b) ? (a) : (b))


static inline void
calc_rgb_to_hsl(unsigned short *rgb, double *hue, double *sat,
		double *lightness)
{
  double red, green, blue;
  double h, s, l;
  double min, max;
  double delta;
  int maxval;

  red   = rgb[0] / 65535.0;
  green = rgb[1] / 65535.0;
  blue  = rgb[2] / 65535.0;

  if (red > green)
    {
      if (red > blue)
	{
	  max = red;
	  maxval = 0;
	}
      else
	{
	  max = blue;
	  maxval = 2;
	}
      min = FMIN(green, blue);
    }
  else
    {
      if (green > blue)
	{
	  max = green;
	  maxval = 1;
	}
      else
	{
	  max = blue;
	  maxval = 2;
	}
      min = FMIN(red, blue);
    }

  l = (max + min) / 2.0;
  delta = max - min;

  if (delta < .000001)	/* Suggested by Eugene Anikin <eugene@anikin.com> */
    {
      s = 0.0;
      h = 0.0;
    }
  else
    {
      if (l <= .5)
	s = delta / (max + min);
      else
	s = delta / (2 - max - min);

      if (maxval == 0)
	h = (green - blue) / delta;
      else if (maxval == 1)
	h = 2 + (blue - red) / delta;
      else
	h = 4 + (red - green) / delta;

      if (h < 0.0)
	h += 6.0;
      else if (h > 6.0)
	h -= 6.0;
    }

  *hue = h;
  *sat = s;
  *lightness = l;
}

static inline double
hsl_value(double n1, double n2, double hue)
{
  if (hue < 0)
    hue += 6.0;
  else if (hue > 6)
    hue -= 6.0;
  if (hue < 1.0)
    return (n1 + (n2 - n1) * hue);
  else if (hue < 3.0)
    return (n2);
  else if (hue < 4.0)
    return (n1 + (n2 - n1) * (4.0 - hue));
  else
    return (n1);
}

static inline void
calc_hsl_to_rgb(unsigned short *rgb, double h, double s, double l)
{
  if (s < .0000001)
    {
      if (l > 1)
	l = 1;
      else if (l < 0)
	l = 0;
      rgb[0] = l * 65535;
      rgb[1] = l * 65535;
      rgb[2] = l * 65535;
    }
  else
    {
      double m1, m2;
      double h1, h2;
      h1 = h + 2;
      h2 = h - 2;

      if (l < .5)
	m2 = l * (1 + s);
      else
	m2 = l + s - (l * s);
      m1 = (l * 2) - m2;
      rgb[0] = 65535 * hsl_value(m1, m2, h1);
      rgb[1] = 65535 * hsl_value(m1, m2, h);
      rgb[2] = 65535 * hsl_value(m1, m2, h2);
    }
}

static inline void
update_cmyk(unsigned short *rgb)
{
  int c = 65535 - rgb[0];
  int m = 65535 - rgb[1];
  int y = 65535 - rgb[2];
  int nc, nm, ny;
  int k;
  if (c == m && c == y)
    return;
  k = FMIN(FMIN(c, m), y);

  /*
   * This is an attempt to achieve better color balance.  The goal
   * is to weaken the pure cyan, magenta, and yellow and strengthen
   * pure red, green, and blue.
   *
   * We also don't want S=1 V=1 cyan to be 100% cyan; it's simply
   * too dark.
   */

  nc = (c * 3 + FMIN(c, FMAX(m, y)) * 4 + FMAX(m, y) * 0 + k) / 8;
  nm = (m * 3 + FMIN(m, FMAX(c, y)) * 4 + FMAX(c, y) * 0 + k) / 8;
  ny = (y * 3 + FMIN(y, FMAX(c, m)) * 4 + FMAX(c, m) * 0 + k) / 8;

  /*
   * Make sure we didn't go overboard.  We don't want to go too
   * close to white unnecessarily.
   */
  nc = c + (nc - c) / 3;
  nm = m + (nm - m) / 3;
  ny = y + (ny - y) / 3;

  if (nc > 65535)
    nc = 65535;
  if (nm > 65535)
    nm = 65535;
  if (ny > 65535)
    ny = 65535;

  rgb[0] = 65535 - nc;
  rgb[1] = 65535 - nm;
  rgb[2] = 65535 - ny;
}

/*
 * A lot of this stuff needs to be factored out of here
 */
static inline unsigned short
lookup_value(unsigned short value, int lut_size, const unsigned short *lut,
	     unsigned shiftval, unsigned bin_size, unsigned bin_shift)
{
  unsigned subrange;
  unsigned remainder;
  unsigned below;
  unsigned above;
  if (lut_size == 65536)
    return lut[value];
  subrange = value >> bin_shift;
  remainder = value & (bin_size - 1);
  below = lut[subrange];
  if (remainder == 0)
    return below;
  if (subrange == (bin_size - 1))
    above = lut[subrange];
  else
    above = lut[subrange + 1];
  if (above == below)
    return above;
  else
    return below + (((above - below) * remainder) >> bin_shift);
}

/*
 * 'gray_to_gray()' - Convert grayscale image data to grayscale (brightness
 *                    adjusted).
 */

static void
gray_to_gray(const stp_vars_t vars,
	     const unsigned char *grayin,
	     unsigned short *grayout,
	     int *zero_mask,
	     int width,
	     int bpp,
	     const unsigned char *cmap,
	     const double *hue_map,
	     const double *lum_map,
	     const double *sat_map)
{
  int i0 = -1;
  int o0 = 0;
  int nz = 0;
  lut_t *lut = (lut_t *)(stp_get_lut(vars));
  double density = stp_get_density(vars);
  if (width <= 0)
    return;
  while (width)
    {
      if (i0 != grayin[0])
	{
	  i0 = grayin[0];
	  o0 = lut->composite[i0];
	  if (density != 1.0)
	    o0 = .5 + (o0 * density);
	  nz |= o0;
	}
      grayout[0] = o0;
      grayin ++;
      grayout ++;
      width --;
    }
  if (zero_mask)
    *zero_mask = nz ? 0 : 1;
}

static void
gray_alpha_to_gray(const stp_vars_t vars,
		   const unsigned char *grayin,
		   unsigned short *grayout,
		   int *zero_mask,
		   int width,
		   int bpp,
		   const unsigned char *cmap,
		   const double *hue_map,
		   const double *lum_map,
		   const double *sat_map)
{
  int i0 = -1;
  int i1 = -1;
  int o0 = 0;
  int nz = 0;
  lut_t *lut = (lut_t *)(stp_get_lut(vars));
  double density = stp_get_density(vars);
  if (width <= 0)
    return;
  while (width)
    {
      if (i0 != grayin[0] || i1 != grayin[1])
	{
	  i0 = grayin[0];
	  i1 = grayin[1];
	  o0 = lut->composite[i0 * i1 / 255 + 255 - i1];
	  if (density != 1.0)
	    o0 = .5 + (o0 * density);
	  nz |= o0;
	}
      grayout[0] = o0;
      grayin += 2;
      grayout ++;
      width --;
    }
  if (zero_mask)
    *zero_mask = nz ? 0 : 1;
}

static void
gray_to_monochrome(const stp_vars_t vars,
		   const unsigned char *grayin,
		   unsigned short *grayout,
		   int *zero_mask,
		   int width,
		   int bpp,
		   const unsigned char *cmap,
		   const double *hue_map,
		   const double *lum_map,
		   const double *sat_map)
{
  int i0 = -1;
  int o0 = 0;
  int nz = 0;
  lut_t *lut = (lut_t *)(stp_get_lut(vars));
  if (width <= 0)
    return;
  while (width)
    {
      if (i0 != grayin[0])
	{
	  i0 = grayin[0];
	  o0 = lut->composite[grayin[0]];
	  if (o0 < 32768)
	    o0 = 0;
	  else
	    o0  = 65535;
	  nz |= o0;
	}
      grayout[0] = o0;
      grayin ++;
      grayout ++;
      width --;
    }
  if (zero_mask)
    *zero_mask = nz ? 0 : 1;
}

static void
gray_alpha_to_monochrome(const stp_vars_t vars,
			 const unsigned char *grayin,
			 unsigned short *grayout,
			 int *zero_mask,
			 int width,
			 int bpp,
			 const unsigned char *cmap,
			 const double *hue_map,
			 const double *lum_map,
			 const double *sat_map)
{
  int i0 = -1;
  int i1 = -1;
  int o0 = 0;
  int nz = 0;
  lut_t *lut = (lut_t *)(stp_get_lut(vars));
  if (width <= 0)
    return;
  while (width)
    {
      if (i0 != grayin[0] || i1 != grayin[1])
	{
	  i0 = grayin[0];
	  i1 = grayin[1];
	  o0 = lut->composite[i0 * i1 / 255 + 255 - i1];
	  if (o0 < 32768)
	    o0 = 0;
	  else
	    o0  = 65535;
	  nz |= o0;
	}
      grayout[0] = o0;
      grayin += 2;
      grayout ++;
      width --;
    }
  if (zero_mask)
    *zero_mask = nz ? 0 : 1;
}

/*
 * 'indexed_to_gray()' - Convert indexed image data to grayscale.
 */

static void
indexed_to_gray(const stp_vars_t vars,
		const unsigned char *indexed,
		unsigned short *gray,
		int *zero_mask,
		int width,
		int bpp,
		const unsigned char *cmap,
		const double *hue_map,
		const double *lum_map,
		const double *sat_map)
{
  int i0 = -1;
  int o0 = 0;
  int i;
  int nz = 0;
  lut_t *lut = (lut_t *)(stp_get_lut(vars));
  unsigned char	gray_cmap[256];		/* Grayscale colormap */
  double density = stp_get_density(vars);
  if (width <= 0)
    return;

  /* Really should precompute this silly thing... */
  for (i = 0; i < 256; i ++, cmap += 3)
    gray_cmap[i] = (cmap[0] * LUM_RED +
		    cmap[1] * LUM_GREEN +
		    cmap[2] * LUM_BLUE) / 100;
  while (width)
    {
      if (i0 != indexed[0])
	{
	  i0 = indexed[0];
	  o0 = lut->composite[gray_cmap[i0]];
	  if (density != 1.0)
	    o0 = .5 + (o0 * density);
	  nz |= o0;
	}
      gray[0] = o0;
      gray ++;
      indexed ++;
      width --;
    }
  if (zero_mask)
    *zero_mask = nz ? 0 : 1;
}

static void
indexed_alpha_to_gray(const stp_vars_t vars,
		      const unsigned char *indexed,
		      unsigned short *gray,
		      int *zero_mask,
		      int width,
		      int bpp,
		      const unsigned char *cmap,
		      const double *hue_map,
		      const double *lum_map,
		      const double *sat_map)
{
  int i0 = -1;
  int i1 = -1;
  int o0 = 0;
  int nz = 0;
  int i;
  lut_t *lut = (lut_t *)(stp_get_lut(vars));
  unsigned char	gray_cmap[256];		/* Grayscale colormap */
  double density = stp_get_density(vars);
  if (width <= 0)
    return;

  /* Really should precompute this silly thing... */
  for (i = 0; i < 256; i ++, cmap += 3)
    gray_cmap[i] = (cmap[0] * LUM_RED +
		    cmap[1] * LUM_GREEN +
		    cmap[2] * LUM_BLUE) / 100;

  while (width)
    {
      if (i0 != indexed[0] || i1 != indexed[1])
	{
	  i0 = indexed[0];
	  i1 = indexed[1];
	  o0 = lut->composite[gray_cmap[i0 * i1 / 255] + 255 - i1];
	  if (density != 1.0)
	    o0 = .5 + (o0 * density);
	  nz |= o0;
	}
      gray[0] = o0;
      gray ++;
      indexed += 2;
      width --;
    }
  if (zero_mask)
    *zero_mask = nz ? 0 : 1;
}

static void
indexed_to_monochrome(const stp_vars_t vars,
		      const unsigned char *indexed,
		      unsigned short *gray,
		      int *zero_mask,
		      int width,
		      int bpp,
		      const unsigned char *cmap,
		      const double *hue_map,
		      const double *lum_map,
		      const double *sat_map)
{
  int i0 = -1;
  int o0 = 0;
  int i;
  int nz = 0;
  lut_t *lut = (lut_t *)(stp_get_lut(vars));
  unsigned char	gray_cmap[256];		/* Grayscale colormap */
  if (width <= 0)
    return;

  /* Really should precompute this silly thing... */
  for (i = 0; i < 256; i ++, cmap += 3)
    gray_cmap[i] = (cmap[0] * LUM_RED +
		    cmap[1] * LUM_GREEN +
		    cmap[2] * LUM_BLUE) / 100;

  while (width)
    {
      if (i0 != indexed[0])
	{
	  i0 = indexed[0];
	  o0 = lut->composite[gray_cmap[i0]];
	  if (o0 < 32768)
	    o0 = 0;
	  else
	    o0  = 65535;
	  nz |= o0;
	}
      gray[0] = o0;
      indexed ++;
      gray ++;
      width --;
    }
  if (zero_mask)
    *zero_mask = nz ? 0 : 1;
}

static void
indexed_alpha_to_monochrome(const stp_vars_t vars,
			    const unsigned char *indexed,
			    unsigned short *gray,
			    int *zero_mask,
			    int width,
			    int bpp,
			    const unsigned char *cmap,
			    const double *hue_map,
			    const double *lum_map,
			    const double *sat_map)
{
  int i0 = -1;
  int i1 = -1;
  int o0 = 0;
  int nz = 0;
  int i;
  lut_t *lut = (lut_t *)(stp_get_lut(vars));
  unsigned char	gray_cmap[256];		/* Grayscale colormap */
  if (width <= 0)
    return;

  /* Really should precompute this silly thing... */
  for (i = 0; i < 256; i ++, cmap += 3)
    gray_cmap[i] = (cmap[0] * LUM_RED +
		    cmap[1] * LUM_GREEN +
		    cmap[2] * LUM_BLUE) / 100;
  while (width)
    {
      if (i0 != indexed[0] || i1 != indexed[1])
	{
	  i0 = indexed[0];
	  i1 = indexed[1];
	  o0 = lut->composite[gray_cmap[i0 * i1 / 255] + 255 - i1];
	  if (o0 < 32768)
	    o0 = 0;
	  else
	    o0  = 65535;
	  nz |= o0;
	}
      gray[0] = o0;
      indexed += 2;
      gray ++;
      width --;
    }
  if (zero_mask)
    *zero_mask = nz ? 0 : 1;
}

/*
 * 'rgb_to_gray()' - Convert RGB image data to grayscale.
 */

static void
rgb_to_gray(const stp_vars_t vars,
	    const unsigned char *rgb,
	    unsigned short *gray,
	    int *zero_mask,
	    int width,
	    int bpp,
	    const unsigned char *cmap,
	    const double *hue_map,
	    const double *lum_map,
	    const double *sat_map)
{
  int i0 = -1;
  int i1 = -1;
  int i2 = -1;
  int o0 = 0;
  int nz = 0;
  lut_t *lut = (lut_t *)(stp_get_lut(vars));
  double density = stp_get_density(vars);
  if (width <= 0)
    return;
  while (width)
    {
      if (i0 != rgb[0] || i1 != rgb[1] || i2 != rgb[2])
	{
	  i0 = rgb[0];
	  i1 = rgb[1];
	  i2 = rgb[2];
	  o0 = lut->composite[(i0 * LUM_RED + i1 * LUM_GREEN +
			       i2 * LUM_BLUE) / 100];
	  if (density != 1.0)
	    o0 = .5 + (o0 * density);
	  nz |= o0;
	}
      gray[0] = o0;
      rgb += 3;
      gray ++;
      width --;
    }
  if (zero_mask)
    *zero_mask = nz ? 0 : 1;
}

static void
rgb_alpha_to_gray(const stp_vars_t vars,
		  const unsigned char *rgb,
		  unsigned short *gray,
		  int *zero_mask,
		  int width,
		  int bpp,
		  const unsigned char *cmap,
		  const double *hue_map,
		  const double *lum_map,
		  const double *sat_map)
{
  int i0 = -1;
  int i1 = -1;
  int i2 = -1;
  int i3 = -1;
  int o0 = 0;
  int nz = 0;
  lut_t *lut = (lut_t *)(stp_get_lut(vars));
  double density = stp_get_density(vars);
  if (width <= 0)
    return;
  while (width)
    {
      if (i0 != rgb[0] || i1 != rgb[1] || i2 != rgb[2] || i3 != rgb[3])
	{
	  i0 = rgb[0];
	  i1 = rgb[1];
	  i2 = rgb[2];
	  i3 = rgb[3];
	  o0= lut->composite[((i0 * LUM_RED +
				     i1 * LUM_GREEN +
				     i2 * LUM_BLUE) *
				    i3 / 25500 + 255 - i3)];
	  if (density != 1.0)
	    o0 = .5 + (o0 * density);
	  nz |= o0;
	}
      gray[0] = o0;
      rgb += 4;
      gray ++;
      width --;
    }
  if (zero_mask)
    *zero_mask = nz ? 0 : 1;
}

static void
rgb_to_monochrome(const stp_vars_t vars,
		  const unsigned char *rgb,
		  unsigned short *gray,
		  int *zero_mask,
		  int width,
		  int bpp,
		  const unsigned char *cmap,
		  const double *hue_map,
		  const double *lum_map,
		  const double *sat_map)
{
  int i0 = -1;
  int i1 = -1;
  int i2 = -1;
  int o0 = 0;
  int nz = 0;
  lut_t *lut = (lut_t *)(stp_get_lut(vars));
  if (width <= 0)
    return;
  while (width)
    {
      if (i0 != rgb[0] || i1 != rgb[1] || i2 != rgb[2])
	{
	  i0 = rgb[0];
	  i1 = rgb[1];
	  i2 = rgb[2];
	  o0 = lut->composite[(i0 * LUM_RED + i1 * LUM_GREEN +
			       i2 * LUM_BLUE) / 100];
	  if (o0 < 32768)
	    o0 = 0;
	  else
	    o0 = 65535;
	  nz |= o0;
	}
      gray[0] = o0;
      rgb += 3;
      gray ++;
      width --;
    }
  if (zero_mask)
    *zero_mask = nz ? 0 : 1;
}

static void
rgb_alpha_to_monochrome(const stp_vars_t vars,
			const unsigned char *rgb,
			unsigned short *gray,
			int *zero_mask,
			int width,
			int bpp,
			const unsigned char *cmap,
			const double *hue_map,
			const double *lum_map,
			const double *sat_map)
{
  int i0 = -1;
  int i1 = -1;
  int i2 = -1;
  int i3 = -1;
  int o0 = 0;
  int nz = 0;
  lut_t *lut = (lut_t *)(stp_get_lut(vars));
  if (width <= 0)
    return;
  while (width)
    {
      if (i0 != rgb[0] || i1 != rgb[1] || i2 != rgb[2] || i3 != rgb[3])
	{
	  i0 = rgb[0];
	  i1 = rgb[1];
	  i2 = rgb[2];
	  i3 = rgb[3];
	  o0= lut->composite[((i0 * LUM_RED +
			       i1 * LUM_GREEN +
			       i2 * LUM_BLUE) *
			      i3 / 25500 + 255 - i3)];
	  if (o0 < 32768)
	    o0 = 0;
	  else
	    o0 = 65535;
	  nz |= o0;
	}
      gray[0] = o0;
      rgb += 4;
      gray ++;
      width --;
    }
  if (zero_mask)
    *zero_mask = nz ? 0 : 1;
}

/*
 * 'rgb_to_rgb()' - Convert rgb image data to RGB.
 */

static void
rgb_to_rgb(const stp_vars_t vars,
	   const unsigned char *rgbin,
	   unsigned short *rgbout,
	   int *zero_mask,
	   int width,
	   int bpp,
	   const unsigned char *cmap,
	   const double *hue_map,
	   const double *lum_map,
	   const double *sat_map)
{
  unsigned ld = stp_get_density(vars) * 65536;
  double isat = 1.0;
  double ssat = stp_get_saturation(vars);
  int i0 = -1;
  int i1 = -1;
  int i2 = -1;
  int i3 = -1;
  int o0 = 0;
  int o1 = 0;
  int o2 = 0;
  int nz0 = 0;
  int nz1 = 0;
  int nz2 = 0;
  lut_t *lut = (lut_t *)(stp_get_lut(vars));
  int compute_saturation = ssat <= .99999 || ssat >= 1.00001;
  int split_saturation = ssat > 1.4;
  if (split_saturation)
    ssat = sqrt(ssat);
  if (ssat > 1)
    isat = 1.0 / ssat;
  while (width > 0)
    {
      double h, s, l;
      switch (bpp)
	{
	case 1:
	  /*
	   * No alpha in image, using colormap...
	   */
	  if (i0 == rgbin[0])
	    {
	      rgbout[0] = o0;
	      rgbout[1] = o1;
	      rgbout[2] = o2;
	      goto out;
	    }
	  else
	    {
	      i0 = rgbin[0] * 3;
	      i1 = cmap[i0 + 1];
	      i2 = cmap[i0 + 2];
	      i0 = cmap[i0];
	      rgbout[0] = i0 | (i0 << 8);
	      rgbout[1] = i1 | (i1 << 8);
	      rgbout[2] = i2 | (i2 << 8);
	      i0 = rgbin[0];
	    }
	  break;
	case 2:
	  if (i0 == rgbin[0] && i1 == rgbin[1])
	    {
	      rgbout[0] = o0;
	      rgbout[1] = o1;
	      rgbout[2] = o2;
	      goto out;
	    }
	  else
	    {
	      i0 = rgbin[0] * 3;
	      i3 = rgbin[1];
	      i1 = cmap[i0 + 1] * i3 / 255 + 255 - i3;
	      i2 = cmap[i0 + 2] * i3 / 255 + 255 - i3;
	      i0 = cmap[i0] * i3 / 255 + 255 - i3;

	      rgbout[0] = i0 | (i0 << 8);
	      rgbout[1] = i1 | (i1 << 8);
	      rgbout[2] = i2 | (i2 << 8);
	      i0 = rgbin[0];
	      i1 = rgbin[1];
	    }
	  break;
	case 3:
	  /*
	   * No alpha in image...
	   */
	  if (i0 == rgbin[0] && i1 == rgbin[1] && i2 == rgbin[2])
	    {
	      rgbout[0] = o0;
	      rgbout[1] = o1;
	      rgbout[2] = o2;
	      goto out;
	    }
	  else
	    {
	      i0 = rgbin[0];
	      i1 = rgbin[1];
	      i2 = rgbin[2];
	      rgbout[0] = i0 | (i0 << 8);
	      rgbout[1] = i1 | (i1 << 8);
	      rgbout[2] = i2 | (i2 << 8);
	    }
	  break;
	case 4:
	  if (i0 == rgbin[0] && i1 == rgbin[1] && i2 == rgbin[2] &&
	      i3 == rgbin[3])
	    {
	      rgbout[0] = o0;
	      rgbout[1] = o1;
	      rgbout[2] = o2;
	      goto out;
	    }
	  else
	    {
	      i3 = rgbin[3];
	      i0 = rgbin[0] * i3 / 255 + 255 - i3;
	      i1 = rgbin[1] * i3 / 255 + 255 - i3;
	      i2 = rgbin[2] * i3 / 255 + 255 - i3;
	      rgbout[0] = i0 | (i0 << 8);
	      rgbout[1] = i1 | (i1 << 8);
	      rgbout[2] = i2 | (i2 << 8);
	    }
	  break;
	}
      if ((compute_saturation) &&
	  (rgbout[0] != rgbout[1] || rgbout[0] != rgbout[2]))
	{
	  calc_rgb_to_hsl(rgbout, &h, &s, &l);
	  if (ssat < 1)
	    s *= ssat;
	  else
	    {
	      double s1 = s * ssat;
	      double s2 = 1.0 - ((1.0 - s) * isat);
	      s = FMIN(s1, s2);
	    }
	  if (s > 1)
	    s = 1.0;
	  calc_hsl_to_rgb(rgbout, h, s, l);
	}
      update_cmyk(rgbout);	/* Fiddle with the INPUT */
      rgbout[0] = lookup_value(rgbout[0], lut->steps,
			       lut->red, lut->shiftval,
			       lut->bin_size, lut->bin_shift);
      rgbout[1] = lookup_value(rgbout[1], lut->steps,
			       lut->green, lut->shiftval,
			       lut->bin_size, lut->bin_shift);
      rgbout[2] = lookup_value(rgbout[2], lut->steps,
			       lut->blue, lut->shiftval,
			       lut->bin_size, lut->bin_shift);
      if ((split_saturation || hue_map || lum_map || sat_map) &&
	  (rgbout[0] != rgbout[1] || rgbout[0] != rgbout[2]))
	{
	  calc_rgb_to_hsl(rgbout, &h, &s, &l);
	  if (split_saturation)
	    {
	      if (ssat < 1)
		s *= ssat;
	      else
		{
		  double s1 = s * ssat;
		  double s2 = 1.0 - ((1.0 - s) * isat);
		  s = FMIN(s1, s2);
		}
	    }
	  if (s > 1)
	    s = 1.0;
	  if (hue_map || lum_map || sat_map)
	    {
	      if (hue_map)
		{
		  int ih;
		  double eh;
		  double nh = h * 8;
		  ih = (int) nh;
		  eh = nh - (double) ih;
		  h = (ih / 8.0) + hue_map[ih] +
		    eh * ((1.0 / 8.0) + hue_map[ih + 1] - hue_map[ih]);
		  if (h < 0.0)
		    h += 6.0;
		  else if (h >= 6.0)
		    h -= 6.0;
		}
	      if (lum_map && l > .0001 && l < .9999)
		{
		  int ih;
		  double eh;
		  double nh = h * 8;
		  ih = (int) nh;
		  eh = nh - (double) ih;
		  if (lum_map[ih] != 1.0 || lum_map[ih + 1] != 1.0)
		    {
		      double el = lum_map[ih] +
			eh * (lum_map[ih + 1] - lum_map[ih]);
		      el = 1.0 + (s * (el - 1.0));
		      if (l > .5)
			el = 1.0 + ((2.0 * (1.0 - l)) * (el - 1.0));
		      l = 1.0 - pow(1.0 - l, el);
		    }
		}
	      if (sat_map)
		{
		  int ih;
		  double eh;
		  double nh = h * 8;
		  ih = (int) nh;
		  eh = nh - (double) ih;
		  if (sat_map[ih] != 1.0 || sat_map[ih + 1] != 1.0)
		    {
		      double es = sat_map[ih] +
			eh * (sat_map[ih + 1] - sat_map[ih]);
		      s = 1.0 - pow(1.0 - s, es);
		    }
		}
	    }
	  calc_hsl_to_rgb(rgbout, h, s, l);
	}
      if (ld < 65536)
	{
	  int i;
	  for (i = 0; i < 3; i++)
	    {
	      unsigned t = rgbout[i];
	      t = t * ld / 65536;
	      rgbout[i] = (unsigned short) t;
	    }
	}
      o0 = rgbout[0];
      o1 = rgbout[1];
      o2 = rgbout[2];
      nz0 |= o0;
      nz1 |= o1;
      nz2 |= o2;
    out:
      rgbin += bpp;
      rgbout += 3;
      width --;
    }
  if (zero_mask)
    {
      *zero_mask = nz0 ? 0 : 1;
      *zero_mask |= nz1 ? 0 : 2;
      *zero_mask |= nz2 ? 0 : 4;
    }
}

static void
indexed_to_rgb(const stp_vars_t vars,
	       const unsigned char *indexed,
	       unsigned short *rgb,
	       int *zero_mask,
	       int width,
	       int bpp,
	       const unsigned char *cmap,
	       const double *hue_map,
	       const double *lum_map,
	       const double *sat_map)
{
  rgb_to_rgb(vars, indexed, rgb, zero_mask, width, bpp, cmap,
	     hue_map, lum_map, sat_map);
}

static void
solid_rgb_to_rgb(const stp_vars_t vars,
		 const unsigned char *rgbin,
		 unsigned short *rgbout,
		 int *zero_mask,
		 int width,
		 int bpp,
		 const unsigned char *cmap,
		 const double *hue_map,
		 const double *lum_map,
		 const double *sat_map)
{
  unsigned ld = stp_get_density(vars) * 65536;
  double isat = 1.0;
  double ssat = stp_get_saturation(vars);
  int i0 = -1;
  int i1 = -1;
  int i2 = -1;
  int i3 = -1;
  int o0 = 0;
  int o1 = 0;
  int o2 = 0;
  int nz0 = 0;
  int nz1 = 0;
  int nz2 = 0;
  lut_t *lut = (lut_t *)(stp_get_lut(vars));
  int compute_saturation = ssat <= .99999 || ssat >= 1.00001;
  int split_saturation = ssat > 1.4;
  if (split_saturation)
    ssat = sqrt(ssat);
  if (ssat > 1)
    isat = 1.0 / ssat;
  while (width > 0)
    {
      double h, s, l;
      switch (bpp)
	{
	case 1:
	  /*
	   * No alpha in image, using colormap...
	   */
	  if (i0 == rgbin[0])
	    {
	      rgbout[0] = o0;
	      rgbout[1] = o1;
	      rgbout[2] = o2;
	      goto out;
	    }
	  else
	    {
	      i0 = rgbin[0] * 3;
	      i1 = cmap[i0 + 1];
	      i2 = cmap[i0 + 2];
	      i0 = cmap[i0];
	      rgbout[0] = i0 | (i0 << 8);
	      rgbout[1] = i1 | (i1 << 8);
	      rgbout[2] = i2 | (i2 << 8);
	      i0 = rgbin[0];
	    }
	  break;
	case 2:
	  if (i0 == rgbin[0] && i1 == rgbin[1])
	    {
	      rgbout[0] = o0;
	      rgbout[1] = o1;
	      rgbout[2] = o2;
	      goto out;
	    }
	  else
	    {
	      i0 = rgbin[0] * 3;
	      i3 = rgbin[1];
	      i1 = cmap[i0 + 1] * i3 / 255 + 255 - i3;
	      i2 = cmap[i0 + 2] * i3 / 255 + 255 - i3;
	      i0 = cmap[i0] * i3 / 255 + 255 - i3;

	      rgbout[0] = i0 | (i0 << 8);
	      rgbout[1] = i1 | (i1 << 8);
	      rgbout[2] = i2 | (i2 << 8);
	      i0 = rgbin[0];
	      i1 = rgbin[1];
	    }
	  break;
	case 3:
	  /*
	   * No alpha in image...
	   */
	  if (i0 == rgbin[0] && i1 == rgbin[1] && i2 == rgbin[2])
	    {
	      rgbout[0] = o0;
	      rgbout[1] = o1;
	      rgbout[2] = o2;
	      goto out;
	    }
	  else
	    {
	      i0 = rgbin[0];
	      i1 = rgbin[1];
	      i2 = rgbin[2];
	      rgbout[0] = i0 | (i0 << 8);
	      rgbout[1] = i1 | (i1 << 8);
	      rgbout[2] = i2 | (i2 << 8);
	    }
	  break;
	case 4:
	  if (i0 == rgbin[0] && i1 == rgbin[1] && i2 == rgbin[2] &&
	      i3 == rgbin[3])
	    {
	      rgbout[0] = o0;
	      rgbout[1] = o1;
	      rgbout[2] = o2;
	      goto out;
	    }
	  else
	    {
	      i3 = rgbin[3];
	      i0 = rgbin[0] * i3 / 255 + 255 - i3;
	      i1 = rgbin[1] * i3 / 255 + 255 - i3;
	      i2 = rgbin[2] * i3 / 255 + 255 - i3;
	      rgbout[0] = i0 | (i0 << 8);
	      rgbout[1] = i1 | (i1 << 8);
	      rgbout[2] = i2 | (i2 << 8);
	    }
	  break;
	}
      if ((compute_saturation) &&
	  (rgbout[0] != rgbout[1] || rgbout[0] != rgbout[2]))
	{
	  calc_rgb_to_hsl(rgbout, &h, &s, &l);
	  if (ssat < 1)
	    s *= ssat;
	  else
	    {
	      double s1 = s * ssat;
	      double s2 = 1.0 - ((1.0 - s) * isat);
	      s = FMIN(s1, s2);
	    }
	  if (s > 1)
	    s = 1.0;
	  calc_hsl_to_rgb(rgbout, h, s, l);
	}
      update_cmyk(rgbout);	/* Fiddle with the INPUT */
      rgbout[0] = lookup_value(rgbout[0], lut->steps,
			       lut->red, lut->shiftval,
			       lut->bin_size, lut->bin_shift);
      rgbout[1] = lookup_value(rgbout[1], lut->steps,
			       lut->green, lut->shiftval,
			       lut->bin_size, lut->bin_shift);
      rgbout[2] = lookup_value(rgbout[2], lut->steps,
			       lut->blue, lut->shiftval,
			       lut->bin_size, lut->bin_shift);
      if ((split_saturation || hue_map || lum_map || sat_map) &&
	  (rgbout[0] != rgbout[1] || rgbout[0] != rgbout[2]))
	{
	  calc_rgb_to_hsl(rgbout, &h, &s, &l);
	  if (split_saturation)
	    {
	      if (ssat < 1)
		s *= ssat;
	      else
		{
		  double s1 = s * ssat;
		  double s2 = 1.0 - ((1.0 - s) * isat);
		  s = FMIN(s1, s2);
		}
	    }
	  if (s > 1)
	    s = 1.0;
	  if (hue_map || lum_map || sat_map)
	    {
	      if (hue_map)
		{
		  int ih;
		  double eh;
		  double nh = h * 8;
		  ih = (int) nh;
		  eh = nh - (double) ih;
		  h = (ih / 8.0) + hue_map[ih] +
		    eh * ((1.0 / 8.0) + hue_map[ih + 1] - hue_map[ih]);
		  if (h < 0.0)
		    h += 6.0;
		  else if (h >= 6.0)
		    h -= 6.0;
		}
	      if (sat_map)
		{
		  int ih;
		  double eh;
		  double nh = h * 8;
		  ih = (int) nh;
		  eh = nh - (double) ih;
		  if (sat_map[ih] != 1.0 || sat_map[ih + 1] != 1.0)
		    {
		      double es = sat_map[ih] +
			eh * (sat_map[ih + 1] - sat_map[ih]);
		      s = 1.0 - pow(1.0 - s, es);
		    }
		}
	    }
	  calc_hsl_to_rgb(rgbout, h, s, l);
	}
      if (ld < 65536)
	{
	  int i;
	  for (i = 0; i < 3; i++)
	    {
	      unsigned t = rgbout[i];
	      t = t * ld / 65536;
	      rgbout[i] = (unsigned short) t;
	    }
	}
      o0 = rgbout[0];
      o1 = rgbout[1];
      o2 = rgbout[2];
      nz0 |= o0;
      nz1 |= o1;
      nz2 |= o2;
    out:
      rgbin += bpp;
      rgbout += 3;
      width --;
    }
  if (zero_mask)
    {
      *zero_mask = nz0 ? 0 : 1;
      *zero_mask |= nz1 ? 0 : 2;
      *zero_mask |= nz2 ? 0 : 4;
    }
}

static void
solid_indexed_to_rgb(const stp_vars_t vars,
		     const unsigned char *indexed,
		     unsigned short *rgb,
		     int *zero_mask,
		     int width,
		     int bpp,
		     const unsigned char *cmap,
		     const double *hue_map,
		     const double *lum_map,
		     const double *sat_map)
{
  solid_rgb_to_rgb(vars, indexed, rgb, zero_mask, width, bpp, cmap,
		   hue_map, lum_map, sat_map);
}

/*
 * 'gray_to_rgb()' - Convert gray image data to RGB.
 */

static void
gray_to_rgb(const stp_vars_t vars,
	    const unsigned char	*grayin,
	    unsigned short *rgbout,
	    int *zero_mask,
	    int width,
	    int bpp,
	    const unsigned char *cmap,
	    const double *hue_map,
	    const double *lum_map,
	    const double *sat_map)
{
  int i0 = -1;
  int i1 = -1;
  int o0 = 0;
  int o1 = 0;
  int o2 = 0;
  int nz0 = 0;
  int nz1 = 0;
  int nz2 = 0;
  lut_t *lut = (lut_t *)(stp_get_lut(vars));
  double density = stp_get_density(vars);
  while (width > 0)
    {
      unsigned short trgb[3];
      if (bpp == 1)
	{
	  /*
	   * No alpha in image...
	   */
	  if (i0 == grayin[0])
	    {
	      rgbout[0] = o0;
	      rgbout[1] = o1;
	      rgbout[2] = o2;
	      goto out;
	    }
	  else
	    {
	      i0 = grayin[0];
	      trgb[0] =
	      trgb[1] =
	      trgb[2] = i0 | (i0 << 8);
	    }
	}
      else
	{
	  if (i0 == grayin[0] && i1 == grayin[1])
	    {
	      rgbout[0] = o0;
	      rgbout[1] = o1;
	      rgbout[2] = o2;
	      goto out;
	    }
	  else
	    {
	      i0 = (grayin[0] * grayin[1] / 255 + 255 - grayin[1]);
	      trgb[0] =
	      trgb[1] =
	      trgb[2] = i0 | (i0 << 8);
	    }
	}
      update_cmyk(trgb);
      rgbout[0] = lookup_value(trgb[0], lut->steps, lut->red, lut->shiftval,
			       lut->bin_size, lut->bin_shift);
      rgbout[1] = lookup_value(trgb[1], lut->steps, lut->green, lut->shiftval,
			       lut->bin_size, lut->bin_shift);
      rgbout[2] = lookup_value(trgb[2], lut->steps, lut->blue, lut->shiftval,
			       lut->bin_size, lut->bin_shift);
      if (density != 1.0)
	{
	  int i;
	  for (i = 0; i < 3; i++)
	    rgbout[i] = .5 + (rgbout[i] * density);
	}
      o0 = rgbout[0];
      o1 = rgbout[1];
      o2 = rgbout[2];
      nz0 |= o0;
      nz1 |= o1;
      nz2 |= o2;
    out:
      grayin += bpp;
      rgbout += 3;
      width --;
    }
  if (zero_mask)
    {
      *zero_mask = nz0 ? 0 : 1;
      *zero_mask |= nz1 ? 0 : 2;
      *zero_mask |= nz2 ? 0 : 4;
    }
}

static void
fast_indexed_to_rgb(const stp_vars_t vars,
		    const unsigned char *indexed,
		    unsigned short *rgb,
		    int *zero_mask,
		    int width,
		    int bpp,
		    const unsigned char *cmap,
		    const double *hue_map,
		    const double *lum_map,
		    const double *sat_map)
{
  int i0 = -1;
  int i1 = -1;
  int o0 = 0;
  int o1 = 0;
  int o2 = 0;
  int nz0 = 0;
  int nz1 = 0;
  int nz2 = 0;
  lut_t *lut = (lut_t *)(stp_get_lut(vars));
  double isat = 1.0;
  double saturation = stp_get_saturation(vars);
  double density = stp_get_density(vars);
  if (saturation > 1)
    isat = 1.0 / saturation;
  while (width > 0)
    {
      double h, s, l;
      if (bpp == 1)
	{
	  /*
	   * No alpha in image...
	   */
	  if (i0 == indexed[0])
	    {
	      rgb[0] = o0;
	      rgb[1] = o1;
	      rgb[2] = o2;
	      goto out;
	    }
	  else
	    {
	      i0 = indexed[0];
	      rgb[0] = lut->red[cmap[i0 * 3 + 0]];
	      rgb[1] = lut->green[cmap[i0 * 3 + 1]];
	      rgb[2] = lut->blue[cmap[i0 * 3 + 2]];
	    }
	}
      else
	{
	  if (i0 == indexed[0] && i1 == indexed[1])
	    {
	      rgb[0] = o0;
	      rgb[1] = o1;
	      rgb[2] = o2;
	      goto out;
	    }
	  else
	    {
	      i0 = indexed[0];
	      i1 = indexed[1];
	      rgb[0] = lut->red[cmap[i0 * 3 + 0] * i1 / 255 + 255 - i1];
	      rgb[1] = lut->green[cmap[i0 * 3 + 1] * i1 / 255 + 255 -i1];
	      rgb[2] = lut->blue[cmap[i0 * 3 + 2] * i1 / 255 + 255 - i1];
	    }
	}
      if (saturation != 1.0)
	{
	  calc_rgb_to_hsl(rgb, &h, &s, &l);
	  if (saturation < 1)
	    s *= saturation;
	  else if (saturation > 1)
	    {
	      double s1 = s * saturation;
	      double s2 = 1.0 - ((1.0 - s) * isat);
	      s = FMIN(s1, s2);
	    }
	  if (s > 1)
	    s = 1.0;
	  calc_hsl_to_rgb(rgb, h, s, l);
	}
      if (density != 1.0)
	{
	  int i;
	  for (i = 0; i < 3; i++)
	    rgb[i] = .5 + (rgb[i] * density);
	}
    out:
      o0 = rgb[0];
      o1 = rgb[1];
      o2 = rgb[2];
      nz0 |= o0;
      nz1 |= o1;
      nz2 |= o2;
      indexed += bpp;
      rgb += 3;
      width --;
    }
  if (zero_mask)
    {
      *zero_mask = nz0 ? 0 : 1;
      *zero_mask |= nz1 ? 0 : 2;
      *zero_mask |= nz2 ? 0 : 4;
    }
}

/*
 * 'rgb_to_rgb()' - Convert rgb image data to RGB.
 */

static void
fast_rgb_to_rgb(const stp_vars_t vars,
		const unsigned char *rgbin,
		unsigned short *rgbout,
		int *zero_mask,
		int width,
		int bpp,
		const unsigned char *cmap,
		const double *hue_map,
		const double *lum_map,
		const double *sat_map)
{
  unsigned ld = stp_get_density(vars) * 65536;
  int i0 = -1;
  int i1 = -1;
  int i2 = -1;
  int i3 = -1;
  int o0 = 0;
  int o1 = 0;
  int o2 = 0;
  int nz0 = 0;
  int nz1 = 0;
  int nz2 = 0;
  lut_t *lut = (lut_t *)(stp_get_lut(vars));
  double isat = 1.0;
  double saturation = stp_get_saturation(vars);
  if (saturation > 1)
    isat = 1.0 / saturation;
  while (width > 0)
    {
      double h, s, l;
      if (bpp == 3)
	{
	  /*
	   * No alpha in image...
	   */
	  if (i0 == rgbin[0] && i1 == rgbin[1] && i2 == rgbin[2])
	    {
	      rgbout[0] = o0;
	      rgbout[1] = o1;
	      rgbout[2] = o2;
	      goto out;
	    }
	  else
	    {
	      i0 = rgbin[0];
	      i1 = rgbin[1];
	      i2 = rgbin[2];
	      rgbout[0] = lut->red[rgbin[0]];
	      rgbout[1] = lut->green[rgbin[1]];
	      rgbout[2] = lut->blue[rgbin[2]];
	    }
	}
      else
	{
	  if (i0 == rgbin[0] && i1 == rgbin[1] && i2 == rgbin[2] &&
	      i3 == rgbin[3])
	    {
	      rgbout[0] = o0;
	      rgbout[1] = o1;
	      rgbout[2] = o2;
	      goto out;
	    }
	  else
	    {
	      i0 = rgbin[0];
	      i1 = rgbin[1];
	      i2 = rgbin[2];
	      i3 = rgbin[3];
	      rgbout[0] = lut->red[i0 * i3 / 255 + 255 - i3];
	      rgbout[1] = lut->green[i1 * i3 / 255 + 255 - i3];
	      rgbout[2] = lut->blue[i2 * i3 / 255 + 255 - i3];
	    }
	}
      if (saturation != 1.0)
	{
	  calc_rgb_to_hsl(rgbout, &h, &s, &l);
	  if (saturation < 1)
	    s *= saturation;
	  else if (saturation > 1)
	    {
	      double s1 = s * saturation;
	      double s2 = 1.0 - ((1.0 - s) * isat);
	      s = FMIN(s1, s2);
	    }
	  if (s > 1)
	    s = 1.0;
	  calc_hsl_to_rgb(rgbout, h, s, l);
	}
      if (ld < 65536)
	{
	  int i;
	  for (i = 0; i < 3; i++)
	    rgbout[i] = rgbout[i] * ld / 65536;
	}
      o0 = rgbout[0];
      o1 = rgbout[1];
      o2 = rgbout[2];
      nz0 |= o0;
      nz1 |= o1;
      nz2 |= o2;
    out:
      rgbin += bpp;
      rgbout += 3;
      width --;
    }
  if (zero_mask)
    {
      *zero_mask = nz0 ? 0 : 1;
      *zero_mask |= nz1 ? 0 : 2;
      *zero_mask |= nz2 ? 0 : 4;
    }
}

/*
 * 'gray_to_rgb()' - Convert gray image data to RGB.
 */

static void
fast_gray_to_rgb(const stp_vars_t vars,
		 const unsigned char *grayin,
		 unsigned short *rgbout,
		 int *zero_mask,
		 int width,
		 int bpp,
		 const unsigned char *cmap,
		 const double *hue_map,
		 const double *lum_map,
		 const double *sat_map)
{
  int i0 = -1;
  int i1 = -1;
  int o0 = 0;
  int o1 = 0;
  int o2 = 0;
  int nz0 = 0;
  int nz1 = 0;
  int nz2 = 0;
  lut_t *lut = (lut_t *)(stp_get_lut(vars));
  double density = stp_get_density(vars);
  while (width > 0)
    {
      if (bpp == 1)
	{
	  /*
	   * No alpha in image...
	   */
	  if (i0 == grayin[0])
	    {
	      rgbout[0] = o0;
	      rgbout[1] = o1;
	      rgbout[2] = o2;
	      goto out;
	    }
	  else
	    {
	      i0 = grayin[0];
	      rgbout[0] = lut->red[grayin[0]];
	      rgbout[1] = lut->green[grayin[0]];
	      rgbout[2] = lut->blue[grayin[0]];
	    }
	}
      else
	{
	  if (i0 == grayin[0] && i1 == grayin[1])
	    {
	      rgbout[0] = o0;
	      rgbout[1] = o1;
	      rgbout[2] = o2;
	      goto out;
	    }
	  else
	    {
	      int lookup = (grayin[0] * grayin[1] / 255 + 255 - grayin[1]);
	      i0 = grayin[0];
	      i1 = grayin[1];
	      rgbout[0] = lut->red[lookup];
	      rgbout[1] = lut->green[lookup];
	      rgbout[2] = lut->blue[lookup];
	    }
	}
      if (density != 1.0)
	{
	  int i;
	  for (i = 0; i < 3; i++)
	    rgbout[i] = .5 + (rgbout[i] * density);
	}
      o0 = rgbout[0];
      o1 = rgbout[1];
      o2 = rgbout[2];
      nz0 |= o0;
      nz1 |= o1;
      nz2 |= o2;
    out:
      grayin += bpp;
      rgbout += 3;
      width --;
    }
  if (zero_mask)
    {
      *zero_mask = nz0 ? 0 : 1;
      *zero_mask |= nz1 ? 0 : 2;
      *zero_mask |= nz2 ? 0 : 4;
    }
}

static void
cmyk_8_to_cmyk(const stp_vars_t vars,
	       const unsigned char *cmykin,
	       unsigned short *cmykout,
	       int *zero_mask,
	       int width,
	       int bpp,
	       const unsigned char *cmap,
	       const double *hue_map,
	       const double *lum_map,
	       const double *sat_map)
{
  int i;
  int j;
  int nz[4];
  static unsigned short	lut[256];
  static double density = -1.0;
  static double print_gamma = -1.0;

  memset(nz, 0, sizeof(nz));
  if (density != stp_get_density(vars) ||
      print_gamma != stp_get_gamma(vars))
  {
    density     = stp_get_density(vars);
    print_gamma = stp_get_gamma(vars);

    for (i = 0; i < 256; i ++)
      lut[i] = 65535.0 * density * pow((double)i / 255.0, print_gamma) + 0.5;
  }

  for (i = 0; i < width; i++)
    {
      j = *cmykin++;
      nz[0] |= j;
      *cmykout++ = lut[j];

      j = *cmykin++;
      nz[1] |= j;
      *cmykout++ = lut[j];

      j = *cmykin++;
      nz[2] |= j;
      *cmykout++ = lut[j];

      j = *cmykin++;
      nz[3] |= j;
      *cmykout++ = lut[j];
    }
  if (zero_mask)
    {
      *zero_mask = nz[0] ? 0 : 1;
      *zero_mask |= nz[1] ? 0 : 2;
      *zero_mask |= nz[2] ? 0 : 4;
      *zero_mask |= nz[3] ? 0 : 8;
    }
}

static void
cmyk_to_cmyk(const stp_vars_t vars,
	     const unsigned char *cmykin,
	     unsigned short *cmykout,
	     int *zero_mask,
	     int width,
	     int bpp,
	     const unsigned char *cmap,
	     const double *hue_map,
	     const double *lum_map,
	     const double *sat_map)
{
  int i;
  int j;
  int nz[4];
  const unsigned short *scmykin = (const unsigned short *) cmykin;

  memset(nz, 0, sizeof(nz));
  for (i = 0; i < width; i++)
    {
      for (j = 0; j < 4; j++)
	{
	  nz[j] |= scmykin[j];
	  cmykout[j] = scmykin[j];
	}
      scmykin += 4;
      cmykout += 4;
    }
  if (zero_mask)
    {
      *zero_mask = nz[0] ? 0 : 1;
      *zero_mask |= nz[1] ? 0 : 2;
      *zero_mask |= nz[2] ? 0 : 4;
      *zero_mask |= nz[3] ? 0 : 8;
    }
}

static lut_t *
allocate_lut(size_t steps)
{
  int i;
  lut_t *ret = stp_malloc(sizeof(lut_t));
  ret->steps = steps;
  ret->composite = stp_zalloc(sizeof(unsigned short) * steps);
  ret->red = stp_zalloc(sizeof(unsigned short) * steps);
  ret->green = stp_zalloc(sizeof(unsigned short) * steps);
  ret->blue = stp_zalloc(sizeof(unsigned short) * steps);
  ret->shiftval = 0;
  for (i = 1; i < steps; i += i)
    ret->shiftval++;
  ret->bin_size = 65536 / steps;
  ret->bin_shift = 16 - ret->shiftval;
  return ret;
}

void
stp_free_lut(stp_vars_t v)
{
  if (stp_get_lut(v))
    {
      lut_t *lut = (lut_t *)(stp_get_lut(v));
      if (lut->composite)
	stp_free(lut->composite);
      if (lut->red)
	stp_free(lut->red);
      if (lut->green)
	stp_free(lut->green);
      if (lut->blue)
	stp_free(lut->blue);
      lut->steps = 0;
      lut->composite = NULL;
      lut->red = NULL;
      lut->green = NULL;
      lut->blue = NULL;
      stp_free(stp_get_lut(v));
      stp_set_lut(v, NULL);
    }
}

void
stp_compute_lut(stp_vars_t v, size_t steps)
{
  double	pixel,		/* Pixel value */
		red_pixel,	/* Pixel value */
		green_pixel,	/* Pixel value */
		blue_pixel;	/* Pixel value */
  int i;
  /*
   * Got an output file/command, now compute a brightness lookup table...
   */

  double cyan = stp_get_cyan(v);
  double magenta = stp_get_magenta(v);
  double yellow = stp_get_yellow(v);
  double print_gamma = stp_get_gamma(v);
  double contrast = stp_get_contrast(v);
  double app_gamma = stp_get_app_gamma(v);
  double brightness = stp_get_brightness(v);
  double screen_gamma = app_gamma / 4.0; /* "Empirical" */
  double pivot = .25;
  double ipivot = 1.0 - pivot;
  lut_t *lut;

  /*
   * Monochrome mode simply thresholds the input
   * to decide whether to print at all.  The printer gamma
   * is intended to represent the analog response of the printer.
   * Using it shifts the threshold, which is not the intent
   * of how this works.
   */
  if (stp_get_output_type(v) == OUTPUT_MONOCHROME)
    print_gamma = 1.0;

  lut = allocate_lut(steps);
  stp_set_lut(v, lut);
  stp_dprintf(STP_DBG_LUT, v, "stp_compute_lut\n");
  stp_dprintf(STP_DBG_LUT, v, " cyan %.3f\n", cyan);
  stp_dprintf(STP_DBG_LUT, v, " magenta %.3f\n", magenta);
  stp_dprintf(STP_DBG_LUT, v, " yellow %.3f\n", yellow);
  stp_dprintf(STP_DBG_LUT, v, " print_gamma %.3f\n", print_gamma);
  stp_dprintf(STP_DBG_LUT, v, " contrast %.3f\n", contrast);
  stp_dprintf(STP_DBG_LUT, v, " brightness %.3f\n", brightness);
  stp_dprintf(STP_DBG_LUT, v, " screen_gamma %.3f\n", screen_gamma);
  for (i = 0; i < steps; i ++)
    {
      double temp_pixel;
      pixel = (double) i / (double) (steps - 1);

      if (stp_get_input_color_model(v) == COLOR_MODEL_CMY)
	pixel = 1.0 - pixel;

      /*
       * First, correct contrast
       */
      if (pixel >= .5)
	temp_pixel = 1.0 - pixel;
      else
	temp_pixel = pixel;
      if (contrast > 3.99999)
	{
	  if (temp_pixel < .5)
	    temp_pixel = 0;
	  else
	    temp_pixel = 1;
	}
      if (temp_pixel <= .000001 && contrast <= .0001)
	temp_pixel = .5;
      else if (temp_pixel > 1)
	temp_pixel = .5 * pow(2 * temp_pixel, pow(contrast, contrast));
      else if (temp_pixel < 1)
	temp_pixel = 0.5 -
	  ((0.5 - .5 * pow(2 * temp_pixel, contrast)) * contrast);
      if (temp_pixel > .5)
	temp_pixel = .5;
      else if (temp_pixel < 0)
	temp_pixel = 0;
      if (pixel < .5)
	pixel = temp_pixel;
      else
	pixel = 1 - temp_pixel;

      /*
       * Second, do brightness
       */
      if (brightness < 1)
	pixel = pixel * brightness;
      else
	pixel = 1 - ((1 - pixel) * (2 - brightness));

      /*
       * Third, correct for the screen gamma
       */

      pixel = 1.0 -
	(1.0 / (1.0 - pow(pivot, screen_gamma))) *
	(pow(pivot + ipivot * pixel, screen_gamma) - pow(pivot, screen_gamma));

      /*
       * Third, fix up cyan, magenta, yellow values
       */
      if (pixel < 0.0)
	pixel = 0.0;
      else if (pixel > 1.0)
	pixel = 1.0;

      if (pixel > .9999 && cyan < .00001)
	red_pixel = 0;
      else
	red_pixel = 1 - pow(1 - pixel, cyan);
      if (pixel > .9999 && magenta < .00001)
	green_pixel = 0;
      else
	green_pixel = 1 - pow(1 - pixel, magenta);
      if (pixel > .9999 && yellow < .00001)
	blue_pixel = 0;
      else
	blue_pixel = 1 - pow(1 - pixel, yellow);

      /*
       * Finally, fix up print gamma and scale
       */

      pixel = 65535 * pow(pixel, print_gamma) + .5;
      red_pixel = 65535 * pow(red_pixel, print_gamma) + .5;
      green_pixel = 65535 * pow(green_pixel, print_gamma) + .5;
      blue_pixel = 65535 * pow(blue_pixel, print_gamma) + .5;
      if (stp_get_output_color_model(v) == COLOR_MODEL_RGB)
	{
	  pixel = 65535 - pixel;
	  red_pixel = 65535 - red_pixel;
	  blue_pixel = 65535 - blue_pixel;
	  green_pixel = 65535 - green_pixel;
	}

      if (pixel <= 0.0)
	lut->composite[i] = 0;
      else if (pixel >= 65535.0)
	lut->composite[i] = 65535;
      else
	lut->composite[i] = (unsigned)(pixel);

      if (red_pixel <= 0.0)
	lut->red[i] = 0;
      else if (red_pixel >= 65535.0)
	lut->red[i] = 65535;
      else
	lut->red[i] = (unsigned)(red_pixel);

      if (green_pixel <= 0.0)
	lut->green[i] = 0;
      else if (green_pixel >= 65535.0)
	lut->green[i] = 65535;
      else
	lut->green[i] = (unsigned)(green_pixel);

      if (blue_pixel <= 0.0)
	lut->blue[i] = 0;
      else if (blue_pixel >= 65535.0)
	lut->blue[i] = 65535;
      else
	lut->blue[i] = (unsigned)(blue_pixel);
      stp_dprintf(STP_DBG_LUT, v,
		  "%3i  %5d  %5d  %5d  %5d\n",
		  i, lut->composite[i], lut->red[i],
		  lut->green[i], lut->blue[i]);
    }
}

#define RETURN_COLORFUNC(x)						   \
do									   \
{									   \
  stp_dprintf(STP_DBG_COLORFUNC, v,					   \
	      "stp_choose_colorfunc(type %d bpp %d cmap %d) ==> %s, %d\n", \
	      output_type, image_bpp, cmap, #x, *out_bpp);		   \
  return (x);								   \
} while (0)								   \

stp_convert_t
stp_choose_colorfunc(int output_type,
		     int image_bpp,
		     const unsigned char *cmap,
		     int *out_bpp,
		     const stp_vars_t v)
{
  switch (stp_get_output_type(v))
    {
    case OUTPUT_MONOCHROME:
      *out_bpp = 1;
      switch (image_bpp)
	{
	case 1:
	  if (cmap)
	    RETURN_COLORFUNC(indexed_to_monochrome);
	  else
	    RETURN_COLORFUNC(gray_to_monochrome);
	case 2:
	  if (cmap)
	    RETURN_COLORFUNC(indexed_alpha_to_monochrome);
	  else
	    RETURN_COLORFUNC(gray_alpha_to_monochrome);
	case 3:
	  RETURN_COLORFUNC(rgb_to_monochrome);
	case 4:
	  RETURN_COLORFUNC(rgb_alpha_to_monochrome);
	default:
	  RETURN_COLORFUNC(NULL);
	}
      break;
    case OUTPUT_RAW_CMYK:
      *out_bpp = 4;
      switch (image_bpp)
	{
	case 4:
	  RETURN_COLORFUNC(cmyk_8_to_cmyk);
	case 8:
	  RETURN_COLORFUNC(cmyk_to_cmyk);
	default:
	  RETURN_COLORFUNC(NULL);
	}
      break;
    case OUTPUT_COLOR:
      *out_bpp = 3;
      switch (stp_get_image_type(v))
	{
	case IMAGE_CONTINUOUS:
	  if (image_bpp >= 3)
	    RETURN_COLORFUNC(rgb_to_rgb);
	  else if (cmap == NULL)
	    RETURN_COLORFUNC(gray_to_rgb);
	  else
	    RETURN_COLORFUNC(indexed_to_rgb);
	case IMAGE_SOLID_TONE:
	  if (image_bpp >= 3)
	    RETURN_COLORFUNC(solid_rgb_to_rgb);
	  else if (cmap == NULL)
	    RETURN_COLORFUNC(gray_to_rgb);
	  else
	    RETURN_COLORFUNC(solid_indexed_to_rgb);
	case IMAGE_LINE_ART:
	  if (image_bpp >= 3)
	    RETURN_COLORFUNC(fast_rgb_to_rgb);
	  else if (cmap == NULL)
	    RETURN_COLORFUNC(fast_gray_to_rgb);
	  else
	    RETURN_COLORFUNC(fast_indexed_to_rgb);
	default:
	  RETURN_COLORFUNC(NULL);
	}
    case OUTPUT_GRAY:
    default:
      *out_bpp = 1;
      switch (image_bpp)
	{
	case 1:
	  if (cmap)
	    RETURN_COLORFUNC(indexed_to_gray);
	  else
	    RETURN_COLORFUNC(gray_to_gray);
	case 2:
	  if (cmap)
	    RETURN_COLORFUNC(indexed_alpha_to_gray);
	  else
	    RETURN_COLORFUNC(gray_alpha_to_gray);
	case 3:
	  RETURN_COLORFUNC(rgb_to_gray);
	case 4:
	  RETURN_COLORFUNC(rgb_alpha_to_gray);
	default:
	  RETURN_COLORFUNC(NULL);
	}
      break;
    }
}

/*
 * "$Id: escp2-channels.c,v 1.68 2007/05/05 23:37:01 rlk Exp $"
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <gutenprint/gutenprint.h>
#include "gutenprint-internal.h"
#include <gutenprint/gutenprint-intl-internal.h>
#include "print-escp2.h"


#define DECLARE_INK_CHANNEL(name)				\
static const ink_channel_t name##_channel =			\
{								\
  #name,							\
  name##_subchannels,						\
  sizeof(name##_subchannels) / sizeof(physical_subchannel_t),	\
  NULL								\
}

#define DECLARE_EXTENDED_INK_CHANNEL(name)			\
static const ink_channel_t name##_channel =			\
{								\
  #name,							\
  name##_subchannels,						\
  sizeof(name##_subchannels) / sizeof(physical_subchannel_t),	\
  &name##_curve							\
}

static hue_curve_t generic_cyan_curve =
{
  "CyanCurve",
  "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
  "<gutenprint>\n"
  "<curve wrap=\"wrap\" type=\"linear\" gamma=\"0\">\n"
  "<sequence count=\"48\" lower-bound=\"0\" upper-bound=\"4\">\n"
  /* C */  "1.000 1.000 1.000 1.000 1.000 1.000 1.000 1.000 "  /* B */
  /* B */  "1.000 0.875 0.750 0.625 0.500 0.375 0.250 0.125 "  /* M */
  /* M */  "0.000 0.000 0.000 0.000 0.000 0.000 0.000 0.000 "  /* R */
  /* R */  "0.000 0.000 0.000 0.000 0.000 0.000 0.000 0.000 "  /* Y */
  /* Y */  "0.000 0.125 0.250 0.375 0.500 0.625 0.750 0.875 "  /* G */
  /* G */  "1.000 1.000 1.000 1.000 1.000 1.000 1.000 1.000 "  /* C */
  "</sequence>\n"
  "</curve>\n"
  "</gutenprint>\n"
};

static hue_curve_t generic_magenta_curve =
{
  "CyanCurve",
  "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
  "<gutenprint>\n"
  "<curve wrap=\"wrap\" type=\"linear\" gamma=\"0\">\n"
  "<sequence count=\"48\" lower-bound=\"0\" upper-bound=\"4\">\n"
  /* C */  "0.000 0.125 0.250 0.375 0.500 0.625 0.750 0.875 "  /* B */
  /* B */  "1.000 1.000 1.000 1.000 1.000 1.000 1.000 1.000 "  /* M */
  /* M */  "1.000 1.000 1.000 1.000 1.000 1.000 1.000 1.000 "  /* R */
  /* R */  "1.000 0.875 0.750 0.625 0.500 0.375 0.250 0.125 "  /* Y */
  /* Y */  "0.000 0.000 0.000 0.000 0.000 0.000 0.000 0.000 "  /* G */
  /* G */  "0.000 0.000 0.000 0.000 0.000 0.000 0.000 0.000 "  /* C */
  "</sequence>\n"
  "</curve>\n"
  "</gutenprint>\n"
};

static hue_curve_t generic_yellow_curve =
{
  "CyanCurve",
  "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
  "<gutenprint>\n"
  "<curve wrap=\"wrap\" type=\"linear\" gamma=\"0\">\n"
  "<sequence count=\"48\" lower-bound=\"0\" upper-bound=\"4\">\n"
  /* C */  "0.000 0.000 0.000 0.000 0.000 0.000 0.000 0.000 "  /* B */
  /* B */  "0.000 0.000 0.000 0.000 0.000 0.000 0.000 0.000 "  /* M */
  /* M */  "0.000 0.125 0.250 0.375 0.500 0.625 0.750 0.875 "  /* R */
  /* R */  "1.000 1.000 1.000 1.000 1.000 1.000 1.000 1.000 "  /* Y */
  /* Y */  "1.000 1.000 1.000 1.000 1.000 1.000 1.000 1.000 "  /* G */
  /* G */  "1.000 0.875 0.750 0.625 0.500 0.375 0.250 0.125 "  /* C */
  "</sequence>\n"
  "</curve>\n"
  "</gutenprint>\n"
};

static hue_curve_t r800_cyan_curve =
{
  "CyanCurve",
  "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
  "<gutenprint>\n"
  "<curve wrap=\"wrap\" type=\"linear\" gamma=\"0\">\n"
  "<sequence count=\"48\" lower-bound=\"0\" upper-bound=\"4\">\n"
  /* C */  "1.000 1.000 1.000 1.000 1.000 1.000 1.000 1.000 "  /* B */
  /* B */  "1.000 0.875 0.700 0.550 0.400 0.300 0.200 0.100 "  /* M */
  /* M */  "0.000 0.000 0.000 0.000 0.000 0.000 0.000 0.000 "  /* R */
  /* R */  "0.000 0.000 0.000 0.000 0.000 0.000 0.000 0.000 "  /* Y */
  /* Y */  "0.000 0.125 0.250 0.375 0.500 0.625 0.750 0.875 "  /* G */
  /* G */  "1.000 1.000 1.000 1.000 1.000 1.000 1.000 1.000 "  /* C */
  "</sequence>\n"
  "</curve>\n"
  "</gutenprint>\n"
};

static hue_curve_t r800_magenta_curve =
{
  "CyanCurve",
  "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
  "<gutenprint>\n"
  "<curve wrap=\"wrap\" type=\"linear\" gamma=\"0\">\n"
  "<sequence count=\"48\" lower-bound=\"0\" upper-bound=\"4\">\n"
  /* C */  "0.000 0.000 0.000 0.000 0.000 0.000 0.000 0.000 "  /* B */
  /* B */  "0.000 0.000 0.000 0.002 0.050 0.300 0.600 0.800 "  /* M */
  /* M */  "1.000 1.000 0.850 0.700 0.600 0.500 0.400 0.300 "  /* R */
  /* R */  "0.200 0.100 0.050 0.000 0.000 0.000 0.000 0.000 "  /* Y */
  /* Y */  "0.000 0.000 0.000 0.000 0.000 0.000 0.000 0.000 "  /* G */
  /* G */  "0.000 0.000 0.000 0.000 0.000 0.000 0.000 0.000 "  /* C */
  "</sequence>\n"
  "</curve>\n"
  "</gutenprint>\n"
};

static hue_curve_t r800_yellow_curve =
{
  "CyanCurve",
  "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
  "<gutenprint>\n"
  "<curve wrap=\"wrap\" type=\"linear\" gamma=\"0\">\n"
  "<sequence count=\"48\" lower-bound=\"0\" upper-bound=\"4\">\n"
  /* C */  "0.000 0.000 0.000 0.000 0.000 0.000 0.000 0.000 "  /* B */
  /* B */  "0.000 0.000 0.000 0.000 0.000 0.000 0.000 0.000 "  /* M */
  /* M */  "0.000 0.000 0.000 0.000 0.000 0.000 0.050 0.150 "  /* R */
  /* R */  "0.250 0.350 0.450 0.550 0.650 0.750 0.850 0.950 "  /* Y */
  /* Y */  "1.000 1.000 1.000 1.000 1.000 1.000 1.000 1.000 "  /* G */
  /* G */  "1.000 0.875 0.750 0.625 0.500 0.375 0.250 0.125 "  /* C */
  "</sequence>\n"
  "</curve>\n"
  "</gutenprint>\n"
};

static hue_curve_t r800_red_curve =
{
  "CyanCurve",
  "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
  "<gutenprint>\n"
  "<curve wrap=\"wrap\" type=\"linear\" gamma=\"0\">\n"
  "<sequence count=\"48\" lower-bound=\"0\" upper-bound=\"4\">\n"
  /* C */  "0.000 0.000 0.000 0.000 0.000 0.000 0.000 0.000 "  /* B */
  /* B */  "0.000 0.000 0.000 0.000 0.000 0.000 0.000 0.000 "  /* M */
  /* M */  "0.025 0.400 0.600 0.750 0.890 1.000 1.000 1.000 "  /* R */
  /* R */  "1.000 0.875 0.750 0.625 0.500 0.375 0.250 0.125 "  /* Y */
  /* Y */  "0.000 0.000 0.000 0.000 0.000 0.000 0.000 0.000 "  /* G */
  /* G */  "0.000 0.000 0.000 0.000 0.000 0.000 0.000 0.000 "  /* C */
  "</sequence>\n"
  "</curve>\n"
  "</gutenprint>\n"
};

static hue_curve_t r800_blue_curve =
{
  "CyanCurve",
  "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
  "<gutenprint>\n"
  "<curve wrap=\"wrap\" type=\"linear\" gamma=\"0\">\n"
  "<sequence count=\"48\" lower-bound=\"0\" upper-bound=\"4\">\n"
  /* C */  "0.000 0.250 0.475 0.700 0.810 0.875 0.940 1.000 "  /* B */
  /* B */  "1.000 0.975 0.930 0.875 0.810 0.740 0.650 0.400 "  /* M */
  /* M */  "0.040 0.002 0.000 0.000 0.000 0.000 0.000 0.000 "  /* R */
  /* R */  "0.000 0.000 0.000 0.000 0.000 0.000 0.000 0.000 "  /* Y */
  /* Y */  "0.000 0.000 0.000 0.000 0.000 0.000 0.000 0.000 "  /* G */
  /* G */  "0.000 0.000 0.000 0.000 0.000 0.000 0.000 0.000 "  /* C */
  "</sequence>\n"
  "</curve>\n"
  "</gutenprint>\n"
};

static hue_curve_t picturemate_cyan_curve =
{
  "CyanCurve",
  "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
  "<gutenprint>\n"
  "<curve wrap=\"wrap\" type=\"linear\" gamma=\"0\">\n"
  "<sequence count=\"48\" lower-bound=\"0\" upper-bound=\"4\">\n"
  /* C */  "1.000 1.000 1.000 1.000 1.000 1.000 1.000 1.000 "  /* B */
  /* B */  "1.000 0.875 0.700 0.550 0.400 0.300 0.200 0.100 "  /* M */
  /* M */  "0.000 0.000 0.000 0.000 0.000 0.000 0.000 0.000 "  /* R */
  /* R */  "0.000 0.000 0.000 0.000 0.000 0.000 0.000 0.000 "  /* Y */
  /* Y */  "0.000 0.125 0.250 0.375 0.500 0.625 0.750 0.875 "  /* G */
  /* G */  "1.000 1.000 1.000 1.000 1.000 1.000 1.000 1.000 "  /* C */
  "</sequence>\n"
  "</curve>\n"
  "</gutenprint>\n"
};

static hue_curve_t picturemate_magenta_curve =
{
  "CyanCurve",
  "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
  "<gutenprint>\n"
  "<curve wrap=\"wrap\" type=\"linear\" gamma=\"0\">\n"
  "<sequence count=\"48\" lower-bound=\"0\" upper-bound=\"4\">\n"
  /* C */  "0.000 0.000 0.000 0.000 0.000 0.000 0.000 0.000 "  /* B */
  /* B */  "0.000 0.000 0.000 0.002 0.050 0.300 0.600 0.800 "  /* M */
  /* M */  "1.000 1.000 0.850 0.700 0.600 0.500 0.400 0.300 "  /* R */
  /* R */  "0.200 0.100 0.050 0.000 0.000 0.000 0.000 0.000 "  /* Y */
  /* Y */  "0.000 0.000 0.000 0.000 0.000 0.000 0.000 0.000 "  /* G */
  /* G */  "0.000 0.000 0.000 0.000 0.000 0.000 0.000 0.000 "  /* C */
  "</sequence>\n"
  "</curve>\n"
  "</gutenprint>\n"
};

static hue_curve_t picturemate_yellow_curve =
{
  "CyanCurve",
  "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
  "<gutenprint>\n"
  "<curve wrap=\"wrap\" type=\"linear\" gamma=\"0\">\n"
  "<sequence count=\"48\" lower-bound=\"0\" upper-bound=\"4\">\n"
  /* C */  "0.000 0.000 0.000 0.000 0.000 0.000 0.000 0.000 "  /* B */
  /* B */  "0.000 0.000 0.000 0.000 0.000 0.000 0.000 0.000 "  /* M */
  /* M */  "0.000 0.000 0.000 0.000 0.000 0.000 0.050 0.150 "  /* R */
  /* R */  "0.250 0.350 0.450 0.550 0.650 0.750 0.850 0.950 "  /* Y */
  /* Y */  "1.000 1.000 1.000 1.000 1.000 1.000 1.000 1.000 "  /* G */
  /* G */  "1.000 0.875 0.750 0.625 0.500 0.375 0.250 0.125 "  /* C */
  "</sequence>\n"
  "</curve>\n"
  "</gutenprint>\n"
};

static hue_curve_t picturemate_red_curve =
{
  "CyanCurve",
  "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
  "<gutenprint>\n"
  "<curve wrap=\"wrap\" type=\"linear\" gamma=\"0\">\n"
  "<sequence count=\"48\" lower-bound=\"0\" upper-bound=\"4\">\n"
  /* C */  "0.000 0.000 0.000 0.000 0.000 0.000 0.000 0.000 "  /* B */
  /* B */  "0.000 0.000 0.000 0.000 0.000 0.000 0.000 0.000 "  /* M */
  /* M */  "0.025 0.400 0.600 0.750 0.890 1.000 1.000 1.000 "  /* R */
  /* R */  "1.000 0.875 0.750 0.625 0.500 0.375 0.250 0.125 "  /* Y */
  /* Y */  "0.000 0.000 0.000 0.000 0.000 0.000 0.000 0.000 "  /* G */
  /* G */  "0.000 0.000 0.000 0.000 0.000 0.000 0.000 0.000 "  /* C */
  "</sequence>\n"
  "</curve>\n"
  "</gutenprint>\n"
};

static hue_curve_t picturemate_blue_curve =
{
  "CyanCurve",
  "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
  "<gutenprint>\n"
  "<curve wrap=\"wrap\" type=\"linear\" gamma=\"0\">\n"
  "<sequence count=\"48\" lower-bound=\"0\" upper-bound=\"4\">\n"
  /* C */  "0.000 0.250 0.475 0.700 0.810 0.875 0.940 1.000 "  /* B */
  /* B */  "1.000 0.975 0.930 0.875 0.810 0.740 0.650 0.400 "  /* M */
  /* M */  "0.040 0.002 0.000 0.000 0.000 0.000 0.000 0.000 "  /* R */
  /* R */  "0.000 0.000 0.000 0.000 0.000 0.000 0.000 0.000 "  /* Y */
  /* Y */  "0.000 0.000 0.000 0.000 0.000 0.000 0.000 0.000 "  /* G */
  /* G */  "0.000 0.000 0.000 0.000 0.000 0.000 0.000 0.000 "  /* C */
  "</sequence>\n"
  "</curve>\n"
  "</gutenprint>\n"
};


static const physical_subchannel_t standard_black_subchannels[] =
{
  { 0, -1, 0, "BlackDensity", NULL }
};

DECLARE_INK_CHANNEL(standard_black);

static const physical_subchannel_t f360_black_subchannels[] =
{
  { 0, 0, 1, "BlackDensity", NULL }
};

DECLARE_INK_CHANNEL(f360_black);

static const physical_subchannel_t x80_black_subchannels[] =
{
  { 0, -1, 48, "BlackDensity", NULL }
};

DECLARE_INK_CHANNEL(x80_black);

static const physical_subchannel_t c80_black_subchannels[] =
{
  { 0, -1, 0, "BlackDensity", NULL }
};

DECLARE_INK_CHANNEL(c80_black);

static const physical_subchannel_t c64_black_subchannels[] =
{
  { 0, -1, 0, "BlackDensity", NULL }
};

DECLARE_INK_CHANNEL(c64_black);

static const physical_subchannel_t standard_cyan_subchannels[] =
{
  { 2, -1, 0, "CyanDensity", NULL }
};

DECLARE_INK_CHANNEL(standard_cyan);

static const physical_subchannel_t f360_standard_cyan_subchannels[] =
{
  { 2, -1, 1, "CyanDensity", NULL }
};

DECLARE_INK_CHANNEL(f360_standard_cyan);

static const physical_subchannel_t r800_cyan_subchannels[] =
{
  { 2, -1, 1, "CyanDensity", NULL }
};

DECLARE_EXTENDED_INK_CHANNEL(r800_cyan);

static const physical_subchannel_t picturemate_cyan_subchannels[] =
{
  { 2, -1, 0, "CyanDensity", NULL }
};

DECLARE_EXTENDED_INK_CHANNEL(picturemate_cyan);

static const physical_subchannel_t cx3650_standard_cyan_subchannels[] =
{
  { 2, -1, 2, "CyanDensity", NULL }
};

DECLARE_INK_CHANNEL(cx3650_standard_cyan);

static const physical_subchannel_t x80_cyan_subchannels[] =
{
  { 2, -1, 96, "CyanDensity", NULL }
};

DECLARE_INK_CHANNEL(x80_cyan);

static const physical_subchannel_t c80_cyan_subchannels[] =
{
  { 2, -1, 0, "CyanDensity", NULL }
};

DECLARE_INK_CHANNEL(c80_cyan);

static const physical_subchannel_t c64_cyan_subchannels[] =
{
  { 2, -1, 0, "CyanDensity", NULL }
};

DECLARE_INK_CHANNEL(c64_cyan);

static const physical_subchannel_t standard_magenta_subchannels[] =
{
  { 1, -1, 0, "MagentaDensity", NULL }
};

DECLARE_INK_CHANNEL(standard_magenta);

static const physical_subchannel_t r800_magenta_subchannels[] =
{
  { 1, -1, 0, "MagentaDensity", NULL }
};

DECLARE_EXTENDED_INK_CHANNEL(r800_magenta);

static const physical_subchannel_t picturemate_magenta_subchannels[] =
{
  { 1, -1, 0, "MagentaDensity", NULL }
};

DECLARE_EXTENDED_INK_CHANNEL(picturemate_magenta);

static const physical_subchannel_t f360_standard_magenta_subchannels[] =
{
  { 1, -1, 1, "MagentaDensity", NULL }
};

DECLARE_INK_CHANNEL(f360_standard_magenta);

static const physical_subchannel_t x80_magenta_subchannels[] =
{
  { 1, -1, 48, "MagentaDensity", NULL }
};

DECLARE_INK_CHANNEL(x80_magenta);

static const physical_subchannel_t c80_magenta_subchannels[] =
{
  { 1, -1, 120, "MagentaDensity", NULL }
};

DECLARE_INK_CHANNEL(c80_magenta);

static const physical_subchannel_t c64_magenta_subchannels[] =
{
  { 1, -1, 90, "MagentaDensity", NULL }
};

DECLARE_INK_CHANNEL(c64_magenta);

static const physical_subchannel_t standard_yellow_subchannels[] =
{
  { 4, -1, 0, "YellowDensity", NULL }
};

DECLARE_INK_CHANNEL(standard_yellow);

static const physical_subchannel_t x80_yellow_subchannels[] =
{
  { 4, -1, 0, "YellowDensity", NULL }
};

DECLARE_INK_CHANNEL(x80_yellow);

static const physical_subchannel_t c80_yellow_subchannels[] =
{
  { 4, -1, 240, "YellowDensity", NULL }
};

DECLARE_INK_CHANNEL(c80_yellow);

static const physical_subchannel_t c64_yellow_subchannels[] =
{
  { 4, -1, 180, "YellowDensity", NULL }
};

DECLARE_INK_CHANNEL(c64_yellow);

static const physical_subchannel_t f360_standard_yellow_subchannels[] =
{
  { 4, -1, 1, "YellowDensity", NULL }
};

DECLARE_INK_CHANNEL(f360_standard_yellow);

static const physical_subchannel_t r800_yellow_subchannels[] =
{
  { 4, -1, 1, "YellowDensity", NULL }
};

DECLARE_EXTENDED_INK_CHANNEL(r800_yellow);

static const physical_subchannel_t picturemate_yellow_subchannels[] =
{
  { 4, -1, 0, "YellowDensity", NULL }
};

DECLARE_EXTENDED_INK_CHANNEL(picturemate_yellow);

static const physical_subchannel_t r800_red_subchannels[] =
{
  { 7, -1, 0, "RedDensity", NULL }
};

DECLARE_EXTENDED_INK_CHANNEL(r800_red);

static const physical_subchannel_t picturemate_red_subchannels[] =
{
  { 7, -1, 0, "RedDensity", NULL }
};

DECLARE_EXTENDED_INK_CHANNEL(picturemate_red);

static const physical_subchannel_t r800_blue_subchannels[] =
{
  { 8, -1, 1, "BlueDensity", NULL }
};

DECLARE_EXTENDED_INK_CHANNEL(r800_blue);

static const physical_subchannel_t picturemate_blue_subchannels[] =
{
  { 8, -1, 1, "BlueDensity", NULL }
};

DECLARE_EXTENDED_INK_CHANNEL(picturemate_blue);

static const physical_subchannel_t standard_gloss_subchannels[] =
{
  { 9, -1, 0, "GlossDensity", NULL }
};

DECLARE_INK_CHANNEL(standard_gloss);

static const physical_subchannel_t f360_gloss_subchannels[] =
{
  { 9, -1, 1, "GlossDensity", NULL }
};

DECLARE_INK_CHANNEL(f360_gloss);

static const physical_subchannel_t standard_photo_black_subchannels[] =
{
  { 0, 4, 0, "BlackDensity", NULL }
};

DECLARE_INK_CHANNEL(standard_photo_black);

static const physical_subchannel_t f360_photo_black_subchannels[] =
{
  { 0, 4, 1, "BlackDensity", NULL }
};

DECLARE_INK_CHANNEL(f360_photo_black);

static const physical_subchannel_t r800_matte_black_subchannels[] =
{
  { 0, 0, 0, "BlackDensity", NULL }
};

DECLARE_INK_CHANNEL(r800_matte_black);

static const physical_subchannel_t photo_black_subchannels[] =
{
  { 0, 0, 0, "BlackDensity", NULL }
};

DECLARE_INK_CHANNEL(photo_black);

static const physical_subchannel_t extended_black_subchannels[] =
{
  { 0, 1, 0, "BlackDensity", NULL }
};

DECLARE_INK_CHANNEL(extended_black);

static const physical_subchannel_t f360_extended_black_subchannels[] =
{
  { 0, 1, 1, "BlackDensity", NULL }
};

DECLARE_INK_CHANNEL(f360_extended_black);

static const physical_subchannel_t photo_cyan_subchannels[] =
{
  { 2, 0, 0, "CyanDensity", NULL },
  { 2, 1, 0, "CyanDensity", "LightCyanTransition" }
};

DECLARE_INK_CHANNEL(photo_cyan);

static const physical_subchannel_t extended_cyan_subchannels[] =
{
  { 2, 1, 0, "CyanDensity", NULL }
};

DECLARE_INK_CHANNEL(extended_cyan);

static const physical_subchannel_t f360_extended_cyan_subchannels[] =
{
  { 2, 1, 1, "CyanDensity", NULL }
};

DECLARE_INK_CHANNEL(f360_extended_cyan);

static const physical_subchannel_t photo_magenta_subchannels[] =
{
  { 1, 0, 0, "MagentaDensity", NULL },
  { 1, 1, 0, "MagentaDensity", "LightMagentaTransition" }
};

DECLARE_INK_CHANNEL(photo_magenta);

static const physical_subchannel_t extended_magenta_subchannels[] =
{
  { 1, 1, 0, "MagentaDensity", NULL }
};

DECLARE_INK_CHANNEL(extended_magenta);

static const physical_subchannel_t f360_extended_magenta_subchannels[] =
{
  { 1, 1, 1, "MagentaDensity", NULL }
};

DECLARE_INK_CHANNEL(f360_extended_magenta);

static const physical_subchannel_t photo_yellow_subchannels[] =
{
  { 4, 0, 0, "YellowDensity", NULL }
};

DECLARE_INK_CHANNEL(photo_yellow);

static const physical_subchannel_t f360_photo_yellow_subchannels[] =
{
  { 4, 0, 1, "YellowDensity", NULL }
};

DECLARE_INK_CHANNEL(f360_photo_yellow);

static const physical_subchannel_t j_extended_yellow_subchannels[] =
{
  { 4, 2, 0, "YellowDensity", NULL }
};

DECLARE_INK_CHANNEL(j_extended_yellow);

static const physical_subchannel_t extended_photo3_black_subchannels[] =
{
  { 0, 3, 0, "BlackDensity", NULL }
};

DECLARE_INK_CHANNEL(extended_photo3_black);

static const physical_subchannel_t f360_extended_photo3_black_subchannels[] =
{
  { 0, 3, 1, "BlackDensity", NULL }
};

DECLARE_INK_CHANNEL(f360_extended_photo3_black);

/* For Japanese 7-color printers, with dark yellow */
static const physical_subchannel_t photo2_yellow_subchannels[] =
{
  { 4, 2, 0, "YellowDensity", NULL },
  { 4, 0, 0, "YellowDensity", "DarkYellowTransition" }
};

DECLARE_INK_CHANNEL(photo2_yellow);

static const physical_subchannel_t f360_photo2_yellow_subchannels[] =
{
  { 4, 2, 0, "YellowDensity", NULL },
  { 4, 0, 1, "YellowDensity", "DarkYellowTransition" }
};

DECLARE_INK_CHANNEL(f360_photo2_yellow);

static const physical_subchannel_t photo2_black_subchannels[] =
{
  { 0, 0, 0, "BlackDensity", NULL },
  { 0, 1, 0, "BlackDensity", "GrayTransition" }
};

DECLARE_INK_CHANNEL(photo2_black);

static const physical_subchannel_t f360_photo2_black_subchannels[] =
{
  { 0, 0, 1, "BlackDensity", NULL },
  { 0, 1, 0, "BlackDensity", "GrayTransition" }
};

DECLARE_INK_CHANNEL(f360_photo2_black);

static const physical_subchannel_t photo3_black_subchannels[] =
{
  { 0, 0, 0, "BlackDensity", NULL },
  { 0, 1, 0, "BlackDensity", "DarkGrayTransition" },
  { 0, 3, 0, "BlackDensity", "LightGrayTransition" }
};

DECLARE_INK_CHANNEL(photo3_black);

static const physical_subchannel_t f360_photo3_black_subchannels[] =
{
  { 0, 0, 1, "BlackDensity", NULL },
  { 0, 1, 0, "BlackDensity", "DarkGrayTransition" },
  { 0, 3, 1, "BlackDensity", "LightGrayTransition" }
};

DECLARE_INK_CHANNEL(f360_photo3_black);

static const physical_subchannel_t quadtone_subchannels[] =
{
  { 0, -1, 0, "BlackDensity", NULL },
  { 2, -1, 0, "BlackDensity", "Gray3Transition" },
  { 1, -1, 0, "BlackDensity", "Gray2Transition" },
  { 4, -1, 0, "BlackDensity", "Gray1Transition" },
};

DECLARE_INK_CHANNEL(quadtone);

static const physical_subchannel_t c80_quadtone_subchannels[] =
{
  { 0, -1, 0, "BlackDensity", NULL },
  { 2, -1, 0, "BlackDensity", "Gray3Transition" },
  { 1, -1, 120, "BlackDensity", "Gray2Transition" },
  { 4, -1, 240, "BlackDensity", "Gray1Transition" },
};

DECLARE_INK_CHANNEL(c80_quadtone);

static const physical_subchannel_t c64_quadtone_subchannels[] =
{
  { 0, -1, 0, "BlackDensity", NULL },
  { 2, -1, 0, "BlackDensity", "Gray3Transition" },
  { 1, -1, 90, "BlackDensity", "Gray2Transition" },
  { 4, -1, 180, "BlackDensity", "Gray1Transition" },
};

DECLARE_INK_CHANNEL(c64_quadtone);

static const physical_subchannel_t f360_quadtone_subchannels[] =
{
  { 0, -1, 0, "BlackDensity", NULL },
  { 2, -1, 1, "BlackDensity", "Gray3Transition" },
  { 1, -1, 1, "BlackDensity", "Gray2Transition" },
  { 4, -1, 0, "BlackDensity", "Gray1Transition" },
};

DECLARE_INK_CHANNEL(f360_quadtone);

static const physical_subchannel_t cx3650_quadtone_subchannels[] =
{
  { 0, -1, 0, "BlackDensity", NULL },
  { 2, -1, 2, "BlackDensity", "Gray3Transition" },
  { 1, -1, 1, "BlackDensity", "Gray2Transition" },
  { 4, -1, 0, "BlackDensity", "Gray1Transition" },
};

DECLARE_INK_CHANNEL(cx3650_quadtone);

static const physical_subchannel_t f360_photo_cyan_subchannels[] =
{
  { 2, 0, 1, "CyanDensity", NULL },
  { 2, 1, 0, "CyanDensity", "LightCyanTransition" }
};

DECLARE_INK_CHANNEL(f360_photo_cyan);

static const physical_subchannel_t f360x_photo_cyan_subchannels[] =
{
  { 2, 0, 0, "CyanDensity", NULL },
  { 2, 1, 1, "CyanDensity", "LightCyanTransition" }
};

DECLARE_INK_CHANNEL(f360x_photo_cyan);

static const physical_subchannel_t f360_photo_magenta_subchannels[] =
{
  { 1, 0, 1, "MagentaDensity", NULL },
  { 1, 1, 0, "MagentaDensity", "LightMagentaTransition" }
};

DECLARE_INK_CHANNEL(f360_photo_magenta);

static const physical_subchannel_t f360x_photo_magenta_subchannels[] =
{
  { 1, 0, 0, "MagentaDensity", NULL },
  { 1, 1, 1, "MagentaDensity", "LightMagentaTransition" }
};

DECLARE_INK_CHANNEL(f360x_photo_magenta);

static const physical_subchannel_t claria_black_subchannels[] =
{
  { 0, 0, 0, "BlackDensity", NULL },
};

DECLARE_INK_CHANNEL(claria_black);

static const physical_subchannel_t claria_yellow_subchannels[] =
{
  { 4, 0, 2, "YellowDensity", NULL },
};

DECLARE_INK_CHANNEL(claria_yellow);

static const physical_subchannel_t claria_cyan_subchannels[] =
{
  { 2, 0, 0, "CyanDensity", NULL },
};

DECLARE_INK_CHANNEL(claria_cyan);

static const physical_subchannel_t claria_photo_cyan_subchannels[] =
{
  { 2, 0, 0, "CyanDensity", NULL },
  { 2, 1, 2, "CyanDensity", "LightCyanTransition" },
};

DECLARE_INK_CHANNEL(claria_photo_cyan);

static const physical_subchannel_t extended_claria_cyan_subchannels[] =
{
  { 2, 1, 2, "CyanDensity", "LightCyanTransition" },
};

DECLARE_INK_CHANNEL(extended_claria_cyan);

static const physical_subchannel_t claria_magenta_subchannels[] =
{
  { 1, 0, 2, "MagentaDensity", NULL },
};

DECLARE_INK_CHANNEL(claria_magenta);

static const physical_subchannel_t claria_photo_magenta_subchannels[] =
{
  { 1, 0, 2, "MagentaDensity", NULL },
  { 1, 1, 0, "MagentaDensity", "LightMagentaTransition" },
};

DECLARE_INK_CHANNEL(claria_photo_magenta);

static const physical_subchannel_t extended_claria_magenta_subchannels[] =
{
  { 1, 1, 0, "MagentaDensity", "LightMagentaTransition" },
};

DECLARE_INK_CHANNEL(extended_claria_magenta);


#define DECLARE_CHANNEL_SET(name)			\
static const channel_set_t name##_channel_set =		\
{							\
  #name " channel set",					\
  name##_channels,					\
  NULL,							\
  sizeof(name##_channels) / sizeof(ink_channel_t *),	\
  0							\
}

#define DECLARE_AUX_CHANNEL_SET(name, aux)		\
static const channel_set_t name##_##aux##_channel_set =	\
{							\
  #name " channel set",					\
  name##_channels,					\
  aux##_channels,					\
  sizeof(name##_channels) / sizeof(ink_channel_t *),	\
  sizeof(aux##_channels) / sizeof(ink_channel_t *),	\
}


/*
 ****************************************************************
 *                                                              *
 * Grayscale                                                    *
 *                                                              *
 ****************************************************************
 */

static const ink_channel_t *const standard_gloss_channels[] =
{
  &standard_gloss_channel
};

static const ink_channel_t *const f360_gloss_channels[] =
{
  &f360_gloss_channel
};

static const ink_channel_t *const standard_black_channels[] =
{
  &standard_black_channel
};

DECLARE_CHANNEL_SET(standard_black);

static const escp2_inkname_t stpi_escp2_default_black_inkset =
{
  "Gray", N_("Grayscale"), INKSET_CMYK,
  &standard_black_channel_set
};

static const ink_channel_t *const standard_photo_black_channels[] =
{
  &standard_photo_black_channel
};

DECLARE_CHANNEL_SET(standard_photo_black);

static const escp2_inkname_t stpi_escp2_default_photo_black_inkset =
{
  "Gray", N_("Grayscale"), INKSET_CMYK,
  &standard_photo_black_channel_set
};

static const ink_channel_t *const standard_photo_gloss_black_channels[] =
{
  &f360_photo_black_channel, &standard_gloss_channel
};

DECLARE_CHANNEL_SET(standard_photo_gloss_black);
DECLARE_AUX_CHANNEL_SET(standard_photo_black, standard_gloss);

static const escp2_inkname_t stpi_escp2_default_photo_gloss_black_inkset =
{
  "GrayG", N_("Grayscale"), INKSET_CMYK,
  &standard_photo_black_standard_gloss_channel_set
};


/*
 ****************************************************************
 *                                                              *
 * Two shade gray                                               *
 *                                                              *
 ****************************************************************
 */

static const ink_channel_t *const photo2_black_channels[] =
{
  &photo2_black_channel
};

DECLARE_CHANNEL_SET(photo2_black);

static const escp2_inkname_t two_color_grayscale_inkset =
{
  "Gray2", N_("Two Level Grayscale"), INKSET_CcMmYKk,
  &photo2_black_channel_set
};

static const ink_channel_t *const f360_photo2_black_channels[] =
{
  &f360_photo2_black_channel
};

DECLARE_CHANNEL_SET(f360_photo2_black);

static const escp2_inkname_t f360_two_color_grayscale_inkset =
{
  "Gray2", N_("Two Level Grayscale"), INKSET_CcMmYKk,
  &f360_photo2_black_channel_set
};


/*
 ****************************************************************
 *                                                              *
 * Three shade gray                                             *
 *                                                              *
 ****************************************************************
 */

static const ink_channel_t *const photo3_black_channels[] =
{
  &photo3_black_channel
};

DECLARE_CHANNEL_SET(photo3_black);

static const escp2_inkname_t three_color_grayscale_inkset =
{
  "Gray3", N_("Three Level Grayscale"), INKSET_CcMmYKk,
  &photo3_black_channel_set
};

static const ink_channel_t *const f360_photo3_black_channels[] =
{
  &f360_photo3_black_channel
};

DECLARE_CHANNEL_SET(f360_photo3_black);

static const escp2_inkname_t f360_three_color_grayscale_inkset =
{
  "Gray3", N_("Three Level Grayscale"), INKSET_CcMmYKk,
  &f360_photo3_black_channel_set
};


/*
 ****************************************************************
 *                                                              *
 * Quadtone gray                                                *
 *                                                              *
 ****************************************************************
 */

static const ink_channel_t *const quadtone_channels[] =
{
  &quadtone_channel
};

DECLARE_CHANNEL_SET(quadtone);

static const escp2_inkname_t generic_quadtone_inkset =
{
  "Quadtone", N_("Quadtone"), INKSET_QUADTONE,
  &quadtone_channel_set
};

static const ink_channel_t *const c80_quadtone_channels[] =
{
  &c80_quadtone_channel
};

DECLARE_CHANNEL_SET(c80_quadtone);

static const escp2_inkname_t c80_generic_quadtone_inkset =
{
  "Quadtone", N_("Quadtone"), INKSET_QUADTONE,
  &c80_quadtone_channel_set
};

static const ink_channel_t *const c64_quadtone_channels[] =
{
  &c64_quadtone_channel
};

DECLARE_CHANNEL_SET(c64_quadtone);

static const escp2_inkname_t c64_generic_quadtone_inkset =
{
  "Quadtone", N_("Quadtone"), INKSET_QUADTONE,
  &c64_quadtone_channel_set
};

static const ink_channel_t *const f360_quadtone_channels[] =
{
  &f360_quadtone_channel
};

DECLARE_CHANNEL_SET(f360_quadtone);

static const escp2_inkname_t f360_generic_quadtone_inkset =
{
  "Quadtone", N_("Quadtone"), INKSET_QUADTONE,
  &f360_quadtone_channel_set
};

static const ink_channel_t *const cx3650_quadtone_channels[] =
{
  &cx3650_quadtone_channel
};

DECLARE_CHANNEL_SET(cx3650_quadtone);

static const escp2_inkname_t cx3650_generic_quadtone_inkset =
{
  "Quadtone", N_("Quadtone"), INKSET_QUADTONE,
  &cx3650_quadtone_channel_set
};



/*
 ****************************************************************
 *                                                              *
 * Three color CMY                                              *
 *                                                              *
 ****************************************************************
 */

static const ink_channel_t *const standard_cmy_channels[] =
{
  NULL, &standard_cyan_channel,
  &standard_magenta_channel, &standard_yellow_channel
};

DECLARE_CHANNEL_SET(standard_cmy);

static const escp2_inkname_t three_color_composite_inkset =
{
  "RGB", N_("Three Color Composite"), INKSET_CMYK,
  &standard_cmy_channel_set
};

static const ink_channel_t *const x80_cmy_channels[] =
{
  NULL, &x80_cyan_channel,
  &x80_magenta_channel, &x80_yellow_channel
};

DECLARE_CHANNEL_SET(x80_cmy);

static const escp2_inkname_t x80_three_color_composite_inkset =
{
  "RGB", N_("Three Color Composite"), INKSET_CMYK,
  &x80_cmy_channel_set
};

static const ink_channel_t *const c80_cmy_channels[] =
{
  NULL, &c80_cyan_channel,
  &c80_magenta_channel, &c80_yellow_channel
};

DECLARE_CHANNEL_SET(c80_cmy);

static const escp2_inkname_t c80_three_color_composite_inkset =
{
  "RGB", N_("Three Color Composite"), INKSET_CMYK,
  &c80_cmy_channel_set
};

static const ink_channel_t *const c64_cmy_channels[] =
{
  NULL, &c64_cyan_channel,
  &c64_magenta_channel, &c64_yellow_channel
};

DECLARE_CHANNEL_SET(c64_cmy);

static const escp2_inkname_t c64_three_color_composite_inkset =
{
  "RGB", N_("Three Color Composite"), INKSET_CMYK,
  &c64_cmy_channel_set
};

static const ink_channel_t *const f360_cmy_channels[] =
{
  NULL, &f360_standard_cyan_channel,
  &f360_standard_magenta_channel, &standard_yellow_channel
};

DECLARE_CHANNEL_SET(f360_cmy);

static const escp2_inkname_t f360_three_color_composite_inkset =
{
  "RGB", N_("Three Color Composite"), INKSET_CMYK,
  &f360_cmy_channel_set
};

static const ink_channel_t *const cx3650_cmy_channels[] =
{
  NULL, &cx3650_standard_cyan_channel,
  &f360_standard_magenta_channel, &standard_yellow_channel
};

DECLARE_CHANNEL_SET(cx3650_cmy);

static const escp2_inkname_t cx3650_three_color_composite_inkset =
{
  "RGB", N_("Three Color Composite"), INKSET_CMYK,
  &cx3650_cmy_channel_set
};

static const ink_channel_t *const standard_gloss_cmy_channels[] =
{
  NULL, &f360_standard_cyan_channel,
  &standard_magenta_channel, &f360_standard_yellow_channel,
  &standard_gloss_channel
};

DECLARE_CHANNEL_SET(standard_gloss_cmy);

static const ink_channel_t *const r800_cmy_channels[] =
{
  NULL, &f360_standard_cyan_channel,
  &standard_magenta_channel, &f360_standard_yellow_channel
};

DECLARE_CHANNEL_SET(r800_cmy);
DECLARE_AUX_CHANNEL_SET(r800_cmy, standard_gloss);

static const escp2_inkname_t three_color_r800_gloss_inkset =
{
  "RGBG", N_("Three Color Composite"), INKSET_CMYK,
  &r800_cmy_standard_gloss_channel_set
};

static const escp2_inkname_t three_color_r800_composite_inkset =
{
  "RGBG", N_("Three Color Composite"), INKSET_CMYK,
  &r800_cmy_channel_set
};

static const ink_channel_t *const r2400_cmy_channels[] =
{
  NULL, &standard_cyan_channel,
  &f360_standard_magenta_channel, &standard_yellow_channel
};

DECLARE_CHANNEL_SET(r2400_cmy);

static const escp2_inkname_t three_color_r2400_composite_inkset =
{
  "CMY", N_("Three Color Composite"), INKSET_CMYK,
  &r2400_cmy_channel_set
};

static const ink_channel_t *const claria_cmy_channels[] =
{
  NULL, &claria_cyan_channel,
  &claria_magenta_channel, &claria_yellow_channel
};

DECLARE_CHANNEL_SET(claria_cmy);

static const escp2_inkname_t claria_three_color_composite_inkset =
{
  "RGB", N_("Three Color Composite"), INKSET_CMYK,
  &claria_cmy_channel_set
};

/*
 ****************************************************************
 *                                                              *
 * Four color CMYK                                              *
 *                                                              *
 ****************************************************************
 */

static const ink_channel_t *const standard_cmyk_channels[] =
{
  &standard_black_channel, &standard_cyan_channel,
  &standard_magenta_channel, &standard_yellow_channel
};

DECLARE_CHANNEL_SET(standard_cmyk);

static const escp2_inkname_t four_color_standard_inkset =
{
  "CMYK", N_("Four Color Standard"), INKSET_CMYK,
  &standard_cmyk_channel_set
};

static const ink_channel_t *const photo_cmyk_channels[] =
{
  &standard_photo_black_channel, &standard_cyan_channel,
  &standard_magenta_channel, &standard_yellow_channel
};

DECLARE_CHANNEL_SET(photo_cmyk);

static const escp2_inkname_t four_color_photo_inkset =
{
  "CMYK", N_("Four Color Standard"), INKSET_CMYK,
  &photo_cmyk_channel_set
};

static const ink_channel_t *const r800_cmyk_channels[] =
{
  &standard_black_channel, &f360_standard_cyan_channel,
  &standard_magenta_channel, &f360_standard_yellow_channel
};

DECLARE_CHANNEL_SET(r800_cmyk);

static const escp2_inkname_t four_color_r800_matte_inkset =
{
  "CMYKG", N_("Four Color Standard"), INKSET_CMYK,
  &r800_cmyk_channel_set
};

static const ink_channel_t *const r2400_cmyk_channels[] =
{
  &f360_black_channel, &standard_cyan_channel,
  &f360_standard_magenta_channel, &standard_yellow_channel
};

DECLARE_CHANNEL_SET(r2400_cmyk);

static const escp2_inkname_t four_color_r2400_standard_inkset =
{
  "CMYK", N_("Four Color Standard"), INKSET_CMYK,
  &r2400_cmyk_channel_set
};

static const ink_channel_t *const photo_gloss_cmyk_channels[] =
{
  &f360_photo_black_channel, &f360_standard_cyan_channel,
  &standard_magenta_channel, &f360_standard_yellow_channel,
  &standard_gloss_channel
};

DECLARE_CHANNEL_SET(photo_gloss_cmyk);

static const ink_channel_t *const r800_photo_cmyk_channels[] =
{
  &f360_photo_black_channel, &f360_standard_cyan_channel,
  &standard_magenta_channel, &f360_standard_yellow_channel,
};

DECLARE_CHANNEL_SET(r800_photo_cmyk);
DECLARE_AUX_CHANNEL_SET(r800_photo_cmyk, standard_gloss);

static const escp2_inkname_t four_color_r800_photo_gloss_inkset =
{
  "CMYKG", N_("Four Color Standard"), INKSET_CMYK,
  &r800_photo_cmyk_standard_gloss_channel_set
};

static const ink_channel_t *const x80_cmyk_channels[] =
{
  &x80_black_channel, &x80_cyan_channel,
  &x80_magenta_channel, &x80_yellow_channel
};

DECLARE_CHANNEL_SET(x80_cmyk);

static const escp2_inkname_t x80_four_color_standard_inkset =
{
  "CMYK", N_("Four Color Standard"), INKSET_CMYK,
  &x80_cmyk_channel_set
};

static const ink_channel_t *const c80_cmyk_channels[] =
{
  &c80_black_channel, &c80_cyan_channel,
  &c80_magenta_channel, &c80_yellow_channel
};

DECLARE_CHANNEL_SET(c80_cmyk);

static const escp2_inkname_t c80_four_color_standard_inkset =
{
  "CMYK", N_("Four Color Standard"), INKSET_CMYK,
  &c80_cmyk_channel_set
};

static const ink_channel_t *const c64_cmyk_channels[] =
{
  &c64_black_channel, &c64_cyan_channel,
  &c64_magenta_channel, &c64_yellow_channel
};

DECLARE_CHANNEL_SET(c64_cmyk);

static const escp2_inkname_t c64_four_color_standard_inkset =
{
  "CMYK", N_("Four Color Standard"), INKSET_CMYK,
  &c64_cmyk_channel_set
};

static const ink_channel_t *const f360_cmyk_channels[] =
{
  &standard_black_channel, &f360_standard_cyan_channel,
  &f360_standard_magenta_channel, &standard_yellow_channel
};

DECLARE_CHANNEL_SET(f360_cmyk);

static const escp2_inkname_t f360_four_color_standard_inkset =
{
  "CMYK", N_("Four Color Standard"), INKSET_CMYK,
  &f360_cmyk_channel_set
};

static const ink_channel_t *const cx3650_cmyk_channels[] =
{
  &standard_black_channel, &cx3650_standard_cyan_channel,
  &f360_standard_magenta_channel, &standard_yellow_channel
};

DECLARE_CHANNEL_SET(cx3650_cmyk);

static const escp2_inkname_t cx3650_four_color_standard_inkset =
{
  "CMYK", N_("Four Color Standard"), INKSET_CMYK,
  &cx3650_cmyk_channel_set
};

static const ink_channel_t *const claria_cmyk_channels[] =
{
  &claria_black_channel, &claria_cyan_channel,
  &claria_magenta_channel, &claria_yellow_channel
};

DECLARE_CHANNEL_SET(claria_cmyk);

static const escp2_inkname_t claria_four_color_standard_inkset =
{
  "CMYK", N_("Four Color Standard"), INKSET_CMYK,
  &claria_cmyk_channel_set
};


/*
 ****************************************************************
 *                                                              *
 * Five color CcMmY                                             *
 *                                                              *
 ****************************************************************
 */

static const ink_channel_t *const photo_composite_channels[] =
{
  NULL, &photo_cyan_channel,
  &photo_magenta_channel, &photo_yellow_channel
};

DECLARE_CHANNEL_SET(photo_composite);

static const escp2_inkname_t five_color_photo_composite_inkset =
{
  "PhotoCMY", N_("Five Color Photo Composite"), INKSET_CcMmYK,
  &photo_composite_channel_set
};

static const ink_channel_t *const f360_photo_composite_channels[] =
{
  NULL, &f360_photo_cyan_channel,
  &f360_photo_magenta_channel, &f360_photo_yellow_channel
};

DECLARE_CHANNEL_SET(f360_photo_composite);

static const escp2_inkname_t f360_five_color_photo_composite_inkset =
{
  "PhotoCMY", N_("Five Color Photo Composite"), INKSET_CcMmYK,
  &f360_photo_composite_channel_set
};

static const ink_channel_t *const five_color_photo3_channels[] =
{
  NULL, &f360x_photo_cyan_channel,
  &f360_photo_magenta_channel, &standard_yellow_channel
};

DECLARE_CHANNEL_SET(five_color_photo3);

static const escp2_inkname_t five_color_photo3_inkset =
{
  "PhotoCMY", N_("Five Color Photo Composite"), INKSET_CcMmYK,
  &five_color_photo3_channel_set
};

static const ink_channel_t *const claria_ccmmy_channels[] =
{
  NULL, &claria_photo_cyan_channel,
  &claria_photo_magenta_channel, &claria_yellow_channel
};

DECLARE_CHANNEL_SET(claria_ccmmy);

static const escp2_inkname_t claria_five_color_photo_composite_inkset =
{
  "PhotoCMY", N_("Five Color Photo Composite"), INKSET_CcMmYK,
  &claria_ccmmy_channel_set
};


/*
 ****************************************************************
 *                                                              *
 * Six color CcMmYK                                             *
 *                                                              *
 ****************************************************************
 */

static const ink_channel_t *const photo_channels[] =
{
  &photo_black_channel, &photo_cyan_channel,
  &photo_magenta_channel, &photo_yellow_channel
};

DECLARE_CHANNEL_SET(photo);

static const escp2_inkname_t six_color_photo_inkset =
{
  "PhotoCMYK", N_("Six Color Photo"), INKSET_CcMmYK,
  &photo_channel_set
};

static const ink_channel_t *const f360_photo_channels[] =
{
  &standard_black_channel, &f360_photo_cyan_channel,
  &f360_photo_magenta_channel, &f360_photo_yellow_channel
};

DECLARE_CHANNEL_SET(f360_photo);

static const escp2_inkname_t f360_six_color_photo_inkset =
{
  "PhotoCMYK", N_("Six Color Photo"), INKSET_CcMmYK,
  &f360_photo_channel_set
};

static const ink_channel_t *const six_color_photo3_channels[] =
{
  &f360_black_channel, &f360x_photo_cyan_channel,
  &f360_photo_magenta_channel, &standard_yellow_channel
};

DECLARE_CHANNEL_SET(six_color_photo3);

static const escp2_inkname_t six_color_photo3_inkset =
{
  "PhotoCMYK", N_("Six Color Photo"), INKSET_CcMmYK,
  &six_color_photo3_channel_set
};

static const ink_channel_t *const claria_ccmmyk_channels[] =
{
  &claria_black_channel, &claria_photo_cyan_channel,
  &claria_photo_magenta_channel, &claria_yellow_channel
};

DECLARE_CHANNEL_SET(claria_ccmmyk);

static const escp2_inkname_t claria_six_color_photo_inkset =
{
  "PhotoCMYK", N_("Six Color Photo"), INKSET_CcMmYK,
  &claria_ccmmyk_channel_set
};

/*
 ****************************************************************
 *                                                              *
 * Six color CcMmYy (Japan)                                     *
 *                                                              *
 ****************************************************************
 */

static const ink_channel_t *const photoj_composite_channels[] =
{
  NULL, &photo_cyan_channel,
  &photo_magenta_channel, &photo2_yellow_channel
};

DECLARE_CHANNEL_SET(photoj_composite);

static const escp2_inkname_t j_six_color_enhanced_composite_inkset =
{
  "PhotoEnhanceJ", N_("Six Color Enhanced Composite"), INKSET_CcMmYyK,
  &photoj_composite_channel_set
};

static const ink_channel_t *const f360_photoj_composite_channels[] =
{
  NULL, &f360_photo_cyan_channel,
  &f360_photo_magenta_channel, &f360_photo2_yellow_channel
};

DECLARE_CHANNEL_SET(f360_photoj_composite);

static const escp2_inkname_t f360_j_six_color_enhanced_composite_inkset =
{
  "PhotoEnhanceJ", N_("Six Color Enhanced Composite"), INKSET_CcMmYyK,
  &f360_photoj_composite_channel_set
};


/*
 ****************************************************************
 *                                                              *
 * Seven color CcMmYKk                                          *
 *                                                              *
 ****************************************************************
 */

static const ink_channel_t *const photo2_channels[] =
{
  &photo2_black_channel, &photo_cyan_channel,
  &photo_magenta_channel, &photo_yellow_channel
};

DECLARE_CHANNEL_SET(photo2);

static const escp2_inkname_t seven_color_enhanced_inkset =
{
  "PhotoCMYK7", N_("Seven Color Photo"), INKSET_CcMmYKk,
  &photo2_channel_set
};

static const ink_channel_t *const f360_photo2_channels[] =
{
  &f360_photo2_black_channel, &f360_photo_cyan_channel,
  &f360_photo_magenta_channel, &f360_photo_yellow_channel
};

DECLARE_CHANNEL_SET(f360_photo2);

static const escp2_inkname_t f360_seven_color_enhanced_inkset =
{
  "PhotoCMYK7", N_("Seven Color Photo"), INKSET_CcMmYKk,
  &f360_photo2_channel_set
};

static const ink_channel_t *const seven_color_photo3_channels[] =
{
  &f360_photo2_black_channel, &f360x_photo_cyan_channel,
  &f360_photo_magenta_channel, &standard_yellow_channel
};

DECLARE_CHANNEL_SET(seven_color_photo3);

static const escp2_inkname_t seven_color_photo3_inkset =
{
  "PhotoCMYK7", N_("Seven Color Photo"), INKSET_CcMmYKk,
  &seven_color_photo3_channel_set
};

/*
 ****************************************************************
 *                                                              *
 * Seven color CcMmYyK (Japan)                                  *
 *                                                              *
 ****************************************************************
 */

static const ink_channel_t *const photoj_channels[] =
{
  &photo_black_channel, &photo_cyan_channel,
  &photo_magenta_channel, &photo2_yellow_channel
};

DECLARE_CHANNEL_SET(photoj);

static const escp2_inkname_t j_seven_color_enhanced_inkset =
{
  "Photo7J", N_("Seven Color Enhanced"), INKSET_CcMmYyK,
  &photoj_channel_set
};

static const ink_channel_t *const f360_photoj_channels[] =
{
  &standard_black_channel, &f360_photo_cyan_channel,
  &f360_photo_magenta_channel, &f360_photo2_yellow_channel
};

DECLARE_CHANNEL_SET(f360_photoj);

static const escp2_inkname_t f360_j_seven_color_enhanced_inkset =
{
  "Photo7J", N_("Seven Color Photo"), INKSET_CcMmYKk,
  &f360_photoj_channel_set
};


/*
 ****************************************************************
 *                                                              *
 * Eight color CcMmYKkk                                         *
 *                                                              *
 ****************************************************************
 */

static const ink_channel_t *const photo3_channels[] =
{
  &f360_photo3_black_channel, &f360x_photo_cyan_channel,
  &f360_photo_magenta_channel, &standard_yellow_channel
};

DECLARE_CHANNEL_SET(photo3);

static const escp2_inkname_t eight_color_enhanced_inkset =
{
  "PhotoCMYK8", N_("Eight Color Photo"), INKSET_CcMmYKk,
  &photo3_channel_set
};


/*
 ****************************************************************
 *                                                              *
 * Five color CMYRB                                             *
 *                                                              *
 ****************************************************************
 */

static const ink_channel_t *const five_color_r800_channels[] =
{
  NULL, &r800_cyan_channel,
  &r800_magenta_channel, &r800_yellow_channel,
  &r800_red_channel, &r800_blue_channel
};

DECLARE_CHANNEL_SET(five_color_r800);

static const escp2_inkname_t five_color_r800_inkset =
{
  "CMYRB", N_("Five Color Photo Composite"), INKSET_CMYKRB,
  &five_color_r800_channel_set
};

static const ink_channel_t *const five_color_r800_photo_channels[] =
{
  NULL, &r800_cyan_channel,
  &r800_magenta_channel, &r800_yellow_channel,
  &r800_red_channel, &r800_blue_channel
};

DECLARE_CHANNEL_SET(five_color_r800_photo);

static const escp2_inkname_t five_color_r800_photo_inkset =
{
  "CMYRB", N_("Five Color Photo Composite"), INKSET_CMYKRB,
  &five_color_r800_photo_channel_set
};

static const ink_channel_t *const five_color_r800_photo_gloss_channels[] =
{
  NULL, &f360_standard_cyan_channel,
  &r800_magenta_channel, &r800_yellow_channel,
  &r800_red_channel, &r800_blue_channel,
  &standard_gloss_channel
};

DECLARE_CHANNEL_SET(five_color_r800_photo_gloss);
DECLARE_AUX_CHANNEL_SET(five_color_r800_photo, standard_gloss);

static const escp2_inkname_t five_color_r800_photo_gloss_inkset =
{
  "CMYRBG", N_("Five Color Photo Composite"), INKSET_CMYKRB,
  &five_color_r800_photo_standard_gloss_channel_set
};


/*
 ****************************************************************
 *                                                              *
 * Six color CMYKRB                                             *
 *                                                              *
 ****************************************************************
 */

static const ink_channel_t *const six_color_r800_channels[] =
{
  &standard_black_channel, &r800_cyan_channel,
  &r800_magenta_channel, &r800_yellow_channel,
  &r800_red_channel, &r800_blue_channel
};

DECLARE_CHANNEL_SET(six_color_r800);

static const escp2_inkname_t six_color_r800_inkset =
{
  "CMYKRB", N_("Six Color Photo"), INKSET_CMYKRB,
  &six_color_r800_channel_set
};

static const ink_channel_t *const six_color_r800_photo_channels[] =
{
  &f360_photo_black_channel, &r800_cyan_channel,
  &r800_magenta_channel, &r800_yellow_channel,
  &r800_red_channel, &r800_blue_channel
};

DECLARE_CHANNEL_SET(six_color_r800_photo);

static const escp2_inkname_t six_color_r800_photo_inkset =
{
  "CMYKRB", N_("Six Color Photo"), INKSET_CMYKRB,
  &six_color_r800_photo_channel_set
};

static const ink_channel_t *const six_color_picturemate_channels[] =
{
  &photo_black_channel, &picturemate_cyan_channel,
  &picturemate_magenta_channel, &picturemate_yellow_channel,
  &picturemate_red_channel, &picturemate_blue_channel
};

DECLARE_CHANNEL_SET(six_color_picturemate);

static const escp2_inkname_t six_color_picturemate_inkset =
{
  "CMYKRB", N_("Six Color Photo"), INKSET_CMYKRB,
  &six_color_picturemate_channel_set
};

static const ink_channel_t *const six_color_r800_photo_gloss_channels[] =
{
  &f360_photo_black_channel, &f360_standard_cyan_channel,
  &r800_magenta_channel, &r800_yellow_channel,
  &r800_red_channel, &r800_blue_channel,
  &standard_gloss_channel
};

DECLARE_CHANNEL_SET(six_color_r800_photo_gloss);
DECLARE_AUX_CHANNEL_SET(six_color_r800_photo, standard_gloss);

static const escp2_inkname_t six_color_r800_photo_gloss_inkset =
{
  "CMYKRBG", N_("Six Color Photo"), INKSET_CMYKRB,
  &six_color_r800_photo_standard_gloss_channel_set
};


/*
 ****************************************************************
 *                                                              *
 * Extended (raw)                                               *
 *                                                              *
 ****************************************************************
 */

static const ink_channel_t *const one_color_extended_channels[] =
{
  &standard_black_channel
};
DECLARE_CHANNEL_SET(one_color_extended);

static const escp2_inkname_t one_color_extended_inkset =
{
  "PhysicalBlack", N_("One Color Raw"), INKSET_EXTENDED,
  &one_color_extended_channel_set
};

static const escp2_inkname_t one_color_photo_extended_inkset =
{
  "PhysicalBlack", N_("One Color Raw"), INKSET_EXTENDED,
  &standard_photo_black_channel_set
};

static const escp2_inkname_t one_color_r800_photo_gloss_extended_inkset =
{
  "PhysicalBlackGloss", N_("One Color Raw Enhanced Gloss"), INKSET_EXTENDED,
  &standard_photo_gloss_black_channel_set
};


static const ink_channel_t *const two_color_extended_channels[] =
{
  &photo_black_channel, &extended_black_channel
};
DECLARE_CHANNEL_SET(two_color_extended);

static const escp2_inkname_t two_color_extended_inkset =
{
  "PhysicalBlack2", N_("Two Color Raw"), INKSET_EXTENDED,
  &two_color_extended_channel_set
};

static const ink_channel_t *const f360_two_color_extended_channels[] =
{
  &standard_black_channel, &extended_black_channel
};
DECLARE_CHANNEL_SET(f360_two_color_extended);

static const escp2_inkname_t f360_two_color_extended_inkset =
{
  "PhysicalBlack2", N_("Two Color Raw"), INKSET_EXTENDED,
  &f360_two_color_extended_channel_set
};


static const ink_channel_t *const standard_three_color_extended_channels[] =
{
  &standard_cyan_channel, &standard_magenta_channel, &standard_yellow_channel
};

DECLARE_CHANNEL_SET(standard_three_color_extended);

static const escp2_inkname_t three_color_extended_inkset =
{
  "PhysicalCMY", N_("Three Color Raw"), INKSET_EXTENDED,
  &standard_three_color_extended_channel_set
};

static const ink_channel_t *const r800_cmy_extended_channels[] =
{
  &f360_standard_cyan_channel, &standard_magenta_channel,
  &f360_standard_yellow_channel
};

DECLARE_CHANNEL_SET(r800_cmy_extended);

static const escp2_inkname_t three_color_r800_extended_inkset =
{
  "PhysicalCMY", N_("Three Color Raw"), INKSET_EXTENDED,
  &r800_cmy_extended_channel_set
};

static const ink_channel_t *const r800_cmy_gloss_extended_channels[] =
{
  &f360_standard_cyan_channel, &standard_magenta_channel,
  &f360_standard_yellow_channel, &standard_gloss_channel
};

DECLARE_CHANNEL_SET(r800_cmy_gloss_extended);

static const escp2_inkname_t three_color_r800_gloss_extended_inkset =
{
  "PhysicalCMY", N_("Three Color Raw Gloss"), INKSET_EXTENDED,
  &r800_cmy_gloss_extended_channel_set
};

static const ink_channel_t *const x80_three_color_extended_channels[] =
{
  &x80_cyan_channel, &x80_magenta_channel, &x80_yellow_channel
};

DECLARE_CHANNEL_SET(x80_three_color_extended);

static const escp2_inkname_t x80_three_color_extended_inkset =
{
  "PhysicalCMY", N_("Three Color Raw"), INKSET_EXTENDED,
  &x80_three_color_extended_channel_set
};

static const ink_channel_t *const c80_three_color_extended_channels[] =
{
  &c80_cyan_channel, &c80_magenta_channel, &c80_yellow_channel
};

DECLARE_CHANNEL_SET(c80_three_color_extended);

static const escp2_inkname_t c80_three_color_extended_inkset =
{
  "PhysicalCMY", N_("Three Color Raw"), INKSET_EXTENDED,
  &c80_three_color_extended_channel_set
};

static const ink_channel_t *const c64_three_color_extended_channels[] =
{
  &c64_cyan_channel, &c64_magenta_channel, &c64_yellow_channel
};

DECLARE_CHANNEL_SET(c64_three_color_extended);

static const escp2_inkname_t c64_three_color_extended_inkset =
{
  "PhysicalCMY", N_("Three Color Raw"), INKSET_EXTENDED,
  &c64_three_color_extended_channel_set
};

static const ink_channel_t *const f360_three_color_extended_channels[] =
{
  &f360_standard_cyan_channel, &f360_standard_magenta_channel,
  &standard_yellow_channel
};

DECLARE_CHANNEL_SET(f360_three_color_extended);

static const escp2_inkname_t f360_three_color_extended_inkset =
{
  "PhysicalCMY", N_("Three Color Raw"), INKSET_EXTENDED,
  &f360_three_color_extended_channel_set
};

static const ink_channel_t *const cx3650_three_color_extended_channels[] =
{
  &cx3650_standard_cyan_channel, &f360_standard_magenta_channel,
  &standard_yellow_channel
};

DECLARE_CHANNEL_SET(cx3650_three_color_extended);

static const escp2_inkname_t cx3650_three_color_extended_inkset =
{
  "PhysicalCMY", N_("Three Color Raw"), INKSET_EXTENDED,
  &cx3650_three_color_extended_channel_set
};

static const ink_channel_t *const claria_three_color_extended_channels[] =
{
  &claria_cyan_channel, &claria_magenta_channel, &claria_yellow_channel
};

DECLARE_CHANNEL_SET(claria_three_color_extended);

static const escp2_inkname_t claria_three_color_extended_inkset =
{
  "PhysicalCMY", N_("Three Color Raw"), INKSET_EXTENDED,
  &claria_three_color_extended_channel_set
};


static const escp2_inkname_t four_color_extended_inkset =
{
  "PhysicalCMYK", N_("Four Color Raw"), INKSET_EXTENDED,
  &standard_cmyk_channel_set
};

static const escp2_inkname_t four_color_photo_extended_inkset =
{
  "PhysicalCMYK", N_("Four Color Raw"), INKSET_EXTENDED,
  &photo_cmyk_channel_set
};

static const escp2_inkname_t x80_four_color_extended_inkset =
{
  "PhysicalCMYK", N_("Four Color Raw"), INKSET_EXTENDED,
  &x80_cmyk_channel_set
};

static const escp2_inkname_t c80_four_color_extended_inkset =
{
  "PhysicalCMYK", N_("Four Color Raw"), INKSET_EXTENDED,
  &c80_cmyk_channel_set
};

static const escp2_inkname_t c64_four_color_extended_inkset =
{
  "PhysicalCMYK", N_("Four Color Raw"), INKSET_EXTENDED,
  &c64_cmyk_channel_set
};

static const escp2_inkname_t f360_four_color_extended_inkset =
{
  "PhysicalCMYK", N_("Four Color Raw"), INKSET_EXTENDED,
  &f360_cmyk_channel_set
};

static const escp2_inkname_t cx3650_four_color_extended_inkset =
{
  "PhysicalCMYK", N_("Four Color Raw"), INKSET_EXTENDED,
  &cx3650_cmyk_channel_set
};

static const escp2_inkname_t claria_four_color_extended_inkset =
{
  "PhysicalCMYK", N_("Four Color Raw"), INKSET_EXTENDED,
  &claria_cmyk_channel_set
};

static const escp2_inkname_t four_color_r800_extended_inkset =
{
  "PhysicalCMYKGloss", N_("Four Color Raw"), INKSET_EXTENDED,
  &r800_cmyk_channel_set
};

static const escp2_inkname_t four_color_r800_photo_gloss_extended_inkset =
{
  "PhysicalCMYKGloss", N_("Four Color Raw Gloss"), INKSET_EXTENDED,
  &photo_gloss_cmyk_channel_set
};


static const ink_channel_t *const five_color_extended_channels[] =
{
  &standard_cyan_channel, &extended_cyan_channel,
  &standard_magenta_channel, &extended_magenta_channel,
  &photo_yellow_channel
};
DECLARE_CHANNEL_SET(five_color_extended);

static const escp2_inkname_t five_color_extended_inkset =
{
  "PhysicalCcMmY", N_("Five Color Raw"), INKSET_EXTENDED,
  &five_color_extended_channel_set
};

static const ink_channel_t *const f360_five_color_extended_channels[] =
{
  &f360_standard_cyan_channel, &extended_cyan_channel,
  &f360_standard_magenta_channel, &extended_magenta_channel,
  &f360_photo_yellow_channel
};
DECLARE_CHANNEL_SET(f360_five_color_extended);

static const escp2_inkname_t f360_five_color_extended_inkset =
{
  "PhysicalCcMmY", N_("Five Color Raw"), INKSET_EXTENDED,
  &f360_five_color_extended_channel_set
};

static const ink_channel_t *const claria_five_color_extended_channels[] =
{
  &claria_cyan_channel, &extended_claria_cyan_channel,
  &claria_magenta_channel, &extended_claria_magenta_channel,
  &claria_yellow_channel
};
DECLARE_CHANNEL_SET(claria_five_color_extended);

static const escp2_inkname_t claria_five_color_extended_inkset =
{
  "PhysicalCcMmYK", N_("Six Color Raw"), INKSET_EXTENDED,
  &claria_five_color_extended_channel_set
};


static const ink_channel_t *const six_color_extended_channels[] =
{
  &photo_black_channel,
  &standard_cyan_channel, &extended_cyan_channel,
  &standard_magenta_channel, &extended_magenta_channel,
  &photo_yellow_channel
};
DECLARE_CHANNEL_SET(six_color_extended);

static const escp2_inkname_t six_color_extended_inkset =
{
  "PhysicalCcMmYK", N_("Six Color Raw"), INKSET_EXTENDED,
  &six_color_extended_channel_set
};

static const escp2_inkname_t six_color_r800_extended_inkset =
{
  "PhysicalCMYKRB", N_("Six Color Raw"), INKSET_EXTENDED,
  &six_color_r800_channel_set
};

static const escp2_inkname_t six_color_picturemate_extended_inkset =
{
  "PhysicalCMYKRB", N_("Six Color Raw"), INKSET_EXTENDED,
  &six_color_picturemate_channel_set
};

static const ink_channel_t *const f360_six_color_extended_channels[] =
{
  &standard_black_channel,
  &f360_standard_cyan_channel, &extended_cyan_channel,
  &f360_standard_magenta_channel, &extended_magenta_channel,
  &f360_photo_yellow_channel
};
DECLARE_CHANNEL_SET(f360_six_color_extended);

static const escp2_inkname_t f360_six_color_extended_inkset =
{
  "PhysicalCcMmYK", N_("Six Color Raw"), INKSET_EXTENDED,
  &f360_six_color_extended_channel_set
};

static const escp2_inkname_t six_color_r800_photo_gloss_extended_inkset =
{
  "PhysicalCMYKRB", N_("Six Color Enhanced Gloss Raw"), INKSET_EXTENDED,
  &six_color_r800_photo_gloss_channel_set
};

static const ink_channel_t *const claria_six_color_extended_channels[] =
{
  &claria_black_channel,
  &claria_cyan_channel, &extended_claria_cyan_channel,
  &claria_magenta_channel, &extended_claria_magenta_channel,
  &claria_yellow_channel
};
DECLARE_CHANNEL_SET(claria_six_color_extended);

static const escp2_inkname_t claria_six_color_extended_inkset =
{
  "PhysicalCcMmYK", N_("Six Color Raw"), INKSET_EXTENDED,
  &claria_six_color_extended_channel_set
};


static const ink_channel_t *const j_seven_color_extended_channels[] =
{
  &photo_black_channel,
  &standard_cyan_channel, &extended_cyan_channel,
  &standard_magenta_channel, &extended_magenta_channel,
  &photo_yellow_channel, &j_extended_yellow_channel
};
DECLARE_CHANNEL_SET(j_seven_color_extended);

static const escp2_inkname_t j_seven_color_extended_inkset =
{
  "PhysicalCcMmYyK", N_("Seven Color Raw"), INKSET_EXTENDED,
  &j_seven_color_extended_channel_set
};

static const ink_channel_t *const seven_color_extended_channels[] =
{
  &photo_black_channel, &extended_black_channel,
  &standard_cyan_channel, &extended_cyan_channel,
  &standard_magenta_channel, &extended_magenta_channel,
  &photo_yellow_channel
};
DECLARE_CHANNEL_SET(seven_color_extended);

static const escp2_inkname_t seven_color_extended_inkset =
{
  "PhysicalCcMmYKk", N_("Seven Color Raw"), INKSET_EXTENDED,
  &seven_color_extended_channel_set
};

static const ink_channel_t *const f360_seven_color_extended_channels[] =
{
  &standard_black_channel, &extended_black_channel,
  &f360_standard_cyan_channel, &extended_cyan_channel,
  &f360_standard_magenta_channel, &extended_magenta_channel,
  &f360_photo_yellow_channel
};
DECLARE_CHANNEL_SET(f360_seven_color_extended);

static const escp2_inkname_t f360_seven_color_extended_inkset =
{
  "PhysicalCcMmYKk", N_("Seven Color Raw"), INKSET_EXTENDED,
  &f360_seven_color_extended_channel_set
};


static const ink_channel_t *const seven_color_r800_gloss_extended_channels[] =
{
  &standard_black_channel, &f360_photo_black_channel,
  &r800_cyan_channel, &r800_magenta_channel,
  &r800_yellow_channel, &r800_red_channel,
  &r800_blue_channel, &standard_gloss_channel
};

DECLARE_CHANNEL_SET(seven_color_r800_gloss_extended);

static const escp2_inkname_t seven_color_r800_gloss_extended_inkset =
{
  "PhysicalCMYKPRB", N_("Seven Color Enhanced Gloss Raw"), INKSET_EXTENDED,
  &seven_color_r800_gloss_extended_channel_set
};


static const ink_channel_t *const eight_color_extended_channels[] =
{
  &f360_black_channel, &extended_black_channel,
  &f360_extended_photo3_black_channel,
  &standard_cyan_channel, &f360_extended_cyan_channel,
  &f360_standard_magenta_channel, &extended_magenta_channel,
  &standard_yellow_channel
};

DECLARE_CHANNEL_SET(eight_color_extended);

static const escp2_inkname_t eight_color_extended_inkset =
{
  "PhysicalCMYKkk", N_("Eight Color Raw"), INKSET_EXTENDED,
  &eight_color_extended_channel_set
};


static const shade_set_t standard_shades =
{
  { 1, { 1.0 }},		/* K */
  { 1, { 1.0 }},		/* C */
  { 1, { 1.0 }},		/* M */
  { 1, { 1.0 }},		/* Y */
  { 1, { 1.0 }},		/* Extended 5 */
  { 1, { 1.0 }},		/* Extended 6 */
  { 1, { 1.0 }},		/* Extended 7 */
  { 1, { 1.0 }},		/* Extended 8 */
};

static const shade_set_t photo_gen1_shades =	/* Stylus 750 and older */
{
  { 1, { 1.0 }},
  { 2, { 1.0, 0.305 }},
  { 2, { 1.0, 0.315 }},
  { 1, { 1.0 }},
  { 1, { 1.0 }},
  { 1, { 1.0 }},
  { 1, { 1.0 }},
  { 1, { 1.0 }},
};

static const shade_set_t photo_gen2_shades =	/* Stylus 870 and newer */
{
  { 1, { 1.0 }},
  { 2, { 1.0, 0.29 }},
  { 2, { 1.0, 0.29 }},
  { 1, { 1.0 }},
  { 1, { 1.0 }},
  { 1, { 1.0 }},
  { 1, { 1.0 }},
  { 1, { 1.0 }},
};

static const shade_set_t photo_gen3_shades =	/* Stylus R300 and newer */
{
  { 1, { 1.0 }},
  { 2, { 1.0, 0.35 }},
  { 2, { 1.0, 0.35 }},
  { 1, { 1.0 }},
  { 1, { 1.0 }},
  { 1, { 1.0 }},
  { 1, { 1.0 }},
  { 1, { 1.0 }},
};

static const shade_set_t esp960_shades =	/* Epson 950/960/PM-950C/PM-970C */
{
  { 1, { 1.0 }},
  { 2, { 1.0, 0.316 }},
  { 2, { 1.0, 0.34 }},
  { 2, { 1.0, 0.5 }},
  { 1, { 1.0 }},
  { 1, { 1.0 }},
  { 1, { 1.0 }},
  { 1, { 1.0 }},
};

static const shade_set_t stp2000_shades =	/* Stylus Photo 2000 */
{
  { 1, { 1.0 }},
  { 2, { 1.0, 0.227 }},		/* Just a guess */
  { 2, { 1.0, 0.227 }},		/* Just a guess */
  { 1, { 1.0 }},
  { 1, { 1.0 }},
  { 1, { 1.0 }},
  { 1, { 1.0 }},
  { 1, { 1.0 }},
};

static const shade_set_t ultrachrome_photo_shades =	/* Ultrachrome with photo black ink */
{
  { 2, { 1.0, 0.48 }},
  { 2, { 1.0, 0.33 }},
  { 2, { 1.0, 0.25 }},
  { 1, { 1.0 }},
  { 1, { 1.0 }},
  { 1, { 1.0 }},
  { 1, { 1.0 }},
  { 1, { 1.0 }},
};

static const shade_set_t ultrachrome_matte_shades =	/* Ultrachrome with matte black ink */
{
  { 2, { 1.0, 0.33 }},
  { 2, { 1.0, 0.33 }},
  { 2, { 1.0, 0.25 }},
  { 1, { 1.0 }},
  { 1, { 1.0 }},
  { 1, { 1.0 }},
  { 1, { 1.0 }},
  { 1, { 1.0 }},
};

static const shade_set_t ultra3_photo_shades =	/* R2400 with photo black ink */
{
  { 3, { 1.0, 0.48, 0.16 }},
  { 2, { 1.0, 0.35 }},
  { 2, { 1.0, 0.20 }},
  { 1, { 1.0 }},
  { 1, { 1.0 }},
  { 1, { 1.0 }},
  { 1, { 1.0 }},
  { 1, { 1.0 }},
};

static const shade_set_t ultra3_matte_shades =	/* R2400 with matte black ink */
{
  { 3, { 1.0, 0.278, 0.093 }},
  { 2, { 1.0, 0.35 }},
  { 2, { 1.0, 0.20 }},
  { 1, { 1.0 }},
  { 1, { 1.0 }},
  { 1, { 1.0 }},
  { 1, { 1.0 }},
  { 1, { 1.0 }},
};

static const shade_set_t quadtone_shades =	/* Some kind of quadtone ink */
{
  { 4, { 1.0, 0.75, 0.5, 0.25 }},
  { 1, { 1.0 }},
  { 1, { 1.0 }},
  { 1, { 1.0 }},
  { 1, { 1.0 }},
  { 1, { 1.0 }},
  { 1, { 1.0 }},
  { 1, { 1.0 }},
};

static const shade_set_t claria_shades =	/* Stylus R260 and newer */
{
  { 1, { 1.0 }},
  { 2, { 1.0, 0.35 }},
  { 2, { 1.0, 0.33 }},
  { 1, { 1.0 }},
  { 1, { 1.0 }},
  { 1, { 1.0 }},
  { 1, { 1.0 }},
  { 1, { 1.0 }},
};

#define DECLARE_INKLIST(tname, name, inks, text, papers, adjustments, shades) \
static const inklist_t name##_inklist =					      \
{									      \
  tname,								      \
  text,									      \
  inks##_ink_types,							      \
  #papers,								      \
  #adjustments,								      \
  &shades##_shades,							      \
  sizeof(inks##_ink_types) / sizeof(escp2_inkname_t *),			      \
}


static const escp2_inkname_t *const cmy_ink_types[] =
{
  &three_color_composite_inkset
};

DECLARE_INKLIST("None", cmy, cmy, N_("EPSON Standard Inks"),
		standard, standard, standard);


static const escp2_inkname_t *const standard_ink_types[] =
{
  &four_color_standard_inkset,
  &three_color_composite_inkset,
  &one_color_extended_inkset,
  &three_color_extended_inkset,
  &four_color_extended_inkset,
};

DECLARE_INKLIST("None", standard, standard, N_("EPSON Standard Inks"),
		standard, standard, standard);
DECLARE_INKLIST("None", photo_gen3_4, standard, N_("EPSON Standard Inks"),
		standard, photo3, standard);

static const escp2_inkname_t *const quadtone_ink_types[] =
{
  &generic_quadtone_inkset,
};

DECLARE_INKLIST("quadtone", quadtone, quadtone, N_("Quadtone"),
		standard, standard, quadtone);

static const escp2_inkname_t *const c80_ink_types[] =
{
  &c80_four_color_standard_inkset,
  &c80_three_color_composite_inkset,
  &one_color_extended_inkset,
  &c80_three_color_extended_inkset,
  &c80_four_color_extended_inkset,
};

DECLARE_INKLIST("None", c80, c80, N_("EPSON Standard Inks"),
		durabrite, durabrite, standard);

DECLARE_INKLIST("None", c82, c80, N_("EPSON Standard Inks"),
		durabrite2, durabrite2, standard);

static const escp2_inkname_t *const c80_quadtone_ink_types[] =
{
  &c80_generic_quadtone_inkset,
};

DECLARE_INKLIST("Quadtone", c80_quadtone, c80_quadtone, N_("Quadtone"),
		standard, standard, quadtone);

static const escp2_inkname_t *const c64_ink_types[] =
{
  &c64_four_color_standard_inkset,
  &c64_three_color_composite_inkset,
  &one_color_extended_inkset,
  &c64_three_color_extended_inkset,
  &c64_four_color_extended_inkset,
};

DECLARE_INKLIST("None", c64, c64, N_("EPSON Standard Inks"),
		durabrite2, durabrite2, standard);

static const escp2_inkname_t *const c64_quadtone_ink_types[] =
{
  &c64_generic_quadtone_inkset,
};

DECLARE_INKLIST("Quadtone", c64_quadtone, c64_quadtone, N_("Quadtone"),
		standard, standard, quadtone);

static const escp2_inkname_t *const f360_ink_types[] =
{
  &f360_four_color_standard_inkset,
  &f360_three_color_composite_inkset,
  &one_color_extended_inkset,
  &f360_three_color_extended_inkset,
  &f360_four_color_extended_inkset,
};

DECLARE_INKLIST("None", f360, f360, N_("EPSON Standard Inks"),
		durabrite2, durabrite2, standard);

static const escp2_inkname_t *const f360_quadtone_ink_types[] =
{
  &f360_generic_quadtone_inkset,
};

DECLARE_INKLIST("Quadtone", f360_quadtone, f360_quadtone, N_("Quadtone"),
		standard, standard, quadtone);

static const escp2_inkname_t *const cx3650_ink_types[] =
{
  &cx3650_four_color_standard_inkset,
  &cx3650_three_color_composite_inkset,
  &one_color_extended_inkset,
  &cx3650_three_color_extended_inkset,
  &cx3650_four_color_extended_inkset,
};

DECLARE_INKLIST("None", cx3650, cx3650, N_("EPSON Standard Inks"),
		durabrite2, durabrite2, standard);

static const escp2_inkname_t *const cx3650_quadtone_ink_types[] =
{
  &cx3650_generic_quadtone_inkset,
};

DECLARE_INKLIST("Quadtone", cx3650_quadtone, cx3650_quadtone, N_("Quadtone"),
		standard, standard, quadtone);

static const escp2_inkname_t *const x80_ink_types[] =
{
  &x80_four_color_standard_inkset,
  &x80_three_color_composite_inkset,
  &one_color_extended_inkset,
  &x80_three_color_extended_inkset,
  &x80_four_color_extended_inkset,
};

DECLARE_INKLIST("None", x80, x80, N_("EPSON Standard Inks"),
		standard, standard, standard);

static const escp2_inkname_t *const photo_ink_types[] =
{
  &six_color_photo_inkset,
  &five_color_photo_composite_inkset,
  &four_color_standard_inkset,
  &three_color_composite_inkset,
  &one_color_extended_inkset,
  &three_color_extended_inkset,
  &four_color_extended_inkset,
  &five_color_extended_inkset,
  &six_color_extended_inkset,
};

DECLARE_INKLIST("None", gen1, photo, N_("EPSON Standard Inks"),
		standard, photo, photo_gen1);
DECLARE_INKLIST("None", photo_gen2, photo, N_("EPSON Standard Inks"),
		standard, photo2, photo_gen2);
DECLARE_INKLIST("None", photo_gen3, photo, N_("EPSON Standard Inks"),
		standard, photo3, photo_gen3);
DECLARE_INKLIST("None", pigment, photo, N_("EPSON Standard Inks"),
		ultrachrome, ultrachrome_photo, stp2000);

static const escp2_inkname_t *const f360_photo_ink_types[] =
{
  &f360_six_color_photo_inkset,
  &f360_five_color_photo_composite_inkset,
  &f360_four_color_standard_inkset,
  &three_color_composite_inkset,
  &one_color_extended_inkset,
  &three_color_extended_inkset,
  &f360_four_color_extended_inkset,
  &f360_five_color_extended_inkset,
  &f360_six_color_extended_inkset,
};

DECLARE_INKLIST("None", f360_photo, f360_photo, N_("EPSON Standard Inks"),
		standard, sp960, esp960);

static const escp2_inkname_t *const claria_ink_types[] =
{
  &claria_six_color_photo_inkset,
  &claria_five_color_photo_composite_inkset,
  &claria_four_color_standard_inkset,
  &claria_three_color_composite_inkset,
  &one_color_extended_inkset,
  &claria_three_color_extended_inkset,
  &claria_four_color_extended_inkset,
  &claria_five_color_extended_inkset,
  &claria_six_color_extended_inkset,
};

DECLARE_INKLIST("None", claria, claria, N_("EPSON Standard Inks"),
		standard, claria, claria);

static const escp2_inkname_t *const f360_photo7_japan_ink_types[] =
{
  &f360_j_seven_color_enhanced_inkset,
  &f360_j_six_color_enhanced_composite_inkset,
  &f360_six_color_photo_inkset,
  &f360_five_color_photo_composite_inkset,
  &four_color_standard_inkset,
  &three_color_composite_inkset,
  &one_color_extended_inkset,
  &three_color_extended_inkset,
  &four_color_extended_inkset,
  &f360_five_color_extended_inkset,
  &f360_six_color_extended_inkset,
  &f360_seven_color_extended_inkset,
};

DECLARE_INKLIST("None", f360_photo7_japan, f360_photo7_japan,
		N_("EPSON Standard Inks"), standard, sp960, esp960);

static const escp2_inkname_t *const f360_photo7_ink_types[] =
{
  &f360_seven_color_enhanced_inkset,
  &f360_six_color_photo_inkset,
  &f360_five_color_photo_composite_inkset,
  &four_color_standard_inkset,
  &three_color_composite_inkset,
  &f360_two_color_grayscale_inkset,
  &one_color_extended_inkset,
  &f360_two_color_extended_inkset,
  &three_color_extended_inkset,
  &four_color_extended_inkset,
  &f360_five_color_extended_inkset,
  &f360_six_color_extended_inkset,
  &f360_seven_color_extended_inkset,
};

DECLARE_INKLIST("ultraphoto", f360_ultra_photo7, f360_photo7,
		N_("UltraChrome Photo Black"), ultrachrome,
		ultrachrome_photo, ultrachrome_photo);

DECLARE_INKLIST("ultramatte", f360_ultra_matte7, f360_photo7,
		N_("UltraChrome Matte Black"), ultrachrome,
		ultrachrome_matte, ultrachrome_matte);

static const escp2_inkname_t *const photo7_ink_types[] =
{
  &seven_color_enhanced_inkset,
  &six_color_photo_inkset,
  &five_color_photo_composite_inkset,
  &four_color_standard_inkset,
  &three_color_composite_inkset,
  &two_color_grayscale_inkset,
  &one_color_extended_inkset,
  &two_color_extended_inkset,
  &three_color_extended_inkset,
  &four_color_extended_inkset,
  &five_color_extended_inkset,
  &six_color_extended_inkset,
  &seven_color_extended_inkset,
};

DECLARE_INKLIST("ultraphoto", ultra_photo7, photo7,
		N_("UltraChrome Photo Black"), ultrachrome,
		ultrachrome_photo, ultrachrome_photo);

DECLARE_INKLIST("ultramatte", ultra_matte7, photo7,
		N_("UltraChrome Matte Black"), ultrachrome,
		ultrachrome_matte, ultrachrome_matte);

static const escp2_inkname_t *const f360_photo8_ink_types[] =
{
  &eight_color_enhanced_inkset,
  &seven_color_photo3_inkset,
  &six_color_photo3_inkset,
  &five_color_photo3_inkset,
  &four_color_r2400_standard_inkset,
  &three_color_r2400_composite_inkset,
  &eight_color_extended_inkset,
};

DECLARE_INKLIST("ultra3photo", f360_ultra_photo8, f360_photo8,
		N_("Photo Black"), ultrachrome_k3,
		ultrachrome_k3_photo, ultra3_photo);

DECLARE_INKLIST("ultra3matte", f360_ultra_matte8, f360_photo8,
		N_("Matte Black"), ultrachrome_k3,
		ultrachrome_k3_matte, ultra3_matte);

static const escp2_inkname_t *const cmykrb_matte_ink_types[] =
{
  &six_color_r800_inkset,
  &five_color_r800_inkset,
  &four_color_r800_matte_inkset,
  &three_color_r800_composite_inkset,
  &one_color_extended_inkset,
  &three_color_r800_extended_inkset,
  &four_color_r800_extended_inkset,
  &six_color_r800_extended_inkset,
};

DECLARE_INKLIST("cmykrbmatte", cmykrb_matte, cmykrb_matte,
		N_("Matte Black"), r800, r800_matte, standard);

static const escp2_inkname_t *const cmykrb_photo_ink_types[] =
{
  &six_color_r800_photo_gloss_inkset,
  &five_color_r800_photo_gloss_inkset,
  &four_color_r800_photo_gloss_inkset,
  &three_color_r800_gloss_inkset,
  &one_color_extended_inkset,
  &one_color_r800_photo_gloss_extended_inkset,
  &three_color_r800_extended_inkset,
  &three_color_r800_gloss_extended_inkset,
  &four_color_r800_photo_gloss_extended_inkset,
  &six_color_r800_photo_gloss_extended_inkset,
  &seven_color_r800_gloss_extended_inkset
};

DECLARE_INKLIST("cmykrbphoto", cmykrb_photo, cmykrb_photo,
		N_("Photo Black"), r800, r800_photo, standard);

static const escp2_inkname_t *const picturemate_photo_ink_types[] =
{
  &six_color_picturemate_inkset,
  &six_color_picturemate_extended_inkset,
};

DECLARE_INKLIST("picturemate", picturemate, picturemate_photo,
		N_("Standard"), picturemate, picturemate, standard);


#define DECLARE_INKGROUP(name)			\
static const inkgroup_t name##_inkgroup =	\
{						\
  #name,					\
  name##_group,					\
  sizeof(name##_group) / sizeof(inklist_t *),	\
}

static const inklist_t *const cmy_group[] =
{
  &cmy_inklist
};

DECLARE_INKGROUP(cmy);

static const inklist_t *const standard_group[] =
{
  &standard_inklist,
  &quadtone_inklist
};

DECLARE_INKGROUP(standard);

static const inklist_t *const c80_group[] =
{
  &c80_inklist,
  &c80_quadtone_inklist
};

DECLARE_INKGROUP(c80);

static const inklist_t *const c82_group[] =
{
  &c82_inklist,
  &c80_quadtone_inklist
};

DECLARE_INKGROUP(c82);

static const inklist_t *const c64_group[] =
{
  &c64_inklist,
  &c64_quadtone_inklist
};

DECLARE_INKGROUP(c64);

static const inklist_t *const f360_group[] =
{
  &f360_inklist,
  &f360_quadtone_inklist
};

DECLARE_INKGROUP(f360);

static const inklist_t *const cx3650_group[] =
{
  &cx3650_inklist,
  &cx3650_quadtone_inklist
};

DECLARE_INKGROUP(cx3650);

static const inklist_t *const x80_group[] =
{
  &x80_inklist
};

DECLARE_INKGROUP(x80);

static const inklist_t *const photo_gen1_group[] =
{
  &gen1_inklist,
  &quadtone_inklist
};

DECLARE_INKGROUP(photo_gen1);

static const inklist_t *const photo_gen2_group[] =
{
  &photo_gen2_inklist,
  &quadtone_inklist
};

DECLARE_INKGROUP(photo_gen2);

static const inklist_t *const photo_gen3_group[] =
{
  &photo_gen3_inklist,
  &quadtone_inklist
};

DECLARE_INKGROUP(photo_gen3);

static const inklist_t *const photo_gen3_4_group[] =
{
  &photo_gen3_4_inklist,
  &quadtone_inklist
};

DECLARE_INKGROUP(photo_gen3_4);

static const inklist_t *const photo_pigment_group[] =
{
  &pigment_inklist
};

DECLARE_INKGROUP(photo_pigment);

static const inklist_t *const f360_photo_group[] =
{
  &f360_photo_inklist
};

DECLARE_INKGROUP(f360_photo);

static const inklist_t *const f360_photo7_japan_group[] =
{
  &f360_photo7_japan_inklist
};

DECLARE_INKGROUP(f360_photo7_japan);

static const inklist_t *const f360_ultrachrome_group[] =
{
  &f360_ultra_photo7_inklist,
  &f360_ultra_matte7_inklist
};

DECLARE_INKGROUP(f360_ultrachrome);

static const inklist_t *const ultrachrome_group[] =
{
  &ultra_photo7_inklist,
  &ultra_matte7_inklist
};

DECLARE_INKGROUP(ultrachrome);

static const inklist_t *const f360_ultrachrome_k3_group[] =
{
  &f360_ultra_photo8_inklist,
  &f360_ultra_matte8_inklist
};

DECLARE_INKGROUP(f360_ultrachrome_k3);

static const inklist_t *const cmykrb_group[] =
{
  &cmykrb_photo_inklist,
  &cmykrb_matte_inklist
};

DECLARE_INKGROUP(cmykrb);

static const inklist_t *const picturemate_group[] =
{
  &picturemate_inklist,
};

DECLARE_INKGROUP(picturemate);

static const inklist_t *const claria_group[] =
{
  &claria_inklist,
};

DECLARE_INKGROUP(claria);

typedef struct
{
  const char *name;
  const inkgroup_t *inkgroup;
} ink_t;

static const ink_t the_inks[] =
{
  { "cmy", &cmy_inkgroup },
  { "standard", &standard_inkgroup },
  { "c80", &c80_inkgroup },
  { "c82", &c82_inkgroup },
  { "c64", &c64_inkgroup },
  { "f360", &f360_inkgroup },
  { "cx3650", &cx3650_inkgroup },
  { "x80", &x80_inkgroup },
  { "photo_gen1", &photo_gen1_inkgroup },
  { "photo_gen2", &photo_gen2_inkgroup },
  { "photo_gen3", &photo_gen3_inkgroup },
  { "photo_gen3_4", &photo_gen3_4_inkgroup },
  { "photo_pigment", &photo_pigment_inkgroup },
  { "ultrachrome", &ultrachrome_inkgroup },
  { "f360_photo", &f360_photo_inkgroup },
  { "f360_photo7_japan", &f360_photo7_japan_inkgroup },
  { "f360_ultrachrome", &f360_ultrachrome_inkgroup },
  { "f360_ultrachrome_k3", &f360_ultrachrome_k3_inkgroup },
  { "cmykrb", &cmykrb_inkgroup },
  { "picturemate", &picturemate_inkgroup },
  { "claria", &claria_inkgroup },
};

const inkgroup_t *
stpi_escp2_get_inkgroup_named(const char *n)
{
  int i;
  if (n)
    for (i = 0; i < sizeof(the_inks) / sizeof(ink_t); i++)
      {
	if (strcmp(n, the_inks[i].name) == 0)
	  return the_inks[i].inkgroup;
      }
  return NULL;
}

const escp2_inkname_t *
stpi_escp2_get_default_black_inkset(void)
{
  return &stpi_escp2_default_black_inkset;
}


#define DECLARE_CHANNEL_LIST(name)			\
static const channel_name_t name##_channel_name_list =	\
{							\
  #name,						\
  sizeof(name##_channel_names) / sizeof(const char *),	\
  name##_channel_names					\
}

static const char *standard_channel_names[] =
{
  N_("Black"),
  N_("Cyan"),
  N_("Magenta"),
  N_("Yellow")
};

DECLARE_CHANNEL_LIST(standard);

static const char *cx3800_channel_names[] =
{
  N_("Cyan"),
  N_("Yellow"),
  N_("Magenta"),
  N_("Black")
};

DECLARE_CHANNEL_LIST(cx3800);

static const char *mfp2005_channel_names[] =
{
  N_("Cyan"),
  N_("Magenta"),
  N_("Yellow"),
  N_("Black")
};

DECLARE_CHANNEL_LIST(mfp2005);

static const char *photo_channel_names[] =
{
  N_("Black"),
  N_("Cyan"),
  N_("Magenta"),
  N_("Yellow"),
  N_("Light Cyan"),
  N_("Light Magenta"),
};

DECLARE_CHANNEL_LIST(photo);

static const char *rx700_channel_names[] =
{
  N_("Black"),
  N_("Cyan"),
  N_("Light Cyan"),
  N_("Magenta"),
  N_("Light Magenta"),
  N_("Yellow"),
};

DECLARE_CHANNEL_LIST(rx700);

static const char *sp2200_channel_names[] =
{
  N_("Black"),
  N_("Cyan"),
  N_("Magenta"),
  N_("Yellow"),
  N_("Light Cyan"),
  N_("Light Magenta"),
  N_("Light Black"),
};

DECLARE_CHANNEL_LIST(sp2200);

static const char *pm_950c_channel_names[] =
{
  N_("Black"),
  N_("Cyan"),
  N_("Magenta"),
  N_("Yellow"),
  N_("Light Cyan"),
  N_("Light Magenta"),
  N_("Dark Yellow"),
};

DECLARE_CHANNEL_LIST(pm_950c);

static const char *sp960_channel_names[] =
{
  N_("Black"),
  N_("Cyan"),
  N_("Magenta"),
  N_("Yellow"),
  N_("Light Cyan"),
  N_("Light Magenta"),
  N_("Black"),
};

DECLARE_CHANNEL_LIST(sp960);

static const char *r800_channel_names[] =
{
  N_("Yellow"),
  N_("Magenta"),
  N_("Cyan"),
  N_("Matte Black"),
  N_("Photo Black"),
  N_("Red"),
  N_("Blue"),
  N_("Gloss Optimizer"),
};

DECLARE_CHANNEL_LIST(r800);

static const char *picturemate_channel_names[] =
{
  N_("Yellow"),
  N_("Magenta"),
  N_("Cyan"),
  N_("Black"),
  N_("Red"),
  N_("Blue"),
};

DECLARE_CHANNEL_LIST(picturemate);

static const char *r2400_channel_names[] =
{
  N_("Light Light Black"),
  N_("Light Magenta"),
  N_("Light Cyan"),
  N_("Light Black"),
  N_("Black"),
  N_("Cyan"),
  N_("Magenta"),
  N_("Yellow"),
};

DECLARE_CHANNEL_LIST(r2400);

typedef struct
{
  const char *name;
  const channel_name_t *channel_name;
} channel_t;

static const channel_t the_channels[] =
{
  { "cx3800", &cx3800_channel_name_list },
  { "mfp2005", &mfp2005_channel_name_list },
  { "photo", &photo_channel_name_list },
  { "picturemate", &picturemate_channel_name_list },
  { "pm_950c", &pm_950c_channel_name_list },
  { "r2400", &r2400_channel_name_list },
  { "r800", &r800_channel_name_list },
  { "rx700", &rx700_channel_name_list },
  { "sp2200", &sp2200_channel_name_list },
  { "sp960", &sp960_channel_name_list },
  { "standard", &standard_channel_name_list },
};

const channel_name_t *
stpi_escp2_get_channel_names_named(const char *n)
{
  int i;
  if (n)
    for (i = 0; i < sizeof(the_channels) / sizeof(channel_t); i++)
      {
	if (strcmp(n, the_channels[i].name) == 0)
	  return the_channels[i].channel_name;
      }
  return NULL;
}

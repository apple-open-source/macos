/*
 * "$Id: escp2-channels.c,v 1.1.1.2 2004/12/22 23:49:38 jlovell Exp $"
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
#include <gimp-print/gimp-print.h>
#include "gimp-print-internal.h"
#include <gimp-print/gimp-print-intl-internal.h>
#include "print-escp2.h"


#define DECLARE_INK_CHANNEL(name)				\
static const ink_channel_t name##_channel =			\
{								\
  #name,							\
  name##_subchannels,						\
  sizeof(name##_subchannels) / sizeof(physical_subchannel_t),	\
}

static const physical_subchannel_t standard_black_subchannels[] =
{
  { 0, -1, 0, "BlackDensity", NULL }
};

DECLARE_INK_CHANNEL(standard_black);

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

static const physical_subchannel_t standard_red_subchannels[] =
{
  { 7, -1, 0, "RedDensity", NULL }
};

DECLARE_INK_CHANNEL(standard_red);

static const physical_subchannel_t standard_blue_subchannels[] =
{
  { 8, -1, 0, "BlueDensity", NULL }
};

DECLARE_INK_CHANNEL(standard_blue);

static const physical_subchannel_t standard_gloss_subchannels[] =
{
  { 9, -1, 0, "GlossDensity", NULL }
};

DECLARE_INK_CHANNEL(standard_gloss);

static const physical_subchannel_t standard_photo_black_subchannels[] =
{
  { 0, 4, 0, "PhotoBlackDensity", NULL }
};

DECLARE_INK_CHANNEL(standard_photo_black);

static const physical_subchannel_t photo_black_subchannels[] =
{
  { 0, 0, 0, "BlackDensity", NULL }
};

DECLARE_INK_CHANNEL(photo_black);

static const physical_subchannel_t f360_photo_black_subchannels[] =
{
  { 0, 0, 0, "BlackDensity", NULL }
};

DECLARE_INK_CHANNEL(f360_photo_black);

static const physical_subchannel_t extended_black_subchannels[] =
{
  { 0, 1, 0, "BlackDensity", NULL }
};

DECLARE_INK_CHANNEL(extended_black);

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

static const physical_subchannel_t f360_photo_cyan_subchannels[] =
{
  { 2, 0, 1, "CyanDensity", NULL },
  { 2, 1, 0, "CyanDensity", "LightCyanTransition" }
};

DECLARE_INK_CHANNEL(f360_photo_cyan);

static const physical_subchannel_t f360_photo_magenta_subchannels[] =
{
  { 1, 0, 1, "MagentaDensity", NULL },
  { 1, 1, 0, "MagentaDensity", "LightMagentaTransition" }
};

DECLARE_INK_CHANNEL(f360_photo_magenta);


#define DECLARE_CHANNEL_SET(name)			\
static const channel_set_t name##_channel_set =		\
{							\
  #name " channel set",					\
  name##_channels,					\
  sizeof(name##_channels) / sizeof(ink_channel_t *),	\
}


/*
 ****************************************************************
 *                                                              *
 * Grayscale                                                    *
 *                                                              *
 ****************************************************************
 */

static const ink_channel_t *const standard_black_channels[] =
{
  &standard_black_channel
};

DECLARE_CHANNEL_SET(standard_black);

const escp2_inkname_t stpi_escp2_default_black_inkset =
{
  "Gray", N_("Grayscale"), INKSET_CMYK,
  &standard_black_channel_set
};

static const ink_channel_t *const standard_photo_black_channels[] =
{
  &standard_photo_black_channel
};

DECLARE_CHANNEL_SET(standard_photo_black);

const escp2_inkname_t stpi_escp2_default_photo_black_inkset =
{
  "Gray", N_("Grayscale"), INKSET_CMYK,
  &standard_photo_black_channel_set
};

static const ink_channel_t *const standard_gloss_black_channels[] =
{
  &standard_black_channel, &standard_gloss_channel
};

DECLARE_CHANNEL_SET(standard_gloss_black);

const escp2_inkname_t stpi_escp2_default_gloss_black_inkset =
{
  "GrayG", N_("Grayscale Enhanced Gloss"), INKSET_CMYK,
  &standard_gloss_black_channel_set
};

static const ink_channel_t *const standard_photo_gloss_black_channels[] =
{
  &standard_photo_black_channel, &standard_gloss_channel
};

DECLARE_CHANNEL_SET(standard_photo_gloss_black);

const escp2_inkname_t stpi_escp2_default_photo_gloss_black_inkset =
{
  "GrayG", N_("Grayscale Enhanced Gloss"), INKSET_CMYK,
  &standard_photo_gloss_black_channel_set
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

static const ink_channel_t *const standard_gloss_cmy_channels[] =
{
  NULL, &standard_cyan_channel,
  &standard_magenta_channel, &standard_yellow_channel,
  &standard_gloss_channel
};

DECLARE_CHANNEL_SET(standard_gloss_cmy);

static const escp2_inkname_t three_color_composite_gloss_inkset =
{
  "RGBG", N_("Three Color Composite Enhanced Gloss"), INKSET_CMYK,
  &standard_gloss_cmy_channel_set
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

static const ink_channel_t *const gloss_cmyk_channels[] =
{
  &standard_black_channel, &standard_cyan_channel,
  &standard_magenta_channel, &standard_yellow_channel,
  &standard_gloss_channel
};

DECLARE_CHANNEL_SET(gloss_cmyk);

static const escp2_inkname_t four_color_gloss_inkset =
{
  "CMYKG", N_("Four Color Standard Enhanced Gloss"), INKSET_CMYK,
  &gloss_cmyk_channel_set
};

static const ink_channel_t *const photo_gloss_cmyk_channels[] =
{
  &standard_photo_black_channel, &standard_cyan_channel,
  &standard_magenta_channel, &standard_yellow_channel,
  &standard_gloss_channel
};

DECLARE_CHANNEL_SET(photo_gloss_cmyk);

static const escp2_inkname_t four_color_photo_gloss_inkset =
{
  "CMYKG", N_("Four Color Standard Enhanced Gloss"), INKSET_CMYK,
  &photo_gloss_cmyk_channel_set
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
  &f360_photo_black_channel, &f360_photo_cyan_channel,
  &f360_photo_magenta_channel, &f360_photo_yellow_channel
};

DECLARE_CHANNEL_SET(f360_photo);

static const escp2_inkname_t f360_six_color_photo_inkset =
{
  "PhotoCMYK", N_("Six Color Photo"), INKSET_CcMmYK,
  &f360_photo_channel_set
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
  &f360_photo_black_channel, &f360_photo_cyan_channel,
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
 * Six color CMYKRB                                             *
 *                                                              *
 ****************************************************************
 */

static const ink_channel_t *const standard_cmykrb_channels[] =
{
  &standard_black_channel, &standard_cyan_channel,
  &standard_magenta_channel, &standard_yellow_channel,
  &standard_red_channel, &standard_blue_channel
};

DECLARE_CHANNEL_SET(standard_cmykrb);

static const escp2_inkname_t standard_cmykrb_inkset =
{
  "CMYKRB", N_("Six Color Photo"), INKSET_CMYKRB,
  &standard_cmykrb_channel_set
};

static const ink_channel_t *const photo_cmykrb_channels[] =
{
  &standard_photo_black_channel, &standard_cyan_channel,
  &standard_magenta_channel, &standard_yellow_channel,
  &standard_red_channel, &standard_blue_channel
};

DECLARE_CHANNEL_SET(photo_cmykrb);

static const escp2_inkname_t photo_cmykrb_inkset =
{
  "CMYKRB", N_("Six Color Photo"), INKSET_CMYKRB,
  &photo_cmykrb_channel_set
};

static const ink_channel_t *const gloss_cmykrb_channels[] =
{
  &standard_black_channel, &standard_cyan_channel,
  &standard_magenta_channel, &standard_yellow_channel,
  &standard_red_channel, &standard_blue_channel,
  &standard_gloss_channel
};

DECLARE_CHANNEL_SET(gloss_cmykrb);

static const escp2_inkname_t gloss_cmykrb_inkset =
{
  "CMYKRBG", N_("Six Color Photo Enhanced Gloss"), INKSET_CMYKRB,
  &gloss_cmykrb_channel_set
};

static const ink_channel_t *const photo_gloss_cmykrb_channels[] =
{
  &standard_photo_black_channel, &standard_cyan_channel,
  &standard_magenta_channel, &standard_yellow_channel,
  &standard_red_channel, &standard_blue_channel,
  &standard_gloss_channel
};

DECLARE_CHANNEL_SET(photo_gloss_cmykrb);

static const escp2_inkname_t photo_gloss_cmykrb_inkset =
{
  "CMYKRBG", N_("Six Color Photo Enhanced Gloss"), INKSET_CMYKRB,
  &photo_gloss_cmykrb_channel_set
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

static const escp2_inkname_t one_color_extended_gloss_inkset =
{
  "PhysicalBlackGloss", N_("One Color Raw Enhanced Gloss"), INKSET_EXTENDED,
  &standard_gloss_black_channel_set
};

static const escp2_inkname_t one_color_photo_extended_gloss_inkset =
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
  &f360_photo_black_channel, &extended_black_channel
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

static const escp2_inkname_t four_color_gloss_extended_inkset =
{
  "PhysicalCMYKGloss", N_("Four Color Raw Gloss"), INKSET_EXTENDED,
  &gloss_cmyk_channel_set
};

static const escp2_inkname_t four_color_photo_gloss_extended_inkset =
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

static const escp2_inkname_t six_color_cmykrb_extended_inkset =
{
  "PhysicalCMYKRB", N_("Six Color Raw"), INKSET_EXTENDED,
  &standard_cmykrb_channel_set
};

static const escp2_inkname_t six_color_cmykrb_photo_extended_inkset =
{
  "PhysicalCMYKRB", N_("Six Color Raw"), INKSET_EXTENDED,
  &photo_cmykrb_channel_set
};

static const ink_channel_t *const f360_six_color_extended_channels[] =
{
  &f360_photo_black_channel,
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


static const escp2_inkname_t six_color_cmykrb_gloss_extended_inkset =
{
  "PhysicalCMYKRB", N_("Six Color Enhanced Gloss Raw"), INKSET_EXTENDED,
  &gloss_cmykrb_channel_set
};

static const escp2_inkname_t six_color_cmykrb_photo_gloss_extended_inkset =
{
  "PhysicalCMYKRB", N_("Six Color Enhanced Gloss Raw"), INKSET_EXTENDED,
  &photo_gloss_cmykrb_channel_set
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
  &f360_photo_black_channel, &extended_black_channel,
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


static const ink_channel_t *const gloss_cmykprb_extended_channels[] =
{
  &standard_black_channel, &standard_photo_black_channel,
  &standard_cyan_channel, &standard_magenta_channel,
  &standard_yellow_channel, &standard_red_channel,
  &standard_blue_channel, &standard_gloss_channel
};

DECLARE_CHANNEL_SET(gloss_cmykprb_extended);

static const escp2_inkname_t seven_color_cmykprb_gloss_extended_inkset =
{
  "PhysicalCMYKPRB", N_("Seven Color Enhanced Gloss Raw"), INKSET_EXTENDED,
  &gloss_cmykprb_extended_channel_set
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

#define DECLARE_INKLIST(tname, name, inks, text, papers, adjustments, shades) \
static const inklist_t name##_inklist =					      \
{									      \
  tname,								      \
  text,									      \
  inks##_ink_types,							      \
  &stpi_escp2_##papers##_paper_list,					      \
  &stpi_escp2_##adjustments##_paper_adjustment_list,			      \
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
		durabrite, durabrite, standard);

static const escp2_inkname_t *const c64_quadtone_ink_types[] =
{
  &c64_generic_quadtone_inkset,
};

DECLARE_INKLIST("Quadtone", c64_quadtone, c64_quadtone, N_("Quadtone"),
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

static const escp2_inkname_t *const cmykrb_matte_ink_types[] =
{
/*  &gloss_cmykrb_inkset, */
  &standard_cmykrb_inkset,
/*  &four_color_gloss_inkset, */
  &four_color_standard_inkset,
/*  &three_color_composite_gloss_inkset, */
  &three_color_composite_inkset,
  &one_color_extended_inkset,
  &one_color_extended_gloss_inkset,
  &three_color_extended_inkset,
  &four_color_extended_inkset,
  &four_color_gloss_extended_inkset,
  &six_color_cmykrb_extended_inkset,
  &six_color_cmykrb_gloss_extended_inkset,
  &seven_color_cmykprb_gloss_extended_inkset
};

DECLARE_INKLIST("cmykrbmatte", cmykrb_matte, cmykrb_matte,
		N_("Matte Black"),
		standard, standard, standard);

static const escp2_inkname_t *const cmykrb_photo_ink_types[] =
{
/*  &photo_gloss_cmykrb_inkset, */
  &photo_cmykrb_inkset,
/*   &four_color_photo_gloss_inkset, */
  &four_color_photo_inkset,
/*   &three_color_composite_gloss_inkset, */
  &three_color_composite_inkset,
  &one_color_photo_extended_inkset,
  &one_color_photo_extended_gloss_inkset,
  &three_color_extended_inkset,
  &four_color_photo_extended_inkset,
  &four_color_photo_gloss_extended_inkset,
  &six_color_cmykrb_photo_extended_inkset,
  &six_color_cmykrb_photo_gloss_extended_inkset,
  &seven_color_cmykprb_gloss_extended_inkset
};

DECLARE_INKLIST("cmykrbphoto", cmykrb_photo, cmykrb_photo,
		N_("Photo Black"),
		standard, standard, standard);


#define DECLARE_INKGROUP(name)			\
const inkgroup_t stpi_escp2_##name##_inkgroup =	\
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

static const inklist_t *const c64_group[] =
{
  &c64_inklist,
  &c64_quadtone_inklist
};

DECLARE_INKGROUP(c64);

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

static const inklist_t *const cmykrb_group[] =
{
  &cmykrb_matte_inklist,
  &cmykrb_photo_inklist
};

DECLARE_INKGROUP(cmykrb);

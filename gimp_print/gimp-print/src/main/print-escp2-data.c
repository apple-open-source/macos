/*
 * "$Id: print-escp2-data.c,v 1.1.1.1 2003/01/27 19:05:32 jlovell Exp $"
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

static const double standard_sat_adjustment[49] =
{
  1.0,  1.1,  1.2,  1.3,  1.4,  1.5,  1.6,  1.7, /* C */
  1.8,  1.9,  1.9,  1.9,  1.7,  1.5,  1.3,  1.1, /* B */
  1.0,  1.0,  1.0,  1.0,  1.0,  1.0,  1.0,  1.0, /* M */
  1.0,  1.0,  1.0,  1.0,  1.0,  1.0,  1.0,  1.0, /* R */
  1.0,  1.0,  1.0,  1.1,  1.2,  1.3,  1.4,  1.5, /* Y */
  1.5,  1.4,  1.3,  1.2,  1.1,  1.0,  1.0,  1.0, /* G */
  1.0				/* C */
};

static const double standard_lum_adjustment[49] =
{
  0.50, 0.6,  0.7,  0.8,  0.9,  0.86, 0.82, 0.79, /* C */
  0.78, 0.8,  0.83, 0.87, 0.9,  0.95, 1.05, 1.15, /* B */
  1.3,  1.25, 1.2,  1.15, 1.12, 1.09, 1.06, 1.03, /* M */
  1.0,  1.0,  1.0,  1.0,  1.0,  1.0,  1.0,  1.0, /* R */
  1.0,  0.9,  0.8,  0.7,  0.65, 0.6,  0.55, 0.52, /* Y */
  0.48, 0.47, 0.47, 0.49, 0.49, 0.49, 0.52, 0.51, /* G */
  0.50				/* C */
};

static const double standard_hue_adjustment[49] =
{
  0.00, 0.05, 0.04, 0.01, -.03, -.10, -.18, -.26, /* C */
  -.35, -.43, -.40, -.32, -.25, -.18, -.10, -.07, /* B */
  0.00, -.04, -.09, -.13, -.18, -.23, -.27, -.31, /* M */
  -.35, -.38, -.30, -.23, -.15, -.08, 0.00, -.02, /* R */
  0.00, 0.08, 0.10, 0.08, 0.05, 0.03, -.03, -.12, /* Y */
  -.20, 0.17, -.20, -.17, -.15, -.12, -.10, -.08, /* G */
  0.00,				/* C */
};

static const double plain_paper_lum_adjustment[49] =
{
  1.2,  1.22, 1.28, 1.34, 1.39, 1.42, 1.45, 1.48, /* C */
  1.5,  1.4,  1.3,  1.25, 1.2,  1.1,  1.05, 1.05, /* B */
  1.05, 1.05, 1.05, 1.05, 1.05, 1.05, 1.05, 1.05, /* M */
  1.05, 1.05, 1.05, 1.1,  1.1,  1.1,  1.1,  1.1, /* R */
  1.1,  1.15, 1.3,  1.45, 1.6,  1.75, 1.9,  2.0, /* Y */
  2.1,  2.0,  1.8,  1.7,  1.6,  1.5,  1.4,  1.3, /* G */
  1.2				/* C */
};

static const double pgpp_sat_adjustment[49] =
{
  1.00, 1.00, 1.00, 1.00, 1.00, 1.00, 1.00, 1.00, /* C */
  1.00, 1.00, 1.00, 1.03, 1.05, 1.07, 1.09, 1.11, /* B */
  1.13, 1.13, 1.13, 1.13, 1.13, 1.13, 1.13, 1.13, /* M */
  1.13, 1.10, 1.05, 1.00, 1.00, 1.00, 1.00, 1.00, /* R */
  1.00, 1.00, 1.00, 1.00, 1.00, 1.00, 1.00, 1.00, /* Y */
  1.00, 1.00, 1.00, 1.00, 1.00, 1.00, 1.00, 1.00, /* G */
  1.00,				/* C */
};

static const double pgpp_lum_adjustment[49] =
{
  1.00, 1.00, 1.00, 1.00, 1.00, 1.00, 1.00, 1.00, /* C */
  1.00, 1.00, 1.00, 1.03, 1.05, 1.07, 1.09, 1.11, /* B */
  1.13, 1.13, 1.13, 1.13, 1.13, 1.13, 1.13, 1.13, /* M */
  1.13, 1.10, 1.05, 1.00, 1.00, 1.00, 1.00, 1.00, /* R */
  1.00, 1.00, 1.00, 1.00, 1.00, 1.00, 1.00, 1.00, /* Y */
  1.00, 1.00, 1.00, 1.00, 1.00, 1.00, 1.00, 1.00, /* G */
  1.00,				/* C */
};

static const double pgpp_hue_adjustment[49] =
{
  0.00, 0.00, 0.00, 0.00, 0.00, 0.01, 0.02, 0.03, /* C */
  0.05, 0.05, 0.05, 0.04, 0.04, 0.03, 0.02, 0.01, /* B */
  0.00, -.03, -.05, -.07, -.09, -.11, -.13, -.14, /* M */
  -.15, -.13, -.10, -.06, -.04, -.02, -.01, 0.00, /* R */
  0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 0.00, /* Y */
  0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 0.00, /* G */
  0.00,				/* C */
};

#define DECLARE_INK(name, density)					\
static const escp2_variable_ink_t name##_ink =				\
{									\
  name##_dither_ranges,							\
  sizeof(name##_dither_ranges) / sizeof(stp_simple_dither_range_t),	\
  density								\
}

#define PIEZO_0  .25
#define PIEZO_1  .5
#define PIEZO_2  .75
#define PIEZO_3 1.0

#define PIEZO_DENSITY 1.0

/***************************************************************\
*                                                               *
*                        SINGLE DOT SIZE                        *
*                                                               *
\***************************************************************/

static const stp_simple_dither_range_t photo_cyan_dither_ranges[] =
{
  { 0.27, 0x1, 1, 1 },
  { 1.0,  0x1, 0, 1 }
};

DECLARE_INK(photo_cyan, 1.0);

static const stp_simple_dither_range_t photo_magenta_dither_ranges[] =
{
  { 0.35, 0x1, 1, 1 },
  { 1.0,  0x1, 0, 1 }
};

DECLARE_INK(photo_magenta, 1.0);

static const stp_simple_dither_range_t photo2_yellow_dither_ranges[] =
{
  { 0.35, 0x1, 1, 1 },
  { 1.0,  0x1, 0, 1 }
};

DECLARE_INK(photo2_yellow, 1.0);

static const stp_simple_dither_range_t photo2_black_dither_ranges[] =
{
  { 0.27, 0x1, 1, 1 },
  { 1.0,  0x1, 0, 1 }
};

DECLARE_INK(photo2_black, 1.0);

static const stp_simple_dither_range_t piezo_quadtone_dither_ranges[] =
{
  { PIEZO_0, 0x1, 0, 1 },
  { PIEZO_1, 0x1, 1, 1 },
  { PIEZO_2, 0x1, 2, 1 },
  { PIEZO_3, 0x1, 3, 1 },
};

DECLARE_INK(piezo_quadtone, PIEZO_DENSITY);

/***************************************************************\
*                                                               *
*       LOW RESOLUTION, 4 AND 6 PICOLITRE PRINTERS              *
*                                                               *
\***************************************************************/

static const stp_simple_dither_range_t standard_multishot_dither_ranges[] =
{
  { 0.28,  0x1, 0, 2 },
  { 0.58,  0x2, 0, 4 },
  { 1.0,   0x3, 0, 7 }
};

DECLARE_INK(standard_multishot, 1.0);

static const stp_simple_dither_range_t photo_multishot_dither_ranges[] =
{
  { 0.0728, 0x1, 1, 1 },
  { 0.151,  0x2, 1, 2 },
  { 0.26,   0x3, 1, 3 },
  { 1.0,    0x3, 0, 3 }
};

DECLARE_INK(photo_multishot, 1.0);

static const stp_simple_dither_range_t photo_multishot_y_dither_ranges[] =
{
  { 0.140, 0x1, 0, 1 },
  { 0.290, 0x2, 0, 2 },
  { 0.5,   0x3, 0, 3 },
  { 1.0,   0x3, 1, 3 }
};

DECLARE_INK(photo_multishot_y, 1.0);

static const stp_simple_dither_range_t piezo_multishot_quadtone_dither_ranges[] =
{
  { PIEZO_0 * .28, 0x1, 0, 2 },
  { PIEZO_0 * .58, 0x2, 0, 4 },
  { PIEZO_1 * .58, 0x2, 1, 4 },
  { PIEZO_2 * .58, 0x2, 2, 4 },
  { PIEZO_2 * 1.0, 0x3, 2, 7 },
  { PIEZO_3 * 1.0, 0x3, 3, 7 },
};

DECLARE_INK(piezo_multishot_quadtone, PIEZO_DENSITY);

/***************************************************************\
*                                                               *
*       4 AND 6 PICOLITRE PRINTERS, 6 PICOLITRE DOTS            *
*                                                               *
\***************************************************************/

static const stp_simple_dither_range_t standard_6pl_dither_ranges[] =
{
  { 0.25,  0x1, 0, 1 },
  { 0.5,   0x2, 0, 2 },
  { 1.0,   0x3, 0, 4 }
};

DECLARE_INK(standard_6pl, 1.0);

static const stp_simple_dither_range_t standard_6pl_1440_dither_ranges[] =
{
  { 0.5,   0x1, 0, 1 },
  { 1.0,   0x2, 0, 2 },
};

DECLARE_INK(standard_6pl_1440, 1.0);

static const stp_simple_dither_range_t standard_6pl_2880_dither_ranges[] =
{
  { 1.0,   0x1, 0, 1 },
};

DECLARE_INK(standard_6pl_2880, 1.0);

static const stp_simple_dither_range_t photo_6pl_dither_ranges[] =
{
  { 0.065, 0x1, 1, 1 },
  { 0.13,  0x2, 1, 2 },
/* { 0.26, 0x3, 1, 4 }, */
  { 0.25,  0x1, 0, 1 },
  { 0.5,   0x2, 0, 2 },
  { 1.0,   0x3, 0, 4 }
};

DECLARE_INK(photo_6pl, 1.0);

static const stp_simple_dither_range_t photo_6pl_y_dither_ranges[] =
{
  { 0.125, 0x1, 0, 1 },
  { 0.25,  0x2, 0, 2 },
  { 0.5,   0x2, 1, 2 },
  { 1.0,   0x3, 1, 4 }
};

DECLARE_INK(photo_6pl_y, 1.0);

static const stp_simple_dither_range_t photo_6pl_1440_dither_ranges[] =
{
  { 0.13,  0x1, 1, 1 },
  { 0.26,  0x2, 1, 2 },
/* { 0.52, 0x3, 1, 4 }, */
  { 0.5,   0x1, 0, 1 },
  { 1.0,   0x2, 0, 2 },
};

DECLARE_INK(photo_6pl_1440, 1.0);

static const stp_simple_dither_range_t photo_6pl_2880_dither_ranges[] =
{
  { 0.26,  0x1, 1, 1 },
  { 1.0,   0x1, 0, 1 },
};

DECLARE_INK(photo_6pl_2880, 1.0);

static const stp_simple_dither_range_t piezo_6pl_quadtone_dither_ranges[] =
{
  { PIEZO_0 * .25, 0x1, 0, 1 },
  { PIEZO_0 * .50, 0x2, 0, 2 },
  { PIEZO_1 * .50, 0x2, 1, 2 },
  { PIEZO_2 * .50, 0x2, 2, 2 },
  { PIEZO_2 * 1.0, 0x3, 2, 4 },
  { PIEZO_3 * 1.0, 0x3, 3, 4 },
};

DECLARE_INK(piezo_6pl_quadtone, PIEZO_DENSITY);

static const stp_simple_dither_range_t piezo_6pl_1440_quadtone_dither_ranges[] =
{
  { PIEZO_0 * .50, 0x1, 0, 1 },
  { PIEZO_1 * .50, 0x1, 1, 1 },
  { PIEZO_2 * .50, 0x1, 2, 1 },
  { PIEZO_2 * 1.0, 0x2, 2, 2 },
  { PIEZO_3 * 1.0, 0x2, 3, 2 },
};

DECLARE_INK(piezo_6pl_1440_quadtone, PIEZO_DENSITY);

static const stp_simple_dither_range_t piezo_6pl_2880_quadtone_dither_ranges[] =
{
  { PIEZO_0 * 1.0, 0x1, 0, 1 },
  { PIEZO_1 * 1.0, 0x1, 1, 1 },
  { PIEZO_2 * 1.0, 0x1, 2, 1 },
  { PIEZO_3 * 1.0, 0x1, 3, 1 },
};

DECLARE_INK(piezo_6pl_2880_quadtone, PIEZO_DENSITY);


/***************************************************************\
*                                                               *
*                      STYLUS COLOR 480/580                     *
*                                                               *
\***************************************************************/

static const stp_simple_dither_range_t standard_x80_multishot_dither_ranges[] =
{
  { 0.163, 0x1, 0, 1 },
  { 0.5,   0x2, 0, 3 },
  { 1.0,   0x3, 0, 6 }
};

DECLARE_INK(standard_x80_multishot, 1.0);

static const stp_simple_dither_range_t standard_x80_6pl_dither_ranges[] =
{
  { 0.325, 0x1, 0, 2 },
  { 0.5,   0x2, 0, 3 },
  { 1.0,   0x3, 0, 6 }
};

DECLARE_INK(standard_x80_6pl, 1.0);

static const stp_simple_dither_range_t standard_x80_1440_6pl_dither_ranges[] =
{
  { 0.65,  0x1, 0, 2 },
  { 1.0,   0x2, 0, 3 },
};

DECLARE_INK(standard_x80_1440_6pl, 1.0);

static const stp_simple_dither_range_t standard_x80_2880_6pl_dither_ranges[] =
{
  { 1.00,  0x1, 0, 1 },
};

DECLARE_INK(standard_x80_2880_6pl, 1.0);

static const stp_simple_dither_range_t piezo_x80_multishot_quadtone_dither_ranges[] =
{
  { PIEZO_0 * .163, 0x1, 0, 1 },
  { PIEZO_0 * .500, 0x2, 0, 3 },
  { PIEZO_1 * .500, 0x2, 1, 3 },
  { PIEZO_2 * .500, 0x2, 2, 3 },
  { PIEZO_2 * 1.00, 0x3, 2, 6 },
  { PIEZO_3 * 1.00, 0x3, 3, 6 },
};

DECLARE_INK(piezo_x80_multishot_quadtone, PIEZO_DENSITY);

static const stp_simple_dither_range_t piezo_x80_6pl_quadtone_dither_ranges[] =
{
  { PIEZO_0 * .325, 0x1, 0, 2 },
  { PIEZO_0 * .500, 0x2, 0, 3 },
  { PIEZO_1 * .500, 0x2, 1, 3 },
  { PIEZO_2 * .500, 0x2, 2, 3 },
  { PIEZO_2 * 1.00, 0x3, 2, 6 },
  { PIEZO_3 * 1.00, 0x3, 3, 6 },
};

DECLARE_INK(piezo_x80_6pl_quadtone, PIEZO_DENSITY);

static const stp_simple_dither_range_t piezo_x80_1440_6pl_quadtone_dither_ranges[] =
{
  { PIEZO_0 * .650, 0x1, 0, 2 },
  { PIEZO_1 * .650, 0x1, 1, 2 },
  { PIEZO_2 * .650, 0x1, 2, 2 },
  { PIEZO_2 * 1.00, 0x2, 2, 3 },
  { PIEZO_3 * 1.00, 0x2, 3, 3 },
};

DECLARE_INK(piezo_x80_1440_6pl_quadtone, PIEZO_DENSITY);

static const stp_simple_dither_range_t piezo_x80_2880_6pl_quadtone_dither_ranges[] =
{
  { PIEZO_0 * 1.00, 0x1, 0, 1 },
  { PIEZO_1 * 1.00, 0x1, 1, 1 },
  { PIEZO_2 * 1.00, 0x1, 2, 1 },
  { PIEZO_3 * 1.00, 0x1, 3, 1 },
};

DECLARE_INK(piezo_x80_2880_6pl_quadtone, PIEZO_DENSITY);


/***************************************************************\
*                                                               *
*                      STYLUS COLOR 680/777
*                                                               *
\***************************************************************/

static const stp_simple_dither_range_t standard_680_multishot_dither_ranges[] =
{
  { 0.375, 0x1, 0, 3 },
  { 0.75,  0x2, 0, 6 },
  { 1.0,   0x3, 0, 8 }
};

DECLARE_INK(standard_680_multishot, 1.0);

static const stp_simple_dither_range_t standard_680_6pl_dither_ranges[] =
{
  { 0.50,  0x1, 0, 3 },
  { 0.66,  0x2, 0, 4 },
  { 1.0,   0x3, 0, 6 }
};

DECLARE_INK(standard_680_6pl, 1.0);

static const stp_simple_dither_range_t piezo_680_multishot_quadtone_dither_ranges[] =
{
  { PIEZO_0 * .375, 0x1, 0, 3 },
  { PIEZO_0 * .750, 0x2, 0, 6 },
  { PIEZO_1 * .750, 0x2, 1, 6 },
  { PIEZO_2 * .750, 0x2, 2, 6 },
  { PIEZO_2 * 1.00, 0x3, 2, 8 },
  { PIEZO_3 * 1.00, 0x3, 3, 8 },
};

DECLARE_INK(piezo_680_multishot_quadtone, PIEZO_DENSITY);

static const stp_simple_dither_range_t piezo_680_6pl_quadtone_dither_ranges[] =
{
  { PIEZO_0 * .500, 0x1, 0, 3 },
  { PIEZO_1 * .660, 0x2, 1, 4 },
  { PIEZO_2 * .660, 0x2, 2, 4 },
  { PIEZO_2 * 1.00, 0x2, 3, 6 },
  { PIEZO_3 * 1.00, 0x3, 3, 6 },
};

DECLARE_INK(piezo_680_6pl_quadtone, PIEZO_DENSITY);


/***************************************************************\
*                                                               *
*                   4 PICOLITRE DOTS                            *
*                                                               *
\***************************************************************/

static const stp_simple_dither_range_t standard_4pl_dither_ranges[] =
{
  { 0.661, 0x1, 0, 2 },
  { 1.00,  0x2, 0, 3 }
};

DECLARE_INK(standard_4pl, 1.0);

static const stp_simple_dither_range_t standard_4pl_2880_dither_ranges[] =
{
  { 1.00,  0x1, 0, 1 },
};

DECLARE_INK(standard_4pl_2880, 1.0);

static const stp_simple_dither_range_t photo_4pl_dither_ranges[] =
{
  { 0.17,  0x1, 1, 2 },
  { 0.26,  0x2, 1, 3 },
  { 0.661, 0x1, 0, 2 },
  { 1.00,  0x2, 0, 3 }
};

DECLARE_INK(photo_4pl, 1.0);

static const stp_simple_dither_range_t photo_4pl_y_dither_ranges[] =
{
  { 0.330, 0x1, 0, 2 },
  { 0.50,  0x2, 0, 3 },
  { 0.661, 0x1, 1, 2 },
  { 1.00,  0x2, 1, 3 }
};

DECLARE_INK(photo_4pl_y, 1.0);

static const stp_simple_dither_range_t photo_4pl_2880_dither_ranges[] =
{
  { 0.26,  0x1, 1, 1 },
  { 1.00,  0x1, 0, 1 },
};

DECLARE_INK(photo_4pl_2880, 1.0);

static const stp_simple_dither_range_t piezo_4pl_quadtone_dither_ranges[] =
{
  { PIEZO_0 * .661, 0x1, 0, 2 },
  { PIEZO_1 * .661, 0x1, 1, 2 },
  { PIEZO_2 * .661, 0x1, 2, 2 },
  { PIEZO_2 * 1.00, 0x2, 2, 3 },
  { PIEZO_3 * 1.00, 0x2, 3, 3 },
};

DECLARE_INK(piezo_4pl_quadtone, PIEZO_DENSITY);

static const stp_simple_dither_range_t piezo_4pl_2880_quadtone_dither_ranges[] =
{
  { PIEZO_0, 0x1, 0, 1 },
  { PIEZO_1, 0x1, 1, 1 },
  { PIEZO_2, 0x1, 2, 1 },
  { PIEZO_3, 0x1, 3, 1 },
};

DECLARE_INK(piezo_4pl_2880_quadtone, PIEZO_DENSITY);


/***************************************************************\
*                                                               *
*                 3 PICOLITRE DOTS (900 AND 980)                *
*                                                               *
\***************************************************************/

static const stp_simple_dither_range_t standard_3pl_dither_ranges[] =
{
  { 0.25,  0x1, 0, 2 },
  { 0.61,  0x2, 0, 5 },
  { 1.0,   0x3, 0, 8 }
};

DECLARE_INK(standard_3pl, 1.0);


static const stp_simple_dither_range_t standard_3pl_1440_dither_ranges[] =
{
  { 0.39, 0x1, 0, 2 },
  { 1.0,  0x2, 0, 5 }
};

DECLARE_INK(standard_3pl_1440, 1.0);


static const stp_simple_dither_range_t standard_3pl_2880_dither_ranges[] =
{
  { 1.0,   0x1, 0, 1 }
};

DECLARE_INK(standard_3pl_2880, 1.0);

static const stp_simple_dither_range_t standard_980_6pl_dither_ranges[] =
{
  { 0.40,  0x1, 0, 4 },
  { 0.675, 0x2, 0, 7 },
  { 1.0,   0x3, 0, 10 }
};

DECLARE_INK(standard_980_6pl, 1.0);

static const stp_simple_dither_range_t piezo_3pl_quadtone_dither_ranges[] =
{
  { PIEZO_0 * .25, 0x1, 0, 2 },
  { PIEZO_0 * .61, 0x2, 0, 5 },
  { PIEZO_1 * .61, 0x2, 1, 5 },
  { PIEZO_2 * .61, 0x2, 2, 5 },
  { PIEZO_2 * 1.0, 0x3, 2, 8 },
  { PIEZO_3 * 1.0, 0x3, 3, 8 },
};

DECLARE_INK(piezo_3pl_quadtone, PIEZO_DENSITY);

static const stp_simple_dither_range_t piezo_3pl_1440_quadtone_dither_ranges[]=
{
  { PIEZO_0 * .390, 0x1, 0, 2 },
  { PIEZO_1 * .390, 0x1, 1, 2 },
  { PIEZO_2 * .390, 0x1, 2, 2 },
  { PIEZO_2 * 1.00, 0x2, 2, 5 },
  { PIEZO_3 * 1.00, 0x2, 3, 5 },
};

DECLARE_INK(piezo_3pl_1440_quadtone, PIEZO_DENSITY);

static const stp_simple_dither_range_t piezo_3pl_2880_quadtone_dither_ranges[]=
{
  { PIEZO_0, 0x1, 0, 1 },
  { PIEZO_1, 0x1, 1, 1 },
  { PIEZO_2, 0x1, 2, 1 },
  { PIEZO_3, 0x1, 3, 1 },
};

DECLARE_INK(piezo_3pl_2880_quadtone, PIEZO_DENSITY);

static const stp_simple_dither_range_t piezo_980_6pl_quadtone_dither_ranges[] =
{
  { PIEZO_0 * .400, 0x1, 0, 4 },
  { PIEZO_0 * .675, 0x2, 0, 7 },
  { PIEZO_1 * .675, 0x2, 1, 7 },
  { PIEZO_2 * .675, 0x2, 2, 7 },
  { PIEZO_3 * 1.00, 0x3, 3, 10 },
};

DECLARE_INK(piezo_980_6pl_quadtone, PIEZO_DENSITY);


/***************************************************************\
*                                                               *
*                  2 PICOLITRE DOTS (950)                       *
*                                                               *
\***************************************************************/

static const stp_simple_dither_range_t standard_2pl_2880_dither_ranges[] =
{
  { 1.00, 0x1, 0, 1 },
};

DECLARE_INK(standard_2pl_2880, 1.0);

static const stp_simple_dither_range_t photo_2pl_2880_dither_ranges[] =
{
  { 0.26, 0x1, 1, 1 },
  { 1.00, 0x1, 0, 1 },
};

DECLARE_INK(photo_2pl_2880, 0.5);

static const stp_simple_dither_range_t photo_2pl_2880_c_dither_ranges[] =
{
  { 0.26, 0x1, 1, 1 },
  { 1.00, 0x1, 0, 1 },
};

DECLARE_INK(photo_2pl_2880_c, .5);

static const stp_simple_dither_range_t photo_2pl_2880_m_dither_ranges[] =
{
  { 0.31, 0x1, 1, 1 },
  { 1.00, 0x1, 0, 1 },
};

DECLARE_INK(photo_2pl_2880_m, .5);

static const stp_simple_dither_range_t photo_2pl_2880_y_dither_ranges[] =
{
  { 0.5,  0x1, 0, 1 },
  { 1.00, 0x1, 1, 1 },
};

DECLARE_INK(photo_2pl_2880_y, 1.00);

static const stp_simple_dither_range_t piezo_2pl_2880_quadtone_dither_ranges[]=
{
  { PIEZO_0, 0x1, 0, 1 },
  { PIEZO_1, 0x1, 1, 1 },
  { PIEZO_2, 0x1, 2, 1 },
  { PIEZO_3, 0x1, 3, 1 },
};

DECLARE_INK(piezo_2pl_2880_quadtone, PIEZO_DENSITY);

static const stp_simple_dither_range_t standard_2pl_1440_dither_ranges[] =
{
  { 0.5,   0x1, 0, 1 },
  { 1.00,  0x2, 0, 2 }
};

DECLARE_INK(standard_2pl_1440, 1.0);

static const stp_simple_dither_range_t piezo_2pl_1440_quadtone_dither_ranges[] =
{
  { PIEZO_0 * .5,   0x1, 0, 1 },
  { PIEZO_1 * .5,   0x1, 1, 1 },
  { PIEZO_2 * .5,   0x1, 2, 1 },
  { PIEZO_2 * 1.00, 0x2, 2, 2 },
  { PIEZO_3 * 1.00, 0x2, 3, 2 },
};

DECLARE_INK(piezo_2pl_1440_quadtone, PIEZO_DENSITY);

static const stp_simple_dither_range_t photo_2pl_1440_dither_ranges[] =
{
  { 0.13,  0x1, 1, 1 },
  { 0.26,  0x2, 1, 2 },
  { 0.5,   0x1, 0, 1 },
  { 1.00,  0x2, 0, 2 }
};

DECLARE_INK(photo_2pl_1440, 1.0);

static const stp_simple_dither_range_t photo_2pl_1440_y_dither_ranges[] =
{
  { 0.25,  0x1, 0, 1 },
  { 0.50,  0x2, 0, 2 },
  { 1.00,  0x2, 1, 2 }
};

DECLARE_INK(photo_2pl_1440_y, 1.0);

static const stp_simple_dither_range_t standard_2pl_720_dither_ranges[] =
{
  { 0.25,  0x1, 0, 1 },
  { 0.5,   0x2, 0, 2 },
  { 1.0,   0x3, 0, 4 }
};

DECLARE_INK(standard_2pl_720, 1.0);

static const stp_simple_dither_range_t photo_2pl_720_dither_ranges[] =
{
  { 0.065, 0x1, 1, 1 },
  { 0.13,  0x2, 1, 2 },
/* { 0.26, 0x3, 1, 4 }, */
  { 0.25,  0x1, 0, 1 },
  { 0.5,   0x2, 0, 2 },
  { 1.0,   0x3, 0, 4 }
};

DECLARE_INK(photo_2pl_720, 1.0);

static const stp_simple_dither_range_t photo_2pl_720_y_dither_ranges[] =
{
  { 0.125, 0x1, 0, 1 },
  { 0.25,  0x2, 0, 2 },
  { 0.5,   0x2, 1, 2 },
  { 1.0,   0x3, 1, 4 }
};

DECLARE_INK(photo_2pl_720_y, 1.0);

static const stp_simple_dither_range_t piezo_2pl_720_quadtone_dither_ranges[] =
{
  { PIEZO_0 * .25, 0x1, 0, 1 },
  { PIEZO_0 * .50, 0x2, 0, 2 },
  { PIEZO_1 * .50, 0x2, 1, 2 },
  { PIEZO_2 * .50, 0x2, 2, 2 },
  { PIEZO_2 * 1.0, 0x3, 2, 4 },
  { PIEZO_3 * 1.0, 0x3, 3, 4 },
};

DECLARE_INK(piezo_2pl_720_quadtone, PIEZO_DENSITY);

static const stp_simple_dither_range_t standard_2pl_360_dither_ranges[] =
{
  { 0.25,  0x1, 0, 2 },
  { 0.5,   0x2, 0, 4 },
  { 1.0,   0x3, 0, 7 }
};

DECLARE_INK(standard_2pl_360, 1.0);

static const stp_simple_dither_range_t photo_2pl_360_dither_ranges[] =
{
  { 0.065,  0x1, 1, 1 },
  { 0.13,   0x2, 1, 2 },
  { 0.26,   0x3, 1, 3 },
  { 1.0,    0x3, 0, 3 }
};

DECLARE_INK(photo_2pl_360, 1.0);

static const stp_simple_dither_range_t photo_2pl_360_y_dither_ranges[] =
{
  { 0.145, 0x1, 0, 1 },
  { 0.290, 0x2, 0, 2 },
  { 0.5,   0x3, 0, 3 },
  { 1.0,   0x3, 1, 3 }
};

DECLARE_INK(photo_2pl_360_y, 1.0);

static const stp_simple_dither_range_t piezo_2pl_360_quadtone_dither_ranges[] =
{
  { PIEZO_0 * .25, 0x1, 0, 2 },
  { PIEZO_0 * .50, 0x2, 0, 4 },
  { PIEZO_1 * .50, 0x2, 1, 4 },
  { PIEZO_2 * .50, 0x2, 2, 4 },
  { PIEZO_2 * 1.0, 0x3, 2, 7 },
  { PIEZO_3 * 1.0, 0x3, 3, 7 },
};

DECLARE_INK(piezo_2pl_360_quadtone, PIEZO_DENSITY);


/***************************************************************\
*                                                               *
*                     STYLUS C70/C80 (PIGMENT)                  *
*                                                               *
\***************************************************************/

static const stp_simple_dither_range_t standard_economy_pigment_dither_ranges[] =
{
  { 1.0,   0x3, 0, 3 }
};

DECLARE_INK(standard_economy_pigment, 1.0);

static const stp_simple_dither_range_t standard_multishot_pigment_dither_ranges[] =
{
  { 0.410, 0x1, 0, 2 },
  { 1.0,   0x3, 0, 5 }
};

DECLARE_INK(standard_multishot_pigment, 1.0);

static const stp_simple_dither_range_t standard_6pl_pigment_dither_ranges[] =
{
  { 0.300, 0x1, 0, 3 },
  { 1.0,   0x3, 0, 10 }
};

DECLARE_INK(standard_6pl_pigment, 1.0);

static const stp_simple_dither_range_t standard_3pl_pigment_dither_ranges[] =
{
  { 0.650, 0x1, 0, 2 },
  { 1.000, 0x2, 0, 3 },
};

DECLARE_INK(standard_3pl_pigment, 1.0);

static const stp_simple_dither_range_t standard_3pl_pigment_2880_dither_ranges[] =
{
  { 1.0,   0x1, 0, 1 }
};

DECLARE_INK(standard_3pl_pigment_2880, 1.0);

static const stp_simple_dither_range_t piezo_economy_pigment_quadtone_dither_ranges[]=
{
  { PIEZO_0, 0x3, 0, 1 },
  { PIEZO_1, 0x3, 1, 1 },
  { PIEZO_2, 0x3, 2, 1 },
  { PIEZO_3, 0x3, 3, 1 },
};

DECLARE_INK(piezo_economy_pigment_quadtone, PIEZO_DENSITY);

static const stp_simple_dither_range_t piezo_multishot_pigment_quadtone_dither_ranges[]=
{
  { PIEZO_0 * .410, 0x1, 0, 2 },
  { PIEZO_1 * .410, 0x1, 1, 2 },
  { PIEZO_2 * .410, 0x1, 2, 2 },
  { PIEZO_3 * 1.00, 0x3, 3, 5 },
};

DECLARE_INK(piezo_multishot_pigment_quadtone, PIEZO_DENSITY);

static const stp_simple_dither_range_t piezo_6pl_pigment_quadtone_dither_ranges[]=
{
  { PIEZO_0 * .300, 0x1, 0, 3 },
  { PIEZO_0 * .600, 0x2, 0, 6 },
  { PIEZO_1 * .600, 0x2, 1, 6 },
  { PIEZO_2 * .600, 0x2, 2, 6 },
  { PIEZO_3 * 1.00, 0x3, 3, 10 },
};

DECLARE_INK(piezo_6pl_pigment_quadtone, PIEZO_DENSITY);

static const stp_simple_dither_range_t piezo_3pl_pigment_quadtone_dither_ranges[]=
{
  { PIEZO_0 * .650, 0x1, 0, 2 },
  { PIEZO_1 * .650, 0x1, 1, 2 },
  { PIEZO_2 * .650, 0x1, 2, 2 },
  { PIEZO_3 * 1.00, 0x2, 3, 3 },
};

DECLARE_INK(piezo_3pl_pigment_quadtone, PIEZO_DENSITY);

static const stp_simple_dither_range_t piezo_3pl_pigment_2880_quadtone_dither_ranges[]=
{
  { PIEZO_0, 0x1, 0, 1 },
  { PIEZO_1, 0x1, 1, 1 },
  { PIEZO_2, 0x1, 2, 1 },
  { PIEZO_3, 0x1, 3, 1 },
};

DECLARE_INK(piezo_3pl_pigment_2880_quadtone, PIEZO_DENSITY);


/***************************************************************\
*                                                               *
*                   STYLUS PHOTO 2000P                          *
*                                                               *
\***************************************************************/

static const stp_simple_dither_range_t standard_pigment_dither_ranges[] =
{ /* MRS: Not calibrated! */
  { 0.55,  0x1, 0, 1 },
  { 1.0,   0x2, 0, 2 }
};

DECLARE_INK(standard_pigment, 1.0);

static const stp_simple_dither_range_t photo_pigment_dither_ranges[] =
{ /* MRS: Not calibrated! */
  { 0.15,  0x1, 1, 1 },
  { 0.227, 0x2, 1, 2 },
  { 0.5,   0x1, 0, 1 },
  { 1.0,   0x2, 0, 2 }
};

DECLARE_INK(photo_pigment, 1.0);

static const stp_simple_dither_range_t piezo_pigment_quadtone_dither_ranges[]=
{
  { PIEZO_0 * .550, 0x1, 0, 1 },
  { PIEZO_1 * .550, 0x1, 1, 1 },
  { PIEZO_2 * .550, 0x1, 2, 1 },
  { PIEZO_3 * .550, 0x1, 3, 1 },
  { PIEZO_3 * 1.00, 0x2, 3, 2 },
};

DECLARE_INK(piezo_pigment_quadtone, PIEZO_DENSITY);


/***************************************************************\
*                                                               *
*            ULTRACHROME (2100/2200, 7600, 9600)                *
*                                                               *
\***************************************************************/

static const stp_simple_dither_range_t standard_4pl_pigment_low_dither_ranges[] =
{
  { 0.40,  0x1, 0, 40 },
  { 0.70,  0x2, 0, 70 },
  { 1.00,  0x3, 0, 100 }
};

DECLARE_INK(standard_4pl_pigment_low, 0.5);

static const stp_simple_dither_range_t photo_4pl_pigment_low_m_dither_ranges[] =
{
  { 0.104,  0x1, 1, 40 },
  { 0.182,  0x2, 1, 70 },
  { 0.26,   0x3, 1, 100 },
  { 0.70,   0x2, 0, 70 },
  { 1.00,   0x3, 0, 100 }
};

DECLARE_INK(photo_4pl_pigment_low_m, 0.5);

static const stp_simple_dither_range_t photo_4pl_pigment_low_c_dither_ranges[] =
{
  { 0.16,   0x1, 1, 40 },
  { 0.28,   0x2, 1, 70 },
  { 0.40,   0x3, 1, 100 },
  { 0.70,   0x2, 0, 70 },
  { 1.00,   0x3, 0, 100 }
};

DECLARE_INK(photo_4pl_pigment_low_c, 0.5);

static const stp_simple_dither_range_t photo_4pl_pigment_low_y_dither_ranges[] =
{
  { 0.20,   0x1, 1, 40 },
  { 0.35,   0x2, 1, 70 },
  { 0.50,   0x3, 1, 100 },
  { 1.00,   0x3, 0, 100 }
};

DECLARE_INK(photo_4pl_pigment_low_y, 1.5);

static const stp_simple_dither_range_t photo_4pl_pigment_low_k_dither_ranges[] =
{
  { 0.196,  0x1, 1, 40 },
  { 0.40,   0x1, 0, 40 },
  { 0.70,   0x2, 0, 70 },
  { 1.00,   0x3, 0, 100 }
};

DECLARE_INK(photo_4pl_pigment_low_k, 0.5);

static const stp_simple_dither_range_t standard_4pl_pigment_dither_ranges[] =
{
  { 0.28,  0x1, 0, 28 },
  { 0.50,  0x2, 0, 50 },
  { 1.00,  0x3, 0, 100 }
};

DECLARE_INK(standard_4pl_pigment, 1.0);

static const stp_simple_dither_range_t photo_4pl_pigment_m_dither_ranges[] =
{
  { 0.0728, 0x1, 1, 28 },
  { 0.13,   0x2, 1, 50 },
  { 0.26,   0x3, 1, 100 },
  { 0.50,   0x2, 0, 50 },
  { 1.00,   0x3, 0, 100 }
};

DECLARE_INK(photo_4pl_pigment_m, 1.0);

static const stp_simple_dither_range_t photo_4pl_pigment_c_dither_ranges[] =
{
  { 0.112,  0x1, 1, 28 },
  { 0.20,  0x2, 1, 50 },
  { 0.40,   0x3, 1, 100 },
  { 0.50,   0x2, 0, 50 },
  { 1.00,   0x3, 0, 100 }
};

DECLARE_INK(photo_4pl_pigment_c, 1.0);

static const stp_simple_dither_range_t photo_4pl_pigment_y_dither_ranges[] =
{
  { 0.14,   0x1, 1, 28 },
  { 0.25,   0x2, 1, 50 },
  { 0.50,   0x3, 1, 100 },
  { 1.00,   0x3, 0, 100 }
};

DECLARE_INK(photo_4pl_pigment_y, 1.5);

static const stp_simple_dither_range_t photo_4pl_pigment_k_dither_ranges[] =
{
  { 0.1344, 0x1, 1, 28 },
  { 0.24,   0x2, 1, 50 },
  { 0.50,   0x2, 0, 50 },
  { 1.00,   0x3, 0, 100 }
};

DECLARE_INK(photo_4pl_pigment_k, 0.75);

static const stp_simple_dither_range_t standard_4pl_pigment_1440_dither_ranges[] =
{
  { 0.56,  0x1, 0, 56 },
  { 1.00,  0x2, 0, 100 },
};

DECLARE_INK(standard_4pl_pigment_1440, 1.0);

static const stp_simple_dither_range_t photo_4pl_pigment_1440_m_dither_ranges[] =
{
  { 0.1456, 0x1, 1, 56 },
  { 0.26,   0x2, 1, 100 },
  { 0.56,   0x1, 0, 56 },
  { 1.00,   0x2, 0, 100 }
};

DECLARE_INK(photo_4pl_pigment_1440_m, 1.0);

static const stp_simple_dither_range_t photo_4pl_pigment_1440_c_dither_ranges[] =
{
  { 0.224,  0x1, 1, 56 },
  { 0.40,   0x2, 1, 100 },
  { 0.56,   0x1, 0, 56 },
  { 1.00,   0x2, 0, 100 }
};

DECLARE_INK(photo_4pl_pigment_1440_c, 1.0);

static const stp_simple_dither_range_t photo_4pl_pigment_1440_y_dither_ranges[] =
{
  { 0.28,   0x1, 1, 56 },
  { 0.50,   0x2, 1, 100 },
  { 1.00,   0x2, 0, 100 }
};

DECLARE_INK(photo_4pl_pigment_1440_y, 1.5);

static const stp_simple_dither_range_t photo_4pl_pigment_1440_k_dither_ranges[] =
{
  { 0.2688, 0x1, 1, 56 },
  { 0.56,   0x1, 0, 56 },
  { 1.00,   0x2, 0, 100 }
};

DECLARE_INK(photo_4pl_pigment_1440_k, 0.75);

static const stp_simple_dither_range_t standard_4pl_pigment_2880_dither_ranges[] =
{
  { 1.00,  0x1, 0, 1 },
};

DECLARE_INK(standard_4pl_pigment_2880, 1.0);

static const stp_simple_dither_range_t photo_4pl_pigment_2880_m_dither_ranges[] =
{
  { 0.26,  0x1, 1, 1 },
  { 1.00,  0x1, 0, 1 },
};

DECLARE_INK(photo_4pl_pigment_2880_m, 0.75);

static const stp_simple_dither_range_t photo_4pl_pigment_2880_c_dither_ranges[] =
{
  { 0.40,  0x1, 1, 1 },
  { 1.00,  0x1, 0, 1 },
};

DECLARE_INK(photo_4pl_pigment_2880_c, 0.75);

static const stp_simple_dither_range_t photo_4pl_pigment_2880_y_dither_ranges[] =
{
  { 0.50,  0x1, 1, 1 },
  { 1.00,  0x1, 0, 1 },
};

DECLARE_INK(photo_4pl_pigment_2880_y, 1.5);

static const stp_simple_dither_range_t photo_4pl_pigment_2880_k_dither_ranges[] =
{
  { 0.48,  0x1, 1, 1 },
  { 1.00,  0x1, 0, 1 },
};

DECLARE_INK(photo_4pl_pigment_2880_k, 0.75);


/***************************************************************\
*                                                               *
*                      STYLUS PRO 10000                         *
*                                                               *
\***************************************************************/

static const stp_simple_dither_range_t spro10000_standard_dither_ranges[] =
{
  { 0.661, 0x1, 0, 2 },
  { 1.00,  0x2, 0, 3 }
};

DECLARE_INK(spro10000_standard, 1.0);

static const stp_simple_dither_range_t spro10000_photo_dither_ranges[] =
{
  { 0.17,  0x1, 1, 2 },
  { 0.26,  0x2, 1, 3 },
  { 0.661, 0x1, 0, 2 },
  { 1.00,  0x2, 0, 3 }
};

DECLARE_INK(spro10000_photo, 1.0);


static const escp2_variable_inkset_t standard_inks =
{
  NULL,
  NULL,
  NULL,
  NULL
};

static const escp2_variable_inkset_t photo_inks =
{
  NULL,
  &photo_cyan_ink,
  &photo_magenta_ink,
  NULL
};

static const escp2_variable_inkset_t piezo_quadtone_inks =
{
  &piezo_quadtone_ink,
  NULL,
  NULL,
  NULL
};

static const escp2_variable_inkset_t escp2_multishot_standard_inks =
{
  &standard_multishot_ink,
  &standard_multishot_ink,
  &standard_multishot_ink,
  &standard_multishot_ink
};

static const escp2_variable_inkset_t escp2_multishot_photo_inks =
{
  &standard_multishot_ink,
  &photo_multishot_ink,
  &photo_multishot_ink,
  &standard_multishot_ink
};

static const escp2_variable_inkset_t escp2_multishot_photo2_inks =
{
  &photo_multishot_ink,
  &photo_multishot_ink,
  &photo_multishot_ink,
  &standard_multishot_ink
};

static const escp2_variable_inkset_t escp2_multishot_photoj_inks =
{
  &standard_multishot_ink,
  &photo_multishot_ink,
  &photo_multishot_ink,
  &photo_multishot_y_ink
};

static const escp2_variable_inkset_t piezo_multishot_quadtone_inks =
{
  &piezo_multishot_quadtone_ink,
  NULL,
  NULL,
  NULL
};


static const escp2_variable_inkset_t escp2_6pl_standard_inks =
{
  &standard_6pl_ink,
  &standard_6pl_ink,
  &standard_6pl_ink,
  &standard_6pl_ink
};

static const escp2_variable_inkset_t escp2_6pl_photo_inks =
{
  &standard_6pl_ink,
  &photo_6pl_ink,
  &photo_6pl_ink,
  &standard_6pl_ink
};

static const escp2_variable_inkset_t escp2_6pl_photo2_inks =
{
  &photo_6pl_ink,
  &photo_6pl_ink,
  &photo_6pl_ink,
  &standard_6pl_ink
};

static const escp2_variable_inkset_t escp2_6pl_photoj_inks =
{
  &standard_6pl_ink,
  &photo_6pl_ink,
  &photo_6pl_ink,
  &photo_6pl_y_ink
};

static const escp2_variable_inkset_t piezo_6pl_quadtone_inks =
{
  &piezo_6pl_quadtone_ink,
  NULL,
  NULL,
  NULL
};


static const escp2_variable_inkset_t escp2_6pl_1440_standard_inks =
{
  &standard_6pl_1440_ink,
  &standard_6pl_1440_ink,
  &standard_6pl_1440_ink,
  &standard_6pl_1440_ink
};

static const escp2_variable_inkset_t escp2_6pl_1440_photo_inks =
{
  &standard_6pl_1440_ink,
  &photo_6pl_1440_ink,
  &photo_6pl_1440_ink,
  &standard_6pl_1440_ink
};

static const escp2_variable_inkset_t piezo_6pl_1440_quadtone_inks =
{
  &piezo_6pl_1440_quadtone_ink,
  NULL,
  NULL,
  NULL
};


static const escp2_variable_inkset_t escp2_6pl_2880_standard_inks =
{
  &standard_6pl_2880_ink,
  &standard_6pl_2880_ink,
  &standard_6pl_2880_ink,
  &standard_6pl_2880_ink
};

static const escp2_variable_inkset_t escp2_6pl_2880_photo_inks =
{
  &standard_6pl_2880_ink,
  &photo_6pl_2880_ink,
  &photo_6pl_2880_ink,
  &standard_6pl_2880_ink
};

static const escp2_variable_inkset_t piezo_6pl_2880_quadtone_inks =
{
  &piezo_6pl_2880_quadtone_ink,
  NULL,
  NULL,
  NULL
};


static const escp2_variable_inkset_t escp2_680_multishot_standard_inks =
{
  &standard_680_multishot_ink,
  &standard_680_multishot_ink,
  &standard_680_multishot_ink,
  &standard_680_multishot_ink
};

static const escp2_variable_inkset_t escp2_680_6pl_standard_inks =
{
  &standard_680_6pl_ink,
  &standard_680_6pl_ink,
  &standard_680_6pl_ink,
  &standard_680_6pl_ink
};

static const escp2_variable_inkset_t piezo_680_multishot_quadtone_inks =
{
  &piezo_680_multishot_quadtone_ink,
  NULL,
  NULL,
  NULL
};

static const escp2_variable_inkset_t piezo_680_6pl_quadtone_inks =
{
  &piezo_680_6pl_quadtone_ink,
  NULL,
  NULL,
  NULL
};


static const escp2_variable_inkset_t escp2_4pl_standard_inks =
{
  &standard_4pl_ink,
  &standard_4pl_ink,
  &standard_4pl_ink,
  &standard_4pl_ink
};

static const escp2_variable_inkset_t escp2_4pl_photo_inks =
{
  &standard_4pl_ink,
  &photo_4pl_ink,
  &photo_4pl_ink,
  &standard_4pl_ink
};

static const escp2_variable_inkset_t escp2_4pl_photoj_inks =
{
  &standard_4pl_ink,
  &photo_4pl_ink,
  &photo_4pl_ink,
  &photo_4pl_y_ink,
};

static const escp2_variable_inkset_t piezo_4pl_quadtone_inks =
{
  &piezo_4pl_quadtone_ink,
  NULL,
  NULL,
  NULL
};

static const escp2_variable_inkset_t escp2_4pl_2880_standard_inks =
{
  &standard_4pl_2880_ink,
  &standard_4pl_2880_ink,
  &standard_4pl_2880_ink,
  &standard_4pl_2880_ink
};

static const escp2_variable_inkset_t escp2_4pl_2880_photo_inks =
{
  &standard_4pl_2880_ink,
  &photo_4pl_2880_ink,
  &photo_4pl_2880_ink,
  &standard_4pl_2880_ink
};

static const escp2_variable_inkset_t piezo_4pl_2880_quadtone_inks =
{
  &piezo_4pl_2880_quadtone_ink,
  NULL,
  NULL,
  NULL
};


static const escp2_variable_inkset_t escp2_6pl_standard_980_inks =
{
  &standard_980_6pl_ink,
  &standard_980_6pl_ink,
  &standard_980_6pl_ink,
  &standard_980_6pl_ink
};

static const escp2_variable_inkset_t piezo_6pl_quadtone_980_inks =
{
  &piezo_980_6pl_quadtone_ink,
  NULL,
  NULL,
  NULL
};

static const escp2_variable_inkset_t escp2_3pl_standard_inks =
{
  &standard_3pl_ink,
  &standard_3pl_ink,
  &standard_3pl_ink,
  &standard_3pl_ink
};

static const escp2_variable_inkset_t escp2_3pl_1440_standard_inks =
{
  &standard_3pl_1440_ink,
  &standard_3pl_1440_ink,
  &standard_3pl_1440_ink,
  &standard_3pl_1440_ink
};

static const escp2_variable_inkset_t escp2_3pl_2880_standard_inks =
{
  &standard_3pl_2880_ink,
  &standard_3pl_2880_ink,
  &standard_3pl_2880_ink,
  &standard_3pl_2880_ink
};

static const escp2_variable_inkset_t piezo_3pl_quadtone_inks =
{
  &piezo_3pl_quadtone_ink,
  NULL,
  NULL,
  NULL
};

static const escp2_variable_inkset_t piezo_3pl_1440_quadtone_inks =
{
  &piezo_3pl_1440_quadtone_ink,
  NULL,
  NULL,
  NULL
};

static const escp2_variable_inkset_t piezo_3pl_2880_quadtone_inks =
{
  &piezo_3pl_2880_quadtone_ink,
  NULL,
  NULL,
  NULL
};


static const escp2_variable_inkset_t escp2_2pl_2880_standard_inks =
{
  &standard_2pl_2880_ink,
  &standard_2pl_2880_ink,
  &standard_2pl_2880_ink,
  &standard_2pl_2880_ink
};

static const escp2_variable_inkset_t escp2_2pl_2880_photo_inks =
{
  &standard_2pl_2880_ink,
  &photo_2pl_2880_c_ink,
  &photo_2pl_2880_m_ink,
  &standard_2pl_2880_ink
};

static const escp2_variable_inkset_t escp2_2pl_2880_photo2_inks =
{
  &photo_2pl_2880_ink,
  &photo_2pl_2880_ink,
  &photo_2pl_2880_ink,
  &standard_2pl_2880_ink
};

static const escp2_variable_inkset_t escp2_2pl_2880_photoj_inks =
{
  &standard_2pl_2880_ink,
  &photo_2pl_2880_ink,
  &photo_2pl_2880_ink,
  &photo_2pl_2880_y_ink
};

static const escp2_variable_inkset_t piezo_2pl_2880_quadtone_inks =
{
  &piezo_2pl_2880_quadtone_ink,
  NULL,
  NULL,
  NULL
};

static const escp2_variable_inkset_t escp2_2pl_1440_standard_inks =
{
  &standard_2pl_1440_ink,
  &standard_2pl_1440_ink,
  &standard_2pl_1440_ink,
  &standard_2pl_1440_ink
};

static const escp2_variable_inkset_t escp2_2pl_1440_photo_inks =
{
  &standard_2pl_1440_ink,
  &photo_2pl_1440_ink,
  &photo_2pl_1440_ink,
  &standard_2pl_1440_ink
};

static const escp2_variable_inkset_t escp2_2pl_1440_photoj_inks =
{
  &standard_2pl_1440_ink,
  &photo_2pl_1440_ink,
  &photo_2pl_1440_ink,
  &photo_2pl_1440_y_ink,
};

static const escp2_variable_inkset_t piezo_2pl_1440_quadtone_inks =
{
  &piezo_2pl_1440_quadtone_ink,
  NULL,
  NULL,
  NULL
};

static const escp2_variable_inkset_t escp2_2pl_720_standard_inks =
{
  &standard_2pl_720_ink,
  &standard_2pl_720_ink,
  &standard_2pl_720_ink,
  &standard_2pl_720_ink
};

static const escp2_variable_inkset_t escp2_2pl_720_photo_inks =
{
  &standard_2pl_720_ink,
  &photo_2pl_720_ink,
  &photo_2pl_720_ink,
  &standard_2pl_720_ink
};

static const escp2_variable_inkset_t escp2_2pl_720_photo2_inks =
{
  &photo_2pl_720_ink,
  &photo_2pl_720_ink,
  &photo_2pl_720_ink,
  &standard_2pl_720_ink
};

static const escp2_variable_inkset_t escp2_2pl_720_photoj_inks =
{
  &standard_2pl_720_ink,
  &photo_2pl_720_ink,
  &photo_2pl_720_ink,
  &photo_2pl_720_y_ink
};

static const escp2_variable_inkset_t piezo_2pl_720_quadtone_inks =
{
  &piezo_2pl_720_quadtone_ink,
  NULL,
  NULL,
  NULL
};

static const escp2_variable_inkset_t escp2_2pl_360_standard_inks =
{
  &standard_2pl_360_ink,
  &standard_2pl_360_ink,
  &standard_2pl_360_ink,
  &standard_2pl_360_ink
};

static const escp2_variable_inkset_t escp2_2pl_360_photo_inks =
{
  &standard_2pl_360_ink,
  &photo_2pl_360_ink,
  &photo_2pl_360_ink,
  &standard_2pl_360_ink
};

static const escp2_variable_inkset_t escp2_2pl_360_photo2_inks =
{
  &photo_2pl_360_ink,
  &photo_2pl_360_ink,
  &photo_2pl_360_ink,
  &standard_2pl_360_ink
};

static const escp2_variable_inkset_t escp2_2pl_360_photoj_inks =
{
  &standard_2pl_360_ink,
  &photo_2pl_360_ink,
  &photo_2pl_360_ink,
  &photo_2pl_360_y_ink
};

static const escp2_variable_inkset_t piezo_2pl_360_quadtone_inks =
{
  &piezo_2pl_360_quadtone_ink,
  NULL,
  NULL,
  NULL
};


static const escp2_variable_inkset_t escp2_x80_multishot_standard_inks =
{
  &standard_x80_multishot_ink,
  &standard_x80_multishot_ink,
  &standard_x80_multishot_ink,
  &standard_x80_multishot_ink
};

static const escp2_variable_inkset_t escp2_x80_6pl_standard_inks =
{
  &standard_x80_6pl_ink,
  &standard_x80_6pl_ink,
  &standard_x80_6pl_ink,
  &standard_x80_6pl_ink
};

static const escp2_variable_inkset_t escp2_x80_1440_6pl_standard_inks =
{
  &standard_x80_1440_6pl_ink,
  &standard_x80_1440_6pl_ink,
  &standard_x80_1440_6pl_ink,
  &standard_x80_1440_6pl_ink
};

static const escp2_variable_inkset_t escp2_x80_2880_6pl_standard_inks =
{
  &standard_x80_2880_6pl_ink,
  &standard_x80_2880_6pl_ink,
  &standard_x80_2880_6pl_ink,
  &standard_x80_2880_6pl_ink
};

static const escp2_variable_inkset_t piezo_x80_multishot_quadtone_inks =
{
  &piezo_x80_multishot_quadtone_ink,
  NULL,
  NULL,
  NULL
};

static const escp2_variable_inkset_t piezo_x80_6pl_quadtone_inks =
{
  &piezo_x80_6pl_quadtone_ink,
  NULL,
  NULL,
  NULL
};

static const escp2_variable_inkset_t piezo_x80_1440_6pl_quadtone_inks =
{
  &piezo_x80_1440_6pl_quadtone_ink,
  NULL,
  NULL,
  NULL
};

static const escp2_variable_inkset_t piezo_x80_2880_6pl_quadtone_inks =
{
  &piezo_x80_2880_6pl_quadtone_ink,
  NULL,
  NULL,
  NULL
};


static const escp2_variable_inkset_t escp2_pigment_standard_inks =
{
  &standard_pigment_ink,
  &standard_pigment_ink,
  &standard_pigment_ink,
  &standard_pigment_ink
};

static const escp2_variable_inkset_t escp2_pigment_photo_inks =
{
  &standard_pigment_ink,
  &photo_pigment_ink,
  &photo_pigment_ink,
  &standard_pigment_ink
};

static const escp2_variable_inkset_t piezo_pigment_quadtone_inks =
{
  &piezo_pigment_quadtone_ink,
  NULL,
  NULL,
  NULL
};


static const escp2_variable_inkset_t escp2_multishot_pigment_standard_inks =
{
  &standard_multishot_pigment_ink,
  &standard_multishot_pigment_ink,
  &standard_multishot_pigment_ink,
  &standard_multishot_pigment_ink
};

static const escp2_variable_inkset_t piezo_multishot_pigment_quadtone_inks =
{
  &piezo_multishot_pigment_quadtone_ink,
  NULL,
  NULL,
  NULL
};

static const escp2_variable_inkset_t escp2_economy_pigment_standard_inks =
{
  &standard_economy_pigment_ink,
  &standard_economy_pigment_ink,
  &standard_economy_pigment_ink,
  &standard_economy_pigment_ink
};

static const escp2_variable_inkset_t piezo_economy_pigment_quadtone_inks =
{
  &piezo_economy_pigment_quadtone_ink,
  NULL,
  NULL,
  NULL
};


static const escp2_variable_inkset_t escp2_6pl_pigment_standard_inks =
{
  &standard_6pl_pigment_ink,
  &standard_6pl_pigment_ink,
  &standard_6pl_pigment_ink,
  &standard_6pl_pigment_ink
};

static const escp2_variable_inkset_t piezo_6pl_pigment_quadtone_inks =
{
  &piezo_6pl_pigment_quadtone_ink,
  NULL,
  NULL,
  NULL
};


static const escp2_variable_inkset_t escp2_4pl_pigment_low_standard_inks =
{
  &standard_4pl_pigment_low_ink,
  &standard_4pl_pigment_low_ink,
  &standard_4pl_pigment_low_ink,
  &standard_4pl_pigment_low_ink
};

static const escp2_variable_inkset_t escp2_4pl_pigment_low_photo_inks =
{
  &standard_4pl_pigment_low_ink,
  &photo_4pl_pigment_low_c_ink,
  &photo_4pl_pigment_low_m_ink,
  &standard_4pl_pigment_low_ink
};

static const escp2_variable_inkset_t escp2_4pl_pigment_low_photo2_inks =
{
  &photo_4pl_pigment_low_k_ink,
  &photo_4pl_pigment_low_c_ink,
  &photo_4pl_pigment_low_m_ink,
  &standard_4pl_pigment_low_ink
};

static const escp2_variable_inkset_t escp2_4pl_pigment_low_photoj_inks =
{
  &standard_4pl_pigment_low_ink,
  &photo_4pl_pigment_low_c_ink,
  &photo_4pl_pigment_low_m_ink,
  &photo_4pl_pigment_low_y_ink
};

static const escp2_variable_inkset_t escp2_4pl_pigment_standard_inks =
{
  &standard_4pl_pigment_ink,
  &standard_4pl_pigment_ink,
  &standard_4pl_pigment_ink,
  &standard_4pl_pigment_ink
};

static const escp2_variable_inkset_t escp2_4pl_pigment_photo_inks =
{
  &standard_4pl_pigment_ink,
  &photo_4pl_pigment_c_ink,
  &photo_4pl_pigment_m_ink,
  &standard_4pl_pigment_ink
};

static const escp2_variable_inkset_t escp2_4pl_pigment_photo2_inks =
{
  &photo_4pl_pigment_k_ink,
  &photo_4pl_pigment_c_ink,
  &photo_4pl_pigment_m_ink,
  &standard_4pl_pigment_ink
};

static const escp2_variable_inkset_t escp2_4pl_pigment_photoj_inks =
{
  &standard_4pl_pigment_ink,
  &photo_4pl_pigment_c_ink,
  &photo_4pl_pigment_m_ink,
  &photo_4pl_pigment_y_ink
};

static const escp2_variable_inkset_t escp2_4pl_pigment_1440_standard_inks =
{
  &standard_4pl_pigment_1440_ink,
  &standard_4pl_pigment_1440_ink,
  &standard_4pl_pigment_1440_ink,
  &standard_4pl_pigment_1440_ink
};

static const escp2_variable_inkset_t escp2_4pl_pigment_1440_photo_inks =
{
  &standard_4pl_pigment_1440_ink,
  &photo_4pl_pigment_1440_c_ink,
  &photo_4pl_pigment_1440_m_ink,
  &standard_4pl_pigment_1440_ink
};

static const escp2_variable_inkset_t escp2_4pl_pigment_1440_photo2_inks =
{
  &photo_4pl_pigment_1440_k_ink,
  &photo_4pl_pigment_1440_c_ink,
  &photo_4pl_pigment_1440_m_ink,
  &standard_4pl_pigment_1440_ink
};

static const escp2_variable_inkset_t escp2_4pl_pigment_1440_photoj_inks =
{
  &standard_4pl_pigment_1440_ink,
  &photo_4pl_pigment_1440_c_ink,
  &photo_4pl_pigment_1440_m_ink,
  &photo_4pl_pigment_1440_y_ink
};

static const escp2_variable_inkset_t escp2_4pl_pigment_2880_standard_inks =
{
  &standard_4pl_pigment_2880_ink,
  &standard_4pl_pigment_2880_ink,
  &standard_4pl_pigment_2880_ink,
  &standard_4pl_pigment_2880_ink
};

static const escp2_variable_inkset_t escp2_4pl_pigment_2880_photo_inks =
{
  &standard_4pl_pigment_2880_ink,
  &photo_4pl_pigment_2880_c_ink,
  &photo_4pl_pigment_2880_m_ink,
  &standard_4pl_pigment_2880_ink
};

static const escp2_variable_inkset_t escp2_4pl_pigment_2880_photo2_inks =
{
  &photo_4pl_pigment_2880_k_ink,
  &photo_4pl_pigment_2880_c_ink,
  &photo_4pl_pigment_2880_m_ink,
  &standard_4pl_pigment_2880_ink
};

static const escp2_variable_inkset_t escp2_4pl_pigment_2880_photoj_inks =
{
  &standard_4pl_pigment_2880_ink,
  &photo_4pl_pigment_2880_c_ink,
  &photo_4pl_pigment_2880_m_ink,
  &photo_4pl_pigment_2880_y_ink
};


static const escp2_variable_inkset_t escp2_3pl_pigment_standard_inks =
{
  &standard_3pl_pigment_ink,
  &standard_3pl_pigment_ink,
  &standard_3pl_pigment_ink,
  &standard_3pl_pigment_ink
};

static const escp2_variable_inkset_t piezo_3pl_pigment_quadtone_inks =
{
  &piezo_3pl_pigment_quadtone_ink,
  NULL,
  NULL,
  NULL
};

static const escp2_variable_inkset_t escp2_3pl_pigment_2880_standard_inks =
{
  &standard_3pl_pigment_2880_ink,
  &standard_3pl_pigment_2880_ink,
  &standard_3pl_pigment_2880_ink,
  &standard_3pl_pigment_2880_ink
};

static const escp2_variable_inkset_t piezo_3pl_pigment_2880_quadtone_inks =
{
  &piezo_3pl_pigment_2880_quadtone_ink,
  NULL,
  NULL,
  NULL
};


static const escp2_variable_inkset_t spro10000_standard_inks =
{
  &spro10000_standard_ink,
  &spro10000_standard_ink,
  &spro10000_standard_ink,
  &spro10000_standard_ink
};

static const escp2_variable_inkset_t spro10000_photo_inks =
{
  &spro10000_standard_ink,
  &spro10000_photo_ink,
  &spro10000_photo_ink,
  &spro10000_standard_ink
};



static const escp2_variable_inklist_t simple_inks =
{
  {
    &standard_inks,
    &standard_inks,
    &standard_inks,
    &standard_inks,
    &standard_inks,
    &standard_inks,
    &standard_inks,
    &standard_inks,
    &standard_inks,
  },
  {
    &photo_inks,
    &photo_inks,
    &photo_inks,
    &photo_inks,
    &photo_inks,
    &photo_inks,
    &photo_inks,
    &photo_inks,
    &photo_inks,
  },
  { NULL, },
  { NULL, },
  {
    &piezo_quadtone_inks,
    &piezo_quadtone_inks,
    &piezo_quadtone_inks,
    &piezo_quadtone_inks,
    &piezo_quadtone_inks,
    &piezo_quadtone_inks,
    &piezo_quadtone_inks,
    &piezo_quadtone_inks,
    &piezo_quadtone_inks,
  }
};

static const escp2_variable_inklist_t variable_6pl_inks =
{
  {
    &escp2_6pl_standard_inks,
    &escp2_6pl_standard_inks,
    &escp2_6pl_standard_inks,
    &escp2_6pl_standard_inks,
    &escp2_6pl_standard_inks,
    &escp2_6pl_1440_standard_inks,
    &escp2_6pl_2880_standard_inks,
    &escp2_6pl_2880_standard_inks,
    &escp2_6pl_2880_standard_inks
  },
  {
    &escp2_6pl_photo_inks,
    &escp2_6pl_photo_inks,
    &escp2_6pl_photo_inks,
    &escp2_6pl_photo_inks,
    &escp2_6pl_photo_inks,
    &escp2_6pl_1440_photo_inks,
    &escp2_6pl_2880_photo_inks,
    &escp2_6pl_2880_photo_inks,
    &escp2_6pl_2880_photo_inks
  },
  { NULL, },
  { NULL, },
  {
    &piezo_6pl_quadtone_inks,
    &piezo_6pl_quadtone_inks,
    &piezo_6pl_quadtone_inks,
    &piezo_6pl_quadtone_inks,
    &piezo_6pl_quadtone_inks,
    &piezo_6pl_1440_quadtone_inks,
    &piezo_6pl_2880_quadtone_inks,
    &piezo_6pl_2880_quadtone_inks,
    &piezo_6pl_2880_quadtone_inks
  },
};

static const escp2_variable_inklist_t variable_x80_6pl_inks =
{
  {
    &escp2_x80_multishot_standard_inks,
    &escp2_x80_multishot_standard_inks,
    &escp2_x80_multishot_standard_inks,
    &escp2_x80_multishot_standard_inks,
    &escp2_x80_6pl_standard_inks,
    &escp2_x80_1440_6pl_standard_inks,
    &escp2_x80_2880_6pl_standard_inks,
    &escp2_x80_2880_6pl_standard_inks,
    &escp2_x80_2880_6pl_standard_inks,
  },
  { NULL, },
  { NULL, },
  { NULL, },
  {
    &piezo_x80_multishot_quadtone_inks,
    &piezo_x80_multishot_quadtone_inks,
    &piezo_x80_multishot_quadtone_inks,
    &piezo_x80_multishot_quadtone_inks,
    &piezo_x80_6pl_quadtone_inks,
    &piezo_x80_1440_6pl_quadtone_inks,
    &piezo_x80_2880_6pl_quadtone_inks,
    &piezo_x80_2880_6pl_quadtone_inks,
    &piezo_x80_2880_6pl_quadtone_inks,
  }
};

static const escp2_variable_inklist_t variable_4pl_inks =
{
  {
    &escp2_multishot_standard_inks,
    &escp2_multishot_standard_inks,
    &escp2_multishot_standard_inks,
    &escp2_multishot_standard_inks,
    &escp2_6pl_standard_inks,
    &escp2_4pl_standard_inks,
    &escp2_4pl_2880_standard_inks,
    &escp2_4pl_2880_standard_inks,
    &escp2_4pl_2880_standard_inks,
  },
  {
    &escp2_multishot_photo_inks,
    &escp2_multishot_photo_inks,
    &escp2_multishot_photo_inks,
    &escp2_multishot_photo_inks,
    &escp2_6pl_photo_inks,
    &escp2_4pl_photo_inks,
    &escp2_4pl_2880_photo_inks,
    &escp2_4pl_2880_photo_inks,
    &escp2_4pl_2880_photo_inks
  },
  { NULL, },
  { NULL, },
  {
    &piezo_multishot_quadtone_inks,
    &piezo_multishot_quadtone_inks,
    &piezo_multishot_quadtone_inks,
    &piezo_multishot_quadtone_inks,
    &piezo_6pl_quadtone_inks,
    &piezo_4pl_quadtone_inks,
    &piezo_4pl_2880_quadtone_inks,
    &piezo_4pl_2880_quadtone_inks,
    &piezo_4pl_2880_quadtone_inks,
  }
};

static const escp2_variable_inklist_t variable_680_4pl_inks =
{
  {
    &escp2_680_multishot_standard_inks,
    &escp2_680_multishot_standard_inks,
    &escp2_680_multishot_standard_inks,
    &escp2_680_multishot_standard_inks,
    &escp2_680_6pl_standard_inks,
    &escp2_4pl_standard_inks,
    &escp2_4pl_2880_standard_inks,
    &escp2_4pl_2880_standard_inks,
    &escp2_4pl_2880_standard_inks,
  },
  { NULL, },
  { NULL, },
  { NULL, },
  {
    &piezo_680_multishot_quadtone_inks,
    &piezo_680_multishot_quadtone_inks,
    &piezo_680_multishot_quadtone_inks,
    &piezo_680_multishot_quadtone_inks,
    &piezo_680_6pl_quadtone_inks,
    &piezo_4pl_quadtone_inks,
    &piezo_4pl_2880_quadtone_inks,
    &piezo_4pl_2880_quadtone_inks,
    &piezo_4pl_2880_quadtone_inks,
  }
};

static const escp2_variable_inklist_t variable_3pl_inks =
{
  {
    &escp2_multishot_standard_inks,
    &escp2_multishot_standard_inks,
    &escp2_6pl_standard_980_inks,
    &escp2_6pl_standard_980_inks,
    &escp2_3pl_standard_inks,
    &escp2_3pl_1440_standard_inks,
    &escp2_3pl_2880_standard_inks,
    &escp2_3pl_2880_standard_inks,
    &escp2_3pl_2880_standard_inks,
  },
  { NULL, },
  { NULL, },
  { NULL, },
  {
    &piezo_multishot_quadtone_inks,
    &piezo_multishot_quadtone_inks,
    &piezo_6pl_quadtone_980_inks,
    &piezo_6pl_quadtone_980_inks,
    &piezo_3pl_quadtone_inks,
    &piezo_3pl_1440_quadtone_inks,
    &piezo_3pl_2880_quadtone_inks,
    &piezo_3pl_2880_quadtone_inks,
    &piezo_3pl_2880_quadtone_inks,
  }
};

static const escp2_variable_inklist_t variable_2pl_inks =
{
  {
    &escp2_2pl_360_standard_inks,
    &escp2_2pl_360_standard_inks,
    &escp2_2pl_360_standard_inks,
    &escp2_2pl_360_standard_inks,
    &escp2_2pl_720_standard_inks,
    &escp2_2pl_1440_standard_inks,
    &escp2_2pl_2880_standard_inks,
    &escp2_2pl_2880_standard_inks,
    &escp2_2pl_2880_standard_inks,
  },
  {
    &escp2_2pl_360_photo_inks,
    &escp2_2pl_360_photo_inks,
    &escp2_2pl_360_photo_inks,
    &escp2_2pl_360_photo_inks,
    &escp2_2pl_720_photo_inks,
    &escp2_2pl_1440_photo_inks,
    &escp2_2pl_2880_photo_inks,
    &escp2_2pl_2880_photo_inks,
    &escp2_2pl_2880_photo_inks
  },
  {
    &escp2_2pl_360_photoj_inks,
    &escp2_2pl_360_photoj_inks,
    &escp2_2pl_360_photoj_inks,
    &escp2_2pl_360_photoj_inks,
    &escp2_2pl_720_photoj_inks,
    &escp2_2pl_1440_photoj_inks,
    &escp2_2pl_2880_photoj_inks,
    &escp2_2pl_2880_photoj_inks,
    &escp2_2pl_2880_photoj_inks
  },
  { NULL, },
  {
    &piezo_2pl_360_quadtone_inks,
    &piezo_2pl_360_quadtone_inks,
    &piezo_2pl_360_quadtone_inks,
    &piezo_2pl_360_quadtone_inks,
    &piezo_2pl_720_quadtone_inks,
    &piezo_2pl_1440_quadtone_inks,
    &piezo_2pl_2880_quadtone_inks,
    &piezo_2pl_2880_quadtone_inks,
    &piezo_2pl_2880_quadtone_inks,
  }
};

static const escp2_variable_inklist_t variable_pigment_inks =
{
  {
    &escp2_pigment_standard_inks,
    &escp2_pigment_standard_inks,
    &escp2_pigment_standard_inks,
    &escp2_pigment_standard_inks,
    &escp2_pigment_standard_inks,
    &escp2_pigment_standard_inks,
    &escp2_pigment_standard_inks,
    &escp2_pigment_standard_inks,
    &escp2_pigment_standard_inks
  },
  {
    &escp2_pigment_photo_inks,
    &escp2_pigment_photo_inks,
    &escp2_pigment_photo_inks,
    &escp2_pigment_photo_inks,
    &escp2_pigment_photo_inks,
    &escp2_pigment_photo_inks,
    &escp2_pigment_photo_inks,
    &escp2_pigment_photo_inks,
    &escp2_pigment_photo_inks
  },
  { NULL, },
  { NULL, },
  {
    &piezo_pigment_quadtone_inks,
    &piezo_pigment_quadtone_inks,
    &piezo_pigment_quadtone_inks,
    &piezo_pigment_quadtone_inks,
    &piezo_pigment_quadtone_inks,
    &piezo_pigment_quadtone_inks,
    &piezo_pigment_quadtone_inks,
    &piezo_pigment_quadtone_inks,
    &piezo_pigment_quadtone_inks
  },
};

static const escp2_variable_inklist_t variable_4pl_pigment_inks =
{
  {
    &escp2_4pl_pigment_low_standard_inks,
    &escp2_4pl_pigment_low_standard_inks,
    &escp2_4pl_pigment_low_standard_inks,
    &escp2_4pl_pigment_low_standard_inks,
    &escp2_4pl_pigment_standard_inks,
    &escp2_4pl_pigment_1440_standard_inks,
    &escp2_4pl_pigment_2880_standard_inks,
    &escp2_4pl_pigment_2880_standard_inks,
    &escp2_4pl_pigment_2880_standard_inks,
  },
  {
    &escp2_4pl_pigment_low_photo_inks,
    &escp2_4pl_pigment_low_photo_inks,
    &escp2_4pl_pigment_low_photo_inks,
    &escp2_4pl_pigment_low_photo_inks,
    &escp2_4pl_pigment_photo_inks,
    &escp2_4pl_pigment_1440_photo_inks,
    &escp2_4pl_pigment_2880_photo_inks,
    &escp2_4pl_pigment_2880_photo_inks,
    &escp2_4pl_pigment_2880_photo_inks
  },
  { NULL, },
  {
    &escp2_4pl_pigment_low_photo2_inks,
    &escp2_4pl_pigment_low_photo2_inks,
    &escp2_4pl_pigment_low_photo2_inks,
    &escp2_4pl_pigment_low_photo2_inks,
    &escp2_4pl_pigment_photo2_inks,
    &escp2_4pl_pigment_1440_photo2_inks,
    &escp2_4pl_pigment_2880_photo2_inks,
    &escp2_4pl_pigment_2880_photo2_inks,
    &escp2_4pl_pigment_2880_photo2_inks
  },
};

static const escp2_variable_inklist_t variable_3pl_pigment_inks =
{
  {
    &escp2_economy_pigment_standard_inks,
    &escp2_economy_pigment_standard_inks,
    &escp2_multishot_pigment_standard_inks,
    &escp2_multishot_pigment_standard_inks,
    &escp2_6pl_pigment_standard_inks,
    &escp2_3pl_pigment_standard_inks,
    &escp2_3pl_pigment_2880_standard_inks,
    &escp2_3pl_pigment_2880_standard_inks,
    &escp2_3pl_pigment_2880_standard_inks,
  },
  { NULL, },
  { NULL, },
  { NULL, },
  {
    &piezo_economy_pigment_quadtone_inks,
    &piezo_economy_pigment_quadtone_inks,
    &piezo_multishot_pigment_quadtone_inks,
    &piezo_multishot_pigment_quadtone_inks,
    &piezo_6pl_pigment_quadtone_inks,
    &piezo_3pl_pigment_quadtone_inks,
    &piezo_3pl_pigment_2880_quadtone_inks,
    &piezo_3pl_pigment_2880_quadtone_inks,
    &piezo_3pl_pigment_2880_quadtone_inks,
  }
};

static const escp2_variable_inklist_t spro10000_inks =
{
  {
    &standard_inks,
    &spro10000_standard_inks,
    &spro10000_standard_inks,
    &spro10000_standard_inks,
    &spro10000_standard_inks,
    &spro10000_standard_inks,
    &spro10000_standard_inks,
    &spro10000_standard_inks,
    &spro10000_standard_inks
  },
  {
    &photo_inks,
    &spro10000_photo_inks,
    &spro10000_photo_inks,
    &spro10000_photo_inks,
    &spro10000_photo_inks,
    &spro10000_photo_inks,
    &spro10000_photo_inks,
    &spro10000_photo_inks,
    &spro10000_photo_inks
  }
};


#define DECLARE_INK_CHANNEL(name)				\
static const ink_channel_t name##_channels =			\
{								\
  name##_subchannels,						\
  sizeof(name##_subchannels) / sizeof(physical_subchannel_t),	\
}

static const physical_subchannel_t standard_black_subchannels[] =
{
  { 0, -1, 0 }
};

DECLARE_INK_CHANNEL(standard_black);

static const physical_subchannel_t x80_black_subchannels[] =
{
  { 0, -1, 48 }
};

DECLARE_INK_CHANNEL(x80_black);

static const physical_subchannel_t c80_black_subchannels[] =
{
  { 0, -1, 0 }
};

DECLARE_INK_CHANNEL(c80_black);

static const physical_subchannel_t standard_cyan_subchannels[] =
{
  { 2, -1, 0 }
};

DECLARE_INK_CHANNEL(standard_cyan);

static const physical_subchannel_t x80_cyan_subchannels[] =
{
  { 2, -1, 96 }
};

DECLARE_INK_CHANNEL(x80_cyan);

static const physical_subchannel_t c80_cyan_subchannels[] =
{
  { 2, -1, 0 }
};

DECLARE_INK_CHANNEL(c80_cyan);

static const physical_subchannel_t standard_magenta_subchannels[] =
{
  { 1, -1, 0 }
};

DECLARE_INK_CHANNEL(standard_magenta);

static const physical_subchannel_t x80_magenta_subchannels[] =
{
  { 1, -1, 48 }
};

DECLARE_INK_CHANNEL(x80_magenta);

static const physical_subchannel_t c80_magenta_subchannels[] =
{
  { 1, -1, 120 }
};

DECLARE_INK_CHANNEL(c80_magenta);

static const physical_subchannel_t standard_yellow_subchannels[] =
{
  { 4, -1, 0 }
};

DECLARE_INK_CHANNEL(standard_yellow);

static const physical_subchannel_t x80_yellow_subchannels[] =
{
  { 4, -1, 0 }
};

DECLARE_INK_CHANNEL(x80_yellow);

static const physical_subchannel_t c80_yellow_subchannels[] =
{
  { 4, -1, 240 }
};

DECLARE_INK_CHANNEL(c80_yellow);

static const physical_subchannel_t photo_black_subchannels[] =
{
  { 0, 0, 0 }
};

DECLARE_INK_CHANNEL(photo_black);

static const physical_subchannel_t photo_cyan_subchannels[] =
{
  { 2, 0, 0 },
  { 2, 1, 0 }
};

DECLARE_INK_CHANNEL(photo_cyan);

static const physical_subchannel_t photo_magenta_subchannels[] =
{
  { 1, 0, 0 },
  { 1, 1, 0 }
};

DECLARE_INK_CHANNEL(photo_magenta);

static const physical_subchannel_t photo_yellow_subchannels[] =
{
  { 4, 0, 0 }
};

DECLARE_INK_CHANNEL(photo_yellow);

/* For Japanese 7-color printers, with dark yellow */
static const physical_subchannel_t photo2_yellow_subchannels[] =
{
  { 4, 0, 0 },
  { 4, 2, 0 }
};

DECLARE_INK_CHANNEL(photo2_yellow);

static const physical_subchannel_t photo2_black_subchannels[] =
{
  { 0, 0, 0 },
  { 0, 1, 0 }
};

DECLARE_INK_CHANNEL(photo2_black);

static const physical_subchannel_t quadtone_subchannels[] =
{
  { 4, -1, 0 },
  { 1, -1, 0 },
  { 2, -1, 0 },
  { 0, -1, 0 }
};

DECLARE_INK_CHANNEL(quadtone);

static const physical_subchannel_t c80_quadtone_subchannels[] =
{
  { 4, -1, 240 },
  { 1, -1, 120 },
  { 2, -1, 0 },
  { 0, -1, 0 }
};

DECLARE_INK_CHANNEL(c80_quadtone);

static const escp2_inkname_t three_color_composite_inkset =
{
  "RGB", N_ ("Three Color Composite"), 1, INKSET_CMYK, 0, 0,
  standard_lum_adjustment, standard_hue_adjustment, standard_sat_adjustment,
  {
    NULL, &standard_cyan_channels,
    &standard_magenta_channels, &standard_yellow_channels
  }
};

static const escp2_inkname_t x80_three_color_composite_inkset =
{
  "RGB", N_ ("Three Color Composite"), 1, INKSET_CMYK, 0, 0,
  standard_lum_adjustment, standard_hue_adjustment, standard_sat_adjustment,
  {
    NULL, &x80_cyan_channels,
    &x80_magenta_channels, &x80_yellow_channels
  }
};

static const escp2_inkname_t c80_three_color_composite_inkset =
{
  "RGB", N_ ("Three Color Composite"), 1, INKSET_CMYK, 0, 0,
  standard_lum_adjustment, standard_hue_adjustment, standard_sat_adjustment,
  {
    NULL, &c80_cyan_channels,
    &c80_magenta_channels, &c80_yellow_channels
  }
};

static const escp2_inkname_t four_color_standard_inkset =
{
  "CMYK", N_ ("Four Color Standard"), 1, INKSET_CMYK, .25, 1.0,
  standard_lum_adjustment, standard_hue_adjustment, standard_sat_adjustment,
  {
    &standard_black_channels, &standard_cyan_channels,
    &standard_magenta_channels, &standard_yellow_channels
  }
};

static const escp2_inkname_t x80_four_color_standard_inkset =
{
  "CMYK", N_ ("Four Color Standard"), 1, INKSET_CMYK, .25, 1.0,
  standard_lum_adjustment, standard_hue_adjustment, standard_sat_adjustment,
  {
    &x80_black_channels, &x80_cyan_channels,
    &x80_magenta_channels, &x80_yellow_channels
  }
};

static const escp2_inkname_t c80_four_color_standard_inkset =
{
  "CMYK", N_ ("Four Color Standard"), 1, INKSET_CMYK, .25, 1.0,
  standard_lum_adjustment, standard_hue_adjustment, standard_sat_adjustment,
  {
    &c80_black_channels, &c80_cyan_channels,
    &c80_magenta_channels, &c80_yellow_channels
  }
};

static const escp2_inkname_t six_color_photo_inkset =
{
  "PhotoCMYK", N_ ("Six Color Photo"), 1, INKSET_CcMmYK, .5, 1.0,
  standard_lum_adjustment, standard_hue_adjustment, standard_sat_adjustment,
  {
    &photo_black_channels, &photo_cyan_channels,
    &photo_magenta_channels, &photo_yellow_channels
  }
};

static const escp2_inkname_t five_color_photo_composite_inkset =
{
  "PhotoCMY", N_ ("Five Color Photo Composite"), 1, INKSET_CcMmYK, 0, 0,
  standard_lum_adjustment, standard_hue_adjustment, standard_sat_adjustment,
  {
    NULL, &photo_cyan_channels,
    &photo_magenta_channels, &photo_yellow_channels
  }
};

static const escp2_inkname_t j_seven_color_enhanced_inkset =
{
  "Photo7J", N_ ("Seven Color Enhanced"), 1, INKSET_CcMmYyK, .5, 1.0,
  standard_lum_adjustment, standard_hue_adjustment, standard_sat_adjustment,
  {
    &photo_black_channels, &photo_cyan_channels,
    &photo_magenta_channels, &photo2_yellow_channels
  }
};

static const escp2_inkname_t j_six_color_enhanced_composite_inkset =
{
  "PhotoEnhanceJ", N_ ("Six Color Enhanced Composite"), 1, INKSET_CcMmYyK, .5, 1.0,
  standard_lum_adjustment, standard_hue_adjustment, standard_sat_adjustment,
  {
    NULL, &standard_cyan_channels,
    &standard_magenta_channels, &standard_yellow_channels
  }
};

static const escp2_inkname_t seven_color_photo_inkset =
{
  "PhotoCMYK7", N_ ("Seven Color Photo"), 1, INKSET_CcMmYKk, .05 , 1.0,
  standard_lum_adjustment, standard_hue_adjustment, standard_sat_adjustment,
  {
    &photo2_black_channels, &photo_cyan_channels,
    &photo_magenta_channels, &photo_yellow_channels
  }
};

static const escp2_inkname_t two_color_grayscale_inkset =
{
  "Gray2", N_ ("Two Level Grayscale"), 0, INKSET_CcMmYKk, 0, 0,
  NULL, NULL, NULL,
  {
    &photo2_black_channels, NULL, NULL, NULL
  }
};

static const escp2_inkname_t piezo_quadtone_inkset =
{
  "Quadtone", N_ ("Quadtone"), 0, INKSET_PIEZO_QUADTONE, 0, 0,
  NULL, NULL, NULL,
  {
    &quadtone_channels, NULL, NULL, NULL
  }
};

static const escp2_inkname_t c80_piezo_quadtone_inkset =
{
  "Quadtone", N_ ("Quadtone"), 0, INKSET_PIEZO_QUADTONE, 0, 0,
  NULL, NULL, NULL,
  {
    &c80_quadtone_channels, NULL, NULL, NULL
  }
};

#define DECLARE_INKLIST(name)				\
static const inklist_t name##_inklist =			\
{							\
  name##_ink_types,					\
  sizeof(name##_ink_types) / sizeof(escp2_inkname_t *),	\
}							\


static const escp2_inkname_t *const cmy_ink_types[] =
{
  &three_color_composite_inkset
};

DECLARE_INKLIST(cmy);

static const escp2_inkname_t *const standard_ink_types[] =
{
  &four_color_standard_inkset,
  &three_color_composite_inkset,
  &piezo_quadtone_inkset
};

DECLARE_INKLIST(standard);

static const escp2_inkname_t *const c80_ink_types[] =
{
  &c80_four_color_standard_inkset,
  &c80_three_color_composite_inkset,
  &c80_piezo_quadtone_inkset
};

DECLARE_INKLIST(c80);

static const escp2_inkname_t *const x80_ink_types[] =
{
  &x80_four_color_standard_inkset,
  &x80_three_color_composite_inkset,
};

DECLARE_INKLIST(x80);

static const escp2_inkname_t *const photo_ink_types[] =
{
  &six_color_photo_inkset,
  &five_color_photo_composite_inkset,
  &four_color_standard_inkset,
  &three_color_composite_inkset,
  &piezo_quadtone_inkset
};

DECLARE_INKLIST(photo);

static const escp2_inkname_t *const photo7_japan_ink_types[] =
{
  &j_seven_color_enhanced_inkset,
  &j_six_color_enhanced_composite_inkset,
  &six_color_photo_inkset,
  &five_color_photo_composite_inkset,
  &four_color_standard_inkset,
  &three_color_composite_inkset,
  &piezo_quadtone_inkset
};

DECLARE_INKLIST(photo7_japan);

static const escp2_inkname_t *const photo7_ink_types[] =
{
  &seven_color_photo_inkset,
  &six_color_photo_inkset,
  &five_color_photo_composite_inkset,
  &four_color_standard_inkset,
  &three_color_composite_inkset,
  &two_color_grayscale_inkset
};

DECLARE_INKLIST(photo7);



static const paper_t standard_papers[] =
{
  { "Plain", N_("Plain Paper"),
    1, 0, 0.80, .1, .5, 1.0, 1.0, 1.0, .9, 1.05, 1.15,
    1, 1.0, 0x6b, 0x1a, 0x01, NULL, plain_paper_lum_adjustment, NULL},
  { "PlainFast", N_("Plain Paper Fast Load"),
    5, 0, 0.80, .1, .5, 1.0, 1.0, 1.0, .9, 1.05, 1.15,
    1, 1.0, 0x6b, 0x1a, 0x01, NULL, plain_paper_lum_adjustment, NULL},
  { "Postcard", N_("Postcard"),
    2, 0, 0.83, .2, .6, 1.0, 1.0, 1.0, .9, 1.0, 1.1,
    1, 1.0, 0x00, 0x00, 0x02, NULL, plain_paper_lum_adjustment, NULL},
  { "GlossyFilm", N_("Glossy Film"),
    3, 0, 1.00 ,1, .999, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0,
    1, 1.0, 0x6d, 0x00, 0x01, NULL, plain_paper_lum_adjustment, NULL},
  { "Transparency", N_("Transparencies"),
    3, 0, 1.00, 1, .999, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0,
    1.0, 1.0, 0x6d, 0x00, 0x02, NULL, plain_paper_lum_adjustment, NULL},
  { "Envelope", N_("Envelopes"),
    4, 0, 0.80, .125, .5, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0,
    1, 1.0, 0x6b, 0x1a, 0x01, NULL, plain_paper_lum_adjustment, NULL},
  { "BackFilm", N_("Back Light Film"),
    6, 0, 1.00, 1, .999, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0,
    1, 1.0, 0x6d, 0x00, 0x01, NULL, NULL, NULL},
  { "Matte", N_("Matte Paper"),
    7, 0, 0.85, 1.0, .999, 1.05, 1.0, 0.95, .9, 1.0, 1.1,
    1, 1.0, 0x00, 0x00, 0x02, NULL, NULL, NULL},
  { "Inkjet", N_("Inkjet Paper"),
    7, 0, 0.85, .25, .6, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0,
    1, 1.0, 0x6b, 0x1a, 0x01, NULL, plain_paper_lum_adjustment, NULL},
  { "Coated", N_("Photo Quality Inkjet Paper"),
    7, 0, 1.00, 1.0, .999, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0,
    1, 1.0, 0x6b, 0x1a, 0x01, NULL, NULL, NULL},
  { "Photo", N_("Photo Paper"),
    8, 0, 1.00, 1.0, .9, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0,
    1, 1.0, 0x67, 0x00, 0x02, NULL, NULL, NULL},
  { "GlossyPhoto", N_("Premium Glossy Photo Paper"),
    8, 0, 1.10, 1, .999, 1.0, 1.0, 1.0, 1.0, 1.03, 1.0,
    1, 1.0, 0x80, 0x00, 0x02,
    pgpp_hue_adjustment, pgpp_lum_adjustment, pgpp_sat_adjustment},
  { "Luster", N_("Premium Luster Photo Paper"),
    8, 0, 1.00, 1, .999, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0,
    1.0, 1.0, 0x80, 0x00, 0x02, NULL, NULL, NULL},
  { "GlossyPaper", N_("Photo Quality Glossy Paper"),
    6, 0, 1.00, 1, .999, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0,
    1.0, 1.0, 0x6b, 0x1a, 0x01, NULL, NULL, NULL},
  { "Ilford", N_("Ilford Heavy Paper"),
    8, 0, .85, .5, 1.35, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0,
    1, 1.0, 0x80, 0x00, 0x02, NULL, NULL, NULL },
  { "ColorLife", N_("ColorLife Paper"),
    8, 0, 1.00, 1.0, .9, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0,
    1, 1.0, 0x67, 0x00, 0x02, NULL, NULL, NULL},
  { "Other", N_("Other"),
    0, 0, 0.80, 0.125, .5, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0,
    1, 1.0, 0x6b, 0x1a, 0x01, NULL, plain_paper_lum_adjustment, NULL},
};

static const paperlist_t standard_paper_list =
{
  sizeof(standard_papers) / sizeof(paper_t),
  standard_papers
};

static const paper_t sp780_papers[] =
{
  { "Plain", N_("Plain Paper"),
    6, 0, 0.80, .1, .5, 1.0, 1.0, 1.0, .9, 1.05, 1.15,
    1, 1.0, 0x6b, 0x1a, 0x01, NULL, plain_paper_lum_adjustment, NULL},
  { "PlainFast", N_("Plain Paper Fast Load"),
    1, 0, 0.80, .1, .5, 1.0, 1.0, 1.0, .9, 1.05, 1.15,
    1, 1.0, 0x6b, 0x1a, 0x01, NULL, plain_paper_lum_adjustment, NULL},
  { "Postcard", N_("Postcard"),
    3, 0, 0.83, .2, .6, 1.0, 1.0, 1.0, .9, 1.0, 1.1,
    1, 1.0, 0x00, 0x00, 0x02, NULL, plain_paper_lum_adjustment, NULL},
  { "GlossyFilm", N_("Glossy Film"),
    0, 0, 1.00 ,1, .999, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0,
    1, 1.0, 0x6d, 0x00, 0x01, NULL, plain_paper_lum_adjustment, NULL},
  { "Transparency", N_("Transparencies"),
    0, 0, 1.00, 1, .999, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0,
    1.0, 1.0, 0x6d, 0x00, 0x02, NULL, plain_paper_lum_adjustment, NULL},
  { "Envelope", N_("Envelopes"),
    4, 0, 0.80, .125, .5, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0,
    1, 1.0, 0x6b, 0x1a, 0x01, NULL, plain_paper_lum_adjustment, NULL},
  { "BackFilm", N_("Back Light Film"),
    0, 0, 1.00, 1, .999, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0,
    1, 1.0, 0x6d, 0x00, 0x01, NULL, NULL, NULL},
  { "Matte", N_("Matte Paper"),
    2, 0, 0.85, 1.0, .999, 1.05, .9, 1.05, .9, 1.0, 1.1,
    1, 1.0, 0x00, 0x00, 0x02, NULL, NULL, NULL},
  { "Inkjet", N_("Inkjet Paper"),
    6, 0, 0.85, .25, .6, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0,
    1, 1.0, 0x6b, 0x1a, 0x01, NULL, plain_paper_lum_adjustment, NULL},
  { "Coated", N_("Photo Quality Inkjet Paper"),
    0, 0, 1.00, 1.0, .999, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0,
    1, 1.0, 0x6b, 0x1a, 0x01, NULL, NULL, NULL},
  { "Photo", N_("Photo Paper"),
    2, 0, 1.00, 1.0, .9, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0,
    1, 1.0, 0x67, 0x00, 0x02, NULL, NULL, NULL},
  { "GlossyPhoto", N_("Premium Glossy Photo Paper"),
    7, 0, 1.10, 1, .999, 1.0, 1.0, 1.0, 1.0, 1.03, 1.0,
    1, 1.0, 0x80, 0x00, 0x02,
    pgpp_hue_adjustment, pgpp_lum_adjustment, pgpp_sat_adjustment},
  { "Luster", N_("Premium Luster Photo Paper"),
    7, 0, 1.00, 1, .999, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0,
    1.0, 1.0, 0x80, 0x00, 0x02, NULL, NULL, NULL},
  { "GlossyPaper", N_("Photo Quality Glossy Paper"),
    0, 0, 1.00, 1, .999, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0,
    1.0, 1.0, 0x6b, 0x1a, 0x01, NULL, NULL, NULL},
  { "Ilford", N_("Ilford Heavy Paper"),
    2, 0, .85, .5, 1.35, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0,
    1, 1.0, 0x80, 0x00, 0x02, NULL, NULL, NULL },
  { "ColorLife", N_("ColorLife Paper"),
    2, 0, 1.00, 1.0, .9, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0,
    1, 1.0, 0x67, 0x00, 0x02, NULL, NULL, NULL},
  { "Other", N_("Other"),
    0, 0, 0.80, 0.125, .5, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0,
    1, 1.0, 0x6b, 0x1a, 0x01, NULL, plain_paper_lum_adjustment, NULL},
};

static const paperlist_t sp780_paper_list =
{
  sizeof(sp780_papers) / sizeof(paper_t),
  sp780_papers
};

static const paper_t c80_papers[] =
{
  { "Plain", N_("Plain Paper"),
    1, 0, 0.80, .1, .5, 1.0, 1.0, 1.0, .9, 1.05, 1.15,
    1, 1.0, 0x6b, 0x1a, 0x01, NULL, plain_paper_lum_adjustment, NULL},
  { "PlainFast", N_("Plain Paper Fast Load"),
    5, 0, 0.80, .1, .5, 1.0, 1.0, 1.0, .9, 1.05, 1.15,
    1, 1.0, 0x6b, 0x1a, 0x01, NULL, plain_paper_lum_adjustment, NULL},
  { "Postcard", N_("Postcard"),
    2, 0, 0.83, .2, .6, 1.0, 1.0, 1.0, .9, 1.0, 1.1,
    1, 1.0, 0x00, 0x00, 0x02, NULL, plain_paper_lum_adjustment, NULL},
  { "GlossyFilm", N_("Glossy Film"),
    3, 0, 1.00 ,1, .999, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0,
    1, 1.0, 0x6d, 0x00, 0x01, NULL, plain_paper_lum_adjustment, NULL},
  { "Transparency", N_("Transparencies"),
    3, 0, 1.00, 1, .999, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0,
    1.0, 1.0, 0x6d, 0x00, 0x02, NULL, plain_paper_lum_adjustment, NULL},
  { "Envelope", N_("Envelopes"),
    4, 0, 0.80, .125, .5, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0,
    1, 1.0, 0x6b, 0x1a, 0x01, NULL, plain_paper_lum_adjustment, NULL},
  { "BackFilm", N_("Back Light Film"),
    6, 0, 1.00, 1, .999, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0,
    1, 1.0, 0x6d, 0x00, 0x01, NULL, NULL, NULL},
  { "Matte", N_("Matte Paper"),
    7, 0, 0.9, 1.0, .999, 1.0, 1.0, 1.0, .9, 1.0, 1.1,
    1, 1.0, 0x00, 0x00, 0x02, NULL, NULL, NULL},
  { "Inkjet", N_("Inkjet Paper"),
    7, 0, 0.85, .25, .6, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0,
    1, 1.0, 0x6b, 0x1a, 0x01, NULL, plain_paper_lum_adjustment, NULL},
  { "Coated", N_("Photo Quality Inkjet Paper"),
    7, 0, 1.00, 1.0, .999, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0,
    1, 1.0, 0x6b, 0x1a, 0x01, NULL, NULL, NULL},
  { "Photo", N_("Photo Paper"),
    8, 0, 1.20, 1.0, .9, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0,
    1, 1.0, 0x67, 0x00, 0x02, NULL, NULL, NULL},
  { "GlossyPhoto", N_("Premium Glossy Photo Paper"),
    8, 0, 1.00, 1, .999, 1.0, 1.0, 1.0, 1.0, 1.03, 1.0,
    1, 1.0, 0x80, 0x00, 0x02,
    pgpp_hue_adjustment, pgpp_lum_adjustment, pgpp_sat_adjustment},
  { "Luster", N_("Premium Luster Photo Paper"),
    8, 0, 1.00, 1, .999, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0,
    1.0, 1.0, 0x80, 0x00, 0x02, NULL, NULL, NULL},
  { "GlossyPaper", N_("Photo Quality Glossy Paper"),
    6, 0, 1.00, 1, .999, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0,
    1.0, 1.0, 0x6b, 0x1a, 0x01, NULL, NULL, NULL},
  { "Ilford", N_("Ilford Heavy Paper"),
    8, 0, .85, .5, 1.35, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0,
    1, 1.0, 0x80, 0x00, 0x02, NULL, NULL, NULL },
  { "ColorLife", N_("ColorLife Paper"),
    8, 0, 1.20, 1.0, .9, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0,
    1, 1.0, 0x67, 0x00, 0x02, NULL, NULL, NULL},
  { "Other", N_("Other"),
    0, 0, 0.80, 0.125, .5, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0,
    1, 1.0, 0x6b, 0x1a, 0x01, NULL, plain_paper_lum_adjustment, NULL},
};

static const paperlist_t c80_paper_list =
{
  sizeof(c80_papers) / sizeof(paper_t),
  c80_papers
};

static const paper_t sp950_papers[] =
{
  { "Plain", N_("Plain Paper"),
    6, 0, 0.80, .1, .5, 1.0, 1.0, 1.0, .9, 1.05, 1.15,
    1, 1.0, 0x6b, 0x1a, 0x01, NULL, plain_paper_lum_adjustment, NULL},
  { "PlainFast", N_("Plain Paper Fast Load"),
    1, 0, 0.80, .1, .5, 1.0, 1.0, 1.0, .9, 1.05, 1.15,
    1, 1.0, 0x6b, 0x1a, 0x01, NULL, plain_paper_lum_adjustment, NULL},
  { "Postcard", N_("Postcard"),
    3, 0, 0.83, .2, .6, 1.0, 1.0, 1.0, .9, 1.0, 1.1,
    1, 1.0, 0x00, 0x00, 0x02, NULL, plain_paper_lum_adjustment, NULL},
  { "GlossyFilm", N_("Glossy Film"),
    0, 0, 1.00 ,1, .999, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0,
    1, 1.0, 0x6d, 0x00, 0x01, NULL, plain_paper_lum_adjustment, NULL},
  { "Transparency", N_("Transparencies"),
    0, 0, 1.00, 1, .999, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0,
    1.0, 1.0, 0x6d, 0x00, 0x02, NULL, plain_paper_lum_adjustment, NULL},
  { "Envelope", N_("Envelopes"),
    4, 0, 0.80, .125, .5, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0,
    1, 1.0, 0x6b, 0x1a, 0x01, NULL, plain_paper_lum_adjustment, NULL},
  { "BackFilm", N_("Back Light Film"),
    0, 0, 1.00, 1, .999, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0,
    1, 1.0, 0x6d, 0x00, 0x01, NULL, NULL, NULL},
  { "Matte", N_("Matte Paper"),
    2, 0, 0.85, 1.0, .999, 1.05, .9, 1.05, .9, 1.0, 1.1,
    1, 1.0, 0x00, 0x00, 0x02, NULL, NULL, NULL},
  { "Inkjet", N_("Inkjet Paper"),
    6, 0, 0.85, .25, .6, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0,
    1, 1.0, 0x6b, 0x1a, 0x01, NULL, plain_paper_lum_adjustment, NULL},
  { "Coated", N_("Photo Quality Inkjet Paper"),
    0, 0, 1.00, 1.0, .999, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0,
    1, 1.0, 0x6b, 0x1a, 0x01, NULL, NULL, NULL},
  { "Photo", N_("Photo Paper"),
    2, 0, 1.00, 1.0, .9, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0,
    1, 1.0, 0x67, 0x00, 0x02, NULL, NULL, NULL},
  { "GlossyPhoto", N_("Premium Glossy Photo Paper"),
    7, 0, 0.85, 1.0, .999, 0.9, 1.04, 0.93, 0.9, 1.04, 0.93,
    0.9, 1.0, 0x80, 0x00, 0x02,
    pgpp_hue_adjustment, pgpp_lum_adjustment, pgpp_sat_adjustment},
  { "Luster", N_("Premium Luster Photo Paper"),
    7, 0, 1.00, 1, .999, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0,
    1.0, 1.0, 0x80, 0x00, 0x02, NULL, NULL, NULL},
  { "GlossyPaper", N_("Photo Quality Glossy Paper"),
    0, 0, 1.00, 1, .999, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0,
    1.0, 1.0, 0x6b, 0x1a, 0x01, NULL, NULL, NULL},
  { "Ilford", N_("Ilford Heavy Paper"),
    2, 0, .85, .5, 1.35, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0,
    1, 1.0, 0x80, 0x00, 0x02, NULL, NULL, NULL },
  { "ColorLife", N_("ColorLife Paper"),
    2, 0, 1.00, 1.0, .9, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0,
    1, 1.0, 0x67, 0x00, 0x02, NULL, NULL, NULL},
  { "Other", N_("Other"),
    0, 0, 0.80, 0.125, .5, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0,
    1, 1.0, 0x6b, 0x1a, 0x01, NULL, plain_paper_lum_adjustment, NULL},
};

static const paperlist_t sp950_paper_list =
{
  sizeof(sp950_papers) / sizeof(paper_t),
  sp950_papers
};

/*
 * Dot sizes are for:
 *
 *  0: 120/180 DPI micro
 *  1: 120/180 DPI soft
 *  2: 360 micro
 *  3: 360 soft
 *  4: 720x360 micro
 *  5: 720x360 soft
 *  6: 720 micro
 *  7: 720 soft
 *  8: 1440x720 micro
 *  9: 1440x720 soft
 * 10: 2880x720 micro
 * 11: 2880x720 soft
 * 12: 2880x1440
 */

/*   0     1     2     3     4     5     6     7     8     9    10    11    12 */

static const escp2_dot_size_t g1_dotsizes =
{ -2,     -1,   -2,   -1,   -1,   -2,   -2,   -1,   -1,   -1,   -1,   -1,   -1 };

static const escp2_dot_size_t g2_dotsizes =
{ -2,     -1,   -2,   -1,   -2,   -2,   -2,   -2,   -1,   -1,   -1,   -1,   -1 };

static const escp2_dot_size_t sc600_dotsizes =
{  4,     -1,    4,   -1,   -1,    3,    2,    2,   -1,    1,   -1,   -1,   -1 };

static const escp2_dot_size_t g3_dotsizes =
{  3,     -1,    3,   -1,   -1,    2,    1,    1,   -1,    1,   -1,   -1,   -1 };

static const escp2_dot_size_t photo_dotsizes =
{  3,     -1,    3,   -1,   -1,    2,   -1,    1,   -1,    4,   -1,   -1,   -1 };

static const escp2_dot_size_t sp5000_dotsizes =
{ -1,      3,   -1,    3,   -1,    2,   -1,    1,   -1,    4,   -1,   -1,   -1 };

static const escp2_dot_size_t sc440_dotsizes =
{  3,     -1,    3,   -1,   -1,    2,   -1,    1,   -1,   -1,   -1,   -1,   -1 };

static const escp2_dot_size_t sc640_dotsizes =
{  3,     -1,    3,   -1,   -1,    2,    1,    1,   -1,    1,   -1,   -1,   -1 };

static const escp2_dot_size_t c6pl_dotsizes =
{ -1,   0x10,   -1, 0x10,   -1, 0x10,   -1, 0x10,   -1, 0x10,   -1, 0x10, 0x10 };

static const escp2_dot_size_t c3pl_dotsizes =
{ -1,   0x11,   -1, 0x11,   -1, 0x11,   -1, 0x10,   -1, 0x10,   -1, 0x10, 0x10 };

static const escp2_dot_size_t c4pl_dotsizes =
{ -1,   0x12,   -1, 0x12,   -1, 0x12,   -1, 0x11,   -1, 0x10,   -1, 0x10, 0x10 };

static const escp2_dot_size_t sc720_dotsizes =
{ -1,   0x12,   -1, 0x12,   -1, 0x11,   -1, 0x11,   -1, 0x11,   -1,   -1,   -1 };

static const escp2_dot_size_t sc660_dotsizes =
{ -1,      3,    3,   -1,    3,    0,   -1,    0,   -1,    0,   -1,   -1,   -1 };

static const escp2_dot_size_t sc480_dotsizes =
{ -1,   0x13,   -1, 0x13,   -1, 0x13,   -1, 0x10,   -1, 0x10,   -1, 0x10, 0x10 };

static const escp2_dot_size_t sc670_dotsizes =
{ -1,   0x12,   -1, 0x12,   -1, 0x12,   -1, 0x11,   -1, 0x11,   -1,   -1,   -1 };

static const escp2_dot_size_t sp2000_dotsizes =
{ -1,   0x11,   -1, 0x11,   -1, 0x11,   -1, 0x10,   -1, 0x10,   -1,   -1,   -1 };

static const escp2_dot_size_t spro_dye_dotsizes =
{    3,   -1,    3,   -1,    3,   -1,    1,   -1,    1,   -1,   -1,   -1,   -1 };

static const escp2_dot_size_t spro_pigment_dotsizes =
{    3,   -1,    3,   -1,    2,   -1,    1,   -1,    1,   -1,   -1,   -1,   -1 };

static const escp2_dot_size_t spro10000_dotsizes =
{    4,   -1, 0x11,   -1, 0x11,   -1, 0x10,   -1, 0x10,   -1,   -1,   -1,   -1 };

static const escp2_dot_size_t c3pl_pigment_dotsizes =
{   -1, 0x10,   -1, 0x10,   -1, 0x10,   -1, 0x11,   -1, 0x12,   -1, 0x12,  0x12 };

static const escp2_dot_size_t c2pl_dotsizes =
{   -1, 0x12,   -1, 0x12,   -1, 0x12,   -1, 0x11,   -1, 0x13,   -1, 0x13,  0x10 };

static const escp2_dot_size_t c4pl_pigment_dotsizes =
{ -1,   0x12,   -1, 0x12,   -1, 0x12,   -1, 0x11,   -1, 0x11,   -1, 0x10, 0x10 };

static const escp2_dot_size_t spro_c4pl_pigment_dotsizes =
{ 0x11,   -1, 0x11,   -1, 0x11,   -1, 0x10,   -1, 0x10,   -1,    5,    5,    5 };

/*
 * Bits are for:
 *
 *  0: 120/180 DPI micro
 *  1: 120/180 DPI soft
 *  2: 360 micro
 *  3: 360 soft
 *  4: 720x360 micro
 *  5: 720x360 soft
 *  6: 720 micro
 *  7: 720 soft
 *  8: 1440x720 micro
 *  9: 1440x720 soft
 * 10: 2880x720 micro
 * 11: 2880x720 soft
 * 12: 2880x1440
 */

/*   0     1     2     3     4     5     6     7     8     9    10    11    12 */

static const escp2_bits_t variable_bits =
{    2,    2,    2,    2,    2,    2,    2,    2,    2,    2,    2,    2,    2 };

static const escp2_bits_t stp950_bits =
{    2,    2,    2,    2,    2,    2,    2,    2,    2,    2,    2,    2,    1 };

static const escp2_bits_t ultrachrome_bits =
{    2,    2,    2,    2,    2,    2,    2,    2,    2,    2,    1,    1,    1 };

static const escp2_bits_t standard_bits =
{    1,    1,    1,    1,    1,    1,    1,    1,    1,    1,    1,    1,    1 };

/*
 * Base resolutions are for:
 *
 *  0: 120/180 DPI micro
 *  1: 120/180 DPI soft
 *  2: 360 micro
 *  3: 360 soft
 *  4: 720x360 micro
 *  5: 720x360 soft
 *  6: 720 micro
 *  7: 720 soft
 *  8: 1440x720 micro
 *  9: 1440x720 soft
 * 10: 2880x720 micro
 * 11: 2880x720 soft
 * 12: 2880x1440
 */

/*   0     1     2     3     4     5     6     7     8     9    10    11    12 */

static const escp2_base_resolutions_t standard_base_res =
{  720,  720,  720,  720,  720,  720,  720,  720,  720,  720,  720,  720,  720 };

static const escp2_base_resolutions_t g3_base_res =
{  720,  720,  720,  720,  720,  720,  720,  720,  360,  360,  360,  360,  360 };

static const escp2_base_resolutions_t variable_base_res =
{  360,  360,  360,  360,  360,  360,  360,  360,  360,  360,  360,  360,  360 };

static const escp2_base_resolutions_t stp950_base_res =
{  360,  360,  360,  360,  360,  360,  360,  360,  360,  360,  720,  720,  720 };

static const escp2_base_resolutions_t ultrachrome_base_res =
{  360,  360,  360,  360,  360,  360,  360,  360,  360,  360,  720,  720,  720 };

static const escp2_base_resolutions_t stc900_base_res =
{  360,  360,  360,  360,  360,  360,  360,  360,  180,  180,  180,  180,  180 };

static const escp2_base_resolutions_t pro_base_res =
{ 2880, 2880, 2880, 2880, 2880, 2880, 2880, 2880, 2880, 2880, 2880, 2880, 2880 };

/*
 * Densities are for:
 *
 *  0: 120/180 DPI micro
 *  1: 120/180 DPI soft
 *  2: 360 micro
 *  3: 360 soft
 *  4: 720x360 micro
 *  5: 720x360 soft
 *  6: 720 micro
 *  7: 720 soft
 *  8: 1440x720 micro
 *  9: 1440x720 soft
 * 10: 2880x720 micro
 * 11: 2880x720 soft
 * 12: 2880x1440
 */

/*  0    1    2    3    4     5      6     7      8      9     10     11     12   */

static const escp2_densities_t g1_densities =
{ 2.0, 2.0, 1.3, 1.3, 1.3,  1.3,  0.568, 0.568,   0.0,   0.0,   0.0,   0.0,   0.0 };

static const escp2_densities_t sc1500_densities =
{ 2.0, 2.0, 1.3, 1.3, 1.3,  1.3,  0.631, 0.631,   0.0,   0.0,   0.0,   0.0,   0.0 };

static const escp2_densities_t g3_densities =
{ 2.0, 2.0, 1.3, 1.3, 1.3,  1.3,  0.775, 0.775, 0.388, 0.388, 0.194, 0.194, 0.097 };

static const escp2_densities_t photo_densities =
{ 2.0, 2.0, 1.3, 1.3, 1.3,  1.3,  0.775, 0.775, 0.55,  0.55,  0.275, 0.275, 0.138 };

static const escp2_densities_t sc440_densities =
{ 3.0, 3.0, 2.0, 2.0, 1.0,  1.0,  0.900, 0.900, 0.45,  0.45,  0.45,  0.45,  0.113 };

static const escp2_densities_t sc480_densities =
{ 2.0, 2.0, 0.0, 1.4, 0.0,  0.7,  0.0,   0.710, 0.0,   0.710, 0.0,   0.546, 0.273 };

static const escp2_densities_t sc980_densities =
{ 2.0, 2.0, 1.3, 1.3, 0.65, 0.65, 0.646, 0.511, 0.49,  0.49,  0.637, 0.637, 0.455 };

static const escp2_densities_t c6pl_densities =
{ 2.0, 2.0, 1.3, 2.0, 0.65, 1.0,  0.646, 0.568, 0.323, 0.568, 0.568, 0.568, 0.284 };

static const escp2_densities_t c3pl_densities =
{ 2.0, 2.0, 1.3, 1.3, 0.65, 0.65, 0.646, 0.73,  0.7,   0.7,   0.91,  0.91,  0.455 };

static const escp2_densities_t sc680_densities =
{ 2.0, 2.0, 1.2, 1.2, 0.60, 0.60, 0.792, 0.792, 0.792, 0.792, 0.594, 0.594, 0.297 };

static const escp2_densities_t c4pl_densities =
{ 2.0, 2.0, 1.3, 1.3, 0.65, 0.65, 0.431, 0.568, 0.784, 0.784, 0.593, 0.593, 0.297 };

static const escp2_densities_t sc660_densities =
{ 3.0, 3.0, 2.0, 2.0, 1.0,  1.0,  0.646, 0.646, 0.323, 0.323, 0.162, 0.162, 0.081 };

static const escp2_densities_t sp2000_densities =
{ 2.0, 2.0, 1.3, 1.3, 0.65, 0.65, 0.775, 0.852, 0.388, 0.438, 0.219, 0.219, 0.110 };

static const escp2_densities_t spro_dye_densities =
{ 2.0, 2.0, 1.3, 1.3, 1.3,  1.3,  0.775, 0.775, 0.388, 0.388, 0.275, 0.275, 0.138 };

static const escp2_densities_t spro_pigment_densities =
{ 2.0, 2.0, 1.5, 1.5, 0.78, 0.78, 0.775, 0.775, 0.388, 0.388, 0.194, 0.194, 0.097 };

static const escp2_densities_t spro10000_densities =
{ 2.0, 2.0, 1.3, 1.3, 0.65, 0.65, 0.431, 0.710, 0.216, 0.784, 0.392, 0.392, 0.196 };

static const escp2_densities_t c3pl_pigment_densities =
{ 2.0, 2.0, 1.3, 1.3, 0.69, 0.69, 0.511, 0.511, 0.765, 0.765, 0.585, 0.585, 0.293 };

static const escp2_densities_t c2pl_densities =
{ 2.0, 2.0, 1.15,1.15,0.57, 0.57, 0.650, 0.650, 0.650, 0.650, 0.650, 0.650, 0.650 };

static const escp2_densities_t c4pl_pigment_densities =
{ 2.0, 2.0, 1.35,1.35,0.68, 0.68, 0.518, 0.518, 0.518, 0.518, 0.518, 0.518, 0.259 };


static const res_t standard_reslist[] =
{
  { "360x90dpi",        N_("360 x 90 DPI Fast Economy Draft"),
    360,  90,   360,  90,   0,  0, 1, 1, 0, 1, 1, RES_120_M },
  { "360x90sw",         N_("360 x 90 DPI Fast Economy Draft"),
    360,  90,   360,  90,   1,  0, 1, 1, 0, 1, 1, RES_120 },

  { "360x120dpi",       N_("360 x 120 DPI Economy Draft"),
    360,  120,  360,  120,  0,  0, 1, 1, 0, 3, 1, RES_120_M },
  { "360x120sw",        N_("360 x 120 DPI Economy Draft"),
    360,  120,  360,  120,  1,  0, 1, 1, 0, 3, 1, RES_120 },

  { "180dpi",           N_("180 DPI Economy Draft"),
    180,  180,  180,  180,  0,  0, 1, 1, 0, 1, 1, RES_180_M },
  { "180sw",            N_("180 DPI Economy Draft"),
    180,  180,  180,  180,  1,  0, 1, 1, 0, 1, 1, RES_180 },

  { "360x240dpi",       N_("360 x 240 DPI Draft"),
    360,  240,  360,  240,  0,  0, 1, 1, 0, 3, 2, RES_180_M },
  { "360x240sw",        N_("360 x 240 DPI Draft"),
    360,  240,  360,  240,  1,  0, 1, 1, 0, 3, 2, RES_180 },

  { "360x180dpi",       N_("360 x 180 DPI Draft"),
    360,  180,  360,  180,  0,  0, 1, 1, 0, 1, 1, RES_180_M },
  { "360x180sw",        N_("360 x 180 DPI Draft"),
    360,  180,  360,  180,  1,  0, 1, 1, 0, 1, 1, RES_180 },

  { "360sw",            N_("360 DPI"),
    360,  360,  360,  360,  1,  0, 1, 1, 0, 1, 1, RES_360 },
  { "360swuni",         N_("360 DPI Unidirectional"),
    360,  360,  360,  360,  1,  0, 1, 1, 1, 1, 1, RES_360 },
  { "360mw",            N_("360 DPI Microweave"),
    360,  360,  360,  360,  0,  1, 1, 1, 0, 1, 1, RES_360_M },
  { "360mwuni",         N_("360 DPI Microweave Unidirectional"),
    360,  360,  360,  360,  0,  1, 1, 1, 1, 1, 1, RES_360_M },
  { "360dpi",           N_("360 DPI"),
    360,  360,  360,  360,  0,  0, 1, 1, 0, 1, 1, RES_360_M },
  { "360uni",           N_("360 DPI Unidirectional"),
    360,  360,  360,  360,  0,  0, 1, 1, 1, 1, 1, RES_360_M },

  { "720x360sw",        N_("720 x 360 DPI"),
    720,  360,  720,  360,  1,  0, 1, 1, 0, 2, 1, RES_720_360 },
  { "720x360swuni",     N_("720 x 360 DPI Unidirectional"),
    720,  360,  720,  360,  1,  0, 1, 1, 1, 2, 1, RES_720_360 },

  { "720mw",            N_("720 DPI Microweave"),
    720,  720,  720,  720,  0,  1, 1, 1, 0, 1, 1, RES_720_M },
  { "720mwuni",         N_("720 DPI Microweave Unidirectional"),
    720,  720,  720,  720,  0,  1, 1, 1, 1, 1, 1, RES_720_M },
  { "720sw",            N_("720 DPI"),
    720,  720,  720,  720,  1,  0, 1, 1, 0, 1, 1, RES_720 },
  { "720swuni",         N_("720 DPI Unidirectional"),
    720,  720,  720,  720,  1,  0, 1, 1, 1, 1, 1, RES_720 },
  { "720hq",            N_("720 DPI High Quality"),
    720,  720,  720,  720,  1,  0, 2, 1, 0, 1, 1, RES_720 },
  { "720hquni",         N_("720 DPI High Quality Unidirectional"),
    720,  720,  720,  720,  1,  0, 2, 1, 1, 1, 1, RES_720 },
  { "720hq2",           N_("720 DPI Highest Quality"),
    720,  720,  720,  720,  1,  0, 4, 1, 1, 1, 1, RES_720 },

  { "1440x720mw",       N_("1440 x 720 DPI Microweave"),
    1440, 720,  1440, 720,  0,  1, 1, 1, 0, 1, 1, RES_1440_720_M },
  { "1440x720mwuni",    N_("1440 x 720 DPI Microweave Unidirectional"),
    1440, 720,  1440, 720,  0,  1, 1, 1, 1, 1, 1, RES_1440_720_M },
  { "1440x720sw",       N_("1440 x 720 DPI"),
    1440, 720,  1440, 720,  1,  0, 1, 1, 0, 1, 1, RES_1440_720 },
  { "1440x720swuni",    N_("1440 x 720 DPI Unidirectional"),
    1440, 720,  1440, 720,  1,  0, 1, 1, 1, 1, 1, RES_1440_720 },
  { "1440x720hq2",      N_("1440 x 720 DPI Highest Quality"),
    1440, 720,  1440, 720,  1,  0, 2, 1, 1, 1, 1, RES_1440_720 },

  {"2880x720sw",       N_("2880 x 720 DPI"),
    2880, 720,  2880, 720,  1,  0, 1, 1, 0, 1, 1, RES_2880_720},
  { "2880x720swuni",    N_("2880 x 720 DPI Unidirectional"),
    2880, 720,  2880, 720,  1,  0, 1, 1, 1, 1, 1, RES_2880_720},

  { "1440x1440sw",      N_("1440 x 1440 DPI"),
    1440, 1440, 1440, 1440, 1,  0, 1, 1, 1, 1, 1, RES_1440_1440},
  { "1440x1440hq2",     N_("1440 x 1440 DPI Highest Quality"),
    1440, 1440, 1440, 1440, 1,  0, 2, 1, 1, 1, 1, RES_1440_1440},

  { "2880x1440sw",      N_("2880 x 1440 DPI"),
    2880, 1440, 2880, 1440, 1,  0, 1, 1, 1, 1, 1, RES_2880_1440},

  { "", "", 0, 0, 0, 0, 0, 0, 0, 0, 1, -1 }
};

static const res_t sp5000_reslist[] =
{
  { "180sw",            N_("180 DPI Economy Draft"),
    180,  180,  180,  180,  1,  0, 1, 1, 0, 4, 1, RES_180 },

  { "360x180sw",        N_("360 x 180 DPI Draft"),
    360,  180,  360,  180,  1,  0, 1, 1, 0, 4, 1, RES_180 },

  { "360sw",            N_("360 DPI"),
    360,  360,  360,  360,  1,  0, 1, 1, 0, 2, 1, RES_360 },
  { "360swuni",         N_("360 DPI Unidirectional"),
    360,  360,  360,  360,  1,  0, 1, 1, 1, 2, 1, RES_360 },

  { "720x360sw",        N_("720 x 360 DPI"),
    720,  360,  720,  360,  1,  0, 1, 1, 0, 2, 1, RES_720_360 },
  { "720x360swuni",     N_("720 x 360 DPI Unidirectional"),
    720,  360,  720,  360,  1,  0, 1, 1, 1, 2, 1, RES_720_360 },

  { "720sw",            N_("720 DPI"),
    720,  720,  720,  720,  1,  0, 1, 1, 0, 1, 1, RES_720 },
  { "720swuni",         N_("720 DPI Unidirectional"),
    720,  720,  720,  720,  1,  0, 1, 1, 1, 1, 1, RES_720 },
  { "720hq",            N_("720 DPI High Quality"),
    720,  720,  720,  720,  1,  0, 2, 1, 0, 1, 1, RES_720 },
  { "720hquni",         N_("720 DPI High Quality Unidirectional"),
    720,  720,  720,  720,  1,  0, 2, 1, 1, 1, 1, RES_720 },
  { "720hq2",           N_("720 DPI Highest Quality"),
    720,  720,  720,  720,  1,  0, 4, 1, 1, 1, 1, RES_720 },

  { "1440x720sw",       N_("1440 x 720 DPI"),
    1440, 720,  1440, 720,  1,  0, 1, 1, 0, 1, 1, RES_1440_720 },
  { "1440x720swuni",    N_("1440 x 720 DPI Unidirectional"),
    1440, 720,  1440, 720,  1,  0, 1, 1, 1, 1, 1, RES_1440_720 },
  { "1440x720hq2",      N_("1440 x 720 DPI Highest Quality"),
    1440, 720,  1440, 720,  1,  0, 2, 1, 1, 1, 1, RES_1440_720 },

  { "", "", 0, 0, 0, 0, 0, 0, 0, 0, 1, -1 }
};

static const res_t escp950_reslist[] =
{
  { "360x180dpi",       N_("360 x 180 DPI Draft"),
    360,  180,  360,  180,  0,  0, 1, 1, 0, 1, 1, RES_180_M },
  { "360x180sw",        N_("360 x 180 DPI Draft"),
    360,  180,  360,  180,  1,  0, 1, 1, 0, 1, 1, RES_180 },

  { "360sw",            N_("360 DPI"),
    360,  360,  360,  360,  1,  0, 1, 1, 0, 1, 1, RES_360 },
  { "360swuni",         N_("360 DPI Unidirectional"),
    360,  360,  360,  360,  1,  0, 1, 1, 1, 1, 1, RES_360 },

  { "720x360sw",        N_("720 x 360 DPI"),
    720,  360,  720,  360,  1,  0, 1, 1, 0, 2, 1, RES_720_360 },
  { "720x360swuni",     N_("720 x 360 DPI Unidirectional"),
    720,  360,  720,  360,  1,  0, 1, 1, 1, 2, 1, RES_720_360 },

  { "720sw",            N_("720 DPI"),
    720,  720,  720,  720,  1,  0, 1, 1, 0, 1, 1, RES_720 },
  { "720swuni",         N_("720 DPI Unidirectional"),
    720,  720,  720,  720,  1,  0, 1, 1, 1, 1, 1, RES_720 },
  { "720hq",            N_("720 DPI High Quality"),
    720,  720,  720,  720,  1,  0, 2, 1, 0, 1, 1, RES_720 },
  { "720hquni",         N_("720 DPI High Quality Unidirectional"),
    720,  720,  720,  720,  1,  0, 2, 1, 1, 1, 1, RES_720 },
  { "720hq2",           N_("720 DPI Highest Quality"),
    720,  720,  720,  720,  1,  0, 4, 1, 1, 1, 1, RES_720 },

  { "1440x720sw",       N_("1440 x 720 DPI"),
    1440, 720,  1440, 720,  1,  0, 1, 1, 0, 1, 1, RES_1440_720 },
  { "1440x720swuni",    N_("1440 x 720 DPI Unidirectional"),
    1440, 720,  1440, 720,  1,  0, 1, 1, 1, 1, 1, RES_1440_720 },
  { "1440x720hq2",      N_("1440 x 720 DPI Highest Quality"),
    1440, 720,  1440, 720,  1,  0, 2, 1, 1, 1, 1, RES_1440_720 },

  { "2880x720sw",       N_("2880 x 720 DPI"),
    2880, 1440, 2880, 720,  1,  0, 1, 1, 0, 1, 1, RES_2880_1440},
  { "2880x720swuni",    N_("2880 x 720 DPI Unidirectional"),
    2880, 1440, 2880, 720,  1,  0, 1, 1, 1, 1, 1, RES_2880_1440},

  { "1440x1440sw",      N_("1440 x 1440 DPI"),
    2880, 1440, 1440, 1440, 1,  0, 1, 1, 1, 1, 1, RES_2880_1440},
  { "1440x1440hq2",     N_("1440 x 1440 DPI Highest Quality"),
    2880, 1440, 1440, 1440, 1,  0, 1, 1, 1, 1, 1, RES_2880_1440},

  { "2880x1440sw",      N_("2880 x 1440 DPI"),
    2880, 1440, 2880, 1440, 1,  0, 1, 1, 1, 1, 1, RES_2880_1440},

  { "", "", 0, 0, 0, 0, 0, 0, 0, 0, 1, -1 }
};

static const res_t escp2200_reslist[] =
{
  { "360x180dpi",       N_("360 x 180 DPI Draft"),
    360,  180,  360,  180,  0,  0, 1, 1, 0, 1, 1, RES_180_M },
  { "360x180sw",        N_("360 x 180 DPI Draft"),
    360,  180,  360,  180,  1,  0, 1, 1, 0, 1, 1, RES_180 },

  { "360sw",            N_("360 DPI"),
    360,  360,  360,  360,  1,  0, 1, 1, 0, 1, 1, RES_360 },
  { "360swuni",         N_("360 DPI Unidirectional"),
    360,  360,  360,  360,  1,  0, 1, 1, 1, 1, 1, RES_360 },

  { "720x360sw",        N_("720 x 360 DPI"),
    720,  360,  720,  360,  1,  0, 1, 1, 0, 2, 1, RES_720_360 },
  { "720x360swuni",     N_("720 x 360 DPI Unidirectional"),
    720,  360,  720,  360,  1,  0, 1, 1, 1, 2, 1, RES_720_360 },

  { "720sw",            N_("720 DPI"),
    720,  720,  720,  720,  1,  0, 1, 1, 0, 1, 1, RES_720 },
  { "720swuni",         N_("720 DPI Unidirectional"),
    720,  720,  720,  720,  1,  0, 1, 1, 1, 1, 1, RES_720 },
  { "720hq",            N_("720 DPI High Quality"),
    720,  720,  720,  720,  1,  0, 2, 1, 0, 1, 1, RES_720 },
  { "720hquni",         N_("720 DPI High Quality Unidirectional"),
    720,  720,  720,  720,  1,  0, 2, 1, 1, 1, 1, RES_720 },
  { "720hq2",           N_("720 DPI Highest Quality"),
    720,  720,  720,  720,  1,  0, 4, 1, 1, 1, 1, RES_720 },

  { "1440x720sw",       N_("1440 x 720 DPI"),
    1440, 720,  1440, 720,  1,  0, 1, 1, 0, 1, 1, RES_1440_720 },
  { "1440x720swuni",    N_("1440 x 720 DPI Unidirectional"),
    1440, 720,  1440, 720,  1,  0, 1, 1, 1, 1, 1, RES_1440_720 },
  { "1440x720hq2",      N_("1440 x 720 DPI Highest Quality"),
    1440, 720,  1440, 720,  1,  0, 2, 1, 1, 1, 1, RES_1440_720 },

  { "2880x720sw",       N_("2880 x 720 DPI"),
    2880, 720,  2880, 720,  1,  0, 1, 1, 0, 1, 1, RES_2880_720},
  { "2880x720swuni",    N_("2880 x 720 DPI Unidirectional"),
    2880, 720,  2880, 720,  1,  0, 1, 1, 1, 1, 1, RES_2880_720},

  { "1440x1440sw",      N_("1440 x 1440 DPI"),
    2880, 1440, 1440, 1440, 1,  0, 1, 1, 1, 1, 1, RES_2880_1440},
  { "1440x1440hq2",     N_("1440 x 1440 DPI Highest Quality"),
    2880, 1440, 1440, 1440, 1,  0, 1, 1, 1, 1, 1, RES_2880_1440},

  { "2880x1440sw",      N_("2880 x 1440 DPI"),
    2880, 1440, 2880, 1440, 1,  0, 1, 1, 1, 1, 1, RES_2880_1440},

  { "", "", 0, 0, 0, 0, 0, 0, 0, 0, 1, -1 }
};

static const res_t no_microweave_reslist[] =
{
  { "360x90dpi",        N_("360 x 90 DPI Fast Economy Draft"),
    360,  90,   360,  90,   0,  0, 1, 1, 0, 1, 1, RES_120_M },
  { "360x90sw",         N_("360 x 90 DPI Fast Economy Draft"),
    360,  90,   360,  90,   1,  0, 1, 1, 0, 1, 1, RES_120 },

  { "360x120dpi",       N_("360 x 120 DPI Economy Draft"),
    360,  120,  360,  120,  0,  0, 1, 1, 0, 3, 1, RES_120_M },
  { "360x120sw",        N_("360 x 120 DPI Economy Draft"),
    360,  120,  360,  120,  1,  0, 1, 1, 0, 3, 1, RES_120 },

  { "180dpi",           N_("180 DPI Economy Draft"),
    180,  180,  180,  180,  0,  0, 1, 1, 0, 1, 1, RES_180_M },
  { "180sw",            N_("180 DPI Economy Draft"),
    180,  180,  180,  180,  1,  0, 1, 1, 0, 1, 1, RES_180 },

  { "360x240dpi",       N_("360 x 240 DPI Draft"),
    360,  240,  360,  240,  0,  0, 1, 1, 0, 3, 2, RES_180_M },
  { "360x240sw",        N_("360 x 240 DPI Draft"),
    360,  240,  360,  240,  1,  0, 1, 1, 0, 3, 2, RES_180 },

  { "360x180dpi",       N_("360 x 180 DPI Draft"),
    360,  180,  360,  180,  0,  0, 1, 1, 0, 1, 1, RES_180_M },
  { "360x180sw",        N_("360 x 180 DPI Draft"),
    360,  180,  360,  180,  1,  0, 1, 1, 0, 1, 1, RES_180 },

  { "360sw",            N_("360 DPI"),
    360,  360,  360,  360,  1,  0, 1, 1, 0, 1, 1, RES_360 },
  { "360swuni",         N_("360 DPI Unidirectional"),
    360,  360,  360,  360,  1,  0, 1, 1, 1, 1, 1, RES_360 },
  { "360dpi",           N_("360 DPI"),
    360,  360,  360,  360,  0,  0, 1, 1, 0, 1, 1, RES_360_M },
  { "360uni",           N_("360 DPI Unidirectional"),
    360,  360,  360,  360,  0,  0, 1, 1, 1, 1, 1, RES_360_M },

  { "720x360sw",        N_("720 x 360 DPI"),
    720,  360,  720,  360,  1,  0, 1, 1, 0, 2, 1, RES_720_360 },
  { "720x360swuni",     N_("720 x 360 DPI Unidirectional"),
    720,  360,  720,  360,  1,  0, 1, 1, 1, 2, 1, RES_720_360 },

  { "720sw",            N_("720 DPI"),
    720,  720,  720,  720,  1,  0, 1, 1, 0, 1, 1, RES_720 },
  { "720swuni",         N_("720 DPI Unidirectional"),
    720,  720,  720,  720,  1,  0, 1, 1, 1, 1, 1, RES_720 },
  { "720hq",            N_("720 DPI High Quality"),
    720,  720,  720,  720,  1,  0, 2, 1, 0, 1, 1, RES_720 },
  { "720hquni",         N_("720 DPI High Quality Unidirectional"),
    720,  720,  720,  720,  1,  0, 2, 1, 1, 1, 1, RES_720 },
  { "720hq2",           N_("720 DPI Highest Quality"),
    720,  720,  720,  720,  1,  0, 4, 1, 1, 1, 1, RES_720 },

  { "1440x720sw",       N_("1440 x 720 DPI"),
    1440, 720,  1440, 720,  1,  0, 1, 1, 0, 1, 1, RES_1440_720 },
  { "1440x720swuni",    N_("1440 x 720 DPI Unidirectional"),
    1440, 720,  1440, 720,  1,  0, 1, 1, 1, 1, 1, RES_1440_720 },
  { "1440x720hq2",      N_("1440 x 720 DPI Highest Quality"),
    1440, 720,  1440, 720,  1,  0, 2, 1, 1, 1, 1, RES_1440_720 },

  { "2880x720sw",       N_("2880 x 720 DPI"),
    2880, 720,  2880, 720,  1,  0, 1, 1, 0, 1, 1, RES_2880_720},
  { "2880x720swuni",    N_("2880 x 720 DPI Unidirectional"),
    2880, 720,  2880, 720,  1,  0, 1, 1, 1, 1, 1, RES_2880_720},

  { "1440x1440sw",      N_("1440 x 1440 DPI"),
    1440, 1440, 1440, 1440, 1,  0, 1, 1, 1, 1, 1, RES_1440_1440},
  { "1440x1440hq2",     N_("1440 x 1440 DPI Highest Quality"),
    1440, 1440, 1440, 1440, 1,  0, 2, 1, 1, 1, 1, RES_1440_1440},

  { "2880x1440sw",      N_("2880 x 1440 DPI"),
    2880, 1440, 2880, 1440, 1,  0, 1, 1, 1, 1, 1, RES_2880_1440},

  { "", "", 0, 0, 0, 0, 0, 0, 0, 0, 1, -1 }
};

static const res_t pro_reslist[] =
{
  { "360x90dpi",        N_("360 x 90 DPI Fast Economy Draft"),
    360,  90,   360,  90,   0,  0, 1, 1, 0, 1, 1, RES_120_M },

  { "360x120dpi",       N_("360 x 120 DPI Economy Draft"),
    360,  120,  360,  120,  0,  0, 1, 1, 0, 3, 1, RES_120_M },

  { "180dpi",           N_("180 DPI Economy Draft"),
    180,  180,  180,  180,  0,  0, 1, 1, 0, 1, 1, RES_180_M },

  { "360x240dpi",       N_("360 x 240 DPI Draft"),
    360,  240,  360,  240,  0,  0, 1, 1, 0, 3, 2, RES_180_M },

  { "360x180dpi",       N_("360 x 180 DPI Draft"),
    360,  180,  360,  180,  0,  0, 1, 1, 0, 1, 1, RES_180_M },

  { "360mw",            N_("360 DPI Microweave"),
    360,  360,  360,  360,  0,  1, 1, 1, 0, 1, 1, RES_360_M },
  { "360mwuni",         N_("360 DPI Microweave Unidirectional"),
    360,  360,  360,  360,  0,  1, 1, 1, 1, 1, 1, RES_360_M },
  { "360dpi",           N_("360 DPI"),
    360,  360,  360,  360,  0,  0, 1, 1, 0, 1, 1, RES_360_M },
  { "360uni",           N_("360 DPI Unidirectional"),
    360,  360,  360,  360,  0,  0, 1, 1, 1, 1, 1, RES_360_M },
  { "360fol",           N_("360 DPI Full Overlap"),
    360,  360,  360,  360,  0,  2, 1, 1, 0, 1, 1, RES_360_M },
  { "360foluni",        N_("360 DPI Full Overlap Unidirectional"),
    360,  360,  360,  360,  0,  2, 1, 1, 1, 1, 1, RES_360_M },
  { "360fol2",          N_("360 DPI FOL2"),
    360,  360,  360,  360,  0,  4, 1, 1, 0, 1, 1, RES_360_M },
  { "360fol2uni",       N_("360 DPI FOL2 Unidirectional"),
    360,  360,  360,  360,  0,  4, 1, 1, 1, 1, 1, RES_360_M },
  { "360mw2",           N_("360 DPI MW2"),
    360,  360,  360,  360,  0,  5, 1, 1, 0, 1, 1, RES_360_M },
  { "360mw2uni",        N_("360 DPI MW2 Unidirectional"),
    360,  360,  360,  360,  0,  5, 1, 1, 1, 1, 1, RES_360_M },

  { "720x360dpi",       N_("720 x 360 DPI"),
    720,  360,  720,  360,  0,  0, 1, 1, 0, 2, 1, RES_720_360_M },
  { "720x360uni",       N_("720 x 360 DPI Unidirectional"),
    720,  360,  720,  360,  0,  0, 1, 1, 1, 2, 1, RES_720_360_M },
  { "720x360mw",        N_("720 x 360 DPI Microweave"),
    720,  360,  720,  360,  0,  1, 1, 1, 0, 2, 1, RES_720_360_M },
  { "720x360mwuni",     N_("720 x 360 DPI Microweave Unidirectional"),
    720,  360,  720,  360,  0,  1, 1, 1, 1, 2, 1, RES_720_360_M },
  { "720x360fol",       N_("720 x 360 DPI FOL"),
    720,  360,  720,  360,  0,  2, 1, 1, 0, 2, 1, RES_720_360_M },
  { "720x360foluni",    N_("720 x 360 DPI FOL Unidirectional"),
    720,  360,  720,  360,  0,  2, 1, 1, 1, 2, 1, RES_720_360_M },
  { "720x360fol2",      N_("720 x 360 DPI FOL2"),
    720,  360,  720,  360,  0,  4, 1, 1, 0, 2, 1, RES_720_360_M },
  { "720x360fol2uni",   N_("720 x 360 DPI FOL2 Unidirectional"),
    720,  360,  720,  360,  0,  4, 1, 1, 1, 2, 1, RES_720_360_M },
  { "720x360mw2",       N_("720 x 360 DPI MW2"),
    720,  360,  720,  360,  0,  5, 1, 1, 0, 2, 1, RES_720_360_M },
  { "720x360mw2uni",    N_("720 x 360 DPI MW2 Unidirectional"),
    720,  360,  720,  360,  0,  5, 1, 1, 1, 2, 1, RES_720_360_M },

  { "720mw",            N_("720 DPI Microweave"),
    720,  720,  720,  720,  0,  1, 1, 1, 0, 1, 1, RES_720_M },
  { "720mwuni",         N_("720 DPI Microweave Unidirectional"),
    720,  720,  720,  720,  0,  1, 1, 1, 1, 1, 1, RES_720_M },
  { "720fol",           N_("720 DPI Full Overlap"),
    720,  720,  720,  720,  0,  2, 1, 1, 0, 1, 1, RES_720_M },
  { "720foluni",        N_("720 DPI Full Overlap Unidirectional"),
    720,  720,  720,  720,  0,  2, 1, 1, 1, 1, 1, RES_720_M },
  { "720fourp",         N_("720 DPI Four Pass"),
    720,  720,  720,  720,  0,  3, 1, 1, 0, 1, 1, RES_720_M },
  { "720fourpuni",      N_("720 DPI Four Pass Unidirectional"),
    720,  720,  720,  720,  0,  3, 1, 1, 1, 1, 1, RES_720_M },

  { "1440x720mw",       N_("1440 x 720 DPI Microweave"),
    1440, 720,  1440, 720,  0,  1, 1, 1, 0, 1, 1, RES_1440_720_M },
  { "1440x720mwuni",    N_("1440 x 720 DPI Microweave Unidirectional"),
    1440, 720,  1440, 720,  0,  1, 1, 1, 1, 1, 1, RES_1440_720_M },
  { "1440x720fol",      N_("1440 x 720 DPI FOL"),
    1440, 720,  1440, 720,  0,  2, 1, 1, 0, 1, 1, RES_1440_720_M },
  { "1440x720foluni",   N_("1440 x 720 DPI FOL Unidirectional"),
    1440, 720,  1440, 720,  0,  2, 1, 1, 1, 1, 1, RES_1440_720_M },
  { "1440x720fourp",    N_("1440 x 720 DPI Four Pass"),
    1440, 720,  1440, 720,  0,  3, 1, 1, 0, 1, 1, RES_1440_720_M },
  { "1440x720fourpuni", N_("1440 x 720 DPI Four Pass Unidirectional"),
    1440, 720,  1440, 720,  0,  3, 1, 1, 1, 1, 1, RES_1440_720_M },

  { "2880x720mw",       N_("2880 x 720 DPI Microweave"),
    2880, 720,  2880, 720,  0,  1, 1, 1, 0, 1, 1, RES_2880_720_M },
  { "2880x720mwuni",    N_("2880 x 720 DPI Microweave Unidirectional"),
    2880, 720,  2880, 720,  0,  1, 1, 1, 1, 1, 1, RES_2880_720_M },
  { "2880x720fol",      N_("2880 x 720 DPI FOL"),
    2880, 720,  2880, 720,  0,  2, 1, 1, 0, 1, 1, RES_2880_720_M },
  { "2880x720foluni",   N_("2880 x 720 DPI FOL Unidirectional"),
    2880, 720,  2880, 720,  0,  2, 1, 1, 1, 1, 1, RES_2880_720_M },
  { "2880x720fourp",    N_("2880 x 720 DPI Four Pass"),
    2880, 720,  2880, 720,  0,  3, 1, 1, 0, 1, 1, RES_2880_720_M },
  { "2880x720fourpuni", N_("2880 x 720 DPI Four Pass Unidirectional"),
    2880, 720,  2880, 720,  0,  3, 1, 1, 1, 1, 1, RES_2880_720_M },

  { "1440x1440mw",       N_("1440 x 1440 DPI Microweave"),
    1440, 1440,  1440, 1440,  0,  1, 1, 1, 0, 1, 1, RES_1440_1440_M },
  { "1440x1440mwuni",    N_("1440 x 1440 DPI Microweave Unidirectional"),
    1440, 1440,  1440, 1440,  0,  1, 1, 1, 1, 1, 1, RES_1440_1440_M },
  { "1440x1440fol",      N_("1440 x 1440 DPI FOL"),
    1440, 1440,  1440, 1440,  0,  2, 1, 1, 0, 1, 1, RES_1440_1440_M },
  { "1440x1440foluni",   N_("1440 x 1440 DPI FOL Unidirectional"),
    1440, 1440,  1440, 1440,  0,  2, 1, 1, 1, 1, 1, RES_1440_1440_M },
  { "1440x1440fourp",    N_("1440 x 1440 DPI Four Pass"),
    1440, 1440,  1440, 1440,  0,  3, 1, 1, 0, 1, 1, RES_1440_1440_M },
  { "1440x1440fourpuni", N_("1440 x 1440 DPI Four Pass Unidirectional"),
    1440, 1440,  1440, 1440,  0,  3, 1, 1, 1, 1, 1, RES_1440_1440_M },

  { "2880x1440mw",       N_("2880 x 1440 DPI Microweave"),
    2880, 1440,  2880, 1440,  0,  1, 1, 1, 0, 1, 1, RES_2880_1440_M },
  { "2880x1440mwuni",    N_("2880 x 1440 DPI Microweave Unidirectional"),
    2880, 1440,  2880, 1440,  0,  1, 1, 1, 1, 1, 1, RES_2880_1440_M },
  { "2880x1440fol",      N_("2880 x 1440 DPI FOL"),
    2880, 1440,  2880, 1440,  0,  2, 1, 1, 0, 1, 1, RES_2880_1440_M },
  { "2880x1440foluni",   N_("2880 x 1440 DPI FOL Unidirectional"),
    2880, 1440,  2880, 1440,  0,  2, 1, 1, 1, 1, 1, RES_2880_1440_M },
  { "2880x1440fourp",    N_("2880 x 1440 DPI Four Pass"),
    2880, 1440,  2880, 1440,  0,  3, 1, 1, 0, 1, 1, RES_2880_1440_M },
  { "2880x1440fourpuni", N_("2880 x 1440 DPI Four Pass Unidirectional"),
    2880, 1440,  2880, 1440,  0,  3, 1, 1, 1, 1, 1, RES_2880_1440_M },

  { "", "", 0, 0, 0, 0, 0, 0, 0, 0, 1, -1 }
};

static const input_slot_t standard_roll_feed_input_slots[] =
{
  {
    "Standard",
    N_("Standard"),
    0,
    0,
    { "IR\002\000\000\001EX\006\000\000\000\000\000\005\000", 16 },
    { "IR\002\000\000\000", 6}
  },
  {
    "Roll",
    N_("Roll Feed"),
    1,
    0,
    { "IR\002\000\000\001EX\006\000\000\000\000\000\005\001", 16 },
    { "IR\002\000\000\002", 6 }
  }
};

static const input_slot_list_t standard_roll_feed_input_slot_list =
{
  standard_roll_feed_input_slots,
  sizeof(standard_roll_feed_input_slots) / sizeof(const input_slot_t)
};

static const input_slot_t cutter_roll_feed_input_slots[] =
{
  {
    "Standard",
    N_("Standard"),
    0,
    0,
    { "IR\002\000\000\001EX\006\000\000\000\000\000\005\000", 16 },
    { "IR\002\000\000\000", 6}
  },
  {
    "RollCutPage",
    N_("Roll Feed (cut each page)"),
    1,
    1,
    { "IR\002\000\000\001EX\006\000\000\000\000\000\005\001", 16 },
    { "IR\002\000\000\002", 6 }
  },
  {
    "RollCutNone",
    N_("Roll Feed (do not cut)"),
    1,
    0,
    { "IR\002\000\000\001EX\006\000\000\000\000\000\005\001", 16 },
    { "IR\002\000\000\002", 6 }
  }
};

static const input_slot_list_t cutter_roll_feed_input_slot_list =
{
  cutter_roll_feed_input_slots,
  sizeof(cutter_roll_feed_input_slots) / sizeof(const input_slot_t)
};

static const input_slot_t pro_roll_feed_input_slots[] =
{
  {
    "Standard",
    N_("Standard"),
    0,
    0,
    { "PP\003\000\000\002\000", 7 },
    { "", 0 }
  },
  {
    "Roll",
    N_("Roll Feed"),
    1,
    0,
    { "PP\003\000\000\003\000", 7 },
    { "", 0 }
  }
};

static const input_slot_list_t pro_roll_feed_input_slot_list =
{
  pro_roll_feed_input_slots,
  sizeof(pro_roll_feed_input_slots) / sizeof(const input_slot_t)
};

static const input_slot_t sp5000_input_slots[] =
{
  {
    "CutSheet1",
    N_("Cut Sheet Bin 1"),
    0,
    0,
    { "PP\003\000\000\001\001", 7 },
    { "", 0 }
  },
  {
    "CutSheet2",
    N_("Cut Sheet Bin 2"),
    0,
    0,
    { "PP\003\000\000\002\001", 7 },
    { "", 0 }
  },
  {
    "CutSheetAuto",
    N_("Cut Sheet Autoselect"),
    0,
    0,
    { "PP\003\000\000\001\377", 7 },
    { "", 0 }
  },
  {
    "ManualSelect",
    N_("Manual Selection"),
    0,
    0,
    { "PP\003\000\000\002\001", 7 },
    { "", 0 }
  }
};

static const input_slot_list_t sp5000_input_slot_list =
{
  sp5000_input_slots,
  sizeof(sp5000_input_slots) / sizeof(const input_slot_t)
};

static const input_slot_list_t default_input_slot_list =
{
  NULL,
  0,
};

static const init_sequence_t new_init_sequence =
{
  "\0\0\0\033\001@EJL 1284.4\n@EJL     \n\033@", 29
};

static const init_sequence_t je_deinit_sequence =
{
  "JE\001\000\000", 5
};

#define INCH(x)		(72 * x)

const escp2_stp_printer_t stp_escp2_model_capabilities[] =
{
  /* FIRST GENERATION PRINTERS */
  /* 0: Stylus Color */
  {
    (MODEL_VARIABLE_NO | MODEL_COMMAND_1998 | MODEL_GRAYMODE_YES |
     MODEL_ROLLFEED_NO | MODEL_XZEROMARGIN_NO | MODEL_VACUUM_NO |
     MODEL_FAST_360_NO),
    15, 1, 4, 15, 1, 4, 15, 1, 4,
    360, 720, 720, 14400, -1, 720, 720, 90, 90,
    INCH(17 / 2), INCH(44), INCH(2), INCH(4),
    9, 9, 9, 40, 9, 9, 9, 40, 9, 9, 0, 0, 9, 9, 0, 0,
    0, 1, 0, 0, 0, 0, 0,
    g1_dotsizes, g1_densities, &simple_inks,
    &standard_paper_list, standard_reslist, &standard_inklist,
    standard_bits, standard_base_res, &default_input_slot_list,
    NULL, NULL
  },
  /* 1: Stylus Color 400/500 */
  {
    (MODEL_VARIABLE_NO | MODEL_COMMAND_1998 | MODEL_GRAYMODE_NO |
     MODEL_ROLLFEED_NO | MODEL_XZEROMARGIN_NO | MODEL_VACUUM_NO |
     MODEL_FAST_360_NO),
    48, 1, 3, 48, 1, 3, 48, 1, 3,
    360, 720, 720, 14400, -1, 720, 720, 90, 90,
    INCH(17 / 2), INCH(44), INCH(2), INCH(4),
    9, 9, 9, 40, 9, 9, 9, 40, 9, 9, 0, 0, 9, 9, 0, 0,
    0, 1, 0, 0, 0, 0, 0,
    g2_dotsizes, g1_densities, &simple_inks,
    &standard_paper_list, standard_reslist, &standard_inklist,
    standard_bits, standard_base_res, &default_input_slot_list,
    NULL, NULL
  },
  /* 2: Stylus Color 1500 */
  {
    (MODEL_VARIABLE_NO | MODEL_COMMAND_1998 | MODEL_GRAYMODE_NO |
     MODEL_ROLLFEED_YES | MODEL_XZEROMARGIN_NO | MODEL_VACUUM_NO |
     MODEL_FAST_360_NO),
    1, 1, 1, 1, 1, 1, 1, 1, 1,
    360, 720, 720, 14400, -1, 720, 720, 90, 90,
    INCH(17), INCH(44), INCH(2), INCH(4),
    9, 9, 9, 40, 9, 9, 9, 40, 9, 9, 0, 0, 9, 9, 0, 0,
    0, 1, 0, 0, 0, 0, 0,
    g1_dotsizes, sc1500_densities, &simple_inks,
    &standard_paper_list, standard_reslist, &cmy_inklist,
    standard_bits, standard_base_res, &standard_roll_feed_input_slot_list,
    NULL, NULL
  },
  /* 3: Stylus Color 600 */
  {
    (MODEL_VARIABLE_NO | MODEL_COMMAND_1998 | MODEL_GRAYMODE_NO |
     MODEL_ROLLFEED_NO | MODEL_XZEROMARGIN_NO | MODEL_VACUUM_NO |
     MODEL_FAST_360_NO),
    32, 1, 4, 32, 1, 4, 32, 1, 4,
    360, 720, 720, 14400, -1, 1440, 720, 90, 90,
    INCH(17 / 2), INCH(44), INCH(2), INCH(4),
    8, 9, 0, 30, 8, 9, 0, 30, 8, 9, 0, 0, 8, 9, 0, 0,
    0, 1, 0, 0, 0, 0, 8,
    sc600_dotsizes, g3_densities, &simple_inks,
    &standard_paper_list, standard_reslist, &standard_inklist,
    standard_bits, g3_base_res, &default_input_slot_list,
    NULL, NULL
  },
  /* 4: Stylus Color 800 */
  {
    (MODEL_VARIABLE_NO | MODEL_COMMAND_1998 | MODEL_GRAYMODE_NO |
     MODEL_ROLLFEED_NO | MODEL_XZEROMARGIN_NO | MODEL_VACUUM_NO |
     MODEL_FAST_360_NO),
    64, 1, 2, 64, 1, 2, 64, 1, 2,
    360, 720, 720, 14400, -1, 1440, 720, 90, 90,
    INCH(17 / 2), INCH(44), INCH(2), INCH(4),
    8, 9, 9, 40, 8, 9, 9, 40, 8, 9, 0, 0, 8, 9, 0, 0,
    0, 1, 4, 0, 0, 0, 0,
    g3_dotsizes, g3_densities, &simple_inks,
    &standard_paper_list, standard_reslist, &standard_inklist,
    standard_bits, g3_base_res, &default_input_slot_list,
    NULL, NULL
  },
  /* 5: Stylus Color 850 */
  {
    (MODEL_VARIABLE_NO | MODEL_COMMAND_1998 | MODEL_GRAYMODE_NO |
     MODEL_ROLLFEED_NO | MODEL_XZEROMARGIN_NO | MODEL_VACUUM_NO |
     MODEL_FAST_360_NO),
    64, 1, 2, 128, 1, 1, 128, 1, 1,
    360, 720, 720, 14400, -1, 1440, 720, 90, 90,
    INCH(17 / 2), INCH(44), INCH(2), INCH(4),
    9, 9, 9, 40, 9, 9, 9, 40, 9, 9, 0, 0, 9, 9, 0, 0,
    0, 1, 4, 0, 0, 0, 0,
    g3_dotsizes, g3_densities, &simple_inks,
    &standard_paper_list, standard_reslist, &standard_inklist,
    standard_bits, g3_base_res, &default_input_slot_list,
    NULL, NULL
  },
  /* 6: Stylus Color 1520 */
  {
    (MODEL_VARIABLE_NO | MODEL_COMMAND_1998 | MODEL_GRAYMODE_NO |
     MODEL_ROLLFEED_YES | MODEL_XZEROMARGIN_NO | MODEL_VACUUM_NO |
     MODEL_FAST_360_NO),
    64, 1, 2, 64, 1, 2, 64, 1, 2,
    360, 720, 720, 14400, -1, 1440, 720, 90, 90,
    INCH(17), INCH(44), INCH(2), INCH(4),
    8, 9, 9, 40, 8, 9, 9, 40, 8, 9, 0, 0, 8, 9, 0, 0,
    0, 1, 4, 0, 0, 0, 0,
    g3_dotsizes, g3_densities, &simple_inks,
    &standard_paper_list, standard_reslist, &standard_inklist,
    standard_bits, g3_base_res, &standard_roll_feed_input_slot_list,
    NULL, NULL
  },

  /* SECOND GENERATION PRINTERS */
  /* 7: Stylus Photo 700 */
  {
    (MODEL_VARIABLE_NO | MODEL_COMMAND_1998 | MODEL_GRAYMODE_NO |
     MODEL_ROLLFEED_NO | MODEL_XZEROMARGIN_NO | MODEL_VACUUM_NO |
     MODEL_FAST_360_NO),
    32, 1, 4, 32, 1, 4, 32, 1, 4,
    360, 720, 720, 14400, -1, 1440, 720, 90, 90,
    INCH(17 / 2), INCH(44), INCH(2), INCH(4),
    9, 9, 0, 30, 9, 9, 0, 30, 9, 9, 0, 0, 9, 9, 0, 0,
    0, 1, 0, 0, 0, 0, 8,
    photo_dotsizes, photo_densities, &simple_inks,
    &standard_paper_list, standard_reslist, &photo_inklist,
    standard_bits, g3_base_res, &default_input_slot_list,
    NULL, NULL
  },
  /* 8: Stylus Photo EX */
  {
    (MODEL_VARIABLE_NO | MODEL_COMMAND_1998 | MODEL_GRAYMODE_NO |
     MODEL_ROLLFEED_NO | MODEL_XZEROMARGIN_NO | MODEL_VACUUM_NO |
     MODEL_FAST_360_NO),
    32, 1, 4, 32, 1, 4, 32, 1, 4,
    360, 720, 720, 14400, -1, 1440, 720, 90, 90,
    INCH(118 / 10), INCH(44), INCH(2), INCH(4),
    9, 9, 0, 30, 9, 9, 0, 30, 9, 9, 0, 0, 9, 9, 0, 0,
    0, 1, 0, 0, 0, 0, 8,
    photo_dotsizes, photo_densities, &simple_inks,
    &standard_paper_list, standard_reslist, &photo_inklist,
    standard_bits, g3_base_res, &default_input_slot_list,
    NULL, NULL
  },
  /* 9: Stylus Photo */
  {
    (MODEL_VARIABLE_NO | MODEL_COMMAND_1998 | MODEL_GRAYMODE_NO |
     MODEL_ROLLFEED_NO | MODEL_XZEROMARGIN_NO | MODEL_VACUUM_NO |
     MODEL_FAST_360_NO),
    32, 1, 4, 32, 1, 4, 32, 1, 4,
    360, 720, 720, 14400, -1, 720, 720, 90, 90,
    INCH(17 / 2), INCH(44), INCH(2), INCH(4),
    9, 9, 0, 30, 9, 9, 0, 30, 9, 9, 0, 0, 9, 9, 0, 0,
    0, 1, 0, 0, 0, 0, 8,
    photo_dotsizes, photo_densities, &simple_inks,
    &standard_paper_list, standard_reslist, &photo_inklist,
    standard_bits, g3_base_res, &default_input_slot_list,
    NULL, NULL
  },

  /* THIRD GENERATION PRINTERS */
  /* 10: Stylus Color 440/460 */
  {
    (MODEL_VARIABLE_NO | MODEL_COMMAND_1999 | MODEL_GRAYMODE_YES |
     MODEL_ROLLFEED_NO | MODEL_XZEROMARGIN_NO | MODEL_VACUUM_NO |
     MODEL_FAST_360_NO),
    21, 1, 4, 21, 1, 4, 21, 1, 4,
    360, 720, 720, 14400, -1, 720, 720, 90, 90,
    INCH(17 / 2), INCH(44), INCH(2), INCH(4),
    9, 9, 0, 9, 9, 9, 0, 9, 9, 9, 0, 0, 9, 9, 0, 0,
    0, 1, 0, 0, 0, 0, 8,
    sc440_dotsizes, sc440_densities, &simple_inks,
    &standard_paper_list, no_microweave_reslist, &standard_inklist,
    standard_bits, standard_base_res, &default_input_slot_list,
    NULL, NULL
  },
  /* 11: Stylus Color 640 */
  {
    (MODEL_VARIABLE_NO | MODEL_COMMAND_1999 | MODEL_GRAYMODE_NO |
     MODEL_ROLLFEED_NO | MODEL_XZEROMARGIN_NO | MODEL_VACUUM_NO |
     MODEL_FAST_360_NO),
    32, 1, 4, 32, 1, 4, 32, 1, 4,
    360, 720, 720, 14400, -1, 1440, 720, 90, 90,
    INCH(17 / 2), INCH(44), INCH(2), INCH(4),
    9, 9, 0, 9, 9, 9, 0, 9, 9, 9, 0, 0, 9, 9, 0, 0,
    0, 1, 0, 0, 0, 0, 8,
    sc640_dotsizes, sc440_densities, &simple_inks,
    &standard_paper_list, standard_reslist, &standard_inklist,
    standard_bits, standard_base_res, &default_input_slot_list,
    NULL, NULL
  },
  /* 12: Stylus Color 740/Stylus Scan 2000/Stylus Scan 2500 */
  {
    (MODEL_VARIABLE_YES | MODEL_COMMAND_1999 | MODEL_GRAYMODE_YES |
     MODEL_ROLLFEED_NO | MODEL_XZEROMARGIN_NO | MODEL_VACUUM_NO |
     MODEL_FAST_360_NO),
    48, 1, 3, 144, 1, 1, 144, 1, 1,
    360, 720, 720, 14400, -1, 1440, 720, 90, 90,
    INCH(17 / 2), INCH(44), INCH(2), INCH(4),
    9, 9, 0, 9, 9, 9, 0, 9, 9, 9, 0, 0, 9, 9, 0, 0,
    0, 1, 0, 0, 0, 0, 0,
    c6pl_dotsizes, c6pl_densities, &variable_6pl_inks,
    &standard_paper_list, standard_reslist, &standard_inklist,
    variable_bits, variable_base_res, &default_input_slot_list,
    NULL, NULL
  },
  /* 13: Stylus Color 900 */
  {
    (MODEL_VARIABLE_YES | MODEL_COMMAND_1999 | MODEL_GRAYMODE_YES |
     MODEL_ROLLFEED_NO | MODEL_XZEROMARGIN_NO | MODEL_VACUUM_NO |
     MODEL_FAST_360_NO),
    96, 1, 2, 192, 1, 1, 192, 1, 1,
    360, 720, 720, 14400, -1, 1440, 720, 90, 90,
    INCH(17 / 2), INCH(44), INCH(2), INCH(4),
    9, 9, 0, 9, 9, 9, 0, 9, 9, 9, 0, 0, 9, 9, 0, 0,
    0, 1, 0, 0, 0, 0, 0,
    c3pl_dotsizes, c3pl_densities, &variable_3pl_inks,
    &standard_paper_list, standard_reslist, &standard_inklist,
    variable_bits, stc900_base_res, &default_input_slot_list,
    &new_init_sequence, &je_deinit_sequence
  },
  /* 14: Stylus Photo 750 */
  {
    (MODEL_VARIABLE_YES | MODEL_COMMAND_1999 | MODEL_GRAYMODE_NO |
     MODEL_ROLLFEED_NO | MODEL_XZEROMARGIN_NO | MODEL_VACUUM_NO |
     MODEL_FAST_360_NO),
    48, 1, 3, 48, 1, 3, 48, 1, 3,
    360, 720, 720, 14400, -1, 1440, 720, 90, 90,
    INCH(17 / 2), INCH(44), INCH(2), INCH(4),
    9, 9, 0, 9, 9, 9, 0, 9, 9, 9, 0, 0, 9, 9, 0, 0,
    0, 1, 0, 0, 0, 0, 0,
    c6pl_dotsizes, c6pl_densities, &variable_6pl_inks,
    &standard_paper_list, standard_reslist, &photo_inklist,
    variable_bits, variable_base_res, &default_input_slot_list,
    &new_init_sequence, &je_deinit_sequence
  },
  /* 15: Stylus Photo 1200 */
  {
    (MODEL_VARIABLE_YES | MODEL_COMMAND_1999 | MODEL_GRAYMODE_NO |
     MODEL_ROLLFEED_YES | MODEL_XZEROMARGIN_NO | MODEL_VACUUM_NO |
     MODEL_FAST_360_NO),
    48, 1, 3, 48, 1, 3, 48, 1, 3,
    360, 720, 720, 14400, -1, 1440, 720, 90, 90,
    INCH(13), INCH(44), INCH(2), INCH(4),
    9, 9, 0, 9, 9, 9, 0, 9, 9, 9, 0, 0, 9, 9, 0, 0,
    0, 1, 0, 0, 0, 0, 0,
    c6pl_dotsizes, c6pl_densities, &variable_6pl_inks,
    &standard_paper_list, standard_reslist, &photo_inklist,
    variable_bits, variable_base_res, &standard_roll_feed_input_slot_list,
    &new_init_sequence, &je_deinit_sequence
  },
  /* 16: Stylus Color 860 */
  {
    (MODEL_VARIABLE_YES | MODEL_COMMAND_1999 | MODEL_GRAYMODE_YES |
     MODEL_ROLLFEED_NO | MODEL_XZEROMARGIN_NO | MODEL_VACUUM_NO |
     MODEL_FAST_360_NO),
    48, 1, 3, 144, 1, 1, 144, 1, 1,
    360, 720, 720, 14400, -1, 1440, 720, 90, 90,
    INCH(17 / 2), INCH(44), INCH(2), INCH(4),
    9, 9, 0, 9, 9, 9, 0, 9, 9, 9, 0, 0, 9, 9, 0, 0,
    0, 1, 0, 0, 0, 0, 0,
    c4pl_dotsizes, c4pl_densities, &variable_4pl_inks,
    &standard_paper_list, standard_reslist, &standard_inklist,
    variable_bits, variable_base_res, &default_input_slot_list,
    &new_init_sequence, &je_deinit_sequence
  },
  /* 17: Stylus Color 1160 */
  {
    (MODEL_VARIABLE_YES | MODEL_COMMAND_1999 | MODEL_GRAYMODE_YES |
     MODEL_ROLLFEED_NO | MODEL_XZEROMARGIN_NO | MODEL_VACUUM_NO |
     MODEL_FAST_360_NO),
    48, 1, 3, 144, 1, 1, 144, 1, 1,
    360, 720, 720, 14400, -1, 1440, 720, 90, 90,
    INCH(13), INCH(44), INCH(2), INCH(4),
    9, 9, 0, 9, 9, 9, 0, 9, 9, 9, 0, 0, 9, 9, 0, 0,
    0, 1, 0, 0, 0, 0, 0,
    c4pl_dotsizes, c4pl_densities, &variable_4pl_inks,
    &standard_paper_list, standard_reslist, &standard_inklist,
    variable_bits, variable_base_res, &default_input_slot_list,
    &new_init_sequence, &je_deinit_sequence
  },
  /* 18: Stylus Color 660 */
  {
    (MODEL_VARIABLE_NO | MODEL_COMMAND_1999 | MODEL_GRAYMODE_NO |
     MODEL_ROLLFEED_NO | MODEL_XZEROMARGIN_NO | MODEL_VACUUM_NO |
     MODEL_FAST_360_NO),
    32, 1, 4, 32, 1, 4, 32, 1, 4,
    360, 720, 720, 14400, -1, 1440, 720, 90, 90,
    INCH(17 / 2), INCH(44), INCH(2), INCH(4),
    9, 9, 9, 9, 9, 9, 9, 26, 9, 9, 9, 0, 9, 9, 9, 0,
    0, 1, 8, 0, 0, 0, 8,
    sc660_dotsizes,sc660_densities, &simple_inks,
    &standard_paper_list, standard_reslist, &standard_inklist,
    standard_bits, standard_base_res, &default_input_slot_list,
    &new_init_sequence, &je_deinit_sequence
  },
  /* 19: Stylus Color 760 */
  {
    (MODEL_VARIABLE_YES | MODEL_COMMAND_1999 | MODEL_GRAYMODE_YES |
     MODEL_ROLLFEED_NO | MODEL_XZEROMARGIN_NO | MODEL_VACUUM_NO |
     MODEL_FAST_360_NO),
    48, 1, 3, 144, 1, 1, 144, 1, 1,
    360, 720, 720, 14400, -1, 1440, 720, 90, 90,
    INCH(17 / 2), INCH(44), INCH(2), INCH(4),
    9, 9, 0, 9, 9, 9, 0, 9, 9, 9, 0, 0, 9, 9, 0, 0,
    0, 1, 0, 0, 0, 0, 0,
    c4pl_dotsizes, c4pl_densities, &variable_4pl_inks,
    &standard_paper_list, standard_reslist, &standard_inklist,
    variable_bits, variable_base_res, &default_input_slot_list,
    &new_init_sequence, &je_deinit_sequence
  },
  /* 20: Stylus Photo 720 (Australia) */
  {
    (MODEL_VARIABLE_YES | MODEL_COMMAND_1999 | MODEL_GRAYMODE_NO |
     MODEL_ROLLFEED_NO | MODEL_XZEROMARGIN_NO | MODEL_VACUUM_NO |
     MODEL_FAST_360_NO),
    32, 1, 4, 32, 1, 4, 32, 1, 4,
    360, 720, 720, 14400, -1, 1440, 720, 90, 90,
    INCH(17 / 2), INCH(44), INCH(2), INCH(4),
    9, 9, 0, 9, 9, 9, 0, 9, 9, 9, 0, 0, 9, 9, 0, 0,
    0, 1, 0, 0, 0, 0, 0,
    sc720_dotsizes, c6pl_densities, &variable_6pl_inks,
    &standard_paper_list, standard_reslist, &photo_inklist,
    variable_bits, variable_base_res, &default_input_slot_list,
    &new_init_sequence, &je_deinit_sequence
  },
  /* 21: Stylus Color 480 */
  {
    (MODEL_VARIABLE_YES | MODEL_COMMAND_2000 | MODEL_GRAYMODE_YES |
     MODEL_ROLLFEED_NO | MODEL_XZEROMARGIN_NO | MODEL_VACUUM_NO |
     MODEL_FAST_360_NO),
    15, 15, 3, 48, 48, 3, 48, 48, 3,
    360, 720, 720, 14400, 360, 720, 720, 90, 90,
    INCH(17 / 2), INCH(1200), INCH(2), INCH(4),
    9, 9, 0, 9, 9, 9, 0, 9, 9, 9, 0, 0, 9, 9, 0, 0,
    0, 1, 0, 0, -99, 0, 0,
    sc480_dotsizes, sc480_densities, &variable_x80_6pl_inks,
    &standard_paper_list, standard_reslist, &x80_inklist,
    variable_bits, variable_base_res, &default_input_slot_list,
    &new_init_sequence, &je_deinit_sequence
  },
  /* 22: Stylus Photo 870/875 */
  {
    (MODEL_VARIABLE_YES | MODEL_COMMAND_2000 | MODEL_GRAYMODE_NO |
     MODEL_ROLLFEED_YES | MODEL_XZEROMARGIN_YES | MODEL_VACUUM_NO |
     MODEL_FAST_360_NO),
    48, 1, 3, 48, 1, 3, 48, 1, 3,
    360, 720, 720, 14400, -1, 1440, 720, 90, 90,
    INCH(17 / 2), INCH(1200), INCH(2), INCH(4),
    0, 0, 0, 9, 0, 0, 0, 9, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 1, 0, 97, 0, 0, 0,
    c4pl_dotsizes, c4pl_densities, &variable_4pl_inks,
    &standard_paper_list, standard_reslist, &photo_inklist,
    variable_bits, variable_base_res, &standard_roll_feed_input_slot_list,
    &new_init_sequence, &je_deinit_sequence
  },
  /* 23: Stylus Photo 1270 */
  {
    (MODEL_VARIABLE_YES | MODEL_COMMAND_2000 | MODEL_GRAYMODE_NO |
     MODEL_ROLLFEED_YES | MODEL_XZEROMARGIN_YES | MODEL_VACUUM_NO |
     MODEL_FAST_360_NO),
    48, 1, 3, 48, 1, 3, 48, 1, 3,
    360, 720, 720, 14400, -1, 1440, 720, 90, 90,
    INCH(13), INCH(1200), INCH(2), INCH(4),
    0, 0, 0, 9, 0, 0, 0, 9, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 1, 0, 97, 0, 0, 0,
    c4pl_dotsizes, c4pl_densities, &variable_4pl_inks,
    &standard_paper_list, standard_reslist, &photo_inklist,
    variable_bits, variable_base_res, &standard_roll_feed_input_slot_list,
    &new_init_sequence, &je_deinit_sequence
  },
  /* 24: Stylus Color 3000 */
  {
    (MODEL_VARIABLE_NO | MODEL_COMMAND_1998 | MODEL_GRAYMODE_YES |
     MODEL_ROLLFEED_YES | MODEL_XZEROMARGIN_NO | MODEL_VACUUM_NO |
     MODEL_FAST_360_NO),
    64, 1, 2, 128, 1, 1, 128, 1, 1,
    360, 720, 720, 14400, -1, 1440, 720, 90, 90,
    INCH(17), INCH(44), INCH(2), INCH(4),
    8, 9, 9, 40, 8, 9, 9, 40, 8, 9, 0, 0, 8, 9, 0, 0,
    0, 1, 4, 0, 0, 0, 0,
    g3_dotsizes, g3_densities, &simple_inks,
    &standard_paper_list, standard_reslist, &standard_inklist,
    standard_bits, g3_base_res, &standard_roll_feed_input_slot_list,
    NULL, NULL
  },
  /* 25: Stylus Color 670 */
  {
    (MODEL_VARIABLE_YES | MODEL_COMMAND_2000 | MODEL_GRAYMODE_YES |
     MODEL_ROLLFEED_NO | MODEL_XZEROMARGIN_NO | MODEL_VACUUM_NO |
     MODEL_FAST_360_NO),
    32, 1, 4, 64, 1, 2, 64, 1, 2,
    360, 720, 720, 14400, -1, 1440, 720, 90, 90,
    INCH(17 / 2), INCH(1200), INCH(2), INCH(4),
    9, 9, 0, 9, 9, 9, 0, 9, 9, 9, 0, 0, 9, 9, 0, 0,
    0, 1, 0, 0, 0, 0, 0,
    sc670_dotsizes, c6pl_densities, &variable_6pl_inks,
    &standard_paper_list, standard_reslist, &standard_inklist,
    variable_bits, variable_base_res, &default_input_slot_list,
    &new_init_sequence, &je_deinit_sequence
  },
  /* 26: Stylus Photo 2000P */
  {
    (MODEL_VARIABLE_YES | MODEL_COMMAND_2000 | MODEL_GRAYMODE_NO |
     MODEL_ROLLFEED_NO | MODEL_XZEROMARGIN_NO | MODEL_VACUUM_NO |
     MODEL_FAST_360_NO),
    48, 1, 3, 144, 1, 1, 144, 1, 1,
    360, 720, 720, 14400, -1, 1440, 720, 90, 90,
    INCH(13), INCH(1200), INCH(2), INCH(4),
    9, 9, 0, 9, 9, 9, 0, 9, 9, 9, 0, 0, 9, 9, 0, 0,
    0, 1, 0, 0, 0, 0, 0,
    sp2000_dotsizes, sp2000_densities, &variable_pigment_inks,
    &standard_paper_list, standard_reslist, &photo_inklist,
    variable_bits, variable_base_res, &default_input_slot_list,
    &new_init_sequence, &je_deinit_sequence
  },
  /* 27: Stylus Pro 5000 */
  {
    (MODEL_VARIABLE_NO | MODEL_COMMAND_1998 | MODEL_GRAYMODE_NO |
     MODEL_ROLLFEED_NO | MODEL_XZEROMARGIN_NO | MODEL_VACUUM_NO |
     MODEL_FAST_360_NO),
    64, 1, 2, 64, 1, 2, 64, 1, 2,
    360, 720, 720, 14400, -1, 1440, 720, 90, 90,
    INCH(13), INCH(44), INCH(2), INCH(4),
    9, 9, 0, 30, 9, 9, 0, 30, 9, 9, 0, 0, 9, 9, 0, 0,
    0, 1, 0, 0, 0, 0, 4,
    sp5000_dotsizes, photo_densities, &simple_inks,
    &standard_paper_list, sp5000_reslist, &photo_inklist,
    standard_bits, g3_base_res, &sp5000_input_slot_list,
    NULL, NULL
  },
  /* 28: Stylus Pro 7000 */
  {
    (MODEL_VARIABLE_NO | MODEL_COMMAND_PRO | MODEL_GRAYMODE_NO |
     MODEL_ROLLFEED_YES | MODEL_XZEROMARGIN_NO | MODEL_VACUUM_NO |
     MODEL_FAST_360_NO),
    1, 1, 1, 1, 1, 1, 1, 1, 1,
    360, 1440, 1440, 14400, -1, 1440, 720, 90, 90,
    INCH(24), INCH(1200), INCH(7), INCH(7),
    9, 9, 9, 40, 9, 9, 9, 40, 9, 9, 9, 9, 9, 9, 9, 9,
    0, 1, 0, 0, 0, 0, 0,
    spro_dye_dotsizes, spro_dye_densities, &simple_inks,
    &standard_paper_list, pro_reslist, &photo_inklist,
    standard_bits, pro_base_res, &pro_roll_feed_input_slot_list,
    NULL, NULL
  },
  /* 29: Stylus Pro 7500 */
  {
    (MODEL_VARIABLE_NO | MODEL_COMMAND_PRO | MODEL_GRAYMODE_NO |
     MODEL_ROLLFEED_YES | MODEL_XZEROMARGIN_NO | MODEL_VACUUM_YES |
     MODEL_FAST_360_NO),
    1, 1, 1, 1, 1, 1, 1, 1, 1,
    360, 1440, 1440, 14400, -1, 1440, 720, 90, 90,
    INCH(24), INCH(1200), INCH(7), INCH(7),
    9, 9, 9, 40, 9, 9, 9, 40, 9, 9, 9, 9, 9, 9, 9, 9,
    0, 1, 0, 0, 0, 0, 0,
    spro_pigment_dotsizes, spro_pigment_densities, &simple_inks,
    &standard_paper_list, pro_reslist, &photo_inklist,
    standard_bits, pro_base_res, &pro_roll_feed_input_slot_list,
    NULL, NULL
  },
  /* 30: Stylus Pro 9000 */
  {
    (MODEL_VARIABLE_NO | MODEL_COMMAND_PRO | MODEL_GRAYMODE_NO |
     MODEL_ROLLFEED_YES | MODEL_XZEROMARGIN_NO | MODEL_VACUUM_NO |
     MODEL_FAST_360_NO),
    1, 1, 1, 1, 1, 1, 1, 1, 1,
    360, 1440, 1440, 14400, -1, 1440, 720, 90, 90,
    INCH(44), INCH(1200), INCH(7), INCH(7),
    9, 9, 9, 40, 9, 9, 9, 40, 9, 9, 9, 9, 9, 9, 9, 9,
    0, 1, 0, 0, 0, 0, 0,
    spro_dye_dotsizes, spro_dye_densities, &simple_inks,
    &standard_paper_list, pro_reslist, &photo_inklist,
    standard_bits, pro_base_res, &pro_roll_feed_input_slot_list,
    NULL, NULL
  },
  /* 31: Stylus Pro 9500 */
  {
    (MODEL_VARIABLE_NO | MODEL_COMMAND_PRO | MODEL_GRAYMODE_NO |
     MODEL_ROLLFEED_YES | MODEL_XZEROMARGIN_NO | MODEL_VACUUM_YES |
     MODEL_FAST_360_NO),
    1, 1, 1, 1, 1, 1, 1, 1, 1,
    360, 1440, 1440, 14400, -1, 1440, 720, 90, 90,
    INCH(44), INCH(1200), INCH(7), INCH(7),
    9, 9, 9, 40, 9, 9, 9, 40, 9, 9, 9, 9, 9, 9, 9, 9,
    0, 1, 0, 0, 0, 0, 0,
    spro_pigment_dotsizes, spro_pigment_densities, &simple_inks,
    &standard_paper_list, pro_reslist, &photo_inklist,
    standard_bits, pro_base_res, &pro_roll_feed_input_slot_list,
    NULL, NULL
  },
  /* 32: Stylus Color 777/680 */
  {
    (MODEL_VARIABLE_YES | MODEL_COMMAND_2000 | MODEL_GRAYMODE_YES |
     MODEL_ROLLFEED_NO | MODEL_XZEROMARGIN_NO | MODEL_VACUUM_NO |
     MODEL_FAST_360_NO),
    48, 1, 3, 144, 1, 1, 144, 1, 1,
    360, 720, 720, 14400, -1, 2880, 720, 90, 90,
    INCH(17 / 2), INCH(1200), INCH(2), INCH(4),
    9, 9, 0, 9, 9, 9, 9, 9, 9, 9, 0, 0, 9, 9, 0, 0,
    0, 1, 0, 0, 0, 0, 0,
    c4pl_dotsizes, sc680_densities, &variable_680_4pl_inks,
    &standard_paper_list, standard_reslist, &standard_inklist,
    variable_bits, variable_base_res, &default_input_slot_list,
    &new_init_sequence, &je_deinit_sequence
  },
  /* 33: Stylus Color 880/83/C60 */
  {
    (MODEL_VARIABLE_YES | MODEL_COMMAND_2000 | MODEL_GRAYMODE_YES |
     MODEL_ROLLFEED_NO | MODEL_XZEROMARGIN_NO | MODEL_VACUUM_NO |
     MODEL_FAST_360_NO),
    48, 1, 3, 144, 1, 1, 144, 1, 1,
    360, 720, 720, 14400, -1, 2880, 720, 90, 90,
    INCH(17 / 2), INCH(1200), INCH(2), INCH(4),
    9, 9, 0, 9, 9, 9, 9, 9, 9, 9, 0, 0, 9, 9, 0, 0,
    0, 1, 0, 0, 0, 0, 0,
    c4pl_dotsizes, c4pl_densities, &variable_4pl_inks,
    &standard_paper_list, standard_reslist, &standard_inklist,
    variable_bits, variable_base_res, &default_input_slot_list,
    &new_init_sequence, &je_deinit_sequence
  },
  /* 34: Stylus Color 980 */
  {
    (MODEL_VARIABLE_YES | MODEL_COMMAND_2000 | MODEL_GRAYMODE_YES |
     MODEL_ROLLFEED_NO | MODEL_XZEROMARGIN_NO | MODEL_VACUUM_NO |
     MODEL_FAST_360_NO),
    96, 1, 2, 192, 1, 1, 192, 1, 1,
    360, 720, 720, 14400, -1, 2880, 720, 90, 90,
    INCH(17 / 2), INCH(1200), INCH(2), INCH(4),
    9, 9, 0, 9, 9, 9, 9, 9, 9, 9, 0, 0, 9, 9, 0, 0,
    192, 1, 0, 0, 0, 0, 0,
    c3pl_dotsizes, sc980_densities, &variable_3pl_inks,
    &standard_paper_list, standard_reslist, &standard_inklist,
    variable_bits, variable_base_res, &default_input_slot_list,
    &new_init_sequence, &je_deinit_sequence
  },
  /* 35: Stylus Photo 780/790/810/820 */
  {
    (MODEL_VARIABLE_YES | MODEL_COMMAND_2000 | MODEL_GRAYMODE_YES |
     MODEL_ROLLFEED_NO | MODEL_XZEROMARGIN_YES | MODEL_VACUUM_NO |
     MODEL_FAST_360_NO),
    48, 1, 3, 48, 1, 3, 48, 1, 3,
    360, 720, 720, 14400, -1, 2880, 720, 90, 90,
    INCH(17 / 2), INCH(1200), INCH(2), INCH(4),
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 1, 0, 55, 0, 0, 0,
    c4pl_dotsizes, c4pl_densities, &variable_4pl_inks,
    &sp780_paper_list, standard_reslist, &photo_inklist,
    variable_bits, variable_base_res, &default_input_slot_list,
    &new_init_sequence, &je_deinit_sequence
  },
  /* 36: Stylus Photo 785/890/895/915 */
  {
    (MODEL_VARIABLE_YES | MODEL_COMMAND_2000 | MODEL_GRAYMODE_YES |
     MODEL_ROLLFEED_YES | MODEL_XZEROMARGIN_YES | MODEL_VACUUM_NO |
     MODEL_FAST_360_NO),
    48, 1, 3, 48, 1, 3, 48, 1, 3,
    360, 720, 720, 14400, -1, 2880, 720, 90, 90,
    INCH(17 / 2), INCH(1200), INCH(2), INCH(4),
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 1, 0, 55, 0, 0, 0,
    c4pl_dotsizes, c4pl_densities, &variable_4pl_inks,
    &standard_paper_list, standard_reslist, &photo_inklist,
    variable_bits, variable_base_res, &standard_roll_feed_input_slot_list,
    &new_init_sequence, &je_deinit_sequence
  },
  /* 37: Stylus Photo 1280/1290 */
  {
    (MODEL_VARIABLE_YES | MODEL_COMMAND_2000 | MODEL_GRAYMODE_YES |
     MODEL_ROLLFEED_YES | MODEL_XZEROMARGIN_YES | MODEL_VACUUM_NO |
     MODEL_FAST_360_NO),
    48, 1, 3, 48, 1, 3, 48, 1, 3,
    360, 720, 720, 14400, -1, 2880, 720, 90, 90,
    INCH(13), INCH(1200), INCH(2), INCH(4),
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 1, 0, 55, 0, 0, 0,
    c4pl_dotsizes, c4pl_densities, &variable_4pl_inks,
    &standard_paper_list, standard_reslist, &photo_inklist,
    variable_bits, variable_base_res, &standard_roll_feed_input_slot_list,
    &new_init_sequence, &je_deinit_sequence
  },
  /* 38: Stylus Color 580 */
  {
    (MODEL_VARIABLE_YES | MODEL_COMMAND_2000 | MODEL_GRAYMODE_YES |
     MODEL_ROLLFEED_NO | MODEL_XZEROMARGIN_NO | MODEL_VACUUM_NO |
     MODEL_FAST_360_NO),
    15, 15, 3, 48, 48, 3, 48, 48, 3,
    360, 720, 720, 14400, 360, 1440, 720, 90, 90,
    INCH(17 / 2), INCH(1200), INCH(2), INCH(4),
    9, 9, 0, 9, 9, 9, 9, 9, 9, 9, 0, 0, 9, 9, 0, 0,
    0, 1, 0, 0, -99, 0, 0,
    sc480_dotsizes, sc480_densities, &variable_x80_6pl_inks,
    &standard_paper_list, standard_reslist, &x80_inklist,
    variable_bits, variable_base_res, &default_input_slot_list,
    &new_init_sequence, &je_deinit_sequence
  },
  /* 39: Stylus Color Pro XL */
  {
    (MODEL_VARIABLE_NO | MODEL_COMMAND_1998 | MODEL_GRAYMODE_NO |
     MODEL_ROLLFEED_NO | MODEL_XZEROMARGIN_NO | MODEL_VACUUM_NO |
     MODEL_FAST_360_NO),
    48, 1, 3, 48, 1, 3, 48, 1, 3,
    360, 720, 720, 14400, -1, 720, 720, 90, 90,
    INCH(13), INCH(1200), INCH(2), INCH(4),
    9, 9, 9, 40, 9, 9, 9, 40, 9, 9, 0, 0, 9, 9, 0, 0,
    0, 1, 0, 0, 0, 0, 0,
    g1_dotsizes, g1_densities, &simple_inks,
    &standard_paper_list, standard_reslist, &standard_inklist,
    standard_bits, standard_base_res, &default_input_slot_list,
    NULL, NULL
  },
  /* 40: Stylus Pro 5500 */
  {
    (MODEL_VARIABLE_NO | MODEL_COMMAND_PRO | MODEL_GRAYMODE_NO |
     MODEL_ROLLFEED_YES | MODEL_XZEROMARGIN_NO | MODEL_VACUUM_YES |
     MODEL_FAST_360_NO),
    1, 1, 1, 1, 1, 1, 1, 1, 1,
    360, 1440, 1440, 14400, -1, 1440, 720, 90, 90,
    INCH(13), INCH(1200), INCH(2), INCH(4),
    9, 9, 9, 40, 9, 9, 9, 40, 9, 9, 0, 0, 9, 9, 0, 0,
    0, 1, 0, 0, 0, 0, 0,
    spro_pigment_dotsizes, spro_pigment_densities, &simple_inks,
    &standard_paper_list, pro_reslist, &photo_inklist,
    standard_bits, pro_base_res, &sp5000_input_slot_list,
    NULL, NULL
  },
  /* 41: Stylus Pro 10000 */
  {
    (MODEL_VARIABLE_NO | MODEL_COMMAND_PRO | MODEL_GRAYMODE_NO |
     MODEL_ROLLFEED_YES | MODEL_XZEROMARGIN_NO | MODEL_VACUUM_YES |
     MODEL_FAST_360_NO),
    1, 1, 1, 1, 1, 1, 1, 1, 1,
    360, 1440, 1440, 14400, -1, 1440, 720, 90, 90,
    INCH(44), INCH(1200), INCH(7), INCH(7),
    9, 9, 9, 40, 9, 9, 9, 40, 9, 9, 9, 9, 9, 9, 9, 9,
    0, 1, 0, 0, 0, 0, 0,
    spro10000_dotsizes, spro10000_densities, &spro10000_inks,
    &standard_paper_list, pro_reslist, &photo_inklist,
    variable_bits, pro_base_res, &pro_roll_feed_input_slot_list,
    NULL, NULL
  },
  /* 42: Stylus C20SX/C20UX */
  {
    (MODEL_VARIABLE_YES | MODEL_COMMAND_2000 | MODEL_GRAYMODE_YES |
     MODEL_ROLLFEED_NO | MODEL_XZEROMARGIN_NO | MODEL_VACUUM_NO |
     MODEL_FAST_360_NO),
    15, 15, 3, 48, 48, 3, 48, 48, 3,
    360, 720, 720, 14400, -1, 720, 720, 90, 90,
    INCH(17 / 2), INCH(1200), INCH(2), INCH(4),
    9, 9, 0, 9, 9, 9, 9, 9, 9, 9, 0, 0, 9, 9, 0, 0,
    0, 1, 0, 0, -99, 0, 0,
    sc480_dotsizes, sc480_densities, &variable_x80_6pl_inks,
    &standard_paper_list, standard_reslist, &x80_inklist,
    variable_bits, variable_base_res, &default_input_slot_list,
    &new_init_sequence, &je_deinit_sequence
  },
  /* 43: Stylus C40SX/C40UX/C41SX/C41UX/C42SX/C42UX */
  {
    (MODEL_VARIABLE_YES | MODEL_COMMAND_2000 | MODEL_GRAYMODE_YES |
     MODEL_ROLLFEED_NO | MODEL_XZEROMARGIN_NO | MODEL_VACUUM_NO |
     MODEL_FAST_360_NO),
    15, 15, 3, 48, 48, 3, 48, 48, 3,
    360, 720, 720, 14400, -1, 1440, 720, 90, 90,
    INCH(17 / 2), INCH(1200), INCH(2), INCH(4),
    9, 9, 0, 9, 9, 9, 9, 9, 9, 9, 0, 0, 9, 9, 0, 0,
    0, 1, 0, 0, -99, 0, 0,
    sc480_dotsizes, sc480_densities, &variable_x80_6pl_inks,
    &standard_paper_list, standard_reslist, &x80_inklist,
    variable_bits, variable_base_res, &default_input_slot_list,
    &new_init_sequence, &je_deinit_sequence
  },
  /* 44: Stylus C70/C80 */
  {
    (MODEL_VARIABLE_YES | MODEL_COMMAND_2000 | MODEL_GRAYMODE_YES |
     MODEL_ROLLFEED_NO | MODEL_XZEROMARGIN_NO | MODEL_VACUUM_NO |
     MODEL_FAST_360_NO),
    60, 60, 2, 180, 180, 2, 180, 180, 2,
    360, 720, 720, 14400, -1, 2880, 1440, 360, 180,
    INCH(17 / 2), INCH(1200), INCH(2), INCH(4),
    9, 9, 0, 9, 9, 9, 9, 9, 9, 9, 0, 0, 9, 9, 0, 0,
    0, 1, 0, 0, -240, 0, 0,
    c3pl_pigment_dotsizes, c3pl_pigment_densities, &variable_3pl_pigment_inks,
    &c80_paper_list, standard_reslist, &c80_inklist,
    variable_bits, variable_base_res, &default_input_slot_list,
    &new_init_sequence, &je_deinit_sequence
  },
  /* 45: Stylus Color Pro */
  {
    (MODEL_VARIABLE_NO | MODEL_COMMAND_1998 | MODEL_GRAYMODE_NO |
     MODEL_ROLLFEED_NO | MODEL_XZEROMARGIN_NO | MODEL_VACUUM_NO |
     MODEL_FAST_360_NO),
    48, 1, 3, 48, 1, 3, 48, 1, 3,
    360, 720, 720, 14400, -1, 720, 720, 90, 90,
    INCH(17 / 2), INCH(44), INCH(2), INCH(4),
    9, 9, 9, 40, 9, 9, 9, 40, 9, 9, 0, 0, 9, 9, 0, 0,
    0, 1, 0, 0, 0, 0, 0,
    g1_dotsizes, g1_densities, &simple_inks,
    &standard_paper_list, standard_reslist, &standard_inklist,
    standard_bits, standard_base_res, &default_input_slot_list,
    NULL, NULL
  },
  /* 46: Stylus Photo 950/960 */
  {
    (MODEL_VARIABLE_YES | MODEL_COMMAND_2000 | MODEL_GRAYMODE_YES |
     MODEL_ROLLFEED_YES | MODEL_XZEROMARGIN_YES | MODEL_VACUUM_NO |
     MODEL_FAST_360_YES),
    96, 1, 2, 96, 1, 2, 24, 1, 1,
    360, 720, 720, 14400, -1, 2880, 1440, 360, 180,
    INCH(17 / 2), INCH(1200), INCH(2), INCH(4),
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 1, 0, 190, 0, 0, 0,
    c2pl_dotsizes, c2pl_densities, &variable_2pl_inks,
    &sp950_paper_list, escp950_reslist, &photo_inklist,
    stp950_bits, stp950_base_res, &cutter_roll_feed_input_slot_list,
    &new_init_sequence, &je_deinit_sequence
  },
  /* 47: Stylus Photo 2100/2200 */
  {
    (MODEL_VARIABLE_YES | MODEL_COMMAND_2000 | MODEL_GRAYMODE_YES |
     MODEL_ROLLFEED_YES | MODEL_XZEROMARGIN_YES | MODEL_VACUUM_NO |
     MODEL_FAST_360_YES),
    96, 1, 2, 96, 1, 2, 192, 1, 1,
    360, 720, 720, 14400, -1, 2880, 1440, 360, 180,
    INCH(13), INCH(1200), INCH(2), INCH(4),
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 1, 0, 190, 0, 0, 0,
    c4pl_pigment_dotsizes, c4pl_pigment_densities, &variable_4pl_pigment_inks,
    &standard_paper_list, escp2200_reslist, &photo7_inklist,
    ultrachrome_bits, ultrachrome_base_res, &cutter_roll_feed_input_slot_list,
    &new_init_sequence, &je_deinit_sequence
  },
  /* 48: Stylus Pro 7600 */
  {
    (MODEL_VARIABLE_YES | MODEL_COMMAND_PRO | MODEL_GRAYMODE_YES |
     MODEL_ROLLFEED_YES | MODEL_XZEROMARGIN_NO | MODEL_VACUUM_YES |
     MODEL_FAST_360_NO),
    1, 1, 1, 1, 1, 1, 1, 1, 1,
    360, 2880, 2880, 14400, -1, 2880, 1440, 360, 180,
    INCH(24), INCH(1200), INCH(7), INCH(7),
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 1, 0, 0, 0, 0, 0,
    spro_c4pl_pigment_dotsizes, c4pl_pigment_densities, &variable_4pl_pigment_inks,
    &standard_paper_list, pro_reslist, &photo7_inklist,
    ultrachrome_bits, pro_base_res, &pro_roll_feed_input_slot_list,
    &new_init_sequence, &je_deinit_sequence
  },
  /* 49: Stylus Pro 9600 */
  {
    (MODEL_VARIABLE_YES | MODEL_COMMAND_PRO | MODEL_GRAYMODE_YES |
     MODEL_ROLLFEED_YES | MODEL_XZEROMARGIN_NO | MODEL_VACUUM_YES |
     MODEL_FAST_360_NO),
    1, 1, 1, 1, 1, 1, 1, 1, 1,
    360, 2880, 2880, 14400, -1, 2880, 1440, 360, 180,
    INCH(44), INCH(1200), INCH(7), INCH(7),
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 1, 0, 0, 0, 0, 0,
    spro_c4pl_pigment_dotsizes, c4pl_pigment_densities, &variable_4pl_pigment_inks,
    &standard_paper_list, pro_reslist, &photo7_inklist,
    ultrachrome_bits, pro_base_res, &pro_roll_feed_input_slot_list,
    &new_init_sequence, &je_deinit_sequence
  },
  /* 50: Stylus Photo 825/830 */
  {
    (MODEL_VARIABLE_YES | MODEL_COMMAND_2000 | MODEL_GRAYMODE_YES |
     MODEL_ROLLFEED_NO | MODEL_XZEROMARGIN_YES | MODEL_VACUUM_NO |
     MODEL_FAST_360_NO),
    48, 1, 3, 48, 1, 3, 48, 1, 3,
    360, 720, 720, 14400, -1, 2880, 1440, 90, 90,
    INCH(17 / 2), INCH(1200), INCH(2), INCH(4),
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 1, 0, 55, 0, 0, 0,
    c4pl_dotsizes, c4pl_densities, &variable_4pl_inks,
    &sp780_paper_list, standard_reslist, &photo_inklist,
    variable_bits, variable_base_res, &default_input_slot_list,
    &new_init_sequence, &je_deinit_sequence
  },
  /* 51: Stylus Photo 925 */
  {
    (MODEL_VARIABLE_YES | MODEL_COMMAND_2000 | MODEL_GRAYMODE_YES |
     MODEL_ROLLFEED_YES | MODEL_XZEROMARGIN_YES | MODEL_VACUUM_NO |
     MODEL_FAST_360_NO),
    48, 1, 3, 48, 1, 3, 48, 1, 3,
    360, 720, 720, 14400, -1, 2880, 1440, 90, 90,
    INCH(17 / 2), INCH(1200), INCH(2), INCH(4),
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 1, 0, 55, 0, 0, 0,
    c4pl_dotsizes, c4pl_densities, &variable_4pl_inks,
    &standard_paper_list, standard_reslist, &photo_inklist,
    variable_bits, variable_base_res, &cutter_roll_feed_input_slot_list,
    &new_init_sequence, &je_deinit_sequence
  },
  /* 52: Stylus Color C62 */
  {
    (MODEL_VARIABLE_YES | MODEL_COMMAND_2000 | MODEL_GRAYMODE_YES |
     MODEL_ROLLFEED_NO | MODEL_XZEROMARGIN_NO | MODEL_VACUUM_NO |
     MODEL_FAST_360_NO),
    48, 1, 3, 144, 1, 1, 144, 1, 1,
    360, 720, 720, 14400, -1, 2880, 1440, 90, 90,
    INCH(17 / 2), INCH(1200), INCH(2), INCH(4),
    9, 9, 0, 9, 9, 9, 9, 9, 9, 9, 0, 0, 9, 9, 0, 0,
    0, 1, 0, 0, 0, 0, 0,
    c4pl_dotsizes, c4pl_densities, &variable_4pl_inks,
    &standard_paper_list, standard_reslist, &standard_inklist,
    variable_bits, variable_base_res, &default_input_slot_list,
    &new_init_sequence, &je_deinit_sequence
  },
  /* 53: Japanese PM-950C */
  {
    (MODEL_VARIABLE_YES | MODEL_COMMAND_2000 | MODEL_GRAYMODE_NO |
     MODEL_ROLLFEED_YES | MODEL_XZEROMARGIN_YES | MODEL_VACUUM_NO |
     MODEL_FAST_360_NO),
    96, 1, 2, 96, 1, 2, 96, 1, 2,
    360, 720, 720, 14400, -1, 2880, 1440, 360, 180,
    INCH(17 / 2), INCH(1200), INCH(2), INCH(4),
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 1, 0, 190, 0, 0, 0,
    c2pl_dotsizes, c2pl_densities, &variable_2pl_inks,
    &sp950_paper_list, escp950_reslist, &photo7_japan_inklist,
    stp950_bits, stp950_base_res, &standard_roll_feed_input_slot_list,
    &new_init_sequence, &je_deinit_sequence
  },
  /* 54: Stylus Photo EX3 */
  {
    (MODEL_VARIABLE_YES | MODEL_COMMAND_1999 | MODEL_GRAYMODE_NO |
     MODEL_ROLLFEED_NO | MODEL_XZEROMARGIN_NO | MODEL_VACUUM_NO |
     MODEL_FAST_360_NO),
    32, 1, 4, 32, 1, 4, 32, 1, 4,
    360, 720, 720, 14400, -1, 1440, 720, 90, 90,
    INCH(13), INCH(44), INCH(2), INCH(4),
    9, 9, 0, 9, 9, 9, 0, 9, 9, 9, 0, 0, 9, 9, 0, 0,
    0, 1, 0, 0, 0, 0, 0,
    sc720_dotsizes, c6pl_densities, &variable_6pl_inks,
    &standard_paper_list, standard_reslist, &photo_inklist,
    variable_bits, variable_base_res, &default_input_slot_list,
    &new_init_sequence, &je_deinit_sequence
  },
  /* 55: Stylus C82/CX-5200 */
  {
    (MODEL_VARIABLE_YES | MODEL_COMMAND_2000 | MODEL_GRAYMODE_YES |
     MODEL_ROLLFEED_NO | MODEL_XZEROMARGIN_NO | MODEL_VACUUM_NO |
     MODEL_FAST_360_NO),
    59, 60, 2, 180, 180, 2, 180, 180, 2,
    360, 720, 720, 14400, -1, 2880, 1440, 360, 180,
    INCH(17 / 2), INCH(1200), INCH(2), INCH(4),
    9, 9, 0, 9, 9, 9, 9, 9, 9, 9, 0, 0, 9, 9, 0, 0,
    0, 1, 0, 0, -240, 0, 0,
    c3pl_pigment_dotsizes, c3pl_pigment_densities, &variable_3pl_pigment_inks,
    &c80_paper_list, standard_reslist, &c80_inklist,
    variable_bits, variable_base_res, &default_input_slot_list,
    &new_init_sequence, &je_deinit_sequence
  },
};

/*
 * "$Id: escp2-inks.c,v 1.58 2007/05/27 17:59:50 rlk Exp $"
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

/* Single drop size printers */
static const escp2_dropsize_t escp2_single_dropsizes =
  { "single", 1, { 1.0 } };

/* 6 pl printers */
static const escp2_dropsize_t escp2_low_dropsizes =
  { "low", 3, { 0.28, 0.58, 1.0 } };
static const escp2_dropsize_t escp2_6pl_dropsizes =
  { "6pl", 3, { 0.25, 0.5, 1.0 } };
static const escp2_dropsize_t escp2_6pl_1440_dropsizes =
  { "6pl_1440", 2, { 0.5, 1.0 } };
static const escp2_dropsize_t escp2_6pl_2880_dropsizes =
  { "6pl_2880", 1, { 1.0 } };

/* Stylus Color 480/580/C40/C50 */
static const escp2_dropsize_t escp2_x80_low_dropsizes =
  { "x80_low", 3, { 0.325, 0.5, 1.0 } };
static const escp2_dropsize_t escp2_x80_6pl_dropsizes =
  { "x80_6pl", 3, { 0.325, 0.5, 1.0 } };
static const escp2_dropsize_t escp2_x80_1440_6pl_dropsizes =
  { "x80_1440_6pl", 2, { 0.65, 1.0 } };
static const escp2_dropsize_t escp2_x80_2880_6pl_dropsizes =
  { "x80_2880_6pl", 1, { 1.0 } };

/* 2880 DPI capable 4 picolitre printers */
static const escp2_dropsize_t escp2_new_low_dropsizes =
  { "680_low", 3, { 0.375, 0.75, 1.0 } };
static const escp2_dropsize_t escp2_new_6pl_dropsizes =
  { "680_6pl", 3, { 0.375, 0.50, 1.0 } };
static const escp2_dropsize_t escp2_new_4pl_dropsizes =
  { "680_4pl", 3, { 0.50, 0.75, 1.0 } };
static const escp2_dropsize_t escp2_4pl_2880_dropsizes =
  { "4pl_2880", 1, { 1.0 } };

/* 1440 DPI capable printers */
static const escp2_dropsize_t escp2_4pl_dropsizes =
  { "4pl", 3, { 0.33, 0.50, 1.0 } };

/* Stylus Color 900/980 */
static const escp2_dropsize_t escp2_3pl_dropsizes =
  { "3pl", 3, { 0.25, 0.61, 1.0 } };
static const escp2_dropsize_t escp2_3pl_1440_dropsizes =
  { "3pl_1440", 2, { 0.39, 1.0 } };
static const escp2_dropsize_t escp2_3pl_2880_dropsizes =
  { "3pl_2880", 1, { 1.0 } };
static const escp2_dropsize_t escp2_980_6pl_dropsizes =
  { "980_6pl", 3, { 0.40, 0.675, 1.0 } };

/* Stylus Photo 960 */
static const escp2_dropsize_t escp2_2pl_360_dropsizes =
  { "2pl_360", 3, { 0.25, 0.5, 1.0 } };
static const escp2_dropsize_t escp2_2pl_720_dropsizes =
  { "2pl_720", 3, { 0.25, 0.5, 1.0 } };
static const escp2_dropsize_t escp2_2pl_1440_dropsizes =
  { "2pl_1440", 2, { 0.5, 1.0 } };
static const escp2_dropsize_t escp2_2pl_2880_dropsizes =
  { "2pl_2880", 1, { 1.0 } };

/* PM-G800 */
/* Guess */
static const escp2_dropsize_t escp2_1_5pl_360_dropsizes =
  { "1_5pl_360", 1, { 1, 0, 1.0 } };
/* 7, 14, 20 pl */
static const escp2_dropsize_t escp2_1_5pl_720_dropsizes =
  { "1_5pl_720", 3, { 0.35, 0.70, 1.0 } };
/*
 * Note that the site
 * (http://www.i-love-epson.co.jp/products/printer/inkjet/pmg800/pmg8002.htm)
 * is unclear: it says 3 pl MSDT, but the diagram reads 2 pl
 */
/* 3, 6, 13 pl */
/* Looks like 3, 7.5, 15 */
static const escp2_dropsize_t escp2_1_5pl_1440_dropsizes =
  { "1_5pl_1440", 3, { 0.2, 0.5, 1.0 } };
/*
 * See above comment.  3 pl makes more sense than 2 pl
 */
/* 1.5, 3, 6 pl */
/* Looks like 1.5, 3.25, 6 */
static const escp2_dropsize_t escp2_1_5pl_2880_dropsizes =
  { "1_5pl_2880", 3, { 0.25, 0.47, 1.0 } };
static const escp2_dropsize_t escp2_1_5pl_2880_2880_dropsizes =
  { "1_5pl_2880_2880", 2, { 0.53, 1.0 } };
static const escp2_dropsize_t escp2_1_5pl_5760_dropsizes =
  { "1_5pl_5760", 1, { 1.0 } };

/* E-100/Picturemate */
static const escp2_dropsize_t escp2_picturemate_1440_dropsizes =
  { "picturemate_1440", 3, { 0.4, 0.65, 1.0 } };
static const escp2_dropsize_t escp2_picturemate_2880_dropsizes =
  { "picturemate_2880", 2, { 0.615, 1.0 } };
static const escp2_dropsize_t escp2_picturemate_5760_dropsizes =
  { "picturemate_5760", 1, { 1.0 } };

/* Stylus Photo R300 */
static const escp2_dropsize_t escp2_r300_360_dropsizes =
  { "r300_360", 3, { 0.15, 0.3, 1.0 } };
static const escp2_dropsize_t escp2_r300_720_dropsizes =
  { "r300_720", 3, { 0.15, 0.5, 1.0 } };
static const escp2_dropsize_t escp2_r300_1440_dropsizes =
  { "r300_1440", 3, { 0.29, 0.48, 1.0 } };
static const escp2_dropsize_t escp2_r300_2880_dropsizes =
  { "r300_2880", 2, { 0.604, 1.0 } };
static const escp2_dropsize_t escp2_r300_2880_1440_dropsizes =
  { "r300_2880_1440", 1, { 1.0 } };

/* Claria inks */

/*
 * Tested relative drop sizes:
 *      S       M       L
 *
 * 0x21 3       4       5
 * 0x23 2       3       4
 * 0x24 1       3       5
 * 0x25 1       2       3
 * 0x26 2       3       5
 * 0x33 1       3       5
 *
 * Therefore, there's no real point to use drop sizes other than 0x24
 * and 0x25.  The economy settings (0x10 and 0x0) don't provide any
 * bigger drops.  We get quite smooth output at 720x360 and 360 DPI, and
 * even 360x180 is surprisingly smooth.
 * Roy Harrington estimates the drop sizes are:
 *
 * 1  1.5 pl
 * 2  2.4 pl
 * 3  6.3 pl
 * 4 12   pl (rlk estimate)
 * 5 21   pl
 */

static const escp2_dropsize_t escp2_claria_720_dropsizes =
  { "claria_720", 3, { 0.071, 0.3, 1.0 } };
static const escp2_dropsize_t escp2_claria_1440_dropsizes =
  { "claria_1440", 3, { 0.23, 0.37, 1.0 } };
static const escp2_dropsize_t escp2_claria_2880_dropsizes =
  { "claria_2880", 2, { 0.62, 1.0 } };
static const escp2_dropsize_t escp2_claria_5760_dropsizes =
  { "claria_5760", 1, { 1.0 } };

/* Stylus Photo 1400 */

/*
 * Tested relative drop sizes:
* 	S	M	L
*
* 0x21	3	4	5
* 0x25	1	2	3
* 0x33	1	3	5
* 0x34	2	3	5
*/

static const escp2_dropsize_t escp2_claria_1400_720_dropsizes =
  { "claria_1400_720", 3, { 0.3, 0.57, 1.0 } };
static const escp2_dropsize_t escp2_claria_1400_1440_720_dropsizes =
  { "claria_1400_1440_720", 3, { 0.071, 0.3, 1.0 } };
static const escp2_dropsize_t escp2_claria_1400_1440_dropsizes =
  { "claria_1400_1440", 3, { 0.23, 0.37, 1.0 } };
static const escp2_dropsize_t escp2_claria_1400_2880_dropsizes =
  { "claria_1400_2880", 2, { 0.62, 1.0 } };
static const escp2_dropsize_t escp2_claria_1400_5760_dropsizes =
  { "claria_1400_5760", 1, { 1.0 } };

/* Stylus Photo R2400 */
static const escp2_dropsize_t escp2_r2400_360_dropsizes =
  { "r2400_360", 1, { 1 } };
static const escp2_dropsize_t escp2_r2400_720_dropsizes =
  { "r2400_720", 3, { 0.180, 0.44, 1 } };
static const escp2_dropsize_t escp2_r2400_1440_dropsizes =
  { "r2400_1440", 3, { 0.180, 0.44, 1 } };
static const escp2_dropsize_t escp2_r2400_2880_dropsizes =
  { "r2400_2880", 3, { 0.180, 0.44, 1 } };
static const escp2_dropsize_t escp2_r2400_2880_1440_dropsizes =
  { "r2400_2880_1440", 2, { 0.41, 1 } };
static const escp2_dropsize_t escp2_r2400_2880_2880_dropsizes =
  { "r2400_2880_2880", 1, { 1.0 } };

/* Stylus C80 */
static const escp2_dropsize_t escp2_economy_pigment_dropsizes =
  { "economy_pigment", 3, { 0, 0, 1.0 } };
static const escp2_dropsize_t escp2_low_pigment_dropsizes =
  { "low_pigment", 3, { 0.28, 0, 1.0 } };
static const escp2_dropsize_t escp2_6pl_pigment_dropsizes =
  { "6pl_pigment", 3, { 0.28, 0, 1.0 } };
static const escp2_dropsize_t escp2_3pl_pigment_dropsizes =
  { "3pl_pigment", 3, { 0.25, 0.5, 1.0 } };
static const escp2_dropsize_t escp2_3pl_pigment_2880_dropsizes =
  { "3pl_pigment_2880", 2, { 0.5, 1.0 } };
static const escp2_dropsize_t escp2_3pl_pigment_5760_dropsizes =
  { "3pl_pigment_5760", 1, { 1.0 } };

/* Stylus C66 */
static const escp2_dropsize_t escp2_economy_pigment_c66_dropsizes =
  { "economy_pigment_c66", 3, { 0, 0, 1.0 } };
static const escp2_dropsize_t escp2_low_pigment_c66_dropsizes =
  { "low_pigment_c66", 3, { 0.125, 0.25, 1.0 } };
static const escp2_dropsize_t escp2_6pl_pigment_c66_dropsizes =
  { "6pl_pigment_c66", 3, { 0.28, 0, 1.0 } };
static const escp2_dropsize_t escp2_3pl_pigment_c66_dropsizes =
  { "3pl_pigment_c66", 3, { 0.25, 0.5, 1.0 } };
static const escp2_dropsize_t escp2_3pl_pigment_c66_2880_dropsizes =
  { "3pl_pigment_c66_2880", 2, { 0.5, 1.0 } };
static const escp2_dropsize_t escp2_3pl_pigment_c66_5760_dropsizes =
  { "3pl_pigment_c66_5760", 1, { 1.0 } };

/* Stylus Photo 2000P */
static const escp2_dropsize_t escp2_2000p_dropsizes =
  { "2000p", 2, { 0.55, 1.0 } };

/* Stylus Photo 2200, Stylus Pro 7600 */
static const escp2_dropsize_t escp2_ultrachrome_low_dropsizes =
  { "ultrachrome_low", 3, { 0.16, 0.4, 1.0 } };
static const escp2_dropsize_t escp2_ultrachrome_720_dropsizes =
  { "ultrachrome_720", 3, { 0.2, 0.45, 1.0 } };
static const escp2_dropsize_t escp2_ultrachrome_2880_dropsizes =
  { "ultrachrome_2880", 1, { 1.0 } };

/* Stylus Pro 10000 */
static const escp2_dropsize_t escp2_spro10000_dropsizes =
  { "spro10000", 2, { 0.661, 1.0 } };

static const escp2_drop_list_t simple_drops =
{
  &escp2_single_dropsizes,
  &escp2_single_dropsizes,
  &escp2_single_dropsizes,
  &escp2_single_dropsizes,
  &escp2_single_dropsizes,
  &escp2_single_dropsizes,
  &escp2_single_dropsizes,
  &escp2_single_dropsizes,
  &escp2_single_dropsizes,
};

static const escp2_drop_list_t variable_6pl_drops =
{
  &escp2_6pl_dropsizes,
  &escp2_6pl_dropsizes,
  &escp2_6pl_dropsizes,
  &escp2_6pl_dropsizes,
  &escp2_6pl_1440_dropsizes,
  &escp2_6pl_2880_dropsizes,
  &escp2_6pl_2880_dropsizes,
  &escp2_6pl_2880_dropsizes,
  &escp2_6pl_2880_dropsizes,
};

static const escp2_drop_list_t variable_x80_6pl_drops =
{
  &escp2_x80_low_dropsizes,
  &escp2_x80_low_dropsizes,
  &escp2_x80_low_dropsizes,
  &escp2_x80_6pl_dropsizes,
  &escp2_x80_1440_6pl_dropsizes,
  &escp2_x80_2880_6pl_dropsizes,
  &escp2_x80_2880_6pl_dropsizes,
  &escp2_x80_2880_6pl_dropsizes,
  &escp2_x80_2880_6pl_dropsizes,
};

static const escp2_drop_list_t variable_1440_4pl_drops =
{
  &escp2_low_dropsizes,
  &escp2_low_dropsizes,
  &escp2_low_dropsizes,
  &escp2_6pl_dropsizes,
  &escp2_4pl_dropsizes,
  &escp2_4pl_2880_dropsizes,
  &escp2_4pl_2880_dropsizes,
  &escp2_4pl_2880_dropsizes,
  &escp2_4pl_2880_dropsizes,
};

static const escp2_drop_list_t variable_2880_4pl_drops =
{
  &escp2_new_low_dropsizes,
  &escp2_new_low_dropsizes,
  &escp2_new_low_dropsizes,
  &escp2_new_6pl_dropsizes,
  &escp2_new_4pl_dropsizes,
  &escp2_4pl_2880_dropsizes,
  &escp2_4pl_2880_dropsizes,
  &escp2_4pl_2880_dropsizes,
  &escp2_4pl_2880_dropsizes,
};

static const escp2_drop_list_t variable_3pl_drops =
{
  &escp2_low_dropsizes,
  &escp2_low_dropsizes,
  &escp2_980_6pl_dropsizes,
  &escp2_3pl_dropsizes,
  &escp2_3pl_1440_dropsizes,
  &escp2_3pl_2880_dropsizes,
  &escp2_3pl_2880_dropsizes,
  &escp2_3pl_2880_dropsizes,
  &escp2_3pl_2880_dropsizes,
};

static const escp2_drop_list_t variable_2pl_drops =
{
  &escp2_2pl_360_dropsizes,
  &escp2_2pl_360_dropsizes,
  &escp2_2pl_360_dropsizes,
  &escp2_2pl_720_dropsizes,
  &escp2_2pl_1440_dropsizes,
  &escp2_2pl_2880_dropsizes,
  &escp2_2pl_2880_dropsizes,
  &escp2_2pl_2880_dropsizes,
  &escp2_2pl_2880_dropsizes,
};

static const escp2_drop_list_t variable_3pl_pmg_drops =
{
  &escp2_r300_360_dropsizes,
  &escp2_r300_360_dropsizes,
  &escp2_r300_360_dropsizes,
  &escp2_r300_720_dropsizes,
  &escp2_r300_1440_dropsizes,
  &escp2_r300_2880_dropsizes,
  &escp2_r300_2880_1440_dropsizes,
  &escp2_r300_2880_1440_dropsizes,
  &escp2_r300_2880_1440_dropsizes,
};

static const escp2_drop_list_t claria_drops =
{
  &escp2_claria_720_dropsizes,
  &escp2_claria_720_dropsizes,
  &escp2_claria_720_dropsizes,
  &escp2_claria_720_dropsizes,
  &escp2_claria_720_dropsizes,
  &escp2_claria_1440_dropsizes,
  &escp2_claria_1440_dropsizes,
  &escp2_claria_2880_dropsizes,
  &escp2_claria_5760_dropsizes,
};

static const escp2_drop_list_t claria_1400_drops =
{
  &escp2_claria_1400_1440_720_dropsizes,
  &escp2_claria_1400_1440_720_dropsizes,
  &escp2_claria_1400_720_dropsizes,
  &escp2_claria_1400_720_dropsizes,
  &escp2_claria_1400_1440_720_dropsizes,
  &escp2_claria_1400_1440_dropsizes,
  &escp2_claria_1400_1440_dropsizes,
  &escp2_claria_1400_2880_dropsizes,
  &escp2_claria_1400_5760_dropsizes,
};

static const escp2_drop_list_t variable_r2400_drops =
{
  &escp2_r2400_360_dropsizes,
  &escp2_r2400_360_dropsizes,
  &escp2_r2400_720_dropsizes,
  &escp2_r2400_720_dropsizes,
  &escp2_r2400_1440_dropsizes,
  &escp2_r2400_2880_dropsizes,
  &escp2_r2400_2880_1440_dropsizes,
  &escp2_r2400_2880_2880_dropsizes,
  &escp2_r2400_2880_2880_dropsizes,
};

static const escp2_drop_list_t variable_picturemate_drops =
{
  &escp2_picturemate_1440_dropsizes,
  &escp2_picturemate_1440_dropsizes,
  &escp2_picturemate_1440_dropsizes,
  &escp2_picturemate_1440_dropsizes,
  &escp2_picturemate_1440_dropsizes,
  &escp2_picturemate_1440_dropsizes,
  &escp2_picturemate_2880_dropsizes,
  &escp2_picturemate_5760_dropsizes,
  &escp2_picturemate_5760_dropsizes,
};

static const escp2_drop_list_t variable_1_5pl_drops =
{
  &escp2_1_5pl_360_dropsizes,
  &escp2_1_5pl_360_dropsizes,
  &escp2_1_5pl_720_dropsizes,	/* Even though we use 0x10 drop size */
  &escp2_1_5pl_720_dropsizes,
  &escp2_1_5pl_1440_dropsizes,
  &escp2_1_5pl_2880_dropsizes,
  &escp2_1_5pl_2880_dropsizes,
  &escp2_1_5pl_2880_2880_dropsizes,
  &escp2_1_5pl_5760_dropsizes,
};

static const escp2_drop_list_t variable_2000p_drops =
{
  &escp2_2000p_dropsizes,
  &escp2_2000p_dropsizes,
  &escp2_2000p_dropsizes,
  &escp2_2000p_dropsizes,
  &escp2_2000p_dropsizes,
  &escp2_2000p_dropsizes,
  &escp2_2000p_dropsizes,
  &escp2_2000p_dropsizes,
  &escp2_2000p_dropsizes
};

static const escp2_drop_list_t variable_ultrachrome_drops =
{
  &escp2_ultrachrome_low_dropsizes,
  &escp2_ultrachrome_low_dropsizes,
  &escp2_ultrachrome_low_dropsizes,
  &escp2_ultrachrome_720_dropsizes,
  &escp2_ultrachrome_720_dropsizes,
  &escp2_ultrachrome_2880_dropsizes,
  &escp2_ultrachrome_2880_dropsizes,
  &escp2_ultrachrome_2880_dropsizes,
  &escp2_ultrachrome_2880_dropsizes,
};

static const escp2_drop_list_t variable_3pl_pigment_drops =
{
  &escp2_economy_pigment_dropsizes,
  &escp2_low_pigment_dropsizes,
  &escp2_low_pigment_dropsizes,
  &escp2_6pl_pigment_dropsizes,
  &escp2_3pl_pigment_dropsizes,
  &escp2_3pl_pigment_2880_dropsizes,
  &escp2_3pl_pigment_5760_dropsizes,
  &escp2_3pl_pigment_5760_dropsizes,
  &escp2_3pl_pigment_5760_dropsizes,
};

static const escp2_drop_list_t variable_3pl_pigment_c66_drops =
{
  &escp2_economy_pigment_c66_dropsizes,
  &escp2_low_pigment_c66_dropsizes,
  &escp2_low_pigment_c66_dropsizes,
  &escp2_6pl_pigment_c66_dropsizes,
  &escp2_3pl_pigment_c66_dropsizes,
  &escp2_3pl_pigment_c66_2880_dropsizes,
  &escp2_3pl_pigment_c66_5760_dropsizes,
  &escp2_3pl_pigment_c66_5760_dropsizes,
  &escp2_3pl_pigment_c66_5760_dropsizes,
};

static const escp2_drop_list_t spro10000_drops =
{
  &escp2_spro10000_dropsizes,
  &escp2_spro10000_dropsizes,
  &escp2_spro10000_dropsizes,
  &escp2_spro10000_dropsizes,
  &escp2_spro10000_dropsizes,
  &escp2_spro10000_dropsizes,
  &escp2_spro10000_dropsizes,
  &escp2_spro10000_dropsizes,
  &escp2_spro10000_dropsizes
};


typedef struct
{
  const char *name;
  const escp2_drop_list_t *const drop_list;
} drop_list_t;

static const drop_list_t the_drop_lists[] =
{
  { "simple", &simple_drops },
  { "spro10000", &spro10000_drops },
  { "variable_1_5pl", &variable_1_5pl_drops },
  { "variable_2pl", &variable_2pl_drops },
  { "variable_3pl", &variable_3pl_drops },
  { "variable_3pl_pigment", &variable_3pl_pigment_drops },
  { "variable_3pl_pigment_c66", &variable_3pl_pigment_c66_drops },
  { "variable_3pl_pmg", &variable_3pl_pmg_drops },
  { "variable_claria", &claria_drops },
  { "variable_claria_1400", &claria_1400_drops },
  { "variable_r2400", &variable_r2400_drops },
  { "variable_picturemate", &variable_picturemate_drops },
  { "variable_1440_4pl", &variable_1440_4pl_drops },
  { "variable_ultrachrome", &variable_ultrachrome_drops },
  { "variable_2880_4pl", &variable_2880_4pl_drops },
  { "variable_6pl", &variable_6pl_drops },
  { "variable_2000p", &variable_2000p_drops },
  { "variable_x80_6pl", &variable_x80_6pl_drops },
};

const escp2_drop_list_t *
stpi_escp2_get_drop_list_named(const char *n)
{
  int i;
  if (n)
    for (i = 0; i < sizeof(the_drop_lists) / sizeof(drop_list_t); i++)
      {
	if (strcmp(n, the_drop_lists[i].name) == 0)
	  return the_drop_lists[i].drop_list;
      }
  return NULL;
}

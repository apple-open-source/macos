/*
 * "$Id: print-canon.c,v 1.1.1.4 2004/12/22 23:49:38 jlovell Exp $"
 *
 *   Print plug-in CANON BJL driver for the GIMP.
 *
 *   Copyright 1997-2000 Michael Sweet (mike@easysw.com),
 *	Robert Krawitz (rlk@alum.mit.edu) and
 *      Andy Thaller (thaller@ph.tum.de)
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

/*
 * Large parts of this file (mainly the ink handling) is based on
 * print-escp2.c -- refer to README.new-printer on how to adjust the colors
 * for a certain model.
 */

/* TODO-LIST
 *
 *   * adjust the colors of all supported models
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <gimp-print/gimp-print.h>
#include "gimp-print-internal.h"
#include <gimp-print/gimp-print-intl-internal.h>
#include <string.h>
#include <stdio.h>
#if defined(HAVE_VARARGS_H) && !defined(HAVE_STDARG_H)
#include <varargs.h>
#else
#include <stdarg.h>
#endif
#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif

/* Solaris with gcc has problems because gcc's limits.h doesn't #define */
/* this */
#ifndef CHAR_BIT
#define CHAR_BIT 8
#endif

#if (0)
#define EXPERIMENTAL_STUFF 0
#endif

#define MAX_CARRIAGE_WIDTH	13 /* This really needs to go away */

/*
 * We really need to get away from this silly static nonsense...
 */
#define MAX_PHYSICAL_BPI 1440
#define MAX_OVERSAMPLED 8
#define MAX_BPP 4
#define COMPBUFWIDTH (MAX_PHYSICAL_BPI * MAX_OVERSAMPLED * MAX_BPP * \
	MAX_CARRIAGE_WIDTH / CHAR_BIT)

#define MIN(a,b) (((a)<(b)) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

static const int channel_color_map[] =
{
  STP_ECOLOR_K, STP_ECOLOR_C, STP_ECOLOR_M, STP_ECOLOR_Y, STP_ECOLOR_C, STP_ECOLOR_M, STP_ECOLOR_Y
};

static const int subchannel_color_map[] =
{
  0, 0, 0, 0, 1, 1, 1
};

static const double ink_darknesses[] =
{
  1.0, 0.31 / .5, 0.61 / .97, 0.08
};

#define USE_3BIT_FOLD_TYPE 323

/*
 * For each printer, we can select from a variety of dot sizes.
 * For single dot size printers, the available sizes are usually 0,
 * which is the "default", and some subset of 1-4.  For simple variable
 * dot size printers (with only one kind of variable dot size), the
 * variable dot size is specified as 0x10.  For newer printers, there
 * is a choice of variable dot sizes available, 0x10, 0x11, and 0x12 in
 * order of increasing size.
 *
 * Normally, we want to specify the smallest dot size that lets us achieve
 * a density of less than .8 or thereabouts (above that we start to get
 * some dither artifacts).  This needs to be tested for each printer and
 * resolution.
 *
 * An entry of -1 in a slot means that this resolution is not available.
 *              0                      standard dot sizes are used.
 *              1                      drop modulation is used.
 */

/* We know a per-model base resolution (like 180dpi or 150dpi)
 * and multipliers for the base resolution in the dotsize-, densities-
 * and inklist:
 * for 180dpi base resolution we would have
 *   s_r11_4 for 4color ink @180dpi
 *   s_r22_4 for 4color ink @360dpi
 *   s_r33_4 for 4color ink @720dpi
 *   s_r43_4 for 4color ink @1440x720dpi
 */

typedef struct canon_dot_sizes
{
  int dot_r11;    /*  180x180  or   150x150  */
  int dot_r22;    /*  360x360  or   300x300  */
  int dot_r33;    /*  720x720  or   600x600  */
  int dot_r43;    /* 1440x720  or  1200x600  */
  int dot_r44;    /* 1440x1440 or  1200x1200 */
  int dot_r55;    /* 2880x2880 or  2400x2400 */
} canon_dot_size_t;

/*
 * Specify the base density for each available resolution.
 *
 */

typedef struct canon_densities
{
  double d_r11;  /*  180x180  or   150x150  */
  double d_r22;  /*  360x360  or   300x300  */
  double d_r33;  /*  720x720  or   600x600  */
  double d_r43;  /* 1440x720  or  1200x600  */
  double d_r44;  /* 1440x1440 or  1200x1200 */
  double d_r55;  /* 2880x2880 or  2400x2400 */
} canon_densities_t;



/*
 * Definition of the multi-level inks available to a given printer.
 * Each printer may use a different kind of ink droplet for variable
 * and single drop size for each supported horizontal resolution and
 * type of ink (4 or 6 color).
 *
 * Recall that 6 color ink is treated as simply another kind of
 * multi-level ink, but the driver offers the user a choice of 4 and
 * 6 color ink, so we need to define appropriate inksets for both
 * kinds of ink.
 *
 * Stuff like the MIS 4 and 6 "color" monochrome inks doesn't fit into
 * this model very nicely, so we'll either have to special case it
 * or find some way of handling it in here.
 */

typedef struct canon_variable_ink
{
  double density;
  const stp_shade_t *shades;
  int numshades;
} canon_variable_ink_t;

typedef struct canon_variable_inkset
{
  const canon_variable_ink_t *c;
  const canon_variable_ink_t *m;
  const canon_variable_ink_t *y;
  const canon_variable_ink_t *k;
} canon_variable_inkset_t;

/*
 * currenty unaccounted for are the 7color printers and the 3color ones
 * (which use CMY only printheads)
 *
 */

typedef struct canon_variable_inklist
{
  const int bits;
  const int colors;
  const canon_variable_inkset_t *r11;    /*  180x180  or   150x150  */
  const canon_variable_inkset_t *r22;    /*  360x360  or   300x300  */
  const canon_variable_inkset_t *r33;    /*  720x720  or   600x600  */
  const canon_variable_inkset_t *r43;    /* 1440x720  or  1200x600  */
  const canon_variable_inkset_t *r44;    /* 1440x1440 or  1200x1200 */
  const canon_variable_inkset_t *r55;    /* 2880x2880 or  2400x2400 */
} canon_variable_inklist_t;


#ifdef EXPERIMENTAL_STUFF
/*
 * A printmode is defined by its resolution (xdpi x ydpi), the bits per pixel
 * and the installed printhead.
 *
 * For a hereby defined printmode we specify the density and gamma multipliers
 * and the ink definition with optional adjustments for lum, hue and sat
 *
 */
typedef struct canon_variable_printmode
{
  const int xdpi;                      /* horizontal resolution */
  const int ydpi;                      /* vertical resolution   */
  const int bits;                      /* bits per pixel        */
  const int printhead;                 /* installed printhead   */
  const int quality;                   /* maximum init-quality  */
  const double density;                /* density multiplier    */
  const double gamma;                  /* gamma multiplier      */
  const canon_variable_inkset_t *inks; /* ink definition        */
  const char *lum_adjustment;          /* optional lum adj.     */
  const char *hue_adjustment;          /* optional hue adj.     */
  const char *sat_adjustment;          /* optional sat adj.     */
} canon_variable_printmode_t;
#endif

/* NOTE  NOTE  NOTE  NOTE  NOTE  NOTE  NOTE  NOTE  NOTE  NOTE  NOTE  NOTE
 *
 * The following dither ranges were taken from print-escp2.c and do NOT
 * represent the requirements of canon inks. Feel free to play with them
 * accoring to the escp2 part of doc/README.new-printer and send me a patch
 * if you get better results. Please send mail to thaller@ph.tum.de
 */

#define DECLARE_INK(name, density)		\
static const canon_variable_ink_t name##_ink =	\
{						\
  density,					\
  name##_shades,				\
  sizeof(name##_shades) / sizeof(stp_shade_t)	\
}

#define SHADE(density, name)				\
{ density, sizeof(name)/sizeof(stp_dotsize_t), name }

/*
 * Dither ranges specifically for Cyan/LightCyan (see NOTE above)
 *
 */

static const stp_dotsize_t single_dotsize[] =
{
  { 0x1, 1.0 }
};

static const stp_shade_t canon_Cc_1bit_shades[] =
{
  SHADE(1.0, single_dotsize),
  SHADE(0.25, single_dotsize),
};

DECLARE_INK(canon_Cc_1bit, 0.75);

/*
 * Dither ranges specifically for Magenta/LightMagenta (see NOTE above)
 *
 */

static const stp_shade_t canon_Mm_1bit_shades[] =
{
  SHADE(1.0, single_dotsize),
  SHADE(0.26, single_dotsize),
};

DECLARE_INK(canon_Mm_1bit, 0.75);

/*
 * Dither ranges specifically for any Color and 2bit/pixel (see NOTE above)
 *
 */
static const stp_dotsize_t two_bit_dotsize[] =
{
  { 0x1, 0.45 },
  { 0x2, 0.68 },
  { 0x3, 1.0 }
};

static const stp_shade_t canon_X_2bit_shades[] =
{
  SHADE(1.0, two_bit_dotsize)
};

DECLARE_INK(canon_X_2bit, 1.0);

/*
 * Dither ranges specifically for any Color/LightColor and 2bit/pixel
 * (see NOTE above)
 */
static const stp_shade_t canon_Xx_2bit_shades[] =
{
  SHADE(1.0, two_bit_dotsize),
  SHADE(0.33, two_bit_dotsize),
};

DECLARE_INK(canon_Xx_2bit, 1.0);

/*
 * Dither ranges specifically for any Color and 3bit/pixel
 * (see NOTE above)
 *
 * BIG NOTE: The bjc8200 has this kind of ink. One Byte seems to hold
 *           drop sizes for 3 pixels in a 3/2/2 bit fashion.
 *           Size values for 3bit-sized pixels range from 1 to 7,
 *           size values for 2bit-sized picels from 1 to 3 (kill msb).
 *
 *
 */
static const stp_dotsize_t three_bit_dotsize[] =
{
  { 0x1, 0.45 },
  { 0x2, 0.55 },
  { 0x3, 0.66 },
  { 0x4, 0.77 },
  { 0x5, 0.88 },
  { 0x6, 1.0 }
};

static const stp_shade_t canon_X_3bit_shades[] =
{
  SHADE(1.0, three_bit_dotsize)
};

DECLARE_INK(canon_X_3bit, 1.0);

/*
 * Dither ranges specifically for any Color/LightColor and 3bit/pixel
 * (see NOTE above)
 */
static const stp_shade_t canon_Xx_3bit_shades[] =
{
  SHADE(1.0, three_bit_dotsize),
  SHADE(0.33, three_bit_dotsize),
};

DECLARE_INK(canon_Xx_3bit, 1.0);


/* Inkset for printing in CMY and 1bit/pixel */
static const canon_variable_inkset_t ci_CMY_1 =
{
  NULL,
  NULL,
  NULL,
  NULL
};

/* Inkset for printing in CMY and 2bit/pixel */
static const canon_variable_inkset_t ci_CMY_2 =
{
  &canon_X_2bit_ink,
  &canon_X_2bit_ink,
  &canon_X_2bit_ink,
  NULL
};

/* Inkset for printing in CMYK and 1bit/pixel */
static const canon_variable_inkset_t ci_CMYK_1 =
{
  NULL,
  NULL,
  NULL,
  NULL
};

/* Inkset for printing in CcMmYK and 1bit/pixel */
static const canon_variable_inkset_t ci_CcMmYK_1 =
{
  &canon_Cc_1bit_ink,
  &canon_Mm_1bit_ink,
  NULL,
  NULL
};

/* Inkset for printing in CMYK and 2bit/pixel */
static const canon_variable_inkset_t ci_CMYK_2 =
{
  &canon_X_2bit_ink,
  &canon_X_2bit_ink,
  &canon_X_2bit_ink,
  &canon_X_2bit_ink
};

/* Inkset for printing in CcMmYK and 2bit/pixel */
static const canon_variable_inkset_t ci_CcMmYK_2 =
{
  &canon_Xx_2bit_ink,
  &canon_Xx_2bit_ink,
  &canon_X_2bit_ink,
  &canon_X_2bit_ink
};

/* Inkset for printing in CMYK and 3bit/pixel */
static const canon_variable_inkset_t ci_CMYK_3 =
{
  &canon_X_3bit_ink,
  &canon_X_3bit_ink,
  &canon_X_3bit_ink,
  &canon_X_3bit_ink
};

/* Inkset for printing in CcMmYK and 3bit/pixel */
static const canon_variable_inkset_t ci_CcMmYK_3 =
{
  &canon_Xx_3bit_ink,
  &canon_Xx_3bit_ink,
  &canon_X_3bit_ink,
  &canon_X_3bit_ink,
};


typedef canon_variable_inklist_t* canon_variable_inklist_p;

/* Ink set should be applicable for any CMYK based model */
static const canon_variable_inklist_t canon_ink_standard[] =
{
  {
    1,4,
    &ci_CMYK_1, &ci_CMYK_1, &ci_CMYK_1,
    &ci_CMYK_1, &ci_CMYK_1, &ci_CMYK_1,
  },
};

/* Ink set for printers using CMY and CMY photo printing, 1 or 2bit/pixel */
static const canon_variable_inklist_t canon_ink_oldphoto[] =
{
  {
    1,3,
    &ci_CMY_1, &ci_CMY_1, &ci_CMY_1,
    &ci_CMY_1, &ci_CMY_1, &ci_CMY_1,
  },
  {
    2,3,
    &ci_CMY_2, &ci_CMY_2,
    &ci_CMY_2, &ci_CMY_2,
    &ci_CMY_2, &ci_CMY_2,
  },
};

/* Ink set for printers using CMYK and CcMmYK printing, 1 or 2bit/pixel */
static const canon_variable_inklist_t canon_ink_standardphoto[] =
{
  {
    1,4,
    &ci_CMYK_1, &ci_CMYK_1, &ci_CMYK_1,
    &ci_CMYK_1, &ci_CMYK_1, &ci_CMYK_1,
  },
  {
    2,4,
    &ci_CMYK_2, &ci_CMYK_2,
    &ci_CMYK_2, &ci_CMYK_2,
    &ci_CMYK_2, &ci_CMYK_2,
  },
  {
    1,6,
    &ci_CcMmYK_1, &ci_CcMmYK_1, &ci_CcMmYK_1,
    &ci_CcMmYK_1, &ci_CcMmYK_1, &ci_CcMmYK_1,
  },
  {
    2,6,
    &ci_CcMmYK_2, &ci_CcMmYK_2, &ci_CcMmYK_2,
    &ci_CcMmYK_2, &ci_CcMmYK_2, &ci_CcMmYK_2,
  },
};

/* Ink set for printers using CMYK and CcMmYK printing, 1 or 3bit/pixel */
static const canon_variable_inklist_t canon_ink_superphoto[] =
{
  {
    1,4,
    &ci_CMYK_1, &ci_CMYK_1, &ci_CMYK_1,
    &ci_CMYK_1, &ci_CMYK_1, &ci_CMYK_1,
  },
  {
    3,4,
    &ci_CMYK_3, &ci_CMYK_3, &ci_CMYK_3,
    &ci_CMYK_3, &ci_CMYK_3, &ci_CMYK_3,
  },
  {
    3,6,
    &ci_CcMmYK_3, &ci_CcMmYK_3, &ci_CcMmYK_3,
    &ci_CcMmYK_3, &ci_CcMmYK_3, &ci_CcMmYK_3,
  },
};


static const char standard_sat_adjustment[] =
"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
"<gimp-print>\n"
"<curve wrap=\"wrap\" type=\"linear\" gamma=\"0\">\n"
"<sequence count=\"48\" lower-bound=\"0\" upper-bound=\"4\">\n"
/* C */  "1.00 1.00 1.00 1.00 1.00 1.00 1.00 1.00 "  /* B */
/* B */  "1.00 1.00 1.00 1.00 1.00 1.00 1.00 1.00 "  /* M */
/* M */  "1.00 0.95 0.90 0.90 0.90 0.90 0.90 0.90 "  /* R */
/* R */  "0.90 0.95 0.95 1.00 1.00 1.00 1.00 1.00 "  /* Y */
/* Y */  "1.00 1.00 1.00 1.00 1.00 1.00 1.00 1.00 "  /* G */
/* G */  "1.00 1.00 1.00 1.00 1.00 1.00 1.00 1.00 "  /* C */
"</sequence>\n"
"</curve>\n"
"</gimp-print>\n";

static const char standard_lum_adjustment[] =
"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
"<gimp-print>\n"
"<curve wrap=\"wrap\" type=\"linear\" gamma=\"0\">\n"
"<sequence count=\"48\" lower-bound=\"0\" upper-bound=\"4\">\n"
/* C */  "0.50 0.52 0.56 0.60 0.66 0.71 0.74 0.77 "  /* B */
/* B */  "0.81 0.79 0.74 0.68 0.70 0.74 0.77 0.82 "  /* M */
/* M */  "0.88 0.93 0.95 0.97 0.97 0.96 0.95 0.95 "  /* R */
/* R */  "0.95 0.96 0.97 0.98 0.99 1.00 1.00 1.00 "  /* Y */
/* Y */  "1.00 0.97 0.94 0.92 0.90 0.88 0.85 0.79 "  /* G */
/* G */  "0.69 0.64 0.58 0.54 0.54 0.54 0.53 0.51 "  /* C */
"</sequence>\n"
"</curve>\n"
"</gimp-print>\n";

static const char standard_hue_adjustment[] =
"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
"<gimp-print>\n"
"<curve wrap=\"wrap\" type=\"linear\" gamma=\"0\">\n"
"<sequence count=\"48\" lower-bound=\"-6\" upper-bound=\"6\">\n"
/* C */  "0.00 0.06 0.10 0.10 0.06 -.01 -.09 -.17 "  /* B */
/* B */  "-.25 -.33 -.38 -.38 -.36 -.34 -.34 -.34 "  /* M */
/* M */  "-.34 -.34 -.36 -.40 -.50 -.40 -.30 -.20 "  /* R */
/* R */  "-.12 -.07 -.04 -.02 0.00 0.00 0.00 0.00 "  /* Y */
/* Y */  "0.00 0.00 0.00 -.05 -.10 -.15 -.22 -.24 "  /* G */
/* G */  "-.26 -.30 -.33 -.28 -.25 -.20 -.13 -.06 "  /* C */
"</sequence>\n"
"</curve>\n"
"</gimp-print>\n";

typedef enum {
  COLOR_MONOCHROME = 1,
  COLOR_CMY = 3,
  COLOR_CMYK = 4,
  COLOR_CCMMYK= 6,
  COLOR_CCMMYYK= 7
} colormode_t;

typedef struct canon_caps {
  int model;          /* model number as used in printers.xml */
  int model_id;       /* model ID code for use in commands */
  int max_width;      /* maximum printable paper size */
  int max_height;
  int base_res;       /* base resolution - shall be 150 or 180 */
  int max_xdpi;       /* maximum horizontal resolution */
  int max_ydpi;       /* maximum vertical resolution */
  int max_quality;
  int border_left;    /* left margin, points */
  int border_right;   /* right margin, points */
  int border_top;     /* absolute top margin, points */
  int border_bottom;  /* absolute bottom margin, points */
  int inks;           /* installable cartridges (CANON_INK_*) */
  int slots;          /* available paperslots */
  unsigned long features;       /* special bjl settings */
#ifdef EXPERIMENTAL_STUFF
  const canon_variable_printmode_t *printmodes;
  int printmodes_cnt;
#else
  int dummy;
  const canon_dot_size_t dot_sizes;   /* Vector of dot sizes for resolutions */
  const canon_densities_t densities;   /* List of densities for each printer */
  const canon_variable_inklist_t *inxs; /* Choices of inks for this printer */
  int inxs_cnt;                         /* number of ink definitions in inxs */
#endif
  const char *lum_adjustment;
  const char *hue_adjustment;
  const char *sat_adjustment;
} canon_cap_t;

typedef struct
{
  const canon_cap_t *caps;
  unsigned char *cols[7];
  int delay[7];
  int delay_max;
  int buf_length;
  int out_width;
  int left;
  int emptylines;
  int bits;
  int ydpi;
} canon_privdata_t;

static void canon_write_line(stp_vars_t *v);


/* Codes for possible ink-tank combinations.
 * Each combo is represented by the colors that can be used with
 * the installed ink-tank(s)
 * Combinations of the codes represent the combinations allowed for a model
 * Note that only preferrable combinations should be used
 */
#define CANON_INK_K           1
#define CANON_INK_CMY         2
#define CANON_INK_CMYK        4
#define CANON_INK_CcMmYK      8
#define CANON_INK_CcMmYyK    16

#define CANON_INK_BLACK_MASK (CANON_INK_K|CANON_INK_CMYK|CANON_INK_CcMmYK)

#define CANON_INK_PHOTO_MASK (CANON_INK_CcMmYK|CANON_INK_CcMmYyK)

/* document feeding */
#define CANON_SLOT_ASF1    1
#define CANON_SLOT_ASF2    2
#define CANON_SLOT_MAN1    4
#define CANON_SLOT_MAN2    8

/* model peculiarities */
#define CANON_CAP_DMT       0x01ul    /* Drop Modulation Technology */
#define CANON_CAP_MSB_FIRST 0x02ul    /* how to send data           */
#define CANON_CAP_a         0x04ul
#define CANON_CAP_b         0x08ul
#define CANON_CAP_q         0x10ul
#define CANON_CAP_m         0x20ul
#define CANON_CAP_d         0x40ul
#define CANON_CAP_t         0x80ul
#define CANON_CAP_c         0x100ul
#define CANON_CAP_p         0x200ul
#define CANON_CAP_l         0x400ul
#define CANON_CAP_r         0x800ul
#define CANON_CAP_g         0x1000ul
#define CANON_CAP_ACKSHORT  0x2000ul

#define CANON_CAP_STD0 (CANON_CAP_b|CANON_CAP_c|CANON_CAP_d|\
                        CANON_CAP_l|CANON_CAP_q|CANON_CAP_t)

#define CANON_CAP_STD1 (CANON_CAP_b|CANON_CAP_c|CANON_CAP_d|CANON_CAP_l|\
                        CANON_CAP_m|CANON_CAP_p|CANON_CAP_q|CANON_CAP_t)

#ifdef EXPERIMENTAL_STUFF
#define CANON_MODES(A) A,sizeof(A)/sizeof(canon_variable_printmode_t*)
#else
#define CANON_MODES(A) 0
#endif

#define CANON_INK(A) A,sizeof(A)/sizeof(canon_variable_inklist_t*)


#ifdef EXPERIMENTAL_STUFF

#define BC_10   CANON_INK_K       /* b/w   */
#define BC_11   CANON_INK_CMYK    /* color */
#define BC_12   CANON_INK_CMYK    /* photo */
#define BC_20   CANON_INK_K       /* b/w   */
#define BC_21   CANON_INK_CMYK    /* color */
#define BC_22   CANON_INK_CMYK    /* photo */
#define BC_29   0                 /* neon! */
#define BC_3031 CANON_INK_CMYK    /* color */
#define BC_3231 CANON_INK_CcMmYK  /* photo */


static const canon_variable_printmode_t canon_nomodes[] =
{
  {0,0,0,0,0,0,0,0,0,0}
};

static const canon_variable_printmode_t canon_modes_30[] = {
  {  180, 180, 1, BC_10, 2,  1.0, 1.0, &ci_CMYK_1,   0,0,0 },
  {  360, 360, 1, BC_10, 2,  1.0, 1.0, &ci_CMYK_1,   0,0,0 },
  {  720, 360, 1, BC_10, 2,  1.0, 1.0, &ci_CMYK_1,   0,0,0 },
};

static const canon_variable_printmode_t canon_modes_85[] = {
  {  360, 360, 1, BC_10, 2,  1.0, 1.0, &ci_CMYK_1,   0,0,0 },
  {  360, 360, 1, BC_11, 2,  1.0, 1.0, &ci_CMYK_1,   0,0,0 },
  {  360, 360, 2, BC_11, 2,  1.0, 1.0, &ci_CMYK_2,   0,0,0 },
  {  360, 360, 1, BC_21, 2,  1.0, 1.0, &ci_CMYK_1,   0,0,0 },
  {  360, 360, 2, BC_21, 2,  1.0, 1.0, &ci_CMYK_2,   0,0,0 },
};

static const canon_variable_printmode_t canon_modes_2x00[] = {
  {  360, 360, 1, BC_20, 2,  1.0, 1.0, &ci_CMYK_1,   0,0,0 },
  {  360, 360, 1, BC_21, 2,  1.0, 1.0, &ci_CMYK_1,   0,0,0 },
  {  360, 360, 1, BC_22, 2,  1.0, 1.0, &ci_CMYK_1,   0,0,0 },
};

static const canon_variable_printmode_t canon_modes_6x00[] = {
  {  360, 360, 1, BC_3031, 2,  1.8, 1.0, &ci_CMYK_1,   0,0,0 },
  {  360, 360, 2, BC_3031, 2,  1.8, 1.0, &ci_CMYK_2,   0,0,0 },
  {  720, 720, 1, BC_3031, 2,  1.0, 1.0, &ci_CMYK_1,   0,0,0 },
  { 1440, 720, 1, BC_3031, 2,  0.5, 1.0, &ci_CMYK_1,   0,0,0 },
  {  360, 360, 1, BC_3231, 2,  1.8, 1.0, &ci_CcMmYK_1, 0,0,0 },
  {  360, 360, 2, BC_3231, 2,  1.8, 1.0, &ci_CcMmYK_2, 0,0,0 },
  {  720, 720, 1, BC_3231, 2,  1.0, 1.0, &ci_CcMmYK_1, 0,0,0 },
  { 1440, 720, 1, BC_3231, 2,  0.5, 1.0, &ci_CcMmYK_1, 0,0,0 },
};
#endif

static const canon_cap_t canon_model_capabilities[] =
{
  /* default settings for unknown models */

  {   -1, 17*72/2,842,180,180,20,20,20,20, CANON_INK_K, CANON_SLOT_ASF1, 0 },

  /* ******************************** */
  /*                                  */
  /* tested and color-adjusted models */
  /*                                  */
  /* ******************************** */




  /* ************************************ */
  /*                                      */
  /* tested models w/out color-adjustment */
  /*                                      */
  /* ************************************ */


  { /* Canon  BJ 30   *//* heads: BC-10 */
    30, 1,
    9.5*72, 14*72,
    90, 360, 360, 2,
    11, 9, 10, 18,
    CANON_INK_K,
    CANON_SLOT_ASF1,
    CANON_CAP_STD0 | CANON_CAP_a,
    CANON_MODES(canon_modes_30),
#ifndef EXPERIMENTAL_STUFF
    {-1,0,0,0,-1,-1}, /*090x090 180x180 360x360 720x360 720x720 1440x1440*/
    {1,1,1,1,1,1},    /*------- 180x180 360x360 720x360 ------- ---------*/
    CANON_INK(canon_ink_standard),
#endif
    standard_lum_adjustment,
    standard_hue_adjustment,
    standard_sat_adjustment
  },

  { /* Canon  BJC 85  *//* heads: BC-20 BC-21 BC-22 */
    85, 1,
    9.5*72, 14*72,
    90, 720, 360, 2,
    11, 9, 10, 18,
    CANON_INK_K | CANON_INK_CMYK | CANON_INK_CcMmYK,
    CANON_SLOT_ASF1,
    CANON_CAP_STD0 | CANON_CAP_a | CANON_CAP_DMT,
    CANON_MODES(canon_modes_85),
#ifndef EXPERIMENTAL_STUFF
    {-1,-1,1,0,-1,-1},/*090x090 180x180 360x360 720x360 720x720 1440x1440*/
    {1,1,1,1,1,1},    /*------- ------- 360x360 720x360 ------- ---------*/
    CANON_INK(canon_ink_standard),
#endif
    standard_lum_adjustment,
    standard_hue_adjustment,
    standard_sat_adjustment
  },

  { /* Canon BJC 4300 *//* heads: BC-20 BC-21 BC-22 BC-29 */
    4300, 1,
    618, 936,      /* 8.58" x 13 " */
    180, 1440, 720, 2,
    11, 9, 10, 18,
    CANON_INK_CMYK | CANON_INK_CcMmYK,
    CANON_SLOT_ASF1 | CANON_SLOT_MAN1,
    CANON_CAP_STD0 | CANON_CAP_DMT,
    CANON_MODES(canon_nomodes),
#ifndef EXPERIMENTAL_STUFF
    {-1,1,0,0,-1,-1}, /*180x180 360x360 720x720 1440x720 1440x1440 2880x2880*/
    {1,1,1,1,1,1},    /*------- 360x360 720x720 1440x720 --------- ---------*/
    CANON_INK(canon_ink_standard),
#endif
    standard_lum_adjustment,
    standard_hue_adjustment,
    standard_sat_adjustment
  },

  { /* Canon BJC 4400 *//* heads: BC-20 BC-21 BC-22 BC-29 */
    4400, 1,
    9.5*72, 14*72,
    90, 720, 360, 2,
    11, 9, 10, 18,
    CANON_INK_K | CANON_INK_CMYK | CANON_INK_CcMmYK,
    CANON_SLOT_ASF1,
    CANON_CAP_STD0 | CANON_CAP_a | CANON_CAP_DMT,
    CANON_MODES(canon_nomodes),
#ifndef EXPERIMENTAL_STUFF
    {-1,-1,0,0,-1,-1},/*090x090 180x180 360x360 720x360 720x720 1440x1440*/
    {1,1,1,1,1,1},    /*------- ------- 360x360 720x360 ------- ---------*/
    CANON_INK(canon_ink_standard),
#endif
    standard_lum_adjustment,
    standard_hue_adjustment,
    standard_sat_adjustment
  },

  { /* Canon BJC 6000 *//* heads: BC-30/BC-31 BC-32/BC-31 */
    6000, 3,
    618, 936,      /* 8.58" x 13 " */
    180, 1440, 720, 2,
    11, 9, 10, 18,
    CANON_INK_CMYK | CANON_INK_CcMmYK,
    CANON_SLOT_ASF1 | CANON_SLOT_MAN1,
    CANON_CAP_STD1 | CANON_CAP_DMT | CANON_CAP_ACKSHORT,
    CANON_MODES(canon_modes_6x00),
#ifndef EXPERIMENTAL_STUFF
    {-1,1,0,0,-1,-1}, /*180x180 360x360 720x720 1440x720 1440x1440 2880x2880*/
    {1,1.8,1,0.5,1,1},/*------- 360x360 720x720 1440x720 --------- ---------*/
    CANON_INK(canon_ink_standardphoto),
#endif
    standard_lum_adjustment,
    standard_hue_adjustment,
    standard_sat_adjustment
  },

  { /* Canon BJC 6200 *//* heads: BC-30/BC-31 BC-32/BC-31 */
    6200, 3,
    618, 936,      /* 8.58" x 13 " */
    180, 1440, 720, 2,
    11, 9, 10, 18,
    CANON_INK_CMYK | CANON_INK_CcMmYK,
    CANON_SLOT_ASF1 | CANON_SLOT_MAN1,
    CANON_CAP_STD1 | CANON_CAP_DMT | CANON_CAP_ACKSHORT,
    CANON_MODES(canon_modes_6x00),
#ifndef EXPERIMENTAL_STUFF
    {-1,1,0,0,-1,-1}, /*180x180 360x360 720x720 1440x720 1440x1440 2880x2880*/
    {0,1.8,1,.5,0,0}, /*------- 360x360 720x720 1440x720 --------- ---------*/
    CANON_INK(canon_ink_standardphoto),
#endif
    standard_lum_adjustment,
    standard_hue_adjustment,
    standard_sat_adjustment
  },

  { /* Canon BJC 6500 *//* heads: BC-30/BC-31 BC-32/BC-31 */
    6500, 3,
    842, 17*72,
    180, 1440, 720, 2,
    11, 9, 10, 18,
    CANON_INK_CMYK | CANON_INK_CcMmYK,
    CANON_SLOT_ASF1 | CANON_SLOT_MAN1,
    CANON_CAP_STD1 | CANON_CAP_DMT,
    CANON_MODES(canon_modes_6x00),
#ifndef EXPERIMENTAL_STUFF
    {-1,1,0,0,-1,-1}, /*180x180 360x360 720x720 1440x720 1440x1440 2880x2880*/
    {0,1.8,1,.5,0,0}, /*------- 360x360 720x720 1440x720 --------- ---------*/
    CANON_INK(canon_ink_standardphoto),
#endif
    standard_lum_adjustment,
    standard_hue_adjustment,
    standard_sat_adjustment
  },

  { /* Canon BJC 8200 *//* heads: BC-50 */
    8200, 3,
    842, 17*72,
    150, 1200,1200, 4,
    11, 9, 10, 18,
    CANON_INK_CMYK, /*  | CANON_INK_CcMmYK */
    CANON_SLOT_ASF1,
    CANON_CAP_STD1 | CANON_CAP_r | CANON_CAP_DMT | CANON_CAP_ACKSHORT,
    CANON_MODES(canon_nomodes),
#ifndef EXPERIMENTAL_STUFF
    {-1,0,0,-1,0,-1}, /*150x150 300x300 600x600 1200x600 1200x1200 2400x2400*/
    {1,1,1,1,1,1},    /*------- 300x300 600x600 -------- 1200x1200 ---------*/
    CANON_INK(canon_ink_superphoto),
#endif
    standard_lum_adjustment,
    standard_hue_adjustment,
    standard_sat_adjustment
  },


  /* *************** */
  /*                 */
  /* untested models */
  /*                 */
  /* *************** */


  { /* Canon BJC 210 *//* heads: BC-02 BC-05 BC-06 */
    210, 1,
    618, 936,      /* 8.58" x 13 " */
    90, 720, 360, 2,
    11, 9, 10, 18,
    CANON_INK_K | CANON_INK_CMY,
    CANON_SLOT_ASF1 | CANON_SLOT_MAN1,
    CANON_CAP_STD0,
    CANON_MODES(canon_nomodes),
#ifndef EXPERIMENTAL_STUFF
    {0,0,0,0,-1,-1},/*180x180 360x360 720x720 1440x720 1440x1440 2880x2880*/
    {1,1,1,1,1,1},    /*180x180 360x360 ------- -------- --------- ---------*/
    CANON_INK(canon_ink_standard),
#endif
    standard_lum_adjustment,
    standard_hue_adjustment,
    standard_sat_adjustment
  },
  { /* Canon BJC 240 *//* heads: BC-02 BC-05 BC-06 */
    240, 1,
    618, 936,      /* 8.58" x 13 " */
    90, 720, 360, 2,
    11, 9, 10, 18,
    CANON_INK_K | CANON_INK_CMY,
    CANON_SLOT_ASF1 | CANON_SLOT_MAN1,
    CANON_CAP_STD0 | CANON_CAP_DMT,
    CANON_MODES(canon_nomodes),
#ifndef EXPERIMENTAL_STUFF
    {0,0,1,0,-1,-1},/*180x180 360x360 720x720 1440x720 1440x1440 2880x2880*/
    {1,1,1,1,1,1},    /*180x180 360x360 ------- -------- --------- ---------*/
    CANON_INK(canon_ink_oldphoto),
#endif
    standard_lum_adjustment,
    standard_hue_adjustment,
    standard_sat_adjustment
  },
  { /* Canon BJC 250 *//* heads: BC-02 BC-05 BC-06 */
    250, 1,
    618, 936,      /* 8.58" x 13 " */
    90, 720, 360, 2,
    11, 9, 10, 18,
    CANON_INK_K | CANON_INK_CMY,
    CANON_SLOT_ASF1 | CANON_SLOT_MAN1,
    CANON_CAP_STD0 | CANON_CAP_DMT,
    CANON_MODES(canon_nomodes),
#ifndef EXPERIMENTAL_STUFF
    {0,0,1,0,-1,-1},/*180x180 360x360 720x720 1440x720 1440x1440 2880x2880*/
    {1,1,1,1,1,1},    /*180x180 360x360 ------- -------- --------- ---------*/
    CANON_INK(canon_ink_oldphoto),
#endif
    standard_lum_adjustment,
    standard_hue_adjustment,
    standard_sat_adjustment
  },
  { /* Canon BJC 1000 *//* heads: BC-02 BC-05 BC-06 */
    1000, 1,
    842, 17*72,
    90, 720, 360, 2,
    11, 9, 10, 18,
    CANON_INK_K | CANON_INK_CMY,
    CANON_SLOT_ASF1,
    CANON_CAP_STD0 | CANON_CAP_DMT | CANON_CAP_a,
    CANON_MODES(canon_nomodes),
#ifndef EXPERIMENTAL_STUFF
    {0,0,1,0,-1,-1},  /*180x180 360x360 720x720 1440x720 1440x1440 2880x2880*/
    {1,1,1,1,1,1},    /*180x180 360x360 ------- -------- --------- ---------*/
    CANON_INK(canon_ink_oldphoto),
#endif
    standard_lum_adjustment,
    standard_hue_adjustment,
    standard_sat_adjustment
  },
  { /* Canon BJC 2000 *//* heads: BC-20 BC-21 BC-22 BC-29 */
    2000, 1,
    842, 17*72,
    180, 720, 360, 2,
    11, 9, 10, 18,
    CANON_INK_CMYK,
    CANON_SLOT_ASF1,
    CANON_CAP_STD0 | CANON_CAP_a,
    CANON_MODES(canon_nomodes),
#ifndef EXPERIMENTAL_STUFF
    {0,0,-1,-1,-1,-1},/*180x180 360x360 720x720 1440x720 1440x1440 2880x2880*/
    {1,1,1,1,1,1},    /*180x180 360x360 ------- -------- --------- ---------*/
    CANON_INK(canon_ink_standard),
#endif
    standard_lum_adjustment,
    standard_hue_adjustment,
    standard_sat_adjustment
  },
  { /* Canon BJC 3000 *//* heads: BC-30 BC-33 BC-34 */
    3000, 3,
    842, 17*72,
    180, 1440, 720, 2,
    11, 9, 10, 18,
    CANON_INK_CMYK | CANON_INK_CcMmYK,
    CANON_SLOT_ASF1,
    CANON_CAP_STD0 | CANON_CAP_a | CANON_CAP_DMT, /*FIX? should have _r? */
    CANON_MODES(canon_nomodes),
#ifndef EXPERIMENTAL_STUFF
    {-1,1,0,0,-1,-1}, /*180x180 360x360 720x720 1440x720 1440x1440 2880x2880*/
    {1,1,1,1,1,1},    /*------- 360x360 720x720 1440x720 --------- ---------*/
    CANON_INK(canon_ink_standard),
#endif
    standard_lum_adjustment,
    standard_hue_adjustment,
    standard_sat_adjustment
  },
  { /* Canon BJC 6100 *//* heads: BC-30/BC-31 BC-32/BC-31 */
    6100, 3,
    842, 17*72,
    180, 1440, 720, 2,
    11, 9, 10, 18,
    CANON_INK_CMYK | CANON_INK_CcMmYK,
    CANON_SLOT_ASF1,
    CANON_CAP_STD1 | CANON_CAP_a | CANON_CAP_r | CANON_CAP_DMT,
    CANON_MODES(canon_modes_6x00),
#ifndef EXPERIMENTAL_STUFF
    {-1,1,0,0,-1,-1}, /*180x180 360x360 720x720 1440x720 1440x1440 2880x2880*/
    {1,1,1,1,1,1},    /*------- 360x360 720x720 1440x720 --------- ---------*/
    CANON_INK(canon_ink_standard),
#endif
    standard_lum_adjustment,
    standard_hue_adjustment,
    standard_sat_adjustment
  },
  { /* Canon BJC 7000 *//* heads: BC-60/BC-61 BC-60/BC-62   ??????? */
    7000, 3,
    842, 17*72,
    150, 1200, 600, 2,
    11, 9, 10, 18,
    CANON_INK_CMYK | CANON_INK_CcMmYyK,
    CANON_SLOT_ASF1,
    CANON_CAP_STD1,
    CANON_MODES(canon_nomodes),
#ifndef EXPERIMENTAL_STUFF
    {-1,0,0,0,-1,-1}, /*150x150 300x300 600x600 1200x600 1200x1200 2400x2400*/
    {1,3.5,1.8,1,1,1},/*------- 300x300 600x600 1200x600 --------- ---------*/
    CANON_INK(canon_ink_standard),
#endif
    standard_lum_adjustment,
    standard_hue_adjustment,
    standard_sat_adjustment
  },
  { /* Canon BJC 7100 *//* heads: BC-60/BC-61 BC-60/BC-62   ??????? */
    7100, 3,
    842, 17*72,
    150, 1200, 600, 2,
    11, 9, 10, 18,
    CANON_INK_CMYK | CANON_INK_CcMmYyK,
    CANON_SLOT_ASF1,
    CANON_CAP_STD0,
    CANON_MODES(canon_nomodes),
#ifndef EXPERIMENTAL_STUFF
    {-1,0,0,0,-1,-1}, /*150x150 300x300 600x600 1200x600 1200x1200 2400x2400*/
    {1,1,1,1,1,1},    /*------- 300x300 600x600 1200x600 --------- ---------*/
    CANON_INK(canon_ink_standard),
#endif
    standard_lum_adjustment,
    standard_hue_adjustment,
    standard_sat_adjustment
  },

  /*****************************/
  /*                           */
  /*  extremely fuzzy models   */
  /* (might never work at all) */
  /*                           */
  /*****************************/

  { /* Canon BJC 5100 *//* heads: BC-20 BC-21 BC-22 BC-23 BC-29 */
    5100, 1,
    17*72, 22*72,
    180, 1440, 720, 2,
    11, 9, 10, 18,
    CANON_INK_CMYK | CANON_INK_CcMmYK,
    CANON_SLOT_ASF1,
    CANON_CAP_STD0 | CANON_CAP_DMT,
    CANON_MODES(canon_nomodes),
#ifndef EXPERIMENTAL_STUFF
    {-1,1,0,0,-1,-1}, /*180x180 360x360 720x720 1440x720 1440x1440 2880x2880*/
    {1,1,1,1,1,1},    /*------- 360x360 720x720 1440x720 --------- ---------*/
    CANON_INK(canon_ink_standard),
#endif
    standard_lum_adjustment,
    standard_hue_adjustment,
    standard_sat_adjustment
  },
  { /* Canon BJC 5500 *//* heads: BC-20 BC-21 BC-29 */
    5500, 1,
    22*72, 34*72,
    180, 720, 360, 2,
    11, 9, 10, 18,
    CANON_INK_CMYK | CANON_INK_CcMmYK,
    CANON_SLOT_ASF1,
    CANON_CAP_STD0 | CANON_CAP_a,
    CANON_MODES(canon_nomodes),
#ifndef EXPERIMENTAL_STUFF
    {0,0,-1,-1,-1,-1},/*180x180 360x360 720x720 1440x720 1440x1440 2880x2880*/
    {1,1,1,1,1,1},    /*180x180 360x360 ------- -------- --------- ---------*/
    CANON_INK(canon_ink_standard),
#endif
    standard_lum_adjustment,
    standard_hue_adjustment,
    standard_sat_adjustment
  },
  { /* Canon BJC 6500 *//* heads: BC-30/BC-31 BC-32/BC-31 */
    6500, 3,
    17*72, 22*72,
    180, 1440, 720, 2,
    11, 9, 10, 18,
    CANON_INK_CMYK | CANON_INK_CcMmYK,
    CANON_SLOT_ASF1,
    CANON_CAP_STD1 | CANON_CAP_a | CANON_CAP_DMT,
    CANON_MODES(canon_nomodes),
#ifndef EXPERIMENTAL_STUFF
    {-1,1,0,0,-1,-1}, /*180x180 360x360 720x720 1440x720 1440x1440 2880x2880*/
    {1,1,1,1,1,1},    /*------- 360x360 720x720 1440x720 --------- ---------*/
    CANON_INK(canon_ink_standard),
#endif
    standard_lum_adjustment,
    standard_hue_adjustment,
    standard_sat_adjustment
  },
  { /* Canon BJC 8500 *//* heads: BC-80/BC-81 BC-82/BC-81 */
    8500, 3,
    17*72, 22*72,
    150, 1200,1200, 2,
    11, 9, 10, 18,
    CANON_INK_CMYK | CANON_INK_CcMmYK,
    CANON_SLOT_ASF1,
    CANON_CAP_STD0,
    CANON_MODES(canon_nomodes),
#ifndef EXPERIMENTAL_STUFF
    {-1,0,0,-1,0,-1}, /*150x150 300x300 600x600 1200x600 1200x1200 2400x2400*/
    {1,1,1,1,1,1},    /*------- 300x300 600x600 -------- 1200x1200 ---------*/
    CANON_INK(canon_ink_standard),
#endif
    standard_lum_adjustment,
    standard_hue_adjustment,
    standard_sat_adjustment
  },
};

typedef struct {
  int x;
  int y;
  const char *name;
  const char *text;
  const char *name_dmt;
  const char *text_dmt;
} canon_res_t;

static const canon_res_t canon_resolutions[] = {
  { 90, 90, "90x90dpi", N_("90x90 DPI"), "90x90dmt", N_("90x90 DPI DMT") },
  { 180, 180, "180x180dpi", N_("180x180 DPI"), "180x180dmt", N_("180x180 DPI DMT") },
  { 360, 360, "360x360dpi", N_("360x360 DPI"), "360x360dmt", N_("360x360 DPI DMT") },
  { 720, 360, "720x360dpi", N_("720x360 DPI"), "720x360dmt", N_("720x360 DPI DMT") },
  { 720, 720, "720x720dpi", N_("720x720 DPI"), "720x720dmt", N_("720x720 DPI DMT") },
  { 1440, 720, "1440x720dpi", N_("1440x720 DPI"), "1440x720dmt", N_("1440x720 DPI DMT") },
  { 1440, 1440, "1440x1440dpi", N_("1440x1440 DPI"), "1440x1440dmt", N_("1440x1440 DPI DMT") },
  { 2880, 2880, "2880x2880dpi", N_("2880x2880 DPI"), "2880x2880dmt", N_("2880x2880 DPI DMT") },
  { 150, 150, "150x150dpi", N_("150x150 DPI"), "150x150dmt", N_("150x150 DPI DMT") },
  { 300, 300, "300x300dpi", N_("300x300 DPI"), "300x300dmt", N_("300x300 DPI DMT") },
  { 600, 300, "600x300dpi", N_("600x300 DPI"), "600x300dmt", N_("600x300 DPI DMT") },
  { 600, 600, "600x600dpi", N_("600x600 DPI"), "600x600dmt", N_("600x600 DPI DMT") },
  { 1200, 600, "1200x600dpi", N_("1200x600 DPI"), "1200x600dmt", N_("1200x600 DPI DMT") },
  { 1200, 1200, "1200x1200dpi", N_("1200x1200 DPI"), "1200x1200dmt", N_("1200x1200 DPI DMT") },
  { 2400, 2400, "2400x2400dpi", N_("2400x2400 DPI"), "2400x2400dmt", N_("2400x2400 DPI DMT") },
  { 0, 0, NULL, NULL, NULL, NULL }
};

static const char plain_paper_lum_adjustment[] =
"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
"<gimp-print>\n"
"<curve wrap=\"wrap\" type=\"linear\" gamma=\"0\">\n"
"<sequence count=\"48\" lower-bound=\"0\" upper-bound=\"4\">\n"
"1.20 1.22 1.28 1.34 1.39 1.42 1.45 1.48 "  /* C */
"1.50 1.40 1.30 1.25 1.20 1.10 1.05 1.05 "  /* B */
"1.05 1.05 1.05 1.05 1.05 1.05 1.05 1.05 "  /* M */
"1.05 1.05 1.05 1.10 1.10 1.10 1.10 1.10 "  /* R */
"1.10 1.15 1.30 1.45 1.60 1.75 1.90 2.00 "  /* Y */
"2.10 2.00 1.80 1.70 1.60 1.50 1.40 1.30 "  /* G */
"</sequence>\n"
"</curve>\n"
"</gimp-print>\n";

typedef struct {
  const char *name;
  const char *text;
  int media_code;
  double base_density;
  double k_lower_scale;
  double k_upper;
  const char *hue_adjustment;
  const char *lum_adjustment;
  const char *sat_adjustment;
} paper_t;

typedef struct {
  const canon_cap_t *caps;
  int printing_color;
  const paper_t *pt;
  int print_head;
  int colormode;
  const char *source_str;
  int xdpi;
  int ydpi;
  int page_width;
  int page_height;
  int top;
  int left;
  int bits;
} canon_init_t;

static const paper_t canon_paper_list[] = {
  { "Plain",		N_ ("Plain Paper"),                0x00, 0.50, 0.25, 0.500, 0, 0, 0 },
  { "Transparency",	N_ ("Transparencies"),             0x02, 1.00, 1.00, 0.900, 0, 0, 0 },
  { "BackPrint",	N_ ("Back Print Film"),            0x03, 1.00, 1.00, 0.900, 0, 0, 0 },
  { "Fabric",		N_ ("Fabric Sheets"),              0x04, 0.50, 0.25, 0.500, 0, 0, 0 },
  { "Envelope",		N_ ("Envelope"),                   0x08, 0.50, 0.25, 0.500, 0, 0, 0 },
  { "Coated",		N_ ("High Resolution Paper"),      0x07, 0.78, 0.25, 0.500, 0, 0, 0 },
  { "TShirt",		N_ ("T-Shirt Transfers"),          0x03, 0.50, 0.25, 0.500, 0, 0, 0 },
  { "GlossyFilm",	N_ ("High Gloss Film"),            0x06, 1.00, 1.00, 0.999, 0, 0, 0 },
  { "GlossyPaper",	N_ ("Glossy Photo Paper"),         0x05, 1.00, 1.00, 0.999, 0, 0, 0 },
  { "GlossyCard",	N_ ("Glossy Photo Cards"),         0x0a, 1.00, 1.00, 0.999, 0, 0, 0 },
  { "GlossyPro",	N_ ("Photo Paper Pro"),            0x09, 1.00, 1.00, 0.999, 0, 0, 0 },
  { "Other",		N_ ("Other"),                      0x00, 0.50, 0.25, .5, 0, 0, 0 },
};

static const int paper_type_count = sizeof(canon_paper_list) / sizeof(paper_t);

static const stp_parameter_t the_parameters[] =
{
  {
    "PageSize", N_("Page Size"), N_("Basic Printer Setup"),
    N_("Size of the paper being printed to"),
    STP_PARAMETER_TYPE_STRING_LIST, STP_PARAMETER_CLASS_CORE,
    STP_PARAMETER_LEVEL_BASIC, 1, 1, -1, 1, 0
  },
  {
    "MediaType", N_("Media Type"), N_("Basic Printer Setup"),
    N_("Type of media (plain paper, photo paper, etc.)"),
    STP_PARAMETER_TYPE_STRING_LIST, STP_PARAMETER_CLASS_FEATURE,
    STP_PARAMETER_LEVEL_BASIC, 1, 1, -1, 1, 0
  },
  {
    "InputSlot", N_("Media Source"), N_("Basic Printer Setup"),
    N_("Source (input slot) of the media"),
    STP_PARAMETER_TYPE_STRING_LIST, STP_PARAMETER_CLASS_FEATURE,
    STP_PARAMETER_LEVEL_BASIC, 1, 1, -1, 1, 0
  },
  {
    "Resolution", N_("Resolution"), N_("Basic Printer Setup"),
    N_("Resolution and quality of the print"),
    STP_PARAMETER_TYPE_STRING_LIST, STP_PARAMETER_CLASS_FEATURE,
    STP_PARAMETER_LEVEL_BASIC, 1, 1, -1, 1, 0
  },
  {
    "InkType", N_("Ink Type"), N_("Advanced Printer Setup"),
    N_("Type of ink in the printer"),
    STP_PARAMETER_TYPE_STRING_LIST, STP_PARAMETER_CLASS_FEATURE,
    STP_PARAMETER_LEVEL_BASIC, 1, 1, -1, 1, 0
  },
  {
    "InkChannels", N_("Ink Channels"), N_("Advanced Printer Functionality"),
    N_("Ink Channels"),
    STP_PARAMETER_TYPE_INT, STP_PARAMETER_CLASS_FEATURE,
    STP_PARAMETER_LEVEL_INTERNAL, 0, 0, -1, 0, 0
  },
  {
    "PrintingMode", N_("Printing Mode"), N_("Core Parameter"),
    N_("Printing Output Mode"),
    STP_PARAMETER_TYPE_STRING_LIST, STP_PARAMETER_CLASS_CORE,
    STP_PARAMETER_LEVEL_BASIC, 1, 1, -1, 1, 0
  },
};

static const int the_parameter_count =
sizeof(the_parameters) / sizeof(const stp_parameter_t);

typedef struct
{
  const stp_parameter_t param;
  double min;
  double max;
  double defval;
  int color_only;
} float_param_t;

static const float_param_t float_parameters[] =
{
  {
    {
      "CyanDensity", N_("Cyan Balance"), N_("Output Level Adjustment"),
      N_("Adjust the cyan balance"),
      STP_PARAMETER_TYPE_DOUBLE, STP_PARAMETER_CLASS_OUTPUT,
      STP_PARAMETER_LEVEL_ADVANCED, 0, 1, 1, 1, 0
    }, 0.0, 2.0, 1.0, 1
  },
  {
    {
      "MagentaDensity", N_("Magenta Balance"), N_("Output Level Adjustment"),
      N_("Adjust the magenta balance"),
      STP_PARAMETER_TYPE_DOUBLE, STP_PARAMETER_CLASS_OUTPUT,
      STP_PARAMETER_LEVEL_ADVANCED, 0, 1, 2, 1, 0
    }, 0.0, 2.0, 1.0, 1
  },
  {
    {
      "YellowDensity", N_("Yellow Balance"), N_("Output Level Adjustment"),
      N_("Adjust the yellow balance"),
      STP_PARAMETER_TYPE_DOUBLE, STP_PARAMETER_CLASS_OUTPUT,
      STP_PARAMETER_LEVEL_ADVANCED, 0, 1, 3, 1, 0
    }, 0.0, 2.0, 1.0, 1
  },
  {
    {
      "BlackDensity", N_("Black Balance"), N_("Output Level Adjustment"),
      N_("Adjust the black balance"),
      STP_PARAMETER_TYPE_DOUBLE, STP_PARAMETER_CLASS_OUTPUT,
      STP_PARAMETER_LEVEL_ADVANCED, 0, 1, 0, 1, 0
    }, 0.0, 2.0, 1.0, 1
  },
  {
    {
      "LightCyanTransition", N_("Light Cyan Transition"), N_("Advanced Ink Adjustment"),
      N_("Light Cyan Transition"),
      STP_PARAMETER_TYPE_DOUBLE, STP_PARAMETER_CLASS_OUTPUT,
      STP_PARAMETER_LEVEL_ADVANCED4, 0, 1, -1, 1, 0
    }, 0.0, 5.0, 1.0, 1
  },
  {
    {
      "LightMagentaTransition", N_("Light Magenta Transition"), N_("Advanced Ink Adjustment"),
      N_("Light Magenta Transition"),
      STP_PARAMETER_TYPE_DOUBLE, STP_PARAMETER_CLASS_OUTPUT,
      STP_PARAMETER_LEVEL_ADVANCED4, 0, 1, -1, 1, 0
    }, 0.0, 5.0, 1.0, 1
  },
 {
    {
      "LightYellowTransition", N_("Light Yellow Transition"), N_("Advanced Ink Adjustment"),
      N_("Light Yellow Transition"),
      STP_PARAMETER_TYPE_DOUBLE, STP_PARAMETER_CLASS_OUTPUT,
      STP_PARAMETER_LEVEL_ADVANCED4, 0, 1, -1, 1, 0
    }, 0.0, 5.0, 1.0, 1
  },
};


static const int float_parameter_count =
sizeof(float_parameters) / sizeof(const float_param_t);

static const paper_t *
get_media_type(const char *name)
{
  int i;
  if (name)
    for (i = 0; i < paper_type_count; i++)
      {
	/* translate paper_t.name */
	if (!strcmp(name, canon_paper_list[i].name))
	  return &(canon_paper_list[i]);
      }
  return NULL;
}


static const canon_cap_t * canon_get_model_capabilities(int model)
{
  int i;
  int models= sizeof(canon_model_capabilities) / sizeof(canon_cap_t);
  for (i=0; i<models; i++) {
    if (canon_model_capabilities[i].model == model) {
      return &(canon_model_capabilities[i]);
    }
  }
  stp_deprintf(STP_DBG_CANON,"canon: model %d not found in capabilities list.\n",model);
  return &(canon_model_capabilities[0]);
}

static int
canon_source_type(const char *name, const canon_cap_t * caps)
{
  /* used internally: do not translate */
  if (name)
    {
      if (!strcmp(name,"Auto"))    return 4;
      if (!strcmp(name,"Manual"))    return 0;
      if (!strcmp(name,"ManualNP")) return 1;
    }

  stp_deprintf(STP_DBG_CANON,"canon: Unknown source type '%s' - reverting to auto\n",name);
  return 4;
}

static int
canon_printhead_type(const char *name, const canon_cap_t * caps)
{
  /* used internally: do not translate */
  if (name)
    {
      if (!strcmp(name,"Gray"))             return 0;
      if (!strcmp(name,"RGB"))             return 1;
      if (!strcmp(name,"CMYK"))       return 2;
      if (!strcmp(name,"PhotoCMY"))       return 3;
      if (!strcmp(name,"Photo"))             return 4;
      if (!strcmp(name,"PhotoCMYK")) return 5;
    }
  if (name && *name == 0) {
    if (caps->inks & CANON_INK_CMYK) return 2;
    if (caps->inks & CANON_INK_CMY)  return 1;
    if (caps->inks & CANON_INK_K)    return 0;
  }

  stp_deprintf(STP_DBG_CANON,"canon: Unknown head combo '%s' - reverting to black\n",name);
  return 0;
}

static colormode_t
canon_printhead_colors(const char *name, const canon_cap_t * caps)
{
  /* used internally: do not translate */
  if (name)
    {
      if (!strcmp(name,"Gray"))             return COLOR_MONOCHROME;
      if (!strcmp(name,"RGB"))             return COLOR_CMY;
      if (!strcmp(name,"CMYK"))       return COLOR_CMYK;
      if (!strcmp(name,"PhotoCMY"))       return COLOR_CCMMYK;
      if (!strcmp(name,"PhotoCMYK")) return COLOR_CCMMYYK;
    }

  if (name && *name == 0) {
    if (caps->inks & CANON_INK_CMYK) return COLOR_CMYK;
    if (caps->inks & CANON_INK_CMY)  return COLOR_CMY;
    if (caps->inks & CANON_INK_K)    return COLOR_MONOCHROME;
  }

  stp_deprintf(STP_DBG_CANON,"canon: Unknown head combo '%s' - reverting to black\n",name);
  return COLOR_MONOCHROME;
}

static unsigned char
canon_size_type(const stp_vars_t *v, const canon_cap_t * caps)
{
  const stp_papersize_t *pp = stp_get_papersize_by_size(stp_get_page_height(v),
							stp_get_page_width(v));
  if (pp)
    {
      const char *name = pp->name;
      /* used internally: do not translate */
      /* built ins: */
      if (!strcmp(name,"A5"))          return 0x01;
      if (!strcmp(name,"A4"))          return 0x03;
      if (!strcmp(name,"B5"))          return 0x08;
      if (!strcmp(name,"Letter"))      return 0x0d;
      if (!strcmp(name,"Legal"))       return 0x0f;
      if (!strcmp(name,"COM10")) return 0x16;
      if (!strcmp(name,"DL")) return 0x17;
      if (!strcmp(name,"LetterExtra"))     return 0x2a;
      if (!strcmp(name,"A4Extra"))         return 0x2b;
      if (!strcmp(name,"w288h144"))   return 0x2d;
      /* custom */

      stp_deprintf(STP_DBG_CANON,"canon: Unknown paper size '%s' - using custom\n",name);
    } else {
      stp_deprintf(STP_DBG_CANON,"canon: Couldn't look up paper size %dx%d - "
	      "using custom\n",stp_get_page_height(v), stp_get_page_width(v));
    }
  return 0;
}

#ifndef EXPERIMENTAL_STUFF
static int canon_res_code(const canon_cap_t * caps, int xdpi, int ydpi)
{
  int x, y, res= 0;

  for (x=1; x<6; x++) if ((xdpi/caps->base_res) == (1<<(x-1))) res= (x<<4);
  for (y=1; y<6; y++) if ((ydpi/caps->base_res) == (1<<(y-1))) res|= y;

  return res;
}
#else
static const canon_variable_printmode_t *canon_printmode(const canon_cap_t * caps,
							 int xdpi, int ydpi,
							 int bpp, int head)
{
  const canon_variable_printmode_t *modes;
  int modes_cnt;
  int i;
  if (!caps) return 0;
  modes= caps->printmodes;
  modes_cnt= caps->printmodes_cnt;
  /* search for the right printmode: */
  for (i=0; i<modes_cnt; i++) {
    if ((modes[i].xdpi== xdpi) && (modes[i].ydpi== ydpi) &&
	(modes[i].bits== bpp) && (modes[i].printhead== head))
      {
	return &(modes[i]);
      }
  }
  /* nothing found -> either return 0 or apply some policy to
   * get a fallback printmode
   */
  if (modes[0].xdpi) return modes;
  return 0;
}
#endif

static int
canon_ink_type(const canon_cap_t * caps, int res_code)
{
#ifndef EXPERIMENTAL_STUFF
  switch (res_code)
    {
    case 0x11: return caps->dot_sizes.dot_r11;
    case 0x22: return caps->dot_sizes.dot_r22;
    case 0x33: return caps->dot_sizes.dot_r33;
    case 0x43: return caps->dot_sizes.dot_r43;
    case 0x44: return caps->dot_sizes.dot_r44;
    case 0x55: return caps->dot_sizes.dot_r55;
    }
  return -1;
#else
  return -1;
#endif
}

static const char *
canon_lum_adjustment(int model)
{
  const canon_cap_t * caps= canon_get_model_capabilities(model);
  return (caps->lum_adjustment);
}

static const char *
canon_hue_adjustment(int model)
{
  const canon_cap_t * caps= canon_get_model_capabilities(model);
  return (caps->hue_adjustment);
}

static const char *
canon_sat_adjustment(int model)
{
  const canon_cap_t * caps= canon_get_model_capabilities(model);
  return (caps->sat_adjustment);
}

static double
canon_density(const canon_cap_t * caps, int res_code)
{
#ifndef EXPERIMENTAL_STUFF
  switch (res_code)
    {
    case 0x11: return caps->densities.d_r11;
    case 0x22: return caps->densities.d_r22;
    case 0x33: return caps->densities.d_r33;
    case 0x43: return caps->densities.d_r43;
    case 0x44: return caps->densities.d_r44;
    case 0x55: return caps->densities.d_r55;
    default:
      stp_deprintf(STP_DBG_CANON,"no such res_code 0x%x in density of model %d\n",
	      res_code,caps->model);
      return 0.2;
    }
#else
  return 0.2;
#endif
}

static const canon_variable_inkset_t *
canon_inks(const canon_cap_t * caps, int res_code, int colors, int bits)
{
#ifndef EXPERIMENTAL_STUFF
  const canon_variable_inklist_t *inks = caps->inxs;
  int i;

  if (!inks)
    return NULL;

  for (i=0; i<caps->inxs_cnt; i++) {
      stp_deprintf(STP_DBG_CANON,"hmm, trying ink for resolution code "
	      "%x, %d bits, %d colors\n",res_code,inks[i].bits,inks[i].colors);
    if ((inks[i].bits==bits) && (inks[i].colors==colors)) {
      stp_deprintf(STP_DBG_CANON,"wow, found ink for resolution code "
	      "%x, %d bits, %d colors\n",res_code,bits,colors);
      switch (res_code)
	{
	case 0x11: return inks[i].r11;
	case 0x22: return inks[i].r22;
	case 0x33: return inks[i].r33;
	case 0x43: return inks[i].r43;
	case 0x44: return inks[i].r44;
	case 0x55: return inks[i].r55;
	}
    }
  }
  stp_deprintf(STP_DBG_CANON,"ooo, found no ink for resolution code "
	  "%x, %d bits, %d colors in all %d defs!\n",
	  res_code,bits,colors,caps->inxs_cnt);
  return NULL;
#else
  return NULL;
#endif
}

static void
canon_describe_resolution(const stp_vars_t *v, int *x, int *y)
{
  const char *resolution = stp_get_string_parameter(v, "Resolution");
  const canon_res_t *res = canon_resolutions;
  while (res->x > 0)
    {
      if (strcmp(resolution, res->name) == 0 ||
	  strcmp(resolution, res->name_dmt) == 0)
	{
	  *x = res->x;
	  *y = res->y;
	  return;
	}
      res++;
    }
  *x = -1;
  *y = -1;
}

static const char *
canon_describe_output(const stp_vars_t *v)
{
  int model = stp_get_model_id(v);
  const canon_cap_t *caps = canon_get_model_capabilities(model);
  const char *print_mode = stp_get_string_parameter(v, "PrintingMode");
  const char *ink_type = stp_get_string_parameter(v, "InkType");
  colormode_t colormode = canon_printhead_colors(ink_type,caps);
  int printhead= canon_printhead_type(ink_type,caps);

  if ((print_mode && strcmp(print_mode, "BW") == 0) ||
      printhead == 0 || caps->inks == CANON_INK_K)
    colormode = COLOR_MONOCHROME;

  switch (colormode)
    {
    case COLOR_CMY:
      return "CMY";
    case COLOR_CMYK:
      return "CMYK";
    case COLOR_MONOCHROME:
    default:
      return "Grayscale";
    }
}

static const stp_param_string_t media_sources[] =
              {
                { "Auto",	N_ ("Auto Sheet Feeder") },
                { "Manual",	N_ ("Manual with Pause") },
                { "ManualNP",	N_ ("Manual without Pause") }
              };


/*
 * 'canon_parameters()' - Return the parameter values for the given parameter.
 */

static stp_parameter_list_t
canon_list_parameters(const stp_vars_t *v)
{
  stp_parameter_list_t *ret = stp_parameter_list_create();
  int i;
  for (i = 0; i < the_parameter_count; i++)
    stp_parameter_list_add_param(ret, &(the_parameters[i]));
  for (i = 0; i < float_parameter_count; i++)
    stp_parameter_list_add_param(ret, &(float_parameters[i].param));
  return ret;
}

static void
canon_parameters(const stp_vars_t *v, const char *name,
		 stp_parameter_t *description)
{
  int		i;

  const canon_cap_t * caps=
    canon_get_model_capabilities(stp_get_model_id(v));
  description->p_type = STP_PARAMETER_TYPE_INVALID;

  if (name == NULL)
    return;

  for (i = 0; i < float_parameter_count; i++)
    if (strcmp(name, float_parameters[i].param.name) == 0)
      {
	stp_fill_parameter_settings(description,
				     &(float_parameters[i].param));
	description->deflt.dbl = float_parameters[i].defval;
	description->bounds.dbl.upper = float_parameters[i].max;
	description->bounds.dbl.lower = float_parameters[i].min;
	return;
      }

  for (i = 0; i < the_parameter_count; i++)
    if (strcmp(name, the_parameters[i].name) == 0)
      {
	stp_fill_parameter_settings(description, &(the_parameters[i]));
	break;
      }
  if (strcmp(name, "PageSize") == 0)
  {
    int height_limit, width_limit;
    int papersizes = stp_known_papersizes();
    description->bounds.str = stp_string_list_create();

    width_limit = caps->max_width;
    height_limit = caps->max_height;

    for (i = 0; i < papersizes; i++) {
      const stp_papersize_t *pt = stp_get_papersize_by_index(i);
      if (strlen(pt->name) > 0 &&
	  pt->width <= width_limit && pt->height <= height_limit)
	{
	  if (stp_string_list_count(description->bounds.str) == 0)
	    description->deflt.str = pt->name;
	  stp_string_list_add_string(description->bounds.str,
				     pt->name, pt->text);
	}
    }
  }
  else if (strcmp(name, "Resolution") == 0)
  {
    int x,y;
    int t;
    description->bounds.str= stp_string_list_create();
    description->deflt.str = NULL;

    for (x=1; x<6; x++) {
      for (y=x-1; y<x+1; y++) {
	if ((t= canon_ink_type(caps,(x<<4)|y)) > -1) {
	  int xx = (1<<x)/2*caps->base_res;
	  int yy = (1<<y)/2*caps->base_res;
	  const canon_res_t *res = canon_resolutions;
	  while (res->x > 0) {
	    if (xx == res->x && yy == res->y) {
	      stp_string_list_add_string(description->bounds.str,
					res->name, _(res->text));
	      stp_deprintf(STP_DBG_CANON,"supports mode '%s'\n",
			   res->name);
	      if (xx >= 300 && yy >= 300 && description->deflt.str == NULL)
		description->deflt.str = res->name;
	      if (t == 1) {
		stp_string_list_add_string(description->bounds.str,
					  res->name_dmt, _(res->text_dmt));
		stp_deprintf(STP_DBG_CANON,"supports mode '%s'\n",
			     res->name_dmt);
	      }
	      break;
	    }
	    res++;
	  }
	}
      }
    }
  }
  else if (strcmp(name, "InkType") == 0)
  {
    description->bounds.str= stp_string_list_create();
    /* used internally: do not translate */
    if ((caps->inks & CANON_INK_K))
      stp_string_list_add_string(description->bounds.str,
			       "Gray", _("Black"));
    if ((caps->inks & CANON_INK_CMY))
      stp_string_list_add_string(description->bounds.str,
			       "RGB", _("CMY Color"));
    if ((caps->inks & CANON_INK_CMYK))
      stp_string_list_add_string(description->bounds.str,
			       "CMYK", _("CMYK Color"));
    if ((caps->inks & CANON_INK_CcMmYK))
      stp_string_list_add_string(description->bounds.str,
			       "PhotoCMY", _("Photo CcMmY Color"));
    if ((caps->inks & CANON_INK_CcMmYyK))
      stp_string_list_add_string(description->bounds.str,
			       "PhotoCMYK", _("Photo CcMmYK Color"));
    description->deflt.str =
      stp_string_list_param(description->bounds.str, 0)->name;
  }
  else if (strcmp(name, "InkChannels") == 0)
    {
      if (caps->inks & CANON_INK_CcMmYyK)
	description->deflt.integer = 7;
      else if (caps->inks & CANON_INK_CcMmYK)
	description->deflt.integer = 6;
      else if (caps->inks & CANON_INK_CMYK)
	description->deflt.integer = 4;
      else if (caps->inks & CANON_INK_CMY)
	description->deflt.integer = 3;
      else
	description->deflt.integer = 1;
      description->bounds.integer.lower = -1;
      description->bounds.integer.upper = -1;
    }
  else if (strcmp(name, "MediaType") == 0)
  {
    int count = sizeof(canon_paper_list) / sizeof(canon_paper_list[0]);
    description->bounds.str= stp_string_list_create();
    description->deflt.str= canon_paper_list[0].name;

    for (i = 0; i < count; i ++)
      stp_string_list_add_string(description->bounds.str,
				canon_paper_list[i].name,
				_(canon_paper_list[i].text));
  }
  else if (strcmp(name, "InputSlot") == 0)
  {
    int count = 3;
    description->bounds.str= stp_string_list_create();
    description->deflt.str= media_sources[0].name;
    for (i = 0; i < count; i ++)
      stp_string_list_add_string(description->bounds.str,
				media_sources[i].name,
				_(media_sources[i].text));
  }
  else if (strcmp(name, "PrintingMode") == 0)
  {
    description->bounds.str = stp_string_list_create();
    stp_string_list_add_string
      (description->bounds.str, "Color", _("Color"));
    stp_string_list_add_string
      (description->bounds.str, "BW", _("Black and White"));
    description->deflt.str =
      stp_string_list_param(description->bounds.str, 0)->name;
  }
}


/*
 * 'canon_imageable_area()' - Return the imageable area of the page.
 */

static void
internal_imageable_area(const stp_vars_t *v,   /* I */
			int  use_paper_margins,
			int  *left,	/* O - Left position in points */
			int  *right,	/* O - Right position in points */
			int  *bottom,	/* O - Bottom position in points */
			int  *top)	/* O - Top position in points */
{
  int	width, length;			/* Size of page */
  int left_margin = 0;
  int right_margin = 0;
  int bottom_margin = 0;
  int top_margin = 0;

  const canon_cap_t * caps= canon_get_model_capabilities(stp_get_model_id(v));
  const char *media_size = stp_get_string_parameter(v, "PageSize");
  const stp_papersize_t *pt = NULL;

  if (media_size && use_paper_margins)
    pt = stp_get_papersize_by_name(media_size);

  stp_default_media_size(v, &width, &length);
  if (pt)
    {
      left_margin = pt->left;
      right_margin = pt->right;
      bottom_margin = pt->bottom;
      top_margin = pt->top;
    }
  left_margin = MAX(left_margin, caps->border_left);
  right_margin = MAX(right_margin, caps->border_right);
  top_margin = MAX(top_margin, caps->border_top);
  bottom_margin = MAX(bottom_margin, caps->border_bottom);

  *left =	left_margin;
  *right =	width - right_margin;
  *top =	top_margin;
  *bottom =	length - bottom_margin;
}

static void
canon_imageable_area(const stp_vars_t *v,   /* I */
                     int  *left,	/* O - Left position in points */
                     int  *right,	/* O - Right position in points */
                     int  *bottom,	/* O - Bottom position in points */
                     int  *top)		/* O - Top position in points */
{
  internal_imageable_area(v, 1, left, right, bottom, top);
}

static void
canon_limit(const stp_vars_t *v,  		/* I */
	    int *width,
	    int *height,
	    int *min_width,
	    int *min_height)
{
  const canon_cap_t * caps=
    canon_get_model_capabilities(stp_get_model_id(v));
  *width =	caps->max_width;
  *height =	caps->max_height;
  *min_width = 1;
  *min_height = 1;
}

/*
 * 'canon_cmd()' - Sends a command with variable args
 */
static void
canon_cmd(const stp_vars_t *v, /* I - the printer         */
	  const char *ini, /* I - 2 bytes start code  */
	  const char cmd,  /* I - command code        */
	  int  num,  /* I - number of arguments */
	  ...        /* I - the args themselves */
	  )
{
  unsigned char *buffer = stp_zalloc(num + 1);
  int i;
  va_list ap;

  if (num)
    {
      va_start(ap, num);
      for (i=0; i<num; i++)
	buffer[i]= (unsigned char) va_arg(ap, int);
      va_end(ap);
    }

  stp_zfwrite(ini,2,1,v);
  if (cmd)
    {
      stp_putc(cmd,v);
      stp_put16_le(num, v);
      if (num)
	stp_zfwrite((const char *)buffer,num,1,v);
    }
  stp_free(buffer);
}

#define PUT(WHAT,VAL,RES) stp_deprintf(STP_DBG_CANON,"canon: "WHAT\
" is %04x =% 5d = %f\" = %f mm\n",(VAL),(VAL),(VAL)/(1.*RES),(VAL)/(RES/25.4))

#define ESC28 "\033\050"
#define ESC5b "\033\133"
#define ESC40 "\033\100"

/* ESC [K --  -- reset printer:
 */
static void
canon_init_resetPrinter(const stp_vars_t *v, canon_init_t *init)
{
  unsigned long f=init->caps->features;
  if (f & (CANON_CAP_ACKSHORT))
    {
      canon_cmd(v,ESC5b,0x4b, 2, 0x00,0x1f);
      stp_puts("BJLSTART\nControlMode=Common\n",v);
      if (f & CANON_CAP_ACKSHORT) stp_puts("AckTime=Short\n",v);
      stp_puts("BJLEND\n",v);
    }
  canon_cmd(v,ESC5b,0x4b, 2, 0x00,0x0f);
}

/* ESC (a -- 0x61 -- cmdSetPageMode --:
 */
static void
canon_init_setPageMode(const stp_vars_t *v, canon_init_t *init)
{
  if (!(init->caps->features & CANON_CAP_a))
    return;

  if (init->caps->features & CANON_CAP_a)
    canon_cmd(v,ESC28,0x61, 1, 0x01);
}

/* ESC (b -- 0x62 -- -- set data compression:
 */
static void
canon_init_setDataCompression(const stp_vars_t *v, canon_init_t *init)
{
  if (!(init->caps->features & CANON_CAP_b))
    return;

  canon_cmd(v,ESC28,0x62, 1, 0x01);
}

/* ESC (c -- 0x63 -- cmdSetColor --:
 */
static void
canon_init_setColor(const stp_vars_t *v, canon_init_t *init)
{
  unsigned char
    arg_63_1, arg_63_2, arg_63_3;


  if (!(init->caps->features & CANON_CAP_c))
    return;

  arg_63_1 = init->caps->model_id << 4;						/* MODEL_ID */

  switch ( init->caps->model_id ) {

  	case 0:			/* very old 360 dpi series: BJC-800/820 */
		break;		/*	tbd */

  	case 1:			/* 360 dpi series - BJC-4000, BJC-210, BJC-70 and their descendants */
		if (!init->printing_color)
    			arg_63_1|= 0x01;					/* PRINT_COLOUR */

  		arg_63_2 = ((init->pt ? init->pt->media_code : 0) << 4)		/* PRINT_MEDIA */
			+ 1;	/* hardcode to High quality for now */		/* PRINT_QUALITY */

  		canon_cmd(v,ESC28,0x63, 2, arg_63_1, arg_63_2);
		break;

	case 2:			/* are any models using this? */
		break;

	case 3:			/* 720 dpi series - BJC-3000 and descendants */
		if (!init->printing_color)
    			arg_63_1|= 0x01;					/* colour mode */

  		arg_63_2 = (init->pt) ? init->pt->media_code : 0;		/* print media type */

		arg_63_3 = 2;	/* hardcode to whatever this means for now */	/* quality, apparently */

  		canon_cmd(v,ESC28,0x63, 3, arg_63_1, arg_63_2, arg_63_3);
		break;
  	}

  return;
}

/* ESC (d -- 0x64 -- -- set raster resolution:
 */
static void
canon_init_setResolution(const stp_vars_t *v, canon_init_t *init)
{
  if (!(init->caps->features & CANON_CAP_d))
    return;

  canon_cmd(v,ESC28,0x64, 4,
	    (init->ydpi >> 8 ), (init->ydpi & 255),
	    (init->xdpi >> 8 ), (init->xdpi & 255));
}

/* ESC (g -- 0x67 -- cmdSetPageMargins --:
 */
static void
canon_init_setPageMargins(const stp_vars_t *v, canon_init_t *init)
{
  /* TOFIX: what exactly is to be sent?
   * Is it the printable length or the bottom border?
   * Is is the printable width or the right border?
   */

  int minlength= 0;
  int minwidth= 0;
  int length= init->page_height*5/36;
  int width= init->page_width*5/36;

  if (!(init->caps->features & CANON_CAP_g))
    return;

  if (minlength>length) length= minlength;
  if (minwidth>width) width= minwidth;

  canon_cmd(v,ESC28,0x67, 4, 0,
	    (unsigned char)(length),1,
	    (unsigned char)(width));

}

/* ESC (l -- 0x6c -- cmdSetTray --:
 */
static void
canon_init_setTray(const stp_vars_t *v, canon_init_t *init)
{
  unsigned char
    arg_6c_1 = 0x00,
    arg_6c_2 = 0x00; /* plain paper */

  /* int media= canon_media_type(media_str,caps); */
  int source= canon_source_type(init->source_str,init->caps);

  if (!(init->caps->features & CANON_CAP_l))
    return;

  arg_6c_1 = init->caps->model_id << 4;

  arg_6c_1|= (source & 0x0f);

  if (init->pt) arg_6c_2= init->pt->media_code;

  canon_cmd(v,ESC28,0x6c, 2, arg_6c_1, arg_6c_2);
}

/* ESC (m -- 0x6d --  -- :
 */
static void
canon_init_setPrintMode(const stp_vars_t *v, canon_init_t *init)
{
  unsigned char
    arg_6d_1 = 0x03, /* color printhead? */
    arg_6d_2 = 0x00, /* 00=color  02=b/w */
    arg_6d_3 = 0x00, /* only 01 for bjc8200 */
    arg_6d_a = 0x03, /* A4 paper */
    arg_6d_b = 0x00;

  if (!(init->caps->features & CANON_CAP_m))
    return;

  arg_6d_a= canon_size_type(v,init->caps);
  if (!arg_6d_a)
    arg_6d_b= 1;

  if (init->print_head==0)
    arg_6d_1= 0x03;
  else if (init->print_head<=2)
    arg_6d_1= 0x02;
  else if (init->print_head<=4)
    arg_6d_1= 0x04;
  if (!init->printing_color)
    arg_6d_2= 0x02;

  if (init->caps->model==8200)
    arg_6d_3= 0x01;

  canon_cmd(v,ESC28,0x6d,12, arg_6d_1,
	    0xff,0xff,0x00,0x00,0x07,0x00,
	    arg_6d_a,arg_6d_b,arg_6d_2,0x00,arg_6d_3);
}

/* ESC (p -- 0x70 -- cmdSetPageMargins2 --:
 */
static void
canon_init_setPageMargins2(const stp_vars_t *v, canon_init_t *init)
{
  /* TOFIX: what exactly is to be sent?
   * Is it the printable length or the bottom border?
   * Is is the printable width or the right border?
   */

  int printable_width=  init->page_width*5/6;
  int printable_length= init->page_height*5/6;

  unsigned char arg_70_1= (printable_length >> 8) & 0xff;
  unsigned char arg_70_2= (printable_length) & 0xff;
  unsigned char arg_70_3= (printable_width >> 8) & 0xff;
  unsigned char arg_70_4= (printable_width) & 0xff;

  if (!(init->caps->features & CANON_CAP_p))
    return;

  canon_cmd(v,ESC28,0x70, 8,
	    arg_70_1, arg_70_2, 0x00, 0x00,
	    arg_70_3, arg_70_4, 0x00, 0x00);
}

/* ESC (q -- 0x71 -- setPageID -- :
 */
static void
canon_init_setPageID(const stp_vars_t *v, canon_init_t *init)
{
  if (!(init->caps->features & CANON_CAP_q))
    return;

  canon_cmd(v,ESC28,0x71, 1, 0x01);
}

/* ESC (r -- 0x72 --  -- :
 */
static void
canon_init_setX72(const stp_vars_t *v, canon_init_t *init)
{
  if (!(init->caps->features & CANON_CAP_r))
    return;

  canon_cmd(v,ESC28,0x72, 1, 0x61); /* whatever for - 8200 needs it */
}

/* ESC (t -- 0x74 -- cmdSetImage --:
 */
static void
canon_init_setImage(const stp_vars_t *v, canon_init_t *init)
{
  unsigned char
    arg_74_1 = 0x01, /* 1 bit per pixel */
    arg_74_2 = 0x00, /*  */
    arg_74_3 = 0x01; /* 01 <= 360 dpi    09 >= 720 dpi */

  if (!(init->caps->features & CANON_CAP_t))
    return;

  if (init->xdpi==1440) arg_74_2= 0x04;
  if (init->ydpi>=720)  arg_74_3= 0x09;

  if (init->bits>1) {
    arg_74_1= 0x02;
    arg_74_2= 0x80;
    arg_74_3= 0x09;
    if (init->colormode == COLOR_CMY) arg_74_3= 0x02; /* for BC-06 cartridge!!! */
  }

  /* workaround for the bjc8200 in 6color mode - not really understood */
  if (init->caps->model==8200) {
    if (init->colormode==COLOR_CCMMYK) {
      arg_74_1= 0xff;
      arg_74_2= 0x90;
      arg_74_3= 0x04;
      init->bits=3;
      if (init->ydpi>600)  arg_74_3= 0x09;
    } else {
      arg_74_1= 0x01;
      arg_74_2= 0x00;
      arg_74_3= 0x01;
      if (init->ydpi>600)  arg_74_3= 0x09;
    }
  }

  canon_cmd(v,ESC28,0x74, 3, arg_74_1, arg_74_2, arg_74_3);
}

static void
canon_init_printer(const stp_vars_t *v, canon_init_t *init)
{
  int mytop;
  /* init printer */

  canon_init_resetPrinter(v,init);       /* ESC [K */
  canon_init_setPageMode(v,init);        /* ESC (a */
  canon_init_setDataCompression(v,init); /* ESC (b */
  canon_init_setPageID(v,init);          /* ESC (q */
  canon_init_setPrintMode(v,init);       /* ESC (m */
  canon_init_setResolution(v,init);      /* ESC (d */
  canon_init_setImage(v,init);           /* ESC (t */
  canon_init_setColor(v,init);           /* ESC (c */
  canon_init_setPageMargins(v,init);     /* ESC (g */
  canon_init_setPageMargins2(v,init);    /* ESC (p */
  canon_init_setTray(v,init);            /* ESC (l */
  canon_init_setX72(v,init);             /* ESC (r */

  /* some linefeeds */

  mytop= (init->top*init->ydpi)/72;
  canon_cmd(v,ESC28,0x65, 2, (mytop >> 8 ),(mytop & 255));
}

static void
canon_deinit_printer(const stp_vars_t *v, canon_init_t *init)
{
  /* eject page */
  stp_putc(0x0c,v);

  /* say goodbye */
  canon_cmd(v,ESC28,0x62,1,0);
  if (init->caps->features & CANON_CAP_a)
    canon_cmd(v,ESC28,0x61, 1, 0);
}

static int
canon_start_job(const stp_vars_t *v, stp_image_t *image)
{
  return 1;
}

static int
canon_end_job(const stp_vars_t *v, stp_image_t *image)
{
  canon_cmd(v,ESC40,0,0);
  return 1;
}

/*
 * 'advance_buffer()' - Move (num) lines of length (len) down one line
 *                      and sets first line to 0s
 *                      accepts NULL pointers as buf
 *                  !!! buf must contain more than (num) lines !!!
 *                      also sets first line to 0s if num<1
 */
static void
canon_advance_buffer(unsigned char *buf, int len, int num)
{
  if (!buf || !len) return;
  if (num>0) memmove(buf+len,buf,len*num);
  memset(buf,0,len);
}

static void
setup_column(canon_privdata_t *privdata, int col, int buf_length)
{
  privdata->cols[col] = stp_zalloc(buf_length * (privdata->delay[col] + 1));
}

static void
canon_printfunc(stp_vars_t *v)
{
  int i;
  canon_privdata_t *pd = (canon_privdata_t *) stp_get_component_data(v, "Driver");
  canon_write_line(v);
  for (i = 0; i < 7; i++)
    canon_advance_buffer(pd->cols[i], pd->buf_length, pd->delay[i]);

}

static double
get_double_param(stp_vars_t *v, const char *param)
{
  if (param && stp_check_float_parameter(v, param, STP_PARAMETER_ACTIVE))
    return stp_get_float_parameter(v, param);
  else
    return 1.0;
}

static void
set_ink_ranges(stp_vars_t *v, const canon_variable_ink_t *ink, int color,
	       const char *channel_param, const char *subchannel_param)
{
  if (!ink)
    return;
  stp_dither_set_inks_full(v, color, ink->numshades, ink->shades, 1.0,
			   ink_darknesses[color]);
  stp_channel_set_density_adjustment
    (v, color, 1, (get_double_param(v, channel_param) *
		   get_double_param(v, subchannel_param) *
		   get_double_param(v, "Density")));
}

/*
 * 'canon_print()' - Print an image to a CANON printer.
 */
static int
canon_do_print(stp_vars_t *v, stp_image_t *image)
{
  int i;
  int		status = 1;
  int		model = stp_get_model_id(v);
  const char	*resolution = stp_get_string_parameter(v, "Resolution");
  const char	*media_source = stp_get_string_parameter(v, "InputSlot");
  const char    *print_mode = stp_get_string_parameter(v, "PrintingMode");
  int printing_color = 0;
  const char	*ink_type = stp_get_string_parameter(v, "InkType");
  int		top = stp_get_top(v);
  int		left = stp_get_left(v);
  int		y;		/* Looping vars */
  int		xdpi, ydpi;	/* Resolution */
  int		n;		/* Output number */
  canon_privdata_t privdata;
  int		page_width,	/* Width of page */
		page_height,	/* Length of page */
		page_left,
		page_top,
		page_right,
		page_bottom,
		page_true_height,	/* True length of page */
		out_width,	/* Width of image on page */
		out_height,	/* Length of image on page */
		out_channels,	/* Output bytes per pixel */
		length,		/* Length of raster data */
		errdiv,		/* Error dividend */
		errmod,		/* Error modulus */
		errval,		/* Current error value */
		errline,	/* Current raster line */
		errlast;	/* Last raster line loaded */
  unsigned	zero_mask;
  int           bits= 1;
  int           image_height,
                image_width;
  int           res_code;
  int           use_6color= 0;
  double        k_upper, k_lower;
  stp_curve_t *lum_adjustment = NULL;
  stp_curve_t *hue_adjustment = NULL;
  stp_curve_t *sat_adjustment = NULL;

  canon_init_t  init;
  const canon_cap_t * caps= canon_get_model_capabilities(model);
  int printhead= canon_printhead_type(ink_type,caps);
  colormode_t colormode = canon_printhead_colors(ink_type,caps);
  const paper_t *pt;
  const canon_variable_inkset_t *inks;
  const canon_res_t *res = canon_resolutions;

  if (!stp_verify(v))
    {
      stp_eprintf(v, "Print options not verified; cannot print.\n");
      return 0;
    }
  if (strcmp(print_mode, "Color") == 0)
    printing_color = 1;

  PUT("top        ",top,72);
  PUT("left       ",left,72);

  /*
  * Setup a read-only pixel region for the entire image...
  */

  stp_image_init(image);

  /* force grayscale if image is grayscale
   *                 or single black cartridge installed
   */

  if (printhead == 0 || caps->inks == CANON_INK_K)
    {
      printing_color = 0;
      stp_set_string_parameter(v, "PrintingMode", "BW");
    }

  if (!printing_color)
    colormode = COLOR_MONOCHROME;

 /*
  * Figure out the output resolution...
  */

  xdpi = -1;
  ydpi = -1;
  while (res->x > 0) {
    if (strcmp(resolution, res->name) == 0 ||
	strcmp(resolution, res->name_dmt) == 0)
      {
	xdpi = res->x;
	ydpi = res->y;
	break;
      }
    res++;
  }

  stp_deprintf(STP_DBG_CANON,"canon: resolution=%dx%d\n",xdpi,ydpi);
  stp_deprintf(STP_DBG_CANON,"       rescode   =0x%x\n",canon_res_code(caps,xdpi,ydpi));
  res_code= canon_res_code(caps,xdpi,ydpi);

  if (strcmp(resolution, res->name_dmt) == 0 &&
      (caps->features & CANON_CAP_DMT)) {
    bits= 2;
    stp_deprintf(STP_DBG_CANON,"canon: using drop modulation technology\n");
  }

 /*
  * Compute the output size...
  */

  out_width = stp_get_width(v);
  out_height = stp_get_height(v);

  internal_imageable_area(v, 0, &page_left, &page_right,
			  &page_bottom, &page_top);
  left -= page_left;
  top -= page_top;
  page_width = page_right - page_left;
  page_height = page_bottom - page_top;

  image_height = stp_image_height(image);
  image_width = stp_image_width(image);

  stp_default_media_size(v, &n, &page_true_height);

  PUT("top        ",top,72);
  PUT("left       ",left,72);
  PUT("page_true_height",page_true_height,72);
  PUT("out_width ", out_width,xdpi);
  PUT("out_height", out_height,ydpi);

  PUT("top     ",top,72);
  PUT("left    ",left,72);

  pt = get_media_type(stp_get_string_parameter(v, "MediaType"));

  init.caps = caps;
  init.printing_color = printing_color;
  init.pt = pt;
  init.print_head = printhead;
  init.colormode = colormode;
  init.source_str = media_source;
  init.xdpi = xdpi;
  init.ydpi = ydpi;
  init.page_width = page_width;
  init.page_height = page_height;
  init.top = top;
  init.left = left;
  init.bits = bits;

  canon_init_printer(v, &init);

  /* possibly changed during initialitation
   * to enforce valid modes of operation:
   */
  bits= init.bits;
  xdpi= init.xdpi;
  ydpi= init.ydpi;

 /*
  * Convert image size to printer resolution...
  */

  out_width  = xdpi * out_width / 72;
  out_height = ydpi * out_height / 72;

  PUT("out_width ", out_width,xdpi);
  PUT("out_height", out_height,ydpi);

  left = xdpi * left / 72;

  PUT("leftskip",left,xdpi);

  for (i = 0; i < 7; i++)
    privdata.cols[i] = NULL;

  if(xdpi==1440){
    privdata.delay[0] = 0;
    privdata.delay[1] = 112;
    privdata.delay[2] = 224;
    privdata.delay[3] = 336;
    privdata.delay[4] = 112;
    privdata.delay[5] = 224;
    privdata.delay[6] = 336;
    privdata.delay_max = 336;
    stp_deprintf(STP_DBG_CANON,"canon: delay on!\n");
  } else {
    for (i = 0; i < 7; i++)
      privdata.delay[i] = 0;
    privdata.delay_max = 0;
    stp_deprintf(STP_DBG_CANON,"canon: delay off!\n");
  }

 /*
  * Allocate memory for the raster data...
  */

  length = (out_width + 7) / 8;

  privdata.buf_length= length*bits;
  privdata.left = left;
  privdata.bits = bits;
  privdata.out_width = out_width;
  privdata.caps = caps;
  privdata.ydpi = ydpi;

  stp_deprintf(STP_DBG_CANON,"canon: buflength is %d!\n",privdata.buf_length);

  if (colormode==COLOR_MONOCHROME) {
    setup_column(&privdata, 0, privdata.buf_length);
  } else {
    setup_column(&privdata, 1, privdata.buf_length);
    setup_column(&privdata, 2, privdata.buf_length);
    setup_column(&privdata, 3, privdata.buf_length);

    if (colormode!=COLOR_CMY)
      setup_column(&privdata, 0, privdata.buf_length);

    if (colormode==COLOR_CCMMYK || colormode==COLOR_CCMMYYK) {
      use_6color= 1;
      setup_column(&privdata, 4, privdata.buf_length);
      setup_column(&privdata, 5, privdata.buf_length);
      if (colormode==CANON_INK_CcMmYyK)
	setup_column(&privdata, 6, privdata.buf_length);
    }
  }

  if (privdata.cols[0])
    {
      if (privdata.cols[1])
	stp_set_string_parameter(v, "STPIOutputType", "KCMY");
      else
	stp_set_string_parameter(v, "STPIOutputType", "Grayscale");
    }
  else
    stp_set_string_parameter(v, "STPIOutputType", "CMY");

  stp_deprintf(STP_DBG_CANON,
	       "canon: driver will use colors %s%s%s%s%s%s%s\n",
	       privdata.cols[0] ? "K" : "",
	       privdata.cols[1] ? "C" : "",
	       privdata.cols[2] ? "M" : "",
	       privdata.cols[3] ? "Y" : "",
	       privdata.cols[4] ? "c" : "",
	       privdata.cols[5] ? "m" : "",
	       privdata.cols[6] ? "y" : "");

  stp_deprintf(STP_DBG_CANON,"density is %f\n",
	       stp_get_float_parameter(v, "Density"));

  /*
   * Compute the LUT.  For now, it's 8 bit, but that may eventually
   * sometimes change.
   */

  if (!stp_check_float_parameter(v, "Density", STP_PARAMETER_DEFAULTED))
    {
      stp_set_float_parameter_active(v, "Density", STP_PARAMETER_ACTIVE);
      stp_set_float_parameter(v, "Density", 1.0);
    }
  if (pt)
    stp_scale_float_parameter(v, "Density", pt->base_density);
  else			/* Can't find paper type? Assume plain */
    stp_scale_float_parameter(v, "Density", .5);
  stp_scale_float_parameter(v, "Density", canon_density(caps, res_code));
  if (stp_get_float_parameter(v, "Density") > 1.0)
    stp_set_float_parameter(v, "Density", 1.0);
  if (colormode == COLOR_MONOCHROME)
    stp_scale_float_parameter(v, "Gamma", 1.25);

  stp_deprintf(STP_DBG_CANON,"density is %f\n",
	       stp_get_float_parameter(v, "Density"));

 /*
  * Output the page...
  */


  if (use_6color)
    k_lower = .4 / bits + .1;
  else
    k_lower = .25 / bits;
  if (pt)
    {
      k_lower *= pt->k_lower_scale;
      k_upper = pt->k_upper;
    }
  else
    {
      k_lower *= .5;
      k_upper = .5;
    }
  if (!stp_check_float_parameter(v, "GCRLower", STP_PARAMETER_ACTIVE))
    stp_set_default_float_parameter(v, "GCRLower", k_lower);
  if (!stp_check_float_parameter(v, "GCRUpper", STP_PARAMETER_ACTIVE))
    stp_set_default_float_parameter(v, "GCRUpper", k_upper);
  stp_dither_init(v, image, out_width, xdpi, ydpi);

  for (i = 0; i < 7; i++)
    {
      if (privdata.cols[i])
	stp_dither_add_channel(v, privdata.cols[i], channel_color_map[i],
			       subchannel_color_map[i]);
    }

  if ((inks = canon_inks(caps, res_code, colormode, bits))!=0)
    {
      set_ink_ranges(v, inks->c, STP_ECOLOR_C, "MagentaDensity",
		     "LightCyanTransition");
      set_ink_ranges(v, inks->m, STP_ECOLOR_M, "MagentaDensity",
		     "LightMagentaTransition");
      set_ink_ranges(v, inks->y, STP_ECOLOR_Y, "YellowDensity",
		     "LightYellowTransition");
      set_ink_ranges(v, inks->k, STP_ECOLOR_K, "BlackDensity", NULL);
    }
  stp_channel_set_density_adjustment
    (v, STP_ECOLOR_C, 0,
     get_double_param(v, "CyanDensity") * get_double_param(v, "Density"));
  stp_channel_set_density_adjustment
    (v, STP_ECOLOR_M, 0,
     get_double_param(v, "MagentaDensity") * get_double_param(v, "Density"));
  stp_channel_set_density_adjustment
    (v, STP_ECOLOR_Y, 0,
     get_double_param(v, "YellowDensity") * get_double_param(v, "Density"));
  stp_channel_set_density_adjustment
    (v, STP_ECOLOR_K, 0,
     get_double_param(v, "BlackDensity") * get_double_param(v, "Density"));

  errdiv  = image_height / out_height;
  errmod  = image_height % out_height;
  errval  = 0;
  errlast = -1;
  errline  = 0;

  if (!stp_check_curve_parameter(v, "HueMap", STP_PARAMETER_ACTIVE) &&
      pt->hue_adjustment)
    {
      hue_adjustment = stp_read_and_compose_curves
	(canon_hue_adjustment(model),
	 pt ? pt->hue_adjustment : NULL, STP_CURVE_COMPOSE_ADD, 384);
      stp_set_curve_parameter(v, "HueMap", hue_adjustment);
      stp_curve_destroy(hue_adjustment);
    }
  if (!stp_check_curve_parameter(v, "LumMap", STP_PARAMETER_ACTIVE) &&
      pt->lum_adjustment)
    {
      lum_adjustment = stp_read_and_compose_curves
	(canon_lum_adjustment(model),
	 pt ? pt->lum_adjustment : NULL, STP_CURVE_COMPOSE_MULTIPLY, 384);
      stp_set_curve_parameter(v, "LumMap", lum_adjustment);
      stp_curve_destroy(lum_adjustment);
    }
  if (!stp_check_curve_parameter(v, "SatMap", STP_PARAMETER_ACTIVE) &&
      pt->sat_adjustment)
    {
      sat_adjustment = stp_read_and_compose_curves
	(canon_sat_adjustment(model),
	 pt ? pt->sat_adjustment : NULL, STP_CURVE_COMPOSE_MULTIPLY, 384);
      stp_set_curve_parameter(v, "SatMap", sat_adjustment);
      stp_curve_destroy(sat_adjustment);
    }

  out_channels = stp_color_init(v, image, 65536);
  stp_allocate_component_data(v, "Driver", NULL, NULL, &privdata);

  privdata.emptylines = 0;
  for (y = 0; y < out_height; y ++)
  {
    int duplicate_line = 1;

    if (errline != errlast)
    {
      errlast = errline;
      duplicate_line = 0;
      if (stp_color_get_row(v, image, errline, &zero_mask))
	{
	  status = 2;
	  break;
	}
    }

    stp_dither(v, y, duplicate_line, zero_mask, NULL);
    canon_printfunc(v);
    errval += errmod;
    errline += errdiv;
    if (errval >= out_height)
    {
      errval -= out_height;
      errline ++;
    }
  }

  /*
   * Flush delayed buffers...
   */

  if (privdata.delay_max) {
    stp_deprintf(STP_DBG_CANON,"\ncanon: flushing %d possibly delayed buffers\n",
		 privdata.delay_max);
    for (y= 0; y<privdata.delay_max; y++) {

      canon_write_line(v);
      for (i = 0; i < 7; i++)
	canon_advance_buffer(privdata.cols[i], privdata.buf_length,
			     privdata.delay[i]);
    }
  }

  stp_image_conclude(image);

 /*
  * Cleanup...
  */

  for (i = 0; i < 7; i++)
    if (privdata.cols[i])
      stp_free(privdata.cols[i]);

  canon_deinit_printer(v, &init);
  return status;
}

static int
canon_print(const stp_vars_t *v, stp_image_t *image)
{
  int status;
  stp_vars_t *nv = stp_vars_create_copy(v);
  stp_prune_inactive_options(nv);
  status = canon_do_print(nv, image);
  stp_vars_destroy(nv);
  return status;
}

static const stp_printfuncs_t print_canon_printfuncs =
{
  canon_list_parameters,
  canon_parameters,
  stp_default_media_size,
  canon_imageable_area,
  canon_limit,
  canon_print,
  canon_describe_resolution,
  canon_describe_output,
  stp_verify_printer_params,
  canon_start_job,
  canon_end_job
};

/*
 * 'canon_fold_lsb_msb()' fold 2 lines in order lsb/msb
 */

static void
canon_fold_2bit(const unsigned char *line,
		int single_length,
		unsigned char *outbuf)
{
  int i;
  for (i = 0; i < single_length; i++) {
    outbuf[0] =
      ((line[0] & (1 << 7)) >> 1) |
      ((line[0] & (1 << 6)) >> 2) |
      ((line[0] & (1 << 5)) >> 3) |
      ((line[0] & (1 << 4)) >> 4) |
      ((line[single_length] & (1 << 7)) >> 0) |
      ((line[single_length] & (1 << 6)) >> 1) |
      ((line[single_length] & (1 << 5)) >> 2) |
      ((line[single_length] & (1 << 4)) >> 3);
    outbuf[1] =
      ((line[0] & (1 << 3)) << 3) |
      ((line[0] & (1 << 2)) << 2) |
      ((line[0] & (1 << 1)) << 1) |
      ((line[0] & (1 << 0)) << 0) |
      ((line[single_length] & (1 << 3)) << 4) |
      ((line[single_length] & (1 << 2)) << 3) |
      ((line[single_length] & (1 << 1)) << 2) |
      ((line[single_length] & (1 << 0)) << 1);
    line++;
    outbuf += 2;
  }
}

#ifndef USE_3BIT_FOLD_TYPE
#error YOU MUST CHOOSE A VALUE FOR USE_3BIT_FOLD_TYPE
#endif

#if USE_3BIT_FOLD_TYPE == 333

static void
canon_fold_3bit(const unsigned char *line,
		int single_length,
		unsigned char *outbuf)
{
  int i;
  for (i = 0; i < single_length; i++) {
    outbuf[0] =
      ((line[0] & (1 << 7)) >> 2) |
      ((line[0] & (1 << 6)) >> 4) |
      ((line[single_length] & (1 << 7)) >> 1) |
      ((line[single_length] & (1 << 6)) >> 3) |
      ((line[single_length] & (1 << 5)) >> 5) |
      ((line[2*single_length] & (1 << 7)) << 0) |
      ((line[2*single_length] & (1 << 6)) >> 2) |
      ((line[2*single_length] & (1 << 5)) >> 4);
    outbuf[1] =
      ((line[0] & (1 << 5)) << 2) |
      ((line[0] & (1 << 4)) << 0) |
      ((line[0] & (1 << 3)) >> 2) |
      ((line[single_length] & (1 << 4)) << 1) |
      ((line[single_length] & (1 << 3)) >> 1) |
      ((line[2*single_length] & (1 << 4)) << 2) |
      ((line[2*single_length] & (1 << 3)) << 0) |
      ((line[2*single_length] & (1 << 2)) >> 2);
    outbuf[2] =
      ((line[0] & (1 << 2)) << 4) |
      ((line[0] & (1 << 1)) << 2) |
      ((line[0] & (1 << 0)) << 0) |
      ((line[single_length] & (1 << 2)) << 5) |
      ((line[single_length] & (1 << 1)) << 3) |
      ((line[single_length] & (1 << 0)) << 1) |
      ((line[2*single_length] & (1 << 1)) << 4) |
      ((line[2*single_length] & (1 << 0)) << 2);
    line++;
    outbuf += 3;
  }
}

#elif USE_3BIT_FOLD_TYPE == 323

static void
canon_fold_3bit(const unsigned char *line,
		int single_length,
		unsigned char *outbuf)
{
  unsigned char A0,A1,A2,B0,B1,B2,C0,C1,C2;
  const unsigned char *last= line+single_length;

  for (; line < last; line+=3, outbuf+=8) {

    A0= line[0]; B0= line[single_length]; C0= line[2*single_length];

    if (line<last-2) {
      A1= line[1]; B1= line[single_length+1]; C1= line[2*single_length+1];
    } else {
      A1= 0; B1= 0; C1= 0;
    }
    if (line<last-1) {
      A2= line[2]; B2= line[single_length+2]; C2= line[2*single_length+2];
    } else {
      A2= 0; B2= 0; C2= 0;
    }

    outbuf[0] =
      ((C0 & 0x80) >> 0) |
      ((B0 & 0x80) >> 1) |
      ((A0 & 0x80) >> 2) |
      ((B0 & 0x40) >> 2) |
      ((A0 & 0x40) >> 3) |
      ((C0 & 0x20) >> 3) |
      ((B0 & 0x20) >> 4) |
      ((A0 & 0x20) >> 5);
    outbuf[1] =
      ((C0 & 0x10) << 3) |
      ((B0 & 0x10) << 2) |
      ((A0 & 0x10) << 1) |
      ((B0 & 0x08) << 1) |
      ((A0 & 0x08) << 0) |
      ((C0 & 0x04) >> 0) |
      ((B0 & 0x04) >> 1) |
      ((A0 & 0x04) >> 2);
    outbuf[2] =
      ((C0 & 0x02) << 6) |
      ((B0 & 0x02) << 5) |
      ((A0 & 0x02) << 4) |
      ((B0 & 0x01) << 4) |
      ((A0 & 0x01) << 3) |
      ((C1 & 0x80) >> 5) |
      ((B1 & 0x80) >> 6) |
      ((A1 & 0x80) >> 7);
    outbuf[3] =
      ((C1 & 0x40) << 1) |
      ((B1 & 0x40) << 0) |
      ((A1 & 0x40) >> 1) |
      ((B1 & 0x20) >> 1) |
      ((A1 & 0x20) >> 2) |
      ((C1 & 0x10) >> 2) |
      ((B1 & 0x10) >> 3) |
      ((A1 & 0x10) >> 4);
    outbuf[4] =
      ((C1 & 0x08) << 4) |
      ((B1 & 0x08) << 3) |
      ((A1 & 0x08) << 2) |
      ((B1 & 0x04) << 2) |
      ((A1 & 0x04) << 1) |
      ((C1 & 0x02) << 1) |
      ((B1 & 0x02) >> 0) |
      ((A1 & 0x02) >> 1);
    outbuf[5] =
      ((C1 & 0x01) << 7) |
      ((B1 & 0x01) << 6) |
      ((A1 & 0x01) << 5) |
      ((B2 & 0x80) >> 3) |
      ((A2 & 0x80) >> 4) |
      ((C2 & 0x40) >> 4) |
      ((B2 & 0x40) >> 5) |
      ((A2 & 0x40) >> 6);
    outbuf[6] =
      ((C2 & 0x20) << 2) |
      ((B2 & 0x20) << 1) |
      ((A2 & 0x20) << 0) |
      ((B2 & 0x10) >> 0) |
      ((A2 & 0x10) >> 1) |
      ((C2 & 0x08) >> 1) |
      ((B2 & 0x08) >> 2) |
      ((A2 & 0x08) >> 3);
    outbuf[7] =
      ((C2 & 0x04) << 5) |
      ((B2 & 0x04) << 4) |
      ((A2 & 0x04) << 3) |
      ((B2 & 0x02) << 3) |
      ((A2 & 0x02) << 2) |
      ((C2 & 0x01) << 2) |
      ((B2 & 0x01) << 1) |
      ((A2 & 0x01) << 0);
  }
}

#else
#error 3BIT FOLD TYPE NOT IMPLEMENTED
#endif

static void
canon_shift_buffer(unsigned char *line,int length,int bits)
{
  int i,j;
  for (j=0; j<bits; j++) {
    for (i=length-1; i>0; i--) {
      line[i]= (line[i] >> 1) | (line[i-1] << 7);
    }
    line[0] = line[0] >> 1;
  }
}

#if 0
static void
canon_shift_buffer2(unsigned char *line,int length,int bits)
{
  int i;
  for (i=length-1; i>0; i--) {
    line[i]= (line[i] >> bits) | (line[i-1] << (8-bits));
  }
  line[0] = line[0] >> bits;
}
#endif

/*
 * 'canon_write()' - Send graphics using TIFF packbits compression.
 */

static int
canon_write(stp_vars_t *v,		/* I - Print file or command */
	    const canon_cap_t *   caps,	        /* I - Printer model */
	    unsigned char *line,	/* I - Output bitmap data */
	    int           length,	/* I - Length of bitmap data */
	    int           coloridx,	/* I - Which color */
	    int           ydpi,		/* I - Vertical resolution */
	    int           *empty,       /* IO- Preceeding empty lines */
	    int           width,	/* I - Printed width */
	    int           offset, 	/* I - Offset from left side */
	    int           bits)
{
  unsigned char
    comp_buf[COMPBUFWIDTH + COMPBUFWIDTH / 4],	/* Compression buffer */
    in_fold[COMPBUFWIDTH],
    *in_ptr= line,
    *comp_ptr, *comp_data;
  int newlength;
  int offset2,bitoffset;
  unsigned char color;

 /* Don't send blank lines... */

  if (line[0] == 0 && memcmp(line, line + 1, length - 1) == 0)
    return 0;

  /* fold lsb/msb pairs if drop modulation is active */



  if (bits==2) {
    memset(in_fold,0,length);
    canon_fold_2bit(line,length,in_fold);
    in_ptr= in_fold;
    length= (length*8/4); /* 4 pixels in 8bit */
    offset= (offset*8/4); /* 4 pixels in 8bit  */
  }
  if (bits==3) {
    memset(in_fold,0,length);
    canon_fold_3bit(line,length,in_fold);
    in_ptr= in_fold;
    length= (length*8)/3;
    offset= (offset/3)*8;
#if 0
    switch(offset%3){
    case 0: offset= (offset/3)*8;   break;
    case 1: offset= (offset/3)*8/*+3 CAREFUL! CANNOT SHIFT _AFTER_ RECODING!!*/; break;
    case 2: offset= (offset/3)*8/*+5 CAREFUL! CANNOT SHIFT _AFTER_ RECODING!!*/; break;
    }
#endif
  }
  /* pack left border rounded to multiples of 8 dots */

  comp_data= comp_buf;
  offset2= offset/8;
  bitoffset= offset%8;
  while (offset2>0) {
    unsigned char toffset = offset2 > 128 ? 128 : offset2;
    comp_data[0] = 1 - toffset;
    comp_data[1] = 0;
    comp_data += 2;
    offset2-= toffset;
  }
  if (bitoffset) {
    if (bitoffset<8)
      canon_shift_buffer(in_ptr,length,bitoffset);
    else
      stp_deprintf(STP_DBG_CANON,"SEVERE BUG IN print-canon.c::canon_write() "
	      "bitoffset=%d!!\n",bitoffset);
  }

  stp_pack_tiff(v, in_ptr, length, comp_data, &comp_ptr, NULL, NULL);
  newlength= comp_ptr - comp_buf;

  /* send packed empty lines if any */

  if (*empty) {
    stp_zfwrite("\033\050\145\002\000", 5, 1, v);
    stp_put16_be(*empty, v);
    *empty= 0;
  }

 /* Send a line of raster graphics... */

  stp_zfwrite("\033\050\101", 3, 1, v);
  stp_put16_le(newlength + 1, v);
  color= "CMYKcmy"[coloridx];
  if (!color) color= 'K';
  stp_putc(color,v);
  stp_zfwrite((const char *)comp_buf, newlength, 1, v);
  stp_putc('\015', v);
  return 1;
}


static void
canon_write_line(stp_vars_t *v)
{
  canon_privdata_t *pd =
    (canon_privdata_t *) stp_get_component_data(v, "Driver");
  static const int write_sequence[] = { 0, 3, 2, 1, 6, 5, 4 };
  static const int write_number[] = { 3, 2, 1, 0, 6, 5, 4 };
  int i;
  int written= 0;

  for (i = 0; i < 7; i++)
    {
      int col = write_sequence[i];
      int num = write_number[i];
      if (pd->cols[col])
	written += canon_write(v, pd->caps,
			       pd->cols[col] + pd->delay[col] * pd->buf_length,
			       pd->buf_length / pd->bits, num, pd->ydpi,
			       &(pd->emptylines), pd->out_width,
			       pd->left, pd->bits);
    }
  if (written)
    stp_zfwrite("\033\050\145\002\000\000\001", 7, 1, v);
  else
    pd->emptylines += 1;
}


static stp_family_t print_canon_module_data =
  {
    &print_canon_printfuncs,
    NULL
  };


static int
print_canon_module_init(void)
{
  return stp_family_register(print_canon_module_data.printer_list);
}


static int
print_canon_module_exit(void)
{
  return stp_family_unregister(print_canon_module_data.printer_list);
}


/* Module header */
#define stp_module_version print_canon_LTX_stp_module_version
#define stp_module_data print_canon_LTX_stp_module_data

stp_module_version_t stp_module_version = {0, 0};

stp_module_t stp_module_data =
  {
    "canon",
    VERSION,
    "Canon family driver",
    STP_MODULE_CLASS_FAMILY,
    NULL,
    print_canon_module_init,
    print_canon_module_exit,
    (void *) &print_canon_module_data
  };


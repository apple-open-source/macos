/*
 * "$Id: print-canon.c,v 1.1.1.1 2003/01/27 19:05:32 jlovell Exp $"
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
#define BITS_PER_BYTE 8
#define COMPBUFWIDTH (MAX_PHYSICAL_BPI * MAX_OVERSAMPLED * MAX_BPP * \
	MAX_CARRIAGE_WIDTH / BITS_PER_BYTE)

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
  const stp_simple_dither_range_t *range;
  int count;
  double density;
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
  const double *lum_adjustment;        /* optional lum adj.     */
  const double *hue_adjustment;        /* optional hue adj.     */
  const double *sat_adjustment;        /* optional sat adj.     */
} canon_variable_printmode_t;
#endif

/* NOTE  NOTE  NOTE  NOTE  NOTE  NOTE  NOTE  NOTE  NOTE  NOTE  NOTE  NOTE
 *
 * The following dither ranges were taken from print-escp2.c and do NOT
 * represent the requirements of canon inks. Feel free to play with them
 * accoring to the escp2 part of doc/README.new-printer and send me a patch
 * if you get better results. Please send mail to thaller@ph.tum.de
 */

/*
 * Dither ranges specifically for Cyan/LightCyan (see NOTE above)
 *
 */
static const stp_simple_dither_range_t canon_dither_ranges_Cc_1bit[] =
{
  { 0.25, 0x1, 1, 1 },
  { 1.0,  0x1, 0, 1 }
};

static const canon_variable_ink_t canon_ink_Cc_1bit =
{
  canon_dither_ranges_Cc_1bit,
  sizeof(canon_dither_ranges_Cc_1bit) / sizeof(stp_simple_dither_range_t),
  .75
};

/*
 * Dither ranges specifically for Magenta/LightMagenta (see NOTE above)
 *
 */
static const stp_simple_dither_range_t canon_dither_ranges_Mm_1bit[] =
{
  { 0.26, 0x1, 1, 1 },
  { 1.0,  0x1, 0, 1 }
};

static const canon_variable_ink_t canon_ink_Mm_1bit =
{
  canon_dither_ranges_Mm_1bit,
  sizeof(canon_dither_ranges_Mm_1bit) / sizeof(stp_simple_dither_range_t),
  .75
};


/*
 * Dither ranges specifically for any Color and 2bit/pixel (see NOTE above)
 *
 */
static const stp_simple_dither_range_t canon_dither_ranges_X_2bit[] =
{
  { 0.45,  0x1, 0, 1 },
  { 0.68,  0x2, 0, 2 },
  { 1.0,   0x3, 0, 3 }
};

static const canon_variable_ink_t canon_ink_X_2bit =
{
  canon_dither_ranges_X_2bit,
  sizeof(canon_dither_ranges_X_2bit) / sizeof(stp_simple_dither_range_t),
  1.0
};

/*
 * Dither ranges specifically for any Color/LightColor and 2bit/pixel
 * (see NOTE above)
 */
static const stp_simple_dither_range_t canon_dither_ranges_Xx_2bit[] =
{
  { 0.15,  0x1, 1, 1 },
  { 0.227, 0x2, 1, 2 },
/*{ 0.333, 0x3, 1, 3 }, */
  { 0.45,  0x1, 0, 1 },
  { 0.68,  0x2, 0, 2 },
  { 1.0,   0x3, 0, 3 }
};

static const canon_variable_ink_t canon_ink_Xx_2bit =
{
  canon_dither_ranges_Xx_2bit,
  sizeof(canon_dither_ranges_Xx_2bit) / sizeof(stp_simple_dither_range_t),
  1.0
};

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
static const stp_simple_dither_range_t canon_dither_ranges_X_3bit[] =
{
  { 0.45,  0x1, 0, 1 },
  { 0.55,  0x2, 0, 2 },
  { 0.66,  0x3, 0, 3 },
  { 0.77,  0x4, 0, 4 },
  { 0.88,  0x5, 0, 5 },
  { 1.0,   0x6, 0, 6 }
};

static const canon_variable_ink_t canon_ink_X_3bit =
{
  canon_dither_ranges_X_3bit,
  sizeof(canon_dither_ranges_X_3bit) / sizeof(stp_simple_dither_range_t),
  1.0
};

/*
 * Dither ranges specifically for any Color/LightColor and 3bit/pixel
 * (see NOTE above)
 */
static const stp_simple_dither_range_t canon_dither_ranges_Xx_3bit[] =
{
  { 0.15,  0x1, 1, 1 },
  { 0.227, 0x2, 1, 2 },
  { 0.333, 0x3, 1, 3 },
/*  { 0.333, 0x3, 1, 3 }, */
  { 0.45,  0x1, 0, 1 },
  { 0.55,  0x2, 0, 2 },
  { 0.66,  0x3, 0, 3 },
  { 0.77,  0x4, 0, 4 },
  { 0.88,  0x5, 0, 5 },
  { 1.0,   0x6, 0, 6 }
};

static const canon_variable_ink_t canon_ink_Xx_3bit =
{
  canon_dither_ranges_Xx_3bit,
  sizeof(canon_dither_ranges_Xx_3bit) / sizeof(stp_simple_dither_range_t),
  1.0
};


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
  &canon_ink_X_2bit,
  &canon_ink_X_2bit,
  &canon_ink_X_2bit,
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
  &canon_ink_Cc_1bit,
  &canon_ink_Mm_1bit,
  NULL,
  NULL
};

/* Inkset for printing in CMYK and 2bit/pixel */
static const canon_variable_inkset_t ci_CMYK_2 =
{
  &canon_ink_X_2bit,
  &canon_ink_X_2bit,
  &canon_ink_X_2bit,
  &canon_ink_X_2bit
};

/* Inkset for printing in CcMmYK and 2bit/pixel */
static const canon_variable_inkset_t ci_CcMmYK_2 =
{
  &canon_ink_Xx_2bit,
  &canon_ink_Xx_2bit,
  &canon_ink_X_2bit,
  &canon_ink_X_2bit
};

/* Inkset for printing in CMYK and 3bit/pixel */
static const canon_variable_inkset_t ci_CMYK_3 =
{
  &canon_ink_X_3bit,
  &canon_ink_X_3bit,
  &canon_ink_X_3bit,
  &canon_ink_X_3bit
};

/* Inkset for printing in CcMmYK and 3bit/pixel */
static const canon_variable_inkset_t ci_CcMmYK_3 =
{
  &canon_ink_Xx_3bit,
  &canon_ink_Xx_3bit,
  &canon_ink_X_3bit,
  &canon_ink_X_3bit,
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


static const double standard_sat_adjustment[49] =
{
  1.0,				/* C */
  1.1,
  1.2,
  1.3,
  1.4,
  1.5,
  1.6,
  1.7,
  1.8,				/* B */
  1.9,
  1.9,
  1.9,
  1.7,
  1.5,
  1.3,
  1.1,
  1.0,				/* M */
  1.0,
  1.0,
  1.0,
  1.0,
  1.0,
  1.0,
  1.0,
  1.0,				/* R */
  1.0,
  1.0,
  1.0,
  1.0,
  1.0,
  1.0,
  1.0,
  1.0,				/* Y */
  1.0,
  1.0,
  1.1,
  1.2,
  1.3,
  1.4,
  1.5,
  1.5,				/* G */
  1.4,
  1.3,
  1.2,
  1.1,
  1.0,
  1.0,
  1.0,
  1.0				/* C */
};

static const double standard_lum_adjustment[49] =
{
  0.50,				/* C */
  0.6,
  0.7,
  0.8,
  0.9,
  0.86,
  0.82,
  0.79,
  0.78,				/* B */
  0.8,
  0.83,
  0.87,
  0.9,
  0.95,
  1.05,
  1.15,
  1.3,				/* M */
  1.25,
  1.2,
  1.15,
  1.12,
  1.09,
  1.06,
  1.03,
  1.0,				/* R */
  1.0,
  1.0,
  1.0,
  1.0,
  1.0,
  1.0,
  1.0,
  1.0,				/* Y */
  0.9,
  0.8,
  0.7,
  0.65,
  0.6,
  0.55,
  0.52,
  0.48,				/* G */
  0.47,
  0.47,
  0.49,
  0.49,
  0.49,
  0.52,
  0.51,
  0.50				/* C */
};

static const double standard_hue_adjustment[49] =
{
  0.00,				/* C */
  0.05,
  0.04,
  0.01,
  -0.03,
  -0.10,
  -0.18,
  -0.26,
  -0.35,			/* B */
  -0.43,
  -0.40,
  -0.32,
  -0.25,
  -0.18,
  -0.10,
  -0.07,
  0.00,				/* M */
  -0.04,
  -0.09,
  -0.13,
  -0.18,
  -0.23,
  -0.27,
  -0.31,
  -0.35,			/* R */
  -0.38,
  -0.30,
  -0.23,
  -0.15,
  -0.08,
  0.00,
  -0.02,
  0.00,				/* Y */
  0.08,
  0.10,
  0.08,
  0.05,
  0.03,
  -0.03,
  -0.12,
  -0.20,			/* G */
  -0.17,
  -0.20,
  -0.17,
  -0.15,
  -0.12,
  -0.10,
  -0.08,
  0.00,				/* C */
};

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
  const double *lum_adjustment;
  const double *hue_adjustment;
  const double *sat_adjustment;
} canon_cap_t;

static void canon_write_line(const stp_vars_t, const canon_cap_t *, int,
			     unsigned char *, int,
			     unsigned char *, int,
			     unsigned char *, int,
			     unsigned char *, int,
			     unsigned char *, int,
			     unsigned char *, int,
			     unsigned char *, int,
			     int, int, int, int *,int);


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
  /* default settings for unkown models */

  {   -1, 8*72,11*72,180,180,20,20,20,20, CANON_INK_K, CANON_SLOT_ASF1, 0 },

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


static const double plain_paper_lum_adjustment[49] =
{
  1.2,				/* C */
  1.22,
  1.28,
  1.34,
  1.39,
  1.42,
  1.45,
  1.48,
  1.5,				/* B */
  1.4,
  1.3,
  1.25,
  1.2,
  1.1,
  1.05,
  1.05,
  1.05,				/* M */
  1.05,
  1.05,
  1.05,
  1.05,
  1.05,
  1.05,
  1.05,
  1.05,				/* R */
  1.05,
  1.05,
  1.1,
  1.1,
  1.1,
  1.1,
  1.1,
  1.1,				/* Y */
  1.15,
  1.3,
  1.45,
  1.6,
  1.75,
  1.9,
  2.0,
  2.1,				/* G */
  2.0,
  1.8,
  1.7,
  1.6,
  1.5,
  1.4,
  1.3,
  1.2				/* C */
};

typedef struct {
  const char *name;
  const char *text;
  int media_code;
  double base_density;
  double k_lower_scale;
  double k_upper;
  const double *hue_adjustment;
  const double *lum_adjustment;
  const double *sat_adjustment;
} paper_t;

typedef struct {
  const canon_cap_t *caps;
  int output_type;
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

static const paper_t *
get_media_type(const char *name)
{
  int i;
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
  if (!strcmp(name,"Auto"))    return 4;
  if (!strcmp(name,"Manual"))    return 0;
  if (!strcmp(name,"ManualNP")) return 1;

  stp_deprintf(STP_DBG_CANON,"canon: Unknown source type '%s' - reverting to auto\n",name);
  return 4;
}

static int
canon_printhead_type(const char *name, const canon_cap_t * caps)
{
  /* used internally: do not translate */
  if (!strcmp(name,"Gray"))             return 0;
  if (!strcmp(name,"RGB"))             return 1;
  if (!strcmp(name,"CMYK"))       return 2;
  if (!strcmp(name,"PhotoCMY"))       return 3;
  if (!strcmp(name,"Photo"))             return 4;
  if (!strcmp(name,"PhotoCMYK")) return 5;

  if (*name == 0) {
    if (caps->inks & CANON_INK_CMYK) return 2;
    if (caps->inks & CANON_INK_CMY)  return 1;
    if (caps->inks & CANON_INK_K)    return 0;
  }

  stp_deprintf(STP_DBG_CANON,"canon: Unknown head combo '%s' - reverting to black",name);
  return 0;
}

static colormode_t
canon_printhead_colors(const char *name, const canon_cap_t * caps)
{
  /* used internally: do not translate */
  if (!strcmp(name,"Gray"))             return COLOR_MONOCHROME;
  if (!strcmp(name,"RGB"))             return COLOR_CMY;
  if (!strcmp(name,"CMYK"))       return COLOR_CMYK;
  if (!strcmp(name,"PhotoCMY"))       return COLOR_CCMMYK;
  if (!strcmp(name,"PhotoCMYK")) return COLOR_CCMMYYK;

  if (*name == 0) {
    if (caps->inks & CANON_INK_CMYK) return COLOR_CMYK;
    if (caps->inks & CANON_INK_CMY)  return COLOR_CMY;
    if (caps->inks & CANON_INK_K)    return COLOR_MONOCHROME;
  }

  stp_deprintf(STP_DBG_CANON,"canon: Unknown head combo '%s' - reverting to black",name);
  return COLOR_MONOCHROME;
}

static unsigned char
canon_size_type(const stp_vars_t v, const canon_cap_t * caps)
{
  const stp_papersize_t pp = stp_get_papersize_by_size(stp_get_page_height(v),
						       stp_get_page_width(v));
  if (pp)
    {
      const char *name = stp_papersize_get_name(pp);
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

static char *
c_strdup(const char *s)
{
  char *ret = stp_malloc(strlen(s) + 1);
  strcpy(ret, s);
  return ret;
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

static const double *
canon_lum_adjustment(const stp_printer_t printer)
{
  const canon_cap_t * caps=
    canon_get_model_capabilities(stp_printer_get_model(printer));
  return (caps->lum_adjustment);
}

static const double *
canon_hue_adjustment(const stp_printer_t printer)
{
  const canon_cap_t * caps=
    canon_get_model_capabilities(stp_printer_get_model(printer));
  return (caps->hue_adjustment);
}

static const double *
canon_sat_adjustment(const stp_printer_t printer)
{
  const canon_cap_t * caps=
    canon_get_model_capabilities(stp_printer_get_model(printer));
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
canon_describe_resolution(const stp_printer_t printer,
			const char *resolution, int *x, int *y)
{
  *x = -1;
  *y = -1;
  sscanf(resolution, "%dx%d", x, y);
  return;
}

static const stp_param_t media_sources[] =
              {
                { "Auto",	N_ ("Auto Sheet Feeder") },
                { "Manual",	N_ ("Manual with Pause") },
                { "ManualNP",	N_ ("Manual without Pause") }
              };


/*
 * 'canon_parameters()' - Return the parameter values for the given parameter.
 */

static stp_param_t *				/* O - Parameter values */
canon_parameters(const stp_printer_t printer,	/* I - Printer model */
                 const char *ppd_file,		/* I - PPD file (not used) */
                 const char *name,		/* I - Name of parameter */
                 int  *count)			/* O - Number of values */
{
  int		i;
  stp_param_t *valptrs= 0;

  const canon_cap_t * caps=
    canon_get_model_capabilities(stp_printer_get_model(printer));

  if (count == NULL)
    return (NULL);

  *count = 0;

  if (name == NULL)
    return (NULL);

  if (strcmp(name, "PageSize") == 0)
  {
    int height_limit, width_limit;
    int papersizes = stp_known_papersizes();
    valptrs = stp_zalloc(sizeof(stp_param_t) * papersizes);
    *count = 0;

    width_limit = caps->max_width;
    height_limit = caps->max_height;

    for (i = 0; i < papersizes; i++) {
      const stp_papersize_t pt = stp_get_papersize_by_index(i);
      if (strlen(stp_papersize_get_name(pt)) > 0 &&
	  stp_papersize_get_width(pt) <= width_limit &&
	  stp_papersize_get_height(pt) <= height_limit)
	{
	  valptrs[*count].name = c_strdup(stp_papersize_get_name(pt));
	  valptrs[*count].text = c_strdup(stp_papersize_get_text(pt));
	  (*count)++;
	}
    }
  }
  else if (strcmp(name, "Resolution") == 0)
  {
    char tmp[100];
    int x,y;
    int c= 0;
    int t;
    valptrs = stp_zalloc(sizeof(stp_param_t) * 10);

    for (x=1; x<6; x++) {
      for (y=x-1; y<x+1; y++) {
	if ((t= canon_ink_type(caps,(x<<4)|y)) > -1) {
	  sprintf(tmp,"%dx%ddpi",
		   (1<<x)/2*caps->base_res,(1<<y)/2*caps->base_res);
	  valptrs[c].name= c_strdup(tmp);

	  sprintf(tmp,"%dx%d DPI",
		   (1<<x)/2*caps->base_res,(1<<y)/2*caps->base_res);
	  stp_deprintf(STP_DBG_CANON,"supports mode '%s'\n",tmp);
	  valptrs[c++].text= c_strdup(tmp);

	  if (t==1) {
	    sprintf(tmp,"%dx%ddmt",
		     (1<<x)/2*caps->base_res,(1<<y)/2*caps->base_res);
	    valptrs[c].name= c_strdup(tmp);
	    sprintf(tmp,"%dx%d DPI DMT",
		     (1<<x)/2*caps->base_res,(1<<y)/2*caps->base_res);
	    stp_deprintf(STP_DBG_CANON,"supports mode '%s'\n",tmp);
	    valptrs[c++].text = c_strdup(tmp);
	  }
	}
      }
    }
    *count= c;
  }
  else if (strcmp(name, "InkType") == 0)
  {
    int c= 0;
    valptrs = stp_zalloc(sizeof(stp_param_t) * 5);
    /* used internally: do not translate */
    if ((caps->inks & CANON_INK_K))
    {
      valptrs[c].name   = c_strdup("Gray");
      valptrs[c++].text = c_strdup(_("Black"));
    }
    if ((caps->inks & CANON_INK_CMY))
    {
      valptrs[c].name   = c_strdup("RGB");
      valptrs[c++].text = c_strdup(_("CMY Color"));
    }
    if ((caps->inks & CANON_INK_CMYK))
    {
      valptrs[c].name   = c_strdup("CMYK");
      valptrs[c++].text = c_strdup(_("CMYK Color"));
    }
    if ((caps->inks & CANON_INK_CcMmYK))
    {
      valptrs[c].name   = c_strdup("PhotoCMY");
      valptrs[c++].text = c_strdup(_("Photo CcMmY Color"));
    }
    if ((caps->inks & CANON_INK_CcMmYyK))
    {
      valptrs[c].name   = c_strdup("PhotoCMYK");
      valptrs[c++].text = c_strdup(_("Photo CcMmYK Color"));
    }

    *count = c;
  }
  else if (strcmp(name, "MediaType") == 0)
  {
    *count = sizeof(canon_paper_list) / sizeof(canon_paper_list[0]);

    valptrs = stp_zalloc(*count * sizeof(stp_param_t));

    for (i = 0; i < *count; i ++)
    {
      valptrs[i].name = c_strdup(canon_paper_list[i].name);
      valptrs[i].text = c_strdup(_(canon_paper_list[i].text));
    }
  }
  else if (strcmp(name, "InputSlot") == 0)
  {
    *count = sizeof(media_sources) / sizeof(media_sources[0]);

    valptrs = stp_zalloc(*count * sizeof(stp_param_t));
    for (i = 0; i < *count; i ++)
    {
      /* translate media_sources */
      valptrs[i].name = c_strdup(media_sources[i].name);
      valptrs[i].text = c_strdup(_(media_sources[i].text));
    }
  }
  else
    return (NULL);

  return (valptrs);
}

static const char *
canon_default_parameters(const stp_printer_t printer,
			 const char *ppd_file,
			 const char *name)
{
  int		i;
  const canon_cap_t * caps=
    canon_get_model_capabilities(stp_printer_get_model(printer));

  if (name == NULL)
    return (NULL);

  if (strcmp(name, "PageSize") == 0)
  {
    int height_limit, width_limit;
    int papersizes = stp_known_papersizes();

    width_limit  = caps->max_width;
    height_limit = caps->max_height;

    for (i = 0; i < papersizes; i++) {
      const stp_papersize_t pt = stp_get_papersize_by_index(i);
      if (strlen(stp_papersize_get_name(pt)) > 0 &&
	  stp_papersize_get_width(pt) <= width_limit &&
	  stp_papersize_get_height(pt) <= height_limit)
	return (stp_papersize_get_name(pt));
    }
    return NULL;
  }
  else if (strcmp(name, "Resolution") == 0)
  {
    char tmp[100];
    int x,y;
    int t;
    int min_res = caps->base_res;
    while (min_res < 300)
      min_res *= 2;

    for (x=1; x<6; x++)
      {
	for (y=x-1; y<x+1; y++)
	  {
	    if ((t= canon_ink_type(caps,(x<<4)|y)) > -1)
	      {
	        if (t == 1)
		  sprintf(tmp, "%dx%ddmt", min_res, min_res);
		else
		  sprintf(tmp,"%dx%ddpi", min_res, min_res);

		stp_deprintf(STP_DBG_CANON,"supports mode '%s'\n",tmp);
		return (c_strdup(tmp));
	      }
	  }
      }
    return NULL;
  }
  else if (strcmp(name, "InkType") == 0)
  {
    /* used internally: do not translate */
    if ((caps->inks & CANON_INK_K))
      return ("Gray");
    if ((caps->inks & CANON_INK_CMY))
      return ("RGB");
    if ((caps->inks & CANON_INK_CMYK))
      return ("CMYK");
    if ((caps->inks & CANON_INK_CcMmYK))
      return ("PhotoCMY");
    if ((caps->inks & CANON_INK_CcMmYyK))
      return ("PhotoCMYK");
    return NULL;
  }
  else if (strcmp(name, "MediaType") == 0)
  {
    return (canon_paper_list[0].name);
  }
  else if (strcmp(name, "InputSlot") == 0)
  {
    return (media_sources[0].name);
  }
  else
    return (NULL);
}


/*
 * 'canon_imageable_area()' - Return the imageable area of the page.
 */

static void
canon_imageable_area(const stp_printer_t printer,	/* I - Printer model */
		     const stp_vars_t v,   /* I */
                     int  *left,	/* O - Left position in points */
                     int  *right,	/* O - Right position in points */
                     int  *bottom,	/* O - Bottom position in points */
                     int  *top)		/* O - Top position in points */
{
  int	width, length;			/* Size of page */

  const canon_cap_t * caps=
    canon_get_model_capabilities(stp_printer_get_model(printer));

  stp_default_media_size(printer, v, &width, &length);

  *left   = caps->border_left;
  *right  = width - caps->border_right;
  *top    = length - caps->border_top;
  *bottom = caps->border_bottom;
}

static void
canon_limit(const stp_printer_t printer,	/* I - Printer model */
	    const stp_vars_t v,  		/* I */
	    int *width,
	    int *height,
	    int *min_width,
	    int *min_height)
{
  const canon_cap_t * caps=
    canon_get_model_capabilities(stp_printer_get_model(printer));
  *width =	caps->max_width;
  *height =	caps->max_height;
  *min_width = 1;
  *min_height = 1;
}

/*
 * 'canon_cmd()' - Sends a command with variable args
 */
static void
canon_cmd(const stp_vars_t v, /* I - the printer         */
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
      stp_putc((num & 255),v);
      stp_putc((num >> 8 ),v);
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

#define MIN(a,b) (((a)<(b)) ? (a) : (b))
#define MAX(a,b) (((a)>(b)) ? (a) : (b))

/* ESC [K --  -- reset printer:
 */
static void
canon_init_resetPrinter(const stp_vars_t v, canon_init_t *init)
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
canon_init_setPageMode(const stp_vars_t v, canon_init_t *init)
{
  if (!(init->caps->features & CANON_CAP_a))
    return;

  if (init->caps->features & CANON_CAP_a)
    canon_cmd(v,ESC28,0x61, 1, 0x01);
}

/* ESC (b -- 0x62 -- -- set data compression:
 */
static void
canon_init_setDataCompression(const stp_vars_t v, canon_init_t *init)
{
  if (!(init->caps->features & CANON_CAP_b))
    return;

  canon_cmd(v,ESC28,0x62, 1, 0x01);
}

/* ESC (c -- 0x63 -- cmdSetColor --:
 */
static void
canon_init_setColor(const stp_vars_t v, canon_init_t *init)
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
		if (init->output_type==OUTPUT_GRAY || init->output_type == OUTPUT_MONOCHROME)
    			arg_63_1|= 0x01;					/* PRINT_COLOUR */

  		arg_63_2 = ((init->pt ? init->pt->media_code : 0) << 4)		/* PRINT_MEDIA */
			+ 1;	/* hardcode to High quality for now */		/* PRINT_QUALITY */

  		canon_cmd(v,ESC28,0x63, 2, arg_63_1, arg_63_2);
		break;

	case 2:			/* are any models using this? */
		break;

	case 3:			/* 720 dpi series - BJC-3000 and descendants */
		if (init->output_type==OUTPUT_GRAY || init->output_type == OUTPUT_MONOCHROME)
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
canon_init_setResolution(const stp_vars_t v, canon_init_t *init)
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
canon_init_setPageMargins(const stp_vars_t v, canon_init_t *init)
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
canon_init_setTray(const stp_vars_t v, canon_init_t *init)
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
canon_init_setPrintMode(const stp_vars_t v, canon_init_t *init)
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
  if (init->output_type==OUTPUT_GRAY || init->output_type == OUTPUT_MONOCHROME)
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
canon_init_setPageMargins2(const stp_vars_t v, canon_init_t *init)
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
canon_init_setPageID(const stp_vars_t v, canon_init_t *init)
{
  if (!(init->caps->features & CANON_CAP_q))
    return;

  canon_cmd(v,ESC28,0x71, 1, 0x01);
}

/* ESC (r -- 0x72 --  -- :
 */
static void
canon_init_setX72(const stp_vars_t v, canon_init_t *init)
{
  if (!(init->caps->features & CANON_CAP_r))
    return;

  canon_cmd(v,ESC28,0x72, 1, 0x61); /* whatever for - 8200 needs it */
}

/* ESC (t -- 0x74 -- cmdSetImage --:
 */
static void
canon_init_setImage(const stp_vars_t v, canon_init_t *init)
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
canon_init_printer(const stp_vars_t v, canon_init_t *init)
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
canon_deinit_printer(const stp_vars_t v, canon_init_t *init)
{
  /* eject page */
  stp_putc(0x0c,v);

  /* say goodbye */
  canon_cmd(v,ESC28,0x62,1,0);
  if (init->caps->features & CANON_CAP_a)
    canon_cmd(v,ESC28,0x61, 1, 0);
  canon_cmd(v,ESC40,0,0);
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

/*
 * 'canon_print()' - Print an image to a CANON printer.
 */
static void
canon_print(const stp_printer_t printer,		/* I - Model */
	    stp_image_t *image,		/* I - Image to print */
	    const stp_vars_t v)
{
  int i;
  const unsigned char *cmap = stp_get_cmap(v);
  int		model = stp_printer_get_model(printer);
  const char	*resolution = stp_get_resolution(v);
  const char	*media_source = stp_get_media_source(v);
  int 		output_type = stp_get_output_type(v);
  int		orientation = stp_get_orientation(v);
  const char	*ink_type = stp_get_ink_type(v);
  double 	scaling = stp_get_scaling(v);
  int		top = stp_get_top(v);
  int		left = stp_get_left(v);
  int		y;		/* Looping vars */
  int		xdpi, ydpi;	/* Resolution */
  int		n;		/* Output number */
  unsigned short *out;	/* Output pixels (16-bit) */
  unsigned char	*in,		/* Input pixels */
		*black,		/* Black bitmap data */
		*cyan,		/* Cyan bitmap data */
		*magenta,	/* Magenta bitmap data */
		*yellow,	/* Yellow bitmap data */
		*lcyan,		/* Light cyan bitmap data */
		*lmagenta,	/* Light magenta bitmap data */
		*lyellow;	/* Light yellow bitmap data */
  int           delay_k,
                delay_c,
                delay_m,
                delay_y,
                delay_lc,
                delay_lm,
                delay_ly,
                delay_max;
  int		page_left,	/* Left margin of page */
		page_right,	/* Right margin of page */
		page_top,	/* Top of page */
		page_bottom,	/* Bottom of page */
		page_width,	/* Width of page */
		page_height,	/* Length of page */
		page_true_height,	/* True length of page */
		out_width,	/* Width of image on page */
		out_length,	/* Length of image on page */
		out_bpp,	/* Output bytes per pixel */
		length,		/* Length of raster data */
                buf_length,     /* Length of raster data buffer (dmt) */
		errdiv,		/* Error dividend */
		errmod,		/* Error modulus */
		errval,		/* Current error value */
		errline,	/* Current raster line */
		errlast;	/* Last raster line loaded */
  stp_convert_t	colorfunc = 0;	/* Color conversion function... */
  int		zero_mask;
  int           bits= 1;
  int           image_height,
                image_width,
                image_bpp;
  int		ink_spread;
  void *	dither;
  int           res_code;
  int           use_6color= 0;
  double        k_upper, k_lower;
  int           emptylines= 0;
  stp_vars_t	nv = stp_allocate_copy(v);
  double lum_adjustment[49], sat_adjustment[49], hue_adjustment[49];
  int have_lum_adjustment= 0;
  int have_sat_adjustment= 0;
  int have_hue_adjustment= 0;
  canon_init_t  init;
  const canon_cap_t * caps= canon_get_model_capabilities(model);
  int printhead= canon_printhead_type(ink_type,caps);
  colormode_t colormode = canon_printhead_colors(ink_type,caps);
  const paper_t *pt;
  const canon_variable_inkset_t *inks;
  stp_dither_data_t *dt;

  if (!stp_get_verified(nv))
    {
      stp_eprintf(nv, "Print options not verified; cannot print.\n");
      return;
    }

  PUT("top        ",top,72);
  PUT("left       ",left,72);

  /*
  * Setup a read-only pixel region for the entire image...
  */

  image->init(image);
  image_height = image->height(image);
  image_width = image->width(image);
  image_bpp = image->bpp(image);

  /* force grayscale if image is grayscale
   *                 or single black cartridge installed
   */

  if ((printhead == 0 || caps->inks == CANON_INK_K) &&
      output_type != OUTPUT_MONOCHROME)
    {
      output_type = OUTPUT_GRAY;
      stp_set_output_type(nv, output_type);
    }

  if (output_type == OUTPUT_GRAY || output_type == OUTPUT_MONOCHROME)
    colormode = COLOR_MONOCHROME;
  stp_set_output_color_model(nv, COLOR_MODEL_CMY);

  /*
   * Choose the correct color conversion function...
   */

  colorfunc = stp_choose_colorfunc(output_type, image_bpp, cmap, &out_bpp, nv);

 /*
  * Figure out the output resolution...
  */

  switch (sscanf(resolution,"%dx%d",&xdpi,&ydpi)) {
  case 1: ydpi= xdpi; if (ydpi>caps->max_ydpi) ydpi/= 2; break;
  case 0: xdpi= caps->max_xdpi; ydpi= caps->max_ydpi; break;
  }

  stp_deprintf(STP_DBG_CANON,"canon: resolution=%dx%d\n",xdpi,ydpi);
  stp_deprintf(STP_DBG_CANON,"       rescode   =0x%x\n",canon_res_code(caps,xdpi,ydpi));
  res_code= canon_res_code(caps,xdpi,ydpi);

  if (((!strcmp(resolution+(strlen(resolution)-3),"DMT")) ||
       (!strcmp(resolution+(strlen(resolution)-3),"dmt"))) &&
       (caps->features & CANON_CAP_DMT) &&
       output_type != OUTPUT_MONOCHROME) {
    bits= 2;
    stp_deprintf(STP_DBG_CANON,"canon: using drop modulation technology\n");
  }

 /*
  * Compute the output size...
  */

  canon_imageable_area(printer, nv, &page_left, &page_right,
                       &page_bottom, &page_top);

  stp_compute_page_parameters(page_right, page_left, page_top, page_bottom,
			  scaling, image_width, image_height, image,
			  &orientation, &page_width, &page_height,
			  &out_width, &out_length, &left, &top);

  /*
   * Recompute the image length and width.  If the image has been
   * rotated, these will change from previously.
   */
  image_height = image->height(image);
  image_width = image->width(image);

  stp_default_media_size(printer, nv, &n, &page_true_height);

  PUT("top        ",top,72);
  PUT("left       ",left,72);
  PUT("page_top   ",page_top,72);
  PUT("page_bottom",page_bottom,72);
  PUT("page_left  ",page_left,72);
  PUT("page_right ",page_right,72);
  PUT("page_width ",page_width,72);
  PUT("page_height",page_height,72);
  PUT("page_true_height",page_true_height,72);
  PUT("out_width ", out_width,xdpi);
  PUT("out_length", out_length,ydpi);

  image->progress_init(image);

  PUT("top     ",top,72);
  PUT("left    ",left,72);

  pt = get_media_type(stp_get_media_type(nv));

  init.caps = caps;
  init.output_type = output_type;
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

  canon_init_printer(nv, &init);

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
  out_length = ydpi * out_length / 72;

  PUT("out_width ", out_width,xdpi);
  PUT("out_length", out_length,ydpi);

  left = xdpi * left / 72;

  PUT("leftskip",left,xdpi);

  if(xdpi==1440){
    delay_k= 0;
    delay_c= 112;
    delay_m= 224;
    delay_y= 336;
    delay_lc= 112;
    delay_lm= 224;
    delay_ly= 336;
    delay_max= 336;
    stp_deprintf(STP_DBG_CANON,"canon: delay on!\n");
  } else {
    delay_k= delay_c= delay_m= delay_y= delay_lc= delay_lm= delay_ly=0;
    delay_max=0;
    stp_deprintf(STP_DBG_CANON,"canon: delay off!\n");
  }

 /*
  * Allocate memory for the raster data...
  */

  length = (out_width + 7) / 8;

  buf_length= length*bits;

  stp_deprintf(STP_DBG_CANON,"canon: buflength is %d!\n",buf_length);

  if (colormode==COLOR_MONOCHROME) {
    black   = stp_zalloc(buf_length*(delay_k+1));
    cyan    = NULL;
    magenta = NULL;
    lcyan   = NULL;
    lmagenta= NULL;
    yellow  = NULL;
    lyellow = NULL;
  } else {
    cyan    = stp_zalloc(buf_length*(delay_c+1));
    magenta = stp_zalloc(buf_length*(delay_m+1));
    yellow  = stp_zalloc(buf_length*(delay_y+1));

    if (colormode!=COLOR_CMY)
      black = stp_zalloc(buf_length*(delay_k+1));
    else
      black = NULL;

    if (colormode==COLOR_CCMMYK || colormode==COLOR_CCMMYYK) {
      use_6color= 1;
      lcyan = stp_zalloc(buf_length*(delay_lc+1));
      lmagenta = stp_zalloc(buf_length*(delay_lm+1));
      if (colormode==CANON_INK_CcMmYyK)
	lyellow = stp_zalloc(buf_length*(delay_lc+1));
      else
	lyellow = NULL;
    } else {
      lcyan = NULL;
      lmagenta = NULL;
      lyellow = NULL;
    }
  }

  stp_deprintf(STP_DBG_CANON,"canon: driver will use colors %s%s%s%s%s%s\n",
	       cyan ? "C" : "", lcyan ? "c" : "", magenta ? "M" : "",
	       lmagenta ? "m" : "", yellow ? "Y" : "", black ? "K" : "");

  stp_deprintf(STP_DBG_CANON,"density is %f\n",stp_get_density(nv));

  /*
   * Compute the LUT.  For now, it's 8 bit, but that may eventually
   * sometimes change.
   */
  if (pt)
    stp_set_density(nv, stp_get_density(nv) * pt->base_density);
  else				/* Can't find paper type? Assume plain */
    stp_set_density(nv, stp_get_density(nv) * .5);
  stp_set_density(nv, stp_get_density(nv) * canon_density(caps, res_code));
  if (stp_get_density(nv) > 1.0)
    stp_set_density(nv, 1.0);
  if (colormode == COLOR_MONOCHROME)
    stp_set_gamma(nv, stp_get_gamma(nv) / .8);
  stp_compute_lut(nv, 256);

  stp_deprintf(STP_DBG_CANON,"density is %f\n",stp_get_density(nv));

 /*
  * Output the page...
  */

  if (xdpi > ydpi)
    dither = stp_init_dither(image_width, out_width, 1, xdpi / ydpi, nv);
  else
    dither = stp_init_dither(image_width, out_width, ydpi / xdpi, 1, nv);

  for (i = 0; i <= NCOLORS; i++)
    stp_dither_set_black_level(dither, i, 1.0);

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
  stp_dither_set_black_lower(dither, k_lower);
  stp_dither_set_black_upper(dither, k_upper);

  if ((inks = canon_inks(caps, res_code, colormode, bits))!=0)
    {
      if (inks->c)
	stp_dither_set_ranges(dither, ECOLOR_C, inks->c->count, inks->c->range,
			  inks->c->density * stp_get_density(nv));
      if (inks->m)
	stp_dither_set_ranges(dither, ECOLOR_M, inks->m->count, inks->m->range,
			  inks->m->density * stp_get_density(nv));
      if (inks->y)
	stp_dither_set_ranges(dither, ECOLOR_Y, inks->y->count, inks->y->range,
			  inks->y->density * stp_get_density(nv));
      if (inks->k)
	stp_dither_set_ranges(dither, ECOLOR_K, inks->k->count, inks->k->range,
			  inks->k->density * stp_get_density(nv));
    }

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
      /*
      if (ydpi > canon_max_vres(model))
	ink_spread++;
      */
      if (bits > 1)
	ink_spread++;
      stp_dither_set_ink_spread(dither, ink_spread);
      break;
    }
  stp_dither_set_density(dither, stp_get_density(nv));

  in  = stp_zalloc(image_width * image_bpp);
  out = stp_zalloc(image_width * out_bpp * 2);

  errdiv  = image_height / out_length;
  errmod  = image_height % out_length;
  errval  = 0;
  errlast = -1;
  errline  = 0;
  if (canon_lum_adjustment(printer)) {
    int k;
    for (k = 0; k < 49; k++) {
      have_lum_adjustment= 1;
      lum_adjustment[k] = canon_lum_adjustment(printer)[k];
      if(pt)
	if (pt->lum_adjustment)
	  lum_adjustment[k] *= pt->lum_adjustment[k];
    }
  }
  if (canon_sat_adjustment(printer)) {
    int k;
    for (k = 0; k < 49; k++) {
      have_sat_adjustment= 1;
      sat_adjustment[k] = canon_sat_adjustment(printer)[k];
      if(pt)
	if (pt->sat_adjustment)
	  sat_adjustment[k] *= pt->sat_adjustment[k];
    }
  }
  if (canon_hue_adjustment(printer)) {
    int k;
    for (k = 0; k < 49; k++) {
      have_hue_adjustment= 1;
      hue_adjustment[k] = canon_hue_adjustment(printer)[k];
      if(pt)
	if (pt->hue_adjustment)
	  hue_adjustment[k] += pt->hue_adjustment[k];
    }
  }

  dt = stp_create_dither_data();
  stp_add_channel(dt, black, ECOLOR_K, 0);
  stp_add_channel(dt, cyan, ECOLOR_C, 0);
  stp_add_channel(dt, lcyan, ECOLOR_C, 1);
  stp_add_channel(dt, magenta, ECOLOR_M, 0);
  stp_add_channel(dt, lmagenta, ECOLOR_M, 1);
  stp_add_channel(dt, yellow, ECOLOR_Y, 0);

  for (y = 0; y < out_length; y ++)
  {
    int duplicate_line = 1;
    if ((y & 63) == 0)
      image->note_progress(image, y, out_length);

    if (errline != errlast)
    {
      errlast = errline;
      duplicate_line = 0;
      if (image->get_row(image, in, errline) != STP_IMAGE_OK)
	break;
      (*colorfunc)(nv, in, out, &zero_mask, image_width, image_bpp, cmap,
		   have_hue_adjustment ? hue_adjustment : NULL,
		   have_lum_adjustment ? lum_adjustment : NULL,
		   have_sat_adjustment ? sat_adjustment : NULL);
    }

    stp_dither(out, y, dither, dt, duplicate_line, zero_mask);

    canon_write_line(nv, caps, ydpi,
		     black,    delay_k,
		     cyan,     delay_c,
		     magenta,  delay_m,
		     yellow,   delay_y,
		     lcyan,    delay_lc,
		     lmagenta, delay_lm,
		     lyellow,  delay_ly,
		     length, out_width, left, &emptylines, bits);

    canon_advance_buffer(black,   buf_length,delay_k);
    canon_advance_buffer(cyan,    buf_length,delay_c);
    canon_advance_buffer(magenta, buf_length,delay_m);
    canon_advance_buffer(yellow,  buf_length,delay_y);
    canon_advance_buffer(lcyan,   buf_length,delay_lc);
    canon_advance_buffer(lmagenta,buf_length,delay_lm);
    canon_advance_buffer(lyellow, buf_length,delay_ly);

    errval += errmod;
    errline += errdiv;
    if (errval >= out_length)
    {
      errval -= out_length;
      errline ++;
    }
  }
  image->progress_conclude(image);

  stp_free_dither_data(dt);
  stp_free_dither(dither);

  /*
   * Flush delayed buffers...
   */

  if (delay_max) {
    stp_deprintf(STP_DBG_CANON,"\ncanon: flushing %d possibly delayed buffers\n",
	    delay_max);
    for (y= 0; y<delay_max; y++) {

      canon_write_line(nv, caps, ydpi,
		       black,    delay_k,
		       cyan,     delay_c,
		       magenta,  delay_m,
		       yellow,   delay_y,
		       lcyan,    delay_lc,
		       lmagenta, delay_lm,
		       lyellow,  delay_ly,
		       length, out_width, left, &emptylines, bits);

      canon_advance_buffer(black,   buf_length,delay_k);
      canon_advance_buffer(cyan,    buf_length,delay_c);
      canon_advance_buffer(magenta, buf_length,delay_m);
      canon_advance_buffer(yellow,  buf_length,delay_y);
      canon_advance_buffer(lcyan,   buf_length,delay_lc);
      canon_advance_buffer(lmagenta,buf_length,delay_lm);
      canon_advance_buffer(lyellow, buf_length,delay_ly);
    }
  }

 /*
  * Cleanup...
  */

  stp_free_lut(nv);
  stp_free(in);
  stp_free(out);

  if (black != NULL)    stp_free(black);
  if (cyan != NULL)     stp_free(cyan);
  if (magenta != NULL)  stp_free(magenta);
  if (yellow != NULL)   stp_free(yellow);
  if (lcyan != NULL)    stp_free(lcyan);
  if (lmagenta != NULL) stp_free(lmagenta);
  if (lyellow != NULL)  stp_free(lyellow);

  canon_deinit_printer(nv, &init);
  stp_free_vars(nv);
}

const stp_printfuncs_t stp_canon_printfuncs =
{
  canon_parameters,
  stp_default_media_size,
  canon_imageable_area,
  canon_limit,
  canon_print,
  canon_default_parameters,
  canon_describe_resolution,
  stp_verify_printer_params,
  stp_start_job,
  stp_end_job
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

static void
canon_shift_buffer2(unsigned char *line,int length,int bits)
{
  int i;
  for (i=length-1; i>0; i--) {
    line[i]= (line[i] >> bits) | (line[i-1] << (8-bits));
  }
  line[0] = line[0] >> bits;
}


/*
 * 'canon_write()' - Send graphics using TIFF packbits compression.
 */

static int
canon_write(const stp_vars_t v,		/* I - Print file or command */
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
    comp_buf[COMPBUFWIDTH],		/* Compression buffer */
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
    memset(in_fold,0,length*2);
    canon_fold_2bit(line,length,in_fold);
    in_ptr= in_fold;
    length= (length*8/4); /* 4 pixels in 8bit */
    offset= (offset*8/4); /* 4 pixels in 8bit  */
  }
  if (bits==3) {
    memset(in_fold,0,length*3);
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

  stp_pack_tiff(in_ptr, length, comp_data, &comp_ptr);
  newlength= comp_ptr - comp_buf;

  /* send packed empty lines if any */

  if (*empty) {
    stp_zfwrite("\033\050\145\002\000", 5, 1, v);
    stp_putc((*empty) >> 8 , v);
    stp_putc((*empty) & 255, v);
    *empty= 0;
  }

 /* Send a line of raster graphics... */

  stp_zfwrite("\033\050\101", 3, 1, v);
  stp_putc((newlength+1) & 255, v);
  stp_putc((newlength+1) >> 8, v);
  color= "CMYKcmy"[coloridx];
  if (!color) color= 'K';
  stp_putc(color,v);
  stp_zfwrite((const char *)comp_buf, newlength, 1, v);
  stp_putc('\015', v);
  return 1;
}


static void
canon_write_line(const stp_vars_t v,	/* I - Print file or command */
		 const canon_cap_t *   caps,	/* I - Printer model */
		 int           ydpi,	/* I - Vertical resolution */
		 unsigned char *k,	/* I - Output bitmap data */
		 int           dk,	/* I -  */
		 unsigned char *c,	/* I - Output bitmap data */
		 int           dc,	/* I -  */
		 unsigned char *m,	/* I - Output bitmap data */
		 int           dm,	/* I -  */
		 unsigned char *y,	/* I - Output bitmap data */
		 int           dy,	/* I -  */
		 unsigned char *lc,	/* I - Output bitmap data */
		 int           dlc,	/* I -  */
		 unsigned char *lm,	/* I - Output bitmap data */
		 int           dlm,	/* I -  */
		 unsigned char *ly,	/* I - Output bitmap data */
		 int           dly,	/* I -  */
		 int           l,	/* I - Length of bitmap data */
		 int           width,	/* I - Printed width */
		 int           offset,  /* I - horizontal offset */
		 int           *empty,  /* IO- unflushed empty lines */
		 int           bits)
{
  int written= 0;

  if (k) written+=
    canon_write(v, caps, k+ dk*l,  l, 3, ydpi, empty, width, offset, bits);
  if (y) written+=
    canon_write(v, caps, y+ dy*l,  l, 2, ydpi, empty, width, offset, bits);
  if (m) written+=
    canon_write(v, caps, m+ dm*l,  l, 1, ydpi, empty, width, offset, bits);
  if (c) written+=
    canon_write(v, caps, c+ dc*l,  l, 0, ydpi, empty, width, offset, bits);
  if (ly) written+=
    canon_write(v, caps, ly+dly*l, l, 6, ydpi, empty, width, offset, bits);
  if (lm) written+=
    canon_write(v, caps, lm+dlm*l, l, 5, ydpi, empty, width, offset, bits);
  if (lc) written+=
    canon_write(v, caps, lc+dlc*l, l, 4, ydpi, empty, width, offset, bits);

  if (written||(empty==0))
    stp_zfwrite("\033\050\145\002\000\000\001", 7, 1, v);
  else
    (*empty)+= 1;
}

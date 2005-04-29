/*
 * "$Id: print-olympus.c,v 1.1.1.2 2004/12/22 23:49:39 jlovell Exp $"
 *
 *   Print plug-in Olympus driver for the GIMP.
 *
 *   Copyright 2003 Michael Mraka (Michael.Mraka@linux.cz)
 *
 *   The plug-in is based on the code of the RAW plugin for the GIMP of
 *   Michael Sweet (mike@easysw.com) and Robert Krawitz (rlk@alum.mit.edu)
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

#ifdef __GNUC__
#define inline __inline__
#endif

#define OLYMPUS_INTERLACE_NONE	0
#define OLYMPUS_INTERLACE_LINE	1
#define OLYMPUS_INTERLACE_PLANE	2

#define OLYMPUS_FEATURE_NONE		0x00000000
#define OLYMPUS_FEATURE_FULL_WIDTH	0x00000001
#define OLYMPUS_FEATURE_FULL_HEIGHT	0x00000002
#define OLYMPUS_FEATURE_BLOCK_ALIGN	0x00000004
#define OLYMPUS_FEATURE_BORDERLESS	0x00000008
#define OLYMPUS_FEATURE_WHITE_BORDER	0x00000010

#define MIN(a,b)	(((a) < (b)) ? (a) : (b))
#define MAX(a,b)	(((a) > (b)) ? (a) : (b))

static const char *zero = "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0";

typedef struct
{
  const char *output_type;
  int output_channels;
  const char *name;
  const char *channel_order;
} ink_t;

typedef struct {
  const ink_t *item;
  size_t n_items;
} ink_list_t;

typedef struct {
  const char* name;
  int xdpi;
  int ydpi;
} olymp_resolution_t;

typedef struct {
  const olymp_resolution_t *item;
  size_t n_items;
} olymp_resolution_list_t;

typedef struct {
  const char* name;
  const char* text;
  int width_pt;
  int height_pt;
  int border_pt_left;
  int border_pt_right;
  int border_pt_top;
  int border_pt_bottom;
} olymp_pagesize_t;

typedef struct {
  const olymp_pagesize_t *item;
  size_t n_items;
} olymp_pagesize_list_t;

typedef struct {
  const char* res_name;
  const char* pagesize_name;
  int width_px;
  int height_px;
} olymp_printsize_t;

typedef struct {
  const olymp_printsize_t *item;
  size_t n_items;
} olymp_printsize_list_t;


typedef struct {
  const char *name;
  const char *text;
  const stp_raw_t seq;
} laminate_t;

typedef struct {
  const laminate_t *item;
  size_t n_items;
} laminate_list_t;

typedef struct
{
  int xdpi, ydpi;
  int xsize, ysize;
  char plane;
  int block_min_x, block_min_y;
  int block_max_x, block_max_y;
  const char* pagesize;
  const laminate_t* laminate;
} olympus_privdata_t;

static olympus_privdata_t privdata;

typedef struct /* printer specific parameters */
{
  int model;		/* printer model number from printers.xml*/
  const ink_list_t *inks;
  const olymp_resolution_list_t *resolution;
  const olymp_pagesize_list_t *pages;
  const olymp_printsize_list_t *printsize;
  int interlacing;	/* color interlacing scheme */
  int block_size;
  int features;		
  void (*printer_init_func)(stp_vars_t *);
  void (*printer_end_func)(stp_vars_t *);
  void (*plane_init_func)(stp_vars_t *);
  void (*plane_end_func)(stp_vars_t *);
  void (*block_init_func)(stp_vars_t *);
  void (*block_end_func)(stp_vars_t *);
  const char *adj_cyan;		/* default color adjustment */
  const char *adj_magenta;
  const char *adj_yellow;
  const laminate_list_t *laminate;
} olympus_cap_t;


static const olympus_cap_t* olympus_get_model_capabilities(int model);
static const laminate_t* olympus_get_laminate_pattern(stp_vars_t *v);


static const ink_t cmy_inks[] =
{
  { "CMY", 3, "CMY", "\1\2\3" },
};

static const ink_list_t cmy_ink_list =
{
  cmy_inks, sizeof(cmy_inks) / sizeof(ink_t)
};

static const ink_t ymc_inks[] =
{
  { "CMY", 3, "CMY", "\3\2\1" },
};

static const ink_list_t ymc_ink_list =
{
  ymc_inks, sizeof(ymc_inks) / sizeof(ink_t)
};

static const ink_t rgb_inks[] =
{
  { "RGB", 3, "RGB", "\1\2\3" },
};

static const ink_list_t rgb_ink_list =
{
  rgb_inks, sizeof(rgb_inks) / sizeof(ink_t)
};

static const ink_t bgr_inks[] =
{
  { "RGB", 3, "RGB", "\3\2\1" },
};

static const ink_list_t bgr_ink_list =
{
  bgr_inks, sizeof(bgr_inks) / sizeof(ink_t)
};


/* Olympus P-10 */
static const olymp_resolution_t res_320dpi[] =
{
  { "320x320", 320, 320},
};

static const olymp_resolution_list_t res_320dpi_list =
{
  res_320dpi, sizeof(res_320dpi) / sizeof(olymp_resolution_t)
};

static const olymp_pagesize_t p10_page[] =
{
  { "w288h432", "4 x 6", -1, -1, 0, 0, 16, 0},	/* 4x6" */
  { "B7", "3.5 x 5", -1, -1, 0, 0, 4, 0},	/* 3.5x5" */
  { "Custom", NULL, -1, -1, 28, 28, 48, 48},
};

static const olymp_pagesize_list_t p10_page_list =
{
  p10_page, sizeof(p10_page) / sizeof(olymp_pagesize_t)
};

static const olymp_printsize_t p10_printsize[] =
{
  { "320x320", "w288h432", 1280, 1848},
  { "320x320", "B7",  1144,  1591},
  { "320x320", "Custom", 1280, 1848},
};

static const olymp_printsize_list_t p10_printsize_list =
{
  p10_printsize, sizeof(p10_printsize) / sizeof(olymp_printsize_t)
};

static void p10_printer_init_func(stp_vars_t *v)
{
  stp_zfwrite("\033R\033M\033S\2\033N\1\033D\1\033Y", 1, 15, v);
  stp_zfwrite((privdata.laminate->seq).data, 1,
		  (privdata.laminate->seq).bytes, v); /* laminate */
  stp_zfwrite("\033Z\0", 1, 3, v);
}

static void p10_printer_end_func(stp_vars_t *v)
{
  stp_zfwrite("\033P", 1, 2, v);
}

static void p10_block_init_func(stp_vars_t *v)
{
  stp_zprintf(v, "\033T%c", privdata.plane);
  stp_put16_le(privdata.block_min_x, v);
  stp_put16_le(privdata.block_min_y, v);
  stp_put16_le(privdata.block_max_x + 1, v);
  stp_put16_le(privdata.block_max_y + 1, v);
}

static const laminate_t p10_laminate[] =
{
  {"Coated",  N_("Coated"),  {1, "\x00"}},
  {"None",    N_("None"),    {1, "\x02"}},
};

static const laminate_list_t p10_laminate_list =
{
  p10_laminate, sizeof(p10_laminate) / sizeof(laminate_t)
};


/* Olympus P-200 series */
static const olymp_pagesize_t p200_page[] =
{
  { "ISOB7", "80x125mm", -1, -1, 16, 17, 33, 33},
  { "Custom", NULL, -1, -1, 16, 17, 33, 33},
};

static const olymp_pagesize_list_t p200_page_list =
{
  p200_page, sizeof(p200_page) / sizeof(olymp_pagesize_t)
};

static const olymp_printsize_t p200_printsize[] =
{
  { "320x320", "ISOB7", 960, 1280},
  { "320x320", "Custom", 960, 1280},
};

static const olymp_printsize_list_t p200_printsize_list =
{
  p200_printsize, sizeof(p200_printsize) / sizeof(olymp_printsize_t)
};

static void p200_printer_init_func(stp_vars_t *v)
{
  stp_zfwrite("S000001\0S010001\1", 1, 16, v);
}

static void p200_plane_init_func(stp_vars_t *v)
{
  stp_zprintf(v, "P0%d9999", 3 - privdata.plane+1 );
  stp_put32_be(privdata.xsize * privdata.ysize, v);
}

static void p200_printer_end_func(stp_vars_t *v)
{
  stp_zprintf(v, "P000001\1");
}

static const char p200_adj_any[] =
  "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
  "<gimp-print>\n"
  "<curve wrap=\"nowrap\" type=\"spline\" gamma=\"0\">\n"
  "<sequence count=\"33\" lower-bound=\"0\" upper-bound=\"1\">\n"
  "0.000000 0.039216 0.078431 0.117647 0.152941 0.192157 0.231373 0.266667\n"
  "0.301961 0.341176 0.376471 0.411765 0.447059 0.482353 0.513725 0.549020\n"
  "0.580392 0.615686 0.647059 0.678431 0.709804 0.741176 0.768627 0.796078\n"
  "0.827451 0.854902 0.878431 0.905882 0.929412 0.949020 0.972549 0.988235\n"
  "1.000000\n"
  "</sequence>\n"
  "</curve>\n"
  "</gimp-print>\n";


/* Olympus P-300 series */
static const olymp_resolution_t p300_res[] =
{
  { "306x306", 306, 306},
  { "153x153", 153, 153},
};

static const olymp_resolution_list_t p300_res_list =
{
  p300_res, sizeof(p300_res) / sizeof(olymp_resolution_t)
};

static const olymp_pagesize_t p300_page[] =
{
  { "A6", NULL, -1, -1, 28, 28, 48, 48},
  { "Custom", NULL, -1, -1, 28, 28, 48, 48},
};

static const olymp_pagesize_list_t p300_page_list =
{
  p300_page, sizeof(p300_page) / sizeof(olymp_pagesize_t)
};

static const olymp_printsize_t p300_printsize[] =
{
  { "306x306", "A6", 1024, 1376},
  { "153x153", "A6",  512,  688},
  { "306x306", "Custom", 1024, 1376},
  { "153x153", "Custom", 1024, 1376},
};

static const olymp_printsize_list_t p300_printsize_list =
{
  p300_printsize, sizeof(p300_printsize) / sizeof(olymp_printsize_t)
};

static void p300_printer_init_func(stp_vars_t *v)
{
  stp_zfwrite("\033\033\033C\033N\1\033F\0\1\033MS\xff\xff\xff"
	      "\033Z", 1, 19, v);
  stp_put16_be(privdata.xdpi, v);
  stp_put16_be(privdata.ydpi, v);
}

static void p300_plane_end_func(stp_vars_t *v)
{
  const char *c = "CMY";
  stp_zprintf(v, "\033\033\033P%cS", c[privdata.plane-1]);
  stp_deprintf(STP_DBG_OLYMPUS, "olympus: p300_plane_end_func: %c\n",
	c[privdata.plane-1]);
}

static void p300_block_init_func(stp_vars_t *v)
{
  const char *c = "CMY";
  stp_zprintf(v, "\033\033\033W%c", c[privdata.plane-1]);
  stp_put16_be(privdata.block_min_y, v);
  stp_put16_be(privdata.block_min_x, v);
  stp_put16_be(privdata.block_max_y, v);
  stp_put16_be(privdata.block_max_x, v);

  stp_deprintf(STP_DBG_OLYMPUS, "olympus: p300_block_init_func: %d-%dx%d-%d\n",
	privdata.block_min_x, privdata.block_max_x,
	privdata.block_min_y, privdata.block_max_y);
}

static const char p300_adj_cyan[] =
  "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
  "<gimp-print>\n"
  "<curve wrap=\"nowrap\" type=\"spline\" gamma=\"0\">\n"
  "<sequence count=\"32\" lower-bound=\"0\" upper-bound=\"1\">\n"
  "0.078431 0.211765 0.250980 0.282353 0.309804 0.333333 0.352941 0.368627\n"
  "0.388235 0.403922 0.427451 0.443137 0.458824 0.478431 0.498039 0.513725\n"
  "0.529412 0.545098 0.556863 0.576471 0.592157 0.611765 0.627451 0.647059\n"
  "0.666667 0.682353 0.701961 0.713725 0.725490 0.729412 0.733333 0.737255\n"
  "</sequence>\n"
  "</curve>\n"
  "</gimp-print>\n";

static const char p300_adj_magenta[] =
  "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
  "<gimp-print>\n"
  "<curve wrap=\"nowrap\" type=\"spline\" gamma=\"0\">\n"
  "<sequence count=\"32\" lower-bound=\"0\" upper-bound=\"1\">\n"
  "0.047059 0.211765 0.250980 0.278431 0.305882 0.333333 0.349020 0.364706\n"
  "0.380392 0.396078 0.415686 0.435294 0.450980 0.466667 0.482353 0.498039\n"
  "0.513725 0.525490 0.541176 0.556863 0.572549 0.592157 0.611765 0.631373\n"
  "0.650980 0.670588 0.694118 0.705882 0.721569 0.741176 0.745098 0.756863\n"
  "</sequence>\n"
  "</curve>\n"
  "</gimp-print>\n";

static const char p300_adj_yellow[] =
  "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
  "<gimp-print>\n"
  "<curve wrap=\"nowrap\" type=\"spline\" gamma=\"0\">\n"
  "<sequence count=\"32\" lower-bound=\"0\" upper-bound=\"1\">\n"
  "0.047059 0.117647 0.203922 0.250980 0.274510 0.301961 0.321569 0.337255\n"
  "0.352941 0.364706 0.380392 0.396078 0.407843 0.423529 0.439216 0.450980\n"
  "0.466667 0.482353 0.498039 0.513725 0.533333 0.552941 0.572549 0.596078\n"
  "0.615686 0.635294 0.650980 0.666667 0.682353 0.690196 0.701961 0.713725\n"
  "</sequence>\n"
  "</curve>\n"
  "</gimp-print>\n";


/* Olympus P-400 series */
static const olymp_resolution_t res_314dpi[] =
{
  { "314x314", 314, 314},
};

static const olymp_resolution_list_t res_314dpi_list =
{
  res_314dpi, sizeof(res_314dpi) / sizeof(olymp_resolution_t)
};

static const olymp_pagesize_t p400_page[] =
{
  { "A4", NULL, -1, -1, 22, 22, 54, 54},
  { "c8x10", "A5 wide", -1, -1, 58, 59, 84, 85},
  { "C6", "2 Postcards (A4)", -1, -1, 9, 9, 9, 9},
  { "Custom", NULL, -1, -1, 22, 22, 54, 54},
};

static const olymp_pagesize_list_t p400_page_list =
{
  p400_page, sizeof(p400_page) / sizeof(olymp_pagesize_t)
};

static const olymp_printsize_t p400_printsize[] =
{
  { "314x314", "A4", 2400, 3200},
  { "314x314", "c8x10", 2000, 2400},
  { "314x314", "C6", 1328, 1920},
  { "314x314", "Custom", 2400, 3200},
};

static const olymp_printsize_list_t p400_printsize_list =
{
  p400_printsize, sizeof(p400_printsize) / sizeof(olymp_printsize_t)
};

static void p400_printer_init_func(stp_vars_t *v)
{
  int wide = (strcmp(privdata.pagesize, "c8x10") == 0
		  || strcmp(privdata.pagesize, "C6") == 0);

  stp_zprintf(v, "\033ZQ"); stp_zfwrite(zero, 1, 61, v);
  stp_zprintf(v, "\033FP"); stp_zfwrite(zero, 1, 61, v);
  stp_zprintf(v, "\033ZF");
  stp_putc((wide ? '\x40' : '\x00'), v); stp_zfwrite(zero, 1, 60, v);
  stp_zprintf(v, "\033ZS");
  if (wide)
    {
      stp_put16_be(privdata.ysize, v);
      stp_put16_be(privdata.xsize, v);
    }
  else
    {
      stp_put16_be(privdata.xsize, v);
      stp_put16_be(privdata.ysize, v);
    }
  stp_zfwrite(zero, 1, 57, v);
  stp_zprintf(v, "\033ZP"); stp_zfwrite(zero, 1, 61, v);
}

static void p400_plane_init_func(stp_vars_t *v)
{
  stp_zprintf(v, "\033ZC"); stp_zfwrite(zero, 1, 61, v);
}

static void p400_plane_end_func(stp_vars_t *v)
{
  stp_zprintf(v, "\033P"); stp_zfwrite(zero, 1, 62, v);
}

static void p400_block_init_func(stp_vars_t *v)
{
  int wide = (strcmp(privdata.pagesize, "c8x10") == 0
		  || strcmp(privdata.pagesize, "C6") == 0);

  stp_zprintf(v, "\033Z%c", '3' - privdata.plane + 1);
  if (wide)
    {
      stp_put16_be(privdata.ysize - privdata.block_max_y - 1, v);
      stp_put16_be(privdata.xsize - privdata.block_max_x - 1, v);
      stp_put16_be(privdata.block_max_y - privdata.block_min_y + 1, v);
      stp_put16_be(privdata.block_max_x - privdata.block_min_x + 1, v);
    }
  else
    {
      stp_put16_be(privdata.block_min_x, v);
      stp_put16_be(privdata.block_min_y, v);
      stp_put16_be(privdata.block_max_x - privdata.block_min_x + 1, v);
      stp_put16_be(privdata.block_max_y - privdata.block_min_y + 1, v);
    }
  stp_zfwrite(zero, 1, 53, v);
}

static const char p400_adj_cyan[] =
  "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
  "<gimp-print>\n"
  "<curve wrap=\"nowrap\" type=\"spline\" gamma=\"0\">\n"
  "<sequence count=\"32\" lower-bound=\"0\" upper-bound=\"1\">\n"
  "0.003922 0.031373 0.058824 0.090196 0.125490 0.156863 0.184314 0.219608\n"
  "0.250980 0.278431 0.309804 0.341176 0.376471 0.403922 0.439216 0.470588\n"
  "0.498039 0.517647 0.533333 0.545098 0.564706 0.576471 0.596078 0.615686\n"
  "0.627451 0.647059 0.658824 0.678431 0.690196 0.705882 0.721569 0.737255\n"
  "</sequence>\n"
  "</curve>\n"
  "</gimp-print>\n";
  
static const char p400_adj_magenta[] =
  "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
  "<gimp-print>\n"
  "<curve wrap=\"nowrap\" type=\"spline\" gamma=\"0\">\n"
  "<sequence count=\"32\" lower-bound=\"0\" upper-bound=\"1\">\n"
  "0.003922 0.031373 0.062745 0.098039 0.125490 0.156863 0.188235 0.215686\n"
  "0.250980 0.282353 0.309804 0.345098 0.376471 0.407843 0.439216 0.470588\n"
  "0.501961 0.521569 0.549020 0.572549 0.592157 0.619608 0.643137 0.662745\n"
  "0.682353 0.713725 0.737255 0.756863 0.784314 0.807843 0.827451 0.850980\n"
  "</sequence>\n"
  "</curve>\n"
  "</gimp-print>\n";
  
static const char p400_adj_yellow[] =
  "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
  "<gimp-print>\n"
  "<curve wrap=\"nowrap\" type=\"spline\" gamma=\"0\">\n"
  "<sequence count=\"32\" lower-bound=\"0\" upper-bound=\"1\">\n"
  "0.003922 0.027451 0.054902 0.090196 0.121569 0.156863 0.184314 0.215686\n"
  "0.250980 0.282353 0.309804 0.345098 0.372549 0.400000 0.435294 0.466667\n"
  "0.498039 0.525490 0.552941 0.580392 0.607843 0.631373 0.658824 0.678431\n"
  "0.698039 0.725490 0.760784 0.784314 0.811765 0.839216 0.866667 0.890196\n"
  "</sequence>\n"
  "</curve>\n"
  "</gimp-print>\n";


/* Olympus P-440 series */
static const olymp_pagesize_t p440_page[] =
{
  { "A4", NULL, -1, -1, 10, 9, 54, 54},
  { "c8x10", "A5 wide", -1, -1, 58, 59, 72, 72},
  { "C6", "2 Postcards (A4)", -1, -1, 9, 9, 9, 9},
  { "w255h581", "A6 wide", -1, -1, 25, 25, 25, 24},
  { "Custom", NULL, -1, -1, 22, 22, 54, 54},
};

static const olymp_pagesize_list_t p440_page_list =
{
  p440_page, sizeof(p440_page) / sizeof(olymp_pagesize_t)
};

static const olymp_printsize_t p440_printsize[] =
{
  { "314x314", "A4", 2508, 3200},
  { "314x314", "c8x10", 2000, 2508},
  { "314x314", "C6", 1328, 1920},
  { "314x314", "w255h581", 892, 2320},
  { "314x314", "Custom", 2508, 3200},
};

static const olymp_printsize_list_t p440_printsize_list =
{
  p440_printsize, sizeof(p440_printsize) / sizeof(olymp_printsize_t)
};

static void p440_printer_init_func(stp_vars_t *v)
{
  int wide = ! (strcmp(privdata.pagesize, "A4") == 0
		  || strcmp(privdata.pagesize, "Custom") == 0);

  stp_zprintf(v, "\033FP"); stp_zfwrite(zero, 1, 61, v);
  stp_zprintf(v, "\033Y");
  stp_zfwrite((privdata.laminate->seq).data, 1,
		  (privdata.laminate->seq).bytes, v); /* laminate */ 
  stp_zfwrite(zero, 1, 61, v);
  stp_zprintf(v, "\033FC"); stp_zfwrite(zero, 1, 61, v);
  stp_zprintf(v, "\033ZF");
  stp_putc((wide ? '\x40' : '\x00'), v); stp_zfwrite(zero, 1, 60, v);
  stp_zprintf(v, "\033N\1"); stp_zfwrite(zero, 1, 61, v);
  stp_zprintf(v, "\033ZS");
  if (wide)
    {
      stp_put16_be(privdata.ysize, v);
      stp_put16_be(privdata.xsize, v);
    }
  else
    {
      stp_put16_be(privdata.xsize, v);
      stp_put16_be(privdata.ysize, v);
    }
  stp_zfwrite(zero, 1, 57, v);
  if (strcmp(privdata.pagesize, "C6") == 0)
    {
      stp_zprintf(v, "\033ZC"); stp_zfwrite(zero, 1, 61, v);
    }
}

static void p440_printer_end_func(stp_vars_t *v)
{
  stp_zprintf(v, "\033P"); stp_zfwrite(zero, 1, 62, v);
}

static void p440_block_init_func(stp_vars_t *v)
{
  int wide = ! (strcmp(privdata.pagesize, "A4") == 0
		  || strcmp(privdata.pagesize, "Custom") == 0);

  stp_zprintf(v, "\033ZT");
  if (wide)
    {
      stp_put16_be(privdata.ysize - privdata.block_max_y - 1, v);
      stp_put16_be(privdata.xsize - privdata.block_max_x - 1, v);
      stp_put16_be(privdata.block_max_y - privdata.block_min_y + 1, v);
      stp_put16_be(privdata.block_max_x - privdata.block_min_x + 1, v);
    }
  else
    {
      stp_put16_be(privdata.block_min_x, v);
      stp_put16_be(privdata.block_min_y, v);
      stp_put16_be(privdata.block_max_x - privdata.block_min_x + 1, v);
      stp_put16_be(privdata.block_max_y - privdata.block_min_y + 1, v);
    }
  stp_zfwrite(zero, 1, 53, v);
}

static void p440_block_end_func(stp_vars_t *v)
{
  int pad = (64 - (((privdata.block_max_x - privdata.block_min_x + 1)
	  * (privdata.block_max_y - privdata.block_min_y + 1) * 3) % 64)) % 64;
  stp_deprintf(STP_DBG_OLYMPUS,
		  "olympus: max_x %d min_x %d max_y %d min_y %d\n",
  		  privdata.block_max_x, privdata.block_min_x,
	  	  privdata.block_max_y, privdata.block_min_y);
  stp_deprintf(STP_DBG_OLYMPUS, "olympus: olympus-p440 padding=%d\n", pad);
  stp_zfwrite(zero, 1, pad, v);
}


/* Canon CP-100 series */
static const olymp_pagesize_t cpx00_page[] =
{
  { "Postcard", "Postcard 148x100mm", -1, -1, 13, 13, 16, 18},
  { "w253h337", "CP_L 89x119mm", -1, -1, 13, 13, 15, 15},
  { "w244h155", "Card 54x86mm", -1, -1, 15, 15, 13, 13},
  { "Custom", NULL, -1, -1, 13, 13, 16, 18},
};

static const olymp_pagesize_list_t cpx00_page_list =
{
  cpx00_page, sizeof(cpx00_page) / sizeof(olymp_pagesize_t)
};

static const olymp_printsize_t cpx00_printsize[] =
{
  { "314x314", "Postcard", 1232, 1808},
  { "314x314", "w253h337", 1100, 1456},
  { "314x314", "w244h155", 1040, 672},
  { "314x314", "Custom", 1232, 1808},
};

static const olymp_printsize_list_t cpx00_printsize_list =
{
  cpx00_printsize, sizeof(cpx00_printsize) / sizeof(olymp_printsize_t)
};

static void cpx00_printer_init_func(stp_vars_t *v)
{
  char pg = (strcmp(privdata.pagesize, "Postcard") == 0 ? '\1' :
		(strcmp(privdata.pagesize, "w253h337") == 0 ? '\2' :
		(strcmp(privdata.pagesize, "w244h155") == 0 ? '\3' :
		(strcmp(privdata.pagesize, "w283h566") == 0 ? '\4' : 
		 '\1' ))));

  stp_put16_be(0x4000, v);
  stp_putc('\0', v);
  stp_putc(pg, v);
  stp_zfwrite(zero, 1, 8, v);
}

static void cpx00_plane_init_func(stp_vars_t *v)
{
  stp_put16_be(0x4001, v);
  stp_put16_le(3 - privdata.plane, v);
  stp_put32_le(privdata.xsize * privdata.ysize, v);
  stp_zfwrite(zero, 1, 4, v);
}

static const char cpx00_adj_cyan[] =
  "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
  "<gimp-print>\n"
  "<curve wrap=\"nowrap\" type=\"spline\" gamma=\"0\">\n"
  "<sequence count=\"32\" lower-bound=\"0\" upper-bound=\"1\">\n"
  "0.000000 0.035294 0.070588 0.101961 0.117647 0.168627 0.180392 0.227451\n"
  "0.258824 0.286275 0.317647 0.341176 0.376471 0.411765 0.427451 0.478431\n"
  "0.505882 0.541176 0.576471 0.611765 0.654902 0.678431 0.705882 0.737255\n"
  "0.764706 0.792157 0.811765 0.839216 0.862745 0.894118 0.909804 0.925490\n"
  "</sequence>\n"
  "</curve>\n"
  "</gimp-print>\n";
  
static const char cpx00_adj_magenta[] =
  "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
  "<gimp-print>\n"
  "<curve wrap=\"nowrap\" type=\"spline\" gamma=\"0\">\n"
  "<sequence count=\"32\" lower-bound=\"0\" upper-bound=\"1\">\n"
  "0.011765 0.019608 0.035294 0.047059 0.054902 0.101961 0.133333 0.156863\n"
  "0.192157 0.235294 0.274510 0.321569 0.360784 0.403922 0.443137 0.482353\n"
  "0.521569 0.549020 0.584314 0.619608 0.658824 0.705882 0.749020 0.792157\n"
  "0.831373 0.890196 0.933333 0.964706 0.988235 0.992157 0.992157 0.996078\n"
  "</sequence>\n"
  "</curve>\n"
  "</gimp-print>\n";
  
static const char cpx00_adj_yellow[] =
  "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
  "<gimp-print>\n"
  "<curve wrap=\"nowrap\" type=\"spline\" gamma=\"0\">\n"
  "<sequence count=\"32\" lower-bound=\"0\" upper-bound=\"1\">\n"
  "0.003922 0.015686 0.015686 0.023529 0.027451 0.054902 0.094118 0.129412\n"
  "0.180392 0.219608 0.250980 0.286275 0.317647 0.341176 0.388235 0.427451\n"
  "0.470588 0.509804 0.552941 0.596078 0.627451 0.682353 0.768627 0.796078\n"
  "0.890196 0.921569 0.949020 0.968627 0.984314 0.992157 0.992157 1.000000\n"
  "</sequence>\n"
  "</curve>\n"
  "</gimp-print>\n";


/* Canon CP-220 series */
static const olymp_pagesize_t cp220_page[] =
{
  { "Postcard", "Postcard 148x100mm", -1, -1, 13, 13, 16, 18},
  { "w253h337", "CP_L 89x119mm", -1, -1, 13, 13, 15, 15},
  { "w244h155", "Card 54x86mm", -1, -1, 15, 15, 13, 13},
  { "w283h566", "Wide 200x100mm", -1, -1, 13, 13, 20, 20},
  { "Custom", NULL, -1, -1, 13, 13, 16, 18},
};

static const olymp_pagesize_list_t cp220_page_list =
{
  cp220_page, sizeof(cp220_page) / sizeof(olymp_pagesize_t)
};

static const olymp_printsize_t cp220_printsize[] =
{
  { "314x314", "Postcard", 1232, 1808},
  { "314x314", "w253h337", 1100, 1456},
  { "314x314", "w244h155", 1040, 672},
  { "314x314", "w283h566", 1232, 2416},
  { "314x314", "Custom", 1232, 1808},
};

static const olymp_printsize_list_t cp220_printsize_list =
{
  cp220_printsize, sizeof(cp220_printsize) / sizeof(olymp_printsize_t)
};


/* Sony UP-DP10 */
static const olymp_resolution_t updp10_res[] =
{
  { "300x300", 300, 300},
};

static const olymp_resolution_list_t updp10_res_list =
{
  updp10_res, sizeof(updp10_res) / sizeof(olymp_resolution_t)
};

static const olymp_pagesize_t updp10_page[] =
{
  { "w288h432", "UPC-10P23 (2:3)", -1, -1, 12, 12, 18, 18},
  { "w288h387", "UPC-10P34 (3:4)", -1, -1, 12, 12, 16, 16},
  { "w288h432", "UPC-10S01 (Sticker)", -1, -1, 12, 12, 18, 18},
  { "Custom", NULL, -1, -1, 12, 12, 0, 0},
};

static const olymp_pagesize_list_t updp10_page_list =
{
  updp10_page, sizeof(updp10_page) / sizeof(olymp_pagesize_t)
};

static const olymp_printsize_t updp10_printsize[] =
{
  { "300x300", "w288h432", 1200, 1800},
  { "300x300", "w288h387", 1200, 1600},
  { "300x300", "Custom", 1200, 1800},
};

static const olymp_printsize_list_t updp10_printsize_list =
{
  updp10_printsize, sizeof(updp10_printsize) / sizeof(olymp_printsize_t)
};

static void updp10_printer_init_func(stp_vars_t *v)
{
  stp_zfwrite("\x98\xff\xff\xff\xff\xff\xff\xff"
	      "\x14\x00\x00\x00\x1b\x15\x00\x00"
	      "\x00\x0d\x00\x00\x00\x00\x00\xc7"
	      "\x00\x00\x00\x00", 1, 28, v);
  stp_put16_be(privdata.xsize, v);
  stp_put16_be(privdata.ysize, v);
  stp_put32_le(privdata.xsize*privdata.ysize*3+11, v);
  stp_zfwrite("\x1b\xea\x00\x00\x00\x00", 1, 6, v);
  stp_put32_be(privdata.xsize*privdata.ysize*3, v);
  stp_zfwrite("\x00", 1, 1, v);
}

static void updp10_printer_end_func(stp_vars_t *v)
{
	stp_zfwrite("\x12\x00\x00\x00\x1b\xe1\x00\x00"
		    "\x00\xb0\x00\x00\04", 1, 13, v);
	stp_zfwrite((privdata.laminate->seq).data, 1,
			(privdata.laminate->seq).bytes, v); /*laminate pattern*/
	stp_zfwrite("\x00\x00\x00\x00" , 1, 4, v);
        stp_put16_be(privdata.ysize, v);
        stp_put16_be(privdata.xsize, v);
	stp_zfwrite("\xff\xff\xff\xff\x09\x00\x00\x00"
		    "\x1b\xee\x00\x00\x00\x02\x00\x00"
		    "\x01\x07\x00\x00\x00\x1b\x0a\x00"
		    "\x00\x00\x00\x00\xfd\xff\xff\xff"
		    "\xff\xff\xff\xff\xf8\xff\xff\xff"
		    , 1, 40, v);
}

static const laminate_t updp10_laminate[] =
{
  {"Glossy",  N_("Glossy"),  {1, "\x00"}},
  {"Texture", N_("Texture"), {1, "\x08"}},
  {"Matte",   N_("Matte"),   {1, "\x0c"}},
};

static const laminate_list_t updp10_laminate_list =
{
  updp10_laminate, sizeof(updp10_laminate) / sizeof(laminate_t)
};

static const char updp10_adj_cyan[] =
  "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
  "<gimp-print>\n"
  "<curve wrap=\"nowrap\" type=\"spline\" gamma=\"0\">\n"
  "<sequence count=\"33\" lower-bound=\"0\" upper-bound=\"1\">\n"
  "0.113725 0.188235 0.247059 0.286275 0.317647 0.345098 0.368627 0.384314\n"
  "0.400000 0.407843 0.423529 0.439216 0.450980 0.466667 0.482353 0.498039\n"
  "0.509804 0.525490 0.545098 0.560784 0.580392 0.596078 0.619608 0.643137\n"
  "0.662745 0.686275 0.709804 0.729412 0.756863 0.780392 0.811765 0.843137\n"
  "1.000000\n"
  "</sequence>\n"
  "</curve>\n"
  "</gimp-print>\n";
  
static const char updp10_adj_magenta[] =
  "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
  "<gimp-print>\n"
  "<curve wrap=\"nowrap\" type=\"spline\" gamma=\"0\">\n"
  "<sequence count=\"33\" lower-bound=\"0\" upper-bound=\"1\">\n"
  "0.105882 0.211765 0.286275 0.333333 0.364706 0.388235 0.403922 0.415686\n"
  "0.427451 0.439216 0.450980 0.462745 0.478431 0.494118 0.505882 0.521569\n"
  "0.537255 0.552941 0.568627 0.584314 0.600000 0.619608 0.643137 0.662745\n"
  "0.682353 0.709804 0.733333 0.760784 0.792157 0.823529 0.858824 0.890196\n"
  "1.000000\n"
  "</sequence>\n"
  "</curve>\n"
  "</gimp-print>\n";
  
static const char updp10_adj_yellow[] =
  "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
  "<gimp-print>\n"
  "<curve wrap=\"nowrap\" type=\"spline\" gamma=\"0\">\n"
  "<sequence count=\"33\" lower-bound=\"0\" upper-bound=\"1\">\n"
  "0.101961 0.160784 0.196078 0.227451 0.243137 0.254902 0.266667 0.286275\n"
  "0.309804 0.337255 0.368627 0.396078 0.423529 0.443137 0.462745 0.478431\n"
  "0.501961 0.517647 0.537255 0.556863 0.576471 0.596078 0.619608 0.643137\n"
  "0.666667 0.690196 0.709804 0.737255 0.760784 0.780392 0.796078 0.803922\n"
  "1.000000\n"
  "</sequence>\n"
  "</curve>\n"
  "</gimp-print>\n";


/* Fujifilm CX-400 */
static const olymp_resolution_t cx400_res[] =
{
  { "317x316", 317, 316},
};

static const olymp_resolution_list_t cx400_res_list =
{
  cx400_res, sizeof(cx400_res) / sizeof(olymp_resolution_t)
};

static const olymp_pagesize_t cx400_page[] =
{
  { "w288h432", NULL, -1, -1, 23, 23, 28, 28},
  { "w288h387", "4x5 3/8 (Digital Camera 3:4)", -1, -1, 23, 23, 27, 26},
  { "w288h504", NULL, -1, -1, 23, 23, 23, 22},
  { "Custom", NULL, -1, -1, 0, 0, 0, 0},
};

static const olymp_pagesize_list_t cx400_page_list =
{
  cx400_page, sizeof(cx400_page) / sizeof(olymp_pagesize_t)
};

static const olymp_printsize_t cx400_printsize[] =
{
  { "317x316", "w288h387", 1268, 1658},
  { "317x316", "w288h432", 1268, 1842},
  { "317x316", "w288h504", 1268, 2208},
  { "317x316", "Custom", 1268, 1842},
};

static const olymp_printsize_list_t cx400_printsize_list =
{
  cx400_printsize, sizeof(cx400_printsize) / sizeof(olymp_printsize_t)
};

static void cx400_printer_init_func(stp_vars_t *v)
{
  char pg = '\0';
  const char *pname = "XXXXXX";		  				

  stp_deprintf(STP_DBG_OLYMPUS,
	"olympus: fuji driver %s\n", stp_get_driver(v));
  if (strcmp(stp_get_driver(v),"fujifilm-cx400") == 0)
    pname = "NX1000";
  else if (strcmp(stp_get_driver(v),"fujifilm-cx550") == 0)
    pname = "QX200\0";

  stp_zfwrite("FUJIFILM", 1, 8, v);
  stp_zfwrite(pname, 1, 6, v);
  stp_putc('\0', v);
  stp_put16_le(privdata.xsize, v);
  stp_put16_le(privdata.ysize, v);
  if (strcmp(privdata.pagesize,"w288h504") == 0)
    pg = '\x0d';
  else if (strcmp(privdata.pagesize,"w288h432") == 0)
    pg = '\x0c';
  else if (strcmp(privdata.pagesize,"w288h387") == 0)
    pg = '\x0b';
  stp_putc(pg, v);
  stp_zfwrite("\x00\x00\x00\x00\x00\x01\x00\x01\x00\x00\x00\x00"
		  "\x00\x00\x2d\x00\x00\x00\x00", 1, 19, v);
  stp_zfwrite("FUJIFILM", 1, 8, v);
  stp_zfwrite(pname, 1, 6, v);
  stp_putc('\1', v);
}
  
static const olymp_resolution_t all_resolutions[] =
{
  { "306x306", 306, 306},
  { "153x153", 153, 153},
  { "314x314", 314, 314},
  { "300x300", 300, 300},
  { "317x316", 317, 316},
  { "320x320", 320, 320},
};

static const olymp_resolution_list_t all_res_list =
{
  all_resolutions, sizeof(all_resolutions) / sizeof(olymp_resolution_t)
};

static const olympus_cap_t olympus_model_capabilities[] =
{
  { /* Olympus P-10 */
    2, 		
    &rgb_ink_list,
    &res_320dpi_list,
    &p10_page_list,
    &p10_printsize_list,
    OLYMPUS_INTERLACE_PLANE,
    1848,
    OLYMPUS_FEATURE_FULL_WIDTH | OLYMPUS_FEATURE_FULL_HEIGHT,
    &p10_printer_init_func, &p10_printer_end_func,
    NULL, NULL, 
    &p10_block_init_func, NULL,
    NULL, NULL, NULL,	/* color profile/adjustment is built into printer */
    &p10_laminate_list,
  },
  { /* Olympus P-200 */
    4, 		
    &ymc_ink_list,
    &res_320dpi_list,
    &p200_page_list,
    &p200_printsize_list,
    OLYMPUS_INTERLACE_PLANE,
    1280,
    OLYMPUS_FEATURE_FULL_WIDTH | OLYMPUS_FEATURE_BLOCK_ALIGN,
    &p200_printer_init_func, &p200_printer_end_func,
    &p200_plane_init_func, NULL,
    NULL, NULL,
    p200_adj_any, p200_adj_any, p200_adj_any,
    NULL,
  },
  { /* Olympus P-300 */
    0, 		
    &ymc_ink_list,
    &p300_res_list,
    &p300_page_list,
    &p300_printsize_list,
    OLYMPUS_INTERLACE_PLANE,
    16,
    OLYMPUS_FEATURE_FULL_WIDTH | OLYMPUS_FEATURE_BLOCK_ALIGN,
    &p300_printer_init_func, NULL,
    NULL, &p300_plane_end_func,
    &p300_block_init_func, NULL,
    p300_adj_cyan, p300_adj_magenta, p300_adj_yellow,
    NULL,
  },
  { /* Olympus P-400 */
    1,
    &ymc_ink_list,
    &res_314dpi_list,
    &p400_page_list,
    &p400_printsize_list,
    OLYMPUS_INTERLACE_PLANE,
    180,
    OLYMPUS_FEATURE_FULL_WIDTH | OLYMPUS_FEATURE_FULL_HEIGHT,
    &p400_printer_init_func, NULL,
    &p400_plane_init_func, &p400_plane_end_func,
    &p400_block_init_func, NULL,
    p400_adj_cyan, p400_adj_magenta, p400_adj_yellow,
    NULL,
  },
  { /* Olympus P-440 */
    3,
    &bgr_ink_list,
    &res_314dpi_list,
    &p440_page_list,
    &p440_printsize_list,
    OLYMPUS_INTERLACE_NONE,
    128,
    OLYMPUS_FEATURE_FULL_WIDTH | OLYMPUS_FEATURE_FULL_HEIGHT,
    &p440_printer_init_func, &p440_printer_end_func,
    NULL, NULL,
    &p440_block_init_func, &p440_block_end_func,
    NULL, NULL, NULL,	/* color profile/adjustment is built into printer */
    &p10_laminate_list,
  },
  { /* Canon CP-100, CP-200, CP-300 */
    1000,
    &ymc_ink_list,
    &res_314dpi_list,
    &cpx00_page_list,
    &cpx00_printsize_list,
    OLYMPUS_INTERLACE_PLANE,
    1808,
    OLYMPUS_FEATURE_FULL_WIDTH | OLYMPUS_FEATURE_FULL_HEIGHT
      | OLYMPUS_FEATURE_BORDERLESS | OLYMPUS_FEATURE_WHITE_BORDER,
    &cpx00_printer_init_func, NULL,
    &cpx00_plane_init_func, NULL,
    NULL, NULL,
    cpx00_adj_cyan, cpx00_adj_magenta, cpx00_adj_yellow,
    NULL,
  },
  { /* Canon CP-220, CP-330 */
    1001,
    &ymc_ink_list,
    &res_314dpi_list,
    &cp220_page_list,
    &cp220_printsize_list,
    OLYMPUS_INTERLACE_PLANE,
    1808,
    OLYMPUS_FEATURE_FULL_WIDTH | OLYMPUS_FEATURE_FULL_HEIGHT
      | OLYMPUS_FEATURE_BORDERLESS | OLYMPUS_FEATURE_WHITE_BORDER,
    &cpx00_printer_init_func, NULL,
    &cpx00_plane_init_func, NULL,
    NULL, NULL,
    cpx00_adj_cyan, cpx00_adj_magenta, cpx00_adj_yellow,
    NULL,
  },
  { /* Sony UP-DP10  */
    2000,
    &cmy_ink_list,
    &updp10_res_list,
    &updp10_page_list,
    &updp10_printsize_list,
    OLYMPUS_INTERLACE_NONE,
    1800,
    OLYMPUS_FEATURE_FULL_WIDTH | OLYMPUS_FEATURE_FULL_HEIGHT
      | OLYMPUS_FEATURE_BORDERLESS,
    &updp10_printer_init_func, &updp10_printer_end_func,
    NULL, NULL,
    NULL, NULL,
    updp10_adj_cyan, updp10_adj_magenta, updp10_adj_yellow,
    &updp10_laminate_list,
  },
  { /* Fujifilm Printpix CX-400  */
    3000,
    &rgb_ink_list,
    &cx400_res_list,
    &cx400_page_list,
    &cx400_printsize_list,
    OLYMPUS_INTERLACE_NONE,
    2208,
    OLYMPUS_FEATURE_FULL_WIDTH | OLYMPUS_FEATURE_FULL_HEIGHT
      | OLYMPUS_FEATURE_BORDERLESS,
    &cx400_printer_init_func, NULL,
    NULL, NULL,
    NULL, NULL,
    NULL, NULL, NULL,	/* color profile/adjustment is built into printer */
    NULL,
  },
  { /* Fujifilm Printpix CX-550  */
    3001,
    &rgb_ink_list,
    &cx400_res_list,
    &cx400_page_list,
    &cx400_printsize_list,
    OLYMPUS_INTERLACE_NONE,
    2208,
    OLYMPUS_FEATURE_FULL_WIDTH | OLYMPUS_FEATURE_FULL_HEIGHT
      | OLYMPUS_FEATURE_BORDERLESS,
    &cx400_printer_init_func, NULL,
    NULL, NULL,
    NULL, NULL,
    NULL, NULL, NULL,	/* color profile/adjustment is built into printer */
    NULL,
  },
};

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
    "Laminate", N_("Laminate Pattern"), N_("Advanced Printer Setup"),
    N_("Laminate Pattern"),
    STP_PARAMETER_TYPE_STRING_LIST, STP_PARAMETER_CLASS_FEATURE,
    STP_PARAMETER_LEVEL_BASIC, 1, 0, -1, 1, 0
  },
  {
    "Borderless", N_("Borderless"), N_("Advanced Printer Setup"),
    N_("Print without borders"),
    STP_PARAMETER_TYPE_BOOLEAN, STP_PARAMETER_CLASS_FEATURE,
    STP_PARAMETER_LEVEL_BASIC, 1, 0, -1, 1, 0
  },
  {
    "PrintingMode", N_("Printing Mode"), N_("Core Parameter"),
    N_("Printing Output Mode"),
    STP_PARAMETER_TYPE_STRING_LIST, STP_PARAMETER_CLASS_CORE,
    STP_PARAMETER_LEVEL_BASIC, 1, 1, -1, 1, 0
  },
};

static int the_parameter_count =
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
};    

static const int float_parameter_count =
sizeof(float_parameters) / sizeof(const float_param_t);

static const olympus_cap_t* olympus_get_model_capabilities(int model)
{
  int i;
  int models = sizeof(olympus_model_capabilities) / sizeof(olympus_cap_t);

  for (i=0; i<models; i++)
    {
      if (olympus_model_capabilities[i].model == model)
        return &(olympus_model_capabilities[i]);
    }
  stp_deprintf(STP_DBG_OLYMPUS,
  	"olympus: model %d not found in capabilities list.\n", model);
  return &(olympus_model_capabilities[0]);
}

static const laminate_t* olympus_get_laminate_pattern(stp_vars_t *v)
{
  const char *lpar = stp_get_string_parameter(v, "Laminate");
  const olympus_cap_t *caps = olympus_get_model_capabilities(
		  				stp_get_model_id(v));
  const laminate_list_t *llist = caps->laminate;
  const laminate_t *l = NULL;
  int i;

  for (i = 0; i < llist->n_items; i++)
    {
      l = &(llist->item[i]);
      if (strcmp(l->name, lpar) == 0)
	 break;
    }
  return l;
}
  
static void
olympus_printsize(const stp_vars_t *v,
		   int  *width,
		   int  *height)
{
  int i;
  const char *page = stp_get_string_parameter(v, "PageSize");
  const char *resolution = stp_get_string_parameter(v, "Resolution");
  const olympus_cap_t *caps = olympus_get_model_capabilities(
		  				stp_get_model_id(v));
  const olymp_printsize_list_t *p = caps->printsize;

  for (i = 0; i < p->n_items; i++)
    {
      if (strcmp(p->item[i].res_name,resolution) == 0 &&
          strcmp(p->item[i].pagesize_name,page) == 0)
        {
          *width  = p->item[i].width_px;
	  *height = p->item[i].height_px;
          return;
        }
    }
  stp_erprintf("olympus_printsize: printsize not found (%s, %s)\n",
	       page, resolution);
}

static int
olympus_feature(const olympus_cap_t *caps, int feature)
{
  return ((caps->features & feature) == feature);
}

static stp_parameter_list_t
olympus_list_parameters(const stp_vars_t *v)
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
olympus_parameters(const stp_vars_t *v, const char *name,
	       stp_parameter_t *description)
{
  int	i;
  const olympus_cap_t *caps = olympus_get_model_capabilities(
		  				stp_get_model_id(v));

  description->p_type = STP_PARAMETER_TYPE_INVALID;
  if (name == NULL)
    return;

  description->deflt.str = NULL;
  for (i = 0; i < float_parameter_count; i++)
    if (strcmp(name, float_parameters[i].param.name) == 0)
      {
	stp_fill_parameter_settings(description,
				     &(float_parameters[i].param));
	description->deflt.dbl = float_parameters[i].defval;
	description->bounds.dbl.upper = float_parameters[i].max;
	description->bounds.dbl.lower = float_parameters[i].min;
      }

  for (i = 0; i < the_parameter_count; i++)
    if (strcmp(name, the_parameters[i].name) == 0)
      {
	stp_fill_parameter_settings(description, &(the_parameters[i]));
	break;
      }
  if (strcmp(name, "PageSize") == 0)
    {
      int default_specified = 0;
      const olymp_pagesize_list_t *p = caps->pages;
      const char* text;

      description->bounds.str = stp_string_list_create();
      for (i = 0; i < p->n_items; i++)
	{
          const stp_papersize_t *pt = stp_get_papersize_by_name(
			  p->item[i].name);

	  text = (p->item[i].text ? p->item[i].text : pt->text);
	  stp_string_list_add_string(description->bounds.str,
			  p->item[i].name, text);
	  if (! default_specified && pt && pt->width > 0 && pt->height > 0)
	    {
	      description->deflt.str = p->item[i].name;
	      default_specified = 1;
	    }
	}
      if (!default_specified)
	description->deflt.str =
	  stp_string_list_param(description->bounds.str, 0)->name;
    }
  else if (strcmp(name, "MediaType") == 0)
    {
      description->bounds.str = stp_string_list_create();
      description->is_active = 0;
    }
  else if (strcmp(name, "InputSlot") == 0)
    {
      description->bounds.str = stp_string_list_create();
      description->is_active = 0;
    }
  else if (strcmp(name, "Resolution") == 0)
    {
      char res_text[24];
      const olymp_resolution_list_t *r = caps->resolution;

      description->bounds.str = stp_string_list_create();
      for (i = 0; i < r->n_items; i++)
        {
	  sprintf(res_text, "%s DPI", r->item[i].name);
	  stp_string_list_add_string(description->bounds.str,
		r->item[i].name, _(res_text));
	}
      if (r->n_items < 1)
        description->is_active = 0;
      description->deflt.str =
	stp_string_list_param(description->bounds.str, 0)->name;
    }
  else if (strcmp(name, "InkType") == 0)
    {
      description->bounds.str = stp_string_list_create();
      for (i = 0; i < caps->inks->n_items; i++)
	stp_string_list_add_string(description->bounds.str,
			  caps->inks->item[i].name, caps->inks->item[i].name);
      description->deflt.str =
	stp_string_list_param(description->bounds.str, 0)->name;
      if (caps->inks->n_items < 2)
        description->is_active = 0;
    }
  else if (strcmp(name, "Laminate") == 0)
    {
      description->bounds.str = stp_string_list_create();
      if (caps->laminate)
        {
          const laminate_list_t *llist = caps->laminate;

          for (i = 0; i < llist->n_items; i++)
            {
              const laminate_t *l = &(llist->item[i]);
	      stp_string_list_add_string(description->bounds.str,
			  	l->name, l->text);
	    }
          description->deflt.str =
	  stp_string_list_param(description->bounds.str, 0)->name;
          description->is_active = 1;
        }
    }
  else if (strcmp(name, "Borderless") == 0)
    {
      if (olympus_feature(caps, OLYMPUS_FEATURE_BORDERLESS)) 
        description->is_active = 1;
    }
  else if (strcmp(name, "PrintingMode") == 0)
    {
      description->bounds.str = stp_string_list_create();
      stp_string_list_add_string
	(description->bounds.str, "Color", _("Color"));
      description->deflt.str =
	stp_string_list_param(description->bounds.str, 0)->name;
    }
  else
    description->is_active = 0;
}


static void
olympus_imageable_area(const stp_vars_t *v,
		   int  *left,
		   int  *right,
		   int  *bottom,
		   int  *top)
{
  int width, height;
  int i;
  const char *page = stp_get_string_parameter(v, "PageSize");
  const stp_papersize_t *pt = stp_get_papersize_by_name(page);
  const olympus_cap_t *caps = olympus_get_model_capabilities(
		  				stp_get_model_id(v));
  const olymp_pagesize_list_t *p = caps->pages;

  for (i = 0; i < p->n_items; i++)
    {
      if (strcmp(p->item[i].name,pt->name) == 0)
        {
/*
    	  if (p->item[i].width_pt >= 0)
    		  stp_set_page_width(v, p->item[i].width_pt);
    	  if (p->item[i].height_pt >= 0)
    		  stp_set_page_height(v, p->item[i].height_pt);
*/

          stp_default_media_size(v, &width, &height);
    
          
	  if (olympus_feature(caps, OLYMPUS_FEATURE_BORDERLESS)
	    && stp_get_boolean_parameter(v, "Borderless"))
            {
              *left = 0;
              *top  = 0;
              *right  = width;
              *bottom = height;
            }
	  else
	    {
              *left = p->item[i].border_pt_left;
              *top  = p->item[i].border_pt_top;
              *right  = width  - p->item[i].border_pt_right;
              *bottom = height - p->item[i].border_pt_bottom;
	    }
          break;
        }
    }
}

static void
olympus_limit(const stp_vars_t *v,			/* I */
	    int *width, int *height,
	    int *min_width, int *min_height)
{
  *width = 65535;
  *height = 65535;
  *min_width = 1;
  *min_height =	1;
}

static void
olympus_describe_resolution(const stp_vars_t *v, int *x, int *y)
{
  const char *resolution = stp_get_string_parameter(v, "Resolution");
  int i;

  *x = -1;
  *y = -1;
  if (resolution)
    {
      for (i = 0; i < all_res_list.n_items; i++)
	{
	  if (strcmp(resolution, all_res_list.item[i].name) == 0)
	    {
	      *x = all_res_list.item[i].xdpi;
	      *y = all_res_list.item[i].ydpi;
	    }
	}
    }  
  return;
}

static const char *
olympus_describe_output(const stp_vars_t *v)
{
  return "CMY";
}

static unsigned short *
olympus_get_cached_output(stp_vars_t *v,
             stp_image_t *image,
	     unsigned short **cache,
             int line, int size)
{
  unsigned zero_mask;

  stp_deprintf(STP_DBG_OLYMPUS, "olympus: get row %d", line);
  if (cache[line] == NULL)
    {
      stp_deprintf(STP_DBG_OLYMPUS, " (calling stp_color_get_row())\n");
      if (!stp_color_get_row(v, image, line, &zero_mask))
        {
          cache[line] = stp_malloc(size);
          memcpy(cache[line], stp_channel_get_output(v), size);
        }
    }
  else
    {
      stp_deprintf(STP_DBG_OLYMPUS, " (cached)\n");
    }
  return cache[line];
}

/*
 * olympus_print()
 */
static int
olympus_do_print(stp_vars_t *v, stp_image_t *image)
{
  int i, j;
  int y, min_y, max_y;			/* Looping vars */
  int min_x, max_x;
  int out_channels, out_bytes;
  unsigned short *final_out = NULL;
  unsigned char  *char_out = NULL;
  unsigned short *real_out = NULL;
  unsigned short *err_out = NULL;
  unsigned short **rows = NULL;		/* "cache" of rows read from image */
  int char_out_width;
  int status = 1;
  int ink_channels = 1;
  const char *ink_order = NULL;
  stp_curve_t   *adjustment = NULL;

  int r_errdiv, r_errmod;
  int r_errval  = 0;
  int r_errlast = -1;
  int r_errline = 0;
  int c_errdiv, c_errmod;
  int c_errval  = 0;
  int c_errlast = -1;
  int c_errcol = 0;

  const int model           = stp_get_model_id(v); 
  const char *ink_type      = stp_get_string_parameter(v, "InkType");
  const olympus_cap_t *caps = olympus_get_model_capabilities(model);
  int max_print_px_width, max_print_px_height;
  int xdpi, ydpi;	/* Resolution */

  /* image in pixels */
  int image_px_width;
  int image_px_height;

  /* output in 1/72" */
  int out_pt_width  = stp_get_width(v);
  int out_pt_height = stp_get_height(v);
  int out_pt_left   = stp_get_left(v);
  int out_pt_top    = stp_get_top(v);

  /* output in pixels */
  int out_px_width, out_px_height;
  int out_px_left, out_px_right, out_px_top, out_px_bottom;

  /* page in 1/72" */
  int page_pt_width  = stp_get_page_width(v);
  int page_pt_height = stp_get_page_height(v);
  int page_pt_left = 0;
  int page_pt_right = 0;
  int page_pt_top = 0;
  int page_pt_bottom = 0;

  /* page w/out borders in pixels (according to selected dpi) */
  int print_px_width;
  int print_px_height;
  
  int pl;
  unsigned char *zeros = NULL;

  if (!stp_verify(v))
    {
      stp_eprintf(v, _("Print options not verified; cannot print.\n"));
      return 0;
    }

  stp_image_init(image);
  image_px_width  = stp_image_width(image);
  image_px_height = stp_image_height(image);

  stp_describe_resolution(v, &xdpi, &ydpi);
  olympus_printsize(v, &max_print_px_width, &max_print_px_height);

  privdata.pagesize = stp_get_string_parameter(v, "PageSize");
  if (caps->laminate)
	  privdata.laminate = olympus_get_laminate_pattern(v);

  if (olympus_feature(caps, OLYMPUS_FEATURE_WHITE_BORDER))
    stp_default_media_size(v, &page_pt_right, &page_pt_bottom);
  else
    olympus_imageable_area(v, &page_pt_left, &page_pt_right,
	&page_pt_bottom, &page_pt_top);
  
  print_px_width  = MIN(max_print_px_width,
		  	(page_pt_right - page_pt_left) * xdpi / 72);
  print_px_height = MIN(max_print_px_height,
			(page_pt_bottom - page_pt_top) * ydpi / 72);
  out_px_width  = out_pt_width  * xdpi / 72;
  out_px_height = out_pt_height * ydpi / 72;


  /* if image size is close enough to output size send out original size */
  if (out_px_width - image_px_width > -5
      && out_px_width - image_px_width < 5
      && out_px_height - image_px_height > -5
      && out_px_height - image_px_height < 5)
    {
      out_px_width  = image_px_width;
      out_px_height = image_px_height;
    }

  out_px_width  = MIN(out_px_width, print_px_width);
  out_px_height = MIN(out_px_height, print_px_height);
  out_px_left   = MIN(((out_pt_left - page_pt_left) * xdpi / 72),
			print_px_width - out_px_width);
  out_px_top    = MIN(((out_pt_top  - page_pt_top)  * ydpi / 72),
			print_px_height - out_px_height);
  out_px_right  = out_px_left + out_px_width;
  out_px_bottom = out_px_top  + out_px_height;
  

  stp_deprintf(STP_DBG_OLYMPUS,
	      "paper (pt)   %d x %d\n"
	      "image (px)   %d x %d\n"
	      "image (pt)   %d x %d\n"
	      "* out (pt)   %d x %d\n"
	      "* out (px)   %d x %d\n"
	      "* left x top (pt) %d x %d\n"
	      "* left x top (px) %d x %d\n"
	      "border (pt) (%d - %d) = %d x (%d - %d) = %d\n"
	      "printable pixels (px)   %d x %d\n"
	      "res (dpi)               %d x %d\n",
	      page_pt_width, page_pt_height,
	      image_px_width, image_px_height,
	      image_px_width * 72 / xdpi, image_px_height * 72 / ydpi,
	      out_pt_width, out_pt_height,
	      out_px_width, out_px_height,
	      out_pt_left, out_pt_top,
	      out_px_left, out_px_top,
	      page_pt_right, page_pt_left, page_pt_right - page_pt_left,
	      page_pt_bottom, page_pt_top, page_pt_bottom - page_pt_top,
	      print_px_width, print_px_height,
	      xdpi, ydpi
	      );	

  privdata.xdpi = xdpi;
  privdata.ydpi = ydpi;
  privdata.xsize = print_px_width;
  privdata.ysize = print_px_height;

  stp_set_string_parameter(v, "STPIOutputType", "CMY");
  
  if (caps->adj_cyan &&
        !stp_check_curve_parameter(v, "CyanCurve", STP_PARAMETER_ACTIVE))
    {
      adjustment = stp_curve_create_from_string(caps->adj_cyan);
      stp_set_curve_parameter(v, "CyanCurve", adjustment);
      stp_set_curve_parameter_active(v, "CyanCurve", STP_PARAMETER_ACTIVE);
      stp_curve_destroy(adjustment);
    }
  if (caps->adj_magenta &&
        !stp_check_curve_parameter(v, "MagentaCurve", STP_PARAMETER_ACTIVE))
    {
      adjustment = stp_curve_create_from_string(caps->adj_magenta);
      stp_set_curve_parameter(v, "MagentaCurve", adjustment);
      stp_set_curve_parameter_active(v, "MagentaCurve", STP_PARAMETER_ACTIVE);
      stp_curve_destroy(adjustment);
    }
  if (caps->adj_yellow &&
        !stp_check_curve_parameter(v, "YellowCurve", STP_PARAMETER_ACTIVE))
    {
      adjustment = stp_curve_create_from_string(caps->adj_yellow);
      stp_set_curve_parameter(v, "YellowCurve", adjustment);
      stp_set_curve_parameter_active(v, "YellowCurve", STP_PARAMETER_ACTIVE);
      stp_curve_destroy(adjustment);
    }

  if (ink_type)
    {
      for (i = 0; i < caps->inks->n_items; i++)
	if (strcmp(ink_type, caps->inks->item[i].name) == 0)
	  {
	    stp_set_string_parameter(v, "STPIOutputType",
				     caps->inks->item[i].output_type);
	    ink_channels = caps->inks->item[i].output_channels;
	    ink_order = caps->inks->item[i].channel_order;
	    break;
	  }
    }

  stp_channel_reset(v);
  for (i = 0; i < ink_channels; i++)
    stp_channel_add(v, i, 0, 1.0);

  out_channels = stp_color_init(v, image, 65536);

#if 0
  if (out_channels != ink_channels && out_channels != 1 && ink_channels != 1)
    {
      stp_eprintf(v, "Internal error!  Output channels or input channels must be 1\n");
      return 0;
    }
#endif

  rows = stp_zalloc(image_px_height * sizeof(unsigned short *));
  err_out = stp_malloc(print_px_width * ink_channels * 2);
  if (out_channels != ink_channels)
    final_out = stp_malloc(print_px_width * ink_channels * 2);

  stp_set_float_parameter(v, "Density", 1.0);

  if (ink_type && 
  	(strcmp(ink_type, "RGB") == 0 || strcmp(ink_type, "BGR") == 0))
    {
      zeros = stp_malloc(ink_channels * print_px_width + 1);
      (void) memset(zeros, '\xff', ink_channels * print_px_width + 1);
    }
  else
    zeros = stp_zalloc(ink_channels * print_px_width + 1);
  
  out_bytes = (caps->interlacing == OLYMPUS_INTERLACE_PLANE ? 1 : ink_channels);

  /* printer init */
  if (caps->printer_init_func)
    {
      stp_deprintf(STP_DBG_OLYMPUS, "olympus: caps->printer_init\n");
      (*(caps->printer_init_func))(v);
    }

  if (olympus_feature(caps, OLYMPUS_FEATURE_FULL_HEIGHT))
    {
      min_y = 0;
      max_y = print_px_height - 1;
    }
  else if (olympus_feature(caps, OLYMPUS_FEATURE_BLOCK_ALIGN))
    {
      min_y = out_px_top - (out_px_top % caps->block_size);
      				/* floor to multiple of block_size */
      max_y = (out_px_bottom - 1) + (caps->block_size - 1)
      		- ((out_px_bottom - 1) % caps->block_size);
				/* ceil to multiple of block_size */
    }
  else
    {
      min_y = out_px_top;
      max_y = out_px_bottom - 1;
    }
  
  if (olympus_feature(caps, OLYMPUS_FEATURE_FULL_WIDTH))
    {
      min_x = 0;
      max_x = print_px_width - 1;
    }
  else
    {
      min_x = out_px_left;
      max_x = out_px_right;
    }
      
  r_errdiv  = image_px_height / out_px_height;
  r_errmod  = image_px_height % out_px_height; 
  c_errdiv = image_px_width / out_px_width;
  c_errmod = image_px_width % out_px_width;

  for (pl = 0; pl < (caps->interlacing == OLYMPUS_INTERLACE_PLANE
			? ink_channels : 1); pl++)
    {
      r_errval  = 0;
      r_errlast = -1;
      r_errline = 0;

      privdata.plane = ink_order[pl];
      stp_deprintf(STP_DBG_OLYMPUS, "olympus: plane %d\n", privdata.plane);

      /* plane init */
      if (caps->plane_init_func)
        {
          stp_deprintf(STP_DBG_OLYMPUS, "olympus: caps->plane_init\n");
          (*(caps->plane_init_func))(v);
        }
  
      for (y = min_y; y <= max_y; y++)
        {
          unsigned short *out;
          int duplicate_line = 1;
/*          unsigned zero_mask; */
    
          if (((y - min_y) % caps->block_size) == 0)
	    {
              /* block init */
              privdata.block_min_y = y;
              privdata.block_min_x = min_x;
              privdata.block_max_y = MIN(y + caps->block_size - 1, max_y);
              privdata.block_max_x = max_x;
    
              if (caps->block_init_func)
	        {
                  stp_deprintf(STP_DBG_OLYMPUS,
			       "olympus: caps->block_init\n");
                  (*(caps->block_init_func))(v);
                }
            }
        
          if (y < out_px_top || y >= out_px_bottom)
  	    stp_zfwrite((char *) zeros, out_bytes, print_px_width, v);
          else
            {
              if (olympus_feature(caps, OLYMPUS_FEATURE_FULL_WIDTH)
	        && out_px_left > 0)
  	        {
                  stp_zfwrite((char *) zeros, out_bytes, out_px_left, v);
                  /* stp_erprintf("left %d ", out_px_left); */
  	        }
  
#if 0
              if (r_errline != r_errlast)
                {
  	          r_errlast = r_errline;
  	          duplicate_line = 0;

                  /* stp_erprintf("r_errline %d, ", r_errline); */
                  if (stp_color_get_row(v, image, r_errline, &zero_mask))
                    {
    	              status = 2;
                      break;
                    }
                }

              out = stp_channel_get_output(v);
#endif
              if (r_errline != r_errlast)
                {
  	          r_errlast = r_errline;
  	          duplicate_line = 0;
		}

	      out = olympus_get_cached_output(v, image, rows, r_errline,
	                                  image_px_width * ink_channels * 2);
	      if (out == NULL)
	        {
		  status = 2;
		  break;
		}

              c_errval  = 0;
              c_errlast = -1;
              c_errcol  = 0;
              for (i = 0; i < out_px_width; i++)
                {
                  if (c_errcol != c_errlast)
        	    c_errlast = c_errcol;
        	  for (j = 0; j < ink_channels; j++)
          	    err_out[i * ink_channels + j] =
      				out[c_errcol * ink_channels + ink_order[j]-1];

                  c_errval += c_errmod;
                  c_errcol += c_errdiv;
                  if (c_errval >= out_px_width)
                    {
                      c_errval -= out_px_width;
        	      c_errcol ++;
                    }
                }

              real_out = err_out;
              if (out_channels != ink_channels)
                {
                  real_out = final_out;
                  if (out_channels < ink_channels)
                    {
        	      for (i = 0; i < out_px_width; i++)
         		{
    	        	  for (j = 0; j < ink_channels; j++)
    		            final_out[i * ink_channels + j] = err_out[i];
    		        }
    	            }
          	  else
    	            {
    	              for (i = 0; i < out_px_width; i++)
    		        {
    		          int avg = 0;
    		          for (j = 0; j < out_channels; j++)
    		            avg += err_out[i * out_channels + j];
    		          final_out[i] = avg / out_channels;
    		        }
    	            }
    	        }
              char_out = (unsigned char *) real_out;
     	      char_out_width = (caps->interlacing == OLYMPUS_INTERLACE_PLANE ?
  				out_px_width : out_px_width * out_channels);
    	      for (i = 0; i < char_out_width; i++)
	        {
                  if (caps->interlacing == OLYMPUS_INTERLACE_PLANE)
  	            j = i * ink_channels + pl;
  	          else if (caps->interlacing == OLYMPUS_INTERLACE_LINE)
  	            j = (i % out_px_width) + (i / out_px_width);
  	          else  /* OLYMPUS_INTERLACE_NONE */
  	            j = i;
    	  
  	          char_out[i] = real_out[j] / 257;
                }
	      
  	      stp_zfwrite((char *) real_out, 1, char_out_width, v);
              /* stp_erprintf("data %d ", out_px_width); */
              if (olympus_feature(caps, OLYMPUS_FEATURE_FULL_WIDTH)
	        && out_px_right < print_px_width)
  	        {
                  stp_zfwrite((char *) zeros, out_bytes,
				  print_px_width - out_px_right, v);
                  /* stp_erprintf("right %d ", print_px_width-out_px_right); */
  	        }
              /* stp_erprintf("\n"); */
  
  	      r_errval += r_errmod;
  	      r_errline += r_errdiv;
  	      if (r_errval >= out_px_height)
  	        {
  	          r_errval -= out_px_height;
  	          r_errline ++;
  	        }
  	    }
        
          if (y == privdata.block_max_y)
	    {
              /* block end */
              if (caps->block_end_func)
	        {
                  stp_deprintf(STP_DBG_OLYMPUS, "olympus: caps->block_end\n");
                  (*(caps->block_end_func))(v);
  	        }
            }
        }

      /* plane end */
      if (caps->plane_end_func) {
        stp_deprintf(STP_DBG_OLYMPUS, "olympus: caps->plane_end\n");
        (*(caps->plane_end_func))(v);
      }
    }

  /* printer end */
  if (caps->printer_end_func)
    {
      stp_deprintf(STP_DBG_OLYMPUS, "olympus: caps->printer_end\n");
      (*(caps->printer_end_func))(v);
    }
  stp_image_conclude(image);
  if (final_out)
    stp_free(final_out);
  if (err_out)
    stp_free(err_out);
  if (zeros)
    stp_free(zeros);
  if (rows)
    {
      for (i = 0; i <image_px_height; i++)
        stp_free(rows[i]);
      stp_free(rows);
    }
  return status;
}

static int
olympus_print(const stp_vars_t *v, stp_image_t *image)
{
  int status;
  stp_vars_t *nv = stp_vars_create_copy(v);
  stp_prune_inactive_options(nv);
  status = olympus_do_print(nv, image);
  stp_vars_destroy(nv);
  return status;
}

static const stp_printfuncs_t print_olympus_printfuncs =
{
  olympus_list_parameters,
  olympus_parameters,
  stp_default_media_size,
  olympus_imageable_area,
  olympus_limit,
  olympus_print,
  olympus_describe_resolution,
  olympus_describe_output,
  stp_verify_printer_params,
  NULL,
  NULL
};




static stp_family_t print_olympus_module_data =
  {
    &print_olympus_printfuncs,
    NULL
  };


static int
print_olympus_module_init(void)
{
  return stp_family_register(print_olympus_module_data.printer_list);
}


static int
print_olympus_module_exit(void)
{
  return stp_family_unregister(print_olympus_module_data.printer_list);
}


/* Module header */
#define stp_module_version print_olympus_LTX_stp_module_version
#define stp_module_data print_olympus_LTX_stp_module_data

stp_module_version_t stp_module_version = {0, 0};

stp_module_t stp_module_data =
  {
    "olympus",
    VERSION,
    "Olympus family driver",
    STP_MODULE_CLASS_FAMILY,
    NULL,
    print_olympus_module_init,
    print_olympus_module_exit,
    (void *) &print_olympus_module_data
  };


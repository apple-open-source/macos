/*
 * "$Id: escp2-resolutions.c,v 1.42 2007/05/27 17:59:50 rlk Exp $"
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

static const res_t r_360x90dpi =
{ "360x90dpi",     N_("360 x 90 DPI Fast Economy"),
  360,  90,  360,  90,  360,  90,   0, 0, 1 };
static const res_t r_360x90sw =
{ "360x90sw",      N_("360 x 90 DPI Fast Economy"),
  360,  90,  360,  90,  360,  90,   1, 0, 1 };

static const res_t r_360x120dpi =
{ "360x120dpi",    N_("360 x 120 DPI Economy"),
  360,  120,  360,  120,  360,  120,  0, 0, 1 };
static const res_t r_360x120sw =
{ "360x120sw",     N_("360 x 120 DPI Economy"),
  360,  120,  360,  120,  360,  120,  1, 0, 1 };

static const res_t r_180dpi =
{ "180dpi",        N_("180 DPI Economy"),
  180,  180,  180,  180,  180,  180,  0, 0, 1 };
static const res_t r_180sw =
{ "180sw",         N_("180 DPI Economy"),
  180,  180,  180,  180,  180,  180,  1, 0, 1 };

static const res_t r_360x180dpi =
{ "360x180dpi",    N_("360 x 180 DPI Draft"),
  360,  180,  360,  180,  360,  180,  0, 0, 1 };
static const res_t r_360x180sw =
{ "360x180sw",     N_("360 x 180 DPI Draft"),
  360,  180,  360,  180,  360,  180,  1, 0, 1 };

static const res_t r_360x240dpi =
{ "360x240dpi",    N_("360 x 240 DPI Draft"),
  360,  240,  360,  240,  360,  240,  0, 0, 1 };
static const res_t r_360x240sw =
{ "360x240sw",     N_("360 x 240 DPI Draft"),
  360,  240,  360,  240,  360,  240,  1, 0, 1 };

static const res_t r_360mw =
{ "360mw",         N_("360 DPI High Quality"),
  360,  360,  360,  360,  360,  360,  0, 1, 1 };
static const res_t r_360pro =
{ "360pro",        N_("360 DPI"),
  360,  360,  360,  360,  360,  360,  0, 1, 1 };
static const res_t r_360 =
{ "360",           N_("360 DPI"),
  360,  360,  360,  360,  360,  360,  0, 0, 1 };
static const res_t r_360sw =
{ "360sw",         N_("360 DPI"),
  360,  360,  360,  360,  360,  360,  1, 0, 1 };

static const res_t r_720x360mw =
{ "720x360mw",     N_("720 x 360 DPI"),
  720,  360,  720,  360,  720,  360,  0, 1, 1 };
static const res_t r_720x360sw =
{ "720x360sw",     N_("720 x 360 DPI"),
  720,  360,  720,  360,  720,  360,  1, 0, 1 };
static const res_t r_720x360un =
{ "720x360un",     N_("720 x 360 DPI Enhanced"),
  720,  360, 1440,  720,  720,  360,  1, 0, 1 };

static const res_t r_720mw =
{ "720mw",         N_("720 DPI"),
  720,  720,  720,  720,  720,  720,  0, 1, 1 };
static const res_t r_720sw =
{ "720sw",         N_("720 DPI"),
  720,  720,  720,  720,  720,  720,  1, 0, 1 };
static const res_t r_720un =
{ "720un",         N_("720 DPI High Quality"),
  720,  720, 1440,  720,  720,  720,  1, 0, 1 };
static const res_t r_720hq =
{ "720hq",         N_("720 DPI High Quality"),
  720,  720,  720,  720,  720,  720,  1, 0, 2 };
static const res_t r_720hq2 =
{ "720hq2",        N_("720 DPI Highest Quality"),
  720,  720,  720,  720,  720,  720,  1, 0, 4 };
static const res_t r_720x720oov =
{ "720x720oov",    N_("720 x 720 DPI"),
  2880, 720,  2880, 720,  720, 720,  1, 0, 1 };

static const res_t r_1440x720mw =
{ "1440x720mw",     N_("1440 x 720 DPI"),
  1440, 720,  1440, 720,  1440, 720,  0, 1, 1 };
static const res_t r_1440x720sw =
{ "1440x720sw",    N_("1440 x 720 DPI"),
  1440, 720,  1440, 720,  1440, 720,  1, 0, 1 };
static const res_t r_1440x720hq2 =
{ "1440x720hq2",   N_("1440 x 720 DPI Highest Quality"),
  1440, 720,  1440, 720,  1440, 720,  1, 0, 2 };
static const res_t r_720x1440sw =
{ "720x1440sw",    N_("1440 x 720 DPI Transposed"),
  720, 1440,  720, 1440,  720, 1440,  1, 0, 1 };
static const res_t r_720x1440ov =
{ "720x1440ov",   N_("1440 x 720 DPI Transposed"),
  1440, 1440, 1440, 1440, 720, 1440,  1, 0, 1};
static const res_t r_1440x720ov =
{ "1440x720ov",    N_("1440 x 720 DPI"),
  2880, 720,  2880, 720,  1440, 720,  1, 0, 1 };

static const res_t r_2880x720mw =
{ "2880x720mw",    N_("2880 x 720 DPI"),
  2880, 720,  2880, 720,  2880, 720,  0, 1, 1};
static const res_t r_2880x720sw =
{ "2880x720sw",    N_("2880 x 720 DPI"),
  2880, 720,  2880, 720,  2880, 720,  1, 0, 1};
static const res_t r_2880x720hq2 =
{ "2880x720hq2",   N_("2880 x 720 DPI Highest Quality"),
  2880, 720,  2880, 720,  2880, 720,  1, 0, 2 };

static const res_t r_1440x1440mw =
{ "1440x1440mw",   N_("1440 x 1440 DPI"),
  1440, 1440,  1440, 1440,  1440, 1440, 0, 1, 1};
static const res_t r_1440x1440sw =
{ "1440x1440sw",   N_("1440 x 1440 DPI"),
  1440, 1440,  1440, 1440,  1440, 1440, 1, 0, 1};
static const res_t r_1440x1440ov =
{ "1440x1440ov",   N_("1440 x 1440 DPI"),
  2880, 1440,  2880, 1440,  1440, 1440, 1, 0, 1};

static const res_t r_2880x1440mw =
{ "2880x1440mw",   N_("2880 x 1440 DPI"),
  2880, 1440,  2880, 1440,  2880, 1440, 0, 1, 1};
static const res_t r_2880x1440sw =
{ "2880x1440sw",   N_("2880 x 1440 DPI"),
  2880, 1440,  2880, 1440,  2880, 1440, 1, 0, 1};
static const res_t r_1440x2880sw =
{ "1440x2880sw",   N_("2880 x 1440 DPI Transposed"),
  1440, 2880,  1440, 2880,  1440, 2880, 1, 0, 1};
static const res_t r_2880x1440sw2400 =
{ "2880x1440sw",   N_("2880 x 1440 DPI"),
  1440, 2880,  1440, 2880,  1440, 2880, 1, 0, 1};

static const res_t r_5760x1440sw =
{ "5760x1440sw",   N_("5760 x 1440 DPI"),
  5760, 1440,  5760, 1440,  5760, 1440, 1, 0, 1};

static const res_t r_2880x2880mw =
{ "2880x2880mw",   N_("2880 x 2880 DPI"),
  2880, 2880,  2880, 2880,  2880, 2880, 0, 1, 1};
static const res_t r_2880x2880sw =
{ "2880x2880sw",   N_("2880 x 2880 DPI"),
  2880, 2880,  2880, 2880,  2880, 2880, 1, 0, 1};

static const res_t r_5760x2880mw =
{ "5760x2880mw",   N_("5760 x 2880 DPI"),
  5760, 2880,  5760, 2880,  5760, 2880, 0, 1, 1};
static const res_t r_5760x2880sw =
{ "5760x2880sw",   N_("5760 x 2880 DPI"),
  5760, 2880,  5760, 2880,  5760, 2880, 1, 0, 1};



static const res_t *const stpi_escp2_720dpi_reslist[] =
{
  &r_360x90dpi,

  &r_360x120dpi,

  &r_180dpi,

  &r_360x240dpi,

  &r_360x180dpi,

  &r_360mw,
  &r_360,

  &r_720x360sw,

  &r_720mw,

  NULL
};

static const res_t *const stpi_escp2_1440dpi_reslist[] =
{
  &r_360x90sw,

  &r_360x120sw,

  &r_180sw,

  &r_360x240sw,

  &r_360x180sw,

  &r_360sw,

  &r_720x360sw,

  &r_720sw,
  &r_720hq,

  &r_1440x720sw,
  &r_1440x720hq2,

  NULL
};

static const res_t *const stpi_escp2_2880dpi_reslist[] =
{
  &r_360x90sw,

  &r_360x120sw,

  &r_180sw,

  &r_360x240sw,

  &r_360x180sw,

  &r_360sw,

  &r_720x360sw,

  &r_720sw,

  &r_1440x720sw,
  &r_1440x720hq2,

  &r_2880x720sw,
  &r_2880x720hq2,

  NULL
};

static const res_t *const stpi_escp2_2880_1440dpi_reslist[] =
{
  &r_360x90sw,

  &r_360x120sw,

  &r_180sw,

  &r_360x240sw,

  &r_360x180sw,

  &r_360sw,

  &r_720x360sw,

  &r_720sw,

  &r_1440x720sw,

  &r_2880x720sw,

  &r_1440x1440sw,

  &r_2880x1440sw,

  &r_2880x2880sw,

  NULL
};

static const res_t *const stpi_escp2_g3_reslist[] =
{
  &r_360x90dpi,

  &r_360x120dpi,

  &r_180dpi,

  &r_360x240dpi,

  &r_360x180dpi,

  &r_360mw,
  &r_360,

  &r_720x360sw,

  &r_720sw,
  &r_720hq,

  &r_1440x720sw,
  &r_1440x720hq2,

  NULL
};

static const res_t *const stpi_escp2_superfine_reslist[] =
{
  &r_360x90sw,

  &r_360x120sw,

  &r_360x180sw,

  &r_360x240sw,

  &r_360sw,

  &r_720x360sw,

  &r_720sw,

  &r_1440x720sw,

  &r_1440x1440ov,

  &r_2880x1440sw,

  &r_5760x1440sw,

  &r_2880x2880sw,

  &r_5760x2880sw,

  NULL
};

static const res_t *const stpi_escp2_claria_1400_reslist[] =
{
  &r_360x90sw,

  &r_360x180sw,

  &r_360x240sw,

  &r_360sw,

  &r_720x360sw,

  &r_720x360un,

  &r_720sw,

  &r_720un,

  &r_1440x720sw,

  &r_1440x1440ov,

  &r_2880x1440sw,

  &r_5760x1440sw,

  &r_2880x2880sw,

  &r_5760x2880sw,

  NULL
};

static const res_t *const stpi_escp2_picturemate_reslist[] =
{

  &r_720x720oov,

  &r_1440x720ov,

  &r_720x1440ov,

  &r_1440x1440sw,

  &r_2880x1440sw,

  &r_5760x1440sw,

  NULL
};

static const res_t *const stpi_escp2_sc500_reslist[] =
{
  &r_360x90dpi,

  &r_360x120dpi,

  &r_180dpi,

  &r_360x240dpi,

  &r_360x180dpi,

  &r_360mw,
  &r_360,

  &r_720x360mw,

  &r_720mw,

  NULL
};

static const res_t *const stpi_escp2_g3_720dpi_reslist[] =
{
  &r_360x90dpi,

  &r_360x120sw,

  &r_180dpi,

  &r_360x240sw,

  &r_360x180dpi,

  &r_360mw,
  &r_360,

  &r_720x360sw,

  &r_720sw,
  &r_720hq,
  &r_720hq2,

  NULL
};

static const res_t *const stpi_escp2_720dpi_soft_reslist[] =
{
  &r_360x90dpi,

  &r_360x120sw,

  &r_180dpi,

  &r_360x240sw,

  &r_360x180dpi,

  &r_360sw,

  &r_720x360sw,

  &r_720sw,
  &r_720hq,
  &r_720hq2,

  NULL
};

static const res_t *const stpi_escp2_sc640_reslist[] =
{
  &r_360x90dpi,

  &r_180dpi,

  &r_360x180dpi,

  &r_360mw,
  &r_360,

  &r_720x360sw,

  &r_720mw,

  &r_1440x720sw,
  &r_1440x720hq2,

  NULL
};

static const res_t *const stpi_escp2_pro_reslist[] =
{
  &r_360x90dpi,

  &r_180dpi,

  &r_360x180dpi,

  &r_360pro,

  &r_720x360mw,

  &r_720mw,

  &r_1440x720mw,

  &r_2880x720mw,

  &r_1440x1440mw,

  &r_2880x1440mw,

  &r_2880x2880mw,

  NULL
};

typedef struct
{
  const char *name;
  const res_t *const *const res_list;
} resolution_t;

static const resolution_t the_resolutions[] =
{
  { "superfine", stpi_escp2_superfine_reslist },
  { "claria_1400", stpi_escp2_claria_1400_reslist },
  { "pro", stpi_escp2_pro_reslist },
  { "720dpi", stpi_escp2_720dpi_reslist },
  { "720dpi_soft", stpi_escp2_720dpi_soft_reslist },
  { "g3_720dpi", stpi_escp2_g3_720dpi_reslist },
  { "1440dpi", stpi_escp2_1440dpi_reslist },
  { "2880dpi", stpi_escp2_2880dpi_reslist },
  { "2880_1440dpi", stpi_escp2_2880_1440dpi_reslist },
  { "g3", stpi_escp2_g3_reslist },
  { "sc500", stpi_escp2_sc500_reslist },
  { "sc640", stpi_escp2_sc640_reslist },
  { "picturemate", stpi_escp2_picturemate_reslist },
};

const res_t *const *
stpi_escp2_get_reslist_named(const char *n)
{
  int i;
  if (n)
    for (i = 0; i < sizeof(the_resolutions) / sizeof(resolution_t); i++)
      {
	if (strcmp(n, the_resolutions[i].name) == 0)
	  return the_resolutions[i].res_list;
      }
  return NULL;
}

#define DECLARE_PRINTER_WEAVES(name)				\
static const printer_weave_list_t name##_printer_weave_list =	\
{								\
  #name,							\
  sizeof(name##_printer_weaves) / sizeof(printer_weave_t),	\
  name##_printer_weaves						\
}

static const printer_weave_t standard_printer_weaves[] =
{
  { "Off", N_("Off"), 0 },
  { "On", N_("On"), 1 }
};

DECLARE_PRINTER_WEAVES(standard);

static const printer_weave_t pro7000_printer_weaves[] =
{
  { "Off", N_("Off"), 0 },
  { "On", N_("Interleave"), 1 },
  { "FOL", N_("Full Overlap"), 2 },
  { "FourPass", N_("Four Pass"), 3 },
  { "FOL2", N_("Full Overlap 2"), 4 },
};

DECLARE_PRINTER_WEAVES(pro7000);

static const printer_weave_t pro7500_printer_weaves[] =
{
  { "Off", N_("Off"), 0 },
  { "On", N_("Interleave"), 1 },
  { "FOL", N_("Full Overlap"), 2 },
  { "FourPass", N_("Four Pass"), 3 },
  { "FOL2", N_("Full Overlap 2"), 4 },
  { "MW2", N_("Interleave 2"), 5 },
};

DECLARE_PRINTER_WEAVES(pro7500);

static const printer_weave_t pro7600_printer_weaves[] =
{
  { "Off", N_("Off"), 0 },
  { "On", N_("Interleave"), 1 },
  { "FOL", N_("Full Overlap"), 2 },
  { "FourPass", N_("Four Pass"), 3 },
  { "FOL2", N_("Full Overlap 2"), 4 },
  { "MW2", N_("Interleave 2"), 5 },
  { "EightPass", N_("Eight Pass"), 6 },
};

DECLARE_PRINTER_WEAVES(pro7600);

typedef struct
{
  const char *name;
  const printer_weave_list_t *weave_list;
} weave_t;

static const weave_t the_weaves[] =
{
  { "standard", &standard_printer_weave_list },
  { "pro7000", &pro7000_printer_weave_list },
  { "pro7500", &pro7500_printer_weave_list },
  { "pro7600", &pro7600_printer_weave_list },
};

const printer_weave_list_t *
stpi_escp2_get_printer_weaves_named(const char *n)
{
  int i;
  if (n)
    for (i = 0; i < sizeof(the_weaves) / sizeof(weave_t); i++)
      {
	if (strcmp(n, the_weaves[i].name) == 0)
	  return the_weaves[i].weave_list;
      }
  return NULL;
}


#define DECLARE_QUALITY_LIST(name)			\
static const quality_list_t name##_quality_list =	\
{							\
  #name,						\
  name##_qualities,					\
  sizeof(name##_qualities) / sizeof(const quality_t),	\
}

static const quality_t standard_qualities[] =
{
  { "FastEconomy", N_("Fast Economy"), 180, 90, 360, 120, 360, 90 },
  { "Economy",     N_("Economy"),      360, 180, 360, 240, 360, 180 },
  { "Draft",       N_("Draft"),        360, 360, 360, 360, 360, 360 },
  { "Standard",    N_("Standard"),     0, 0, 0, 0, 720, 360 },
  { "High",        N_("High"),         0, 0, 0, 0, 720, 720 },
  { "Photo",       N_("Photo"),        1440, 720, 2880, 720, 1440, 720 },
  { "HighPhoto",   N_("Super Photo"),  1440, 1440, 2880, 1440, 1440, 1440 },
  { "UltraPhoto",  N_("Ultra Photo"),  2880, 2880, 2880, 2880, 2880, 2880 },
  { "Best",        N_("Best"),         720, 360, 0, 0, -1, -1 },
};

DECLARE_QUALITY_LIST(standard);

static const quality_t p1_5_qualities[] =
{
  { "FastEconomy", N_("Fast Economy"), 180, 90, 360, 120, 360, 90 },
  { "Economy",     N_("Economy"),      360, 180, 360, 240, 360, 180 },
  { "Draft",       N_("Draft"),        360, 360, 360, 360, 360, 360 },
  { "Standard",    N_("Standard"),     0, 0, 0, 0, 720, 360 },
  { "High",        N_("High"),         0, 0, 0, 0, 720, 720 },
  { "Photo",       N_("Photo"),        1440, 720, 1440, 720, 1440, 720 },
  { "HighPhoto",   N_("Super Photo"),  1440, 1440, 2880, 1440, 1440, 1440 },
  { "UltraPhoto",  N_("Ultra Photo"),  2880, 2880, 2880, 2880, 2880, 2880 },
  { "Best",        N_("Best"),         720, 360, 0, 0, -1, -1 },
};

DECLARE_QUALITY_LIST(p1_5);

static const quality_t picturemate_qualities[] =
{
  { "Draft",       N_("Draft"),        1440,  720, 1440,  720, 1440,  720 },
  { "Standard",    N_("Standard"),     1440, 1440, 1440, 1440, 1440, 1440 },
  { "Photo",       N_("Photo"),        1440, 1440, 1440, 1440, 1440, 1440 },
  { "High",        N_("High"),         2880, 1440, 2880, 1440, 2880, 1440 },
  { "HighPhoto",   N_("Super Photo"),  2880, 1440, 2880, 1440, 2880, 1440 },
  { "UltraPhoto",  N_("Ultra Photo"),  5760, 1440, 5760, 1440, 5760, 1440 },
  { "Best",        N_("Best"),         5760, 1440, 5760, 1440, 5760, 1440 },
};

DECLARE_QUALITY_LIST(picturemate);

typedef struct
{
  const char *name;
  const quality_list_t *quality_list;
} qual_t;

static const qual_t the_qualities[] =
{
  { "standard", &standard_quality_list },
  { "p1_5", &p1_5_quality_list },
  { "picturemate", &picturemate_quality_list },
};

const quality_list_t *
stpi_escp2_get_quality_list_named(const char *n)
{
  int i;
  if (n)
    for (i = 0; i < sizeof(the_qualities) / sizeof(qual_t); i++)
      {
	if (strcmp(n, the_qualities[i].name) == 0)
	  return the_qualities[i].quality_list;
      }
  return NULL;
}

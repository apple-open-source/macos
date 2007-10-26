/*
 * "$Id: escp2-papers.c,v 1.105 2007/05/13 16:49:40 rlk Exp $"
 *
 *   Print plug-in EPSON ESC/P2 driver for the GIMP.
 *
 *   Copyright 1997-2000 Michael Sweet (mike@easysw.com) and
 *	Robert Krawitz (rlk@alum.mit.edu)
 *
 *   This program is free software; you can redistribute it and/or modify it
 *   under the terms of the GNU eral Public License as published by the Free
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

static const char standard_sat_adj[] =
"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
"<gutenprint>\n"
"<curve wrap=\"wrap\" type=\"linear\" gamma=\"0\">\n"
"<sequence count=\"48\" lower-bound=\"0\" upper-bound=\"4\">\n"
/* C */  "1.20 1.20 1.30 1.50 1.70 1.90 2.00 2.00 "  /* B */
/* B */  "2.00 2.00 2.00 2.00 2.00 2.00 2.00 2.00 "  /* M */
/* M */  "2.00 1.80 1.60 1.40 1.20 1.00 1.00 1.00 "  /* R */
/* R */  "1.00 1.00 1.00 1.00 1.00 1.00 1.00 1.00 "  /* Y */
/* Y */  "1.00 1.15 1.40 1.70 2.00 2.30 2.40 2.40 "  /* G */
/* G */  "2.40 2.40 2.40 2.30 2.00 1.70 1.40 1.15 "  /* C */
"</sequence>\n"
"</curve>\n"
"</gutenprint>\n";

static const char standard_lum_adj[] =
"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
"<gutenprint>\n"
"<curve wrap=\"wrap\" type=\"linear\" gamma=\"0\">\n"
"<sequence count=\"48\" lower-bound=\"0\" upper-bound=\"4\">\n"
/* C */  "0.56 0.58 0.62 0.68 0.73 0.78 0.82 0.85 "  /* B */
/* B */  "0.85 0.82 0.78 0.78 0.79 0.80 0.82 0.85 "  /* M */
/* M */  "0.87 0.90 0.94 0.97 1.00 1.00 1.00 1.00 "  /* R */
/* R */  "1.00 1.00 1.00 1.00 1.00 1.00 1.00 1.00 "  /* Y */
/* Y */  "1.00 1.00 1.00 0.99 0.98 0.97 0.95 0.93 "  /* G */
/* G */  "0.90 0.76 0.65 0.58 0.58 0.57 0.56 0.56 "  /* C */
"</sequence>\n"
"</curve>\n"
"</gutenprint>\n";

static const char standard_hue_adj[] =
"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
"<gutenprint>\n"
"<curve wrap=\"wrap\" type=\"linear\" gamma=\"0\">\n"
"<sequence count=\"48\" lower-bound=\"-6\" upper-bound=\"6\">\n"
/* C */  "0.00 0.00 0.00 -.02 -.04 -.08 -.12 -.16 "  /* B */
/* B */  "-.20 -.24 -.28 -.32 -.32 -.32 -.32 -.32 "  /* M */
/* M */  "-.36 -.40 -.44 -.48 -.50 -.45 -.40 -.30 "  /* R */
/* R */  "-.12 -.07 -.04 -.02 0.00 0.00 0.00 0.00 "  /* Y */
/* Y */  "0.00 -.00 -.03 -.06 -.09 -.13 -.17 -.21 "  /* G */
/* G */  "-.25 -.22 -.19 -.16 -.13 -.10 -.07 -.03 "  /* C */
"</sequence>\n"
"</curve>\n"
"</gutenprint>\n";


static const char photo2_sat_adj[] =
"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
"<gutenprint>\n"
"<curve wrap=\"wrap\" type=\"linear\" gamma=\"0\">\n"
"<sequence count=\"48\" lower-bound=\"0\" upper-bound=\"4\">\n"
/* C */  "1.20 1.20 1.30 1.50 1.70 1.90 2.00 2.00 "  /* B */
/* B */  "2.00 2.00 2.00 2.00 2.00 2.00 2.00 2.00 "  /* M */
/* M */  "2.00 1.80 1.60 1.40 1.20 1.00 1.00 1.00 "  /* R */
/* R */  "1.00 1.00 1.00 1.00 1.00 1.00 1.00 1.00 "  /* Y */
/* Y */  "1.00 1.15 1.40 1.70 2.00 2.30 2.40 2.40 "  /* G */
/* G */  "2.40 2.40 2.40 2.30 2.00 1.70 1.40 1.15 "  /* C */
"</sequence>\n"
"</curve>\n"
"</gutenprint>\n";

static const char photo2_lum_adj[] =
"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
"<gutenprint>\n"
"<curve wrap=\"wrap\" type=\"linear\" gamma=\"0\">\n"
"<sequence count=\"48\" lower-bound=\"0\" upper-bound=\"4\">\n"
/* C */  "0.56 0.58 0.62 0.68 0.73 0.78 0.82 0.85 "  /* B */
/* B */  "0.85 0.82 0.78 0.78 0.79 0.80 0.82 0.85 "  /* M */
/* M */  "0.87 0.90 0.94 0.97 1.00 1.00 1.00 1.00 "  /* R */
/* R */  "1.00 1.00 1.00 1.00 1.00 1.00 1.00 1.00 "  /* Y */
/* Y */  "1.00 1.00 1.00 0.99 0.98 0.97 0.95 0.93 "  /* G */
/* G */  "0.90 0.76 0.65 0.58 0.58 0.57 0.56 0.56 "  /* C */
"</sequence>\n"
"</curve>\n"
"</gutenprint>\n";

static const char photo2_hue_adj[] =
"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
"<gutenprint>\n"
"<curve wrap=\"wrap\" type=\"linear\" gamma=\"0\">\n"
"<sequence count=\"48\" lower-bound=\"-6\" upper-bound=\"6\">\n"
/* C */  "0.00 0.00 0.00 -.02 -.04 -.08 -.12 -.16 "  /* B */
/* B */  "-.20 -.24 -.28 -.32 -.32 -.32 -.32 -.32 "  /* M */
/* M */  "-.36 -.40 -.44 -.48 -.50 -.45 -.40 -.30 "  /* R */
/* R */  "-.12 -.07 -.04 -.02 0.00 0.00 0.00 0.00 "  /* Y */
/* Y */  "0.00 -.00 -.03 -.06 -.09 -.13 -.17 -.21 "  /* G */
/* G */  "-.25 -.22 -.19 -.16 -.13 -.10 -.07 -.03 "  /* C */
"</sequence>\n"
"</curve>\n"
"</gutenprint>\n";


static const char photo3_sat_adj[] =
"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
"<gutenprint>\n"
"<curve wrap=\"wrap\" type=\"linear\" gamma=\"0\">\n"
"<sequence count=\"48\" lower-bound=\"0\" upper-bound=\"4\">\n"
/* C */  "1.00 1.05 1.15 1.25 1.35 1.45 1.50 1.50 "  /* B */
/* B */  "1.50 1.50 1.50 1.50 1.50 1.50 1.50 1.50 "  /* M */
/* M */  "1.50 1.40 1.30 1.20 1.10 1.00 1.00 1.00 "  /* R */
/* R */  "1.00 1.00 1.00 1.00 1.00 1.00 1.00 1.00 "  /* Y */
/* Y */  "1.00 1.10 1.30 1.55 1.80 1.95 2.00 2.00 "  /* G */
/* G */  "2.00 2.00 2.00 1.95 1.80 1.55 1.30 1.10 "  /* C */
"</sequence>\n"
"</curve>\n"
"</gutenprint>\n";

static const char photo3_lum_adj[] =
"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
"<gutenprint>\n"
"<curve wrap=\"wrap\" type=\"linear\" gamma=\"0\">\n"
"<sequence count=\"48\" lower-bound=\"0\" upper-bound=\"4\">\n"
/* C */  "0.66 0.67 0.69 0.73 0.77 0.83 0.87 0.89 "  /* B */
/* B */  "0.91 0.88 0.82 0.78 0.78 0.80 0.82 0.85 "  /* M */
/* M */  "0.87 0.90 0.94 0.97 1.00 1.00 1.00 1.00 "  /* R */
/* R */  "1.00 1.00 1.00 1.00 1.00 1.00 1.00 1.00 "  /* Y */
/* Y */  "1.00 1.00 0.99 0.98 0.96 0.94 0.92 0.88 "  /* G */
/* G */  "0.84 0.72 0.69 0.67 0.66 0.66 0.66 0.66 "  /* C */
"</sequence>\n"
"</curve>\n"
"</gutenprint>\n";

static const char photo3_hue_adj[] =
"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
"<gutenprint>\n"
"<curve wrap=\"wrap\" type=\"linear\" gamma=\"0\">\n"
"<sequence count=\"48\" lower-bound=\"-6\" upper-bound=\"6\">\n"
/* C */  "0.00 -.01 -.03 -.06 -.10 -.15 -.20 -.25 "  /* B */
/* B */  "-.28 -.30 -.34 -.35 -.35 -.34 -.33 -.33 "  /* M */
/* M */  "-.36 -.40 -.44 -.48 -.50 -.45 -.40 -.30 "  /* R */
/* R */  "-.12 -.07 -.04 -.02 0.00 0.00 0.00 0.00 "  /* Y */
/* Y */  "0.00 -.00 -.00 -.00 -.02 -.04 -.08 -.13 "  /* G */
/* G */  "-.18 -.18 -.19 -.16 -.13 -.10 -.07 -.03 "  /* C */
"</sequence>\n"
"</curve>\n"
"</gutenprint>\n";



static const char claria_sat_adj[] =
"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
"<gutenprint>\n"
"<curve wrap=\"wrap\" type=\"linear\" gamma=\"0\">\n"
"<sequence count=\"48\" lower-bound=\"0\" upper-bound=\"4\">\n"
/* C */  "1.00 1.05 1.15 1.25 1.35 1.45 1.50 1.50 "  /* B */
/* B */  "1.50 1.50 1.50 1.50 1.50 1.50 1.50 1.50 "  /* M */
/* M */  "1.50 1.40 1.30 1.20 1.10 1.00 1.00 1.00 "  /* R */
/* R */  "1.00 1.00 1.00 1.00 1.00 1.00 1.00 1.00 "  /* Y */
/* Y */  "1.00 1.10 1.30 1.55 1.80 1.95 2.00 2.00 "  /* G */
/* G */  "2.00 2.00 2.00 1.95 1.80 1.55 1.30 1.10 "  /* C */
"</sequence>\n"
"</curve>\n"
"</gutenprint>\n";

static const char claria_lum_adj[] =
"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
"<gutenprint>\n"
"<curve wrap=\"wrap\" type=\"linear\" gamma=\"0\">\n"
"<sequence count=\"48\" lower-bound=\"0\" upper-bound=\"4\">\n"
/* C */  "0.66 0.67 0.69 0.73 0.77 0.83 0.87 0.89 "  /* B */
/* B */  "0.91 0.88 0.84 0.78 0.78 0.80 0.82 0.85 "  /* M */
/* M */  "0.87 0.90 0.94 0.97 1.00 1.00 1.00 1.00 "  /* R */
/* R */  "1.00 1.00 1.00 1.00 1.00 1.00 1.00 1.00 "  /* Y */
/* Y */  "1.00 1.00 0.99 0.98 0.96 0.94 0.92 0.88 "  /* G */
/* G */  "0.84 0.72 0.69 0.67 0.66 0.66 0.66 0.66 "  /* C */
"</sequence>\n"
"</curve>\n"
"</gutenprint>\n";

static const char claria_hue_adj[] =
"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
"<gutenprint>\n"
"<curve wrap=\"wrap\" type=\"linear\" gamma=\"0\">\n"
"<sequence count=\"48\" lower-bound=\"-6\" upper-bound=\"6\">\n"
/* C */  "0.00 -.01 -.03 -.06 -.10 -.15 -.20 -.25 "  /* B */
/* B */  "-.30 -.35 -.38 -.40 -.42 -.46 -.49 -.52 "  /* M */
/* M */  "-.55 -.57 -.57 -.55 -.52 -.48 -.40 -.30 "  /* R */
/* R */  "-.12 -.07 -.04 -.02 0.00 0.00 0.00 0.00 "  /* Y */
/* Y */  "0.00 -.00 -.00 -.00 -.02 -.04 -.08 -.13 "  /* G */
/* G */  "-.18 -.18 -.19 -.16 -.13 -.10 -.07 -.03 "  /* C */
"</sequence>\n"
"</curve>\n"
"</gutenprint>\n";


static const char sp960_sat_adj[] =
"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
"<gutenprint>\n"
"<curve wrap=\"wrap\" type=\"linear\" gamma=\"0\">\n"
"<sequence count=\"48\" lower-bound=\"0\" upper-bound=\"4\">\n"
/* C */  "1.00 1.05 1.15 1.25 1.35 1.45 1.50 1.50 "  /* B */
/* B */  "1.50 1.50 1.50 1.50 1.50 1.50 1.50 1.50 "  /* M */
/* M */  "1.50 1.40 1.30 1.20 1.10 1.00 1.00 1.00 "  /* R */
/* R */  "1.00 1.00 1.00 1.00 1.00 1.00 1.00 1.00 "  /* Y */
/* Y */  "1.00 1.10 1.30 1.55 1.80 1.95 2.00 2.00 "  /* G */
/* G */  "2.00 2.00 2.00 1.95 1.80 1.55 1.30 1.10 "  /* C */
"</sequence>\n"
"</curve>\n"
"</gutenprint>\n";

static const char sp960_lum_adj[] =
"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
"<gutenprint>\n"
"<curve wrap=\"wrap\" type=\"linear\" gamma=\"0\">\n"
"<sequence count=\"48\" lower-bound=\"0\" upper-bound=\"4\">\n"
/* C */  "0.58 0.60 0.65 0.69 0.74 0.79 0.82 0.84 "  /* B */
/* B */  "0.86 0.81 0.76 0.76 0.78 0.79 0.83 0.86 "  /* M */
/* M */  "0.93 0.95 0.97 0.98 1.00 1.00 1.00 1.00 "  /* R */
/* R */  "1.00 1.00 1.00 1.00 1.00 1.00 1.00 1.00 "  /* Y */
/* Y */  "1.00 0.99 0.98 0.97 0.96 0.94 0.93 0.89 "  /* G */
/* G */  "0.86 0.73 0.65 0.58 0.59 0.59 0.58 0.58 "  /* C */
"</sequence>\n"
"</curve>\n"
"</gutenprint>\n";

static const char sp960_hue_adj[] =
"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
"<gutenprint>\n"
"<curve wrap=\"wrap\" type=\"linear\" gamma=\"0\">\n"
"<sequence count=\"48\" lower-bound=\"-6\" upper-bound=\"6\">\n"
/* C */  "0.00 0.06 0.10 0.10 0.06 0.00 -.06 -.12 "  /* B */
/* B */  "-.18 -.21 -.22 -.23 -.24 -.25 -.26 -.27 "  /* M */
/* M */  "-.28 -.33 -.38 -.45 -.50 -.40 -.30 -.20 "  /* R */
/* R */  "-.12 -.07 -.04 -.02 0.00 0.00 0.00 0.00 "  /* Y */
/* Y */  "0.00 -.00 -.00 -.00 -.00 -.00 -.00 -.00 "  /* G */
/* G */  "-.00 -.00 -.00 -.00 -.00 -.00 -.00 -.00 "  /* C */
"</sequence>\n"
"</curve>\n"
"</gutenprint>\n";

static const char sp960_matte_sat_adj[] =
"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
"<gutenprint>\n"
"<curve wrap=\"wrap\" type=\"linear\" gamma=\"0\">\n"
"<sequence count=\"48\" lower-bound=\"0\" upper-bound=\"4\">\n"
/* C */  "1.00 1.05 1.15 1.25 1.35 1.45 1.50 1.50 "  /* B */
/* B */  "1.50 1.50 1.50 1.50 1.50 1.50 1.50 1.50 "  /* M */
/* M */  "1.50 1.40 1.30 1.20 1.10 1.00 1.00 1.00 "  /* R */
/* R */  "1.00 1.00 1.00 1.00 1.00 1.00 1.00 1.00 "  /* Y */
/* Y */  "1.00 1.10 1.30 1.55 1.80 1.95 2.00 2.00 "  /* G */
/* G */  "2.00 2.00 2.00 1.95 1.80 1.55 1.30 1.10 "  /* C */
"</sequence>\n"
"</curve>\n"
"</gutenprint>\n";

static const char sp960_matte_lum_adj[] =
"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
"<gutenprint>\n"
"<curve wrap=\"wrap\" type=\"linear\" gamma=\"0\">\n"
"<sequence count=\"48\" lower-bound=\"0\" upper-bound=\"4\">\n"
/* C */  "0.58 0.63 0.70 0.75 0.80 0.86 0.88 0.90 "  /* B */
/* B */  "0.90 0.83 0.78 0.78 0.78 0.79 0.83 0.86 "  /* M */
/* M */  "0.93 0.95 0.97 0.98 1.00 1.00 1.00 1.00 "  /* R */
/* R */  "1.00 1.00 1.00 1.00 1.00 1.00 1.00 1.00 "  /* Y */
/* Y */  "1.00 0.99 0.98 0.97 0.96 0.94 0.93 0.89 "  /* G */
/* G */  "0.86 0.73 0.65 0.58 0.59 0.59 0.58 0.58 "  /* C */
"</sequence>\n"
"</curve>\n"
"</gutenprint>\n";

static const char sp960_matte_hue_adj[] =
"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
"<gutenprint>\n"
"<curve wrap=\"wrap\" type=\"linear\" gamma=\"0\">\n"
"<sequence count=\"48\" lower-bound=\"-6\" upper-bound=\"6\">\n"
/* C */  "0.00 0.06 0.10 0.10 0.06 0.00 -.06 -.12 "  /* B */
/* B */  "-.18 -.21 -.22 -.23 -.24 -.25 -.26 -.27 "  /* M */
/* M */  "-.28 -.33 -.38 -.45 -.50 -.40 -.30 -.20 "  /* R */
/* R */  "-.12 -.07 -.04 -.02 0.00 0.00 0.00 0.00 "  /* Y */
/* Y */  "0.00 -.00 -.00 -.00 -.00 -.00 -.00 -.00 "  /* G */
/* G */  "-.00 -.00 -.00 -.00 -.00 -.00 -.00 -.00 "  /* C */
"</sequence>\n"
"</curve>\n"
"</gutenprint>\n";


static const char ultra_matte_sat_adj[] =
"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
"<gutenprint>\n"
"<curve wrap=\"wrap\" type=\"linear\" gamma=\"0\">\n"
"<sequence count=\"48\" lower-bound=\"0\" upper-bound=\"4\">\n"
/* C */  "1.00 1.00 1.00 1.05 1.10 1.15 1.15 1.15 "  /* B */
/* B */  "1.15 1.15 1.15 1.10 1.10 1.05 1.05 1.00 "  /* M */
/* M */  "1.00 1.00 1.00 1.00 1.00 1.00 1.00 1.00 "  /* R */
/* R */  "1.00 1.00 1.00 1.00 1.00 1.00 1.00 1.00 "  /* Y */
/* Y */  "1.00 1.05 1.15 1.25 1.35 1.45 1.50 1.50 "  /* G */
/* G */  "1.50 1.50 1.50 1.45 1.35 1.25 1.15 1.05 "  /* C */
"</sequence>\n"
"</curve>\n"
"</gutenprint>\n";

static const char ultra_matte_lum_adj[] =
"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
"<gutenprint>\n"
"<curve wrap=\"wrap\" type=\"linear\" gamma=\"0\">\n"
"<sequence count=\"48\" lower-bound=\"0\" upper-bound=\"4\">\n"
/* C */  "0.49 0.51 0.55 0.61 0.67 0.71 0.76 0.79 "  /* B */
/* B */  "0.83 0.80 0.76 0.76 0.78 0.79 0.83 0.86 "  /* M */
/* M */  "0.93 0.95 0.97 0.97 0.97 0.97 0.96 0.96 "  /* R */
/* R */  "0.96 0.97 0.97 0.98 0.99 1.00 1.00 1.00 "  /* Y */
/* Y */  "1.00 0.98 0.97 0.95 0.94 0.93 0.90 0.86 "  /* G */
/* G */  "0.82 0.69 0.60 0.54 0.52 0.51 0.50 0.49 "  /* C */
"</sequence>\n"
"</curve>\n"
"</gutenprint>\n";

static const char ultra_matte_hue_adj[] =
"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
"<gutenprint>\n"
"<curve wrap=\"wrap\" type=\"linear\" gamma=\"0\">\n"
"<sequence count=\"48\" lower-bound=\"-6\" upper-bound=\"6\">\n"
/* C */  "0.00 0.06 0.10 0.10 0.06 0.00 -.06 -.12 "  /* B */
/* B */  "-.18 -.21 -.22 -.22 -.22 -.22 -.22 -.22 "  /* M */
/* M */  "-.22 -.28 -.34 -.40 -.50 -.40 -.30 -.20 "  /* R */
/* R */  "-.12 -.07 -.04 -.02 0.00 0.00 0.00 0.00 "  /* Y */
/* Y */  "0.00 -.00 -.03 -.07 -.11 -.15 -.19 -.22 "  /* G */
/* G */  "-.25 -.22 -.19 -.15 -.12 -.10 -.06 -.03 "  /* C */
"</sequence>\n"
"</curve>\n"
"</gutenprint>\n";

static const char ultra_glossy_sat_adj[] =
"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
"<gutenprint>\n"
"<curve wrap=\"wrap\" type=\"linear\" gamma=\"0\">\n"
"<sequence count=\"48\" lower-bound=\"0\" upper-bound=\"4\">\n"
/* C */  "1.00 1.00 1.00 1.05 1.10 1.15 1.15 1.15 "  /* B */
/* B */  "1.15 1.15 1.15 1.10 1.10 1.05 1.05 1.00 "  /* M */
/* M */  "1.00 1.00 1.00 1.00 1.00 1.00 1.00 1.00 "  /* R */
/* R */  "1.00 1.00 1.00 1.00 1.00 1.00 1.00 1.00 "  /* Y */
/* Y */  "1.00 1.05 1.15 1.25 1.35 1.45 1.50 1.50 "  /* G */
/* G */  "1.50 1.50 1.50 1.45 1.35 1.25 1.15 1.05 "  /* C */
"</sequence>\n"
"</curve>\n"
"</gutenprint>\n";

static const char ultra_glossy_lum_adj[] =
"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
"<gutenprint>\n"
"<curve wrap=\"wrap\" type=\"linear\" gamma=\"0\">\n"
"<sequence count=\"48\" lower-bound=\"0\" upper-bound=\"4\">\n"
/* C */  "0.49 0.53 0.60 0.64 0.69 0.73 0.77 0.80 "  /* B */
/* B */  "0.84 0.81 0.77 0.77 0.78 0.80 0.84 0.87 "  /* M */
/* M */  "0.93 0.95 0.97 0.98 0.98 0.97 0.96 0.96 "  /* R */
/* R */  "0.96 0.97 0.98 0.98 0.99 1.00 1.00 1.00 "  /* Y */
/* Y */  "1.00 0.98 0.97 0.96 0.95 0.93 0.90 0.87 "  /* G */
/* G */  "0.83 0.69 0.61 0.55 0.53 0.52 0.50 0.49 "  /* C */
"</sequence>\n"
"</curve>\n"
"</gutenprint>\n";

static const char ultra_glossy_hue_adj[] =
"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
"<gutenprint>\n"
"<curve wrap=\"wrap\" type=\"linear\" gamma=\"0\">\n"
"<sequence count=\"48\" lower-bound=\"-6\" upper-bound=\"6\">\n"
/* C */  "0.00 0.06 0.10 0.10 0.06 0.00 -.06 -.12 "  /* B */
/* B */  "-.18 -.21 -.22 -.22 -.22 -.22 -.22 -.22 "  /* M */
/* M */  "-.22 -.28 -.34 -.40 -.50 -.40 -.30 -.20 "  /* R */
/* R */  "-.12 -.07 -.04 -.02 0.00 0.00 0.00 0.00 "  /* Y */
/* Y */  "0.00 -.00 -.03 -.07 -.11 -.15 -.19 -.22 "  /* G */
/* G */  "-.25 -.22 -.19 -.15 -.12 -.10 -.06 -.03 "  /* C */
"</sequence>\n"
"</curve>\n"
"</gutenprint>\n";


static const char ultra_k3_matte_sat_adj[] =
"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
"<gutenprint>\n"
"<curve wrap=\"wrap\" type=\"linear\" gamma=\"0\">\n"
"<sequence count=\"48\" lower-bound=\"0\" upper-bound=\"4\">\n"
/* C */  "1.00 1.00 1.00 1.05 1.10 1.15 1.15 1.15 "  /* B */
/* B */  "1.15 1.15 1.15 1.10 1.10 1.05 1.05 1.00 "  /* M */
/* M */  "1.00 1.00 1.00 1.00 1.00 1.00 1.00 1.00 "  /* R */
/* R */  "1.00 1.00 1.00 1.00 1.00 1.00 1.00 1.00 "  /* Y */
/* Y */  "1.00 1.05 1.15 1.25 1.35 1.45 1.50 1.50 "  /* G */
/* G */  "1.50 1.50 1.50 1.45 1.35 1.25 1.15 1.05 "  /* C */
"</sequence>\n"
"</curve>\n"
"</gutenprint>\n";

static const char ultra_k3_matte_lum_adj[] =
"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
"<gutenprint>\n"
"<curve wrap=\"wrap\" type=\"linear\" gamma=\"0\">\n"
"<sequence count=\"48\" lower-bound=\"0\" upper-bound=\"4\">\n"
/* C */  "0.55 0.57 0.61 0.64 0.67 0.69 0.72 0.75 "  /* B */
/* B */  "0.83 0.80 0.76 0.76 0.78 0.79 0.83 0.86 "  /* M */
/* M */  "0.93 0.95 0.97 0.97 0.97 0.97 0.96 0.96 "  /* R */
/* R */  "0.96 0.97 0.97 0.98 0.99 1.00 1.00 1.00 "  /* Y */
/* Y */  "1.00 0.98 0.97 0.95 0.93 0.91 0.88 0.83 "  /* G */
/* G */  "0.83 0.71 0.65 0.61 0.58 0.56 0.55 0.55 "  /* C */
"</sequence>\n"
"</curve>\n"
"</gutenprint>\n";

static const char ultra_k3_matte_hue_adj[] =
"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
"<gutenprint>\n"
"<curve wrap=\"wrap\" type=\"linear\" gamma=\"0\">\n"
"<sequence count=\"48\" lower-bound=\"-6\" upper-bound=\"6\">\n"
/* C */  "0.00 0.06 0.10 0.10 0.06 0.00 -.06 -.12 "  /* B */
/* B */  "-.18 -.21 -.22 -.22 -.22 -.22 -.22 -.22 "  /* M */
/* M */  "-.22 -.28 -.34 -.40 -.50 -.40 -.30 -.20 "  /* R */
/* R */  "-.12 -.07 -.04 -.02 0.00 0.00 0.00 0.00 "  /* Y */
/* Y */  "0.00 -.00 -.03 -.07 -.11 -.15 -.17 -.18 "  /* G */
/* G */  "-.19 -.20 -.19 -.18 -.16 -.12 -.08 -.04 "  /* C */
"</sequence>\n"
"</curve>\n"
"</gutenprint>\n";

static const char ultra_k3_glossy_sat_adj[] =
"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
"<gutenprint>\n"
"<curve wrap=\"wrap\" type=\"linear\" gamma=\"0\">\n"
"<sequence count=\"48\" lower-bound=\"0\" upper-bound=\"4\">\n"
/* C */  "1.00 1.00 1.00 1.05 1.10 1.15 1.15 1.15 "  /* B */
/* B */  "1.15 1.15 1.15 1.10 1.10 1.05 1.05 1.00 "  /* M */
/* M */  "1.00 1.00 1.00 1.00 1.00 1.00 1.00 1.00 "  /* R */
/* R */  "1.00 1.00 1.00 1.00 1.00 1.00 1.00 1.00 "  /* Y */
/* Y */  "1.00 1.05 1.15 1.25 1.35 1.45 1.50 1.50 "  /* G */
/* G */  "1.50 1.50 1.50 1.45 1.35 1.25 1.15 1.05 "  /* C */
"</sequence>\n"
"</curve>\n"
"</gutenprint>\n";

static const char ultra_k3_glossy_lum_adj[] =
"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
"<gutenprint>\n"
"<curve wrap=\"wrap\" type=\"linear\" gamma=\"0\">\n"
"<sequence count=\"48\" lower-bound=\"0\" upper-bound=\"4\">\n"
/* C */  "0.55 0.57 0.61 0.64 0.67 0.69 0.72 0.75 "  /* B */
/* B */  "0.75 0.71 0.70 0.70 0.72 0.76 0.81 0.87 "  /* M */
/* M */  "0.93 0.95 0.97 0.98 0.98 0.97 0.96 0.96 "  /* R */
/* R */  "0.96 0.97 0.98 0.98 0.99 1.00 1.00 1.00 "  /* Y */
/* Y */  "1.00 0.98 0.97 0.96 0.95 0.93 0.90 0.87 "  /* G */
/* G */  "0.83 0.71 0.65 0.61 0.58 0.56 0.55 0.55 "  /* C */
"</sequence>\n"
"</curve>\n"
"</gutenprint>\n";

static const char ultra_k3_glossy_hue_adj[] =
"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
"<gutenprint>\n"
"<curve wrap=\"wrap\" type=\"linear\" gamma=\"0\">\n"
"<sequence count=\"48\" lower-bound=\"-6\" upper-bound=\"6\">\n"
/* C */  "0.00 0.06 0.10 0.10 0.06 0.00 -.06 -.12 "  /* B */
/* B */  "-.18 -.21 -.22 -.22 -.22 -.22 -.22 -.22 "  /* M */
/* M */  "-.22 -.28 -.34 -.40 -.50 -.40 -.30 -.20 "  /* R */
/* R */  "-.12 -.07 -.04 -.02 0.00 0.00 0.00 0.00 "  /* Y */
/* Y */  "0.00 -.00 -.03 -.07 -.11 -.15 -.19 -.22 "  /* G */
/* G */  "-.25 -.22 -.19 -.15 -.12 -.10 -.06 -.03 "  /* C */
"</sequence>\n"
"</curve>\n"
"</gutenprint>\n";


static const char r800_matte_sat_adj[] =
"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
"<gutenprint>\n"
"<curve wrap=\"wrap\" type=\"linear\" gamma=\"0\">\n"
"<sequence count=\"48\" lower-bound=\"0\" upper-bound=\"4\">\n"
/* C */  "1.00 1.00 1.00 1.00 1.00 1.00 1.00 1.00 "  /* B */
/* B */  "1.00 1.00 1.00 1.00 1.00 1.00 1.00 1.00 "  /* M */
/* M */  "1.00 1.00 1.00 1.00 1.00 1.00 1.00 1.00 "  /* R */
/* R */  "1.00 1.00 1.00 1.00 1.00 1.00 1.00 1.00 "  /* Y */
/* Y */  "1.00 1.03 1.06 1.09 1.12 1.15 1.18 1.20 "  /* G */
/* G */  "1.20 1.15 1.10 1.05 1.00 1.00 1.00 1.00 "  /* C */
"</sequence>\n"
"</curve>\n"
"</gutenprint>\n";

static const char r800_matte_lum_adj[] =
"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
"<gutenprint>\n"
"<curve wrap=\"wrap\" type=\"linear\" gamma=\"0\">\n"
"<sequence count=\"48\" lower-bound=\"0\" upper-bound=\"4\">\n"
/* C */  "0.75 0.75 0.75 0.75 0.76 0.80 0.85 0.90 "  /* B */
/* B */  "0.90 0.88 0.82 0.78 0.78 0.82 0.85 0.92 "  /* M */
/* M */  "0.98 0.98 0.97 0.97 0.96 0.96 0.96 0.96 "  /* R */
/* R */  "0.96 0.97 0.98 0.98 0.99 1.00 1.00 1.00 "  /* Y */
/* Y */  "1.00 1.00 1.00 0.99 0.98 0.97 0.96 0.93 "  /* G */
/* G */  "0.88 0.87 0.86 0.85 0.82 0.79 0.76 0.75 "  /* C */
"</sequence>\n"
"</curve>\n"
"</gutenprint>\n";

static const char r800_matte_hue_adj[] =
"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
"<gutenprint>\n"
"<curve wrap=\"wrap\" type=\"linear\" gamma=\"0\">\n"
"<sequence count=\"48\" lower-bound=\"-6\" upper-bound=\"6\">\n"
/* C */  "0.00 -.07 -.10 -.15 -.19 -.25 -.30 -.35 "  /* B */
/* B */  "-.38 -.38 -.30 -.20 -.10 -.00 0.02 0.02 "  /* M */
/* M */  "-.00 -.00 -.00 -.00 -.00 -.00 -.00 0.00 "  /* R */
/* R */  "0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 "  /* Y */
/* Y */  "0.00 0.02 0.05 0.09 0.13 0.15 0.16 0.17 "  /* G */
/* G */  "0.17 0.17 0.16 0.15 0.13 0.09 0.05 0.02 "  /* C */
"</sequence>\n"
"</curve>\n"
"</gutenprint>\n";

static const char r800_glossy_sat_adj[] =
"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
"<gutenprint>\n"
"<curve wrap=\"wrap\" type=\"linear\" gamma=\"0\">\n"
"<sequence count=\"48\" lower-bound=\"0\" upper-bound=\"4\">\n"
/* C */  "1.00 1.00 1.00 1.00 1.00 1.00 1.00 1.00 "  /* B */
/* B */  "1.00 1.00 1.00 1.00 1.00 1.00 1.00 1.00 "  /* M */
/* M */  "1.00 1.00 1.00 1.00 1.00 1.00 1.00 1.00 "  /* R */
/* R */  "1.00 1.00 1.00 1.00 1.00 1.00 1.00 1.00 "  /* Y */
/* Y */  "1.00 1.03 1.06 1.09 1.12 1.15 1.18 1.20 "  /* G */
/* G */  "1.20 1.15 1.10 1.05 1.00 1.00 1.00 1.00 "  /* C */
"</sequence>\n"
"</curve>\n"
"</gutenprint>\n";


static const char r800_glossy_lum_adj[] =
"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
"<gutenprint>\n"
"<curve wrap=\"wrap\" type=\"linear\" gamma=\"0\">\n"
"<sequence count=\"48\" lower-bound=\"0\" upper-bound=\"4\">\n"
/* C */  "0.75 0.75 0.75 0.75 0.76 0.80 0.85 0.90 "  /* B */
/* B */  "0.90 0.88 0.82 0.85 0.87 0.89 0.91 0.95 "  /* M */
/* M */  "0.98 0.98 0.97 0.97 0.96 0.96 0.96 0.96 "  /* R */
/* R */  "0.96 0.97 0.98 0.98 0.99 1.00 1.00 1.00 "  /* Y */
/* Y */  "1.00 1.00 1.00 0.99 0.98 0.97 0.96 0.93 "  /* G */
/* G */  "0.88 0.87 0.86 0.85 0.82 0.79 0.76 0.75 "  /* C */
"</sequence>\n"
"</curve>\n"
"</gutenprint>\n";

static const char r800_glossy_hue_adj[] =
"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
"<gutenprint>\n"
"<curve wrap=\"wrap\" type=\"linear\" gamma=\"0\">\n"
"<sequence count=\"48\" lower-bound=\"-6\" upper-bound=\"6\">\n"
/* C */  "0.00 -.07 -.10 -.15 -.19 -.25 -.30 -.35 "  /* B */
/* B */  "-.38 -.38 -.30 -.20 -.10 -.00 0.00 0.00 "  /* M */
/* M */  "-.00 -.00 -.00 -.00 -.00 -.00 -.00 0.00 "  /* R */
/* R */  "0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 "  /* Y */
/* Y */  "0.00 0.02 0.05 0.09 0.13 0.15 0.16 0.17 "  /* G */
/* G */  "0.17 0.17 0.16 0.15 0.13 0.09 0.05 0.02 "  /* C */
"</sequence>\n"
"</curve>\n"
"</gutenprint>\n";

#define DECLARE_PAPERS(name)			\
static const paperlist_t name##_paper_list =	\
{						\
  #name,					\
  sizeof(name##_papers) / sizeof(paper_t),	\
  name##_papers					\
}

#define DECLARE_PAPER_ADJUSTMENTS(name)					\
static const paper_adjustment_list_t name##_paper_adjustment_list =	\
{									\
  #name,								\
  sizeof(name##_adjustments) / sizeof(paper_adjustment_t),		\
  name##_adjustments							\
}

static const paper_adjustment_t standard_adjustments[] =
{
  { "Plain", 0.615, .5, 1, .075, .9, 1, 1, 1, 1, 1, 1,
    standard_hue_adj, standard_lum_adj, standard_sat_adj },
  { "PlainFast", 0.615, .5, 1, .075, .9, 1, 1, 1, 1, 1, 1,
    standard_hue_adj, standard_lum_adj, standard_sat_adj },
  { "Postcard", 0.83, .5, 1, .075, .9, 1, 1, 1, 1, 1, 1,
    standard_hue_adj, standard_lum_adj, standard_sat_adj },
  { "GlossyFilm", 1.00, 1.0, 1, .15, .999, 1, 1, 1, 1, 1, 1,
    standard_hue_adj, standard_lum_adj, standard_sat_adj },
  { "Transparency", 1.00, .75, 1, .15, .999, 1, 1, 1, 1, 1, 1,
    standard_hue_adj, standard_lum_adj, standard_sat_adj },
  { "Envelope", 0.615, .5, 1, .075, .9, 1, 1, 1, 1, 1, 1,
    standard_hue_adj, standard_lum_adj, standard_sat_adj },
  { "BackFilm", 1.00, .75, 1, .15, .999, 1, 1, 1, 1, 1, 1,
    standard_hue_adj, standard_lum_adj, standard_sat_adj },
  { "Matte", 0.85, .8, 1.0, .15, .999, 1, 1, 1, 1, 1, 1,
    standard_hue_adj, standard_lum_adj, standard_sat_adj },
  { "MatteHeavy", 1.0, 1.0, 1, .15, .999, 1, 1, 1, 1, 1, 1,
    standard_hue_adj, standard_lum_adj, standard_sat_adj },
  { "Inkjet", 0.85, .5, 1, .10, .9, 1, 1, 1, 1, 1, 1,
    standard_hue_adj, standard_lum_adj, standard_sat_adj },
  { "Coated", 1.10, 1.0, 1, .15, .999, 1, 1, 1, 1, 1, 1,
    standard_hue_adj, standard_lum_adj, standard_sat_adj },
  { "Photo", 1.00, 1.0, 1, .15, .999, 1, 1, 1, 1, 1, 1,
    standard_hue_adj, standard_lum_adj, standard_sat_adj },
  { "GlossyPhoto", 1.10, 1.0, 1, .15, .999, 1, 1, 1, 1, 1, 1,
    standard_hue_adj, standard_lum_adj, standard_sat_adj },
  { "Semigloss", 1.00, 1.0, 1, .15, .999, 1, 1, 1, 1, 1, 1,
    standard_hue_adj, standard_lum_adj, standard_sat_adj },
  { "Luster", 1.00, 1.0, 1, .15, .999, 1, 1, 1, 1, 1, 1,
    standard_hue_adj, standard_lum_adj, standard_sat_adj },
  { "GlossyPaper", 1.00, 1.0, 1, .15, .999, 1, 1, 1, 1, 1, 1,
    standard_hue_adj, standard_lum_adj, standard_sat_adj },
  { "Ilford", 1.0, 1.0, 1, .15, 1.35, 1, 1, 1, 1, 1, 1,
    standard_hue_adj, standard_lum_adj, standard_sat_adj  },
  { "ColorLife", 1.00, 1.0, 1, .15, .9, 1, 1, 1, 1, 1, 1,
    standard_hue_adj, standard_lum_adj, standard_sat_adj },
  { "Other", 0.615, .5, 1, .075, .9, 1, 1, 1, 1, 1, 1,
    standard_hue_adj, standard_lum_adj, standard_sat_adj },
};

DECLARE_PAPER_ADJUSTMENTS(standard);

static const paper_adjustment_t photo_adjustments[] =
{
  { "Plain", 0.615, .25, 1, .15, .9, 1, 1, 1, 1, 1, 1,
    standard_hue_adj, standard_lum_adj, standard_sat_adj },
  { "PlainFast", 0.615, .25, 1, .15, .9, 1, 1, 1, 1, 1, 1,
    standard_hue_adj, standard_lum_adj, standard_sat_adj },
  { "Postcard", 0.83, .25, 1, .15, .9, 1, 1, 1, 1, 1, 1,
    standard_hue_adj, standard_lum_adj, standard_sat_adj },
  { "GlossyFilm", 1.00, 1.0, 1, .2, .999, 1, 1, 1, 1, 1, 1,
    standard_hue_adj, standard_lum_adj, standard_sat_adj },
  { "Transparency", 1.00, .75, 1, .2, .999, 1, 1, 1, 1, 1, 1,
    standard_hue_adj, standard_lum_adj, standard_sat_adj },
  { "Envelope", 0.615, .25, 1, .15, .9, 1, 1, 1, 1, 1, 1,
    standard_hue_adj, standard_lum_adj, standard_sat_adj },
  { "BackFilm", 1.00, .75, 1, .2, .999, 1, 1, 1, 1, 1, 1,
    standard_hue_adj, standard_lum_adj, standard_sat_adj },
  { "Matte", 0.85, .8, 1.0, .2, .999, 1, 1, 1, 1, 1, 1,
    standard_hue_adj, standard_lum_adj, standard_sat_adj },
  { "MatteHeavy", 1.0, 1.0, 1, .35, .999, 1, 1, 1, 1, 1, 1,
    standard_hue_adj, standard_lum_adj, standard_sat_adj },
  { "Inkjet", 0.85, .375, 1, .2, .9, 1, 1, 1, 1, 1, 1,
    standard_hue_adj, standard_lum_adj, standard_sat_adj },
  { "Coated", 1.10, 1.0, 1, .35, .999, 1, 1, 1, 1, 1, 1,
    standard_hue_adj, standard_lum_adj, standard_sat_adj },
  { "Photo", 1.00, 1.00, 1, .35, .999, 1, 1, 1, 1, 1, 1,
    standard_hue_adj, standard_lum_adj, standard_sat_adj },
  { "GlossyPhoto", 1.10, 1.0, 1, .35, .999, 1, 1, 1, 1, 1, 1,
    standard_hue_adj, standard_lum_adj, standard_sat_adj },
  { "Semigloss", 1.00, 1.0, 1, .35, .999, 1, 1, 1, 1, 1, 1,
    standard_hue_adj, standard_lum_adj, standard_sat_adj },
  { "Luster", 1.00, 1.0, 1, .35, .999, 1, 1, 1, 1, 1, 1,
    standard_hue_adj, standard_lum_adj, standard_sat_adj },
  { "GlossyPaper", 1.00, 1.0, 1, .35, .999, 1, 1, 1, 1, 1, 1,
    standard_hue_adj, standard_lum_adj, standard_sat_adj },
  { "Ilford", 1.0, 1.0, 1, .35, 1.35, 1, 1, 1, 1, 1, 1,
    standard_hue_adj, standard_lum_adj, standard_sat_adj  },
  { "ColorLife", 1.00, 1.0, 1, .35, .9, 1, 1, 1, 1, 1, 1,
    standard_hue_adj, standard_lum_adj, standard_sat_adj },
  { "Other", 0.615, .25, 1, .15, .9, 1, 1, 1, 1, 1, 1,
    standard_hue_adj, standard_lum_adj, standard_sat_adj },
};

DECLARE_PAPER_ADJUSTMENTS(photo);

static const paper_adjustment_t photo2_adjustments[] =
{
  { "Plain", 0.738, 1.0, 0.5, .1, .9, 1, 1, 1, 1, 1, 1.0,
    photo2_hue_adj, photo2_lum_adj, photo2_sat_adj },
  { "PlainFast", 0.738, 1.0, 0.5, .1, .9, 1, 1, 1, 1, 1, 1.0,
    photo2_hue_adj, photo2_lum_adj, photo2_sat_adj },
  { "Postcard", 0.83, 1.0, 0.5, .1, .9, 1, 1, 1, 1, 1, 1.0,
    photo2_hue_adj, photo2_lum_adj, photo2_sat_adj },
  { "GlossyFilm", 1.00, 1.0, 0.5, .2, .999, 1, 1, 1, 1, 1, 1,
    photo2_hue_adj, photo2_lum_adj, photo2_sat_adj },
  { "Transparency", 1.00, 1.0, 0.25, .2, .999, 1, 1, 1, 1, 1, 1,
    photo2_hue_adj, photo2_lum_adj, photo2_sat_adj },
  { "Envelope", 0.738, 1.0, 0.5, .1, .9, 1, 1, 1, 1, 1, 1,
    photo2_hue_adj, photo2_lum_adj, photo2_sat_adj },
  { "BackFilm", 1.00, 1.0, 0.25, .2, .999, 1, 1, 1, 1, 1, 1,
    photo2_hue_adj, photo2_lum_adj, photo2_sat_adj },
  { "Matte", 0.85, 1.0, 0.4, .3, .999, 1, 1, 1, 1, 1, 1,
    photo2_hue_adj, photo2_lum_adj, photo2_sat_adj },
  { "MatteHeavy", 0.85, 1.0, .3, .2, .999, 1, 1, 1, 1, 1, 1,
    photo2_hue_adj, photo2_lum_adj, photo2_sat_adj },
  { "Inkjet", 0.85, 1.0, 0.5, .15, .9, 1, 1, 1, 1, 1, 1,
    photo2_hue_adj, photo2_lum_adj, photo2_sat_adj },
  { "Coated", 1.2, 1.0, .25, .15, .999, .89, 1, 1, .9, 1, 1.,
    photo2_hue_adj, photo2_lum_adj, photo2_sat_adj },
  { "Photo", 1.00, 1.0, 0.25, .2, .999, 1, 1, 1, 1, 1, 1,
    photo2_hue_adj, photo2_lum_adj, photo2_sat_adj },
  { "GlossyPhoto", 1.0, 1.0, 0.5, .3, .999, .9, .98, 1, .9, 1, 0.92,
    photo2_hue_adj, photo2_lum_adj, photo2_sat_adj },
  { "Semigloss", 1.0, 1.0, 0.5, .3, .999, .9, .98, 1, .9, 1, 0.92,
    photo2_hue_adj, photo2_lum_adj, photo2_sat_adj },
  { "Luster", 1.0, 1.0, 0.5, .3, .999, .9, .98, 1, .9, 1, 0.92,
    photo2_hue_adj, photo2_lum_adj, photo2_sat_adj },
  { "GlossyPaper", 1.00, 1.0, 0.25, .2, .999, 1, 1, 1, 1, 1, 1,
    photo2_hue_adj, photo2_lum_adj, photo2_sat_adj },
  { "Ilford", .85, 1.0, 0.25, .2, .999, 1, 1, 1, 1, 1, 1,
    photo2_hue_adj, photo2_lum_adj, photo2_sat_adj  },
  { "ColorLife", 1.00, 1.0, 0.25, .2, .9, 1, 1, 1, 1, 1, 1,
    photo2_hue_adj, photo2_lum_adj, photo2_sat_adj },
  { "Other", 0.738, 1.0, 0.5, .1, .9, 1, 1, 1, 1, 1, 1,
    photo2_hue_adj, photo2_lum_adj, photo2_sat_adj },
};

DECLARE_PAPER_ADJUSTMENTS(photo2);

static const paper_adjustment_t photo3_adjustments[] =
{
  { "Plain",        0.615, .35, 0.75, .15, .9, 1, .85, .85, .9, 1, 1.0,
    photo3_hue_adj, photo3_lum_adj, photo3_sat_adj },
  { "PlainFast",    0.615, .35, 0.75, .15, .9, 1, .85, .85, .9, 1, 1.0,
    photo3_hue_adj, photo3_lum_adj, photo3_sat_adj },
  { "Postcard",     0.692, .35, 0.5, .2, .9, 1, .85, .85, .9, 1, 1.0,
    photo3_hue_adj, photo3_lum_adj, photo3_sat_adj },
  { "GlossyFilm",   0.833, .5, 0.75, .2, .999, 1, .7, .8, .9, 1, 1,
    photo3_hue_adj, photo3_lum_adj, photo3_sat_adj },
  { "Transparency", 0.833, .35, 0.75, .2, .999, 1, .59, .7, .9, 1, 1,
    photo3_hue_adj, photo3_lum_adj, photo3_sat_adj },
  { "Envelope",     0.615, .35, 0.75, .15, .9, 1, .85, .85, .9, 1, 1.0,
    photo3_hue_adj, photo3_lum_adj, photo3_sat_adj },
  { "BackFilm",     0.833, .5, 0.75, .2, .999, 1, .59, .7, .9, 1, 1,
    photo3_hue_adj, photo3_lum_adj, photo3_sat_adj },
  { "Matte",        0.833, .35, 0.5, .25, .999, 1, .67, .72, .9, 1, 1,
    photo3_hue_adj, photo3_lum_adj, photo3_sat_adj },
  { "MatteHeavy",   0.833, .35, 0.5, .25, .999, 1, .85, .85, .9, 1, 1,
    photo3_hue_adj, photo3_lum_adj, photo3_sat_adj },
  { "Inkjet",       0.709, .5, 0.75, .2, .9, 1, .85, .85, .9, 1, 1,
    photo3_hue_adj, photo3_lum_adj, photo3_sat_adj },
  { "Coated",       0.833, .45, 0.5, .25, .999, 1, .76, .84, .66, 1, 1,
    photo3_hue_adj, photo3_lum_adj, photo3_sat_adj },
  { "Photo",        0.833, .5, 0.5, .25, .999, 1, .59, .7, .9, 1, 1,
    photo3_hue_adj, photo3_lum_adj, photo3_sat_adj },
  { "GlossyPhoto",  0.75, .5, 0.3, .25, 1.05, 1, .85, .85, .66, 1, 0.92,
    photo3_hue_adj, photo3_lum_adj, photo3_sat_adj },
  { "Semigloss",    0.75, .5, 0.3, .25, .999, 1, .85, .85, .66, 1, 0.92,
    photo3_hue_adj, photo3_lum_adj, photo3_sat_adj },
  { "Luster",       0.75, .5, 0.3, .25, .999, 1, .85, .85, .66, 1, 0.92,
    photo3_hue_adj, photo3_lum_adj, photo3_sat_adj },
  { "GlossyPaper",  0.833, .5, 0.75, .2, .999, 1, .59, .7, .9, 1, 1,
    photo3_hue_adj, photo3_lum_adj, photo3_sat_adj },
  { "Ilford",       0.833, .5, 0.75, .2, .999, 1, .59, .7, .9, 1, 1,
    photo3_hue_adj, photo3_lum_adj, photo3_sat_adj  },
  { "ColorLife",    0.833, .5, 0.75, .2, .9, 1, .59, .7, .9, 1, 1,
    photo3_hue_adj, photo3_lum_adj, photo3_sat_adj },
  { "Other",        0.615, .35, 0.5, .5, .9, 1, .85, .85, .9, 1, 1,
    photo3_hue_adj, photo3_lum_adj, photo3_sat_adj },
};

DECLARE_PAPER_ADJUSTMENTS(photo3);

static const paper_adjustment_t claria_adjustments[] =
{
  { "Plain",        0.540, .25, 0.75, .1, .5, 1, .7, .7, 1, 1, 1.0,
    claria_hue_adj, claria_lum_adj, claria_sat_adj },
  { "PlainFast",    0.540, .25, 0.75, .1, .5, 1, .7, .7, 1, 1, 1.0,
    claria_hue_adj, claria_lum_adj, claria_sat_adj },
  { "Postcard",     0.692, .25, 0.5, .1, .5, 1, .7, .7, 1, 1, 1.0,
    claria_hue_adj, claria_lum_adj, claria_sat_adj },
  { "GlossyFilm",   0.833, .25, 0.75, .2, .999, 1, .7, .7, 1, 1, 1,
    claria_hue_adj, claria_lum_adj, claria_sat_adj },
  { "Transparency", 0.833, .25, 0.75, .2, .999, 1, .7, .7, 1, 1, 1,
    claria_hue_adj, claria_lum_adj, claria_sat_adj },
  { "Envelope",     0.540, .25, 0.75, .1, .5, 1, .7, .7, 1, 1, 1.0,
    claria_hue_adj, claria_lum_adj, claria_sat_adj },
  { "BackFilm",     0.833, .25, 0.75, .2, .999, 1, .7, .7, 1, 1, 1,
    claria_hue_adj, claria_lum_adj, claria_sat_adj },
  { "Matte",        0.833, .25, 0.6, .15, .999, 1, .7, .7, 1, 1, 1,
    claria_hue_adj, claria_lum_adj, claria_sat_adj },
  { "MatteHeavy",   0.833, .25, 0.5, .25, .999, 1, .7, .7, 1, 1, 1,
    claria_hue_adj, claria_lum_adj, claria_sat_adj },
  { "Inkjet",       0.709, .25, 0.75, .15, .75, 1, .7, .7, 1, 1, 1,
    claria_hue_adj, claria_lum_adj, claria_sat_adj },
  { "Coated",       0.833, .25, 0.5, .25, .999, 1, .7, .7, 1, 1, 1,
    claria_hue_adj, claria_lum_adj, claria_sat_adj },
  { "Photo",        0.833, .25, 0.5, .25, .999, 1, .7, .7, 1, 1, 1,
    claria_hue_adj, claria_lum_adj, claria_sat_adj },
  { "GlossyPhoto",  0.75, .25, 0.3, .25, .999, 1, .7, .7, 1, 1, 0.92,
    claria_hue_adj, claria_lum_adj, claria_sat_adj },
  { "Semigloss",    0.75, .25, 0.3, .25, .999, 1, .7, .7, 1, 1, 0.92,
    claria_hue_adj, claria_lum_adj, claria_sat_adj },
  { "Luster",       0.75, .25, 0.3, .25, .999, 1, .7, .7, 1, 1, 0.92,
    claria_hue_adj, claria_lum_adj, claria_sat_adj },
  { "GlossyPaper",  0.833, .25, 0.75, .2, .999, 1, .7, .7, 1, 1, 1,
    claria_hue_adj, claria_lum_adj, claria_sat_adj },
  { "Ilford",       0.833, .25, 0.75, .2, .999, 1, .7, .7, 1, 1, 1,
    claria_hue_adj, claria_lum_adj, claria_sat_adj  },
  { "ColorLife",    0.833, .25, 0.75, .2, .9, 1, .7, .7, 1, 1, 1,
    claria_hue_adj, claria_lum_adj, claria_sat_adj },
  { "Other",        0.540, .25, 0.5, .1, .5, 1, .7, .7, 1, 1, 1,
    claria_hue_adj, claria_lum_adj, claria_sat_adj },
};

DECLARE_PAPER_ADJUSTMENTS(claria);

static const paper_adjustment_t sp960_adjustments[] =
{
  { "Plain",        0.86, .2,  0.4, .1,   .9,   .9, 1, 1, 1, 1, 1,
    sp960_matte_hue_adj, sp960_matte_lum_adj, sp960_matte_sat_adj },
  { "PlainFast",    0.86, .2,  0.4, .1,   .9,   1, 1, 1, 1, 1, 1,
    sp960_matte_hue_adj, sp960_matte_lum_adj, sp960_matte_sat_adj },
  { "Postcard",     0.90, .2,  0.4, .1,   .9,   .9, 1, 1, 1, 1, 1,
    sp960_matte_hue_adj, sp960_matte_lum_adj, sp960_matte_sat_adj },
  { "GlossyFilm",   0.9,  .3,  0.4, .2,   .999, 1, 1, 1, 1, 1, 1,
    sp960_matte_hue_adj, sp960_matte_lum_adj, sp960_matte_sat_adj },
  { "Transparency", 0.9,  .2,  0.4, .1,   .9,   1, 1, 1, 1, 1, 1,
    sp960_matte_hue_adj, sp960_matte_lum_adj, sp960_matte_sat_adj },
  { "Envelope",     0.86, .2,  0.4, .1,   .9,   1, 1, 1, 1, 1, 1,
    sp960_matte_hue_adj, sp960_matte_lum_adj, sp960_matte_sat_adj },
  { "BackFilm",     0.9,  .2,  0.4, .1,   .9,   1, 1, 1, 1, 1, 1,
    sp960_matte_hue_adj, sp960_matte_lum_adj, sp960_matte_sat_adj },
  { "Matte",        0.9,  .25, 0.4, .2,   .9,   1, 1, 1, 1, 1, 1,
    sp960_matte_hue_adj, sp960_matte_lum_adj, sp960_matte_sat_adj },
  { "MatteHeavy",   0.9,  .3,  0.4, .2,   .999, 1, 1, 1, 1, 1, 1,
    sp960_matte_hue_adj, sp960_matte_lum_adj, sp960_matte_sat_adj },
  { "Inkjet",       0.9,  .2,  0.4, .15,  .9,   1, 1, 1, 1, 1, 1,
    sp960_matte_hue_adj, sp960_matte_lum_adj, sp960_matte_sat_adj },
  { "Coated",       0.9,  .3,  0.4, .2,   .999, 1, 1, 1, 1, 1, 1,
    sp960_matte_hue_adj, sp960_matte_lum_adj, sp960_matte_sat_adj },
  { "Photo",        0.9,  .3,  0.4, .2,   .999, 1, 1, 1, 1, 1, 1,
    sp960_matte_hue_adj, sp960_matte_lum_adj, sp960_matte_sat_adj },
  { "GlossyPhoto",  0.9,  .3,  0.4, .2,   .999, 1, 1, 1, 1, 1, 1,
    sp960_hue_adj, sp960_lum_adj, sp960_sat_adj },
  { "Semigloss",    0.9,  .3,  0.4, .2,   .999, 1, 1, 1, 1, 1, 1,
    sp960_hue_adj, sp960_lum_adj, sp960_sat_adj },
  { "Luster",       0.9,  .3,  0.4, .2,   .999, 1, 1, 1, 1, 1, 1,
    sp960_hue_adj, sp960_lum_adj, sp960_sat_adj },
  { "GlossyPaper",  0.9,  .3,  0.4, .15,  .9,   1, 1, 1, 1, 1, 1,
    sp960_matte_hue_adj, sp960_matte_lum_adj, sp960_matte_sat_adj },
  { "Ilford",       0.85, .3,  0.4, .15, 1.35,  1, 1, 1, 1, 1, 1,
    sp960_matte_hue_adj, sp960_matte_lum_adj, sp960_matte_sat_adj  },
  { "ColorLife",    0.9,  .3,  0.4, .15,  .9,   1, 1, 1, 1, 1, 1,
    sp960_matte_hue_adj, sp960_matte_lum_adj, sp960_matte_sat_adj },
  { "Other",        0.86, .2,  0.4, .1,   .9,   1, 1, 1, 1, 1, 1,
    sp960_matte_hue_adj, sp960_matte_lum_adj, sp960_matte_sat_adj },
};

DECLARE_PAPER_ADJUSTMENTS(sp960);

static const paper_adjustment_t ultrachrome_photo_adjustments[] =
{
  { "Plain", 0.72, .1, 1, .01, 1.5, 1, 1, 1, 1, 1, 1.0,
    ultra_matte_hue_adj, ultra_matte_lum_adj, ultra_matte_sat_adj },
  { "PlainFast", 0.72, .1, 1, .01, 1.5, 1, 1, 1, 1, 1, 1.0,
    ultra_matte_hue_adj, ultra_matte_lum_adj, ultra_matte_sat_adj },
  { "Postcard", 0.72, .1, 1, .01, 1.5, 1, 1, 1, 1, 1, 1.0,
    ultra_matte_hue_adj, ultra_matte_lum_adj, ultra_matte_sat_adj },
  { "GlossyFilm", 0.83, 1.0, 1, .01, 1.5, 1, 1, 1, 1, 1, 1.0,
    ultra_matte_hue_adj, ultra_matte_lum_adj, ultra_matte_sat_adj },
  { "Transparency", 0.83, .75, 1, .01, 1.5, 1, 1, 1, 1, 1, 1.0,
    ultra_matte_hue_adj, ultra_matte_lum_adj, ultra_matte_sat_adj },
  { "Envelope", 0.72, .1, 1, .01, 1.5, 1, 1, 1, 1, 1, 1.0,
    ultra_matte_hue_adj, ultra_matte_lum_adj, ultra_matte_sat_adj },
  { "BackFilm", 0.83, .75, 1, .01, 1.5, 1, 1, 1, 1, 1, 1.0,
    ultra_matte_hue_adj, ultra_matte_lum_adj, ultra_matte_sat_adj },
  { "Matte", 0.92, 0.4, 1, .01, 1.5, 1, 1, 1, 1, 1, 1.0,
    ultra_matte_hue_adj, ultra_matte_lum_adj, ultra_matte_sat_adj },
  { "MatteHeavy", 0.92, 0.4, 1, .01, 1.5, 1, 1, 1, 1, 1, 1.0,
    ultra_matte_hue_adj, ultra_matte_lum_adj, ultra_matte_sat_adj },
  { "Inkjet", 0.72, .5, 1, .01, 1.5, 1, 1, 1, 1, 1, 1.0,
    ultra_matte_hue_adj, ultra_matte_lum_adj, ultra_matte_sat_adj },
  { "Coated", 0.83, .5, 1, .01, 1.5, 1, 1, 1, 1, 1, 1.0,
    ultra_matte_hue_adj, ultra_matte_lum_adj, ultra_matte_sat_adj },
  { "Photo", 1.0, .75, 1, .01, 1.5, 1, 1, 1, 1, 1, 1.0,
    ultra_matte_hue_adj, ultra_matte_lum_adj, ultra_matte_sat_adj },
  { "GlossyPhoto", 0.72, 1, 1, .01, 1.8, 1, 1, 1, 1, 1, .92,
    ultra_glossy_hue_adj, ultra_glossy_lum_adj, ultra_glossy_sat_adj },
  { "Semigloss", 0.72, .8, 1, .01, 1.8, 1, 1, 1, 1, 1, .92,
    ultra_glossy_hue_adj, ultra_glossy_lum_adj, ultra_glossy_sat_adj },
  { "Luster", 0.72, .8, 1, .01, 1.8, 1, 1, 1, 1, 1, .92,
    ultra_glossy_hue_adj, ultra_glossy_lum_adj, ultra_glossy_sat_adj },
  { "ArchivalMatte", 0.92, .4, 1, .01, 1.5, 1, 1, 1, 1, 1, 1.0,
    ultra_matte_hue_adj, ultra_matte_lum_adj, ultra_matte_sat_adj },
  { "WaterColorRadiant", 0.92, .4, 1, .01, 1.5, 1, 1, 1, 1, 1, 1.0,
    ultra_matte_hue_adj, ultra_matte_lum_adj, ultra_matte_sat_adj },
  { "GlossyPaper", 0.83, 1.0, 1, .01, 1.5, 1, 1, 1, 1, 1, 1.0,
    ultra_matte_hue_adj, ultra_matte_lum_adj, ultra_matte_sat_adj },
  { "Ilford", 0.83, 1.0, 1, .01, 1.5, 1, 1, 1, 1, 1, 1.0,
    ultra_matte_hue_adj, ultra_matte_lum_adj, ultra_matte_sat_adj  },
  { "ColorLife", 0.83, 1.0, 1, .01, 1.5, 1, 1, 1, 1, 1, 1.0,
    ultra_matte_hue_adj, ultra_matte_lum_adj, ultra_matte_sat_adj },
  { "Other", 0.72, .1, 1, .01, 1.5, 1, 1, 1, 1, 1, 1.0,
    ultra_matte_hue_adj, ultra_matte_lum_adj, ultra_matte_sat_adj },
};

DECLARE_PAPER_ADJUSTMENTS(ultrachrome_photo);

static const paper_adjustment_t ultrachrome_matte_adjustments[] =
{
  { "Plain", 0.72, .1, 1, 0, 0.5, 1, 1, 1, .6, 1, 1.0,
    ultra_matte_hue_adj, ultra_matte_lum_adj, ultra_matte_sat_adj },
  { "PlainFast", 0.72, .1, 1, 0, 0.5, 1, 1, 1, .6, 1, 1.0,
    ultra_matte_hue_adj, ultra_matte_lum_adj, ultra_matte_sat_adj },
  { "Postcard", 0.72, .1, 1, 0, 0.5, 1, 1, 1, .6, 1, 1.0,
    ultra_matte_hue_adj, ultra_matte_lum_adj, ultra_matte_sat_adj },
  { "GlossyFilm", 0.83, .5, 1, 0.01, 0.5, 1, 1, 1, .6, 1, 1.0,
    ultra_matte_hue_adj, ultra_matte_lum_adj, ultra_matte_sat_adj },
  { "Transparency", 0.83, .5, 1, 0.01, 0.5, 1, 1, 1, .6, 1, 1.0,
    ultra_matte_hue_adj, ultra_matte_lum_adj, ultra_matte_sat_adj },
  { "Envelope", 0.72, .1, 1, 0, 0.5, 1, 1, 1, .6, 1, 1.0,
    ultra_matte_hue_adj, ultra_matte_lum_adj, ultra_matte_sat_adj },
  { "BackFilm", 0.83, .5, 1, 0.01, 0.5, 1, 1, 1, .6, 1, 1.0,
    ultra_matte_hue_adj, ultra_matte_lum_adj, ultra_matte_sat_adj },
  { "Matte", 0.92, 0.4, 1, 0.00, 1.25, 1, 1, 1, .6, 1, 1.0,
    ultra_matte_hue_adj, ultra_matte_lum_adj, ultra_matte_sat_adj },
  { "MatteHeavy", 0.92, 0.4, .4, .01, 0.5, 1, 1, 1, .6, 1, 1.0,
    ultra_matte_hue_adj, ultra_matte_lum_adj, ultra_matte_sat_adj },
  { "Inkjet", 0.72, .3, 1, .01, 0.5, 1, 1, 1, .6, 1, 1.0,
    ultra_matte_hue_adj, ultra_matte_lum_adj, ultra_matte_sat_adj },
  { "Coated", 0.83, .4, 1, .01, 0.5, 1, 1, 1, .6, 1, 1.0,
    ultra_matte_hue_adj, ultra_matte_lum_adj, ultra_matte_sat_adj },
  { "Photo", 1.0, 0.5, 1, 0.01, 0.5, 1, 1, 1, .6, 1, 1.0,
    ultra_matte_hue_adj, ultra_matte_lum_adj, ultra_matte_sat_adj },
  { "GlossyPhoto", 0.72, 1, 1, .01, 0.5, 1, 1, 1, .6, 1, .92,
    ultra_glossy_hue_adj, ultra_glossy_lum_adj, ultra_glossy_sat_adj },
  { "Semigloss", 0.72, .8, 1, .01, 0.5, 1, 1, 1, .6, 1, .92,
    ultra_glossy_hue_adj, ultra_glossy_lum_adj, ultra_glossy_sat_adj },
  { "Luster", 0.72, .8, 1, .01, 0.5, 1, 1, 1, 1, 1, .92,
    ultra_glossy_hue_adj, ultra_glossy_lum_adj, ultra_glossy_sat_adj },
  { "WaterColorRadiant", 0.92, 0.4, 1, .01, 0.5, 1, 1, 1, .6, 1, 1.0,
    ultra_matte_hue_adj, ultra_matte_lum_adj, ultra_matte_sat_adj },
  { "GlossyPaper", 0.83, 0.5, 1, 0.01, 0.5, 1, 1, 1, .6, 1, 1.0,
    ultra_matte_hue_adj, ultra_matte_lum_adj, ultra_matte_sat_adj },
  { "Ilford", 0.83, 0.5, 1, 0.01, 0.5, 1, 1, 1, .6, 1, 1.0,
    ultra_matte_hue_adj, ultra_matte_lum_adj, ultra_matte_sat_adj  },
  { "ColorLife", 0.83, 0.5, 1, 0.01, 0.5, 1, 1, 1, .6, 1, 1.0,
    ultra_matte_hue_adj, ultra_matte_lum_adj, ultra_matte_sat_adj },
  { "Other", 0.72, .1, .4, 0, 0.5, 1, 1, 1, .6, 1, 1.0,
    ultra_matte_hue_adj, ultra_matte_lum_adj, ultra_matte_sat_adj },
};

DECLARE_PAPER_ADJUSTMENTS(ultrachrome_matte);

static const paper_adjustment_t ultrachrome_k3_photo_adjustments[] =
{
  { "Plain", 0.72, .8, 1, .01, 1.5, 1, .95, .9, 1, 1, 1.0,
    ultra_k3_matte_hue_adj, ultra_k3_matte_lum_adj, ultra_k3_matte_sat_adj },
  { "PlainFast", 0.72, .8, 1, .01, 1.5, 1, .95, .9, 1, 1, 1.0,
    ultra_k3_matte_hue_adj, ultra_k3_matte_lum_adj, ultra_k3_matte_sat_adj },
  { "Postcard", 0.72, .8, 1, .01, 1.5, 1, .95, .9, 1, 1, 1.0,
    ultra_k3_matte_hue_adj, ultra_k3_matte_lum_adj, ultra_k3_matte_sat_adj },
  { "GlossyFilm", 0.83, 1.0, 1, .01, 1.5, 1, .95, .9, 1, 1, 1.0,
    ultra_k3_matte_hue_adj, ultra_k3_matte_lum_adj, ultra_k3_matte_sat_adj },
  { "Transparency", 0.83, .75, 1, .01, 1.5, 1, .95, .9, 1, 1, 1.0,
    ultra_k3_matte_hue_adj, ultra_k3_matte_lum_adj, ultra_k3_matte_sat_adj },
  { "Envelope", 0.72, .8, 1, .01, 1.5, 1, .95, .9, 1, 1, 1.0,
    ultra_k3_matte_hue_adj, ultra_k3_matte_lum_adj, ultra_k3_matte_sat_adj },
  { "BackFilm", 0.83, .75, 1, .01, 1.5, 1, .95, .9, 1, 1, 1.0,
    ultra_k3_matte_hue_adj, ultra_k3_matte_lum_adj, ultra_k3_matte_sat_adj },
  { "Matte", 0.92, 0.8, 1, .01, 1.5, 1, .95, .9, 1, 1, 1.0,
    ultra_k3_matte_hue_adj, ultra_k3_matte_lum_adj, ultra_k3_matte_sat_adj },
  { "MatteHeavy", 0.92, 0.8, 1, .01, 1.5, 1, .95, .9, 1, 1, 1.0,
    ultra_k3_matte_hue_adj, ultra_k3_matte_lum_adj, ultra_k3_matte_sat_adj },
  { "Inkjet", 0.72, .8, 1, .01, 1.5, 1, .95, .9, 1, 1, 1.0,
    ultra_k3_matte_hue_adj, ultra_k3_matte_lum_adj, ultra_k3_matte_sat_adj },
  { "Coated", 0.83, .8, 1, .01, 1.5, 1, .95, .9, 1, 1, 1.0,
    ultra_k3_matte_hue_adj, ultra_k3_matte_lum_adj, ultra_k3_matte_sat_adj },
  { "Photo", 1.0, .5, 1, .01, 1.5, 1, .95, .9, 1, 1, 1.0,
    ultra_k3_matte_hue_adj, ultra_k3_matte_lum_adj, ultra_k3_matte_sat_adj },
  { "GlossyPhoto", 0.72, .8, 1, .01, 1.8, 1, .95, .9, 1, 1, .92,
    ultra_k3_glossy_hue_adj, ultra_k3_glossy_lum_adj, ultra_k3_glossy_sat_adj },
  { "Semigloss", 0.72, .8, 1, .01, 1.8, 1, .95, .9, 1, 1, .92,
    ultra_k3_glossy_hue_adj, ultra_k3_glossy_lum_adj, ultra_k3_glossy_sat_adj },
  { "Luster", 0.72, .8, 1, .01, 1.8, 1, .95, .9, 1, 1, .92,
    ultra_k3_glossy_hue_adj, ultra_k3_glossy_lum_adj, ultra_k3_glossy_sat_adj },
  { "ArchivalMatte", 0.92, .8, 1, .01, 1.5, 1, .95, .9, 1, 1, 1.0,
    ultra_k3_matte_hue_adj, ultra_k3_matte_lum_adj, ultra_k3_matte_sat_adj },
  { "WaterColorRadiant", 0.92, .8, 1, .01, 1.5, 1, .95, .9, 1, 1, 1.0,
    ultra_k3_matte_hue_adj, ultra_k3_matte_lum_adj, ultra_k3_matte_sat_adj },
  { "GlossyPaper", 0.83, 1.0, 1, .01, 1.5, 1, .95, .9, 1, 1, 1.0,
    ultra_k3_matte_hue_adj, ultra_k3_matte_lum_adj, ultra_k3_matte_sat_adj },
  { "Ilford", 0.83, 1.0, 1, .01, 1.5, 1, .95, .9, 1, 1, 1.0,
    ultra_k3_matte_hue_adj, ultra_k3_matte_lum_adj, ultra_k3_matte_sat_adj  },
  { "ColorLife", 0.83, 1.0, 1, .01, 1.5, 1, .95, .9, 1, 1, 1.0,
    ultra_k3_matte_hue_adj, ultra_k3_matte_lum_adj, ultra_k3_matte_sat_adj },
  { "Other", 0.72, .1, 1, .01, 1.5, 1, .95, .9, 1, 1, 1.0,
    ultra_k3_matte_hue_adj, ultra_k3_matte_lum_adj, ultra_k3_matte_sat_adj },
};

DECLARE_PAPER_ADJUSTMENTS(ultrachrome_k3_photo);

static const paper_adjustment_t ultrachrome_k3_matte_adjustments[] =
{
  { "Plain", 0.72, .1, 1, 0, 1.25, 1, .95, .9, 1, 1, 1.0,
    ultra_k3_matte_hue_adj, ultra_k3_matte_lum_adj, ultra_k3_matte_sat_adj },
  { "PlainFast", 0.72, .1, 1, 0, 1.25, 1, .95, .9, 1, 1, 1.0,
    ultra_k3_matte_hue_adj, ultra_k3_matte_lum_adj, ultra_k3_matte_sat_adj },
  { "Postcard", 0.72, .1, 1, 0, 1.25, 1, .95, .9, 1, 1, 1.0,
    ultra_k3_matte_hue_adj, ultra_k3_matte_lum_adj, ultra_k3_matte_sat_adj },
  { "GlossyFilm", 0.83, .5, 1, 0.00, 1.25, 1, .95, .9, 1, 1, 1.0,
    ultra_k3_matte_hue_adj, ultra_k3_matte_lum_adj, ultra_k3_matte_sat_adj },
  { "Transparency", 0.83, .5, 1, 0.00, 1.25, 1, .95, .9, 1, 1, 1.0,
    ultra_k3_matte_hue_adj, ultra_k3_matte_lum_adj, ultra_k3_matte_sat_adj },
  { "Envelope", 0.72, .1, 1, 0, 1.25, 1, .95, .9, 1, 1, 1.0,
    ultra_k3_matte_hue_adj, ultra_k3_matte_lum_adj, ultra_k3_matte_sat_adj },
  { "BackFilm", 0.83, .5, 1, 0.00, 1.25, 1, .95, .9, 1, 1, 1.0,
    ultra_k3_matte_hue_adj, ultra_k3_matte_lum_adj, ultra_k3_matte_sat_adj },
  { "Matte", 0.92, 0.5, 1, 0.00, 1.25, 1, .95, .9, 1, 1, 1.0,
    ultra_k3_matte_hue_adj, ultra_k3_matte_lum_adj, ultra_k3_matte_sat_adj },
  { "MatteHeavy", 0.92, 0.5, 1, .00, 1.25, 1, .95, .9, 1, 1, 1.0,
    ultra_k3_matte_hue_adj, ultra_k3_matte_lum_adj, ultra_k3_matte_sat_adj },
  { "Inkjet", 0.72, .3, 1, .00, 1.25, 1, .95, .9, 1, 1, 1.0,
    ultra_k3_matte_hue_adj, ultra_k3_matte_lum_adj, ultra_k3_matte_sat_adj },
  { "Coated", 0.83, .4, 1, .00, 1.25, 1, .95, .9, 1, 1, 1.0,
    ultra_k3_matte_hue_adj, ultra_k3_matte_lum_adj, ultra_k3_matte_sat_adj },
  { "Photo", 1.0, 0.5, 1, 0.00, 1.25, 1, .95, .9, 1, 1, 1.0,
    ultra_k3_matte_hue_adj, ultra_k3_matte_lum_adj, ultra_k3_matte_sat_adj },
  { "GlossyPhoto", 0.72, 1, 1, .00, 1.25, 1, .95, .9, 1, 1, .92,
    ultra_k3_glossy_hue_adj, ultra_k3_glossy_lum_adj, ultra_k3_glossy_sat_adj },
  { "Semigloss", 0.72, .8, 1, .00, 1.25, 1, .95, .9, 1, 1, .92,
    ultra_k3_glossy_hue_adj, ultra_k3_glossy_lum_adj, ultra_k3_glossy_sat_adj },
  { "Luster", 0.72, .8, 1, .00, 1.25, 1, .95, .9, 1, 1, .92,
    ultra_k3_glossy_hue_adj, ultra_k3_glossy_lum_adj, ultra_k3_glossy_sat_adj },
  { "WaterColorRadiant", 0.92, 0.4, 1, .00, 1.25, 1, .95, .9, 1, 1, 1.0,
    ultra_k3_matte_hue_adj, ultra_k3_matte_lum_adj, ultra_k3_matte_sat_adj },
  { "GlossyPaper", 0.83, 0.5, 1, 0.00, 1.25, 1, .95, .9, 1, 1, 1.0,
    ultra_k3_matte_hue_adj, ultra_k3_matte_lum_adj, ultra_k3_matte_sat_adj },
  { "Ilford", 0.83, 0.5, 1, 0.00, 1.25, 1, .95, .9, 1, 1, 1.0,
    ultra_k3_matte_hue_adj, ultra_k3_matte_lum_adj, ultra_k3_matte_sat_adj  },
  { "ColorLife", 0.83, 0.5, 1, 0.00, 1.25, 1, .95, .9, 1, 1, 1.0,
    ultra_k3_matte_hue_adj, ultra_k3_matte_lum_adj, ultra_k3_matte_sat_adj },
  { "Other", 0.72, .1, .4, 0, 1.25, 1, .95, .9, 1, 1, 1.0,
    ultra_k3_matte_hue_adj, ultra_k3_matte_lum_adj, ultra_k3_matte_sat_adj },
};

DECLARE_PAPER_ADJUSTMENTS(ultrachrome_k3_matte);

static const paper_adjustment_t r800_photo_adjustments[] =
{
  { "Plain", 0.72, .1, 1, .02, 1.4, .882, 1, .250, 1, 1, 1.0,
    r800_glossy_hue_adj, r800_glossy_lum_adj, r800_glossy_sat_adj },
  { "PlainFast", 0.72, .1, 1, .02, 1.4, .882, 1, .250, 1, 1, 1.0,
    r800_glossy_hue_adj, r800_glossy_lum_adj, r800_glossy_sat_adj },
  { "Postcard", 0.72, .1, 1, .02, 1.4, .882, 1, .250, 1, 1, 1.0,
    r800_glossy_hue_adj, r800_glossy_lum_adj, r800_glossy_sat_adj },
  { "GlossyFilm", 0.83, 1.0, 1, .02, 1.4, .882, 1, .250, 1, 1, 1.0,
    r800_glossy_hue_adj, r800_glossy_lum_adj, r800_glossy_sat_adj },
  { "Transparency", 0.83, .75, 1, .02, 1.4, .882, 1, .250, 1, 1, 1.0,
    r800_glossy_hue_adj, r800_glossy_lum_adj, r800_glossy_sat_adj },
  { "Envelope", 0.72, .1, 1, .02, 1.4, .882, 1, .250, 1, 1, 1.0,
    r800_glossy_hue_adj, r800_glossy_lum_adj, r800_glossy_sat_adj },
  { "BackFilm", 0.83, .75, 1, .02, 1.4, .882, 1, .250, 1, 1, 1.0,
    r800_glossy_hue_adj, r800_glossy_lum_adj, r800_glossy_sat_adj },
  { "Matte", 0.92, .4, 1, .02, 1.4, .882, 1, .250, 1, 1, 1.0,
    r800_glossy_hue_adj, r800_glossy_lum_adj, r800_glossy_sat_adj },
  { "MatteHeavy", 0.92, .4, 1, .02, 1.4, .882, 1, .250, 1, 1, 1.0,
    r800_glossy_hue_adj, r800_glossy_lum_adj, r800_glossy_sat_adj },
  { "Glossy", 0.92, 0.4, 1, .02, 1.4, .882, 1, .250, 1, 1, 1.0,
    r800_glossy_hue_adj, r800_glossy_lum_adj, r800_glossy_sat_adj },
  { "GlossyHeavy", 0.92, 0.4, 1, .02, 1.4, .882, 1, .250, 1, 1, 1.0,
    r800_glossy_hue_adj, r800_glossy_lum_adj, r800_glossy_sat_adj },
  { "Inkjet", 0.72, .5, 1, .02, 1.4, .882, 1, .250, 1, 1, 1.0,
    r800_glossy_hue_adj, r800_glossy_lum_adj, r800_glossy_sat_adj },
  { "Coated", 0.83, .5, 1, .02, 1.4, .882, 1, .250, 1, 1, 1.0,
    r800_glossy_hue_adj, r800_glossy_lum_adj, r800_glossy_sat_adj },
  { "Photo", 1.0, .75, 1, .02, 1.4, .882, 1, .250, 1, 1, 1.0,
    r800_glossy_hue_adj, r800_glossy_lum_adj, r800_glossy_sat_adj },
  { "GlossyPhoto", 0.600, 1, 1, .02, 2.0, .882, 1, .250, 1, 1, 0.92,
    r800_glossy_hue_adj, r800_glossy_lum_adj, r800_glossy_sat_adj },
  { "Semigloss", 0.600, .8, 1, .02, 2.0, .882, 1, .250, 1, 1, 0.92,
    r800_glossy_hue_adj, r800_glossy_lum_adj, r800_glossy_sat_adj },
  { "Luster", 0.600, .8, 1, .02, 2.0, .882, 1, .250, 1, 1, 0.92,
    r800_glossy_hue_adj, r800_glossy_lum_adj, r800_glossy_sat_adj },
  { "ArchivalGlossy", 0.92, .4, 1, .02, 1.4, .882, 1, .250, 1, 1, 1.0,
    r800_glossy_hue_adj, r800_glossy_lum_adj, r800_glossy_sat_adj },
  { "WaterColorRadiant", 0.92, .4, 1, .02, 1.4, .882, 1, .250, 1, 1, 1.0,
    r800_glossy_hue_adj, r800_glossy_lum_adj, r800_glossy_sat_adj },
  { "ArchivalMatte", 0.92, .4, 1, .02, 1.4, .882, 1, .250, 1, 1, 1.0,
    r800_glossy_hue_adj, r800_glossy_lum_adj, r800_glossy_sat_adj },
  { "GlossyPaper", 0.83, 1.0, 1, .02, 1.4, .882, 1, .250, 1, 1, 1.0,
    r800_glossy_hue_adj, r800_glossy_lum_adj, r800_glossy_sat_adj },
  { "Ilford", 0.83, 1.0, 1, .02, 1.4, .882, 1, .250, 1, 1, 1.0,
    r800_glossy_hue_adj, r800_glossy_lum_adj, r800_glossy_sat_adj  },
  { "ColorLife", 0.83, 1.0, 1, .02, 1.4, .882, 1, .250, 1, 1, 1.0,
    r800_glossy_hue_adj, r800_glossy_lum_adj, r800_glossy_sat_adj },
  { "Other", 0.72, .1, 1, .02, 1.4, .882, 1, .250, 1, 1, 1.0,
    r800_glossy_hue_adj, r800_glossy_lum_adj, r800_glossy_sat_adj },
};

DECLARE_PAPER_ADJUSTMENTS(r800_photo);

static const paper_adjustment_t r800_matte_adjustments[] =
{
  { "Plain", 0.72, .1, .5, .025, .999, .882, 1, .250, 1, 1, 1.0,
    r800_matte_hue_adj, r800_matte_lum_adj, r800_matte_sat_adj },
  { "PlainFast", 0.72, .1, .5, .025, .999, .882, 1, .250, 1, 1, 1.0,
    r800_matte_hue_adj, r800_matte_lum_adj, r800_matte_sat_adj },
  { "Postcard", 0.72, .1, .5, .025, .999, .882, 1, .250, 1, 1, 1.0,
    r800_matte_hue_adj, r800_matte_lum_adj, r800_matte_sat_adj },
  { "GlossyFilm", 0.83, .5, .5, .025, .999, .882, 1, .250, 1, 1, 1.0,
    r800_matte_hue_adj, r800_matte_lum_adj, r800_matte_sat_adj },
  { "Transparency", 0.83, .5, .5, .025, .999, .882, 1, .250, 1, 1, 1.0,
    r800_matte_hue_adj, r800_matte_lum_adj, r800_matte_sat_adj },
  { "Envelope", 0.72, .1, .5, .025, .999, .882, 1, .250, 1, 1, 1.0,
    r800_matte_hue_adj, r800_matte_lum_adj, r800_matte_sat_adj },
  { "BackFilm", 0.83, .5, .5, .025, .999, .882, 1, .250, 1, 1, 1.0,
    r800_matte_hue_adj, r800_matte_lum_adj, r800_matte_sat_adj },
  { "Matte", 0.92, 0.4, .5, .025, .999, .882, 1, .250, 1, 1, 1.0,
    r800_matte_hue_adj, r800_matte_lum_adj, r800_matte_sat_adj },
  { "MatteHeavy", 0.92, 0.4, .5, .025, .999, .882, 1, .250, 1, 1, 1.0,
    r800_matte_hue_adj, r800_matte_lum_adj, r800_matte_sat_adj },
  { "Inkjet", 0.72, .3, .5, .025, .999, .882, 1, .250, 1, 1, 1.0,
    r800_matte_hue_adj, r800_matte_lum_adj, r800_matte_sat_adj },
  { "Coated", 0.83, .4, .5, .025, .999, .882, 1, .250, 1, 1, 1.0,
    r800_matte_hue_adj, r800_matte_lum_adj, r800_matte_sat_adj },
  { "Photo", 1.0, 0.5, .5, .025, .999, .882, 1, .250, 1, 1, 1.0,
    r800_matte_hue_adj, r800_matte_lum_adj, r800_matte_sat_adj },
  { "GlossyPhoto", 0.546, 1, .5, .025, .999, .882, 1, .250, 1, 1, 0.92,
    r800_glossy_hue_adj, r800_glossy_lum_adj, r800_glossy_sat_adj },
  { "Semigloss", 0.546, .8, .5, .025, .999, .882, 1, .250, 1, 1, 0.92,
    r800_glossy_hue_adj, r800_glossy_lum_adj, r800_glossy_sat_adj },
  { "Luster", 0.546, .8, .5, .025, .999, .882, 1, .250, 1, 1, 0.92,
    r800_glossy_hue_adj, r800_glossy_lum_adj, r800_glossy_sat_adj },
  { "ArchivalMatte", 0.92, 0.4, .5, .025, .999, .882, 1, .250, 1, 1, 1.0,
    r800_matte_hue_adj, r800_matte_lum_adj, r800_matte_sat_adj },
  { "WaterColorRadiant", 0.92, 0.4, .5, .025, .999, .882, 1, .250, 1, 1, 1.0,
    r800_matte_hue_adj, r800_matte_lum_adj, r800_matte_sat_adj },
  { "GlossyPaper", 0.83, 0.5, .5, .025, .999, .882, 1, .250, 1, 1, 1.0,
    r800_matte_hue_adj, r800_matte_lum_adj, r800_matte_sat_adj },
  { "Ilford", 0.83, 0.5, .5, .025, .999, .882, 1, .250, 1, 1, 1.0,
    r800_matte_hue_adj, r800_matte_lum_adj, r800_matte_sat_adj  },
  { "ColorLife", 0.83, 0.5, .5, .025, .999, .882, 1, .250, 1, 1, 1.0,
    r800_matte_hue_adj, r800_matte_lum_adj, r800_matte_sat_adj },
  { "Other", 0.72, .1, .5, .025, .999, .882, 1, .250, 1, 1, 1.0,
    r800_matte_hue_adj, r800_matte_lum_adj, r800_matte_sat_adj },
};

DECLARE_PAPER_ADJUSTMENTS(r800_matte);

static const paper_adjustment_t picturemate_adjustments[] =
{
  { "GlossyPhoto", 1.00, 1, 1, .02, 2.0, .882, 1, .250, 1, 1, 0.92,
    r800_glossy_hue_adj, r800_glossy_lum_adj, r800_glossy_sat_adj },
  { "Other", 0.878, .1, 1, .02, 2.0, .882, 1, .250, 1, 1, 1.0,
    r800_glossy_hue_adj, r800_glossy_lum_adj, r800_glossy_sat_adj },
};

DECLARE_PAPER_ADJUSTMENTS(picturemate);

static const paper_adjustment_t durabrite_adjustments[] =
{
  { "Plain", 1.0, .5, .5, .05, .9, 1, 1, 1, 1, 1, 1.0,
    standard_hue_adj, standard_lum_adj, standard_sat_adj },
  { "PlainFast", 1.0, .5, .5, .05, .9, 1, 1, 1, 1, 1, 1.0,
    standard_hue_adj, standard_lum_adj, standard_sat_adj },
  { "Postcard", 1.0, .5, 1, .05, .9, 1, 1, 1, 1, 1, 1.0,
    standard_hue_adj, standard_lum_adj, standard_sat_adj },
  { "GlossyFilm", 0.8, 1.0, 1, .05, .999, 1, 1, 1, 1, 1, 1,
    standard_hue_adj, standard_lum_adj, standard_sat_adj },
  { "Transparency", 0.8, .75, 1, .05, .999, 1, 1, 1, 1, 1, 1,
    standard_hue_adj, standard_lum_adj, standard_sat_adj },
  { "Envelope", 1.0, .5, 1, .05, .9, 1, 1, 1, 1, 1, 1.0,
    standard_hue_adj, standard_lum_adj, standard_sat_adj },
  { "BackFilm", 0.8, .75, 1, .05, .999, 1, 1, 1, 1, 1, 1,
    standard_hue_adj, standard_lum_adj, standard_sat_adj },
  { "Matte", 0.9, .5, .5, .075, .999, 1, .975, .975, 1, 1, 1.1,
    standard_hue_adj, standard_lum_adj, standard_sat_adj },
  { "MatteHeavy", 0.9, .5, .5, .075, .999, 1, .975, .975, 1, 1, 1,
    standard_hue_adj, standard_lum_adj, standard_sat_adj },
  { "Inkjet", 1.0, .5, .5, .05, .9, 1, 1, 1, 1, 1, 1.0,
    standard_hue_adj, standard_lum_adj, standard_sat_adj },
  { "Coated", 1.0, .5, .5, .075, .999, 1, 1, 1, 1, 1, 1.1,
    standard_hue_adj, standard_lum_adj, standard_sat_adj },
  { "Photo", .833, .5, .5, .075, .999, 1, 1, 1, 1, 1, 1,
    standard_hue_adj, standard_lum_adj, standard_sat_adj },
  { "GlossyPhoto", .833, 1.0, 1, .15, .999, 1, 1, 1, 1, 1, .92,
    standard_hue_adj, standard_lum_adj, standard_sat_adj },
  { "Semigloss", .833, 1.0, 1, .15, .999, 1, 1, 1, 1, 1, .92,
    standard_hue_adj, standard_lum_adj, standard_sat_adj },
  { "Luster", .833, 1.0, 1, .15, .999, 1, 1, 1, 1, 1, .92,
    standard_hue_adj, standard_lum_adj, standard_sat_adj },
  { "GlossyPaper", .833, 1.0, 1, .15, .999, 1, 1, 1, 1, 1, 1.0,
    standard_hue_adj, standard_lum_adj, standard_sat_adj },
  { "Ilford", .833, 1.0, 1, .15, 1.35, 1, 1, 1, 1, 1, 1,
    standard_hue_adj, standard_lum_adj, standard_sat_adj  },
  { "ColorLife", .833, 1.0, 1, .15, .9, 1, 1, 1, 1, 1, 1,
    standard_hue_adj, standard_lum_adj, standard_sat_adj },
  { "Other", 1.0, .5, 1, .05, .9, 1, 1, 1, 1, 1, 1,
    standard_hue_adj, standard_lum_adj, standard_sat_adj },
};

DECLARE_PAPER_ADJUSTMENTS(durabrite);

static const paper_adjustment_t durabrite2_adjustments[] =
{
  { "Plain", 1.0, .5, .5, .05, .9, 1, 1, 1, 1, 1, 1.0,
    standard_hue_adj, standard_lum_adj, standard_sat_adj },
  { "PlainFast", 1.0, .5, .5, .05, .9, 1, 1, 1, 1, 1, 1.0,
    standard_hue_adj, standard_lum_adj, standard_sat_adj },
  { "Postcard", 1.0, .5, 1, .05, .9, 1, 1, 1, 1, 1, 1.0,
    standard_hue_adj, standard_lum_adj, standard_sat_adj },
  { "GlossyFilm", 0.8, 1.0, 1, .05, .999, 1, 1, 1, 1, 1, 1,
    standard_hue_adj, standard_lum_adj, standard_sat_adj },
  { "Transparency", 0.8, .75, 1, .05, .999, 1, 1, 1, 1, 1, 1,
    standard_hue_adj, standard_lum_adj, standard_sat_adj },
  { "Envelope", 1.0, .5, 1, .05, .9, 1, 1, 1, 1, 1, 1.0,
    standard_hue_adj, standard_lum_adj, standard_sat_adj },
  { "BackFilm", 0.8, .75, 1, .05, .999, 1, 1, 1, 1, 1, 1,
    standard_hue_adj, standard_lum_adj, standard_sat_adj },
  { "Matte", 0.9, .5, .5, .075, .999, 1, .975, .975, 1, 1, 1.0,
    standard_hue_adj, standard_lum_adj, standard_sat_adj },
  { "MatteHeavy", 0.9, .5, .5, .075, .999, 1, .975, .975, 1, 1, 1.0,
    standard_hue_adj, standard_lum_adj, standard_sat_adj },
  { "Inkjet", 1.0, .5, .5, .05, .9, 1, 1, 1, 1, 1, 1.0,
    standard_hue_adj, standard_lum_adj, standard_sat_adj },
  { "Coated", 1.0, .5, .5, .075, .999, 1, 1, 1, 1, 1, 1.0,
    standard_hue_adj, standard_lum_adj, standard_sat_adj },
  { "Photo", .833, .5, .5, .075, .999, 1, 1, 1, 1, 1, 1,
    standard_hue_adj, standard_lum_adj, standard_sat_adj },
  { "GlossyPhoto", .833, 1.0, 1, .15, .999, 1, 1, 1, 1, 1, .92,
    standard_hue_adj, standard_lum_adj, standard_sat_adj },
  { "Semigloss", .833, 1.0, 1, .15, .999, 1, 1, 1, 1, 1, .92,
    standard_hue_adj, standard_lum_adj, standard_sat_adj },
  { "Luster", .833, 1.0, 1, .15, .999, 1, 1, 1, 1, 1, .92,
    standard_hue_adj, standard_lum_adj, standard_sat_adj },
  { "GlossyPaper", .833, 1.0, 1, .15, .999, 1, 1, 1, 1, 1, 1.0,
    standard_hue_adj, standard_lum_adj, standard_sat_adj },
  { "Ilford", .833, 1.0, 1, .15, 1.35, 1, 1, 1, 1, 1, 1,
    standard_hue_adj, standard_lum_adj, standard_sat_adj  },
  { "ColorLife", .833, 1.0, 1, .15, .9, 1, 1, 1, 1, 1, 1,
    standard_hue_adj, standard_lum_adj, standard_sat_adj },
  { "Other", 1.0, .5, 1, .05, .9, 1, 1, 1, 1, 1, 1,
    standard_hue_adj, standard_lum_adj, standard_sat_adj },
};

DECLARE_PAPER_ADJUSTMENTS(durabrite2);

static const paper_t standard_papers[] =
{
  { "Plain", N_("Plain Paper"), PAPER_PLAIN,
    1, 0, 0x6b, 0x1a, 0x01, NULL, NULL },
  { "PlainFast", N_("Plain Paper Fast Load"), PAPER_PLAIN,
    5, 0, 0x6b, 0x1a, 0x01, NULL, NULL },
  { "Postcard", N_("Postcard"), PAPER_PLAIN,
    2, 0, 0x00, 0x00, 0x02, NULL, NULL },
  { "GlossyFilm", N_("Glossy Film"), PAPER_PHOTO,
    3, 0, 0x6d, 0x00, 0x01, NULL, NULL },
  { "Transparency", N_("Transparencies"), PAPER_TRANSPARENCY,
    3, 0, 0x6d, 0x00, 0x02, NULL, NULL },
  { "Envelope", N_("Envelopes"), PAPER_PLAIN,
    4, 0, 0x6b, 0x1a, 0x01, NULL, NULL },
  { "BackFilm", N_("Back Light Film"), PAPER_TRANSPARENCY,
    6, 0, 0x6d, 0x00, 0x01, NULL, NULL },
  { "Matte", N_("Matte Paper"), PAPER_GOOD,
    7, 0, 0x00, 0x00, 0x02, NULL, NULL },
  { "MatteHeavy", N_("Matte Paper Heavyweight"), PAPER_GOOD,
    7, 0, 0x00, 0x00, 0x02, NULL, NULL },
  { "Inkjet", N_("Inkjet Paper"), PAPER_GOOD,
    7, 0, 0x6b, 0x1a, 0x01, NULL, NULL },
  { "Coated", N_("Photo Quality Inkjet Paper"), PAPER_GOOD,
    7, 0, 0x6b, 0x1a, 0x01, NULL, NULL },
  { "Photo", N_("Photo Paper"), PAPER_PHOTO,
    8, 0, 0x67, 0x00, 0x02, NULL, NULL },
  { "GlossyPhoto", N_("Premium Glossy Photo Paper"), PAPER_PREMIUM_PHOTO,
    8, 0, 0x80, 0x00, 0x02, NULL, NULL },
  { "Semigloss", N_("Premium Semigloss Photo Paper"), PAPER_PREMIUM_PHOTO,
    8, 0, 0x80, 0x00, 0x02, NULL, NULL },
  { "Luster", N_("Premium Luster Photo Paper"), PAPER_PREMIUM_PHOTO,
    8, 0, 0x80, 0x00, 0x02, NULL, NULL },
  { "GlossyPaper", N_("Photo Quality Glossy Paper"), PAPER_PREMIUM_PHOTO,
    6, 0, 0x6b, 0x1a, 0x01, NULL, NULL },
  { "Ilford", N_("Ilford Heavy Paper"), PAPER_PREMIUM_PHOTO,
    8, 0, 0x80, 0x00, 0x02, NULL, NULL },
  { "ColorLife", N_("ColorLife Paper"), PAPER_PREMIUM_PHOTO,
    8, 0, 0x67, 0x00, 0x02, NULL, NULL },
  { "Other", N_("Other"), PAPER_PLAIN,
    0, 0, 0x6b, 0x1a, 0x01, NULL, NULL },
};

DECLARE_PAPERS(standard);

static const paper_t durabrite_papers[] =
{
  { "Plain", N_("Plain Paper"), PAPER_PLAIN,
    1, 0, 0x6b, 0x1a, 0x01, NULL, NULL },
  { "PlainFast", N_("Plain Paper Fast Load"), PAPER_PLAIN,
    5, 0, 0x6b, 0x1a, 0x01, NULL, NULL },
  { "Postcard", N_("Postcard"), PAPER_PLAIN,
    2, 0, 0x00, 0x00, 0x02, NULL, NULL },
  { "GlossyFilm", N_("Glossy Film"), PAPER_PHOTO,
    3, 0, 0x6d, 0x00, 0x01, NULL, NULL },
  { "Transparency", N_("Transparencies"), PAPER_TRANSPARENCY,
    3, 0, 0x6d, 0x00, 0x02, NULL, NULL },
  { "Envelope", N_("Envelopes"), PAPER_PLAIN,
    4, 0, 0x6b, 0x1a, 0x01, NULL, NULL },
  { "BackFilm", N_("Back Light Film"), PAPER_TRANSPARENCY,
    6, 0, 0x6d, 0x00, 0x01, NULL, NULL },
  { "Matte", N_("Matte Paper"), PAPER_GOOD,
    7, 0, 0x00, 0x00, 0x02, NULL, NULL },
  { "MatteHeavy", N_("Matte Paper Heavyweight"), PAPER_GOOD,
    7, 0, 0x00, 0x00, 0x02, NULL, NULL },
  { "Inkjet", N_("Inkjet Paper"), PAPER_GOOD,
    7, 0, 0x6b, 0x1a, 0x01, NULL, NULL },
  { "Coated", N_("Photo Quality Inkjet Paper"), PAPER_GOOD,
    7, 0, 0x6b, 0x1a, 0x01, NULL, NULL },
  { "Photo", N_("Photo Paper"), PAPER_PHOTO,
    8, 0, 0x67, 0x00, 0x02, "RGB", NULL },
  { "GlossyPhoto", N_("Premium Glossy Photo Paper"), PAPER_PREMIUM_PHOTO,
    8, 0, 0x80, 0x00, 0x02, "RGB", NULL },
  { "Semigloss", N_("Premium Semigloss Photo Paper"), PAPER_PREMIUM_PHOTO,
    8, 0, 0x80, 0x00, 0x02, "RGB", NULL },
  { "Luster", N_("Premium Luster Photo Paper"), PAPER_PREMIUM_PHOTO,
    8, 0, 0x80, 0x00, 0x02, "RGB", NULL },
  { "GlossyPaper", N_("Photo Quality Glossy Paper"), PAPER_PHOTO,
    6, 0, 0x6b, 0x1a, 0x01, "RGB", NULL },
  { "Ilford", N_("Ilford Heavy Paper"), PAPER_PREMIUM_PHOTO,
    8, 0, 0x80, 0x00, 0x02, NULL, NULL },
  { "ColorLife", N_("ColorLife Paper"), PAPER_PREMIUM_PHOTO,
    8, 0, 0x67, 0x00, 0x02, NULL, NULL },
  { "Other", N_("Other"), PAPER_PLAIN,
    0, 0, 0x6b, 0x1a, 0x01, NULL, NULL },
};

DECLARE_PAPERS(durabrite);

static const paper_t ultrachrome_papers[] =
{
  { "Plain", N_("Plain Paper"), PAPER_PLAIN,
    1, 0, 0x6b, 0x1a, 0x01, NULL, "UltraMatte" },
  { "PlainFast", N_("Plain Paper Fast Load"), PAPER_PLAIN,
    5, 0, 0x6b, 0x1a, 0x01, NULL, "UltraMatte" },
  { "Postcard", N_("Postcard"), PAPER_PLAIN,
    2, 0, 0x00, 0x00, 0x02, NULL, "UltraMatte" },
  { "GlossyFilm", N_("Glossy Film"), PAPER_PHOTO,
    3, 0, 0x6d, 0x00, 0x01, NULL, "UltraPhoto" },
  { "Transparency", N_("Transparencies"), PAPER_TRANSPARENCY,
    3, 0, 0x6d, 0x00, 0x02, NULL, "UltraPhoto" },
  { "Envelope", N_("Envelopes"), PAPER_PLAIN,
    4, 0, 0x6b, 0x1a, 0x01, NULL, "UltraMatte" },
  { "BackFilm", N_("Back Light Film"), PAPER_TRANSPARENCY,
    6, 0, 0x6d, 0x00, 0x01, NULL, "UltraPhoto" },
  { "Matte", N_("Matte Paper"), PAPER_GOOD,
    7, 0, 0x00, 0x00, 0x02, NULL, "UltraMatte" },
  { "MatteHeavy", N_("Matte Paper Heavyweight"), PAPER_GOOD,
    7, 0, 0x00, 0x00, 0x02, NULL, "UltraMatte" },
  { "Inkjet", N_("Inkjet Paper"), PAPER_GOOD,
    7, 0, 0x6b, 0x1a, 0x01, NULL, "UltraMatte" },
  { "Coated", N_("Photo Quality Inkjet Paper"), PAPER_GOOD,
    7, 0, 0x6b, 0x1a, 0x01, NULL, "UltraPhoto" },
  { "Photo", N_("Photo Paper"), PAPER_PHOTO,
    8, 0, 0x67, 0x00, 0x02, NULL, "UltraPhoto" },
  { "GlossyPhoto", N_("Premium Glossy Photo Paper"), PAPER_PREMIUM_PHOTO,
    8, 0, 0x80, 0x00, 0x02, NULL, "UltraPhoto" },
  { "Semigloss", N_("Premium Semigloss Photo Paper"), PAPER_PREMIUM_PHOTO,
    8, 0, 0x80, 0x00, 0x02, NULL, "UltraPhoto" },
  { "Luster", N_("Premium Luster Photo Paper"), PAPER_PREMIUM_PHOTO,
    8, 0, 0x80, 0x00, 0x02, NULL, "UltraPhoto" },
  { "ArchivalMatte", N_("Archival Matte Paper"), PAPER_PREMIUM_PHOTO,
    7, 0, 0x00, 0x00, 0x02, NULL, "UltraMatte" },
  { "WaterColorRadiant", N_("Watercolor Paper - Radiant White"), PAPER_PREMIUM_PHOTO,
    7, 0, 0x00, 0x00, 0x02, NULL, "UltraMatte" },
  { "GlossyPaper", N_("Photo Quality Glossy Paper"), PAPER_PHOTO,
    6, 0, 0x6b, 0x1a, 0x01, NULL, "UltraPhoto" },
  { "Ilford", N_("Ilford Heavy Paper"), PAPER_PREMIUM_PHOTO,
    8, 0, 0x80, 0x00, 0x02, NULL, "UltraMatte" },
  { "ColorLife", N_("ColorLife Paper"), PAPER_PREMIUM_PHOTO,
    8, 0, 0x67, 0x00, 0x02, NULL, "UltraPhoto" },
  { "Other", N_("Other"), PAPER_PLAIN,
    0, 0, 0x6b, 0x1a, 0x01, NULL, "UltraMatte" },
};

DECLARE_PAPERS(ultrachrome);

static const paper_t ultrachrome_k3_papers[] =
{
  { "Plain", N_("Plain Paper"), PAPER_PLAIN,
    1, 0, 0x6b, 0x1a, 0x01, NULL, "UltraMatte" },
  { "PlainFast", N_("Plain Paper Fast Load"), PAPER_PLAIN,
    5, 0, 0x6b, 0x1a, 0x01, NULL, "UltraMatte" },
  { "Postcard", N_("Postcard"), PAPER_PLAIN,
    2, 0, 0x00, 0x00, 0x02, NULL, "UltraMatte" },
  { "GlossyFilm", N_("Glossy Film"), PAPER_PHOTO,
    3, 0, 0x6d, 0x00, 0x01, NULL, "UltraPhoto" },
  { "Transparency", N_("Transparencies"), PAPER_TRANSPARENCY,
    3, 0, 0x6d, 0x00, 0x02, NULL, "UltraPhoto" },
  { "Envelope", N_("Envelopes"), PAPER_PLAIN,
    4, 0, 0x6b, 0x1a, 0x01, NULL, "UltraMatte" },
  { "BackFilm", N_("Back Light Film"), PAPER_TRANSPARENCY,
    6, 0, 0x6d, 0x00, 0x01, NULL, "UltraPhoto" },
  { "Matte", N_("Matte Paper"), PAPER_GOOD,
    7, 0, 0x00, 0x00, 0x02, NULL, "UltraMatte" },
  { "MatteHeavy", N_("Matte Paper Heavyweight"), PAPER_GOOD,
    7, 0, 0x00, 0x00, 0x02, NULL, "UltraMatte" },
  { "Inkjet", N_("Inkjet Paper"), PAPER_GOOD,
    7, 0, 0x6b, 0x1a, 0x01, NULL, "UltraMatte" },
  { "Coated", N_("Photo Quality Inkjet Paper"), PAPER_GOOD,
    7, 0, 0x6b, 0x1a, 0x01, NULL, "UltraPhoto" },
  { "Photo", N_("Photo Paper"), PAPER_PHOTO,
    8, 0, 0x67, 0x00, 0x02, NULL, "UltraPhoto" },
  { "GlossyPhoto", N_("Premium Glossy Photo Paper"), PAPER_PREMIUM_PHOTO,
    8, 0, 0x80, 0x00, 0x02, NULL, "UltraPhoto" },
  { "Semigloss", N_("Premium Semigloss Photo Paper"), PAPER_PREMIUM_PHOTO,
    8, 0, 0x80, 0x00, 0x02, NULL, "UltraPhoto" },
  { "Luster", N_("Premium Luster Photo Paper"), PAPER_PREMIUM_PHOTO,
    8, 0, 0x80, 0x00, 0x02, NULL, "UltraPhoto" },
  { "ArchivalMatte", N_("Archival Matte Paper"), PAPER_PREMIUM_PHOTO,
    7, 0, 0x00, 0x00, 0x02, NULL, "UltraMatte" },
  { "WaterColorRadiant", N_("Watercolor Paper - Radiant White"), PAPER_PREMIUM_PHOTO,
    7, 0, 0x00, 0x00, 0x02, NULL, "UltraMatte" },
  { "GlossyPaper", N_("Photo Quality Glossy Paper"), PAPER_PHOTO,
    6, 0, 0x6b, 0x1a, 0x01, NULL, "UltraPhoto" },
  { "Ilford", N_("Ilford Heavy Paper"), PAPER_PREMIUM_PHOTO,
    8, 0, 0x80, 0x00, 0x02, NULL, "UltraMatte" },
  { "ColorLife", N_("ColorLife Paper"), PAPER_PREMIUM_PHOTO,
    8, 0, 0x67, 0x00, 0x02, NULL, "UltraPhoto" },
  { "Other", N_("Other"), PAPER_PLAIN,
    0, 0, 0x6b, 0x1a, 0x01, NULL, "UltraMatte" },
};

DECLARE_PAPERS(ultrachrome_k3);

static const paper_t durabrite2_papers[] =
{
  { "Plain", N_("Plain Paper"), PAPER_PLAIN,
    1, 0, 0x6b, 0x1a, 0x01, NULL, NULL },
  { "PlainFast", N_("Plain Paper Fast Load"), PAPER_PLAIN,
    5, 0, 0x6b, 0x1a, 0x01, NULL, NULL },
  { "Postcard", N_("Postcard"), PAPER_PLAIN,
    2, 0, 0x00, 0x00, 0x02, NULL, NULL },
  { "GlossyFilm", N_("Glossy Film"), PAPER_PHOTO,
    3, 0, 0x6d, 0x00, 0x01, NULL, NULL },
  { "Transparency", N_("Transparencies"), PAPER_TRANSPARENCY,
    3, 0, 0x6d, 0x00, 0x02, NULL, NULL },
  { "Envelope", N_("Envelopes"), PAPER_PLAIN,
    4, 0, 0x6b, 0x1a, 0x01, NULL, NULL },
  { "BackFilm", N_("Back Light Film"), PAPER_TRANSPARENCY,
    6, 0, 0x6d, 0x00, 0x01, NULL, NULL },
  { "Matte", N_("Matte Paper"), PAPER_GOOD,
    7, 0, 0x00, 0x00, 0x02, NULL, NULL },
  { "MatteHeavy", N_("Matte Paper Heavyweight"), PAPER_GOOD,
    7, 0, 0x00, 0x00, 0x02, NULL, NULL },
  { "Inkjet", N_("Inkjet Paper"), PAPER_GOOD,
    7, 0, 0x6b, 0x1a, 0x01, NULL, NULL },
  { "Coated", N_("Photo Quality Inkjet Paper"), PAPER_GOOD,
    7, 0, 0x6b, 0x1a, 0x01, NULL, NULL },
  { "Photo", N_("Photo Paper"), PAPER_PHOTO,
    8, 0, 0x67, 0x00, 0x02, NULL, NULL },
  { "GlossyPhoto", N_("Premium Glossy Photo Paper"), PAPER_PREMIUM_PHOTO,
    8, 0, 0x80, 0x00, 0x02, NULL, NULL },
  { "Semigloss", N_("Premium Semigloss Photo Paper"), PAPER_PREMIUM_PHOTO,
    8, 0, 0x80, 0x00, 0x02, NULL, NULL },
  { "Luster", N_("Premium Luster Photo Paper"), PAPER_PREMIUM_PHOTO,
    8, 0, 0x80, 0x00, 0x02, NULL, NULL },
  { "GlossyPaper", N_("Photo Quality Glossy Paper"), PAPER_PHOTO,
    6, 0, 0x6b, 0x1a, 0x01, NULL, NULL },
  { "Ilford", N_("Ilford Heavy Paper"), PAPER_PREMIUM_PHOTO,
    8, 0, 0x80, 0x00, 0x02, NULL, NULL },
  { "ColorLife", N_("ColorLife Paper"), PAPER_PREMIUM_PHOTO,
    8, 0, 0x67, 0x00, 0x02, NULL, NULL },
  { "Other", N_("Other"), PAPER_PLAIN,
    0, 0, 0x6b, 0x1a, 0x01, NULL, NULL },
};

DECLARE_PAPERS(durabrite2);

static const paper_t r800_papers[] =
{
  { "Plain", N_("Plain Paper"), PAPER_PLAIN,
    1, 0, 0x6b, 0x1a, 0x01, NULL, "r800Matte" },
  { "PlainFast", N_("Plain Paper Fast Load"), PAPER_PLAIN,
    5, 0, 0x6b, 0x1a, 0x01, NULL, "r800Matte" },
  { "Postcard", N_("Postcard"), PAPER_PLAIN,
    2, 0, 0x00, 0x00, 0x02, NULL, "r800Matte" },
  { "GlossyFilm", N_("Glossy Film"), PAPER_PHOTO,
    3, 0, 0x6d, 0x00, 0x01, NULL, "r800Photo" },
  { "Transparency", N_("Transparencies"), PAPER_TRANSPARENCY,
    3, 0, 0x6d, 0x00, 0x02, NULL, "r800Photo" },
  { "Envelope", N_("Envelopes"), PAPER_PLAIN,
    4, 0, 0x6b, 0x1a, 0x01, NULL, "r800Matte" },
  { "BackFilm", N_("Back Light Film"), PAPER_TRANSPARENCY,
    6, 0, 0x6d, 0x00, 0x01, NULL, "r800Photo" },
  { "Matte", N_("Matte Paper"), PAPER_GOOD,
    7, 0, 0x00, 0x00, 0x02, NULL, "r800Matte" },
  { "MatteHeavy", N_("Matte Paper Heavyweight"), PAPER_GOOD,
    7, 0, 0x00, 0x00, 0x02, NULL, "r800Matte" },
  { "Inkjet", N_("Inkjet Paper"), PAPER_GOOD,
    7, 0, 0x6b, 0x1a, 0x01, NULL, "r800Matte" },
  { "Coated", N_("Photo Quality Inkjet Paper"), PAPER_GOOD,
    7, 0, 0x6b, 0x1a, 0x01, NULL, "r800Photo" },
  { "Photo", N_("Photo Paper"), PAPER_PHOTO,
    8, 0, 0x67, 0x00, 0x02, NULL, "r800Photo" },
  { "GlossyPhoto", N_("Premium Glossy Photo Paper"), PAPER_PREMIUM_PHOTO,
    8, 0, 0x80, 0x00, 0x02, NULL, "r800Photo" },
  { "Semigloss", N_("Premium Semigloss Photo Paper"), PAPER_PREMIUM_PHOTO,
    8, 0, 0x80, 0x00, 0x02, NULL, "r800Photo" },
  { "Luster", N_("Premium Luster Photo Paper"), PAPER_PREMIUM_PHOTO,
    8, 0, 0x80, 0x00, 0x02, NULL, "r800Photo" },
  { "ArchivalMatte", N_("Archival Matte Paper"), PAPER_PREMIUM_PHOTO,
    7, 0, 0x00, 0x00, 0x02, NULL, "r800Matte" },
  { "WaterColorRadiant", N_("Watercolor Paper - Radiant White"), PAPER_PREMIUM_PHOTO,
    7, 0, 0x00, 0x00, 0x02, NULL, "r800Matte" },
  { "GlossyPaper", N_("Photo Quality Glossy Paper"), PAPER_PHOTO,
    6, 0, 0x6b, 0x1a, 0x01, NULL, "r800Photo" },
  { "Ilford", N_("Ilford Heavy Paper"), PAPER_PREMIUM_PHOTO,
    8, 0, 0x80, 0x00, 0x02, NULL, "r800Matte" },
  { "ColorLife", N_("ColorLife Paper"), PAPER_PREMIUM_PHOTO,
    8, 0, 0x67, 0x00, 0x02, NULL, "r800Photo" },
  { "Other", N_("Other"), PAPER_PLAIN,
    0, 0, 0x6b, 0x1a, 0x01, NULL, "r800Matte" },
};

DECLARE_PAPERS(r800);

static const paper_t picturemate_papers[] =
{
  { "GlossyPhoto", N_("Premium Glossy Photo Paper"), PAPER_PREMIUM_PHOTO,
    8, 0, 0x80, 0x00, 0x02, NULL, "picturematePhoto" },
  { "Other", N_("Other"), PAPER_PLAIN,
    0, 0, 0x6b, 0x1a, 0x01, NULL, "picturemateMatte" },
};

DECLARE_PAPERS(picturemate);

typedef struct
{
  const char *name;
  const paperlist_t *paper_list;
} paperl_t;

static const paperl_t the_papers[] =
{
  { "standard", &standard_paper_list },
  { "durabrite", &durabrite_paper_list },
  { "durabrite2", &durabrite2_paper_list },
  { "ultrachrome", &ultrachrome_paper_list },
  { "ultrachrome_k3", &ultrachrome_k3_paper_list },
  { "r800", &r800_paper_list },
  { "picturemate", &picturemate_paper_list },
};

const paperlist_t *
stpi_escp2_get_paperlist_named(const char *n)
{
  int i;
  if (n)
    for (i = 0; i < sizeof(the_papers) / sizeof(paperl_t); i++)
      {
	if (strcmp(n, the_papers[i].name) == 0)
	  return the_papers[i].paper_list;
      }
  return NULL;
}

typedef struct
{
  const char *name;
  const paper_adjustment_list_t *paper_list;
} paperadj_t;

static const paperadj_t the_adjustments[] =
{
  { "standard", &standard_paper_adjustment_list },
  { "durabrite", &durabrite_paper_adjustment_list },
  { "durabrite2", &durabrite2_paper_adjustment_list },
  { "photo", &photo_paper_adjustment_list },
  { "photo2", &photo2_paper_adjustment_list },
  { "photo3", &photo3_paper_adjustment_list },
  { "sp960", &sp960_paper_adjustment_list },
  { "ultrachrome_photo", &ultrachrome_photo_paper_adjustment_list },
  { "ultrachrome_matte", &ultrachrome_matte_paper_adjustment_list },
  { "ultrachrome_k3_photo", &ultrachrome_k3_photo_paper_adjustment_list },
  { "ultrachrome_k3_matte", &ultrachrome_k3_matte_paper_adjustment_list },
  { "r800_photo", &r800_photo_paper_adjustment_list },
  { "r800_matte", &r800_matte_paper_adjustment_list },
  { "picturemate", &picturemate_paper_adjustment_list },
  { "claria", &claria_paper_adjustment_list },
};

const paper_adjustment_list_t *
stpi_escp2_get_paper_adjustment_list_named(const char *n)
{
  int i;
  if (n)
    for (i = 0; i < sizeof(the_adjustments) / sizeof(paperadj_t); i++)
      {
	if (strcmp(n, the_adjustments[i].name) == 0)
	  return the_adjustments[i].paper_list;
      }
  return NULL;
}


#define DECLARE_INPUT_SLOT(name)				\
static const input_slot_list_t name##_input_slot_list =		\
{								\
  #name,							\
  name##_input_slots,						\
  sizeof(name##_input_slots) / sizeof(const input_slot_t),	\
}

static const input_slot_t standard_roll_feed_input_slots[] =
{
  {
    "Standard",
    N_("Standard"),
    0,
    0,
    0,
    { 16, "IR\002\000\000\001EX\006\000\000\000\000\000\005\000" },
    { 6, "IR\002\000\000\000"}
  },
  {
    "Roll",
    N_("Roll Feed"),
    0,
    1,
    ROLL_FEED_DONT_EJECT,
    { 16, "IR\002\000\000\001EX\006\000\000\000\000\000\005\001" },
    { 6, "IR\002\000\000\002" }
  }
};

DECLARE_INPUT_SLOT(standard_roll_feed);

static const input_slot_t cutter_roll_feed_input_slots[] =
{
  {
    "Standard",
    N_("Standard"),
    0,
    0,
    0,
    { 16, "IR\002\000\000\001EX\006\000\000\000\000\000\005\000" },
    { 6, "IR\002\000\000\000"}
  },
  {
    "RollCutPage",
    N_("Roll Feed (cut each page)"),
    0,
    1,
    ROLL_FEED_CUT_ALL,
    { 16, "IR\002\000\000\001EX\006\000\000\000\000\000\005\001" },
    { 6, "IR\002\000\000\002" }
  },
  {
    "RollCutNone",
    N_("Roll Feed (do not cut)"),
    0,
    1,
    ROLL_FEED_DONT_EJECT,
    { 16, "IR\002\000\000\001EX\006\000\000\000\000\000\005\001" },
    { 6, "IR\002\000\000\002" }
  }
};

DECLARE_INPUT_SLOT(cutter_roll_feed);

static const input_slot_t cd_cutter_roll_feed_input_slots[] =
{
  {
    "Standard",
    N_("Standard"),
    0,
    0,
    0,
    { 23, "IR\002\000\000\001EX\006\000\000\000\000\000\005\000PP\003\000\000\001\377" },
    { 6, "IR\002\000\000\000"}
  },
  {
    "Manual",
    N_("Manual Feed"),
    0,
    0,
    0,
    { 36, "PM\002\000\000\000IR\002\000\000\001EX\006\000\000\000\000\000\005\000FP\003\000\000\000\000PP\003\000\000\002\001" },
    { 6, "IR\002\000\000\000"}
  },
  {
    "CD",
    N_("Print to CD"),
    1,
    0,
    0,
    { 36, "PM\002\000\000\000IR\002\000\000\001EX\006\000\000\000\000\000\005\000FP\003\000\000\000\000PP\003\000\000\002\001" },
    { 6, "IR\002\000\000\000"}
  },
  {
    "RollCutPage",
    N_("Roll Feed (cut each page)"),
    0,
    1,
    ROLL_FEED_CUT_ALL,
    { 23, "IR\002\000\000\001EX\006\000\000\000\000\000\005\001PP\003\000\000\001\377" },
    { 6, "IR\002\000\000\002" }
  },
  {
    "RollCutNone",
    N_("Roll Feed (do not cut)"),
    0,
    1,
    ROLL_FEED_DONT_EJECT,
    { 23, "IR\002\000\000\001EX\006\000\000\000\000\000\005\001PP\003\000\000\001\377" },
    { 6, "IR\002\000\000\002" }
  }
};

DECLARE_INPUT_SLOT(cd_cutter_roll_feed);

static const input_slot_t cd_roll_feed_input_slots[] =
{
  {
    "Standard",
    N_("Standard"),
    0,
    0,
    0,
    { 23, "IR\002\000\000\001EX\006\000\000\000\000\000\005\000PP\003\000\000\001\377" },
    { 6, "IR\002\000\000\000"}
  },
  {
    "Manual",
    N_("Manual Feed"),
    0,
    0,
    0,
    { 36, "PM\002\000\000\000IR\002\000\000\001EX\006\000\000\000\000\000\005\000FP\003\000\000\000\000PP\003\000\000\002\001" },
    { 6, "IR\002\000\000\000"}
  },
  {
    "CD",
    N_("Print to CD"),
    1,
    0,
    0,
    { 36, "PM\002\000\000\000IR\002\000\000\001EX\006\000\000\000\000\000\005\000FP\003\000\000\000\000PP\003\000\000\002\001" },
    { 6, "IR\002\000\000\000"}
  },
  {
    "Roll",
    N_("Roll Feed"),
    0,
    1,
    ROLL_FEED_DONT_EJECT,
    { 23, "IR\002\000\000\001EX\006\000\000\000\000\000\005\001PP\003\000\000\001\377" },
    { 6, "IR\002\000\000\002" }
  }
};

DECLARE_INPUT_SLOT(cd_roll_feed);

static const input_slot_t r2400_input_slots[] =
{
  {
    "Standard",
    N_("Standard"),
    0,
    0,
    0,
    { 23, "IR\002\000\000\001EX\006\000\000\000\000\000\005\000PP\003\000\000\001\377" },
    { 6, "IR\002\000\000\000"}
  },
  {
    "Velvet",
    N_("Manual Sheet Guide"),
    0,
    0,
    0,
    { 23, "IR\002\000\000\001EX\006\000\000\000\000\000\005\000PP\003\000\000\003\000" },
    { 6, "IR\002\000\000\000"}
  },
  {
    "Matte",
    N_("Manual Feed (Front)"),
    0,
    0,
    0,
    { 23, "IR\002\000\000\001EX\006\000\000\000\000\000\005\000PP\003\000\000\002\000" },
    { 6, "IR\002\000\000\000"}
  },
  {
    "Roll",
    N_("Roll Feed"),
    0,
    1,
    ROLL_FEED_DONT_EJECT,
    { 23, "IR\002\000\000\001EX\006\000\000\000\000\000\005\001PP\003\000\000\003\001" },
    { 6, "IR\002\000\000\002" }
  }
};

DECLARE_INPUT_SLOT(r2400);

static const input_slot_t r1800_input_slots[] =
{
  {
    "Standard",
    N_("Standard"),
    0,
    0,
    0,
    { 23, "IR\002\000\000\001EX\006\000\000\000\000\000\005\000PP\003\000\000\001\377" },
    { 6, "IR\002\000\000\000"}
  },
  {
    "Velvet",
    N_("Manual Sheet Guide"),
    0,
    0,
    0,
    { 23, "IR\002\000\000\001EX\006\000\000\000\000\000\005\000PP\003\000\000\003\000" },
    { 6, "IR\002\000\000\000"}
  },
  {
    "Matte",
    N_("Manual Feed (Front)"),
    0,
    0,
    0,
    { 23, "IR\002\000\000\001EX\006\000\000\000\000\000\005\000PP\003\000\000\002\000" },
    { 6, "IR\002\000\000\000"}
  },
  {
    "Roll",
    N_("Roll Feed"),
    0,
    1,
    ROLL_FEED_DONT_EJECT,
    { 23, "IR\002\000\000\001EX\006\000\000\000\000\000\005\001PP\003\000\000\003\001" },
    { 6, "IR\002\000\000\002" }
  },
  {
    "CD",
    N_("Print to CD"),
    1,
    0,
    0,
    { 36, "PM\002\000\000\000IR\002\000\000\001EX\006\000\000\000\000\000\005\000FP\003\000\000\000\000PP\003\000\000\002\001" },
    { 6, "IR\002\000\000\000"}
  },
};

DECLARE_INPUT_SLOT(r1800);

static const input_slot_t rx700_input_slots[] =
{
  {
    "Rear",
    N_("Rear Tray"),
    0,
    0,
    0,
    { 23, "IR\002\000\000\001EX\006\000\000\000\000\000\005\000PP\003\000\000\001\000" },
    { 6, "IR\002\000\000\000"}
  },
  {
    "Front",
    N_("Front Tray"),
    0,
    0,
    0,
    { 23, "IR\002\000\000\001EX\006\000\000\000\000\000\005\000PP\003\000\000\001\001" },
    { 6, "IR\002\000\000\000"}
  },
  {
    "CD",
    N_("Print to CD"),
    1,
    0,
    0,
    { 36, "PM\002\000\000\000IR\002\000\000\001EX\006\000\000\000\000\000\005\000FP\003\000\000\000\000PP\003\000\000\002\001" },
    { 6, "IR\002\000\000\000"}
  },
  {
    "PhotoBoard",
    N_("Photo Board"),
    0,
    0,
    0,
    { 23, "IR\002\000\000\001EX\006\000\000\000\000\000\005\000PP\003\000\000\002\000" },
    { 6, "IR\002\000\000\000"}
  },
};

DECLARE_INPUT_SLOT(rx700);

static const input_slot_t pro_roll_feed_input_slots[] =
{
  {
    "Standard",
    N_("Standard"),
    0,
    0,
    0,
    { 7, "PP\003\000\000\002\000" },
    { 0, "" }
  },
  {
    "Roll",
    N_("Roll Feed"),
    0,
    1,
    0,
    { 7, "PP\003\000\000\003\000" },
    { 0, "" }
  }
};

DECLARE_INPUT_SLOT(pro_roll_feed);

static const input_slot_t spro5000_input_slots[] =
{
  {
    "CutSheet1",
    N_("Cut Sheet Bin 1"),
    0,
    0,
    0,
    { 7, "PP\003\000\000\001\001" },
    { 0, "" }
  },
  {
    "CutSheet2",
    N_("Cut Sheet Bin 2"),
    0,
    0,
    0,
    { 7, "PP\003\000\000\002\001" },
    { 0, "" }
  },
  {
    "CutSheetAuto",
    N_("Cut Sheet Autoselect"),
    0,
    0,
    0,
    { 7, "PP\003\000\000\001\377" },
    { 0, "" }
  },
  {
    "ManualSelect",
    N_("Manual Selection"),
    0,
    0,
    0,
    { 7, "PP\003\000\000\002\001" },
    { 0, "" }
  }
};

DECLARE_INPUT_SLOT(spro5000);

static const input_slot_list_t default_input_slot_list =
{
  "Standard",
  NULL,
  0,
};

typedef struct
{
  const char *name;
  const input_slot_list_t *input_slots;
} inslot_t;

static const inslot_t the_slots[] =
{
  { "cd_cutter_roll_feed", &cd_cutter_roll_feed_input_slot_list },
  { "cd_roll_feed", &cd_roll_feed_input_slot_list },
  { "cutter_roll_feed", &cutter_roll_feed_input_slot_list },
  { "default", &default_input_slot_list },
  { "pro_roll_feed", &pro_roll_feed_input_slot_list },
  { "r1800", &r1800_input_slot_list },
  { "r2400", &r2400_input_slot_list },
  { "rx700", &rx700_input_slot_list },
  { "spro5000", &spro5000_input_slot_list },
  { "standard_roll_feed", &standard_roll_feed_input_slot_list },
};

const input_slot_list_t *
stpi_escp2_get_input_slot_list_named(const char *n)
{
  int i;
  if (n)
    for (i = 0; i < sizeof(the_slots) / sizeof(inslot_t); i++)
      {
	if (strcmp(n, the_slots[i].name) == 0)
	  return the_slots[i].input_slots;
      }
  return NULL;
}

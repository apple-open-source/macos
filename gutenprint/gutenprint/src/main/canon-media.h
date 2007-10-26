/*
 *   Print plug-in CANON BJL driver for the GIMP.
 *
 *   Copyright 1997-2000 Michael Sweet (mike@easysw.com),
 *	Robert Krawitz (rlk@alum.mit.edu) and
 *      Andy Thaller (thaller@ph.tum.de)
 *   Copyright (c) 2006 Sascha Sommer (saschasommer@freenet.de)
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

/* This file contains the definitions for the possible Media Types
   TODO: Color Correction and Density adjustment 
*/

#ifndef GUTENPRINT_INTERNAL_CANON_MEDIA_H
#define GUTENPRINT_INTERNAL_CANON_MEDIA_H

/* media related structs */


/* media slots */

typedef struct {
  const char* name;
  const char* text;
  unsigned char code;
} canon_slot_t;

typedef struct {
  const char *name;
  short count;
  const canon_slot_t *slots;
} canon_slotlist_t;

#define DECLARE_SLOTS(name)                           \
static const canon_slotlist_t name##_slotlist = {     \
  #name,                                              \
  sizeof(name##_slots) / sizeof(canon_slot_t),        \
  name##_slots                                        \
}


static const canon_slot_t canon_default_slots[] = {
  { "Auto",       N_ ("Auto Sheet Feeder"), 4 },
  { "Manual",     N_ ("Manual with Pause"), 0 },
  { "ManualNP",   N_ ("Manual without Pause"), 1 },
  { "Cassette",   N_ ("Cassette"), 8 },
  { "CD",         N_ ("CD tray"), 10 },
};
DECLARE_SLOTS(canon_default);


static const canon_slot_t canon_PIXMA_iP4000_slots[] = {
  { "SelectKey",  N_ ("Selected by Paper Select Key"), 3 },
  { "Auto",       N_ ("Auto Sheet Feeder"), 4 },
  { "Cassette",   N_ ("Cassette"), 0x8 },
  { "CD",         N_ ("CD tray"), 10 },
  { "AutoSwitch", N_ ("Automatic Paper Source Switching"), 15 },
};
DECLARE_SLOTS(canon_PIXMA_iP4000);

/* media types */

typedef struct {
  const char *name;                        /* Internal Name may not contain spaces */
  const char *text;                        /* Translateable name */
  unsigned char media_code_c;              /* Media Code used for the ESC (c (SetColor) command */
  unsigned char media_code_l;              /* Media Code used for the ESC (l (SetTray) command */
  unsigned int qualities;
#define Q0        0                        /* lowest quality */
#define Q1        1
#define Q2        2
#define Q3        4
#define Q4        8                        /* highest quality */
  double base_density;
  double k_lower_scale;
  double k_upper;
  const char *hue_adjustment;
  const char *lum_adjustment;
  const char *sat_adjustment;
} canon_paper_t;

typedef struct {
  const char *name;
  short count;
  const canon_paper_t *papers;
} canon_paperlist_t;

#define DECLARE_PAPERS(name)                            \
static const canon_paperlist_t name##_paperlist = {     \
  #name,                                                \
  sizeof(name##_papers) / sizeof(canon_paper_t),        \
  name##_papers                                         \
}


/* paperlists for the various printers. The first entry will be the default */

static const canon_paper_t canon_default_papers[] = {
  { "Plain",		N_ ("Plain Paper"),		0x00, 0x00,Q2,0.50, 0.25, 0.500, 0, 0, 0 },
  { "Transparency",	N_ ("Transparencies"),		0x02, 0x02,Q2,1.00, 1.00, 0.900, 0, 0, 0 },
  { "BackPrint",	N_ ("Back Print Film"),		0x03, 0x03,Q2,1.00, 1.00, 0.900, 0, 0, 0 },
  { "Fabric",		N_ ("Fabric Sheets"),		0x04, 0x04,Q2,0.50, 0.25, 0.500, 0, 0, 0 },
  { "Envelope",		N_ ("Envelope"),		0x08, 0x08,Q2,0.50, 0.25, 0.500, 0, 0, 0 },
  { "Coated",		N_ ("High Resolution Paper"),	0x07, 0x07,Q2,0.78, 0.25, 0.500, 0, 0, 0 },
  { "TShirt",		N_ ("T-Shirt Transfers"),	0x03, 0x03,Q2,0.50, 0.25, 0.500, 0, 0, 0 },
  { "GlossyFilm",	N_ ("High Gloss Film"),		0x06, 0x06,Q2,1.00, 1.00, 0.999, 0, 0, 0 },
  { "GlossyPaper",	N_ ("Glossy Photo Paper"),	0x05, 0x05,Q2,1.00, 1.00, 0.999, 0, 0, 0 },
  { "GlossyCard",	N_ ("Glossy Photo Cards"),	0x0a, 0x0a,Q2,1.00, 1.00, 0.999, 0, 0, 0 },
  { "GlossyPro",	N_ ("Photo Paper Pro"),		0x09, 0x09,Q2,1.00, 1.00, 0.999, 0, 0, 0 },
  { "Other",		N_ ("Other"),                   0x00, 0x00,Q2,0.50, 0.25, .5, 0, 0, 0 },
};
DECLARE_PAPERS(canon_default);

static const canon_paper_t canon_PIXMA_iP4000_papers[] = {
  { "Plain",		N_ ("Plain Paper"),			0x00,0x00,Q0|Q1|Q2|Q3,1.00, 0.25, 0.500, 0, 0, 0 },
  { "Transparency", 	N_ ("Transparencies"),			0x02,0x02,Q2|Q3,0.78, 0.25, 0.500, 0, 0, 0 },
  { "TShirt",		N_ ("T-Shirt Transfers"),		0x03,0x03,Q2,0.78, 0.25, 0.500, 0, 0, 0 },
  { "GlossyPaper",	N_ ("Glossy Photo Paper"),		0x05,0x05,Q2|Q3,0.78, 0.25, 0.500, 0, 0, 0 },
  { "Coated",		N_ ("High Resolution Paper"),		0x07,0x07,Q2|Q3,0.78, 0.25, 0.500, 0, 0, 0 },
  { "Envelope",		N_ ("Envelope"),			0x08,0x08,Q2|Q3,0.78, 0.25, 0.500, 0, 0, 0 },
  { "PhotopaperPro",	N_ ("Professional Photo Paper"),	0x09,0x0d,Q2|Q3|Q4,0.78, 0.25, 0.500, 0, 0, 0 },
  { "PhotopaperMatte",	N_ ("Photo Paper Matte"),		0x0a,0x10,Q2|Q3,0.78, 0.25, 0.500, 0, 0, 0 },
  { "PhotopaperPlus",	N_ ("Glossy Photo Paper Plus"), 	0x0b,0x11,Q1|Q2|Q3,0.78, 0.25, 0.500, 0, 0, 0 },
  { "CD",		N_ ("CD"),				0x00,0x12,Q2|Q3|Q4,0.78, 0.25, 0.500, 0, 0, 0 }, 
  /* FIXME media code for c) should be 0x0c for CD but this will restrict CD printing to a single, not well supported, resolution */
  { "PhotopaperOther",	N_ ("Other Photo Paper"),		0x0f,0x14,Q2,0.78, 0.25, 0.500, 0, 0, 0 },
  { "PhotopaperPlusDouble", N_ ("Photoper Plus Double-Sided"),	0x10,0x15,Q2|Q3,0.78, 0.25, 0.500, 0, 0, 0 },
};
DECLARE_PAPERS(canon_PIXMA_iP4000);

#endif


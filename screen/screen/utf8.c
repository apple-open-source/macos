/* Copyright (c) 1993-2001
 *      Juergen Weigert (jnweiger@immd4.informatik.uni-erlangen.de)
 *      Michael Schroeder (mlschroe@immd4.informatik.uni-erlangen.de)
 * Copyright (c) 1987 Oliver Laumann 
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option) 
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, 
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the 
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program (see the file COPYING); if not, write to the
 * Free Software Foundation, Inc.,
 * 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA
 *
 ****************************************************************
 */

#include "rcs.h" 
RCS_ID("$Id: utf8.c,v 1.1.1.1 2001/12/14 22:08:29 bbraun Exp $ FAU") 

#include <sys/types.h>

#include "config.h"
#include "screen.h"
#include "extern.h"

#ifdef UTF8

extern char *null;
extern struct display *display;

static unsigned short recodetab[][2] = {
  { 0x305f, 0x25AE },	/* 0: special graphics (line drawing) */
  { 0x3060, 0x25C6 },
  { 0x3061, 0x2592 },
  { 0x3062, 0x2409 },
  { 0x3063, 0x240C },
  { 0x3064, 0x240D },
  { 0x3065, 0x240A },
  { 0x3066, 0x00B0 },
  { 0x3067, 0x00B1 },
  { 0x3068, 0x2424 },
  { 0x3069, 0x240B },
  { 0x306a, 0x2518 },
  { 0x306b, 0x2510 },
  { 0x306c, 0x250C },
  { 0x306d, 0x2514 },
  { 0x306e, 0x253C },
  { 0x306f, 0x23BA },
  { 0x3070, 0x23BB },
  { 0x3071, 0x2500 },
  { 0x3072, 0x23BC },
  { 0x3073, 0x23BD },
  { 0x3074, 0x251C },
  { 0x3075, 0x2524 },
  { 0x3076, 0x2534 },
  { 0x3077, 0x252C },
  { 0x3078, 0x2502 },
  { 0x3079, 0x2264 },
  { 0x307a, 0x2265 },
  { 0x307b, 0x03C0 },
  { 0x307c, 0x2260 },
  { 0x307d, 0x00A3 },
  { 0x307e, 0x00B7 },

  { 0x3423, 0x00a3 },	/* 4: Dutch */
  { 0x3440, 0x00be },
  { 0x345b, 0x00ff },
  { 0x345c, 0x00bd },
  { 0x345d, 0x007c },
  { 0x347b, 0x00a8 },
  { 0x347c, 0x0066 },
  { 0x347d, 0x00bc },
  { 0x347e, 0x00b4 },

  { 0x355b, 0x00c4 },	/* 5: Finnish */
  { 0x355c, 0x00d6 },
  { 0x355d, 0x00c5 },
  { 0x355e, 0x00dc },
  { 0x3560, 0x00e9 },
  { 0x357b, 0x00e4 },
  { 0x357c, 0x00f6 },
  { 0x357d, 0x00e5 },
  { 0x357e, 0x00fc },

  { 0x3640, 0x00c4 },	/* 6: Norwegian/Danish */
  { 0x365b, 0x00c6 },
  { 0x365c, 0x00d8 },
  { 0x365d, 0x00c5 },
  { 0x365e, 0x00dc },
  { 0x3660, 0x00e4 },
  { 0x367b, 0x00e6 },
  { 0x367c, 0x00f8 },
  { 0x367d, 0x00e5 },
  { 0x367e, 0x00fc },

  { 0x3740, 0x00c9 },	/* 7: Swedish */
  { 0x375b, 0x00c4 },
  { 0x375c, 0x00d6 },
  { 0x375d, 0x00c5 },
  { 0x375e, 0x00dc },
  { 0x3760, 0x00e9 },
  { 0x377b, 0x00e4 },
  { 0x377c, 0x00f6 },
  { 0x377d, 0x00e5 },
  { 0x377e, 0x00fc },

  { 0x3d23, 0x00f9 },	/* =: Swiss */
  { 0x3d40, 0x00e0 },
  { 0x3d5b, 0x00e9 },
  { 0x3d5c, 0x00e7 },
  { 0x3d5d, 0x00ea },
  { 0x3d5e, 0x00ee },
  { 0x3d5f, 0x00e8 },
  { 0x3d60, 0x00f4 },
  { 0x3d7b, 0x00e4 },
  { 0x3d7c, 0x00f6 },
  { 0x3d7d, 0x00fc },
  { 0x3d7e, 0x00fb },

  { 0x4123, 0x00a3 },	/* A: UK */

  { 0x4b40, 0x00a7 },	/* K: German */
  { 0x4b5b, 0x00c4 },
  { 0x4b5c, 0x00d6 },
  { 0x4b5d, 0x00dc },
  { 0x4b7b, 0x00e4 },
  { 0x4b7c, 0x00f6 },
  { 0x4b7d, 0x00fc },
  { 0x4b7e, 0x00df },

  { 0x5140, 0x00e0 },	/* Q: French Canadian */
  { 0x515b, 0x00e2 },
  { 0x515c, 0x00e7 },
  { 0x515d, 0x00ea },
  { 0x515e, 0x00ee },
  { 0x5160, 0x00f4 },
  { 0x517b, 0x00e9 },
  { 0x517c, 0x00f9 },
  { 0x517d, 0x00e8 },
  { 0x517e, 0x00fb },

  { 0x5223, 0x00a3 },	/* R: French */
  { 0x5240, 0x00e0 },
  { 0x525b, 0x00b0 },
  { 0x525c, 0x00e7 },
  { 0x525d, 0x00a7 },
  { 0x527b, 0x00e9 },
  { 0x527c, 0x00f9 },
  { 0x527d, 0x00e8 },
  { 0x527e, 0x00a8 },

  { 0x5923, 0x00a3 },	/* Y: Italian */
  { 0x5940, 0x00a7 },
  { 0x595b, 0x00b0 },
  { 0x595c, 0x00e7 },
  { 0x595d, 0x00e9 },
  { 0x5960, 0x00f9 },
  { 0x597b, 0x00e0 },
  { 0x597c, 0x00f2 },
  { 0x597d, 0x00e8 },
  { 0x597e, 0x00ec },

  { 0x5a23, 0x00a3 },	/* Z: Spanish */
  { 0x5a40, 0x00a7 },
  { 0x5a5b, 0x00a1 },
  { 0x5a5c, 0x00d1 },
  { 0x5a5d, 0x00bf },
  { 0x5a7b, 0x00b0 },
  { 0x5a7c, 0x00f1 },
  { 0x5a7d, 0x00e7 }
};

int
recode_char(c, from, to)
int c, from, to;
{
  int i;
  if (c < 256)
    return c;
  if (from == 0)
    {
      /* map aliases to keep the table small */
      switch (c >> 8)
	{
	  case 'C':
	    c ^= ('C' ^ '5') << 8;
	    break;
	  case 'E':
	    c ^= ('E' ^ '6') << 8;
	    break;
	  case 'H':
	    c ^= ('H' ^ '7') << 8;
	    break;
	  default:
	    break;
	}
    }
  for (i = 0; i < sizeof(recodetab)/sizeof(*recodetab); i++)
    {
      if (i && (recodetab[i][0] & 0x8000) != 0)
	{
	  if (recodetab[i - 1][from] <= c && c <= (from ? recodetab[i][from] : (recodetab[i][from] & 0x7fff)))
	    {
	      debug2("recoded %d:%04x to ", from, c);
	      c = c - recodetab[i - 1][from] + recodetab[i - 1][to];
	      debug2("%d:%04x\n", to, c);
	      return c;
	    }
	}
      if (recodetab[i][from] == c)
	{
	  debug2("recoded %d:%04x to ", from, c);
	  c = recodetab[i][to];
	  debug2("%d:%04x\n", to, c);
	  return c;
	}
    }
  debug2("failed to recode %d:%04x\n", from, c);
  if (to)
    return c & 255;	/* -> utf8: map to latin1 */
  else
    return '?';		/* utf8 -> */
}


struct mchar *
recode_mchar(mc, from, to)
struct mchar *mc;
int from, to;
{
  static struct mchar rmc;
  int c;

  if (from == to)
    return mc;
  if (mc->font == 0)	/* latin1 is the same in unicode */
    return mc;
  rmc = *mc;
  c = (unsigned char)rmc.image | ((unsigned char)rmc.font << 8);
  c = recode_char(c, from, to);
  rmc.image = c & 255;
  rmc.font = c >> 8 & 255;
  return &rmc;
}

struct mline *
recode_mline(ml, w, from, to)
struct mline *ml;
int w;
int from, to;
{
  static int maxlen;
  static int last;
  static struct mline rml[2], *rl;
  int i, c;

  if (from == to || w == 0)
    return ml;
  if (ml->font == null)
    return ml;
  if (w > maxlen)
    {
      for (i = 0; i < 2; i++)
	{
	  if (rml[i].image == 0)
	    rml[i].image = malloc(w);
	  else
	    rml[i].image = realloc(rml[i].image, w);
	  if (rml[i].font == 0)
	    rml[i].font = malloc(w);
	  else
	    rml[i].font = realloc(rml[i].font, w);
	  if (rml[i].image == 0 || rml[i].font == 0)
	    {
	      maxlen = 0;
	      return ml;	/* sorry */
	    }
	}
      maxlen = w;
    }
  rl = rml + last;
  rl->attr = ml->attr;
#ifdef COLOR
  rl->color = ml->color;
#endif
  for (i = 0; i < w; i++)
    {
      c = (unsigned char)ml->image[i] | ((unsigned char)ml->font[i] << 8);
      c = recode_char(c, from, to);
      rl->image[i] = c & 255;
      rl->font[i] = c >> 8 & 255;
    }
  last ^= 1;
  return rl;
}

void
AddUtf8(c)
int c;
{
  ASSERT(D_utf8);
  if (c >= 0x800)
    {
      AddChar((c & 0xf000) >> 12 | 0xe0);
      c = (c & 0x0fff) | 0x1000; 
    }
  if (c >= 0x80)
    {
      AddChar((c & 0x1fc0) >> 6 ^ 0xc0);
      c = (c & 0x3f) | 0x80; 
    }
  AddChar(c);
}

int
ToUtf8(p, c)
char *p;
int c;
{
  int l = 1;
  if (c >= 0x800)
    {
      if (p)
	*p++ = (c & 0xf000) >> 12 | 0xe0; 
      l++;
      c = (c & 0x0fff) | 0x1000; 
    }
  if (c >= 0x80)
    {
      if (p)
	*p++ = (c & 0x1fc0) >> 6 ^ 0xc0; 
      l++;
      c = (c & 0x3f) | 0x80; 
    }
  if (p)
    *p++ = c;
  return l;
}

/*
 * returns:
 * -1: need more bytes, sequence not finished
 * -2: corrupt sequence found, redo last char
 * >= 0: decoded character
 */
int
FromUtf8(c, utf8charp)
int c, *utf8charp;
{
  int utf8char = *utf8charp;
  if (utf8char)
    {
      if ((c & 0xc0) != 0x80)
	{
	  *utf8charp = 0;
	  return -2; /* corrupt sequence! */
	}
      else
	c = (c & 0x3f) | (utf8char << 6);
      if (!(utf8char & 0x40000000))
	{
	  /* check for overlong sequences */
	  if ((c & 0x820823e0) == 0x80000000)
	    c = 0xfdffffff;
	  else if ((c & 0x020821f0) == 0x02000000)
	    c = 0xfff7ffff;
	  else if ((c & 0x000820f8) == 0x00080000)
	    c = 0xffffd000;
	  else if ((c & 0x0000207c) == 0x00002000)
	    c = 0xffffff70;
	}
    }
  else
    {
      /* new sequence */
      if (c >= 0xfe)
	c = UCS_REPL;
      else if (c >= 0xfc)
	c = (c & 0x01) | 0xbffffffc;	/* 5 bytes to follow */
      else if (c >= 0xf8)
	c = (c & 0x03) | 0xbfffff00;	/* 4 */
      else if (c >= 0xf0)
	c = (c & 0x07) | 0xbfffc000;	/* 3 */
      else if (c >= 0xe0)
	c = (c & 0x0f) | 0xbff00000;	/* 2 */
      else if (c >= 0xc2)
	c = (c & 0x1f) | 0xfc000000;	/* 1 */
      else if (c >= 0xc0)
	c = 0xfdffffff;		/* overlong */
      else if (c >= 0x80)
	c = UCS_REPL;
    }
  *utf8charp = utf8char = (c & 0x80000000) ? c : 0;
  if (utf8char)
    return -1;
  if (c & 0xffff0000)
    c = UCS_REPL;	/* sorry, only know 16bit Unicode */
  if (c >= 0xd800 && (c <= 0xdfff || c == 0xfffe || c == 0xffff))
    c = UCS_REPL;	/* illegal code */
  return c;
}


void
WinSwitchUtf8(p, utf8)
struct win *p;
int utf8;
{
  int i, j, c;
  struct mline *ml;

  if (p->w_utf8 == utf8)
    return;
  for (j = 0; j < p->w_height + p->w_histheight; j++)
    {
#ifdef COPY_PASTE
      ml = j < p->w_height ? &p->w_mlines[j] : &p->w_hlines[j - p->w_height];
#else
      ml = &p->w_mlines[j];
#endif
      if (ml->font == 0 || ml->font == null)
	continue;
      for (i = 0; i < p->w_width; i++)
	{
	  if (ml->font[i] == 0)
	    continue;
	  c = (unsigned char)ml->image[i] | ((unsigned char)ml->font[i] << 8);
	  c = recode_char(c, p->w_utf8, utf8);
	  ml->image[i] = c & 255;
	  ml->font[i] = c >> 8 & 255;
	}
    }
  p->w_utf8 = utf8;
  return;
}

#endif

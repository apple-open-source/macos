/* Copyright (c) 1993-2003
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

#include <sys/types.h>

#include "config.h"
#include "screen.h"
#include "extern.h"

#ifdef ENCODINGS

extern unsigned char *null;
extern struct display *display, *displays;
extern struct layer *flayer;

extern char *screenencodings;

static int  encmatch __P((char *, char *));
# ifdef UTF8
static int   recode_char __P((int, int, int));
static int   recode_char_to_encoding __P((int, int));
static void  comb_tofront __P((int, int));
#  ifdef DW_CHARS
static int   recode_char_dw __P((int, int *, int, int));
static int   recode_char_dw_to_encoding __P((int, int *, int));
#  endif
# endif

struct encoding {
  char *name;
  char *charsets;
  int  deffont;
  int  usegr;
  int  noc1;
  char *fontlist; 
};

/* big5 font:   ^X */
/* KOI8-R font: 96 ! */
/* CP1251 font: 96 ? */

struct encoding encodings[] = {
  { "C",		0,		0,		0, 0, 0 },
  { "eucJP",		"B\002I\00401",	0,		1, 0, "\002\004I" },
  { "SJIS",		"BIBB01",	0,		1, 1, "\002I" },
  { "eucKR",		"B\003BB01",	0,		1, 0, "\003" },
  { "eucCN",		"B\001BB01",	0,		1, 0, "\001" },
  { "Big5",		"B\030BB01",	0,		1, 0, "\030" },
  { "KOI8-R",		0,		0x80|'!',	0, 1, 0 },
  { "CP1251",		0,		0x80|'?',	0, 1, 0 },
  { "UTF-8",		0,		-1,		0, 0, 0 },
  { "ISO8859-2",	0,		0x80|'B',	0, 0, 0 },
  { "ISO8859-3",	0,		0x80|'C',	0, 0, 0 },
  { "ISO8859-4",	0,		0x80|'D',	0, 0, 0 },
  { "ISO8859-5",	0,		0x80|'L',	0, 0, 0 },
  { "ISO8859-6",	0,		0x80|'G',	0, 0, 0 },
  { "ISO8859-7",	0,		0x80|'F',	0, 0, 0 },
  { "ISO8859-8",	0,		0x80|'H',	0, 0, 0 },
  { "ISO8859-9",	0,		0x80|'M',	0, 0, 0 },
  { "ISO8859-10",	0,		0x80|'V',	0, 0, 0 },
  { "ISO8859-15",	0,		0x80|'b',	0, 0, 0 },
  { "jis",		0,		0,		0, 0, "\002\004I" },
  { "GBK",		"B\031BB01",	0x80|'b',	1, 1, "\031" }
};

#ifdef UTF8

static unsigned short builtin_tabs[][2] = {
  { 0x30, 0 },		/* 0: special graphics (line drawing) */
  { 0x005f, 0x25AE },
  { 0x0060, 0x25C6 },
  { 0x0061, 0x2592 },
  { 0x0062, 0x2409 },
  { 0x0063, 0x240C },
  { 0x0064, 0x240D },
  { 0x0065, 0x240A },
  { 0x0066, 0x00B0 },
  { 0x0067, 0x00B1 },
  { 0x0068, 0x2424 },
  { 0x0069, 0x240B },
  { 0x006a, 0x2518 },
  { 0x006b, 0x2510 },
  { 0x006c, 0x250C },
  { 0x006d, 0x2514 },
  { 0x006e, 0x253C },
  { 0x006f, 0x23BA },
  { 0x0070, 0x23BB },
  { 0x0071, 0x2500 },
  { 0x0072, 0x23BC },
  { 0x0073, 0x23BD },
  { 0x0074, 0x251C },
  { 0x0075, 0x2524 },
  { 0x0076, 0x2534 },
  { 0x0077, 0x252C },
  { 0x0078, 0x2502 },
  { 0x0079, 0x2264 },
  { 0x007a, 0x2265 },
  { 0x007b, 0x03C0 },
  { 0x007c, 0x2260 },
  { 0x007d, 0x00A3 },
  { 0x007e, 0x00B7 },
  { 0, 0},

  { 0x34, 0 },		/* 4: Dutch */
  { 0x0023, 0x00a3 },
  { 0x0040, 0x00be },
  { 0x005b, 0x00ff },
  { 0x005c, 0x00bd },
  { 0x005d, 0x007c },
  { 0x007b, 0x00a8 },
  { 0x007c, 0x0066 },
  { 0x007d, 0x00bc },
  { 0x007e, 0x00b4 },
  { 0, 0},

  { 0x35, 0 },		/* 5: Finnish */
  { 0x005b, 0x00c4 },
  { 0x005c, 0x00d6 },
  { 0x005d, 0x00c5 },
  { 0x005e, 0x00dc },
  { 0x0060, 0x00e9 },
  { 0x007b, 0x00e4 },
  { 0x007c, 0x00f6 },
  { 0x007d, 0x00e5 },
  { 0x007e, 0x00fc },
  { 0, 0},

  { 0x36, 0 },		/* 6: Norwegian/Danish */
  { 0x0040, 0x00c4 },
  { 0x005b, 0x00c6 },
  { 0x005c, 0x00d8 },
  { 0x005d, 0x00c5 },
  { 0x005e, 0x00dc },
  { 0x0060, 0x00e4 },
  { 0x007b, 0x00e6 },
  { 0x007c, 0x00f8 },
  { 0x007d, 0x00e5 },
  { 0x007e, 0x00fc },
  { 0, 0},

  { 0x37, 0 },		/* 7: Swedish */
  { 0x0040, 0x00c9 },
  { 0x005b, 0x00c4 },
  { 0x005c, 0x00d6 },
  { 0x005d, 0x00c5 },
  { 0x005e, 0x00dc },
  { 0x0060, 0x00e9 },
  { 0x007b, 0x00e4 },
  { 0x007c, 0x00f6 },
  { 0x007d, 0x00e5 },
  { 0x007e, 0x00fc },
  { 0, 0},

  { 0x3d, 0},		/* =: Swiss */
  { 0x0023, 0x00f9 },
  { 0x0040, 0x00e0 },
  { 0x005b, 0x00e9 },
  { 0x005c, 0x00e7 },
  { 0x005d, 0x00ea },
  { 0x005e, 0x00ee },
  { 0x005f, 0x00e8 },
  { 0x0060, 0x00f4 },
  { 0x007b, 0x00e4 },
  { 0x007c, 0x00f6 },
  { 0x007d, 0x00fc },
  { 0x007e, 0x00fb },
  { 0, 0},

  { 0x41, 0},		/* A: UK */
  { 0x0023, 0x00a3 },
  { 0, 0},

  { 0x4b, 0},		/* K: German */
  { 0x0040, 0x00a7 },
  { 0x005b, 0x00c4 },
  { 0x005c, 0x00d6 },
  { 0x005d, 0x00dc },
  { 0x007b, 0x00e4 },
  { 0x007c, 0x00f6 },
  { 0x007d, 0x00fc },
  { 0x007e, 0x00df },
  { 0, 0},

  { 0x51, 0},		/* Q: French Canadian */
  { 0x0040, 0x00e0 },
  { 0x005b, 0x00e2 },
  { 0x005c, 0x00e7 },
  { 0x005d, 0x00ea },
  { 0x005e, 0x00ee },
  { 0x0060, 0x00f4 },
  { 0x007b, 0x00e9 },
  { 0x007c, 0x00f9 },
  { 0x007d, 0x00e8 },
  { 0x007e, 0x00fb },
  { 0, 0},

  { 0x52, 0},		/* R: French */
  { 0x0023, 0x00a3 },
  { 0x0040, 0x00e0 },
  { 0x005b, 0x00b0 },
  { 0x005c, 0x00e7 },
  { 0x005d, 0x00a7 },
  { 0x007b, 0x00e9 },
  { 0x007c, 0x00f9 },
  { 0x007d, 0x00e8 },
  { 0x007e, 0x00a8 },
  { 0, 0},

  { 0x59, 0},		/* Y: Italian */
  { 0x0023, 0x00a3 },
  { 0x0040, 0x00a7 },
  { 0x005b, 0x00b0 },
  { 0x005c, 0x00e7 },
  { 0x005d, 0x00e9 },
  { 0x0060, 0x00f9 },
  { 0x007b, 0x00e0 },
  { 0x007c, 0x00f2 },
  { 0x007d, 0x00e8 },
  { 0x007e, 0x00ec },
  { 0, 0},

  { 0x5a, 0},		/* Z: Spanish */
  { 0x0023, 0x00a3 },
  { 0x0040, 0x00a7 },
  { 0x005b, 0x00a1 },
  { 0x005c, 0x00d1 },
  { 0x005d, 0x00bf },
  { 0x007b, 0x00b0 },
  { 0x007c, 0x00f1 },
  { 0x007d, 0x00e7 },
  { 0, 0},

  { 0xe2, 0},		/* 96-b: ISO-8859-15 */
  { 0x00a4, 0x20ac },
  { 0x00a6, 0x0160 },
  { 0x00a8, 0x0161 },
  { 0x00b4, 0x017D },
  { 0x00b8, 0x017E },
  { 0x00bc, 0x0152 },
  { 0x00bd, 0x0153 },
  { 0x00be, 0x0178 },
  { 0, 0},

  { 0x4a, 0},		/* J: JIS 0201 Roman */
  { 0x005c, 0x00a5 },
  { 0x007e, 0x203e },
  { 0, 0},

  { 0x49, 0},		/* I: halfwidth katakana */
  { 0x0021, 0xff61 },
  { 0x005f|0x8000, 0xff9f },
  { 0, 0},

  { 0, 0}
};

struct recodetab
{
  unsigned short (*tab)[2];
  int flags;
};

#define RECODETAB_ALLOCED	1
#define RECODETAB_BUILTIN	2
#define RECODETAB_TRIED		4

static struct recodetab recodetabs[256];

void
InitBuiltinTabs()
{
  unsigned short (*p)[2];
  for (p = builtin_tabs; (*p)[0]; p++)
    {
      recodetabs[(*p)[0]].flags = RECODETAB_BUILTIN;
      recodetabs[(*p)[0]].tab = p + 1;
      p++;
      while((*p)[0])
	p++;
    }
}

static int
recode_char(c, to_utf, font)
int c, to_utf, font;
{
  int f;
  unsigned short (*p)[2];

  if (to_utf)
    {
      if (c < 256)
	return c;
      f = (c >> 8) & 0xff;
      c &= 0xff;
      /* map aliases to keep the table small */
      switch (f)
	{
	  case 'C':
	    f ^= ('C' ^ '5');
	    break;
	  case 'E':
	    f ^= ('E' ^ '6');
	    break;
	  case 'H':
	    f ^= ('H' ^ '7');
	    break;
	  default:
	    break;
	}
      p = recodetabs[f].tab;
      if (p == 0 && recodetabs[f].flags == 0)
	{
	  LoadFontTranslation(f, 0);
          p = recodetabs[f].tab;
	}
      if (p)
        for (; (*p)[0]; p++)
	  {
	    if ((p[0][0] & 0x8000) && (c <= (p[0][0] & 0x7fff)) && c >= p[-1][0])
	      return c - p[-1][0] + p[-1][1];
	    if ((*p)[0] == c)
	      return (*p)[1];
	  }
      return c & 0xff;	/* map to latin1 */
    }
  if (font == -1)
    {
      if (c < 256)
	return c;	/* latin1 */
      for (font = 32; font < 128; font++)
	{
	  p = recodetabs[font].tab;
	  if (p)
	    for (; (*p)[1]; p++)
	      {
		if ((p[0][0] & 0x8000) && c <= p[0][1] && c >= p[-1][1])
		  return (c - p[-1][1] + p[-1][0]) | (font << 8);
	        if ((*p)[1] == c)
		  return (*p)[0] | (font << 8);
	      }
	}
      return '?';
    }
  if (c < 128 && (font & 128) != 0)
    return c;
  if (font >= 32)
    {
      p = recodetabs[font].tab;
      if (p == 0 && recodetabs[font].flags == 0)
	{
	  LoadFontTranslation(font, 0);
          p = recodetabs[font].tab;
	}
      if (p)
	for (; (*p)[1]; p++)
	  {
	    if ((p[0][0] & 0x8000) && c <= p[0][1] && c >= p[-1][1])
	      return (c - p[-1][1] + p[-1][0]) | (font & 128 ? 0 : font << 8);
	    if ((*p)[1] == c)
	      return (*p)[0] | (font & 128 ? 0 : font << 8);
	  }
    }
  return -1;
}


#ifdef DW_CHARS
static int
recode_char_dw(c, c2p, to_utf, font)
int c, *c2p, to_utf, font;
{
  int f;
  unsigned short (*p)[2];

  if (to_utf)
    {
      f = (c >> 8) & 0xff;
      c = (c & 255) << 8 | (*c2p & 255);
      *c2p = 0xffff;
      p = recodetabs[f].tab;
      if (p == 0 && recodetabs[f].flags == 0)
	{
	  LoadFontTranslation(f, 0);
          p = recodetabs[f].tab;
	}
      if (p)
        for (; (*p)[0]; p++)
	  if ((*p)[0] == c)
	    {
#ifdef DW_CHARS
	      if (!utf8_isdouble((*p)[1]))
		*c2p = ' ';
#endif
	      return (*p)[1];
	    }
      return UCS_REPL_DW;
    }
  if (font == -1)
    {
      for (font = 0; font < 030; font++)
	{
	  p = recodetabs[font].tab;
	  if (p)
	    for (; (*p)[1]; p++)
	      if ((*p)[1] == c)
		{
		  *c2p = ((*p)[0] & 255) | font << 8 | 0x8000;
		  return ((*p)[0] >> 8) | font << 8;
		}
	}
      *c2p = '?';
      return '?';
    }
  if (font < 32)
    {
      p = recodetabs[font].tab;
      if (p == 0 && recodetabs[font].flags == 0)
	{
	  LoadFontTranslation(font, 0);
          p = recodetabs[font].tab;
	}
      if (p)
	for (; (*p)[1]; p++)
	  if ((*p)[1] == c)
	    {
	      *c2p = ((*p)[0] & 255) | font << 8 | 0x8000;
	      return ((*p)[0] >> 8) | font << 8;
	    }
    }
  return -1;
}
#endif

static int
recode_char_to_encoding(c, encoding)
int c, encoding;
{
  char *fp;
  int x;

  if (encoding == UTF8)
    return recode_char(c, 1, -1);
  if ((fp = encodings[encoding].fontlist) != 0)
    while(*fp)
      if ((x = recode_char(c, 0, (unsigned char)*fp++)) != -1)
        return x;
  if (encodings[encoding].deffont)
    if ((x = recode_char(c, 0, encodings[encoding].deffont)) != -1)
      return x;
  return recode_char(c, 0, -1);
}

#ifdef DW_CHARS
static int
recode_char_dw_to_encoding(c, c2p, encoding)
int c, *c2p, encoding;
{
  char *fp;
  int x;

  if (encoding == UTF8)
    return recode_char_dw(c, c2p, 1, -1);
  if ((fp = encodings[encoding].fontlist) != 0)
    while(*fp)
      if ((x = recode_char_dw(c, c2p, 0, (unsigned char)*fp++)) != -1)
        return x;
  if (encodings[encoding].deffont)
    if ((x = recode_char_dw(c, c2p, 0, encodings[encoding].deffont)) != -1)
      return x;
  return recode_char_dw(c, c2p, 0, -1);
}
#endif


struct mchar *
recode_mchar(mc, from, to)
struct mchar *mc;
int from, to;
{
  static struct mchar rmc;
  int c;

  debug3("recode_mchar %02x from %d to %d\n", mc->image, from, to);
  if (from == to || (from != UTF8 && to != UTF8))
    return mc;
  rmc = *mc;
  if (rmc.font == 0 && from != UTF8)
    rmc.font = encodings[from].deffont;
  if (rmc.font == 0)	/* latin1 is the same in unicode */
    return mc;
  c = rmc.image | (rmc.font << 8);
#ifdef DW_CHARS
  if (rmc.mbcs)
    {
      int c2 = rmc.mbcs;
      c = recode_char_dw_to_encoding(c, &c2, to);
      rmc.mbcs = c2;
    }
  else
#endif
    c = recode_char_to_encoding(c, to);
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

  if (from == to || (from != UTF8 && to != UTF8) || w == 0)
    return ml;
  if (ml->font == null && encodings[from].deffont == 0)
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

  debug("recode_mline: from\n");
  for (i = 0; i < w; i++)
    debug1("%c", "0123456789abcdef"[(ml->image[i] >> 4) & 15]);
  debug("\n");
  for (i = 0; i < w; i++)
    debug1("%c", "0123456789abcdef"[(ml->image[i]     ) & 15]);
  debug("\n");
  for (i = 0; i < w; i++)
    debug1("%c", "0123456789abcdef"[(ml->font[i] >> 4) & 15]);
  debug("\n");
  for (i = 0; i < w; i++)
    debug1("%c", "0123456789abcdef"[(ml->font[i]     ) & 15]);
  debug("\n");

  rl = rml + last;
  rl->attr = ml->attr;
#ifdef COLOR
  rl->color = ml->color;
# ifdef COLORS256
  rl->colorx = ml->colorx;
# endif
#endif
  for (i = 0; i < w; i++)
    {
      c = ml->image[i] | (ml->font[i] << 8);
      if (from != UTF8 && c < 256)
	c |= encodings[from].deffont << 8;
#ifdef DW_CHARS
      if ((from != UTF8 && (c & 0x1f00) != 0 && (c & 0xe000) == 0) || (from == UTF8 && utf8_isdouble(c)))
	{
	  if (i + 1 == w)
	    c = '?';
	  else
	    {
	      int c2;
	      i++;
	      c2 = ml->image[i] | (ml->font[i] << 8);
	      c = recode_char_dw_to_encoding(c, &c2, to);
	      rl->font[i - 1]  = c >> 8 & 255;
	      rl->image[i - 1] = c      & 255;
	      c = c2;
	    }
	}
      else
#endif
        c = recode_char_to_encoding(c, to);
      rl->image[i] = c & 255;
      rl->font[i] = c >> 8 & 255;
    }
  last ^= 1;
  debug("recode_mline: to\n");
  for (i = 0; i < w; i++)
    debug1("%c", "0123456789abcdef"[(rl->image[i] >> 4) & 15]);
  debug("\n");
  for (i = 0; i < w; i++)
    debug1("%c", "0123456789abcdef"[(rl->image[i]     ) & 15]);
  debug("\n");
  for (i = 0; i < w; i++)
    debug1("%c", "0123456789abcdef"[(rl->font[i] >> 4) & 15]);
  debug("\n");
  for (i = 0; i < w; i++)
    debug1("%c", "0123456789abcdef"[(rl->font[i]     ) & 15]);
  debug("\n");
  return rl;
}

struct combchar {
  unsigned short c1;
  unsigned short c2;
  unsigned short next;
  unsigned short prev;
};
struct combchar **combchars;

void
AddUtf8(c)
int c;
{
  ASSERT(D_encoding == UTF8);
  if (c >= 0xd800 && c < 0xe000 && combchars && combchars[c - 0xd800])
    {
      AddUtf8(combchars[c - 0xd800]->c1);
      c = combchars[c - 0xd800]->c2;
    }
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
ToUtf8_comb(p, c)
char *p;
int c;
{
  int l;

  if (c >= 0xd800 && c < 0xe000 && combchars && combchars[c - 0xd800])
    {
      l = ToUtf8_comb(p, combchars[c - 0xd800]->c1);
      return l + ToUtf8(p ? p + l : 0, combchars[c - 0xd800]->c2);
    }
  return ToUtf8(p, c);
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
WinSwitchEncoding(p, encoding)
struct win *p;
int encoding;
{
  int i, j, c;
  struct mline *ml;
  struct display *d;
  struct canvas *cv;
  struct layer *oldflayer;

  if ((p->w_encoding == UTF8) == (encoding == UTF8))
    {
      p->w_encoding = encoding;
      return;
    }
  oldflayer = flayer;
  for (d = displays; d; d = d->d_next)
    for (cv = d->d_cvlist; cv; cv = cv->c_next)
      if (p == Layer2Window(cv->c_layer))
	{
	  flayer = cv->c_layer;
	  while(flayer->l_next)
	    {
	      if (oldflayer == flayer)
		oldflayer = flayer->l_next;
	      ExitOverlayPage();
	    }
	}
  flayer = oldflayer;
  for (j = 0; j < p->w_height + p->w_histheight; j++)
    {
#ifdef COPY_PASTE
      ml = j < p->w_height ? &p->w_mlines[j] : &p->w_hlines[j - p->w_height];
#else
      ml = &p->w_mlines[j];
#endif
      if (ml->font == null && encodings[p->w_encoding].deffont == 0)
	continue;
      for (i = 0; i < p->w_width; i++)
	{
	  c = ml->image[i] | (ml->font[i] << 8);
	  if (p->w_encoding != UTF8 && c < 256)
	    c |= encodings[p->w_encoding].deffont << 8;
	  if (c < 256)
	    continue;
	  if (ml->font == null)
	    {
	      if ((ml->font = (unsigned char *)malloc(p->w_width + 1)) == 0)
		{
		  ml->font = null;
		  break;
		}
	      bzero(ml->font, p->w_width + 1);
	    }
#ifdef DW_CHARS
	  if ((p->w_encoding != UTF8 && (c & 0x1f00) != 0 && (c & 0xe000) == 0) || (p->w_encoding == UTF8 && utf8_isdouble(c)))
	    {
	      if (i + 1 == p->w_width)
		c = '?';
	      else
		{
		  int c2;
		  i++;
		  c2 = ml->image[i] | (ml->font[i] << 8);
		  c = recode_char_dw_to_encoding(c, &c2, encoding);
		  ml->font[i - 1]  = c >> 8 & 255;
		  ml->image[i - 1] = c      & 255;
		  c = c2;
		}
	    }
	  else
#endif
	    c = recode_char_to_encoding(c, encoding);
	  ml->image[i] = c & 255;
	  ml->font[i] = c >> 8 & 255;
	}
    }
  p->w_encoding = encoding;
  return;
}

#ifdef DW_CHARS
int
utf8_isdouble(c)
int c;
{
  return
    (c >= 0x1100 &&
     (c <= 0x115f ||                    /* Hangul Jamo init. consonants */
      (c >= 0x2e80 && c <= 0xa4cf && (c & ~0x0011) != 0x300a &&
       c != 0x303f) ||                  /* CJK ... Yi */
      (c >= 0xac00 && c <= 0xd7a3) || /* Hangul Syllables */
      (c >= 0xdf00 && c <= 0xdfff) || /* dw combining sequence */
      (c >= 0xf900 && c <= 0xfaff) || /* CJK Compatibility Ideographs */
      (c >= 0xfe30 && c <= 0xfe6f) || /* CJK Compatibility Forms */
      (c >= 0xff00 && c <= 0xff5f) || /* Fullwidth Forms */
      (c >= 0xffe0 && c <= 0xffe6) ||
      (c >= 0x20000 && c <= 0x2ffff)));
}
#endif

int
utf8_iscomb(c)
int c;
{
  /* taken from Markus Kuhn's wcwidth */
  static struct {
    unsigned short first;
    unsigned short last;
  } combining[] = {
    { 0x0300, 0x034F }, { 0x0360, 0x036F }, { 0x0483, 0x0486 },
    { 0x0488, 0x0489 }, { 0x0591, 0x05A1 }, { 0x05A3, 0x05B9 },
    { 0x05BB, 0x05BD }, { 0x05BF, 0x05BF }, { 0x05C1, 0x05C2 },
    { 0x05C4, 0x05C4 }, { 0x064B, 0x0655 }, { 0x0670, 0x0670 },
    { 0x06D6, 0x06E4 }, { 0x06E7, 0x06E8 }, { 0x06EA, 0x06ED },
    { 0x070F, 0x070F }, { 0x0711, 0x0711 }, { 0x0730, 0x074A },
    { 0x07A6, 0x07B0 }, { 0x0901, 0x0902 }, { 0x093C, 0x093C },
    { 0x0941, 0x0948 }, { 0x094D, 0x094D }, { 0x0951, 0x0954 },
    { 0x0962, 0x0963 }, { 0x0981, 0x0981 }, { 0x09BC, 0x09BC },
    { 0x09C1, 0x09C4 }, { 0x09CD, 0x09CD }, { 0x09E2, 0x09E3 },
    { 0x0A02, 0x0A02 }, { 0x0A3C, 0x0A3C }, { 0x0A41, 0x0A42 },
    { 0x0A47, 0x0A48 }, { 0x0A4B, 0x0A4D }, { 0x0A70, 0x0A71 },
    { 0x0A81, 0x0A82 }, { 0x0ABC, 0x0ABC }, { 0x0AC1, 0x0AC5 },
    { 0x0AC7, 0x0AC8 }, { 0x0ACD, 0x0ACD }, { 0x0B01, 0x0B01 },
    { 0x0B3C, 0x0B3C }, { 0x0B3F, 0x0B3F }, { 0x0B41, 0x0B43 },
    { 0x0B4D, 0x0B4D }, { 0x0B56, 0x0B56 }, { 0x0B82, 0x0B82 },
    { 0x0BC0, 0x0BC0 }, { 0x0BCD, 0x0BCD }, { 0x0C3E, 0x0C40 },
    { 0x0C46, 0x0C48 }, { 0x0C4A, 0x0C4D }, { 0x0C55, 0x0C56 },
    { 0x0CBF, 0x0CBF }, { 0x0CC6, 0x0CC6 }, { 0x0CCC, 0x0CCD },
    { 0x0D41, 0x0D43 }, { 0x0D4D, 0x0D4D }, { 0x0DCA, 0x0DCA },
    { 0x0DD2, 0x0DD4 }, { 0x0DD6, 0x0DD6 }, { 0x0E31, 0x0E31 },
    { 0x0E34, 0x0E3A }, { 0x0E47, 0x0E4E }, { 0x0EB1, 0x0EB1 },
    { 0x0EB4, 0x0EB9 }, { 0x0EBB, 0x0EBC }, { 0x0EC8, 0x0ECD },
    { 0x0F18, 0x0F19 }, { 0x0F35, 0x0F35 }, { 0x0F37, 0x0F37 },
    { 0x0F39, 0x0F39 }, { 0x0F71, 0x0F7E }, { 0x0F80, 0x0F84 },
    { 0x0F86, 0x0F87 }, { 0x0F90, 0x0F97 }, { 0x0F99, 0x0FBC },
    { 0x0FC6, 0x0FC6 }, { 0x102D, 0x1030 }, { 0x1032, 0x1032 },
    { 0x1036, 0x1037 }, { 0x1039, 0x1039 }, { 0x1058, 0x1059 },
    { 0x1160, 0x11FF }, { 0x1712, 0x1714 }, { 0x1732, 0x1734 },
    { 0x1752, 0x1753 }, { 0x1772, 0x1773 }, { 0x17B7, 0x17BD },
    { 0x17C6, 0x17C6 }, { 0x17C9, 0x17D3 }, { 0x180B, 0x180E },
    { 0x18A9, 0x18A9 }, { 0x200B, 0x200F }, { 0x202A, 0x202E },
    { 0x2060, 0x2063 }, { 0x206A, 0x206F }, { 0x20D0, 0x20EA },
    { 0x302A, 0x302F }, { 0x3099, 0x309A }, { 0xFB1E, 0xFB1E },
    { 0xFE00, 0xFE0F }, { 0xFE20, 0xFE23 }, { 0xFEFF, 0xFEFF },
    { 0xFFF9, 0xFFFB }
  };
  int mid, min = 0, max = sizeof(combining)/sizeof(*combining) - 1;

  if (c < 0x0300 || c > 0xfffb)
    return 0;
  while (max >= min)
    {
      mid = (min + max) / 2;
      if (c > combining[mid].last)
	min = mid + 1;
      else if (c < combining[mid].first)
	max = mid - 1;
      else
	return 1;
    }
  return 0;
}

static void
comb_tofront(root, i)
int root, i;
{
  for (;;)
    {
      debug1("bring to front: %x\n", i);
      combchars[combchars[i]->prev]->next = combchars[i]->next;
      combchars[combchars[i]->next]->prev = combchars[i]->prev;
      combchars[i]->next = combchars[root]->next;
      combchars[i]->prev = root;
      combchars[combchars[root]->next]->prev = i;
      combchars[root]->next = i;
      i = combchars[i]->c1;
      if (i < 0xd800 || i >= 0xe000)
	return;
      i -= 0xd800;
    }
}

void
utf8_handle_comb(c, mc)
int c;
struct mchar *mc;
{
  int root, i, c1;
  int isdouble;

  c1 = mc->image | (mc->font << 8);
  isdouble = c1 >= 0x1100 && utf8_isdouble(c1);
  if (!combchars)
    {
      combchars = (struct combchar **)malloc(sizeof(struct combchar *) * 0x802);
      if (!combchars)
	return;
      bzero((char *)combchars, sizeof(struct combchar *) * 0x802);
      combchars[0x800] = (struct combchar *)malloc(sizeof(struct combchar));
      combchars[0x801] = (struct combchar *)malloc(sizeof(struct combchar));
      if (!combchars[0x800] || !combchars[0x801])
	{
	  if (combchars[0x800])
	    free(combchars[0x800]);
	  if (combchars[0x801])
	    free(combchars[0x801]);
	  free(combchars);
	  return;
	}
      combchars[0x800]->c1 = 0x000;
      combchars[0x800]->c2 = 0x700;
      combchars[0x800]->next = 0x800;
      combchars[0x800]->prev = 0x800;
      combchars[0x801]->c1 = 0x700;
      combchars[0x801]->c2 = 0x800;
      combchars[0x801]->next = 0x801;
      combchars[0x801]->prev = 0x801;
    }
  root = isdouble ? 0x801 : 0x800;
  for (i = combchars[root]->c1; i < combchars[root]->c2; i++)
    {
      if (!combchars[i])
	break;
      if (combchars[i]->c1 == c1 && combchars[i]->c2 == c)
	break;
    }
  if (i == combchars[root]->c2)
    {
      /* full, recycle old entry */
      if (c1 >= 0xd800 && c1 < 0xe000)
        comb_tofront(root, c1 - 0xd800);
      i = combchars[root]->prev;
      if (c1 == i + 0xd800) 
	{
	  /* completely full, can't recycle */
	  debug("utf8_handle_comp: completely full!\n");
	  mc->image = '?';
	  mc->font  = 0;
	  return;
	}
      /* FIXME: delete old char from all buffers */
    }
  else if (!combchars[i])
    {
      combchars[i] = (struct combchar *)malloc(sizeof(struct combchar));
      if (!combchars[i])
	return;
      combchars[i]->prev = i;
      combchars[i]->next = i;
    }
  combchars[i]->c1 = c1;
  combchars[i]->c2 = c;
  mc->image = i & 0xff;
  mc->font  = (i >> 8) + 0xd8;
  debug3("combinig char %x %x -> %x\n", c1, c, i + 0xd800);
  comb_tofront(root, i);
}

#else /* !UTF8 */

void
WinSwitchEncoding(p, encoding)
struct win *p;
int encoding;
{
  p->w_encoding = encoding;
  return;
}

#endif /* UTF8 */

static int
encmatch(s1, s2)
char *s1;
char *s2;
{
  int c1, c2;
  do
    {
      c1 = (unsigned char)*s1;
      if (c1 >= 'A' && c1 <= 'Z')
	c1 += 'a' - 'A';
      if (!(c1 >= 'a' && c1 <= 'z') && !(c1 >= '0' && c1 <= '9'))
	{
	  s1++;
	  continue;
	}
      c2 = (unsigned char)*s2;
      if (c2 >= 'A' && c2 <= 'Z')
	c2 += 'a' - 'A';
      if (!(c2 >= 'a' && c2 <= 'z') && !(c2 >= '0' && c2 <= '9'))
	{
	  s2++;
	  continue;
	}
      if (c1 != c2)
	return 0;
      s1++;
      s2++;
    }
  while(c1);
  return 1;
}

int
FindEncoding(name)
char *name;
{
  int encoding;

  debug1("FindEncoding %s\n", name);
  if (name == 0 || *name == 0)
    return 0;
  if (encmatch(name, "euc"))
    name = "eucJP";
  if (encmatch(name, "off") || encmatch(name, "iso8859-1"))
    return 0;
#ifndef UTF8
  if (encmatch(name, "UTF-8"))
    return -1;
#endif
  for (encoding = 0; encoding < (int)(sizeof(encodings)/sizeof(*encodings)); encoding++)
    if (encmatch(name, encodings[encoding].name))
      {
#ifdef UTF8
	LoadFontTranslationsForEncoding(encoding);
#endif
        return encoding;
      }
  return -1;
}

char *
EncodingName(encoding)
int encoding;
{
  if (encoding >= (int)(sizeof(encodings)/sizeof(*encodings)))
    return 0;
  return encodings[encoding].name;
}

int
EncodingDefFont(encoding)
int encoding;
{
  return encodings[encoding].deffont;
}

void
ResetEncoding(p)
struct win *p;
{
  char *c;
  int encoding = p->w_encoding;

  c = encodings[encoding].charsets;
  if (c)
    SetCharsets(p, c);
#ifdef UTF8
  LoadFontTranslationsForEncoding(encoding);
#endif
  if (encodings[encoding].usegr)
    {
      p->w_gr = 2;
      p->w_FontE = encodings[encoding].charsets[1];
    }
  else
    p->w_FontE = 0;
  if (encodings[encoding].noc1)
    p->w_c1 = 0;
}

int
DecodeChar(c, encoding, statep)
int c;
int encoding;
int *statep;
{
  int t;

  debug2("Decoding char %02x for encoding %d\n", c, encoding);
#ifdef UTF8
  if (encoding == UTF8)
    return FromUtf8(c, statep);
#endif
  if (encoding == SJIS)
    {
      if (!*statep)
	{
	  if ((0x81 <= c && c <= 0x9f) || (0xe0 <= c && c <= 0xef))
	    {
	      *statep = c;
	      return -1;
	    }
	  return c | (KANA << 16);
	}
      t = c;
      c = *statep;
      *statep = 0;
      if (0x40 <= t && t <= 0xfc && t != 0x7f)
	{
	  if (c <= 0x9f) c = (c - 0x81) * 2 + 0x21; 
	  else c = (c - 0xc1) * 2 + 0x21; 
	  if (t <= 0x7e) t -= 0x1f;
	  else if (t <= 0x9e) t -= 0x20;
	  else t -= 0x7e, c++;
	  return (c << 8) | t | (KANJI << 16);
	}
      return t;
    }
  if (encoding == EUC_JP || encoding == EUC_KR || encoding == EUC_CN)
    {
      if (!*statep)
	{
	  if (c & 0x80)
	    {
	      *statep = c;
	      return -1;
	    }
	  return c;
	}
      t = c;
      c = *statep;
      *statep = 0;
      if (encoding == EUC_JP)
	{
	  if (c == 0x8e)
	    return t | (KANA << 16);
	  if (c == 0x8f)
	    {
	      *statep = t | (KANJI0212 << 8);
	      return -1;
	    }
	}
      c &= 0xff7f;
      t &= 0x7f;
      c = c << 8 | t;
      if (encoding == EUC_KR)
	return c | (3 << 16);
      if (encoding == EUC_CN)
	return c | (1 << 16);
      if (c & (KANJI0212 << 16))
        return c;
      else
        return c | (KANJI << 16);
    }
  if (encoding == BIG5 || encoding == GBK)
    {
      if (!*statep)
	{
	  if (c & 0x80)
	    {
	      if (encoding == GBK && c == 0x80)
		return 0xa4 | (('b'|0x80) << 16);
	      *statep = c;
	      return -1;
	    }
	  return c;
	}
      t = c;
      c = *statep;
      *statep = 0;
      c &= 0x7f;
      return c << 8 | t | (encoding == BIG5  ? 030 << 16 : 031 << 16);
    }
  return c | (encodings[encoding].deffont << 16);
}

int
EncodeChar(bp, c, encoding, fontp)
char *bp;
int c;
int encoding;
int *fontp;
{
  int t, f, l;

  debug2("Encoding char %02x for encoding %d\n", c, encoding);
  if (c == -1 && fontp)
    {
      if (*fontp == 0)
	return 0;
      if (bp)
	{
	  *bp++ = 033;
	  *bp++ = '(';
	  *bp++ = 'B';
	}
      return 3;
    }
  f = c >> 16;

#ifdef UTF8
  if (encoding == UTF8)
    {
      if (f)
	{
# ifdef DW_CHARS
	  if (is_dw_font(f))
	    {
	      int c2 = c & 0xff;
	      c = (c >> 8 & 0xff) | (f << 8);
	      c = recode_char_dw_to_encoding(c, &c2, encoding);
	    }
	  else
# endif
	    {
	      c = (c & 0xff) | (f << 8);
	      c = recode_char_to_encoding(c, encoding);
	    }
        }
      return ToUtf8(bp, c);
    }
  if ((c & 0xff00) && f == 0)	/* is_utf8? */
    {
# ifdef DW_CHARS
      if (utf8_isdouble(c))
	{
	  int c2 = 0xffff;
	  c = recode_char_dw_to_encoding(c, &c2, encoding);
	  c = (c << 8) | (c2 & 0xff);
	}
      else
# endif
	{
	  c = recode_char_to_encoding(c, encoding);
	  c = ((c & 0xff00) << 8) | (c & 0xff);
	}
      debug1("Encode: char mapped from utf8 to %x\n", c);
      f = c >> 16;
    }
#endif
  if (f & 0x80)		/* map special 96-fonts to latin1 */
    f = 0;

  if (encoding == SJIS)
    {
      if (f == KANA)
        c = (c & 0xff) | 0x80;
      else if (f == KANJI)
	{
	  if (!bp)
	    return 2;
	  t = c & 0xff;
	  c = (c >> 8) & 0xff;
	  t += (c & 1) ? ((t <= 0x5f) ? 0x1f : 0x20) : 0x7e; 
	  c = (c - 0x21) / 2 + ((c < 0x5f) ? 0x81 : 0xc1);
	  *bp++ = c;
	  *bp++ = t;
	  return 2;
	}
    }
  if (encoding == EUC)
    {
      if (f == KANA)
	{
	  if (bp)
	    {
	      *bp++ = 0x8e;
	      *bp++ = c;
	    }
	  return 2;
	}
      if (f == KANJI)
	{
	  if (bp)
	    {
	      *bp++ = (c >> 8) | 0x80;
	      *bp++ = c | 0x80;
	    }
	  return 2;
	}
      if (f == KANJI0212)
	{
	  if (bp)
	    {
	      *bp++ = 0x8f;
	      *bp++ = c >> 8;
	      *bp++ = c;
	    }
	  return 3;
	}
    }
  if ((encoding == EUC_KR && f == 3) || (encoding == EUC_CN && f == 1))
    {
      if (bp)
	{
	  *bp++ = (c >> 8) | 0x80;
	  *bp++ = c | 0x80;
	}
      return 2;
    }
  if ((encoding == BIG5 && f == 030) || (encoding == GBK && f == 031))
    {
      if (bp)
	{
	  *bp++ = (c >> 8) | 0x80;
	  *bp++ = c;
	}
      return 2;
    }
  if (encoding == GBK && f == 0 && c == 0xa4)
    c = 0x80;

  l = 0;
  if (fontp && f != *fontp)
    {
      *fontp = f;
      if (f && f < ' ')
	{
	  if (bp)
	   {
	     *bp++ = 033;
	     *bp++ = '$';
	     if (f > 2)
	       *bp++ = '(';
	     *bp++ = '@' + f;
	   }
	  l += f > 2 ? 4 : 3;
	}
      else if (f < 128)
	{
	  if (f == 0)
	    f = 'B';
	  if (bp)
	    {
	      *bp++ = 033;
	      *bp++ = '(';
	      *bp++ = f;
	    }
	  l += 3;
	}
    }
  if (c & 0xff00)
    {
      if (bp)
	*bp++ = c >> 8;
      l++;
    }
  if (bp)
    *bp++ = c;
  return l + 1;
}

int
CanEncodeFont(encoding, f)
int encoding, f;
{
  switch(encoding)
    {
#ifdef UTF8
    case UTF8:
      return 1;
#endif
    case SJIS:
      return f == KANJI || f == KANA;
    case EUC:
      return f == KANJI || f == KANA || f == KANJI0212;
    case EUC_KR:
      return f == 3;
    case EUC_CN:
      return f == 1;
    case BIG5:
      return f == 030;
    case GBK:
      return f == 031;
    default:
      break;
    }
  return 0;
}

#ifdef DW_CHARS
int
PrepareEncodedChar(c)
int c;
{
  int encoding;
  int t = 0;
  int f;

  encoding = D_encoding;
  f = D_rend.font;
  t = D_mbcs;
  if (encoding == SJIS)
    {
      if (f == KANA)
        return c | 0x80;
      else if (f == KANJI)
	{
	  t += (c & 1) ? ((t <= 0x5f) ? 0x1f : 0x20) : 0x7e; 
	  c = (c - 0x21) / 2 + ((c < 0x5f) ? 0x81 : 0xc1);
	  D_mbcs = t;
	}
      return c;
    }
  if (encoding == EUC)
    {
      if (f == KANA)
	{
	  AddChar(0x8e);
	  return c | 0x80;
	}
      if (f == KANJI)
	{
	  D_mbcs = t | 0x80;
	  return c | 0x80;
	}
      if (f == KANJI0212)
	{
	  AddChar(0x8f);
	  D_mbcs = t | 0x80;
	  return c | 0x80;
	}
    }
  if ((encoding == EUC_KR && f == 3) || (encoding == EUC_CN && f == 1))
    {
      D_mbcs = t | 0x80;
      return c | 0x80;
    }
  if ((encoding == BIG5 && f == 030) || (encoding == GBK && f == 031))
    return c | 0x80;
  return c;
}
#endif

int
RecodeBuf(fbuf, flen, fenc, tenc, tbuf)
unsigned char *fbuf;
int flen;
int fenc, tenc;
unsigned char *tbuf;
{
  int c, i, j;
  int decstate = 0, font = 0;

  for (i = j = 0; i < flen; i++)
    {
      c = fbuf[i];
      c = DecodeChar(c, fenc, &decstate);
      if (c == -2)
	i--;
      if (c < 0)
	continue;
      j += EncodeChar(tbuf ? (char *)tbuf + j : 0, c, tenc, &font);
    }
  j += EncodeChar(tbuf ? (char *)tbuf + j : 0, -1, tenc, &font);
  return j;
}

#ifdef UTF8
int
ContainsSpecialDeffont(ml, xs, xe, encoding)
struct mline *ml;
int xs, xe;
int encoding;
{
  unsigned char *f, *i;
  int c, x, dx;

  if (encoding == UTF8 || encodings[encoding].deffont == 0)
    return 0;
  i = ml->image + xs;
  f = ml->font + xs;
  dx = xe - xs + 1;
  while (dx-- > 0)
    {
      if (*f++)
	continue;
      c = *i++;
      x = recode_char_to_encoding(c | (encodings[encoding].deffont << 8), UTF8);
      if (c != x)
	{
	  debug2("ContainsSpecialDeffont: yes %02x != %02x\n", c, x);
	  return 1;
	}
    }
  debug("ContainsSpecialDeffont: no\n");
  return 0;
}


int
LoadFontTranslation(font, file)
int font;
char *file;
{
  char buf[1024], *myfile;
  FILE *f;
  int i;
  int fo;
  int x, u, c, ok;
  unsigned short (*p)[2], (*tab)[2];

  myfile = file;
  if (myfile == 0)
    {
      if (font == 0 || screenencodings == 0)
	return -1;
      if (strlen(screenencodings) > sizeof(buf) - 10)
	return -1;
      sprintf(buf, "%s/%02x", screenencodings, font & 0xff);
      myfile = buf;
    }
  debug1("LoadFontTranslation: trying %s\n", myfile);
  if ((f = secfopen(myfile, "r")) == 0)
    return -1;
  i = ok = 0;
  for (;;)
    {
      for(; i < 12; i++)
	if (getc(f) != "ScreenI2UTF8"[i])
	  break;
      if (getc(f) != 0)		/* format */
	break;
      fo = getc(f);		/* id */
      if (fo == EOF)
	break;
      if (font != -1 && font != fo)
	break;
      i = getc(f);
      x = getc(f);
      if (x == EOF)
	break;
      i = i << 8 | x;
      getc(f);
      while ((x = getc(f)) && x != EOF)
	getc(f); 	/* skip font name (padded to 2 bytes) */
      if ((p = malloc(sizeof(*p) * (i + 1))) == 0)
	break;
      tab = p;
      while(i > 0)
	{
	  x = getc(f);
	  x = x << 8 | getc(f);
	  u = getc(f);
	  c = getc(f);
	  u = u << 8 | c;
	  if (c == EOF)
	    break;
	  (*p)[0] = x;
	  (*p)[1] = u;
	  p++;
	  i--;
	}
      (*p)[0] = 0;
      (*p)[1] = 0;
      if (i || (tab[0][0] & 0x8000))
	{
	  free(tab);
	  break;
	}
      if (recodetabs[fo].tab && (recodetabs[fo].flags & RECODETAB_ALLOCED) != 0)
	free(recodetabs[fo].tab);
      recodetabs[fo].tab = tab;
      recodetabs[fo].flags = RECODETAB_ALLOCED;
      debug1("Successful load of recodetab %02x\n", fo);
      c = getc(f);
      if (c == EOF)
	{
	  ok = 1;
	  break;
	}
      if (c != 'S')
	break;
      i = 1;
    }
  fclose(f);
  if (font != -1 && file == 0 && recodetabs[font].flags == 0)
    recodetabs[font].flags = RECODETAB_TRIED;
  return ok ? 0 : -1;
}

void
LoadFontTranslationsForEncoding(encoding)
int encoding;
{
  char *c;
  int f;

  debug1("LoadFontTranslationsForEncoding: encoding %d\n", encoding);
  if ((c = encodings[encoding].fontlist) != 0)
    while ((f = (unsigned char)*c++) != 0)
      if (recodetabs[f].flags == 0)
	  LoadFontTranslation(f, 0);
  f = encodings[encoding].deffont;
  if (f > 0 && recodetabs[f].flags == 0)
    LoadFontTranslation(f, 0);
}

#endif /* UTF8 */

#else /* !ENCODINGS */

/* Simple version of EncodeChar to encode font changes for
 * copy/paste mode
 */
int
EncodeChar(bp, c, encoding, fontp)
char *bp;
int c;
int encoding;
int *fontp;
{
  int f, l;
  f = (c == -1) ? 0 : c >> 16;
  l = 0;
  if (fontp && f != *fontp)
    {
      *fontp = f;
      if (f && f < ' ')
	{
	  if (bp)
	   {
	     *bp++ = 033;
	     *bp++ = '$';
	     if (f > 2)
	       *bp++ = '(';
	     *bp++ = '@' + f;
	   }
	  l += f > 2 ? 4 : 3;
	}
      else if (f < 128)
	{
	  if (f == 0)
	    f = 'B';
	  if (bp)
	    {
	      *bp++ = 033;
	      *bp++ = '(';
	      *bp++ = f;
	    }
	  l += 3;
	}
    }
  if (c == -1)
    return l;
  if (c & 0xff00)
    {
      if (bp)
	*bp++ = c >> 8;
      l++;
    }
  if (bp)
    *bp++ = c;
  return l + 1;
}

#endif /* ENCODINGS */

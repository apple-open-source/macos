/* Copyright (c) 1993-2002
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
#include "mark.h"
#include "extern.h"
#include "braille.h"

extern struct display *display, *displays;

extern struct mline mline_blank, mline_null;
extern struct mchar mchar_blank, mchar_null;

extern struct layer *flayer;	/* sigh */
extern struct LayFuncs WinLf;
extern struct LayFuncs BlankLf;


static struct mline *mloff __P((struct mline *, int));

/*
 * Layer subsystem.
 *
 * ...here is all the clipping code... beware!
 *
 * XXX: add some speedup code!
 *
 */

static struct mline *
mloff(ml, off)
struct mline *ml;
int off;
{
  static struct mline mml;

  if (ml == 0)
    return 0;
  mml.image = ml->image + off;
  mml.attr  = ml->attr  + off;
#ifdef FONT
  mml.font  = ml->font  + off;
#endif
#ifdef COLOR
  mml.color = ml->color + off;
# ifdef COLORS256
  mml.colorx = ml->colorx + off;
# endif
#endif
  return &mml;
}

#ifdef UTF8
# define RECODE_MCHAR(mc) ((l->l_encoding == UTF8) != (D_encoding == UTF8) ? recode_mchar(mc, l->l_encoding, D_encoding) : (mc))
# define RECODE_MLINE(ml) ((l->l_encoding == UTF8) != (D_encoding == UTF8) ? recode_mline(ml, l->l_width, l->l_encoding, D_encoding) : (ml))
#else
# define RECODE_MCHAR(mc) (mc)
# define RECODE_MLINE(ml) (ml)
#endif


void
LGotoPos(l, x, y)
struct layer *l;
int x, y;
{
  struct canvas *cv;
  struct viewport *vp;
  int x2, y2;

#ifdef HAVE_BRAILLE
  if (bd.bd_refreshing)
    return;
#endif
  for (cv = l->l_cvlist; cv; cv = cv->c_lnext)
    {
      display = cv->c_display;
      if (D_blocked)
	continue;
      if (cv != D_forecv)
	continue;
      x2 = x + cv->c_xoff;
      y2 = y + cv->c_yoff;
      debug2("---LGotoPos %d %d\n", x2, y2);
      if (x2 < cv->c_xs)
	x2 = cv->c_xs;
      if (y2 < cv->c_ys)
	y2 = cv->c_ys;
      if (x2 > cv->c_xe)
	x2 = cv->c_xe;
      if (y2 > cv->c_ye)
	y2 = cv->c_ye;
      for (vp = cv->c_vplist; vp; vp = vp->v_next)
	{
	  if (x2 < vp->v_xs || x2 > vp->v_xe)
	    continue;
	  if (y2 < vp->v_ys || y2 > vp->v_ye)
	    continue;
	  GotoPos(x2, y2);
	  break;
	}
    }
}

void
LScrollH(l, n, y, xs, xe, bce, ol)
struct layer *l;
int n, y, xs, xe;
int bce;
struct mline *ol;
{
  struct canvas *cv;
  struct viewport *vp;
  int y2, xs2, xe2;

  if (n == 0)
    return;
  for (cv = l->l_cvlist; cv; cv = cv->c_lnext)
    for (vp = cv->c_vplist; vp; vp = vp->v_next)
      {
	y2 = y + vp->v_yoff;
	if (y2 < vp->v_ys || y2 > vp->v_ye)
	  continue;
	xs2 = xs + vp->v_xoff;
	xe2 = xe + vp->v_xoff;
	if (xs2 < vp->v_xs)
	  xs2 = vp->v_xs;
	if (xe2 > vp->v_xe)
	  xe2 = vp->v_xe;
	if (xs2 > xe2)
	  continue;
	display = cv->c_display;
	if (D_blocked)
	  continue;
	ScrollH(y2, xs2, xe2, n, bce, ol ? mloff(ol, -vp->v_xoff) : 0);
	if (xe2 - xs2 == xe - xs)
	  continue;
	if (n > 0)
	  {
	    xs2 = xe2 + 1 - n;
	    xe2 = xe + vp->v_xoff - n;
	  }
	else
	  {
	    xe2 = xs2 - 1 - n;
	    xs2 = xs + vp->v_xoff - n;
	  }
	if (xs2 < vp->v_xs)
	  xs2 = vp->v_xs;
	if (xe2 > vp->v_xe)
	  xe2 = vp->v_xe;
	if (xs2 <= xe2)
	  RefreshArea(xs2, y2, xe2, y2, 1);
      }
}

void
LScrollV(l, n, ys, ye, bce)
struct layer *l;
int n;
int ys, ye;
int bce;
{
  struct canvas *cv;
  struct viewport *vp;
  int ys2, ye2, xs2, xe2;
  if (n == 0)
    return;
  for (cv = l->l_cvlist; cv; cv = cv->c_lnext)
    for (vp = cv->c_vplist; vp; vp = vp->v_next)
      {
	xs2 = vp->v_xoff;
	xe2 = l->l_width - 1 + vp->v_xoff;
	ys2 = ys + vp->v_yoff;
	ye2 = ye + vp->v_yoff;
	if (xs2 < vp->v_xs)
	  xs2 = vp->v_xs;
	if (xe2 > vp->v_xe)
	  xe2 = vp->v_xe;
	if (ys2 < vp->v_ys)
	  ys2 = vp->v_ys;
	if (ye2 > vp->v_ye)
	  ye2 = vp->v_ye;
	if (ys2 > ye2 || xs2 > xe2)
	  continue;
	display = cv->c_display;
	if (D_blocked)
	  continue;
#if 0
	ScrollV(xs2, ys2, xe2, ye2, n, bce);
#else
	ScrollV(vp->v_xs, ys2, vp->v_xe, ye2, n, bce);
#endif
	debug2("LScrollV: %d %d", ys, ye);
	debug2(" -> %d %d\n", ys2, ye2);
	if (ye2 - ys2 == ye - ys)
	  continue;
	if (n > 0)
	  {
	    ys2 = ye2 + 1 - n;
	    ye2 = ye + vp->v_yoff - n;
	  }
	else
	  {
	    ye2 = ys2 - 1 - n;
	    ys2 = ys + vp->v_yoff - n;
	  }
	debug2("LScrollV: - %d %d\n", ys2, ye2);
	if (ys2 < vp->v_ys)
	  ys2 = vp->v_ys;
	if (ye2 > vp->v_ye)
	  ye2 = vp->v_ye;
	debug2("LScrollV: - %d %d\n", ys2, ye2);
	if (ys2 <= ye2)
	  RefreshArea(xs2, ys2, xe2, ye2, 1);
      }
}

void
LInsChar(l, c, x, y, ol)
struct layer *l;
struct mchar *c;
int x, y;
struct mline *ol;
{
  struct canvas *cv;
  struct viewport *vp;
  int xs2, xe2, y2, f;
  struct mchar *c2, cc;
  struct mline *rol;

  for (cv = l->l_cvlist; cv; cv = cv->c_lnext)
    for (vp = cv->c_vplist; vp; vp = vp->v_next)
      {
	y2 = y + vp->v_yoff;
	if (y2 < vp->v_ys || y2 > vp->v_ye)
	  continue;
	xs2 = x + vp->v_xoff;
	xe2 = l->l_width - 1 + vp->v_xoff;
	c2 = c;
	f = 0;
	if (xs2 < vp->v_xs)
	  {
	    xs2 = vp->v_xs;
	    c2 = &mchar_blank;
	    if (ol)
	      {
		int i;
		i = xs2 - vp->v_xoff - 1;
		if (i >= 0 && i < l->l_width)
		  {
		    copy_mline2mchar(&cc, ol, i);
		    c2 = &cc;
		  }
	      }
	    else
	      f = 1;
	  }
	if (xe2 > vp->v_xe)
	  xe2 = vp->v_xe;
	if (xs2 > xe2)
	  continue;
	display = cv->c_display;
	if (D_blocked)
	  continue;
        rol = RECODE_MLINE(ol);
	InsChar(RECODE_MCHAR(c2), xs2, xe2, y2, mloff(rol, -vp->v_xoff));
	if (f)
	  RefreshArea(xs2, y2, xs2, y2, 1);
      }
}

void
LPutChar(l, c, x, y)
struct layer *l;
struct mchar *c;
int x, y;
{
  struct canvas *cv;
  struct viewport *vp;
  int x2, y2;
#ifdef HAVE_BRAILLE
  if (bd.bd_refreshing)
    {
      BPutChar(l, c, x, y);
      return;
    }
#endif
  for (cv = l->l_cvlist; cv; cv = cv->c_lnext)
    {
      display = cv->c_display;
      if (D_blocked)
	continue;
      for (vp = cv->c_vplist; vp; vp = vp->v_next)
	{
	  y2 = y + vp->v_yoff;
	  if (y2 < vp->v_ys || y2 > vp->v_ye)
	    continue;
	  x2 = x + vp->v_xoff;
	  if (x2 < vp->v_xs || x2 > vp->v_xe)
	    continue;
	  PutChar(RECODE_MCHAR(c), x2, y2);
	  break;
	}
    }
}

void
LPutStr(l, s, n, r, x, y)
struct layer *l;
char *s;
int n;
struct mchar *r;
int x, y;
{
  struct canvas *cv;
  struct viewport *vp;
  char *s2;
  int xs2, xe2, y2;

  if (x + n > l->l_width)
    n = l->l_width - x;
#ifdef HAVE_BRAILLE
  if (bd.bd_refreshing)
    {
      BPutStr(l, s, n, r, x, y);
      return;
    }
#endif
  for (cv = l->l_cvlist; cv; cv = cv->c_lnext)
    for (vp = cv->c_vplist; vp; vp = vp->v_next)
      {
	y2 = y + vp->v_yoff;
	if (y2 < vp->v_ys || y2 > vp->v_ye)
	  continue;
	xs2 = x + vp->v_xoff;
	xe2 = xs2 + n - 1;
	if (xs2 < vp->v_xs)
	  xs2 = vp->v_xs;
	if (xe2 > vp->v_xe)
	  xe2 = vp->v_xe;
	if (xs2 > xe2)
	  continue;
	display = cv->c_display;
        if (D_blocked)
	  continue;
	GotoPos(xs2, y2);
	SetRendition(r);
	s2 = s + xs2 - x - vp->v_xoff;
#ifdef UTF8
	if (D_encoding == UTF8 && l->l_encoding != UTF8 && (r->font || l->l_encoding))
	  {
	    struct mchar mc;
	    mc = *r;
	    while (xs2 <= xe2)
	      {
		mc.image = *s2++;
	        PutChar(RECODE_MCHAR(&mc), xs2++, y2);
	      }
	    continue;
	  }
#endif
	while (xs2++ <= xe2)
	  PUTCHARLP(*s2++);
      }
}

void
LPutWinMsg(l, s, n, r, x, y)
struct layer *l;
char *s;
int n;
struct mchar *r;
int x, y;
{
  struct canvas *cv;
  struct viewport *vp;
  char *s2;
  int xs2, xe2, y2, len, len2;
  struct mchar or;

  if (x + n > l->l_width)
    n = l->l_width - x;
#ifdef HAVE_BRAILLE
  if (bd.bd_refreshing)
    {
      BPutStr(l, s, n, r, x, y);
      return;
    }
#endif
  len = strlen(s);
  if (len > n)
    len = n;
  for (cv = l->l_cvlist; cv; cv = cv->c_lnext)
    for (vp = cv->c_vplist; vp; vp = vp->v_next)
      {
	y2 = y + vp->v_yoff;
	if (y2 < vp->v_ys || y2 > vp->v_ye)
	  continue;
	xs2 = x + vp->v_xoff;
	xe2 = xs2 + n - 1;
	if (xs2 < vp->v_xs)
	  xs2 = vp->v_xs;
	if (xe2 > vp->v_xe)
	  xe2 = vp->v_xe;
	if (xs2 > xe2)
	  continue;
	display = cv->c_display;
        if (D_blocked)
	  continue;
	GotoPos(xs2, y2);
	SetRendition(r);
	len2 = xe2 - (x + vp->v_xoff) + 1;
	if (len2 > len)
	  len2 = len;
	if (!PutWinMsg(s, xs2 - x - vp->v_xoff, len2))
	  {
	    s2 = s + xs2 - x - vp->v_xoff;
	    while (len2-- > 0)
	      {
	        PUTCHARLP(*s2++);
		xs2++;
	      }
	  }
        else
	  xs2 = x + vp->v_xoff + len2;
	if (xs2 < vp->v_xs)
	  xs2 = vp->v_xs;
	or = D_rend;
	GotoPos(xs2, y2);
	SetRendition(&or);
	while (xs2++ <= xe2)
	  PUTCHARLP(' ');
      }
}

void
LClearLine(l, y, xs, xe, bce, ol)
struct layer *l;
#ifdef __APPLE__
int y;
#endif
int xs, xe, bce;
struct mline *ol;
{
  struct canvas *cv;
  struct viewport *vp;
  int y2, xs2, xe2;

  /* check for magic margin condition */
  if (xs >= l->l_width)
    xs = l->l_width - 1;
  if (xe >= l->l_width)
    xe = l->l_width - 1;
  for (cv = l->l_cvlist; cv; cv = cv->c_lnext)
    for (vp = cv->c_vplist; vp; vp = vp->v_next)
      {
	xs2 = xs + vp->v_xoff;
	xe2 = xe + vp->v_xoff;
	y2  = y + vp->v_yoff;
	if (y2 < vp->v_ys || y2 > vp->v_ye)
	  continue;
	if (xs2 < vp->v_xs)
	  xs2 = vp->v_xs;
	if (xe2 > vp->v_xe)
	  xe2 = vp->v_xe;
	if (xs2 > xe2)
	  continue;
	display = cv->c_display;
        if (D_blocked)
	  continue;
	ClearLine(ol ? mloff(RECODE_MLINE(ol), -vp->v_xoff) : (struct mline *)0, y2, xs2, xe2, bce);
      }
}

void
LClearArea(l, xs, ys, xe, ye, bce, uself)
struct layer *l;
int xs, ys, xe, ye;
int bce;
int uself;
{
  struct canvas *cv;
  struct viewport *vp;
  int xs2, ys2, xe2, ye2;
#ifdef HAVE_BRAILLE
  if (bd.bd_refreshing)
    return;
#endif
  /* check for magic margin condition */
  if (xs >= l->l_width)
    xs = l->l_width - 1;
  if (xe >= l->l_width)
    xe = l->l_width - 1;
  for (cv = l->l_cvlist; cv; cv = cv->c_lnext)
    {
      display = cv->c_display;
      if (D_blocked)
	continue;
      for (vp = cv->c_vplist; vp; vp = vp->v_next)
	{
	  xs2 = xs + vp->v_xoff;
	  xe2 = xe + vp->v_xoff;
	  ys2 = ys + vp->v_yoff;
	  ye2 = ye + vp->v_yoff;
	  if (xs2 < vp->v_xs)
	    xs2 = vp->v_xs;
	  if (xe2 > vp->v_xe)
	    xe2 = vp->v_xe;
	  if (xs2 > vp->v_xe)
	    ys2++;
	  if (xe2 < vp->v_xs)
	    ye2--;
	  if (ys2 < vp->v_ys)
	    ys2 = vp->v_ys;
	  if (ye2 > vp->v_ye)
	    ye2 = vp->v_ye;
	  if (ys2 > ye2)
	    continue;
#if 0
	  xcs = vp->v_xoff;
	  xce = l->l_width - 1 + vp->v_xoff;
	  if (xcs < vp->v_xs)
	    xcs = vp->v_xs;
	  if (xce > vp->v_xe)
	    xce = vp->v_xe;
	  if (xcs > xce)
	    continue;
	  if (ys2 != ys + vp->v_yoff)
	    xs2 = xcs;
	  if (ye2 != ye + vp->v_yoff)
	    xe2 = xce;
	  display = cv->c_display;
	  ClearArea(xs2, ys2, xcs, xce, xe2, ye2, bce, uself);
#else
	  if (xs == 0 || ys2 != ys + vp->v_yoff)
	    xs2 = vp->v_xs;
	  if (xe == l->l_width - 1 || ye2 != ye + vp->v_yoff)
	    xe2 = vp->v_xe;
	  display = cv->c_display;
	  ClearArea(xs2, ys2, vp->v_xs, vp->v_xe, xe2, ye2, bce, uself);
#endif
	}
    }
}

void
LCDisplayLine(l, ml, y, xs, xe, isblank)
struct layer *l;
struct mline *ml;
int y, xs, xe;
int isblank;
{
  struct canvas *cv;
  struct viewport *vp;
  int xs2, xe2, y2;
#ifdef HAVE_BRAILLE
  if (bd.bd_refreshing)
    {
      BCDisplayLine(l, ml, y, xs, xe, isblank);
      return;
    }
#endif
  for (cv = l->l_cvlist; cv; cv = cv->c_lnext)
    {
      display = cv->c_display;
      if (D_blocked)
	continue;
      for (vp = cv->c_vplist; vp; vp = vp->v_next)
	{
	  xs2 = xs + vp->v_xoff;
	  xe2 = xe + vp->v_xoff;
	  y2  = y + vp->v_yoff;
	  if (y2 < vp->v_ys || y2 > vp->v_ye)
	    continue;
	  if (xs2 < vp->v_xs)
	    xs2 = vp->v_xs;
	  if (xe2 > vp->v_xe)
	    xe2 = vp->v_xe;
	  if (xs2 > xe2)
	    continue;
	  display = cv->c_display;
	  debug3("LCDisplayLine: DisplayLine %d, %d-%d", y2, xs2, xe2);
	  debug1("  mloff = %d\n", -vp->v_xoff);
	  DisplayLine(isblank ? &mline_blank : &mline_null, mloff(RECODE_MLINE(ml), -vp->v_xoff), y2, xs2, xe2);
	}
    }
}

void
LCDisplayLineWrap(l, ml, y, from, to, isblank)
struct layer *l;
struct mline *ml;
int y, from, to;
int isblank;
{
  struct mchar nc;
  copy_mline2mchar(&nc, ml, 0);
#ifdef DW_CHARS
  if (dw_left(ml, 0, l->l_encoding))
    {
      nc.mbcs = ml->image[1];
      from++;
    }
#endif
  LWrapChar(l, &nc, y - 1, -1, -1, 0);
  from++;
  if (from <= to)
    LCDisplayLine(l, ml, y, from, to, isblank);
}

void
LSetRendition(l, r)
struct layer *l;
struct mchar *r;
{
  struct canvas *cv;

  for (cv = l->l_cvlist; cv; cv = cv->c_lnext)
    {
      display = cv->c_display;
      if (D_blocked)
	continue;
      SetRendition(r);
    }
}

void
LWrapChar(l, c, y, top, bot, ins)
struct layer *l;
struct mchar *c;
int y, top, bot;
int ins;
{
  struct canvas *cv, *cvlist, *cvlnext;
  struct viewport *vp, *evp, **vpp;
  int yy, y2, yy2, top2, bot2;
  int bce;

#ifdef COLOR
  bce = rend_getbg(c);
#else
  bce = 0;
#endif
  if (y != bot)
    {
      /* simple case: no scrolling */

      /* cursor after wrapping */
      yy = y == l->l_height - 1 ? y : y + 1;

      for (cv = l->l_cvlist; cv; cv = cv->c_lnext)
	{
	  y2 = 0;       /* gcc -Wall */
	  display = cv->c_display;
	  if (D_blocked)
	    continue;
	  /* find the viewport of the wrapped character */
	  for (vp = cv->c_vplist; vp; vp = vp->v_next)
	    {
	      y2 =  y + vp->v_yoff;
	      yy2 = yy + vp->v_yoff;
	      if (yy2 >= vp->v_ys && yy2 <= vp->v_ye && vp->v_xoff >= vp->v_xs && vp->v_xoff <= vp->v_xe)
		break;
	    }
	  if (vp == 0)
	    continue;	/* nothing to do, character not visible */
	  /* find the viewport of the character at the end of the line*/
	  for (evp = cv->c_vplist; evp; evp = evp->v_next)
	    if (y2 >= evp->v_ys && y2 <= evp->v_ye && evp->v_xoff + l->l_width - 1 >= evp->v_xs && evp->v_xoff + l->l_width - 1 <= evp->v_xe)
	      break;	/* gotcha! */
	  if (evp == 0 || (ins && vp->v_xoff + l->l_width - 1 > vp->v_ye))
	    {
	      /* no wrapping possible */
	      debug("LWrap: can't wrap!\n");
	      cvlist = l->l_cvlist;
	      cvlnext = cv->c_lnext;
	      l->l_cvlist = cv;
	      cv->c_lnext = 0;
	      if (ins)
		LInsChar(l, c, 0, yy, 0);
	      else
	        LPutChar(l, c, 0, yy);
	      l->l_cvlist = cvlist;
	      cv->c_lnext = cvlnext;
	    }
	  else
	    {
	      WrapChar(RECODE_MCHAR(c), vp->v_xoff + l->l_width, y2, vp->v_xoff, -1, vp->v_xoff + l->l_width - 1, -1, ins);
	    }
	}
    }
  else
    {
      /* hard case: scroll up*/

      for (cv = l->l_cvlist; cv; cv = cv->c_lnext)
	{
	  display = cv->c_display;
	  if (D_blocked)
	    continue;
	  /* search for wrap viewport */
	  for (vpp = &cv->c_vplist; (vp = *vpp); vpp = &vp->v_next)
	    {
	      yy2 = bot + vp->v_yoff;
	      if (yy2 >= vp->v_ys && yy2 <= vp->v_ye && vp->v_xoff >= vp->v_xs && vp->v_xoff + l->l_width - 1 <= vp->v_xe)
		break;
	    }

	  if (vp)
	    {
	      /* great, can use Wrap on the vp */
	      /* temporarily remove vp from cvlist */
	      *vpp = vp->v_next;
	    }
	  if (cv->c_vplist)
	    {
	      /* scroll all viewports != vp */
	      cvlist = l->l_cvlist;
	      cvlnext = cv->c_lnext;
	      l->l_cvlist = cv;
	      cv->c_lnext = 0;
	      LScrollV(l, 1, top, bot, bce);
	      if (!vp)
		{
		  if (ins)
		    LInsChar(l, c, 0, bot, 0);
		  else
		    LPutChar(l, c, 0, bot);
		}
	      l->l_cvlist = cvlist;
	      cv->c_lnext = cvlnext;
	    }
	  if (vp)
	    {
	      /* add vp back to cvlist */
	      *vpp = vp;
	      top2 = top + vp->v_yoff;
	      bot2 = bot + vp->v_yoff;
	      if (top2 < vp->v_ys)
		top2 = vp->v_ys;
	      WrapChar(RECODE_MCHAR(c), vp->v_xoff + l->l_width, bot2, vp->v_xoff, top2, vp->v_xoff + l->l_width - 1, bot2, ins);
	    }
	}
    }
}


void
LCursorVisibility(l, vis)
struct layer *l;
int vis;
{
  struct canvas *cv;
  for (cv = l->l_cvlist; cv; cv = cv->c_lnext)
    {
      display = cv->c_display;
      if (D_blocked)
	continue;
      if (cv != D_forecv)
	continue;
      CursorVisibility(vis);
    }
}

void
LSetFlow(l, flow)
struct layer *l;
int flow;
{
  struct canvas *cv;
  for (cv = l->l_cvlist; cv; cv = cv->c_lnext)
    {
      display = cv->c_display;
      if (cv != D_forecv)
	continue;
      SetFlow(flow);
    }
}

void
LKeypadMode(l, on)
struct layer *l;
int on;
{
  struct canvas *cv;
  for (cv = l->l_cvlist; cv; cv = cv->c_lnext)
    {
      display = cv->c_display;
      if (D_blocked)
	continue;
      if (cv != D_forecv)
	continue;
      KeypadMode(on);
    }
}

void
LCursorkeysMode(l, on)
struct layer *l;
int on;
{
  struct canvas *cv;
  for (cv = l->l_cvlist; cv; cv = cv->c_lnext)
    {
      display = cv->c_display;
      if (D_blocked)
	continue;
      if (cv != D_forecv)
	continue;
      CursorkeysMode(on);
    }
}

void
LMouseMode(l, on)
struct layer *l;
int on;
{
  struct canvas *cv;
  for (cv = l->l_cvlist; cv; cv = cv->c_lnext)
    {
      display = cv->c_display;
      if (D_blocked)
	continue;
      if (cv != D_forecv)
	continue;
      MouseMode(on);
    }
}


/*******************************************************************/

void
LClearAll(l, uself)
struct layer *l;
int uself;
{
  LClearArea(l, 0, 0, l->l_width - 1, l->l_height - 1, 0, uself);
}

void
LRefreshAll(l, isblank)
struct layer *l;
int isblank;
{
  struct layer *oldflayer;
  int y;

  debug1("LRefreshAll isblank=%d\n", isblank);
  oldflayer = flayer;
  flayer = l;
  if (!isblank)
    LClearArea(l, 0, 0, l->l_width - 1, l->l_height - 1, 0, 0);
  /* signal full refresh */
  LayRedisplayLine(-1, -1, -1, 1);
  for (y = 0; y < l->l_height; y++)
    LayRedisplayLine(y, 0, l->l_width - 1, 1);
  flayer = oldflayer;
}


void
KillLayerChain(lay)
struct layer *lay;
{
  struct canvas *cv, *ncv;
  struct layer *l, *oldflayer;

  oldflayer = flayer;
  debug1("KillLayerChain %#x\n", lay);
  for (l = lay; l; l = l->l_next)
    {
      if (l->l_layfn == &WinLf || l->l_layfn == &BlankLf)
	break;
      debug1("- killing %#x\n", l);
      if (oldflayer == l)
	oldflayer = 0;
      for (cv = l->l_cvlist; cv; cv = ncv)
	{
	  ncv = cv->c_lnext;
	  cv->c_layer = 0;
	  cv->c_lnext = 0;
	}
    }
  flayer = lay;
  while (flayer != l)
    ExitOverlayPage();
  flayer = oldflayer;
}


/*******************************************************************/
/*******************************************************************/

/*
 *  Layer creation / removal
 */

int
InitOverlayPage(datasize, lf, block)
int datasize;
struct LayFuncs *lf;
int block;
{
  char *data;
  struct layer *newlay;
  struct canvas *cv, *cvp, **cvpp;
  struct win *p;

  ASSERT(flayer);

  cv = 0;
  if (display && D_forecv->c_layer == flayer)
    cv = D_forecv;	/* work only on this cv! */

  if ((newlay = (struct layer *)calloc(1, sizeof(struct layer))) == 0)
    {
      Msg(0, "No memory for layer struct");
      return -1;
    }
  debug2("Entering new layer on top of %#x: %#x\n", (unsigned int)flayer, newlay);
  data = 0;
  if (datasize)
    {
      if ((data = malloc(datasize)) == 0)
	{
	  free((char *)newlay);
	  Msg(0, "No memory for layer data");
	  return -1;
	}
      bzero(data, datasize);
    }

  p = Layer2Window(flayer);

  if (p && (p->w_savelayer == flayer || (block && flayer->l_next == 0)))
    {
      if (p->w_savelayer && p->w_savelayer != flayer && p->w_savelayer->l_cvlist == 0)
	KillLayerChain(p->w_savelayer);
      p->w_savelayer = newlay;
    }

  if (cv && flayer->l_next == 0 && !block)
    {
      struct display *olddisplay = display;
      display = cv->c_display;
      RemoveStatus();
      display = olddisplay;

      /* new branch -> just get canvas vps */
      for (cvpp = &flayer->l_cvlist; (cvp = *cvpp); cvpp = &cvp->c_lnext)
	if (cvp == cv)
	  break;
      ASSERT(cvp);
      *cvpp = cv->c_lnext;
      newlay->l_cvlist = cv;
      cv->c_lnext = 0;
      cv->c_layer = newlay;
    }
  else
    {
      LAY_DISPLAYS(flayer, RemoveStatus());
      if (block)
        debug("layer is blocking\n");
      if (block && flayer->l_layfn == &WinLf)
	{
          debug("...and is first, so window gets blocked\n");
	  ASSERT(p->w_blocked == 0);
	  p->w_blocked++;
	  newlay->l_blocking = 1;
	}
      /* change all canvases */
      newlay->l_cvlist = flayer->l_cvlist;
      for (cvp = newlay->l_cvlist; cvp; cvp = cvp->c_lnext)
	cvp->c_layer = newlay;
      flayer->l_cvlist = 0;
    }
  newlay->l_width = flayer->l_width;
  newlay->l_height = flayer->l_height;
  newlay->l_encoding = 0;
  newlay->l_layfn = lf;
  newlay->l_data = data;
  newlay->l_next = flayer;
  newlay->l_bottom = flayer->l_bottom;
  flayer = newlay;
  LayRestore();
  return 0;
}

void
ExitOverlayPage()
{
  struct layer *oldlay;
  struct win *p;
  int doredisplay = 0;
  struct canvas *cv, *ocv;

  ASSERT(flayer);
  debug1("Exiting layer %#x\n", (unsigned int)flayer);
  oldlay = flayer;
  if (oldlay->l_data)
    free(oldlay->l_data);

  p = Layer2Window(flayer);

  flayer = oldlay->l_next;
  if (flayer->l_layfn == &WinLf)
    {
      if (oldlay->l_blocking)
	{
	  ASSERT(p->w_blocked > 0);
	  p->w_blocked--;
          debug1("layer was blocking, -> w_blocked now %d\n", p->w_blocked);
	}
      /* don't warp dead layers: check cvlist */
      if (p->w_blocked && p->w_savelayer && p->w_savelayer != flayer && oldlay->l_cvlist)
	{
          debug("warping to top of blocking chain!\n");
	  /* warp ourself into savelayer */
	  flayer = p->w_savelayer;
	  doredisplay = 1;
	}
    }
  if (p && p->w_savelayer == oldlay)
    p->w_savelayer = flayer;
#ifdef COPY_PASTE
  if (p && oldlay == p->w_paster.pa_pastelayer)
    p->w_paster.pa_pastelayer = 0;
#endif

  /* add all canvases back into next layer's canvas list */
  for (ocv = 0, cv = oldlay->l_cvlist; cv; cv = cv->c_lnext)
    {
      cv->c_layer = flayer;
      ocv = cv;
    }
  if (ocv)
    {
      cv = flayer->l_cvlist;
      ocv->c_lnext = 0;
      flayer->l_cvlist = oldlay->l_cvlist;
      /* redisplay only the warped cvs */
      if (doredisplay)
	LRefreshAll(flayer, 0);
      ocv->c_lnext = cv;
    }
  oldlay->l_cvlist = 0;
  free((char *)oldlay);
  LayRestore();
  LaySetCursor();
}

void
/*VARARGS2*/
#if defined(USEVARARGS) && defined(__STDC__)
LMsg(int err, char *fmt, VA_DOTS)
#else
LMsg(err, fmt, VA_DOTS)
int err;
char *fmt;
VA_DECL
#endif
{
  VA_LIST(ap)
  char buf[MAXPATHLEN*2];
  char *p = buf;
  struct canvas *cv;

  VA_START(ap, fmt);
  fmt = DoNLS(fmt);
  (void)vsnprintf(p, sizeof(buf) - 100, fmt, VA_ARGS(ap));
  VA_END(ap);
  if (err)
    {
      p += strlen(p);
      *p++ = ':';
      *p++ = ' ';
      strncpy(p, strerror(err), buf + sizeof(buf) - p - 1);
      buf[sizeof(buf) - 1] = 0;
    }
  debug2("LMsg('%s') (%#x);\n", buf, (unsigned int)flayer);
  for (display = displays; display; display = display->d_next)
    {
      for (cv = D_cvlist; cv; cv = cv->c_next)
	if (cv->c_layer == flayer)
	  break;
      if (cv == 0)
	continue;
      MakeStatus(buf);
    }
}


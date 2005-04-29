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
#include <signal.h>
#ifndef sun
#include <sys/ioctl.h>
#endif

#ifdef ISC
# include <sys/tty.h>
# include <sys/sioctl.h>
# include <sys/pty.h>
#endif

#include "config.h"
#include "screen.h"
#include "extern.h"

static void CheckMaxSize __P((int));
static void FreeMline  __P((struct mline *));
static int  AllocMline __P((struct mline *ml, int));
static void MakeBlankLine __P((unsigned char *, int));
static void kaablamm __P((void));
static int  BcopyMline __P((struct mline *, int, struct mline *, int, int, int));
static void SwapAltScreen __P((struct win *));

extern struct layer *flayer;
extern struct display *display, *displays;
extern unsigned char *blank, *null;
extern struct mline mline_blank, mline_null, mline_old;
extern struct win *windows;
extern int Z0width, Z1width;
extern int captionalways;

#if defined(TIOCGWINSZ) || defined(TIOCSWINSZ)
struct winsize glwz;
#endif

static struct mline mline_zero = {
 (unsigned char *)0,
 (unsigned char *)0
#ifdef FONT
 ,(unsigned char *)0
#endif
#ifdef COLOR
 ,(unsigned char *)0
# ifdef COLORS256
 ,(unsigned char *)0
# endif
#endif
};

/*
 * ChangeFlag:   0: try to modify no window
 *               1: modify fore (and try to modify no other) + redisplay
 *               2: modify all windows
 *
 * Note: Activate() is only called if change_flag == 1
 *       i.e. on a WINCH event
 */

void
CheckScreenSize(change_flag)
int change_flag;
{
  int wi, he;

  if (display == 0)
    {
      debug("CheckScreenSize: No display -> no check.\n");
      return;
    }
#ifdef TIOCGWINSZ
  if (ioctl(D_userfd, TIOCGWINSZ, (char *)&glwz) != 0)
    {
      debug2("CheckScreenSize: ioctl(%d, TIOCGWINSZ) errno %d\n", D_userfd, errno);
      wi = D_CO;
      he = D_LI;
    }
  else
    {
      wi = glwz.ws_col;
      he = glwz.ws_row;
      if (wi == 0)
        wi = D_CO;
      if (he == 0)
        he = D_LI;
    }
#else
  wi = D_CO;
  he = D_LI;
#endif
  
  debug2("CheckScreenSize: screen is (%d,%d)\n", wi, he);

#if 0 /* XXX: Fixme */
  if (change_flag == 2)
    {
      debug("Trying to adapt all windows (-A)\n");
      for (p = windows; p; p = p->w_next)
	if (p->w_display == 0 || p->w_display == display)
          ChangeWindowSize(p, wi, he, p->w_histheight);
    }
#endif
  if (D_width == wi && D_height == he)
    {
      debug("CheckScreenSize: No change -> return.\n");
      return;
    }
#ifdef BLANKER_PRG
  KillBlanker();
#endif
  ResetIdle();
  ChangeScreenSize(wi, he, change_flag);
/* XXX Redisplay logic */
#if 0
  if (change_flag == 1)
    Redisplay(D_fore ? D_fore->w_norefresh : 0);
#endif
}

void
ChangeScreenSize(wi, he, change_fore)
int wi, he;
int change_fore;
{
  struct win *p;
  struct canvas *cv, **cvpp;
  int wwi;
  int y, h, hn;

  debug2("ChangeScreenSize from (%d,%d) ", D_width, D_height);
  debug3("to (%d,%d) (change_fore: %d)\n",wi, he, change_fore);

  /*
   *  STRATEGY: keep the ratios.
   *  if canvas doesn't fit anymore, throw it off.
   *  (ATTENTION: cvlist must be sorted!)
   */
  y = 0;
  h = he;
  if (D_has_hstatus == HSTATUS_LASTLINE)
    {
      if (h > 1)
        h--;
      else
        D_has_hstatus = 0;	/* sorry */
    }
  for (cvpp = &D_cvlist; (cv = *cvpp); )
    {
      if (h < 2 && cvpp != &D_cvlist)
        {
          /* kill canvas */
	  SetCanvasWindow(cv, 0);
          *cvpp = cv->c_next;
	  free(cv);
	  if (D_forecv == cv)
	    D_forecv = 0;
          continue;
        }
      hn = (cv->c_ye - cv->c_ys + 1) * he / D_height;
      if (hn == 0)
        hn = 1;
      if (hn + 2 >= h || cv->c_next == 0)
        hn = h - 1;
      if ((!captionalways && cv == D_cvlist && h - hn < 2) || hn == 0)
        hn = h;
      ASSERT(hn > 0);
      cv->c_xs = 0;
      cv->c_xe = wi - 1;
      cv->c_ys = y;
      cv->c_ye = y + hn - 1;

      cv->c_xoff = cv->c_xs;
      cv->c_yoff = cv->c_ys;

      y += hn + 1;
      h -= hn + 1;
      cvpp = &cv->c_next;
    }
  RethinkDisplayViewports();
  if (D_forecv == 0)
    D_forecv = D_cvlist;
  if (D_forecv)
    D_fore = Layer2Window(D_forecv->c_layer);

  D_width = wi;
  D_height = he;

  CheckMaxSize(wi);
  if (D_CWS)
    {
      D_defwidth = D_CO;
      D_defheight = D_LI;
    }
  else
    {
      if (D_CZ0 && (wi == Z0width || wi == Z1width) &&
          (D_CO == Z0width || D_CO == Z1width))
        D_defwidth = D_CO;
      else
        D_defwidth = wi;
      D_defheight = he;
    }
  debug2("Default size: (%d,%d)\n", D_defwidth, D_defheight);
  if (change_fore)
    ResizeLayersToCanvases();
  if (D_CWS == NULL && displays->d_next == 0)
    {
      /* adapt all windows  -  to be removed ? */
      for (p = windows; p; p = p->w_next)
        {
          debug1("Trying to change window %d.\n", p->w_number);
          wwi = wi;
#if 0
          if (D_CZ0 && p->w_width != wi && (wi == Z0width || wi == Z1width))
	    {
	      if (p->w_width > (Z0width + Z1width) / 2)
		wwi = Z0width;
	      else
		wwi = Z1width;
	    }
#endif
	  if (p->w_savelayer && p->w_savelayer->l_cvlist == 0)
	    ResizeLayer(p->w_savelayer, wwi, he, 0);
#if 0
          ChangeWindowSize(p, wwi, he, p->w_histheight);
#endif
        }
    }
}

void
ResizeLayersToCanvases()
{
  struct canvas *cv;
  struct layer *l;
  int lx, ly;

  debug("ResizeLayersToCanvases\n");
  D_kaablamm = 0;
  for (cv = D_cvlist; cv; cv = cv->c_next)
    {
      l = cv->c_layer;
      if (l == 0)
	continue;
      debug("Doing canvas: ");
      if (l->l_width  == cv->c_xe - cv->c_xs + 1 &&
          l->l_height == cv->c_ye - cv->c_ys + 1)
        {
          debug("already fitting.\n");
          continue;
        }
      if (!MayResizeLayer(l))
        {
          debug("may not resize.\n");
        }
      else
	{
	  debug("doing resize.\n");
	  ResizeLayer(l, cv->c_xe - cv->c_xs + 1, cv->c_ye - cv->c_ys + 1, display);
	}

      /* normalize window, see screen.c */
      lx = cv->c_layer->l_x;
      ly = cv->c_layer->l_y;
      if (ly + cv->c_yoff < cv->c_ys)
	{
          cv->c_yoff = cv->c_ys - ly;
          RethinkViewportOffsets(cv);
	}
      else if (ly + cv->c_yoff > cv->c_ye)
	{
	  cv->c_yoff = cv->c_ye - ly;
          RethinkViewportOffsets(cv);
	}
      if (lx + cv->c_xoff < cv->c_xs)
        {
	  int n = cv->c_xs - (lx + cv->c_xoff);
	  if (n < (cv->c_xe - cv->c_xs + 1) / 2)
	    n = (cv->c_xe - cv->c_xs + 1) / 2;
	  if (cv->c_xoff + n > cv->c_xs)
	    n = cv->c_xs - cv->c_xoff;
	  cv->c_xoff += n;
	  RethinkViewportOffsets(cv);
        }
      else if (lx + cv->c_xoff > cv->c_xe)
	{
	  int n = lx + cv->c_xoff - cv->c_xe;
	  if (n < (cv->c_xe - cv->c_xs + 1) / 2)
	    n = (cv->c_xe - cv->c_xs + 1) / 2;
	  if (cv->c_xoff - n + cv->c_layer->l_width - 1 < cv->c_xe)
	    n = cv->c_xoff + cv->c_layer->l_width - 1 - cv->c_xe;
	  cv->c_xoff -= n;
	  RethinkViewportOffsets(cv);
	}
    }
  Redisplay(0);
  if (D_kaablamm)
    {
      kaablamm();
      D_kaablamm = 0;
    }
}

int
MayResizeLayer(l)
struct layer *l;
{
  int cvs = 0;
  debug("MayResizeLayer:\n");
  for (; l; l = l->l_next)
    {
      if (l->l_cvlist)
        if (++cvs > 1 || l->l_cvlist->c_lnext)
	  {
	    debug1("may not - cvs %d\n", cvs);
	    return 0;
	  }
    }
  debug("may resize\n");
  return 1;
}

/*
 *  Easy implementation: rely on the fact that the only layers
 *  supporting resize are Win and Blank. So just kill all overlays.
 *
 *  This is a lot harder if done the right way...
 */

static void
kaablamm()
{
  Msg(0, "Aborted because of window size change.");
}

void
ResizeLayer(l, wi, he, norefdisp)
struct layer *l;
int wi, he;
struct display *norefdisp;
{
  struct win *p;
  struct canvas *cv;
  struct layer *oldflayer = flayer;
  struct display *d, *olddisplay = display;

  if (l->l_width == wi && l->l_height == he)
    return;
  p = Layer2Window(l);

  if (oldflayer && (l == oldflayer || Layer2Window(oldflayer) == p))
    while (oldflayer->l_next)
      oldflayer = oldflayer->l_next;
    
  if (p)
    {
      for (d = displays; d; d = d->d_next)
	for (cv = d->d_cvlist; cv; cv = cv->c_next)
	  {
	    if (p == Layer2Window(cv->c_layer))
	      {
		flayer = cv->c_layer;
		if (flayer->l_next)
		  d->d_kaablamm = 1;
	        while (flayer->l_next)
		  ExitOverlayPage();
	      }
	  }
      l = p->w_savelayer;
    }
  flayer = l;
  if (p == 0 && flayer->l_next && flayer->l_next->l_next == 0 && LayResize(wi, he) == 0)
    {
      flayer = flayer->l_next;
      LayResize(wi, he);
      flayer = l;
    }
  else
    {
      if (flayer->l_next)
        for (cv = flayer->l_cvlist; cv; cv = cv->c_lnext)
	  cv->c_display->d_kaablamm = 1;
      while (flayer->l_next)
	ExitOverlayPage();
    }
  if (p)
    flayer = &p->w_layer;
  LayResize(wi, he);
  /* now everybody is on flayer, redisplay */
  l = flayer;
  for (display = displays; display; display = display->d_next)
    {
      if (display == norefdisp)
	continue;
      for (cv = D_cvlist; cv; cv = cv->c_next)
	if (cv->c_layer == l)
	  {
            CV_CALL(cv, LayRedisplayLine(-1, -1, -1, 0));
            RefreshArea(cv->c_xs, cv->c_ys, cv->c_xe, cv->c_ye, 0);
	  }
      if (D_kaablamm)
	{
	  kaablamm();
	  D_kaablamm = 0;
	}
    }
  flayer = oldflayer;
  display = olddisplay;
}


static void
FreeMline(ml)
struct mline *ml;
{
  if (ml->image)
    free(ml->image);
  if (ml->attr && ml->attr != null)
    free(ml->attr);
#ifdef FONT
  if (ml->font && ml->font != null)
    free(ml->font);
#endif
#ifdef COLOR
  if (ml->color && ml->color != null)
    free(ml->color);
# ifdef COLORS256
  if (ml->colorx && ml->colorx != null)
    free(ml->colorx);
# endif
#endif
  *ml = mline_zero;
}

static int
AllocMline(ml, w)
struct mline *ml;
int w;
{
  ml->image = malloc(w);
  ml->attr  = null;
#ifdef FONT
  ml->font  = null;
#endif
#ifdef COLOR
  ml->color = null;
# ifdef COLORS256
  ml->colorx = null;
# endif
#endif
  if (ml->image == 0)
    return -1;
  return 0;
}


static int
BcopyMline(mlf, xf, mlt, xt, l, w)
struct mline *mlf, *mlt;
int xf, xt, l, w;
{
  int r = 0;

  bcopy((char *)mlf->image + xf, (char *)mlt->image + xt, l);
  if (mlf->attr != null && mlt->attr == null)
    {
      if ((mlt->attr = (unsigned char *)malloc(w)) == 0)
	mlt->attr = null, r = -1;
      bzero((char *)mlt->attr, w);
    }
  if (mlt->attr != null)
    bcopy((char *)mlf->attr + xf, (char *)mlt->attr + xt, l);
#ifdef FONT
  if (mlf->font != null && mlt->font == null)
    {
      if ((mlt->font = (unsigned char *)malloc(w)) == 0)
	mlt->font = null, r = -1;
      bzero((char *)mlt->font, w);
    }
  if (mlt->font != null)
    bcopy((char *)mlf->font + xf, (char *)mlt->font + xt, l);
#endif
#ifdef COLOR
  if (mlf->color != null && mlt->color == null)
    {
      if ((mlt->color = (unsigned char *)malloc(w)) == 0)
	mlt->color = null, r = -1;
      bzero((char *)mlt->color, w);
    }
  if (mlt->color != null)
    bcopy((char *)mlf->color + xf, (char *)mlt->color + xt, l);
# ifdef COLORS256
  if (mlf->colorx != null && mlt->colorx == null)
    {
      if ((mlt->colorx = (unsigned char *)malloc(w)) == 0)
	mlt->colorx = null, r = -1;
      bzero((char *)mlt->colorx, w);
    }
  if (mlt->colorx != null)
    bcopy((char *)mlf->colorx + xf, (char *)mlt->colorx + xt, l);
# endif
#endif
  return r;
}


static int maxwidth;

static void
CheckMaxSize(wi)
int wi;
{
  unsigned char *oldnull = null;
  struct win *p;
  int i;
  struct mline *ml;

  wi = ((wi + 1) + 255) & ~255;
  if (wi <= maxwidth)
    return;
  maxwidth = wi;
  debug1("New maxwidth: %d\n", maxwidth);
  blank = (unsigned char *)xrealloc((char *)blank, maxwidth);
  null = (unsigned char *)xrealloc((char *)null, maxwidth);
  mline_old.image = (unsigned char *)xrealloc((char *)mline_old.image, maxwidth);
  mline_old.attr = (unsigned char *)xrealloc((char *)mline_old.attr, maxwidth);
#ifdef FONT
  mline_old.font = (unsigned char *)xrealloc((char *)mline_old.font, maxwidth);
#endif
#ifdef COLOR
  mline_old.color = (unsigned char *)xrealloc((char *)mline_old.color, maxwidth);
# ifdef COLORS256
  mline_old.colorx = (unsigned char *)xrealloc((char *)mline_old.color, maxwidth);
# endif
#endif
  if (!(blank && null && mline_old.image && mline_old.attr IFFONT(&& mline_old.font) IFCOLOR(&& mline_old.color) IFCOLORX(&& mline_old.colorx)))
    Panic(0, strnomem);

  MakeBlankLine(blank, maxwidth);
  bzero((char *)null, maxwidth);

  mline_blank.image = blank;
  mline_blank.attr  = null;
  mline_null.image = null;
  mline_null.attr  = null;
#ifdef FONT
  mline_blank.font  = null;
  mline_null.font  = null;
#endif
#ifdef COLOR
  mline_blank.color = null;
  mline_null.color = null;
# ifdef COLORS256
  mline_blank.colorx = null;
  mline_null.colorx = null;
# endif
#endif

  /* We have to run through all windows to substitute
   * the null references.
   */
  for (p = windows; p; p = p->w_next)
    {
      ml = p->w_mlines;
      for (i = 0; i < p->w_height; i++, ml++)
	{
	  if (ml->attr == oldnull)
	    ml->attr = null;
#ifdef FONT
	  if (ml->font == oldnull)
	    ml->font = null;
#endif
#ifdef COLOR
	  if (ml->color == oldnull)
	    ml->color= null;
#ifdef COLORS256
	  if (ml->colorx == oldnull)
	    ml->colorx = null;
#endif
#endif
	}
#ifdef COPY_PASTE
      ml = p->w_hlines;
      for (i = 0; i < p->w_histheight; i++, ml++)
	{
	  if (ml->attr == oldnull)
	    ml->attr = null;
# ifdef FONT
	  if (ml->font == oldnull)
	    ml->font = null;
# endif
# ifdef COLOR
	  if (ml->color == oldnull)
	    ml->color= null;
#  ifdef COLORS256
	  if (ml->colorx == oldnull)
	    ml->colorx = null;
#  endif
# endif
	}
#endif
    }
}


char *
xrealloc(mem, len)
char *mem;
int len;
{
  register char *nmem;

  if (mem == 0)
    return malloc(len);
  if ((nmem = realloc(mem, len)))
    return nmem;
  free(mem);
  return (char *)0;
}

static void
MakeBlankLine(p, n)
register unsigned char *p;
register int n;
{
  while (n--)
    *p++ = ' ';
}




#ifdef COPY_PASTE

#define OLDWIN(y) ((y < p->w_histheight) \
        ? &p->w_hlines[(p->w_histidx + y) % p->w_histheight] \
        : &p->w_mlines[y - p->w_histheight])

#define NEWWIN(y) ((y < hi) ? &nhlines[y] : &nmlines[y - hi])
	
#else

#define OLDWIN(y) (&p->w_mlines[y])
#define NEWWIN(y) (&nmlines[y])

#endif


int
ChangeWindowSize(p, wi, he, hi)
struct win *p;
int wi, he, hi;
{
  struct mline *mlf = 0, *mlt = 0, *ml, *nmlines, *nhlines;
  int fy, ty, l, lx, lf, lt, yy, oty, addone;
  int ncx, ncy, naka, t;
  int y, shift;

  if (wi == 0)
    he = hi = 0;

  if (p->w_width == wi && p->w_height == he && p->w_histheight == hi)
    {
      debug("ChangeWindowSize: No change.\n");
      return 0;
    }

  CheckMaxSize(wi);

  /* XXX */
#if 0
  /* just in case ... */
  if (wi && (p->w_width != wi || p->w_height != he) && p->w_lay != &p->w_winlay)
    {
      debug("ChangeWindowSize: No resize because of overlay?\n");
      return -1;
    }
#endif

  debug("ChangeWindowSize");
  debug3(" from (%d,%d)+%d", p->w_width, p->w_height, p->w_histheight);
  debug3(" to(%d,%d)+%d\n", wi, he, hi);

  fy = p->w_histheight + p->w_height - 1;
  ty = hi + he - 1;

  nmlines = nhlines = 0;
  ncx = 0;
  ncy = 0;
  naka = 0;

  if (wi)
    {
      if (wi != p->w_width || he != p->w_height)
	{
	  if ((nmlines = (struct mline *)calloc(he, sizeof(struct mline))) == 0)
	    {
	      KillWindow(p);
	      Msg(0, strnomem);
	      return -1;
	    }
	}
      else
	{
	  debug1("image stays the same: %d lines\n", he);
	  nmlines = p->w_mlines;
	  fy -= he;
	  ty -= he;
	  ncx = p->w_x;
	  ncy = p->w_y;
	  naka = p->w_autoaka;
	}
    }
#ifdef COPY_PASTE
  if (hi)
    {
      if ((nhlines = (struct mline *)calloc(hi, sizeof(struct mline))) == 0)
	{
	  Msg(0, "No memory for history buffer - turned off");
	  hi = 0;
	  ty = he - 1;
	}
    }
#endif

  /* special case: cursor is at magic margin position */
  addone = 0;
  if (p->w_width && p->w_x == p->w_width)
    {
      debug2("Special addone case: %d %d\n", p->w_x, p->w_y);
      addone = 1;
      p->w_x--;
    }

  /* handle the cursor and autoaka lines now if the widths are equal */
  if (p->w_width == wi)
    {
      ncx = p->w_x + addone;
      ncy = p->w_y + he - p->w_height;
      /* never lose sight of the line with the cursor on it */
      shift = -ncy;
      for (yy = p->w_y + p->w_histheight - 1; yy >= 0 && ncy + shift < he; yy--)
	{
	  ml = OLDWIN(yy);
	  if (ml->image[p->w_width] == ' ')
	    break;
	  shift++;
	}
      if (shift < 0)
	shift = 0;
      else
	debug1("resize: cursor out of bounds, shifting %d\n", shift);
      ncy += shift;
      if (p->w_autoaka > 0)
	{
	  naka = p->w_autoaka + he - p->w_height + shift;
	  if (naka < 1 || naka > he)
	    naka = 0;
	}
      while (shift-- > 0)
	{
	  ml = OLDWIN(fy);
	  FreeMline(ml);
	  fy--;
	}
    }
  debug2("fy %d ty %d\n", fy, ty);
  if (fy >= 0)
    mlf = OLDWIN(fy);
  if (ty >= 0)
    mlt = NEWWIN(ty);

  while (fy >= 0 && ty >= 0)
    {
      if (p->w_width == wi)
	{
	  /* here is a simple shortcut: just copy over */
	  *mlt = *mlf;
          *mlf = mline_zero;
	  if (--fy >= 0)
	    mlf = OLDWIN(fy);
	  if (--ty >= 0)
	    mlt = NEWWIN(ty);
	  continue;
	}

      /* calculate lenght */
      for (l = p->w_width - 1; l > 0; l--)
	if (mlf->image[l] != ' ' || mlf->attr[l])
	  break;
      if (fy == p->w_y + p->w_histheight && l < p->w_x)
	l = p->w_x;	/* cursor is non blank */
      l++;
      lf = l;

      /* add wrapped lines to length */
      for (yy = fy - 1; yy >= 0; yy--)
	{
	  ml = OLDWIN(yy);
	  if (ml->image[p->w_width] == ' ')
	    break;
	  l += p->w_width;
	}

      /* rewrap lines */
      lt = (l - 1) % wi + 1;	/* lf is set above */
      oty = ty;
      while (l > 0 && fy >= 0 && ty >= 0)
	{
	  lx = lt > lf ? lf : lt;
	  if (mlt->image == 0)
	    {
	      if (AllocMline(mlt, wi + 1))
		goto nomem;
    	      MakeBlankLine(mlt->image + lt, wi - lt);
	      mlt->image[wi] = ((oty == ty) ? ' ' : 0);
	    }
	  if (BcopyMline(mlf, lf - lx, mlt, lt - lx, lx, wi + 1))
	    goto nomem;

	  /* did we copy the cursor ? */
	  if (fy == p->w_y + p->w_histheight && lf - lx <= p->w_x && lf > p->w_x)
	    {
	      ncx = p->w_x + lt - lf + addone;
	      ncy = ty - hi;
	      shift = wi ? -ncy + (l - lx) / wi : 0;
	      if (ty + shift > hi + he - 1)
		shift = hi + he - 1 - ty;
	      if (shift > 0)
		{
	          debug3("resize: cursor out of bounds, shifting %d [%d/%d]\n", shift, lt - lx, wi);
		  for (y = hi + he - 1; y >= ty; y--)
		    {
		      mlt = NEWWIN(y);
		      FreeMline(mlt);
		      if (y - shift < ty)
			continue;
		      ml  = NEWWIN(y - shift);
		      *mlt = *ml;
		      *ml = mline_zero;
		    }
		  ncy += shift;
		  ty += shift;
		  mlt = NEWWIN(ty);
		  if (naka > 0)
		    naka = naka + shift > he ? 0 : naka + shift;
		}
	      ASSERT(ncy >= 0);
	    }
	  /* did we copy autoaka line ? */
	  if (p->w_autoaka > 0 && fy == p->w_autoaka - 1 + p->w_histheight && lf - lx <= 0)
	    naka = ty - hi >= 0 ? 1 + ty - hi : 0;

	  lf -= lx;
	  lt -= lx;
	  l  -= lx;
	  if (lf == 0)
	    {
	      FreeMline(mlf);
	      lf = p->w_width;
	      if (--fy >= 0)
	        mlf = OLDWIN(fy);
	    }
	  if (lt == 0)
	    {
	      lt = wi;
	      if (--ty >= 0)
	        mlt = NEWWIN(ty);
	    }
	}
      ASSERT(l != 0 || fy == yy);
    }
  while (fy >= 0)
    {
      FreeMline(mlf);
      if (--fy >= 0)
	mlf = OLDWIN(fy);
    }
  while (ty >= 0)
    {
      if (AllocMline(mlt, wi + 1))
	goto nomem;
      MakeBlankLine(mlt->image, wi + 1);
      if (--ty >= 0)
	mlt = NEWWIN(ty);
    }

#ifdef DEBUG
  if (nmlines != p->w_mlines)
    for (fy = 0; fy < p->w_height + p->w_histheight; fy++)
      {
	ml = OLDWIN(fy);
	ASSERT(ml->image == 0);
      }
#endif

  if (p->w_mlines && p->w_mlines != nmlines)
    free((char *)p->w_mlines);
  p->w_mlines = nmlines;
#ifdef COPY_PASTE
  if (p->w_hlines && p->w_hlines != nhlines)
    free((char *)p->w_hlines);
  p->w_hlines = nhlines;
#endif
  nmlines = nhlines = 0;

  /* change tabs */
  if (p->w_width != wi)
    {
      if (wi)
	{
	  t = p->w_tabs ? p->w_width : 0;
	  p->w_tabs = xrealloc(p->w_tabs, wi + 1);
	  if (p->w_tabs == 0)
	    {
	    nomem:
	      if (nmlines)
		{
		  for (ty = he + hi - 1; ty >= 0; ty--)
		    {
		      mlt = NEWWIN(ty);
		      FreeMline(mlt);
		    }
		  if (nmlines && p->w_mlines != nmlines)
		    free((char *)nmlines);
#ifdef COPY_PASTE
		  if (nhlines && p->w_hlines != nhlines)
		    free((char *)nhlines);
#endif
		}
	      KillWindow(p);
	      Msg(0, strnomem);
	      return -1;
	    }
	  for (; t < wi; t++)
	    p->w_tabs[t] = t && !(t & 7) ? 1 : 0; 
	  p->w_tabs[wi] = 0; 
	}
      else
	{
	  if (p->w_tabs)
	    free(p->w_tabs);
	  p->w_tabs = 0;
	}
    }

  /* Change w_Saved_y - this is only an estimate... */
  p->w_Saved_y += ncy - p->w_y;

  p->w_x = ncx;
  p->w_y = ncy;
  if (p->w_autoaka > 0)
    p->w_autoaka = naka;

  /* do sanity checks */
  if (p->w_x > wi)
    p->w_x = wi;
  if (p->w_y >= he)
    p->w_y = he - 1;
  if (p->w_Saved_x > wi)
    p->w_Saved_x = wi;
  if (p->w_Saved_y < 0)
    p->w_Saved_y = 0;
  if (p->w_Saved_y >= he)
    p->w_Saved_y = he - 1;

  /* reset scrolling region */
  p->w_top = 0;
  p->w_bot = he - 1;

  /* signal new size to window */
#ifdef TIOCSWINSZ
  if (wi && (p->w_width != wi || p->w_height != he) && p->w_ptyfd >= 0 && p->w_pid)
    {
      glwz.ws_col = wi;
      glwz.ws_row = he;
      debug("Setting pty winsize.\n");
      if (ioctl(p->w_ptyfd, TIOCSWINSZ, (char *)&glwz))
	debug2("SetPtySize: errno %d (fd:%d)\n", errno, p->w_ptyfd);
    }
#endif /* TIOCSWINSZ */

  /* store new size */
  p->w_width = wi;
  p->w_height = he;
#ifdef COPY_PASTE
  p->w_histidx = 0;
  p->w_histheight = hi;
#endif

#ifdef BUILTIN_TELNET
  if (p->w_type == W_TYPE_TELNET)
    TelWindowSize(p);
#endif

#ifdef DEBUG
  /* Test if everything was ok */
  for (fy = 0; fy < p->w_height + p->w_histheight; fy++)
    {
      ml = OLDWIN(fy);
      ASSERT(ml->image);
# ifdef UTF8
      if (p->w_encoding == UTF8)
	{
	  for (l = 0; l < p->w_width; l++)
	    ASSERT(ml->image[l] >= ' ' || ml->font[l]);
	}
      else
#endif
        for (l = 0; l < p->w_width; l++)
          ASSERT(ml->image[l] >= ' ');
    }
#endif
  return 0;
}

void
FreeAltScreen(p)
struct win *p;
{
  int i;

  if (p->w_alt_mlines)
    for (i = 0; i < p->w_alt_height; i++)
      FreeMline(p->w_alt_mlines + i);
  p->w_alt_mlines = 0;
  p->w_alt_width = 0;
  p->w_alt_height = 0;
  p->w_alt_x = 0;
  p->w_alt_y = 0;
#ifdef COPY_PASTE
  if (p->w_alt_hlines)
    for (i = 0; i < p->w_alt_histheight; i++)
      FreeMline(p->w_alt_hlines + i);
  p->w_alt_hlines = 0;
  p->w_alt_histidx = 0;
#endif
  p->w_alt_histheight = 0;
}

static void
SwapAltScreen(p)
struct win *p;
{
  struct mline *ml;
  int t;

  ml = p->w_alt_mlines; p->w_alt_mlines = p->w_mlines; p->w_mlines = ml;
  t = p->w_alt_width; p->w_alt_width = p->w_width; p->w_width = t;
  t = p->w_alt_height; p->w_alt_height = p->w_height; p->w_height = t;
  t = p->w_alt_histheight; p->w_alt_histheight = p->w_histheight; p->w_histheight = t;
  t = p->w_alt_x; p->w_alt_x = p->w_x; p->w_x = t;
  t = p->w_alt_y; p->w_alt_y = p->w_y; p->w_y = t;
#ifdef COPY_PASTE
  ml = p->w_alt_hlines; p->w_alt_hlines = p->w_hlines; p->w_hlines = ml;
  t = p->w_alt_histidx; p->w_alt_histidx = p->w_histidx; p->w_histidx = t;
#endif
}

void
EnterAltScreen(p)
struct win *p;
{
  int ox = p->w_x, oy = p->w_y;
  FreeAltScreen(p);
  SwapAltScreen(p);
  ChangeWindowSize(p, p->w_alt_width, p->w_alt_height, p->w_alt_histheight);
  p->w_x = ox;
  p->w_y = oy;
}

void
LeaveAltScreen(p)
struct win *p;
{
  if (!p->w_alt_mlines)
    return;
  SwapAltScreen(p);
  ChangeWindowSize(p, p->w_alt_width, p->w_alt_height, p->w_alt_histheight);
  FreeAltScreen(p);
}

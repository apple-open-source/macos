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
#include "extern.h"

#define INPUTLINE (flayer->l_height - 1)

static void InpProcess __P((char **, int *));
static void InpAbort __P((void));
static void InpRedisplayLine __P((int, int, int, int));

extern struct layer *flayer;
extern struct display *display;
extern struct mchar mchar_blank, mchar_so;

struct inpline
{
  char  buf[101];	/* text buffer */
  int  len;		/* length of the editible string */
  int  pos;		/* cursor position in editable string */
};

static struct inpline inphist; /* XXX: should be a dynamic list */


struct inpdata
{
  struct inpline inp;
  int  inpmaxlen;	/* 100, or less, if caller has shorter buffer */
  char *inpstring;	/* the prompt */
  int  inpstringlen;	/* length of the prompt */
  int  inpmode;		/* INP_NOECHO, INP_RAW, INP_EVERY */
  void (*inpfinfunc) __P((char *buf, int len, char *priv));
  char  *priv;		/* private data for finfunc */
};

static struct LayFuncs InpLf =
{
  InpProcess,
  InpAbort,
  InpRedisplayLine,
  DefClearLine,
  DefRewrite,
  DefResize,
  DefRestore
};

/*
**   Here is the input routine
*/

/* called once, after InitOverlayPage in Input() or Isearch() */
void
inp_setprompt(p, s)
char *p, *s;
{
  struct inpdata *inpdata;
  
  inpdata = (struct inpdata *)flayer->l_data;
  if (p)
    {
      inpdata->inpstringlen = strlen(p);
      inpdata->inpstring = p;
    }
  if (s)
    {
      if (s != inpdata->inp.buf)
	strncpy(inpdata->inp.buf, s, sizeof(inpdata->inp.buf) - 1);
      inpdata->inp.buf[sizeof(inpdata->inp.buf) - 1] = 0;
      inpdata->inp.pos = inpdata->inp.len = strlen(inpdata->inp.buf);
    }
  InpRedisplayLine(INPUTLINE, 0, flayer->l_width - 1, 0);
}

/*
 * We dont use HS status line with Input().
 * If we would use it, then we should check e_tgetflag("es") if
 * we are allowed to use esc sequences there.
 *
 * mode is an OR of
 * INP_NOECHO == suppress echoing of characters.
 * INP_RAW    == raw mode. call finfunc after each character typed.
 * INP_EVERY  == digraph mode.
 */
void
Input(istr, len, mode, finfunc, data)
char *istr;
int len;
int mode;
void (*finfunc) __P((char *buf, int len, char *data));
char *data;
{
  int maxlen;
  struct inpdata *inpdata;
  
  if (len > 100)
    len = 100;
  if (!(mode & INP_NOECHO))
    {
      maxlen = flayer->l_width - 1 - strlen(istr);
      if (len > maxlen)
	len = maxlen;
    }
  if (len < 0)
    {
      LMsg(0, "Width %d chars too small", -len);
      return;
    }
  if (InitOverlayPage(sizeof(*inpdata), &InpLf, 1))
    return;
  inpdata = (struct inpdata *)flayer->l_data;
  inpdata->inpmaxlen = len;
  inpdata->inpfinfunc = finfunc;
  inpdata->inp.pos = inpdata->inp.len = 0;
  inpdata->inpmode = mode;
  inpdata->priv = data;
  inp_setprompt(istr, (char *)NULL);
  flayer->l_x = inpdata->inpstringlen;
  flayer->l_y = INPUTLINE;
}

static void
InpProcess(ppbuf, plen)
char **ppbuf;
int *plen;
{
  int len, x;
  char *pbuf;
  char ch;
  struct inpdata *inpdata;
  struct display *inpdisplay;

  inpdata = (struct inpdata *)flayer->l_data;
  inpdisplay = display;

  LGotoPos(flayer, inpdata->inpstringlen + (inpdata->inpmode & INP_NOECHO ? 0 : inpdata->inp.pos), INPUTLINE);
  if (ppbuf == 0)
    {
      InpAbort();
      return;
    }
  x = inpdata->inpstringlen + inpdata->inp.pos;
  len = *plen;
  pbuf = *ppbuf;
  while (len)
    {
      char *p = inpdata->inp.buf + inpdata->inp.pos;

      ch = *pbuf++;
      len--;
      if (inpdata->inpmode & INP_EVERY)
	{
	  inpdata->inp.buf[inpdata->inp.len] = ch;
	  inpdata->inp.buf[inpdata->inp.len + 1] = ch;	/* gross */
	  display = inpdisplay;
	  (*inpdata->inpfinfunc)(inpdata->inp.buf, inpdata->inp.len, inpdata->priv);
	  ch = inpdata->inp.buf[inpdata->inp.len];
	}
      else if (inpdata->inpmode & INP_RAW)
	{
	  display = inpdisplay;
          (*inpdata->inpfinfunc)(&ch, 1, inpdata->priv);	/* raw */
	  if (ch)
	    continue;
	}
      if (((unsigned char)ch & 0177) >= ' ' && ch != 0177 && inpdata->inp.len < inpdata->inpmaxlen)
	{
	  if (inpdata->inp.len > inpdata->inp.pos)
	    bcopy(p, p+1, inpdata->inp.len - inpdata->inp.pos);
	  inpdata->inp.buf[inpdata->inp.pos++] = ch;
	  inpdata->inp.len++;

	  if (!(inpdata->inpmode & INP_NOECHO))
	    {
	      struct mchar mc;
	      mc = mchar_so;
	      mc.image = *p++;
	      LPutChar(flayer, &mc, x, INPUTLINE);
	      x++;
	      if (p < inpdata->inp.buf+inpdata->inp.len)
		{
		  while (p < inpdata->inp.buf+inpdata->inp.len)
		    {
		      mc.image = *p++;
		      LPutChar(flayer, &mc, x++, INPUTLINE);
		    }
		  x = inpdata->inpstringlen + inpdata->inp.pos;
		  LGotoPos(flayer, x, INPUTLINE);
		}
	    }
	}
      else if ((ch == '\b' || ch == 0177) && inpdata->inp.pos > 0)
	{
	  if (inpdata->inp.len > inpdata->inp.pos)
	    bcopy(p, p-1, inpdata->inp.len - inpdata->inp.pos);
	  inpdata->inp.len--;
	  inpdata->inp.pos--;
	  p--;

	  if (!(inpdata->inpmode & INP_NOECHO))
	    {
	      struct mchar mc;
	      mc = mchar_so;
	      x--;
	      while (p < inpdata->inp.buf+inpdata->inp.len)
		{
		  mc.image = *p++;
		  LPutChar(flayer, &mc, x++, INPUTLINE);
		}
	      LPutChar(flayer, &mchar_blank, x, INPUTLINE);
	      x = inpdata->inpstringlen + inpdata->inp.pos;
	      LGotoPos(flayer, x, INPUTLINE);
	    }
	}
      else if (ch == '\025')			/* CTRL-U */
	{
  	  x = inpdata->inpstringlen;
	  if (inpdata->inp.len && !(inpdata->inpmode & INP_NOECHO))
	    {
	      LClearArea(flayer, x, INPUTLINE, x + inpdata->inp.len - 1, INPUTLINE, 0, 0);
	      LGotoPos(flayer, x, INPUTLINE);
	    }
	  inpdata->inp.len = inpdata->inp.pos = 0;
	}
      else if (ch == '\013')			/* CTRL-K */
	{
  	  x = inpdata->inpstringlen + inpdata->inp.pos;
	  if (inpdata->inp.len > inpdata->inp.pos && !(inpdata->inpmode & INP_NOECHO))
	    {
	      LClearArea(flayer, x, INPUTLINE, x + inpdata->inp.len - inpdata->inp.pos - 1, INPUTLINE, 0, 0);
	      LGotoPos(flayer, x, INPUTLINE);
	    }
	  inpdata->inp.len = inpdata->inp.pos;
	}
      else if (ch == '\001' || (unsigned char)ch == 0201)	/* CTRL-A */
	{
	  LGotoPos(flayer, x -= inpdata->inp.pos, INPUTLINE);
	  inpdata->inp.pos = 0;
	}
      else if ((ch == '\002' || (unsigned char)ch == 0202) && inpdata->inp.pos > 0)	/* CTRL-B */
	{
	  LGotoPos(flayer, --x, INPUTLINE);
	  inpdata->inp.pos--;
	}
      else if (ch == '\005' || (unsigned char)ch == 0205)	/* CTRL-E */
	{
	  LGotoPos(flayer, x += inpdata->inp.len - inpdata->inp.pos, INPUTLINE);
	  inpdata->inp.pos = inpdata->inp.len;
	}
      else if ((ch == '\006' || (unsigned char)ch == 0206) && inpdata->inp.pos < inpdata->inp.len)	/* CTRL-F */
	{
	  LGotoPos(flayer, ++x, INPUTLINE);
	  inpdata->inp.pos++;
	}
      else if (ch == '\020' || (unsigned char)ch == 0220)	/* CTRL-P */
        {
	  struct mchar mc;
	  mc = mchar_so;
	  if (inpdata->inp.len && !(inpdata->inpmode & INP_NOECHO))
	    LClearArea(flayer, inpdata->inpstringlen, INPUTLINE, inpdata->inpstringlen + inpdata->inp.len - 1, INPUTLINE, 0, 0);

	  inpdata->inp = inphist;	/* structure copy */
	  if (inpdata->inp.len > inpdata->inpmaxlen)
	    inpdata->inp.len = inpdata->inpmaxlen;
	  if (inpdata->inp.pos > inpdata->inp.len)
	    inpdata->inp.pos = inpdata->inp.len;

  	  x = inpdata->inpstringlen;
	  p = inpdata->inp.buf;
	  
	  if (!(inpdata->inpmode & INP_NOECHO))
	    {
	      while (p < inpdata->inp.buf+inpdata->inp.len)
		{
		  mc.image = *p++;
		  LPutChar(flayer, &mc, x++, INPUTLINE);
		}
	    }
  	  x = inpdata->inpstringlen + inpdata->inp.pos;
	  LGotoPos(flayer, x, INPUTLINE);
	}

      else if (ch == '\004' || ch == '\003' || ch == '\007' || ch == '\033' ||
	       ch == '\000' || ch == '\n' || ch == '\r')
	{
          if (ch != '\004' && ch != '\n' && ch != '\r')
	    inpdata->inp.len = 0;
	  inpdata->inp.buf[inpdata->inp.len] = 0;

	  if (inpdata->inp.len && inpdata->inpmode == 0)
	    inphist = inpdata->inp;	/* structure copy */
	  
  	  flayer->l_data = 0;
          InpAbort();		/* redisplays... */
	  *ppbuf = pbuf;
	  *plen = len;
	  display = inpdisplay;
	  if ((inpdata->inpmode & INP_RAW) == 0)
            (*inpdata->inpfinfunc)(inpdata->inp.buf, inpdata->inp.len, inpdata->priv);
	  else
            (*inpdata->inpfinfunc)(pbuf - 1, 0, inpdata->priv);
	  free((char *)inpdata);
	  return;
	}
    }
  if (!(inpdata->inpmode & INP_RAW))
    {
      flayer->l_x = inpdata->inpstringlen + (inpdata->inpmode & INP_NOECHO ? 0 : inpdata->inp.pos);
      flayer->l_y = INPUTLINE;
    }
  *ppbuf = pbuf;
  *plen = len;
}

static void
InpAbort()
{
  LAY_CALL_UP(LayRedisplayLine(INPUTLINE, 0, flayer->l_width - 1, 0));
  ExitOverlayPage();
}

static void
InpRedisplayLine(y, xs, xe, isblank)
int y, xs, xe, isblank;
{
  int q, r, s, l, v;
  struct inpdata *inpdata;
  
  inpdata = (struct inpdata *)flayer->l_data;
  if (y != INPUTLINE)
    {
      LAY_CALL_UP(LayRedisplayLine(y, xs, xe, isblank));
      return;
    }
  inpdata->inp.buf[inpdata->inp.len] = 0;
  q = xs;
  v = xe - xs + 1;
  s = 0;
  r = inpdata->inpstringlen;
  if (v > 0 && q < r)
    {
      l = v;
      if (l > r - q)
	l = r - q;
      LPutStr(flayer, inpdata->inpstring + q - s, l, &mchar_so, q, y);
      q += l;
      v -= l;
    }
  s = r;
  r += inpdata->inp.len;
  if (!(inpdata->inpmode & INP_NOECHO) && v > 0 && q < r)
    {
      l = v;
      if (l > r - q)
	l = r - q;
      LPutStr(flayer, inpdata->inp.buf + q - s, l, &mchar_so, q, y);
      q += l;
      v -= l;
    }
  s = r;
  r = flayer->l_width;
  if (!isblank && v > 0 && q < r)
    {
      l = v;
      if (l > r - q)
	l = r - q;
      LClearArea(flayer, q, y, q + l - 1, y, 0, 0);
      q += l;
    }
}

int
InInput()
{
  if (flayer && flayer->l_layfn == &InpLf)
    return 1;
  return 0;
}


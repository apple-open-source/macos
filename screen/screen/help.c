/* Copyright (c) 1993-2000
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
RCS_ID("$Id: help.c,v 1.1.1.1 2001/12/14 22:08:29 bbraun Exp $ FAU")

#include <sys/types.h>

#include "config.h"

#include "screen.h"
#include "extern.h"

char version[40];      /* initialised by main() */

extern struct layer *flayer;
extern struct display *displays;
extern char *noargs[];
extern struct mchar mchar_blank;
extern char *blank;

static void PadStr __P((char *, int, int, int));


void
exit_with_usage(myname, message, arg)
char *myname, *message, *arg;
{
  printf("Use: %s [-opts] [cmd [args]]\n", myname);
  printf(" or: %s -r [host.tty]\n\nOptions:\n", myname);
  printf("-a            Force all capabilities into each window's termcap.\n");
  printf("-A -[r|R]     Adapt all windows to the new display width & height.\n");
  printf("-c file       Read configuration file instead of '.screenrc'.\n");
#ifdef REMOTE_DETACH
  printf("-d (-r)       Detach the elsewhere running screen (and reattach here).\n");
  printf("-dmS name     Start as daemon: Screen session in detached mode.\n");
  printf("-D (-r)       Detach and logout remote (and reattach here).\n");
  printf("-D -RR        Do whatever is needed to get a screen session.\n");
#endif
  printf("-e xy         Change command characters.\n");
  printf("-f            Flow control on, -fn = off, -fa = auto.\n");
  printf("-h lines      Set the size of the scrollback history buffer.\n");
  printf("-i            Interrupt output sooner when flow control is on.\n");
#if defined(LOGOUTOK) && defined(UTMPOK)
  printf("-l            Login mode on (update %s), -ln = off.\n", UTMPFILE);
#endif
  printf("-list         or -ls. Do nothing, just list our SockDir.\n");
  printf("-L            Terminal's last character can be safely updated.\n");
  printf("-m            ignore $STY variable, do create a new screen session.\n");
  printf("-O            Choose optimal output rather than exact vt100 emulation.\n");
  printf("-p window     Preselect the named window if it exists.\n");
  printf("-q            Quiet startup. Exits with non-zero return code if unsuccessful.\n");
  printf("-r            Reattach to a detached screen process.\n");
  printf("-R            Reattach if possible, otherwise start a new session.\n");
  printf("-s shell      Shell to execute rather than $SHELL.\n");
  printf("-S sockname   Name this session <pid>.sockname instead of <pid>.<tty>.<host>.\n");
  printf("-t title      Set title. (window's name).\n");
  printf("-T term       Use term as $TERM for windows, rather than \"screen\".\n");
#ifdef UTF8
  printf("-U            Tell screen to use UTF-8 encoding.\n");
#endif
  printf("-v            Print \"Screen version %s\".\n", version);
  printf("-wipe         Do nothing, just clean up SockDir.\n");
#ifdef MULTI
  printf("-x            Attach to a not detached screen. (Multi display mode).\n");
#endif /* MULTI */
  printf("-X            Execute <cmd> as a screen command in the specified session.\n");
  if (message && *message)
    {
      printf("\nError: ");
      printf(message, arg);
      printf("\n");
    }
  exit(1);
}

/*
**   Here come the help page routines
*/

extern struct comm comms[];
extern struct action ktab[];

static void HelpProcess __P((char **, int *));
static void HelpAbort __P((void));
static void HelpRedisplayLine __P((int, int, int, int));
static void add_key_to_buf __P((char *, int));
static void AddAction __P((struct action *, int, int));
static int  helppage __P((void));

struct helpdata
{
  int	maxrow, grow, numcols, numrows, num_names;
  int	numskip, numpages;
  int	command_search, command_bindings;
  int   refgrow, refcommand_search;
  int   inter, mcom, mkey;
  int   nact[RC_LAST + 1];
};

#define MAXKLEN 256

static struct LayFuncs HelpLf =
{
  HelpProcess,
  HelpAbort,
  HelpRedisplayLine,
  DefClearLine,
  DefRewrite,
  DefResize,
  DefRestore
};


void
display_help()
{
  int i, n, key, mcom, mkey, l;
  struct helpdata *helpdata;
  int used[RC_LAST + 1];

  if (flayer->l_height < 6)
    {
      LMsg(0, "Window height too small for help page");
      return;
    }
  if (InitOverlayPage(sizeof(*helpdata), &HelpLf, 0))
    return;

  helpdata = (struct helpdata *)flayer->l_data;
  helpdata->num_names = helpdata->command_bindings = 0;
  helpdata->command_search = 0;
  for (n = 0; n <= RC_LAST; n++)
    used[n] = 0;
  mcom = 0;
  mkey = 0;
  for (key = 0; key < 256; key++)
    {
      n = ktab[key].nr;
      if (n == RC_ILLEGAL)
	continue;
      if (ktab[key].args == noargs)
	{
          used[n] += (key <= ' ' || key == 0x7f) ? 3 :
                     (key > 0x7f) ? 5 : 2;
	}
      else
	helpdata->command_bindings++;
    }
  for (n = i = 0; n <= RC_LAST; n++)
    if (used[n])
      {
	l = strlen(comms[n].name);
	if (l > mcom)
	  mcom = l;
	if (used[n] > mkey)
	  mkey = used[n];
        helpdata->nact[i++] = n;
      }
  debug1("help: %d commands bound to keys with no arguments\n", i);
  debug2("mcom: %d  mkey: %d\n", mcom, mkey);
  helpdata->num_names = i;

  if (mkey > MAXKLEN)
    mkey = MAXKLEN;
  helpdata->numcols = flayer->l_width / (mcom + mkey + 1);
  if (helpdata->numcols == 0)
    {
      HelpAbort();
      LMsg(0, "Width too small");
      return;
    }
  helpdata->inter = (flayer->l_width - (mcom + mkey) * helpdata->numcols) / (helpdata->numcols + 1);
  if (helpdata->inter <= 0)
    helpdata->inter = 1;
  debug1("inter: %d\n", helpdata->inter);
  helpdata->mcom = mcom;
  helpdata->mkey = mkey;
  helpdata->numrows = (helpdata->num_names + helpdata->numcols - 1) / helpdata->numcols;
  debug1("Numrows: %d\n", helpdata->numrows);
  helpdata->numskip = flayer->l_height-5 - (2 + helpdata->numrows);
  while (helpdata->numskip < 0)
    helpdata->numskip += flayer->l_height-5;
  helpdata->numskip %= flayer->l_height-5;
  debug1("Numskip: %d\n", helpdata->numskip);
  if (helpdata->numskip > flayer->l_height/3 || helpdata->numskip > helpdata->command_bindings)
    helpdata->numskip = 1;
  helpdata->maxrow = 2 + helpdata->numrows + helpdata->numskip + helpdata->command_bindings;
  helpdata->grow = 0;

  helpdata->numpages = (helpdata->maxrow + flayer->l_height-6) / (flayer->l_height-5);
  flayer->l_x = 0;
  flayer->l_y = flayer->l_height - 1;
  helppage();
}

static void
HelpProcess(ppbuf, plen)
char **ppbuf;
int *plen;
{
  int done = 0;

  while (!done && *plen > 0)
    {
      switch (**ppbuf)
	{
	case ' ':
	  if (helppage() == 0)
            break;
	  /* FALLTHROUGH */
	case '\r':
	case '\n':
	  done = 1;
	  break;
	default:
	  break;
	}
      ++*ppbuf;
      --*plen;
    }
  if (done)
    HelpAbort();
}

static void
HelpAbort()
{
  LAY_CALL_UP(LRefreshAll(flayer, 0));
  ExitOverlayPage();
}


static int
helppage()
{
  struct helpdata *helpdata;
  int col, crow, n, key, x;
  char buf[MAXKLEN], Esc_buf[5], cbuf[256];

  helpdata = (struct helpdata *)flayer->l_data;

  if (helpdata->grow >= helpdata->maxrow)
    return -1;
  helpdata->refgrow = helpdata->grow;
  helpdata->refcommand_search = helpdata->command_search;

  /* Clear the help screen */
  LClearAll(flayer, 0);
  
  sprintf(cbuf,"Screen key bindings, page %d of %d.", helpdata->grow / (flayer->l_height-5) + 1, helpdata->numpages);
  centerline(cbuf, 0);
  crow = 2;

  *Esc_buf = '\0';
  *buf = '\0';
  /* XXX fix escape character */
  if (flayer->l_cvlist && flayer->l_cvlist->c_display)
    {
      add_key_to_buf(buf, flayer->l_cvlist->c_display->d_user->u_MetaEsc);
      add_key_to_buf(Esc_buf, flayer->l_cvlist->c_display->d_user->u_Esc);
    }
  else
    {
      strcpy(Esc_buf, "??");
      strcpy(buf, "??");
    }

  for (; crow < flayer->l_height - 3; crow++)
    {
      if (helpdata->grow < 1)
        {
          sprintf(cbuf,"Command key:  %s   Literal %s:  %s", Esc_buf, Esc_buf, buf);
          centerline(cbuf, crow);
	  helpdata->grow++;
        }
      else if (helpdata->grow >= 2 && helpdata->grow-2 < helpdata->numrows)
	{
	  x = 0;
	  for (col = 0; col < helpdata->numcols && (n = helpdata->numrows * col + (helpdata->grow-2)) < helpdata->num_names; col++)
	    {
	      x += helpdata->inter - !col;
	      n = helpdata->nact[n];
	      buf[0] = '\0';
	      for (key = 0; key < 256; key++)
		if (ktab[key].nr == n && ktab[key].args == noargs && strlen(buf) < sizeof(buf) - 7)
		  {
		    strcat(buf, " ");
		    add_key_to_buf(buf, key);
		  }
	      PadStr(comms[n].name, helpdata->mcom, x, crow);
	      x += helpdata->mcom;
	      PadStr(buf, helpdata->mkey, x, crow);
	      x += helpdata->mkey;
	    }
          helpdata->grow++;
        }
      else if (helpdata->grow-2-helpdata->numrows >= helpdata->numskip 
               && helpdata->grow-2-helpdata->numrows-helpdata->numskip < helpdata->command_bindings)
        {
	  while ((n = ktab[helpdata->command_search].nr) == RC_ILLEGAL
		 || ktab[helpdata->command_search].args == noargs)
	    {
	      if (++helpdata->command_search >= 256)
		return -1;
	    }
	  buf[0] = '\0';
	  add_key_to_buf(buf, helpdata->command_search);
	  PadStr(buf, 4, 0, crow);
	  AddAction(&ktab[helpdata->command_search++], 4, crow);
	  helpdata->grow++;
	}
      else
	helpdata->grow++;
    }
  sprintf(cbuf,"[Press Space %s Return to end.]",
	 helpdata->grow < helpdata->maxrow ? "for next page;" : "or");
  centerline(cbuf, flayer->l_height - 2);
  LaySetCursor();
  return 0;
}

static void
AddAction(act, x, y)
struct action *act;
int x, y;
{
  char buf[256];
  int del, l;
  char *bp, *cp, **pp;
  int fr;
  struct mchar mchar_dol;

  mchar_dol = mchar_blank;
  mchar_dol.image = '$';

  fr = flayer->l_width - 1 - x;
  if (fr <= 0)
    return;
  l = strlen(comms[act->nr].name);
  
  if (l + 1 > fr)
    l = fr - 1;
  PadStr(comms[act->nr].name, l, x, y);
  x += l;
  fr -= l + 1;
  LPutChar(flayer, fr ? &mchar_blank : &mchar_dol, x++, y);

  pp = act->args;
  while (pp && (cp = *pp) != NULL)
    {
      del = 0;
      bp = buf;
      if (!*cp || (index(cp, ' ') != NULL))
	{
	  if (index(cp, '\'') != NULL)
	    *bp++ = del = '"';
	  else
	    *bp++ = del = '\'';
	}
      while (*cp && bp < buf + 250)
        bp += AddXChar(bp, *(unsigned char *)cp++);
      if (del)
	*bp++ = del;
      *bp = 0;
      if ((fr -= (bp - buf) + 1) < 0)
	{
	  fr += bp - buf;
	  if (fr > 0)
	    PadStr(buf, fr, x, y);
	  if (fr == 0)
	    LPutChar(flayer, &mchar_dol, x, y);
	  return;
	}
      PadStr(buf, strlen(buf), x, y);
      x += strlen(buf);
      pp++;
      if (*pp)
        LPutChar(flayer, fr ? &mchar_blank : &mchar_dol, x++, y);
    }
}

static void
add_key_to_buf(buf, key)
char *buf;
int key;
{
  buf += strlen(buf);
  if (key < 0)
    strcpy(buf, "unset");
  else if (key == ' ')
    strcpy(buf, "sp");
  else
    buf[AddXChar(buf, key)] = 0;
}


static void
HelpRedisplayLine(y, xs, xe, isblank)
int y, xs, xe, isblank;
{
  if (y < 0)
    {
      struct helpdata *helpdata;

      helpdata = (struct helpdata *)flayer->l_data;
      helpdata->grow = helpdata->refgrow;
      helpdata->command_search = helpdata->refcommand_search;
      helppage();
      return;
    }
  if (y != 0 && y != flayer->l_height - 1)
    return;
  if (!isblank)
    LClearArea(flayer, xs, y, xe, y, 0, 0);
}


/*
**
**    here is all the copyright stuff 
**
*/

static void CopyrightProcess __P((char **, int *));
static void CopyrightRedisplayLine __P((int, int, int, int));
static void CopyrightAbort __P((void));
static void copypage __P((void));

struct copydata
{
  char	*cps, *savedcps;	/* position in the message */
  char	*refcps, *refsavedcps;	/* backup for redisplaying */
};

static struct LayFuncs CopyrightLf =
{
  CopyrightProcess,
  CopyrightAbort,
  CopyrightRedisplayLine,
  DefClearLine,
  DefRewrite,
  DefResize,
  DefRestore
};

static const char cpmsg[] = "\
\n\
Screen version %v\n\
\n\
Copyright (c) 1993-2000 Juergen Weigert, Michael Schroeder\n\
Copyright (c) 1987 Oliver Laumann\n\
\n\
This program is free software; you can redistribute it and/or \
modify it under the terms of the GNU General Public License as published \
by the Free Software Foundation; either version 2, or (at your option) \
any later version.\n\
\n\
This program is distributed in the hope that it will be useful, \
but WITHOUT ANY WARRANTY; without even the implied warranty of \
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the \
GNU General Public License for more details.\n\
\n\
You should have received a copy of the GNU General Public License \
along with this program (see the file COPYING); if not, write to the \
Free Software Foundation, Inc., \
59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.\n\
\n\
Send bugreports, fixes, enhancements, t-shirts, money, beer & pizza to \
screen@uni-erlangen.de\n";


static void
CopyrightProcess(ppbuf, plen)
char **ppbuf;
int *plen;
{
  int done = 0;
  struct copydata *copydata;

  copydata = (struct copydata *)flayer->l_data;
  while (!done && *plen > 0)
    {
      switch (**ppbuf)
	{
	case ' ':
          if (*copydata->cps)
	    {
	      copypage();
	      break;
	    }
	  /* FALLTHROUGH */
	case '\r':
	case '\n':
	  CopyrightAbort();
	  done = 1;
	  break;
	default:
	  break;
	}
      ++*ppbuf;
      --*plen;
    }
}

static void
CopyrightAbort()
{
  LAY_CALL_UP(LRefreshAll(flayer, 0));
  ExitOverlayPage();
}

void
display_copyright()
{
  struct copydata *copydata;

  if (flayer->l_width < 10 || flayer->l_height < 5)
    {
      LMsg(0, "Window size too small for copyright page");
      return;
    }
  if (InitOverlayPage(sizeof(*copydata), &CopyrightLf, 0))
    return;
  copydata = (struct copydata *)flayer->l_data;
  copydata->cps = (char *)cpmsg;
  copydata->savedcps = 0;
  flayer->l_x = 0;
  flayer->l_y = flayer->l_height - 1;
  copypage();
}

static void
copypage()
{
  register char *cps;
  char *ws;
  int x, y, l;
  char cbuf[80];
  struct copydata *copydata;

  ASSERT(flayer);
  copydata = (struct copydata *)flayer->l_data;

  LClearAll(flayer, 0);
  x = y = 0;
  cps = copydata->cps;
  copydata->refcps = cps;
  copydata->refsavedcps = copydata->savedcps;
  while (*cps && y < flayer->l_height - 3)
    {
      ws = cps;
      while (*cps == ' ')
	cps++;
      if (strncmp(cps, "%v", 2) == 0)
	{
	  copydata->savedcps = cps + 2;
	  cps = version;
	  continue;
	}
      while (*cps && *cps != ' ' && *cps != '\n')
	cps++;
      l = cps - ws;
      cps = ws;
      if (l > flayer->l_width - 1)
	l = flayer->l_width - 1;
      if (x && x + l >= flayer->l_width - 2)
	{
	  x = 0;
	  y++;
	  continue;
	}
      if (x)
	{
	  LPutChar(flayer, &mchar_blank, x, y);
	  x++;
	}
      if (l)
        LPutStr(flayer, ws, l, &mchar_blank, x, y);
      x += l;
      cps += l;
      if (*cps == 0 && copydata->savedcps)
	{
	  cps = copydata->savedcps;
	  copydata->savedcps = 0;
	}
      if (*cps == '\n')
	{
	  x = 0;
	  y++;
	}
      if (*cps == ' ' || *cps == '\n')
	cps++;
    }
  while (*cps == '\n')
    cps++;
  sprintf(cbuf,"[Press Space %s Return to end.]",
	 *cps ? "for next page;" : "or");
  centerline(cbuf, flayer->l_height - 2);
  copydata->cps = cps;
  LaySetCursor();
}

static void
CopyrightRedisplayLine(y, xs, xe, isblank)
int y, xs, xe, isblank;
{
  ASSERT(flayer);
  if (y < 0)
    {
      struct copydata *copydata;

      copydata = (struct copydata *)flayer->l_data;
      copydata->cps = copydata->refcps;
      copydata->savedcps = copydata->refsavedcps;
      copypage();
      return;
    }
  if (y != 0 && y != flayer->l_height - 1)
    return;
  if (isblank)
    return;
  LClearArea(flayer, xs, y, xe, y, 0, 0);
}



/*
**
**    here is all the displays stuff 
**
*/

#ifdef MULTI

static void DisplaysProcess __P((char **, int *));
static void DisplaysRedisplayLine __P((int, int, int, int));
static void displayspage __P((void));

struct displaysdata
{
  int dummy_element_for_solaris;
};

static struct LayFuncs DisplaysLf =
{
  DisplaysProcess,
  HelpAbort,
  DisplaysRedisplayLine,
  DefClearLine,
  DefRewrite,
  DefResize,
  DefRestore
};

static void
DisplaysProcess(ppbuf, plen)
char **ppbuf;
int *plen;
{
  int done = 0;
  struct displaysdata *displaysdata;

  ASSERT(flayer);
  displaysdata = (struct displaysdata *)flayer->l_data;
  while (!done && *plen > 0)
    {
      switch (**ppbuf)
	{
	case ' ':
	  displayspage();
	  break;
	  /* FALLTHROUGH */
	case '\r':
	case '\n':
	  HelpAbort();
	  done = 1;
	  break;
	default:
	  break;
	}
      ++*ppbuf;
      --*plen;
    }
}


void
display_displays()
{
  struct displaysdata *displaysdata;

  if (flayer->l_width < 10 || flayer->l_height < 5)
    {
      LMsg(0, "Window size too small for displays page");
      return;
    }
  if (InitOverlayPage(sizeof(*displaysdata), &DisplaysLf, 0))
    return;
  displaysdata = (struct displaysdata *)flayer->l_data;
  flayer->l_x = 0;
  flayer->l_y = flayer->l_height - 1;
  displayspage();
}

/*
 * layout of the displays page is as follows:

xterm 80x42      jnweiger@/dev/ttyp4    0(m11)    &rWx
facit 80x24 nb   mlschroe@/dev/ttyhf   11(tcsh)    rwx
xterm 80x42      jnhollma@/dev/ttyp5    0(m11)    &R.x

  |     |    |      |         |         |   |     | ¦___ window permissions 
  |     |    |      |         |         |   |     |      (R. is locked r-only,
  |     |    |      |         |         |   |     |       W has wlock)
  |     |    |      |         |         |   |     |___ Window is shared
  |     |    |      |         |         |   |___ Name/Title of window
  |     |    |      |         |         |___ Number of window
  |     |    |      |         |___ Name of the display (the attached device)
  |     |    |      |___ Username who is logged in at the display
  |     |    |___ Display is in nonblocking mode. Shows 'NB' if obuf is full.
  |     |___ Displays geometry as width x height.
  |___ the terminal type known by screen for this display.
 
 */

static void
displayspage()
{
  int y, l;
  char tbuf[80];
  struct displaysdata *displaysdata;
  struct display *d;
  struct win *w;

  displaysdata = (struct displaysdata *)flayer->l_data;

  LClearAll(flayer, 0);

  leftline("term-type   size         user interface           window", 0);  
  leftline("---------- ------- ---------- ----------------- ----------", 1);
  y = 2;
  
  for (d = displays; d; d = d->d_next)
    {
      w = d->d_fore;

      if (y >= flayer->l_height - 3)
	break;
      sprintf(tbuf, "%-10.10s%4dx%-4d%10.10s@%-16.16s%s",
	      d->d_termname, d->d_width, d->d_height, d->d_user->u_name, 
	      d->d_usertty, 
	      d->d_nonblock ? ((( d->d_obufp - d->d_obuf) > d->d_obufmax) ?
	        "NB" : "nb") : "  ");

      if (w)
	{
          l = 10 - strlen(w->w_title);
	  if (l < 0) 
	    l = 0;
	  sprintf(tbuf + strlen(tbuf), "%3d(%.10s)%*s%c%c%c%c",
		  w->w_number, w->w_title, l, "", 
		  /* w->w_dlist->next */ 0 ? '&' : ' ', 
		  /*
		   * The rwx triple:
		   * -,r,R	no read, read, read only due to foreign wlock
		   * -,.,w,W	no write, write suppressed by foreign wlock,
		   *            write, own wlock
		   * -,x	no execute, execute
		   */
#ifdef MULTIUSER
		  (AclCheckPermWin(d->d_user, ACL_READ, w) ? '-' : 
		   ((w->w_wlock == WLOCK_OFF || d->d_user == w->w_wlockuser) ?
		    'r' : 'R')),
		  (AclCheckPermWin(d->d_user, ACL_READ, w) ? '-' :
		   ((w->w_wlock == WLOCK_OFF) ? 'w' :
		    ((d->d_user == w->w_wlockuser) ? 'W' : 'v'))),
		  (AclCheckPermWin(d->d_user, ACL_READ, w) ? '-' : 'x')
#else
		  'r', 'w', 'x'
#endif
	  );
	}
      leftline(tbuf, y);
      y++;
    }
  sprintf(tbuf,"[Press Space %s Return to end.]",
	  1 ? "to refresh;" : "or");
  centerline(tbuf, flayer->l_height - 2);
  LaySetCursor();
}

static void
DisplaysRedisplayLine(y, xs, xe, isblank)
int y, xs, xe, isblank;
{
  ASSERT(flayer);
  if (y < 0)
    {
      struct displaysdata *displaysdata;

      displaysdata = (struct displaysdata *)flayer->l_data;
      displayspage();
      return;
    }
  if (y != 0 && y != flayer->l_height - 1)
    return;
  if (isblank)
    return;
  LClearArea(flayer, xs, y, xe, y, 0, 0);
  /* To be filled in... */
}

#endif /* MULTI */


/*
**
**    The bindkey help page
**
*/

#ifdef MAPKEYS

extern char *kmap_extras[];
extern int kmap_extras_fl[];
extern struct term term[];

static void BindkeyProcess __P((char **, int *));
static void BindkeyAbort __P((void));
static void BindkeyRedisplayLine __P((int, int, int, int));
static void bindkeypage __P((void));

struct bindkeydata
{
  char *title;
  struct action *tab;
  int pos;
  int last;
  int page;
  int pages;
};

static struct LayFuncs BindkeyLf =
{
  BindkeyProcess,
  BindkeyAbort,
  BindkeyRedisplayLine,
  DefClearLine,
  DefRewrite,
  DefResize,
  DefRestore
};


void
display_bindkey(title, tab)
char *title;
struct action *tab;
{
  struct bindkeydata *bindkeydata;
  int i, n;

  if (flayer->l_height < 6)
    {
      LMsg(0, "Window height too small for bindkey page");
      return;
    }
  if (InitOverlayPage(sizeof(*bindkeydata), &BindkeyLf, 0))
    return;

  bindkeydata = (struct bindkeydata *)flayer->l_data;
  bindkeydata->title = title;
  bindkeydata->tab = tab;
  
  n = 0;
  for (i = 0; i < KMAP_KEYS+KMAP_AKEYS+KMAP_EXT; i++)
    {
      if (tab[i].nr != RC_ILLEGAL)
	n++;
    }
  bindkeydata->pos = 0;
  bindkeydata->page = 1;
  bindkeydata->pages = (n + flayer->l_height - 6) / (flayer->l_height - 5);
  if (bindkeydata->pages == 0)
    bindkeydata->pages = 1;
  flayer->l_x = 0;
  flayer->l_y = flayer->l_height - 1;
  bindkeypage();
}

static void
BindkeyAbort()
{
  LAY_CALL_UP(LRefreshAll(flayer, 0));
  ExitOverlayPage();
}

static void
bindkeypage()
{
  struct bindkeydata *bindkeydata;
  char tbuf[256];
  int del, i, ch, y;
  struct action *act;
  char *xch, *s, *p;

  bindkeydata = (struct bindkeydata *)flayer->l_data;

  LClearAll(flayer, 0);

  sprintf(tbuf, "%s key bindings, page %d of %d.", bindkeydata->title, bindkeydata->page, bindkeydata->pages);
  centerline(tbuf, 0);
  y = 2;
  for (i = bindkeydata->pos; i < KMAP_KEYS+KMAP_AKEYS+KMAP_EXT && y < flayer->l_height - 3; i++)
    {
      p = tbuf;
      act = &bindkeydata->tab[i];
      if (act->nr == RC_ILLEGAL)
	continue;
      xch = "   ";
      if (i < KMAP_KEYS)
	{
	  del = *p++ = ':';
	  s = term[i + T_CAPS].tcname;
	}
      else if (i < KMAP_KEYS+KMAP_AKEYS)
	{
	  del = *p++ = ':';
	  s = term[i + (T_CAPS - T_OCAPS + T_CURSOR)].tcname;
	  xch = "[A]";
	}
      else
	{
	  del = 0;
	  s = kmap_extras[i - (KMAP_KEYS+KMAP_AKEYS)];
	  if (kmap_extras_fl[i - (KMAP_KEYS+KMAP_AKEYS)])
	    xch = "[T]";
	}
      while ((ch = *(unsigned char *)s++))
	p += AddXChar(p, ch);
      if (del)
	*p++ = del;
      *p++ = ' ';
      while (p < tbuf + 15)
        *p++ = ' ';
      sprintf(p, "%s -> ", xch);
      p += 7;
      if (p - tbuf > flayer->l_width - 1)
	{
	  tbuf[flayer->l_width - 2] = '$';
	  tbuf[flayer->l_width - 1] = 0;
	}
      PadStr(tbuf, strlen(tbuf), 0, y);
      AddAction(act, strlen(tbuf), y);
      y++;
    }
  y++;
  bindkeydata->last = i;
  sprintf(tbuf,"[Press Space %s Return to end.]", bindkeydata->page < bindkeydata->pages ? "for next page;" : "or");
  centerline(tbuf, flayer->l_height - 2);
  LaySetCursor();
}
 
static void
BindkeyProcess(ppbuf, plen)
char **ppbuf;
int *plen;
{
  int done = 0;
  struct bindkeydata *bindkeydata;

  bindkeydata = (struct bindkeydata *)flayer->l_data;
  while (!done && *plen > 0)
    {
      switch (**ppbuf)
	{
	case ' ':
	  if (bindkeydata->page < bindkeydata->pages)
	    {
	      bindkeydata->pos = bindkeydata->last;
	      bindkeydata->page++;
	      bindkeypage();
	      break;
	    }
	  /* FALLTHROUGH */
	case '\r':
	case '\n':
	  done = 1;
	  break;
	default:
	  break;
	}
      ++*ppbuf;
      --*plen;
    }
  if (done)
    BindkeyAbort();
}

static void
BindkeyRedisplayLine(y, xs, xe, isblank)
int y, xs, xe, isblank;
{
  if (y < 0)
    {
      bindkeypage();
      return;
    }
  if (y != 0 && y != flayer->l_height - 1)
    return;
  if (!isblank)
    LClearArea(flayer, xs, y, xe, y, 0, 0);
}

#endif /* MAPKEYS */




static void
PadStr(str, n, x, y)
char *str;
int n, x, y;
{
  int l;

  l = strlen(str);
  if (l > n)
    l = n;
  LPutStr(flayer, str, l, &mchar_blank, x, y);
  if (l < n)
    LPutStr(flayer, blank, n - l, &mchar_blank, x + l, y);
}


/*
 * zle_main.c - main routines for line editor
 *
 * This file is part of zsh, the Z shell.
 *
 * Copyright (c) 1992-1996 Paul Falstad
 * All rights reserved.
 *
 * Permission is hereby granted, without written agreement and without
 * license or royalty fees, to use, copy, modify, and distribute this
 * software and to distribute modified versions of this software for any
 * purpose, provided that the above copyright notice and the following
 * two paragraphs appear in all copies of this software.
 *
 * In no event shall Paul Falstad or the Zsh Development Group be liable
 * to any party for direct, indirect, special, incidental, or consequential
 * damages arising out of the use of this software and its documentation,
 * even if Paul Falstad and the Zsh Development Group have been advised of
 * the possibility of such damage.
 *
 * Paul Falstad and the Zsh Development Group specifically disclaim any
 * warranties, including, but not limited to, the implied warranties of
 * merchantability and fitness for a particular purpose.  The software
 * provided hereunder is on an "as is" basis, and Paul Falstad and the
 * Zsh Development Group have no obligation to provide maintenance,
 * support, updates, enhancements, or modifications.
 *
 */

/*
 * Note on output from zle (PWS 1995/05/23):
 *
 * All input and output from the editor should be to/from the file descriptor
 * `SHTTY' and FILE `shout'.  (Normally, the former is used for input
 * operations, reading one key at a time, and the latter for output
 * operations, flushing after each refresh()).  Thus fprintf(shout, ...),
 * putc(..., shout), etc., should be used for output within zle.
 *
 * However, the functions printbind() and printbinding() can be invoked from
 * the builtin bindkey as well as zle, in which case output should be to
 * stdout.  For this purpose, the static variable FILE *bindout exists, which
 * is set to stdout in bin_bindkey() and shout in zleread().
 */

#define ZLEGLOBALS
#define ZLE
#include "zsh.h"

static int emacs_cur_bindtab[256], eofchar, eofsent;
static int viins_cur_bindtab[256];

static Key cky;

static char *keybuf = NULL;
static int buflen;
static long keytimeout;

#ifdef FIONREAD
static int delayzsetterm;
#endif

/* set up terminal */

/**/
void
setterm(void)
{
    struct ttyinfo ti;

#if defined(FIONREAD)
    int val;

    ioctl(SHTTY, FIONREAD, (char *)&val);
    if (val) {
	/*
	 * Problems can occur on some systems when switching from
	 * canonical to non-canonical input.  The former is usually
	 * set while running programmes, but the latter is necessary
	 * for zle.  If there is input in canonical mode, then we
	 * need to read it without setting up the terminal.  Furthermore,
	 * while that input gets processed there may be more input
	 * being typed (i.e. further typeahead).  This means that
	 * we can't set up the terminal for zle *at all* until
	 * we are sure there is no more typeahead to come.  So
	 * if there is typeahead, we set the flag delayzsetterm.
	 * Then getkey() performs another FIONREAD call; if that is
	 * 0, we have finally used up all the typeahead, and it is
	 * safe to alter the terminal, which we do at that point.
	 */
	delayzsetterm = 1;
	return;
    } else
	delayzsetterm = 0;
#endif

/* sanitize the tty */
#ifdef HAS_TIO
    shttyinfo.tio.c_lflag |= ICANON | ECHO;
# ifdef FLUSHO
    shttyinfo.tio.c_lflag &= ~FLUSHO;
# endif
#else				/* not HAS_TIO */
    shttyinfo.sgttyb.sg_flags = (shttyinfo.sgttyb.sg_flags & ~CBREAK) | ECHO;
    shttyinfo.lmodes &= ~LFLUSHO;
#endif

    attachtty(mypgrp);
    ti = shttyinfo;
#ifdef HAS_TIO
    if (unset(FLOWCONTROL))
	ti.tio.c_iflag &= ~IXON;
    ti.tio.c_lflag &= ~(ICANON | ECHO
# ifdef FLUSHO
			| FLUSHO
# endif
	);
# ifdef TAB3
    ti.tio.c_oflag &= ~TAB3;
# else
#  ifdef OXTABS
    ti.tio.c_oflag &= ~OXTABS;
#  else
    ti.tio.c_oflag &= ~XTABS;
#  endif
# endif
    ti.tio.c_oflag |= ONLCR;
    ti.tio.c_cc[VQUIT] =
# ifdef VDISCARD
	ti.tio.c_cc[VDISCARD] =
# endif
# ifdef VSUSP
	ti.tio.c_cc[VSUSP] =
# endif
# ifdef VDSUSP
	ti.tio.c_cc[VDSUSP] =
# endif
# ifdef VSWTCH
	ti.tio.c_cc[VSWTCH] =
# endif
# ifdef VLNEXT
	ti.tio.c_cc[VLNEXT] =
# endif
	VDISABLEVAL;
# if defined(VSTART) && defined(VSTOP)
    if (unset(FLOWCONTROL))
	ti.tio.c_cc[VSTART] = ti.tio.c_cc[VSTOP] = VDISABLEVAL;
# endif
    eofchar = ti.tio.c_cc[VEOF];
    ti.tio.c_cc[VMIN] = 1;
    ti.tio.c_cc[VTIME] = 0;
    ti.tio.c_iflag |= (INLCR | ICRNL);
 /* this line exchanges \n and \r; it's changed back in getkey
	so that the net effect is no change at all inside the shell.
	This double swap is to allow typeahead in common cases, eg.

	% bindkey -s '^J' 'echo foo^M'
	% sleep 10
	echo foo<return>  <--- typed before sleep returns

	The shell sees \n instead of \r, since it was changed by the kernel
	while zsh wasn't looking. Then in getkey() \n is changed back to \r,
	and it sees "echo foo<accept line>", as expected. Without the double
	swap the shell would see "echo foo\n", which is translated to
	"echo fooecho foo<accept line>" because of the binding.
	Note that if you type <line-feed> during the sleep the shell just sees
	\n, which is translated to \r in getkey(), and you just get another
	prompt. For type-ahead to work in ALL cases you have to use
	stty inlcr.
	This workaround is due to Sven Wischnowsky <oberon@cs.tu-berlin.de>.

	Unfortunately it's IMPOSSIBLE to have a general solution if both
	<return> and <line-feed> are mapped to the same character. The shell
	could check if there is input and read it before setting it's own
	terminal modes but if we get a \n we don't know whether to keep it or
	change to \r :-(
	*/

#else				/* not HAS_TIO */
    ti.sgttyb.sg_flags = (ti.sgttyb.sg_flags | CBREAK) & ~ECHO & ~XTABS;
    ti.lmodes &= ~LFLUSHO;
    eofchar = ti.tchars.t_eofc;
    ti.tchars.t_quitc =
	ti.ltchars.t_suspc =
	ti.ltchars.t_flushc =
	ti.ltchars.t_dsuspc = ti.ltchars.t_lnextc = -1;
#endif

#if defined(TTY_NEEDS_DRAINING) && defined(TIOCOUTQ) && defined(HAVE_SELECT)
    if (baud) {			/**/
	int n = 0;

	while ((ioctl(SHTTY, TIOCOUTQ, (char *)&n) >= 0) && n) {
	    struct timeval tv;

	    tv.tv_sec = n / baud;
	    tv.tv_usec = ((n % baud) * 1000000) / baud;
	    select(0, NULL, NULL, NULL, &tv);
	}
    }
#endif

    settyinfo(&ti);
}

static char *kungetbuf;
static int kungetct, kungetsz;

/**/
void
ungetkey(int ch)
{
    if (kungetct == kungetsz)
	kungetbuf = realloc(kungetbuf, kungetsz *= 2);
    kungetbuf[kungetct++] = ch;
}

/**/
void
ungetkeys(char *s, int len)
{
    s += len;
    while (len--)
	ungetkey(*--s);
}

#if defined(pyr) && defined(HAVE_SELECT)
static int
breakread(int fd, char *buf, int n)
{
    fd_set f;

    FD_ZERO(&f);
    FD_SET(fd, &f);
    return (select(fd + 1, (SELECT_ARG_2_T) & f, NULL, NULL, NULL) == -1 ?
	    EOF : read(fd, buf, n));
}

# define read    breakread
#endif

/**/
int
getkey(int keytmout)
{
    char cc;
    unsigned int ret;
    long exp100ths;
    int die = 0, r, icnt = 0;
    int old_errno = errno;

#ifdef HAVE_SELECT
    fd_set foofd;

#else
# ifdef HAS_TIO
    struct ttyinfo ti;

# endif
#endif

    if (kungetct)
	ret = STOUC(kungetbuf[--kungetct]);
    else {
#ifdef FIONREAD
	if (delayzsetterm) {
	    int val;
	    ioctl(SHTTY, FIONREAD, (char *)&val);
	    if (!val)
		setterm();
	}
#endif
	if (keytmout
#ifdef FIONREAD
	    && ! delayzsetterm
#endif
	    ) {
	    if (keytimeout > 500)
		exp100ths = 500;
	    else if (keytimeout > 0)
		exp100ths = keytimeout;
	    else
		exp100ths = 0;
#ifdef HAVE_SELECT
	    if (exp100ths) {
		struct timeval expire_tv;

		expire_tv.tv_sec = exp100ths / 100;
		expire_tv.tv_usec = (exp100ths % 100) * 10000L;
		FD_ZERO(&foofd);
		FD_SET(SHTTY, &foofd);
		if (select(SHTTY+1, (SELECT_ARG_2_T) & foofd,
			   NULL, NULL, &expire_tv) <= 0)
		    return EOF;
	    }
#else
# ifdef HAS_TIO
	    ti = shttyinfo;
	    ti.tio.c_lflag &= ~ICANON;
	    ti.tio.c_cc[VMIN] = 0;
	    ti.tio.c_cc[VTIME] = exp100ths / 10;
#  ifdef HAVE_TERMIOS_H
	    tcsetattr(SHTTY, TCSANOW, &ti.tio);
#  else
	    ioctl(SHTTY, TCSETA, &ti.tio);
#  endif
	    r = read(SHTTY, &cc, 1);
#  ifdef HAVE_TERMIOS_H
	    tcsetattr(SHTTY, TCSANOW, &shttyinfo.tio);
#  else
	    ioctl(SHTTY, TCSETA, &shttyinfo.tio);
#  endif
	    return (r <= 0) ? EOF : cc;
# endif
#endif
	}
	while ((r = read(SHTTY, &cc, 1)) != 1) {
	    if (r == 0) {
		/* The test for IGNOREEOF was added to make zsh ignore ^Ds
		   that were typed while commands are running.  Unfortuantely
		   this caused trouble under at least one system (SunOS 4.1).
		   Here shells that lost their xterm (e.g. if it was killed
		   with -9) didn't fail to read from the terminal but instead
		   happily continued to read EOFs, so that the above read
		   returned with 0, and, with IGNOREEOF set, this caused
		   an infinite loop.  The simple way around this was to add
		   the counter (icnt) so that this happens 20 times and than
		   the shell gives up (yes, this is a bit dirty...). */
		if (isset(IGNOREEOF) && icnt++ < 20)
		    continue;
		stopmsg = 1;
		zexit(1, 0);
	    }
	    icnt = 0;
	    if (errno == EINTR) {
		die = 0;
		if (!errflag && !retflag && !breaks)
		    continue;
		errflag = 0;
		errno = old_errno;
		return EOF;
	    } else if (errno == EWOULDBLOCK) {
		fcntl(0, F_SETFL, 0);
	    } else if (errno == EIO && !die) {
		ret = opts[MONITOR];
		opts[MONITOR] = 1;
		attachtty(mypgrp);
		refresh();	/* kludge! */
		opts[MONITOR] = ret;
		die = 1;
	    } else if (errno != 0) {
		zerr("error on TTY read: %e", NULL, errno);
		stopmsg = 1;
		zexit(1, 0);
	    }
	}
	if (cc == '\r')		/* undo the exchange of \n and \r determined by */
	    cc = '\n';		/* setterm() */
	else if (cc == '\n')
	    cc = '\r';

	ret = STOUC(cc);
    }
    if (vichgflag) {
	if (vichgbufptr == vichgbufsz)
	    vichgbuf = realloc(vichgbuf, vichgbufsz *= 2);
	vichgbuf[vichgbufptr++] = ret;
    }
    errno = old_errno;
    return ret;
}

/* Where to print out bindings:  either stdout, or the zle output shout */
static FILE *bindout;

/* Read a line.  It is returned metafied. */

static int no_restore_tty;

/**/
unsigned char *
zleread(char *lp, char *rp)
{
    unsigned char *s;
    int old_errno = errno;
    int tmout = getiparam("TMOUT");

#ifdef HAVE_SELECT
    long costmult;
    struct timeval tv;
    fd_set foofd;

    baud = getiparam("BAUD");
    costmult = (baud) ? 3840000L / baud : 0;
#endif

    keytimeout = getiparam("KEYTIMEOUT");
    if (!shout) {
	if (SHTTY != -1)
	    init_shout();

	if (!shout)
	    return NULL;
	/* We could be smarter and default to a system read. */

	/* If we just got a new shout, make sure the terminal is set up. */
	if (termflags & TERM_UNKNOWN)
	    init_term();
    }

    fflush(shout);
    fflush(stderr);
    intr();
    insmode = unset(OVERSTRIKE);
    eofsent = 0;
    resetneeded = 0;
    lpmpt = lp;
    rpmpt = rp;
    PERMALLOC {
	histline = curhist;
#ifdef HAVE_SELECT
	FD_ZERO(&foofd);
#endif
	undoing = 1;
	line = (unsigned char *)zalloc((linesz = 256) + 2);
	virangeflag = vilinerange = lastcmd = done = cs = ll = mark = 0;
	curhistline = NULL;
	zmult = 1;
	vibufspec = 0;
	vibufappend = 0;
	gotmult = gotvibufspec = 0;
	bindtab = mainbindtab;
	addedsuffix = complexpect = vichgflag = 0;
	viinsbegin = 0;
	statusline = NULL;
	bindout = shout;		/* always print bindings on terminal */
	if ((s = (unsigned char *)getlinknode(bufstack))) {
	    setline((char *)s);
	    zsfree((char *)s);
	    if (stackcs != -1) {
		cs = stackcs;
		stackcs = -1;
		if (cs > ll)
		    cs = ll;
	    }
	    if (stackhist != -1) {
		histline = stackhist;
		stackhist = -1;
	    }
	}
	initundo();
	if (isset(PROMPTCR))
	    putc('\r', shout);
	if (tmout)
	    alarm(tmout);
	zleactive = 1;
	/* If the history mechanism is set to restore the tty,
	 * don't do so here.  Fixes typeahead clobber problem.
	 * This is handled more cleanly in 3.1.5, but here we
	 * break abstraction to avoid adding a third parameter
	 * to every call to zleread().
	 */
	no_restore_tty = (histdone & HISTFLAG_SETTY);
	resetneeded = 1;
	refresh();
	errflag = retflag = 0;
	while (!done && !errflag) {
	    struct zlecmd *zc;

	    statusline = NULL;
	    bindk = getkeycmd();
	    if (!ll && isfirstln && c == eofchar) {
		eofsent = 1;
		break;
	    }
	    if (bindk != -1) {
		int ce = complexpect;

		zc = zlecmds + bindk;
		if (!(lastcmd & ZLE_ARG)) {
		    zmult = 1;
		    vibufspec = 0;
		    gotmult = gotvibufspec = 0;
		}
		if ((lastcmd & ZLE_UNDO) != (zc->flags & ZLE_UNDO) && undoing)
		    addundo();
		if (bindk != z_sendstring) {
		    if (!(zc->flags & ZLE_MENUCMP))
			invalidatelist();
		    if (!(zc->flags & ZLE_MENUCMP) &&
			addedsuffix && !(zc->flags & ZLE_DELETE) &&
			!((zc->flags & ZLE_INSERT) && c != ' ' &&
			  (c != '/' || addedsuffix > 1 || line[cs-1] != c))) {
			backdel(addedsuffix);
		    }
		    if (!menucmp && !((zc->flags & ZLE_INSERT) && /*{*/
				      complexpect == 2 && c == '}'))
			addedsuffix = 0;
		}
		if (zc->func)
		    (*zc->func) ();
		/* for vi mode, make sure the cursor isn't somewhere illegal */
		if (bindtab == altbindtab && cs > findbol() &&
		    (cs == ll || line[cs] == '\n'))
		    cs--;
		if (ce == complexpect && ce && !menucmp)
		    complexpect = 0;
		if (bindk != z_sendstring)
		    lastcmd = zc->flags;
		if (!(lastcmd & ZLE_UNDO) && undoing)
		    addundo();
	    } else {
		errflag = 1;
		break;
	    }
#ifdef HAVE_SELECT
	    if (baud && !(lastcmd & ZLE_MENUCMP)) {
		FD_SET(SHTTY, &foofd);
		tv.tv_sec = 0;
		if ((tv.tv_usec = cost * costmult) > 500000)
		    tv.tv_usec = 500000;
		if (!kungetct && select(SHTTY+1, (SELECT_ARG_2_T) & foofd,
					NULL, NULL, &tv) <= 0)
		    refresh();
	    } else
#endif
		if (!kungetct)
		    refresh();
	}
	statusline = NULL;
	invalidatelist();
	trashzle();
	zleactive = no_restore_tty = 0;
	alarm(0);
    } LASTALLOC;
    zsfree(curhistline);
    free(lastline);		/* freeundo */
    if (eofsent) {
	free(line);
	line = NULL;
    } else {
	line[ll++] = '\n';
	line = (unsigned char *) metafy((char *) line, ll, META_REALLOC);
    }
    forget_edits();
    errno = old_errno;
    return line;
}

/**/
int
getkeycmd(void)
{
    int ret;
    static int hops = 0;

    cky = NULL;
    if (!keybuf)
	keybuf = (char *)zalloc(buflen = 50);
    keybuf[1] = '\0';

    if ((c = getkey(0)) < 0)
	return -1;
    keybuf[0] = c;
    if ((ret = bindtab[c]) == z_prefix) {
	int lastlen = 0, t0 = 1, firstc = c;
	Key ky;

	if ((cky = (Key) keybindtab->getnode(keybindtab, keybuf))->func == z_undefinedkey)
	    cky = NULL;
	else
	    lastlen = 1;
	if (!c)
	    keybuf[0] = (char)0x80;
	for (;;) {
	    if ((c = getkey(cky ? 1 : 0)) >= 0) {
		if (t0 == buflen - 1)
		    keybuf = (char *)realloc(keybuf, buflen *= 2);
		keybuf[t0++] = (c) ? c : 0x80;
		keybuf[t0] = '\0';
		ky = (Key) keybindtab->getnode(keybindtab, keybuf);
	    } else
		ky = NULL;
	    if (ky) {
		if (ky->func == z_undefinedkey)
		    continue;
		cky = ky;
		if (!ky->prefixct) {
		    ret = ky->func;
		    break;
		}
		lastlen = t0;
	    } else if (cky) {
		ungetkeys(keybuf + lastlen, t0 - lastlen);
		keybuf[lastlen] = '\0';
		if(vichgflag)
		    vichgbufptr -= t0 - lastlen;
		ret = cky->func;
		if (lastlen == 1)
		    keybuf[0] = firstc;
		c = keybuf[lastlen - 1];
		break;
	    } else
		return z_undefinedkey;
	}
    }
    if (ret == z_executenamedcmd && !statusline) {
	while(ret == z_executenamedcmd)
	    ret = executenamedcommand("execute: ");
	if(ret == -1)
	    ret = z_undefinedkey;
	else if(ret != z_executelastnamedcmd)
	    lastnamed = ret;
    }
    if (ret == z_executelastnamedcmd)
	ret = lastnamed;
    if (ret == z_sendstring) {
#define MAXHOPS 20
	if (++hops == MAXHOPS) {
	    zerr("string inserting another one too many times", NULL, 0);
	    hops = 0;
	    return -1;
	}
    } else
	hops = 0;
    if (ret == z_vidigitorbeginningofline)
	ret = (lastcmd & ZLE_DIGIT) ? z_digitargument : z_vibeginningofline;
    return ret;
}

/**/
void
ungetkeycmd(void)
{
    int len = strlen(keybuf);

    ungetkeys(keybuf, len ? len : 1);
}


/**/
void
sendstring(void)
{
    if (!cky)
	cky = (Key) keybindtab->getnode(keybindtab, keybuf);
    if(cky)
	ungetkeys(cky->str, cky->len);
    else
	feep();
}

/**/
Key
makefunckey(int fun)
{
    Key ky = (Key) zcalloc(sizeof *ky);

    ky->func = fun;
    return ky;
}

/* initialize the key bindings */

/**/
void
initkeybindings(void)
{
    Key ky;
    char buf[3], *s;
    int i;

    lastnamed = z_undefinedkey;
    for (i = 0; i < 32; i++)
	viins_cur_bindtab[i] = viinsbind[i];
    for (i = 32; i < 256; i++)
	viins_cur_bindtab[i] = z_selfinsert;
    viins_cur_bindtab[127] = z_backwarddeletechar;
    for (i = 0; i < 128; i++)
	emacs_cur_bindtab[i] = emacsbind[i];
    for (i = 128; i < 256; i++)
	emacs_cur_bindtab[i] = z_selfinsert;

    /* If VISUAL or EDITOR contain the string "vi" when    *
     * initializing the keymaps, then the main keymap will *
     * be bound to vi insert mode by default.              */
    if (((s = zgetenv("VISUAL")) && strstr(s, "vi")) ||
	((s = zgetenv("EDITOR")) && strstr(s, "vi"))) {
	mainbindtab = viins_cur_bindtab;
	keybindtab = vikeybindtab;
    } else {
	mainbindtab = emacs_cur_bindtab;
	keybindtab = emkeybindtab;
    }

    for (i = 0200; i < 0240; i++)
	emacs_cur_bindtab[i] = viins_cur_bindtab[i] = z_undefinedkey;
    for (i = 0; i < 128; i++)
	altbindtab[i] = vicmdbind[i];
    for (i = 128; i < 256; i++)
	altbindtab[i] = emacsbind[i];
    bindtab = mainbindtab;
    if (!kungetbuf)
	kungetbuf = (char *) zalloc(kungetsz = 32);

    emkeybindtab->addnode(emkeybindtab, ztrdup("\33\133"), ky = makefunckey(z_undefinedkey));
    ky->prefixct = 4;
    emkeybindtab->addnode(emkeybindtab, ztrdup("\33\133C"), makefunckey(z_forwardchar));
    emkeybindtab->addnode(emkeybindtab, ztrdup("\33\133D"), makefunckey(z_backwardchar));
    emkeybindtab->addnode(emkeybindtab, ztrdup("\33\133A"), makefunckey(z_uplineorhistory));
    emkeybindtab->addnode(emkeybindtab, ztrdup("\33\133B"), makefunckey(z_downlineorhistory));
    vikeybindtab->addnode(vikeybindtab, ztrdup("\33"), ky = makefunckey(z_vicmdmode));
    ky->prefixct = 4;
    vikeybindtab->addnode(vikeybindtab, ztrdup("\33\133"), ky = makefunckey(z_undefinedkey));
    ky->prefixct = 4;
    vikeybindtab->addnode(vikeybindtab, ztrdup("\33\133C"), makefunckey(z_forwardchar));
    vikeybindtab->addnode(vikeybindtab, ztrdup("\33\133D"), makefunckey(z_backwardchar));
    vikeybindtab->addnode(vikeybindtab, ztrdup("\33\133A"), makefunckey(z_uplineorhistory));
    vikeybindtab->addnode(vikeybindtab, ztrdup("\33\133B"), makefunckey(z_downlineorhistory));
    emkeybindtab->addnode(emkeybindtab, ztrdup("\30"), ky = makefunckey(z_undefinedkey));
    ky->prefixct = 15;
    emkeybindtab->addnode(emkeybindtab, ztrdup("\30*"), makefunckey(z_expandword));
    emkeybindtab->addnode(emkeybindtab, ztrdup("\30g"), makefunckey(z_listexpand));
    emkeybindtab->addnode(emkeybindtab, ztrdup("\30G"), makefunckey(z_listexpand));
    emkeybindtab->addnode(emkeybindtab, ztrdup("\30\16"), makefunckey(z_infernexthistory));
    emkeybindtab->addnode(emkeybindtab, ztrdup("\30\13"), makefunckey(z_killbuffer));
    emkeybindtab->addnode(emkeybindtab, ztrdup("\30\6"), makefunckey(z_vifindnextchar));
    emkeybindtab->addnode(emkeybindtab, ztrdup("\30\17"), makefunckey(z_overwritemode));
    emkeybindtab->addnode(emkeybindtab, ztrdup("\30\25"), makefunckey(z_undo));
    emkeybindtab->addnode(emkeybindtab, ztrdup("\30\26"), makefunckey(z_vicmdmode));
    emkeybindtab->addnode(emkeybindtab, ztrdup("\30\12"), makefunckey(z_vijoin));
    emkeybindtab->addnode(emkeybindtab, ztrdup("\30\2"), makefunckey(z_vimatchbracket));
    emkeybindtab->addnode(emkeybindtab, ztrdup("\30s"), makefunckey(z_historyincrementalsearchforward));
    emkeybindtab->addnode(emkeybindtab, ztrdup("\30r"), makefunckey(z_historyincrementalsearchbackward));
    emkeybindtab->addnode(emkeybindtab, ztrdup("\30u"), makefunckey(z_undo));
    emkeybindtab->addnode(emkeybindtab, ztrdup("\30\30"), makefunckey(z_exchangepointandmark));
    emkeybindtab->addnode(emkeybindtab, ztrdup("\33"), ky = makefunckey(z_undefinedkey));
    ky->prefixct = 4;

    strcpy(buf, "\33q");
    for (i = 128; i < 256; i++)
	if (emacsbind[i] != z_undefinedkey) {
	    buf[1] = i & 0x7f;
	    emkeybindtab->addnode(emkeybindtab, ztrdup(buf), makefunckey(emacsbind[i]));
	    ky->prefixct++;
	}
    stackhist = stackcs = -1;
}

/**/
char *
getkeystring(char *s, int *len, int fromwhere, int *misc)
{
    char *buf = ((fromwhere == 2)
		 ? zalloc(strlen(s) + 1) : alloc(strlen(s) + 1));
    char *t = buf, *u = NULL;
    char svchar = '\0';
    int meta = 0, control = 0;

    for (; *s; s++) {
	if (*s == '\\' && s[1]) {
	    switch (*++s) {
	    case 'a':
#ifdef __STDC__
		*t++ = '\a';
#else
		*t++ = '\07';
#endif
		break;
	    case 'n':
		*t++ = '\n';
		break;
	    case 'b':
		*t++ = '\b';
		break;
	    case 't':
		*t++ = '\t';
		break;
	    case 'v':
		*t++ = '\v';
		break;
	    case 'f':
		*t++ = '\f';
		break;
	    case 'r':
		*t++ = '\r';
		break;
	    case 'E':
		if (!fromwhere) {
		    *t++ = '\\', s--;
		    continue;
		}
	    case 'e':
		*t++ = '\033';
		break;
	    case 'M':
		if (fromwhere) {
		    if (s[1] == '-')
			s++;
		    meta = 1 + control;	/* preserve the order of ^ and meta */
		} else
		    *t++ = '\\', s--;
		continue;
	    case 'C':
		if (fromwhere) {
		    if (s[1] == '-')
			s++;
		    control = 1;
		} else
		    *t++ = '\\', s--;
		continue;
	    case Meta:
		*t++ = '\\', s--;
		break;
	    case 'c':
		if (fromwhere < 2) {
		    *misc = 1;
		    break;
		}
	    default:
		if ((idigit(*s) && *s < '8') || *s == 'x') {
		    if (!fromwhere) {
			if (*s == '0')
			    s++;
			else if (*s != 'x') {
			    *t++ = '\\', s--;
			    continue;
			}
		    }
		    if (s[1] && s[2] && s[3]) {
			svchar = s[3];
			s[3] = '\0';
			u = s;
		    }
		    *t++ = zstrtol(s + (*s == 'x'), &s,
				   (*s == 'x') ? 16 : 8);
		    if (svchar) {
			u[3] = svchar;
			svchar = '\0';
		    }
		    s--;
		} else {
		    if (!fromwhere && *s != '\\')
			*t++ = '\\';
		    *t++ = *s;
		}
		break;
	    }
	} else if (*s == '^' && !control && fromwhere == 2) {
	    control = 1;
	    continue;
	} else if (*s == Meta)
	    *t++ = *++s ^ 32;
	else
	    *t++ = *s;
	if (meta == 2) {
	    t[-1] |= 0x80;
	    meta = 0;
	}
	if (control) {
	    if (t[-1] == '?')
		t[-1] = 0x7f;
	    else
		t[-1] &= 0x9f;
	    control = 0;
	}
	if (meta) {
	    t[-1] |= 0x80;
	    meta = 0;
	}
    }
    *t = '\0';
    *len = t - buf;
    return buf;
}

/**/
void
printbind(char *s, int len, int metanul)
{
    int ch;

    putc('"', bindout);
    metanul |= len == 1;
    while (len--) {
	ch = STOUC(*s++);
	if (ch & 0x80) {
	     if (ch != 0x80 || metanul)
		fprintf(bindout, "\\M-");
	    ch &= 0x7f;
	}
	if (icntrl(ch))
	    switch (ch) {
	    case 0x7f:
		fprintf(bindout, "^?");
		break;
	    default:
		fprintf(bindout, "^%c", (ch | 0x40));
		break;
	} else {
	    if (ch == '\\' || ch == '^' || ch == '"')
		putc('\\', bindout);
	    putc(ch, bindout);
	}
    }
    putc('"', bindout);
}

/**/
void
printbinding(HashNode hn, int printflags)
{
    Key k = (Key) hn;
    int len;

    if (k->func == z_undefinedkey)
	return;
    printbind(k->nam, (len = strlen(k->nam)) ? len : 1, 0);
    putc('\t', bindout);
    if (k->func == z_sendstring) {
	printbind(k->str, k->len, 1);
	putc('\n', bindout);
    } else
	fprintf(bindout, "%s\n", zlecmds[k->func].name);
}

/**/
int
bin_bindkey(char *name, char **argv, char *ops, int junc)
{
    int i, *tab;

    if (ops['v'] && ops['e']) {
	zwarnnam(name, "incompatible options", NULL, 0);
	return 1;
    }
    if (ops['v'] || ops['e'] || ops['d'] || ops['m']) {
	if (*argv) {
	    zwarnnam(name, "too many arguments", NULL, 0);
	    return 1;
	}
	if (ops['d']) {
	    /* empty the hash tables for multi-character bindings */
	    emkeybindtab->emptytable(emkeybindtab);
	    vikeybindtab->emptytable(vikeybindtab);

	    /* reset all key bindings to initial setting */
	    initkeybindings();
	}
	if (ops['e']) {
	    mainbindtab = emacs_cur_bindtab;
	    keybindtab = emkeybindtab;
	} else if (ops['v']) {
	    mainbindtab = viins_cur_bindtab;
	    keybindtab = vikeybindtab;
	}
	if (ops['m'])
	    for (i = 128; i < 256; i++)
		if (mainbindtab[i] == z_selfinsert)
		    mainbindtab[i] = emacsbind[i];
	return 0;
    }
    tab = (ops['a']) ? altbindtab : mainbindtab;

    /* print bindings to stdout */
    bindout = stdout;
    if (!*argv) {
	char buf[2];

	buf[1] = '\0';
	for (i = 0; i < 256; i++) {
	    buf[0] = i;
	    printbind(buf, 1, 1);
	    if (i < 254 && tab[i] == tab[i + 1] && tab[i] == tab[i + 2]) {
		printf(" to ");
		while (tab[i] == tab[i + 1])
		    i++;
		buf[0] = i;
		printbind(buf, 1, 1);
	    }
	    printf("\t%s\n", zlecmds[tab[i]].name);
	}
	scanhashtable(keybindtab, 1, 0, 0, printbinding, 0);
	return 0;
    }
    while (*argv) {
	Key ky = NULL, cur = NULL;
	char *s;
	int func, len, firstzero = 0;

	s = getkeystring(*argv++, &len, 2, NULL);
	if (len > 1) {
	    if (s[0])
		firstzero = 0;
	    else
		firstzero = 1;
	    for (i = 0; i < len; i++)
		if (!s[i])
		    s[i] = (char)0x80;
	}
	if (!*argv || ops['r']) {
	    if (len == 1)
		func = tab[STOUC(*s)];
	    else
		func = (ky = (Key) keybindtab->getnode(keybindtab, s)) ? ky->func
		    : z_undefinedkey;
	    if (func == z_undefinedkey) {
		zwarnnam(name, "in-string is not bound", NULL, 0);
		zfree(s, len);
		return 1;
	    }
	    if (ops['r']) {
		if (len == 1 && func != z_prefix) {
		    tab[STOUC(*s)] = z_undefinedkey;
		    if (func == z_sendstring)
			free(keybindtab->removenode(keybindtab, s));
		} else {
		    if (ky && ky->prefixct) {
			if (ky->func == z_sendstring) {
			    zfree(ky->str, ky->len);
			    ky->str = NULL;
			}
			ky->func = z_undefinedkey;
		    } else
			free(keybindtab->removenode(keybindtab, s));
		    if (len > 1) {
			s[--len] = '\0';
			while (len > 1) {
			    (ky = (Key) keybindtab->getnode(keybindtab, s))->prefixct--;
			    if (!ky->prefixct && ky->func == z_undefinedkey)
				free(keybindtab->removenode(keybindtab, s));
			    s[--len] = '\0';
			}
			(ky = (Key) keybindtab->getnode(keybindtab, s))->prefixct--;
			if (!ky->prefixct) {
			    int *otab = ops['a'] ? mainbindtab : altbindtab;
			    tab[STOUC(*s)] = ky->func;
			    /*
			     * If the bindtab we are not using also
			     * adds this key as a prefix, it must also
			     * be reset.
			     */
			    if (otab[STOUC(*s)] == z_prefix)
				otab[STOUC(*s)] = ky->func;
			    if (ky->func != z_sendstring)
				free(keybindtab->removenode(keybindtab, s));
			}
		    }
		}
		zfree(s, len);
		continue;
	    }
	    if (func == z_sendstring) {
		if (len == 1)
		    ky = (Key) keybindtab->getnode(keybindtab, s);
		printbind(ky->str, ky->len, 1);
		putchar('\n');
	    } else
		printf("%s\n", zlecmds[func].name);
	    zfree(s, len);
	    return 0;
	}
	if (!ops['s']) {
	    for (i = 0; i != ZLECMDCOUNT; i++)
		if (!strcmp(*argv, zlecmds[i].name))
		    break;
	    if (i == ZLECMDCOUNT) {
		zwarnnam(name, "undefined function: %s", *argv, 0);
		zfree(s, len);
		return 1;
	    }
	    func = i;
	} else
	    func = z_sendstring;

	if (len == 1 && tab[STOUC(*s)] != z_prefix) {
	    if (ops['s']) {
		keybindtab->addnode(keybindtab, ztrdup(s), cur = makefunckey(z_sendstring));
	    } else if (tab[STOUC(*s)] == z_sendstring)
		free(keybindtab->removenode(keybindtab, s));
	    tab[STOUC(*s)] = func;
	} else {
	    if (!(cur = (Key) keybindtab->getnode(keybindtab, s))
		|| (cur->func == z_undefinedkey))
		for (i = len - 1; i > 0; i--) {
		    char sav;

		    sav = s[i];
		    s[i] = '\0';
		    if (i == 1 && firstzero)
			*s = '\0';
		    if (!(ky = (Key) keybindtab->getnode(keybindtab, s)))
			keybindtab->addnode(keybindtab, ztrdup(s), ky = makefunckey(z_undefinedkey));
		    ky->prefixct++;
		    s[i] = sav;
		    if (i == 1 && firstzero)
			*s = (char)0x80;
		}
	    if (cur) {
		if (cur->str) {
		    zfree(cur->str, cur->len);
		    cur->str = NULL;
		}
		cur->func = func;
	    } else
		keybindtab->addnode(keybindtab, ztrdup(s), cur = makefunckey(func));
	    if (firstzero)
		*s = 0;
	    if (tab[STOUC(*s)] != z_prefix) {
		cur->func = tab[STOUC(*s)];
		tab[STOUC(*s)] = z_prefix;
	    }
	}
	if (ops['s']) {
	    cur->str = getkeystring(*argv, &cur->len, 2, NULL);
	    cur->str = (char *)realloc(cur->str, cur->len);
	}
	argv++;
	zfree(s, len);
    }
    return 0;
}

/**/
void
freekeynode(HashNode hn)
{
    Key k = (Key) hn;

    zsfree(k->nam);
    if (k->str)
	zfree(k->str, k->len);
    zfree(k, sizeof(struct key));
}

extern int clearflag;

/**/
void
describekeybriefly(void)
{
    int cmd;
    int len;

    if (statusline)
	return;
    clearlist = 1;
    statusline = "Describe key briefly: _";
    statusll = strlen(statusline);
    refresh();
    cmd = getkeycmd();
    statusline = NULL;
    if (cmd < 0)
	return;
    trashzle();
    clearflag = (isset(USEZLE) && !termflags &&
		 (isset(ALWAYSLASTPROMPT) && !gotmult)) ||
	(unset(ALWAYSLASTPROMPT) && gotmult);
    printbind(keybuf, (len = strlen(keybuf)) ? len : 1, 0);
    fprintf(shout, " is ");
    if (cmd == z_sendstring) {
	if (!cky)
	    cky = (Key) keybindtab->getnode(keybindtab, keybuf);
	printbind(cky->str, cky->len, 1);
    }
    else
	fprintf(shout, "%s", zlecmds[cmd].name);
    if (clearflag)
	putc('\r', shout), tcmultout(TCUP, TCMULTUP, nlnct);
    else
	putc('\n', shout);
    showinglist = 0;
}

static int func, funcfound;
#define MAXFOUND 4

static void
printfuncbind(HashNode hn, int printflags)
{
    Key k = (Key) hn;
    int len = strlen(k->nam);

    if (k->func != func || funcfound >= MAXFOUND ||
	((len <= 1) && mainbindtab[*(unsigned char *)k->nam] == z_sendstring))
	return;
    if (!funcfound++)
	fprintf(shout, " on");
    putc(' ', shout);
    printbind(k->nam, len, 0);
}

/**/
void
whereis(void)
{
    int i;

    if ((func = executenamedcommand("Where is: ")) == -1)
	return;
    funcfound = 0;
    trashzle();
    clearflag = (isset(USEZLE) && !termflags &&
		 (isset(ALWAYSLASTPROMPT) && !gotmult)) ||
	(unset(ALWAYSLASTPROMPT) && gotmult);
    if (func == z_selfinsert || func == z_undefinedkey)
	fprintf(shout, "%s is on many keys", zlecmds[func].name);
    else {
	fprintf(shout, "%s is", zlecmds[func].name);
	for (i = 0; funcfound < MAXFOUND && i < 256; i++)
	    if (mainbindtab[i] == func) {
		char ch = i;
		if (!funcfound++)
		    fprintf(shout, " on");
		putc(' ', shout);
		printbind(&ch, 1, 1);
	    }
	if (funcfound < MAXFOUND)
	    scanhashtable(keybindtab, 1, 0, 0, printfuncbind, 0);
	if (!funcfound)
	    fprintf(shout, " not bound to any key");
    }
    if (clearflag)
	putc('\r', shout), tcmultout(TCUP, TCMULTUP, nlnct);
    else
	putc('\n', shout);
    showinglist = 0;
}

/**/
void
trashzle(void)
{
    if (zleactive) {
	/* This refresh() is just to get the main editor display right and *
	 * get the cursor in the right place.  For that reason, we disable *
	 * list display (which would otherwise result in infinite          *
	 * recursion [at least, it would if refresh() didn't have its      *
	 * extra `inlist' check]).                                         */
	int sl = showinglist;
	showinglist = 0;
	refresh();
	showinglist = sl;
	moveto(nlnct, 0);
	if (clearflag && tccan(TCCLEAREOD)) {
	    tcout(TCCLEAREOD);
	    clearflag = listshown = 0;
	}
	if (postedit)
	    fprintf(shout, "%s", postedit);
	fflush(shout);
	resetneeded = 1;
	if (!no_restore_tty)
	    settyinfo(&shttyinfo);
    }
    if (errflag)
	kungetct = 0;
}

/* $XTermId: xtermcap.h,v 1.7 2009/03/15 18:18:11 tom Exp $ */

/************************************************************

Copyright 2007,2009 by Thomas E. Dickey

                        All Rights Reserved

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE ABOVE LISTED COPYRIGHT HOLDER(S) BE LIABLE FOR ANY
CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name(s) of the above copyright
holders shall not be used in advertising or otherwise to promote the
sale, use or other dealings in this Software without prior written
authorization.

********************************************************/

/*
 * Common/useful definitions for XTERM termcap interface.
 */
#ifndef included_xtermcap_h
#define included_xtermcap_h

#include <xterm.h>

#include <ptyx.h>

#ifdef HAVE_TERMCAP_H
#include <termcap.h>
#if defined(NCURSES_VERSION)
	/* The tgetent emulation function in SVr4-style curses implementations
	 * (e.g., ncurses) ignores the buffer, so TERMCAP can't be set from it.
	 * Instead, just use terminfo.
	 */
#undef USE_TERMCAP
#include <curses.h>
#endif
#else
#undef ERR			/* workaround for glibc 2.1.3 */
#include <curses.h>
#ifdef HAVE_NCURSES_TERM_H
#include <ncurses/term.h>
#elif defined(HAVE_TERM_H)
#include <term.h>		/* tgetent() */
#endif /*CYGWIN */
#endif /* HAVE_TERMCAP_H  */

/***====================================================================***/

#ifdef __cplusplus
extern "C" {
#endif

#define	MOD_NONE	1
#define	MOD_SHIFT	1
#define	MOD_ALT		2
#define	MOD_CTRL	4
#define	MOD_META	8

#define MODIFIER_NAME(parm, name) \
	(((parm > MOD_NONE) && ((parm - MOD_NONE) & name)) ? " "#name : "")

/* xtermcap.c */
extern Bool get_termcap(char * /* name */, char * /* buffer */);

#if OPT_TCAP_FKEYS
extern int xtermcapString(XtermWidget /* xw */, int /* keycode */, unsigned /* mask */);
#endif

#if OPT_TCAP_QUERY
extern int xtermcapKeycode(XtermWidget /* xw */, char ** /* params */, unsigned * /* state */, Bool * /* fkey */);
#endif

#ifdef __cplusplus
	}
#endif

#endif	/* included_xtermcap_h */

/*
 *	$Xorg: data.c,v 1.3 2000/08/17 19:55:08 cpqbld Exp $
 */

/* $XFree86: xc/programs/xterm/data.c,v 3.28 2003/10/27 01:07:57 dickey Exp $ */

/*
 * Copyright 2002,2003 by Thomas E. Dickey
 *
 *                         All Rights Reserved
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE ABOVE LISTED COPYRIGHT HOLDER(S) BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * Except as contained in this notice, the name(s) of the above copyright
 * holders shall not be used in advertising or otherwise to promote the
 * sale, use or other dealings in this Software without prior written
 * authorization.
 *
 * Copyright 1987 by Digital Equipment Corporation, Maynard, Massachusetts.
 *
 *                         All Rights Reserved
 *
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose and without fee is hereby granted,
 * provided that the above copyright notice appear in all copies and that
 * both that copyright notice and this permission notice appear in
 * supporting documentation, and that the name of Digital Equipment
 * Corporation not be used in advertising or publicity pertaining to
 * distribution of the software without specific, written prior permission.
 *
 *
 * DIGITAL DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
 * ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
 * DIGITAL BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
 * ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
 * WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
 * ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 */

#include <data.h>

Widget toplevel;		/* top-most widget in xterm */

#if OPT_TEK4014
PtyData *Tbuffer;
Char *Tpushb;
Char *Tpushback;
TekLink *TekRefresh;
TekWidget tekWidget;
Widget tekshellwidget;
int TEKgcFontMask = GCFont;
int T_lastx = -1;
int T_lasty = -1;
int Ttoggled = 0;
jmp_buf Tekend;
#endif

char *ProgramName;

Arg ourTopLevelShellArgs[] =
{
    {XtNallowShellResize, (XtArgVal) TRUE},
    {XtNinput, (XtArgVal) TRUE},
};
int number_ourTopLevelShellArgs = 2;

Bool waiting_for_initial_map;

Atom wm_delete_window;		/* for ICCCM delete window */

XTERM_RESOURCE resource;

PtyData VTbuffer;

jmp_buf VTend;

#ifdef DEBUG
int debug = 0;			/* true causes error messages to be displayed */
#endif /* DEBUG */

XtAppContext app_con;
XtermWidget term;		/* master data structure for client */
char *xterm_name;		/* argv[0] */
int hold_screen;

#if OPT_ZICONBEEP
int zIconBeep;			/* non-zero means beep; see charproc.c for details -IAN! */
Boolean zIconBeep_flagged;	/* True if the icon name has been changed */
#endif /* OPT_ZICONBEEP */

#if OPT_SAME_NAME
Boolean sameName;		/* Don't change the title or icon name if it
				   is the same.  This prevents flicker on the
				   screen at the cost of an extra request to
				   the server */
#endif

int am_slave = -1;		/* set to file-descriptor if we're a slave process */
int max_plus1;
#ifdef VMS
int Select_mask;
int X_mask;
int pty_mask;
#else /* VMS */
fd_set Select_mask;
fd_set X_mask;
fd_set pty_mask;
#endif /* VMS */
char *ptydev;
char *ttydev;

int waitingForTrackInfo = 0;
EventMode eventMode = NORMAL;

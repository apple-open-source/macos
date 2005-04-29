/*
 *	$Xorg: scrollbar.c,v 1.4 2000/08/17 19:55:09 cpqbld Exp $
 */

/* $XFree86: xc/programs/xterm/scrollbar.c,v 3.39 2003/10/20 00:58:55 dickey Exp $ */

/*
 * Copyright 2000-2002,2003 by Thomas E. Dickey
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

#include <xterm.h>

#include <X11/Xatom.h>

#if defined(HAVE_LIB_XAW)
#include <X11/Xaw/Scrollbar.h>
#elif defined(HAVE_LIB_XAW3D)
#include <X11/Xaw3d/Scrollbar.h>
#elif defined(HAVE_LIB_NEXTAW)
#include <X11/neXtaw/Scrollbar.h>
#elif defined(HAVE_LIB_XAWPLUS)
#include <X11/XawPlus/Scrollbar.h>
#endif

#include <data.h>
#include <error.h>
#include <menu.h>
#include <xcharmouse.h>

/* Event handlers */

static void ScrollTextTo PROTO_XT_CALLBACK_ARGS;
static void ScrollTextUpDownBy PROTO_XT_CALLBACK_ARGS;

/* resize the text window for a terminal screen, modifying the
 * appropriate WM_SIZE_HINTS and taking advantage of bit gravity.
 */

static void
ResizeScreen(XtermWidget xw, int min_width, int min_height)
{
    TScreen *screen = &xw->screen;
#ifndef nothack
    XSizeHints sizehints;
    long supp;
#endif
    XtGeometryResult geomreqresult;
    Dimension reqWidth, reqHeight, repWidth, repHeight;
#ifndef NO_ACTIVE_ICON
    struct _vtwin *saveWin = screen->whichVwin;

    /* all units here want to be in the normal font units */
    screen->whichVwin = &screen->fullVwin;
#endif /* NO_ACTIVE_ICON */

    /*
     * I'm going to try to explain, as I understand it, why we
     * have to do XGetWMNormalHints and XSetWMNormalHints here,
     * although I can't guarantee that I've got it right.
     *
     * In a correctly written toolkit program, the Shell widget
     * parses the user supplied geometry argument.  However,
     * because of the way xterm does things, the VT100 widget does
     * the parsing of the geometry option, not the Shell widget.
     * The result of this is that the Shell widget doesn't set the
     * correct window manager hints, and doesn't know that the
     * user has specified a geometry.
     *
     * The XtVaSetValues call below tells the Shell widget to
     * change its hints.  However, since it's confused about the
     * hints to begin with, it doesn't get them all right when it
     * does the SetValues -- it undoes some of what the VT100
     * widget did when it originally set the hints.
     *
     * To fix this, we do the following:
     *
     * 1. Get the sizehints directly from the window, going around
     *    the (confused) shell widget.
     * 2. Call XtVaSetValues to let the shell widget know which
     *    hints have changed.  Note that this may not even be
     *    necessary, since we're going to right ahead after that
     *    and set the hints ourselves, but it's good to put it
     *    here anyway, so that when we finally do fix the code so
     *    that the Shell does the right thing with hints, we
     *    already have the XtVaSetValues in place.
     * 3. We set the sizehints directly, this fixing up whatever
     *    damage was done by the Shell widget during the
     *    XtVaSetValues.
     *
     * Gross, huh?
     *
     * The correct fix is to redo VTRealize, VTInitialize and
     * VTSetValues so that font processing happens early enough to
     * give back responsibility for the size hints to the Shell.
     *
     * Someday, we hope to have time to do this.  Someday, we hope
     * to have time to completely rewrite xterm.
     */

    TRACE(("ResizeScreen(min_width=%d, min_height=%d) xw=%#lx\n",
	   min_width, min_height, (long) xw));

#ifndef nothack
    /*
     * NOTE: If you change the way any of the hints are calculated
     * below, make sure you change the calculation both in the
     * sizehints assignments and in the XtVaSetValues.
     */

    if (!XGetWMNormalHints(screen->display, XtWindow(XtParent(xw)),
			   &sizehints, &supp))
	bzero(&sizehints, sizeof(sizehints));
    sizehints.base_width = min_width;
    sizehints.base_height = min_height;
    sizehints.width_inc = FontWidth(screen);
    sizehints.height_inc = FontHeight(screen);
    sizehints.min_width = sizehints.base_width + sizehints.width_inc;
    sizehints.min_height = sizehints.base_height + sizehints.height_inc;
    sizehints.flags |= (PBaseSize | PMinSize | PResizeInc);
    /* These are obsolete, but old clients may use them */
    sizehints.width = (screen->max_col + 1) * FontWidth(screen)
	+ min_width;
    sizehints.height = (screen->max_row + 1) * FontHeight(screen)
	+ min_height;
#endif

    /*
     * Note: width and height are not set here because they are
     * obsolete.
     */
    XtVaSetValues(XtParent(xw),
		  XtNbaseWidth, min_width,
		  XtNbaseHeight, min_height,
		  XtNwidthInc, FontWidth(screen),
		  XtNheightInc, FontHeight(screen),
		  XtNminWidth, min_width + FontWidth(screen),
		  XtNminHeight, min_height + FontHeight(screen),
		  (XtPointer) 0);

    reqWidth = screen->fullVwin.f_width * (screen->max_col + 1) + min_width;
    reqHeight = screen->fullVwin.f_height * (screen->max_row + 1) + min_height;

    TRACE(("...requesting screensize chars %dx%d, pixels %dx%d\n",
	   (screen->max_row + 1),
	   (screen->max_col + 1),
	   reqHeight, reqWidth));

    geomreqresult = XtMakeResizeRequest((Widget) xw, reqWidth, reqHeight,
					&repWidth, &repHeight);

    if (geomreqresult == XtGeometryAlmost) {
	TRACE(("...almost, retry screensize %dx%d\n", repHeight, repWidth));
	geomreqresult = XtMakeResizeRequest((Widget) xw, repWidth,
					    repHeight, NULL, NULL);
    }
    XSync(screen->display, FALSE);	/* synchronize */
    if (XtAppPending(app_con))
	xevents();

#ifndef nothack
    XSetWMNormalHints(screen->display, XtWindow(XtParent(xw)), &sizehints);
#endif
#ifndef NO_ACTIVE_ICON
    screen->whichVwin = saveWin;
#endif /* NO_ACTIVE_ICON */
}

void
DoResizeScreen(XtermWidget xw)
{
    int border = 2 * xw->screen.border;
    ResizeScreen(xw, border + xw->screen.fullVwin.sb_info.width, border);
}

static Widget
CreateScrollBar(XtermWidget xw, int x, int y, int height)
{
    Widget scrollWidget;
    Arg args[6];

    XtSetArg(args[0], XtNx, x);
    XtSetArg(args[1], XtNy, y);
    XtSetArg(args[2], XtNheight, height);
    XtSetArg(args[3], XtNreverseVideo, xw->misc.re_verse);
    XtSetArg(args[4], XtNorientation, XtorientVertical);
    XtSetArg(args[5], XtNborderWidth, 1);

    scrollWidget = XtCreateWidget("scrollbar", scrollbarWidgetClass,
				  (Widget) xw, args, XtNumber(args));
    XtAddCallback(scrollWidget, XtNscrollProc, ScrollTextUpDownBy, 0);
    XtAddCallback(scrollWidget, XtNjumpProc, ScrollTextTo, 0);
    return (scrollWidget);
}

void
ScrollBarReverseVideo(Widget scrollWidget)
{
    SbInfo *sb = &(term->screen.fullVwin.sb_info);
    Arg args[4];
    Cardinal nargs = XtNumber(args);

    /*
     * Remember the scrollbar's original colors.
     */
    if (sb->rv_cached == False) {
	XtSetArg(args[0], XtNbackground, &(sb->bg));
	XtSetArg(args[1], XtNforeground, &(sb->fg));
	XtSetArg(args[2], XtNborderColor, &(sb->bdr));
	XtSetArg(args[3], XtNborderPixmap, &(sb->bdpix));
	XtGetValues(scrollWidget, args, nargs);
	sb->rv_cached = True;
	sb->rv_active = 0;
    }

    sb->rv_active = !(sb->rv_active);
    XtSetArg(args[!(sb->rv_active)], XtNbackground, sb->bg);
    XtSetArg(args[(sb->rv_active)], XtNforeground, sb->fg);
    nargs = 2;			/* don't set border_pixmap */
    if (sb->bdpix == XtUnspecifiedPixmap) {	/* if not pixmap then pixel */
	if (sb->rv_active) {	/* keep border visible */
	    XtSetArg(args[2], XtNborderColor, args[1].value);
	} else {
	    XtSetArg(args[2], XtNborderColor, sb->bdr);
	}
	nargs = 3;
    }
    XtSetValues(scrollWidget, args, nargs);
}

void
ScrollBarDrawThumb(Widget scrollWidget)
{
    TScreen *screen = &term->screen;
    int thumbTop, thumbHeight, totalHeight;

    thumbTop = screen->topline + screen->savedlines;
    thumbHeight = screen->max_row + 1;
    totalHeight = thumbHeight + screen->savedlines;

    XawScrollbarSetThumb(scrollWidget,
			 ((float) thumbTop) / totalHeight,
			 ((float) thumbHeight) / totalHeight);
}

void
ResizeScrollBar(TScreen * screen)
{
    XtConfigureWidget(
			 screen->scrollWidget,
#ifdef SCROLLBAR_RIGHT
			 (term->misc.useRight)
			 ? (screen->fullVwin.fullwidth -
			    screen->scrollWidget->core.width -
			    screen->scrollWidget->core.border_width)
			 : -1,
#else
			 -1,
#endif
			 -1,
			 screen->scrollWidget->core.width,
			 screen->fullVwin.height + screen->border * 2,
			 screen->scrollWidget->core.border_width);
    ScrollBarDrawThumb(screen->scrollWidget);
}

void
WindowScroll(TScreen * screen, int top)
{
    int i, lines;
    int scrolltop, scrollheight, refreshtop;
    int x = 0;

    if (top < -screen->savedlines)
	top = -screen->savedlines;
    else if (top > 0)
	top = 0;
    if ((i = screen->topline - top) == 0) {
	ScrollBarDrawThumb(screen->scrollWidget);
	return;
    }

    if (screen->cursor_state)
	HideCursor();
    lines = i > 0 ? i : -i;
    if (lines > screen->max_row + 1)
	lines = screen->max_row + 1;
    scrollheight = screen->max_row - lines + 1;
    if (i > 0)
	refreshtop = scrolltop = 0;
    else {
	scrolltop = lines;
	refreshtop = scrollheight;
    }
    x = OriginX(screen);
    scrolling_copy_area(screen, scrolltop, scrollheight, -i);
    screen->topline = top;

    ScrollSelection(screen, i);

    XClearArea(
		  screen->display,
		  VWindow(screen),
		  (int) x,
		  (int) refreshtop * FontHeight(screen) + screen->border,
		  (unsigned) Width(screen),
		  (unsigned) lines * FontHeight(screen),
		  FALSE);
    ScrnRefresh(screen, refreshtop, 0, lines, screen->max_col + 1, False);

    ScrollBarDrawThumb(screen->scrollWidget);
}

void
ScrollBarOn(XtermWidget xw, int init, int doalloc)
{
    TScreen *screen = &xw->screen;
    int i, j, k;

    if (screen->fullVwin.sb_info.width)
	return;

    if (init) {			/* then create it only */
	if (screen->scrollWidget)
	    return;

	/* make it a dummy size and resize later */
	if ((screen->scrollWidget = CreateScrollBar(xw, -1, -1, 5))
	    == NULL) {
	    Bell(XkbBI_MinorError, 0);
	    return;
	}

	return;

    }

    if (!screen->scrollWidget) {
	Bell(XkbBI_MinorError, 0);
	Bell(XkbBI_MinorError, 0);
	return;
    }

    if (doalloc && screen->allbuf) {
	/* FIXME: this is not integrated well with Allocate */
	if ((screen->allbuf =
	     (ScrnBuf) realloc((char *) screen->visbuf,
			       (unsigned) MAX_PTRS * (screen->max_row + 2 +
						      screen->savelines) *
			       sizeof(char *)))
	    == NULL)
	      SysError(ERROR_SBRALLOC);
	screen->visbuf = &screen->allbuf[MAX_PTRS * screen->savelines];
	memmove((char *) screen->visbuf, (char *) screen->allbuf,
		MAX_PTRS * (screen->max_row + 2) * sizeof(char *));
	for (i = k = 0; i < screen->savelines; i++) {
	    k += BUF_HEAD;
	    for (j = BUF_HEAD; j < MAX_PTRS; j++) {
		if ((screen->allbuf[k++] =
		     (Char *) calloc((unsigned) screen->max_col + 1,
				     sizeof(Char))
		    ) == NULL)
		    SysError(ERROR_SBRALLOC2);
	    }
	}
    }

    ResizeScrollBar(screen);
    xtermAddInput(screen->scrollWidget);
    XtRealizeWidget(screen->scrollWidget);
    TRACE_TRANS("scrollbar", screen->scrollWidget);

    screen->fullVwin.sb_info.rv_cached = False;
    screen->fullVwin.sb_info.width = screen->scrollWidget->core.width +
	screen->scrollWidget->core.border_width;

    ScrollBarDrawThumb(screen->scrollWidget);
    DoResizeScreen(xw);

#ifdef SCROLLBAR_RIGHT
    /*
     * Adjust the scrollbar position if we're asked to turn on scrollbars
     * for the first time after the xterm is already running.  That makes
     * the window grow after we've initially configured the scrollbar's
     * position.  (There must be a better way).
     */
    if (term->misc.useRight
	&& screen->fullVwin.fullwidth < term->core.width)
	XtVaSetValues(screen->scrollWidget,
		      XtNx, screen->fullVwin.fullwidth - screen->scrollWidget->core.border_width,
		      (XtPointer) 0);
#endif

    XtMapWidget(screen->scrollWidget);
    update_scrollbar();
    if (screen->visbuf) {
	XClearWindow(screen->display, XtWindow(term));
	Redraw();
    }
}

void
ScrollBarOff(TScreen * screen)
{
    if (!screen->fullVwin.sb_info.width)
	return;
    XtUnmapWidget(screen->scrollWidget);
    screen->fullVwin.sb_info.width = 0;
    DoResizeScreen(term);
    update_scrollbar();
    if (screen->visbuf) {
	XClearWindow(screen->display, XtWindow(term));
	Redraw();
    }
}

/*
 * Toggle the visibility of the scrollbars.
 */
void
ToggleScrollBar(XtermWidget w)
{
    TScreen *screen = &w->screen;

    if (screen->fullVwin.sb_info.width) {
	ScrollBarOff(screen);
    } else {
	ScrollBarOn(w, FALSE, FALSE);
    }
    update_scrollbar();
}

/*ARGSUSED*/
static void
ScrollTextTo(
		Widget scrollbarWidget GCC_UNUSED,
		XtPointer client_data GCC_UNUSED,
		XtPointer call_data)
{
    float *topPercent = (float *) call_data;
    TScreen *screen = &term->screen;
    int thumbTop;		/* relative to first saved line */
    int newTopLine;

/*
   screen->savedlines : Number of offscreen text lines,
   screen->maxrow + 1 : Number of onscreen  text lines,
   screen->topline    : -Number of lines above the last screen->max_row+1 lines
*/

    thumbTop = (int) (*topPercent * (screen->savedlines + screen->max_row + 1));
    newTopLine = thumbTop - screen->savedlines;
    WindowScroll(screen, newTopLine);
}

/*ARGSUSED*/
static void
ScrollTextUpDownBy(
		      Widget scrollbarWidget GCC_UNUSED,
		      XtPointer client_data GCC_UNUSED,
		      XtPointer call_data)
{
    long pixels = (long) call_data;

    TScreen *screen = &term->screen;
    int rowOnScreen, newTopLine;

    rowOnScreen = pixels / FontHeight(screen);
    if (rowOnScreen == 0) {
	if (pixels < 0)
	    rowOnScreen = -1;
	else if (pixels > 0)
	    rowOnScreen = 1;
    }
    newTopLine = screen->topline + rowOnScreen;
    WindowScroll(screen, newTopLine);
}

/*
 * assume that b is lower case and allow plural
 */
static int
specialcmplowerwiths(char *a, char *b, int *modifier)
{
    char ca, cb;

    *modifier = 0;
    if (!a || !b)
	return 0;

    while (1) {
	ca = char2lower(*a);
	cb = *b;
	if (ca != cb || ca == '\0')
	    break;		/* if not eq else both nul */
	a++, b++;
    }
    if (cb != '\0')
	return 0;

    if (ca == 's')
	ca = *++a;

    switch (ca) {
    case '+':
    case '-':
	*modifier = (ca == '-' ? -1 : 1) * atoi(a + 1);
	return 1;

    case '\0':
	return 1;

    default:
	return 0;
    }
}

static long
params_to_pixels(TScreen * screen, String * params, Cardinal n)
{
    int mult = 1;
    char *s;
    int modifier;

    switch (n > 2 ? 2 : n) {
    case 2:
	s = params[1];
	if (specialcmplowerwiths(s, "page", &modifier)) {
	    mult = (screen->max_row + 1 + modifier) * FontHeight(screen);
	} else if (specialcmplowerwiths(s, "halfpage", &modifier)) {
	    mult = ((screen->max_row + 1 + modifier) * FontHeight(screen)) / 2;
	} else if (specialcmplowerwiths(s, "pixel", &modifier)) {
	    mult = 1;
	} else {
	    /* else assume that it is Line */
	    mult = FontHeight(screen);
	}
	mult *= atoi(params[0]);
	break;
    case 1:
	mult = atoi(params[0]) * FontHeight(screen);	/* lines */
	break;
    default:
	mult = screen->scrolllines * FontHeight(screen);
	break;
    }
    return mult;
}

static long
AmountToScroll(Widget gw, String * params, Cardinal nparams)
{
    if (gw != 0) {
	if (IsXtermWidget(gw)) {
	    TScreen *screen = &((XtermWidget) gw)->screen;
	    if (nparams > 2
		&& screen->send_mouse_pos != MOUSE_OFF)
		return 0;
	    return params_to_pixels(screen, params, nparams);
	} else {
	    /*
	     * This may have been the scrollbar widget.  Try its parent, which
	     * would be the VT100 widget.
	     */
	    return AmountToScroll(XtParent(gw), params, nparams);
	}
    }
    return 0;
}

/*ARGSUSED*/
void
HandleScrollForward(
		       Widget gw,
		       XEvent * event GCC_UNUSED,
		       String * params,
		       Cardinal * nparams)
{
    long amount;

    if ((amount = AmountToScroll(gw, params, *nparams)) != 0) {
	ScrollTextUpDownBy(gw, (XtPointer) 0, (XtPointer) amount);
    }
}

/*ARGSUSED*/
void
HandleScrollBack(
		    Widget gw,
		    XEvent * event GCC_UNUSED,
		    String * params,
		    Cardinal * nparams)
{
    long amount;

    if ((amount = -AmountToScroll(gw, params, *nparams)) != 0) {
	ScrollTextUpDownBy(gw, (XtPointer) 0, (XtPointer) amount);
    }
}

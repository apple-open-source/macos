/*
 *	$Xorg: misc.c,v 1.3 2000/08/17 19:55:09 cpqbld Exp $
 */

/* $XFree86: xc/programs/xterm/misc.c,v 3.75 2003/02/06 23:09:43 dickey Exp $ */

/*
 *
 * Copyright 1999-2002,2003 by Thomas E. Dickey
 *
 *                        All Rights Reserved
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

#include <sys/stat.h>
#include <stdio.h>
#include <signal.h>
#include <ctype.h>
#include <pwd.h>
#include <sys/wait.h>

#include <X11/Xatom.h>
#include <X11/cursorfont.h>

#include <X11/Xmu/Error.h>
#include <X11/Xmu/SysUtil.h>
#include <X11/Xmu/WinUtil.h>
#if HAVE_X11_SUNKEYSYM_H
#include <X11/Sunkeysym.h>
#endif

#include <data.h>
#include <error.h>
#include <menu.h>
#include <fontutils.h>
#include <xcharmouse.h>
#include <xstrings.h>
#include <VTparse.h>

#if (XtSpecificationRelease < 6)
#ifndef X_GETTIMEOFDAY
#define X_GETTIMEOFDAY(t) gettimeofday(t,(struct timezone *)0)
#endif
#endif

#ifdef __UNIXOS2__
#define environ gblenvp		/* circumvent a bug */
#endif

#ifdef VMS
#define XTERM_VMS_LOGFILE "SYS$SCRATCH:XTERM_LOG.TXT"
#ifdef ALLOWLOGFILEEXEC
#undef ALLOWLOGFILEEXEC
#endif
#endif /* VMS */

extern char **environ;		/* used in 'xtermSetenv()' */

#if OPT_TEK4014
#define OUR_EVENT(event,Type) \
		(event.type == Type && \
		  (event.xcrossing.window == XtWindow(XtParent(term)) || \
		    (tekWidget && \
		     event.xcrossing.window == XtWindow(XtParent(tekWidget)))))
#else
#define OUR_EVENT(event,Type) \
		(event.type == Type && \
		   (event.xcrossing.window == XtWindow(XtParent(term))))
#endif

static Boolean ChangeColorsRequest(XtermWidget pTerm, int start, char
				   *names, int final);
static void DoSpecialEnterNotify(XEnterWindowEvent * ev);
static void DoSpecialLeaveNotify(XEnterWindowEvent * ev);
static void selectwindow(TScreen * screen, int flag);
static void unselectwindow(TScreen * screen, int flag);
static void Sleep(int msec);

void
do_xevents(void)
{
    register TScreen *screen = &term->screen;

    if (XtAppPending(app_con)
	||
#if defined(VMS) || defined(__VMS)
	screen->display->qlen > 0
#else
	GetBytesAvailable(ConnectionNumber(screen->display)) > 0
#endif
	)
	xevents();
}

void
xevents(void)
{
    XEvent event;
    XtInputMask input_mask;
    register TScreen *screen = &term->screen;

    if (screen->scroll_amt)
	FlushScroll(screen);
    /*
     * process timeouts, relying on the fact that XtAppProcessEvent
     * will process the timeout and return without blockng on the
     * XEvent queue.  Other sources i.e. the pty are handled elsewhere
     * with select().
     */
    while ((input_mask = XtAppPending(app_con)) & XtIMTimer)
	XtAppProcessEvent(app_con, XtIMTimer);
    /*
     * If there's no XEvents, don't wait around...
     */
    if ((input_mask & XtIMXEvent) != XtIMXEvent)
	return;
    do {
	/*
	 * This check makes xterm hang when in mouse hilite tracking mode.
	 * We simply ignore all events except for those not passed down to
	 * this function, e.g., those handled in in_put().
	 */
	if (waitingForTrackInfo) {
	    Sleep(10);
	    return;
	}
	XtAppNextEvent(app_con, &event);
	/*
	 * Hack to get around problems with the toolkit throwing away
	 * eventing during the exclusive grab of the menu popup.  By
	 * looking at the event ourselves we make sure that we can
	 * do the right thing.
	 */
	if (OUR_EVENT(event, EnterNotify))
	    DoSpecialEnterNotify(&event.xcrossing);
	else if (OUR_EVENT(event, LeaveNotify))
	    DoSpecialLeaveNotify(&event.xcrossing);
	else if ((screen->send_mouse_pos == ANY_EVENT_MOUSE
#if OPT_DEC_LOCATOR
		  || screen->send_mouse_pos == DEC_LOCATOR
#endif /* OPT_DEC_LOCATOR */
		 )
		 && event.xany.type == MotionNotify
		 && event.xcrossing.window == XtWindow(term)) {
	    SendMousePosition((Widget) term, &event);
	    continue;
	}

	if (!event.xany.send_event ||
	    screen->allowSendEvents ||
	    ((event.xany.type != KeyPress) &&
	     (event.xany.type != KeyRelease) &&
	     (event.xany.type != ButtonPress) &&
	     (event.xany.type != ButtonRelease)))
	    XtDispatchEvent(&event);
    } while ((input_mask = XtAppPending(app_con)) & XtIMXEvent);
}

Cursor
make_colored_cursor(
		       unsigned cursorindex,	/* index into font */
		       unsigned long fg,	/* pixel value */
		       unsigned long bg)	/* pixel value */
{
    register TScreen *screen = &term->screen;
    Cursor c;
    register Display *dpy = screen->display;

    c = XCreateFontCursor(dpy, cursorindex);
    if (c == (Cursor) 0)
	return (c);

    recolor_cursor(c, fg, bg);
    return (c);
}

/* ARGSUSED */
void
HandleKeyPressed(
		    Widget w GCC_UNUSED,
		    XEvent * event,
		    String * params GCC_UNUSED,
		    Cardinal * nparams GCC_UNUSED)
{
    register TScreen *screen = &term->screen;

#ifdef ACTIVEWINDOWINPUTONLY
    if (w == CURRENT_EMU(screen))
#endif
	Input(&term->keyboard, screen, &event->xkey, False);
}

/* ARGSUSED */
void
HandleEightBitKeyPressed(
			    Widget w GCC_UNUSED,
			    XEvent * event,
			    String * params GCC_UNUSED,
			    Cardinal * nparams GCC_UNUSED)
{
    register TScreen *screen = &term->screen;

#ifdef ACTIVEWINDOWINPUTONLY
    if (w == CURRENT_EMU(screen))
#endif
	Input(&term->keyboard, screen, &event->xkey, True);
}

/* ARGSUSED */
void
HandleStringEvent(
		     Widget w GCC_UNUSED,
		     XEvent * event GCC_UNUSED,
		     String * params,
		     Cardinal * nparams)
{
    register TScreen *screen = &term->screen;

#ifdef ACTIVEWINDOWINPUTONLY
    if (w != CURRENT_EMU(screen))
	return;
#endif

    if (*nparams != 1)
	return;

    if ((*params)[0] == '0' && (*params)[1] == 'x' && (*params)[2] != '\0') {
	Char c, *p;
	Char hexval[2];
	hexval[0] = hexval[1] = 0;
	for (p = (Char *) (*params + 2); (c = *p); p++) {
	    hexval[0] *= 16;
	    if (isupper(c))
		c = tolower(c);
	    if (c >= '0' && c <= '9')
		hexval[0] += c - '0';
	    else if (c >= 'a' && c <= 'f')
		hexval[0] += c - 'a' + 10;
	    else
		break;
	}
	if (c == '\0')
	    StringInput(screen, hexval, 1);
    } else {
	StringInput(screen, (Char *) * params, strlen(*params));
    }
}

/*
 * Rather than sending characters to the host, put them directly into our
 * input queue.  That lets a user have access to any of the control sequences
 * for a key binding.  This is the equivalent of local function key support.
 *
 * NOTE:  This code does not support the hexadecimal kludge used in
 * HandleStringEvent because it prevents us from sending an arbitrary string
 * (but it appears in a lot of examples - so we are stuck with it).  The
 * standard string converter does recognize "\" for newline ("\n") and for
 * octal constants (e.g., "\007" for BEL).  So we assume the user can make do
 * without a specialized converter.  (Don't try to use \000, though).
 */
/* ARGSUSED */
void
HandleInterpret(
		   Widget w GCC_UNUSED,
		   XEvent * event GCC_UNUSED,
		   String * params,
		   Cardinal * param_count)
{
    if (*param_count == 1) {
	char *value = params[0];
	int need = strlen(value);
	int used = usedPtyData(&VTbuffer);
	int have = (VTbuffer.cnt >= 0) ? VTbuffer.cnt : 0;
	int n;

	if (have - used + need < BUF_SIZE) {

	    FlushLog(&term->screen);

	    if (have != 0
		&& used < have) {
		memmove(VTbuffer.ptr + (need - used),
			VTbuffer.ptr,
			VTbuffer.cnt * sizeof(*VTbuffer.ptr));
	    } else {
		initPtyData(&VTbuffer);
		used = 0;
	    }

	    VTbuffer.cnt += (need - used);
	    VTbuffer.ptr -= used;
	    for (n = 0; n < need; n++)
		VTbuffer.ptr[n] = CharOf(value[n]);
	}
    }
}

static void
DoSpecialEnterNotify(register XEnterWindowEvent * ev)
{
    register TScreen *screen = &term->screen;

#ifdef ACTIVEWINDOWINPUTONLY
    if (ev->window == XtWindow(XtParent(CURRENT_EMU(screen))))
#endif
	if (((ev->detail) != NotifyInferior) &&
	    ev->focus &&
	    !(screen->select & FOCUS))
	    selectwindow(screen, INWINDOW);
}

/*ARGSUSED*/
void
HandleEnterWindow(
		     Widget w GCC_UNUSED,
		     XtPointer eventdata GCC_UNUSED,
		     XEvent * event GCC_UNUSED,
		     Boolean * cont GCC_UNUSED)
{
    /* NOP since we handled it above */
}

static void
DoSpecialLeaveNotify(register XEnterWindowEvent * ev)
{
    register TScreen *screen = &term->screen;

#ifdef ACTIVEWINDOWINPUTONLY
    if (ev->window == XtWindow(XtParent(CURRENT_EMU(screen))))
#endif
	if (((ev->detail) != NotifyInferior) &&
	    ev->focus &&
	    !(screen->select & FOCUS))
	    unselectwindow(screen, INWINDOW);
}

/*ARGSUSED*/
void
HandleLeaveWindow(
		     Widget w GCC_UNUSED,
		     XtPointer eventdata GCC_UNUSED,
		     XEvent * event GCC_UNUSED,
		     Boolean * cont GCC_UNUSED)
{
    /* NOP since we handled it above */
}

/*ARGSUSED*/
void
HandleFocusChange(
		     Widget w GCC_UNUSED,
		     XtPointer eventdata GCC_UNUSED,
		     XEvent * ev,
		     Boolean * cont GCC_UNUSED)
{
    register XFocusChangeEvent *event = (XFocusChangeEvent *) ev;
    register TScreen *screen = &term->screen;

    if (event->type == FocusIn)
	selectwindow(screen,
		     (event->detail == NotifyPointer) ? INWINDOW :
		     FOCUS);
    else {
	unselectwindow(screen,
		       (event->detail == NotifyPointer) ? INWINDOW :
		       FOCUS);
	if (screen->grabbedKbd && (event->mode == NotifyUngrab)) {
	    Bell(XkbBI_Info, 100);
	    ReverseVideo(term);
	    screen->grabbedKbd = FALSE;
	    update_securekbd();
	}
    }
}

static void
selectwindow(register TScreen * screen, register int flag)
{
#if OPT_TEK4014
    if (screen->TekEmu) {
	if (!Ttoggled)
	    TCursorToggle(TOGGLE);
	screen->select |= flag;
	if (!Ttoggled)
	    TCursorToggle(TOGGLE);
	return;
    } else
#endif
    {
	if (screen->xic)
	    XSetICFocus(screen->xic);

	if (screen->cursor_state &&
	    (screen->cursor_col != screen->cur_col ||
	     screen->cursor_row != screen->cur_row))
	    HideCursor();
	screen->select |= flag;
	if (screen->cursor_state)
	    ShowCursor();
	return;
    }
}

static void
unselectwindow(register TScreen * screen, register int flag)
{
    if (screen->always_highlight)
	return;

#if OPT_TEK4014
    if (screen->TekEmu) {
	if (!Ttoggled)
	    TCursorToggle(TOGGLE);
	screen->select &= ~flag;
	if (!Ttoggled)
	    TCursorToggle(TOGGLE);
    } else
#endif
    {
	if (screen->xic)
	    XUnsetICFocus(screen->xic);
	screen->select &= ~flag;
	if (screen->cursor_state &&
	    (screen->cursor_col != screen->cur_col ||
	     screen->cursor_row != screen->cur_row))
	    HideCursor();
	if (screen->cursor_state)
	    ShowCursor();
    }
}

static long lastBellTime;	/* in milliseconds */

void
Bell(int which GCC_UNUSED, int percent)
{
    register TScreen *screen = &term->screen;
    struct timeval curtime;
    long now_msecs;

    TRACE(("BELL %d\n", percent));

    /* has enough time gone by that we are allowed to ring
       the bell again? */
    if (screen->bellSuppressTime) {
	if (screen->bellInProgress) {
	    do_xevents();
	    if (screen->bellInProgress) {	/* even after new events? */
		return;
	    }
	}
	X_GETTIMEOFDAY(&curtime);
	now_msecs = 1000 * curtime.tv_sec + curtime.tv_usec / 1000;
	if (lastBellTime != 0 && now_msecs - lastBellTime >= 0 &&
	    now_msecs - lastBellTime < screen->bellSuppressTime) {
	    return;
	}
	lastBellTime = now_msecs;
    }

    if (screen->visualbell)
	VisualBell();
    else
#ifdef XKB
	XkbStdBell(screen->display, TWindow(screen), percent, which);
#else
	XBell(screen->display, percent);
#endif

    if (screen->poponbell)
	XRaiseWindow(screen->display, VShellWindow);

    if (screen->bellSuppressTime) {
	/* now we change a property and wait for the notify event to come
	   back.  If the server is suspending operations while the bell
	   is being emitted (problematic for audio bell), this lets us
	   know when the previous bell has finished */
	Widget w = CURRENT_EMU(screen);
	XChangeProperty(XtDisplay(w), XtWindow(w),
			XA_NOTICE, XA_NOTICE, 8, PropModeAppend, NULL, 0);
	screen->bellInProgress = TRUE;
    }
}

void
VisualBell(void)
{
    register TScreen *screen = &term->screen;
    register Pixel xorPixel = screen->foreground ^ term->core.background_pixel;
    XGCValues gcval;
    GC visualGC;

    gcval.function = GXxor;
    gcval.foreground = xorPixel;
    visualGC = XtGetGC((Widget) term, GCFunction + GCForeground, &gcval);
#if OPT_TEK4014
    if (screen->TekEmu) {
	XFillRectangle(
			  screen->display,
			  TWindow(screen),
			  visualGC,
			  0, 0,
			  (unsigned) TFullWidth(screen),
			  (unsigned) TFullHeight(screen));
	XFlush(screen->display);
	XFillRectangle(
			  screen->display,
			  TWindow(screen),
			  visualGC,
			  0, 0,
			  (unsigned) TFullWidth(screen),
			  (unsigned) TFullHeight(screen));
    } else
#endif
    {
	XFillRectangle(
			  screen->display,
			  VWindow(screen),
			  visualGC,
			  0, 0,
			  (unsigned) FullWidth(screen),
			  (unsigned) FullHeight(screen));
	XFlush(screen->display);
	XFillRectangle(
			  screen->display,
			  VWindow(screen),
			  visualGC,
			  0, 0,
			  (unsigned) FullWidth(screen),
			  (unsigned) FullHeight(screen));
    }
}

/* ARGSUSED */
void
HandleBellPropertyChange(
			    Widget w GCC_UNUSED,
			    XtPointer data GCC_UNUSED,
			    XEvent * ev,
			    Boolean * more GCC_UNUSED)
{
    register TScreen *screen = &term->screen;

    if (ev->xproperty.atom == XA_NOTICE) {
	screen->bellInProgress = FALSE;
    }
}

Window
WMFrameWindow(XtermWidget termw)
{
    Window win_root, win_current, *children;
    Window win_parent = 0;
    unsigned int nchildren;

    win_current = XtWindow(termw);

    /* find the parent which is child of root */
    do {
	if (win_parent)
	    win_current = win_parent;
	XQueryTree((&termw->screen)->display,
		   win_current,
		   &win_root,
		   &win_parent,
		   &children,
		   &nchildren);
	XFree(children);
    } while (win_root != win_parent);

    return win_current;
}

#if OPT_MAXIMIZE
/*ARGSUSED*/
void
HandleDeIconify(
		   Widget gw,
		   XEvent * event GCC_UNUSED,
		   String * params GCC_UNUSED,
		   Cardinal * nparams GCC_UNUSED)
{
    if (IsXtermWidget(gw)) {
	register TScreen *screen = &((XtermWidget) gw)->screen;
	XMapWindow(screen->display, VShellWindow);
    }
}

/*ARGSUSED*/
void
HandleIconify(
		 Widget gw,
		 XEvent * event GCC_UNUSED,
		 String * params GCC_UNUSED,
		 Cardinal * nparams GCC_UNUSED)
{
    if (IsXtermWidget(gw)) {
	register TScreen *screen = &((XtermWidget) gw)->screen;
	XIconifyWindow(screen->display,
		       VShellWindow,
		       DefaultScreen(screen->display));
    }
}

int
QueryMaximize(TScreen * screen, unsigned *width, unsigned *height)
{
    XSizeHints hints;
    long supp = 0;
    Window root_win;
    int root_x = -1;		/* saved co-ordinates */
    int root_y = -1;
    unsigned root_border;
    unsigned root_depth;

    if (XGetGeometry(screen->display,
		     XDefaultRootWindow(screen->display),
		     &root_win,
		     &root_x,
		     &root_y,
		     width,
		     height,
		     &root_border,
		     &root_depth)) {
	TRACE(("QueryMaximize: XGetGeometry position %d,%d size %d,%d border %d\n",
	       root_x,
	       root_y,
	       *width,
	       *height,
	       root_border));
	*width -= (screen->border * 2),
	    *height -= (screen->border * 2);

	hints.flags = PMaxSize;
	if (XGetWMNormalHints(screen->display,
			      VShellWindow,
			      &hints,
			      &supp)
	    && (hints.flags & PMaxSize) != 0) {

	    TRACE(("QueryMaximize: WM hints max_w %#x max_h %#x\n",
		   hints.max_width,
		   hints.max_height));

	    if ((unsigned) hints.max_width < *width)
		*width = hints.max_width;
	    if ((unsigned) hints.max_height < *height)
		*height = hints.max_height;
	}
	return 1;
    }
    return 0;
}

void
RequestMaximize(XtermWidget termw, int maximize)
{
    register TScreen *screen = &termw->screen;
    XWindowAttributes win_attrs;
    unsigned root_width, root_height;

    if (maximize) {

	if (QueryMaximize(screen, &root_width, &root_height)) {

	    if (XGetWindowAttributes(screen->display,
				     WMFrameWindow(termw),
				     &win_attrs)) {

		if (screen->restore_data != True
		    || screen->restore_width != root_width
		    || screen->restore_height != root_height) {
		    screen->restore_data = True;
		    screen->restore_x = win_attrs.x;
		    screen->restore_y = win_attrs.y;
		    screen->restore_width = win_attrs.width;
		    screen->restore_height = win_attrs.height;
		    TRACE(("HandleMaximize: save window position %d,%d size %d,%d\n",
			   screen->restore_x,
			   screen->restore_y,
			   screen->restore_width,
			   screen->restore_height));
		}

		XMoveResizeWindow(screen->display,
				  VShellWindow,
				  0,	/* x */
				  0,	/* y */
				  root_width,
				  root_height);
	    }
	}
    } else {
	if (screen->restore_data) {
	    TRACE(("HandleRestoreSize: position %d,%d size %d,%d\n",
		   screen->restore_x,
		   screen->restore_y,
		   screen->restore_width,
		   screen->restore_height));
	    screen->restore_data = False;
	    XMoveResizeWindow(screen->display,
			      VShellWindow,
			      screen->restore_x,
			      screen->restore_y,
			      screen->restore_width,
			      screen->restore_height);
	}
    }
}

/*ARGSUSED*/
void
HandleMaximize(
		  Widget gw,
		  XEvent * event GCC_UNUSED,
		  String * params GCC_UNUSED,
		  Cardinal * nparams GCC_UNUSED)
{
    if (IsXtermWidget(gw)) {
	RequestMaximize((XtermWidget) gw, 1);
    }
}

/*ARGSUSED*/
void
HandleRestoreSize(
		     Widget gw,
		     XEvent * event GCC_UNUSED,
		     String * params GCC_UNUSED,
		     Cardinal * nparams GCC_UNUSED)
{
    if (IsXtermWidget(gw)) {
	RequestMaximize((XtermWidget) gw, 0);
    }
}
#endif /* OPT_MAXIMIZE */

void
Redraw(void)
{
    register TScreen *screen = &term->screen;
    XExposeEvent event;

    event.type = Expose;
    event.display = screen->display;
    event.x = 0;
    event.y = 0;
    event.count = 0;

    if (VWindow(screen)) {
	event.window = VWindow(screen);
	event.width = term->core.width;
	event.height = term->core.height;
	(*term->core.widget_class->core_class.expose) ((Widget) term,
						       (XEvent *) & event,
						       NULL);
	if (ScrollbarWidth(screen)) {
	    (screen->scrollWidget->core.widget_class->core_class.expose)
		(screen->scrollWidget, (XEvent *) & event, NULL);
	}
    }
#if OPT_TEK4014
    if (TWindow(screen) && screen->Tshow) {
	event.window = TWindow(screen);
	event.width = tekWidget->core.width;
	event.height = tekWidget->core.height;
	TekExpose((Widget) tekWidget, (XEvent *) & event, NULL);
    }
#endif
}

#ifdef VMS
#define TIMESTAMP_FMT "%s%d-%02d-%02d-%02d-%02d-%02d"
#else
#define TIMESTAMP_FMT "%s%d-%02d-%02d.%02d:%02d:%02d"
#endif

void
timestamp_filename(char *dst, const char *src)
{
    time_t tstamp;
    struct tm *tstruct;

    time(&tstamp);
    tstruct = localtime(&tstamp);
    sprintf(dst, TIMESTAMP_FMT,
	    src,
	    tstruct->tm_year + 1900,
	    tstruct->tm_mon + 1,
	    tstruct->tm_mday,
	    tstruct->tm_hour,
	    tstruct->tm_min,
	    tstruct->tm_sec);
}

int
open_userfile(int uid, int gid, char *path, Boolean append)
{
    int fd;
    struct stat sb;

#ifdef VMS
    if ((fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644)) < 0) {
	fprintf(stderr, "%s: cannot open %s: %d:%s\n",
		xterm_name,
		path,
		errno,
		SysErrorMsg(errno));
	return -1;
    }
    chown(path, uid, gid);
#else
    if ((access(path, F_OK) != 0 && (errno != ENOENT))
	|| (!(creat_as(uid, gid, append, path, 0644)))
	|| ((fd = open(path, O_WRONLY | O_APPEND)) < 0)) {
	fprintf(stderr, "%s: cannot open %s: %d:%s\n",
		xterm_name,
		path,
		errno,
		SysErrorMsg(errno));
	return -1;
    }
#endif

    /*
     * Doublecheck that the user really owns the file that we've opened before
     * we do any damage, and that it is not world-writable.
     */
    if (fstat(fd, &sb) < 0
	|| (int) sb.st_uid != uid
	|| (sb.st_mode & 022) != 0) {
	fprintf(stderr, "%s: you do not own %s\n", xterm_name, path);
	close(fd);
	return -1;
    }
    return fd;
}

#ifndef VMS
/*
 * Create a file only if we could with the permissions of the real user id.
 * We could emulate this with careful use of access() and following
 * symbolic links, but that is messy and has race conditions.
 * Forking is messy, too, but we can't count on setreuid() or saved set-uids
 * being available.
 *
 * Note: When called for user logging, we have ensured that the real and
 * effective user ids are the same, so this remains as a convenience function
 * for the debug logs.
 *
 * Returns 1 if we can proceed to open the file in relative safety, 0
 * otherwise.
 */
int
creat_as(int uid, int gid, Boolean append, char *pathname, int mode)
{
    int fd;
    int pid;
    int retval = 0;
    int childstat = 0;
#ifndef HAVE_WAITPID
    int waited;
    SIGNAL_T(*chldfunc) (int);

    chldfunc = signal(SIGCHLD, SIG_DFL);
#endif /* HAVE_WAITPID */

    pid = fork();
    switch (pid) {
    case 0:			/* child */
	setgid(gid);
	setuid(uid);
	fd = open(pathname,
		  O_WRONLY | O_CREAT | (append ? O_APPEND : O_EXCL),
		  mode);
	if (fd >= 0) {
	    close(fd);
	    _exit(0);
	} else
	    _exit(1);
	/* NOTREACHED */
    case -1:			/* error */
	return retval;
    default:			/* parent */
#ifdef HAVE_WAITPID
	while (waitpid(pid, &childstat, 0) < 0) {
#ifdef EINTR
	    if (errno == EINTR)
		continue;
#endif /* EINTR */
#ifdef ERESTARTSYS
	    if (errno == ERESTARTSYS)
		continue;
#endif /* ERESTARTSYS */
	    break;
	}
#else /* HAVE_WAITPID */
	waited = wait(&childstat);
	signal(SIGCHLD, chldfunc);
	/*
	   Since we had the signal handler uninstalled for a while,
	   we might have missed the termination of our screen child.
	   If we can check for this possibility without hanging, do so.
	 */
	do
	    if (waited == term->screen.pid)
		Cleanup(0);
	while ((waited = nonblocking_wait()) > 0) ;
#endif /* HAVE_WAITPID */
#ifndef WIFEXITED
#define WIFEXITED(status) ((status & 0xff) != 0)
#endif
	if (WIFEXITED(childstat))
	    retval = 1;
	return retval;
    }
}
#endif /* !VMS */

#ifdef ALLOWLOGGING

/*
 * Logging is a security hole, since it allows a setuid program to write
 * arbitrary data to an arbitrary file.  So it is disabled by default.
 */

#ifdef ALLOWLOGFILEEXEC
static SIGNAL_T
logpipe(int sig GCC_UNUSED)
{
    register TScreen *screen = &term->screen;

#ifdef SYSV
    (void) signal(SIGPIPE, SIG_IGN);
#endif /* SYSV */
    if (screen->logging)
	CloseLog(screen);
}
#endif /* ALLOWLOGFILEEXEC */

void
StartLog(register TScreen * screen)
{
    static char *log_default;
#ifdef ALLOWLOGFILEEXEC
    register char *cp;
    register int i = 0;
#endif /* ALLOWLOGFILEEXEC */

    if (screen->logging || (screen->inhibit & I_LOG))
	return;
#ifdef VMS			/* file name is fixed in VMS variant */
    screen->logfd = open(XTERM_VMS_LOGFILE,
			 O_CREAT | O_TRUNC | O_APPEND | O_RDWR,
			 0640);
    if (screen->logfd < 0)
	return;			/* open failed */
#else /*VMS */
    if (screen->logfile == NULL || *screen->logfile == 0) {
	if (screen->logfile)
	    free(screen->logfile);
	if (log_default == NULL) {
#if defined(HAVE_GETHOSTNAME) && defined(HAVE_STRFTIME)
	    char log_def_name[512];	/* see sprintf below */
	    char hostname[255 + 1];	/* Internet standard limit (RFC 1035):
					   ``To simplify implementations, the
					   total length of a domain name (i.e.,
					   label octets and label length
					   octets) is restricted to 255 octets
					   or less.'' */
	    char yyyy_mm_dd_hh_mm_ss[4 + 5 * (1 + 2) + 1];
	    time_t now;
	    struct tm *ltm;

	    (void) time(&now);
	    ltm = (struct tm *) localtime(&now);
	    if ((gethostname(hostname, sizeof(hostname)) == 0) &&
		(strftime(yyyy_mm_dd_hh_mm_ss,
			  sizeof(yyyy_mm_dd_hh_mm_ss),
			  "%Y.%m.%d.%H.%M.%S", ltm) > 0)) {
		(void) sprintf(log_def_name, "Xterm.log.%.255s.%.20s.XXXXXX",
			       hostname, yyyy_mm_dd_hh_mm_ss);
	    }
#else
	    const char *log_def_name = "XtermLog.XXXXXX";
#endif
	    if ((log_default = x_strdup(log_def_name)) == NULL)
		return;

	    mktemp(log_default);
	}
	if ((screen->logfile = x_strdup(log_default)) == 0)
	    return;
    }
    if (*screen->logfile == '|') {	/* exec command */
#ifdef ALLOWLOGFILEEXEC
	/*
	 * Warning, enabling this "feature" allows arbitrary programs
	 * to be run.  If ALLOWLOGFILECHANGES is enabled, this can be
	 * done through escape sequences....  You have been warned.
	 */
	int p[2];
	static char *shell;
	register struct passwd *pw;

	if (pipe(p) < 0 || (i = fork()) < 0)
	    return;
	if (i == 0) {		/* child */
	    /*
	     * Close our output (we won't be talking back to the
	     * parent), and redirect our child's output to the
	     * original stderr.
	     */
	    close(p[1]);
	    dup2(p[0], 0);
	    close(p[0]);
	    dup2(fileno(stderr), 1);
	    dup2(fileno(stderr), 2);

	    close(fileno(stderr));
	    close(ConnectionNumber(screen->display));
	    close(screen->respond);

	    if ((((cp = getenv("SHELL")) == NULL || *cp == 0)
		 && ((pw = getpwuid(screen->uid)) == NULL
		     || *(cp = pw->pw_shell) == 0))
		|| (shell = (char *) malloc(strlen(cp) + 1)) == 0)
		shell = "/bin/sh";
	    else
		strcpy(shell, cp);

	    signal(SIGHUP, SIG_DFL);
	    signal(SIGCHLD, SIG_DFL);

	    /* (this is redundant) */
	    setgid(screen->gid);
	    setuid(screen->uid);

	    execl(shell, shell, "-c", &screen->logfile[1], (void *) 0);

	    fprintf(stderr, "%s: Can't exec `%s'\n", xterm_name,
		    &screen->logfile[1]);
	    exit(ERROR_LOGEXEC);
	}
	close(p[0]);
	screen->logfd = p[1];
	signal(SIGPIPE, logpipe);
#else
	Bell(XkbBI_Info, 0);
	Bell(XkbBI_Info, 0);
	return;
#endif
    } else {
	if ((screen->logfd = open_userfile(screen->uid,
					   screen->gid,
					   screen->logfile,
					   (log_default != 0))) < 0)
	    return;
    }
#endif /*VMS */
    screen->logstart = CURRENT_EMU_VAL(screen, Tbuffer->ptr, VTbuffer.ptr);
    screen->logging = TRUE;
    update_logging();
}

void
CloseLog(register TScreen * screen)
{
    if (!screen->logging || (screen->inhibit & I_LOG))
	return;
    FlushLog(screen);
    close(screen->logfd);
    screen->logging = FALSE;
    update_logging();
}

void
FlushLog(register TScreen * screen)
{
    register IChar *cp;
    register int i;

#ifdef VMS			/* avoid logging output loops which otherwise occur sometimes
				   when there is no output and cp/screen->logstart are 1 apart */
    if (!tt_new_output)
	return;
    tt_new_output = FALSE;
#endif /* VMS */
    cp = CURRENT_EMU_VAL(screen, Tbuffer->ptr, VTbuffer.ptr);
    if (screen->logstart != 0
	&& (i = cp - screen->logstart) > 0) {
#if OPT_WIDE_CHARS
	Char temp[80];
	IChar code;
	unsigned n;
	while (i-- > 0) {
	    code = *(screen->logstart)++;
	    if (screen->utf8_mode) {
		n = convertFromUTF8(code & 0xffff, temp);
	    } else {
		temp[0] = code;
		n = 1;
		while (i > 0 && n < sizeof(temp)) {
		    i--;
		    temp[n++] = *(screen->logstart)++;
		}
	    }
	    write(screen->logfd, temp, n);
	}
#else
	write(screen->logfd, (char *) screen->logstart, i);
#endif
    }
    screen->logstart = DecodedData(CURRENT_EMU_VAL(screen, Tbuffer, &VTbuffer));
}

#endif /* ALLOWLOGGING */

/***====================================================================***/

#if OPT_ISO_COLORS
static void
ReportAnsiColorRequest(XtermWidget pTerm, int colornum, int final)
{
    XColor color;
    Colormap cmap = pTerm->core.colormap;
    char buffer[80];

    TRACE(("ReportAnsiColorRequest %d\n", colornum));
    color.pixel = GET_COLOR_RES(pTerm->screen.Acolors[colornum]);
    XQueryColor(term->screen.display, cmap, &color);
    sprintf(buffer, "4;%d;rgb:%04x/%04x/%04x",
	    colornum,
	    color.red,
	    color.green,
	    color.blue);
    unparseputc1(OSC, pTerm->screen.respond);
    unparseputs(buffer, pTerm->screen.respond);
    unparseputc1(final, pTerm->screen.respond);
}

/*
* Find closest color for "def" in "cmap".
* Set "def" to the resulting color.
* Based on Monish Shah's "find_closest_color()" for Vim 6.0,
* modified with ideas from David Tong's "noflash" library.
* Return FALSE if not able to find or allocate a color.
*/
static int
find_closest_color(Display * display, Colormap cmap, XColor * def)
{
    double tmp, distance, closestDistance;
    int i, closest, numFound, cmap_size;
    XColor *colortable;
    XVisualInfo template, *visInfoPtr;
    char *found;
    int attempts;

    template.visualid = XVisualIDFromVisual(DefaultVisual(display,
							  XDefaultScreen(display)));
    visInfoPtr = XGetVisualInfo(display, (long) VisualIDMask,
				&template, &numFound);
    if (numFound < 1) {
	/* FindClosestColor couldn't lookup visual */
	return FALSE;
    }

    cmap_size = visInfoPtr->colormap_size;
    XFree((char *) visInfoPtr);
    colortable = (XColor *) malloc(cmap_size * sizeof(XColor));
    if (!colortable) {
	return FALSE;		/* out of memory */
    }
    found = (char *) calloc(cmap_size, sizeof(char));
    if (!found) {
	free(colortable);
	return FALSE;		/* out of memory */
    }

    for (i = 0; i < cmap_size; i++) {
	colortable[i].pixel = (unsigned long) i;
    }
    XQueryColors(display, cmap, colortable, cmap_size);

    /*
     * Find the color that best approximates the desired one, then
     * try to allocate that color.  If that fails, it must mean that
     * the color was read-write (so we can't use it, since its owner
     * might change it) or else it was already freed.  Try again,
     * over and over again, until something succeeds.
     */
    for (attempts = 0; attempts < cmap_size; attempts++) {
	closestDistance = 1e30;
	closest = 0;
	for (i = 0; i < cmap_size; i++) {
	    if (!found[closest]) {
		/*
		 * Use Euclidean distance in RGB space, weighted by Y (of YIQ)
		 * as the objective function;  this accounts for differences
		 * in the color sensitivity of the eye.
		 */
		tmp = .30 * (((int) def->red) - (int) colortable[i].red);
		distance = tmp * tmp;
		tmp = .61 * (((int) def->green) - (int) colortable[i].green);
		distance += tmp * tmp;
		tmp = .11 * (((int) def->blue) - (int) colortable[i].blue);
		distance += tmp * tmp;
		if (distance < closestDistance) {
		    closest = i;
		    closestDistance = distance;
		}
	    }
	}
	if (XAllocColor(display, cmap, &colortable[closest]) != 0) {
	    *def = colortable[closest];
	    break;
	}
	found[closest] = TRUE;	/* Don't look at this entry again */
    }

    free(colortable);
    free(found);
    if (attempts < cmap_size) {
	return TRUE;		/* Got a closest matching color */
    } else {
	return FALSE;		/* Couldn't allocate a near match */
    }
}

static Boolean
AllocateAnsiColor(XtermWidget pTerm,
		  ColorRes * res,
		  char *spec)
{
    XColor def;
    register TScreen *screen = &pTerm->screen;
    Colormap cmap = pTerm->core.colormap;

    if (XParseColor(screen->display, cmap, spec, &def)
	&& (XAllocColor(screen->display, cmap, &def)
	    || find_closest_color(screen->display, cmap, &def))) {
	SET_COLOR_RES(res, def.pixel);
	TRACE(("AllocateAnsiColor[%d] %s (pixel %#lx)\n",
	       (res - screen->Acolors), spec, def.pixel));
	res->mode = True;
	return (TRUE);
    }
    TRACE(("AllocateAnsiColor %s (failed)\n", spec));
    return (FALSE);
}

#if OPT_COLOR_RES
Pixel
xtermGetColorRes(ColorRes * res)
{
    if (!res->mode) {
	TRACE(("xtermGetColorRes for Acolors[%d]\n",
	       res - term->screen.Acolors));
	if (!AllocateAnsiColor(term, res, res->resource)) {
	    res->value = term->screen.foreground;
	    res->mode = -True;
	    fprintf(stderr,
		    "%s: Cannot allocate color %s\n",
		    xterm_name,
		    res->resource);
	}
    }
    return res->value;
}
#endif

static Boolean
ChangeAnsiColorRequest(
			  XtermWidget pTerm,
			  register char *buf,
			  int final)
{
    char *name;
    int color;
    int r = False;

    TRACE(("ChangeAnsiColorRequest string='%s'\n", buf));

    while (buf && *buf) {
	name = strchr(buf, ';');
	if (name == NULL)
	    break;
	*name = '\0';
	name++;
	color = atoi(buf);
	if (color < 0 || color >= NUM_ANSI_COLORS)
	    break;
	buf = strchr(name, ';');
	if (buf) {
	    *buf = '\0';
	    buf++;
	}
	if (!strcmp(name, "?"))
	    ReportAnsiColorRequest(pTerm, color, final);
	else {
	    TRACE(("ChangeAnsiColor for Acolors[%d]\n", color));
	    if (!AllocateAnsiColor(pTerm, &(pTerm->screen.Acolors[color]), name))
		break;
	    /* FIXME:  free old color somehow?  We aren't for the other color
	     * change style (dynamic colors).
	     */
	    r = True;
	}
    }
    if (r)
	ChangeAnsiColors(pTerm);
    return (r);
}
#else
#define find_closest_color(display, cmap, def) 0
#endif /* OPT_ISO_COLORS */

/***====================================================================***/

void
do_osc(Char * oscbuf, int len GCC_UNUSED, int final)
{
    register TScreen *screen = &(term->screen);
    register int mode;
    register Char *cp;
    int state = 0;
    char *buf = 0;

    /*
     * Lines should be of the form <OSC> number ; string <ST>, however
     * older xterms can accept <BEL> as a final character.  We will respond
     * with the same final character as the application sends to make this
     * work better with shell scripts, which may have trouble reading an
     * <ESC><backslash>, which is the 7-bit equivalent to <ST>.
     */
    mode = 0;
    for (cp = oscbuf; *cp != '\0'; cp++) {
	switch (state) {
	case 0:
	    if (isdigit(*cp)) {
		mode = 10 * mode + (*cp - '0');
		if (mode > 65535)
		    return;
		break;
	    }
	    /* FALLTHRU */
	case 1:
	    if (*cp != ';')
		return;
	    state = 2;
	    break;
	case 2:
	    buf = (char *) cp;
	    state = 3;
	    /* FALLTHRU */
	default:
	    if (ansi_table[CharOf(*cp)] != CASE_PRINT)
		return;
	}
    }
    if (buf == 0)
	return;

    switch (mode) {
    case 0:			/* new icon name and title */
	Changename(buf);
	Changetitle(buf);
	break;

    case 1:			/* new icon name only */
	Changename(buf);
	break;

    case 2:			/* new title only */
	Changetitle(buf);
	break;

    case 3:			/* change X property */
	ChangeXprop(buf);
	break;
#if OPT_ISO_COLORS
    case 4:
	ChangeAnsiColorRequest(term, buf, final);
	break;
#endif
    case 10:
    case 11:
    case 12:
    case 13:
    case 14:
    case 15:
    case 16:
    case 17:
	if (term->misc.dynamicColors)
	    ChangeColorsRequest(term, mode - 10, buf, final);
	break;

    case 30:
    case 31:
	/* reserved for Konsole (Stephan Binner <Stephan.Binner@gmx.de>) */
	break;

#ifdef ALLOWLOGGING
    case 46:			/* new log file */
#ifdef ALLOWLOGFILECHANGES
	/*
	 * Warning, enabling this feature allows people to overwrite
	 * arbitrary files accessible to the person running xterm.
	 */
	if (buf != 0
	    && strcmp(buf, "?")
	    && ((cp = malloc((unsigned) strlen(buf) + 1)) != NULL)) {
	    strcpy(cp, buf);
	    if (screen->logfile)
		free(screen->logfile);
	    screen->logfile = cp;
	    break;
	}
#endif
	Bell(XkbBI_Info, 0);
	Bell(XkbBI_Info, 0);
	break;
#endif /* ALLOWLOGGING */

    case 50:
	if (buf != 0 && !strcmp(buf, "?")) {
	    int num = screen->menu_font_number;

	    unparseputc1(OSC, screen->respond);
	    unparseputs("50", screen->respond);

	    if ((buf = screen->menu_font_names[num]) != 0) {
		unparseputc(';', screen->respond);
		unparseputs(buf, screen->respond);
	    }
	    unparseputc1(final, screen->respond);
	} else {
	    /*
	     * If the font specification is a "#", followed by an
	     * optional sign and optional number, lookup the
	     * corresponding menu font entry.
	     */
	    if (buf != 0 && *buf == '#') {
		int num = screen->menu_font_number;
		int rel = 0;

		if (*++buf == '+') {
		    rel = 1;
		    buf++;
		} else if (*buf == '-') {
		    rel = -1;
		    buf++;
		}

		if (isdigit(CharOf(*buf))) {
		    int val = atoi(buf);
		    if (rel > 0)
			num += val;
		    else if (rel < 0)
			num -= val;
		    else
			num = val;
		} else if (rel) {
		    num += rel;
		} else {
		    num = 0;
		}

		if (num < 0
		    || num > fontMenu_lastBuiltin
		    || (buf = screen->menu_font_names[num]) == 0) {
		    Bell(XkbBI_MinorError, 0);
		    break;
		}
	    }
	    SetVTFont(fontMenu_fontescape, True, VT_FONTSET(buf, NULL, NULL, NULL));
	}
	break;
    case 51:
	/* reserved for Emacs shell (Rob Myoff <mayoff@dqd.com>) */
	break;

	/*
	 * One could write code to send back the display and host names,
	 * but that could potentially open a fairly nasty security hole.
	 */
    }
}

#ifdef SunXK_F36
#define MAX_UDK 37
#else
#define MAX_UDK 35
#endif
static struct {
    char *str;
    int len;
} user_keys[MAX_UDK];

/*
 * Parse one nibble of a hex byte from the OSC string.  We have removed the
 * string-terminator (replacing it with a null), so the only other delimiter
 * that is expected is semicolon.  Ignore other characters (Ray Neuman says
 * "real" terminals accept commas in the string definitions).
 */
static int
udk_value(char **cp)
{
    int c;

    for (;;) {
	if ((c = **cp) != '\0')
	    *cp = *cp + 1;
	if (c == ';' || c == '\0')
	    return -1;
	if (c >= '0' && c <= '9')
	    return c - '0';
	if (c >= 'A' && c <= 'F')
	    return c - 'A' + 10;
	if (c >= 'a' && c <= 'f')
	    return c - 'a' + 10;
    }
}

void
reset_decudk(void)
{
    int n;
    for (n = 0; n < MAX_UDK; n++) {
	if (user_keys[n].str != 0) {
	    free(user_keys[n].str);
	    user_keys[n].str = 0;
	    user_keys[n].len = 0;
	}
    }
}

void
do_dcs(Char * dcsbuf, size_t dcslen)
{
    TScreen *screen = &term->screen;
    char reply[BUFSIZ];
    char *cp = (char *) dcsbuf;
    Bool okay;
    Bool clear_all;
    Bool lock_keys;

    TRACE(("do_dcs(%s:%d)\n", (char *) dcsbuf, dcslen));

    if (dcslen != strlen(cp))
	/* shouldn't have nulls in the string */
	return;

    switch (*cp) {		/* intermediate character, or parameter */
    case '$':			/* DECRQSS */
	okay = True;

	cp++;
	if (*cp++ == 'q') {
	    if (!strcmp(cp, "\"q")) {	/* DECSCA */
		sprintf(reply, "%d%s",
			(screen->protected_mode == DEC_PROTECT)
			&& (term->flags & PROTECTED) ? 1 : 0,
			cp);
	    } else if (!strcmp(cp, "\"p")) {	/* DECSCL */
		sprintf(reply, "%d%s%s",
			(screen->ansi_level ?
			 screen->ansi_level : 1) + 60,
			(screen->ansi_level >= 2)
			? (screen->control_eight_bits
			   ? ";0" : ";1")
			: "",
			cp);
	    } else if (!strcmp(cp, "r")) {	/* DECSTBM */
		sprintf(reply, "%d;%dr",
			screen->top_marg + 1,
			screen->bot_marg + 1);
	    } else if (!strcmp(cp, "m")) {	/* SGR */
		strcpy(reply, "0");
		if (term->flags & BOLD)
		    strcat(reply, ";1");
		if (term->flags & UNDERLINE)
		    strcat(reply, ";4");
		if (term->flags & BLINK)
		    strcat(reply, ";5");
		if (term->flags & INVERSE)
		    strcat(reply, ";7");
		if (term->flags & INVISIBLE)
		    strcat(reply, ";8");
		if_OPT_EXT_COLORS(screen, {
		    if (term->flags & FG_COLOR) {
			if (term->cur_foreground >= 16)
			    sprintf(reply + strlen(reply),
				    ";38;5;%d", term->cur_foreground);
			else
			    sprintf(reply + strlen(reply),
				    ";%d%d",
				    term->cur_foreground >= 8 ? 9 : 3,
				    term->cur_foreground >= 8 ?
				    term->cur_foreground - 8 :
				    term->cur_foreground);
		    }
		    if (term->flags & BG_COLOR) {
			if (term->cur_background >= 16)
			    sprintf(reply + strlen(reply),
				    ";48;5;%d", term->cur_foreground);
			else
			    sprintf(reply + strlen(reply),
				    ";%d%d",
				    term->cur_background >= 8 ? 10 : 4,
				    term->cur_background >= 8 ?
				    term->cur_background - 8 :
				    term->cur_background);
		    }
		});
		if_OPT_ISO_TRADITIONAL_COLORS(screen, {
		    if (term->flags & FG_COLOR)
			sprintf(reply + strlen(reply),
				";%d%d",
				term->cur_foreground >= 8 ? 9 : 3,
				term->cur_foreground >= 8 ?
				term->cur_foreground - 8 :
				term->cur_foreground);
		    if (term->flags & BG_COLOR)
			sprintf(reply + strlen(reply),
				";%d%d",
				term->cur_background >= 8 ? 10 : 4,
				term->cur_background >= 8 ?
				term->cur_background - 8 :
				term->cur_background);
		});
		strcat(reply, "m");
	    } else
		okay = False;

	    unparseputc1(DCS, screen->respond);
	    unparseputc(okay ? '1' : '0', screen->respond);
	    unparseputc('$', screen->respond);
	    unparseputc('r', screen->respond);
	    if (okay)
		cp = reply;
	    unparseputs(cp, screen->respond);
	    unparseputc1(ST, screen->respond);
	} else {
	    unparseputc(CAN, screen->respond);
	}
	break;
#if OPT_TCAP_QUERY
    case '+':
	cp++;
	if (*cp == 'q') {
	    unsigned state;
	    int code;
	    char *tmp;

	    ++cp;
	    code = xtermcapKeycode(cp, &state);
	    unparseputc1(DCS, screen->respond);
	    unparseputc(code >= 0 ? '1' : '0', screen->respond);
	    unparseputc('+', screen->respond);
	    unparseputc('r', screen->respond);
	    for (tmp = cp; *tmp; ++tmp)
		unparseputc(*tmp, screen->respond);
	    if (code >= 0) {
		unparseputc('=', screen->respond);
		screen->tc_query = code;
		/* XK_COLORS is a fake code for the "Co" entry (maximum
		 * number of colors) */
		if (code == XK_COLORS) {
# if OPT_256_COLORS
		    unparseputc('2', screen->respond);
		    unparseputc('5', screen->respond);
		    unparseputc('6', screen->respond);
# elif OPT_88_COLORS
		    unparseputc('8', screen->respond);
		    unparseputc('8', screen->respond);
# else
		    unparseputc('1', screen->respond);
		    unparseputc('6', screen->respond);
# endif
		} else {
		    XKeyEvent event;
		    event.state = state;
		    Input(&(term->keyboard), screen, &event, False);
		}
		screen->tc_query = -1;
	    }
	    unparseputc1(ST, screen->respond);
	}
	break;
#endif
    default:
	if (isdigit(CharOf(*cp))) {	/* digits are DECUDK, otherwise ignore */
	    clear_all = True;
	    lock_keys = True;

	    if (*cp == '0') {
		cp++;
	    } else if (*cp == '1') {
		cp++;
		clear_all = False;
	    }

	    if (*cp == ';')
		cp++;
	    else if (*cp != '|')
		return;

	    if (*cp == '0') {
		cp++;
	    } else if (*cp == '1') {
		cp++;
		lock_keys = False;
	    }

	    if (*cp++ != '|')
		return;

	    if (clear_all)
		reset_decudk();

	    while (*cp) {
		char *base = cp;
		char *str = (char *) malloc(strlen(cp) + 2);
		unsigned key = 0;
		int lo, hi;
		int len = 0;

		while (isdigit(CharOf(*cp)))
		    key = (key * 10) + (*cp++ - '0');
		if (*cp == '/') {
		    cp++;
		    while ((hi = udk_value(&cp)) >= 0
			   && (lo = udk_value(&cp)) >= 0) {
			str[len++] = (hi << 4) | lo;
		    }
		}
		if (len > 0 && key < MAX_UDK) {
		    if (user_keys[key].str != 0)
			free(user_keys[key].str);
		    user_keys[key].str = str;
		    user_keys[key].len = len;
		} else {
		    free(str);
		}
		if (*cp == ';')
		    cp++;
		if (cp == base)	/* badly-formed sequence - bail out */
		    break;
	    }
	}
	break;
    }
}

char *
udk_lookup(int keycode, int *len)
{
    if (keycode >= 0 && keycode < MAX_UDK) {
	*len = user_keys[keycode].len;
	return user_keys[keycode].str;
    }
    return 0;
}

static void
ChangeGroup(String attribute, char *value)
{
    Arg args[1];
    char *name = (value != 0) ? (char *) value : "";

    TRACE(("ChangeGroup(attribute=%s, value=%s)\n", attribute, name));
#if OPT_SAME_NAME
    /* If the attribute isn't going to change, then don't bother... */

    if (sameName) {
	char *buf;
	XtSetArg(args[0], attribute, &buf);
	XtGetValues(toplevel, args, 1);
	if (strcmp(name, buf) == 0)
	    return;
    }
#endif /* OPT_SAME_NAME */

    XtSetArg(args[0], attribute, name);
    XtSetValues(toplevel, args, 1);
}

void
Changename(register char *name)
{
    if (name == 0)
	name = "";
#if OPT_ZICONBEEP		/* If warning should be given then give it */
    if (zIconBeep && zIconBeep_flagged) {
	char *newname = (char *) malloc(strlen(name) + 4 + 1);
	if (!newname) {
	    fprintf(stderr, "malloc failed in Changename\n");
	    return;
	}
	strcpy(newname, "*** ");
	strcat(newname, name);
	ChangeGroup(XtNiconName, newname);
	free(newname);
    } else
#endif /* OPT_ZICONBEEP */
	ChangeGroup(XtNiconName, name);
}

void
Changetitle(register char *name)
{
    ChangeGroup(XtNtitle, name);
}

#define Strlen(s) strlen((char *)(s))

void
ChangeXprop(register char *buf)
{
    Display *dpy = XtDisplay(toplevel);
    Window w = XtWindow(toplevel);
    XTextProperty text_prop;
    Atom aprop;
    Char *pchEndPropName = (Char *) strchr(buf, '=');

    if (pchEndPropName)
	*pchEndPropName = '\0';
    aprop = XInternAtom(dpy, buf, False);
    if (pchEndPropName == NULL) {
	/* no "=value" given, so delete the property */
	XDeleteProperty(dpy, w, aprop);
    } else {
	text_prop.value = pchEndPropName + 1;
	text_prop.encoding = XA_STRING;
	text_prop.format = 8;
	text_prop.nitems = Strlen(text_prop.value);
	XSetTextProperty(dpy, w, &text_prop, aprop);
    }
}

/***====================================================================***/

ScrnColors *pOldColors = NULL;

static Boolean
GetOldColors(XtermWidget pTerm)
{
    int i;
    if (pOldColors == NULL) {
	pOldColors = (ScrnColors *) XtMalloc(sizeof(ScrnColors));
	if (pOldColors == NULL) {
	    fprintf(stderr, "allocation failure in GetOldColors\n");
	    return (FALSE);
	}
	pOldColors->which = 0;
	for (i = 0; i < NCOLORS; i++) {
	    pOldColors->colors[i] = 0;
	    pOldColors->names[i] = NULL;
	}
	GetColors(pTerm, pOldColors);
    }
    return (TRUE);
}

static void
ReportColorRequest(XtermWidget pTerm, int ndx, int final)
{
    XColor color;
    Colormap cmap = pTerm->core.colormap;
    char buffer[80];

    GetOldColors(pTerm);
    color.pixel = pOldColors->colors[ndx];
    TRACE(("ReportColors %d: %#lx\n", ndx, pOldColors->colors[ndx]));
    XQueryColor(term->screen.display, cmap, &color);
    sprintf(buffer, "%d;rgb:%04x/%04x/%04x", ndx + 10,
	    color.red,
	    color.green,
	    color.blue);
    unparseputc1(OSC, pTerm->screen.respond);
    unparseputs(buffer, pTerm->screen.respond);
    unparseputc1(final, pTerm->screen.respond);
}

static Boolean
UpdateOldColors(
		   XtermWidget pTerm GCC_UNUSED,
		   ScrnColors * pNew)
{
    int i;

    /* if we were going to free old colors, this would be the place to
     * do it.   I've decided not to (for now), because it seems likely
     * that we'd have a small set of colors we use over and over, and that
     * we could save some overhead this way.   The only case in which this
     * (clearly) fails is if someone is trying a boatload of colors, in
     * which case they can restart xterm
     */
    for (i = 0; i < NCOLORS; i++) {
	if (COLOR_DEFINED(pNew, i)) {
	    if (pOldColors->names[i] != NULL) {
		XtFree(pOldColors->names[i]);
		pOldColors->names[i] = NULL;
	    }
	    if (pNew->names[i]) {
		pOldColors->names[i] = pNew->names[i];
	    }
	    pOldColors->colors[i] = pNew->colors[i];
	}
    }
    return (TRUE);
}

void
ReverseOldColors(void)
{
    register ScrnColors *pOld = pOldColors;
    Pixel tmpPix;
    char *tmpName;

    if (pOld) {
	/* change text cursor, if necesary */
	if (pOld->colors[TEXT_CURSOR] == pOld->colors[TEXT_FG]) {
	    pOld->colors[TEXT_CURSOR] = pOld->colors[TEXT_BG];
	    if (pOld->names[TEXT_CURSOR]) {
		XtFree(pOldColors->names[TEXT_CURSOR]);
		pOld->names[TEXT_CURSOR] = NULL;
	    }
	    if (pOld->names[TEXT_BG]) {
		tmpName = XtMalloc(strlen(pOld->names[TEXT_BG]) + 1);
		if (tmpName) {
		    strcpy(tmpName, pOld->names[TEXT_BG]);
		    pOld->names[TEXT_CURSOR] = tmpName;
		}
	    }
	}

	EXCHANGE(pOld->colors[TEXT_FG], pOld->colors[TEXT_BG], tmpPix);
	EXCHANGE(pOld->names[TEXT_FG], pOld->names[TEXT_BG], tmpName);

	EXCHANGE(pOld->colors[MOUSE_FG], pOld->colors[MOUSE_BG], tmpPix);
	EXCHANGE(pOld->names[MOUSE_FG], pOld->names[MOUSE_BG], tmpName);

	EXCHANGE(pOld->colors[TEK_FG], pOld->colors[TEK_BG], tmpPix);
	EXCHANGE(pOld->names[TEK_FG], pOld->names[TEK_BG], tmpName);
    }
    return;
}

static Boolean
AllocateColor(
		 XtermWidget pTerm,
		 ScrnColors * pNew,
		 int ndx,
		 char *name)
{
    XColor def;
    register TScreen *screen = &pTerm->screen;
    Colormap cmap = pTerm->core.colormap;
    char *newName;

    if (XParseColor(screen->display, cmap, name, &def)
	&& (XAllocColor(screen->display, cmap, &def)
	    || find_closest_color(screen->display, cmap, &def))
	&& (newName = XtMalloc(strlen(name) + 1)) != 0) {
	SET_COLOR_VALUE(pNew, ndx, def.pixel);
	strcpy(newName, name);
	SET_COLOR_NAME(pNew, ndx, newName);
	TRACE(("AllocateColor #%d: %s (pixel %#lx)\n", ndx, newName, def.pixel));
	return (TRUE);
    }
    TRACE(("AllocateColor #%d: %s (failed)\n", ndx, name));
    return (FALSE);
}

static Boolean
ChangeColorsRequest(
		       XtermWidget pTerm,
		       int start,
		       register char *names,
		       int final)
{
    char *thisName;
    ScrnColors newColors;
    int i, ndx;

    TRACE(("ChangeColorsRequest start=%d, names='%s'\n", start, names));

    if ((pOldColors == NULL)
	&& (!GetOldColors(pTerm))) {
	return (FALSE);
    }
    newColors.which = 0;
    for (i = 0; i < NCOLORS; i++) {
	newColors.names[i] = NULL;
    }
    for (i = start; i < NCOLORS; i++) {
	if (term->misc.re_verse)
	    ndx = OPPOSITE_COLOR(i);
	else
	    ndx = i;
	if ((names == NULL) || (names[0] == '\0')) {
	    newColors.names[ndx] = NULL;
	} else {
	    if (names[0] == ';')
		thisName = NULL;
	    else
		thisName = names;
	    names = strchr(names, ';');
	    if (names != NULL) {
		*names = '\0';
		names++;
	    }
	    if (thisName != 0 && !strcmp(thisName, "?"))
		ReportColorRequest(pTerm, ndx, final);
	    else if (!pOldColors->names[ndx]
		     || (thisName
			 && strcmp(thisName, pOldColors->names[ndx]))) {
		AllocateColor(pTerm, &newColors, ndx, thisName);
	    }
	}
    }

    if (newColors.which == 0)
	return (TRUE);

    ChangeColors(pTerm, &newColors);
    UpdateOldColors(pTerm, &newColors);
    return (TRUE);
}

/***====================================================================***/

#ifndef DEBUG
/* ARGSUSED */
#endif
void
Panic(char *s GCC_UNUSED, int a GCC_UNUSED)
{
#ifdef DEBUG
    if (debug) {
	fprintf(stderr, "%s: PANIC!\t", xterm_name);
	fprintf(stderr, s, a);
	fputs("\r\n", stderr);
	fflush(stderr);
    }
#endif /* DEBUG */
}

char *
SysErrorMsg(int n)
{
    register char *s = strerror(n);
    return s ? s : "unknown error";
}

void
SysError(int i)
{
    static const char *table[] =
    {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
	,"main: ioctl() failed on FIONBIO"	/* 11 */
	,"main: ioctl() failed on F_GETFL"	/* 12 */
	,"main: ioctl() failed on F_SETFL"	/* 13 */
	,"spawn: open() failed on /dev/tty"	/* 14 */
	,"spawn: ioctl() failed on TIOCGETP"	/* 15 */
	,0
	,"spawn: ptsname() failed"	/* 17 */
	,"spawn: open() failed on ptsname"	/* 18 */
	,"spawn: ioctl() failed on I_PUSH/\"ptem\""	/* 19 */
	,"spawn: ioctl() failed on I_PUSH/\"consem\""	/* 20 */
	,"spawn: ioctl() failed on I_PUSH/\"ldterm\""	/* 21 */
	,"spawn: ioctl() failed on I_PUSH/\"ttcompat\""		/* 22 */
	,"spawn: ioctl() failed on TIOCSETP"	/* 23 */
	,"spawn: ioctl() failed on TIOCSETC"	/* 24 */
	,"spawn: ioctl() failed on TIOCSETD"	/* 25 */
	,"spawn: ioctl() failed on TIOCSLTC"	/* 26 */
	,"spawn: ioctl() failed on TIOCLSET"	/* 27 */
	,"spawn: initgroups() failed"	/* 28 */
	,"spawn: fork() failed"	/* 29 */
	,"spawn: exec() failed"	/* 30 */
	,0
	,"get_pty: not enough ptys"	/* 32 */
	,0
	,"waiting for initial map"	/* 34 */
	,"spawn: setuid() failed"	/* 35 */
	,"spawn: can't initialize window"	/* 36 */
	,0, 0, 0, 0, 0, 0, 0, 0, 0
	,"spawn: ioctl() failed on TIOCKSET"	/* 46 */
	,"spawn: ioctl() failed on TIOCKSETC"	/* 47 */
	,"spawn: realloc of ttydev failed"	/* 48 */
	,"luit: command-line malloc failed"	/* 49 */
	,"in_put: select() failed"	/* 50 */
	,0, 0, 0
	,"VTInit: can't initialize window"	/* 54 */
	,0, 0
	,"HandleKeymapChange: malloc failed"	/* 57 */
	,0, 0
	,"Tinput: select() failed"	/* 60 */
	,0, 0, 0
	,"TekInit: can't initialize window"	/* 64 */
	,0, 0, 0, 0, 0, 0
	,"SaltTextAway: malloc() failed"	/* 71 */
	,0, 0, 0, 0, 0, 0, 0, 0
	,"StartLog: exec() failed"	/* 80 */
	,0, 0
	,"xerror: XError event"	/* 83 */
	,"xioerror: X I/O error"	/* 84 */
	,0, 0, 0, 0, 0
	,"Alloc: calloc() failed on base"	/* 90 */
	,"Alloc: calloc() failed on rows"	/* 91 */
	,"ScreenResize: realloc() failed on alt base"	/* 92 */
	,0, 0, 0
	,"ScreenResize: malloc() or realloc() failed"	/* 96 */
	,0, 0, 0, 0, 0
	,"ScrnPointers: malloc/realloc() failed"	/* 102 */
	,0, 0, 0, 0, 0, 0, 0
	,"ScrollBarOn: realloc() failed on base"	/* 110 */
	,"ScrollBarOn: realloc() failed on rows"	/* 111 */
	,0, 0, 0, 0, 0, 0, 0, 0, 0
	,"my_memmove: malloc/realloc failed"	/* 121 */
    };
    int oerrno;

    oerrno = errno;
    fprintf(stderr, "%s: Error %d, errno %d: ", xterm_name, i, oerrno);
    fprintf(stderr, "%s\n", SysErrorMsg(oerrno));
    if ((Cardinal) i < XtNumber(table) && table[i] != 0) {
	fprintf(stderr, "Reason: %s\n", table[i]);
    }
    Cleanup(i);
}

static void
Sleep(int msec)
{
    static struct timeval select_timeout;

    select_timeout.tv_sec = 0;
    select_timeout.tv_usec = msec * 1000;
    select(0, 0, 0, 0, &select_timeout);
}

/*
 * cleanup by sending SIGHUP to client processes
 */
void
Cleanup(int code)
{
    static Boolean cleaning;
    register TScreen *screen;

    if (cleaning) {
	hold_screen = 0;
	return;
    }
    cleaning = TRUE;

    TRACE(("Cleanup %d\n", code));

    screen = &term->screen;

    if (hold_screen) {
	hold_screen = 2;
	while (hold_screen) {
	    xevents();
	    Sleep(10);
	}
    }
#if OPT_SESSION_MGT
    if (resource.sessionMgt) {
	XtVaSetValues(toplevel,
		      XtNjoinSession, False,
		      (XtPointer *) 0);
    }
#endif

    if (screen->pid > 1) {
	(void) kill_process_group(screen->pid, SIGHUP);
    }
    Exit(code);
}

/*
 * sets the value of var to be arg in the Unix 4.2 BSD environment env.
 * Var should end with '=' (bindings are of the form "var=value").
 * This procedure assumes the memory for the first level of environ
 * was allocated using calloc, with enough extra room at the end so not
 * to have to do a realloc().
 */
void
xtermSetenv(register char *var, register char *value)
{
    register int envindex = 0;
    register size_t len = strlen(var);

    TRACE(("xtermSetenv(var=%s, value=%s)\n", var, value));

    while (environ[envindex] != NULL) {
	if (strncmp(environ[envindex], var, len) == 0) {
	    /* found it */
	    environ[envindex] = (char *) malloc((unsigned) len
						+ strlen(value) + 1);
	    strcpy(environ[envindex], var);
	    strcat(environ[envindex], value);
	    return;
	}
	envindex++;
    }

    TRACE(("...expanding env to %d\n", envindex + 1));

    environ[envindex] = (char *) malloc((unsigned) len + strlen(value) + 1);
    (void) strcpy(environ[envindex], var);
    strcat(environ[envindex], value);
    environ[++envindex] = NULL;
}

/*ARGSUSED*/
int
xerror(Display * d, register XErrorEvent * ev)
{
    fprintf(stderr, "%s:  warning, error event received:\n", xterm_name);
    (void) XmuPrintDefaultErrorMessage(d, ev, stderr);
    Exit(ERROR_XERROR);
    return 0;			/* appease the compiler */
}

/*ARGSUSED*/
int
xioerror(Display * dpy)
{
    (void) fprintf(stderr,
		   "%s:  fatal IO error %d (%s) or KillClient on X server \"%s\"\r\n",
		   xterm_name, errno, SysErrorMsg(errno),
		   DisplayString(dpy));

    Exit(ERROR_XIOERROR);
    return 0;			/* appease the compiler */
}

void
xt_error(String message)
{
    (void) fprintf(stderr, "%s Xt error: %s\n", ProgramName, message);
    exit(1);
}

int
XStrCmp(char *s1, char *s2)
{
    if (s1 && s2)
	return (strcmp(s1, s2));
    if (s1 && *s1)
	return (1);
    if (s2 && *s2)
	return (-1);
    return (0);
}

#if OPT_TEK4014
static void
withdraw_window(Display * dpy, Window w, int scr)
{
    TRACE(("withdraw_window %#lx\n", (long) w));
    (void) XmuUpdateMapHints(dpy, w, NULL);
    XWithdrawWindow(dpy, w, scr);
    return;
}
#endif

void
set_vt_visibility(Boolean on)
{
    register TScreen *screen = &term->screen;

    TRACE(("set_vt_visibility(%d)\n", on));
    if (on) {
	if (!screen->Vshow && term) {
	    VTInit();
	    XtMapWidget(XtParent(term));
#if OPT_TOOLBAR
	    /* we need both of these during initialization */
	    XtMapWidget(SHELL_OF(term));
#endif
	    screen->Vshow = TRUE;
	}
    }
#if OPT_TEK4014
    else {
	if (screen->Vshow && term) {
	    withdraw_window(XtDisplay(term),
			    XtWindow(SHELL_OF(term)),
			    XScreenNumberOfScreen(XtScreen(term)));
	    screen->Vshow = FALSE;
	}
    }
    set_vthide_sensitivity();
    set_tekhide_sensitivity();
    update_vttekmode();
    update_tekshow();
    update_vtshow();
#endif
    return;
}

#if OPT_TEK4014
void
set_tek_visibility(Boolean on)
{
    register TScreen *screen = &term->screen;

    TRACE(("set_tek_visibility(%d)\n", on));
    if (on) {
	if (!screen->Tshow && (tekWidget || TekInit())) {
	    Widget tekParent = SHELL_OF(tekWidget);
	    XtRealizeWidget(tekParent);
	    XtMapWidget(tekParent);
	    XtOverrideTranslations(tekParent,
				   XtParseTranslationTable
				   ("<Message>WM_PROTOCOLS: DeleteWindow()"));
	    (void) XSetWMProtocols(XtDisplay(tekParent),
				   XtWindow(tekParent),
				   &wm_delete_window, 1);
	    screen->Tshow = TRUE;
	}
    } else {
	if (screen->Tshow && tekWidget) {
	    withdraw_window(XtDisplay(tekWidget),
			    XtWindow(SHELL_OF(tekWidget)),
			    XScreenNumberOfScreen(XtScreen(tekWidget)));
	    screen->Tshow = FALSE;
	}
    }
    set_tekhide_sensitivity();
    set_vthide_sensitivity();
    update_vtshow();
    update_tekshow();
    update_vttekmode();
    return;
}

void
end_tek_mode(void)
{
    register TScreen *screen = &term->screen;

    if (screen->TekEmu) {
#ifdef ALLOWLOGGING
	if (screen->logging) {
	    FlushLog(screen);
	    screen->logstart = DecodedData(&VTbuffer);
	}
#endif
	longjmp(Tekend, 1);
    }
    return;
}

void
end_vt_mode(void)
{
    register TScreen *screen = &term->screen;

    if (!screen->TekEmu) {
#ifdef ALLOWLOGGING
	if (screen->logging && TekPtyData()) {
	    FlushLog(screen);
	    screen->logstart = DecodedData(Tbuffer);
	}
#endif
	screen->TekEmu = TRUE;
	longjmp(VTend, 1);
    }
    return;
}

void
switch_modes(Bool tovt)		/* if true, then become vt mode */
{
    if (tovt) {
	if (TekRefresh)
	    dorefresh();
	end_tek_mode();		/* WARNING: this does a longjmp... */
    } else {
	end_vt_mode();		/* WARNING: this does a longjmp... */
    }
}

void
hide_vt_window(void)
{
    register TScreen *screen = &term->screen;

    set_vt_visibility(FALSE);
    if (!screen->TekEmu)
	switch_modes(False);	/* switch to tek mode */
}

void
hide_tek_window(void)
{
    register TScreen *screen = &term->screen;

    set_tek_visibility(FALSE);
    TekRefresh = (TekLink *) 0;
    if (screen->TekEmu)
	switch_modes(True);	/* does longjmp to vt mode */
}
#endif /* OPT_TEK4014 */

static const char *
skip_punct(const char *s)
{
    while (*s == '-' || *s == '/' || *s == '+' || *s == '#' || *s == '%') {
	++s;
    }
    return s;
}

static int
cmp_options(const void *a, const void *b)
{
    return strcmp(skip_punct(((const OptionHelp *) a)->opt),
		  skip_punct(((const OptionHelp *) b)->opt));
}

static int
cmp_resources(const void *a, const void *b)
{
    return strcmp(((const XrmOptionDescRec *) a)->option,
		  ((const XrmOptionDescRec *) b)->option);
}

XrmOptionDescRec *
sortedOptDescs(XrmOptionDescRec * descs, Cardinal res_count)
{
    static XrmOptionDescRec *res_array = 0;

    if (res_array == 0) {
	Cardinal j;

	/* make a sorted index to 'resources' */
	res_array = (XrmOptionDescRec *) calloc(res_count, sizeof(*res_array));
	for (j = 0; j < res_count; j++)
	    res_array[j] = descs[j];
	qsort(res_array, res_count, sizeof(*res_array), cmp_resources);
    }
    return res_array;
}

/*
 * The first time this is called, construct sorted index to the main program's
 * list of options, taking into account the on/off options which will be
 * compressed into one token.  It's a lot simpler to do it this way than
 * maintain the list in sorted form with lots of ifdef's.
 */
OptionHelp *
sortedOpts(OptionHelp * options, XrmOptionDescRec * descs, Cardinal numDescs)
{
    static OptionHelp *opt_array = 0;

    if (opt_array == 0) {
	Cardinal opt_count, j, k;
#if OPT_TRACE
	XrmOptionDescRec *res_array = sortedOptDescs(descs, numDescs);
	int code;
	char *mesg;
#else
	(void) k;
	(void) descs;
	(void) numDescs;
#endif

	/* count 'options' and make a sorted index to it */
	for (opt_count = 0; options[opt_count].opt != 0; ++opt_count) {
	    ;
	}
	opt_array = (OptionHelp *) calloc(opt_count + 1, sizeof(OptionHelp));
	for (j = 0; j < opt_count; j++)
	    opt_array[j] = options[j];
	qsort(opt_array, opt_count, sizeof(OptionHelp), cmp_options);

	/* supply the "turn on/off" strings if needed */
#if OPT_TRACE
	for (j = 0; j < opt_count; j++) {
	    if (!strncmp(opt_array[j].opt, "-/+", 3)) {
		char *name = opt_array[j].opt + 3;
		for (k = 0; k < numDescs; ++k) {
		    char *value = res_array[k].value;
		    if (res_array[k].option[0] == '-') {
			code = -1;
		    } else if (res_array[k].option[0] == '+') {
			code = 1;
		    } else {
			code = 0;
		    }
		    if (x_strindex(opt_array[j].desc, "inhibit") != 0)
			code = -code;
		    if (code != 0
			&& res_array[k].value != 0
			&& !strcmp(name, res_array[k].option + 1)) {
			if (((code < 0) && !strcmp(value, "on"))
			    || ((code > 0) && !strcmp(value, "off"))
			    || ((code > 0) && !strcmp(value, "0"))) {
			    mesg = "turn on/off";
			} else {
			    mesg = "turn off/on";
			}
			if (strncmp(mesg, opt_array[j].desc, strlen(mesg))) {
			    if (strncmp(opt_array[j].desc, "turn ", 5)) {
				char *s = malloc(strlen(mesg)
						 + 2
						 + strlen(opt_array[j].desc));
				if (s != 0) {
				    sprintf(s, "%s %s", mesg, opt_array[j].desc);
				    opt_array[j].desc = s;
				}
			    } else {
				TRACE(("OOPS "));
			    }
			}
			TRACE(("%s: %s %s: %s (%s)\n",
			       mesg,
			       res_array[k].option,
			       res_array[k].value,
			       opt_array[j].opt,
			       opt_array[j].desc));
			break;
		    }
		}
	    }
	}
#endif
    }
    return opt_array;
}

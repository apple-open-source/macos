/* $XTermId: misc.c,v 1.520 2011/02/13 21:03:21 tom Exp $ */

/*
 * Copyright 1999-2010,2011 by Thomas E. Dickey
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

#include <version.h>
#include <main.h>
#include <xterm.h>

#include <sys/stat.h>
#include <stdio.h>
#include <signal.h>
#include <ctype.h>
#include <pwd.h>
#include <sys/wait.h>

#include <X11/keysym.h>
#include <X11/Xatom.h>
#include <X11/cursorfont.h>
#include <X11/Xlocale.h>

#include <X11/Xmu/Error.h>
#include <X11/Xmu/SysUtil.h>
#include <X11/Xmu/WinUtil.h>
#include <X11/Xmu/Xmu.h>
#if HAVE_X11_SUNKEYSYM_H
#include <X11/Sunkeysym.h>
#endif

#ifdef HAVE_LANGINFO_CODESET
#include <langinfo.h>
#endif

#include <xutf8.h>

#include <data.h>
#include <error.h>
#include <menu.h>
#include <fontutils.h>
#include <xcharmouse.h>
#include <xstrings.h>
#include <xtermcap.h>
#include <VTparse.h>

#include <assert.h>

#if (XtSpecificationRelease < 6)
#ifndef X_GETTIMEOFDAY
#define X_GETTIMEOFDAY(t) gettimeofday(t,(struct timezone *)0)
#endif
#endif

#ifdef VMS
#define XTERM_VMS_LOGFILE "SYS$SCRATCH:XTERM_LOG.TXT"
#ifdef ALLOWLOGFILEEXEC
#undef ALLOWLOGFILEEXEC
#endif
#endif /* VMS */

#if OPT_TEK4014
#define OUR_EVENT(event,Type) \
		(event.type == Type && \
		  (event.xcrossing.window == XtWindow(XtParent(xw)) || \
		    (tekWidget && \
		     event.xcrossing.window == XtWindow(XtParent(tekWidget)))))
#else
#define OUR_EVENT(event,Type) \
		(event.type == Type && \
		   (event.xcrossing.window == XtWindow(XtParent(xw))))
#endif

static Cursor make_hidden_cursor(XtermWidget);

#if OPT_EXEC_XTERM
/* Like readlink(2), but returns a malloc()ed buffer, or NULL on
   error; adapted from libc docs */
static char *
Readlink(const char *filename)
{
    char *buf = NULL;
    size_t size = 100;
    int n;

    for (;;) {
	buf = TypeRealloc(char, size, buf);
	memset(buf, 0, size);

	n = (int) readlink(filename, buf, size);
	if (n < 0) {
	    free(buf);
	    return NULL;
	}

	if ((unsigned) n < size) {
	    return buf;
	}

	size *= 2;
    }
}
#endif /* OPT_EXEC_XTERM */

static void
Sleep(int msec)
{
    static struct timeval select_timeout;

    select_timeout.tv_sec = 0;
    select_timeout.tv_usec = msec * 1000;
    select(0, 0, 0, 0, &select_timeout);
}

static void
selectwindow(TScreen * screen, int flag)
{
    TRACE(("selectwindow(%d) flag=%d\n", screen->select, flag));

#if OPT_TEK4014
    if (TEK4014_ACTIVE(term)) {
	if (!Ttoggled)
	    TCursorToggle(tekWidget, TOGGLE);
	screen->select |= flag;
	if (!Ttoggled)
	    TCursorToggle(tekWidget, TOGGLE);
    } else
#endif
    {
	if (screen->xic)
	    XSetICFocus(screen->xic);

	if (screen->cursor_state && CursorMoved(screen))
	    HideCursor();
	screen->select |= flag;
	if (screen->cursor_state)
	    ShowCursor();
    }
    GetScrollLock(screen);
}

static void
unselectwindow(TScreen * screen, int flag)
{
    TRACE(("unselectwindow(%d) flag=%d\n", screen->select, flag));

    if (screen->hide_pointer) {
	screen->hide_pointer = False;
	xtermDisplayCursor(term);
    }

    if (!screen->always_highlight) {
#if OPT_TEK4014
	if (TEK4014_ACTIVE(term)) {
	    if (!Ttoggled)
		TCursorToggle(tekWidget, TOGGLE);
	    screen->select &= ~flag;
	    if (!Ttoggled)
		TCursorToggle(tekWidget, TOGGLE);
	} else
#endif
	{
	    if (screen->xic)
		XUnsetICFocus(screen->xic);

	    screen->select &= ~flag;
	    if (screen->cursor_state && CursorMoved(screen))
		HideCursor();
	    if (screen->cursor_state)
		ShowCursor();
	}
    }
}

static void
DoSpecialEnterNotify(XtermWidget xw, XEnterWindowEvent * ev)
{
    TScreen *screen = TScreenOf(xw);

    TRACE(("DoSpecialEnterNotify(%d)\n", screen->select));
    TRACE_FOCUS(xw, ev);
    if (((ev->detail) != NotifyInferior) &&
	ev->focus &&
	!(screen->select & FOCUS))
	selectwindow(screen, INWINDOW);
}

static void
DoSpecialLeaveNotify(XtermWidget xw, XEnterWindowEvent * ev)
{
    TScreen *screen = TScreenOf(xw);

    TRACE(("DoSpecialLeaveNotify(%d)\n", screen->select));
    TRACE_FOCUS(xw, ev);
    if (((ev->detail) != NotifyInferior) &&
	ev->focus &&
	!(screen->select & FOCUS))
	unselectwindow(screen, INWINDOW);
}

#ifndef XUrgencyHint
#define XUrgencyHint (1L << 8)	/* X11R5 does not define */
#endif

static void
setXUrgency(XtermWidget xw, Bool enable)
{
    TScreen *screen = TScreenOf(xw);

    if (screen->bellIsUrgent) {
	XWMHints *h = XGetWMHints(screen->display, VShellWindow(xw));
	if (h != 0) {
	    if (enable && !(screen->select & FOCUS)) {
		h->flags |= XUrgencyHint;
	    } else {
		h->flags &= ~XUrgencyHint;
	    }
	    XSetWMHints(screen->display, VShellWindow(xw), h);
	}
    }
}

void
do_xevents(void)
{
    TScreen *screen = TScreenOf(term);

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
xtermDisplayCursor(XtermWidget xw)
{
    TScreen *screen = TScreenOf(xw);

    if (screen->Vshow) {
	if (screen->hide_pointer) {
	    TRACE(("Display hidden_cursor\n"));
	    XDefineCursor(screen->display, VWindow(screen), screen->hidden_cursor);
	} else {
	    TRACE(("Display pointer_cursor\n"));
	    recolor_cursor(screen,
			   screen->pointer_cursor,
			   T_COLOR(screen, MOUSE_FG),
			   T_COLOR(screen, MOUSE_BG));
	    XDefineCursor(screen->display, VWindow(screen), screen->pointer_cursor);
	}
    }
}

void
xtermShowPointer(XtermWidget xw, Bool enable)
{
    static int tried = -1;
    TScreen *screen = TScreenOf(xw);

#if OPT_TEK4014
    if (TEK4014_SHOWN(xw))
	enable = True;
#endif

    /*
     * Whether we actually hide the pointer depends on the pointer-mode and
     * the mouse-mode:
     */
    if (!enable) {
	switch (screen->pointer_mode) {
	case pNever:
	    enable = True;
	    break;
	case pNoMouse:
	    if (screen->send_mouse_pos != MOUSE_OFF)
		enable = True;
	    break;
	case pAlways:
	    break;
	}
    }

    if (enable) {
	if (screen->hide_pointer) {
	    screen->hide_pointer = False;
	    xtermDisplayCursor(xw);
	    switch (screen->send_mouse_pos) {
	    case ANY_EVENT_MOUSE:
		break;
	    default:
		MotionOff(screen, xw);
		break;
	    }
	}
    } else if (!(screen->hide_pointer) && (tried <= 0)) {
	if (screen->hidden_cursor == 0) {
	    screen->hidden_cursor = make_hidden_cursor(xw);
	}
	if (screen->hidden_cursor == 0) {
	    tried = 1;
	} else {
	    tried = 0;
	    screen->hide_pointer = True;
	    xtermDisplayCursor(xw);
	    MotionOn(screen, xw);
	}
    }
}

void
xevents(void)
{
    XtermWidget xw = term;
    TScreen *screen = TScreenOf(xw);
    XEvent event;
    XtInputMask input_mask;

    if (need_cleanup)
	Cleanup(0);

    if (screen->scroll_amt)
	FlushScroll(xw);
    /*
     * process timeouts, relying on the fact that XtAppProcessEvent
     * will process the timeout and return without blockng on the
     * XEvent queue.  Other sources i.e., the pty are handled elsewhere
     * with select().
     */
    while ((input_mask = XtAppPending(app_con)) != 0) {
	if (input_mask & XtIMTimer)
	    XtAppProcessEvent(app_con, (XtInputMask) XtIMTimer);
#if OPT_SESSION_MGT
	/*
	 * Session management events are alternative input events. Deal with
	 * them in the same way.
	 */
	else if (input_mask & XtIMAlternateInput)
	    XtAppProcessEvent(app_con, (XtInputMask) XtIMAlternateInput);
#endif
	else
	    break;
    }

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
	if (screen->waitingForTrackInfo) {
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
	if (OUR_EVENT(event, EnterNotify)) {
	    DoSpecialEnterNotify(xw, &event.xcrossing);
	} else if (OUR_EVENT(event, LeaveNotify)) {
	    DoSpecialLeaveNotify(xw, &event.xcrossing);
	} else if ((screen->send_mouse_pos == ANY_EVENT_MOUSE
#if OPT_DEC_LOCATOR
		    || screen->send_mouse_pos == DEC_LOCATOR
#endif /* OPT_DEC_LOCATOR */
		   )
		   && event.xany.type == MotionNotify
		   && event.xcrossing.window == XtWindow(xw)) {
	    SendMousePosition(xw, &event);
	    xtermShowPointer(xw, True);
	    continue;
	}

	/*
	 * If the event is interesting (and not a keyboard event), turn the
	 * mouse pointer back on.
	 */
	if (screen->hide_pointer) {
	    switch (event.xany.type) {
	    case KeyPress:
	    case KeyRelease:
	    case ButtonPress:
	    case ButtonRelease:
		/* also these... */
	    case Expose:
	    case NoExpose:
	    case PropertyNotify:
	    case ClientMessage:
		break;
	    default:
		xtermShowPointer(xw, True);
		break;
	    }
	}

	if (!event.xany.send_event ||
	    screen->allowSendEvents ||
	    ((event.xany.type != KeyPress) &&
	     (event.xany.type != KeyRelease) &&
	     (event.xany.type != ButtonPress) &&
	     (event.xany.type != ButtonRelease))) {

	    XtDispatchEvent(&event);
	}
    } while (XtAppPending(app_con) & XtIMXEvent);
}

static Cursor
make_hidden_cursor(XtermWidget xw)
{
    TScreen *screen = TScreenOf(xw);
    Cursor c;
    Display *dpy = screen->display;
    XFontStruct *fn;

    static XColor dummy;

    /*
     * Prefer nil2 (which is normally available) to "fixed" (which is supposed
     * to be "always" available), since it's a smaller glyph in case the
     * server insists on drawing _something_.
     */
    TRACE(("Ask for nil2 font\n"));
    if ((fn = XLoadQueryFont(dpy, "nil2")) == 0) {
	TRACE(("...Ask for fixed font\n"));
	fn = XLoadQueryFont(dpy, DEFFONT);
    }

    if (fn != 0) {
	/* a space character seems to work as a cursor (dots are not needed) */
	c = XCreateGlyphCursor(dpy, fn->fid, fn->fid, 'X', ' ', &dummy, &dummy);
	XFreeFont(dpy, fn);
    } else {
	c = 0;
    }
    TRACE(("XCreateGlyphCursor ->%#lx\n", c));
    return (c);
}

Cursor
make_colored_cursor(unsigned cursorindex,	/* index into font */
		    unsigned long fg,	/* pixel value */
		    unsigned long bg)	/* pixel value */
{
    TScreen *screen = TScreenOf(term);
    Cursor c;
    Display *dpy = screen->display;

    c = XCreateFontCursor(dpy, cursorindex);
    if (c != None) {
	recolor_cursor(screen, c, fg, bg);
    }
    return (c);
}

/* ARGSUSED */
void
HandleKeyPressed(Widget w GCC_UNUSED,
		 XEvent * event,
		 String * params GCC_UNUSED,
		 Cardinal *nparams GCC_UNUSED)
{
    TRACE(("Handle insert-seven-bit for %p\n", (void *) w));
    Input(term, &event->xkey, False);
}

/* ARGSUSED */
void
HandleEightBitKeyPressed(Widget w GCC_UNUSED,
			 XEvent * event,
			 String * params GCC_UNUSED,
			 Cardinal *nparams GCC_UNUSED)
{
    TRACE(("Handle insert-eight-bit for %p\n", (void *) w));
    Input(term, &event->xkey, True);
}

/* ARGSUSED */
void
HandleStringEvent(Widget w GCC_UNUSED,
		  XEvent * event GCC_UNUSED,
		  String * params,
		  Cardinal *nparams)
{

    if (*nparams != 1)
	return;

    if ((*params)[0] == '0' && (*params)[1] == 'x' && (*params)[2] != '\0') {
	const char *abcdef = "ABCDEF";
	const char *xxxxxx;
	Char c;
	UString p;
	unsigned value = 0;

	for (p = (UString) (*params + 2); (c = CharOf(x_toupper(*p))) !=
	     '\0'; p++) {
	    value *= 16;
	    if (c >= '0' && c <= '9')
		value += (unsigned) (c - '0');
	    else if ((xxxxxx = strchr(abcdef, c)) != 0)
		value += (unsigned) (xxxxxx - abcdef) + 10;
	    else
		break;
	}
	if (c == '\0') {
	    Char hexval[2];
	    hexval[0] = (Char) value;
	    hexval[1] = 0;
	    StringInput(term, hexval, (size_t) 1);
	}
    } else {
	StringInput(term, (const Char *) *params, strlen(*params));
    }
}

#if OPT_EXEC_XTERM

#ifndef PROCFS_ROOT
#define PROCFS_ROOT "/proc"
#endif

/* ARGSUSED */
void
HandleSpawnTerminal(Widget w GCC_UNUSED,
		    XEvent * event GCC_UNUSED,
		    String * params,
		    Cardinal *nparams)
{
    TScreen *screen = TScreenOf(term);
    char *child_cwd = NULL;
    char *child_exe;
    pid_t pid;

    /*
     * Try to find the actual program which is running in the child process.
     * This works for Linux.  If we cannot find the program, fall back to the
     * xterm program (which is usually adequate).  Give up if we are given only
     * a relative path to xterm, since that would not always match $PATH.
     */
    child_exe = Readlink(PROCFS_ROOT "/self/exe");
    if (!child_exe) {
	if (strncmp(ProgramName, "./", (size_t) 2)
	    && strncmp(ProgramName, "../", (size_t) 3)) {
	    child_exe = xtermFindShell(ProgramName, True);
	} else {
	    fprintf(stderr, "Cannot exec-xterm given %s\n", ProgramName);
	}
	if (child_exe == 0)
	    return;
    }

    /*
     * Determine the current working directory of the child so that we can
     * spawn a new terminal in the same directory.
     *
     * If we cannot get the CWD of the child, just use our own.
     */
    if (screen->pid) {
	char child_cwd_link[sizeof(PROCFS_ROOT) + 80];
	sprintf(child_cwd_link, PROCFS_ROOT "/%lu/cwd", (unsigned long) screen->pid);
	child_cwd = Readlink(child_cwd_link);
    }

    /* The reaper will take care of cleaning up the child */
    pid = fork();
    if (pid == -1) {
	fprintf(stderr, "Could not fork: %s\n", SysErrorMsg(errno));
    } else if (!pid) {
	/* We are the child */
	if (child_cwd) {
	    IGNORE_RC(chdir(child_cwd));	/* We don't care if this fails */
	}

	if (setuid(screen->uid) == -1
	    || setgid(screen->gid) == -1) {
	    fprintf(stderr, "Cannot reset uid/gid\n");
	} else {
	    unsigned myargc = *nparams + 1;
	    char **myargv = TypeMallocN(char *, myargc + 1);
	    unsigned n = 0;

	    myargv[n++] = child_exe;

	    while (n < myargc) {
		myargv[n++] = *params++;
	    }

	    myargv[n] = 0;
	    execv(child_exe, myargv);

	    /* If we get here, we've failed */
	    fprintf(stderr, "exec of '%s': %s\n", child_exe, SysErrorMsg(errno));
	}
	_exit(0);
    } else {
	/* We are the parent; clean up */
	if (child_cwd)
	    free(child_cwd);
	if (child_exe)
	    free(child_exe);
    }
}
#endif /* OPT_EXEC_XTERM */

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
HandleInterpret(Widget w GCC_UNUSED,
		XEvent * event GCC_UNUSED,
		String * params,
		Cardinal *param_count)
{
    if (*param_count == 1) {
	const char *value = params[0];
	int need = (int) strlen(value);
	int used = (int) (VTbuffer->next - VTbuffer->buffer);
	int have = (int) (VTbuffer->last - VTbuffer->buffer);

	if (have - used + need < BUF_SIZE) {

	    fillPtyData(term, VTbuffer, value, (int) strlen(value));

	    TRACE(("Interpret %s\n", value));
	    VTbuffer->update++;
	}
    }
}

/*ARGSUSED*/
void
HandleEnterWindow(Widget w GCC_UNUSED,
		  XtPointer eventdata GCC_UNUSED,
		  XEvent * event GCC_UNUSED,
		  Boolean * cont GCC_UNUSED)
{
    /* NOP since we handled it above */
    TRACE(("HandleEnterWindow ignored\n"));
    TRACE_FOCUS(w, event);
}

/*ARGSUSED*/
void
HandleLeaveWindow(Widget w GCC_UNUSED,
		  XtPointer eventdata GCC_UNUSED,
		  XEvent * event GCC_UNUSED,
		  Boolean * cont GCC_UNUSED)
{
    /* NOP since we handled it above */
    TRACE(("HandleLeaveWindow ignored\n"));
    TRACE_FOCUS(w, event);
}

/*ARGSUSED*/
void
HandleFocusChange(Widget w GCC_UNUSED,
		  XtPointer eventdata GCC_UNUSED,
		  XEvent * ev,
		  Boolean * cont GCC_UNUSED)
{
    XFocusChangeEvent *event = (XFocusChangeEvent *) ev;
    XtermWidget xw = term;
    TScreen *screen = TScreenOf(xw);

    TRACE(("HandleFocusChange type=%s, mode=%d, detail=%d\n",
	   visibleEventType(event->type),
	   event->mode,
	   event->detail));
    TRACE_FOCUS(xw, event);

    if (screen->quiet_grab
	&& (event->mode == NotifyGrab || event->mode == NotifyUngrab)) {
	/* EMPTY */ ;
    } else if (event->type == FocusIn) {
	setXUrgency(xw, False);

	/*
	 * NotifyNonlinear only happens (on FocusIn) if the pointer was not in
	 * one of our windows.  Use this to reset a case where one xterm is
	 * partly obscuring another, and X gets (us) confused about whether the
	 * pointer was in the window.  In particular, this can happen if the
	 * user is resizing the obscuring window, causing some events to not be
	 * delivered to the obscured window.
	 */
	if (event->detail == NotifyNonlinear
	    && (screen->select & INWINDOW) != 0) {
	    unselectwindow(screen, INWINDOW);
	}
	selectwindow(screen,
		     ((event->detail == NotifyPointer)
		      ? INWINDOW
		      : FOCUS));
	SendFocusButton(xw, event);
    } else {
#if OPT_FOCUS_EVENT
	if (event->type == FocusOut) {
	    SendFocusButton(xw, event);
	}
#endif
	/*
	 * XGrabKeyboard() will generate NotifyGrab event that we want to
	 * ignore.
	 */
	if (event->mode != NotifyGrab) {
	    unselectwindow(screen,
			   ((event->detail == NotifyPointer)
			    ? INWINDOW
			    : FOCUS));
	}
	if (screen->grabbedKbd && (event->mode == NotifyUngrab)) {
	    Bell(xw, XkbBI_Info, 100);
	    ReverseVideo(xw);
	    screen->grabbedKbd = False;
	    update_securekbd();
	}
    }
}

static long lastBellTime;	/* in milliseconds */

#if defined(HAVE_XKB_BELL_EXT)
static Atom
AtomBell(XtermWidget xw, int which)
{
#define DATA(name) { XkbBI_##name, XkbBN_##name }
    static struct {
	int value;
	const char *name;
    } table[] = {
	DATA(Info),
	    DATA(MarginBell),
	    DATA(MinorError),
	    DATA(TerminalBell)
    };
    Cardinal n;
    Atom result = None;

    for (n = 0; n < XtNumber(table); ++n) {
	if (table[n].value == which) {
	    result = XInternAtom(XtDisplay(xw), table[n].name, False);
	    break;
	}
    }
    return result;
}
#endif

void
xtermBell(XtermWidget xw, int which, int percent)
{
    TScreen *screen = TScreenOf(xw);
#if defined(HAVE_XKB_BELL_EXT)
    Atom tony = AtomBell(xw, which);
#endif

    switch (which) {
    case XkbBI_Info:
    case XkbBI_MinorError:
    case XkbBI_MajorError:
    case XkbBI_TerminalBell:
	switch (screen->warningVolume) {
	case bvOff:
	    percent = -100;
	    break;
	case bvLow:
	    break;
	case bvHigh:
	    percent = 100;
	    break;
	}
	break;
    case XkbBI_MarginBell:
	switch (screen->marginVolume) {
	case bvOff:
	    percent = -100;
	    break;
	case bvLow:
	    break;
	case bvHigh:
	    percent = 100;
	    break;
	}
	break;
    default:
	break;
    }

#if defined(HAVE_XKB_BELL_EXT)
    if (tony != None) {
	XkbBell(screen->display, VShellWindow(xw), percent, tony);
    } else
#endif
	XBell(screen->display, percent);
}

void
Bell(XtermWidget xw, int which, int percent)
{
    TScreen *screen = TScreenOf(xw);
    struct timeval curtime;
    long now_msecs;

    TRACE(("BELL %d %d%%\n", which, percent));
    if (!XtIsRealized((Widget) xw)) {
	return;
    }

    setXUrgency(xw, True);

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

    if (screen->visualbell) {
	VisualBell();
    } else {
	xtermBell(xw, which, percent);
    }

    if (screen->poponbell)
	XRaiseWindow(screen->display, VShellWindow(xw));

    if (screen->bellSuppressTime) {
	/* now we change a property and wait for the notify event to come
	   back.  If the server is suspending operations while the bell
	   is being emitted (problematic for audio bell), this lets us
	   know when the previous bell has finished */
	Widget w = CURRENT_EMU();
	XChangeProperty(XtDisplay(w), XtWindow(w),
			XA_NOTICE, XA_NOTICE, 8, PropModeAppend, NULL, 0);
	screen->bellInProgress = True;
    }
}

#define VB_DELAY screen->visualBellDelay

static void
flashWindow(TScreen * screen, Window window, GC visualGC, unsigned width, unsigned height)
{
    XFillRectangle(screen->display, window, visualGC, 0, 0, width, height);
    XFlush(screen->display);
    Sleep(VB_DELAY);
    XFillRectangle(screen->display, window, visualGC, 0, 0, width, height);
}

void
VisualBell(void)
{
    TScreen *screen = TScreenOf(term);

    if (VB_DELAY > 0) {
	Pixel xorPixel = (T_COLOR(screen, TEXT_FG) ^
			  T_COLOR(screen, TEXT_BG));
	XGCValues gcval;
	GC visualGC;

	gcval.function = GXxor;
	gcval.foreground = xorPixel;
	visualGC = XtGetGC((Widget) term, GCFunction + GCForeground, &gcval);
#if OPT_TEK4014
	if (TEK4014_ACTIVE(term)) {
	    TekScreen *tekscr = TekScreenOf(tekWidget);
	    flashWindow(screen, TWindow(tekscr), visualGC,
			TFullWidth(tekscr),
			TFullHeight(tekscr));
	} else
#endif
	{
	    flashWindow(screen, VWindow(screen), visualGC,
			FullWidth(screen),
			FullHeight(screen));
	}
	XtReleaseGC((Widget) term, visualGC);
    }
}

/* ARGSUSED */
void
HandleBellPropertyChange(Widget w GCC_UNUSED,
			 XtPointer data GCC_UNUSED,
			 XEvent * ev,
			 Boolean * more GCC_UNUSED)
{
    TScreen *screen = TScreenOf(term);

    if (ev->xproperty.atom == XA_NOTICE) {
	screen->bellInProgress = False;
    }
}

Window
WMFrameWindow(XtermWidget xw)
{
    Window win_root, win_current, *children;
    Window win_parent = 0;
    unsigned int nchildren;

    win_current = XtWindow(xw);

    /* find the parent which is child of root */
    do {
	if (win_parent)
	    win_current = win_parent;
	XQueryTree(TScreenOf(xw)->display,
		   win_current,
		   &win_root,
		   &win_parent,
		   &children,
		   &nchildren);
	XFree(children);
    } while (win_root != win_parent);

    return win_current;
}

#if OPT_DABBREV
/*
 * The following code implements `dynamic abbreviation' expansion a la
 * Emacs.  It looks in the preceding visible screen and its scrollback
 * to find expansions of a typed word.  It compares consecutive
 * expansions and ignores one of them if they are identical.
 * (Tomasz J. Cholewo, t.cholewo@ieee.org)
 */

#define IS_WORD_CONSTITUENT(x) ((x) != ' ' && (x) != '\0')
#define MAXWLEN 1024		/* maximum word length as in tcsh */

static int
dabbrev_prev_char(TScreen * screen, CELL * cell, LineData ** ld)
{
    int result = -1;
    int firstLine = -(screen->savedlines);

    *ld = getLineData(screen, cell->row);
    while (cell->row >= firstLine) {
	if (--(cell->col) >= 0) {
	    result = (int) (*ld)->charData[cell->col];
	    break;
	}
	if (--(cell->row) < firstLine)
	    break;		/* ...there is no previous line */
	*ld = getLineData(screen, cell->row);
	cell->col = MaxCols(screen);
	if (!LineTstWrapped(*ld)) {
	    result = ' ';	/* treat lines as separate */
	    break;
	}
    }
    return result;
}

static char *
dabbrev_prev_word(TScreen * screen, CELL * cell, LineData ** ld)
{
    static char ab[MAXWLEN];

    char *abword;
    int c;
    char *ab_end = (ab + MAXWLEN - 1);
    char *result = 0;

    abword = ab_end;
    *abword = '\0';		/* end of string marker */

    while ((c = dabbrev_prev_char(screen, cell, ld)) >= 0 &&
	   IS_WORD_CONSTITUENT(c)) {
	if (abword > ab)	/* store only |MAXWLEN| last chars */
	    *(--abword) = (char) c;
    }

    if (c >= 0) {
	result = abword;
    } else if (abword != ab_end) {
	result = abword;
    }

    if (result != 0) {
	while ((c = dabbrev_prev_char(screen, cell, ld)) >= 0 &&
	       !IS_WORD_CONSTITUENT(c)) {
	    ;			/* skip preceding spaces */
	}
	(cell->col)++;		/* can be | > screen->max_col| */
    }
    return result;
}

static int
dabbrev_expand(TScreen * screen)
{
    int pty = screen->respond;	/* file descriptor of pty */

    static CELL cell;
    static char *dabbrev_hint = 0, *lastexpansion = 0;
    static unsigned int expansions;

    char *expansion;
    Char *copybuffer;
    size_t hint_len;
    size_t del_cnt;
    size_t buf_cnt;
    int result = 0;
    LineData *ld;

    if (!screen->dabbrev_working) {	/* initialize */
	expansions = 0;
	cell.col = screen->cur_col;
	cell.row = screen->cur_row;

	if (dabbrev_hint != 0)
	    free(dabbrev_hint);

	if ((dabbrev_hint = dabbrev_prev_word(screen, &cell, &ld)) != 0) {

	    if (lastexpansion != 0)
		free(lastexpansion);

	    if ((lastexpansion = strdup(dabbrev_hint)) != 0) {

		/* make own copy */
		if ((dabbrev_hint = strdup(dabbrev_hint)) != 0) {
		    screen->dabbrev_working = True;
		    /* we are in the middle of dabbrev process */
		}
	    } else {
		return result;
	    }
	} else {
	    return result;
	}
	if (!screen->dabbrev_working) {
	    if (lastexpansion != 0) {
		free(lastexpansion);
		lastexpansion = 0;
	    }
	    return result;
	}
    }

    if (dabbrev_hint == 0)
	return result;

    hint_len = strlen(dabbrev_hint);
    for (;;) {
	if ((expansion = dabbrev_prev_word(screen, &cell, &ld)) == 0) {
	    if (expansions >= 2) {
		expansions = 0;
		cell.col = screen->cur_col;
		cell.row = screen->cur_row;
		continue;
	    }
	    break;
	}
	if (!strncmp(dabbrev_hint, expansion, hint_len) &&	/* empty hint matches everything */
	    strlen(expansion) > hint_len &&	/* trivial expansion disallowed */
	    strcmp(expansion, lastexpansion))	/* different from previous */
	    break;
    }

    if (expansion != 0) {
	del_cnt = strlen(lastexpansion) - hint_len;
	buf_cnt = del_cnt + strlen(expansion) - hint_len;

	if ((copybuffer = TypeMallocN(Char, buf_cnt)) != 0) {
	    /* delete previous expansion */
	    memset(copybuffer, screen->dabbrev_erase_char, del_cnt);
	    memmove(copybuffer + del_cnt,
		    expansion + hint_len,
		    strlen(expansion) - hint_len);
	    v_write(pty, copybuffer, (unsigned) buf_cnt);
	    /* v_write() just reset our flag */
	    screen->dabbrev_working = True;
	    free(copybuffer);

	    free(lastexpansion);

	    if ((lastexpansion = strdup(expansion)) != 0) {
		result = 1;
		expansions++;
	    }
	}
    }

    return result;
}

/*ARGSUSED*/
void
HandleDabbrevExpand(Widget w,
		    XEvent * event GCC_UNUSED,
		    String * params GCC_UNUSED,
		    Cardinal *nparams GCC_UNUSED)
{
    XtermWidget xw;

    TRACE(("Handle dabbrev-expand for %p\n", (void *) w));
    if ((xw = getXtermWidget(w)) != 0) {
	TScreen *screen = TScreenOf(xw);
	if (!dabbrev_expand(screen))
	    Bell(xw, XkbBI_TerminalBell, 0);
    }
}
#endif /* OPT_DABBREV */

#if OPT_MAXIMIZE
/*ARGSUSED*/
void
HandleDeIconify(Widget w,
		XEvent * event GCC_UNUSED,
		String * params GCC_UNUSED,
		Cardinal *nparams GCC_UNUSED)
{
    XtermWidget xw;

    if ((xw = getXtermWidget(w)) != 0) {
	TScreen *screen = TScreenOf(xw);
	XMapWindow(screen->display, VShellWindow(xw));
    }
}

/*ARGSUSED*/
void
HandleIconify(Widget w,
	      XEvent * event GCC_UNUSED,
	      String * params GCC_UNUSED,
	      Cardinal *nparams GCC_UNUSED)
{
    XtermWidget xw;

    if ((xw = getXtermWidget(w)) != 0) {
	TScreen *screen = TScreenOf(xw);
	XIconifyWindow(screen->display,
		       VShellWindow(xw),
		       DefaultScreen(screen->display));
    }
}

int
QueryMaximize(XtermWidget xw, unsigned *width, unsigned *height)
{
    TScreen *screen = TScreenOf(xw);
    XSizeHints hints;
    long supp = 0;
    Window root_win;
    int root_x = -1;		/* saved co-ordinates */
    int root_y = -1;
    unsigned root_border;
    unsigned root_depth;

    if (XGetGeometry(screen->display,
		     RootWindowOfScreen(XtScreen(xw)),
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

	*width -= (root_border * 2);
	*height -= (root_border * 2);

	hints.flags = PMaxSize;
	if (XGetWMNormalHints(screen->display,
			      VShellWindow(xw),
			      &hints,
			      &supp)
	    && (hints.flags & PMaxSize) != 0) {

	    TRACE(("QueryMaximize: WM hints max_w %#x max_h %#x\n",
		   hints.max_width,
		   hints.max_height));

	    if ((unsigned) hints.max_width < *width)
		*width = (unsigned) hints.max_width;
	    if ((unsigned) hints.max_height < *height)
		*height = (unsigned) hints.max_height;
	}
	return 1;
    }
    return 0;
}

void
RequestMaximize(XtermWidget xw, int maximize)
{
    TScreen *screen = TScreenOf(xw);
    XWindowAttributes wm_attrs, vshell_attrs;
    unsigned root_width, root_height;

    TRACE(("RequestMaximize %s\n", maximize ? "maximize" : "restore"));

    if (maximize) {

	if (QueryMaximize(xw, &root_width, &root_height)) {

	    if (XGetWindowAttributes(screen->display,
				     WMFrameWindow(xw),
				     &wm_attrs)) {

		if (XGetWindowAttributes(screen->display,
					 VShellWindow(xw),
					 &vshell_attrs)) {

		    if (screen->restore_data != True
			|| screen->restore_width != root_width
			|| screen->restore_height != root_height) {
			screen->restore_data = True;
			screen->restore_x = wm_attrs.x + wm_attrs.border_width;
			screen->restore_y = wm_attrs.y + wm_attrs.border_width;
			screen->restore_width = (unsigned) vshell_attrs.width;
			screen->restore_height = (unsigned) vshell_attrs.height;
			TRACE(("HandleMaximize: save window position %d,%d size %d,%d\n",
			       screen->restore_x,
			       screen->restore_y,
			       screen->restore_width,
			       screen->restore_height));
		    }

		    /* subtract wm decoration dimensions */
		    root_width -=
			(unsigned) ((wm_attrs.width - vshell_attrs.width)
				    + (wm_attrs.border_width * 2));
		    root_height -=
			(unsigned) ((wm_attrs.height - vshell_attrs.height)
				    + (wm_attrs.border_width * 2));

		    XMoveResizeWindow(screen->display, VShellWindow(xw),
				      0 + wm_attrs.border_width,	/* x */
				      0 + wm_attrs.border_width,	/* y */
				      root_width,
				      root_height);
		}
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
			      VShellWindow(xw),
			      screen->restore_x,
			      screen->restore_y,
			      screen->restore_width,
			      screen->restore_height);
	}
    }
}

/*ARGSUSED*/
void
HandleMaximize(Widget w,
	       XEvent * event GCC_UNUSED,
	       String * params GCC_UNUSED,
	       Cardinal *nparams GCC_UNUSED)
{
    XtermWidget xw;

    if ((xw = getXtermWidget(w)) != 0) {
	RequestMaximize(xw, 1);
    }
}

/*ARGSUSED*/
void
HandleRestoreSize(Widget w,
		  XEvent * event GCC_UNUSED,
		  String * params GCC_UNUSED,
		  Cardinal *nparams GCC_UNUSED)
{
    XtermWidget xw;

    if ((xw = getXtermWidget(w)) != 0) {
	RequestMaximize(xw, 0);
    }
}
#endif /* OPT_MAXIMIZE */

void
Redraw(void)
{
    TScreen *screen = TScreenOf(term);
    XExposeEvent event;

    TRACE(("Redraw\n"));

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
    if (TEK4014_SHOWN(term)) {
	TekScreen *tekscr = TekScreenOf(tekWidget);
	event.window = TWindow(tekscr);
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

    tstamp = time((time_t *) 0);
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
open_userfile(uid_t uid, gid_t gid, char *path, Bool append)
{
    int fd;
    struct stat sb;

#ifdef VMS
    if ((fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644)) < 0) {
	int the_error = errno;
	fprintf(stderr, "%s: cannot open %s: %d:%s\n",
		ProgramName,
		path,
		the_error,
		SysErrorMsg(the_error));
	return -1;
    }
    chown(path, uid, gid);
#else
    if ((access(path, F_OK) != 0 && (errno != ENOENT))
	|| (creat_as(uid, gid, append, path, 0644) <= 0)
	|| ((fd = open(path, O_WRONLY | O_APPEND)) < 0)) {
	int the_error = errno;
	fprintf(stderr, "%s: cannot open %s: %d:%s\n",
		ProgramName,
		path,
		the_error,
		SysErrorMsg(the_error));
	return -1;
    }
#endif

    /*
     * Doublecheck that the user really owns the file that we've opened before
     * we do any damage, and that it is not world-writable.
     */
    if (fstat(fd, &sb) < 0
	|| sb.st_uid != uid
	|| (sb.st_mode & 022) != 0) {
	fprintf(stderr, "%s: you do not own %s\n", ProgramName, path);
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
 * Returns
 *	 1 if we can proceed to open the file in relative safety,
 *	-1 on error, e.g., cannot fork
 *	 0 otherwise.
 */
int
creat_as(uid_t uid, gid_t gid, Bool append, char *pathname, int mode)
{
    int fd;
    pid_t pid;
    int retval = 0;
    int childstat = 0;
#ifndef HAVE_WAITPID
    int waited;
    SIGNAL_T(*chldfunc) (int);

    chldfunc = signal(SIGCHLD, SIG_DFL);
#endif /* HAVE_WAITPID */

    TRACE(("creat_as(uid=%d/%d, gid=%d/%d, append=%d, pathname=%s, mode=%#o)\n",
	   (int) uid, (int) geteuid(),
	   (int) gid, (int) getegid(),
	   append,
	   pathname,
	   mode));

    if (uid == geteuid() && gid == getegid()) {
	fd = open(pathname,
		  O_WRONLY | O_CREAT | (append ? O_APPEND : O_EXCL),
		  mode);
	if (fd >= 0)
	    close(fd);
	return (fd >= 0);
    }

    pid = fork();
    switch (pid) {
    case 0:			/* child */
	if (setgid(gid) == -1
	    || setuid(uid) == -1) {
	    /* we cannot report an error here via stderr, just quit */
	    retval = 1;
	} else {
	    fd = open(pathname,
		      O_WRONLY | O_CREAT | (append ? O_APPEND : O_EXCL),
		      mode);
	    if (fd >= 0) {
		close(fd);
		retval = 0;
	    } else {
		retval = 1;
	    }
	}
	_exit(retval);
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
	    if (waited == TScreenOf(term)->pid)
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

int
xtermResetIds(TScreen * screen)
{
    int result = 0;
    if (setgid(screen->gid) == -1) {
	fprintf(stderr, "%s: unable to reset group-id\n", ProgramName);
	result = -1;
    }
    if (setuid(screen->uid) == -1) {
	fprintf(stderr, "%s: unable to reset user-id\n", ProgramName);
	result = -1;
    }
    return result;
}

#ifdef ALLOWLOGGING

/*
 * Logging is a security hole, since it allows a setuid program to write
 * arbitrary data to an arbitrary file.  So it is disabled by default.
 */

#ifdef ALLOWLOGFILEEXEC
static SIGNAL_T
logpipe(int sig GCC_UNUSED)
{
    XtermWidget xw = term;
    TScreen *screen = TScreenOf(xw);

#ifdef SYSV
    (void) signal(SIGPIPE, SIG_IGN);
#endif /* SYSV */
    if (screen->logging)
	CloseLog(xw);
}
#endif /* ALLOWLOGFILEEXEC */

void
StartLog(XtermWidget xw)
{
    static char *log_default;
#ifdef ALLOWLOGFILEEXEC
    char *cp;
#endif /* ALLOWLOGFILEEXEC */
    TScreen *screen = TScreenOf(xw);

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

	    now = time((time_t *) 0);
	    ltm = (struct tm *) localtime(&now);
	    if ((gethostname(hostname, sizeof(hostname)) == 0) &&
		(strftime(yyyy_mm_dd_hh_mm_ss,
			  sizeof(yyyy_mm_dd_hh_mm_ss),
			  "%Y.%m.%d.%H.%M.%S", ltm) > 0)) {
		(void) sprintf(log_def_name, "Xterm.log.%.255s.%.20s.%d",
			       hostname, yyyy_mm_dd_hh_mm_ss, (int) getpid());
	    }
	    if ((log_default = x_strdup(log_def_name)) == NULL)
		return;
#else
	    const char *log_def_name = "XtermLog.XXXXXX";
	    if ((log_default = x_strdup(log_def_name)) == NULL)
		return;

	    mktemp(log_default);
#endif
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
	int pid;
	int p[2];
	static char *shell;
	struct passwd *pw;

	if (pipe(p) < 0 || (pid = fork()) < 0)
	    return;
	if (pid == 0) {		/* child */
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

	    if ((((cp = x_getenv("SHELL")) == NULL)
		 && ((pw = getpwuid(screen->uid)) == NULL
		     || *(cp = pw->pw_shell) == 0))
		|| (shell = CastMallocN(char, strlen(cp))) == 0) {
		static char dummy[] = "/bin/sh";
		shell = dummy;
	    } else {
		strcpy(shell, cp);
	    }

	    signal(SIGHUP, SIG_DFL);
	    signal(SIGCHLD, SIG_DFL);

	    /* (this is redundant) */
	    if (xtermResetIds(screen) < 0)
		exit(ERROR_SETUID);

	    execl(shell, shell, "-c", &screen->logfile[1], (void *) 0);

	    fprintf(stderr, "%s: Can't exec `%s'\n",
		    ProgramName,
		    &screen->logfile[1]);
	    exit(ERROR_LOGEXEC);
	}
	close(p[0]);
	screen->logfd = p[1];
	signal(SIGPIPE, logpipe);
#else
	Bell(xw, XkbBI_Info, 0);
	Bell(xw, XkbBI_Info, 0);
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
    screen->logstart = VTbuffer->next;
    screen->logging = True;
    update_logging();
}

void
CloseLog(XtermWidget xw)
{
    TScreen *screen = TScreenOf(xw);

    if (!screen->logging || (screen->inhibit & I_LOG))
	return;
    FlushLog(xw);
    close(screen->logfd);
    screen->logging = False;
    update_logging();
}

void
FlushLog(XtermWidget xw)
{
    TScreen *screen = TScreenOf(xw);

    if (screen->logging && !(screen->inhibit & I_LOG)) {
	Char *cp;
	int i;

#ifdef VMS			/* avoid logging output loops which otherwise occur sometimes
				   when there is no output and cp/screen->logstart are 1 apart */
	if (!tt_new_output)
	    return;
	tt_new_output = False;
#endif /* VMS */
	cp = VTbuffer->next;
	if (screen->logstart != 0
	    && (i = (int) (cp - screen->logstart)) > 0) {
	    IGNORE_RC(write(screen->logfd, screen->logstart, (size_t) i));
	}
	screen->logstart = VTbuffer->next;
    }
}

#endif /* ALLOWLOGGING */

/***====================================================================***/

#if OPT_ISO_COLORS
static void
ReportAnsiColorRequest(XtermWidget xw, int colornum, int final)
{
    if (AllowColorOps(xw, ecGetAnsiColor)) {
	XColor color;
	Colormap cmap = xw->core.colormap;
	char buffer[80];

	TRACE(("ReportAnsiColorRequest %d\n", colornum));
	color.pixel = GET_COLOR_RES(xw, TScreenOf(xw)->Acolors[colornum]);
	XQueryColor(TScreenOf(xw)->display, cmap, &color);
	sprintf(buffer, "4;%d;rgb:%04x/%04x/%04x",
		colornum,
		color.red,
		color.green,
		color.blue);
	unparseputc1(xw, ANSI_OSC);
	unparseputs(xw, buffer);
	unparseputc1(xw, final);
	unparse_end(xw);
    }
}

static unsigned
getColormapSize(Display * display)
{
    unsigned result;
    int numFound;
    XVisualInfo myTemplate, *visInfoPtr;

    myTemplate.visualid = XVisualIDFromVisual(DefaultVisual(display,
							    XDefaultScreen(display)));
    visInfoPtr = XGetVisualInfo(display, (long) VisualIDMask,
				&myTemplate, &numFound);
    result = (numFound >= 1) ? (unsigned) visInfoPtr->colormap_size : 0;

    XFree((char *) visInfoPtr);
    return result;
}

/*
 * Find closest color for "def" in "cmap".
 * Set "def" to the resulting color.
 *
 * Based on Monish Shah's "find_closest_color()" for Vim 6.0,
 * modified with ideas from David Tong's "noflash" library.
 * The code from Vim in turn was derived from FindClosestColor() in Tcl/Tk.
 *
 * These provide some introduction:
 *	http://en.wikipedia.org/wiki/YIQ
 *		for an introduction to YIQ weights.
 *	http://en.wikipedia.org/wiki/Luminance_(video)
 *		for a discussion of luma.
 *	http://en.wikipedia.org/wiki/YUV
 *
 * Return False if not able to find or allocate a color.
 */
static Boolean
find_closest_color(Display * dpy, Colormap cmap, XColor * def)
{
    Boolean result = False;
    XColor *colortable;
    char *tried;
    double diff, thisRGB, bestRGB;
    unsigned attempts;
    unsigned bestInx;
    unsigned cmap_size;
    unsigned i;

    cmap_size = getColormapSize(dpy);
    if (cmap_size != 0) {

	colortable = TypeMallocN(XColor, (size_t) cmap_size);
	if (colortable != 0) {

	    tried = TypeCallocN(char, (size_t) cmap_size);
	    if (tried != 0) {

		for (i = 0; i < cmap_size; i++) {
		    colortable[i].pixel = (unsigned long) i;
		}
		XQueryColors(dpy, cmap, colortable, (int) cmap_size);

		/*
		 * Try (possibly each entry in the color map) to find the best
		 * approximation to the requested color.
		 */
		for (attempts = 0; attempts < cmap_size; attempts++) {
		    Boolean first = True;

		    bestRGB = 0.0;
		    bestInx = 0;
		    for (i = 0; i < cmap_size; i++) {
			if (!tried[bestInx]) {
			    /*
			     * Look for the best match based on luminance.
			     * Measure this by the least-squares difference of
			     * the weighted R/G/B components from the color map
			     * versus the requested color.  Use the Y (luma)
			     * component of the YIQ color space model for
			     * weights that correspond to the luminance.
			     */
#define AddColorWeight(weight, color) \
			    diff = weight * (int) ((def->color) - colortable[i].color); \
			    thisRGB = diff * diff

			    AddColorWeight(0.30, red);
			    AddColorWeight(0.61, green);
			    AddColorWeight(0.11, blue);

			    if (first || (thisRGB < bestRGB)) {
				first = False;
				bestInx = i;
				bestRGB = thisRGB;
			    }
			}
		    }
		    if (XAllocColor(dpy, cmap, &colortable[bestInx]) != 0) {
			*def = colortable[bestInx];
			result = True;
			break;
		    }
		    /*
		     * It failed - either the color map entry was readonly, or
		     * another client has allocated the entry.  Mark the entry
		     * so we will ignore it
		     */
		    tried[bestInx] = True;
		}
		free(tried);
	    }
	    free(colortable);
	}
    }
    return result;
}

/*
 * Allocate a color for the "ANSI" colors.  That actually includes colors up
 * to 256.
 *
 * Returns
 *	-1 on error
 *	0 on no change
 *	1 if a new color was allocated.
 */
static int
AllocateAnsiColor(XtermWidget xw,
		  ColorRes * res,
		  const char *spec)
{
    int result;
    XColor def;
    TScreen *screen = TScreenOf(xw);
    Colormap cmap = xw->core.colormap;

    if (XParseColor(screen->display, cmap, spec, &def)
	&& (XAllocColor(screen->display, cmap, &def)
	    || find_closest_color(screen->display, cmap, &def))) {
	if (
#if OPT_COLOR_RES
	       res->mode == True &&
#endif
	       EQL_COLOR_RES(res, def.pixel)) {
	    result = 0;
	} else {
	    result = 1;
	    SET_COLOR_RES(res, def.pixel);
	    TRACE(("AllocateAnsiColor[%d] %s (pixel %#lx)\n",
		   (int) (res - screen->Acolors), spec, def.pixel));
#if OPT_COLOR_RES
	    if (!res->mode)
		result = 0;
	    res->mode = True;
#endif
	}
    } else {
	TRACE(("AllocateAnsiColor %s (failed)\n", spec));
	result = -1;
    }
    return (result);
}

#if OPT_COLOR_RES
Pixel
xtermGetColorRes(XtermWidget xw, ColorRes * res)
{
    Pixel result = 0;

    if (res->mode) {
	result = res->value;
    } else {
	TRACE(("xtermGetColorRes for Acolors[%d]\n",
	       (int) (res - TScreenOf(xw)->Acolors)));

	if (res >= TScreenOf(xw)->Acolors) {
	    assert(res - TScreenOf(xw)->Acolors < MAXCOLORS);

	    if (AllocateAnsiColor(xw, res, res->resource) < 0) {
		res->value = TScreenOf(xw)->Tcolors[TEXT_FG].value;
		res->mode = -True;
		fprintf(stderr,
			"%s: Cannot allocate color \"%s\"\n",
			ProgramName,
			NonNull(res->resource));
	    }
	    result = res->value;
	} else {
	    result = 0;
	}
    }
    return result;
}
#endif

static int
ChangeOneAnsiColor(XtermWidget xw, int color, const char *name)
{
    int code;

    if (color < 0 || color >= MAXCOLORS) {
	code = -1;
    } else {
	ColorRes *res = &(TScreenOf(xw)->Acolors[color]);

	TRACE(("ChangeAnsiColor for Acolors[%d]\n", color));
	code = AllocateAnsiColor(xw, res, name);
    }
    return code;
}

/*
 * Set or query entries in the Acolors[] array by parsing pairs of color/name
 * values from the given buffer.
 *
 * The color can be any legal index into Acolors[], which consists of the
 * 16/88/256 "ANSI" colors, followed by special color values for the various
 * colorXX resources.  The indices for the special color values are not
 * simple to work with, so an alternative is to use the calls which pass in
 * 'first' set to the beginning of those indices.
 *
 * If the name is "?", report to the host the current value for the color.
 */
static Bool
ChangeAnsiColorRequest(XtermWidget xw,
		       char *buf,
		       int first,
		       int final)
{
    char *name;
    int color;
    int repaint = False;
    int code;
    int last = (MAXCOLORS - first);

    TRACE(("ChangeAnsiColorRequest string='%s'\n", buf));

    while (buf && *buf) {
	name = strchr(buf, ';');
	if (name == NULL)
	    break;
	*name = '\0';
	name++;
	color = atoi(buf);
	if (color < 0 || color >= last)
	    break;		/* quit on any error */
	buf = strchr(name, ';');
	if (buf) {
	    *buf = '\0';
	    buf++;
	}
	if (!strcmp(name, "?")) {
	    ReportAnsiColorRequest(xw, color + first, final);
	} else {
	    code = ChangeOneAnsiColor(xw, color + first, name);
	    if (code < 0) {
		/* stop on any error */
		break;
	    } else if (code > 0) {
		repaint = True;
	    }
	    /* FIXME:  free old color somehow?  We aren't for the other color
	     * change style (dynamic colors).
	     */
	}
    }

    return (repaint);
}

static Bool
ResetOneAnsiColor(XtermWidget xw, int color, int start)
{
    Bool repaint = False;
    int last = MAXCOLORS - start;

    if (color >= 0 && color < last) {
	ColorRes *res = &(TScreenOf(xw)->Acolors[color + start]);

	if (res->mode) {
	    /* a color has been allocated for this slot - test further... */
	    if (ChangeOneAnsiColor(xw, color + start, res->resource) > 0) {
		repaint = True;
	    }
	}
    }
    return repaint;
}

int
ResetAnsiColorRequest(XtermWidget xw, char *buf, int start)
{
    int repaint = 0;
    int color;

    TRACE(("ResetAnsiColorRequest(%s)\n", buf));
    if (*buf != '\0') {
	/* reset specific colors */
	while (!IsEmpty(buf)) {
	    char *next;

	    color = (int) strtol(buf, &next, 10);
	    if ((next == buf) || (color < 0))
		break;		/* no number at all */
	    if (next != 0) {
		if (strchr(";", *next) == 0)
		    break;	/* unexpected delimiter */
		++next;
	    }

	    if (ResetOneAnsiColor(xw, color, start)) {
		++repaint;
	    }
	    buf = next;
	}
    } else {
	TRACE(("...resetting all %d colors\n", MAXCOLORS));
	for (color = 0; color < MAXCOLORS; ++color) {
	    if (ResetOneAnsiColor(xw, color, start)) {
		++repaint;
	    }
	}
    }
    TRACE(("...ResetAnsiColorRequest ->%d\n", repaint));
    return repaint;
}
#else
#define find_closest_color(display, cmap, def) 0
#endif /* OPT_ISO_COLORS */

#if OPT_PASTE64
static void
ManipulateSelectionData(XtermWidget xw, TScreen * screen, char *buf, int final)
{
#define PDATA(a,b) { a, #b }
    static struct {
	char given;
	String result;
    } table[] = {
	PDATA('s', SELECT),
	    PDATA('p', PRIMARY),
	    PDATA('c', CLIPBOARD),
	    PDATA('0', CUT_BUFFER0),
	    PDATA('1', CUT_BUFFER1),
	    PDATA('2', CUT_BUFFER2),
	    PDATA('3', CUT_BUFFER3),
	    PDATA('4', CUT_BUFFER4),
	    PDATA('5', CUT_BUFFER5),
	    PDATA('6', CUT_BUFFER6),
	    PDATA('7', CUT_BUFFER7),
    };

    const char *base = buf;
    char *used = x_strdup(base);
    Cardinal j, n = 0;
    String *select_args = 0;

    TRACE(("Manipulate selection data\n"));

    while (*buf != ';' && *buf != '\0') {
	++buf;
    }

    if (*buf == ';') {
	*buf++ = '\0';

	if (*base == '\0')
	    base = "s0";
	if ((select_args = TypeCallocN(String, 1 + strlen(base))) == 0)
	    return;
	while (*base != '\0') {
	    for (j = 0; j < XtNumber(table); ++j) {
		if (*base == table[j].given) {
		    used[n] = *base;
		    select_args[n++] = table[j].result;
		    TRACE(("atom[%d] %s\n", n, table[j].result));
		    break;
		}
	    }
	    ++base;
	}
	used[n] = 0;

	if (!strcmp(buf, "?")) {
	    if (AllowWindowOps(xw, ewGetSelection)) {
		TRACE(("Getting selection\n"));
		unparseputc1(xw, ANSI_OSC);
		unparseputs(xw, "52");
		unparseputc(xw, ';');

		unparseputs(xw, used);
		unparseputc(xw, ';');

		/* Tell xtermGetSelection data is base64 encoded */
		screen->base64_paste = n;
		screen->base64_final = final;

		/* terminator will be written in this call */
		xtermGetSelection((Widget) xw, (Time) 0, select_args, n, NULL);
	    }
	} else {
	    if (AllowWindowOps(xw, ewSetSelection)) {
		TRACE(("Setting selection with %s\n", buf));
		ClearSelectionBuffer(screen);
		while (*buf != '\0')
		    AppendToSelectionBuffer(screen, CharOf(*buf++));
		CompleteSelection(xw, select_args, n);
	    }
	}
    }
}
#endif /* OPT_PASTE64 */

/***====================================================================***/

#define IsSetUtf8Title(xw) (IsTitleMode(xw, tmSetUtf8) || (xw->screen.utf8_title))

static Bool
xtermIsPrintable(XtermWidget xw, Char ** bufp, Char * last)
{
    TScreen *screen = TScreenOf(xw);
    Bool result = False;
    Char *cp = *bufp;
    Char *next = cp;

    (void) screen;
    (void) last;

#if OPT_WIDE_CHARS
    if (xtermEnvUTF8() && IsSetUtf8Title(xw)) {
	PtyData data;

	if (decodeUtf8(fakePtyData(&data, cp, last))) {
	    if (data.utf_data != UCS_REPL
		&& (data.utf_data >= 128 ||
		    ansi_table[data.utf_data] == CASE_PRINT)) {
		next += (data.utf_size - 1);
		result = True;
	    } else {
		result = False;
	    }
	} else {
	    result = False;
	}
    } else
#endif
#if OPT_C1_PRINT
	if (screen->c1_printable
	    && (*cp >= 128 && *cp < 160)) {
	result = True;
    } else
#endif
    if (ansi_table[*cp] == CASE_PRINT) {
	result = True;
    }
    *bufp = next;
    return result;
}

/***====================================================================***/

/*
 * Enum corresponding to the actual OSC codes rather than the internal
 * array indices.  Compare with TermColors.
 */
typedef enum {
    OSC_TEXT_FG = 10
    ,OSC_TEXT_BG
    ,OSC_TEXT_CURSOR
    ,OSC_MOUSE_FG
    ,OSC_MOUSE_BG
#if OPT_TEK4014
    ,OSC_TEK_FG = 15
    ,OSC_TEK_BG
#endif
#if OPT_HIGHLIGHT_COLOR
    ,OSC_HIGHLIGHT_BG = 17
#endif
#if OPT_TEK4014
    ,OSC_TEK_CURSOR = 18
#endif
#if OPT_HIGHLIGHT_COLOR
    ,OSC_HIGHLIGHT_FG = 19
#endif
    ,OSC_NCOLORS
} OscTextColors;

/*
 * Map codes to OSC controls that can reset colors.
 */
#define OSC_RESET 100
#define OSC_Reset(code) (code) + OSC_RESET

static ScrnColors *pOldColors = NULL;

static Bool
GetOldColors(XtermWidget xw)
{
    int i;
    if (pOldColors == NULL) {
	pOldColors = TypeXtMalloc(ScrnColors);
	if (pOldColors == NULL) {
	    fprintf(stderr, "allocation failure in GetOldColors\n");
	    return (False);
	}
	pOldColors->which = 0;
	for (i = 0; i < NCOLORS; i++) {
	    pOldColors->colors[i] = 0;
	    pOldColors->names[i] = NULL;
	}
	GetColors(xw, pOldColors);
    }
    return (True);
}

static int
oppositeColor(int n)
{
    switch (n) {
    case TEXT_FG:
	n = TEXT_BG;
	break;
    case TEXT_BG:
	n = TEXT_FG;
	break;
    case MOUSE_FG:
	n = MOUSE_BG;
	break;
    case MOUSE_BG:
	n = MOUSE_FG;
	break;
#if OPT_TEK4014
    case TEK_FG:
	n = TEK_BG;
	break;
    case TEK_BG:
	n = TEK_FG;
	break;
#endif
#if OPT_HIGHLIGHT_COLOR
    case HIGHLIGHT_FG:
	n = HIGHLIGHT_BG;
	break;
    case HIGHLIGHT_BG:
	n = HIGHLIGHT_FG;
	break;
#endif
    default:
	break;
    }
    return n;
}

static void
ReportColorRequest(XtermWidget xw, int ndx, int final)
{
    if (AllowColorOps(xw, ecGetColor)) {
	XColor color;
	Colormap cmap = xw->core.colormap;
	char buffer[80];

	/*
	 * ChangeColorsRequest() has "always" chosen the opposite color when
	 * reverse-video is set.  Report this as the original color index, but
	 * reporting the opposite color which would be used.
	 */
	int i = (xw->misc.re_verse) ? oppositeColor(ndx) : ndx;

	GetOldColors(xw);
	color.pixel = pOldColors->colors[ndx];
	XQueryColor(TScreenOf(xw)->display, cmap, &color);
	sprintf(buffer, "%d;rgb:%04x/%04x/%04x", i + 10,
		color.red,
		color.green,
		color.blue);
	TRACE(("ReportColors %d: %#lx as %s\n",
	       ndx, pOldColors->colors[ndx], buffer));
	unparseputc1(xw, ANSI_OSC);
	unparseputs(xw, buffer);
	unparseputc1(xw, final);
	unparse_end(xw);
    }
}

static Bool
UpdateOldColors(XtermWidget xw GCC_UNUSED, ScrnColors * pNew)
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
    return (True);
}

/*
 * OSC codes are constant, but the indices for the color arrays depend on how
 * xterm is compiled.
 */
static int
OscToColorIndex(OscTextColors mode)
{
    int result = 0;

#define CASE(name) case OSC_##name: result = name; break
    switch (mode) {
	CASE(TEXT_FG);
	CASE(TEXT_BG);
	CASE(TEXT_CURSOR);
	CASE(MOUSE_FG);
	CASE(MOUSE_BG);
#if OPT_TEK4014
	CASE(TEK_FG);
	CASE(TEK_BG);
#endif
#if OPT_HIGHLIGHT_COLOR
	CASE(HIGHLIGHT_BG);
	CASE(HIGHLIGHT_FG);
#endif
#if OPT_TEK4014
	CASE(TEK_CURSOR);
#endif
    case OSC_NCOLORS:
	break;
    }
    return result;
}

static Bool
ChangeColorsRequest(XtermWidget xw,
		    int start,
		    char *names,
		    int final)
{
    Bool result = False;
    char *thisName;
    ScrnColors newColors;
    int i, ndx;

    TRACE(("ChangeColorsRequest start=%d, names='%s'\n", start, names));

    if (GetOldColors(xw)) {
	newColors.which = 0;
	for (i = 0; i < NCOLORS; i++) {
	    newColors.names[i] = NULL;
	}
	for (i = start; i < OSC_NCOLORS; i++) {
	    ndx = OscToColorIndex((OscTextColors) i);
	    if (xw->misc.re_verse)
		ndx = oppositeColor(ndx);

	    if (IsEmpty(names)) {
		newColors.names[ndx] = NULL;
	    } else {
		if (names[0] == ';')
		    thisName = NULL;
		else
		    thisName = names;
		names = strchr(names, ';');
		if (names != NULL) {
		    *names++ = '\0';
		}
		if (thisName != 0 && !strcmp(thisName, "?")) {
		    ReportColorRequest(xw, ndx, final);
		} else if (!pOldColors->names[ndx]
			   || (thisName
			       && strcmp(thisName, pOldColors->names[ndx]))) {
		    AllocateTermColor(xw, &newColors, ndx, thisName, False);
		}
	    }
	}

	if (newColors.which != 0) {
	    ChangeColors(xw, &newColors);
	    UpdateOldColors(xw, &newColors);
	}
	result = True;
    }
    return result;
}

static Bool
ResetColorsRequest(XtermWidget xw,
		   int code)
{
    Bool result = False;
    const char *thisName;
    ScrnColors newColors;
    int ndx;

    TRACE(("ResetColorsRequest code=%d\n", code));

#if OPT_COLOR_RES
    if (GetOldColors(xw)) {
	ndx = OscToColorIndex((OscTextColors) (code - OSC_RESET));
	if (xw->misc.re_verse)
	    ndx = oppositeColor(ndx);

	thisName = xw->screen.Tcolors[ndx].resource;

	newColors.which = 0;
	newColors.names[ndx] = NULL;

	if (thisName != 0
	    && pOldColors->names[ndx] != 0
	    && strcmp(thisName, pOldColors->names[ndx])) {
	    AllocateTermColor(xw, &newColors, ndx, thisName, False);

	    if (newColors.which != 0) {
		ChangeColors(xw, &newColors);
		UpdateOldColors(xw, &newColors);
	    }
	}
	result = True;
    }
#endif
    return result;
}

#if OPT_SHIFT_FONTS
/*
 * Initially, 'source' points to '#' or '?'.
 *
 * Look for an optional sign and optional number.  If those are found, lookup
 * the corresponding menu font entry.
 */
static int
ParseShiftedFont(XtermWidget xw, String source, String * target)
{
    TScreen *screen = TScreenOf(xw);
    int num = screen->menu_font_number;
    int rel = 0;

    if (*++source == '+') {
	rel = 1;
	source++;
    } else if (*source == '-') {
	rel = -1;
	source++;
    }

    if (isdigit(CharOf(*source))) {
	int val = atoi(source);
	if (rel > 0)
	    rel = val;
	else if (rel < 0)
	    rel = -val;
	else
	    num = val;
    }

    if (rel != 0) {
	num = lookupRelativeFontSize(xw,
				     screen->menu_font_number, rel);

    }
    TRACE(("ParseShiftedFont(%s) ->%d (%s)\n", *target, num, source));
    *target = source;
    return num;
}

static void
QueryFontRequest(XtermWidget xw, String buf, int final)
{
    if (AllowFontOps(xw, efGetFont)) {
	TScreen *screen = TScreenOf(xw);
	Bool success = True;
	int num;
	String base = buf + 1;
	const char *name = 0;
	char temp[10];

	num = ParseShiftedFont(xw, buf, &buf);
	if (num < 0
	    || num > fontMenu_lastBuiltin) {
	    Bell(xw, XkbBI_MinorError, 0);
	    success = False;
	} else {
#if OPT_RENDERFONT
	    if (UsingRenderFont(xw)) {
		name = getFaceName(xw, False);
	    } else
#endif
	    if ((name = screen->MenuFontName(num)) == 0) {
		success = False;
	    }
	}

	unparseputc1(xw, ANSI_OSC);
	unparseputs(xw, "50");

	if (success) {
	    unparseputc(xw, ';');
	    if (buf >= base) {
		/* identify the font-entry, unless it is the current one */
		if (*buf != '\0') {
		    unparseputc(xw, '#');
		    sprintf(temp, "%d", num);
		    unparseputs(xw, temp);
		    if (*name != '\0')
			unparseputc(xw, ' ');
		}
	    }
	    unparseputs(xw, name);
	}

	unparseputc1(xw, final);
	unparse_end(xw);
    }
}

static void
ChangeFontRequest(XtermWidget xw, String buf)
{
    if (AllowFontOps(xw, efSetFont)) {
	TScreen *screen = TScreenOf(xw);
	Bool success = True;
	int num;
	VTFontNames fonts;
	char *name;

	/*
	 * If the font specification is a "#", followed by an optional sign and
	 * optional number, lookup the corresponding menu font entry.
	 *
	 * Further, if the "#", etc., is followed by a font name, use that
	 * to load the font entry.
	 */
	if (*buf == '#') {
	    num = ParseShiftedFont(xw, buf, &buf);

	    if (num < 0
		|| num > fontMenu_lastBuiltin) {
		Bell(xw, XkbBI_MinorError, 0);
		success = False;
	    } else {
		/*
		 * Skip past the optional number, and any whitespace to look
		 * for a font specification within the control.
		 */
		while (isdigit(CharOf(*buf))) {
		    ++buf;
		}
		while (isspace(CharOf(*buf))) {
		    ++buf;
		}
#if OPT_RENDERFONT
		if (UsingRenderFont(xw)) {
		    /* EMPTY */
		    /* there is only one font entry to load */
		    ;
		} else
#endif
		{
		    /*
		     * Normally there is no font specified in the control.
		     * But if there is, simply overwrite the font entry.
		     */
		    if (*buf == '\0') {
			if ((buf = screen->MenuFontName(num)) == 0) {
			    success = False;
			}
		    }
		}
	    }
	} else {
	    num = screen->menu_font_number;
	}
	name = x_strtrim(buf);
	if (success && !IsEmpty(name)) {
#if OPT_RENDERFONT
	    if (UsingRenderFont(xw)) {
		setFaceName(xw, name);
		xtermUpdateFontInfo(xw, True);
	    } else
#endif
	    {
		memset(&fonts, 0, sizeof(fonts));
		fonts.f_n = name;
		SetVTFont(xw, num, True, &fonts);
	    }
	} else {
	    Bell(xw, XkbBI_MinorError, 0);
	}
	free(name);
    }
}
#endif /* OPT_SHIFT_FONTS */

/***====================================================================***/

void
do_osc(XtermWidget xw, Char * oscbuf, size_t len, int final)
{
    TScreen *screen = TScreenOf(xw);
    int mode;
    Char *cp;
    int state = 0;
    char *buf = 0;
    char temp[2];
#if OPT_ISO_COLORS
    int ansi_colors = 0;
#endif
    Bool need_data = True;

    TRACE(("do_osc %s\n", oscbuf));

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
		if (mode > 65535) {
		    TRACE(("do_osc found unknown mode %d\n", mode));
		    return;
		}
		break;
	    }
	    /* FALLTHRU */
	case 1:
	    if (*cp != ';') {
		TRACE(("do_osc did not find semicolon offset %d\n",
		       (int) (cp - oscbuf)));
		return;
	    }
	    state = 2;
	    break;
	case 2:
	    buf = (char *) cp;
	    state = 3;
	    /* FALLTHRU */
	default:
	    if (!xtermIsPrintable(xw, &cp, oscbuf + len)) {
		switch (mode) {
		case 0:
		case 1:
		case 2:
		    break;
		default:
		    TRACE(("do_osc found nonprinting char %02X offset %d\n",
			   CharOf(*cp),
			   (int) (cp - oscbuf)));
		    return;
		}
	    }
	}
    }

    /*
     * Most OSC controls other than resets require data.  Handle the others as
     * a special case.
     */
    switch (mode) {
#if OPT_ISO_COLORS
    case OSC_Reset(4):
    case OSC_Reset(5):
    case OSC_Reset(OSC_TEXT_FG):
    case OSC_Reset(OSC_TEXT_BG):
    case OSC_Reset(OSC_TEXT_CURSOR):
    case OSC_Reset(OSC_MOUSE_FG):
    case OSC_Reset(OSC_MOUSE_BG):
#if OPT_HIGHLIGHT_COLOR
    case OSC_Reset(OSC_HIGHLIGHT_BG):
    case OSC_Reset(OSC_HIGHLIGHT_FG):
#endif
#if OPT_TEK4014
    case OSC_Reset(OSC_TEK_FG):
    case OSC_Reset(OSC_TEK_BG):
    case OSC_Reset(OSC_TEK_CURSOR):
#endif
	need_data = False;
	break;
#endif
    default:
	break;
    }

    /*
     * Check if we have data when we want, and not when we do not want it.
     * Either way, that is a malformed control sequence, and will be ignored.
     */
    if (IsEmpty(buf)) {
	if (need_data) {
	    TRACE(("do_osc found no data\n"));
	    return;
	}
	temp[0] = '\0';
	buf = temp;
    } else if (!need_data) {
	TRACE(("do_osc found found unwanted data\n"));
	return;
    }

    switch (mode) {
    case 0:			/* new icon name and title */
	ChangeIconName(xw, buf);
	ChangeTitle(xw, buf);
	break;

    case 1:			/* new icon name only */
	ChangeIconName(xw, buf);
	break;

    case 2:			/* new title only */
	ChangeTitle(xw, buf);
	break;

    case 3:			/* change X property */
	if (AllowWindowOps(xw, ewSetXprop))
	    ChangeXprop(buf);
	break;
#if OPT_ISO_COLORS
    case 5:
	ansi_colors = NUM_ANSI_COLORS;
	/* FALLTHRU */
    case 4:
	if (ChangeAnsiColorRequest(xw, buf, ansi_colors, final))
	    xtermRepaint(xw);
	break;
    case OSC_Reset(5):
	ansi_colors = NUM_ANSI_COLORS;
	/* FALLTHRU */
    case OSC_Reset(4):
	if (ResetAnsiColorRequest(xw, buf, ansi_colors))
	    xtermRepaint(xw);
	break;
#endif
    case OSC_TEXT_FG:
    case OSC_TEXT_BG:
    case OSC_TEXT_CURSOR:
    case OSC_MOUSE_FG:
    case OSC_MOUSE_BG:
#if OPT_HIGHLIGHT_COLOR
    case OSC_HIGHLIGHT_BG:
    case OSC_HIGHLIGHT_FG:
#endif
#if OPT_TEK4014
    case OSC_TEK_FG:
    case OSC_TEK_BG:
    case OSC_TEK_CURSOR:
#endif
	if (xw->misc.dynamicColors) {
	    ChangeColorsRequest(xw, mode, buf, final);
	}
	break;
    case OSC_Reset(OSC_TEXT_FG):
    case OSC_Reset(OSC_TEXT_BG):
    case OSC_Reset(OSC_TEXT_CURSOR):
    case OSC_Reset(OSC_MOUSE_FG):
    case OSC_Reset(OSC_MOUSE_BG):
#if OPT_HIGHLIGHT_COLOR
    case OSC_Reset(OSC_HIGHLIGHT_BG):
    case OSC_Reset(OSC_HIGHLIGHT_FG):
#endif
#if OPT_TEK4014
    case OSC_Reset(OSC_TEK_FG):
    case OSC_Reset(OSC_TEK_BG):
    case OSC_Reset(OSC_TEK_CURSOR):
#endif
	if (xw->misc.dynamicColors) {
	    ResetColorsRequest(xw, mode);
	}
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
	if (strcmp(buf, "?")
	    && (cp = CastMallocN(char, strlen(buf)) != NULL)) {
	    strcpy(cp, buf);
	    if (screen->logfile)
		free(screen->logfile);
	    screen->logfile = cp;
	    break;
	}
#endif
	Bell(xw, XkbBI_Info, 0);
	Bell(xw, XkbBI_Info, 0);
	break;
#endif /* ALLOWLOGGING */

    case 50:
#if OPT_SHIFT_FONTS
	if (*buf == '?') {
	    QueryFontRequest(xw, buf, final);
	} else if (xw->misc.shift_fonts) {
	    ChangeFontRequest(xw, buf);
	}
#endif /* OPT_SHIFT_FONTS */
	break;
    case 51:
	/* reserved for Emacs shell (Rob Mayoff <mayoff@dqd.com>) */
	break;

#if OPT_PASTE64
    case 52:
	ManipulateSelectionData(xw, screen, buf, final);
	break;
#endif
	/*
	 * One could write code to send back the display and host names,
	 * but that could potentially open a fairly nasty security hole.
	 */
    default:
	TRACE(("do_osc - unrecognized code\n"));
	break;
    }
    unparse_end(xw);
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
udk_value(const char **cp)
{
    int result = -1;
    int c;

    for (;;) {
	if ((c = **cp) != '\0')
	    *cp = *cp + 1;
	if (c == ';' || c == '\0')
	    break;
	if ((result = x_hex2int(c)) >= 0)
	    break;
    }

    return result;
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

/*
 * Parse the data for DECUDK (user-defined keys).
 */
static void
parse_decudk(const char *cp)
{
    while (*cp) {
	const char *base = cp;
	char *str = CastMallocN(char, strlen(cp) + 1);
	unsigned key = 0;
	int lo, hi;
	int len = 0;

	while (isdigit(CharOf(*cp)))
	    key = (key * 10) + (unsigned) (*cp++ - '0');
	if (*cp == '/') {
	    cp++;
	    while ((hi = udk_value(&cp)) >= 0
		   && (lo = udk_value(&cp)) >= 0) {
		str[len++] = (char) ((hi << 4) | lo);
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
	if (cp == base)		/* badly-formed sequence - bail out */
	    break;
    }
}

#if OPT_TRACE
#define SOFT_WIDE 10
#define SOFT_HIGH 20

static void
parse_decdld(ANSI * params, const char *string)
{
    char DscsName[8];
    int len;
    int Pfn = params->a_param[0];
    int Pcn = params->a_param[1];
    int Pe = params->a_param[2];
    int Pcmw = params->a_param[3];
    int Pw = params->a_param[4];
    int Pt = params->a_param[5];
    int Pcmh = params->a_param[6];
    int Pcss = params->a_param[7];

    int start_char = Pcn + 0x20;
    int char_wide = ((Pcmw == 0)
		     ? (Pcss ? 6 : 10)
		     : (Pcmw > 4
			? Pcmw
			: (Pcmw + 3)));
    int char_high = ((Pcmh == 0)
		     ? ((Pcmw >= 2 || Pcmw <= 4)
			? 10
			: 20)
		     : Pcmh);
    Char ch;
    Char bits[SOFT_HIGH][SOFT_WIDE];
    Bool first = True;
    Bool prior = False;
    int row = 0, col = 0;

    TRACE(("Parsing DECDLD\n"));
    TRACE(("  font number   %d\n", Pfn));
    TRACE(("  starting char %d\n", Pcn));
    TRACE(("  erase control %d\n", Pe));
    TRACE(("  char-width    %d\n", Pcmw));
    TRACE(("  font-width    %d\n", Pw));
    TRACE(("  text/full     %d\n", Pt));
    TRACE(("  char-height   %d\n", Pcmh));
    TRACE(("  charset-size  %d\n", Pcss));

    if (Pfn > 1
	|| Pcn > 95
	|| Pe > 2
	|| Pcmw > 10
	|| Pcmw == 1
	|| Pt > 2
	|| Pcmh > 20
	|| Pcss > 1
	|| char_wide > SOFT_WIDE
	|| char_high > SOFT_HIGH) {
	TRACE(("DECDLD illegal parameter\n"));
	return;
    }

    len = 0;
    while (*string != '\0') {
	ch = CharOf(*string++);
	if (ch >= ANSI_SPA && ch <= 0x2f) {
	    if (len < 2)
		DscsName[len++] = (char) ch;
	} else if (ch >= 0x30 && ch <= 0x7e) {
	    DscsName[len++] = (char) ch;
	    break;
	}
    }
    DscsName[len] = 0;
    TRACE(("  Dscs name     '%s'\n", DscsName));

    TRACE(("  character matrix %dx%d\n", char_high, char_wide));
    while (*string != '\0') {
	if (first) {
	    TRACE(("Char %d:\n", start_char));
	    if (prior) {
		for (row = 0; row < char_high; ++row) {
		    TRACE(("%.*s\n", char_wide, bits[row]));
		}
	    }
	    prior = False;
	    first = False;
	    for (row = 0; row < char_high; ++row) {
		for (col = 0; col < char_wide; ++col) {
		    bits[row][col] = '.';
		}
	    }
	    row = col = 0;
	}
	ch = CharOf(*string++);
	if (ch >= 0x3f && ch <= 0x7e) {
	    int n;

	    ch = CharOf(ch - 0x3f);
	    for (n = 0; n < 6; ++n) {
		bits[row + n][col] = CharOf((ch & (1 << n)) ? '*' : '.');
	    }
	    col += 1;
	    prior = True;
	} else if (ch == '/') {
	    row += 6;
	    col = 0;
	} else if (ch == ';') {
	    first = True;
	    ++start_char;
	}
    }
}
#else
#define parse_decdld(p,q)	/* nothing */
#endif

/*
 * Parse numeric parameters.  Normally we use a state machine to simplify
 * interspersing with control characters, but have the string already.
 */
static void
parse_ansi_params(ANSI * params, const char **string)
{
    const char *cp = *string;
    ParmType nparam = 0;

    memset(params, 0, sizeof(*params));
    while (*cp != '\0') {
	Char ch = CharOf(*cp++);

	if (isdigit(ch)) {
	    if (nparam < NPARAM) {
		params->a_param[nparam] =
		    (ParmType) ((params->a_param[nparam] * 10)
				+ (ch - '0'));
	    }
	} else if (ch == ';') {
	    if (++nparam < NPARAM)
		params->a_nparam = nparam;
	} else if (ch < 32) {
	    /* EMPTY */ ;
	} else {
	    /* should be 0x30 to 0x7e */
	    params->a_final = ch;
	    break;
	}
    }
    *string = cp;
}

void
do_dcs(XtermWidget xw, Char * dcsbuf, size_t dcslen)
{
    TScreen *screen = TScreenOf(xw);
    char reply[BUFSIZ];
    const char *cp = (const char *) dcsbuf;
    Bool okay;
    ANSI params;

    TRACE(("do_dcs(%s:%lu)\n", (char *) dcsbuf, (unsigned long) dcslen));

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
			&& (xw->flags & PROTECTED) ? 1 : 0,
			cp);
	    } else if (!strcmp(cp, "\"p")) {	/* DECSCL */
		sprintf(reply, "%d%s%s",
			(screen->vtXX_level ?
			 screen->vtXX_level : 1) + 60,
			(screen->vtXX_level >= 2)
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
		if (xw->flags & BOLD)
		    strcat(reply, ";1");
		if (xw->flags & UNDERLINE)
		    strcat(reply, ";4");
		if (xw->flags & BLINK)
		    strcat(reply, ";5");
		if (xw->flags & INVERSE)
		    strcat(reply, ";7");
		if (xw->flags & INVISIBLE)
		    strcat(reply, ";8");
#if OPT_256_COLORS || OPT_88_COLORS
		if_OPT_ISO_COLORS(screen, {
		    if (xw->flags & FG_COLOR) {
			if (xw->cur_foreground >= 16)
			    sprintf(reply + strlen(reply),
				    ";38;5;%d", xw->cur_foreground);
			else
			    sprintf(reply + strlen(reply),
				    ";%d%d",
				    xw->cur_foreground >= 8 ? 9 : 3,
				    xw->cur_foreground >= 8 ?
				    xw->cur_foreground - 8 :
				    xw->cur_foreground);
		    }
		    if (xw->flags & BG_COLOR) {
			if (xw->cur_background >= 16)
			    sprintf(reply + strlen(reply),
				    ";48;5;%d", xw->cur_foreground);
			else
			    sprintf(reply + strlen(reply),
				    ";%d%d",
				    xw->cur_background >= 8 ? 10 : 4,
				    xw->cur_background >= 8 ?
				    xw->cur_background - 8 :
				    xw->cur_background);
		    }
		});
#elif OPT_ISO_COLORS
		if_OPT_ISO_COLORS(screen, {
		    if (xw->flags & FG_COLOR)
			sprintf(reply + strlen(reply),
				";%d%d",
				xw->cur_foreground >= 8 ? 9 : 3,
				xw->cur_foreground >= 8 ?
				xw->cur_foreground - 8 :
				xw->cur_foreground);
		    if (xw->flags & BG_COLOR)
			sprintf(reply + strlen(reply),
				";%d%d",
				xw->cur_background >= 8 ? 10 : 4,
				xw->cur_background >= 8 ?
				xw->cur_background - 8 :
				xw->cur_background);
		});
#endif
		strcat(reply, "m");
	    } else
		okay = False;

	    if (okay) {
		unparseputc1(xw, ANSI_DCS);
		unparseputc(xw, okay ? '1' : '0');
		unparseputc(xw, '$');
		unparseputc(xw, 'r');
		cp = reply;
		unparseputs(xw, cp);
		unparseputc1(xw, ANSI_ST);
	    } else {
		unparseputc(xw, ANSI_CAN);
	    }
	} else {
	    unparseputc(xw, ANSI_CAN);
	}
	break;
#if OPT_TCAP_QUERY
    case '+':
	cp++;
	switch (*cp) {
	case 'p':
	    if (AllowTcapOps(xw, etSetTcap)) {
		set_termcap(xw, cp + 1);
	    }
	    break;
	case 'q':
	    if (AllowTcapOps(xw, etGetTcap)) {
		Bool fkey;
		unsigned state;
		int code;
		const char *tmp;
		const char *parsed = ++cp;

		code = xtermcapKeycode(xw, &parsed, &state, &fkey);

		unparseputc1(xw, ANSI_DCS);

		unparseputc(xw, code >= 0 ? '1' : '0');

		unparseputc(xw, '+');
		unparseputc(xw, 'r');

		while (*cp != 0 && (code >= -1)) {
		    if (cp == parsed)
			break;	/* no data found, error */

		    for (tmp = cp; tmp != parsed; ++tmp)
			unparseputc(xw, *tmp);

		    if (code >= 0) {
			unparseputc(xw, '=');
			screen->tc_query_code = code;
			screen->tc_query_fkey = fkey;
#if OPT_ISO_COLORS
			/* XK_COLORS is a fake code for the "Co" entry (maximum
			 * number of colors) */
			if (code == XK_COLORS) {
			    unparseputn(xw, NUM_ANSI_COLORS);
			} else
#endif
			if (code == XK_TCAPNAME) {
			    unparseputs(xw, resource.term_name);
			} else {
			    XKeyEvent event;
			    event.state = state;
			    Input(xw, &event, False);
			}
			screen->tc_query_code = -1;
		    } else {
			break;	/* no match found, error */
		    }

		    cp = parsed;
		    if (*parsed == ';') {
			unparseputc(xw, *parsed++);
			cp = parsed;
			code = xtermcapKeycode(xw, &parsed, &state, &fkey);
		    }
		}
		unparseputc1(xw, ANSI_ST);
	    }
	    break;
	}
	break;
#endif
    default:
	if (screen->terminal_id >= 200) {	/* VT220 */
	    parse_ansi_params(&params, &cp);
	    switch (params.a_final) {
	    case '|':		/* DECUDK */
		if (params.a_param[0] == 0)
		    reset_decudk();
		parse_decudk(cp);
		break;
	    case '{':		/* DECDLD (no '}' case though) */
		parse_decdld(&params, cp);
		break;
	    }
	}
	break;
    }
    unparse_end(xw);
}

#if OPT_DEC_RECTOPS
enum {
    mdUnknown = 0,
    mdMaybeSet = 1,
    mdMaybeReset = 2,
    mdAlwaysSet = 3,
    mdAlwaysReset = 4
};

#define MdBool(bool)      ((bool) ? mdMaybeSet : mdMaybeReset)
#define MdFlag(mode,flag) MdBool(xw->keyboard.flags & MODE_KAM)

/*
 * Reply is the same format as the query, with pair of mode/value:
 * 0 - not recognized
 * 1 - set
 * 2 - reset
 * 3 - permanently set
 * 4 - permanently reset
 * Only one mode can be reported at a time.
 */
void
do_rpm(XtermWidget xw, int nparams, int *params)
{
    ANSI reply;
    int result = 0;
    int count = 0;

    TRACE(("do_rpm %d:%d\n", nparams, params[0]));
    memset(&reply, 0, sizeof(reply));
    if (nparams >= 1) {
	switch (params[0]) {
	case 1:		/* GATM */
	    result = mdAlwaysReset;
	    break;
	case 2:
	    result = MdFlag(xw->keyboard.flags, MODE_KAM);
	    break;
	case 3:		/* CRM */
	    result = mdMaybeReset;
	    break;
	case 4:
	    result = MdFlag(xw->flags, INSERT);
	    break;
	case 5:		/* SRTM */
	case 7:		/* VEM */
	case 10:		/* HEM */
	case 11:		/* PUM */
	    result = mdAlwaysReset;
	    break;
	case 12:
	    result = MdFlag(xw->keyboard.flags, MODE_SRM);
	    break;
	case 13:		/* FEAM */
	case 14:		/* FETM */
	case 15:		/* MATM */
	case 16:		/* TTM */
	case 17:		/* SATM */
	case 18:		/* TSM */
	case 19:		/* EBM */
	    result = mdAlwaysReset;
	    break;
	case 20:
	    result = MdFlag(xw->flags, LINEFEED);
	    break;
	}
	reply.a_param[count++] = (ParmType) params[0];
	reply.a_param[count++] = (ParmType) result;
    }
    reply.a_type = ANSI_CSI;
    reply.a_nparam = (ParmType) count;
    reply.a_inters = '$';
    reply.a_final = 'y';
    unparseseq(xw, &reply);
}

void
do_decrpm(XtermWidget xw, int nparams, int *params)
{
    ANSI reply;
    int result = 0;
    int count = 0;

    TRACE(("do_decrpm %d:%d\n", nparams, params[0]));
    memset(&reply, 0, sizeof(reply));
    if (nparams >= 1) {
	TScreen *screen = TScreenOf(xw);

	switch (params[0]) {
	case 1:		/* DECCKM                       */
	    result = MdFlag(xw->keyboard.flags, MODE_DECCKM);
	    break;
	case 2:		/* DECANM - ANSI/VT52 mode      */
#if OPT_VT52_MODE
	    result = MdBool(screen->terminal_id >= 100);
#else
	    result = mdMaybeSet;
#endif
	    break;
	case 3:		/* DECCOLM                      */
	    result = MdFlag(xw->flags, IN132COLUMNS);
	    break;
	case 4:		/* DECSCLM (slow scroll)        */
	    result = MdFlag(xw->flags, SMOOTHSCROLL);
	    break;
	case 5:		/* DECSCNM                      */
	    result = MdFlag(xw->flags, REVERSE_VIDEO);
	    break;
	case 6:		/* DECOM                        */
	    result = MdFlag(xw->flags, ORIGIN);
	    break;
	case 7:		/* DECAWM                       */
	    result = MdFlag(xw->flags, WRAPAROUND);
	    break;
	case 8:		/* DECARM                       */
	    result = mdAlwaysReset;
	    break;
	case SET_X10_MOUSE:	/* X10 mouse                    */
	    result = MdBool(screen->send_mouse_pos == X10_MOUSE);
	    break;
#if OPT_TOOLBAR
	case 10:		/* rxvt */
	    result = MdBool(resource.toolBar);
	    break;
#endif
#if OPT_BLINK_CURS
	case 12:		/* att610: Start/stop blinking cursor */
	    result = MdBool(screen->cursor_blink_res);
	    break;
#endif
	case 18:		/* DECPFF: print form feed */
	    result = MdBool(screen->printer_formfeed);
	    break;
	case 19:		/* DECPEX: print extent */
	    result = MdBool(screen->printer_extent);
	    break;
	case 25:		/* DECTCEM: Show/hide cursor (VT200) */
	    result = MdBool(screen->cursor_set);
	    break;
	case 30:		/* rxvt */
	    result = MdBool(screen->fullVwin.sb_info.width != OFF);
	    break;
#if OPT_SHIFT_FONTS
	case 35:		/* rxvt */
	    result = MdBool(xw->misc.shift_fonts);
	    break;
#endif
#if OPT_TEK4014
	case 38:		/* DECTEK                       */
	    result = MdBool(TEK4014_ACTIVE(xw));
	    break;
#endif
	case 40:		/* 132 column mode              */
	    result = MdBool(screen->c132);
	    break;
	case 41:		/* curses hack                  */
	    result = MdBool(screen->curses);
	    break;
	case 42:		/* DECNRCM national charset (VT220) */
	    result = MdFlag(xw->flags, NATIONAL);
	    break;
	case 44:		/* margin bell                  */
	    result = MdBool(screen->marginbell);
	    break;
	case 45:		/* reverse wraparound   */
	    result = MdFlag(xw->flags, REVERSEWRAP);
	    break;
#ifdef ALLOWLOGGING
	case 46:		/* logging              */
#ifdef ALLOWLOGFILEONOFF
	    result = MdBool(screen->logging);
#endif /* ALLOWLOGFILEONOFF */
	    break;
#endif
	case 1049:		/* alternate buffer & cursor */
	    /* FALLTHRU */
	case 1047:
	    /* FALLTHRU */
	case 47:		/* alternate buffer */
	    result = MdBool(screen->whichBuf);
	    break;
	case 66:		/* DECNKM */
	    result = MdFlag(xw->keyboard.flags, MODE_DECKPAM);
	    break;
	case 67:		/* DECBKM */
	    result = MdFlag(xw->keyboard.flags, MODE_DECBKM);
	    break;
	case SET_VT200_MOUSE:	/* xterm bogus sequence         */
	    result = MdBool(screen->send_mouse_pos == VT200_MOUSE);
	    break;
	case SET_VT200_HIGHLIGHT_MOUSE:	/* xterm sequence w/hilite tracking */
	    result = MdBool(screen->send_mouse_pos == VT200_HIGHLIGHT_MOUSE);
	    break;
	case SET_BTN_EVENT_MOUSE:
	    result = MdBool(screen->send_mouse_pos == BTN_EVENT_MOUSE);
	    break;
	case SET_ANY_EVENT_MOUSE:
	    result = MdBool(screen->send_mouse_pos == ANY_EVENT_MOUSE);
	    break;
#if OPT_FOCUS_EVENT
	case SET_FOCUS_EVENT_MOUSE:
	    result = MdBool(screen->send_focus_pos);
	    break;
#endif
	case SET_EXT_MODE_MOUSE:
	    result = MdBool(screen->ext_mode_mouse);
	    break;
	case 1010:		/* rxvt */
	    result = MdBool(screen->scrollttyoutput);
	    break;
	case 1011:		/* rxvt */
	    result = MdBool(screen->scrollkey);
	    break;
	case 1034:
	    result = MdBool(screen->input_eight_bits);
	    break;
#if OPT_NUM_LOCK
	case 1035:
	    result = MdBool(xw->misc.real_NumLock);
	    break;
	case 1036:
	    result = MdBool(screen->meta_sends_esc);
	    break;
#endif
	case 1037:
	    result = MdBool(screen->delete_is_del);
	    break;
#if OPT_NUM_LOCK
	case 1039:
	    result = MdBool(screen->alt_sends_esc);
	    break;
#endif
	case 1040:
	    result = MdBool(screen->keepSelection);
	    break;
	case 1041:
	    result = MdBool(screen->selectToClipboard);
	    break;
	case 1042:
	    result = MdBool(screen->bellIsUrgent);
	    break;
	case 1043:
	    result = MdBool(screen->poponbell);
	    break;
	case 1048:
	    result = MdBool(screen->sc[screen->whichBuf].saved);
	    break;
#if OPT_TCAP_FKEYS
	case 1050:
	    result = MdBool(xw->keyboard.type == keyboardIsTermcap);
	    break;
#endif
#if OPT_SUN_FUNC_KEYS
	case 1051:
	    result = MdBool(xw->keyboard.type == keyboardIsSun);
	    break;
#endif
#if OPT_HP_FUNC_KEYS
	case 1052:
	    result = MdBool(xw->keyboard.type == keyboardIsHP);
	    break;
#endif
#if OPT_SCO_FUNC_KEYS
	case 1053:
	    result = MdBool(xw->keyboard.type == keyboardIsSCO);
	    break;
#endif
	case 1060:
	    result = MdBool(xw->keyboard.type == keyboardIsLegacy);
	    break;
#if OPT_SUNPC_KBD
	case 1061:
	    result = MdBool(xw->keyboard.type == keyboardIsVT220);
	    break;
#endif
#if OPT_READLINE
	case SET_BUTTON1_MOVE_POINT:
	    result = MdBool(screen->click1_moves);
	    break;
	case SET_BUTTON2_MOVE_POINT:
	    result = MdBool(screen->paste_moves);
	    break;
	case SET_DBUTTON3_DELETE:
	    result = MdBool(screen->dclick3_deletes);
	    break;
	case SET_PASTE_IN_BRACKET:
	    result = MdBool(screen->paste_brackets);
	    break;
	case SET_PASTE_QUOTE:
	    result = MdBool(screen->paste_quotes);
	    break;
	case SET_PASTE_LITERAL_NL:
	    result = MdBool(screen->paste_literal_nl);
	    break;
#endif /* OPT_READLINE */
	}
	reply.a_param[count++] = (ParmType) params[0];
	reply.a_param[count++] = (ParmType) result;
    }
    reply.a_type = ANSI_CSI;
    reply.a_pintro = '?';
    reply.a_nparam = (ParmType) count;
    reply.a_inters = '$';
    reply.a_final = 'y';
    unparseseq(xw, &reply);
}
#endif /* OPT_DEC_RECTOPS */

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
ChangeGroup(XtermWidget xw, const char *attribute, char *value)
{
#if OPT_WIDE_CHARS
    static Char *converted;	/* NO_LEAKS */
#endif
    static char empty[1];

    Arg args[1];
    Boolean changed = True;
    Widget w = CURRENT_EMU();
    Widget top = SHELL_OF(w);

    char *my_attr;
    char *name;
    size_t limit;
    Char *c1;
    Char *cp;

    if (!AllowTitleOps(xw))
	return;

    if (value == 0)
	value = empty;
    if (IsTitleMode(xw, tmSetBase16)) {
	const char *temp;
	char *test;

	value = x_decode_hex(value, &temp);
	if (*temp != '\0')
	    return;
	for (test = value; *test != '\0'; ++test) {
	    if (CharOf(*test) < 32) {
		*test = '\0';
		break;
	    }
	}
    }

    c1 = (Char *) value;
    name = value;
    limit = strlen(name);
    my_attr = x_strdup(attribute);

    TRACE(("ChangeGroup(attribute=%s, value=%s)\n", my_attr, name));

    /*
     * Ignore titles that are too long to be plausible requests.
     */
    if (limit > 0 && limit < 1024) {

	/*
	 * After all decoding, overwrite nonprintable characters with '?'.
	 */
	for (cp = c1; *cp != 0; ++cp) {
	    Char *c2 = cp;
	    if (!xtermIsPrintable(xw, &cp, c1 + limit)) {
		memset(c2, '?', (size_t) (cp + 1 - c2));
	    }
	}

#if OPT_WIDE_CHARS
	/*
	 * If we're running in UTF-8 mode, and have not been told that the
	 * title string is in UTF-8, it is likely that non-ASCII text in the
	 * string will be rejected because it is not printable in the current
	 * locale.  So we convert it to UTF-8, allowing the X library to
	 * convert it back.
	 */
	if (xtermEnvUTF8() && !IsSetUtf8Title(xw)) {
	    int n;

	    for (n = 0; name[n] != '\0'; ++n) {
		if (CharOf(name[n]) > 127) {
		    if (converted != 0)
			free(converted);
		    if ((converted = TypeMallocN(Char, 1 + (6 * limit))) != 0) {
			Char *temp = converted;
			while (*name != 0) {
			    temp = convertToUTF8(temp, CharOf(*name));
			    ++name;
			}
			*temp = 0;
			name = (char *) converted;
			TRACE(("...converted{%s}\n", name));
		    }
		    break;
		}
	    }
	}
#endif

#if OPT_SAME_NAME
	/* If the attribute isn't going to change, then don't bother... */

	if (resource.sameName) {
	    char *buf = 0;
	    XtSetArg(args[0], my_attr, &buf);
	    XtGetValues(top, args, 1);
	    TRACE(("...comparing{%s}\n", buf));
	    if (buf != 0 && strcmp(name, buf) == 0)
		changed = False;
	}
#endif /* OPT_SAME_NAME */

	if (changed) {
	    TRACE(("...updating %s\n", my_attr));
	    TRACE(("...value is %s\n", name));
	    XtSetArg(args[0], my_attr, name);
	    XtSetValues(top, args, 1);

#if OPT_WIDE_CHARS
	    if (xtermEnvUTF8()) {
		Display *dpy = XtDisplay(xw);
		Atom my_atom;

		const char *propname = (!strcmp(my_attr, XtNtitle)
					? "_NET_WM_NAME"
					: "_NET_WM_ICON_NAME");
		if ((my_atom = XInternAtom(dpy, propname, False)) != None) {
		    if (IsSetUtf8Title(xw)) {
			TRACE(("...updating %s\n", propname));
			TRACE(("...value is %s\n", value));
			XChangeProperty(dpy, VShellWindow(xw), my_atom,
					XA_UTF8_STRING(dpy), 8,
					PropModeReplace,
					(Char *) value,
					(int) strlen(value));
		    } else {
			TRACE(("...deleting %s\n", propname));
			XDeleteProperty(dpy, VShellWindow(xw), my_atom);
		    }
		}
	    }
#endif
	}

	free(my_attr);

	if (IsTitleMode(xw, tmSetBase16))
	    free(value);

    }
    return;
}

void
ChangeIconName(XtermWidget xw, char *name)
{
    if (name == 0) {
	static char dummy[] = "";
	name = dummy;
    }
#if OPT_ZICONBEEP		/* If warning should be given then give it */
    if (resource.zIconBeep && TScreenOf(xw)->zIconBeep_flagged) {
	char *newname = CastMallocN(char, strlen(name) + 4);
	if (!newname) {
	    fprintf(stderr, "malloc failed in ChangeIconName\n");
	    return;
	}
	strcpy(newname, "*** ");
	strcat(newname, name);
	ChangeGroup(xw, XtNiconName, newname);
	free(newname);
    } else
#endif /* OPT_ZICONBEEP */
	ChangeGroup(xw, XtNiconName, name);
}

void
ChangeTitle(XtermWidget xw, char *name)
{
    ChangeGroup(xw, XtNtitle, name);
}

#define Strlen(s) strlen((char *)(s))

void
ChangeXprop(char *buf)
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

/*
 * This is part of ReverseVideo().  It reverses the data stored for the old
 * "dynamic" colors that might have been retrieved using OSC 10-18.
 */
void
ReverseOldColors(void)
{
    ScrnColors *pOld = pOldColors;
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
		if ((tmpName = x_strdup(pOld->names[TEXT_BG])) != 0) {
		    pOld->names[TEXT_CURSOR] = tmpName;
		}
	    }
	}

	EXCHANGE(pOld->colors[TEXT_FG], pOld->colors[TEXT_BG], tmpPix);
	EXCHANGE(pOld->names[TEXT_FG], pOld->names[TEXT_BG], tmpName);

	EXCHANGE(pOld->colors[MOUSE_FG], pOld->colors[MOUSE_BG], tmpPix);
	EXCHANGE(pOld->names[MOUSE_FG], pOld->names[MOUSE_BG], tmpName);

#if OPT_TEK4014
	EXCHANGE(pOld->colors[TEK_FG], pOld->colors[TEK_BG], tmpPix);
	EXCHANGE(pOld->names[TEK_FG], pOld->names[TEK_BG], tmpName);
#endif
    }
    return;
}

Bool
AllocateTermColor(XtermWidget xw,
		  ScrnColors * pNew,
		  int ndx,
		  const char *name,
		  Bool always)
{
    Bool result = False;

    if (always || AllowColorOps(xw, ecSetColor)) {
	XColor def;
	TScreen *screen = TScreenOf(xw);
	Colormap cmap = xw->core.colormap;
	char *newName;

	if (XParseColor(screen->display, cmap, name, &def)
	    && (XAllocColor(screen->display, cmap, &def)
		|| find_closest_color(screen->display, cmap, &def))
	    && (newName = x_strdup(name)) != 0) {
	    if (COLOR_DEFINED(pNew, ndx))
		free(pNew->names[ndx]);
	    SET_COLOR_VALUE(pNew, ndx, def.pixel);
	    SET_COLOR_NAME(pNew, ndx, newName);
	    TRACE(("AllocateTermColor #%d: %s (pixel %#lx)\n", ndx, newName, def.pixel));
	    result = True;
	} else {
	    TRACE(("AllocateTermColor #%d: %s (failed)\n", ndx, name));
	}
    }
    return result;
}
/***====================================================================***/

/* ARGSUSED */
void
Panic(const char *s GCC_UNUSED, int a GCC_UNUSED)
{
#ifdef DEBUG
    if (debug) {
	fprintf(stderr, "%s: PANIC!\t", ProgramName);
	fprintf(stderr, s, a);
	fputs("\r\n", stderr);
	fflush(stderr);
    }
#endif /* DEBUG */
}

const char *
SysErrorMsg(int code)
{
    static char unknown[] = "unknown error";
    char *s = strerror(code);
    return s ? s : unknown;
}

const char *
SysReasonMsg(int code)
{
    /* *INDENT-OFF* */
    static const struct {
	int code;
	const char *name;
    } table[] = {
	{ ERROR_FIONBIO,	"main:  ioctl() failed on FIONBIO" },
	{ ERROR_F_GETFL,	"main: ioctl() failed on F_GETFL" },
	{ ERROR_F_SETFL,	"main: ioctl() failed on F_SETFL", },
	{ ERROR_OPDEVTTY,	"spawn: open() failed on /dev/tty", },
	{ ERROR_TIOCGETP,	"spawn: ioctl() failed on TIOCGETP", },
	{ ERROR_PTSNAME,	"spawn: ptsname() failed", },
	{ ERROR_OPPTSNAME,	"spawn: open() failed on ptsname", },
	{ ERROR_PTEM,		"spawn: ioctl() failed on I_PUSH/\"ptem\"" },
	{ ERROR_CONSEM,		"spawn: ioctl() failed on I_PUSH/\"consem\"" },
	{ ERROR_LDTERM,		"spawn: ioctl() failed on I_PUSH/\"ldterm\"" },
	{ ERROR_TTCOMPAT,	"spawn: ioctl() failed on I_PUSH/\"ttcompat\"" },
	{ ERROR_TIOCSETP,	"spawn: ioctl() failed on TIOCSETP" },
	{ ERROR_TIOCSETC,	"spawn: ioctl() failed on TIOCSETC" },
	{ ERROR_TIOCSETD,	"spawn: ioctl() failed on TIOCSETD" },
	{ ERROR_TIOCSLTC,	"spawn: ioctl() failed on TIOCSLTC" },
	{ ERROR_TIOCLSET,	"spawn: ioctl() failed on TIOCLSET" },
	{ ERROR_INIGROUPS,	"spawn: initgroups() failed" },
	{ ERROR_FORK,		"spawn: fork() failed" },
	{ ERROR_EXEC,		"spawn: exec() failed" },
	{ ERROR_PTYS,		"get_pty: not enough ptys" },
	{ ERROR_PTY_EXEC,	"waiting for initial map" },
	{ ERROR_SETUID,		"spawn: setuid() failed" },
	{ ERROR_INIT,		"spawn: can't initialize window" },
	{ ERROR_TIOCKSET,	"spawn: ioctl() failed on TIOCKSET" },
	{ ERROR_TIOCKSETC,	"spawn: ioctl() failed on TIOCKSETC" },
	{ ERROR_LUMALLOC,	"luit: command-line malloc failed" },
	{ ERROR_SELECT,		"in_put: select() failed" },
	{ ERROR_VINIT,		"VTInit: can't initialize window" },
	{ ERROR_KMMALLOC1,	"HandleKeymapChange: malloc failed" },
	{ ERROR_TSELECT,	"Tinput: select() failed" },
	{ ERROR_TINIT,		"TekInit: can't initialize window" },
	{ ERROR_BMALLOC2,	"SaltTextAway: malloc() failed" },
	{ ERROR_LOGEXEC,	"StartLog: exec() failed" },
	{ ERROR_XERROR,		"xerror: XError event" },
	{ ERROR_XIOERROR,	"xioerror: X I/O error" },
	{ ERROR_SCALLOC,	"Alloc: calloc() failed on base" },
	{ ERROR_SCALLOC2,	"Alloc: calloc() failed on rows" },
	{ ERROR_SAVE_PTR,	"ScrnPointers: malloc/realloc() failed" },
	{ ERROR_MMALLOC,	"my_memmove: malloc/realloc failed" },
    };
    /* *INDENT-ON* */

    Cardinal n;
    const char *result = "?";

    for (n = 0; n < XtNumber(table); ++n) {
	if (code == table[n].code) {
	    result = table[n].name;
	    break;
	}
    }
    return result;
}

void
SysError(int code)
{
    int oerrno = errno;

    fprintf(stderr, "%s: Error %d, errno %d: ", ProgramName, code, oerrno);
    fprintf(stderr, "%s\n", SysErrorMsg(oerrno));
    fprintf(stderr, "Reason: %s\n", SysReasonMsg(code));

    Cleanup(code);
}

/*
 * cleanup by sending SIGHUP to client processes
 */
void
Cleanup(int code)
{
    static Bool cleaning;
    TScreen *screen = TScreenOf(term);

    /*
     * Process "-hold" and session cleanup only for a normal exit.
     */
    if (code == 0) {
	if (cleaning) {
	    hold_screen = 0;
	    return;
	}

	cleaning = True;
	need_cleanup = False;

	TRACE(("Cleanup %d\n", code));

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
			  NULL);
	}
#endif
    }

    if (screen->pid > 1) {
	(void) kill_process_group(screen->pid, SIGHUP);
    }
    Exit(code);
}

#ifndef VMS
char *
xtermFindShell(char *leaf, Bool warning)
{
    char *s;
    char *d;
    char *tmp;
    char *result = leaf;

    TRACE(("xtermFindShell(%s)\n", leaf));
    if (*result != '\0' && strchr("+/-", *result) == 0) {
	/* find it in $PATH */
	if ((s = x_getenv("PATH")) != 0) {
	    if ((tmp = TypeMallocN(char, strlen(leaf) + strlen(s) + 2)) != 0) {
		Bool found = False;
		while (*s != '\0') {
		    strcpy(tmp, s);
		    for (d = tmp;; ++d) {
			if (*d == ':' || *d == '\0') {
			    int skip = (*d != '\0');
			    *d = '/';
			    strcpy(d + 1, leaf);
			    if (skip)
				++d;
			    s += (d - tmp);
			    if (*tmp == '/'
				&& strstr(tmp, "..") == 0
				&& access(tmp, X_OK) == 0) {
				result = x_strdup(tmp);
				found = True;
			    }
			    break;
			}
			if (found)
			    break;
		    }
		    if (found)
			break;
		}
		free(tmp);
	    }
	}
    }
    TRACE(("...xtermFindShell(%s)\n", result));
    if (*result != '/'
	|| strstr(result, "..") != 0
	|| access(result, X_OK) != 0) {
	if (warning)
	    fprintf(stderr, "No absolute path found for shell: %s\n", result);
	result = 0;
    }
    return result;
}
#endif /* VMS */

#define ENV_HUNK(n)	(unsigned) ((((n) + 1) | 31) + 1)

/*
 * copy the environment before Setenv'ing.
 */
void
xtermCopyEnv(char **oldenv)
{
    unsigned size;
    char **newenv;

    for (size = 0; oldenv[size] != NULL; size++) {
	;
    }

    newenv = TypeCallocN(char *, ENV_HUNK(size));
    memmove(newenv, oldenv, size * sizeof(char *));
    environ = newenv;
}

/*
 * sets the value of var to be arg in the Unix 4.2 BSD environment env.
 * Var should end with '=' (bindings are of the form "var=value").
 * This procedure assumes the memory for the first level of environ
 * was allocated using calloc, with enough extra room at the end so not
 * to have to do a realloc().
 */
void
xtermSetenv(const char *var, const char *value)
{
    if (value != 0) {
	char *test;
	int envindex = 0;
	size_t len = strlen(var);
	int found = -1;

	TRACE(("xtermSetenv(%s=%s)\n", var, value));

	while ((test = environ[envindex]) != NULL) {
	    if (strncmp(test, var, len) == 0 && test[len] == '=') {
		found = envindex;
		break;
	    }
	    envindex++;
	}

	if (found < 0) {
	    unsigned need = ENV_HUNK(envindex + 1);
	    unsigned have = ENV_HUNK(envindex);

	    if (need > have) {
		char **newenv;
		newenv = TypeMallocN(char *, need);
		if (newenv == 0) {
		    fprintf(stderr, "Cannot increase environment\n");
		    return;
		}
		memmove(newenv, environ, have * sizeof(*newenv));
		free(environ);
		environ = newenv;
	    }

	    found = envindex;
	    environ[found + 1] = NULL;
	    environ = environ;
	}

	environ[found] = CastMallocN(char, 1 + len + strlen(value));
	if (environ[found] == 0) {
	    fprintf(stderr, "Cannot allocate environment %s\n", var);
	    return;
	}
	sprintf(environ[found], "%s=%s", var, value);
    }
}

/*ARGSUSED*/
int
xerror(Display * d, XErrorEvent * ev)
{
    fprintf(stderr, "%s:  warning, error event received:\n", ProgramName);
    (void) XmuPrintDefaultErrorMessage(d, ev, stderr);
    Exit(ERROR_XERROR);
    return 0;			/* appease the compiler */
}

/*ARGSUSED*/
int
xioerror(Display * dpy)
{
    int the_error = errno;

    (void) fprintf(stderr,
		   "%s:  fatal IO error %d (%s) or KillClient on X server \"%s\"\r\n",
		   ProgramName, the_error, SysErrorMsg(the_error),
		   DisplayString(dpy));

    Exit(ERROR_XIOERROR);
    return 0;			/* appease the compiler */
}

void
xt_error(String message)
{
    (void) fprintf(stderr, "%s Xt error: %s\n", ProgramName, message);

    /*
     * Check for the obvious - Xt does a poor job of reporting this.
     */
    if (x_getenv("DISPLAY") == 0) {
	fprintf(stderr, "%s:  DISPLAY is not set\n", ProgramName);
    }
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
set_vt_visibility(Bool on)
{
    XtermWidget xw = term;
    TScreen *screen = TScreenOf(xw);

    TRACE(("set_vt_visibility(%d)\n", on));
    if (on) {
	if (!screen->Vshow && xw) {
	    VTInit(xw);
	    XtMapWidget(XtParent(xw));
#if OPT_TOOLBAR
	    /* we need both of these during initialization */
	    XtMapWidget(SHELL_OF(xw));
	    ShowToolbar(resource.toolBar);
#endif
	    screen->Vshow = True;
	}
    }
#if OPT_TEK4014
    else {
	if (screen->Vshow && xw) {
	    withdraw_window(XtDisplay(xw),
			    VShellWindow(xw),
			    XScreenNumberOfScreen(XtScreen(xw)));
	    screen->Vshow = False;
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
set_tek_visibility(Bool on)
{
    TRACE(("set_tek_visibility(%d)\n", on));

    if (on) {
	if (!TEK4014_SHOWN(term)) {
	    if (tekWidget == 0) {
		TekInit();	/* will exit on failure */
	    }
	    if (tekWidget != 0) {
		Widget tekParent = SHELL_OF(tekWidget);
		XtRealizeWidget(tekParent);
		XtMapWidget(XtParent(tekWidget));
#if OPT_TOOLBAR
		/* we need both of these during initialization */
		XtMapWidget(tekParent);
		XtMapWidget(tekWidget);
#endif
		XtOverrideTranslations(tekParent,
				       XtParseTranslationTable
				       ("<Message>WM_PROTOCOLS: DeleteWindow()"));
		(void) XSetWMProtocols(XtDisplay(tekParent),
				       XtWindow(tekParent),
				       &wm_delete_window, 1);
		TEK4014_SHOWN(term) = True;
	    }
	}
    } else {
	if (TEK4014_SHOWN(term) && tekWidget) {
	    withdraw_window(XtDisplay(tekWidget),
			    TShellWindow,
			    XScreenNumberOfScreen(XtScreen(tekWidget)));
	    TEK4014_SHOWN(term) = False;
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
    XtermWidget xw = term;

    if (TEK4014_ACTIVE(xw)) {
	FlushLog(xw);
	longjmp(Tekend, 1);
    }
    return;
}

void
end_vt_mode(void)
{
    XtermWidget xw = term;

    if (!TEK4014_ACTIVE(xw)) {
	FlushLog(xw);
	TEK4014_ACTIVE(xw) = True;
	longjmp(VTend, 1);
    }
    return;
}

void
switch_modes(Bool tovt)		/* if true, then become vt mode */
{
    if (tovt) {
	if (tekRefreshList)
	    TekRefresh(tekWidget);
	end_tek_mode();		/* WARNING: this does a longjmp... */
    } else {
	end_vt_mode();		/* WARNING: this does a longjmp... */
    }
}

void
hide_vt_window(void)
{
    set_vt_visibility(False);
    if (!TEK4014_ACTIVE(term))
	switch_modes(False);	/* switch to tek mode */
}

void
hide_tek_window(void)
{
    set_tek_visibility(False);
    tekRefreshList = (TekLink *) 0;
    if (TEK4014_ACTIVE(term))
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
    const char *s1 = skip_punct(((const OptionHelp *) a)->opt);
    const char *s2 = skip_punct(((const OptionHelp *) b)->opt);
    return strcmp(s1, s2);
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

#ifdef NO_LEAKS
    if (descs == 0) {
	if (res_array != 0) {
	    free(res_array);
	    res_array = 0;
	}
    } else
#endif
    if (res_array == 0) {
	Cardinal j;

	/* make a sorted index to 'resources' */
	res_array = TypeCallocN(XrmOptionDescRec, res_count);
	if (res_array != 0) {
	    for (j = 0; j < res_count; j++)
		res_array[j] = descs[j];
	    qsort(res_array, (size_t) res_count, sizeof(*res_array), cmp_resources);
	}
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

#ifdef NO_LEAKS
    if (descs == 0 && opt_array != 0) {
	sortedOptDescs(descs, numDescs);
	free(opt_array);
	opt_array = 0;
	return 0;
    } else if (options == 0 || descs == 0) {
	return 0;
    }
#endif

    if (opt_array == 0) {
	size_t opt_count, j;
#if OPT_TRACE
	Cardinal k;
	XrmOptionDescRec *res_array = sortedOptDescs(descs, numDescs);
	int code;
	const char *mesg;
#else
	(void) descs;
	(void) numDescs;
#endif

	/* count 'options' and make a sorted index to it */
	for (opt_count = 0; options[opt_count].opt != 0; ++opt_count) {
	    ;
	}
	opt_array = TypeCallocN(OptionHelp, opt_count + 1);
	for (j = 0; j < opt_count; j++)
	    opt_array[j] = options[j];
	qsort(opt_array, opt_count, sizeof(OptionHelp), cmp_options);

	/* supply the "turn on/off" strings if needed */
#if OPT_TRACE
	for (j = 0; j < opt_count; j++) {
	    if (!strncmp(opt_array[j].opt, "-/+", 3)) {
		char temp[80];
		const char *name = opt_array[j].opt + 3;
		for (k = 0; k < numDescs; ++k) {
		    const char *value = res_array[k].value;
		    if (res_array[k].option[0] == '-') {
			code = -1;
		    } else if (res_array[k].option[0] == '+') {
			code = 1;
		    } else {
			code = 0;
		    }
		    strcpy(temp, opt_array[j].desc);
		    if (x_strindex(temp, "inhibit") != 0)
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
				char *s = CastMallocN(char,
						      strlen(mesg)
						      + 1
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

/*
 * Report the character-type locale that xterm was started in.
 */
char *
xtermEnvLocale(void)
{
    static char *result;

    if (result == 0) {
	if ((result = x_nonempty(setlocale(LC_CTYPE, 0))) == 0) {
	    result = x_strdup("C");
	} else {
	    result = x_strdup(result);
	}
	TRACE(("xtermEnvLocale ->%s\n", result));
    }
    return result;
}

char *
xtermEnvEncoding(void)
{
    static char *result;

    if (result == 0) {
#ifdef HAVE_LANGINFO_CODESET
	result = nl_langinfo(CODESET);
#else
	char *locale = xtermEnvLocale();
	if (!strcmp(locale, "C") || !strcmp(locale, "POSIX")) {
	    result = "ASCII";
	} else {
	    result = "ISO-8859-1";
	}
#endif
	TRACE(("xtermEnvEncoding ->%s\n", result));
    }
    return result;
}

#if OPT_WIDE_CHARS
/*
 * Tell whether xterm was started in a locale that uses UTF-8 encoding for
 * characters.  That environment is inherited by subprocesses and used in
 * various library calls.
 */
Bool
xtermEnvUTF8(void)
{
    static Bool init = False;
    static Bool result = False;

    if (!init) {
	init = True;
#ifdef HAVE_LANGINFO_CODESET
	result = (strcmp(xtermEnvEncoding(), "UTF-8") == 0);
#else
	result = (strstr(xtermEnvLocale(), "UTF-8") != NULL);
#endif
	TRACE(("xtermEnvUTF8 ->%s\n", BtoS(result)));
    }
    return result;
}
#endif /* OPT_WIDE_CHARS */

/*
 * Returns the version-string used in the "-v' message as well as a few other
 * places.  It is derived (when possible) from the __vendorversion__ symbol
 * that some newer imake configurations define.
 */
char *
xtermVersion(void)
{
    static char vendor_version[] = __vendorversion__;
    static char *result;

    if (result == 0) {
	char *vendor = vendor_version;
	char first[BUFSIZ];
	char second[BUFSIZ];

	result = CastMallocN(char, strlen(vendor) + 9);
	if (result == 0)
	    result = vendor;
	else {
	    /* some vendors leave trash in this string */
	    for (;;) {
		if (!strncmp(vendor, "Version ", (size_t) 8))
		    vendor += 8;
		else if (isspace(CharOf(*vendor)))
		    ++vendor;
		else
		    break;
	    }
	    if (strlen(vendor) < BUFSIZ &&
		sscanf(vendor, "%[0-9.] %[A-Za-z_0-9.]", first, second) == 2)
		sprintf(result, "%s %s(%d)", second, first, XTERM_PATCH);
	    else
		sprintf(result, "%s(%d)", vendor, XTERM_PATCH);
	}
    }
    return result;
}

/*
 * Check if the current widget, or any parent, is the VT100 "xterm" widget.
 */
XtermWidget
getXtermWidget(Widget w)
{
    XtermWidget xw;

    if (w == 0) {
	xw = (XtermWidget) CURRENT_EMU();
	if (!IsXtermWidget(xw)) {
	    xw = 0;
	}
    } else if (IsXtermWidget(w)) {
	xw = (XtermWidget) w;
    } else {
	xw = getXtermWidget(XtParent(w));
    }
    TRACE2(("getXtermWidget %p -> %p\n", w, xw));
    return xw;
}

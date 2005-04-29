/* $Xorg: button.c,v 1.3 2000/08/17 19:55:08 cpqbld Exp $ */
/*
 * Copyright 1999-2002,2003 by Thomas E. Dickey
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
/* $XFree86: xc/programs/xterm/button.c,v 3.74 2003/09/21 17:12:45 dickey Exp $ */

/*
button.c	Handles button events in the terminal emulator.
		does cut/paste operations, change modes via menu,
		passes button events through to some applications.
				J. Gettys.
*/

#include <xterm.h>

#include <stdio.h>

#include <X11/Xatom.h>
#include <X11/Xmu/Atoms.h>
#include <X11/Xmu/StdSel.h>

#include <xutf8.h>

#include <data.h>
#include <error.h>
#include <menu.h>
#include <xcharmouse.h>
#include <charclass.h>

#if OPT_WIDE_CHARS
#include <wcwidth.h>
#else
#define CharacterClass(value) \
	charClass[value & ((sizeof(charClass)/sizeof(charClass[0]))-1)]
#endif

#define XTERM_CELL(row,col) getXtermCell(screen, row + screen->topline, col)
#define XTERM_CELL_C1(row,col) getXtermCellComb1(screen, row + screen->topline, col)
#define XTERM_CELL_C2(row,col) getXtermCellComb2(screen, row + screen->topline, col)

      /*
       * We reserve shift modifier for cut/paste operations.  In principle we
       * can pass through control and meta modifiers, but in practice, the
       * popup menu uses control, and the window manager is likely to use meta,
       * so those events are not delivered to SendMousePosition.
       */
#define OurModifiers (ShiftMask | ControlMask | Mod1Mask)
#define AllModifiers (ShiftMask | LockMask | ControlMask | Mod1Mask | \
		      Mod2Mask | Mod3Mask | Mod4Mask | Mod5Mask)

#define KeyModifiers (event->xbutton.state & OurModifiers)

#define KeyState(x) (((x) & (ShiftMask|ControlMask)) + (((x) & Mod1Mask) ? 2 : 0))
    /* adds together the bits:
       shift key -> 1
       meta key  -> 2
       control key -> 4 */

#define	Coordinate(r,c)		((r) * (term->screen.max_col+1) + (c))

#if OPT_DEC_LOCATOR
static ANSI reply;
#endif

/* Selection/extension variables */

/* Raw char position where the selection started */
static int rawRow, rawCol;

/* Selected area before CHAR, WORD, LINE selectUnit processing */
static int startRRow, startRCol, endRRow, endRCol = 0;

/* Selected area after CHAR, WORD, LINE selectUnit processing */
static int startSRow, startSCol, endSRow, endSCol = 0;

/* Valid rows for selection clipping */
static int firstValidRow, lastValidRow;

/* Start, end of extension */
static int startERow, startECol, endERow, endECol;

/* Saved values of raw selection for extend to restore to */
static int saveStartRRow, saveStartRCol, saveEndRRow, saveEndRCol;

/* Saved value of WORD selection for LINE processing to restore to */
static int saveStartWRow, saveStartWCol;

/* Multi-click handling */
static int numberOfClicks = 0;
static Time lastButtonUpTime = 0;

#if OPT_READLINE
static Time lastButtonDownTime = 0;
static int ExtendingSelection = 0;
static Time lastButton3UpTime = 0;
static Time lastButton3DoubleDownTime = 0;
static int lastButton3row, lastButton3col;	/* At the release time */
#endif /* OPT_READLINE */

typedef int SelectUnit;

#define SELECTCHAR 0
#define SELECTWORD 1
#define SELECTLINE 2
#define NSELECTUNITS 3
static SelectUnit selectUnit;

/* Send emacs escape code when done selecting or extending? */
static int replyToEmacs;

static Char *SaveText(TScreen * screen, int row, int scol, int ecol, Char *
		      lp, int *eol);
static int Length(TScreen * screen, int row, int scol, int ecol);
static void ComputeSelect(int startRow, int startCol, int endRow, int
			  endCol, Bool extend);
static void EditorButton(XButtonEvent * event);
static void EndExtend(Widget w, XEvent * event, String * params, Cardinal
		      num_params, Bool use_cursor_loc);
static void ExtendExtend(int row, int col);
static void PointToRowCol(int y, int x, int *r, int *c);
static void ReHiliteText(int frow, int fcol, int trow, int tcol);
static void SaltTextAway(int crow, int ccol, int row, int col, String *
			 params, Cardinal num_params);
static void SelectSet(Widget w, XEvent * event, String * params, Cardinal num_params);
static void SelectionReceived PROTO_XT_SEL_CB_ARGS;
static void StartSelect(int startrow, int startcol);
static void TrackDown(XButtonEvent * event);
static void _OwnSelection(XtermWidget termw, String * selections, Cardinal count);
static void do_select_end(Widget w, XEvent * event, String * params,
			  Cardinal * num_params, Bool use_cursor_loc);

Boolean
SendMousePosition(Widget w, XEvent * event)
{
    TScreen *screen;

    if (!IsXtermWidget(w))
	return False;

    screen = &((XtermWidget) w)->screen;

    /* If send_mouse_pos mode isn't on, we shouldn't be here */
    if (screen->send_mouse_pos == MOUSE_OFF)
	return False;

#if OPT_DEC_LOCATOR
    if (screen->send_mouse_pos == DEC_LOCATOR) {
	return (SendLocatorPosition(w, event));
    }
#endif /* OPT_DEC_LOCATOR */

    /* Make sure the event is an appropriate type */
    if ((screen->send_mouse_pos != BTN_EVENT_MOUSE)
	&& (screen->send_mouse_pos != ANY_EVENT_MOUSE)
	&& event->type != ButtonPress
	&& event->type != ButtonRelease)
	return False;

    switch (screen->send_mouse_pos) {
    case X10_MOUSE:		/* X10 compatibility sequences */

	if (KeyModifiers == 0) {
	    if (event->type == ButtonPress)
		EditorButton((XButtonEvent *) event);
	    return True;
	}
	return False;

    case VT200_HIGHLIGHT_MOUSE:	/* DEC vt200 hilite tracking */
	if (event->type == ButtonPress &&
	    KeyModifiers == 0 &&
	    event->xbutton.button == Button1) {
	    TrackDown((XButtonEvent *) event);
	    return True;
	}
	if (KeyModifiers == 0 || KeyModifiers == ControlMask) {
	    EditorButton((XButtonEvent *) event);
	    return True;
	}
	return False;

    case VT200_MOUSE:		/* DEC vt200 compatible */

	/* xterm extension for motion reporting. June 1998 */
	/* EditorButton() will distinguish between the modes */
    case BTN_EVENT_MOUSE:
    case ANY_EVENT_MOUSE:
	if (KeyModifiers == 0 || KeyModifiers == ControlMask) {
	    EditorButton((XButtonEvent *) event);
	    return True;
	}
	return False;

    default:
	return False;
    }
}

#if OPT_DEC_LOCATOR

#define	LocatorCoords( row, col, x, y, oor )			\
    if( screen->locator_pixels ) {				\
	(oor)=FALSE; (row) = (y)+1; (col) = (x)+1;		\
	/* Limit to screen dimensions */			\
	if ((row) < 1) (row) = 1,(oor)=TRUE;			\
	else if ((row) > screen->border*2+Height(screen))	\
	    (row) = screen->border*2+Height(screen),(oor)=TRUE;	\
	if ((col) < 1) (col) = 1,(oor)=TRUE;			\
	else if ((col) > OriginX(screen)*2+Width(screen))	\
	    (col) = OriginX(screen)*2+Width(screen),(oor)=TRUE;	\
    } else {							\
	(oor)=FALSE;						\
	/* Compute character position of mouse pointer */	\
	(row) = ((y) - screen->border) / FontHeight(screen);	\
	(col) = ((x) - OriginX(screen)) / FontWidth(screen);	\
	/* Limit to screen dimensions */			\
	if ((row) < 0) (row) = 0,(oor)=TRUE;			\
	else if ((row) > screen->max_row)			\
	    (row) = screen->max_row,(oor)=TRUE;			\
	if ((col) < 0) (col) = 0,(oor)=TRUE;			\
	else if ((col) > screen->max_col)			\
	    (col) = screen->max_col,(oor)=TRUE;			\
	(row)++; (col)++;					\
    }

#define	MotionOff( s, t ) {						\
	    (s)->event_mask |= ButtonMotionMask;			\
	    (s)->event_mask &= ~PointerMotionMask;			\
	    XSelectInput(XtDisplay((t)), XtWindow((t)), (s)->event_mask); }

#define	MotionOn( s, t ) {						\
	    (s)->event_mask &= ~ButtonMotionMask;			\
	    (s)->event_mask |= PointerMotionMask;			\
	    XSelectInput(XtDisplay((t)), XtWindow((t)), (s)->event_mask); }

Boolean
SendLocatorPosition(Widget w, XEvent * event)
{
    TScreen *screen = &((XtermWidget) w)->screen;
    int row, col;
    Boolean oor;
    int button;
    int state;

    /* Make sure the event is an appropriate type */
    if ((event->type != ButtonPress &&
	 event->type != ButtonRelease &&
	 !screen->loc_filter) ||
	(KeyModifiers != 0 && KeyModifiers != ControlMask))
	return (False);

    if ((event->type == ButtonPress &&
	 !(screen->locator_events & LOC_BTNS_DN)) ||
	(event->type == ButtonRelease &&
	 !(screen->locator_events & LOC_BTNS_UP)))
	return (True);

    if (event->type == MotionNotify) {
	CheckLocatorPosition(w, event);
	return (True);
    }

    /* get button # */
    button = event->xbutton.button - 1;

    LocatorCoords(row, col, event->xbutton.x, event->xbutton.y, oor);

    /*
     * DECterm mouse:
     *
     * ESCAPE '[' event ; mask ; row ; column '&' 'w'
     */
    reply.a_type = CSI;

    if (oor) {
	reply.a_nparam = 1;
	reply.a_param[0] = 0;	/* Event - 0 = locator unavailable */
	reply.a_inters = '&';
	reply.a_final = 'w';
	unparseseq(&reply, screen->respond);

	if (screen->locator_reset) {
	    MotionOff(screen, term);
	    screen->send_mouse_pos = MOUSE_OFF;
	}
	return (True);
    }

    /*
     * event:
     *        1       no buttons
     *        2       left button down
     *        3       left button up
     *        4       middle button down
     *        5       middle button up
     *        6       right button down
     *        7       right button up
     *        8       M4 down
     *        9       M4 up
     */
    reply.a_nparam = 4;
    switch (event->type) {
    case ButtonPress:
	reply.a_param[0] = 2 + (button << 1);
	break;
    case ButtonRelease:
	reply.a_param[0] = 3 + (button << 1);
	break;
    default:
	return (True);
    }
    /*
     * mask:
     * bit 7   bit 6   bit 5   bit 4   bit 3   bit 2       bit 1         bit 0
     *                                 M4 down left down   middle down   right down
     *
     * Notice that Button1 (left) and Button3 (right) are swapped in the mask.
     * Also, mask should be the state after the button press/release,
     * X provides the state not including the button press/release.
     */
    state = (event->xbutton.state
	     & (Button1Mask | Button2Mask | Button3Mask | Button4Mask)) >> 8;
    state ^= 1 << button;	/* update mask to "after" state */
    state = (state & ~(4 | 1)) | ((state & 1) ? 4 : 0) | ((state & 4) ? 1 : 0);		/* swap Button1 & Button3 */

    reply.a_param[1] = state;
    reply.a_param[2] = row;
    reply.a_param[3] = col;
    reply.a_inters = '&';
    reply.a_final = 'w';

    unparseseq(&reply, screen->respond);

    if (screen->locator_reset) {
	MotionOff(screen, term);
	screen->send_mouse_pos = MOUSE_OFF;
    }

    /*
     * DECterm turns the Locator off if a button is pressed while a filter rectangle
     * is active. This might be a bug, but I don't know, so I'll emulate it anyways.
     */
    if (screen->loc_filter) {
	screen->send_mouse_pos = MOUSE_OFF;
	screen->loc_filter = FALSE;
	screen->locator_events = 0;
	MotionOff(screen, term);
    }

    return (True);
}

/*
 * mask:
 * bit 7   bit 6   bit 5   bit 4   bit 3   bit 2       bit 1         bit 0
 *                                 M4 down left down   middle down   right down
 *
 * Button1 (left) and Button3 (right) are swapped in the mask relative to X.
 */
#define	ButtonState(state, mask)	\
{ (state) = ((mask) & (Button1Mask | Button2Mask | Button3Mask | Button4Mask)) >> 8;	\
  /* swap Button1 & Button3 */								\
  (state) = ((state) & ~(4|1)) | (((state)&1)?4:0) | (((state)&4)?1:0);			\
}

void
GetLocatorPosition(XtermWidget w)
{
    TScreen *screen = &w->screen;
    Window root, child;
    int rx, ry, x, y;
    unsigned int mask;
    int row = 0, col = 0;
    Boolean oor = FALSE;
    Bool ret = FALSE;
    int state;

    /*
     * DECterm turns the Locator off if the position is requested while a filter rectangle
     * is active.  This might be a bug, but I don't know, so I'll emulate it anyways.
     */
    if (screen->loc_filter) {
	screen->send_mouse_pos = MOUSE_OFF;
	screen->loc_filter = FALSE;
	screen->locator_events = 0;
	MotionOff(screen, term);
    }

    reply.a_type = CSI;

    if (screen->send_mouse_pos == DEC_LOCATOR) {
	ret = XQueryPointer(screen->display, VWindow(screen), &root,
			    &child, &rx, &ry, &x, &y, &mask);
	if (ret) {
	    LocatorCoords(row, col, x, y, oor);
	}
    }
    if (ret == FALSE || oor) {
	reply.a_nparam = 1;
	reply.a_param[0] = 0;	/* Event - 0 = locator unavailable */
	reply.a_inters = '&';
	reply.a_final = 'w';
	unparseseq(&reply, screen->respond);

	if (screen->locator_reset) {
	    MotionOff(screen, term);
	    screen->send_mouse_pos = MOUSE_OFF;
	}
	return;
    }

    ButtonState(state, mask);

    reply.a_nparam = 4;
    reply.a_param[0] = 1;	/* Event - 1 = response to locator request */
    reply.a_param[1] = state;
    reply.a_param[2] = row;
    reply.a_param[3] = col;
    reply.a_inters = '&';
    reply.a_final = 'w';
    unparseseq(&reply, screen->respond);

    if (screen->locator_reset) {
	MotionOff(screen, term);
	screen->send_mouse_pos = MOUSE_OFF;
    }
}

void
InitLocatorFilter(XtermWidget w)
{
    TScreen *screen = &w->screen;
    Window root, child;
    int rx, ry, x, y;
    unsigned int mask;
    int row = 0, col = 0;
    Boolean oor = 0;
    Bool ret;
    int state;

    ret = XQueryPointer(screen->display, VWindow(screen),
			&root, &child, &rx, &ry, &x, &y, &mask);
    if (ret) {
	LocatorCoords(row, col, x, y, oor);
    }
    if (ret == FALSE || oor) {
	/* Locator is unavailable */

	if (screen->loc_filter_top != LOC_FILTER_POS ||
	    screen->loc_filter_left != LOC_FILTER_POS ||
	    screen->loc_filter_bottom != LOC_FILTER_POS ||
	    screen->loc_filter_right != LOC_FILTER_POS) {
	    /*
	     * If any explicit coordinates were received,
	     * report immediately with no coordinates.
	     */
	    reply.a_type = CSI;
	    reply.a_nparam = 1;
	    reply.a_param[0] = 0;	/* Event - 0 = locator unavailable */
	    reply.a_inters = '&';
	    reply.a_final = 'w';
	    unparseseq(&reply, screen->respond);

	    if (screen->locator_reset) {
		MotionOff(screen, term);
		screen->send_mouse_pos = MOUSE_OFF;
	    }
	} else {
	    /*
	     * No explicit coordinates were received, and the pointer is
	     * unavailable.  Report when the pointer re-enters the window.
	     */
	    screen->loc_filter = TRUE;
	    MotionOn(screen, term);
	}
	return;
    }

    /*
     * Adjust rectangle coordinates:
     *  1. Replace "LOC_FILTER_POS" with current coordinates
     *  2. Limit coordinates to screen size
     *  3. make sure top and left are less than bottom and right, resp.
     */
    if (screen->locator_pixels) {
	rx = OriginX(screen) * 2 + Width(screen);
	ry = screen->border * 2 + Height(screen);
    } else {
	rx = screen->max_col;
	ry = screen->max_row;
    }

#define	Adjust( coord, def, max )				\
	if( (coord) == LOC_FILTER_POS )	(coord) = (def);	\
	else if ((coord) < 1)		(coord) = 1;		\
	else if ((coord) > (max))	(coord) = (max)

    Adjust(screen->loc_filter_top, row, ry);
    Adjust(screen->loc_filter_left, col, rx);
    Adjust(screen->loc_filter_bottom, row, ry);
    Adjust(screen->loc_filter_right, col, rx);

    if (screen->loc_filter_top > screen->loc_filter_bottom) {
	ry = screen->loc_filter_top;
	screen->loc_filter_top = screen->loc_filter_bottom;
	screen->loc_filter_bottom = ry;
    }

    if (screen->loc_filter_left > screen->loc_filter_right) {
	rx = screen->loc_filter_left;
	screen->loc_filter_left = screen->loc_filter_right;
	screen->loc_filter_right = rx;
    }

    if ((col < screen->loc_filter_left) ||
	(col > screen->loc_filter_right) ||
	(row < screen->loc_filter_top) ||
	(row > screen->loc_filter_bottom)) {
	/* Pointer is already outside the rectangle - report immediately */
	ButtonState(state, mask);

	reply.a_type = CSI;
	reply.a_nparam = 4;
	reply.a_param[0] = 10;	/* Event - 10 = locator outside filter */
	reply.a_param[1] = state;
	reply.a_param[2] = row;
	reply.a_param[3] = col;
	reply.a_inters = '&';
	reply.a_final = 'w';
	unparseseq(&reply, screen->respond);

	if (screen->locator_reset) {
	    MotionOff(screen, term);
	    screen->send_mouse_pos = MOUSE_OFF;
	}
	return;
    }

    /*
     * Rectangle is set up.  Allow pointer tracking
     * to detect if the mouse leaves the rectangle.
     */
    screen->loc_filter = TRUE;
    MotionOn(screen, term);
}

void
CheckLocatorPosition(Widget w, XEvent * event)
{
    TScreen *screen = &((XtermWidget) w)->screen;
    int row, col;
    Boolean oor;
    int state;

    LocatorCoords(row, col, event->xbutton.x, event->xbutton.y, oor);

    /*
     * Send report if the pointer left the filter rectangle, if
     * the pointer left the window, or if the filter rectangle
     * had no coordinates and the pointer re-entered the window.
     */
    if (oor || (screen->loc_filter_top == LOC_FILTER_POS) ||
	(col < screen->loc_filter_left) ||
	(col > screen->loc_filter_right) ||
	(row < screen->loc_filter_top) ||
	(row > screen->loc_filter_bottom)) {
	/* Filter triggered - disable it */
	screen->loc_filter = FALSE;
	MotionOff(screen, term);

	reply.a_type = CSI;
	if (oor) {
	    reply.a_nparam = 1;
	    reply.a_param[0] = 0;	/* Event - 0 = locator unavailable */
	} else {
	    ButtonState(state, event->xbutton.state);

	    reply.a_nparam = 4;
	    reply.a_param[0] = 10;	/* Event - 10 = locator outside filter */
	    reply.a_param[1] = state;
	    reply.a_param[2] = row;
	    reply.a_param[3] = col;
	}

	reply.a_inters = '&';
	reply.a_final = 'w';
	unparseseq(&reply, screen->respond);

	if (screen->locator_reset) {
	    MotionOff(screen, term);
	    screen->send_mouse_pos = MOUSE_OFF;
	}
    }
}
#endif /* OPT_DEC_LOCATOR */

#if OPT_READLINE
static int
isClick1_clean(XEvent * event)
{
    TScreen *screen = &term->screen;
    int delta;

    if (!(event->type == ButtonPress || event->type == ButtonRelease)
    /* Disable on Shift-Click-1, including the application-mouse modes */
	|| (KeyModifiers & ShiftMask)
	|| (screen->send_mouse_pos != MOUSE_OFF)	/* Kinda duplicate... */
	||ExtendingSelection)	/* Was moved */
	return 0;
    if (event->type != ButtonRelease)
	return 0;
    if (lastButtonDownTime == (Time) 0)		/* first time or once in a blue moon */
	delta = term->screen.multiClickTime + 1;
    else if (event->xbutton.time > lastButtonDownTime)	/* most of the time */
	delta = event->xbutton.time - lastButtonDownTime;
    else			/* time has rolled over since lastButtonUpTime */
	delta = (((Time) ~ 0) - lastButtonDownTime) + event->xbutton.time;
    return delta <= term->screen.multiClickTime;
}

static int
isDoubleClick3(XEvent * event)
{
    int delta;

    if (event->type != ButtonRelease
	|| (KeyModifiers & ShiftMask)
	|| event->xbutton.button != Button3) {
	lastButton3UpTime = 0;	/* Disable the cached info */
	return 0;
    }
    /* Process Btn3Release. */
    if (lastButton3DoubleDownTime == (Time) 0)	/* No previous click
						   or once in a blue moon */
	delta = term->screen.multiClickTime + 1;
    else if (event->xbutton.time > lastButton3DoubleDownTime)	/* most of the time */
	delta = event->xbutton.time - lastButton3DoubleDownTime;
    else			/* time has rolled over since lastButton3DoubleDownTime */
	delta = (((Time) ~ 0) - lastButton3DoubleDownTime) + event->xbutton.time;
    if (delta <= term->screen.multiClickTime) {
	/* Double click */
	int row, col;

	/* Cannot check ExtendingSelection, since mouse-3 always sets it */
	PointToRowCol(event->xbutton.y, event->xbutton.x, &row, &col);
	if (row == lastButton3row && col == lastButton3col) {
	    lastButton3DoubleDownTime = 0;	/* Disable the third click */
	    return 1;
	}
    }
    /* Not a double click, memorize for future check. */
    lastButton3UpTime = event->xbutton.time;
    PointToRowCol(event->xbutton.y, event->xbutton.x,
		  &lastButton3row, &lastButton3col);
    return 0;
}

static int
CheckSecondPress3(XEvent * event)
{
    int delta, row, col;

    if (event->type != ButtonPress
	|| (KeyModifiers & ShiftMask)
	|| event->xbutton.button != Button3) {
	lastButton3DoubleDownTime = 0;	/* Disable the cached info */
	return 0;
    }
    /* Process Btn3Press. */
    if (lastButton3UpTime == (Time) 0)	/* No previous click
					   or once in a blue moon */
	delta = term->screen.multiClickTime + 1;
    else if (event->xbutton.time > lastButton3UpTime)	/* most of the time */
	delta = event->xbutton.time - lastButton3UpTime;
    else			/* time has rolled over since lastButton3UpTime */
	delta = (((Time) ~ 0) - lastButton3UpTime) + event->xbutton.time;
    if (delta <= term->screen.multiClickTime) {
	PointToRowCol(event->xbutton.y, event->xbutton.x, &row, &col);
	if (row == lastButton3row && col == lastButton3col) {
	    /* A candidate for a double-click */
	    lastButton3DoubleDownTime = event->xbutton.time;
	    PointToRowCol(event->xbutton.y, event->xbutton.x,
			  &lastButton3row, &lastButton3col);
	    return 1;
	}
	lastButton3UpTime = 0;	/* Disable the info about the previous click */
    }
    /* Either too long, or moved, disable. */
    lastButton3DoubleDownTime = 0;
    return 0;
}

static int
rowOnCurrentLine(int line, int *deltap)		/* must be XButtonEvent */
{
    TScreen *screen = &term->screen;
    int l1, l2;

    *deltap = 0;
    if (line == screen->cur_row)
	return 1;

    if (line < screen->cur_row)
	l1 = line, l2 = screen->cur_row;
    else
	l2 = line, l1 = screen->cur_row;
    l1--;
    while (++l1 < l2)
	if (!ScrnTstWrapped(screen, l1))
	    return 0;
    /* Everything is on one "wrapped line" now */
    *deltap = line - screen->cur_row;
    return 1;
}

static int
eventRow(XEvent * event)	/* must be XButtonEvent */
{
    TScreen *screen = &term->screen;

    return (event->xbutton.y - screen->border) / FontHeight(screen);
}

static int
eventColBetween(XEvent * event)	/* must be XButtonEvent */
{
    TScreen *screen = &term->screen;

    /* Correct by half a width - we are acting on a boundary, not on a cell. */
    return ((event->xbutton.x - OriginX(screen) + (FontWidth(screen) - 1) / 2)
	    / FontWidth(screen));
}

static int
ReadLineMovePoint(int col, int ldelta)
{
    TScreen *screen = &term->screen;
    Char line[6];
    int count = 0;

    col += ldelta * (screen->max_col + 1) - screen->cur_col;
    if (col == 0)
	return 0;
    if (screen->control_eight_bits) {
	line[count++] = CSI;
    } else {
	line[count++] = ESC;
	line[count++] = '[';	/* XXX maybe sometimes O is better? */
    }
    line[count++] = (col > 0 ? 'C' : 'D');
    if (col < 0)
	col = -col;
    while (col--)
	v_write(screen->respond, line, 3);
    return 1;
}

static int
ReadLineDelete(int r1, int c1, int r2, int c2)
{
    TScreen *screen = &term->screen;
    int del;

    del = c2 - c1 + (r2 - r1) * (screen->max_col + 1);
    if (del <= 0)		/* Just in case... */
	return 0;
    while (del--)
	v_write(screen->respond, "\177", 1);	/* XXX Sometimes "\08"? */
    return 1;
}
#endif /* OPT_READLINE */

/* ^XM-G<line+' '><col+' '> */
void
DiredButton(Widget w GCC_UNUSED,
	    XEvent * event,	/* must be XButtonEvent */
	    String * params GCC_UNUSED,		/* selections */
	    Cardinal * num_params GCC_UNUSED)
{
    TScreen *screen = &term->screen;
    Char Line[6];
    unsigned line, col;

    if (event->type == ButtonPress || event->type == ButtonRelease) {
	line = (event->xbutton.y - screen->border) / FontHeight(screen);
	col = (event->xbutton.x - OriginX(screen)) / FontWidth(screen);
	Line[0] = CONTROL('X');
	Line[1] = ESC;
	Line[2] = 'G';
	Line[3] = ' ' + col;
	Line[4] = ' ' + line;
	v_write(screen->respond, Line, 5);
    }
}

#if OPT_READLINE
void
ReadLineButton(Widget w GCC_UNUSED,
	       XEvent * event,	/* must be XButtonEvent */
	       String * params GCC_UNUSED,	/* selections */
	       Cardinal * num_params GCC_UNUSED)
{
    TScreen *screen = &term->screen;
    Char Line[6];
    int line, col, ldelta = 0;

    if (!(event->type == ButtonPress || event->type == ButtonRelease)
	|| (screen->send_mouse_pos != MOUSE_OFF) || ExtendingSelection)
	goto finish;
    if (event->type == ButtonRelease) {
	int delta;

	if (lastButtonDownTime == (Time) 0)	/* first time and once in a blue moon */
	    delta = screen->multiClickTime + 1;
	else if (event->xbutton.time > lastButtonDownTime)	/* most of the time */
	    delta = event->xbutton.time - lastButtonDownTime;
	else			/* time has rolled over since lastButtonUpTime */
	    delta = (((Time) ~ 0) - lastButtonDownTime) + event->xbutton.time;
	if (delta > screen->multiClickTime)
	    goto finish;	/* All this work for this... */
    }
    line = (event->xbutton.y - screen->border) / FontHeight(screen);
    if (line != screen->cur_row) {
	int l1, l2;

	if (line < screen->cur_row)
	    l1 = line, l2 = screen->cur_row;
	else
	    l2 = line, l1 = screen->cur_row;
	l1--;
	while (++l1 < l2)
	    if (!ScrnTstWrapped(screen, l1))
		goto finish;
	/* Everything is on one "wrapped line" now */
	ldelta = line - screen->cur_row;
    }
    /* Correct by half a width - we are acting on a boundary, not on a cell. */
    col = (event->xbutton.x - OriginX(screen) + (FontWidth(screen) - 1) / 2)
	/ FontWidth(screen) - screen->cur_col + ldelta * (screen->max_col + 1);
    if (col == 0)
	goto finish;
    Line[0] = ESC;
    /* XXX: sometimes it is better to send '['? */
    Line[1] = 'O';
    Line[2] = (col > 0 ? 'C' : 'D');
    if (col < 0)
	col = -col;
    while (col--)
	v_write(screen->respond, Line, 3);
  finish:
    if (event->type == ButtonRelease)
	do_select_end(w, event, params, num_params, False);
}
#endif /* OPT_READLINE */

/* repeats <ESC>n or <ESC>p */
void
ViButton(Widget w GCC_UNUSED,
	 XEvent * event,	/* must be XButtonEvent */
	 String * params GCC_UNUSED,	/* selections */
	 Cardinal * num_params GCC_UNUSED)
{
    TScreen *screen = &term->screen;
    int pty = screen->respond;
    Char Line[6];
    int line;

    if (event->type == ButtonPress || event->type == ButtonRelease) {

	line = screen->cur_row -
	    ((event->xbutton.y - screen->border) / FontHeight(screen));
	if (line != 0) {
	    Line[0] = ESC;	/* force an exit from insert-mode */
	    v_write(pty, Line, 1);

	    if (line < 0) {
		line = -line;
		Line[0] = CONTROL('n');
	    } else {
		Line[0] = CONTROL('p');
	    }
	    while (--line >= 0)
		v_write(pty, Line, 1);
	}
    }
}

/*
 * This function handles button-motion events
 */
/*ARGSUSED*/
void
HandleSelectExtend(Widget w,
		   XEvent * event,	/* must be XMotionEvent */
		   String * params GCC_UNUSED,
		   Cardinal * num_params GCC_UNUSED)
{
    TScreen *screen;
    int row, col;

    if (!IsXtermWidget(w))
	return;

    screen = &((XtermWidget) w)->screen;
    screen->selection_time = event->xmotion.time;
    switch (eventMode) {
	/* If not in one of the DEC mouse-reporting modes */
    case LEFTEXTENSION:
    case RIGHTEXTENSION:
	PointToRowCol(event->xmotion.y, event->xmotion.x,
		      &row, &col);
	ExtendExtend(row, col);
	break;

	/* If in motion reporting mode, send mouse position to
	   character process as a key sequence \E[M... */
    case NORMAL:
	/* will get here if send_mouse_pos != MOUSE_OFF */
	if (screen->send_mouse_pos == BTN_EVENT_MOUSE
	    || screen->send_mouse_pos == ANY_EVENT_MOUSE)
	    SendMousePosition(w, event);
	break;
    }
}

static void
do_select_end(Widget w,
	      XEvent * event,	/* must be XButtonEvent */
	      String * params,	/* selections */
	      Cardinal * num_params,
	      Bool use_cursor_loc)
{
#if OPT_READLINE
    int ldelta1, ldelta2;
    TScreen *screen = &term->screen;
#endif

    if (!IsXtermWidget(w))
	return;

    ((XtermWidget) w)->screen.selection_time = event->xbutton.time;
    switch (eventMode) {
    case NORMAL:
	(void) SendMousePosition(w, event);
	break;
    case LEFTEXTENSION:
    case RIGHTEXTENSION:
	EndExtend(w, event, params, *num_params, use_cursor_loc);
#if OPT_READLINE
	if (isClick1_clean(event)
	    && SCREEN_FLAG(screen, click1_moves)
	    && rowOnCurrentLine(eventRow(event), &ldelta1)) {
	    ReadLineMovePoint(eventColBetween(event), ldelta1);
	}
	if (isDoubleClick3(event)
	    && SCREEN_FLAG(screen, dclick3_deletes)
	    && rowOnCurrentLine(startSRow, &ldelta1)
	    && rowOnCurrentLine(endSRow, &ldelta2)) {
	    ReadLineMovePoint(endSCol, ldelta2);
	    ReadLineDelete(startSRow, startSCol, endSRow, endSCol);
	}
#endif /* OPT_READLINE */
	break;
    }
}

void
HandleSelectEnd(Widget w,
		XEvent * event,	/* must be XButtonEvent */
		String * params,	/* selections */
		Cardinal * num_params)
{
    do_select_end(w, event, params, num_params, False);
}

void
HandleKeyboardSelectEnd(Widget w,
			XEvent * event,		/* must be XButtonEvent */
			String * params,	/* selections */
			Cardinal * num_params)
{
    do_select_end(w, event, params, num_params, True);
}

struct _SelectionList {
    String *params;
    Cardinal count;
    Atom *targets;
    Time time;
};

/* convert a UTF-8 string to Latin-1, replacing non Latin-1 characters
 * by `#'. */

#if OPT_WIDE_CHARS
static Char *
UTF8toLatin1(Char * s, int len, unsigned long *result)
{
    static Char *buffer;
    static size_t used;

    Char *p = s;
    Char *q;

    if (used == 0) {
	buffer = (Char *) XtMalloc(used = len);
    } else if (len > (int) used) {
	buffer = (Char *) XtRealloc((char *) buffer, used = len);
    }
    q = buffer;

    /* We're assuming that the xterm widget never contains Unicode
       control characters. */

    while (p < s + len) {
	if ((*p & 0x80) == 0) {
	    *q++ = *p++;
	} else if ((*p & 0x7C) == 0x40 && p < s + len - 1) {
	    *q++ = ((*p & 0x03) << 6) | (p[1] & 0x3F);
	    p += 2;
	} else if ((*p & 0x60) == 0x40) {
	    *q++ = '#';
	    p += 2;
	} else if ((*p & 0x50) == 0x40) {
	    *q++ = '#';
	    p += 3;
	} else {		/* this cannot happen */
	    *q++ = '#';
	    p++;
	}
    }
    *result = q - buffer;
    return buffer;
}
#endif /* OPT_WIDE_CHARS */

static Atom *
_SelectionTargets(Widget w)
{
    static Atom *eightBitSelectionTargets = NULL;
    TScreen *screen;
    int n;

    if (!IsXtermWidget(w))
	return NULL;

    screen = &((XtermWidget) w)->screen;

#if OPT_WIDE_CHARS
    if (screen->wide_chars) {
	static Atom *utf8SelectionTargets = NULL;

	if (utf8SelectionTargets == NULL) {
	    utf8SelectionTargets = (Atom *) XtMalloc(5 * sizeof(Atom));
	    if (utf8SelectionTargets == NULL) {
		TRACE(("Couldn't allocate utf8SelectionTargets\n"));
		return NULL;
	    }
	    n = 0;
	    utf8SelectionTargets[n++] = XA_UTF8_STRING(XtDisplay(w));
#ifdef X_HAVE_UTF8_STRING
	    if (screen->i18nSelections) {
		utf8SelectionTargets[n++] = XA_TEXT(XtDisplay(w));
		utf8SelectionTargets[n++] = XA_COMPOUND_TEXT(XtDisplay(w));
	    }
#endif
	    utf8SelectionTargets[n++] = XA_STRING;
	    utf8SelectionTargets[n] = None;
	}
	return utf8SelectionTargets;
    }
#endif

    /* not screen->wide_chars */
    if (eightBitSelectionTargets == NULL) {
	eightBitSelectionTargets = (Atom *) XtMalloc(5 * sizeof(Atom));
	if (eightBitSelectionTargets == NULL) {
	    TRACE(("Couldn't allocate eightBitSelectionTargets\n"));
	    return NULL;
	}
	n = 0;
#ifdef X_HAVE_UTF8_STRING
	eightBitSelectionTargets[n++] = XA_UTF8_STRING(XtDisplay(w));
#endif
	if (screen->i18nSelections) {
	    eightBitSelectionTargets[n++] = XA_TEXT(XtDisplay(w));
	    eightBitSelectionTargets[n++] = XA_COMPOUND_TEXT(XtDisplay(w));
	}
	eightBitSelectionTargets[n++] = XA_STRING;
	eightBitSelectionTargets[n] = None;
    }
    return eightBitSelectionTargets;
}

static void
_GetSelection(Widget w,
	      Time ev_time,
	      String * params,	/* selections in precedence order */
	      Cardinal num_params,
	      Atom * targets)
{
    Atom selection;
    int cutbuffer;
    Atom target;

    if (!IsXtermWidget(w))
	return;

    XmuInternStrings(XtDisplay(w), params, (Cardinal) 1, &selection);
    switch (selection) {
    case XA_CUT_BUFFER0:
	cutbuffer = 0;
	break;
    case XA_CUT_BUFFER1:
	cutbuffer = 1;
	break;
    case XA_CUT_BUFFER2:
	cutbuffer = 2;
	break;
    case XA_CUT_BUFFER3:
	cutbuffer = 3;
	break;
    case XA_CUT_BUFFER4:
	cutbuffer = 4;
	break;
    case XA_CUT_BUFFER5:
	cutbuffer = 5;
	break;
    case XA_CUT_BUFFER6:
	cutbuffer = 6;
	break;
    case XA_CUT_BUFFER7:
	cutbuffer = 7;
	break;
    default:
	cutbuffer = -1;
    }
    TRACE(("Cutbuffer: %d, target: %lu\n", cutbuffer,
	   targets ? (unsigned long) targets[0] : 0));
    if (cutbuffer >= 0) {
	int inbytes;
	unsigned long nbytes;
	int fmt8 = 8;
	Atom type = XA_STRING;
	char *line = XFetchBuffer(XtDisplay(w), &inbytes, cutbuffer);
	nbytes = (unsigned long) inbytes;
	if (nbytes > 0)
	    SelectionReceived(w, NULL, &selection, &type, (XtPointer) line,
			      &nbytes, &fmt8);
	else if (num_params > 1)
	    _GetSelection(w, ev_time, params + 1, num_params - 1, NULL);
	return;
    } else {
	struct _SelectionList *list;

	if (targets == NULL || targets[0] == None) {
	    targets = _SelectionTargets(w);
	}

	if (targets != 0) {
	    target = targets[0];

	    if (targets[1] == None) {	/* last target in list */
		params++;
		num_params--;
		targets = _SelectionTargets(w);
	    } else {
		targets = &(targets[1]);
	    }

	    if (num_params) {
		list = XtNew(struct _SelectionList);
		list->params = params;
		list->count = num_params;
		list->targets = targets;
		list->time = ev_time;
	    } else
		list = NULL;

	    XtGetSelectionValue(w, selection,
				target,
				SelectionReceived,
				(XtPointer) list, ev_time);
	}
    }
}

#if OPT_TRACE && OPT_WIDE_CHARS
static void
GettingSelection(Display * dpy, Atom type, Char * line, int len)
{
    Char *cp;
    char *name;

    name = XGetAtomName(dpy, type);

    Trace("Getting %s (%ld)\n", XGetAtomName(dpy, type), (long int) type);
    for (cp = line; cp < line + len; cp++)
	Trace("%c\n", *cp);
}
#else
#define GettingSelection(dpy,type,line,len)	/* nothing */
#endif

#ifdef VMS
#  define tty_vwrite(pty,lag,l)		tt_write(lag,l)
#else /* !( VMS ) */
#  define tty_vwrite(pty,lag,l)		v_write(pty,lag,l)
#endif /* defined VMS */

static void
_qWriteSelectionData(TScreen * screen, Char * lag, int length)
{
#if OPT_READLINE
    if (SCREEN_FLAG(screen, paste_quotes)) {
	while (length--) {
	    tty_vwrite(screen->respond, "\026", 1);	/* Control-V */
	    tty_vwrite(screen->respond, lag++, 1);
	}
    } else
#endif
	tty_vwrite(screen->respond, lag, length);
}

static void
_WriteSelectionData(TScreen * screen, Char * line, int length)
{
    /* Write data to pty a line at a time. */
    /* Doing this one line at a time may no longer be necessary
       because v_write has been re-written. */

    Char *lag, *cp, *end;

    /* in the VMS version, if tt_pasting isn't set to TRUE then qio
       reads aren't blocked and an infinite loop is entered, where the
       pasted text shows up as new input, goes in again, shows up
       again, ad nauseum. */
#ifdef VMS
    tt_pasting = TRUE;
#endif

    end = &line[length];
    lag = line;
    if (!SCREEN_FLAG(screen, paste_literal_nl)) {
	for (cp = line; cp != end; cp++) {
	    if (*cp == '\n') {
		*cp = '\r';
		_qWriteSelectionData(screen, lag, cp - lag + 1);
		lag = cp + 1;
	    }
	}
    }
    if (lag != end) {
	_qWriteSelectionData(screen, lag, end - lag);
    }
#ifdef VMS
    tt_pasting = FALSE;
    tt_start_read();		/* reenable reads or a character may be lost */
#endif
}

#if OPT_READLINE
static void
_WriteKey(TScreen * screen, Char * in)
{
    char line[16];
    int count = 0, length = strlen(in);

    if (screen->control_eight_bits) {
	line[count++] = CSI;
    } else {
	line[count++] = ESC;
	line[count++] = '[';
    }
    while (length--)
	line[count++] = *in++;
    line[count++] = '~';
    tty_vwrite(screen->respond, line, count);
}
#endif /* OPT_READLINE */

/* SelectionReceived: stuff received selection text into pty */

/* ARGSUSED */
static void
SelectionReceived(Widget w,
		  XtPointer client_data,
		  Atom * selection GCC_UNUSED,
		  Atom * type,
		  XtPointer value,
		  unsigned long *length,
		  int *format GCC_UNUSED)
{
    char **text_list = NULL;
    int text_list_count;
    XTextProperty text_prop;
    TScreen *screen;
    Display *dpy;
#if OPT_TRACE && OPT_WIDE_CHARS
    Char *line = (Char *) value;
#endif

    if (!IsXtermWidget(w))
	return;
    screen = &((XtermWidget) w)->screen;
    dpy = XtDisplay(w);

    if (*type == 0		/*XT_CONVERT_FAIL */
	|| *length == 0
	|| value == NULL)
	goto fail;

    text_prop.value = (unsigned char *) value;
    text_prop.encoding = *type;
    text_prop.format = *format;
    text_prop.nitems = *length;

#if OPT_WIDE_CHARS
    if (screen->wide_chars) {
	if (*type == XA_UTF8_STRING(XtDisplay(w)) ||
	    *type == XA_STRING ||
	    *type == XA_COMPOUND_TEXT(XtDisplay(w))) {
	    GettingSelection(dpy, *type, line, *length);
	    if (Xutf8TextPropertyToTextList(dpy, &text_prop,
					    &text_list,
					    &text_list_count) < 0) {
		TRACE(("Conversion failed\n"));
		text_list = NULL;
	    }
	}
    } else
#endif /* OPT_WIDE_CHARS */
    {
	/* Convert the selection to locale's multibyte encoding. */

	/* There's no need to special-case UTF8_STRING.  If Xlib
	   doesn't know about it, we didn't request it.  If a broken
	   selection holder sends it anyhow, the conversion function
	   will fail. */

	if (*type == XA_UTF8_STRING(XtDisplay(w)) ||
	    *type == XA_STRING ||
	    *type == XA_COMPOUND_TEXT(XtDisplay(w))) {
	    Status rc;
	    GettingSelection(dpy, *type, line, *length);
	    if (*type == XA_STRING && screen->brokenSelections) {
		rc = XTextPropertyToStringList(&text_prop,
					       &text_list, &text_list_count);
	    } else {
		rc = XmbTextPropertyToTextList(dpy, &text_prop,
					       &text_list,
					       &text_list_count);
	    }
	    if (rc < 0) {
		TRACE(("Conversion failed\n"));
		text_list = NULL;
	    }
	}
    }

    if (text_list != NULL && text_list_count != 0) {
	int i;

#if OPT_READLINE
	if (SCREEN_FLAG(screen, paste_brackets))
	    _WriteKey(screen, "200");
#endif
	for (i = 0; i < text_list_count; i++) {
	    int len = strlen(text_list[i]);
	    _WriteSelectionData(screen, (Char *) text_list[i], len);
	}
#if OPT_READLINE
	if (SCREEN_FLAG(screen, paste_brackets))
	    _WriteKey(screen, "201");
#endif
	XFreeStringList(text_list);
    } else
	goto fail;

    XtFree((char *) client_data);
    XtFree((char *) value);

    return;

  fail:
    if (client_data != 0) {
	struct _SelectionList *list = (struct _SelectionList *) client_data;
	_GetSelection(w, list->time,
		      list->params, list->count, list->targets);
	XtFree((char *) client_data);
    }
    return;
}

void
HandleInsertSelection(Widget w,
		      XEvent * event,	/* assumed to be XButtonEvent* */
		      String * params,	/* selections in precedence order */
		      Cardinal * num_params)
{
#if OPT_READLINE
    int ldelta;
    TScreen *screen = &((XtermWidget) w)->screen;
#endif

    if (SendMousePosition(w, event))
	return;

#if OPT_READLINE
    if ((event->type == ButtonPress || event->type == ButtonRelease)
    /* Disable on Shift-mouse, including the application-mouse modes */
	&& !(KeyModifiers & ShiftMask)
	&& (screen->send_mouse_pos == MOUSE_OFF)
	&& SCREEN_FLAG(screen, paste_moves)
	&& rowOnCurrentLine(eventRow(event), &ldelta))
	ReadLineMovePoint(eventColBetween(event), ldelta);
#endif /* OPT_READLINE */

    _GetSelection(w, event->xbutton.time, params, *num_params, NULL);
}

static SelectUnit
EvalSelectUnit(Time buttonDownTime, SelectUnit defaultUnit)
{
    int delta;

    if (lastButtonUpTime == (Time) 0)	/* first time and once in a blue moon */
	delta = term->screen.multiClickTime + 1;
    else if (buttonDownTime > lastButtonUpTime)		/* most of the time */
	delta = buttonDownTime - lastButtonUpTime;
    else			/* time has rolled over since lastButtonUpTime */
	delta = (((Time) ~ 0) - lastButtonUpTime) + buttonDownTime;

    if (delta > term->screen.multiClickTime) {
	numberOfClicks = 1;
	return defaultUnit;
    } else {
	++numberOfClicks;
	return ((selectUnit + 1) % NSELECTUNITS);
    }
}

static void
do_select_start(Widget w,
		XEvent * event,	/* must be XButtonEvent* */
		int startrow,
		int startcol)
{
    if (SendMousePosition(w, event))
	return;
    selectUnit = EvalSelectUnit(event->xbutton.time, SELECTCHAR);
    replyToEmacs = FALSE;

#if OPT_READLINE
    lastButtonDownTime = event->xbutton.time;
#endif

    StartSelect(startrow, startcol);
}

/* ARGSUSED */
void
HandleSelectStart(Widget w,
		  XEvent * event,	/* must be XButtonEvent* */
		  String * params GCC_UNUSED,
		  Cardinal * num_params GCC_UNUSED)
{
    TScreen *screen;
    int startrow, startcol;

    if (!IsXtermWidget(w))
	return;

    screen = &((XtermWidget) w)->screen;
    firstValidRow = 0;
    lastValidRow = screen->max_row;
    PointToRowCol(event->xbutton.y, event->xbutton.x, &startrow, &startcol);

#if OPT_READLINE
    ExtendingSelection = 0;
#endif

    do_select_start(w, event, startrow, startcol);
}

/* ARGSUSED */
void
HandleKeyboardSelectStart(Widget w,
			  XEvent * event,	/* must be XButtonEvent* */
			  String * params GCC_UNUSED,
			  Cardinal * num_params GCC_UNUSED)
{
    TScreen *screen;

    if (!IsXtermWidget(w))
	return;

    screen = &((XtermWidget) w)->screen;
    do_select_start(w, event, screen->cursor_row, screen->cursor_col);
}

static void
TrackDown(XButtonEvent * event)
{
    int startrow, startcol;

    selectUnit = EvalSelectUnit(event->time, SELECTCHAR);
    if (numberOfClicks > 1) {
	PointToRowCol(event->y, event->x, &startrow, &startcol);
	replyToEmacs = TRUE;
	StartSelect(startrow, startcol);
    } else {
	waitingForTrackInfo = 1;
	EditorButton((XButtonEvent *) event);
    }
}

#define boundsCheck(x)	if (x < 0) \
			    x = 0; \
			else if (x >= screen->max_row) \
			    x = screen->max_row;

void
TrackMouse(int func, int startrow, int startcol, int firstrow, int lastrow)
{
    TScreen *screen = &term->screen;

    if (!waitingForTrackInfo) {	/* Timed out, so ignore */
	return;
    }
    waitingForTrackInfo = 0;
    if (func == 0)
	return;
    boundsCheck(startrow)
	boundsCheck(firstrow)
	boundsCheck(lastrow)
	firstValidRow = firstrow;
    lastValidRow = lastrow;
    replyToEmacs = TRUE;
    StartSelect(startrow, startcol);
}

static void
StartSelect(int startrow, int startcol)
{
    TScreen *screen = &term->screen;

    TRACE(("StartSelect row=%d, col=%d\n", startrow, startcol));
    if (screen->cursor_state)
	HideCursor();
    if (numberOfClicks == 1) {
	/* set start of selection */
	rawRow = startrow;
	rawCol = startcol;

    }
    /* else use old values in rawRow, Col */
    saveStartRRow = startERow = rawRow;
    saveStartRCol = startECol = rawCol;
    saveEndRRow = endERow = rawRow;
    saveEndRCol = endECol = rawCol;
    if (Coordinate(startrow, startcol) < Coordinate(rawRow, rawCol)) {
	eventMode = LEFTEXTENSION;
	startERow = startrow;
	startECol = startcol;
    } else {
	eventMode = RIGHTEXTENSION;
	endERow = startrow;
	endECol = startcol;
    }
    ComputeSelect(startERow, startECol, endERow, endECol, False);

}

static void
EndExtend(Widget w,
	  XEvent * event,	/* must be XButtonEvent */
	  String * params,	/* selections */
	  Cardinal num_params,
	  Bool use_cursor_loc)
{
    int row, col, count;
    TScreen *screen = &term->screen;
    Char line[9];

    if (use_cursor_loc) {
	row = screen->cursor_row;
	col = screen->cursor_col;
    } else {
	PointToRowCol(event->xbutton.y, event->xbutton.x, &row, &col);
    }
    ExtendExtend(row, col);
    lastButtonUpTime = event->xbutton.time;
    if (startSRow != endSRow || startSCol != endSCol) {
	if (replyToEmacs) {
	    count = 0;
	    if (screen->control_eight_bits) {
		line[count++] = CSI;
	    } else {
		line[count++] = ESC;
		line[count++] = '[';
	    }
	    if (rawRow == startSRow && rawCol == startSCol
		&& row == endSRow && col == endSCol) {
		/* Use short-form emacs select */
		line[count++] = 't';
		line[count++] = ' ' + endSCol + 1;
		line[count++] = ' ' + endSRow + 1;
	    } else {
		/* long-form, specify everything */
		line[count++] = 'T';
		line[count++] = ' ' + startSCol + 1;
		line[count++] = ' ' + startSRow + 1;
		line[count++] = ' ' + endSCol + 1;
		line[count++] = ' ' + endSRow + 1;
		line[count++] = ' ' + col + 1;
		line[count++] = ' ' + row + 1;
	    }
	    v_write(screen->respond, line, count);
	    TrackText(0, 0, 0, 0);
	}
    }
    SelectSet(w, event, params, num_params);
    eventMode = NORMAL;
}

void
HandleSelectSet(Widget w,
		XEvent * event,
		String * params,
		Cardinal * num_params)
{
    SelectSet(w, event, params, *num_params);
}

/* ARGSUSED */
static void
SelectSet(Widget w GCC_UNUSED,
	  XEvent * event GCC_UNUSED,
	  String * params,
	  Cardinal num_params)
{
    /* Only do select stuff if non-null select */
    if (startSRow != endSRow || startSCol != endSCol) {
	SaltTextAway(startSRow, startSCol, endSRow, endSCol,
		     params, num_params);
    } else
	DisownSelection(term);
}

#define Abs(x)		((x) < 0 ? -(x) : (x))

/* ARGSUSED */
static void
do_start_extend(Widget w,
		XEvent * event,	/* must be XButtonEvent* */
		String * params GCC_UNUSED,
		Cardinal * num_params GCC_UNUSED,
		Bool use_cursor_loc)
{
    TScreen *screen;
    int row, col, coord;

    if (!IsXtermWidget(w))
	return;

    screen = &((XtermWidget) w)->screen;
    if (SendMousePosition(w, event))
	return;
    firstValidRow = 0;
    lastValidRow = screen->max_row;
#if OPT_READLINE
    if ((KeyModifiers & ShiftMask)
	|| event->xbutton.button != Button3
	|| !(SCREEN_FLAG(screen, dclick3_deletes)))
#endif
	selectUnit = EvalSelectUnit(event->xbutton.time, selectUnit);
    replyToEmacs = FALSE;

#if OPT_READLINE
    CheckSecondPress3(event);
#endif

    if (numberOfClicks == 1
	|| (SCREEN_FLAG(screen, dclick3_deletes)	/* Dclick special */
	    &&!(KeyModifiers & ShiftMask))) {
	/* Save existing selection so we can reestablish it if the guy
	   extends past the other end of the selection */
	saveStartRRow = startERow = startRRow;
	saveStartRCol = startECol = startRCol;
	saveEndRRow = endERow = endRRow;
	saveEndRCol = endECol = endRCol;
    } else {
	/* He just needed the selection mode changed, use old values. */
	startERow = startRRow = saveStartRRow;
	startECol = startRCol = saveStartRCol;
	endERow = endRRow = saveEndRRow;
	endECol = endRCol = saveEndRCol;

    }
    if (use_cursor_loc) {
	row = screen->cursor_row;
	col = screen->cursor_col;
    } else {
	PointToRowCol(event->xbutton.y, event->xbutton.x, &row, &col);
    }
    coord = Coordinate(row, col);

    if (Abs(coord - Coordinate(startSRow, startSCol))
	< Abs(coord - Coordinate(endSRow, endSCol))
	|| coord < Coordinate(startSRow, startSCol)) {
	/* point is close to left side of selection */
	eventMode = LEFTEXTENSION;
	startERow = row;
	startECol = col;
    } else {
	/* point is close to left side of selection */
	eventMode = RIGHTEXTENSION;
	endERow = row;
	endECol = col;
    }
    ComputeSelect(startERow, startECol, endERow, endECol, True);

#if OPT_READLINE
    if (startSRow != endSRow || startSCol != endSCol)
	ExtendingSelection = 1;
#endif
}

static void
ExtendExtend(int row, int col)
{
    int coord = Coordinate(row, col);

    TRACE(("ExtendExtend row=%d, col=%d\n", row, col));
    if (eventMode == LEFTEXTENSION
	&& (coord + (selectUnit != SELECTCHAR)) > Coordinate(endSRow, endSCol)) {
	/* Whoops, he's changed his mind.  Do RIGHTEXTENSION */
	eventMode = RIGHTEXTENSION;
	startERow = saveStartRRow;
	startECol = saveStartRCol;
    } else if (eventMode == RIGHTEXTENSION
	       && coord < Coordinate(startSRow, startSCol)) {
	/* Whoops, he's changed his mind.  Do LEFTEXTENSION */
	eventMode = LEFTEXTENSION;
	endERow = saveEndRRow;
	endECol = saveEndRCol;
    }
    if (eventMode == LEFTEXTENSION) {
	startERow = row;
	startECol = col;
    } else {
	endERow = row;
	endECol = col;
    }
    ComputeSelect(startERow, startECol, endERow, endECol, False);

#if OPT_READLINE
    if (startSRow != endSRow || startSCol != endSCol)
	ExtendingSelection = 1;
#endif
}

void
HandleStartExtend(Widget w,
		  XEvent * event,	/* must be XButtonEvent* */
		  String * params,	/* unused */
		  Cardinal * num_params)	/* unused */
{
    do_start_extend(w, event, params, num_params, False);
}

void
HandleKeyboardStartExtend(Widget w,
			  XEvent * event,	/* must be XButtonEvent* */
			  String * params,	/* unused */
			  Cardinal * num_params)	/* unused */
{
    do_start_extend(w, event, params, num_params, True);
}

void
ScrollSelection(TScreen * screen, int amount)
{
    int minrow = -screen->savedlines - screen->topline;
    int maxrow = screen->max_row - screen->topline;
    int maxcol = screen->max_col;

#define scroll_update_one(row, col) \
	row += amount; \
	if (row < minrow) { \
	    row = minrow; \
	    col = 0; \
	} \
	if (row > maxrow) { \
	    row = maxrow; \
	    col = maxcol; \
	}

    scroll_update_one(startRRow, startRCol);
    scroll_update_one(endRRow, endRCol);
    scroll_update_one(startSRow, startSCol);
    scroll_update_one(endSRow, endSCol);

    scroll_update_one(rawRow, rawCol);

    scroll_update_one(screen->startHRow, screen->startHCol);
    scroll_update_one(screen->endHRow, screen->endHCol);

    screen->startHCoord = Coordinate(screen->startHRow, screen->startHCol);
    screen->endHCoord = Coordinate(screen->endHRow, screen->endHCol);
}

/*ARGSUSED*/
void
ResizeSelection(TScreen * screen GCC_UNUSED, int rows, int cols)
{
    rows--;			/* decr to get 0-max */
    cols--;

    if (startRRow > rows)
	startRRow = rows;
    if (startSRow > rows)
	startSRow = rows;
    if (endRRow > rows)
	endRRow = rows;
    if (endSRow > rows)
	endSRow = rows;
    if (rawRow > rows)
	rawRow = rows;

    if (startRCol > cols)
	startRCol = cols;
    if (startSCol > cols)
	startSCol = cols;
    if (endRCol > cols)
	endRCol = cols;
    if (endSCol > cols)
	endSCol = cols;
    if (rawCol > cols)
	rawCol = cols;
}

#if OPT_WIDE_CHARS
int
iswide(int i)
{
    return (i == HIDDEN_CHAR) || (my_wcwidth(i) == 2);
}
#endif

static void
PointToRowCol(int y,
	      int x,
	      int *r,
	      int *c)
/* Convert pixel coordinates to character coordinates.
   Rows are clipped between firstValidRow and lastValidRow.
   Columns are clipped between to be 0 or greater, but are not clipped to some
       maximum value. */
{
    TScreen *screen = &term->screen;
    int row, col;

    row = (y - screen->border) / FontHeight(screen);
    if (row < firstValidRow)
	row = firstValidRow;
    else if (row > lastValidRow)
	row = lastValidRow;
    col = (x - OriginX(screen)) / FontWidth(screen);
    if (col < 0)
	col = 0;
    else if (col > screen->max_col + 1) {
	col = screen->max_col + 1;
    }
#if OPT_WIDE_CHARS
    /*
     * If we got a click on the right half of a doublewidth character,
     * pretend it happened on the left half.
     */
    if (col > 0
	&& iswide(XTERM_CELL(row, col - 1))
	&& (XTERM_CELL(row, col) == HIDDEN_CHAR)) {
	col -= 1;
    }
#endif
    *r = row;
    *c = col;
}

static int
LastTextCol(int row)
{
    TScreen *screen = &term->screen;
    int i;
    Char *ch;

    if ((row += screen->topline) + screen->savedlines >= 0) {
	for (i = screen->max_col,
	     ch = SCRN_BUF_ATTRS(screen, row) + i;
	     i >= 0 && !(*ch & CHARDRAWN);
	     ch--, i--) ;
#if OPT_DEC_CHRSET
	if (CSET_DOUBLE(SCRN_BUF_CSETS(screen, row)[0])) {
	    i *= 2;
	}
#endif
    } else {
	i = -1;
    }
    return (i);
}

#if !OPT_WIDE_CHARS
/*
** double click table for cut and paste in 8 bits
**
** This table is divided in four parts :
**
**	- control characters	[0,0x1f] U [0x80,0x9f]
**	- separators		[0x20,0x3f] U [0xa0,0xb9]
**	- binding characters	[0x40,0x7f] U [0xc0,0xff]
**	- exceptions
*/
/* *INDENT-OFF* */
static int charClass[256] =
{
/* NUL  SOH  STX  ETX  EOT  ENQ  ACK  BEL */
    32,  1,    1,   1,   1,   1,   1,   1,
/*  BS   HT   NL   VT   NP   CR   SO   SI */
     1,  32,   1,   1,   1,   1,   1,   1,
/* DLE  DC1  DC2  DC3  DC4  NAK  SYN  ETB */
     1,   1,   1,   1,   1,   1,   1,   1,
/* CAN   EM  SUB  ESC   FS   GS   RS   US */
     1,   1,   1,   1,   1,   1,   1,   1,
/*  SP    !    "    #    $    %    &    ' */
    32,  33,  34,  35,  36,  37,  38,  39,
/*   (    )    *    +    ,    -    .    / */
    40,  41,  42,  43,  44,  45,  46,  47,
/*   0    1    2    3    4    5    6    7 */
    48,  48,  48,  48,  48,  48,  48,  48,
/*   8    9    :    ;    <    =    >    ? */
    48,  48,  58,  59,  60,  61,  62,  63,
/*   @    A    B    C    D    E    F    G */
    64,  48,  48,  48,  48,  48,  48,  48,
/*   H    I    J    K    L    M    N    O */
    48,  48,  48,  48,  48,  48,  48,  48,
/*   P    Q    R    S    T    U    V    W */
    48,  48,  48,  48,  48,  48,  48,  48,
/*   X    Y    Z    [    \    ]    ^    _ */
    48,  48,  48,  91,  92,  93,  94,  48,
/*   `    a    b    c    d    e    f    g */
    96,  48,  48,  48,  48,  48,  48,  48,
/*   h    i    j    k    l    m    n    o */
    48,  48,  48,  48,  48,  48,  48,  48,
/*   p    q    r    s    t    u    v    w */
    48,  48,  48,  48,  48,  48,  48,  48,
/*   x    y    z    {    |    }    ~  DEL */
    48,  48,  48, 123, 124, 125, 126,   1,
/* x80  x81  x82  x83  IND  NEL  SSA  ESA */
    1,    1,   1,   1,   1,   1,   1,   1,
/* HTS  HTJ  VTS  PLD  PLU   RI  SS2  SS3 */
    1,    1,   1,   1,   1,   1,   1,   1,
/* DCS  PU1  PU2  STS  CCH   MW  SPA  EPA */
    1,    1,   1,   1,   1,   1,   1,   1,
/* x98  x99  x9A  CSI   ST  OSC   PM  APC */
    1,    1,   1,   1,   1,   1,   1,   1,
/*   -    i   c/    L   ox   Y-    |   So */
    160, 161, 162, 163, 164, 165, 166, 167,
/*  ..   c0   ip   <<    _        R0    - */
    168, 169, 170, 171, 172, 173, 174, 175,
/*   o   +-    2    3    '    u   q|    . */
    176, 177, 178, 179, 180, 181, 182, 183,
/*   ,    1    2   >>  1/4  1/2  3/4    ? */
    184, 185, 186, 187, 188, 189, 190, 191,
/*  A`   A'   A^   A~   A:   Ao   AE   C, */
     48,  48,  48,  48,  48,  48,  48,  48,
/*  E`   E'   E^   E:   I`   I'   I^   I: */
     48,  48,  48,  48,  48,  48,  48,  48,
/*  D-   N~   O`   O'   O^   O~   O:    X */
     48,  48,  48,  48,  48,  48,  48, 215,
/*  O/   U`   U'   U^   U:   Y'    P    B */
     48,  48,  48,  48,  48,  48,  48,  48,
/*  a`   a'   a^   a~   a:   ao   ae   c, */
     48,  48,  48,  48,  48,  48,  48,  48,
/*  e`   e'   e^   e:    i`  i'   i^   i: */
     48,  48,  48,  48,  48,  48,  48,  48,
/*   d   n~   o`   o'   o^   o~   o:   -: */
     48,  48,  48,  48,  48,  48,  48, 247,
/*  o/   u`   u'   u^   u:   y'    P   y: */
     48,  48,  48,  48,  48,  48,  48,  48};
/* *INDENT-ON* */

int
SetCharacterClassRange(int low,	/* in range of [0..255] */
		       int high,
		       int value)	/* arbitrary */
{

    if (low < 0 || high > 255 || high < low)
	return (-1);

    for (; low <= high; low++)
	charClass[low] = value;

    return (0);
}
#endif

#if OPT_WIDE_CHARS
static int
class_of(TScreen * screen, int row, int col)
{
    unsigned value;
#if OPT_DEC_CHRSET
    if (CSET_DOUBLE(SCRN_BUF_CSETS(screen, row + screen->topline)[0])) {
	col /= 2;
    }
#endif
    value = XTERM_CELL(row, col);
    if_OPT_WIDE_CHARS(screen, {
	return CharacterClass(value);
    });
    return CharacterClass(value);
}
#define ClassSelects(screen, row, col, cclass) \
	 (class_of(screen, row, col) == cclass \
	 || XTERM_CELL(row, col) == HIDDEN_CHAR)
#else
#define class_of(screen,row,col) charClass[XTERM_CELL(row, col)]
#define ClassSelects(screen, row, col, cclass) \
	 (class_of(screen,row, col) == cclass)
#endif

/*
 * sets startSRow startSCol endSRow endSCol
 * ensuring that they have legal values
 */
static void
ComputeSelect(int startRow,
	      int startCol,
	      int endRow,
	      int endCol,
	      Bool extend)
{
    TScreen *screen = &term->screen;
    int length;
    int cclass;

    TRACE(("ComputeSelect(startRow=%d, startCol=%d, endRow=%d, endCol=%d, %sextend)\n",
	   startRow, startCol, endRow, endCol, extend ? "" : "no"));

#if OPT_WIDE_CHARS
    if (startCol > 1
	&& iswide(XTERM_CELL(startRow, startCol - 1))
	&& XTERM_CELL(startRow, startCol - 0) == HIDDEN_CHAR) {
	fprintf(stderr, "Adjusting start. Changing downwards from %i.\n", startCol);
	startCol -= 1;
	if (endCol == (startCol + 1))
	    endCol--;
    }

    if (iswide(XTERM_CELL(endRow, endCol - 1))
	&& XTERM_CELL(endRow, endCol) == HIDDEN_CHAR) {
	endCol += 1;
    }
#endif

    if (Coordinate(startRow, startCol) <= Coordinate(endRow, endCol)) {
	startSRow = startRRow = startRow;
	startSCol = startRCol = startCol;
	endSRow = endRRow = endRow;
	endSCol = endRCol = endCol;
    } else {			/* Swap them */
	startSRow = startRRow = endRow;
	startSCol = startRCol = endCol;
	endSRow = endRRow = startRow;
	endSCol = endRCol = startCol;
    }

    switch (selectUnit) {
    case SELECTCHAR:
	if (startSCol > (LastTextCol(startSRow) + 1)) {
	    startSCol = 0;
	    startSRow++;
	}
	if (endSCol > (LastTextCol(endSRow) + 1)) {
	    endSCol = 0;
	    endSRow++;
	}
	break;
    case SELECTWORD:
	if (startSCol > (LastTextCol(startSRow) + 1)) {
	    startSCol = 0;
	    startSRow++;
	} else {
	    cclass = class_of(screen, startSRow, startSCol);
	    do {
		--startSCol;
		if (startSCol < 0
		    && ScrnTstWrapped(screen, startSRow - 1)) {
		    --startSRow;
		    startSCol = LastTextCol(startSRow);
		}
	    } while (startSCol >= 0
		     && ClassSelects(screen, startSRow, startSCol, cclass));
	    ++startSCol;
	}

#if OPT_WIDE_CHARS
	if (startSCol && XTERM_CELL(startSRow, startSCol) == HIDDEN_CHAR)
	    startSCol++;
#endif

	if (endSCol > (LastTextCol(endSRow) + 1)) {
	    endSCol = 0;
	    endSRow++;
	} else {
	    length = LastTextCol(endSRow);
	    cclass = class_of(screen, endSRow, endSCol);
	    do {
		++endSCol;
		if (endSCol > length
		    && ScrnTstWrapped(screen, endSRow)) {
		    endSCol = 0;
		    ++endSRow;
		    length = LastTextCol(endSRow);
		}
	    } while (endSCol <= length
		     && ClassSelects(screen, endSRow, endSCol, cclass));
	    /* Word select selects if pointing to any char
	       in "word", especially in that it includes
	       the last character in a word.  So no --endSCol
	       and do special eol handling */
	    if (endSCol > length + 1) {
		endSCol = 0;
		++endSRow;
	    }
	}

#if OPT_WIDE_CHARS
	if (endSCol && XTERM_CELL(endSRow, endSCol) == HIDDEN_CHAR)
	    endSCol++;
#endif

	saveStartWRow = startSRow;
	saveStartWCol = startSCol;
	break;
    case SELECTLINE:
	while (ScrnTstWrapped(screen, endSRow)) {
	    ++endSRow;
	}
	if (term->screen.cutToBeginningOfLine
	    || startSRow < saveStartWRow) {
	    startSCol = 0;
	    while (ScrnTstWrapped(screen, startSRow - 1)) {
		--startSRow;
	    }
	} else if (!extend) {
	    if ((startRow < saveStartWRow)
		|| (startRow == saveStartWRow
		    && startCol < saveStartWCol)) {
		startSCol = 0;
		while (ScrnTstWrapped(screen, startSRow - 1)) {
		    --startSRow;
		}
	    } else {
		startSRow = saveStartWRow;
		startSCol = saveStartWCol;
	    }
	}
	if (term->screen.cutNewline) {
	    endSCol = 0;
	    ++endSRow;
	} else {
	    endSCol = LastTextCol(endSRow) + 1;
	}
	break;
    }

    /* check boundaries */
    ScrollSelection(screen, 0);

    TrackText(startSRow, startSCol, endSRow, endSCol);
    return;
}

void
TrackText(int frow,
	  int fcol,
	  int trow,
	  int tcol)
    /* Guaranteed (frow, fcol) <= (trow, tcol) */
{
    int from, to;
    TScreen *screen = &term->screen;
    int old_startrow, old_startcol, old_endrow, old_endcol;

    TRACE(("TrackText(frow=%d, fcol=%d, trow=%d, tcol=%d)\n",
	   frow, fcol, trow, tcol));

    old_startrow = screen->startHRow;
    old_startcol = screen->startHCol;
    old_endrow = screen->endHRow;
    old_endcol = screen->endHCol;
    if (frow == old_startrow && fcol == old_startcol &&
	trow == old_endrow && tcol == old_endcol)
	return;
    screen->startHRow = frow;
    screen->startHCol = fcol;
    screen->endHRow = trow;
    screen->endHCol = tcol;
    from = Coordinate(frow, fcol);
    to = Coordinate(trow, tcol);
    if (to <= screen->startHCoord || from > screen->endHCoord) {
	/* No overlap whatsoever between old and new hilite */
	ReHiliteText(old_startrow, old_startcol, old_endrow, old_endcol);
	ReHiliteText(frow, fcol, trow, tcol);
    } else {
	if (from < screen->startHCoord) {
	    /* Extend left end */
	    ReHiliteText(frow, fcol, old_startrow, old_startcol);
	} else if (from > screen->startHCoord) {
	    /* Shorten left end */
	    ReHiliteText(old_startrow, old_startcol, frow, fcol);
	}
	if (to > screen->endHCoord) {
	    /* Extend right end */
	    ReHiliteText(old_endrow, old_endcol, trow, tcol);
	} else if (to < screen->endHCoord) {
	    /* Shorten right end */
	    ReHiliteText(trow, tcol, old_endrow, old_endcol);
	}
    }
    screen->startHCoord = from;
    screen->endHCoord = to;
}

static void
ReHiliteText(int frow,
	     int fcol,
	     int trow,
	     int tcol)
    /* Guaranteed that (frow, fcol) <= (trow, tcol) */
{
    TScreen *screen = &term->screen;
    int i;

    if (frow < 0)
	frow = fcol = 0;
    else if (frow > screen->max_row)
	return;			/* nothing to do, since trow >= frow */

    if (trow < 0)
	return;			/* nothing to do, since frow <= trow */
    else if (trow > screen->max_row) {
	trow = screen->max_row;
	tcol = screen->max_col + 1;
    }
    if (frow == trow && fcol == tcol)
	return;

    if (frow != trow) {		/* do multiple rows */
	if ((i = screen->max_col - fcol + 1) > 0) {	/* first row */
	    ScrnRefresh(screen, frow, fcol, 1, i, True);
	}
	if ((i = trow - frow - 1) > 0) {	/* middle rows */
	    ScrnRefresh(screen, frow + 1, 0, i, screen->max_col + 1, True);
	}
	if (tcol > 0 && trow <= screen->max_row) {	/* last row */
	    ScrnRefresh(screen, trow, 0, 1, tcol, True);
	}
    } else {			/* do single row */
	ScrnRefresh(screen, frow, fcol, 1, tcol - fcol, True);
    }
}

static void
SaltTextAway(int crow, int ccol, int row, int col,
	     String * params,	/* selections */
	     Cardinal num_params)
    /* Guaranteed that (crow, ccol) <= (row, col), and that both points are valid
       (may have row = screen->max_row+1, col = 0) */
{
    TScreen *screen = &term->screen;
    int i, j = 0;
    int eol;
    Char *line;
    Char *lp;

    if (crow == row && ccol > col) {
	int tmp = ccol;
	ccol = col;
	col = tmp;
    }

    --col;
    /* first we need to know how long the string is before we can save it */

    if (row == crow) {
	j = Length(screen, crow, ccol, col);
    } else {			/* two cases, cut is on same line, cut spans multiple lines */
	j += Length(screen, crow, ccol, screen->max_col) + 1;
	for (i = crow + 1; i < row; i++)
	    j += Length(screen, i, 0, screen->max_col) + 1;
	if (col >= 0)
	    j += Length(screen, row, 0, col);
    }

    /* UTF-8 may require more space */
    if_OPT_WIDE_CHARS(screen, {
	j *= 4;
    });

    /* now get some memory to save it in */

    if (screen->selection_size <= j) {
	if ((line = (Char *) malloc((unsigned) j + 1)) == 0)
	    SysError(ERROR_BMALLOC2);
	XtFree((char *) screen->selection_data);
	screen->selection_data = line;
	screen->selection_size = j + 1;
    } else {
	line = screen->selection_data;
    }

    if ((line == 0)
	|| (j < 0))
	return;

    line[j] = '\0';		/* make sure it is null terminated */
    lp = line;			/* lp points to where to save the text */
    if (row == crow) {
	lp = SaveText(screen, row, ccol, col, lp, &eol);
    } else {
	lp = SaveText(screen, crow, ccol, screen->max_col, lp, &eol);
	if (eol)
	    *lp++ = '\n';	/* put in newline at end of line */
	for (i = crow + 1; i < row; i++) {
	    lp = SaveText(screen, i, 0, screen->max_col, lp, &eol);
	    if (eol)
		*lp++ = '\n';
	}
	if (col >= 0)
	    lp = SaveText(screen, row, 0, col, lp, &eol);
    }
    *lp = '\0';			/* make sure we have end marked */

    TRACE(("Salted TEXT:%.*s\n", lp - line, line));
    screen->selection_length = (lp - line);
    _OwnSelection(term, params, num_params);
}

static Boolean
_ConvertSelectionHelper(Widget w,
			Atom * type, XtPointer * value,
			unsigned long *length, int *format,
			int (*conversion_function) (Display *,
						    char **, int,
						    XICCEncodingStyle,
						    XTextProperty *),
			XICCEncodingStyle conversion_style)
{
    Display *d = XtDisplay(w);
    TScreen *screen;
    XTextProperty textprop;

    if (!IsXtermWidget(w))
	return False;

    screen = &((XtermWidget) w)->screen;

    if (conversion_function(d, (char **) &screen->selection_data, 1,
			    conversion_style,
			    &textprop) < Success)
	return False;
    *value = (XtPointer) textprop.value;
    *length = textprop.nitems;
    *type = textprop.encoding;
    *format = textprop.format;
    return True;
}

static Boolean
ConvertSelection(Widget w,
		 Atom * selection,
		 Atom * target,
		 Atom * type,
		 XtPointer * value,
		 unsigned long *length,
		 int *format)
{
    Display *d = XtDisplay(w);
    TScreen *screen;
    Boolean result = False;

    if (!IsXtermWidget(w))
	return False;

    screen = &((XtermWidget) w)->screen;

    if (screen->selection_data == NULL)
	return False;		/* can this happen? */

    if (*target == XA_TARGETS(d)) {
	Atom *targetP;
	Atom *std_targets;
	XPointer std_return = 0;
	unsigned long std_length;
	if (XmuConvertStandardSelection(w, screen->selection_time, selection,
					target, type, &std_return,
					&std_length, format)) {
	    std_targets = (Atom *) (std_return);
	    *length = std_length + 6;
	    targetP = (Atom *) XtMalloc(sizeof(Atom) * (*length));
	    *value = (XtPointer) targetP;
	    *targetP++ = XA_STRING;
	    *targetP++ = XA_TEXT(d);
#ifdef X_HAVE_UTF8_STRING
	    *targetP++ = XA_COMPOUND_TEXT(d);
	    *targetP++ = XA_UTF8_STRING(d);
#else
	    *targetP = XA_COMPOUND_TEXT(d);
	    if_OPT_WIDE_CHARS(screen, {
		*targetP = XA_UTF8_STRING(d);
	    });
	    targetP++;
#endif
	    *targetP++ = XA_LENGTH(d);
	    *targetP++ = XA_LIST_LENGTH(d);
	    memcpy(targetP, std_targets, sizeof(Atom) * std_length);
	    XtFree((char *) std_targets);
	    *type = XA_ATOM;
	    *format = 32;
	    result = True;
	}
    }
#if OPT_WIDE_CHARS
    else if (screen->wide_chars && *target == XA_STRING) {
	result =
	    _ConvertSelectionHelper(w,
				    type, value, length, format,
				    Xutf8TextListToTextProperty,
				    XStringStyle);
    } else if (screen->wide_chars && *target == XA_UTF8_STRING(d)) {
	result =
	    _ConvertSelectionHelper(w,
				    type, value, length, format,
				    Xutf8TextListToTextProperty,
				    XUTF8StringStyle);
    } else if (screen->wide_chars && *target == XA_TEXT(d)) {
	result =
	    _ConvertSelectionHelper(w,
				    type, value, length, format,
				    Xutf8TextListToTextProperty,
				    XStdICCTextStyle);
    } else if (screen->wide_chars && *target == XA_COMPOUND_TEXT(d)) {
	result =
	    _ConvertSelectionHelper(w,
				    type, value, length, format,
				    Xutf8TextListToTextProperty,
				    XCompoundTextStyle);
    }
#endif

    else if (*target == XA_STRING) {	/* not wide_chars */
	/* We can only reach this point if the selection requestor
	   requested STRING before any of TEXT, COMPOUND_TEXT or
	   UTF8_STRING.  We therefore assume that the requestor is not
	   properly internationalised, and dump raw eight-bit data
	   with no conversion into the selection.  Yes, this breaks
	   the ICCCM in non-Latin-1 locales. */
	*type = XA_STRING;
	*value = screen->selection_data;
	*length = screen->selection_length;
	*format = 8;
	result = True;
    } else if (*target == XA_TEXT(d)) {		/* not wide_chars */
	result =
	    _ConvertSelectionHelper(w,
				    type, value, length, format,
				    XmbTextListToTextProperty,
				    XStdICCTextStyle);
    } else if (*target == XA_COMPOUND_TEXT(d)) {	/* not wide_chars */
	result =
	    _ConvertSelectionHelper(w,
				    type, value, length, format,
				    XmbTextListToTextProperty,
				    XCompoundTextStyle);
    }
#ifdef X_HAVE_UTF8_STRING
    else if (*target == XA_UTF8_STRING(d)) {	/* not wide_chars */
	result =
	    _ConvertSelectionHelper(w,
				    type, value, length, format,
				    XmbTextListToTextProperty,
				    XUTF8StringStyle);
    }
#endif
    else if (*target == XA_LIST_LENGTH(d)) {
	*value = XtMalloc(4);
	if (sizeof(long) == 4)
	     *(long *) *value = 1;
	else {
	    long temp = 1;
	    memcpy((char *) *value, ((char *) &temp) + sizeof(long) - 4, 4);
	}
	*type = XA_INTEGER;
	*length = 1;
	*format = 32;
	result = True;
    } else if (*target == XA_LENGTH(d)) {
	/* This value is wrong if we have UTF-8 text */
	*value = XtMalloc(4);
	if (sizeof(long) == 4)
	     *(long *) *value = screen->selection_length;
	else {
	    long temp = screen->selection_length;
	    memcpy((char *) *value, ((char *) &temp) + sizeof(long) - 4, 4);
	}
	*type = XA_INTEGER;
	*length = 1;
	*format = 32;
	result = True;
    } else if (XmuConvertStandardSelection(w,
					   screen->selection_time, selection,
					   target, type, (XPointer *) value,
					   length, format)) {
	result = True;
    }

    /* else */
    return result;
}

static void
LoseSelection(Widget w, Atom * selection)
{
    TScreen *screen;
    Atom *atomP;
    Cardinal i;

    if (!IsXtermWidget(w))
	return;

    screen = &((XtermWidget) w)->screen;
    for (i = 0, atomP = screen->selection_atoms;
	 i < screen->selection_count; i++, atomP++) {
	if (*selection == *atomP)
	    *atomP = (Atom) 0;
	switch (*atomP) {
	case XA_CUT_BUFFER0:
	case XA_CUT_BUFFER1:
	case XA_CUT_BUFFER2:
	case XA_CUT_BUFFER3:
	case XA_CUT_BUFFER4:
	case XA_CUT_BUFFER5:
	case XA_CUT_BUFFER6:
	case XA_CUT_BUFFER7:
	    *atomP = (Atom) 0;
	}
    }

    for (i = screen->selection_count; i; i--) {
	if (screen->selection_atoms[i - 1] != 0)
	    break;
    }
    screen->selection_count = i;

    for (i = 0, atomP = screen->selection_atoms;
	 i < screen->selection_count; i++, atomP++) {
	if (*atomP == (Atom) 0) {
	    *atomP = screen->selection_atoms[--screen->selection_count];
	}
    }

    if (screen->selection_count == 0)
	TrackText(0, 0, 0, 0);
}

/* ARGSUSED */
static void
SelectionDone(Widget w GCC_UNUSED,
	      Atom * selection GCC_UNUSED,
	      Atom * target GCC_UNUSED)
{
    /* empty proc so Intrinsics know we want to keep storage */
}

static void
_OwnSelection(XtermWidget termw,
	      String * selections,
	      Cardinal count)
{
    Atom *atoms = termw->screen.selection_atoms;
    Cardinal i;
    Boolean have_selection = False;

    if (termw->screen.selection_length < 0)
	return;

    if (count > termw->screen.sel_atoms_size) {
	XtFree((char *) atoms);
	atoms = (Atom *) XtMalloc(count * sizeof(Atom));
	termw->screen.selection_atoms = atoms;
	termw->screen.sel_atoms_size = count;
    }
    XmuInternStrings(XtDisplay((Widget) termw), selections, count, atoms);
    for (i = 0; i < count; i++) {
	int cutbuffer;
	switch (atoms[i]) {
	case XA_CUT_BUFFER0:
	    cutbuffer = 0;
	    break;
	case XA_CUT_BUFFER1:
	    cutbuffer = 1;
	    break;
	case XA_CUT_BUFFER2:
	    cutbuffer = 2;
	    break;
	case XA_CUT_BUFFER3:
	    cutbuffer = 3;
	    break;
	case XA_CUT_BUFFER4:
	    cutbuffer = 4;
	    break;
	case XA_CUT_BUFFER5:
	    cutbuffer = 5;
	    break;
	case XA_CUT_BUFFER6:
	    cutbuffer = 6;
	    break;
	case XA_CUT_BUFFER7:
	    cutbuffer = 7;
	    break;
	default:
	    cutbuffer = -1;
	}
	if (cutbuffer >= 0) {
	    if (termw->screen.selection_length >
		4 * XMaxRequestSize(XtDisplay((Widget) termw)) - 32) {
		fprintf(stderr,
			"%s: selection too big (%d bytes), not storing in CUT_BUFFER%d\n",
			xterm_name, termw->screen.selection_length, cutbuffer);
	    } else {
		/* This used to just use the UTF-8 data, which was totally
		 * broken as not even the corresponding paste code in Xterm
		 * understood this!  So now it converts to Latin1 first.
		 *   Robert Brady, 2000-09-05
		 */
		unsigned long length = termw->screen.selection_length;
		Char *data = termw->screen.selection_data;
		if_OPT_WIDE_CHARS((&(termw->screen)), {
		    data = UTF8toLatin1(data, length, &length);
		});
		TRACE(("XStoreBuffer(%d)\n", cutbuffer));
		XStoreBuffer(XtDisplay((Widget) termw),
			     (char *) data, length, cutbuffer);
	    }
	} else if (!replyToEmacs) {
	    have_selection |=
		XtOwnSelection((Widget) termw, atoms[i],
			       termw->screen.selection_time,
			       ConvertSelection, LoseSelection, SelectionDone);
	}
    }
    if (!replyToEmacs)
	termw->screen.selection_count = count;
    if (!have_selection)
	TrackText(0, 0, 0, 0);
}

void
DisownSelection(XtermWidget termw)
{
    Atom *atoms = termw->screen.selection_atoms;
    Cardinal count = termw->screen.selection_count;
    Cardinal i;

    for (i = 0; i < count; i++) {
	int cutbuffer;
	switch (atoms[i]) {
	case XA_CUT_BUFFER0:
	    cutbuffer = 0;
	    break;
	case XA_CUT_BUFFER1:
	    cutbuffer = 1;
	    break;
	case XA_CUT_BUFFER2:
	    cutbuffer = 2;
	    break;
	case XA_CUT_BUFFER3:
	    cutbuffer = 3;
	    break;
	case XA_CUT_BUFFER4:
	    cutbuffer = 4;
	    break;
	case XA_CUT_BUFFER5:
	    cutbuffer = 5;
	    break;
	case XA_CUT_BUFFER6:
	    cutbuffer = 6;
	    break;
	case XA_CUT_BUFFER7:
	    cutbuffer = 7;
	    break;
	default:
	    cutbuffer = -1;
	}
	if (cutbuffer < 0)
	    XtDisownSelection((Widget) termw, atoms[i],
			      termw->screen.selection_time);
    }
    termw->screen.selection_count = 0;
    termw->screen.startHRow = termw->screen.startHCol = 0;
    termw->screen.endHRow = termw->screen.endHCol = 0;
}

/* returns number of chars in line from scol to ecol out */
/* ARGSUSED */
static int
Length(TScreen * screen GCC_UNUSED,
       int row,
       int scol,
       int ecol)
{
    int lastcol = LastTextCol(row);

    if (ecol > lastcol)
	ecol = lastcol;
    return (ecol - scol + 1);
}

/* copies text into line, preallocated */
static Char *
SaveText(TScreen * screen,
	 int row,
	 int scol,
	 int ecol,
	 Char * lp,		/* pointer to where to put the text */
	 int *eol)
{
    int i = 0;
    unsigned c;
    Char *result = lp;
#if OPT_WIDE_CHARS
    int previous = 0;
    unsigned c_1 = 0, c_2 = 0;
#endif

    i = Length(screen, row, scol, ecol);
    ecol = scol + i;
#if OPT_DEC_CHRSET
    if (CSET_DOUBLE(SCRN_BUF_CSETS(screen, row + screen->topline)[0])) {
	scol = (scol + 0) / 2;
	ecol = (ecol + 1) / 2;
    }
#endif
    *eol = !ScrnTstWrapped(screen, row);
    for (i = scol; i < ecol; i++) {
	c = E2A(XTERM_CELL(row, i));
#if OPT_WIDE_CHARS
	if (screen->utf8_mode) {
	    c_1 = E2A(XTERM_CELL_C1(row, i));
	    c_2 = E2A(XTERM_CELL_C2(row, i));
	}

	/* We want to strip out every occurrence of HIDDEN_CHAR AFTER a
	 * wide character.
	 */
	if (c == HIDDEN_CHAR && iswide(previous)) {
	    previous = c;
	    /* Combining characters attached to double-width characters
	       are in memory attached to the HIDDEN_CHAR */
	    if (c_1) {
		lp = convertToUTF8(lp, c_1);
		if (c_2)
		    lp = convertToUTF8(lp, c_2);
	    }
	    continue;
	}
	previous = c;
	if (screen->utf8_mode) {
	    lp = convertToUTF8(lp, c);
	    if (c_1) {
		lp = convertToUTF8(lp, c_1);
		if (c_2)
		    lp = convertToUTF8(lp, c_2);
	    }
	} else
#endif
	{
	    if (c == 0) {
		c = E2A(' ');
	    } else if (c < E2A(' ')) {
		if (c == XPOUND)
		    c = 0x23;	/* char on screen is pound sterling */
		else
		    c += 0x5f;	/* char is from DEC drawing set */
	    } else if (c == 0x7f) {
		c = 0x5f;
	    }
	    *lp++ = A2E(c);
	}
	if (c != E2A(' '))
	    result = lp;
    }

    /*
     * If requested, trim trailing blanks from selected lines.  Do not do this
     * if the line is wrapped.
     */
    if (!*eol || !screen->trim_selection)
	result = lp;

    return (result);
}

static int
BtnCode(XButtonEvent * event, int button)
{
    int result = 32 + (KeyState(event->state) << 2);

    if (button < 0 || button > 5) {
	result += 3;
    } else {
	if (button > 3)
	    result += (64 - 4);
	if (event->type == MotionNotify)
	    result += 32;
	result += button;
    }
    return result;
}

#define MOUSE_LIMIT (255 - 32)

static void
EditorButton(XButtonEvent * event)
{
    TScreen *screen = &term->screen;
    int pty = screen->respond;
    Char line[6];
    int row, col;
    int button, count = 0;

    /* If button event, get button # adjusted for DEC compatibility */
    button = event->button - 1;
    if (button >= 3)
	button++;

    /* Compute character position of mouse pointer */
    row = (event->y - screen->border) / FontHeight(screen);
    col = (event->x - OriginX(screen)) / FontWidth(screen);

    /* Limit to screen dimensions */
    if (row < 0)
	row = 0;
    else if (row > screen->max_row)
	row = screen->max_row;
    else if (row > MOUSE_LIMIT)
	row = MOUSE_LIMIT;

    if (col < 0)
	col = 0;
    else if (col > screen->max_col)
	col = screen->max_col;
    else if (col > MOUSE_LIMIT)
	col = MOUSE_LIMIT;

    /* Build key sequence starting with \E[M */
    if (screen->control_eight_bits) {
	line[count++] = CSI;
    } else {
	line[count++] = ESC;
	line[count++] = '[';
    }
    line[count++] = 'M';

    /* Add event code to key sequence */
    if (screen->send_mouse_pos == X10_MOUSE) {
	line[count++] = ' ' + button;
    } else {
	/* Button-Motion events */
	switch (event->type) {
	case ButtonPress:
	    line[count++] = BtnCode(event, screen->mouse_button = button);
	    break;
	case ButtonRelease:
	    /*
	     * Wheel mouse interface generates release-events for buttons
	     * 4 and 5, coded here as 3 and 4 respectively.  We change the
	     * release for buttons 1..3 to a -1.
	     */
	    if (button < 3)
		button = -1;
	    line[count++] = BtnCode(event, screen->mouse_button = button);
	    break;
	case MotionNotify:
	    /* BTN_EVENT_MOUSE and ANY_EVENT_MOUSE modes send motion
	     * events only if character cell has changed.
	     */
	    if ((row == screen->mouse_row)
		&& (col == screen->mouse_col))
		return;
	    line[count++] = BtnCode(event, screen->mouse_button);
	    break;
	default:
	    return;
	}
    }

    screen->mouse_row = row;
    screen->mouse_col = col;

    /* Add pointer position to key sequence */
    line[count++] = ' ' + col + 1;
    line[count++] = ' ' + row + 1;

    TRACE(("mouse at %d,%d button+mask = %#x\n", row, col,
	   (screen->control_eight_bits) ? line[2] : line[3]));

    /* Transmit key sequence to process running under xterm */
    v_write(pty, line, count);
}

/*ARGSUSED*/
#if OPT_TEK4014
void
HandleGINInput(Widget w GCC_UNUSED,
	       XEvent * event GCC_UNUSED,
	       String * param_list,
	       Cardinal * nparamsp)
{
    if (term->screen.TekGIN && *nparamsp == 1) {
	int c = param_list[0][0];
	switch (c) {
	case 'l':
	case 'm':
	case 'r':
	case 'L':
	case 'M':
	case 'R':
	    break;
	default:
	    Bell(XkbBI_MinorError, 0);	/* let them know they goofed */
	    c = 'l';		/* provide a default */
	}
	TekEnqMouse(c | 0x80);
	TekGINoff();
    } else {
	Bell(XkbBI_MinorError, 0);
    }
}
#endif /* OPT_TEK4014 */

/* ARGSUSED */
void
HandleSecure(Widget w GCC_UNUSED,
	     XEvent * event,	/* unused */
	     String * params GCC_UNUSED,	/* [0] = volume */
	     Cardinal * param_count GCC_UNUSED)		/* 0 or 1 */
{
    Time ev_time = CurrentTime;

    if ((event->xany.type == KeyPress) ||
	(event->xany.type == KeyRelease))
	ev_time = event->xkey.time;
    else if ((event->xany.type == ButtonPress) ||
	     (event->xany.type == ButtonRelease))
	ev_time = event->xbutton.time;
    DoSecureKeyboard(ev_time);
}

/*
 *	$Xorg: util.c,v 1.3 2000/08/17 19:55:10 cpqbld Exp $
 */

/* $XFree86: xc/programs/xterm/util.c,v 3.73 2002/12/27 21:05:23 dickey Exp $ */

/*
 * Copyright 1999-2001,2002 by Thomas E. Dickey
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

/* util.c */

#include <xterm.h>

#include <data.h>
#include <error.h>
#include <menu.h>
#include <fontutils.h>

#if OPT_WIDE_CHARS
#include <wcwidth.h>
#endif

#include <stdio.h>
#include <ctype.h>

static int ClearInLine(TScreen * screen, int row, int col, int len);
static int handle_translated_exposure(TScreen * screen,
				      int rect_x,
				      int rect_y,
				      unsigned rect_width,
				      unsigned rect_height);
static void ClearLeft(TScreen * screen);
static void CopyWait(TScreen * screen);
static void horizontal_copy_area(TScreen * screen,
				 int firstchar,
				 int nchars,
				 int amount);
static void vertical_copy_area(TScreen * screen,
			       int firstline,
			       int nlines,
			       int amount);

/*
 * These routines are used for the jump scroll feature
 */
void
FlushScroll(TScreen * screen)
{
    int i;
    int shift = -screen->topline;
    int bot = screen->max_row - shift;
    int refreshtop;
    int refreshheight;
    int scrolltop;
    int scrollheight;

    if (screen->cursor_state)
	HideCursor();
    if (screen->scroll_amt > 0) {
	refreshheight = screen->refresh_amt;
	scrollheight = screen->bot_marg - screen->top_marg -
	    refreshheight + 1;
	if ((refreshtop = screen->bot_marg - refreshheight + 1 + shift) >
	    (i = screen->max_row - screen->scroll_amt + 1))
	    refreshtop = i;
	if (screen->scrollWidget && !screen->alternate
	    && screen->top_marg == 0) {
	    scrolltop = 0;
	    if ((scrollheight += shift) > i)
		scrollheight = i;
	    if ((i = screen->bot_marg - bot) > 0 &&
		(refreshheight -= i) < screen->scroll_amt)
		refreshheight = screen->scroll_amt;
	    if ((i = screen->savedlines) < screen->savelines) {
		if ((i += screen->scroll_amt) >
		    screen->savelines)
		    i = screen->savelines;
		screen->savedlines = i;
		ScrollBarDrawThumb(screen->scrollWidget);
	    }
	} else {
	    scrolltop = screen->top_marg + shift;
	    if ((i = bot - (screen->bot_marg - screen->refresh_amt +
			    screen->scroll_amt)) > 0) {
		if (bot < screen->bot_marg)
		    refreshheight = screen->scroll_amt + i;
	    } else {
		scrollheight += i;
		refreshheight = screen->scroll_amt;
		if ((i = screen->top_marg + screen->scroll_amt -
		     1 - bot) > 0) {
		    refreshtop += i;
		    refreshheight -= i;
		}
	    }
	}
    } else {
	refreshheight = -screen->refresh_amt;
	scrollheight = screen->bot_marg - screen->top_marg -
	    refreshheight + 1;
	refreshtop = screen->top_marg + shift;
	scrolltop = refreshtop + refreshheight;
	if ((i = screen->bot_marg - bot) > 0)
	    scrollheight -= i;
	if ((i = screen->top_marg + refreshheight - 1 - bot) > 0)
	    refreshheight -= i;
    }
    scrolling_copy_area(screen, scrolltop + screen->scroll_amt,
			scrollheight, screen->scroll_amt);
    ScrollSelection(screen, -(screen->scroll_amt));
    screen->scroll_amt = 0;
    screen->refresh_amt = 0;
    if (refreshheight > 0) {
	ClearCurBackground(screen,
			   (int) refreshtop * FontHeight(screen) + screen->border,
			   (int) OriginX(screen),
			   (unsigned) refreshheight * FontHeight(screen),
			   (unsigned) Width(screen));
	ScrnRefresh(screen, refreshtop, 0, refreshheight,
		    screen->max_col + 1, False);
    }
}

int
AddToRefresh(TScreen * screen)
{
    int amount = screen->refresh_amt;
    int row = screen->cur_row;

    if (amount == 0)
	return (0);
    if (amount > 0) {
	int bottom;

	if (row == (bottom = screen->bot_marg) - amount) {
	    screen->refresh_amt++;
	    return (1);
	}
	return (row >= bottom - amount + 1 && row <= bottom);
    } else {
	int top;

	amount = -amount;
	if (row == (top = screen->top_marg) + amount) {
	    screen->refresh_amt--;
	    return (1);
	}
	return (row <= top + amount - 1 && row >= top);
    }
}

/*
 * scrolls the screen by amount lines, erases bottom, doesn't alter
 * cursor position (i.e. cursor moves down amount relative to text).
 * All done within the scrolling region, of course.
 * requires: amount > 0
 */
void
xtermScroll(TScreen * screen, int amount)
{
    int i = screen->bot_marg - screen->top_marg + 1;
    int shift;
    int bot;
    int refreshtop = 0;
    int refreshheight;
    int scrolltop;
    int scrollheight;

    if (screen->cursor_state)
	HideCursor();
    if (amount > i)
	amount = i;
    if (screen->jumpscroll) {
	if (screen->scroll_amt > 0) {
	    if (screen->refresh_amt + amount > i)
		FlushScroll(screen);
	    screen->scroll_amt += amount;
	    screen->refresh_amt += amount;
	} else {
	    if (screen->scroll_amt < 0)
		FlushScroll(screen);
	    screen->scroll_amt = amount;
	    screen->refresh_amt = amount;
	}
	refreshheight = 0;
    } else {
	ScrollSelection(screen, -(amount));
	if (amount == i) {
	    ClearScreen(screen);
	    return;
	}
	shift = -screen->topline;
	bot = screen->max_row - shift;
	scrollheight = i - amount;
	refreshheight = amount;
	if ((refreshtop = screen->bot_marg - refreshheight + 1 + shift) >
	    (i = screen->max_row - refreshheight + 1))
	    refreshtop = i;
	if (screen->scrollWidget && !screen->alternate
	    && screen->top_marg == 0) {
	    scrolltop = 0;
	    if ((scrollheight += shift) > i)
		scrollheight = i;
	    if ((i = screen->savedlines) < screen->savelines) {
		if ((i += amount) > screen->savelines)
		    i = screen->savelines;
		screen->savedlines = i;
		ScrollBarDrawThumb(screen->scrollWidget);
	    }
	} else {
	    scrolltop = screen->top_marg + shift;
	    if ((i = screen->bot_marg - bot) > 0) {
		scrollheight -= i;
		if ((i = screen->top_marg + amount - 1 - bot) >= 0) {
		    refreshtop += i;
		    refreshheight -= i;
		}
	    }
	}

	if (screen->multiscroll && amount == 1 &&
	    screen->topline == 0 && screen->top_marg == 0 &&
	    screen->bot_marg == screen->max_row) {
	    if (screen->incopy < 0 && screen->scrolls == 0)
		CopyWait(screen);
	    screen->scrolls++;
	}
	scrolling_copy_area(screen, scrolltop + amount, scrollheight, amount);
	if (refreshheight > 0) {
	    ClearCurBackground(screen,
			       (int) refreshtop * FontHeight(screen) + screen->border,
			       (int) OriginX(screen),
			       (unsigned) refreshheight * FontHeight(screen),
			       (unsigned) Width(screen));
	    if (refreshheight > shift)
		refreshheight = shift;
	}
    }
    if (screen->scrollWidget && !screen->alternate && screen->top_marg == 0)
	ScrnDeleteLine(screen, screen->allbuf,
		       screen->bot_marg + screen->savelines, 0,
		       amount, screen->max_col + 1);
    else
	ScrnDeleteLine(screen, screen->visbuf,
		       screen->bot_marg, screen->top_marg,
		       amount, screen->max_col + 1);
    if (refreshheight > 0)
	ScrnRefresh(screen, refreshtop, 0, refreshheight,
		    screen->max_col + 1, False);
}

/*
 * Reverse scrolls the screen by amount lines, erases top, doesn't alter
 * cursor position (i.e. cursor moves up amount relative to text).
 * All done within the scrolling region, of course.
 * Requires: amount > 0
 */
void
RevScroll(TScreen * screen, int amount)
{
    int i = screen->bot_marg - screen->top_marg + 1;
    int shift;
    int bot;
    int refreshtop;
    int refreshheight;
    int scrolltop;
    int scrollheight;

    if (screen->cursor_state)
	HideCursor();
    if (amount > i)
	amount = i;
    if (screen->jumpscroll) {
	if (screen->scroll_amt < 0) {
	    if (-screen->refresh_amt + amount > i)
		FlushScroll(screen);
	    screen->scroll_amt -= amount;
	    screen->refresh_amt -= amount;
	} else {
	    if (screen->scroll_amt > 0)
		FlushScroll(screen);
	    screen->scroll_amt = -amount;
	    screen->refresh_amt = -amount;
	}
    } else {
	shift = -screen->topline;
	bot = screen->max_row - shift;
	refreshheight = amount;
	scrollheight = screen->bot_marg - screen->top_marg -
	    refreshheight + 1;
	refreshtop = screen->top_marg + shift;
	scrolltop = refreshtop + refreshheight;
	if ((i = screen->bot_marg - bot) > 0)
	    scrollheight -= i;
	if ((i = screen->top_marg + refreshheight - 1 - bot) > 0)
	    refreshheight -= i;

	if (screen->multiscroll && amount == 1 &&
	    screen->topline == 0 && screen->top_marg == 0 &&
	    screen->bot_marg == screen->max_row) {
	    if (screen->incopy < 0 && screen->scrolls == 0)
		CopyWait(screen);
	    screen->scrolls++;
	}
	scrolling_copy_area(screen, scrolltop - amount, scrollheight, -amount);
	if (refreshheight > 0) {
	    ClearCurBackground(screen,
			       (int) refreshtop * FontHeight(screen) + screen->border,
			       (int) OriginX(screen),
			       (unsigned) refreshheight * FontHeight(screen),
			       (unsigned) Width(screen));
	}
    }
    ScrnInsertLine(screen, screen->visbuf, screen->bot_marg, screen->top_marg,
		   amount, screen->max_col + 1);
}

/*
 * If cursor not in scrolling region, returns.  Else,
 * inserts n blank lines at the cursor's position.  Lines above the
 * bottom margin are lost.
 */
void
InsertLine(TScreen * screen, int n)
{
    int i;
    int shift;
    int bot;
    int refreshtop;
    int refreshheight;
    int scrolltop;
    int scrollheight;

    if (screen->cur_row < screen->top_marg ||
	screen->cur_row > screen->bot_marg)
	return;
    if (screen->cursor_state)
	HideCursor();
    screen->do_wrap = 0;
    if (n > (i = screen->bot_marg - screen->cur_row + 1))
	n = i;
    if (screen->jumpscroll) {
	if (screen->scroll_amt <= 0 &&
	    screen->cur_row <= -screen->refresh_amt) {
	    if (-screen->refresh_amt + n > screen->max_row + 1)
		FlushScroll(screen);
	    screen->scroll_amt -= n;
	    screen->refresh_amt -= n;
	} else if (screen->scroll_amt)
	    FlushScroll(screen);
    }
    if (!screen->scroll_amt) {
	shift = -screen->topline;
	bot = screen->max_row - shift;
	refreshheight = n;
	scrollheight = screen->bot_marg - screen->cur_row - refreshheight + 1;
	refreshtop = screen->cur_row + shift;
	scrolltop = refreshtop + refreshheight;
	if ((i = screen->bot_marg - bot) > 0)
	    scrollheight -= i;
	if ((i = screen->cur_row + refreshheight - 1 - bot) > 0)
	    refreshheight -= i;
	vertical_copy_area(screen, scrolltop - n, scrollheight, -n);
	if (refreshheight > 0) {
	    ClearCurBackground(screen,
			       (int) refreshtop * FontHeight(screen) + screen->border,
			       (int) OriginX(screen),
			       (unsigned) refreshheight * FontHeight(screen),
			       (unsigned) Width(screen));
	}
    }
    ScrnInsertLine(screen, screen->visbuf, screen->bot_marg, screen->cur_row,
		   n, screen->max_col + 1);
}

/*
 * If cursor not in scrolling region, returns.  Else, deletes n lines
 * at the cursor's position, lines added at bottom margin are blank.
 */
void
DeleteLine(TScreen * screen, int n)
{
    int i;
    int shift;
    int bot;
    int refreshtop;
    int refreshheight;
    int scrolltop;
    int scrollheight;

    if (screen->cur_row < screen->top_marg ||
	screen->cur_row > screen->bot_marg)
	return;
    if (screen->cursor_state)
	HideCursor();
    screen->do_wrap = 0;
    if (n > (i = screen->bot_marg - screen->cur_row + 1))
	n = i;
    if (screen->jumpscroll) {
	if (screen->scroll_amt >= 0 && screen->cur_row == screen->top_marg) {
	    if (screen->refresh_amt + n > screen->max_row + 1)
		FlushScroll(screen);
	    screen->scroll_amt += n;
	    screen->refresh_amt += n;
	} else if (screen->scroll_amt)
	    FlushScroll(screen);
    }
    if (!screen->scroll_amt) {

	shift = -screen->topline;
	bot = screen->max_row - shift;
	scrollheight = i - n;
	refreshheight = n;
	if ((refreshtop = screen->bot_marg - refreshheight + 1 + shift) >
	    (i = screen->max_row - refreshheight + 1))
	    refreshtop = i;
	if (screen->scrollWidget && !screen->alternate && screen->cur_row == 0) {
	    scrolltop = 0;
	    if ((scrollheight += shift) > i)
		scrollheight = i;
	    if ((i = screen->savedlines) < screen->savelines) {
		if ((i += n) > screen->savelines)
		    i = screen->savelines;
		screen->savedlines = i;
		ScrollBarDrawThumb(screen->scrollWidget);
	    }
	} else {
	    scrolltop = screen->cur_row + shift;
	    if ((i = screen->bot_marg - bot) > 0) {
		scrollheight -= i;
		if ((i = screen->cur_row + n - 1 - bot) >= 0) {
		    refreshheight -= i;
		}
	    }
	}
	vertical_copy_area(screen, scrolltop + n, scrollheight, n);
	if (refreshheight > 0) {
	    ClearCurBackground(screen,
			       (int) refreshtop * FontHeight(screen) + screen->border,
			       (int) OriginX(screen),
			       (unsigned) refreshheight * FontHeight(screen),
			       (unsigned) Width(screen));
	}
    }
    /* adjust screen->buf */
    if (screen->scrollWidget && !screen->alternate && screen->cur_row == 0)
	ScrnDeleteLine(screen, screen->allbuf,
		       screen->bot_marg + screen->savelines, 0,
		       n, screen->max_col + 1);
    else
	ScrnDeleteLine(screen, screen->visbuf,
		       screen->bot_marg, screen->cur_row,
		       n, screen->max_col + 1);
}

/*
 * Insert n blanks at the cursor's position, no wraparound
 */
void
InsertChar(TScreen * screen, int n)
{
    register int width;

    if (screen->cursor_state)
	HideCursor();
    screen->do_wrap = 0;
    if (n > (width = screen->max_col + 1 - screen->cur_col))
	n = width;

    if (screen->cur_row - screen->topline <= screen->max_row) {
	if (!AddToRefresh(screen)) {
	    int col = screen->max_col + 1 - n;
	    if (screen->scroll_amt)
		FlushScroll(screen);

#if OPT_DEC_CHRSET
	    if (CSET_DOUBLE(SCRN_BUF_CSETS(screen, screen->cur_row)[0])) {
		col = (screen->max_col + 1) / 2 - n;
	    }
#endif
	    /*
	     * prevent InsertChar from shifting the end of a line over
	     * if it is being appended to
	     */
	    if (non_blank_line(screen->visbuf, screen->cur_row,
			       screen->cur_col, screen->max_col + 1))
		horizontal_copy_area(screen, screen->cur_col,
				     col - screen->cur_col,
				     n);

	    ClearCurBackground(
				  screen,
				  CursorY(screen, screen->cur_row),
				  CurCursorX(screen, screen->cur_row, screen->cur_col),
				  FontHeight(screen),
				  n * CurFontWidth(screen, screen->cur_row));
	}
    }
    /* adjust screen->buf */
    ScrnInsertChar(screen, n);
}

/*
 * Deletes n chars at the cursor's position, no wraparound.
 */
void
DeleteChar(TScreen * screen, int n)
{
    int width;

    if (screen->cursor_state)
	HideCursor();
    screen->do_wrap = 0;
    if (n > (width = screen->max_col + 1 - screen->cur_col))
	n = width;

    if (screen->cur_row - screen->topline <= screen->max_row) {
	if (!AddToRefresh(screen)) {
	    int col = screen->max_col + 1 - n;
	    if (screen->scroll_amt)
		FlushScroll(screen);

#if OPT_DEC_CHRSET
	    if (CSET_DOUBLE(SCRN_BUF_CSETS(screen, screen->cur_row)[0])) {
		col = (screen->max_col + 1) / 2 - n;
	    }
#endif
	    horizontal_copy_area(screen, screen->cur_col + n,
				 col - screen->cur_col,
				 -n);

	    ClearCurBackground(
				  screen,
				  CursorY(screen, screen->cur_row),
				  CurCursorX(screen, screen->cur_row, col),
				  FontHeight(screen),
				  n * CurFontWidth(screen, screen->cur_row));
	}
    }
    /* adjust screen->buf */
    ScrnDeleteChar(screen, n);
}

/*
 * Clear from cursor position to beginning of display, inclusive.
 */
static void
ClearAbove(TScreen * screen)
{
    if (screen->protected_mode != OFF_PROTECT) {
	int row;
	for (row = 0; row <= screen->max_row; row++)
	    ClearInLine(screen, row, 0, screen->max_col + 1);
    } else {
	int top, height;

	if (screen->cursor_state)
	    HideCursor();
	if ((top = -screen->topline) <= screen->max_row) {
	    if (screen->scroll_amt)
		FlushScroll(screen);
	    if ((height = screen->cur_row + top) > screen->max_row)
		height = screen->max_row;
	    if ((height -= top) > 0) {
		ClearCurBackground(screen,
				   top * FontHeight(screen) + screen->border,
				   OriginX(screen),
				   height * FontHeight(screen),
				   Width(screen));
	    }
	}
	ClearBufRows(screen, 0, screen->cur_row - 1);
    }

    if (screen->cur_row - screen->topline <= screen->max_row)
	ClearLeft(screen);
}

/*
 * Clear from cursor position to end of display, inclusive.
 */
static void
ClearBelow(TScreen * screen)
{
    ClearRight(screen, -1);

    if (screen->protected_mode != OFF_PROTECT) {
	int row;
	for (row = screen->cur_row + 1; row <= screen->max_row; row++)
	    ClearInLine(screen, row, 0, screen->max_col + 1);
    } else {
	int top;

	if ((top = screen->cur_row - screen->topline) <= screen->max_row) {
	    if (screen->scroll_amt)
		FlushScroll(screen);
	    if (++top <= screen->max_row) {
		ClearCurBackground(screen,
				   top * FontHeight(screen) + screen->border,
				   OriginX(screen),
				   (screen->max_row - top + 1) * FontHeight(screen),
				   Width(screen));
	    }
	}
	ClearBufRows(screen, screen->cur_row + 1, screen->max_row);
    }
}

/*
 * Clear the given row, for the given range of columns, returning 1 if no
 * protected characters were found, 0 otherwise.
 */
static int
ClearInLine(TScreen * screen, int row, int col, int len)
{
    int rc = 1;
    int flags = TERM_COLOR_FLAGS;

    /*
     * If we're clearing to the end of the line, we won't count this as
     * "drawn" characters.  We'll only do cut/paste on "drawn" characters,
     * so this has the effect of suppressing trailing blanks from a
     * selection.
     */
    if (col + len < screen->max_col + 1) {
	flags |= CHARDRAWN;
    } else {
	len = screen->max_col + 1 - col;
    }

    /* If we've marked protected text on the screen, we'll have to
     * check each time we do an erase.
     */
    if (screen->protected_mode != OFF_PROTECT) {
	int n;
	Char *attrs = SCRN_BUF_ATTRS(screen, row) + col;
	int saved_mode = screen->protected_mode;
	Bool done;

	/* disable this branch during recursion */
	screen->protected_mode = OFF_PROTECT;

	do {
	    done = True;
	    for (n = 0; n < len; n++) {
		if (attrs[n] & PROTECTED) {
		    rc = 0;	/* found a protected segment */
		    if (n != 0)
			ClearInLine(screen, row, col, n);
		    while ((n < len)
			   && (attrs[n] & PROTECTED))
			n++;
		    done = False;
		    break;
		}
	    }
	    /* setup for another segment, past the protected text */
	    if (!done) {
		attrs += n;
		col += n;
		len -= n;
	    }
	} while (!done);

	screen->protected_mode = saved_mode;
	if (len <= 0)
	    return 0;
    }
    /* fall through to the final non-protected segment */

    if (screen->cursor_state)
	HideCursor();
    screen->do_wrap = 0;

    if (row - screen->topline <= screen->max_row) {
	if (!AddToRefresh(screen)) {
	    if (screen->scroll_amt)
		FlushScroll(screen);
	    ClearCurBackground(
				  screen,
				  CursorY(screen, row),
				  CurCursorX(screen, row, col),
				  FontHeight(screen),
				  len * CurFontWidth(screen, row));
	}
    }

    memset(SCRN_BUF_CHARS(screen, row) + col, ' ', len);
    memset(SCRN_BUF_ATTRS(screen, row) + col, flags, len);

    if_OPT_EXT_COLORS(screen, {
	memset(SCRN_BUF_FGRND(screen, row) + col, term->sgr_foreground, len);
	memset(SCRN_BUF_BGRND(screen, row) + col, term->cur_background, len);
    });
    if_OPT_ISO_TRADITIONAL_COLORS(screen, {
	memset(SCRN_BUF_COLOR(screen, row) + col, xtermColorPair(), len);
    });
    if_OPT_DEC_CHRSET({
	memset(SCRN_BUF_CSETS(screen, row) + col,
	       curXtermChrSet(screen->cur_row), len);
    });
    if_OPT_WIDE_CHARS(screen, {
	memset(SCRN_BUF_WIDEC(screen, row) + col, 0, len);
	memset(SCRN_BUF_COM1L(screen, row) + col, 0, len);
	memset(SCRN_BUF_COM1H(screen, row) + col, 0, len);
	memset(SCRN_BUF_COM2L(screen, row) + col, 0, len);
	memset(SCRN_BUF_COM2H(screen, row) + col, 0, len);
    });

    return rc;
}

/*
 * Clear the next n characters on the cursor's line, including the cursor's
 * position.
 */
void
ClearRight(TScreen * screen, int n)
{
    int len = (screen->max_col - screen->cur_col + 1);

    if (n < 0)			/* the remainder of the line */
	n = screen->max_col + 1;
    if (n == 0)			/* default for 'ECH' */
	n = 1;

    if (len > n)
	len = n;

    (void) ClearInLine(screen, screen->cur_row, screen->cur_col, len);

    /* with the right part cleared, we can't be wrapping */
    ScrnClrWrapped(screen, screen->cur_row);
}

/*
 * Clear first part of cursor's line, inclusive.
 */
static void
ClearLeft(TScreen * screen)
{
    (void) ClearInLine(screen, screen->cur_row, 0, screen->cur_col + 1);
}

/*
 * Erase the cursor's line.
 */
static void
ClearLine(TScreen * screen)
{
    (void) ClearInLine(screen, screen->cur_row, 0, screen->max_col + 1);
}

void
ClearScreen(TScreen * screen)
{
    int top;

    if (screen->cursor_state)
	HideCursor();
    screen->do_wrap = 0;
    if ((top = -screen->topline) <= screen->max_row) {
	if (screen->scroll_amt)
	    FlushScroll(screen);
	ClearCurBackground(screen,
			   top * FontHeight(screen) + screen->border,
			   OriginX(screen),
			   (screen->max_row - top + 1) * FontHeight(screen),
			   Width(screen));
    }
    ClearBufRows(screen, 0, screen->max_row);
}

/*
 * If we've written protected text DEC-style, and are issuing a non-DEC
 * erase, temporarily reset the protected_mode flag so that the erase will
 * ignore the protected flags.
 */
void
do_erase_line(TScreen * screen, int param, int mode)
{
    int saved_mode = screen->protected_mode;

    if (saved_mode == DEC_PROTECT
	&& saved_mode != mode)
	screen->protected_mode = OFF_PROTECT;

    switch (param) {
    case -1:			/* DEFAULT */
    case 0:
	ClearRight(screen, -1);
	break;
    case 1:
	ClearLeft(screen);
	break;
    case 2:
	ClearLine(screen);
	break;
    }
    screen->protected_mode = saved_mode;
}

/*
 * Just like 'do_erase_line()', except that this intercepts ED controls.  If we
 * clear the whole screen, we'll get the return-value from ClearInLine, and
 * find if there were any protected characters left.  If not, reset the
 * protected mode flag in the screen data (it's slower).
 */
void
do_erase_display(TScreen * screen, int param, int mode)
{
    int saved_mode = screen->protected_mode;

    if (saved_mode == DEC_PROTECT
	&& saved_mode != mode)
	screen->protected_mode = OFF_PROTECT;

    switch (param) {
    case -1:			/* DEFAULT */
    case 0:
	if (screen->cur_row == 0
	    && screen->cur_col == 0) {
	    screen->protected_mode = saved_mode;
	    do_erase_display(screen, 2, mode);
	    saved_mode = screen->protected_mode;
	} else
	    ClearBelow(screen);
	break;

    case 1:
	if (screen->cur_row == screen->max_row
	    && screen->cur_col == screen->max_col) {
	    screen->protected_mode = saved_mode;
	    do_erase_display(screen, 2, mode);
	    saved_mode = screen->protected_mode;
	} else
	    ClearAbove(screen);
	break;

    case 2:
	/*
	 * We use 'ClearScreen()' throughout the remainder of the
	 * program for places where we don't care if the characters are
	 * protected or not.  So we modify the logic around this call
	 * on 'ClearScreen()' to handle protected characters.
	 */
	if (screen->protected_mode != OFF_PROTECT) {
	    int row;
	    int rc = 1;
	    for (row = 0; row <= screen->max_row; row++)
		rc &= ClearInLine(screen, row, 0, screen->max_col + 1);
	    if (rc != 0)
		saved_mode = OFF_PROTECT;
	} else {
	    ClearScreen(screen);
	}
	break;

    case 3:
	/* xterm addition - erase saved lines. */
	screen->savedlines = 0;
	ScrollBarDrawThumb(screen->scrollWidget);
	break;
    }
    screen->protected_mode = saved_mode;
}

static void
CopyWait(TScreen * screen)
{
    XEvent reply;
    XEvent *rep = &reply;

    while (1) {
	XWindowEvent(screen->display, VWindow(screen),
		     ExposureMask, &reply);
	switch (reply.type) {
	case Expose:
	    HandleExposure(screen, &reply);
	    break;
	case NoExpose:
	case GraphicsExpose:
	    if (screen->incopy <= 0) {
		screen->incopy = 1;
		if (screen->scrolls > 0)
		    screen->scrolls--;
	    }
	    if (reply.type == GraphicsExpose)
		HandleExposure(screen, &reply);

	    if ((reply.type == NoExpose) ||
		((XExposeEvent *) rep)->count == 0) {
		if (screen->incopy <= 0 && screen->scrolls > 0)
		    screen->scrolls--;
		if (screen->scrolls == 0) {
		    screen->incopy = 0;
		    return;
		}
		screen->incopy = -1;
	    }
	    break;
	}
    }
}

/*
 * used by vertical_copy_area and and horizontal_copy_area
 */
static void
copy_area(TScreen * screen,
	  int src_x,
	  int src_y,
	  unsigned width,
	  unsigned height,
	  int dest_x,
	  int dest_y)
{
    /* wait for previous CopyArea to complete unless
       multiscroll is enabled and active */
    if (screen->incopy && screen->scrolls == 0)
	CopyWait(screen);
    screen->incopy = -1;

    /* save for translating Expose events */
    screen->copy_src_x = src_x;
    screen->copy_src_y = src_y;
    screen->copy_width = width;
    screen->copy_height = height;
    screen->copy_dest_x = dest_x;
    screen->copy_dest_y = dest_y;

    XCopyArea(screen->display,
	      VWindow(screen), VWindow(screen),
	      NormalGC(screen),
	      src_x, src_y, width, height, dest_x, dest_y);
}

/*
 * use when inserting or deleting characters on the current line
 */
static void
horizontal_copy_area(TScreen * screen,
		     int firstchar,	/* char pos on screen to start copying at */
		     int nchars,
		     int amount)	/* number of characters to move right */
{
    int src_x = CurCursorX(screen, screen->cur_row, firstchar);
    int src_y = CursorY(screen, screen->cur_row);

    copy_area(screen, src_x, src_y,
	      (unsigned) nchars * CurFontWidth(screen, screen->cur_row),
	      FontHeight(screen),
	      src_x + amount * CurFontWidth(screen, screen->cur_row), src_y);
}

/*
 * use when inserting or deleting lines from the screen
 */
static void
vertical_copy_area(TScreen * screen,
		   int firstline,	/* line on screen to start copying at */
		   int nlines,
		   int amount)	/* number of lines to move up (neg=down) */
{
    if (nlines > 0) {
	int src_x = OriginX(screen);
	int src_y = firstline * FontHeight(screen) + screen->border;

	copy_area(screen, src_x, src_y,
		  (unsigned) Width(screen), nlines * FontHeight(screen),
		  src_x, src_y - amount * FontHeight(screen));
    }
}

/*
 * use when scrolling the entire screen
 */
void
scrolling_copy_area(TScreen * screen,
		    int firstline,	/* line on screen to start copying at */
		    int nlines,
		    int amount)	/* number of lines to move up (neg=down) */
{

    if (nlines > 0) {
	vertical_copy_area(screen, firstline, nlines, amount);
    }
}

/*
 * Handler for Expose events on the VT widget.
 * Returns 1 iff the area where the cursor was got refreshed.
 */
int
HandleExposure(TScreen * screen, XEvent * event)
{
    XExposeEvent *reply = (XExposeEvent *) event;

#ifndef NO_ACTIVE_ICON
    if (reply->window == screen->iconVwin.window)
	screen->whichVwin = &screen->iconVwin;
    else
	screen->whichVwin = &screen->fullVwin;
#endif /* NO_ACTIVE_ICON */

    /* if not doing CopyArea or if this is a GraphicsExpose, don't translate */
    if (!screen->incopy || event->type != Expose)
	return handle_translated_exposure(screen, reply->x, reply->y,
					  reply->width, reply->height);
    else {
	/* compute intersection of area being copied with
	   area being exposed. */
	int both_x1 = Max(screen->copy_src_x, reply->x);
	int both_y1 = Max(screen->copy_src_y, reply->y);
	int both_x2 = Min(screen->copy_src_x + screen->copy_width,
			  (unsigned) (reply->x + reply->width));
	int both_y2 = Min(screen->copy_src_y + screen->copy_height,
			  (unsigned) (reply->y + reply->height));
	int value = 0;

	/* was anything copied affected? */
	if (both_x2 > both_x1 && both_y2 > both_y1) {
	    /* do the copied area */
	    value = handle_translated_exposure
		(screen, reply->x + screen->copy_dest_x - screen->copy_src_x,
		 reply->y + screen->copy_dest_y - screen->copy_src_y,
		 reply->width, reply->height);
	}
	/* was anything not copied affected? */
	if (reply->x < both_x1 || reply->y < both_y1
	    || reply->x + reply->width > both_x2
	    || reply->y + reply->height > both_y2)
	    value = handle_translated_exposure(screen, reply->x, reply->y,
					       reply->width, reply->height);

	return value;
    }
}

/*
 * Called by the ExposeHandler to do the actual repaint after the coordinates
 * have been translated to allow for any CopyArea in progress.
 * The rectangle passed in is pixel coordinates.
 */
static int
handle_translated_exposure(TScreen * screen,
			   int rect_x,
			   int rect_y,
			   unsigned rect_width,
			   unsigned rect_height)
{
    int toprow, leftcol, nrows, ncols;

    TRACE(("handle_translated_exposure (%d,%d) - (%d,%d)\n",
	   rect_y, rect_x, rect_height, rect_width));

    toprow = (rect_y - screen->border) / FontHeight(screen);
    if (toprow < 0)
	toprow = 0;
    leftcol = (rect_x - OriginX(screen))
	/ CurFontWidth(screen, screen->cur_row);
    if (leftcol < 0)
	leftcol = 0;
    nrows = (rect_y + rect_height - 1 - screen->border) /
	FontHeight(screen) - toprow + 1;
    ncols = (rect_x + rect_width - 1 - OriginX(screen)) /
	FontWidth(screen) - leftcol + 1;
    toprow -= screen->scrolls;
    if (toprow < 0) {
	nrows += toprow;
	toprow = 0;
    }
    if (toprow + nrows - 1 > screen->max_row)
	nrows = screen->max_row - toprow + 1;
    if (leftcol + ncols - 1 > screen->max_col)
	ncols = screen->max_col - leftcol + 1;

    if (nrows > 0 && ncols > 0) {
	ScrnRefresh(screen, toprow, leftcol, nrows, ncols, False);
	if (waiting_for_initial_map) {
	    first_map_occurred();
	}
	if (screen->cur_row >= toprow &&
	    screen->cur_row < toprow + nrows &&
	    screen->cur_col >= leftcol &&
	    screen->cur_col < leftcol + ncols)
	    return (1);

    }
    return (0);
}

/***====================================================================***/

void
GetColors(XtermWidget tw, ScrnColors * pColors)
{
    TScreen *screen = &tw->screen;

    pColors->which = 0;
    SET_COLOR_VALUE(pColors, TEXT_FG, screen->foreground);
    SET_COLOR_VALUE(pColors, TEXT_BG, tw->core.background_pixel);
    SET_COLOR_VALUE(pColors, TEXT_CURSOR, screen->cursorcolor);
    SET_COLOR_VALUE(pColors, MOUSE_FG, screen->mousecolor);
    SET_COLOR_VALUE(pColors, MOUSE_BG, screen->mousecolorback);
#if OPT_HIGHLIGHT_COLOR
    SET_COLOR_VALUE(pColors, HIGHLIGHT_BG, screen->highlightcolor);
#endif
#if OPT_TEK4014
    SET_COLOR_VALUE(pColors, TEK_FG, screen->Tforeground);
    SET_COLOR_VALUE(pColors, TEK_BG, screen->Tbackground);
#endif
}

void
ChangeColors(XtermWidget tw, ScrnColors * pNew)
{
    TScreen *screen = &tw->screen;
#if OPT_TEK4014
    Window tek = TWindow(screen);
#endif

    if (COLOR_DEFINED(pNew, TEXT_BG)) {
	tw->core.background_pixel = COLOR_VALUE(pNew, TEXT_BG);
    }

    if (COLOR_DEFINED(pNew, TEXT_CURSOR)) {
	screen->cursorcolor = COLOR_VALUE(pNew, TEXT_CURSOR);
    } else if ((screen->cursorcolor == screen->foreground) &&
	       (COLOR_DEFINED(pNew, TEXT_FG))) {
	screen->cursorcolor = COLOR_VALUE(pNew, TEXT_FG);
    }

    if (COLOR_DEFINED(pNew, TEXT_FG)) {
	Pixel fg = COLOR_VALUE(pNew, TEXT_FG);
	screen->foreground = fg;
	XSetForeground(screen->display, NormalGC(screen), fg);
	XSetBackground(screen->display, ReverseGC(screen), fg);
	XSetForeground(screen->display, NormalBoldGC(screen), fg);
	XSetBackground(screen->display, ReverseBoldGC(screen), fg);
    }

    if (COLOR_DEFINED(pNew, TEXT_BG)) {
	Pixel bg = COLOR_VALUE(pNew, TEXT_BG);
	tw->core.background_pixel = bg;
	XSetBackground(screen->display, NormalGC(screen), bg);
	XSetForeground(screen->display, ReverseGC(screen), bg);
	XSetBackground(screen->display, NormalBoldGC(screen), bg);
	XSetForeground(screen->display, ReverseBoldGC(screen), bg);
	XSetWindowBackground(screen->display, VWindow(screen),
			     tw->core.background_pixel);
    }

    if (COLOR_DEFINED(pNew, MOUSE_FG) || (COLOR_DEFINED(pNew, MOUSE_BG))) {
	if (COLOR_DEFINED(pNew, MOUSE_FG))
	    screen->mousecolor = COLOR_VALUE(pNew, MOUSE_FG);
	if (COLOR_DEFINED(pNew, MOUSE_BG))
	    screen->mousecolorback = COLOR_VALUE(pNew, MOUSE_BG);

	recolor_cursor(screen->pointer_cursor,
		       screen->mousecolor, screen->mousecolorback);
	recolor_cursor(screen->arrow,
		       screen->mousecolor, screen->mousecolorback);
	XDefineCursor(screen->display, VWindow(screen),
		      screen->pointer_cursor);

#if OPT_HIGHLIGHT_COLOR
	if (COLOR_DEFINED(pNew, HIGHLIGHT_BG)) {
	    screen->highlightcolor = COLOR_VALUE(pNew, HIGHLIGHT_BG);
	}
#endif

#if OPT_TEK4014
	if (tek)
	    XDefineCursor(screen->display, tek, screen->arrow);
#endif
    }
#if OPT_TEK4014
    if ((tek) && (COLOR_DEFINED(pNew, TEK_FG) || COLOR_DEFINED(pNew, TEK_BG))) {
	ChangeTekColors(screen, pNew);
    }
#endif
    set_cursor_gcs(screen);
    XClearWindow(screen->display, VWindow(screen));
    ScrnRefresh(screen, 0, 0, screen->max_row + 1,
		screen->max_col + 1, False);
#if OPT_TEK4014
    if (screen->Tshow) {
	XClearWindow(screen->display, tek);
	TekExpose((Widget) NULL, (XEvent *) NULL, (Region) NULL);
    }
#endif
}

void
ChangeAnsiColors(XtermWidget tw)
{
    TScreen *screen = &tw->screen;

    XClearWindow(screen->display, VWindow(screen));
    ScrnRefresh(screen, 0, 0,
		screen->max_row + 1,
		screen->max_col + 1, False);
}

/***====================================================================***/

void
ReverseVideo(XtermWidget termw)
{
    TScreen *screen = &termw->screen;
    GC tmpGC;
    Pixel tmp;
#if OPT_TEK4014
    Window tek = TWindow(screen);
#endif

    /*
     * Swap SGR foreground and background colors.  By convention, these are
     * the colors assigned to "black" (SGR #0) and "white" (SGR #7).  Also,
     * SGR #8 and SGR #15 are the bold (or bright) versions of SGR #0 and
     * #7, respectively.
     *
     * We don't swap colors that happen to match the screen's foreground
     * and background because that tends to produce bizarre effects.
     */
    if_OPT_ISO_COLORS(screen, {
	ColorRes tmp2;
	EXCHANGE(screen->Acolors[0], screen->Acolors[7], tmp2)
	    EXCHANGE(screen->Acolors[8], screen->Acolors[15], tmp2)
    });

    tmp = termw->core.background_pixel;
    if (screen->cursorcolor == screen->foreground)
	screen->cursorcolor = tmp;
    termw->core.background_pixel = screen->foreground;
    screen->foreground = tmp;

    EXCHANGE(screen->mousecolor, screen->mousecolorback, tmp)
	EXCHANGE(NormalGC(screen), ReverseGC(screen), tmpGC)
	EXCHANGE(NormalBoldGC(screen), ReverseBoldGC(screen), tmpGC)
#ifndef NO_ACTIVE_ICON
	tmpGC = screen->iconVwin.normalGC;
    screen->iconVwin.normalGC = screen->iconVwin.reverseGC;
    screen->iconVwin.reverseGC = tmpGC;

    tmpGC = screen->iconVwin.normalboldGC;
    screen->iconVwin.normalboldGC = screen->iconVwin.reverseboldGC;
    screen->iconVwin.reverseboldGC = tmpGC;
#endif /* NO_ACTIVE_ICON */

    recolor_cursor(screen->pointer_cursor,
		   screen->mousecolor, screen->mousecolorback);
    recolor_cursor(screen->arrow,
		   screen->mousecolor, screen->mousecolorback);

    termw->misc.re_verse = !termw->misc.re_verse;

    XDefineCursor(screen->display, VWindow(screen), screen->pointer_cursor);
#if OPT_TEK4014
    if (tek)
	XDefineCursor(screen->display, tek, screen->arrow);
#endif

    if (screen->scrollWidget)
	ScrollBarReverseVideo(screen->scrollWidget);

    XSetWindowBackground(screen->display, VWindow(screen), termw->core.background_pixel);

    /* the shell-window's background will be used in the first repainting
     * on resizing
     */
    XSetWindowBackground(screen->display, VShellWindow, termw->core.background_pixel);

#if OPT_TEK4014
    if (tek) {
	TekReverseVideo(screen);
    }
#endif
    XClearWindow(screen->display, VWindow(screen));
    ScrnRefresh(screen, 0, 0, screen->max_row + 1,
		screen->max_col + 1, False);
#if OPT_TEK4014
    if (screen->Tshow) {
	XClearWindow(screen->display, tek);
	TekExpose((Widget) NULL, (XEvent *) NULL, (Region) NULL);
    }
#endif
    ReverseOldColors();
    update_reversevideo();
}

void
recolor_cursor(Cursor cursor,	/* X cursor ID to set */
	       unsigned long fg,	/* pixel indexes to look up */
	       unsigned long bg)	/* pixel indexes to look up */
{
    TScreen *screen = &term->screen;
    Display *dpy = screen->display;
    XColor colordefs[2];	/* 0 is foreground, 1 is background */

    colordefs[0].pixel = fg;
    colordefs[1].pixel = bg;
    XQueryColors(dpy, DefaultColormap(dpy, DefaultScreen(dpy)),
		 colordefs, 2);
    XRecolorCursor(dpy, cursor, colordefs, colordefs + 1);
    return;
}

/*
 * Set the fnt_wide/fnt_high values to a known state, based on the currently
 * active font.
 */
#ifndef NO_ACTIVE_ICON
#define SAVE_FONT_INFO(screen) xtermSaveFontInfo (screen, IsIcon(screen) ? screen->fnt_icon : screen->fnt_norm)
#else
#define SAVE_FONT_INFO(screen) xtermSaveFontInfo (screen, screen->fnt_norm)
#endif

#ifdef XRENDERFONT
static XftColor *
getColor(Pixel pixel)
{
#define CACHE_SIZE  4
    static struct {
	XftColor color;
	int use;
    } cache[CACHE_SIZE];
    static int use;
    int i;
    int oldest, oldestuse;
    XColor color;

    oldestuse = 0x7fffffff;
    oldest = 0;
    for (i = 0; i < CACHE_SIZE; i++) {
	if (cache[i].use) {
	    if (cache[i].color.pixel == pixel) {
		cache[i].use = ++use;
		return &cache[i].color;
	    }
	}
	if (cache[i].use < oldestuse) {
	    oldestuse = cache[i].use;
	    oldest = i;
	}
    }
    i = oldest;
    color.pixel = pixel;
    XQueryColor(term->screen.display, term->core.colormap, &color);
    cache[i].color.color.red = color.red;
    cache[i].color.color.green = color.green;
    cache[i].color.color.blue = color.blue;
    cache[i].color.color.alpha = 0xffff;
    cache[i].color.pixel = pixel;
    cache[i].use = ++use;
    return &cache[i].color;
}
#endif

/*
 * Draws text with the specified combination of bold/underline
 */
int
drawXtermText(TScreen * screen,
	      unsigned flags,
	      GC gc,
	      int x,
	      int y,
	      int chrset,
	      PAIRED_CHARS(Char * text, Char * text2),
	      Cardinal len,
	      int on_wide)
{
    int real_length = len;
    int draw_len;

#ifdef XRENDERFONT
    if (screen->renderFont) {
	Display *dpy = screen->display;
	XftFont *font;
	XGCValues values;

	if (!screen->renderDraw) {
	    int scr;
	    Drawable draw = VWindow(screen);
	    Visual *visual;

	    scr = DefaultScreen(dpy);
	    visual = DefaultVisual(dpy, scr);
	    screen->renderDraw = XftDrawCreate(dpy, draw, visual,
					       DefaultColormap(dpy, scr));
	}
	if ((flags & (BOLD | BLINK)) && screen->renderFontBold)
	    font = screen->renderFontBold;
	else
	    font = screen->renderFont;
	XGetGCValues(dpy, gc, GCForeground | GCBackground, &values);
	XftDrawRect(screen->renderDraw,
		    getColor(values.background),
		    x, y,
		    len * FontWidth(screen), FontHeight(screen));

	y += font->ascent;
#if OPT_WIDE_CHARS
	if (text2) {
	    static XftChar16 *sbuf;
	    static unsigned slen;
	    unsigned n;

	    if (slen < len) {
		slen = (len + 1) * 2;
		sbuf = (XftChar16 *) XtRealloc((char *) sbuf, slen * sizeof(XftChar16));
	    }
	    for (n = 0; n < len; n++)
		sbuf[n] = *text++ | (*text2++ << 8);
	    XftDrawString16(screen->renderDraw,
			    getColor(values.foreground),
			    font,
			    x, y, sbuf, len);
	} else
#endif
	{
	    XftDrawString8(screen->renderDraw,
			   getColor(values.foreground),
			   font,
			   x, y, (unsigned char *) text, len);
	}

	return x + len * FontWidth(screen);
    }
#endif
#if OPT_WIDE_CHARS
    /*
     * It's simpler to pass in a null pointer for text2 in places where
     * we only use codes through 255.  Fix text2 here so we can increment
     * it, etc.
     */
    if (text2 == 0) {
	static Char *dbuf;
	static unsigned dlen;
	if (dlen <= len) {
	    dlen = (len + 1) * 2;
	    dbuf = (Char *) XtRealloc((char *) dbuf, dlen);
	    memset(dbuf, 0, dlen);
	}
	text2 = dbuf;
    }
#endif
#if OPT_DEC_CHRSET
    if (CSET_DOUBLE(chrset)) {
	/* We could try drawing double-size characters in the icon, but
	 * given that the icon font is usually nil or nil2, there
	 * doesn't seem to be much point.
	 */
	GC gc2 = ((!IsIcon(screen) && screen->font_doublesize)
		  ? xterm_DoubleGC(chrset, flags, gc)
		  : 0);

	TRACE(("DRAWTEXT%c[%4d,%4d] (%d) %d:%.*s\n",
	       screen->cursor_state == OFF ? ' ' : '*',
	       y, x, chrset, len, (int) len, text));

	if (gc2 != 0) {		/* draw actual double-sized characters */
	    XFontStruct *fs =
	    screen->double_fonts[xterm_Double_index(chrset, flags)].fs;
	    XRectangle rect, *rp = &rect;
	    Cardinal nr = 1;
	    int adjust;

	    SAVE_FONT_INFO(screen);
	    screen->fnt_wide *= 2;

	    rect.x = 0;
	    rect.y = 0;
	    rect.width = len * screen->fnt_wide;
	    rect.height = FontHeight(screen);

	    switch (chrset) {
	    case CSET_DHL_TOP:
		rect.y = -(rect.height / 2);
		y -= rect.y;
		screen->fnt_high *= 2;
		break;
	    case CSET_DHL_BOT:
		rect.y = (rect.height / 2);
		y -= rect.y;
		screen->fnt_high *= 2;
		break;
	    default:
		nr = 0;
		break;
	    }

	    /*
	     * Though it is the right "size", a given bold font may
	     * be shifted up by a pixel or two.  Shift it back into
	     * the clipping rectangle.
	     */
	    if (nr != 0) {
		adjust = fs->ascent
		    + fs->descent
		    - (2 * FontHeight(screen));
		rect.y -= adjust;
		y += adjust;
	    }

	    if (nr)
		XSetClipRectangles(screen->display, gc2,
				   x, y, rp, nr, YXBanded);
	    else
		XSetClipMask(screen->display, gc2, None);

	    /*
	     * If we're trying to use proportional font, or if the
	     * font server didn't give us what we asked for wrt
	     * width, position each character independently.
	     */
	    if (screen->fnt_prop
		|| (fs->min_bounds.width != fs->max_bounds.width)
		|| (fs->min_bounds.width != 2 * FontWidth(screen))) {
		while (len--) {
		    x = drawXtermText(screen, flags, gc2,
				      x, y, 0,
				      PAIRED_CHARS(text++, text2++),
				      1, on_wide);
		    x += FontWidth(screen);
		}
	    } else {
		x = drawXtermText(screen, flags, gc2,
				  x, y, 0,
				  PAIRED_CHARS(text, text2),
				  len, on_wide);
		x += len * FontWidth(screen);
	    }

	    TRACE(("drewtext [%4d,%4d]\n", y, x));
	    SAVE_FONT_INFO(screen);

	} else {		/* simulate double-sized characters */
#if OPT_WIDE_CHARS
	    Char *wide = 0;
#endif
	    unsigned need = 2 * len;
	    Char *temp = (Char *) malloc(need);
	    int n = 0;
	    if_OPT_WIDE_CHARS(screen, {
		wide = (Char *) malloc(need);
	    });
	    while (len--) {
		if_OPT_WIDE_CHARS(screen, {
		    wide[n] = *text2++;
		    wide[n + 1] = 0;
		});
		temp[n++] = *text++;
		temp[n++] = ' ';
	    }
	    x = drawXtermText(screen,
			      flags,
			      gc,
			      x, y,
			      0,
			      PAIRED_CHARS(temp, wide),
			      n,
			      on_wide);
	    free(temp);
	    if_OPT_WIDE_CHARS(screen, {
		free(wide);
	    });
	}
	return x;
    }
#endif
    /*
     * If we're asked to display a proportional font, do this with a fixed
     * pitch.  Yes, it's ugly.  But we cannot distinguish the use of xterm
     * as a dumb terminal vs its use as in fullscreen programs such as vi.
     */
    if (screen->fnt_prop) {
	int adj, width;
	GC fillGC = gc;		/* might be cursorGC */
	XFontStruct *fs = (flags & (BOLD | BLINK))
	? screen->fnt_bold
	: screen->fnt_norm;
	screen->fnt_prop = False;

#define GC_PAIRS(a,b) \
	if (gc == a) fillGC = b; \
	if (gc == b) fillGC = a

	/*
	 * Fill the area where we'll write the characters, otherwise
	 * we'll get gaps between them.  The cursor is a special case,
	 * because the XFillRectangle call only uses the foreground,
	 * while we've set the cursor color in the background.  So we
	 * need a special GC for that.
	 */
	if (gc == screen->cursorGC
	    || gc == screen->reversecursorGC)
	    fillGC = screen->fillCursorGC;
	GC_PAIRS(NormalGC(screen), ReverseGC(screen));
	GC_PAIRS(NormalBoldGC(screen), ReverseBoldGC(screen));

	XFillRectangle(screen->display, VWindow(screen), fillGC,
		       x, y, len * FontWidth(screen), FontHeight(screen));

	while (len--) {
	    width = XTextWidth(fs, (char *) text, 1);
	    adj = (FontWidth(screen) - width) / 2;
	    (void) drawXtermText(screen, flags, gc, x + adj, y,
				 chrset,
				 PAIRED_CHARS(text++, text2++), 1, on_wide);
	    x += FontWidth(screen);
	}
	screen->fnt_prop = True;
	return x;
    }

    /* If the font is complete, draw it as-is */
    if (screen->fnt_boxes && !screen->force_box_chars) {
	TRACE(("drawXtermText%c[%4d,%4d] (%d) %d:%s\n",
	       screen->cursor_state == OFF ? ' ' : '*',
	       y, x, chrset, len,
	       visibleChars(PAIRED_CHARS(text, text2), len)));
	y += FontAscent(screen);

#if OPT_WIDE_CHARS
	if (screen->wide_chars) {
	    int ascent_adjust = 0;
	    static XChar2b *sbuf;
	    static Cardinal slen;
	    Cardinal n;
	    int ch = text[0] | (text2[0] << 8);
	    int wideness = (on_wide || iswide(ch) != 0)
	    && (screen->fnt_dwd != NULL);
	    unsigned char *endtext = text + len;
	    if (slen < len) {
		slen = (len + 1) * 2;
		sbuf = (XChar2b *) XtRealloc((char *) sbuf, slen * sizeof(*sbuf));
	    }
	    for (n = 0; n < len; n++) {
		sbuf[n].byte2 = *text;
		sbuf[n].byte1 = *text2;
		text++;
		text2++;
		if (wideness) {
		    /* filter out those pesky fake characters. */
		    while (text < endtext
			   && *text == HIDDEN_HI
			   && *text2 == HIDDEN_LO) {
			text++;
			text2++;
			len--;
		    }
		}
	    }
	    /* This is probably wrong. But it works. */
	    draw_len = len;
	    if (wideness
		&& (screen->fnt_dwd->fid || screen->fnt_dwdb->fid)) {
		draw_len = real_length = len * 2;
		if ((flags & (BOLD | BLINK)) != 0
		    && screen->fnt_dwdb->fid) {
		    XSetFont(screen->display, gc, screen->fnt_dwdb->fid);
		    ascent_adjust = screen->fnt_dwdb->ascent - screen->fnt_norm->ascent;
		} else {
		    XSetFont(screen->display, gc, screen->fnt_dwd->fid);
		    ascent_adjust = screen->fnt_dwd->ascent - screen->fnt_norm->ascent;
		}
		/* fix ascent */
	    } else if ((flags & (BOLD | BLINK)) != 0
		       && screen->fnt_bold->fid)
		XSetFont(screen->display, gc, screen->fnt_bold->fid);
	    else
		XSetFont(screen->display, gc, screen->fnt_norm->fid);

	    if (my_wcwidth(ch) == 0)
		XDrawString16(screen->display,
			      VWindow(screen), gc,
			      x, y + ascent_adjust,
			      sbuf, n);
	    else
		XDrawImageString16(screen->display,
				   VWindow(screen), gc,
				   x, y + ascent_adjust,
				   sbuf, n);

	} else
#endif
	{
	    XDrawImageString(screen->display, VWindow(screen), gc,
			     x, y, (char *) text, len);
	    draw_len = len;
	    if ((flags & (BOLD | BLINK)) && screen->enbolden) {
#if OPT_CLIP_BOLD
		/*
		 * This special case is a couple of percent slower, but
		 * avoids a lot of pixel trash in rxcurses' hanoi.cmd
		 * demo (e.g., 10x20 font).
		 */
		if (screen->fnt_wide > 2) {
		    XRectangle clip;
		    int clip_x = x;
		    int clip_y = y - FontHeight(screen) + FontDescent(screen);
		    clip.x = 0;
		    clip.y = 0;
		    clip.height = FontHeight(screen);
		    clip.width = screen->fnt_wide * len;
		    XSetClipRectangles(screen->display, gc,
				       clip_x, clip_y,
				       &clip, 1, Unsorted);
		}
#endif
		XDrawString(screen->display, VWindow(screen), gc,
			    x + 1, y, (char *) text, len);
#if OPT_CLIP_BOLD
		XSetClipMask(screen->display, gc, None);
#endif
	    }
	}

	if ((flags & UNDERLINE) && screen->underline) {
	    if (FontDescent(screen) > 1)
		y++;
	    XDrawLine(screen->display, VWindow(screen), gc,
		      x, y, x + draw_len * screen->fnt_wide - 1, y);
	}
#if OPT_BOX_CHARS
#define DrawX(col) x + (col * (screen->fnt_wide))
#define DrawSegment(first,last) (void)drawXtermText(screen, flags, gc, DrawX(first), y, chrset, PAIRED_CHARS(text+first, text2+first), last-first, on_wide)
    } else {			/* fill in missing box-characters */
	XFontStruct *font = ((flags & BOLD)
			     ? screen->fnt_bold
			     : screen->fnt_norm);
	Cardinal last, first = 0;
	Boolean save_force = screen->force_box_chars;
	screen->fnt_boxes = True;
	for (last = 0; last < len; last++) {
	    unsigned ch = text[last];
	    Boolean isMissing;
#if OPT_WIDE_CHARS
	    if (text2 != 0)
		ch |= (text2[last] << 8);
	    isMissing = (ch != HIDDEN_CHAR)
		&& (xtermMissingChar(ch,
				     ((on_wide || iswide(ch)) && screen->fnt_dwd)
				     ? screen->fnt_dwd
				     : font));
#else
	    isMissing = xtermMissingChar(ch, font);
#endif
	    if (isMissing) {
		if (last > first) {
		    screen->force_box_chars = False;
		    DrawSegment(first, last);
		    screen->force_box_chars = save_force;
		}
		xtermDrawBoxChar(screen, ch, flags, gc, DrawX(last), y);
		first = last + 1;
	    }
	}
	if (last > first) {
	    screen->force_box_chars = False;
	    DrawSegment(first, last);
	}
	screen->fnt_boxes = False;
	screen->force_box_chars = save_force;
#endif
    }

    return x + real_length * FontWidth(screen);
}

/*
 * Returns a GC, selected according to the font (reverse/bold/normal) that is
 * required for the current position (implied).  The GC is updated with the
 * current screen foreground and background colors.
 */
GC
updatedXtermGC(TScreen * screen, int flags, int fg_bg, Bool hilite)
{
    int my_fg = extract_fg(fg_bg, flags);
    int my_bg = extract_bg(fg_bg, flags);
    Pixel fg_pix = getXtermForeground(flags, my_fg);
    Pixel bg_pix = getXtermBackground(flags, my_bg);
#if OPT_HIGHLIGHT_COLOR
    Pixel hi_pix = screen->highlightcolor;
#endif
    GC gc;

    checkVeryBoldColors(flags, my_fg);

    if (ReverseOrHilite(screen, flags, hilite)) {
	if (flags & (BOLD | BLINK))
	    gc = ReverseBoldGC(screen);
	else
	    gc = ReverseGC(screen);

#if OPT_HIGHLIGHT_COLOR
	if (hi_pix != screen->foreground
	    && hi_pix != fg_pix
	    && hi_pix != bg_pix
	    && hi_pix != term->dft_foreground) {
	    bg_pix = fg_pix;
	    fg_pix = hi_pix;
	}
#endif
	XSetForeground(screen->display, gc, bg_pix);
	XSetBackground(screen->display, gc, fg_pix);
    } else {
	if (flags & (BOLD | BLINK))
	    gc = NormalBoldGC(screen);
	else
	    gc = NormalGC(screen);

	XSetForeground(screen->display, gc, fg_pix);
	XSetBackground(screen->display, gc, bg_pix);
    }
    return gc;
}

/*
 * Resets the foreground/background of the GC returned by 'updatedXtermGC()'
 * to the values that would be set in SGR_Foreground and SGR_Background. This
 * duplicates some logic, but only modifies 1/4 as many GC's.
 */
void
resetXtermGC(TScreen * screen, int flags, Bool hilite)
{
    Pixel fg_pix = getXtermForeground(flags, term->cur_foreground);
    Pixel bg_pix = getXtermBackground(flags, term->cur_background);
    GC gc;

    checkVeryBoldColors(flags, term->cur_foreground);

    if (ReverseOrHilite(screen, flags, hilite)) {
	if (flags & (BOLD | BLINK))
	    gc = ReverseBoldGC(screen);
	else
	    gc = ReverseGC(screen);

	XSetForeground(screen->display, gc, bg_pix);
	XSetBackground(screen->display, gc, fg_pix);

    } else {
	if (flags & (BOLD | BLINK))
	    gc = NormalBoldGC(screen);
	else
	    gc = NormalGC(screen);

	XSetForeground(screen->display, gc, fg_pix);
	XSetBackground(screen->display, gc, bg_pix);
    }
}

#if OPT_ISO_COLORS
/*
 * Extract the foreground-color index from a one-byte color pair.  If we've got
 * BOLD or UNDERLINE color-mode active, those will be used.
 */
int
extract_fg(unsigned color, unsigned flags)
{
    int fg = (int) ExtractForeground(color);

    if (term->screen.colorAttrMode
	|| (fg == (int) ExtractBackground(color))) {
	if (term->screen.colorULMode && (flags & UNDERLINE))
	    fg = COLOR_UL;
	if (term->screen.colorBDMode && (flags & BOLD))
	    fg = COLOR_BD;
	if (term->screen.colorBLMode && (flags & BLINK))
	    fg = COLOR_BL;
    }
    return fg;
}

/*
 * Extract the background-color index from a one-byte color pair.
 * If we've got INVERSE color-mode active, that will be used.
 */
int
extract_bg(unsigned color, unsigned flags)
{
    int bg = (int) ExtractBackground(color);

    if (term->screen.colorAttrMode
	|| (bg == (int) ExtractForeground(color))) {
	if (term->screen.colorRVMode && (flags & INVERSE))
	    bg = COLOR_RV;
    }
    return bg;
}

/*
 * Combine the current foreground and background into a single 8-bit number.
 * Note that we're storing the SGR foreground, since cur_foreground may be set
 * to COLOR_UL, COLOR_BD or COLOR_BL, which would make the code larger than 8
 * bits.
 *
 * This assumes that fg/bg are equal when we override with one of the special
 * attribute colors.
 */
unsigned
makeColorPair(int fg, int bg)
{
    unsigned my_bg = (bg >= 0) && (bg < NUM_ANSI_COLORS) ? (unsigned) bg : 0;
    unsigned my_fg = (fg >= 0) && (fg < NUM_ANSI_COLORS) ? (unsigned) fg : my_bg;
#if OPT_EXT_COLORS
    return (my_fg << 8) | my_bg;
#else
    return (my_fg << 4) | my_bg;
#endif
}

/*
 * Using the "current" SGR background, clear a rectangle.
 */
void
ClearCurBackground(
		      TScreen * screen,
		      int top,
		      int left,
		      unsigned height,
		      unsigned width)
{
    XSetWindowBackground(
			    screen->display,
			    VWindow(screen),
			    getXtermBackground(term->flags, term->cur_background));

    XClearArea(screen->display, VWindow(screen),
	       left, top, width, height, FALSE);

    XSetWindowBackground(
			    screen->display,
			    VWindow(screen),
			    getXtermBackground(term->flags, MAXCOLORS));
}
#endif /* OPT_ISO_COLORS */

#if OPT_WIDE_CHARS
/*
 * Returns a single 8/16-bit number for the given cell
 */
unsigned
getXtermCell(TScreen * screen, int row, int col)
{
    unsigned ch = SCRN_BUF_CHARS(screen, row)[col];
    if_OPT_WIDE_CHARS(screen, {
	ch |= (SCRN_BUF_WIDEC(screen, row)[col] << 8);
    });
    return ch;
}

unsigned
getXtermCellComb1(TScreen * screen, int row, int col)
{
    unsigned ch = SCRN_BUF_COM1L(screen, row)[col];
    ch |= (SCRN_BUF_COM1H(screen, row)[col] << 8);
    return ch;
}

unsigned
getXtermCellComb2(TScreen * screen, int row, int col)
{
    unsigned ch = SCRN_BUF_COM2L(screen, row)[col];
    ch |= (SCRN_BUF_COM2H(screen, row)[col] << 8);
    return ch;
}

/*
 * Sets a single 8/16-bit number for the given cell
 */
void
putXtermCell(TScreen * screen, int row, int col, int ch)
{
    SCRN_BUF_CHARS(screen, row)[col] = ch;
    if_OPT_WIDE_CHARS(screen, {
	SCRN_BUF_WIDEC(screen, row)[col] = (ch >> 8);
	SCRN_BUF_COM1L(screen, row)[col] = 0;
	SCRN_BUF_COM1H(screen, row)[col] = 0;
	SCRN_BUF_COM2L(screen, row)[col] = 0;
	SCRN_BUF_COM2H(screen, row)[col] = 0;
    });
}

/*
 * Add a combining character for the given cell
 */
void
addXtermCombining(TScreen * screen, int row, int col, unsigned ch)
{
    if (!SCRN_BUF_COM1L(screen, row)[col]
	&& !SCRN_BUF_COM1H(screen, row)[col]) {
	SCRN_BUF_COM1L(screen, row)[col] = ch & 0xff;
	SCRN_BUF_COM1H(screen, row)[col] = ch >> 8;
    } else if (!SCRN_BUF_COM2H(screen, row)[col]) {
	SCRN_BUF_COM2L(screen, row)[col] = ch & 0xff;
	SCRN_BUF_COM2H(screen, row)[col] = ch >> 8;
    }
}
#endif

#ifdef HAVE_CONFIG_H
#ifdef USE_MY_MEMMOVE
char *
my_memmove(char *s1, char *s2, size_t n)
{
    if (n != 0) {
	if ((s1 + n > s2) && (s2 + n > s1)) {
	    static char *bfr;
	    static size_t length;
	    size_t j;
	    if (length < n) {
		length = (n * 3) / 2;
		bfr = ((bfr != 0)
		       ? realloc(bfr, length)
		       : malloc(length));
		if (bfr == NULL)
		    SysError(ERROR_MMALLOC);
	    }
	    for (j = 0; j < n; j++)
		bfr[j] = s2[j];
	    s2 = bfr;
	}
	while (n-- != 0)
	    s1[n] = s2[n];
    }
    return s1;
}
#endif /* USE_MY_MEMMOVE */

#ifndef HAVE_STRERROR
char *
my_strerror(int n)
{
    extern char *sys_errlist[];
    extern int sys_nerr;
    if (n > 0 && n < sys_nerr)
	return sys_errlist[n];
    return "?";
}
#endif
#endif

int
char2lower(int ch)
{
    if (isascii(ch) && isupper(ch)) {	/* lowercasify */
#ifdef _tolower
	ch = _tolower(ch);
#else
	ch = tolower(ch);
#endif
    }
    return ch;
}

void
update_keyboard_type(void)
{
    update_delete_del();
    update_old_fkeys();
    update_hp_fkeys();
    update_sco_fkeys();
    update_sun_fkeys();
    update_sun_kbd();
}

void
set_keyboard_type(xtermKeyboardType type, Bool set)
{
    xtermKeyboardType save = term->keyboard.type;

    if (set) {
	term->keyboard.type = type;
    } else {
	term->keyboard.type = keyboardIsDefault;
    }

    if (save != term->keyboard.type) {
	update_keyboard_type();
    }
}

void
toggle_keyboard_type(xtermKeyboardType type)
{
    xtermKeyboardType save = term->keyboard.type;

    if (term->keyboard.type == type) {
	term->keyboard.type = keyboardIsDefault;
    } else {
	term->keyboard.type = type;
    }

    if (save != term->keyboard.type) {
	update_keyboard_type();
    }
}

void
init_keyboard_type(xtermKeyboardType type, Bool set)
{
    static Bool wasSet = False;

    if (set) {
	if (wasSet) {
	    fprintf(stderr, "Conflicting keyboard type option (%d/%d)\n",
		    term->keyboard.type, type);
	}
	term->keyboard.type = type;
	wasSet = True;
    }
}

/* $XTermId: util.c,v 1.255 2005/09/18 23:48:13 tom Exp $ */

/*
 *	$Xorg: util.c,v 1.3 2000/08/17 19:55:10 cpqbld Exp $
 */

/* $XFree86: xc/programs/xterm/util.c,v 3.93 2005/09/18 23:48:13 dickey Exp $ */

/*
 * Copyright 1999-2004,2005 by Thomas E. Dickey
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
#include <xstrings.h>

#if OPT_WIDE_CHARS
#if defined(HAVE_WCHAR_H) && defined(HAVE_WCWIDTH)
#include <wchar.h>
#endif
#include <wcwidth.h>
#endif

#include <stdio.h>
#include <ctype.h>
#include <assert.h>

static int ClearInLine(TScreen * screen, int row, int col, unsigned len);
static int handle_translated_exposure(TScreen * screen,
				      int rect_x,
				      int rect_y,
				      int rect_width,
				      int rect_height);
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

#if OPT_WIDE_CHARS
int (*my_wcwidth) (wchar_t);
#endif

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
    ScrollSelection(screen, -(screen->scroll_amt), False);
    screen->scroll_amt = 0;
    screen->refresh_amt = 0;
    if (refreshheight > 0) {
	ClearCurBackground(screen,
			   (int) refreshtop * FontHeight(screen) + screen->border,
			   (int) OriginX(screen),
			   (unsigned) refreshheight * FontHeight(screen),
			   (unsigned) Width(screen));
	ScrnRefresh(screen, refreshtop, 0, refreshheight,
		    MaxCols(screen), False);
    }
    return;
}

int
AddToRefresh(TScreen * screen)
{
    int amount = screen->refresh_amt;
    int row = screen->cur_row;

    if (amount == 0) {
	return (0);
    } else if (amount > 0) {
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
 * If we're scrolling, leave the selection intact if possible.
 * If it will bump into one of the extremes of the saved-lines, truncate that.
 * If the selection is not contained within the scrolled region, clear it.
 */
static void
adjustHiliteOnFwdScroll(TScreen * screen, int amount, Boolean all_lines)
{
    int lo_row = (all_lines
		  ? (screen->bot_marg - screen->savelines)
		  : screen->top_marg);
    int hi_row = screen->bot_marg;

    TRACE2(("adjustSelection FWD %s by %d (%s)\n",
	    screen->alternate ? "alternate" : "normal",
	    amount,
	    all_lines ? "all" : "visible"));
    TRACE2(("  before highlite %d.%d .. %d.%d\n",
	    screen->startHRow,
	    screen->startHCol,
	    screen->endHRow,
	    screen->endHCol));
    TRACE2(("  margins %d..%d\n", screen->top_marg, screen->bot_marg));
    TRACE2(("  limits  %d..%d\n", lo_row, hi_row));

    if (screen->startHRow >= lo_row
	&& screen->startHRow - amount < lo_row) {
	/* truncate the selection because its start would move out of region */
	if (lo_row + amount <= screen->endHRow) {
	    TRACE2(("truncate selection by changing start %d.%d to %d.%d\n",
		    screen->startHRow,
		    screen->startHCol,
		    lo_row + amount,
		    0));
	    screen->startHRow = lo_row + amount;
	    screen->startHCol = 0;
	} else {
	    TRACE2(("deselect because %d.%d .. %d.%d shifted %d is outside margins %d..%d\n",
		    screen->startHRow,
		    screen->startHCol,
		    screen->endHRow,
		    screen->endHCol,
		    -amount,
		    lo_row,
		    hi_row));
	    ScrnDisownSelection(screen);
	}
    } else if (screen->startHRow <= hi_row && screen->endHRow > hi_row) {
	ScrnDisownSelection(screen);
    } else if (screen->startHRow < lo_row && screen->endHRow > lo_row) {
	ScrnDisownSelection(screen);
    }

    TRACE2(("  after highlite %d.%d .. %d.%d\n",
	    screen->startHRow,
	    screen->startHCol,
	    screen->endHRow,
	    screen->endHCol));
}

/*
 * This is the same as adjustHiliteOnFwdScroll(), but reversed.  In this case,
 * only the visible lines are affected.
 */
static void
adjustHiliteOnBakScroll(TScreen * screen, int amount)
{
    int lo_row = screen->top_marg;
    int hi_row = screen->bot_marg;

    TRACE2(("adjustSelection BAK %s by %d (%s)\n",
	    screen->alternate ? "alternate" : "normal",
	    amount,
	    "visible"));
    TRACE2(("  before highlite %d.%d .. %d.%d\n",
	    screen->startHRow,
	    screen->startHCol,
	    screen->endHRow,
	    screen->endHCol));
    TRACE2(("  margins %d..%d\n", screen->top_marg, screen->bot_marg));

    if (screen->endHRow >= hi_row
	&& screen->endHRow + amount > hi_row) {
	/* truncate the selection because its start would move out of region */
	if (hi_row - amount >= screen->startHRow) {
	    TRACE2(("truncate selection by changing start %d.%d to %d.%d\n",
		    screen->startHRow,
		    screen->startHCol,
		    hi_row - amount,
		    0));
	    screen->endHRow = hi_row - amount;
	    screen->endHCol = 0;
	} else {
	    TRACE2(("deselect because %d.%d .. %d.%d shifted %d is outside margins %d..%d\n",
		    screen->startHRow,
		    screen->startHCol,
		    screen->endHRow,
		    screen->endHCol,
		    amount,
		    lo_row,
		    hi_row));
	    ScrnDisownSelection(screen);
	}
    } else if (screen->endHRow >= lo_row && screen->startHRow < lo_row) {
	ScrnDisownSelection(screen);
    } else if (screen->endHRow > hi_row && screen->startHRow > hi_row) {
	ScrnDisownSelection(screen);
    }

    TRACE2(("  after highlite %d.%d .. %d.%d\n",
	    screen->startHRow,
	    screen->startHCol,
	    screen->endHRow,
	    screen->endHCol));
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
    Boolean scroll_all_lines = (screen->scrollWidget
				&& !screen->alternate
				&& screen->top_marg == 0);

    TRACE(("xtermScroll count=%d\n", amount));

    screen->cursor_busy += 1;
    screen->cursor_moved = True;

    if (screen->cursor_state)
	HideCursor();

    if (amount > i)
	amount = i;

    if (ScrnHaveSelection(screen))
	adjustHiliteOnFwdScroll(screen, amount, scroll_all_lines);

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
	ScrollSelection(screen, -(amount), False);
	if (amount == i) {
	    ClearScreen(screen);
	    screen->cursor_busy -= 1;
	    return;
	}

	shift = -screen->topline;
	bot = screen->max_row - shift;
	scrollheight = i - amount;
	refreshheight = amount;

	if ((refreshtop = screen->bot_marg - refreshheight + 1 + shift) >
	    (i = screen->max_row - refreshheight + 1))
	    refreshtop = i;

	if (scroll_all_lines) {
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

    if (amount > 0) {
	if (scroll_all_lines) {
	    ScrnDeleteLine(screen,
			   screen->allbuf,
			   screen->bot_marg + screen->savelines,
			   0,
			   (unsigned) amount,
			   (unsigned) MaxCols(screen));
	} else {
	    ScrnDeleteLine(screen,
			   screen->visbuf,
			   screen->bot_marg,
			   screen->top_marg,
			   (unsigned) amount,
			   (unsigned) MaxCols(screen));
	}
    }

    if (refreshheight > 0) {
	ScrnRefresh(screen, refreshtop, 0, refreshheight,
		    MaxCols(screen), False);
    }

    screen->cursor_busy -= 1;
    return;
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

    TRACE(("RevScroll count=%d\n", amount));

    screen->cursor_busy += 1;
    screen->cursor_moved = True;

    if (screen->cursor_state)
	HideCursor();

    if (amount > i)
	amount = i;

    if (ScrnHaveSelection(screen))
	adjustHiliteOnBakScroll(screen, amount);

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
    if (amount > 0) {
	ScrnInsertLine(screen,
		       screen->visbuf,
		       screen->bot_marg,
		       screen->top_marg,
		       (unsigned) amount,
		       (unsigned) MaxCols(screen));
    }
    screen->cursor_busy -= 1;
    return;
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

    if (!ScrnIsLineInMargins(screen, screen->cur_row - screen->topline))
	return;

    TRACE(("InsertLine count=%d\n", n));

    if (screen->cursor_state)
	HideCursor();

    if (ScrnHaveSelection(screen)
	&& ScrnAreLinesInSelection(screen, screen->top_marg, screen->bot_marg)) {
	ScrnDisownSelection(screen);
    }

    screen->do_wrap = 0;
    if (n > (i = screen->bot_marg - screen->cur_row + 1))
	n = i;
    if (screen->jumpscroll) {
	if (screen->scroll_amt <= 0 &&
	    screen->cur_row <= -screen->refresh_amt) {
	    if (-screen->refresh_amt + n > MaxRows(screen))
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
    if (n > 0) {
	ScrnInsertLine(screen,
		       screen->visbuf,
		       screen->bot_marg,
		       screen->cur_row,
		       (unsigned) n,
		       (unsigned) MaxCols(screen));
    }
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

    if (!ScrnIsLineInMargins(screen, screen->cur_row - screen->topline))
	return;

    TRACE(("DeleteLine count=%d\n", n));

    if (screen->cursor_state)
	HideCursor();

    if (ScrnHaveSelection(screen)
	&& ScrnAreLinesInSelection(screen, screen->top_marg, screen->bot_marg)) {
	ScrnDisownSelection(screen);
    }

    screen->do_wrap = 0;
    if (n > (i = screen->bot_marg - screen->cur_row + 1))
	n = i;
    if (screen->jumpscroll) {
	if (screen->scroll_amt >= 0 && screen->cur_row == screen->top_marg) {
	    if (screen->refresh_amt + n > MaxRows(screen))
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
    if (n > 0) {
	if (screen->scrollWidget
	    && !screen->alternate
	    && screen->cur_row == 0)
	    ScrnDeleteLine(screen,
			   screen->allbuf,
			   screen->bot_marg + screen->savelines,
			   0,
			   (unsigned) n,
			   (unsigned) MaxCols(screen));
	else
	    ScrnDeleteLine(screen,
			   screen->visbuf,
			   screen->bot_marg,
			   screen->cur_row,
			   (unsigned) n,
			   (unsigned) MaxCols(screen));
    }
}

/*
 * Insert n blanks at the cursor's position, no wraparound
 */
void
InsertChar(TScreen * screen, unsigned n)
{
    unsigned limit;
    int row = screen->cur_row - screen->topline;

    if (screen->cursor_state)
	HideCursor();

    TRACE(("InsertChar count=%d\n", n));

    if (ScrnHaveSelection(screen)
	&& ScrnIsLineInSelection(screen, row)) {
	ScrnDisownSelection(screen);
    }
    screen->do_wrap = 0;

    assert(screen->cur_col <= screen->max_col);
    limit = MaxCols(screen) - screen->cur_col;

    if (n > limit)
	n = limit;

    assert(n != 0);
    if (row <= screen->max_row) {
	if (!AddToRefresh(screen)) {
	    int col = MaxCols(screen) - n;
	    if (screen->scroll_amt)
		FlushScroll(screen);

#if OPT_DEC_CHRSET
	    if (CSET_DOUBLE(SCRN_BUF_CSETS(screen, screen->cur_row)[0])) {
		col = MaxCols(screen) / 2 - n;
	    }
#endif
	    /*
	     * prevent InsertChar from shifting the end of a line over
	     * if it is being appended to
	     */
	    if (non_blank_line(screen->visbuf, screen->cur_row,
			       screen->cur_col, MaxCols(screen)))
		horizontal_copy_area(screen, screen->cur_col,
				     col - screen->cur_col,
				     (int) n);

	    ClearCurBackground(screen,
			       CursorY(screen, screen->cur_row),
			       CurCursorX(screen, screen->cur_row, screen->cur_col),
			       (unsigned) FontHeight(screen),
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
DeleteChar(TScreen * screen, unsigned n)
{
    unsigned limit;
    int row = screen->cur_row - screen->topline;

    if (screen->cursor_state)
	HideCursor();

    TRACE(("DeleteChar count=%d\n", n));

    if (ScrnHaveSelection(screen)
	&& ScrnIsLineInSelection(screen, row)) {
	ScrnDisownSelection(screen);
    }
    screen->do_wrap = 0;

    assert(screen->cur_col <= screen->max_col);
    limit = MaxCols(screen) - screen->cur_col;

    if (n > limit)
	n = limit;

    assert(n != 0);
    if (row <= screen->max_row) {
	if (!AddToRefresh(screen)) {
	    int col = MaxCols(screen) - n;
	    if (screen->scroll_amt)
		FlushScroll(screen);

#if OPT_DEC_CHRSET
	    if (CSET_DOUBLE(SCRN_BUF_CSETS(screen, screen->cur_row)[0])) {
		col = MaxCols(screen) / 2 - n;
	    }
#endif
	    horizontal_copy_area(screen,
				 (int) (screen->cur_col + n),
				 col - screen->cur_col,
				 -((int) n));

	    ClearCurBackground(screen,
			       CursorY(screen, screen->cur_row),
			       CurCursorX(screen, screen->cur_row, col),
			       (unsigned) FontHeight(screen),
			       n * CurFontWidth(screen, screen->cur_row));
	}
    }
    if (n > 0) {
	/* adjust screen->buf */
	ScrnDeleteChar(screen, (unsigned) n);
    }
}

/*
 * Clear from cursor position to beginning of display, inclusive.
 */
static void
ClearAbove(TScreen * screen)
{
    if (screen->protected_mode != OFF_PROTECT) {
	int row;
	unsigned len = MaxCols(screen);

	assert(screen->max_col >= 0);
	for (row = 0; row <= screen->max_row; row++)
	    ClearInLine(screen, row, 0, len);
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
				   (unsigned) (height * FontHeight(screen)),
				   (unsigned) (Width(screen)));
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
	unsigned len = MaxCols(screen);

	assert(screen->max_col >= 0);
	for (row = screen->cur_row + 1; row <= screen->max_row; row++)
	    ClearInLine(screen, row, 0, len);
    } else {
	int top;

	if ((top = screen->cur_row - screen->topline) <= screen->max_row) {
	    if (screen->scroll_amt)
		FlushScroll(screen);
	    if (++top <= screen->max_row) {
		ClearCurBackground(screen,
				   top * FontHeight(screen) + screen->border,
				   OriginX(screen),
				   (unsigned) ((screen->max_row - top + 1)
					       * FontHeight(screen)),
				   (unsigned) (Width(screen)));
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
ClearInLine(TScreen * screen, int row, int col, unsigned len)
{
    int rc = 1;
    int flags = TERM_COLOR_FLAGS(term);

    TRACE(("ClearInLine(row=%d, col=%d, len=%d) vs %d..%d\n",
	   row, col, len,
	   screen->startHRow,
	   screen->startHCol));

    if (ScrnHaveSelection(screen)
	&& ScrnIsLineInSelection(screen, row)) {
	ScrnDisownSelection(screen);
    }

    /*
     * If we're clearing to the end of the line, we won't count this as
     * "drawn" characters.  We'll only do cut/paste on "drawn" characters,
     * so this has the effect of suppressing trailing blanks from a
     * selection.
     */
    if (col + (int) len < MaxCols(screen)) {
	flags |= CHARDRAWN;
    } else {
	len = MaxCols(screen) - col;
    }

    /* If we've marked protected text on the screen, we'll have to
     * check each time we do an erase.
     */
    if (screen->protected_mode != OFF_PROTECT) {
	unsigned n;
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
	    ClearCurBackground(screen,
			       CursorY(screen, row),
			       CurCursorX(screen, row, col),
			       (unsigned) FontHeight(screen),
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
    unsigned len = (MaxCols(screen) - screen->cur_col);

    assert(screen->max_col >= 0);
    assert(screen->max_col >= screen->cur_col);

    if (n < 0)			/* the remainder of the line */
	n = MaxCols(screen);
    if (n == 0)			/* default for 'ECH' */
	n = 1;

    if (len > (unsigned) n)
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
    unsigned len = screen->cur_col + 1;
    assert(screen->cur_col >= 0);

    (void) ClearInLine(screen, screen->cur_row, 0, len);
}

/*
 * Erase the cursor's line.
 */
static void
ClearLine(TScreen * screen)
{
    unsigned len = MaxCols(screen);

    assert(screen->max_col >= 0);
    (void) ClearInLine(screen, screen->cur_row, 0, len);
}

void
ClearScreen(TScreen * screen)
{
    int top;

    if (screen->cursor_state)
	HideCursor();

    ScrnDisownSelection(screen);
    screen->do_wrap = 0;
    if ((top = -screen->topline) <= screen->max_row) {
	if (screen->scroll_amt)
	    FlushScroll(screen);
	ClearCurBackground(screen,
			   top * FontHeight(screen) + screen->border,
			   OriginX(screen),
			   (unsigned) ((screen->max_row - top + 1)
				       * FontHeight(screen)),
			   (unsigned) Width(screen));
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
	    unsigned len = MaxCols(screen);

	    assert(screen->max_col >= 0);
	    for (row = 0; row <= screen->max_row; row++)
		rc &= ClearInLine(screen, row, 0, len);
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
    if (width != 0 && height != 0) {
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
	      (unsigned) FontHeight(screen),
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
		  (unsigned) Width(screen),
		  (unsigned) (nlines * FontHeight(screen)),
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
    if (reply->window == screen->iconVwin.window) {
	WhichVWin(screen) = &screen->iconVwin;
	TRACE(("HandleExposure - icon"));
    } else {
	WhichVWin(screen) = &screen->fullVwin;
	TRACE(("HandleExposure - normal"));
    }
    TRACE((" event %d,%d %dx%d\n",
	   reply->y,
	   reply->x,
	   reply->height,
	   reply->width));
#endif /* NO_ACTIVE_ICON */

    /* if not doing CopyArea or if this is a GraphicsExpose, don't translate */
    if (!screen->incopy || event->type != Expose)
	return handle_translated_exposure(screen, reply->x, reply->y,
					  reply->width,
					  reply->height);
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
			   int rect_width,
			   int rect_height)
{
    int toprow, leftcol, nrows, ncols;
    int x0, x1;
    int y0, y1;

    TRACE(("handle_translated_exposure at %d,%d size %dx%d\n",
	   rect_y, rect_x, rect_height, rect_width));

    x0 = (rect_x - OriginX(screen));
    x1 = (x0 + rect_width);

    y0 = (rect_y - OriginY(screen));
    y1 = (y0 + rect_height);

    toprow = y0 / FontHeight(screen);
    if (toprow < 0)
	toprow = 0;

    leftcol = x0 / CurFontWidth(screen, screen->cur_row);
    if (leftcol < 0)
	leftcol = 0;

    nrows = (y1 - 1) / FontHeight(screen) - toprow + 1;
    ncols = (x1 - 1) / FontWidth(screen) - leftcol + 1;
    toprow -= screen->scrolls;
    if (toprow < 0) {
	nrows += toprow;
	toprow = 0;
    }
    if (toprow + nrows > MaxRows(screen))
	nrows = MaxRows(screen) - toprow;
    if (leftcol + ncols > MaxCols(screen))
	ncols = MaxCols(screen) - leftcol;

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
    int n;

    pColors->which = 0;
    for (n = 0; n < NCOLORS; ++n) {
	SET_COLOR_VALUE(pColors, n, T_COLOR(screen, n));
    }
}

void
ChangeColors(XtermWidget tw, ScrnColors * pNew)
{
    TScreen *screen = &tw->screen;
#if OPT_TEK4014
    Window tek = TWindow(screen);
#endif

    TRACE(("ChangeColors\n"));

    if (COLOR_DEFINED(pNew, TEXT_CURSOR)) {
	T_COLOR(screen, TEXT_CURSOR) = COLOR_VALUE(pNew, TEXT_CURSOR);
	TRACE(("... TEXT_CURSOR: %#lx\n", T_COLOR(screen, TEXT_CURSOR)));
    } else if ((T_COLOR(screen, TEXT_CURSOR) == T_COLOR(screen, TEXT_FG)) &&
	       (COLOR_DEFINED(pNew, TEXT_FG))) {
	T_COLOR(screen, TEXT_CURSOR) = COLOR_VALUE(pNew, TEXT_FG);
	TRACE(("... TEXT_CURSOR: %#lx\n", T_COLOR(screen, TEXT_CURSOR)));
    }

    if (COLOR_DEFINED(pNew, TEXT_FG)) {
	Pixel fg = COLOR_VALUE(pNew, TEXT_FG);
	T_COLOR(screen, TEXT_FG) = fg;
	TRACE(("... TEXT_FG: %#lx\n", T_COLOR(screen, TEXT_FG)));
	XSetForeground(screen->display, NormalGC(screen), fg);
	XSetBackground(screen->display, ReverseGC(screen), fg);
	XSetForeground(screen->display, NormalBoldGC(screen), fg);
	XSetBackground(screen->display, ReverseBoldGC(screen), fg);
    }

    if (COLOR_DEFINED(pNew, TEXT_BG)) {
	Pixel bg = COLOR_VALUE(pNew, TEXT_BG);
	T_COLOR(screen, TEXT_BG) = bg;
	TRACE(("... TEXT_BG: %#lx\n", T_COLOR(screen, TEXT_BG)));
	XSetBackground(screen->display, NormalGC(screen), bg);
	XSetForeground(screen->display, ReverseGC(screen), bg);
	XSetBackground(screen->display, NormalBoldGC(screen), bg);
	XSetForeground(screen->display, ReverseBoldGC(screen), bg);
	XSetWindowBackground(screen->display, VWindow(screen),
			     T_COLOR(screen, TEXT_BG));
    }
#if OPT_HIGHLIGHT_COLOR
    if (COLOR_DEFINED(pNew, HIGHLIGHT_BG)) {
	T_COLOR(screen, HIGHLIGHT_BG) = COLOR_VALUE(pNew, HIGHLIGHT_BG);
	TRACE(("... HIGHLIGHT_BG: %#lx\n", T_COLOR(screen, HIGHLIGHT_BG)));
    }
#endif

    if (COLOR_DEFINED(pNew, MOUSE_FG) || (COLOR_DEFINED(pNew, MOUSE_BG))) {
	if (COLOR_DEFINED(pNew, MOUSE_FG)) {
	    T_COLOR(screen, MOUSE_FG) = COLOR_VALUE(pNew, MOUSE_FG);
	    TRACE(("... MOUSE_FG: %#lx\n", T_COLOR(screen, MOUSE_FG)));
	}
	if (COLOR_DEFINED(pNew, MOUSE_BG)) {
	    T_COLOR(screen, MOUSE_BG) = COLOR_VALUE(pNew, MOUSE_BG);
	    TRACE(("... MOUSE_BG: %#lx\n", T_COLOR(screen, MOUSE_BG)));
	}

	recolor_cursor(screen->pointer_cursor,
		       T_COLOR(screen, MOUSE_FG),
		       T_COLOR(screen, MOUSE_BG));
	recolor_cursor(screen->arrow,
		       T_COLOR(screen, MOUSE_FG),
		       T_COLOR(screen, MOUSE_BG));
	XDefineCursor(screen->display, VWindow(screen),
		      screen->pointer_cursor);

#if OPT_TEK4014
	if (tek)
	    XDefineCursor(screen->display, tek, screen->arrow);
#endif
    }
#if OPT_TEK4014
    if (COLOR_DEFINED(pNew, TEK_FG) ||
	COLOR_DEFINED(pNew, TEK_BG) ||
	COLOR_DEFINED(pNew, TEK_CURSOR)) {
	ChangeTekColors(screen, pNew);
    }
#endif
    set_cursor_gcs(screen);
    XClearWindow(screen->display, VWindow(screen));
    ScrnRefresh(screen, 0, 0, MaxRows(screen),
		MaxCols(screen), False);
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
		MaxRows(screen),
		MaxCols(screen), False);
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
	EXCHANGE(screen->Acolors[0], screen->Acolors[7], tmp2);
	EXCHANGE(screen->Acolors[8], screen->Acolors[15], tmp2);
    });

    tmp = T_COLOR(screen, TEXT_BG);
    if (T_COLOR(screen, TEXT_CURSOR) == T_COLOR(screen, TEXT_FG))
	T_COLOR(screen, TEXT_CURSOR) = tmp;
    T_COLOR(screen, TEXT_BG) = T_COLOR(screen, TEXT_FG);
    T_COLOR(screen, TEXT_FG) = tmp;

    EXCHANGE(T_COLOR(screen, MOUSE_FG), T_COLOR(screen, MOUSE_BG), tmp);
    EXCHANGE(NormalGC(screen), ReverseGC(screen), tmpGC);
    EXCHANGE(NormalBoldGC(screen), ReverseBoldGC(screen), tmpGC);
#ifndef NO_ACTIVE_ICON
    tmpGC = screen->iconVwin.normalGC;
    screen->iconVwin.normalGC = screen->iconVwin.reverseGC;
    screen->iconVwin.reverseGC = tmpGC;

    tmpGC = screen->iconVwin.normalboldGC;
    screen->iconVwin.normalboldGC = screen->iconVwin.reverseboldGC;
    screen->iconVwin.reverseboldGC = tmpGC;
#endif /* NO_ACTIVE_ICON */

    recolor_cursor(screen->pointer_cursor,
		   T_COLOR(screen, MOUSE_FG),
		   T_COLOR(screen, MOUSE_BG));
    recolor_cursor(screen->arrow,
		   T_COLOR(screen, MOUSE_FG),
		   T_COLOR(screen, MOUSE_BG));

    termw->misc.re_verse = !termw->misc.re_verse;

    if (XtIsRealized((Widget) termw)) {
	XDefineCursor(screen->display, VWindow(screen), screen->pointer_cursor);
    }
#if OPT_TEK4014
    if (tek)
	XDefineCursor(screen->display, tek, screen->arrow);
#endif

    if (screen->scrollWidget)
	ScrollBarReverseVideo(screen->scrollWidget);

    if (XtIsRealized((Widget) termw)) {
	XSetWindowBackground(screen->display, VWindow(screen),
			     T_COLOR(screen, TEXT_BG));

	/* the shell-window's background will be used in the first repainting
	 * on resizing
	 */
	XSetWindowBackground(screen->display, VShellWindow,
			     T_COLOR(screen, TEXT_BG));
    }
#if OPT_TEK4014
    TekReverseVideo(screen);
#endif
    if (XtIsRealized((Widget) termw)) {
	XClearWindow(screen->display, VWindow(screen));
	ScrnRefresh(screen, 0, 0, MaxRows(screen),
		    MaxCols(screen), False);
    }
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

#if OPT_RENDERFONT
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

/*
 * fontconfig/Xft combination prior to 2.2 has a problem with
 * CJK truetype 'double-width' (bi-width/monospace) fonts leading
 * to the 's p a c e d o u t' rendering. Consequently, we can't
 * rely on XftDrawString8/16  when one of  those fonts is used.
 * Instead, we need to roll out our own using XftDrawCharSpec.
 * A patch in the same spirit (but in a rather different form)
 * was applied to gnome vte and gtk2 port of vim.
 * See http://bugzilla.mozilla.org/show_bug.cgi?id=196312
 */
static void
xtermXftDrawString(TScreen * screen,
		   unsigned flags GCC_UNUSED,
		   XftColor * color,
		   XftFont * font,
		   int x,
		   int y,
		   PAIRED_CHARS(Char * text, Char * text2),
		   int len,
		   int fwidth,
		   int *deltax)
{
#if OPT_RENDERWIDE
    XftFont *wfont;
    int n;
    int ncells = 0;		/* # of 'half-width' charcells */
    static XftCharSpec *sbuf;
    static int slen = 0;
    XftFont *lastFont = 0;
    XftFont *currFont = 0;
    int start = 0;
    int charWidth;
    FcChar32 wc;
    int fontnum = screen->menu_font_number;

    if (len == 0 || !(*text || *text2)) {
	return;
    }
#if OPT_ISO_COLORS
    if ((flags & UNDERLINE)
	&& screen->italicULMode
	&& screen->renderWideItal[fontnum]) {
	wfont = screen->renderWideItal[fontnum];
    } else
#endif
	if ((flags & BOLDATTR(screen))
	    && screen->renderWideBold[fontnum]) {
	wfont = screen->renderWideBold[fontnum];
    } else {
	wfont = screen->renderWideNorm[fontnum];
    }

    if ((int) slen < len) {
	slen = (len + 1) * 2;
	sbuf = (XftCharSpec *) XtRealloc((char *) sbuf,
					 slen * sizeof(XftCharSpec));
    }

    for (n = 0; n < len; n++) {
	if (text2)
	    wc = *text++ | (*text2++ << 8);
	else
	    wc = *text++;
	sbuf[n].ucs4 = wc;
	sbuf[n].x = x + fwidth * ncells;
	sbuf[n].y = y;
	charWidth = my_wcwidth((int) wc);
	currFont = (charWidth == 2 && wfont != 0) ? wfont : font;
	ncells += charWidth;
	if (lastFont != currFont) {
	    if (lastFont != 0) {
		XftDrawCharSpec(screen->renderDraw,
				color,
				lastFont,
				sbuf + start,
				n - start);
	    }
	    start = n;
	    lastFont = currFont;
	}
    }
    XftDrawCharSpec(screen->renderDraw,
		    color,
		    lastFont,
		    sbuf + start,
		    n - start);

    if (deltax)
	*deltax = ncells * fwidth;
#else

    XftDrawString8(screen->renderDraw,
		   color,
		   font,
		   x, y, (unsigned char *) text, len);
    if (deltax)
	*deltax = len * fwidth;
#endif
}
#endif /* OPT_RENDERFONT */

#define DrawX(col) x + (col * (font_width))
#define DrawSegment(first,last) (void)drawXtermText(screen, flags|NOTRANSLATION, gc, DrawX(first), y, chrset, PAIRED_CHARS(text+first, text2+first), (unsigned)(last - first), on_wide)

#if OPT_WIDE_CHARS
/*
 * Actually this should be called "groff_workaround()" - for the places where
 * groff stomps on compatibility.  Still, if enough people get used to it,
 * this might someday become a quasi-standard.
 */
static int
ucs_workaround(TScreen * screen,
	       unsigned ch,
	       unsigned flags,
	       GC gc,
	       int x,
	       int y,
	       int chrset,
	       int on_wide)
{
    int fixed = False;

    if (screen->wide_chars && screen->utf8_mode && ch > 256) {
	switch (ch) {
	case 0x2010:		/* groff "-" */
	case 0x2011:
	case 0x2012:
	case 0x2013:
	case 0x2014:
	case 0x2015:
	case 0x2212:		/* groff "\-" */
	    ch = '-';
	    fixed = True;
	    break;
	case 0x2018:		/* groff "`" */
	    ch = '`';
	    fixed = True;
	    break;
	case 0x2019:		/* groff ' */
	    ch = '\'';
	    fixed = True;
	    break;
	case 0x201C:		/* groff lq */
	case 0x201D:		/* groff rq */
	    ch = '"';
	    fixed = True;
	    break;
	}
	if (fixed) {
	    Char text[2];
	    Char text2[2];

	    text[0] = ch;
	    text2[0] = 0;
	    drawXtermText(screen,
			  flags,
			  gc,
			  x,
			  y,
			  chrset,
			  PAIRED_CHARS(text, text2),
			  1,
			  on_wide);
	}
    }
    return fixed;
}
#endif

#if OPT_CLIP_BOLD
/*
 * This special case is a couple of percent slower, but avoids a lot of pixel
 * trash in rxcurses' hanoi.cmd demo (e.g., 10x20 font).
 */
#define beginClipping(screen,gc,pwidth,plength) \
	    if (pwidth > 2) { \
		XRectangle clip; \
		int clip_x = x; \
		int clip_y = y - FontHeight(screen) + FontDescent(screen); \
		clip.x = 0; \
		clip.y = 0; \
		clip.height = FontHeight(screen); \
		clip.width = pwidth * plength; \
		XSetClipRectangles(screen->display, gc, \
				   clip_x, clip_y, \
				   &clip, 1, Unsorted); \
	    }
#define endClipping(screen,gc) \
	    XSetClipMask(screen->display, gc, None)
#else
#define beginClipping(screen,gc,pwidth,plength)		/* nothing */
#define endClipping(screen,gc)	/* nothing */
#endif /* OPT_CLIP_BOLD */

/*
 * Draws text with the specified combination of bold/underline.  The return
 * value is the updated x position.
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
    int underline_len;
    /* Intended width of the font to draw (as opposed to the actual width of
       the X font, and the width of the default font) */
    int font_width = ((flags & DOUBLEWFONT) ? 2 : 1) * screen->fnt_wide;
    Bool did_ul = False;

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
		  ? xterm_DoubleGC((unsigned) chrset, flags, gc)
		  : 0);

	TRACE(("DRAWTEXT%c[%4d,%4d] (%d) %d:%.*s\n",
	       screen->cursor_state == OFF ? ' ' : '*',
	       y, x, chrset, len, (int) len, text));

	if (gc2 != 0) {		/* draw actual double-sized characters */
	    /* Update the last-used cache of double-sized fonts */
	    int inx = xterm_Double_index((unsigned) chrset, flags);
	    XFontStruct *fs = screen->double_fonts[inx].fs;
	    XRectangle rect, *rp = &rect;
	    int nr = 1;
	    int adjust;

	    font_width *= 2;
	    flags |= DOUBLEWFONT;

	    rect.x = 0;
	    rect.y = 0;
	    rect.width = len * font_width;
	    rect.height = FontHeight(screen);

	    switch (chrset) {
	    case CSET_DHL_TOP:
		rect.y = -(rect.height / 2);
		y -= rect.y;
		flags |= DOUBLEHFONT;
		break;
	    case CSET_DHL_BOT:
		rect.y = (rect.height / 2);
		y -= rect.y;
		flags |= DOUBLEHFONT;
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

	    /* Call ourselves recursively with the new gc */

	    /*
	     * If we're trying to use proportional font, or if the
	     * font server didn't give us what we asked for wrt
	     * width, position each character independently.
	     */
	    if (screen->fnt_prop
		|| (fs->min_bounds.width != fs->max_bounds.width)
		|| (fs->min_bounds.width != 2 * FontWidth(screen))) {
		/* It is hard to fall-through to the main
		   branch: in a lot of places the check
		   for the cached font info is for
		   normal/bold fonts only. */
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

	    TRACE(("drawtext [%4d,%4d]\n", y, x));
	} else {		/* simulate double-sized characters */
#if OPT_WIDE_CHARS
	    Char *wide = 0;
#endif
	    unsigned need = 2 * len;
	    Char *temp = TypeMallocN(Char, need);
	    unsigned n = 0;
	    if_OPT_WIDE_CHARS(screen, {
		wide = TypeMallocN(Char, need);
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
#if OPT_RENDERFONT
    if (UsingRenderFont(term)) {
	Display *dpy = screen->display;
	XftFont *font;
	XGCValues values;
	int fontnum = screen->menu_font_number;

	if (!screen->renderDraw) {
	    int scr;
	    Drawable draw = VWindow(screen);
	    Visual *visual;

	    scr = DefaultScreen(dpy);
	    visual = DefaultVisual(dpy, scr);
	    screen->renderDraw = XftDrawCreate(dpy, draw, visual,
					       DefaultColormap(dpy, scr));
	}
#if OPT_ISO_COLORS
	if ((flags & UNDERLINE)
	    && screen->italicULMode
	    && screen->renderFontItal[fontnum]) {
	    font = screen->renderFontItal[fontnum];
	    did_ul = True;
	} else
#endif
	    if ((flags & BOLDATTR(screen))
		&& screen->renderFontBold[fontnum]) {
	    font = screen->renderFontBold[fontnum];
	} else {
	    font = screen->renderFontNorm[fontnum];
	}
	XGetGCValues(dpy, gc, GCForeground | GCBackground, &values);
	if (!(flags & NOBACKGROUND))
	    XftDrawRect(screen->renderDraw,
			getColor(values.background),
			x, y,
			len * FontWidth(screen),
			(unsigned) FontHeight(screen));

	y += font->ascent;
#if OPT_BOX_CHARS
	if (!screen->force_box_chars) {
	    /* adding code to substitute simulated line-drawing characters */
	    int last, first = 0;
	    Dimension old_wide, old_high = 0;
	    int curX = x;

	    for (last = 0; last < (int) len; last++) {
		unsigned ch = text[last];
		int deltax = 0;

		/*
		 * If we're reading UTF-8 from the client, we may have a
		 * line-drawing character.  Translate it back to our box-code
		 * if it is really a line-drawing character (since the
		 * fonts used by Xft generally do not have correct glyphs),
		 * or if Xft can tell us that the glyph is really missing.
		 */
		if_OPT_WIDE_CHARS(screen, {
		    unsigned full = (ch | (text2[last] << 8));
		    unsigned part = ucs2dec(full);
		    if (xtermIsDecGraphic(part) &&
			(xtermIsLineDrawing(part)
			 || xtermXftMissing(term, font, full)))
			ch = part;
		    else
			ch = full;
		});
		/*
		 * If we have one of our box-codes, draw it directly.
		 */
		if (xtermIsDecGraphic(ch)) {
		    /* line drawing character time */
		    if (last > first) {
			xtermXftDrawString(screen, flags,
					   getColor(values.foreground),
					   font, curX, y,
					   PAIRED_CHARS(text + first,
							text2 + first),
					   last - first,
					   FontWidth(screen),
					   &deltax);
			curX += deltax;
		    }
		    old_wide = screen->fnt_wide;
		    old_high = screen->fnt_high;
		    screen->fnt_wide = FontWidth(screen);
		    screen->fnt_high = FontHeight(screen);
		    xtermDrawBoxChar(term, ch, flags, gc,
				     curX, y - FontAscent(screen));
		    curX += FontWidth(screen);
		    screen->fnt_wide = old_wide;
		    screen->fnt_high = old_high;
		    first = last + 1;
		}
	    }
	    if (last > first) {
		xtermXftDrawString(screen, flags,
				   getColor(values.foreground),
				   font, curX, y,
				   PAIRED_CHARS(text + first, text2 + first),
				   last - first,
				   FontWidth(screen),
				   NULL);
	    }
	} else
#endif /* OPT_BOX_CHARS */
	{
	    xtermXftDrawString(screen, flags,
			       getColor(values.foreground),
			       font, x, y, PAIRED_CHARS(text, text2),
			       (int) len, FontWidth(screen), NULL);
	}

	if ((flags & UNDERLINE) && screen->underline && !did_ul) {
	    if (FontDescent(screen) > 1)
		y++;
	    XDrawLine(screen->display, VWindow(screen), gc,
		      x, y,
		      x + (int) len * FontWidth(screen) - 1,
		      y);
	}
	return x + len * FontWidth(screen);
    }
#endif /* OPT_RENDERFONT */
    /*
     * If we're asked to display a proportional font, do this with a fixed
     * pitch.  Yes, it's ugly.  But we cannot distinguish the use of xterm
     * as a dumb terminal vs its use as in fullscreen programs such as vi.
     * Hint: do not try to use a proportional font in the icon.
     */
    if (!IsIcon(screen) && !(flags & CHARBYCHAR) && screen->fnt_prop) {
	int adj, width;
	GC fillGC = gc;		/* might be cursorGC */
	XFontStruct *fs = ((flags & BOLDATTR(screen))
			   ? BoldFont(screen)
			   : NormalFont(screen));

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

	if (!(flags & NOBACKGROUND))
	    XFillRectangle(screen->display, VWindow(screen), fillGC,
			   x, y,
			   len * FontWidth(screen),
			   (unsigned) FontHeight(screen));

	while (len--) {
	    width = XTextWidth(fs, (char *) text, 1);
	    adj = (FontWidth(screen) - width) / 2;
	    (void) drawXtermText(screen, flags | NOBACKGROUND | CHARBYCHAR,
				 gc, x + adj, y, chrset,
				 PAIRED_CHARS(text++, text2++), 1, on_wide);
	    x += FontWidth(screen);
	}
	return x;
    }
#if OPT_BOX_CHARS
    /* If the font is incomplete, draw some substitutions */
    if (!IsIcon(screen)
	&& !(flags & NOTRANSLATION)
	&& (!screen->fnt_boxes || screen->force_box_chars)) {
	/* Fill in missing box-characters.
	   Find regions without missing characters, and draw
	   them calling ourselves recursively.  Draw missing
	   characters via xtermDrawBoxChar(). */
	XFontStruct *font = ((flags & BOLD)
			     ? BoldFont(screen)
			     : NormalFont(screen));
	int last, first = 0;
	for (last = 0; last < (int) len; last++) {
	    unsigned ch = text[last];
	    Bool isMissing;
#if OPT_WIDE_CHARS
	    if (text2 != 0)
		ch |= (text2[last] << 8);
	    isMissing = (ch != HIDDEN_CHAR)
		&& (xtermMissingChar(term, ch,
				     ((on_wide || iswide((int) ch))
				      && screen->fnt_dwd)
				     ? screen->fnt_dwd
				     : font));
#else
	    isMissing = xtermMissingChar(term, ch, font);
#endif
	    if (isMissing) {
		if (last > first)
		    DrawSegment(first, last);
#if OPT_WIDE_CHARS
		if (!ucs_workaround(screen, ch, flags, gc, DrawX(last), y,
				    chrset, on_wide))
#endif
		    xtermDrawBoxChar(term, ch, flags, gc, DrawX(last), y);
		first = last + 1;
	    }
	}
	if (last <= first) {
	    return x + real_length * FontWidth(screen);
	}
	text += first;
#if OPT_WIDE_CHARS
	text2 += first;
#endif
	len = last - first;
	flags |= NOTRANSLATION;
	if (DrawX(first) != x) {
	    return drawXtermText(screen,
				 flags,
				 gc,
				 DrawX(first),
				 y,
				 chrset,
				 PAIRED_CHARS(text, text2),
				 len,
				 on_wide);
	}
    }
#endif /* OPT_BOX_CHARS */
    /*
     * Behave as if the font has (maybe Unicode-replacements for) drawing
     * characters in the range 1-31 (either we were not asked to ignore them,
     * or the caller made sure that there is none).  The only translation we do
     * in this branch is the removal of HIDDEN_CHAR (for the wide-char case).
     */
    TRACE(("drawtext%c[%4d,%4d] (%d) %d:%s\n",
	   screen->cursor_state == OFF ? ' ' : '*',
	   y, x, chrset, len,
	   visibleChars(PAIRED_CHARS(text, text2), len)));
    y += FontAscent(screen);

#if OPT_WIDE_CHARS
    if (screen->wide_chars || screen->unicode_font) {
	int ascent_adjust = 0;
	static XChar2b *sbuf;
	static Cardinal slen;
	int n;
	unsigned ch = text[0] | (text2[0] << 8);
	int wideness = (!IsIcon(screen)
			&& ((on_wide || iswide((int) ch) != 0)
			    && (screen->fnt_dwd != NULL)));
	unsigned char *endtext = text + len;
	if (slen < len) {
	    slen = (len + 1) * 2;
	    sbuf = (XChar2b *) XtRealloc((char *) sbuf, slen * sizeof(*sbuf));
	}
	for (n = 0; n < (int) len; n++) {
	    sbuf[n].byte2 = *text;
	    sbuf[n].byte1 = *text2;
#if OPT_MINI_LUIT
#define UCS2SBUF(n,value)	sbuf[n].byte2 = (value & 0xff);\
	    			sbuf[n].byte1 = (value >> 8)
#define Map2Sbuf(n,from,to) (*text == from) { UCS2SBUF(n,to); }
	    if (screen->latin9_mode && !screen->utf8_mode && *text2 == 0) {

		/* see http://www.cs.tut.fi/~jkorpela/latin9.html */
		/* *INDENT-OFF* */
		if Map2Sbuf(n, 0xa4, 0x20ac)
		else if Map2Sbuf(n, 0xa6, 0x0160)
		else if Map2Sbuf(n, 0xa8, 0x0161)
		else if Map2Sbuf(n, 0xb4, 0x017d)
		else if Map2Sbuf(n, 0xb8, 0x017e)
		else if Map2Sbuf(n, 0xbc, 0x0152)
		else if Map2Sbuf(n, 0xbd, 0x0153)
		else if Map2Sbuf(n, 0xbe, 0x0178)
		/* *INDENT-ON* */

	    }
	    if (screen->unicode_font
		&& *text2 == 0
		&& (*text == 0x7f || *text < 0x20)) {
		int ni = dec2ucs(*text == 0x7f ? 0 : *text);
		UCS2SBUF(n, ni);
	    }
#endif /* OPT_MINI_LUIT */
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
	underline_len = len;

	/* Set the drawing font */
	if (flags & (DOUBLEHFONT | DOUBLEWFONT)) {
	    ;			/* Do nothing: font is already set */
	} else if (wideness
		   && (screen->fnt_dwd->fid || screen->fnt_dwdb->fid)) {
	    underline_len = real_length = len * 2;
	    if ((flags & BOLDATTR(screen)) != 0
		&& screen->fnt_dwdb->fid) {
		XSetFont(screen->display, gc, screen->fnt_dwdb->fid);
		ascent_adjust = (screen->fnt_dwdb->ascent
				 - NormalFont(screen)->ascent);
	    } else {
		XSetFont(screen->display, gc, screen->fnt_dwd->fid);
		ascent_adjust = (screen->fnt_dwd->ascent
				 - NormalFont(screen)->ascent);
	    }
	    /* fix ascent */
	} else if ((flags & BOLDATTR(screen)) != 0
		   && BoldFont(screen)->fid) {
	    XSetFont(screen->display, gc, BoldFont(screen)->fid);
	} else {
	    XSetFont(screen->display, gc, NormalFont(screen)->fid);
	}

	if (flags & NOBACKGROUND)
	    XDrawString16(screen->display,
			  VWindow(screen), gc,
			  x, y + ascent_adjust,
			  sbuf, n);
	else
	    XDrawImageString16(screen->display,
			       VWindow(screen), gc,
			       x, y + ascent_adjust,
			       sbuf, n);

	if ((flags & BOLDATTR(screen)) && screen->enbolden) {
	    beginClipping(screen, gc, font_width, len);
	    XDrawString16(screen->display, VWindow(screen), gc,
			  x + 1, y + ascent_adjust, sbuf, n);
	    endClipping(screen, gc);
	}

    } else
#endif /* OPT_WIDE_CHARS */
    {
	int length = len;	/* X should have used unsigned */

	if (flags & NOBACKGROUND)
	    XDrawString(screen->display, VWindow(screen), gc,
			x, y, (char *) text, length);
	else
	    XDrawImageString(screen->display, VWindow(screen), gc,
			     x, y, (char *) text, length);
	underline_len = length;
	if ((flags & BOLDATTR(screen)) && screen->enbolden) {
	    beginClipping(screen, gc, font_width, length);
	    XDrawString(screen->display, VWindow(screen), gc,
			x + 1, y, (char *) text, length);
	    endClipping(screen, gc);
	}
    }

    if ((flags & UNDERLINE) && screen->underline && !did_ul) {
	if (FontDescent(screen) > 1)
	    y++;
	XDrawLine(screen->display, VWindow(screen), gc,
		  x, y, x + underline_len * font_width - 1, y);
    }

    return x + real_length * FontWidth(screen);
}

/* set up size hints for window manager; min 1 char by 1 char */
void
xtermSizeHints(XtermWidget xw, XSizeHints * sizehints, int scrollbarWidth)
{
    TScreen *screen = &xw->screen;

    TRACE(("xtermSizeHints\n"));
    TRACE(("   border    %d\n", xw->core.border_width));
    TRACE(("   scrollbar %d\n", scrollbarWidth));

    sizehints->base_width = 2 * screen->border + scrollbarWidth;
    sizehints->base_height = 2 * screen->border;

#if OPT_TOOLBAR
    TRACE(("   toolbar   %d\n", ToolbarHeight(xw)));

    sizehints->base_height += ToolbarHeight(xw);
    sizehints->base_height += xw->core.border_width * 2;
    sizehints->base_width += xw->core.border_width * 2;
#endif

    sizehints->width_inc = FontWidth(screen);
    sizehints->height_inc = FontHeight(screen);
    sizehints->min_width = sizehints->base_width + sizehints->width_inc;
    sizehints->min_height = sizehints->base_height + sizehints->height_inc;

    sizehints->width = MaxCols(screen) * FontWidth(screen) + sizehints->min_width;
    sizehints->height = MaxRows(screen) * FontHeight(screen) + sizehints->min_height;

    sizehints->flags |= (PSize | PBaseSize | PMinSize | PResizeInc);

    TRACE_HINTS(sizehints);
}

/*
 * Returns a GC, selected according to the font (reverse/bold/normal) that is
 * required for the current position (implied).  The GC is updated with the
 * current screen foreground and background colors.
 */
GC
updatedXtermGC(TScreen * screen, unsigned flags, unsigned fg_bg, Bool hilite)
{
    int my_fg = extract_fg(fg_bg, flags);
    int my_bg = extract_bg(fg_bg, flags);
    Pixel fg_pix = getXtermForeground(flags, my_fg);
    Pixel bg_pix = getXtermBackground(flags, my_bg);
    Pixel xx_pix;
#if OPT_HIGHLIGHT_COLOR
    Pixel hi_pix = T_COLOR(screen, HIGHLIGHT_BG);
#endif
    GC gc;

    checkVeryBoldColors(flags, my_fg);

    if (ReverseOrHilite(screen, flags, hilite)) {
	if (flags & BOLDATTR(screen))
	    gc = ReverseBoldGC(screen);
	else
	    gc = ReverseGC(screen);

#if OPT_HIGHLIGHT_COLOR
	if (hi_pix != T_COLOR(screen, TEXT_FG)
	    && hi_pix != fg_pix
	    && hi_pix != bg_pix
	    && hi_pix != term->dft_foreground) {
	    bg_pix = fg_pix;
	    fg_pix = hi_pix;
	}
#endif
	xx_pix = bg_pix;
	bg_pix = fg_pix;
	fg_pix = xx_pix;
    } else {
	if (flags & BOLDATTR(screen))
	    gc = NormalBoldGC(screen);
	else
	    gc = NormalGC(screen);

    }

#if OPT_BLINK_TEXT
    if ((screen->blink_state == ON) && (!screen->blink_as_bold) && (flags & BLINK)) {
	fg_pix = bg_pix;
    }
#endif

    XSetForeground(screen->display, gc, fg_pix);
    XSetBackground(screen->display, gc, bg_pix);
    return gc;
}

/*
 * Resets the foreground/background of the GC returned by 'updatedXtermGC()'
 * to the values that would be set in SGR_Foreground and SGR_Background. This
 * duplicates some logic, but only modifies 1/4 as many GC's.
 */
void
resetXtermGC(TScreen * screen, unsigned flags, Bool hilite)
{
    Pixel fg_pix = getXtermForeground(flags, term->cur_foreground);
    Pixel bg_pix = getXtermBackground(flags, term->cur_background);
    GC gc;

    checkVeryBoldColors(flags, term->cur_foreground);

    if (ReverseOrHilite(screen, flags, hilite)) {
	if (flags & BOLDATTR(screen))
	    gc = ReverseBoldGC(screen);
	else
	    gc = ReverseGC(screen);

	XSetForeground(screen->display, gc, bg_pix);
	XSetBackground(screen->display, gc, fg_pix);

    } else {
	if (flags & BOLDATTR(screen))
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
unsigned
extract_fg(unsigned color, unsigned flags)
{
    unsigned fg = ExtractForeground(color);

    if (term->screen.colorAttrMode
	|| (fg == ExtractBackground(color))) {
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
unsigned
extract_bg(unsigned color, unsigned flags)
{
    unsigned bg = ExtractBackground(color);

    if (term->screen.colorAttrMode
	|| (bg == ExtractForeground(color))) {
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
ClearCurBackground(TScreen * screen,
		   int top,
		   int left,
		   unsigned height,
		   unsigned width)
{
    XSetWindowBackground(screen->display,
			 VWindow(screen),
			 getXtermBackground(term->flags, term->cur_background));

    XClearArea(screen->display, VWindow(screen),
	       left, top, width, height, False);

    XSetWindowBackground(screen->display,
			 VWindow(screen),
			 getXtermBackground(term->flags, MAXCOLORS));
}
#endif /* OPT_ISO_COLORS */

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

#if OPT_WIDE_CHARS
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
		       ? TypeRealloc(char, length, bfr)
		       : TypeMallocN(char, length));
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

    TRACE(("set_keyboard_type(%s, %s) currently %s\n",
	   visibleKeyboardType(type),
	   BtoS(set),
	   visibleKeyboardType(term->keyboard.type)));
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

    TRACE(("toggle_keyboard_type(%s) currently %s\n",
	   visibleKeyboardType(type),
	   visibleKeyboardType(term->keyboard.type)));
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

    TRACE(("init_keyboard_type(%s, %s) currently %s\n",
	   visibleKeyboardType(type),
	   BtoS(set),
	   visibleKeyboardType(term->keyboard.type)));
    if (set) {
	if (wasSet) {
	    fprintf(stderr, "Conflicting keyboard type option (%u/%u)\n",
		    term->keyboard.type, type);
	}
	term->keyboard.type = type;
	wasSet = True;
	update_keyboard_type();
    }
}

/*
 * If the keyboardType resource is set, use that, overriding the individual
 * boolean resources for different keyboard types.
 */
void
decode_keyboard_type(XTERM_RESOURCE * rp)
{
#define DATA(n, t, f) { n, t, XtOffsetOf(XTERM_RESOURCE, f) }
#define FLAG(n) *(Boolean *)(((char *)rp) + table[n].offset)
    static struct {
	const char *name;
	xtermKeyboardType type;
	unsigned offset;
    } table[] = {
#if OPT_HP_FUNC_KEYS
	DATA(NAME_HP_KT, keyboardIsHP, hpFunctionKeys),
#endif
#if OPT_SCO_FUNC_KEYS
	    DATA(NAME_SCO_KT, keyboardIsSCO, scoFunctionKeys),
#endif
	    DATA(NAME_SUN_KT, keyboardIsSun, sunFunctionKeys),
#if OPT_SUNPC_KBD
	    DATA(NAME_VT220_KT, keyboardIsVT220, sunKeyboard),
#endif
    };
    Cardinal n;

    if (x_strcasecmp(rp->keyboardType, "unknown")) {
	Bool found = False;
	for (n = 0; n < XtNumber(table); ++n) {
	    if (!x_strcasecmp(rp->keyboardType, table[n].name + 1)) {
		FLAG(n) = True;
		found = True;
		init_keyboard_type(table[n].type, FLAG(n));
	    } else {
		FLAG(n) = False;
	    }
	}
	if (!found) {
	    fprintf(stderr,
		    "KeyboardType resource \"%s\" not found\n",
		    rp->keyboardType);
	}
    } else {
	for (n = 0; n < XtNumber(table); ++n)
	    init_keyboard_type(table[n].type, FLAG(n));
    }
#undef DATA
#undef FLAG
}

#if OPT_WIDE_CHARS
#if defined(HAVE_WCHAR_H) && defined(HAVE_WCWIDTH)
/*
 * If xterm is running in a UTF-8 locale, it is still possible to encounter
 * old runtime configurations which yield incomplete or inaccurate data.
 */
static Bool
systemWcwidthOk(void)
{
    wchar_t n;
    int oops = 0;
    int last = 1024;

    for (n = 0; n < last; ++n) {
	int system_code = wcwidth(n);
	int intern_code = mk_wcwidth(n);

	/*
	 * Since mk_wcwidth() is designed to check for nonspacing characters,
	 * and has rough range-checks for double-width characters, it will
	 * generally not detect cases where a code has not been assigned.
	 *
	 * Some experimentation with GNU libc suggests that up to 1/4 of the
	 * codes would differ, simply because the runtime library would have a
	 * table listing the unassigned codes, and return -1 for those.  If
	 * mk_wcwidth() has no information about a code, it returns 1.  On the
	 * other hand, if the runtime returns a positive number, the two should
	 * agree.
	 *
	 * The "up to" is measured for 4k, 8k, 16k of data.  With only 1k, the
	 * number of differences was only 77.  However, that is only one
	 * system, and this is only a sanity check to avoid using broken
	 * libraries.
	 */
	if ((system_code < 0 && intern_code >= 1)
	    || (system_code >= 0 && intern_code != system_code)) {
	    ++oops;
	}
    }
    TRACE(("systemWcwidthOk: %d/%d mismatches\n", oops, last));
    return (oops < (last / 4));
}
#endif /* HAVE_WCWIDTH */

void
decode_wcwidth(int mode)
{
    switch (mode) {
    default:
#if defined(HAVE_WCHAR_H) && defined(HAVE_WCWIDTH)
	if (xtermEnvUTF8() && systemWcwidthOk()) {
	    my_wcwidth = wcwidth;
	    TRACE(("using system wcwidth() function\n"));
	    break;
	}
	/* FALLTHRU */
    case 2:
#endif
	my_wcwidth = &mk_wcwidth;
	TRACE(("using MK wcwidth() function\n"));
	break;
    case 3:
    case 4:
	my_wcwidth = &mk_wcwidth_cjk;
	TRACE(("using MK-CJK wcwidth() function\n"));
	break;
    }
}
#endif

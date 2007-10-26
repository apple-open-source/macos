/* $XTermId: cursor.c,v 1.36 2005/08/05 01:25:39 tom Exp $ */

/*
 *	$Xorg: cursor.c,v 1.3 2000/08/17 19:55:08 cpqbld Exp $
 */

/* $XFree86: xc/programs/xterm/cursor.c,v 3.18 2005/08/05 01:25:39 dickey Exp $ */

/*
 * Copyright 2002-2004,2005 by Thomas E. Dickey
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

/* cursor.c */

#include <xterm.h>
#include <data.h>

#include <assert.h>

/*
 * Moves the cursor to the specified position, checking for bounds.
 * (this includes scrolling regions)
 * The origin is considered to be 0, 0 for this procedure.
 */
void
CursorSet(TScreen * screen, int row, int col, unsigned flags)
{
    int use_row = row;
    int max_row;

    col = (col < 0 ? 0 : col);
    set_cur_col(screen, (col <= screen->max_col ? col : screen->max_col));
    max_row = screen->max_row;
    if (flags & ORIGIN) {
	use_row += screen->top_marg;
	max_row = screen->bot_marg;
    }
    use_row = (use_row < 0 ? 0 : use_row);
    set_cur_row(screen, (use_row <= max_row ? use_row : max_row));
    screen->do_wrap = 0;

    TRACE(("CursorSet(%d,%d) margins [%d..%d] -> %d,%d %s\n",
	   row, col,
	   screen->top_marg,
	   screen->bot_marg,
	   screen->cur_row,
	   screen->cur_col,
	   (flags & ORIGIN ? "origin" : "normal")));
}

/*
 * moves the cursor left n, no wrap around
 */
void
CursorBack(TScreen * screen, int n)
{
    int i, j, k, rev;

    if ((rev = (term->flags & (REVERSEWRAP | WRAPAROUND)) ==
	 (REVERSEWRAP | WRAPAROUND)) != 0
	&& screen->do_wrap)
	n--;
    if ((screen->cur_col -= n) < 0) {
	if (rev) {
	    if ((i = ((j = MaxCols(screen))
		      * screen->cur_row) + screen->cur_col) < 0) {
		k = j * MaxRows(screen);
		i += ((-i) / k + 1) * k;
	    }
	    set_cur_row(screen, i / j);
	    set_cur_col(screen, i % j);
	} else
	    set_cur_col(screen, 0);
    }
    screen->do_wrap = 0;
}

/*
 * moves the cursor forward n, no wraparound
 */
void
CursorForward(TScreen * screen, int n)
{
    int next = screen->cur_col + n;
    int max = CurMaxCol(screen, screen->cur_row);

    if (next > max)
	next = max;

    set_cur_col(screen, next);
    screen->do_wrap = 0;
}

/*
 * moves the cursor down n, no scrolling.
 * Won't pass bottom margin or bottom of screen.
 */
void
CursorDown(TScreen * screen, int n)
{
    int max;
    int next = screen->cur_row + n;

    max = (screen->cur_row > screen->bot_marg ?
	   screen->max_row : screen->bot_marg);
    if (next > max)
	next = max;
    if (next > screen->max_row)
	next = screen->max_row;

    set_cur_row(screen, next);
    screen->do_wrap = 0;
}

/*
 * moves the cursor up n, no linestarving.
 * Won't pass top margin or top of screen.
 */
void
CursorUp(TScreen * screen, int n)
{
    int min;
    int next = screen->cur_row - n;

    min = ((screen->cur_row < screen->top_marg)
	   ? 0
	   : screen->top_marg);
    if (next < min)
	next = min;
    if (next < 0)
	next = 0;

    set_cur_row(screen, next);
    screen->do_wrap = 0;
}

/*
 * Moves cursor down amount lines, scrolls if necessary.
 * Won't leave scrolling region. No carriage return.
 */
void
xtermIndex(TScreen * screen, int amount)
{
    int j;

    /*
     * indexing when below scrolling region is cursor down.
     * if cursor high enough, no scrolling necessary.
     */
    if (screen->cur_row > screen->bot_marg
	|| screen->cur_row + amount <= screen->bot_marg) {
	CursorDown(screen, amount);
	return;
    }

    CursorDown(screen, j = screen->bot_marg - screen->cur_row);
    xtermScroll(screen, amount - j);
}

/*
 * Moves cursor up amount lines, reverse scrolls if necessary.
 * Won't leave scrolling region. No carriage return.
 */
void
RevIndex(TScreen * screen, int amount)
{
    /*
     * reverse indexing when above scrolling region is cursor up.
     * if cursor low enough, no reverse indexing needed
     */
    if (screen->cur_row < screen->top_marg
	|| screen->cur_row - amount >= screen->top_marg) {
	CursorUp(screen, amount);
	return;
    }

    RevScroll(screen, amount - (screen->cur_row - screen->top_marg));
    CursorUp(screen, screen->cur_row - screen->top_marg);
}

/*
 * Moves Cursor To First Column In Line
 * (Note: xterm doesn't implement SLH, SLL which would affect use of this)
 */
void
CarriageReturn(TScreen * screen)
{
    set_cur_col(screen, 0);
    screen->do_wrap = 0;
}

/*
 * Save Cursor and Attributes
 */
void
CursorSave(XtermWidget tw)
{
    TScreen *screen = &tw->screen;
    SavedCursor *sc = &screen->sc[screen->alternate != False];

    sc->saved = True;
    sc->row = screen->cur_row;
    sc->col = screen->cur_col;
    sc->flags = tw->flags;
    sc->curgl = screen->curgl;
    sc->curgr = screen->curgr;
#if OPT_ISO_COLORS
    sc->cur_foreground = tw->cur_foreground;
    sc->cur_background = tw->cur_background;
    sc->sgr_foreground = tw->sgr_foreground;
#endif
    memmove(sc->gsets, screen->gsets, sizeof(screen->gsets));
}

/*
 * We save/restore all visible attributes, plus wrapping, origin mode, and the
 * selective erase attribute.
 */
#define DECSC_FLAGS (ATTRIBUTES|ORIGIN|WRAPAROUND|PROTECTED)

/*
 * Restore Cursor and Attributes
 */
void
CursorRestore(XtermWidget tw)
{
    TScreen *screen = &tw->screen;
    SavedCursor *sc = &screen->sc[screen->alternate != False];

    /* Restore the character sets, unless we never did a save-cursor op.
     * In that case, we'll reset the character sets.
     */
    if (sc->saved) {
	memmove(screen->gsets, sc->gsets, sizeof(screen->gsets));
	screen->curgl = sc->curgl;
	screen->curgr = sc->curgr;
    } else {
	resetCharsets(screen);
    }

    tw->flags &= ~DECSC_FLAGS;
    tw->flags |= sc->flags & DECSC_FLAGS;
    CursorSet(screen,
	      ((tw->flags & ORIGIN)
	       ? sc->row - screen->top_marg
	       : sc->row),
	      sc->col, tw->flags);

#if OPT_ISO_COLORS
    tw->sgr_foreground = sc->sgr_foreground;
    SGR_Foreground(tw->flags & FG_COLOR ? sc->cur_foreground : -1);
    SGR_Background(tw->flags & BG_COLOR ? sc->cur_background : -1);
#endif
}

/*
 * Move the cursor to the first column of the n-th next line.
 */
void
CursorNextLine(TScreen * screen, int count)
{
    CursorDown(screen, count < 1 ? 1 : count);
    CarriageReturn(screen);
    do_xevents();
}

/*
 * Move the cursor to the first column of the n-th previous line.
 */
void
CursorPrevLine(TScreen * screen, int count)
{
    CursorUp(screen, count < 1 ? 1 : count);
    CarriageReturn(screen);
    do_xevents();
}

#if OPT_TRACE
int
set_cur_row(TScreen * screen, int value)
{
    assert(screen != 0);
    assert(value >= 0);
    assert(value <= screen->max_row);
    screen->cur_row = value;
    return value;
}

int
set_cur_col(TScreen * screen, int value)
{
    assert(screen != 0);
    assert(value >= 0);
    assert(value <= screen->max_col);
    screen->cur_col = value;
    return value;
}
#endif /* OPT_TRACE */

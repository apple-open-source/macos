/*
 *	$Xorg: cursor.c,v 1.3 2000/08/17 19:55:08 cpqbld Exp $
 */

/* $XFree86: xc/programs/xterm/cursor.c,v 3.15 2002/04/28 19:04:20 dickey Exp $ */

/*
 * Copyright 2002 by Thomas E. Dickey
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

/*
 * Clear the selection if the cursor moves "before" the current position. 
 * Moving "after" is ok.
 *
 * That sounds fine - if the cursor really had anything direct relationship to
 * the selection.  For instance, if the cursor moved due to command line
 * editing, it would be nice to deselect.  However, what that means in practice
 * is that a fullscreen program which scrolls back a line will (because it must
 * temporarily reposition the cursor) clear the selection.
 *
 * However, it has an indirect relationship to the selection - we want to
 * prevent the application from changing the screen contents under the
 * highlighted region.
 */
#define _CheckSelection(screen) \
    if ((screen->cur_row < screen->endHRow) || \
	(screen->cur_row == screen->endHRow && \
	 screen->cur_col < screen->endHCol)) \
	DisownSelection(term);

/*
 * Moves the cursor to the specified position, checking for bounds.
 * (this includes scrolling regions)
 * The origin is considered to be 0, 0 for this procedure.
 */
void
CursorSet(register TScreen * screen, register int row, register int col, unsigned flags)
{
    register int maxr;

    col = (col < 0 ? 0 : col);
    screen->cur_col = (col <= screen->max_col ? col : screen->max_col);
    maxr = screen->max_row;
    if (flags & ORIGIN) {
	row += screen->top_marg;
	maxr = screen->bot_marg;
    }
    row = (row < 0 ? 0 : row);
    screen->cur_row = (row <= maxr ? row : maxr);
    screen->do_wrap = 0;
    _CheckSelection(screen);
}

/*
 * moves the cursor left n, no wrap around
 */
void
CursorBack(register TScreen * screen, int n)
{
    register int i, j, k, rev;

    if ((rev = (term->flags & (REVERSEWRAP | WRAPAROUND)) ==
	 (REVERSEWRAP | WRAPAROUND)) != 0
	&& screen->do_wrap)
	n--;
    if ((screen->cur_col -= n) < 0) {
	if (rev) {
	    if ((i = ((j = screen->max_col + 1)
		      * screen->cur_row) + screen->cur_col) < 0) {
		k = j * (screen->max_row + 1);
		i += ((-i) / k + 1) * k;
	    }
	    screen->cur_row = i / j;
	    screen->cur_col = i % j;
	} else
	    screen->cur_col = 0;
    }
    screen->do_wrap = 0;
    _CheckSelection(screen);
}

/*
 * moves the cursor forward n, no wraparound
 */
void
CursorForward(register TScreen * screen, int n)
{
    screen->cur_col += n;
    if (screen->cur_col > CurMaxCol(screen, screen->cur_row))
	screen->cur_col = CurMaxCol(screen, screen->cur_row);
    screen->do_wrap = 0;
    _CheckSelection(screen);
}

/*
 * moves the cursor down n, no scrolling.
 * Won't pass bottom margin or bottom of screen.
 */
void
CursorDown(register TScreen * screen, int n)
{
    register int max;

    max = (screen->cur_row > screen->bot_marg ?
	   screen->max_row : screen->bot_marg);

    screen->cur_row += n;
    if (screen->cur_row > max)
	screen->cur_row = max;
    screen->do_wrap = 0;
    _CheckSelection(screen);
}

/*
 * moves the cursor up n, no linestarving.
 * Won't pass top margin or top of screen.
 */
void
CursorUp(register TScreen * screen, int n)
{
    register int min;

    min = (screen->cur_row < screen->top_marg ?
	   0 : screen->top_marg);

    screen->cur_row -= n;
    if (screen->cur_row < min)
	screen->cur_row = min;
    screen->do_wrap = 0;
    _CheckSelection(screen);
}

/*
 * Moves cursor down amount lines, scrolls if necessary.
 * Won't leave scrolling region. No carriage return.
 */
void
xtermIndex(register TScreen * screen, register int amount)
{
    register int j;

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
RevIndex(register TScreen * screen, register int amount)
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
CarriageReturn(register TScreen * screen)
{
    screen->cur_col = 0;
    screen->do_wrap = 0;
    _CheckSelection(screen);
}

/*
 * Save Cursor and Attributes
 */
void
CursorSave(register XtermWidget tw)
{
    register TScreen *screen = &tw->screen;
    register SavedCursor *sc = &screen->sc[screen->alternate != False];

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
CursorRestore(register XtermWidget tw)
{
    register TScreen *screen = &tw->screen;
    register SavedCursor *sc = &screen->sc[screen->alternate != False];

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

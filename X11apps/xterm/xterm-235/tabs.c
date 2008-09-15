/* $XTermId: tabs.c,v 1.31 2008/02/24 17:35:03 Nemeth Exp $ */

/*
 *	$XFree86: xc/programs/xterm/tabs.c,v 3.14 2006/02/13 01:14:59 dickey Exp $
 */

/*
 * Copyright 2000-2006,2008 by Thomas E. Dickey
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

/* tabs.c */

#include <xterm.h>
#include <data.h>

#define TAB_INDEX(n) ((n) >> TAB_BITS_SHIFT)
#define TAB_MASK(n)  (1 << ((n) & (TAB_BITS_WIDTH-1)))

#define SET_TAB(tabs,n) tabs[TAB_INDEX(n)] |=  TAB_MASK(n)
#define CLR_TAB(tabs,n) tabs[TAB_INDEX(n)] &= ~TAB_MASK(n)
#define TST_TAB(tabs,n) tabs[TAB_INDEX(n)] &   TAB_MASK(n)

/*
 * places tabstops at only every 8 columns
 */
void
TabReset(Tabs tabs)
{
    int i;

    TabZonk(tabs);

    for (i = 0; i < MAX_TABS; i += 8)
	TabSet(tabs, i);
}

/*
 * places a tabstop at col
 */
void
TabSet(Tabs tabs, int col)
{
    if (col >= 0 && col < MAX_TABS) {
	SET_TAB(tabs, col);
    }
}

/*
 * clears a tabstop at col
 */
void
TabClear(Tabs tabs, int col)
{
    if (col >= 0 && col < MAX_TABS) {
	CLR_TAB(tabs, col);
    }
}

/*
 * returns the column of the next tabstop
 * (or MAX_TABS - 1 if there are no more).
 * A tabstop at col is ignored.
 */
static int
TabNext(XtermWidget xw, Tabs tabs, int col)
{
    TScreen *screen = &(xw->screen);

    if (screen->curses && screen->do_wrap && (xw->flags & WRAPAROUND)) {
	xtermIndex(xw, 1);
	set_cur_col(screen, 0);
	col = screen->do_wrap = 0;
    }
    for (++col; col < MAX_TABS; ++col)
	if (TST_TAB(tabs, col))
	    return (col);

    return (MAX_TABS - 1);
}

/*
 * returns the column of the previous tabstop
 * (or 0 if there are no more).
 * A tabstop at col is ignored.
 */
static int
TabPrev(Tabs tabs, int col)
{
    for (--col; col >= 0; --col)
	if (TST_TAB(tabs, col))
	    return (col);

    return (0);
}

/*
 * Tab to the next stop, returning true if the cursor moved
 */
Bool
TabToNextStop(XtermWidget xw)
{
    TScreen *screen = &(xw->screen);
    int saved_column = screen->cur_col;
    int next = TabNext(xw, xw->tabs, screen->cur_col);
    int max = CurMaxCol(screen, screen->cur_row);

    if (next > max)
	next = max;
    set_cur_col(screen, next);

    return (screen->cur_col > saved_column);
}

/*
 * Tab to the previous stop, returning true if the cursor moved
 */
Bool
TabToPrevStop(XtermWidget xw)
{
    TScreen *screen = &(xw->screen);
    int saved_column = screen->cur_col;

    set_cur_col(screen, TabPrev(xw->tabs, screen->cur_col));

    return (screen->cur_col < saved_column);
}

/*
 * clears all tabs
 */
void
TabZonk(Tabs tabs)
{
    memset(tabs, 0, sizeof(*tabs) * TAB_ARRAY_SIZE);
}

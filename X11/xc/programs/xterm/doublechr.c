/*
 * $XFree86: xc/programs/xterm/doublechr.c,v 3.12 2003/10/13 00:58:22 dickey Exp $
 */

/************************************************************

Copyright 1997-2002,2003 by Thomas E. Dickey

                        All Rights Reserved

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. 
IN NO EVENT SHALL THE ABOVE LISTED COPYRIGHT HOLDER(S) BE LIABLE FOR ANY
CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name(s) of the above copyright
holders shall not be used in advertising or otherwise to promote the
sale, use or other dealings in this Software without prior written
authorization.

********************************************************/

#include <xterm.h>
#include <data.h>
#include <fontutils.h>

/*
 * The first column is all that matters for double-size characters (since the
 * controls apply to a whole line).  However, it's easier to maintain the
 * information for special fonts by writing to all cells.
 */
#define curChrSet SCRN_BUF_CSETS(screen, screen->cur_row)[0]

#if OPT_DEC_CHRSET
static void
repaint_line(unsigned newChrSet)
{
    register TScreen *screen = &term->screen;
    int curcol = screen->cur_col;
    int currow = screen->cur_row;
    int len = screen->max_col + 1;
    int width = len;
    unsigned oldChrSet = SCRN_BUF_CSETS(screen, currow)[0];

    /*
     * Ignore repetition.
     */
    if (oldChrSet == newChrSet)
	return;

    TRACE(("repaint_line(%2d,%2d) (%d)\n", currow, screen->cur_col, newChrSet));
    HideCursor();

    /* If switching from single-width, keep the cursor in the visible part
     * of the line.
     */
    if (CSET_DOUBLE(newChrSet)) {
	width /= 2;
	if (curcol > width)
	    curcol = width;
    }

    /*
     * ScrnRefresh won't paint blanks for us if we're switching between a
     * single-size and double-size font.
     */
    if (CSET_DOUBLE(oldChrSet) != CSET_DOUBLE(newChrSet)) {
	ClearCurBackground(
			      screen,
			      CursorY(screen, currow),
			      CurCursorX(screen, currow, 0),
			      FontHeight(screen),
			      len * CurFontWidth(screen, currow));
    }

    /* FIXME: do VT220 softchars allow double-sizes? */
    memset(SCRN_BUF_CSETS(screen, currow), newChrSet, len);

    screen->cur_col = 0;
    ScrnRefresh(screen, currow, 0, 1, len, True);
    screen->cur_col = curcol;
}
#endif

/*
 * Set the line to double-height characters.  The 'top' flag denotes whether
 * we'll be using it for the top (true) or bottom (false) of the line.
 */
void
xterm_DECDHL(Bool top)
{
#if OPT_DEC_CHRSET
    repaint_line(top ? CSET_DHL_TOP : CSET_DHL_BOT);
#endif
}

/*
 * Set the line to single-width characters (the normal state).
 */
void
xterm_DECSWL(void)
{
#if OPT_DEC_CHRSET
    repaint_line(CSET_SWL);
#endif
}

/*
 * Set the line to double-width characters
 */
void
xterm_DECDWL(void)
{
#if OPT_DEC_CHRSET
    repaint_line(CSET_DWL);
#endif
}

#if OPT_DEC_CHRSET
static void
discard_font(TScreen * screen, XTermFonts * data)
{
    TRACE(("discard_font chrset=%d %s\n", data->chrset,
	   (data->fn != 0) ? data->fn : "<no-name>"));

    data->chrset = 0;
    data->flags = 0;
    if (data->gc != 0) {
	XFreeGC(screen->display, data->gc);
	data->gc = 0;
    }
    if (data->fn != 0) {
	free(data->fn);
	data->fn = 0;
    }
    if (data->fs != 0) {
	XFreeFont(screen->display, data->fs);
	data->fs = 0;
    }
}

int
xterm_Double_index(unsigned chrset, unsigned flags)
{
    int n;
    TScreen *screen = &term->screen;
    XTermFonts *data = screen->double_fonts;

    flags &= BOLD;
    TRACE(("xterm_Double_index chrset=%#x, flags=%#x\n", chrset, flags));

    for (n = 0; n < screen->fonts_used; n++) {
	if (data[n].chrset == chrset
	    && data[n].flags == flags) {
	    if (n != 0) {
		XTermFonts save;
		TRACE(("...xterm_Double_index -> %d (OLD:%d)\n", n, screen->fonts_used));
		save = data[n];
		while (n > 0) {
		    data[n] = data[n - 1];
		    n--;
		}
		data[n] = save;
	    }
	    return n;
	}
    }

    /* Not, found, push back existing fonts and create a new entry */
    if (screen->fonts_used >= screen->cache_doublesize) {
	TRACE(("...xterm_Double_index: discard oldest\n"));
	discard_font(screen, &(data[screen->fonts_used - 1]));
    } else {
	screen->fonts_used += 1;
    }
    for (n = screen->fonts_used; n > 0; n--)
	data[n] = data[n - 1];

    TRACE(("...xterm_Double_index -> (NEW:%d)\n", screen->fonts_used));

    data[0].chrset = chrset;
    data[0].flags = flags;
    data[0].fn = 0;
    data[0].fs = 0;
    data[0].gc = 0;
    return 0;
}

/*
 * Lookup/cache a GC for the double-size character display.  We save up to
 * NUM_CHRSET values.
 */
GC
xterm_DoubleGC(unsigned chrset, unsigned flags, GC old_gc)
{
    XGCValues gcv;
    register TScreen *screen = &term->screen;
    unsigned long mask = (GCForeground | GCBackground | GCFont);
    int n;
    char *name;
    XTermFonts *data;

    if ((name = xtermSpecialFont(flags, chrset)) == 0)
	return 0;

    n = xterm_Double_index(chrset, flags);
    data = &(screen->double_fonts[n]);
    if (data->fn != 0) {
	if (!strcmp(data->fn, name)) {
	    if (data->fs != 0) {
		XCopyGC(screen->display, old_gc, ~GCFont, data->gc);
		return data->gc;
	    }
	}
	discard_font(screen, data);
	data->chrset = chrset;
	data->flags = flags & BOLD;
    }
    data->fn = name;

    TRACE(("xterm_DoubleGC %s %d: %s\n", flags & BOLD ? "BOLD" : "NORM", n, name));

    if ((data->fs = XLoadQueryFont(screen->display, name)) == 0) {
	/* Retry with * in resolutions */
	char *nname = xtermSpecialFont(flags | NORESOLUTION, chrset);

	if (!nname)
	    return 0;
	if ((data->fs = XLoadQueryFont(screen->display, nname)) == 0) {
	    XtFree(nname);
	    return 0;
	}
	XtFree(name);
	data->fn = nname;
    }

    TRACE(("-> OK\n"));

    gcv.graphics_exposures = TRUE;	/* default */
    gcv.font = data->fs->fid;
    gcv.foreground = screen->foreground;
    gcv.background = term->core.background_pixel;

    data->gc = XCreateGC(screen->display, VWindow(screen), mask, &gcv);
    XCopyGC(screen->display, old_gc, ~GCFont, data->gc);
    return data->gc;
}
#endif

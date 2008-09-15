/* $XTermId: doublechr.c,v 1.60 2008/01/27 15:07:16 tom Exp $ */

/************************************************************

Copyright 1997-2007,2008 by Thomas E. Dickey

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

#include <assert.h>

#define WhichCgsId(flag) (((flag) & BOLD) ? gcCBold : gcCNorm)

/*
 * The first column is all that matters for double-size characters (since the
 * controls apply to a whole line).  However, it's easier to maintain the
 * information for special fonts by writing to all cells.
 */
#define curChrSet SCRN_BUF_CSETS(screen, screen->cur_row)[0]

#if OPT_DEC_CHRSET

static void
repaint_line(XtermWidget xw, unsigned newChrSet)
{
    register TScreen *screen = &xw->screen;
    int curcol = screen->cur_col;
    int currow = screen->cur_row;
    unsigned len = MaxCols(screen);
    int width = len;
    unsigned oldChrSet = SCRN_BUF_CSETS(screen, currow)[0];

    assert(width > 0);

    /*
     * Ignore repetition.
     */
    if (oldChrSet == newChrSet)
	return;

    TRACE(("repaint_line(%2d,%2d) (%s -> %s)\n", currow, screen->cur_col,
	   visibleChrsetName(oldChrSet),
	   visibleChrsetName(newChrSet)));
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
     * single-size and double-size font.  So we paint our own.
     */
    ClearCurBackground(xw,
		       CursorY(screen, currow),
		       CurCursorX(screen, currow, 0),
		       (unsigned) FontHeight(screen),
		       len * CurFontWidth(screen, currow));

    /* FIXME: do VT220 softchars allow double-sizes? */
    memset(SCRN_BUF_CSETS(screen, currow), (Char) newChrSet, len);

    set_cur_col(screen, 0);
    ScrnUpdate(xw, currow, 0, 1, (int) len, True);
    set_cur_col(screen, curcol);
}
#endif

/*
 * Set the line to double-height characters.  The 'top' flag denotes whether
 * we'll be using it for the top (true) or bottom (false) of the line.
 */
void
xterm_DECDHL(XtermWidget xw GCC_UNUSED, Bool top)
{
#if OPT_DEC_CHRSET
    repaint_line(xw, (unsigned) (top ? CSET_DHL_TOP : CSET_DHL_BOT));
#else
    (void) top;
#endif
}

/*
 * Set the line to single-width characters (the normal state).
 */
void
xterm_DECSWL(XtermWidget xw GCC_UNUSED)
{
#if OPT_DEC_CHRSET
    repaint_line(xw, CSET_SWL);
#endif
}

/*
 * Set the line to double-width characters
 */
void
xterm_DECDWL(XtermWidget xw GCC_UNUSED)
{
#if OPT_DEC_CHRSET
    repaint_line(xw, CSET_DWL);
#endif
}

#if OPT_DEC_CHRSET
static void
discard_font(XtermWidget xw, int n)
{
    TScreen *screen = &xw->screen;
    XTermFonts *data = &(screen->double_fonts[n]);

    TRACE(("discard_font chrset=%d %s\n", data->chrset,
	   (data->fn != 0) ? data->fn : "<no-name>"));

    data->chrset = 0;
    data->flags = 0;
    if (data->fn != 0) {
	free(data->fn);
	data->fn = 0;
    }
    (void) xtermCloseFont(xw, data);

    screen->fonts_used -= 1;
    while (n < screen->fonts_used) {
	screen->double_fonts[n] = screen->double_fonts[n + 1];
	++n;
    }
}

/* push back existing fonts and create a new entry */
static XTermFonts *
pushback_font(XtermWidget xw, XTermFonts * source)
{
    TScreen *screen = &xw->screen;
    XTermFonts *data = screen->double_fonts;
    int n;

    if (screen->fonts_used >= screen->cache_doublesize) {
	TRACE(("pushback_font: discard oldest\n"));
	discard_font(xw, screen->fonts_used - 1);
    } else {
	screen->fonts_used += 1;
    }

    for (n = screen->fonts_used; n > 0; n--)
	data[n] = data[n - 1];
    data[0] = *source;

    TRACE(("pushback_font -> (NEW:%d)\n", screen->fonts_used));

    return data;
}

int
xterm_Double_index(XtermWidget xw, unsigned chrset, unsigned flags)
{
    int n;
    int result = -1;
    TScreen *screen = &xw->screen;
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
	    result = n;
	    break;
	}
    }

    return result;
}

/*
 * Lookup/cache a GC for the double-size character display.  We save up to
 * NUM_CHRSET values.
 */
GC
xterm_DoubleGC(XtermWidget xw,
	       unsigned chrset,
	       unsigned flags,
	       GC old_gc,
	       int *inxp)
{
    TScreen *screen = &(xw->screen);
    VTwin *cgsWin = WhichVWin(screen);
    int n;
    char *name;
    XTermFonts *data = 0;
    GC result = 0;

    if ((name = xtermSpecialFont(screen, flags, chrset)) != 0) {
	CgsEnum cgsId = WhichCgsId(flags);
	Boolean found = False;

	if ((n = xterm_Double_index(xw, chrset, flags)) >= 0) {
	    data = &(screen->double_fonts[n]);
	    if (data->fn != 0) {
		if (!strcmp(data->fn, name)
		    && data->fs != 0) {
		    found = True;
		    free(name);
		} else {
		    discard_font(xw, n);
		}
	    }
	}

	if (!found) {
	    XTermFonts temp;

	    TRACE(("xterm_DoubleGC %s %d: %s\n",
		   flags & BOLD ? "BOLD" : "NORM", n, name));

	    memset(&temp, 0, sizeof(temp));
	    temp.fn = name;
	    temp.chrset = chrset;
	    temp.flags = (flags & BOLD);

	    if (!xtermOpenFont(xw, name, &temp)) {
		/* Retry with * in resolutions */
		char *nname = xtermSpecialFont(screen, flags | NORESOLUTION, chrset);

		if (nname != 0) {
		    found = xtermOpenFont(xw, nname, &temp);
		    free(nname);
		}
	    } else {
		found = True;
	    }
	    free(name);

	    if (found) {
		n = 0;
		data = pushback_font(xw, &temp);
	    }

	    TRACE(("-> %s\n", found ? "OK" : "FAIL"));
	}

	if (found) {
	    setCgsCSet(xw, cgsWin, cgsId, chrset);
	    setCgsFont(xw, cgsWin, cgsId, data);
	    setCgsFore(xw, cgsWin, cgsId, getCgsFore(xw, cgsWin, old_gc));
	    setCgsBack(xw, cgsWin, cgsId, getCgsBack(xw, cgsWin, old_gc));
	    result = getCgsGC(xw, cgsWin, cgsId);
	    *inxp = n;
	} else if (flags & BOLD) {
	    flags &= ~BOLD;
	    result = xterm_DoubleGC(xw, chrset, flags, old_gc, inxp);
	}
    }

    return result;
}
#endif

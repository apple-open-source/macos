/* $XTermId: linedata.c,v 1.72 2009/09/28 23:58:59 tom Exp $ */

/************************************************************

Copyright 2009 by Thomas E. Dickey

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
#include <data.h>		/* FIXME - needed for 'term' */

#include <assert.h>

/*
 * Given a row-number, find the corresponding data for the line in the VT100
 * widget.  Row numbers can be positive or negative.
 *
 * If the data comes from the scrollback, defer that to getScrollback().
 */
LineData *
getLineData(TScreen * screen, int row)
{
    LineData *result = 0;
    ScrnBuf buffer;

    if (row >= 0) {
	buffer = screen->visbuf;
    } else {
#if OPT_FIFO_LINES
	buffer = 0;
	result = getScrollback(screen, row);
#else
	buffer = screen->saveBuf_index;
	row += screen->savelines;
#endif
    }
    if (row >= 0) {
	assert(row <= screen->max_row);
	result = (LineData *) scrnHeadAddr(screen, buffer, (unsigned) row);
	if (result != 0) {
#if 1				/* FIXME - these should be done in setupLineData, etc. */
	    result->lineSize = (Dimension) MaxCols(screen);
#if OPT_WIDE_CHARS
	    if (screen->wide_chars) {
		result->combSize = (Char) screen->max_combining;
	    } else {
		result->combSize = 0;
	    }
#endif
#endif /* FIXME */
	}
    }

    return result;
}

/*
 * Copy line's data, e.g., from one screen buffer to another, given the preset
 * pointers for the destination.
 *
 * TODO: optionally prune unused combining character data from the result.
 */
void
copyLineData(LineData * dst, LineData * src)
{
    dst->bufHead = src->bufHead;

#if OPT_WIDE_CHARS
    dst->combSize = src->combSize;
#endif

    /*
     * Usually we're copying the same-sized line; a memcpy is faster than
     * several loops.
     */
    if (dst->lineSize == src->lineSize) {
	size_t size = (sizeof(dst->attribs[0])
#if OPT_ISO_COLORS
		       + sizeof(dst->color[0])
#endif
		       + sizeof(dst->charData[0])
#if OPT_WIDE_CHARS
		       + sizeof(dst->combData[0][0]) * dst->combSize
#endif
	);

	memcpy(dst->attribs, src->attribs, size * dst->lineSize);
    } else {
	Dimension col;
	Dimension limit = ((dst->lineSize < src->lineSize)
			   ? dst->lineSize
			   : src->lineSize);
#if OPT_WIDE_CHARS
	Char comb;
#endif

	for (col = 0; col < limit; ++col) {
	    dst->attribs[col] = src->attribs[col];
#if OPT_ISO_COLORS
	    dst->color[col] = src->color[col];
#endif
	    dst->charData[col] = src->charData[col];
#if OPT_WIDE_CHARS
	    for (comb = 0; comb < dst->combSize; ++comb) {
		dst->combData[comb][col] = src->combData[comb][col];
	    }
#endif
	}
	for (col = limit; col < dst->lineSize; ++col) {
	    dst->attribs[col] = 0;
#if OPT_ISO_COLORS
	    dst->color[col] = 0;
#endif
	    dst->charData[col] = 0;
#if OPT_WIDE_CHARS
	    for (comb = 0; comb < dst->combSize; ++comb) {
		dst->combData[comb][col] = 0;
	    }
#endif
	}
    }
}

#if OPT_WIDE_CHARS
#define initLineExtra(screen) \
    screen->lineExtra = ((size_t) (screen->max_combining) * sizeof(IChar *))
#else
#define initLineExtra(screen) \
    screen->lineExtra = 0
#endif

void
initLineData(XtermWidget xw)
{
    TScreen *screen = &(xw->screen);

    initLineExtra(screen);

    TRACE(("initLineData %d\n", screen->lineExtra));
    TRACE(("...sizeof(LineData)  %d\n", sizeof(LineData)));
#if OPT_ISO_COLORS
    TRACE(("...sizeof(CellColor) %d\n", sizeof(CellColor)));
#endif
    TRACE(("...sizeof(RowData)   %d\n", sizeof(RowData)));
    TRACE(("...offset(lineSize)  %d\n", offsetof(LineData, lineSize)));
    TRACE(("...offset(bufHead)   %d\n", offsetof(LineData, bufHead)));
#if OPT_WIDE_CHARS
    TRACE(("...offset(combSize)  %d\n", offsetof(LineData, combSize)));
#endif
    TRACE(("...offset(attribs)   %d\n", offsetof(LineData, attribs)));
#if OPT_ISO_COLORS
    TRACE(("...offset(color)     %d\n", offsetof(LineData, color)));
#endif
    TRACE(("...offset(charData)  %d\n", offsetof(LineData, charData)));
    TRACE(("...offset(combData)  %d\n", offsetof(LineData, combData)));
}

/*
 * CellData size depends on the "combiningChars" resource.
 * FIXME - revise this to reduce arithmetic...
 */
#define CellDataSize(screen) (SizeOfCellData + screen->lineExtra)

#define CellDataAddr(screen, data, cell) \
	(CellData *)((char *)data + (cell * CellDataSize(screen)))

CellData *
newCellData(XtermWidget xw, Cardinal count)
{
    CellData *result;
    TScreen *screen = &(xw->screen);

    initLineExtra(screen);
    result = (CellData *) calloc((size_t) count, (size_t) CellDataSize(screen));
    return result;
}

void
saveCellData(TScreen * screen,
	     CellData * data,
	     Cardinal cell,
	     LineData * ld,
	     int column)
{
    CellData *item = CellDataAddr(screen, data, cell);

    if (column < MaxCols(screen)) {
	item->attribs = ld->attribs[column];
#if OPT_ISO_COLORS
	item->color = ld->color[column];
#endif
	item->charData = ld->charData[column];
	if_OPT_WIDE_CHARS(screen, {
	    size_t off;
	    item->combSize = ld->combSize;
	    for_each_combData(off, ld) {
		item->combData[off] = ld->combData[off][column];
	    }
	})
    }
}

void
restoreCellData(TScreen * screen,
		CellData * data,
		Cardinal cell,
		LineData * ld,
		int column)
{
    CellData *item = CellDataAddr(screen, data, cell);

    if (column < MaxCols(screen)) {
	ld->attribs[column] = item->attribs;
#if OPT_ISO_COLORS
	ld->color[column] = item->color;
#endif
	ld->charData[column] = item->charData;
	if_OPT_WIDE_CHARS(screen, {
	    size_t off;
	    ld->combSize = item->combSize;
	    for_each_combData(off, ld) {
		ld->combData[off][column] = item->combData[off];
	    }
	})
    }
}

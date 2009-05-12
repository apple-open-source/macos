/* $XTermId: screen.c,v 1.241 2008/04/20 21:07:10 tom Exp $ */

/*
 * Copyright 1999-2007,2008 by Thomas E. Dickey
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

/* screen.c */

#include <stdio.h>
#include <xterm.h>
#include <error.h>
#include <data.h>
#include <xcharmouse.h>
#include <xterm_io.h>

#if OPT_WIDE_CHARS
#include <fontutils.h>
#include <menu.h>
#endif

#include <assert.h>
#include <signal.h>

#define getMinRow(screen) ((xw->flags & ORIGIN) ? (screen)->top_marg : 0)
#define getMaxRow(screen) ((xw->flags & ORIGIN) ? (screen)->bot_marg : (screen)->max_row)
#define getMinCol(screen) 0
#define getMaxCol(screen) ((screen)->max_col)

/*
 * Allocates memory for a 2-dimensional array of chars and returns a pointer
 * thereto.  Each line is formed from a set of char arrays, with an index
 * (i.e., the ScrnBuf type).  The first pointer in the index is reserved for
 * per-line flags, and does not point to data.
 *
 * After the per-line flags, we have a series of pointers to char arrays:  The
 * first one is the actual character array, the second one is the attributes,
 * the third is the foreground and background colors, and the fourth denotes
 * the character set.
 *
 * We store it all as pointers, because of alignment considerations, together
 * with the intention of being able to change the total number of pointers per
 * row according to whether the user wants color or not.
 */
ScrnBuf
Allocate(int nrow, int ncol, Char ** addr)
{
    ScrnBuf base;
    Char *tmp;
    int i, j, k;
    size_t entries = MAX_PTRS * nrow;
    size_t length = BUF_PTRS * nrow * ncol;

    if ((base = TypeCallocN(ScrnPtr, entries)) == 0)
	SysError(ERROR_SCALLOC);

    if ((tmp = TypeCallocN(Char, length)) == 0)
	SysError(ERROR_SCALLOC2);

    *addr = tmp;
    for (i = k = 0; i < nrow; i++) {
	base[k] = 0;		/* per-line flags */
	k += BUF_HEAD;
	for (j = BUF_HEAD; j < MAX_PTRS; j++) {
	    base[k++] = tmp;
	    tmp += ncol;
	}
    }

    return (base);
}

/*
 *  This is called when the screen is resized.
 *  Returns the number of lines the text was moved down (neg for up).
 *  (Return value only necessary with SouthWestGravity.)
 */
static int
Reallocate(XtermWidget xw,
	   ScrnBuf * sbuf,
	   Char ** sbufaddr,
	   int nrow,
	   int ncol,
	   int oldrow,
	   int oldcol)
{
    ScrnBuf base;
    Char *tmp;
    int i, j, k, minrows;
    size_t mincols;
    Char *oldbuf;
    int move_down = 0, move_up = 0;
    size_t entries = MAX_PTRS * nrow;
    size_t length = BUF_PTRS * nrow * ncol;

    if (sbuf == NULL || *sbuf == NULL) {
	return 0;
    }

    oldbuf = *sbufaddr;

    /*
     * Special case if oldcol == ncol - straight forward realloc and
     * update of the additional lines in sbuf
     *
     * FIXME: this is a good idea, but doesn't seem to be implemented.
     * -gildea
     */

    /*
     * realloc sbuf, the pointers to all the lines.
     * If the screen shrinks, remove lines off the top of the buffer
     * if resizeGravity resource says to do so.
     */
    if (nrow < oldrow
	&& xw->misc.resizeGravity == SouthWestGravity) {
	/* Remove lines off the top of the buffer if necessary. */
	move_up = (oldrow - nrow)
	    - (xw->screen.max_row - xw->screen.cur_row);
	if (move_up < 0)
	    move_up = 0;
	/* Overlapping memmove here! */
	memmove(*sbuf, *sbuf + (move_up * MAX_PTRS),
		MAX_PTRS * (oldrow - move_up) * sizeof((*sbuf)[0]));
    }
    *sbuf = TypeRealloc(ScrnPtr, entries, *sbuf);
    if (*sbuf == 0)
	SysError(ERROR_RESIZE);
    base = *sbuf;

    /*
     *  create the new buffer space and copy old buffer contents there
     *  line by line.
     */
    if ((tmp = TypeCallocN(Char, length)) == 0)
	SysError(ERROR_SREALLOC);
    *sbufaddr = tmp;
    minrows = (oldrow < nrow) ? oldrow : nrow;
    mincols = (oldcol < ncol) ? oldcol : ncol;
    if (nrow > oldrow
	&& xw->misc.resizeGravity == SouthWestGravity) {
	/* move data down to bottom of expanded screen */
	move_down = Min(nrow - oldrow, xw->screen.savedlines);
	tmp += (ncol * move_down * BUF_PTRS);
    }

    for (i = k = 0; i < minrows; i++) {
	k += BUF_HEAD;
	for (j = BUF_HEAD; j < MAX_PTRS; j++) {
	    memcpy(tmp, base[k++], mincols);
	    tmp += ncol;
	}
    }

    /*
     * update the pointers in sbuf
     */
    for (i = k = 0, tmp = *sbufaddr; i < nrow; i++) {
	for (j = 0; j < BUF_HEAD; j++)
	    base[k++] = 0;
	for (j = BUF_HEAD; j < MAX_PTRS; j++) {
	    base[k++] = tmp;
	    tmp += ncol;
	}
    }

    /* Now free the old buffer */
    free(oldbuf);

    return move_down ? move_down : -move_up;	/* convert to rows */
}

#if OPT_WIDE_CHARS
#if 0
static void
dump_screen(const char *tag,
	    XtermWidget xw,
	    ScrnBuf sbuf,
	    Char * sbufaddr,
	    unsigned nrow,
	    unsigned ncol)
{
    unsigned y, x;

    TRACE(("DUMP %s, ptrs %d\n", tag, xw->num_ptrs));
    TRACE(("  sbuf      %p\n", sbuf));
    TRACE(("  sbufaddr  %p\n", sbufaddr));
    TRACE(("  nrow      %d\n", nrow));
    TRACE(("  ncol      %d\n", ncol));

    for (y = 0; y < nrow; ++y) {
	ScrnPtr ptr = BUF_CHARS(sbuf, y);
	TRACE(("%3d:%p:", y, ptr));
	for (x = 0; x < ncol; ++x) {
	    Char c = ptr[x];
	    if (c == 0)
		c = '~';
	    TRACE(("%c", c));
	}
	TRACE(("\n"));
    }
}
#else
#define dump_screen(tag, xw, sbuf, sbufaddr, nrow, ncol)	/* nothing */
#endif

/*
 * This function reallocates memory if changing the number of Buf offsets.
 * The code is based on Reallocate().
 */
static void
ReallocateBufOffsets(XtermWidget xw,
		     ScrnBuf * sbuf,
		     Char ** sbufaddr,
		     unsigned nrow,
		     unsigned ncol,
		     size_t new_max_offsets)
{
    unsigned i;
    int j, k;
    ScrnBuf base;
    Char *oldbuf, *tmp;
    size_t entries, length;
    /*
     * As there are 2 buffers (allbuf, altbuf), we cannot change num_ptrs in
     * this function.  However MAX_PTRS and BUF_PTRS depend on num_ptrs so
     * change it now and restore the value when done.
     */
    int old_max_ptrs = MAX_PTRS;

    assert(nrow != 0);
    assert(ncol != 0);
    assert(new_max_offsets != 0);

    dump_screen("before", xw, *sbuf, *sbufaddr, nrow, ncol);

    xw->num_ptrs = new_max_offsets;

    entries = MAX_PTRS * nrow;
    length = BUF_PTRS * nrow * ncol;
    oldbuf = *sbufaddr;

    *sbuf = TypeRealloc(ScrnPtr, entries, *sbuf);
    if (*sbuf == 0)
	SysError(ERROR_RESIZE);
    base = *sbuf;

    if ((tmp = TypeCallocN(Char, length)) == 0)
	SysError(ERROR_SREALLOC);
    *sbufaddr = tmp;

    for (i = k = 0; i < nrow; i++) {
	k += BUF_HEAD;
	for (j = BUF_HEAD; j < old_max_ptrs; j++) {
	    memcpy(tmp, base[k++], ncol);
	    tmp += ncol;
	}
	tmp += ncol * (new_max_offsets - old_max_ptrs);
    }

    /*
     * update the pointers in sbuf
     */
    for (i = k = 0, tmp = *sbufaddr; i < nrow; i++) {
	for (j = 0; j < BUF_HEAD; j++)
	    base[k++] = 0;
	for (j = BUF_HEAD; j < MAX_PTRS; j++) {
	    base[k++] = tmp;
	    tmp += ncol;
	}
    }

    /* Now free the old buffer and restore num_ptrs */
    free(oldbuf);
    dump_screen("after", xw, *sbuf, *sbufaddr, nrow, ncol);

    xw->num_ptrs = old_max_ptrs;
}

/*
 * This function dynamically adds support for wide-characters.
 */
void
ChangeToWide(XtermWidget xw)
{
    TScreen *screen = &(xw->screen);
    unsigned new_bufoffset = OFF_FINAL + (screen->max_combining * 2);
    int savelines = screen->scrollWidget ? screen->savelines : 0;

    if (screen->wide_chars)
	return;

    TRACE(("ChangeToWide\n"));
    if (xtermLoadWideFonts(xw, True)) {
	if (savelines < 0)
	    savelines = 0;

	/*
	 * If we're displaying the alternate screen, switch the pointers back
	 * temporarily so ReallocateBufOffsets() will operate on the proper
	 * data in altbuf.
	 */
	if (screen->alternate)
	    SwitchBufPtrs(screen);

	ReallocateBufOffsets(xw,
			     &screen->allbuf, &screen->sbuf_address,
			     (unsigned) (MaxRows(screen) + savelines),
			     (unsigned) MaxCols(screen),
			     new_bufoffset);
	if (screen->altbuf) {
	    ReallocateBufOffsets(xw,
				 &screen->altbuf, &screen->abuf_address,
				 (unsigned) MaxRows(screen),
				 (unsigned) MaxCols(screen),
				 new_bufoffset);
	}

	screen->wide_chars = True;
	xw->num_ptrs = new_bufoffset;
	screen->visbuf = &screen->allbuf[MAX_PTRS * savelines];

	/*
	 * Switch the pointers back before we start painting on the screen.
	 */
	if (screen->alternate)
	    SwitchBufPtrs(screen);

	update_font_utf8_mode();
	SetVTFont(xw, screen->menu_font_number, True, NULL);
    }
    TRACE(("...ChangeToWide\n"));
}
#endif

/*
 * Clear cells, no side-effects.
 */
void
ClearCells(XtermWidget xw, int flags, unsigned len, int row, int col)
{
    if (len != 0) {
	TScreen *screen = &(xw->screen);
	flags |= TERM_COLOR_FLAGS(xw);

	memset(SCRN_BUF_CHARS(screen, row) + col, ' ', len);
	memset(SCRN_BUF_ATTRS(screen, row) + col, flags, len);

	if_OPT_EXT_COLORS(screen, {
	    memset(SCRN_BUF_FGRND(screen, row) + col,
		   xw->sgr_foreground, len);
	    memset(SCRN_BUF_BGRND(screen, row) + col,
		   xw->cur_background, len);
	});
	if_OPT_ISO_TRADITIONAL_COLORS(screen, {
	    memset(SCRN_BUF_COLOR(screen, row) + col,
		   (int) xtermColorPair(xw), len);
	});
	if_OPT_DEC_CHRSET({
	    memset(SCRN_BUF_CSETS(screen, row) + col,
		   curXtermChrSet(xw, row), len);
	});
	if_OPT_WIDE_CHARS(screen, {
	    int off;
	    for (off = OFF_WIDEC; off < MAX_PTRS; ++off) {
		memset(SCREEN_PTR(screen, row, off) + col, 0, len);
	    }
	});
    }
}

/*
 * Clear data in the screen-structure (no I/O).
 * Check for wide-character damage as well, clearing the damaged cells.
 */
void
ScrnClearCells(XtermWidget xw, int row, int col, unsigned len)
{
#if OPT_WIDE_CHARS
    TScreen *screen = &(xw->screen);
#endif
    int flags = 0;

    if_OPT_WIDE_CHARS(screen, {
	int kl;
	int kr;
	if (DamagedCells(screen, len, &kl, &kr, INX2ROW(screen, row), col)
	    && kr >= kl) {
	    ClearCells(xw, flags, (unsigned) (kr - kl + 1), row, kl);
	}
    });
    ClearCells(xw, flags, len, row, col);
}

/*
 * Disown the selection and repaint the area that is highlighted so it is no
 * longer highlighted.
 */
void
ScrnDisownSelection(XtermWidget xw)
{
    if (ScrnHaveSelection(&(xw->screen))) {
	if (xw->screen.keepSelection) {
	    UnhiliteSelection(xw);
	} else {
	    DisownSelection(xw);
	}
    }
}

/*
 * Writes str into buf at screen's current row and column.  Characters are set
 * to match flags.
 */
void
ScrnWriteText(XtermWidget xw,
	      PAIRED_CHARS(Char * str, Char * str2),
	      unsigned flags,
	      unsigned cur_fg_bg,
	      unsigned length)
{
    TScreen *screen = &(xw->screen);
#if OPT_ISO_COLORS
#if OPT_EXT_COLORS
    Char *fbf = 0;
    Char *fbb = 0;
#else
    Char *fb = 0;
#endif
#endif
#if OPT_DEC_CHRSET
    Char *cb = 0;
#endif
    Char *attrs;
    int avail = MaxCols(screen) - screen->cur_col;
    Char *chars;
#if OPT_WIDE_CHARS
    Char starcol1, starcol2;
#endif
    unsigned real_width = visual_width(PAIRED_CHARS(str, str2), length);

    (void) cur_fg_bg;

    if (avail <= 0)
	return;
    if (length > (unsigned) avail)
	length = avail;
    if (length == 0 || real_width == 0)
	return;

    chars = SCRN_BUF_CHARS(screen, screen->cur_row) + screen->cur_col;
    attrs = SCRN_BUF_ATTRS(screen, screen->cur_row) + screen->cur_col;

    if_OPT_EXT_COLORS(screen, {
	fbf = SCRN_BUF_FGRND(screen, screen->cur_row) + screen->cur_col;
	fbb = SCRN_BUF_BGRND(screen, screen->cur_row) + screen->cur_col;
    });
    if_OPT_ISO_TRADITIONAL_COLORS(screen, {
	fb = SCRN_BUF_COLOR(screen, screen->cur_row) + screen->cur_col;
    });
    if_OPT_DEC_CHRSET({
	cb = SCRN_BUF_CSETS(screen, screen->cur_row) + screen->cur_col;
    });

#if OPT_WIDE_CHARS
    starcol1 = *chars;
    starcol2 = chars[length - 1];
#endif

    /* write blanks if we're writing invisible text */
    if (flags & INVISIBLE) {
	memset(chars, ' ', length);
    } else {
	memcpy(chars, str, length);	/* This can stand for the present. If it
					   is wrong, we will scribble over it */
    }

#if OPT_BLINK_TEXT
    if ((flags & BLINK) && !(screen->blink_as_bold)) {
	ScrnSetBlinked(screen, screen->cur_row);
    }
#endif

#define ERROR_1 0x20
#define ERROR_2 0x00
    if_OPT_WIDE_CHARS(screen, {

	Char *char2;

	if (real_width != length) {
	    Char *char1 = chars;
	    char2 = SCRN_BUF_WIDEC(screen, screen->cur_row);
	    char2 += screen->cur_col;
	    if (screen->cur_col && starcol1 == HIDDEN_LO && *char2 == HIDDEN_HI
		&& iswide(PACK_PAIR(char1, char2, -1))) {
		char1[-1] = ERROR_1;
		char2[-1] = ERROR_2;
	    }
	    /* if we are overwriting the right hand half of a
	       wide character, make the other half vanish */
	    while (length) {
		int ch = PACK_PAIR(str, str2, 0);

		*char1 = *str;
		char1++;
		str++;

		if (str2) {
		    *char2 = *str2;
		    str2++;
		} else
		    *char2 = 0;
		char2++;
		length--;

		if (iswide(ch)) {
		    *char1 = HIDDEN_LO;
		    *char2 = HIDDEN_HI;
		    char1++;
		    char2++;
		}
	    }

	    if (*char1 == HIDDEN_LO
		&& *char2 == HIDDEN_HI
		&& char1[-1] == HIDDEN_LO
		&& char2[-1] == HIDDEN_HI) {
		*char1 = ERROR_1;
		*char2 = ERROR_2;
	    }
	    /* if we are overwriting the left hand half of a
	       wide character, make the other half vanish */
	} else {

	    if ((char2 = SCRN_BUF_WIDEC(screen, screen->cur_row)) != 0) {
		char2 += screen->cur_col;
		if (screen->cur_col && starcol1 == HIDDEN_LO && *char2 == HIDDEN_HI
		    && iswide(PACK_PAIR(chars, char2, -1))) {
		    chars[-1] = ERROR_1;
		    char2[-1] = ERROR_2;
		}
		/* if we are overwriting the right hand half of a
		   wide character, make the other half vanish */
		if (chars[length] == HIDDEN_LO && char2[length] == HIDDEN_HI &&
		    iswide(PACK_PAIR(chars, char2, length - 1))) {
		    chars[length] = ERROR_1;
		    char2[length] = ERROR_2;
		}
		/* if we are overwriting the left hand half of a
		   wide character, make the other half vanish */
		if ((flags & INVISIBLE) || (str2 == 0))
		    memset(char2, 0, length);
		else
		    memcpy(char2, str2, length);
	    }
	}
    });

    flags &= ATTRIBUTES;
    flags |= CHARDRAWN;
    memset(attrs, (Char) flags, real_width);

    if_OPT_WIDE_CHARS(screen, {
	int off;
	for (off = OFF_FINAL; off < MAX_PTRS; ++off) {
	    memset(SCREEN_PTR(screen,
			      screen->cur_row,
			      off) + screen->cur_col,
		   0, real_width);
	}
    });
    if_OPT_EXT_COLORS(screen, {
	memset(fbf, (int) ExtractForeground(cur_fg_bg), real_width);
	memset(fbb, (int) ExtractBackground(cur_fg_bg), real_width);
    });
    if_OPT_ISO_TRADITIONAL_COLORS(screen, {
	memset(fb, (int) cur_fg_bg, real_width);
    });
    if_OPT_DEC_CHRSET({
	memset(cb, curXtermChrSet(xw, screen->cur_row), real_width);
    });

    if_OPT_WIDE_CHARS(screen, {
	screen->last_written_col = screen->cur_col + real_width - 1;
	screen->last_written_row = screen->cur_row;
    });

    if_OPT_XMC_GLITCH(screen, {
	Resolve_XMC(xw);
    });
}

/*
 * Saves pointers to the n lines beginning at sb + where, and clears the lines
 */
static void
ScrnClearLines(XtermWidget xw, ScrnBuf sb, int where, unsigned n, unsigned size)
{
    TScreen *screen = &(xw->screen);
    int i, j;
    size_t len = ScrnPointers(screen, n);
    int last = (n * MAX_PTRS);

    TRACE(("ScrnClearLines(where %d, n %d, size %d)\n", where, n, size));

    assert(n != 0);
    assert(size != 0);

    /* save n lines at where */
    memcpy((char *) screen->save_ptr,
	   (char *) &sb[MAX_PTRS * where],
	   len);

    /* clear contents of old rows */
    if (TERM_COLOR_FLAGS(xw)) {
	int flags = TERM_COLOR_FLAGS(xw);
	for (i = 0; i < last; i += MAX_PTRS) {
	    for (j = 0; j < MAX_PTRS; j++) {
		if (j < BUF_HEAD)
		    screen->save_ptr[i + j] = 0;
		else if (j == OFF_ATTRS)
		    memset(screen->save_ptr[i + j], flags, size);
#if OPT_ISO_COLORS
#if OPT_EXT_COLORS
		else if (j == OFF_FGRND)
		    memset(screen->save_ptr[i + j], xw->sgr_foreground, size);
		else if (j == OFF_BGRND)
		    memset(screen->save_ptr[i + j], xw->cur_background, size);
#else
		else if (j == OFF_COLOR)
		    memset(screen->save_ptr[i + j], (int)
			   xtermColorPair(xw), size);
#endif
#endif
		else
		    bzero(screen->save_ptr[i + j], size);
	    }
	}
    } else {
	for (i = 0; i < last; i += MAX_PTRS) {
	    for (j = 0; j < BUF_HEAD; j++)
		screen->save_ptr[i + j] = 0;
	    for (j = BUF_HEAD; j < MAX_PTRS; j++)
		bzero(screen->save_ptr[i + j], size);
	}
    }
}

size_t
ScrnPointers(TScreen * screen, size_t len)
{
    len *= MAX_PTRS;

    if (len > screen->save_len) {
	if (screen->save_len)
	    screen->save_ptr = TypeRealloc(ScrnPtr, len, screen->save_ptr);
	else
	    screen->save_ptr = TypeMallocN(ScrnPtr, len);
	screen->save_len = len;
	if (screen->save_ptr == 0)
	    SysError(ERROR_SAVE_PTR);
    }
    return len * sizeof(ScrnPtr);
}

/*
 * Inserts n blank lines at sb + where, treating last as a bottom margin.
 * size is the size of each entry in sb.
 */
void
ScrnInsertLine(XtermWidget xw, ScrnBuf sb, int last, int where,
	       unsigned n, unsigned size)
{
    TScreen *screen = &(xw->screen);
    size_t len = ScrnPointers(screen, n);

    assert(where >= 0);
    assert(last >= (int) n);
    assert(last >= where);

    assert(n != 0);
    assert(size != 0);
    assert(MAX_PTRS > 0);

    /* save n lines at bottom */
    ScrnClearLines(xw, sb, (last -= n - 1), n, size);

    /*
     * WARNING, overlapping copy operation.  Move down lines (pointers).
     *
     *   +----|---------|--------+
     *
     * is copied in the array to:
     *
     *   +--------|---------|----+
     */
    assert(last >= where);
    memmove((char *) &sb[MAX_PTRS * (where + n)],
	    (char *) &sb[MAX_PTRS * where],
	    MAX_PTRS * sizeof(char *) * (last - where));

    /* reuse storage for new lines at where */
    memcpy((char *) &sb[MAX_PTRS * where],
	   (char *) screen->save_ptr,
	   len);
}

/*
 * Deletes n lines at sb + where, treating last as a bottom margin.
 * size is the size of each entry in sb.
 */
void
ScrnDeleteLine(XtermWidget xw, ScrnBuf sb, int last, int where,
	       unsigned n, unsigned size)
{
    TScreen *screen = &(xw->screen);

    assert(where >= 0);
    assert(last >= where + (int) n - 1);

    assert(n != 0);
    assert(size != 0);
    assert(MAX_PTRS > 0);

    ScrnClearLines(xw, sb, where, n, size);

    /* move up lines */
    memmove((char *) &sb[MAX_PTRS * where],
	    (char *) &sb[MAX_PTRS * (where + n)],
	    MAX_PTRS * sizeof(char *) * ((last -= n - 1) - where));

    /* reuse storage for new bottom lines */
    memcpy((char *) &sb[MAX_PTRS * last],
	   (char *) screen->save_ptr,
	   MAX_PTRS * sizeof(char *) * n);
}

/*
 * Inserts n blanks in screen at current row, col.  Size is the size of each
 * row.
 */
void
ScrnInsertChar(XtermWidget xw, unsigned n)
{
#define Target (data + col + n)
#define Source (data + col)

    TScreen *screen = &(xw->screen);
    ScrnBuf sb = screen->visbuf;
    int last = MaxCols(screen);
    int row = screen->cur_row;
    int col = screen->cur_col;
    Char *data;
    size_t nbytes;

    if (last <= (int) (col + n)) {
	if (last <= col)
	    return;
	n = last - col;
    }
    nbytes = (last - (col + n));

    assert(screen->cur_col >= 0);
    assert(screen->cur_row >= 0);
    assert(n > 0);
    assert(last > (int) n);

    if_OPT_WIDE_CHARS(screen, {
	int xx = INX2ROW(screen, screen->cur_row);
	int kl;
	int kr = screen->cur_col;
	if (DamagedCells(screen, n, &kl, (int *) 0, xx, kr) && kr > kl) {
	    ClearCells(xw, 0, (unsigned) (kr - kl + 1), row, kl);
	}
	kr = screen->max_col - n + 1;
	if (DamagedCells(screen, n, &kl, (int *) 0, xx, kr) && kr > kl) {
	    ClearCells(xw, 0, (unsigned) (kr - kl + 1), row, kl);
	}
    });

    data = BUF_CHARS(sb, row);
    memmove(Target, Source, nbytes);

    data = BUF_ATTRS(sb, row);
    memmove(Target, Source, nbytes);

    if_OPT_EXT_COLORS(screen, {
	data = BUF_FGRND(sb, row);
	memmove(Target, Source, nbytes);
	data = BUF_BGRND(sb, row);
	memmove(Target, Source, nbytes);
    });
    if_OPT_ISO_TRADITIONAL_COLORS(screen, {
	data = BUF_COLOR(sb, row);
	memmove(Target, Source, nbytes);
    });
    if_OPT_DEC_CHRSET({
	data = BUF_CSETS(sb, row);
	memmove(Target, Source, nbytes);
    });
    if_OPT_WIDE_CHARS(screen, {
	int off;
	for (off = OFF_WIDEC; off < MAX_PTRS; ++off) {
	    data = BUFFER_PTR(sb, row, off);
	    memmove(Target, Source, nbytes);
	}
    });
    ClearCells(xw, CHARDRAWN, n, row, col);

#undef Source
#undef Target
}

/*
 * Deletes n characters at current row, col.
 */
void
ScrnDeleteChar(XtermWidget xw, unsigned n)
{
#define Target (data + col)
#define Source (data + col + n)

    TScreen *screen = &(xw->screen);
    ScrnBuf sb = screen->visbuf;
    int last = MaxCols(screen);
    int row = screen->cur_row;
    int col = screen->cur_col;
    Char *data;
    size_t nbytes;

    if (last <= (int) (col + n)) {
	if (last <= col)
	    return;
	n = last - col;
    }
    nbytes = (last - (col + n));

    assert(screen->cur_col >= 0);
    assert(screen->cur_row >= 0);
    assert(n > 0);
    assert(last > (int) n);

    if_OPT_WIDE_CHARS(screen, {
	int kl;
	int kr;
	if (DamagedCells(screen, n, &kl, &kr,
			 INX2ROW(screen, screen->cur_row),
			 screen->cur_col))
	    ClearCells(xw, 0, (unsigned) (kr - kl + 1), row, kl);
    });

    data = BUF_CHARS(sb, row);
    memmove(Target, Source, nbytes);

    data = BUF_ATTRS(sb, row);
    memmove(Target, Source, nbytes);

    if_OPT_EXT_COLORS(screen, {
	data = BUF_FGRND(sb, row);
	memmove(Target, Source, nbytes);
	data = BUF_BGRND(sb, row);
	memmove(Target, Source, nbytes);
    });
    if_OPT_ISO_TRADITIONAL_COLORS(screen, {
	data = BUF_COLOR(sb, row);
	memmove(Target, Source, nbytes);
    });
    if_OPT_DEC_CHRSET({
	data = BUF_CSETS(sb, row);
	memmove(Target, Source, nbytes);
    });
    if_OPT_WIDE_CHARS(screen, {
	int off;
	for (off = OFF_WIDEC; off < MAX_PTRS; ++off) {
	    data = BUFFER_PTR(sb, row, off);
	    memmove(Target, Source, nbytes);
	}
    });
    ClearCells(xw, 0, n, row, (int) (last - n));
    ScrnClrWrapped(screen, row);

#undef Source
#undef Target
}

/*
 * Repaints the area enclosed by the parameters.
 * Requires: (toprow, leftcol), (toprow + nrows, leftcol + ncols) are
 * 	     coordinates of characters in screen;
 *	     nrows and ncols positive.
 *	     all dimensions are based on single-characters.
 */
void
ScrnRefresh(XtermWidget xw,
	    int toprow,
	    int leftcol,
	    int nrows,
	    int ncols,
	    Bool force)		/* ... leading/trailing spaces */
{
    TScreen *screen = &(xw->screen);
    int y = toprow * FontHeight(screen) + screen->border;
    int row;
    int maxrow = toprow + nrows - 1;
    int scrollamt = screen->scroll_amt;
    int max = screen->max_row;
    int gc_changes = 0;
#ifdef __CYGWIN__
    static char first_time = 1;
#endif
    static int recurse = 0;

    TRACE(("ScrnRefresh (%d,%d) - (%d,%d)%s\n",
	   toprow, leftcol,
	   nrows, ncols,
	   force ? " force" : ""));

    if (screen->cursorp.col >= leftcol
	&& screen->cursorp.col <= (leftcol + ncols - 1)
	&& screen->cursorp.row >= ROW2INX(screen, toprow)
	&& screen->cursorp.row <= ROW2INX(screen, maxrow))
	screen->cursor_state = OFF;

    for (row = toprow; row <= maxrow; y += FontHeight(screen), row++) {
#if OPT_ISO_COLORS
#if OPT_EXT_COLORS
	Char *fbf = 0;
	Char *fbb = 0;
#define ColorOf(col) (unsigned) ((fbf[col] << 8) | fbb[col])
#else
	Char *fb = 0;
#define ColorOf(col) (unsigned) (fb[col])
#endif
#endif
#if OPT_DEC_CHRSET
	Char *cb = 0;
#endif
#if OPT_WIDE_CHARS
	int wideness = 0;
	Char *widec = 0;
#define WIDEC_PTR(cell) widec ? &widec[cell] : 0
#define BLANK_CEL(cell) ((chars[cell] == ' ') && (widec == 0 || widec[cell] == 0))
#else
#define BLANK_CEL(cell) (chars[cell] == ' ')
#endif
	Char cs = 0;
	Char *chars;
	Char *attrs;
	int col = leftcol;
	int maxcol = leftcol + ncols - 1;
	int hi_col = maxcol;
	int lastind;
	unsigned flags;
	unsigned test;
	unsigned fg_bg = 0, fg = 0, bg = 0;
	int x;
	GC gc;
	Bool hilite;

	(void) fg;
	(void) bg;

	if (row < screen->top_marg || row > screen->bot_marg)
	    lastind = row;
	else
	    lastind = row - scrollamt;

	TRACE(("ScrnRefresh row=%d lastind=%d/%d\n", row, lastind, max));
	if (lastind < 0 || lastind > max)
	    continue;

	chars = SCRN_BUF_CHARS(screen, ROW2INX(screen, lastind));
	attrs = SCRN_BUF_ATTRS(screen, ROW2INX(screen, lastind));

	if_OPT_DEC_CHRSET({
	    cb = SCRN_BUF_CSETS(screen, ROW2INX(screen, lastind));
	});

	if_OPT_WIDE_CHARS(screen, {
	    widec = SCRN_BUF_WIDEC(screen, ROW2INX(screen, lastind));
	});

	if_OPT_WIDE_CHARS(screen, {
	    /* This fixes an infinite recursion bug, that leads
	       to display anomalies. It seems to be related to
	       problems with the selection. */
	    if (recurse < 3) {
		/* adjust to redraw all of a widechar if we just wanted
		   to draw the right hand half */
		if (leftcol > 0 &&
		    (PACK_PAIR(chars, widec, leftcol)) == HIDDEN_CHAR &&
		    iswide(PACK_PAIR(chars, widec, leftcol - 1))) {
		    leftcol--;
		    ncols++;
		    col = leftcol;
		}
	    } else {
		fprintf(stderr, "This should not happen. Why is it so?\n");
	    }
	});

	if (row < screen->startH.row || row > screen->endH.row ||
	    (row == screen->startH.row && maxcol < screen->startH.col) ||
	    (row == screen->endH.row && col >= screen->endH.col)) {
#if OPT_DEC_CHRSET
	    /*
	     * Temporarily change dimensions to double-sized characters so
	     * we can reuse the recursion on this function.
	     */
	    if (CSET_DOUBLE(*cb)) {
		col /= 2;
		maxcol /= 2;
	    }
#endif
	    /*
	     * If row does not intersect selection; don't hilite blanks.
	     */
	    if (!force) {
		while (col <= maxcol && (attrs[col] & ~BOLD) == 0 &&
		       BLANK_CEL(col))
		    col++;

		while (col <= maxcol && (attrs[maxcol] & ~BOLD) == 0 &&
		       BLANK_CEL(maxcol))
		    maxcol--;
	    }
#if OPT_DEC_CHRSET
	    if (CSET_DOUBLE(*cb)) {
		col *= 2;
		maxcol *= 2;
	    }
#endif
	    hilite = False;
	} else {
	    /* row intersects selection; split into pieces of single type */
	    if (row == screen->startH.row && col < screen->startH.col) {
		recurse++;
		ScrnRefresh(xw, row, col, 1, screen->startH.col - col,
			    force);
		col = screen->startH.col;
	    }
	    if (row == screen->endH.row && maxcol >= screen->endH.col) {
		recurse++;
		ScrnRefresh(xw, row, screen->endH.col, 1,
			    maxcol - screen->endH.col + 1, force);
		maxcol = screen->endH.col - 1;
	    }

	    /*
	     * If we're highlighting because the user is doing cut/paste,
	     * trim the trailing blanks from the highlighted region so we're
	     * showing the actual extent of the text that'll be cut.  If
	     * we're selecting a blank line, we'll highlight one column
	     * anyway.
	     *
	     * We don't do this if the mouse-hilite mode is set because that
	     * would be too confusing.
	     *
	     * The default if the highlightSelection resource isn't set will
	     * highlight the whole width of the terminal, which is easy to
	     * see, but harder to use (because trailing blanks aren't as
	     * apparent).
	     */
	    if (screen->highlight_selection
		&& screen->send_mouse_pos != VT200_HIGHLIGHT_MOUSE) {
		hi_col = screen->max_col;
		while (hi_col > 0 && !(attrs[hi_col] & CHARDRAWN))
		    hi_col--;
	    }

	    /* remaining piece should be hilited */
	    hilite = True;
	}

	if (col > maxcol)
	    continue;

	/*
	 * Go back to double-sized character dimensions if the line has
	 * double-width characters.  Note that 'hi_col' is already in the
	 * right units.
	 */
	if_OPT_DEC_CHRSET({
	    if (CSET_DOUBLE(*cb)) {
		col /= 2;
		maxcol /= 2;
	    }
	    cs = cb[col];
	});

	flags = attrs[col];
#if OPT_WIDE_CHARS
	if (widec)
	    wideness = iswide(PACK_PAIR(chars, widec, col));
	else
	    wideness = 0;
#endif
	if_OPT_EXT_COLORS(screen, {
	    fbf = SCRN_BUF_FGRND(screen, ROW2INX(screen, lastind));
	    fbb = SCRN_BUF_BGRND(screen, ROW2INX(screen, lastind));
	    fg_bg = ColorOf(col);
	    /* this combines them, then splits them again.  but
	       extract_fg does more, so seems reasonable */
	    fg = extract_fg(xw, fg_bg, flags);
	    bg = extract_bg(xw, fg_bg, flags);
	});
	if_OPT_ISO_TRADITIONAL_COLORS(screen, {
	    fb = SCRN_BUF_COLOR(screen, ROW2INX(screen, lastind));
	    fg_bg = ColorOf(col);
	    fg = extract_fg(xw, fg_bg, flags);
	    bg = extract_bg(xw, fg_bg, flags);
	});

	gc = updatedXtermGC(xw, flags, fg_bg, hilite);
	gc_changes |= (flags & (FG_COLOR | BG_COLOR));

	x = CurCursorX(screen, ROW2INX(screen, row), col);
	lastind = col;

	for (; col <= maxcol; col++) {
	    if ((attrs[col] != flags)
		|| (hilite && (col > hi_col))
#if OPT_ISO_COLORS
		|| ((flags & FG_COLOR)
		    && (extract_fg(xw, ColorOf(col), attrs[col]) != fg))
		|| ((flags & BG_COLOR)
		    && (extract_bg(xw, ColorOf(col), attrs[col]) != bg))
#endif
#if OPT_WIDE_CHARS
		|| (widec
		    && ((iswide(PACK_PAIR(chars, widec, col))) != wideness)
		    && !((PACK_PAIR(chars, widec, col)) == HIDDEN_CHAR))
#endif
#if OPT_DEC_CHRSET
		|| (cb[col] != cs)
#endif
		) {
		assert(col >= lastind);
		TRACE(("ScrnRefresh looping drawXtermText %d..%d:%s\n",
		       lastind, col,
		       visibleChars(PAIRED_CHARS(&chars[lastind],
						 WIDEC_PTR(lastind)),
				    (unsigned) (col - lastind))));

		test = flags;
		checkVeryBoldColors(test, fg);

		x = drawXtermText(xw, test & DRAWX_MASK, gc, x, y,
				  cs,
				  PAIRED_CHARS(&chars[lastind], WIDEC_PTR(lastind)),
				  (unsigned) (col - lastind), 0);

		if_OPT_WIDE_CHARS(screen, {
		    int i;
		    int off;
		    for (off = OFF_FINAL; off < MAX_PTRS; off += 2) {
			Char *com_lo = BUFFER_PTR(screen->visbuf,
						  ROW2INX(screen, row),
						  off + 0);
			Char *com_hi = BUFFER_PTR(screen->visbuf,
						  ROW2INX(screen, row),
						  off + 1);
			for (i = lastind; i < col; i++) {
			    int my_x = CurCursorX(screen,
						  ROW2INX(screen, row),
						  i);
			    int base = PACK_PAIR(chars, widec, i);
			    int combo = PACK_PAIR(com_lo, com_hi, i);

			    if (iswide(base))
				my_x = CurCursorX(screen,
						  ROW2INX(screen, row),
						  i - 1);

			    if (combo != 0)
				drawXtermText(xw,
					      (test & DRAWX_MASK)
					      | NOBACKGROUND,
					      gc, my_x, y, cs,
					      PAIRED_CHARS(com_lo + i,
							   com_hi + i),
					      1, iswide(base));
			}
		    }
		});

		resetXtermGC(xw, flags, hilite);

		lastind = col;

		if (hilite && (col > hi_col))
		    hilite = False;

		flags = attrs[col];
		if_OPT_EXT_COLORS(screen, {
		    fg_bg = ColorOf(col);
		    fg = extract_fg(xw, fg_bg, flags);
		    bg = extract_bg(xw, fg_bg, flags);
		});
		if_OPT_ISO_TRADITIONAL_COLORS(screen, {
		    fg_bg = ColorOf(col);
		    fg = extract_fg(xw, fg_bg, flags);
		    bg = extract_bg(xw, fg_bg, flags);
		});
		if_OPT_DEC_CHRSET({
		    cs = cb[col];
		});
#if OPT_WIDE_CHARS
		if (widec)
		    wideness = iswide(PACK_PAIR(chars, widec, col));
#endif

		gc = updatedXtermGC(xw, flags, fg_bg, hilite);
		gc_changes |= (flags & (FG_COLOR | BG_COLOR));
	    }

	    if (chars[col] == 0) {
#if OPT_WIDE_CHARS
		if (widec == 0 || widec[col] == 0)
#endif
		    chars[col] = ' ';
	    }
	}

	assert(col >= lastind);
	TRACE(("ScrnRefresh calling drawXtermText %d..%d:%s\n",
	       lastind, col,
	       visibleChars(PAIRED_CHARS(&chars[lastind], WIDEC_PTR(lastind)),
			    (unsigned) (col - lastind))));

	test = flags;
	checkVeryBoldColors(test, fg);

	drawXtermText(xw, test & DRAWX_MASK, gc, x, y,
		      cs,
		      PAIRED_CHARS(&chars[lastind], WIDEC_PTR(lastind)),
		      (unsigned) (col - lastind), 0);

	if_OPT_WIDE_CHARS(screen, {
	    int i;
	    int off;
	    for (off = OFF_FINAL; off < MAX_PTRS; off += 2) {
		Char *com_lo = BUFFER_PTR(screen->visbuf,
					  ROW2INX(screen, row),
					  off + 0);
		Char *com_hi = BUFFER_PTR(screen->visbuf,
					  ROW2INX(screen, row),
					  off + 1);
		for (i = lastind; i < col; i++) {
		    int my_x = CurCursorX(screen,
					  ROW2INX(screen, row),
					  i);
		    int base = PACK_PAIR(chars, widec, i);
		    int combo = PACK_PAIR(com_lo, com_hi, i);

		    if (iswide(base))
			my_x = CurCursorX(screen,
					  ROW2INX(screen, row),
					  i - 1);

		    if (combo != 0)
			drawXtermText(xw,
				      (test & DRAWX_MASK)
				      | NOBACKGROUND,
				      gc, my_x, y, cs,
				      PAIRED_CHARS(com_lo + i,
						   com_hi + i),
				      1, iswide(base));
		}
	    }
	});

	resetXtermGC(xw, flags, hilite);
    }

    /*
     * If we're in color mode, reset the various GC's to the current
     * screen foreground and background so that other functions (e.g.,
     * ClearRight) will get the correct colors.
     */
    if_OPT_ISO_COLORS(screen, {
	if (gc_changes & FG_COLOR)
	    SGR_Foreground(xw, xw->cur_foreground);
	if (gc_changes & BG_COLOR)
	    SGR_Background(xw, xw->cur_background);
    });

#if defined(__CYGWIN__) && defined(TIOCSWINSZ)
    if (first_time == 1) {
	TTYSIZE_STRUCT ts;

	first_time = 0;
	TTYSIZE_ROWS(ts) = nrows;
	TTYSIZE_COLS(ts) = ncols;
	ts.ws_xpixel = xw->core.width;
	ts.ws_ypixel = xw->core.height;
	SET_TTYSIZE(screen->respond, ts);
    }
#endif
    recurse--;
}

/*
 * Call this wrapper to ScrnRefresh() when the data has changed.  If the
 * refresh region overlaps the selection, we will release the primary selection.
 */
void
ScrnUpdate(XtermWidget xw,
	   int toprow,
	   int leftcol,
	   int nrows,
	   int ncols,
	   Bool force)		/* ... leading/trailing spaces */
{
    TScreen *screen = &(xw->screen);

    if (ScrnHaveSelection(screen)
	&& (toprow <= screen->endH.row)
	&& (toprow + nrows - 1 >= screen->startH.row)) {
	ScrnDisownSelection(xw);
    }
    ScrnRefresh(xw, toprow, leftcol, nrows, ncols, force);
}

/*
 * Sets the rows first though last of the buffer of screen to spaces.
 * Requires first <= last; first, last are rows of screen->buf.
 */
void
ClearBufRows(XtermWidget xw,
	     int first,
	     int last)
{
    TScreen *screen = &(xw->screen);
    unsigned len = MaxCols(screen);
    int row;

    TRACE(("ClearBufRows %d..%d\n", first, last));
    for (row = first; row <= last; row++) {
	if_OPT_DEC_CHRSET({
	    /* clearing the whole row resets the doublesize characters */
	    SCRN_ROW_CSET(screen, row) = CSET_SWL;
	});
	ScrnClrWrapped(screen, row);
	ClearCells(xw, 0, len, row, 0);
    }
}

/*
  Resizes screen:
  1. If new window would have fractional characters, sets window size so as to
  discard fractional characters and returns -1.
  Minimum screen size is 1 X 1.
  Note that this causes another ExposeWindow event.
  2. Enlarges screen->buf if necessary.  New space is appended to the bottom
  and to the right
  3. Reduces  screen->buf if necessary.  Old space is removed from the bottom
  and from the right
  4. Cursor is positioned as closely to its former position as possible
  5. Sets screen->max_row and screen->max_col to reflect new size
  6. Maintains the inner border (and clears the border on the screen).
  7. Clears origin mode and sets scrolling region to be entire screen.
  8. Returns 0
  */
int
ScreenResize(XtermWidget xw,
	     int width,
	     int height,
	     unsigned *flags)
{
    TScreen *screen = &(xw->screen);
    int code, rows, cols;
    int border = 2 * screen->border;
    int move_down_by = 0;
#ifdef TTYSIZE_STRUCT
    TTYSIZE_STRUCT ts;
#endif
    Window tw = VWindow(screen);

    TRACE(("ScreenResize %dx%d border %d font %dx%d\n",
	   height, width, border,
	   FontHeight(screen), FontWidth(screen)));

    assert(width > 0);
    assert(height > 0);

    if (screen->is_running) {
	/* clear the right and bottom internal border because of NorthWest
	   gravity might have left junk on the right and bottom edges */
	if (width >= FullWidth(screen)) {
	    XClearArea(screen->display, tw,
		       FullWidth(screen), 0,	/* right edge */
		       0, (unsigned) height,	/* from top to bottom */
		       False);
	}
	if (height >= FullHeight(screen)) {
	    XClearArea(screen->display, tw,
		       0, FullHeight(screen),	/* bottom */
		       (unsigned) width, 0,	/* all across the bottom */
		       False);
	}
    }

    TRACE(("...computing rows/cols: %.2f %.2f\n",
	   (double) (height - border) / FontHeight(screen),
	   (double) (width - border - ScrollbarWidth(screen)) / FontWidth(screen)));

    rows = (height - border) / FontHeight(screen);
    cols = (width - border - ScrollbarWidth(screen)) / FontWidth(screen);
    if (rows < 1)
	rows = 1;
    if (cols < 1)
	cols = 1;

    /* update buffers if the screen has changed size */
    if (MaxRows(screen) != rows || MaxCols(screen) != cols) {
	int savelines = (screen->scrollWidget
			 ? screen->savelines
			 : 0);
	int delta_rows = rows - MaxRows(screen);

	TRACE(("...ScreenResize chars %dx%d\n", rows, cols));

	if (screen->is_running) {
	    if (screen->cursor_state)
		HideCursor();
	    if (screen->alternate
		&& xw->misc.resizeGravity == SouthWestGravity)
		/* swap buffer pointers back to make this work */
		SwitchBufPtrs(screen);
	    if (screen->altbuf)
		(void) Reallocate(xw,
				  &screen->altbuf,
				  &screen->abuf_address,
				  rows,
				  cols,
				  MaxRows(screen),
				  MaxCols(screen));
	    move_down_by = Reallocate(xw,
				      &screen->allbuf,
				      &screen->sbuf_address,
				      rows + savelines, cols,
				      MaxRows(screen) + savelines,
				      MaxCols(screen));
	    screen->visbuf = &screen->allbuf[MAX_PTRS * savelines];
	}

	AdjustSavedCursor(xw, move_down_by);
	set_max_row(screen, screen->max_row + delta_rows);
	set_max_col(screen, cols - 1);

	if (screen->is_running) {
	    if (xw->misc.resizeGravity == SouthWestGravity) {
		screen->savedlines -= move_down_by;
		if (screen->savedlines < 0)
		    screen->savedlines = 0;
		if (screen->savedlines > screen->savelines)
		    screen->savedlines = screen->savelines;
		if (screen->topline < -screen->savedlines)
		    screen->topline = -screen->savedlines;
		set_cur_row(screen, screen->cur_row + move_down_by);
		screen->cursorp.row += move_down_by;
		ScrollSelection(screen, move_down_by, True);

		if (screen->alternate)
		    SwitchBufPtrs(screen);	/* put the pointers back */
	    }
	}

	/* adjust scrolling region */
	set_tb_margins(screen, 0, screen->max_row);
	*flags &= ~ORIGIN;

	if (screen->cur_row > screen->max_row)
	    set_cur_row(screen, screen->max_row);
	if (screen->cur_col > screen->max_col)
	    set_cur_col(screen, screen->max_col);

	screen->fullVwin.height = height - border;
	screen->fullVwin.width = width - border - screen->fullVwin.sb_info.width;

    } else if (FullHeight(screen) == height && FullWidth(screen) == width)
	return (0);		/* nothing has changed at all */

    screen->fullVwin.fullheight = height;
    screen->fullVwin.fullwidth = width;

    ResizeScrollBar(xw);
    ResizeSelection(screen, rows, cols);

#ifndef NO_ACTIVE_ICON
    if (screen->iconVwin.window) {
	XWindowChanges changes;
	screen->iconVwin.width =
	    MaxCols(screen) * screen->iconVwin.f_width;

	screen->iconVwin.height =
	    MaxRows(screen) * screen->iconVwin.f_height;

	changes.width = screen->iconVwin.fullwidth =
	    screen->iconVwin.width + 2 * xw->misc.icon_border_width;
	changes.height = screen->iconVwin.fullheight =
	    screen->iconVwin.height + 2 * xw->misc.icon_border_width;
	changes.border_width = xw->misc.icon_border_width;

	TRACE(("resizing icon window %dx%d\n", changes.height, changes.width));
	XConfigureWindow(XtDisplay(xw), screen->iconVwin.window,
			 CWWidth | CWHeight | CWBorderWidth, &changes);
    }
#endif /* NO_ACTIVE_ICON */

#ifdef TTYSIZE_STRUCT
    /* Set tty's idea of window size */
    TTYSIZE_ROWS(ts) = rows;
    TTYSIZE_COLS(ts) = cols;
#ifdef USE_STRUCT_WINSIZE
    ts.ws_xpixel = width;
    ts.ws_ypixel = height;
#endif
    code = SET_TTYSIZE(screen->respond, ts);
    TRACE(("return %d from SET_TTYSIZE %dx%d\n", code, rows, cols));
    (void) code;

#if defined(SIGWINCH) && defined(USE_STRUCT_TTYSIZE)
    if (screen->pid > 1) {
	int pgrp;

	TRACE(("getting process-group\n"));
	if (ioctl(screen->respond, TIOCGPGRP, &pgrp) != -1) {
	    TRACE(("sending SIGWINCH to process group %d\n", pgrp));
	    kill_process_group(pgrp, SIGWINCH);
	}
    }
#endif /* SIGWINCH */

#else
    TRACE(("ScreenResize cannot do anything to pty\n"));
#endif /* TTYSIZE_STRUCT */
    return (0);
}

/*
 * Return true if any character cell starting at [row,col], for len-cells is
 * nonnull.
 */
Bool
non_blank_line(TScreen * screen,
	       int row,
	       int col,
	       int len)
{
    ScrnBuf sb = screen->visbuf;
    int i;
    Char *ptr = BUF_CHARS(sb, row);

    for (i = col; i < len; i++) {
	if (ptr[i])
	    return True;
    }

    if_OPT_WIDE_CHARS(screen, {
	if ((ptr = BUF_WIDEC(sb, row)) != 0) {
	    for (i = col; i < len; i++) {
		if (ptr[i])
		    return True;
	    }
	}
    });

    return False;
}

/*
 * Rectangle parameters start from one.
 */
#define minRectRow(screen) (getMinRow(screen) + 1)
#define minRectCol(screen) (getMinCol(screen) + 1)
#define maxRectRow(screen) (getMaxRow(screen) + 1)
#define maxRectCol(screen) (getMaxCol(screen) + 1)

static int
limitedParseRow(XtermWidget xw, TScreen * screen, int row)
{
    int min_row = minRectRow(screen);
    int max_row = maxRectRow(screen);

    if (row < min_row)
	row = min_row;
    else if (row > max_row)
	row = max_row;
    return row;
}

static int
limitedParseCol(XtermWidget xw, TScreen * screen, int col)
{
    int min_col = minRectCol(screen);
    int max_col = maxRectCol(screen);

    (void) xw;
    if (col < min_col)
	col = min_col;
    else if (col > max_col)
	col = max_col;
    return col;
}

#define LimitedParse(num, func, dft) \
	func(xw, screen, (nparams > num) ? params[num] : dft)

/*
 * Copy the rectangle boundaries into a struct, providing default values as
 * needed.
 */
void
xtermParseRect(XtermWidget xw, int nparams, int *params, XTermRect * target)
{
    TScreen *screen = &(xw->screen);

    memset(target, 0, sizeof(*target));
    target->top = LimitedParse(0, limitedParseRow, minRectRow(screen));
    target->left = LimitedParse(1, limitedParseCol, minRectCol(screen));
    target->bottom = LimitedParse(2, limitedParseRow, maxRectRow(screen));
    target->right = LimitedParse(3, limitedParseCol, maxRectCol(screen));
    TRACE(("parsed rectangle %d,%d %d,%d\n",
	   target->top,
	   target->left,
	   target->bottom,
	   target->right));
}

static Bool
validRect(XtermWidget xw, XTermRect * target)
{
    TScreen *screen = &(xw->screen);

    TRACE(("comparing against screensize %dx%d\n",
	   maxRectRow(screen),
	   maxRectCol(screen)));
    return (target != 0
	    && target->top >= minRectRow(screen)
	    && target->left >= minRectCol(screen)
	    && target->top <= target->bottom
	    && target->left <= target->right
	    && target->top <= maxRectRow(screen)
	    && target->right <= maxRectCol(screen));
}

/*
 * Fills a rectangle with the given 8-bit character and video-attributes.
 * Colors and double-size attribute are unmodified.
 */
void
ScrnFillRectangle(XtermWidget xw,
		  XTermRect * target,
		  int value,
		  unsigned flags,
		  Bool keepColors)
{
    TScreen *screen = &(xw->screen);

    TRACE(("filling rectangle with '%c' flags %#x\n", value, flags));
    if (validRect(xw, target)) {
	unsigned left = target->left - 1;
	unsigned size = target->right - left;
	Char attrs = flags;
	int row, col;

	attrs &= ATTRIBUTES;
	attrs |= CHARDRAWN;
	for (row = target->bottom - 1; row >= (target->top - 1); row--) {
	    TRACE(("filling %d [%d..%d]\n", row, left, left + size));

	    /*
	     * Fill attributes, preserving "protected" flag, as well as
	     * colors if asked.
	     */
	    for (col = left; col < target->right; ++col) {
		Char temp = SCRN_BUF_ATTRS(screen, row)[col];
		if (!keepColors) {
		    temp &= ~(FG_COLOR | BG_COLOR);
		}
		temp = attrs | (temp & (FG_COLOR | BG_COLOR | PROTECTED));
		temp |= CHARDRAWN;
		SCRN_BUF_ATTRS(screen, row)[col] = temp;
#if OPT_ISO_COLORS
		if (attrs & (FG_COLOR | BG_COLOR)) {
		    if_OPT_EXT_COLORS(screen, {
			SCRN_BUF_FGRND(screen, row)[col] = xw->sgr_foreground;
			SCRN_BUF_BGRND(screen, row)[col] = xw->cur_background;
		    });
		    if_OPT_ISO_TRADITIONAL_COLORS(screen, {
			SCRN_BUF_COLOR(screen, row)[col] = xtermColorPair(xw);
		    });
		}
#endif
	    }

	    memset(SCRN_BUF_CHARS(screen, row) + left, (Char) value, size);
	    if_OPT_WIDE_CHARS(screen, {
		int off;
		for (off = OFF_WIDEC; off < MAX_PTRS; ++off) {
		    memset(SCREEN_PTR(screen, row, off) + left, 0, size);
		}
	    })
	}
	ScrnUpdate(xw,
		   target->top - 1,
		   target->left - 1,
		   (target->bottom - target->top) + 1,
		   (target->right - target->left) + 1,
		   False);
    }
}

#if OPT_DEC_RECTOPS
/*
 * Copies the source rectangle to the target location, including video
 * attributes.
 *
 * This implementation ignores page numbers.
 *
 * The reference manual does not indicate if it handles overlapping copy
 * properly - so we make a local copy of the source rectangle first, then apply
 * the target from that.
 */
void
ScrnCopyRectangle(XtermWidget xw, XTermRect * source, int nparam, int *params)
{
    TScreen *screen = &(xw->screen);

    TRACE(("copying rectangle\n"));

    if (validRect(xw, source)) {
	XTermRect target;
	xtermParseRect(xw,
		       ((nparam > 3) ? 2 : (nparam - 1)),
		       params,
		       &target);
	if (validRect(xw, &target)) {
	    unsigned high = (source->bottom - source->top) + 1;
	    unsigned wide = (source->right - source->left) + 1;
	    unsigned size = (high * wide * MAX_PTRS);
	    int row, col, n, j;

	    Char *cells = TypeMallocN(Char, size);

	    if (cells == 0)
		return;

	    TRACE(("OK - make copy %dx%d\n", high, wide));
	    target.bottom = target.top + (high - 1);
	    target.right = target.left + (wide - 1);

	    for (row = source->top - 1; row < source->bottom; ++row) {
		for (col = source->left - 1; col < source->right; ++col) {
		    n = (((1 + row - source->top) * wide)
			 + (1 + col - source->left)) * MAX_PTRS;
		    for (j = OFF_ATTRS; j < MAX_PTRS; ++j)
			cells[n + j] = SCREEN_PTR(screen, row, j)[col];
		}
	    }
	    for (row = target.top - 1; row < target.bottom; ++row) {
		for (col = target.left - 1; col < target.right; ++col) {
		    if (row >= getMinRow(screen)
			&& row <= getMaxRow(screen)
			&& col >= getMinCol(screen)
			&& col <= getMaxCol(screen)) {
			n = (((1 + row - target.top) * wide)
			     + (1 + col - target.left)) * MAX_PTRS;
			for (j = OFF_ATTRS; j < MAX_PTRS; ++j)
			    SCREEN_PTR(screen, row, j)[col] = cells[n + j];
			SCRN_BUF_ATTRS(screen, row)[col] |= CHARDRAWN;
		    }
		}
	    }
	    free(cells);

	    ScrnUpdate(xw,
		       (target.top - 1),
		       (target.left - 1),
		       (target.bottom - target.top) + 1,
		       ((target.right - target.left) + 1),
		       False);
	}
    }
}

/*
 * Modifies the video-attributes only - so selection (not a video attribute) is
 * unaffected.  Colors and double-size flags are unaffected as well.
 *
 * FIXME: our representation for "invisible" does not work with this operation,
 * since the attribute byte is fully-allocated for other flags.  The logic
 * is shown for INVISIBLE because it's harmless, and useful in case the
 * CHARDRAWN or PROTECTED flags are reassigned.
 */
void
ScrnMarkRectangle(XtermWidget xw,
		  XTermRect * target,
		  Bool reverse,
		  int nparam,
		  int *params)
{
    TScreen *screen = &(xw->screen);
    Bool exact = (screen->cur_decsace == 2);

    TRACE(("%s %s\n",
	   reverse ? "reversing" : "marking",
	   (exact
	    ? "rectangle"
	    : "region")));

    if (validRect(xw, target)) {
	int top = target->top - 1;
	int bottom = target->bottom - 1;
	int row, col;
	int n;

	for (row = top; row <= bottom; ++row) {
	    int left = ((exact || (row == top))
			? (target->left - 1)
			: getMinCol(screen));
	    int right = ((exact || (row == bottom))
			 ? (target->right - 1)
			 : getMaxCol(screen));

	    TRACE(("marking %d [%d..%d]\n", row, left, right));
	    for (col = left; col <= right; ++col) {
		unsigned flags = SCRN_BUF_ATTRS(screen, row)[col];

		for (n = 0; n < nparam; ++n) {
#if OPT_TRACE
		    if (row == top && col == left)
			TRACE(("attr param[%d] %d\n", n + 1, params[n]));
#endif
		    if (reverse) {
			switch (params[n]) {
			case 1:
			    flags ^= BOLD;
			    break;
			case 4:
			    flags ^= UNDERLINE;
			    break;
			case 5:
			    flags ^= BLINK;
			    break;
			case 7:
			    flags ^= INVERSE;
			    break;
			case 8:
			    flags ^= INVISIBLE;
			    break;
			}
		    } else {
			switch (params[n]) {
			case 0:
			    flags &= ~SGR_MASK;
			    break;
			case 1:
			    flags |= BOLD;
			    break;
			case 4:
			    flags |= UNDERLINE;
			    break;
			case 5:
			    flags |= BLINK;
			    break;
			case 7:
			    flags |= INVERSE;
			    break;
			case 8:
			    flags |= INVISIBLE;
			    break;
			case 22:
			    flags &= ~BOLD;
			    break;
			case 24:
			    flags &= ~UNDERLINE;
			    break;
			case 25:
			    flags &= ~BLINK;
			    break;
			case 27:
			    flags &= ~INVERSE;
			    break;
			case 28:
			    flags &= ~INVISIBLE;
			    break;
			}
		    }
		}
#if OPT_TRACE
		if (row == top && col == left)
		    TRACE(("first mask-change is %#x\n",
			   SCRN_BUF_ATTRS(screen, row)[col] ^ flags));
#endif
		SCRN_BUF_ATTRS(screen, row)[col] = flags;
	    }
	}
	ScrnRefresh(xw,
		    (target->top - 1),
		    (exact ? (target->left - 1) : getMinCol(screen)),
		    (target->bottom - target->top) + 1,
		    (exact
		     ? ((target->right - target->left) + 1)
		     : (getMaxCol(screen) - getMinCol(screen) + 1)),
		    False);
    }
}

/*
 * Resets characters to space, except where prohibited by DECSCA.  Video
 * attributes (including color) are untouched.
 */
void
ScrnWipeRectangle(XtermWidget xw,
		  XTermRect * target)
{
    TScreen *screen = &(xw->screen);

    TRACE(("wiping rectangle\n"));

    if (validRect(xw, target)) {
	int top = target->top - 1;
	int bottom = target->bottom - 1;
	int row, col;

	for (row = top; row <= bottom; ++row) {
	    int left = (target->left - 1);
	    int right = (target->right - 1);

	    TRACE(("wiping %d [%d..%d]\n", row, left, right));
	    for (col = left; col <= right; ++col) {
		if (!((screen->protected_mode == DEC_PROTECT)
		      && (SCRN_BUF_ATTRS(screen, row)[col] & PROTECTED))) {
		    SCRN_BUF_ATTRS(screen, row)[col] |= CHARDRAWN;
		    SCRN_BUF_CHARS(screen, row)[col] = ' ';
		    if_OPT_WIDE_CHARS(screen, {
			int off;
			for (off = OFF_WIDEC; off < MAX_PTRS; ++off) {
			    memset(SCREEN_PTR(screen, row, off) + col, 0, 1);
			}
		    })
		}
	    }
	}
	ScrnUpdate(xw,
		   (target->top - 1),
		   (target->left - 1),
		   (target->bottom - target->top) + 1,
		   ((target->right - target->left) + 1),
		   False);
    }
}
#endif /* OPT_DEC_RECTOPS */

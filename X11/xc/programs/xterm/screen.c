/*
 *	$Xorg: screen.c,v 1.3 2000/08/17 19:55:09 cpqbld Exp $
 */

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

/* $XFree86: xc/programs/xterm/screen.c,v 3.65 2003/10/20 00:58:54 dickey Exp $ */

/* screen.c */

#include <stdio.h>
#include <xterm.h>
#include <error.h>
#include <data.h>
#include <xcharmouse.h>
#include <xterm_io.h>

#include <signal.h>

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

    if ((base = (ScrnBuf) calloc(entries, sizeof(char *))) == 0)
	  SysError(ERROR_SCALLOC);

    if ((tmp = (Char *) calloc(length, sizeof(Char))) == 0)
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
Reallocate(ScrnBuf * sbuf,
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
	&& term->misc.resizeGravity == SouthWestGravity) {
	/* Remove lines off the top of the buffer if necessary. */
	move_up = (oldrow - nrow)
	    - (term->screen.max_row - term->screen.cur_row);
	if (move_up < 0)
	    move_up = 0;
	/* Overlapping memmove here! */
	memmove(*sbuf, *sbuf + (move_up * MAX_PTRS),
		MAX_PTRS * (oldrow - move_up) * sizeof((*sbuf)[0]));
    }
    *sbuf = (ScrnBuf) realloc((char *) (*sbuf), entries * sizeof(char *));
    if (*sbuf == 0)
	SysError(ERROR_RESIZE);
    base = *sbuf;

    /*
     *  create the new buffer space and copy old buffer contents there
     *  line by line.
     */
    if ((tmp = (Char *) calloc(length, sizeof(Char))) == 0)
	SysError(ERROR_SREALLOC);
    *sbufaddr = tmp;
    minrows = (oldrow < nrow) ? oldrow : nrow;
    mincols = (oldcol < ncol) ? oldcol : ncol;
    if (nrow > oldrow
	&& term->misc.resizeGravity == SouthWestGravity) {
	/* move data down to bottom of expanded screen */
	move_down = Min(nrow - oldrow, term->screen.savedlines);
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

int last_written_row = -1;
int last_written_col = -1;

/*
 * Writes str into buf at screen's current row and column.  Characters are set
 * to match flags.
 */
void
ScreenWrite(TScreen * screen,
	    PAIRED_CHARS(Char * str, Char * str2),
	    unsigned flags,
	    unsigned cur_fg_bg,
	    int len)		/* length of string */
{
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
    int length = len;		/* workaround for compiler bug? */
    Char *attrs;
    int avail = screen->max_col - screen->cur_col + 1;
    Char *col;
    int wrappedbit;
#if OPT_WIDE_CHARS
    Char starcol, starcol2;
    Char *comb1l = 0, *comb1h = 0, *comb2l = 0, *comb2h = 0;
#endif

#if OPT_WIDE_CHARS
    int real_width = visual_width(PAIRED_CHARS(str, str2), length);
#else
    int real_width = length;
#endif

    if (length > avail)
	length = avail;
    if (length <= 0)
	return;

    col = SCRN_BUF_CHARS(screen, screen->cur_row) + screen->cur_col;
    attrs = SCRN_BUF_ATTRS(screen, screen->cur_row) + screen->cur_col;

    if_OPT_WIDE_CHARS(screen, {
	comb1l = SCRN_BUF_COM1L(screen, screen->cur_row) + screen->cur_col;
	comb1h = SCRN_BUF_COM1H(screen, screen->cur_row) + screen->cur_col;
	comb2l = SCRN_BUF_COM2L(screen, screen->cur_row) + screen->cur_col;
	comb2h = SCRN_BUF_COM2H(screen, screen->cur_row) + screen->cur_col;
    });

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

    wrappedbit = ScrnTstWrapped(screen, screen->cur_row);

#if OPT_WIDE_CHARS
    starcol = *col;
    starcol2 = col[length - 1];
#endif

    /* write blanks if we're writing invisible text */
    if (flags & INVISIBLE) {
	memset(col, ' ', length);
    } else {
	memcpy(col, str, length);	/* This can stand for the present. If it
					   is wrong, we will scribble over it */
    }
#define ERROR_1 0x20
#define ERROR_2 0x00
    if_OPT_WIDE_CHARS(screen, {

	Char *wc;

	if (real_width != length) {
	    Char *c = col;
	    wc = SCRN_BUF_WIDEC(screen, screen->cur_row);
	    wc += screen->cur_col;
	    if (screen->cur_col && starcol == HIDDEN_LO && *wc == HIDDEN_HI
		&& iswide(c[-1] | (wc[-1] << 8))) {
		c[-1] = ERROR_1;
		wc[-1] = ERROR_2;
	    }
	    /* if we are overwriting the right hand half of a
	       wide character, make the other half vanish */
	    while (length) {
		int ch = *str;
		if (str2)
		    ch |= *str2 << 8;

		*c = *str;
		c++;
		str++;

		if (str2) {
		    *wc = *str2;
		    str2++;
		} else
		    *wc = 0;
		wc++;
		length--;

		if (iswide(ch)) {
		    *c = HIDDEN_LO;
		    *wc = HIDDEN_HI;
		    c++;
		    wc++;
		}
	    }

	    if (*c == HIDDEN_LO
		&& *wc == HIDDEN_HI
		&& c[-1] == HIDDEN_LO
		&& wc[-1] == HIDDEN_HI) {
		*c = ERROR_1;
		*wc = ERROR_2;
	    }
	    /* if we are overwriting the left hand half of a
	       wide character, make the other half vanish */
	}

	else {

	    if ((wc = SCRN_BUF_WIDEC(screen, screen->cur_row)) != 0) {
		wc += screen->cur_col;
		if (screen->cur_col && starcol == HIDDEN_LO && *wc == HIDDEN_HI
		    && iswide(col[-1] | (wc[-1] << 8))) {
		    col[-1] = ERROR_1;
		    wc[-1] = ERROR_2;
		}
		/* if we are overwriting the right hand half of a
		   wide character, make the other half vanish */
		if (col[length] == HIDDEN_LO && wc[length] == HIDDEN_HI &&
		    iswide(starcol2 | (wc[length - 1] << 8))) {
		    col[length] = ERROR_1;
		    wc[length] = ERROR_2;
		}
		/* if we are overwriting the left hand half of a
		   wide character, make the other half vanish */
		if ((flags & INVISIBLE) || (str2 == 0))
		    memset(wc, 0, length);
		else
		    memcpy(wc, str2, length);
	    }
	}
    });

    flags &= ATTRIBUTES;
    flags |= CHARDRAWN;
    memset(attrs, flags, real_width);

    if_OPT_WIDE_CHARS(screen, {
	memset(comb1l, 0, real_width);
	memset(comb2l, 0, real_width);
	memset(comb1h, 0, real_width);
	memset(comb2h, 0, real_width);
    });
    if_OPT_EXT_COLORS(screen, {
	memset(fbf, cur_fg_bg >> 8, real_width);
	memset(fbb, cur_fg_bg & 0xff, real_width);
    });
    if_OPT_ISO_TRADITIONAL_COLORS(screen, {
	memset(fb, cur_fg_bg, real_width);
    });
    if_OPT_DEC_CHRSET({
	memset(cb, curXtermChrSet(screen->cur_row), real_width);
    });

    if (wrappedbit)
	ScrnSetWrapped(screen, screen->cur_row);
    else
	ScrnClrWrapped(screen, screen->cur_row);

    last_written_col = screen->cur_col + real_width - 1;
    last_written_row = screen->cur_row;

    if_OPT_XMC_GLITCH(screen, {
	Resolve_XMC(screen);
    });
}

/*
 * Saves pointers to the n lines beginning at sb + where, and clears the lines
 */
static void
ScrnClearLines(TScreen * screen, ScrnBuf sb, int where, int n, int size)
{
    int i, j;
    size_t len = ScrnPointers(screen, n);
    int last = (n * MAX_PTRS);

    /* save n lines at where */
    memcpy((char *) screen->save_ptr,
	   (char *) &sb[MAX_PTRS * where],
	   len);

    /* clear contents of old rows */
    if (TERM_COLOR_FLAGS) {
	int flags = TERM_COLOR_FLAGS;
	for (i = 0; i < last; i += MAX_PTRS) {
	    for (j = 0; j < MAX_PTRS; j++) {
		if (j < BUF_HEAD)
		    screen->save_ptr[i + j] = 0;
		else if (j == OFF_ATTRS)
		    memset(screen->save_ptr[i + j], flags, size);
#if OPT_ISO_COLORS
#if OPT_EXT_COLORS
		else if (j == OFF_FGRND)
		    memset(screen->save_ptr[i + j], term->sgr_foreground, size);
		else if (j == OFF_BGRND)
		    memset(screen->save_ptr[i + j], term->cur_background, size);
#else
		else if (j == OFF_COLOR)
		    memset(screen->save_ptr[i + j], xtermColorPair(), size);
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
    len *= (MAX_PTRS * sizeof(Char *));

    if (len > screen->save_len) {
	if (screen->save_len)
	    screen->save_ptr = (Char **) realloc(screen->save_ptr, len);
	else
	    screen->save_ptr = (Char **) malloc(len);
	screen->save_len = len;
	if (screen->save_ptr == 0)
	    SysError(ERROR_SAVE_PTR);
    }
    return len;
}

/*
 * Inserts n blank lines at sb + where, treating last as a bottom margin.
 * Size is the size of each entry in sb.
 * Requires: 0 <= where < where + n <= last
 */
void
ScrnInsertLine(TScreen * screen, ScrnBuf sb, int last, int where, int n, int size)
{
    size_t len = ScrnPointers(screen, n);

    /* save n lines at bottom */
    ScrnClearLines(screen, sb, (last -= n - 1), n, size);

    /*
     * WARNING, overlapping copy operation.  Move down lines (pointers).
     *
     *   +----|---------|--------+
     *
     * is copied in the array to:
     *
     *   +--------|---------|----+
     */
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
 * Size is the size of each entry in sb.
 * Requires 0 <= where < where + n < = last
 */
void
ScrnDeleteLine(TScreen * screen, ScrnBuf sb, int last, int where, int n, int size)
{
    ScrnClearLines(screen, sb, where, n, size);

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
ScrnInsertChar(TScreen * screen, int n)
{
    ScrnBuf sb = screen->visbuf;
    int size = screen->max_col + 1;
    int row = screen->cur_row;
    int col = screen->cur_col;
    int i, j;
    Char *ptr = BUF_CHARS(sb, row);
    Char *attrs = BUF_ATTRS(sb, row);
    int wrappedbit = ScrnTstWrapped(screen, row);
    int flags = CHARDRAWN | TERM_COLOR_FLAGS;
    size_t nbytes;

    if (size - (col + n) <= 0) {
	if ((n = size - col) <= 0) {
	    return;
	}
    }
    nbytes = (size - (col + n));

    ScrnClrWrapped(screen, row);	/* make sure the bit isn't moved */
    for (i = size - 1; i >= col + n; i--) {
	ptr[i] = ptr[j = i - n];
	attrs[i] = attrs[j];
    }

    for (i = col; i < col + n; i++)
	ptr[i] = ' ';
    for (i = col; i < col + n; i++)
	attrs[i] = flags;
    if_OPT_EXT_COLORS(screen, {
	ptr = BUF_FGRND(sb, row);
	memmove(ptr + col + n, ptr + col, nbytes);
	memset(ptr + col, term->sgr_foreground, n);
	ptr = BUF_BGRND(sb, row);
	memmove(ptr + col + n, ptr + col, nbytes);
	memset(ptr + col, term->cur_background, n);
    });
    if_OPT_ISO_TRADITIONAL_COLORS(screen, {
	ptr = BUF_COLOR(sb, row);
	memmove(ptr + col + n, ptr + col, nbytes);
	memset(ptr + col, xtermColorPair(), n);
    });
    if_OPT_DEC_CHRSET({
	ptr = BUF_CSETS(sb, row);
	memmove(ptr + col + n, ptr + col, nbytes);
	memset(ptr + col, curXtermChrSet(row), n);
    });
    if_OPT_WIDE_CHARS(screen, {
	ptr = BUF_WIDEC(sb, row);
	memmove(ptr + col + n, ptr + col, nbytes);
	memset(ptr + col, 0, n);

	ptr = BUF_COM1L(sb, row);
	memmove(ptr + col + n, ptr + col, nbytes);
	memset(ptr + col, 0, n);

	ptr = BUF_COM1H(sb, row);
	memmove(ptr + col + n, ptr + col, nbytes);
	memset(ptr + col, 0, n);

	ptr = BUF_COM2L(sb, row);
	memmove(ptr + col + n, ptr + col, nbytes);
	memset(ptr + col, 0, n);

	ptr = BUF_COM2H(sb, row);
	memmove(ptr + col + n, ptr + col, nbytes);
	memset(ptr + col, 0, n);
    });

    if (wrappedbit)
	ScrnSetWrapped(screen, row);
    else
	ScrnClrWrapped(screen, row);
}

/*
 * Deletes n characters at current row, col.
 */
void
ScrnDeleteChar(TScreen * screen, int n)
{
    ScrnBuf sb = screen->visbuf;
    int size = screen->max_col + 1;
    int row = screen->cur_row;
    int col = screen->cur_col;
    Char *ptr = BUF_CHARS(sb, row);
    Char *attrs = BUF_ATTRS(sb, row);
    size_t nbytes;

    if (size - (col + n) <= 0) {
	if ((n = size - col) <= 0) {
	    return;
	}
    }
    nbytes = (size - (col + n));

    memmove(ptr + col, ptr + col + n, nbytes);
    memmove(attrs + col, attrs + col + n, nbytes);
    bzero(ptr + size - n, n);
    memset(attrs + size - n, TERM_COLOR_FLAGS, n);

    if_OPT_EXT_COLORS(screen, {
	ptr = BUF_FGRND(sb, row);
	memmove(ptr + col, ptr + col + n, nbytes);
	memset(ptr + size - n, term->sgr_foreground, n);
	ptr = BUF_BGRND(sb, row);
	memmove(ptr + col, ptr + col + n, nbytes);
	memset(ptr + size - n, term->cur_background, n);
    });
    if_OPT_ISO_TRADITIONAL_COLORS(screen, {
	ptr = BUF_COLOR(sb, row);
	memmove(ptr + col, ptr + col + n, nbytes);
	memset(ptr + size - n, xtermColorPair(), n);
    });
    if_OPT_DEC_CHRSET({
	ptr = BUF_CSETS(sb, row);
	memmove(ptr + col, ptr + col + n, nbytes);
	memset(ptr + size - n, curXtermChrSet(row), n);
    });
    if_OPT_WIDE_CHARS(screen, {
	ptr = BUF_WIDEC(sb, row);
	memmove(ptr + col, ptr + col + n, nbytes);
	memset(ptr + size - n, 0, n);

	ptr = BUF_COM1L(sb, row);
	memmove(ptr + col, ptr + col + n, nbytes);
	memset(ptr + size - n, 0, n);

	ptr = BUF_COM1H(sb, row);
	memmove(ptr + col, ptr + col + n, nbytes);
	memset(ptr + size - n, 0, n);

	ptr = BUF_COM2L(sb, row);
	memmove(ptr + col, ptr + col + n, nbytes);
	memset(ptr + size - n, 0, n);

	ptr = BUF_COM2H(sb, row);
	memmove(ptr + col, ptr + col + n, nbytes);
	memset(ptr + size - n, 0, n);
    });
    ScrnClrWrapped(screen, row);
}

/*
 * Repaints the area enclosed by the parameters.
 * Requires: (toprow, leftcol), (toprow + nrows, leftcol + ncols) are
 * 	     coordinates of characters in screen;
 *	     nrows and ncols positive.
 *	     all dimensions are based on single-characters.
 */
void
ScrnRefresh(TScreen * screen,
	    int toprow,
	    int leftcol,
	    int nrows,
	    int ncols,
	    Bool force)		/* ... leading/trailing spaces */
{
    int y = toprow * FontHeight(screen) + screen->border;
    int row;
    int topline = screen->topline;
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

    if (screen->cursor_col >= leftcol
	&& screen->cursor_col <= (leftcol + ncols - 1)
	&& screen->cursor_row >= toprow + topline
	&& screen->cursor_row <= maxrow + topline)
	screen->cursor_state = OFF;

    for (row = toprow; row <= maxrow; y += FontHeight(screen), row++) {
#if OPT_ISO_COLORS
#if OPT_EXT_COLORS
	Char *fbf = 0;
	Char *fbb = 0;
#define ColorOf(col) ((fbf[col] << 8) | fbb[col])
#else
	Char *fb = 0;
#define ColorOf(col) (fb[col])
#endif
#endif
#if OPT_DEC_CHRSET
	Char *cb = 0;
#endif
#if OPT_WIDE_CHARS
	int wideness = 0;
	Char *widec = 0;
#define WIDEC_PTR(cell) widec ? &widec[cell] : 0
#endif
	Char cs = 0;
	Char *chars;
	Char *attrs;
	int col = leftcol;
	int maxcol = leftcol + ncols - 1;
	int hi_col = maxcol;
	int lastind;
	int flags;
	int test;
	int fg_bg = 0, fg = 0, bg = 0;
	int x;
	GC gc;
	Boolean hilite;

	if (row < screen->top_marg || row > screen->bot_marg)
	    lastind = row;
	else
	    lastind = row - scrollamt;

	TRACE(("ScrnRefresh row=%d lastind=%d/%d\n", row, lastind, max));
	if (lastind < 0 || lastind > max)
	    continue;

	chars = SCRN_BUF_CHARS(screen, lastind + topline);
	attrs = SCRN_BUF_ATTRS(screen, lastind + topline);

	if_OPT_DEC_CHRSET({
	    cb = SCRN_BUF_CSETS(screen, lastind + topline);
	});

	if_OPT_WIDE_CHARS(screen, {
	    widec = SCRN_BUF_WIDEC(screen, lastind + topline);
	});

	if_OPT_WIDE_CHARS(screen, {
	    /* This fixes an infinite recursion bug, that leads
	       to display anomalies. It seems to be related to 
	       problems with the selection. */
	    if (recurse < 3) {
		/* adjust to redraw all of a widechar if we just wanted 
		   to draw the right hand half */
		if (leftcol > 0 &&
		    (chars[leftcol] | (widec[leftcol] << 8)) == HIDDEN_CHAR &&
		    iswide(chars[leftcol - 1] | (widec[leftcol - 1] << 8))) {
		    leftcol--;
		    ncols++;
		    col = leftcol;
		}
	    } else {
		fprintf(stderr, "This should not happen. Why is it so?\n");
	    }
	});

	if (row < screen->startHRow || row > screen->endHRow ||
	    (row == screen->startHRow && maxcol < screen->startHCol) ||
	    (row == screen->endHRow && col >= screen->endHCol)) {
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
	    /* row does not intersect selection; don't hilite */
	    if (!force) {
		while (col <= maxcol && (attrs[col] & ~BOLD) == 0 &&
		       (chars[col] & ~040) == 0)
		    col++;

		while (col <= maxcol && (attrs[maxcol] & ~BOLD) == 0 &&
		       (chars[maxcol] & ~040) == 0)
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
	    if (row == screen->startHRow && col < screen->startHCol) {
		recurse++;
		ScrnRefresh(screen, row, col, 1, screen->startHCol - col,
			    force);
		col = screen->startHCol;
	    }
	    if (row == screen->endHRow && maxcol >= screen->endHCol) {
		recurse++;
		ScrnRefresh(screen, row, screen->endHCol, 1,
			    maxcol - screen->endHCol + 1, force);
		maxcol = screen->endHCol - 1;
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
	    wideness = iswide(chars[col] | (widec[col] << 8));
	else
	    wideness = 0;
#endif
	if_OPT_EXT_COLORS(screen, {
	    fbf = SCRN_BUF_FGRND(screen, lastind + topline);
	    fbb = SCRN_BUF_BGRND(screen, lastind + topline);
	    fg_bg = ColorOf(col);
	    /* this combines them, then splits them again.  but
	       extract_fg does more, so seems reasonable */
	    fg = extract_fg(fg_bg, flags);
	    bg = extract_bg(fg_bg, flags);
	});
	if_OPT_ISO_TRADITIONAL_COLORS(screen, {
	    fb = SCRN_BUF_COLOR(screen, lastind + topline);
	    fg_bg = ColorOf(col);
	    fg = extract_fg(fg_bg, flags);
	    bg = extract_bg(fg_bg, flags);
	});

	gc = updatedXtermGC(screen, flags, fg_bg, hilite);
	gc_changes |= (flags & (FG_COLOR | BG_COLOR));

	x = CurCursorX(screen, row + topline, col);
	lastind = col;

	for (; col <= maxcol; col++) {
	    if ((attrs[col] != flags)
		|| (hilite && (col > hi_col))
#if OPT_ISO_COLORS
		|| ((flags & FG_COLOR)
		    && (extract_fg(ColorOf(col), attrs[col]) != fg))
		|| ((flags & BG_COLOR)
		    && (extract_bg(ColorOf(col), attrs[col]) != bg))
#endif
#if OPT_WIDE_CHARS
		|| (widec
		    && ((iswide(chars[col] | (widec[col] << 8))) != wideness)
		    && !((chars[col] | (widec[col] << 8)) == HIDDEN_CHAR))
#endif
#if OPT_DEC_CHRSET
		|| (cb[col] != cs)
#endif
		) {
		TRACE(("ScrnRefresh looping drawXtermText %d..%d:%s\n",
		       lastind, col,
		       visibleChars(PAIRED_CHARS(&chars[lastind],
						 WIDEC_PTR(lastind)),
				    col - lastind)));

		test = flags;
		checkVeryBoldColors(test, fg);

		x = drawXtermText(screen, test & DRAWX_MASK, gc, x, y,
				  cs,
				  PAIRED_CHARS(&chars[lastind], WIDEC_PTR(lastind)),
				  col - lastind, 0);

		if_OPT_WIDE_CHARS(screen, {
		    int i;
		    Char *comb1l = BUF_COM1L(screen->visbuf, row + topline);
		    Char *comb2l = BUF_COM2L(screen->visbuf, row + topline);
		    Char *comb1h = BUF_COM1H(screen->visbuf, row + topline);
		    Char *comb2h = BUF_COM2H(screen->visbuf, row + topline);
		    for (i = lastind; i < col; i++) {
			int my_x = CurCursorX(screen, row + topline, i);
			int base = chars[i] | (widec[i] << 8);
			int comb1 = comb1l[i] | (comb1h[i] << 8);
			int comb2 = comb2l[i] | (comb2h[i] << 8);

			if (iswide(base))
			    my_x = CurCursorX(screen, row + topline, i - 1);

			if (comb1 != 0) {
			    drawXtermText(screen, (test & DRAWX_MASK)
					  | NOBACKGROUND, gc, my_x, y, cs,
					  PAIRED_CHARS(comb1l + i, comb1h + i),
					  1, iswide(base));
			}

			if (comb2 != 0) {
			    drawXtermText(screen, (test & DRAWX_MASK)
					  | NOBACKGROUND, gc, my_x, y, cs,
					  PAIRED_CHARS(comb2l + i, comb2h + i),
					  1, iswide(base));
			}
		    }
		});

		resetXtermGC(screen, flags, hilite);

		lastind = col;

		if (hilite && (col > hi_col))
		    hilite = False;

		flags = attrs[col];
		if_OPT_EXT_COLORS(screen, {
		    fg_bg = ColorOf(col);
		    fg = extract_fg(fg_bg, flags);
		    bg = extract_bg(fg_bg, flags);
		});
		if_OPT_ISO_TRADITIONAL_COLORS(screen, {
		    fg_bg = ColorOf(col);
		    fg = extract_fg(fg_bg, flags);
		    bg = extract_bg(fg_bg, flags);
		});
		if_OPT_DEC_CHRSET({
		    cs = cb[col];
		});
#if OPT_WIDE_CHARS
		if (widec)
		    wideness = iswide(chars[col] | (widec[col] << 8));
#endif

		gc = updatedXtermGC(screen, flags, fg_bg, hilite);
		gc_changes |= (flags & (FG_COLOR | BG_COLOR));
	    }

	    if (chars[col] == 0) {
#if OPT_WIDE_CHARS
		if (widec == 0 || widec[col] == 0)
#endif
		    chars[col] = ' ';
	    }
	}

	TRACE(("ScrnRefresh calling drawXtermText %d..%d:%s\n",
	       lastind, col,
	       visibleChars(PAIRED_CHARS(&chars[lastind], WIDEC_PTR(lastind)),
			    col - lastind)));

	test = flags;
	checkVeryBoldColors(test, fg);

	drawXtermText(screen, test & DRAWX_MASK, gc, x, y,
		      cs,
		      PAIRED_CHARS(&chars[lastind], WIDEC_PTR(lastind)),
		      col - lastind, 0);

	if_OPT_WIDE_CHARS(screen, {
	    int i;
	    Char *comb1l = BUF_COM1L(screen->visbuf, row + topline);
	    Char *comb2l = BUF_COM2L(screen->visbuf, row + topline);
	    Char *comb1h = BUF_COM1H(screen->visbuf, row + topline);
	    Char *comb2h = BUF_COM2H(screen->visbuf, row + topline);
	    for (i = lastind; i < col; i++) {
		int my_x = CurCursorX(screen, row + topline, i);
		int base = chars[i] | (widec[i] << 8);
		int comb1 = comb1l[i] | (comb1h[i] << 8);
		int comb2 = comb2l[i] | (comb2h[i] << 8);

		if (iswide(base))
		    my_x = CurCursorX(screen, row + topline, i - 1);

		if (comb1 != 0) {
		    drawXtermText(screen, (test & DRAWX_MASK) |
				  NOBACKGROUND, gc, my_x, y, cs,
				  PAIRED_CHARS(comb1l + i, comb1h + i),
				  1, iswide(base));
		}

		if (comb2 != 0) {
		    drawXtermText(screen, (test & DRAWX_MASK) |
				  NOBACKGROUND, gc, my_x, y, cs,
				  PAIRED_CHARS(comb2l + i, comb2h + i),
				  1, iswide(base));
		}
	    }
	});

	resetXtermGC(screen, flags, hilite);
    }

    /*
     * If we're in color mode, reset the various GC's to the current
     * screen foreground and background so that other functions (e.g.,
     * ClearRight) will get the correct colors.
     */
    if_OPT_ISO_COLORS(screen, {
	if (gc_changes & FG_COLOR)
	    SGR_Foreground(term->cur_foreground);
	if (gc_changes & BG_COLOR)
	    SGR_Background(term->cur_background);
    });

#if defined(__CYGWIN__) && defined(TIOCSWINSZ)
    if (first_time == 1) {
	TTYSIZE_STRUCT ts;

	first_time = 0;
	TTYSIZE_ROWS(ts) = nrows;
	TTYSIZE_COLS(ts) = ncols;
	ts.ws_xpixel = term->core.width;
	ts.ws_ypixel = term->core.height;
	SET_TTYSIZE(screen->respond, ts);
    }
#endif
    recurse--;
}

/*
 * Sets the rows first though last of the buffer of screen to spaces.
 * Requires first <= last; first, last are rows of screen->buf.
 */
void
ClearBufRows(TScreen * screen,
	     int first,
	     int last)
{
    ScrnBuf buf = screen->visbuf;
    int len = screen->max_col + 1;
    int row;
    int flags = TERM_COLOR_FLAGS;

    TRACE(("ClearBufRows %d..%d\n", first, last));
    for (row = first; row <= last; row++) {
	ScrnClrWrapped(screen, row);
	bzero(BUF_CHARS(buf, row), len);
	memset(BUF_ATTRS(buf, row), flags, len);
	if_OPT_EXT_COLORS(screen, {
	    memset(BUF_FGRND(buf, row), term->sgr_foreground, len);
	    memset(BUF_BGRND(buf, row), term->cur_background, len);
	});
	if_OPT_ISO_TRADITIONAL_COLORS(screen, {
	    memset(BUF_COLOR(buf, row), xtermColorPair(), len);
	});
	if_OPT_DEC_CHRSET({
	    memset(BUF_CSETS(buf, row), 0, len);
	});
	if_OPT_WIDE_CHARS(screen, {
	    memset(BUF_WIDEC(buf, row), 0, len);
	    memset(BUF_COM1L(buf, row), 0, len);
	    memset(BUF_COM1H(buf, row), 0, len);
	    memset(BUF_COM2L(buf, row), 0, len);
	    memset(BUF_COM2H(buf, row), 0, len);
	});
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
ScreenResize(TScreen * screen,
	     int width,
	     int height,
	     unsigned *flags)
{
    int code, rows, cols;
    int border = 2 * screen->border;
    int move_down_by;
#ifdef TTYSIZE_STRUCT
    TTYSIZE_STRUCT ts;
#endif
    Window tw = VWindow(screen);

    TRACE(("ScreenResize %dx%d\n", height, width));

    /* clear the right and bottom internal border because of NorthWest
       gravity might have left junk on the right and bottom edges */
    if (width >= FullWidth(screen)) {
	XClearArea(screen->display, tw,
		   FullWidth(screen), 0,	/* right edge */
		   0, height,	/* from top to bottom */
		   False);
    }
    if (height >= FullHeight(screen)) {
	XClearArea(screen->display, tw,
		   0, FullHeight(screen),	/* bottom */
		   width, 0,	/* all across the bottom */
		   False);
    }

    /* round so that it is unlikely the screen will change size on  */
    /* small mouse movements.                                       */
    rows = (height + FontHeight(screen) / 2 - border) /
	FontHeight(screen);
    cols = (width + FontWidth(screen) / 2 - border - ScrollbarWidth(screen)) /
	FontWidth(screen);
    if (rows < 1)
	rows = 1;
    if (cols < 1)
	cols = 1;

    /* update buffers if the screen has changed size */
    if (screen->max_row != rows - 1 || screen->max_col != cols - 1) {
	int savelines = screen->scrollWidget
	? screen->savelines : 0;
	int delta_rows = rows - (screen->max_row + 1);

	TRACE(("...ScreenResize chars %dx%d\n", rows, cols));

	if (screen->cursor_state)
	    HideCursor();
	if (screen->alternate
	    && term->misc.resizeGravity == SouthWestGravity)
	    /* swap buffer pointers back to make all this hair work */
	    SwitchBufPtrs(screen);
	if (screen->altbuf)
	    (void) Reallocate(&screen->altbuf,
			      &screen->abuf_address,
			      rows,
			      cols,
			      screen->max_row + 1,
			      screen->max_col + 1);
	move_down_by = Reallocate(&screen->allbuf,
				  &screen->sbuf_address,
				  rows + savelines, cols,
				  screen->max_row + 1 + savelines,
				  screen->max_col + 1);
	screen->visbuf = &screen->allbuf[MAX_PTRS * savelines];

	screen->max_row += delta_rows;
	screen->max_col = cols - 1;

	if (term->misc.resizeGravity == SouthWestGravity) {
	    screen->savedlines -= move_down_by;
	    if (screen->savedlines < 0)
		screen->savedlines = 0;
	    if (screen->savedlines > screen->savelines)
		screen->savedlines = screen->savelines;
	    if (screen->topline < -screen->savedlines)
		screen->topline = -screen->savedlines;
	    screen->cur_row += move_down_by;
	    screen->cursor_row += move_down_by;
	    ScrollSelection(screen, move_down_by);

	    if (screen->alternate)
		SwitchBufPtrs(screen);	/* put the pointers back */
	}

	/* adjust scrolling region */
	screen->top_marg = 0;
	screen->bot_marg = screen->max_row;
	*flags &= ~ORIGIN;

	if (screen->cur_row > screen->max_row)
	    screen->cur_row = screen->max_row;
	if (screen->cur_col > screen->max_col)
	    screen->cur_col = screen->max_col;

	screen->fullVwin.height = height - border;
	screen->fullVwin.width = width - border - screen->fullVwin.sb_info.width;

    } else if (FullHeight(screen) == height && FullWidth(screen) == width)
	return (0);		/* nothing has changed at all */

    screen->fullVwin.fullheight = height;
    screen->fullVwin.fullwidth = width;

    if (screen->scrollWidget)
	ResizeScrollBar(screen);

    ResizeSelection(screen, rows, cols);

#ifndef NO_ACTIVE_ICON
    if (screen->iconVwin.window) {
	XWindowChanges changes;
	screen->iconVwin.width =
	    (screen->max_col + 1) * screen->iconVwin.f_width;

	screen->iconVwin.height =
	    (screen->max_row + 1) * screen->iconVwin.f_height;

	changes.width = screen->iconVwin.fullwidth =
	    screen->iconVwin.width + 2 * screen->border;

	changes.height = screen->iconVwin.fullheight =
	    screen->iconVwin.height + 2 * screen->border;

	XConfigureWindow(XtDisplay(term), screen->iconVwin.window,
			 CWWidth | CWHeight, &changes);
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

	if (ioctl(screen->respond, TIOCGPGRP, &pgrp) != -1)
	    kill_process_group(pgrp, SIGWINCH);
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
non_blank_line(ScrnBuf sb,
	       int row,
	       int col,
	       int len)
{
    int i;
    Char *ptr = BUF_CHARS(sb, row);

    for (i = col; i < len; i++) {
	if (ptr[i])
	    return True;
    }

    if_OPT_WIDE_CHARS((&(term->screen)), {
	if ((ptr = BUF_WIDEC(sb, row)) != 0) {
	    for (i = col; i < len; i++) {
		if (ptr[i])
		    return True;
	    }
	}
    });

    return False;
}

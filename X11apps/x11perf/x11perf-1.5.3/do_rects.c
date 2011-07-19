/*****************************************************************************
Copyright 1988, 1989 by Digital Equipment Corporation, Maynard, Massachusetts.

                        All Rights Reserved

Permission to use, copy, modify, and distribute this software and its 
documentation for any purpose and without fee is hereby granted, 
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in 
supporting documentation, and that the name of Digital not be
used in advertising or publicity pertaining to distribution of the
software without specific, written prior permission.  

DIGITAL DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
DIGITAL BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.

******************************************************************************/

#include "x11perf.h"
#include "bitmaps.h"

static XRectangle   *rects;
static GC	    pgc;

int 
InitRectangles(XParms xp, Parms p, int reps)
{
    int i;
    int size = p->special;
    int step;
    int x, y;
    int rows;
    int	lw = 0;

    pgc = xp->fggc;

    if (p->bfont)
    {
	lw = atoi (p->bfont);

	XSetLineAttributes(xp->d, xp->bggc, lw, LineSolid, CapButt, JoinMiter);
	XSetLineAttributes(xp->d, xp->fggc, lw, LineSolid, CapButt, JoinMiter);
	lw = (lw >> 1) + 1;
    }

    rects = (XRectangle *)malloc(p->objects * sizeof(XRectangle));
    x = lw;
    y = lw;
    rows = 0;
    if (xp->pack) {
	/* Pack rectangles as close as possible, mainly for debugging faster
	   tiling, stippling routines in a server */
	step = size;
    } else {
	/* Try to exercise all alignments...any odd number is okay */
	step = size + 1 + (size % 2);
    }

    for (i = 0; i != p->objects; i++) {
	rects[i].x = x;
        rects[i].y = y;
	rects[i].width = rects[i].height = size;

	y += step;
	rows++;
	if (y + size > HEIGHT || rows == MAXROWS) {
	    rows = 0;
	    y = lw;
	    x += step;
	    if (x + size > WIDTH) {
		x = lw;
	    }
	}
    }

    SetFillStyle(xp, p);

    return reps;
}

void 
DoRectangles(XParms xp, Parms p, int reps)
{
    int i;

    for (i = 0; i != reps; i++) {
        XFillRectangles(xp->d, xp->w, pgc, rects, p->objects);
        if (pgc == xp->bggc)
            pgc = xp->fggc;
        else
            pgc = xp->bggc;
	CheckAbort ();
    }
}

void 
DoOutlineRectangles(XParms xp, Parms  p, int reps)
{
    int	i;

    for (i = 0; i != reps; i++) {
	XDrawRectangles (xp->d, xp->w, pgc, rects, p->objects);
        if (pgc == xp->bggc)
            pgc = xp->fggc;
        else
            pgc = xp->bggc;
	CheckAbort ();
    }
}

void 
EndRectangles(XParms xp, Parms p)
{
    free(rects);
}


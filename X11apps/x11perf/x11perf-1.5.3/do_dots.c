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

static XPoint   *points;
static GC       pgc;

int 
InitDots(XParms xp, Parms p, int reps)
{
    int i;

    pgc = xp->fggc;

    points = (XPoint *)malloc(p->objects * sizeof(XPoint));

    for (i = 0; i != p->objects; i++) {
	points[i].x = 2 * (i/MAXROWS);
	points[i].y = 2 * (i%MAXROWS);
    }
    return reps;
}

void 
DoDots(XParms xp, Parms p, int reps)
{
    int     i;

    for (i = 0; i != reps; i++) {
        XDrawPoints(xp->d, xp->w, pgc, points, p->objects, CoordModeOrigin);
        if (pgc == xp->bggc)
            pgc = xp->fggc;
        else
            pgc = xp->bggc;
	CheckAbort ();
    }
}

void 
EndDots(XParms xp, Parms p)
{
    free(points);
}


/* $Xorg: do_traps.c,v 1.3 2000/08/17 19:54:10 cpqbld Exp $ */
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
/* $XFree86: xc/programs/x11perf/do_traps.c,v 1.9 2002/12/04 10:28:08 eich Exp $ */

#include "x11perf.h"
#include "bitmaps.h"

#define NUM_POINTS 4   /* 4 points to a trapezoid */
static XPoint *points;
static GC      pgc;

int 
InitTrapezoids(XParms xp, Parms p, int reps)
{
    int     i, numPoints;
    int     rows;
    int     x, y;
    int     size, skew;
    XPoint  *curPoint;

    pgc = xp->fggc;

    size = p->special;
    numPoints = (p->objects) * NUM_POINTS;  
    points = (XPoint *)malloc(numPoints * sizeof(XPoint));
    curPoint = points;
    x = size;
    y = 0;
    rows = 0;
    skew = size;

    for (i = 0; i != p->objects; i++, curPoint += NUM_POINTS) {
	curPoint[0].x = x - skew;
	curPoint[0].y = y;
	curPoint[1].x = x - skew + size;
	curPoint[1].y = y;
	curPoint[2].x = x + skew;
	curPoint[2].y = y + size;
	curPoint[3].x = x + skew - size;
	curPoint[3].y = y + size;

	skew--;
	if (skew < 0) skew = size;

	y += size;
	rows++;
	if (y + size > HEIGHT || rows == MAXROWS) {
	    rows = 0;
	    y = 0;
	    x += 2 * size;
	    if (x + size > WIDTH) {
		x = size;
	    }
	}
    }

    SetFillStyle(xp, p);
    return reps;
}

void 
DoTrapezoids(XParms xp, Parms p, int reps)
{
    int     i, j;
    XPoint  *curPoint;

    for (i = 0; i != reps; i++) {
        curPoint = points;
        for (j = 0; j != p->objects; j++) {
            XFillPolygon(xp->d, xp->w, pgc, curPoint, NUM_POINTS, Convex, 
			 CoordModeOrigin);
            curPoint += NUM_POINTS;
	}
        if (pgc == xp->bggc)
            pgc = xp->fggc;
        else
            pgc = xp->bggc;
	CheckAbort ();
    }
}

void 
EndTrapezoids(XParms xp, Parms p)
{
    free(points);
}

#ifdef XRENDER
#include <X11/extensions/Xrender.h>
#include <X11/Xft/Xft.h>

static XTrapezoid	*traps;
static XftDraw		*aadraw;
static XftColor		aablack, aawhite;
static XRenderPictFormat    *maskFormat;

int
InitFixedTrapezoids(XParms xp, Parms p, int reps)
{
    int     i, numTraps;
    int     rows;
    int     x, y;
    int     size, skew;
    XTrapezoid	*curTrap;
    XRenderColor	color;

    pgc = xp->fggc;

    size = p->special;
    numTraps = p->objects;
    traps = (XTrapezoid *)malloc(numTraps * sizeof(XTrapezoid));
    curTrap = traps;
    x = size;
    y = 0;
    rows = 0;
    skew = size;
    aadraw = XftDrawCreate (xp->d, xp->w, 
			    xp->vinfo.visual, 
			    xp->cmap);
    if (p->font && !strcmp (p->font, "add"))
    {
	XRenderPictFormat   templ;
	templ.type = PictTypeDirect;
	templ.depth = 8;
	templ.direct.alpha = 0;
	templ.direct.alphaMask = 0xff;
	maskFormat = XRenderFindFormat (xp->d, 
					PictFormatType |
					PictFormatDepth |
					PictFormatAlpha |
					PictFormatAlphaMask,
					&templ,
					0);
    }
    else
	maskFormat = 0;
    color.red = 0;
    color.green = 0;
    color.blue = 0;
    color.alpha = 0xffff;
    if (!XftColorAllocValue (xp->d,
			     xp->vinfo.visual, 
			     xp->cmap,
			     &color, &aablack))
    {
	XftDrawDestroy (aadraw);
	aadraw = 0;
	return 0;
    }
    color.red = 0xffff;
    color.green = 0xffff;
    color.blue = 0xffff;
    color.alpha = 0xffff;
    if (!XftColorAllocValue (xp->d,
			     xp->vinfo.visual, 
			     xp->cmap,
			     &color, &aawhite))
    {
	XftDrawDestroy (aadraw);
	aadraw = 0;
	return 0;
    }

    for (i = 0; i != p->objects; i++, curTrap ++) {
	curTrap->top = XDoubleToFixed (y);
	curTrap->bottom = XDoubleToFixed (y + size);
	curTrap->left.p1.x = XDoubleToFixed (x - skew);
	curTrap->left.p1.y = XDoubleToFixed (y);
	curTrap->left.p2.x = XDoubleToFixed (x + skew - size);
	curTrap->left.p2.y = XDoubleToFixed (y + size);
	
	curTrap->right.p1.x = XDoubleToFixed (x - skew + size);
	curTrap->right.p1.y = XDoubleToFixed (y);
	curTrap->right.p2.x = XDoubleToFixed (x + skew);
	curTrap->right.p2.y = XDoubleToFixed (y + size);
	
	skew--;
	if (skew < 0) skew = size;

	y += size;
	rows++;
	if (y + size > HEIGHT || rows == MAXROWS) {
	    rows = 0;
	    y = 0;
	    x += 2 * size;
	    if (x + size > WIDTH) {
		x = size;
	    }
	}
    }

    
    SetFillStyle(xp, p);
    return reps;
}

void 
DoFixedTrapezoids(XParms xp, Parms p, int reps)
{
    int		i;
    XTrapezoid	*curTrap;
    Picture	white, black, src, dst;

    white = XftDrawSrcPicture (aadraw, &aawhite);
    black = XftDrawSrcPicture (aadraw, &aablack);
    dst = XftDrawPicture (aadraw);

    src = black;
    for (i = 0; i != reps; i++) {
        curTrap = traps;
	XRenderCompositeTrapezoids (xp->d, PictOpOver, src, dst, maskFormat,
				    0, 0, traps, p->objects);
        if (src == black)
	    src = white;
        else
            src = black;
	CheckAbort ();
    }
}

void
EndFixedTrapezoids (XParms xp, Parms p)
{
    free (traps);
    XftDrawDestroy (aadraw);
}

#endif /* XRENDER */

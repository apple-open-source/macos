/* $Xorg: do_valgc.c,v 1.3 2000/08/17 19:54:10 cpqbld Exp $ */
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
/* $XFree86: xc/programs/x11perf/do_valgc.c,v 1.5 2001/01/17 23:45:12 dawes Exp $ */

#ifndef VMS
#include <X11/Xatom.h>
#else
#include <decw$include/Xatom.h>
#endif
#include "x11perf.h"

static Window win[2];

int 
InitGC(XParms xp, Parms p, int reps)
{
    win[0] = XCreateSimpleWindow(
	xp->d, xp->w, 10, 10, 10, 10, 1, xp->foreground, xp->background);
    win[1] = XCreateSimpleWindow(
	xp->d, xp->w, 30, 30, 10, 10, 1, xp->foreground, xp->background);
    XMapSubwindows(xp->d, xp->w);
    return reps;
}

void 
DoChangeGC(XParms xp, Parms p, int reps)
{
    int		i;
    XGCValues   gcv;

    for (i = 0; i != reps; i++) {
        gcv.foreground = xp->foreground;
        XChangeGC(xp->d, xp->fggc, GCForeground , &gcv);
        XDrawPoint(xp->d, win[0], xp->fggc, 5, 5);       

        gcv.foreground = xp->background;
        XChangeGC(xp->d, xp->fggc, GCForeground , &gcv);
        XDrawPoint(xp->d, win[1], xp->fggc, 5, 5);       

        gcv.foreground = xp->background;
        XChangeGC(xp->d, xp->fggc, GCForeground , &gcv);
        XDrawPoint(xp->d, win[0], xp->fggc, 5, 5);       

        gcv.foreground = xp->foreground;
        XChangeGC(xp->d, xp->fggc, GCForeground , &gcv);
        XDrawPoint(xp->d, win[1], xp->fggc, 5, 5);       
	CheckAbort ();
    }
}

void 
EndGC(XParms xp, Parms p)
{
    XDestroyWindow(xp->d, win[0]);
    XDestroyWindow(xp->d, win[1]);
}


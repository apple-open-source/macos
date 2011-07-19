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

#ifndef VMS
#include <X11/Xatom.h>
#else
#include <decw$include/Xatom.h>
#endif

#include "x11perf.h"

static Atom XA_PK_TEMP;

void 
DoNoOp(XParms xp, Parms p, int reps)
{
    int     i;

    for (i = 0; i != reps; i++) {
	XNoOp(xp->d);
	CheckAbort ();
    }
}


void 
DoGetAtom(XParms xp, Parms p, int reps)
{
    char    *atom;
    int     i;

    for (i = 0; i != reps; i++) {
	atom = XGetAtomName (xp->d, 1);
 	XFree(atom); /* fix XBUG 6480 */
	CheckAbort ();
    }
}

void 
DoQueryPointer(XParms xp, Parms p, int reps)
{
    int     i;
    Window  w;
    int     x;
    unsigned int m;

    for (i = 0; i != reps; i++) {
	XQueryPointer (xp->d, xp->w, &w, &w, &x, &x, &x, &x, &m);
	CheckAbort ();
    }
}

int 
InitGetProperty(XParms xp, Parms p, int reps)
{
    long foo[4];

    foo[0] = 41;
    foo[1] = 14;
    foo[2] = 37;
    foo[3] = 73;
    XA_PK_TEMP = XInternAtom (xp->d, "_PK_TEMP", False);
    XChangeProperty (
	    xp->d, xp->w, XA_PK_TEMP, XA_INTEGER, 32,
	    PropModeReplace, (unsigned char *)foo, 4);
    return reps;
}

void 
DoGetProperty(XParms xp, Parms p, int reps)
{
    int     i;
    int     actual_format;
    unsigned long actual_length, bytes_remaining;
    unsigned char *prop;
    
    Atom actual_type;

    for (i = 0; i != reps; i++) {
	XGetWindowProperty (
		xp->d, xp->w, XA_PK_TEMP, 0, 4,
		False, AnyPropertyType, &actual_type, &actual_format,
		&actual_length, &bytes_remaining, &prop);
	CheckAbort ();
	XFree(prop);
    }
}

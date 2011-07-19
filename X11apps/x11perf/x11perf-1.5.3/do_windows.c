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

static Window *parents;
static Window *isolates;
static int childrows, childcolumns, childwindows;
static int parentrows, parentcolumns, parentwindows;
static int parentwidth, parentheight;
static Window popup;

static void 
ComputeSizes(XParms xp, Parms p)
{
    childwindows = p->objects;
    childrows = (childwindows + MAXCOLS - 1) / MAXCOLS;
    childcolumns = (childrows > 1 ? MAXCOLS : childwindows);

    parentwidth = (CHILDSIZE+CHILDSPACE) * childcolumns;
    parentheight = (CHILDSIZE+CHILDSPACE) * childrows;
}

int 
CreateParents(XParms xp, Parms p, int reps)
{
    int     i;

    ComputeSizes(xp, p);

    parentcolumns = WIDTH / parentwidth;
    parentrows = HEIGHT / parentheight;
    parentwindows = parentcolumns * parentrows; /* Max reps we can fit */

    if (parentwindows > reps) {
	parentwindows = reps;
    }

    /* We will do parentwindows sets of childwindows, in order to get better
       timing accuracy.  Creating 4 windows at a millisecond apiece or so
       is a bit faster than the 60 Hz clock. */
    isolates = (Window *)malloc(parentwindows * sizeof(Window));
    parents = (Window *)malloc(parentwindows * sizeof(Window));

    /*
     *  Create isolation windows for the parents, and then the parents
     *  themselves.  These isolation windows ensure that parent and children
     *  windows created/mapped in DoWins and DoWin2 all see the same local
     *  environment...the parent is an only child, and each parent contains
     *  the number of children we are trying to get benchmarks on.
     */

    for (i = 0; i != parentwindows; i++) {
	isolates[i] = XCreateSimpleWindow(xp->d, xp->w,
	    (i/parentrows)*parentwidth, (i%parentrows)*parentheight,
	    parentwidth, parentheight, 0, xp->background, xp->background);
	parents[i] = XCreateSimpleWindow(xp->d, isolates[i],
	    0, 0, parentwidth, parentheight, 0, xp->background, xp->background);
    }

    XMapSubwindows(xp->d, xp->w);
    return parentwindows;
} /* CreateParents */


void 
MapParents(XParms xp, Parms p, int reps)
{
    int i;

    for (i = 0; i != parentwindows; i++) {
	XMapWindow(xp->d, parents[i]);
    }
}

void 
MapParentsCleanup(XParms xp, Parms p)
{
    int i;

    for (i = 0; i != parentwindows; i++) {
	XMapWindow(xp->d, parents[i]);
    }
}


int 
InitCreate(XParms xp, Parms p, int reps)
{
    reps = CreateParents(xp, p, reps);
    MapParents(xp, p, reps);
    return reps;
}

static void 
CreateChildGroup(XParms xp, Parms p, Window parent)
{
    int j;

    for (j = 0; j != childwindows; j++) {
	(void) XCreateSimpleWindow (xp->d, parent,
		(CHILDSIZE+CHILDSPACE) * (j/childrows) + CHILDSPACE/2,
		(CHILDSIZE+CHILDSPACE) * (j%childrows) + CHILDSPACE/2,
		CHILDSIZE, CHILDSIZE, 0, xp->background, xp->foreground);
    }

    if (p->special)
	XMapSubwindows (xp->d, parent);
}

void 
CreateChildren(XParms xp, Parms p, int reps)
{
    int     i;

    for (i = 0; i != parentwindows; i++) {
	CreateChildGroup(xp, p, parents[i]);
    } /* end i */
}

void 
DestroyChildren(XParms xp, Parms p)
{
    int i;

    for (i = 0; i != parentwindows; i++) {
	XDestroySubwindows(xp->d, parents[i]);
    }
}

void 
EndCreate(XParms xp, Parms p)
{
    XDestroySubwindows(xp->d, xp->w);
    free(parents);
    free(isolates);
}


int 
InitMap(XParms xp, Parms p, int reps)
{
    reps = CreateParents(xp, p, reps);
    CreateChildren(xp, p, reps);
    return reps;
}

void 
UnmapParents(XParms xp, Parms p, int reps)
{
    int i;

    for (i = 0; i != parentwindows; i++) {
	XUnmapWindow(xp->d, parents[i]);
    }
}

void 
UnmapParentsCleanup(XParms xp, Parms p)
{
    int i;

    for (i = 0; i != parentwindows; i++) {
	XUnmapWindow(xp->d, parents[i]);
    }
}

int 
InitDestroy(XParms xp, Parms p, int reps)
{
    reps = CreateParents(xp, p, reps);
    CreateChildren(xp, p, reps);
    MapParents(xp, p, reps);
    return reps;
}

void 
DestroyParents(XParms xp, Parms p, int reps)
{
    int i;

    for (i = 0; i != parentwindows; i++) {
	XDestroyWindow(xp->d, parents[i]);
    }
}


void 
RenewParents(XParms xp, Parms p)
{
    int i;

    for (i = 0; i != parentwindows; i++) {
	parents[i] = XCreateSimpleWindow(xp->d, isolates[i],
	    0, 0, parentwidth, parentheight, 0, xp->background, xp->background);
    }
    CreateChildren(xp, p, parentwindows);
    MapParents(xp, p, parentwindows);
}

int 
InitPopups(XParms xp, Parms p, int reps)
{
#ifdef CHILDROOT
    XWindowAttributes    xwa;
#endif
    XSetWindowAttributes xswa;
    Window isolate;

#ifdef CHILDROOT
    ComputeSizes(xp, p);
    CreateChildGroup(xp, p, xp->w);

    /* Now create simple window to pop up over children */
    (void) XGetWindowAttributes(xp->d, xp->w, &xwa);
    xswa.override_redirect = True;
    popup =  XCreateSimpleWindow (
	    xp->d, DefaultRootWindow(xp->d), 
	    xwa.x + xwa.border_width, xwa.y + xwa.border_width,
	    parentwidth, parentheight,
	    0, xp->foreground, xp->foreground);
#else   
    isolate = XCreateSimpleWindow(
	    xp->d, xp->w, 0, 0, WIDTH, HEIGHT,
	    0, xp->background, xp->background);

    ComputeSizes(xp, p);
    CreateChildGroup(xp, p, isolate);
    XMapWindow(xp->d, isolate);

    /* Now create simple window to pop up over children */
    xswa.override_redirect = True;
    popup =  XCreateSimpleWindow (
	    xp->d, xp->w, 0, 0,
	    parentwidth, parentheight,
	    0, xp->foreground, xp->foreground);
#endif
    XChangeWindowAttributes (xp->d, popup, CWOverrideRedirect, &xswa);
    return reps;
}

void 
DoPopUps(XParms xp, Parms p, int reps)
{
    int i;
    for (i = 0; i != reps; i++) {
        XMapWindow(xp->d, popup);
	XUnmapWindow(xp->d, popup);
	CheckAbort ();
    }
}

void 
EndPopups(XParms xp, Parms p)
{
    XDestroySubwindows(xp->d, xp->w);
#ifdef CHILDROOT
    XDestroyWindow(xp->d, popup);
#endif
}


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

#if HAVE_CONFIG_H
#include "config.h"
#endif

#ifndef VMS
#include <X11/Xlib.h>
#include <stdlib.h>
#include <X11/Xutil.h>
#else
#include <decw$include/Xlib.h>
#include <decw$include/Xutil.h>
#endif
#if defined(XlibSpecificationRelease) && XlibSpecificationRelease >= 5
#include <X11/Xfuncs.h>
#endif
#include <stddef.h>
#include <X11/extensions/Xrender.h>

#define POLY	     1000       /* # (small) items in poly calls	*/
#define MAXROWS	       40       /* Max rows of items in poly calls      */
#define MAXCOLS	       25       /* Max columns of items			*/

#define WIDTH         600	/* Size of large window to work within  */
#define HEIGHT        600

#define CHILDSIZE       8       /* Size of children on windowing tests  */
#define CHILDSPACE      4       /* Space between children		*/

#define BigTile	((char *)2)		/* Big tile/stipple */
#define OddTile	((char *)1)		/* Odd sized tile/stipple */

#define PictStandardNative    5

typedef unsigned char Version;

#define VERSION1_2  ((Version)(1 << 0))
#define VERSION1_3  ((Version)(1 << 1))
#define VERSION1_4  ((Version)(1 << 2))
#define VERSION1_5  ((Version)(1 << 3))
#define VERSION1_6  ((Version)(1 << 4))
#define V1_2ONLY VERSION1_2
#define V1_2FEATURE	(VERSION1_2 | VERSION1_3 | VERSION1_4 | VERSION1_5 | VERSION1_6)
#define V1_3FEATURE	(VERSION1_3 | VERSION1_4 | VERSION1_5 | VERSION1_6)
#define V1_4FEATURE	(VERSION1_4 | VERSION1_5 | VERSION1_6)
#define V1_5FEATURE     (VERSION1_5 | VERSION1_6)
#define V1_6FEATURE     (VERSION1_6)

typedef struct _Parms {
    /* Required fields */
    int  objects;       /* Number of objects to process in one X call	    */

    /* Optional fields.  (Wouldn't object-oriented programming be nice ?)   */
    int  special;       /* Usually size of objects to paint		    */
    char *font, *bfont;
    int  fillStyle;     /* Solid, transparent stipple, opaque stipple, tile */
} ParmRec, *Parms;

typedef struct _XParms {
    Display	    *d;
    Window	    w;
    Pixmap          p;
    GC		    fggc;
    GC		    bggc;
    GC		    ddfggc;
    GC		    ddbggc;
    Picture	    w_picture;
    Picture	    p_picture;
    unsigned long   foreground;
    unsigned long   background;
    unsigned long   ddbackground;
    XVisualInfo     vinfo;
    Bool	    pack;
    Version	    version;
    Bool	    save_under;
    int		    backing_store;
    unsigned long   planemask;
    int		    func;
    int		    format;
    Colormap	    cmap;
} XParmRec, *XParms;

typedef int (*InitProc)(XParms xp, Parms p, int reps);
typedef void (*Proc)(XParms xp, Parms p, int reps);
typedef void (*CleanupProc)(XParms xp, Parms p);

typedef enum {
    WINDOW,     /* Windowing test, rop, planemask have no affect	*/
    ROP,	/* Graphics test, rop, planemask has some affect	*/
    PLANEMASK,  /* Graphics test, rop no affect, planemask some affect  */
    NONROP,     /* Graphics or overhead test, rop has no affect		*/
    COMP        /* Graphics test, render op                             */
} TestType;

typedef struct _Test {
    char	*option;    /* Name to use in prompt line		    */
    char	*label;     /* Fuller description of test		    */
    char	*label14;   /* Labels that are different in Version 1.4     */
    InitProc    init;       /* Initialization procedure			    */
    Proc	proc;       /* Timed benchmark procedure		    */
    CleanupProc	passCleanup;/* Cleanup between repetitions of same test     */
    CleanupProc	cleanup;    /* Cleanup after test			    */
    Version     versions;   /* Test in 1.2 only, 1.3 only, or both	    */
    TestType    testType;   /* Windowing, graphics rop, graphics non-rop    */
    int		clips;      /* Number of obscuring windows to force clipping*/
    ParmRec     parms;      /* Parameters passed to test procedures	    */
} Test;

extern void NullProc(XParms xp, Parms p);
extern int NullInitProc(XParms xp, Parms p, int reps);

extern Test test[];

extern int abortTest;

extern void AbortTest (void);

#define CheckAbort()  if (abortTest) AbortTest ()
#define ForEachTest(x) for (x = 0; test[x].option != NULL; x++)


/* do_arcs.c */
extern int InitCircles ( XParms xp, Parms p, int reps );
extern int InitPartCircles ( XParms xp, Parms p, int reps );
extern int InitChordPartCircles ( XParms xp, Parms p, int reps );
extern int InitSlicePartCircles ( XParms xp, Parms p, int reps );
extern int InitWideCircles ( XParms xp, Parms p, int reps );
extern int InitPartWideCircles ( XParms xp, Parms p, int reps );
extern int InitDashedCircles ( XParms xp, Parms p, int reps );
extern int InitWideDashedCircles ( XParms xp, Parms p, int reps );
extern int InitDoubleDashedCircles ( XParms xp, Parms p, int reps );
extern int InitWideDoubleDashedCircles ( XParms xp, Parms p, int reps );
extern int InitEllipses ( XParms xp, Parms p, int reps );
extern int InitPartEllipses ( XParms xp, Parms p, int reps );
extern int InitChordPartEllipses ( XParms xp, Parms p, int reps );
extern int InitSlicePartEllipses ( XParms xp, Parms p, int reps );
extern int InitWideEllipses ( XParms xp, Parms p, int reps );
extern int InitPartWideEllipses ( XParms xp, Parms p, int reps );
extern int InitDashedEllipses ( XParms xp, Parms p, int reps );
extern int InitWideDashedEllipses ( XParms xp, Parms p, int reps );
extern int InitDoubleDashedEllipses ( XParms xp, Parms p, int reps );
extern int InitWideDoubleDashedEllipses ( XParms xp, Parms p, int reps );
extern void DoArcs ( XParms xp, Parms p, int reps );
extern void DoFilledArcs ( XParms xp, Parms p, int reps );
extern void EndArcs ( XParms xp, Parms p );

/* do_blt.c */
extern int InitScroll ( XParms xp, Parms p, int reps );
extern void DoScroll ( XParms xp, Parms p, int reps );
extern void MidScroll ( XParms xp, Parms p );
extern void EndScroll ( XParms xp, Parms p );
extern int InitCopyWin ( XParms xp, Parms p, int reps );
extern int InitCopyPix ( XParms xp, Parms p, int reps );
extern int InitGetImage ( XParms xp, Parms p, int reps );
extern int InitPutImage ( XParms xp, Parms p, int reps );
extern void DoCopyWinWin ( XParms xp, Parms p, int reps );
extern void DoCopyPixWin ( XParms xp, Parms p, int reps );
extern void DoCopyWinPix ( XParms xp, Parms p, int reps );
extern void DoCopyPixPix ( XParms xp, Parms p, int reps );
extern void DoGetImage ( XParms xp, Parms p, int reps );
extern void DoPutImage ( XParms xp, Parms p, int reps );
#ifdef MITSHM
extern int InitShmPutImage ( XParms xp, Parms p, int reps );
extern void DoShmPutImage ( XParms xp, Parms p, int reps );
extern void EndShmPutImage ( XParms xp, Parms p );
#endif
extern void MidCopyPix ( XParms xp, Parms p );
extern void EndCopyWin ( XParms xp, Parms p );
extern void EndCopyPix ( XParms xp, Parms p );
extern void EndGetImage ( XParms xp, Parms p );
extern int InitCopyPlane ( XParms xp, Parms p, int reps );
extern void DoCopyPlane ( XParms xp, Parms p, int reps );

int
InitCompositeWin(XParms xp, Parms p, int reps);

void
EndCompositeWin (XParms xp, Parms p);

void
DoCompositeWinWin (XParms xp, Parms p, int reps);

int
InitCompositePix(XParms xp, Parms p, int reps);

void
EndCompositePix (XParms xp, Parms p);

void
DoCompositePixWin (XParms xp, Parms p, int reps);

/* do_complex.c */
extern int InitComplexPoly ( XParms xp, Parms p, int reps );
extern void DoComplexPoly ( XParms xp, Parms p, int reps );
extern void EndComplexPoly ( XParms xp, Parms p );
extern int InitGeneralPoly ( XParms xp, Parms p, int reps );
extern void DoGeneralPoly ( XParms xp, Parms p, int reps );

/* do_dots.c */
extern int InitDots ( XParms xp, Parms p, int reps );
extern void DoDots ( XParms xp, Parms p, int reps );
extern void EndDots ( XParms xp, Parms p );

/* do_lines.c */
extern int InitLines ( XParms xp, Parms p, int reps );
extern int InitWideLines ( XParms xp, Parms p, int reps );
extern int InitDashedLines ( XParms xp, Parms p, int reps );
extern int InitWideDashedLines ( XParms xp, Parms p, int reps );
extern int InitDoubleDashedLines ( XParms xp, Parms p, int reps );
extern int InitWideDoubleDashedLines ( XParms xp, Parms p, int reps );
extern void DoLines ( XParms xp, Parms p, int reps );
extern void EndLines ( XParms xp, Parms p );

/* do_movewin.c */
extern int InitMoveWindows ( XParms xp, Parms p, int reps );
extern void DoMoveWindows ( XParms xp, Parms p, int reps );
extern void EndMoveWindows ( XParms xp, Parms p );
extern void DoResizeWindows ( XParms xp, Parms p, int reps );
extern int InitCircWindows ( XParms xp, Parms p, int reps );
extern void DoCircWindows ( XParms xp, Parms p, int reps );
extern void EndCircWindows ( XParms xp, Parms p );
extern int InitMoveTree ( XParms xp, Parms p, int reps );
extern void DoMoveTree ( XParms xp, Parms p, int reps );
extern void EndMoveTree ( XParms xp, Parms p );

/* do_rects.c */
extern int InitRectangles ( XParms xp, Parms p, int reps );
extern void DoRectangles ( XParms xp, Parms p, int reps );
extern void DoOutlineRectangles ( XParms xp, Parms p, int reps );
extern void EndRectangles ( XParms xp, Parms p );

/* do_segs.c */
extern int InitSegments ( XParms xp, Parms p, int reps );
extern int InitDashedSegments ( XParms xp, Parms p, int reps );
extern int InitDoubleDashedSegments ( XParms xp, Parms p, int reps );
extern int InitHorizSegments ( XParms xp, Parms p, int reps );
extern int InitWideHorizSegments ( XParms xp, Parms p, int reps );
extern int InitVertSegments ( XParms xp, Parms p, int reps );
extern int InitWideVertSegments ( XParms xp, Parms p, int reps );
extern void DoSegments ( XParms xp, Parms p, int reps );
extern void EndSegments ( XParms xp, Parms p );

/* do_simple.c */
extern void DoNoOp ( XParms xp, Parms p, int reps );
extern void DoGetAtom ( XParms xp, Parms p, int reps );
extern void DoQueryPointer ( XParms xp, Parms p, int reps );
extern int InitGetProperty ( XParms xp, Parms p, int reps );
extern void DoGetProperty ( XParms xp, Parms p, int reps );

/* do_text.c */
extern int InitText ( XParms xp, Parms p, int reps );
extern int InitText16 ( XParms xp, Parms p, int reps );
extern void DoText ( XParms xp, Parms p, int reps );
extern void DoText16 ( XParms xp, Parms p, int reps );
extern void DoPolyText ( XParms xp, Parms p, int reps );
extern void DoPolyText16 ( XParms xp, Parms p, int reps );
extern void DoImageText ( XParms xp, Parms p, int reps );
extern void DoImageText16 ( XParms xp, Parms p, int reps );
extern void ClearTextWin ( XParms xp, Parms p );
extern void EndText ( XParms xp, Parms p );
extern void EndText16 ( XParms xp, Parms p );
#ifdef XFT
extern int InitAAText (XParms xp, Parms p, int reps );
extern void DoAAText (XParms xp, Parms p, int reps );
extern void EndAAText ( XParms xp, Parms p );
#endif

/* do_traps.c */
extern int InitTrapezoids ( XParms xp, Parms p, int reps );
extern void DoTrapezoids ( XParms xp, Parms p, int reps );
extern void EndTrapezoids ( XParms xp, Parms p );
#if defined(XRENDER) && defined(XFT)
extern int InitFixedTraps ( XParms xp, Parms p, int reps );
extern void DoFixedTraps ( XParms xp, Parms p, int reps );
extern void EndFixedTraps ( XParms xp, Parms p );
extern int InitFixedTrapezoids ( XParms xp, Parms p, int reps );
extern void DoFixedTrapezoids ( XParms xp, Parms p, int reps );
extern void EndFixedTrapezoids ( XParms xp, Parms p );
#endif

/* do_tris.c */
extern int InitTriangles ( XParms xp, Parms p, int reps );
extern void DoTriangles ( XParms xp, Parms p, int reps );
extern void EndTriangles ( XParms xp, Parms p );

/* do_valgc.c */
extern int InitGC ( XParms xp, Parms p, int reps );
extern void DoChangeGC ( XParms xp, Parms p, int reps );
extern void EndGC ( XParms xp, Parms p );

/* do_windows.c */
extern int CreateParents ( XParms xp, Parms p, int reps );
extern void MapParents ( XParms xp, Parms p, int reps );
extern void MapParentsCleanup ( XParms xp, Parms p );
extern int InitCreate ( XParms xp, Parms p, int reps );
extern void CreateChildren ( XParms xp, Parms p, int reps );
extern void DestroyChildren ( XParms xp, Parms p );
extern void EndCreate ( XParms xp, Parms p );
extern int InitMap ( XParms xp, Parms p, int reps );
extern void UnmapParents ( XParms xp, Parms p, int reps );
extern void UnmapParentsCleanup ( XParms xp, Parms p);
extern int InitDestroy ( XParms xp, Parms p, int reps );
extern void DestroyParents ( XParms xp, Parms p, int reps );
extern void RenewParents ( XParms xp, Parms p );
extern int InitPopups ( XParms xp, Parms p, int reps );
extern void DoPopUps ( XParms xp, Parms p, int reps );
extern void EndPopups ( XParms xp, Parms p );


/*****************************************************************************

About x11perf:

Many graphics benchmarks assume that the graphics device is used to display the
output of a single fancy graphics application, and that the user gets his work
done on some other device, like a terminal.  Such benchmarks usually measure
drawing speed for lines, polygons, text, etc.

Since workstations are not used as standalone graphics engines, but as
super-terminals, x11perf measures window management performance as well as
traditional graphics performace.  x11perf includes benchmarks for the time it
takes to create and map windows (as when you start up an application); to map a
pre-existing set of windows onto the screen (as when you deiconify an
application or pop up a menu); and to rearrange windows (as when you slosh
windows to and fro trying to find the one you want).

x11perf also measures graphics performance for operations not normally used in
standalone graphics displays, but are nonetheless used frequently by X
applications.  Such operations include CopyPlane (used by the PostScript
previewer), scrolling (used in text windows), and various stipples and tiles
(used for CAD and half-toning, respectively).

x11perf DOES NOT attempt to whittle down measurements to a single ``HeXStones''
number.  We consider such numbers to be uninformative at best and misleading at
worst.  x11perf should be used to analyze particular strengths and weaknesses
of servers, and is most useful when used by a server writer who wants to
analyze and improve a server.

For repeatable results, x11perf should be run using a local connection on a
freshly-started server.  The default configuration runs each test 5 times, in
order to see if each trial takes approximately the same amount of time.
Strange glitches should be examined; if non-repeatable I chalk them up to
daemons and network traffic.  Each trial is run for 5 seconds, in order to
reduce random time differences.  The number of objects processed per second is
displayed to 3 significant digits, but you'll be lucky on most UNIX system if
the numbers are actually consistent to 2 digits.

The current program is mostly the responsibility of Joel McCormack.  It is
based upon the x11perf developed by Phil Karlton, Susan Angebranndt, and Chris
Kent, who wanted to assess performance differences between various servers.
Mary Walker, Todd Newman, and I added several tests in order to write and tune
the pmax (DECStation 3100) servers.  For a general release to the world, I've
basically rewritten x11perf to ease making comparisons between widely varying
machines, to cover most important X functionality (the notable exception being
wide lines), and to exercise graphics operations in as many different
orientations and alignments as possible.

******************************************************************************/

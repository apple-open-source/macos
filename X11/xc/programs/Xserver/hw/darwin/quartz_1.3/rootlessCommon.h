/*
 * Common internal rootless definitions and code
 *
 * Greg Parker     gparker@cs.stanford.edu
 */
/* $XFree86: xc/programs/Xserver/hw/darwin/quartz_1.3/rootlessCommon.h,v 1.1 2002/03/28 02:21:20 torrey Exp $ */

#ifndef _ROOTLESSCOMMON_H
#define _ROOTLESSCOMMON_H

#include "rootless.h"

#include "pixmapstr.h"
#include "windowstr.h"

#ifdef RENDER
#include "picturestr.h"
#endif


// Debug output, or not.
#ifdef ROOTLESSDEBUG
#define RL_DEBUG_MSG ErrorF
#else
#define RL_DEBUG_MSG(a, ...)
#endif


// Global variables
extern int rootlessGCPrivateIndex;
extern int rootlessScreenPrivateIndex;
extern int rootlessWindowPrivateIndex;


// RootlessGCRec: private per-gc data
typedef struct {
    GCFuncs *originalFuncs;
    GCOps *originalOps;
} RootlessGCRec;


// RootlessWindowRec: private per-window data
typedef struct RootlessWindowRec {
    RootlessFrameRec frame;
    RegionRec damage;
    unsigned int borderWidth; // needed for MoveWindow(VTOther) (%$#@!!!)
    PixmapPtr pixmap;
#ifdef SHAPE
    BOOL shapeDamage; // TRUE if shape has changed
#endif
} RootlessWindowRec;


// RootlessScreenRec: per-screen private data
typedef struct {
    ScreenPtr pScreen;
    RootlessFrameProcs frameProcs;

    CloseScreenProcPtr CloseScreen;

    CreateWindowProcPtr CreateWindow;
    DestroyWindowProcPtr DestroyWindow;
    RealizeWindowProcPtr RealizeWindow;
    UnrealizeWindowProcPtr UnrealizeWindow;
    MoveWindowProcPtr MoveWindow;
    ResizeWindowProcPtr ResizeWindow;
    RestackWindowProcPtr RestackWindow;
    ChangeBorderWidthProcPtr ChangeBorderWidth;
    PositionWindowProcPtr PositionWindow;
    ChangeWindowAttributesProcPtr ChangeWindowAttributes;

    CreateGCProcPtr CreateGC;
    PaintWindowBackgroundProcPtr PaintWindowBackground;
    PaintWindowBorderProcPtr PaintWindowBorder;
    CopyWindowProcPtr CopyWindow;
    GetImageProcPtr GetImage;

    MarkOverlappedWindowsProcPtr MarkOverlappedWindows;
    ValidateTreeProcPtr ValidateTree;

#ifdef SHAPE
    SetShapeProcPtr SetShape;
#endif

#ifdef RENDER
    CompositeProcPtr Composite;
    GlyphsProcPtr Glyphs;
#endif

} RootlessScreenRec;


// "Definition of the Porting Layer for the X11 Sample Server" says
// unwrap and rewrap of screen functions is unnecessary, but
// screen->CreateGC changes after a call to cfbCreateGC.

#define SCREEN_UNWRAP(screen, fn) \
    screen->fn = SCREENREC(screen)->fn;

#define SCREEN_WRAP(screen, fn) \
    SCREENREC(screen)->fn = screen->fn; \
    screen->fn = Rootless##fn


// Accessors for screen and window privates

#define SCREENREC(pScreen) \
   ((RootlessScreenRec*)(pScreen)->devPrivates[rootlessScreenPrivateIndex].ptr)

#define WINREC(pWin) \
    ((RootlessWindowRec *)(pWin)->devPrivates[rootlessWindowPrivateIndex].ptr)


// Call a rootless implementation function.
// Many rootless implementation functions are allowed to be NULL.
#define CallFrameProc(pScreen, proc, params) \
    if (SCREENREC(pScreen)->frameProcs.proc) { \
        RL_DEBUG_MSG("calling frame proc " #proc " "); \
        SCREENREC(pScreen)->frameProcs.proc params; \
    }


// BoxRec manipulators
// Copied from shadowfb

#define TRIM_BOX(box, pGC) { \
    BoxPtr extents = &pGC->pCompositeClip->extents;\
    if(box.x1 < extents->x1) box.x1 = extents->x1; \
    if(box.x2 > extents->x2) box.x2 = extents->x2; \
    if(box.y1 < extents->y1) box.y1 = extents->y1; \
    if(box.y2 > extents->y2) box.y2 = extents->y2; \
}

#define TRANSLATE_BOX(box, pDraw) { \
    box.x1 += pDraw->x; \
    box.x2 += pDraw->x; \
    box.y1 += pDraw->y; \
    box.y2 += pDraw->y; \
}

#define TRIM_AND_TRANSLATE_BOX(box, pDraw, pGC) { \
    TRANSLATE_BOX(box, pDraw); \
    TRIM_BOX(box, pGC); \
}

#define BOX_NOT_EMPTY(box) \
    (((box.x2 - box.x1) > 0) && ((box.y2 - box.y1) > 0))


// HUGE_ROOT and NORMAL_ROOT
// We don't want to clip windows to the edge of the screen.
// HUGE_ROOT temporarily makes the root window really big.
// This is needed as a wrapper around any function that calls
// SetWinSize or SetBorderSize which clip a window against its
// parents, including the root.

extern RegionRec rootlessHugeRoot;

#define HUGE_ROOT(pWin) \
    { \
        WindowPtr w = pWin; \
        while (w->parent) w = w->parent; \
        saveRoot = w->winSize; \
        w->winSize = rootlessHugeRoot; \
    }

#define NORMAL_ROOT(pWin) \
    { \
        WindowPtr w = pWin; \
        while (w->parent) w = w->parent; \
        w->winSize = saveRoot; \
    }


// Returns TRUE if this window is a top-level window (i.e. child of the root)
// The root is not a top-level window.
#define IsTopLevel(pWin) \
    ((pWin)  &&  (pWin)->parent  &&  !(pWin)->parent->parent)

// Returns TRUE if this window is a root window
#define IsRoot(pWin) \
    ((pWin) == WindowTable[(pWin)->drawable.pScreen->myNum])

// Returns the top-level parent of pWindow.
// The root is the top-level parent of itself, even though the root is
// not otherwise considered to be a top-level window.
WindowPtr TopLevelParent(WindowPtr pWindow);

// Returns TRUE if this window is visible inside a frame
// (e.g. it is visible and has a top-level or root parent)
Bool IsFramedWindow(WindowPtr pWin);

// Move the given pixmap's base address to where pixel (0, 0)
// would be if the pixmap's actual data started at (x, y).
void SetPixmapBaseToScreen(PixmapPtr pix, int x, int y);

// Update pWindow's pixmap.
// This needs to be called every time a window moves relative to
// its top-level parent, or the parent's pixmap data is reallocated.
void UpdatePixmap(WindowPtr pWindow);

// Routines that cause regions to get redrawn.
// DamageRegion and DamageRect are in global coordinates.
// DamageBox is in window-local coordinates.
void RootlessDamageRegion(WindowPtr pWindow, RegionPtr pRegion);
void RootlessDamageRect(WindowPtr pWindow, int x, int y, int w, int h);
void RootlessDamageBox(WindowPtr pWindow, BoxPtr pBox);
void RootlessRedisplay(WindowPtr pWindow);
void RootlessRedisplayScreen(ScreenPtr pScreen);

// Window reshape needs to be updated. The reshape also forces complete redraw.
void RootlessDamageShape(WindowPtr pWin);

#endif // _ROOTLESSCOMMON_H

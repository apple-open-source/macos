/*
 * External interface to generic rootless mode
 *
 * Greg Parker     gparker@cs.stanford.edu     March 3, 2001
 */
/* $XFree86: xc/programs/Xserver/hw/darwin/quartz_1.3/rootless.h,v 1.1 2002/03/28 02:21:20 torrey Exp $ */

#ifndef _ROOTLESS_H
#define _ROOTLESS_H

#include "mi.h"
#include "gcstruct.h"

// RootlessFrameRec
// Describes a single rootless window (aka frame).
// The rootless mode keeps track of window position, and the 
// rootless implementation is responsible for the pixmap.
// Multiple screens: all coordinates are SCREEN-LOCAL, not global.


typedef struct RootlessFrameRec {
    /* Data maintained by rootless mode */
    /* position and size, including window border, in screen coordinates */
    int x;
    int y;
    unsigned int w;
    unsigned int h;
    WindowPtr win;    /* the top-level window drawn in this frame */
    int isRoot;       /* TRUE if this is the root window */

    /* Data maintained by rootless implementation */
    char *pixelData;
    int depth; // color bits per pixel; depth <= bitsPerPixel
    int bitsPerPixel;
    int bytesPerRow;

    void *devPrivate; /* for caller's use */
} RootlessFrameRec, *RootlessFramePtr;


// Create a new frame.
// pUpper is the window above the new frame, or NULL if the new 
//  frame will be on top.
// pFrame is completely initialized. devPrivate is NULL
// The pixmap must be valid when this is done.
typedef void (*RootlessCreateFrameProc)
    (ScreenPtr pScreen, RootlessFramePtr pFrame, RootlessFramePtr pUpper);

// Destroy a frame. Caller must free any private data and the pixmap.
// All drawing is stopped and all updates are flushed before this is called.
typedef void (*RootlessDestroyFrameProc)
    (ScreenPtr pScreen, RootlessFramePtr pFrame);

// Move a frame on screen.
// The frame changes position and nothing else.
// pFrame and pFrame->win already contain the information about the 
// new position. oldX and oldY are the old position.
// All updates are flushed before this is called.
// The pixmap may change during this function.
typedef void (*RootlessMoveFrameProc) 
    (ScreenPtr pScreen, RootlessFramePtr pFrame, int oldX, int oldY);

// Change frame ordering (aka stacking, layering)
// pFrame->win already has its new siblings.
// pOldNext is the window that was below this one, or NULL if this was 
//  at the bottom. 
// pNewNext is the window that is now below this one, or NULL if this is 
//  now at the bottom.
typedef void (*RootlessRestackFrameProc)
    (ScreenPtr pScreen, RootlessFramePtr pFrame, 
     RootlessFramePtr pOldNext, RootlessFramePtr pNewNext);

// Flush drawing updates to the screen. 
// pDamage contains all changed pixels. 
// pDamage is in frame-local coordinates.
// pDamage is clipped to the frame bounds and the frame shape.
typedef void (*RootlessUpdateRegionProc)
    (ScreenPtr pScreen, RootlessFramePtr pFrame, RegionPtr pDamage);

// Change the frame's shape.
// pNewShape is in frame-local coordinates.
// Everything outside pNewShape is no longer part of the frame.
// pNewShape is {0, 0, width, height} for a plain-shaped frame.
// fixme can the pixmap change here?
// fixme reimplement shape
typedef void (*RootlessReshapeFrameProc)
    (ScreenPtr pScreen, RootlessFramePtr pFrame, RegionPtr pNewShape);

// Frame is about to resize. 
// The frame has its new position and size already.
// postconditions:
//  The pixmap MUST point to a pixmap with the new size. 
//  The pixmap data is undefined.
//  The old pixmap may be destroyed here.
typedef void (*RootlessStartResizeFrameProc)
    (ScreenPtr pScreen, RootlessFramePtr pFrame, 
     int oldX, int oldY, unsigned int oldW, unsigned int oldH);

// Frame is done resizing.
// Destroy the old pixmap if you haven't already.
typedef void (*RootlessFinishResizeFrameProc)
    (ScreenPtr pScreen, RootlessFramePtr pFrame, 
     int oldX, int oldY, unsigned int oldW, unsigned int oldH);


// The callback function list. 
// Any of these may be NULL.		
typedef struct RootlessFrameProcs {
    RootlessCreateFrameProc CreateFrame;
    RootlessDestroyFrameProc DestroyFrame;

    RootlessMoveFrameProc MoveFrame;
    RootlessStartResizeFrameProc StartResizeFrame;
    RootlessFinishResizeFrameProc FinishResizeFrame;
    RootlessRestackFrameProc RestackFrame;
    RootlessReshapeFrameProc ReshapeFrame;

    RootlessUpdateRegionProc UpdateRegion;

    // RootlessStartDrawingProc StartDrawing;
    // RootlessStopDrawingProc StopDrawing;
} RootlessFrameProcs;

// Initialize rootless mode on the given screen.
Bool RootlessInit(ScreenPtr pScreen, RootlessFrameProcs *procs);

#endif /* _ROOTLESS_H */

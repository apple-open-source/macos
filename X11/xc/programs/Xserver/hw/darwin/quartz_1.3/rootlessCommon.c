/*
 * Common rootless definitions and code
 *
 * Greg Parker     gparker@cs.stanford.edu
 */
/* $XFree86: xc/programs/Xserver/hw/darwin/quartz_1.3/rootlessCommon.c,v 1.2 2003/11/10 18:21:47 tsi Exp $ */

#include "rootlessCommon.h"


RegionRec rootlessHugeRoot = {{-32767, -32767, 32767, 32767}, NULL};


// Returns the top-level parent of pWindow.
// The root is the top-level parent of itself, even though the root is
// not otherwise considered to be a top-level window.
WindowPtr TopLevelParent(WindowPtr pWindow)
{
    WindowPtr top = pWindow;

    if (IsRoot(pWindow)) return pWindow; // root is top-level parent of itself
    while (top && ! IsTopLevel(top)) top = top->parent;
    return top;
}


// Returns TRUE if this window is visible inside a frame
// (e.g. it is visible and has a top-level or root parent)
Bool IsFramedWindow(WindowPtr pWin)
{
    WindowPtr top;

    if (! pWin->realized) return FALSE;
    top = TopLevelParent(pWin);
    return (top && WINREC(top));
}


// Move the given pixmap's base address to where pixel (0, 0)
// would be if the pixmap's actual data started at (x, y)
void SetPixmapBaseToScreen(PixmapPtr pix, int x, int y)
{
    pix->devPrivate.ptr = (char *)(pix->devPrivate.ptr) -
        (pix->drawable.bitsPerPixel/8 * x + y*pix->devKind);
}


// Update pWindow's pixmap.
// This needs to be called every time a window moves relative to
// its top-level parent, or the parent's pixmap data is reallocated.
// Three cases:
//  * window is top-level with no existing pixmap: make one
//  * window is top-level with existing pixmap: update it in place
//  * window is descendant of top-level: point to top-level's pixmap
void UpdatePixmap(WindowPtr pWindow)
{
    WindowPtr top = TopLevelParent(pWindow);
    RootlessWindowRec *winRec;
    ScreenPtr pScreen = pWindow->drawable.pScreen;
    PixmapPtr pix;

    RL_DEBUG_MSG("update pixmap (win 0x%x)", pWindow);

    // Don't use IsFramedWindow(); window is unrealized during RealizeWindow().

    if (! top) {
        RL_DEBUG_MSG("no parent\n");
        return;
    }
    winRec = WINREC(top);
    if (!winRec) {
        RL_DEBUG_MSG("not framed\n");
        return;
    }

    if (pWindow == top) {
        // This is the top window. Update its pixmap.
        if (winRec->pixmap == NULL) {
            // Allocate a new pixmap.
            pix = GetScratchPixmapHeader(pScreen,
                                         winRec->frame.w, winRec->frame.h,
                                         winRec->frame.depth,
                                         winRec->frame.bitsPerPixel,
                                         winRec->frame.bytesPerRow,
                                         winRec->frame.pixelData);
            SetPixmapBaseToScreen(pix, winRec->frame.x, winRec->frame.y);
            pScreen->SetWindowPixmap(pWindow, pix);
            winRec->pixmap = pix;
        } else {
            // Update existing pixmap. Update in place so we don't have to
            // change the children's pixmaps.
            pix = winRec->pixmap;
            pScreen->ModifyPixmapHeader(pix,
                                        winRec->frame.w, winRec->frame.h,
                                        winRec->frame.depth,
                                        winRec->frame.bitsPerPixel,
                                        winRec->frame.bytesPerRow,
                                        winRec->frame.pixelData);
            SetPixmapBaseToScreen(pix, winRec->frame.x, winRec->frame.y);
        }
    } else {
        // This is not the top window. Point to the parent's pixmap.
        pix = winRec->pixmap;
        pScreen->SetWindowPixmap(pWindow, pix);
    }

    RL_DEBUG_MSG("done\n");
}


#ifdef SHAPE

// boundingShape = outside border (like borderClip)
// clipShape = inside border (like clipList)
// Both are in window-local coordinates
// We only care about boundingShape (fixme true?)

// RootlessReallySetShape is used in several places other than SetShape.
// Most importantly, SetShape is often called on unmapped windows, so we
// have to wait until the window is mapped to reshape the frame.
static void RootlessReallySetShape(WindowPtr pWin)
{
    RootlessWindowRec *winRec = WINREC(pWin);
    ScreenPtr pScreen = pWin->drawable.pScreen;
    RegionRec newShape;

    if (IsRoot(pWin)) return;
    if (!IsTopLevel(pWin)) return;
    if (!winRec) return;

    if (wBoundingShape(pWin)) {
        // wBoundingShape is relative to *inner* origin of window.
        // Translate by borderWidth to get the outside-relative position.
        REGION_NULL(pScreen, &newShape);
        REGION_COPY(pScreen, &newShape, wBoundingShape(pWin));
        REGION_TRANSLATE(pScreen, &newShape, pWin->borderWidth,
                         pWin->borderWidth);
    } else {
        newShape.data = NULL;
        newShape.extents.x1 = 0;
        newShape.extents.y1 = 0;
        newShape.extents.x2 = winRec->frame.w;
        newShape.extents.y2 = winRec->frame.h;
    }
    RL_DEBUG_MSG("reshaping...");
    RL_DEBUG_MSG("numrects %d, extents %d %d %d %d ",
                 REGION_NUM_RECTS(&newShape),
                 newShape.extents.x1, newShape.extents.y1,
                 newShape.extents.x2, newShape.extents.y2);
    CallFrameProc(pScreen, ReshapeFrame,(pScreen, &winRec->frame, &newShape));
    REGION_UNINIT(pScreen, &newShape);
}

#endif // SHAPE


// pRegion is GLOBAL
void
RootlessDamageRegion(WindowPtr pWindow, RegionPtr pRegion)
{
    pWindow = TopLevelParent(pWindow);
    if (!pWindow) {
        RL_DEBUG_MSG("RootlessDamageRegion: window is not framed\n");
    } else if (!WINREC(pWindow)) {
        RL_DEBUG_MSG("RootlessDamageRegion: top-level window not a frame\n");
    } else {
        REGION_UNION((pWindow)->drawable.pScreen, &WINREC(pWindow)->damage,
                     &WINREC(pWindow)->damage, (pRegion));
    }
}


// pBox is GLOBAL
void
RootlessDamageBox(WindowPtr pWindow, BoxPtr pBox)
{
    RegionRec region;

    REGION_INIT(pWindow->drawable.pScreen, &region, pBox, 1);
    RootlessDamageRegion(pWindow, &region);
}


// (x, y, w, h) is in window-local coordinates.
void
RootlessDamageRect(WindowPtr pWindow, int x, int y, int w, int h)
{
    BoxRec box;
    RegionRec region;

    x += pWindow->drawable.x;
    y += pWindow->drawable.y;
    box.x1 = x;
    box.x2 = x + w;
    box.y1 = y;
    box.y2 = y + h;
    REGION_INIT(pWindow->drawable.pScreen, &region, &box, 1);
    RootlessDamageRegion(pWindow, &region);
}

#ifdef SHAPE

void
RootlessDamageShape(WindowPtr pWin)
{
    RootlessWindowRec *winRec = WINREC(pWin);

    // We only care about the shape of top-level framed windows.
    if (IsRoot(pWin)) return;
    if (!IsTopLevel(pWin)) return;
    if (!winRec) return;

    winRec->shapeDamage = TRUE;
}

#endif // SHAPE

void
RootlessRedisplay(WindowPtr pWindow)
{
    RootlessWindowRec *winRec = WINREC(pWindow);
    ScreenPtr pScreen = pWindow->drawable.pScreen;

#ifdef SHAPE
    if (winRec->shapeDamage) {
        // Reshape the window. This will also update the entire window.
        RootlessReallySetShape(pWindow);
       REGION_EMPTY(pScreen, &winRec->damage);
       winRec->shapeDamage = FALSE;
    }
    else
#endif // SHAPE
    if (REGION_NOTEMPTY(pScreen, &winRec->damage)) {
        REGION_INTERSECT(pScreen, &winRec->damage, &winRec->damage,
                          &pWindow->borderSize);

        // move region to window local coords
        REGION_TRANSLATE(pScreen, &winRec->damage,
                         -winRec->frame.x, -winRec->frame.y);
        CallFrameProc(pScreen, UpdateRegion,
                      (pScreen, &winRec->frame, &winRec->damage));
        REGION_EMPTY(pScreen, &winRec->damage);
    }
}


void
RootlessRedisplayScreen(ScreenPtr pScreen)
{
    WindowPtr root = WindowTable[pScreen->myNum];

    if (root) {
        WindowPtr win;

        RootlessRedisplay(root);
        for (win = root->firstChild; win; win = win->nextSib) {
            if (WINREC(win)) {
                RootlessRedisplay(win);
            }
        }
    }
}

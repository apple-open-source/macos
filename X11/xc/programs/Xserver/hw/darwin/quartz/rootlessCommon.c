/*
 * Common rootless definitions and code
 */
/*
 * Copyright (c) 2001 Greg Parker. All Rights Reserved.
 * Copyright (c) 2002-2003 Torrey T. Lyons. All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE ABOVE LISTED COPYRIGHT HOLDER(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Except as contained in this notice, the name(s) of the above copyright
 * holders shall not be used in advertising or otherwise to promote the sale,
 * use or other dealings in this Software without prior written authorization.
 */
/* $XFree86: xc/programs/Xserver/hw/darwin/quartz/rootlessCommon.c,v 1.7 2003/01/29 01:11:05 torrey Exp $ */

#include "rootlessCommon.h"


RegionRec rootlessHugeRoot = {{-32767, -32767, 32767, 32767}, NULL};


/*
 * TopLevelParent
 *  Returns the top-level parent of pWindow.
 *  The root is the top-level parent of itself, even though the root is
 *  not otherwise considered to be a top-level window.
 */
WindowPtr TopLevelParent(WindowPtr pWindow)
{
    WindowPtr top = pWindow;

    if (IsRoot(pWindow)) return pWindow; // root is top-level parent of itself
    while (top && ! IsTopLevel(top)) top = top->parent;
    return top;
}


/*
 * IsFramedWindow
 *  Returns TRUE if this window is visible inside a frame
 *  (e.g. it is visible and has a top-level or root parent)
 */
Bool IsFramedWindow(WindowPtr pWin)
{
    WindowPtr top;

    if (! pWin->realized) return FALSE;
    top = TopLevelParent(pWin);
    return (top && WINREC(top));
}


/*
 * RootlessStartDrawing
 *  Prepare a window for direct access to its backing buffer.
 *  Each top-level parent has a Pixmap representing its backing buffer,
 *  which all of its children inherit.
 */
void RootlessStartDrawing(WindowPtr pWindow)
{
    ScreenPtr pScreen = pWindow->drawable.pScreen;
    WindowPtr top = TopLevelParent(pWindow);
    RootlessWindowRec *winRec;

    if (!top) return;
    winRec = WINREC(top);
    if (!winRec) return;

    // Make sure the window's top-level parent is prepared for drawing.
    if (!winRec->drawing) {
        int bw = wBorderWidth(top);

        CallFrameProc(pScreen, StartDrawing, (pScreen, &winRec->frame));
        winRec->pixmap =
            GetScratchPixmapHeader(pScreen, winRec->frame.w, winRec->frame.h,
                                   winRec->frame.depth,
                                   winRec->frame.bitsPerPixel,
                                   winRec->frame.bytesPerRow,
                                   winRec->frame.pixelData);
        SetPixmapBaseToScreen(winRec->pixmap,
                              top->drawable.x - bw, top->drawable.y - bw);
        winRec->drawing = TRUE;
    }

    winRec->oldPixmap = pScreen->GetWindowPixmap(pWindow);
    pScreen->SetWindowPixmap(pWindow, winRec->pixmap);
}


/*
 * RootlessStopDrawing
 *  Stop drawing to a window's backing buffer.
 */
void RootlessStopDrawing(WindowPtr pWindow)
{
    WindowPtr top = TopLevelParent(pWindow);
    RootlessWindowRec *winRec;

    if (!top) return;
    winRec = WINREC(top);
    if (!winRec) return;

    if (winRec->drawing) {
        ScreenPtr pScreen = pWindow->drawable.pScreen;
        CallFrameProc(pScreen, StopDrawing, (pScreen, &winRec->frame));
        FreeScratchPixmapHeader(winRec->pixmap);
        pScreen->SetWindowPixmap(pWindow, winRec->oldPixmap);
        winRec->pixmap = NULL;
        winRec->drawing = FALSE;
    }
}

#if 0
// NSCarbonWindow Note: Windows no longer need a backing pixmap
// other than the one provided by the implementation.
// This routine is obsolete.

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
        int bw = wBorderWidth(pWindow);

        if (winRec->pixmap == NULL) {
            // Allocate a new pixmap.
            pix = GetScratchPixmapHeader(pScreen,
                                         winRec->frame.w, winRec->frame.h,
                                         winRec->frame.depth,
                                         winRec->frame.bitsPerPixel,
                                         winRec->frame.bytesPerRow,
                                         winRec->frame.pixelData);
            SetPixmapBaseToScreen(pix, pWindow->drawable.x - bw,
                                  pWindow->drawable.y - bw);
            pScreen->SetWindowPixmap(pWindow, pix);
            winRec->pixmap = pix;
        } else {
            // Update existing pixmap. Update in place so we don't have to
            // change the children's pixmaps.
            int bw = wBorderWidth(top);

            pix = winRec->pixmap;
            pScreen->ModifyPixmapHeader(pix,
                                        winRec->frame.w, winRec->frame.h,
                                        winRec->frame.depth,
                                        winRec->frame.bitsPerPixel,
                                        winRec->frame.bytesPerRow,
                                        winRec->frame.pixelData);
            SetPixmapBaseToScreen(pix, top->drawable.x - bw, 
                                  top->drawable.y - bw);
        }
    } else {
        // This is not the top window. Point to the parent's pixmap.
        pix = winRec->pixmap;
        pScreen->SetWindowPixmap(pWindow, pix);
    }

    RL_DEBUG_MSG("done\n");
}
#endif

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

    RootlessStopDrawing(pWin);

    if (wBoundingShape(pWin)) {
        // wBoundingShape is relative to *inner* origin of window.
        // Translate by borderWidth to get the outside-relative position.
        REGION_INIT(pScreen, &newShape, NullBox, 0);
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
    CallFrameProc(pScreen, ReshapeFrame, (pScreen, &winRec->frame, &newShape));
    REGION_UNINIT(pScreen, &newShape);
}

#endif // SHAPE


// pRegion is GLOBAL
void
RootlessDamageRegion(WindowPtr pWindow, RegionPtr pRegion)
{
    RL_DEBUG_MSG("Damaged win 0x%x ", pWindow);
    pWindow = TopLevelParent(pWindow);
    RL_DEBUG_MSG("parent 0x%x:\n", pWindow);
    if (!pWindow) {
        RL_DEBUG_MSG("RootlessDamageRegion: window is not framed\n");
    } else if (!WINREC(pWindow)) {
        RL_DEBUG_MSG("RootlessDamageRegion: top-level window not a frame\n");
    } else {
        REGION_UNION((pWindow)->drawable.pScreen, &WINREC(pWindow)->damage,
                     &WINREC(pWindow)->damage, (pRegion));
    }

#ifdef ROOTLESSDEBUG
    {
        BoxRec *box = REGION_RECTS(pRegion), *end;
        int numBox = REGION_NUM_RECTS(pRegion);

        for (end = box+numBox; box < end; box++) {
            RL_DEBUG_MSG("Damage rect: %i, %i, %i, %i\n",
                         box->x1, box->x2, box->y1, box->y2);
        }
    }
#endif
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

/*
 * RootlessRedisplay
 *  Stop drawing and redisplay the damaged region of a window.
 */
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
    } else
#endif // SHAPE
    {
        RootlessStopDrawing(pWindow);
        if (REGION_NOTEMPTY(pScreen, &winRec->damage)) {
            RL_DEBUG_MSG("Redisplay Win 0x%x, %i x %i @ (%i, %i)\n",
                         pWindow, winRec->frame.w, winRec->frame.h,
                         winRec->frame.x, winRec->frame.y);

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
}


/*
 * RootlessRedisplayScreen
 *  Walk every window on a screen and redisplay the damaged regions.
 */
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

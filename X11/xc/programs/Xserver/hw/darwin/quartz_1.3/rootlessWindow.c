/*
 * Rootless window management
 *
 * Greg Parker     gparker@cs.stanford.edu
 */
/* $XFree86: xc/programs/Xserver/hw/darwin/quartz_1.3/rootlessWindow.c,v 1.1 2002/03/28 02:21:20 torrey Exp $ */

#include "rootlessCommon.h"
#include "rootlessWindow.h"

#include "fb.h"


// RootlessCreateWindow
// For now, don't create a frame until the window is realized.
// Do reset the window size so it's not clipped by the root window.
Bool
RootlessCreateWindow(WindowPtr pWin)
{
    Bool result;
    RegionRec saveRoot;

    WINREC(pWin) = NULL;
    SCREEN_UNWRAP(pWin->drawable.pScreen, CreateWindow);
    if (!IsRoot(pWin)) {
        // win/border size set by DIX, not by wrapped CreateWindow, so
        // correct it here.
        // Don't HUGE_ROOT when pWin is the root!
        HUGE_ROOT(pWin);
        SetWinSize(pWin);
        SetBorderSize(pWin);
    }
    result = pWin->drawable.pScreen->CreateWindow(pWin);
    if (pWin->parent) {
        NORMAL_ROOT(pWin);
    }
    SCREEN_WRAP(pWin->drawable.pScreen, CreateWindow);
    return result;
}


// RootlessDestroyWindow
// For now, all window destruction takes place in UnrealizeWindow
Bool
RootlessDestroyWindow(WindowPtr pWin)
{
    Bool result;

    SCREEN_UNWRAP(pWin->drawable.pScreen, DestroyWindow);
    result = pWin->drawable.pScreen->DestroyWindow(pWin);
    SCREEN_WRAP(pWin->drawable.pScreen, DestroyWindow);
    return result;
}


#ifdef SHAPE

// RootlessSetShape
// Shape is usually set before the window is mapped, but (for now) we
// don't keep track of frames before they're mapped. So we just record
// that the shape needs to updated later.
void
RootlessSetShape(WindowPtr pWin)
{
    ScreenPtr pScreen = pWin->drawable.pScreen;

    RootlessDamageShape(pWin);
    SCREEN_UNWRAP(pScreen, SetShape);
    pScreen->SetShape(pWin);
    SCREEN_WRAP(pScreen, SetShape);
}

#endif // SHAPE


// Disallow ParentRelative background on top-level windows
// because the root window doesn't really have the right background
// and cfb will try to draw on the root instead of on the window.
// fixme what about fb?
// fixme implement ParentRelative with real transparency?
// ParentRelative prevention is also in RealizeWindow()
Bool
RootlessChangeWindowAttributes(WindowPtr pWin, unsigned long vmask)
{
    Bool result;
    ScreenPtr pScreen = pWin->drawable.pScreen;

    RL_DEBUG_MSG("change window attributes start ");

    SCREEN_UNWRAP(pScreen, ChangeWindowAttributes);
    result = pScreen->ChangeWindowAttributes(pWin, vmask);
    SCREEN_WRAP(pScreen, ChangeWindowAttributes);

    if (WINREC(pWin)) {
        // disallow ParentRelative background state
        if (pWin->backgroundState == ParentRelative) {
            XID pixel = 0;
            ChangeWindowAttributes(pWin, CWBackPixel, &pixel, serverClient);
        }
    }

    RL_DEBUG_MSG("change window attributes end\n");
    return result;
}


// Update the frame position now.
// (x, y) are *inside* position!
// After this, mi and fb are expecting the pixmap to be at the new location.
Bool
RootlessPositionWindow(WindowPtr pWin, int x, int y)
{
    RootlessWindowRec *winRec = WINREC(pWin);
    ScreenPtr pScreen = pWin->drawable.pScreen;
    Bool result;

    RL_DEBUG_MSG("positionwindow start\n");
    if (winRec) {
        winRec->frame.x = x - pWin->borderWidth;
        winRec->frame.y = y - pWin->borderWidth;
    }

    UpdatePixmap(pWin);

    SCREEN_UNWRAP(pScreen, PositionWindow);
    result = pScreen->PositionWindow(pWin, x, y);
    SCREEN_WRAP(pScreen, PositionWindow);

    RL_DEBUG_MSG("positionwindow end\n");
    return result;
}


// RootlessRealizeWindow
// The frame is created here and not in CreateWindow.
// fixme change this? probably not - would be faster, but eat more memory
Bool
RootlessRealizeWindow(WindowPtr pWin)
{
    Bool result = FALSE;
    RegionRec saveRoot;
    ScreenPtr pScreen = pWin->drawable.pScreen;

    RL_DEBUG_MSG("realizewindow start ");

    if (IsTopLevel(pWin)  ||  IsRoot(pWin)) {
        DrawablePtr d = &pWin->drawable;
        RootlessWindowRec *winRec = xalloc(sizeof(RootlessWindowRec));

        if (! winRec) goto windowcreatebad;

        winRec->frame.isRoot = (pWin == WindowTable[pScreen->myNum]);
        winRec->frame.x = d->x - pWin->borderWidth;
        winRec->frame.y = d->y - pWin->borderWidth;
        winRec->frame.w = d->width + 2*pWin->borderWidth;
        winRec->frame.h = d->height + 2*pWin->borderWidth;
        winRec->frame.win = pWin;
        winRec->frame.devPrivate = NULL;

        REGION_INIT(pScreen, &winRec->damage, NullBox, 0);
        winRec->borderWidth = pWin->borderWidth;

        winRec->pixmap = NULL;
        // UpdatePixmap() called below

        WINREC(pWin) = winRec;

        RL_DEBUG_MSG("creating frame ");
        CallFrameProc(pScreen, CreateFrame,
                      (pScreen, &WINREC(pWin)->frame,
                      pWin->prevSib ? &WINREC(pWin->prevSib)->frame : NULL));

        // fixme implement ParentRelative with transparency?
        //  need non-interfering fb first
        // Disallow ParentRelative background state
        // This might have been set before the window was mapped
        if (pWin->backgroundState == ParentRelative) {
            XID pixel = 0;
            ChangeWindowAttributes(pWin, CWBackPixel, &pixel, serverClient);
        }

#ifdef SHAPE
        // Shape is usually set before the window is mapped, but
        // (for now) we don't keep track of frames before they're mapped.
        winRec->shapeDamage = TRUE;
#endif
    }

    UpdatePixmap(pWin);

    if (!IsRoot(pWin)) HUGE_ROOT(pWin);
    SCREEN_UNWRAP(pScreen, RealizeWindow);
    result = pScreen->RealizeWindow(pWin);
    SCREEN_WRAP(pScreen, RealizeWindow);
    if (!IsRoot(pWin)) NORMAL_ROOT(pWin);

    RL_DEBUG_MSG("realizewindow end\n");
    return result;

windowcreatebad:
    RL_DEBUG_MSG("window create bad! ");
    RL_DEBUG_MSG("realizewindow end\n");
    return NULL;
}


Bool
RootlessUnrealizeWindow(WindowPtr pWin)
{
    Bool result;
    ScreenPtr pScreen = pWin->drawable.pScreen;

    RL_DEBUG_MSG("unrealizewindow start ");

    if (IsTopLevel(pWin) || IsRoot(pWin)) {
        RootlessWindowRec *winRec = WINREC(pWin);

        RootlessRedisplay(pWin);
        CallFrameProc(pScreen, DestroyFrame, (pScreen, &winRec->frame));

        REGION_UNINIT(pScreen, &winRec->damage);

        xfree(winRec);
        WINREC(pWin) = NULL;
    }

    SCREEN_UNWRAP(pScreen, UnrealizeWindow);
    result = pScreen->UnrealizeWindow(pWin);
    SCREEN_WRAP(pScreen, UnrealizeWindow);
    RL_DEBUG_MSG("unrealizewindow end\n");
    return result;
}


void
RootlessRestackWindow(WindowPtr pWin, WindowPtr pOldNextSib)
{
    RegionRec saveRoot;
    RootlessWindowRec *winRec = WINREC(pWin);
    ScreenPtr pScreen = pWin->drawable.pScreen;

    RL_DEBUG_MSG("restackwindow start ");
    if (winRec) RL_DEBUG_MSG("restack top level \n");

    HUGE_ROOT(pWin);
    SCREEN_UNWRAP(pScreen, RestackWindow);
    if (pScreen->RestackWindow) pScreen->RestackWindow(pWin, pOldNextSib);
    SCREEN_WRAP(pScreen, RestackWindow);
    NORMAL_ROOT(pWin);

    if (winRec) {
        // fixme simplify the following

        WindowPtr oldNextW, newNextW, oldPrevW, newPrevW;
        RootlessFramePtr oldNext, newNext, oldPrev, newPrev;

        oldNextW = pOldNextSib;
        while (oldNextW  &&  ! WINREC(oldNextW)) oldNextW = oldNextW->nextSib;
        oldNext = oldNextW ? &WINREC(oldNextW)->frame : NULL;

        newNextW = pWin->nextSib;
        while (newNextW  &&  ! WINREC(newNextW)) newNextW = newNextW->nextSib;
        newNext = newNextW ? &WINREC(newNextW)->frame : NULL;

        oldPrevW= pOldNextSib ? pOldNextSib->prevSib : pWin->parent->lastChild;
        while (oldPrevW  &&  ! WINREC(oldPrevW)) oldPrevW = oldPrevW->prevSib;
        oldPrev = oldPrevW ? &WINREC(oldPrevW)->frame : NULL;

        newPrevW = pWin->prevSib;
        while (newPrevW  &&  ! WINREC(newPrevW)) newPrevW = newPrevW->prevSib;
        newPrev = newPrevW ? &WINREC(newPrevW)->frame : NULL;

        if (pWin->prevSib) {
            WindowPtr w = pWin->prevSib;
            while (w) {
                RL_DEBUG_MSG("w 0x%x\n", w);
                w = w->parent;
            }
        }

        CallFrameProc(pScreen, RestackFrame,
                      (pScreen, &winRec->frame, oldPrev, newPrev));
    }

    RL_DEBUG_MSG("restackwindow end\n");
}


/*
 * Specialized window copy procedures
 */

// Globals needed during window resize and move.
static PixmapPtr gResizeDeathPix = NULL;
static pointer gResizeDeathBits = NULL;
static PixmapPtr gResizeCopyWindowSource = NULL;
static CopyWindowProcPtr gResizeOldCopyWindowProc = NULL;

// CopyWindow() that doesn't do anything.
// For MoveWindow() of top-level windows.
static void
RootlessNoCopyWindow(WindowPtr pWin, DDXPointRec ptOldOrg,
                     RegionPtr prgnSrc)
{
    // some code expects the region to be translated
    int dx = ptOldOrg.x - pWin->drawable.x;
    int dy = ptOldOrg.y - pWin->drawable.y;
    RL_DEBUG_MSG("ROOTLESSNOCOPYWINDOW ");

    REGION_TRANSLATE(pWin->drawable.pScreen, prgnSrc, -dx, -dy);
}


// CopyWindow used during ResizeWindow for gravity moves.
// Cloned from fbCopyWindow
// The original always draws on the root pixmap (which we don't have).
// Instead, draw on the parent window's pixmap.
// Resize version: the old location's pixels are in gResizeCopyWindowSource
static void
RootlessResizeCopyWindow(WindowPtr pWin, DDXPointRec ptOldOrg,
                         RegionPtr prgnSrc)
{
    ScreenPtr pScreen = pWin->drawable.pScreen;
    SCREEN_UNWRAP(pScreen, CopyWindow);
    RL_DEBUG_MSG("resizecopywindowFB start (win 0x%x) ", pWin);

    {
        RegionRec   rgnDst;
        int         dx, dy;

        dx = ptOldOrg.x - pWin->drawable.x;
        dy = ptOldOrg.y - pWin->drawable.y;
        REGION_TRANSLATE(pScreen, prgnSrc, -dx, -dy);
        REGION_INIT (pScreen, &rgnDst, NullBox, 0);
        REGION_INTERSECT(pScreen, &rgnDst, &pWin->borderClip, prgnSrc);

        fbCopyRegion (&gResizeCopyWindowSource->drawable,
                      &pScreen->GetWindowPixmap(pWin)->drawable,
                      0,
                      &rgnDst, dx, dy, fbCopyWindowProc, 0, 0);

        // don't update - resize will update everything
        // fixme DO update?
        REGION_UNINIT(pScreen, &rgnDst);
        fbValidateDrawable (&pWin->drawable);
    }

    SCREEN_WRAP(pScreen, CopyWindow);
    RL_DEBUG_MSG("resizecopywindowFB end\n");
}


/* Update *new* location of window. Old location is redrawn with
 *  PaintWindowBackground/Border.
 * Cloned from fbCopyWindow
 * The original always draws on the root pixmap (which we don't have).
 * Instead, draw on the parent window's pixmap.
 */
void
RootlessCopyWindow(WindowPtr pWin, DDXPointRec ptOldOrg, RegionPtr prgnSrc)
{
    ScreenPtr pScreen = pWin->drawable.pScreen;
    SCREEN_UNWRAP(pScreen, CopyWindow);
    RL_DEBUG_MSG("copywindowFB start (win 0x%x) ", pWin);

    {
        RegionRec   rgnDst;
        int         dx, dy;

        dx = ptOldOrg.x - pWin->drawable.x;
        dy = ptOldOrg.y - pWin->drawable.y;
        REGION_TRANSLATE(pScreen, prgnSrc, -dx, -dy);

        REGION_INIT(pScreen, &rgnDst, NullBox, 0);
        REGION_INTERSECT(pScreen, &rgnDst, &pWin->borderClip, prgnSrc);

        fbCopyRegion ((DrawablePtr)pWin, (DrawablePtr)pWin,
                      0,
                      &rgnDst, dx, dy, fbCopyWindowProc, 0, 0);

        // prgnSrc has been translated to dst position
        RootlessDamageRegion(pWin, prgnSrc);
        REGION_UNINIT(pScreen, &rgnDst);
        fbValidateDrawable (&pWin->drawable);
    }

    SCREEN_WRAP(pScreen, CopyWindow);
    RL_DEBUG_MSG("copywindowFB end\n");
}


/*
 * Window resize procedures
 */

// Prepare to resize a window.
// The old window's pixels are saved and the implementation is told
// to change the window size.
// (x,y,w,h) is outer frame of window (outside border)
static void
StartFrameResize(WindowPtr pWin, Bool gravity,
                 int oldX, int oldY,
                 unsigned int oldW, unsigned int oldH, unsigned int oldBW,
                 int newX, int newY,
                 unsigned int newW, unsigned int newH, unsigned int newBW)
{
    ScreenPtr pScreen = pWin->drawable.pScreen;
    RootlessWindowRec *winRec = WINREC(pWin);

    RL_DEBUG_MSG("RESIZE TOPLEVEL WINDOW ");
    RL_DEBUG_MSG("%d %d %d %d %d   %d %d %d %d %d  ",
                 oldX, oldY, oldW, oldH, oldBW,
                 newX, newY, newW, newH, newBW);

    RootlessRedisplay(pWin);

    // Make a copy of the current pixmap and all its data.
    // The original will go away when we ask the frame manager to
    // allocate the new pixmap.

    gResizeDeathBits = xalloc(winRec->frame.bytesPerRow * winRec->frame.h);
    memcpy(gResizeDeathBits, winRec->frame.pixelData,
           winRec->frame.bytesPerRow * winRec->frame.h);
    gResizeDeathPix =
        GetScratchPixmapHeader(pScreen, winRec->frame.w, winRec->frame.h,
                               winRec->frame.depth, winRec->frame.bitsPerPixel,
                               winRec->frame.bytesPerRow, gResizeDeathBits);
    SetPixmapBaseToScreen(gResizeDeathPix, winRec->frame.x, winRec->frame.y);

    winRec->frame.x = newX;
    winRec->frame.y = newY;
    winRec->frame.w = newW;
    winRec->frame.h = newH;
    winRec->borderWidth = newBW;

    CallFrameProc(pScreen, StartResizeFrame,
                  (pScreen, &winRec->frame, oldX, oldY, oldW, oldH));
    UpdatePixmap(pWin);

    // Use custom CopyWindow when moving gravity bits around
    // ResizeWindow assumes the old window contents are in the same
    // pixmap, but here they're in deathPix instead.
    if (gravity) {
        gResizeCopyWindowSource = gResizeDeathPix;
        gResizeOldCopyWindowProc = pScreen->CopyWindow;
        pScreen->CopyWindow = RootlessResizeCopyWindow;
    }

    // Copy pixels in intersection from src to dst.
    // ResizeWindow assumes these pixels are already present when
    // making gravity adjustments.
    // pWin currently has new-sized pixmap but is in old position
    // fixme border width change!
    {
        RegionRec r;
        DrawablePtr src = &gResizeDeathPix->drawable;
        DrawablePtr dst = &pScreen->GetWindowPixmap(pWin)->drawable;
       // These vars are needed because implicit unsigned->signed fails
       int oldX2 = (int)(oldX + oldW), newX2 = (int)(newX + newW);
       int oldY2 = (int)(oldY + oldH), newY2 = (int)(newY + newH);

        r.data = NULL;
        r.extents.x1 = max(oldX, newX);
        r.extents.y1 = max(oldY, newY);
        r.extents.x2 = min(oldX2, newX2);
        r.extents.y2 = min(oldY2, newY2);

        // r is now intersection of of old location and new location
        if (r.extents.x2 > r.extents.x1  &&  r.extents.y2 > r.extents.y1) {
#if 0
            DDXPointRec srcPt = {r.extents.x1, r.extents.y1};
            // Correct for border width change
            // fixme need to correct for border width change
            int dx = newX - oldX;
            int dy = newY - oldY;
            REGION_TRANSLATE(pScreen, &r, dx, dy);
#endif
            fbCopyRegion(src, dst, NULL, &r, 0, 0, fbCopyWindowProc, 0, 0);
        }
        REGION_UNINIT(pScreen, &r);
    }
}


static void
FinishFrameResize(WindowPtr pWin, Bool gravity,
                  int oldX, int oldY,
                  unsigned int oldW, unsigned int oldH, unsigned int oldBW,
                  int newX, int newY,
                  unsigned int newW, unsigned int newH, unsigned int newBW)
{
    ScreenPtr pScreen = pWin->drawable.pScreen;
    RootlessWindowRec *winRec = WINREC(pWin);

    CallFrameProc(pScreen, FinishResizeFrame,
                  (pScreen, &winRec->frame, oldX, oldY, oldW, oldH));
    if (wBoundingShape(pWin)) {
        RootlessDamageShape(pWin);
    }

    // Destroy temp pixmap
    FreeScratchPixmapHeader(gResizeDeathPix);
    xfree(gResizeDeathBits);
    gResizeDeathPix = gResizeDeathBits = NULL;

    if (gravity) {
        pScreen->CopyWindow = gResizeOldCopyWindowProc;
        gResizeCopyWindowSource = NULL;
    }
}


// If kind==VTOther, window border is resizing (and borderWidth is
//   already changed!!@#$)  This case works like window resize, not move.
void
RootlessMoveWindow(WindowPtr pWin, int x, int y, WindowPtr pSib, VTKind kind)
{
    CopyWindowProcPtr oldCopyWindowProc = NULL;
    RegionRec saveRoot;
    RootlessWindowRec *winRec = WINREC(pWin);
    ScreenPtr pScreen = pWin->drawable.pScreen;
    int oldX = 0, oldY = 0, newX = 0, newY = 0;
    unsigned int oldW = 0, oldH = 0, oldBW = 0, newW = 0, newH = 0, newBW = 0;

    RL_DEBUG_MSG("movewindow start \n");

    if (winRec) {
        if (kind == VTMove) {
            oldX = winRec->frame.x;
            oldY = winRec->frame.y;
            RootlessRedisplay(pWin);
        } else {
            RL_DEBUG_MSG("movewindow border resizing ");
            oldBW = winRec->borderWidth;
            oldX = winRec->frame.x;
            oldY = winRec->frame.y;
            oldW = winRec->frame.w;
            oldH = winRec->frame.h;
            newBW = pWin->borderWidth;
            newX = x;
            newY = y;
            newW = pWin->drawable.width  + 2*newBW;
            newH = pWin->drawable.height + 2*newBW;
            StartFrameResize(pWin, FALSE, oldX, oldY, oldW, oldH, oldBW,
                             newX, newY, newW, newH, newBW);
        }
    }

    HUGE_ROOT(pWin);
    SCREEN_UNWRAP(pScreen, MoveWindow);
    if (winRec) {
        oldCopyWindowProc = pScreen->CopyWindow;
        pScreen->CopyWindow = RootlessNoCopyWindow;
    }
    pScreen->MoveWindow(pWin, x, y, pSib, kind);
    if (winRec) {
        pScreen->CopyWindow = oldCopyWindowProc;
    }
    NORMAL_ROOT(pWin);
    SCREEN_WRAP(pScreen, MoveWindow);

    if (winRec) {
        if (kind == VTMove) {
            // PositionWindow has already set the new frame position.
            CallFrameProc(pScreen, MoveFrame,
                          (pScreen, &winRec->frame, oldX, oldY));
        } else {
            FinishFrameResize(pWin, FALSE, oldX, oldY, oldW, oldH, oldBW,
                              newX, newY, newW, newH, newBW);
        }
    }

    RL_DEBUG_MSG("movewindow end\n");
}


// Note: (x, y, w, h) as passed to this procedure don't match
//  the frame definition.
// (x,y) is corner of very outer edge, *outside* border
// w,h is width and height *inside8 border, *ignoring* border width
// The rect (x, y, w, h) doesn't mean anything.
// (x, y, w+2*bw, h+2*bw) is total rect
// (x+bw, y+bw, w, h) is inner rect

void
RootlessResizeWindow(WindowPtr pWin, int x, int y,
                     unsigned int w, unsigned int h, WindowPtr pSib)
{
    RegionRec saveRoot;
    RootlessWindowRec *winRec = WINREC(pWin);
    ScreenPtr pScreen = pWin->drawable.pScreen;
    int oldX = 0, oldY = 0, newX = 0, newY = 0;
    unsigned int oldW = 0, oldH = 0, oldBW = 0, newW = 0, newH = 0, newBW = 0;

    RL_DEBUG_MSG("resizewindow start (win 0x%x) ", pWin);

    if (winRec) {
        oldBW = winRec->borderWidth;
        oldX = winRec->frame.x;
        oldY = winRec->frame.y;
        oldW = winRec->frame.w;
        oldH = winRec->frame.h;

        newBW = oldBW;
        newX = x;
        newY = y;
        newW = w + 2*newBW;
        newH = h + 2*newBW;

        StartFrameResize(pWin, TRUE, oldX, oldY, oldW, oldH, oldBW,
                         newX, newY, newW, newH, newBW);
    }

    HUGE_ROOT(pWin);
    SCREEN_UNWRAP(pScreen, ResizeWindow);
    pScreen->ResizeWindow(pWin, x, y, w, h, pSib);
    SCREEN_WRAP(pScreen, ResizeWindow);
    NORMAL_ROOT(pWin);

    if (winRec) {
        FinishFrameResize(pWin, TRUE, oldX, oldY, oldW, oldH, oldBW,
                        newX, newY, newW, newH, newBW);
    }

    RL_DEBUG_MSG("resizewindow end\n");
}


// fixme untested!
// pWin inside corner stays the same
// pWin->drawable.[xy] stays the same
// frame moves and resizes
void
RootlessChangeBorderWidth(WindowPtr pWin, unsigned int width)
{
    RegionRec saveRoot;

    RL_DEBUG_MSG("change border width ");
    if (width != pWin->borderWidth) {
        RootlessWindowRec *winRec = WINREC(pWin);
        int oldX = 0, oldY = 0, newX = 0, newY = 0;
        unsigned int oldW = 0, oldH = 0, oldBW = 0;
        unsigned int newW = 0, newH = 0, newBW = 0;

        if (winRec) {
            oldBW = winRec->borderWidth;
            oldX = winRec->frame.x;
            oldY = winRec->frame.y;
            oldW = winRec->frame.w;
            oldH = winRec->frame.h;

            newBW = width;
            newX = pWin->drawable.x - newBW;
            newY = pWin->drawable.y - newBW;
            newW = pWin->drawable.width  + 2*newBW;
            newH = pWin->drawable.height + 2*newBW;

            StartFrameResize(pWin, FALSE, oldX, oldY, oldW, oldH, oldBW,
                            newX, newY, newW, newH, newBW);
        }

        HUGE_ROOT(pWin);
        SCREEN_UNWRAP(pWin->drawable.pScreen, ChangeBorderWidth);
        pWin->drawable.pScreen->ChangeBorderWidth(pWin, width);
        SCREEN_WRAP(pWin->drawable.pScreen, ChangeBorderWidth);
        NORMAL_ROOT(pWin);

        if (winRec) {
            FinishFrameResize(pWin, FALSE, oldX, oldY, oldW, oldH, oldBW,
                              newX, newY, newW, newH, newBW);
        }
    }
    RL_DEBUG_MSG("change border width end\n");
}

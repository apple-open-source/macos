/*
 * Generic rootless to Aqua specific glue code
 *
 * This code acts as a glue between the generic rootless X server code
 * and the Aqua specific implementation, which includes definitions that
 * conflict with stardard X types.
 *
 * Greg Parker     gparker@cs.stanford.edu
 */
/* $XFree86: xc/programs/Xserver/hw/darwin/quartz/rootlessAquaGlue.c,v 1.5 2002/12/10 00:00:39 torrey Exp $ */

#include "quartzCommon.h"
#include "darwin.h"
#include "rootlessAqua.h"
#include "rootlessAquaImp.h"
#include "rootless.h"
#include "aqua.h"

#include "regionstr.h"
#include "scrnintstr.h"
#include "picturestr.h"
#include "globals.h" // dixScreenOrigins[]


/////////////////////////////////////////
// Rootless mode callback glue

static void
AquaGlueCreateFrame(ScreenPtr pScreen, RootlessFramePtr pFrame,
                    RootlessFramePtr pUpper)
{
    int sx = dixScreenOrigins[pScreen->myNum].x + darwinMainScreenX;
    int sy = dixScreenOrigins[pScreen->myNum].y + darwinMainScreenY;

    pFrame->devPrivate = AquaNewWindow(pUpper ? pUpper->devPrivate : NULL,
                                       pFrame->x + sx, pFrame->y + sy,
                                       pFrame->w, pFrame->h,
                                       pFrame->isRoot);
}


static void
AquaGlueDestroyFrame(ScreenPtr pScreen, RootlessFramePtr pFrame)
{
    AquaDestroyWindow(pFrame->devPrivate);
}


static void
AquaGlueMoveFrame(ScreenPtr pScreen, RootlessFramePtr pFrame,
                  int oldX, int oldY)
{
    int sx = dixScreenOrigins[pScreen->myNum].x + darwinMainScreenX;
    int sy = dixScreenOrigins[pScreen->myNum].y + darwinMainScreenY;

    AquaMoveWindow(pFrame->devPrivate, pFrame->x + sx, pFrame->y + sy);
}


static void
AquaGlueStartResizeFrame(ScreenPtr pScreen, RootlessFramePtr pFrame,
                         int oldX, int oldY,
                         unsigned int oldW, unsigned int oldH)
{
    int sx = dixScreenOrigins[pScreen->myNum].x + darwinMainScreenX;
    int sy = dixScreenOrigins[pScreen->myNum].y + darwinMainScreenY;

    AquaStartResizeWindow(pFrame->devPrivate,
                          pFrame->x + sx, pFrame->y + sy, pFrame->w, pFrame->h);
}

static void
AquaGlueFinishResizeFrame(ScreenPtr pScreen, RootlessFramePtr pFrame,
                          int oldX, int oldY,
                          unsigned int oldW, unsigned int oldH)
{
    int sx = dixScreenOrigins[pScreen->myNum].x + darwinMainScreenX;
    int sy = dixScreenOrigins[pScreen->myNum].y + darwinMainScreenY;

    AquaFinishResizeWindow(pFrame->devPrivate,
                           pFrame->x + sx, pFrame->y + sy,
                           pFrame->w, pFrame->h);
}


static void
AquaGlueRestackFrame(ScreenPtr pScreen, RootlessFramePtr pFrame,
                     RootlessFramePtr pOldPrev,
                     RootlessFramePtr pNewPrev)
{
    AquaRestackWindow(pFrame->devPrivate,
                      pNewPrev ? pNewPrev->devPrivate : NULL);
}


static void
AquaGlueReshapeFrame(ScreenPtr pScreen, RootlessFramePtr pFrame,
                     RegionPtr pNewShape)
{
    BoxRec shapeBox = {0, 0, pFrame->w, pFrame->h};

    // Don't correct for dixScreenOrigins here.
    // pNewShape is in window-local coordinates.

    if (pFrame->isRoot) return; // shouldn't happen; mi or dix covers this

    REGION_INVERSE(pScreen, pNewShape, pNewShape, &shapeBox);
    AquaReshapeWindow(pFrame->devPrivate,
                      (fakeBoxRec *) REGION_RECTS(pNewShape),
                      REGION_NUM_RECTS(pNewShape));
}


static void
AquaGlueUpdateRegion(ScreenPtr pScreen, RootlessFramePtr pFrame,
                     RegionPtr pDamage)
{
    AquaUpdateRects(pFrame->devPrivate,
                    (fakeBoxRec *) REGION_RECTS(pDamage),
                    REGION_NUM_RECTS(pDamage));
}


static void
AquaGlueStartDrawing(ScreenPtr pScreen, RootlessFramePtr pFrame)
{
    AquaStartDrawing(pFrame->devPrivate, &pFrame->pixelData,
                     &pFrame->bytesPerRow, &pFrame->depth,
                     &pFrame->bitsPerPixel);
}

static void
AquaGlueStopDrawing(ScreenPtr pScreen, RootlessFramePtr pFrame)
{
    AquaStopDrawing(pFrame->devPrivate);
}


static RootlessFrameProcs aquaRootlessProcs = {
    AquaGlueCreateFrame,
    AquaGlueDestroyFrame,
    AquaGlueMoveFrame,
    AquaGlueStartResizeFrame,
    AquaGlueFinishResizeFrame,
    AquaGlueRestackFrame,
    AquaGlueReshapeFrame,
    AquaGlueUpdateRegion,
    AquaGlueStartDrawing,
    AquaGlueStopDrawing
};


///////////////////////////////////////
// Rootless mode initialization.
// Exported by rootlessAqua.h

/*
 * AquaDisplayInit
 *  Find all Aqua screens.
 */
void
AquaDisplayInit(void)
{
    darwinScreensFound = AquaDisplayCount();
}


/*
 * AquaAddScreen
 *  Init the framebuffer and record pixmap parameters for the screen.
 */
Bool
AquaAddScreen(int index, ScreenPtr pScreen)
{
    DarwinFramebufferPtr dfb = SCREEN_PRIV(pScreen);
    QuartzScreenPtr displayInfo = QUARTZ_PRIV(pScreen);
    CGRect cgRect;
    CGDisplayCount numDisplays;
    CGDisplayCount allocatedDisplays = 0;
    CGDirectDisplayID *displays = NULL;
    CGDisplayErr cgErr;
    int componentCount;

    dfb->colorType = TrueColor;
    AquaScreenInit(index, &dfb->x, &dfb->y, &dfb->width, &dfb->height,
                    &dfb->pitch, &dfb->bitsPerComponent,
                    &componentCount, &dfb->bitsPerPixel);
    dfb->colorBitsPerPixel = dfb->bitsPerComponent * componentCount;

    // No frame buffer - it's all in window pixmaps.
    dfb->framebuffer = NULL; // malloc(dfb.pitch * dfb.height);

    // Get all CoreGraphics displays covered by this X11 display.
    cgRect = CGRectMake(dfb->x, dfb->y, dfb->width, dfb->height);
    do {
        cgErr = CGGetDisplaysWithRect(cgRect, 0, NULL, &numDisplays);
        if (cgErr) break;
        allocatedDisplays = numDisplays;
        displays = xrealloc(displays,
                            numDisplays * sizeof(CGDirectDisplayID));
        cgErr = CGGetDisplaysWithRect(cgRect, allocatedDisplays, displays,
                                      &numDisplays);
        if (cgErr != CGDisplayNoErr) break;
    } while (numDisplays > allocatedDisplays);

    if (cgErr != CGDisplayNoErr || numDisplays == 0) {
        ErrorF("Could not find CGDirectDisplayID(s) for X11 screen %d: %dx%d @ %d,%d.\n",
               index, dfb->width, dfb->height, dfb->x, dfb->y);
        return FALSE;
    }

    // This X11 screen covers all CoreGraphics displays we just found.
    // If there's more than one CG display, then video mirroring is on
    // or PseudoramiX is on.
    displayInfo->displayCount = allocatedDisplays;
    displayInfo->displayIDs = displays;

    return TRUE;
}


/*
 * AquaSetupScreen
 *  Setup the screen for rootless access.
 */
Bool
AquaSetupScreen(int index, ScreenPtr pScreen)
{
    // Add Aqua specific replacements for fb screen functions
    pScreen->PaintWindowBackground = AquaPaintWindow;
    pScreen->PaintWindowBorder = AquaPaintWindow;

#ifdef RENDER
    {
        PictureScreenPtr ps = GetPictureScreen(pScreen);
        ps->Composite = AquaComposite;
    }
#endif /* RENDER */

    // Initialize generic rootless code
    return RootlessInit(pScreen, &aquaRootlessProcs);
}

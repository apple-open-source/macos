/*
 * Screen routines for Mac OS X rootless X server
 *
 * Greg Parker     gparker@cs.stanford.edu
 *
 * February 2001  Created
 * March 3, 2001  Restructured as generic rootless mode
 */
/* $XFree86: xc/programs/Xserver/hw/darwin/quartz_1.3/rootlessScreen.c,v 1.2 2002/10/16 21:13:33 dawes Exp $ */


#include "mi.h"
#include "scrnintstr.h"
#include "gcstruct.h"
#include "pixmapstr.h"
#include "windowstr.h"
#include "propertyst.h"
#include "mivalidate.h"
#include "picturestr.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "rootlessCommon.h"
#include "rootlessWindow.h"

extern int
rootlessMiValidateTree(WindowPtr pRoot, WindowPtr pChild, VTKind kind);
extern Bool
RootlessCreateGC(GCPtr pGC);

// Initialize globals
int rootlessGCPrivateIndex = -1;
int rootlessScreenPrivateIndex = -1;
int rootlessWindowPrivateIndex = -1;


static Bool
RootlessCloseScreen(int i, ScreenPtr pScreen)
{
    RootlessScreenRec *s;

    s = SCREENREC(pScreen);

    // fixme unwrap everything that was wrapped?
    pScreen->CloseScreen = s->CloseScreen;

    xfree(s);
    return pScreen->CloseScreen(i, pScreen);
}


static void
RootlessGetImage(DrawablePtr pDrawable, int sx, int sy, int w, int h,
                 unsigned int format, unsigned long planeMask, char *pdstLine)
{
    ScreenPtr pScreen = pDrawable->pScreen;
    SCREEN_UNWRAP(pScreen, GetImage);

    if (pDrawable->type == DRAWABLE_WINDOW) {
        /* Many apps use GetImage to sync with the visible frame buffer */
        // fixme entire screen or just window or all screens?
        RootlessRedisplayScreen(pScreen);
    }

    pScreen->GetImage(pDrawable, sx, sy, w, h, format, planeMask, pdstLine);

    SCREEN_WRAP(pScreen, GetImage);
}


#ifdef RENDER

static void
RootlessComposite(CARD8 op, PicturePtr pSrc, PicturePtr pMask, PicturePtr pDst,
                  INT16 xSrc, INT16 ySrc, INT16  xMask, INT16  yMask,
                  INT16 xDst, INT16 yDst, CARD16 width, CARD16 height)
{
    ScreenPtr pScreen = pDst->pDrawable->pScreen;
    PictureScreenPtr ps = GetPictureScreen(pScreen);
    WindowPtr dstWin;

    dstWin  = (pDst->pDrawable->type  == DRAWABLE_WINDOW) ?
        (WindowPtr)pDst->pDrawable  :  NULL;

    // SCREEN_UNWRAP(ps, Composite);
    ps->Composite = SCREENREC(pScreen)->Composite;

    ps->Composite(op, pSrc, pMask, pDst,
                  xSrc, ySrc, xMask, yMask,
                  xDst, yDst, width, height);

    if (dstWin  && IsFramedWindow(dstWin)) {
        RootlessDamageRect(dstWin, xDst, yDst, width, height);
    }

    ps->Composite = RootlessComposite;
    // SCREEN_WRAP(ps, Composite);
}


static void
RootlessGlyphs(CARD8 op, PicturePtr pSrc, PicturePtr pDst,
               PictFormatPtr maskFormat, INT16 xSrc, INT16 ySrc,
               int nlist, GlyphListPtr list, GlyphPtr *glyphs)
{
    ScreenPtr pScreen = pDst->pDrawable->pScreen;
    PictureScreenPtr ps = GetPictureScreen(pScreen);
    int x, y;
    int n;
    GlyphPtr glyph;
    WindowPtr dstWin;

    dstWin = (pDst->pDrawable->type == DRAWABLE_WINDOW) ?
        (WindowPtr)pDst->pDrawable  :  NULL;

    //SCREEN_UNWRAP(ps, Glyphs);
    ps->Glyphs = SCREENREC(pScreen)->Glyphs;
    ps->Glyphs(op, pSrc, pDst, maskFormat, xSrc, ySrc, nlist, list, glyphs);
    ps->Glyphs = RootlessGlyphs;
    //SCREEN_WRAP(ps, Glyphs);

    if (dstWin && IsFramedWindow(dstWin)) {
        x = xSrc;
        y = ySrc;
        while (nlist--) {
            x += list->xOff;
            y += list->yOff;
            n = list->len;
            while (n--) {
                glyph = *glyphs++;
                RootlessDamageRect(dstWin,
                                   x - glyph->info.x, y - glyph->info.y,
                                   glyph->info.width, glyph->info.height);
                x += glyph->info.xOff;
                y += glyph->info.yOff;
            }
            list++;
        }
    }
}

#endif // RENDER


// RootlessValidateTree
// ValidateTree is modified in two ways:
// * top-level windows don't clip each other
// * windows aren't clipped against root.
// These only matter when validating from the root.
static int
RootlessValidateTree(WindowPtr pParent, WindowPtr pChild, VTKind kind)
{
    int result;
    RegionRec saveRoot;
    ScreenPtr pScreen = pParent->drawable.pScreen;

    SCREEN_UNWRAP(pScreen, ValidateTree);
    RL_DEBUG_MSG("VALIDATETREE start ");

    // Use our custom version to validate from root
    if (IsRoot(pParent)) {
        RL_DEBUG_MSG("custom ");
        result = rootlessMiValidateTree(pParent, pChild, kind);
    } else {
        HUGE_ROOT(pParent);
        result = pScreen->ValidateTree(pParent, pChild, kind);
        NORMAL_ROOT(pParent);
    }

    SCREEN_WRAP(pScreen, ValidateTree);
    RL_DEBUG_MSG("VALIDATETREE end\n");

    return result;
}


// RootlessMarkOverlappedWindows
// MarkOverlappedWindows is modified to ignore overlapping
// top-level windows.
static Bool
RootlessMarkOverlappedWindows(WindowPtr pWin, WindowPtr pFirst,
                              WindowPtr *ppLayerWin)
{
    RegionRec saveRoot;
    Bool result;
    ScreenPtr pScreen = pWin->drawable.pScreen;
    SCREEN_UNWRAP(pScreen, MarkOverlappedWindows);
    RL_DEBUG_MSG("MARKOVERLAPPEDWINDOWS start ");

    HUGE_ROOT(pWin);
    if (IsRoot(pWin)) {
        // root - mark nothing
        RL_DEBUG_MSG("is root not marking ");
        result = FALSE;
    }
    else if (! IsTopLevel(pWin)) {
        // not top-level window - mark normally
        result = pScreen->MarkOverlappedWindows(pWin, pFirst, ppLayerWin);
    }
    else {
        //top-level window - mark children ONLY - NO overlaps with sibs (?)
        // This code copied from miMarkOverlappedWindows()

        register WindowPtr pChild;
        Bool anyMarked = FALSE;
        void (* MarkWindow)() = pScreen->MarkWindow;

        RL_DEBUG_MSG("is top level! ");
        /* single layered systems are easy */
        if (ppLayerWin) *ppLayerWin = pWin;

        if (pWin == pFirst) {
            /* Blindly mark pWin and all of its inferiors.   This is a slight
            * overkill if there are mapped windows that outside pWin's border,
            * but it's better than wasting time on RectIn checks.
            */
            pChild = pWin;
            while (1) {
                if (pChild->viewable) {
                    if (REGION_BROKEN (pScreen, &pChild->winSize))
                        SetWinSize (pChild);
                    if (REGION_BROKEN (pScreen, &pChild->borderSize))
                        SetBorderSize (pChild);
                    (* MarkWindow)(pChild);
                    if (pChild->firstChild) {
                        pChild = pChild->firstChild;
                        continue;
                    }
                }
                while (!pChild->nextSib && (pChild != pWin))
                    pChild = pChild->parent;
                if (pChild == pWin)
                    break;
                pChild = pChild->nextSib;
            }
            anyMarked = TRUE;
            pFirst = pFirst->nextSib;
        }
        if (anyMarked)
            (* MarkWindow)(pWin->parent);
        result = anyMarked;
    }
    NORMAL_ROOT(pWin);
    SCREEN_WRAP(pScreen, MarkOverlappedWindows);
    RL_DEBUG_MSG("MARKOVERLAPPEDWINDOWS end\n");
    return result;
}


static void
RootlessPaintWindowBackground(WindowPtr pWin, RegionPtr pRegion, int what)
{
    int oldBackgroundState = 0;
    PixUnion oldBackground;
    ScreenPtr pScreen = pWin->drawable.pScreen;

    SCREEN_UNWRAP(pScreen, PaintWindowBackground);
    RL_DEBUG_MSG("paintwindowbackground start (win 0x%x) ", pWin);
    if (IsFramedWindow(pWin)) {
        if (IsRoot(pWin)) {
            // set root background to magic transparent color
            oldBackgroundState = pWin->backgroundState;
            oldBackground = pWin->background;
            pWin->backgroundState = BackgroundPixel;
            pWin->background.pixel = 0x00fffffe;
        }
    }

    pScreen->PaintWindowBackground(pWin, pRegion, what);

    if (IsFramedWindow(pWin)) {
        RootlessDamageRegion(pWin, pRegion);
        if (IsRoot(pWin)) {
            pWin->backgroundState = oldBackgroundState;
            pWin->background = oldBackground;
        }
    }
    SCREEN_WRAP(pScreen, PaintWindowBackground);
    RL_DEBUG_MSG("paintwindowbackground end\n");
}


static void
RootlessPaintWindowBorder(WindowPtr pWin, RegionPtr pRegion, int what)
{
    SCREEN_UNWRAP(pWin->drawable.pScreen, PaintWindowBorder);
    RL_DEBUG_MSG("paintwindowborder start (win 0x%x) ", pWin);
    pWin->drawable.pScreen->PaintWindowBorder(pWin, pRegion, what);
    if (IsFramedWindow(pWin)) {
        RootlessDamageRegion(pWin, pRegion);
    }
    SCREEN_WRAP(pWin->drawable.pScreen, PaintWindowBorder);
    RL_DEBUG_MSG("paintwindowborder end\n");
}


// Flush drawing before blocking on select().
static void
RootlessBlockHandler(pointer pbdata, OSTimePtr pTimeout, pointer pReadmask)
{
    RootlessRedisplayScreen((ScreenPtr) pbdata);
}


static void
RootlessWakeupHandler(pointer data, int i, pointer LastSelectMask)
{
    // nothing here
}


static Bool
RootlessAllocatePrivates(ScreenPtr pScreen)
{
    RootlessScreenRec *s;
    static unsigned long rootlessGeneration = 0;

    if (rootlessGeneration != serverGeneration) {
        rootlessScreenPrivateIndex = AllocateScreenPrivateIndex();
        if (rootlessScreenPrivateIndex == -1) return FALSE;
        rootlessGCPrivateIndex = AllocateGCPrivateIndex();
        if (rootlessGCPrivateIndex == -1) return FALSE;
        rootlessWindowPrivateIndex = AllocateWindowPrivateIndex();
        if (rootlessWindowPrivateIndex == -1) return FALSE;
        rootlessGeneration = serverGeneration;
    }

    // no allocation needed for screen privates
    if (!AllocateGCPrivate(pScreen, rootlessGCPrivateIndex,
                           sizeof(RootlessGCRec)))
        return FALSE;
    if (!AllocateWindowPrivate(pScreen, rootlessWindowPrivateIndex, 0))
        return FALSE;

    s = xalloc(sizeof(RootlessScreenRec));
    if (! s) return FALSE;
    SCREENREC(pScreen) = s;

    return TRUE;
}


static void
RootlessWrap(ScreenPtr pScreen)
{
    RootlessScreenRec *s = (RootlessScreenRec*)
            pScreen->devPrivates[rootlessScreenPrivateIndex].ptr;

#define WRAP(a) \
    if (pScreen->a) { \
        s->a = pScreen->a; \
    } else { \
        RL_DEBUG_MSG("null screen fn " #a "\n"); \
        s->a = NULL; \
    } \
    pScreen->a = Rootless##a

    WRAP(CloseScreen);
    WRAP(CreateGC);
    WRAP(PaintWindowBackground);
    WRAP(PaintWindowBorder);
    WRAP(CopyWindow);
    WRAP(GetImage);
    WRAP(CreateWindow);
    WRAP(DestroyWindow);
    WRAP(RealizeWindow);
    WRAP(UnrealizeWindow);
    WRAP(MoveWindow);
    WRAP(PositionWindow);
    WRAP(ResizeWindow);
    WRAP(RestackWindow);
    WRAP(ChangeBorderWidth);
    WRAP(MarkOverlappedWindows);
    WRAP(ValidateTree);
    WRAP(ChangeWindowAttributes);

#ifdef SHAPE
    WRAP(SetShape);
#endif

#ifdef RENDER
    {
        // Composite and Glyphs don't use normal screen wrapping
        PictureScreenPtr ps = GetPictureScreen(pScreen);
        s->Composite = ps->Composite;
        ps->Composite = RootlessComposite;
        s->Glyphs = ps->Glyphs;
        ps->Glyphs = RootlessGlyphs;
    }
#endif

    // WRAP(ClearToBackground); fixme put this back? useful for shaped wins?
    // WRAP(RestoreAreas); fixme put this back?

#undef WRAP
}


/*
 * RootlessInit
 * Rootless wraps lots of stuff and needs a bunch of devPrivates.
 */
Bool RootlessInit(ScreenPtr pScreen, RootlessFrameProcs *procs)
{
    RootlessScreenRec *s;

    if (! RootlessAllocatePrivates(pScreen)) return FALSE;
    s = (RootlessScreenRec*)
            pScreen->devPrivates[rootlessScreenPrivateIndex].ptr;

    s->pScreen = pScreen;
    s->frameProcs = *procs;

    RootlessWrap(pScreen);

    if (!RegisterBlockAndWakeupHandlers (RootlessBlockHandler,
                                         RootlessWakeupHandler,
                                         (pointer) pScreen))
    {
        return FALSE;
    }

    return TRUE;
}

/* **********************************************************
 * Copyright (C) 1998-2001 VMware, Inc.
 * All Rights Reserved
 * **********************************************************/
#ifdef VMX86_DEVEL
char rcsId_vmwarecurs[] =
    "Id: vmwarecurs.c,v 1.5 2001/01/30 23:33:02 bennett Exp $";
#endif
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/vmware/vmwarecurs.c,v 1.11 2003/02/05 12:47:42 dawes Exp $ */

#include "vmware.h"
#include "bits2pixels.h"

static void VMWAREGetImage(DrawablePtr src, int x, int y, int w, int h,
                           unsigned int format, unsigned long planeMask,
                           char *pBinImage);
static void VMWARECopyWindow(WindowPtr pWin, DDXPointRec ptOldOrg,
                             RegionPtr prgnSrc);

#ifdef RENDER
static void VMWAREComposite(CARD8 op, PicturePtr pSrc, PicturePtr pMask,
			    PicturePtr pDst, INT16 xSrc, INT16 ySrc,
			    INT16 xMask, INT16 yMask, INT16 xDst, INT16 yDst,
			    CARD16 width, CARD16 height);
#endif /* RENDER */

static void
RedefineCursor(VMWAREPtr pVMWARE)
{
    int i;

    VmwareLog(("RedefineCursor\n"));

    pVMWARE->cursorDefined = FALSE;

    /* Define cursor */
    vmwareWriteWordToFIFO(pVMWARE, SVGA_CMD_DEFINE_CURSOR);
    vmwareWriteWordToFIFO(pVMWARE, MOUSE_ID);
    vmwareWriteWordToFIFO(pVMWARE, 0);          /* HotX/HotY seem to be zero? */
    vmwareWriteWordToFIFO(pVMWARE, 0);          /* HotX/HotY seem to be zero? */
    vmwareWriteWordToFIFO(pVMWARE, pVMWARE->CursorInfoRec->MaxWidth);
    vmwareWriteWordToFIFO(pVMWARE, pVMWARE->CursorInfoRec->MaxHeight);
    vmwareWriteWordToFIFO(pVMWARE, 1);
    vmwareWriteWordToFIFO(pVMWARE, pVMWARE->bitsPerPixel);

    /*
     * Since we have AND and XOR masks rather than 'source' and 'mask',
     * color expand 'mask' with all zero as its foreground and all one as
     * its background.  This makes 'image & 0 ^ 'source' = source.  We
     * arange for 'image' & 1 ^ 'source' = 'image' below when we clip
     * 'source' below.
     */
    vmwareRaster_BitsToPixels((uint8 *) pVMWARE->hwcur.mask,
                        SVGA_BITMAP_INCREMENT(pVMWARE->CursorInfoRec->MaxWidth),
                        (uint8 *) pVMWARE->hwcur.maskPixmap,
                        SVGA_PIXMAP_INCREMENT(pVMWARE->CursorInfoRec->MaxWidth,
                                              pVMWARE->bitsPerPixel),
                        pVMWARE->bitsPerPixel / 8,
                        pVMWARE->CursorInfoRec->MaxWidth,
                        pVMWARE->CursorInfoRec->MaxHeight, 0, ~0);
    for (i = 0; i < SVGA_BITMAP_SIZE(pVMWARE->CursorInfoRec->MaxWidth,
                                     pVMWARE->CursorInfoRec->MaxHeight); i++) {
        vmwareWriteWordToFIFO(pVMWARE, ~pVMWARE->hwcur.mask[i]);
    }
    
    vmwareRaster_BitsToPixels((uint8 *) pVMWARE->hwcur.source,
                        SVGA_BITMAP_INCREMENT(pVMWARE->CursorInfoRec->MaxWidth),
                        (uint8 *) pVMWARE->hwcur.sourcePixmap,
                        SVGA_PIXMAP_INCREMENT(pVMWARE->CursorInfoRec->MaxWidth,
                                              pVMWARE->bitsPerPixel),
                        pVMWARE->bitsPerPixel / 8,
                        pVMWARE->CursorInfoRec->MaxWidth,
                        pVMWARE->CursorInfoRec->MaxHeight,
                        pVMWARE->hwcur.fg, pVMWARE->hwcur.bg);
    /*
     * As pointed out above, we need to clip the expanded 'source' against
     * the expanded 'mask' since we actually have AND and XOR masks in the
     * virtual hardware.  Effectively, 'source' becomes a three color fg/bg/0
     * pixmap that XORs appropriately.
     */
    for (i = 0; i < SVGA_PIXMAP_SIZE(pVMWARE->CursorInfoRec->MaxWidth,
                                     pVMWARE->CursorInfoRec->MaxHeight,
                                     pVMWARE->bitsPerPixel); i++) {
        pVMWARE->hwcur.sourcePixmap[i] &= ~pVMWARE->hwcur.maskPixmap[i];
	vmwareWriteWordToFIFO(pVMWARE, pVMWARE->hwcur.sourcePixmap[i]);
    }

    /* Sync the FIFO, so that the definition preceeds any use of the cursor */
    vmwareWaitForFB(pVMWARE);
    pVMWARE->cursorDefined = TRUE;
}

static void
vmwareSetCursorColors(ScrnInfoPtr pScrn, int bg, int fg)
{
    VMWAREPtr pVMWARE = VMWAREPTR(pScrn);
    TRACEPOINT

    if (pVMWARE->hwcur.fg != fg || pVMWARE->hwcur.bg != bg) {
        VmwareLog(("SetCursorColors(0x%08x, 0x%08x)\n", bg, fg));
        pVMWARE->hwcur.fg = fg;
        pVMWARE->hwcur.bg = bg;
        RedefineCursor(pVMWARE);
    }
}

static void
vmwareLoadCursorImage(ScrnInfoPtr pScrn, unsigned char *src )
{
    VMWAREPtr pVMWARE = VMWAREPTR(pScrn);
    const int imageSize = SVGA_BITMAP_SIZE(pVMWARE->CursorInfoRec->MaxWidth,
                                           pVMWARE->CursorInfoRec->MaxHeight);
    TRACEPOINT

    memcpy(pVMWARE->hwcur.source, src, imageSize * sizeof(uint32));
    memcpy(pVMWARE->hwcur.mask,
           src + imageSize * sizeof(uint32), imageSize * sizeof(uint32));
    RedefineCursor(pVMWARE);
}

#ifdef ARGB_CURSOR
#include "cursorstr.h"

static Bool
vmwareUseHWCursorARGB(ScreenPtr pScreen, CursorPtr pCurs)
{
    ScrnInfoPtr pScrn = infoFromScreen(pScreen);
    return pCurs->bits->height <= MAX_CURS &&
           pCurs->bits->width <= MAX_CURS &&
           pScrn->bitsPerPixel > 8;
}

static void
vmwareLoadCursorARGB(ScrnInfoPtr pScrn, CursorPtr pCurs)
{
    VMWAREPtr pVMWARE = VMWAREPTR(pScrn);
    CARD32 width = pCurs->bits->width;
    CARD32 height = pCurs->bits->height;
    CARD32* image = pCurs->bits->argb;
    CARD32* imageEnd = image + (width * height);

    pVMWARE->cursorDefined = FALSE;

    vmwareWriteWordToFIFO(pVMWARE, SVGA_CMD_DEFINE_ALPHA_CURSOR);
    vmwareWriteWordToFIFO(pVMWARE, MOUSE_ID);
    vmwareWriteWordToFIFO(pVMWARE, 0);
    vmwareWriteWordToFIFO(pVMWARE, 0);
    vmwareWriteWordToFIFO(pVMWARE, width);
    vmwareWriteWordToFIFO(pVMWARE, height);

    while (image != imageEnd) {
        vmwareWriteWordToFIFO(pVMWARE, *image++);
    }

    vmwareWaitForFB(pVMWARE);

    pVMWARE->cursorDefined = TRUE;
}
#endif

void
vmwareWriteCursorRegs(VMWAREPtr pVMWARE, Bool visible, Bool force)
{
    int enableVal;

    vmwareWriteReg(pVMWARE, SVGA_REG_CURSOR_ID, MOUSE_ID);
    if (visible) {
        vmwareWriteReg(pVMWARE, SVGA_REG_CURSOR_X, pVMWARE->hwcur.x);
        vmwareWriteReg(pVMWARE, SVGA_REG_CURSOR_Y, pVMWARE->hwcur.y);
    }
    
    if (force) {
        enableVal = visible ? SVGA_CURSOR_ON_SHOW : SVGA_CURSOR_ON_HIDE;
    } else {
        enableVal = visible ? pVMWARE->cursorRestoreToFB :
            pVMWARE->cursorRemoveFromFB;
    }
    vmwareWriteReg(pVMWARE, SVGA_REG_CURSOR_ON, enableVal);
}

/* disabled by default to reduce spew in DEBUG_LOGGING mode. */
/* #define DEBUG_LOG_MOUSE_HIDE_SHOW */

static void
vmwareShowCursor(ScrnInfoPtr pScrn)
{
    VMWAREPtr pVMWARE = VMWAREPTR(pScrn);
#ifdef DEBUG_LOG_MOUSE_HIDE_SHOW
    VmwareLog(("Show: %d %d %d\n", pVMWARE->cursorSema, pVMWARE->cursorDefined,
	       pVMWARE->cursorShouldBeHidden));
#endif
    pVMWARE->cursorShouldBeHidden = FALSE;
    if (pVMWARE->cursorSema == 0 && pVMWARE->cursorDefined) {
        vmwareWriteCursorRegs(pVMWARE, TRUE, TRUE);
    }
}

static void
vmwareHideCursor(ScrnInfoPtr pScrn)
{
    VMWAREPtr pVMWARE = VMWAREPTR(pScrn);
#ifdef DEBUG_LOG_MOUSE_HIDE_SHOW
    VmwareLog(("Hide: %d %d %d\n", pVMWARE->cursorSema, pVMWARE->cursorDefined,
	       pVMWARE->cursorShouldBeHidden));
#endif
    if (pVMWARE->cursorDefined) {
        vmwareWriteCursorRegs(pVMWARE, FALSE, TRUE);
    }
    pVMWARE->cursorShouldBeHidden = TRUE;
}

/* disabled by default to reduce spew in DEBUG_LOGGING mode. */
/* #define DEBUG_LOG_MOUSE_MOVE */

static void
vmwareSetCursorPosition(ScrnInfoPtr pScrn, int x, int y)
{
    VMWAREPtr pVMWARE = VMWAREPTR(pScrn);
#ifdef DEBUG_LOG_MOUSE_MOVE
    VmwareLog(("Move: %d %d %d\n", pVMWARE->cursorSema, pVMWARE->cursorDefined,
	       pVMWARE->cursorShouldBeHidden));
#endif
    /*
     * We're bad people.  We have no concept of a frame (VMWAREAdjustFrame()
     * is a NOP).  The hwcursor code expects us to be frame aware though, so
     * we have to do this.  I'm open to suggestions.  I tried not even
     * hooking AdjustFrame and it didn't help.
     */
    pVMWARE->hwcur.x = x + pScrn->frameX0;
    pVMWARE->hwcur.y = y + pScrn->frameY0;
    pVMWARE->hwcur.box.x1 = pVMWARE->hwcur.x;
    pVMWARE->hwcur.box.x2 = pVMWARE->hwcur.x + pVMWARE->CursorInfoRec->MaxWidth;
    pVMWARE->hwcur.box.y1 = pVMWARE->hwcur.y;
    pVMWARE->hwcur.box.y2 = pVMWARE->hwcur.y + pVMWARE->CursorInfoRec->MaxHeight;

    vmwareShowCursor(pScrn);
}

void
vmwareCursorModeInit(ScrnInfoPtr pScrn, DisplayModePtr mode)
{
    VMWAREPtr pVMWARE = VMWAREPTR(pScrn);
    
    if (pVMWARE->cursorDefined) {
        vmwareWriteCursorRegs(pVMWARE, !pVMWARE->cursorShouldBeHidden, TRUE);
    }
}

Bool
vmwareCursorInit(ScreenPtr pScreen)
{
    xf86CursorInfoPtr infoPtr;
    VMWAREPtr pVMWARE = VMWAREPTR(infoFromScreen(pScreen));
    Bool ret;

    TRACEPOINT

    /* Require cursor bypass for hwcursor.  Ignore deprecated FIFO hwcursor */
    if (!(pVMWARE->vmwareCapability & SVGA_CAP_CURSOR_BYPASS)) {
        return FALSE;
    }

    infoPtr = xf86CreateCursorInfoRec();
    if (!infoPtr) 
        return FALSE;

    pVMWARE->CursorInfoRec = infoPtr;

    infoPtr->MaxWidth = MAX_CURS;
    infoPtr->MaxHeight = MAX_CURS;
    infoPtr->Flags = HARDWARE_CURSOR_BIT_ORDER_MSBFIRST |
                     HARDWARE_CURSOR_SOURCE_MASK_NOT_INTERLEAVED;
    infoPtr->SetCursorColors = vmwareSetCursorColors;
    infoPtr->SetCursorPosition = vmwareSetCursorPosition;
    infoPtr->LoadCursorImage = vmwareLoadCursorImage;
    infoPtr->HideCursor = vmwareHideCursor;
    infoPtr->ShowCursor = vmwareShowCursor;

#ifdef ARGB_CURSOR
    if (pVMWARE->vmwareCapability & SVGA_CAP_ALPHA_CURSOR) {
        infoPtr->UseHWCursorARGB = vmwareUseHWCursorARGB;
        infoPtr->LoadCursorARGB = vmwareLoadCursorARGB;
    }
#endif

    ret = xf86InitCursor(pScreen, infoPtr);
    if (!ret) {
        xf86DestroyCursorInfoRec(infoPtr);
        pVMWARE->CursorInfoRec = NULL;
    }
    return ret;
}

void
vmwareCursorCloseScreen(ScreenPtr pScreen)
{
    ScrnInfoPtr pScrn = infoFromScreen(pScreen);
    VMWAREPtr pVMWARE = VMWAREPTR(pScrn);
#ifdef RENDER
    PictureScreenPtr ps = GetPictureScreenIfSet(pScreen);
#endif
    
    pScreen->GetImage = pVMWARE->ScrnFuncs.GetImage;
    pScreen->CopyWindow = pVMWARE->ScrnFuncs.CopyWindow;
#ifdef RENDER
    if (ps) {
        ps->Composite = pVMWARE->Composite;
    }
#endif /* RENDER */

    vmwareHideCursor(pScrn);
    xf86DestroyCursorInfoRec(pVMWARE->CursorInfoRec);
}

/***  Wrap functions that read from the framebuffer ***/

void
vmwareCursorHookWrappers(ScreenPtr pScreen)
{
    VMWAREPtr pVMWARE = VMWAREPTR(infoFromScreen(pScreen));
#ifdef RENDER
    PictureScreenPtr ps = GetPictureScreenIfSet(pScreen);
#endif

    TRACEPOINT

    pVMWARE->ScrnFuncs.GetImage = pScreen->GetImage;
    pVMWARE->ScrnFuncs.CopyWindow = pScreen->CopyWindow;
    pScreen->GetImage = VMWAREGetImage;
    pScreen->CopyWindow = VMWARECopyWindow;

#ifdef RENDER
    if (ps) {
        pVMWARE->Composite = ps->Composite;
        ps->Composite = VMWAREComposite;
    }
#endif /* RENDER */

}

static void
VMWAREGetImage(DrawablePtr src, int x, int y, int w, int h,
               unsigned int format, unsigned long planeMask, char *pBinImage)
{
    ScreenPtr pScreen = src->pScreen;
    VMWAREPtr pVMWARE = VMWAREPTR(infoFromScreen(src->pScreen));
    BoxRec box;
    Bool hidden = FALSE;
    
    VmwareLog(("VMWAREGetImage(%p, %d, %d, %d, %d, %d, %d, %p)\n",
               src, x, y, w, h, format, planeMask, pBinImage));

    box.x1 = src->x + x;
    box.y1 = src->y + y;
    box.x2 = box.x1 + w;
    box.y2 = box.y1 + h;

    if (BOX_INTERSECT(box, pVMWARE->hwcur.box)) {
        PRE_OP_HIDE_CURSOR();
        hidden = TRUE;
    }

    pScreen->GetImage = pVMWARE->ScrnFuncs.GetImage;
    (*pScreen->GetImage)(src, x, y, w, h, format, planeMask, pBinImage);
    pScreen->GetImage = VMWAREGetImage;

    if (hidden) {
        POST_OP_SHOW_CURSOR();
    }
}

static void
VMWARECopyWindow(WindowPtr pWin, DDXPointRec ptOldOrg, RegionPtr prgnSrc)
{
    ScreenPtr pScreen = pWin->drawable.pScreen;
    VMWAREPtr pVMWARE = VMWAREPTR(infoFromScreen(pWin->drawable.pScreen));
    BoxPtr pBB;
    Bool hidden = FALSE;
    
    /*
     * We only worry about the source region here, since shadowfb will
     * take care of the destination region.
     */
    pBB = REGION_EXTENTS(pWin->drawable.pScreen, prgnSrc);

    VmwareLog(("VMWARECopyWindow(%p, (%d, %d), (%d, %d - %d, %d)\n",
               pWin, ptOldOrg.x, ptOldOrg.y,
               pBB->x1, pBB->y1, pBB->x2, pBB->y2));
    
    if (BOX_INTERSECT(*pBB, pVMWARE->hwcur.box)) {
        PRE_OP_HIDE_CURSOR();
        hidden = TRUE;
    }

    pScreen->CopyWindow = pVMWARE->ScrnFuncs.CopyWindow;
    (*pScreen->CopyWindow)(pWin, ptOldOrg, prgnSrc);
    pScreen->CopyWindow = VMWARECopyWindow;
    
    if (hidden) {
        POST_OP_SHOW_CURSOR();
    }
}

#ifdef RENDER
static void
VMWAREComposite(CARD8 op, PicturePtr pSrc, PicturePtr pMask,
		PicturePtr pDst, INT16 xSrc, INT16 ySrc,
		INT16 xMask, INT16 yMask, INT16 xDst, INT16 yDst,
		CARD16 width, CARD16 height)
{
    ScreenPtr pScreen = pDst->pDrawable->pScreen;
    VMWAREPtr pVMWARE = VMWAREPTR(infoFromScreen(pScreen));
    PictureScreenPtr ps = GetPictureScreen(pScreen);
    BoxRec box;
    Bool hidden = FALSE;
    
    VmwareLog(("VMWAREComposite op = %d, pSrc = %p, pMask = %p, pDst = %p,"
               " src = (%d, %d), mask = (%d, %d), dst = (%d, %d), w = %d,"
               " h = %d\n", op, pSrc, pMask, pDst, xSrc, ySrc, xMask, yMask,
               xDst, yDst, width, height));

    /*
     * We only worry about the source region here, since shadowfb or XAA will
     * take care of the destination region.
     */
    box.x1 = pSrc->pDrawable->x + xSrc;
    box.y1 = pSrc->pDrawable->y + ySrc;
    box.x2 = box.x1 + width;
    box.y2 = box.y1 + height;

    if (BOX_INTERSECT(box, pVMWARE->hwcur.box)) {
        PRE_OP_HIDE_CURSOR();
        hidden = TRUE;
    }
    
    ps->Composite = pVMWARE->Composite;
    (*ps->Composite)(op, pSrc, pMask, pDst, xSrc, ySrc,
		     xMask, yMask, xDst, yDst, width, height);
    ps->Composite = VMWAREComposite;

    if (hidden) {
        POST_OP_SHOW_CURSOR();
    }
}
#endif /* RENDER */

/* **********************************************************
 * Copyright (C) 1998-2001 VMware, Inc.
 * All Rights Reserved
 * **********************************************************/
#ifdef VMX86_DEVEL
char rcsId_vmwarexaa[] =
    "Id: $";
#endif
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/vmware/vmwarexaa.c,v 1.5 2003/02/04 01:39:53 dawes Exp $ */

#include "vmware.h"

#define OFFSCREEN_SCRATCH_SIZE 1*1024*1024
/* We'll assume we average about 32x32 alpha surfaces (4096 bytes) or larger */
#define OFFSCREEN_SCRATCH_MAX_SLOTS OFFSCREEN_SCRATCH_SIZE / 4096

const char *vmwareXaaSymbols[] = {
    "XAACreateInfoRec",
    "XAADestroyInfoRec",
    "XAAInit",
    NULL
};

static void vmwareXAASync(ScrnInfoPtr pScrn);

static void vmwareSetupForSolidFill(ScrnInfoPtr pScrn, int color, int rop,
                                    unsigned int planemask);
static void vmwareSubsequentSolidFillRect(ScrnInfoPtr pScrn,
                                          int x, int y, int w, int h);

static void vmwareSetupForScreenToScreenCopy(ScrnInfoPtr pScrn,
                                             int xdir, int ydir, int rop,
                                             unsigned int planemask,
                                             int trans_color);
static void vmwareSubsequentScreenToScreenCopy(ScrnInfoPtr pScrn,
                                               int x1, int y1,
                                               int x2, int y2,
                                               int width, int height);

static void vmwareSetupForScanlineCPUToScreenColorExpandFill(ScrnInfoPtr pScrn,
                                                             int fg, int bg,
                                                             int rop,
                                                             unsigned int planemask);
static void vmwareSubsequentScanlineCPUToScreenColorExpandFill(ScrnInfoPtr pScrn,
                                                               int x, int y,
                                                               int w, int h,
                                                               int skipleft );
static void vmwareSubsequentColorExpandScanline(ScrnInfoPtr pScrn, int bufno);

#ifdef RENDER
static Bool vmwareSetupForCPUToScreenAlphaTexture(ScrnInfoPtr pScrn, int op,
                                                  CARD16 red, CARD16 green,
                                                  CARD16 blue, CARD16 alpha,
                                                  int alphaType, CARD8 *alphaPtr,
                                                  int alphaPitch,
                                                  int width, int height,
                                                  int flags);

static Bool vmwareSetupForCPUToScreenTexture(ScrnInfoPtr pScrn, int op,
                                             int texType, CARD8 *texPtr,
                                             int texPitch,
                                             int width, int height,
                                             int flags);

static void vmwareSubsequentCPUToScreenTexture(ScrnInfoPtr pScrn,
                                               int dstx, int dsty,
                                               int srcx, int srcy,
                                               int width, int height);

CARD32 vmwareAlphaTextureFormats[2] = {PICT_a8, 0};
CARD32 vmwareTextureFormats[2] = {PICT_a8r8g8b8, 0};

#endif

#define DESTROY_XAA_INFO(pVMWARE) \
    if (pVMWARE->xaaInfo) { XAADestroyInfoRec(pVMWARE->xaaInfo); \
    pVMWARE->xaaInfo = NULL; }

Bool
vmwareXAAScreenInit(ScreenPtr pScreen)
{
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    VMWAREPtr pVMWARE = VMWAREPTR(pScrn);
    XAAInfoRecPtr xaaInfo;
        
    pVMWARE->xaaInfo = XAACreateInfoRec();
    if (!pVMWARE->xaaInfo) {
        return FALSE;
    }

    xaaInfo = pVMWARE->xaaInfo;

    xaaInfo->Sync = vmwareXAASync;

    if (pVMWARE->vmwareCapability & SVGA_CAP_RECT_FILL) {
        xaaInfo->SetupForSolidFill = vmwareSetupForSolidFill;
        xaaInfo->SubsequentSolidFillRect = vmwareSubsequentSolidFillRect;

        xaaInfo->SolidFillFlags = NO_PLANEMASK |
            (pVMWARE->vmwareCapability & SVGA_CAP_RASTER_OP ? 0 : GXCOPY_ONLY);
    }

    if (pVMWARE->vmwareCapability & SVGA_CAP_RECT_COPY) {
        xaaInfo->SetupForScreenToScreenCopy = vmwareSetupForScreenToScreenCopy;
        xaaInfo->SubsequentScreenToScreenCopy =
           vmwareSubsequentScreenToScreenCopy;

        xaaInfo->ScreenToScreenCopyFlags = NO_TRANSPARENCY | NO_PLANEMASK |
           (pVMWARE->vmwareCapability & SVGA_CAP_RASTER_OP ? 0 : GXCOPY_ONLY);
    }

    /*
     * We don't support SVGA_CAP_GLYPH without clipping, since we use clipping
     * for normal glyphs.
     */
    if (pVMWARE->vmwareCapability & SVGA_CAP_GLYPH_CLIPPING) {
        xaaInfo->SetupForScanlineCPUToScreenColorExpandFill =
           vmwareSetupForScanlineCPUToScreenColorExpandFill;
        xaaInfo->SubsequentScanlineCPUToScreenColorExpandFill =
           vmwareSubsequentScanlineCPUToScreenColorExpandFill;
        xaaInfo->SubsequentColorExpandScanline =
           vmwareSubsequentColorExpandScanline;

        xaaInfo->NumScanlineColorExpandBuffers = 1;
        xaaInfo->ScanlineColorExpandBuffers = pVMWARE->xaaColorExpScanLine;

        xaaInfo->ScanlineCPUToScreenColorExpandFillFlags = GXCOPY_ONLY |
           NO_PLANEMASK | BIT_ORDER_IN_BYTE_MSBFIRST | LEFT_EDGE_CLIPPING;
    }

    if (pVMWARE->vmwareCapability & SVGA_CAP_OFFSCREEN_1) {
        int scratchSizeBytes = ((OFFSCREEN_SCRATCH_SIZE + pVMWARE->fbPitch - 1) /
                                pVMWARE->fbPitch) * pVMWARE->fbPitch;
        BoxRec box;
        RegionRec region;

        box.x1 = 0;
        box.y1 = (pVMWARE->FbSize + pVMWARE->fbPitch - 1) / pVMWARE->fbPitch;
        box.x2 = pScrn->displayWidth;
        box.y2 = pVMWARE->videoRam / pVMWARE->fbPitch;

#ifdef RENDER
        if (pVMWARE->vmwareCapability & SVGA_CAP_ALPHA_BLEND &&
            pScrn->bitsPerPixel > 8) {
            if (box.y2 - (scratchSizeBytes / pVMWARE->fbPitch) > box.y1 + 4) {
                CARD8* osPtr = pVMWARE->FbBase + pVMWARE->videoRam -
                   scratchSizeBytes;
                box.y2 -= scratchSizeBytes / pVMWARE->fbPitch;

                VmwareLog(("Allocated %d bytes at offset %d for alpha scratch\n",
                           scratchSizeBytes,
                           pVMWARE->videoRam - scratchSizeBytes)); 

                pVMWARE->heap = vmwareHeap_Create(osPtr,
                                                  scratchSizeBytes,
                                                  OFFSCREEN_SCRATCH_MAX_SLOTS,
                                                  pVMWARE->videoRam - scratchSizeBytes,
                                                  pScrn->virtualX,
                                                  pScrn->virtualY,
                                                  pVMWARE->bitsPerPixel,
                                                  pVMWARE->fbPitch,
                                                  pVMWARE->fbOffset);
                pVMWARE->frontBuffer = vmwareHeap_GetFrontBuffer(pVMWARE->heap);

                xaaInfo->SetupForCPUToScreenAlphaTexture =
                   vmwareSetupForCPUToScreenAlphaTexture;
                xaaInfo->SubsequentCPUToScreenAlphaTexture =
                   vmwareSubsequentCPUToScreenTexture;
                xaaInfo->CPUToScreenAlphaTextureFlags = XAA_RENDER_NO_TILE |
                   XAA_RENDER_NO_SRC_ALPHA;
                xaaInfo->CPUToScreenAlphaTextureFormats = vmwareAlphaTextureFormats;

                xaaInfo->SetupForCPUToScreenTexture =
                   vmwareSetupForCPUToScreenTexture;
                xaaInfo->SubsequentCPUToScreenTexture = 
                   vmwareSubsequentCPUToScreenTexture;
                xaaInfo->CPUToScreenTextureFlags = XAA_RENDER_NO_TILE;
                xaaInfo->CPUToScreenTextureFormats = vmwareTextureFormats;
            } else {
                xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "Allocation of offscreen "
                           "scratch area for alpha blending failed\n");
            }
        }
#endif

        if (box.y2 > box.y1) {
            REGION_INIT(pScreen, &region, &box, 1);

            if (REGION_NOTEMPTY(pScreen, &region) &&
                xf86InitFBManagerRegion(pScreen, &region)) {
                VmwareLog(("Offscreen memory initialized: (%d, %d) - (%d, %d)\n",
                           box.x1, box.y1, box.x2, box.y2));

                xaaInfo->Flags =
                   LINEAR_FRAMEBUFFER | PIXMAP_CACHE | OFFSCREEN_PIXMAPS;
            } else {
                xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "Offscreen memory manager "
                           "initialization failed.\n");
            }

            REGION_UNINIT(pScreen, &region);
        }
    }

    if (!XAAInit(pScreen, xaaInfo)) {
        DESTROY_XAA_INFO(pVMWARE);
        return FALSE;
    }

    return TRUE;
}

Bool
vmwareXAAModeInit(ScrnInfoPtr pScrn, DisplayModePtr mode)
{
    VMWAREPtr pVMWARE = VMWAREPTR(pScrn);

    /*
     * Alloc a sufficiently large buffer for XAA to render scanlines
     * for a color expand into.
     */
    if (pVMWARE->vmwareCapability & SVGA_CAP_GLYPH) {
        /*
         * This formula came straight from the XAA.HOWTO doc.  The +62 is
         * there because we potentially have 31 extra bits off to the left,
         * since we claim LEFT_EDGE_CLIPPING support.
         */
        int scanLineSize = ((pScrn->virtualX + 62)/32) * 4;
        if (pVMWARE->xaaColorExpScanLine[0]) {
            xfree(pVMWARE->xaaColorExpScanLine[0]);
        }
        pVMWARE->xaaColorExpScanLine[0] = xalloc(scanLineSize);
        return pVMWARE->xaaColorExpScanLine[0] != NULL;
    }

    return TRUE;
}

void
vmwareXAACloseScreen(ScreenPtr pScreen)
{
    VMWAREPtr pVMWARE = VMWAREPTR(xf86Screens[pScreen->myNum]);

    if (pVMWARE->xaaColorExpScanLine[0]) {
        xfree(pVMWARE->xaaColorExpScanLine[0]);
        pVMWARE->xaaColorExpScanLine[0] = NULL;
    }

    DESTROY_XAA_INFO(pVMWARE);
    
#ifdef RENDER
    if (pVMWARE->heap) {
        vmwareHeap_Destroy(pVMWARE->heap);
        pVMWARE->heap = NULL;
    }
#endif
}

static void
vmwareXAASync(ScrnInfoPtr pScrn)
{
    VMWAREPtr pVMWARE = VMWAREPTR(pScrn);

    VmwareLog(("Sync\n"));

    vmwareWaitForFB(pVMWARE);

#ifdef RENDER
    if (pVMWARE->heap) {
        vmwareHeap_Clear(pVMWARE->heap);
    }
#endif
}

static void
vmwareSetupForSolidFill(ScrnInfoPtr pScrn, 
                        int color, int rop, unsigned int planemask)
{
    VMWAREPtr pVMWARE = VMWAREPTR(pScrn);
    pVMWARE->xaaFGColor = color;
    pVMWARE->xaaRop = rop;

    VmwareLog(("Setup Solid Fill (color = %d, rop = %d)\n", color, rop));
}

static void
vmwareSubsequentSolidFillRect(ScrnInfoPtr pScrn, int x, int y, int w, int h)
{
    VMWAREPtr pVMWARE = VMWAREPTR(pScrn);

    VmwareLog((" Do Solid Fill (x = %d, y = %d, w = %d, h = %d)\n", x, y, w, h));

    if (pVMWARE->xaaRop != GXcopy) {
        /*
         * We'll never get here if SVGA_CAP_RASTER_OP isn't set, since
         * we tell XAA we are GXCOPY_ONLY.
         */
        vmwareWriteWordToFIFO(pVMWARE, SVGA_CMD_RECT_ROP_FILL);
        vmwareWriteWordToFIFO(pVMWARE, pVMWARE->xaaFGColor);
        vmwareWriteWordToFIFO(pVMWARE, x);
        vmwareWriteWordToFIFO(pVMWARE, y);
        vmwareWriteWordToFIFO(pVMWARE, w);
        vmwareWriteWordToFIFO(pVMWARE, h);
        vmwareWriteWordToFIFO(pVMWARE, pVMWARE->xaaRop);
    } else {
        vmwareWriteWordToFIFO(pVMWARE, SVGA_CMD_RECT_FILL);
        vmwareWriteWordToFIFO(pVMWARE, pVMWARE->xaaFGColor);
        vmwareWriteWordToFIFO(pVMWARE, x);
        vmwareWriteWordToFIFO(pVMWARE, y);
        vmwareWriteWordToFIFO(pVMWARE, w);
        vmwareWriteWordToFIFO(pVMWARE, h);
    }
}

static void
vmwareSetupForScreenToScreenCopy(ScrnInfoPtr pScrn,
                                 int xdir, int ydir, int rop,
                                 unsigned int planemask,
                                 int trans_color)
{
    /*
     * We can safely ignore xdir and ydir since our "hardware" is smart
     * enough to figure out the direction from the srcx/y, dstx/y, w and h.
     */
    VMWAREPtr pVMWARE = VMWAREPTR(pScrn);
    pVMWARE->xaaRop = rop;

    VmwareLog(("Setup Screen2Screen copy (rop = %d)\n", rop));
}

static void
vmwareSubsequentScreenToScreenCopy(ScrnInfoPtr pScrn,
                                   int x1, int y1,
                                   int x2, int y2,
                                   int width, int height)
{
    VMWAREPtr pVMWARE = VMWAREPTR(pScrn);

    VmwareLog((" Do Screen2Screen copy (x1 = %d, y1 = %d, x2 = %d, y2 = %d,"
               " w = %d, h = %d)\n", x1, y1, x2, y2, width, height));

    if (pVMWARE->xaaRop != GXcopy) {
        /*
         * We'll never get here if SVGA_CAP_RASTER_OP isn't set, since
         * we tell XAA we are GXCOPY_ONLY.
         */
        vmwareWriteWordToFIFO(pVMWARE, SVGA_CMD_RECT_ROP_COPY);
        vmwareWriteWordToFIFO(pVMWARE, x1);
        vmwareWriteWordToFIFO(pVMWARE, y1);
        vmwareWriteWordToFIFO(pVMWARE, x2);
        vmwareWriteWordToFIFO(pVMWARE, y2);
        vmwareWriteWordToFIFO(pVMWARE, width);
        vmwareWriteWordToFIFO(pVMWARE, height);
        vmwareWriteWordToFIFO(pVMWARE, pVMWARE->xaaRop);
    } else {
        vmwareWriteWordToFIFO(pVMWARE, SVGA_CMD_RECT_COPY);
        vmwareWriteWordToFIFO(pVMWARE, x1);
        vmwareWriteWordToFIFO(pVMWARE, y1);
        vmwareWriteWordToFIFO(pVMWARE, x2);
        vmwareWriteWordToFIFO(pVMWARE, y2);
        vmwareWriteWordToFIFO(pVMWARE, width);
        vmwareWriteWordToFIFO(pVMWARE, height);
    }
}

static void
vmwareSetupForScanlineCPUToScreenColorExpandFill(ScrnInfoPtr pScrn,
                                                 int fg, int bg,
                                                 int rop,
                                                 unsigned int planemask)
{
    VMWAREPtr pVMWARE = VMWAREPTR(pScrn);

    pVMWARE->xaaFGColor = fg;
    pVMWARE->xaaBGColor = bg;
    VmwareLog(("Setup color expand (fg = %d, bg = %d, rop = %d)\n",
               fg, bg, rop));
}

static void
vmwareSubsequentScanlineCPUToScreenColorExpandFill(ScrnInfoPtr pScrn,
                                                   int x, int y,
                                                   int w, int h,
                                                   int skipleft )
{
    /*
     * XXX TODO: if xaaColorExpSize will fit entirely into the FIFO,
     *           temporarily switch to direct mode, and have XAA write bits
     *           directly into the fifo.  See ATI driver for an example.
     */
    VMWAREPtr pVMWARE = VMWAREPTR(pScrn);

    pVMWARE->xaaColorExpSize = SVGA_GLYPH_SCANLINE_SIZE_DWORDS(w);

    VmwareLog((" Do color expand (x = %d, y = %d, w = %d, h = %d,"
               " skipleft = %d, sizedw = %d)\n",
               x, y, w, h, skipleft, pVMWARE->xaaColorExpSize));

    vmwareWriteWordToFIFO(pVMWARE, SVGA_CMD_DRAW_GLYPH_CLIPPED);
    vmwareWriteWordToFIFO(pVMWARE, x);
    vmwareWriteWordToFIFO(pVMWARE, y);
    vmwareWriteWordToFIFO(pVMWARE, pVMWARE->xaaColorExpSize * 32U);
    vmwareWriteWordToFIFO(pVMWARE, h);
    vmwareWriteWordToFIFO(pVMWARE, pVMWARE->xaaFGColor);
    vmwareWriteWordToFIFO(pVMWARE, pVMWARE->xaaBGColor);
    vmwareWriteWordToFIFO(pVMWARE, x + skipleft);
    vmwareWriteWordToFIFO(pVMWARE, y);
    vmwareWriteWordToFIFO(pVMWARE, w - skipleft);
    vmwareWriteWordToFIFO(pVMWARE, h);
}

static void
vmwareSubsequentColorExpandScanline(ScrnInfoPtr pScrn, int bufno)
{
    VMWAREPtr pVMWARE = VMWAREPTR(pScrn);
    CARD32* scanLine = (CARD32*)pVMWARE->xaaColorExpScanLine[0];
    unsigned int dwords = pVMWARE->xaaColorExpSize;

    while (dwords--) {
        vmwareWriteWordToFIFO(pVMWARE, *scanLine++);
    }
}

#ifdef RENDER

static void
RGBPlusAlphaChannelToPremultipliedRGBA(
    CARD8 red, CARD8 blue, CARD8 green,
    CARD8 *alphaPtr,   /* in bytes */
    int alphaPitch,
    CARD32 *dstPtr,
    int dstPitch,	/* in dwords */
    int width, int height)
{
    int x;

    while (height--) {
        for (x = 0; x < width; x++) {
            CARD8 alpha = alphaPtr[x];
            dstPtr[x] = (alpha << 24) |
               ((red * alpha / 255) << 16) |
               ((green * alpha / 255) << 8) |
               (blue * alpha / 255);
        }
        dstPtr += dstPitch;
        alphaPtr += alphaPitch;
    } 
}

Bool
vmwareSetupForCPUToScreenAlphaTexture(ScrnInfoPtr pScrn, int op,
                                      CARD16 red, CARD16 green,
                                      CARD16 blue, CARD16 alpha,
                                      int alphaType, CARD8 *alphaPtr,
                                      int alphaPitch,
                                      int width, int height,
                                      int flags)
{
    VMWAREPtr pVMWARE = VMWAREPTR(pScrn);
    SVGASurface* surf;

    VmwareLog(("Setup alpha texture (op = %d, r = %d, g = %d, b = %d,"
               " a = %d, alphaType = %d, alphaPitch = %d, w = %d, h = %d,"
               " flags = %d)\n", op, red, green, blue, alpha, alphaType,
               alphaPitch, width, height, flags));

    if (op > PictOpSaturate) {
        return FALSE;
    }
    
    surf = vmwareHeap_AllocSurface(pVMWARE->heap, width, height, width * 4, 32);
    
    if (!surf) {
        return FALSE;
    }

    RGBPlusAlphaChannelToPremultipliedRGBA(
       red >> 8, green >> 8, blue >> 8,
       alphaPtr, alphaPitch,
       (CARD32*)(pVMWARE->FbBase + surf->dataOffset),
       width, width, height);

    pVMWARE->curPict = surf;
    pVMWARE->op = op;

    return TRUE;
}

Bool
vmwareSetupForCPUToScreenTexture(ScrnInfoPtr pScrn, int op,
                                 int texType, CARD8 *texPtr,
                                 int texPitch,
                                 int width, int height,
                                 int flags)
{
    VMWAREPtr pVMWARE = VMWAREPTR(pScrn);
    SVGASurface* surf;

    VmwareLog(("Setup texture (op = %d, texType = %d, texPitch = %d,"
               " w = %d, h = %d, flags = %d)\n", op, texType, texPitch,
               width, height, flags));

    if (op > PictOpSaturate) {
        return FALSE;
    }
    
    surf = vmwareHeap_AllocSurface(pVMWARE->heap, width, height, texPitch, 32);
    
    if (!surf) {
        return FALSE;
    }

    memcpy(pVMWARE->FbBase + surf->dataOffset, texPtr, texPitch * height);

    pVMWARE->curPict = surf;
    pVMWARE->op = op;

    return TRUE;
}

void
vmwareSubsequentCPUToScreenTexture(ScrnInfoPtr pScrn,
                                   int dstx, int dsty,
                                   int srcx, int srcy,
                                   int width, int height)
{
    VMWAREPtr pVMWARE = VMWAREPTR(pScrn);

    VmwareLog((" Do texture (dstx = %d, dsty = %d, srcx = %d, srcy = %d"
               " w = %d, h = %d)\n", dstx, dsty, srcx, srcy, width, height));

    pVMWARE->curPict->numQueued++;
    pVMWARE->frontBuffer->numQueued++;
       
    vmwareWriteWordToFIFO(pVMWARE, SVGA_CMD_SURFACE_ALPHA_BLEND);
    vmwareWriteWordToFIFO(pVMWARE, (CARD8*)pVMWARE->curPict - pVMWARE->FbBase);
    vmwareWriteWordToFIFO(pVMWARE, (CARD8*)pVMWARE->frontBuffer - pVMWARE->FbBase);
    vmwareWriteWordToFIFO(pVMWARE, srcx);
    vmwareWriteWordToFIFO(pVMWARE, srcy);
    vmwareWriteWordToFIFO(pVMWARE, dstx);
    vmwareWriteWordToFIFO(pVMWARE, dsty);
    vmwareWriteWordToFIFO(pVMWARE, width);
    vmwareWriteWordToFIFO(pVMWARE, height);
    vmwareWriteWordToFIFO(pVMWARE, pVMWARE->op);
    vmwareWriteWordToFIFO(pVMWARE, 0);  /* flags */
    vmwareWriteWordToFIFO(pVMWARE, 0);  /* param1 */
    vmwareWriteWordToFIFO(pVMWARE, 0);  /* param2 */
}
#endif

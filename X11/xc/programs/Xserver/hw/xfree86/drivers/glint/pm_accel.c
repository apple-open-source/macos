/*
 * Copyright 1997-2001 by Alan Hourihane, Wigan, England.
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Alan Hourihane not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  Alan Hourihane makes no representations
 * about the suitability of this software for any purpose.  It is provided
 * "as is" without express or implied warranty.
 *
 * ALAN HOURIHANE DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL ALAN HOURIHANE BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 *
 * Authors:  Alan Hourihane, <alanh@fairlite.demon.co.uk>
 *           Dirk Hohndel, <hohndel@suse.de>
 *           Stefan Dirsch, <sndirsch@suse.de>
 *
 * this work is sponsored by S.u.S.E. GmbH, Fuerth, Elsa GmbH, Aachen and
 * Siemens Nixdorf Informationssysteme
 * 
 * Permedia accelerated options.
 */
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/glint/pm_accel.c,v 1.23 2001/05/29 11:23:38 alanh Exp $ */

#include "xf86.h"
#include "xf86_OSproc.h"
#include "xf86_ansic.h"

#include "xf86PciInfo.h"
#include "xf86Pci.h"

#include "fb.h"

#include "glint_regs.h"
#include "glint.h"

#include "miline.h"		/* for octants */
#include "xaalocal.h"		/* For replacements */

static void PermediaSync(ScrnInfoPtr pScrn);
static void PermediaSetupForFillRectSolid(ScrnInfoPtr pScrn, int color, int rop,
				unsigned int planemask);
static void PermediaSubsequentFillRectSolid(ScrnInfoPtr pScrn, int x, int y,
				int w, int h);
static void PermediaSetupForSolidLine(ScrnInfoPtr pScrn, int color,
				int rop, unsigned int planemask);
static void PermediaSubsequentHorVertLine(ScrnInfoPtr pScrn, int x, int y, 
				int len, int dir);
static void PermediaSubsequentSolidBresenhamLine(ScrnInfoPtr pScrn,
        			int x, int y, int dmaj, int dmin, int e, 
				int len, int octant);
static void PermediaSubsequentScreenToScreenCopy(ScrnInfoPtr pScrn, int x1, 
				int y1, int x2, int y2, int w, int h);
static void PermediaSetupForScreenToScreenCopy(ScrnInfoPtr pScrn, int xdir,
				int ydir, int rop, unsigned int planemask,
				int transparency_color);
static void PermediaSetClippingRectangle(ScrnInfoPtr pScrn, int x1, int y1, 
				int x2, int y2);
static void PermediaDisableClipping(ScrnInfoPtr pScrn);
static void PermediaSetupForScanlineCPUToScreenColorExpandFill(
				ScrnInfoPtr pScrn,
				int fg, int bg, int rop, 
				unsigned int planemask);
static void PermediaSubsequentScanlineCPUToScreenColorExpandFill(
				ScrnInfoPtr pScrn, int x,
				int y, int w, int h, int skipleft);
static void PermediaSubsequentColorExpandScanline(ScrnInfoPtr pScrn, int bufno);
static void PermediaWriteBitmap(ScrnInfoPtr pScrn, int x, int y, int w, int h,
    				unsigned char *src, int srcwidth, int skipleft,
    				int fg, int bg, int rop,unsigned int planemask);
static void PermediaWritePixmap8bpp(ScrnInfoPtr pScrn, int x, int y, int w,
    				int h, unsigned char *src, int srcwidth,
    				int rop, unsigned int planemask,
    				int transparency_color, int bpp, int depth);
static void PermediaWritePixmap16bpp(ScrnInfoPtr pScrn, int x, int y, int w,
    				int h, unsigned char *src, int srcwidth,
    				int rop, unsigned int planemask,
    				int transparency_color, int bpp, int depth);
static void PermediaWritePixmap32bpp(ScrnInfoPtr pScrn, int x, int y, int w,
    				int h, unsigned char *src, int srcwidth,
    				int rop, unsigned int planemask,
    				int transparency_color, int bpp, int depth);
static void PermediaSetupForMono8x8PatternFill(ScrnInfoPtr pScrn, 
				int patternx, int patterny,
				int fg, int bg, int rop,
				unsigned planemask);
static void PermediaSubsequentMono8x8PatternFillRect(ScrnInfoPtr pScrn,
				int patternx, int patterny, int x, int y,
			   	int w, int h);
static void PermediaLoadCoord(ScrnInfoPtr pScrn, int x, int y, int w, int h,
				int a, int d);
static void PermediaPolylinesThinSolidWrapper(DrawablePtr pDraw, GCPtr pGC,
   				int mode, int npt, DDXPointPtr pPts);
static void PermediaPolySegmentThinSolidWrapper(DrawablePtr pDraw, GCPtr pGC,
 				int nseg, xSegment *pSeg);

void
PermediaInitializeEngine(ScrnInfoPtr pScrn)
{
    GLINTPtr pGlint = GLINTPTR(pScrn);

    /* Initialize the Accelerator Engine to defaults */

    GLINT_SLOW_WRITE_REG(UNIT_DISABLE,	ScissorMode);
    GLINT_SLOW_WRITE_REG(UNIT_ENABLE,	FBWriteMode);
    GLINT_SLOW_WRITE_REG(0, 		dXSub);
    GLINT_SLOW_WRITE_REG(GWIN_DisableLBUpdate,   GLINTWindow);
    GLINT_SLOW_WRITE_REG(UNIT_DISABLE,	DitherMode);
    GLINT_SLOW_WRITE_REG(UNIT_DISABLE,	AlphaBlendMode);
    GLINT_SLOW_WRITE_REG(UNIT_DISABLE,	ColorDDAMode);
    GLINT_SLOW_WRITE_REG(UNIT_DISABLE,	TextureColorMode);
    GLINT_SLOW_WRITE_REG(UNIT_DISABLE,	TextureAddressMode);
    GLINT_SLOW_WRITE_REG(UNIT_DISABLE,	PMTextureReadMode);
    GLINT_SLOW_WRITE_REG(pGlint->pprod,	LBReadMode);
    GLINT_SLOW_WRITE_REG(UNIT_DISABLE,	AlphaBlendMode);
    GLINT_SLOW_WRITE_REG(UNIT_DISABLE,	TexelLUTMode);
    GLINT_SLOW_WRITE_REG(UNIT_DISABLE,	YUVMode);
    GLINT_SLOW_WRITE_REG(UNIT_DISABLE,	DepthMode);
    GLINT_SLOW_WRITE_REG(UNIT_DISABLE,	RouterMode);
    GLINT_SLOW_WRITE_REG(UNIT_DISABLE,	FogMode);
    GLINT_SLOW_WRITE_REG(UNIT_DISABLE,	AntialiasMode);
    GLINT_SLOW_WRITE_REG(UNIT_DISABLE,	AlphaTestMode);
    GLINT_SLOW_WRITE_REG(UNIT_DISABLE,	StencilMode);
    GLINT_SLOW_WRITE_REG(UNIT_DISABLE,	AreaStippleMode);
    GLINT_SLOW_WRITE_REG(UNIT_DISABLE,	LogicalOpMode);
    GLINT_SLOW_WRITE_REG(UNIT_DISABLE,	DepthMode);
    GLINT_SLOW_WRITE_REG(UNIT_DISABLE,	StatisticMode);
    GLINT_SLOW_WRITE_REG(0x400,		FilterMode);
    GLINT_SLOW_WRITE_REG(0xffffffff,	FBHardwareWriteMask);
    GLINT_SLOW_WRITE_REG(0xffffffff,	FBSoftwareWriteMask);
    GLINT_SLOW_WRITE_REG(UNIT_DISABLE,	RasterizerMode);
    GLINT_SLOW_WRITE_REG(UNIT_DISABLE,	GLINTDepth);
    GLINT_SLOW_WRITE_REG(UNIT_DISABLE,	FBSourceOffset);
    GLINT_SLOW_WRITE_REG(UNIT_DISABLE,	FBPixelOffset);
    GLINT_SLOW_WRITE_REG(UNIT_DISABLE,	LBSourceOffset);
    GLINT_SLOW_WRITE_REG(UNIT_DISABLE,	WindowOrigin);
    GLINT_SLOW_WRITE_REG(UNIT_DISABLE,	FBWindowBase);
    GLINT_SLOW_WRITE_REG(UNIT_DISABLE,	LBWindowBase);

    switch (pScrn->bitsPerPixel) {
	case 8:
	    GLINT_SLOW_WRITE_REG(0x0, FBReadPixel); /* 8 Bits */
	    GLINT_SLOW_WRITE_REG(pGlint->pprod,	PMTextureMapFormat);
	    break;
	case 16:
	    GLINT_SLOW_WRITE_REG(0x1, FBReadPixel); /* 16 Bits */
	    GLINT_SLOW_WRITE_REG(pGlint->pprod | 1<<19,	PMTextureMapFormat);
	    break;
	case 32:
	    GLINT_SLOW_WRITE_REG(0x2, FBReadPixel); /* 32 Bits */
	    GLINT_SLOW_WRITE_REG(pGlint->pprod | 2<<19,	PMTextureMapFormat);
  	    break;
    }
    pGlint->ROP = 0xFF;
    pGlint->ClippingOn = FALSE;
    pGlint->startxsub = 0;
    pGlint->startxdom = 0;
    pGlint->starty = 0;
    pGlint->count = 0;
    pGlint->dxdom = 0;
    pGlint->dy = 1<<16;
    GLINT_WAIT(6);
    GLINT_WRITE_REG(0, StartXSub);
    GLINT_WRITE_REG(0,StartXDom);
    GLINT_WRITE_REG(0,StartY);
    GLINT_WRITE_REG(0,GLINTCount);
    GLINT_WRITE_REG(0,dXDom);
    GLINT_WRITE_REG(1<<16,dY);
}

Bool
PermediaAccelInit(ScreenPtr pScreen)
{
    XAAInfoRecPtr infoPtr;
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    GLINTPtr pGlint = GLINTPTR(pScrn);
    BoxRec AvailFBArea;

    pGlint->AccelInfoRec = infoPtr = XAACreateInfoRec();
    if (!infoPtr) return FALSE;

    PermediaInitializeEngine(pScrn);

    infoPtr->Flags = PIXMAP_CACHE |
		     LINEAR_FRAMEBUFFER |
		     OFFSCREEN_PIXMAPS;
 
    infoPtr->Sync = PermediaSync;

    infoPtr->SetClippingRectangle = PermediaSetClippingRectangle;
    infoPtr->DisableClipping = PermediaDisableClipping;
    infoPtr->ClippingFlags = HARDWARE_CLIP_MONO_8x8_FILL;

    infoPtr->SolidFillFlags = 0;
    infoPtr->SetupForSolidFill = PermediaSetupForFillRectSolid;
    infoPtr->SubsequentSolidFillRect = PermediaSubsequentFillRectSolid;
    infoPtr->SolidLineFlags = 0;
    infoPtr->PolySegmentThinSolidFlags = 0;
    infoPtr->PolylinesThinSolidFlags = 0;
    infoPtr->SetupForSolidLine = PermediaSetupForSolidLine;
    infoPtr->SubsequentSolidHorVertLine = PermediaSubsequentHorVertLine;
    if (!(pScrn->overlayFlags & OVERLAY_8_32_PLANAR))
    {
        infoPtr->SubsequentSolidBresenhamLine = 
				PermediaSubsequentSolidBresenhamLine;
    }
    infoPtr->PolySegmentThinSolid = PermediaPolySegmentThinSolidWrapper;
    infoPtr->PolylinesThinSolid = PermediaPolylinesThinSolidWrapper;
  
    infoPtr->ScreenToScreenCopyFlags = NO_TRANSPARENCY;

    infoPtr->SetupForScreenToScreenCopy = PermediaSetupForScreenToScreenCopy;
    infoPtr->SubsequentScreenToScreenCopy = PermediaSubsequentScreenToScreenCopy;

    infoPtr->Mono8x8PatternFillFlags = 
    				HARDWARE_PATTERN_PROGRAMMED_ORIGIN |
    				HARDWARE_PATTERN_PROGRAMMED_BITS |
    				HARDWARE_PATTERN_SCREEN_ORIGIN;

    infoPtr->SetupForMono8x8PatternFill =
				PermediaSetupForMono8x8PatternFill;
    infoPtr->SubsequentMono8x8PatternFillRect = 
				PermediaSubsequentMono8x8PatternFillRect;

    infoPtr->ScanlineCPUToScreenColorExpandFillFlags = 
#if 0
					       LEFT_EDGE_CLIPPING |
					       LEFT_EDGE_CLIPPING_NEGATIVE_X |
#endif
					       BIT_ORDER_IN_BYTE_LSBFIRST;

    infoPtr->NumScanlineColorExpandBuffers = 1;
    pGlint->ScratchBuffer                 = xalloc(((pScrn->virtualX+62)/32*4)
					    + (pScrn->virtualX
					    * pScrn->bitsPerPixel / 8));
    infoPtr->ScanlineColorExpandBuffers = 
					pGlint->XAAScanlineColorExpandBuffers;
    pGlint->XAAScanlineColorExpandBuffers[0] = 
					pGlint->IOBase + OutputFIFO + 4;

    infoPtr->SetupForScanlineCPUToScreenColorExpandFill =
			PermediaSetupForScanlineCPUToScreenColorExpandFill;
    infoPtr->SubsequentScanlineCPUToScreenColorExpandFill = 
			PermediaSubsequentScanlineCPUToScreenColorExpandFill;
    infoPtr->SubsequentColorExpandScanline = 
			PermediaSubsequentColorExpandScanline;

    infoPtr->ColorExpandRange = pGlint->FIFOSize;

    infoPtr->WriteBitmap = PermediaWriteBitmap;

    if (pScrn->bitsPerPixel == 8)
        infoPtr->WritePixmap = PermediaWritePixmap8bpp;
    else
    if (pScrn->bitsPerPixel == 16)
        infoPtr->WritePixmap = PermediaWritePixmap16bpp;
    else
    if (pScrn->bitsPerPixel == 32)
        infoPtr->WritePixmap = PermediaWritePixmap32bpp;

    AvailFBArea.x1 = 0;
    AvailFBArea.y1 = 0;
    AvailFBArea.x2 = pScrn->displayWidth;
    AvailFBArea.y2 = pGlint->FbMapSize / (pScrn->displayWidth * 
					  pScrn->bitsPerPixel / 8);
  
    if (AvailFBArea.y2 > 1023) AvailFBArea.y2 = 1023;

    xf86InitFBManager(pScreen, &AvailFBArea);

    return (XAAInit(pScreen, infoPtr));
}

static void PermediaLoadCoord(
	ScrnInfoPtr pScrn,
	int x, int y,
	int w, int h,
	int a, int d
){
    GLINTPtr pGlint = GLINTPTR(pScrn);
    
    if (w != pGlint->startxsub) {
    	GLINT_WRITE_REG(w, StartXSub);
	pGlint->startxsub = w;
    }
    if (x != pGlint->startxdom) {
    	GLINT_WRITE_REG(x,StartXDom);
	pGlint->startxdom = x;
    }
    if (y != pGlint->starty) {
    	GLINT_WRITE_REG(y,StartY);
	pGlint->starty = y;
    }
    if (h != pGlint->count) {
    	GLINT_WRITE_REG(h,GLINTCount);
	pGlint->count = h;
    }
    if (a != pGlint->dxdom) {
    	GLINT_WRITE_REG(a,dXDom);
	pGlint->dxdom = a;
    }
    if (d != pGlint->dy) {
    	GLINT_WRITE_REG(d,dY);
	pGlint->dy = d;
    }
}

static void
PermediaSync(ScrnInfoPtr pScrn)
{
	GLINTPtr pGlint = GLINTPTR(pScrn);

	CHECKCLIPPING;

	while (GLINT_READ_REG(DMACount) != 0);
	GLINT_WAIT(2);
    	GLINT_WRITE_REG(0x400, FilterMode);
	GLINT_WRITE_REG(0, GlintSync);
	do {
    		while(GLINT_READ_REG(OutFIFOWords) == 0);
	} while (GLINT_READ_REG(OutputFIFO) != Sync_tag);
}

static void
PermediaSetClippingRectangle(
	ScrnInfoPtr pScrn,
	int x1, int y1, 
	int x2, int y2
){
    GLINTPtr pGlint = GLINTPTR(pScrn);
    GLINT_WAIT(3);
    GLINT_WRITE_REG (((y1&0x0FFF) << 16) | (x1&0x0FFF), ScissorMinXY);
    GLINT_WRITE_REG (((y2&0x0FFF) << 16) | (x2&0x0FFF), ScissorMaxXY);
    GLINT_WRITE_REG (1, ScissorMode);
    pGlint->ClippingOn = TRUE;
}

static void
PermediaDisableClipping(
	ScrnInfoPtr pScrn
){
    GLINTPtr pGlint = GLINTPTR(pScrn);
    CHECKCLIPPING;
}

static void
PermediaSetupForScreenToScreenCopy(
	ScrnInfoPtr pScrn, 
	int xdir, int ydir, int rop,
	unsigned int planemask, int transparency_color
){
    GLINTPtr pGlint = GLINTPTR(pScrn);

    pGlint->BltScanDirection = 0;
    if (ydir == 1) pGlint->BltScanDirection |= YPositive;

    GLINT_WAIT(4);
    DO_PLANEMASK(planemask);
    GLINT_WRITE_REG(0, RasterizerMode);
    GLINT_WRITE_REG(UNIT_DISABLE, ColorDDAMode);

    if ((rop == GXset) || (rop == GXclear)) {
	pGlint->FrameBufferReadMode = pGlint->pprod;
    } else
    if ((rop == GXcopy) || (rop == GXcopyInverted)) {
	pGlint->FrameBufferReadMode = pGlint->pprod | FBRM_SrcEnable;
    } else {
	pGlint->FrameBufferReadMode = pGlint->pprod | FBRM_SrcEnable | 
				      FBRM_DstEnable;
    }
    LOADROP(rop);
}

static void
PermediaSubsequentScreenToScreenCopy(
	ScrnInfoPtr pScrn, 
	int x1, int y1, 
	int x2, int y2,
	int w, int h)
{
    GLINTPtr pGlint = GLINTPTR(pScrn);
    int srcaddr;
    int dstaddr;
    char align;
    int direction;

    if (!(pGlint->BltScanDirection & YPositive)) {
	y1 = y1 + h - 1;
	y2 = y2 + h - 1;
	direction = -1<<16;
    } else {
	direction = 1<<16;
    }

    /* We can only use GXcopy for Packed modes, and less than 32 width
     * gives us speed for small blits. */
    if ((w < 32) || (pGlint->ROP != GXcopy)) {
  	GLINT_WAIT(9);
	PermediaLoadCoord(pScrn, x2<<16, y2<<16, (x2+w)<<16, h, 0, direction);
  	srcaddr = x1;
  	dstaddr = x2;
	GLINT_WRITE_REG(pGlint->FrameBufferReadMode, FBReadMode);
    } else {
  	GLINT_WAIT(10);
	PermediaLoadCoord(pScrn, (x2>>pGlint->BppShift)<<16, y2<<16, 
				((x2+w+7)>>pGlint->BppShift)<<16, h, 0, 
				direction);
  	srcaddr = (x1 & ~pGlint->bppalign);
  	dstaddr = (x2 & ~pGlint->bppalign);
  	align = (x2 & pGlint->bppalign) - (x1 & pGlint->bppalign);
	GLINT_WRITE_REG(pGlint->FrameBufferReadMode | FBRM_Packed | 
						(align&7)<<20, FBReadMode);
  	GLINT_WRITE_REG(x2<<16|(x2+w), PackedDataLimits);
    }
    srcaddr += y1 * pScrn->displayWidth;
    dstaddr += y2 * pScrn->displayWidth;
    GLINT_WRITE_REG(srcaddr - dstaddr, FBSourceOffset);
    GLINT_WRITE_REG(PrimitiveTrapezoid, Render);
}

static void 
PermediaSetupForFillRectSolid(
	ScrnInfoPtr pScrn, 
	int color, int rop, 
	unsigned int planemask
){
    GLINTPtr pGlint = GLINTPTR(pScrn);
    REPLICATE(color);

    GLINT_WAIT(6);
    DO_PLANEMASK(planemask);
    GLINT_WRITE_REG(0, RasterizerMode);
    if (rop == GXcopy) {
  	GLINT_WRITE_REG(pGlint->pprod, FBReadMode);
  	GLINT_WRITE_REG(UNIT_DISABLE, ColorDDAMode);
	GLINT_WRITE_REG(color, FBBlockColor);
    } else {
      	GLINT_WRITE_REG(UNIT_ENABLE, ColorDDAMode);
      	GLINT_WRITE_REG(color, ConstantColor);
    }
    LOADROP(rop);
}

static void
PermediaSubsequentFillRectSolid(
	ScrnInfoPtr pScrn, 
	int x, int y, 
	int w, int h)
{
    GLINTPtr pGlint = GLINTPTR(pScrn);
    int speed = 0;
    if (pGlint->ROP == GXcopy) {
	GLINT_WAIT(7);
	PermediaLoadCoord(pScrn, x<<16, y<<16, (x+w)<<16, h, 0, 1<<16);
  	speed = FastFillEnable;
    } else {
	GLINT_WAIT(9);
      	GLINT_WRITE_REG(pGlint->pprod | FBRM_Packed | FBRM_DstEnable, FBReadMode);
	PermediaLoadCoord(pScrn, (x>>pGlint->BppShift)<<16, y<<16, 
				((x+w+7)>>pGlint->BppShift)<<16, h, 0, 1<<16);
  	GLINT_WRITE_REG(x<<16|(x+w), PackedDataLimits);
    }
    GLINT_WRITE_REG(PrimitiveTrapezoid | speed, Render);
}

static void
PermediaSetupForMono8x8PatternFill(
	ScrnInfoPtr pScrn, 
	int patternx, int patterny,
	int fg, int bg, int rop,
	unsigned int planemask)
{
    GLINTPtr pGlint = GLINTPTR(pScrn);
    if (bg == -1) pGlint->FrameBufferReadMode = -1;
	else	pGlint->FrameBufferReadMode = 0;
    pGlint->ForeGroundColor = fg;
    pGlint->BackGroundColor = bg;
    REPLICATE(pGlint->ForeGroundColor);
    REPLICATE(pGlint->BackGroundColor);

    GLINT_WAIT(8);
    GLINT_WRITE_REG ((patternx & 0x000000ff), AreaStipplePattern0);
    GLINT_WRITE_REG ((patternx & 0x0000ff00) >> 8, AreaStipplePattern1);
    GLINT_WRITE_REG ((patternx & 0x00ff0000) >> 16, AreaStipplePattern2);
    GLINT_WRITE_REG ((patternx & 0xff000000) >> 24, AreaStipplePattern3);
    GLINT_WRITE_REG ((patterny & 0x000000ff), AreaStipplePattern4);
    GLINT_WRITE_REG ((patterny & 0x0000ff00) >> 8, AreaStipplePattern5);
    GLINT_WRITE_REG ((patterny & 0x00ff0000) >> 16, AreaStipplePattern6);
    GLINT_WRITE_REG ((patterny & 0xff000000) >> 24, AreaStipplePattern7);

    GLINT_WAIT(7);
    DO_PLANEMASK(planemask);
    GLINT_WRITE_REG(0, RasterizerMode);

    if (rop == GXcopy) {
	GLINT_WRITE_REG(pGlint->pprod, FBReadMode);
    } else {
	GLINT_WRITE_REG(pGlint->pprod | FBRM_DstEnable, FBReadMode);
    }
    GLINT_WRITE_REG(UNIT_ENABLE, ColorDDAMode);
    GLINT_WRITE_REG(pGlint->ForeGroundColor, ConstantColor);
    GLINT_WRITE_REG(pGlint->BackGroundColor, Texel0);
    LOADROP(rop);
}

static void 
PermediaSubsequentMono8x8PatternFillRect(
	ScrnInfoPtr pScrn,
	int patternx, int patterny, 
	int x, int y,
	int w, int h)
{
    GLINTPtr pGlint = GLINTPTR(pScrn);
    GLINT_WAIT(8);
    PermediaLoadCoord(pScrn, x<<16, y<<16, (x+w)<<16, h, 0, 1<<16);
    if (pGlint->FrameBufferReadMode != -1) {
   	GLINT_WRITE_REG(1<<20|patternx<<7|patterny<<12|UNIT_ENABLE, 
							AreaStippleMode);
  	GLINT_WRITE_REG(AreaStippleEnable | TextureEnable | PrimitiveTrapezoid, 
							Render);
    } else { 
  	GLINT_WRITE_REG(patternx<<7|patterny<<12|UNIT_ENABLE, AreaStippleMode);
  	GLINT_WRITE_REG(AreaStippleEnable | PrimitiveTrapezoid, Render);
    }
}

static void 
PermediaWriteBitmap(ScrnInfoPtr pScrn, 
    int x, int y, int w, int h,
    unsigned char *src,
    int srcwidth,
    int skipleft,
    int fg, int bg,
    int rop,
    unsigned int planemask
){
    XAAInfoRecPtr infoRec = GET_XAAINFORECPTR_FROM_SCRNINFOPTR(pScrn);
    GLINTPtr pGlint = GLINTPTR(pScrn);
    unsigned char *srcpntr;
    int dwords, height;
    register int count; 
    register CARD32* pattern;
    int dobackground = 0;

    w += skipleft;
    x -= skipleft;
    dwords = (w + 31) >> 5;

    PermediaSetClippingRectangle(pScrn, x+skipleft, y, x+w, y+h);

    GLINT_WAIT(14);
    DO_PLANEMASK(planemask);
    LOADROP(rop);
    if (bg != -1) dobackground = ForceBackgroundColor;
    if (rop == GXcopy) {
	GLINT_WRITE_REG(pGlint->pprod, FBReadMode);
    } else {
	GLINT_WRITE_REG(pGlint->pprod | FBRM_DstEnable, FBReadMode);
    }
    pGlint->BackGroundColor = bg;
    pGlint->ForeGroundColor = fg;
    REPLICATE(fg);
    REPLICATE(bg);
    if ((rop == GXcopy) && (pGlint->BackGroundColor == -1)) {
	pGlint->FrameBufferReadMode = FastFillEnable;
    	GLINT_WRITE_REG(0, RasterizerMode);
	GLINT_WRITE_REG(UNIT_DISABLE, ColorDDAMode);
	GLINT_WRITE_REG(fg, FBBlockColor);
    } else {
    	GLINT_WRITE_REG(BitMaskPackingEachScanline|dobackground,RasterizerMode);
	GLINT_WRITE_REG(UNIT_ENABLE, ColorDDAMode);
	GLINT_WRITE_REG(fg, ConstantColor);
	if (dobackground) {
	    GLINT_WRITE_REG(bg, Texel0);
	    pGlint->FrameBufferReadMode = TextureEnable;
	} else {
            pGlint->FrameBufferReadMode = 0;
	}
    }
    PermediaLoadCoord(pScrn, x<<16, y<<16, (x+w)<<16, h, 0, 1<<16);

    GLINT_WRITE_REG(PrimitiveTrapezoid | pGlint->FrameBufferReadMode | SyncOnBitMask, Render);
    
    height = h;
    srcpntr = src;
    while(height--) {
	count = dwords >> 3;
	pattern = (CARD32*)srcpntr;
	while(count--) {
		GLINT_WAIT(8);
		GLINT_WRITE_REG(*(pattern), BitMaskPattern);
		GLINT_WRITE_REG(*(pattern+1), BitMaskPattern);
		GLINT_WRITE_REG(*(pattern+2), BitMaskPattern);
		GLINT_WRITE_REG(*(pattern+3), BitMaskPattern);
		GLINT_WRITE_REG(*(pattern+4), BitMaskPattern);
		GLINT_WRITE_REG(*(pattern+5), BitMaskPattern);
		GLINT_WRITE_REG(*(pattern+6), BitMaskPattern);
		GLINT_WRITE_REG(*(pattern+7), BitMaskPattern);
		pattern+=8;
	}
	count = dwords & 0x07;
	GLINT_WAIT(count);
	while (count--)
		GLINT_WRITE_REG(*(pattern++), BitMaskPattern);
	srcpntr += srcwidth;
    }    

    PermediaDisableClipping(pScrn);
    SET_SYNC_FLAG(infoRec);	
}

static void
PermediaSetupForScanlineCPUToScreenColorExpandFill(
	ScrnInfoPtr pScrn,
	int fg, int bg, 
	int rop, 
	unsigned int planemask
){
    GLINTPtr pGlint = GLINTPTR(pScrn);
    int dobackground = 0;

    if (bg != -1) dobackground |= ForceBackgroundColor;
    pGlint->BackGroundColor = bg;
    pGlint->ForeGroundColor = fg;
    REPLICATE(fg);
    REPLICATE(bg);
    GLINT_WAIT(7);
    DO_PLANEMASK(planemask);
    if (rop == GXcopy) {
  	GLINT_WRITE_REG(pGlint->pprod, FBReadMode);
    } else {
  	GLINT_WRITE_REG(pGlint->pprod|FBRM_DstEnable, FBReadMode);
    }
    if ((rop == GXcopy) && (pGlint->BackGroundColor == -1)) {
	pGlint->FrameBufferReadMode = FastFillEnable;
  	GLINT_WRITE_REG(UNIT_DISABLE, ColorDDAMode);
        GLINT_WRITE_REG(fg, FBBlockColor);
	GLINT_WRITE_REG(0,RasterizerMode);
    } else {
        GLINT_WRITE_REG(UNIT_ENABLE, ColorDDAMode);
        GLINT_WRITE_REG(fg, ConstantColor);
	GLINT_WRITE_REG(BitMaskPackingEachScanline|dobackground,RasterizerMode);
	if (dobackground) {
	    GLINT_WRITE_REG(bg, Texel0);
	    pGlint->FrameBufferReadMode = TextureEnable;
	} else {
            pGlint->FrameBufferReadMode = 0;
	}
    }
    LOADROP(rop);
}

static void
PermediaSubsequentScanlineCPUToScreenColorExpandFill(
	ScrnInfoPtr pScrn,
	int x, int y, int w, int h,
	int skipleft
){
    GLINTPtr pGlint = GLINTPTR(pScrn);

    pGlint->dwords = ((w + 31) >> 5); /* dwords per scanline */

#if 0
    PermediaSetClippingRectangle(pScrn, x+skipleft, y, x+w, y+h);
#endif

    pGlint->cpucount = h;

    GLINT_WAIT(8);
    PermediaLoadCoord(pScrn, x<<16, y<<16, (x+w)<<16, h, 0, 1<<16);
    GLINT_WRITE_REG(PrimitiveTrapezoid | pGlint->FrameBufferReadMode | SyncOnBitMask,
							Render);
#if defined(__alpha__)
    if (0) /* force Alpha to use indirect always */
#else
    if ((pGlint->dwords*h) < pGlint->FIFOSize)
#endif
    {
	/* Turn on direct for less than FIFOSize dword colour expansion */
    	pGlint->XAAScanlineColorExpandBuffers[0] = pGlint->IOBase+OutputFIFO+4;
	pGlint->ScanlineDirect = 1;
    	GLINT_WRITE_REG(((pGlint->dwords*h)-1)<<16 | 0x0D, OutputFIFO);
    	GLINT_WAIT(pGlint->dwords*h);
    } else {
	/* Use indirect for anything else */
    	pGlint->XAAScanlineColorExpandBuffers[0] = pGlint->ScratchBuffer;
	pGlint->ScanlineDirect   = 0;
    }

    pGlint->cpucount--;
}

static void
PermediaSubsequentColorExpandScanline(ScrnInfoPtr pScrn, int bufno)
{
    GLINTPtr pGlint = GLINTPTR(pScrn);
    CARD32 *srcp = (CARD32*)pGlint->XAAScanlineColorExpandBuffers[bufno];
    int dwords = pGlint->dwords;

    if (!pGlint->ScanlineDirect) {
	while(dwords >= pGlint->FIFOSize) {
	    GLINT_WAIT(pGlint->FIFOSize);
            GLINT_WRITE_REG(((pGlint->FIFOSize - 2) << 16) | 0x0D, OutputFIFO);
	    GLINT_MoveDWORDS(
			(CARD32*)((char*)pGlint->IOBase + OutputFIFO + 4),
	 		(CARD32*)srcp, pGlint->FIFOSize - 1);
	    dwords -= pGlint->FIFOSize - 1;
	    srcp += pGlint->FIFOSize - 1;
	}
	if(dwords) {
	    GLINT_WAIT(dwords + 1);
            GLINT_WRITE_REG(((dwords - 1) << 16) | 0x0D, OutputFIFO);
	    GLINT_MoveDWORDS(
			(CARD32*)((char*)pGlint->IOBase + OutputFIFO + 4),
	 		(CARD32*)srcp, dwords);
	}
    }
}


static void
PermediaWritePixmap8bpp(
    ScrnInfoPtr pScrn,
    int x, int y, int w, int h,
    unsigned char *src,
    int srcwidth,
    int rop,
    unsigned int planemask,
    int transparency_color,
    int bpp, int depth
)
{
    XAAInfoRecPtr infoRec = GET_XAAINFORECPTR_FROM_SCRNINFOPTR(pScrn);
    GLINTPtr pGlint = GLINTPTR(pScrn);
    int skipleft, dwords, count;
    CARD32* srcp;
    unsigned char *srcpbyte;
    Bool FastTexLoad = FALSE;

    GLINT_WAIT(2);
    DO_PLANEMASK(planemask);
    GLINT_WRITE_REG(0, RasterizerMode);

    dwords = (w + 3) >> 2;
    if((!(x&3)) && (!(w&3))) FastTexLoad = TRUE;	
    if((rop != GXcopy) || (planemask != ~0))
	FastTexLoad = FALSE;

#if 0
    if (rop == GXcopy) {
	skipleft = 0;
    } else {
	if((skipleft = (long)src & 0x03)) {
	    	skipleft /= (bpp>>3);

	    x -= skipleft;	     
	    w += skipleft;
	
	       src = (unsigned char*)((long)src & ~0x03); 
	}
    }
#endif
    skipleft = 0;
	
        if(FastTexLoad) {
	  int address;

	  GLINT_WAIT(1);
	  GLINT_WRITE_REG(UNIT_DISABLE, FBWriteMode);
	  PermediaSync(pScrn);	/* we are not using the rasterizer */
	  while(h--) {
	      count = dwords;
	      address = ((y * pScrn->displayWidth) + x) >> 2;
	      srcp = (CARD32*)src;
	      GLINT_WAIT(1);
	      GLINT_WRITE_REG(address, TextureDownloadOffset);
	      while(count >= infoRec->ColorExpandRange) {
    	    	GLINT_WAIT(infoRec->ColorExpandRange);
		/* (0x11 << 4) | 0x0D is the TAG for TextureData */
        	GLINT_WRITE_REG(((infoRec->ColorExpandRange - 2) << 16)
					| (0x11 << 4) |	0x0D, OutputFIFO);
		GLINT_MoveDWORDS(
			(CARD32*)((char*)pGlint->IOBase + OutputFIFO + 4),
	 		(CARD32*)srcp, infoRec->ColorExpandRange - 1);
		count -= infoRec->ColorExpandRange - 1;
		address += infoRec->ColorExpandRange - 1;
		srcp += infoRec->ColorExpandRange - 1;
	      }
	      if(count) {
    	    	GLINT_WAIT(count + 1);
		/* (0x11 << 4) | 0x0D is the TAG for TextureData */
        	GLINT_WRITE_REG(((count - 1) << 16) | (0x11 << 4) | 0x0D,
					 OutputFIFO);
		GLINT_MoveDWORDS(
			(CARD32*)((char*)pGlint->IOBase + OutputFIFO + 4),
	 		(CARD32*)srcp, count);
	      }
	      src += srcwidth;
	      y++;
	  }
	  GLINT_WAIT(1);
	  GLINT_WRITE_REG(UNIT_ENABLE, FBWriteMode);
	} else {
	   GLINT_WAIT(10);
	   if (rop == GXcopy) {
	     GLINT_WRITE_REG(pGlint->pprod, FBReadMode);
	   } else {
	     GLINT_WRITE_REG(pGlint->pprod | FBRM_DstEnable, FBReadMode);
	   }
           PermediaLoadCoord(pScrn, x<<16, y<<16, (x+w)<<16, h, 0, 1<<16);
	   GLINT_WRITE_REG(UNIT_DISABLE, ColorDDAMode);
	   LOADROP(rop);
  	   GLINT_WRITE_REG(PrimitiveTrapezoid | SyncOnHostData, Render);

	   {
	    while(h--) {
	      count = w;
	      srcpbyte = (unsigned char *)src;
	      while(count >= infoRec->ColorExpandRange) {
    	    	GLINT_WAIT(infoRec->ColorExpandRange);
		/* (0x15 << 4) | 0x05 is the TAG for FBSourceData */
        	GLINT_WRITE_REG(((infoRec->ColorExpandRange - 2) << 16) | 
				(0x15 << 4) | 0x05, OutputFIFO);
		GLINT_MoveBYTE(
			(CARD32*)((char*)pGlint->IOBase + OutputFIFO + 4),
	 		(unsigned char *)srcpbyte, infoRec->ColorExpandRange-1);
		count -= infoRec->ColorExpandRange - 1;
		srcpbyte += infoRec->ColorExpandRange - 1;
	      }
	      if(count) {
    	    	GLINT_WAIT(count + 1);
		/* (0x15 << 4) | 0x05 is the TAG for FBSourceData */
        	GLINT_WRITE_REG(((count - 1) << 16) | (0x15 << 4) | 
					0x05, OutputFIFO);
		GLINT_MoveBYTE(
			(CARD32*)((char*)pGlint->IOBase + OutputFIFO + 4),
	 		(unsigned char *)srcpbyte, count);
	      }
	      src += srcwidth;
	    }  
	   }
	}

    SET_SYNC_FLAG(infoRec);
}

static void
PermediaWritePixmap16bpp(
    ScrnInfoPtr pScrn,
    int x, int y, int w, int h,
    unsigned char *src,
    int srcwidth,
    int rop,
    unsigned int planemask,
    int transparency_color,
    int bpp, int depth
)
{
    XAAInfoRecPtr infoRec = GET_XAAINFORECPTR_FROM_SCRNINFOPTR(pScrn);
    GLINTPtr pGlint = GLINTPTR(pScrn);
    int skipleft, dwords, count;
    CARD32* srcp;
    unsigned short* srcpword;
    Bool FastTexLoad;

    GLINT_WAIT(2);
    DO_PLANEMASK(planemask);
    GLINT_WRITE_REG(0, RasterizerMode);

	FastTexLoad = FALSE;
	dwords = (w + 1) >> 1;
	if((!(x&1)) && (!(w&1))) FastTexLoad = TRUE;
	if((rop != GXcopy) || (planemask != ~0))
		FastTexLoad = FALSE;

#if 0
	if (rop == GXcopy) {
	  skipleft = 0;
	} else {
	  if((skipleft = (long)src & 0x03L)) {
	    		skipleft /= (bpp>>3);

	    	x -= skipleft;	     
	    	w += skipleft;
	
	    	   src = (unsigned char*)((long)src & ~0x03L); 
	  }
	}
#endif
	skipleft = 0;
	
        if(FastTexLoad) {
	  int address;

	  GLINT_WAIT(1);
	  GLINT_WRITE_REG(UNIT_DISABLE, FBWriteMode);
	  PermediaSync(pScrn);	/* we are not using the rasterizer */
	  while(h--) {
	      count = dwords;
	      address = ((y * pScrn->displayWidth) + x) >> 1;
	      srcp = (CARD32*)src;
	      GLINT_WAIT(1);
	      GLINT_WRITE_REG(address, TextureDownloadOffset);
	      while(count >= infoRec->ColorExpandRange) {
    	    	GLINT_WAIT(infoRec->ColorExpandRange);
		/* (0x11 << 4) | 0x0D is the TAG for TextureData */
        	GLINT_WRITE_REG(((infoRec->ColorExpandRange - 2) << 16) |
					(0x11 << 4) | 0x0D, OutputFIFO);
		GLINT_MoveDWORDS(
			(CARD32*)((char*)pGlint->IOBase + OutputFIFO + 4),
	 		(CARD32*)srcp, infoRec->ColorExpandRange - 1);
		count -= infoRec->ColorExpandRange - 1;
		address += infoRec->ColorExpandRange - 1;
		srcp += infoRec->ColorExpandRange - 1;
	      }
	      if(count) {
    	    	GLINT_WAIT(count + 1);
		/* (0x11 << 4) | 0x0D is the TAG for TextureData */
        	GLINT_WRITE_REG(((count - 1) << 16) | (0x11 << 4) | 0x0D,
					 OutputFIFO);
		GLINT_MoveDWORDS(
			(CARD32*)((char*)pGlint->IOBase + OutputFIFO + 4),
	 		(CARD32*)srcp, count);
	      }
	      src += srcwidth;
	      y++;
	  }
	  GLINT_WAIT(1);
	  GLINT_WRITE_REG(UNIT_ENABLE, FBWriteMode);
	} else {
	   GLINT_WAIT(10);
	   if (rop == GXcopy) {
	     GLINT_WRITE_REG(pGlint->pprod, FBReadMode);
	   } else {
	     GLINT_WRITE_REG(pGlint->pprod | FBRM_DstEnable, FBReadMode);
	   }
           PermediaLoadCoord(pScrn, x<<16, y<<16, (x+w)<<16, h, 0, 1<<16);
	   GLINT_WRITE_REG(UNIT_DISABLE, ColorDDAMode);
	   LOADROP(rop);
  	   GLINT_WRITE_REG(PrimitiveTrapezoid | SyncOnHostData, Render);

	   {
	    while(h--) {
	      count = w;
	      srcpword = (unsigned short *)src;
	      while(count >= infoRec->ColorExpandRange) {
    	    	GLINT_WAIT(infoRec->ColorExpandRange);
		/* (0x15 << 4) | 0x05 is the TAG for FBSourceData */
        	GLINT_WRITE_REG(((infoRec->ColorExpandRange - 2) << 16) | 
				(0x15 << 4) | 0x05, OutputFIFO);
		GLINT_MoveWORDS(
			(CARD32*)((char*)pGlint->IOBase + OutputFIFO + 4),
	 		(unsigned short *)srcpword,infoRec->ColorExpandRange-1);
		count -= infoRec->ColorExpandRange - 1;
		srcpword += infoRec->ColorExpandRange - 1;
	      }
	      if(count) {
    	    	GLINT_WAIT(count + 1);
		/* (0x15 << 4) | 0x05 is the TAG for FBSourceData */
        	GLINT_WRITE_REG(((count - 1) << 16) | (0x15 << 4) | 
					0x05, OutputFIFO);
		GLINT_MoveWORDS(
			(CARD32*)((char*)pGlint->IOBase + OutputFIFO + 4),
	 		(unsigned short *)srcpword, count);
	      }
	      src += srcwidth;
	    }  
	   }
	}

    SET_SYNC_FLAG(infoRec);
}

static void
PermediaWritePixmap32bpp(
    ScrnInfoPtr pScrn,
    int x, int y, int w, int h,
    unsigned char *src,
    int srcwidth,
    int rop,
    unsigned int planemask,
    int transparency_color,
    int bpp, int depth
)
{
    XAAInfoRecPtr infoRec = GET_XAAINFORECPTR_FROM_SCRNINFOPTR(pScrn);
    GLINTPtr pGlint = GLINTPTR(pScrn);
    int skipleft, dwords, count;
    CARD32* srcp;
    Bool FastTexLoad;

    GLINT_WAIT(3);
    DO_PLANEMASK(planemask);
    GLINT_WRITE_REG(0, RasterizerMode);
    if (rop == GXcopy) {      
	GLINT_WRITE_REG(pGlint->pprod, FBReadMode);
    } else {
	GLINT_WRITE_REG(pGlint->pprod | FBRM_DstEnable, FBReadMode);
    }

	FastTexLoad = TRUE;
	dwords = w;
	if((rop != GXcopy) || (planemask != ~0))
		FastTexLoad = FALSE;
	
#if 0
	if (!FastTexLoad) {
	  if((skipleft = (long)src & 0x03L)) {
	    		skipleft /= (bpp>>3);

	    	x -= skipleft;	     
	    	w += skipleft;
	
	    	   src = (unsigned char*)((long)src & ~0x03L); 
	  }
	}
#endif
	skipleft = 0;
	
        if(FastTexLoad) {
	  int address;

	  GLINT_WAIT(1);
	  GLINT_WRITE_REG(UNIT_DISABLE, FBWriteMode);
	  PermediaSync(pScrn);	/* we are not using the rasterizer */
	  while(h--) {
	      count = dwords;
	      address = (y * pScrn->displayWidth) + x;
	      srcp = (CARD32*)src;
	      GLINT_WAIT(1);
	      GLINT_WRITE_REG(address, TextureDownloadOffset);
	      while(count >= infoRec->ColorExpandRange) {
    	    	GLINT_WAIT(infoRec->ColorExpandRange);
		/* (0x11 << 4) | 0x0D is the TAG for TextureData */
        	GLINT_WRITE_REG(((infoRec->ColorExpandRange - 2) << 16) |
					(0x11 << 4) | 0x0D, OutputFIFO);
		GLINT_MoveDWORDS(
			(CARD32*)((char*)pGlint->IOBase + OutputFIFO + 4),
	 		(CARD32*)srcp, infoRec->ColorExpandRange - 1);
		count -= infoRec->ColorExpandRange - 1;
		address += infoRec->ColorExpandRange - 1;
		srcp += infoRec->ColorExpandRange - 1;
	      }
	      if(count) {
    	    	GLINT_WAIT(count + 1);
		/* (0x11 << 4) | 0x0D is the TAG for TextureData */
        	GLINT_WRITE_REG(((count - 1) << 16) | (0x11 << 4) | 0x0D,
					 OutputFIFO);
		GLINT_MoveDWORDS(
			(CARD32*)((char*)pGlint->IOBase + OutputFIFO + 4),
	 		(CARD32*)srcp, count);
	      }
	      src += srcwidth;
	      y++;
	  }
	  GLINT_WAIT(1);
	  GLINT_WRITE_REG(UNIT_ENABLE, FBWriteMode);
	} else {
	   GLINT_WAIT(9);
           PermediaLoadCoord(pScrn, (x&0xFFFF)<<16, y<<16, ((x&0xFFFF)+w)<<16, h, 0, 1<<16);
	   LOADROP(rop);
	   GLINT_WRITE_REG(UNIT_DISABLE, ColorDDAMode);
  	   GLINT_WRITE_REG(PrimitiveTrapezoid | SyncOnHostData, Render);

	   while(h--) {
	      count = dwords;
	      srcp = (CARD32*)src;
	      while(count >= infoRec->ColorExpandRange) {
    	    	GLINT_WAIT(infoRec->ColorExpandRange);
		/* (0x15 << 4) | 0x05 is the TAG for FBSourceData */
        	GLINT_WRITE_REG(((infoRec->ColorExpandRange - 2) << 16) |
				(0x15 << 4) | 0x05, OutputFIFO);
		GLINT_MoveDWORDS(
			(CARD32*)((char*)pGlint->IOBase + OutputFIFO + 4),
	 		(CARD32*)srcp, infoRec->ColorExpandRange - 1);
		count -= infoRec->ColorExpandRange - 1;
		srcp += infoRec->ColorExpandRange - 1;
	      }
	      if(count) {
    	    	GLINT_WAIT(count + 1);
		/* (0x15 << 4) | 0x05 is the TAG for FBSourceData */
        	GLINT_WRITE_REG(((count - 1) << 16) | (0x15 << 4) | 
					0x05, OutputFIFO);
		GLINT_MoveDWORDS(
			(CARD32*)((char*)pGlint->IOBase + OutputFIFO + 4),
	 		(CARD32*)srcp, count);
	      }
	      src += srcwidth;
	   }  
	}

    SET_SYNC_FLAG(infoRec);
}

static void 
PermediaPolylinesThinSolidWrapper(
   DrawablePtr     pDraw,
   GCPtr           pGC,
   int             mode,
   int             npt,
   DDXPointPtr     pPts
){
    XAAInfoRecPtr infoRec = GET_XAAINFORECPTR_FROM_GC(pGC);
    GLINTPtr pGlint = GLINTPTR(infoRec->pScrn);
    pGlint->CurrentGC = pGC;
    pGlint->CurrentDrawable = pDraw;
    if(infoRec->NeedToSync) (*infoRec->Sync)(infoRec->pScrn);
    XAAPolyLines(pDraw, pGC, mode, npt, pPts);
}

static void 
PermediaPolySegmentThinSolidWrapper(
   DrawablePtr     pDraw,
   GCPtr           pGC,
   int             nseg,
   xSegment        *pSeg
){
    XAAInfoRecPtr infoRec = GET_XAAINFORECPTR_FROM_GC(pGC);
    GLINTPtr pGlint = GLINTPTR(infoRec->pScrn);
    pGlint->CurrentGC = pGC;
    pGlint->CurrentDrawable = pDraw;
    if(infoRec->NeedToSync) (*infoRec->Sync)(infoRec->pScrn);
    XAAPolySegment(pDraw, pGC, nseg, pSeg);
}

static void
PermediaSetupForSolidLine(ScrnInfoPtr pScrn, int color,
					 int rop, unsigned int planemask)
{
    GLINTPtr pGlint = GLINTPTR(pScrn);

    GLINT_WAIT(6);
    DO_PLANEMASK(planemask);
    GLINT_WRITE_REG(UNIT_DISABLE, ColorDDAMode);
    GLINT_WRITE_REG(color, GLINTColor);
    GLINT_WRITE_REG(0, RasterizerMode);
    if (rop == GXcopy) {
  	GLINT_WRITE_REG(pGlint->pprod, FBReadMode);
    } else {
  	GLINT_WRITE_REG(pGlint->pprod | FBRM_DstEnable, FBReadMode);
    }
    LOADROP(rop);
}

static void
PermediaSubsequentHorVertLine(ScrnInfoPtr pScrn,int x,int y,int len,int dir)
{
    GLINTPtr pGlint = GLINTPTR(pScrn);
  
    GLINT_WAIT(7);
    if (dir == DEGREES_0) {
        PermediaLoadCoord(pScrn, x<<16, y<<16, 0, len, 1<<16, 0);
    } else {
        PermediaLoadCoord(pScrn, x<<16, y<<16, 0, len, 0, 1<<16);
    }

    GLINT_WRITE_REG(PrimitiveLine, Render);
}

static void 
PermediaSubsequentSolidBresenhamLine( ScrnInfoPtr pScrn,
        int x, int y, int dmaj, int dmin, int e, int len, int octant)
{
    GLINTPtr pGlint = GLINTPTR(pScrn);
    int dxdom, dy;

    if(dmaj == dmin) {
	GLINT_WAIT(7);
	if(octant & YDECREASING) {
	    dy = -1<<16;
	} else {
	    dy = 1<<16;
	}

	if(octant & XDECREASING) {
	    dxdom = -1<<16;
	} else {
	    dxdom = 1<<16;
	}

        PermediaLoadCoord(pScrn, x<<16, y<<16, 0, len, dxdom, dy);
	GLINT_WRITE_REG(PrimitiveLine, Render);
	return;
    }

    fbBres(pGlint->CurrentDrawable, pGlint->CurrentGC, 0,
                (octant & XDECREASING) ? -1 : 1, 
                (octant & YDECREASING) ? -1 : 1, 
                (octant & YMAJOR) ? Y_AXIS : X_AXIS,
                x, y, dmin + e, dmin, -dmaj, len);
}

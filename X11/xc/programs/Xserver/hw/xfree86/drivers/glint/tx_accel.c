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
 * GLINT 500TX / MX accelerated options.
 */
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/glint/tx_accel.c,v 1.28 2001/05/30 10:07:56 alanh Exp $ */

#include "xf86.h"
#include "xf86_OSproc.h"
#include "xf86_ansic.h"

#include "xf86PciInfo.h"
#include "xf86Pci.h"

#include "fb.h"

#include "miline.h"

#include "glint_regs.h"
#include "glint.h"

#include "xaalocal.h"	/* For replacements */

static void TXSync(ScrnInfoPtr pScrn);
static void DualTXSync(ScrnInfoPtr pScrn);
static void TXSetupForFillRectSolid(ScrnInfoPtr pScrn, int color, int rop,
						unsigned int planemask);
static void TXSubsequentFillRectSolid(ScrnInfoPtr pScrn, int x, int y,
						int w, int h);
static void TXSetupForMono8x8PatternFill(ScrnInfoPtr pScrn, int patternx, 
						int patterny, 
					   	int fg, int bg, int rop,
					   	unsigned int planemask);
static void TXSubsequentMono8x8PatternFillRect(ScrnInfoPtr pScrn, int patternx,
						int patterny, int x, int y,
				   		int w, int h);
static void TXSetupForScreenToScreenCopy(ScrnInfoPtr pScrn, int xdir, int ydir,
						int rop, unsigned int planemask,
				    		int transparency_color);
static void TXSubsequentScreenToScreenCopy(ScrnInfoPtr pScrn, int x1, int y1,
						int x2, int y2, int w, int h);
static void TXWriteBitmap(ScrnInfoPtr pScrn, int x, int y, int w, int h,
    				unsigned char *src, int srcwidth,
				int skipleft, int fg, int bg, int rop,
    				unsigned int planemask);
static void TXSetClippingRectangle(ScrnInfoPtr pScrn, int x1, int y1, 
						int x2,int y2);
static void TXDisableClipping(ScrnInfoPtr pScrn);
static void TXWritePixmap(ScrnInfoPtr pScrn, int x, int y, int w, int h,
   				unsigned char *src, int srcwidth, int rop,
   				unsigned int planemask, int trans,
   				int bpp, int depth);
static void TXSetupForScanlineCPUToScreenColorExpandFill(ScrnInfoPtr pScrn, int fg,
				int bg, int rop, unsigned int planemask);
static void TXSubsequentScanlineCPUToScreenColorExpandFill(ScrnInfoPtr pScrn, int x,
				int y, int w, int h, int skipleft);
static void TXSubsequentColorExpandScanline(ScrnInfoPtr pScrn, int bufno);
static void TXLoadCoord(ScrnInfoPtr pScrn, int x, int y, int w, int h,
				int a, int d);
static void TXSetupForSolidLine(ScrnInfoPtr pScrn, int color, int rop,
				unsigned int planemask);
static void TXSubsequentHorVertLine(ScrnInfoPtr pScrn, int x1, int y1,
				int len, int dir);
static void TXSubsequentSolidBresenhamLine(ScrnInfoPtr pScrn,
        			int x, int y, int dmaj, int dmin, int e, 
				int len, int octant);
static void TXPolylinesThinSolidWrapper(DrawablePtr pDraw, GCPtr pGC,
   				int mode, int npt, DDXPointPtr pPts);
static void TXPolySegmentThinSolidWrapper(DrawablePtr pDraw, GCPtr pGC,
 				int nseg, xSegment *pSeg);

void
TXInitializeEngine(ScrnInfoPtr pScrn)
{
    GLINTPtr pGlint = GLINTPTR(pScrn);
    /* Initialize the Accelerator Engine to defaults */
    pGlint->rasterizerMode = UNIT_DISABLE;

    if (pGlint->MultiAperture) {
    	pGlint->rasterizerMode = RMMultiGLINT;

    	/* Only write the following register to the first chip */
    	GLINT_SLOW_WRITE_REG(1, BroadcastMask);
    	GLINT_SLOW_WRITE_REG(0x00000001,    ScanLineOwnership);

    	/* Only write the following register to the second chip */
    	GLINT_SLOW_WRITE_REG(2, BroadcastMask);
    	GLINT_SLOW_WRITE_REG(0x00000005,    ScanLineOwnership);

    	/* Make sure the rest of the register writes go to both chip's */
    	GLINT_SLOW_WRITE_REG(3, BroadcastMask);

    	pGlint->pprod |= FBRM_ScanlineInt2;
    }

    GLINT_SLOW_WRITE_REG(pGlint->pprod, LBReadMode);
    GLINT_SLOW_WRITE_REG(pGlint->rasterizerMode, RasterizerMode);
    GLINT_SLOW_WRITE_REG(UNIT_DISABLE, ScissorMode);
    GLINT_SLOW_WRITE_REG(pGlint->pprod,	FBReadMode);
    GLINT_SLOW_WRITE_REG(0, dXSub);
    GLINT_SLOW_WRITE_REG(UNIT_DISABLE,	LBWriteMode);
    GLINT_SLOW_WRITE_REG(UNIT_ENABLE,	FBWriteMode);
    GLINT_SLOW_WRITE_REG(UNIT_DISABLE,	DitherMode);
    GLINT_SLOW_WRITE_REG(UNIT_DISABLE,	AlphaBlendMode);
    GLINT_SLOW_WRITE_REG(UNIT_DISABLE,	ColorDDAMode);
    GLINT_SLOW_WRITE_REG(UNIT_DISABLE,	TextureColorMode);
    GLINT_SLOW_WRITE_REG(UNIT_DISABLE,	TextureAddressMode);
    GLINT_SLOW_WRITE_REG(UNIT_DISABLE,  TextureReadMode);
    GLINT_SLOW_WRITE_REG(UNIT_DISABLE,  GLINTWindow);
    GLINT_SLOW_WRITE_REG(UNIT_DISABLE,  AlphaBlendMode);
    GLINT_SLOW_WRITE_REG(UNIT_DISABLE,  DepthMode);
    GLINT_SLOW_WRITE_REG(UNIT_DISABLE,  RouterMode);
    GLINT_SLOW_WRITE_REG(UNIT_DISABLE,	FogMode);
    GLINT_SLOW_WRITE_REG(UNIT_DISABLE,	AntialiasMode);
    GLINT_SLOW_WRITE_REG(UNIT_DISABLE,	AlphaTestMode);
    GLINT_SLOW_WRITE_REG(UNIT_DISABLE,	StencilMode);
    GLINT_SLOW_WRITE_REG(UNIT_DISABLE,	AreaStippleMode);
    GLINT_SLOW_WRITE_REG(UNIT_DISABLE,	LineStippleMode);
    GLINT_SLOW_WRITE_REG(0,		UpdateLineStippleCounters);
    GLINT_SLOW_WRITE_REG(UNIT_DISABLE,	LogicalOpMode);
    GLINT_SLOW_WRITE_REG(UNIT_DISABLE,	DepthMode);
    GLINT_SLOW_WRITE_REG(UNIT_DISABLE,	StatisticMode);
    GLINT_SLOW_WRITE_REG(0x400,		FilterMode);
    GLINT_SLOW_WRITE_REG(0xffffffff,	FBHardwareWriteMask);
    GLINT_SLOW_WRITE_REG(0xffffffff,	FBSoftwareWriteMask);
    GLINT_SLOW_WRITE_REG(UNIT_DISABLE,	GLINTDepth);
    GLINT_SLOW_WRITE_REG(UNIT_DISABLE,	FBSourceOffset);
    GLINT_SLOW_WRITE_REG(UNIT_DISABLE,	FBPixelOffset);
    GLINT_SLOW_WRITE_REG(UNIT_DISABLE,	LBSourceOffset);
    GLINT_SLOW_WRITE_REG(UNIT_DISABLE,	WindowOrigin);
    GLINT_SLOW_WRITE_REG(UNIT_DISABLE,	FBWindowBase);
    GLINT_SLOW_WRITE_REG(UNIT_DISABLE,	LBWindowBase);
    GLINT_SLOW_WRITE_REG(UNIT_DISABLE,	TextureAddressMode);
    GLINT_SLOW_WRITE_REG(UNIT_DISABLE,	RouterMode);
    GLINT_SLOW_WRITE_REG(UNIT_DISABLE,	PatternRamMode);

    switch (pScrn->bitsPerPixel) {
    	case 8:
	    GLINT_SLOW_WRITE_REG(0x2,	PixelSize);
	    break;
	case 16:
	    GLINT_SLOW_WRITE_REG(0x1,	PixelSize);
	    break;
	case 32:
	    GLINT_SLOW_WRITE_REG(0x0,	PixelSize);
	    break;
    }
    pGlint->ROP = 0xFF;
    pGlint->ClippingOn = FALSE;
    pGlint->startxsub = 0;
    pGlint->startxdom = 0;
    pGlint->starty = 0;
    pGlint->count = 0;
    pGlint->dxdom = 0;
    pGlint->dy = 1;
    pGlint->planemask = 0;
    GLINT_SLOW_WRITE_REG(0, StartXSub);
    GLINT_SLOW_WRITE_REG(0, StartXDom);
    GLINT_SLOW_WRITE_REG(0, StartY);
    GLINT_SLOW_WRITE_REG(0, GLINTCount);
    GLINT_SLOW_WRITE_REG(0, dXDom);
    GLINT_SLOW_WRITE_REG(1<<16, dY);
}

Bool
TXAccelInit(ScreenPtr pScreen)
{
    XAAInfoRecPtr infoPtr;
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    GLINTPtr pGlint = GLINTPTR(pScrn);
    long memory = pGlint->FbMapSize;
    BoxRec AvailFBArea;

    pGlint->AccelInfoRec = infoPtr = XAACreateInfoRec();
    if (!infoPtr) return FALSE;

    TXInitializeEngine(pScrn);

    infoPtr->Flags = PIXMAP_CACHE |
		     LINEAR_FRAMEBUFFER |
		     OFFSCREEN_PIXMAPS;
 
    if (pGlint->MultiAperture)
    	infoPtr->Sync = DualTXSync;
    else
    	infoPtr->Sync = TXSync;

    infoPtr->SetClippingRectangle = TXSetClippingRectangle;
    infoPtr->DisableClipping = TXDisableClipping;
    infoPtr->ClippingFlags = HARDWARE_CLIP_MONO_8x8_FILL |
			     HARDWARE_CLIP_SCREEN_TO_SCREEN_COPY |
			     HARDWARE_CLIP_SOLID_FILL;

    infoPtr->SolidFillFlags = 0;
    infoPtr->SetupForSolidFill = TXSetupForFillRectSolid;
    infoPtr->SubsequentSolidFillRect = TXSubsequentFillRectSolid;

    infoPtr->SolidLineFlags = 0;
    infoPtr->PolySegmentThinSolidFlags = 0;
    infoPtr->PolylinesThinSolidFlags = 0;
    infoPtr->SetupForSolidLine = TXSetupForSolidLine;
    infoPtr->SubsequentSolidHorVertLine = TXSubsequentHorVertLine;
    if (!(pScrn->overlayFlags & OVERLAY_8_32_PLANAR))
    {
        infoPtr->SubsequentSolidBresenhamLine = 
					TXSubsequentSolidBresenhamLine;
    }
    infoPtr->PolySegmentThinSolid = TXPolySegmentThinSolidWrapper;
    infoPtr->PolylinesThinSolid = TXPolylinesThinSolidWrapper;

    if (!pGlint->MultiAperture) {
    	infoPtr->ScreenToScreenCopyFlags = NO_TRANSPARENCY |
					       ONLY_LEFT_TO_RIGHT_BITBLT;
    	infoPtr->SetupForScreenToScreenCopy = TXSetupForScreenToScreenCopy;
    	infoPtr->SubsequentScreenToScreenCopy = TXSubsequentScreenToScreenCopy;
    }

    infoPtr->Mono8x8PatternFillFlags = HARDWARE_PATTERN_PROGRAMMED_ORIGIN |
				       HARDWARE_PATTERN_SCREEN_ORIGIN |
				       HARDWARE_PATTERN_PROGRAMMED_BITS;
    infoPtr->SetupForMono8x8PatternFill = TXSetupForMono8x8PatternFill;
    infoPtr->SubsequentMono8x8PatternFillRect = TXSubsequentMono8x8PatternFillRect;

    infoPtr->ScanlineCPUToScreenColorExpandFillFlags = 
					       TRANSPARENCY_ONLY |
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
				TXSetupForScanlineCPUToScreenColorExpandFill;
    infoPtr->SubsequentScanlineCPUToScreenColorExpandFill = 
				TXSubsequentScanlineCPUToScreenColorExpandFill;
    infoPtr->SubsequentColorExpandScanline = 
				TXSubsequentColorExpandScanline;

    infoPtr->ColorExpandRange = pGlint->FIFOSize;

    infoPtr->WriteBitmap = TXWriteBitmap;
    infoPtr->WritePixmap = TXWritePixmap;

    AvailFBArea.x1 = 0;
    AvailFBArea.y1 = 0;
    AvailFBArea.x2 = pScrn->displayWidth;
    if (memory > (16383*1024)) memory = 16383*1024;
    AvailFBArea.y2 = memory / (pScrn->displayWidth * 
					  pScrn->bitsPerPixel / 8);

    if (AvailFBArea.y2 > 4095) AvailFBArea.y2 = 4095;

    xf86InitFBManager(pScreen, &AvailFBArea);

    return (XAAInit(pScreen, infoPtr));
}

static void TXLoadCoord(
	ScrnInfoPtr pScrn,
	int x, int y,
	int w, int h,
	int a, int d
){
    GLINTPtr pGlint = GLINTPTR(pScrn);
    
#ifndef XF86DRI
    if (w != pGlint->startxsub) {
    	GLINT_WRITE_REG(w<<16, StartXSub);
	pGlint->startxsub = w;
    }
    if (x != pGlint->startxdom) {
    	GLINT_WRITE_REG(x<<16,StartXDom);
	pGlint->startxdom = x;
    }
    if (y != pGlint->starty) {
    	GLINT_WRITE_REG(y<<16,StartY);
	pGlint->starty = y;
    }
    if (h != pGlint->count) {
    	GLINT_WRITE_REG(h,GLINTCount);
	pGlint->count = h;
    }
    if (a != pGlint->dxdom) {
    	GLINT_WRITE_REG(a<<16,dXDom);
	pGlint->dxdom = a;
    }
    if (d != pGlint->dy) {
    	GLINT_WRITE_REG(d<<16,dY);
	pGlint->dy = d;
    }
#else
    	GLINT_WRITE_REG(w<<16, StartXSub);
    	GLINT_WRITE_REG(x<<16,StartXDom);
    	GLINT_WRITE_REG(y<<16,StartY);
    	GLINT_WRITE_REG(h,GLINTCount);
    	GLINT_WRITE_REG(a<<16,dXDom);
    	GLINT_WRITE_REG(d<<16,dY);
	pGlint->startxsub = w;
	pGlint->startxdom = x;
	pGlint->starty = y;
	pGlint->count = h;
	pGlint->dxdom = a;
	pGlint->dy = d;
#endif
}

static void
TXSync(
	ScrnInfoPtr pScrn
){
    GLINTPtr pGlint = GLINTPTR(pScrn);
    CARD32 readValue;

    CHECKCLIPPING;

    while (GLINT_READ_REG(DMACount) != 0);
    GLINT_WAIT(2);
    GLINT_WRITE_REG(0x400, FilterMode);
    GLINT_WRITE_REG(0, GlintSync);
    do {
   	while(GLINT_READ_REG(OutFIFOWords) == 0);
	readValue = GLINT_READ_REG(OutputFIFO);
    } while (readValue != Sync_tag);
}

static void
DualTXSync(
	ScrnInfoPtr pScrn
){
    GLINTPtr pGlint = GLINTPTR(pScrn);
    unsigned long readValue;

    CHECKCLIPPING;

    while (GLINT_READ_REG(DMACount) != 0);
    GLINT_WAIT(3);
    
    /* hack! this shouldn't need to be reloaded */
    GLINT_WRITE_REG(3, BroadcastMask); 
    GLINT_WRITE_REG(0x400, FilterMode);
    GLINT_WRITE_REG(0, GlintSync);

    /* Read 1st MX until Sync Tag shows */
    ACCESSCHIP1();
    do {
   	while(GLINT_READ_REG(OutFIFOWords) == 0);
	readValue = GLINT_READ_REG(OutputFIFO);
    } while (readValue != Sync_tag);

    ACCESSCHIP2();
    /* Read 2nd MX until Sync Tag shows */
    do {
   	while(GLINT_READ_REG(OutFIFOWords) == 0);
	readValue = GLINT_READ_REG(OutputFIFO);
    } while (readValue != Sync_tag);

    ACCESSCHIP1();
}


static void
TXSetupForFillRectSolid(
	ScrnInfoPtr pScrn, 
	int color, int rop, 
	unsigned int planemask
){
    GLINTPtr pGlint = GLINTPTR(pScrn);
    pGlint->ForeGroundColor = color;
	
    GLINT_WAIT(5);
    REPLICATE(color);
    DO_PLANEMASK(planemask);
    if (rop == GXcopy) {
	GLINT_WRITE_REG(pGlint->pprod, FBReadMode);
	GLINT_WRITE_REG(UNIT_DISABLE, PatternRamMode);
	GLINT_WRITE_REG(color, FBBlockColor);
	pGlint->FrameBufferReadMode = FastFillEnable;
    } else {
	GLINT_WRITE_REG(pGlint->pprod | FBRM_DstEnable, FBReadMode);
	GLINT_WRITE_REG(UNIT_ENABLE, PatternRamMode);
	GLINT_WRITE_REG(color, PatternRamData0);
	pGlint->FrameBufferReadMode = FastFillEnable | SpanOperation;
    }
    LOADROP(rop);
}

static void
TXSubsequentFillRectSolid(
	ScrnInfoPtr pScrn, 
	int x, int y, 
	int w, int h
){
    GLINTPtr pGlint = GLINTPTR(pScrn);

    GLINT_WAIT(7);
    TXLoadCoord(pScrn, x, y, x+w, h, 0, 1);
    GLINT_WRITE_REG(PrimitiveTrapezoid | pGlint->FrameBufferReadMode,Render);
}

static void
TXSetClippingRectangle(
	ScrnInfoPtr pScrn, 	
	int x1, int y1, 
	int x2, int y2
){
    GLINTPtr pGlint = GLINTPTR(pScrn);

    GLINT_WAIT(3);
    GLINT_WRITE_REG((y1&0xFFFF)<<16|(x1&0xFFFF), ScissorMinXY);
    GLINT_WRITE_REG((y2&0xFFFF)<<16|(x2&0xFFFF), ScissorMaxXY);
    GLINT_WRITE_REG(1, ScissorMode); /* Enable Scissor Mode */
    pGlint->ClippingOn = TRUE;
}

static void
TXDisableClipping(
	ScrnInfoPtr pScrn
){
    GLINTPtr pGlint = GLINTPTR(pScrn);
    CHECKCLIPPING;
}

static void
TXSetupForScreenToScreenCopy(
	ScrnInfoPtr pScrn,
	int xdir, int  ydir, 	
	int rop,
	unsigned int planemask, 
	int transparency_color
){
    GLINTPtr pGlint = GLINTPTR(pScrn);
    
    pGlint->BltScanDirection = ydir;

    GLINT_WAIT(5);
    DO_PLANEMASK(planemask);
    GLINT_WRITE_REG(UNIT_DISABLE, PatternRamMode);

    if (rop == GXcopy) {
	GLINT_WRITE_REG(pGlint->pprod | FBRM_SrcEnable, FBReadMode);
    } else {
	GLINT_WRITE_REG(pGlint->pprod | FBRM_SrcEnable | FBRM_DstEnable, FBReadMode);
    }
    LOADROP(rop);
}

static void
TXSubsequentScreenToScreenCopy(
	ScrnInfoPtr pScrn, 
	int x1, int y1, 
	int x2, int y2,
	int w, int h
){
    GLINTPtr pGlint = GLINTPTR(pScrn);
    int srcaddr, dstaddr;

    GLINT_WAIT(8);
    if (pGlint->BltScanDirection != 1) {
	y1 += h - 1;
	y2 += h - 1;
        TXLoadCoord(pScrn, x2, y2, x2+w, h, 0, -1);
    } else {
        TXLoadCoord(pScrn, x2, y2, x2+w, h, 0, 1);
    }	

    srcaddr = y1 * pScrn->displayWidth + x1;
    dstaddr = y2 * pScrn->displayWidth + x2;

    GLINT_WRITE_REG(srcaddr - dstaddr, FBSourceOffset);
    GLINT_WRITE_REG(PrimitiveTrapezoid| FastFillEnable | SpanOperation, Render);
}

static void
TXSetupForScanlineCPUToScreenColorExpandFill(
	ScrnInfoPtr pScrn,
	int fg, int bg, 
	int rop, 
	unsigned int planemask
){
    GLINTPtr pGlint = GLINTPTR(pScrn);
    REPLICATE(fg);
    REPLICATE(bg);
    GLINT_WAIT(6);
    DO_PLANEMASK(planemask);
    GLINT_WRITE_REG(pGlint->rasterizerMode, RasterizerMode);
    if (rop == GXcopy) {
        GLINT_WRITE_REG(pGlint->pprod, FBReadMode);
        GLINT_WRITE_REG(UNIT_DISABLE, PatternRamMode);
        pGlint->FrameBufferReadMode = FastFillEnable;
	GLINT_WRITE_REG(fg, FBBlockColor);
    } else {
        GLINT_WRITE_REG(pGlint->pprod | FBRM_DstEnable, FBReadMode);
        GLINT_WRITE_REG(UNIT_ENABLE, PatternRamMode);
        pGlint->FrameBufferReadMode = FastFillEnable | SpanOperation;
	GLINT_WRITE_REG(fg, PatternRamData0);
    }
    LOADROP(rop);
}

static void
TXSubsequentScanlineCPUToScreenColorExpandFill(
	ScrnInfoPtr pScrn,
	int x, int y, int w, int h,
	int skipleft
){
    GLINTPtr pGlint = GLINTPTR(pScrn);

    pGlint->dwords = ((w + 31) >> 5); /* dwords per scanline */

    pGlint->cpucount = h;

    GLINT_WAIT(8);
    TXLoadCoord(pScrn, x, y, (x+w), h, 0, 1);
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
TXSubsequentColorExpandScanline(ScrnInfoPtr pScrn, int bufno)
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

void TXSetupForMono8x8PatternFill(
	ScrnInfoPtr pScrn,
	int patternx, int patterny, 
	int fg, int bg, int rop,
	unsigned int planemask
){
    GLINTPtr pGlint = GLINTPTR(pScrn);

    if (bg == -1) pGlint->FrameBufferReadMode = -1;
	else    pGlint->FrameBufferReadMode = 0;
    pGlint->ForeGroundColor = fg;
    pGlint->BackGroundColor = bg;
    REPLICATE(pGlint->ForeGroundColor);
    REPLICATE(pGlint->BackGroundColor);

    GLINT_WAIT(13);
    DO_PLANEMASK(planemask);
    GLINT_WRITE_REG((patternx & 0x000000FF),       AreaStipplePattern0);
    GLINT_WRITE_REG((patternx & 0x0000FF00) >> 8,  AreaStipplePattern1);
    GLINT_WRITE_REG((patternx & 0x00FF0000) >> 16, AreaStipplePattern2);
    GLINT_WRITE_REG((patternx & 0xFF000000) >> 24, AreaStipplePattern3);
    GLINT_WRITE_REG((patterny & 0x000000FF),       AreaStipplePattern4);
    GLINT_WRITE_REG((patterny & 0x0000FF00) >> 8,  AreaStipplePattern5);
    GLINT_WRITE_REG((patterny & 0x00FF0000) >> 16, AreaStipplePattern6);
    GLINT_WRITE_REG((patterny & 0xFF000000) >> 24, AreaStipplePattern7);
  
    if (rop == GXcopy) {
	GLINT_WRITE_REG(UNIT_DISABLE, PatternRamMode);
	GLINT_WRITE_REG(pGlint->pprod, FBReadMode);
    } else {
	GLINT_WRITE_REG(UNIT_ENABLE, PatternRamMode);
	GLINT_WRITE_REG(pGlint->pprod | FBRM_DstEnable, FBReadMode);
    }
    LOADROP(rop);
}

static void 
TXSubsequentMono8x8PatternFillRect(
	ScrnInfoPtr pScrn, 
	int patternx, int patterny, 
	int x, int y,
	int w, int h
){
    GLINTPtr pGlint = GLINTPTR(pScrn);
    int span = 0;
  
    GLINT_WAIT(12);
    TXLoadCoord(pScrn, x, y, x+w, h, 0, 1);

    if (pGlint->FrameBufferReadMode != -1) {
	if (pGlint->ROP == GXcopy) {
	  GLINT_WRITE_REG(pGlint->BackGroundColor, FBBlockColor);
	  span = 0;
	} else {
  	  GLINT_WRITE_REG(pGlint->BackGroundColor, PatternRamData0);
	  span = SpanOperation;
	}
	GLINT_WRITE_REG(2<<1|2<<4|patternx<<7|patterny<<12|ASM_InvertPattern |
					UNIT_ENABLE, AreaStippleMode);
	GLINT_WRITE_REG(AreaStippleEnable | span | FastFillEnable | 
						PrimitiveTrapezoid, Render);
    }

    if (pGlint->ROP == GXcopy) {
	GLINT_WRITE_REG(pGlint->ForeGroundColor, FBBlockColor);
	span = 0;
    } else {
  	GLINT_WRITE_REG(pGlint->ForeGroundColor, PatternRamData0);
	span = SpanOperation;
    }
    GLINT_WRITE_REG(2<<1|2<<4|patternx<<7|patterny<<12|
  						UNIT_ENABLE, AreaStippleMode);
    GLINT_WRITE_REG(AreaStippleEnable | span | FastFillEnable | 
						PrimitiveTrapezoid, Render);
}

static void 
TXWriteBitmap(ScrnInfoPtr pScrn,
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
    int dwords, height, mode;
    Bool SecondPass = FALSE;
    register int count;
    register CARD32* pattern;

    w += skipleft;
    x -= skipleft;
    dwords = (w + 31) >> 5;

    TXSetClippingRectangle(pScrn,x+skipleft, y, x+w, y+h);

    GLINT_WAIT(11);
    DO_PLANEMASK(planemask);
    GLINT_WRITE_REG(pGlint->rasterizerMode, RasterizerMode);
    LOADROP(rop);
    if (rop == GXcopy) {
	mode = 0;
	GLINT_WRITE_REG(UNIT_DISABLE, PatternRamMode);
	GLINT_WRITE_REG(pGlint->pprod, FBReadMode);
    } else {
	mode = SpanOperation;
	GLINT_WRITE_REG(UNIT_ENABLE, PatternRamMode);
	GLINT_WRITE_REG(pGlint->pprod | FBRM_DstEnable, FBReadMode);
    }
    TXLoadCoord(pScrn, x, y, x+w, h, 0, 1);

    if(bg == -1) {
	REPLICATE(fg);
	GLINT_WAIT(3);
	if (rop == GXcopy) {
	    GLINT_WRITE_REG(fg, FBBlockColor);
	} else {
	    GLINT_WRITE_REG(fg, PatternRamData0);
	}
    } else if(rop == GXcopy) {
	REPLICATE(bg);
	GLINT_WAIT(5);
	if (rop == GXcopy) {
	    GLINT_WRITE_REG(bg, FBBlockColor);
	} else {
	    GLINT_WRITE_REG(bg, PatternRamData0);
	}
	GLINT_WRITE_REG(PrimitiveTrapezoid |mode|FastFillEnable,Render);
	REPLICATE(fg);
	if (rop == GXcopy) {
	    GLINT_WRITE_REG(fg, FBBlockColor);
	} else {
	    GLINT_WRITE_REG(fg, PatternRamData0);
	}
    } else {
	SecondPass = TRUE;
	REPLICATE(fg);
	GLINT_WAIT(3);
	if (rop == GXcopy) {
	    GLINT_WRITE_REG(fg, FBBlockColor);
	} else {
	    GLINT_WRITE_REG(fg, PatternRamData0);
	}
    }

SECOND_PASS:
    GLINT_WRITE_REG(PrimitiveTrapezoid | FastFillEnable | mode | SyncOnBitMask, Render);
    
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

    if(SecondPass) {
	SecondPass = FALSE;
	REPLICATE(bg);
	GLINT_WAIT(4);
	GLINT_WRITE_REG(InvertBitMask | pGlint->rasterizerMode, RasterizerMode);
	if (rop == GXcopy) {
	    GLINT_WRITE_REG(bg, FBBlockColor);
	} else {
	    GLINT_WRITE_REG(bg, PatternRamData0);
	}
	goto SECOND_PASS;
    }

    GLINT_WAIT(2);
    GLINT_WRITE_REG(pGlint->rasterizerMode, RasterizerMode);
    CHECKCLIPPING;
    SET_SYNC_FLAG(infoRec);
}

static void 
TXWritePixmap(
   ScrnInfoPtr pScrn,
   int x, int y, int w, int h,
   unsigned char *src,	
   int srcwidth,	/* bytes */
   int rop,
   unsigned int planemask,
   int trans,
   int bpp, int depth
){
    XAAInfoRecPtr infoRec = GET_XAAINFORECPTR_FROM_SCRNINFOPTR(pScrn);
    GLINTPtr pGlint = GLINTPTR(pScrn);
    CARD32 *srcp;
    int count,dwords, skipleft, Bpp = bpp >> 3; 

    if((skipleft = (long)src & 0x03L)) {
	skipleft /= Bpp;

	x -= skipleft;	     
	w += skipleft;
	
	src = (unsigned char*)((long)src & ~0x03L);     
    }

    switch(Bpp) {
    case 1:	dwords = (w + 3) >> 2;
		break;
    case 2:	dwords = (w + 1) >> 1;
		break;
    case 4:	dwords = w;
		break;
    default: return; 
    }

    TXSetClippingRectangle(pScrn,x+skipleft, y, x+w, y+h);

    GLINT_WAIT(12);
    DO_PLANEMASK(planemask);
    GLINT_WRITE_REG(0, PatternRamMode);
    if (rop == GXcopy) {
        GLINT_WRITE_REG(pGlint->pprod, FBReadMode);
    } else {
        GLINT_WRITE_REG(pGlint->pprod | FBRM_DstEnable, FBReadMode);
    }
    LOADROP(rop);
    TXLoadCoord(pScrn, x, y, x+w, h, 0, 1);
    GLINT_WRITE_REG(PrimitiveTrapezoid | FastFillEnable | SpanOperation | 
						SyncOnHostData, Render);

    while(h--) {
      count = dwords;
      srcp = (CARD32*)src;
      while(count >= infoRec->ColorExpandRange) {
	GLINT_WAIT(infoRec->ColorExpandRange);
	/* (0x15 << 4) | 0x05 is the TAG for FBSourceData */
       	GLINT_WRITE_REG(((infoRec->ColorExpandRange - 2) << 16) | (0x15 << 4) | 
				0x05, OutputFIFO);
	GLINT_MoveDWORDS(
		(CARD32*)((char*)pGlint->IOBase + OutputFIFO + 4),
 		(CARD32*)srcp, infoRec->ColorExpandRange - 1);
	count -= infoRec->ColorExpandRange - 1;
	srcp += infoRec->ColorExpandRange - 1;
      }
      if(count) {
	GLINT_WAIT(count);
	/* (0x15 << 4) | 0x05 is the TAG for FBSourceData */
       	GLINT_WRITE_REG(((count - 1) << 16) | (0x15 << 4) | 
				0x05, OutputFIFO);
	GLINT_MoveDWORDS(
		(CARD32*)((char*)pGlint->IOBase + OutputFIFO + 4),
 		(CARD32*)srcp, count);
      }
      src += srcwidth;
    }  
    CHECKCLIPPING;
    SET_SYNC_FLAG(infoRec);
}

static void 
TXPolylinesThinSolidWrapper(
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
TXPolySegmentThinSolidWrapper(
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
TXSetupForSolidLine(ScrnInfoPtr pScrn, int color,
					 int rop, unsigned int planemask)
{
    GLINTPtr pGlint = GLINTPTR(pScrn);

    GLINT_WAIT(5);
    DO_PLANEMASK(planemask);
    GLINT_WRITE_REG(color, GLINTColor);
    if (rop == GXcopy) {
  	GLINT_WRITE_REG(pGlint->pprod, FBReadMode);
    } else {
  	GLINT_WRITE_REG(pGlint->pprod | FBRM_DstEnable, FBReadMode);
    }
    LOADROP(rop);
}

static void
TXSubsequentHorVertLine(ScrnInfoPtr pScrn,int x,int y,int len,int dir)
{
    GLINTPtr pGlint = GLINTPTR(pScrn);
  
    GLINT_WAIT(7);
    if (dir == DEGREES_0) {
        TXLoadCoord(pScrn, x, y, 0, len, 1, 0);
    } else {
        TXLoadCoord(pScrn, x, y, 0, len, 0, 1);
    }

    GLINT_WRITE_REG(PrimitiveLine, Render);
}

static void 
TXSubsequentSolidBresenhamLine( ScrnInfoPtr pScrn,
        int x, int y, int dmaj, int dmin, int e, int len, int octant)
{
    GLINTPtr pGlint = GLINTPTR(pScrn);
    int dxdom, dy;

    if(dmaj == dmin) {
	GLINT_WAIT(7);
	if(octant & YDECREASING) {
	    dy = -1;
	} else {
	    dy = 1;
	}

	if(octant & XDECREASING) {
	    dxdom = -1;
	} else {
	    dxdom = 1;
	}

        TXLoadCoord(pScrn, x, y, 0, len, dxdom, dy);
	GLINT_WRITE_REG(PrimitiveLine, Render);
	return;
    }

    fbBres(pGlint->CurrentDrawable, pGlint->CurrentGC, 0,
                (octant & XDECREASING) ? -1 : 1, 
                (octant & YDECREASING) ? -1 : 1, 
                (octant & YMAJOR) ? Y_AXIS : X_AXIS,
                x, y, dmin + e, dmin, -dmaj, len);
}

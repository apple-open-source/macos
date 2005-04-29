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
 *           Mark Vojkovich, <mvojkovi@ucsd.edu>
 *           Michel Dänzer, <michdaen@iiic.ethz.ch>
 *
 * this work is sponsored by S.u.S.E. GmbH, Fuerth, Elsa GmbH, Aachen and
 * Siemens Nixdorf Informationssysteme
 * 
 * Permedia 2 accelerated options.
 */
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/glint/pm2_accel.c,v 1.32 2003/10/08 15:48:39 eich Exp $ */

#include "Xarch.h"
#include "xf86.h"
#include "xf86_OSproc.h"
#include "xf86_ansic.h"

#include "xf86PciInfo.h"
#include "xf86Pci.h"

#include "miline.h"

#include "fb.h"

#include "glint_regs.h"
#include "glint.h"

#include "xaalocal.h"		/* For replacements */

#define DEBUG 0

#if DEBUG
# define TRACE_ENTER(str)       ErrorF("pm2_accel: " str " %d\n",pScrn->scrnIndex)
# define TRACE_EXIT(str)        ErrorF("pm2_accel: " str " done\n")
# define TRACE(str)             ErrorF("pm2_accel trace: " str "\n")
#else
# define TRACE_ENTER(str)
# define TRACE_EXIT(str)
# define TRACE(str)
#endif

static void Permedia2SetupForFillRectSolid(ScrnInfoPtr pScrn, int color,
				int rop, unsigned int planemask);
static void Permedia2SubsequentFillRectSolid(ScrnInfoPtr pScrn, int x,
				int y, int w, int h);
static void Permedia2SetupForFillRectSolid24bpp(ScrnInfoPtr pScrn,int color,
				int rop, unsigned int planemask);
static void Permedia2SubsequentFillRectSolid24bpp(ScrnInfoPtr pScrn, int x,
				int y, int w, int h);
static void Permedia2SubsequentScreenToScreenCopy(ScrnInfoPtr pScrn,
				int x1, int y1, int x2,
				int y2, int w, int h);
static void Permedia2SetupForScreenToScreenCopy(ScrnInfoPtr pScrn,
				int xdir, int ydir, int rop, 
                                unsigned int planemask,
				int transparency_color);
static void Permedia2SubsequentScreenToScreenCopy2432bpp(ScrnInfoPtr pScrn,
				int x1, int y1, int x2, int y2, int w, int h);
static void Permedia2SetupForScreenToScreenCopy2432bpp(ScrnInfoPtr pScrn,
				int xdir, int ydir, int rop,
				unsigned int planemask,
				int transparency_color);
static void Permedia2SetClippingRectangle(ScrnInfoPtr pScrn, int x, int y,
				int w, int h);
static void Permedia2DisableClipping(ScrnInfoPtr pScrn);
static void Permedia2SetupForSolidLine(ScrnInfoPtr pScrn, int color,
				int rop, unsigned int planemask);
static void Permedia2SubsequentHorVertLine(ScrnInfoPtr pScrn, int x, int y, 
				int len, int dir);
static void Permedia2SubsequentSolidBresenhamLine(ScrnInfoPtr pScrn,
        			int x, int y, int dmaj, int dmin, int e, 
				int len, int octant);
static void Permedia2WriteBitmap(ScrnInfoPtr pScrn, int x, int y, int w, int h, 
				unsigned char *src, int srcwidth, int skipleft, 
				int fg, int bg, int rop,unsigned int planemask);
static void Permedia2SetupForMono8x8PatternFill(ScrnInfoPtr pScrn, 
				int patternx, int patterny, int fg, int bg, 
				int rop, unsigned int planemask);
static void Permedia2SubsequentMono8x8PatternFillRect(ScrnInfoPtr pScrn, 
				int patternx, int patterny, int x, int y, 
				int w, int h);
static void Permedia2SetupForMono8x8PatternFill24bpp(ScrnInfoPtr pScrn, 
				int patternx, int patterny, int fg, int bg, 
				int rop, unsigned int planemask);
static void Permedia2SubsequentMono8x8PatternFillRect24bpp(ScrnInfoPtr pScrn, 
				int patternx, int patterny, int x, int y, 
				int w, int h);
static void Permedia2WritePixmap8bpp(ScrnInfoPtr pScrn, int x, int y, int w, 
				int h, unsigned char *src, int srcwidth, 
				int rop, unsigned int planemask, 
				int transparency_color, int bpp, int depth);
static void Permedia2WritePixmap16bpp(ScrnInfoPtr pScrn, int x, int y, int w, 
				int h, unsigned char *src, int srcwidth, 
				int rop, unsigned int planemask, 
				int transparency_color, int bpp, int depth);
static void Permedia2WritePixmap32bpp(ScrnInfoPtr pScrn, int x, int y, int w, 
				int h, unsigned char *src, int srcwidth, 
				int rop, unsigned int planemask, 
				int transparency_color, int bpp, int depth);
static void Permedia2SetupForScanlineCPUToScreenColorExpandFill(
				ScrnInfoPtr pScrn,
				int fg, int bg, int rop, 
				unsigned int planemask);
static void Permedia2SubsequentScanlineCPUToScreenColorExpandFill(
				ScrnInfoPtr pScrn, int x,
				int y, int w, int h, int skipleft);
static void Permedia2SubsequentColorExpandScanline(ScrnInfoPtr pScrn, int bufno);
static void Permedia2LoadCoord(ScrnInfoPtr pScrn, int x, int y, int w, int h);
static void Permedia2PolylinesThinSolidWrapper(DrawablePtr pDraw, GCPtr pGC,
   				int mode, int npt, DDXPointPtr pPts);
static void Permedia2PolySegmentThinSolidWrapper(DrawablePtr pDraw, GCPtr pGC,
 				int nseg, xSegment *pSeg);

#if X_BYTE_ORDER == X_BIG_ENDIAN
# define STIPPLE_SWAP	1<<18		/* Mirror stipple pattern horizontally */
#else
# define STIPPLE_SWAP	0
#endif

void
Permedia2InitializeEngine(ScrnInfoPtr pScrn)
{
    GLINTPtr pGlint = GLINTPTR(pScrn);

    /* Initialize the Accelerator Engine to defaults */

    TRACE_ENTER("Permedia2InitializeEngine");

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
    GLINT_SLOW_WRITE_REG(UNIT_DISABLE,	FBSourceBase);
    GLINT_SLOW_WRITE_REG(UNIT_DISABLE,	LBWindowBase);

#if X_BYTE_ORDER == X_BIG_ENDIAN
    pGlint->RasterizerSwap = 1;
#else
    pGlint->RasterizerSwap = 0;
#endif

    switch (pScrn->bitsPerPixel) {
	case 8:
	    pGlint->PixelWidth = 0x0; /* 8 Bits */
	    pGlint->TexMapFormat = pGlint->pprod;
#if X_BYTE_ORDER == X_BIG_ENDIAN
	    pGlint->RasterizerSwap |= 3<<15;	/* Swap host data */
#endif
	    break;
	case 16:
	    pGlint->PixelWidth = 0x1; /* 16 Bits */
	    pGlint->TexMapFormat = pGlint->pprod | 1<<19;
#if X_BYTE_ORDER == X_BIG_ENDIAN
	    pGlint->RasterizerSwap |= 2<<15;	/* Swap host data */
#endif
	    break;
	case 24:
 	    pGlint->PixelWidth = 0x4; /* 24 Bits */
	    pGlint->TexMapFormat = pGlint->pprod | 2<<19;
	    break;
	case 32:
	    pGlint->PixelWidth = 0x2; /* 32 Bits */
	    pGlint->TexMapFormat = pGlint->pprod | 2<<19;
  	    break;
    }
    pGlint->ClippingOn = FALSE;
    pGlint->startxdom = 0;
    pGlint->startxsub = 0;
    pGlint->starty = 0;
    pGlint->count = 0;
    pGlint->dy = 1<<16;
    pGlint->dxdom = 0;
    pGlint->x = 0;
    pGlint->y = 0;
    pGlint->h = 0;
    pGlint->w = 0;
    pGlint->ROP = 0xFF;
    GLINT_SLOW_WRITE_REG(pGlint->PixelWidth, FBReadPixel);
    GLINT_SLOW_WRITE_REG(pGlint->TexMapFormat, PMTextureMapFormat);
    GLINT_SLOW_WRITE_REG(0, RectangleSize);
    GLINT_SLOW_WRITE_REG(0, RectangleOrigin);
    GLINT_SLOW_WRITE_REG(0, dXDom);
    GLINT_SLOW_WRITE_REG(1<<16, dY);
    GLINT_SLOW_WRITE_REG(0, StartXDom);
    GLINT_SLOW_WRITE_REG(0, StartXSub);
    GLINT_SLOW_WRITE_REG(0, StartY);
    GLINT_SLOW_WRITE_REG(0, GLINTCount);

    TRACE_EXIT("Permedia2InitializeEngine");
}

Bool
Permedia2AccelInit(ScreenPtr pScreen)
{
    XAAInfoRecPtr infoPtr;
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    GLINTPtr pGlint = GLINTPTR(pScrn);
    BoxRec AvailFBArea;

    pGlint->AccelInfoRec = infoPtr = XAACreateInfoRec();
    if (!infoPtr) return FALSE;

    Permedia2InitializeEngine(pScrn);

    infoPtr->Flags = PIXMAP_CACHE |
		     OFFSCREEN_PIXMAPS |
		     LINEAR_FRAMEBUFFER;
 
    infoPtr->Sync = Permedia2Sync;

    infoPtr->SetClippingRectangle = Permedia2SetClippingRectangle;
    infoPtr->DisableClipping = Permedia2DisableClipping;
    infoPtr->ClippingFlags = HARDWARE_CLIP_MONO_8x8_FILL;

    infoPtr->SolidFillFlags = 0;
    infoPtr->ScreenToScreenCopyFlags = NO_TRANSPARENCY; 
    infoPtr->WriteBitmapFlags = 0;
    if (pScrn->bitsPerPixel == 24) {
    	infoPtr->SetupForSolidFill = 
				Permedia2SetupForFillRectSolid24bpp;
    	infoPtr->SubsequentSolidFillRect = 	
				Permedia2SubsequentFillRectSolid24bpp;
    } else {
        infoPtr->SolidLineFlags = 0;
        infoPtr->PolySegmentThinSolidFlags = 0;
        infoPtr->PolylinesThinSolidFlags = 0;
        infoPtr->SetupForSolidLine = Permedia2SetupForSolidLine;
        infoPtr->SubsequentSolidHorVertLine = Permedia2SubsequentHorVertLine;
        if (!(pScrn->overlayFlags & OVERLAY_8_32_PLANAR))
        {
        	infoPtr->SubsequentSolidBresenhamLine = 
				Permedia2SubsequentSolidBresenhamLine;
        }
 	infoPtr->PolySegmentThinSolid = Permedia2PolySegmentThinSolidWrapper;
	infoPtr->PolylinesThinSolid = Permedia2PolylinesThinSolidWrapper;
    	infoPtr->SetupForSolidFill = Permedia2SetupForFillRectSolid;
    	infoPtr->SubsequentSolidFillRect = Permedia2SubsequentFillRectSolid;
    }
    
    if (pScrn->bitsPerPixel >= 24) {
	infoPtr->SetupForScreenToScreenCopy = 	
				Permedia2SetupForScreenToScreenCopy2432bpp;
    	infoPtr->SubsequentScreenToScreenCopy = 		
				Permedia2SubsequentScreenToScreenCopy2432bpp;
    } else {
    	infoPtr->SetupForScreenToScreenCopy = 	
				Permedia2SetupForScreenToScreenCopy;
    	infoPtr->SubsequentScreenToScreenCopy = 		
				Permedia2SubsequentScreenToScreenCopy;
    }

    infoPtr->Mono8x8PatternFillFlags = 
    				HARDWARE_PATTERN_PROGRAMMED_ORIGIN |
    				HARDWARE_PATTERN_PROGRAMMED_BITS |
    				HARDWARE_PATTERN_SCREEN_ORIGIN;

    if (pScrn->bitsPerPixel == 24) {
	infoPtr->SetupForMono8x8PatternFill =
				Permedia2SetupForMono8x8PatternFill24bpp;
	infoPtr->SubsequentMono8x8PatternFillRect = 
				Permedia2SubsequentMono8x8PatternFillRect24bpp;
    } else {
	infoPtr->SetupForMono8x8PatternFill =
				Permedia2SetupForMono8x8PatternFill;
	infoPtr->SubsequentMono8x8PatternFillRect = 
				Permedia2SubsequentMono8x8PatternFillRect;
    }

    infoPtr->ScanlineCPUToScreenColorExpandFillFlags = 
						BIT_ORDER_IN_BYTE_LSBFIRST;

    infoPtr->NumScanlineColorExpandBuffers = 1;
    infoPtr->ScanlineColorExpandBuffers = 
					pGlint->XAAScanlineColorExpandBuffers;
    pGlint->XAAScanlineColorExpandBuffers[0] = 
					pGlint->IOBase + OutputFIFO + 4;

    infoPtr->SetupForScanlineCPUToScreenColorExpandFill =
			Permedia2SetupForScanlineCPUToScreenColorExpandFill;
    infoPtr->SubsequentScanlineCPUToScreenColorExpandFill = 
			Permedia2SubsequentScanlineCPUToScreenColorExpandFill;
    infoPtr->SubsequentColorExpandScanline = 
			Permedia2SubsequentColorExpandScanline;

    infoPtr->WriteBitmap = Permedia2WriteBitmap;

    if (pScrn->bitsPerPixel == 8) {
  	infoPtr->WritePixmap = Permedia2WritePixmap8bpp;
	infoPtr->WritePixmapFlags = NO_GXCOPY;
    } else
    if (pScrn->bitsPerPixel == 16) {
  	infoPtr->WritePixmap = Permedia2WritePixmap16bpp;
	infoPtr->WritePixmapFlags = NO_GXCOPY;
    } else
    if (pScrn->bitsPerPixel == 32)
  	infoPtr->WritePixmap = Permedia2WritePixmap32bpp;

    /* Now fixup if we are 24bpp */
    if (pScrn->bitsPerPixel == 24) {
	infoPtr->SolidFillFlags |= NO_PLANEMASK;
	infoPtr->ScreenToScreenCopyFlags |= NO_PLANEMASK;
        infoPtr->WriteBitmapFlags |= NO_PLANEMASK;
        infoPtr->ScanlineCPUToScreenColorExpandFillFlags |= NO_PLANEMASK;
        infoPtr->Mono8x8PatternFillFlags |= NO_PLANEMASK;
    }

    AvailFBArea.x1 = 0;
    AvailFBArea.y1 = 0;
    AvailFBArea.x2 = pScrn->displayWidth;
    AvailFBArea.y2 = pGlint->FbMapSize / (pScrn->displayWidth * 
					  pScrn->bitsPerPixel / 8);

    if (AvailFBArea.y2 > 2047) AvailFBArea.y2 = 2047;

    xf86InitFBManager(pScreen, &AvailFBArea);

    return(XAAInit(pScreen, infoPtr));
}

static void Permedia2LoadCoord(
	ScrnInfoPtr pScrn,
	int x, int y,
	int w, int h
){
    GLINTPtr pGlint = GLINTPTR(pScrn);
    
    if ((h != pGlint->h) || (w != pGlint->w)) {
	pGlint->w = w;
	pGlint->h = h;
	GLINT_WRITE_REG(((h&0x0FFF)<<16)|(w&0x0FFF), RectangleSize);
    }
    if ((y != pGlint->y) || (x != pGlint->x)) {
	pGlint->x = x;
	pGlint->y = y;
	GLINT_WRITE_REG(((y&0x0FFF)<<16)|(x&0x0FFF), RectangleOrigin);
    }
}


void
Permedia2Sync(ScrnInfoPtr pScrn)
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
Permedia2SetClippingRectangle(ScrnInfoPtr pScrn, int x1, int y1, int x2, int y2)
{
    GLINTPtr pGlint = GLINTPTR(pScrn);
    TRACE_ENTER("Permedia2SetClippingRectangle");
    GLINT_WAIT(3);
    GLINT_WRITE_REG(((y1&0x0fff)<<16)|(x1&0x0fff), ScissorMinXY);
    GLINT_WRITE_REG(((y2&0x0fff)<<16)|(x2&0x0fff), ScissorMaxXY);
    GLINT_WRITE_REG(1, ScissorMode);
    pGlint->ClippingOn = TRUE;
}

static void
Permedia2DisableClipping(ScrnInfoPtr pScrn)
{
    GLINTPtr pGlint = GLINTPTR(pScrn);
    CHECKCLIPPING;
}

static void
Permedia2SetupForScreenToScreenCopy2432bpp(ScrnInfoPtr pScrn, 
				 int xdir, int ydir, int rop,
				 unsigned int planemask, int transparency_color)
{
    GLINTPtr pGlint = GLINTPTR(pScrn);

    TRACE_ENTER("Permedia2SetupForScreenToScreenCopy2432bpp");
    pGlint->BltScanDirection = 0;
    if (xdir == 1) pGlint->BltScanDirection |= XPositive;
    if (ydir == 1) pGlint->BltScanDirection |= YPositive;
  
    if (pScrn->bitsPerPixel == 24) {
	GLINT_WAIT(4);
    } else {
        GLINT_WAIT(5);
        DO_PLANEMASK(planemask);
    }

    GLINT_WRITE_REG(UNIT_DISABLE, ColorDDAMode);
    if ((rop == GXset) || (rop == GXclear)) {
	GLINT_WRITE_REG(pGlint->pprod, FBReadMode);
    } else {
    	if ((rop == GXcopy) || (rop == GXcopyInverted)) {
	    GLINT_WRITE_REG(pGlint->pprod|FBRM_SrcEnable, FBReadMode);
        } else {
	    GLINT_WRITE_REG(pGlint->pprod|FBRM_SrcEnable|FBRM_DstEnable, 
								FBReadMode);
        }
    }
    LOADROP(rop);
}

static void
Permedia2SubsequentScreenToScreenCopy2432bpp(ScrnInfoPtr pScrn, int x1,
					int y1, int x2, int y2, int w, int h)
{
    GLINTPtr pGlint = GLINTPTR(pScrn);

    TRACE_ENTER("Permedia2SubsequentScreenToScreenCopy2432bpp");
    GLINT_WAIT(4);
    Permedia2LoadCoord(pScrn, x2, y2, w, h);
    GLINT_WRITE_REG(((y1-y2)&0x0FFF)<<16 | ((x1-x2)&0x0FFF), FBSourceDelta);
    GLINT_WRITE_REG(PrimitiveRectangle | pGlint->BltScanDirection, Render);
}

static void
Permedia2SetupForScreenToScreenCopy(ScrnInfoPtr pScrn, 
				int xdir, int ydir, int rop,
				unsigned int planemask, int transparency_color)
{
    GLINTPtr pGlint = GLINTPTR(pScrn);

    TRACE_ENTER("Permedia2SetupForScreenToScreenCopy");
    pGlint->BltScanDirection = 0;
    if (xdir == 1) pGlint->BltScanDirection |= XPositive;
    if (ydir == 1) pGlint->BltScanDirection |= YPositive;
  
    GLINT_WAIT(4);
    DO_PLANEMASK(planemask);

    GLINT_WRITE_REG(UNIT_DISABLE, ColorDDAMode);
    if ((rop == GXset) || (rop == GXclear)) {
	pGlint->FrameBufferReadMode = pGlint->pprod;
    } else
    if ((rop == GXcopy) || (rop == GXcopyInverted)) {
	pGlint->FrameBufferReadMode = pGlint->pprod |FBRM_SrcEnable;
    } else {
	pGlint->FrameBufferReadMode = pGlint->pprod | FBRM_SrcEnable |
							FBRM_DstEnable;
    }
    LOADROP(rop);
    TRACE_EXIT("Permedia2SetupForScreenToScreenCopy");
}

static void
Permedia2SubsequentScreenToScreenCopy(ScrnInfoPtr pScrn, int x1, int y1,
					int x2, int y2, int w, int h)
{
    GLINTPtr pGlint = GLINTPTR(pScrn);
    char align;

    TRACE_ENTER("Permedia2SubsequentScreenToScreenCopy");
    /* We can only use GXcopy for Packed modes */
    if (pGlint->ROP != GXcopy) {
	GLINT_WAIT(5);
	GLINT_WRITE_REG(pGlint->FrameBufferReadMode, FBReadMode);
        Permedia2LoadCoord(pScrn, x2, y2, w, h);
        GLINT_WRITE_REG(((y1-y2)&0x0FFF)<<16 | ((x1-x2)&0x0FFF), FBSourceDelta);
    } else {
  	align = (x2 & pGlint->bppalign) - (x1 & pGlint->bppalign);
	GLINT_WAIT(6);
	GLINT_WRITE_REG(pGlint->FrameBufferReadMode|FBRM_Packed, FBReadMode);
        Permedia2LoadCoord(pScrn, x2>>pGlint->BppShift, y2, 
						(w+7)>>pGlint->BppShift, h);
  	GLINT_WRITE_REG(align<<29|x2<<16|(x2+w), PackedDataLimits);
        GLINT_WRITE_REG(((y1-y2)&0x0FFF)<<16 | (((x1 & ~pGlint->bppalign)-(x2 & ~pGlint->bppalign))&0x0FFF), FBSourceDelta);
    }

    GLINT_WRITE_REG(PrimitiveRectangle | pGlint->BltScanDirection, Render);
    TRACE_EXIT("Permedia2SubsequentScreenToScreenCopy");
}



static void 
Permedia2PolylinesThinSolidWrapper(
   DrawablePtr     pDraw,
   GCPtr           pGC,
   int             mode,
   int             npt,
   DDXPointPtr     pPts
){
    XAAInfoRecPtr infoRec = GET_XAAINFORECPTR_FROM_GC(pGC);
    GLINTPtr pGlint = GLINTPTR(infoRec->pScrn);
    TRACE("Permedia2PolylinesThinSolidWrapper");
    pGlint->CurrentGC = pGC;
    pGlint->CurrentDrawable = pDraw;
    if(infoRec->NeedToSync) (*infoRec->Sync)(infoRec->pScrn);
    XAAPolyLines(pDraw, pGC, mode, npt, pPts);
}

static void 
Permedia2PolySegmentThinSolidWrapper(
   DrawablePtr     pDraw,
   GCPtr           pGC,
   int             nseg,
   xSegment        *pSeg
){
    XAAInfoRecPtr infoRec = GET_XAAINFORECPTR_FROM_GC(pGC);
    GLINTPtr pGlint = GLINTPTR(infoRec->pScrn);
    TRACE("Permedia2PolySegmentThinSolidWrapper");
    pGlint->CurrentGC = pGC;
    pGlint->CurrentDrawable = pDraw;
    if(infoRec->NeedToSync) (*infoRec->Sync)(infoRec->pScrn);
    XAAPolySegment(pDraw, pGC, nseg, pSeg);
}

static void
Permedia2SetupForSolidLine(ScrnInfoPtr pScrn, int color,
					 int rop, unsigned int planemask)
{
    GLINTPtr pGlint = GLINTPTR(pScrn);

    TRACE_ENTER("Permedia2SetupForSolidLine");
    GLINT_WAIT(6);
    DO_PLANEMASK(planemask);
    GLINT_WRITE_REG(UNIT_DISABLE, ColorDDAMode);
    GLINT_WRITE_REG(color, GLINTColor);
    if (rop == GXcopy) {
  	GLINT_WRITE_REG(pGlint->pprod, FBReadMode);
    } else {
  	GLINT_WRITE_REG(pGlint->pprod | FBRM_DstEnable, FBReadMode);
    }
    LOADROP(rop);
}

static void
Permedia2SubsequentHorVertLine(ScrnInfoPtr pScrn,int x,int y,int len,int dir)
{
    GLINTPtr pGlint = GLINTPTR(pScrn);
    
    TRACE_ENTER("Permedia2SubsequentHorVertLine");
    GLINT_WAIT(6);
    GLINT_WRITE_REG(x<<16, StartXDom);
    GLINT_WRITE_REG(y<<16, StartY);
    if (dir == DEGREES_0) {
	GLINT_WRITE_REG(1<<16, dXDom);
	GLINT_WRITE_REG(0<<16, dY);
    } else {
	GLINT_WRITE_REG(0<<16, dXDom);
	GLINT_WRITE_REG(1<<16, dY);
    }

    GLINT_WRITE_REG(len, GLINTCount);
    GLINT_WRITE_REG(PrimitiveLine, Render);
}

static void 
Permedia2SubsequentSolidBresenhamLine( ScrnInfoPtr pScrn,
        int x, int y, int dmaj, int dmin, int e, int len, int octant)
{
    GLINTPtr pGlint = GLINTPTR(pScrn);

    TRACE_ENTER("Permedia2SubsequentSolidBresenhamLine");
    if(dmaj == dmin) {
	GLINT_WAIT(6);
	if(octant & YDECREASING) {
	    GLINT_WRITE_REG(-1<<16, dY);
	} else {
	    GLINT_WRITE_REG(1<<16, dY);
	}

	if(octant & XDECREASING) {
	    GLINT_WRITE_REG(-1<<16, dXDom);
	} else {
	    GLINT_WRITE_REG(1<<16, dXDom);
	}

	GLINT_WRITE_REG(x<<16, StartXDom);
	GLINT_WRITE_REG(y<<16, StartY);
	GLINT_WRITE_REG(len,GLINTCount);
	GLINT_WRITE_REG(PrimitiveLine, Render);
	return;
    }
    
    fbBres(pGlint->CurrentDrawable, pGlint->CurrentGC, 0,
                (octant & XDECREASING) ? -1 : 1, 
                (octant & YDECREASING) ? -1 : 1, 
                (octant & YMAJOR) ? Y_AXIS : X_AXIS,
                x, y, e, dmin, -dmaj, len);
}

static void
Permedia2SetupForFillRectSolid24bpp(ScrnInfoPtr pScrn, int color,
					 int rop, unsigned int planemask)
{
    GLINTPtr pGlint = GLINTPTR(pScrn);
    pGlint->ForeGroundColor = color;

    TRACE_ENTER("Permedia2SetupForFillRectSolid24bpp");
    GLINT_WAIT(5);
    GLINT_WRITE_REG(UNIT_ENABLE, ColorDDAMode);
    GLINT_WRITE_REG(color, ConstantColor);
    if (rop == GXcopy) {
  	GLINT_WRITE_REG(pGlint->pprod, FBReadMode);
    } else {
  	GLINT_WRITE_REG(pGlint->pprod | FBRM_DstEnable, FBReadMode);
    }
    LOADROP(rop);
}

static void
Permedia2SetupForFillRectSolid(ScrnInfoPtr pScrn, int color, 
				    int rop, unsigned int planemask)
{
    GLINTPtr pGlint = GLINTPTR(pScrn);
    TRACE_ENTER("Permedia2SetupForFillRectSolid");

    REPLICATE(color);

    GLINT_WAIT(6);
    DO_PLANEMASK(planemask);
    if (rop == GXcopy) {
	GLINT_WRITE_REG(UNIT_DISABLE, ColorDDAMode);
	GLINT_WRITE_REG(pGlint->pprod, FBReadMode);
	GLINT_WRITE_REG(color, FBBlockColor);
    } else {
	GLINT_WRITE_REG(UNIT_ENABLE, ColorDDAMode);
      	GLINT_WRITE_REG(color, ConstantColor);
	/* We can use Packed mode for filling solid non-GXcopy rasters */
	GLINT_WRITE_REG(pGlint->pprod|FBRM_DstEnable|FBRM_Packed, FBReadMode);
    }
    LOADROP(rop);
    TRACE_EXIT("Permedia2SetupForFillRectSolid");
}

static void
Permedia2SubsequentFillRectSolid24bpp(ScrnInfoPtr pScrn, int x, int y, int w, int h)
{
    GLINTPtr pGlint = GLINTPTR(pScrn);
    TRACE_ENTER("Permedia2SubsequentFillRectSolid24bpp");
    GLINT_WAIT(3);
    Permedia2LoadCoord(pScrn, x, y, w, h);
    GLINT_WRITE_REG(PrimitiveRectangle | XPositive | YPositive, Render);
}

static void
Permedia2SubsequentFillRectSolid(ScrnInfoPtr pScrn, int x, int y, int w, int h)
{
    GLINTPtr pGlint = GLINTPTR(pScrn);
    int speed = 0;

    TRACE_ENTER("Permedia2SubsequentFillRectSolid");
    if (pGlint->ROP == GXcopy) {
	GLINT_WAIT(3);
        Permedia2LoadCoord(pScrn, x, y, w, h);
  	speed = FastFillEnable;
    } else {
	GLINT_WAIT(4);
        Permedia2LoadCoord(pScrn, x>>pGlint->BppShift, y, 
						(w+7)>>pGlint->BppShift, h);
  	GLINT_WRITE_REG(x<<16|(x+w), PackedDataLimits);
  	speed = 0;
    }
    GLINT_WRITE_REG(PrimitiveRectangle | XPositive | YPositive | speed, Render);
    TRACE_EXIT("Permedia2SubsequentFillRectSolid");
}

static void 
Permedia2SetupForMono8x8PatternFill24bpp(ScrnInfoPtr pScrn, 
					   int patternx, int patterny, 
					   int fg, int bg, int rop,
					   unsigned int planemask)
{
    GLINTPtr pGlint = GLINTPTR(pScrn);

    TRACE_ENTER("Permedia2SetupForMono8x8PatternFill24bpp");
    if (bg == -1) pGlint->FrameBufferReadMode = -1;
	else    pGlint->FrameBufferReadMode = 0;

    pGlint->ForeGroundColor = fg;
    pGlint->BackGroundColor = bg;
    REPLICATE(pGlint->ForeGroundColor);
    REPLICATE(pGlint->BackGroundColor);
  
    GLINT_WAIT(12);
    GLINT_WRITE_REG((patternx & 0xFF), AreaStipplePattern0);
    GLINT_WRITE_REG((patternx & 0xFF00) >> 8, AreaStipplePattern1);
    GLINT_WRITE_REG((patternx & 0xFF0000) >> 16, AreaStipplePattern2);
    GLINT_WRITE_REG((patternx & 0xFF000000) >> 24, AreaStipplePattern3);
    GLINT_WRITE_REG((patterny & 0xFF), AreaStipplePattern4);
    GLINT_WRITE_REG((patterny & 0xFF00) >> 8, AreaStipplePattern5);
    GLINT_WRITE_REG((patterny & 0xFF0000) >> 16, AreaStipplePattern6);
    GLINT_WRITE_REG((patterny & 0xFF000000) >> 24, AreaStipplePattern7);
  
    GLINT_WRITE_REG(UNIT_ENABLE, ColorDDAMode);
    if (rop == GXcopy) {
	GLINT_WRITE_REG(pGlint->pprod, FBReadMode);
    } else {
	GLINT_WRITE_REG(pGlint->pprod | FBRM_DstEnable, FBReadMode);
    }
    LOADROP(rop);
}

static void 
Permedia2SetupForMono8x8PatternFill(ScrnInfoPtr pScrn, 
					   int patternx, int patterny, 
					   int fg, int bg, int rop,
					   unsigned int planemask)
{
    GLINTPtr pGlint = GLINTPTR(pScrn);

    TRACE_ENTER("Permedia2SetupForMono8x8PatternFill");
    if (bg == -1) pGlint->FrameBufferReadMode = -1;
	else    pGlint->FrameBufferReadMode = 0;

    pGlint->ForeGroundColor = fg;
    pGlint->BackGroundColor = bg;
    REPLICATE(pGlint->ForeGroundColor);
    REPLICATE(pGlint->BackGroundColor);
  
#if DEBUG
    ErrorF("patternx: %x patterny: %x\n", patternx, patterny);
#endif

    GLINT_WAIT(13);
    DO_PLANEMASK(planemask);
    GLINT_WRITE_REG((patternx & 0xFF), AreaStipplePattern0);
    GLINT_WRITE_REG((patternx & 0xFF00) >> 8, AreaStipplePattern1);
    GLINT_WRITE_REG((patternx & 0xFF0000) >> 16, AreaStipplePattern2);
    GLINT_WRITE_REG((patternx & 0xFF000000) >> 24, AreaStipplePattern3);
    GLINT_WRITE_REG((patterny & 0xFF), AreaStipplePattern4);
    GLINT_WRITE_REG((patterny & 0xFF00) >> 8, AreaStipplePattern5);
    GLINT_WRITE_REG((patterny & 0xFF0000) >> 16, AreaStipplePattern6);
    GLINT_WRITE_REG((patterny & 0xFF000000) >> 24, AreaStipplePattern7);

    if (rop == GXcopy) {
        GLINT_WRITE_REG(UNIT_DISABLE, ColorDDAMode);
	GLINT_WRITE_REG(pGlint->pprod, FBReadMode);
    } else {
        GLINT_WRITE_REG(UNIT_ENABLE, ColorDDAMode);
	GLINT_WRITE_REG(pGlint->pprod | FBRM_DstEnable, FBReadMode);
    }
    LOADROP(rop);
    TRACE_EXIT("Permedia2SetupForMono8x8PatternFill");
}

static void 
Permedia2SubsequentMono8x8PatternFillRect24bpp(ScrnInfoPtr pScrn, 	
				   int patternx, int patterny,
				   int x, int y,
				   int w, int h)
{
    GLINTPtr pGlint = GLINTPTR(pScrn);

    TRACE_ENTER("Permedia2SubsequentMono8x8PatternFillRect24bpp");
    GLINT_WAIT(8);
    Permedia2LoadCoord(pScrn, x, y, w, h);

    if (pGlint->FrameBufferReadMode != -1) {
	GLINT_WRITE_REG(pGlint->BackGroundColor, ConstantColor);
	GLINT_WRITE_REG(patternx<<7|patterny<<12| ASM_InvertPattern |
				STIPPLE_SWAP | UNIT_ENABLE, AreaStippleMode);
	GLINT_WRITE_REG(AreaStippleEnable | XPositive | 
					YPositive | PrimitiveRectangle, Render);
    }

    GLINT_WRITE_REG(pGlint->ForeGroundColor, ConstantColor);
    GLINT_WRITE_REG(patternx<<7|patterny<<12|UNIT_ENABLE, AreaStippleMode);
    GLINT_WRITE_REG(AreaStippleEnable | XPositive | YPositive |
						PrimitiveRectangle, Render);
}

static void 
Permedia2SubsequentMono8x8PatternFillRect(ScrnInfoPtr pScrn, 
				   int patternx, int patterny,
				   int x, int y,
				   int w, int h)
{
    GLINTPtr pGlint = GLINTPTR(pScrn);
  
    TRACE_ENTER("Permedia2SubsequentMono8x8PatternFillRect()");
	
    GLINT_WAIT(9);
    Permedia2LoadCoord(pScrn, x, y, w, h);

    if (pGlint->FrameBufferReadMode != -1) {
	if (pGlint->ROP == GXcopy) {
      	    GLINT_WRITE_REG(pGlint->BackGroundColor, FBBlockColor);
    	    GLINT_WRITE_REG(ASM_InvertPattern|patternx<<7|patterny<<12|
			STIPPLE_SWAP | UNIT_ENABLE, AreaStippleMode);
    	    GLINT_WRITE_REG(AreaStippleEnable | FastFillEnable | 
			XPositive | YPositive | PrimitiveRectangle, Render);
	} else { 
  	    GLINT_WRITE_REG(pGlint->ForeGroundColor, ConstantColor);
      	    GLINT_WRITE_REG(pGlint->BackGroundColor, Texel0);
	    GLINT_WRITE_REG(patternx<<7|patterny<<12| 
			STIPPLE_SWAP | UNIT_ENABLE, AreaStippleMode);
  	    GLINT_WRITE_REG(AreaStippleEnable | XPositive | TextureEnable |
				YPositive | PrimitiveRectangle, Render);
	    return;
	}
    }

    if (pGlint->ROP == GXcopy) {
	GLINT_WRITE_REG(pGlint->ForeGroundColor, FBBlockColor);
	pGlint->FrameBufferReadMode = FastFillEnable;
    } else {
  	GLINT_WRITE_REG(pGlint->ForeGroundColor, ConstantColor);
	pGlint->FrameBufferReadMode = 0;
    }
    GLINT_WRITE_REG(patternx<<7|patterny<<12|STIPPLE_SWAP|UNIT_ENABLE, AreaStippleMode);
    GLINT_WRITE_REG(AreaStippleEnable | pGlint->FrameBufferReadMode | 
			XPositive | YPositive | PrimitiveRectangle, Render);

    TRACE_EXIT("Permedia2SubsequentMono8x8PatternFillRect()");
}

static void
Permedia2SetupForScanlineCPUToScreenColorExpandFill(
	ScrnInfoPtr pScrn,
	int fg, int bg, 
	int rop, 
	unsigned int planemask
){
    GLINTPtr pGlint = GLINTPTR(pScrn);
    int dobackground = 0;

    TRACE_ENTER("Permedia2SetupForScanlineCPUToScreenColorExpandFill");
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
	GLINT_WRITE_REG(pGlint->RasterizerSwap,RasterizerMode);
    } else {
        GLINT_WRITE_REG(UNIT_ENABLE, ColorDDAMode);
        GLINT_WRITE_REG(fg, ConstantColor);
	GLINT_WRITE_REG(BitMaskPackingEachScanline|dobackground|
				pGlint->RasterizerSwap,RasterizerMode);
	if (dobackground) {
	    GLINT_WRITE_REG(bg, Texel0);
	    pGlint->FrameBufferReadMode = TextureEnable;
	} else {
            pGlint->FrameBufferReadMode = 0;
	}
    }
    LOADROP(rop);
    TRACE_EXIT("Permedia2SetupForScanlineCPUToScreenColorExpandFill");
}

static void
Permedia2SubsequentScanlineCPUToScreenColorExpandFill(
	ScrnInfoPtr pScrn,
	int x, int y, int w, int h,
	int skipleft
){
    GLINTPtr pGlint = GLINTPTR(pScrn);

    TRACE_ENTER("Permedia2SubsequentScanlineCPUToScreenColorExpandFill");
	
    pGlint->dwords = ((w + 31) >> 5); /* dwords per scanline */
 
    pGlint->cpucount = h;

    GLINT_WAIT(6);
    Permedia2LoadCoord(pScrn, x, y, w, h);
    GLINT_WRITE_REG(PrimitiveRectangle | XPositive | YPositive | pGlint->FrameBufferReadMode | SyncOnBitMask, Render);
    GLINT_WRITE_REG(((pGlint->dwords*h)-1)<<16 | 0x0D, OutputFIFO);
    GLINT_WAIT(pGlint->dwords);
    pGlint->cpucount--;
    TRACE_EXIT("Permedia2SubsequentScanlineCPUToScreenColorExpandFill");
}

static void
Permedia2SubsequentColorExpandScanline(ScrnInfoPtr pScrn, int bufno)
{
    GLINTPtr pGlint = GLINTPTR(pScrn);

    TRACE_ENTER("Permedia2SubsequentColorExpandScanline");
    if (pGlint->cpucount--)
    	GLINT_WAIT(pGlint->dwords);
}


static void
Permedia2WriteBitmap(ScrnInfoPtr pScrn,
    int x, int y, int w, int h,
    unsigned char *src,
    int srcwidth,
    int skipleft,
    int fg, int bg,
    int rop,
    unsigned int planemask
)
{
    XAAInfoRecPtr infoRec = GET_XAAINFORECPTR_FROM_SCRNINFOPTR(pScrn);
    GLINTPtr pGlint = GLINTPTR(pScrn);
    unsigned char *srcpntr;
    int dwords, height, mode;
    Bool SecondPass = FALSE;

    TRACE_ENTER("Permedia2WriteBitmap");

    w += skipleft;
    x -= skipleft;
    dwords = (w + 31) >> 5;

    Permedia2SetClippingRectangle(pScrn,x+skipleft,y,x+w,y+h);
 
    if (pScrn->bitsPerPixel == 24) {
	GLINT_WAIT(10);
    } else {
        GLINT_WAIT(11);
        DO_PLANEMASK(planemask);
    }
    LOADROP(rop);
    Permedia2LoadCoord(pScrn, x&0xFFFF, y, w, h);
    if (rop == GXcopy) {
    	GLINT_WRITE_REG(pGlint->pprod, FBReadMode);
    } else {
	GLINT_WRITE_REG(pGlint->pprod | FBRM_DstEnable, FBReadMode);
    }
    if ((pScrn->bitsPerPixel != 24) && (rop == GXcopy)) {
	mode = FastFillEnable;
	GLINT_WRITE_REG(UNIT_DISABLE, ColorDDAMode);
    	GLINT_WRITE_REG(pGlint->RasterizerSwap,RasterizerMode);
    } else {
	mode = 0;
	GLINT_WRITE_REG(UNIT_ENABLE, ColorDDAMode);
	GLINT_WRITE_REG(BitMaskPackingEachScanline|
		pGlint->RasterizerSwap,RasterizerMode);
    }

    if(bg == -1) {
	REPLICATE(fg);
        if ((pScrn->bitsPerPixel != 24) && (rop == GXcopy)) {
	    GLINT_WRITE_REG(fg, FBBlockColor);
	} else {
	    GLINT_WRITE_REG(fg, ConstantColor);
	}
    } else if((rop == GXcopy) && (pScrn->bitsPerPixel != 24)) {
	REPLICATE(bg);
	GLINT_WRITE_REG(bg, FBBlockColor);
	GLINT_WRITE_REG(PrimitiveRectangle | XPositive |YPositive |mode,Render);
	REPLICATE(fg);
	GLINT_WRITE_REG(fg, FBBlockColor);
    } else {
	SecondPass = TRUE;
	REPLICATE(fg);
        if ((pScrn->bitsPerPixel != 24) && (rop == GXcopy)) {
	    GLINT_WRITE_REG(fg, FBBlockColor);
	} else {
	    GLINT_WRITE_REG(fg, ConstantColor);
	}
    }

SECOND_PASS:
    GLINT_WRITE_REG(PrimitiveRectangle | XPositive | YPositive | mode | SyncOnBitMask, Render);
    
    height = h;
    srcpntr = src;
    while(height--) {
    	GLINT_WAIT(dwords + 1);
    	/* 0x0D is the TAG value for BitMaskPattern */
    	GLINT_WRITE_REG(((dwords - 1) << 16) | 0x0D, OutputFIFO);
   	GLINT_MoveDWORDS((CARD32*)((char*)pGlint->IOBase + OutputFIFO + 4),
		(CARD32 *)srcpntr, dwords);
	srcpntr += srcwidth;
    }   

    if(SecondPass) {
	SecondPass = FALSE;
	REPLICATE(bg);
	GLINT_WAIT(3);
        if ((pScrn->bitsPerPixel != 24) && (rop == GXcopy)) {
   	    GLINT_WRITE_REG(InvertBitMask|pGlint->RasterizerSwap,RasterizerMode);
	    GLINT_WRITE_REG(bg, FBBlockColor);
	} else {
    	    GLINT_WRITE_REG(InvertBitMask|BitMaskPackingEachScanline|
    	    			pGlint->RasterizerSwap, RasterizerMode);
	    GLINT_WRITE_REG(bg, ConstantColor);
	}
	goto SECOND_PASS;
    }

    GLINT_WAIT(1);
    GLINT_WRITE_REG(pGlint->RasterizerSwap, RasterizerMode);
    Permedia2DisableClipping(pScrn);
    SET_SYNC_FLAG(infoRec);
    TRACE_EXIT("Permedia2WriteBitmap");
}


static void
Permedia2WritePixmap8bpp(
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

    TRACE_ENTER("Permedia2SubsequentMono8x8PatternFillRect24bpp");
    GLINT_WAIT(3);
    DO_PLANEMASK(planemask);
    GLINT_WRITE_REG(pGlint->RasterizerSwap,RasterizerMode);
    if (rop == GXcopy) {      
	GLINT_WRITE_REG(pGlint->pprod | FBRM_Packed, FBReadMode);
    } else {
	GLINT_WRITE_REG(pGlint->pprod | FBRM_DstEnable, FBReadMode);
    }

    dwords = (w + 3) >> 2;
#if X_BYTE_ORDER == X_LITTLE_ENDIAN
    if((!(x&3)) && (!(w&3))) FastTexLoad = TRUE;
#endif	
    if((rop != GXcopy) || (planemask != ~0))
	FastTexLoad = FALSE;

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
	

        if(FastTexLoad) {
	  int address;

	  GLINT_WAIT(1);
	  GLINT_WRITE_REG(UNIT_DISABLE, FBWriteMode);
	  Permedia2Sync(pScrn);	/* we are not using the rasterizer */
	  while(h--) {
	      count = dwords;
	      address = ((y * pScrn->displayWidth) + x) >> 2;
	      srcp = (CARD32*)src;
	      GLINT_WAIT(1); /*??*/
	      GLINT_WRITE_REG(address, TextureDownloadOffset);
	      while(count >= pGlint->FIFOSize) {
    	    	GLINT_WAIT(pGlint->FIFOSize);
		/* (0x11 << 4) | 0x0D is the TAG for TextureData */
        	GLINT_WRITE_REG(((pGlint->FIFOSize - 2) << 16) | (0x11 << 4) |
						0x0D, OutputFIFO);
		GLINT_MoveDWORDS(
			(CARD32*)((char*)pGlint->IOBase + OutputFIFO + 4),
	 		(CARD32*)srcp, pGlint->FIFOSize - 1);
		count -= pGlint->FIFOSize - 1;
		address += pGlint->FIFOSize - 1;
		srcp += pGlint->FIFOSize - 1;
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
  	   char align = (x & pGlint->bppalign);
		

	   GLINT_WAIT(1); /*??*/
	   GLINT_WRITE_REG(UNIT_DISABLE, ColorDDAMode);
	   if (rop == GXcopy) {
	     GLINT_WAIT(6);
             Permedia2LoadCoord(pScrn, x>>pGlint->BppShift, y, 
				(w+pGlint->bppalign)>>pGlint->BppShift, h);
  	     GLINT_WRITE_REG(align<<29|x<<16|(x+w), PackedDataLimits);
	   } else {
	     Permedia2SetClippingRectangle(pScrn,x+skipleft,y,x+w,y+h);
	     GLINT_WAIT(5);
             Permedia2LoadCoord(pScrn, x, y, w, h);
	   }
	   LOADROP(rop);
  	   GLINT_WRITE_REG(PrimitiveRectangle | XPositive | YPositive |
				SyncOnHostData, Render);

	   if (rop == GXcopy) {
	    while(h--) {
	      count = dwords;
	      srcp = (CARD32*)src;
	      while(count >= pGlint->FIFOSize) {
    	    	GLINT_WAIT(pGlint->FIFOSize);
		/* (0x15 << 4) | 0x05 is the TAG for FBSourceData */
        	GLINT_WRITE_REG(((pGlint->FIFOSize - 2) << 16) | (0x15 << 4) | 
					0x05, OutputFIFO);
		GLINT_MoveDWORDS(
			(CARD32*)((char*)pGlint->IOBase + OutputFIFO + 4),
	 		(CARD32*)srcp, pGlint->FIFOSize - 1);
		count -= pGlint->FIFOSize - 1;
		srcp += pGlint->FIFOSize - 1;
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
	   } else {
	    while(h--) {
	      count = w;
	      srcpbyte = (unsigned char *)src;
	      while(count >= pGlint->FIFOSize) {
    	    	GLINT_WAIT(pGlint->FIFOSize);
		/* (0x15 << 4) | 0x05 is the TAG for FBSourceData */
        	GLINT_WRITE_REG(((pGlint->FIFOSize - 2) << 16) | (0x15 << 4) | 
					0x05, OutputFIFO);
		GLINT_MoveBYTE(
			(CARD32*)((char*)pGlint->IOBase + OutputFIFO + 4),
	 		(unsigned char *)srcpbyte, pGlint->FIFOSize - 1);
		count -= pGlint->FIFOSize - 1;
		srcpbyte += pGlint->FIFOSize - 1;
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

    Permedia2DisableClipping(pScrn);
    SET_SYNC_FLAG(infoRec);
}

static void
Permedia2WritePixmap16bpp(
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

    TRACE_ENTER("Permedia2WritePixmap16bpp");
    GLINT_WAIT(3);
    DO_PLANEMASK(planemask);
    GLINT_WRITE_REG(pGlint->RasterizerSwap,RasterizerMode);
    if (rop == GXcopy) {      
	GLINT_WRITE_REG(pGlint->pprod | FBRM_Packed, FBReadMode);
    } else {
	GLINT_WRITE_REG(pGlint->pprod | FBRM_DstEnable, FBReadMode);
    }

	FastTexLoad = FALSE;
	dwords = (w + 1) >> 1;
#if X_BYTE_ORDER == X_LITTLE_ENDIAN
	if((!(x&1)) && (!(w&1))) FastTexLoad = TRUE;
#endif
	if((rop != GXcopy) || (planemask != ~0))
		FastTexLoad = FALSE;

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
	
        if(FastTexLoad) {
	  int address;

	  GLINT_WAIT(1);
	  GLINT_WRITE_REG(UNIT_DISABLE, FBWriteMode);
	  Permedia2Sync(pScrn);	/* we are not using the rasterizer */
	  while(h--) {
	      count = dwords;
	      address = ((y * pScrn->displayWidth) + x) >> 1;
	      srcp = (CARD32*)src;
	      GLINT_WAIT(1); /*??*/
	      GLINT_WRITE_REG(address, TextureDownloadOffset);
	      while(count >= pGlint->FIFOSize) {
    	    	GLINT_WAIT(pGlint->FIFOSize);
		/* (0x11 << 4) | 0x0D is the TAG for TextureData */
        	GLINT_WRITE_REG(((pGlint->FIFOSize - 2) << 16) | (0x11 << 4) |
						0x0D, OutputFIFO);
		GLINT_MoveDWORDS(
			(CARD32*)((char*)pGlint->IOBase + OutputFIFO + 4),
	 		(CARD32*)srcp, pGlint->FIFOSize - 1);
		count -= pGlint->FIFOSize - 1;
		address += pGlint->FIFOSize - 1;
		srcp += pGlint->FIFOSize - 1;
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
  	   char align = (x & pGlint->bppalign);
		

	   GLINT_WRITE_REG(UNIT_DISABLE, ColorDDAMode);
	   if (rop == GXcopy) {
	     GLINT_WAIT(6);
             Permedia2LoadCoord(pScrn, x>>pGlint->BppShift, y, 
				(w+pGlint->bppalign)>>pGlint->BppShift, h);
  	     GLINT_WRITE_REG(align<<29|x<<16|(x+w), PackedDataLimits);
	   } else {
	     Permedia2SetClippingRectangle(pScrn,x+skipleft,y,x+w,y+h);
	     GLINT_WAIT(5);
             Permedia2LoadCoord(pScrn, x, y, w, h);
	   }
	   LOADROP(rop);
  	   GLINT_WRITE_REG(PrimitiveRectangle | XPositive | YPositive |
				SyncOnHostData, Render);

	   if (rop == GXcopy) {
	    while(h--) {
	      count = dwords;
	      srcp = (CARD32*)src;
	      while(count >= pGlint->FIFOSize) {
    	    	GLINT_WAIT(pGlint->FIFOSize);
		/* (0x15 << 4) | 0x05 is the TAG for FBSourceData */
        	GLINT_WRITE_REG(((pGlint->FIFOSize - 2) << 16) | (0x15 << 4) | 
					0x05, OutputFIFO);
		GLINT_MoveDWORDS(
			(CARD32*)((char*)pGlint->IOBase + OutputFIFO + 4),
	 		(CARD32*)srcp, pGlint->FIFOSize - 1);
		count -= pGlint->FIFOSize - 1;
		srcp += pGlint->FIFOSize - 1;
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
	   } else {
	    while(h--) {
	      count = w;
	      srcpword = (unsigned short *)src;
	      while(count >= pGlint->FIFOSize) {
    	    	GLINT_WAIT(pGlint->FIFOSize);
		/* (0x15 << 4) | 0x05 is the TAG for FBSourceData */
        	GLINT_WRITE_REG(((pGlint->FIFOSize - 2) << 16) | (0x15 << 4) | 
					0x05, OutputFIFO);
		GLINT_MoveWORDS(
			(CARD32*)((char*)pGlint->IOBase + OutputFIFO + 4),
	 		(unsigned short *)srcpword, pGlint->FIFOSize - 1);
		count -= pGlint->FIFOSize - 1;
		srcpword += pGlint->FIFOSize - 1;
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

    Permedia2DisableClipping(pScrn);
    SET_SYNC_FLAG(infoRec);
    TRACE_EXIT("Permedia2WritePixmap16bpp");
}

static void
Permedia2WritePixmap32bpp(
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
    
    TRACE_ENTER("Permedia2WritePixmap32bpp");
    GLINT_WAIT(3);
    DO_PLANEMASK(planemask);
    GLINT_WRITE_REG(pGlint->RasterizerSwap,RasterizerMode);
    if (rop == GXcopy) {      
	GLINT_WRITE_REG(pGlint->pprod, FBReadMode);
    } else {
	GLINT_WRITE_REG(pGlint->pprod | FBRM_DstEnable, FBReadMode);
    }

	dwords = w;
	
	if((rop == GXcopy) && (planemask == ~0)) {
	  int address;

	  GLINT_WAIT(1);
	  GLINT_WRITE_REG(UNIT_DISABLE, FBWriteMode);
	  Permedia2Sync(pScrn);	/* we are not using the rasterizer */
	  while(h--) {
	      count = dwords;
	      address = (y * pScrn->displayWidth) + x;
	      srcp = (CARD32*)src;
	      GLINT_WAIT(1); /*??*/
	      GLINT_WRITE_REG(address, TextureDownloadOffset);
	      while(count >= pGlint->FIFOSize) {
    	    	GLINT_WAIT(pGlint->FIFOSize);
		/* (0x11 << 4) | 0x0D is the TAG for TextureData */
        	GLINT_WRITE_REG(((pGlint->FIFOSize - 2) << 16) | (0x11 << 4) |
						0x0D, OutputFIFO);
		GLINT_MoveDWORDS(
			(CARD32*)((char*)pGlint->IOBase + OutputFIFO + 4),
	 		(CARD32*)srcp, pGlint->FIFOSize - 1);
		count -= pGlint->FIFOSize - 1;
		address += pGlint->FIFOSize - 1;
		srcp += pGlint->FIFOSize - 1;
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
	   if((skipleft = (long)src & 0x03L)) {
	      skipleft /= (bpp>>3);

	      x -= skipleft;	     
	      w += skipleft;
	
	      src = (unsigned char*)((long)src & ~0x03L); 
	   }

	   Permedia2SetClippingRectangle(pScrn,x+skipleft,y,x+w,y+h);

	   GLINT_WAIT(6);
           Permedia2LoadCoord(pScrn, x, y, w, h);
	   GLINT_WRITE_REG(UNIT_DISABLE, ColorDDAMode);
	   LOADROP(rop);
  	   GLINT_WRITE_REG(PrimitiveRectangle | XPositive | YPositive |
				SyncOnHostData, Render);

	   while(h--) {
	      count = dwords;
	      srcp = (CARD32*)src;
	      while(count >= pGlint->FIFOSize) {
    	    	GLINT_WAIT(pGlint->FIFOSize);
		/* (0x15 << 4) | 0x05 is the TAG for FBSourceData */
        	GLINT_WRITE_REG(((pGlint->FIFOSize - 2) << 16) | (0x15 << 4) | 
					0x05, OutputFIFO);
		GLINT_MoveDWORDS(
			(CARD32*)((char*)pGlint->IOBase + OutputFIFO + 4),
	 		(CARD32*)srcp, pGlint->FIFOSize - 1);
		count -= pGlint->FIFOSize - 1;
		srcp += pGlint->FIFOSize - 1;
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

    Permedia2DisableClipping(pScrn);
    SET_SYNC_FLAG(infoRec);
}

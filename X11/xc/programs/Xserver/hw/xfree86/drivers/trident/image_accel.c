/*
 * Copyright 1997-2003 by Alan Hourihane, North Wales, UK.
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
 * 
 * Trident 3DImage' accelerated options.
 */
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/trident/image_accel.c,v 1.28 2004/02/20 23:34:11 alanh Exp $ */

#include "xf86.h"
#include "xf86_OSproc.h"
#include "xf86_ansic.h"

#include "xf86PciInfo.h"
#include "xf86Pci.h"

#include "miline.h"

#include "trident.h"
#include "trident_regs.h"

#include "xaarop.h"
#include "xaalocal.h"

static void ImageSync(ScrnInfoPtr pScrn);
static void ImageSyncClip(ScrnInfoPtr pScrn);
#if 0
static void ImageSetupForSolidLine(ScrnInfoPtr pScrn, int color,
				int rop, unsigned int planemask);
static void ImageSubsequentSolidBresenhamLine(ScrnInfoPtr pScrn,
        			int x, int y, int dmaj, int dmin, int e, 
				int len, int octant);
#endif
static void ImageSetupForFillRectSolid(ScrnInfoPtr pScrn, int color,
				int rop, unsigned int planemask);
static void ImageSubsequentFillRectSolid(ScrnInfoPtr pScrn, int x,
				int y, int w, int h);
static void ImageSubsequentScreenToScreenCopy(ScrnInfoPtr pScrn,
				int x1, int y1, int x2,
				int y2, int w, int h);
static void ImageSetupForScreenToScreenCopy(ScrnInfoPtr pScrn,
				int xdir, int ydir, int rop, 
                                unsigned int planemask,
				int transparency_color);
static void ImageSetClippingRectangle(ScrnInfoPtr pScrn, int x1, int y1, 
				int x2, int y2);
static void ImageDisableClipping(ScrnInfoPtr pScrn);
static void ImageSetupForMono8x8PatternFill(ScrnInfoPtr pScrn, 
				int patternx, int patterny, int fg, int bg, 
				int rop, unsigned int planemask);
static void ImageSubsequentMono8x8PatternFillRect(ScrnInfoPtr pScrn, 
				int patternx, int patterny, int x, int y, 
				int w, int h);
#if 0
static void ImageSetupForColor8x8PatternFill(ScrnInfoPtr pScrn, 
				int patternx, int patterny, 
				int rop, unsigned int planemask, int trans_col);
static void ImageSubsequentColor8x8PatternFillRect(ScrnInfoPtr pScrn, 
				int patternx, int patterny, int x, int y, 
				int w, int h);
#endif
static void ImageSetupForScanlineImageWrite(ScrnInfoPtr pScrn, int rop,
				unsigned int planemask, int transparency_color,
				int bpp, int depth);
static void ImageSubsequentScanlineImageWriteRect(ScrnInfoPtr pScrn, int x,
				int y, int w, int h, int skipleft);
static void ImageSubsequentImageWriteScanline(ScrnInfoPtr pScrn, int bufno);
static void ImageSetupForScanlineCPUToScreenColorExpandFill(
				ScrnInfoPtr pScrn,
				int fg, int bg, int rop, 
				unsigned int planemask);
static void ImageSubsequentScanlineCPUToScreenColorExpandFill(
				ScrnInfoPtr pScrn, int x,
				int y, int w, int h, int skipleft);
static void ImageSubsequentColorExpandScanline(ScrnInfoPtr pScrn, int bufno);

static void
ImageInitializeAccelerator(ScrnInfoPtr pScrn)
{
    TRIDENTPtr pTrident = TRIDENTPTR(pScrn);

    switch (pScrn->depth) {
	case 8:
	    pTrident->EngineOperation = 0;
	    break;
	case 15:
	    pTrident->EngineOperation = 5;
	    break;
	case 16:
	    pTrident->EngineOperation = 1;
	    break;
	case 24:
	    pTrident->EngineOperation = 2;
	    break;
    }
    IMAGE_OUT(0x2120, 0xF0000000);
    IMAGE_OUT(0x2120, 0x40000000 | pTrident->EngineOperation);
    IMAGE_OUT(0x2120, 0x80000000);
    IMAGE_OUT(0x2144, 0x00000000);
    IMAGE_OUT(0x2148, 0x00000000);
    IMAGE_OUT(0x2150, 0x00000000);
    IMAGE_OUT(0x2154, 0x00000000);
    IMAGE_OUT(0x2120, 0x60000000 |pScrn->displayWidth<<16 |pScrn->displayWidth);
    IMAGE_OUT(0x216C, 0x00000000);
    IMAGE_OUT(0x2170, 0x00000000);
    IMAGE_OUT(0x217C, 0x00000000);
    IMAGE_OUT(0x2120, 0x10000000);
    IMAGE_OUT(0x2130, 2047 << 16 | 2047);
    pTrident->Clipping = FALSE;
    pTrident->DstEnable = FALSE;
}

Bool
ImageAccelInit(ScreenPtr pScreen)
{
    XAAInfoRecPtr infoPtr;
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    TRIDENTPtr pTrident = TRIDENTPTR(pScrn);

    if (pTrident->NoAccel)
	return FALSE;

    pTrident->AccelInfoRec = infoPtr = XAACreateInfoRec();
    if (!infoPtr) return FALSE;

    pTrident->InitializeAccelerator = ImageInitializeAccelerator;
    ImageInitializeAccelerator(pScrn);

    infoPtr->Flags = PIXMAP_CACHE |
		     LINEAR_FRAMEBUFFER |
		     OFFSCREEN_PIXMAPS;

    infoPtr->Sync = ImageSync;

#if 0
    infoPtr->SetClippingRectangle = ImageSetClippingRectangle;
    infoPtr->DisableClipping = ImageDisableClipping;
    infoPtr->ClippingFlags = HARDWARE_CLIP_SOLID_FILL |
			     HARDWARE_CLIP_SCREEN_TO_SCREEN_COPY |
			     HARDWARE_CLIP_MONO_8x8_FILL;
#endif

#if 0
    infoPtr->SolidLineFlags = NO_PLANEMASK;
    infoPtr->SetupForSolidLine = ImageSetupForSolidLine;
    infoPtr->SolidBresenhamLineErrorTermBits = 13;
    infoPtr->SubsequentSolidBresenhamLine = ImageSubsequentSolidBresenhamLine;
    infoPtr->ClippingFlags |= HARDWARE_CLIP_SOLID_LINE;
#endif

    infoPtr->SolidFillFlags = NO_PLANEMASK;
    infoPtr->SetupForSolidFill = ImageSetupForFillRectSolid;
    infoPtr->SubsequentSolidFillRect = ImageSubsequentFillRectSolid;
    
    infoPtr->ScreenToScreenCopyFlags = NO_PLANEMASK |
				       NO_TRANSPARENCY |
				       ONLY_TWO_BITBLT_DIRECTIONS;

    infoPtr->SetupForScreenToScreenCopy = 	
				ImageSetupForScreenToScreenCopy;
    infoPtr->SubsequentScreenToScreenCopy = 		
				ImageSubsequentScreenToScreenCopy;

    infoPtr->Mono8x8PatternFillFlags =  NO_PLANEMASK | 
					BIT_ORDER_IN_BYTE_MSBFIRST |
					HARDWARE_PATTERN_SCREEN_ORIGIN |
					HARDWARE_PATTERN_PROGRAMMED_BITS;

    infoPtr->SetupForMono8x8PatternFill =
				ImageSetupForMono8x8PatternFill;
    infoPtr->SubsequentMono8x8PatternFillRect = 
				ImageSubsequentMono8x8PatternFillRect;

#if 0
    infoPtr->Color8x8PatternFillFlags = NO_PLANEMASK | 
					NO_TRANSPARENCY |
					HARDWARE_PATTERN_SCREEN_ORIGIN | 
					BIT_ORDER_IN_BYTE_MSBFIRST;

    infoPtr->SetupForColor8x8PatternFill =
				ImageSetupForColor8x8PatternFill;
    infoPtr->SubsequentColor8x8PatternFillRect = 
				ImageSubsequentColor8x8PatternFillRect;
    infoPtr->ClippingFlags |= HARDWARE_CLIP_COLOR_8x8_FILL;
#endif

  if (pTrident->Chipset != CYBER9397DVD) {
   /* It seems as though the 9397DVD doesn't like the transfer window   */
   /* But then, I've also tried at the two port addresses too, with     */
   /* no luck. Disable for this chipset for now. I'd guess there's some */
   /* extra setup needed for this chipset.                              */
    infoPtr->ScanlineCPUToScreenColorExpandFillFlags = NO_PLANEMASK |
					LEFT_EDGE_CLIPPING |
					BIT_ORDER_IN_BYTE_MSBFIRST;

    pTrident->XAAScanlineColorExpandBuffers[0] =
	    xnfalloc(((pScrn->virtualX + 63)) *4* (pScrn->bitsPerPixel / 8));

    infoPtr->NumScanlineColorExpandBuffers = 1;
    infoPtr->ScanlineColorExpandBuffers = 
					pTrident->XAAScanlineColorExpandBuffers;

    infoPtr->SetupForScanlineCPUToScreenColorExpandFill =
			ImageSetupForScanlineCPUToScreenColorExpandFill;
    infoPtr->SubsequentScanlineCPUToScreenColorExpandFill = 
			ImageSubsequentScanlineCPUToScreenColorExpandFill;
    infoPtr->SubsequentColorExpandScanline = 
			ImageSubsequentColorExpandScanline;

    infoPtr->ScanlineImageWriteFlags = NO_PLANEMASK |
				       LEFT_EDGE_CLIPPING;

    infoPtr->SetupForScanlineImageWrite = ImageSetupForScanlineImageWrite;
    infoPtr->SubsequentScanlineImageWriteRect =
       					ImageSubsequentScanlineImageWriteRect;
    infoPtr->SubsequentImageWriteScanline = ImageSubsequentImageWriteScanline;
    
    infoPtr->NumScanlineImageWriteBuffers = 1;
    infoPtr->ScanlineImageWriteBuffers = pTrident->XAAImageScanlineBuffer;

    pTrident->XAAImageScanlineBuffer[0] = 
			xnfalloc(pScrn->virtualX * pScrn->bitsPerPixel / 8); 

    infoPtr->ImageWriteBase = pTrident->IOBase + 0x10000;
  }

    return(XAAInit(pScreen, infoPtr));
}

static void
ImageSync(ScrnInfoPtr pScrn)
{
    TRIDENTPtr pTrident = TRIDENTPTR(pScrn);
    int busy;
    int cnt = 5000000;

    if (pTrident->Clipping) ImageDisableClipping(pScrn);
    if (pTrident->DstEnable) {
	IMAGE_OUT(0x2120, 0x70000000);
	pTrident->DstEnable = FALSE;
    }

    IMAGEBUSY(busy);
    while (busy != 0) {
	if (--cnt < 0) {
	    ErrorF("GE timeout\n");
	    IMAGE_OUT(0x2164, 0x80000000);
	}
    	IMAGEBUSY(busy);
    }
}

static void
ImageSyncClip(ScrnInfoPtr pScrn)
{
    TRIDENTPtr pTrident = TRIDENTPTR(pScrn);
    int busy;
    int cnt = 5000000;

    IMAGEBUSY(busy);
    while (busy != 0) {
	if (--cnt < 0) {
	    ErrorF("GE timeout\n");
	    IMAGE_OUT(0x2164, 0x80000000);
	}
    	IMAGEBUSY(busy);
    }
}

static void
ImageSetupForScreenToScreenCopy(ScrnInfoPtr pScrn, 
				int xdir, int ydir, int rop,
				unsigned int planemask, int transparency_color)
{
    TRIDENTPtr pTrident = TRIDENTPTR(pScrn);

    pTrident->BltScanDirection = 0;
    if ((xdir < 0) || (ydir < 0)) pTrident->BltScanDirection |= 1<<2;

    IMAGE_OUT(0x2120, 0x80000000);
    IMAGE_OUT(0x2120, 0x90000000 | XAACopyROP[rop]);

    if (transparency_color != -1) {
	IMAGE_OUT(0x2120, 0x70000000 | 1<<26 | (transparency_color&0xffffff));
	pTrident->DstEnable = TRUE;
    }
}

static void
ImageSubsequentScreenToScreenCopy(ScrnInfoPtr pScrn, int x1, int y1,
					int x2, int y2, int w, int h)
{
    TRIDENTPtr pTrident = TRIDENTPTR(pScrn);

    if (pTrident->BltScanDirection) {
	IMAGE_OUT(0x2100, (y1+h-1)<<16 | (x1+w-1));
	IMAGE_OUT(0x2104, y1<<16 | x1);
	IMAGE_OUT(0x2108, (y2+h-1)<<16 | (x2+w-1));
	IMAGE_OUT(0x210C, y2<<16 | x2);
    } else {
	IMAGE_OUT(0x2100, y1<<16 | x1);
	IMAGE_OUT(0x2104, (y1+h-1)<<16 | (x1+w-1));
	IMAGE_OUT(0x2108, y2<<16 | x2);
	IMAGE_OUT(0x210C, (y2+h-1)<<16 | (x2+w-1));
    }

    IMAGE_OUT(0x2124, 0x80000000 | 1<<7 | 1<<22 | 1<<10 | pTrident->BltScanDirection | (pTrident->Clipping ? 1 : 0));

    if (!pTrident->UsePCIRetry)
    	ImageSyncClip(pScrn);
}

static void
ImageSetClippingRectangle(ScrnInfoPtr pScrn, int x1, int y1, int x2, int y2)
{
    TRIDENTPtr pTrident = TRIDENTPTR(pScrn);

    IMAGE_OUT(0x2120, 0x10000000 | ((y1&0xfff)<<16) | (x1&0xfff));
    IMAGE_OUT(0x2130, ((y2&0xfff)<<16) | (x2&0xfff));
    pTrident->Clipping = TRUE;
}

static void
ImageDisableClipping(ScrnInfoPtr pScrn)
{
    TRIDENTPtr pTrident = TRIDENTPTR(pScrn);
    pTrident->Clipping = FALSE;
}
    
#if 0
static void
ImageSetupForSolidLine(ScrnInfoPtr pScrn, int color,
					 int rop, unsigned int planemask)
{
    TRIDENTPtr pTrident = TRIDENTPTR(pScrn);

    REPLICATE(color);
    IMAGE_OUT(0x2120, 0x84000000);
    IMAGE_OUT(0x2120, 0x90000000 | XAACopyROP[rop]);
    IMAGE_OUT(0x2144, color);
}

static void 
ImageSubsequentSolidBresenhamLine( ScrnInfoPtr pScrn,
        int x, int y, int dmaj, int dmin, int e, int len, int octant)
{
    TRIDENTPtr pTrident = TRIDENTPTR(pScrn);
    int tmp;
    int D = 0, E = 0, ymajor = 0;

    IMAGE_OUT(0x2124, 0x20000000 | 3<<22 | 1<<10 | 1<<9 | (pTrident->Clipping ? 1:0));
    if (!(octant & YMAJOR)) {
    	if ((!(octant&XDECREASING)) && (!(octant&YDECREASING))) {E = 1; D = 0;}
    	if ((!(octant&XDECREASING)) && ( (octant&YDECREASING))) {E = 1; D = 1;}
    	if (( (octant&XDECREASING)) && (!(octant&YDECREASING))) {E = 1; D = 2;}
    	if (( (octant&XDECREASING)) && ( (octant&YDECREASING))) {E = 1; D = 3;}
	ymajor = 0;
    } else {
    	if ((!(octant&XDECREASING)) && (!(octant&YDECREASING))) {E = 0; D = 0;}
    	if ((!(octant&XDECREASING)) && ( (octant&YDECREASING))) {E = 0; D = 2;}
    	if (( (octant&XDECREASING)) && (!(octant&YDECREASING))) {E = 0; D = 1;}
    	if (( (octant&XDECREASING)) && ( (octant&YDECREASING))) {E = 0; D = 3;}
	ymajor = 1<<18;
    }

    if (E) { 
	tmp = x; x = y; y = tmp; 
    }
    if (D&0x02) {
    IMAGE_OUT(0x21FC, 0x20000000 | 1<<27 | 1<<19 | 1<<17 | ymajor | (x+len-1));
    } else {
    IMAGE_OUT(0x21FC, 0x20000000 | 1<<27 | 1<<19 | 1<<17 | ymajor | (y+len-1));
    }
    IMAGE_OUT(0x2100, E<<30 | (y&0xfff)<<16 | (x&0xfff));
    IMAGE_OUT(0x2104, D<<30 | (((dmaj-dmin)&0xfff) << 16) | (-dmin&0xfff));
    IMAGE_OUT(0x2108, ((-e&0xfff) << 16));

    if (!pTrident->UsePCIRetry)
    	ImageSyncClip(pScrn);
}
#endif

static void
ImageSetupForFillRectSolid(ScrnInfoPtr pScrn, int color, 
				    int rop, unsigned int planemask)
{
    TRIDENTPtr pTrident = TRIDENTPTR(pScrn);

    REPLICATE(color);
    IMAGE_OUT(0x2120, 0x80000000);
    IMAGE_OUT(0x2120, 0x90000000 | XAACopyROP[rop]);
    IMAGE_OUT(0x2144, color);
}

static void
ImageSubsequentFillRectSolid(ScrnInfoPtr pScrn, int x, int y, int w, int h)
{
    TRIDENTPtr pTrident = TRIDENTPTR(pScrn);

    if ((w<=0) || (h<=0))
	return;

    IMAGE_OUT(0x2108, ((y&0xfff)<<16) | (x&0xfff));
    IMAGE_OUT(0x210C, (((y+h-1)&0xfff)<<16) | ((x+w-1)&0xfff));
    IMAGE_OUT(0x2124, 0x80000000| 3<<22| 1<<10| 1<<9| (pTrident->Clipping?1:0));
    if (!pTrident->UsePCIRetry)
    	ImageSyncClip(pScrn);
}

static
void MoveDWORDS(
   register CARD32* dest,
   register CARD32* src,
   register int dwords )
{
     Bool extra = FALSE;
     if (dwords & 0x01) extra = TRUE;
     while(dwords & ~0x03) {
	*dest = *src;
	*(dest + 1) = *(src + 1);
	*(dest + 2) = *(src + 2);
	*(dest + 3) = *(src + 3);
	src += 4;
	dest += 4;
	dwords -= 4;
     }	
     if(!dwords) {
	if (extra) *dest = 0x00000000;
	return;
     }
     *dest = *src;
     if(dwords == 1) {
	if (extra) *(dest + 1) = 0x00000000;
	return;
     }
     *(dest + 1) = *(src + 1);
     if(dwords == 2) {
	if (extra) *(dest + 2) = 0x00000000;
	return;
     }
     *(dest + 2) = *(src + 2);
     if (extra) *(dest + 3) = 0x00000000;
}

static void 
ImageSetupForMono8x8PatternFill(ScrnInfoPtr pScrn, 
					   int patternx, int patterny, 
					   int fg, int bg, int rop,
					   unsigned int planemask)
{
    TRIDENTPtr pTrident = TRIDENTPTR(pScrn);

    IMAGE_OUT(0x2120, 0x90000000 | XAAPatternROP[rop]);
    if (bg == -1) {
	REPLICATE(fg);
	IMAGE_OUT(0x2120, 0x80000000 | 1<<27);
	IMAGE_OUT(0x2130, patternx);
	IMAGE_OUT(0x2134, patterny);
	IMAGE_OUT(0x2150, fg);
    	IMAGE_OUT(0x2154, ~fg);
    } else {
	REPLICATE(bg);
	REPLICATE(fg);
	IMAGE_OUT(0x2120, 0x80000000 | 1<<27 | 1<<26);
	IMAGE_OUT(0x2130, patternx);
	IMAGE_OUT(0x2134, patterny);
	IMAGE_OUT(0x2150, fg);
	IMAGE_OUT(0x2154, bg);
    }
}

static void 
ImageSubsequentMono8x8PatternFillRect(ScrnInfoPtr pScrn, 	
				   int patternx, int patterny,
				   int x, int y,
				   int w, int h)
{
    TRIDENTPtr pTrident = TRIDENTPTR(pScrn);

    IMAGE_OUT(0x2108, ((y&0xfff)<<16) | (x&0xfff));
    IMAGE_OUT(0x210C, (((y+h-1)&0xfff)<<16) | ((x+w-1)&0xfff));
    IMAGE_OUT(0x2124, 0x80000000 | 7<<18 | 1<<22 | 1<<10 | 1<<9 | (pTrident->Clipping ? 1 : 0));
    if (!pTrident->UsePCIRetry)
    	ImageSyncClip(pScrn);
}

#if 0
static void 
ImageSetupForColor8x8PatternFill(ScrnInfoPtr pScrn, 
					   int patternx, int patterny, 
					   int rop,
					   unsigned int planemask,
					   int transparency_color)
{
    TRIDENTPtr pTrident = TRIDENTPTR(pScrn);

    IMAGE_OUT(0x2120, 0x90000000 | XAAPatternROP[rop]);
    IMAGE_OUT(0x2120, 0x80000000 | 1<<26);
    if (transparency_color != -1) {
	IMAGE_OUT(0x2120, 0x70000000 | 1<<26 | (transparency_color&0xffffff));
	pTrident->DstEnable = TRUE;
    }
}

static void 
ImageSubsequentColor8x8PatternFillRect(ScrnInfoPtr pScrn, 	
				   int patternx, int patterny,
				   int x, int y,
				   int w, int h)
{
    TRIDENTPtr pTrident = TRIDENTPTR(pScrn);

    IMAGE_OUT(0x2100, (patterny&0xfff)<<16 | (patternx&0xfff));
    IMAGE_OUT(0x2104, (((patterny+h-1)&0xfff)<<16) | ((patternx+w-1)&0xfff));
    IMAGE_OUT(0x2108, (y&0xfff)<<16 | (x&0xfff));
    IMAGE_OUT(0x210C, (((y+h-1)&0xfff)<<16) | ((x+w-1)&0xfff));
    IMAGE_OUT(0x2124, 0x80000000 | 1<<22 | 1<<10 | 1<<7 | (pTrident->Clipping ? 1 : 0));
    if (!pTrident->UsePCIRetry)
    	ImageSyncClip(pScrn);
}
#endif

static void
ImageSetupForScanlineCPUToScreenColorExpandFill(
	ScrnInfoPtr pScrn,
	int fg, int bg, 
	int rop, 
	unsigned int planemask
){
    TRIDENTPtr pTrident = TRIDENTPTR(pScrn);

    IMAGE_OUT(0x2120, 0x80000000);
    IMAGE_OUT(0x2120, 0x90000000 | XAACopyROP[rop]);
    if (bg == -1) {
	pTrident->ROP = 2<<22;
    	REPLICATE(fg);
    	IMAGE_OUT(0x2144, fg);
    	IMAGE_OUT(0x2148, ~fg);
    } else {
	pTrident->ROP = 3<<22;
    	REPLICATE(fg);
    	IMAGE_OUT(0x2144, fg);
    	REPLICATE(bg);
    	IMAGE_OUT(0x2148, bg);
    }
}

static void
ImageSubsequentScanlineCPUToScreenColorExpandFill(
	ScrnInfoPtr pScrn,
	int x, int y, int w, int h,
	int skipleft
){
    TRIDENTPtr pTrident = TRIDENTPTR(pScrn);
    ImageSetClippingRectangle(pScrn,(x+skipleft),y,(x+w-1),(y+h-1));
    IMAGE_OUT(0x2108, (y&0xfff)<<16 | (x&0xfff));
    IMAGE_OUT(0x210C, (((y+h-1)&0xfff)<<16) | ((x+w-1)&0xfff));
    IMAGE_OUT(0x2124, 0x80000000 | pTrident->ROP | 1<<10 | 1);
    pTrident->dwords = (w + 31) >> 5;
    pTrident->h = h;
}

static void
ImageSubsequentColorExpandScanline(ScrnInfoPtr pScrn, int bufno)
{
    TRIDENTPtr pTrident = TRIDENTPTR(pScrn);
    XAAInfoRecPtr infoRec;
    infoRec = GET_XAAINFORECPTR_FROM_SCRNINFOPTR(pScrn);

    MoveDWORDS((CARD32*)infoRec->ImageWriteBase,
 	(CARD32*)pTrident->XAAScanlineColorExpandBuffers[bufno], pTrident->dwords);

    pTrident->h--;
    if (!pTrident->h)
	ImageSync(pScrn);
}

static void
ImageSetupForScanlineImageWrite(ScrnInfoPtr pScrn, int rop,
                             unsigned int planemask, int transparency_color,
                             int bpp, int depth)
{
    TRIDENTPtr pTrident = TRIDENTPTR(pScrn);
    IMAGE_OUT(0x2120, 0x90000000 | XAACopyROP[rop]);
    if (transparency_color != -1) {
	IMAGE_OUT(0x2120, 0x70000000 | 1<<26 | (transparency_color&0xffffff));
	pTrident->DstEnable = TRUE;
    }
    IMAGE_OUT(0x2120, 0x80000000);
}

static void
ImageSubsequentScanlineImageWriteRect(ScrnInfoPtr pScrn, int x, int y,
                                   int w, int h, int skipleft)
{
    TRIDENTPtr pTrident = TRIDENTPTR(pScrn);
    ImageSetClippingRectangle(pScrn,(x+skipleft),y,(x+w-1),(y+h-1));
    IMAGE_OUT(0x2108, ((y&0xfff)<<16) | (x&0xfff));
    IMAGE_OUT(0x210C, (((y+h-1)&0xfff)<<16) | ((x+w-1)&0xfff));
    IMAGE_OUT(0x2124, 0x80000000 | 1<<22 | 1<<10 | 1);
    pTrident->dwords = ((w * (pScrn->bitsPerPixel/8)) + 3) >> 2;
    pTrident->h = h;
}


static void
ImageSubsequentImageWriteScanline(ScrnInfoPtr pScrn, int bufno)
{
    TRIDENTPtr pTrident = TRIDENTPTR(pScrn);
    XAAInfoRecPtr infoRec;
    infoRec = GET_XAAINFORECPTR_FROM_SCRNINFOPTR(pScrn);

    MoveDWORDS((CARD32*)infoRec->ImageWriteBase,
 	(CARD32*)pTrident->XAAImageScanlineBuffer[bufno], pTrident->dwords);

    pTrident->h--;
    if (!pTrident->h)
	ImageSync(pScrn);
}

/*
 * Copyright 1997,1998 by Alan Hourihane, Wigan, England.
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
 * Trident Blade3D accelerated options.
 */
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/trident/blade_accel.c,v 1.18 2002/10/08 22:14:11 tsi Exp $ */

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

static void BladeSync(ScrnInfoPtr pScrn);
#if 0
static void BladeSetupForSolidLine(ScrnInfoPtr pScrn, int color,
				int rop, unsigned int planemask);
static void BladeSubsequentSolidBresenhamLine(ScrnInfoPtr pScrn,
        			int x, int y, int dmaj, int dmin, int e, 
				int len, int octant);
static void BladeSubsequentSolidTwoPointLine( ScrnInfoPtr pScrn,
        			int x1, int y1, int x2, int y2, int flags); 
static void BladeSetupForDashedLine(ScrnInfoPtr pScrn, int fg, int bg,
				int rop, unsigned int planemask, int length,
				unsigned char *pattern);
static void BladeSubsequentDashedTwoPointLine( ScrnInfoPtr pScrn,
        			int x1, int y1, int x2, int y2, int flags,
				int phase); 
#endif
static void BladeSetupForFillRectSolid(ScrnInfoPtr pScrn, int color,
				int rop, unsigned int planemask);
static void BladeSubsequentFillRectSolid(ScrnInfoPtr pScrn, int x,
				int y, int w, int h);
static void BladeSubsequentScreenToScreenCopy(ScrnInfoPtr pScrn,
				int x1, int y1, int x2,
				int y2, int w, int h);
static void BladeSetupForScreenToScreenCopy(ScrnInfoPtr pScrn,
				int xdir, int ydir, int rop, 
                                unsigned int planemask,
				int transparency_color);
#if 0
static void BladeSetupForScreenToScreenColorExpand(ScrnInfoPtr pScrn,
    				int fg, int bg, int rop,
    				unsigned int planemask);
static void BladeSubsequentScreenToScreenColorExpand(ScrnInfoPtr pScrn,
    				int x, int y, int w, int h, int srcx, int srcy,
				int offset);
#endif
static void BladeSetupForCPUToScreenColorExpand(ScrnInfoPtr pScrn,
    				int fg, int bg, int rop,
    				unsigned int planemask);
static void BladeSubsequentCPUToScreenColorExpand(ScrnInfoPtr pScrn,
				int x, int y, int w, int h, int skipleft);
static void BladeSetClippingRectangle(ScrnInfoPtr pScrn, int x1, int y1, 
				int x2, int y2);
static void BladeDisableClipping(ScrnInfoPtr pScrn);
static void BladeSetupForMono8x8PatternFill(ScrnInfoPtr pScrn, 
				int patternx, int patterny, int fg, int bg, 
				int rop, unsigned int planemask);
static void BladeSubsequentMono8x8PatternFillRect(ScrnInfoPtr pScrn, 
				int patternx, int patterny, int x, int y, 
				int w, int h);
#if 0
static void BladeSetupForColor8x8PatternFill(ScrnInfoPtr pScrn, 
				int patternx, int patterny, 
				int rop, unsigned int planemask, int trans_col);
static void BladeSubsequentColor8x8PatternFillRect(ScrnInfoPtr pScrn, 
				int patternx, int patterny, int x, int y, 
				int w, int h);
#endif
static void BladeSetupForImageWrite(ScrnInfoPtr pScrn, int rop,
   				unsigned int planemask, int transparency_color,
   				int bpp, int depth);
static void BladeSubsequentImageWriteRect(ScrnInfoPtr pScrn, int x,
   				int y, int w, int h, int skipleft);

static void
BladeInitializeAccelerator(ScrnInfoPtr pScrn)
{
    TRIDENTPtr pTrident = TRIDENTPTR(pScrn);
    CARD32 stride;
 
    stride = (pScrn->displayWidth >> 3) << 20;

    BLADE_OUT(0x21C8, stride);
    BLADE_OUT(0x21CC, stride);
    BLADE_OUT(0x21D0, stride);
    BLADE_OUT(0x21D4, stride);
    switch (pScrn->depth) {
	case 8:
	    stride |= 0<<29;
	    break;
	case 15:
	    stride |= 5<<29;
	    break;
	case 16:
	    stride |= 1<<29;
	    break;
	case 24:
	    stride |= 2<<29;
	    break;
    }
    BLADE_OUT(0x21B8, 0);
    BLADE_OUT(0x21B8, stride);
    BLADE_OUT(0x21BC, stride);
    BLADE_OUT(0x21C0, stride);
    BLADE_OUT(0x21C4, stride);
#if 0
    /* It appears that the driver sometimes misdetects the RAM type, so we
     * don't force this for now */
    if (pTrident->HasSGRAM)
    	BLADE_OUT(0x2168, 1<<26); /* Enables Block Write if available (SGRAM) */
    else
	BLADE_OUT(0x2168, 0);
#endif
    BLADE_OUT(0x216C, 0);
}

Bool
BladeAccelInit(ScreenPtr pScreen)
{
    XAAInfoRecPtr infoPtr;
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    TRIDENTPtr pTrident = TRIDENTPTR(pScrn);
    BoxRec AvailFBArea;

    AvailFBArea.x1 = 0;
    AvailFBArea.y1 = 0;
    AvailFBArea.x2 = pScrn->displayWidth;
    AvailFBArea.y2 = (pTrident->FbMapSize - 4096) / (pScrn->displayWidth *
					    pScrn->bitsPerPixel / 8);
    if (AvailFBArea.y2 > 2047) AvailFBArea.y2 = 2047;

    xf86InitFBManager(pScreen, &AvailFBArea);

    if (pTrident->NoAccel)
	return FALSE;

    pTrident->AccelInfoRec = infoPtr = XAACreateInfoRec();
    if (!infoPtr) return FALSE;

    pTrident->InitializeAccelerator = BladeInitializeAccelerator;
    BladeInitializeAccelerator(pScrn);

    infoPtr->Flags = PIXMAP_CACHE |
		     LINEAR_FRAMEBUFFER |
		     OFFSCREEN_PIXMAPS;
 
    infoPtr->Sync = BladeSync;

    infoPtr->SetClippingRectangle = BladeSetClippingRectangle;
    infoPtr->DisableClipping = BladeDisableClipping;

#if 0
    infoPtr->SolidLineFlags = 0;
    infoPtr->SetupForSolidLine = BladeSetupForSolidLine;
    infoPtr->SubsequentSolidTwoPointLine = BladeSubsequentSolidTwoPointLine;
    infoPtr->SetupForDashedLine = BladeSetupForDashedLine;
    infoPtr->SubsequentDashedTwoPointLine = BladeSubsequentDashedTwoPointLine;
    infoPtr->DashPatternMaxLength = 16;
    infoPtr->DashedLineFlags = LINE_PATTERN_LSBFIRST_LSBJUSTIFIED |
			       LINE_PATTERN_POWER_OF_2_ONLY;
#endif

    infoPtr->SolidFillFlags = NO_PLANEMASK;
    infoPtr->SetupForSolidFill = BladeSetupForFillRectSolid;
    infoPtr->SubsequentSolidFillRect = BladeSubsequentFillRectSolid;
    
    infoPtr->ScreenToScreenCopyFlags = ONLY_TWO_BITBLT_DIRECTIONS |
					NO_PLANEMASK |
					NO_TRANSPARENCY;

    infoPtr->SetupForScreenToScreenCopy = 	
				BladeSetupForScreenToScreenCopy;
    infoPtr->SubsequentScreenToScreenCopy = 		
				BladeSubsequentScreenToScreenCopy;

    infoPtr->Mono8x8PatternFillFlags = NO_PLANEMASK | NO_TRANSPARENCY |
					BIT_ORDER_IN_BYTE_MSBFIRST |
					HARDWARE_PATTERN_SCREEN_ORIGIN |
					HARDWARE_PATTERN_PROGRAMMED_BITS;

    infoPtr->SetupForMono8x8PatternFill =
				BladeSetupForMono8x8PatternFill;
    infoPtr->SubsequentMono8x8PatternFillRect = 
				BladeSubsequentMono8x8PatternFillRect;

#if 0
    infoPtr->Color8x8PatternFillFlags =  
					HARDWARE_PATTERN_SCREEN_ORIGIN | 
					BIT_ORDER_IN_BYTE_MSBFIRST;

    infoPtr->SetupForColor8x8PatternFill =
				TridentSetupForColor8x8PatternFill;
    infoPtr->SubsequentColor8x8PatternFillRect = 
				TridentSubsequentColor8x8PatternFillRect;

    infoPtr->ScreenToScreenColorExpandFillFlags = 0;

    infoPtr->SetupForScreenToScreenColorExpandFill = 	
				BladeSetupForScreenToScreenColorExpand;
    infoPtr->SubsequentScreenToScreenColorExpandFill = 		
				BladeSubsequentScreenToScreenColorExpand;
#endif

    infoPtr->CPUToScreenColorExpandFillFlags = CPU_TRANSFER_PAD_DWORD |
				LEFT_EDGE_CLIPPING |
				SYNC_AFTER_COLOR_EXPAND |
				NO_PLANEMASK |
				BIT_ORDER_IN_BYTE_MSBFIRST |
			        SCANLINE_PAD_DWORD;
    infoPtr->ColorExpandRange = 0x10000;
    infoPtr->ColorExpandBase = pTrident->IOBase + 0x10000;
    infoPtr->SetupForCPUToScreenColorExpandFill = 	
				BladeSetupForCPUToScreenColorExpand;
    infoPtr->SubsequentCPUToScreenColorExpandFill = 		
				BladeSubsequentCPUToScreenColorExpand;

    infoPtr->SetupForImageWrite = BladeSetupForImageWrite;
    infoPtr->SubsequentImageWriteRect = 
					BladeSubsequentImageWriteRect;
    infoPtr->ImageWriteFlags =  NO_PLANEMASK |
				LEFT_EDGE_CLIPPING |
				CPU_TRANSFER_PAD_DWORD |
				SYNC_AFTER_IMAGE_WRITE;
    infoPtr->ImageWriteBase = pTrident->IOBase + 0x10000;
    infoPtr->ImageWriteRange = 0x10000;

    return(XAAInit(pScreen, infoPtr));
}

static void
BladeSync(ScrnInfoPtr pScrn)
{
    TRIDENTPtr pTrident = TRIDENTPTR(pScrn);
    int busy;
    int cnt = 10000000;

    if (pTrident->Clipping) BladeDisableClipping(pScrn);
    BLADE_OUT(0x216C, 0);

    BLADEBUSY(busy);
    while (busy != 0) {
	if (--cnt < 0) {
	    ErrorF("GE timeout\n");
	    BLADE_OUT(0x2124, 1<<7);
	    BLADE_OUT(0x2124, 0);
	    break;
	}
    	BLADEBUSY(busy);
    }
}

static void
BladeSetupForScreenToScreenCopy(ScrnInfoPtr pScrn, 
				int xdir, int ydir, int rop,
				unsigned int planemask, int transparency_color)
{
    TRIDENTPtr pTrident = TRIDENTPTR(pScrn);

    pTrident->BltScanDirection = 0;
    if ((xdir < 0) || (ydir < 0)) pTrident->BltScanDirection |= 1<<1;

#if 0
    if (transparency_color != -1) {
	BLADE_OUT(0x2168, transparency_color & 0xffffff);
	pTrident->BltScanDirection |= 1<<6;
    }
#endif
 
    REPLICATE(planemask);
    if (planemask != -1) {
	BLADE_OUT(0x2184, ~planemask);
	pTrident->BltScanDirection |= 1<<5;
    }
    BLADE_OUT(0x2148, XAACopyROP[rop]);
}

static void
BladeSubsequentScreenToScreenCopy(ScrnInfoPtr pScrn, int x1, int y1,
					int x2, int y2, int w, int h)
{
    TRIDENTPtr pTrident = TRIDENTPTR(pScrn);
    int clip = 0;

    if (pTrident->Clipping) clip = 1;

    BLADE_OUT(0x2144, 0xE0000000 | 1<<19 | 1<<4 | 1<<2 | pTrident->BltScanDirection | clip);

    if (pTrident->BltScanDirection) {
	BLADE_OUT(0x2100, (y1+h-1)<<16 | (x1+w-1));
	BLADE_OUT(0x2104, y1<<16 | x1);
	BLADE_OUT(0x2108, (y2+h-1)<<16 | (x2+w-1));
	BLADE_OUT(0x210C, (y2&0xfff)<<16 | (x2&0xfff));
    } else {
	BLADE_OUT(0x2100, y1<<16 | x1);
	BLADE_OUT(0x2104, (y1+h-1)<<16 | (x1+w-1));
	BLADE_OUT(0x2108, y2<<16 | x2);
	BLADE_OUT(0x210C, ((y2+h-1)&0xfff)<<16 | ((x2+w-1)&0xfff));
    }
}

static void
BladeSetClippingRectangle(ScrnInfoPtr pScrn, int x1, int y1, int x2, int y2)
{
    TRIDENTPtr pTrident = TRIDENTPTR(pScrn);

    BLADE_OUT(0x2154, (y1&0x0fff)<<16 | (x1&0x0fff));
    BLADE_OUT(0x2158, (y2&0x0fff)<<16 | (x2&0x0fff));
    pTrident->Clipping = TRUE;
}

static void
BladeDisableClipping(ScrnInfoPtr pScrn)
{
    TRIDENTPtr pTrident = TRIDENTPTR(pScrn);
    pTrident->Clipping = FALSE;
}
    
#if 0
static void
BladeSetupForSolidLine(ScrnInfoPtr pScrn, int color,
					 int rop, unsigned int planemask)
{
    TRIDENTPtr pTrident = TRIDENTPTR(pScrn);

    REPLICATE(color);
    BLADE_OUT(0x2160, color);
    BLADE_OUT(0x2148, XAACopyROP[rop]);
    pTrident->BltScanDirection = 0;
    REPLICATE(planemask);
    if (planemask != -1) {
	BLADE_OUT(0x2184, ~planemask);
	pTrident->BltScanDirection |= 1<<5;
    }
}

static void 
BladeSubsequentSolidBresenhamLine( ScrnInfoPtr pScrn,
        int x, int y, int dmaj, int dmin, int e, int len, int octant)
{
    TRIDENTPtr pTrident = TRIDENTPTR(pScrn);
    int tmp;
    int D = 0, E = 0, ymajor = 0;


    BLADE_OUT(0x2144, 0x20000000 | 3<<19 | 1<<4 | 2<<2 | (pTrident->Clipping ? 1: 0));

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
	ymajor = 1<<21;
    }

    if (E) { 
	tmp = x; x = y; y = tmp; 
    }
    BLADE_OUT(0x2130, 0x00000001);
    if (D&0x02) {
    BLADE_OUT(0x213C, 0x10000000 | 1<<25 | 1<<19 | 1<<17 | ymajor | ((x+len-1)<<4));
    } else {
    BLADE_OUT(0x213C, 0x10000000 | 1<<25 | 1<<19 | 1<<17 | ymajor | ((y+len-1)<<4));
    }
    BLADE_OUT(0x2140, E<<30 | (y&0xfff)<<20 | ((x&0xfff)<<4));
    BLADE_OUT(0x2144, D<<30 | (((dmaj-dmin)&0xfff) << 16) | (-dmin&0xfff));
    BLADE_OUT(0x2148, ((-(dmin+e)&0xfff) << 16));
}

static void 
BladeSubsequentSolidTwoPointLine( ScrnInfoPtr pScrn,
        int x1, int y1, int x2, int y2, int flags)
{
    TRIDENTPtr pTrident = TRIDENTPTR(pScrn);

#if 0
    if (flags & OMIT_LAST)
	BladeSetClippingRectangle(pScrn,x1,y1,x2-1,y2-1);
#endif

    BLADE_OUT(0x2144, 0x20000000 | pTrident->BltScanDirection | 1<<19 | 1<<4 | 2<<2);
    BLADE_OUT(0x2130, 0x3);
    BLADE_OUT(0x2108, y1<<16 | x1);
    BLADE_OUT(0x210C, (y2&0xfff)<<16 | (x2&0xfff));

#if 0
    if (flags & OMIT_LAST)
	BladeDisableClipping(pScrn);
#endif
}

static void
BladeSetupForDashedLine(ScrnInfoPtr pScrn, int fg, int bg, int rop,
		unsigned int planemask, int length, unsigned char *pattern)
{
    TRIDENTPtr pTrident = TRIDENTPTR(pScrn);

    pTrident->LinePattern = *((CARD16 *)pattern) & ((1<<length) - 1);
    switch (length) {
	case 2:
	    pTrident->LinePattern = pTrident->LinePattern | (pTrident->LinePattern << 2);
	case 4:
	    pTrident->LinePattern = pTrident->LinePattern | (pTrident->LinePattern << 4);
	case 8:
	    pTrident->LinePattern = pTrident->LinePattern | (pTrident->LinePattern << 8);
    }

    REPLICATE(fg);
    REPLICATE(bg);
    BLADE_OUT(0x2160, fg);
    BLADE_OUT(0x2164, bg);
    BLADE_OUT(0x2148, XAACopyROP[rop]);
    pTrident->BltScanDirection = 0;
    REPLICATE(planemask);
    if (planemask != -1) {
	BLADE_OUT(0x2184, ~planemask);
	pTrident->BltScanDirection |= 1<<5;
    }
}

static void 
BladeSubsequentDashedTwoPointLine( ScrnInfoPtr pScrn,
        int x1, int y1, int x2, int y2, int flags, int phase)
{
    TRIDENTPtr pTrident = TRIDENTPTR(pScrn);

    if (flags & OMIT_LAST)
	BladeSetClippingRectangle(pScrn,x1,y1,x2-1,y2-1);

    BLADE_OUT(0x216C, (pTrident->LinePattern >> phase) | (pTrident->LinePattern << (16-phase)));
    BLADE_OUT(0x2144, 0x20000000 | pTrident->BltScanDirection | 1<<27 | 1<<19 | 1<<4 | 2<<2);
    BLADE_OUT(0x2108, y1<<16 | x1);
    BLADE_OUT(0x210C, (y2&0xfff)<<16 | (x2&0xfff));

    if (flags & OMIT_LAST)
	BladeDisableClipping(pScrn);
}
#endif

static void
BladeSetupForFillRectSolid(ScrnInfoPtr pScrn, int color, 
				    int rop, unsigned int planemask)
{
    TRIDENTPtr pTrident = TRIDENTPTR(pScrn);

    REPLICATE(color);
    BLADE_OUT(0x2160, color);
    BLADE_OUT(0x2148, XAACopyROP[rop]);
    pTrident->BltScanDirection = 0;
    REPLICATE(planemask);
    if (planemask != -1) {
	BLADE_OUT(0x2184, ~planemask);
	pTrident->BltScanDirection |= 1<<5;
    }
}

static void
BladeSubsequentFillRectSolid(ScrnInfoPtr pScrn, int x, int y, int w, int h)
{
    TRIDENTPtr pTrident = TRIDENTPTR(pScrn);

    BLADE_OUT(0x2144, 0x20000000 | pTrident->BltScanDirection | 1<<19 | 1<<4 | 2<<2 | (pTrident->Clipping ? 1:0));
    BLADE_OUT(0x2108, y<<16 | x);
    BLADE_OUT(0x210C, ((y+h-1)&0xfff)<<16 | ((x+w-1)&0xfff));
}

#if 0
static void 
BladeSetupForScreenToScreenColorExpand(ScrnInfoPtr pScrn,
    int fg, int bg, int rop,
    unsigned int planemask)
{
    TRIDENTPtr pTrident = TRIDENTPTR(pScrn);

    pTrident->ROP = rop;

    REPLICATE(bg);
    REPLICATE(fg);
    IMAGE_OUT(0x44, fg);
    IMAGE_OUT(0x48, bg);
    IMAGE_OUT(0x20, 0x90000000 | XAACopyROP[rop]);
    pTrident->BltScanDirection = 0;
    REPLICATE(planemask);
    if (planemask != -1) {
	BLADE_OUT(0x2184, ~planemask);
	pTrident->BltScanDirection |= 1<<5;
    }
}

static void 
BladeSubsequentScreenToScreenColorExpand(ScrnInfoPtr pScrn,
    int x, int y, int w, int h, int srcx, int srcy, int offset)
{
    TRIDENTPtr pTrident = TRIDENTPTR(pScrn);

	IMAGE_OUT(0x00, srcy<<16 | srcx);
	IMAGE_OUT(0x04, (srcy+h-1)<<16 | (srcx+w-1));
	IMAGE_OUT(0x08, y<<16 | x);
	IMAGE_OUT(0x0C, (y+h-1)<<16 | (x+w-1));

    IMAGE_OUT(0x24, 0x80000000 | 3<<22 | 1<<7 | pTrident->BltScanDirection | (pTrident->ROP == GXcopy ? 0 : 1<<10) | offset<<25);
}
#endif

static void 
BladeSetupForCPUToScreenColorExpand(ScrnInfoPtr pScrn,
    int fg, int bg, int rop,
    unsigned int planemask)
{
    TRIDENTPtr pTrident = TRIDENTPTR(pScrn);

    pTrident->BltScanDirection = 0;
    BLADE_OUT(0x2148, XAACopyROP[rop]);
    if (bg == -1) {
	pTrident->BltScanDirection |= 2<<19;
    	REPLICATE(fg);
    	BLADE_OUT(0x2160, fg);
    	BLADE_OUT(0x2164, ~fg);
    } else { 
	pTrident->BltScanDirection |= 3<<19;
    	REPLICATE(fg);
    	REPLICATE(bg);
    	BLADE_OUT(0x2160, fg);
    	BLADE_OUT(0x2164, bg);
    }
    REPLICATE(planemask);
    if (planemask != -1) {
	BLADE_OUT(0x2184, ~planemask);
	pTrident->BltScanDirection |= 1<<5;
    }
}

static void
BladeSubsequentCPUToScreenColorExpand(ScrnInfoPtr pScrn,
	int x, int y, int w, int h, int skipleft)
{
    TRIDENTPtr pTrident = TRIDENTPTR(pScrn);

    if (skipleft) BladeSetClippingRectangle(pScrn,x+skipleft,y,(x+w-1),(y+h-1));
    BLADE_OUT(0x2144, 0xE0000000 | pTrident->BltScanDirection | 1<<4 | (skipleft ? 1 : 0));
    BLADE_OUT(0x2108, (y&0xfff)<<16 | (x&0xfff));
    BLADE_OUT(0x210C, ((y+h-1)&0xfff)<<16 | ((x+w-1)&0xfff));
}

static void 
BladeSetupForMono8x8PatternFill(ScrnInfoPtr pScrn, 
					   int patternx, int patterny, 
					   int fg, int bg, int rop,
					   unsigned int planemask)
{
    TRIDENTPtr pTrident = TRIDENTPTR(pScrn);

    BladeSync(pScrn);
    BLADE_OUT(0x2148, XAAPatternROP[rop]);

    if (bg == -1) {
    REPLICATE(fg);
    BLADE_OUT(0x216C, 0x80000000 | 1<<30);
    BLADE_OUT(0x216C, 0x80000000 | 1<<28 | 1<<30);
    BLADE_OUT(0x2170, patternx);
    BLADE_OUT(0x2170, patterny);
    BLADE_OUT(0x2174, fg);
#if 0
    BLADE_OUT(0x2178, ~fg);
#endif
    } else {
    REPLICATE(fg);
    REPLICATE(bg);
    BLADE_OUT(0x216C, 0x80000000);
    BLADE_OUT(0x216C, 0x80000000 | 1<<28);
    BLADE_OUT(0x2170, patternx);
    BLADE_OUT(0x2170, patterny);
    BLADE_OUT(0x2174, fg);
    BLADE_OUT(0x2178, bg);
    }
    pTrident->BltScanDirection = 0;
    REPLICATE(planemask);
    if (planemask != -1) {
	BLADE_OUT(0x2184, ~planemask);
	pTrident->BltScanDirection |= 1<<5;
    }
}

static void 
BladeSubsequentMono8x8PatternFillRect(ScrnInfoPtr pScrn, 	
				   int patternx, int patterny,
				   int x, int y,
				   int w, int h)
{
    TRIDENTPtr pTrident = TRIDENTPTR(pScrn);
    int clip = 0;

    if (pTrident->Clipping) clip = 1;
    BLADE_OUT(0x2144, 0x20000000 | pTrident->BltScanDirection | 7<<12 | 1<<4 | 1<<19 | 2<<2 | clip);
    BLADE_OUT(0x2108, y<<16 | x);
    BLADE_OUT(0x210C, ((y+h-1)&0xfff)<<16 | ((x+w-1)&0xfff));
}

#if 0
static void 
BladeSetupForColor8x8PatternFill(ScrnInfoPtr pScrn, 
					   int patternx, int patterny, 
					   int rop,
					   unsigned int planemask,
					   int transparency_color)
{
    TRIDENTPtr pTrident = TRIDENTPTR(pScrn);

    TGUI_PATLOC(((patterny * pScrn->displayWidth * pScrn->bitsPerPixel / 8) +
		 (patternx * pScrn->bitsPerPixel / 8)) >> 6);
    pTrident->BltScanDirection = 0;
    if (transparency_color != -1) {
	BLADE_OUT(0x2168, transparency_color & 0xffffff);
	pTrident->BltScanDirection |= 1<<6;
    }
    TGUI_FMIX(XAAPatternROP[rop]);
    REPLICATE(planemask);
    if (planemask != -1) {
	BLADE_OUT(0x2184, ~planemask);
	pTrident->BltScanDirection |= 1<<5;
    }
}

static void 
BladeSubsequentColor8x8PatternFillRect(ScrnInfoPtr pScrn, 	
				   int patternx, int patterny,
				   int x, int y,
				   int w, int h)
{
    TRIDENTPtr pTrident = TRIDENTPTR(pScrn);
  
    TGUI_DEST_XY(x,y);
    TGUI_DIM_XY(w,h);
    TGUI_COMMAND(GE_BLT);
    CHECKCLIPPING;
}
#endif

static void BladeSetupForImageWrite(	
   ScrnInfoPtr pScrn,
   int rop,
   unsigned int planemask,
   int transparency_color,
   int bpp, int depth
){
    TRIDENTPtr pTrident = TRIDENTPTR(pScrn);

    BLADE_OUT(0x2148, XAACopyROP[rop]);
    pTrident->BltScanDirection = 0;
    REPLICATE(planemask);
    if (planemask != -1) {
	BLADE_OUT(0x2184, ~planemask);
	pTrident->BltScanDirection |= 1<<5;
    }
}

static void BladeSubsequentImageWriteRect(
   ScrnInfoPtr pScrn,
   int x, int y, int w, int h,
   int skipleft
){
    TRIDENTPtr pTrident = TRIDENTPTR(pScrn);

    if (skipleft) BladeSetClippingRectangle(pScrn,x+skipleft,y,(x+w-1),(y+h-1));
    BLADE_OUT(0x2144, 0xE0000000 | 1<<19 | 1<<4 | pTrident->BltScanDirection | (skipleft ? 1 : 0));
    BLADE_OUT(0x2108, y<<16 | (x&0xfff));
    BLADE_OUT(0x210C, ((y+h-1)&0xfff)<<16 | ((x+w-1)&0xfff));
}

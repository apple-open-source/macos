/*
 * Copyright 1992-2002 by Alan Hourihane, Sychdyn, North Wales, UK.
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
 * BladeXP accelerated options.
 */
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/trident/xp_accel.c,v 1.4 2002/10/09 16:38:20 tsi Exp $ */

#include "xf86.h"
#include "xf86_OSproc.h"
#include "xf86_ansic.h"

#include "xf86PciInfo.h"
#include "xf86Pci.h"

#include "miline.h"

#include "trident.h"
#include "trident_regs.h"

#include "xaalocal.h"
#include "xaarop.h"

static void XPSync(ScrnInfoPtr pScrn);
#if 0
static void XPSetupForDashedLine(ScrnInfoPtr pScrn, int fg, int bg, 
				int rop, unsigned int planemask, int length,
    				unsigned char *pattern);
static void XPSubsequentDashedBresenhamLine(ScrnInfoPtr pScrn,
        			int x, int y, int dmaj, int dmin, int e, 
				int len, int octant, int phase);
static void XPSetupForSolidLine(ScrnInfoPtr pScrn, int color,
				int rop, unsigned int planemask);
static void XPSubsequentSolidBresenhamLine(ScrnInfoPtr pScrn,
        			int x, int y, int dmaj, int dmin, int e, 
				int len, int octant);
#endif
static void XPSubsequentSolidHorVertLine(ScrnInfoPtr pScrn, int x, int y,
    				int len, int dir);
static void XPSetupForFillRectSolid(ScrnInfoPtr pScrn, int color,
				int rop, unsigned int planemask);
static void XPSubsequentFillRectSolid(ScrnInfoPtr pScrn, int x,
				int y, int w, int h);
static void XPSubsequentScreenToScreenCopy(ScrnInfoPtr pScrn,
				int x1, int y1, int x2,
				int y2, int w, int h);
static void XPSetupForScreenToScreenCopy(ScrnInfoPtr pScrn,
				int xdir, int ydir, int rop, 
                                unsigned int planemask,
				int transparency_color);
static void XPSetupForMono8x8PatternFill(ScrnInfoPtr pScrn, 
				int patternx, int patterny, int fg, int bg, 
				int rop, unsigned int planemask);
static void XPSubsequentMono8x8PatternFillRect(ScrnInfoPtr pScrn, 
				int patternx, int patterny, int x, int y, 
				int w, int h);
#if 0
static void XPSetupForScanlineCPUToScreenColorExpandFill(
				ScrnInfoPtr pScrn,
				int fg, int bg, int rop, 
				unsigned int planemask);
static void XPSubsequentScanlineCPUToScreenColorExpandFill(
				ScrnInfoPtr pScrn, int x,
				int y, int w, int h, int skipleft);
static void XPSubsequentColorExpandScanline(ScrnInfoPtr pScrn, int bufno);
#endif

static void
XPInitializeAccelerator(ScrnInfoPtr pScrn)
{
    TRIDENTPtr pTrident = TRIDENTPTR(pScrn);
    int shift;

    /* This forces updating the clipper */
    pTrident->Clipping = TRUE;

    CHECKCLIPPING;

    BLADE_XP_OPERMODE(pTrident->EngineOperation);
    pTrident->EngineOperation |= 0x40;
    switch (pScrn->bitsPerPixel) {
	case 8:
	default:		/* Muffle compiler */
		shift = 18;
		break;
	case 16:
		shift = 19;
		break;
	case 32:
		shift = 20;
		break;
    }
    MMIO_OUT32(pTrident->IOBase, 0x2154, (pScrn->displayWidth) << shift);
    MMIO_OUT32(pTrident->IOBase, 0x2150, (pScrn->displayWidth) << shift);
    MMIO_OUT8(pTrident->IOBase, 0x2126, 3);
}

Bool
XPAccelInit(ScreenPtr pScreen)
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

    infoPtr->Flags = PIXMAP_CACHE |
		     OFFSCREEN_PIXMAPS |
		     LINEAR_FRAMEBUFFER;

    pTrident->InitializeAccelerator = XPInitializeAccelerator;
    XPInitializeAccelerator(pScrn);

    infoPtr->Sync = XPSync;

#if 0 /* TO DO for the XP */
    infoPtr->SolidLineFlags = NO_PLANEMASK;
    infoPtr->SetupForSolidLine = XPSetupForSolidLine;
    infoPtr->SolidBresenhamLineErrorTermBits = 12;
    infoPtr->SubsequentSolidBresenhamLine = XPSubsequentSolidBresenhamLine;

    infoPtr->DashedLineFlags = LINE_PATTERN_MSBFIRST_LSBJUSTIFIED |
			       NO_PLANEMASK |
			       LINE_PATTERN_POWER_OF_2_ONLY;
    infoPtr->SetupForDashedLine = XPSetupForDashedLine;
    infoPtr->DashedBresenhamLineErrorTermBits = 12;
    infoPtr->SubsequentDashedBresenhamLine = 
					XPSubsequentDashedBresenhamLine;  
    infoPtr->DashPatternMaxLength = 16;
#endif

    infoPtr->SolidFillFlags = NO_PLANEMASK;
    infoPtr->SetupForSolidFill = XPSetupForFillRectSolid;
    infoPtr->SubsequentSolidFillRect = XPSubsequentFillRectSolid;
    infoPtr->SubsequentSolidHorVertLine = XPSubsequentSolidHorVertLine;
    
    infoPtr->ScreenToScreenCopyFlags = NO_PLANEMASK | NO_TRANSPARENCY;

    infoPtr->SetupForScreenToScreenCopy = 	
				XPSetupForScreenToScreenCopy;
    infoPtr->SubsequentScreenToScreenCopy = 		
				XPSubsequentScreenToScreenCopy;

    infoPtr->Mono8x8PatternFillFlags =  NO_PLANEMASK | 
					HARDWARE_PATTERN_PROGRAMMED_BITS |
					BIT_ORDER_IN_BYTE_MSBFIRST;

    infoPtr->SetupForMono8x8PatternFill =
				XPSetupForMono8x8PatternFill;
    infoPtr->SubsequentMono8x8PatternFillRect = 
				XPSubsequentMono8x8PatternFillRect;

#if 0 /* Needs fixing */
    infoPtr->ScanlineCPUToScreenColorExpandFillFlags = NO_PLANEMASK |
					BIT_ORDER_IN_BYTE_MSBFIRST;

    pTrident->XAAScanlineColorExpandBuffers[0] =
	    xnfalloc(((pScrn->virtualX + 63)) *4* (pScrn->bitsPerPixel / 8));

    infoPtr->NumScanlineColorExpandBuffers = 1;
    infoPtr->ScanlineColorExpandBuffers = 
					pTrident->XAAScanlineColorExpandBuffers;

    infoPtr->SetupForScanlineCPUToScreenColorExpandFill =
			XPSetupForScanlineCPUToScreenColorExpandFill;
    infoPtr->SubsequentScanlineCPUToScreenColorExpandFill = 
			XPSubsequentScanlineCPUToScreenColorExpandFill;
    infoPtr->SubsequentColorExpandScanline = 
			XPSubsequentColorExpandScanline;
#endif

    return(XAAInit(pScreen, infoPtr));
}

static void
XPSync(ScrnInfoPtr pScrn)
{
    TRIDENTPtr pTrident = TRIDENTPTR(pScrn);
    int count = 0, timeout = 0;
    int busy;

    BLADE_XP_OPERMODE(pTrident->EngineOperation);

    for (;;) {
	BLTBUSY(busy);
	if (busy != GE_BUSY) {
	    return;
	}
	count++;
	if (count == 10000000) {
	    ErrorF("XP: BitBLT engine time-out.\n");
	    count = 9990000;
	    timeout++;
	    if (timeout == 8) {
		/* Reset BitBLT Engine */
		TGUI_STATUS(0x00);
		return;
	    }
	}
    }
}

static void
XPClearSync(ScrnInfoPtr pScrn)
{
    TRIDENTPtr pTrident = TRIDENTPTR(pScrn);
    int count = 0, timeout = 0;
    int busy;

    for (;;) {
	BLTBUSY(busy);
	if (busy != GE_BUSY) {
	    return;
	}
	count++;
	if (count == 10000000) {
	    ErrorF("XP: BitBLT engine time-out.\n");
	    count = 9990000;
	    timeout++;
	    if (timeout == 8) {
		/* Reset BitBLT Engine */
		TGUI_STATUS(0x00);
		return;
	    }
	}
    }
}

static void
XPSetupForScreenToScreenCopy(ScrnInfoPtr pScrn, 
				int xdir, int ydir, int rop,
				unsigned int planemask, int transparency_color)
{
    TRIDENTPtr pTrident = TRIDENTPTR(pScrn);
    int dst = 0;

    pTrident->BltScanDirection = 0;
    if (xdir < 0) pTrident->BltScanDirection |= XNEG;
    if (ydir < 0) pTrident->BltScanDirection |= YNEG;

    REPLICATE(transparency_color);
    if (transparency_color != -1) {
	dst |= 3<<16;
    	MMIO_OUT32(pTrident->IOBase, 0x2134, transparency_color);
    }

    TGUI_DRAWFLAG(pTrident->BltScanDirection | SCR2SCR | dst);
    TGUI_FMIX(XAACopyROP[rop]);
}

static void
XPSubsequentScreenToScreenCopy(ScrnInfoPtr pScrn, int x1, int y1,
					int x2, int y2, int w, int h)
{
    TRIDENTPtr pTrident = TRIDENTPTR(pScrn);

    if (pTrident->BltScanDirection & YNEG) {
        y1 = y1 + h - 1;
	y2 = y2 + h - 1;
    }
    if (pTrident->BltScanDirection & XNEG) {
	x1 = x1 + w - 1;
	x2 = x2 + w - 1;
    }
    XP_SRC_XY(x1,y1);
    XP_DEST_XY(x2,y2);
    XP_DIM_XY(w,h);
    TGUI_COMMAND(GE_BLT);
    XPClearSync(pScrn);
}

#if 0
static void
XPSetupForSolidLine(ScrnInfoPtr pScrn, int color,
					 int rop, unsigned int planemask)
{
    TRIDENTPtr pTrident = TRIDENTPTR(pScrn);

    pTrident->BltScanDirection = 0;
    REPLICATE(color);
    TGUI_FMIX(XAAPatternROP[rop]);
    if (pTrident->Chipset >= PROVIDIA9685) {
    	TGUI_FPATCOL(color);
    } else {
    	TGUI_FCOLOUR(color);
    }
}

static void 
XPSubsequentSolidBresenhamLine( ScrnInfoPtr pScrn,
        int x, int y, int dmaj, int dmin, int e, int len, int octant)
{
    TRIDENTPtr pTrident = TRIDENTPTR(pScrn);
    int tmp = pTrident->BltScanDirection;

    if (octant & YMAJOR) tmp |= YMAJ;
    if (octant & XDECREASING) tmp |= XNEG;
    if (octant & YDECREASING) tmp |= YNEG;
    TGUI_DRAWFLAG(SOLIDFILL | STENCIL | tmp);
    XP_SRC_XY(dmin-dmaj,dmin);
    XP_DEST_XY(x,y);
    XP_DIM_XY(dmin+e,len);
    TGUI_COMMAND(GE_BRESLINE);
    XPSync(pScrn);
}
#endif

static void 
XPSubsequentSolidHorVertLine(
    ScrnInfoPtr pScrn,
    int x, int y, 
    int len, int dir
){
    TRIDENTPtr pTrident = TRIDENTPTR(pScrn);

    TGUI_DRAWFLAG(SOLIDFILL);
    if (dir == DEGREES_0) {
    	XP_DIM_XY(len,1);
    	XP_DEST_XY(x,y);
    } else {
    	XP_DIM_XY(1,len);
    	XP_DEST_XY(x,y);
    }
    TGUI_COMMAND(GE_BLT);
    XPSync(pScrn);
}

#if 0
void
XPSetupForDashedLine(
    ScrnInfoPtr pScrn, 
    int fg, int bg, int rop,
    unsigned int planemask,
    int length,
    unsigned char *pattern
){
    TRIDENTPtr pTrident = TRIDENTPTR(pScrn);
    CARD32 *DashPattern = (CARD32*)pattern;
    CARD32 NiceDashPattern = DashPattern[0];

    NiceDashPattern = *((CARD16 *)pattern) & ((1<<length) - 1);
    switch(length) {
	case 2:	NiceDashPattern |= NiceDashPattern << 2;
	case 4:	NiceDashPattern |= NiceDashPattern << 4;
	case 8:	NiceDashPattern |= NiceDashPattern << 8;
    }
    pTrident->BltScanDirection = 0;
    REPLICATE(fg);
    if (pTrident->Chipset >= PROVIDIA9685) {
	TGUI_FPATCOL(fg);
    	if (bg == -1) {
	    pTrident->BltScanDirection |= 1<<12;
    	    TGUI_BPATCOL(~fg);
    	} else {
    	    REPLICATE(bg);
    	    TGUI_BPATCOL(bg);
    	}
    } else {
    	TGUI_FCOLOUR(fg);
    	if (bg == -1) {
	    pTrident->BltScanDirection |= 1<<12;
    	    TGUI_BCOLOUR(~fg);
    	} else {
    	    REPLICATE(bg);
    	    TGUI_BCOLOUR(bg);
    	}
    }
    TGUI_FMIX(XAAPatternROP[rop]);
    pTrident->LinePattern = NiceDashPattern;
}

void
XPSubsequentDashedBresenhamLine(ScrnInfoPtr pScrn,
        int x, int y, int dmaj, int dmin, int e, int len, int octant, int phase)
{
    TRIDENTPtr pTrident = TRIDENTPTR(pScrn);
    int tmp = pTrident->BltScanDirection;

    if (octant & YMAJOR) tmp |= YMAJ;
    if (octant & XDECREASING) tmp |= XNEG;
    if (octant & YDECREASING) tmp |= YNEG;

    TGUI_STYLE(((pTrident->LinePattern >> phase) | 
		(pTrident->LinePattern << (16-phase))) & 0x0000FFFF);
    TGUI_DRAWFLAG(STENCIL | tmp);
    XP_SRC_XY(dmin-dmaj,dmin);
    XP_DEST_XY(x,y);
    XP_DIM_XY(e+dmin,len);
    TGUI_COMMAND(GE_BRESLINE);
    XPSync(pScrn);
}
#endif

static void
XPSetupForFillRectSolid(ScrnInfoPtr pScrn, int color, 
				    int rop, unsigned int planemask)
{
    TRIDENTPtr pTrident = TRIDENTPTR(pScrn);

    REPLICATE(color);
    TGUI_FMIX(XAAPatternROP[rop]);
    MMIO_OUT32(pTrident->IOBase, 0x2158, color);
    TGUI_DRAWFLAG(SOLIDFILL);
}

static void
XPSubsequentFillRectSolid(ScrnInfoPtr pScrn, int x, int y, int w, int h)
{
    TRIDENTPtr pTrident = TRIDENTPTR(pScrn);

    XP_DIM_XY(w,h);
    XP_DEST_XY(x,y);
    TGUI_COMMAND(GE_BLT);
    XPSync(pScrn);
}

#if 0
static void MoveDWORDS(
   register CARD32* dest,
   register CARD32* src,
   register int dwords )
{
     while(dwords & ~0x03) {
	*dest = *src;
	*(dest + 1) = *(src + 1);
	*(dest + 2) = *(src + 2);
	*(dest + 3) = *(src + 3);
	src += 4;
	dest += 4;
	dwords -= 4;
     }	
     if (!dwords) return;
     *dest = *src;
     dest += 1;
     src += 1;
     if (dwords == 1) return;
     *dest = *src;
     dest += 1;
     src += 1;
     if (dwords == 2) return;
     *dest = *src;
     dest += 1;
     src += 1;
}
#endif

#if 0
static void MoveDWORDS_FixedBase(
   register CARD32* dest,
   register CARD32* src,
   register int dwords )
{
     while(dwords & ~0x03) {
	 *dest = *src;
	 *dest = *(src + 1);
	 *dest = *(src + 2);
	 *dest = *(src + 3);	
	 dwords -= 4;
	 src += 4;
     }

     if(!dwords) return;
     *dest = *src;
     if(dwords == 1) return;
     *dest = *(src + 1);
     if(dwords == 2) return;
     *dest = *(src + 2);
}
#endif


static void 
XPSetupForMono8x8PatternFill(ScrnInfoPtr pScrn, 
					   int patternx, int patterny, 
					   int fg, int bg, int rop,
					   unsigned int planemask)
{
    TRIDENTPtr pTrident = TRIDENTPTR(pScrn);
    int drawflag = 0;

    REPLICATE(fg);
    MMIO_OUT32(pTrident->IOBase, 0x2158, fg);

    if (bg == -1) {
	drawflag |= 1<<12;
   	MMIO_OUT32(pTrident->IOBase, 0x215C, ~fg);
    } else {
    	REPLICATE(bg);
   	MMIO_OUT32(pTrident->IOBase, 0x215C, bg);
    }

    drawflag |= 7<<18;
    TGUI_DRAWFLAG(PATMONO | drawflag);
    MMIO_OUT32(pTrident->IOBase, 0x2180, patternx);
    MMIO_OUT32(pTrident->IOBase, 0x2184, patterny);
    TGUI_FMIX(XAAPatternROP[rop]);
}

static void 
XPSubsequentMono8x8PatternFillRect(ScrnInfoPtr pScrn, 	
				   int patternx, int patterny,
				   int x, int y,
				   int w, int h)
{
    TRIDENTPtr pTrident = TRIDENTPTR(pScrn);
  
    XP_DEST_XY(x,y);
    XP_DIM_XY(w,h);
    TGUI_COMMAND(GE_BLT);
    XPSync(pScrn);
}

#if 0
static void
XPSetupForScanlineCPUToScreenColorExpandFill(
	ScrnInfoPtr pScrn,
	int fg, int bg, 
	int rop, 
	unsigned int planemask
){
    TRIDENTPtr pTrident = TRIDENTPTR(pScrn);

    TGUI_FMIX(XAACopyROP[rop]);
    if (bg == -1) {
    TGUI_DRAWFLAG(SRCMONO | 1<<12);
    	REPLICATE(fg);
	TGUI_FCOLOUR(fg);
    } else { 
    TGUI_DRAWFLAG(SRCMONO);
    	REPLICATE(fg);
    	REPLICATE(bg);
	TGUI_FCOLOUR(fg);
	TGUI_BCOLOUR(bg);
    }
}

static void
XPSubsequentScanlineCPUToScreenColorExpandFill(
	ScrnInfoPtr pScrn,
	int x, int y, int w, int h,
	int skipleft
){
    TRIDENTPtr pTrident = TRIDENTPTR(pScrn);
    pTrident->dwords = (w + 31) >> 5;
    pTrident->h = h;

    XP_DEST_XY(x,y);
    XP_DIM_XY(w>>1,h);
    TGUI_COMMAND(GE_BLT);
}

static void
XPSubsequentColorExpandScanline(ScrnInfoPtr pScrn, int bufno)
{
    TRIDENTPtr pTrident = TRIDENTPTR(pScrn);
    XAAInfoRecPtr infoRec;
    infoRec = GET_XAAINFORECPTR_FROM_SCRNINFOPTR(pScrn);

    MoveDWORDS_FixedBase((CARD32 *)pTrident->IOBase + 0x2160,
		(CARD32 *)pTrident->XAAScanlineColorExpandBuffers[0], 
			pTrident->dwords);

    pTrident->h--;
    if (pTrident->h)
    	XPSync(pScrn);
}
#endif

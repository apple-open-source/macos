/*
 * Copyright 1992-2000 by Alan Hourihane, Wigan, England.
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
 * Trident accelerated options.
 */
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/trident/trident_accel.c,v 1.26 2003/02/12 21:46:42 tsi Exp $ */

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

static void TridentSync(ScrnInfoPtr pScrn);
static void TridentSetupForDashedLine(ScrnInfoPtr pScrn, int fg, int bg, 
				int rop, unsigned int planemask, int length,
    				unsigned char *pattern);
static void TridentSubsequentDashedBresenhamLine(ScrnInfoPtr pScrn,
        			int x, int y, int dmaj, int dmin, int e, 
				int len, int octant, int phase);
static void TridentSetupForSolidLine(ScrnInfoPtr pScrn, int color,
				int rop, unsigned int planemask);
static void TridentSubsequentSolidBresenhamLine(ScrnInfoPtr pScrn,
        			int x, int y, int dmaj, int dmin, int e, 
				int len, int octant);
static void TridentSubsequentSolidHorVertLine(ScrnInfoPtr pScrn, int x, int y,
    				int len, int dir);
static void TridentSetupForFillRectSolid(ScrnInfoPtr pScrn, int color,
				int rop, unsigned int planemask);
static void TridentSubsequentFillRectSolid(ScrnInfoPtr pScrn, int x,
				int y, int w, int h);
static void TridentSubsequentScreenToScreenCopy(ScrnInfoPtr pScrn,
				int x1, int y1, int x2,
				int y2, int w, int h);
static void TridentSetupForScreenToScreenCopy(ScrnInfoPtr pScrn,
				int xdir, int ydir, int rop, 
                                unsigned int planemask,
				int transparency_color);
static void TridentSetupForMono8x8PatternFill(ScrnInfoPtr pScrn, 
				int patternx, int patterny, int fg, int bg, 
				int rop, unsigned int planemask);
static void TridentSubsequentMono8x8PatternFillRect(ScrnInfoPtr pScrn, 
				int patternx, int patterny, int x, int y, 
				int w, int h);
#if 0
static void TridentSetupForColor8x8PatternFill(ScrnInfoPtr pScrn, 
				int patternx, int patterny, 
				int rop, unsigned int planemask, int trans_col);
static void TridentSubsequentColor8x8PatternFillRect(ScrnInfoPtr pScrn, 
				int patternx, int patterny, int x, int y, 
				int w, int h);
#endif
#if 0
static void TridentSetupForScanlineCPUToScreenColorExpandFill(
				ScrnInfoPtr pScrn,
				int fg, int bg, int rop, 
				unsigned int planemask);
static void TridentSubsequentScanlineCPUToScreenColorExpandFill(
				ScrnInfoPtr pScrn, int x,
				int y, int w, int h, int skipleft);
static void TridentSubsequentColorExpandScanline(ScrnInfoPtr pScrn, int bufno);
#endif


static void
TridentInitializeAccelerator(ScrnInfoPtr pScrn)
{
    TRIDENTPtr pTrident = TRIDENTPTR(pScrn);

    /* This forces updating the clipper */
    pTrident->Clipping = TRUE;

    CHECKCLIPPING;

    if ( (pTrident->Chipset == PROVIDIA9682) ||
         (pTrident->Chipset == CYBER9385) ||
         (pTrident->Chipset == CYBER9382) ) 
    	pTrident->EngineOperation |= 0x100; /* Disable Clipping */

    TGUI_OPERMODE(pTrident->EngineOperation);

    pTrident->PatternLocation = pScrn->displayWidth*pScrn->bitsPerPixel/8;
}

Bool
TridentAccelInit(ScreenPtr pScreen)
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

    if (!(pTrident->Chipset == TGUI9440AGi && pScrn->bitsPerPixel > 8)) 
    	infoPtr->Flags = PIXMAP_CACHE |
		     OFFSCREEN_PIXMAPS |
		     LINEAR_FRAMEBUFFER;

    pTrident->InitializeAccelerator = TridentInitializeAccelerator;
    TridentInitializeAccelerator(pScrn);

    infoPtr->PixmapCacheFlags = DO_NOT_BLIT_STIPPLES;
 
    infoPtr->Sync = TridentSync;

    infoPtr->SolidLineFlags = NO_PLANEMASK;
    infoPtr->SetupForSolidLine = TridentSetupForSolidLine;
    infoPtr->SolidBresenhamLineErrorTermBits = 12;
    infoPtr->SubsequentSolidBresenhamLine = TridentSubsequentSolidBresenhamLine;
    infoPtr->SubsequentSolidHorVertLine = TridentSubsequentSolidHorVertLine;

    infoPtr->DashedLineFlags = LINE_PATTERN_MSBFIRST_LSBJUSTIFIED |
			       NO_PLANEMASK |
			       LINE_PATTERN_POWER_OF_2_ONLY;
    infoPtr->SetupForDashedLine = TridentSetupForDashedLine;
    infoPtr->DashedBresenhamLineErrorTermBits = 12;
    infoPtr->SubsequentDashedBresenhamLine = 
					TridentSubsequentDashedBresenhamLine;  
    infoPtr->DashPatternMaxLength = 16;

    infoPtr->SolidFillFlags = NO_PLANEMASK;
    infoPtr->SetupForSolidFill = TridentSetupForFillRectSolid;
    infoPtr->SubsequentSolidFillRect = TridentSubsequentFillRectSolid;
    
    infoPtr->ScreenToScreenCopyFlags = NO_PLANEMASK;

    if (!HAS_DST_TRANS) infoPtr->ScreenToScreenCopyFlags |= NO_TRANSPARENCY;

    infoPtr->SetupForScreenToScreenCopy = 	
				TridentSetupForScreenToScreenCopy;
    infoPtr->SubsequentScreenToScreenCopy = 		
				TridentSubsequentScreenToScreenCopy;

    if (!((pTrident->Chipset == PROVIDIA9685 ||
	   pTrident->Chipset == CYBER9388) && pScrn->bitsPerPixel > 8)) { 
    	infoPtr->Mono8x8PatternFillFlags =  NO_PLANEMASK | 
					HARDWARE_PATTERN_SCREEN_ORIGIN | 
					BIT_ORDER_IN_BYTE_MSBFIRST;

    	infoPtr->SetupForMono8x8PatternFill =
				TridentSetupForMono8x8PatternFill;
    	infoPtr->SubsequentMono8x8PatternFillRect = 
				TridentSubsequentMono8x8PatternFillRect;
    }

#if 0 /* Not convinced this works 100% yet */
    infoPtr->Color8x8PatternFillFlags = NO_PLANEMASK | 
					HARDWARE_PATTERN_SCREEN_ORIGIN | 
					BIT_ORDER_IN_BYTE_MSBFIRST;

    if (!HAS_DST_TRANS) infoPtr->Color8x8PatternFillFlags |= NO_TRANSPARENCY;

    infoPtr->SetupForColor8x8PatternFill =
				TridentSetupForColor8x8PatternFill;
    infoPtr->SubsequentColor8x8PatternFillRect = 
				TridentSubsequentColor8x8PatternFillRect;
#endif

#if 0 /* This is buggy, it only seems to work 95% of the time.... */
    {
    	infoPtr->ScanlineCPUToScreenColorExpandFillFlags = NO_PLANEMASK |
					NO_TRANSPARENCY |
					BIT_ORDER_IN_BYTE_MSBFIRST;

    	pTrident->XAAScanlineColorExpandBuffers[0] =
	    xnfalloc(((pScrn->virtualX + 63)) *4* (pScrn->bitsPerPixel / 8));

    	infoPtr->NumScanlineColorExpandBuffers = 1;
    	infoPtr->ScanlineColorExpandBuffers = 
					pTrident->XAAScanlineColorExpandBuffers;

    	infoPtr->SetupForScanlineCPUToScreenColorExpandFill =
			TridentSetupForScanlineCPUToScreenColorExpandFill;
    	infoPtr->SubsequentScanlineCPUToScreenColorExpandFill = 
			TridentSubsequentScanlineCPUToScreenColorExpandFill;
    	infoPtr->SubsequentColorExpandScanline = 
			TridentSubsequentColorExpandScanline;
    }
#endif

    return(XAAInit(pScreen, infoPtr));
}

static void
TridentSync(ScrnInfoPtr pScrn)
{
    TRIDENTPtr pTrident = TRIDENTPTR(pScrn);
    int count = 0, timeout = 0;
    int busy;

    TGUI_OPERMODE(pTrident->EngineOperation);

    for (;;) {
	BLTBUSY(busy);
	if (busy != GE_BUSY) {
	    return;
	}
	count++;
	if (count == 10000000) {
	    ErrorF("Trident: BitBLT engine time-out.\n");
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
TridentClearSync(ScrnInfoPtr pScrn)
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
	    ErrorF("Trident: BitBLT engine time-out.\n");
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
TridentSetupForScreenToScreenCopy(ScrnInfoPtr pScrn, 
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
	if (pTrident->Chipset == PROVIDIA9685 ||
	    pTrident->Chipset == CYBER9388) {
	    dst |= 1<<16;
	} else {
    	    TGUI_OPERMODE(pTrident->EngineOperation | DST_ENABLE);
	}
	TGUI_CKEY(transparency_color);
    }

    TGUI_DRAWFLAG(pTrident->DrawFlag | pTrident->BltScanDirection | SCR2SCR | dst);
    TGUI_FMIX(XAACopyROP[rop]);
}

static void
TridentSubsequentScreenToScreenCopy(ScrnInfoPtr pScrn, int x1, int y1,
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
    TGUI_SRC_XY(x1,y1);
    TGUI_DEST_XY(x2,y2);
    TGUI_DIM_XY(w,h);
    TGUI_COMMAND(GE_BLT);
    TridentClearSync(pScrn);
}

static void
TridentSetupForSolidLine(ScrnInfoPtr pScrn, int color,
					 int rop, unsigned int planemask)
{
    TRIDENTPtr pTrident = TRIDENTPTR(pScrn);

    pTrident->BltScanDirection = 0;
    REPLICATE(color);
    TGUI_FMIX(XAAPatternROP[rop]);
    if (pTrident->Chipset == PROVIDIA9685 ||
        pTrident->Chipset == CYBER9388) {
    	TGUI_FPATCOL(color);
    } else {
    	TGUI_FCOLOUR(color);
    }
}

static void 
TridentSubsequentSolidBresenhamLine( ScrnInfoPtr pScrn,
        int x, int y, int dmaj, int dmin, int e, int len, int octant)
{
    TRIDENTPtr pTrident = TRIDENTPTR(pScrn);
    int tmp = pTrident->BltScanDirection;

    if (octant & YMAJOR) tmp |= YMAJ;
    if (octant & XDECREASING) tmp |= XNEG;
    if (octant & YDECREASING) tmp |= YNEG;
    TGUI_DRAWFLAG(pTrident->DrawFlag | SOLIDFILL | STENCIL | tmp);
    TGUI_SRC_XY(dmin-dmaj,dmin);
    TGUI_DEST_XY(x,y);
    TGUI_DIM_XY(dmin+e,len);
    TGUI_COMMAND(GE_BRESLINE);
    TridentSync(pScrn);
}

static void 
TridentSubsequentSolidHorVertLine(
    ScrnInfoPtr pScrn,
    int x, int y, 
    int len, int dir
){
    TRIDENTPtr pTrident = TRIDENTPTR(pScrn);

    TGUI_DRAWFLAG(pTrident->DrawFlag | SOLIDFILL);
    if (dir == DEGREES_0) {
    	TGUI_DIM_XY(len,1);
    	TGUI_DEST_XY(x,y);
    } else {
    	TGUI_DIM_XY(1,len);
    	TGUI_DEST_XY(x,y);
    }
    TGUI_COMMAND(GE_BLT);
    TridentSync(pScrn);
}

void
TridentSetupForDashedLine(
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
    if (pTrident->Chipset == PROVIDIA9685 ||
        pTrident->Chipset == CYBER9388) {
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
TridentSubsequentDashedBresenhamLine(ScrnInfoPtr pScrn,
        int x, int y, int dmaj, int dmin, int e, int len, int octant, int phase)
{
    TRIDENTPtr pTrident = TRIDENTPTR(pScrn);
    int tmp = pTrident->BltScanDirection;

    if (octant & YMAJOR) tmp |= YMAJ;
    if (octant & XDECREASING) tmp |= XNEG;
    if (octant & YDECREASING) tmp |= YNEG;

    TGUI_STYLE(((pTrident->LinePattern >> phase) | 
		(pTrident->LinePattern << (16-phase))) & 0x0000FFFF);
    TGUI_DRAWFLAG(pTrident->DrawFlag | STENCIL | tmp);
    TGUI_SRC_XY(dmin-dmaj,dmin);
    TGUI_DEST_XY(x,y);
    TGUI_DIM_XY(e+dmin,len);
    TGUI_COMMAND(GE_BRESLINE);
    TridentSync(pScrn);
}

static void
TridentSetupForFillRectSolid(ScrnInfoPtr pScrn, int color, 
				    int rop, unsigned int planemask)
{
    TRIDENTPtr pTrident = TRIDENTPTR(pScrn);
    int drawflag = 0;

    REPLICATE(color);
    TGUI_FMIX(XAAPatternROP[rop]);
    if (pTrident->Chipset == PROVIDIA9685 ||
        pTrident->Chipset == CYBER9388) {
    	TGUI_FPATCOL(color);
    } else {
    	drawflag |= PATMONO;
    	TGUI_FCOLOUR(color);
    }
    TGUI_DRAWFLAG(pTrident->DrawFlag | SOLIDFILL | drawflag);
}

static void
TridentSubsequentFillRectSolid(ScrnInfoPtr pScrn, int x, int y, int w, int h)
{
    TRIDENTPtr pTrident = TRIDENTPTR(pScrn);

    TGUI_DIM_XY(w,h);
    TGUI_DEST_XY(x,y);
    TGUI_COMMAND(GE_BLT);
    TridentSync(pScrn);
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

static void 
TridentSetupForMono8x8PatternFill(ScrnInfoPtr pScrn, 
					   int patternx, int patterny, 
					   int fg, int bg, int rop,
					   unsigned int planemask)
{
    TRIDENTPtr pTrident = TRIDENTPTR(pScrn);
    int drawflag = 0;

    REPLICATE(fg);
    if (pTrident->Chipset == PROVIDIA9685 ||
        pTrident->Chipset == CYBER9388)
    	TGUI_FPATCOL(fg);
    else
    	TGUI_FCOLOUR(fg);

    if (bg == -1) {
	drawflag |= 1<<12;
    	if (pTrident->Chipset == PROVIDIA9685 ||
            pTrident->Chipset == CYBER9388) 
    	    TGUI_BPATCOL(~fg);
	else
    	    TGUI_BCOLOUR(~fg);
    } else {
    	REPLICATE(bg);
    	if (pTrident->Chipset == PROVIDIA9685 ||
            pTrident->Chipset == CYBER9388) 
    	    TGUI_BPATCOL(bg);
	else
    	    TGUI_BCOLOUR(bg);
    }

    if (pTrident->Chipset == PROVIDIA9685 ||
        pTrident->Chipset == CYBER9388) {
	drawflag |= 7<<18;
    }
    TGUI_DRAWFLAG(pTrident->DrawFlag | PAT2SCR | PATMONO | drawflag);
    TGUI_PATLOC(((patterny * pTrident->PatternLocation) +
		 (patternx * pScrn->bitsPerPixel / 8)) >> 6);
    TGUI_FMIX(XAAPatternROP[rop]);
}

static void 
TridentSubsequentMono8x8PatternFillRect(ScrnInfoPtr pScrn, 	
				   int patternx, int patterny,
				   int x, int y,
				   int w, int h)
{
    TRIDENTPtr pTrident = TRIDENTPTR(pScrn);
  
    TGUI_DEST_XY(x,y);
    TGUI_DIM_XY(w,h);
    TGUI_COMMAND(GE_BLT);
    TridentSync(pScrn);
}

#if 0
static void 
TridentSetupForColor8x8PatternFill(ScrnInfoPtr pScrn, 
					   int patternx, int patterny, 
					   int rop,
					   unsigned int planemask,
					   int transparency_color)
{
    TRIDENTPtr pTrident = TRIDENTPTR(pScrn);
    int drawflag = 0;

    REPLICATE(transparency_color);
    if (transparency_color != -1) {
    	if (pTrident->Chipset == PROVIDIA9685 ||
            pTrident->Chipset == CYBER9388) {
	    drawflag |= 1<<16;
	} else {
    	    TGUI_OPERMODE(pTrident->EngineOperation | DST_ENABLE);
	}
	TGUI_CKEY(transparency_color);
    }

    TGUI_DRAWFLAG(pTrident->DrawFlag | PAT2SCR | drawflag);
    TGUI_PATLOC(((patterny * pTrident->PatternLocation) +
		 (patternx * pScrn->bitsPerPixel / 8)) >> 6);
    TGUI_FMIX(XAAPatternROP[rop]);
}

static void 
TridentSubsequentColor8x8PatternFillRect(ScrnInfoPtr pScrn, 	
				   int patternx, int patterny,
				   int x, int y,
				   int w, int h)
{
    TRIDENTPtr pTrident = TRIDENTPTR(pScrn);
  
    TGUI_DEST_XY(x,y);
    TGUI_DIM_XY(w,h);
    TGUI_COMMAND(GE_BLT);
    TridentClearSync(pScrn);
}
#endif

#if 0
static void
TridentSetupForScanlineCPUToScreenColorExpandFill(
	ScrnInfoPtr pScrn,
	int fg, int bg, 
	int rop, 
	unsigned int planemask
){
    TRIDENTPtr pTrident = TRIDENTPTR(pScrn);
    int drawflag = SRCMONO;

    REPLICATE(fg);
    TGUI_FCOLOUR(fg);
    if (bg == -1) {
	drawflag |= 1<<12;
    	TGUI_BCOLOUR(~fg);
    } else {
    	REPLICATE(bg);
    	TGUI_BCOLOUR(bg);
    }

    TGUI_SRC_XY(0,0);
    TGUI_DRAWFLAG(drawflag);
    TGUI_FMIX(XAACopyROP[rop]);
}

static void
TridentSubsequentScanlineCPUToScreenColorExpandFill(
	ScrnInfoPtr pScrn,
	int x, int y, int w, int h,
	int skipleft
){
    TRIDENTPtr pTrident = TRIDENTPTR(pScrn);
    pTrident->dwords = (w + 31) >> 5;
    pTrident->h = h;
    pTrident->y = y;
    pTrident->x = x;
    pTrident->w = w;

    TGUI_DEST_XY(x,pTrident->y++);
    TGUI_DIM_XY(w,1);
    TGUI_COMMAND(GE_BLT);
}

static void
TridentSubsequentColorExpandScanline(ScrnInfoPtr pScrn, int bufno)
{
    TRIDENTPtr pTrident = TRIDENTPTR(pScrn);
    XAAInfoRecPtr infoRec;
    infoRec = GET_XAAINFORECPTR_FROM_SCRNINFOPTR(pScrn);

    MoveDWORDS((CARD32 *)pTrident->FbBase, 
		(CARD32 *)pTrident->XAAScanlineColorExpandBuffers[0], 
			pTrident->dwords);

    pTrident->h--;
    TridentSync(pScrn);
    if (pTrident->h) {
    	TGUI_DEST_XY(pTrident->x,pTrident->y++);
    	TGUI_DIM_XY(pTrident->w,1);
    	TGUI_COMMAND(GE_BLT);
    }
}
#endif

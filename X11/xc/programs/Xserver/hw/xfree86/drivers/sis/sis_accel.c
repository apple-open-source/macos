/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/sis/sis_accel.c,v 1.25 2003/01/29 15:42:16 eich Exp $ */
/*
 * Copyright 1998,1999 by Alan Hourihane, Wigan, England.
 * Parts Copyright 2002 Thomas Winischhofer, Vienna, Austria.
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
 * Authors:  Alan Hourihane, alanh@fairlite.demon.co.uk
 *           Mike Chapman <mike@paranoia.com>,
 *           Juanjo Santamarta <santamarta@ctv.es>,
 *           Mitani Hiroshi <hmitani@drl.mei.co.jp>
 *           David Thomas <davtom@dream.org.uk>
 *	     Thomas Winischhofer <thomas@winischhofer.net>
 */

#if 0
#define CTSCE		/* TW: Include enhanced color expansion code */
#endif			/*     This produces drawing errors sometimes */

#include "xf86.h"
#include "xf86_OSproc.h"
#include "xf86_ansic.h"

#include "xf86PciInfo.h"
#include "xf86Pci.h"

#include "sis_accel.h"
#include "sis_regs.h"
#include "sis.h"
#include "xaarop.h"

static void SiSSync(ScrnInfoPtr pScrn);
static void SiSSetupForFillRectSolid(ScrnInfoPtr pScrn, int color,
                int rop, unsigned int planemask);
static void SiSSubsequentFillRectSolid(ScrnInfoPtr pScrn, int x,
                int y, int w, int h);
static void SiSSetupForScreenToScreenCopy(ScrnInfoPtr pScrn,
                int xdir, int ydir, int rop, 
                unsigned int planemask, int transparency_color);
static void SiSSubsequentScreenToScreenCopy(ScrnInfoPtr pScrn,
                int x1, int y1, int x2,
                int y2, int w, int h);
static void SiSSetupForMono8x8PatternFill(ScrnInfoPtr pScrn,
                int patternx, int patterny, int fg, int bg, 
                int rop, unsigned int planemask);
static void SiSSubsequentMono8x8PatternFillRect(ScrnInfoPtr pScrn, 
                int patternx, int patterny, int x, int y, 
                int w, int h);
#if 0
static void SiSSetupForScreenToScreenColorExpandFill (ScrnInfoPtr pScrn,
                int fg, int bg, 
                int rop, unsigned int planemask);
static void SiSSubsequentScreenToScreenColorExpandFill( ScrnInfoPtr pScrn,
                int x, int y, int w, int h,
                int srcx, int srcy, int offset );
#endif
static void SiSSetClippingRectangle ( ScrnInfoPtr pScrn,
                int left, int top, int right, int bottom);
static void SiSDisableClipping (ScrnInfoPtr pScrn);
static void SiSSetupForSolidLine(ScrnInfoPtr pScrn,
                int color, int rop, unsigned int planemask);
static void SiSSubsequentSolidTwoPointLine(ScrnInfoPtr pScrn,
                int x1, int y1, int x2, int y2, int flags);
static void SiSSubsequentSolidHorVertLine(ScrnInfoPtr pScrn,
                int x, int y, int len, int dir);
#ifdef CTSCE
static void SiSSetupForScanlineCPUToScreenColorExpandFill(ScrnInfoPtr pScrn,
                                int fg, int bg, int rop,
                                unsigned int planemask);
static void SiSSubsequentScanlineCPUToScreenColorExpandFill(ScrnInfoPtr pScrn,
                                int x, int y, int w, int h,
                                int skipleft);
static void SiSSubsequentColorExpandScanline(ScrnInfoPtr pScrn, int bufno);
#endif

Bool
SiSAccelInit(ScreenPtr pScreen)
{
    XAAInfoRecPtr  infoPtr;
    ScrnInfoPtr    pScrn = xf86Screens[pScreen->myNum];
    SISPtr         pSiS = SISPTR(pScrn);
    BoxRec         AvailFBArea;
    int            topFB, i;
    int            reservedFbSize;
    int            UsableFbSize;
    unsigned char  *AvailBufBase;

    pSiS->AccelInfoPtr = infoPtr = XAACreateInfoRec();
    if (!infoPtr)  return FALSE;

    infoPtr->Flags = LINEAR_FRAMEBUFFER |
  		     OFFSCREEN_PIXMAPS |
  		     PIXMAP_CACHE;

    /* Sync */
    infoPtr->Sync = SiSSync;

    /* Screen To Screen copy */
    infoPtr->SetupForScreenToScreenCopy =  SiSSetupForScreenToScreenCopy;
    infoPtr->SubsequentScreenToScreenCopy = SiSSubsequentScreenToScreenCopy;
    infoPtr->ScreenToScreenCopyFlags = NO_TRANSPARENCY | NO_PLANEMASK;

    /* Solid fill */
    infoPtr->SetupForSolidFill = SiSSetupForFillRectSolid;
    infoPtr->SubsequentSolidFillRect = SiSSubsequentFillRectSolid;
    infoPtr->SolidFillFlags = NO_PLANEMASK;

    /* On 5597/5598 and 6326, clipping and lines only work
       for 1024, 2048, 4096 logical width */
    if(pSiS->ValidWidth) {
        /* Clipping */
        infoPtr->SetClippingRectangle = SiSSetClippingRectangle;
        infoPtr->DisableClipping = SiSDisableClipping;
        infoPtr->ClippingFlags =  
                    HARDWARE_CLIP_SOLID_LINE | 
                    HARDWARE_CLIP_SCREEN_TO_SCREEN_COPY |
                    HARDWARE_CLIP_MONO_8x8_FILL |
                    HARDWARE_CLIP_SOLID_FILL  ;

    	/* Solid Lines */
    	infoPtr->SetupForSolidLine = SiSSetupForSolidLine;
    	infoPtr->SubsequentSolidTwoPointLine = SiSSubsequentSolidTwoPointLine;
    	infoPtr->SubsequentSolidHorVertLine = SiSSubsequentSolidHorVertLine;
	infoPtr->SolidLineFlags = NO_PLANEMASK;
    }

    if(pScrn->bitsPerPixel != 24) {
        /* 8x8 mono pattern */
        infoPtr->SetupForMono8x8PatternFill = SiSSetupForMono8x8PatternFill;
        infoPtr->SubsequentMono8x8PatternFillRect = SiSSubsequentMono8x8PatternFillRect;
	infoPtr->Mono8x8PatternFillFlags =
                    NO_PLANEMASK |
                    HARDWARE_PATTERN_PROGRAMMED_BITS |
                    HARDWARE_PATTERN_PROGRAMMED_ORIGIN |
                    BIT_ORDER_IN_BYTE_MSBFIRST;
    }

#ifdef CTSCE
    if(pScrn->bitsPerPixel != 24) {
       /* TW: per-scanline color expansion (using indirect method) */
       pSiS->ColorExpandBufferNumber = 4;
       pSiS->ColorExpandBufferCountMask = 0x03;
       pSiS->PerColorExpandBufferSize = ((pScrn->virtualX + 31) / 32) * 4;

       infoPtr->NumScanlineColorExpandBuffers = pSiS->ColorExpandBufferNumber;
       infoPtr->ScanlineColorExpandBuffers = (unsigned char **)&pSiS->ColorExpandBufferAddr[0];

       infoPtr->SetupForScanlineCPUToScreenColorExpandFill =
	                            SiSSetupForScanlineCPUToScreenColorExpandFill;
       infoPtr->SubsequentScanlineCPUToScreenColorExpandFill =
	                            SiSSubsequentScanlineCPUToScreenColorExpandFill;
       infoPtr->SubsequentColorExpandScanline =
	                            SiSSubsequentColorExpandScanline;
       infoPtr->ScanlineCPUToScreenColorExpandFillFlags =
	    NO_PLANEMASK |
	    CPU_TRANSFER_PAD_DWORD |
	    SCANLINE_PAD_DWORD |
	    BIT_ORDER_IN_BYTE_MSBFIRST |
	    LEFT_EDGE_CLIPPING;
    } else {
       pSiS->ColorExpandBufferNumber = 0;
    }
#else
    pSiS->ColorExpandBufferNumber = 0;
#endif

    topFB = pSiS->maxxfbmem;

    reservedFbSize = pSiS->ColorExpandBufferNumber * pSiS->PerColorExpandBufferSize;

    UsableFbSize = topFB - reservedFbSize;

    /* Layout: (Sizes do not reflect correct proportions)
     * |--------------++++++++++++++++++++|  ====================~~~~~~~~~~~~|
     *   UsableFbSize  ColorExpandBuffers |        TurboQueue     HWCursor
     *                                  topFB
     */

    if(pSiS->ColorExpandBufferNumber) {
      AvailBufBase = pSiS->FbBase + UsableFbSize;
      for (i = 0; i < pSiS->ColorExpandBufferNumber; i++) {
	  pSiS->ColorExpandBufferAddr[i] = AvailBufBase +
		    i * pSiS->PerColorExpandBufferSize;
	  pSiS->ColorExpandBufferScreenOffset[i] = UsableFbSize +
		    i * pSiS->PerColorExpandBufferSize;
      }
    }
    AvailFBArea.x1 = 0;
    AvailFBArea.y1 = 0;
    AvailFBArea.x2 = pScrn->displayWidth;
    AvailFBArea.y2 = UsableFbSize / (pScrn->displayWidth * pScrn->bitsPerPixel / 8) - 1;

    if (AvailFBArea.y2 < 0)
	AvailFBArea.y2 = 32767;

    if(AvailFBArea.y2 < pScrn->currentMode->VDisplay) {
	xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
	 	"Not enough video RAM for accelerator. At least "
		"%dKB needed, %dKB available\n",
		((((pScrn->displayWidth * pScrn->bitsPerPixel/8)   /* TW: +8 for make it sure */
		     * pScrn->currentMode->VDisplay) + reservedFbSize) / 1024) + 8,
		pSiS->maxxfbmem/1024);
	pSiS->NoAccel = TRUE;
	pSiS->NoXvideo = TRUE;
	XAADestroyInfoRec(pSiS->AccelInfoPtr);
	pSiS->AccelInfoPtr = NULL;
	return FALSE;
    }

    xf86DrvMsg(pScrn->scrnIndex, X_INFO,
	   "Frame Buffer From (%d,%d) To (%d,%d)\n",
	   AvailFBArea.x1, AvailFBArea.y1, AvailFBArea.x2, AvailFBArea.y2);

    xf86InitFBManager(pScreen, &AvailFBArea);

    return(XAAInit(pScreen, infoPtr));
}

/* sync */
static void 
SiSSync(ScrnInfoPtr pScrn) {
    SISPtr pSiS = SISPTR(pScrn);
    sisBLTSync;
}

/* Clipping */
static void SiSSetClippingRectangle ( ScrnInfoPtr pScrn,
                int left, int top, int right, int bottom)
{
    SISPtr pSiS = SISPTR(pScrn);

    sisBLTSync;
    sisSETCLIPTOP(left,top);
    sisSETCLIPBOTTOM(right,bottom);
    pSiS->ClipEnabled = TRUE;
}

static void SiSDisableClipping (ScrnInfoPtr pScrn)
{
    SISPtr pSiS = SISPTR(pScrn);
    pSiS->ClipEnabled = FALSE;
}

static const int sisALUConv[] =
{
    0x00,       /* dest = 0;            0,      GXclear,        0 */
    0x88,       /* dest &= src;         DSa,    GXand,          0x1 */
    0x44,       /* dest = src & ~dest;  SDna,   GXandReverse,   0x2 */
    0xCC,       /* dest = src;          S,      GXcopy,         0x3 */
    0x22,       /* dest &= ~src;        DSna,   GXandInverted,  0x4 */
    0xAA,       /* dest = dest;         D,      GXnoop,         0x5 */
    0x66,       /* dest = ^src;         DSx,    GXxor,          0x6 */
    0xEE,       /* dest |= src;         DSo,    GXor,           0x7 */
    0x11,       /* dest = ~src & ~dest; DSon,   GXnor,          0x8 */
    0x99,       /* dest ^= ~src ;       DSxn,   GXequiv,        0x9 */
    0x55,       /* dest = ~dest;        Dn,     GXInvert,       0xA */
    0xDD,       /* dest = src|~dest ;   SDno,   GXorReverse,    0xB */
    0x33,       /* dest = ~src;         Sn,     GXcopyInverted, 0xC */
    0xBB,       /* dest |= ~src;        DSno,   GXorInverted,   0xD */
    0x77,       /* dest = ~src|~dest;   DSan,   GXnand,         0xE */
    0xFF,       /* dest = 0xFF;         1,      GXset,          0xF */
};
/* same ROP but with Pattern as Source */
static const int sisPatALUConv[] =
{
    0x00,       /* dest = 0;            0,      GXclear,        0 */
    0xA0,       /* dest &= src;         DPa,    GXand,          0x1 */
    0x50,       /* dest = src & ~dest;  PDna,   GXandReverse,   0x2 */
    0xF0,       /* dest = src;          P,      GXcopy,         0x3 */
    0x0A,       /* dest &= ~src;        DPna,   GXandInverted,  0x4 */
    0xAA,       /* dest = dest;         D,      GXnoop,         0x5 */
    0x5A,       /* dest = ^src;         DPx,    GXxor,          0x6 */
    0xFA,       /* dest |= src;         DPo,    GXor,           0x7 */
    0x05,       /* dest = ~src & ~dest; DPon,   GXnor,          0x8 */
    0xA5,       /* dest ^= ~src ;       DPxn,   GXequiv,        0x9 */
    0x55,       /* dest = ~dest;        Dn,     GXInvert,       0xA */
    0xF5,       /* dest = src|~dest ;   PDno,   GXorReverse,    0xB */
    0x0F,       /* dest = ~src;         Pn,     GXcopyInverted, 0xC */
    0xAF,       /* dest |= ~src;        DPno,   GXorInverted,   0xD */
    0x5F,       /* dest = ~src|~dest;   DPan,   GXnand,         0xE */
    0xFF,       /* dest = 0xFF;         1,      GXset,          0xF */
};


/* Screen to screen copy */
static void
SiSSetupForScreenToScreenCopy(ScrnInfoPtr pScrn, int xdir, int ydir,
                int rop, unsigned int planemask,
                int transparency_color)
{
    SISPtr pSiS = SISPTR(pScrn);
    sisBLTSync;
    sisSETPITCH(pSiS->scrnOffset, pSiS->scrnOffset);

    sisSETROP(XAACopyROP[rop]);
    pSiS->Xdirection = xdir;
    pSiS->Ydirection = ydir;
}

static void
SiSSubsequentScreenToScreenCopy(ScrnInfoPtr pScrn, int x1, int y1, int x2,
                int y2, int w, int h)
{
    SISPtr pSiS = SISPTR(pScrn);
    int srcaddr, destaddr;
    int op ;

    op = sisCMDBLT | sisSRCVIDEO;
    if(pSiS->Ydirection == -1) {
        op |= sisBOTTOM2TOP;
	srcaddr = (y1 + h - 1) * pSiS->CurrentLayout.displayWidth;
        destaddr = (y2 + h - 1) * pSiS->CurrentLayout.displayWidth;
    } else {
        op |= sisTOP2BOTTOM;
	srcaddr = y1 * pSiS->CurrentLayout.displayWidth;
        destaddr = y2 * pSiS->CurrentLayout.displayWidth;
    }
    if(pSiS->Xdirection == -1) {
        op |= sisRIGHT2LEFT;
        srcaddr += x1 + w - 1;
        destaddr += x2 + w - 1;
    } else {
        op |= sisLEFT2RIGHT;
        srcaddr += x1;
        destaddr += x2;
    }
    if (pSiS->ClipEnabled)
        op |= sisCLIPINTRN | sisCLIPENABL;

    srcaddr *= (pSiS->CurrentLayout.bitsPerPixel/8);
    destaddr *= (pSiS->CurrentLayout.bitsPerPixel/8);
    if(((pSiS->CurrentLayout.bitsPerPixel / 8) > 1) && (pSiS->Xdirection == -1)) {
        srcaddr += (pSiS->CurrentLayout.bitsPerPixel/8)-1;
        destaddr += (pSiS->CurrentLayout.bitsPerPixel/8)-1;
    }

    sisBLTSync;
    sisSETSRCADDR(srcaddr);
    sisSETDSTADDR(destaddr);
    sisSETHEIGHTWIDTH(h-1, w * (pSiS->CurrentLayout.bitsPerPixel/8)-1);
    sisSETCMD(op);
}

/* solid fill */
static void 
SiSSetupForFillRectSolid(ScrnInfoPtr pScrn, int color, int rop, 
             unsigned int planemask)
{
    SISPtr pSiS = SISPTR(pScrn);

    sisBLTSync;
    sisSETBGROPCOL(XAACopyROP[rop], color);
    sisSETFGROPCOL(XAACopyROP[rop], color);
    sisSETPITCH(pSiS->scrnOffset, pSiS->scrnOffset);
}

static void 
SiSSubsequentFillRectSolid(ScrnInfoPtr pScrn, int x, int y, int w, int h)
{
    SISPtr pSiS = SISPTR(pScrn);
    int destaddr, op;

    destaddr = y * pSiS->CurrentLayout.displayWidth + x;

    op = sisCMDBLT | sisSRCBG | sisTOP2BOTTOM | sisLEFT2RIGHT;

    if(pSiS->ClipEnabled)
        op |= sisCLIPINTRN | sisCLIPENABL;

    destaddr *= (pSiS->CurrentLayout.bitsPerPixel / 8);

    sisBLTSync;
    sisSETHEIGHTWIDTH(h-1, w * (pSiS->CurrentLayout.bitsPerPixel/8)-1);
    sisSETDSTADDR(destaddr);
    sisSETCMD(op);
}

/* 8x8 mono */
static void 
SiSSetupForMono8x8PatternFill(ScrnInfoPtr pScrn, int patternx, int patterny, 
                int fg, int bg, int rop, unsigned int planemask)
{
    SISPtr pSiS = SISPTR(pScrn);
    unsigned int  *patternRegPtr;
    int  i;

    (void)XAAHelpPatternROP(pScrn, &fg, &bg, planemask, &rop);

    sisBLTSync;
    if(bg != -1) {
        sisSETBGROPCOL(0xcc, bg);  /* copy */
    } else {
        sisSETBGROPCOL(0xAA, bg);  /* noop */
    }
    sisSETFGROPCOL(rop, fg);
    sisSETPITCH(0, pSiS->scrnOffset);
    sisSETSRCADDR(0);
    patternRegPtr =  (unsigned int *)sisSETPATREG();
    pSiS->sisPatternReg[0] = pSiS->sisPatternReg[2] = patternx ;
    pSiS->sisPatternReg[1] = pSiS->sisPatternReg[3] = patterny ;
    for ( i = 0 ; i < 16 /* sisPatternHeight */ ; ) {
        patternRegPtr[i++] = patternx ;
        patternRegPtr[i++] = patterny ;
    }
}

static void 
SiSSubsequentMono8x8PatternFillRect(ScrnInfoPtr pScrn, int patternx, 
                int patterny, int x, int y, int w, int h)
{
    SISPtr                  pSiS = SISPTR(pScrn);
    int                     dstaddr;
    register unsigned char  *patternRegPtr;
    register unsigned char  *srcPatternRegPtr;
    register unsigned int   *patternRegPtrL;
    int                     i, k;
    unsigned short          tmp;
    int                     shift;
    int                     op  = sisCMDCOLEXP |
                                  sisTOP2BOTTOM |
		                  sisLEFT2RIGHT |
                                  sisPATFG |
		                  sisSRCBG;

    if (pSiS->ClipEnabled)
        op |= sisCLIPINTRN | sisCLIPENABL;

    dstaddr = ( y * pSiS->CurrentLayout.displayWidth + x ) *
                           pSiS->CurrentLayout.bitsPerPixel / 8;

    sisBLTSync;

    patternRegPtr = sisSETPATREG();
    srcPatternRegPtr = (unsigned char *)pSiS->sisPatternReg ;
    shift = 8 - patternx ;
    for ( i = 0, k = patterny ; i < 8 ; i++, k++ ) {
        tmp = srcPatternRegPtr[k]<<8 | srcPatternRegPtr[k] ;
        tmp >>= shift ;
        patternRegPtr[i] = tmp & 0xff;
    }
    patternRegPtrL = (unsigned int *)sisSETPATREG();
    for ( i = 2 ; i < 16 /* sisPatternHeight */; ) {
        patternRegPtrL[i++] = patternRegPtrL[0];
        patternRegPtrL[i++] = patternRegPtrL[1];
    }

    sisSETDSTADDR(dstaddr);
    sisSETHEIGHTWIDTH(h-1, w*(pSiS->CurrentLayout.bitsPerPixel/8)-1);
    sisSETCMD(op);
}

/* Line */
static void SiSSetupForSolidLine(ScrnInfoPtr pScrn, 
                int color, int rop, unsigned int planemask)
{
    SISPtr pSiS = SISPTR(pScrn);

    sisBLTSync;
    sisSETBGROPCOL(XAACopyROP[rop], 0);
    sisSETFGROPCOL(XAACopyROP[rop], color);
}

static void SiSSubsequentSolidTwoPointLine(ScrnInfoPtr pScrn,
            int x1, int y1, int x2, int y2, int flags)

{
    SISPtr pSiS = SISPTR(pScrn);
    int op ;
    int major, minor, err,K1,K2, tmp;

    op = sisCMDLINE  | sisSRCFG;

    if ((flags & OMIT_LAST))
        op |= sisLASTPIX;

    if (pSiS->ClipEnabled)
        op |= sisCLIPINTRN | sisCLIPENABL;

    if ((major = x2 - x1) <= 0) {
       major = -major;
    } else
        op |= sisXINCREASE;

    if ((minor = y2 - y1) <= 0) {
       minor = -minor;
    } else
        op |= sisYINCREASE;

    if (minor >= major) {
       tmp = minor; 
       minor = major; 
       major = tmp;
    } else
        op |= sisXMAJOR;

    K1 = (minor - major)<<1;
    K2 = minor<<1;
    err = (minor<<1) - major;

    sisBLTSync;
    sisSETXStart(x1);
    sisSETYStart(y1);
    sisSETLineSteps((short)K1,(short)K2); 
    sisSETLineErrorTerm((short)err);
    sisSETLineMajorCount((short)major);
    sisSETCMD(op);
}

static void SiSSubsequentSolidHorVertLine(ScrnInfoPtr pScrn,
                                int x, int y, int len, int dir)
{
    SISPtr pSiS = SISPTR(pScrn);
    int destaddr, op;

    destaddr = y * pSiS->CurrentLayout.displayWidth + x;

    op = sisCMDBLT | sisSRCFG | sisTOP2BOTTOM | sisLEFT2RIGHT;

    if (pSiS->ClipEnabled)
        op |= sisCLIPINTRN | sisCLIPENABL;

    destaddr *= (pSiS->CurrentLayout.bitsPerPixel / 8);

    sisBLTSync;

    sisSETPITCH(pSiS->scrnOffset, pSiS->scrnOffset);

    if(dir == DEGREES_0) {
        sisSETHEIGHTWIDTH(0, len * (pSiS->CurrentLayout.bitsPerPixel >> 3) - 1);
    } else {
        sisSETHEIGHTWIDTH(len - 1, (pSiS->CurrentLayout.bitsPerPixel >> 3) - 1);
    }

    sisSETDSTADDR(destaddr);
    sisSETCMD(op);
}

#ifdef CTSCE
/* TW: ----- CPU To Screen Color Expand (scanline-wise) ------ */
static void
SiSSetupForScanlineCPUToScreenColorExpandFill(ScrnInfoPtr pScrn,
		int fg, int bg, int rop, unsigned int planemask)
{
    SISPtr pSiS=SISPTR(pScrn);

    pSiS->CommandReg = 0;

    pSiS->CommandReg |= (sisCMDECOLEXP |
  		         sisLEFT2RIGHT |
			 sisTOP2BOTTOM);

    sisBLTSync;

    /* TW: The combination of flags in the following
     *     is not understandable. However, this is the
     *     only combination that seems to work.
     */
    if(bg == -1) {
        sisSETROPBG(0xAA);             /* dst = dst (=noop) */
	pSiS->CommandReg |= sisSRCFG;
    } else {
        sisSETBGROPCOL(sisPatALUConv[rop], bg);
	pSiS->CommandReg |= sisSRCFG | sisPATBG;
    }

    sisSETFGROPCOL(sisALUConv[rop], fg);

    sisSETDSTPITCH(pSiS->scrnOffset);
}


static void
SiSSubsequentScanlineCPUToScreenColorExpandFill(
                        ScrnInfoPtr pScrn, int x, int y, int w,
                        int h, int skipleft)
{
    SISPtr pSiS = SISPTR(pScrn);
    int _x0, _y0, _x1, _y1;
    int op = pSiS->CommandReg;

    if(skipleft > 0) {
	_x0 = x + skipleft;
	_y0 = y;
	_x1 = x + w;
	_y1 = y + h;
	sisSETCLIPTOP(_x0, _y0);
	sisSETCLIPBOTTOM(_x1, _y1);
	op |= sisCLIPENABL;
    } else {
	op &= (~(sisCLIPINTRN | sisCLIPENABL));
    }

    sisSETSRCPITCH(((((w+7)/8)+3) >> 2) * 4);

    sisSETHEIGHTWIDTH(1-1, (w * (pSiS->CurrentLayout.bitsPerPixel/8)) - 1);

    pSiS->xcurrent = x;
    pSiS->ycurrent = y;

    pSiS->CommandReg = op;
}

static void
SiSSubsequentColorExpandScanline(ScrnInfoPtr pScrn, int bufno)
{
    SISPtr pSiS = SISPTR(pScrn);
    long   cbo = pSiS->ColorExpandBufferScreenOffset[bufno];
    int    op = pSiS->CommandReg;
    int    destaddr;

    destaddr = (pSiS->ycurrent * pSiS->CurrentLayout.displayWidth) + pSiS->xcurrent;
    destaddr *= (pSiS->CurrentLayout.bitsPerPixel / 8);

    /* TW: Wait until there is no color expansion command in queue */
    /* sisBLTSync; */

    sisSETSRCADDR(cbo);

    sisSETDSTADDR(destaddr);

    sisSETCMD(op);

    pSiS->ycurrent++;

    /* TW: Wait for eventual color expand commands to finish */
    /* (needs to be done, otherwise the data in the buffer may
     *  be overwritten while accessed by the hardware)
     */
    while((MMIO_IN32(pSiS->IOBase, 0x8284) & 0x80000000)) {}

    sisBLTSync;
}
#endif  /* CTSCE */



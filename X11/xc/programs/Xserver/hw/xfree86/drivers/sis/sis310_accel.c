/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/sis/sis310_accel.c,v 1.2 2003/01/29 15:42:16 eich Exp $ */
/*
 * 2D Acceleration for SiS 310/325 series (315, 550, 650, 740, M650, 651)
 *
 * Copyright 2002 by Thomas Winischhofer, Vienna, Austria
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Thomas Winischhofer not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  Thomas Winischhofer makes no representations
 * about the suitability of this software for any purpose.  It is provided
 * "as is" without express or implied warranty.
 *
 * THOMAS WINISCHHOFER DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL THOMAS WINISCHHOFER BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 *
 * Based on sis300_accel.c
 *
 *      Author:  Thomas Winischhofer <thomas@winischhofer.net>
 *
 */

#include "xf86.h"
#include "xf86_OSproc.h"
#include "xf86_ansic.h"
#include "xf86PciInfo.h"
#include "xf86Pci.h"
#include "compiler.h"
#include "xaa.h"

#include "sis.h"
#include "sis310_accel.h"

#ifdef SISDUALHEAD
/* TW: This is the offset to the memory for each head */
#define HEADOFFSET 	(pSiS->dhmOffset)
#endif

#undef TRAP     	/* TW: Use/Don't use Trapezoid Fills - does not work - XAA provides
		         * illegal trapezoid data (left and right edges cross each other
			 * sometimes) which causes drawing errors.
                         */

#define CTSCE           /* Use/Don't use CPUToScreenColorExpand. */

/* Accelerator functions */
static void SiSInitializeAccelerator(ScrnInfoPtr pScrn);
static void SiSSync(ScrnInfoPtr pScrn);
static void SiSSetupForScreenToScreenCopy(ScrnInfoPtr pScrn,
                                int xdir, int ydir, int rop,
                                unsigned int planemask, int trans_color);
static void SiSSubsequentScreenToScreenCopy(ScrnInfoPtr pScrn,
                                int x1, int y1, int x2, int y2,
                                int width, int height);
static void SiSSetupForSolidFill(ScrnInfoPtr pScrn, int color,
                                int rop, unsigned int planemask);
static void SiSSubsequentSolidFillRect(ScrnInfoPtr pScrn,
                                int x, int y, int w, int h);
#ifdef TRAP
static void SiSSubsequentSolidFillTrap(ScrnInfoPtr pScrn, int y, int h,
	                        int left, int dxL, int dyL, int eL,
	                        int right, int dxR, int dyR, int eR);
#endif
static void SiSSetupForSolidLine(ScrnInfoPtr pScrn, int color,
                                int rop, unsigned int planemask);
static void SiSSubsequentSolidTwoPointLine(ScrnInfoPtr pScrn, int x1,
                                int y1, int x2, int y2, int flags);
static void SiSSubsequentSolidHorzVertLine(ScrnInfoPtr pScrn,
                                int x, int y, int len, int dir);
static void SiSSetupForDashedLine(ScrnInfoPtr pScrn,
                                int fg, int bg, int rop, unsigned int planemask,
                                int length, unsigned char *pattern);
static void SiSSubsequentDashedTwoPointLine(ScrnInfoPtr pScrn,
                                int x1, int y1, int x2, int y2,
                                int flags, int phase);
static void SiSSetupForMonoPatternFill(ScrnInfoPtr pScrn,
                                int patx, int paty, int fg, int bg,
                                int rop, unsigned int planemask);
static void SiSSubsequentMonoPatternFill(ScrnInfoPtr pScrn,
                                int patx, int paty,
                                int x, int y, int w, int h);
#ifdef TRAP
static void SiSSubsequentMonoPatternFillTrap(ScrnInfoPtr pScrn,
                                int patx, int paty,
                                int y, int h,
                                int left, int dxL, int dyL, int eL,
	                        int right, int dxR, int dyR, int eR);
#endif
#ifdef CTSCE
static void SiSSetupForScanlineCPUToScreenColorExpandFill(ScrnInfoPtr pScrn,
                                int fg, int bg, int rop,
                                unsigned int planemask);
static void SiSSubsequentScanlineCPUToScreenColorExpandFill(ScrnInfoPtr pScrn,
                                int x, int y, int w, int h,
                                int skipleft);
static void SiSSubsequentColorExpandScanline(ScrnInfoPtr pScrn, int bufno);
#endif

#ifdef SISDUALHEAD
static void SiSRestoreAccelState(ScrnInfoPtr pScrn);
#endif

static void
SiSInitializeAccelerator(ScrnInfoPtr pScrn)
{
	SISPtr  pSiS = SISPTR(pScrn);

	pSiS->DoColorExpand = FALSE;
}

Bool
SiS310AccelInit(ScreenPtr pScreen)
{
	XAAInfoRecPtr   infoPtr;
	ScrnInfoPtr     pScrn = xf86Screens[pScreen->myNum];
	SISPtr          pSiS = SISPTR(pScrn);
	int		topFB;
	int             reservedFbSize;
	int             UsableFbSize;
	unsigned char   *AvailBufBase;
	BoxRec          Avail;
	int             i;

	pSiS->AccelInfoPtr = infoPtr = XAACreateInfoRec();
	if (!infoPtr)
		return FALSE;

	SiSInitializeAccelerator(pScrn);

	infoPtr->Flags = LINEAR_FRAMEBUFFER |
			 OFFSCREEN_PIXMAPS |
			 PIXMAP_CACHE;

	/* sync */
	infoPtr->Sync = SiSSync;

	if ((pScrn->bitsPerPixel != 8) && (pScrn->bitsPerPixel != 16) &&
		(pScrn->bitsPerPixel != 32))
			return FALSE;

	/* BitBlt */
	infoPtr->SetupForScreenToScreenCopy = SiSSetupForScreenToScreenCopy;
	infoPtr->SubsequentScreenToScreenCopy = SiSSubsequentScreenToScreenCopy;
	infoPtr->ScreenToScreenCopyFlags = NO_PLANEMASK | TRANSPARENCY_GXCOPY_ONLY;
					 /*| NO_TRANSPARENCY; */

	/* solid fills */
	infoPtr->SetupForSolidFill = SiSSetupForSolidFill;
	infoPtr->SubsequentSolidFillRect = SiSSubsequentSolidFillRect;
#ifdef TRAP
	infoPtr->SubsequentSolidFillTrap = SiSSubsequentSolidFillTrap;
#endif
	infoPtr->SolidFillFlags = NO_PLANEMASK;

	/* solid line */
	infoPtr->SetupForSolidLine = SiSSetupForSolidLine;
	infoPtr->SubsequentSolidTwoPointLine = SiSSubsequentSolidTwoPointLine;
	infoPtr->SubsequentSolidHorVertLine = SiSSubsequentSolidHorzVertLine;
	infoPtr->SolidLineFlags = NO_PLANEMASK;

	/* dashed line */
	infoPtr->SetupForDashedLine = SiSSetupForDashedLine;
	infoPtr->SubsequentDashedTwoPointLine = SiSSubsequentDashedTwoPointLine;
	infoPtr->DashPatternMaxLength = 64;
	infoPtr->DashedLineFlags = NO_PLANEMASK |
				   LINE_PATTERN_MSBFIRST_LSBJUSTIFIED;

	/* 8x8 mono pattern fill */
	infoPtr->SetupForMono8x8PatternFill = SiSSetupForMonoPatternFill;
	infoPtr->SubsequentMono8x8PatternFillRect = SiSSubsequentMonoPatternFill;
#ifdef TRAP
	infoPtr->SubsequentMono8x8PatternFillTrap = SiSSubsequentMonoPatternFillTrap;
#endif
	infoPtr->Mono8x8PatternFillFlags = NO_PLANEMASK |
					   HARDWARE_PATTERN_SCREEN_ORIGIN |
					   HARDWARE_PATTERN_PROGRAMMED_BITS |
					   NO_TRANSPARENCY |
					   BIT_ORDER_IN_BYTE_MSBFIRST ;

#if 0
	/* Screen To Screen Color Expand */
	/* TW: The hardware does not seem to support this the way we need it */
	infoPtr->SetupForScreenToScreenColorExpandFill =
	    			SiSSetupForScreenToScreenColorExpand;
	infoPtr->SubsequentScreenToScreenColorExpandFill =
	    			SiSSubsequentScreenToScreenColorExpand;
	infoPtr->ScreenToScreenColorExpandFillFlags = NO_PLANEMASK |
	                                              BIT_ORDER_IN_BYTE_MSBFIRST ;
#endif

	/* per-scanline color expansion - indirect method */
	pSiS->ColorExpandBufferNumber = 16;
	pSiS->ColorExpandBufferCountMask = 0x0F;
	pSiS->PerColorExpandBufferSize = ((pScrn->virtualX + 31)/32) * 4;
#ifdef CTSCE
	infoPtr->NumScanlineColorExpandBuffers = pSiS->ColorExpandBufferNumber;
	infoPtr->ScanlineColorExpandBuffers = (unsigned char **)&pSiS->ColorExpandBufferAddr[0];
	infoPtr->SetupForScanlineCPUToScreenColorExpandFill = SiSSetupForScanlineCPUToScreenColorExpandFill;
	infoPtr->SubsequentScanlineCPUToScreenColorExpandFill = SiSSubsequentScanlineCPUToScreenColorExpandFill;
	infoPtr->SubsequentColorExpandScanline = SiSSubsequentColorExpandScanline;
	infoPtr->ScanlineCPUToScreenColorExpandFillFlags =
	    NO_PLANEMASK |
	    CPU_TRANSFER_PAD_DWORD |
	    SCANLINE_PAD_DWORD |
	    BIT_ORDER_IN_BYTE_MSBFIRST |
	    LEFT_EDGE_CLIPPING;
#endif

#ifdef SISDUALHEAD
	if (pSiS->DualHeadMode) {
		infoPtr->RestoreAccelState = SiSRestoreAccelState;
	}
#endif

	/* init Frame Buffer Manager */

	topFB = pSiS->maxxfbmem;

	reservedFbSize = (pSiS->ColorExpandBufferNumber
			   * pSiS->PerColorExpandBufferSize);
        /* TW: New for MaxXFBmem Option */
	UsableFbSize = topFB - reservedFbSize;
	/* Layout:
	 * |--------------++++++++++++++++++++^************==========~~~~~~~~~~~~|
	 *   UsableFbSize  ColorExpandBuffers |  DRI-Heap   HWCursor  CommandQueue
	 *                                 topFB
	 */
	AvailBufBase = pSiS->FbBase + UsableFbSize;
	for (i = 0; i < pSiS->ColorExpandBufferNumber; i++) {
		pSiS->ColorExpandBufferAddr[i] = AvailBufBase + 
		    i * pSiS->PerColorExpandBufferSize;
		pSiS->ColorExpandBufferScreenOffset[i] = UsableFbSize +
		    i * pSiS->PerColorExpandBufferSize;
	}
	Avail.x1 = 0;
	Avail.y1 = 0;
	Avail.x2 = pScrn->displayWidth;
	Avail.y2 = UsableFbSize
	    / (pScrn->displayWidth * pScrn->bitsPerPixel/8) - 1;
	if (Avail.y2 < 0)
		Avail.y2 = 32767;
	if (Avail.y2 < pScrn->currentMode->VDisplay) {
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
		   Avail.x1, Avail.y1, Avail.x2, Avail.y2);
	
	xf86InitFBManager(pScreen, &Avail);

	return(XAAInit(pScreen, infoPtr));
}

static void
SiSSync(ScrnInfoPtr pScrn)
{
	SISPtr pSiS = SISPTR(pScrn);

	PDEBUG(ErrorF("SiSSync()\n"));

	pSiS->DoColorExpand = FALSE;
	SiSIdle
}

#ifdef SISDUALHEAD
static void
SiSRestoreAccelState(ScrnInfoPtr pScrn)
{
	SISPtr pSiS = SISPTR(pScrn);

	/* TW: We don't need to do anything special here */
	pSiS->DoColorExpand = FALSE;
	SiSIdle
}
#endif

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

static void SiSSetupForScreenToScreenCopy(ScrnInfoPtr pScrn,
                                int xdir, int ydir, int rop,
                                unsigned int planemask, int trans_color)
{
	SISPtr  pSiS = SISPTR(pScrn);

	PDEBUG(ErrorF("Setup ScreenCopy(%d, %d, 0x%x, 0x%x, 0x%x)\n",
			xdir, ydir, rop, planemask, trans_color));

	/* "AGP base" - color depth depending value (see sis_vga.c) */
	SiSSetupDSTColorDepth(pSiS->DstColor);
	/* SRC pitch */
	SiSSetupSRCPitch(pSiS->scrnOffset)
	/* DST pitch and height (-1 for disabling merge-clipping) */
	SiSSetupDSTRect(pSiS->scrnOffset, -1)
	/* Init CommandReg and set ROP */
	if (trans_color != -1) {
		SiSSetupROP(0x0A)
		SiSSetupSRCTrans(trans_color)
		SiSSetupCMDFlag(TRANSPARENT_BITBLT)
	} else {
	        SiSSetupROP(sisALUConv[rop])
		/* Set command - not needed, both 0 */
		/* SiSSetupCMDFlag(BITBLT | SRCVIDEO) */
	}
	/* Set some color depth depending value (see sis_vga.c) */
	SiSSetupCMDFlag(pSiS->SiS310_AccelDepth)

	/* TW: The 310/325 series is smart enough to know the direction */
}

static void SiSSubsequentScreenToScreenCopy(ScrnInfoPtr pScrn,
                                int src_x, int src_y, int dst_x, int dst_y,
                                int width, int height)
{
	SISPtr  pSiS = SISPTR(pScrn);
	long srcbase, dstbase;

	PDEBUG(ErrorF("Subsequent ScreenCopy(%d,%d, %d,%d, %d,%d)\n",
			  src_x, src_y, dst_x, dst_y, width, height));

	srcbase = dstbase = 0;
	if (src_y >= 2048) {
		srcbase = pSiS->scrnOffset * src_y;
		src_y = 0;
	}
	if ((dst_y >= pScrn->virtualY) || (dst_y >= 2048)) {
		dstbase = pSiS->scrnOffset*dst_y;
		dst_y = 0;
	}
#ifdef SISDUALHEAD
	srcbase += HEADOFFSET;
	dstbase += HEADOFFSET;
#endif
	SiSSetupSRCBase(srcbase);
	SiSSetupDSTBase(dstbase);
	SiSSetupRect(width, height)
	SiSSetupSRCXY(src_x, src_y)
	SiSSetupDSTXY(dst_x, dst_y)
	SiSDoCMD
}

static void
SiSSetupForSolidFill(ScrnInfoPtr pScrn, int color,
			int rop, unsigned int planemask)
{
	SISPtr  pSiS = SISPTR(pScrn);

	PDEBUG(ErrorF("Setup SolidFill(0x%x, 0x%x, 0x%x)\n",
					color, rop, planemask));

	SiSSetupPATFG(color)
	SiSSetupDSTRect(pSiS->scrnOffset, -1)
	SiSSetupDSTColorDepth(pSiS->DstColor);
	SiSSetupROP(sisPatALUConv[rop])
	SiSSetupCMDFlag(PATFG | pSiS->SiS310_AccelDepth)
}

static void
SiSSubsequentSolidFillRect(ScrnInfoPtr pScrn,
                        int x, int y, int w, int h)
{
	SISPtr  pSiS = SISPTR(pScrn);
	long dstbase;

	PDEBUG(ErrorF("Subsequent SolidFillRect(%d, %d, %d, %d)\n",
					x, y, w, h));
	dstbase = 0;
	if (y >= 2048) {
		dstbase=pSiS->scrnOffset*y;
		y = 0;
	}
#ifdef SISDUALHEAD
	dstbase += HEADOFFSET;
#endif
	SiSSetupDSTBase(dstbase)
	SiSSetupDSTXY(x,y)
	SiSSetupRect(w,h)
	pSiS->CommandReg &= ~(T_XISMAJORL | T_XISMAJORR |
	                      T_L_X_INC | T_L_Y_INC |
	                      T_R_X_INC | T_R_Y_INC |
			      TRAPAZOID_FILL);
	SiSSetupCMDFlag(BITBLT)
	SiSDoCMD
}

/* TW: Trapezoid */
/* This would work better if XAA would provide us with valid trapezoids.
 * In fact, with small trapezoids the left and the right edge often cross
 * each other which causes drawing errors (filling over whole scanline).
 */
#ifdef TRAP
static void
SiSSubsequentSolidFillTrap(ScrnInfoPtr pScrn, int y, int h,
	       int left,  int dxL, int dyL, int eL,
	       int right, int dxR, int dyR, int eR )
{
	SISPtr  pSiS = SISPTR(pScrn);
	long dstbase;
#if 0
	float kL, kR;
#endif

	dstbase = 0;
	if (y >= 2048) {
		dstbase=pSiS->scrnOffset*y;
		y = 0;
	}
#ifdef SISDUALHEAD
	dstbase += HEADOFFSET;
#endif
	SiSSetupDSTBase(dstbase)

#if 1
	SiSSetupPATFG(0xff0000) /* FOR TESTING */
#endif

	/* Clear CommandReg because SetUp can be used for Rect and Trap */
	pSiS->CommandReg &= ~(T_L_X_INC | T_L_Y_INC |
	                      T_R_X_INC | T_R_Y_INC |
	                      T_XISMAJORL | T_XISMAJORR |
			      BITBLT);

        xf86DrvMsg(0, X_INFO, "Trap (%d %d %d %d) dxL %d dyL %d eL %d   dxR %d dyR %d eR %d\n",
		left, right, y, h, dxL, dyL, eL, dxR, dyR, eR);

	/* Unfortunately, we must check if the right and the left edge
	 * cross each other...  INCOMPLETE (equation wrong)
	 */
#if 0
	if (dxL == 0) kL = 0;
	else kL = (float)dyL / (float)dxL;
	if (dxR == 0) kR = 0;
	else kR = (float)dyR / (float)dxR;
	xf86DrvMsg(0, X_INFO, "kL %f kR %f!\n", kL, kR);
	if ( (kR != kL) &&
	     (!(kR == 0 && kL == 0)) &&
	     (!(kR <  0 && kL >  0)) ) {
	   xf86DrvMsg(0, X_INFO, "Inside if (%f - %d)\n", ( kL * ( ( ((float)right - (float)left) / (kL - kR) ) - left) + y), h+y);
           if ( ( ( kL * ( ( ((float)right - (float)left) / (kL - kR) ) - (float)left) + (float)y) < (h + y) ) ) {
	     xf86DrvMsg(0, X_INFO, "Cross detected!\n");
	   }
	}
#endif

	/* Determine egde angles */
	if (dxL < 0) { dxL = -dxL; }
	else { SiSSetupCMDFlag(T_L_X_INC) }
	if (dxR < 0) { dxR = -dxR; }
	else { SiSSetupCMDFlag(T_R_X_INC) }

	/* (Y direction always positive - do this anyway) */
	if (dyL < 0) { dyL = -dyL; }
	else { SiSSetupCMDFlag(T_L_Y_INC) }
	if (dyR < 0) { dyR = -dyR; }
	else { SiSSetupCMDFlag(T_R_Y_INC) }

	/* Determine major axis */
	if (dxL >= dyL) {      /* X is major axis */
		SiSSetupCMDFlag(T_XISMAJORL)
	}
	if (dxR >= dyR) {      /* X is major axis */
		SiSSetupCMDFlag(T_XISMAJORR)
	}

	/* Set up deltas */
	SiSSetupdL(dxL, dyL)
	SiSSetupdR(dxR, dyR)

	/* Set up y, h, left, right */
	SiSSetupYH(y,h)
	SiSSetupLR(left,right)

	/* Set up initial error term */
	SiSSetupEL(eL)
	SiSSetupER(eR)

	SiSSetupCMDFlag(TRAPAZOID_FILL);

	SiSDoCMD
}
#endif

static void
SiSSetupForSolidLine(ScrnInfoPtr pScrn, int color, int rop,
                                     unsigned int planemask)
{
	SISPtr  pSiS = SISPTR(pScrn);

	PDEBUG(ErrorF("Setup SolidLine(0x%x, 0x%x, 0x%x)\n",
					color, rop, planemask));

	SiSSetupLineCount(1)
	SiSSetupPATFG(color)
	SiSSetupDSTRect(pSiS->scrnOffset, -1)
	SiSSetupDSTColorDepth(pSiS->DstColor);
	SiSSetupROP(sisPatALUConv[rop])
	SiSSetupCMDFlag(PATFG | LINE | pSiS->SiS310_AccelDepth)
}

static void
SiSSubsequentSolidTwoPointLine(ScrnInfoPtr pScrn,
                        int x1, int y1, int x2, int y2, int flags)
{
	SISPtr  pSiS = SISPTR(pScrn);
	long dstbase,miny,maxy;

	PDEBUG(ErrorF("Subsequent SolidLine(%d, %d, %d, %d, 0x%x)\n",
					x1, y1, x2, y2, flags));

	dstbase = 0;
	miny = (y1 > y2) ? y2 : y1;
	maxy = (y1 > y2) ? y1 : y2;
	if (maxy >= 2048) {
		dstbase = pSiS->scrnOffset*miny;
		y1 -= miny;
		y2 -= miny;
	}
#ifdef SISDUALHEAD
	dstbase += HEADOFFSET;
#endif
	SiSSetupDSTBase(dstbase)

	SiSSetupX0Y0(x1,y1)
	SiSSetupX1Y1(x2,y2)
	if (flags & OMIT_LAST) {
		SiSSetupCMDFlag(NO_LAST_PIXEL)
	} else {
		pSiS->CommandReg &= ~(NO_LAST_PIXEL);
	}
	SiSDoCMD
}

static void
SiSSubsequentSolidHorzVertLine(ScrnInfoPtr pScrn,
                                int x, int y, int len, int dir)
{
	SISPtr  pSiS = SISPTR(pScrn);
	long dstbase;

	PDEBUG(ErrorF("Subsequent SolidHorzVertLine(%d, %d, %d, %d)\n",
					x, y, len, dir));

	len--; /* starting point is included! */
	dstbase = 0;
	if ((y >= 2048) || ((y + len) >= 2048)) {
		dstbase = pSiS->scrnOffset * y;
		y = 0;
	}
#ifdef SISDUALHEAD
	dstbase += HEADOFFSET;
#endif
	SiSSetupDSTBase(dstbase)

	SiSSetupX0Y0(x,y)
	if (dir == DEGREES_0) {
		SiSSetupX1Y1(x + len, y);
	} else {
		SiSSetupX1Y1(x, y + len);
	}
	SiSDoCMD
}

static void
SiSSetupForDashedLine(ScrnInfoPtr pScrn,
                                int fg, int bg, int rop, unsigned int planemask,
                                int length, unsigned char *pattern)
{
	SISPtr pSiS = SISPTR(pScrn);

	PDEBUG(ErrorF("Setup DashedLine(0x%x, 0x%x, 0x%x, 0x%x, %d, 0x%x:%x)\n",
			fg, bg, rop, planemask, length, *(pattern+4), *pattern));

	SiSSetupLineCount(1)
	SiSSetupDSTRect(pSiS->scrnOffset, -1)
	SiSSetupDSTColorDepth(pSiS->DstColor);
	SiSSetupStyleLow(*pattern)
	SiSSetupStyleHigh(*(pattern+4))
	SiSSetupStylePeriod(length-1);			/* TW: This was missing!!! */
	SiSSetupROP(sisPatALUConv[rop])
	SiSSetupPATFG(fg)
	SiSSetupCMDFlag(LINE | LINE_STYLE)  		/* TW: This was missing!!! */
	if (bg != -1) {
		SiSSetupPATBG(bg)
	} else {
		SiSSetupCMDFlag(TRANSPARENT)		/* TW: This was missing!!! */
	}
	SiSSetupCMDFlag(pSiS->SiS310_AccelDepth)
}

static void
SiSSubsequentDashedTwoPointLine(ScrnInfoPtr pScrn,
                                int x1, int y1, int x2, int y2,
                                int flags, int phase)
{
	SISPtr pSiS = SISPTR(pScrn);
	long dstbase,miny,maxy;

	PDEBUG(ErrorF("Subsequent DashedLine(%d,%d, %d,%d, 0x%x,0x%x)\n",
			x1, y1, x2, y2, flags, phase));

	dstbase = 0;
	miny=(y1 > y2) ? y2 : y1;
	maxy=(y1 > y2) ? y1 : y2;
	if (maxy >= 2048) {
		dstbase = pSiS->scrnOffset * miny;
		y1 -= miny;
		y2 -= miny;
	}
#ifdef SISDUALHEAD
	dstbase += HEADOFFSET;
#endif
	SiSSetupDSTBase(dstbase)

	SiSSetupX0Y0(x1,y1)
	SiSSetupX1Y1(x2,y2)
	if (flags & OMIT_LAST) {
		SiSSetupCMDFlag(NO_LAST_PIXEL)
	} else {
		pSiS->CommandReg &= ~(NO_LAST_PIXEL);
	}
	SiSDoCMD
}

static void
SiSSetupForMonoPatternFill(ScrnInfoPtr pScrn,
                                int patx, int paty, int fg, int bg,
                                int rop, unsigned int planemask)
{
	SISPtr pSiS = SISPTR(pScrn);

	PDEBUG(ErrorF("Setup MonoPatFill(0x%x,0x%x, 0x%x,0x%x, 0x%x, 0x%x)\n",
					patx, paty, fg, bg, rop, planemask));
	SiSSetupDSTRect(pSiS->scrnOffset, -1)
	SiSSetupDSTColorDepth(pSiS->DstColor);
	SiSSetupMONOPAT(patx,paty)
	SiSSetupPATFG(fg)
	SiSSetupROP(sisPatALUConv[rop])
	SiSSetupCMDFlag(PATMONO | pSiS->SiS310_AccelDepth)
	SiSSetupPATBG(bg)
}

static void
SiSSubsequentMonoPatternFill(ScrnInfoPtr pScrn,
                                int patx, int paty,
                                int x, int y, int w, int h)
{
	SISPtr pSiS = SISPTR(pScrn);
	long dstbase;

	PDEBUG(ErrorF("Subsequent MonoPatFill(0x%x,0x%x, %d,%d, %d,%d)\n",
							patx, paty, x, y, w, h));
	dstbase = 0;
	if (y >= 2048) {
		dstbase = pSiS->scrnOffset * y;
		y = 0;
	}
#ifdef SISDUALHEAD
	dstbase += HEADOFFSET;
#endif
	SiSSetupDSTBase(dstbase)
	SiSSetupDSTXY(x,y)
	SiSSetupRect(w,h)
	/* Clear commandReg because Setup can be used for Rect and Trap */
	pSiS->CommandReg &= ~(T_XISMAJORL | T_XISMAJORR |
	                      T_L_X_INC | T_L_Y_INC |
	                      T_R_X_INC | T_R_Y_INC |
	                      TRAPAZOID_FILL);
	SiSDoCMD
}

/* TW: Trapezoid */
#ifdef TRAP
static void
SiSSubsequentMonoPatternFillTrap(ScrnInfoPtr pScrn,
               int patx, int paty,
               int y, int h,
	       int left, int dxL, int dyL, int eL,
	       int right, int dxR, int dyR, int eR)
{
	SISPtr  pSiS = SISPTR(pScrn);
	long dstbase;

	PDEBUG(ErrorF("Subsequent Mono8x8PatternFillTrap(%d, %d, %d - %d %d/%d %d/%d)\n",
					y, h, left, right, dxL, dxR, eL, eR));

	dstbase = 0;
	if (y >= 2048) {
		dstbase=pSiS->scrnOffset*y;
		y = 0;
	}
#ifdef SISDUALHEAD
	dstbase += HEADOFFSET;
#endif
	SiSSetupDSTBase(dstbase)

	/* Clear CommandReg because SetUp can be used for Rect and Trap */
	pSiS->CommandReg &= ~(T_XISMAJORL | T_XISMAJORR |
	                      T_L_X_INC | T_L_Y_INC |
			      T_R_X_INC | T_R_Y_INC |
			      BITBLT);

	if (dxL < 0) { dxL = -dxL;  }
	else { SiSSetupCMDFlag(T_L_X_INC) }
	if (dxR < 0) { dxR = -dxR; }
	else { SiSSetupCMDFlag(T_R_X_INC) }

	if (dyL < 0) { dyL = -dyL; }
	else { SiSSetupCMDFlag(T_L_Y_INC) }
	if (dyR < 0) { dyR = -dyR; }
	else { SiSSetupCMDFlag(T_R_Y_INC) }

	/* Determine major axis */
	if (dxL >= dyL) {      /* X is major axis */
		SiSSetupCMDFlag(T_XISMAJORL)
	}
	if (dxR >= dyR) {      /* X is major axis */
		SiSSetupCMDFlag(T_XISMAJORR)
	}

	SiSSetupYH(y,h)
	SiSSetupLR(left,right)

	SiSSetupdL(dxL, dyL)
	SiSSetupdR(dxR, dyR)

	SiSSetupEL(eL)
	SiSSetupER(eR)

	SiSSetupCMDFlag(TRAPAZOID_FILL);

	SiSDoCMD
}
#endif

/* ---- CPUToScreen Color Expand */

#ifdef CTSCE
/* We use the indirect method */
static void
SiSSetupForScanlineCPUToScreenColorExpandFill(ScrnInfoPtr pScrn,
		int fg, int bg, int rop, unsigned int planemask)
{
	SISPtr pSiS=SISPTR(pScrn);

	/* TW: FIXME: How do I check the "CPU driven blit stage" on the
	 * 310/325 series?
	 * That's the 300 series method but definitely wrong for
	 * 310/325 series (bit 28 is already used for idle!)
	 */
	/* while ((MMIO_IN16(pSiS->IOBase, Q_STATUS+2) & 0x1F00) != 0) {} */

	/* TW: Do Idle instead... */
	SiSIdle

	SiSSetupSRCXY(0,0);
	SiSSetupROP(sisALUConv[rop]);
	SiSSetupSRCFG(fg);
	SiSSetupDSTRect(pSiS->scrnOffset, -1);
	SiSSetupDSTColorDepth(pSiS->DstColor);
	if (bg == -1) {
		SiSSetupCMDFlag(TRANSPARENT | ENCOLOREXP | SRCCPUBLITBUF
					| pSiS->SiS310_AccelDepth);
	} else {
		SiSSetupSRCBG(bg);
		SiSSetupCMDFlag(ENCOLOREXP | SRCCPUBLITBUF
					| pSiS->SiS310_AccelDepth);
	};
}

static void
SiSSubsequentScanlineCPUToScreenColorExpandFill(
                        ScrnInfoPtr pScrn, int x, int y, int w,
                        int h, int skipleft)
{
	SISPtr pSiS = SISPTR(pScrn);
	int _x0, _y0, _x1, _y1;
	long dstbase;

	dstbase = 0;
	if (y >= 2048) {
		dstbase = pSiS->scrnOffset*y;
		y = 0;
	}
#ifdef SISDUALHEAD
	dstbase += HEADOFFSET;
#endif

        if((MMIO_IN16(pSiS->IOBase, Q_STATUS+2) & 0x8000) != 0x8000) {
		SiSIdle;
        }

	SiSSetupDSTBase(dstbase)

	if (skipleft > 0) {
		_x0 = x+skipleft;
		_y0 = y;
		_x1 = x+w;
		_y1 = y+h;
		SiSSetupClipLT(_x0, _y0);
		SiSSetupClipRB(_x1, _y1);
		SiSSetupCMDFlag(CLIPENABLE);
	} else {
		pSiS->CommandReg &= (~CLIPENABLE);
	}
	SiSSetupRect(w, 1);
	SiSSetupSRCPitch(((((w+7)/8)+3) >> 2) * 4);
	pSiS->ycurrent = y;
	pSiS->xcurrent = x;
}

static void
SiSSubsequentColorExpandScanline(ScrnInfoPtr pScrn, int bufno)
{
	SISPtr pSiS=SISPTR(pScrn);
	long cbo;

	cbo = pSiS->ColorExpandBufferScreenOffset[bufno];
#ifdef SISDUALHEAD
	cbo += HEADOFFSET;
#endif

	if((MMIO_IN16(pSiS->IOBase, Q_STATUS+2) & 0x8000) != 0x8000) {
		SiSIdle;
        }

	SiSSetupSRCBase(cbo);

	SiSSetupDSTXY(pSiS->xcurrent, pSiS->ycurrent);

	SiSDoCMD

	pSiS->ycurrent++;

	SiSIdle
}
#endif



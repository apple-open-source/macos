/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/sis/sis310_accel.c,v 1.41 2004/02/25 17:45:11 twini Exp $ */
/*
 * 2D Acceleration for SiS 315 and 330 series
 *
 * Copyright (C) 2001-2004 by Thomas Winischhofer, Vienna, Austria
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1) Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2) Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3) The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESSED OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *
 * Author:  	Thomas Winischhofer <thomas@winischhofer.net>
 *
 * 2003/08/18: Rewritten for using VRAM command queue
 *
 */

#include "xf86.h"
#include "xf86_OSproc.h"
#include "xf86_ansic.h"
#include "xf86PciInfo.h"
#include "xf86Pci.h"
#include "compiler.h"
#include "xaa.h"
#include "xaalocal.h"
#include "xaarop.h"

#include "sis.h"
#include "sis310_accel.h"

#if 0
#define ACCELDEBUG
#endif

#ifdef SISDUALHEAD
#define HEADOFFSET 	(pSiS->dhmOffset)
#endif

#undef TRAP     	/* Use/Don't use Trapezoid Fills
			 * DOES NOT WORK. XAA sometimes provides illegal
			 * trapezoid data (left and right edges cross each
			 * other) which causes drawing errors. Since
			 * checking the trapezoid for such a case is very
			 * time-intensive, it is faster to let it be done
			 * by the generic polygon functions.
			 * Does not work on 330 series at all, hangs the engine.
			 * Even with correct trapezoids, this is slower than
			 * doing it by the CPU.
                         */

#undef CTSCE          	/* Use/Don't use CPUToScreenColorExpand. Disabled
			 * because it is slower than doing it by the CPU.
			 * Indirect mode does not work in VRAM queue mode.
			 * Does not work on 330 series (even in MMIO mode).
			 */
#undef CTSCE_DIRECT    	/* Use direct method - This works (on both 315 and 330 at
			 * least in VRAM queue mode) but we don't use this either,
			 * because it's slower than doing it by the CPU. (Using it
			 * would require defining CTSCE)
			 */

#undef STSCE    	/* Use/Don't use ScreenToScreenColorExpand - does not work,
  			 * see comments below.
			 */

#define INCL_RENDER	/* Use/Don't use RENDER extension acceleration */

#ifdef INCL_RENDER
#ifdef RENDER
#include "mipict.h"
#include "dixstruct.h"
#endif
#endif

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
#ifdef SISVRAMQ
static void SiSSetupForColor8x8PatternFill(ScrnInfoPtr pScrn,
				int patternx, int patterny,
				int rop, unsigned int planemask, int trans_col);
static void SiSSubsequentColor8x8PatternFillRect(ScrnInfoPtr pScrn,
				int patternx, int patterny, int x, int y,
				int w, int h);
#endif
#ifdef STSCE
static void SiSSetupForScreenToScreenColorExpand(ScrnInfoPtr pScrn,
                                int fg, int bg,
                                int rop, unsigned int planemask);
static void SiSSubsequentScreenToScreenColorExpand(ScrnInfoPtr pScrn,
                                int x, int y, int w, int h,
                                int srcx, int srcy, int skipleft);
#endif
#ifdef CTSCE
#ifdef CTSCE_DIRECT
static void SiSSetupForCPUToScreenColorExpandFill(ScrnInfoPtr pScrn,
                                int fg, int bg, int rop,
                                unsigned int planemask);
static void SiSSubsequentCPUToScreenColorExpandFill(ScrnInfoPtr pScrn,
                                int x, int y, int w, int h,
                                int skipleft);
#else
static void SiSSetupForScanlineCPUToScreenColorExpandFill(ScrnInfoPtr pScrn,
                                int fg, int bg, int rop,
                                unsigned int planemask);
static void SiSSubsequentScanlineCPUToScreenColorExpandFill(ScrnInfoPtr pScrn,
                                int x, int y, int w, int h,
                                int skipleft);
static void SiSSubsequentColorExpandScanline(ScrnInfoPtr pScrn, int bufno);
#endif
#endif
#ifdef INCL_RENDER
#ifdef RENDER
extern Bool SiSSetupForCPUToScreenAlphaTexture(ScrnInfoPtr pScrn,
				int op, CARD16 red, CARD16 green,
				CARD16 blue, CARD16 alpha,
				int alphaType, CARD8 *alphaPtr,
				int alphaPitch, int width,
				int height, int	flags);

extern Bool SiSSetupForCPUToScreenTexture( ScrnInfoPtr pScrn,
				int op, int texType, CARD8 *texPtr,
				int texPitch, int width,
				int height, int	flags);

extern void SiSSubsequentCPUToScreenTexture(ScrnInfoPtr	pScrn,
				int dstx, int dsty,
				int srcx, int srcy,
				int width, int height);

extern CARD32 SiSAlphaTextureFormats[2];
extern CARD32 SiSTextureFormats[2];
CARD32 SiSAlphaTextureFormats[2] = { PICT_a8      , 0 };
CARD32 SiSTextureFormats[2]      = { PICT_a8r8g8b8, 0 };
#endif
#endif

#ifdef SISDUALHEAD
static void SiSRestoreAccelState(ScrnInfoPtr pScrn);
#endif

static void
SiSInitializeAccelerator(ScrnInfoPtr pScrn)
{
	SISPtr  pSiS = SISPTR(pScrn);

	pSiS->DoColorExpand = FALSE;
	pSiS->alphaBlitBusy = FALSE;
#ifndef SISVRAMQ
	if(pSiS->ChipFlags & SiSCF_Integrated) {
	   CmdQueLen = 0;
        } else {
	   CmdQueLen = ((128 * 1024) / 4) - 64;
        }
#endif
}

Bool
SiS315AccelInit(ScreenPtr pScreen)
{
	XAAInfoRecPtr   infoPtr;
	ScrnInfoPtr     pScrn = xf86Screens[pScreen->myNum];
	SISPtr          pSiS = SISPTR(pScrn);
	int		topFB;
	int             reservedFbSize;
	int             UsableFbSize;
	BoxRec          Avail;
#ifdef SISDUALHEAD
        SISEntPtr       pSiSEnt = NULL;
#endif
#ifdef CTSCE
	unsigned char   *AvailBufBase;
#ifndef CTSCE_DIRECT
	int             i;
#endif
#endif

	pSiS->AccelInfoPtr = infoPtr = XAACreateInfoRec();
	if(!infoPtr) return FALSE;

	SiSInitializeAccelerator(pScrn);

	infoPtr->Flags = LINEAR_FRAMEBUFFER |
			 OFFSCREEN_PIXMAPS |
			 PIXMAP_CACHE;

	/* sync */
	infoPtr->Sync = SiSSync;

	if((pScrn->bitsPerPixel != 8) && (pScrn->bitsPerPixel != 16) &&
		(pScrn->bitsPerPixel != 32))
			return FALSE;

#ifdef SISDUALHEAD
	pSiSEnt = pSiS->entityPrivate;
#endif

	/* BitBlt */
	infoPtr->SetupForScreenToScreenCopy = SiSSetupForScreenToScreenCopy;
	infoPtr->SubsequentScreenToScreenCopy = SiSSubsequentScreenToScreenCopy;
	infoPtr->ScreenToScreenCopyFlags = NO_PLANEMASK | TRANSPARENCY_GXCOPY_ONLY;

	/* solid fills */
	infoPtr->SetupForSolidFill = SiSSetupForSolidFill;
	infoPtr->SubsequentSolidFillRect = SiSSubsequentSolidFillRect;
#ifdef TRAP
	if((pSiS->Chipset != PCI_CHIP_SIS660) &&
	   (pSiS->Chipset != PCI_CHIP_SIS330)) {
	   infoPtr->SubsequentSolidFillTrap = SiSSubsequentSolidFillTrap;
	}
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
        if((pSiS->Chipset != PCI_CHIP_SIS660) &&
	   (pSiS->Chipset != PCI_CHIP_SIS330)) {
	   infoPtr->SubsequentMono8x8PatternFillTrap = SiSSubsequentMonoPatternFillTrap;
	}
#endif
	infoPtr->Mono8x8PatternFillFlags = NO_PLANEMASK |
					   HARDWARE_PATTERN_SCREEN_ORIGIN |
					   HARDWARE_PATTERN_PROGRAMMED_BITS |
					   BIT_ORDER_IN_BYTE_MSBFIRST;

#ifdef SISVRAMQ
	/* 8x8 color pattern fill (MMIO support not implemented) */
	infoPtr->SetupForColor8x8PatternFill = SiSSetupForColor8x8PatternFill;
	infoPtr->SubsequentColor8x8PatternFillRect = SiSSubsequentColor8x8PatternFillRect;
	infoPtr->Color8x8PatternFillFlags = NO_PLANEMASK |
	 				    HARDWARE_PATTERN_SCREEN_ORIGIN |
					    NO_TRANSPARENCY;
#endif

#ifdef STSCE
	/* Screen To Screen Color Expand */
	/* The hardware does not support this the way we need it, because
	 * the mono-bitmap is not provided with a pitch of (width), but
	 * with a pitch of scrnOffset (= width * bpp / 8).
	 */
	infoPtr->SetupForScreenToScreenColorExpandFill =
	    			SiSSetupForScreenToScreenColorExpand;
	infoPtr->SubsequentScreenToScreenColorExpandFill =
	    			SiSSubsequentScreenToScreenColorExpand;
	infoPtr->ScreenToScreenColorExpandFillFlags = NO_PLANEMASK |
	                                              BIT_ORDER_IN_BYTE_MSBFIRST ;
#endif

#ifdef CTSCE
#ifdef CTSCE_DIRECT
	/* CPU color expansion - direct method
	 *
	 * We somewhat fake this function here in the following way:
	 * XAA copies its mono-bitmap data not into an aperture, but
	 * into our video RAM buffer. We then do a ScreenToScreen
	 * color expand.
	 * Unfortunately, XAA sends the data to the aperture AFTER
	 * the call to Subsequent(), therefore we do not execute the
	 * command in Subsequent, but in the following call to Sync().
	 * (Hence, the SYNC_AFTER_COLOR_EXPAND flag MUST BE SET)
	 *
	 * This is slower than doing it by the CPU.
	 */

	 pSiS->ColorExpandBufferNumber = 48;
	 pSiS->PerColorExpandBufferSize = ((pScrn->virtualX + 31)/32) * 4;
	 infoPtr->SetupForCPUToScreenColorExpandFill = SiSSetupForCPUToScreenColorExpandFill;
	 infoPtr->SubsequentCPUToScreenColorExpandFill = SiSSubsequentCPUToScreenColorExpandFill;
	 infoPtr->ColorExpandRange = pSiS->ColorExpandBufferNumber * pSiS->PerColorExpandBufferSize;
	 infoPtr->CPUToScreenColorExpandFillFlags =
	     NO_PLANEMASK |
	     CPU_TRANSFER_PAD_DWORD |
	     SCANLINE_PAD_DWORD |
	     BIT_ORDER_IN_BYTE_MSBFIRST |
	     LEFT_EDGE_CLIPPING |
	     SYNC_AFTER_COLOR_EXPAND;
#else
        /* CPU color expansion - per-scanline / indirect method
	 *
	 * SLOW! SLOWER! SLOWEST!
	 *
	 * Does not work on 330 series, hangs the engine (both VRAM and MMIO).
	 * Does not work in VRAM queue mode.
	 */
#ifndef SISVRAMQ
        if((pSiS->Chipset != PCI_CHIP_SIS650) &&
	   (pSiS->Chipset != PCI_CHIP_SIS660) &&
	   (pSiS->Chipset != PCI_CHIP_SIS330)) {
	   pSiS->ColorExpandBufferNumber = 16;
	   pSiS->ColorExpandBufferCountMask = 0x0F;
	   pSiS->PerColorExpandBufferSize = ((pScrn->virtualX + 31)/32) * 4;
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
	} else {
#endif
	   pSiS->ColorExpandBufferNumber = 0;
	   pSiS->PerColorExpandBufferSize = 0;
#ifndef SISVRAMQ
	}
#endif
#endif
#else
        pSiS->ColorExpandBufferNumber = 0;
	pSiS->PerColorExpandBufferSize = 0;
#endif

	pSiS->RenderAccelArray = NULL;

#ifdef INCL_RENDER
#ifdef RENDER
        /* Render */
        if(((pScrn->bitsPerPixel == 16) || (pScrn->bitsPerPixel == 32)) && pSiS->doRender) {
	   int i, j;
#ifdef SISDUALHEAD
	   if(pSiSEnt) pSiS->RenderAccelArray = pSiSEnt->RenderAccelArray;
#endif
	   if(!pSiS->RenderAccelArray) {
	      if((pSiS->RenderAccelArray = xnfcalloc(65536, 1))) {
#ifdef SISDUALHEAD
  	         if(pSiSEnt) pSiSEnt->RenderAccelArray = pSiS->RenderAccelArray;
#endif
	         for(i = 0; i < 256; i++) {
	            for(j = 0; j < 256; j++) {
	               pSiS->RenderAccelArray[(i << 8) + j] = (i * j) / 255;
		    }
	         }
	      }
	   }
	   if(pSiS->RenderAccelArray) {
	      pSiS->AccelLinearScratch = NULL;

	      infoPtr->SetupForCPUToScreenAlphaTexture = SiSSetupForCPUToScreenAlphaTexture;
	      infoPtr->SubsequentCPUToScreenAlphaTexture = SiSSubsequentCPUToScreenTexture;
	      infoPtr->CPUToScreenAlphaTextureFormats = SiSAlphaTextureFormats;
	      infoPtr->CPUToScreenAlphaTextureFlags = XAA_RENDER_NO_TILE;

              infoPtr->SetupForCPUToScreenTexture = SiSSetupForCPUToScreenTexture;
              infoPtr->SubsequentCPUToScreenTexture = SiSSubsequentCPUToScreenTexture;
              infoPtr->CPUToScreenTextureFormats = SiSTextureFormats;
	      infoPtr->CPUToScreenTextureFlags = XAA_RENDER_NO_TILE;
	      xf86DrvMsg(pScrn->scrnIndex, X_INFO, "RENDER acceleration enabled\n");
	   }
	}
#endif
#endif

#ifdef SISDUALHEAD
	if(pSiS->DualHeadMode) {
	   infoPtr->RestoreAccelState = SiSRestoreAccelState;
	}
#endif

	/* Init Frame Buffer Manager */

	topFB = pSiS->maxxfbmem;

	reservedFbSize = (pSiS->ColorExpandBufferNumber
			   * pSiS->PerColorExpandBufferSize);

	UsableFbSize = topFB - reservedFbSize;
	/* Layout:
	 * |--------------++++++++++++++++++++^************==========~~~~~~~~~~~~|
	 *   UsableFbSize  ColorExpandBuffers |  DRI-Heap   HWCursor  CommandQueue
	 *                                 topFB
	 */
#ifdef CTSCE
	AvailBufBase = pSiS->FbBase + UsableFbSize;
	if(pSiS->ColorExpandBufferNumber) {
#ifdef CTSCE_DIRECT
	   infoPtr->ColorExpandBase = (unsigned char *)AvailBufBase;
	   pSiS->ColorExpandBase = UsableFbSize;
#else
	   for(i = 0; i < pSiS->ColorExpandBufferNumber; i++) {
	      pSiS->ColorExpandBufferAddr[i] = AvailBufBase +
		    i * pSiS->PerColorExpandBufferSize;
	      pSiS->ColorExpandBufferScreenOffset[i] = UsableFbSize +
		    i * pSiS->PerColorExpandBufferSize;
	   }
#endif
	}
#endif

	Avail.x1 = 0;
	Avail.y1 = 0;
	Avail.x2 = pScrn->displayWidth;
	Avail.y2 = (UsableFbSize / (pScrn->displayWidth * pScrn->bitsPerPixel/8)) - 1;

	if(Avail.y2 < 0) Avail.y2 = 32767;
	if(Avail.y2 < pScrn->currentMode->VDisplay) {
	   xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		"Not enough video RAM for accelerator. At least "
		"%dKB needed, %ldKB available\n",
		((((pScrn->displayWidth * pScrn->bitsPerPixel/8)   /* +8 for make it sure */
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

#ifdef CTSCE
#ifdef CTSCE_DIRECT
	if(pSiS->DoColorExpand) {
	   SiSDoCMD
	   pSiS->ColorExpandBusy = TRUE;
	}
#endif
#endif

	pSiS->DoColorExpand = FALSE;
	pSiS->alphaBlitBusy = FALSE;

	SiSIdle
}

#ifdef SISDUALHEAD
static void
SiSRestoreAccelState(ScrnInfoPtr pScrn)
{
	SISPtr pSiS = SISPTR(pScrn);

	pSiS->ColorExpandBusy = FALSE;
	pSiS->alphaBlitBusy = FALSE;
	SiSIdle
}
#endif

static void SiSSetupForScreenToScreenCopy(ScrnInfoPtr pScrn,
                                int xdir, int ydir, int rop,
                                unsigned int planemask, int trans_color)
{
	SISPtr  pSiS = SISPTR(pScrn);

	PDEBUG(ErrorF("Setup ScreenCopy(%d, %d, 0x%x, 0x%x, 0x%x)\n",
			xdir, ydir, rop, planemask, trans_color));

#ifdef SISVRAMQ
        SiSSetupDSTColorDepth(pSiS->SiS310_AccelDepth);
	SiSCheckQueue(16 * 2);
	SiSSetupSRCPitchDSTRect(pSiS->scrnOffset, pSiS->scrnOffset, -1)
#else
	SiSSetupDSTColorDepth(pSiS->DstColor);
	SiSSetupSRCPitch(pSiS->scrnOffset)
	SiSSetupDSTRect(pSiS->scrnOffset, -1)
#endif

	if(trans_color != -1) {
	   SiSSetupROP(0x0A)
	   SiSSetupSRCTrans(trans_color)
	   SiSSetupCMDFlag(TRANSPARENT_BITBLT)
	} else {
	   SiSSetupROP(XAACopyROP[rop])
	   /* Set command - not needed, both 0 */
	   /* SiSSetupCMDFlag(BITBLT | SRCVIDEO) */
	}

#ifndef SISVRAMQ
	SiSSetupCMDFlag(pSiS->SiS310_AccelDepth)
#endif

#ifdef SISVRAMQ
        SiSSyncWP
#endif

	/* The chip is smart enough to know the direction */
}

static void SiSSubsequentScreenToScreenCopy(ScrnInfoPtr pScrn,
                                int src_x, int src_y, int dst_x, int dst_y,
                                int width, int height)
{
	SISPtr  pSiS = SISPTR(pScrn);
	long srcbase, dstbase;
	int mymin, mymax;

	PDEBUG(ErrorF("Subsequent ScreenCopy(%d,%d, %d,%d, %d,%d)\n",
			  src_x, src_y, dst_x, dst_y, width, height));

	srcbase = dstbase = 0;
	mymin = min(src_y, dst_y);
	mymax = max(src_y, dst_y);

	/* Libxaa.a has a bug: The tilecache cannot operate
	 * correctly if there are 512x512 slots, but no 256x256
	 * slots. This leads to catastrophic data fed to us.
	 * Filter this out here and warn the user.
	 * Fixed in 4.3.99.10 (?) and Debian's 4.3.0.1
	 */
#if (XF86_VERSION_CURRENT < XF86_VERSION_NUMERIC(4,3,99,10,0)) && (XF86_VERSION_CURRENT != XF86_VERSION_NUMERIC(4,3,0,1,0))
        if((src_x < 0)  ||
	   (dst_x < 0)  ||
	   (src_y < 0)  ||
	   (dst_y < 0)  ||
	   (width <= 0) ||
	   (height <= 0)) {
	   xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
	   	"BitBlit fatal error: Illegal coordinates:\n");
	   xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
	        "Source x %d y %d, dest x %d y %d, width %d height %d\n",
			  src_x, src_y, dst_x, dst_y, width, height);
	   xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
	   	"This is very probably caused by a known bug in libxaa.a.\n");
	   xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
	   	"Please update libxaa.a to avoid this error.\n");
	   return;
	}
#endif

	/* Although the chip knows the direction to use
	 * if the source and destination areas overlap,
	 * that logic fails if we fiddle with the bitmap
	 * addresses. Therefore, we check if the source
	 * and destination blitting areas overlap and
	 * adapt the bitmap addresses synchronously 
	 * if the coordinates exceed the valid range.
	 * The the areas do not overlap, we do our 
	 * normal check.
	 */
	if((mymax - mymin) < height) {
	   if((src_y >= 2048) || (dst_y >= 2048)) {	      
	      srcbase = pSiS->scrnOffset * mymin;
	      dstbase = pSiS->scrnOffset * mymin;
	      src_y -= mymin;
	      dst_y -= mymin;
	   }
	} else {
	   if(src_y >= 2048) {
	      srcbase = pSiS->scrnOffset * src_y;
	      src_y = 0;
	   }
	   if((dst_y >= pScrn->virtualY) || (dst_y >= 2048)) {
	      dstbase = pSiS->scrnOffset * dst_y;
	      dst_y = 0;
	   }
	}
#ifdef SISDUALHEAD
	srcbase += HEADOFFSET;
	dstbase += HEADOFFSET;
#endif

#ifdef SISVRAMQ
	SiSCheckQueue(16 * 3);
        SiSSetupSRCDSTBase(srcbase, dstbase)
	SiSSetupSRCDSTXY(src_x, src_y, dst_x, dst_y)
	SiSSetRectDoCMD(width,height)
#else
	SiSSetupSRCBase(srcbase);
	SiSSetupDSTBase(dstbase);
	SiSSetupRect(width, height)
	SiSSetupSRCXY(src_x, src_y)
	SiSSetupDSTXY(dst_x, dst_y)
	SiSDoCMD
#endif
}

static void
SiSSetupForSolidFill(ScrnInfoPtr pScrn, int color,
			int rop, unsigned int planemask)
{
	SISPtr  pSiS = SISPTR(pScrn);

	PDEBUG(ErrorF("Setup SolidFill(0x%x, 0x%x, 0x%x)\n",
					color, rop, planemask));

	if(pSiS->disablecolorkeycurrent) {
	   if((CARD32)color == pSiS->colorKey) {
	      rop = 5;  /* NOOP */
	   }
	}

#ifdef SISVRAMQ
	SiSSetupDSTColorDepth(pSiS->SiS310_AccelDepth);
	SiSCheckQueue(16 * 1);
	SiSSetupPATFGDSTRect(color, pSiS->scrnOffset, -1)
	SiSSetupROP(XAAPatternROP[rop])
	SiSSetupCMDFlag(PATFG)
        SiSSyncWP
#else
  	SiSSetupPATFG(color)
	SiSSetupDSTRect(pSiS->scrnOffset, -1)
	SiSSetupDSTColorDepth(pSiS->DstColor);
	SiSSetupROP(XAAPatternROP[rop])
	SiSSetupCMDFlag(PATFG | pSiS->SiS310_AccelDepth)
#endif
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
	if(y >= 2048) {
	   dstbase = pSiS->scrnOffset * y;
	   y = 0;
	}
#ifdef SISDUALHEAD
	dstbase += HEADOFFSET;
#endif

	pSiS->CommandReg &= ~(T_XISMAJORL | T_XISMAJORR |
	                      T_L_X_INC | T_L_Y_INC |
	                      T_R_X_INC | T_R_Y_INC |
			      TRAPAZOID_FILL);

	/* SiSSetupCMDFlag(BITBLT)  - BITBLT = 0 */

#ifdef SISVRAMQ
	SiSCheckQueue(16 * 2)
	SiSSetupDSTXYRect(x,y,w,h)
	SiSSetupDSTBaseDoCMD(dstbase)
#else
	SiSSetupDSTBase(dstbase)
	SiSSetupDSTXY(x,y)
	SiSSetupRect(w,h)
	SiSDoCMD
#endif
}

/* Trapezoid */
/* This would work better if XAA would provide us with valid trapezoids.
 * In fact, with small trapezoids the left and the right edge often cross
 * each other which causes drawing errors (filling over whole scanline).
 * DOES NOT WORK ON 330 SERIES, HANGS THE ENGINE.
 */
#ifdef TRAP
static void
SiSSubsequentSolidFillTrap(ScrnInfoPtr pScrn, int y, int h,
	       int left,  int dxL, int dyL, int eL,
	       int right, int dxR, int dyR, int eR )
{
	SISPtr  pSiS = SISPTR(pScrn);
	long dstbase;

	dstbase = 0;
	if(y >= 2048) {
	   dstbase = pSiS->scrnOffset * y;
	   y = 0;
	}
#ifdef SISDUALHEAD
	dstbase += HEADOFFSET;
#endif

#ifdef SISVRAMQ	/* Not optimized yet */
	SiSCheckQueue(16 * 10)
#else
	SiSSetupDSTBase(dstbase)
#endif

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

	/* Determine egde angles */
	if(dxL < 0) 	{ dxL = -dxL; }
	else 		{ SiSSetupCMDFlag(T_L_X_INC) }
	if(dxR < 0) 	{ dxR = -dxR; }
	else 		{ SiSSetupCMDFlag(T_R_X_INC) }

	/* (Y direction always positive - do this anyway) */
	if(dyL < 0) 	{ dyL = -dyL; }
	else 		{ SiSSetupCMDFlag(T_L_Y_INC) }
	if(dyR < 0) 	{ dyR = -dyR; }
	else 		{ SiSSetupCMDFlag(T_R_Y_INC) }

	/* Determine major axis */
	if(dxL >= dyL) {  SiSSetupCMDFlag(T_XISMAJORL) }
	if(dxR >= dyR) {  SiSSetupCMDFlag(T_XISMAJORR) }

	SiSSetupCMDFlag(TRAPAZOID_FILL);

#ifdef SISVRAMQ
	SiSSetupYHLR(y, h, left, right)
	SiSSetupdLdR(dxL, dyL, dxR, dyR)
	SiSSetupELER(eL, eR)
	SiSSetupDSTBaseDoCMD(dstbase)
#else
	/* Set up deltas */
	SiSSetupdL(dxL, dyL)
	SiSSetupdR(dxR, dyR)
	/* Set up y, h, left, right */
	SiSSetupYH(y, h)
	SiSSetupLR(left, right)
	/* Set up initial error term */
	SiSSetupEL(eL)
	SiSSetupER(eR)
	SiSDoCMD
#endif
}
#endif

static void
SiSSetupForSolidLine(ScrnInfoPtr pScrn, int color, int rop,
                                     unsigned int planemask)
{
	SISPtr  pSiS = SISPTR(pScrn);

	PDEBUG(ErrorF("Setup SolidLine(0x%x, 0x%x, 0x%x)\n",
					color, rop, planemask));

#ifdef SISVRAMQ
	SiSSetupDSTColorDepth(pSiS->SiS310_AccelDepth);
	SiSCheckQueue(16 * 3);
        SiSSetupLineCountPeriod(1, 1)
	SiSSetupPATFGDSTRect(color, pSiS->scrnOffset, -1)
	SiSSetupROP(XAAPatternROP[rop])
	SiSSetupCMDFlag(PATFG | LINE)
        SiSSyncWP
#else
	SiSSetupLineCount(1)
	SiSSetupPATFG(color)
	SiSSetupDSTRect(pSiS->scrnOffset, -1)
	SiSSetupDSTColorDepth(pSiS->DstColor)
	SiSSetupROP(XAAPatternROP[rop])
	SiSSetupCMDFlag(PATFG | LINE | pSiS->SiS310_AccelDepth)
#endif
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
	if(maxy >= 2048) {
	   dstbase = pSiS->scrnOffset*miny;
	   y1 -= miny;
	   y2 -= miny;
	}
#ifdef SISDUALHEAD
	dstbase += HEADOFFSET;
#endif

	if(flags & OMIT_LAST) {
	   SiSSetupCMDFlag(NO_LAST_PIXEL)
	} else {
	   pSiS->CommandReg &= ~(NO_LAST_PIXEL);
	}

#ifdef SISVRAMQ
	SiSCheckQueue(16 * 2);
        SiSSetupX0Y0X1Y1(x1,y1,x2,y2)
	SiSSetupDSTBaseDoCMD(dstbase)
#else
	SiSSetupDSTBase(dstbase)
	SiSSetupX0Y0(x1,y1)
	SiSSetupX1Y1(x2,y2)
	SiSDoCMD
#endif
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
	if((y >= 2048) || ((y + len) >= 2048)) {
	   dstbase = pSiS->scrnOffset * y;
	   y = 0;
	}
#ifdef SISDUALHEAD
	dstbase += HEADOFFSET;
#endif

#ifdef SISVRAMQ
	SiSCheckQueue(16 * 2);
    	if(dir == DEGREES_0) {
	   SiSSetupX0Y0X1Y1(x, y, (x + len), y)
	} else {
	   SiSSetupX0Y0X1Y1(x, y, x, (y + len))
	}
	SiSSetupDSTBaseDoCMD(dstbase)
#else
	SiSSetupDSTBase(dstbase)
	SiSSetupX0Y0(x,y)
	if(dir == DEGREES_0) {
	   SiSSetupX1Y1(x + len, y);
	} else {
	   SiSSetupX1Y1(x, y + len);
	}
	SiSDoCMD
#endif
}

static void
SiSSetupForDashedLine(ScrnInfoPtr pScrn,
                                int fg, int bg, int rop, unsigned int planemask,
                                int length, unsigned char *pattern)
{
	SISPtr pSiS = SISPTR(pScrn);

	PDEBUG(ErrorF("Setup DashedLine(0x%x, 0x%x, 0x%x, 0x%x, %d, 0x%x:%x)\n",
			fg, bg, rop, planemask, length, *(pattern+4), *pattern));

#ifdef SISVRAMQ
	SiSSetupDSTColorDepth(pSiS->SiS310_AccelDepth);
	SiSCheckQueue(16 * 3);
 	SiSSetupLineCountPeriod(1, length-1)
	SiSSetupStyle(*pattern,*(pattern+4))
	SiSSetupPATFGDSTRect(fg, pSiS->scrnOffset, -1)
#else
	SiSSetupLineCount(1)
	SiSSetupDSTRect(pSiS->scrnOffset, -1)
	SiSSetupDSTColorDepth(pSiS->DstColor);
	SiSSetupStyleLow(*pattern)
	SiSSetupStyleHigh(*(pattern+4))
	SiSSetupStylePeriod(length-1);
	SiSSetupPATFG(fg)
#endif

	SiSSetupROP(XAAPatternROP[rop])

	SiSSetupCMDFlag(LINE | LINE_STYLE)

	if(bg != -1) {
	   SiSSetupPATBG(bg)
	} else {
	   SiSSetupCMDFlag(TRANSPARENT)
	}
#ifndef SISVRAMQ
	SiSSetupCMDFlag(pSiS->SiS310_AccelDepth)
#endif

#ifdef SISVRAMQ
        SiSSyncWP
#endif
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
	miny = (y1 > y2) ? y2 : y1;
	maxy = (y1 > y2) ? y1 : y2;
	if(maxy >= 2048) {
	   dstbase = pSiS->scrnOffset * miny;
	   y1 -= miny;
	   y2 -= miny;
	}
#ifdef SISDUALHEAD
	dstbase += HEADOFFSET;
#endif

	if(flags & OMIT_LAST) {
	   SiSSetupCMDFlag(NO_LAST_PIXEL)
	} else {
	   pSiS->CommandReg &= ~(NO_LAST_PIXEL);
	}

#ifdef SISVRAMQ
	SiSCheckQueue(16 * 2);
	SiSSetupX0Y0X1Y1(x1,y1,x2,y2)
	SiSSetupDSTBaseDoCMD(dstbase)
#else
	SiSSetupDSTBase(dstbase)
	SiSSetupX0Y0(x1,y1)
	SiSSetupX1Y1(x2,y2)
	SiSDoCMD
#endif
}

static void
SiSSetupForMonoPatternFill(ScrnInfoPtr pScrn,
                                int patx, int paty, int fg, int bg,
                                int rop, unsigned int planemask)
{
	SISPtr pSiS = SISPTR(pScrn);

	PDEBUG(ErrorF("Setup MonoPatFill(0x%x,0x%x, 0x%x,0x%x, 0x%x, 0x%x)\n",
					patx, paty, fg, bg, rop, planemask));

#ifdef SISVRAMQ
	SiSSetupDSTColorDepth(pSiS->SiS310_AccelDepth);
	SiSCheckQueue(16 * 3);
	SiSSetupPATFGDSTRect(fg, pSiS->scrnOffset, -1)
#else
	SiSSetupDSTRect(pSiS->scrnOffset, -1)
	SiSSetupDSTColorDepth(pSiS->DstColor);
#endif

	SiSSetupMONOPAT(patx,paty)

	SiSSetupROP(XAAPatternROP[rop])

#ifdef SISVRAMQ
        SiSSetupCMDFlag(PATMONO)
#else
	SiSSetupPATFG(fg)
	SiSSetupCMDFlag(PATMONO | pSiS->SiS310_AccelDepth)
#endif

	if(bg != -1) {
	   SiSSetupPATBG(bg)
	} else {
	   SiSSetupCMDFlag(TRANSPARENT)
	}

#ifdef SISVRAMQ
        SiSSyncWP
#endif
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
	if(y >= 2048) {
	   dstbase = pSiS->scrnOffset * y;
	   y = 0;
	}
#ifdef SISDUALHEAD
	dstbase += HEADOFFSET;
#endif

	/* Clear commandReg because Setup can be used for Rect and Trap */
	pSiS->CommandReg &= ~(T_XISMAJORL | T_XISMAJORR |
	                      T_L_X_INC | T_L_Y_INC |
	                      T_R_X_INC | T_R_Y_INC |
	                      TRAPAZOID_FILL);

#ifdef SISVRAMQ
	SiSCheckQueue(16 * 2);
	SiSSetupDSTXYRect(x,y,w,h)
	SiSSetupDSTBaseDoCMD(dstbase)
#else
	SiSSetupDSTBase(dstbase)
	SiSSetupDSTXY(x,y)
	SiSSetupRect(w,h)
	SiSDoCMD
#endif
}

/* --- Trapezoid --- */

/* Does not work at all on 330 series */

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
	if(y >= 2048) {
	   dstbase=pSiS->scrnOffset*y;
	   y = 0;
	}
#ifdef SISDUALHEAD
	dstbase += HEADOFFSET;
#endif

#ifdef SISVRAMQ
	SiSCheckQueue(16 * 4);
#else
	SiSSetupDSTBase(dstbase)
#endif

	/* Clear CommandReg because SetUp can be used for Rect and Trap */
	pSiS->CommandReg &= ~(T_XISMAJORL | T_XISMAJORR |
	                      T_L_X_INC | T_L_Y_INC |
			      T_R_X_INC | T_R_Y_INC |
			      BITBLT);

	if(dxL < 0) 	{ dxL = -dxL;  }
	else 		{ SiSSetupCMDFlag(T_L_X_INC) }
	if(dxR < 0) 	{ dxR = -dxR; }
	else 		{ SiSSetupCMDFlag(T_R_X_INC) }

	if(dyL < 0) 	{ dyL = -dyL; }
	else 		{ SiSSetupCMDFlag(T_L_Y_INC) }
	if(dyR < 0) 	{ dyR = -dyR; }
	else 		{ SiSSetupCMDFlag(T_R_Y_INC) }

	/* Determine major axis */
	if(dxL >= dyL)  { SiSSetupCMDFlag(T_XISMAJORL) }
	if(dxR >= dyR)  { SiSSetupCMDFlag(T_XISMAJORR) }

	SiSSetupCMDFlag(TRAPAZOID_FILL);

#ifdef SISVRAMQ
	SiSSetupYHLR(y, h, left, right)
	SiSSetupdLdR(dxL, dyL, dxR, dyR)
	SiSSetupELER(eL, eR)
	SiSSetupDSTBaseDoCMD(dstbase)
#else
	SiSSetupYH(y, h)
	SiSSetupLR(left, right)
	SiSSetupdL(dxL, dyL)
	SiSSetupdR(dxR, dyR)
	SiSSetupEL(eL)
	SiSSetupER(eR)
	SiSDoCMD
#endif
}
#endif

/* Color 8x8 pattern */

#ifdef SISVRAMQ
static void
SiSSetupForColor8x8PatternFill(ScrnInfoPtr pScrn, int patternx, int patterny,
			int rop, unsigned int planemask, int trans_col)
{
	SISPtr pSiS = SISPTR(pScrn);
	int j = pScrn->bitsPerPixel >> 3;
	CARD32 *patadr = (CARD32 *)(pSiS->FbBase + (patterny * pSiS->scrnOffset) +
				(patternx * j));

#ifdef ACCELDEBUG
	xf86DrvMsg(0, X_INFO, "Setup Color8x8PatFill(0x%x, 0x%x, 0x%x, 0x%x)\n",
				patternx, patterny, rop, planemask);
#endif

	SiSSetupDSTColorDepth(pSiS->SiS310_AccelDepth);
	SiSCheckQueue(16 * 3);

	SiSSetupDSTRectBurstHeader(pSiS->scrnOffset, -1, PATTERN_REG, (pScrn->bitsPerPixel << 1))

	while(j--) {
	   SiSSetupPatternRegBurst(patadr[0],  patadr[1],  patadr[2],  patadr[3]);
	   SiSSetupPatternRegBurst(patadr[4],  patadr[5],  patadr[6],  patadr[7]);
	   SiSSetupPatternRegBurst(patadr[8],  patadr[9],  patadr[10], patadr[11]);
	   SiSSetupPatternRegBurst(patadr[12], patadr[13], patadr[14], patadr[15]);
	   patadr += 16;  /* = 64 due to (CARD32 *) */
	}

	SiSSetupROP(XAAPatternROP[rop])

	SiSSetupCMDFlag(PATPATREG)

        SiSSyncWP
}

static void
SiSSubsequentColor8x8PatternFillRect(ScrnInfoPtr pScrn, int patternx,
			int patterny, int x, int y, int w, int h)
{
	SISPtr  pSiS = SISPTR(pScrn);
	long dstbase;

#ifdef ACCELDEBUG
	xf86DrvMsg(0, X_INFO, "Subsequent Color8x8FillRect(%d, %d, %d, %d)\n",
					x, y, w, h);
#endif

	dstbase = 0;
	if(y >= 2048) {
	   dstbase = pSiS->scrnOffset * y;
	   y = 0;
	}
#ifdef SISDUALHEAD
	dstbase += HEADOFFSET;
#endif
	/* SiSSetupCMDFlag(BITBLT)  - BITBLT = 0 */

	SiSCheckQueue(16 * 2)
	SiSSetupDSTXYRect(x,y,w,h)
	SiSSetupDSTBaseDoCMD(dstbase)
}
#endif

/* ---- CPUToScreen Color Expand --- */

#ifdef CTSCE

#ifdef CTSCE_DIRECT

/* Direct method */

/* This is somewhat a fake. We let XAA copy its data not to an
 * aperture, but to video RAM, and then do a ScreenToScreen
 * color expansion.
 * Since the data is sent AFTER the call to Subsequent, we
 * don't execute the command here, but set a flag and do
 * that in the (subsequent) call to Sync()
 */

static void
SiSSetupForCPUToScreenColorExpandFill(ScrnInfoPtr pScrn,
		int fg, int bg, int rop, unsigned int planemask)
{
	SISPtr pSiS=SISPTR(pScrn);

#ifdef SISVRAMQ
        SiSSetupDSTColorDepth(pSiS->SiS310_AccelDepth);
	SiSSetupROP(XAACopyROP[rop]);
	SiSSetupSRCFGDSTRect(fg, pSiS->scrnOffset, -1)
	if(bg == -1) {
	   SiSSetupCMDFlag(TRANSPARENT | ENCOLOREXP | SRCVIDEO);
	} else {
	   SiSSetupSRCBG(bg);
	   SiSSetupCMDFlag(ENCOLOREXP | SRCVIDEO);
	}
        SiSSyncWP
#else
	SiSSetupSRCXY(0,0);
	SiSSetupROP(XAACopyROP[rop]);
	SiSSetupSRCFG(fg);
	SiSSetupDSTRect(pSiS->scrnOffset, -1);
	SiSSetupDSTColorDepth(pSiS->DstColor);
	if(bg == -1) {
	   SiSSetupCMDFlag(TRANSPARENT | ENCOLOREXP | SRCVIDEO
				       | pSiS->SiS310_AccelDepth);
	} else {
	   SiSSetupSRCBG(bg);
	   SiSSetupCMDFlag(ENCOLOREXP | SRCVIDEO | pSiS->SiS310_AccelDepth);
	}
#endif
}

static void
SiSSubsequentCPUToScreenColorExpandFill(
                        ScrnInfoPtr pScrn, int x, int y, int w,
                        int h, int skipleft)
{
	SISPtr pSiS = SISPTR(pScrn);
	int _x0, _y0, _x1, _y1;
	long srcbase, dstbase;

	srcbase = pSiS->ColorExpandBase;

	dstbase = 0;
	if(y >= 2048) {
	   dstbase = pSiS->scrnOffset*y;
	   y = 0;
	}

#ifdef SISDUALHEAD
	srcbase += HEADOFFSET;
	dstbase += HEADOFFSET;
#endif

#ifdef SISVRAMQ
	SiSSetupSRCDSTBase(srcbase,dstbase);
#else
	SiSSetupSRCBase(srcbase);
	SiSSetupDSTBase(dstbase)
#endif

	if(skipleft > 0) {
	   _x0 = x + skipleft;
	   _y0 = y;
	   _x1 = x + w;
	   _y1 = y + h;
#ifdef SISVRAMQ
           SiSSetupClip(_x0, _y0, _x1, _y1);
#else
	   SiSSetupClipLT(_x0, _y0);
	   SiSSetupClipRB(_x1, _y1);
#endif
	   SiSSetupCMDFlag(CLIPENABLE);
	} else {
	   pSiS->CommandReg &= (~CLIPENABLE);
	}

#ifdef SISVRAMQ
	SiSSetupRectSRCPitch(w, h, ((((w + 7) >> 3) + 3) >> 2) << 2);
	SiSSetupSRCDSTXY(0, 0, x, y);
#else
	SiSSetupRect(w, h);
	SiSSetupSRCPitch(((((w+7)/8)+3) >> 2) * 4);
	SiSSetupDSTXY(x, y);
#endif

	if(pSiS->ColorExpandBusy) {
	   pSiS->ColorExpandBusy = FALSE;
	   SiSIdle
	}

	pSiS->DoColorExpand = TRUE;
}

#else

/* Indirect method */

/* This is SLOW, slower than the CPU on most chipsets */
/* Does not work in VRAM queue mode. */

static void
SiSSetupForScanlineCPUToScreenColorExpandFill(ScrnInfoPtr pScrn,
		int fg, int bg, int rop, unsigned int planemask)
{
	SISPtr pSiS=SISPTR(pScrn);

#ifdef SISVRAMQ
        SiSSetupDSTColorDepth(pSiS->SiS310_AccelDepth);
#endif

	/* !!! DOES NOT WORK IN VRAM QUEUE MODE !!! */

	/* (hence this is not optimized for VRAM mode) */
#ifndef SISVRAMQ
	SiSIdle
#endif
	SiSSetupSRCXY(0,0);

	SiSSetupROP(XAACopyROP[rop]);
	SiSSetupSRCFG(fg);
	SiSSetupDSTRect(pSiS->scrnOffset, -1);
#ifndef SISVRAMQ
	SiSSetupDSTColorDepth(pSiS->DstColor);
#endif
	if(bg == -1) {
#ifdef SISVRAMQ
	   SiSSetupCMDFlag(TRANSPARENT | ENCOLOREXP | SRCVIDEO);
#else
	   SiSSetupCMDFlag(TRANSPARENT | ENCOLOREXP | SRCCPUBLITBUF
				       | pSiS->SiS310_AccelDepth);
#endif
	} else {
	   SiSSetupSRCBG(bg);
#ifdef SISVRAMQ
	   SiSSetupCMDFlag(ENCOLOREXP | SRCCPUBLITBUF);
#else
	   SiSSetupCMDFlag(ENCOLOREXP | SRCCPUBLITBUF | pSiS->SiS310_AccelDepth);
#endif
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
	if(y >= 2048) {
	   dstbase = pSiS->scrnOffset*y;
	   y = 0;
	}
#ifdef SISDUALHEAD
	dstbase += HEADOFFSET;
#endif

#ifndef SISVRAMQ
        if((MMIO_IN16(pSiS->IOBase, Q_STATUS+2) & 0x8000) != 0x8000) {
	   SiSIdle;
        }
#endif

	SiSSetupDSTBase(dstbase)

	if(skipleft > 0) {
	   _x0 = x+skipleft;
	   _y0 = y;
	   _x1 = x+w;
	   _y1 = y+h;
#ifdef SISVRAMQ
           SiSSetupClip(_x0, _y0, _x1, _y1);
#else
	   SiSSetupClipLT(_x0, _y0);
	   SiSSetupClipRB(_x1, _y1);
#endif
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
	SISPtr pSiS = SISPTR(pScrn);
	long cbo;

	cbo = pSiS->ColorExpandBufferScreenOffset[bufno];
#ifdef SISDUALHEAD
	cbo += HEADOFFSET;
#endif

#ifndef SISVRAMQ
	if((MMIO_IN16(pSiS->IOBase, Q_STATUS+2) & 0x8000) != 0x8000) {
	   SiSIdle;
        }
#endif

	SiSSetupSRCBase(cbo);

	SiSSetupDSTXY(pSiS->xcurrent, pSiS->ycurrent);

	SiSDoCMD

	pSiS->ycurrent++;
#ifndef SISVRAMQ
	SiSIdle
#endif
}
#endif
#endif

/* --- Screen To Screen Color Expand --- */

/* This method blits in a single task; this does not work because
 * the hardware does not use the source pitch as scanline offset
 * but to calculate pattern address from source X and Y and to
 * limit the drawing width (similar to width set by SetupRect).
 * XAA provides the pattern bitmap with scrnOffset (displayWidth * bpp/8)
 * offset, but this is not supported by the hardware.
 * DOES NOT WORK ON 330 SERIES, HANGS ENGINE.
 */

#ifdef STSCE
static void
SiSSetupForScreenToScreenColorExpand(ScrnInfoPtr pScrn,
                                int fg, int bg,
                                int rop, unsigned int planemask)
{
	SISPtr          pSiS = SISPTR(pScrn);

#ifdef SISVRAMQ
        SiSSetupDSTColorDepth(pSiS->SiS310_AccelDepth);
#else
	SiSSetupDSTColorDepth(pSiS->DstColor)
#endif
	SiSSetupDSTRect(pSiS->scrnOffset, -1)
	SiSSetupROP(XAACopyROP[rop])
	SiSSetupSRCFG(fg)
	/* SiSSetupSRCXY(0,0) */

	if(bg == -1) {
	   SiSSetupCMDFlag(TRANSPARENT | ENCOLOREXP | SRCVIDEO);
	} else {
	   SiSSetupSRCBG(bg);
	   SiSSetupCMDFlag(ENCOLOREXP | SRCVIDEO);
	};

#ifdef SISVRAMQ
        SiSSyncWP
#endif
}

/* For testing, these are the methods: (use only one at a time!) */

#undef npitch 		/* Normal: Use srcx/y as srcx/y, use scrnOffset as source pitch
			 * Does not work on 315 series, because the hardware does not
			 * regard the src x and y. Apart from this problem:
			 * This would work if the hareware used the source pitch for
			 * incrementing the source address after each scanline - but
			 * it doesn't do this! The first line of the area is correctly
			 * color expanded, but since the source pitch is ignored and
			 * the source address not incremented correctly, the following
			 * lines are color expanded with any bit pattern that is left
			 * in the unused space of the source bitmap (which is organized
			 * with the depth of the screen framebuffer hence with a pitch
			 * of scrnOffset).
			 */

#undef pitchdw    	/* Use source pitch "displayWidth / 8" instead
		   	 * of scrnOffset (=displayWidth * bpp / 8)
			 * This can't work, because the pitch of the source
			 * bitmap is scrnoffset!
		   	 */

#define nopitch   	/* Calculate srcbase with srcx and srcy, set the
		   	 * pitch to scrnOffset (which IS the correct pitch
		   	 * for the source bitmap) and set srcx and srcy both
		   	 * to 0.
			 * This would work if the hareware used the source pitch for
			 * incrementing the source address after each scanline - but
			 * it doesn't do this! Again: The first line of the area is
			 * correctly color expanded, but since the source pitch is
			 * ignored for scanline address incremention, the following
			 * lines are not correctly color expanded.
			 * This is the only way it works (apart from the problem
			 * described above). The hardware does not regard the src
			 * x and y values in any way.
		   	 */

static void
SiSSubsequentScreenToScreenColorExpand(ScrnInfoPtr pScrn,
                                int x, int y, int w, int h,
                                int srcx, int srcy, int skipleft)
{
	SISPtr pSiS = SISPTR(pScrn);
        long srcbase, dstbase;
#if 0
	int _x0, _y0, _x1, _y1;
#endif
#ifdef pitchdw
	int newsrcx, newsrcy;

	/* srcx and srcy are provided based on a scrnOffset pitch ( = displayWidth * bpp / 8 )
	 * We recalulate srcx and srcy based on pitch = displayWidth / 8
	 */
        newsrcy = ((pSiS->scrnOffset * srcy) + (srcx * ((pScrn->bitsPerPixel+7)/8))) /
					  (pScrn->displayWidth/8);
        newsrcx = ((pSiS->scrnOffset * srcy) + (srcx * ((pScrn->bitsPerPixel+7)/8))) %
					  (pScrn->displayWidth/8);
#endif
	xf86DrvMsg(0, X_INFO, "Sub ScreenToScreen ColorExp(%d,%d, %d,%d, %d,%d, %d)\n",
					x, y, w, h, srcx, srcy, skipleft);

	srcbase = dstbase = 0;

#ifdef pitchdw
	if(newsrcy >= 2048) {
	   srcbase = (pScrn->displayWidth / 8) * newsrcy;
	   newsrcy = 0;
	}
#endif
#ifdef nopitch
	srcbase = (pSiS->scrnOffset * srcy) + (srcx * ((pScrn->bitsPerPixel+7)/8));
#endif
#ifdef npitch
	if(srcy >= 2048) {
	   srcbase = pSiS->scrnOffset * srcy;
	   srcy = 0;
	}
#endif
	if(y >= 2048) {
	   dstbase = pSiS->scrnOffset * y;
	   y = 0;
	}

#ifdef SISDUALHEAD
	srcbase += HEADOFFSET;
	dstbase += HEADOFFSET;
#endif

	SiSSetupSRCBase(srcbase)
	SiSSetupDSTBase(dstbase)

	/* 315 series seem to treat the src pitch as
	 * a "drawing limit", but still (as 300 series)
	 * does not use it for incrementing the
	 * address pointer for the next scanline. ARGH!
	 */

#ifdef pitchdw
	SiSSetupSRCPitch(pScrn->displayWidth/8)
#endif
#ifdef nopitch
	SiSSetupSRCPitch(pScrn->displayWidth/8)
	/* SiSSetupSRCPitch(1024/8) */ /* For test */
#endif
#ifdef npitch
	SiSSetupSRCPitch(pScrn->displayWidth/8)
	/* SiSSetupSRCPitch(pSiS->scrnOffset) */
#endif

	SiSSetupRect(w,h)

#if 0   /* How do I implement the offset? Not this way, that's for sure.. */
	if (skipleft > 0) {
		_x0 = x+skipleft;
		_y0 = y;
		_x1 = x+w;
		_y1 = y+h;
		SiSSetupClipLT(_x0, _y0);
		SiSSetupClipRB(_x1, _y1);
		SiSSetupCMDFlag(CLIPENABLE);
	}
#endif
#ifdef pitchdw
	SiSSetupSRCXY(newsrcx, newsrcy)
#endif
#ifdef nopitch
	SiSSetupSRCXY(0,0)
#endif
#ifdef npitch
	SiSSetupSRCXY(srcx, srcy)
#endif

	SiSSetupDSTXY(x,y)

	SiSDoCMD
#ifdef SISVRAMQ
	/* We MUST sync here, there must not be 2 or more color expansion commands in the queue */
	SiSIdle
#endif	
}
#endif

/* ---- RENDER ---- */

#ifdef INCL_RENDER
#ifdef RENDER
static void
SiSRenderCallback(ScrnInfoPtr pScrn)
{
    	SISPtr pSiS = SISPTR(pScrn);

    	if((currentTime.milliseconds > pSiS->RenderTime) && pSiS->AccelLinearScratch) {
	   xf86FreeOffscreenLinear(pSiS->AccelLinearScratch);
	   pSiS->AccelLinearScratch = NULL;
    	}

    	if(!pSiS->AccelLinearScratch) {
	   pSiS->RenderCallback = NULL;
	}
}

#define RENDER_DELAY 15000

static Bool
SiSAllocateLinear(ScrnInfoPtr pScrn, int sizeNeeded)
{
   	SISPtr pSiS = SISPTR(pScrn);

	pSiS->RenderTime = currentTime.milliseconds + RENDER_DELAY;
        pSiS->RenderCallback = SiSRenderCallback;

   	if(pSiS->AccelLinearScratch) {
	   if(pSiS->AccelLinearScratch->size >= sizeNeeded) {
	      return TRUE;
	   } else {
	      if(pSiS->alphaBlitBusy) {
	         pSiS->alphaBlitBusy = FALSE;
	         SiSIdle
	      }
	      if(xf86ResizeOffscreenLinear(pSiS->AccelLinearScratch, sizeNeeded)) {
		 return TRUE;
	      }
	      xf86FreeOffscreenLinear(pSiS->AccelLinearScratch);
	      pSiS->AccelLinearScratch = NULL;
	   }
   	}

   	pSiS->AccelLinearScratch = xf86AllocateOffscreenLinear(
				 	pScrn->pScreen, sizeNeeded, 32,
				 	NULL, NULL, NULL);

	return(pSiS->AccelLinearScratch != NULL);
}

Bool
SiSSetupForCPUToScreenAlphaTexture(ScrnInfoPtr pScrn,
   			int op, CARD16 red, CARD16 green,
   			CARD16 blue, CARD16 alpha,
   			int alphaType, CARD8 *alphaPtr,
   			int alphaPitch, int width,
   			int height, int	flags)
{
    	SISPtr pSiS = SISPTR(pScrn);
    	int x, pitch, sizeNeeded, offset;
	CARD8  myalpha;
	CARD32 *dstPtr;
	unsigned char *renderaccelarray;

#ifdef ACCELDEBUG
	xf86DrvMsg(0, X_INFO, "AT: op %d type %d ARGB %x %x %x %x, w %d h %d A-pitch %d\n",
		op, alphaType, alpha, red, green, blue, width, height, alphaPitch);
#endif

    	if(op != PictOpOver) return FALSE;

    	if((width > 2048) || (height > 2048)) return FALSE;

    	pitch = (width + 31) & ~31;
    	sizeNeeded = pitch * height;
    	if(pScrn->bitsPerPixel == 16) sizeNeeded <<= 1;

	if(!((renderaccelarray = pSiS->RenderAccelArray)))
	   return FALSE;

	if(!SiSAllocateLinear(pScrn, sizeNeeded))
	   return FALSE;

	red &= 0xff00;
	green &= 0xff00;
	blue &= 0xff00;

#ifdef SISVRAMQ
        SiSSetupDSTColorDepth(pSiS->SiS310_AccelDepth);
	SiSSetupSRCPitchDSTRect((pitch << 2), pSiS->scrnOffset, -1);
	SiSSetupCMDFlag(ALPHA_BLEND | SRCVIDEO | A_PERPIXELALPHA)
        SiSSyncWP
#else
	SiSSetupDSTColorDepth(pSiS->DstColor);
	SiSSetupSRCPitch((pitch << 2));
	SiSSetupDSTRect(pSiS->scrnOffset, -1)
	SiSSetupROP(0)
	SiSSetupCMDFlag(ALPHA_BLEND | SRCVIDEO | A_PERPIXELALPHA | pSiS->SiS310_AccelDepth)
#endif

    	offset = pSiS->AccelLinearScratch->offset << 1;
    	if(pScrn->bitsPerPixel == 32) offset <<= 1;

	dstPtr = (CARD32*)(pSiS->FbBase + offset);

	if(pSiS->alphaBlitBusy) {
	   pSiS->alphaBlitBusy = FALSE;
	   SiSIdle
	}


	if(alpha == 0xffff) {

           while(height--) {
	      for(x = 0; x < width; x++) {
	         myalpha = alphaPtr[x];
	         dstPtr[x] = (renderaccelarray[red + myalpha] << 16)  |
	     	             (renderaccelarray[green + myalpha] << 8) |
			     renderaccelarray[blue + myalpha]         |
			     myalpha << 24;
	      }
	      dstPtr += pitch;
	      alphaPtr += alphaPitch;
           }

	} else {

	   alpha &= 0xff00;

	   while(height--) {
	      for(x = 0; x < width; x++) {
	         myalpha = alphaPtr[x];
	         dstPtr[x] = (renderaccelarray[alpha + myalpha] << 24) |
		    	     (renderaccelarray[red + myalpha] << 16)   |
	   	    	     (renderaccelarray[green + myalpha] << 8)  |
			     renderaccelarray[blue + myalpha];
	      }
	      dstPtr += pitch;
	      alphaPtr += alphaPitch;
           }

	}

    	return TRUE;
}

Bool
SiSSetupForCPUToScreenTexture(ScrnInfoPtr pScrn,
   			int op, int texType, CARD8 *texPtr,
   			int texPitch, int width,
   			int height, int	flags)
{
    	SISPtr pSiS = SISPTR(pScrn);
    	int pitch, sizeNeeded, offset;
	CARD8 *dst;

#ifdef ACCELDEBUG
	xf86DrvMsg(0, X_INFO, "T: type %d op %d w %d h %d T-pitch %d\n",
		texType, op, width, height, texPitch);
#endif

    	if(op != PictOpOver) return FALSE;

    	if((width > 2048) || (height > 2048)) return FALSE;

    	pitch = (width + 31) & ~31;
    	sizeNeeded = pitch * height;
    	if(pScrn->bitsPerPixel == 16) sizeNeeded <<= 1;

	width <<= 2;
	pitch <<= 2;

	if(!SiSAllocateLinear(pScrn, sizeNeeded))
	   return FALSE;

#ifdef SISVRAMQ
        SiSSetupDSTColorDepth(pSiS->SiS310_AccelDepth);
	SiSSetupSRCPitchDSTRect(pitch, pSiS->scrnOffset, -1);
	SiSSetupAlpha(0x00)
	SiSSetupCMDFlag(ALPHA_BLEND | SRCVIDEO | A_PERPIXELALPHA)
        SiSSyncWP
#else
	SiSSetupDSTColorDepth(pSiS->DstColor);
	SiSSetupSRCPitch(pitch);
	SiSSetupDSTRect(pSiS->scrnOffset, -1)
	SiSSetupAlpha(0x00)
	SiSSetupCMDFlag(ALPHA_BLEND | SRCVIDEO | A_PERPIXELALPHA | pSiS->SiS310_AccelDepth)
#endif

    	offset = pSiS->AccelLinearScratch->offset << 1;
    	if(pScrn->bitsPerPixel == 32) offset <<= 1;

	dst = (CARD8*)(pSiS->FbBase + offset);

	if(pSiS->alphaBlitBusy) {
	   pSiS->alphaBlitBusy = FALSE;
	   SiSIdle
	}

	while(height--) {
	   memcpy(dst, texPtr, width);
	   texPtr += texPitch;
	   dst += pitch;
        }

	return TRUE;
}

void
SiSSubsequentCPUToScreenTexture(ScrnInfoPtr pScrn,
    			int dst_x, int dst_y,
    			int src_x, int src_y,
    			int width, int height)
{
    	SISPtr pSiS = SISPTR(pScrn);
	long srcbase, dstbase;

	srcbase = pSiS->AccelLinearScratch->offset << 1;
	if(pScrn->bitsPerPixel == 32) srcbase <<= 1;

#ifdef ACCELDEBUG
	xf86DrvMsg(0, X_INFO, "FIRE: scrbase %x dx %d dy %d w %d h %d\n",
		srcbase, dst_x, dst_y, width, height);
#endif

	dstbase = 0;
	if((dst_y >= pScrn->virtualY) || (dst_y >= 2048)) {
	   dstbase = pSiS->scrnOffset * dst_y;
	   dst_y = 0;
	}
#ifdef SISDUALHEAD
	srcbase += HEADOFFSET;
	dstbase += HEADOFFSET;
#endif

#ifdef SISVRAMQ
	SiSCheckQueue(16 * 3)
	SiSSetupSRCDSTBase(srcbase,dstbase);
	SiSSetupSRCDSTXY(src_x, src_y, dst_x, dst_y)
	SiSSetRectDoCMD(width,height)
#else
	SiSSetupSRCBase(srcbase);
	SiSSetupDSTBase(dstbase);
	SiSSetupRect(width, height)
	SiSSetupSRCXY(src_x, src_y)
	SiSSetupDSTXY(dst_x, dst_y)
	SiSDoCMD
#endif
	pSiS->alphaBlitBusy = TRUE;
}
#endif
#endif


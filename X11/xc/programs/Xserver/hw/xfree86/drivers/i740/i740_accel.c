
/**************************************************************************

Copyright 1998-1999 Precision Insight, Inc., Cedar Park, Texas.
All Rights Reserved.

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sub license, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice (including the
next paragraph) shall be included in all copies or substantial portions
of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
IN NO EVENT SHALL PRECISION INSIGHT AND/OR ITS SUPPLIERS BE LIABLE FOR
ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

**************************************************************************/
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/i740/i740_accel.c,v 1.7 2002/10/21 13:32:58 alanh Exp $ */

/*
 * Authors:
 *   Daryll Strauss <daryll@precisioninsight.com>
 *
 */

#include "xf86.h"
#include "xf86_OSproc.h"
#include "xf86_ansic.h"

#include "compiler.h"

#include "xf86Pci.h"
#include "xf86PciInfo.h"

#include "xaa.h"
#include "vgaHW.h"

#include "xf86xv.h"
#include "i740.h"

static unsigned int i740Rop[16] = {
    0x00, /* GXclear      */
    0x88, /* GXand        */
    0x44, /* GXandReverse */
    0xCC, /* GXcopy       */
    0x22, /* GXandInvert  */
    0xAA, /* GXnoop       */
    0x66, /* GXxor        */
    0xEE, /* GXor         */
    0x11, /* GXnor        */
    0x99, /* GXequiv      */
    0x55, /* GXinvert     */
    0xDD, /* GXorReverse  */
    0x33, /* GXcopyInvert */
    0xBB, /* GXorInverted */
    0x77, /* GXnand       */
    0xFF  /* GXset        */
};

static unsigned int i740PatternRop[16] = {
    0x00, /* GXclear      */
    0xA0, /* GXand        */
    0x50, /* GXandReverse */
    0xF0, /* GXcopy       */
    0x0A, /* GXandInvert  */
    0xAA, /* GXnoop       */
    0x5A, /* GXxor        */
    0xFA, /* GXor         */
    0x05, /* GXnor        */
    0xA5, /* GXequiv      */
    0x55, /* GXinvert     */
    0xF5, /* GXorReverse  */
    0x0F, /* GXcopyInvert */
    0xAF, /* GXorInverted */
    0x5F, /* GXnand       */
    0xFF  /* GXset        */
};

static void I740SyncPIO(ScrnInfoPtr pScrn);
static void I740SyncMMIO(ScrnInfoPtr pScrn);
static void I740SetupForSolidFill(ScrnInfoPtr pScrn, int color, int rop,
				      unsigned int planemask);
static void I740SubsequentSolidFillRect(ScrnInfoPtr pScrn, int x, int y, 
					int w, int h);
static void I740SetupForScreenToScreenCopy(ScrnInfoPtr pScrn, int xdir, 
					   int ydir, int rop,
					   unsigned int planemask, 
					   int transparency_color);
static void I740SubsequentScreenToScreenCopy(ScrnInfoPtr pScrn, int x1, int y1,
					     int x2, int y2, int w, int h);
static void I740SetupForMono8x8PatternFill(ScrnInfoPtr pScrn, 
					   int pattx, int patty,
					   int fg, int bg, int rop,
					   unsigned int planemask);
static void I740SubsequentMono8x8PatternFillRect(ScrnInfoPtr pScrn, 
						  int pattx, int patty,
						  int x, int y, int w, int h);
#if 0
static void I740SetupForCPUToScreenColorExpandFill(ScrnInfoPtr pScrn, 
						   int bg, int fg, int rop,
						   unsigned int planemask);
static void I740SubsequentCPUToScreenColorExpandFill(ScrnInfoPtr pScrn, 
						     int x, int y, int w, int h,
						     int skipleft);
#endif
/*
 * The following function sets up the supported acceleration. Call it
 * from the FbInit() function in the SVGA driver, or before ScreenInit
 * in a monolithic server.
 */
Bool
I740AccelInit(ScreenPtr pScreen) {
  XAAInfoRecPtr infoPtr;
  ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
  I740Ptr pI740 = I740PTR(pScrn);

  pI740->AccelInfoRec = infoPtr = XAACreateInfoRec();
  if (!infoPtr) return FALSE;

  if (pScrn->bitsPerPixel == 32) {
    infoPtr->Flags = 0; /* Disables all acceleration */
    return TRUE;;
  }

  infoPtr->Flags = PIXMAP_CACHE | OFFSCREEN_PIXMAPS | LINEAR_FRAMEBUFFER;

  /* Sync */
  if (pI740->usePIO)
    infoPtr->Sync = I740SyncPIO;
  else
    infoPtr->Sync = I740SyncMMIO;

  infoPtr->CachePixelGranularity=8/pI740->cpp;

  /* Solid filled rectangles */
  infoPtr->SolidFillFlags = NO_PLANEMASK;
  infoPtr->SetupForSolidFill = I740SetupForSolidFill;
  infoPtr->SubsequentSolidFillRect = I740SubsequentSolidFillRect;

  /* Screen to screen copy */
  infoPtr->ScreenToScreenCopyFlags = (NO_PLANEMASK | NO_TRANSPARENCY);
  infoPtr->SetupForScreenToScreenCopy = I740SetupForScreenToScreenCopy;
  infoPtr->SubsequentScreenToScreenCopy = I740SubsequentScreenToScreenCopy;

  /* 8x8 pattern fills */
  infoPtr->SetupForMono8x8PatternFill = I740SetupForMono8x8PatternFill;
  infoPtr->SubsequentMono8x8PatternFillRect = I740SubsequentMono8x8PatternFillRect;
  infoPtr->Mono8x8PatternFillFlags = NO_PLANEMASK | HARDWARE_PATTERN_SCREEN_ORIGIN |
    BIT_ORDER_IN_BYTE_MSBFIRST;

  /* CPU to screen color expansion */
  /* Currently XAA is limited to only DWORD padding.  The 3.3 driver
   * uses NO_PAD scanlines b/c of problems with using the chip in
   * DWORD mode. Once other padding modes are available in XAA this
   * Code can be turned back on. 
   */
#if 0
#ifndef ALLOW_PCI_COLOR_EXP
  if (pI740->Chipset != PCI_CHIP_I740_PCI) {
#endif
    /*
     * Currently, we are not properly able to read the bitblt engine
     * busy bit on the PCI i740 card.  When we are able to do so, we
     * can re-enable color expansion.
     */
    infoPtr->CPUToScreenColorExpandFillFlags = 
       					  NO_PLANEMASK |
#ifdef USE_DWORD_COLOR_EXP
					  SCANLINE_PAD_DWORD |
#endif
					  CPU_TRANSFER_PAD_QWORD |
					  SYNC_AFTER_COLOR_EXPAND |
					  BIT_ORDER_IN_BYTE_MSBFIRST;
    infoPtr->ColorExpandBase = (unsigned char *)(pI740->MMIOBase + BLTDATA);
    infoPtr->ColorExpandRange = 0x10000;
    infoPtr->SetupForCPUToScreenColorExpandFill = I740SetupForCPUToScreenColorExpandFill;
    infoPtr->SubsequentCPUToScreenColorExpandFill = I740SubsequentCPUToScreenColorExpandFill;
#ifndef ALLOW_PCI_COLOR_EXP
    }
#endif
#endif
  return XAAInit(pScreen, infoPtr);
}

static void
I740SyncPIO(ScrnInfoPtr pScrn) {
  WAIT_ENGINE_IDLE_PIO();
}

static void
I740SyncMMIO(ScrnInfoPtr pScrn) {
  I740Ptr pI740;

  pI740 = I740PTR(pScrn);
  WAIT_ENGINE_IDLE_MMIO();
}

static void
I740SetupForSolidFill(ScrnInfoPtr pScrn, int color, int rop, 
		      unsigned int planemask) {
  I740Ptr pI740;

  pI740 = I740PTR(pScrn);

  pI740->bltcmd.BR00 = ((pScrn->displayWidth * pI740->cpp) << 16) |
    (pScrn->displayWidth * pI740->cpp);
  pI740->bltcmd.BR01 = color;
  pI740->bltcmd.BR04 = SOLID_PAT_SELECT | PAT_IS_MONO | i740PatternRop[rop];
}

static void
I740SubsequentSolidFillRect(ScrnInfoPtr pScrn, int x, int y, int w, int h) {
  I740Ptr pI740;

  pI740 = I740PTR(pScrn);
  WAIT_LP_FIFO(12);
  OUTREG(LP_FIFO, 0x6000000A);
  OUTREG(LP_FIFO, pI740->bltcmd.BR00);
  OUTREG(LP_FIFO, pI740->bltcmd.BR01);
  OUTREG(LP_FIFO, 0x00000000);
  OUTREG(LP_FIFO, 0x00000000);
  OUTREG(LP_FIFO, pI740->bltcmd.BR04);
  OUTREG(LP_FIFO, 0x00000000);
  OUTREG(LP_FIFO, 0x00000000);
  OUTREG(LP_FIFO, (y * pScrn->displayWidth + x) * pI740->cpp);
  OUTREG(LP_FIFO, 0x00000000);
  OUTREG(LP_FIFO, 0x00000000);
  OUTREG(LP_FIFO, (h << 16) | (w * pI740->cpp));
}

static void
I740SetupForScreenToScreenCopy(ScrnInfoPtr pScrn, int xdir, int ydir, int rop,
			       unsigned int planemask, int transparency_color)
{
  I740Ptr pI740;

  pI740 = I740PTR(pScrn);
  pI740->bltcmd.BR00 = (((pScrn->displayWidth * pI740->cpp) << 16) |
			(pScrn->displayWidth * pI740->cpp));

  pI740->bltcmd.BR04 = SRC_IS_IN_COLOR | SRC_USE_SRC_ADDR | i740Rop[rop];
  if (xdir == -1)
    pI740->bltcmd.BR04 |= BLT_RIGHT_TO_LEFT;
  else
    pI740->bltcmd.BR04 |= BLT_LEFT_TO_RIGHT;

  if (ydir == -1)
    pI740->bltcmd.BR04 |= BLT_BOT_TO_TOP;
  else
    pI740->bltcmd.BR04 |= BLT_TOP_TO_BOT;

    pI740->bltcmd.BR01 = 0x00000000;

}

static void
I740SubsequentScreenToScreenCopy(ScrnInfoPtr pScrn, int x1, int y1, 
				 int x2, int y2, int w, int h) {
  I740Ptr pI740;

  pI740 = I740PTR(pScrn);
  if (pI740->bltcmd.BR04 & BLT_BOT_TO_TOP) {
    pI740->bltcmd.BR06 = (y1 + h - 1) *
      pScrn->displayWidth * pI740->cpp;
    pI740->bltcmd.BR07 = (y2 + h - 1) *
      pScrn->displayWidth * pI740->cpp;
    } else {
      pI740->bltcmd.BR06 = y1 * pScrn->displayWidth * pI740->cpp;
      pI740->bltcmd.BR07 = y2 * pScrn->displayWidth * pI740->cpp;
    }

  if (pI740->bltcmd.BR04 & BLT_RIGHT_TO_LEFT) {
    pI740->bltcmd.BR06 += (x1 + w - 1) * pI740->cpp + pI740->cpp - 1;
    pI740->bltcmd.BR07 += (x2 + w - 1) * pI740->cpp + pI740->cpp - 1;
  } else {
    pI740->bltcmd.BR06 += x1 * pI740->cpp;
    pI740->bltcmd.BR07 += x2 * pI740->cpp;
  }

  WAIT_LP_FIFO(12);
  OUTREG(LP_FIFO, 0x6000000A);
  OUTREG(LP_FIFO, pI740->bltcmd.BR00);
  OUTREG(LP_FIFO, pI740->bltcmd.BR01);
  OUTREG(LP_FIFO, 0x00000000);
  OUTREG(LP_FIFO, 0x00000000);
  OUTREG(LP_FIFO, pI740->bltcmd.BR04);
  OUTREG(LP_FIFO, 0x00000000);
  OUTREG(LP_FIFO, pI740->bltcmd.BR06);
  OUTREG(LP_FIFO, pI740->bltcmd.BR07);
  OUTREG(LP_FIFO, 0x00000000);
  OUTREG(LP_FIFO, 0x00000000);
  OUTREG(LP_FIFO, (h << 16) | (w * pI740->cpp));
}

static void
I740SetupForMono8x8PatternFill(ScrnInfoPtr pScrn, int pattx, int patty, 
				int fg, int bg, int rop, 
				unsigned int planemask) {
  I740Ptr pI740;

  pI740 = I740PTR(pScrn);
  pI740->bltcmd.BR00 = ((pScrn->displayWidth * pI740->cpp) << 16) |
			(pScrn->displayWidth * pI740->cpp);

  pI740->bltcmd.BR01 = bg;
  pI740->bltcmd.BR02 = fg;

  pI740->bltcmd.BR04 = PAT_IS_MONO | i740PatternRop[rop];
  if (bg == -1) pI740->bltcmd.BR04 |= MONO_PAT_TRANSP;

  pI740->bltcmd.BR05 = (pattx + patty * pScrn->displayWidth) * pI740->cpp;
}

static void
I740SubsequentMono8x8PatternFillRect(ScrnInfoPtr pScrn, int pattx, int patty, 
				     int x, int y, int w, int h) {
  I740Ptr pI740;

  pI740 = I740PTR(pScrn);
  WAIT_LP_FIFO(12);
  OUTREG(LP_FIFO, 0x6000000A);
  OUTREG(LP_FIFO, pI740->bltcmd.BR00);
  OUTREG(LP_FIFO, pI740->bltcmd.BR01);
  OUTREG(LP_FIFO, pI740->bltcmd.BR02);
  OUTREG(LP_FIFO, 0x00000000);
  OUTREG(LP_FIFO, pI740->bltcmd.BR04 | ((y<<20) & PAT_VERT_ALIGN));
  OUTREG(LP_FIFO, pI740->bltcmd.BR05);
  OUTREG(LP_FIFO, 0x00000000);
  OUTREG(LP_FIFO, (y * pScrn->displayWidth + x) * pI740->cpp);
  OUTREG(LP_FIFO, 0x00000000);
  OUTREG(LP_FIFO, 0x00000000);
  OUTREG(LP_FIFO, (h << 16) | (w * pI740->cpp));
}
#if 0
static void
I740SetupForCPUToScreenColorExpandFill(ScrnInfoPtr pScrn, int bg, int fg, 
				       int rop, unsigned int planemask) {
  I740Ptr pI740;

  pI740 = I740PTR(pScrn);
  pI740->bltcmd.BR00 = (pScrn->displayWidth * pI740->cpp) << 16;
  pI740->bltcmd.BR01 = bg;
  pI740->bltcmd.BR02 = fg;
#ifdef USE_DWORD_COLOR_EXP
  pI740->bltcmd.BR03 = MONO_DWORD_ALIGN | MONO_USE_COLEXP;
#else
  pI740->bltcmd.BR03 = MONO_BIT_ALIGN | MONO_USE_COLEXP;
#endif
  pI740->bltcmd.BR04 = SRC_IS_MONO | SRC_USE_BLTDATA | i740Rop[rop];
  if (bg == -1) pI740->bltcmd.BR04 |= MONO_SRC_TRANSP;
}

static void
I740SubsequentCPUToScreenColorExpandFill(ScrnInfoPtr pScrn, int x, int y, 
				     int w, int h, int skipleft) {
  I740Ptr pI740;

  pI740 = I740PTR(pScrn);
  pI740->AccelInfoRec->Sync(pScrn);
  OUTREG(LP_FIFO, 0x6000000A);
  OUTREG(LP_FIFO, pI740->bltcmd.BR00);
  OUTREG(LP_FIFO, pI740->bltcmd.BR01);
  OUTREG(LP_FIFO, pI740->bltcmd.BR02);
  OUTREG(LP_FIFO, pI740->bltcmd.BR03 | (skipleft & MONO_SRC_LEFT_CLIP));
  OUTREG(LP_FIFO, pI740->bltcmd.BR04);
  OUTREG(LP_FIFO, 0x00000000);
  OUTREG(LP_FIFO, 0x00000000);
  OUTREG(LP_FIFO, (y * pScrn->displayWidth + x) * pI740->cpp);
  OUTREG(LP_FIFO, 0x00000000);
  OUTREG(LP_FIFO, 0x00000000);
#ifdef USE_DWORD_COLOR_EXP
  /*
   * This extra wait is necessary to keep the bitblt engine from
   * locking up, but I am not sure why it is needed.  If we take it
   * out, "x11perf -copyplane10" will lock the bitblt engine.  When
   * the bitblt engine is locked, it is waiting for mono data to be
   * written to the BLTDATA region, which seems to imply that some of
   * the data that was written was lost.  This might be fixed by
   * BLT_SKEW changes.  Update: The engine still locks up with this
   * extra wait.  More investigation (and time) is needed.
   */
  WAIT_BLT_IDLE();
#endif
  OUTREG(LP_FIFO, (h << 16) | (w * pI740->cpp));
}
#endif

/**********************************************************************
Copyright 1998, 1999 by Precision Insight, Inc., Cedar Park, Texas.

                        All Rights Reserved

Permission to use, copy, modify, distribute, and sell this software and
its documentation for any purpose is hereby granted without fee,
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in
supporting documentation, and that the name of Precision Insight not be
used in advertising or publicity pertaining to distribution of the
software without specific, written prior permission.  Precision Insight
and its suppliers make no representations about the suitability of this
software for any purpose.  It is provided "as is" without express or 
implied warranty.

PRECISION INSIGHT DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
EVENT SHALL PRECISION INSIGHT AND/OR ITS SUPPLIERS BE LIABLE FOR ANY
SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER
RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF
CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
**********************************************************************/
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/neomagic/neo_2090.c,v 1.7 2002/10/30 12:52:21 alanh Exp $ */

/*
 * The original Precision Insight driver for
 * XFree86 v.3.3 has been sponsored by Red Hat.
 *
 * Authors:
 *   Jens Owen (jens@tungstengraphics.com)
 *   Kevin E. Martin (kevin@precisioninsight.com)
 *
 * Port to Xfree86 v.4.0
 *   1998, 1999 by Egbert Eich (Egbert.Eich@Physik.TU-Darmstadt.DE)
 */

#include "xf86.h"
#include "xf86_OSproc.h"
#include "xf86_ansic.h"
#include "compiler.h"
#include "xf86PciInfo.h"

/* Drivers that use XAA need this */
#include "xf86fbman.h"

#include "miline.h"

#include "neo.h"
#include "neo_reg.h"
#include "neo_macros.h"

static unsigned int neo2090Rop[16] = {
    0x000000,    /* 0x0000 - GXclear         */
    0x080000,    /* 0x1000 - GXand           */
    0x040000,    /* 0x0100 - GXandReverse    */
    0x0c0000,    /* 0x1100 - GXcopy          */
    0x020000,    /* 0x0010 - GXandInvert     */
    0x0a0000,    /* 0x1010 - GXnoop          */
    0x060000,    /* 0x0110 - GXxor           */
    0x0e0000,    /* 0x1110 - GXor            */
    0x010000,    /* 0x0001 - GXnor           */
    0x090000,    /* 0x1001 - GXequiv         */
    0x050000,    /* 0x0101 - GXinvert        */
    0x0d0000,    /* 0x1101 - GXorReverse     */
    0x030000,    /* 0x0011 - GXcopyInvert    */
    0x0b0000,    /* 0x1011 - GXorInverted    */
    0x070000,    /* 0x0111 - GXnand          */
    0x0f0000     /* 0x1111 - GXset           */
};

static void Neo2090Sync(ScrnInfoPtr pScrn);
static void Neo2090SetupForScreenToScreenCopy(ScrnInfoPtr pScrn, int xdir,
					      int ydir, int rop,
					      unsigned int planemask,
					      int trans_color);
static void Neo2090SubsequentScreenToScreenCopy(ScrnInfoPtr pScrn, int srcX,
						int srcY, int dstX, int dstY,
						int w, int h);
static void Neo2090SetupForSolidFillRect(ScrnInfoPtr pScrn, int color, int rop,
				  unsigned int planemask);
static void Neo2090SubsequentSolidFillRect(ScrnInfoPtr pScrn, int x, int y,
					   int w, int h);
static void Neo2093SetupForScanlineCPUToScreenColorExpandFill(
                                                      ScrnInfoPtr pScrn,
						      int fg, int bg,
						      int rop,
						unsigned int planemask);
static void Neo2093SubsequentScanlineCPUToScreenColorExpandFill(
                                                      ScrnInfoPtr pScrn,
							int x, int y,
							int w, int h,
							int skipleft);
static void Neo2093SubsequentColorExpandScanline(ScrnInfoPtr pScrn, int bufno);

Bool 
Neo2090AccelInit(ScreenPtr pScreen)
{
    XAAInfoRecPtr infoPtr;
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    NEOPtr nPtr = NEOPTR(pScrn);
    NEOACLPtr nAcl = NEOACLPTR(pScrn);

    nPtr->AccelInfoRec = infoPtr = XAACreateInfoRec();
    if(!infoPtr) return FALSE;

    /*
     * Set up the main acceleration flags.
     */
    infoPtr->Flags = LINEAR_FRAMEBUFFER | OFFSCREEN_PIXMAPS;
    if(nAcl->cacheEnd > nAcl->cacheStart) infoPtr->Flags |= PIXMAP_CACHE;
#if 0
    infoPtr->PixmapCacheFlags |= DO_NOT_BLIT_STIPPLES;
#endif
    /* sync */
    infoPtr->Sync = Neo2090Sync;

    /* screen to screen copy */
    infoPtr->ScreenToScreenCopyFlags = (NO_TRANSPARENCY | NO_PLANEMASK);
    infoPtr->SetupForScreenToScreenCopy = 
	Neo2090SetupForScreenToScreenCopy;
    infoPtr->SubsequentScreenToScreenCopy = 
	Neo2090SubsequentScreenToScreenCopy;

    /* solid filled rectangles */
    infoPtr->SolidFillFlags = NO_PLANEMASK;
    infoPtr->SetupForSolidFill = 
	Neo2090SetupForSolidFillRect;
    infoPtr->SubsequentSolidFillRect = 
	Neo2090SubsequentSolidFillRect;

    if (nPtr->NeoChipset == PCI_CHIP_NM2093 && !nPtr->strangeLockups) {
	/*
	 * We do CPUToScreenColorExpand (ab)using the Scanline functions:
	 * the neo chipsets need byte padding however we can only do dword
	 * padding. Fortunately the graphics engine doesn't choke if we
	 * transfer up to 3 bytes more than it wants.
	 */

	/* cpu to screen color expansion */
	infoPtr->ScanlineColorExpandBuffers =
	    (unsigned char **)xnfalloc(sizeof(char*));
	infoPtr->ScanlineColorExpandBuffers[0] = 
	    (unsigned char *)(nPtr->NeoMMIOBase + 0x100000);
	infoPtr->NumScanlineColorExpandBuffers = 1;
	infoPtr->ScanlineCPUToScreenColorExpandFillFlags = ( NO_PLANEMASK |
						     SCANLINE_PAD_DWORD |
						     CPU_TRANSFER_PAD_DWORD |
						BIT_ORDER_IN_BYTE_MSBFIRST );

	infoPtr->SetupForScanlineCPUToScreenColorExpandFill = 
	    Neo2093SetupForScanlineCPUToScreenColorExpandFill;
	infoPtr->SubsequentScanlineCPUToScreenColorExpandFill = 
	    Neo2093SubsequentScanlineCPUToScreenColorExpandFill;
	infoPtr->SubsequentColorExpandScanline =
	    Neo2093SubsequentColorExpandScanline;
    }

    /*
     * Setup some global variables
     */
    nAcl->ColorShiftAmt = 0;
    
    /* Initialize for 8bpp or 15/16bpp support accellerated */
    switch (pScrn->bitsPerPixel) {
    case 8:
	nAcl->BltCntlFlags = NEO_BC1_DEPTH8;
	nAcl->ColorShiftAmt = 8;
	break;
    case 15:
    case 16:
	nAcl->BltCntlFlags = NEO_BC1_DEPTH16;
	nAcl->ColorShiftAmt = 0;
	break;
    case 24:
    default:
	return FALSE;
    }

    /* Initialize for widths */
    switch (pScrn->displayWidth) {
    case 640:
	nAcl->BltCntlFlags |= NEO_BC1_X_640;
	break;
    case 800:
	nAcl->BltCntlFlags |= NEO_BC1_X_800;
	break;
    case 1024:
	nAcl->BltCntlFlags |= NEO_BC1_X_1024;
	break;
    default:
	return FALSE;
    }

    nAcl->BltCntlFlags |= NEO_BC3_FIFO_EN;

    return(XAAInit(pScreen, infoPtr));
}

static void
Neo2090Sync(ScrnInfoPtr pScrn)
{
    NEOPtr nPtr = NEOPTR(pScrn);

    WAIT_ENGINE_IDLE();
}

static void
Neo2090SetupForScreenToScreenCopy(ScrnInfoPtr pScrn, int xdir, int ydir,
				  int rop,
				  unsigned int planemask,
				  int trans_color)
{
    NEOPtr nPtr = NEOPTR(pScrn);
    NEOACLPtr nAcl = NEOACLPTR(pScrn);

    nAcl->tmpBltCntlFlags = (nAcl->BltCntlFlags  |
		              NEO_BC3_SKIP_MAPPING |
		              NEO_BC3_DST_XY_ADDR  |
		              NEO_BC3_SRC_XY_ADDR  | neo2090Rop[rop]);

    /* set blt control */
    WAIT_FIFO(2);
    OUTREG(NEOREG_BLTCNTL, nAcl->tmpBltCntlFlags);
}

static void
Neo2090SubsequentScreenToScreenCopy(ScrnInfoPtr pScrn,
				    int srcX, int srcY,
				    int dstX, int dstY,
				    int w, int h)
{
    NEOPtr nPtr = NEOPTR(pScrn);
    NEOACLPtr nAcl = NEOACLPTR(pScrn);

    if ((dstY < srcY) || ((dstY == srcY) && (dstX < srcX))) {
	/* start with upper left corner */
	WAIT_FIFO(4);
	OUTREG(NEOREG_BLTCNTL, nAcl->tmpBltCntlFlags);
	OUTREG(NEOREG_SRCSTARTOFF, (srcY<<16) | (srcX & 0xffff));
	OUTREG(NEOREG_DSTSTARTOFF, (dstY<<16) | (dstX & 0xffff));
	OUTREG(NEOREG_XYEXT, (h<<16) | (w & 0xffff));
    }
    else {
	/* start with lower right corner */
	WAIT_FIFO(4);
	OUTREG(NEOREG_BLTCNTL, (nAcl->tmpBltCntlFlags | NEO_BC0_X_DEC
                                                       | NEO_BC0_DST_Y_DEC 
                                                       | NEO_BC0_SRC_Y_DEC));
	OUTREG(NEOREG_SRCSTARTOFF, ((srcY+h-1)<<16) | ((srcX+w-1) & 0xffff));
	OUTREG(NEOREG_DSTSTARTOFF, ((dstY+h-1)<<16) | ((dstX+w-1) & 0xffff));
	OUTREG(NEOREG_XYEXT, (h<<16) | (w & 0xffff));
    }
}


static void
Neo2090SetupForSolidFillRect(ScrnInfoPtr pScrn, int color, int rop,
			     unsigned int planemask)
{
    NEOPtr nPtr = NEOPTR(pScrn);
    NEOACLPtr nAcl = NEOACLPTR(pScrn);

    WAIT_FIFO(2);

    /* set blt control */
    OUTREG(NEOREG_BLTCNTL, nAcl->BltCntlFlags  |
                           NEO_BC0_SRC_IS_FG    |
                           NEO_BC3_SKIP_MAPPING |
                           NEO_BC3_DST_XY_ADDR  |
                           NEO_BC3_SRC_XY_ADDR  | neo2090Rop[rop]);

    /* set foreground color */
    OUTREG(NEOREG_FGCOLOR, color |= (color << nAcl->ColorShiftAmt));
}


static void
Neo2090SubsequentSolidFillRect(ScrnInfoPtr pScrn, int x, int y, int w, int h)
{
    NEOPtr nPtr = NEOPTR(pScrn);

    WAIT_FIFO(2);
    OUTREG(NEOREG_DSTSTARTOFF, (y<<16) | (x & 0xffff));
    OUTREG(NEOREG_XYEXT, (h<<16) | (w & 0xffff));

}

static void
Neo2093SetupForScanlineCPUToScreenColorExpandFill(ScrnInfoPtr pScrn,
				      int fg, int bg,
				      int rop,
				      unsigned int planemask)
{
    NEOPtr nPtr = NEOPTR(pScrn);
    NEOACLPtr nAcl = NEOACLPTR(pScrn);

    if (bg == -1) {
	/* transparent setup */
	WAIT_FIFO(2);
	OUTREG(NEOREG_BLTCNTL, nAcl->BltCntlFlags  |
			       NEO_BC0_SYS_TO_VID   |
			       NEO_BC0_SRC_MONO     |
			       NEO_BC0_SRC_TRANS    |
			       NEO_BC3_SKIP_MAPPING |
			       NEO_BC3_DST_XY_ADDR  | neo2090Rop[rop]);

	OUTREG(NEOREG_FGCOLOR, fg |= (fg << nAcl->ColorShiftAmt));
    }
    else {
	/* opaque setup */
	WAIT_FIFO(3);
	OUTREG(NEOREG_BLTCNTL, nAcl->BltCntlFlags  |
			       NEO_BC0_SYS_TO_VID   |
			       NEO_BC0_SRC_MONO     |
			       NEO_BC3_SKIP_MAPPING |
			       NEO_BC3_DST_XY_ADDR  | neo2090Rop[rop]);

	OUTREG(NEOREG_FGCOLOR, fg |= (fg << nAcl->ColorShiftAmt));
	OUTREG(NEOREG_BGCOLOR, bg |= (bg << nAcl->ColorShiftAmt));
    }
}

static void
Neo2093SubsequentScanlineCPUToScreenColorExpandFill(ScrnInfoPtr pScrn,
					int x, int y,
					int w, int h,
					int skipleft)
{
    NEOPtr nPtr = NEOPTR(pScrn);
    NEOACLPtr nAcl = NEOACLPTR(pScrn);

    nAcl->CPUToScreenColorExpandFill_x = x;
    nAcl->CPUToScreenColorExpandFill_y = y;
    nAcl->CPUToScreenColorExpandFill_w = w;
    nAcl->CPUToScreenColorExpandFill_h = h;
    nAcl->CPUToScreenColorExpandFill_skipleft = skipleft;

    WAIT_FIFO(4);
    OUTREG(NEOREG_SRCBITOFF, skipleft);
    OUTREG(NEOREG_SRCSTARTOFF, 0);
    OUTREG(NEOREG_DSTSTARTOFF, (y<<16) | (x & 0xffff));
    OUTREG(NEOREG_XYEXT, (1<<16) | (w & 0xffff));
}

static void
Neo2093SubsequentColorExpandScanline(ScrnInfoPtr pScrn,	int bufno)
{
    NEOPtr nPtr = NEOPTR(pScrn);
    NEOACLPtr nAcl = NEOACLPTR(pScrn);

    if (!(--nAcl->CPUToScreenColorExpandFill_h))
	return;

    WAIT_FIFO(4);
    OUTREG(NEOREG_SRCBITOFF, nAcl->CPUToScreenColorExpandFill_skipleft);
    OUTREG(NEOREG_SRCSTARTOFF, 0);
    OUTREG(NEOREG_DSTSTARTOFF, ((++nAcl->CPUToScreenColorExpandFill_y)<<16)
	   | (nAcl->CPUToScreenColorExpandFill_x & 0xffff));
    OUTREG(NEOREG_XYEXT, (1<<16)
	   | (nAcl->CPUToScreenColorExpandFill_w & 0xffff));
}




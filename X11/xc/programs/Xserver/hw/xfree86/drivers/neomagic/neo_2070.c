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
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/neomagic/neo_2070.c,v 1.5 2002/10/30 12:52:21 alanh Exp $ */

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

/* Drivers that use XAA need this */
#include "xf86fbman.h"

#include "miline.h"

#include "neo.h"
#include "neo_reg.h"
#include "neo_macros.h"

/* Memory Mapped I/O for BitBlt */
#define NEO2070_BLTSTAT		0x00
#define NEO2070_BLTCNTL		0x04
#define NEO2070_XPCOLOR		0x08
#define NEO2070_FGCOLOR		0x0c
#define NEO2070_BGCOLOR		0x10
#define NEO2070_PLANEMASK	0x14
#define NEO2070_XYEXT           0x18
#define NEO2070_SRCPITCH        0x1c
#define NEO2070_SRCBITOFF       0x20
#define NEO2070_SRCSTART        0x24
#define NEO2070_DSTPITCH        0x28
#define NEO2070_DSTBITOFF       0x2c
#define NEO2070_DSTSTART        0x30

static unsigned int neo2070Rop[16] = {
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

static void Neo2070Sync(ScrnInfoPtr pScrn);
static void Neo2070SetupForScreenToScreenCopy(ScrnInfoPtr pScrn, int xdir,
					      int ydir, int rop,
					      unsigned int planemask,
					      int trans_color);
static void Neo2070SubsequentScreenToScreenCopy(ScrnInfoPtr pScrn, int srcX,
						int srcY, int dstX, int dstY,
						int w, int h);
static void Neo2070SetupForSolidFillRect(ScrnInfoPtr pScrn, int color, int rop,
				  unsigned int planemask);
static void Neo2070SubsequentSolidFillRect(ScrnInfoPtr pScrn, int x, int y,
					   int w, int h);

Bool 
Neo2070AccelInit(ScreenPtr pScreen)
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
    infoPtr->Sync = Neo2070Sync;

    /* screen to screen copy */
    infoPtr->ScreenToScreenCopyFlags = (NO_TRANSPARENCY | GXCOPY_ONLY);
    infoPtr->SetupForScreenToScreenCopy = 
	Neo2070SetupForScreenToScreenCopy;
    infoPtr->SubsequentScreenToScreenCopy = 
	Neo2070SubsequentScreenToScreenCopy;

    /* solid filled rectangles */
    infoPtr->SolidFillFlags = GXCOPY_ONLY;
    infoPtr->SetupForSolidFill = 
	Neo2070SetupForSolidFillRect;
    infoPtr->SubsequentSolidFillRect = 
	Neo2070SubsequentSolidFillRect;

    /*
     * Setup some global variables
     */
    
    /* Initialize for 8bpp or 15/16bpp support accellerated */
    switch (pScrn->bitsPerPixel) {
    case 8:
	nAcl->BltCntlFlags = NEO_BC1_DEPTH8;
	nAcl->ColorShiftAmt = 8;
	nAcl->PixelWidth = 1;
	nAcl->PlaneMask = 0xff;
	break;
    case 15:
    case 16:
	nAcl->BltCntlFlags = NEO_BC1_DEPTH16;
	nAcl->ColorShiftAmt = 0;
	nAcl->PixelWidth = 2;
	nAcl->PlaneMask = 0xffff;
	break;
    case 24: /* not supported, but check anyway */
    default:
	return FALSE;
    }
    
    return(XAAInit(pScreen, infoPtr));

}

static void
Neo2070Sync(ScrnInfoPtr pScrn)
{
    NEOPtr nPtr = NEOPTR(pScrn);
    WAIT_ENGINE_IDLE();
}

static void
Neo2070SetupForScreenToScreenCopy(ScrnInfoPtr pScrn, int xdir, int ydir,
				  int rop,
				  unsigned int planemask,
				  int trans_color)
{
    NEOPtr nPtr = NEOPTR(pScrn);
    NEOACLPtr nAcl = NEOACLPTR(pScrn);

    /* set blt control */
    WAIT_ENGINE_IDLE();
    OUTREG(NEO2070_BLTCNTL, nAcl->tmpBltCntlFlags);
    OUTREG(NEO2070_PLANEMASK, planemask |= (planemask << nAcl->ColorShiftAmt));
    OUTREG(NEO2070_SRCPITCH, nAcl->Pitch);
    OUTREG(NEO2070_DSTPITCH, nAcl->Pitch);
    OUTREG(NEO2070_SRCBITOFF, 0);
    OUTREG(NEO2070_DSTBITOFF, 0);
}

static void
Neo2070SubsequentScreenToScreenCopy(ScrnInfoPtr pScrn,
				    int srcX, int srcY,
				    int dstX, int dstY,
				    int w, int h)
{
    NEOPtr nPtr = NEOPTR(pScrn);
    NEOACLPtr nAcl = NEOACLPTR(pScrn);

    if ((dstY < srcY) || ((dstY == srcY) && (dstX < srcX))) {
	/* start with upper left corner */
	WAIT_ENGINE_IDLE();
	OUTREG(NEO2070_BLTCNTL, nAcl->tmpBltCntlFlags);
	OUTREG(NEO2070_XYEXT, ((h-1)<<16) | ((w-1) & 0xffff));
	OUTREG(NEO2070_SRCSTART, 
	    (srcY * nAcl->Pitch) + (srcX * nAcl->PixelWidth));
	OUTREG(NEO2070_DSTSTART, 
	    (dstY * nAcl->Pitch) + (dstX * nAcl->PixelWidth));
    }
    else {
	/* start with lower right corner */
	WAIT_ENGINE_IDLE();
	OUTREG(NEO2070_BLTCNTL, (nAcl->tmpBltCntlFlags | NEO_BC0_X_DEC
                                                        | NEO_BC0_DST_Y_DEC 
                                                        | NEO_BC0_SRC_Y_DEC));
	OUTREG(NEO2070_XYEXT, ((h-1)<<16) | ((w-1) & 0xffff));
	OUTREG(NEO2070_SRCSTART, 
	    ((srcY+h-1) * nAcl->Pitch) + ((srcX+w-1) * nAcl->PixelWidth));
	OUTREG(NEO2070_DSTSTART, 
	    ((dstY+h-1) * nAcl->Pitch) + ((dstX+w-1) * nAcl->PixelWidth));
    }
}


static void
Neo2070SetupForSolidFillRect(ScrnInfoPtr pScrn, int color, int rop,
			     unsigned int planemask)
{
    NEOPtr nPtr = NEOPTR(pScrn);
    NEOACLPtr nAcl = NEOACLPTR(pScrn);

    planemask &= nAcl->PlaneMask;
    if (!rop) color=0;

    WAIT_ENGINE_IDLE();

    OUTREG(NEO2070_BLTCNTL, nAcl->BltCntlFlags  |
                            NEO_BC0_SRC_IS_FG    | neo2070Rop[3]);
    OUTREG(NEO2070_PLANEMASK, planemask |= (planemask << nAcl->ColorShiftAmt));
    if (pScrn->bitsPerPixel == 8) 
	OUTREG(NEO2070_FGCOLOR, color |= (color << 8));
    else
	/* swap bytes in color */
	OUTREG(NEO2070_FGCOLOR, ((color&0xff00) >> 8) | (color << 8));
    OUTREG(NEO2070_SRCPITCH, nAcl->Pitch);
    OUTREG(NEO2070_DSTPITCH, nAcl->Pitch);
    OUTREG(NEO2070_SRCBITOFF, 0);
    OUTREG(NEO2070_DSTBITOFF, 0);
}


static void
Neo2070SubsequentSolidFillRect(ScrnInfoPtr pScrn, int x, int y, int w, int h)
{
    NEOPtr nPtr = NEOPTR(pScrn);
    NEOACLPtr nAcl = NEOACLPTR(pScrn);

    WAIT_ENGINE_IDLE();
    OUTREG(NEO2070_XYEXT, ((h-1)<<16) | ((w-1) & 0xffff));
    OUTREG(NEO2070_DSTSTART, (y * nAcl->Pitch) + (x * nAcl->PixelWidth));
}






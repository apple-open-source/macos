/*
 * Copyright 2000 by Richard A. Hecker, California, United States
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Richard Hecker not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  Richard Hecker makes no representations
 * about the suitability of this software for any purpose.  It is provided
 * "as is" without express or implied warranty.
 *
 * RICHARD HECKER DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL RICHARD HECKER BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 *
 * Author:  Richard Hecker, hecker@cat.dfrc.nasa.gov
 *          Re-written for XFree86 v4.0
 * Previous driver (pre-XFree86 v4.0) by
 *          Annius V. Groenink (A.V.Groenink@zfc.nl, avg@cwi.nl),
 *          Dirk H. Hohndel (hohndel@suse.de),
 *          Portions: the GGI project & confidential CYRIX databooks.
 */
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/cyrix/cyrix_accel.c,v 1.5 2002/11/06 11:38:59 alanh Exp $ */

#include "vgaHW.h"
#include "xf86.h"
#include "xf86_ansic.h"
#include "xaalocal.h"
#include "cyrix.h"
#include "miline.h"
#include "xf86_libc.h"
#include "compiler.h"

/* size of color expand source area (embedded in frame buffer) */
#define CYRIXexpandSize       32768

/* Raster operations are converted in such a way that we can use them to
   do planemask operations: lower nybble is NOP (pattern=planemask),
   upper nybble inverted X raster operation (bits 0 - 3 correspond to
   bits 3 - 0 and 7 - 4 in Windows style ROP).  In some routines,
   the role of source and pattern is inverted. */
static const int windowsROPpatMask[16] = { 0x0A, 0x8A, 0x4A, 0xCA,
                                           0x2A, 0xAA, 0x6A, 0xEA,
                                           0x1A, 0x9A, 0x5A, 0xDA,
                                           0x3A, 0xBA, 0x7A, 0xFA };

static const int windowsROPsrcMask[16] = { 0x22, 0xA2, 0x62, 0xE2,
                                           0x2A, 0xAA, 0x6A, 0xEA,
                                           0x26, 0xA6, 0x66, 0xE6,
                                           0x2E, 0xAE, 0x6E, 0xEE };

/* Forward declaration of functions used in the driver */
void CYRIXAccelSync(ScrnInfoPtr pScrn);
void CYRIXSetupForSolidFill(ScrnInfoPtr pScrn, int color, int rop,
				unsigned int planemask);
void CYRIXSubsequentSolidFillRect(ScrnInfoPtr pScrn, int x, int y,
					int w, int h);
void CYRIXSetupForScreenToScreenCopy(ScrnInfoPtr, int xdir, int ydir,
		int rop, unsigned int planemask, int transparency_color);
void CYRIXSubsequentScreenToScreenCopy(ScrnInfoPtr, int x1, int y1, int x2,
					int y2, int w, int h);
void CYRIXSubsequentBresenhamLine(ScrnInfoPtr pScrn,int x1, int y1,
				  int e1, int e2, int err, int length,
				  int octant);
void CYRIXSetupForColor8x8PatternFillRect(ScrnInfoPtr pScrn, int patternx,
	int patterny, int rop, unsigned int planemask, int transparency_color);
void CYRIXSubsequentColor8x8PatternFillRect(ScrnInfoPtr pScrn, int patternx,
				int patterny, int x, int y, int w, int h);
void CYRIXSetupForCPUToScreenColorExpandFill(ScrnInfoPtr pScrn, int fg,
			int bg, int rop, unsigned int planemask);
void CYRIXSubsequentCPUToScreenColorExpandFill(ScrnInfoPtr pScrn, int x,
			int y, int w, int h, int skipleft);
void InitPixmapCache(ScreenPtr pScreen, RegionPtr areas, pointer data);
/* Info Rec for all these routines to use */

/* Acceleration init function, sets up pointers to our accelerated functions */
void 
CYRIXAccelInit(ScreenPtr pScreen) 
{	/* General acceleration flags */

	CYRIXPrvPtr	pCyrix;
	ScrnInfoPtr 	pScrn;
	XAAInfoRecPtr localRecPtr;

	pScrn = xf86Screens[pScreen->myNum];
	pCyrix = CYRIXPTR(pScrn);
	pCyrix->AccelInfoRec = localRecPtr = XAACreateInfoRec();
	localRecPtr->Flags = PIXMAP_CACHE
	                       | PW_BACKGROUND
#if 0
	                       | HARDWARE_PATTERN_MONO_TRANSPARENCY
#endif
	                       | HARDWARE_PATTERN_SCREEN_ORIGIN
	                       | BIT_ORDER_IN_BYTE_MSBFIRST
	                       | HARDWARE_PATTERN_PROGRAMMED_BITS;

	/* Sync */
	localRecPtr->Sync = CYRIXAccelSync;

	/* Filled rectangles */
	localRecPtr->SetupForSolidFill = 
	    CYRIXSetupForSolidFill;
	localRecPtr->SubsequentSolidFillRect = 
	    CYRIXSubsequentSolidFillRect;
	pCyrix->AccelInfoRec->PolyFillRectSolidFlags = NO_PLANEMASK;

	/* ScreenToScreen copies */
	localRecPtr->SetupForScreenToScreenCopy =
	    CYRIXSetupForScreenToScreenCopy;
	localRecPtr->SubsequentScreenToScreenCopy =
	    CYRIXSubsequentScreenToScreenCopy;

	pCyrix->AccelInfoRec->CopyAreaFlags = NO_PLANEMASK | GXCOPY_ONLY;

#if 0
	/* Bresenham lines - disable because of minor display errors */
	localRecPtr->SubsequentBresenhamLine =
	    CYRIXSubsequentBresenhamLine;
	localRecPtr->ErrorTermBits = 15;
#endif

	/* 8x8 color-expanded patterns */
	localRecPtr->SetupForColor8x8PatternFill =
	    CYRIXSetupForColor8x8PatternFillRect;
	localRecPtr->SubsequentColor8x8PatternFillRect =
	    CYRIXSubsequentColor8x8PatternFillRect;

	/* Color expansion */
	localRecPtr->Color8x8PatternFillFlags =
			BIT_ORDER_IN_BYTE_MSBFIRST | NO_PLANEMASK |
			TRANSPARENCY_GXCOPY_ONLY | SCANLINE_PAD_DWORD;

	/* Use two blit buffers in a row for text expansion
	   (this is an undefendable fix to a text display distortion
	   bug if we don't give XAA enough room, but the only thing that
	   seems to make it work properly) */
	localRecPtr->ColorExpandBase =
	    (unsigned char*)(pCyrix->GXregisters + pCyrix->CYRIXbltBuf0Address);
	localRecPtr->ColorExpandRange =
	    pCyrix->CYRIXbltBufSize * 2;

	localRecPtr->SetupForCPUToScreenColorExpandFill =
	    CYRIXSetupForCPUToScreenColorExpandFill;
	localRecPtr->SubsequentCPUToScreenColorExpandFill =
	    CYRIXSubsequentCPUToScreenColorExpandFill;

	/* calculate the pixel width of a blit buffer for convenience */
	pCyrix->bltBufWidth = pCyrix->CYRIXbltBufSize / (pScrn->bitsPerPixel / 8);
} 


/* set colors - called through access macros in cyrix.h */
static __inline__ void CYRIXsetColors01(ScrnInfoPtr pScrn, int reg,
					int col0, int col1) {
	CYRIXPrvPtr pCyrix = CYRIXPTR(pScrn);
	
    if (pScrn->bitsPerPixel == 16)
		GX_REG(reg) = ((col1 & 0xFFFF) << 16) | (col0 & 0xFFFF);
	else
	{	col0 &= 0xFF;
		col1 &= 0xFF;
		GX_REG(reg) = (col1 << 24) | (col1 << 16) | (col0 << 8) | col0;
}	}


/* The generic Sync() function that waits for everything to
   be completed (e.g. before writing to the frame buffer
   directly). */
void
CYRIXAccelSync(ScrnInfoPtr pScrn)
{
    CYRIXPrvPtr pCyrix = CYRIXPTR(pScrn);
	
    if (pCyrix->setBlitModeOnSync)
	{	pCyrix->setBlitModeOnSync = 0;
		CYRIXsetupSync();
		CYRIXsetBlitMode();
	}
	while (GX_REG(GP_BLIT_STATUS) &
	       (BS_BLIT_BUSY|BS_PIPELINE_BUSY|BS_BLIT_PENDING));
}


/* Solid rectangles */
void 
CYRIXSetupForSolidFill(pScrn, color, rop, planemask)
ScrnInfoPtr pScrn;
int color, rop;
unsigned int planemask;
{
	CYRIXPrvPtr	pCyrix;

	pCyrix = CYRIXPTR(pScrn);
	if (pCyrix->AccelInfoRec->PolyFillRectSolidFlags & GXCOPY_ONLY)
	    rop = GXcopy;
	if (pCyrix->AccelInfoRec->PolyFillRectSolidFlags & NO_PLANEMASK)
	    planemask = 0xFFFF;
	CYRIXsetupSync();
	CYRIXsetSourceColors01(pScrn, color, color);
	CYRIXsetPatColors01(pScrn, planemask, 0);
	CYRIXsetPatMode(rop, RM_PAT_DISABLE);
	pCyrix->blitMode = BM_READ_SRC_NONE | BM_WRITE_FB | BM_SOURCE_EXPAND
	    | IfDest(rop, planemask, BM_READ_DST_FB0);
	pCyrix->vectorMode = IfDest(rop, planemask, VM_READ_DST_FB);
}
    
    
void 
CYRIXSubsequentSolidFillRect(pScrn, x, y, w, h)
ScrnInfoPtr pScrn;
int x, y, w, h;
{
    CYRIXPrvPtr pCyrix = CYRIXPTR(pScrn);

    /* divide the operation into columns if required; use twice the
           blit buffer width because buffer 0 will overflow into buffer 1 */
	while (w > 2 * pCyrix->bltBufWidth)
	{	CYRIXSubsequentSolidFillRect(pScrn, x, y,
					     2 * pCyrix->bltBufWidth, h);
		x += 2 * pCyrix->bltBufWidth;
		w -= 2 * pCyrix->bltBufWidth;
	}
	CYRIXsetupSync();
	CYRIXsetDstXY(x, y);
	CYRIXsetWH(w, h);
	CYRIXsetBlitMode();
}


/* Screen to screen copies */
void 
CYRIXSetupForScreenToScreenCopy(pScrn, xdir, ydir, rop,
				planemask, transparency_color)
ScrnInfoPtr pScrn;
int xdir, ydir;
int rop;
unsigned int planemask;
int transparency_color;
{
	CYRIXPrvPtr	pCyrix;

	pCyrix = CYRIXPTR(pScrn);
	if (pCyrix->AccelInfoRec->CopyAreaFlags & NO_PLANEMASK)
		planemask = 0xFFFF;
	if (pCyrix->AccelInfoRec->CopyAreaFlags & GXCOPY_ONLY)
		rop = GXcopy;
	if (pCyrix->AccelInfoRec->CopyAreaFlags & NO_TRANSPARENCY)
		transparency_color = -1;

	CYRIXsetupSync();
	CYRIXsetPatColors01(pScrn, planemask, 0);

	if (transparency_color == -1)
	{	CYRIXsetPatMode(rop, RM_PAT_DISABLE);
		pCyrix->transMode = 0;
	}
	else
	{	CYRIXsetPatModeTrans(RM_PAT_DISABLE);
		pCyrix->transMode = 1;

		if (pCyrix->AccelInfoRec->CopyAreaFlags &
						TRANSPARENCY_GXCOPY_ONLY)
			rop = GXcopy;

		/* fill blit buffer 1 with the transparency color */
		if (pScrn->bitsPerPixel == 16)
		{	int              k   = pCyrix->CYRIXbltBufSize / 4;
			CARD32           val = (transparency_color << 16) |
			                       transparency_color;
			volatile CARD32* buf = &(GX_REG(pCyrix->CYRIXbltBuf1Address));

			while (--k >= 0) buf[k] = val;
		}
		else
			memset(pCyrix->GXregisters + pCyrix->CYRIXbltBuf1Address,
			       transparency_color, pCyrix->CYRIXbltBufSize);
	}

	pCyrix->blitMode = BM_READ_SRC_FB | BM_WRITE_FB | BM_SOURCE_COLOR
	    | (pCyrix->transMode ? BM_READ_DST_NONE : IfDest(rop, planemask, BM_READ_DST_FB1))
	    | (ydir < 0 ? BM_REVERSE_Y : 0);

	pCyrix->copyXdir = xdir;
}

void 
CYRIXSubsequentScreenToScreenCopy(pScrn, x1, y1, x2, y2, w, h)
ScrnInfoPtr pScrn;
int x1, y1, x2, y2, w, h;
{
    CYRIXPrvPtr pCyrix = CYRIXPTR(pScrn);
    int up       = (pCyrix->blitMode & BM_REVERSE_Y);

	/* divide the operation into columns when necessary */
	if (pCyrix->copyXdir < 0)
	{	int x_offset = w - pCyrix->bltBufWidth;

		while (x_offset > 0)
		{	CYRIXSubsequentScreenToScreenCopy(pScrn, x1 + x_offset, y1,
			                                  x2 + x_offset, y2,
			                                  pCyrix->bltBufWidth, h);
			x_offset -= pCyrix->bltBufWidth;
			w -= pCyrix->bltBufWidth;
	}	}
	else while (w > pCyrix->bltBufWidth)
	{	CYRIXSubsequentScreenToScreenCopy(pScrn, x1, y1, x2, y2,
		                                  pCyrix->bltBufWidth, h);
		x1 += pCyrix->bltBufWidth;
		x2 += pCyrix->bltBufWidth;
		w -= pCyrix->bltBufWidth;
	}

	CYRIXsetupSync();
	CYRIXsetSrcXY(x1, (up ? (y1 + h - 1) : y1));
	CYRIXsetDstXY(x2, (up ? (y2 + h - 1) : y2));

	/* in transparent mode, one line reads the transparency color
	   into a processor-internal register, and the remaining lines
	   can be done in a single second pass */
	if (pCyrix->transMode)
	{	pCyrix->blitMode |= BM_READ_DST_BB1;
		CYRIXsetWH(w, 1);
		CYRIXsetBlitMode();
		h--;
		if (!h) return;
		if (up) { y1--; y2--; }
		else { y1++; y2++; }
		CYRIXsetupSync();
		pCyrix->blitMode &= ~(BM_READ_DST_BB1);
	}
	CYRIXsetWH(w, h);
	CYRIXsetBlitMode();
}


/* Bresenham lines */
void
CYRIXSubsequentBresenhamLine(pScrn, x1, y1, e1, e2, err, length, octant)
ScrnInfoPtr pScrn; int x1, y1, octant, err, e1, e2, length;
{
    CYRIXPrvPtr pCyrix = CYRIXPTR(pScrn);
    
    if (octant & YMAJOR) {
	pCyrix->vectorMode = (pCyrix->vectorMode & VM_READ_DST_FB) | VM_Y_MAJOR;
	if (!(octant & XDECREASING)) pCyrix->vectorMode |= VM_MINOR_INC;
	if (!(octant & YDECREASING)) pCyrix->vectorMode |= VM_MAJOR_INC;
    } else {
	pCyrix->vectorMode = (pCyrix->vectorMode & VM_READ_DST_FB) | VM_X_MAJOR;
	if (!(octant & XDECREASING)) pCyrix->vectorMode |= VM_MAJOR_INC;
	if (!(octant & YDECREASING)) pCyrix->vectorMode |= VM_MINOR_INC;
    }
    
    CYRIXsetupSync();
    CYRIXsetDstXY(x1, y1);
    CYRIXsetWH(length, (err & 0xFFFF));
    CYRIXsetSrcXY((e1 & 0xFFFF), (e2 & 0xFFFF));
    CYRIXsetVectorMode();
}


/* 8x8 pattern color expand */
void CYRIXSetupForColor8x8PatternFillRect(pScrn, patternx, patterny,
						rop, planemask, transparency_color)
ScrnInfoPtr pScrn;
int patternx, patterny;
int rop, transparency_color;
unsigned int planemask;
{
    CYRIXPrvPtr pCyrix = CYRIXPTR(pScrn);
    XAAInfoRecPtr localRecPtr = pCyrix->AccelInfoRec;
    
	if (localRecPtr->Color8x8PatternFillFlags & NO_PLANEMASK)
		planemask = 0xFFFF;
	if ((transparency_color == -1) && (localRecPtr->Color8x8PatternFillFlags &
						TRANSPARENCY_GXCOPY_ONLY))
		rop = GXcopy;

	CYRIXsetupSync();
	CYRIXsetPatColors01(pScrn, (transparency_color == -1) ?
					0 : transparency_color, planemask);
	CYRIXsetPatData(patternx, patterny);
	CYRIXsetPatModeX(rop, RM_PAT_MONO | ((transparency_color == -1) ?
						RM_PAT_TRANSPARENT : 0));

	pCyrix->blitMode = BM_READ_SRC_NONE | BM_WRITE_FB | BM_SOURCE_EXPAND |
	         ((transparency_color == -1) ?
		 IfDest(rop, planemask, BM_READ_DST_FB0) : BM_READ_DST_NONE);
}

void CYRIXSubsequentColor8x8PatternFillRect(pScrn, patternx, patterny, x, y, w, h)
ScrnInfoPtr pScrn;
int patternx, patterny;
int x, y, w, h;
{	CYRIXSubsequentSolidFillRect(pScrn, x, y, w, h);
}


/* CPU-to-screen color expansion */
void CYRIXSetupForCPUToScreenColorExpandFill(pScrn, fg, bg, rop, planemask)
ScrnInfoPtr pScrn;
int bg, fg, rop;
unsigned int planemask;
{
    CYRIXPrvPtr pCyrix = CYRIXPTR(pScrn);
    XAAInfoRecPtr localRecPtr = pCyrix->AccelInfoRec;

    int trans = (bg == -1);

	if (trans && (localRecPtr->CPUToScreenColorExpandFillFlags &
						TRANSPARENCY_GXCOPY_ONLY))
		rop = GXcopy;

	CYRIXsetupSync();
	CYRIXsetSourceColors01(pScrn, trans ? 0 : bg, fg);
	CYRIXsetPatColors01(pScrn, planemask, 0);

	CYRIXsetPatMode(rop, RM_PAT_DISABLE | (trans ? RM_SRC_TRANSPARENT : 0));

	/* this is formally incorrect: XAA may use both BB0 and BB1
	   for the text source bitmap, so READ_DST_FB1 should not be
	   used.  So far, this problem has not manifested itself in
	   practice. */
	pCyrix->blitMode = BM_READ_SRC_BB0 | BM_WRITE_FB | BM_SOURCE_EXPAND
	    | (trans ? IfDest(rop, planemask, BM_READ_DST_FB1) : BM_READ_DST_NONE);
}

void CYRIXSubsequentCPUToScreenColorExpandFill(pScrn, x, y, w, h, skipleft)
ScrnInfoPtr pScrn;
int x, y, w, h;
int skipleft;
{
    CYRIXPrvPtr pCyrix = CYRIXPTR(pScrn);
    CYRIXsetupSync();
    CYRIXsetSrcXY(0, 0);
    CYRIXsetDstXY(x, y);
    CYRIXsetWH(w, h);
    
    CYRIXAccelSync(pScrn);
    pCyrix->setBlitModeOnSync = 1;
}


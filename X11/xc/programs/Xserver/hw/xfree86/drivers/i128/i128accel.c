/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/i128/i128accel.c,v 1.8 2003/02/17 16:08:28 dawes Exp $ */

/*
 * Copyright 1997-2000 by Robin Cutshaw <robin@XFree86.Org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Robin Cutshaw not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  Robin Cutshaw makes no representations
 * about the suitability of this software for any purpose.  It is provided
 * "as is" without express or implied warranty.
 *
 * ROBIN CUTSHAW DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL ROBIN CUTSHAW BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 *
 */

#include "xaa.h"
#include "xaalocal.h"
#include "xf86fbman.h"
#include "miline.h"
#include "servermd.h"

#include "xf86.h"
#include "xf86_OSproc.h"
#include "xf86_ansic.h"
#include "xf86Pci.h"
#include "xf86PciInfo.h"

#include "i128.h"
#include "i128reg.h"

void I128BitBlit(ScrnInfoPtr pScrn, int x1, int y1, int x2, int y2,
	int w, int h);
void I128SetupForScreenToScreenCopy(ScrnInfoPtr pScrn, int xdir, int ydir,
	int rop, unsigned planemask, int transparency_color);
void I128SubsequentScreenToScreenCopy(ScrnInfoPtr pScrn, int x1, int y1,
	int x2, int y2, int w, int h);
void I128SetupForSolidFill(ScrnInfoPtr pScrn, int color, int rop,
	unsigned planemask);
void I128SubsequentSolidFillRect(ScrnInfoPtr pScrn, int x, int y, int w, int h);
void I128SubsequentSolidTwoPointLine(ScrnInfoPtr pScrn, int x1, int y1, int x2,
	int y2, int flags);
void I128SetClippingRectangle(ScrnInfoPtr pScrn, int x1, int y1,
	int x2, int y2);
void I128FillSolidRects(ScrnInfoPtr pScrn, int fg, int rop,
	unsigned int planemask, int nBox, register BoxPtr pBoxI);
void I128ScreenToScreenBitBlt(ScrnInfoPtr pScrn, int nbox, DDXPointPtr pptSrc,
	BoxPtr pbox, int xdir, int ydir, int alu, unsigned planemask);


#define ENG_PIPELINE_READY() { while (pI128->mem.rbase_a[BUSY] & BUSY_BUSY) ; }
#define ENG_DONE() { while (pI128->mem.rbase_a[FLOW] & (FLOW_DEB | FLOW_MCB | FLOW_PRV)) ;}


/* pre-shift rops and just or in as needed */

static CARD32 i128alu[16] =
{
   CR_CLEAR<<8,
   CR_AND<<8,
   CR_AND_REV<<8,
   CR_COPY<<8,
   CR_AND_INV<<8,
   CR_NOOP<<8,
   CR_XOR<<8,
   CR_OR<<8,
   CR_NOR<<8,
   CR_EQUIV<<8,
   CR_INVERT<<8,
   CR_OR_REV<<8,
   CR_COPY_INV<<8,
   CR_OR_INV<<8,
   CR_NAND<<8,
   CR_SET<<8
};
                        /*  8bpp   16bpp  32bpp unused */
static int min_size[]   = { 0x62,  0x32,  0x1A, 0x00 };
static int max_size[]   = { 0x80,  0x40,  0x20, 0x00 };
static int split_size[] = { 0x20,  0x10,  0x08, 0x00 };


void
I128EngineDone(ScrnInfoPtr pScrn)
{
	I128Ptr pI128 = I128PTR(pScrn);
	ENG_DONE();
}


void
I128BitBlit(ScrnInfoPtr pScrn, int x1, int y1, int x2, int y2, int w, int h)
{
	I128Ptr pI128 = I128PTR(pScrn);

#if 0
	if (pI128->Debug)
		xf86DrvMsg(pScrn->scrnIndex, X_INFO, "BB %d,%d %d,%d %d,%d 0x%x/0x%x\n", x1, y1, x2, y2, w, h, pI128->cmd);
#endif

	ENG_PIPELINE_READY();

	pI128->mem.rbase_a[CMD] = pI128->cmd;
	/*pI128->mem.rbase_a[XY3_DIR] = pI128->blitdir;*/

	if (pI128->blitdir & DIR_RL_TB) {
		x1 += w; x1--;
		x2 += w; x2--;
	}
	if (pI128->blitdir & DIR_LR_BT) {
		y1 += h; y1--;
		y2 += h; y2--;
	}



	if (pI128->Chipset == PCI_CHIP_I128) {
		int bppi;

		static int first_time_through = 1;

		/* The I128-1 has a nasty bitblit bug
		 * that occurs when dest is exactly 8 pages wide
		 */
		
		bppi = (pI128->mem.rbase_a[BUF_CTRL] & BC_PSIZ_MSK) >> 24;

		if ((w >= min_size[bppi]) && (w <= max_size[bppi])) {
			if (first_time_through) {
	    			xf86DrvMsg(pScrn->scrnIndex, X_INFO,
            				"Using I128-1 workarounds.\n");
				first_time_through = 0;
			}

			bppi = split_size[bppi];
#if 1
			/* split method */

			pI128->mem.rbase_a[XY2_WH] = (bppi<<16) | h;
			pI128->mem.rbase_a[XY0_SRC] = (x1<<16) | y1;	MB;
			pI128->mem.rbase_a[XY1_DST] = (x2<<16) | y2;	MB;

			ENG_PIPELINE_READY();

			w -= bppi;

			if (pI128->blitdir & DIR_RL_TB) {
				/* right to left blit */
				x1 -= bppi;
				x2 -= bppi;
			} else {
				/* left to right blit */
				x1 += bppi;
				x2 += bppi;
			}
#else
			/* clip method */
			pI128->mem.rbase_a[CLPTL] = (x2<<16) | y2;
			pI128->mem.rbase_a[CLPBR] = ((x2+w)<<16) | (y2+h);
			w += bppi;
#endif
		}
	}

	pI128->mem.rbase_a[XY2_WH] = (w<<16) | h;
	pI128->mem.rbase_a[XY0_SRC] = (x1<<16) | y1;			MB;
	pI128->mem.rbase_a[XY1_DST] = (x2<<16) | y2;			MB;
}

void
I128SetupForScreenToScreenCopy(ScrnInfoPtr pScrn, int xdir, int ydir,
	int rop, unsigned planemask, int transparency_color)
{
	I128Ptr pI128 = I128PTR(pScrn);

#if 0
	if (pI128->Debug)
		xf86DrvMsg(pScrn->scrnIndex, X_INFO, "SFSSC %d,%d %d 0x%x %d\n", xdir, ydir, rop, planemask, transparency_color);
#endif

	ENG_PIPELINE_READY();

	if (planemask == -1)
		pI128->mem.rbase_a[MASK] = -1;
	else switch (pI128->bitsPerPixel) {
		case 8:
			pI128->mem.rbase_a[MASK] = planemask |
					(planemask<<8) |
					(planemask<<16) |
					(planemask<<24);
			break;
		case 16:
			pI128->mem.rbase_a[MASK] = planemask | (planemask<<16);
			break;
		case 24:
		case 32:
		default:
			pI128->mem.rbase_a[MASK] = planemask;
			break;
	}


	pI128->mem.rbase_a[CLPTL] = 0x00000000;
	pI128->mem.rbase_a[CLPBR] = (4095<<16) | 2047;

	if (transparency_color != -1)
		pI128->mem.rbase_a[BACK] = transparency_color;


	if (xdir == -1) {
		if (ydir == -1) pI128->blitdir = DIR_RL_BT;
		else            pI128->blitdir = DIR_RL_TB;
	} else {
		if (ydir == -1) pI128->blitdir = DIR_LR_BT;
		else            pI128->blitdir = DIR_LR_TB;
	}
	pI128->mem.rbase_a[XY3_DIR] = pI128->blitdir;

	pI128->rop = i128alu[rop];
	pI128->cmd = (transparency_color != -1 ? (CS_TRNSP<<16) : 0) |
		  pI128->rop | CO_BITBLT;
	pI128->mem.rbase_a[CMD] = pI128->cmd;
}

void
I128SubsequentScreenToScreenCopy(ScrnInfoPtr pScrn, int x1, int y1,
	int x2, int y2, int w, int h)
{
	I128BitBlit(pScrn, x1, y1, x2, y2, w, h);
}

void
I128SetupForSolidFill(ScrnInfoPtr pScrn, int color, int rop, unsigned planemask)
{
	I128Ptr pI128 = I128PTR(pScrn);

#if 0
	if (pI128->Debug)
		xf86DrvMsg(pScrn->scrnIndex, X_INFO, "SFSF color 0x%x rop 0x%x (I128rop 0x%x) pmask 0x%x\n", color, rop, i128alu[rop]>>8, planemask);
#endif

	ENG_PIPELINE_READY();

	if (planemask == -1)
		pI128->mem.rbase_a[MASK] = -1;
	else switch (pI128->bitsPerPixel) {
		case 8:
			pI128->mem.rbase_a[MASK] = planemask |
					(planemask<<8) |
					(planemask<<16) |
					(planemask<<24);
			break;
		case 16:
			pI128->mem.rbase_a[MASK] = planemask | (planemask<<16);
			break;
		case 24:
		case 32:
		default:
			pI128->mem.rbase_a[MASK] = planemask;
			break;
	}

	pI128->mem.rbase_a[FORE] = color;

	pI128->clptl = pI128->mem.rbase_a[CLPTL] = 0x00000000;
	pI128->clpbr = pI128->mem.rbase_a[CLPBR] = (4095<<16) | 2047 ;

	pI128->mem.rbase_a[XY3_DIR] = pI128->blitdir = DIR_LR_TB;

	pI128->rop = i128alu[rop];
	pI128->cmd = (CS_SOLID<<16) | pI128->rop | CO_BITBLT;
	pI128->mem.rbase_a[CMD] = pI128->cmd;
}

void
I128SubsequentSolidFillRect(ScrnInfoPtr pScrn, int x, int y, int w, int h)
{
#if 0
	I128Ptr pI128 = I128PTR(pScrn);

	if (pI128->Debug)
		xf86DrvMsg(pScrn->scrnIndex, X_INFO, "SSFR %d,%d %d,%d\n", x, y, w, h);
#endif
	I128BitBlit(pScrn, 0, 0, x, y, w, h);
}

void
I128SubsequentSolidTwoPointLine(ScrnInfoPtr pScrn, int x1, int y1, int x2,
	int y2, int flags)
{
	I128Ptr pI128 = I128PTR(pScrn);

#if 0
	if (pI128->Debug)
		xf86DrvMsg(pScrn->scrnIndex, X_INFO, "STPL I128rop 0x%x  %d,%d %d,%d   clip %d,%d %d,%d\n", pI128->rop, x1, y1, x2, y2, pI128->clptl>>16, pI128->clptl&0xffff, (pI128->clpbr>>16)&0xffff, pI128->clpbr&0xffff);
#endif

	ENG_PIPELINE_READY();

	pI128->mem.rbase_a[CMD] =
			((flags&0x0100) ? (CP_NLST<<24) : 0) |
			(CC_CLPRECI<<21) |
			(CS_SOLID<<16) |
			pI128->rop |
			CO_LINE;

	pI128->mem.rbase_a[CLPTL] = pI128->clptl;
	pI128->mem.rbase_a[CLPBR] = pI128->clpbr;

	pI128->mem.rbase_a[XY0_SRC] = (x1<<16) | y1;			MB;
	pI128->mem.rbase_a[XY1_DST] = (x2<<16) | y2;			MB;
}

void
I128SetClippingRectangle(ScrnInfoPtr pScrn, int x1, int y1, int x2, int y2)
{
	I128Ptr pI128 = I128PTR(pScrn);
	int tmp;

#if 0
	if (pI128->Debug)
		xf86DrvMsg(pScrn->scrnIndex, X_INFO, "SCR  %d,%d %d,%d\n", x1, y1, x2, y2);
#endif

	if (x1 > x2) { tmp = x2; x2 = x1; x1 = tmp; }
	if (y1 > y2) { tmp = y2; y2 = y1; y1 = tmp; }

	pI128->clptl = (x1<<16) | y1;
	pI128->clpbr = (x2<<16) | y2;
}


void
I128FillSolidRects(ScrnInfoPtr pScrn, int fg, int rop, unsigned int planemask,
	int nBox, register BoxPtr pBoxI)
{
	I128Ptr pI128 = I128PTR(pScrn);
	register int w, h;

#if 0
	if (pI128->Debug)
		xf86DrvMsg(pScrn->scrnIndex, X_INFO, "FSR color 0x%x rop 0x%x (I128rop 0x%x) pmask 0x%x\n", fg, rop, i128alu[rop]>>8, planemask);
#endif

	ENG_PIPELINE_READY();

	if (planemask != -1) {
		if (pI128->bitsPerPixel == 8) {
			planemask |= (planemask<<8)  |
				     (planemask<<16) |
				     (planemask<<24);
		} else if (pI128->bitsPerPixel == 16)
			planemask |= planemask<<16;
	}

	pI128->mem.rbase_a[MASK] = planemask;
	pI128->mem.rbase_a[FORE] = fg;
	pI128->mem.rbase_a[CMD] = (CS_SOLID<<16) | i128alu[rop] | CO_BITBLT;
	pI128->mem.rbase_a[CLPTL] = 0x00000000;
	pI128->mem.rbase_a[CLPBR] = (4095<<16) | 2047;

	pI128->mem.rbase_a[XY3_DIR] = DIR_LR_TB;
	pI128->mem.rbase_a[XY0_SRC] = 0x00000000;

	while (nBox > 0) {
		w = pBoxI->x2 - pBoxI->x1;
		h = pBoxI->y2 - pBoxI->y1;
		if (w > 0 && h > 0) {
			pI128->mem.rbase_a[XY2_WH] = (w<<16) | h;	MB;
			pI128->mem.rbase_a[XY1_DST] =
				(pBoxI->x1<<16) | pBoxI->y1;		MB;

			ENG_PIPELINE_READY();
#if 0
			if (pI128->Debug)
				xf86DrvMsg(pScrn->scrnIndex, X_INFO, "FSR x,y %d,%d w,h %d,%d\n", pBoxI->x1, pBoxI->y1, w, h);
#endif

		}
		pBoxI++;
		nBox--;
	}

	ENG_DONE();

}


void
I128ScreenToScreenBitBlt(ScrnInfoPtr pScrn, int nbox, DDXPointPtr pptSrc,
	BoxPtr pbox, int xdir, int ydir, int alu, unsigned planemask)
{
	I128Ptr pI128 = I128PTR(pScrn);
        I128SetupForScreenToScreenCopy(pScrn, xdir, ydir, alu, planemask, -1);
        for (; nbox; pbox++, pptSrc++, nbox--)
            I128SubsequentScreenToScreenCopy(pScrn, pptSrc->x, pptSrc->y,
                pbox->x1, pbox->y1, pbox->x2 - pbox->x1, pbox->y2 - pbox->y1);
	ENG_DONE();
}


Bool
I128AccelInit(ScreenPtr pScreen)
{
	XAAInfoRecPtr infoPtr;
	ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
	I128Ptr pI128 = I128PTR(pScrn);
	BoxRec AvailFBArea;
	CARD32 buf_ctrl;
	int maxlines;

	pI128->AccelInfoRec = infoPtr = XAACreateInfoRec();
	if (!infoPtr) return FALSE;

	infoPtr->Flags = 	PIXMAP_CACHE |
				OFFSCREEN_PIXMAPS |
				LINEAR_FRAMEBUFFER ;

	infoPtr->Sync = I128EngineDone;

	/* screen to screen copy */
	infoPtr->ScreenToScreenCopyFlags = NO_TRANSPARENCY;
	if (pI128->Chipset == PCI_CHIP_I128_T2R)
		infoPtr->ScreenToScreenCopyFlags |= ONLY_LEFT_TO_RIGHT_BITBLT;

	infoPtr->SetupForScreenToScreenCopy = I128SetupForScreenToScreenCopy;
	infoPtr->SubsequentScreenToScreenCopy =
					    I128SubsequentScreenToScreenCopy;

#if 0
	/* screen to screen color expansion */
	if (pI128->Chipset == PCI_CHIP_I128_T2R)
		infoPtr->ScreenToScreenColorExpandFillFlags |=
			ONLY_LEFT_TO_RIGHT_BITBLT;

	infoPtr->SetupForScreenToScreenColorExpandFill =
		I128SetupForScreenToScreenColorExpandFill;
	infoPtr->SubsequentScreenToScreenColorExpandFill =
		I128SubsequentScreenToScreenColorExpandFill;
#endif

	/* solid fills */
	infoPtr->SetupForSolidFill = I128SetupForSolidFill;
	infoPtr->SubsequentSolidFillRect = I128SubsequentSolidFillRect;

	infoPtr->FillSolidRects = I128FillSolidRects;

	/* solid lines */
	infoPtr->SubsequentSolidTwoPointLine = I128SubsequentSolidTwoPointLine;

	/* clipping */
	infoPtr->ClippingFlags = HARDWARE_CLIP_LINE;
	infoPtr->SetClippingRectangle = I128SetClippingRectangle;

	infoPtr->PolyFillRectSolidFlags = 0;

	maxlines = ((pI128->MemorySize * 1024) - 1024) /
			  (pScrn->displayWidth * pI128->bitsPerPixel / 8);
	AvailFBArea.x1 = 0;
	AvailFBArea.x2 = pI128->displayWidth;
	AvailFBArea.y1 = 0;
	AvailFBArea.y2 = maxlines;
	xf86InitFBManager(pScreen, &AvailFBArea);
	xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		"Using %d lines for offscreen memory\n",
		maxlines - pScrn->virtualY);

	switch (pI128->bitsPerPixel) {
		case 8:  buf_ctrl = BC_PSIZ_8B;  break;
		case 16: buf_ctrl = BC_PSIZ_16B; break;
		case 24:
		case 32: buf_ctrl = BC_PSIZ_32B; break;
		default: buf_ctrl = 0;           break; /* error */
	}
	if (pI128->Chipset == PCI_CHIP_I128_T2R) {
		if (pI128->MemoryType == I128_MEMORY_SGRAM)
			buf_ctrl |= BC_MDM_PLN;
		else
			buf_ctrl |= BC_BLK_ENA;
	}
	pI128->mem.rbase_a[BUF_CTRL] = buf_ctrl;

	pI128->mem.rbase_a[DE_PGE] = 0x00;
	pI128->mem.rbase_a[DE_SORG] = pI128->displayOffset;
	pI128->mem.rbase_a[DE_DORG] = pI128->displayOffset;
	pI128->mem.rbase_a[DE_MSRC] = 0x00;
	pI128->mem.rbase_a[DE_WKEY] = 0x00;
	pI128->mem.rbase_a[DE_SPTCH] = pI128->mem.rbase_g[DB_PTCH];
	pI128->mem.rbase_a[DE_DPTCH] = pI128->mem.rbase_g[DB_PTCH];
	if (pI128->Chipset == PCI_CHIP_I128_T2R4)
		pI128->mem.rbase_a[DE_ZPTCH] = pI128->mem.rbase_g[DB_PTCH];
	pI128->mem.rbase_a[RMSK] = 0x00000000;
	pI128->mem.rbase_a[XY4_ZM] = ZOOM_NONE;
	pI128->mem.rbase_a[LPAT] = 0xffffffff;  /* for lines */
	pI128->mem.rbase_a[PCTRL] = 0x00000000; /* for lines */
	pI128->mem.rbase_a[CLPTL] = 0x00000000;
	pI128->mem.rbase_a[CLPBR] = (4095<<16) | 2047 ;
	pI128->mem.rbase_a[ACNTRL] = 0x00000000;
	pI128->mem.rbase_a[INTM] = 0x03;

	if (pI128->Debug) {
		xf86DrvMsg(pScrn->scrnIndex, X_INFO, "I128AccelInit done\n");
		I128DumpActiveRegisters(pScrn);
	}

	return(XAAInit(pScreen, infoPtr));
}

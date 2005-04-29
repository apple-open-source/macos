/*
 *      Copyright 2001  Ani Joshi <ajoshi@unixbox.com>
 * 
 *      XFree86 4.x driver for S3 chipsets
 * 
 * 
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that copyright
 * notice and this permission notice appear in supporting documentation and
 * that the name of Ani Joshi not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  Ani Joshi makes no representations
 * about the suitability of this software for any purpose.  It is provided
 * "as-is" without express or implied warranty.
 *                 
 * ANI JOSHI DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL ANI JOSHI BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 *
 *
 */
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/s3/s3_accel.c,v 1.3 2003/11/23 18:07:31 twini Exp $ */


#include "xf86.h"

#include "miline.h"

#include "s3.h"
#include "s3_reg.h"


#if 0
static Bool NicePattern;
static int DashPatternSize;
#define MAX_LINE_PATTERN_LENGTH	512
#define LINE_PATTERN_START	((MAX_LINE_PATTERN_LENGTH >> 5) - 1)
static CARD32 DashPattern[MAX_LINE_PATTERN_LENGTH >> 5];
#endif


static void S3Sync(ScrnInfoPtr pScrn)
{
	WaitIdle();
}

static void S3SetupForSolidFill(ScrnInfoPtr pScrn, int color, int rop,
				unsigned int planemask)
{
	S3Ptr pS3 = S3PTR(pScrn);

	WaitQueue16_32(4,6);
	SET_PIX_CNTL(0);
	SET_FRGD_COLOR(color);
	SET_FRGD_MIX(FSS_FRGDCOL | s3alu[rop]);
	SET_WRT_MASK(planemask);
}

static void S3SubsequentSolidFillRect(ScrnInfoPtr pScrn, int x, int y,
				      int w, int h)
{
#ifdef S3_NEWMMIO
	S3Ptr pS3 = S3PTR(pScrn);
#endif

	WaitQueue(5);
	SET_CURPT((short)x, (short)y);
	SET_AXIS_PCNT(w - 1, h - 1);
	SET_CMD(CMD_RECT | DRAW | INC_X | INC_Y | WRTDATA);
}


static void S3SetupForScreenToScreenCopy(ScrnInfoPtr pScrn, int xdir,
					 int ydir, int rop,
					 unsigned int planemask,
					 int trans_color)
{
	S3Ptr pS3 = S3PTR(pScrn);

	pS3->BltDir = CMD_BITBLT | DRAW | WRTDATA;

	if (xdir == 1)
		pS3->BltDir |= INC_X;
	if (ydir == 1)
		pS3->BltDir |= INC_Y;

	pS3->trans_color = trans_color;

	WaitQueue16_32(3,4);
	SET_PIX_CNTL(0);
	SET_FRGD_MIX(FSS_BITBLT | s3alu[rop]);
	SET_WRT_MASK(planemask);
}


static void S3SubsequentScreenToScreenCopy(ScrnInfoPtr pScrn,
					   int srcx, int srcy,
					   int dstx, int dsty,
					   int w, int h)
{
	S3Ptr pS3 = S3PTR(pScrn);

	w--;
	h--;

	if (!(pS3->BltDir & INC_Y)) {
		srcy += h;
		dsty += h;
	}

	if (!(pS3->BltDir & INC_X)) {
		srcx += w;
		dstx += w;
	}

	if (pS3->trans_color == -1) {
		WaitQueue(7);
		SET_CURPT((short)srcx, (short)srcy);
		SET_DESTSTP((short)dstx, (short)dsty);
		SET_AXIS_PCNT((short)w, (short)h);
		SET_CMD(pS3->BltDir);
	} else {
		WaitQueue16_32(2,3);
		SET_MULT_MISC(CMD_REG_WIDTH | 0x0100);
		SET_COLOR_CMP(pS3->trans_color);

		WaitQueue(8);
		SET_CURPT((short)srcx, (short)srcy);
		SET_DESTSTP((short)dstx, (short)dsty);
		SET_AXIS_PCNT((short)w, (short)h);
		SET_CMD(pS3->BltDir);
		SET_MULT_MISC(CMD_REG_WIDTH);
	}
}


static void S3SetupForColor8x8PatternFill(ScrnInfoPtr pScrn,
					  int patx, int paty,
					  int rop, unsigned int planemask,
					  int trans_color)
{
	S3Ptr pS3 = S3PTR(pScrn);

	pS3->trans_color = trans_color;

	WaitQueue16_32(3,4);
	SET_PIX_CNTL(0);
	SET_FRGD_MIX(FSS_BITBLT | s3alu[rop]);
	SET_WRT_MASK(planemask);
}

static void S3SubsequentColor8x8PatternFillRect(ScrnInfoPtr pScrn,
					    	int patx, int paty,
					    	int x, int y,
					    	int w, int h)
{
	S3Ptr pS3 = S3PTR(pScrn);

	if (pS3->trans_color == -1) {
		WaitQueue(7);
		SET_CURPT((short)patx, (short)paty);
		SET_DESTSTP((short)x, (short)y);
		SET_AXIS_PCNT(w - 1, h - 1);
		SET_CMD(CMD_PFILL | DRAW | INC_Y | INC_X | WRTDATA);
	} else {
		WaitQueue16_32(2,3);
		SET_MULT_MISC(CMD_REG_WIDTH | 0x0100);
		SET_COLOR_CMP(pS3->trans_color);

		WaitQueue(8);
		SET_CURPT((short)patx, (short)paty);
		SET_DESTSTP((short)x, (short)y);
		SET_AXIS_PCNT(w - 1, h - 1);
		SET_CMD(CMD_PFILL | DRAW | INC_Y | INC_X | WRTDATA);
		SET_MULT_MISC(CMD_REG_WIDTH);
	}
}

#ifdef S3_NEWMMIO
static void S3SetupForCPUToScreenColorExpandFill(ScrnInfoPtr pScrn,
						 int fg, int bg,
						 int rop,
						 unsigned int planemask)
{
	S3Ptr pS3 = S3PTR(pScrn);

	WaitQueue16_32(3,4);
	if (bg == -1) {
		if (pS3->ColorExpandBug) {
			SET_MIX(FSS_FRGDCOL | s3alu[rop], BSS_BKGDCOL | MIX_XOR);
			SET_BKGD_COLOR(0);
		} else
			SET_MIX(FSS_FRGDCOL | s3alu[rop], BSS_BKGDCOL | MIX_DST);
	} else {
		SET_MIX(FSS_FRGDCOL | s3alu[rop], BSS_BKGDCOL | s3alu[rop]);
		SET_BKGD_COLOR(bg);
	}

	WaitQueue16_32(3,5);
	SET_FRGD_COLOR(fg);
	SET_WRT_MASK(planemask);
	SET_PIX_CNTL(MIXSEL_EXPPC);
}


static void S3SubsequentCPUToScreenColorExpandFill32(ScrnInfoPtr pScrn,
						     int x, int y,
						     int w, int h,
						     int skipleft)
{
	S3Ptr pS3 = S3PTR(pScrn);

	WaitQueue(4);
	SET_CURPT((short)x, (short)y);
	SET_AXIS_PCNT((short)w-1, (short)h-1);

	WaitIdle();
	SET_CMD(CMD_RECT | BYTSEQ | _32BIT | PCDATA | DRAW |
		PLANAR | INC_Y | INC_X | WRTDATA);
}
#endif

#if 0
#ifndef S3_NEWMMIO

static void S3SetupForScanlineImageWriteNoMMIO(ScrnInfoPtr pScrn, int rop,
					       unsigned int planemask,
					       int trans_color,
					       int bpp, int depth)
{
	S3Ptr pS3 = S3PTR(pScrn);

	WaitQueue16_32(3,4)
	SET_FRGD_MIX(FSS_PCDATA | s3alu[rop]);
	SET_WRT_MASK(planemask);
	SET_PIX_CNTL(0);
}

static void S3SubsequentScanlineImageWriteRectNoMMIO(ScrnInfoPtr pScrn,
						     int x, int y,
						     int w, int h,
						     int skipleft)
{
	S3Ptr pS3 = S3PTR(pScrn);

	pS3->imageWidth = w;
	pS3->imageHeight = h;

	WaitQueue(5);
	SET_CURPT((short)x, (short)y);
	SET_AXIS_PCNT((short)w-1, (short)h-1);
	WaitIdle();
	SET_CMD(CMD_RECT | BYTSEQ | _16BIT | INC_Y | INC_X | DRAW |
		PCDATA | WRTDATA);
}


static void S3SubsequentImageWriteScanlineNoMMIO(ScrnInfoPtr pScrn,
						 int bufno)
{
	S3Ptr pS3 = S3PTR(pScrn);
	int i, j;
	int w, h;
	CARD16 *src = (CARD16 *)&pS3->imageBuffer;

	w = pS3->imageWidth * pS3->s3Bpp;
	h = pS3->imageHeight;

	for(j=0; j<h; j++) {
		for(i=0; i<(w & ~1); ) {
			/* XXX not for 32bpp */
			SET_PIX_TRANS_W(ldw_u(src));
			src++;
			i += 2;
		}
	}
}	

#endif
#endif


static void S3SetupForSolidLine(ScrnInfoPtr pScrn, int color, int rop,
				unsigned int planemask)
{
	S3Ptr pS3 = S3PTR(pScrn);

	WaitQueue16_32(4,6);
	SET_PIX_CNTL(0);
	SET_FRGD_COLOR(color);
	SET_FRGD_MIX(FSS_FRGDCOL | s3alu[rop]);
	SET_WRT_MASK(planemask);
}


static void S3SubsequentSolidBresenhamLine(ScrnInfoPtr pScrn,
					   int x, int y,
					   int major, int minor,
					   int err, int len, int octant)
{
#ifdef S3_NEWMMIO
	S3Ptr pS3 = S3PTR(pScrn);
#endif
	unsigned short cmd;
	int error, e1, e2;

	error = minor + err;
	e1 = minor;   /* was: major, wrong (twini@xfree86.org) */
	e2 = minor - major;

	if (major) {
		cmd = CMD_LINE | DRAW | WRTDATA | LASTPIX;

		if (octant & YMAJOR)
			cmd |= YMAJAXIS;
		if (!(octant & XDECREASING))
			cmd |= INC_X;
		if (!(octant & YDECREASING))
			cmd |= INC_Y;

		WaitQueue(7);
		SET_CURPT((short)x, (short)y);
		SET_ERR_TERM((short)error);
		SET_DESTSTP((short)e2, (short)e1);
		SET_MAJ_AXIS_PCNT((short)len);
		SET_CMD(cmd);
	} else {
		WaitQueue(4);
		SET_CURPT((short)x, (short)y);
		SET_MAJ_AXIS_PCNT((short)len-1);
		SET_CMD(CMD_LINE | DRAW | LINETYPE | WRTDATA | VECDIR_270);
	}
}



static void S3SubsequentSolidHorVertLine(ScrnInfoPtr pScrn,
					 int x, int y, int len, int dir)
{
	if (dir == DEGREES_0)
		S3SubsequentSolidFillRect(pScrn, x, y, len, 1);
	else
		S3SubsequentSolidFillRect(pScrn, x, y, 1, len);
}


#if 0

static void S3SetupForDashedLine(ScrnInfoPtr pScrn,
                                 int fg, int bg,
                                 int rop, unsigned int planemask,
                                 int len, unsigned char *pattern)
{
#ifdef S3_NEWMMIO
	S3Ptr pS3 = S3PTR(pScrn);

	S3SetupForCPUToScreenColorExpandFill(pScrn, bg, fg, rop, planemask);
#endif

	WaitQueue(4);

	NicePattern = FALSE;

	if (len <= 32) {
		register CARD32 scratch = DashPattern[LINE_PATTERN_START];

		if (len & (len - 1)) {
			while (len < 16) {
				scratch |= (scratch >> len);
				len <<= 1;
			}
			scratch |= (scratch >> len);
			DashPattern[LINE_PATTERN_START] = scratch;
		} else {
			switch (len) {
				case 2:
					scratch |= scratch >> 2;
				case 4:
					scratch |= scratch >> 4;
				case 8:
					scratch |= scratch >> 8;
				case 16:
					scratch |= scratch >> 16;
					DashPattern[LINE_PATTERN_START] = scratch;
				case 32:
					NicePattern = TRUE;
				default:
					break;
			}
		}
	}

	DashPatternSize = len;
}


static void S3SubsequentDashedBresenhamLine32(ScrnInfoPtr pScrn,
					      int x, int y,
					      int absmaj, int absmin,
					      int err, int len,
					      int octant, int phase)
{
#ifdef S3_NEWMMIO
	S3Ptr pS3 = S3PTR(pScrn);
#endif
	register int count = (len + 31) >> 5;
	register CARD32 pattern;
	int error, e1, e2;

	error = absmin + err;
	e1 = absmaj;
	e2 = absmin - absmaj;

	if (err) {
		unsigned short cmd = _32BIT | PLANAR | WRTDATA | DRAW |
				     PCDATA | LASTPIX | CMD_LINE;

		if (octant & YMAJOR)
			cmd |= YMAJAXIS;
		if (!(octant & XDECREASING))
			cmd |= INC_X;
		if (!(octant & YDECREASING))
			cmd |= INC_Y;

		WaitQueue(7);
		SET_CURPT((short)x, (short)y);
		SET_ERR_TERM((short)error);
		SET_DESTSTP((short)e2, (short)e1);
		SET_MAJ_AXIS_PCNT((short)len);
		SET_CMD(cmd);
	} else {
		if (octant & YMAJOR) {
			WaitQueue(4);
			SET_CURPT((short)x, (short)y);
			SET_MAJ_AXIS_PCNT((short)len - 1);

			if (octant & YDECREASING) {
				SET_CMD(_32BIT | PLANAR | WRTDATA | DRAW |
					CMD_LINE | LINETYPE | VECDIR_090);
			} else {
				SET_CMD(_32BIT | PLANAR | WRTDATA | DRAW |
					CMD_LINE | LINETYPE | VECDIR_270);
			}
		} else {
			if (octant & XDECREASING) {
				WaitQueue(4);
				SET_CURPT((short)x, (short)y);
				SET_MAJ_AXIS_PCNT((short)len - 1);
				SET_CMD(_32BIT | PLANAR | WRTDATA | DRAW |
					PCDATA | CMD_LINE | LINETYPE | VECDIR_180);
			} else {
				WaitQueue(4);
				SET_CURPT((short)x, (short)y);
				SET_MAJ_AXIS_PCNT((short)len - 1);
				SET_CMD(_32BIT | PLANAR | WRTDATA | DRAW |
					PCDATA | CMD_RECT | INC_Y | INC_X);
			}
		}
	}

	if (NicePattern) {
#ifdef S3_NEWMMIO
		register CARD32 *dest = (CARD32*)&IMG_TRANS;
#endif

		pattern = (phase) ? (DashPattern[LINE_PATTERN_START] << phase) |
			(DashPattern[LINE_PATTERN_START] >> (32 - phase)) :
			 DashPattern[LINE_PATTERN_START];

#ifdef S3_NEWMMIO
		while (count & ~0x03) {
			dest[0] = dest[1] = dest[2] = dest[3] = pattern;
			dest += 4;
			count -= 4;
		}
		switch (count) {
		case 1:
			dest[0] = pattern;
			break;
		case 2:
			dest[0] = dest[1] = pattern;
			break;
		case 3:
			dest[0] = dest[1] = dest[2] = pattern;
			break;
		}
#else

		while (count--)
			SET_PIX_TRANS_L(pattern);
#endif
	} else if (DashPatternSize < 32) {
		register int offset = phase;

		while (count--) {
			SET_PIX_TRANS_L((DashPattern[LINE_PATTERN_START] << offset) |
				(DashPattern[LINE_PATTERN_START] >>
				(DashPatternSize - offset)));
			offset += 32;
			while (offset > DashPatternSize)
				offset -= DashPatternSize;
		}
	} else {
		int offset = phase;
		register unsigned char *srcp = (unsigned char *)(DashPattern) +
			(MAX_LINE_PATTERN_LENGTH >> 3) - 1;
		register CARD32 *scratch;
		int scratch2, shift;

		while (count--) {
			shift = DashPatternSize - offset;
			scratch = (CARD32*)(srcp - (offset >> 3) - 3);
			scratch2 = offset & 0x07;

			if (shift & ~31) {
				if (scratch2) {
					pattern = (*scratch << scratch2) |
						  (*(scratch - 1) >> (32 - scratch2));
				} else
					pattern = *scratch;
			} else {
				pattern = (*((CARD32*)(srcp - 3)) >> shift) |
					  (*scratch << scratch2);
			}
			SET_PIX_TRANS_L(pattern);
			offset += 32;
			while (offset >= DashPatternSize)
				offset -= DashPatternSize;
		}
	}
}

#endif
 	
#ifdef S3_NEWMMIO
Bool S3AccelInitNewMMIO(ScreenPtr pScreen)
#else
Bool S3AccelInitPIO(ScreenPtr pScreen)
#endif
{
	ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
	S3Ptr pS3 = S3PTR(pScrn);
	XAAInfoRecPtr pXAA;

	if (pS3->Chipset == PCI_CHIP_968)
		pS3->ColorExpandBug = TRUE;
	else
		pS3->ColorExpandBug = FALSE;

	if (!(pXAA = XAACreateInfoRec()))
		return FALSE;

	pS3->pXAA = pXAA;

	pXAA->Flags = (PIXMAP_CACHE | OFFSCREEN_PIXMAPS |
		       LINEAR_FRAMEBUFFER);

	pXAA->Sync = S3Sync;

	pXAA->SetupForSolidFill = S3SetupForSolidFill;
	pXAA->SubsequentSolidFillRect = S3SubsequentSolidFillRect;

	pXAA->SetupForScreenToScreenCopy = S3SetupForScreenToScreenCopy;
	pXAA->SubsequentScreenToScreenCopy = S3SubsequentScreenToScreenCopy;

	pXAA->SetupForColor8x8PatternFill = S3SetupForColor8x8PatternFill;
	pXAA->SubsequentColor8x8PatternFillRect = S3SubsequentColor8x8PatternFillRect;

#ifdef S3_NEWMMIO
	pXAA->SetupForCPUToScreenColorExpandFill =
		S3SetupForCPUToScreenColorExpandFill;
	pXAA->SubsequentCPUToScreenColorExpandFill =
		S3SubsequentCPUToScreenColorExpandFill32;
	pXAA->ColorExpandBase = (void *) &IMG_TRANS;
	pXAA->ColorExpandRange = 0x8000;
	pXAA->CPUToScreenColorExpandFillFlags = CPU_TRANSFER_PAD_DWORD |
						BIT_ORDER_IN_BYTE_MSBFIRST |
						SCANLINE_PAD_DWORD;
#endif

#if 0
#ifndef S3_NEWMMIO
	pXAA->ScanlineImageWriteFlags = NO_TRANSPARENCY;
	pXAA->SetupForScanlineImageWrite =
		S3SetupForScanlineImageWriteNoMMIO;
	pXAA->SubsequentScanlineImageWriteRect =
		S3SubsequentScanlineImageWriteRectNoMMIO;
	pXAA->SubsequentImageWriteScanline =
		S3SubsequentImageWriteScanlineNoMMIO;
	pXAA->NumScanlineImageWriteBuffers = 1;
	pXAA->ScanlineImageWriteBuffers = &pS3->imageBuffer;
#endif
#endif

	pXAA->SetupForSolidLine = S3SetupForSolidLine;
	pXAA->SubsequentSolidBresenhamLine = S3SubsequentSolidBresenhamLine;
	pXAA->SubsequentSolidHorVertLine = S3SubsequentSolidHorVertLine;
	pXAA->SolidBresenhamLineErrorTermBits = 12;
#if 0
	/* kinda buggy...  and its faster without it */
	pXAA->SetupForDashedLine = S3SetupForDashedLine;
	pXAA->SubsequentDashedBresenhamLine = S3SubsequentDashedBresenhamLine32;
	pXAA->DashPatternMaxLength = MAX_LINE_PATTERN_LENGTH;
#endif

	return XAAInit(pScreen, pXAA);
}

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
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/s3/s3_IBMRGB.c,v 1.5 2003/02/17 16:45:24 dawes Exp $ */


#include "xf86.h"
#include "xf86_OSproc.h"
#include "xf86_ansic.h"

#include "compiler.h"

#include "IBM.h"

#include "s3.h"


#define IBMRGB_WRITE_ADDR           0x3C8   /* CR55 low bit == 0 */
#define IBMRGB_RAMDAC_DATA          0x3C9   /* CR55 low bit == 0 */
#define IBMRGB_PIXEL_MASK           0x3C6   /* CR55 low bit == 0 */
#define IBMRGB_READ_ADDR            0x3C7   /* CR55 low bit == 0 */
#define IBMRGB_INDEX_LOW            0x3C8   /* CR55 low bit == 1 */
#define IBMRGB_INDEX_HIGH           0x3C9   /* CR55 low bit == 1 */
#define IBMRGB_INDEX_DATA           0x3C6   /* CR55 low bit == 1 */
#define IBMRGB_INDEX_CONTROL        0x3C7   /* CR55 low bit == 1 */


static void S3OutIBMRGBIndReg(ScrnInfoPtr pScrn, CARD32 reg,
		       unsigned char mask, unsigned char data)
{
	S3Ptr pS3 = S3PTR(pScrn);
	unsigned char tmp, tmp2 = 0x00;
	int vgaCRIndex = pS3->vgaCRIndex, vgaCRReg = pS3->vgaCRReg;

	outb(vgaCRIndex, 0x55);
	tmp = inb(vgaCRReg) & 0xfc;
	outb(vgaCRReg, tmp | 0x01);

	outb(IBMRGB_INDEX_LOW, reg);

	if (mask != 0x00)
		tmp2 = inb(IBMRGB_INDEX_DATA) & mask;
	outb(IBMRGB_INDEX_DATA, tmp2 | data);

	outb(vgaCRIndex, 0x55);
	outb(vgaCRReg, tmp);
}


static unsigned char S3InIBMRGBIndReg(ScrnInfoPtr pScrn, CARD32 reg)
{
	S3Ptr pS3 = S3PTR(pScrn);
	unsigned char tmp, ret;
	int vgaCRIndex = pS3->vgaCRIndex, vgaCRReg = pS3->vgaCRReg;

	outb(vgaCRIndex, 0x55);
	tmp = inb(vgaCRReg) & 0xfc;
	outb(vgaCRReg, tmp | 0x01);

	outb(IBMRGB_INDEX_LOW, reg);
	ret = inb(IBMRGB_INDEX_DATA);

	outb(vgaCRIndex, 0x55);
	outb(vgaCRReg, tmp);

	return ret;
}


static void S3IBMWriteAddress(ScrnInfoPtr pScrn, CARD32 index)
{
	outb(IBMRGB_WRITE_ADDR, index);
}

static void S3IBMWriteData(ScrnInfoPtr pScrn, unsigned char data)
{
	outb(IBMRGB_INDEX_DATA, data);
}

static void S3IBMReadAddress(ScrnInfoPtr pScrn, CARD32 index)
{
	outb(IBMRGB_READ_ADDR, index);
}

static unsigned char S3IBMReadData(ScrnInfoPtr pScrn)
{
	return inb(IBMRGB_RAMDAC_DATA);
}


Bool S3ProbeIBMramdac(ScrnInfoPtr pScrn)
{
	S3Ptr pS3 = S3PTR(pScrn);

	if (pS3->Chipset != PCI_CHIP_968)
		return FALSE;

	pS3->RamDacRec = RamDacCreateInfoRec();
	pS3->RamDacRec->ReadDAC = S3InIBMRGBIndReg;
	pS3->RamDacRec->WriteDAC = S3OutIBMRGBIndReg;
	pS3->RamDacRec->ReadAddress = S3IBMReadAddress;
	pS3->RamDacRec->WriteAddress = S3IBMWriteAddress;
	pS3->RamDacRec->ReadData = S3IBMReadData;
	pS3->RamDacRec->WriteData = S3IBMWriteData;
	pS3->RamDacRec->LoadPalette = NULL;

	if (!RamDacInit(pScrn, pS3->RamDacRec)) {
		RamDacDestroyInfoRec(pS3->RamDacRec);
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "RamDacInit failed\n");
		return FALSE;
	}

	pS3->RamDac = IBMramdacProbe(pScrn, S3IBMRamdacs);
	if (pS3->RamDac)
		return TRUE;

	return FALSE;
}

static void S3ProgramIBMRGBClock(ScrnInfoPtr pScrn, int clk, unsigned char m,
			  unsigned char n, unsigned char df)
{
	S3OutIBMRGBIndReg(pScrn, IBMRGB_misc_clock, ~1, 1);

	S3OutIBMRGBIndReg(pScrn, IBMRGB_m0+2*clk, 0, (df<<6)|(m&0x3f));
	S3OutIBMRGBIndReg(pScrn, IBMRGB_n0+2*clk, 0, n);

	S3OutIBMRGBIndReg(pScrn, IBMRGB_pll_ctrl2, 0xf0, clk);
	S3OutIBMRGBIndReg(pScrn, IBMRGB_pll_ctrl1, 0xf8, 3);
}


static void S3IBMRGBSetClock(ScrnInfoPtr pScrn, long freq, int clk, long dacspeed,
		      long fref)
{
	volatile double ffreq, fdacspeed, ffref;
	volatile int df, n, m, max_n, min_df;
	volatile int best_m=69, best_n=17, best_df=0;
	volatile double diff, mindiff;

#define FREQ_MIN	16250
#define FREQ_MAX	dacspeed

	if (freq < FREQ_MIN)
		ffreq = FREQ_MIN / 1000.0;
	else if (freq > FREQ_MAX)
		ffreq = FREQ_MAX / 1000.0;
	else
		ffreq = freq / 1000.0;

	fdacspeed = dacspeed / 1e3;
	ffref = fref / 1e3;

	ffreq /= ffref;
	ffreq *= 16;
	mindiff = ffreq;

	if (freq <= dacspeed/4)
		min_df = 0;
	else if (freq <= dacspeed/2)
		min_df = 1;
	else
		min_df = 2;

	for (df=0; df<4; df++) {
		ffreq /= 2;
		mindiff /= 2;
		if (df < min_df)
			continue;

		if (df < 3)
			max_n = fref / 1000 / 2;
		else
			max_n = fref / 1000;
		if (max_n > 31)
			max_n = 31;

		for (n=2; n <= max_n; n++) {
			m = (int)(ffreq * n + 0.5) - 65;
			if (m < 0)
				m = 0;
			else if (m > 63)
				m = 63;
			diff = (m+65.0)/n-ffreq;
			if (diff < 0)
				diff = -diff;
			if (diff < mindiff) {
				mindiff = diff;
				best_n = n;
				best_m = m;
				best_df = df;
			}
		}
	}

	S3ProgramIBMRGBClock(pScrn, clk, best_m, best_n, best_df);
}

void S3IBMRGB_Restore(ScrnInfoPtr pScrn)
{
	S3Ptr pS3 = S3PTR(pScrn);
	S3RegPtr restore = &pS3->SavedRegs;
	int vgaCRIndex = pS3->vgaCRIndex, vgaCRReg = pS3->vgaCRReg;
	int i;

	for(i=0; i<0x100; i++)
		S3OutIBMRGBIndReg(pScrn, i, 0, restore->dacregs[i]);

	outb(vgaCRIndex, 0x22);
	outb(vgaCRReg, restore->dacregs[0x100]);
}


void S3IBMRGB_Save(ScrnInfoPtr pScrn)
{
	S3Ptr pS3 = S3PTR(pScrn);
	S3RegPtr save = &pS3->SavedRegs;
	int vgaCRIndex = pS3->vgaCRIndex, vgaCRReg = pS3->vgaCRReg;
	int i;

	for (i=0; i<0x100; i++)
		save->dacregs[i] = S3InIBMRGBIndReg(pScrn, i);

	outb(vgaCRIndex, 0x22);
	save->dacregs[0x100] = inb(vgaCRReg);
}


void S3IBMRGB_PreInit(ScrnInfoPtr pScrn)
{
	S3Ptr pS3 = S3PTR(pScrn);
	int vgaCRIndex = pS3->vgaCRIndex, vgaCRReg = pS3->vgaCRReg;
	unsigned char cr55, tmp;

	outb(vgaCRIndex, 0x43);
	tmp = inb(vgaCRReg);
	outb(vgaCRReg, tmp & ~0x02);

	outb(vgaCRIndex, 0x55);
	cr55 = inb(vgaCRReg);
	outb(vgaCRReg, (cr55 & ~0x03) | 0x01);	/* set rs2 */

	tmp = inb(IBMRGB_INDEX_CONTROL);
	outb(IBMRGB_INDEX_CONTROL, tmp & ~1);
	outb(IBMRGB_INDEX_HIGH, 0);

	outb(vgaCRIndex, 0x55);
	outb(vgaCRReg, cr55 & ~0x03);

	{
		int m, n, df, mclk=0;

		m = S3InIBMRGBIndReg(pScrn, IBMRGB_sysclk_vco_div);
		n = S3InIBMRGBIndReg(pScrn, IBMRGB_sysclk_ref_div) & 0x1f;
		df = m >> 6;
		m &= 0x3f;
		if (!n) {
			m = 0;
			n = 1;
		}
		mclk = ((pS3->RefClock*100 * (m+65)) / n / (8 >> df) + 50) / 100;
		pS3->mclk = mclk;
		xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "MCLK %1.3f MHz\n",
			   mclk / 1000.0);
	}
}


void S3IBMRGB_Init(ScrnInfoPtr pScrn, DisplayModePtr mode)
{
	S3Ptr pS3 = S3PTR(pScrn);
	unsigned char tmp, blank;
	int vgaCRIndex = pS3->vgaCRIndex, vgaCRReg = pS3->vgaCRReg;

	S3IBMRGBSetClock(pScrn, mode->Clock, 2, pS3->MaxClock,
			 pS3->RefClock);

	outb(0x3c4, 1);
	blank = inb(0x3c5);
	outb(0x3c5, blank | 0x20);

	S3OutIBMRGBIndReg(pScrn, IBMRGB_misc_clock, 0xf0, 0x03);
	S3OutIBMRGBIndReg(pScrn, IBMRGB_sync, 0, 0);
	S3OutIBMRGBIndReg(pScrn, IBMRGB_hsync_pos, 0, 0);
	S3OutIBMRGBIndReg(pScrn, IBMRGB_pwr_mgmt, 0, 0);
	S3OutIBMRGBIndReg(pScrn, IBMRGB_dac_op, ~8, 0);
	S3OutIBMRGBIndReg(pScrn, IBMRGB_dac_op, ~2, 2);
	S3OutIBMRGBIndReg(pScrn, IBMRGB_pal_ctrl, 0, 0);
	S3OutIBMRGBIndReg(pScrn, IBMRGB_misc1, ~0x43, 1);
	S3OutIBMRGBIndReg(pScrn, IBMRGB_misc2, 0, 0x47);

	outb(vgaCRIndex, 0x22);
	tmp = inb(vgaCRReg);
	if (pS3->s3Bpp == 1)
		outb(vgaCRReg, tmp | 8);
	else
		outb(vgaCRReg, tmp & ~8);

	outb(vgaCRIndex, 0x65);
	outb(vgaCRReg, 0x00);	/* ! 528 */

	outb(vgaCRIndex, 0x40);
	outb(vgaCRReg, 0x11);
	outb(vgaCRIndex, 0x55);
	outb(vgaCRReg, 0x00);

	switch (pScrn->depth) {
	case 8:
		S3OutIBMRGBIndReg(pScrn, IBMRGB_pix_fmt, 0xf8, 3);
		S3OutIBMRGBIndReg(pScrn, IBMRGB_8bpp, 0, 0);
		break;
	case 15:
		S3OutIBMRGBIndReg(pScrn, IBMRGB_pix_fmt, 0xf8, 4);
		S3OutIBMRGBIndReg(pScrn, IBMRGB_16bpp, 0, 0xc0);
		break;
	case 16:
		S3OutIBMRGBIndReg(pScrn, IBMRGB_pix_fmt, 0xf8, 4);
		S3OutIBMRGBIndReg(pScrn, IBMRGB_16bpp, 0, 0xc2);
		break;
	}

	outb(vgaCRIndex, 0x66);
	tmp = inb(vgaCRReg) & 0xf8;
	outb(vgaCRReg, tmp);

	outb(vgaCRIndex, 0x58);
	tmp = (inb(vgaCRReg) & 0xbf) | 0x40;
	outb(vgaCRReg, tmp);

	outb(vgaCRIndex, 0x67);
	outb(vgaCRReg, 0x11);

	switch (pScrn->bitsPerPixel) {
	case 8:
		tmp = 0x21;
		break;
	case 16:
		tmp = 0x10;
		break;
	}
	outb(vgaCRIndex, 0x6d);
	outb(vgaCRReg, tmp);

	outb(0x3c4, 1);
	outb(0x3c5, blank);
}


/* hardware cursor */

static void S3IBMRGBSetCursorColors(ScrnInfoPtr pScrn, int bg, int fg)
{
	S3Ptr pS3 = S3PTR(pScrn);
	int vgaCRIndex = pS3->vgaCRIndex, vgaCRReg = pS3->vgaCRReg;
	unsigned char tmp;

	/* unlock sys regs */
	outb(vgaCRIndex, 0x39);
	outb(vgaCRReg, 0xa5);

	outb(vgaCRIndex, 0x55);
	tmp = inb(vgaCRReg) & 0xfc;
	outb(vgaCRReg, tmp | 0x01);

	outb(IBMRGB_INDEX_LOW, IBMRGB_curs_col1_r);
	outb(IBMRGB_INDEX_DATA, (bg & 0x00ff0000) >> 16);
	outb(IBMRGB_INDEX_LOW, IBMRGB_curs_col1_g);
	outb(IBMRGB_INDEX_DATA, (bg & 0x0000ff00) >> 8);
	outb(IBMRGB_INDEX_LOW, IBMRGB_curs_col1_b);
	outb(IBMRGB_INDEX_DATA, (bg & 0x000000ff));
	outb(IBMRGB_INDEX_LOW, IBMRGB_curs_col2_r);
	outb(IBMRGB_INDEX_DATA, (fg & 0x00ff0000) >> 16);
	outb(IBMRGB_INDEX_LOW, IBMRGB_curs_col2_g);
	outb(IBMRGB_INDEX_DATA, (fg & 0x0000ff00) >> 8);
	outb(IBMRGB_INDEX_LOW, IBMRGB_curs_col2_b);
	outb(IBMRGB_INDEX_DATA, (fg & 0x000000ff));

	outb(vgaCRReg, tmp);
}


static void S3IBMRGBSetCursorPosition(ScrnInfoPtr pScrn, int x, int y)
{
	S3Ptr pS3 = S3PTR(pScrn);
	int vgaCRIndex = pS3->vgaCRIndex, vgaCRReg = pS3->vgaCRReg;
	unsigned char tmp;

	/* unlock sys regs */
	outb(vgaCRIndex, 0x39);
	outb(vgaCRReg, 0xa5);

	outb(vgaCRIndex, 0x55);
	tmp = inb(vgaCRReg) & 0xfc;
	outb(vgaCRReg, tmp | 0x01);

	outb(IBMRGB_INDEX_LOW, IBMRGB_curs_xl);
	outb(IBMRGB_INDEX_DATA, x);
	outb(IBMRGB_INDEX_LOW, IBMRGB_curs_xh);
	outb(IBMRGB_INDEX_DATA, x >> 8);
	outb(IBMRGB_INDEX_LOW, IBMRGB_curs_yl);
	outb(IBMRGB_INDEX_DATA, y);
	outb(IBMRGB_INDEX_LOW, IBMRGB_curs_yh);
	outb(IBMRGB_INDEX_DATA, y >> 8);

	outb(vgaCRReg, tmp);
}


static void S3IBMRGBHideCursor(ScrnInfoPtr pScrn)
{
	S3Ptr pS3 = S3PTR(pScrn);
	int vgaCRIndex = pS3->vgaCRIndex, vgaCRReg = pS3->vgaCRReg;

	/* unlock sys regs */
	outb(vgaCRIndex, 0x39);
	outb(vgaCRReg, 0xa5);

	S3OutIBMRGBIndReg(pScrn, IBMRGB_curs, ~3, 0x00);
}


static void S3IBMRGBShowCursor(ScrnInfoPtr pScrn)
{
	S3Ptr pS3 = S3PTR(pScrn);
	int vgaCRIndex = pS3->vgaCRIndex, vgaCRReg = pS3->vgaCRReg;
	unsigned char tmp;

	/* unlock sys regs */
	outb(vgaCRIndex, 0x39);
	outb(vgaCRReg, 0xa5);

	outb(vgaCRIndex, 0x55);
	tmp = (inb(vgaCRReg) & 0xdf) | 0x20;
	outb(vgaCRReg, tmp);

	outb(vgaCRIndex, 0x45);
	tmp = inb(vgaCRReg) & ~0x20;
	outb(vgaCRReg, tmp);

	S3OutIBMRGBIndReg(pScrn, IBMRGB_curs, 0, 0x27);
}


static void S3IBMRGBLoadCursorImage(ScrnInfoPtr pScrn, unsigned char *image)
{
	S3Ptr pS3 = S3PTR(pScrn);
	int vgaCRIndex = pS3->vgaCRIndex, vgaCRReg = pS3->vgaCRReg;
	unsigned char tmp, tmp2;
	register int i;

	/* unlock sys regs */
	outb(vgaCRIndex, 0x39);
	outb(vgaCRReg, 0xa5);

	outb(vgaCRIndex, 0x55);
	tmp = inb(vgaCRReg) & 0xfc;
	outb(vgaCRReg, tmp | 0x01);

	outb(IBMRGB_INDEX_LOW, IBMRGB_curs_hot_x);
	outb(IBMRGB_INDEX_DATA, 0);
	outb(IBMRGB_INDEX_LOW, IBMRGB_curs_hot_y);
	outb(IBMRGB_INDEX_DATA, 0);
	outb(IBMRGB_INDEX_LOW, IBMRGB_curs_xl);
	outb(IBMRGB_INDEX_DATA, 0xff);
	outb(IBMRGB_INDEX_LOW, IBMRGB_curs_xh);
	outb(IBMRGB_INDEX_DATA, 0x7f);
	outb(IBMRGB_INDEX_LOW, IBMRGB_curs_yl);
	outb(IBMRGB_INDEX_DATA, 0xff);
	outb(IBMRGB_INDEX_LOW, IBMRGB_curs_yh);
	outb(IBMRGB_INDEX_DATA, 0x7f);

	tmp2 = inb(IBMRGB_INDEX_CONTROL) & 0xfe;
	outb(IBMRGB_INDEX_CONTROL, tmp2 | 1);	/* enable auto increment */

	outb(IBMRGB_INDEX_HIGH, (unsigned char) (IBMRGB_curs_array >> 8));
	outb(IBMRGB_INDEX_LOW, (unsigned char) (IBMRGB_curs_array));

	for (i=0; i<1024; i++)
		outb(IBMRGB_INDEX_DATA, *image++);

	outb(IBMRGB_INDEX_HIGH, 0);
	outb(IBMRGB_INDEX_CONTROL, tmp2);	/* disable auto increment */
	outb(vgaCRIndex, 0x55);
	outb(vgaCRReg, tmp);
}


static Bool S3IBMRGBUseHWCursor(ScreenPtr pScreen, CursorPtr pCurs)
{
	return TRUE;
}


Bool S3IBMRGB_CursorInit(ScreenPtr pScreen)
{
	ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
	S3Ptr pS3 = S3PTR(pScrn);
	xf86CursorInfoPtr pCurs;

	if (!(pCurs = pS3->pCurs = xf86CreateCursorInfoRec()))
		return FALSE;

	pCurs->MaxWidth = 64;
	pCurs->MaxHeight = 64;
	pCurs->Flags = HARDWARE_CURSOR_TRUECOLOR_AT_8BPP |
		       HARDWARE_CURSOR_SOURCE_MASK_INTERLEAVE_1 |
		       HARDWARE_CURSOR_AND_SOURCE_WITH_MASK | 
		       HARDWARE_CURSOR_NIBBLE_SWAPPED |
		       HARDWARE_CURSOR_BIT_ORDER_MSBFIRST;

	pCurs->SetCursorColors = S3IBMRGBSetCursorColors;
	pCurs->SetCursorPosition = S3IBMRGBSetCursorPosition;
	pCurs->LoadCursorImage = S3IBMRGBLoadCursorImage;
	pCurs->HideCursor = S3IBMRGBHideCursor;
	pCurs->ShowCursor = S3IBMRGBShowCursor;
	pCurs->UseHWCursor = S3IBMRGBUseHWCursor;

	return xf86InitCursor(pScreen, pCurs);
}



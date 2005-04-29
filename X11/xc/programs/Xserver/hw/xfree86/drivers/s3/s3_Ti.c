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
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/s3/s3_Ti.c,v 1.6 2003/07/17 08:19:36 eich Exp $ */

#include "xf86.h"
#include "xf86_OSproc.h"
#include "xf86_ansic.h"

#include "compiler.h"

#include "TI.h"

#include "s3.h"


#define TI_WRITE_ADDR           0x3C8   /* CR55 low bit == 0 */
#define TI_RAMDAC_DATA          0x3C9   /* CR55 low bit == 0 */
#define TI_PIXEL_MASK           0x3C6   /* CR55 low bit == 0 */
#define TI_READ_ADDR            0x3C7   /* CR55 low bit == 0 */
#define TI_INDEX_REG            0x3C6   /* CR55 low bit == 1 */
#define TI_DATA_REG             0x3C7   /* CR55 low bit == 1 */

#define	TIDAC_output_clock_select	0x1b
#define	TIDAC_auxiliary_ctrl		0x29
#define	TIDAC_general_io_ctrl		0x2a
#define TIDAC_general_io_data		0x2b
#define TIDAC_cursor_color_0_red	0x23
#define TIDAC_cursor_color_0_green	0x24
#define TIDAC_cursor_color_0_blue	0x25
#define TIDAC_cursor_color_1_red	0x26
#define TIDAC_cursor_color_1_green	0x27
#define TIDAC_cursor_color_1_blue	0x28
#define TIDAC_cursor_x_low		0x00
#define TIDAC_cursor_x_high		0x01
#define TIDAC_cursor_y_low		0x02
#define TIDAC_cursor_y_high		0x03
#define TIDAC_cursor_ram_addr_low	0x08
#define TIDAC_cursor_ram_addr_high	0x09
#define TIDAC_cursor_ram_data		0x0a


#define TI_REF_FREQ             14.31818  /* 3025 only */

#undef  FREQ_MIN
#define FREQ_MIN   12000
#define FREQ_MAX  220000



void S3OutTiIndReg(ScrnInfoPtr pScrn, CARD32 reg, unsigned char mask,
		   unsigned char data)
{
	S3Ptr pS3 = S3PTR(pScrn);
	int vgaCRIndex = pS3->vgaCRIndex, vgaCRReg = pS3->vgaCRReg;
	unsigned char tmp, tmp1, tmp2 = 0x00;

	outb(vgaCRIndex, 0x55);
	tmp = inb(vgaCRReg) & 0xfc;
	outb(vgaCRReg, tmp | 0x01);
	tmp1 = inb(TI_INDEX_REG);
	outb(TI_INDEX_REG, reg);

	if (mask != 0x00)
		tmp2 = inb(TI_DATA_REG) & mask;
	outb(TI_DATA_REG, tmp2 | data);

	outb(TI_INDEX_REG, tmp1);
	outb(vgaCRReg, tmp);
}


static unsigned char S3InTiIndReg(ScrnInfoPtr pScrn, CARD32 reg)
{
	S3Ptr pS3 = S3PTR(pScrn);
	int vgaCRIndex = pS3->vgaCRIndex, vgaCRReg = pS3->vgaCRReg;
	unsigned char tmp, tmp1, ret;

	outb(vgaCRIndex, 0x55);
	tmp = inb(vgaCRReg) & 0xfc;
	outb(vgaCRReg, tmp | 0x01);
	tmp1 = inb(TI_INDEX_REG);
	outb(TI_INDEX_REG, reg);

	ret = inb(TI_DATA_REG);

	outb(TI_INDEX_REG, tmp1);
	outb(vgaCRReg, tmp);

	return ret;
}


Bool S3TiDACProbe(ScrnInfoPtr pScrn)
{
	S3Ptr pS3 = S3PTR(pScrn);
	int found = 0;
	int vgaCRIndex = pS3->vgaCRIndex, vgaCRReg = pS3->vgaCRReg;
	unsigned char cr55, cr45, cr43, cr5c;
	unsigned char TIndx, TIndx2, TIdata;

	if (!S3_964_SERIES())
		return FALSE;

	outb(vgaCRIndex, 0x43);
	cr43 = inb(vgaCRReg);
	outb(vgaCRReg, cr43 & ~0x02);

	outb(vgaCRIndex, 0x45);
	cr45 = inb(vgaCRReg);

	outb(vgaCRIndex, 0x55);
	cr55 = inb(vgaCRReg);
	outb(vgaCRReg, (cr55 & 0xfc) | 0x01);

	TIndx = inb(TI_INDEX_REG);
	outb(TI_INDEX_REG, TIDAC_id);
	if(inb(TI_DATA_REG) == 0x20) {
		found = TI3020_RAMDAC;
		cr43 &= ~0x02;
		cr45 &= ~0x20;
	} else {
		outb(vgaCRIndex, 0x5c);
		cr5c = inb(vgaCRReg);
		outb(vgaCRReg, cr5c & 0xdf);
		TIndx2 = inb(TI_INDEX_REG);
		outb(TI_INDEX_REG, TIDAC_ind_curs_ctrl);
		TIdata = inb(TI_DATA_REG);
		outb(TI_DATA_REG, TIdata & 0x7f);

		outb(TI_INDEX_REG, TIDAC_id);
		if (inb(TI_DATA_REG) == 0x25) {
			found = TI3025_RAMDAC;
			cr43 &= ~0x02;
			cr45 &= ~0x20;
		}

		outb(TI_INDEX_REG, TIDAC_ind_curs_ctrl);
		outb(TI_DATA_REG, TIdata);
		outb(TI_INDEX_REG, TIndx2);
		outb(vgaCRIndex, 0x5c);
		outb(vgaCRReg, cr5c);
	}

	outb(TI_INDEX_REG, TIndx);
	outb(vgaCRIndex, 0x55);
	outb(vgaCRReg, cr55);

	outb(vgaCRIndex, 0x45);
	outb(vgaCRReg, cr45);

	outb(vgaCRIndex, 0x43);
	outb(vgaCRReg, cr43);

	if (found) {
		RamDacInit(pScrn, pS3->RamDacRec);
		pS3->RamDac = RamDacHelperCreateInfoRec();
		pS3->RamDac->RamDacType = found;
		return TRUE;
	}

	return FALSE;
}


void S3TiDAC_Save(ScrnInfoPtr pScrn)
{
	S3Ptr pS3 = S3PTR(pScrn);
	S3RegPtr save = &pS3->SavedRegs;
	int vgaCRIndex = pS3->vgaCRIndex, vgaCRReg = pS3->vgaCRReg;

	if (pS3->RamDac->RamDacType == TI3025_RAMDAC) {
		unsigned char cr5c;

		outb(vgaCRIndex, 0x5c);
		cr5c = inb(vgaCRReg);
		outb(vgaCRReg, cr5c & 0xdf);

		save->dacregs[TIDAC_ind_curs_ctrl] =
				 S3InTiIndReg(pScrn, TIDAC_ind_curs_ctrl);
		S3OutTiIndReg(pScrn, TIDAC_ind_curs_ctrl, 0x7f, 0x00);
	}

	save->dacregs[TIDAC_true_color_ctrl] = 
			S3InTiIndReg(pScrn, TIDAC_true_color_ctrl);
	save->dacregs[TIDAC_multiplex_ctrl] =
			S3InTiIndReg(pScrn, TIDAC_multiplex_ctrl);
	save->dacregs[TIDAC_clock_select] =
			S3InTiIndReg(pScrn, TIDAC_clock_select);
	save->dacregs[TIDAC_output_clock_select] =
			S3InTiIndReg(pScrn, TIDAC_output_clock_select);
	save->dacregs[TIDAC_general_ctrl] =
			S3InTiIndReg(pScrn, TIDAC_general_ctrl);
	save->dacregs[TIDAC_auxiliary_ctrl] =
			S3InTiIndReg(pScrn, TIDAC_auxiliary_ctrl);
	S3OutTiIndReg(pScrn, TIDAC_general_io_ctrl, 0x00, 0x1f);
	save->dacregs[TIDAC_general_io_data] =
			S3InTiIndReg(pScrn, TIDAC_general_io_data);

	if (pS3->RamDac->RamDacType == TI3025_RAMDAC) {
		save->dacregs[0x0e] = S3InTiIndReg(pScrn, 0x0e);
		save->dacregs[TIDAC_misc_ctrl] =
			S3InTiIndReg(pScrn, TIDAC_misc_ctrl);
		S3OutTiIndReg(pScrn, TIDAC_pll_addr, 0x00, 0x00);
		save->dacregs[0x40] = S3InTiIndReg(pScrn, TIDAC_pll_pixel_data);
		S3OutTiIndReg(pScrn, TIDAC_pll_pixel_data, 0x00,
			      save->dacregs[0x40]);
		save->dacregs[0x41] = S3InTiIndReg(pScrn, TIDAC_pll_pixel_data);
		S3OutTiIndReg(pScrn, TIDAC_pll_pixel_data, 0x00,
			      save->dacregs[0x41]);
		save->dacregs[0x42] = S3InTiIndReg(pScrn, TIDAC_pll_pixel_data);
		S3OutTiIndReg(pScrn, TIDAC_pll_pixel_data, 0x00,
			      save->dacregs[0x42]);
		save->dacregs[0x43] = S3InTiIndReg(pScrn, TIDAC_pll_memory_data);
		S3OutTiIndReg(pScrn, TIDAC_pll_memory_data, 0x00,
			      save->dacregs[0x43]);
		save->dacregs[0x44] = S3InTiIndReg(pScrn, TIDAC_pll_memory_data);
		S3OutTiIndReg(pScrn, TIDAC_pll_memory_data, 0x00,
			      save->dacregs[0x44]);
		save->dacregs[0x45] = S3InTiIndReg(pScrn, TIDAC_pll_memory_data);
		S3OutTiIndReg(pScrn, TIDAC_pll_memory_data, 0x00,
			      save->dacregs[0x45]);
		save->dacregs[0x46] = S3InTiIndReg(pScrn, TIDAC_pll_loop_data);
		S3OutTiIndReg(pScrn, TIDAC_pll_loop_data, 0x00,
			      save->dacregs[0x46]);
		save->dacregs[0x47] = S3InTiIndReg(pScrn, TIDAC_pll_loop_data);
		S3OutTiIndReg(pScrn, TIDAC_pll_loop_data, 0x00,
			      save->dacregs[0x47]);
		save->dacregs[0x48] = S3InTiIndReg(pScrn, TIDAC_pll_loop_data);
		S3OutTiIndReg(pScrn, TIDAC_pll_loop_data, 0x00,
			      save->dacregs[0x48]);
	}
}


void S3TiDAC_Restore(ScrnInfoPtr pScrn)
{
	S3Ptr pS3 = S3PTR(pScrn);
	S3RegPtr restore = &pS3->SavedRegs;

	S3OutTiIndReg(pScrn, TIDAC_true_color_ctrl, 0x00,
		      restore->dacregs[TIDAC_true_color_ctrl]);
	S3OutTiIndReg(pScrn, TIDAC_multiplex_ctrl, 0x00,
		      restore->dacregs[TIDAC_multiplex_ctrl]);	
	S3OutTiIndReg(pScrn, TIDAC_clock_select, 0x00,
		      restore->dacregs[TIDAC_clock_select]);
	S3OutTiIndReg(pScrn, TIDAC_output_clock_select, 0x00,
		      restore->dacregs[TIDAC_output_clock_select]);
	S3OutTiIndReg(pScrn, TIDAC_general_ctrl, 0x00,
		      restore->dacregs[TIDAC_general_ctrl]);
	S3OutTiIndReg(pScrn, TIDAC_auxiliary_ctrl, 0x00,
		      restore->dacregs[TIDAC_auxiliary_ctrl]);
	S3OutTiIndReg(pScrn, TIDAC_general_io_ctrl, 0x00, 0x1f);
	S3OutTiIndReg(pScrn, TIDAC_general_io_data, 0x00,
		      restore->dacregs[TIDAC_general_io_data]);
	if (pS3->RamDac->RamDacType == TI3025_RAMDAC) {
		S3OutTiIndReg(pScrn, TIDAC_pll_addr, 0x00,
			      restore->dacregs[TIDAC_pll_addr]);
		S3OutTiIndReg(pScrn, TIDAC_pll_pixel_data, 0x00,
			      restore->dacregs[0x40]);
		S3OutTiIndReg(pScrn, TIDAC_pll_pixel_data, 0x00,
			      restore->dacregs[0x41]);
		S3OutTiIndReg(pScrn, TIDAC_pll_pixel_data, 0x00,
			      restore->dacregs[0x42]);

		S3OutTiIndReg(pScrn, TIDAC_pll_memory_data, 0x00,
			      restore->dacregs[0x43]);
		S3OutTiIndReg(pScrn, TIDAC_pll_memory_data, 0x00,
			      restore->dacregs[0x44]);
		S3OutTiIndReg(pScrn, TIDAC_pll_memory_data, 0x00,
			      restore->dacregs[0x45] | 0x80);

		S3OutTiIndReg(pScrn, TIDAC_pll_loop_data, 0x00,
			      restore->dacregs[0x46]);
		S3OutTiIndReg(pScrn, TIDAC_pll_loop_data, 0x00,
			      restore->dacregs[0x47]);
		S3OutTiIndReg(pScrn, TIDAC_pll_loop_data, 0x00,
			      restore->dacregs[0x48]);

		S3OutTiIndReg(pScrn, 0x0e, 0x00, restore->dacregs[0x0e]);
		S3OutTiIndReg(pScrn, TIDAC_misc_ctrl, 0x00,
			      restore->dacregs[TIDAC_misc_ctrl]);
	}

	S3OutTiIndReg(pScrn, TIDAC_ind_curs_ctrl, 0x00,
		      restore->dacregs[TIDAC_ind_curs_ctrl]);
}


void S3TiDAC_PreInit(ScrnInfoPtr pScrn)
{
        S3Ptr pS3 = S3PTR(pScrn);
        int vgaCRIndex = pS3->vgaCRIndex, vgaCRReg = pS3->vgaCRReg;
	int mclk, m, n, p, mcc, cr5c;

	outb(vgaCRIndex, 0x5c);
	cr5c = inb(vgaCRReg);
	outb(vgaCRReg, cr5c & 0xdf);

	S3OutTiIndReg(pScrn, TIDAC_pll_addr, 0x00, 0x00);
	n = S3InTiIndReg(pScrn, TIDAC_pll_memory_data) & 0x7f;
	S3OutTiIndReg(pScrn, TIDAC_pll_addr, 0x00, 0x01);
	m = S3InTiIndReg(pScrn, TIDAC_pll_memory_data) & 0x7f;
	S3OutTiIndReg(pScrn, TIDAC_pll_addr, 0x00, 0x02);
	p = S3InTiIndReg(pScrn, TIDAC_pll_memory_data) & 0x03;
	mcc = S3InTiIndReg(pScrn, TIDAC_clock_ctrl);
	if (mcc & 0x08)
		mcc = (mcc & 0x07) * 2 + 2;
	else
		mcc = 1;
 
	mclk = ((1431818 * ((m+2) * 8)) / (n+2) / (1 << p) / mcc + 50) / 100;
	pS3->mclk = mclk;
     	xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "MCLK %1.3f MHz\n",
           	   mclk / 1000.0);

	outb(vgaCRIndex, 0x5c);
	outb(vgaCRReg, cr5c);
}


static void S3TiDACCalcNMP(long freq, int *calc_n, int *calc_m, int *calc_p)
{
	double ffreq;
	int n, m, p;
	int best_n=32, best_m=32;
	double diff, mindiff;

	if (freq < FREQ_MIN)
		ffreq = FREQ_MIN / 1000.0;
	else if (freq > FREQ_MAX)
		ffreq = FREQ_MAX / 1000.0;
	else
		ffreq = freq / 1000.0;

	for (p=0; p<4 && ffreq<110.0; p++)
		ffreq *= 2;
#if FREQ_MIN < 110000/8
	if (p == 4) {
		ffreq /= 2;
		p--;
	}
#endif

	ffreq /= TI_REF_FREQ;

	mindiff = ffreq;

	for (n=1; n<=(int)(TI_REF_FREQ/0.5 - 2); n++) {
		m = (int)(ffreq * (n+2) / 8.0 + 0.5) - 2;
		if (m < 1)
			m = 1;
		else if (m > 127)
			m = 127;

		diff = ((m+2) * 8) / (n+2.0) - ffreq;
		if (diff < 0)
			diff = -diff;

		if (diff < mindiff) {
			mindiff = diff;
			best_n = n;
			best_m = m;
		}
	}

	*calc_n = best_n;
   	*calc_m = best_m;
   	*calc_p = p;
}


static void S3TiDACProgramClock(ScrnInfoPtr pScrn, int clk,
				unsigned char n, unsigned char m,
				unsigned char p)
{
	S3OutTiIndReg(pScrn, TIDAC_pll_addr, 0x00, 0x00);

	if (clk != TIDAC_pll_memory_data) {
		S3OutTiIndReg(pScrn, TIDAC_pll_pixel_data, 0x00, n);
		S3OutTiIndReg(pScrn, TIDAC_pll_pixel_data, 0x00, m);
		S3OutTiIndReg(pScrn, TIDAC_pll_pixel_data, 0x00, p | 0x08);

		S3OutTiIndReg(pScrn, TIDAC_pll_loop_data, 0x00, 0x01);
		S3OutTiIndReg(pScrn, TIDAC_pll_loop_data, 0x00, 0x01);
		S3OutTiIndReg(pScrn, TIDAC_pll_loop_data, 0x00, p > 0 ? p : 1);
		S3OutTiIndReg(pScrn, TIDAC_misc_ctrl, 0x00, 0x80 | 0x40 | 0x04);

		S3OutTiIndReg(pScrn, TIDAC_clock_select, 0x00, 0x05);
	} else {
		S3OutTiIndReg(pScrn, TIDAC_pll_memory_data, 0x00, n);
		S3OutTiIndReg(pScrn, TIDAC_pll_memory_data, 0x00, m);
		S3OutTiIndReg(pScrn, TIDAC_pll_memory_data, 0x00, p | 0x80);
	}
}


static void S3TiDACSetClock(ScrnInfoPtr pScrn, long freq, int clk)
{
	int m, n, p;

	S3TiDACCalcNMP(freq, &n, &m, &p);

	S3TiDACProgramClock(pScrn, clk, n, m, p);
}



void S3TiDAC_Init(ScrnInfoPtr pScrn, DisplayModePtr mode)
{
        S3Ptr pS3 = S3PTR(pScrn);
        vgaHWPtr hwp = VGAHWPTR(pScrn);
        vgaRegPtr pVga = &hwp->ModeReg;
        int vgaCRIndex = pS3->vgaCRIndex, vgaCRReg = pS3->vgaCRReg;
	unsigned char tmp, tmp1, tmp2;

	S3TiDACSetClock(pScrn, mode->Clock, 2);
	outb(vgaCRIndex, 0x42);
	tmp = inb(vgaCRReg) & 0xf0;
	outb(vgaCRReg, tmp | 0x02);
	usleep(150000);

	outb(0x3c4, 1);
	tmp2 = inb(0x3c5);
	outb(0x3c5, tmp2 | 0x20);	/* blank */

	tmp = pVga->MiscOutReg;
	pVga->MiscOutReg |= 0xc0;
	tmp1 = 0x00;
	if (!(tmp & 0x80))
		tmp1 |= 0x02;
      	if (!(tmp & 0x40))
		tmp1 |= 0x01;

	S3OutTiIndReg(pScrn, TIDAC_general_ctrl, 0x00, tmp1);
	S3OutTiIndReg(pScrn, 0x0e, 0x00, 0x00);

	/* XXX do 3020 input_clock_sel */

	outb(vgaCRIndex, 0x65);
	/* XXX 3025 */
	outb(vgaCRReg, 0x00);

	/* XXX 3025 */
	outb(vgaCRIndex, 0x40);
	outb(vgaCRReg, 0x11);
	outb(vgaCRIndex, 0x55);
	outb(vgaCRReg, 0x00);

	if (pScrn->bitsPerPixel > 8)
		S3OutTiIndReg(pScrn, TIDAC_auxiliary_ctrl, 0x00, 0x00);
	else
		S3OutTiIndReg(pScrn, TIDAC_auxiliary_ctrl, 0x00, 0x01);

	switch (pScrn->depth) {
	case 8:
		S3OutTiIndReg(pScrn, TIDAC_output_clock_select, 0x00,
			      0x4b);
		outb(vgaCRIndex, 0x66);
		tmp = inb(vgaCRReg);
		if (mode->Clock > 80000)
			outb(vgaCRReg, (tmp & 0xf8) | 0x02);
		else
			outb(vgaCRReg, (tmp & 0xf8) | 0x03);
		break;
	case 16:
		S3OutTiIndReg(pScrn, TIDAC_output_clock_select, 0x00,
			      0x4a);
		outb(vgaCRIndex, 0x66);
		tmp = inb(vgaCRReg);
		if (mode->Clock > 80000)
			outb(vgaCRReg, (tmp & 0xf8) | 0x01);
		else
			outb(vgaCRReg, (tmp & 0xf8) | 0x02);
		break;
	case 24:
		S3OutTiIndReg(pScrn, TIDAC_output_clock_select, 0x00,
			      (0x40 | 0x08 | 0x01));
		outb(vgaCRIndex, 0x66);
		tmp = inb(vgaCRReg);
		if (mode->Clock > 80000)
			outb(vgaCRReg, (tmp & 0xf8) | 0x00);
		else
			outb(vgaCRReg, (tmp & 0xf8) | 0x01);
		break;
	}

	outb(vgaCRIndex, 0x58);
	tmp = inb(vgaCRReg);
	outb(vgaCRReg, (tmp & 0xbf) | 0x40);

	switch(pScrn->depth) {
	case 8:
		S3OutTiIndReg(pScrn, TIDAC_true_color_ctrl, 0x00, 0x80);
		S3OutTiIndReg(pScrn, TIDAC_multiplex_ctrl, 0x00, 0x1c);
		break;
	case 15:
		S3OutTiIndReg(pScrn, TIDAC_true_color_ctrl, 0x00, 0x4c);
		S3OutTiIndReg(pScrn, TIDAC_multiplex_ctrl, 0x00, 0x04);
		S3OutTiIndReg(pScrn, TIDAC_key_ctrl, 0x00, 0x01);
		break;
	case 16:
		S3OutTiIndReg(pScrn, TIDAC_true_color_ctrl, 0x00, 0x4d);
		S3OutTiIndReg(pScrn, TIDAC_multiplex_ctrl, 0x00, 0x04);
		S3OutTiIndReg(pScrn, TIDAC_key_ctrl, 0x00, 0x01);
		break;
	case 24:
		S3OutTiIndReg(pScrn, TIDAC_true_color_ctrl, 0x00, 0x4e);
		S3OutTiIndReg(pScrn, TIDAC_multiplex_ctrl, 0x00, 0x04);
		S3OutTiIndReg(pScrn, TIDAC_key_ctrl, 0x00, 0x01);
		break;
	}

	S3OutTiIndReg(pScrn, TIDAC_general_io_ctrl, 0x00, 0x1f);
	S3OutTiIndReg(pScrn, TIDAC_general_io_data, 0x00, 0x01);
	S3OutTiIndReg(pScrn, TIDAC_general_io_ctrl, 0x00, 0x00);
	S3OutTiIndReg(pScrn, TIDAC_misc_ctrl, 0xf0, (0x04 | 0x08));

	outb(vgaCRIndex, 0x6d);
	if (pS3->s3Bpp == 1)
		if (mode->Clock > 80000)
			outb(vgaCRReg, 0x02);
		else
			outb(vgaCRReg, 0x03);
	else if (pS3->s3Bpp == 2)
		if (mode->Clock > 80000)
			outb(vgaCRReg, 0x00);
		else
			outb(vgaCRReg, 0x01);
	else
		outb(vgaCRReg, 0x00);

	S3OutTiIndReg(pScrn, TIDAC_sense_test, 0x00, 0x00);

	if (pS3->s3Bpp > 1)
	{
		int j;

		outb(0x3c6, 0xff);
		outb(0x3c8, 0x00);
		for(j=0; j<768; j++) {
			outb(0x3c9, j);
			outb(0x3c9, j);
			outb(0x3c9, j);
		}
	}

	outb(0x3c4, 1);
	outb(0x3c5, tmp2);	/* unblank */
}


void S3TiLoadPalette(ScrnInfoPtr pScrn, int numColors, int *indicies, LOCO *colors,
		     VisualPtr pVisual)
{
	int i;

	outb(0x3c6, 0xff);
	outb(0x3c8, 0x00);

	for (i=0; i<768; i++) {
		outb(0x3c9, i);
		usleep(1000);
		outb(0x3c9, i);
		usleep(1000);
		outb(0x3c9, i);
		usleep(1000);
	}
}



/* hardware cursor */

static void S3TiSetCursorColors(ScrnInfoPtr pScrn, int bg, int fg)
{
        S3Ptr pS3 = S3PTR(pScrn);
        int vgaCRIndex = pS3->vgaCRIndex, vgaCRReg = pS3->vgaCRReg;

        /* unlock sys regs */
        outb(vgaCRIndex, 0x39);
        outb(vgaCRReg, 0xa5);

	S3OutTiIndReg(pScrn, TIDAC_cursor_color_0_red, 0x00,
		      (bg & 0x00FF0000) >> 16);
	S3OutTiIndReg(pScrn, TIDAC_cursor_color_0_green, 0x00,
		      (bg & 0x0000FF00) >> 8);
	S3OutTiIndReg(pScrn, TIDAC_cursor_color_0_blue, 0x00,
		      (bg & 0x000000FF));

	S3OutTiIndReg(pScrn, TIDAC_cursor_color_1_red, 0x00,
		      (fg & 0x00FF0000) >> 16);
	S3OutTiIndReg(pScrn, TIDAC_cursor_color_1_green, 0x00,
		      (fg & 0x0000FF00) >> 8);
	S3OutTiIndReg(pScrn, TIDAC_cursor_color_1_blue, 0x00,
		      (fg & 0x000000FF));

}


static void S3TiSetCursorPosition(ScrnInfoPtr pScrn, int x, int y)
{
        S3Ptr pS3 = S3PTR(pScrn);
        int vgaCRIndex = pS3->vgaCRIndex, vgaCRReg = pS3->vgaCRReg;

        /* unlock sys regs */
        outb(vgaCRIndex, 0x39);
        outb(vgaCRReg, 0xa5);
        
	S3OutTiIndReg(pScrn, TIDAC_cursor_x_low, 0x00, x & 0xff);
	S3OutTiIndReg(pScrn, TIDAC_cursor_x_high, 0x00, (x >> 8) & 0x0f);
	S3OutTiIndReg(pScrn, TIDAC_cursor_y_low, 0x00, y & 0xff);
	S3OutTiIndReg(pScrn, TIDAC_cursor_y_high, 0x00, (y >> 8) & 0x0f);
}


static void S3TiHideCursor(ScrnInfoPtr pScrn)
{
        S3Ptr pS3 = S3PTR(pScrn);  
        int vgaCRIndex = pS3->vgaCRIndex, vgaCRReg = pS3->vgaCRReg;
        
        /* unlock sys regs */
        outb(vgaCRIndex, 0x39);
        outb(vgaCRReg, 0xa5);

	S3OutTiIndReg(pScrn, TIDAC_ind_curs_ctrl, (unsigned char)
		      ~(0x40 | 0x10), 0x00);
}


static void S3TiShowCursor(ScrnInfoPtr pScrn)
{
        S3Ptr pS3 = S3PTR(pScrn);
        int vgaCRIndex = pS3->vgaCRIndex, vgaCRReg = pS3->vgaCRReg;
        unsigned char tmp;

        /* unlock sys regs */
        outb(vgaCRIndex, 0x39);  
        outb(vgaCRReg, 0xa5);
        
	outb(vgaCRIndex, 0x55);
	tmp = inb(vgaCRReg) & 0xdf;
	outb(vgaCRReg, tmp | 0x20);

	outb(vgaCRIndex, 0x45);
	tmp = inb(vgaCRReg) & 0xdf;
	outb(vgaCRReg, tmp | 0x20);

	S3OutTiIndReg(pScrn, TIDAC_ind_curs_ctrl, (unsigned char)
		      ~(0x40 | 0x10), (0x40 | 0x10));
}


static void S3TiLoadCursorImage(ScrnInfoPtr pScrn, unsigned char *image)
{
        S3Ptr pS3 = S3PTR(pScrn);
        int vgaCRIndex = pS3->vgaCRIndex, vgaCRReg = pS3->vgaCRReg;
        unsigned char tmp, tmp1;
        register int i;
#if 0
	register unsigned char *mask = image + 1;
#endif

        /* unlock sys regs */
        outb(vgaCRIndex, 0x39);  
        outb(vgaCRReg, 0xa5);
        
	outb(vgaCRIndex, 0x55);
	tmp = inb(vgaCRReg) & 0xfc;
	outb(vgaCRReg, tmp | 0x01);
	tmp1 = inb(TI_INDEX_REG);

	outb(TI_INDEX_REG, TIDAC_cursor_ram_addr_low);
	outb(TI_DATA_REG, 0x00);
	outb(TI_INDEX_REG, TIDAC_cursor_ram_addr_high);
	outb(TI_DATA_REG, 0x00);
	outb(TI_INDEX_REG, TIDAC_cursor_ram_data);

#if 0
	for (i=0; i<512; i++, mask+=2)
		outb(TI_DATA_REG, *mask);
	for (i=0; i<512; i++, image+=2)
		outb(TI_DATA_REG, *image);
#else
	for (i=0; i<1024; i++) {
		outb(TI_DATA_REG, *image);
		image++;
	}
#endif

	outb(TI_INDEX_REG, tmp1);

	outb(vgaCRIndex, 0x55);
	outb(vgaCRReg, tmp);
}



static Bool S3TiUseHWCursor(ScreenPtr pScreen, CursorPtr pCurs)
{
	ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
	S3Ptr pS3 = S3PTR(pScrn);
	return (pS3->hwCursor);
}       



Bool S3Ti_CursorInit(ScreenPtr pScreen)
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
		       HARDWARE_CURSOR_NIBBLE_SWAPPED |
		       HARDWARE_CURSOR_BIT_ORDER_MSBFIRST;

        pCurs->SetCursorColors = S3TiSetCursorColors;
        pCurs->SetCursorPosition = S3TiSetCursorPosition;
        pCurs->LoadCursorImage = S3TiLoadCursorImage;
        pCurs->HideCursor = S3TiHideCursor;
        pCurs->ShowCursor = S3TiShowCursor;
        pCurs->UseHWCursor = S3TiUseHWCursor;
        
        return xf86InitCursor(pScreen, pCurs);
}

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
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/s3/s3_Trio64DAC.c,v 1.5 2003/02/17 16:45:24 dawes Exp $ */

#include "xf86.h"
#include "xf86_OSproc.h"
#include "xf86_ansic.h"

#include "compiler.h"

#include "s3.h"

/* this is really quite dumb */
Bool S3Trio64DACProbe(ScrnInfoPtr pScrn)
{
	S3Ptr pS3 = S3PTR(pScrn);

	if (!S3_TRIO_SERIES())
		return FALSE;

	RamDacInit(pScrn, pS3->RamDacRec);

	pS3->RamDac = RamDacHelperCreateInfoRec();
	pS3->RamDac->RamDacType = TRIO64_RAMDAC;

	return TRUE;
}


void S3Trio64DAC_Save(ScrnInfoPtr pScrn)
{
	S3Ptr pS3 = S3PTR(pScrn);
	S3RegPtr save = &pS3->SavedRegs;

	save->dacregs[0] = inb(0x3cc);

	outb(0x3c4, 0x08);
	save->dacregs[1] = inb(0x3c5);
	outb(0x3c5, 0x06);

	outb(0x3c4, 0x09);
	save->dacregs[2] = inb(0x3c5);
	outb(0x3c4, 0x0a);
	save->dacregs[3] = inb(0x3c5);
	outb(0x3c4, 0x0b);
	save->dacregs[4] = inb(0x3c5);
	outb(0x3c4, 0x0d);
	save->dacregs[5] = inb(0x3c5);
	outb(0x3c4, 0x15);
	save->dacregs[6] = inb(0x3c5) & 0xfe;
	outb(0x3c5, save->dacregs[6]);

	outb(0x3c4, 0x18);
	save->dacregs[7] = inb(0x3c5);
	outb(0x3c4, 0x10);
	save->dacregs[8] = inb(0x3c5);
	outb(0x3c4, 0x11);
	save->dacregs[9] = inb(0x3c5);
	outb(0x3c4, 0x12);
	save->dacregs[10] = inb(0x3c5);
	outb(0x3c4, 0x13);
	save->dacregs[11] = inb(0x3c5);
	outb(0x3c4, 0x1a);
	save->dacregs[12] = inb(0x3c5);
	outb(0x3c4, 0x1b);
	save->dacregs[13] = inb(0x3c5);

	if (pS3->Chipset == PCI_CHIP_AURORA64VP) {
		int i;

		for (i=0x1a; i <= 0x6f; i++) {
			outb(0x3c4, i);
			save->dacregs[i] = inb(0x3c5);
		}
	}

	outb(0x3c4, 0x08);
	outb(0x3c5, 0x00);
}


void S3Trio64DAC_Restore(ScrnInfoPtr pScrn)
{
	S3Ptr pS3 = S3PTR(pScrn);
	S3RegPtr restore = &pS3->SavedRegs;
	unsigned char tmp;

	outb(0x3c2, restore->dacregs[0]);
	outb(0x3c4, 0x08);
	outb(0x3c5, 0x06);

	outb(0x3c4, 0x09);
	outb(0x3c5, restore->dacregs[2]);
	outb(0x3c4, 0x0a);
	outb(0x3c5, restore->dacregs[3]);
	outb(0x3c4, 0x0b);
	outb(0x3c5, restore->dacregs[4]);
	outb(0x3c4, 0x0d);
	outb(0x3c5, restore->dacregs[5]);

	outb(0x3c4, 0x10);
	outb(0x3c5, restore->dacregs[8]);
	outb(0x3c4, 0x11);
	outb(0x3c5, restore->dacregs[9]);
	outb(0x3c4, 0x12);
	outb(0x3c5, restore->dacregs[10]);
	outb(0x3c4, 0x13);
	outb(0x3c5, restore->dacregs[11]);
	outb(0x3c4, 0x1a);
	outb(0x3c5, restore->dacregs[12]);
	outb(0x3c4, 0x1b);
	outb(0x3c5, restore->dacregs[13]);
	outb(0x3c4, 0x15);
	tmp = inb(0x3c5);
	outb(0x3c4, tmp & ~0x20);
	outb(0x3c4, tmp | 0x20);
	outb(0x3c4, tmp & ~0x20);

	outb(0x3c4, 0x15);
	outb(0x3c5, restore->dacregs[6]);
	outb(0x3c4, 0x18);
	outb(0x3c5, restore->dacregs[7]);

	if (pS3->Chipset == PCI_CHIP_AURORA64VP) {
		int i;

		for (i = 0x1a; i <= 0x6f; i++) {
			outb(0x3c4, i);
			outb(0x3c5, restore->dacregs[i]);
		}
	}

	outb(0x3c4, 0x08);
	outb(0x3c5, restore->dacregs[1]);
}


static void
S3TrioCalcClock(long freq, int min_m, int min_n1, int max_n1, int min_n2,
		int max_n2, long freq_min, long freq_max,
		unsigned char *mdiv, unsigned char *ndiv)
{
	double ffreq, ffreq_min, ffreq_max;
	double div, diff, best_diff;
	unsigned int m;
	unsigned char n1, n2, best_n1=18, best_n2=2, best_m=127;

#define BASE_FREQ	14.31818
	ffreq = freq / 1000.0 / BASE_FREQ;
	ffreq_min = freq_min / 1000.0 / BASE_FREQ;
	ffreq_max = freq_max / 1000.0 / BASE_FREQ;

	if (ffreq < ffreq_min / (1<<max_n2)) {
		ErrorF("invalid frequency %1.3f Mhz [freq >= %1.3f Mhz]\n",
			ffreq*BASE_FREQ, ffreq_min*BASE_FREQ/(1<<max_n2));
		ffreq = ffreq_min / (1<<max_n2);
	}
	if (ffreq > ffreq_max / (1<<min_n2)) {
		ErrorF("invalid frequency %1.3F Mhz [freq <= %1.3f Mhz]\n",
			ffreq*BASE_FREQ, ffreq_max*BASE_FREQ/(1<<min_n2));
		ffreq = ffreq_max / (1<<min_n2);
	}

	best_diff = ffreq;

	for(n2=min_n2; n2<=max_n2; n2++) {
		for(n1=min_n1+2; n1<=max_n1+2; n1++) {
			m = (int)(ffreq*n1*(1<<n2)+0.5);
			if (m<min_m+2 || m > 127+2)
				continue;
			div = (double)(m)/(double)(n1);
			if ((div >= ffreq_min) &&
			    (div <= ffreq_max)) {
				diff = ffreq - div / (1<<n2);
				if (diff < 0.0)
					diff = -diff;
				if (diff < best_diff) {
					best_diff = diff;
					best_m = m;
					best_n1 = n1;
					best_n2 = n2;
				}
			}
		}
	}

	if (max_n1 == 63)
		*ndiv = (best_n1 - 2) | (best_n2 << 6);
	else
		*ndiv = (best_n1 - 2) | (best_n2 << 5);
	*mdiv = best_m - 2;
}


static void S3TrioSetPLL(ScrnInfoPtr pScrn, int clk, unsigned char m,
		  unsigned char n)
{
	unsigned char tmp;
	int index2;

	if (clk < 2) {
		tmp = inb(0x3cc);
		outb(0x3c2, (tmp & 0xf3) | (clk << 2));
	} else {
		tmp = inb(0x3cc);
		outb(0x3c2, tmp | 0x0c);

		outb(0x3c4, 0x08);
		outb(0x3c5, 0x06);	/* unlock extended CR9-18 */

		if (clk != 10) {
			outb(0x3c4, 0x12);
			outb(0x3c5, n);
			outb(0x3c4, 0x13);
			outb(0x3c5, m);

			outb(0x3c4, 0x15);
			tmp = inb(0x3c5) & ~0x21;
			outb(0x3c5, tmp | 0x02);
			outb(0x3c5, tmp | 0x22);
			outb(0x3c5, tmp | 0x02);
		} else {
			index2 = 0x10;
			outb(0x3c4, 0x10);
			outb(0x3c5, n);
			outb(0x3c4, 0x11);
			outb(0x3c5, m);
			outb(0x3c4, 0x1a);
			outb(0x3c5, n);

			outb(0x3c4, 0x15);
			tmp = inb(0x3c5) & ~0x21;
			outb(0x3c5, tmp | 0x01);
			outb(0x3c5, tmp | 0x21);
			outb(0x3c5, tmp | 0x01);
			outb(0x3c5, tmp);
		}

		outb(0x3c4, 0x08);
		outb(0x3c5, 0x00);	/* lock em */
	}
}


static void S3TrioSetClock(ScrnInfoPtr pScrn, long freq, int clk, int min_m,
		    int min_n1, int max_n1, int min_n2, int max_n2,
		    int pll_type, long freq_min, long freq_max)
{
	unsigned char m, n;

	S3TrioCalcClock(freq, min_m, min_n1, max_n1, min_n2, max_n2,
			freq_min, freq_max, &m, &n);

	/* XXX for pll_type == TRIO */
	S3TrioSetPLL(pScrn, clk, m, n);
}

void S3Trio64DAC_PreInit(ScrnInfoPtr pScrn)
{
	S3Ptr pS3 = S3PTR(pScrn);
	unsigned char SR8, SR27, SR28;
	int m, n, n1, n2, mclk;

	outb(0x3c4, 0x08);
	SR8 = inb(0x3c5);
	outb(0x3c5, 0x06);

	outb(0x3c4, 0x11);
	m = inb(0x3c5);
	outb(0x3c4, 0x10);
	n = inb(0x3c5);

	m &= 0x7f;
	n1 = n & 0x1f;
	n2 = (n >> 5) & 0x03;
	mclk = ((1431818 * (m+2)) / (n1+2) / (1<<n2)+50)/100;
	if (pS3->Chipset == PCI_CHIP_AURORA64VP) {
		outb(0x3c4, 0x27);
		SR27 = inb(0x3c5);
		outb(0x3c4, 0x28);
		SR28 = inb(0x3c5);
		mclk /= ((SR27 >> 2) & 0x03) + 1;
	}
	pS3->mclk = mclk;

	outb(0x3c4, 0x08);
	outb(0x3c5, SR8);

	xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "MCLK %1.3f Mhz\n",
		   mclk / 1000.0);
}


void S3Trio64DAC_Init(ScrnInfoPtr pScrn, DisplayModePtr mode)
{
	S3Ptr pS3 = S3PTR(pScrn);
	int pixmux=0, invert_vclk=0, sr8, sr15, sr18, cr33;
	unsigned char blank, tmp;

	if (pS3->Chipset == PCI_CHIP_AURORA64VP)
		S3TrioSetClock(pScrn, mode->Clock, 2, 1, 1, 63, 0, 3, 2,
			       135000, 270000);
	else
		S3TrioSetClock(pScrn, mode->Clock, 2, 1, 1, 31, 0, 3, 2,
			       135000, 270000);

	outb(0x3c4, 1);
	blank = inb(0x3c5);
	outb(0x3c5, blank | 0x20);	/* blank the screen */

	outb(0x3c4, 0x08);
	sr8 = inb(0x3c5);
	outb(0x3c5, 0x06);

	outb(0x3c4, 0x0d0);
	tmp = inb(0x3c5) & ~1;
	outb(0x3c5, tmp);

	outb(0x3c4, 0x15);
	sr15 = inb(0x3c5) & ~0x10;

	outb(0x3c4, 0x18);
	sr18 = inb(0x3c5) & ~0x80;
	outb(pS3->vgaCRIndex, 0x33);
	cr33 = inb(pS3->vgaCRReg) & ~0x28;

	/* ! pixmux */
	switch (pScrn->depth) {
	case 8:
		break;
	case 15:
		cr33 |= 0x08;
		pixmux = 0x30;
		break;
	case 16:
		cr33 |= 0x08;
		pixmux = 0x50;
		break;
	case 32:
		pixmux = 0xd0;
		break;
	}

	outb(pS3->vgaCRReg, cr33);

	outb(pS3->vgaCRIndex, 0x67);
	outb(pS3->vgaCRReg, pixmux | invert_vclk);

	outb(0x3c4, 0x15);
	outb(0x3c5, sr15);
	outb(0x3c4, 0x18);
	outb(0x3c5, sr18);

	if (pS3->Chipset == PCI_CHIP_AURORA64VP) {
		outb(0x3c4, 0x28);
		outb(0x3c5, 0x00);
	}

	outb(0x3c4, 0x08);
	outb(0x3c5, sr8);

	outb(0x3c4, 1);
	outb(0x3c5, blank);	/* unblank the screen */
}

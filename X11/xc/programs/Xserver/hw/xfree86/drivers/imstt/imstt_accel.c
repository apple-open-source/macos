/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/imstt/imstt_accel.c,v 1.6 2001/05/16 06:48:09 keithp Exp $ */

/*
 *	Copyright 2000	Ani Joshi <ajoshi@unixbox.com>
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
 */


#include "Xarch.h"
#include "xf86.h"
#include "xf86_ansic.h"
#include "compiler.h"
#include "xf86_OSproc.h"
#include "xaa.h"
#include "xf86PciInfo.h"

#include "imstt.h"
#include "imstt_reg.h"



static void IMSTTSync(ScrnInfoPtr pScrn)
{
	IMSTTPtr iptr = IMSTTPTR(pScrn);
/*	IMSTTMMIO_VARS(); */

	while(INREG(IMSTT_SSTATUS) & 0x80);
	while(INREG(IMSTT_SSTATUS) & 0x40);
	
	return;
}


static void IMSTTSetupForSolidFill(ScrnInfoPtr pScrn, int color,
				   int rop, unsigned int planemask)
{
	IMSTTPtr iptr = IMSTTPTR(pScrn);
/*	IMSTTMMIO_VARS(); */

	switch (pScrn->depth) {
		case 8:
			iptr->color = color | (color << 8) | (color << 16) | (color << 24);
			break;
		case 15:
		case 16:
			iptr->color = color | (color << 8) | (color << 16);
			break;
		default:
			iptr->color = color;
			break;
	}
}


static void IMSTTSubsequentSolidFillRect(ScrnInfoPtr pScrn,
					 int x, int y, int w, int h)
{
	IMSTTPtr iptr = IMSTTPTR(pScrn);
/*	IMSTTMMIO_VARS(); */

	x *= (pScrn->bitsPerPixel >> 3);
	y *= iptr->ll;
	w *= (pScrn->bitsPerPixel >> 3);
	h--;
	w--;

	while(INREG(IMSTT_SSTATUS) & 0x80);
	OUTREG(IMSTT_DSA, x + y);
	OUTREG(IMSTT_CNT, (h << 16) | w);
	OUTREG(IMSTT_DP_OCTL, iptr->ll);
	OUTREG(IMSTT_SP, iptr->ll);
	OUTREG(IMSTT_BI, 0xffffffff);
	OUTREG(IMSTT_MBC, 0xffffffff);
	OUTREG(IMSTT_CLR, iptr->color);

	if (iptr->rev == 2)
		OUTREG(IMSTT_BLTCTL, 0x200000);
	else
		OUTREG(IMSTT_BLTCTL, 0x840);

	while(INREG(IMSTT_SSTATUS) & 0x80);
	while(INREG(IMSTT_SSTATUS) & 0x40);
}


static void IMSTTSetupForScreenToScreenCopy(ScrnInfoPtr pScrn, int xdir,
					    int ydir, int rop, unsigned int planemask,
					    int trans_color)
{
	IMSTTPtr iptr = IMSTTPTR(pScrn);
/*	IMSTTMMIO_VARS(); */
	unsigned long sp, dp, ll;

	iptr->bltctl = 0x05;

	ll = pScrn->displayWidth * (pScrn->bitsPerPixel >> 3);
	ll = iptr->ll;

	sp = ll << 16;

	if (xdir < 0) {
		iptr->bltctl |= 0x80;
		iptr->cnt = 1;
	} else {
		iptr->cnt = 0;
	}

	if (ydir < 0) {
		sp |= -(ll) & 0xffff;
		dp = -(ll) & 0xffff;
		iptr->ydir = 1;
	} else {
		sp |= ll;
		dp = ll;
		iptr->ydir = 0;
	}

	iptr->sp = sp;
	iptr->dp = dp;
	iptr->ll = ll;
}


static void IMSTTSubsequentScreenToScreenCopy(ScrnInfoPtr pScrn,
					      int x1, int y1,
					      int x2, int y2,
					      int w, int h)
{
	IMSTTPtr iptr = IMSTTPTR(pScrn);
/*	IMSTTMMIO_VARS(); */
	unsigned long cnt;

	x1 *= (pScrn->bitsPerPixel >> 3);
	x2 *= (pScrn->bitsPerPixel >> 3);
	w *= (pScrn->bitsPerPixel >> 3);
	w--;
	h--;
	cnt = h << 16;

	if (iptr->cnt) {
		x1 += w;
		x2 += w;
		cnt |= -(w) & 0xffff;
	}
	else
		cnt |= w;

	if (iptr->ydir) {
		y1 += h;
		y2 += h;
	}

	OUTREG(IMSTT_S1SA, y1 * iptr->ll + x1);
	OUTREG(IMSTT_SP, iptr->sp);
	OUTREG(IMSTT_DSA, y2 * iptr->ll + x2);
	OUTREG(IMSTT_CNT, cnt);
	OUTREG(IMSTT_DP_OCTL, iptr->dp);
	OUTREG(IMSTT_BLTCTL, iptr->bltctl);
	while(INREG(IMSTT_SSTATUS) & 0x80);
	while(INREG(IMSTT_SSTATUS) & 0x40);
}



Bool IMSTTAccelInit(ScreenPtr pScreen)
{
	ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
	IMSTTPtr iptr = IMSTTPTR(pScrn);
	XAAInfoRecPtr xaaptr;

	if (!(xaaptr = iptr->AccelInfoRec = XAACreateInfoRec()))
		return FALSE;

	iptr->ll = pScrn->displayWidth * (pScrn->bitsPerPixel >> 3);

	switch (pScrn->bitsPerPixel) {
		case 16:
			iptr->screen_width = iptr->pitch >> 1;
			break;
		case 24:
		case 32:
			iptr->screen_width = iptr->pitch >> 2;
			break;
		default:
			iptr->screen_width = iptr->pitch = iptr->ll;
			break;
	}

	xaaptr->Flags = (PIXMAP_CACHE | OFFSCREEN_PIXMAPS | LINEAR_FRAMEBUFFER);

	xaaptr->Sync = IMSTTSync;

	if (pScrn->bitsPerPixel == 8) {
		/* FIXME fills are broken > 8bpp, iptr->color needs to be setup right */
		xaaptr->SetupForSolidFill = IMSTTSetupForSolidFill;
		xaaptr->SubsequentSolidFillRect = IMSTTSubsequentSolidFillRect;
	}

	xaaptr->ScreenToScreenCopyFlags = NO_TRANSPARENCY;
	xaaptr->SetupForScreenToScreenCopy = IMSTTSetupForScreenToScreenCopy;
	xaaptr->SubsequentScreenToScreenCopy = IMSTTSubsequentScreenToScreenCopy;

	return XAAInit(pScreen, xaaptr);
}


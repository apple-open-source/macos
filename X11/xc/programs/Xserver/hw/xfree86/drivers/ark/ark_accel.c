/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/ark/ark_accel.c,v 1.7 2002/12/11 17:09:39 dawes Exp $ */
/*
 *      Copyright 2000  Ani Joshi <ajoshi@unixbox.com>
 *
 *      XFree86 4.x driver for ARK Logic chipset
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
 *      Based on the 3.3.x driver by:
 *              Harm Hanemaayer <H.Hanemaayer@inter.nl.net>
 *
 */


#include "Xarch.h"
#include "xf86.h"
#include "xf86_ansic.h"
#include "xf86_OSproc.h"
#include "xaa.h"
#include "xf86PciInfo.h"
#include "compiler.h"

#include "ark.h"
#include "ark_reg.h"


static int curx, cury, cmd_flags;


static void ARKSync(ScrnInfoPtr pScrn)
{
	IOADDRESS port = pScrn->domainIOBase + 0x3cb;

	for (;;) {
		if (!(inb(port) & 0x40))
			break;
	}
}


static void ARKSetupForSolidFill(ScrnInfoPtr pScrn, int color,
				 int rop, unsigned int planemask)
{
	ARKPtr pARK = ARKPTR(pScrn);

	OUTREG16(FG_COLOR, color);
	/* ARK color mix matches X raster-ops */
	OUTREG16(COLOR_MIX_SEL, (rop | (rop << 8)));
	switch (pScrn->bitsPerPixel) {
		case 8:
			if ((planemask & 0xff) == 0xff)
				cmd_flags = DISABLE_PLANEMASK;
			else {
				OUTREG16(WRITE_PLANEMASK, planemask);
				cmd_flags = 0;
			}
			break;
		case 16:
			if ((planemask & 0xffff) == 0xffff)
				cmd_flags = DISABLE_PLANEMASK;
			else {
				OUTREG16(WRITE_PLANEMASK, planemask);
				cmd_flags = 0;
			}
			break;
		case 32:
			OUTREG16(FG_COLOR_HI, color >> 16);
			if ((planemask & 0xffffff) == 0xffffff)
				cmd_flags = DISABLE_PLANEMASK;
			else {
				OUTREG16(WRITE_PLANEMASK, planemask);
				cmd_flags = 0;
			}
			break;
	}

	curx = cury = -1;
}


static void ARKSubsequentSolidFillRect(ScrnInfoPtr pScrn, int x,
				       int y, int w, int h)
{
	ARKPtr pARK = ARKPTR(pScrn);
	int dst_addr;

	OUTREG(WIDTH, ((h - 1) << 16) | (w - 1));
	if (x != curx || y != cury) {
		dst_addr = y * pScrn->displayWidth + x;
		OUTREG(DST_ADDR, dst_addr);
		curx = x;
		cury = y;
	}
	OUTREG16(COMMAND, SELECT_BG_COLOR | SELECT_FG_COLOR |
			  STENCIL_ONES | DISABLE_CLIPPING | BITBLT |
			  cmd_flags);
	cury += h;
}



static void ARKSetupForScreenToScreenCopy(ScrnInfoPtr pScrn, int xdir,
					  int ydir, int rop, unsigned int planemask,
					  int trans_color)
{
	ARKPtr pARK = ARKPTR(pScrn);

	cmd_flags = 0;
	if (trans_color != -1) {
		if (pScrn->bitsPerPixel <= 16)
			OUTREG16(TRANS_COLOR, trans_color);
		else {
			OUTREG16(TRANS_COLOR, trans_color & 0xffff);
			OUTREG16(TRANS_COLOR_HI, trans_color >> 16);
		}
		cmd_flags = STENCIL_GENERATED;
		OUTREG16(COLOR_MIX_SEL, rop | 0x0500);
	} else {
		OUTREG16(COLOR_MIX_SEL, rop | (rop << 8));
	}

	if (ydir < 0)
		cmd_flags |= UP;
	if (xdir < 0)
		cmd_flags |= LEFT;

	/* yes, quite ugly */
	if ((pScrn->bitsPerPixel == 8 && (planemask & 0xff) == 0xff) ||
	    (pScrn->bitsPerPixel == 16 && (planemask & 0xffff) == 0xffff) ||
	    (pScrn->bitsPerPixel == 32 && (planemask & 0xffffff) == 0xffffff))
		cmd_flags |= DISABLE_PLANEMASK;
	else
		OUTREG16(WRITE_PLANEMASK, planemask);

}



static void ARKSubsequentScreenToScreenCopy(ScrnInfoPtr pScrn,
					    int x1, int y1,
					    int x2, int y2,
					    int w, int h)
{
	ARKPtr pARK = ARKPTR(pScrn);
	int src_addr, dst_addr;

	if (cmd_flags & UP) {
		src_addr = (y1 + h - 1) * pScrn->displayWidth;
		dst_addr = (y2 + h - 1) * pScrn->displayWidth;
	} else {
		src_addr = y1 * pScrn->displayWidth;
		dst_addr = y2 * pScrn->displayWidth;
	}
	if (cmd_flags & LEFT) {
		src_addr += x1 + w - 1;
		dst_addr += x2 + w - 1;
	} else {
		src_addr += x1;
		dst_addr += x2;
	}

	OUTREG(SRC_ADDR, src_addr);
	OUTREG(DST_ADDR, dst_addr);
	OUTREG(WIDTH, ((h - 1) << 16) | (w - 1));
	OUTREG16(COMMAND, BG_BITMAP | FG_BITMAP | DISABLE_CLIPPING |
			  BITBLT | cmd_flags);
}



Bool ARKAccelInit(ScreenPtr pScreen)
{
	ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
	ARKPtr pARK = ARKPTR(pScrn);
	XAAInfoRecPtr pXAA;

	if (!(pXAA = XAACreateInfoRec()))
		return FALSE;

	pXAA->Flags = LINEAR_FRAMEBUFFER;

	pXAA->Sync = ARKSync;
	pXAA->SetupForSolidFill = ARKSetupForSolidFill;
	pXAA->SubsequentSolidFillRect = ARKSubsequentSolidFillRect;
	pXAA->ScreenToScreenCopyFlags = 0;
	pXAA->SetupForScreenToScreenCopy = ARKSetupForScreenToScreenCopy;
	pXAA->SubsequentScreenToScreenCopy = ARKSubsequentScreenToScreenCopy;

	OUTREG16(COLOR_MIX_SEL, 0x0303);
	if (pARK->Chipset == PCI_CHIP_1000PV) {
		OUTREG16(WRITE_PLANEMASK, 0xffff);
		OUTREG16(TRANS_COLOR_MSK, 0xffff);
	} else {
		OUTREG16(TRANS_COLOR, 0xffff);
		OUTREG16(TRANS_COLOR, 0xffffffff >> 16);
	}
	if (pARK->Chipset == PCI_CHIP_1000PV && pScrn->bitsPerPixel == 32) {
		OUTREG16(STENCIL_PITCH, pScrn->displayWidth * 2);
		OUTREG16(SRC_PITCH, pScrn->displayWidth * 2);
		OUTREG16(DST_PITCH, pScrn->displayWidth * 2);
	} else {
		OUTREG16(STENCIL_PITCH, pScrn->displayWidth);
		OUTREG16(SRC_PITCH, pScrn->displayWidth);
		OUTREG16(DST_PITCH, pScrn->displayWidth);
	}

	OUTREG16(BITMAP_CONFIG, LINEAR_STENCIL_ADDR | LINEAR_SRC_ADDR |
				LINEAR_DST_ADDR);

	return XAAInit(pScreen, pXAA);
}

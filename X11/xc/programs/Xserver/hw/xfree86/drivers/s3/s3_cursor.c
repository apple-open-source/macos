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
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/s3/s3_cursor.c,v 1.1 2001/07/02 10:46:04 alanh Exp $ */

#include "xf86.h"
#include "xf86_OSproc.h"
#include "xf86_ansic.h"

#include "compiler.h"

#include "s3.h"
#include "s3_reg.h"


static void S3SetCursorColors(ScrnInfoPtr pScrn, int bg, int fg)
{
	S3Ptr pS3 = S3PTR(pScrn);
	int vgaCRIndex = pS3->vgaCRIndex, vgaCRReg = pS3->vgaCRReg;
	unsigned short packfg, packbg;

	switch (pS3->s3Bpp) {
	case 1:
		/* XXX Trio series only */
		outb(vgaCRIndex, 0x45);
		inb(vgaCRReg);
		outb(vgaCRIndex, 0x4a);
		outb(vgaCRReg, fg);
		outb(vgaCRReg, fg);

		outb(vgaCRIndex, 0x45);
		inb(vgaCRReg);
		outb(vgaCRIndex, 0x4b);
		outb(vgaCRReg, bg);
		outb(vgaCRReg, bg);

		break;
	case 2:
		/* XXX depth 16 */
		packfg = ((fg & 0x00f80000) >> 19) | ((fg & 0x0000fc00) >> 5) |
			 ((fg & 0x000000f8) << 8);
		packbg = ((bg & 0x00f80000) >> 19) | ((bg & 0x0000fc00) >> 5) |
			 ((bg & 0x000000f8) << 8);

		outb(vgaCRIndex, 0x45);
		inb(vgaCRReg);
		outb(vgaCRIndex, 0x4a);
		outb(vgaCRReg, packfg);
		outb(vgaCRReg, packfg >> 8);

		outb(vgaCRIndex, 0x45);
		inb(vgaCRReg);
		outb(vgaCRIndex, 0x4b);
		outb(vgaCRReg, packbg);
		outb(vgaCRReg, packbg >> 8);

		break;
	default:
		outb(vgaCRIndex, 0x45);
		inb(vgaCRReg);
		outb(vgaCRIndex, 0x4a);
		outb(vgaCRReg, (fg & 0x00ff0000) >> 16);
		outb(vgaCRReg, (fg & 0x0000ff00) >> 8);
		outb(vgaCRReg, (fg & 0x000000ff));

		outb(vgaCRIndex, 0x45);
		inb(vgaCRReg);
		outb(vgaCRIndex, 0x4b);
		outb(vgaCRReg, (bg & 0x00ff0000) >> 16);
		outb(vgaCRReg, (bg & 0x0000ff00) >> 8);
		outb(vgaCRReg, (bg & 0x000000ff));

		break;
	}
}


static void S3SetCursorPosition(ScrnInfoPtr pScrn, int x, int y)
{
	S3Ptr pS3 = S3PTR(pScrn);
	int vgaCRIndex = pS3->vgaCRIndex, vgaCRReg = pS3->vgaCRReg;

	outb(vgaCRIndex, 0x39);
	outb(vgaCRReg, 0xa5);

	outb(vgaCRIndex, 0x46);
	outb(vgaCRReg, x >> 8);
	outb(vgaCRIndex, 0x47);
	outb(vgaCRReg, x);

	outb(vgaCRIndex, 0x49);
	outb(vgaCRReg, y);
	outb(vgaCRIndex, 0x48);
	outb(vgaCRReg, y >> 8);
}


static void S3HideCursor(ScrnInfoPtr pScrn)
{
	S3Ptr pS3 = S3PTR(pScrn);
	int vgaCRIndex = pS3->vgaCRIndex, vgaCRReg = pS3->vgaCRReg;
	unsigned char tmp;

	outb(vgaCRIndex, 0x45);
	tmp = inb(vgaCRReg);
	outb(vgaCRReg, tmp & ~0x01);
}


static void S3ShowCursor(ScrnInfoPtr pScrn)
{
	S3Ptr pS3 = S3PTR(pScrn);
	int vgaCRIndex = pS3->vgaCRIndex, vgaCRReg = pS3->vgaCRReg;
	unsigned char tmp;

	outb(vgaCRIndex, 0x39);
	outb(vgaCRReg, 0xa5);

	outb(vgaCRIndex, 0x55);
	tmp = inb(vgaCRReg);
	outb(vgaCRReg, tmp | 0x10);

	outb(vgaCRIndex, 0x4c);
	outb(vgaCRReg, pS3->FBCursorOffset >> 8);
	outb(vgaCRIndex, 0x4d);
	outb(vgaCRReg, pS3->FBCursorOffset);

	outb(vgaCRIndex, 0x45);
	tmp = inb(vgaCRReg);
	outb(vgaCRReg, tmp | 0x01);
}	


static void S3LoadCursorImage(ScrnInfoPtr pScrn, unsigned char *image)
{
	S3Ptr pS3 = S3PTR(pScrn);
	int vgaCRIndex = pS3->vgaCRIndex, vgaCRReg = pS3->vgaCRReg;
	unsigned char cr45;

	outb(vgaCRIndex, 0x39);
	outb(vgaCRReg, 0xa5);

	WaitIdle();

	VerticalRetraceWait();

	outb(vgaCRIndex, 0x45);
	cr45 = inb(vgaCRReg);
	outb(vgaCRReg, cr45 & 0xfe);

	outb(vgaCRIndex, 0x46);
	outb(vgaCRReg, 0xff);
	outb(vgaCRIndex, 0x47);
	outb(vgaCRReg, 0x7f);
	outb(vgaCRIndex, 0x49);
	outb(vgaCRReg, 0xff);
	outb(vgaCRIndex, 0x4e);
	outb(vgaCRReg, 0x3f);
	outb(vgaCRIndex, 0x4f);
	outb(vgaCRReg, 0x3f);
	outb(vgaCRIndex, 0x48);
	outb(vgaCRReg, 0x7f);

	memcpy(pS3->FBBase + (pS3->FBCursorOffset * 1024), image, 1024);

	VerticalRetraceWait();

	outb(vgaCRIndex, 0x45);
	outb(vgaCRReg, cr45);
}


static Bool S3UseHWCursor(ScreenPtr pScreen, CursorPtr pCurs)
{
	return TRUE;
}


Bool S3_CursorInit(ScreenPtr pScreen)
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
		       HARDWARE_CURSOR_BIT_ORDER_MSBFIRST;

	pCurs->SetCursorColors = S3SetCursorColors;
	pCurs->SetCursorPosition = S3SetCursorPosition;
	pCurs->LoadCursorImage = S3LoadCursorImage;
	pCurs->HideCursor = S3HideCursor;
	pCurs->ShowCursor = S3ShowCursor;
	pCurs->UseHWCursor = S3UseHWCursor;

	return xf86InitCursor(pScreen, pCurs);
}


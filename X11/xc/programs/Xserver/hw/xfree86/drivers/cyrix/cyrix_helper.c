/*
 * Copyright 2000 by Richard A. Hecker, California, United States
 * Copyright 2002 by Red Hat Inc.
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
 * RED HAT DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL RICHARD HECKER BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 *
 * Author:  Richard Hecker, hecker@cat.dfrc.nasa.gov
 *          Re-written for XFree86 v4.0
 *
 * Chunks re-written again for XFree86 v4.2
 *	    Alan Cox <alan@redhat.com>
 *
 * Previous driver (pre-XFree86 v4.0) by
 *          Annius V. Groenink (A.V.Groenink@zfc.nl, avg@cwi.nl),
 *          Dirk H. Hohndel (hohndel@suse.de),
 *          Portions: the GGI project & confidential CYRIX databooks.
 */
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/cyrix/cyrix_helper.c,v 1.5 2003/09/24 02:43:21 dawes Exp $ */

#include "cyrix.h"
#include "vgaHW.h"
#include "xf86_ansic.h"
#include "compiler.h"
#include "xf86.h"

#define CYRIXmarkLinesDirty { \
                               int k = 0; \
                               while (k < 1024) { \
	                         GX_REG(MC_DR_ADD) = k++; \
                                 GX_REG(MC_DR_ACC) = 0; \
                               } \
                            }

static void CYRIXresetVGA(ScrnInfoPtr pScrn, unsigned long vgaIOBase);

void  Cyrix1bppColorMap(ScrnInfoPtr pScrn)
{	/* use dedicated color map routines on new chipsets in 8bpp */
	xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "Cyrix 1BPP is only a stub for now.\n");
	return;
}


int CyrixHWCursor(ScreenPtr pScreen)
{
	return 1024;
}

void CyrixRestore(ScrnInfoPtr pScrn, CYRIXRegPtr cyrixReg)
{
	unsigned char temp;
	CYRIXPrvPtr pCyrix;
	vgaHWPtr hwp;
	vgaRegPtr vgaReg;
	unsigned long vgaIOBase;

	vgaHWProtect(pScrn, TRUE);		/* Blank the screen */
	pCyrix = CYRIXPTR(pScrn);
/*	vgaReg = &VGAHWPTR(pScrn)->SavedReg; */
	hwp = VGAHWPTR(pScrn);
	vgaHWUnlock(hwp);
	vgaReg = &hwp->ModeReg;

	vgaIOBase = VGAHWPTR(pScrn)->IOBase;

	/* restore standard VGA portion */
	outb(vgaIOBase + 4, CrtcModeSwitchControl);
        outb(vgaIOBase + 5, 0x01);
	CYRIXresetVGA(pScrn,vgaIOBase);
	vgaHWRestore(pScrn, vgaReg, VGA_SR_ALL);

	vgaHWProtect(pScrn, TRUE);		/* Blank the screen */

	CYRIXmarkLinesDirty;

	/* restore miscellaneous output registers */
	outb(0x3C2, vgaReg->MiscOutReg);

	/* restore SoftVGA extended registers */
	outb(vgaIOBase + 4, CrtcDriverControl);
	temp = inb(vgaIOBase + 5);
	outb(vgaIOBase + 5, (pCyrix->PrevExt.DriverControl & 0x01)
	                  | (temp & 0xfe));

	outb(vgaIOBase + 4, CrtcVerticalTimingExtension);
	temp = inb(vgaIOBase + 5);
	outb(vgaIOBase + 5, (pCyrix->PrevExt.VerticalTimingExtension & 0x55)
	                  | (temp & 0xaa));

	outb(vgaIOBase + 4, CrtcExtendedAddressControl);
	temp = inb(vgaIOBase + 5);
	outb(vgaIOBase + 5, (pCyrix->PrevExt.ExtendedAddressControl & 0x07)
	                  | (temp & 0xf8));

	outb(vgaIOBase + 4, 19);
	outb(vgaIOBase + 5, pCyrix->PrevExt.Offset);
	
	outb(vgaIOBase + 4, CrtcExtendedOffset);
	temp = inb(vgaIOBase + 5);
	outb(vgaIOBase + 5, ((pCyrix->PrevExt.ExtendedOffset) & 0x03)
	                  | (temp & 0xfc));

	outb(vgaIOBase + 4, CrtcExtendedColorControl);
	temp = inb(vgaIOBase + 5);
	outb(vgaIOBase + 5, (pCyrix->PrevExt.ExtendedColorControl & 0x07)
	                  | (temp & 0xf8));

	outb(vgaIOBase + 4, CrtcDisplayCompression);
	temp = inb(vgaIOBase + 5);
	outb(vgaIOBase + 5, (pCyrix->PrevExt.DisplayCompression & 0x0f)
	                  | (temp & 0xf0));

	outb(vgaIOBase + 4, CrtcDACControl);
	temp = inb(vgaIOBase + 5);
	outb(vgaIOBase + 5, (pCyrix->PrevExt.DACControl & 0x0e)
	                  | (temp & 0xf1));

	outb(vgaIOBase + 4, CrtcModeSwitchControl);
        outb(vgaIOBase + 5, 0x00);

	/* let SoftVGA programming settle before we access DC registers,
	   but don't wait too long */
	usleep(1000);
	CYRIXmarkLinesDirty;

	/* restore display controller hardware registers */
#ifndef MONOVGA
#define DCFG_MASK       (DC_GCFG_FDTY | DC_GCFG_DECE | DC_GCFG_CMPE | DC_GCFG_FBLC | DC_GCFG_CURE)
#define GPBS_MASK       (BC_16BPP | BC_FB_WIDTH_2048)

	GX_REG(DC_UNLOCK) = DC_UNLOCK_VALUE;

	GX_REG(DC_CURS_ST_OFFSET) = pCyrix->PrevExt.DcCursStOffset;
	GX_REG(DC_CB_ST_OFFSET)  = pCyrix->PrevExt.DcCbStOffset;
	GX_REG(DC_LINE_DELTA)    = (GX_REG(DC_LINE_DELTA) & 0xFFC00FFF)
	                         | (pCyrix->PrevExt.DcLineDelta & 0x003FF000);
	GX_REG(DC_BUF_SIZE)      = (GX_REG(DC_BUF_SIZE) & 0xFFFF01FF)
                                 | (pCyrix->PrevExt.DcBufSize & 0x0000FE00);
	GX_REG(DC_CURSOR_X)      = pCyrix->PrevExt.DcCursorX;
	GX_REG(DC_CURSOR_Y)      = pCyrix->PrevExt.DcCursorY;
	GX_REG(DC_CURSOR_COLOR)  = pCyrix->PrevExt.DcCursorColor;

	GX_REG(DC_GENERAL_CFG)   = (GX_REG(DC_GENERAL_CFG) & (~DCFG_MASK))
	                         | (pCyrix->PrevExt.DcGeneralCfg & DCFG_MASK);

	GX_REG(DC_UNLOCK) = 0;

	GX_REG(GP_BLIT_STATUS)   = (GX_REG(GP_BLIT_STATUS) & (~GPBS_MASK))
	                         | (pCyrix->PrevExt.GpBlitStatus & GPBS_MASK);

#endif

	vgaHWProtect(pScrn, FALSE);		/* Turn on screen */
}

void *
CyrixSave(ScrnInfoPtr pScrn, CYRIXRegPtr cyrixReg)
{
	CYRIXPrvPtr	pCyrix;
	vgaRegPtr vgaReg;
	unsigned long	vgaIOBase;

#ifndef MONOVGA
	/* If we don't turn on the screen we end up restoring a 
	   blanked display on some boxes whose APM is a bit too smart.
	   Save the display turned -on- therefore */
	   
	vgaHWProtect(pScrn, FALSE);		/* Turn on screen */
	
	/* save graphics pipeline registers */
	pCyrix = CYRIXPTR(pScrn);
	vgaIOBase = VGAHWPTR(pScrn)->IOBase;
	vgaReg = &VGAHWPTR(pScrn)->SavedReg;
	pCyrix->PrevExt.GpBlitStatus   = GX_REG(GP_BLIT_STATUS);

	/* save miscellaneous output registers */
	vgaReg->MiscOutReg   = inb(0x3CC);

	/* save graphics pipeline registers */
	GX_REG(DC_UNLOCK)  = DC_UNLOCK_VALUE;
	pCyrix->PrevExt.DcGeneralCfg   = GX_REG(DC_GENERAL_CFG);
	pCyrix->PrevExt.DcCursStOffset = GX_REG(DC_CURS_ST_OFFSET);
	pCyrix->PrevExt.DcCbStOffset   = GX_REG(DC_CB_ST_OFFSET);
	pCyrix->PrevExt.DcLineDelta    = GX_REG(DC_LINE_DELTA);
	pCyrix->PrevExt.DcBufSize      = GX_REG(DC_BUF_SIZE);
	pCyrix->PrevExt.DcCursorX      = GX_REG(DC_CURSOR_X);
	pCyrix->PrevExt.DcCursorY      = GX_REG(DC_CURSOR_Y);
	pCyrix->PrevExt.DcCursorColor  = GX_REG(DC_CURSOR_COLOR);
	GX_REG(DC_UNLOCK)  = 0;

#endif

	/* save SoftVGA pCyrix->PrevExt.nded registers */
	outb(vgaIOBase + 4, CrtcVerticalTimingExtension);
	pCyrix->PrevExt.VerticalTimingExtension = inb(vgaIOBase + 5);

	outb(vgaIOBase + 4, CrtcExtendedAddressControl);
	pCyrix->PrevExt.ExtendedAddressControl = inb(vgaIOBase + 5);

	outb(vgaIOBase + 4, 19);
	pCyrix->PrevExt.Offset = inb(vgaIOBase + 5);
	
	outb(vgaIOBase + 4, CrtcExtendedOffset);
	pCyrix->PrevExt.ExtendedOffset = inb(vgaIOBase + 5);

	outb(vgaIOBase + 4, CrtcExtendedColorControl);
	pCyrix->PrevExt.ExtendedColorControl = inb(vgaIOBase + 5);

	outb(vgaIOBase + 4, CrtcDisplayCompression);
	pCyrix->PrevExt.DisplayCompression = inb(vgaIOBase + 5);

	outb(vgaIOBase + 4, CrtcDriverControl);
	pCyrix->PrevExt.DriverControl = inb(vgaIOBase + 5);

	outb(vgaIOBase + 4, CrtcDACControl);
	pCyrix->PrevExt.DACControl = inb(vgaIOBase + 5);

	outb(vgaIOBase + 4, CrtcClockControl);
	pCyrix->PrevExt.ClockControl = inb(vgaIOBase + 5);

	outb(vgaIOBase + 4, CrtcClockFrequency);
	pCyrix->PrevExt.CrtClockFrequency = inb(vgaIOBase + 5);

	outb(vgaIOBase + 4, CrtcClockFrequencyFraction);
	pCyrix->PrevExt.CrtClockFrequencyFraction = inb(vgaIOBase + 5);

	outb(vgaIOBase + 4, CrtcRefreshRate);
	pCyrix->PrevExt.RefreshRate = inb(vgaIOBase + 5);

	/* save standard VGA portion */
	CYRIXresetVGA(pScrn,vgaIOBase);

	return((void *)vgaIOBase);
}


Bool
CyrixInit(ScrnInfoPtr pScrn, DisplayModePtr mode)
{
	CYRIXPrvPtr	pCyrix;
	int offset_shift = (pScrn->bitsPerPixel == 16) ? 2 :
                           (pScrn->bitsPerPixel == 8) ? 3 : 4;
	int line_offset = pScrn->displayWidth >> offset_shift;

	pCyrix = CYRIXPTR(pScrn);

	/* initialize standard VGA portion */
	if (!vgaHWInit(pScrn, mode))
		return(FALSE);

	/* initialize SoftVGA extended registers */
	pCyrix->PrevExt.VerticalTimingExtension =
		((mode->CrtcVSyncStart & 0x400) >> 4) |
		(((mode->CrtcVDisplay - 1) & 0x400) >> 8) |
		(((mode->CrtcVTotal - 2) & 0x400) >> 10) |
		((mode->CrtcVSyncStart & 0x400) >> 6);

	if (pScrn->bitsPerPixel < 8)
		pCyrix->PrevExt.ExtendedAddressControl = EAC_DIRECT_FRAME_BUFFER;
	else
		pCyrix->PrevExt.ExtendedAddressControl = EAC_DIRECT_FRAME_BUFFER |
		                                       EAC_PACKED_CHAIN4;

	pCyrix->PrevExt.ExtendedOffset = ((line_offset >> 8) & 0x03);
	pCyrix->PrevExt.Offset = line_offset;

	pCyrix->PrevExt.ExtendedColorControl = (pScrn->bitsPerPixel == 16)
                                           ? ECC_16BPP | ECC_565_FORMAT
                                           : ECC_8BPP;

	/* display compression is set using the DC registers */
	pCyrix->PrevExt.DisplayCompression = 0x00;

	/* we drive the palette through the display controller (in new
	   chipsets only) in 8bpp and 16bpp (that is, whenever the
	   hardware cursor is used). */
	if (pScrn->bitsPerPixel < 8)
		pCyrix->PrevExt.DriverControl = 0x00;
	else
		pCyrix->PrevExt.DriverControl = DRVCT_DISPLAY_DRIVER_ACTIVE;

	/* set `16 bit bus' or else compression will hang the
	   system in 16bpp mode */
	if (pScrn->bitsPerPixel == 16)
		pCyrix->PrevExt.DACControl = DACCT_ENABLE_16BIT_BUS;
	else
		pCyrix->PrevExt.DACControl = 0;


#ifndef MONOVGA
	/* initialize masked contents of display controller
	   hardware registers. */
	pCyrix->PrevExt.DcCursStOffset =  pCyrix->CYRIXcursorAddress;
	pCyrix->PrevExt.DcLineDelta   =  0 << 12;
	pCyrix->PrevExt.DcBufSize     =  0x41 << 9;
	pCyrix->PrevExt.DcCursorX     =  0;
	pCyrix->PrevExt.DcCursorY     =  0;
	pCyrix->PrevExt.DcCursorColor =  0;

	/* Compression  is enabled only  when a buffer  was allocated by
	   FbInit  and provided that the displayed screen is the virtual
	   screen.  If the line delta is not 1024 or 2048, entire frames
	   will be flagged dirty as opposed to lines.  Problems with 16bpp
	   and line-dirty flagging seem to have been solved now.  */
	   
	if (pCyrix->NoCompress == FALSE &&
		mode->CrtcVDisplay == pScrn->virtualY &&
		mode->CrtcHDisplay == pScrn->virtualX &&
		0 == GX_REG(DC_FB_ST_OFFSET))
	{
			pCyrix->PrevExt.DcGeneralCfg = DC_GCFG_DECE | DC_GCFG_CMPE;
			if (/* pScrn->bitsPerPixel != 8 ||   -- this is OK now */
			   (pScrn->displayWidth * (pScrn->bitsPerPixel / 8)) & 0x03FF)
				pCyrix->PrevExt.DcGeneralCfg |= DC_GCFG_FDTY;
	}
	else
		pCyrix->PrevExt.DcGeneralCfg = 0;


	/* initialize the graphics pipeline registers */
	pCyrix->PrevExt.GpBlitStatus  =  ((pScrn->displayWidth == 2048) ?
	                                BC_FB_WIDTH_2048 : BC_FB_WIDTH_1024) |
	                               ((pScrn->bitsPerPixel == 16) ?
	                                BC_16BPP : BC_8BPP);
#endif

	return(TRUE);
}


static void
CYRIXresetVGA(ScrnInfoPtr pScrn, unsigned long vgaIOBase)
{
    CYRIXPrvPtr pCyrix = CYRIXPTR(pScrn);
    unsigned char temp;
    /* switch off compression and cursor the hard way */
    GX_REG(DC_UNLOCK)  = DC_UNLOCK_VALUE;
	GX_REG(DC_GENERAL_CFG) &= ~(DC_GCFG_CMPE | DC_GCFG_DECE | DC_GCFG_FDTY | DC_GCFG_CURE);
	GX_REG(DC_UNLOCK)  = 0;
	CYRIXmarkLinesDirty;

	/* reset SoftVGA extensions to standard VGA behaviour */
	outb(vgaIOBase + 4, CrtcExtendedAddressControl);
	temp = inb(vgaIOBase + 5);
	outb(vgaIOBase + 5, temp & 0xf8);
	outb(vgaIOBase + 4, CrtcExtendedStartAddress);
	outb(vgaIOBase + 5, 0x00);
	outb(vgaIOBase + 4, CrtcWriteMemoryAperture);
	temp = inb(vgaIOBase + 5);
	outb(vgaIOBase + 5, temp & 0xe0);
	outb(vgaIOBase + 4, CrtcReadMemoryAperture);
	temp = inb(vgaIOBase + 5);
	outb(vgaIOBase + 5, temp & 0xe0);
	outb(vgaIOBase + 4, CrtcDriverControl);
	temp = inb(vgaIOBase + 5);
	outb(vgaIOBase + 5, temp & 0xfe);
	outb(vgaIOBase + 4, CrtcDisplayCompression);
	temp = inb(vgaIOBase + 5);
	outb(vgaIOBase + 5, temp & 0xf0);
}

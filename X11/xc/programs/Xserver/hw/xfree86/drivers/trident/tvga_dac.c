/*
 * Copyright 1992-1999 by Alan Hourihane, Wigan, England.
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Alan Hourihane not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  Alan Hourihane makes no representations
 * about the suitability of this software for any purpose.  It is provided
 * "as is" without express or implied warranty.
 *
 * ALAN HOURIHANE DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL ALAN HOURIHANE BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 *
 * Author:  Alan Hourihane, alanh@fairlite.demon.co.uk
 */
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/trident/tvga_dac.c,v 1.6 2002/03/29 18:33:29 alanh Exp $ */

#include "xf86.h"
#include "xf86_OSproc.h"
#include "xf86_ansic.h"
#include "xf86Version.h"
#include "xf86PciInfo.h"
#include "xf86Pci.h"

#include "vgaHW.h"

#include "trident.h"
#include "trident_regs.h"

Bool
TVGAInit(ScrnInfoPtr pScrn, DisplayModePtr mode)
{
    TRIDENTPtr pTrident = TRIDENTPTR(pScrn);
    TRIDENTRegPtr pReg = &pTrident->ModeReg;
    int vgaIOBase;
    int offset = 0;
    int clock = mode->Clock;
    vgaIOBase = VGAHWPTR(pScrn)->IOBase;

    OUTB(0x3C4, 0x0B); INB(0x3C5); /* Ensure we are in New Mode */

    pReg->tridentRegsDAC[0x00] = 0x00;
    OUTB(0x3C4, ConfPort2);
    pReg->tridentRegs3C4[ConfPort2] = INB(0x3C5);
    OUTB(0x3CE, MiscExtFunc);
    pReg->tridentRegs3CE[MiscExtFunc] = INB(0x3CF) & 0xF0;
    OUTB(vgaIOBase + 4, FIFOControl);
    pReg->tridentRegs3x4[FIFOControl] = INB(vgaIOBase + 5) | 0x24;

    if (pScrn->bitsPerPixel >= 8) {
       	/* YUK ! here we have to mess with old mode operation */
       	OUTB(0x3C4, 0x0B); OUTB(0x3C5, 0x00); /* Goto Old Mode */
       	OUTB(0x3C4, OldMode2 + NewMode2);
       	pReg->tridentRegs3C4[OldMode2] = 0x10;
       	OUTB(0x3C4, 0x0B); INB(0x3C5); /* Back to New Mode */
   	pReg->tridentRegs3x4[Underline] = 0x40;
	if (pTrident->Chipset < TGUI9440AGi)
	    pReg->tridentRegs3x4[CRTCMode] = 0xA3;
    }

    if (pScrn->videoRam > 512)
    	pReg->tridentRegs3C4[ConfPort2] |= 0x20;
    else
    	pReg->tridentRegs3C4[ConfPort2] &= 0xDF;

    switch (pScrn->bitsPerPixel) {
	case 1:
	case 4:
    	    offset = pScrn->displayWidth >> 4;
	    break;
	case 8:
	    if (pScrn->videoRam < 1024)
    	    	offset = pScrn->displayWidth >> 3;
	    else
    	    	offset = pScrn->displayWidth >> 4;
	    pReg->tridentRegs3CE[MiscExtFunc] |= 0x02;
	    break;
	case 16:
	    pReg->tridentRegs3CE[MiscExtFunc] |= 0x02;
    	    offset = pScrn->displayWidth >> 3;
	    /* Reload with any chipset specific stuff here */
	    if (pTrident->Chipset == TVGA8900D) {
		if (pScrn->depth == 15)
	    	    pReg->tridentRegsDAC[0x00] = 0xA0;
		else
	    	    pReg->tridentRegsDAC[0x00] = 0xE0;
    	    	pReg->tridentRegs3CE[MiscExtFunc] |= 0x08; /* Clock Div by 2*/
	    	clock *= 2;	/* Double the clock */
	    }
	    break;
	case 24:
	    pReg->tridentRegs3CE[MiscExtFunc] |= 0x02;
    	    offset = (pScrn->displayWidth * 3) >> 3;
	    pReg->tridentRegsDAC[0x00] = 0xD0;
	    break;
	case 32:
	    pReg->tridentRegs3CE[MiscExtFunc] |= 0x02;
    	    pReg->tridentRegs3CE[MiscExtFunc] |= 0x08; /* Clock Division by 2*/
	    clock *= 2;	/* Double the clock */
    	    offset = pScrn->displayWidth >> 1;
	    pReg->tridentRegsDAC[0x00] = 0x42;
	    break;
    }
    pReg->tridentRegs3x4[Offset] = offset & 0xFF;

    pReg->tridentRegsClock[0x00] = mode->ClockIndex;

    pReg->tridentRegs3C4[NewMode1] = 0x80;

    if (pTrident->Linear)
    	pReg->tridentRegs3x4[LinearAddReg] = ((pTrident->FbAddress >> 24) << 6)|
					 ((pTrident->FbAddress >> 20) & 0x0F)|
					 0x20;
    else {
	pReg->tridentRegs3CE[MiscExtFunc] |= 0x04;
    	pReg->tridentRegs3x4[LinearAddReg] = 0;
    }

    pReg->tridentRegs3x4[CRTCModuleTest] = 
				(mode->Flags & V_INTERLACE ? 0x84 : 0x80);
    OUTB(vgaIOBase+ 4, AddColReg);
    pReg->tridentRegs3x4[AddColReg] = (INB(vgaIOBase + 5) & 0xCF) |
				      ((offset & 0x100) >> 4);
   
    return(TRUE);
}

void
TVGARestore(ScrnInfoPtr pScrn, TRIDENTRegPtr tridentReg)
{
    TRIDENTPtr pTrident = TRIDENTPTR(pScrn);
    CARD8 temp;
    int vgaIOBase;
    vgaIOBase = VGAHWPTR(pScrn)->IOBase;

    /* Goto Old Mode */
    OUTB(0x3C4, 0x0B);
    OUTB(0x3C5, 0x00);
    OUTB(0x3C4, OldMode2 + NewMode2);
    OUTB(0x3C5, tridentReg->tridentRegs3C4[OldMode2]);

    /* Goto New Mode */
    OUTB(0x3C4, 0x0B);
    temp = INB(0x3C5);

    /* Unprotect registers */
    OUTW(0x3C4, (0x80 << 8) | NewMode1);

    temp = INB(0x3C8);
    temp = INB(0x3C6);
    temp = INB(0x3C6);
    temp = INB(0x3C6);
    temp = INB(0x3C6);
    OUTB(0x3C6, tridentReg->tridentRegsDAC[0x00]);
    temp = INB(0x3C8);

    OUTW_3x4(CRTCModuleTest);
    OUTW_3x4(LinearAddReg);
    OUTW_3x4(FIFOControl);
    OUTW_3C4(ConfPort2);
    if (pScrn->bitsPerPixel >= 8) {
       OUTW_3x4(Underline);
       if (pTrident->Chipset < TGUI9440AGi)
           OUTW_3x4(CRTCMode);
    }
    OUTW_3x4(AddColReg);
    OUTW_3CE(MiscExtFunc);
    OUTW_3x4(Offset);

    TRIDENTClockSelect(pScrn, tridentReg->tridentRegsClock[0x00]);

    OUTW(0x3C4, ((tridentReg->tridentRegs3C4[NewMode1]) << 8)| NewMode1);
}

void
TVGASave(ScrnInfoPtr pScrn, TRIDENTRegPtr tridentReg)
{
    TRIDENTPtr pTrident = TRIDENTPTR(pScrn);
    CARD8 temp;
    int vgaIOBase;
    vgaIOBase = VGAHWPTR(pScrn)->IOBase;

    temp = INB(0x3C8);
    temp = INB(0x3C6);
    temp = INB(0x3C6);
    temp = INB(0x3C6);
    temp = INB(0x3C6);
    tridentReg->tridentRegsDAC[0x00] = INB(0x3C6);
    temp = INB(0x3C8);

    /* Goto Old Mode */
    OUTB(0x3C4, 0x0B);
    OUTB(0x3C5, 0x00);
    OUTB(0x3C4, OldMode2 + NewMode2);
    tridentReg->tridentRegs3C4[OldMode2] = INB(0x3C5);

    /* Goto New Mode */
    OUTB(0x3C4, 0x0B);
    temp = INB(0x3C5);

    INB_3C4(NewMode1);

    /* Unprotect registers */
    OUTW(0x3C4, ((0x80 ^ 0x02) << 8) | NewMode1);
    OUTW(vgaIOBase + 4, (0x92 << 8) | NewMode1);

    if (pScrn->bitsPerPixel >= 8) {
        INB_3x4(Underline);
        if (pTrident->Chipset < TGUI9440AGi)
            INB_3x4(CRTCMode);
    }
    INB_3x4(LinearAddReg);
    INB_3x4(FIFOControl);
    INB_3x4(CRTCModuleTest);
    INB_3x4(AddColReg);
    INB_3CE(MiscExtFunc);
    INB_3C4(ConfPort2);

    TRIDENTClockSelect(pScrn, CLK_REG_SAVE);

    /* Protect registers */
    OUTW_3C4(NewMode1);
}

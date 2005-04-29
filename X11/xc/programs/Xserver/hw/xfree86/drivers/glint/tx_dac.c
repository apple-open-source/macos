/*
 * Copyright 1997-2001 by Alan Hourihane <alanh@fairlite.demon.co.uk>
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
 * Authors:  Alan Hourihane, <alanh@fairlite.demon.co.uk>
 *           Dirk Hohndel,   <hohndel@suse.de>
 *	     Stefan Dirsch,  <sndirsch@suse.de>
 *	     Helmut Fahrion, <hf@suse.de>
 *
 * this work is sponsored by S.u.S.E. GmbH, Fuerth, Elsa GmbH, Aachen and
 * Siemens Nixdorf Informationssysteme
 */
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/glint/tx_dac.c,v 1.16 2003/11/03 05:11:14 tsi Exp $ */

#include "xf86.h"
#include "xf86_OSproc.h"
#include "xf86_ansic.h"

#include "xf86PciInfo.h"
#include "xf86Pci.h"

#include "IBM.h"
#include "TI.h"
#include "glint_regs.h"
#include "glint.h"

Bool
TXInit(ScrnInfoPtr pScrn, DisplayModePtr mode, GLINTRegPtr pReg)
{
    GLINTPtr pGlint = GLINTPTR(pScrn);
    RamDacHWRecPtr pRamDac = RAMDACHWPTR(pScrn);
    RamDacRegRecPtr ramdacReg = &pRamDac->ModeReg;
    CARD32 temp1, temp2, temp3, temp4;

    if (pGlint->numMultiDevices == 2) {
	STOREREG(GCSRAperture, GCSRSecondaryGLINTMapEn);
    }

    if (pGlint->MultiAperture) {
        /* 
         * Setup HW 
         * 
         * Note: The order of discovery for the MX devices is dependent
         * on which way the resource allocation code decides to scan the
         * bus.  This setup assumes the first MX found owns the even
         * scanlines.  Should the implementation change an scan the bus
         * in the opposite direction, then simple invert the indices for
         * MultiPciInfo below.  If this is setup wrong, the bug will appear
         * as incorrect scanline interleaving when software rendering.
         */
	STOREREG(GMultGLINTAperture, pGlint->realWidth);
	STOREREG(GMultGLINT1, 
			pGlint->MultiPciInfo[0]->memBase[2] & 0xFF800000);
	STOREREG(GMultGLINT2,
			pGlint->MultiPciInfo[1]->memBase[2] & 0xFF800000);
    }

    if (IS_GMX2000 || IS_GLORIAXXL) {
    	pReg->glintRegs[LBMemoryEDO >> 3] = GLINT_READ_REG(LBMemoryEDO);
    	pReg->glintRegs[LBMemoryEDO >> 3] &= ~(LBEDOMask |
					   LBEDOBankSizeMask |
					   LBTwoPageDetectorMask);
    	pReg->glintRegs[LBMemoryEDO >> 3] |= (LBEDOEnabled |
					  LBEDOBankSize4M |
					  LBTwoPageDetector);
    	pReg->glintRegs[LBMemoryCtl >> 3] = GLINT_READ_REG(LBMemoryCtl);
    	pReg->glintRegs[LBMemoryCtl >> 3] &= ~(LBNumBanksMask |
					   LBPageSizeMask |
					   LBRASCASLowMask |
					   LBRASPrechargeMask |
					   LBCASLowMask |
					   LBPageModeMask |
					   LBRefreshCountMask);
    	pReg->glintRegs[LBMemoryCtl >> 3] |= (LBNumBanks2 |
					  LBPageSize1024 |
					  LBRASCASLow2 |
					  LBRASPrecharge2 |
					  LBCASLow1 |
					  LBPageModeEnabled |
					  (0x20 << LBRefreshCountShift));
    }

    STOREREG(Aperture0, 0);
    STOREREG(Aperture1, 0);

    STOREREG(DFIFODis, GLINT_READ_REG(DFIFODis) & 0xFFFFFFFE);
    STOREREG(FIFODis, GLINT_READ_REG(FIFODis) | 0x01);

    temp1 = mode->CrtcHSyncStart - mode->CrtcHDisplay;
    temp2 = mode->CrtcVSyncStart - mode->CrtcVDisplay;
    temp3 = mode->CrtcHSyncEnd - mode->CrtcHSyncStart;
    temp4 = mode->CrtcVSyncEnd - mode->CrtcVSyncStart;

    STOREREG(VTGHLimit, Shiftbpp(pScrn, mode->CrtcHTotal));
    STOREREG(VTGHSyncEnd, Shiftbpp(pScrn, temp1 + temp3));
    STOREREG(VTGHSyncStart, Shiftbpp(pScrn, temp1));
    STOREREG(VTGHBlankEnd, 
			Shiftbpp(pScrn, mode->CrtcHTotal - mode->CrtcHDisplay));

    STOREREG(VTGVLimit, mode->CrtcVTotal);
    STOREREG(VTGVSyncEnd, temp2 + temp4);
    STOREREG(VTGVSyncStart, temp2);
    STOREREG(VTGVBlankEnd, mode->CrtcVTotal - mode->CrtcVDisplay);

    if (IS_GMX2000) {
	STOREREG(VTGPolarity, 0xba);
    } else {
	STOREREG(VTGPolarity, (((mode->Flags & V_PHSYNC ? 0:2)<<2) |
			   ((mode->Flags & V_PVSYNC) ? 0 : 2) | (0xb0)));
    }

    STOREREG(VClkCtl, 0);
    STOREREG(VTGVGateStart, mode->CrtcVTotal - mode->CrtcVDisplay - 1);
    STOREREG(VTGVGateEnd, mode->CrtcVTotal - mode->CrtcVDisplay);

    /* This is ugly */
    if (pGlint->UseFireGL3000) {
    	STOREREG(VTGSerialClk, 0x05);
	STOREREG(VTGHGateStart, 
		    Shiftbpp(pScrn, mode->CrtcHTotal - mode->CrtcHDisplay - 1));
    	STOREREG(VTGHGateEnd, Shiftbpp(pScrn, mode->CrtcHTotal) - 1);
	STOREREG(FBModeSel, 0x907);
	STOREREG(VTGModeCtl, 0x00);
    } else 
    if (IS_GMX2000) {
    	STOREREG(VTGSerialClk, 0x02);
	STOREREG(VTGHGateStart, 
		    Shiftbpp(pScrn, mode->CrtcHTotal - mode->CrtcHDisplay - 1));
    	STOREREG(VTGHGateEnd, Shiftbpp(pScrn, mode->CrtcHTotal) - 1);
	STOREREG(FBModeSel, 0x907);
	STOREREG(VTGModeCtl, 0x04);
    } else {
    	STOREREG(VTGSerialClk, 0x05);
	STOREREG(VTGHGateStart, 
		    Shiftbpp(pScrn, mode->CrtcHTotal - mode->CrtcHDisplay) - 2);
    	STOREREG(VTGHGateEnd, Shiftbpp(pScrn, mode->CrtcHTotal) - 2);
	STOREREG(FBModeSel, 0x0A07);
	STOREREG(VTGModeCtl, 0x44);
    }

    if (IS_GMX2000 || IS_GLORIAXXL) {
    	STOREREG(FBMemoryCtl, 0x800); /* Optimum memory timings */
    } else {
    	STOREREG(FBMemoryCtl, GLINT_READ_REG(FBMemoryCtl));
    }

    /* Override FBModeSel for 300SX chip */
    if ( (pGlint->Chipset == PCI_VENDOR_3DLABS_CHIP_300SX) ||
        ((pGlint->Chipset == PCI_VENDOR_3DLABS_CHIP_DELTA) &&
	 (pGlint->MultiChip == PCI_CHIP_300SX)) ) {
	switch (pScrn->bitsPerPixel) {
	    case 8:
		STOREREG(FBModeSel, 0x905);
		break;
	    case 16:
		STOREREG(FBModeSel, 0x903);
		break;
	    case 32:
		STOREREG(FBModeSel, 0x901);
		break;
	}
    }

    switch (pGlint->RamDac->RamDacType) {
    case IBM526DB_RAMDAC:
    case IBM526_RAMDAC:
    {
	/* Get the programmable clock values */
    	unsigned long m=0,n=0,p=0,c=0;

	(void) IBMramdac526CalculateMNPCForClock(pGlint->RefClock, mode->Clock,
			1, pGlint->MinClock, pGlint->MaxClock, &m, &n, &p, &c);
			
	STORERAMDAC(IBMRGB_m0, m);
	STORERAMDAC(IBMRGB_n0, n);
	STORERAMDAC(IBMRGB_p0, p);
	STORERAMDAC(IBMRGB_c0, c);

	STORERAMDAC(IBMRGB_pll_ctrl1, 0x05);
	STORERAMDAC(IBMRGB_pll_ctrl2, 0x00);

	p = 1;
	(void) IBMramdac526CalculateMNPCForClock(pGlint->RefClock, mode->Clock,
			0, pGlint->MinClock, pGlint->MaxClock, &m, &n, &p, &c);

	STORERAMDAC(IBMRGB_sysclk, 0x05);
	STORERAMDAC(IBMRGB_sysclk_m, m);
	STORERAMDAC(IBMRGB_sysclk_n, n);
	STORERAMDAC(IBMRGB_sysclk_p, p);
	STORERAMDAC(IBMRGB_sysclk_c, c);
    }
    STORERAMDAC(IBMRGB_misc1, SENS_DSAB_DISABLE | VRAM_SIZE_64);
    STORERAMDAC(IBMRGB_misc2, COL_RES_8BIT | PORT_SEL_VRAM | PCLK_SEL_PLL);
    STORERAMDAC(IBMRGB_misc3, 0);
    STORERAMDAC(IBMRGB_misc_clock, 1);
    STORERAMDAC(IBMRGB_sync, 0);
    STORERAMDAC(IBMRGB_hsync_pos, 0);
    STORERAMDAC(IBMRGB_pwr_mgmt, 0);
    STORERAMDAC(IBMRGB_dac_op, 0);
    STORERAMDAC(IBMRGB_pal_ctrl, 0);

    break;
    case IBM640_RAMDAC:
    {
	/* Get the programmable clock values */
    	unsigned long m=0,n=0,p=0,c=0;

	(void) IBMramdac640CalculateMNPCForClock(pGlint->RefClock, mode->Clock,
			1, pGlint->MinClock, pGlint->MaxClock, &m, &n, &p, &c);

	STORERAMDAC(RGB640_PLL_N, n);
	STORERAMDAC(RGB640_PLL_M, m);
	STORERAMDAC(RGB640_PLL_P, p<<1);
	STORERAMDAC(RGB640_PLL_CTL, c | IBM640_PLL_EN);
	STORERAMDAC(RGB640_AUX_PLL_CTL, 0); /* Disable AUX PLL */
    }
    STORERAMDAC(RGB640_PIXEL_INTERLEAVE, 0x00);

    temp1 = IBM640_RDBK | IBM640_VRAM;
    if (pScrn->rgbBits == 8) 
    	temp1 |= IBM640_PSIZE8;
    STORERAMDAC(RGB640_VGA_CONTROL, temp1);

    STORERAMDAC(RGB640_DAC_CONTROL, IBM640_DACENBL | IBM640_SHUNT);
    STORERAMDAC(RGB640_OUTPUT_CONTROL, IBM640_RDAI | IBM640_WATCTL);
    STORERAMDAC(RGB640_SYNC_CONTROL, 0x00);
    STORERAMDAC(RGB640_VRAM_MASK0, 0xFF);
    STORERAMDAC(RGB640_VRAM_MASK1, 0xFF);
    STORERAMDAC(RGB640_VRAM_MASK2, 0x0F);
    
    STOREREG(VTGModeCtl, 0x04);
    break;

    case TI3026_RAMDAC:
    case TI3030_RAMDAC:
    {
	/* Get the programmable clock values */
	unsigned long m=0,n=0,p=0;
	unsigned long clock;
	unsigned long q, VCO = 0;

	clock = TIramdacCalculateMNPForClock(pGlint->RefClock, 
		mode->Clock, 1, pGlint->MinClock, pGlint->MaxClock, &m, &n, &p);

	STORERAMDAC(TIDAC_PIXEL_N, ((n & 0x3f) | 0xC0));
	STORERAMDAC(TIDAC_PIXEL_M,  (m & 0x3f));
	STORERAMDAC(TIDAC_PIXEL_P, ((p & 0x03) | 0xbc));
	STORERAMDAC(TIDAC_PIXEL_VALID, TRUE);

    	if (pGlint->RamDac->RamDacType == (TI3026_RAMDAC))
            n = 65 - ((64 << 2) / pScrn->bitsPerPixel);
	else
            n = 65 - ((128 << 2) / pScrn->bitsPerPixel);
	m = 61;
	p = 0;
	for (q = 0; q < 8; q++) {
	    if (q > 0) p = 3;
	    for ( ; p < 4; p++) {
		VCO = ((clock * (q + 1) * (65 - m)) / (65 - n)) << (p + 1);
		if (VCO >= 110000) { break; }
	    }
	    if (VCO >= 110000) { break; }
	}
	STORERAMDAC(TIDAC_clock_ctrl, (q | 0x38));

	STORERAMDAC(TIDAC_LOOP_N, ((n & 0x3f) | 0xC0));
	STORERAMDAC(TIDAC_LOOP_M,  (m & 0x3f));
	STORERAMDAC(TIDAC_LOOP_P, ((p & 0x03) | 0xF0));
	STORERAMDAC(TIDAC_LOOP_VALID, TRUE);
    }
    if (pGlint->RamDac->RamDacType == (TI3030_RAMDAC))
	STOREREG(VTGModeCtl, 0x04);
    break;
    }

    /* Now use helper routines to setup bpp for this driver */
    (*pGlint->RamDac->SetBpp)(pScrn, ramdacReg);

    return(TRUE);
}

void
TXSave(ScrnInfoPtr pScrn, GLINTRegPtr pReg)
{
    GLINTPtr pGlint = GLINTPTR(pScrn);

    if (pGlint->numMultiDevices == 2) {
	SAVEREG(GCSRAperture);
    }

    if (pGlint->MultiAperture) {
	SAVEREG(GMultGLINTAperture);
	SAVEREG(GMultGLINT1);
	SAVEREG(GMultGLINT2);
    }

    SAVEREG(Aperture0);
    SAVEREG(Aperture1);

    SAVEREG(DFIFODis);

    if (pGlint->Chipset != PCI_VENDOR_3DLABS_CHIP_300SX) {
	SAVEREG(FIFODis);
	SAVEREG(VTGModeCtl);
    }

    SAVEREG(VClkCtl);
    SAVEREG(VTGPolarity);
    SAVEREG(VTGHLimit);
    SAVEREG(VTGHBlankEnd);
    SAVEREG(VTGHSyncStart);
    SAVEREG(VTGHSyncEnd);
    SAVEREG(VTGVLimit);
    SAVEREG(VTGVBlankEnd);
    SAVEREG(VTGVSyncStart);
    SAVEREG(VTGVSyncEnd);
    SAVEREG(VTGVGateStart);
    SAVEREG(VTGVGateEnd);
    SAVEREG(VTGSerialClk);
    SAVEREG(FBModeSel);
    SAVEREG(VTGHGateStart);
    SAVEREG(VTGHGateEnd);
    SAVEREG(FBMemoryCtl);

    if (IS_GMX2000 || IS_GLORIAXXL) {
    	SAVEREG(LBMemoryEDO);
    	SAVEREG(LBMemoryCtl);
    }
}

void
TXRestore(ScrnInfoPtr pScrn, GLINTRegPtr pReg)
{
    GLINTPtr pGlint = GLINTPTR(pScrn);

    if (pGlint->numMultiDevices == 2) {
	RESTOREREG(GCSRAperture);
    }

    if (pGlint->MultiAperture) {
	RESTOREREG(GMultGLINTAperture);
	RESTOREREG(GMultGLINT1);
	RESTOREREG(GMultGLINT2);
    }

    RESTOREREG(Aperture0);
    RESTOREREG(Aperture1);

    RESTOREREG(DFIFODis);

    if (pGlint->Chipset != PCI_VENDOR_3DLABS_CHIP_300SX) {
	RESTOREREG(FIFODis);
	RESTOREREG(VTGModeCtl);
    }

    RESTOREREG(VTGPolarity);
    RESTOREREG(VClkCtl);
    RESTOREREG(VTGSerialClk);
    RESTOREREG(VTGHLimit);
    RESTOREREG(VTGHSyncStart);
    RESTOREREG(VTGHSyncEnd);
    RESTOREREG(VTGHBlankEnd);
    RESTOREREG(VTGVLimit);
    RESTOREREG(VTGVSyncStart);
    RESTOREREG(VTGVSyncEnd);
    RESTOREREG(VTGVBlankEnd);
    RESTOREREG(VTGVGateStart);
    RESTOREREG(VTGVGateEnd);
    RESTOREREG(FBModeSel);
    RESTOREREG(VTGHGateStart);
    RESTOREREG(VTGHGateEnd);
    RESTOREREG(FBMemoryCtl);

    if (IS_GMX2000 || IS_GLORIAXXL) {
    	RESTOREREG(LBMemoryEDO);
    	RESTOREREG(LBMemoryCtl);
    }
}

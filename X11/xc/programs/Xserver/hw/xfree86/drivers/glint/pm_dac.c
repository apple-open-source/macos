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
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/glint/pm_dac.c,v 1.12 2003/11/03 05:11:14 tsi Exp $ */

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
PermediaInit(ScrnInfoPtr pScrn, DisplayModePtr mode)
{
    GLINTPtr pGlint = GLINTPTR(pScrn);
    GLINTRegPtr pReg = &pGlint->ModeReg[0];
    RamDacHWRecPtr pIBM = RAMDACHWPTR(pScrn);
    RamDacRegRecPtr ramdacReg = &pIBM->ModeReg;
    CARD32 temp1, temp2, temp3, temp4;

    STOREREG(Aperture0, 0x00000000);
    STOREREG(Aperture1, 0x00000000);

    pReg->glintRegs[PMFramebufferWriteMask >> 3] = 0xFFFFFFFF;
    pReg->glintRegs[PMBypassWriteMask >> 3] = 0xFFFFFFFF;

    pReg->glintRegs[DFIFODis >> 3] = 1;
    pReg->glintRegs[FIFODis >> 3] = 3;

    temp1 = mode->CrtcHSyncStart - mode->CrtcHDisplay;
    temp2 = mode->CrtcVSyncStart - mode->CrtcVDisplay;
    temp3 = mode->CrtcHSyncEnd - mode->CrtcHSyncStart;
    temp4 = mode->CrtcVSyncEnd - mode->CrtcVSyncStart;

    pReg->glintRegs[PMHTotal >> 3] = Shiftbpp(pScrn,mode->CrtcHTotal);
    pReg->glintRegs[PMHsEnd >> 3] = Shiftbpp(pScrn, temp1 + temp3);
    pReg->glintRegs[PMHsStart >> 3] = Shiftbpp(pScrn, temp1);
    pReg->glintRegs[PMHbEnd >> 3] = Shiftbpp(pScrn, mode->CrtcHTotal - 
							mode->CrtcHDisplay);
    pReg->glintRegs[PMHgEnd >> 3] = Shiftbpp(pScrn, mode->CrtcHTotal - 
							mode->CrtcHDisplay);
    pReg->glintRegs[PMScreenStride >> 3] = 
					Shiftbpp(pScrn,pScrn->displayWidth>>1);

    pReg->glintRegs[PMVTotal >> 3] = mode->CrtcVTotal;
    pReg->glintRegs[PMVsEnd >> 3] = temp2 + temp4;
    pReg->glintRegs[PMVsStart >> 3] = temp2;
    pReg->glintRegs[PMVbEnd >> 3] = mode->CrtcVTotal - mode->CrtcVDisplay;

    pReg->glintRegs[PMVideoControl >> 3] = 
 	    (((mode->Flags & V_PHSYNC) ? 0x1 : 0x3) << 3) |  
 	    (((mode->Flags & V_PVSYNC) ? 0x1 : 0x3) << 5) | 1; 

    pReg->glintRegs[VClkCtl >> 3] = 3;
    pReg->glintRegs[PMScreenBase >> 3] = 0;
    pReg->glintRegs[PMHTotal >> 3] -= 1; /* PMHTotal */
    pReg->glintRegs[PMHsStart >> 3] -= 1; /* PMHsStart */
    pReg->glintRegs[PMVTotal >> 3] -= 1; /* PMVTotal */
    pReg->glintRegs[ChipConfig >> 3] = GLINT_READ_REG(ChipConfig) & 0xFFFFFFFD;

    switch(pGlint->RamDac->RamDacType) {
    case IBM526_RAMDAC:
    case IBM526DB_RAMDAC:
    {
	/* Get the programmable clock values */
    	unsigned long m=0,n=0,p=0,c=0;
	
	(void) IBMramdac526CalculateMNPCForClock(pGlint->RefClock, mode->Clock,
			1, pGlint->MinClock, pGlint->MaxClock, &m, &n, &p, &c);
			
	ramdacReg->DacRegs[IBMRGB_m0] = m;
	ramdacReg->DacRegs[IBMRGB_n0] = n;
	ramdacReg->DacRegs[IBMRGB_p0] = p;
	ramdacReg->DacRegs[IBMRGB_c0] = c;

	ramdacReg->DacRegs[IBMRGB_pll_ctrl1] = 0x05;
	ramdacReg->DacRegs[IBMRGB_pll_ctrl2] = 0x00;

	p = 1;
	(void) IBMramdac526CalculateMNPCForClock(pGlint->RefClock, mode->Clock,
			0, pGlint->MinClock, pGlint->MaxClock, &m, &n, &p, &c);

	ramdacReg->DacRegs[IBMRGB_sysclk] = 0x05;
	ramdacReg->DacRegs[IBMRGB_sysclk_m] = m;
	ramdacReg->DacRegs[IBMRGB_sysclk_n] = n;
	ramdacReg->DacRegs[IBMRGB_sysclk_p] = p;
	ramdacReg->DacRegs[IBMRGB_sysclk_c] = c;
    }
    ramdacReg->DacRegs[IBMRGB_misc1] = SENS_DSAB_DISABLE | VRAM_SIZE_32;
    ramdacReg->DacRegs[IBMRGB_misc2] = COL_RES_8BIT | PORT_SEL_VRAM;
    if (pScrn->depth >= 24)
	ramdacReg->DacRegs[IBMRGB_misc2] |= PCLK_SEL_LCLK;
    else 
	ramdacReg->DacRegs[IBMRGB_misc2] |= PCLK_SEL_PLL;
    ramdacReg->DacRegs[IBMRGB_misc3] = 0;
    ramdacReg->DacRegs[IBMRGB_misc_clock] = 1;
    ramdacReg->DacRegs[IBMRGB_sync] = 0;
    ramdacReg->DacRegs[IBMRGB_hsync_pos] = 0;
    ramdacReg->DacRegs[IBMRGB_pwr_mgmt] = 0;
    ramdacReg->DacRegs[IBMRGB_dac_op] = 0;
    ramdacReg->DacRegs[IBMRGB_pal_ctrl] = 0;
    break;
    case TI3026_RAMDAC:
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
            n = 65 - ((32 << 2) / pScrn->bitsPerPixel);
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
	break;
    }
    }

    (*pGlint->RamDac->SetBpp)(pScrn, ramdacReg);

    /* The permedia uses a 32bit data path, the TI ramdac code
     * defaults to 64bit. So we knock it down to 32bit here */
    if (pGlint->RamDac->RamDacType == (TI3026_RAMDAC))
	ramdacReg->DacRegs[TIDAC_multiplex_ctrl] -= 1;

    return(TRUE);
}

void
PermediaSave(ScrnInfoPtr pScrn, GLINTRegPtr glintReg)
{
    GLINTPtr pGlint = GLINTPTR(pScrn);

    /* We can't rely on the vgahw layer copying the font information
     * back properly, due to problems with MMIO access to VGA space
     * so we memcpy the information using the slow routines */
    xf86SlowBcopy((CARD8*)pGlint->FbBase, (CARD8*)pGlint->VGAdata, 65536);

    glintReg->glintRegs[Aperture0 >> 3]  = GLINT_READ_REG(Aperture0);
    glintReg->glintRegs[Aperture1 >> 3]  = GLINT_READ_REG(Aperture1);
    glintReg->glintRegs[PMFramebufferWriteMask] = 
					GLINT_READ_REG(PMFramebufferWriteMask);
    glintReg->glintRegs[PMBypassWriteMask >> 3] = 
					GLINT_READ_REG(PMBypassWriteMask);
    glintReg->glintRegs[DFIFODis >> 3]  = GLINT_READ_REG(DFIFODis);
    glintReg->glintRegs[FIFODis >> 3]  = GLINT_READ_REG(FIFODis);

    glintReg->glintRegs[PMHTotal >> 3] = GLINT_READ_REG(PMHTotal);
    glintReg->glintRegs[PMHbEnd >> 3] = GLINT_READ_REG(PMHbEnd);
    glintReg->glintRegs[PMHgEnd >> 3] = GLINT_READ_REG(PMHgEnd);
    glintReg->glintRegs[PMScreenStride >> 3] = GLINT_READ_REG(PMScreenStride);
    glintReg->glintRegs[PMHsStart >> 3] = GLINT_READ_REG(PMHsStart);
    glintReg->glintRegs[PMHsEnd >> 3] = GLINT_READ_REG(PMHsEnd);
    glintReg->glintRegs[PMVTotal >> 3] = GLINT_READ_REG(PMVTotal);
    glintReg->glintRegs[PMVbEnd >> 3] = GLINT_READ_REG(PMVbEnd);
    glintReg->glintRegs[PMVsStart >> 3] = GLINT_READ_REG(PMVsStart);
    glintReg->glintRegs[PMVsEnd >> 3] = GLINT_READ_REG(PMVsEnd);
    glintReg->glintRegs[PMScreenBase >> 3] = GLINT_READ_REG(PMScreenBase);
    glintReg->glintRegs[PMVideoControl >> 3] = GLINT_READ_REG(PMVideoControl);
    glintReg->glintRegs[VClkCtl >> 3] = GLINT_READ_REG(VClkCtl);
    glintReg->glintRegs[ChipConfig >> 3] = GLINT_READ_REG(ChipConfig);
}

void
PermediaRestore(ScrnInfoPtr pScrn, GLINTRegPtr pReg)
{
    GLINTPtr pGlint = GLINTPTR(pScrn);

    /* We can't rely on the vgahw layer copying the font information
     * back properly, due to problems with MMIO access to VGA space
     * so we memcpy the information using the slow routines */
    if (pGlint->STATE)
	xf86SlowBcopy((CARD8*)pGlint->VGAdata, (CARD8*)pGlint->FbBase, 65536);

    RESTOREREG(ChipConfig);
    RESTOREREG(DFIFODis);
    RESTOREREG(FIFODis);
    RESTOREREG(Aperture0);
    RESTOREREG(Aperture1);
    RESTOREREG(PMFramebufferWriteMask);
    RESTOREREG(PMBypassWriteMask);
    RESTOREREG(PMVideoControl);
    RESTOREREG(PMHgEnd);
    RESTOREREG(VClkCtl);
    RESTOREREG(PMHTotal);
    RESTOREREG(PMHbEnd);
    RESTOREREG(PMHsStart);
    RESTOREREG(PMHsEnd);
    RESTOREREG(PMVTotal);
    RESTOREREG(PMVbEnd);
    RESTOREREG(PMVsStart);
    RESTOREREG(PMVsEnd);
    RESTOREREG(PMScreenBase);
    RESTOREREG(PMScreenStride);
}

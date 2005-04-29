/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/tseng/tseng_ramdac.c,v 1.28 2004/02/13 23:58:44 dawes Exp $ */





/*
 *
 * Copyright 1993-1997 The XFree86 Project, Inc.
 * All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject
 * to the following conditions:
 *
 *   1.  Redistributions of source code must retain the above copyright
 *       notice, this list of conditions, and the following disclaimer.
 *
 *   2.  Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer
 *       in the documentation and/or other materials provided with the
 *       distribution, and in the same place and form as other copyright,
 *       license and disclaimer information.
 *
 *   3.  The end-user documentation included with the redistribution,
 *       if any, must include the following acknowledgment: "This product
 *       includes software developed by The XFree86 Project, Inc
 *       (http://www.xfree86.org/) and its contributors", in the same
 *       place and form as other third-party acknowledgments.  Alternately,
 *       this acknowledgment may appear in the software itself, in the
 *       same form and location as other such third-party acknowledgments.
 *
 *   4.  Except as contained in this notice, the name of The XFree86
 *       Project, Inc shall not be used in advertising or otherwise to
 *       promote the sale, use or other dealings in this Software without
 *       prior written authorization from The XFree86 Project, Inc.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESSED OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE XFREE86 PROJECT, INC OR ITS CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

/*
 * tseng_ramdac.c.
 *
 * Much of this code was taken from the XF86_W32 (3.2) server [kmg]
 */

#include "tseng.h"

SymTabRec TsengDacTable[] =
{
    {NORMAL_DAC, "normal"},
    {ATT20C47xA_DAC, "att20c47xa"},
    {Sierra1502X_DAC, "sc1502x"},
    {ATT20C497_DAC, "att20c497"},
    {ATT20C490_DAC, "att20c490"},
    {ATT20C493_DAC, "att20c493"},
    {ATT20C491_DAC, "att20c491"},
    {ATT20C492_DAC, "att20c492"},
    {ICS5341_DAC, "ics5341"},
    {ICS5301_DAC, "ics5301"},
    {STG1700_DAC, "stg1700"},
    {STG1702_DAC, "stg1702"},
    {STG1703_DAC, "stg1703"},
    {ET6000_DAC, "et6000"},
    {CH8398_DAC, "ch8398"},
    {MUSIC4910_DAC, "music4910"},
    {UNKNOWN_DAC, "unknown"},
};

/*** private data ***/

#define RAMDAC_RMR 0x3c6
#define RAMDAC_READ 0x3c7
#define RAMDAC_WRITE 0x3c8
#define RAMDAC_RAM 0x3c9

static unsigned char white_cmap[] =
{0xff, 0xff, 0xff};


void
tseng_dactopel(void)
{
    outb(0x3C8, 0);
    return;
}

unsigned char
tseng_dactocomm(void)
{
    (void)inb(0x3C6);
    (void)inb(0x3C6);
    (void)inb(0x3C6);
    return (inb(0x3C6));
}

unsigned char
tseng_getdaccomm(void)
{
    unsigned char ret;

    tseng_dactopel();
    (void)tseng_dactocomm();
    ret = inb(0x3C6);
    tseng_dactopel();
    return (ret);
}

void
tseng_setdaccomm(unsigned char comm)
{
    tseng_dactopel();
    (void)tseng_dactocomm();
    outb(0x3C6, comm);
    tseng_dactopel();
    return;
}

static Bool
ProbeSTG1703(TsengPtr pTseng, Bool quiet)
{
    unsigned char cid, did, daccomm, readmask;
    Bool Found = FALSE;

    readmask = inb(RAMDAC_RMR);
    tseng_dactopel();
    daccomm = tseng_getdaccomm();
    tseng_setdaccomm(daccomm | 0x10);
    tseng_dactocomm();
    inb(0x3C6);
    outb(RAMDAC_RMR, 0x00);
    outb(RAMDAC_RMR, 0x00);
    cid = inb(RAMDAC_RMR);	       /* company ID */
    did = inb(RAMDAC_RMR);	       /* device ID */
    tseng_dactopel();
    outb(RAMDAC_RMR, readmask);
    tseng_setdaccomm(daccomm);

    if (cid == 0x44) {		       /* STG170x RAMDAC found */
	Found = TRUE;
	switch (did) {
	case 0x02:
	    pTseng->DacInfo.DacType = STG1702_DAC;
	    break;
	case 0x03:
	    pTseng->DacInfo.DacType = STG1703_DAC;
	    break;
	case 0x00:
	default:
	    pTseng->DacInfo.DacType = STG1700_DAC;
	    /* treat an unknown STG170x as a 1700 */
	}
    }
    return (Found);
}

static Bool
ProbeGenDAC(TsengPtr pTseng, int scrnIndex, Bool quiet)
{
    /* probe for ICS GENDAC (ICS5341) */
    /*
     * GENDAC and SDAC have two fixed read only PLL clocks
     *     CLK0 f0: 25.255MHz   M-byte 0x28  N-byte 0x61
     *     CLK0 f1: 28.311MHz   M-byte 0x3d  N-byte 0x62
     * which can be used to detect GENDAC and SDAC since there is no chip-id
     * for the GENDAC.
     *
     * code was taken from S3 XFree86 driver.
     * NOTE: for the GENDAC on a ET4000W32p, reading PLL values
     * for CLK0 f0 and f1 always returns 0x7f (but is documented "read only")
     * In fact, all "read only" registers return 0x7f
     */

    unsigned char saveCR31, savelut[6];
    int i;
    long clock01, clock23;
    Bool found = FALSE;
    unsigned char dbyte = 0;
    int mclk = 0;
    int iobase = VGAHW_GET_IOBASE();

    outb(iobase + 4, 0x31);
    saveCR31 = inb(iobase + 5);

    outb(iobase + 5, saveCR31 & ~0x40);

    outb(RAMDAC_READ, 0);
    for (i = 0; i < 2 * 3; i++)	       /* save first two LUT entries */
	savelut[i] = inb(RAMDAC_RAM);
    outb(RAMDAC_WRITE, 0);
    for (i = 0; i < 2 * 3; i++)	       /* set first two LUT entries to zero */
	outb(RAMDAC_RAM, 0);

    outb(iobase + 4, 0x31);
    outb(iobase + 5, saveCR31 | 0x40);

    outb(RAMDAC_READ, 0);
    for (i = clock01 = 0; i < 4; i++)
	clock01 = (clock01 << 8) | (inb(RAMDAC_RAM) & 0xff);
    for (i = clock23 = 0; i < 4; i++)
	clock23 = (clock23 << 8) | (inb(RAMDAC_RAM) & 0xff);

    /* get MClk value */
    outb(RAMDAC_READ, 0x0a);
    mclk = (inb(RAMDAC_RAM) + 2) * 14318;
    dbyte = inb(RAMDAC_RAM);
    mclk /= ((dbyte & 0x1f) + 2) * (1 << ((dbyte >> 5) & 0x03));
    pTseng->MClkInfo.MemClk = mclk;

    outb(iobase + 4, 0x31);
    outb(iobase + 5, saveCR31 & ~0x40);

    outb(RAMDAC_WRITE, 0);
    for (i = 0; i < 2 * 3; i++)	       /* restore first two LUT entries */
	outb(RAMDAC_RAM, savelut[i]);

    outb(iobase + 4, 0x31);
    outb(iobase + 5, saveCR31);

    if (clock01 == 0x28613d62 ||
	(clock01 == 0x7f7f7f7f && clock23 != 0x7f7f7f7f)) {
	found = TRUE;

	tseng_dactopel();
	inb(RAMDAC_RMR);
	inb(RAMDAC_RMR);
	inb(RAMDAC_RMR);

	dbyte = inb(RAMDAC_RMR);
	/* the fourth read will show the GenDAC/SDAC chip ID and revision */
	switch (dbyte & 0xf0) {
	case 0xb0:
	    if (!quiet) {
		xf86DrvMsg(scrnIndex, X_PROBED, "Ramdac: ICS 5341 GenDAC and programmable clock (MClk = %d MHz)\n",
		    mclk/1000);
	    }
	    pTseng->DacInfo.DacType = ICS5341_DAC;
	    break;
	case 0xf0:
	    if (!quiet) {
		xf86DrvMsg(scrnIndex, X_PROBED, "Ramdac: ICS 5301 GenDAC and programmable clock (MClk = %d MHz)\n",
		    mclk/1000);
	    }
	    pTseng->DacInfo.DacType = ICS5301_DAC;
	    break;
	default:
	    if (!quiet) {
		xf86DrvMsg(scrnIndex, X_PROBED, "Ramdac: unkown GenDAC and programmable clock (ID code = 0x%02x). Please report. (we'll treat it as a standard ICS5301 for now).\n",
		    dbyte);
	    }
	    pTseng->DacInfo.DacType = ICS5301_DAC;
	}
	tseng_dactopel();
    }
    return found;
}

/* probe for RAMDAC using the chip-ID method */
static Bool
ProbeRamdacID(TsengPtr pTseng, Bool quiet)
{
    unsigned char cid;
    Bool Found = FALSE;

    tseng_dactopel();
    cid = inb(RAMDAC_RMR);
    cid = inb(RAMDAC_RMR);
    cid = inb(RAMDAC_RMR);
    cid = inb(RAMDAC_RMR);	       /* this returns chip ID */
    switch (cid) {
    case 0xc0:
	Found = TRUE;
	pTseng->DacInfo.DacType = CH8398_DAC;
	break;
    case 0x82:
	Found = TRUE;
	pTseng->DacInfo.DacType = MUSIC4910_DAC;
	break;
    default:
	Found = FALSE;
    }
    tseng_dactopel();

    return Found;
}

/*
 *  For a description of the following, see AT&T's data sheet for ATT20C490/491
 *  and ATT20C492/493--GGL
 */

static void
write_cr(int cr)
{
    inb(RAMDAC_WRITE);
    xf86IODelay();
    inb(RAMDAC_RMR);
    xf86IODelay();
    inb(RAMDAC_RMR);
    xf86IODelay();
    inb(RAMDAC_RMR);
    xf86IODelay();
    inb(RAMDAC_RMR);
    xf86IODelay();
    outb(RAMDAC_RMR, cr);
    xf86IODelay();
    inb(RAMDAC_WRITE);
    xf86IODelay();
}

static int
read_cr(void)
{
    unsigned int cr;

    inb(RAMDAC_WRITE);
    xf86IODelay();
    inb(RAMDAC_RMR);
    xf86IODelay();
    inb(RAMDAC_RMR);
    xf86IODelay();
    inb(RAMDAC_RMR);
    xf86IODelay();
    inb(RAMDAC_RMR);
    xf86IODelay();
    cr = inb(RAMDAC_RMR);
    xf86IODelay();
    inb(RAMDAC_WRITE);
    return cr;
}

static void
write_color(int entry, unsigned char *cmap)
{
    outb(RAMDAC_WRITE, entry);
    xf86IODelay();
    outb(RAMDAC_RAM, cmap[0]);
    xf86IODelay();
    outb(RAMDAC_RAM, cmap[1]);
    xf86IODelay();
    outb(RAMDAC_RAM, cmap[2]);
    xf86IODelay();
}

static void
read_color(int entry, unsigned char *cmap)
{
    outb(RAMDAC_READ, entry);
    xf86IODelay();
    cmap[0] = inb(RAMDAC_RAM);
    xf86IODelay();
    cmap[1] = inb(RAMDAC_RAM);
    xf86IODelay();
    cmap[2] = inb(RAMDAC_RAM);
    xf86IODelay();
}

Bool
Check_Tseng_Ramdac(ScrnInfoPtr pScrn)
{
    unsigned char cmap[3], save_cmap[3];
    BOOL cr_saved;
    int mclk;
    int dbyte;
    TsengPtr pTseng = TsengPTR(pScrn);
    rgb zeros = {0, 0, 0};

    PDEBUG("	Check_Tseng_Ramdac\n");

    pTseng->dac.rmr = inb(RAMDAC_RMR);
    pTseng->dac.saved_cr = read_cr();
    cr_saved = TRUE;

    /* first see if ramdac type was given in XF86Config. If so, assume that is 
     * correct, and don't probe for it.
     */
    if (pScrn->ramdac) {
	pTseng->DacInfo.DacType =
	    (t_ramdactype)xf86StringToToken(TsengDacTable, pScrn->ramdac);
	if (pTseng->DacInfo.DacType < 0) {
	    xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "Unknown RAMDAC type \"%s\" specified\n", pScrn->ramdac);
	    return FALSE;
	}
    } else {			       /* autoprobe for the RAMDAC */
	if (Is_ET6K) {
	    pTseng->DacInfo.DacType = ET6000_DAC;
	    (void) inb(pTseng->IOAddress + 0x67);
	    outb(pTseng->IOAddress + 0x67, 10);
	    mclk = (inb(pTseng->IOAddress + 0x69) + 2) * 14318;
	    dbyte = inb(pTseng->IOAddress + 0x69);
	    mclk /= ((dbyte & 0x1f) + 2) * (1 << ((dbyte >> 5) & 0x03));
	    pTseng->MClkInfo.MemClk = mclk;
	} else if (ProbeGenDAC(pTseng, pScrn->scrnIndex, FALSE)) {
	    /* It is. Nothing to do here */
	} else if (ProbeSTG1703(pTseng, FALSE)) {
	    /* it's a STG170x */
	} else if (ProbeRamdacID(pTseng, FALSE)) {
	    /* found one using RAMDAC ID code */
	} else
	    /* if none of the above: start probing for other DACs */
	{
	    outb(RAMDAC_RMR, 0xff);
	    xf86IODelay();
	    inb(RAMDAC_RMR);
	    xf86IODelay();
	    inb(RAMDAC_RMR);
	    xf86IODelay();
	    inb(RAMDAC_RMR);
	    xf86IODelay();
	    inb(RAMDAC_RMR);
	    xf86IODelay();
	    outb(RAMDAC_RMR, 0x1c);
	    xf86IODelay();

	    if (inb(RAMDAC_RMR) != 0xff) {
		cr_saved = FALSE;
		pTseng->DacInfo.DacType = ATT20C47xA_DAC;
		goto dac_found;
	    }
	    write_cr(0xe0);
	    if ((read_cr() >> 5) != 0x7) {
		pTseng->DacInfo.DacType = ATT20C497_DAC;
		goto dac_found;
	    }
	    write_cr(0x60);
	    if ((read_cr() >> 5) == 0) {
		write_cr(0x2);
		if ((read_cr() & 0x2) != 0)
		    pTseng->DacInfo.DacType = ATT20C490_DAC;
		else
		    pTseng->DacInfo.DacType = ATT20C493_DAC;
	    } else {
		write_cr(0x2);
		outb(RAMDAC_RMR, 0xff);
		read_color(0xff, save_cmap);

		write_color(0xff, white_cmap);
		read_color(0xff, cmap);

		if (cmap[0] == 0xff && cmap[1] == 0xff && cmap[2] == 0xff)
		    pTseng->DacInfo.DacType = ATT20C491_DAC;
		else
		    pTseng->DacInfo.DacType = ATT20C492_DAC;

		write_color(0xff, save_cmap);
	    }
	}
    }

  dac_found:
    /* defaults: 8-bit wide DAC port, 6-bit color lookup-tables */
    pTseng->DacInfo.RamdacShift = 10;
    pTseng->DacInfo.RamdacMask = 0x3f;
    pTseng->DacInfo.Dac8Bit = FALSE;
    pTseng->DacInfo.DacPort16 = FALSE;
    pTseng->DacInfo.NotAttCompat = FALSE;	/* default: treat as ATT compatible DAC */
    pTseng->DacInfo.rgb24packed = zeros;
    pScrn->progClock = FALSE;
    pTseng->ClockChip = CLOCKCHIP_DEFAULT;
    pTseng->MClkInfo.Programmable = FALSE;

    /* now override defaults with appropriate values for each RAMDAC */
    switch (pTseng->DacInfo.DacType) {
    case ATT20C490_DAC:
    case ATT20C491_DAC:
	pTseng->DacInfo.RamdacShift = 8;
	pTseng->DacInfo.RamdacMask = 0xff;
	pTseng->DacInfo.Dac8Bit = TRUE;
	break;
    case UNKNOWN_DAC:
    case Sierra1502X_DAC:
	pTseng->DacInfo.NotAttCompat = TRUE;	/* avoids treatment as ATT compatible DAC */
	break;
    case ET6000_DAC:
	pScrn->progClock = TRUE;
	pTseng->ClockChip = CLOCKCHIP_ET6000;
	pTseng->DacInfo.NotAttCompat = TRUE;	/* avoids treatment as ATT compatible DAC */
	pTseng->MClkInfo.Programmable = TRUE;
	pTseng->MClkInfo.min = 80000;
	pTseng->MClkInfo.max = 110000;
	break;
    case ICS5341_DAC:
	pScrn->progClock = TRUE;
	pTseng->ClockChip = CLOCKCHIP_ICS5341;
	pTseng->MClkInfo.Programmable = TRUE;
	pTseng->MClkInfo.min = 40000;
	pTseng->MClkInfo.max = 60000;
	pTseng->DacInfo.DacPort16 = TRUE;
	pTseng->DacInfo.rgb24packed.red = 0xff;
	pTseng->DacInfo.rgb24packed.green = 0xff0000;
	pTseng->DacInfo.rgb24packed.blue = 0xff00;
	break;
    case ICS5301_DAC:
	pScrn->progClock = TRUE;
	pTseng->ClockChip = CLOCKCHIP_ICS5301;
	break;
    case STG1702_DAC:
    case STG1700_DAC:
	pTseng->DacInfo.DacPort16 = TRUE;
	break;
    case STG1703_DAC:
	pScrn->progClock = TRUE;
	pTseng->ClockChip = CLOCKCHIP_STG1703;
	pTseng->DacInfo.DacPort16 = TRUE;
	break;
    case CH8398_DAC:
	pScrn->progClock = TRUE;
	pTseng->ClockChip = CLOCKCHIP_CH8398;
	pTseng->DacInfo.DacPort16 = TRUE;
	break;
    default:
	/* defaults already set */
	;
    }
    
    xf86DrvMsg(pScrn->scrnIndex, (pScrn->ramdac) ? X_CONFIG : X_PROBED, "Ramdac: \"%s\"\n",
	xf86TokenToString(TsengDacTable, pTseng->DacInfo.DacType));

    if (cr_saved && pTseng->DacInfo.RamdacShift == 10)
	write_cr(pTseng->dac.saved_cr);
    outb(RAMDAC_RMR, 0xff);
    
    return TRUE;
}

/*
 * The following arrays hold command register values for all possible
 * modes of the 16-bit DACs used on ET4000W32p cards (bpp/pixel_bus_width):
 *
 * { 8bpp/8, 15bpp/8, 16bpp/8, 24bpp/8, 32bpp/8,
 *   8bpp/16, 15bpp/16, 16bpp/16, 24bpp/16, 32bpp/16
 * } 
 *
 * "0xFF" is used as a "not-supported" flag. Assuming no RAMDAC uses this
 * value for some real configuration...
 */
static unsigned char CMD_GENDAC[] =
{0x00, 0x20, 0x60, 0x40, 0xFF,
    0x10, 0x30, 0x50, 0x90, 0x70};

static unsigned char CMD_STG170x[] =
{0x00, 0x08, 0xFF, 0xFF, 0xFF,
    0x05, 0x02, 0x03, 0x09, 0x04};

static unsigned char CMD_CH8398[] =
{0x04, 0xC4, 0x64, 0x74, 0xFF,
    0x24, 0x14, 0x34, 0xB4, 0xFF};

static unsigned char CMD_ATT49x[] =
{0x00, 0xa0, 0xc0, 0xe0, 0xe0,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

#if 0
static unsigned char CMD_SC15025[] =
{0x00, 0xa0, 0xe0, 0x60, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
#endif

static unsigned char CMD_MU4910[] =
{0x1C, 0xBC, 0xDC, 0xFC, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

/*
 * This sets up the RAMDAC registers for the correct BPP and pixmux values.
 * (also set VGA controller registers for pixmux and BPP)
 */
void
tseng_set_ramdac_bpp(ScrnInfoPtr pScrn, DisplayModePtr mode)
{
    Bool rgb555;
    Bool dac16bit; /* use DAC in 16-bit mode if set (W32p only) */
    unsigned char *cmd_array = NULL;
    unsigned char *cmd_dest = NULL;
    int index;
    TsengPtr pTseng = TsengPTR(pScrn);

    rgb555 = (pScrn->weight.red == 5 && pScrn->weight.green == 5
	&& pScrn->weight.blue == 5);  /* rgb565 otherwise */
    
    /* This is not the good way to find out if we're in 8- or 16-bit RAMDAC
     * mode It should rather be passed on from the TsengValidMode() code.
     * Right now it'd better agree with what TsengValidMode() proposed. FIXME
     */
    dac16bit = (mode->PrivFlags == TSENG_MODE_DACBUS16) ||
		(mode->PrivFlags == TSENG_MODE_PIXMUX);

    pTseng->ModeReg.ExtATC &= 0xCF;    /* ATC index 0x16 -- bits-per-PCLK */
    if (Is_ET6K)
	pTseng->ModeReg.ExtATC |= (pTseng->Bytesperpixel - 1) << 4;
    else if (dac16bit)
	pTseng->ModeReg.ExtATC |= 0x20;

    switch (pTseng->DacInfo.DacType) {
    case ATT20C490_DAC:
    case ATT20C491_DAC:
    case ATT20C492_DAC:
    case ATT20C493_DAC:
	cmd_array = CMD_ATT49x;
	cmd_dest = &(pTseng->ModeReg.ATTdac_cmd);
	break;
    case STG1700_DAC:
    case STG1702_DAC:
    case STG1703_DAC:
	pTseng->ModeReg.pll.cmd_reg &= 0x04;	/* keep 7.5 IRE setup setting */
	pTseng->ModeReg.pll.cmd_reg |= 0x18;	/* enable ext regs and pixel modes */
	switch (pTseng->Bytesperpixel) {
	case 2:
	    if (rgb555)
		pTseng->ModeReg.pll.cmd_reg |= 0xA0;
	    else
		pTseng->ModeReg.pll.cmd_reg |= 0xC0;
	    break;
	case 3:
	case 4:
	    pTseng->ModeReg.pll.cmd_reg |= 0xE0;
	    break;
	}
	cmd_array = CMD_STG170x;
	cmd_dest = &(pTseng->ModeReg.pll.ctrl);
	/* set PLL (input) range */
	if (mode->SynthClock <= 16000)
	    pTseng->ModeReg.pll.timingctrl = 0;
	else if (mode->SynthClock <= 32000)
	    pTseng->ModeReg.pll.timingctrl = 1;
	else if (mode->SynthClock <= 67500)
	    pTseng->ModeReg.pll.timingctrl = 2;
	else
	    pTseng->ModeReg.pll.timingctrl = 3;
	break;
    case ICS5341_DAC:
    case ICS5301_DAC:
	cmd_array = CMD_GENDAC;
	pTseng->ModeReg.pll.ctrl = 0;
	cmd_dest = &(pTseng->ModeReg.pll.cmd_reg);
	break;
    case CH8398_DAC:
	cmd_array = CMD_CH8398;
	cmd_dest = &(pTseng->ModeReg.pll.cmd_reg);
	break;
    case ET6000_DAC:
	if (pScrn->bitsPerPixel == 16) {
	    if (rgb555)
		pTseng->ModeReg.ExtET6K[0x58] &= ~0x02;	/* 5-5-5 RGB mode */
	    else
		pTseng->ModeReg.ExtET6K[0x58] |= 0x02;	/* 5-6-5 RGB mode */
	}
	break;
    case MUSIC4910_DAC:
	cmd_array = CMD_MU4910;
	cmd_dest = &(pTseng->ModeReg.ATTdac_cmd);
	break;
    default:
        break;
    }

    if (cmd_array != NULL) {
	switch (pTseng->Bytesperpixel) {
	default:
	case 1:
	    index = 0;
	    break;
	case 2:
	    index = rgb555 ? 1 : 2;
	    break;
	case 3:
	    index = 3;
	    break;
	case 4:
	    index = 4;
	    break;
	}
	if (dac16bit)
	    index += 5;
	if (cmd_array[index] != 0xFF) {
	    if (cmd_dest != NULL) {
		*cmd_dest = cmd_array[index];
	    } else
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR, " cmd_dest = NULL -- please report\n");
	} else {
	    pTseng->ModeReg.pll.cmd_reg = 0;
	    xf86DrvMsg(pScrn->scrnIndex, X_ERROR, " %dbpp not supported in %d-bit DAC mode on this RAMDAC -- Please report.\n",
		pScrn->bitsPerPixel, dac16bit ? 16 : 8);
	}
    }
#ifdef FIXME /* still needed? */
    if (mode->PrivFlags == TSENG_MODE_PIXMUX) {
	VGAHWPTR(pScrn)->ModeReg.CRTC[0x17] &= 0xFB;

	/* to avoid blurred vertical line during flyback, disable H-blanking
	 * (better solution needed !!!)
	 */
	VGAHWPTR(pScrn)->ModeReg.CRTC[0x02] = 0xff;
    }
#endif
}

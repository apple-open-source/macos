
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/tseng/tseng_clock.c,v 1.17 2001/02/15 17:54:55 eich Exp $ */





/*
 *
 * Copyright 1993-1997 The XFree86 Project, Inc.
 *
 */
/**
 ** Clock setting methods for Tseng chips
 **
 ** The *ClockSelect() fucntions are ONLY used used for clock probing!
 ** Setting the actual clock is done in TsengRestore().
 **/

#include "tseng.h"

static Bool Tseng_ET4000ClockSelect(ScrnInfoPtr pScrn, int no);
static Bool Tseng_LegendClockSelect(ScrnInfoPtr pScrn, int no);


static SymTabRec TsengClockChips[] =
{
    {CLOCKCHIP_ICD2061A, "icd2061a"},
    {CLOCKCHIP_ET6000, "et6000"},
    {CLOCKCHIP_ICS5341, "ics5341"},
    {CLOCKCHIP_ICS5301, "ics5301"},
    {CLOCKCHIP_CH8398, "ch8398"},
    {CLOCKCHIP_STG1703, "stg1703"},
    {-1, NULL}
};

Bool
Tseng_check_clockchip(ScrnInfoPtr pScrn)
{
    MessageType from;
    TsengPtr pTseng = TsengPTR(pScrn);

    PDEBUG("	Tseng_check_clockchip\n");

    if (pTseng->pEnt->device->clockchip && *pTseng->pEnt->device->clockchip) {
	/* clockchip given as a string in the config file */
	pScrn->clockchip = pTseng->pEnt->device->clockchip;
	pTseng->ClockChip = xf86StringToToken(TsengClockChips, pScrn->clockchip);
	if (pTseng->ClockChip == -1) {
	    xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "Unknown clockchip: \"%s\"\n",
		pScrn->clockchip);
	    return FALSE;
	}
	from = X_CONFIG;
    } else {
	/* ramdac probe already defined pTseng->ClockChip */
	pScrn->clockchip = (char *)xf86TokenToString(TsengClockChips, pTseng->ClockChip);
	from = X_PROBED;
    }
    xf86DrvMsg(pScrn->scrnIndex, from, "Clockchip: \"%s\"\n",
	pScrn->clockchip);

    return TRUE;
}


void tseng_clock_setup(ScrnInfoPtr pScrn)
{
    TsengPtr pTseng = TsengPTR(pScrn);
    int iobase = VGAHW_GET_IOBASE();
    MessageType from;
    int dacspeed, mem_bw;
    Bool forceSpeed = FALSE;
    int i;

    PDEBUG("	tseng_clock_setup\n");

    /*
     * Memory bandwidth is important in > 8bpp modes, especially on ET4000
     *
     * This code evaluates a video mode with respect to requested dot clock
     * (depends on the VGA chip and the RAMDAC) and the resulting bandwidth
     * demand on memory (which in turn depends on color depth).
     *
     * For each mode, the minimum of max data transfer speed (dot clock
     * limit) and memory bandwidth determines if the mode is allowed.
     *
     * We should also take acceleration into account: accelerated modes
     * strain the bandwidth heavily, because they cause lots of random
     * acesses to video memory, which is bad for bandwidth due to smaller
     * page-mode memory requests.
     */

    /* Set the min pixel clock */
    pTseng->MinClock = 12000;	       /* XXX Guess, need to check this */

    /*
     * If the user has specified ramdac speed in the XF86Config
     * file, we respect that setting.
     */
    if (pTseng->pEnt->device->dacSpeeds[0]) {
	from = X_CONFIG;
	forceSpeed = TRUE;
	switch (pScrn->bitsPerPixel) {
	default:
	case 1:
	case 4:
	case 8:
	    dacspeed = pTseng->pEnt->device->dacSpeeds[DAC_BPP8];
	    break;
	case 16:
	    dacspeed = pTseng->pEnt->device->dacSpeeds[DAC_BPP16];
	    break;
	case 24:
	    dacspeed = pTseng->pEnt->device->dacSpeeds[DAC_BPP24];
	    break;
	case 32:
	    dacspeed = pTseng->pEnt->device->dacSpeeds[DAC_BPP32];
	    break;
	}
	pTseng->max_vco_freq = pTseng->pEnt->device->dacSpeeds[0]*2+1;
	/* if a bpp-specific DacSpeed is not defined, use the "default" one (=8bpp) */
	if (dacspeed == 0)
	    dacspeed = pTseng->pEnt->device->dacSpeeds[0];
    } else {
	from = X_PROBED;
	forceSpeed = FALSE;
	/* default */
	dacspeed = MAX_TSENG_CLOCK;
	/*
	 * According to Tseng (about the ET6000):
	 * "Besides the 135 MHz maximum pixel clock frequency, the other limit has to
	 * do with where you get FIFO breakdown (usually appears as stray horizontal
	 * lines on the screen). Assuming the accelerator is running steadily doing a
	 * worst case operation, to avoid FIFO breakdown you should keep the product
	 *   pixel_clock*(bytes/pixel) <= 225 MHz . This is based on an XCLK
	 * (system/memory) clock of 92 MHz (which is what we currently use) and
	 * a value in the RAS/CAS Configuration register (CFG 44) of either 015h
	 * or 014h (depending on the type of MDRAM chips). Also, the FIFO low
	 * threshold control bit (bit 4 of CFG 41) should be set for modes where
	 * pixel_clock*(bytes/pixel) > 130 MHz . These limits are for the
	 * current ET6000 chips. The ET6100 will raise the pixel clock limit
	 * to 175 MHz and the pixel_clock*(bytes/pixel) FIFO breakdown limit
	 * to about 275 MHz."
	 */
	if (Is_ET6100) {
	    dacspeed	= 175000;
	    mem_bw	= 280000;               /* 275000 is _just_ not enough for 1152x864x24 @ 70Hz */
	} else if (Is_ET6000) {
	    dacspeed	= 135000;
	    mem_bw	= 225000;
	} else {
	    if ( (pTseng->DacInfo.DacPort16) &&
		(pScrn->bitsPerPixel == 8) &&
		(!(DAC_is_GenDAC && pTseng->NoClockchip)) ) {
		    dacspeed = 135000;              /* we can do PIXMUX */
		}
	    mem_bw	= 90000;
	    if (pScrn->videoRam > 1024)
		mem_bw	= 150000;               /* interleaved DRAM gives 70% more bandwidth */
	}
	pTseng->max_vco_freq = dacspeed*2+1;
	/*
	 * "dacspeed" is the theoretical limit imposed by the RAMDAC.
	 * "mem_bw" is the max memory bandwidth in mb/sec available
	 * for the pixel FIFO.
	 * The lowest of the two determines the actual pixel clock limit.
	 */
	dacspeed = min(dacspeed, (mem_bw / pTseng->Bytesperpixel));
    }

    /*
     * Setup the ClockRanges, which describe what clock ranges are available,
     * and what sort of modes they can be used for.
     *
     * First, we set up the default case, and modify it later if needed.
     */
    pTseng->clockRange[0] = xnfcalloc(sizeof(ClockRange), 1);
    pTseng->clockRange[0]->next = NULL;
    pTseng->clockRange[0]->minClock = pTseng->MinClock;
    pTseng->clockRange[0]->maxClock = dacspeed;
    pTseng->clockRange[0]->clockIndex = -1;      /* programmable -- not used */
    pTseng->clockRange[0]->interlaceAllowed = TRUE;
    pTseng->clockRange[0]->doubleScanAllowed = TRUE;
    pTseng->clockRange[0]->ClockMulFactor = 1;
    pTseng->clockRange[0]->ClockDivFactor = 1;
    pTseng->clockRange[0]->PrivFlags = TSENG_MODE_NORMAL;
    
    /*
     * Handle PIXMUX modes.
     *
     * NOTE: We disable PIXMUX when clockchip programming on the GenDAC
     * family is disabled. PIXMUX requires that the N2 post-divider in the
     * PLL clock programming word is >= 2, which is not always true for the
     * default (BIOS) clocks programmed in the 8 clock registers.
     */
    if ( (pTseng->DacInfo.DacPort16) &&
	(pScrn->bitsPerPixel == 8) &&
	(!(DAC_is_GenDAC && pTseng->NoClockchip)) ) {
	pTseng->clockRange[0]->maxClock = MAX_TSENG_CLOCK;
	/* set up 2nd clock range for PIXMUX modes */
	pTseng->clockRange[1] = xnfcalloc(sizeof(ClockRange), 1);
	pTseng->clockRange[0]->next = pTseng->clockRange[1];
	pTseng->clockRange[1]->next = NULL;
	pTseng->clockRange[1]->minClock = 75000;
	pTseng->clockRange[1]->maxClock = dacspeed;
	pTseng->clockRange[1]->clockIndex = -1;      /* programmable -- not used */
	pTseng->clockRange[1]->interlaceAllowed = TRUE;
	pTseng->clockRange[1]->doubleScanAllowed = TRUE;
	pTseng->clockRange[1]->ClockMulFactor = 1;
	pTseng->clockRange[1]->ClockDivFactor = 2;
	pTseng->clockRange[1]->PrivFlags = TSENG_MODE_PIXMUX;
    }

    /*
     * Handle 16/24/32 bpp modes that require some form of clock scaling. We
     * can have either 8-bit DACs that require "bytesperpixel" clocks per
     * pixel, or 16-bit DACs that can transport 8 or 16 bits per clock.
     */
     if ((pTseng->Bytesperpixel > 1) && (!Is_ET6K)) {
        /* in either 8 or 16-bit DAC case, we can use an 8-bit interface */
	pTseng->clockRange[0]->maxClock = (forceSpeed) ? dacspeed :
	    min(MAX_TSENG_CLOCK / pTseng->Bytesperpixel, dacspeed);
	pTseng->clockRange[0]->ClockMulFactor = pTseng->Bytesperpixel;
	pTseng->clockRange[0]->ClockDivFactor = 1;
	/* in addition, 16-bit DACs can also transport 2 bytes per clock */
	if (pTseng->DacInfo.DacPort16) {
	    pTseng->clockRange[1] = xnfcalloc(sizeof(ClockRange), 1);
	    pTseng->clockRange[0]->next = pTseng->clockRange[1];
	    pTseng->clockRange[1]->next = NULL;
	    pTseng->clockRange[1]->minClock = pTseng->MinClock;
	    pTseng->clockRange[1]->maxClock = (forceSpeed) ? dacspeed :
		min((MAX_TSENG_CLOCK * 2) / pTseng->Bytesperpixel, dacspeed);
	    pTseng->clockRange[1]->clockIndex = -1;      /* programmable -- not used */
	    pTseng->clockRange[1]->interlaceAllowed = TRUE;
	    pTseng->clockRange[1]->doubleScanAllowed = TRUE;
	    pTseng->clockRange[1]->ClockMulFactor = pTseng->Bytesperpixel;
	    pTseng->clockRange[1]->ClockDivFactor = 2;
	    pTseng->clockRange[1]->PrivFlags = TSENG_MODE_DACBUS16;
	}
    }

    if (pTseng->clockRange[1])
	pTseng->MaxClock = pTseng->clockRange[1]->maxClock;
    else
	pTseng->MaxClock = pTseng->clockRange[0]->maxClock;

    xf86DrvMsg(pScrn->scrnIndex, X_DEFAULT, "Min pixel clock is %d MHz\n",
	pTseng->MinClock / 1000);
    xf86DrvMsg(pScrn->scrnIndex, from, "Max pixel clock is %d MHz\n",
	pTseng->MaxClock / 1000);

    /* Memory clock setup */
    pTseng->MClkInfo.Set = FALSE;
    /* Only set MemClk if appropriate for the ramdac */
    if (pTseng->MClkInfo.Programmable) {
	from = X_PROBED;
	if (pTseng->MemClk > 0) {
	    if ((pTseng->MemClk < pTseng->MClkInfo.min)
		|| (pTseng->MemClk > pTseng->MClkInfo.max)) {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		    "MCLK %d MHz out of range (=%d..%d); not changed!\n",
		    pTseng->MemClk / 1000,
		    pTseng->MClkInfo.min / 1000,
		    pTseng->MClkInfo.max / 1000);
	    } else {
		pTseng->MClkInfo.MemClk = pTseng->MemClk;
		pTseng->MClkInfo.Set = TRUE;
		from = X_CONFIG;
	    }
	}
	xf86DrvMsg(pScrn->scrnIndex, from, "MCLK used is %d MHz\n",
	    pTseng->MClkInfo.MemClk / 1000);
    }
    
    /*
     * Set up the list-of-clocks stuff if we don't have a programmable
     * clockchip (the RAMDAC probe sets the pScrn->progClock field).
     */
    if (!pScrn->progClock) {
	int NoClocks;
	Bool (*TsengClockSelect)(ScrnInfoPtr, int);

	/* first determine how many clocks there are (or can be) */
	if (pTseng->Legend) {
	    TsengClockSelect = Tseng_LegendClockSelect;
	    NoClocks = 32;
	} else {
	    TsengClockSelect = Tseng_ET4000ClockSelect;
	    /*
	     * The CH8398 RAMDAC uses CS3 for register selection (RS2), not for clock selection.
	     * The GenDAC family only has 8 clocks. Together with MCLK/2, that's 16 clocks.
	     */
	    if ( (!Is_stdET4K)
		&& (!DAC_is_GenDAC) && (pTseng->DacInfo.DacType != CH8398_DAC) )
		    NoClocks = 32;
	    else
		    NoClocks = 16;
	    }
	/* now probe for the clocks if they are not specified */
	if (!pTseng->pEnt->device->numclocks) {
	    pScrn->numClocks = NoClocks;
	    xf86GetClocks(pScrn, NoClocks, TsengClockSelect,
			  TsengProtect, TsengBlankScreen,
			  iobase + 0x0A, 0x08, 1, 28322);
	    from = X_PROBED;
	} else {
	    pScrn->numClocks = pTseng->pEnt->device->numclocks;
	    if (pScrn->numClocks > NoClocks) {
		xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
			   "Too many Clocks specified in configuration file.\n");
		xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
			   "\t\tAt most %d clocks may be specified\n", NoClocks);
		pScrn->numClocks= NoClocks;
	    }
	    for (i = 0; i < pScrn->numClocks; i++)
		pScrn->clock[i] = pTseng->pEnt->device->clock[i];
	    from = X_CONFIG;
	}
	/*
	 * Scale clocks for current bpp depending on RAMDAC type, and print
	 * out the list of clocks used.
	 */

#ifdef FIXME
	for (i = 0; i < pScrn->numClocks; i++) {
	    pScrn->clock[i] *= pTseng->DacInfo.ClockDivFactor;
	    pScrn->clock[i] /= pTseng->DacInfo.ClockMulFactor;
	}
#endif
	xf86ShowClocks(pScrn, from);
    }
}


/*
 * ET4000ClockSelect --
 *      select one of the possible clocks ...
 */

static Bool
Tseng_ET4000ClockSelect(ScrnInfoPtr pScrn, int no)
{
    TsengPtr pTseng = TsengPTR(pScrn);
    unsigned char temp;
    int iobase = VGAHW_GET_IOBASE();

    switch (no) {
    case CLK_REG_SAVE:
	pTseng->save_clock.save1 = inb(0x3CC);
	outb(iobase + 4, 0x34);
	pTseng->save_clock.save2 = inb(iobase + 5);
	outb(0x3C4, 7);
	pTseng->save_clock.save3 = inb(0x3C5);
	if (!Is_stdET4K) {
	    outb(iobase + 4, 0x31);
	    pTseng->save_clock.save4 = inb(iobase + 5);
	}
	break;
    case CLK_REG_RESTORE:
	outb(0x3C2, pTseng->save_clock.save1);
	outw(iobase + 4, 0x34 | (pTseng->save_clock.save2 << 8));
	outw(0x3C4, 7 | (pTseng->save_clock.save3 << 8));
	if (!Is_stdET4K) {
	    outw(iobase + 4, 0x31 | (pTseng->save_clock.save4 << 8));
	}
	break;
    default:
	/* CS0,CS1 = clock select bits 0,1 */
	temp = inb(0x3CC);
	outb(0x3C2, (temp & 0xf3) | ((no << 2) & 0x0C));
	/* CS2 = clock select bit 2 */
	outb(iobase + 4, 0x34);     /* don't nuke the other bits in CR34 */
	temp = inb(iobase + 5);
	outw(iobase + 4, 0x34 | ((temp & 0xFD) << 8) | ((no & 0x04) << 7));
	/* CS3 = clock select bit 4 */
	outb(0x3C4, 7);
	temp = inb(0x3C5);
	outb(0x3C5, (pTseng->save_divide ^ ((no & 0x8) << 3)) | (temp & 0xBF));
	/* CS4 = MCLK/2 */
	outb(iobase + 4, 0x31);
	temp = inb(iobase + 5);
	outb(iobase + 5, (temp & 0x3f) | ((no & 0x10) << 2));
    }
    return (TRUE);
}

/*
 * LegendClockSelect --
 *      select one of the possible clocks ...
 */

static Bool
Tseng_LegendClockSelect(ScrnInfoPtr pScrn, int no)
{
    /*
     * Sigma Legend special handling
     *
     * The Legend uses an ICS 1394-046 clock generator.  This can generate 32
     * different frequencies.  The Legend can use all 32.  Here's how:
     *
     * There are two flip/flops used to latch two inputs into the ICS clock
     * generator.  The five inputs to the ICS are then
     *
     * ICS     ET-4000
     * ---     ---
     * FS0     CS0
     * FS1     CS1
     * FS2     ff0     flip/flop 0 output
     * FS3     CS2
     * FS4     ff1     flip/flop 1 output
     *
     * The flip/flops are loaded from CS0 and CS1.  The flip/flops are
     * latched by CS2, on the rising edge. After CS2 is set low, and then high,
     * it is then set to its final value.
     *
     */
    TsengPtr pTseng = TsengPTR(pScrn);
    unsigned char temp;
    int iobase = VGAHW_GET_IOBASE();

    switch (no) {
    case CLK_REG_SAVE:
	pTseng->save_clock.save1 = inb(0x3CC);
	outb(iobase + 4, 0x34);
	pTseng->save_clock.save2 = inb(iobase + 5);
	break;
    case CLK_REG_RESTORE:
	outb(0x3C2, pTseng->save_clock.save1);
	outw(iobase + 4, 0x34 | (pTseng->save_clock.save2 << 8));
	break;
    default:
	temp = inb(0x3CC);
	outb(0x3C2, (temp & 0xF3) | ((no & 0x10) >> 1) | (no & 0x04));
	outw(iobase + 4, 0x0034);
	outw(iobase + 4, 0x0234);
	outw(iobase + 4, ((no & 0x08) << 6) | 0x34);
	outb(0x3C2, (temp & 0xF3) | ((no << 2) & 0x0C));
    }
    return (TRUE);
}


#define BASE_FREQ         14.31818     /* MHz */
void
TsengcommonCalcClock(long freq, int min_m, int min_n1, int max_n1, int min_n2, int max_n2,
    long freq_min, long freq_max,
    unsigned char *mdiv, unsigned char *ndiv)
{
    double ffreq, ffreq_min, ffreq_max;
    double div, diff, best_diff;
    unsigned int m;
    unsigned char n1, n2;
    unsigned char best_n1 = 16 + 2, best_n2 = 2, best_m = 125 + 2;

    PDEBUG("	commonCalcClock\n");
    ffreq = freq / 1000.0 / BASE_FREQ;
    ffreq_min = freq_min / 1000.0 / BASE_FREQ;
    ffreq_max = freq_max / 1000.0 / BASE_FREQ;

    if (ffreq < ffreq_min / (1 << max_n2)) {
	ErrorF("invalid frequency %1.3f MHz  [freq >= %1.3f MHz]\n",
	    ffreq * BASE_FREQ, ffreq_min * BASE_FREQ / (1 << max_n2));
	ffreq = ffreq_min / (1 << max_n2);
    }
    if (ffreq > ffreq_max / (1 << min_n2)) {
	ErrorF("invalid frequency %1.3f MHz  [freq <= %1.3f MHz]\n",
	    ffreq * BASE_FREQ, ffreq_max * BASE_FREQ / (1 << min_n2));
	ffreq = ffreq_max / (1 << min_n2);
    }
    /* work out suitable timings */

    best_diff = ffreq;

    for (n2 = min_n2; n2 <= max_n2; n2++) {
	for (n1 = min_n1 + 2; n1 <= max_n1 + 2; n1++) {
	    m = (int)(ffreq * n1 * (1 << n2) + 0.5);
	    if (m < min_m + 2 || m > 127 + 2)
		continue;
	    div = (double)(m) / (double)(n1);
	    if ((div >= ffreq_min) &&
		(div <= ffreq_max)) {
		diff = ffreq - div / (1 << n2);
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

#ifdef EXTENDED_DEBUG
    ErrorF("Clock parameters for %1.6f MHz: m=%d, n1=%d, n2=%d\n",
	((double)(best_m) / (double)(best_n1) / (1 << best_n2)) * BASE_FREQ,
	best_m - 2, best_n1 - 2, best_n2);
#endif

    if (max_n1 == 63)
	*ndiv = (best_n1 - 2) | (best_n2 << 6);
    else
	*ndiv = (best_n1 - 2) | (best_n2 << 5);
    *mdiv = best_m - 2;
}


/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/ati/atidsp.c,v 1.22 2004/01/05 16:42:02 tsi Exp $ */
/*
 * Copyright 1997 through 2004 by Marc Aurele La France (TSI @ UQV), tsi@xfree86.org
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that copyright
 * notice and this permission notice appear in supporting documentation, and
 * that the name of Marc Aurele La France not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  Marc Aurele La France makes no representations
 * about the suitability of this software for any purpose.  It is provided
 * "as-is" without express or implied warranty.
 *
 * MARC AURELE LA FRANCE DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS.  IN NO
 * EVENT SHALL MARC AURELE LA FRANCE BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#include "ati.h"
#include "atichip.h"
#include "aticrtc.h"
#include "atidsp.h"
#include "atimach64io.h"
#include "atividmem.h"

/*
 * ATIDSPPreInit --
 *
 * This function initialises global variables used to set DSP registers on a
 * VT-B or later.
 */
Bool
ATIDSPPreInit
(
    int    iScreen,
    ATIPtr pATI
)
{
    CARD32 IOValue, dsp_config, dsp_on_off, vga_dsp_config, vga_dsp_on_off;
    int trp;

    /*
     * VT-B's and later have additional post-dividers that are not powers of
     * two.
     */
    pATI->ClockDescriptor.NumD = 8;

    /* Retrieve XCLK settings */
    IOValue = ATIMach64GetPLLReg(PLL_XCLK_CNTL);
    pATI->XCLKPostDivider = GetBits(IOValue, PLL_XCLK_SRC_SEL);
    pATI->XCLKReferenceDivider = 1;
    switch (pATI->XCLKPostDivider)
    {
        case 0:  case 1:  case 2:  case 3:
            break;

        case 4:
            pATI->XCLKReferenceDivider = 3;
            pATI->XCLKPostDivider = 0;
            break;

        default:
            xf86DrvMsg(iScreen, X_ERROR,
                "Unsupported XCLK source:  %d.\n", pATI->XCLKPostDivider);
            return FALSE;
    }

    pATI->XCLKPostDivider -= GetBits(IOValue, PLL_MFB_TIMES_4_2B);
    pATI->XCLKFeedbackDivider = ATIMach64GetPLLReg(PLL_MCLK_FB_DIV);

    xf86DrvMsgVerb(iScreen, X_INFO, 2,
        "Engine XCLK %.3f MHz;  Refresh rate code %ld.\n",
        ATIDivide(pATI->XCLKFeedbackDivider * pATI->ReferenceNumerator,
                  pATI->XCLKReferenceDivider * pATI->ClockDescriptor.MaxM *
                  pATI->ReferenceDenominator, 1 - pATI->XCLKPostDivider, 0) /
        (double)1000.0,
        GetBits(pATI->LockData.mem_cntl, CTL_MEM_REFRESH_RATE_B));

    /* Compute maximum RAS delay and friends */
    trp = GetBits(pATI->LockData.mem_cntl, CTL_MEM_TRP);
    pATI->XCLKPageFaultDelay = GetBits(pATI->LockData.mem_cntl, CTL_MEM_TRCD) +
        GetBits(pATI->LockData.mem_cntl, CTL_MEM_TCRD) + trp + 2;
    pATI->XCLKMaxRASDelay = GetBits(pATI->LockData.mem_cntl, CTL_MEM_TRAS) +
        trp + 2;
    pATI->DisplayFIFODepth = 32;

    if (pATI->Chip < ATI_CHIP_264VT4)
    {
        pATI->XCLKPageFaultDelay += 2;
        pATI->XCLKMaxRASDelay += 3;
        pATI->DisplayFIFODepth = 24;
    }

    switch (pATI->MemoryType)
    {
        case MEM_264_DRAM:
            if (pATI->VideoRAM <= 1024)
            {
                pATI->DisplayLoopLatency = 10;
            }
            else
            {
                pATI->DisplayLoopLatency = 8;
                pATI->XCLKPageFaultDelay += 2;
            }
            break;

        case MEM_264_EDO:
        case MEM_264_PSEUDO_EDO:
            if (pATI->VideoRAM <= 1024)
            {
                pATI->DisplayLoopLatency = 9;
            }
            else
            {
                pATI->DisplayLoopLatency = 8;
                pATI->XCLKPageFaultDelay++;
            }
            break;

        case MEM_264_SDRAM:
            if (pATI->VideoRAM <= 1024)
            {
                pATI->DisplayLoopLatency = 11;
            }
            else
            {
                pATI->DisplayLoopLatency = 10;
                pATI->XCLKPageFaultDelay++;
            }
            break;

        case MEM_264_SGRAM:
            pATI->DisplayLoopLatency = 8;
            pATI->XCLKPageFaultDelay += 3;
            break;

        default:                /* Set maximums */
            pATI->DisplayLoopLatency = 11;
            pATI->XCLKPageFaultDelay += 3;
            break;
    }

    if (pATI->XCLKMaxRASDelay <= pATI->XCLKPageFaultDelay)
        pATI->XCLKMaxRASDelay = pATI->XCLKPageFaultDelay + 1;

    /* Allow BIOS to override */
    dsp_config = inr(DSP_CONFIG);
    dsp_on_off = inr(DSP_ON_OFF);
    vga_dsp_config = inr(VGA_DSP_CONFIG);
    vga_dsp_on_off = inr(VGA_DSP_ON_OFF);

    if (dsp_config)
        pATI->DisplayLoopLatency = GetBits(dsp_config, DSP_LOOP_LATENCY);

    if ((!dsp_on_off && (pATI->Chip < ATI_CHIP_264GTPRO)) ||
        ((dsp_on_off == vga_dsp_on_off) &&
         (!dsp_config || !((dsp_config ^ vga_dsp_config) & DSP_XCLKS_PER_QW))))
    {
        if (ATIDivide(GetBits(vga_dsp_on_off, VGA_DSP_OFF),
                      GetBits(vga_dsp_config, VGA_DSP_XCLKS_PER_QW), 5, 1) > 24)
            pATI->DisplayFIFODepth = 32;
        else
            pATI->DisplayFIFODepth = 24;
    }

    return TRUE;
}

/*
 * ATIDSPSave --
 *
 * This function is called to remember DSP register values on VT-B and later
 * controllers.
 */
void
ATIDSPSave
(
    ATIPtr   pATI,
    ATIHWPtr pATIHW
)
{
    pATIHW->dsp_on_off = inr(DSP_ON_OFF);
    pATIHW->dsp_config = inr(DSP_CONFIG);
}


/*
 * ATIDSPCalculate --
 *
 * This function sets up DSP register values for a VTB or later.  Note that
 * this would be slightly different if VCLK 0 or 1 were used for the mode
 * instead.  In that case, this function would set VGA_DSP_CONFIG and
 * VGA_DSP_ON_OFF, would have to zero out DSP_CONFIG and DSP_ON_OFF, and would
 * have to consider that VGA_DSP_CONFIG is partitioned slightly differently
 * than DSP_CONFIG.
 */
void
ATIDSPCalculate
(
    ATIPtr         pATI,
    ATIHWPtr       pATIHW,
    DisplayModePtr pMode
)
{
    int Multiplier, Divider;
    int RASMultiplier = pATI->XCLKMaxRASDelay, RASDivider = 1;
    int dsp_precision, dsp_on, dsp_off, dsp_xclks;
    int tmp, vshift, xshift;

#   define Maximum_DSP_PRECISION ((int)MaxBits(DSP_PRECISION))

    /* Compute a memory-to-screen bandwidth ratio */
    Multiplier = pATI->XCLKFeedbackDivider *
        pATI->ClockDescriptor.PostDividers[pATIHW->PostDivider];
    Divider = pATIHW->FeedbackDivider * pATI->XCLKReferenceDivider;

#ifndef AVOID_CPIO

    if (pATI->depth >= 8)

#endif /* AVOID_CPIO */

    {
        Divider *= pATI->bitsPerPixel / 4;
    }

    /* Start by assuming a display FIFO width of 64 bits */
    vshift = (6 - 2) - pATI->XCLKPostDivider;

#ifndef AVOID_CPIO

    if (pATIHW->crtc == ATI_CRTC_VGA)
        vshift--;               /* Nope, it's 32 bits wide */

#endif /* AVOID_CPIO */

    if (pATI->OptionPanelDisplay && (pATI->LCDPanelID >= 0))
    {
        /* Compensate for horizontal stretching */
        Multiplier *= pATI->LCDHorizontal;
        Divider *= pMode->HDisplay & ~7;

        RASMultiplier *= pATI->LCDHorizontal;
        RASDivider *= pMode->HDisplay & ~7;
    }

    /* Determine dsp_precision first */
    tmp = ATIDivide(Multiplier * pATI->DisplayFIFODepth, Divider, vshift, -1);
    for (dsp_precision = -5;  tmp;  dsp_precision++)
        tmp >>= 1;
    if (dsp_precision < 0)
        dsp_precision = 0;
    else if (dsp_precision > Maximum_DSP_PRECISION)
        dsp_precision = Maximum_DSP_PRECISION;

    xshift = 6 - dsp_precision;
    vshift += xshift;

    /* Move on to dsp_off */
    dsp_off = ATIDivide(Multiplier * (pATI->DisplayFIFODepth - 1), Divider,
        vshift, -1) - ATIDivide(1, 1, vshift - xshift, 1);

    /* Next is dsp_on */

#ifndef AVOID_CPIO

    if ((pATIHW->crtc == ATI_CRTC_VGA) /* && (dsp_precision < 3) */)
    {
        /*
         * TODO:  I don't yet know why something like this appears necessary.
         *        But I don't have time to explore this right now.
         */
        dsp_on = ATIDivide(Multiplier * 5, Divider, vshift + 2, 1);
    }
    else

#endif /* AVOID_CPIO */

    {
        dsp_on = ATIDivide(Multiplier, Divider, vshift, 1);
        tmp = ATIDivide(RASMultiplier, RASDivider, xshift, 1);
        if (dsp_on < tmp)
            dsp_on = tmp;
        dsp_on += (tmp * 2) +
            ATIDivide(pATI->XCLKPageFaultDelay, 1, xshift, 1);
    }

    /* Calculate rounding factor and apply it to dsp_on */
    tmp = ((1 << (Maximum_DSP_PRECISION - dsp_precision)) - 1) >> 1;
    dsp_on = ((dsp_on + tmp) / (tmp + 1)) * (tmp + 1);

    if (dsp_on >= ((dsp_off / (tmp + 1)) * (tmp + 1)))
    {
        dsp_on = dsp_off - ATIDivide(Multiplier, Divider, vshift, -1);
        dsp_on = (dsp_on / (tmp + 1)) * (tmp + 1);
    }

    /* Last but not least:  dsp_xclks */
    dsp_xclks = ATIDivide(Multiplier, Divider, vshift + 5, 1);

    /* Build DSP register contents */
    pATIHW->dsp_on_off = SetBits(dsp_on, DSP_ON) |
        SetBits(dsp_off, DSP_OFF);
    pATIHW->dsp_config = SetBits(dsp_precision, DSP_PRECISION) |
        SetBits(dsp_xclks, DSP_XCLKS_PER_QW) |
        SetBits(pATI->DisplayLoopLatency, DSP_LOOP_LATENCY);
}

/*
 * ATIDSPSet --
 *
 * This function is called to set DSP registers on VT-B and later controllers.
 */
void
ATIDSPSet
(
    ATIPtr   pATI,
    ATIHWPtr pATIHW
)
{
    outr(DSP_ON_OFF, pATIHW->dsp_on_off);
    outr(DSP_CONFIG, pATIHW->dsp_config);
}

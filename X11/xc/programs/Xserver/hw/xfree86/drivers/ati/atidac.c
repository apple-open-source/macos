/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/ati/atidac.c,v 1.18 2003/02/25 17:58:13 tsi Exp $ */
/*
 * Copyright 1997 through 2003 by Marc Aurele La France (TSI @ UQV), tsi@xfree86.org
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
#include "atidac.h"
#include "atimach64io.h"
#include "atimono.h"

/*
 * RAMDAC-related definitions.
 */
const SymTabRec ATIDACDescriptors[] =
{   /* Keep this table in ascending DACType order */
    {ATI_DAC_ATI68830,      "ATI 68830 or similar"},
    {ATI_DAC_SC11483,       "Sierra 11483 or similar"},
    {ATI_DAC_ATI68875,      "ATI 68875 or similar"},
    {ATI_DAC_TVP3026_A,     "TI ViewPoint3026 or similar"},
    {ATI_DAC_GENERIC,       "Brooktree 476 or similar"},
    {ATI_DAC_BT481,         "Brooktree 481 or similar"},
    {ATI_DAC_ATT20C491,     "AT&T 20C491 or similar"},
    {ATI_DAC_SC15026,       "Sierra 15026 or similar"},
    {ATI_DAC_MU9C1880,      "Music 9C1880 or similar"},
    {ATI_DAC_IMSG174,       "Inmos G174 or similar"},
    {ATI_DAC_ATI68860_B,    "ATI 68860 (Revision B) or similar"},
    {ATI_DAC_ATI68860_C,    "ATI 68860 (Revision C) or similar"},
    {ATI_DAC_TVP3026_B,     "TI ViewPoint3026 or similar"},
    {ATI_DAC_STG1700,       "SGS-Thompson 1700 or similar"},
    {ATI_DAC_ATT20C498,     "AT&T 20C498 or similar"},
    {ATI_DAC_STG1702,       "SGS-Thompson 1702 or similar"},
    {ATI_DAC_SC15021,       "Sierra 15021 or similar"},
    {ATI_DAC_ATT21C498,     "AT&T 21C498 or similar"},
    {ATI_DAC_STG1703,       "SGS-Thompson 1703 or similar"},
    {ATI_DAC_CH8398,        "Chrontel 8398 or similar"},
    {ATI_DAC_ATT20C408,     "AT&T 20C408 or similar"},
    {ATI_DAC_INTERNAL,      "Internal"},
    {ATI_DAC_IBMRGB514,     "IBM RGB 514 or similar"},
    {ATI_DAC_UNKNOWN,       "Unknown"}          /* Must be last */
};

#ifndef AVOID_CPIO

/*
 * ATISetDACIOPorts --
 *
 * This function sets up DAC access I/O port numbers.
 */
void
ATISetDACIOPorts
(
    ATIPtr      pATI,
    ATICRTCType crtc
)
{
    switch (crtc)
    {
        case ATI_CRTC_VGA:
            pATI->CPIO_DAC_DATA = VGA_DAC_DATA;
            pATI->CPIO_DAC_MASK = VGA_DAC_MASK;
            pATI->CPIO_DAC_READ = VGA_DAC_READ;
            pATI->CPIO_DAC_WRITE = VGA_DAC_WRITE;
            pATI->CPIO_DAC_WAIT = GENS1(pATI->CPIO_VGABase);
            break;

        case ATI_CRTC_8514:
            pATI->CPIO_DAC_DATA = IBM_DAC_DATA;
            pATI->CPIO_DAC_MASK = IBM_DAC_MASK;
            pATI->CPIO_DAC_READ = IBM_DAC_READ;
            pATI->CPIO_DAC_WRITE = IBM_DAC_WRITE;
            pATI->CPIO_DAC_WAIT = pATI->CPIO_DAC_MASK;
            break;

        case ATI_CRTC_MACH64:
            pATI->CPIO_DAC_DATA = ATIIOPort(DAC_REGS) + 1;
            pATI->CPIO_DAC_MASK = ATIIOPort(DAC_REGS) + 2;
            pATI->CPIO_DAC_READ = ATIIOPort(DAC_REGS) + 3;
            pATI->CPIO_DAC_WRITE = ATIIOPort(DAC_REGS) + 0;
            pATI->CPIO_DAC_WAIT = pATI->CPIOBase;
            break;

        default:
            break;
    }
}

#endif /* AVOID_CPIO */

/*
 * ATIGetDACCmdReg --
 *
 * Setup to access a RAMDAC's command register.
 */
CARD8
ATIGetDACCmdReg
(
    ATIPtr pATI
)
{

#ifdef AVOID_CPIO

    (void)in8(M64_DAC_WRITE);           /* Reset to PEL mode */
    (void)in8(M64_DAC_MASK);
    (void)in8(M64_DAC_MASK);
    (void)in8(M64_DAC_MASK);
    return in8(M64_DAC_MASK);

#else /* AVOID_CPIO */

    (void)inb(pATI->CPIO_DAC_WRITE);    /* Reset to PEL mode */
    (void)inb(pATI->CPIO_DAC_MASK);
    (void)inb(pATI->CPIO_DAC_MASK);
    (void)inb(pATI->CPIO_DAC_MASK);
    return inb(pATI->CPIO_DAC_MASK);

#endif /* AVOID_CPIO */

}

/*
 * ATIDACPreInit --
 *
 * This function initialises the fields in an ATIHWRec that relate to DACs.
 */
void
ATIDACPreInit
(
    ScrnInfoPtr pScreenInfo,
    ATIPtr      pATI,
    ATIHWPtr    pATIHW
)
{
    int Index, Index2;
    CARD8 maxColour = (1 << pATI->rgbBits) - 1;

    pATIHW->dac_read = pATIHW->dac_write = 0x00U;
    pATIHW->dac_mask = 0xFFU;

    /*
     * Set colour lookup table.  The first entry has already been zeroed out.
     */
    if (pATI->depth > 8)
        for (Index = 1;  Index < (NumberOf(pATIHW->lut) / 3);  Index++)
        {
            Index2 = Index * 3;
            pATIHW->lut[Index2 + 0] =
                pATIHW->lut[Index2 + 1] =
                pATIHW->lut[Index2 + 2] = Index;
        }
    else
    {
        /*
         * Initialise hardware colour map so that use of uninitialised
         * software colour map entries can easily be seen.  For 256-colour
         * modes, this doesn't remain effective for very long...
         */
        pATIHW->lut[3] = pATIHW->lut[4] = pATIHW->lut[5] = 0xFFU;
        for (Index = 2;  Index < (NumberOf(pATIHW->lut) / 3);  Index++)
        {
            Index2 = Index * 3;
            pATIHW->lut[Index2 + 0] = maxColour;
            pATIHW->lut[Index2 + 1] = 0x00U;
            pATIHW->lut[Index2 + 2] = maxColour;
        }

#ifndef AVOID_CPIO

        if (pATI->depth == 1)
        {
            rgb blackColour = pScreenInfo->display->blackColour,
                whiteColour = pScreenInfo->display->whiteColour;

            if (blackColour.red > maxColour)
                blackColour.red = maxColour;
            if (blackColour.green > maxColour)
                blackColour.green = maxColour;
            if (blackColour.blue > maxColour)
                blackColour.blue = maxColour;
            if (whiteColour.red > maxColour)
                whiteColour.red = maxColour;
            if (whiteColour.green > maxColour)
                whiteColour.green = maxColour;
            if (whiteColour.blue > maxColour)
                whiteColour.blue = maxColour;

            if ((blackColour.red == whiteColour.red) &&
                (blackColour.green == whiteColour.green) &&
                (blackColour.blue == whiteColour.blue))
            {
                blackColour.red ^= maxColour;
                blackColour.green ^= maxColour;
                blackColour.blue ^= maxColour;
            }

            pATIHW->lut[(MONO_BLACK * 3) + 0] = blackColour.red;
            pATIHW->lut[(MONO_BLACK * 3) + 1] = blackColour.green;
            pATIHW->lut[(MONO_BLACK * 3) + 2] = blackColour.blue;
            pATIHW->lut[(MONO_WHITE * 3) + 0] = whiteColour.red;
            pATIHW->lut[(MONO_WHITE * 3) + 1] = whiteColour.green;
            pATIHW->lut[(MONO_WHITE * 3) + 2] = whiteColour.blue;
        }

        if (pATIHW->crtc == ATI_CRTC_VGA)
        {
            /* Initialise overscan to black */
            Index = pATIHW->attr[17] * 3;
            pATIHW->lut[Index + 0] =
                pATIHW->lut[Index + 1] =
                pATIHW->lut[Index + 2] = 0x00U;
        }

#endif /* AVOID_CPIO */

    }
}

/*
 * ATIDACSave --
 *
 * This function is called to save the current RAMDAC state into an ATIHWRec
 * structure occurrence.
 */
void
ATIDACSave
(
    ATIPtr   pATI,
    ATIHWPtr pATIHW
)
{
    int Index;

#ifdef AVOID_CPIO

    pATIHW->dac_read = in8(M64_DAC_READ);
    DACDelay;
    pATIHW->dac_write = in8(M64_DAC_WRITE);
    DACDelay;
    pATIHW->dac_mask = in8(M64_DAC_MASK);
    DACDelay;

    /* Save DAC's colour lookup table */
    out8(M64_DAC_MASK, 0xFFU);
    DACDelay;
    out8(M64_DAC_READ, 0x00U);
    DACDelay;
    for (Index = 0;  Index < NumberOf(pATIHW->lut);  Index++)
    {
        pATIHW->lut[Index] = in8(M64_DAC_DATA);
        DACDelay;
    }

    out8(M64_DAC_MASK, pATIHW->dac_mask);
    DACDelay;
    out8(M64_DAC_READ, pATIHW->dac_read);
    DACDelay;

#else /* AVOID_CPIO */

    ATISetDACIOPorts(pATI, pATIHW->crtc);

    pATIHW->dac_read = inb(pATI->CPIO_DAC_READ);
    DACDelay;
    pATIHW->dac_write = inb(pATI->CPIO_DAC_WRITE);
    DACDelay;
    pATIHW->dac_mask = inb(pATI->CPIO_DAC_MASK);
    DACDelay;

    /* Save DAC's colour lookup table */
    outb(pATI->CPIO_DAC_MASK, 0xFFU);
    DACDelay;
    outb(pATI->CPIO_DAC_READ, 0x00U);
    DACDelay;
    for (Index = 0;  Index < NumberOf(pATIHW->lut);  Index++)
    {
        pATIHW->lut[Index] = inb(pATI->CPIO_DAC_DATA);
        DACDelay;
    }

    outb(pATI->CPIO_DAC_MASK, pATIHW->dac_mask);
    DACDelay;
    outb(pATI->CPIO_DAC_READ, pATIHW->dac_read);
    DACDelay;

#endif /* AVOID_CPIO */

}

/*
 * ATIDACSet --
 *
 * This function loads RAMDAC data from an ATIHWRec structure occurrence.
 */
void
ATIDACSet
(
    ATIPtr   pATI,
    ATIHWPtr pATIHW
)
{
    int Index;

#ifdef AVOID_CPIO

    /* Load DAC's colour lookup table */
    out8(M64_DAC_MASK, 0xFFU);
    DACDelay;
    out8(M64_DAC_WRITE, 0x00U);
    DACDelay;
    for (Index = 0;  Index < NumberOf(pATIHW->lut);  Index++)
    {
        out8(M64_DAC_DATA, pATIHW->lut[Index]);
        DACDelay;
    }

    out8(M64_DAC_MASK, pATIHW->dac_mask);
    DACDelay;
    out8(M64_DAC_READ, pATIHW->dac_read);
    DACDelay;
    out8(M64_DAC_WRITE, pATIHW->dac_write);
    DACDelay;

#else /* AVOID_CPIO */

    ATISetDACIOPorts(pATI, pATIHW->crtc);

    /* Load DAC's colour lookup table */
    outb(pATI->CPIO_DAC_MASK, 0xFFU);
    DACDelay;
    outb(pATI->CPIO_DAC_WRITE, 0x00U);
    DACDelay;
    for (Index = 0;  Index < NumberOf(pATIHW->lut);  Index++)
    {
        outb(pATI->CPIO_DAC_DATA, pATIHW->lut[Index]);
        DACDelay;
    }

    outb(pATI->CPIO_DAC_MASK, pATIHW->dac_mask);
    DACDelay;
    outb(pATI->CPIO_DAC_READ, pATIHW->dac_read);
    DACDelay;
    outb(pATI->CPIO_DAC_WRITE, pATIHW->dac_write);
    DACDelay;

#endif /* AVOID_CPIO */

}

/*
 * ATILoadPalette --
 *
 * This function updates the RAMDAC's LUT and the in-memory copy of it in
 * NewHW.
 */
void
ATILoadPalette
(
    ScrnInfoPtr pScreenInfo,
    int         nColours,
    int         *Indices,
    LOCO        *Colours,
    VisualPtr   pVisual
)
{
    ATIPtr pATI = ATIPTR(pScreenInfo);
    CARD8  *LUTEntry;
    int    i, j, Index;

    if (((pVisual->class | DynamicClass) == DirectColor) &&
        ((1 << pVisual->nplanes) > (SizeOf(pATI->NewHW.lut) / 3)))
    {
        int reds = pVisual->redMask >> pVisual->offsetRed;
        int greens = pVisual->greenMask >> pVisual->offsetGreen;
        int blues = pVisual->blueMask >> pVisual->offsetBlue;

        int redShift = 8 - pATI->weight.red;
        int greenShift = 8 - pATI->weight.green;
        int blueShift = 8 - pATI->weight.blue;

        int redMult = 3 << redShift;
        int greenMult = 3 << greenShift;
        int blueMult = 3 << blueShift;

        int minShift;

        CARD8 fChanged[SizeOf(pATI->NewHW.lut) / 3];

        (void)memset(fChanged, SizeOf(fChanged), 0);

        minShift = redShift;
        if (minShift > greenShift)
            minShift = greenShift;
        if (minShift > blueShift)
            minShift = blueShift;

        for (i = 0;  i < nColours;  i++)
        {
            if((Index = Indices[i]) < 0)
                continue;

            if (Index <= reds)
            {
                j = Index * redMult;
                pATI->NewHW.lut[j + 0] = Colours[Index].red;
                fChanged[j / 3] = TRUE;
            }
            if (Index <= greens)
            {
                j = Index * greenMult;
                pATI->NewHW.lut[j + 1] = Colours[Index].green;
                fChanged[j / 3] = TRUE;
            }
            if (Index <= blues)
            {
                j = Index * blueMult;
                pATI->NewHW.lut[j + 2] = Colours[Index].blue;
                fChanged[j / 3] = TRUE;
            }
        }

        if (pScreenInfo->vtSema || pATI->currentMode)
        {
            /* Rewrite LUT entries that could have been changed */
            i = 1 << minShift;
            LUTEntry = pATI->NewHW.lut;

            for (Index = 0;
                 Index < (SizeOf(pATI->NewHW.lut) / 3);
                 Index += i, LUTEntry += i * 3)
            {
                if (!fChanged[Index])
                    continue;

#ifdef AVOID_CPIO

                out8(M64_DAC_WRITE, Index);
                DACDelay;
                out8(M64_DAC_DATA, LUTEntry[0]);
                DACDelay;
                out8(M64_DAC_DATA, LUTEntry[1]);
                DACDelay;
                out8(M64_DAC_DATA, LUTEntry[2]);
                DACDelay;

#else /* AVOID_CPIO */

                outb(pATI->CPIO_DAC_WRITE, Index);
                DACDelay;
                outb(pATI->CPIO_DAC_DATA, LUTEntry[0]);
                DACDelay;
                outb(pATI->CPIO_DAC_DATA, LUTEntry[1]);
                DACDelay;
                outb(pATI->CPIO_DAC_DATA, LUTEntry[2]);
                DACDelay;

#endif /* AVOID_CPIO */

            }
        }
    }
    else
    {
        for (i = 0;  i < nColours;  i++)
        {
            Index = Indices[i];
            if ((Index < 0) || (Index >= (SizeOf(pATI->NewHW.lut) / 3)))
                continue;

            LUTEntry = &pATI->NewHW.lut[Index * 3];
            LUTEntry[0] = Colours[Index].red;
            LUTEntry[1] = Colours[Index].green;
            LUTEntry[2] = Colours[Index].blue;

            if (pScreenInfo->vtSema || pATI->currentMode)
            {

#ifdef AVOID_CPIO

                out8(M64_DAC_WRITE, Index);
                DACDelay;
                out8(M64_DAC_DATA, LUTEntry[0]);
                DACDelay;
                out8(M64_DAC_DATA, LUTEntry[1]);
                DACDelay;
                out8(M64_DAC_DATA, LUTEntry[2]);
                DACDelay;

#else /* AVOID_CPIO */

                outb(pATI->CPIO_DAC_WRITE, Index);
                DACDelay;
                outb(pATI->CPIO_DAC_DATA, LUTEntry[0]);
                DACDelay;
                outb(pATI->CPIO_DAC_DATA, LUTEntry[1]);
                DACDelay;
                outb(pATI->CPIO_DAC_DATA, LUTEntry[2]);
                DACDelay;

#endif /* AVOID_CPIO */

            }
        }
    }
}

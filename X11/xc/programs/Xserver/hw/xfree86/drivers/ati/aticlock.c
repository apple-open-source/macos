/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/ati/aticlock.c,v 1.20 2003/01/01 19:16:31 tsi Exp $ */
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

/*
 * Adapters prior to V5 use 4 crystals.  Adapters V5 and later use a clock
 * generator chip.  V3 and V4 adapters differ when it comes to choosing clock
 * frequencies.
 *
 * VGA Wonder V3/V4 Adapter Clock Frequencies
 * R E G I S T E R S
 * 1CE(*)    3C2     3C2    Frequency
 * B2h/BEh
 * Bit 6/4  Bit 3   Bit 2   (MHz)
 * ------- ------- -------  -------
 *    0       0       0     50.000
 *    0       0       1     56.644
 *    0       1       0     Spare 1
 *    0       1       1     44.900
 *    1       0       0     44.900
 *    1       0       1     50.000
 *    1       1       0     Spare 2
 *    1       1       1     36.000
 *
 * (*):  V3 uses index B2h, bit 6;  V4 uses index BEh, bit 4
 *
 * V5, PLUS, XL and XL24 usually have an ATI 18810 clock generator chip, but
 * some have an ATI 18811-0, and it's quite conceivable that some exist with
 * ATI 18811-1's or ATI 18811-2's.  Mach32 adapters are known to use any one of
 * these clock generators.  Mach32 adapters also use a different dot clock
 * ordering.  ATI says there is no reliable way for the driver to determine
 * which clock generator is on the adapter, but this driver will do its best to
 * do so anyway.
 *
 * VGA Wonder V5/PLUS/XL/XL24 Clock Frequencies
 * R E G I S T E R S
 *   1CE     1CE     3C2     3C2    Frequency
 *   B9h     BEh                     (MHz)   18811-0  18811-1
 *  Bit 1   Bit 4   Bit 3   Bit 2    18810   18812-0  18811-2  (*5)
 * ------- ------- ------- -------  -------  -------  -------  -------
 *    0       0       0       0      30.240   30.240  135.000   75.000
 *    0       0       0       1      32.000   32.000   32.000   77.500
 *    0       0       1       0      37.500  110.000  110.000   80.000
 *    0       0       1       1      39.000   80.000   80.000   90.000
 *    0       1       0       0      42.954   42.954  100.000   25.175
 *    0       1       0       1      48.771   48.771  126.000   28.322
 *    0       1       1       0        (*1)   92.400   92.400   31.500
 *    0       1       1       1      36.000   36.000   36.000   36.000
 *    1       0       0       0      40.000   39.910   39.910  100.000
 *    1       0       0       1        (*4)   44.900   44.900  110.000
 *    1       0       1       0      75.000   75.000   75.000  126.000
 *    1       0       1       1      65.000   65.000   65.000  135.000
 *    1       1       0       0      50.350   50.350   50.350   40.000
 *    1       1       0       1      56.640   56.640   56.640   44.900
 *    1       1       1       0        (*2)     (*3)     (*3)   50.000
 *    1       1       1       1      44.900   44.900   44.900   65.000
 *
 * (*1) External 0 (supposedly 16.657 Mhz)
 * (*2) External 1 (supposedly 28.322 MHz)
 * (*3) This setting doesn't seem to generate anything
 * (*4) This setting is documented to be 56.644 MHz, but something close to 82
 *      MHz has also been encountered.
 * (*5) This setting is for Dell OmniPlex 590 systems, with a 68800AX on the
 *      motherboard, along with an AT&T21C498 DAC (which is reported as an
 *      STG1700) and ICS2494AM clock generator (a.k.a. ATI 18811-?).
 *
 * Mach32 Clock Frequencies
 * R E G I S T E R S
 *   1CE     1CE     3C2     3C2    Frequency
 *   B9h     BEh                     (MHz)   18811-0  18811-1
 *  Bit 1   Bit 4   Bit 3   Bit 2    18810   18812-0  18811-2  (*5)
 * ------- ------- ------- -------  -------  -------  -------  -------
 *    0       0       0       0      42.954   42.954  100.000   25.175
 *    0       0       0       1      48.771   48.771  126.000   28.322
 *    0       0       1       0        (*1)   92.400   92.400   31.500
 *    0       0       1       1      36.000   36.000   36.000   36.000
 *    0       1       0       0      30.240   30.240  135.000   75.000
 *    0       1       0       1      32.000   32.000   32.000   77.500
 *    0       1       1       0      37.500  110.000  110.000   80.000
 *    0       1       1       1      39.000   80.000   80.000   90.000
 *    1       0       0       0      50.350   50.350   50.350   40.000
 *    1       0       0       1      56.640   56.640   56.640   44.900
 *    1       0       1       0        (*2)     (*3)     (*3)   50.000
 *    1       0       1       1      44.900   44.900   44.900   65.000
 *    1       1       0       0      40.000   39.910   39.910  100.000
 *    1       1       0       1        (*4)   44.900   44.900  110.000
 *    1       1       1       0      75.000   75.000   75.000  126.000
 *    1       1       1       1      65.000   65.000   65.000  135.000
 *
 * (*1) External 0 (supposedly 16.657 Mhz)
 * (*2) External 1 (supposedly 28.322 MHz)
 * (*3) This setting doesn't seem to generate anything
 * (*4) This setting is documented to be 56.644 MHz, but something close to 82
 *      MHz has also been encountered.
 * (*5) This setting is for Dell OmniPlex 590 systems, with a 68800AX on the
 *      motherboard, along with an AT&T21C498 DAC (which is reported as an
 *      STG1700) and ICS2494AM clock generator (a.k.a. ATI 18811-?).
 *
 * Note that, to reduce confusion, this driver masks out the different clock
 * ordering.
 *
 * For all adapters, these frequencies can be divided by 1 or 2.  For all
 * adapters, except Mach32's and Mach64's, frequencies can also be divided by 3
 * or 4.
 *
 *      Register 1CE, index B8h
 *       Bit 7    Bit 6
 *      -------  -------
 *         0        0           Divide by 1
 *         0        1           Divide by 2
 *         1        0           Divide by 3
 *         1        1           Divide by 4
 *
 * With respect to clocks, Mach64's are entirely different animals.
 *
 * The oldest Mach64's use one of the non-programmable clock generators
 * described above.  In this case, the driver will handle clocks in much the
 * same way as it would for a Mach32.
 *
 * All other Mach64 adapters use a programmable clock generator.  BIOS
 * initialisation programmes an initial set of frequencies.  Two of these are
 * reserved to allow for the setting of modes that do not use a frequency from
 * this initial set.  One of these reserved slots is used by the BIOS mode set
 * routine, the other by the particular accelerated driver used (MS-Windows,
 * AutoCAD, etc.).  The slots reserved in this way are dependent on the
 * particular clock generator used by the adapter.
 *
 * If the driver does not support the adapter's clock generator, it will try to
 * match the (probed or specified) clocks to one of the following sets.
 *
 * Mach64 Clock Frequencies for unsupported programmable clock generators
 * R E G I S T E R S
 *   1CE     1CE     3C2     3C2    Frequency
 *   B9h     BEh                     (MHz)
 *  Bit 1   Bit 4   Bit 3   Bit 2    Set 1    Set 2    Set 3
 * ------- ------- ------- -------  -------  -------  -------
 *    0       0       0       0      50.350   25.180   25.180
 *    0       0       0       1      56.640   28.320   28.320
 *    0       0       1       0      63.000   31.500    0.000
 *    0       0       1       1      72.000   36.000    0.000
 *    0       1       0       0       0.000    0.000    0.000
 *    0       1       0       1     110.000  110.000    0.000
 *    0       1       1       0     126.000  126.000    0.000
 *    0       1       1       1     135.000  135.000    0.000
 *    1       0       0       0      40.000   40.000    0.000
 *    1       0       0       1      44.900   44.900    0.000
 *    1       0       1       0      49.500   49.500    0.000
 *    1       0       1       1      50.000   50.000    0.000
 *    1       1       0       0       0.000    0.000    0.000
 *    1       1       0       1      80.000   80.000    0.000
 *    1       1       1       0      75.000   75.000    0.000
 *    1       1       1       1      65.000   65.000    0.000
 *
 * The driver will never select a setting of 0.000 MHz.  The above comments on
 * clock ordering and clock divider apply here also.
 *
 * For all supported programmable clock generators, the driver will ignore any
 * XF86Config clock line and programme, as needed, the clock number reserved by
 * the BIOS for accelerated drivers.  The driver's mode initialisation routine
 * finds integers N, M and D such that
 *
 *             N
 *      R * -------  MHz
 *           M * D
 *
 * best approximates the mode's clock frequency, where R is the crystal-
 * generated reference frequency (usually 14.318 MHz).  D is a power of 2
 * except for those integrated controllers that also offer odd dividers.
 * Different clock generators have different restrictions on the value N, M and
 * D can assume.  The driver contains an internal table to record these
 * restrictions (among other things).  The resulting values of N, M and D are
 * then encoded in a generator-specific way and used to programme the clock.
 * The Mach64's clock divider is not used in this case.
 */

#include "ati.h"
#include "atiadapter.h"
#include "atichip.h"
#include "atidac.h"
#include "atidsp.h"
#include "atimach64io.h"
#include "atimode.h"
#include "atiwonderio.h"

/*
 * Definitions related to non-programmable clock generators.
 */
const char *ATIClockNames[] =
{
    "unknown",
    "IBM VGA compatible",
    "crystals",
    "ATI 18810 or similar",
    "ATI 18811-0 or similar",
    "ATI 18811-1 or similar",
    "ICS 2494-AM or similar",
    "Programmable (BIOS setting 1)",
    "Programmable (BIOS setting 2)",
    "Programmable (BIOS setting 3)"
};

/*
 * Definitions related to programmable clock generators.
 */
static CARD16 ATIPostDividers[] = {1, 2, 4, 8, 16, 32, 64, 128},
              ATI264xTPostDividers[] = {1, 2, 4, 8, 3, 0, 6, 12};
ClockRec ATIClockDescriptors[] =
{
    {
          0,   0,   0, 1, 1,
          1,   1,   0,
          0, NULL,
        "Non-programmable"
    },
    {
        257, 512, 257, 1, 1,
         46,  46,   0,
          4, ATIPostDividers,
        "ATI 18818 or ICS 2595 or similar"
    },
    {
          2, 129,   2, 1, 1,
          8,  14,   2,
          8, ATIPostDividers,
        "SGS-Thompson 1703 or similar"
    },
    {
         16, 263,   8, 8, 9,
          4,  12,   2,
          4, ATIPostDividers,
        "Chrontel 8398 or similar"
    },
    {
          2, 255,   0, 1, 1,
         45,  45,   0,
          4, ATI264xTPostDividers,
        "Internal"
    },
    {
          2, 257,   2, 1, 1,
          2,  32,   2,
          4, ATIPostDividers,
        "AT&T 20C408 or similar"
    },
    {
         65, 128,  65, 1, 1,
          2,  14,   0,
          4, ATIPostDividers,
        "IBM RGB 514 or similar"
    }
};

/*
 * XF86Config clocks line that start with the following will either be rejected
 * for ATI adapters, or accepted for non-ATI adapters.
 */
static const int
ATIVGAClocks[] =
{
     25175,  28322,
        -1
};

/*
 * The driver will attempt to match fixed clocks to one of the following
 * specifications.
 */
static const int
ATICrystalFrequencies[] =
{
     50000,  56644,      0,  44900,  44900,  50000,      0,  36000,
        -1
},
ATI18810Frequencies[] =
{
     30240,  32000,  37500,  39000,  42954,  48771,      0,  36000,
     40000,      0,  75000,  65000,  50350,  56640,      0,  44900
},
ATI188110Frequencies[] =
{
     30240,  32000, 110000,  80000,  42954,  48771,  92400,  36000,
     39910,  44900,  75000,  65000,  50350,  56640,      0,  44900
},
ATI188111Frequencies[] =
{
    135000,  32000, 110000,  80000, 100000, 126000,  92400,  36000,
     39910,  44900,  75000,  65000,  50350,  56640,      0,  44900
},
ATI2494AMFrequencies[] =
{
     75000,  77500,  80000,  90000,  25175,  28322,  31500,  36000,
    100000, 110000, 126000, 135000,  40000,  44900,  50000,  65000
},
ATIMach64AFrequencies[] =
{
         0, 110000, 126000, 135000,  50350,  56640,  63000,  72000,
         0,  80000,  75000,  65000,  40000,  44900,  49500,  50000
},
ATIMach64BFrequencies[] =
{
         0, 110000, 126000, 135000,  25180,  28320,  31500,  36000,
         0,  80000,  75000,  65000,  40000,  44900,  49500,  50000
},
ATIMach64CFrequencies[] =
{
         0,      0,      0,      0,  25180,  28320,      0,      0,
         0,      0,      0,      0,      0,      0,      0,      0
},
*SpecificationClockLine[] =
{
    NULL,
    ATIVGAClocks,
    ATICrystalFrequencies,
    ATI18810Frequencies,
    ATI188110Frequencies,
    ATI188111Frequencies,
    ATI2494AMFrequencies,
    ATIMach64AFrequencies,
    ATIMach64BFrequencies,
    ATIMach64CFrequencies,
    NULL
};

/*
 * The driver will reject XF86Config clocks lines that start with, or are an
 * initial subset of, one of the following.
 */
static const int
ATIPre_2_1_1_Clocks_A[] =       /* Based on 18810 */
{
     18000,  22450,  25175,  28320,  36000,  44900,  50350,  56640,
     30240,  32000,  37500,  39000,  40000,      0,  75000,  65000,
        -1
},
ATIPre_2_1_1_Clocks_B[] =       /* Based on 18811-0 */
{
     18000,  22450,  25175,  28320,  36000,  44900,  50350,  56640,
     30240,  32000, 110000,  80000,  39910,  44900,  75000,  65000,
        -1
},
ATIPre_2_1_1_Clocks_C[] =       /* Based on 18811-1 (or -2) */
{
     18000,  22450,  25175,  28320,  36000,  44900,  50350,  56640,
    135000,  32000, 110000,  80000,  39910,  44900,  75000,  65000,
        -1
},
ATIPre_2_1_1_Clocks_D[] =       /* Based on ICS 2494AM */
{
     18000,  32500,  20000,  22450,  36000,  65000,  40000,  44900,
     75000,  77500,  80000,  90000, 100000, 110000, 126000, 135000,
        -1
},
ATIPre_2_1_1_Clocks_E[] =       /* Based on programmable setting 1 */
{
     36000,  25000,  20000,  22450,  72000,  50000,  40000,  44900,
         0, 110000, 126000, 135000,      0,  80000,  75000,  65000,
        -1
},
ATIPre_2_1_1_Clocks_F[] =       /* Based on programmable setting 2 */
{
     18000,  25000,  20000,  22450,  36000,  50000,  40000,  44900,
         0, 110000, 126000, 135000,      0,  80000,  75000,  65000,
        -1
},
*InvalidClockLine[] =
{
    NULL,
    ATIVGAClocks,
    ATIPre_2_1_1_Clocks_A,
    ATIPre_2_1_1_Clocks_B,
    ATIPre_2_1_1_Clocks_C,
    ATIPre_2_1_1_Clocks_D,
    ATIPre_2_1_1_Clocks_E,
    ATIPre_2_1_1_Clocks_F,
    NULL
};

/*
 * Clock maps.
 */
static const CARD8 ClockMaps[][4] =
{
    /* Null map */
    { 0, 1, 2, 3},
    /* VGA Wonder map <-> Mach{8,32,64} */
    { 1, 0, 3, 2},
    /* VGA Wonder map <-> Accelerator */
    { 0, 2, 1, 3},
    /* VGA -> Accelerator map */
    { 2, 0, 3, 1},
    /* Accelerator -> VGA map */
    { 1, 3, 0, 2}
};
#define ATIVGAWonderClockMap         ClockMaps[0]
#define ATIVGAWonderClockUnmap       ATIVGAWonderClockMap
#define ATIMachVGAClockMap           ClockMaps[1]
#define ATIMachVGAClockUnmap         ATIMachVGAClockMap
#define ATIVGAProgrammableClockMap   ClockMaps[2]
#define ATIVGAProgrammableClockUnmap ATIVGAProgrammableClockMap
#define ATIAcceleratorClockMap       ClockMaps[3]
#define ATIAcceleratorClockUnmap     ClockMaps[4]
#define ATIProgrammableClockMap      ClockMaps[0]
#define ATIProgrammableClockUnmap    ATIProgrammableClockMap
#define MapClockIndex(_ClockMap, _Index) \
    (SetBits((_ClockMap)[GetBits(_Index, 0x0CU)], 0x0CU) | \
     ((_Index) & ~0x0CU))

/*
 * ATIMatchClockLine --
 *
 * This function tries to match the XF86Config clocks to one of an array of
 * clock lines.  It returns a clock line number or 0.
 */
static int
ATIMatchClockLine
(
    ScrnInfoPtr              pScreenInfo,
    ATIPtr                   pATI,
    const                int **ClockLine,
    const unsigned short int NumberOfClocks,
    const                int CalibrationClockNumber,
    const                int ClockMap
)
{
    int ClockChip = 0, ClockChipIndex = 0;
    int NumberOfMatchingClocks = 0;
    int MinimumGap = CLOCK_TOLERANCE + 1;

    /* For ATI adapters, reject generic VGA clocks */

#ifndef AVOID_CPIO

    if (pATI->Adapter != ATI_ADAPTER_VGA)

#endif /* AVOID_CPIO */

    {
        if (ClockLine == SpecificationClockLine)
            ClockChipIndex++;
    }

    /* If checking for XF86Config clock order, skip crystals */
    if (ClockMap)
        ClockChipIndex++;

    for (;  ClockLine[++ClockChipIndex];  )
    {
        int MaximumGap = 0, ClockCount = 0, ClockIndex = 0;

#ifndef AVOID_CPIO

        /* Only Mach64's and later can have programmable clocks */
        if ((ClockChipIndex >= ATI_CLOCK_MACH64A) &&
            (pATI->Adapter < ATI_ADAPTER_MACH64))
            break;

#endif /* AVOID_CPIO */

        for (;  ClockIndex < NumberOfClocks;  ClockIndex++)
        {
            int Gap, XF86ConfigClock, SpecificationClock;

            SpecificationClock = ClockLine[ClockChipIndex]
                [MapClockIndex(ClockMaps[ClockMap], ClockIndex)];
            if (SpecificationClock < 0)
                break;
            if (!SpecificationClock)
                continue;

            XF86ConfigClock = pScreenInfo->clock[ClockIndex];
            if (!XF86ConfigClock)
                continue;

            Gap = abs(XF86ConfigClock - SpecificationClock);
            if (Gap >= MinimumGap)
                goto SkipThisClockGenerator;
            if (!Gap)
            {
                if (ClockIndex == CalibrationClockNumber)
                    continue;
            }
            else if (Gap > MaximumGap)
                MaximumGap = Gap;
            ClockCount++;
        }

        if (ClockCount <= NumberOfMatchingClocks)
            continue;
        NumberOfMatchingClocks = ClockCount;
        ClockChip = ClockChipIndex;
        if (!(MinimumGap = MaximumGap))
            break;

SkipThisClockGenerator:;

#ifndef AVOID_CPIO

        /* For non-ATI adapters, only normalise standard VGA clocks */
        if (pATI->Adapter == ATI_ADAPTER_VGA)
            break;

#endif /* AVOID_CPIO */

    }

    return ClockChip;
}

/*
 * ATIClockPreInit --
 *
 * This function is called by ATIPreInit() and handles the XF86Config clocks
 * line (or lack thereof).
 */
void
ATIClockPreInit
(
    ScrnInfoPtr   pScreenInfo,
    ATIPtr        pATI,
    GDevPtr       pGDev,
    ClockRangePtr pRange
)
{
    double ScaleFactor;
    unsigned short int NumberOfUndividedClocks;
    unsigned short int NumberOfDividers, NumberOfClocks;
    int CalibrationClockNumber, CalibrationClockValue;
    int ClockIndex, SpecificationClock, ClockMap = 0, Index;
    CARD8 CanDisableInterrupts;

#ifndef AVOID_CPIO

    CARD8 genmo;

#endif /* AVOID_CPIO */

    /*
     * Decide what to do about the XF86Config clocks for programmable clock
     * generators.
     */
    if (pATI->ProgrammableClock != ATI_CLOCK_FIXED)
    {
        /* Check for those that are not (yet) handled */
        if ((pATI->ProgrammableClock == ATI_CLOCK_UNKNOWN) ||
            (pATI->ProgrammableClock > NumberOf(ATIClockDescriptors)))
            xf86DrvMsgVerb(pScreenInfo->scrnIndex, X_WARNING, 0,
                "Unknown programmable clock generator type (0x%02X)"
                " detected.\n", pATI->ProgrammableClock);
        else if (pATI->ClockDescriptor.MaxN <= 0)
            xf86DrvMsgVerb(pScreenInfo->scrnIndex, X_WARNING, 0,
                "Unsupported programmable clock generator detected:  %s.\n",
                pATI->ClockDescriptor.ClockName);
        else
        {
            /*
             * Recognise supported clock generators.  This involves telling the
             * rest of the server about it and (re-)initializing the XF86Config
             * clocks line.
             */
            pRange->clockIndex = -1;
            pScreenInfo->progClock = TRUE;

            /* Set internal clock ordering */

#ifndef AVOID_CPIO

            if (pATI->NewHW.crtc == ATI_CRTC_VGA)
            {
                pATI->NewHW.ClockMap = ATIVGAProgrammableClockMap;
                pATI->NewHW.ClockUnmap = ATIVGAProgrammableClockUnmap;
            }
            else

#endif /* AVOID_CPIO */

            {
                pATI->NewHW.ClockMap = ATIProgrammableClockMap;
                pATI->NewHW.ClockUnmap = ATIProgrammableClockUnmap;
            }

            xf86DrvMsg(pScreenInfo->scrnIndex, X_PROBED,
                "%s programmable clock generator detected.\n",
                pATI->ClockDescriptor.ClockName);
            if (pATI->ReferenceDenominator == 1)
                xf86DrvMsg(pScreenInfo->scrnIndex, X_PROBED,
                    "Reference clock %.3f MHz.\n",
                    (double)pATI->ReferenceNumerator / 1000.0);
            else
                xf86DrvMsg(pScreenInfo->scrnIndex, X_PROBED,
                    "Reference clock %.6g/%d (%.3f) MHz.\n",
                    (double)pATI->ReferenceNumerator / 1000.0,
                    pATI->ReferenceDenominator,
                    (double)pATI->ReferenceNumerator /
                        ((double)pATI->ReferenceDenominator * 1000.0));

            /* Clobber XF86Config clocks line */
            if (pGDev->numclocks)
                xf86DrvMsg(pScreenInfo->scrnIndex, X_NOTICE,
                    "XF86Config clocks specification ignored.\n");

            if (pATI->ProgrammableClock == ATI_CLOCK_CH8398)
            {   /* First two are fixed */
                pScreenInfo->numClocks = 2;
                pScreenInfo->clock[0] = 25175;
                pScreenInfo->clock[1] = 28322;
            }
            else if (pATI->ProgrammableClock == ATI_CLOCK_INTERNAL)
            /*
             * The integrated PLL generates clocks as if the reference
             * frequency were doubled.
             */
                pATI->ReferenceNumerator <<= 1;

            return;     /* ... to ATIPreInit() */
        }
    }

#ifndef AVOID_CPIO

    /* Set default clock maps */
    pATI->NewHW.ClockMap = ATIVGAWonderClockMap;
    pATI->NewHW.ClockUnmap = ATIVGAWonderClockUnmap;

#endif /* AVOID_CPIO */

    /*
     * Determine the number of clock values the adapter should be able to
     * generate and the dot clock to use for probe calibration.
     */
ProbeClocks:

#ifndef AVOID_CPIO

    if (pATI->Adapter == ATI_ADAPTER_VGA)
    {
        NumberOfDividers = 1;
        NumberOfUndividedClocks = 4;
        CalibrationClockNumber = 1;
        CalibrationClockValue = 28322;
    }
    else

#endif /* AVOID_CPIO */

    {

#ifndef AVOID_CPIO

        NumberOfDividers = 4;
        if ((pATI->Chip <= ATI_CHIP_18800) ||
            (pATI->Adapter == ATI_ADAPTER_V4))
        {
            NumberOfUndividedClocks = 8;
            /* Actually, any undivided clock will do */
            CalibrationClockNumber = 1;
            CalibrationClockValue = 56644;
        }
        else

#endif /* AVOID_CPIO */

        {
            NumberOfUndividedClocks = 16;

#ifndef AVOID_CPIO

            CalibrationClockNumber = 7;
            CalibrationClockValue = 36000;
            if (pATI->Chip >= ATI_CHIP_68800)

#endif /* AVOID_CPIO */

            {
                NumberOfDividers = 2;
                if (pATI->Chip >= ATI_CHIP_264CT)
                {
                    NumberOfDividers = 1;
                    NumberOfUndividedClocks = 4;
                    CalibrationClockNumber = 1;
                    CalibrationClockValue = 28322;
                }
                else

#ifndef AVOID_CPIO

                if (pATI->Adapter >= ATI_ADAPTER_MACH64)

#endif /* AVOID_CPIO */

                {
                    CalibrationClockNumber = 10 /* or 11 */;
                    CalibrationClockValue = 75000 /* or 65000 */;
                }

                /*
                 * When selecting clocks, all ATI accelerators use a different
                 * clock ordering.
                 */

#ifndef AVOID_CPIO

                if (pATI->NewHW.crtc == ATI_CRTC_VGA)
                {
                    pATI->NewHW.ClockMap = ATIMachVGAClockMap;
                    pATI->NewHW.ClockUnmap = ATIMachVGAClockUnmap;
                }
                else

#endif /* AVOID_CPIO */

                {
                    pATI->NewHW.ClockMap = ATIAcceleratorClockMap;
                    pATI->NewHW.ClockUnmap = ATIAcceleratorClockUnmap;
                }
            }
        }
    }

    pATI->OldHW.ClockMap = pATI->NewHW.ClockMap;
    pATI->OldHW.ClockUnmap = pATI->NewHW.ClockUnmap;

    NumberOfClocks = NumberOfUndividedClocks * NumberOfDividers;

    /*
     * Respect any XF86Config clocks line.  Well, that's the theory, anyway.
     * In practice, however, the regular use of probed values is widespread, at
     * times causing otherwise inexplicable results.  So, attempt to normalise
     * the clocks to known (i.e. specification) values.
     */
    if (!pGDev->numclocks || pATI->OptionProbeClocks ||
        xf86ServerIsOnlyProbing())
    {
        if (pATI->ProgrammableClock != ATI_CLOCK_FIXED)
        {
            /*
             * For unsupported programmable clock generators, pick the highest
             * frequency set by BIOS initialisation for clock calibration.
             */
            CalibrationClockNumber = CalibrationClockValue = 0;
            for (ClockIndex = 0;
                 ClockIndex < NumberOfUndividedClocks;
                 ClockIndex++)
                if (CalibrationClockValue < pATI->BIOSClocks[ClockIndex])
                {
                    CalibrationClockNumber = ClockIndex;
                    CalibrationClockValue = pATI->BIOSClocks[ClockIndex];
                }
            CalibrationClockNumber =
                MapClockIndex(pATI->NewHW.ClockUnmap, CalibrationClockNumber);
            CalibrationClockValue *= 10;
        }

#ifndef AVOID_CPIO

        if (pATI->VGAAdapter != ATI_ADAPTER_NONE)
        {
            /*
             * The current video state needs to be saved before the clock
             * probe, and restored after.  Video memory corruption and other
             * effects occur because, at this early stage, the clock probe
             * cannot reliably be prevented from enabling frequencies that are
             * greater than what the adapter can handle.
             */
            ATIModeSave(pScreenInfo, pATI, &pATI->OldHW);

            /* Ensure clock select pins are not OR'ed with anything */
            if (pATI->CPIO_VGAWonder && (pATI->OldHW.crtc == ATI_CRTC_VGA))
                ATIModifyExtReg(pATI, 0xB5U, pATI->OldHW.b5, 0x7FU, 0x00U);
        }

#endif /* AVOID_CPIO */

        /*
         * Probe the adapter for clock values.  The following is essentially
         * the common layer's xf86GetClocks() reworked to fit.  One difference
         * is the ability to monitor a VSync bit in MMIO space.
         */
        CanDisableInterrupts = TRUE;    /* An assumption verified below */

        for (ClockIndex = 0;  ClockIndex < NumberOfClocks;  ClockIndex++)
        {
            pScreenInfo->clock[ClockIndex] = 0;

            /* Remap clock number */
            Index = MapClockIndex(pATI->OldHW.ClockMap, ClockIndex);

            /* Select the clock */
            switch (pATI->OldHW.crtc)
            {

#ifndef AVOID_CPIO

                case ATI_CRTC_VGA:
                    /* Get generic two low-order bits */
                    genmo = (inb(R_GENMO) & 0xF3U) | ((Index << 2) & 0x0CU);

                    if (pATI->CPIO_VGAWonder)
                    {
                        /*
                         * On adapters with crystals, switching to one of the
                         * spare assignments doesn't do anything (i.e. the
                         * previous setting remains in effect).  So, disable
                         * their selection.
                         */
                        if (((Index & 0x03U) == 0x02U) &&
                            ((pATI->Chip <= ATI_CHIP_18800) ||
                             (pATI->Adapter == ATI_ADAPTER_V4)))
                            continue;

                        /* Start sequencer reset */
                        PutReg(SEQX, 0x00U, 0x00U);

                        /* Set high-order bits */
                        if (pATI->Chip <= ATI_CHIP_18800)
                            ATIModifyExtReg(pATI, 0xB2U, -1, 0xBFU,
                                Index << 4);
                        else
                        {
                            ATIModifyExtReg(pATI, 0xBEU, -1, 0xEFU,
                                Index << 2);
                            if (pATI->Adapter != ATI_ADAPTER_V4)
                            {
                                Index >>= 1;
                                ATIModifyExtReg(pATI, 0xB9U, -1, 0xFDU,
                                    Index >> 1);
                            }
                        }

                        /* Set clock divider bits */
                        ATIModifyExtReg(pATI, 0xB8U, -1, 0x00U,
                            (Index << 3) & 0xC0U);
                    }
                    else
                    {
                        /*
                         * Reject clocks that cannot be selected.
                         */
                        if (Index & ~0x03U)
                            continue;

                        /* Start sequencer reset */
                        PutReg(SEQX, 0x00U, 0x00U);
                    }

                    /* Must set miscellaneous output register last */
                    outb(GENMO, genmo);

                    /* End sequencer reset */
                    PutReg(SEQX, 0x00U, 0x03U);

                    break;

#endif /* AVOID_CPIO */

                case ATI_CRTC_MACH64:
                    out8(CLOCK_CNTL, CLOCK_STROBE |
                        SetBits(Index, CLOCK_SELECT | CLOCK_DIVIDER));
                    break;

                default:
                    continue;
            }

            usleep(50000);      /* Let clock stabilise */

            xf86SetPriority(TRUE);

            /* Try to disable interrupts */
            if (CanDisableInterrupts && !xf86DisableInterrupts())
            {
                xf86DrvMsg(pScreenInfo->scrnIndex, X_WARNING,
                    "Unable to disable interrupts;  Clock probe will not be as"
                    " accurate.\n");
                CanDisableInterrupts = FALSE;
            }

            /*
             * Generate a count while monitoring the vertical sync or blanking
             * pulse.  This is dependent on the CRTC used by the mode on server
             * entry.
             */
            switch (pATI->OldHW.crtc)
            {

#ifndef AVOID_CPIO

                case ATI_CRTC_VGA:
                    /* Verify vertical sync pulses are in fact occurring */
                    Index = 1 << 19;
                    while (!(inb(GENS1(pATI->CPIO_VGABase)) & 0x08U))
                        if (Index-- <= 0)
                            goto EnableInterrupts;
                    Index = 1 << 19;
                    while (inb(GENS1(pATI->CPIO_VGABase)) & 0x08U)
                        if (Index-- <= 0)
                            goto EnableInterrupts;
                    Index = 1 << 19;
                    while (!(inb(GENS1(pATI->CPIO_VGABase)) & 0x08U))
                        if (Index-- <= 0)
                            goto EnableInterrupts;

                    /* Generate the count */
                    for (Index = 0;  Index < 8;  Index++)
                    {
                        while (inb(GENS1(pATI->CPIO_VGABase)) & 0x08U)
                            pScreenInfo->clock[ClockIndex]++;
                        while (!(inb(GENS1(pATI->CPIO_VGABase)) & 0x08U))
                            pScreenInfo->clock[ClockIndex]++;
                    }
                    break;

#endif /* AVOID_CPIO */

                case ATI_CRTC_MACH64:
                    /* Verify vertical blanking pulses are in fact occurring */
                    Index = 1 << 19;
                    while (!(inr(CRTC_INT_CNTL) & CRTC_VBLANK))
                        if (Index-- <= 0)
                            goto EnableInterrupts;
                    Index = 1 << 19;
                    while (inr(CRTC_INT_CNTL) & CRTC_VBLANK)
                        if (Index-- <= 0)
                            goto EnableInterrupts;
                    Index = 1 << 19;
                    while (!(inr(CRTC_INT_CNTL) & CRTC_VBLANK))
                        if (Index-- <= 0)
                            goto EnableInterrupts;

                    /* Generate the count */
                    for (Index = 0;  Index < 4;  Index++)
                    {
                        while (inr(CRTC_INT_CNTL) & CRTC_VBLANK)
                            pScreenInfo->clock[ClockIndex]++;
                        while (!(inr(CRTC_INT_CNTL) & CRTC_VBLANK))
                            pScreenInfo->clock[ClockIndex]++;
                    }
                    break;

                default:
                    break;
            }

        EnableInterrupts:
            if (CanDisableInterrupts)
                xf86EnableInterrupts();

            xf86SetPriority(FALSE);
        }

        ScaleFactor = (double)CalibrationClockValue *
            (double)pScreenInfo->clock[CalibrationClockNumber];

        /* Scale the clocks from counts to kHz */
        for (ClockIndex = 0;  ClockIndex < NumberOfClocks;  ClockIndex++)
        {
            if (ClockIndex == CalibrationClockNumber)
                pScreenInfo->clock[ClockIndex] = CalibrationClockValue;
            else if (pScreenInfo->clock[ClockIndex])
                /* Round to the nearest 10 kHz */
                pScreenInfo->clock[ClockIndex] =
                    (int)(((ScaleFactor /
                            (double)pScreenInfo->clock[ClockIndex]) +
                           5) / 10) * 10;
        }

        pScreenInfo->numClocks = NumberOfClocks;

#ifndef AVOID_CPIO

        if (pATI->VGAAdapter != ATI_ADAPTER_NONE)
        {
            /* Restore video state */
            ATIModeSet(pScreenInfo, pATI, &pATI->OldHW);
            xfree(pATI->OldHW.frame_buffer);
            pATI->OldHW.frame_buffer = NULL;
        }

#endif /* AVOID_CPIO */

        /* Tell user clocks were probed, instead of supplied */
        pATI->OptionProbeClocks = TRUE;

        /* Attempt to match probed clocks to a known specification */
        pATI->Clock = ATIMatchClockLine(pScreenInfo, pATI,
            SpecificationClockLine, NumberOfUndividedClocks,
            CalibrationClockNumber, 0);

#ifndef AVOID_CPIO

        if ((pATI->Chip <= ATI_CHIP_18800) ||
            (pATI->Adapter == ATI_ADAPTER_V4))
        {
            /* V3 and V4 adapters don't have clock chips */
            if (pATI->Clock > ATI_CLOCK_CRYSTALS)
                pATI->Clock = ATI_CLOCK_NONE;
        }
        else

#endif /* AVOID_CPIO */

        {
            /* All others don't have crystals */
            if (pATI->Clock == ATI_CLOCK_CRYSTALS)
                pATI->Clock = ATI_CLOCK_NONE;
        }
    }
    else
    {
        /*
         * Allow for an initial subset of specification clocks.  Can't allow
         * for any more than that though...
         */
        if (NumberOfClocks > pGDev->numclocks)
        {
            NumberOfClocks = pGDev->numclocks;
            if (NumberOfUndividedClocks > NumberOfClocks)
                NumberOfUndividedClocks = NumberOfClocks;
        }

        /* Move XF86Config clocks into the ScrnInfoRec */
        for (ClockIndex = 0;  ClockIndex < NumberOfClocks;  ClockIndex++)
            pScreenInfo->clock[ClockIndex] = pGDev->clock[ClockIndex];
        pScreenInfo->numClocks = NumberOfClocks;

        /* Attempt to match clocks to a known specification */
        pATI->Clock = ATIMatchClockLine(pScreenInfo, pATI,
            SpecificationClockLine, NumberOfUndividedClocks, -1, 0);

#ifndef AVOID_CPIO

        if (pATI->Adapter != ATI_ADAPTER_VGA)

#endif /* AVOID_CPIO */

        {
            if (pATI->Clock == ATI_CLOCK_NONE)
            {
                /*
                 * Reject certain clock lines that are obviously wrong.  This
                 * includes the standard VGA clocks for ATI adapters, and clock
                 * lines that could have been used with the pre-2.1.1 driver.
                 */
                if (ATIMatchClockLine(pScreenInfo, pATI, InvalidClockLine,
                    NumberOfClocks, -1, 0))
                    pATI->OptionProbeClocks = TRUE;
                else

#ifndef AVOID_CPIO

                if ((pATI->Chip >= ATI_CHIP_18800) &&
                    (pATI->Adapter != ATI_ADAPTER_V4))

#endif /* AVOID_CPIO */

                {
                    /*
                     * Check for clocks that are specified in the wrong order.
                     * This is meant to catch those who are trying to use the
                     * clock order intended for the old accelerated servers.
                     */
                    while ((++ClockMap, ClockMap %= NumberOf(ClockMaps)))
                    {
                        pATI->Clock = ATIMatchClockLine(pScreenInfo, pATI,
                            SpecificationClockLine, NumberOfUndividedClocks,
                            -1, ClockMap);
                        if (pATI->Clock != ATI_CLOCK_NONE)
                        {
                            xf86DrvMsgVerb(pScreenInfo->scrnIndex,
                                X_WARNING, 0,
                                "XF86Config clock ordering incorrect.  Clocks"
                                " will be reordered.\n");
                            break;
                        }
                    }
                }
            }
            else
            /* Ensure crystals are not matched to clock chips, and vice versa */

#ifndef AVOID_CPIO

            if ((pATI->Chip <= ATI_CHIP_18800) ||
                (pATI->Adapter == ATI_ADAPTER_V4))
            {
                if (pATI->Clock > ATI_CLOCK_CRYSTALS)
                    pATI->OptionProbeClocks = TRUE;
            }
            else

#endif /* AVOID_CPIO */

            {
                if (pATI->Clock == ATI_CLOCK_CRYSTALS)
                    pATI->OptionProbeClocks = TRUE;
            }

            if (pATI->OptionProbeClocks)
            {
                xf86DrvMsgVerb(pScreenInfo->scrnIndex, X_WARNING, 0,
                    "Invalid or obsolete XF86Config clocks line rejected.\n"
                    " Clocks will be probed.\n");
                goto ProbeClocks;
            }
        }
    }

    if (pATI->ProgrammableClock != ATI_CLOCK_FIXED)
        pATI->ProgrammableClock = ATI_CLOCK_FIXED;
    else if (pATI->Clock == ATI_CLOCK_NONE)
        xf86DrvMsgVerb(pScreenInfo->scrnIndex, X_WARNING, 0,
            "Unknown clock generator detected.\n");
    else

#ifndef AVOID_CPIO

    if (pATI->Clock == ATI_CLOCK_CRYSTALS)
        xf86DrvMsg(pScreenInfo->scrnIndex, X_PROBED,
            "This adapter uses crystals to generate clock frequencies.\n");
    else if (pATI->Clock != ATI_CLOCK_VGA)

#endif /* AVOID_CPIO */

    {
        xf86DrvMsg(pScreenInfo->scrnIndex, X_PROBED,
            "%s clock chip detected.\n", ATIClockNames[pATI->Clock]);
    }

    if (pATI->Clock != ATI_CLOCK_NONE)
    {
        /* Replace undivided clocks with specification values */
        for (ClockIndex = 0;
             ClockIndex < NumberOfUndividedClocks;
             ClockIndex++)
        {
            /*
             * Don't replace clocks that are probed, documented, or set by the
             * user to zero.  One exception is that we need to override the
             * user's value for the spare settings on a crystal-based adapter.
             * Another exception is when the user specifies the clock ordering
             * intended for the old accelerated servers.
             */
            SpecificationClock =
                SpecificationClockLine[pATI->Clock][ClockIndex];
            if (SpecificationClock < 0)
                break;
            if (!ClockMap)
            {
                if (!pScreenInfo->clock[ClockIndex])
                    continue;
                if (!SpecificationClock)
                {
                    if (pATI->Clock != ATI_CLOCK_CRYSTALS)
                        continue;
                }
                else
                {
                    /*
                     * Due to the way clock lines are matched, the following
                     * can prevent the override if the clock is probed,
                     * documented or set by the user to a value greater than
                     * maxClock.
                     */
                    if (abs(SpecificationClock -
                            pScreenInfo->clock[ClockIndex]) > CLOCK_TOLERANCE)
                        continue;
                }
            }
            pScreenInfo->clock[ClockIndex] = SpecificationClock;
        }

        /* Adjust divided clocks */
        for (ClockIndex = NumberOfUndividedClocks;
             ClockIndex < NumberOfClocks;
             ClockIndex++)
            pScreenInfo->clock[ClockIndex] = ATIDivide(
                pScreenInfo->clock[ClockIndex % NumberOfUndividedClocks],
                (ClockIndex / NumberOfUndividedClocks) + 1, 0, 0);
    }

    /* Tell user about fixed clocks */
    xf86ShowClocks(pScreenInfo, pATI->OptionProbeClocks ? X_PROBED : X_CONFIG);

    /* Prevent selection of high clocks, even by V_CLKDIV2 modes */
    for (ClockIndex = 0;  ClockIndex < NumberOfClocks;  ClockIndex++)
        if (pScreenInfo->clock[ClockIndex] > pRange->maxClock)
            pScreenInfo->clock[ClockIndex] = 0;
}

/*
 * ATIClockSave --
 *
 * This function saves that part of an ATIHWRec that relates to clocks.
 */
void
ATIClockSave
(
    ScrnInfoPtr pScreenInfo,
    ATIPtr      pATI,
    ATIHWPtr    pATIHW
)
{
    if (pScreenInfo->vtSema && (pATI->ProgrammableClock > ATI_CLOCK_FIXED))
    {

#ifndef AVOID_CPIO

        if (pATIHW->crtc == ATI_CRTC_VGA)
        {
            pATIHW->ClockMap = ATIVGAProgrammableClockMap;
            pATIHW->ClockUnmap = ATIVGAProgrammableClockUnmap;
        }
        else

#endif /* AVOID_CPIO */

        {
            pATIHW->ClockMap = ATIProgrammableClockMap;
            pATIHW->ClockUnmap = ATIProgrammableClockUnmap;
        }
    }
    else
    {

#ifndef AVOID_CPIO

        if (pATIHW->crtc != ATI_CRTC_VGA)

#endif /* AVOID_CPIO */

        {
            pATIHW->ClockMap = ATIAcceleratorClockMap;
            pATIHW->ClockUnmap = ATIAcceleratorClockUnmap;
        }

#ifndef AVOID_CPIO

        else if (pATI->Chip < ATI_CHIP_68800)
        {
            pATIHW->ClockMap = ATIVGAWonderClockMap;
            pATIHW->ClockUnmap = ATIVGAWonderClockUnmap;
        }
        else
        {
            pATIHW->ClockMap = ATIMachVGAClockMap;
            pATIHW->ClockUnmap = ATIMachVGAClockUnmap;
        }

#endif /* AVOID_CPIO */

    }
}

/*
 * ATIClockCalculate --
 *
 * This function is called to generate, if necessary, the data needed for clock
 * programming, and set clock select bits in various register values.
 */
Bool
ATIClockCalculate
(
    int            iScreen,
    ATIPtr         pATI,
    ATIHWPtr       pATIHW,
    DisplayModePtr pMode
)
{
    int N, M, D;
    int ClockSelect, N1, MinimumGap;
    int Frequency, Multiple;            /* Used as temporaries */

    /* Set default values */
    pATIHW->FeedbackDivider = pATIHW->ReferenceDivider = pATIHW->PostDivider = 0;

    if ((pATI->ProgrammableClock <= ATI_CLOCK_FIXED) ||
        ((pATI->ProgrammableClock == ATI_CLOCK_CH8398) &&
         (pMode->ClockIndex < 2)))
    {
        /* Use a fixed clock */
        ClockSelect = pMode->ClockIndex;
    }
    else
    {
        /* Generate clock programme word, using units of kHz */
        MinimumGap = ((unsigned int)(-1)) >> 1;

        /* Loop through reference dividers */
        for (M = pATI->ClockDescriptor.MinM;
             M <= pATI->ClockDescriptor.MaxM;
             M++)
        {
            /* Loop through post-dividers */
            for (D = 0;  D < pATI->ClockDescriptor.NumD;  D++)
            {
                if (!pATI->ClockDescriptor.PostDividers[D])
                    continue;

                /* Limit undivided VCO to maxClock */
                if (pATI->maxClock &&
                    ((pATI->maxClock / pATI->ClockDescriptor.PostDividers[D]) <
                     pMode->Clock))
                    continue;

                /*
                 * Calculate closest feedback divider and apply its
                 * restrictions.
                 */
                Multiple = M * pATI->ReferenceDenominator *
                    pATI->ClockDescriptor.PostDividers[D];
                N = ATIDivide(pMode->Clock * Multiple,
                    pATI->ReferenceNumerator, 0, 0);
                if (N < pATI->ClockDescriptor.MinN)
                    N = pATI->ClockDescriptor.MinN;
                else if (N > pATI->ClockDescriptor.MaxN)
                    N = pATI->ClockDescriptor.MaxN;
                N -= pATI->ClockDescriptor.NAdjust;
                N1 = (N / pATI->ClockDescriptor.N1) * pATI->ClockDescriptor.N2;
                if (N > N1)
                    N = ATIDivide(N1 + 1, pATI->ClockDescriptor.N1, 0, 1);
                N += pATI->ClockDescriptor.NAdjust;
                N1 += pATI->ClockDescriptor.NAdjust;

                for (;  ;  N = N1)
                {
                    /* Pick the closest setting */
                    Frequency = abs(ATIDivide(N * pATI->ReferenceNumerator,
                        Multiple, 0, 0) - pMode->Clock);
                    if ((Frequency < MinimumGap) ||
                        ((Frequency == MinimumGap) &&
                         (pATIHW->FeedbackDivider < N)))
                    {
                        /* Save settings */
                        pATIHW->FeedbackDivider = N;
                        pATIHW->ReferenceDivider = M;
                        pATIHW->PostDivider = D;
                        MinimumGap = Frequency;
                    }

                    if (N <= N1)
                        break;
                }
            }
        }

        Multiple = pATIHW->ReferenceDivider * pATI->ReferenceDenominator *
            pATI->ClockDescriptor.PostDividers[pATIHW->PostDivider];
        Frequency = pATIHW->FeedbackDivider * pATI->ReferenceNumerator;
        Frequency = ATIDivide(Frequency, Multiple, 0, 0);
        if (abs(Frequency - pMode->Clock) > CLOCK_TOLERANCE)
        {
            xf86DrvMsg(iScreen, X_ERROR,
                "Unable to programme clock %.3fMHz for mode %s.\n",
                (double)(pMode->Clock) / 1000.0, pMode->name);
            return FALSE;
        }
        pMode->SynthClock = Frequency;
        ClockSelect = pATI->ClockNumberToProgramme;

        xf86ErrorFVerb(4,
            "\n Programming clock %d to %.3fMHz for mode %s."
            "  N=%d, M=%d, D=%d.\n",
            ClockSelect, (double)Frequency / 1000.0, pMode->name,
            pATIHW->FeedbackDivider, pATIHW->ReferenceDivider,
            pATIHW->PostDivider);

        if (pATI->Chip >= ATI_CHIP_264VTB)
            ATIDSPCalculate(pATI, pATIHW, pMode);
    }

    /* Set clock select bits, after remapping them */
    pATIHW->clock = ClockSelect;        /* Save pre-map clock number */
    ClockSelect = MapClockIndex(pATIHW->ClockMap, ClockSelect);

    switch (pATIHW->crtc)
    {

#ifndef AVOID_CPIO

        case ATI_CRTC_VGA:
            pATIHW->genmo = (pATIHW->genmo & 0xF3U) |
                ((ClockSelect << 2) & 0x0CU);

            if (pATI->CPIO_VGAWonder)
            {
                /* Set ATI clock select bits */
                if (pATI->Chip <= ATI_CHIP_18800)
                    pATIHW->b2 = (pATIHW->b2 & 0xBFU) |
                        ((ClockSelect << 4) & 0x40U);
                else
                {
                    pATIHW->be = (pATIHW->be & 0xEFU) |
                        ((ClockSelect << 2) & 0x10U);
                    if (pATI->Adapter != ATI_ADAPTER_V4)
                    {
                        ClockSelect >>= 1;
                        pATIHW->b9 = (pATIHW->b9 & 0xFDU) |
                            ((ClockSelect >> 1) & 0x02U);
                    }
                }

                /* Set clock divider bits */
                pATIHW->b8 = (pATIHW->b8 & 0x3FU) |
                    ((ClockSelect << 3) & 0xC0U);
            }
            break;

#endif /* AVOID_CPIO */

        case ATI_CRTC_MACH64:
            pATIHW->clock_cntl = CLOCK_STROBE |
                SetBits(ClockSelect, CLOCK_SELECT | CLOCK_DIVIDER);
            break;

        default:
            break;
    }

    return TRUE;
}

/*
 * ATIClockSet --
 *
 * This function is called to programme a clock for the mode being set.
 */
void
ATIClockSet
(
    ATIPtr      pATI,
    ATIHWPtr    pATIHW
)
{
    CARD32 crtc_gen_cntl, tmp;
    CARD8 clock_cntl0;
    CARD8 tmp2;
    unsigned int Programme;
    int N = pATIHW->FeedbackDivider - pATI->ClockDescriptor.NAdjust;
    int M = pATIHW->ReferenceDivider - pATI->ClockDescriptor.MAdjust;
    int D = pATIHW->PostDivider;

    /* Temporarily switch to accelerator mode */
    crtc_gen_cntl = inr(CRTC_GEN_CNTL);
    if (!(crtc_gen_cntl & CRTC_EXT_DISP_EN))
        outr(CRTC_GEN_CNTL, crtc_gen_cntl | CRTC_EXT_DISP_EN);

    switch (pATI->ProgrammableClock)
    {
        case ATI_CLOCK_ICS2595:
            clock_cntl0 = in8(CLOCK_CNTL);

            Programme = (SetBits(pATIHW->clock, ICS2595_CLOCK) |
                SetBits(N, ICS2595_FB_DIV) | SetBits(D, ICS2595_POST_DIV)) ^
                ICS2595_TOGGLE;

            ATIDelay(50000);            /* 50 milliseconds */

            (void)xf86DisableInterrupts();

            /* Send all 20 bits of programme word */
            while (Programme >= CLOCK_BIT)
            {
                tmp = (Programme & CLOCK_BIT) | CLOCK_STROBE;
                out8(CLOCK_CNTL, tmp);
                ATIDelay(26);           /* 26 microseconds */
                out8(CLOCK_CNTL, tmp | CLOCK_PULSE);
                ATIDelay(26);           /* 26 microseconds */
                Programme >>= 1;
            }

            xf86EnableInterrupts();

            /* Restore register */
            out8(CLOCK_CNTL, clock_cntl0 | CLOCK_STROBE);
            break;

        case ATI_CLOCK_STG1703:
            (void)ATIGetDACCmdReg(pATI);
            (void)in8(M64_DAC_MASK);
            out8(M64_DAC_MASK, (pATIHW->clock << 1) + 0x20U);
            out8(M64_DAC_MASK, 0);
            out8(M64_DAC_MASK, SetBits(N, 0xFFU));
            out8(M64_DAC_MASK, SetBits(M, 0x1FU) | SetBits(D, 0xE0U));
            break;

        case ATI_CLOCK_CH8398:
            tmp = inr(DAC_CNTL) | (DAC_EXT_SEL_RS2 | DAC_EXT_SEL_RS3);
            outr(DAC_CNTL, tmp);
            out8(M64_DAC_WRITE, pATIHW->clock);
            out8(M64_DAC_DATA, SetBits(N, 0xFFU));
            out8(M64_DAC_DATA, SetBits(M, 0x3FU) | SetBits(D, 0xC0U));
            out8(M64_DAC_MASK, 0x04U);
            outr(DAC_CNTL, tmp & ~(DAC_EXT_SEL_RS2 | DAC_EXT_SEL_RS3));
            tmp2 = in8(M64_DAC_WRITE);
            out8(M64_DAC_WRITE, (tmp2 & 0x70U) | 0x80U);
            outr(DAC_CNTL, tmp & ~DAC_EXT_SEL_RS2);
            break;

        case ATI_CLOCK_INTERNAL:
            /* Reset VCLK generator */
            ATIPutMach64PLLReg(PLL_VCLK_CNTL, pATIHW->pll_vclk_cntl);

            /* Set post-divider */
            tmp2 = pATIHW->clock << 1;
            tmp = ATIGetMach64PLLReg(PLL_VCLK_POST_DIV);
            tmp &= ~(0x03U << tmp2);
            tmp |= SetBits(D, 0x03U) << tmp2;
            ATIPutMach64PLLReg(PLL_VCLK_POST_DIV, tmp);

            /* Set extended post-divider */
            tmp = ATIGetMach64PLLReg(PLL_XCLK_CNTL);
            tmp &= ~(SetBits(1, PLL_VCLK0_XDIV) << pATIHW->clock);
            tmp |= SetBits(D >> 2, PLL_VCLK0_XDIV) << pATIHW->clock;
            ATIPutMach64PLLReg(PLL_XCLK_CNTL, tmp);

            /* Set feedback divider */
            tmp = PLL_VCLK0_FB_DIV + pATIHW->clock;
            ATIPutMach64PLLReg(tmp, SetBits(N, 0xFFU));

            /* End VCLK generator reset */
            ATIPutMach64PLLReg(PLL_VCLK_CNTL,
                pATIHW->pll_vclk_cntl & ~PLL_VCLK_RESET);

            /* Reset write bit */
            ATIAccessMach64PLLReg(pATI, 0, FALSE);
            break;

        case ATI_CLOCK_ATT20C408:
            (void)ATIGetDACCmdReg(pATI);
            tmp = in8(M64_DAC_MASK);
            (void)ATIGetDACCmdReg(pATI);
            out8(M64_DAC_MASK, tmp | 1);
            out8(M64_DAC_WRITE, 1);
            out8(M64_DAC_MASK, tmp | 9);
            ATIDelay(400);              /* 400 microseconds */
            tmp2 = (pATIHW->clock << 2) + 0x40U;
            out8(M64_DAC_WRITE, tmp2);
            out8(M64_DAC_MASK, SetBits(N, 0xFFU));
            out8(M64_DAC_WRITE, ++tmp2);
            out8(M64_DAC_MASK, SetBits(M, 0x3FU) | SetBits(D, 0xC0U));
            out8(M64_DAC_WRITE, ++tmp2);
            out8(M64_DAC_MASK, 0x77U);
            ATIDelay(400);              /* 400 microseconds */
            out8(M64_DAC_WRITE, 1);
            out8(M64_DAC_MASK, tmp);
            break;

        case ATI_CLOCK_IBMRGB514:
            /*
             * Here, only update in-core data.  It will be written out later by
             * ATIRGB514Set().
             */
            tmp = (pATIHW->clock << 1) + 0x20U;
            pATIHW->ibmrgb514[tmp] =
                (SetBits(N, 0x3FU) | SetBits(D, 0xC0U)) ^ 0xC0U;
            pATIHW->ibmrgb514[tmp + 1] = SetBits(M, 0x3FU);
            break;

        default:
            break;
    }

    (void)in8(M64_DAC_WRITE);    /* Clear DAC counter */

    /* Restore register */
    if (!(crtc_gen_cntl & CRTC_EXT_DISP_EN))
        outr(CRTC_GEN_CNTL, crtc_gen_cntl);
}

/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/ati/atipreinit.c,v 1.65 2003/01/01 19:16:33 tsi Exp $ */
/*
 * Copyright 1999 through 2003 by Marc Aurele La France (TSI @ UQV), tsi@xfree86.org
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
#include "atiadapter.h"
#include "atiadjust.h"
#include "atibus.h"
#include "atichip.h"
#include "aticonfig.h"
#include "aticursor.h"
#include "atidac.h"
#include "atidsp.h"
#include "atiident.h"
#include "atiload.h"
#include "atilock.h"
#include "atimach64.h"
#include "atimach64io.h"
#include "atimode.h"
#include "atipreinit.h"
#include "atiprint.h"
#include "atividmem.h"
#include "atiwonderio.h"

#include "vbe.h"
#include "xf86RAC.h"

#ifndef AVOID_CPIO

typedef CARD16 Colour;          /* The correct spelling should be OK :-) */

/*
 * Bit patterns which are extremely unlikely to show up when reading from
 * nonexistant memory (which normally shows up as either all bits set or all
 * bits clear).
 */
static const Colour Test_Pixel[] = {0x5AA5U, 0x55AAU, 0xA55AU, 0xCA53U};

static const struct
{
    int videoRamSize;
    int Miscellaneous_Options_Setting;
    struct
    {
        short int x, y;
    }
    Coordinates[NumberOf(Test_Pixel) + 1];
}
Test_Case[] =
{
    /*
     * Given the engine settings used, only a 4M card will have enough memory
     * to back up the 1025th line of the display.  Since the pixel coordinates
     * are zero-based, line 1024 will be the first one which is only backed on
     * 4M cards.
     *
     * <Mark_Weaver@brown.edu>:
     * In case memory is being wrapped, (0,0) and (0,1024) to make sure they
     * can each hold a unique value.
     */
    {4096, MEM_SIZE_4M, {{0,0}, {0,1024}, {-1,-1}}},

    /*
     * This card has 2M or less.  On a 1M card, the first 2M of the card's
     * memory will have even doublewords backed by physical memory and odd
     * doublewords unbacked.
     *
     * Pixels 0 and 1 of a row will be in the zeroth doubleword, while pixels 2
     * and 3 will be in the first.  Check both pixels 2 and 3 in case this is a
     * pseudo-1M card (one chip pulled to turn a 2M card into a 1M card).
     *
     * <Mark_Weaver@brown.edu>:
     * I don't have a 1M card, so I'm taking a stab in the dark.  Maybe memory
     * wraps every 512 lines, or maybe odd doublewords are aliases of their
     * even doubleword counterparts.  I try everything here.
     */
    {2048, MEM_SIZE_2M, {{0,0}, {0,512}, {2,0}, {3,0}, {-1,-1}}},

    /*
     * This is a either a 1M card or a 512k card.  Test pixel 1, since it is an
     * odd word in an even doubleword.
     *
     * <Mark_Weaver@brown.edu>:
     * This is the same idea as the test above.
     */
    {1024, MEM_SIZE_1M, {{0,0}, {0,256}, {1,0}, {-1,-1}}},

    /*
     * Assume it is a 512k card by default, since that is the minimum
     * configuration.
     */
    {512, MEM_SIZE_512K, {{-1,-1}}}
};

/*
 * ATIMach32ReadPixel --
 *
 * Return the colour of the specified screen location.  Called from
 * ATIMach32videoRam function below.
 */
static Colour
ATIMach32ReadPixel
(
    const short int X,
    const short int Y
)
{
    Colour Pixel_Colour;

    /* Wait for idle engine */
    ProbeWaitIdleEmpty();

    /* Set up engine for pixel read */
    ATIWaitQueue(7);
    outw(RD_MASK, (CARD16)(~0));
    outw(DP_CONFIG, FG_COLOR_SRC_BLIT | DATA_WIDTH | DRAW | DATA_ORDER);
    outw(CUR_X, X);
    outw(CUR_Y, Y);
    outw(DEST_X_START, X);
    outw(DEST_X_END, X + 1);
    outw(DEST_Y_END, Y + 1);

    /* Wait for data to become ready */
    ATIWaitQueue(16);
    WaitDataReady();

    /* Read pixel colour */
    Pixel_Colour = inw(PIX_TRANS);
    ProbeWaitIdleEmpty();
    return Pixel_Colour;
}

/*
 * ATIMach32WritePixel --
 *
 * Set the colour of the specified screen location.  Called from
 * ATIMach32videoRam function below.
 */
static void
ATIMach32WritePixel
(
    const short int X,
    const short int Y,
    const Colour    Pixel_Colour
)
{
    /* Set up engine for pixel write */
    ATIWaitQueue(9);
    outw(WRT_MASK, (CARD16)(~0));
    outw(DP_CONFIG, FG_COLOR_SRC_FG | DRAW | READ_WRITE);
    outw(ALU_FG_FN, MIX_FN_PAINT);
    outw(FRGD_COLOR, Pixel_Colour);
    outw(CUR_X, X);
    outw(CUR_Y, Y);
    outw(DEST_X_START, X);
    outw(DEST_X_END, X + 1);
    outw(DEST_Y_END, Y + 1);
}

/*
 * ATIMach32videoRam --
 *
 * Determine the amount of video memory installed on an 68800-6 based adapter.
 * This is done because these chips exhibit a bug that causes their
 * MISC_OPTIONS register to report 1M rather than the true amount of memory.
 *
 * This function is adapted from a similar function in mach32mem.c written by
 * Robert Wolff, David Dawes and Mark Weaver.
 */
static int
ATIMach32videoRam
(
    void
)
{
    CARD16 clock_sel, mem_bndry, misc_options, ext_ge_config;
    Colour saved_Pixel[NumberOf(Test_Pixel)];
    unsigned int Case_Number, Pixel_Number;
    Bool AllPixelsOK;

    /* Save register values to be modified */
    clock_sel = inw(CLOCK_SEL);
    mem_bndry = inw(MEM_BNDRY);
    misc_options = inw(MISC_OPTIONS) & ~MEM_SIZE_ALIAS;
    ext_ge_config = inw(R_EXT_GE_CONFIG);

    /* Wait for enough FIFO entries */
    ATIWaitQueue(7);

    /* Enable accelerator */
    outw(CLOCK_SEL, clock_sel | DISABPASSTHRU);

    /* Make accelerator and VGA share video memory */
    outw(MEM_BNDRY, mem_bndry & ~(MEM_PAGE_BNDRY | MEM_BNDRY_ENA));

    /* Prevent video memory wrap */
    outw(MISC_OPTIONS, misc_options | MEM_SIZE_4M);

    /*
     * Set up the drawing engine for a pitch of 1024 at 16 bits per pixel.  No
     * need to mess with the CRT because the results of this test are not
     * intended to be seen.
     */
    outw(EXT_GE_CONFIG, PIXEL_WIDTH_16 | ORDER_16BPP_565 | MONITOR_8514 |
        ALIAS_ENA);
    outw(GE_PITCH, 1024 >> 3);
    outw(GE_OFFSET_HI, 0);
    outw(GE_OFFSET_LO, 0);

    for (Case_Number = 0;
         Case_Number < (NumberOf(Test_Case) - 1);
         Case_Number++)
    {
        /* Reduce redundancy as per Mark_Weaver@brown.edu */
#       define TestPixel Test_Case[Case_Number].Coordinates[Pixel_Number]
#       define ForEachTestPixel        \
            for (Pixel_Number = 0;  TestPixel.x >= 0;  Pixel_Number++)

        /* Save pixel colours that will be clobbered */
        ForEachTestPixel
            saved_Pixel[Pixel_Number] =
                ATIMach32ReadPixel(TestPixel.x, TestPixel.y);

        /* Write test patterns */
        ForEachTestPixel
            ATIMach32WritePixel(TestPixel.x, TestPixel.y,
                Test_Pixel[Pixel_Number]);

        /* Test for lost pixels */
        AllPixelsOK = TRUE;
        ForEachTestPixel
            if (ATIMach32ReadPixel(TestPixel.x, TestPixel.y) !=
                Test_Pixel[Pixel_Number])
            {
                AllPixelsOK = FALSE;
                break;
            }

        /* Restore clobbered pixels */
        ForEachTestPixel
            ATIMach32WritePixel(TestPixel.x, TestPixel.y,
                saved_Pixel[Pixel_Number]);

        /* End test on success */
        if (AllPixelsOK)
            break;

        /* Completeness */
#       undef ForEachTestPixel
#       undef TestPixel
    }

    /* Restore what was changed and correct MISC_OPTIONS register */
    ATIWaitQueue(4);
    outw(EXT_GE_CONFIG, ext_ge_config);
    misc_options |= Test_Case[Case_Number].Miscellaneous_Options_Setting;
    outw(MISC_OPTIONS, misc_options);
    outw(MEM_BNDRY, mem_bndry);
    outw(CLOCK_SEL, clock_sel);

    /* Wait for activity to die down */
    ProbeWaitIdleEmpty();

    /* Tell ATIPreInit the REAL story */
    return Test_Case[Case_Number].videoRamSize;
}

#endif /* AVOID_CPIO */

/*
 * ATIReportMemory --
 *
 * This function reports on the amount and type of video memory found.
 */
static void
ATIReportMemory
(
    ScrnInfoPtr pScreenInfo,
    ATIPtr      pATI,
    const char *MemoryTypeName
)
{
    char Buffer[128], *Message;

    Message = Buffer +
        snprintf(Buffer, SizeOf(Buffer), "%d kB of %s detected",
            pATI->VideoRAM, MemoryTypeName);

#ifndef AVOID_CPIO

    if (pATI->depth == 1)
    {
        /* 1bpp only uses one plane of four */
        pScreenInfo->videoRam /= 4;
        Message += snprintf(Message, Buffer + SizeOf(Buffer) - Message,
            " (using %d kB)", pScreenInfo->videoRam);
    }
    else

#endif /* AVOID_CPIO */

    if (pATI->VideoRAM > pScreenInfo->videoRam)
        Message += snprintf(Message, Buffer + SizeOf(Buffer) - Message,
            " (using %d kB)", pScreenInfo->videoRam);
    xf86DrvMsg(pScreenInfo->scrnIndex, X_PROBED, "%s.\n", Buffer);
}

static const int videoRamSizes[] =
    {0, 256, 512, 1024, 2*1024, 4*1024, 6*1024, 8*1024, 12*1024, 16*1024, 0};
static const rgb   defaultWeight = {0, 0, 0};
static const Gamma defaultGamma  = {0.0, 0.0, 0.0};

/*
 * ATIMapMach64 --
 *
 * This function attempts to mmap() a Mach64's MMIO aperture.
 */
static void
ATIMapMach64
(
    int    iScreen,
    ATIPtr pATI
)
{
    (void)ATIMapApertures(iScreen, pATI);
    if (!pATI->pBlock[0] ||
        (pATI->config_chip_id != inr(CONFIG_CHIP_ID)))
        ATIUnmapApertures(iScreen, pATI);
}

/*
 * ATIPrintNoiseIfRequested --
 *
 * This function formats debugging information on the server's stderr when
 * requested by the user through the server's verbosity setting.
 */
static void
ATIPrintNoiseIfRequested
(
    ATIPtr       pATI,
    CARD8       *BIOS,
    unsigned int BIOSSize
)
{
    if (xf86GetVerbosity() <= 3)
        return;

    if (BIOSSize > 0)
        ATIPrintBIOS(BIOS, BIOSSize);
    xf86ErrorFVerb(4, "\n On server entry:\n");
    ATIPrintRegisters(pATI);
}

/*
 * ATIPreInit --
 *
 * This function is only called once per screen at the start of the first
 * server generation.
 */
Bool
ATIPreInit
(
    ScrnInfoPtr pScreenInfo,
    int flags
)
{
#   define           BIOS_SIZE       0x00010000U     /* 64kB */
    CARD8            BIOS[BIOS_SIZE];
#   define           BIOSByte(_n)    ((CARD8)(BIOS[_n]))
#   define           BIOSWord(_n)    ((CARD16)(BIOS[_n] |                \
                                               (BIOS[(_n) + 1] << 8)))
#   define           BIOSLong(_n)    ((CARD32)(BIOS[_n] |                \
                                               (BIOS[(_n) + 1] << 8) |   \
                                               (BIOS[(_n) + 2] << 16) |  \
                                               (BIOS[(_n) + 3] << 24)))
    unsigned int     BIOSSize = 0;
    unsigned int     ROMTable = 0, ClockTable = 0, FrequencyTable = 0;
    unsigned int     LCDTable = 0, LCDPanelInfo = 0;

    char             Buffer[128], *Message;
    ATIPtr           pATI;
    GDevPtr          pGDev;
    EntityInfoPtr    pEntity;
    resPtr           pResources;
    pciVideoPtr      pVideo;
    DisplayModePtr   pMode;
    unsigned long    Block0Base;
    CARD32           IOValue;
    int              i, j, AcceleratorVideoRAM = 0, ServerVideoRAM;
    int              Numerator, Denominator;
    int              MinX, MinY;
    ClockRange       ATIClockRange = {NULL, 0, 80000, 0, TRUE, TRUE, 1, 1, 0};
    int              DefaultmaxClock = 0;
    int              minPitch, maxPitch = 0xFFU, maxHeight = 0;
    int              ApertureSize = 0x00010000U;
    LookupModeFlags  Strategy = LOOKUP_CLOSEST_CLOCK;

#   define           pATIHW     (&pATI->OldHW)

#ifndef AVOID_CPIO

    xf86Int10InfoPtr pInt10Info = NULL;
    vbeInfoPtr       pVBE;
    pointer          pInt10Module, pDDCModule = NULL, pVBEModule = NULL;
    int              VGAVideoRAM = 0;
    resRange         Resources[2] = {{0, 0, 0}, _END};

#endif /* AVOID_CPIO */

    if (pScreenInfo->numEntities != 1)
    {
        xf86DrvMsg(pScreenInfo->scrnIndex, X_ERROR,
            "Logic error:  Number of attached entities not 1.\n");
        return FALSE;
    }

    pATI = ATIPTR(pScreenInfo);

    if (pATI->iEntity != pScreenInfo->entityList[0])
    {
        xf86DrvMsg(pScreenInfo->scrnIndex, X_ERROR,
            "Logic error:  Entity mismatch.\n");
        return FALSE;
    }

    /* Register resources */
    pEntity = xf86GetEntityInfo(pATI->iEntity);
    pGDev = pEntity->device;
    pResources = pEntity->resources;
    xfree(pEntity);
    if (!pResources)
        pResources = xf86RegisterResources(pATI->iEntity, NULL,
            pATI->SharedAccelerator ? ResShared : ResExclusive);
    if (pResources)
    {
        xf86DrvMsg(pScreenInfo->scrnIndex, X_ERROR,
            "Unable to register the following bus resources:\n");
        xf86PrintResList(0, pResources);
        xf86FreeResList(pResources);
        return FALSE;
    }

    ConfiguredMonitor = NULL;
    (void)memset(BIOS, 0, SizeOf(BIOS));

    if (!(flags & PROBE_DETECT))
    {
        xf86DrvMsg(pScreenInfo->scrnIndex,
            pATI->Chipset ? X_CONFIG : X_DEFAULT,
            "Chipset:  \"%s\".\n", ATIChipsetNames[pATI->Chipset]);

        /* Promote chipset specification */
        switch (pATI->Chipset)
        {

#ifndef AVOID_CPIO

            case ATI_CHIPSET_IBMVGA:
                if (pATI->Adapter == ATI_ADAPTER_VGA)
                    break;      /* XXX */
                /* Fall through */

            case ATI_CHIPSET_VGAWONDER:
                pATI->Chipset = ATI_CHIPSET_ATIVGA;
                break;

            case ATI_CHIPSET_IBM8514:
                if (pATI->Adapter == ATI_ADAPTER_8514A)
                    break;      /* XXX */
                /* Fall through */

            case ATI_CHIPSET_MACH8:
            case ATI_CHIPSET_MACH32:

#endif /* AVOID_CPIO */

            case ATI_CHIPSET_MACH64:
            case ATI_CHIPSET_RAGE128:
            case ATI_CHIPSET_RADEON:
                pATI->Chipset = ATI_CHIPSET_ATI;
                break;

            default:
                break;
        }

        /* Set monitor */
        pScreenInfo->monitor = pScreenInfo->confScreen->monitor;

        /* Set depth, bpp, etc. */
        if ((pATI->Chipset != ATI_CHIPSET_ATI) ||
            (pATI->Chip < ATI_CHIP_264CT))
            i = NoDepth24Support;       /* No support for >8bpp either */
        else
            i = Support24bppFb | Support32bppFb;
        if (!xf86SetDepthBpp(pScreenInfo, 8, 8, 8, i))
            return FALSE;

        for (j = 0;  ;  j++)
        {
            static const CARD8 AllowedDepthBpp[][2] =
            {

#ifndef AVOID_CPIO

                { 1,  1},
                { 4,  4},
                { 4,  8},

#endif /* AVOID_CPIO */

                { 8,  8},
                {15, 16},
                {16, 16},
                {24, 24},
                {24, 32}
            };

            if (j < NumberOf(AllowedDepthBpp))
            {
                if (pScreenInfo->depth > AllowedDepthBpp[j][0])
                    continue;

                if (pScreenInfo->depth == AllowedDepthBpp[j][0])
                {
                    if (pScreenInfo->bitsPerPixel > AllowedDepthBpp[j][1])
                        continue;

                    if (pScreenInfo->bitsPerPixel == AllowedDepthBpp[j][1])
                        break;
                }
            }

            xf86DrvMsg(pScreenInfo->scrnIndex, X_ERROR,
                "Driver does not support depth %d at fbbpp %d.\n",
                pScreenInfo->depth, pScreenInfo->bitsPerPixel);
            return FALSE;
        }

        xf86PrintDepthBpp(pScreenInfo);

        if ((i == NoDepth24Support) && (pScreenInfo->depth > 8))
        {
            xf86DrvMsg(pScreenInfo->scrnIndex, X_ERROR,
                "Depth %d is not supported through this adapter.\n",
                pScreenInfo->depth);
            return FALSE;
        }

        /* Pick up XF86Config options */
        ATIProcessOptions(pScreenInfo, pATI);
    }

#ifdef AVOID_CPIO

    else /* if (flags & PROBE_DETECT) */
        return TRUE;

#else /* AVOID_CPIO */

    /*
     * If there is an ix86-style BIOS, ensure its initialisation entry point
     * has been executed, and retrieve DDC and VBE information from it.
     */
    if (!(pInt10Module = ATILoadModule(pScreenInfo, "int10", ATIint10Symbols)))
        xf86DrvMsg(pScreenInfo->scrnIndex, X_WARNING,
            "Unable to load int10 module.\n");
    else if (!(pInt10Info = xf86InitInt10(pATI->iEntity)))
        xf86DrvMsg(pScreenInfo->scrnIndex, X_WARNING,
             "Unable to initialise int10 interface.\n");
    else
    {
        if (!(pDDCModule = ATILoadModule(pScreenInfo, "ddc", ATIddcSymbols)))
            xf86DrvMsg(pScreenInfo->scrnIndex, X_WARNING,
                "Unable to load ddc module.\n");
        else
        if (!(pVBEModule = ATILoadModule(pScreenInfo, "vbe", ATIvbeSymbols)))
            xf86DrvMsg(pScreenInfo->scrnIndex, X_WARNING,
                "Unable to load vbe module.\n");
        else
        {
            if ((pVBE = VBEInit(pInt10Info, pATI->iEntity)))
            {
                ConfiguredMonitor = vbeDoEDID(pVBE, pDDCModule);
                vbeFree(pVBE);
            }
            xf86UnloadSubModule(pVBEModule);
        }

        if (!(flags & PROBE_DETECT))
        {
            /* Validate, then make a private copy of, the initialised BIOS */
            CARD8 *pBIOS = xf86int10Addr(pInt10Info, pInt10Info->BIOSseg << 4);

            if ((pBIOS[0] != 0x55U) || (pBIOS[1] != 0xAAU) || !pBIOS[2])
                xf86DrvMsg(pScreenInfo->scrnIndex, X_WARNING,
                    "Unable to correctly retrieve adapter BIOS.\n");
            else
            {
                BIOSSize = pBIOS[2] << 9;
                if (BIOSSize > BIOS_SIZE)
                    BIOSSize = BIOS_SIZE;
                (void)memcpy(BIOS, pBIOS, BIOSSize);
            }
        }
    }

    /* De-activate int10 */
    xf86FreeInt10(pInt10Info);
    xf86UnloadSubModule(pInt10Module);

    if (flags & PROBE_DETECT)
    {
        xf86UnloadSubModule(pDDCModule);
        return TRUE;
    }

    if (ConfiguredMonitor)
    {
        xf86PrintEDID(ConfiguredMonitor);
        xf86SetDDCproperties(pScreenInfo, ConfiguredMonitor);
    }

    /* DDC module is no longer needed at this point */
    xf86UnloadSubModule(pDDCModule);

#endif /* AVOID_CPIO */

    pATI->Block0Base = 0;       /* Might no longer be valid */
    if ((pVideo = pATI->PCIInfo))
    {
        if (pATI->CPIODecoding == BLOCK_IO)
            pATI->CPIOBase = pVideo->ioBase[1];

        /* Set MMIO address from PCI configuration space, if available */
        if ((pATI->Block0Base = pVideo->memBase[2]))
        {
            if (pATI->Block0Base >= (CARD32)(-1 << pVideo->size[2]))
                pATI->Block0Base = 0;
            else
                pATI->Block0Base += 0x0400U;
        }
    }

#ifdef AVOID_CPIO

    pScreenInfo->racMemFlags =
        RAC_FB | RAC_COLORMAP | RAC_VIEWPORT | RAC_CURSOR;

#else /* AVOID_CPIO */

    pScreenInfo->racIoFlags =
        RAC_FB | RAC_COLORMAP | RAC_VIEWPORT | RAC_CURSOR;
    pScreenInfo->racMemFlags = RAC_FB | RAC_CURSOR;

#endif /* AVOID_CPIO */

    /* Deal with ChipID & ChipRev overrides */
    if (pGDev->chipID >= 0)
    {
        ATIChipType Chip;

        Chip = ATIChipID(pGDev->chipID,
            (pGDev->chipRev < 0) ? pATI->ChipRev : pGDev->chipRev);
        if (Chip != pATI->Chip)
        {
            pATI->Chip = Chip;
            pATI->ChipType = pGDev->chipID;
            if (pGDev->chipRev < 0)
            {
                xf86DrvMsg(pScreenInfo->scrnIndex, X_CONFIG,
                    "Driver messages reflect ChipID 0x%04X override.\n",
                    pATI->ChipType);
            }
            else
            {
                pATI->ChipRev = pGDev->chipRev;
                pATI->ChipVersion = GetBits(pATI->ChipRev,
                    GetBits(CFG_CHIP_VERSION, CFG_CHIP_REV));
                pATI->ChipFoundry = GetBits(pATI->ChipRev,
                    GetBits(CFG_CHIP_FOUNDRY, CFG_CHIP_REV));
                pATI->ChipRevision = GetBits(pATI->ChipRev,
                    GetBits(CFG_CHIP_REVISION, CFG_CHIP_REV));
                xf86DrvMsg(pScreenInfo->scrnIndex, X_CONFIG,
                    "Driver messages reflect ChipID 0x%04X and ChipRev 0x%02X"
                    " overrides.\n", pATI->ChipType, pATI->ChipRev);
            }
        }
    }

    /* Finish private area initialisation */
    pATI->DAC = ATI_DAC_GENERIC;

#ifndef AVOID_CPIO

    pATI->NewHW.SetBank = ATIx8800SetBank;
    pATI->BankInfo.SetSourceBank = ATIx8800SetRead;
    pATI->BankInfo.SetDestinationBank = ATIx8800SetWrite;
    pATI->BankInfo.SetSourceAndDestinationBanks = ATIx8800SetReadWrite;
    pATI->BankInfo.BankSize = 0x00010000U;      /* 64kB */

#endif /* AVOID_CPIO */

    pATI->LCDPanelID = -1;
    pATI->nFIFOEntries = 16;                    /* For now */

    /* Finish probing the adapter */
    switch (pATI->Adapter)
    {

#ifndef AVOID_CPIO

        case ATI_ADAPTER_NONE:
        case ATI_ADAPTER_EGA:
        case ATI_ADAPTER_EGA_PLUS:
        case ATI_ADAPTER_VGA:
        case ATI_ADAPTER_BASIC:
            pATI->NewHW.SetBank = (ATIBankProcPtr)NoopDDA;
            pATI->BankInfo.SetSourceBank =
                pATI->BankInfo.SetDestinationBank =
                pATI->BankInfo.SetSourceAndDestinationBanks =
                    (miBankProcPtr)NoopDDA;
            break;

        case ATI_ADAPTER_V3:
            pATI->NewHW.SetBank = ATIV3SetBank;
            pATI->BankInfo.SetSourceBank = ATIV3SetRead;
            pATI->BankInfo.SetDestinationBank = ATIV3SetWrite;
            pATI->BankInfo.SetSourceAndDestinationBanks = ATIV3SetReadWrite;
            break;

        case ATI_ADAPTER_V4:
        case ATI_ADAPTER_V5:
            pATI->NewHW.SetBank = ATIV4V5SetBank;
            pATI->BankInfo.SetSourceBank = ATIV4V5SetRead;
            pATI->BankInfo.SetDestinationBank = ATIV4V5SetWrite;
            pATI->BankInfo.SetSourceAndDestinationBanks = ATIV4V5SetReadWrite;
            break;

        case ATI_ADAPTER_XL:
            pATI->DAC = ATI_DAC_SC11483;
            break;

        case ATI_ADAPTER_8514A:
            pATI->VideoRAM =
                videoRamSizes[GetBits(inw(SUBSYS_STAT), _8PLANE) + 2];
            break;

        case ATI_ADAPTER_MACH8:
            pATI->VideoRAM =
                videoRamSizes[GetBits(inw(CONFIG_STATUS_1), MEM_INSTALLED) + 2];
            break;

        case ATI_ADAPTER_MACH32:
            IOValue = inw(CONFIG_STATUS_1);
            pATI->DAC = ATI_DAC(GetBits(IOValue, DACTYPE), 0);
            pATI->MemoryType = GetBits(IOValue, MEM_TYPE);

            IOValue = inw(MISC_OPTIONS);
            pATI->VideoRAM =
                videoRamSizes[GetBits(IOValue, MEM_SIZE_ALIAS) + 2];

            /*
             * The 68800-6 doesn't necessarily report the correct video memory
             * size.
             */
            if ((pATI->Chip == ATI_CHIP_68800_6) && (pATI->VideoRAM == 1024))
                pATI->VideoRAM = ATIMach32videoRam();

            break;

#endif /* AVOID_CPIO */

        case ATI_ADAPTER_MACH64:
            do
            {
                /*
                 * Find and mmap() MMIO area.  Allow only auxiliary aperture if
                 * it exists.
                 */
                if (!(Block0Base = pATI->Block0Base))
                {
                    if (pVideo)
                    {
                        /* Check tail end of linear (8MB or 4MB) aperture */
                        if ((pATI->Block0Base = pVideo->memBase[0]))
                        {
                            pATI->Block0Base += 0x007FFC00U;
                            ATIMapMach64(pScreenInfo->scrnIndex, pATI);
                            if (pATI->pBlock[0])
                                break;

                            pATI->Block0Base -= 0x00400000U;
                            ATIMapMach64(pScreenInfo->scrnIndex, pATI);
                            if (pATI->pBlock[0])
                                break;
                        }
                    }

                    /* Check VGA MMIO aperture */
                    pATI->Block0Base = 0x000BFC00U;
                }

                ATIMapMach64(pScreenInfo->scrnIndex, pATI);
            } while (0);
            pATI->Block0Base = Block0Base;

#ifdef AVOID_CPIO

            if (!pATI->pBlock[0])
            {
                xf86DrvMsg(pScreenInfo->scrnIndex, X_ERROR,
                    "Unable to mmap() adapter registers.\n");
                return FALSE;
            }

#endif /* AVOID_CPIO */

            pATIHW->crtc_gen_cntl = inr(CRTC_GEN_CNTL);
            if (!(pATIHW->crtc_gen_cntl & CRTC_EN) &&
                (pATI->Chip >= ATI_CHIP_264CT))
            {
                xf86DrvMsg(pScreenInfo->scrnIndex, X_ERROR,
                    "Adapter has not been initialised.\n");
                ATIPrintNoiseIfRequested(pATI, BIOS, BIOSSize);
                ATIUnmapApertures(pScreenInfo->scrnIndex, pATI);
                return FALSE;
            }

#ifdef AVOID_CPIO

            if (!(pATIHW->crtc_gen_cntl & CRTC_EXT_DISP_EN))
            {
                xf86DrvMsg(pScreenInfo->scrnIndex, X_ERROR,
                    "Adapters found to be in VGA mode on server entry are not"
                    " supported by the MMIO-only version of this driver.\n");
                ATIPrintNoiseIfRequested(pATI, BIOS, BIOSSize);
                ATIUnmapApertures(pScreenInfo->scrnIndex, pATI);
                return FALSE;
            }

#endif /* AVOID_CPIO */

            pATIHW->mem_cntl = inr(MEM_CNTL);
            if (pATI->Chip < ATI_CHIP_264VTB)
                pATI->VideoRAM =
                    videoRamSizes[GetBits(pATIHW->mem_cntl, CTL_MEM_SIZE) + 2];
            else
            {
                pATI->nFIFOEntries =            /* Don't care */
                    (unsigned int)(-1) >> 1;

                IOValue = GetBits(pATIHW->mem_cntl, CTL_MEM_SIZEB);
                if (IOValue < 8)
                    pATI->VideoRAM = (IOValue + 1) * 512;
                else if (IOValue < 12)
                    pATI->VideoRAM = (IOValue - 3) * 1024;
                else
                    pATI->VideoRAM = (IOValue - 7) * 2048;
            }

            pATI->DAC = GetBits(inr(DAC_CNTL), DAC_TYPE);

            IOValue = inr(CONFIG_STATUS64_0);
            if (pATI->Chip >= ATI_CHIP_264CT)
            {
                pATI->MemoryType = GetBits(IOValue, CFG_MEM_TYPE_T);

                /* Get LCD panel id and set LCD & TV I/O port numbers */
                if (pATI->Chip == ATI_CHIP_264LT)
                {
                    pATI->LCDPanelID = GetBits(IOValue, CFG_PANEL_ID);

                    pATIHW->horz_stretching = inr(HORZ_STRETCHING);
                    pATIHW->vert_stretching = inr(VERT_STRETCHING);
                    pATIHW->lcd_gen_ctrl = inr(LCD_GEN_CTRL);
                }
                else if ((pATI->Chip == ATI_CHIP_264LTPRO) ||
                         (pATI->Chip == ATI_CHIP_264XL) ||
                         (pATI->Chip == ATI_CHIP_MOBILITY))
                {
                    pATI->LCDPanelID = GetBits(IOValue, CFG_PANEL_ID);

                    pATIHW->lcd_index = inr(LCD_INDEX);
                    pATIHW->horz_stretching =
                        ATIGetMach64LCDReg(LCD_HORZ_STRETCHING);
                    pATI->LCDHorizontal =
                        GetBits(pATIHW->horz_stretching, HORZ_PANEL_SIZE);
                    if (pATI->LCDHorizontal)
                    {
                        if (pATI->LCDHorizontal == MaxBits(HORZ_PANEL_SIZE))
                            pATI->LCDHorizontal = 0;
                        else
                            pATI->LCDHorizontal =
                                (pATI->LCDHorizontal + 1) << 3;
                    }
                    pATIHW->ext_vert_stretch =
                        ATIGetMach64LCDReg(LCD_EXT_VERT_STRETCH);
                    pATI->LCDVertical =
                        GetBits(pATIHW->ext_vert_stretch, VERT_PANEL_SIZE);
                    if (pATI->LCDVertical)
                    {
                        if (pATI->LCDVertical == MaxBits(VERT_PANEL_SIZE))
                            pATI->LCDVertical = 0;
                        else
                            pATI->LCDVertical++;
                    }
                    pATIHW->vert_stretching =
                        ATIGetMach64LCDReg(LCD_VERT_STRETCHING);
                    pATIHW->lcd_gen_ctrl = ATIGetMach64LCDReg(LCD_GEN_CNTL);
                    outr(LCD_INDEX, pATIHW->lcd_index);
                }

                /*
                 * Don't bother with panel support if it hasn't been previously
                 * enabled.
                 */
                if ((pATI->LCDPanelID >= 0) &&
                    !(pATIHW->horz_stretching & HORZ_STRETCH_EN) &&
                    !(pATIHW->vert_stretching & VERT_STRETCH_EN) &&
                    !(pATIHW->lcd_gen_ctrl & LCD_ON))
                {
                    /*
                     * At this point, if an XL or Mobility BIOS hasn't set
                     * panel dimensions, then there is no panel.  Otherwise,
                     * keep any panel disabled to allow for modes greater than
                     * the panel's dimensions.
                     */
                    if ((pATI->Chip >= ATI_CHIP_264XL) &&
                        (!pATI->LCDHorizontal || !pATI->LCDVertical))
                        pATI->LCDPanelID = -1;
                    else
                        pATI->OptionPanelDisplay = FALSE;
                }
            }
            else
            {
                pATI->MemoryType = GetBits(IOValue, CFG_MEM_TYPE);

                /* Factor in what the BIOS says the DAC is */
                pATI->DAC = ATI_DAC(pATI->DAC,
                    GetBits(inr(SCRATCH_REG1), BIOS_INIT_DAC_SUBTYPE));
            }

            /*
             * RAMDAC types 0 & 1 for Mach64's are different than those for
             * Mach32's.
             */
            if (pATI->DAC < ATI_DAC_ATI68875)
                pATI->DAC += ATI_DAC_INTERNAL;

            break;

        default:
            break;
    }

    /*
     * For Mach64 adapters, pick up, from the BIOS, the type of programmable
     * clock generator (if any), and various information about it.
     */

#ifndef AVOID_CPIO

    if (pATI->Chip >= ATI_CHIP_88800GXC)

#endif /* AVOID_CPIO */

    {
        CARD16 ClockDac;

        /* Set up non-zero defaults */
        pATI->ClockDescriptor = ATIClockDescriptors[ATI_CLOCK_FIXED];
        pATI->ClockNumberToProgramme = -1;

        ROMTable = BIOSWord(0x48U);
        if ((ROMTable + 0x12U) > BIOSSize)
            ROMTable = 0;

        if (ROMTable > 0)
        {
            ClockTable = BIOSWord(ROMTable + 0x10U);
            if ((ClockTable + 0x20U) > BIOSSize)
                ClockTable = 0;
        }

        if (ClockTable > 0)
        {
            FrequencyTable = BIOSWord(ClockTable - 0x02U);
            if ((FrequencyTable > 0) &&
                ((FrequencyTable + 0x20U) <= BIOSSize))
            {
                for (i = 0;  i < 16;  i++)
                {
                    pATI->BIOSClocks[i] = BIOSWord(FrequencyTable);
                    FrequencyTable += 2;
                }
            }
            pATI->ProgrammableClock = BIOSByte(ClockTable);
            pATI->ClockNumberToProgramme = BIOSByte(ClockTable + 0x06U);
            switch (BIOSWord(ClockTable + 0x08U) / 10)
            {
                case 143:
                    pATI->ReferenceNumerator = 157500;
                    pATI->ReferenceDenominator = 11;
                    break;

                case 286:
                    pATI->ReferenceNumerator = 315000;
                    pATI->ReferenceDenominator = 11;
                    break;

                default:
                    pATI->ReferenceNumerator =
                        BIOSWord(ClockTable + 0x08U) * 10;
                    pATI->ReferenceDenominator = 1;
                    break;
            }
        }
        else
        {
            /*
             * Compensate for BIOS absence.  Note that the reference
             * frequency has already been set by option processing.
             */
            if ((pATI->DAC & ~0x0FU) == ATI_DAC_INTERNAL)
                pATI->ProgrammableClock = ATI_CLOCK_INTERNAL;
            else switch (pATI->DAC)
            {
                case ATI_DAC_STG1703:
                    pATI->ProgrammableClock = ATI_CLOCK_STG1703;
                    break;

                case ATI_DAC_CH8398:
                    pATI->ProgrammableClock = ATI_CLOCK_CH8398;
                    break;

                case ATI_DAC_ATT20C408:
                    pATI->ProgrammableClock = ATI_CLOCK_ATT20C408;
                    break;

                case ATI_DAC_IBMRGB514:
                    pATI->ProgrammableClock = ATI_CLOCK_IBMRGB514;
                    break;

                default:        /* Provisional */
                    pATI->ProgrammableClock = ATI_CLOCK_ICS2595;
                    break;
            }

            /* This should be safe for all generators except IBM's RGB514 */
            pATI->ClockNumberToProgramme = 3;
        }

        if ((pATI->ProgrammableClock > ATI_CLOCK_FIXED) &&
            (pATI->ProgrammableClock < ATI_CLOCK_MAX))
        {
            /*
             * Graphics PRO TURBO 1600's are unusual in that an ICS2595 is used
             * to generate clocks for VGA modes, and an IBM RGB514 is used for
             * accelerator modes.
             */
            if ((pATI->ProgrammableClock == ATI_CLOCK_ICS2595) &&
                (pATI->DAC == ATI_DAC_IBMRGB514) &&
                (pScreenInfo->depth >= 8) &&
                (pATI->Chipset == ATI_CHIPSET_ATI))
                pATI->ProgrammableClock = ATI_CLOCK_IBMRGB514;

            pATI->ClockDescriptor =
                ATIClockDescriptors[pATI->ProgrammableClock];
        }

        ClockDac = pATI->DAC;
        switch (pATI->ProgrammableClock)
        {
            case ATI_CLOCK_ICS2595:
                /*
                 * Pick up reference divider (43 or 46) appropriate to the chip
                 * revision level.
                 */
                if (ClockTable > 0)
                    pATI->ClockDescriptor.MinM =
                        pATI->ClockDescriptor.MaxM =
                            BIOSWord(ClockTable + 0x0AU);
                else if (!xf86NameCmp(pGDev->clockchip, "ATI 18818-0"))
                    pATI->ClockDescriptor.MinM =
                        pATI->ClockDescriptor.MaxM = 43;
                else if (!xf86NameCmp(pGDev->clockchip, "ATI 18818-1"))
                    pATI->ClockDescriptor.MinM =
                        pATI->ClockDescriptor.MaxM = 46;
                else
                    pATI->ProgrammableClock = ATI_CLOCK_UNKNOWN;
                break;

            case ATI_CLOCK_STG1703:
                /* This one's also a RAMDAC */
                ClockDac = ATI_DAC_STG1703;
                break;

            case ATI_CLOCK_CH8398:
                /* This one's also a RAMDAC */
                ClockDac = ATI_DAC_CH8398;
                break;

            case ATI_CLOCK_INTERNAL:
                /*
                 * The reference divider has already been programmed by BIOS
                 * initialisation.  Because, there is only one reference
                 * divider for all generated frequencies (including MCLK), it
                 * cannot be changed without reprogramming all clocks every
                 * time one of them needs a different reference divider.
                 *
                 * Besides, it's not a good idea to change the reference
                 * divider.  BIOS initialisation sets it to a value that
                 * effectively prevents generating frequencies beyond the
                 * graphics controller's tolerance.
                 */
                pATI->ClockDescriptor.MinM = pATI->ClockDescriptor.MaxM =
                    ATIGetMach64PLLReg(PLL_REF_DIV);

                /* The DAC is also integrated */
                if ((pATI->DAC & ~0x0FU) != ATI_DAC_INTERNAL)
                    ClockDac = ATI_DAC_INTERNAL;

                break;

            case ATI_CLOCK_ATT20C408:
                /* This one's also a RAMDAC */
                ClockDac = ATI_DAC_ATT20C408;
                break;

            case ATI_CLOCK_IBMRGB514:
                /* This one's also a RAMDAC */
                ClockDac = ATI_DAC_IBMRGB514;
                pATI->ClockNumberToProgramme = 7;
                break;

            default:
                break;
        }

        /*
         * We now have up to two indications of what RAMDAC the adapter uses.
         * They should be the same.  The following test and corresponding
         * action are under construction.
         */
        if (pATI->DAC != ClockDac)
        {
            xf86DrvMsg(pScreenInfo->scrnIndex, X_WARNING,
                       "Mach64 RAMDAC probe discrepancy detected:\n"
                       "  DAC=0x%02X;  ClockDac=0x%02X.\n",
                       pATI->DAC, ClockDac);

            if (pATI->DAC == ATI_DAC_IBMRGB514)
            {
                pATI->ProgrammableClock = ATI_CLOCK_IBMRGB514;
                pATI->ClockDescriptor =
                    ATIClockDescriptors[ATI_CLOCK_IBMRGB514];
                pATI->ClockNumberToProgramme = 7;
            }
            else
                pATI->DAC = ClockDac;   /* For now */
        }

        /* Determine panel dimensions */
        if (pATI->LCDPanelID >= 0)
        {
            LCDTable = BIOSWord(0x78U);
            if ((LCDTable + BIOSByte(LCDTable + 5)) > BIOSSize)
                LCDTable = 0;

            if (LCDTable > 0)
            {
                LCDPanelInfo = BIOSWord(LCDTable + 0x0AU);
                if (((LCDPanelInfo + 0x1DU) > BIOSSize) ||
                    ((BIOSByte(LCDPanelInfo) != pATI->LCDPanelID) &&
                     (pATI->LCDPanelID || (BIOSByte(LCDPanelInfo) > 0x1FU) ||
                      (pATI->Chip <= ATI_CHIP_264LTPRO))))
                    LCDPanelInfo = 0;
            }

            if (!LCDPanelInfo)
            {
                /*
                 * Scan BIOS for panel info table.
                 */
                for (i = 0;  i <= (int)(BIOSSize - 0x1DU);  i++)
                {
                    /* Look for panel ID ... */
                    if ((BIOSByte(i) != pATI->LCDPanelID) &&
                        (pATI->LCDPanelID || (BIOSByte(i) > 0x1FU) ||
                         (pATI->Chip <= ATI_CHIP_264LTPRO)))
                        continue;

                    /* ... followed by 24-byte panel model name ... */
                    for (j = 0;  j < 24;  j++)
                        if ((CARD8)(BIOSByte(i + j + 1) - 0x20U) > 0x5FU)
                        {
                            i += j;
                            goto NextBIOSByte;
                        }

                    /* ... verify panel width ... */
                    if (pATI->LCDHorizontal &&
                        (pATI->LCDHorizontal != BIOSWord(i + 0x19U)))
                        continue;

                    /* ... and verify panel height */
                    if (pATI->LCDVertical &&
                        (pATI->LCDVertical != BIOSWord(i + 0x1BU)))
                        continue;

                    if (LCDPanelInfo)
                    {
                        /*
                         * More than one possibility, but don't care if all
                         * tables describe panels of the same size.
                         */
                        if ((BIOSByte(LCDPanelInfo + 0x19U) ==
                             BIOSByte(i + 0x19U)) &&
                            (BIOSByte(LCDPanelInfo + 0x1AU) ==
                             BIOSByte(i + 0x1AU)) &&
                            (BIOSByte(LCDPanelInfo + 0x1BU) ==
                             BIOSByte(i + 0x1BU)) &&
                            (BIOSByte(LCDPanelInfo + 0x1CU) ==
                             BIOSByte(i + 0x1CU)))
                            continue;

                        LCDPanelInfo = 0;
                        break;
                    }

                    LCDPanelInfo = i;

            NextBIOSByte:  ;
                }
            }

            if (LCDPanelInfo > 0)
            {
                pATI->LCDPanelID = BIOSByte(LCDPanelInfo);
                pATI->LCDHorizontal = BIOSWord(LCDPanelInfo + 0x19U);
                pATI->LCDVertical = BIOSWord(LCDPanelInfo + 0x1BU);
            }
        }
    }

    ATIUnlock(pATI);            /* Unlock registers */

#ifndef AVOID_CPIO

    /* Sometimes, the BIOS lies about the chip */
    if ((pATI->Chip >= ATI_CHIP_28800_4) && (pATI->Chip <= ATI_CHIP_28800_6))
    {
        IOValue = GetBits(ATIGetExtReg(0xAAU), 0x0FU) +
            (ATI_CHIP_28800_4 - 4);
        if ((IOValue <= ATI_CHIP_28800_6) && (IOValue > pATI->Chip))
            pATI->Chip = IOValue;
    }

#endif /* AVOID_CPIO */

    /* Report what was found */
    xf86DrvMsg(pScreenInfo->scrnIndex, X_PROBED,
        "%s graphics controller detected.\n", ATIChipNames[pATI->Chip]);

#ifndef AVOID_CPIO

    if ((pATI->Chip >= ATI_CHIP_68800) && (pATI->Chip != ATI_CHIP_68800_3))

#endif /* AVOID_CPIO */

    {
        Message = Buffer + snprintf(Buffer, SizeOf(Buffer), "Chip type %04X",
            pATI->ChipType);
        if (!(pATI->ChipType & ~(CHIP_CODE_0 | CHIP_CODE_1)))
            Message += snprintf(Message, Buffer + SizeOf(Buffer) - Message,
                " (%c%c)",
                GetBits(pATI->ChipType, CHIP_CODE_1) + 0x41U,
                GetBits(pATI->ChipType, CHIP_CODE_0) + 0x41U);
        else if ((pATI->ChipType & 0x4040U) == 0x4040U)
            Message += snprintf(Message, Buffer + SizeOf(Buffer) - Message,
                " \"%c%c\"",
                GetByte(pATI->ChipType, 1), GetByte(pATI->ChipType, 0));
        if ((pATI->Chip >= ATI_CHIP_264CT) && (pATI->Chip != ATI_CHIP_Mach64))
            Message += snprintf(Message, Buffer + SizeOf(Buffer) - Message,
                ", version %d, foundry %s",
                pATI->ChipVersion, ATIFoundryNames[pATI->ChipFoundry]);
        xf86DrvMsg(pScreenInfo->scrnIndex, X_PROBED,
            "%s, class %d, revision 0x%02X.\n",
            Buffer, pATI->ChipClass, pATI->ChipRevision);
    }

#ifndef AVOID_CPIO

    if (pATI->Adapter >= ATI_ADAPTER_MACH8)

#endif /* AVOID_CPIO */

    {
        Message = Buffer + snprintf(Buffer, SizeOf(Buffer),
            "%s bus interface detected", ATIBusNames[pATI->BusType]);

#ifndef AVOID_CPIO

        if (pATI->Adapter >= ATI_ADAPTER_MACH64)

        {
            Message += snprintf(Message, Buffer + SizeOf(Buffer) - Message,
                ";  %s I/O base is 0x%04lX",
                (pATI->CPIODecoding == SPARSE_IO) ? "sparse" : "block",
                pATI->CPIOBase);
        }

#endif /* AVOID_CPIO */

        xf86DrvMsg(pScreenInfo->scrnIndex, X_PROBED, "%s.\n", Buffer);
    }

#ifndef AVOID_CPIO

    if (pATI->CPIO_VGAWonder)
        xf86DrvMsg(pScreenInfo->scrnIndex, X_PROBED,
            "VGA Wonder registers at I/O port 0x%04X.\n",
            pATI->CPIO_VGAWonder);

    if (pATI->Coprocessor != ATI_CHIP_NONE)
        xf86DrvMsg(pScreenInfo->scrnIndex, X_PROBED,
            "%s graphics accelerator detected,\n with %d kB of coprocessor"
            " memory.\n",
            ATIChipNames[pATI->Coprocessor], pATI->VideoRAM);

#endif /* AVOID_CPIO */

    xf86DrvMsg(pScreenInfo->scrnIndex, X_PROBED,
        "%s adapter detected.\n", ATIAdapterNames[pATI->Adapter]);

    if (pATI->Chip >= ATI_CHIP_264GT)
        xf86DrvMsg(pScreenInfo->scrnIndex, X_NOTICE,
            "For information on using the multimedia capabilities\n of this"
            " adapter, please see http://gatos.sf.net.\n");

    if ((pATI->DAC & ~0x0FU) == ATI_DAC_INTERNAL)
        xf86DrvMsg(pScreenInfo->scrnIndex, X_PROBED,
            "Internal RAMDAC (subtype %d) detected.\n", pATI->DAC & 0x0FU);
    else
    {
        const SymTabRec *DAC;

        for (DAC = ATIDACDescriptors;  ;  DAC++)
        {
            if (pATI->DAC == DAC->token)
            {
                xf86DrvMsg(pScreenInfo->scrnIndex, X_PROBED,
                    "%s RAMDAC detected.\n", DAC->name);
                break;
            }

            if (pATI->DAC < DAC->token)
            {
                xf86DrvMsgVerb(pScreenInfo->scrnIndex, X_WARNING, 0,
                    "Unknown RAMDAC type 0x%02X detected.\n", pATI->DAC);
                break;
            }
        }
    }

#ifdef AVOID_CPIO

    if (!xf86LinearVidMem())
    {
        xf86DrvMsg(pScreenInfo->scrnIndex, X_ERROR,
            "A linear aperture is not available.\n");
        ATILock(pATI);
        ATIPrintNoiseIfRequested(pATI, BIOS, BIOSSize);
        ATIUnmapApertures(pScreenInfo->scrnIndex, pATI);
        return FALSE;
    }

#endif /* AVOID_CPIO */

    /*
     * Set colour weights.
     */

    if (pATI->Chip < ATI_CHIP_264CT)
        pScreenInfo->rgbBits = 6;
    else
        pScreenInfo->rgbBits = 8;
    pATI->rgbBits = pScreenInfo->rgbBits;
    if (!xf86SetWeight(pScreenInfo, defaultWeight, defaultWeight))
    {
        ATILock(pATI);
        ATIPrintNoiseIfRequested(pATI, BIOS, BIOSSize);
        ATIUnmapApertures(pScreenInfo->scrnIndex, pATI);
        return FALSE;
    }

    if ((pScreenInfo->depth > 8) &&
        ((pScreenInfo->weight.red != pScreenInfo->weight.blue) ||
         (pScreenInfo->weight.red != (CARD32)(pScreenInfo->depth / 3)) ||
         ((CARD32)pScreenInfo->depth != (pScreenInfo->weight.red +
                                         pScreenInfo->weight.green +
                                         pScreenInfo->weight.blue))))
    {
        xf86DrvMsg(pScreenInfo->scrnIndex, X_ERROR,
            "Driver does not support weight %d%d%d for depth %d.\n",
            pScreenInfo->weight.red, pScreenInfo->weight.green,
            pScreenInfo->weight.blue, pScreenInfo->depth);
        ATILock(pATI);
        ATIPrintNoiseIfRequested(pATI, BIOS, BIOSSize);
        ATIUnmapApertures(pScreenInfo->scrnIndex, pATI);
        return FALSE;
    }

    /*
     * Set default visual.
     */

    if (!xf86SetDefaultVisual(pScreenInfo, -1))
    {
        ATILock(pATI);
        ATIPrintNoiseIfRequested(pATI, BIOS, BIOSSize);
        ATIUnmapApertures(pScreenInfo->scrnIndex, pATI);
        return FALSE;
    }

    if ((pScreenInfo->depth > 8) &&
        (((pScreenInfo->defaultVisual | DynamicClass) != DirectColor) ||
         ((pScreenInfo->defaultVisual == DirectColor) &&
          (pATI->DAC == ATI_DAC_INTERNAL))))
    {
        xf86DrvMsg(pScreenInfo->scrnIndex, X_ERROR,
            "Driver does not support default visual %s for depth %d.\n",
            xf86GetVisualName(pScreenInfo->defaultVisual),
            pScreenInfo->depth);
        ATILock(pATI);
        ATIPrintNoiseIfRequested(pATI, BIOS, BIOSSize);
        ATIUnmapApertures(pScreenInfo->scrnIndex, pATI);
        return FALSE;
    }

    /*
     * Set colour gamma.
     */

#ifndef AVOID_CPIO

    if (pScreenInfo->depth > 1)

#endif /* AVOID_CPIO */

    {
        if (!xf86SetGamma(pScreenInfo, defaultGamma))
        {
            ATILock(pATI);
            ATIPrintNoiseIfRequested(pATI, BIOS, BIOSSize);
            ATIUnmapApertures(pScreenInfo->scrnIndex, pATI);
            return FALSE;
        }
    }

    pATI->depth = pScreenInfo->depth;
    pATI->bitsPerPixel = pScreenInfo->bitsPerPixel;
    pATI->weight = pScreenInfo->weight;
    pATI->XModifier = pATI->bitsPerPixel / UnitOf(pATI->bitsPerPixel);

    /*
     * Determine which CRT controller to use for video modes.
     */

#ifndef AVOID_CPIO

    if ((pATI->Chip >= ATI_CHIP_88800GXC) &&
        (pATI->depth >= 8) &&
        (pATI->Chipset == ATI_CHIPSET_ATI))

#endif /* AVOID_CPIO */

    {
        pATI->NewHW.crtc = ATI_CRTC_MACH64;

        xf86DrvMsg(pScreenInfo->scrnIndex, X_INFO,
            "Using Mach64 accelerator CRTC.\n");

#ifndef AVOID_CPIO

        if (pATI->VGAAdapter != ATI_ADAPTER_NONE)
        {
            /*
             * No need for VGA I/O resources during operating state (but they
             * are still decoded).
             */
            pResources =
                xf86SetOperatingState(resVgaIo, pATI->iEntity, ResUnusedOpr);
            if (pResources)
            {
                xf86DrvMsg(pScreenInfo->scrnIndex, X_WARNING,
                    "Logic error setting operating state for VGA I/O.\n");
                xf86FreeResList(pResources);
            }

            if (pATI->CPIO_VGAWonder)
            {
                pResources = xf86SetOperatingState(pATI->VGAWonderResources,
                    pATI->iEntity, ResUnusedOpr);
                if (pResources)
                {
                    xf86DrvMsg(pScreenInfo->scrnIndex, X_WARNING,
                        "Logic error setting operating state for"
                        " VGAWonder I/O.\n");
                    xf86FreeResList(pResources);
                }
            }
        }

#endif /* AVOID_CPIO */

    }

#ifndef AVOID_CPIO

    else
    {
        pATI->NewHW.crtc = ATI_CRTC_VGA;

        xf86DrvMsg(pScreenInfo->scrnIndex, X_INFO,
            "Using VGA CRTC.\n");
    }

    /* Complain if VGA is needed but not there */
    if ((pATI->NewHW.crtc == ATI_CRTC_VGA) || !pATI->OptionLinear)
    {
        /* VGA is required at this point */
        if (pATI->VGAAdapter == ATI_ADAPTER_NONE)
        {
            xf86DrvMsg(pScreenInfo->scrnIndex, X_ERROR,
                "VGA is not available through this adapter.\n");
            ATILock(pATI);
            ATIPrintNoiseIfRequested(pATI, BIOS, BIOSSize);
            ATIUnmapApertures(pScreenInfo->scrnIndex, pATI);
            return FALSE;
        }

        if (pATI->Coprocessor != ATI_CHIP_NONE)
        {
            /* Ignore any 8514/A or Mach8 accelerator from this point on */
            pATI->Adapter = pATI->VGAAdapter;

            /* Accelerator and VGA cannot share memory */
            pATI->VideoRAM = 0;
        }
    }

#endif /* AVOID_CPIO */

    /*
     * Decide between the CRT and the panel.
     */
    if (pATI->LCDPanelID >= 0)
    {
        if (!pATI->OptionPanelDisplay)
        {
            xf86DrvMsg(pScreenInfo->scrnIndex, X_CONFIG,
                "Using CRT interface and disabling digital flat panel.\n");
        }
        else
        {
            int HDisplay, VDisplay;
            CARD8 ClockMask, PostMask;

            /*
             * Determine porch data.  The following is inaccurate (but still
             * good enough) when BIOS initialisation has set things up so that
             * the registers read here are not the ones actually in use by the
             * panel.  Thus, a further refinement here would be to flip back
             * and forth between shadow and non-shadow registers as dictated by
             * the various LCD_GEN_CNTL and CONFIG_PANEL bits involved.
             *
             * This groks the mode on entry to extract the width and position
             * of its sync and blanking pulses, and considers any overscan as
             * part of the displayed area, given that the overscan is also
             * stretched.
             *
             * This also attempts to determine panel dimensions but cannot do
             * so for one that is "auto-stretched".
             */

#ifndef AVOID_CPIO

            if (!(pATIHW->crtc_gen_cntl & CRTC_EXT_DISP_EN))
            {
                pATIHW->clock = (inb(R_GENMO) & 0x0CU) >> 2;

                pATIHW->crt[0] = GetReg(CRTX(pATI->CPIO_VGABase), 0x00U);
                pATIHW->crt[2] = GetReg(CRTX(pATI->CPIO_VGABase), 0x02U);
                pATIHW->crt[3] = GetReg(CRTX(pATI->CPIO_VGABase), 0x03U);
                pATIHW->crt[4] = GetReg(CRTX(pATI->CPIO_VGABase), 0x04U);
                pATIHW->crt[5] = GetReg(CRTX(pATI->CPIO_VGABase), 0x05U);
                pATIHW->crt[6] = GetReg(CRTX(pATI->CPIO_VGABase), 0x06U);
                pATIHW->crt[7] = GetReg(CRTX(pATI->CPIO_VGABase), 0x07U);
                pATIHW->crt[9] = GetReg(CRTX(pATI->CPIO_VGABase), 0x09U);
                pATIHW->crt[16] = GetReg(CRTX(pATI->CPIO_VGABase), 0x10U);
                pATIHW->crt[17] = GetReg(CRTX(pATI->CPIO_VGABase), 0x11U);
                pATIHW->crt[21] = GetReg(CRTX(pATI->CPIO_VGABase), 0x15U);
                pATIHW->crt[22] = GetReg(CRTX(pATI->CPIO_VGABase), 0x16U);

                pATI->LCDHSyncWidth =
                    (pATIHW->crt[5] - pATIHW->crt[4]) & 0x1FU;
                pATI->LCDHBlankWidth = (((pATIHW->crt[3] & 0x1FU) |
                                         ((pATIHW->crt[5] >> 2) & 0x20U)) -
                                        pATIHW->crt[2]) & 0x3FU;
                pATI->LCDVSyncWidth =
                    (pATIHW->crt[17] - pATIHW->crt[16]) & 0x0FU;
                pATI->LCDVBlankWidth =
                    ((pATIHW->crt[22] - pATIHW->crt[21]) & 0xFFU) + 1;

                pATI->LCDHSyncStart =
                    ((pATIHW->crt[4] - pATIHW->crt[2]) & 0xFFU) + 1;
                pATI->LCDVSyncStart = (((((pATIHW->crt[7] << 2) & 0x0200U) |
                                         ((pATIHW->crt[7] << 6) & 0x0100U) |
                                         pATIHW->crt[16]) -
                                        (((pATIHW->crt[9] << 4) & 0x0200U) |
                                         ((pATIHW->crt[7] << 5) & 0x0100U) |
                                         pATIHW->crt[21])) & 0xFFU) + 1;

                HDisplay = pATI->LCDHSyncStart + pATI->LCDHSyncWidth -
                    pATI->LCDHBlankWidth;
                if (HDisplay > 0)
                    pATI->LCDHBlankWidth += (HDisplay + 0x3FU) & ~0x3FU;
                VDisplay = pATI->LCDVSyncStart + pATI->LCDVSyncWidth -
                    pATI->LCDVBlankWidth;
                if (VDisplay > 0)
                    pATI->LCDVBlankWidth += (VDisplay + 0xFFU) & ~0xFFU;

                HDisplay = pATIHW->crt[0] + 5 - pATI->LCDHBlankWidth;
                VDisplay = (((pATIHW->crt[7] << 4) & 0x0200U) |
                            ((pATIHW->crt[7] << 8) & 0x0100U) |
                            pATIHW->crt[6]) + 3 - pATI->LCDVBlankWidth;
            }
            else

#endif /* AVOID_CPIO */

            {
                pATIHW->clock = inr(CLOCK_CNTL) & 0x03U;

                pATIHW->crtc_h_total_disp = inr(CRTC_H_TOTAL_DISP);
                pATIHW->crtc_h_sync_strt_wid = inr(CRTC_H_SYNC_STRT_WID);
                pATIHW->crtc_v_total_disp = inr(CRTC_V_TOTAL_DISP);
                pATIHW->crtc_v_sync_strt_wid = inr(CRTC_V_SYNC_STRT_WID);
                pATIHW->ovr_wid_left_right = inr(OVR_WID_LEFT_RIGHT);
                pATIHW->ovr_wid_top_bottom = inr(OVR_WID_TOP_BOTTOM);

                HDisplay = GetBits(pATIHW->crtc_h_total_disp, CRTC_H_DISP) +
                    GetBits(pATIHW->ovr_wid_left_right, OVR_WID_LEFT) +
                    GetBits(pATIHW->ovr_wid_left_right, OVR_WID_RIGHT);
                VDisplay = GetBits(pATIHW->crtc_v_total_disp, CRTC_V_DISP) +
                    GetBits(pATIHW->ovr_wid_top_bottom, OVR_WID_TOP) +
                    GetBits(pATIHW->ovr_wid_top_bottom, OVR_WID_BOTTOM);

                pATI->LCDHSyncStart =
                    (GetBits(pATIHW->crtc_h_sync_strt_wid,
                        CRTC_H_SYNC_STRT_HI) *
                     (MaxBits(CRTC_H_SYNC_STRT) + 1)) +
                    GetBits(pATIHW->crtc_h_sync_strt_wid, CRTC_H_SYNC_STRT) -
                    HDisplay;
                pATI->LCDHSyncWidth =
                    GetBits(pATIHW->crtc_h_sync_strt_wid, CRTC_H_SYNC_WID);
                pATI->LCDHBlankWidth =
                    GetBits(pATIHW->crtc_h_total_disp, CRTC_H_TOTAL) -
                    HDisplay;
                pATI->LCDVSyncStart =
                    GetBits(pATIHW->crtc_v_sync_strt_wid, CRTC_V_SYNC_STRT) -
                    VDisplay;
                pATI->LCDVSyncWidth =
                    GetBits(pATIHW->crtc_v_sync_strt_wid, CRTC_V_SYNC_WID);
                pATI->LCDVBlankWidth =
                    GetBits(pATIHW->crtc_v_total_disp, CRTC_V_TOTAL) -
                    VDisplay;

                HDisplay++;
                VDisplay++;
            }

            HDisplay <<= 3;
            pATI->LCDHSyncStart <<= 3;
            pATI->LCDHSyncWidth <<= 3;
            pATI->LCDHBlankWidth <<= 3;

            /* Calculate panel dimensions implied by the input timing */
            if ((pATIHW->horz_stretching &
                 (HORZ_STRETCH_EN | AUTO_HORZ_RATIO)) ==
                HORZ_STRETCH_EN)
            {
                if (pATIHW->horz_stretching & HORZ_STRETCH_MODE)
                {
                    if (pATIHW->horz_stretching & HORZ_STRETCH_BLEND)
                    {
                        HDisplay =
                            (HDisplay * (MaxBits(HORZ_STRETCH_BLEND) + 1)) /
                            GetBits(pATIHW->horz_stretching,
                                HORZ_STRETCH_BLEND);
                    }
                }
                else if (((pATIHW->horz_stretching & HORZ_STRETCH_LOOP) >
                          HORZ_STRETCH_LOOP15) ||
                         (pATIHW->horz_stretching &
                          SetBits(1, HORZ_STRETCH_RATIO)))
                {
                    xf86DrvMsg(pScreenInfo->scrnIndex, X_WARNING,
                        "Ignoring invalid horizontal stretch ratio in mode on"
                        " server entry.\n");
                }
                else
                {
                    IOValue =
                        GetBits(pATIHW->horz_stretching, HORZ_STRETCH_RATIO);

                    switch (GetBits(pATIHW->horz_stretching,
                                    HORZ_STRETCH_LOOP))
                    {
                        case GetBits(HORZ_STRETCH_LOOP09, HORZ_STRETCH_LOOP):
                            i = 9;
                            IOValue &= (1 << 9) - 1;
                            break;

                        case GetBits(HORZ_STRETCH_LOOP11, HORZ_STRETCH_LOOP):
                            i = 11;
                            IOValue &= (1 << 11) - 1;
                            break;

                        case GetBits(HORZ_STRETCH_LOOP12, HORZ_STRETCH_LOOP):
                            i = 12;
                            IOValue &= (1 << 12) - 1;
                            break;

                        case GetBits(HORZ_STRETCH_LOOP14, HORZ_STRETCH_LOOP):
                            i = 14;
                            IOValue &= (1 << 14) - 1;
                            break;

                        case GetBits(HORZ_STRETCH_LOOP15, HORZ_STRETCH_LOOP):
                        default:    /* Muffle compiler */
                            i = 15;
                            IOValue &= (1 << 15) - 1;
                            break;
                    }

                    if (IOValue)
                    {
                        /* Count the number of bits in IOValue */
                        j = (IOValue >> 1) & 0x36DBU;
                        j = IOValue - j - ((j >> 1) & 0x36DBU);
                        j = ((j + (j >> 3)) & 0x71C7U) % 0x3FU;

                        HDisplay = (HDisplay * i) / j;
                    }
                }
            }

            if ((pATIHW->vert_stretching & VERT_STRETCH_EN) &&
                !(pATIHW->ext_vert_stretch & AUTO_VERT_RATIO))
            {
                if ((pATIHW->vert_stretching & VERT_STRETCH_USE0) ||
                    (VDisplay <= 350))
                    IOValue =
                        GetBits(pATIHW->vert_stretching, VERT_STRETCH_RATIO0);
                else if (VDisplay <= 400)
                    IOValue =
                        GetBits(pATIHW->vert_stretching, VERT_STRETCH_RATIO1);
                else if ((VDisplay <= 480) ||
                         !(pATIHW->ext_vert_stretch & VERT_STRETCH_RATIO3))
                    IOValue =
                        GetBits(pATIHW->vert_stretching, VERT_STRETCH_RATIO2);
                else
                    IOValue =
                        GetBits(pATIHW->ext_vert_stretch, VERT_STRETCH_RATIO3);

                if (IOValue)
                    VDisplay =
                        (VDisplay * (MaxBits(VERT_STRETCH_RATIO0) + 1)) /
                        IOValue;
            }

            /* Match calculated dimensions to probed dimensions */
            if (!pATI->LCDHorizontal)
            {
                if ((pATIHW->horz_stretching &
                     (HORZ_STRETCH_EN | AUTO_HORZ_RATIO)) !=
                     (HORZ_STRETCH_EN | AUTO_HORZ_RATIO))
                    pATI->LCDHorizontal = HDisplay;
            }
            else if (pATI->LCDHorizontal != HDisplay)
            {
                if ((pATIHW->horz_stretching &
                    (HORZ_STRETCH_EN | AUTO_HORZ_RATIO)) !=
                    (HORZ_STRETCH_EN | AUTO_HORZ_RATIO))
                    xf86DrvMsg(pScreenInfo->scrnIndex, X_WARNING,
                        "Inconsistent panel horizontal dimension:"
                        "  %d and %d.\n", pATI->LCDHorizontal, HDisplay);
                HDisplay = pATI->LCDHorizontal;
            }

            if (!pATI->LCDVertical)
            {
                if (!(pATIHW->vert_stretching & VERT_STRETCH_EN) ||
                    !(pATIHW->ext_vert_stretch & AUTO_VERT_RATIO))
                    pATI->LCDVertical = VDisplay;
            }
            else if (pATI->LCDVertical != VDisplay)
            {
                if (!(pATIHW->vert_stretching & VERT_STRETCH_EN) ||
                    !(pATIHW->ext_vert_stretch & AUTO_VERT_RATIO))
                    xf86DrvMsg(pScreenInfo->scrnIndex, X_WARNING,
                        "Inconsistent panel vertical dimension:  %d and %d.\n",
                        pATI->LCDVertical, VDisplay);
                VDisplay = pATI->LCDVertical;
            }

            if (!pATI->LCDHorizontal || !pATI->LCDVertical)
            {
                if (pATI->LCDPanelID || (pATI->Chip <= ATI_CHIP_264LTPRO))
                    xf86DrvMsg(pScreenInfo->scrnIndex, X_ERROR,
                        "Unable to determine dimensions of panel (ID %d).\n",
                        pATI->LCDPanelID);
                else
                    xf86DrvMsg(pScreenInfo->scrnIndex, X_ERROR,
                        "Unable to determine dimensions of panel.\n");

                ATILock(pATI);
                ATIPrintNoiseIfRequested(pATI, BIOS, BIOSSize);
                ATIUnmapApertures(pScreenInfo->scrnIndex, pATI);
                return FALSE;
            }

            /* If the mode on entry wasn't stretched, adjust timings */
            if (!(pATIHW->horz_stretching & HORZ_STRETCH_EN) &&
                ((HDisplay = pATI->LCDHorizontal - HDisplay) > 0))
            {
                pATI->LCDHSyncStart -= HDisplay;
                if (pATI->LCDHSyncStart < 0)
                    pATI->LCDHSyncStart = 0;
                pATI->LCDHBlankWidth -= HDisplay;
                HDisplay = pATI->LCDHSyncStart + pATI->LCDHSyncWidth;
                if (pATI->LCDHBlankWidth < HDisplay)
                    pATI->LCDHBlankWidth = HDisplay;
            }

            if (!(pATIHW->vert_stretching & VERT_STRETCH_EN) &&
                ((VDisplay = pATI->LCDVertical - VDisplay) > 0))
            {
                pATI->LCDVSyncStart -= VDisplay;
                if (pATI->LCDVSyncStart < 0)
                    pATI->LCDVSyncStart = 0;
                pATI->LCDVBlankWidth -= VDisplay;
                VDisplay = pATI->LCDVSyncStart + pATI->LCDVSyncWidth;
                if (pATI->LCDVBlankWidth < VDisplay)
                    pATI->LCDVBlankWidth = VDisplay;
            }

            if (pATI->LCDPanelID || (pATI->Chip <= ATI_CHIP_264LTPRO))
                xf86DrvMsg(pScreenInfo->scrnIndex, X_PROBED,
                    "%dx%d panel (ID %d) detected.\n",
                    pATI->LCDHorizontal, pATI->LCDVertical, pATI->LCDPanelID);
            else
                xf86DrvMsg(pScreenInfo->scrnIndex, X_PROBED,
                    "%dx%d panel detected.\n",
                    pATI->LCDHorizontal, pATI->LCDVertical);

            if (LCDPanelInfo)
            {
                for (i = 0;  i < 24;  i++)
                    Buffer[i] = BIOSByte(LCDPanelInfo + 1 + i);
                for (;  --i >= 0;  )
                    if (Buffer[i] && Buffer[i] != ' ')
                    {
                        Buffer[i + 1] = '\0';
                        xf86DrvMsg(pScreenInfo->scrnIndex, X_PROBED,
                            "Panel model %s.\n", Buffer);
                        break;
                    }
            }

            /*
             * Determine panel clock.  This must be done after option
             * processing so that the adapter's reference frequency is always
             * available.
             *
             * Get post divider.  A GCC bug has caused the following expression
             * to be broken down into its individual components.
             */
            ClockMask = PLL_VCLK0_XDIV << pATIHW->clock;
            PostMask = PLL_VCLK0_POST_DIV << (pATIHW->clock * 2);
            i = GetBits(ATIGetMach64PLLReg(PLL_XCLK_CNTL), ClockMask);
            i *= MaxBits(PLL_VCLK0_POST_DIV) + 1;
            i |= GetBits(ATIGetMach64PLLReg(PLL_VCLK_POST_DIV), PostMask);

            /* Calculate clock of mode on entry */
            Numerator = ATIGetMach64PLLReg(PLL_VCLK0_FB_DIV + pATIHW->clock) *
                pATI->ReferenceNumerator;
            Denominator = pATI->ClockDescriptor.MinM *
                pATI->ReferenceDenominator *
                pATI->ClockDescriptor.PostDividers[i];
            pATI->LCDClock = ATIDivide(Numerator, Denominator, 1, 0);

            xf86DrvMsg(pScreenInfo->scrnIndex, X_PROBED,
                "Panel clock is %.3f MHz.\n",
                (double)(pATI->LCDClock) / 1000.0);

            xf86DrvMsg(pScreenInfo->scrnIndex, X_INFO,
                "Using digital flat panel interface%s.\n",
                pATI->OptionCRTDisplay ?
                    " to display on both CRT and panel" : "");
        }
    }

    /*
     * Finish detecting video RAM size.
     */
    pScreenInfo->videoRam = pATI->VideoRAM;

#ifndef AVOID_CPIO

    AcceleratorVideoRAM = pScreenInfo->videoRam;
    if (pATI->Chip == ATI_CHIP_VGA)
    {
        if (pATI->depth <= 4)
            VGAVideoRAM = 256;
        else
            VGAVideoRAM = 64;

        /* For VGA, allow a lower override */
        if ((pGDev->videoRam > 0) && (pGDev->videoRam < VGAVideoRAM))
            VGAVideoRAM = pGDev->videoRam;
    }
    else if (pATI->CPIO_VGAWonder)
    {
        /*
         * XXX There's an assumption here that the values retrieved are those
         * set by BIOS initialisation.
         */
        if (pATI->Chip <= ATI_CHIP_18800_1)
        {
            VGAVideoRAM =
                videoRamSizes[GetBits(ATIGetExtReg(0xBBU), 0x20U) + 1];
            if (AcceleratorVideoRAM > 512)
                AcceleratorVideoRAM = 512;
        }
        else
        {
            IOValue = ATIGetExtReg(0xB0U);
            if (IOValue & 0x08U)
                VGAVideoRAM = 1024;
            else if (IOValue & 0x10U)
                VGAVideoRAM = 512;
            else
                VGAVideoRAM = 256;
            if (AcceleratorVideoRAM > 1024)
                AcceleratorVideoRAM = 1024;
        }
    }

    /* Check for hardware limitations */
    if (!AcceleratorVideoRAM)
    {
        pScreenInfo->videoRam = pATI->VideoRAM = VGAVideoRAM;

        /*
         * VGA Wonder V3's, V4's and V5's don't appear to support banking in
         * planar modes.
         */
        if ((pATI->depth <= 4) &&
            (pATI->Chip <= ATI_CHIP_18800_1) &&
            (VGAVideoRAM > 256))
        {
            if (pATI->OptionDevel)
            {
                xf86DrvMsg(pScreenInfo->scrnIndex, X_NOTICE,
                    "Virtual resolutions requiring more than %s kB\n of video"
                    " memory might not function properly.\n",
                    (pATI->depth == 1) ? "64" : "256");
            }
            else
            {
                xf86DrvMsg(pScreenInfo->scrnIndex, X_NOTICE,
                    "VideoRAM reduced to 256 kB due to hardware"
                    " limitations.\n");
                pScreenInfo->videoRam = 256;
            }
        }
    }
    else if ((pATI->NewHW.crtc == ATI_CRTC_MACH64) ||
             (pATI->Chip >= ATI_CHIP_264CT))

#endif /* AVOID_CPIO */

    {

#ifndef AVOID_CPIO

        if (pATI->depth >= 8)

#endif /* AVOID_CPIO */

        {
            /* Get adapter's linear aperture configuration */
            pATIHW->config_cntl = inr(CONFIG_CNTL);
            pATI->LinearBase =
                GetBits(pATIHW->config_cntl, CFG_MEM_AP_LOC) << 22;
            if ((pATIHW->config_cntl & CFG_MEM_AP_SIZE) != CFG_MEM_AP_SIZE)
            {
                pATI->LinearSize =
                    GetBits(pATIHW->config_cntl, CFG_MEM_AP_SIZE) << 22;

                /*
                 * Linear aperture could have been disabled (but still
                 * assigned) by BIOS initialisation.
                 */
                if (pATI->LinearBase && !pATI->LinearSize)
                {
                    if ((pATI->Chip <= ATI_CHIP_88800GXD) &&
                        (pATI->VideoRAM < 4096))
                        pATI->LinearSize = 4 * 1024 * 1024;
                    else
                        pATI->LinearSize = 8 * 1024 * 1024;
                }
            }

#ifndef AVOID_CPIO

            /* Except for PCI & AGP, allow for user override */
            if (!pVideo)
            {
                if (pATI->Chip == ATI_CHIP_88800CX)
                    IOValue = ~((CARD32)((1 << 23) - 1));
                else if (pATI->Chip >= ATI_CHIP_88800GXE)
                    IOValue = ~((CARD32)((1 << 24) - 1));
                else if (pATI->VideoRAM >= 4096)
                    IOValue = ~((CARD32)((1 << 23) - 1));
                else
                    IOValue = ~((CARD32)((1 << 22) - 1));

                if ((IOValue &= pGDev->MemBase) &&
                    (IOValue <= (MaxBits(CFG_MEM_AP_LOC) << 22)))
                    pATI->LinearBase = IOValue;

                if (!pATI->LinearBase)
                    xf86DrvMsg(pScreenInfo->scrnIndex, X_WARNING,
                        "Linear aperture not configured.  Specify \"MemBase\""
                        " override in XF86Config \"Device\" section.\n");
                else
                {
                    if (!pATI->LinearSize)
                    {
                        if ((pATI->Chip <= ATI_CHIP_88800GXD) &&
                            (pATI->VideoRAM < 4096))
                            pATI->LinearSize = 4 * 1024 * 1024;
                        else
                            pATI->LinearSize = 8 * 1024 * 1024;
                    }

                    Resources[0].type = ResExcMemBlock | ResBus;
                    Resources[0].rBegin = pATI->LinearBase;
                    Resources[0].rEnd =
                        pATI->LinearBase + pATI->LinearSize - 1;
                    if (xf86RegisterResources(pATI->iEntity, Resources,
                                              ResNone))
                    {
                        xf86DrvMsg(pScreenInfo->scrnIndex, X_WARNING,
                            "Unable to register %d MB linear aperture at"
                            " 0x%08X.\n", pATI->LinearSize >> 10,
                            pATI->LinearBase);

                        pATI->LinearSize = 0;
                    }
                }
            }

#endif /* AVOID_CPIO */

            if (pATI->LinearBase && pATI->LinearSize)
            {
                /*
                 * Unless specified in PCI configuration space, set MMIO
                 * address to tail end of linear aperture.
                 */
                if (!pATI->Block0Base)
                {
                    pATI->Block0Base =
                        pATI->LinearBase + pATI->LinearSize - 0x00000400U;
                    pATI->MMIOInLinear = pATI->OptionAccel;
                }

                AcceleratorVideoRAM = pATI->LinearSize >> 10;

                /*
                 * Account for MMIO area at the tail end of the linear
                 * aperture, if it is needed or if it cannot be disabled.
                 */
                if (pATI->MMIOInLinear || (pATI->Chip < ATI_CHIP_264VTB))
                    AcceleratorVideoRAM -= 2;

                ServerVideoRAM = pATI->VideoRAM;

                if (pATI->Cursor > ATI_CURSOR_SOFTWARE)
                {
                    /*
                     * Allocate a 1 kB cursor image area at the top of the
                     * little-endian aperture, just before any MMIO area that
                     * might also be there.
                     */
                    if (ServerVideoRAM > AcceleratorVideoRAM)
                        ServerVideoRAM = AcceleratorVideoRAM;

                    ServerVideoRAM--;
                    pATI->CursorOffset = ServerVideoRAM << 10;
                    pATI->CursorBase = pATI->LinearBase + pATI->CursorOffset;

                    xf86DrvMsg(pScreenInfo->scrnIndex, X_INFO,
                        "Storing hardware cursor image at 0x%08X.\n",
                        pATI->CursorBase);
                }

#ifndef AVOID_CPIO

                if (pATI->OptionLinear)

#endif /* AVOID_CPIO */

                {
                    CARD32 PageSize = getpagesize() >> 10;

#if X_BYTE_ORDER == X_LITTLE_ENDIAN

                    /*
                     * MMIO areas must be mmap()'ed separately to avoid write
                     * combining them.  Thus, they might not end up still
                     * adjacent with the little-endian linear aperture after
                     * mmap()'ing.  So, round down the linear aperture size to
                     * avoid an overlap.  Any hardware cursor image area might
                     * not end up being write combined, but this seems
                     * preferable to further reducing the video memory size
                     * advertised to the server.
                     *
                     * XXX Ideally this should be dealt with in the os-support
                     *     layer, i.e., it should be possible to reset a
                     *     subarea's write combining after it has been
                     *     mmap()'ed, but doing so currently causes the removal
                     *     of write combining for the entire aperture.
                     */
                    if (pATI->MMIOInLinear)
                        AcceleratorVideoRAM -= AcceleratorVideoRAM % PageSize;

#else /* if X_BYTE_ORDER != X_LITTLE_ENDIAN */

                    /*
                     * Big-endian apertures are 8 MB higher and don't contain
                     * an MMIO area.
                     */
                    pATI->LinearBase += 0x00800000U;
                    AcceleratorVideoRAM = pATI->LinearSize >> 10;

#endif /* X_BYTE_ORDER */

                    if (ServerVideoRAM > AcceleratorVideoRAM)
                        ServerVideoRAM = AcceleratorVideoRAM;
                    else if (AcceleratorVideoRAM > pATI->VideoRAM)
                        AcceleratorVideoRAM = pATI->VideoRAM;

                    PageSize--;
                    AcceleratorVideoRAM =
                        (AcceleratorVideoRAM + PageSize) & ~PageSize;

                    xf86DrvMsg(pScreenInfo->scrnIndex, X_INFO,
                        "Using %d MB linear aperture at 0x%08X.\n",
                        pATI->LinearSize >> 20, pATI->LinearBase);

                    /* Only mmap what is needed */
                    ApertureSize = pATI->LinearSize =
                        AcceleratorVideoRAM << 10;
                }

                if (ServerVideoRAM < pATI->VideoRAM)
                {
                    pScreenInfo->videoRam = ServerVideoRAM;
                    xf86DrvMsg(pScreenInfo->scrnIndex, X_NOTICE,
                        "Virtual resolutions will be limited to %d kB\n due to"
                        " linear aperture size and/or placement of hardware"
                        " cursor image area.\n",
                        ServerVideoRAM);
                }
            }
        }

#ifndef AVOID_CPIO

        /* Set up for a banked aperture */
        if (pATI->VGAAdapter != ATI_ADAPTER_NONE)
        {
            pATI->UseSmallApertures = TRUE;

            /* Set banking functions */
            if (pATI->depth <= 4)
            {
                pATI->NewHW.SetBank = ATIMach64SetBankPlanar;
                pATI->BankInfo.SetSourceBank = ATIMach64SetReadPlanar;
                pATI->BankInfo.SetDestinationBank = ATIMach64SetWritePlanar;
                pATI->BankInfo.SetSourceAndDestinationBanks =
                    ATIMach64SetReadWritePlanar;
            }
            else
            {
                pATI->NewHW.SetBank = ATIMach64SetBankPacked;
                pATI->BankInfo.SetSourceBank = ATIMach64SetReadPacked;
                pATI->BankInfo.SetDestinationBank = ATIMach64SetWritePacked;
                pATI->BankInfo.SetSourceAndDestinationBanks =
                    ATIMach64SetReadWritePacked;
            }

            /*
             * Unless specified in PCI configuration space, or at the top of
             * of a little-endian linear aperture, set MMIO address to the one
             * just above the VGA aperture.  This does not work on the CT
             * (maybe others).
             */
            if (!pATI->Block0Base &&
                ((pATI->Chip < ATI_CHIP_264CT) ||
                 (pATI->Chip >= ATI_CHIP_264VT) ||
                 pATI->OptionDevel))
                pATI->Block0Base = 0x000BFC00U;
        }

        if (!pATI->OptionLinear)
            pATI->LinearBase = 0;       /* Not needed */

#endif /* AVOID_CPIO */

        if (!pATI->LinearBase || !pATI->LinearSize)
        {

#ifndef AVOID_CPIO

            if (pATI->VGAAdapter == ATI_ADAPTER_NONE)

#endif /* AVOID_CPIO */

            {
                xf86DrvMsg(pScreenInfo->scrnIndex, X_ERROR,
                    "Linear aperture not available.\n");
                ATILock(pATI);
                ATIPrintNoiseIfRequested(pATI, BIOS, BIOSSize);
                ATIUnmapApertures(pScreenInfo->scrnIndex, pATI);
                return FALSE;
            }

#ifndef AVOID_CPIO

            /* Insurance */
            pATI->LinearBase = pATI->LinearSize = 0;

#endif /* AVOID_CPIO */

        }

#ifndef AVOID_CPIO

        if (!pATI->OptionAccel)
        {
            pATI->Block0Base = 0;       /* Not needed */
            pATI->MMIOInLinear = FALSE;
        }
        else

#endif

        if (pATI->Block0Base)
        {
            xf86DrvMsg(pScreenInfo->scrnIndex, X_INFO,
                "Using Block 0 MMIO aperture at 0x%08X.\n", pATI->Block0Base);

            /* Set Block1 MMIO address if supported */
            if (pATI->Chip >= ATI_CHIP_264VT)
            {
                pATI->Block1Base = pATI->Block0Base - 0x00000400U;
                xf86DrvMsg(pScreenInfo->scrnIndex, X_INFO,
                    "Using Block 1 MMIO aperture at 0x%08X.\n",
                    pATI->Block1Base);
            }
        }
    }

#ifndef AVOID_CPIO

    else
    /*
     * After BIOS initialisation, the accelerator (if any) and the VGA won't
     * necessarily agree on the amount of video memory, depending on whether or
     * where the memory boundary is configured.  Any discrepancy will be
     * resolved by ATIModePreInit().
     *
     * However, it's possible that there is more video memory than VGA Wonder
     * can architecturally handle.
     */
    if (((pATI->Chip < ATI_CHIP_68800) || (pATI->Chip > ATI_CHIP_68800AX)) &&
        (AcceleratorVideoRAM < pScreenInfo->videoRam))
    {
        if (pATI->OptionDevel)
        {
            if (pATI->depth == 1)
                AcceleratorVideoRAM /= 4;

            xf86DrvMsg(pScreenInfo->scrnIndex, X_NOTICE,
                "Virtual resolutions requiring more than %d kB\n of video"
                " memory might not function correctly.\n",
                AcceleratorVideoRAM);
        }
        else
        {
            xf86DrvMsg(pScreenInfo->scrnIndex, X_NOTICE,
                "VideoRAM reduced to %d kB due to hardware limitations.\n",
                AcceleratorVideoRAM);

            pScreenInfo->videoRam = AcceleratorVideoRAM;
        }
    }

    if (pATI->OptionLinear)
    {
        if (!pATI->LinearBase)
        {
            xf86DrvMsg(pScreenInfo->scrnIndex, X_WARNING,
                "Linear aperture not supported in this configuration.\n");
            pATI->OptionLinear = FALSE;
        }
        else
        {
            if (pATI->VGAAdapter != ATI_ADAPTER_NONE)
            {
                /*
                 * Free VGA memory aperture during operating state (but it is
                 * still decoded).
                 */
                pResources = xf86SetOperatingState(resVgaMem,
                    pATI->iEntity, ResUnusedOpr);
                if (pResources)
                {
                    xf86DrvMsg(pScreenInfo->scrnIndex, X_WARNING,
                        "Logic error setting operating state for VGA memory"
                        " aperture.\n");
                    xf86FreeResList(pResources);
                }
            }
        }
    }

#endif /* AVOID_CPIO */

    if ((pATI->Cursor > ATI_CURSOR_SOFTWARE) && !pATI->CursorBase)
    {
        xf86DrvMsg(pScreenInfo->scrnIndex, X_WARNING,
            "Unable to store hardware cursor image.  Reverting to software"
            " cursor.\n");
        pATI->Cursor = ATI_CURSOR_SOFTWARE;
    }

    /*
     * Remap apertures.  Must lock and re-unlock around this in case the
     * remapping fails.
     */
    ATILock(pATI);
    ATIUnmapApertures(pScreenInfo->scrnIndex, pATI);
    if (!ATIMapApertures(pScreenInfo->scrnIndex, pATI))
        return FALSE;

    ATIUnlock(pATI);

    if (pATI->OptionAccel)
    {

#ifndef AVOID_CPIO

        if (!pATI->Block0Base || (pATI->NewHW.crtc == ATI_CRTC_VGA))
        {
            xf86DrvMsg(pScreenInfo->scrnIndex, X_WARNING,
                "Acceleration not supported in this configuration.\n");
            pATI->OptionAccel = FALSE;
        }
        else

#endif /* AVOID_CPIO */

        {
            xf86DrvMsg(pScreenInfo->scrnIndex, X_INFO,
                "MMIO write caching %sabled.\n",
                pATI->OptionMMIOCache ? "en" : "dis");
        }
    }

#ifndef AVOID_CPIO

    if (pATI->Adapter >= ATI_ADAPTER_MACH32)

#endif /* AVOID_CPIO */

    {
        if (pATI->Chip >= ATI_CHIP_264CT)
            ATIReportMemory(pScreenInfo, pATI,
                ATIMemoryTypeNames_264xT[pATI->MemoryType]);
        else if (pATI->Chip == ATI_CHIP_88800CX)
            ATIReportMemory(pScreenInfo, pATI,
                ATIMemoryTypeNames_88800CX[pATI->MemoryType]);
        else
            ATIReportMemory(pScreenInfo, pATI,
                ATIMemoryTypeNames_Mach[pATI->MemoryType]);
    }

#ifndef AVOID_CPIO

    else if (pATI->Adapter >= ATI_ADAPTER_V3)
        ATIReportMemory(pScreenInfo, pATI,
            (ATIGetExtReg(0xB7U) & 0x04U) ? "DRAM" : "VRAM");
    else
        ATIReportMemory(pScreenInfo, pATI, "video memory");

#endif /* AVOID_CPIO */

    /*
     * Finish banking setup.  This needs to be fixed to not assume the mode on
     * entry is a VGA mode.  XXX
     */

#ifndef AVOID_CPIO

    if (pATI->VGAAdapter == ATI_ADAPTER_NONE)

#endif /* AVOID_CPIO */

    {
        pATIHW->crtc = pATI->NewHW.crtc;

#ifndef AVOID_CPIO

        pATIHW->SetBank = (ATIBankProcPtr)NoopDDA;
        pATI->BankInfo.BankSize = 0;            /* No banking */

#endif /* AVOID_CPIO */

    }

#ifndef AVOID_CPIO

    else
    {
        pATIHW->crtc = ATI_CRTC_VGA;
#if 0 /* ___NOT_YET___ */
        if (pATI->ChipHasSUBSYS_CNTL)
        {
        }
        else
#endif
        if ((pATI->Chip >= ATI_CHIP_88800GXC) &&
            (pATI->LockData.crtc_gen_cntl & CRTC_EXT_DISP_EN))
            pATIHW->crtc = ATI_CRTC_MACH64;

        if (pATI->depth <= 4)
        {
            pATI->BankInfo.nBankDepth = 1;
            pATI->NewHW.nPlane = 4;
        }
        else
        {
            pATI->BankInfo.nBankDepth = pATI->depth;
            pATI->NewHW.nPlane = 1;
        }

        if ((pATIHW->crtc != ATI_CRTC_VGA) || (GetReg(SEQX, 0x04U) & 0x08U))
            pATIHW->nPlane = 1;
        else
            pATIHW->nPlane = 4;

        pATIHW->nBank = ATIDivide(pATI->VideoRAM,
            pATIHW->nPlane * pATI->BankInfo.BankSize, 10, 1);
        pATI->NewHW.nBank = ATIDivide(pATI->VideoRAM,
            pATI->NewHW.nPlane * pATI->BankInfo.BankSize, 10, 1);

        if (pATI->VGAAdapter == ATI_ADAPTER_VGA)
        {
            pATIHW->SetBank = pATI->NewHW.SetBank =
                (ATIBankProcPtr)NoopDDA;
            pATIHW->nBank = pATI->NewHW.nBank = 1;
        }
        else if (!pATI->UseSmallApertures)
            pATIHW->SetBank = pATI->NewHW.SetBank;
        else if ((pATIHW->crtc == ATI_CRTC_VGA) &&
                 !(pATI->LockData.config_cntl & CFG_MEM_VGA_AP_EN))
        {
            pATIHW->SetBank = (ATIBankProcPtr)NoopDDA;
            pATIHW->nBank = 1;
        }
        else if (pATIHW->nPlane == 1)
            pATIHW->SetBank = ATIMach64SetBankPacked;
        else
            pATIHW->SetBank = ATIMach64SetBankPlanar;

        if (((ApertureSize * pATI->depth) / pATI->BankInfo.nBankDepth) >=
            (unsigned)(pScreenInfo->videoRam * 1024))
            pATI->BankInfo.BankSize = 0;        /* No banking */
    }

#endif /* AVOID_CPIO */

    if (pATI->OptionShadowFB)
    {
        /* Until ShadowFB becomes a true screen wrapper, if it ever does... */

#ifndef AVOID_CPIO

        if (pATI->BankInfo.BankSize)
        {
            xf86DrvMsg(pScreenInfo->scrnIndex, X_WARNING,
                "Cannot shadow a banked frame buffer.\n");
            pATI->OptionShadowFB = FALSE;
        }
        else if (pATI->depth < 8)
        {
            xf86DrvMsg(pScreenInfo->scrnIndex, X_WARNING,
                "Cannot shadow a planar frame buffer.\n");
            pATI->OptionShadowFB = FALSE;
        }
        else

#endif /* AVOID_CPIO */

        if (pATI->OptionAccel)
        {
            xf86DrvMsg(pScreenInfo->scrnIndex, X_WARNING,
                "Cannot shadow an accelerated frame buffer.\n");
            pATI->OptionShadowFB = FALSE;
        }
        else
            xf86DrvMsg(pScreenInfo->scrnIndex, X_INFO,
                "Using shadow frame buffer.\n");
    }

    /* 264VT-B's and later have DSP registers */
    if ((pATI->Chip >= ATI_CHIP_264VTB) &&
        !ATIDSPPreInit(pScreenInfo->scrnIndex, pATI))
    {
        ATILock(pATI);
        ATIPrintNoiseIfRequested(pATI, BIOS, BIOSSize);
        ATIUnmapApertures(pScreenInfo->scrnIndex, pATI);
        return FALSE;
    }

    /*
     * Determine minClock and maxClock.  For adapters with supported
     * programmable clock generators, start with an absolute maximum.
     */
    if (pATI->ClockDescriptor.MaxN > 0)
    {
        Numerator = pATI->ClockDescriptor.MaxN * pATI->ReferenceNumerator;
        Denominator = pATI->ClockDescriptor.MinM * pATI->ReferenceDenominator *
            pATI->ClockDescriptor.PostDividers[0];

        /*
         * An integrated PLL behaves as though the reference frequency were
         * doubled.  It also does not appear to care about the colour depth.
         */
        if (pATI->ProgrammableClock == ATI_CLOCK_INTERNAL)
            Numerator <<= 1;
        else if (pATI->depth > 8)
            Denominator *= (pATI->bitsPerPixel / 8);

        ATIClockRange.maxClock = (Numerator / (Denominator * 1000)) * 1000;

        Numerator = pATI->ClockDescriptor.MinN * pATI->ReferenceNumerator;
        Denominator = pATI->ClockDescriptor.MaxM * pATI->ReferenceDenominator *
            pATI->ClockDescriptor.PostDividers[pATI->ClockDescriptor.NumD - 1];

        if (pATI->ProgrammableClock == ATI_CLOCK_INTERNAL)
            Numerator <<= 1;

        ATIClockRange.minClock = (Numerator / (Denominator * 1000)) * 1000;

        if (pATI->XCLKFeedbackDivider)
        {
            /* Possibly reduce maxClock due to memory bandwidth */
            Numerator = pATI->XCLKFeedbackDivider * 2 *
                pATI->ReferenceNumerator;
            Denominator = pATI->ClockDescriptor.MinM *
                pATI->XCLKReferenceDivider * pATI->ReferenceDenominator;

#ifndef AVOID_CPIO

            if (pATI->depth >= 8)

#endif /* AVOID_CPIO */

            {
                Denominator *= pATI->bitsPerPixel / 4;
            }

            i = (6 - 2) - pATI->XCLKPostDivider;

#ifndef AVOID_CPIO

            if (pATI->NewHW.crtc == ATI_CRTC_VGA)
               i--;

#endif /* AVOID_CPIO */

            i = (ATIDivide(Numerator, Denominator, i, -1) / 1000) * 1000;
            if (i < ATIClockRange.maxClock)
                ATIClockRange.maxClock = i;
        }
    }

    /*
     * Assume an internal DAC can handle whatever frequency the internal PLL
     * can produce (with the reference divider set by BIOS initialisation), but
     * default maxClock to a lower chip-specific default.
     */
    if ((pATI->DAC & ~0x0FU) == ATI_DAC_INTERNAL)
    {
        int DacSpeed;
        switch (pATI->bitsPerPixel)
        {
            case 15:
            case 16:
                DacSpeed = pGDev->dacSpeeds[DAC_BPP16];
                break;

            case 24:
                DacSpeed = pGDev->dacSpeeds[DAC_BPP24];
                break;

            case 32:
                DacSpeed = pGDev->dacSpeeds[DAC_BPP32];
                break;

            default:
                DacSpeed = 0;
                break;
        }
        if (!DacSpeed)
            DacSpeed = pGDev->dacSpeeds[DAC_BPP8];
        if (DacSpeed < ATIClockRange.maxClock)
        {
            DefaultmaxClock = 135000;

            if (pATI->depth > 8)
                DefaultmaxClock = 80000;

            if ((pATI->Chip >= ATI_CHIP_264VTB) &&
                (pATI->Chip != ATI_CHIP_Mach64))
            {
                if ((pATI->Chip >= ATI_CHIP_264VT4) &&
                    (pATI->Chip != ATI_CHIP_264LTPRO))
                    DefaultmaxClock = 230000;
                else if (pATI->Chip >= ATI_CHIP_264VT3)
                    DefaultmaxClock = 200000;
                else
                    DefaultmaxClock = 170000;
            }
            if (DacSpeed > DefaultmaxClock)
                ATIClockRange.maxClock = DacSpeed;
            else if (DefaultmaxClock < ATIClockRange.maxClock)
                ATIClockRange.maxClock = DefaultmaxClock;
        }
    }
    else
    {
        switch(pATI->DAC)
        {
            case ATI_DAC_STG1700:
            case ATI_DAC_STG1702:
            case ATI_DAC_STG1703:
                DefaultmaxClock = 110000;
                break;

            case ATI_DAC_IBMRGB514:
                pATI->maxClock = 220000;

#ifndef AVOID_CPIO

                if (pATI->NewHW.crtc == ATI_CRTC_VGA)
                    DefaultmaxClock = 100000;
                else

#endif /* AVOID_CPIO */

                    DefaultmaxClock = 220000;
                break;

            default:

#ifndef AVOID_CPIO

                /*
                 * 80 MHz is too high in some cases.  Limit 18800-x's to 40
                 * MHz.  Don't exceed the memory clock on VGA Wonder capables
                 * with less than 1 MB, if using a packed mode.
                 */
                if ((pATI->Chip == ATI_CHIP_18800) ||
                    (pATI->Chip == ATI_CHIP_18800_1))
                    DefaultmaxClock = 40000;
                else if (pATI->CPIO_VGAWonder &&
                        (pATI->VideoRAM < 1024) &&
                        (pATI->depth >= 8))
                    DefaultmaxClock =
                        (GetBits(BIOSByte(0x44U), 0x04U) * 5000) + 40000;
                else

#endif /* AVOID_CPIO */

                {
                    DefaultmaxClock = 80000;
                }

                break;
        }

        if (DefaultmaxClock < ATIClockRange.maxClock)
            ATIClockRange.maxClock = DefaultmaxClock;
    }

    if (pATI->ClockDescriptor.MaxN <= 0)
    {
        ATIClockRange.maxClock = DefaultmaxClock;
        xf86DrvMsg(pScreenInfo->scrnIndex, X_INFO,
            "Maximum pixel clock:  %.3f MHz.\n",
            (double)ATIClockRange.maxClock / 1000.0);
    }

    /*
     * Determine available pixel clock frequencies.
     */

    ATIClockPreInit(pScreenInfo, pATI, pGDev, &ATIClockRange);
    if (pATI->ProgrammableClock > ATI_CLOCK_FIXED)
        Strategy = LOOKUP_BEST_REFRESH;

    /*
     * Mode validation.
     */

#ifdef AVOID_CPIO

    if (pATI->Chip >= ATI_CHIP_264CT)
        minPitch = 8;

#else /* AVOID_CPIO */

    if ((pATI->depth >= 8) && (pATI->Chip >= ATI_CHIP_264CT))
        minPitch = 8;
    else if (pATI->CPIO_VGAWonder &&
             (pATI->Chip <= ATI_CHIP_18800_1) &&
             (pATI->VideoRAM == 256) &&
             (pATI->depth >= 8))
    {
        minPitch = 32;          /* Very strange, but true */
        maxPitch = 0x3FU;
    }

#endif /* AVOID_CPIO */

    else
        minPitch = 16;

    pATI->pitchInc = minPitch;

#ifndef AVOID_CPIO

    if (pATI->depth >= 8)

#endif /* AVOID_CPIO */

    {
        pATI->pitchInc *= pATI->bitsPerPixel;
    }

    switch (pATI->NewHW.crtc)
    {

#ifndef AVOID_CPIO

        case ATI_CRTC_VGA:
            /*
             * IBM's VGA doesn't allow for interlaced modes.
             */
            if (pATI->Adapter <= ATI_ADAPTER_VGA)
                ATIClockRange.interlaceAllowed = FALSE;

            pScreenInfo->maxHValue = (0xFFU + 1) << 3;  /* max HTotal */

            /*
             * The maximum VTotal value set here applies to all modes,
             * including interlaced, doublescanned or multiscanned modes.
             * Finer-grained checks are done in ATIValidateMode().
             */
            pScreenInfo->maxVValue = 0x03FFU + 1;
            if (pATI->Adapter > ATI_ADAPTER_VGA)
            {
                pScreenInfo->maxVValue <<= 1;
                if (ATIClockRange.interlaceAllowed &&
                    (pATI->Chip < ATI_CHIP_264CT))
                    pScreenInfo->maxVValue <<= 1;
            }

            /*
             * 18800-x and 28800-x do not support interlaced modes when the
             * scanline pitch is 2048 pixels or more.  For 18800-x's with 256
             * kB of video memory, the limit for 8bpp is 1024.
             */
            if (ATIClockRange.interlaceAllowed &&
                (pATI->Chip <= ATI_CHIP_28800_6))
            {
                if (minPitch == 32)
                    pATI->MaximumInterlacedPitch = 0x1FU * 32;
                else
                    pATI->MaximumInterlacedPitch = 0x7FU * minPitch;
            }

            Strategy |= LOOKUP_CLKDIV2;

            break;

#endif /* AVOID_CPIO */

        case ATI_CRTC_MACH64:
            pScreenInfo->maxHValue = (MaxBits(CRTC_H_TOTAL) + 1) << 3;

            if (pATI->Chip < ATI_CHIP_264VT)
            {
                /*
                 * ATI finally fixed accelerated doublescanning in the 264VT
                 * and later.  On 88800's, the bit is documented to exist, but
                 * only doubles the vertical timings.  On the 264CT and 264ET,
                 * the bit is ignored.
                 */
                ATIClockRange.doubleScanAllowed = FALSE;

                /* CRTC_H_TOTAL is one bit narrower */
                pScreenInfo->maxHValue >>= 1;
            }

            pScreenInfo->maxVValue = MaxBits(CRTC_V_TOTAL) + 1;

            maxPitch = MaxBits(CRTC_PITCH);

            break;

        default:
            break;
    }

    maxPitch *= minPitch;

    if (pATI->OptionAccel)
    {
        /*
         * Set engine restrictions on coordinate space.  Use maxPitch for the
         * horizontal and maxHeight for the vertical.
         */
        if (maxPitch > (ATIMach64MaxX / pATI->XModifier))
            maxPitch = ATIMach64MaxX / pATI->XModifier;
        maxHeight = ATIMach64MaxY;

        /*
         * For SGRAM & WRAM adapters, the display engine limits the pitch to
         * multiples of 64 bytes.
         */
        if ((pATI->Chip >= ATI_CHIP_264CT) &&
            ((pATI->Chip >= ATI_CHIP_264VTB) ||
             (pATI->MemoryType >= MEM_264_SGRAM)))
            pATI->pitchInc = pATI->XModifier * (64 * 8);
    }

    if (pATI->OptionPanelDisplay && (pATI->LCDPanelID >= 0))
    {
        /*
         * Given LCD modes are more tightly controlled than CRT modes, allow
         * the user the option of not specifying a panel's horizontal sync
         * and/or vertical refresh tolerances.
         */
        Strategy |= LOOKUP_OPTIONAL_TOLERANCES;

        /*
         * Add a mode to the end of the monitor's list for the panel's native
         * resolution.
         */
        pMode = (DisplayModePtr)xnfcalloc(1, SizeOf(DisplayModeRec));
        pMode->name = "Native panel mode";
        pMode->type = M_T_BUILTIN;
        pMode->Clock = pATI->LCDClock;
        pMode->HDisplay = pATI->LCDHorizontal;
        pMode->VDisplay = pATI->LCDVertical;

        /*
         * These timings are bogus, but enough to survive sync tolerance
         * checks.
         */
        pMode->HSyncStart = pMode->HDisplay;
        pMode->HSyncEnd = pMode->HSyncStart + minPitch;
        pMode->HTotal = pMode->HSyncEnd + minPitch;
        pMode->VSyncStart = pMode->VDisplay;
        pMode->VSyncEnd = pMode->VSyncStart + 1;
        pMode->VTotal = pMode->VSyncEnd + 1;

        pMode->CrtcHDisplay = pMode->HDisplay;
        pMode->CrtcHBlankStart = pMode->HDisplay;
        pMode->CrtcHSyncStart = pMode->HSyncStart;
        pMode->CrtcHSyncEnd = pMode->HSyncEnd;
        pMode->CrtcHBlankEnd = pMode->HTotal;
        pMode->CrtcHTotal = pMode->HTotal;

        pMode->CrtcVDisplay = pMode->VDisplay;
        pMode->CrtcVBlankStart = pMode->VDisplay;
        pMode->CrtcVSyncStart = pMode->VSyncStart;
        pMode->CrtcVSyncEnd = pMode->VSyncEnd;
        pMode->CrtcVBlankEnd = pMode->VTotal;
        pMode->CrtcVTotal = pMode->VTotal;

        if (!pScreenInfo->monitor->Modes)
            pScreenInfo->monitor->Modes = pMode;
        else
        {
            pScreenInfo->monitor->Last->next = pMode;
            pMode->prev = pScreenInfo->monitor->Last;
        }

        pScreenInfo->monitor->Last = pMode;

        /*
         * Defeat Xconfigurator brain damage.  Ignore all HorizSync and
         * VertRefresh specifications.  For now, this does not take
         * SYNC_TOLERANCE into account.
         */
        if (pScreenInfo->monitor->nHsync > 0)
        {
            double hsync = (double)pMode->Clock /
                           (pATI->LCDHorizontal + pATI->LCDHBlankWidth);

            for (i = 0;  ;  i++)
            {
                if (i >= pScreenInfo->monitor->nHsync)
                {
                    xf86DrvMsg(pScreenInfo->scrnIndex, X_NOTICE,
                        "Conflicting XF86Config HorizSync specification(s)"
                        " ignored.\n");
                    break;
                }

                if ((hsync >= pScreenInfo->monitor->hsync[i].lo) &&
                    (hsync <= pScreenInfo->monitor->hsync[i].hi))
                {
                    xf86DrvMsg(pScreenInfo->scrnIndex, X_WARNING,
                        "Extraneous XF86Config HorizSync specification(s)"
                        " ignored.\n");
                    break;
                }
            }

            pScreenInfo->monitor->nHsync = 0;
        }

        if (pScreenInfo->monitor->nVrefresh > 0)
        {
            double vrefresh = ((double)pMode->Clock * 1000.0) /
                              ((pATI->LCDHorizontal + pATI->LCDHBlankWidth) *
                               (pATI->LCDVertical + pATI->LCDVBlankWidth));

            for (i = 0;  ;  i++)
            {
                if (i >= pScreenInfo->monitor->nVrefresh)
                {
                    xf86DrvMsg(pScreenInfo->scrnIndex, X_NOTICE,
                        "Conflicting XF86Config VertRefresh specification(s)"
                        " ignored.\n");
                    break;
                }

                if ((vrefresh >= pScreenInfo->monitor->vrefresh[i].lo) &&
                    (vrefresh <= pScreenInfo->monitor->vrefresh[i].hi))
                {
                    xf86DrvMsg(pScreenInfo->scrnIndex, X_WARNING,
                        "Extraneous XF86Config VertRefresh specification(s)"
                        " ignored.\n");
                    break;
                }
            }

            pScreenInfo->monitor->nVrefresh = 0;
        }
    }

    i = xf86ValidateModes(pScreenInfo,
            pScreenInfo->monitor->Modes, pScreenInfo->display->modes,
            &ATIClockRange, NULL, minPitch, maxPitch,
            pATI->pitchInc, 0, maxHeight,
            pScreenInfo->display->virtualX, pScreenInfo->display->virtualY,
            ApertureSize, Strategy);
    if (i <= 0)
    {
        ATILock(pATI);
        ATIPrintNoiseIfRequested(pATI, BIOS, BIOSSize);
        ATIUnmapApertures(pScreenInfo->scrnIndex, pATI);
        return FALSE;
    }

    /* Remove invalid modes */
    xf86PruneDriverModes(pScreenInfo);

    /* Set current mode to the first in the list */
    pScreenInfo->currentMode = pScreenInfo->modes;

    /* Print mode list */
    xf86PrintModes(pScreenInfo);

    /* Set display resolution */
    xf86SetDpi(pScreenInfo, 0, 0);

    /* Load required modules */
    if (!ATILoadModules(pScreenInfo, pATI))
    {
        ATILock(pATI);
        ATIPrintNoiseIfRequested(pATI, BIOS, BIOSSize);
        ATIUnmapApertures(pScreenInfo->scrnIndex, pATI);
        return FALSE;
    }

    pATI->displayWidth = pScreenInfo->displayWidth;

    /* Initialise for panning */
    ATIAdjustPreInit(pATI);

    /*
     * Warn about modes that are too small, or not aligned, to scroll to the
     * bottom right corner of the virtual screen.
     */
    MinX = pScreenInfo->virtualX - pATI->AdjustMaxX;
    MinY = pScreenInfo->virtualY - pATI->AdjustMaxY;

    pMode = pScreenInfo->modes;
    do
    {
        if ((pMode->VDisplay <= MinY) &&
            ((pMode->VDisplay < MinY) || (pMode->HDisplay < MinX)))
            xf86DrvMsg(pScreenInfo->scrnIndex, X_WARNING,
                "Mode \"%s\" too small to scroll to bottom right corner of"
                " virtual resolution.\n", pMode->name);
        else if ((pMode->HDisplay & ~pATI->AdjustMask) / pScreenInfo->xInc)
            xf86DrvMsg(pScreenInfo->scrnIndex, X_WARNING,
                "Mode \"%s\" cannot scroll to bottom right corner of virtual"
                " resolution.\n Horizontal dimension not a multiple of %d.\n",
                pMode->name, ~pATI->AdjustMask + 1);
    } while ((pMode = pMode->next) != pScreenInfo->modes);

    /* Initialise CRTC code */
    ATIModePreInit(pScreenInfo, pATI, &pATI->NewHW);

    if (!pScreenInfo->chipset || !*pScreenInfo->chipset)
        pScreenInfo->chipset = (char *)ATIChipsetNames[0];

    ATILock(pATI);
    ATIPrintNoiseIfRequested(pATI, BIOS, BIOSSize);
    ATIUnmapApertures(pScreenInfo->scrnIndex, pATI);

    return TRUE;
}

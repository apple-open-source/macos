/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/ati/atiprobe.c,v 1.54 2003/01/01 19:16:33 tsi Exp $ */
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
#include "atiadapter.h"
#include "atiadjust.h"
#include "atibus.h"
#include "atichip.h"
#include "aticonsole.h"
#include "atiident.h"
#include "atimach64io.h"
#include "atimodule.h"
#include "atipreinit.h"
#include "atiprobe.h"
#include "atiscreen.h"
#include "ativalid.h"
#include "ativersion.h"
#include "atividmem.h"
#include "atiwonderio.h"

#include "radeon_probe.h"
#include "radeon_version.h"
#include "r128_probe.h"
#include "r128_version.h"

/*
 * NOTES:
 *
 * - The driver private structures (ATIRec's) are allocated here, rather than
 *   in ATIPreInit().  This allows ATIProbe() to pass information to later
 *   stages.
 * - A minor point, perhaps, is that XF86Config Chipset names denote functional
 *   levels, rather than specific graphics controller chips.
 * - ATIProbe() does not call xf86MatchPciInstances(), because ATIProbe()
 *   should be able to match a mix of PCI and non-PCI devices to XF86Config
 *   Device sections.  Also, PCI configuration space for Mach32's is to be
 *   largely ignored.
 */

#ifdef XFree86LOADER

/*
 * The following exists to prevent the compiler from considering entry points
 * defined in a separate module from being constants.
 */
static xf86PreInitProc     * const volatile PreInitProc     = ATIPreInit;
static xf86ScreenInitProc  * const volatile ScreenInitProc  = ATIScreenInit;
static xf86SwitchModeProc  * const volatile SwitchModeProc  = ATISwitchMode;
static xf86AdjustFrameProc * const volatile AdjustFrameProc = ATIAdjustFrame;
static xf86EnterVTProc     * const volatile EnterVTProc     = ATIEnterVT;
static xf86LeaveVTProc     * const volatile LeaveVTProc     = ATILeaveVT;
static xf86FreeScreenProc  * const volatile FreeScreenProc  = ATIFreeScreen;
static xf86ValidModeProc   * const volatile ValidModeProc   = ATIValidMode;

#define ATIPreInit     PreInitProc
#define ATIScreenInit  ScreenInitProc
#define ATISwitchMode  SwitchModeProc
#define ATIAdjustFrame AdjustFrameProc
#define ATIEnterVT     EnterVTProc
#define ATILeaveVT     LeaveVTProc
#define ATIFreeScreen  FreeScreenProc
#define ATIValidMode   ValidModeProc

#endif

/* Used as a temporary buffer */
#define Identifier ((char *)(pATI->MMIOCache))

/*
 * An internal structure definition to facilitate the matching of detected
 * adapters to XF86Config Device sections.
 */
typedef struct _ATIGDev
{
    GDevPtr pGDev;
    int     iATIPtr;
    CARD8   Chipset;
} ATIGDev, *ATIGDevPtr;

#ifndef AVOID_CPIO

/*
 * Definitions for I/O conflict avoidance.
 */
#define LongPort(_Port) GetBits(_Port, PCIGETIO(SPARSE_IO_BASE))
#define DetectedVGA    (1 << 0)
#define Detected8514A  (1 << 1)
#define DetectedMach64 (1 << 2)
#define Allowed        (1 << 3)
#define DoProbe        (1 << 4)
typedef struct
{
    IOADDRESS Base;
    CARD8     Size;
    CARD8     Flag;
} PortRec, *PortPtr;

/*
 * ATIScanPCIBases --
 *
 * This function loops though a device's PCI registered bases and accumulates
 * a list of block I/O bases in use in the system.
 */
static void
ATIScanPCIBases
(
    PortPtr      *PCIPorts,
    int          *nPCIPort,
    const CARD32 *pBase,
    const int    *pSize,
    const CARD8  ProbeFlag
)
{
    IOADDRESS Base;
    int       i, j;

    for (i = 6;  --i >= 0;  pBase++, pSize++)
    {
        if (*pBase & PCI_MAP_IO)
        {
            Base = *pBase & ~IO_BYTE_SELECT;
            for (j = 0;  ;  j++)
            {
                if (j >= *nPCIPort)
                {
                    (*nPCIPort)++;
                    *PCIPorts = (PortPtr)xnfrealloc(*PCIPorts,
                        *nPCIPort * SizeOf(PortRec));
                    (*PCIPorts)[j].Base = Base;
                    (*PCIPorts)[j].Size = (CARD8)*pSize;
                    (*PCIPorts)[j].Flag = ProbeFlag;
                    break;
                }

                if (Base == (*PCIPorts)[j].Base)
                    break;
            }

            continue;
        }

        /* Allow for 64-bit addresses */
        if (!PCI_MAP_IS64BITMEM(*pBase))
            continue;

        i--;
        pBase++;
        pSize++;
    }
}

/*
 * ATICheckSparseIOBases --
 *
 * This function checks whether a sparse I/O base can safely be probed.
 */
static CARD8
ATICheckSparseIOBases
(
    pciVideoPtr     pVideo,
    CARD8           *ProbeFlags,
    const IOADDRESS IOBase,
    const int       Count,
    const Bool      Override
)
{
    CARD32 FirstPort, LastPort;

    if (!pVideo || !xf86IsPrimaryPci(pVideo))
    {
        FirstPort = LongPort(IOBase);
        LastPort  = LongPort(IOBase + Count - 1);

        for (;  FirstPort <= LastPort;  FirstPort++)
        {
            CARD8 ProbeFlag = ProbeFlags[FirstPort];

            if (ProbeFlag & DoProbe)
                continue;

            if (!(ProbeFlag & Allowed))
                return ProbeFlag;

            if (Override)
                continue;

            /* User might wish to override this decision */
            xf86Msg(X_WARNING,
                ATI_NAME ":  Sparse I/O base 0x%04X not probed.\n", IOBase);
            return Allowed;
        }
    }

    return DoProbe;
}

/*
 * ATIClaimSparseIOBases --
 *
 * This function updates the sparse I/O base table with information from the
 * hardware probes.
 */
static void
ATIClaimSparseIOBases
(
    CARD8           *ProbeFlags,
    const IOADDRESS IOBase,
    const int       Count,
    const CARD8     ProbeFlag
)
{
    CARD32 FirstPort = LongPort(IOBase),
           LastPort  = LongPort(IOBase + Count - 1);

    for (;  FirstPort <= LastPort;  FirstPort++)
        ProbeFlags[FirstPort] = ProbeFlag;
}

/*
 * ATIVGAProbe --
 *
 * This function looks for an IBM standard VGA, or clone, and sets
 * pATI->VGAAdapter if one is found.
 */
static ATIPtr
ATIVGAProbe
(
    ATIPtr pVGA
)
{
    CARD8 IOValue1, IOValue2, IOValue3;

    if (!pVGA)
        pVGA = (ATIPtr)xnfcalloc(1, SizeOf(ATIRec));

    /*
     * VGA has one more attribute register than EGA.  See if it can be read and
     * written.  Note that the CRTC registers are not used here, so there's no
     * need to unlock them.
     */
    ATISetVGAIOBase(pVGA, inb(R_GENMO));
    (void)inb(GENS1(pVGA->CPIO_VGABase));
    IOValue1 = inb(ATTRX);
    (void)inb(GENS1(pVGA->CPIO_VGABase));
    IOValue2 = GetReg(ATTRX, 0x14U | 0x20U);
    outb(ATTRX, IOValue2 ^ 0x0FU);
    IOValue3 = GetReg(ATTRX, 0x14U | 0x20U);
    outb(ATTRX, IOValue2);
    outb(ATTRX, IOValue1);
    (void)inb(GENS1(pVGA->CPIO_VGABase));
    if (IOValue3 == (IOValue2 ^ 0x0FU))
    {
        /* VGA device detected */
        if (pVGA->Chip == ATI_CHIP_NONE)
            pVGA->Chip = ATI_CHIP_VGA;
        if (pVGA->VGAAdapter == ATI_ADAPTER_NONE)
            pVGA->VGAAdapter = ATI_ADAPTER_VGA;
        if (pVGA->Adapter == ATI_ADAPTER_NONE)
            pVGA->Adapter = ATI_ADAPTER_VGA;
    }
    else
        pVGA->VGAAdapter = ATI_ADAPTER_NONE;

    return pVGA;
}

/*
 * ATIVGAWonderProbe --
 *
 * This function determines if ATI extended VGA registers can be accessed
 * through the I/O port specified by pATI->CPIO_VGAWonder.  If not, the
 * function resets pATI->CPIO_VGAWonder to zero.
 */
static void
ATIVGAWonderProbe
(
    pciVideoPtr pVideo,
    ATIPtr      pATI,
    ATIPtr      p8514,
    CARD8       *ProbeFlags
)
{
    CARD8 IOValue1, IOValue2, IOValue3, IOValue4, IOValue5, IOValue6;

    switch (ATICheckSparseIOBases(pVideo, ProbeFlags,
        pATI->CPIO_VGAWonder, 2, TRUE))
    {
        case 0:
            xf86Msg(X_WARNING,
                ATI_NAME ":  Expected VGA Wonder capability could not be"
                " detected at I/O port 0x%04X because it would conflict with"
                " a non-video PCI/AGP device.\n", pATI->CPIO_VGAWonder);
            pATI->CPIO_VGAWonder = 0;
            break;

        case Detected8514A:
            xf86Msg(X_WARNING,
                ATI_NAME ":  Expected VGA Wonder capability could not be"
                " detected at I/O port 0x%04X because it would conflict with"
                " a %s %s.\n", pATI->CPIO_VGAWonder,
                ATIBusNames[p8514->BusType], ATIAdapterNames[p8514->Adapter]);
            pATI->CPIO_VGAWonder = 0;
            break;

        case DetectedMach64:
            xf86Msg(X_WARNING,
                ATI_NAME ":  Expected VGA Wonder capability could not be"
                " detected at I/O port 0x%04X because it would conflict with"
                " a Mach64.\n", pATI->CPIO_VGAWonder);
            pATI->CPIO_VGAWonder = 0;
            break;

        case DetectedVGA:
        default:                /* Must be DoProbe */
            if (pVideo && !xf86IsPrimaryPci(pVideo) &&
                (pATI->Chip <= ATI_CHIP_88800GXD))
            {
                /* Set up extended VGA register addressing */
                PutReg(GRAX, 0x50U, GetByte(pATI->CPIO_VGAWonder, 0));
                PutReg(GRAX, 0x51U,
                    GetByte(pATI->CPIO_VGAWonder, 1) | pATI->VGAOffset);
            }
            /*
             * Register 0xBB is used by the BIOS to keep track of various
             * things (monitor type, etc.).  Except for 18800-x's, register
             * 0xBC must be zero and causes the adapter to enter a test mode
             * when written to with a non-zero value.
             */
            IOValue1 = inb(pATI->CPIO_VGAWonder);
            IOValue2 = ATIGetExtReg(IOValue1);
            IOValue3 = ATIGetExtReg(0xBBU);
            ATIPutExtReg(0xBBU, IOValue3 ^ 0xAAU);
            IOValue4 = ATIGetExtReg(0xBBU);
            ATIPutExtReg(0xBBU, IOValue3 ^ 0x55U);
            IOValue5 = ATIGetExtReg(0xBBU);
            ATIPutExtReg(0xBBU, IOValue3);
            if (pATI->Chip <= ATI_CHIP_18800_1)
                IOValue6 = 0;
            else
                IOValue6 = ATIGetExtReg(0xBCU);
            ATIPutExtReg(IOValue1, IOValue2);

            if ((IOValue4 == (IOValue3 ^ 0xAAU)) &&
                (IOValue5 == (IOValue3 ^ 0x55U)) &&
                (IOValue6 == 0))
            {
                xf86MsgVerb(X_INFO, 3,
                    ATI_NAME ":  VGA Wonder at I/O port 0x%04X detected.\n",
                    pATI->CPIO_VGAWonder);
            }
            else
            {
                xf86Msg(X_WARNING,
                    ATI_NAME ":  Expected VGA Wonder capability at I/O port"
                    " 0x%04X was not detected.\n", pATI->CPIO_VGAWonder);
                pATI->CPIO_VGAWonder = 0;
            }
            break;
    }
}

/*
 * ATI8514Probe --
 *
 * This function looks for an 8514/A compatible and returns an ATIRec if one is
 * found.  The function also determines whether or not the detected 8514/A
 * compatible device is actually a Mach8 or Mach32, and sets pATI->Adapter
 * accordingly.
 */
static ATIPtr
ATI8514Probe
(
    pciVideoPtr pVideo
)
{
    ATIPtr pATI = NULL;
    CARD16 IOValue1, IOValue2;

    /*
     * Save register value to be modified, just in case there is no 8514/A
     * compatible accelerator.  Note that, in more ways than one,
     * SUBSYS_STAT == SUBSYS_CNTL.
     */
    IOValue1 = inw(SUBSYS_STAT);
    IOValue2 = IOValue1 & _8PLANE;

    /* Reset any 8514/A compatible adapter that might be present */
    outw(SUBSYS_CNTL, IOValue2 | (GPCTRL_RESET | CHPTEST_NORMAL));
    outw(SUBSYS_CNTL, IOValue2 | (GPCTRL_ENAB | CHPTEST_NORMAL |
        RVBLNKFLG | RPICKFLAG | RINVALIDIO | RGPIDLE));

    /* Probe for an 8514/A compatible */
    IOValue2 = inw(ERR_TERM);
    outw(ERR_TERM, 0x5A5AU);
    ProbeWaitIdleEmpty();
    if (inw(ERR_TERM) == 0x5A5AU)
    {
        outw(ERR_TERM, 0x2525U);
        if (inw(ERR_TERM) == 0x2525U)
        {
            pATI = (ATIPtr)xnfcalloc(1, SizeOf(ATIRec));
            pATI->Adapter = ATI_ADAPTER_8514A;
            pATI->ChipHasSUBSYS_CNTL = TRUE;
            pATI->PCIInfo = pVideo;
        }
    }
    outw(ERR_TERM, IOValue2);

    /* Restore register value clobbered by 8514/A reset attempt */
    if (!pATI)
    {
        outw(SUBSYS_CNTL, IOValue1);
        return NULL;
    }

    /* Ensure any Mach8 or Mach32 is not in 8514/A emulation mode */
    IOValue1 = inw(CLOCK_SEL);
    outw(CLOCK_SEL, IOValue1);
    ProbeWaitIdleEmpty();

    IOValue1 = IOValue2 = inw(ROM_ADDR_1);
    outw(ROM_ADDR_1, 0x5555U);
    ProbeWaitIdleEmpty();
    if (inw(ROM_ADDR_1) == 0x5555U)
    {
        outw(ROM_ADDR_1, 0x2A2AU);
        ProbeWaitIdleEmpty();
        if (inw(ROM_ADDR_1) == 0x2A2AU)
            pATI->Adapter = ATI_ADAPTER_MACH8;
    }
    outw(ROM_ADDR_1, IOValue1);

    if (pATI->Adapter == ATI_ADAPTER_MACH8)
    {
        /* A Mach8 or Mach32 has been detected */
        IOValue1 = inw(READ_SRC_X);
        outw(DESTX_DIASTP, 0xAAAAU);
        ProbeWaitIdleEmpty();
        if (inw(READ_SRC_X) == 0x02AAU)
            pATI->Adapter = ATI_ADAPTER_MACH32;

        outw(DESTX_DIASTP, 0x5555U);
        ProbeWaitIdleEmpty();
        if (inw(READ_SRC_X) == 0x0555U)
        {
            if (pATI->Adapter != ATI_ADAPTER_MACH32)
                pATI->Adapter = ATI_ADAPTER_8514A;
        }
        else
        {
            if (pATI->Adapter != ATI_ADAPTER_MACH8)
                pATI->Adapter = ATI_ADAPTER_8514A;
        }
        outw(DESTX_DIASTP, IOValue1);
    }

    switch (pATI->Adapter)
    {
        case ATI_ADAPTER_8514A:
            pATI->Coprocessor = ATI_CHIP_8514A;
            IOValue1 = inb(EXT_CONFIG_3);
            outb(EXT_CONFIG_3, IOValue1 & 0x0FU);
            if (!(inb(EXT_CONFIG_3) & 0xF0U))
            {
                outb(EXT_CONFIG_3, IOValue1 | 0xF0U);
                if ((inb(EXT_CONFIG_3) & 0xF0U) == 0xF0U)
                    pATI->Coprocessor = ATI_CHIP_CT480;
            }
            outb(EXT_CONFIG_3, IOValue1);
            break;

        case ATI_ADAPTER_MACH8:
            pATI->Coprocessor = ATI_CHIP_38800_1;
            if (inw(CONFIG_STATUS_1) & MC_BUS)
                pATI->BusType = ATI_BUS_MCA16;
            break;

        case ATI_ADAPTER_MACH32:
            IOValue1 = inw(CONFIG_STATUS_1);
            pATI->BusType = GetBits(IOValue1, BUS_TYPE);
            pATI->BIOSBase = 0x000C0000U +
                (GetBits(IOValue2, BIOS_BASE_SEGMENT) << 11);
            if (!(IOValue1 & (_8514_ONLY | CHIP_DIS)))
            {
                pATI->VGAAdapter = ATI_ADAPTER_MACH32;
                if ((xf86ReadBIOS(pATI->BIOSBase, 0x10U,
                         (pointer)(&pATI->CPIO_VGAWonder),
                         SizeOf(pATI->CPIO_VGAWonder)) <
                         SizeOf(pATI->CPIO_VGAWonder)) ||
                    !(pATI->CPIO_VGAWonder &= SPARSE_IO_PORT))
                    pATI->CPIO_VGAWonder = 0x01CEU;
                pATI->VGAOffset = 0x80U;
            }

            ATIMach32ChipID(pATI);
            break;

        default:
            break;
    }

    return pATI;
}

#endif /* AVOID_CPIO */

/*
 * ATIDetectMach64 --
 *
 * This function determines if a Mach64 is detectable at a particular base
 * address.
 */
static Bool
ATIDetectMach64
(
    ATIPtr            pATI,
    const CARD16      ChipType,
    const ATIChipType Chip
)
{
    CARD32 IOValue, bus_cntl, gen_test_cntl;

    (void)ATIMapApertures(-1, pATI);    /* Ignore errors */

#ifdef AVOID_CPIO

    if (!pATI->pBlock[0])
    {
        ATIUnmapApertures(-1, pATI);
        return FALSE;
    }

#endif /* AVOID_CPIO */

    /* Make sure any Mach64 is not in some weird state */
    bus_cntl = inr(BUS_CNTL);
    if (Chip < ATI_CHIP_264VTB)
        outr(BUS_CNTL,
             (bus_cntl & ~(BUS_HOST_ERR_INT_EN | BUS_FIFO_ERR_INT_EN)) |
             (BUS_HOST_ERR_INT | BUS_FIFO_ERR_INT));
    else if (Chip < ATI_CHIP_264VT4)
        outr(BUS_CNTL, (bus_cntl & ~BUS_HOST_ERR_INT_EN) | BUS_HOST_ERR_INT);

    gen_test_cntl = inr(GEN_TEST_CNTL);
    IOValue = gen_test_cntl &
        (GEN_OVR_OUTPUT_EN | GEN_OVR_POLARITY | GEN_CUR_EN | GEN_BLOCK_WR_EN);
    outr(GEN_TEST_CNTL, IOValue | GEN_GUI_EN);
    outr(GEN_TEST_CNTL, IOValue);
    outr(GEN_TEST_CNTL, IOValue | GEN_GUI_EN);

    /* See if a Mach64 answers */
    IOValue = inr(SCRATCH_REG0);

    /* Test odd bits */
    outr(SCRATCH_REG0, 0x55555555U);
    if (inr(SCRATCH_REG0) == 0x55555555U)
    {
        /* Test even bits */
        outr(SCRATCH_REG0, 0xAAAAAAAAU);
        if (inr(SCRATCH_REG0) == 0xAAAAAAAAU)
        {
            /*
             * *Something* has a R/W 32-bit register at this address.  Try to
             * make sure it's a Mach64.  The following assumes that ATI will
             * not be producing any more adapters that do not register
             * themselves in PCI configuration space.
             */
            ATIMach64ChipID(pATI, ChipType);
            if ((pATI->Chip != ATI_CHIP_Mach64) ||
                (pATI->CPIODecoding == BLOCK_IO))
                pATI->Adapter = ATI_ADAPTER_MACH64;
        }
    }

    /* Restore clobbered register value */
    outr(SCRATCH_REG0, IOValue);

    /* If no Mach64 was detected, return now */
    if (pATI->Adapter != ATI_ADAPTER_MACH64)
    {
        outr(GEN_TEST_CNTL, gen_test_cntl);
        outr(BUS_CNTL, bus_cntl);
        ATIUnmapApertures(-1, pATI);
        return FALSE;
    }

    /* Determine legacy BIOS address */
    pATI->BIOSBase = 0x000C0000U +
        (GetBits(inr(SCRATCH_REG1), BIOS_BASE_SEGMENT) << 11);

    ATIUnmapApertures(-1, pATI);
    pATI->PCIInfo = NULL;
    return TRUE;
}

#ifdef AVOID_CPIO

/*
 * ATIMach64Probe --
 *
 * This function looks for a Mach64 at a particular MMIO address and returns an
 * ATIRec if one is found.
 */
static ATIPtr
ATIMach64Probe
(
    pciVideoPtr       pVideo,
    const IOADDRESS   IOBase,
    const CARD8       IODecoding,
    const ATIChipType Chip
)
{
    ATIPtr pATI     = (ATIPtr)xnfcalloc(1, SizeOf(ATIRec));
    CARD16 ChipType = 0;

    pATI->CPIOBase = IOBase;
    pATI->CPIODecoding = IODecoding;

    if (pVideo)
    {
        pATI->PCIInfo = pVideo;
        ChipType = pVideo->chipType;

        /*
         * Probe through auxiliary MMIO aperture if one exists.  Because such
         * apertures can be enabled/disabled only through PCI, this probes no
         * further.
         */
        if ((pVideo->size[2] >= 12) &&
            (pATI->Block0Base = pVideo->memBase[2]) &&
            (pATI->Block0Base < (CARD32)(-1 << pVideo->size[2])))
        {
            pATI->Block0Base += 0x00000400U;
            goto LastProbe;
        }

        /*
         * Probe through the primary MMIO aperture that exists at the tail end
         * of the linear aperture.  Test for both 8MB and 4MB linear apertures.
         */
        if ((pVideo->size[0] >= 22) && (pATI->Block0Base = pVideo->memBase[0]))
        {
            pATI->Block0Base += 0x007FFC00U;
            if ((pVideo->size[0] >= 23) &&
                ATIDetectMach64(pATI, ChipType, Chip))
                return pATI;

            pATI->Block0Base -= 0x00400000U;
            if (ATIDetectMach64(pATI, ChipType, Chip))
                return pATI;
        }
    }

    /*
     * A last, perhaps desparate, probe attempt.  Note that if this succeeds,
     * there's a VGA in the system and it's likely the PIO version of the
     * driver should be used instead (barring OS issues).
     */
    pATI->Block0Base = 0x000BFC00U;

LastProbe:
    if (ATIDetectMach64(pATI, ChipType, Chip))
        return pATI;

    xfree(pATI);
    return NULL;
}

#else /* AVOID_CPIO */

/*
 * ATIMach64Probe --
 *
 * This function looks for a Mach64 at a particular PIO address and returns an
 * ATIRec if one is found.
 */
static ATIPtr
ATIMach64Probe
(
    pciVideoPtr       pVideo,
    const IOADDRESS   IOBase,
    const CARD8       IODecoding,
    const ATIChipType Chip
)
{
    ATIPtr pATI;
    CARD32 IOValue;
    CARD16 ChipType = 0;

    if (!IOBase)
        return NULL;

    if (pVideo)
    {
        if ((IODecoding == BLOCK_IO) &&
            ((pVideo->size[1] < 8) ||
             (IOBase >= (CARD32)(-1 << pVideo->size[1]))))
            return NULL;

        ChipType = pVideo->chipType;
    }

    pATI = (ATIPtr)xnfcalloc(1, SizeOf(ATIRec));
    pATI->CPIOBase = IOBase;
    pATI->CPIODecoding = IODecoding;
    pATI->PCIInfo = pVideo;

    if (!ATIDetectMach64(pATI, ChipType, Chip))
    {
        xfree(pATI);
        return NULL;
    }

    /*
     * Determine VGA capability.  VGA can always be enabled on integrated
     * controllers.  For the GX/CX, it's a board strap.
     */
    if (pATI->Chip >= ATI_CHIP_264CT)
        pATI->VGAAdapter = ATI_ADAPTER_MACH64;
    else
    {
        IOValue = inr(CONFIG_STATUS64_0);
        pATI->BusType = GetBits(IOValue, CFG_BUS_TYPE);
        IOValue &= (CFG_VGA_EN | CFG_CHIP_EN);
        if (pATI->Chip == ATI_CHIP_88800CX)
            IOValue |= CFG_VGA_EN;
        if (IOValue == (CFG_VGA_EN | CFG_CHIP_EN))
        {
            pATI->VGAAdapter = ATI_ADAPTER_MACH64;
            pATI->CPIO_VGAWonder = 0x01CEU;
            pATI->VGAOffset = 0x80U;
        }
    }

    return pATI;
}

/*
 * ATIAssignVGA --
 *
 * This function is called to associate a VGA interface with an accelerator.
 * This is done by temporarily configuring the accelerator to route VGA RAMDAC
 * I/O through the accelerator's RAMDAC.  A value is then written through the
 * VGA DAC ports and a check is made to see if the same value shows up on the
 * accelerator side.
 */
static void
ATIAssignVGA
(
    pciVideoPtr pVideo,
    ATIPtr      *ppVGA,
    ATIPtr      pATI,
    ATIPtr      p8514,
    CARD8       *ProbeFlags
)
{
    ATIPtr pVGA = *ppVGA;
    CARD8  OldDACMask;

    /* Assume unassignable VGA */
    pATI->VGAAdapter = ATI_ADAPTER_NONE;

    /* If no assignable VGA, return now */
    if ((pATI != pVGA) && (!pVGA || (pVGA->Adapter > ATI_ADAPTER_VGA)))
        return;

    switch (pATI->Adapter)
    {
        case ATI_ADAPTER_8514A:
            {
                /*
                 * Assumption:  Bit DISABPASSTHRU in ADVFUNC_CNTL is already
                 *              off.
                 */
                OldDACMask = inb(VGA_DAC_MASK);

                if (inb(IBM_DAC_MASK) == OldDACMask)
                {
                    outb(VGA_DAC_MASK, 0xA5U);
                    if (inb(IBM_DAC_MASK) == 0xA5U)
                        pATI->VGAAdapter = ATI_ADAPTER_VGA;
                }

                outb(VGA_DAC_MASK, OldDACMask);
            }
            break;

        case ATI_ADAPTER_MACH8:
            {
                CARD16 ClockSel = inw(CLOCK_SEL);

                if (ClockSel & DISABPASSTHRU)
                    outw(CLOCK_SEL, ClockSel & ~DISABPASSTHRU);

                ProbeWaitIdleEmpty();

                OldDACMask = inb(VGA_DAC_MASK);

                if (inb(IBM_DAC_MASK) == OldDACMask)
                {
                    outb(VGA_DAC_MASK, 0xA5U);
                    if (inb(IBM_DAC_MASK) == 0xA5U)
                        pATI->VGAAdapter = ATI_ADAPTER_VGA;
                }

                outb(VGA_DAC_MASK, OldDACMask);

                if (ClockSel & DISABPASSTHRU)
                    outw(CLOCK_SEL, ClockSel);
            }
            break;

        case ATI_ADAPTER_MACH32:
            {
                CARD16 ClockSel    = inw(CLOCK_SEL),
                       MiscOptions = inw(MISC_OPTIONS);

                if (ClockSel & DISABPASSTHRU)
                    outw(CLOCK_SEL, ClockSel & ~DISABPASSTHRU);
                if (MiscOptions & (DISABLE_VGA | DISABLE_DAC))
                    outw(MISC_OPTIONS,
                        MiscOptions & ~(DISABLE_VGA | DISABLE_DAC));

                ProbeWaitIdleEmpty();

                OldDACMask = inb(VGA_DAC_MASK);

                if (inb(IBM_DAC_MASK) == OldDACMask)
                {
                    outb(VGA_DAC_MASK, 0xA5U);
                    if (inb(IBM_DAC_MASK) == 0xA5U)
                        pATI->VGAAdapter = ATI_ADAPTER_MACH32;
                }

                outb(VGA_DAC_MASK, OldDACMask);

                if (ClockSel & DISABPASSTHRU)
                    outw(CLOCK_SEL, ClockSel);
                if (MiscOptions & (DISABLE_VGA | DISABLE_DAC))
                    outw(MISC_OPTIONS, MiscOptions);
            }
            break;

        case ATI_ADAPTER_MACH64:
            {
                CARD32 DACCntl = inr(DAC_CNTL);

                if (!(DACCntl & DAC_VGA_ADR_EN))
                    outr(DAC_CNTL, DACCntl | DAC_VGA_ADR_EN);

                OldDACMask = inb(VGA_DAC_MASK);

                if (in8(M64_DAC_MASK) == OldDACMask)
                {
                    outb(VGA_DAC_MASK, 0xA5U);
                    if (in8(M64_DAC_MASK) == 0xA5U)
                        pATI->VGAAdapter = ATI_ADAPTER_MACH64;
                }

                outb(VGA_DAC_MASK, OldDACMask);

                if (!(DACCntl & DAC_VGA_ADR_EN))
                    outr(DAC_CNTL, DACCntl);
            }
            break;

        default:
            break;
    }

    if (pATI->VGAAdapter == ATI_ADAPTER_NONE)
    {
        pATI->CPIO_VGAWonder = 0;
        return;
    }

    if (pATI->CPIO_VGAWonder)
    {
        ATIVGAWonderProbe(pVideo, pATI, p8514, ProbeFlags);
        if (!pATI->CPIO_VGAWonder)
        {
            /*
             * Some adapters are reputed to append ATI extended VGA registers
             * to the VGA Graphics controller registers.  In particular, 0x01CE
             * cannot, in general, be used in a PCI environment due to routing
             * of I/O through the bus tree.
             */
            pATI->CPIO_VGAWonder = GRAX;
            ATIVGAWonderProbe(pVideo, pATI, p8514, ProbeFlags);
        }
    }

    if (pATI == pVGA)
    {
        pATI->SharedVGA = TRUE;
        return;
    }

    /* Assign the VGA to this adapter */
    xfree(pVGA);
    *ppVGA = pATI;

    xf86MsgVerb(X_INFO, 3, ATI_NAME ":  VGA assigned to this adapter.\n");
}

/*
 * ATIClaimVGA --
 *
 * Attempt to assign a non-shareable VGA to an accelerator.  If successful,
 * update ProbeFlags array.
 */
static void
ATIClaimVGA
(
    pciVideoPtr pVideo,
    ATIPtr      *ppVGA,
    ATIPtr      pATI,
    ATIPtr      p8514,
    CARD8       *ProbeFlags,
    int         Detected
)
{
    ATIAssignVGA(pVideo, ppVGA, pATI, p8514, ProbeFlags);
    if (pATI->VGAAdapter == ATI_ADAPTER_NONE)
        return;

    ATIClaimSparseIOBases(ProbeFlags, MonochromeIOBase, 48, Detected);
    if (!pATI->CPIO_VGAWonder)
        return;

    ATIClaimSparseIOBases(ProbeFlags, pATI->CPIO_VGAWonder, 2, Detected);
}

/*
 * ATIFindVGA --
 *
 * This function determines if a VGA associated with an ATI PCI adapter is
 * shareable.
 */
static void
ATIFindVGA
(
    pciVideoPtr pVideo,
    ATIPtr      *ppVGA,
    ATIPtr      *ppATI,
    ATIPtr      p8514,
    CARD8       *ProbeFlags
)
{
    ATIPtr pATI = *ppATI;

    if (!*ppVGA)
    {
        /*
         * An ATI PCI adapter has been detected at this point, and its VGA, if
         * any, is shareable.  Ensure the VGA isn't in sleep mode.
         */
        outb(GENENA, 0x16U);
        outb(GENVS, 0x01U);
        outb(GENENA, 0x0EU);

        pATI = ATIVGAProbe(pATI);
        if (pATI->VGAAdapter == ATI_ADAPTER_NONE)
            return;

        ppVGA = ppATI;
    }

    ATIAssignVGA(pVideo, ppVGA, pATI, p8514, ProbeFlags);
}

#endif /* AVOID_CPIO */

/*
 * ATIProbe --
 *
 * This function is called once, at the start of the first server generation to
 * do a minimal probe for supported hardware.
 */
Bool
ATIProbe
(
    DriverPtr pDriver,
    int       flags
)
{
    ATIPtr                 pATI, *ATIPtrs = NULL;
    GDevPtr                *GDevs, pGDev;
    pciVideoPtr            pVideo, *xf86PciVideoInfo = xf86GetPciVideoInfo();
    pciConfigPtr           pPCI;
    ATIGDev                *ATIGDevs = NULL, *pATIGDev;
    ScrnInfoPtr            pScreenInfo;
    CARD32                 PciReg;
    Bool                   ProbeSuccess = FALSE;
    Bool                   DoRage128 = FALSE, DoRadeon = FALSE;
    int                    i, j, k;
    int                    nGDev, nATIGDev = -1, nATIPtr = 0;
    int                    Chipset;
    ATIChipType            Chip;

#ifndef AVOID_CPIO

    ATIPtr                 pVGA = NULL, p8514 = NULL;
    ATIPtr                 pMach64[3] = {NULL, NULL, NULL};
    pciConfigPtr           *xf86PciInfo = xf86GetPciConfigInfo();
    PortPtr                PCIPorts = NULL;
    int                    nPCIPort = 0;
    CARD8                  fChipsets[ATI_CHIPSET_MAX];
    static const IOADDRESS Mach64SparseIOBases[] = {0x02ECU, 0x01CCU, 0x01C8U};
    CARD8                  ProbeFlags[LongPort(SPARSE_IO_BASE) + 1];

    unsigned long          BIOSBase;
    static const CARD8     ATISignature[] = " 761295520";
#   define                 SignatureSize 10
#   define                 PrefixSize    0x50U
#   define                 BIOSSignature 0x30U
    CARD8                  BIOS[PrefixSize];
#   define                 BIOSWord(_n)  (BIOS[_n] | (BIOS[(_n) + 1] << 8))

#endif /* AVOID_CPIO */

#   define                 AddAdapter(_p)                                  \
    do                                                                     \
    {                                                                      \
        nATIPtr++;                                                         \
        ATIPtrs = (ATIPtr *)xnfrealloc(ATIPtrs, SizeOf(ATIPtr) * nATIPtr); \
        ATIPtrs[nATIPtr - 1] = (_p);                                       \
        (_p)->iEntity = -2;                                                \
    } while (0)

#ifndef AVOID_CPIO

    (void)memset(fChipsets, FALSE, SizeOf(fChipsets));

#endif /* AVOID_CPIO */

    if (!(flags & PROBE_DETECT))
    {
        /*
         * Get a list of XF86Config device sections whose "Driver" is either
         * not specified, or specified as this driver.  From this list,
         * eliminate those device sections that specify a "Chipset" or a
         * "ChipID" not recognised by the driver.  Those device sections that
         * specify a "ChipRev" without a "ChipID" are also weeded out.
         */
        nATIGDev = 0;
        if ((nGDev = xf86MatchDevice(ATI_NAME, &GDevs)) > 0)
        {
            ATIGDevs = (ATIGDevPtr)xnfcalloc(nGDev, SizeOf(ATIGDev));

            for (i = 0, pATIGDev = ATIGDevs;  i < nGDev;  i++)
            {
                pGDev = GDevs[i];
                Chipset = ATIIdentProbe(pGDev->chipset);
                if (Chipset == -1)
                    continue;

                if ((pGDev->chipID > (int)((CARD16)(-1))) ||
                    (pGDev->chipRev > (int)((CARD8)(-1))))
                    continue;

                if (pGDev->chipID >= 0)
                {
                    if (ATIChipID(pGDev->chipID, 0) == ATI_CHIP_Mach64)
                        continue;
                }
                else
                {
                    if (pGDev->chipRev >= 0)
                        continue;
                }

                pATIGDev->pGDev = pGDev;
                pATIGDev->Chipset = Chipset;
                nATIGDev++;
                pATIGDev++;

                xf86MsgVerb(X_INFO, 3,
                    ATI_NAME ":  Candidate \"Device\" section \"%s\".\n",
                    pGDev->identifier);

#ifndef AVOID_CPIO

                fChipsets[Chipset] = TRUE;

#endif /* AVOID_CPIO */

            }

            xfree(GDevs);

            if (!nATIGDev)
            {
                xfree(ATIGDevs);
                ATIGDevs = NULL;
            }
        }

        if (xf86MatchDevice(R128_NAME, NULL) > 0)
            DoRage128 = TRUE;
        if (xf86MatchDevice(RADEON_NAME, NULL) > 0)
            DoRadeon = TRUE;
    }

#ifndef AVOID_CPIO

    /*
     * Collect hardware information.  This must be done with care to avoid
     * lockups due to overlapping I/O port assignments.
     *
     * First, scan PCI configuration space for registered I/O ports (which will
     * be block I/O bases).  Each such port is used to generate a list of
     * sparse I/O bases it precludes.  This list is then used to decide whether
     * or not certain sparse I/O probes are done.  Unfortunately, this assumes
     * that any registered I/O base actually reserves upto the full 256 ports
     * allowed by the PCI specification.  This assumption holds true for PCI
     * Mach64, but probably doesn't for other device types.  For some things,
     * such as video devices, the number of ports a base represents is
     * determined by the server's PCI probe, but, for other devices, this
     * cannot be done by a user-level process without jeopardizing system
     * integrity.  This information should ideally be retrieved from the OS's
     * own PCI probe (if any), but there's currently no portable way of doing
     * so.  The following allows sparse I/O probes to be forced in certain
     * circumstances when an appropriate chipset specification is used in any
     * XF86Config Device section.
     *
     * Note that this is not bullet-proof.  Lockups can still occur, but they
     * will usually be due to devices that are misconfigured to respond to the
     * same I/O ports as 8514/A's or ATI sparse I/O devices without registering
     * them in PCI configuration space.
     */
    if (nATIGDev)
    {
        if (xf86PciVideoInfo)
        {
            for (i = 0;  (pVideo = xf86PciVideoInfo[i++]);  )
            {
                if (pVideo->vendor == PCI_VENDOR_ATI)
                    continue;

                pPCI = pVideo->thisCard;

                ATIScanPCIBases(&PCIPorts, &nPCIPort,
                    &pPCI->pci_base0, pVideo->size,
                    (pciReadLong(pPCI->tag, PCI_CMD_STAT_REG) &
                     PCI_CMD_IO_ENABLE) ? 0 : Allowed);
            }
        }

        /* Check non-video PCI devices for I/O bases */
        if (xf86PciInfo)
        {
            for (i = 0;  (pPCI = xf86PciInfo[i++]);  )
            {
                if ((pPCI->pci_vendor == PCI_VENDOR_ATI) ||
                    (pPCI->pci_base_class == PCI_CLASS_BRIDGE) ||
                    (pPCI->pci_header_type &
                      ~GetByte(PCI_HEADER_MULTIFUNCTION, 2)))
                    continue;

                ATIScanPCIBases(&PCIPorts, &nPCIPort,
                    &pPCI->pci_base0, pPCI->basesize,
                    (pciReadLong(pPCI->tag, PCI_CMD_STAT_REG) &
                     PCI_CMD_IO_ENABLE) ? 0 : Allowed);
            }
        }

        /* Generate ProbeFlags array from list of registered PCI I/O bases */
        (void)memset(ProbeFlags, Allowed | DoProbe, SizeOf(ProbeFlags));
        for (i = 0;  i < nPCIPort;  i++)
        {
            CARD32 Base = PCIPorts[i].Base;
            CARD16 Count = (1 << PCIPorts[i].Size) - 1;
            CARD8  ProbeFlag = PCIPorts[i].Flag;

            /*
             * The following reduction of Count is based on the assumption that
             * PCI-registered I/O port ranges do not overlap.
             */
            for (j = 0;  j < nPCIPort;  j++)
            {
                CARD32 Base2 = PCIPorts[j].Base;

                if (Base < Base2)
                    while ((Base + Count) >= Base2)
                        Count >>= 1;
            }

            Base = LongPort(Base);
            Count = LongPort((Count | IO_BYTE_SELECT) + 1);
            while (Count--)
                ProbeFlags[Base++] &= ProbeFlag;
        }

        xfree(PCIPorts);

        /*
         * A note on probe strategy.  I/O and memory response by certain PCI
         * devices has been disabled by the common layer at this point,
         * including any devices this driver might be interested in.  The
         * following does sparse I/O probes, followed by block I/O probes.
         * Block I/O probes are dictated by what is found to be of interest in
         * PCI configuration space.  All this will detect ATI adapters that do
         * not implement this disablement, pre-PCI or not.
         *
         * PCI configuration space is then scanned again for ATI devices that
         * failed to be detected the first time around.  Each such device is
         * probed for again, this time with I/O temporarily enabled through
         * PCI.
         */
        if (ATICheckSparseIOBases(NULL, ProbeFlags, ATTRX, 16, TRUE) ==
            DoProbe)
        {
            pATI = ATIVGAProbe(NULL);
            if (pATI->Adapter == ATI_ADAPTER_NONE)
            {
                xfree(pATI);

                xf86MsgVerb(X_INFO, 4,
                    ATI_NAME ":  Unshared VGA not detected.\n");
            }
            else
            {
                /*
                 * Claim all MDA/HGA/CGA/EGA/VGA I/O ports.  This might need to
                 * be more selective.
                 */
                ATIClaimSparseIOBases(ProbeFlags, MonochromeIOBase, 48,
                    DetectedVGA);

                pVGA = pATI;
                strcpy(Identifier, "Unshared VGA");
                xf86MsgVerb(X_INFO, 3,
                    ATI_NAME ":  %s detected.\n", Identifier);
            }
        }
        else
        {
            xf86MsgVerb(X_INFO, 2, ATI_NAME ":  Unshared VGA not probed.\n");
        }

        if (ATICheckSparseIOBases(NULL, ProbeFlags, 0x02E8U, 8,
                fChipsets[ATI_CHIPSET_IBM8514] ||
                fChipsets[ATI_CHIPSET_MACH8] ||
                fChipsets[ATI_CHIPSET_MACH32]) == DoProbe)
        {
            if ((pATI = ATI8514Probe(NULL)))
            {
                strcpy(Identifier, "Unshared 8514/A");
                xf86MsgVerb(X_INFO, 3,
                    ATI_NAME ":  %s detected.\n", Identifier);

                AddAdapter(p8514 = pATI);

                if ((pATI->VGAAdapter != ATI_ADAPTER_NONE) ||
                    (pATI->Coprocessor != ATI_CHIP_NONE))
                    ATIClaimVGA(NULL, &pVGA, pATI, p8514, ProbeFlags,
                        Detected8514A);

                ATIClaimSparseIOBases(ProbeFlags, 0x02E8U, 8, Detected8514A);
            }
            else
            {
                xf86MsgVerb(X_INFO, 4,
                    ATI_NAME ":  Unshared 8514/A not detected.\n");
            }
        }
        else
        {
            xf86MsgVerb(X_INFO, 2,
                ATI_NAME ":  Unshared 8514/A not probed.\n");
        }

        for (i = 0;  i < NumberOf(Mach64SparseIOBases);  i++)
        {
            if (ATICheckSparseIOBases(NULL, ProbeFlags, Mach64SparseIOBases[i],
                    4, fChipsets[ATI_CHIPSET_MACH64]) != DoProbe)
            {
                xf86MsgVerb(X_INFO, 2,
                    ATI_NAME ":  Unshared Mach64 at PIO base 0x%04X not"
                    " probed.\n",
                    Mach64SparseIOBases[i]);
                continue;
            }

            pATI = ATIMach64Probe(NULL, Mach64SparseIOBases[i], SPARSE_IO, 0);
            if (!pATI)
            {
                xf86MsgVerb(X_INFO, 4,
                    ATI_NAME ":  Unshared Mach64 at PIO base 0x%04X not"
                    " detected.\n", Mach64SparseIOBases[i]);
                continue;
            }

            sprintf(Identifier, "Unshared Mach64 at sparse PIO base 0x%04lX",
                Mach64SparseIOBases[i]);
            xf86MsgVerb(X_INFO, 3, ATI_NAME ":  %s detected.\n", Identifier);

            AddAdapter(pMach64[i] = pATI);

            if (pATI->VGAAdapter != ATI_ADAPTER_NONE)
                ATIClaimVGA(NULL, &pVGA, pATI, p8514, ProbeFlags,
                    DetectedMach64);

            ATIClaimSparseIOBases(ProbeFlags, Mach64SparseIOBases[i], 4,
                DetectedMach64);
        }
    }

#endif /* AVOID_CPIO */

    if (xf86PciVideoInfo)
    {
        if (nATIGDev)
        {

#ifndef AVOID_NON_PCI

#ifdef AVOID_CPIO

            /* PCI sparse I/O adapters can still be used through MMIO */
            for (i = 0;  (pVideo = xf86PciVideoInfo[i++]);  )
            {
                if ((pVideo->vendor != PCI_VENDOR_ATI) ||
                    (pVideo->chipType == PCI_CHIP_MACH32) ||
                    pVideo->size[1])
                    continue;

                pPCI = pVideo->thisCard;
                PciReg = pciReadLong(pPCI->tag, PCI_REG_USERCONFIG);

                /* Possibly fix block I/O indicator */
                if (PciReg & 0x00000004U)
                    pciWriteLong(pPCI->tag, PCI_REG_USERCONFIG,
                        PciReg & ~0x00000004U);

                Chip = ATIChipID(pVideo->chipType, pVideo->chipRev);

                /*
                 * The CPIO base used by the adapter is of little concern here.
                 */
                pATI = ATIMach64Probe(pVideo, 0, SPARSE_IO, Chip);
                if (!pATI)
                    continue;

                sprintf(Identifier,
                    "Unshared PCI sparse I/O Mach64 in slot %d:%d:%d",
                    pVideo->bus, pVideo->device, pVideo->func);
                xf86MsgVerb(X_INFO, 3,
                    ATI_NAME ":  %s detected through Block 0 at 0x%08X.\n",
                    Identifier, pATI->Block0Base);
                AddAdapter(pATI);
                pATI->PCIInfo = pVideo;
            }

#endif /* AVOID_CPIO */

            for (i = 0;  (pVideo = xf86PciVideoInfo[i++]);  )
            {
                if ((pVideo->vendor != PCI_VENDOR_ATI) ||
                    (pVideo->chipType == PCI_CHIP_MACH32) ||
                    !pVideo->size[1])
                    continue;

                /* For now, ignore Rage128's and Radeon's */
                Chip = ATIChipID(pVideo->chipType, pVideo->chipRev);
                if (Chip > ATI_CHIP_Mach64)
                    continue;

                pPCI = pVideo->thisCard;

                /*
                 * Possibly fix block I/O indicator in PCI configuration space.
                 */
                PciReg = pciReadLong(pPCI->tag, PCI_REG_USERCONFIG);
                if (!(PciReg & 0x00000004U))
                    pciWriteLong(pPCI->tag, PCI_REG_USERCONFIG,
                        PciReg | 0x00000004U);

                pATI =
                    ATIMach64Probe(pVideo, pVideo->ioBase[1], BLOCK_IO, Chip);
                if (!pATI)
                    continue;

                sprintf(Identifier, "Unshared PCI/AGP Mach64 in slot %d:%d:%d",
                    pVideo->bus, pVideo->device, pVideo->func);
                xf86MsgVerb(X_INFO, 3,
                    ATI_NAME ":  %s detected.\n", Identifier);
                AddAdapter(pATI);

#ifndef AVOID_CPIO

                /* This is probably not necessary */
                if (pATI->VGAAdapter != ATI_ADAPTER_NONE)
                    ATIClaimVGA(pVideo, &pVGA, pATI, p8514,
                        ProbeFlags, DetectedMach64);

#endif /* AVOID_CPIO */

            }

#endif /* AVOID_NON_PCI */

#ifndef AVOID_CPIO

            /*
             * This is the second pass through PCI configuration space.  Much
             * of this is verbiage to deal with potential situations that are
             * very unlikely to occur in practice.
             *
             * First, look for non-ATI shareable VGA's.  For now, these must
             * the primary device.
             */
            if (ATICheckSparseIOBases(NULL, ProbeFlags, ATTRX, 16, TRUE) ==
                DoProbe)
            {
                for (i = 0;  (pVideo = xf86PciVideoInfo[i++]);  )
                {
                    if ((pVideo->vendor == PCI_VENDOR_ATI) ||
                        !xf86IsPrimaryPci(pVideo))
                        continue;

                    if (!xf86CheckPciSlot(pVideo->bus,
                                          pVideo->device,
                                          pVideo->func))
                        continue;

                    xf86SetPciVideo(pVideo, MEM_IO);

                    pATI = ATIVGAProbe(NULL);
                    if (pATI->Adapter == ATI_ADAPTER_NONE)
                    {
                        xfree(pATI);
                        xf86Msg(X_WARNING,
                            ATI_NAME ":  PCI/AGP VGA compatible in slot"
                            " %d:%d:%d could not be detected!\n",
                            pVideo->bus, pVideo->device, pVideo->func);
                    }
                    else
                    {
                        sprintf(Identifier,
                            "Shared non-ATI VGA in PCI/AGP slot %d:%d:%d",
                            pVideo->bus, pVideo->device, pVideo->func);
                        xf86MsgVerb(X_INFO, 3, ATI_NAME ":  %s detected.\n",
                            Identifier);
                        AddAdapter(pATI);
                        pATI->SharedVGA = TRUE;
                        pATI->BusType = ATI_BUS_PCI;
                        pATI->PCIInfo = pVideo;
                    }

                    xf86SetPciVideo(NULL, NONE);
                }
            }

            /* Next, look for PCI Mach32's */
            for (i = 0;  (pVideo = xf86PciVideoInfo[i++]);  )
            {
                if ((pVideo->vendor != PCI_VENDOR_ATI) ||
                    (pVideo->chipType != PCI_CHIP_MACH32))
                    continue;

                switch (ATICheckSparseIOBases(pVideo, ProbeFlags,
                    0x02E8U, 8, TRUE))
                {
                    case 0:
                        xf86Msg(X_WARNING,
                            ATI_NAME ":  PCI Mach32 in slot %d:%d:%d will not"
                            " be enabled\n because it conflicts with a"
                            " non-video PCI/AGP device.\n",
                            pVideo->bus, pVideo->device, pVideo->func);
                        break;

                    case Detected8514A:
                        if ((p8514->BusType >= ATI_BUS_PCI) && !p8514->PCIInfo)
                            p8514->PCIInfo = pVideo;
                        else
                            xf86Msg(X_WARNING,
                                ATI_NAME ":  PCI Mach32 in slot %d:%d:%d will"
                                " not be enabled\n because it conflicts with"
                                " another %s %s.\n",
                                pVideo->bus, pVideo->device, pVideo->func,
                                ATIBusNames[p8514->BusType],
                                ATIAdapterNames[p8514->Adapter]);
                        break;

                    case DetectedMach64:
                        xf86Msg(X_WARNING,
                            ATI_NAME ":  PCI Mach32 in slot %d:%d:%d will not"
                            " be enabled\n because it conflicts with a Mach64"
                            " at I/O base 0x02EC.\n",
                            pVideo->bus, pVideo->device, pVideo->func);
                        break;

                    default:    /* Must be DoProbe */
                        if (!xf86CheckPciSlot(pVideo->bus,
                                              pVideo->device,
                                              pVideo->func))
                            continue;

                        xf86SetPciVideo(pVideo, MEM_IO);

                        if (!(pATI = ATI8514Probe(pVideo)))
                            xf86Msg(X_WARNING,
                                ATI_NAME ":  PCI Mach32 in slot %d:%d:%d could"
                                " not be detected!\n",
                                pVideo->bus, pVideo->device, pVideo->func);
                        else
                        {
                            sprintf(Identifier,
                                "Shared 8514/A in PCI slot %d:%d:%d",
                                pVideo->bus, pVideo->device, pVideo->func);
                            xf86MsgVerb(X_INFO, 3,
                                ATI_NAME ":  %s detected.\n", Identifier);
                            if (pATI->Adapter != ATI_ADAPTER_MACH32)
                                xf86Msg(X_WARNING,
                                    ATI_NAME ":  PCI Mach32 in slot %d:%d:%d"
                                    " could only be detected as an %s!\n",
                                    pVideo->bus, pVideo->device, pVideo->func,
                                    ATIAdapterNames[pATI->Adapter]);

                            AddAdapter(pATI);
                            pATI->SharedAccelerator = TRUE;

                            if ((pATI->VGAAdapter != ATI_ADAPTER_NONE) ||
                                (pATI->Coprocessor != ATI_CHIP_NONE))
                                ATIFindVGA(pVideo, &pVGA, &pATI, p8514,
                                    ProbeFlags);
                        }

                        xf86SetPciVideo(NULL, NONE);
                        break;
                }
            }

            /* Next, look for sparse I/O Mach64's */
            for (i = 0;  (pVideo = xf86PciVideoInfo[i++]);  )
            {
                if ((pVideo->vendor != PCI_VENDOR_ATI) ||
                    (pVideo->chipType == PCI_CHIP_MACH32) ||
                    pVideo->size[1])
                    continue;

                pPCI = pVideo->thisCard;
                PciReg = pciReadLong(pPCI->tag, PCI_REG_USERCONFIG);
                j = PciReg & 0x03U;
                if (j == 0x03U)
                    xf86Msg(X_WARNING,
                        ATI_NAME ":  PCI Mach64 in slot %d:%d:%d cannot be"
                        " enabled\n because it has neither a block, nor a"
                        " sparse, I/O base.\n",
                        pVideo->bus, pVideo->device, pVideo->func);
                else switch(ATICheckSparseIOBases(pVideo, ProbeFlags,
                    Mach64SparseIOBases[j], 4, TRUE))
                {
                    case 0:
                        xf86Msg(X_WARNING,
                            ATI_NAME ":  PCI Mach64 in slot %d:%d:%d will not"
                            " be enabled\n because it conflicts with another"
                            " non-video PCI device.\n",
                            pVideo->bus, pVideo->device, pVideo->func);
                        break;

                    case Detected8514A:
                        xf86Msg(X_WARNING,
                            ATI_NAME ":  PCI Mach64 in slot %d:%d:%d will not"
                            " be enabled\n because it conflicts with an %s.\n",
                            pVideo->bus, pVideo->device, pVideo->func,
                            ATIAdapterNames[p8514->Adapter]);
                        break;

                    case DetectedMach64:
                        pATI = pMach64[j];
                        if ((pATI->BusType >= ATI_BUS_PCI) && !pATI->PCIInfo)
                            pATI->PCIInfo = pVideo;
                        else
                            xf86Msg(X_WARNING,
                                ATI_NAME ":  PCI Mach64 in slot %d:%d:%d will"
                                " not be enabled\n because it conflicts with"
                                " another %s Mach64 at sparse I/O base"
                                " 0x%04X.\n",
                                pVideo->bus, pVideo->device, pVideo->func,
                                ATIBusNames[pATI->BusType],
                                Mach64SparseIOBases[j]);
                        break;

                    default:        /* Must be DoProbe */
                        if (!xf86CheckPciSlot(pVideo->bus,
                                              pVideo->device,
                                              pVideo->func))
                            continue;

                        /* Possibly fix block I/O indicator */
                        if (PciReg & 0x00000004U)
                            pciWriteLong(pPCI->tag, PCI_REG_USERCONFIG,
                                PciReg & ~0x00000004U);

                        xf86SetPciVideo(pVideo, MEM_IO);

                        Chip = ATIChipID(pVideo->chipType, pVideo->chipRev);
                        pATI = ATIMach64Probe(pVideo, Mach64SparseIOBases[j],
                            SPARSE_IO, Chip);
                        if (!pATI)
                            xf86Msg(X_WARNING,
                                ATI_NAME ":  PCI Mach64 in slot %d:%d:%d could"
                                " not be detected!\n",
                                pVideo->bus, pVideo->device, pVideo->func);
                        else
                        {
                            sprintf(Identifier,
                                "Shared PCI Mach64 in slot %d:%d:%d",
                                pVideo->bus, pVideo->device, pVideo->func);
                            xf86MsgVerb(X_INFO, 3,
                                ATI_NAME ":  %s with sparse PIO base 0x%04X"
                                " detected.\n", Identifier,
                                Mach64SparseIOBases[j]);
                            AddAdapter(pATI);
                            pATI->SharedAccelerator = TRUE;
                            pATI->PCIInfo = pVideo;

                            if (pATI->VGAAdapter != ATI_ADAPTER_NONE)
                                ATIFindVGA(pVideo, &pVGA, &pATI, p8514,
                                    ProbeFlags);
                        }

                        xf86SetPciVideo(NULL, NONE);
                        break;
                }
            }

#else /* AVOID_CPIO */

            for (i = 0;  (pVideo = xf86PciVideoInfo[i++]);  )
            {
                if ((pVideo->vendor != PCI_VENDOR_ATI) ||
                    (pVideo->chipType == PCI_CHIP_MACH32) ||
                    pVideo->size[1])
                    continue;

                /* Check if this one has already been detected */
                for (j = 0;  j < nATIPtr;  j++)
                {
                    pATI = ATIPtrs[j];
                    if (pATI->PCIInfo == pVideo)
                        goto SkipThisSlot;
                }

                if (!xf86CheckPciSlot(pVideo->bus,
                                      pVideo->device,
                                      pVideo->func))
                    continue;

                xf86SetPciVideo(pVideo, MEM_IO);

                Chip = ATIChipID(pVideo->chipType, pVideo->chipRev);

                /* The adapter's CPIO base is of little concern here */
                pATI = ATIMach64Probe(pVideo, 0, SPARSE_IO, Chip);
                if (pATI)
                {
                    sprintf(Identifier, "Shared PCI Mach64 in slot %d:%d:%d",
                        pVideo->bus, pVideo->device, pVideo->func);
                    xf86MsgVerb(X_INFO, 3,
                        ATI_NAME ":  %s with Block 0 base 0x%08X detected.\n",
                        Identifier, pATI->Block0Base);
                    AddAdapter(pATI);
                    pATI->SharedAccelerator = TRUE;
                    pATI->PCIInfo = pVideo;
                }
                else
                {
                    xf86Msg(X_WARNING,
                        ATI_NAME ":  PCI Mach64 in slot %d:%d:%d could not be"
                        " detected!\n",
                        pVideo->bus, pVideo->device, pVideo->func);
                }

                xf86SetPciVideo(NULL, NONE);

        SkipThisSlot:;
            }

#endif /* AVOID_CPIO */

        }

        /* Lastly, look for block I/O devices */
        for (i = 0;  (pVideo = xf86PciVideoInfo[i++]);  )
        {
            if ((pVideo->vendor != PCI_VENDOR_ATI) ||
                (pVideo->chipType == PCI_CHIP_MACH32) ||
                !pVideo->size[1])
                continue;

            /* Check for Rage128's, Radeon's and later adapters */
            Chip = ATIChipID(pVideo->chipType, pVideo->chipRev);
            if (Chip > ATI_CHIP_Mach64)
            {
                switch (Chip)
                {
                    case ATI_CHIP_RAGE128GL:
                    case ATI_CHIP_RAGE128VR:
                    case ATI_CHIP_RAGE128PROULTRA:
                    case ATI_CHIP_RAGE128PROGL:
                    case ATI_CHIP_RAGE128PROVR:
                    case ATI_CHIP_RAGE128MOBILITY3:
                    case ATI_CHIP_RAGE128MOBILITY4:
                        DoRage128 = TRUE;
                        continue;

                    case ATI_CHIP_RADEON:
                    case ATI_CHIP_RADEONVE:
                    case ATI_CHIP_RADEONMOBILITY6:
                    case ATI_CHIP_RADEONMOBILITY7:
                    case ATI_CHIP_R200:
                    case ATI_CHIP_RV200:
                    case ATI_CHIP_RV250:
                    case ATI_CHIP_RADEONMOBILITY9:
                    case ATI_CHIP_R300:
                        DoRadeon = TRUE;
                        continue;

                    case ATI_CHIP_HDTV:
                    default:
                        continue;
                }
            }

            if (!nATIGDev)
                continue;

            /* Check if this one has already been detected */
            for (j = 0;  j < nATIPtr;  j++)
            {
                pATI = ATIPtrs[j];
                if (pATI->CPIOBase == pVideo->ioBase[1])
                    goto SetPCIInfo;
            }

            if (!xf86CheckPciSlot(pVideo->bus, pVideo->device, pVideo->func))
                continue;

            /* Probe for it */
            xf86SetPciVideo(pVideo, MEM_IO);

            pATI = ATIMach64Probe(pVideo, pVideo->ioBase[1], BLOCK_IO, Chip);
            if (pATI)
            {
                sprintf(Identifier, "Shared PCI/AGP Mach64 in slot %d:%d:%d",
                    pVideo->bus, pVideo->device, pVideo->func);
                xf86MsgVerb(X_INFO, 3, ATI_NAME ":  %s detected.\n",
                    Identifier);
                AddAdapter(pATI);
                pATI->SharedAccelerator = TRUE;

#ifndef AVOID_CPIO

                if (pATI->VGAAdapter != ATI_ADAPTER_NONE)
                    ATIFindVGA(pVideo, &pVGA, &pATI, p8514, ProbeFlags);

#endif /* AVOID_CPIO */

            }

            xf86SetPciVideo(NULL, NONE);

            if (!pATI)
            {
                xf86Msg(X_WARNING,
                    ATI_NAME ":  PCI/AGP Mach64 in slot %d:%d:%d could not be"
                    " detected!\n", pVideo->bus, pVideo->device, pVideo->func);
                continue;
            }

        SetPCIInfo:
            pATI->PCIInfo = pVideo;
        }
    }

#ifndef AVOID_CPIO

    /*
     * At this point, if there's a non-shareable VGA with its own framebuffer,
     * find out if it's an ATI VGA Wonder.
     */
    do
    {
        if (!nATIGDev || !pVGA || (pVGA->VGAAdapter > ATI_ADAPTER_VGA))
            break;

        /* If it has not been assigned to a coprocessor, keep track of it */
        if (pVGA->Coprocessor == ATI_CHIP_NONE)
            AddAdapter(pVGA);

        /*
         * A VGA should have installed its int 10 vector.  Use that to find the
         * VGA BIOS.  If this fails, scan all legacy BIOS segments, in 512-byte
         * increments.
         */
        if (xf86ReadBIOS(0U, 0x42U, BIOS, 2) != 2)
            goto NoVGAWonder;

        pATI = NULL;
        BIOSBase = 0;
        if (!(BIOS[0] & 0x1FU)) /* Otherwise there's no 512-byte alignment */
            BIOSBase = ((BIOS[1] << 8) | BIOS[0]) << 4;

        /* Look for its BIOS */
        for(;  ;  BIOSBase += 0x00000200U)
        {
            if (!BIOSBase)
                goto SkipBiosSegment;

            if (BIOSBase >= 0x000F8000U)
                goto NoVGAWonder;

            /* Skip over those that are already known */
            for (i = 0;  i < nATIPtr;  i++)
                if (ATIPtrs[i]->BIOSBase == BIOSBase)
                    goto SkipBiosSegment;

            /* Get first 80 bytes of video BIOS */
            if (xf86ReadBIOS(BIOSBase, 0, BIOS, SizeOf(BIOS)) !=
                SizeOf(BIOS))
                goto NoVGAWonder;

            if ((BIOS[0x00U] != 0x55U) || (BIOS[0x01U] != 0xAAU))
                goto SkipBiosSegment;

            if ((BIOS[0x1EU] == 'I') &&
                (BIOS[0x1FU] == 'B') &&
                (BIOS[0x20U] == 'M'))
                break;

            /* XXX Should PCI BIOS signature be checked for here ? */
            if ((BIOS[0x20U] == 'P') &&
                (BIOS[0x21U] == 'C') &&
                (BIOS[0x22U] == 'I'))
                break;

    SkipBiosSegment:
            if (pATI)
                continue;

            pATI = pVGA;
            BIOSBase = 0x000C0000U - 0x00000200U;
        }

        pVGA->BIOSBase = BIOSBase;

        /* Look for the ATI signature string */
        if (memcmp(BIOS + BIOSSignature, ATISignature, SignatureSize))
            break;

        if (BIOS[0x40U] != '3')
            break;

        switch (BIOS[0x41U])
        {
            case '1':
                /* This is a Mach8 or VGA Wonder adapter of some kind */
                if ((BIOS[0x43U] >= '1') && (BIOS[0x43U] <= '6'))
                    pVGA->Chip = BIOS[0x43U] - ('1' - ATI_CHIP_18800);

                switch (BIOS[0x43U])
                {
                    case '1':   /* ATI_CHIP_18800 */
                        pVGA->VGAOffset = 0xB0U;
                        pVGA->VGAAdapter = ATI_ADAPTER_V3;
                        break;

                    case '2':   /* ATI_CHIP_18800_1 */
                        pVGA->VGAOffset = 0xB0U;
                        if (BIOS[0x42U] & 0x10U)
                            pVGA->VGAAdapter = ATI_ADAPTER_V5;
                        else
                            pVGA->VGAAdapter = ATI_ADAPTER_V4;
                        break;

                    case '3':   /* ATI_CHIP_28800_2 */
                    case '4':   /* ATI_CHIP_28800_4 */
                    case '5':   /* ATI_CHIP_28800_5 */
                    case '6':   /* ATI_CHIP_28800_6 */
                        pVGA->VGAOffset = 0xA0U;
                        if (BIOS[0x44U] & 0x80U)
                            pVGA->VGAAdapter = ATI_ADAPTER_XL;
                        else
                            pVGA->VGAAdapter = ATI_ADAPTER_PLUS;
                        break;

                    case 'a':   /* A crippled Mach32 */
                    case 'b':
                    case 'c':
                        pVGA->VGAOffset = 0x80U;
                        pVGA->VGAAdapter = ATI_ADAPTER_NONISA;
                        ATIMach32ChipID(pVGA);
                        ProbeWaitIdleEmpty();
                        if (inw(SUBSYS_STAT) != (CARD16)(-1))
                            pVGA->ChipHasSUBSYS_CNTL = TRUE;
                        break;
#if 0
                    case ' ':   /* A crippled Mach64 */
                        pVGA->VGAOffset = 0x80U;
                        pVGA->VGAAdapter = ATI_ADAPTER_NONISA;
                        ATIMach64ChipID(pVGA, 0);
                        break;
#endif
                    default:
                        break;
                }

                if (pVGA->VGAAdapter == ATI_ADAPTER_NONE)
                    break;

                /* Set VGA Wonder I/O port */
                pVGA->CPIO_VGAWonder = BIOSWord(0x10U) & SPARSE_IO_PORT;
                if (!pVGA->CPIO_VGAWonder)
                    pVGA->CPIO_VGAWonder = 0x01CEU;

                ATIVGAWonderProbe(NULL, pVGA, p8514, ProbeFlags);
                break;
#if 0
            case '2':
                pVGA->VGAOffset = 0xB0U;            /* Presumably */
                pVGA->VGAAdapter = ATI_ADAPTER_EGA_PLUS;
                break;

            case '3':
                pVGA->VGAOffset = 0xB0U;            /* Presumably */
                pVGA->VGAAdapter = ATI_ADAPTER_BASIC;
                break;

            case '?':           /* A crippled Mach64 */
                pVGA->VGAAdapter = ATI_ADAPTER_NONISA;
                ATIMach64ChipID(pVGA, 0);
                break;
#endif
            default:
                break;
        }

        if (pVGA->Adapter <= ATI_ADAPTER_VGA)
            pVGA->Adapter = pVGA->VGAAdapter;

NoVGAWonder:;
    } while (0);

#endif /* AVOID_CPIO */

    /*
     * Re-order list of detected devices so that the primary device is before
     * any other PCI device.
     */
    for (i = 0;  i < nATIPtr;  i++)
    {
        if (!ATIPtrs[i]->PCIInfo)
            continue;

        for (j = i;  j < nATIPtr;  j++)
        {
            pATI = ATIPtrs[j];
            if (!xf86IsPrimaryPci(pATI->PCIInfo))
                continue;

            for (;  j > i;  j--)
                ATIPtrs[j] = ATIPtrs[j - 1];
            ATIPtrs[j] = pATI;
            break;
        }

        break;
    }

    if (flags & PROBE_DETECT)
    {
        /*
         * No XF86Config information available, so use the default Chipset of
         * "ati", and as many device sections as there are adapters.
         */
        for (i = 0;  i < nATIPtr;  i++)
        {
            pATI = ATIPtrs[i];

#ifndef AVOID_CPIO

            if ((pATI->Adapter != ATI_ADAPTER_VGA) &&
                ((pATI->Adapter != ATI_ADAPTER_8514A) ||
                 ((pATI->VGAAdapter != ATI_ADAPTER_VGA) &&
                  (pATI->VGAAdapter != ATI_ADAPTER_NONE))))

#endif /* AVOID_CPIO */

            {
                ProbeSuccess = TRUE;
                pGDev = xf86AddDeviceToConfigure(ATI_DRIVER_NAME,
                    pATI->PCIInfo, ATI_CHIPSET_ATI);
                if (pGDev)
                {
                    /* Fill in additional information */
                    pGDev->vendor = ATI_NAME;
                    pGDev->chipset = (char *)ATIChipsetNames[ATI_CHIPSET_ATI];
                    if (!pATI->PCIInfo)
                        pGDev->busID = NULL;
                }
            }

            xfree(pATI);
        }
    }
    else
    {
        /*
         * Assign detected devices to XF86Config Device sections.  This is done
         * by comparing certain Device section specifications against the
         * corresponding adapter information.  Begin with those specifications
         * that are independent of the adapter's bus location.
         */
        for (i = 0, pATIGDev = ATIGDevs;  i < nATIGDev;  i++, pATIGDev++)
        {
            pGDev = pATIGDev->pGDev;

            for (j = 0;  j < nATIPtr;  j++)
            {
                pATI = ATIPtrs[j];

                /*
                 * First check the Chipset specification.  The placement of
                 * "break" and "continue" statements here is carefully chosen
                 * to produce the intended behaviour for each Chipset value.
                 */
                switch (pATIGDev->Chipset)
                {
                    case ATI_CHIPSET_ATI:

#ifndef AVOID_CPIO

                        if (pATI->Adapter == ATI_ADAPTER_VGA)
                            continue;
                        if (pATI->Adapter != ATI_ADAPTER_8514A)
                            break;
                        /* Fall through */

                    case ATI_CHIPSET_ATIVGA:
                        if (pATI->VGAAdapter == ATI_ADAPTER_VGA)
                            continue;
                        /* Fall through */

                    case ATI_CHIPSET_IBMVGA:
                        if (pATI->VGAAdapter == ATI_ADAPTER_NONE)
                            continue;
                        break;

                    case ATI_CHIPSET_VGAWONDER:
                        if (!pATI->CPIO_VGAWonder)
                            continue;
                        break;

                    case ATI_CHIPSET_IBM8514:
                        if (pATI->Adapter == ATI_ADAPTER_8514A)
                            break;
                        /* Fall through */

                    case ATI_CHIPSET_MACH8:
                        if (pATI->Adapter == ATI_ADAPTER_MACH8)
                            break;
                        /* Fall through */

                    case ATI_CHIPSET_MACH32:
                        if (pATI->Adapter == ATI_ADAPTER_MACH32)
                            break;
                        continue;

#endif /* AVOID_CPIO */

                    case ATI_CHIPSET_MACH64:
                        if (pATI->Adapter == ATI_ADAPTER_MACH64)
                            break;
                        continue;

                    default:
                        continue;
                }

                /*
                 * The ChipID and ChipRev specifications are compared next.
                 * First, require these to be unspecified for anything other
                 * than Mach32 or Mach64 adapters.  ChipRev is also required to
                 * be unspecified for Mach32's.  ChipID is optional for
                 * Mach32's, and both specifications are optional for Mach64's.
                 * Lastly, allow both specifications to override their detected
                 * value in the case of Mach64 adapters whose ChipID is
                 * unrecognised.
                 */
                pVideo = pATI->PCIInfo;
                if (pGDev->chipID >= 0)
                {
                    if ((pATI->ChipType != pGDev->chipID) &&
                        (!pVideo || (pGDev->chipID != pVideo->chipType)))
                    {
                        if ((pATI->Adapter != ATI_ADAPTER_MACH64) ||
                            (pATI->Chip != ATI_CHIP_Mach64))
                            continue;

                        Chip = ATIChipID(pGDev->chipID, 0);
                        if ((Chip <= ATI_CHIP_264GTB) ||
                            (Chip == ATI_CHIP_Mach64))
                            continue;
                    }
                    if ((pGDev->chipRev >= 0) &&
                        (pATI->ChipRev != pGDev->chipRev) &&
                        (!pVideo || (pGDev->chipRev != pVideo->chipRev) ||
                         (pGDev->chipID != pVideo->chipType)))
                    {
                        if (pATI->Chip < ATI_CHIP_264CT)
                            continue;

                        if (pATI->Chip != ATI_CHIP_Mach64)
                        {
                            /*
                             * There are two foundry codes for UMC.  Some
                             * adapters will advertise one in CONFIG_CHIP_ID
                             * and the other in PCI configuration space.  For
                             * matching purposes, make both codes compare
                             * equal.
                             */
#                           define UMC_IGNORE \
                                (ATI_FOUNDRY_UMC ^ ATI_FOUNDRY_UMCA)
#                           define UMC_NOCARE \
                                GetBits(SetBits(UMC_IGNORE, CFG_CHIP_FOUNDRY), \
                                    CFG_CHIP_REV)

                            if ((pATI->ChipRev ^ pGDev->chipRev) & ~UMC_NOCARE)
                                continue;

                            if ((pATI->ChipFoundry != ATI_FOUNDRY_UMC) &&
                                (pATI->ChipFoundry != ATI_FOUNDRY_UMCA))
                                continue;

                            k = GetBits(pGDev->chipRev,
                                GetBits(CFG_CHIP_FOUNDRY, CFG_CHIP_REV));
                            if ((k != ATI_FOUNDRY_UMC) &&
                                (k != ATI_FOUNDRY_UMCA))
                                continue;
                        }
                    }
                }

                /*
                 * IOBase is next.  This is the first specification that is
                 * potentially dependent on bus location.  It is only allowed
                 * for Mach64 adapters, and is optional.
                 */
                if (pGDev->IOBase && (pATI->CPIOBase != pGDev->IOBase))
                    continue;

                /*
                 * Compare BusID's.  This specification is only allowed for PCI
                 * Mach32's or Mach64's and is optional.
                 */
                if (pGDev->busID && pGDev->busID[0])
                {
                    pVideo = pATI->PCIInfo;

#ifndef AVOID_CPIO

                    if (!pVideo)
                        continue;

#endif /* AVOID_CPIO */

                    if (!xf86ComparePciBusString(pGDev->busID,
                            pVideo->bus, pVideo->device, pVideo->func))
                        continue;
                }

                /*
                 * Ensure no two adapters are assigned to the same XF86Config
                 * Device section.
                 */
                if (pATIGDev->iATIPtr)
                {
                    if (pATIGDev->iATIPtr < 0)
                        break;

                    xf86Msg(X_ERROR,
                        ATI_NAME ":  XF86Config Device section \"%s\" may not"
                        " be assigned to more than one adapter.\n",
                        pGDev->identifier);
                    pATIGDev->iATIPtr = -1;
                    break;
                }

                /* Assign adapter */
                pATIGDev->iATIPtr = j + 1;

                /*
                 * For compatibility with previous releases, assign the first
                 * applicable adapter if there is only one Device section.
                 */
                if (nATIGDev == 1)
                    break;
            }
        }

        /*
         * Ensure no two XF86Config Device sections are assigned to the same
         * adapter.  Then, generate screens for any that are left.
         */
        for (i = 0, pATIGDev = ATIGDevs;  i < nATIGDev;  i++, pATIGDev++)
        {
            pGDev = pATIGDev->pGDev;

            j = pATIGDev->iATIPtr;
            if (j <= 0)
                continue;

            for (k = i;  ++k < nATIGDev;  )
            {
                if (j == ATIGDevs[k].iATIPtr)
                {
                    xf86Msg(X_ERROR,
                        ATI_NAME ":  XF86Config Device sections \"%s\" and"
                        " \"%s\" may not be assigned to the same adapter.\n",
                        pGDev->identifier, ATIGDevs[k].pGDev->identifier);
                    pATIGDev->iATIPtr = ATIGDevs[k].iATIPtr = -1;
                }
            }

            j = ATIGDevs[i].iATIPtr;
            if (j <= 0)
                continue;

            pATI = ATIPtrs[j - 1];

            xf86MsgVerb(X_INFO, 3,
                ATI_NAME ":  %s assigned to %sactive \"Device\" section"
                " \"%s\".\n",
                Identifier, pGDev->active ? "" : "in", pGDev->identifier);

            /*
             * Attach adapter to XF86Config Device section and register its
             * resources.
             */
            if (ATIClaimBusSlot(pDriver, pATIGDev->Chipset,
                                pGDev, pGDev->active, pATI) < 0)
            {
                xf86Msg(X_ERROR,
                    ATI_NAME ":  Could not claim bus slot for %s.\n",
                    Identifier);
                continue;
            }

            if (!pGDev->active)
                continue;

            /* Allocate screen */
            pScreenInfo = xf86AllocateScreen(pDriver, 0);

#ifdef XFree86LOADER

            if (!xf86LoadSubModule(pScreenInfo, "atimisc"))
            {
                xf86Msg(X_ERROR,
                    ATI_NAME ":  Failed to load \"atimisc\" module.\n");
                xf86DeleteScreen(pScreenInfo->scrnIndex, 0);
                continue;
            }

            xf86LoaderReqSymLists(ATISymbols, NULL);

#endif

            /* Attach device to screen */
            xf86AddEntityToScreen(pScreenInfo, pATI->iEntity);

            ATIPtrs[j - 1] = NULL;

            /* Fill in probe data */
            pScreenInfo->driverVersion = ATI_VERSION_CURRENT;
            pScreenInfo->driverName    = ATI_DRIVER_NAME;
            pScreenInfo->name          = ATI_NAME;
            pScreenInfo->Probe         = ATIProbe;
            pScreenInfo->PreInit       = ATIPreInit;
            pScreenInfo->ScreenInit    = ATIScreenInit;
            pScreenInfo->SwitchMode    = ATISwitchMode;
            pScreenInfo->AdjustFrame   = ATIAdjustFrame;
            pScreenInfo->EnterVT       = ATIEnterVT;
            pScreenInfo->LeaveVT       = ATILeaveVT;
            pScreenInfo->FreeScreen    = ATIFreeScreen;
            pScreenInfo->ValidMode     = ATIValidMode;

            pScreenInfo->driverPrivate = pATI;

            pATI->Chipset = pATIGDev->Chipset;

            ProbeSuccess = TRUE;
        }

        /* Deal with unassigned adapters */
        for (i = 0;  i < nATIPtr;  i++)
        {
            if (!(pATI = ATIPtrs[i]))
                continue;

#ifndef AVOID_CPIO

            if (pATI->Adapter > ATI_ADAPTER_VGA)

#endif /* AVOID_CPIO */

            {
                if (pATI->iEntity < 0)
                    (void)ATIClaimBusSlot(pDriver, 0, NULL, FALSE, pATI);
            }

            xfree(pATI);
        }

        xfree(ATIGDevs);
    }

    xfree(ATIPtrs);

    /* Call Rage 128 driver probe */
    if (DoRage128 && R128Probe(pDriver, flags))
        ProbeSuccess = TRUE;

    /* Call Radeon driver probe */
    if (DoRadeon && RADEONProbe(pDriver, flags))
        ProbeSuccess = TRUE;

    return ProbeSuccess;
}

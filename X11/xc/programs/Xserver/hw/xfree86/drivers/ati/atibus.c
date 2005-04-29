/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/ati/atibus.c,v 1.19 2004/01/05 16:42:00 tsi Exp $ */
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
#include "atiadapter.h"
#include "atibus.h"
#include "atichip.h"
#include "atiio.h"
#include "atistruct.h"
#include "ativersion.h"

/*
 * Definitions related to an adapter's system bus interface.
 */

const char *ATIBusNames[] =
{
    "16-Bit ISA",
    "EISA",
    "16-Bit MicroChannel",
    "32-Bit MicroChannel",
    "386SX Local Bus",
    "386DX Local Bus",
    "VESA Local Bus",
    "PCI",
    "AGP"
};

/*
 * ATIClaimResources --
 *
 * This function registers most of the bus resources used by an adapter.  The
 * exceptions are PCI-configured resources and non-PCI non-AGP linear
 * apertures, both of which are registered by ATIPreInit().  This function also
 * attempts to register unshareable resources for inactive PCI adapters,
 * whether or not they are relocatable.
 */
static void
ATIClaimResources
(
    ATIPtr pATI,
    Bool   Active
)
{
    resPtr   pResources;

#ifndef AVOID_CPIO

    resRange Resources[2] = {{0, 0, 0}, _END};

    /* Claim VGA and VGAWonder resources */
    if ((pATI->VGAAdapter != ATI_ADAPTER_NONE) && (Active || !pATI->SharedVGA))
    {
        /*
         * 18800-x's are the only ATI controllers that decode all ISA aliases
         * of VGA and VGA Wonder I/O ports.  Other x8800's do not decode >any<
         * VGA aliases, but do decode VGA Wonder aliases whose most significant
         * nibble is zero.
         */
        xf86ClaimFixedResources(
            (pATI->Chip <= ATI_CHIP_18800_1) ?
            (pATI->SharedVGA ? resVgaSparseShared : resVgaSparseExclusive) :
            (pATI->SharedVGA ? resVgaShared : resVgaExclusive),
            pATI->iEntity);

        if (pATI->CPIO_VGAWonder)
        {
            if (pATI->SharedVGA)
                Resources[0].type = ResShrIoSparse | ResBus;
            else
                Resources[0].type = ResExcIoSparse | ResBus;
            Resources[0].rBase = pATI->CPIO_VGAWonder;
            if (pATI->Chip <= ATI_CHIP_18800_1)
                Resources[0].rMask = 0x03FEU;
            else
                Resources[0].rMask = 0xF3FEU;

            xf86ClaimFixedResources(Resources, pATI->iEntity);

            (void)memcpy(pATI->VGAWonderResources,
                Resources, SizeOf(Resources));
        }
    }

    if (!Active && pATI->SharedAccelerator)
        return;

    /* Claim 8514/A resources */
    if (pATI->ChipHasSUBSYS_CNTL)
        xf86ClaimFixedResources(
            pATI->SharedAccelerator ? res8514Shared : res8514Exclusive,
            pATI->iEntity);

    /* Claim Mach64 sparse I/O resources */
    if ((pATI->Adapter == ATI_ADAPTER_MACH64) &&
        (pATI->CPIODecoding == SPARSE_IO))
    {
        if (pATI->SharedAccelerator)
            Resources[0].type = ResShrIoSparse | ResBus;
        else
            Resources[0].type = ResExcIoSparse | ResBus;
        Resources[0].rBase = pATI->CPIOBase;
        Resources[0].rMask = 0x03FCU;

        xf86ClaimFixedResources(Resources, pATI->iEntity);
    }

    if (Active)
        return;

#else /* AVOID_CPIO */

    if (pATI->SharedAccelerator)
        return;

#endif /* AVOID_CPIO */

    /* Register unshared relocatable resources for inactive adapters */
    do
    {
        pResources = xf86RegisterResources(pATI->iEntity, NULL, ResExclusive);
        if (!pResources)
            return;

        pResources = xf86ReallocatePciResources(pATI->iEntity, pResources);
    } while (!pResources);

    xf86Msg(X_WARNING,
        ATI_NAME ":  Unable to register the following resources for inactive"
        " adapter:\n");
    xf86PrintResList(1, pResources);
    xf86FreeResList(pResources);
}

/*
 * ATIClaimBusSlot --
 *
 * Claim an adapter and register its resources.
 */
int
ATIClaimBusSlot
(
    DriverPtr pDriver,
    int       Chipset,
    GDevPtr   pGDev,
    Bool      Active,
    ATIPtr    pATI
)
{
    pciVideoPtr pVideo = pATI->PCIInfo;

    if (pVideo)
        pATI->iEntity =
            xf86ClaimPciSlot(pVideo->bus, pVideo->device, pVideo->func,
                pDriver, Chipset, pGDev, Active);
    else
        pATI->iEntity = xf86ClaimIsaSlot(pDriver, Chipset, pGDev, Active);

    if (pATI->iEntity >= 0)
        ATIClaimResources(pATI, Active);

    return pATI->iEntity;
}

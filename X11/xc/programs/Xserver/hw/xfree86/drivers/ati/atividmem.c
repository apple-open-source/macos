/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/ati/atividmem.c,v 1.16 2004/01/05 16:42:05 tsi Exp $ */
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
#include "atistruct.h"
#include "atividmem.h"

/* Memory types for 68800's and 88800GX's */
const char *ATIMemoryTypeNames_Mach[] =
{
    "DRAM (256Kx4)",
    "VRAM (256Kx4, x8, x16)",
    "VRAM (256Kx16 with short shift register)",
    "DRAM (256Kx16)",
    "Graphics DRAM (256Kx16)",
    "Enhanced VRAM (256Kx4, x8, x16)",
    "Enhanced VRAM (256Kx16 with short shift register)",
    "Unknown video memory type"
};

/* Memory types for 88800CX's */
const char *ATIMemoryTypeNames_88800CX[] =
{
    "DRAM (256Kx4, x8, x16)",
    "EDO DRAM (256Kx4, x8, x16)",
    "Unknown video memory type",
    "DRAM (256Kx16 with assymetric RAS/CAS)",
    "Unknown video memory type",
    "Unknown video memory type",
    "Unknown video memory type",
    "Unknown video memory type"
};

/* Memory types for 264xT's */
const char *ATIMemoryTypeNames_264xT[] =
{
    "Disabled video memory",
    "DRAM",
    "EDO DRAM",
    "Pseudo-EDO DRAM",
    "SDRAM (1:1)",
    "SGRAM (1:1)",
    "SGRAM (2:1) 32-bit",
    "Unknown video memory type"
};

#ifndef AVOID_CPIO

/*
 * ATIUnmapVGA --
 *
 * Unmap VGA aperture.
 */
static void
ATIUnmapVGA
(
    int    iScreen,
    ATIPtr pATI
)
{
    if (!pATI->pBank)
        return;

    xf86UnMapVidMem(iScreen, pATI->pBank, 0x00010000U);

    pATI->pBank = pATI->BankInfo.pBankA = pATI->BankInfo.pBankB = NULL;
}

#endif /* AVOID_CPIO */

/*
 * ATIUnmapLinear --
 *
 * Unmap linear aperture.
 */
static void
ATIUnmapLinear
(
    int    iScreen,
    ATIPtr pATI
)
{

#ifdef AVOID_CPIO

    if (!pATI->pMemory)
        return;

#else /* AVOID_CPIO */

    if (pATI->pMemory != pATI->pBank)

#endif /* AVOID_CPIO */

    {
        xf86UnMapVidMem(iScreen, pATI->pMemory, pATI->LinearSize);

#if X_BYTE_ORDER != X_LITTLE_ENDIAN

        if (pATI->pMemoryLE)
            xf86UnMapVidMem(iScreen, pATI->pMemoryLE, pATI->LinearSize);

#endif /* X_BYTE_ORDER */

    }

    pATI->pMemory = pATI->pMemoryLE = NULL;
}

/*
 * ATIUnmapMMIO --
 *
 * Unmap MMIO registers.
 */
static void
ATIUnmapMMIO
(
    int    iScreen,
    ATIPtr pATI
)
{
    if (pATI->pMMIO)
        xf86UnMapVidMem(iScreen, pATI->pMMIO, getpagesize());

    pATI->pMMIO = pATI->pBlock[0] = pATI->pBlock[1] = NULL;
}

/*
 * ATIUnmapCursor --
 *
 * Unmap hardware cursor image area.
 */
static void
ATIUnmapCursor
(
    int    iScreen,
    ATIPtr pATI
)
{
    if (pATI->pCursorPage)
        xf86UnMapVidMem(iScreen, pATI->pCursorPage, getpagesize());

    pATI->pCursorPage = pATI->pCursorImage = NULL;
}

/*
 * ATIMapApertures --
 *
 * This function maps all apertures used by the driver.
 */
Bool
ATIMapApertures
(
    int    iScreen,
    ATIPtr pATI
)
{
    pciVideoPtr   pVideo;
    PCITAG        Tag;
    unsigned long PageSize;

    if (pATI->Mapped)
        return TRUE;

#ifndef AVOID_CPIO

    if (pATI->VGAAdapter == ATI_ADAPTER_NONE)

#endif /* AVOID_CPIO */

    {
        if (!pATI->LinearBase && !pATI->Block0Base)
            return FALSE;
    }

    PageSize = getpagesize();

    if ((pVideo = pATI->PCIInfo))
        Tag = ((pciConfigPtr)(pVideo->thisCard))->tag;
    else
        Tag = 0;

#ifndef AVOID_CPIO

    /* Map VGA aperture */
    if (pATI->VGAAdapter != ATI_ADAPTER_NONE)
    {
        /*
         * No relocation, resizing, caching or write-combining of this
         * aperture is supported.  Hence, the hard-coded values here...
         */
        if (pVideo)
            pATI->pBank = xf86MapPciMem(iScreen, VIDMEM_MMIO,
                Tag, 0x000A0000U, 0x00010000U);
        else
            pATI->pBank = xf86MapVidMem(iScreen, VIDMEM_MMIO,
                0x000A0000U, 0x00010000U);

        if (!pATI->pBank)
            return FALSE;

        pATI->pMemory =
            pATI->BankInfo.pBankA =
            pATI->BankInfo.pBankB = pATI->pBank;

        pATI->Mapped = TRUE;
    }

#endif /* AVOID_CPIO */

    /* Map linear aperture */
    if (pATI->LinearBase)
    {
        if (pVideo)
            pATI->pMemory = xf86MapPciMem(iScreen, VIDMEM_FRAMEBUFFER,
                Tag, pATI->LinearBase, pATI->LinearSize);
        else
            pATI->pMemory = xf86MapVidMem(iScreen, VIDMEM_FRAMEBUFFER,
                pATI->LinearBase, pATI->LinearSize);

        if (!pATI->pMemory)
        {

#ifndef AVOID_CPIO

            ATIUnmapVGA(iScreen, pATI);

#endif /* AVOID_CPIO */

            pATI->Mapped = FALSE;
            return FALSE;
        }

        pATI->Mapped = TRUE;

#if X_BYTE_ORDER == X_LITTLE_ENDIAN

        if ((pATI->CursorBase >= pATI->LinearBase) &&
            ((pATI->CursorOffset + 0x00000400UL) <= (CARD32)pATI->LinearSize))
            pATI->pCursorImage = (char *)pATI->pMemory + pATI->CursorOffset;

        pATI->pMemoryLE = pATI->pMemory;

#else /* if X_BYTE_ORDER != X_LITTLE_ENDIAN */

        /*
         * Map the little-endian aperture (used for video, etc.).  Note that
         * caching of this area is _not_ wanted.
         */
        if (pVideo)
        {
            pATI->pMemoryLE = xf86MapPciMem(iScreen, VIDMEM_MMIO, Tag,
                pATI->LinearBase - 0x00800000U, pATI->LinearSize);

            if (!pATI->pMemoryLE)
            {
                ATIUnmapLinear(iScreen, pATI);

#ifndef AVOID_CPIO

                ATIUnmapVGA(iScreen, pATI);

#endif /* AVOID_CPIO */

                pATI->Mapped = FALSE;
                return FALSE;
            }
        }

#endif /* X_BYTE_ORDER */

    }

    /* Map MMIO aperture */
    if (pATI->Block0Base)
    {
        unsigned long MMIOBase = pATI->Block0Base & ~(PageSize - 1);

        if (pVideo)
            pATI->pMMIO = xf86MapPciMem(iScreen, VIDMEM_MMIO,
                Tag, MMIOBase, PageSize);
        else
            pATI->pMMIO = xf86MapVidMem(iScreen, VIDMEM_MMIO,
                MMIOBase, PageSize);

        if (!pATI->pMMIO)
        {

#if X_BYTE_ORDER == X_LITTLE_ENDIAN

            ATIUnmapCursor(iScreen, pATI);

#endif /* X_BYTE_ORDER */

            ATIUnmapLinear(iScreen, pATI);

#ifndef AVOID_CPIO

            ATIUnmapVGA(iScreen, pATI);

#endif /* AVOID_CPIO */

            pATI->Mapped = FALSE;
            return FALSE;
        }

        pATI->Mapped = TRUE;

        pATI->pBlock[0] = (char *)pATI->pMMIO +
            (pATI->Block0Base - MMIOBase);

        if (pATI->Block1Base)
            pATI->pBlock[1] = (char *)pATI->pBlock[0] - 0x00000400U;

#if X_BYTE_ORDER == X_LITTLE_ENDIAN

        if (!pATI->pCursorImage)

#endif /* X_BYTE_ORDER */

        {
            if ((pATI->CursorBase >= MMIOBase) &&
                ((pATI->CursorBase + 0x00000400UL) <= (MMIOBase + PageSize)))
                pATI->pCursorImage = (char *)pATI->pMMIO +
                    (pATI->CursorBase - MMIOBase);
        }
    }

    /* Map hardware cursor image area */
    if (pATI->CursorBase && !pATI->pCursorImage)
    {
        unsigned long CursorBase = pATI->CursorBase & ~(PageSize - 1);

        if (pVideo)
            pATI->pCursorPage = xf86MapPciMem(iScreen, VIDMEM_FRAMEBUFFER,
                Tag, CursorBase, PageSize);
        else
            pATI->pCursorPage = xf86MapVidMem(iScreen, VIDMEM_FRAMEBUFFER,
                CursorBase, PageSize);

        if (!pATI->pCursorPage)
        {
            ATIUnmapCursor(iScreen, pATI);
            ATIUnmapMMIO(iScreen, pATI);
            ATIUnmapLinear(iScreen, pATI);

#ifndef AVOID_CPIO

            ATIUnmapVGA(iScreen, pATI);

#endif /* AVOID_CPIO */

            pATI->Mapped = FALSE;
            return FALSE;
        }

        pATI->pCursorImage = (char *)pATI->pCursorPage +
            (pATI->CursorBase - CursorBase);
    }

    return TRUE;
}

/*
 * ATIUnmapApertures --
 *
 * This function unmaps all apertures used by the driver.
 */
void
ATIUnmapApertures
(
    int    iScreen,
    ATIPtr pATI
)
{
    if (!pATI->Mapped)
        return;
    pATI->Mapped = FALSE;

    /* Unmap hardware cursor image area */
    ATIUnmapCursor(iScreen, pATI);

    /* Unmap MMIO area */
    ATIUnmapMMIO(iScreen, pATI);

    /* Unmap linear aperture */
    ATIUnmapLinear(iScreen, pATI);

#ifndef AVOID_CPIO

    /* Unmap VGA aperture */
    ATIUnmapVGA(iScreen, pATI);

#endif /* AVOID_CPIO */

}

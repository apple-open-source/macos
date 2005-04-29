/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/ati/atibank.c,v 1.13 2004/01/05 16:42:00 tsi Exp $ */
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
#include "atibank.h"
#include "atimach64io.h"
#include "atiwonderio.h"

#ifndef AVOID_CPIO

/*
 * ATI VGA Wonder V3 adapters use an ATI 18800 chip and are single-banked.
 * Bank selection is done with bits 0x1E of ATI extended VGA register index
 * 0xB2.
 */

/*
 * ATIV3SetBank --
 *
 * Set an ATI 18800's bank number.
 */
void
ATIV3SetBank
(
    ATIPtr       pATI,
    unsigned int iBank
)
{
    ATIModifyExtReg(pATI, 0xB2U, -1, (CARD8)(~0x1EU), SetBits(iBank, 0x1EU));
}

/*
 * ATIV3SetReadWrite --
 *
 * Set an ATI 18800's bank number.
 */
int
ATIV3SetReadWrite
(
    ScreenPtr    pScreen,
    unsigned int iBank
)
{
    ATIPtr pATI = ATIPTR(XF86SCRNINFO(pScreen));

    ATIModifyExtReg(pATI, 0xB2U, -1, (CARD8)(~0x1EU), SetBits(iBank, 0x1EU));
    return 0;
}

/*
 * ATI VGA Wonder V4 and V5 adapters use an ATI 18800-1 chip.  Bank selection
 * is done with ATI extended VGA register index 0xB2.  The format is:
 *
 *   0xE0 - Read bank select bits 0x07
 *   0x1E - Write bank select bits 0x0F
 *   0x01 - Read bank select bit 0x08.
 */

/*
 * ATIV4V5SetBank --
 *
 * Set an ATI 18800-1's read and write bank numbers.
 */
void
ATIV4V5SetBank
(
    ATIPtr       pATI,
    unsigned int iBank
)
{
    pATI->B2Reg = SetBits(iBank, 0x1EU) | SetBits(iBank, 0xE0U) |
        SetBits(GetBits(iBank, 0x08U), 0x01U);
    ATIPutExtReg(0xB2U, pATI->B2Reg);
}

/*
 * ATIV4V5SetRead --
 *
 * Set an ATI 18800-1's read bank number.
 */
int
ATIV4V5SetRead
(
    ScreenPtr    pScreen,
    unsigned int iBank
)
{
    ATIPtr pATI  = ATIPTR(XF86SCRNINFO(pScreen));
    CARD8  B2Reg = (pATI->B2Reg & 0x1EU) | SetBits(iBank, 0xE0U) |
                   SetBits(GetBits(iBank, 0x08U), 0x01U);

    if (B2Reg != pATI->B2Reg)
    {
        ATIPutExtReg(0xB2U, B2Reg);
        pATI->B2Reg = B2Reg;
    }

    return 0;
}

/*
 * ATIV4V5SetWrite --
 *
 * Set an ATI 18800-1's write bank number.
 */
int
ATIV4V5SetWrite
(
    ScreenPtr    pScreen,
    unsigned int iBank
)
{
    ATIPtr pATI = ATIPTR(XF86SCRNINFO(pScreen));
    CARD8  B2Reg = (pATI->B2Reg & 0xE1U) | SetBits(iBank, 0x1EU);

    if (B2Reg != pATI->B2Reg)
    {
        ATIPutExtReg(0xB2U, B2Reg);
        pATI->B2Reg = B2Reg;
    }
    return 0;
}

/*
 * ATIV4V5SetReadWrite --
 *
 * Set an ATI 18800-1's read and write bank numbers.
 */
int
ATIV4V5SetReadWrite
(
    ScreenPtr    pScreen,
    unsigned int iBank
)
{
    ATIV4V5SetBank(ATIPTR(XF86SCRNINFO(pScreen)), iBank);
    return 0;
}

/*
 * In addition to ATI extended register index 0xB2, 28800's, 68800's and
 * 88800's define banking bits in bits 0x0F of ATI extended VGA register index
 * 0xAE.  These are only needed for adapters with more than 1MB of video
 * memory, and it is questionable whether or not they are actually implemented
 * by 28800's and 88800's.  ATI extended VGA register index 0xAE is defined as
 * follows:
 *
 *    0xF0 - reserved
 *    0x0C - read bank select bits 0x30
 *    0x03 - write bank select bits 0x30
 */

/*
 * ATIx8800SetBank --
 *
 * Set an ATI 28800's, 68800's or 88800's read and write bank numbers.
 */
void
ATIx8800SetBank
(
    ATIPtr       pATI,
    unsigned int iBank
)
{
    ATIV4V5SetBank(pATI, iBank);
    iBank = GetBits(iBank, 0x30U);
    ATIModifyExtReg(pATI, 0xAEU, -1, (CARD8)(~0x0FU),
        SetBits(iBank, 0x03U) | SetBits(iBank, 0x0CU));
}

/*
 * ATIx8800SetRead --
 *
 * Set an ATI 28800's, 68800's or 88800's read bank numbers.
 */
int
ATIx8800SetRead
(
    ScreenPtr    pScreen,
    unsigned int iBank
)
{
    (void)ATIV4V5SetRead(pScreen, iBank);
    ATIModifyExtReg(ATIPTR(XF86SCRNINFO(pScreen)), 0xAEU, -1, (CARD8)(~0x0CU),
        SetBits(GetBits(iBank, 0x30U), 0x0CU));
    return 0;
}

/*
 * ATIx8800SetWrite --
 *
 * Set an ATI 28800's, 68800's or 88800's write bank numbers.
 */
int
ATIx8800SetWrite
(
    ScreenPtr    pScreen,
    unsigned int iBank
)
{
    (void)ATIV4V5SetWrite(pScreen, iBank);
    ATIModifyExtReg(ATIPTR(XF86SCRNINFO(pScreen)), 0xAEU, -1, (CARD8)(~0x03U),
        SetBits(GetBits(iBank, 0x30U), 0x03U));
    return 0;
}

/*
 * ATIx8800SetReadWrite --
 *
 * Set an ATI 28800's, 68800's or 88800's read and write bank numbers.
 */
int
ATIx8800SetReadWrite
(
    ScreenPtr    pScreen,
    unsigned int iBank
)
{
    ATIx8800SetBank(ATIPTR(XF86SCRNINFO(pScreen)), iBank);
    return 0;
}

/*
 * Functions to simulate a banked VGA aperture using a Mach64's small dual
 * paged apertures.  There are two sets of these:  one for packed modes, the
 * other for planar modes.
 */

static CARD32
ATIMach64MassagePackedBankNumber
(
    CARD8 iBank
)
{
    iBank <<= 1;
    return ((iBank + 1) << 16) | iBank;
}

/*
 * ATIMach64SetBankPacked --
 *
 * Set read and write bank numbers for small dual paged apertures.
 */
void
ATIMach64SetBankPacked
(
    ATIPtr       pATI,
    unsigned int iBank
)
{
    CARD32 tmp = ATIMach64MassagePackedBankNumber(iBank);

    outr(MEM_VGA_RP_SEL, tmp);
    outr(MEM_VGA_WP_SEL, tmp);
}

/*
 * ATIMach64SetReadPacked --
 *
 * Set read bank number for small dual paged apertures.
 */
int
ATIMach64SetReadPacked
(
    ScreenPtr    pScreen,
    unsigned int iBank
)
{
    ATIPtr pATI = ATIPTR(XF86SCRNINFO(pScreen));

    outr(MEM_VGA_RP_SEL, ATIMach64MassagePackedBankNumber(iBank));
    return 0;
}

/*
 * ATIMach64SetWritePacked --
 *
 * Set write bank number for small dual paged apertures.
 */
int
ATIMach64SetWritePacked
(
    ScreenPtr    pScreen,
    unsigned int iBank
)
{
    ATIPtr pATI = ATIPTR(XF86SCRNINFO(pScreen));

    outr(MEM_VGA_WP_SEL, ATIMach64MassagePackedBankNumber(iBank));
    return 0;
}

/*
 * ATIMach64SetReadWritePacked --
 *
 * Set read and write bank numbers for small dual paged apertures.
 */
int
ATIMach64SetReadWritePacked
(
    ScreenPtr    pScreen,
    unsigned int iBank
)
{
    ATIMach64SetBankPacked(ATIPTR(XF86SCRNINFO(pScreen)), iBank);
    return 0;
}

static CARD32
ATIMach64MassagePlanarBankNumber
(
    CARD8 iBank
)
{
    iBank <<= 3;
    return ((iBank + 4) << 16) | iBank;
}

/*
 * ATIMach64SetBankPlanar --
 *
 * Set read and write bank numbers for small dual paged apertures.
 */
void
ATIMach64SetBankPlanar
(
    ATIPtr       pATI,
    unsigned int iBank
)
{
    CARD32 tmp = ATIMach64MassagePlanarBankNumber(iBank);

    outr(MEM_VGA_RP_SEL, tmp);
    outr(MEM_VGA_WP_SEL, tmp);
}

/*
 * ATIMach64SetReadPlanar --
 *
 * Set read bank number for small dual paged apertures.
 */
int
ATIMach64SetReadPlanar
(
    ScreenPtr    pScreen,
    unsigned int iBank
)
{
    ATIPtr pATI = ATIPTR(XF86SCRNINFO(pScreen));

    outr(MEM_VGA_RP_SEL, ATIMach64MassagePlanarBankNumber(iBank));
    return 0;
}

/*
 * ATIMach64SetWritePlanar --
 *
 * Set write bank number for small dual paged apertures.
 */
int
ATIMach64SetWritePlanar
(
    ScreenPtr    pScreen,
    unsigned int iBank
)
{
    ATIPtr pATI = ATIPTR(XF86SCRNINFO(pScreen));

    outr(MEM_VGA_WP_SEL, ATIMach64MassagePlanarBankNumber(iBank));
    return 0;
}

/*
 * ATIMach64SetReadWritePlanar --
 *
 * Set read and write bank numbers for small dual paged apertures.
 */
int
ATIMach64SetReadWritePlanar
(
    ScreenPtr    pScreen,
    unsigned int iBank
)
{
    ATIMach64SetBankPlanar(ATIPTR(XF86SCRNINFO(pScreen)), iBank);
    return 0;
}

#endif /* AVOID_CPIO */

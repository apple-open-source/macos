/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/ati/atiadjust.c,v 1.16 2004/01/05 16:42:00 tsi Exp $ */
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
#include "atiadjust.h"
#include "atichip.h"
#include "aticrtc.h"
#include "atilock.h"
#include "atimach64io.h"
#include "atiwonderio.h"

/*
 * The display start address is expressed in units of 32-bit (VGA) or 64-bit
 * (accelerator) words where all planar modes are considered as 4bpp modes.
 * These functions ensure the start address does not exceed architectural
 * limits.  Also, to avoid colour changes while panning, these 32-bit or 64-bit
 * boundaries may not fall within a pixel.
 */

/*
 * ATIAjustPreInit --
 *
 * This function calculates values needed to speed up the setting of the
 * display start address.
 */
void
ATIAdjustPreInit
(
    ATIPtr      pATI
)
{
    unsigned long MaxBase;

#ifndef AVOID_CPIO

    if ((pATI->CPIO_VGAWonder) &&
        (pATI->Chip <= ATI_CHIP_18800_1) &&
        (pATI->VideoRAM == 256) &&
        (pATI->depth >= 8))
    {
        /* Strange, to say the least ... */
        pATI->AdjustDepth = (pATI->bitsPerPixel + 3) >> 2;
        pATI->AdjustMask = (unsigned long)(-32);
    }
    else

#endif /* AVOID_CPIO */

    {
        pATI->AdjustDepth = (pATI->bitsPerPixel + 7) >> 3;

        pATI->AdjustMask = 64;
        while (pATI->AdjustMask % (unsigned long)(pATI->AdjustDepth))
            pATI->AdjustMask += 64;
        pATI->AdjustMask =
            ~(((pATI->AdjustMask / (unsigned long)(pATI->AdjustDepth)) >> 3) -
              1);
    }

    switch (pATI->NewHW.crtc)
    {

#ifndef AVOID_CPIO

        case ATI_CRTC_VGA:
            if (pATI->Chip >= ATI_CHIP_264CT)
            {
                pATI->AdjustMaxBase = MaxBits(CRTC_OFFSET_VGA) << 2;
                if (pATI->depth <= 4)
                    pATI->AdjustMaxBase <<= 1;
            }
            else if (!pATI->CPIO_VGAWonder)
            {
                pATI->AdjustMaxBase = 0xFFFFU << 3;
            }
            else if (pATI->Chip <= ATI_CHIP_28800_6)
            {
                pATI->AdjustMaxBase = 0x03FFFFU << 3;
            }
            else /* Mach32 & Mach64 */
            {
                pATI->AdjustMaxBase = 0x0FFFFFU << 3;
            }
            break;

#endif /* AVOID_CPIO */

        case ATI_CRTC_MACH64:
            pATI->AdjustMaxBase = MaxBits(CRTC_OFFSET) << 3;
            break;

        default:
            pATI->AdjustMaxBase = 0;
            break;
    }

    MaxBase = (pATI->AdjustMaxBase / (unsigned long)pATI->AdjustDepth) |
        ~pATI->AdjustMask;

    pATI->AdjustMaxX = MaxBase % pATI->displayWidth;
    pATI->AdjustMaxY = MaxBase / pATI->displayWidth;
}

/*
 * ATIAdjustFrame --
 *
 * This function is used to initialise the SVGA Start Address - the first
 * displayed location in video memory.  This is used to implement the virtual
 * window.
 */
void
ATIAdjustFrame
(
    int scrnIndex,
    int x,
    int y,
    int flags
)
{
    ScrnInfoPtr pScreenInfo = xf86Screens[scrnIndex];
    ATIPtr      pATI = ATIPTR(pScreenInfo);
    int         Base, xy;

    /*
     * Assume the caller has already done its homework in ensuring the physical
     * screen is still contained in the virtual resolution.
     */
    if (y >= pATI->AdjustMaxY)
    {
        y = pATI->AdjustMaxY;
        if (x > pATI->AdjustMaxX)
            y--;
    }

    Base = ((((y * pATI->displayWidth) + x) & pATI->AdjustMask) *
            pATI->AdjustDepth) >> 3;

    if (!pATI->currentMode)
    {
        /*
         * Not in DGA.  This reverse-calculates pScreenInfo->frame[XY][01] so
         * that the cursor does not move on mode switches.
         */
        xy = (Base << 3) / pATI->AdjustDepth;
        pScreenInfo->frameX0 = xy % pATI->displayWidth;
        pScreenInfo->frameY0 = xy / pATI->displayWidth;
        pScreenInfo->frameX1 =
            pScreenInfo->frameX0 + pScreenInfo->currentMode->HDisplay - 1;
        pScreenInfo->frameY1 =
            pScreenInfo->frameY0 + pScreenInfo->currentMode->VDisplay - 1;
    }

    /* Unlock registers */
    ATIUnlock(pATI);

#ifndef AVOID_CPIO

    if ((pATI->NewHW.crtc == ATI_CRTC_VGA) && (pATI->Chip < ATI_CHIP_264CT))
    {
        PutReg(CRTX(pATI->CPIO_VGABase), 0x0CU, GetByte(Base, 1));
        PutReg(CRTX(pATI->CPIO_VGABase), 0x0DU, GetByte(Base, 0));

        if (pATI->CPIO_VGAWonder)
        {
            if (pATI->Chip <= ATI_CHIP_18800_1)
                ATIModifyExtReg(pATI, 0xB0U, -1, 0x3FU, Base >> 10);
            else
            {
                ATIModifyExtReg(pATI, 0xB0U, -1, 0xBFU, Base >> 10);
                ATIModifyExtReg(pATI, 0xA3U, -1, 0xEFU, Base >> 13);

                /*
                 * I don't know if this also applies to Mach64's, but give it a
                 * shot...
                 */
                if (pATI->Chip >= ATI_CHIP_68800)
                    ATIModifyExtReg(pATI, 0xADU, -1, 0xF3U, Base >> 16);
            }
        }
    }
    else
    /*
     * On integrated controllers, there is only one set of CRTC control bits,
     * many of which are simultaneously accessible through both VGA and
     * accelerator I/O ports.  Given VGA's architectural limitations, setting
     * the CRTC's offset register to more than 256k needs to be done through
     * the accelerator port.
     */
    if (pATI->depth <= 4)
    {
        outr(CRTC_OFF_PITCH, SetBits(pATI->displayWidth >> 4, CRTC_PITCH) |
            SetBits(Base, CRTC_OFFSET));
    }
    else

#endif /* AVOID_CPIO */

    {

#ifndef AVOID_CPIO

        if (pATI->NewHW.crtc == ATI_CRTC_VGA)
            Base <<= 1;                 /* LSBit must be zero */

#endif /* AVOID_CPIO */

        outr(CRTC_OFF_PITCH, SetBits(pATI->displayWidth >> 3, CRTC_PITCH) |
            SetBits(Base, CRTC_OFFSET));
    }
}

/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/ati/atimach64io.c,v 1.5 2003/01/01 19:16:32 tsi Exp $ */
/*
 * Copyright 2000 through 2003 by Marc Aurele La France (TSI @ UQV), tsi@xfree86.org
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
#include "atimach64io.h"

/*
 * ATIAccessMach64PLLReg --
 *
 * This function sets up the addressing required to access, for read or write,
 * a 264xT's PLL registers.
 */
void
ATIAccessMach64PLLReg
(
    ATIPtr      pATI,
    const CARD8 Index,
    const Bool  Write
)
{
    CARD8 clock_cntl1 = in8(CLOCK_CNTL + 1) &
        ~GetByte(PLL_WR_EN | PLL_ADDR, 1);

    /* Set PLL register to be read or written */
    out8(CLOCK_CNTL + 1, clock_cntl1 |
        GetByte(SetBits(Index, PLL_ADDR) | SetBits(Write, PLL_WR_EN), 1));
}

/*
 * ATIMach64PollEngineStatus --
 *
 * This function refreshes the driver's view of the draw engine's status.  This
 * has been moved into a separate compilation unit to prevent inlining.
 */
void
ATIMach64PollEngineStatus
(
    ATIPtr pATI
)
{
    CARD32 IOValue;
    int    Count;

    if (pATI->Chip < ATI_CHIP_264VTB)
    {
        /*
         * TODO:  Deal with locked engines.
         */
        IOValue = inm(FIFO_STAT);
        pATI->EngineIsLocked = GetBits(IOValue, FIFO_ERR);

        /*
         * The following counts the number of bits in FIFO_STAT_BITS, and is
         * derived from miSetVisualTypes() (formerly cfbSetVisualTypes()).
         */
        IOValue = GetBits(IOValue, FIFO_STAT_BITS);
        Count = (IOValue >> 1) & 0x36DBU;
        Count = IOValue - Count - ((Count >> 1) & 0x36DBU);
        Count = ((Count + (Count >> 3)) & 0x71C7U) % 0x3FU;
        Count = pATI->nFIFOEntries - Count;
        if (Count > pATI->nAvailableFIFOEntries)
            pATI->nAvailableFIFOEntries = Count;

        /*
         * If the command FIFO is non-empty, then the engine isn't idle.
         */
        if (pATI->nAvailableFIFOEntries < pATI->nFIFOEntries)
        {
            pATI->EngineIsBusy = TRUE;
            return;
        }
    }

    IOValue = inm(GUI_STAT);
    pATI->EngineIsBusy = GetBits(IOValue, GUI_ACTIVE);
    Count = GetBits(IOValue, GUI_FIFO);
    if (Count > pATI->nAvailableFIFOEntries)
        pATI->nAvailableFIFOEntries = Count;
}

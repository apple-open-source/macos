/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/ati/atiprint.c,v 1.29 2004/01/05 16:42:04 tsi Exp $ */
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
#include "atichip.h"
#include "atidac.h"
#include "atimach64io.h"
#include "atiprint.h"
#include "atiwonderio.h"

/*
 * ATIPrintBIOS --
 *
 * Display various parts of the BIOS when the server is invoked with -verbose.
 */
void
ATIPrintBIOS
(
    const CARD8        *BIOS,
    const unsigned int Length   /* A multiple of 512 */
)
{
    unsigned char *Char = NULL;
    unsigned int  Index;
    unsigned char Printable[17];

    if (xf86GetVerbosity() <= 4)
        return;

    (void)memset(Printable, 0, SizeOf(Printable));

    xf86ErrorFVerb(5, "\n BIOS image:");

    for (Index = 0;  Index < Length;  Index++)
    {
        if (!(Index & (4U - 1U)))
        {
            if (!(Index & (16U - 1U)))
            {
                if (Printable[0])
                    xf86ErrorFVerb(5, "  |%s|", Printable);
                Char = Printable;
                xf86ErrorFVerb(5, "\n 0x%08X: ", Index);
            }
            xf86ErrorFVerb(5, " ");
        }
        xf86ErrorFVerb(5, "%02X", BIOS[Index]);
        if (isprint(BIOS[Index]))
            *Char++ = BIOS[Index];
        else
            *Char++ = '.';
    }

    xf86ErrorFVerb(5, "  |%s|\n", Printable);
}

#ifndef AVOID_CPIO

/*
 * ATIPrintIndexedRegisters --
 *
 * Display a set of indexed byte-size registers when the server is invoked with
 * -verbose.
 */
static void
ATIPrintIndexedRegisters
(
    const IOADDRESS Port,
    const CARD8     StartIndex,
    const CARD8     EndIndex,
    const char      *Name,
    const IOADDRESS GenS1
)
{
    int Index;

    xf86ErrorFVerb(4, "\n %s register values:", Name);
    for (Index = StartIndex;  Index < EndIndex;  Index++)
    {
        if (!(Index & (4U - 1U)))
        {
            if (!(Index & (16U - 1U)))
                xf86ErrorFVerb(4, "\n 0x%02X: ", Index);
            xf86ErrorFVerb(4, " ");
        }
        if (Port == ATTRX)
            (void)inb(GenS1);           /* Reset flip-flop */
        xf86ErrorFVerb(4, "%02X", GetReg(Port, Index));
    }

    if (Port == ATTRX)
    {
        (void)inb(GenS1);               /* Reset flip-flop */
        outb(ATTRX, 0x20U);             /* Turn on PAS bit */
    }

    xf86ErrorFVerb(4, "\n");
}

#endif /* AVOID_CPIO */

/*
 * ATIMach64PrintRegisters --
 *
 * Display a Mach64's main register bank when the server is invoked with
 * -verbose.
 */
static void
ATIMach64PrintRegisters
(
    ATIPtr     pATI,
    CARD8      *crtc,
    const char *Description
)
{
    CARD32 IOValue;
    CARD8 dac_read, dac_mask, dac_data, dac_write;
    int Index, Limit;

#ifndef AVOID_CPIO

    int Step;

#endif /* AVOID_CPIO */

    xf86ErrorFVerb(4, "\n Mach64 %s register values:", Description);

#ifdef AVOID_CPIO

    if (pATI->pBlock[1])
        Limit = DWORD_SELECT;
    else
        Limit = MM_IO_SELECT;

    for (Index = 0;  Index <= Limit;  Index += UnitOf(MM_IO_SELECT))
    {
        if (!(Index & SetBits(3, MM_IO_SELECT)))
            xf86ErrorFVerb(4, "\n 0x%04X: ", Index);
        if (Index == (DAC_REGS & DWORD_SELECT))
        {
            dac_read = in8(DAC_REGS + 3);
            DACDelay;
            dac_mask = in8(DAC_REGS + 2);
            DACDelay;
            dac_data = in8(DAC_REGS + 1);
            DACDelay;
            dac_write = in8(DAC_REGS + 0);
            DACDelay;

            xf86ErrorFVerb(4, " %02X%02X%02X%02X",
                dac_read, dac_mask, dac_data, dac_write);

            out8(DAC_REGS + 2, dac_mask);
            DACDelay;
            out8(DAC_REGS + 3, dac_read);
            DACDelay;
        }
        else
        {
            IOValue = inm(Index);

            if ((Index == (CRTC_GEN_CNTL & DWORD_SELECT)) &&
                (IOValue & CRTC_EXT_DISP_EN))
                *crtc = ATI_CRTC_MACH64;

            xf86ErrorFVerb(4, " %08lX", (unsigned long)IOValue);
        }
    }

#else /* AVOID_CPIO */

    Limit = ATIIOPort(IOPortTag(0x1FU, 0x3FU));
    Step = ATIIOPort(IOPortTag(0x01U, 0x01U)) - pATI->CPIOBase;
    for (Index = pATI->CPIOBase;  Index <= Limit;  Index += Step)
    {
        if (!(((Index - pATI->CPIOBase) / Step) & 0x03U))
            xf86ErrorFVerb(4, "\n 0x%04X: ", Index);
        if (Index == (int)ATIIOPort(DAC_REGS))
        {
            dac_read = in8(DAC_REGS + 3);
            DACDelay;
            dac_mask = in8(DAC_REGS + 2);
            DACDelay;
            dac_data = in8(DAC_REGS + 1);
            DACDelay;
            dac_write = in8(DAC_REGS + 0);
            DACDelay;

            xf86ErrorFVerb(4, " %02X%02X%02X%02X",
                dac_read, dac_mask, dac_data, dac_write);

            out8(DAC_REGS + 2, dac_mask);
            DACDelay;
            out8(DAC_REGS + 3, dac_read);
            DACDelay;
        }
        else
        {
            IOValue = inl(Index);

            if ((Index == (int)ATIIOPort(CRTC_GEN_CNTL)) &&
                (IOValue & CRTC_EXT_DISP_EN))
                *crtc = ATI_CRTC_MACH64;

            xf86ErrorFVerb(4, " %08lX", (unsigned long)IOValue);
        }
    }

#endif /* AVOID_CPIO */

    xf86ErrorFVerb(4, "\n");
}

/*
 * ATIMach64PrintPLLRegisters --
 *
 * Display an integrated Mach64's PLL registers when the server is invoked with
 * -verbose.
 */
static void
ATIMach64PrintPLLRegisters
(
    ATIPtr pATI
)
{
    int Index, Limit;
    CARD8 PLLReg[MaxBits(PLL_ADDR) + 1];

    for (Limit = 0;  Limit < SizeOf(PLLReg);  Limit++)
        PLLReg[Limit] = ATIMach64GetPLLReg(Limit);

    /* Determine how many PLL registers there really are */
    while ((Limit = Limit >> 1))
        for (Index = 0;  Index < Limit;  Index++)
            if (PLLReg[Index] != PLLReg[Index + Limit])
                goto FoundLimit;
FoundLimit:
    Limit <<= 1;

    xf86ErrorFVerb(4, "\n Mach64 PLL register values:");
    for (Index = 0;  Index < Limit;  Index++)
    {
        if (!(Index & 3))
        {
            if (!(Index & 15))
                xf86ErrorFVerb(4, "\n 0x%02X: ", Index);
            xf86ErrorFVerb(4, " ");
        }
        xf86ErrorFVerb(4, "%02X", PLLReg[Index]);
    }

    xf86ErrorFVerb(4, "\n");
}

/*
 * ATIRGB514PrintRegisters --
 *
 * Display IBM RGB 514 registers when the server is invoked with -verbose.
 */
static void
ATIRGB514PrintRegisters
(
    ATIPtr pATI
)
{
    CARD32 crtc_gen_cntl, dac_cntl;
    CARD8  index_lo, index_hi, index_ctl;
    int    Index;

    /* Temporarily switch to Mach64 CRTC */
    crtc_gen_cntl = inr(CRTC_GEN_CNTL);
    if (!(crtc_gen_cntl & CRTC_EXT_DISP_EN))
        outr(CRTC_GEN_CNTL, crtc_gen_cntl | CRTC_EXT_DISP_EN);

    /* Temporarily switch to IBM RGB 514 registers */
    dac_cntl = inr(DAC_CNTL);
    outr(DAC_CNTL, (dac_cntl & ~DAC_EXT_SEL_RS3) | DAC_EXT_SEL_RS2);

    index_lo = in8(M64_DAC_WRITE);
    index_hi = in8(M64_DAC_DATA);
    index_ctl = in8(M64_DAC_READ);

    out8(M64_DAC_WRITE, 0x00U);
    out8(M64_DAC_DATA, 0x00U);
    out8(M64_DAC_READ, 0x01U);  /* Auto-increment */

    xf86ErrorFVerb(4, "\n IBM RGB 514 registers:");
    for (Index = 0;  Index < 0x0800;  Index++)
    {
        if (!(Index & 3))
        {
            if (!(Index & 15))
            {
                xf86ErrorFVerb(4, "\n 0x%04X: ", Index);

                /* Need to rewrite index every so often... */
                if ((Index == 0x0100) || (Index == 0x0500))
                {
                    out8(M64_DAC_WRITE, 0x00U);
                    out8(M64_DAC_DATA, Index >> 8);
                }
            }

            xf86ErrorFVerb(4, " ");
        }

        xf86ErrorFVerb(4, "%02X", in8(M64_DAC_MASK));
    }

    /* Restore registers */
    out8(M64_DAC_WRITE, index_lo);
    out8(M64_DAC_DATA, index_hi);
    out8(M64_DAC_READ, index_ctl);
    outr(DAC_CNTL, dac_cntl);
    if (!(crtc_gen_cntl & CRTC_EXT_DISP_EN))
        outr(CRTC_GEN_CNTL, crtc_gen_cntl);

    xf86ErrorFVerb(4, "\n");
}

/*
 * ATIPrintRegisters --
 *
 * Display various registers when the server is invoked with -verbose.
 */
void
ATIPrintRegisters
(
    ATIPtr pATI
)
{
    pciVideoPtr  pVideo;
    pciConfigPtr pPCI;
    int          Index;
    CARD32       lcd_index, tv_out_index, lcd_gen_ctrl;
    CARD8        dac_read, dac_mask, dac_write;
    CARD8        crtc;

#ifndef AVOID_CPIO

    CARD8 genmo, seq1 = 0;

    crtc = ATI_CRTC_VGA;

    if (pATI->VGAAdapter != ATI_ADAPTER_NONE)
    {
        xf86ErrorFVerb(4, "\n Miscellaneous output register value:  0x%02X.\n",
            genmo = inb(R_GENMO));

        if (genmo & 0x01U)
        {
            if (pATI->Chip == ATI_CHIP_264LT)
            {
                lcd_gen_ctrl = inr(LCD_GEN_CTRL);

                outr(LCD_GEN_CTRL, lcd_gen_ctrl & ~SHADOW_RW_EN);
                ATIPrintIndexedRegisters(CRTX(ColourIOBase), 0, 64,
                    "Non-shadow colour CRT controller", 0);

                outr(LCD_GEN_CTRL, lcd_gen_ctrl | SHADOW_RW_EN);
                ATIPrintIndexedRegisters(CRTX(ColourIOBase), 0, 64,
                    "Shadow colour CRT controller", 0);

                outr(LCD_GEN_CTRL, lcd_gen_ctrl);
            }
            else if ((pATI->Chip == ATI_CHIP_264LTPRO) ||
                     (pATI->Chip == ATI_CHIP_264XL) ||
                     (pATI->Chip == ATI_CHIP_MOBILITY))
            {
                lcd_index = inr(LCD_INDEX);
                lcd_gen_ctrl = ATIMach64GetLCDReg(LCD_GEN_CNTL);

                ATIMach64PutLCDReg(LCD_GEN_CNTL,
                    lcd_gen_ctrl & ~(CRTC_RW_SELECT | SHADOW_RW_EN));
                ATIPrintIndexedRegisters(CRTX(ColourIOBase), 0, 64,
                    "Non-shadow colour CRT controller", 0);

                ATIMach64PutLCDReg(LCD_GEN_CNTL,
                    (lcd_gen_ctrl & ~CRTC_RW_SELECT) | SHADOW_RW_EN);
                ATIPrintIndexedRegisters(CRTX(ColourIOBase), 0, 64,
                    "Shadow colour CRT controller", 0);

                ATIMach64PutLCDReg(LCD_GEN_CNTL, lcd_gen_ctrl);
                outr(LCD_INDEX, lcd_index);
            }
            else
            {
                ATIPrintIndexedRegisters(CRTX(ColourIOBase), 0, 64,
                    "Colour CRT controller", 0);
            }

            ATIPrintIndexedRegisters(ATTRX, 0, 32, "Attribute controller",
                GENS1(ColourIOBase));
        }
        else
        {
            if (pATI->Chip == ATI_CHIP_264LT)
            {
                lcd_gen_ctrl = inr(LCD_GEN_CTRL);

                outr(LCD_GEN_CTRL, lcd_gen_ctrl & ~SHADOW_RW_EN);
                ATIPrintIndexedRegisters(CRTX(MonochromeIOBase), 0, 64,
                    "Non-shadow monochrome CRT controller", 0);

                outr(LCD_GEN_CTRL, lcd_gen_ctrl | SHADOW_RW_EN);
                ATIPrintIndexedRegisters(CRTX(MonochromeIOBase), 0, 64,
                    "Shadow monochrome CRT controller", 0);

                outr(LCD_GEN_CTRL, lcd_gen_ctrl);
            }
            else if ((pATI->Chip == ATI_CHIP_264LTPRO) ||
                     (pATI->Chip == ATI_CHIP_264XL) ||
                     (pATI->Chip == ATI_CHIP_MOBILITY))
            {
                lcd_index = inr(LCD_INDEX);
                lcd_gen_ctrl = ATIMach64GetLCDReg(LCD_GEN_CNTL);

                ATIMach64PutLCDReg(LCD_GEN_CNTL,
                    lcd_gen_ctrl & ~(CRTC_RW_SELECT | SHADOW_RW_EN));
                ATIPrintIndexedRegisters(CRTX(MonochromeIOBase), 0, 64,
                    "Non-shadow monochrome CRT controller", 0);

                ATIMach64PutLCDReg(LCD_GEN_CNTL,
                    (lcd_gen_ctrl & ~CRTC_RW_SELECT) | SHADOW_RW_EN);
                ATIPrintIndexedRegisters(CRTX(MonochromeIOBase), 0, 64,
                    "Shadow monochrome CRT controller", 0);

                ATIMach64PutLCDReg(LCD_GEN_CNTL, lcd_gen_ctrl);
                outr(LCD_INDEX, lcd_index);
            }
            else
            {
                ATIPrintIndexedRegisters(CRTX(MonochromeIOBase), 0, 64,
                    "Monochrome CRT controller", 0);
            }

            ATIPrintIndexedRegisters(ATTRX, 0, 32, "Attribute controller",
                GENS1(MonochromeIOBase));
        }

        ATIPrintIndexedRegisters(GRAX, 0, 16, "Graphics controller", 0);
        ATIPrintIndexedRegisters(SEQX, 0, 8, "Sequencer", 0);

        if (pATI->CPIO_VGAWonder)
            ATIPrintIndexedRegisters(pATI->CPIO_VGAWonder,
                xf86ServerIsOnlyProbing() ? 0x80U : pATI->VGAOffset, 0xC0U,
                "ATI extended VGA", 0);
    }

    if (pATI->ChipHasSUBSYS_CNTL)
    {
        xf86ErrorFVerb(4, "\n 8514/A register values:");
        for (Index = 0x02E8U;  Index <= 0x0FEE8;  Index += 0x0400U)
        {
            if (!((Index - 0x02E8U) & 0x0C00U))
                xf86ErrorFVerb(4, "\n 0x%04X: ", Index);
            xf86ErrorFVerb(4, " %04X", inw(Index));
        }

        if (pATI->Adapter >= ATI_ADAPTER_MACH8)
        {
            xf86ErrorFVerb(4, "\n\n Mach8/Mach32 register values:");
            for (Index = 0x02EEU;  Index <= 0x0FEEE;  Index += 0x0400U)
            {
                if (!((Index - 0x02EEU) & 0x0C00U))
                    xf86ErrorFVerb(4, "\n 0x%04X: ", Index);
                xf86ErrorFVerb(4, " %04X", inw(Index));
            }
        }

        xf86ErrorFVerb(4, "\n");
    }
    else

#endif /* AVOID_CPIO */

    if (pATI->Chip == ATI_CHIP_264LT)
    {
        lcd_gen_ctrl = inr(LCD_GEN_CTRL);

        outr(LCD_GEN_CTRL, lcd_gen_ctrl & ~SHADOW_RW_EN);
        ATIMach64PrintRegisters(pATI, &crtc, "non-shadow");

        outr(LCD_GEN_CTRL, lcd_gen_ctrl | SHADOW_RW_EN);
        ATIMach64PrintRegisters(pATI, &crtc, "shadow");

        outr(LCD_GEN_CTRL, lcd_gen_ctrl);

        ATIMach64PrintPLLRegisters(pATI);
    }
    else if ((pATI->Chip == ATI_CHIP_264LTPRO) ||
             (pATI->Chip == ATI_CHIP_264XL) ||
             (pATI->Chip == ATI_CHIP_MOBILITY))
    {
        lcd_index = inr(LCD_INDEX);
        lcd_gen_ctrl = ATIMach64GetLCDReg(LCD_GEN_CNTL);

        ATIMach64PutLCDReg(LCD_GEN_CNTL,
            lcd_gen_ctrl & ~(CRTC_RW_SELECT | SHADOW_RW_EN));
        ATIMach64PrintRegisters(pATI, &crtc, "non-shadow");

        ATIMach64PutLCDReg(LCD_GEN_CNTL,
            (lcd_gen_ctrl & ~CRTC_RW_SELECT) | SHADOW_RW_EN);
        ATIMach64PrintRegisters(pATI, &crtc, "shadow");

        if (pATI->Chip != ATI_CHIP_264XL)
        {
            ATIMach64PutLCDReg(LCD_GEN_CNTL, lcd_gen_ctrl | CRTC_RW_SELECT);
            ATIMach64PrintRegisters(pATI, &crtc, "secondary");
        }

        ATIMach64PutLCDReg(LCD_GEN_CNTL, lcd_gen_ctrl);

        ATIMach64PrintPLLRegisters(pATI);

        xf86ErrorFVerb(4, "\n LCD register values:");
        for (Index = 0;  Index < 64;  Index++)
        {
            if (!(Index & 3))
                xf86ErrorFVerb(4, "\n 0x%02X: ", Index);
            xf86ErrorFVerb(4, " %08X", ATIMach64GetLCDReg(Index));
        }

        outr(LCD_INDEX, lcd_index);

        tv_out_index = inr(TV_OUT_INDEX);

        xf86ErrorFVerb(4, "\n\n TV_OUT register values:");
        for (Index = 0;  Index < 256;  Index++)
        {
            if (!(Index & 3))
                xf86ErrorFVerb(4, "\n 0x%02X: ", Index);
            xf86ErrorFVerb(4, " %08X", ATIMach64GetTVReg(Index));
        }

        outr(TV_OUT_INDEX, tv_out_index);

        xf86ErrorFVerb(4, "\n");
    }
    else

#ifndef AVOID_CPIO

    if (pATI->Chip >= ATI_CHIP_88800GXC)

#endif /* AVOID_CPIO */

    {

#ifdef AVOID_CPIO

        ATIMach64PrintRegisters(pATI, &crtc, "MMIO");

#else /* AVOID_CPIO */

        ATIMach64PrintRegisters(pATI, &crtc,
            (pATI->CPIODecoding == SPARSE_IO) ? "sparse" : "block");

#endif /* AVOID_CPIO */

        if (pATI->Chip >= ATI_CHIP_264CT)
            ATIMach64PrintPLLRegisters(pATI);

        if (pATI->DAC == ATI_DAC_IBMRGB514)
            ATIRGB514PrintRegisters(pATI);
    }

#ifdef AVOID_CPIO

    dac_read = in8(M64_DAC_READ);
    DACDelay;
    dac_write = in8(M64_DAC_WRITE);
    DACDelay;
    dac_mask = in8(M64_DAC_MASK);
    DACDelay;

    xf86ErrorFVerb(4, "\n"
               " DAC read index:   0x%02X\n"
               " DAC write index:  0x%02X\n"
               " DAC mask:         0x%02X\n\n"
               " DAC colour lookup table:",
        dac_read, dac_write, dac_mask);

    out8(M64_DAC_MASK, 0xFFU);
    DACDelay;
    out8(M64_DAC_READ, 0x00U);
    DACDelay;

    for (Index = 0;  Index < 256;  Index++)
    {
        if (!(Index & 3))
            xf86ErrorFVerb(4, "\n 0x%02X:", Index);
        xf86ErrorFVerb(4, "  %02X", in8(M64_DAC_DATA));
        DACDelay;
        xf86ErrorFVerb(4, " %02X", in8(M64_DAC_DATA));
        DACDelay;
        xf86ErrorFVerb(4, " %02X", in8(M64_DAC_DATA));
        DACDelay;
    }

    out8(M64_DAC_MASK, dac_mask);
    DACDelay;
    out8(M64_DAC_READ, dac_read);
    DACDelay;

#else /* AVOID_CPIO */

    ATISetDACIOPorts(pATI, crtc);

    /* Temporarily turn off CLKDIV2 while reading DAC's LUT */
    if (pATI->Adapter == ATI_ADAPTER_NONISA)
    {
        seq1 = GetReg(SEQX, 0x01U);
        if (seq1 & 0x08U)
            PutReg(SEQX, 0x01U, seq1 & ~0x08U);
    }

    dac_read = inb(pATI->CPIO_DAC_READ);
    DACDelay;
    dac_write = inb(pATI->CPIO_DAC_WRITE);
    DACDelay;
    dac_mask = inb(pATI->CPIO_DAC_MASK);
    DACDelay;

    xf86ErrorFVerb(4, "\n"
               " DAC read index:   0x%02X\n"
               " DAC write index:  0x%02X\n"
               " DAC mask:         0x%02X\n\n"
               " DAC colour lookup table:",
        dac_read, dac_write, dac_mask);

    outb(pATI->CPIO_DAC_MASK, 0xFFU);
    DACDelay;
    outb(pATI->CPIO_DAC_READ, 0x00U);
    DACDelay;

    for (Index = 0;  Index < 256;  Index++)
    {
        if (!(Index & 3))
            xf86ErrorFVerb(4, "\n 0x%02X:", Index);
        xf86ErrorFVerb(4, "  %02X", inb(pATI->CPIO_DAC_DATA));
        DACDelay;
        xf86ErrorFVerb(4, " %02X", inb(pATI->CPIO_DAC_DATA));
        DACDelay;
        xf86ErrorFVerb(4, " %02X", inb(pATI->CPIO_DAC_DATA));
        DACDelay;
    }

    outb(pATI->CPIO_DAC_MASK, dac_mask);
    DACDelay;
    outb(pATI->CPIO_DAC_READ, dac_read);
    DACDelay;

    if ((pATI->Adapter == ATI_ADAPTER_NONISA) && (seq1 & 0x08U))
        PutReg(SEQX, 0x01U, seq1);

#endif /* AVOID_CPIO */

    if ((pVideo = pATI->PCIInfo))
    {
        pPCI = pVideo->thisCard;
        xf86ErrorFVerb(4, "\n\n PCI configuration register values:");
        for (Index = 0;  Index < 256;  Index+= 4)
        {
            if (!(Index & 15))
                xf86ErrorFVerb(4, "\n 0x%02X: ", Index);
            xf86ErrorFVerb(4, " 0x%08lX",
			   (unsigned long)pciReadLong(pPCI->tag, Index));
        }
    }

    xf86ErrorFVerb(4, "\n");

#ifndef AVOID_CPIO

    if (pATI->pBank)
        xf86ErrorFVerb(4, "\n Banked aperture at 0x%0lX.",
            (unsigned long)pATI->pBank);
    else
        xf86ErrorFVerb(4, "\n No banked aperture.");

    if (pATI->pMemory == pATI->pBank)
    {
        xf86ErrorFVerb(4, "\n No linear aperture.\n");
    }
    else

#else /* AVOID_CPIO */

    if (pATI->pMemory)

#endif /* AVOID_CPIO */

    {
        xf86ErrorFVerb(4, "\n Linear aperture at %p.\n", pATI->pMemory);
    }

    if (pATI->pBlock[0])
    {
        xf86ErrorFVerb(4, " Block 0 aperture at %p.\n", pATI->pBlock[0]);
        if (inr(CONFIG_CHIP_ID) == pATI->config_chip_id)
            xf86ErrorFVerb(4, " MMIO registers are correctly mapped.\n");
        else
            xf86ErrorFVerb(4, " MMIO mapping is in error!\n");
        if (pATI->pBlock[1])
            xf86ErrorFVerb(4, " Block 1 aperture at %p.\n",
                pATI->pBlock[1]);
    }
    else
    {
        xf86ErrorFVerb(4, " No MMIO aperture.\n");
    }

    if (pATI->pCursorImage)
        xf86ErrorFVerb(4, " Hardware cursor image aperture at %p.\n",
            pATI->pCursorImage);
    else
        xf86ErrorFVerb(4, " No hardware cursor image aperture.\n");

    xf86ErrorFVerb(4, "\n");
}

/*
 * A table to associate mode attributes with character strings.
 */
static const SymTabRec ModeAttributeNames[] =
{
    {V_PHSYNC,    "+hsync"},
    {V_NHSYNC,    "-hsync"},
    {V_PVSYNC,    "+vsync"},
    {V_NVSYNC,    "-vsync"},
    {V_PCSYNC,    "+csync"},
    {V_NCSYNC,    "-csync"},
    {V_INTERLACE, "interlace"},
    {V_DBLSCAN,   "doublescan"},
    {V_CSYNC,     "composite"},
    {V_DBLCLK,    "dblclk"},
    {V_CLKDIV2,   "clkdiv2"},
    {0,           NULL}
};

/*
 * ATIPrintMode --
 *
 * This function displays a mode's timing information.
 */
void
ATIPrintMode
(
    DisplayModePtr pMode
)
{
    const SymTabRec *pSymbol  = ModeAttributeNames;
    int             flags     = pMode->Flags;
    double          mClock, hSync, vRefresh;

    mClock = (double)pMode->SynthClock;
    if (pMode->HSync > 0.0)
        hSync = pMode->HSync;
    else
        hSync = mClock / pMode->HTotal;
    if (pMode->VRefresh > 0.0)
    {
        vRefresh = pMode->VRefresh;
    }
    else
    {
        vRefresh = (hSync * 1000.0) / pMode->VTotal;
        if (flags & V_INTERLACE)
            vRefresh *= 2.0;
        if (flags & V_DBLSCAN)
            vRefresh /= 2.0;
        if (pMode->VScan > 1)
            vRefresh /= pMode->VScan;
    }

    xf86ErrorFVerb(4, " Dot clock:           %7.3f MHz\n", mClock / 1000.0);
    xf86ErrorFVerb(4, " Horizontal sync:     %7.3f kHz\n", hSync);
    xf86ErrorFVerb(4, " Vertical refresh:    %7.3f Hz (%s)\n", vRefresh,
        (flags & V_INTERLACE) ? "I" : "NI");
    if ((pMode->ClockIndex >= 0) && (pMode->ClockIndex < MAXCLOCKS))
        xf86ErrorFVerb(4, " Clock index:         %d\n", pMode->ClockIndex);
    xf86ErrorFVerb(4, " Horizontal timings:  %4d %4d %4d %4d\n"
                      " Vertical timings:    %4d %4d %4d %4d\n",
        pMode->HDisplay, pMode->HSyncStart, pMode->HSyncEnd, pMode->HTotal,
        pMode->VDisplay, pMode->VSyncStart, pMode->VSyncEnd, pMode->VTotal);

    if (flags & V_HSKEW)
    {
        flags &= ~V_HSKEW;
        xf86ErrorFVerb(4, " Horizontal skew:     %4d\n", pMode->HSkew);
    }

    if (pMode->VScan >= 1)
        xf86ErrorFVerb(4, " Vertical scan:       %4d\n", pMode->VScan);

    xf86ErrorFVerb(4, " Flags:              ");
    for (;  pSymbol->token;  pSymbol++)
    {
        if (flags & pSymbol->token)
        {
            xf86ErrorFVerb(4, " %s", pSymbol->name);
            flags &= ~pSymbol->token;
            if (!flags)
                break;
        }
    }

    xf86ErrorFVerb(4, "\n");
}

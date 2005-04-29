/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/ati/atilock.c,v 1.21 2004/02/24 16:51:22 tsi Exp $ */
/*
 * Copyright 1999 through 2004 by Marc Aurele La France (TSI @ UQV), tsi@xfree86.org
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
#include "atilock.h"
#include "atimach64io.h"
#include "atiwonderio.h"

/*
 * ATIUnlock --
 *
 * This function is entered to unlock registers and disable unwanted
 * emulations.  It saves the current state for later restoration by ATILock().
 */
void
ATIUnlock
(
    ATIPtr pATI
)
{
    CARD32 tmp;

#ifndef AVOID_CPIO

    CARD32 saved_lcd_gen_ctrl = 0, lcd_gen_ctrl = 0;

#endif /* AVOID_CPIO */

    if (pATI->Unlocked)
        return;
    pATI->Unlocked = TRUE;

#ifndef AVOID_CPIO

    if (pATI->ChipHasSUBSYS_CNTL)
    {
        /* Save register values to be modified */
        pATI->LockData.clock_sel = inw(CLOCK_SEL);
        if (pATI->Chip >= ATI_CHIP_68800)
        {
            pATI->LockData.misc_options = inw(MISC_OPTIONS);
            pATI->LockData.mem_bndry = inw(MEM_BNDRY);
            pATI->LockData.mem_cfg = inw(MEM_CFG);
        }

        tmp = inw(SUBSYS_STAT) & _8PLANE;

        /* Reset the 8514/A and disable all interrupts */
        outw(SUBSYS_CNTL, tmp | (GPCTRL_RESET | CHPTEST_NORMAL));
        outw(SUBSYS_CNTL, tmp | (GPCTRL_ENAB | CHPTEST_NORMAL | RVBLNKFLG |
            RPICKFLAG | RINVALIDIO | RGPIDLE));

        /* Ensure VGA is enabled */
        outw(CLOCK_SEL, pATI->LockData.clock_sel &~DISABPASSTHRU);
        if (pATI->Chip >= ATI_CHIP_68800)
        {
            outw(MISC_OPTIONS, pATI->LockData.misc_options &
                ~(DISABLE_VGA | DISABLE_DAC));

            /* Disable any video memory boundary */
            outw(MEM_BNDRY, pATI->LockData.mem_bndry &
                ~(MEM_PAGE_BNDRY | MEM_BNDRY_ENA));

            /* Disable direct video memory aperture */
            outw(MEM_CFG, pATI->LockData.mem_cfg &
                ~(MEM_APERT_SEL | MEM_APERT_PAGE | MEM_APERT_LOC));
        }

        /* Wait for all activity to die down */
        ProbeWaitIdleEmpty();
    }
    else if (pATI->Chip >= ATI_CHIP_88800GXC)

#endif /* AVOID_CPIO */

    {
        /* Reset everything */
        pATI->LockData.bus_cntl = inr(BUS_CNTL);
        if (pATI->Chip < ATI_CHIP_264VT4)
        {
            pATI->LockData.bus_cntl =
                (pATI->LockData.bus_cntl & ~BUS_HOST_ERR_INT_EN) |
                BUS_HOST_ERR_INT;
            if (pATI->Chip < ATI_CHIP_264VTB)
                pATI->LockData.bus_cntl =
                    (pATI->LockData.bus_cntl & ~BUS_FIFO_ERR_INT_EN) |
                        BUS_FIFO_ERR_INT;
        }
        tmp = pATI->LockData.bus_cntl & ~BUS_ROM_DIS;
        if (pATI->Chip < ATI_CHIP_264VTB)
            tmp |= SetBits(15, BUS_FIFO_WS);
        else
            tmp &= ~BUS_MASTER_DIS;
        if (pATI->Chip >= ATI_CHIP_264VT)
            tmp |= BUS_EXT_REG_EN;              /* Enable Block 1 */
        outr(BUS_CNTL, tmp);
        pATI->LockData.crtc_int_cntl = inr(CRTC_INT_CNTL);
        outr(CRTC_INT_CNTL, (pATI->LockData.crtc_int_cntl & ~CRTC_INT_ENS) |
            CRTC_INT_ACKS);
        pATI->LockData.gen_test_cntl = inr(GEN_TEST_CNTL) &
            (GEN_OVR_OUTPUT_EN | GEN_OVR_POLARITY | GEN_CUR_EN |
                GEN_BLOCK_WR_EN);
        tmp = pATI->LockData.gen_test_cntl & ~GEN_CUR_EN;
        outr(GEN_TEST_CNTL, tmp | GEN_GUI_EN);
        outr(GEN_TEST_CNTL, tmp);
        outr(GEN_TEST_CNTL, tmp | GEN_GUI_EN);
        tmp = pATI->LockData.crtc_gen_cntl = inr(CRTC_GEN_CNTL) &
            ~(CRTC_EN | CRTC_LOCK_REGS);
        if (pATI->Chip >= ATI_CHIP_264XL)
            tmp = (tmp & ~CRTC_INT_ENS_X) | CRTC_INT_ACKS_X;
        outr(CRTC_GEN_CNTL, tmp | CRTC_EN);
        outr(CRTC_GEN_CNTL, tmp);
        outr(CRTC_GEN_CNTL, tmp | CRTC_EN);
        if ((pATI->LCDPanelID >= 0) && (pATI->Chip != ATI_CHIP_264LT))
        {
            pATI->LockData.lcd_index = inr(LCD_INDEX);
            if (pATI->Chip >= ATI_CHIP_264XL)
                outr(LCD_INDEX, pATI->LockData.lcd_index &
                    ~(LCD_MONDET_INT_EN | LCD_MONDET_INT));

            /*
             * Prevent BIOS initiated display switches on dual-CRT controllers.
             */
            if (!pATI->OptionBIOSDisplay && (pATI->Chip != ATI_CHIP_264XL))
            {
                pATI->LockData.scratch_reg3 = inr(SCRATCH_REG3);
                outr(SCRATCH_REG3,
                    pATI->LockData.scratch_reg3 | DISPLAY_SWITCH_DISABLE);
            }
        }

        pATI->LockData.mem_cntl = inr(MEM_CNTL);
        if (pATI->Chip < ATI_CHIP_264CT)
            outr(MEM_CNTL, pATI->LockData.mem_cntl &
                ~(CTL_MEM_BNDRY | CTL_MEM_BNDRY_EN));

        /* Disable feature connector on integrated controllers */
        tmp = pATI->LockData.dac_cntl = inr(DAC_CNTL);
        if (pATI->Chip >= ATI_CHIP_264CT)
            tmp &= ~DAC_FEA_CON_EN;

#ifndef AVOID_CPIO

        /* Ensure VGA aperture is enabled */
        pATI->LockData.config_cntl = inr(CONFIG_CNTL);
        tmp |= DAC_VGA_ADR_EN;
        outr(CONFIG_CNTL, pATI->LockData.config_cntl & ~CFG_VGA_DIS);

#endif /* AVOID_CPIO */

        outr(DAC_CNTL, tmp);

        if (pATI->Chip >= ATI_CHIP_264VTB)
        {
            pATI->LockData.mpp_config = inr(MPP_CONFIG);
            pATI->LockData.mpp_strobe_seq = inr(MPP_STROBE_SEQ);
            pATI->LockData.tvo_cntl = inr(TVO_CNTL);

            if (pATI->Chip >= ATI_CHIP_264GT2C)
            {
                pATI->LockData.hw_debug = inr(HW_DEBUG);

                if (pATI->Chip >= ATI_CHIP_264GTPRO)
                {
                    if (!(pATI->LockData.hw_debug & CMDFIFO_SIZE_EN))
                        outr(HW_DEBUG,
                            pATI->LockData.hw_debug | CMDFIFO_SIZE_EN);

                    pATI->LockData.i2c_cntl_0 =
                        inr(I2C_CNTL_0) | (I2C_CNTL_STAT | I2C_CNTL_HPTR_RST);
                    outr(I2C_CNTL_0,
                        pATI->LockData.i2c_cntl_0 & ~I2C_CNTL_INT_EN);
                    pATI->LockData.i2c_cntl_1 = inr(I2C_CNTL_1);
                }
                else
                {
                    if (pATI->LockData.hw_debug & CMDFIFO_SIZE_DIS)
                        outr(HW_DEBUG,
                            pATI->LockData.hw_debug & ~CMDFIFO_SIZE_DIS);
                }
            }
        }

#ifndef AVOID_CPIO

    }

    if (pATI->VGAAdapter != ATI_ADAPTER_NONE)
    {
        if (pATI->CPIO_VGAWonder)
        {
            /*
             * Ensure all registers are read/write and disable all non-VGA
             * emulations.
             */
            pATI->LockData.b1 = ATIGetExtReg(0xB1U);
            ATIModifyExtReg(pATI, 0xB1U, pATI->LockData.b1, 0xFCU, 0x00U);
            pATI->LockData.b4 = ATIGetExtReg(0xB4U);
            ATIModifyExtReg(pATI, 0xB4U, pATI->LockData.b4, 0x00U, 0x00U);
            pATI->LockData.b5 = ATIGetExtReg(0xB5U);
            ATIModifyExtReg(pATI, 0xB5U, pATI->LockData.b5, 0xBFU, 0x00U);
            pATI->LockData.b6 = ATIGetExtReg(0xB6U);
            ATIModifyExtReg(pATI, 0xB6U, pATI->LockData.b6, 0xDDU, 0x00U);
            pATI->LockData.b8 = ATIGetExtReg(0xB8U);
            ATIModifyExtReg(pATI, 0xB8U, pATI->LockData.b8, 0xC0U, 0x00U);
            pATI->LockData.b9 = ATIGetExtReg(0xB9U);
            ATIModifyExtReg(pATI, 0xB9U, pATI->LockData.b9, 0x7FU, 0x00U);
            if (pATI->Chip > ATI_CHIP_18800)
            {
                pATI->LockData.be = ATIGetExtReg(0xBEU);
                ATIModifyExtReg(pATI, 0xBEU, pATI->LockData.be, 0xFAU, 0x01U);
                if (pATI->Chip >= ATI_CHIP_28800_2)
                {
                    pATI->LockData.a6 = ATIGetExtReg(0xA6U);
                    ATIModifyExtReg(pATI, 0xA6U, pATI->LockData.a6,
                        0x7FU, 0x00U);
                    pATI->LockData.ab = ATIGetExtReg(0xABU);
                    ATIModifyExtReg(pATI, 0xABU, pATI->LockData.ab,
                        0xE7U, 0x00U);
                }
            }
        }

        if (pATI->LCDPanelID >= 0)
        {
            if (pATI->Chip == ATI_CHIP_264LT)
            {
                saved_lcd_gen_ctrl = inr(LCD_GEN_CTRL);

                /* Setup to unlock non-shadow registers */
                lcd_gen_ctrl = saved_lcd_gen_ctrl & ~SHADOW_RW_EN;
                outr(LCD_GEN_CTRL, lcd_gen_ctrl);
            }
            else /* if ((pATI->Chip == ATI_CHIP_264LTPRO) ||
                        (pATI->Chip == ATI_CHIP_264XL) ||
                        (pATI->Chip == ATI_CHIP_MOBILITY)) */
            {
                saved_lcd_gen_ctrl = ATIMach64GetLCDReg(LCD_GEN_CNTL);

                /* Setup to unlock non-shadow registers */
                lcd_gen_ctrl = saved_lcd_gen_ctrl &
                    ~(CRTC_RW_SELECT | SHADOW_RW_EN);
                ATIMach64PutLCDReg(LCD_GEN_CNTL, lcd_gen_ctrl);
            }
        }

        ATISetVGAIOBase(pATI, inb(R_GENMO));

        /*
         * There's a bizarre interaction here.  If bit 0x80 of CRTC[17] is on,
         * then CRTC[3] is read-only.  If bit 0x80 of CRTC[3] is off, then
         * CRTC[17] is write-only (or a read attempt actually returns bits from
         * C/EGA's light pen position).  This means that if both conditions are
         * met, CRTC[17]'s value on server entry cannot be retrieved.
         */

        pATI->LockData.crt03 = tmp = GetReg(CRTX(pATI->CPIO_VGABase), 0x03U);
        if ((tmp & 0x80U) ||
            ((outb(CRTD(pATI->CPIO_VGABase), tmp | 0x80U),
                tmp = inb(CRTD(pATI->CPIO_VGABase))) & 0x80U))
        {
            /* CRTC[16-17] should be readable */
            pATI->LockData.crt11 = tmp =
                GetReg(CRTX(pATI->CPIO_VGABase), 0x11U);
            if (tmp & 0x80U)            /* Unprotect CRTC[0-7] */
                outb(CRTD(pATI->CPIO_VGABase), tmp & 0x7FU);
        }
        else
        {
            /*
             * Could not make CRTC[17] readable, so unprotect CRTC[0-7]
             * replacing VSyncEnd with zero.  This zero will be replaced after
             * acquiring the needed access.
             */
            unsigned int VSyncEnd, VBlankStart, VBlankEnd;
            CARD8 crt07, crt09;

            PutReg(CRTX(pATI->CPIO_VGABase), 0x11U, 0x20U);
            /* Make CRTC[16-17] readable */
            PutReg(CRTX(pATI->CPIO_VGABase), 0x03U, tmp | 0x80U);
            /* Make vertical synch pulse as wide as possible */
            crt07 = GetReg(CRTX(pATI->CPIO_VGABase), 0x07U);
            crt09 = GetReg(CRTX(pATI->CPIO_VGABase), 0x09U);
            VBlankStart = (((crt09 & 0x20U) << 4) | ((crt07 & 0x08U) << 5) |
                GetReg(CRTX(pATI->CPIO_VGABase), 0x15U)) + 1;
            VBlankEnd = (VBlankStart & 0x0300U) |
                GetReg(CRTX(pATI->CPIO_VGABase), 0x16U);
            if (VBlankEnd <= VBlankStart)
                VBlankEnd += 0x0100U;
            VSyncEnd = (((crt07 & 0x80U) << 2) | ((crt07 & 0x04U) << 6) |
                GetReg(CRTX(pATI->CPIO_VGABase), 0x10U)) + 0x0FU;
            if (VSyncEnd >= VBlankEnd)
                VSyncEnd = VBlankEnd - 1;
            pATI->LockData.crt11 = (VSyncEnd & 0x0FU) | 0x20U;
            PutReg(CRTX(pATI->CPIO_VGABase), 0x11U, pATI->LockData.crt11);
            pATI->LockData.crt11 |= 0x80U;
        }

        if (pATI->LCDPanelID >= 0)
        {
            /* Setup to unlock shadow registers */
            lcd_gen_ctrl |= SHADOW_RW_EN;

            if (pATI->Chip == ATI_CHIP_264LT)
                outr(LCD_GEN_CTRL, lcd_gen_ctrl);
            else /* if ((pATI->Chip == ATI_CHIP_264LTPRO) ||
                        (pATI->Chip == ATI_CHIP_264XL) ||
                        (pATI->Chip == ATI_CHIP_MOBILITY)) */
                ATIMach64PutLCDReg(LCD_GEN_CNTL, lcd_gen_ctrl);

            /* Unlock shadow registers */
            ATISetVGAIOBase(pATI, inb(R_GENMO));

            pATI->LockData.shadow_crt03 = tmp =
                GetReg(CRTX(pATI->CPIO_VGABase), 0x03U);
            if ((tmp & 0x80U) ||
                ((outb(CRTD(pATI->CPIO_VGABase), tmp | 0x80U),
                    tmp = inb(CRTD(pATI->CPIO_VGABase))) & 0x80U))
            {
                /* CRTC[16-17] should be readable */
                pATI->LockData.shadow_crt11 = tmp =
                    GetReg(CRTX(pATI->CPIO_VGABase), 0x11U);
                if (tmp & 0x80U)            /* Unprotect CRTC[0-7] */
                {
                    outb(CRTD(pATI->CPIO_VGABase), tmp & 0x7FU);
                }
                else if (!tmp && pATI->LockData.crt11)
                {
                    pATI->LockData.shadow_crt11 = tmp = pATI->LockData.crt11;
                    outb(CRTD(pATI->CPIO_VGABase), tmp & 0x7FU);
                }
            }
            else
            {
                /*
                 * Could not make CRTC[17] readable, so unprotect CRTC[0-7]
                 * replacing VSyncEnd with zero.  This zero will be replaced
                 * after acquiring the needed access.
                 */
                unsigned int VSyncEnd, VBlankStart, VBlankEnd;
                CARD8 crt07, crt09;

                PutReg(CRTX(pATI->CPIO_VGABase), 0x11U, 0x20U);
                /* Make CRTC[16-17] readable */
                PutReg(CRTX(pATI->CPIO_VGABase), 0x03U, tmp | 0x80U);
                /* Make vertical synch pulse as wide as possible */
                crt07 = GetReg(CRTX(pATI->CPIO_VGABase), 0x07U);
                crt09 = GetReg(CRTX(pATI->CPIO_VGABase), 0x09U);
                VBlankStart = (((crt09 & 0x20U) << 4) |
                    ((crt07 & 0x08U) << 5) |
                    GetReg(CRTX(pATI->CPIO_VGABase), 0x15U)) + 1;
                VBlankEnd = (VBlankStart & 0x0300U) |
                    GetReg(CRTX(pATI->CPIO_VGABase), 0x16U);
                if (VBlankEnd <= VBlankStart)
                    VBlankEnd += 0x0100U;
                VSyncEnd = (((crt07 & 0x80U) << 2) | ((crt07 & 0x04U) << 6) |
                    GetReg(CRTX(pATI->CPIO_VGABase), 0x10U)) + 0x0FU;
                if (VSyncEnd >= VBlankEnd)
                    VSyncEnd = VBlankEnd - 1;
                pATI->LockData.shadow_crt11 = (VSyncEnd & 0x0FU) | 0x20U;
                PutReg(CRTX(pATI->CPIO_VGABase), 0x11U,
                    pATI->LockData.shadow_crt11);
                pATI->LockData.shadow_crt11 |= 0x80U;
            }

            /* Restore selection */
            if (pATI->Chip == ATI_CHIP_264LT)
            {
                outr(LCD_GEN_CTRL, saved_lcd_gen_ctrl);
            }
            else /* if ((pATI->Chip == ATI_CHIP_264LTPRO) ||
                        (pATI->Chip == ATI_CHIP_264XL) ||
                        (pATI->Chip == ATI_CHIP_MOBILITY)) */
            {
                ATIMach64PutLCDReg(LCD_GEN_CNTL, saved_lcd_gen_ctrl);

                /* Restore LCD index */
                out8(LCD_INDEX, GetByte(pATI->LockData.lcd_index, 0));
            }
        }

#endif /* AVOID_CPIO */

    }
}

/*
 * ATILock --
 *
 * This function restores the state saved by ATIUnlock() above.
 */
void
ATILock
(
    ATIPtr pATI
)
{

#ifndef AVOID_CPIO

    CARD32 tmp, saved_lcd_gen_ctrl = 0, lcd_gen_ctrl = 0;

#endif /* AVOID_CPIO */

    if (!pATI->Unlocked)
        return;
    pATI->Unlocked = FALSE;

#ifndef AVOID_CPIO

    if (pATI->VGAAdapter != ATI_ADAPTER_NONE)
    {
        if (pATI->LCDPanelID >= 0)
        {
            if (pATI->Chip == ATI_CHIP_264LT)
            {
                saved_lcd_gen_ctrl = inr(LCD_GEN_CTRL);

                /* Setup to lock non-shadow registers */
                lcd_gen_ctrl = saved_lcd_gen_ctrl & ~SHADOW_RW_EN;
                outr(LCD_GEN_CTRL, lcd_gen_ctrl);
            }
            else /* if ((pATI->Chip == ATI_CHIP_264LTPRO) ||
                        (pATI->Chip == ATI_CHIP_264XL) ||
                        (pATI->Chip == ATI_CHIP_MOBILITY)) */
            {
                saved_lcd_gen_ctrl = ATIMach64GetLCDReg(LCD_GEN_CNTL);

                /* Setup to lock non-shadow registers */
                lcd_gen_ctrl = saved_lcd_gen_ctrl &
                    ~(CRTC_RW_SELECT | SHADOW_RW_EN);
                ATIMach64PutLCDReg(LCD_GEN_CNTL, lcd_gen_ctrl);
            }
        }

        ATISetVGAIOBase(pATI, inb(R_GENMO));

        /* Restore VGA locks */
        PutReg(CRTX(pATI->CPIO_VGABase), 0x03U, pATI->LockData.crt03);
        PutReg(CRTX(pATI->CPIO_VGABase), 0x11U, pATI->LockData.crt11);

        if (pATI->LCDPanelID >= 0)
        {
            /* Setup to lock shadow registers */
            lcd_gen_ctrl |= SHADOW_RW_EN;

            if (pATI->Chip == ATI_CHIP_264LT)
                outr(LCD_GEN_CTRL, lcd_gen_ctrl);
            else /* if ((pATI->Chip == ATI_CHIP_264LTPRO) ||
                        (pATI->Chip == ATI_CHIP_264XL) ||
                        (pATI->Chip == ATI_CHIP_MOBILITY)) */
                ATIMach64PutLCDReg(LCD_GEN_CNTL, lcd_gen_ctrl);

            /* Lock shadow registers */
            ATISetVGAIOBase(pATI, inb(R_GENMO));

            PutReg(CRTX(pATI->CPIO_VGABase), 0x03U,
                pATI->LockData.shadow_crt03);
            PutReg(CRTX(pATI->CPIO_VGABase), 0x11U,
                pATI->LockData.shadow_crt11);

            /* Restore selection */
            if (pATI->Chip == ATI_CHIP_264LT)
                outr(LCD_GEN_CTRL, saved_lcd_gen_ctrl);
            else /* if ((pATI->Chip == ATI_CHIP_264LTPRO) ||
                        (pATI->Chip == ATI_CHIP_264XL) ||
                        (pATI->Chip == ATI_CHIP_MOBILITY)) */
                ATIMach64PutLCDReg(LCD_GEN_CNTL, saved_lcd_gen_ctrl);
        }

        if (pATI->CPIO_VGAWonder)
        {
            /*
             * Restore emulation and protection bits in ATI extended VGA
             * registers.
             */
            ATIModifyExtReg(pATI, 0xB1U, -1, 0xFCU, pATI->LockData.b1);
            ATIModifyExtReg(pATI, 0xB4U, -1, 0x00U, pATI->LockData.b4);
            ATIModifyExtReg(pATI, 0xB5U, -1, 0xBFU, pATI->LockData.b5);
            ATIModifyExtReg(pATI, 0xB6U, -1, 0xDDU, pATI->LockData.b6);
            ATIModifyExtReg(pATI, 0xB8U, -1, 0xC0U, pATI->LockData.b8 & 0x03U);
            ATIModifyExtReg(pATI, 0xB9U, -1, 0x7FU, pATI->LockData.b9);
            if (pATI->Chip > ATI_CHIP_18800)
            {
                ATIModifyExtReg(pATI, 0xBEU, -1, 0xFAU, pATI->LockData.be);
                if (pATI->Chip >= ATI_CHIP_28800_2)
                {
                    ATIModifyExtReg(pATI, 0xA6U, -1, 0x7FU, pATI->LockData.a6);
                    ATIModifyExtReg(pATI, 0xABU, -1, 0xE7U, pATI->LockData.ab);
                }
            }
            ATIModifyExtReg(pATI, 0xB8U, -1, 0xC0U, pATI->LockData.b8);
        }
    }

    if (pATI->ChipHasSUBSYS_CNTL)
    {
        tmp = inw(SUBSYS_STAT) & _8PLANE;

        /* Reset the 8514/A and disable all interrupts */
        outw(SUBSYS_CNTL, tmp | (GPCTRL_RESET | CHPTEST_NORMAL));
        outw(SUBSYS_CNTL, tmp | (GPCTRL_ENAB | CHPTEST_NORMAL | RVBLNKFLG |
            RPICKFLAG | RINVALIDIO | RGPIDLE));

        /* Restore modified accelerator registers */
        outw(CLOCK_SEL, pATI->LockData.clock_sel);
        if (pATI->Chip >= ATI_CHIP_68800)
        {
            outw(MISC_OPTIONS, pATI->LockData.misc_options);
            outw(MEM_BNDRY, pATI->LockData.mem_bndry);
            outw(MEM_CFG, pATI->LockData.mem_cfg);
        }

        /* Wait for all activity to die down */
        ProbeWaitIdleEmpty();
    }
    else if (pATI->Chip >= ATI_CHIP_88800GXC)

#endif /* AVOID_CPIO */

    {
        /* Reset everything */
        outr(BUS_CNTL, pATI->LockData.bus_cntl);

        outr(CRTC_INT_CNTL, pATI->LockData.crtc_int_cntl);

        outr(GEN_TEST_CNTL, pATI->LockData.gen_test_cntl | GEN_GUI_EN);
        outr(GEN_TEST_CNTL, pATI->LockData.gen_test_cntl);
        outr(GEN_TEST_CNTL, pATI->LockData.gen_test_cntl | GEN_GUI_EN);

        outr(CRTC_GEN_CNTL, pATI->LockData.crtc_gen_cntl | CRTC_EN);
        outr(CRTC_GEN_CNTL, pATI->LockData.crtc_gen_cntl);
        outr(CRTC_GEN_CNTL, pATI->LockData.crtc_gen_cntl | CRTC_EN);

#ifndef AVOID_CPIO

        outr(CONFIG_CNTL, pATI->LockData.config_cntl);

#endif /* AVOID_CPIO */

        outr(DAC_CNTL, pATI->LockData.dac_cntl);
        if (pATI->Chip < ATI_CHIP_264CT)
            outr(MEM_CNTL, pATI->LockData.mem_cntl);
        if ((pATI->LCDPanelID >= 0) && (pATI->Chip != ATI_CHIP_264LT))
        {
            outr(LCD_INDEX, pATI->LockData.lcd_index);
            if (!pATI->OptionBIOSDisplay && (pATI->Chip != ATI_CHIP_264XL))
                outr(SCRATCH_REG3, pATI->LockData.scratch_reg3);
        }
        if (pATI->Chip >= ATI_CHIP_264VTB)
        {
            outr(MPP_CONFIG, pATI->LockData.mpp_config);
            outr(MPP_STROBE_SEQ, pATI->LockData.mpp_strobe_seq);
            outr(TVO_CNTL, pATI->LockData.tvo_cntl);
            if (pATI->Chip >= ATI_CHIP_264GT2C)
            {
                outr(HW_DEBUG, pATI->LockData.hw_debug);
                if (pATI->Chip >= ATI_CHIP_264GTPRO)
                {
                    outr(I2C_CNTL_0, pATI->LockData.i2c_cntl_0);
                    outr(I2C_CNTL_1, pATI->LockData.i2c_cntl_1);
                }
            }
        }
    }
}

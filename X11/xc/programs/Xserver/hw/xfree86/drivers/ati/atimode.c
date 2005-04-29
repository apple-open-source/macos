/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/ati/atimode.c,v 1.19 2004/02/11 17:20:06 tsi Exp $ */
/*
 * Copyright 2000 through 2004 by Marc Aurele La France (TSI @ UQV), tsi@xfree86.org
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
#include "atidsp.h"
#include "atimach64.h"
#include "atimach64io.h"
#include "atimode.h"
#include "atiprint.h"
#include "atirgb514.h"
#include "ativga.h"
#include "atiwonder.h"
#include "atiwonderio.h"

#ifndef AVOID_CPIO

/*
 * ATICopyVGAMemory --
 *
 * This function is called to copy one or all banks of a VGA plane.
 */
static void
ATICopyVGAMemory
(
    ATIPtr   pATI,
    ATIHWPtr pATIHW,
    pointer  *saveptr,
    pointer  *from,
    pointer  *to
)
{
    unsigned int iBank;

    for (iBank = 0;  iBank < pATIHW->nBank;  iBank++)
    {
        (*pATIHW->SetBank)(pATI, iBank);
        (void)memcpy(*to, *from, 0x00010000U);
        *saveptr = (char *)(*saveptr) + 0x00010000U;
    }
}

/*
 * ATISwap --
 *
 * This function saves/restores video memory contents during video mode
 * switches.
 */
static void
ATISwap
(
    int      iScreen,
    ATIPtr   pATI,
    ATIHWPtr pATIHW,
    Bool     ToFB
)
{
    pointer save, *from, *to;
    unsigned int iPlane = 0, PlaneMask = 1;
    CARD8 seq2, seq4, gra1, gra3, gra4, gra5, gra6, gra8;

    /*
     * This is only done for non-accelerator modes.  If the video state on
     * server entry was an accelerator mode, the application that relinquished
     * the console had better do the Right Thing (tm) anyway by saving and
     * restoring its own video memory contents.
     */
    if (pATIHW->crtc != ATI_CRTC_VGA)
        return;

    if (ToFB)
    {
        if (!pATIHW->frame_buffer)
            return;

        from = &save;
        to = &pATI->pBank;
    }
    else
    {
        /* Allocate the memory */
        if (!pATIHW->frame_buffer)
        {
            pATIHW->frame_buffer =
                (pointer)xalloc(pATIHW->nBank * pATIHW->nPlane * 0x00010000U);
            if (!pATIHW->frame_buffer)
            {
                xf86DrvMsg(iScreen, X_WARNING,
                    "Temporary frame buffer could not be allocated.\n");
                return;
            }
        }

        from = &pATI->pBank;
        to = &save;
    }

    /* Turn off screen */
    ATIVGASaveScreen(pATI, SCREEN_SAVER_ON);

    /* Save register values to be modified */
    seq2 = GetReg(SEQX, 0x02U);
    seq4 = GetReg(SEQX, 0x04U);
    gra1 = GetReg(GRAX, 0x01U);
    gra3 = GetReg(GRAX, 0x03U);
    gra5 = GetReg(GRAX, 0x05U);
    gra6 = GetReg(GRAX, 0x06U);
    gra8 = GetReg(GRAX, 0x08U);

    save = pATIHW->frame_buffer;

    /* Temporarily normalise the mode */
    if (gra1 != 0x00U)
        PutReg(GRAX, 0x01U, 0x00U);
    if (gra3 != 0x00U)
        PutReg(GRAX, 0x03U, 0x00U);
    if (gra6 != 0x05U)
        PutReg(GRAX, 0x06U, 0x05U);
    if (gra8 != 0xFFU)
        PutReg(GRAX, 0x08U, 0xFFU);

    if (seq4 & 0x08U)
    {
        /* Setup packed mode memory */
        if (seq2 != 0x0FU)
            PutReg(SEQX, 0x02U, 0x0FU);
        if (seq4 != 0x0AU)
            PutReg(SEQX, 0x04U, 0x0AU);
        if (pATI->Chip < ATI_CHIP_264CT)
        {
            if (gra5 != 0x00U)
                PutReg(GRAX, 0x05U, 0x00U);
        }
        else
        {
            if (gra5 != 0x40U)
                PutReg(GRAX, 0x05U, 0x40U);
        }

        ATICopyVGAMemory(pATI, pATIHW, &save, from, to);

        if (seq2 != 0x0FU)
            PutReg(SEQX, 0x02U, seq2);
        if (seq4 != 0x0AU)
            PutReg(SEQX, 0x04U, seq4);
        if (pATI->Chip < ATI_CHIP_264CT)
        {
            if (gra5 != 0x00U)
                PutReg(GRAX, 0x05U, gra5);
        }
        else
        {
            if (gra5 != 0x40U)
                PutReg(GRAX, 0x05U, gra5);
        }
    }
    else
    {
        gra4 = GetReg(GRAX, 0x04U);

        /* Setup planar mode memory */
        if (seq4 != 0x06U)
            PutReg(SEQX, 0x04U, 0x06U);
        if (gra5 != 0x00U)
            PutReg(GRAX, 0x05U, 0x00U);

        for (;  iPlane < pATIHW->nPlane;  iPlane++)
        {
            PutReg(SEQX, 0x02U, PlaneMask);
            PutReg(GRAX, 0x04U, iPlane);
            ATICopyVGAMemory(pATI, pATIHW, &save, from, to);
            PlaneMask <<= 1;
        }

        PutReg(SEQX, 0x02U, seq2);
        if (seq4 != 0x06U)
            PutReg(SEQX, 0x04U, seq4);
        PutReg(GRAX, 0x04U, gra4);
        if (gra5 != 0x00U)
            PutReg(GRAX, 0x05U, gra5);
    }

    /* Restore registers */
    if (gra1 != 0x00U)
        PutReg(GRAX, 0x01U, gra1);
    if (gra3 != 0x00U)
        PutReg(GRAX, 0x03U, gra3);
    if (gra6 != 0x05U)
        PutReg(GRAX, 0x06U, gra6);
    if (gra8 != 0xFFU)
        PutReg(GRAX, 0x08U, gra8);

    /* Back to bank 0 */
    (*pATIHW->SetBank)(pATI, 0);

    /*
     * If restoring video memory for a server video mode, free the frame buffer
     * save area.
     */
    if (ToFB && (pATIHW == &pATI->NewHW))
    {
        xfree(pATIHW->frame_buffer);
        pATIHW->frame_buffer = NULL;
    }
}

#endif /* AVOID_CPIO */

/*
 * ATIModePreInit --
 *
 * This function initialises an ATIHWRec with information common to all video
 * states generated by the driver.
 */
void
ATIModePreInit
(
    ScrnInfoPtr pScreenInfo,
    ATIPtr      pATI,
    ATIHWPtr    pATIHW
)
{
    CARD32 lcd_index;

#ifndef AVOID_CPIO

    if (pATI->VGAAdapter != ATI_ADAPTER_NONE)
    {
        /* Fill in VGA data */
        ATIVGAPreInit(pATI, pATIHW);

        /* Fill in VGA Wonder data */
        if (pATI->CPIO_VGAWonder)
            ATIVGAWonderPreInit(pATI, pATIHW);
    }

    if (pATI->Chip >= ATI_CHIP_88800GXC)

#endif /* AVOID_CPIO */

    {
        /* Fill in Mach64 data */
        ATIMach64PreInit(pScreenInfo, pATI, pATIHW);

        if (pATI->Chip >= ATI_CHIP_264CT)
        {
            /* Ensure proper VCLK source */
            pATIHW->pll_vclk_cntl = ATIMach64GetPLLReg(PLL_VCLK_CNTL) |
                (PLL_VCLK_SRC_SEL | PLL_VCLK_RESET);

            /* Set provisional values for other PLL registers */
            pATIHW->pll_vclk_post_div = ATIMach64GetPLLReg(PLL_VCLK_POST_DIV);
            pATIHW->pll_vclk0_fb_div = ATIMach64GetPLLReg(PLL_VCLK0_FB_DIV);
            pATIHW->pll_vclk1_fb_div = ATIMach64GetPLLReg(PLL_VCLK1_FB_DIV);
            pATIHW->pll_vclk2_fb_div = ATIMach64GetPLLReg(PLL_VCLK2_FB_DIV);
            pATIHW->pll_vclk3_fb_div = ATIMach64GetPLLReg(PLL_VCLK3_FB_DIV);
            pATIHW->pll_xclk_cntl = ATIMach64GetPLLReg(PLL_XCLK_CNTL);

            /* For now disable extended reference and feedback dividers */
            if (pATI->Chip >= ATI_CHIP_264LT)
                pATIHW->pll_ext_vpll_cntl =
                    ATIMach64GetPLLReg(PLL_EXT_VPLL_CNTL) &
                    ~(PLL_EXT_VPLL_EN | PLL_EXT_VPLL_VGA_EN |
                      PLL_EXT_VPLL_INSYNC);

            /* Initialise CRTC data for LCD panels */
            if (pATI->LCDPanelID >= 0)
            {
                if (pATI->Chip == ATI_CHIP_264LT)
                {
                    pATIHW->lcd_gen_ctrl = inr(LCD_GEN_CTRL);
                }
                else /* if ((pATI->Chip == ATI_CHIP_264LTPRO) ||
                            (pATI->Chip == ATI_CHIP_264XL) ||
                            (pATI->Chip == ATI_CHIP_MOBILITY)) */
                {
                    lcd_index = inr(LCD_INDEX);
                    pATIHW->lcd_index = lcd_index &
                        ~(LCD_REG_INDEX | LCD_DISPLAY_DIS | LCD_SRC_SEL |
                          LCD_CRTC2_DISPLAY_DIS);
                    if (pATI->Chip != ATI_CHIP_264XL)
                        pATIHW->lcd_index |= LCD_CRTC2_DISPLAY_DIS;
                    pATIHW->config_panel =
                        ATIMach64GetLCDReg(LCD_CONFIG_PANEL) |
                        DONT_SHADOW_HEND;
                    pATIHW->lcd_gen_ctrl =
                        ATIMach64GetLCDReg(LCD_GEN_CNTL) & ~CRTC_RW_SELECT;
                    outr(LCD_INDEX, lcd_index);
                }

                pATIHW->lcd_gen_ctrl &=
                    ~(HORZ_DIVBY2_EN | DIS_HOR_CRT_DIVBY2 | MCLK_PM_EN |
                      VCLK_DAC_PM_EN | USE_SHADOWED_VEND |
                      USE_SHADOWED_ROWCUR | SHADOW_EN | SHADOW_RW_EN);
                pATIHW->lcd_gen_ctrl |= DONT_SHADOW_VPAR | LOCK_8DOT;

                if (!pATI->OptionPanelDisplay)
                {
                    /*
                     * Use primary CRTC to drive the CRT.  Turn off panel
                     * interface.
                     */
                    pATIHW->lcd_gen_ctrl &= ~LCD_ON;
                    pATIHW->lcd_gen_ctrl |= CRT_ON;
                }
                else
                {
                    /* Use primary CRTC to drive the panel */
                    pATIHW->lcd_gen_ctrl |= LCD_ON;

                    /* If requested, also force CRT on */
                    if (pATI->OptionCRTDisplay)
                        pATIHW->lcd_gen_ctrl |= CRT_ON;
                }
            }
        }
        else if (pATI->DAC == ATI_DAC_IBMRGB514)
        {
            ATIRGB514PreInit(pATI, pATIHW);
        }
    }

    /* Set RAMDAC data */
    ATIDACPreInit(pScreenInfo, pATI, pATIHW);
}

/*
 * ATIModeSave --
 *
 * This function saves the current video state.
 */
void
ATIModeSave
(
    ScrnInfoPtr pScreenInfo,
    ATIPtr      pATI,
    ATIHWPtr    pATIHW
)
{

#ifndef AVOID_CPIO

    int Index;

    /* Get back to bank 0 */
    (*pATIHW->SetBank)(pATI, 0);

#endif /* AVOID_CPIO */

    /* Save clock data */
    ATIClockSave(pScreenInfo, pATI, pATIHW);

    if (pATI->Chip >= ATI_CHIP_264CT)
    {
        pATIHW->pll_vclk_cntl = ATIMach64GetPLLReg(PLL_VCLK_CNTL) |
            PLL_VCLK_RESET;
        pATIHW->pll_vclk_post_div = ATIMach64GetPLLReg(PLL_VCLK_POST_DIV);
        pATIHW->pll_vclk0_fb_div = ATIMach64GetPLLReg(PLL_VCLK0_FB_DIV);
        pATIHW->pll_vclk1_fb_div = ATIMach64GetPLLReg(PLL_VCLK1_FB_DIV);
        pATIHW->pll_vclk2_fb_div = ATIMach64GetPLLReg(PLL_VCLK2_FB_DIV);
        pATIHW->pll_vclk3_fb_div = ATIMach64GetPLLReg(PLL_VCLK3_FB_DIV);
        pATIHW->pll_xclk_cntl = ATIMach64GetPLLReg(PLL_XCLK_CNTL);
        if (pATI->Chip >= ATI_CHIP_264LT)
            pATIHW->pll_ext_vpll_cntl = ATIMach64GetPLLReg(PLL_EXT_VPLL_CNTL);

        /* Save LCD registers */
        if (pATI->LCDPanelID >= 0)
        {
            if (pATI->Chip == ATI_CHIP_264LT)
            {
                pATIHW->horz_stretching = inr(HORZ_STRETCHING);
                pATIHW->vert_stretching = inr(VERT_STRETCHING);
                pATIHW->lcd_gen_ctrl = inr(LCD_GEN_CTRL);

                /* Set up to save non-shadow registers */
                outr(LCD_GEN_CTRL, pATIHW->lcd_gen_ctrl & ~SHADOW_RW_EN);
            }
            else /* if ((pATI->Chip == ATI_CHIP_264LTPRO) ||
                        (pATI->Chip == ATI_CHIP_264XL) ||
                        (pATI->Chip == ATI_CHIP_MOBILITY)) */
            {
                pATIHW->lcd_index = inr(LCD_INDEX);
                pATIHW->config_panel = ATIMach64GetLCDReg(LCD_CONFIG_PANEL);
                pATIHW->lcd_gen_ctrl = ATIMach64GetLCDReg(LCD_GEN_CNTL);
                pATIHW->horz_stretching =
                    ATIMach64GetLCDReg(LCD_HORZ_STRETCHING);
                pATIHW->vert_stretching =
                    ATIMach64GetLCDReg(LCD_VERT_STRETCHING);
                pATIHW->ext_vert_stretch =
                    ATIMach64GetLCDReg(LCD_EXT_VERT_STRETCH);

                /* Set up to save non-shadow registers */
                ATIMach64PutLCDReg(LCD_GEN_CNTL,
                    pATIHW->lcd_gen_ctrl & ~(CRTC_RW_SELECT | SHADOW_RW_EN));
            }
        }
    }

#ifndef AVOID_CPIO

    if (pATI->VGAAdapter != ATI_ADAPTER_NONE)
    {
        /* Save VGA data */
        ATIVGASave(pATI, pATIHW);

        /* Save VGA Wonder data */
        if (pATI->CPIO_VGAWonder)
            ATIVGAWonderSave(pATI, pATIHW);
    }

    if (pATI->Chip >= ATI_CHIP_88800GXC)

#endif /* AVOID_CPIO */

    {
        /* Save Mach64 data */
        ATIMach64Save(pATI, pATIHW);

        if (pATI->Chip >= ATI_CHIP_264VTB)
        {
            /* Save DSP data */
            ATIDSPSave(pATI, pATIHW);

            if (pATI->LCDPanelID >= 0)
            {
                /* Switch to shadow registers */
                if (pATI->Chip == ATI_CHIP_264LT)
                    outr(LCD_GEN_CTRL, pATIHW->lcd_gen_ctrl | SHADOW_RW_EN);
                else /* if ((pATI->Chip == ATI_CHIP_264LTPRO) ||
                            (pATI->Chip == ATI_CHIP_264XL) ||
                            (pATI->Chip == ATI_CHIP_MOBILITY)) */
                    ATIMach64PutLCDReg(LCD_GEN_CNTL,
                        (pATIHW->lcd_gen_ctrl & ~CRTC_RW_SELECT) |
                        SHADOW_RW_EN);

#ifndef AVOID_CPIO

                /* Save shadow VGA CRTC registers */
                for (Index = 0;
                     Index < NumberOf(pATIHW->shadow_vga);
                     Index++)
                    pATIHW->shadow_vga[Index] =
                        GetReg(CRTX(pATI->CPIO_VGABase), Index);

#endif /* AVOID_CPIO */

                /* Save shadow Mach64 CRTC registers */
                pATIHW->shadow_h_total_disp = inr(CRTC_H_TOTAL_DISP);
                pATIHW->shadow_h_sync_strt_wid = inr(CRTC_H_SYNC_STRT_WID);
                pATIHW->shadow_v_total_disp = inr(CRTC_V_TOTAL_DISP);
                pATIHW->shadow_v_sync_strt_wid = inr(CRTC_V_SYNC_STRT_WID);

                /* Restore CRTC selection and shadow state */
                if (pATI->Chip == ATI_CHIP_264LT)
                {
                    outr(LCD_GEN_CTRL, pATIHW->lcd_gen_ctrl);
                }
                else /* if ((pATI->Chip == ATI_CHIP_264LTPRO) ||
                            (pATI->Chip == ATI_CHIP_264XL) ||
                            (pATI->Chip == ATI_CHIP_MOBILITY)) */
                {
                    ATIMach64PutLCDReg(LCD_GEN_CNTL, pATIHW->lcd_gen_ctrl);
                    outr(LCD_INDEX, pATIHW->lcd_index);
                }
            }
        }
        else if (pATI->DAC == ATI_DAC_IBMRGB514)
            ATIRGB514Save(pATI, pATIHW);
    }

#ifndef AVOID_CPIO

    /*
     * For some unknown reason, CLKDIV2 needs to be turned off to save the
     * DAC's LUT reliably on VGA Wonder VLB adapters.
     */
    if ((pATI->Adapter == ATI_ADAPTER_NONISA) && (pATIHW->seq[1] & 0x08U))
        PutReg(SEQX, 0x01U, pATIHW->seq[1] & ~0x08U);

#endif /* AVOID_CPIO */

    /* Save RAMDAC state */
    ATIDACSave(pATI, pATIHW);

#ifndef AVOID_CPIO

    if ((pATI->Adapter == ATI_ADAPTER_NONISA) && (pATIHW->seq[1] & 0x08U))
        PutReg(SEQX, 0x01U, pATIHW->seq[1]);

#endif /* AVOID_CPIO */

    /*
     * The server has already saved video memory contents when switching out of
     * its virtual console, so don't do it again.
     */
    if (pATIHW != &pATI->NewHW)
    {
        pATIHW->FeedbackDivider = 0;    /* Don't programme clock */

#ifndef AVOID_CPIO

        /* Save video memory */
        ATISwap(pScreenInfo->scrnIndex, pATI, pATIHW, FALSE);

#endif /* AVOID_CPIO */

    }

#ifndef AVOID_CPIO

    if (pATI->VGAAdapter != ATI_ADAPTER_NONE)
        ATIVGASaveScreen(pATI, SCREEN_SAVER_OFF);       /* Turn on screen */

#endif /* AVOID_CPIO */

}

/*
 * ATIModeCalculate --
 *
 * This function fills in an ATIHWRec with all register values needed to enable
 * a video state.  It's important that this be done without modifying the
 * current video state.
 */
Bool
ATIModeCalculate
(
    int            iScreen,
    ATIPtr         pATI,
    ATIHWPtr       pATIHW,
    DisplayModePtr pMode
)
{
    CARD32 lcd_index;
    int Index, ECPClock, MaxScalerClock;

    /* Clobber mode timings */
    if (pATI->OptionPanelDisplay && (pATI->LCDPanelID >= 0) &&
        !pMode->CrtcHAdjusted && !pMode->CrtcVAdjusted &&
        (!pATI->OptionLCDSync || (pMode->type & M_T_BUILTIN)))
    {
        int VScan;

        pMode->Clock = pATI->LCDClock;
        pMode->Flags &= ~(V_DBLSCAN | V_INTERLACE | V_CLKDIV2);

        /*
         * Use doublescanning or multiscanning to get around vertical blending
         * limitations.
         */
        VScan = pATI->LCDVertical / pMode->VDisplay;
        switch (pATIHW->crtc)
        {

#ifndef AVOID_CPIO

            case ATI_CRTC_VGA:
                if (VScan > 64)
                    VScan = 64;
                pMode->VScan = VScan;
                break;

#endif /* AVOID_CPIO */

            case ATI_CRTC_MACH64:
                pMode->VScan = 0;
                if (VScan <= 1)
                    break;
                VScan = 2;
                pMode->Flags |= V_DBLSCAN;
                break;

            default:
                break;
        }

        pMode->HSyncStart = pMode->HDisplay + pATI->LCDHSyncStart;
        pMode->HSyncEnd = pMode->HSyncStart + pATI->LCDHSyncWidth;
        pMode->HTotal = pMode->HDisplay + pATI->LCDHBlankWidth;

        pMode->VSyncStart = pMode->VDisplay +
            ATIDivide(pATI->LCDVSyncStart, VScan, 0, 0);
        pMode->VSyncEnd = pMode->VSyncStart +
            ATIDivide(pATI->LCDVSyncWidth, VScan, 0, 1);
        pMode->VTotal = pMode->VDisplay +
            ATIDivide(pATI->LCDVBlankWidth, VScan, 0, 0);
    }

    switch (pATIHW->crtc)
    {

#ifndef AVOID_CPIO

        case ATI_CRTC_VGA:
            /* Fill in VGA data */
            ATIVGACalculate(pATI, pATIHW, pMode);

            /* Fill in VGA Wonder data */
            if (pATI->CPIO_VGAWonder)
                ATIVGAWonderCalculate(pATI, pATIHW, pMode);

            if (pATI->Chip >= ATI_CHIP_88800GXC)
            {
                if (pATI->Chip >= ATI_CHIP_264CT)
                {
                    /*
                     * Selected bits of accelerator & VGA CRTC registers are
                     * actually copies of each other.
                     */
                    pATIHW->crtc_h_total_disp =
                        SetBits(pMode->CrtcHTotal, CRTC_H_TOTAL) |
                            SetBits(pMode->CrtcHDisplay, CRTC_H_DISP);
                    pATIHW->crtc_h_sync_strt_wid =
                        SetBits(pMode->CrtcHSyncStart, CRTC_H_SYNC_STRT) |
                            SetBits(pMode->CrtcHSkew, CRTC_H_SYNC_DLY) | /* ? */
                            SetBits(GetBits(pMode->CrtcHSyncStart, 0x0100U),
                                CRTC_H_SYNC_STRT_HI) |
                            SetBits(pMode->CrtcHSyncEnd, CRTC_H_SYNC_WID);
                    if (pMode->Flags & V_NHSYNC)
                        pATIHW->crtc_h_sync_strt_wid |= CRTC_H_SYNC_POL;

                    pATIHW->crtc_v_total_disp =
                        SetBits(pMode->CrtcVTotal, CRTC_V_TOTAL) |
                            SetBits(pMode->CrtcVDisplay, CRTC_V_DISP);
                    pATIHW->crtc_v_sync_strt_wid =
                        SetBits(pMode->CrtcVSyncStart, CRTC_V_SYNC_STRT) |
                            SetBits(pMode->CrtcVSyncEnd, CRTC_V_SYNC_WID);
                    if (pMode->Flags & V_NVSYNC)
                        pATIHW->crtc_v_sync_strt_wid |= CRTC_V_SYNC_POL;
                }

                pATIHW->crtc_gen_cntl = inr(CRTC_GEN_CNTL) &
                    ~(CRTC_DBL_SCAN_EN | CRTC_INTERLACE_EN |
                      CRTC_HSYNC_DIS | CRTC_VSYNC_DIS | CRTC_CSYNC_EN |
                      CRTC_PIX_BY_2_EN | CRTC_DISPLAY_DIS |
                      CRTC_VGA_XOVERSCAN | CRTC_PIX_WIDTH |
                      CRTC_BYTE_PIX_ORDER | CRTC_VGA_128KAP_PAGING |
                      CRTC_VFC_SYNC_TRISTATE |
                      CRTC_LOCK_REGS |  /* Already off, but ... */
                      CRTC_SYNC_TRISTATE | CRTC_EXT_DISP_EN |
                      CRTC_DISP_REQ_EN | CRTC_VGA_LINEAR | CRTC_VGA_TEXT_132 |
                      CRTC_CUR_B_TEST);
                /* Some of these are not relevent, but that doesn't matter */
                switch (pATI->depth)
                {
                    case 1:
                        pATIHW->crtc_gen_cntl |=
                            SetBits(PIX_WIDTH_1BPP, CRTC_PIX_WIDTH);
                        break;

                    case 4:
                        pATIHW->crtc_gen_cntl |=
                            SetBits(PIX_WIDTH_4BPP, CRTC_PIX_WIDTH);
                        break;

                    case 8:
                        pATIHW->crtc_gen_cntl |=
                            SetBits(PIX_WIDTH_8BPP, CRTC_PIX_WIDTH);
                        break;

                    case 15:
                        pATIHW->crtc_gen_cntl |=
                            SetBits(PIX_WIDTH_15BPP, CRTC_PIX_WIDTH);
                        break;

                    case 16:
                        pATIHW->crtc_gen_cntl |=
                            SetBits(PIX_WIDTH_16BPP, CRTC_PIX_WIDTH);
                        break;

                    case 24:
                        if (pATI->bitsPerPixel == 24)
                        {
                            pATIHW->crtc_gen_cntl |=
                                SetBits(PIX_WIDTH_24BPP, CRTC_PIX_WIDTH);
                            break;
                        }
                        if (pATI->bitsPerPixel != 32)
                            break;
                        /* Fall through */

                    case 32:
                        pATIHW->crtc_gen_cntl |=
                            SetBits(PIX_WIDTH_32BPP, CRTC_PIX_WIDTH);
                        break;

                    default:
                        break;
                }
#if 0           /* This isn't needed, but is kept for reference */
                if (pMode->Flags & V_DBLSCAN)
                    pATIHW->crtc_gen_cntl |= CRTC_DBL_SCAN_EN;
#endif
                if (pMode->Flags & V_INTERLACE)
                    pATIHW->crtc_gen_cntl |= CRTC_INTERLACE_EN;
                if ((pMode->Flags & (V_CSYNC | V_PCSYNC)) || pATI->OptionCSync)
                    pATIHW->crtc_gen_cntl |= CRTC_CSYNC_EN;
                if (pATI->depth <= 4)
                    pATIHW->crtc_gen_cntl |= CRTC_EN | CRTC_CNT_EN;
                else
                    pATIHW->crtc_gen_cntl |=
                        CRTC_EN | CRTC_VGA_LINEAR | CRTC_CNT_EN;
            }
            break;

#endif /* AVOID_CPIO */

        case ATI_CRTC_MACH64:
            /* Fill in Mach64 data */
            ATIMach64Calculate(pATI, pATIHW, pMode);
            break;

        default:
            break;
    }

    /* Set up LCD register values */
    if (pATI->LCDPanelID >= 0)
    {
        int VDisplay = pMode->VDisplay;

        if (pMode->Flags & V_DBLSCAN)
            VDisplay <<= 1;
        if (pMode->VScan > 1)
            VDisplay *= pMode->VScan;
        if (pMode->Flags & V_INTERLACE)
            VDisplay >>= 1;

        /* Ensure secondary CRTC is completely disabled */
        pATIHW->crtc_gen_cntl &= ~(CRTC2_EN | CRTC2_PIX_WIDTH);

        if (pATI->Chip == ATI_CHIP_264LT)
        {
            pATIHW->horz_stretching = inr(HORZ_STRETCHING);
        }
        else /* if ((pATI->Chip == ATI_CHIP_264LTPRO) ||
                    (pATI->Chip == ATI_CHIP_264XL) ||
                    (pATI->Chip == ATI_CHIP_MOBILITY)) */
        {
            lcd_index = inr(LCD_INDEX);
            pATIHW->horz_stretching = ATIMach64GetLCDReg(LCD_HORZ_STRETCHING);
            pATIHW->ext_vert_stretch =
                ATIMach64GetLCDReg(LCD_EXT_VERT_STRETCH) &
                ~(AUTO_VERT_RATIO | VERT_STRETCH_MODE | VERT_STRETCH_RATIO3);

            /*
             * Don't use vertical blending if the mode is too wide or not
             * vertically stretched.
             */
            if (pATI->OptionPanelDisplay &&
                (pMode->HDisplay <= pATI->LCDVBlendFIFOSize) &&
                (VDisplay < pATI->LCDVertical))
                pATIHW->ext_vert_stretch |= VERT_STRETCH_MODE;

            outr(LCD_INDEX, lcd_index);
        }

        pATIHW->horz_stretching &=
            ~(HORZ_STRETCH_RATIO | HORZ_STRETCH_LOOP | AUTO_HORZ_RATIO |
              HORZ_STRETCH_MODE | HORZ_STRETCH_EN);
        if (pATI->OptionPanelDisplay &&
            (pMode->HDisplay < pATI->LCDHorizontal))
        do
        {
            /*
             * The horizontal blender misbehaves when HDisplay is less than a
             * a certain threshold (440 for a 1024-wide panel).  It doesn't
             * stretch such modes enough.  Use pixel replication instead of
             * blending to stretch modes that can be made to exactly fit the
             * panel width.  The undocumented "NoLCDBlend" option allows the
             * pixel-replicated mode to be slightly wider or narrower than the
             * panel width.  It also causes a mode that is exactly half as wide
             * as the panel to be pixel-replicated, rather than blended.
             */
            int HDisplay  = pMode->HDisplay & ~7;
            int nStretch  = pATI->LCDHorizontal / HDisplay;
            int Remainder = pATI->LCDHorizontal % HDisplay;

            if ((!Remainder && ((nStretch > 2) || !pATI->OptionBlend)) ||
                (((HDisplay * 16) / pATI->LCDHorizontal) < 7))
            {
                static const char StretchLoops[] = {10, 12, 13, 15, 16};
                int horz_stretch_loop = -1, BestRemainder;
                int Numerator = HDisplay, Denominator = pATI->LCDHorizontal;

                ATIReduceRatio(&Numerator, &Denominator);

                BestRemainder = (Numerator * 16) / Denominator;
                Index = NumberOf(StretchLoops);
                while (--Index >= 0)
                {
                    Remainder =
                        ((Denominator - Numerator) * StretchLoops[Index]) %
                        Denominator;
                    if (Remainder < BestRemainder)
                    {
                        horz_stretch_loop = Index;
                        if (!(BestRemainder = Remainder))
                            break;
                    }
#if 0
                    /*
                     * Enabling this code allows the pixel-replicated mode to
                     * be slightly wider than the panel width.
                     */
                    Remainder = Denominator - Remainder;
                    if (Remainder < BestRemainder)
                    {
                        horz_stretch_loop = Index;
                        BestRemainder = Remainder;
                    }
#endif
                }

                if ((horz_stretch_loop >= 0) &&
                    (!BestRemainder || !pATI->OptionBlend))
                {
                    int horz_stretch_ratio = 0, Accumulator = 0;
                    int reuse_previous = 1;

                    Index = StretchLoops[horz_stretch_loop];

                    while (--Index >= 0)
                    {
                        if (Accumulator > 0)
                            horz_stretch_ratio |= reuse_previous;
                        else
                            Accumulator += Denominator;
                        Accumulator -= Numerator;
                        reuse_previous <<= 1;
                    }

                    pATIHW->horz_stretching |= HORZ_STRETCH_EN |
                        SetBits(horz_stretch_loop, HORZ_STRETCH_LOOP) |
                        SetBits(horz_stretch_ratio, HORZ_STRETCH_RATIO);
                    break;      /* Out of the do { ... } while (0) */
                }
            }

            pATIHW->horz_stretching |= (HORZ_STRETCH_MODE | HORZ_STRETCH_EN) |
                SetBits((HDisplay * (MaxBits(HORZ_STRETCH_BLEND) + 1)) /
                        pATI->LCDHorizontal, HORZ_STRETCH_BLEND);
        } while (0);

        if (!pATI->OptionPanelDisplay || (VDisplay >= pATI->LCDVertical))
        {
            pATIHW->vert_stretching = 0;
        }
        else
        {
            pATIHW->vert_stretching = (VERT_STRETCH_USE0 | VERT_STRETCH_EN) |
                SetBits((VDisplay * (MaxBits(VERT_STRETCH_RATIO0) + 1)) /
                        pATI->LCDVertical, VERT_STRETCH_RATIO0);
        }

#ifndef AVOID_CPIO

        /* Copy non-shadow CRTC register values to the shadow set */
        for (Index = 0;  Index < NumberOf(pATIHW->shadow_vga);  Index++)
            pATIHW->shadow_vga[Index] = pATIHW->crt[Index];

#endif /* AVOID_CPIO */

        pATIHW->shadow_h_total_disp = pATIHW->crtc_h_total_disp;
        pATIHW->shadow_h_sync_strt_wid = pATIHW->crtc_h_sync_strt_wid;
        pATIHW->shadow_v_total_disp = pATIHW->crtc_v_total_disp;
        pATIHW->shadow_v_sync_strt_wid = pATIHW->crtc_v_sync_strt_wid;
    }

    /* Fill in clock data */
    if (!ATIClockCalculate(iScreen, pATI, pATIHW, pMode))
        return FALSE;

    /* Setup ECP clock divider */
    if (pATI->Chip >= ATI_CHIP_264VT)
    {
        if (pATI->Chip <= ATI_CHIP_264VT3)
            MaxScalerClock = 80000;
        else if (pATI->Chip <= ATI_CHIP_264GT2C)
            MaxScalerClock = 100000;
        else if (pATI->Chip == ATI_CHIP_264GTPRO)
            MaxScalerClock = 125000;
        else if (pATI->Chip <= ATI_CHIP_MOBILITY)
            MaxScalerClock = 135000;
        else
            MaxScalerClock = 80000;     /* Conservative */
        pATIHW->pll_vclk_cntl &= ~PLL_ECP_DIV;
        /* XXX Don't do this for TVOut! */
        ECPClock = pMode->SynthClock;
        for (Index = 0;  (ECPClock > MaxScalerClock) && (Index < 2);  Index++)
            ECPClock >>= 1;
        pATIHW->pll_vclk_cntl |= SetBits(Index, PLL_ECP_DIV);
    }
    else if (pATI->DAC == ATI_DAC_IBMRGB514)
    {
        ATIRGB514Calculate(pATI, pATIHW, pMode);
    }

    return TRUE;
}

/*
 * ATIModeSet --
 *
 * This function sets a video mode.  It writes out all video state data that
 * has been previously calculated or saved.
 */
void
ATIModeSet
(
    ScrnInfoPtr pScreenInfo,
    ATIPtr      pATI,
    ATIHWPtr    pATIHW
)
{

#ifndef AVOID_CPIO

    int Index;

    /* Get back to bank 0 */
    (*pATIHW->SetBank)(pATI, 0);

#endif /* AVOID_CPIO */

    if (pATI->Chip >= ATI_CHIP_88800GXC)
    {
        /* Stop CRTC */
        outr(CRTC_GEN_CNTL,
            pATIHW->crtc_gen_cntl & ~(CRTC_EXT_DISP_EN | CRTC_EN));

        if (pATI->Chip >= ATI_CHIP_264CT)
        {
            ATIMach64PutPLLReg(PLL_VCLK_CNTL, pATIHW->pll_vclk_cntl);
            ATIMach64PutPLLReg(PLL_VCLK_POST_DIV, pATIHW->pll_vclk_post_div);
            ATIMach64PutPLLReg(PLL_VCLK0_FB_DIV, pATIHW->pll_vclk0_fb_div);
            ATIMach64PutPLLReg(PLL_VCLK1_FB_DIV, pATIHW->pll_vclk1_fb_div);
            ATIMach64PutPLLReg(PLL_VCLK2_FB_DIV, pATIHW->pll_vclk2_fb_div);
            ATIMach64PutPLLReg(PLL_VCLK3_FB_DIV, pATIHW->pll_vclk3_fb_div);
            ATIMach64PutPLLReg(PLL_XCLK_CNTL, pATIHW->pll_xclk_cntl);
            if (pATI->Chip >= ATI_CHIP_264LT)
                ATIMach64PutPLLReg(PLL_EXT_VPLL_CNTL,
                    pATIHW->pll_ext_vpll_cntl);
            ATIMach64PutPLLReg(PLL_VCLK_CNTL,
                pATIHW->pll_vclk_cntl & ~PLL_VCLK_RESET);

            /* Load LCD registers */
            if (pATI->LCDPanelID >= 0)
            {
                if (pATI->Chip == ATI_CHIP_264LT)
                {
                    /* Update non-shadow registers first */
                    outr(LCD_GEN_CTRL, pATIHW->lcd_gen_ctrl & ~SHADOW_RW_EN);

                    /* Temporarily disable stretching */
                    outr(HORZ_STRETCHING, pATIHW->horz_stretching &
                        ~(HORZ_STRETCH_MODE | HORZ_STRETCH_EN));
                    outr(VERT_STRETCHING, pATIHW->vert_stretching &
                        ~(VERT_STRETCH_RATIO1 | VERT_STRETCH_RATIO2 |
                          VERT_STRETCH_USE0 | VERT_STRETCH_EN));
                }
                else /* if ((pATI->Chip == ATI_CHIP_264LTPRO) ||
                            (pATI->Chip == ATI_CHIP_264XL) ||
                            (pATI->Chip == ATI_CHIP_MOBILITY)) */
                {
                    /* Update non-shadow registers first */
                    ATIMach64PutLCDReg(LCD_CONFIG_PANEL, pATIHW->config_panel);
                    ATIMach64PutLCDReg(LCD_GEN_CNTL, pATIHW->lcd_gen_ctrl &
                        ~(CRTC_RW_SELECT | SHADOW_RW_EN));

                    /* Temporarily disable stretching */
                    ATIMach64PutLCDReg(LCD_HORZ_STRETCHING,
                        pATIHW->horz_stretching &
                        ~(HORZ_STRETCH_MODE | HORZ_STRETCH_EN));
                    ATIMach64PutLCDReg(LCD_VERT_STRETCHING,
                        pATIHW->vert_stretching &
                        ~(VERT_STRETCH_RATIO1 | VERT_STRETCH_RATIO2 |
                          VERT_STRETCH_USE0 | VERT_STRETCH_EN));
                }
            }
        }
    }

    switch (pATIHW->crtc)
    {

#ifndef AVOID_CPIO

        case ATI_CRTC_VGA:
            /* Start sequencer reset */
            PutReg(SEQX, 0x00U, 0x00U);

            /* Set pixel clock */
            if ((pATIHW->FeedbackDivider > 0) &&
                (pATI->ProgrammableClock > ATI_CLOCK_FIXED))
                ATIClockSet(pATI, pATIHW);

            /* Set up RAMDAC */
            if (pATI->DAC == ATI_DAC_IBMRGB514)
                ATIRGB514Set(pATI, pATIHW);

            /* Load VGA Wonder */
            if (pATI->CPIO_VGAWonder)
                ATIVGAWonderSet(pATI, pATIHW);

            /* Load VGA device */
            ATIVGASet(pATI, pATIHW);

            /* Load Mach64 registers */
            if (pATI->Chip >= ATI_CHIP_88800GXC)
            {
                /* Load MMIO registers */
                if (pATI->Block0Base)
                    ATIMach64Set(pATI, pATIHW);

                outr(CRTC_GEN_CNTL, pATIHW->crtc_gen_cntl);
                outr(CUR_CLR0, pATIHW->cur_clr0);
                outr(CUR_CLR1, pATIHW->cur_clr1);
                outr(CUR_OFFSET, pATIHW->cur_offset);
                outr(CUR_HORZ_VERT_POSN, pATIHW->cur_horz_vert_posn);
                outr(CUR_HORZ_VERT_OFF, pATIHW->cur_horz_vert_off);
                outr(BUS_CNTL, pATIHW->bus_cntl);
                outr(MEM_VGA_WP_SEL, pATIHW->mem_vga_wp_sel);
                outr(MEM_VGA_RP_SEL, pATIHW->mem_vga_rp_sel);
                outr(DAC_CNTL, pATIHW->dac_cntl);
                outr(GEN_TEST_CNTL, pATIHW->gen_test_cntl | GEN_GUI_EN);
                outr(GEN_TEST_CNTL, pATIHW->gen_test_cntl);
                outr(GEN_TEST_CNTL, pATIHW->gen_test_cntl | GEN_GUI_EN);
                outr(CONFIG_CNTL, pATIHW->config_cntl);
                if (pATI->Chip >= ATI_CHIP_264CT)
                {
                    outr(CRTC_H_TOTAL_DISP, pATIHW->crtc_h_total_disp);
                    outr(CRTC_H_SYNC_STRT_WID, pATIHW->crtc_h_sync_strt_wid);
                    outr(CRTC_V_TOTAL_DISP, pATIHW->crtc_v_total_disp);
                    outr(CRTC_V_SYNC_STRT_WID, pATIHW->crtc_v_sync_strt_wid);
                    outr(CRTC_OFF_PITCH, pATIHW->crtc_off_pitch);
                    if (pATI->Chip >= ATI_CHIP_264VTB)
                    {
                        outr(MEM_CNTL, pATIHW->mem_cntl);
                        outr(MPP_CONFIG, pATIHW->mpp_config);
                        outr(MPP_STROBE_SEQ, pATIHW->mpp_strobe_seq);
                        outr(TVO_CNTL, pATIHW->tvo_cntl);
                    }
                }
            }

            break;

#endif /* AVOID_CPIO */

        case ATI_CRTC_MACH64:
            /* Load Mach64 CRTC registers */
            ATIMach64Set(pATI, pATIHW);

#ifndef AVOID_CPIO

            if (pATI->UseSmallApertures)
            {
                /* Oddly enough, these need to be set also, maybe others */
                PutReg(SEQX, 0x02U, pATIHW->seq[2]);
                PutReg(SEQX, 0x04U, pATIHW->seq[4]);
                PutReg(GRAX, 0x06U, pATIHW->gra[6]);
                if (pATI->CPIO_VGAWonder)
                    ATIModifyExtReg(pATI, 0xB6U, -1, 0x00U, pATIHW->b6);
            }

#endif /* AVOID_CPIO */

            break;

        default:
            break;
    }

    if (pATI->LCDPanelID >= 0)
    {
        /* Switch to shadow registers */
        if (pATI->Chip == ATI_CHIP_264LT)
            outr(LCD_GEN_CTRL, pATIHW->lcd_gen_ctrl | SHADOW_RW_EN);
        else /* if ((pATI->Chip == ATI_CHIP_264LTPRO) ||
                    (pATI->Chip == ATI_CHIP_264XL) ||
                    (pATI->Chip == ATI_CHIP_MOBILITY)) */
            ATIMach64PutLCDReg(LCD_GEN_CNTL,
                (pATIHW->lcd_gen_ctrl & ~CRTC_RW_SELECT) | SHADOW_RW_EN);

        /* Restore shadow registers */
        switch (pATIHW->crtc)
        {

#ifndef AVOID_CPIO

            case ATI_CRTC_VGA:
                for (Index = 0;
                     Index < NumberOf(pATIHW->shadow_vga);
                     Index++)
                    PutReg(CRTX(pATI->CPIO_VGABase), Index,
                        pATIHW->shadow_vga[Index]);
                /* Fall through */

#endif /* AVOID_CPIO */

            case ATI_CRTC_MACH64:
                outr(CRTC_H_TOTAL_DISP, pATIHW->shadow_h_total_disp);
                outr(CRTC_H_SYNC_STRT_WID, pATIHW->shadow_h_sync_strt_wid);
                outr(CRTC_V_TOTAL_DISP, pATIHW->shadow_v_total_disp);
                outr(CRTC_V_SYNC_STRT_WID, pATIHW->shadow_v_sync_strt_wid);
                break;

            default:
                break;
        }

        /* Restore CRTC selection & shadow state and enable stretching */
        if (pATI->Chip == ATI_CHIP_264LT)
        {
            outr(LCD_GEN_CTRL, pATIHW->lcd_gen_ctrl);
            outr(HORZ_STRETCHING, pATIHW->horz_stretching);
            outr(VERT_STRETCHING, pATIHW->vert_stretching);
        }
        else /* if ((pATI->Chip == ATI_CHIP_264LTPRO) ||
                    (pATI->Chip == ATI_CHIP_264XL) ||
                    (pATI->Chip == ATI_CHIP_MOBILITY)) */
        {
            ATIMach64PutLCDReg(LCD_GEN_CNTL, pATIHW->lcd_gen_ctrl);
            ATIMach64PutLCDReg(LCD_HORZ_STRETCHING, pATIHW->horz_stretching);
            ATIMach64PutLCDReg(LCD_VERT_STRETCHING, pATIHW->vert_stretching);
            ATIMach64PutLCDReg(LCD_EXT_VERT_STRETCH, pATIHW->ext_vert_stretch);
            outr(LCD_INDEX, pATIHW->lcd_index);
        }
    }

    /*
     * Set DSP registers.  Note that, for some reason, sequencer resets clear
     * the DSP_CONFIG register on early integrated controllers.
     */
    if (pATI->Chip >= ATI_CHIP_264VTB)
        ATIDSPSet(pATI, pATIHW);

    /* Load RAMDAC */
    ATIDACSet(pATI, pATIHW);

    /* Reset hardware cursor caching */
    pATI->CursorXOffset = pATI->CursorYOffset = (CARD16)(-1);

#ifndef AVOID_CPIO

    /* Restore video memory */
    ATISwap(pScreenInfo->scrnIndex, pATI, pATIHW, TRUE);

    if (pATI->VGAAdapter != ATI_ADAPTER_NONE)
        ATIVGASaveScreen(pATI, SCREEN_SAVER_OFF);       /* Turn on screen */

#endif /* AVOID_CPIO */

    if ((xf86GetVerbosity() > 3) && (pATIHW == &pATI->NewHW))
    {
        xf86ErrorFVerb(4, "\n After setting mode \"%s\":\n\n",
            pScreenInfo->currentMode->name);
        ATIPrintMode(pScreenInfo->currentMode);
        ATIPrintRegisters(pATI);
    }
}

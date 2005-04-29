/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/i128/i128init.c,v 1.6 2003/09/24 02:43:23 dawes Exp $ */
/*
 * Copyright 1995-2000 by Robin Cutshaw <robin@XFree86.Org>
 * Copyright 1998 by Number Nine Visual Technology, Inc.
 * 
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Robin Cutshaw not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  Robin Cutshaw and Number Nine make no
 * representations about the suitability of this software for any purpose.  It
 * is provided "as is" without express or implied warranty.
 *
 * ROBIN CUTSHAW AND NUMBER NINE DISCLAIM ALL WARRANTIES WITH REGARD TO THIS
 * SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS, IN NO EVENT SHALL ROBIN CUTSHAW OR NUMBER NINE BE LIABLE FOR
 * ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 */

/* $XConsortium: $ */


#include "xf86.h"
#include "xf86_ansic.h"
#include "xf86PciInfo.h"
#include "xf86Pci.h"

#include "i128.h"
#include "i128reg.h"
#include "Ti302X.h"
#include "IBMRGB.h"

void I128SavePalette(I128Ptr pI128);
void I128RestorePalette(I128Ptr pI128);


void
I128SaveState(ScrnInfoPtr pScrn)
{
        I128Ptr pI128 = I128PTR(pScrn);
	I128RegPtr iR = &pI128->RegRec;

        if (pI128->Debug)
        	xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "I128SaveState start\n");

	if (pI128->Debug) {
		unsigned long tmp1 = inl(iR->iobase + 0x1C);
		unsigned long tmp2 = inl(iR->iobase + 0x20);
        	xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "I128SaveState saving, config1/2 = 0x%lx/0x%lx\n", tmp1, tmp2);
		I128DumpActiveRegisters(pScrn);
	}

	/* iobase is filled in during the device probe (as well as config 1&2)*/

	if ((pI128->io.id&0x7) > 0) {
		iR->vga_ctl = inl(iR->iobase + 0x30);
	}

	iR->i128_base_g[INT_VCNT] = pI128->mem.rbase_g[INT_VCNT]; /*  0x0020  */
	iR->i128_base_g[INT_HCNT] = pI128->mem.rbase_g[INT_HCNT]; /*  0x0024  */
	iR->i128_base_g[DB_ADR]   = pI128->mem.rbase_g[DB_ADR];   /*  0x0028  */
	iR->i128_base_g[DB_PTCH]  = pI128->mem.rbase_g[DB_PTCH];  /*  0x002C  */
	iR->i128_base_g[CRT_HAC]  = pI128->mem.rbase_g[CRT_HAC];  /*  0x0030  */
	iR->i128_base_g[CRT_HBL]  = pI128->mem.rbase_g[CRT_HBL];  /*  0x0034  */
	iR->i128_base_g[CRT_HFP]  = pI128->mem.rbase_g[CRT_HFP];  /*  0x0038  */
	iR->i128_base_g[CRT_HS]   = pI128->mem.rbase_g[CRT_HS];   /*  0x003C  */
	iR->i128_base_g[CRT_VAC]  = pI128->mem.rbase_g[CRT_VAC];  /*  0x0040  */
	iR->i128_base_g[CRT_VBL]  = pI128->mem.rbase_g[CRT_VBL];  /*  0x0044  */
	iR->i128_base_g[CRT_VFP]  = pI128->mem.rbase_g[CRT_VFP];  /*  0x0048  */
	iR->i128_base_g[CRT_VS]   = pI128->mem.rbase_g[CRT_VS];   /*  0x004C  */
	iR->i128_base_g[CRT_LCNT] = pI128->mem.rbase_g[CRT_LCNT]; /*  0x0050  */
	iR->i128_base_g[CRT_ZOOM] = pI128->mem.rbase_g[CRT_ZOOM]; /*  0x0054  */
	iR->i128_base_g[CRT_1CON] = pI128->mem.rbase_g[CRT_1CON]; /*  0x0058  */
	iR->i128_base_g[CRT_2CON] = pI128->mem.rbase_g[CRT_2CON]; /*  0x005C  */

	iR->i128_base_w[MW0_CTRL] = pI128->mem.rbase_w[MW0_CTRL]; /*  0x0000  */
	iR->i128_base_w[MW0_SZ]   = pI128->mem.rbase_w[MW0_SZ];   /*  0x0008  */
	iR->i128_base_w[MW0_PGE]  = pI128->mem.rbase_w[MW0_PGE];  /*  0x000C  */
	iR->i128_base_w[MW0_ORG]  = pI128->mem.rbase_w[MW0_ORG];  /*  0x0010  */
	iR->i128_base_w[MW0_MSRC] = pI128->mem.rbase_w[MW0_MSRC]; /*  0x0018  */
	iR->i128_base_w[MW0_WKEY] = pI128->mem.rbase_w[MW0_WKEY]; /*  0x001C  */
	iR->i128_base_w[MW0_KDAT] = pI128->mem.rbase_w[MW0_KDAT]; /*  0x0020  */
	iR->i128_base_w[MW0_MASK] = pI128->mem.rbase_w[MW0_MASK]; /*  0x0024  */

	if (pI128->RamdacType == TI3025_DAC) {
		pI128->mem.rbase_g[INDEX_TI] = TI_CURS_CONTROL;		MB;
		iR->Ti302X[TI_CURS_CONTROL] = pI128->mem.rbase_g[DATA_TI];
		pI128->mem.rbase_g[INDEX_TI] = TI_TRUE_COLOR_CONTROL;	MB;
		iR->Ti302X[TI_TRUE_COLOR_CONTROL] = pI128->mem.rbase_g[DATA_TI];
		pI128->mem.rbase_g[INDEX_TI] = TI_VGA_SWITCH_CONTROL;	MB;
		iR->Ti302X[TI_VGA_SWITCH_CONTROL] = pI128->mem.rbase_g[DATA_TI];
		pI128->mem.rbase_g[INDEX_TI] = TI_MUX_CONTROL_1;	MB;
		iR->Ti302X[TI_MUX_CONTROL_1] = pI128->mem.rbase_g[DATA_TI];
		pI128->mem.rbase_g[INDEX_TI] = TI_MUX_CONTROL_2;	MB;
		iR->Ti302X[TI_MUX_CONTROL_2] = pI128->mem.rbase_g[DATA_TI];
		pI128->mem.rbase_g[INDEX_TI] = TI_INPUT_CLOCK_SELECT;	MB;
		iR->Ti302X[TI_INPUT_CLOCK_SELECT] = pI128->mem.rbase_g[DATA_TI];
		pI128->mem.rbase_g[INDEX_TI] = TI_OUTPUT_CLOCK_SELECT;	MB;
		iR->Ti302X[TI_OUTPUT_CLOCK_SELECT] = pI128->mem.rbase_g[DATA_TI];
		pI128->mem.rbase_g[INDEX_TI] = TI_PALETTE_PAGE;		MB;
		iR->Ti302X[TI_PALETTE_PAGE] = pI128->mem.rbase_g[DATA_TI];
		pI128->mem.rbase_g[INDEX_TI] = TI_GENERAL_CONTROL;	MB;
		iR->Ti302X[TI_GENERAL_CONTROL] = pI128->mem.rbase_g[DATA_TI];
		pI128->mem.rbase_g[INDEX_TI] = TI_MISC_CONTROL;		MB;
		iR->Ti302X[TI_MISC_CONTROL] = pI128->mem.rbase_g[DATA_TI];
		pI128->mem.rbase_g[INDEX_TI] = TI_AUXILIARY_CONTROL;	MB;
		iR->Ti302X[TI_AUXILIARY_CONTROL] = pI128->mem.rbase_g[DATA_TI];
		pI128->mem.rbase_g[INDEX_TI] = TI_GENERAL_IO_CONTROL;	MB;
		iR->Ti302X[TI_GENERAL_IO_CONTROL] = pI128->mem.rbase_g[DATA_TI];
		pI128->mem.rbase_g[INDEX_TI] = TI_GENERAL_IO_DATA;	MB;
		iR->Ti302X[TI_GENERAL_IO_DATA] = pI128->mem.rbase_g[DATA_TI];
		pI128->mem.rbase_g[INDEX_TI] = TI_MCLK_DCLK_CONTROL;	MB;
		iR->Ti302X[TI_MCLK_DCLK_CONTROL] = pI128->mem.rbase_g[DATA_TI];
		pI128->mem.rbase_g[INDEX_TI] = TI_COLOR_KEY_CONTROL;	MB;
		iR->Ti302X[TI_COLOR_KEY_CONTROL] = pI128->mem.rbase_g[DATA_TI];

		pI128->mem.rbase_g[INDEX_TI] = TI_PLL_CONTROL;		MB;
		pI128->mem.rbase_g[DATA_TI] = 0x00;			MB;
		pI128->mem.rbase_g[INDEX_TI] = TI_PIXEL_CLOCK_PLL_DATA;	MB;
		iR->Ti3025[0] = pI128->mem.rbase_g[DATA_TI];

		pI128->mem.rbase_g[INDEX_TI] = TI_PLL_CONTROL;		MB;
		pI128->mem.rbase_g[DATA_TI] = 0x01;			MB;
		pI128->mem.rbase_g[INDEX_TI] = TI_PIXEL_CLOCK_PLL_DATA;	MB;
		iR->Ti3025[1] = pI128->mem.rbase_g[DATA_TI];

		pI128->mem.rbase_g[INDEX_TI] = TI_PLL_CONTROL;		MB;
		pI128->mem.rbase_g[DATA_TI] = 0x02;			MB;
		pI128->mem.rbase_g[INDEX_TI] = TI_PIXEL_CLOCK_PLL_DATA;	MB;
		iR->Ti3025[2] = pI128->mem.rbase_g[DATA_TI];

		pI128->mem.rbase_g[INDEX_TI] = TI_PLL_CONTROL;		MB;
		pI128->mem.rbase_g[DATA_TI] = 0x00;			MB;
		pI128->mem.rbase_g[INDEX_TI] = TI_MCLK_PLL_DATA;	MB;
		iR->Ti3025[3] = pI128->mem.rbase_g[DATA_TI];

		pI128->mem.rbase_g[INDEX_TI] = TI_PLL_CONTROL;		MB;
		pI128->mem.rbase_g[DATA_TI] = 0x01;			MB;
		pI128->mem.rbase_g[INDEX_TI] = TI_MCLK_PLL_DATA;	MB;
		iR->Ti3025[4] = pI128->mem.rbase_g[DATA_TI];

		pI128->mem.rbase_g[INDEX_TI] = TI_PLL_CONTROL;		MB;
		pI128->mem.rbase_g[DATA_TI] = 0x02;			MB;
		pI128->mem.rbase_g[INDEX_TI] = TI_MCLK_PLL_DATA;	MB;
		iR->Ti3025[5] = pI128->mem.rbase_g[DATA_TI];

		pI128->mem.rbase_g[INDEX_TI] = TI_PLL_CONTROL;		MB;
		pI128->mem.rbase_g[DATA_TI] = 0x00;			MB;
		pI128->mem.rbase_g[INDEX_TI] = TI_LOOP_CLOCK_PLL_DATA;	MB;
		iR->Ti3025[6] = pI128->mem.rbase_g[DATA_TI];

		pI128->mem.rbase_g[INDEX_TI] = TI_PLL_CONTROL;		MB;
		pI128->mem.rbase_g[DATA_TI] = 0x01;			MB;
		pI128->mem.rbase_g[INDEX_TI] = TI_LOOP_CLOCK_PLL_DATA;	MB;
		iR->Ti3025[7] = pI128->mem.rbase_g[DATA_TI];

		pI128->mem.rbase_g[INDEX_TI] = TI_PLL_CONTROL;		MB;
		pI128->mem.rbase_g[DATA_TI] = 0x02;			MB;
		pI128->mem.rbase_g[INDEX_TI] = TI_LOOP_CLOCK_PLL_DATA;	MB;
		iR->Ti3025[8] = pI128->mem.rbase_g[DATA_TI];
	} else if ((pI128->RamdacType == IBM526_DAC) ||
		   (pI128->RamdacType == IBM528_DAC) ||
		   (pI128->RamdacType == SILVER_HAMMER_DAC)) {
		CARD32 i;

		for (i=0; i<0x94; i++) {
			pI128->mem.rbase_g[IDXL_I] = i;			MB;
			iR->IBMRGB[i] = pI128->mem.rbase_g[DATA_I];
		}
	}

	I128SavePalette(pI128);

        if (pI128->Debug)
        	xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "I128SaveState complete\n");

}


void
I128RestoreState(ScrnInfoPtr pScrn)
{
        I128Ptr pI128 = I128PTR(pScrn);
	I128RegPtr iR = &pI128->RegRec;

        if (pI128->Debug)
        	xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "I128RestoreState start\n");

	if (pI128->RamdacType == TI3025_DAC) {
		pI128->mem.rbase_g[INDEX_TI] = TI_PLL_CONTROL;		MB;
		pI128->mem.rbase_g[DATA_TI] = 0x00;			MB;
		pI128->mem.rbase_g[INDEX_TI] = TI_PIXEL_CLOCK_PLL_DATA;	MB;
		pI128->mem.rbase_g[DATA_TI] = iR->Ti3025[0];		MB;

		pI128->mem.rbase_g[INDEX_TI] = TI_PLL_CONTROL;		MB;
		pI128->mem.rbase_g[DATA_TI] = 0x01;			MB;
		pI128->mem.rbase_g[INDEX_TI] = TI_PIXEL_CLOCK_PLL_DATA;	MB;
		pI128->mem.rbase_g[DATA_TI] = iR->Ti3025[1];		MB;

		pI128->mem.rbase_g[INDEX_TI] = TI_PLL_CONTROL;		MB;
		pI128->mem.rbase_g[DATA_TI] = 0x02;			MB;
		pI128->mem.rbase_g[INDEX_TI] = TI_PIXEL_CLOCK_PLL_DATA;	MB;
		pI128->mem.rbase_g[DATA_TI] = iR->Ti3025[2];		MB;

		pI128->mem.rbase_g[INDEX_TI] = TI_PLL_CONTROL;		MB;
		pI128->mem.rbase_g[DATA_TI] = 0x00;			MB;
		pI128->mem.rbase_g[INDEX_TI] = TI_MCLK_PLL_DATA;	MB;
		pI128->mem.rbase_g[DATA_TI] = iR->Ti3025[3];		MB;

		pI128->mem.rbase_g[INDEX_TI] = TI_PLL_CONTROL;		MB;
		pI128->mem.rbase_g[DATA_TI] = 0x01;			MB;
		pI128->mem.rbase_g[INDEX_TI] = TI_MCLK_PLL_DATA;	MB;
		pI128->mem.rbase_g[DATA_TI] = iR->Ti3025[4];		MB;

		pI128->mem.rbase_g[INDEX_TI] = TI_PLL_CONTROL;		MB;
		pI128->mem.rbase_g[DATA_TI] = 0x02;			MB;
		pI128->mem.rbase_g[INDEX_TI] = TI_MCLK_PLL_DATA;	MB;
		pI128->mem.rbase_g[DATA_TI] = iR->Ti3025[5];		MB;

		pI128->mem.rbase_g[INDEX_TI] = TI_PLL_CONTROL;		MB;
		pI128->mem.rbase_g[DATA_TI] = 0x00;			MB;
		pI128->mem.rbase_g[INDEX_TI] = TI_LOOP_CLOCK_PLL_DATA;	MB;
		pI128->mem.rbase_g[DATA_TI] = iR->Ti3025[6];		MB;

		pI128->mem.rbase_g[INDEX_TI] = TI_PLL_CONTROL;		MB;
		pI128->mem.rbase_g[DATA_TI] = 0x01;			MB;
		pI128->mem.rbase_g[INDEX_TI] = TI_LOOP_CLOCK_PLL_DATA;	MB;
		pI128->mem.rbase_g[DATA_TI] = iR->Ti3025[7];		MB;

		pI128->mem.rbase_g[INDEX_TI] = TI_PLL_CONTROL;		MB;
		pI128->mem.rbase_g[DATA_TI] = 0x02;			MB;
		pI128->mem.rbase_g[INDEX_TI] = TI_LOOP_CLOCK_PLL_DATA;	MB;
		pI128->mem.rbase_g[DATA_TI] = iR->Ti3025[8];		MB;

		pI128->mem.rbase_g[INDEX_TI] = TI_CURS_CONTROL;		MB;
		pI128->mem.rbase_g[DATA_TI] = iR->Ti302X[TI_CURS_CONTROL];MB;
		pI128->mem.rbase_g[INDEX_TI] = TI_TRUE_COLOR_CONTROL;	MB;
		pI128->mem.rbase_g[DATA_TI] = iR->Ti302X[TI_TRUE_COLOR_CONTROL]; MB;
		pI128->mem.rbase_g[INDEX_TI] = TI_VGA_SWITCH_CONTROL;	MB;
		pI128->mem.rbase_g[DATA_TI] = iR->Ti302X[TI_VGA_SWITCH_CONTROL]; MB;
		pI128->mem.rbase_g[INDEX_TI] = TI_MUX_CONTROL_1;	MB;
		pI128->mem.rbase_g[DATA_TI] = iR->Ti302X[TI_MUX_CONTROL_1];MB;
		pI128->mem.rbase_g[INDEX_TI] = TI_MUX_CONTROL_2;	MB;
		pI128->mem.rbase_g[DATA_TI] = iR->Ti302X[TI_MUX_CONTROL_2];MB;
		pI128->mem.rbase_g[INDEX_TI] = TI_INPUT_CLOCK_SELECT;	MB;
		pI128->mem.rbase_g[DATA_TI] = iR->Ti302X[TI_INPUT_CLOCK_SELECT]; MB;
		pI128->mem.rbase_g[INDEX_TI] = TI_OUTPUT_CLOCK_SELECT;	MB;
		pI128->mem.rbase_g[DATA_TI] = iR->Ti302X[TI_OUTPUT_CLOCK_SELECT];MB;
		pI128->mem.rbase_g[INDEX_TI] = TI_PALETTE_PAGE;		MB;
		pI128->mem.rbase_g[DATA_TI] = iR->Ti302X[TI_PALETTE_PAGE];MB;
		pI128->mem.rbase_g[INDEX_TI] = TI_GENERAL_CONTROL;	MB;
		pI128->mem.rbase_g[DATA_TI] = iR->Ti302X[TI_GENERAL_CONTROL]; MB;
		pI128->mem.rbase_g[INDEX_TI] = TI_MISC_CONTROL;		MB;
		pI128->mem.rbase_g[DATA_TI] = iR->Ti302X[TI_MISC_CONTROL];MB;
		pI128->mem.rbase_g[INDEX_TI] = TI_AUXILIARY_CONTROL;	MB;
		pI128->mem.rbase_g[DATA_TI] = iR->Ti302X[TI_AUXILIARY_CONTROL]; MB;
		pI128->mem.rbase_g[INDEX_TI] = TI_GENERAL_IO_CONTROL;	MB;
		pI128->mem.rbase_g[DATA_TI] = iR->Ti302X[TI_GENERAL_IO_CONTROL]; MB;
		pI128->mem.rbase_g[INDEX_TI] = TI_GENERAL_IO_DATA;	MB;
		pI128->mem.rbase_g[DATA_TI] = iR->Ti302X[TI_GENERAL_IO_DATA]; MB;
		pI128->mem.rbase_g[INDEX_TI] = TI_MCLK_DCLK_CONTROL;	MB;
		pI128->mem.rbase_g[DATA_TI] = iR->Ti302X[TI_MCLK_DCLK_CONTROL]; MB;
		pI128->mem.rbase_g[INDEX_TI] = TI_COLOR_KEY_CONTROL;	MB;
		pI128->mem.rbase_g[DATA_TI] = iR->Ti302X[TI_COLOR_KEY_CONTROL]; MB;
	} else if ((pI128->RamdacType == IBM526_DAC) ||
		   (pI128->RamdacType == IBM528_DAC) ||
		   (pI128->RamdacType == SILVER_HAMMER_DAC)) {
		CARD32 i;

		if (pI128->Debug) {
			unsigned long tmp1 = inl(iR->iobase + 0x1C);
			unsigned long tmp2 = inl(iR->iobase + 0x20);
       	 	xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "I128RestoreState restoring, config1/2 = 0x%lx/0x%lx\n", tmp1, tmp2);
			I128DumpActiveRegisters(pScrn);
		}

		for (i=0; i<0x94; i++) {
			if ((i == IBMRGB_sysclk_vco_div) ||
			    (i == IBMRGB_sysclk_ref_div))
				continue;
			pI128->mem.rbase_g[IDXL_I] = i;			MB;
			pI128->mem.rbase_g[DATA_I] = iR->IBMRGB[i];	MB;
		}

   		pI128->mem.rbase_g[IDXL_I] = IBMRGB_sysclk_ref_div;	MB;
   		pI128->mem.rbase_g[DATA_I] =
			iR->IBMRGB[IBMRGB_sysclk_ref_div];		MB;
   		pI128->mem.rbase_g[IDXL_I] = IBMRGB_sysclk_vco_div;	MB;
   		pI128->mem.rbase_g[DATA_I] =
			iR->IBMRGB[IBMRGB_sysclk_vco_div];		MB;
		usleep(50000);
	}

	/* iobase is filled in during the device probe (as well as config 1&2)*/

	if (((pI128->io.id&0x7) > 0) ||
	    (pI128->Chipset == PCI_CHIP_I128_T2R) ||
	    (pI128->Chipset == PCI_CHIP_I128_T2R4)) {
		outl(iR->iobase + 0x30, iR->vga_ctl);
	}

	I128RestorePalette(pI128);

	pI128->mem.rbase_w[MW0_CTRL] = iR->i128_base_w[MW0_CTRL]; /*  0x0000  */
	pI128->mem.rbase_w[MW0_SZ]   = iR->i128_base_w[MW0_SZ];   /*  0x0008  */
	pI128->mem.rbase_w[MW0_PGE]  = iR->i128_base_w[MW0_PGE];  /*  0x000C  */
	pI128->mem.rbase_w[MW0_ORG]  = iR->i128_base_w[MW0_ORG];  /*  0x0010  */
	pI128->mem.rbase_w[MW0_MSRC] = iR->i128_base_w[MW0_MSRC]; /*  0x0018  */
	pI128->mem.rbase_w[MW0_WKEY] = iR->i128_base_w[MW0_WKEY]; /*  0x001C  */
	pI128->mem.rbase_w[MW0_KDAT] = iR->i128_base_w[MW0_KDAT]; /*  0x0020  */
	pI128->mem.rbase_w[MW0_MASK] = iR->i128_base_w[MW0_MASK]; /*  0x0024  */
									MB;

	pI128->mem.rbase_g[INT_VCNT] = iR->i128_base_g[INT_VCNT]; /*  0x0020  */
	pI128->mem.rbase_g[INT_HCNT] = iR->i128_base_g[INT_HCNT]; /*  0x0024  */
	pI128->mem.rbase_g[DB_ADR]   = iR->i128_base_g[DB_ADR];   /*  0x0028  */
	pI128->mem.rbase_g[DB_PTCH]  = iR->i128_base_g[DB_PTCH];  /*  0x002C  */
	pI128->mem.rbase_g[CRT_HAC]  = iR->i128_base_g[CRT_HAC];  /*  0x0030  */
	pI128->mem.rbase_g[CRT_HBL]  = iR->i128_base_g[CRT_HBL];  /*  0x0034  */
	pI128->mem.rbase_g[CRT_HFP]  = iR->i128_base_g[CRT_HFP];  /*  0x0038  */
	pI128->mem.rbase_g[CRT_HS]   = iR->i128_base_g[CRT_HS];   /*  0x003C  */
	pI128->mem.rbase_g[CRT_VAC]  = iR->i128_base_g[CRT_VAC];  /*  0x0040  */
	pI128->mem.rbase_g[CRT_VBL]  = iR->i128_base_g[CRT_VBL];  /*  0x0044  */
	pI128->mem.rbase_g[CRT_VFP]  = iR->i128_base_g[CRT_VFP];  /*  0x0048  */
	pI128->mem.rbase_g[CRT_VS]   = iR->i128_base_g[CRT_VS];   /*  0x004C  */
	pI128->mem.rbase_g[CRT_LCNT] = iR->i128_base_g[CRT_LCNT]; /*  0x0050  */
	pI128->mem.rbase_g[CRT_ZOOM] = iR->i128_base_g[CRT_ZOOM]; /*  0x0054  */
	pI128->mem.rbase_g[CRT_1CON] = iR->i128_base_g[CRT_1CON]; /*  0x0058  */
	pI128->mem.rbase_g[CRT_2CON] = iR->i128_base_g[CRT_2CON]; /*  0x005C  */
									MB;

	if (pI128->Debug) {
		unsigned long tmp1 = inl(iR->iobase + 0x1C);
		unsigned long tmp2 = inl(iR->iobase + 0x20);
        	xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "I128RestoreState resetting config1/2 from 0x%lx/0x%lx to 0x%lx/0x%lx\n", tmp1, tmp2, (unsigned long)iR->config1, (unsigned long)iR->config2);
		I128DumpActiveRegisters(pScrn);
	}

	if (pI128->MemoryType == I128_MEMORY_SGRAM) {
		outl(iR->iobase + 0x24, iR->sgram & 0x7FFFFFFF);
		outl(iR->iobase + 0x24, iR->sgram | 0x80000000);
	}

	outl(iR->iobase + 0x20, iR->config2);
	outl(iR->iobase + 0x1C, iR->config1);

        if (pI128->Debug)
        	xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "I128RestoreState complete\n");
}


Bool
I128Init(ScrnInfoPtr pScrn, DisplayModePtr mode)
{
	I128Ptr pI128;
	I128RegPtr iR;
	int pitch_multiplier, iclock;
	Bool ret;
	CARD32 tmp;
	int doubled = 1;

	if (mode->Flags & V_DBLSCAN)
		doubled = 2;

        pI128 = I128PTR(pScrn);
	iR = &pI128->RegRec;
	pI128->HDisplay = mode->HDisplay;

        if (pI128->Debug)
        	xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "I128Init start\n");

	/* config 1 and 2 were saved in Probe()
	 * we reset here again in case there was a VT switch
	 */

	outl(iR->iobase + 0x1C, pI128->io.config1);
	outl(iR->iobase + 0x20, pI128->io.config2);

	if (pI128->MemoryType == I128_MEMORY_SGRAM) {
		outl(iR->iobase + 0x24, pI128->io.sgram & 0x7FFFFFFF);
		outl(iR->iobase + 0x24, pI128->io.sgram | 0x80000000);
	}


	if (pI128->bitsPerPixel == 32)		pitch_multiplier = 4;
	else if (pI128->bitsPerPixel == 16)	pitch_multiplier = 2;
	else					pitch_multiplier = 1;

	if (pI128->RamdacType == TI3025_DAC)
		iclock = 4;
	else if (pI128->RamdacType == IBM528_DAC)
		iclock = 128 / pI128->bitsPerPixel;
	else if (pI128->RamdacType == SILVER_HAMMER_DAC)
		iclock = 64 / pI128->bitsPerPixel;
	else if ((pI128->MemoryType == I128_MEMORY_DRAM) ||
		 (pI128->MemoryType == I128_MEMORY_SGRAM))
		iclock = 32 / pI128->bitsPerPixel; /* IBM526 DAC 32b bus */
	else
		iclock = 64 / pI128->bitsPerPixel; /* IBM524/526 DAC */

	pI128->mem.rbase_g[INT_VCNT] = 0x00;
	pI128->mem.rbase_g[INT_HCNT] = 0x00;
	pI128->mem.rbase_g[DB_ADR] = pI128->displayOffset;
	pI128->mem.rbase_g[DB_PTCH] = pI128->displayWidth * pitch_multiplier;
	pI128->mem.rbase_g[CRT_HAC] = mode->HDisplay/iclock;
	pI128->mem.rbase_g[CRT_HBL] = (mode->HTotal - mode->HDisplay)/iclock;
	pI128->mem.rbase_g[CRT_HFP] = (mode->HSyncStart - mode->HDisplay)/iclock;
	pI128->mem.rbase_g[CRT_HS] = (mode->HSyncEnd - mode->HSyncStart)/iclock;
	pI128->mem.rbase_g[CRT_VAC] = mode->VDisplay * doubled;
	pI128->mem.rbase_g[CRT_VBL] = (mode->VTotal - mode->VDisplay) * doubled;
	pI128->mem.rbase_g[CRT_VFP] = (mode->VSyncStart - mode->VDisplay)* doubled;
	pI128->mem.rbase_g[CRT_VS] = (mode->VSyncEnd - mode->VSyncStart) * doubled;
	tmp = 0x00000070;
	if (pI128->Chipset == PCI_CHIP_I128_T2R)
		tmp |= 0x00000100;
	if (pI128->Chipset == PCI_CHIP_I128_T2R4) {
		if (pI128->FlatPanel)
			tmp |= 0x00000100;    /* Turn on digital flat panel */
		else
			tmp &= 0xfffffeff;    /* Turn off digital flat panel */
	}
	if (pI128->DACSyncOnGreen || (mode->Flags & V_CSYNC))
		tmp |= 0x00000004;
	pI128->mem.rbase_g[CRT_1CON] = tmp;
	if ((pI128->MemoryType == I128_MEMORY_DRAM) ||
	    (pI128->MemoryType == I128_MEMORY_SGRAM))
		tmp = 0x20000100;
	else if (pI128->MemoryType == I128_MEMORY_WRAM)
		tmp = 0x00040100;
	else {
		tmp = 0x00040101;
		if (pI128->MemorySize == 2048)
			tmp |= 0x00000002;
		if ((pI128->displayWidth & (pI128->displayWidth-1)) ||
		    ((pI128->displayWidth * pI128->bitsPerPixel) > 32768L))
			tmp |= 0x01000000;  /* split transfer */
	}
	pI128->mem.rbase_g[CRT_2CON] = tmp;
        if (mode->Flags & V_DBLSCAN)
		pI128->DoubleScan = TRUE;
        else
		pI128->DoubleScan = FALSE;
	pI128->mem.rbase_g[CRT_ZOOM] = (pI128->DoubleScan ? 0x00000001 : 0x00000000);

	pI128->mem.rbase_w[MW0_CTRL] = 0x00000000;
	switch (pI128->MemorySize) {
		case 2048:
			pI128->mem.rbase_w[MW0_SZ]   = 0x00000009;
			break;
		case 8192:
			pI128->mem.rbase_w[MW0_SZ]   = 0x0000000B;
			break;
		case 8192+4096:
			/* no break */
		case 16384:
			pI128->mem.rbase_w[MW0_SZ]   = 0x0000000C;
			break;
		case 16384+4096:
			/* no break */
		case 16384+8192:
			/* no break */
		case 16384+8192+4096:
			/* no break */
		case 32768:
			pI128->mem.rbase_w[MW0_SZ]   = 0x0000000D;
			break;
		case 4096:
			/* no break */
		default:
			pI128->mem.rbase_w[MW0_SZ]   = 0x0000000A;/* default 4MB */
			break;
	}
	pI128->mem.rbase_w[MW0_PGE]  = 0x00000000;
	pI128->mem.rbase_w[MW0_ORG]  = 0x00000000;
	pI128->mem.rbase_w[MW0_MSRC] = 0x00000000;
	pI128->mem.rbase_w[MW0_WKEY] = 0x00000000;
	pI128->mem.rbase_w[MW0_KDAT] = 0x00000000;
	pI128->mem.rbase_w[MW0_MASK] = 0xFFFFFFFF;
									MB;

	if ((pI128->io.id&0x7) > 0 || pI128->Chipset == PCI_CHIP_I128_T2R
			        || pI128->Chipset == PCI_CHIP_I128_T2R4) {

	   	pI128->io.vga_ctl &= 0x0000FF00;
   		pI128->io.vga_ctl |= 0x00000082;
                if (pI128->FlatPanel && (mode->Flags & V_DBLSCAN))
		   pI128->io.vga_ctl |= 0x00000020;  /* Stretch horizontally */
   		outl(iR->iobase + 0x30, pI128->io.vga_ctl);

                if (pI128->Chipset == PCI_CHIP_I128_T2R4) {
                        outl(iR->iobase + 0x24, 0x211BF030);
			usleep(5000);
			outl(iR->iobase + 0x24, 0xA11BF030);
		} else if (pI128->MemoryType == I128_MEMORY_SGRAM) {
			outl(iR->iobase + 0x24, 0x21089030);
			usleep(5000);
			outl(iR->iobase + 0x24, 0xA1089030);
		}
	}

	ret = pI128->ProgramDAC(pScrn, mode);

	pI128->InitCursorFlag = TRUE;
	pI128->Initialized = 1;

        if (pI128->Debug)
        	xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "I128Init complete\n");

	return(ret);
}


void
I128SavePalette(I128Ptr pI128)
{
   short i;

   pI128->mem.rbase_g[PEL_MASK] = 0xff;					MB;

   if (!pI128->LUTSaved) {
   	pI128->mem.rbase_g[RD_ADR] = 0x00;				MB;
   	for (i=0; i<256; i++) {
   	   pI128->lutorig[i].r = pI128->mem.rbase_g[PAL_DAT];		MB;
   	   pI128->lutorig[i].g = pI128->mem.rbase_g[PAL_DAT];		MB;
   	   pI128->lutorig[i].b = pI128->mem.rbase_g[PAL_DAT];		MB;
   	}
	pI128->LUTSaved = TRUE;
   }
      
}


void
I128RestorePalette(I128Ptr pI128)
{
   int i;
   /* restore the LUT */

   pI128->mem.rbase_g[PEL_MASK] = 0xff;				MB;
   pI128->mem.rbase_g[WR_ADR] = 0x00;				MB;

   for (i=0; i<256; i++) {
      pI128->mem.rbase_g[PAL_DAT] = pI128->lutorig[i].r;		MB;
      pI128->mem.rbase_g[PAL_DAT] = pI128->lutorig[i].g;		MB;
      pI128->mem.rbase_g[PAL_DAT] = pI128->lutorig[i].b;		MB;
   }
}


void
I128LoadPalette(ScrnInfoPtr pScrn, int numColors, int *indices, LOCO *colors,
	VisualPtr pVisual)
{
   I128Ptr pI128;

   if (pVisual->nplanes != 8)
      return;

   pI128 = I128PTR(pScrn);

   pI128->mem.rbase_g[PEL_MASK] = 0xff;					MB;

   while (numColors--) {
      pI128->mem.rbase_g[WR_ADR] = *indices;				MB;
      pI128->mem.rbase_g[PAL_DAT] = colors[*indices].red;		MB;
      pI128->mem.rbase_g[PAL_DAT] = colors[*indices].green;		MB;
      pI128->mem.rbase_g[PAL_DAT] = colors[*indices].blue;		MB;
      indices++;
   }
}

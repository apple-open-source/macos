/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/nsc/panel/dora9211.c,v 1.1 2002/12/10 15:12:28 alanh Exp $ */
/*
 * $Workfile: dora9211.c $
 * $Revision: 1.1.1.1 $
 *
 * File Contents: This file contains the panel functions to interface 
 *                the dorado platform.
 *
 * SubModule:     Geode FlatPanel library
 *
 */

/* 
 * NSC_LIC_ALTERNATIVE_PREAMBLE
 *
 * Revision 1.0
 *
 * National Semiconductor Alternative GPL-BSD License
 *
 * National Semiconductor Corporation licenses this software 
 * ("Software"):
 *
 * Panel Library
 *
 * under one of the two following licenses, depending on how the 
 * Software is received by the Licensee.
 * 
 * If this Software is received as part of the Linux Framebuffer or
 * other GPL licensed software, then the GPL license designated 
 * NSC_LIC_GPL applies to this Software; in all other circumstances 
 * then the BSD-style license designated NSC_LIC_BSD shall apply.
 *
 * END_NSC_LIC_ALTERNATIVE_PREAMBLE */

/* NSC_LIC_BSD
 *
 * National Semiconductor Corporation Open Source License for 
 *
 * Panel Library
 *
 * (BSD License with Export Notice)
 *
 * Copyright (c) 1999-2001
 * National Semiconductor Corporation.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions 
 * are met: 
 *
 *   * Redistributions of source code must retain the above copyright 
 *     notice, this list of conditions and the following disclaimer. 
 *
 *   * Redistributions in binary form must reproduce the above 
 *     copyright notice, this list of conditions and the following 
 *     disclaimer in the documentation and/or other materials provided 
 *     with the distribution. 
 *
 *   * Neither the name of the National Semiconductor Corporation nor 
 *     the names of its contributors may be used to endorse or promote 
 *     products derived from this software without specific prior 
 *     written permission. 
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS 
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE 
 * NATIONAL SEMICONDUCTOR CORPORATION OR CONTRIBUTORS BE LIABLE FOR ANY 
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL 
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE 
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER 
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE,
 * INTELLECTUAL PROPERTY INFRINGEMENT, OR OTHERWISE) ARISING IN ANY WAY 
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY 
 * OF SUCH DAMAGE.
 *
 * EXPORT LAWS: THIS LICENSE ADDS NO RESTRICTIONS TO THE EXPORT LAWS OF 
 * YOUR JURISDICTION. It is licensee's responsibility to comply with 
 * any export regulations applicable in licensee's jurisdiction. Under 
 * CURRENT (2001) U.S. export regulations this software 
 * is eligible for export from the U.S. and can be downloaded by or 
 * otherwise exported or reexported worldwide EXCEPT to U.S. embargoed 
 * destinations which include Cuba, Iraq, Libya, North Korea, Iran, 
 * Syria, Sudan, Afghanistan and any other country to which the U.S. 
 * has embargoed goods and services. 
 *
 * END_NSC_LIC_BSD */

/* NSC_LIC_GPL
 *
 * National Semiconductor Corporation Gnu General Public License for 
 *
 * Panel Library
 *
 * (GPL License with Export Notice)
 *
 * Copyright (c) 1999-2001
 * National Semiconductor Corporation.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted under the terms of the GNU General 
 * Public License as published by the Free Software Foundation; either 
 * version 2 of the License, or (at your option) any later version  
 *
 * In addition to the terms of the GNU General Public License, neither 
 * the name of the National Semiconductor Corporation nor the names of 
 * its contributors may be used to endorse or promote products derived 
 * from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS 
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE 
 * NATIONAL SEMICONDUCTOR CORPORATION OR CONTRIBUTORS BE LIABLE FOR ANY 
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL 
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE 
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER 
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE, 
 * INTELLECTUAL PROPERTY INFRINGEMENT, OR OTHERWISE) ARISING IN ANY WAY 
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY 
 * OF SUCH DAMAGE. See the GNU General Public License for more details. 
 *
 * EXPORT LAWS: THIS LICENSE ADDS NO RESTRICTIONS TO THE EXPORT LAWS OF 
 * YOUR JURISDICTION. It is licensee's responsibility to comply with 
 * any export regulations applicable in licensee's jurisdiction. Under 
 * CURRENT (2001) U.S. export regulations this software 
 * is eligible for export from the U.S. and can be downloaded by or 
 * otherwise exported or reexported worldwide EXCEPT to U.S. embargoed 
 * destinations which include Cuba, Iraq, Libya, North Korea, Iran, 
 * Syria, Sudan, Afghanistan and any other country to which the U.S. 
 * has embargoed goods and services. 
 *
 * You should have received a copy of the GNU General Public License 
 * along with this file; if not, write to the Free Software Foundation, 
 * Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA 
 *
 * END_NSC_LIC_GPL */

#include "dora9211.h"

void
Dorado_Get_9211_Details(unsigned long flags, PPnl_PanelParams pParam)
{
   unsigned long DPanelType;
   int i;

   for (i = 0; i < 0x7fff; i++) {
   }

   Dorado9211GpioInit();

   for (i = 0; i < 5; i++)
      toggle_Centaurus_9211_clock();

   if (flags & PNL_PANELCHIP) {
      DPanelType = Dorado9211ReadReg(0x430);

      if ((DPanelType & 0xFFFF0000) == 0x92110000) {	/* found 9211 */
	 /* check the values for revision ID */
	 if (DPanelType >= 0x92110301)
	    pParam->PanelChip = PNL_9211_C;
	 else if ((DPanelType >= 0x92110101) && (DPanelType < 0x92110301))
	    pParam->PanelChip = PNL_9211_A;
	 else
	    pParam->PanelChip = PNL_UNKNOWN_CHIP;
      } else {				/* no 9211 present */
	 pParam->PanelChip = PNL_UNKNOWN_CHIP;
      }
   }

   if ((pParam->PanelChip != PNL_UNKNOWN_CHIP) && (flags & PNL_PANELSTAT)) {
      unsigned long PanelTypeOrg;
      unsigned char Panel_2Byte;

      DPanelType = Dorado9211ReadReg(0x438);
      DPanelType &= 0x00e8e8e8;
      DPanelType |= 0x00170000;
      Dorado9211WriteReg(0x438, DPanelType);
      DPanelType = 0;

      DPanelType = Dorado9211ReadReg(0x434);
      DPanelType = (DPanelType >> (DRD_LCDRESGPIO1 + 1));
      PanelTypeOrg = DPanelType >> 8;
      Panel_2Byte = (unsigned char)PanelTypeOrg;
      Panel_2Byte = (Panel_2Byte >> (DRD_LCDRESGPIO2 - DRD_LCDRESGPIO1 - 1));
      DPanelType = (DPanelType | ((unsigned int)Panel_2Byte << 8));
      DPanelType = DPanelType >> 1;
      PanelTypeOrg = DPanelType >> 8;
      Panel_2Byte = (unsigned char)PanelTypeOrg;
      Panel_2Byte = (Panel_2Byte >> (DRD_LCDRESGPIO3 - DRD_LCDRESGPIO2 - 1));
      DPanelType = (DPanelType | ((unsigned int)Panel_2Byte << 8));
      DPanelType = DPanelType >> 1;
      PanelTypeOrg = DPanelType >> 8;
      Panel_2Byte = (unsigned char)PanelTypeOrg;
      Panel_2Byte = (Panel_2Byte >> (DRD_LCDRESGPIO4 - DRD_LCDRESGPIO3 - 1));
      DPanelType = (DPanelType | ((unsigned int)Panel_2Byte << 8));
      DPanelType = DPanelType >> 5;
      DPanelType &= 0xf;

      switch (DPanelType) {
      case 8:
	 pParam->PanelStat.XRes = 800;
	 pParam->PanelStat.YRes = 600;
	 pParam->PanelStat.Depth = 18;
	 pParam->PanelStat.MonoColor = PNL_COLOR_PANEL;
	 pParam->PanelStat.Type = PNL_TFT;
	 break;

      case 9:
	 pParam->PanelStat.XRes = 640;
	 pParam->PanelStat.YRes = 480;
	 pParam->PanelStat.Depth = 8;
	 pParam->PanelStat.MonoColor = PNL_COLOR_PANEL;
	 pParam->PanelStat.Type = PNL_SSTN;
	 break;

      case 10:
	 pParam->PanelStat.XRes = 1024;
	 pParam->PanelStat.YRes = 768;
	 pParam->PanelStat.Depth = 18;
	 pParam->PanelStat.MonoColor = PNL_COLOR_PANEL;
	 pParam->PanelStat.Type = PNL_TFT;
	 break;
      case 0:
      case 1:
      case 2:
      case 3:
      case 4:
      case 5:
      case 6:
      case 7:
      case 11:
	 pParam->PanelStat.XRes = 640;
	 pParam->PanelStat.YRes = 480;
	 pParam->PanelStat.Depth = 16;
	 pParam->PanelStat.MonoColor = PNL_COLOR_PANEL;
	 pParam->PanelStat.Type = PNL_DSTN;
	 break;
      case 12:
	 pParam->PanelStat.XRes = 640;
	 pParam->PanelStat.YRes = 480;
	 pParam->PanelStat.Depth = 18;
	 pParam->PanelStat.MonoColor = PNL_COLOR_PANEL;
	 pParam->PanelStat.Type = PNL_TFT;
	 break;
      case 13:
	 pParam->PanelStat.XRes = 1024;
	 pParam->PanelStat.YRes = 768;
	 pParam->PanelStat.Depth = 24;
	 pParam->PanelStat.MonoColor = PNL_COLOR_PANEL;
	 pParam->PanelStat.Type = PNL_DSTN;
	 break;
      case 14:
	 pParam->PanelStat.XRes = 640;
	 pParam->PanelStat.YRes = 480;
	 pParam->PanelStat.Depth = 8;
	 pParam->PanelStat.MonoColor = PNL_MONO_PANEL;
	 pParam->PanelStat.Type = PNL_DSTN;
	 break;
      case 15:
	 pParam->PanelStat.XRes = 800;
	 pParam->PanelStat.YRes = 600;
	 pParam->PanelStat.Depth = 16;
	 pParam->PanelStat.MonoColor = PNL_COLOR_PANEL;
	 pParam->PanelStat.Type = PNL_DSTN;
	 break;
      default:
	 break;
      }
   }
   /* if block end */
}

void
Dorado9211Init(Pnl_PanelStat * pstat)
{
   int mode;
   unsigned long orig_value, pm_value;

   gfx_delay_milliseconds(100);
   Dorado9211GpioInit();

   Dorado9211ToggleClock();
   Dorado9211ToggleClock();
   Dorado9211ToggleClock();
   Dorado9211ToggleClock();
   Dorado9211ToggleClock();

   gfx_delay_milliseconds(100);

   Dorado9211ToggleClock();
   Dorado9211ToggleClock();
   Dorado9211ToggleClock();
   Dorado9211ToggleClock();
   Dorado9211ToggleClock();

   Dorado9211WriteReg(CS92xx_LCD_PWR_MAN, 0x0);

   gfx_delay_milliseconds(100);
   gfx_delay_milliseconds(100);

   /* LOOP THROUGH THE AVAILABLE MODES TO FIND A MATCH */
   for (mode = 0; mode < NUM_92XX_MODES; mode++) {
      if ((FPModeParams[mode].xres == pstat->XRes) &&
	  (FPModeParams[mode].yres == pstat->YRes) &&
	  (FPModeParams[mode].bpp == pstat->Depth) &&
	  (FPModeParams[mode].panel_type == pstat->Type) &&
	  (FPModeParams[mode].color_type == pstat->MonoColor)) {

	 /* SET THE 92xx FOR THE SELECTED MODE */
	 CS92xx_MODE *pMode = &FPModeParams[mode];

	 Dorado9211WriteReg(CS92xx_LCD_PAN_TIMING1, pMode->panel_timing1);
	 Dorado9211WriteReg(CS92xx_LCD_PAN_TIMING2, pMode->panel_timing2);
	 Dorado9211WriteReg(CS92xx_LCD_DITH_FR_CNTRL,
			    pMode->rev_C_dither_frc);
	 Dorado9211WriteReg(CS92xx_BLUE_LSFR_SEED, pMode->blue_lsfr_seed);
	 Dorado9211WriteReg(CS92xx_RED_GREEN_LSFR_SEED,
			    pMode->red_green_lsfr_seed);
	 DoradoProgramFRMload();
	 Dorado9211WriteReg(CS92xx_LCD_MEM_CNTRL, pMode->memory_control);
	 Dorado9211WriteReg(CS92xx_LCD_PWR_MAN, pMode->power_management);
	 gfx_delay_milliseconds(100);
	 gfx_delay_milliseconds(100);
	 Dorado9211ClearCS();

	 /* This code is added to take care of Panel initialization.
	  * Irrespective of Xpressrom is enabling the panel or not. 
	  */
	 orig_value = READ_VID32(0X04);
	 WRITE_VID32(0x04, 0x00200141);
	 gfx_delay_milliseconds(21);
	 pm_value = gfx_ind(0x9030);

	 pm_value |= 0x400;
	 gfx_outd(0x9030, pm_value);
	 gfx_delay_milliseconds(4);
	 orig_value &= 0xfff1ffff;
	 WRITE_VID32(0X4, orig_value);
	 return;
      }					/*end if() */
   }					/*end for() */

}

void
Dorado9211SetCS(void)
{
   unsigned long value;

   value = gfx_ind(DRD_CSP9211IN);
   gfx_outd(DRD_CSP9211OUT, value | DRD_CS9211);
}

void
Dorado9211ClearCS(void)
{
   unsigned long value;

   value = gfx_ind(DRD_CSP9211IN);
   gfx_outd(DRD_CSP9211OUT, value & (~DRD_CS9211));
}

void
Dorado9211SetDataOut(void)
{
   unsigned long value;

   value = gfx_ind(DRD_DATAOUTP9211IN);
   gfx_outd(DRD_DATAOUTP9211OUT, value | DRD_DATAIN9211);
}

void
Dorado9211ClearDataOut(void)
{
   unsigned long value;

   value = gfx_ind(DRD_DATAOUTP9211IN);
   gfx_outd(DRD_DATAOUTP9211OUT, value & (~DRD_DATAIN9211));
}

unsigned char
Dorado9211ReadDataIn(void)
{
   unsigned char readdata = 0;
   unsigned long value;

   /* why to read 4 times ??? */
   value = gfx_ind(DRD_DATAINP9211IN);
   value = gfx_ind(DRD_DATAINP9211IN);
   value = gfx_ind(DRD_DATAINP9211IN);
   value = gfx_ind(DRD_DATAINP9211IN);
   if (value & DRD_DATAOUT9211)
      readdata = 1;
   return (readdata);
}

void
Dorado9211ToggleClock(void)
{
   Dorado9211SetClock();
   Dorado9211ClearClock();
}

void
Dorado9211SetClock(void)
{
   unsigned long value;

   value = gfx_ind(DRD_CLOCKP9211IN);
   gfx_outd(DRD_CLOCKP9211OUT, value | DRD_CLOCK9211);
}

void
Dorado9211ClearClock(void)
{
   unsigned long value;

   value = gfx_ind(DRD_CLOCKP9211IN);
   gfx_outd(DRD_CLOCKP9211OUT, value & (~DRD_CLOCK9211));
}

void
Dorado9211GpioInit(void)
{
   unsigned long value;

   /* set output enable on gpio 7, 9, 11 */
   gfx_outd((DRD_GEODE_GPIO_BASE + DRD_GEODE_GPPIN_SEL), DRD_CLOCK9211CFG);
   gfx_outd((DRD_GEODE_GPIO_BASE + DRD_GEODE_GPPIN_CFG), 3);
   /* set output enable on gpio 7, 9, 11 */
   gfx_outd((DRD_GEODE_GPIO_BASE + DRD_GEODE_GPPIN_SEL), DRD_CS9211CFG);
   gfx_outd((DRD_GEODE_GPIO_BASE + DRD_GEODE_GPPIN_CFG), 3);
   /* set output enable on gpio 7, 9, 18 */
   gfx_outd((DRD_GEODE_GPIO_BASE + DRD_GEODE_GPPIN_SEL), DRD_DATAIN9211CFG);
   gfx_outd((DRD_GEODE_GPIO_BASE + DRD_GEODE_GPPIN_CFG), 3);
   /* disable on gpio 11 - This is the output from the 9211 */
   gfx_outd((DRD_GEODE_GPIO_BASE + DRD_GEODE_GPPIN_SEL), DRD_DATAOUT9211CFG);
   gfx_outd((DRD_GEODE_GPIO_BASE + DRD_GEODE_GPPIN_CFG), 0);
   /* Set all PINS low */
   value = gfx_ind(DRD_GEODE_GPIO_BASE + DRD_GEODE_GPDI0);
   value &= ~(DRD_CS9211 | DRD_CLOCK9211 | DRD_DATAIN9211);
   gfx_outd((DRD_GEODE_GPIO_BASE + DRD_GEODE_GPDO0), value);
}

unsigned long
Dorado9211ReadReg(unsigned short index)
{

   unsigned char i, readbit;
   unsigned long data;

   Dorado9211ClearDataOut();

   Dorado9211SetCS();
   Dorado9211ToggleClock();

   Dorado9211SetDataOut();
   Dorado9211ToggleClock();

   for (i = 0; i < 12; i++) {
      if (index & 0x1) {
	 Dorado9211SetDataOut();
      } else {
	 Dorado9211ClearDataOut();
      }
      Dorado9211ToggleClock();
      index >>= 1;
   }

   Dorado9211ClearDataOut();
   Dorado9211ToggleClock();

   /* Idle clock, 7 clocks, no data set */

   Dorado9211ToggleClock();
   Dorado9211ToggleClock();
   Dorado9211ToggleClock();
   Dorado9211ToggleClock();
   Dorado9211ToggleClock();
   Dorado9211ToggleClock();
   Dorado9211ToggleClock();

   data = 0;
   for (i = 0; i < 32; i++) {
      Dorado9211ToggleClock();
      readbit = Dorado9211ReadDataIn();
      data |= (((unsigned long)readbit) << i);
   }

   Dorado9211ClearCS();
   Dorado9211ToggleClock();
   return (data);

}

void
Dorado9211WriteReg(unsigned short index, unsigned long data)
{

   unsigned char i;

   Dorado9211ClearDataOut();
   Dorado9211SetDataOut();
   Dorado9211SetCS();
   Dorado9211ToggleClock();
   Dorado9211SetDataOut();
   Dorado9211ToggleClock();

   for (i = 0; i < 12; i++) {
      if (index & 0x1) {
	 Dorado9211SetDataOut();
      } else {
	 Dorado9211ClearDataOut();
      }
      Dorado9211ToggleClock();
      index >>= 1;
   }

   Dorado9211SetDataOut();
   Dorado9211ToggleClock();

   for (i = 0; i < 32; i++) {
      if (data & 0x1) {
	 Dorado9211SetDataOut();
      } else {
	 Dorado9211ClearDataOut();
      }
      Dorado9211ToggleClock();
      data >>= 1;
   }

   Dorado9211ClearCS();

   Dorado9211ToggleClock();
   Dorado9211ToggleClock();
   Dorado9211ToggleClock();
   Dorado9211ToggleClock();
}

void
DoradoProgramFRMload(void)
{
   unsigned long DoradoFRMtable[] = {

      0x00000000,
      0x00000000,
      0x01000100,
      0x01000100,
      0x01010101,
      0x01010101,
      0x02081041,
      0x02081041,
      0x10111111,
      0x11111101,
      0x49249241,
      0x12412492,
      0x92244891,
      0x92244891,
      0x22252525,
      0x22252525,
      0x528294a5,
      0x2528494a,
      0x294a5295,
      0x294a5295,
      0x54a54a95,
      0x2952a52a,
      0x2a552a55,
      0x2a552a55,
      0x554aa955,
      0x2a9552aa,
      0x2aaa5555,
      0x2aaa5555,
      0x55555555,
      0x2aaaaaaa,
      0x55555555,
      0x55555555,
      0xaaaaaaab,
      0x55555555,
      0x5555aaab,
      0x5555aaab,
      0xaab556ab,
      0x556aad55,
      0x55ab55ab,
      0x55ab55ab,
      0xab5ab56b,
      0x56ad5ad5,
      0x56b5ad6b,
      0x56b5ad6b,
      0xad6d6b5b,
      0x5ad6b6b6,
      0x5b5b5b5b,
      0x5b5b5b5b,
      0x5F6db6db,
      0x5F6db6db,
      0xF776F776,
      0xF776F776,
      0xFBDEFBDE,
      0xFBDEFBDE,
      0x7eFFBFF7,
      0x7eFFBFF7,
      0xFF7FF7F7,
      0xFF7FF7F7,
      0xFF7FFF7F,
      0xFF7FFF7F,
      0xFFF7FFFF,
      0xFFF7FFFF,
      0xFFFFFFFF,
      0xFFFFFFFF,
   };

   unsigned char i;
   unsigned short index;
   unsigned long data;

   Dorado9211WriteReg(CS92xx_FRM_MEMORY_INDEX, 0);
   index = CS92xx_FRM_MEMORY_DATA;
   for (i = 0; i < 64; i += 2) {
      data = DoradoFRMtable[i];
      Dorado9211WriteReg(index, data);
      data = DoradoFRMtable[i + 1];
      Dorado9211WriteReg(index, data);
   }

/*
 * The first FRM location (64 bits) does not program correctly.
 * This location always reads back with the last value programmed.
 * ie. If 32 64-bit values are programmed, location 0 reads back as the 32nd
 * If 30 locations are programmed, location 0 reads back as the 30th, etc.
 * Fix this by re-writing location 0 after programming all 64 in the writeFRM 
 * loop in RevCFrmload() in CS9211.
 */

   Dorado9211WriteReg(CS92xx_FRM_MEMORY_INDEX, 0);
   Dorado9211WriteReg(CS92xx_FRM_MEMORY_DATA, 0);
   Dorado9211WriteReg(CS92xx_FRM_MEMORY_DATA, 0);

}

/******************************************************************************
 * void Dorado_Enable_Power((void);
 * Enables the power of the CX9211 on Dorado board.
 ******************************************************************************
 */

void
Dorado_Power_Up(void)
{
   Dorado9211WriteReg(CS92xx_LCD_PWR_MAN, 0x01000000);
   return;

}					/* disable_Centaurus_Power */

/******************************************************************************
 * void Dorado_Disable_Power((void);
 * Disables the power of the CX9211 on Dorado board.
 *****************************************************************************
 */

void
Dorado_Power_Down(void)
{
   Dorado9211WriteReg(CS92xx_LCD_PWR_MAN, 0x0);
   return;

}					/* disable_Centaurus_Power */

void
Dorado_Save_Panel_State(void)
{

   /* set 9211 registers using the desired panel settings     */
   cs9211_regs.panel_timing1 = Dorado9211ReadReg(CS92xx_LCD_PAN_TIMING1);
   cs9211_regs.panel_timing2 = Dorado9211ReadReg(CS92xx_LCD_PAN_TIMING2);

   cs9211_regs.dither_frc_ctrl = Dorado9211ReadReg(CS92xx_LCD_DITH_FR_CNTRL);
   cs9211_regs.blue_lsfr_seed = Dorado9211ReadReg(CS92xx_BLUE_LSFR_SEED);
   cs9211_regs.red_green_lsfr_seed =
	 Dorado9211ReadReg(CS92xx_RED_GREEN_LSFR_SEED);

   /* CentaurusProgramFRMload(); */
   cs9211_regs.memory_control = Dorado9211ReadReg(CS92xx_LCD_MEM_CNTRL);

   /* Set the power register last. This will turn the panel on at the 9211. */
   cs9211_regs.power_management = Dorado9211ReadReg(CS92xx_LCD_PWR_MAN);
   cs9211_regs.panel_state = cs9211_regs.power_management;
}

void
Dorado_Restore_Panel_State(void)
{
   unsigned long off_data = 0;

   /* Before restoring the 9211 registers, power off the 9211. */

   Dorado9211WriteReg(CS92xx_LCD_PWR_MAN, off_data);

   /* set 9211 registers using the desired panel settings    */
   Dorado9211WriteReg(CS92xx_LCD_PAN_TIMING1, cs9211_regs.panel_timing1);
   Dorado9211WriteReg(CS92xx_LCD_PAN_TIMING2, cs9211_regs.panel_timing2);
   /* load the LSFR seeds */
   Dorado9211WriteReg(CS92xx_LCD_DITH_FR_CNTRL, cs9211_regs.dither_frc_ctrl);
   Dorado9211WriteReg(CS92xx_BLUE_LSFR_SEED, cs9211_regs.blue_lsfr_seed);
   Dorado9211WriteReg(CS92xx_RED_GREEN_LSFR_SEED,
		      cs9211_regs.red_green_lsfr_seed);

   Dorado9211WriteReg(CS92xx_LCD_MEM_CNTRL, cs9211_regs.memory_control);
   /* Set the power register last. This will turn the panel on at the 9211. */
   Dorado9211WriteReg(CS92xx_LCD_PWR_MAN, cs9211_regs.power_management);
}

/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/nsc/panel/pnl_bios.c,v 1.3 2003/01/14 09:34:36 alanh Exp $ */
/*
 * $Workfile: pnl_bios.c $
 * $Revision: 1.1.1.1 $
 *
 * File Contents: This file panel functions which query for the BIOS for current FP 
 *                Paramters.
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

#include "panel.h"

#if defined(_WIN32)			/* windows */
extern unsigned long gfx_cpu_version;
extern void gfx_outw(unsigned short port, unsigned short data);
extern unsigned short gfx_inw(unsigned short port);
#endif

#define SOFTVGA_DISPLAY_ENABLE   0x50
#define SOFTVGA_FPRESOLUTION     0x52
#define SOFTVGA_FPCLOCKFREQUENCY 0x54

/* SOFTVG VIRTUAL REGISTER DEFINITIONS */

#define VR_INDEX                0xAC1C
#define VR_DATA                 0xAC1E
#define VR_UNLOCK               0xFC53
#define VRC_VG                  0x0002	/* SoftVG Virtual Register Class    */
#define VG_MEM_SIZE             0x0000	/* MemSize Virtual Register */
#define FP_DETECT_MASK          0x8000

#define VG_FP_TYPE      0x0002		/* Flat Panel Info Virtual Register */

#define FP_DEV_MASK     0x0003		/* Flat Panel type */
#define FP_TYPE_SSTN	0x0000		/* SSTN panel type value */
#define FP_TYPE_DSTN	0x0001		/* DSTN panel type value */
#define FP_TYPE_TFT		0x0002	/* TFT panel type value */
#define FP_TYPE_LVDS	0x0003		/* LVDS panel type value */

#define FP_RESOLUTION_MASK      0x0038
#define FP_RES_6X4		0x0000	/* 640x480 resolution value */
#define FP_RES_8X6		0x0008	/* 800x600 resolution value */
#define FP_RES_10X7		0x0010	/* 1024x768 resolution value */
#define FP_RES_12X10	0x0018		/* 1280x1024 resolution value */
#define FP_RES_16X12	0x0020		/* 1600x1200 resolution value */

#define FP_WIDTH_MASK   0x01C0
#define FP_WIDTH_8		0x0000	/* 8 bit data bus width */
#define FP_WIDTH_9		0x0040	/* 9 bit data bus width */
#define FP_WIDTH_12		0x0080	/* 12 bit data bus width */
#define FP_WIDTH_18		0x00C0	/* 18 bit data bus width */
#define FP_WIDTH_24		0x0100	/* 24 bit data bus width */
#define FP_WIDTH_16		0x0140	/* 16 bit data bus width - 16 bit Mono DSTN only */

#define FP_COLOR_MASK   0x0200
#define FP_COLOR_COLOR	0x0000		/* Color panel */
#define FP_COLOR_MONO	0x0200		/* Mono Panel */

#define FP_PPC_MASK     0x0400
#define FP_PPC_1PPC		0x0000	/* One pixel per clock */
#define FP_PPC_2PPC		0x0400	/* Two pixels per clock */

#define FP_HPOL_MASK    0x0800
#define	FP_H_POL_LGH	0x0000		/* HSync at panel, normally low, active high */
#define FP_H_POL_HGL	0x0800		/* HSync at panel, normally high, active low */

#define FP_VPOL_MASK    0x1000
#define FP_V_POL_LGH	0x0000		/* VSync at panel, normally low, active high */
#define FP_V_POL_HGL	0x1000		/* VSync at panel, normally high, active low */

#define FP_REF_MASK     0xD000
#define FP_REF_60		0x0000	/* 60Hz refresh rate */
#define FP_REF_65		0x2000	/* 65Hz refresh rate */
#define FP_REF_70		0x4000	/* 70Hz refresh rate */
#define FP_REF_72		0x6000	/* 72Hz refresh rate */
#define FP_REF_75		0x8000	/* 75Hz refresh rate */
#define FP_REF_85		0xA000	/* 85Hz refresh rate */

/*-----------------------------------------------------------------
 * Pnl_IsPanelEnabledInBIOS
 *
 * Description:	This function specifies whether the panel is enabled
 *				by the BIOS or not.
 *  parameters: none.
 *      return: 1 - Enabled, 0 - Disabled
 *-----------------------------------------------------------------*/
int
Pnl_IsPanelEnabledInBIOS(void)
{
   unsigned char ret = 0;

   if ((gfx_cpu_version & 0xFF) == GFX_CPU_REDCLOUD) {
      unsigned short data;

      gfx_outw(VR_INDEX, VR_UNLOCK);
      gfx_outw(VR_INDEX, (VRC_VG << 8) | VG_MEM_SIZE);
      data = gfx_inw(VR_DATA);
      if (data & FP_DETECT_MASK)
	 ret = 1;
   } else {
      unsigned short crtcindex, crtcdata;

      crtcindex = (gfx_inb(0x3CC) & 0x01) ? 0x3D4 : 0x3B4;
      crtcdata = crtcindex + 1;

      /* CHECK DisplayEnable Reg in SoftVGA */

      gfx_outb(crtcindex, (unsigned char)SOFTVGA_DISPLAY_ENABLE);
      ret = gfx_inb(crtcdata);
   }

   return (ret & 0x1);
}

/*-----------------------------------------------------------------
 * Pnl_GetPanelInfoFromBIOS
 *
 * Description:	This function queries the panel information from 
 *              the BIOS.
 *  parameters: 
 *        xres: width of the panel configured
 *        yres: height of the panel configured
 *         bpp: depth of the panel configured
 *          hz: vertical frequency of the panel configured
 *      return: none
 *-----------------------------------------------------------------*/
void
Pnl_GetPanelInfoFromBIOS(int *xres, int *yres, int *bpp, int *hz)
{
   unsigned short crtcindex, crtcdata;
   unsigned short ret;

   if ((gfx_cpu_version & 0xFF) == GFX_CPU_REDCLOUD) {
      gfx_outw(VR_INDEX, VR_UNLOCK);
      gfx_outw(VR_INDEX, (VRC_VG << 8) | VG_FP_TYPE);
      ret = gfx_inw(VR_DATA);
      switch (ret & FP_RESOLUTION_MASK) {
      case FP_RES_6X4:
	 *xres = 640;
	 *yres = 480;
	 break;
      case FP_RES_8X6:
	 *xres = 800;
	 *yres = 600;
	 break;
      case FP_RES_10X7:
	 *xres = 1024;
	 *yres = 768;
	 break;
      case FP_RES_12X10:
	 *xres = 1280;
	 *yres = 1024;
	 break;
      case FP_RES_16X12:
	 *xres = 1600;
	 *yres = 1200;
	 break;
      }

      switch (ret & FP_WIDTH_MASK) {
      case FP_WIDTH_8:
	 *bpp = 8;
	 break;
      case FP_WIDTH_9:
	 *bpp = 9;
	 break;
      case FP_WIDTH_12:
	 *bpp = 12;
	 break;
      case FP_WIDTH_18:
	 *bpp = 18;
	 break;
      case FP_WIDTH_24:
	 *bpp = 24;
	 break;
      case FP_WIDTH_16:
	 *bpp = 16;
	 break;
      }

      switch (ret & FP_REF_MASK) {
      case FP_REF_60:
	 *hz = 60;
	 break;
      case FP_REF_65:
	 *hz = 65;
	 break;
      case FP_REF_70:
	 *hz = 70;
	 break;
      case FP_REF_72:
	 *hz = 72;
	 break;
      case FP_REF_75:
	 *hz = 75;
	 break;
      case FP_REF_85:
	 *hz = 85;
	 break;
      }

   } else {
      crtcindex = (gfx_inb(0x3CC) & 0x01) ? 0x3D4 : 0x3B4;
      crtcdata = crtcindex + 1;

      /* CHECK FPResolution Reg in SoftVGA */

      gfx_outb(crtcindex, (unsigned char)SOFTVGA_FPRESOLUTION);
      ret = gfx_inb(crtcdata);

      switch (ret & 0x3) {
      case 0:
	 *xres = 640;
	 *yres = 480;
	 break;
      case 1:
	 *xres = 800;
	 *yres = 600;
	 break;
      case 2:
	 *xres = 1024;
	 *yres = 768;
	 break;
      }

      switch ((ret >> 4) & 0x3) {
      case 0:
	 *bpp = 12;
	 break;
      case 1:
	 *bpp = 18;
	 break;
      case 2:
	 *bpp = 16;
	 break;
      case 3:
	 *bpp = 8;
	 break;
      }

      /* CHECK FPClockFrequency Reg in SoftVGA */

      gfx_outb(crtcindex, (unsigned char)SOFTVGA_FPCLOCKFREQUENCY);
      *hz = gfx_inb(crtcdata);
   }
}

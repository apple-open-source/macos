/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/nsc/gfx/tv_fs450.h,v 1.1 2002/12/10 15:12:27 alanh Exp $ */
/*
 * $Workfile: tv_fs450.h $
 *
 * This file defines the common FS450 API.
 *
 * NSC_LIC_ALTERNATIVE_PREAMBLE
 *
 * Revision 1.0
 *
 * National Semiconductor Alternative GPL-BSD License
 *
 * National Semiconductor Corporation licenses this software 
 * ("Software"):
 *
 *      Durango
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
 * National Semiconductor Corporation Open Source License for Durango
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
 * National Semiconductor Corporation Gnu General Public License for Durango
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

#ifndef __FS450_H__
#define __FS450_H__

#ifdef __cplusplus
extern "C"
{
#endif

/* ==========================================================================*/
/*	Init and cleanup functions*/

   int FS450_init(void);
   void FS450_cleanup(void);
   /* call FS450_init at startup to probe for and initialize FS450. */
   /* returns 0 if successful. */

/* ==========================================================================*/
/*	TV output on or off*/

   int FS450_get_tv_enable(unsigned int *p_on);
   int FS450_set_tv_enable(unsigned int on);
   /* on is 1 for TV on, 0 for off */

/* ==========================================================================*/
/*	TV standard*/

   int FS450_get_tv_standard(unsigned long *p_standard);
   int FS450_get_available_tv_standards(unsigned long *p_standards);
   int FS450_set_tv_standard(unsigned long standard);
   /* standard is one of the FS450_TV_STANDARD constants */
   /* standards is a bitmask of zero or more FS450_TV_STANDARD constants */

/* FS450 TV Standard flags*/
#define FS450_TV_STANDARD_NTSC_M 0x0001
#define FS450_TV_STANDARD_NTSC_M_J 0x0002
#define FS450_TV_STANDARD_PAL_B 0x0004
#define FS450_TV_STANDARD_PAL_D 0x0008
#define FS450_TV_STANDARD_PAL_H 0x0010
#define FS450_TV_STANDARD_PAL_I 0x0020
#define FS450_TV_STANDARD_PAL_M 0x0040
#define FS450_TV_STANDARD_PAL_N 0x0080
#define FS450_TV_STANDARD_PAL_G 0x0100

/* ==========================================================================*/
/*	VGA mode assumed by FS450*/

   int FS450_get_vga_mode(unsigned long *p_vga_mode);
   int FS450_get_available_vga_modes(unsigned long *p_vga_modes);
   int FS450_set_vga_mode(unsigned long vga_mode);
   /* vga_mode is one of the FS450_VGA_MODE constants */
   /* vga_modes is a bitmask of zero or more FS450_VGA_MODE constants */

/* FS450 VGA Mode flags*/
#define FS450_VGA_MODE_UNKNOWN 0
#define FS450_VGA_MODE_640X480 0x0001
#define FS450_VGA_MODE_720X487 0x0002
#define FS450_VGA_MODE_720X576 0x0004
#define FS450_VGA_MODE_800X600 0x0008
#define FS450_VGA_MODE_1024X768 0x0010

/* ==========================================================================*/
/*	TVout mode*/

   int FS450_get_tvout_mode(unsigned long *p_tvout_mode);
   int FS450_set_tvout_mode(unsigned long tvout_mode);
   /* tvout_mode is a bitmask of FS450_TVOUT_MODE constants */

/* FS450 TVout mode flags*/
#define FS450_TVOUT_MODE_CVBS 0x0001
#define FS450_TVOUT_MODE_YC 0x0002
#define FS450_TVOUT_MODE_RGB 0x0004
#define FS450_TVOUT_MODE_CVBS_YC (FS450_TVOUT_MODE_CVBS | FS450_TVOUT_MODE_YC)

/* ==========================================================================*/
/*	Flicker control*/

   int FS450_get_sharpness(int *p_sharpness);
   int FS450_set_sharpness(int sharpness);
   /* sharpness is a percentage in tenths of a percent, 0 to 1000 */

   int FS450_get_flicker_filter(int *p_flicker);
   int FS450_set_flicker_filter(int flicker);
   /* flicker_filter is a percentage in tenths of a percent, 0 to 1000 */

/* ==========================================================================*/
/*	Size and Position*/

   int FS450_get_overscan(int *p_x, int *p_y);
   int FS450_set_overscan(int x, int y);
   int FS450_get_position(int *p_x, int *p_y);
   int FS450_set_position(int x, int y);
   /* x and y are horizontal and vertical adjustments, -1000 to +1000 */

/* ==========================================================================*/
/*	Visual adjustments*/

   int FS450_get_color(int *p_color);
   int FS450_set_color(int color);
   /* color is a percentage, 0 to 100 */

   int FS450_get_brightness(int *p_brightness);
   int FS450_set_brightness(int brightness);
   /* brightness is a percentage, 0 to 100 */

   int FS450_get_contrast(int *p_contrast);
   int FS450_set_contrast(int constrast);
   /* contrast is a percentage, 0 to 100 */

/* ==========================================================================*/
/*	Luma and Chroma filter*/

   int FS450_get_yc_filter(unsigned int *p_yc_filter);
   int FS450_set_yc_filter(unsigned int yc_filter);
   /* yc_filter is a bitmask of FS450_LUMA_FILTER and/or FS450_CHROMA_FILTER */

/* FS450 Luma and Chroma Filters*/
#define FS450_LUMA_FILTER 0x0001
#define FS450_CHROMA_FILTER 0x0002

/* ==========================================================================*/
/*	Macrovision*/

   int FS450_get_aps_trigger_bits(unsigned int *p_trigger_bits);
   int FS450_set_aps_trigger_bits(unsigned int trigger_bits);
   /* trigger_bits is one of the FS450_APS_TRIGGER constants */

/* APS Trigger Bits*/
#define FS450_APS_TRIGGER_OFF 0
#define FS450_APS_TRIGGER_AGC_ONLY 1
#define FS450_APS_TRIGGER_AGC_2_LINE 2
#define FS450_APS_TRIGGER_AGC_4_LINE 3

/* ==========================================================================*/
/*	direct access to Houston and platform registers (debug builds only)*/
/*	The two functions FS450_ReadRegister and FS450_WriteRegister allow access*/
/*	to device registers.  These functions are intended for debugging purposes*/
/*	only and should not be included in a shipping product.*/

#ifdef FS450_DIRECTREG

#define SOURCE_HOUSTON 0
#define SOURCE_GCC 1

   typedef struct _S_REG_INFO
   {
      int source;
      unsigned int size;
      unsigned long offset;
      unsigned long value;
   }
   S_REG_INFO;

   int FS450_ReadRegister(S_REG_INFO * p_reg);
   int FS450_WriteRegister(S_REG_INFO * p_reg);

#endif

/* ==========================================================================*/
/* Error Codes*/

#define ERR_INVALID_PARAMETER			0x1000
#define ERR_NOT_SUPPORTED				0x1001
#define ERR_CANNOT_CHANGE_WHILE_TV_ON	0x1002

#define ERR_DRIVER_NOT_FOUND			0x1100
#define ERR_DRIVER_ERROR				0x1101
#define ERR_DEVICE_NOT_FOUND			0x1120

#define ERR_I2C_MISSING_DEVICE			0x1200
#define ERR_I2C_WRITE_FAILED			0x1201
#define ERR_I2C_READ_FAILED				0x1202

#ifdef __cplusplus
}
#endif

#endif

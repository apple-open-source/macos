/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/nsc/panel/gx2_9211.h,v 1.3 2003/01/14 09:34:35 alanh Exp $ */
/*
 * $Workfile: gx2_9211.h $
 *
 * This header file defines the pneumonics used when calling Durango routines. 
 * This file is automatically included by gfx_rtns.h
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
 *      Panel Library
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

/* -----------------------------------------------------------
 *  GX2 FLAT PANEL CONTROLLER REGISTER DEFINITIONS           
 *-----------------------------------------------------------
 */

#define	GX2_VP_MSR_PAD_SELECT		0x2011
#define	GX2_VP_PAD_SELECT_MASK		0x3FFFFFFF
#define	GX2_VP_PAD_SELECT_TFT		0x1FFFFFFF
#define	GX2_VP_PAD_SELECT_DSTN		0x00000000

/* This is useful for generating addresses incrementally 
 * (ie, vidtest register display code).
 */

#define GX2_FP_LCD_OFFSET       0x00000400
#define CS9211_REDCLOUD         0x0400	/* Moved 9211 Rev C3 up to next major no. */

#define GX2_FP_PAN_TIMING1      0x0400	/* FP timings 1 */
#define GX2_FP_PAN_TIMING2      0x0408	/* FP timings 2 */
#define GX2_FP_PWR_MAN          0x0410	/* FP power management */
#define GX2_FP_DITH_FR_CNTRL    0x0418	/* FP dither and frame rate */
#define GX2_FP_BLFSR            0x0420	/* Blue LFSR seed */
#define GX2_FP_RLFSR            0x0428	/* Red and Green LFSR seed */
#define GX2_FP_FMI              0x0430	/* FRM Memory Index */
#define GX2_FP_FMD              0x0438	/* FRM Memory Data */
#define GX2_FP_DCA              0x0448	/* Dither ram control and address */
#define GX2_FP_DMD              0x0450	/* Dither memory data */
#define GX2_FP_PAN_CRC_SIG      0x0458	/* FP CRC signature */
#define GX2_FP_FBB              0x0460	/* Frame Buffer Base Address */

/* GX2_FP_PAN_TIMING2 bits */

#define GX2_FP_TFT_PASS_THRU		0x40000000	/* TFT pass through enable */
#define GX2_FP_PT2_PIX_OUT_MASK		0xFFF8FFFF	/* panel output bit formats */
#define GX2_FP_PT2_PIX_OUT_TFT		0x00000000	/* 8 BIT DSTN or TFT panel */
#define GX2_FP_PT2_COLOR_MONO		0x00080000	/* color or monochrome */
#define GX2_FP_PT2_DSTN_TFT_MASK	0xFFCFFFFF	/* panel type bits */
#define GX2_FP_PT2_DSTN_TFT_TFT		0x00100000	/* TFT panel */
#define GX2_FP_PT2_PSH_CLK_CTL		0x08000000	/* shift clock retrace activity control */

/*  GX2_FP_PWR_MAN bits */

#define GX2_FP_PM_SHFCLK_INVERT		0x00002000	/* Invert shfclk to panel */
#define GX2_FP_PM_VSYNC_DELAY		0x0000C000	/* Vert Sync delay */
#define GX2_FP_PM_HSYNC_DELAY		0x00030000	/* Horiz Sync delay */
#define GX2_FP_PM_PWRDN_PHASE_BIT0	0x00040000	/* panel power down phase bit 0 */
#define GX2_FP_PM_PWRDN_PHASE_BIT1	0x00080000	/* panel power down phase bit 1 */
#define GX2_FP_PM_PWRDN_PHASE_BIT2	0x00100000	/* panel power down phase bit 2 */
#define GX2_FP_PM_PWRUP_PHASE_BIT0	0x00200000	/* panel power up phase bit 0 */
#define GX2_FP_PM_PWRUP_PHASE_BIT1	0x00400000	/* panel power up phase bit 1 */
#define GX2_FP_PM_PWRUP_PHASE_BIT2	0x00800000	/* panel power up phase bit 2 */
#define GX2_FP_PM_PWR_ON			0x01000000	/* panel power ON */
#define GX2_FP_PM_DIS_OFF_CTL		0x02000000	/* disable the panel back light */
#define GX2_FP_PM_EXT_PWR_SEQ		0x08000000	/* external power sequence */

/*  GX2_FP_PAN_CRC_SIG bits */

#define GX2_FP_PAN_CRC_SIGE         0x00000001	/* CRC Sig Enable */
#define GX2_FP_PAN_CRC_SFR          0x00000002	/* CRC Sig Free Run */

/* This define is used by the hardware CRC mechanism */
#define GX2_FP_CRC_PASS_THRU_MASK	0x00000070

#define GX2_READ 0
#define GX2_WRITE 1

void SetFPBaseAddr(unsigned long);
void Redcloud_9211init(Pnl_PanelStat *);
void protected_mode_access(unsigned long mode,
              unsigned long width,
              unsigned long addr, char *pdata);
void write_video_reg64_low(unsigned long offset, unsigned long value);
unsigned long read_video_reg64_low(unsigned long offset);
void Redcloud_fp_reg(int mode, unsigned long address, unsigned long *data);
void set_Redcloud_92xx_mode_params(int mode);
unsigned char set_Redcloud_92xx_mode(Pnl_PanelStat * pstat);

/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/nsc/gfx/vid_rdcl.c,v 1.3 2003/02/21 16:51:10 alanh Exp $ */
/*
 * $Workfile: vid_rdcl.c $
 *
 * This file contains routines to control the Redcloud display filter video overlay hardware.
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

/* REDCLOUD PLL TABLE  */

typedef struct RCDFPLL
{
   long frequency;			/* 16.16 fixed point frequency            */
   unsigned long post_div3;		/* MCP Frequency dividers and multipliers */
   unsigned long pre_mul2;
   unsigned long pre_div2;
   unsigned long pll_value;		/* MCP DotPLL Register Upper 32(0x0015)   */
}
RCDFPLLENTRY;

RCDFPLLENTRY RCDF_PLLtable48MHz[] = {
   {0x00192CCC, 0, 0, 0, 0x00000037},	/*  25.1750 */
   {0x001C526E, 1, 1, 0, 0x00000B1A},	/*  28.3220 */
   {0x001F8000, 1, 0, 0, 0x000002D2},	/*  31.5000 */
   {0x00240000, 1, 1, 0, 0x00000FE2},	/*  36.0000 */
   {0x00258000, 1, 0, 0, 0x0000057A},	/*  37.5000 */
   {0x00280000, 1, 0, 0, 0x0000030A},	/*  40.0000 */
   {0x002CE666, 0, 0, 0, 0x00000063},	/*  44.9000 */
   {0x00318000, 0, 0, 0, 0x0000054B},	/*  49.5000 */
   {0x00320000, 0, 0, 0, 0x0000026E},	/*  50.0000 */
   {0x00325999, 0, 1, 0, 0x00000037},	/*  50.3500 */
   {0x00360000, 1, 1, 0, 0x00000B0D},	/*  54.0000 */
   {0x00384000, 0, 0, 0, 0x00000577},	/*  56.2500 */
   {0x0038643F, 0, 0, 0, 0x000007F7},	/*  56.3916 */
   {0x0038A4DD, 0, 0, 0, 0x0000057B},	/*  56.6444 */
   {0x003B0000, 0, 1, 0, 0x00000707},	/*  59.0000 */
   {0x003F0000, 1, 1, 0, 0x00000B39},	/*  63.0000 */
   {0x00410000, 1, 1, 0, 0x00000B45},	/*  65.0000 */
   {0x00438000, 1, 1, 0, 0x00000FC1},	/*  67.5000 */
   {0x0046CCCC, 1, 0, 0, 0x00000561},	/*  70.8000 */
   {0x00480000, 1, 0, 0, 0x000007E1},	/*  72.0000 */
   {0x004B0000, 0, 0, 0, 0x00000052},	/*  75.0000 */
   {0x004EC000, 0, 0, 0, 0x00000056},	/*  78.7500 */
   {0x00500000, 1, 1, 0, 0x00000709},	/*  80.0000 */
   {0x0059CCCC, 0, 1, 0, 0x00000262},	/*  89.8000 */
   {0x005E8000, 0, 0, 0, 0x000002D2},	/*  94.5000 */
   {0x00630000, 0, 1, 0, 0x00000B4A},	/*  99.0000 */
   {0x00640000, 0, 1, 0, 0x00000036},	/* 100.0000 */
   {0x006C0000, 0, 0, 0, 0x000007E2},	/* 108.0000 */
   {0x00708000, 0, 0, 0, 0x000007F6},	/* 112.5000 */
   {0x00820000, 1, 1, 0, 0x00000FB0},	/* 130.0000 */
   {0x00870000, 1, 1, 0, 0x00000B50},	/* 135.0000 */
   {0x009D8000, 0, 0, 0, 0x00000055},	/* 157.5000 */
   {0x00A20000, 0, 0, 0, 0x000009C1},	/* 162.0000 */
   {0x00AF8000, 0, 0, 0, 0x000002C1},	/* 175.5000 */
   {0x00BD0000, 0, 0, 0, 0x000002D1},	/* 189.0000 */
   {0x00CA8000, 0, 0, 0, 0x00000551},	/* 202.5000 */
   {0x00E58000, 0, 0, 0, 0x0000057D},	/* 229.5000 */
};

RCDFPLLENTRY RCDF_PLLtable14MHz[] = {
   {0x00192CCC, 0, 0, 0, 0x00000037},	/*  25.1750 */
   {0x001C526E, 0, 0, 0, 0x00000B7B},	/*  28.3220 */
   {0x001F8000, 0, 0, 0, 0x000004D3},	/*  31.5000 */
   {0x00240000, 0, 0, 0, 0x00000BE3},	/*  36.0000 */
   {0x00258000, 0, 0, 0, 0x0000074F},	/*  37.5000 */
   {0x00280000, 0, 0, 0, 0x0000050B},	/*  40.0000 */
   {0x002CE666, 0, 0, 0, 0x00000063},	/*  44.9000 */
   {0x00318000, 0, 0, 0, 0x0000054B},	/*  49.5000 */
   {0x00320000, 0, 0, 0, 0x0000026E},	/*  50.0000 */
   {0x00325999, 0, 0, 0, 0x000007C3},	/*  50.3500 */
   {0x00360000, 0, 0, 0, 0x000007E3},	/*  54.0000 */
   {0x00384000, 0, 0, 0, 0x00000577},	/*  56.2500 */
   {0x0038643F, 0, 0, 0, 0x000002FB},	/*  56.3916 */
   {0x0038A4DD, 0, 0, 0, 0x0000057B},	/*  56.6444 */
   {0x003B0000, 0, 0, 0, 0x0000058B},	/*  59.0000 */
   {0x003F0000, 0, 0, 0, 0x0000095E},	/*  63.0000 */
   {0x00410000, 0, 0, 0, 0x0000096A},	/*  65.0000 */
   {0x00438000, 0, 0, 0, 0x00000BC2},	/*  67.5000 */
   {0x0046CCCC, 0, 0, 0, 0x0000098A},	/*  70.8000 */
   {0x00480000, 0, 0, 0, 0x00000BE2},	/*  72.0000 */
   {0x004B0000, 0, 0, 0, 0x00000052},	/*  75.0000 */
   {0x004EC000, 0, 0, 0, 0x00000056},	/*  78.7500 */
   {0x00500000, 0, 0, 0, 0x0000050A},	/*  80.0000 */
   {0x0059CCCC, 0, 0, 0, 0x0000078E},	/*  89.8000 */
   {0x005E8000, 0, 0, 0, 0x000002D2},	/*  94.5000 */
   {0x00630000, 0, 0, 0, 0x000011F6},	/*  99.0000 */
   {0x00640000, 0, 0, 0, 0x0000054E},	/* 100.0000 */
   {0x006C0000, 0, 0, 0, 0x000007E2},	/* 108.0000 */
   {0x00708000, 0, 0, 0, 0x000002FA},	/* 112.5000 */
   {0x00820000, 0, 0, 0, 0x00000BB1},	/* 130.0000 */
   {0x00870000, 0, 0, 0, 0x00000975},	/* 135.0000 */
   {0x009D8000, 0, 0, 0, 0x00000055},	/* 157.5000 */
   {0x00A20000, 0, 0, 0, 0x000009C1},	/* 162.0000 */
   {0x00AF8000, 0, 0, 0, 0x000002C1},	/* 175.5000 */
   {0x00BD0000, 0, 0, 0, 0x00000539},	/* 189.0000 */
   {0x00CA8000, 0, 0, 0, 0x00000551},	/* 202.5000 */
   {0x00E58000, 0, 0, 0, 0x0000057D},	/* 229.5000 */
};

#define NUM_RCDF_FREQUENCIES sizeof(RCDF_PLLtable14MHz)/sizeof(RCDFPLLENTRY)

int redcloud_set_video_enable(int enable);
int redcloud_set_video_format(unsigned long format);
int redcloud_set_video_size(unsigned short width, unsigned short height);
int redcloud_set_video_yuv_pitch(unsigned long ypitch, unsigned long uvpitch);
int redcloud_set_video_offset(unsigned long offset);
int redcloud_set_video_yuv_offsets(unsigned long yoffset,
				   unsigned long uoffset,
				   unsigned long voffset);
int redcloud_set_video_window(short x, short y, unsigned short w,
			      unsigned short h);
int redcloud_set_video_left_crop(unsigned short x);
int redcloud_set_video_upscale(unsigned short srcw, unsigned short srch,
			       unsigned short dstw, unsigned short dsth);
int redcloud_set_video_scale(unsigned short srcw, unsigned short srch,
			     unsigned short dstw, unsigned short dsth);
int redcloud_set_video_vertical_downscale(unsigned short srch,
					  unsigned short dsth);
void redcloud_set_video_vertical_downscale_enable(int enable);
int redcloud_set_video_downscale_config(unsigned short type,
					unsigned short m);
int redcloud_set_video_color_key(unsigned long key, unsigned long mask,
				 int bluescreen);
int redcloud_set_video_filter(int xfilter, int yfilter);
int redcloud_set_video_palette(unsigned long *palette);
int redcloud_set_video_palette_entry(unsigned long index,
				     unsigned long color);
int redcloud_set_video_downscale_coefficients(unsigned short coef1,
					      unsigned short coef2,
					      unsigned short coef3,
					      unsigned short coef4);
int redcloud_set_video_downscale_enable(int enable);
int redcloud_set_video_source(VideoSourceType source);
int redcloud_set_vbi_source(VbiSourceType source);
int redcloud_set_vbi_lines(unsigned long even, unsigned long odd);
int redcloud_set_vbi_total(unsigned long even, unsigned long odd);
int redcloud_set_video_interlaced(int enable);
int redcloud_set_color_space_YUV(int enable);
int redcloud_set_vertical_scaler_offset(char offset);
int redcloud_set_top_line_in_odd(int enable);
int redcloud_set_genlock_delay(unsigned long delay);
int redcloud_set_genlock_enable(int flags);
int redcloud_set_video_cursor(unsigned long key, unsigned long mask,
			      unsigned short select_color2,
			      unsigned long color1, unsigned long color2);
int redcloud_set_video_cursor_enable(int enable);
int redcloud_set_video_request(short x, short y);

int redcloud_select_alpha_region(int region);
int redcloud_set_alpha_enable(int enable);
int redcloud_set_alpha_window(short x, short y,
			      unsigned short width, unsigned short height);
int redcloud_set_alpha_value(unsigned char alpha, char delta);
int redcloud_set_alpha_priority(int priority);
int redcloud_set_alpha_color(unsigned long color);
int redcloud_set_alpha_color_enable(int enable);
int redcloud_set_no_ck_outside_alpha(int enable);
int redcloud_disable_softvga(void);
int redcloud_enable_softvga(void);
int redcloud_set_macrovision_enable(int enable);
void redcloud_reset_video(void);
int redcloud_set_display_control(int sync_polarities);
void redcloud_set_clock_frequency(unsigned long frequency);
int redcloud_set_crt_enable(int enable);

/* READ ROUTINES IN GFX_VID.C */

int redcloud_get_video_enable(void);
int redcloud_get_video_format(void);
unsigned long redcloud_get_video_src_size(void);
unsigned long redcloud_get_video_line_size(void);
unsigned long redcloud_get_video_xclip(void);
unsigned long redcloud_get_video_offset(void);
void redcloud_get_video_yuv_offsets(unsigned long *yoffset,
				    unsigned long *uoffset,
				    unsigned long *voffset);
void redcloud_get_video_yuv_pitch(unsigned long *ypitch,
				  unsigned long *uvpitch);
unsigned long redcloud_get_video_upscale(void);
unsigned long redcloud_get_video_scale(void);
unsigned long redcloud_get_video_downscale_delta(void);
int redcloud_get_video_vertical_downscale_enable(void);
int redcloud_get_video_downscale_config(unsigned short *type,
					unsigned short *m);
void redcloud_get_video_downscale_coefficients(unsigned short *coef1,
					       unsigned short *coef2,
					       unsigned short *coef3,
					       unsigned short *coef4);
void redcloud_get_video_downscale_enable(int *enable);
unsigned long redcloud_get_video_dst_size(void);
unsigned long redcloud_get_video_position(void);
unsigned long redcloud_get_video_color_key(void);
unsigned long redcloud_get_video_color_key_mask(void);
int redcloud_get_video_palette_entry(unsigned long index,
				     unsigned long *palette);
int redcloud_get_video_color_key_src(void);
int redcloud_get_video_filter(void);
int redcloud_get_video_request(short *x, short *y);
int redcloud_get_video_source(VideoSourceType * source);
int redcloud_get_vbi_source(VbiSourceType * source);
unsigned long redcloud_get_vbi_lines(int odd);
unsigned long redcloud_get_vbi_total(int odd);
int redcloud_get_video_interlaced(void);
int redcloud_get_color_space_YUV(void);
int redcloud_get_vertical_scaler_offset(char *offset);
unsigned long redcloud_get_genlock_delay(void);
int redcloud_get_genlock_enable(void);
int redcloud_get_video_cursor(unsigned long *key, unsigned long *mask,
			      unsigned short *select_color2,
			      unsigned long *color1, unsigned short *color2);
unsigned long redcloud_read_crc(void);
unsigned long redcloud_read_crc32(void);
unsigned long redcloud_read_window_crc(int source, unsigned short x,
				       unsigned short y, unsigned short width,
				       unsigned short height, int crc32);
int redcloud_get_macrovision_enable(void);

void redcloud_get_alpha_enable(int *enable);
void redcloud_get_alpha_size(unsigned short *x, unsigned short *y,
			     unsigned short *width, unsigned short *height);
void redcloud_get_alpha_value(unsigned char *alpha, char *delta);
void redcloud_get_alpha_priority(int *priority);
void redcloud_get_alpha_color(unsigned long *color);
unsigned long redcloud_get_clock_frequency(void);
int redcloud_get_sync_polarities(void);

/*---------------------------------------------------------------------------
 * gfx_reset_video (PRIVATE ROUTINE: NOT PART OF DURANGO API)
 *
 * This routine is used to disable all components of video overlay before
 * performing a mode switch.
 *---------------------------------------------------------------------------
 */
#if GFX_VIDEO_DYNAMIC
void
redcloud_reset_video(void)
#else
void
gfx_reset_video(void)
#endif
{
   gfx_set_video_enable(0);
   gfx_select_alpha_region(1);
   gfx_set_alpha_enable(0);
   gfx_select_alpha_region(2);
   gfx_set_alpha_enable(0);

   /* SET REGION 0 AFTER RESET */

   gfx_select_alpha_region(0);
   gfx_set_alpha_enable(0);
}

/*-----------------------------------------------------------------------------
 * gfx_set_display_control (PRIVATE ROUTINE: NOT PART OF DURANGO API)
 *
 * This routine configures the display output.
 *
 * "sync_polarities" is used to set the polarities of the sync pulses according
 * to the following mask:
 *
 *     Bit 0: If set to 1, negative horizontal polarity is programmed,
 *            otherwise positive horizontal polarity is programmed.
 *     Bit 1: If set to 1, negative vertical polarity is programmed,
 *            otherwise positive vertical polarity is programmed.
 *
 *-----------------------------------------------------------------------------
 */
#if GFX_VIDEO_DYNAMIC
int
redcloud_set_display_control(int sync_polarities)
#else
int
gfx_set_display_control(int sync_polarities)
#endif
{
   unsigned long power;
   unsigned long dcfg;

   /* CONFIGURE DISPLAY OUTPUT FROM VIDEO PROCESSOR */

   dcfg = READ_VID32(RCDF_DISPLAY_CONFIG);
   dcfg &= ~(RCDF_DCFG_CRT_SYNC_SKW_MASK | RCDF_DCFG_PWR_SEQ_DLY_MASK |
	     RCDF_DCFG_CRT_HSYNC_POL | RCDF_DCFG_CRT_VSYNC_POL |
	     RCDF_DCFG_FP_PWR_EN | RCDF_DCFG_FP_DATA_EN);

   dcfg |= (RCDF_DCFG_CRT_SYNC_SKW_INIT |
	    RCDF_DCFG_PWR_SEQ_DLY_INIT | RCDF_DCFG_GV_PAL_BYP);

   if (PanelEnable) {
      power = READ_VID32(RCDF_POWER_MANAGEMENT);
      power |= RCDF_PM_PANEL_POWER_ON;
      WRITE_VID32(RCDF_POWER_MANAGEMENT, power);
   }

   /* SET APPROPRIATE SYNC POLARITIES */

   if (sync_polarities & 0x1)
      dcfg |= RCDF_DCFG_CRT_HSYNC_POL;
   if (sync_polarities & 0x2)
      dcfg |= RCDF_DCFG_CRT_VSYNC_POL;

   WRITE_VID32(RCDF_DISPLAY_CONFIG, dcfg);

   return (0);
}

/*---------------------------------------------------------------------------
 * gfx_set_clock_frequency
 *
 * This routine sets the clock frequency, specified as a 16.16 fixed point
 * value (0x00318000 = 49.5 MHz).  It will set the closest frequency found
 * in the lookup table.
 *---------------------------------------------------------------------------
 */
#if GFX_VIDEO_DYNAMIC
void
redcloud_set_clock_frequency(unsigned long frequency)
#else
void
gfx_set_clock_frequency(unsigned long frequency)
#endif
{
   Q_WORD msr_value;
   unsigned int i, index = 0;
   unsigned long value;
   long timeout = 1000;
   long min, diff;
   RCDFPLLENTRY *PllTable;

   /* READ PLL REFERENCE FREQUENCY */
   /* The reference frequency of GX2 1.x is different from 2.x and above. */

   if ((gfx_cpu_version & 0xFF00) >= 0x0200)
      PllTable = RCDF_PLLtable48MHz;
   else
      PllTable = RCDF_PLLtable14MHz;

   /* FIND THE REGISTER VALUES FOR THE DESIRED FREQUENCY */
   /* Search the table for the closest frequency (16.16 format). */

   value = PllTable[0].pll_value;
   min = (long)PllTable[0].frequency - frequency;
   if (min < 0L)
      min = -min;
   for (i = 1; i < NUM_RCDF_FREQUENCIES; i++) {
      diff = (long)PllTable[i].frequency - frequency;
      if (diff < 0L)
	 diff = -diff;
      if (diff < min) {
	 min = diff;
	 index = i;
      }
   }

   /* PROGRAM THE SETTINGS WITH THE RESET BIT SET */
   /* Clear the bypass bit to ensure that the programmed */
   /* M, N and P values are being used.                  */

   gfx_msr_read(RC_ID_MCP, MCP_DOTPLL, &msr_value);
   msr_value.high = PllTable[index].pll_value;
   msr_value.low |= 0x00000001;
   msr_value.low &= ~MCP_DOTPLL_BYPASS;
   gfx_msr_write(RC_ID_MCP, MCP_DOTPLL, &msr_value);

   /* PROGRAM THE MCP DIVIDER VALUES */

   gfx_msr_read(RC_ID_MCP, MCP_SYS_RSTPLL, &msr_value);
   if (PllTable[index].post_div3)
      msr_value.low |= MCP_DOTPOSTDIV3;
   else
      msr_value.low &= ~MCP_DOTPOSTDIV3;
   if (PllTable[index].pre_div2)
      msr_value.low |= MCP_DOTPREDIV2;
   else
      msr_value.low &= ~MCP_DOTPREDIV2;
   if (PllTable[index].pre_mul2)
      msr_value.low |= MCP_DOTPREMULT2;
   else
      msr_value.low &= ~MCP_DOTPREMULT2;
   gfx_msr_write(RC_ID_MCP, MCP_SYS_RSTPLL, &msr_value);

   /* CLEAR THE RESET BIT */

   gfx_msr_read(RC_ID_MCP, MCP_DOTPLL, &msr_value);
   msr_value.low &= 0xFFFFFFFE;
   gfx_msr_write(RC_ID_MCP, MCP_DOTPLL, &msr_value);

   /* WAIT FOR LOCK BIT */

   do {
      gfx_msr_read(RC_ID_MCP, MCP_DOTPLL, &msr_value);
   } while (timeout-- && !(msr_value.low & MCP_DOTPLL_LOCK));
}

/*---------------------------------------------------------------------------
 * gfx_set_crt_enable
 * 
 * This routine enables or disables the CRT output from the video processor.
 *---------------------------------------------------------------------------
 */
#if GFX_VIDEO_DYNAMIC
int
redcloud_set_crt_enable(int enable)
#else
int
gfx_set_crt_enable(int enable)
#endif
{
   unsigned long config, misc;

   config = READ_VID32(RCDF_DISPLAY_CONFIG);
   misc = READ_VID32(RCDF_VID_MISC);

   switch (enable) {
   case CRT_DISABLE:			/* DISABLE EVERYTHING */

      WRITE_VID32(RCDF_DISPLAY_CONFIG,
		  config & ~(RCDF_DCFG_DIS_EN | RCDF_DCFG_HSYNC_EN |
			     RCDF_DCFG_VSYNC_EN | RCDF_DCFG_DAC_BL_EN));
      WRITE_VID32(RCDF_VID_MISC, misc | RCDF_DAC_POWER_DOWN);
      break;

   case CRT_ENABLE:			/* ENABLE CRT DISPLAY, INCLUDING DISPLAY LOGIC */

      WRITE_VID32(RCDF_DISPLAY_CONFIG,
		  config | RCDF_DCFG_DIS_EN | RCDF_DCFG_HSYNC_EN |
		  RCDF_DCFG_VSYNC_EN | RCDF_DCFG_DAC_BL_EN);
      WRITE_VID32(RCDF_VID_MISC,
		  misc & ~RCDF_DAC_POWER_DOWN & ~RCDF_ANALOG_POWER_DOWN);
      break;

   case CRT_STANDBY:			/* HSYNC:OFF VSYNC:ON */

      WRITE_VID32(RCDF_DISPLAY_CONFIG,
		  (config &
		   ~(RCDF_DCFG_DIS_EN | RCDF_DCFG_HSYNC_EN |
		     RCDF_DCFG_DAC_BL_EN)) | RCDF_DCFG_VSYNC_EN);
      WRITE_VID32(RCDF_VID_MISC, misc | RCDF_DAC_POWER_DOWN);
      break;

   case CRT_SUSPEND:			/* HSYNC:ON VSYNC:OFF */

      WRITE_VID32(RCDF_DISPLAY_CONFIG,
		  (config &
		   ~(RCDF_DCFG_DIS_EN | RCDF_DCFG_VSYNC_EN |
		     RCDF_DCFG_DAC_BL_EN)) | RCDF_DCFG_HSYNC_EN);
      WRITE_VID32(RCDF_VID_MISC, misc | RCDF_DAC_POWER_DOWN);
      break;

   default:
      return (GFX_STATUS_BAD_PARAMETER);
   }
   return (GFX_STATUS_OK);
}

/*-----------------------------------------------------------------------------
 * gfx_set_video_enable
 *
 * This routine enables or disables the video overlay functionality.
 *-----------------------------------------------------------------------------
 */
#if GFX_VIDEO_DYNAMIC
int
redcloud_set_video_enable(int enable)
#else
int
gfx_set_video_enable(int enable)
#endif
{
   unsigned long vcfg;

   /* WAIT FOR VERTICAL BLANK TO START */
   /* Otherwise a glitch can be observed. */

   if (gfx_test_timing_active()) {
      if (!gfx_test_vertical_active()) {
	 while (!gfx_test_vertical_active()) ;
      }
      while (gfx_test_vertical_active()) ;
   }

   vcfg = READ_VID32(RCDF_VIDEO_CONFIG);
   if (enable) {
      /* ENABLE VIDEO OVERLAY FROM DISPLAY CONTROLLER */
      /* Use private routine to abstract the display controller. */

      gfx_set_display_video_enable(1);

      /* ENABLE DISPLAY FILTER VIDEO OVERLAY */

      vcfg |= RCDF_VCFG_VID_EN;
      WRITE_VID32(RCDF_VIDEO_CONFIG, vcfg);
   } else {
      /* DISABLE DISPLAY FILTER VIDEO OVERLAY */

      vcfg &= ~RCDF_VCFG_VID_EN;
      WRITE_VID32(RCDF_VIDEO_CONFIG, vcfg);

      /* DISABLE VIDEO OVERLAY FROM DISPLAY CONTROLLER */
      /* Use private routine to abstract the display controller. */

      gfx_set_display_video_enable(0);
   }
   return (0);
}

/*-----------------------------------------------------------------------------
 * gfx_set_video_format
 *
 * Sets input video format type, to one of the YUV formats or to RGB.
 *-----------------------------------------------------------------------------
 */
#if GFX_VIDEO_DYNAMIC
int
redcloud_set_video_format(unsigned long format)
#else
int
gfx_set_video_format(unsigned long format)
#endif
{
   unsigned long ctrl, vcfg = 0;

   /* SET THE DISPLAY FILTER VIDEO INPUT FORMAT */

   vcfg = READ_VID32(RCDF_VIDEO_CONFIG);
   ctrl = READ_VID32(RCDF_VID_ALPHA_CONTROL);
   ctrl &= ~(RCDF_VIDEO_INPUT_IS_RGB | RCDF_CSC_VIDEO_YUV_TO_RGB);
   vcfg &= ~(RCDF_VCFG_VID_INP_FORMAT | RCDF_VCFG_4_2_0_MODE);
   switch (format) {
   case VIDEO_FORMAT_UYVY:
      vcfg |= RCDF_VCFG_UYVY_FORMAT;
      ctrl |= RCDF_CSC_VIDEO_YUV_TO_RGB;
      break;
   case VIDEO_FORMAT_YUYV:
      vcfg |= RCDF_VCFG_YUYV_FORMAT;
      ctrl |= RCDF_CSC_VIDEO_YUV_TO_RGB;
      break;
   case VIDEO_FORMAT_Y2YU:
      vcfg |= RCDF_VCFG_Y2YU_FORMAT;
      ctrl |= RCDF_CSC_VIDEO_YUV_TO_RGB;
      break;
   case VIDEO_FORMAT_YVYU:
      vcfg |= RCDF_VCFG_YVYU_FORMAT;
      ctrl |= RCDF_CSC_VIDEO_YUV_TO_RGB;
      break;
   case VIDEO_FORMAT_Y0Y1Y2Y3:
      vcfg |= RCDF_VCFG_UYVY_FORMAT;
      ctrl |= RCDF_CSC_VIDEO_YUV_TO_RGB;
      vcfg |= RCDF_VCFG_4_2_0_MODE;
      break;
   case VIDEO_FORMAT_Y3Y2Y1Y0:
      vcfg |= RCDF_VCFG_Y2YU_FORMAT;
      ctrl |= RCDF_CSC_VIDEO_YUV_TO_RGB;
      vcfg |= RCDF_VCFG_4_2_0_MODE;
      break;
   case VIDEO_FORMAT_Y1Y0Y3Y2:
      vcfg |= RCDF_VCFG_YUYV_FORMAT;
      ctrl |= RCDF_CSC_VIDEO_YUV_TO_RGB;
      vcfg |= RCDF_VCFG_4_2_0_MODE;
      break;
   case VIDEO_FORMAT_Y1Y2Y3Y0:
      vcfg |= RCDF_VCFG_YVYU_FORMAT;
      ctrl |= RCDF_CSC_VIDEO_YUV_TO_RGB;
      vcfg |= RCDF_VCFG_4_2_0_MODE;
      break;
   case VIDEO_FORMAT_RGB:
      ctrl |= RCDF_VIDEO_INPUT_IS_RGB;
      vcfg |= RCDF_VCFG_UYVY_FORMAT;
      break;
   case VIDEO_FORMAT_P2M_P2L_P1M_P1L:
      ctrl |= RCDF_VIDEO_INPUT_IS_RGB;
      vcfg |= RCDF_VCFG_Y2YU_FORMAT;
      break;
   case VIDEO_FORMAT_P1M_P1L_P2M_P2L:
      ctrl |= RCDF_VIDEO_INPUT_IS_RGB;
      vcfg |= RCDF_VCFG_YUYV_FORMAT;
      break;
   case VIDEO_FORMAT_P1M_P2L_P2M_P1L:
      ctrl |= RCDF_VIDEO_INPUT_IS_RGB;
      vcfg |= RCDF_VCFG_YVYU_FORMAT;
      break;
   default:
      return GFX_STATUS_BAD_PARAMETER;
   }
   WRITE_VID32(RCDF_VIDEO_CONFIG, vcfg);
   WRITE_VID32(RCDF_VID_ALPHA_CONTROL, ctrl);

   /* SET THE VIDEO FORMAT IN THE DISPLAY CONTROLLER      */
   /* Use private routine to abstract display controller. */

   gfx_set_display_video_format(format);
   return (0);
}

/*-----------------------------------------------------------------------------
 * gfx_set_video_size
 *
 * This routine specifies the size of the source data.  It is used only 
 * to determine how much data to transfer per frame, and is not used to 
 * calculate the scaling value (that is handled by a separate routine).
 *-----------------------------------------------------------------------------
 */
#if GFX_VIDEO_DYNAMIC
int
redcloud_set_video_size(unsigned short width, unsigned short height)
#else
int
gfx_set_video_size(unsigned short width, unsigned short height)
#endif
{
   unsigned long size, vcfg, vid_420, pitch;

   /* SET THE DISPLAY FILTER VIDEO LINE SIZE                            */
   /* Match the DC hardware alignment requirement.  The line size must  */
   /* always be 32-byte aligned.  However, we can manage smaller        */
   /* alignments by decreasing the pitch and clipping the video window. */
   /* The VG will fetch extra data for each line, but the decreased     */
   /* pitch will ensure that it always begins fetching at the start of  */
   /* the video line.                                                   */

   vcfg = READ_VID32(RCDF_VIDEO_CONFIG);

   vid_420 = vcfg & RCDF_VCFG_4_2_0_MODE;

   vcfg &= ~(RCDF_VCFG_LINE_SIZE_LOWER_MASK | RCDF_VCFG_LINE_SIZE_UPPER);

   size = ((width >> 1) + 7) & 0xFFF8;
   pitch = ((width << 1) + 7) & 0xFFF8;

   vcfg |= (size & 0x00FF) << 8;
   if (size & 0x0100)
      vcfg |= RCDF_VCFG_LINE_SIZE_UPPER;
   WRITE_VID32(RCDF_VIDEO_CONFIG, vcfg);

   /* SET VIDEO BUFFER LINE SIZE IN DISPLAY CONTROLLER */
   /* Use private routine to abstract the display controller. */

   gfx_set_display_video_size(width, height);

   /* SET VIDEO PITCH */
   /* We are only maintaining legacy for 4:2:2 video formats. */
   /* 4:2:0 video in previous chips was inadequate for most   */
   /* common video formats.                                   */

   if (!vid_420)
      gfx_set_video_yuv_pitch(pitch, pitch << 1);

   return (0);
}

/*-----------------------------------------------------------------------------
 * gfx_set_video_offset
 *
 * This routine sets the starting offset for the video buffer when only 
 * one offset needs to be specified.
 *-----------------------------------------------------------------------------
 */
#if GFX_VIDEO_DYNAMIC
int
redcloud_set_video_offset(unsigned long offset)
#else
int
gfx_set_video_offset(unsigned long offset)
#endif
{
   /* SAVE VALUE FOR FUTURE CLIPPING OF THE TOP OF THE VIDEO WINDOW */

   gfx_vid_offset = offset;

   /* SET VIDEO BUFFER OFFSET IN DISPLAY CONTROLLER */
   /* Use private routine to abstract the display controller. */

   gfx_set_display_video_offset(offset);
   return (0);
}

/*-----------------------------------------------------------------------------
 * gfx_set_video_yuv_offsets
 *
 * This routine sets the starting offset for the video buffer when displaying 
 * 4:2:0 video.
 *-----------------------------------------------------------------------------
 */
#if GFX_VIDEO_DYNAMIC
int
redcloud_set_video_yuv_offsets(unsigned long yoffset, unsigned long uoffset,
			       unsigned long voffset)
#else
int
gfx_set_video_yuv_offsets(unsigned long yoffset, unsigned long uoffset,
			  unsigned long voffset)
#endif
{
   /* SAVE VALUE FOR FUTURE CLIPPING OF THE TOP OF THE VIDEO WINDOW */

   gfx_vid_offset = yoffset;
   gfx_vid_uoffset = uoffset;
   gfx_vid_voffset = voffset;

   /* SET VIDEO BUFFER OFFSET IN DISPLAY CONTROLLER */
   /* Use private routine to abstract the display controller. */

   gfx_set_display_video_yuv_offsets(yoffset, uoffset, voffset);

   return (0);
}

/*-----------------------------------------------------------------------------
 * gfx_set_video_yuv_pitch
 *
 * This routine sets the byte offset between consecutive scanlines of YUV video data
 *-----------------------------------------------------------------------------
 */
#if GFX_VIDEO_DYNAMIC
int
redcloud_set_video_yuv_pitch(unsigned long ypitch, unsigned long uvpitch)
#else
int
gfx_set_video_yuv_pitch(unsigned long ypitch, unsigned long uvpitch)
#endif
{
   /* SET VIDEO PITCH IN DISPLAY CONTROLLER */
   /* Use private routine to abstract the display controller. */

   gfx_set_display_video_yuv_pitch(ypitch, uvpitch);

   return (0);
}

/*---------------------------------------------------------------------------
 * gfx_set_video_scale
 * 
 * This routine sets the scale factor for the video overlay window.  The 
 * size of the source and destination regions are specified in pixels.  
 *---------------------------------------------------------------------------
 */
#if GFX_VIDEO_DYNAMIC
int
redcloud_set_video_scale(unsigned short srcw, unsigned short srch,
			 unsigned short dstw, unsigned short dsth)
#else
int
gfx_set_video_scale(unsigned short srcw, unsigned short srch,
		    unsigned short dstw, unsigned short dsth)
#endif
{
   unsigned long xscale, yscale;

   /* SAVE PARAMETERS (unless don't-care zero destination arguments are used) */
   /* These are needed for clipping the video window later. */

   if (dstw != 0) {
      gfx_vid_srcw = srcw;
      gfx_vid_dstw = dstw;
   }
   if (dsth != 0) {
      gfx_vid_srch = srch;
      gfx_vid_dsth = dsth;
   }

   /* CALCULATE DISPLAY FILTER SCALE FACTORS */
   /* Zero width and height indicate don't care conditions */
   /* Downscaling is performed in a separate function.     */

   if (dstw == 0)
      xscale = READ_VID32(RCDF_VIDEO_SCALE) & 0xffff;	/* keep previous if don't-care argument */
   else if (dstw <= srcw)
      xscale = 0x2000;			/* horizontal downscaling is currently done in a separate function */
   else if ((srcw == 1) || (dstw == 1))
      return GFX_STATUS_BAD_PARAMETER;
   else
      xscale = (0x2000l * (srcw - 1l)) / (dstw - 1l);

   if (dsth == 0)
      yscale = (READ_VID32(RCDF_VIDEO_SCALE) & 0xffff0000) >> 16;	/* keep previous if don't-care argument */
   else if (dsth <= srch)
      yscale = 0x2000;			/* vertical downscaling is handled in a separate function */
   else if ((srch == 1) || (dsth == 1))
      return GFX_STATUS_BAD_PARAMETER;
   else
      yscale = (0x2000l * (srch - 1l)) / (dsth - 1l);

   WRITE_VID32(RCDF_VIDEO_SCALE, (yscale << 16) | xscale);

   /* CALL ROUTINE TO UPDATE WINDOW POSITION */
   /* This is required because the scale values affect the number of */
   /* source data pixels that need to be clipped, as well as the     */
   /* amount of data that needs to be transferred.                   */

   gfx_set_video_window(gfx_vid_xpos, gfx_vid_ypos, gfx_vid_width,
			gfx_vid_height);
   return (0);
}

/*---------------------------------------------------------------------------
 * gfx_set_video_vertical_downscale
 * 
 * This routine sets the vertical downscale factor for the video overlay window.  
 * The height of the source and destination regions are specified in pixels.  
 *---------------------------------------------------------------------------
 */
#if GFX_VIDEO_DYNAMIC
int
redcloud_set_video_vertical_downscale(unsigned short srch,
				      unsigned short dsth)
#else
int
gfx_set_video_vertical_downscale(unsigned short srch, unsigned short dsth)
#endif
{
   /* SET VIDEO SCALE IN DISPLAY CONTROLLER    */
   /* Use private routine to abstract hardware */

   gfx_set_display_video_downscale(srch, dsth);
   return 0;
}

/*---------------------------------------------------------------------------
 * gfx_set_video_vertical_downscale_enable
 * 
 * This routine sets the vertical downscale enable for the video overlay window.  
 *---------------------------------------------------------------------------
 */
#if GFX_VIDEO_DYNAMIC
void
redcloud_set_video_vertical_downscale_enable(int enable)
#else
void
gfx_set_video_vertical_downscale_enable(int enable)
#endif
{
   /* SET VIDEO SCALE IN DISPLAY CONTROLLER    */
   /* Use private routine to abstract hardware */

   gfx_set_display_video_vertical_downscale_enable(enable);
}

/*---------------------------------------------------------------------------
 * gfx_set_video_downscale_config
 * 
 * This routine sets the downscale type and factor for the video overlay window.
 * Note: No downscaling support for RGB565 and YUV420 video formats.
 *---------------------------------------------------------------------------
 */
#if GFX_VIDEO_DYNAMIC
int
redcloud_set_video_downscale_config(unsigned short type, unsigned short m)
#else
int
gfx_set_video_downscale_config(unsigned short type, unsigned short m)
#endif
{
   unsigned long downscale;

   if ((m < 1) || (m > 16))
      return GFX_STATUS_BAD_PARAMETER;

   downscale = READ_VID32(RCDF_VIDEO_DOWNSCALER_CONTROL);
   downscale &=
	 ~(RCDF_VIDEO_DOWNSCALE_FACTOR_MASK | RCDF_VIDEO_DOWNSCALE_TYPE_MASK);
   downscale |= ((unsigned long)(m - 1) << RCDF_VIDEO_DOWNSCALE_FACTOR_POS);
   switch (type) {
   case VIDEO_DOWNSCALE_KEEP_1_OF:
      downscale |= RCDF_VIDEO_DOWNSCALE_TYPE_A;
      break;
   case VIDEO_DOWNSCALE_DROP_1_OF:
      downscale |= RCDF_VIDEO_DOWNSCALE_TYPE_B;
      break;
   default:
      return GFX_STATUS_BAD_PARAMETER;
   }
   WRITE_VID32(RCDF_VIDEO_DOWNSCALER_CONTROL, downscale);
   return (0);
}

/*---------------------------------------------------------------------------
 * gfx_set_video_downscale_coefficients
 * 
 * This routine sets the downscale filter coefficients.
 *---------------------------------------------------------------------------
 */
#if GFX_VIDEO_DYNAMIC
int
redcloud_set_video_downscale_coefficients(unsigned short coef1,
					  unsigned short coef2,
					  unsigned short coef3,
					  unsigned short coef4)
#else
int
gfx_set_video_downscale_coefficients(unsigned short coef1,
				     unsigned short coef2,
				     unsigned short coef3,
				     unsigned short coef4)
#endif
{
   if ((coef1 + coef2 + coef3 + coef4) != 16)
      return GFX_STATUS_BAD_PARAMETER;

   WRITE_VID32(RCDF_VIDEO_DOWNSCALER_COEFFICIENTS,
	       ((unsigned long)coef1 << RCDF_VIDEO_DOWNSCALER_COEF1_POS) |
	       ((unsigned long)coef2 << RCDF_VIDEO_DOWNSCALER_COEF2_POS) |
	       ((unsigned long)coef3 << RCDF_VIDEO_DOWNSCALER_COEF3_POS) |
	       ((unsigned long)coef4 << RCDF_VIDEO_DOWNSCALER_COEF4_POS));
   return (0);
}

/*---------------------------------------------------------------------------
 * gfx_set_video_downscale_enable
 * 
 * This routine enables or disables downscaling for the video overlay window.
 *---------------------------------------------------------------------------
 */
#if GFX_VIDEO_DYNAMIC
int
redcloud_set_video_downscale_enable(int enable)
#else
int
gfx_set_video_downscale_enable(int enable)
#endif
{
   unsigned long downscale;

   downscale = READ_VID32(RCDF_VIDEO_DOWNSCALER_CONTROL);
   if (enable)
      downscale |= RCDF_VIDEO_DOWNSCALE_ENABLE;
   else
      downscale &= ~RCDF_VIDEO_DOWNSCALE_ENABLE;
   WRITE_VID32(RCDF_VIDEO_DOWNSCALER_CONTROL, downscale);
   return (0);
}

/*---------------------------------------------------------------------------
 * gfx_set_video_window
 * 
 * This routine sets the position and size of the video overlay window.  The 
 * x and y positions are specified in screen relative coordinates, and may be negative.  
 * The size of destination region is specified in pixels.  The line size
 * indicates the number of bytes of source data per scanline.
 *---------------------------------------------------------------------------
 */
#if GFX_VIDEO_DYNAMIC
int
redcloud_set_video_window(short x, short y, unsigned short w,
			  unsigned short h)
#else
int
gfx_set_video_window(short x, short y, unsigned short w, unsigned short h)
#endif
{
   unsigned long hadjust, vadjust;
   unsigned long xstart, ystart, xend, yend;

   /* SAVE PARAMETERS */
   /* These are needed to call this routine if the scale value changes. */

   gfx_vid_xpos = x;
   gfx_vid_ypos = y;
   gfx_vid_width = w;
   gfx_vid_height = h;

   /* GET ADJUSTMENT VALUES */
   /* Use routines to abstract version of display controller. */

   hadjust = gfx_get_htotal() - gfx_get_hsync_end() - 14l;
   vadjust = gfx_get_vtotal() - gfx_get_vsync_end() + 1l;

   /* HORIZONTAL START */
   xstart = (unsigned long)x + hadjust;

   /* HORIZONTAL END */
   /* End positions in register are non-inclusive (one more than the actual end) */

   if ((x + w) < gfx_get_hactive())
      xend = (unsigned long)x + (unsigned long)w + hadjust;

   /* RIGHT-CLIPPING */
   else
      xend = (unsigned long)gfx_get_hactive() + hadjust;

   /* VERTICAL START */

   ystart = (unsigned long)y + vadjust;

   /* VERTICAL END */

   if ((y + h) < gfx_get_vactive())
      yend = (unsigned long)y + (unsigned long)h + vadjust;

   /* BOTTOM-CLIPPING */
   else
      yend = (unsigned long)gfx_get_vactive() + vadjust;

   /* SET VIDEO POSITION */

   WRITE_VID32(RCDF_VIDEO_X_POS, (xend << 16) | xstart);
   WRITE_VID32(RCDF_VIDEO_Y_POS, (yend << 16) | ystart);

   return (0);
}

/*---------------------------------------------------------------------------
 * gfx_set_video_left_crop
 * 
 * This routine sets the number of pixels which will be cropped from the
 * beginning of each video line. The video window will begin to display only
 * from the pixel following the cropped pixels, and the cropped pixels
 * will be ignored.
 *---------------------------------------------------------------------------
 */
#if GFX_VIDEO_DYNAMIC
int
redcloud_set_video_left_crop(unsigned short x)
#else
int
gfx_set_video_left_crop(unsigned short x)
#endif
{
   unsigned long vcfg, initread;

   vcfg = READ_VID32(RCDF_VIDEO_CONFIG);

   /* CLIPPING ON LEFT */
   /* Adjust initial read for scale, checking for divide by zero. Mask the     */
   /* lower three bits when clipping 4:2:0 video.  By masking the bits instead */
   /* of rounding up we ensure that we always clip less than or equal to the   */
   /* desired number of pixels.  This prevents visual artifacts from           */
   /* over-clipping.  We mask three bits to meet the HW requirement that 4:2:0 */
   /* clipping be 16-byte or 8-pixel aligned.                                  */

   if (gfx_vid_dstw) {
      initread = (unsigned long)x *gfx_vid_srcw / gfx_vid_dstw;

      if (vcfg & RCDF_VCFG_4_2_0_MODE)
	 initread &= 0xFFF8;
   } else
      initread = 0;

   /* SET INITIAL READ ADDRESS */

   vcfg &= ~RCDF_VCFG_INIT_READ_MASK;
   vcfg |= (initread << 15) & RCDF_VCFG_INIT_READ_MASK;
   WRITE_VID32(RCDF_VIDEO_CONFIG, vcfg);
   return (0);
}

/*---------------------------------------------------------------------------
 * gfx_set_video_color_key
 * 
 * This routine specifies the color key value and mask for the video overlay
 * hardware. To disable color key, the color and mask should both be set to 
 * zero. The hardware uses the color key in the following equation:
 *
 * ((source data) & (color key mask)) == ((color key) & (color key mask))
 *
 * If "graphics" is set to TRUE, the source data is graphics, and color key
 * is an RGB value. If "graphics" is set to FALSE, the source data is the video,
 * and color key is a YUV value.
 *---------------------------------------------------------------------------
 */
#if GFX_VIDEO_DYNAMIC
int
redcloud_set_video_color_key(unsigned long key, unsigned long mask,
			     int graphics)
#else
int
gfx_set_video_color_key(unsigned long key, unsigned long mask, int graphics)
#endif
{
   unsigned long dcfg = 0;

   /* SET RCDF COLOR KEY VALUE */

   WRITE_VID32(RCDF_VIDEO_COLOR_KEY, key);
   WRITE_VID32(RCDF_VIDEO_COLOR_MASK, mask);

   /* SELECT GRAPHICS OR VIDEO DATA TO COMPARE TO THE COLOR KEY */

   dcfg = READ_VID32(RCDF_DISPLAY_CONFIG);
   if (graphics & 0x01)
      dcfg &= ~RCDF_DCFG_VG_CK;
   else
      dcfg |= RCDF_DCFG_VG_CK;
   WRITE_VID32(RCDF_DISPLAY_CONFIG, dcfg);
   return (0);
}

/*---------------------------------------------------------------------------
 * gfx_set_video_filter
 * 
 * This routine enables or disables the video overlay filters.
 *---------------------------------------------------------------------------
 */
#if GFX_VIDEO_DYNAMIC
int
redcloud_set_video_filter(int xfilter, int yfilter)
#else
int
gfx_set_video_filter(int xfilter, int yfilter)
#endif
{
   unsigned long vcfg = 0;

   /* ENABLE OR DISABLE DISPLAY FILTER VIDEO OVERLAY FILTERS */

   vcfg = READ_VID32(RCDF_VIDEO_CONFIG);
   vcfg &= ~(RCDF_VCFG_X_FILTER_EN | RCDF_VCFG_Y_FILTER_EN);
   if (xfilter)
      vcfg |= RCDF_VCFG_X_FILTER_EN;
   if (yfilter)
      vcfg |= RCDF_VCFG_Y_FILTER_EN;
   WRITE_VID32(RCDF_VIDEO_CONFIG, vcfg);
   return (0);
}

/*---------------------------------------------------------------------------
 * gfx_set_video_palette
 * 
 * This routine loads the video hardware palette.  If a NULL pointer is 
 * specified, the palette is bypassed (for Redcloud, this means loading the 
 * palette with identity values). 
 *---------------------------------------------------------------------------
 */
#if GFX_VIDEO_DYNAMIC
int
redcloud_set_video_palette(unsigned long *palette)
#else
int
gfx_set_video_palette(unsigned long *palette)
#endif
{
   unsigned long i, entry;

   /* LOAD REDCLOUD VIDEO PALETTE */

   WRITE_VID32(RCDF_PALETTE_ADDRESS, 0);
   for (i = 0; i < 256; i++) {
      if (palette)
	 entry = palette[i];
      else
	 entry = i | (i << 8) | (i << 16);
      WRITE_VID32(RCDF_PALETTE_DATA, entry);
   }
   return (0);
}

/*---------------------------------------------------------------------------
 * gfx_set_video_palette_entry
 * 
 * This routine loads a single entry of the video hardware palette.  
 *---------------------------------------------------------------------------
 */
#if GFX_VIDEO_DYNAMIC
int
redcloud_set_video_palette_entry(unsigned long index, unsigned long palette)
#else
int
gfx_set_video_palette_entry(unsigned long index, unsigned long palette)
#endif
{
   if (index > 0xFF)
      return GFX_STATUS_BAD_PARAMETER;

   /* SET A SINGLE ENTRY */

   WRITE_VID32(RCDF_PALETTE_ADDRESS, index);
   WRITE_VID32(RCDF_PALETTE_DATA, palette);

   return (0);
}

/*---------------------------------------------------------------------------
 * gfx_set_video_request()
 * 
 * This routine sets the horizontal (pixel) and vertical (line) video request
 * values.
 *---------------------------------------------------------------------------
 */
#if GFX_VIDEO_DYNAMIC
int
redcloud_set_video_request(short x, short y)
#else
int
gfx_set_video_request(short x, short y)
#endif
{
   /* SET DISPLAY FILTER VIDEO REQUEST */

   x += gfx_get_htotal() - gfx_get_hsync_end() - 2;
   y += gfx_get_vtotal() - gfx_get_vsync_end() + 1;

   if ((x < 0) || (x > RCDF_VIDEO_REQUEST_MASK) ||
       (y < 0) || (y > RCDF_VIDEO_REQUEST_MASK))
      return GFX_STATUS_BAD_PARAMETER;

   WRITE_VID32(RCDF_VIDEO_REQUEST,
	       ((unsigned long)x << RCDF_VIDEO_X_REQUEST_POS) |
	       ((unsigned long)y << RCDF_VIDEO_Y_REQUEST_POS));
   return (0);
}

/*---------------------------------------------------------------------------
 * gfx_set_video_cursor()
 * 
 * This routine configures the video hardware cursor.
 * If the "mask"ed bits in the graphics pixel match "key", then either "color1"
 * or "color2" will be used for this pixel, according to the value of bit
 * number "select_color2" of the graphics pixel.
 *
 * key - 24 bit RGB value
 * mask - 24 bit mask
 * color1, color2 - RGB or YUV, depending on the current color space conversion
 * select_color2 - value between 0 to 23
 *
 * To disable match, a "mask" and "key" value of 0xffffff should be set,
 * because the graphics pixels incoming to the video processor have maximum 16
 * bits set (0xF8FCF8).
 *
 * This feature is useful for disabling alpha blending of the cursor.
 * Otherwise cursor image would be blurred (or completely invisible if video
 * alpha is maximum value).
 * Note: the cursor pixel replacements take place both inside and outside the
 * video overlay window.
 *---------------------------------------------------------------------------
 */
#if GFX_VIDEO_DYNAMIC
int
redcloud_set_video_cursor(unsigned long key, unsigned long mask,
			  unsigned short select_color2, unsigned long color1,
			  unsigned long color2)
#else
int
gfx_set_video_cursor(unsigned long key, unsigned long mask,
		     unsigned short select_color2, unsigned long color1,
		     unsigned long color2)
#endif
{
   if (select_color2 > RCDF_CURSOR_COLOR_BITS)
      return GFX_STATUS_BAD_PARAMETER;
   key = (key & RCDF_COLOR_MASK) | ((unsigned long)select_color2 <<
				    RCDF_CURSOR_COLOR_KEY_OFFSET_POS);
   WRITE_VID32(RCDF_CURSOR_COLOR_KEY, key);
   WRITE_VID32(RCDF_CURSOR_COLOR_MASK, mask);
   WRITE_VID32(RCDF_CURSOR_COLOR_1, color1);
   WRITE_VID32(RCDF_CURSOR_COLOR_2, color2);
   return (0);
}

/*---------------------------------------------------------------------------
 * gfx_set_video_cursor()
 * 
 * This routine configures the video hardware cursor.
 * If the "mask"ed bits in the graphics pixel match "key", then either "color1"
 * or "color2" will be used for this pixel, according to the value of bit
 * number "select_color2" of the graphics pixel.
 *
 * key - 24 bit RGB value
 * mask - 24 bit mask
 * color1, color2 - RGB or YUV, depending on the current color space conversion
 * select_color2 - value between 0 to 23
 *
 * To disable match, a "mask" and "key" value of 0xffffff should be set,
 * because the graphics pixels incoming to the video processor have maximum 16
 * bits set (0xF8FCF8).
 *
 * This feature is useful for disabling alpha blending of the cursor.
 * Otherwise cursor image would be blurred (or completely invisible if video
 * alpha is maximum value).
 * Note: the cursor pixel replacements take place both inside and outside the
 * video overlay window.
 *---------------------------------------------------------------------------
 */
#if GFX_VIDEO_DYNAMIC
int
redcloud_set_video_cursor_enable(int enable)
#else
int
gfx_set_video_cursor_enable(int enable)
#endif
{
   unsigned long temp = READ_VID32(RCDF_CURSOR_COLOR_KEY);

   if (enable)
      temp |= RCDF_CURSOR_COLOR_KEY_ENABLE;
   else
      temp &= ~RCDF_CURSOR_COLOR_KEY_ENABLE;

   WRITE_VID32(RCDF_CURSOR_COLOR_KEY, temp);
   return (0);
}

/*---------------------------------------------------------------------------
 * gfx_set_alpha_enable
 * 
 * This routine enables or disables the currently selected alpha region.
 *---------------------------------------------------------------------------
 */
#if GFX_VIDEO_DYNAMIC
int
redcloud_set_alpha_enable(int enable)
#else
int
gfx_set_alpha_enable(int enable)
#endif
{
   unsigned long address = 0, value = 0;

   if (gfx_alpha_select > 2)
      return (GFX_STATUS_UNSUPPORTED);
   address = RCDF_ALPHA_CONTROL_1 + ((unsigned long)gfx_alpha_select << 5);
   value = READ_VID32(address);
   if (enable)
      value |= RCDF_ACTRL_WIN_ENABLE;
   else
      value &= ~(RCDF_ACTRL_WIN_ENABLE);
   WRITE_VID32(address, value);
   return (GFX_STATUS_OK);
}

/*---------------------------------------------------------------------------
 * gfx_set_alpha_window
 * 
 * This routine sets the size of the currently selected alpha region.
 * Note: "x" and "y" are signed to enable using negative values needed for
 * implementing workarounds of hardware issues.
 *---------------------------------------------------------------------------
 */
#if GFX_VIDEO_DYNAMIC
int
redcloud_set_alpha_window(short x, short y,
			  unsigned short width, unsigned short height)
#else
int
gfx_set_alpha_window(short x, short y,
		     unsigned short width, unsigned short height)
#endif
{
   unsigned long address = 0;

   /* CHECK FOR CLIPPING */

   if ((x + width) > gfx_get_hactive())
      width = gfx_get_hactive() - x;
   if ((y + height) > gfx_get_vactive())
      height = gfx_get_vactive() - y;

   /* ADJUST POSITIONS */

   x += gfx_get_htotal() - gfx_get_hsync_end() - 2;
   y += gfx_get_vtotal() - gfx_get_vsync_end() + 1;

   if (gfx_alpha_select > 2)
      return (GFX_STATUS_UNSUPPORTED);
   address = RCDF_ALPHA_XPOS_1 + ((unsigned long)gfx_alpha_select << 5);

   /* END POSITIONS IN REGISTERS ARE NON-INCLUSIVE (ONE MORE THAN ACTUAL END) */

   WRITE_VID32(address, (unsigned long)x |
	       ((unsigned long)(x + width) << 16));
   WRITE_VID32(address + 8, (unsigned long)y |
	       ((unsigned long)(y + height) << 16));
   return (GFX_STATUS_OK);
}

/*---------------------------------------------------------------------------
 * gfx_set_alpha_value
 * 
 * This routine sets the alpha value for the currently selected alpha
 * region.  It also specifies an increment/decrement value for fading.
 *---------------------------------------------------------------------------
 */
#if GFX_VIDEO_DYNAMIC
int
redcloud_set_alpha_value(unsigned char alpha, char delta)
#else
int
gfx_set_alpha_value(unsigned char alpha, char delta)
#endif
{
   unsigned long address = 0, value = 0;

   if (gfx_alpha_select > 2)
      return (GFX_STATUS_UNSUPPORTED);
   address = RCDF_ALPHA_CONTROL_1 + ((unsigned long)gfx_alpha_select << 5);
   value = READ_VID32(address);
   value &= RCDF_ACTRL_WIN_ENABLE;	/* keep only enable bit */
   value |= (unsigned long)alpha;
   value |= (((unsigned long)delta) & 0xff) << 8;
   value |= RCDF_ACTRL_LOAD_ALPHA;
   WRITE_VID32(address, value);
   return (GFX_STATUS_OK);
}

/*---------------------------------------------------------------------------
 * gfx_set_alpha_priority
 * 
 * This routine sets the priority of the currently selected alpha region.
 * A higher value indicates a higher priority.
 * Note: Priority of enabled alpha windows must be different.
 *---------------------------------------------------------------------------
 */
#if GFX_VIDEO_DYNAMIC
int
redcloud_set_alpha_priority(int priority)
#else
int
gfx_set_alpha_priority(int priority)
#endif
{
   unsigned long pos = 0, value = 0;

   if (priority > 3)
      return (GFX_STATUS_BAD_PARAMETER);
   if (gfx_alpha_select > 2)
      return (GFX_STATUS_UNSUPPORTED);
   value = READ_VID32(RCDF_VID_ALPHA_CONTROL);
   pos = 16 + (gfx_alpha_select << 1);
   value &= ~(0x03l << pos);
   value |= (unsigned long)priority << pos;
   WRITE_VID32(RCDF_VID_ALPHA_CONTROL, value);
   return (GFX_STATUS_OK);
}

/*---------------------------------------------------------------------------
 * gfx_set_alpha_color
 * 
 * This routine sets the color to be displayed inside the currently selected
 * alpha window when there is a color key match (when the alpha color
 * mechanism is enabled).
 * "color" is an RGB value (for RGB blending) or a YUV value (for YUV blending).
 * In Interlaced YUV blending mode, Y/2 value should be used.
 *---------------------------------------------------------------------------
 */
#if GFX_VIDEO_DYNAMIC
int
redcloud_set_alpha_color(unsigned long color)
#else
int
gfx_set_alpha_color(unsigned long color)
#endif
{
   unsigned long address = 0;

   if (gfx_alpha_select > 2)
      return (GFX_STATUS_UNSUPPORTED);
   address = RCDF_ALPHA_COLOR_1 + ((unsigned long)gfx_alpha_select << 5);
   WRITE_VID32(address, color);
   return (GFX_STATUS_OK);
}

/*---------------------------------------------------------------------------
 * gfx_set_alpha_color_enable
 * 
 * Enable or disable the color mechanism in the alpha window.
 *---------------------------------------------------------------------------
 */
#if GFX_VIDEO_DYNAMIC
int
redcloud_set_alpha_color_enable(int enable)
#else
int
gfx_set_alpha_color_enable(int enable)
#endif
{
   unsigned long color;
   unsigned long address = 0;

   if (gfx_alpha_select > 2)
      return (GFX_STATUS_UNSUPPORTED);
   address = RCDF_ALPHA_COLOR_1 + ((unsigned long)gfx_alpha_select << 5);
   color = READ_VID32(address);
   if (enable)
      color |= RCDF_ALPHA_COLOR_ENABLE;
   else
      color &= ~RCDF_ALPHA_COLOR_ENABLE;
   WRITE_VID32(address, color);
   return (GFX_STATUS_OK);
}

/*---------------------------------------------------------------------------
 * gfx_set_no_ck_outside_alpha
 * 
 * This function affects where inside the video window color key or chroma
 * key comparison is done:
 * If enable is TRUE, color/chroma key comparison is performed only inside
 * the enabled alpha windows. Outside the (enabled) alpha windows, only video
 * is displayed if color key is used, and only graphics is displayed if chroma
 * key is used.
 * If enable is FALSE, color/chroma key comparison is performed in all the
 * video window area.
 *---------------------------------------------------------------------------
 */
#if GFX_VIDEO_DYNAMIC
int
redcloud_set_no_ck_outside_alpha(int enable)
#else
int
gfx_set_no_ck_outside_alpha(int enable)
#endif
{
   unsigned long value;

   value = READ_VID32(RCDF_VID_ALPHA_CONTROL);
   if (enable)
      WRITE_VID32(RCDF_VID_ALPHA_CONTROL, value | RCDF_NO_CK_OUTSIDE_ALPHA);
   else
      WRITE_VID32(RCDF_VID_ALPHA_CONTROL, value & ~RCDF_NO_CK_OUTSIDE_ALPHA);
   return (0);
}

/*---------------------------------------------------------------------------
 * gfx_get_clock_frequency
 *
 * This routine returns the current clock frequency in 16.16 format.
 * It reads the current register value and finds the match in the table.
 * If no match is found, this routine returns 0.
 *---------------------------------------------------------------------------
 */
#if GFX_VIDEO_DYNAMIC
unsigned long
redcloud_get_clock_frequency(void)
#else
unsigned long
gfx_get_clock_frequency(void)
#endif
{
   Q_WORD msr_value;
   RCDFPLLENTRY *PLLTable;
   unsigned int index;
   unsigned long value, mask = 0x00001FFF;
   unsigned long post_div3 = 0, pre_mult2 = 0;

   /* READ PLL SETTING */

   gfx_msr_read(RC_ID_MCP, MCP_DOTPLL, &msr_value);
   value = msr_value.high & mask;

   /* READ DIVISOR SETTINGS */

   if ((gfx_cpu_version & 0xFF00) == 0x200) {
      PLLTable = RCDF_PLLtable48MHz;

      gfx_msr_read(RC_ID_MCP, MCP_SYS_RSTPLL, &msr_value);
      post_div3 = (msr_value.low & MCP_DOTPOSTDIV3) ? 1 : 0;
      pre_mult2 = (msr_value.low & MCP_DOTPREMULT2) ? 1 : 0;
   } else
      PLLTable = RCDF_PLLtable14MHz;

   /* SEARCH FOR A MATCH */

   for (index = 0; index < NUM_RCDF_FREQUENCIES; index++) {
      if ((PLLTable[index].pll_value & mask) == value &&
	  post_div3 == PLLTable[index].post_div3 &&
	  pre_mult2 == PLLTable[index].pre_mul2)
	 return (PLLTable[index].frequency);
   }
   return (0);
}

/*************************************************************/
/*  READ ROUTINES  |  INCLUDED FOR DIAGNOSTIC PURPOSES ONLY  */
/*************************************************************/

#if GFX_READ_ROUTINES

/*---------------------------------------------------------------------------
 * gfx_get_sync_polarities
 *
 * This routine returns the polarities of the sync pulses:
 *     Bit 0: Set if negative horizontal polarity.
 *     Bit 1: Set if negative vertical polarity.
 *---------------------------------------------------------------------------
 */
#if GFX_VIDEO_DYNAMIC
int
redcloud_get_sync_polarities(void)
#else
int
gfx_get_sync_polarities(void)
#endif
{
   int polarities = 0;

   if (READ_VID32(RCDF_DISPLAY_CONFIG) & RCDF_DCFG_CRT_HSYNC_POL)
      polarities |= 1;
   if (READ_VID32(RCDF_DISPLAY_CONFIG) & RCDF_DCFG_CRT_VSYNC_POL)
      polarities |= 2;
   return (polarities);
}

/*---------------------------------------------------------------------------
 * gfx_get_video_palette_entry
 *
 * This routine returns a single palette entry.
 *---------------------------------------------------------------------------
 */
#if GFX_VIDEO_DYNAMIC
int
redcloud_get_video_palette_entry(unsigned long index, unsigned long *palette)
#else
int
gfx_get_video_palette_entry(unsigned long index, unsigned long *palette)
#endif
{
   if (index > 0xFF)
      return GFX_STATUS_BAD_PARAMETER;

   /* READ A SINGLE ENTRY */

   WRITE_VID32(RCDF_PALETTE_ADDRESS, index);
   *palette = READ_VID32(RCDF_PALETTE_DATA);

   return (GFX_STATUS_OK);
}

/*-----------------------------------------------------------------------------
 * gfx_get_video_enable
 *
 * This routine returns the value "one" if video overlay is currently enabled,
 * otherwise it returns the value "zero".
 *-----------------------------------------------------------------------------
 */
#if GFX_VIDEO_DYNAMIC
int
redcloud_get_video_enable(void)
#else
int
gfx_get_video_enable(void)
#endif
{
   if (READ_VID32(RCDF_VIDEO_CONFIG) & RCDF_VCFG_VID_EN)
      return (1);
   return (0);
}

/*-----------------------------------------------------------------------------
 * gfx_get_video_format
 *
 * This routine returns the current video overlay format.
 *-----------------------------------------------------------------------------
 */
#if GFX_VIDEO_DYNAMIC
int
redcloud_get_video_format(void)
#else
int
gfx_get_video_format(void)
#endif
{
   unsigned long ctrl, vcfg;

   ctrl = READ_VID32(RCDF_VID_ALPHA_CONTROL);
   vcfg = READ_VID32(RCDF_VIDEO_CONFIG);

   if (ctrl & RCDF_VIDEO_INPUT_IS_RGB) {
      switch (vcfg & RCDF_VCFG_VID_INP_FORMAT) {
      case RCDF_VCFG_UYVY_FORMAT:
	 return VIDEO_FORMAT_RGB;
      case RCDF_VCFG_Y2YU_FORMAT:
	 return VIDEO_FORMAT_P2M_P2L_P1M_P1L;
      case RCDF_VCFG_YUYV_FORMAT:
	 return VIDEO_FORMAT_P1M_P1L_P2M_P2L;
      case RCDF_VCFG_YVYU_FORMAT:
	 return VIDEO_FORMAT_P1M_P2L_P2M_P1L;
      }
   }

   if (vcfg & RCDF_VCFG_4_2_0_MODE) {
      switch (vcfg & RCDF_VCFG_VID_INP_FORMAT) {
      case RCDF_VCFG_UYVY_FORMAT:
	 return VIDEO_FORMAT_Y0Y1Y2Y3;
      case RCDF_VCFG_Y2YU_FORMAT:
	 return VIDEO_FORMAT_Y3Y2Y1Y0;
      case RCDF_VCFG_YUYV_FORMAT:
	 return VIDEO_FORMAT_Y1Y0Y3Y2;
      case RCDF_VCFG_YVYU_FORMAT:
	 return VIDEO_FORMAT_Y1Y2Y3Y0;
      }
   } else {
      switch (vcfg & RCDF_VCFG_VID_INP_FORMAT) {
      case RCDF_VCFG_UYVY_FORMAT:
	 return VIDEO_FORMAT_UYVY;
      case RCDF_VCFG_Y2YU_FORMAT:
	 return VIDEO_FORMAT_Y2YU;
      case RCDF_VCFG_YUYV_FORMAT:
	 return VIDEO_FORMAT_YUYV;
      case RCDF_VCFG_YVYU_FORMAT:
	 return VIDEO_FORMAT_YVYU;
      }
   }
   return (GFX_STATUS_ERROR);
}

/*-----------------------------------------------------------------------------
 * gfx_get_video_src_size
 *
 * This routine returns the size of the source video overlay buffer.  The 
 * return value is (height << 16) | width.
 *-----------------------------------------------------------------------------
 */
#if GFX_VIDEO_DYNAMIC
unsigned long
redcloud_get_video_src_size(void)
#else
unsigned long
gfx_get_video_src_size(void)
#endif
{
   unsigned long width, height, scale, delta;
   int down_enable;

   /* DETERMINE SOURCE WIDTH FROM THE DISPLAY FILTER VIDEO LINE SIZE */

   width = (READ_VID32(RCDF_VIDEO_CONFIG) >> 7) & 0x000001FE;
   if (READ_VID32(RCDF_VIDEO_CONFIG) & RCDF_VCFG_LINE_SIZE_UPPER)
      width += 512l;

   /* DETERMINE SOURCE HEIGHT FROM THE DISPLAY FILTER HEIGHT AND SCALE VALUES     */
   /* There is no true "source buffer size" in Redcloud.  Instead, the VG module  */
   /* provides video data as needed on a per-line basis.  The source buffer size  */
   /* is always assumed to equal the amount of required video data.  The returned */
   /* height is equal to the height of the required video buffer data (before all */
   /* scaling.)                                                                   */

   scale = (READ_VID32(RCDF_VIDEO_SCALE) >> 16) & 0x3FFF;
   height = ((READ_VID32(RCDF_VIDEO_Y_POS) >> 16) & 0x7FF) -
	 (READ_VID32(RCDF_VIDEO_Y_POS) & 0x7FF);
   delta = gfx_get_video_downscale_delta();
   down_enable = gfx_get_video_vertical_downscale_enable();

   /* REVERSE UPSCALING */

   if (height)
      height = ((scale * (height - 1l)) / 0x2000l) + 2l;

   /* REVERSE DOWNSCALING */
   /* Original lines = height * (0x3FFF + delta) / 0x3FFF */
   /* As this may cause rounding errors, we add 1 to the  */
   /* returned source size.  The return value of this     */
   /* function could thus be off by 1.                    */

   if (down_enable && height)
      height = ((height * (0x3FFFl + delta)) / 0x3FFFl) + 1;

   return ((height << 16) | width);
}

/*-----------------------------------------------------------------------------
 * gfx_get_video_line_size
 *
 * This routine returns the line size of the source video overlay buffer, in
 * pixels.
 *-----------------------------------------------------------------------------
 */
#if GFX_VIDEO_DYNAMIC
unsigned long
redcloud_get_video_line_size(void)
#else
unsigned long
gfx_get_video_line_size(void)
#endif
{
   unsigned long width = 0;

   /* DETERMINE SOURCE WIDTH FROM THE RCDF VIDEO LINE SIZE */

   width = (READ_VID32(RCDF_VIDEO_CONFIG) >> 7) & 0x000001FE;
   if (READ_VID32(RCDF_VIDEO_CONFIG) & RCDF_VCFG_LINE_SIZE_UPPER)
      width += 512l;
   return (width);
}

/*-----------------------------------------------------------------------------
 * gfx_get_video_xclip
 *
 * This routine returns the number of bytes clipped on the left side of a
 * video overlay line (skipped at beginning).
 *-----------------------------------------------------------------------------
 */
#if GFX_VIDEO_DYNAMIC
unsigned long
redcloud_get_video_xclip(void)
#else
unsigned long
gfx_get_video_xclip(void)
#endif
{
   unsigned long clip = 0;

   /* DETERMINE SOURCE WIDTH FROM THE RCDF VIDEO LINE SIZE */

   clip = (READ_VID32(RCDF_VIDEO_CONFIG) >> 14) & 0x000007FC;
   return (clip);
}

/*-----------------------------------------------------------------------------
 * gfx_get_video_offset
 *
 * This routine returns the current offset for the video overlay buffer.
 *-----------------------------------------------------------------------------
 */
#if GFX_VIDEO_DYNAMIC
unsigned long
redcloud_get_video_offset(void)
#else
unsigned long
gfx_get_video_offset(void)
#endif
{
   return (gfx_get_display_video_offset());
}

/*-----------------------------------------------------------------------------
 * gfx_get_video_yuv_offsets
 *
 * This routine returns the current offsets for the video overlay buffer when in 4:2:0.
 *-----------------------------------------------------------------------------
 */
#if GFX_VIDEO_DYNAMIC
void
redcloud_get_video_yuv_offsets(unsigned long *yoffset, unsigned long *uoffset,
			       unsigned long *voffset)
#else
void
gfx_get_video_yuv_offsets(unsigned long *yoffset, unsigned long *uoffset,
			  unsigned long *voffset)
#endif
{
   gfx_get_display_video_yuv_offsets(yoffset, uoffset, voffset);
}

/*-----------------------------------------------------------------------------
 * gfx_get_video_yuv_pitch
 *
 * This routine returns the current pitch values for the video overlay buffer.
 *-----------------------------------------------------------------------------
 */
#if GFX_VIDEO_DYNAMIC
void
redcloud_get_video_yuv_pitch(unsigned long *ypitch, unsigned long *uvpitch)
#else
void
gfx_get_video_yuv_pitch(unsigned long *ypitch, unsigned long *uvpitch)
#endif
{
   gfx_get_display_video_yuv_pitch(ypitch, uvpitch);
}

/*---------------------------------------------------------------------------
 * gfx_get_video_scale
 * 
 * This routine returns the scale factor for the video overlay window.
 *---------------------------------------------------------------------------
 */
#if GFX_VIDEO_DYNAMIC
unsigned long
redcloud_get_video_scale(void)
#else
unsigned long
gfx_get_video_scale(void)
#endif
{
   return (READ_VID32(RCDF_VIDEO_SCALE));
}

/*---------------------------------------------------------------------------
 * gfx_get_video_downscale_delta
 * 
 * This routine returns the vertical downscale factor for the video overlay window.
 *---------------------------------------------------------------------------
 */
#if GFX_VIDEO_DYNAMIC
unsigned long
redcloud_get_video_downscale_delta(void)
#else
unsigned long
gfx_get_video_downscale_delta(void)
#endif
{
   /* USE PRIVATE ROUTINE TO ABSTRACT THE DIPSLAY CONTROLLER */

   return (gfx_get_display_video_downscale_delta());
}

/*---------------------------------------------------------------------------
 * gfx_get_video_vertical_downscale_enable
 * 
 * This routine returns the vertical downscale enable for the video overlay window.
 *---------------------------------------------------------------------------
 */
#if GFX_VIDEO_DYNAMIC
int
redcloud_get_video_vertical_downscale_enable(void)
#else
int
gfx_get_video_vertical_downscale_enable(void)
#endif
{
   /* USE PRIVATE ROUTINE TO ABSTRACT THE DIPSLAY CONTROLLER */

   return (gfx_get_display_video_downscale_enable());
}

/*---------------------------------------------------------------------------
 * gfx_get_video_downscale_config
 * 
 * This routine returns the current type and value of video downscaling.
 *---------------------------------------------------------------------------
 */
#if GFX_VIDEO_DYNAMIC
int
redcloud_get_video_downscale_config(unsigned short *type, unsigned short *m)
#else
int
gfx_get_video_downscale_config(unsigned short *type, unsigned short *m)
#endif
{
   unsigned long downscale;

   downscale = READ_VID32(RCDF_VIDEO_DOWNSCALER_CONTROL);
   *m = (unsigned short)((downscale & RCDF_VIDEO_DOWNSCALE_FACTOR_MASK) >>
			 RCDF_VIDEO_DOWNSCALE_FACTOR_POS) + 1;

   switch (downscale & RCDF_VIDEO_DOWNSCALE_TYPE_MASK) {
   case RCDF_VIDEO_DOWNSCALE_TYPE_A:
      *type = VIDEO_DOWNSCALE_KEEP_1_OF;
      break;
   case RCDF_VIDEO_DOWNSCALE_TYPE_B:
      *type = VIDEO_DOWNSCALE_DROP_1_OF;
      break;
   default:
      return GFX_STATUS_ERROR;
      break;
   }
   return (0);
}

/*---------------------------------------------------------------------------
 * gfx_get_video_downscale_coefficients
 * 
 * This routine returns the current video downscaling coefficients.
 *---------------------------------------------------------------------------
 */
#if GFX_VIDEO_DYNAMIC
void
redcloud_get_video_downscale_coefficients(unsigned short *coef1,
					  unsigned short *coef2,
					  unsigned short *coef3,
					  unsigned short *coef4)
#else
void
gfx_get_video_downscale_coefficients(unsigned short *coef1,
				     unsigned short *coef2,
				     unsigned short *coef3,
				     unsigned short *coef4)
#endif
{
   unsigned long coef;

   coef = READ_VID32(RCDF_VIDEO_DOWNSCALER_COEFFICIENTS);
   *coef1 =
	 (unsigned short)((coef >> RCDF_VIDEO_DOWNSCALER_COEF1_POS) &
			  RCDF_VIDEO_DOWNSCALER_COEF_MASK);
   *coef2 =
	 (unsigned short)((coef >> RCDF_VIDEO_DOWNSCALER_COEF2_POS) &
			  RCDF_VIDEO_DOWNSCALER_COEF_MASK);
   *coef3 =
	 (unsigned short)((coef >> RCDF_VIDEO_DOWNSCALER_COEF3_POS) &
			  RCDF_VIDEO_DOWNSCALER_COEF_MASK);
   *coef4 =
	 (unsigned short)((coef >> RCDF_VIDEO_DOWNSCALER_COEF4_POS) &
			  RCDF_VIDEO_DOWNSCALER_COEF_MASK);
   return;
}

/*---------------------------------------------------------------------------
 * gfx_get_video_downscale_enable
 * 
 * This routine returns 1 if video downscaling is currently enabled,
 * or 0 if it is currently disabled.
 *---------------------------------------------------------------------------
 */
#if GFX_VIDEO_DYNAMIC
void
redcloud_get_video_downscale_enable(int *enable)
#else
void
gfx_get_video_downscale_enable(int *enable)
#endif
{
   if (READ_VID32(RCDF_VIDEO_DOWNSCALER_CONTROL) &
       RCDF_VIDEO_DOWNSCALE_ENABLE)
      *enable = 1;
   else
      *enable = 0;
   return;
}

/*---------------------------------------------------------------------------
 * gfx_get_video_dst_size
 * 
 * This routine returns the size of the displayed video overlay window.
 *---------------------------------------------------------------------------
 */
#if GFX_VIDEO_DYNAMIC
unsigned long
redcloud_get_video_dst_size(void)
#else
unsigned long
gfx_get_video_dst_size(void)
#endif
{
   unsigned long xsize, ysize;

   xsize = READ_VID32(RCDF_VIDEO_X_POS);
   xsize = ((xsize >> 16) & 0x7FF) - (xsize & 0x7FF);
   ysize = READ_VID32(RCDF_VIDEO_Y_POS);
   ysize = ((ysize >> 16) & 0x7FF) - (ysize & 0x7FF);
   return ((ysize << 16) | xsize);
}

/*---------------------------------------------------------------------------
 * gfx_get_video_position
 * 
 * This routine returns the position of the video overlay window.  The
 * return value is (ypos << 16) | xpos.
 *---------------------------------------------------------------------------
 */
#if GFX_VIDEO_DYNAMIC
unsigned long
redcloud_get_video_position(void)
#else
unsigned long
gfx_get_video_position(void)
#endif
{
   unsigned long hadjust, vadjust;
   unsigned long xpos, ypos;

   /* READ HARDWARE POSITION */

   xpos = READ_VID32(RCDF_VIDEO_X_POS) & 0x000007FF;
   ypos = READ_VID32(RCDF_VIDEO_Y_POS) & 0x000007FF;

   /* GET ADJUSTMENT VALUES */
   /* Use routines to abstract version of display controller. */

   hadjust =
	 (unsigned long)gfx_get_htotal() -
	 (unsigned long)gfx_get_hsync_end() - 14l;
   vadjust =
	 (unsigned long)gfx_get_vtotal() -
	 (unsigned long)gfx_get_vsync_end() + 1l;
   xpos -= hadjust;
   ypos -= vadjust;
   return ((ypos << 16) | (xpos & 0x0000FFFF));
}

/*---------------------------------------------------------------------------
 * gfx_get_video_color_key
 * 
 * This routine returns the current video color key value.
 *---------------------------------------------------------------------------
 */
#if GFX_VIDEO_DYNAMIC
unsigned long
redcloud_get_video_color_key(void)
#else
unsigned long
gfx_get_video_color_key(void)
#endif
{
   return (READ_VID32(RCDF_VIDEO_COLOR_KEY));
}

/*---------------------------------------------------------------------------
 * gfx_get_video_color_key_mask
 * 
 * This routine returns the current video color mask value.
 *---------------------------------------------------------------------------
 */
#if GFX_VIDEO_DYNAMIC
unsigned long
redcloud_get_video_color_key_mask(void)
#else
unsigned long
gfx_get_video_color_key_mask(void)
#endif
{
   return (READ_VID32(RCDF_VIDEO_COLOR_MASK));
}

/*---------------------------------------------------------------------------
 * gfx_get_video_color_key_src
 * 
 * This routine returns 0 for video data compare, 1 for graphics data.
 *---------------------------------------------------------------------------
 */
#if GFX_VIDEO_DYNAMIC
int
redcloud_get_video_color_key_src(void)
#else
int
gfx_get_video_color_key_src(void)
#endif
{
   if (READ_VID32(RCDF_DISPLAY_CONFIG) & RCDF_DCFG_VG_CK)
      return (0);
   return (1);
}

/*---------------------------------------------------------------------------
 * gfx_get_video_filter
 * 
 * This routine returns if the filters are currently enabled.
 *---------------------------------------------------------------------------
 */
#if GFX_VIDEO_DYNAMIC
int
redcloud_get_video_filter(void)
#else
int
gfx_get_video_filter(void)
#endif
{
   int retval = 0;

   if (READ_VID32(RCDF_VIDEO_CONFIG) & RCDF_VCFG_X_FILTER_EN)
      retval |= 1;
   if (READ_VID32(RCDF_VIDEO_CONFIG) & RCDF_VCFG_Y_FILTER_EN)
      retval |= 2;
   return (retval);
}

/*---------------------------------------------------------------------------
 * gfx_get_video_request
 * 
 * This routine returns the horizontal (pixel) and vertical (lines) video
 * request values.
 *---------------------------------------------------------------------------
 */
#if GFX_VIDEO_DYNAMIC
int
redcloud_get_video_request(short *x, short *y)
#else
int
gfx_get_video_request(short *x, short *y)
#endif
{
   unsigned long request = 0;

   request = (READ_VID32(RCDF_VIDEO_REQUEST));
   *x = (short)((request >> RCDF_VIDEO_X_REQUEST_POS) &
		RCDF_VIDEO_REQUEST_MASK);
   *y = (short)((request >> RCDF_VIDEO_Y_REQUEST_POS) &
		RCDF_VIDEO_REQUEST_MASK);

   *x -= gfx_get_htotal() - gfx_get_hsync_end() - 2;
   *y -= gfx_get_vtotal() - gfx_get_vsync_end() + 1;

   return (0);
}

/*---------------------------------------------------------------------------
 * gfx_get_video_cursor()
 * 
 * This routine configures the video hardware cursor.
 * If the "mask"ed bits in the graphics pixel match "key", then either "color1"
 * or "color2" will be used for this pixel, according to the value of the bit
 * in offset "select_color2".
 *---------------------------------------------------------------------------
 */
#if GFX_VIDEO_DYNAMIC
int
redcloud_get_video_cursor(unsigned long *key, unsigned long *mask,
			  unsigned short *select_color2,
			  unsigned long *color1, unsigned short *color2)
#else
int
gfx_get_video_cursor(unsigned long *key, unsigned long *mask,
		     unsigned short *select_color2, unsigned long *color1,
		     unsigned short *color2)
#endif
{
   *select_color2 =
	 (unsigned short)(READ_VID32(RCDF_CURSOR_COLOR_KEY) >>
			  RCDF_CURSOR_COLOR_KEY_OFFSET_POS);
   *key = READ_VID32(RCDF_CURSOR_COLOR_KEY) & RCDF_COLOR_MASK;
   *mask = READ_VID32(RCDF_CURSOR_COLOR_MASK) & RCDF_COLOR_MASK;
   *color1 = READ_VID32(RCDF_CURSOR_COLOR_1) & RCDF_COLOR_MASK;
   *color2 =
	 (unsigned short)(READ_VID32(RCDF_CURSOR_COLOR_2) & RCDF_COLOR_MASK);
   return (0);
}

/*---------------------------------------------------------------------------
 * gfx_read_crc
 *
 * This routine returns the hardware CRC value, which is used for automated 
 * testing.  The value is like a checksum, but will change if pixels move
 * locations.
 *---------------------------------------------------------------------------
 */
#if GFX_VIDEO_DYNAMIC
unsigned long
redcloud_read_crc(void)
#else
unsigned long
gfx_read_crc(void)
#endif
{
   Q_WORD msr_value;
   unsigned long crc = 0xFFFFFFFF;

   /* DISABLE 32-BIT CRCS */
   /* For GX1.x, this is a reserved bit, and is assumed to be a benign access */

   gfx_msr_read(RC_ID_DF, RCDF_MBD_MSR_DIAG_DF, &msr_value);
   msr_value.low &= ~RCDF_DIAG_32BIT_CRC;
   gfx_msr_write(RC_ID_DF, RCDF_MBD_MSR_DIAG_DF, &msr_value);

   if (gfx_test_timing_active()) {
      /* WAIT UNTIL ACTIVE DISPLAY */

      while (!gfx_test_vertical_active()) ;

      /* RESET CRC DURING ACTIVE DISPLAY */

      WRITE_VID32(RCDF_VID_CRC, 0);
      WRITE_VID32(RCDF_VID_CRC, 1);

      /* WAIT UNTIL NOT ACTIVE, THEN ACTIVE, NOT ACTIVE, THEN ACTIVE */

      while (!gfx_test_vertical_active()) ;
      while (gfx_test_vertical_active()) ;
      while (!gfx_test_vertical_active()) ;
      while (gfx_test_vertical_active()) ;
      while (!gfx_test_vertical_active()) ;
      crc = READ_VID32(RCDF_VID_CRC) >> 8;
   }
   return (crc);
}

/*---------------------------------------------------------------------------
 * gfx_read_crc32
 *
 * This routine returns the 32-bit hardware CRC value, which is used for automated 
 * testing.  The value is like a checksum, but will change if pixels move
 * locations.
 *---------------------------------------------------------------------------
 */
#if GFX_VIDEO_DYNAMIC
unsigned long
redcloud_read_crc32(void)
#else
unsigned long
gfx_read_crc32(void)
#endif
{
   Q_WORD msr_value;
   unsigned long crc = 0xFFFFFFFF;

   /* ENABLE 32-BIT CRCS */
   /* For GX1.x, this is a reserved bit, and is assumed to be a benign access */

   gfx_msr_read(RC_ID_DF, RCDF_MBD_MSR_DIAG_DF, &msr_value);
   msr_value.low |= RCDF_DIAG_32BIT_CRC;
   gfx_msr_write(RC_ID_DF, RCDF_MBD_MSR_DIAG_DF, &msr_value);

   if (gfx_test_timing_active()) {
      /* WAIT UNTIL ACTIVE DISPLAY */

      while (!gfx_test_vertical_active()) ;

      /* RESET CRC DURING ACTIVE DISPLAY */

      WRITE_VID32(RCDF_VID_CRC, 0);
      WRITE_VID32(RCDF_VID_CRC, 1);

      /* WAIT UNTIL NOT ACTIVE, THEN ACTIVE, NOT ACTIVE, THEN ACTIVE */

      while (!gfx_test_vertical_active()) ;
      while (gfx_test_vertical_active()) ;
      while (!gfx_test_vertical_active()) ;
      while (gfx_test_vertical_active()) ;
      while (!gfx_test_vertical_active()) ;
      crc = READ_VID32(RCDF_VID_CRC32);
   }
   return (crc);
}

/*---------------------------------------------------------------------------
 * gfx_read_window_crc
 *
 * This routine returns the hardware CRC value for a subsection of the display.
 * This value is used to debug whole-screen CRC failures.
 *---------------------------------------------------------------------------
 */
#if GFX_VIDEO_DYNAMIC
unsigned long
redcloud_read_window_crc(int source, unsigned short x, unsigned short y,
			 unsigned short width, unsigned short height,
			 int crc32)
#else
unsigned long
gfx_read_window_crc(int source, unsigned short x, unsigned short y,
		    unsigned short width, unsigned short height, int crc32)
#endif
{
   Q_WORD msr_value;
   unsigned long xpos, ypos, crc = 0;
   unsigned long old_fmt = 0;
   unsigned int vsync_active_base, vsync_inactive_base, hsync_active_base;
   unsigned int vsync_active_shift, vsync_inactive_shift, hsync_active_shift;
   unsigned int vsync_bit, hsync_bit, sync_polarities = 0;

   /* CONFIGURE DISPLAY FILTER TO LOAD DATA ONTO LOWER 32-BITS */

   msr_value.high = 0;
   msr_value.low =
	 (source == CRC_SOURCE_GFX_DATA) ? (RCDF_MBD_DIAG_EN0 | 0x0000000F)
	 : (RCDF_MBD_DIAG_EN0 | 0x0000000B);
   gfx_msr_write(RC_ID_DF, MBD_MSR_DIAG, &msr_value);

   /* CONFIGURE DISPLAY FILTER FOR APPROPRIATE OUTPUT */

   if (source != CRC_SOURCE_GFX_DATA) {
      gfx_msr_read(RC_ID_DF, MBD_MSR_CONFIG, &msr_value);
      old_fmt = msr_value.low;
      msr_value.low &= ~(RCDF_CONFIG_FMT_MASK);
      msr_value.low |= ((source == CRC_SOURCE_FP_DATA) ? RCDF_CONFIG_FMT_FP :
			RCDF_CONFIG_FMT_CRT);
      gfx_msr_write(RC_ID_DF, MBD_MSR_CONFIG, &msr_value);
   }

   /* CONFIGURE MCP TO LOAD REGB DATA ONTO UPPER 32-BITS */

   msr_value.low = MCP_MBD_DIAG_EN1 | 0x00050000;
   gfx_msr_write(RC_ID_MCP, MBD_MSR_DIAG, &msr_value);

   /* ENABLE HW CLOCK GATING AND SET MCP CLOCK TO DOT CLOCK */

   msr_value.low = 1l;
   gfx_msr_write(RC_ID_MCP, MBD_MSR_PM, &msr_value);
   msr_value.low = 0;
   gfx_msr_write(RC_ID_MCP, MCP_DBGCLKCTL, &msr_value);
   msr_value.low = 3;
   gfx_msr_write(RC_ID_MCP, MCP_DBGCLKCTL, &msr_value);

   /* DISABLE MCP ACTIONS */

   msr_value.high = 0x00000000;
   msr_value.low = 0x00000000;
   gfx_msr_write(RC_ID_MCP, MCP_DIAGCTL, &msr_value);

   /* SET APPROPRIATE BASE ADDRESS */
   /* M-Sets use normal diag bits, while N-Sets use inverted diag bits */
   /* We thus use the M-sets when polling for a high signal and the N  */
   /* sets when polling for a low signal.                              */

   if (source != CRC_SOURCE_GFX_DATA) {
      sync_polarities = gfx_get_sync_polarities();
      vsync_bit = 29;
      hsync_bit = 30;
   } else {
      vsync_bit = 25;
      hsync_bit = 26;
   }

   if (sync_polarities & 1) {
      hsync_active_base = MCP_SETM0CTL;
      hsync_active_shift = 2;
   } else {
      hsync_active_base = MCP_SETN0CTL;
      hsync_active_shift = 1;
   }
   if (sync_polarities & 2) {
      vsync_active_base = MCP_SETM0CTL;
      vsync_inactive_base = MCP_SETN0CTL;
      vsync_active_shift = 2;
      vsync_inactive_shift = 1;
   } else {
      vsync_active_base = MCP_SETN0CTL;
      vsync_inactive_base = MCP_SETM0CTL;
      vsync_active_shift = 1;
      vsync_inactive_shift = 2;
   }

   /* SET STATE TRANSITIONS          */

   /* STATE 0-1 TRANSITION (SET 0)      */
   /* XState = 00 and VSync Inactive    */
   /* Note: DF VSync = Diag Bus Bit 29  */
   /*       VG VSync = Diag Bus Bit 25  */

   msr_value.low = 0x000000A0;
   msr_value.high = 0x00008000 | ((unsigned long)vsync_bit << 16) |
	 ((unsigned long)vsync_bit << 21) | ((unsigned long)vsync_bit << 26);
   gfx_msr_write(RC_ID_MCP, vsync_inactive_base, &msr_value);

   /* STATE 1-2 TRANSITION (SET 4)   */
   /* XState = 01 and VSync Active   */

   msr_value.low = 0x000000C0;
   gfx_msr_write(RC_ID_MCP, vsync_active_base + 4, &msr_value);

   /* STATE 2-3 TRANSITION (SET 1)   */
   /* XState = 10 and VSync Inactive */

   msr_value.low = 0x00000120;
   gfx_msr_write(RC_ID_MCP, vsync_inactive_base + 1, &msr_value);

   /* HORIZONTAL COUNTER (SET 5)         */
   /* XState = 10 and HSync Active       */
   /* Notes: DF HSync = Diag Bus Bit 30  */
   /*        VG HSync = Diag Bus Bit 26  */

   msr_value.high = 0x00008000 | ((unsigned long)hsync_bit << 16) |
	 ((unsigned long)hsync_bit << 21) | ((unsigned long)hsync_bit << 26);
   msr_value.low = 0x00000120;
   gfx_msr_write(RC_ID_MCP, hsync_active_base + 5, &msr_value);

   /* HORIZONTAL COUNTER RESET (SET 4)     */
   /* XState = 10 and H. Counter = limit   */
   /* Note: H. Counter is lower 16-bits of */
   /*       RegB.                          */

   msr_value.high = 0x00000000;
   msr_value.low = 0x00000128;
   gfx_msr_write(RC_ID_MCP, vsync_inactive_base + 4, &msr_value);

   /* CRC TRIGGER (SET 0)   */
   /* Cmp0 <= xpos <  Cmp1  */
   /* Cmp2 <= ypos <  Cmp2  */

   msr_value.high = 0x00000000;
   msr_value.low = 0x10C20120;
   gfx_msr_write(RC_ID_MCP, vsync_active_base, &msr_value);

   /* SET COMPARATOR VALUES */
   /* Note: The VG data outputs from the DF are delayed by one pixel clock. */
   /*       In this mode, we thus add one to horizontal comparator limits.  */

   /* COMPARATOR 0                                        */
   /* Lower limit = xpos + (h_blank_pixels - 1) - 3       */
   /* Notes:                                              */
   /*   1. 3 is the pipeline delay for MCP register       */
   /*      data to access the diag bus                    */
   /*   2. h_blank_pixels = HTOTAL - HSYNC_END            */

   xpos = (unsigned long)x + ((unsigned long)gfx_get_htotal() -
			      (unsigned long)gfx_get_hsync_end() - 1l) - 3l;
   if (source == CRC_SOURCE_GFX_DATA)
      xpos++;
   msr_value.high = 0x00000000;
   msr_value.low = xpos;
   gfx_msr_write(RC_ID_MCP, MCP_CMPVAL0, &msr_value);

   /* COMPARATOR 1                                          */
   /* Upper limit = xpos + width + (h_blank_pixels - 1) - 3 */

   msr_value.low = xpos + (unsigned long)width;
   gfx_msr_write(RC_ID_MCP, MCP_CMPVAL0 + 2, &msr_value);

   /* COMPARATOR 2                                  */
   /* Lower limit = ypos + v_blank_pixels           */
   /* Notes:                                        */
   /*   1. v_blank_pixels = VTOTAL - VSYNC_END      */

   ypos = (unsigned long)y + (unsigned long)gfx_get_vtotal() -
	 (unsigned long)gfx_get_vsync_end();
   msr_value.low = ypos << 16;
   gfx_msr_write(RC_ID_MCP, MCP_CMPVAL0 + 4, &msr_value);

   /* COMPARATOR 3                                  */
   /* Upper limit = ypos + height + v_blank_pixels  */

   msr_value.low = (ypos + (unsigned long)height) << 16;
   gfx_msr_write(RC_ID_MCP, MCP_CMPVAL0 + 6, &msr_value);

   /* SET COMPARATOR MASKS */

   /* COMPARATORS 0 AND 1 REFER TO LOWER 16 BITS OF REGB */

   msr_value.high = 0x00000000;
   msr_value.low = 0x0000FFFF;
   gfx_msr_write(RC_ID_MCP, MCP_CMPMASK0, &msr_value);
   gfx_msr_write(RC_ID_MCP, MCP_CMPMASK0 + 2, &msr_value);

   /* COMPARATORS 2 AND 3 REFER TO UPPER 16 BITS OF REGB */

   msr_value.low = 0xFFFF0000;
   gfx_msr_write(RC_ID_MCP, MCP_CMPMASK0 + 4, &msr_value);
   gfx_msr_write(RC_ID_MCP, MCP_CMPMASK0 + 6, &msr_value);

   /* SET REGA MASK TO CRC ONLY 24 BITS OF DATA */

   msr_value.high = 0x00000000;
   msr_value.low = 0x00FFFFFF;
   gfx_msr_write(RC_ID_MCP, MCP_REGAMASK, &msr_value);

   /* SET REGB VALUE */
   /* Lower 16 bits use HTOTAL - SYNC TIME - 1 to set the counter rollover limit. */
   /* Upper 16 bits use 0xFFFF to remove auto-clear behavior.     */

   msr_value.high = 0x00000000;
   msr_value.low = 0xFFFF0000 |
	 ((gfx_get_htotal() - (gfx_get_hsync_end() - gfx_get_hsync_start()) -
	   1) & 0xFFFF);
   gfx_msr_write(RC_ID_MCP, MCP_REGBVAL, &msr_value);

   /* PROGRAM ACTIONS */

   /* GOTO STATE 01 */

   msr_value.high = 0x00000000;
   msr_value.low = 0x00000008 | (1l << vsync_inactive_shift);
   gfx_msr_write(RC_ID_MCP, MCP_ACTION0 + 14, &msr_value);

   /* GOTO STATE 10 */

   msr_value.low = 0x00080000 | (1l << (vsync_active_shift + 16));
   gfx_msr_write(RC_ID_MCP, MCP_ACTION0 + 15, &msr_value);

   /* GOTO STATE 11 */

   msr_value.low = 0x00000080 | (1l << (vsync_inactive_shift + 4));
   gfx_msr_write(RC_ID_MCP, MCP_ACTION0 + 16, &msr_value);

   /* CLEAR REGB (COUNTERS)  */
   /* RegB is cleared upon transitioning to state 10              */
   /* RegA is not cleared as the initial value must be 0x00000001 */

   msr_value.low = 0x00080000 | (1l << (vsync_active_shift + 16));
   gfx_msr_write(RC_ID_MCP, MCP_ACTION0, &msr_value);

   /* CRC INTO REGA        */
   /* INCREMENT H. COUNTER */
   /* cmp0 <= xpos < cmp1  */
   /* cmp2 <= ypos < cmp3  */
   /* XState = 10          */

   msr_value.low = 0x00000008 | (1l << vsync_active_shift) |
	 0x00800000 | (1l << (hsync_active_shift + 20));
   gfx_msr_write(RC_ID_MCP, MCP_ACTION0 + 1, &msr_value);

   /* INCREMENT V. COUNTER */
   /* V. Counter is incremented when the H. Counter */
   /* rolls over.                                   */

   msr_value.low = 0x00080000 | (1l << (vsync_inactive_shift + 16));
   gfx_msr_write(RC_ID_MCP, MCP_ACTION0 + 2, &msr_value);

   /* CLEAR ALL OTHER ACTIONS */
   /* This prevents side-effects from previous accesses to the MCP */
   /* debug logic.                                                 */
   msr_value.low = 0x00000000;
   msr_value.high = 0x00000000;
   gfx_msr_write(RC_ID_MCP, MCP_ACTION0 + 3, &msr_value);
   gfx_msr_write(RC_ID_MCP, MCP_ACTION0 + 4, &msr_value);
   gfx_msr_write(RC_ID_MCP, MCP_ACTION0 + 5, &msr_value);
   gfx_msr_write(RC_ID_MCP, MCP_ACTION0 + 6, &msr_value);
   gfx_msr_write(RC_ID_MCP, MCP_ACTION0 + 7, &msr_value);
   gfx_msr_write(RC_ID_MCP, MCP_ACTION0 + 8, &msr_value);
   gfx_msr_write(RC_ID_MCP, MCP_ACTION0 + 9, &msr_value);
   gfx_msr_write(RC_ID_MCP, MCP_ACTION0 + 10, &msr_value);
   gfx_msr_write(RC_ID_MCP, MCP_ACTION0 + 11, &msr_value);
   gfx_msr_write(RC_ID_MCP, MCP_ACTION0 + 12, &msr_value);
   gfx_msr_write(RC_ID_MCP, MCP_ACTION0 + 13, &msr_value);
   gfx_msr_write(RC_ID_MCP, MCP_ACTION0 + 17, &msr_value);
   gfx_msr_write(RC_ID_MCP, MCP_ACTION0 + 18, &msr_value);
   gfx_msr_write(RC_ID_MCP, MCP_ACTION0 + 19, &msr_value);
   gfx_msr_write(RC_ID_MCP, MCP_ACTION0 + 20, &msr_value);

   /* SET REGA CRC VALUE TO 1 OR 0 */

   if (!crc32)
      msr_value.low = 0x00000001;
   gfx_msr_write(RC_ID_MCP, MCP_REGA, &msr_value);

   /* SET XSTATE TO 0 */

   msr_value.low = 0;
   msr_value.high = 0;
   gfx_msr_write(RC_ID_MCP, MCP_XSTATE, &msr_value);

   /* CONFIGURE DIAG CONTROL */
   /* Set all four comparators to watch the upper diag bus.           */
   /* Set REGA action1 to legacy CRC or 32-bit CRC.                   */
   /* Set REGB action1 to increment lower 16 bits and clear at limit. */
   /* Set REGB action2 to increment upper 16 bits.                    */
   /* Enable all actions.                                             */

   if (crc32)
      msr_value.low = 0x9A820055;
   else
      msr_value.low = 0x9A840055;
   msr_value.high = 0x00000000;
   gfx_msr_write(RC_ID_MCP, MCP_DIAGCTL, &msr_value);

   /* DELAY TWO FRAMES */

   while (!gfx_test_vertical_active()) ;
   while (gfx_test_vertical_active()) ;
   while (!gfx_test_vertical_active()) ;
   while (gfx_test_vertical_active()) ;
   while (!gfx_test_vertical_active()) ;

   /* VERIFY THAT XSTATE = 11 */

   gfx_msr_read(RC_ID_MCP, MCP_XSTATE, &msr_value);
   if ((msr_value.low & 3) == 3) {
      gfx_msr_read(RC_ID_MCP, MCP_REGA, &msr_value);

      crc = msr_value.low;
      if (!crc32)
	 crc &= 0xFFFFFF;
   }

   /* DISABLE MCP AND DF DIAG BUS OUTPUTS */

   msr_value.low = 0x00000000;
   msr_value.high = 0x00000000;
   gfx_msr_write(RC_ID_DF, MBD_MSR_DIAG, &msr_value);
   gfx_msr_write(RC_ID_MCP, MBD_MSR_DIAG, &msr_value);

   /* DISABLE MCP ACTIONS */

   msr_value.high = 0x00000000;
   msr_value.low = 0x00000000;
   gfx_msr_write(RC_ID_MCP, MCP_DIAGCTL, &msr_value);

   /* RESTORE PREVIOUS OUTPUT FORMAT */

   if (source != CRC_SOURCE_GFX_DATA) {
      gfx_msr_read(RC_ID_DF, MBD_MSR_CONFIG, &msr_value);
      msr_value.low = old_fmt;
      gfx_msr_write(RC_ID_DF, MBD_MSR_CONFIG, &msr_value);
   }
   return crc;
}

/*---------------------------------------------------------------------------
 * gfx_get_alpha_enable
 * 
 * This routine returns 1 if the selected alpha window is currently 
 * enabled, or 0 if it is currently disabled.
 *---------------------------------------------------------------------------
 */
#if GFX_VIDEO_DYNAMIC
void
redcloud_get_alpha_enable(int *enable)
#else
void
gfx_get_alpha_enable(int *enable)
#endif
{
   unsigned long value = 0;

   *enable = 0;
   if (gfx_alpha_select <= 2) {
      value =
	    READ_VID32(RCDF_ALPHA_CONTROL_1 +
		       ((unsigned long)gfx_alpha_select << 5));
      if (value & RCDF_ACTRL_WIN_ENABLE)
	 *enable = 1;
   }
   return;
}

/*---------------------------------------------------------------------------
 * gfx_get_alpha_size
 * 
 * This routine returns the size of the currently selected alpha region.
 *---------------------------------------------------------------------------
 */
#if GFX_VIDEO_DYNAMIC
void
redcloud_get_alpha_size(unsigned short *x, unsigned short *y,
			unsigned short *width, unsigned short *height)
#else
void
gfx_get_alpha_size(unsigned short *x, unsigned short *y,
		   unsigned short *width, unsigned short *height)
#endif
{
   unsigned long value = 0;

   *x = 0;
   *y = 0;
   *width = 0;
   *height = 0;
   if (gfx_alpha_select <= 2) {
      value =
	    READ_VID32(RCDF_ALPHA_XPOS_1 +
		       ((unsigned long)gfx_alpha_select << 5));
      *x = (unsigned short)(value & 0x000007FF);
      *width = (unsigned short)((value >> 16) & 0x000007FF) - *x;
      value =
	    READ_VID32(RCDF_ALPHA_YPOS_1 +
		       ((unsigned long)gfx_alpha_select << 5));
      *y = (unsigned short)(value & 0x000007FF);
      *height = (unsigned short)((value >> 16) & 0x000007FF) - *y;
   }
   *x -= gfx_get_htotal() - gfx_get_hsync_end() - 2;
   *y -= gfx_get_vtotal() - gfx_get_vsync_end() + 1;
   return;
}

/*---------------------------------------------------------------------------
 * gfx_get_alpha_value
 * 
 * This routine returns the alpha value and increment/decrement value of 
 * the currently selected alpha region.
 *---------------------------------------------------------------------------
 */
#if GFX_VIDEO_DYNAMIC
void
redcloud_get_alpha_value(unsigned char *alpha, char *delta)
#else
void
gfx_get_alpha_value(unsigned char *alpha, char *delta)
#endif
{
   unsigned long value = 0;

   *alpha = 0;
   *delta = 0;
   if (gfx_alpha_select <= 2) {
      value =
	    READ_VID32(RCDF_ALPHA_CONTROL_1 +
		       ((unsigned long)gfx_alpha_select << 5));
      *alpha = (unsigned char)(value & 0x00FF);
      *delta = (char)((value >> 8) & 0x00FF);
   }
   return;
}

/*---------------------------------------------------------------------------
 * gfx_get_alpha_priority
 * 
 * This routine returns the priority of the currently selected alpha region.
 *---------------------------------------------------------------------------
 */
#if GFX_VIDEO_DYNAMIC
void
redcloud_get_alpha_priority(int *priority)
#else
void
gfx_get_alpha_priority(int *priority)
#endif
{
   unsigned long pos = 0, value = 0;

   *priority = 0;
   if (gfx_alpha_select <= 2) {
      value = READ_VID32(RCDF_VID_ALPHA_CONTROL);
      pos = 16 + (gfx_alpha_select << 1);
      *priority = (int)((value >> pos) & 3);
   }
   return;
}

/*---------------------------------------------------------------------------
 * gfx_get_alpha_color
 * 
 * This routine returns the color register value for the currently selected 
 * alpha region.  Bit 24 is set if the color register is enabled.
 *---------------------------------------------------------------------------
 */
#if GFX_VIDEO_DYNAMIC
void
redcloud_get_alpha_color(unsigned long *color)
#else
void
gfx_get_alpha_color(unsigned long *color)
#endif
{
   *color = 0;
   if (gfx_alpha_select <= 2) {
      *color =
	    READ_VID32(RCDF_ALPHA_COLOR_1 +
		       ((unsigned long)gfx_alpha_select << 5));
   }
   return;
}

#endif /* GFX_READ_ROUTINES */

/* END OF FILE */

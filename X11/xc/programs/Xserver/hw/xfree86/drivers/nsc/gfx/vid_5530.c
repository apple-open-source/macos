/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/nsc/gfx/vid_5530.c,v 1.2 2003/02/21 16:51:10 alanh Exp $ */
/*
 * $Workfile: vid_5530.c $
 *
 * This file contains routines to control the CS5530 video overlay hardware.
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

/*----------------------------------------------------------------------------
 * CS5530 PLL TABLE
 *----------------------------------------------------------------------------
 */
typedef struct tagCS5530PLLENTRY
{
   long frequency;			/* 16.16 fixed point frequency */
   unsigned long pll_value;		/* associated register value */
}
CS5530PLLENTRY;

CS5530PLLENTRY CS5530_PLLtable[] = {
   {0x00192CCC, 0x31C45801,},		/*  25.1750 */
   {0x001C526E, 0x20E36802,},		/*  28.3220 */
   {0x001F8000, 0x33915801,},		/*  31.5000 */
   {0x00240000, 0x31EC4801,},		/*  36.0000 */
   {0x00258000, 0x21E22801,},		/*  37.5000 */
   {0x00280000, 0x33088801,},		/*  40.0000 */
   {0x002CE666, 0x33E22801,},		/*  44.9000 */
   {0x00318000, 0x336C4801,},		/*  49.5000 */
   {0x00320000, 0x23088801,},		/*  50.0000 */
   {0x00325999, 0x23088801,},		/*  50.3500 */
   {0x00360000, 0x3708A801,},		/*  54.0000 */
   {0x00384000, 0x23E36802,},		/*  56.2500 */
   {0x0038643F, 0x23E36802,},		/*  56.3916 */
   {0x0038A4DD, 0x23E36802,},		/*  56.6444 */
   {0x003B0000, 0x37C45801,},		/*  59.0000 */
   {0x003F0000, 0x23EC4801,},		/*  63.0000 */
   {0x00410000, 0x37911801,},		/*  65.0000 */
   {0x00438000, 0x37963803,},		/*  67.5000 */
   {0x0046CCCC, 0x37058803,},		/*  70.8000 */
   {0x00480000, 0x3710C805,},		/*  72.0000 */
   {0x004B0000, 0x37E22801,},		/*  75.0000 */
   {0x004EC000, 0x27915801,},		/*  78.7500 */
   {0x00500000, 0x37D8D802,},		/*  80.0000 */
   {0x0059CCCC, 0x27588802,},		/*  89.8000 */
   {0x005E8000, 0x27EC4802,},		/*  94.5000 */
   {0x00630000, 0x27AC6803,},		/*  99.0000 */
   {0x00640000, 0x27088801,},		/* 100.0000 */
   {0x006C0000, 0x2710C805,},		/* 108.0000 */
   {0x00708000, 0x27E36802,},		/* 112.5000 */
   {0x00820000, 0x27C58803,},		/* 130.0000 */
   {0x00870000, 0x27316803,},		/* 135.0000 */
   {0x009D8000, 0x2F915801,},		/* 157.5000 */
   {0x00A20000, 0x2F08A801,},		/* 162.0000 */
   {0x00AF0000, 0x2FB11802,},		/* 175.0000 */
   {0x00BD0000, 0x2FEC4802,},		/* 189.0000 */
   {0x00CA0000, 0x2F963803,},		/* 202.0000 */
   {0x00E80000, 0x2FB1B802,},		/* 232.0000 */
};

#define NUM_CS5530_FREQUENCIES sizeof(CS5530_PLLtable)/sizeof(CS5530PLLENTRY)

int cs5530_set_video_enable(int enable);
int cs5530_set_video_format(unsigned long format);
int cs5530_set_video_size(unsigned short width, unsigned short height);
int cs5530_set_video_yuv_pitch(unsigned long ypitch, unsigned long uvpitch);
int cs5530_set_video_offset(unsigned long offset);
int cs5530_set_video_yuv_offsets(unsigned long yoffset, unsigned long uoffset,
				 unsigned long voffset);
int cs5530_set_video_window(short x, short y, unsigned short w,
			    unsigned short h);
int cs5530_set_video_left_crop(unsigned short x);
int cs5530_set_video_upscale(unsigned short srcw, unsigned short srch,
			     unsigned short dstw, unsigned short dsth);
int cs5530_set_video_scale(unsigned short srcw, unsigned short srch,
			   unsigned short dstw, unsigned short dsth);
int cs5530_set_video_vertical_downscale(unsigned short srch,
					unsigned short dsth);
void cs5530_set_video_vertical_downscale_enable(int enable);
int cs5530_set_video_downscale_config(unsigned short type, unsigned short m);
int cs5530_set_video_color_key(unsigned long key, unsigned long mask,
			       int bluescreen);
int cs5530_set_video_filter(int xfilter, int yfilter);
int cs5530_set_video_palette(unsigned long *palette);
int cs5530_set_video_palette_entry(unsigned long index, unsigned long color);
int cs5530_set_video_downscale_coefficients(unsigned short coef1,
					    unsigned short coef2,
					    unsigned short coef3,
					    unsigned short coef4);
int cs5530_set_video_downscale_enable(int enable);
int cs5530_set_video_source(VideoSourceType source);
int cs5530_set_vbi_source(VbiSourceType source);
int cs5530_set_vbi_lines(unsigned long even, unsigned long odd);
int cs5530_set_vbi_total(unsigned long even, unsigned long odd);
int cs5530_set_video_interlaced(int enable);
int cs5530_set_color_space_YUV(int enable);
int cs5530_set_vertical_scaler_offset(char offset);
int cs5530_set_top_line_in_odd(int enable);
int cs5530_set_genlock_delay(unsigned long delay);
int cs5530_set_genlock_enable(int flags);
int cs5530_set_video_cursor(unsigned long key, unsigned long mask,
			    unsigned short select_color2,
			    unsigned long color1, unsigned long color2);
int cs5530_set_video_cursor_enable(int enable);
int cs5530_set_video_request(short x, short y);

int cs5530_select_alpha_region(int region);
int cs5530_set_alpha_enable(int enable);
int cs5530_set_alpha_window(short x, short y,
			    unsigned short width, unsigned short height);
int cs5530_set_alpha_value(unsigned char alpha, char delta);
int cs5530_set_alpha_priority(int priority);
int cs5530_set_alpha_color(unsigned long color);
int cs5530_set_alpha_color_enable(int enable);
int cs5530_set_no_ck_outside_alpha(int enable);
int cs5530_disable_softvga(void);
int cs5530_enable_softvga(void);
int cs5530_set_macrovision_enable(int enable);
int cs5530_set_crt_enable(int enable);
void cs5530_reset_video(void);
int cs5530_set_display_control(int sync_polarities);
void cs5530_set_clock_frequency(unsigned long frequency);

/* READ ROUTINES IN GFX_VID.C */

int cs5530_get_video_enable(void);
int cs5530_get_video_format(void);
unsigned long cs5530_get_video_src_size(void);
unsigned long cs5530_get_video_line_size(void);
unsigned long cs5530_get_video_xclip(void);
unsigned long cs5530_get_video_offset(void);
void cs5530_get_video_yuv_offsets(unsigned long *yoffset,
				  unsigned long *uoffset,
				  unsigned long *voffset);
void cs5530_get_video_yuv_pitch(unsigned long *ypitch,
				unsigned long *uvpitch);
unsigned long cs5530_get_video_upscale(void);
unsigned long cs5530_get_video_scale(void);
unsigned long cs5530_get_video_downscale_delta(void);
int cs5530_get_video_vertical_downscale_enable(void);
int cs5530_get_video_downscale_config(unsigned short *type,
				      unsigned short *m);
void cs5530_get_video_downscale_coefficients(unsigned short *coef1,
					     unsigned short *coef2,
					     unsigned short *coef3,
					     unsigned short *coef4);
void cs5530_get_video_downscale_enable(int *enable);
unsigned long cs5530_get_video_dst_size(void);
unsigned long cs5530_get_video_position(void);
unsigned long cs5530_get_video_color_key(void);
unsigned long cs5530_get_video_color_key_mask(void);
int cs5530_get_video_palette_entry(unsigned long index,
				   unsigned long *palette);
int cs5530_get_video_color_key_src(void);
int cs5530_get_video_filter(void);
int cs5530_get_video_request(short *x, short *y);
int cs5530_get_video_source(VideoSourceType * source);
int cs5530_get_vbi_source(VbiSourceType * source);
unsigned long cs5530_get_vbi_lines(int odd);
unsigned long cs5530_get_vbi_total(int odd);
int cs5530_get_video_interlaced(void);
int cs5530_get_color_space_YUV(void);
int cs5530_get_vertical_scaler_offset(char *offset);
unsigned long cs5530_get_genlock_delay(void);
int cs5530_get_genlock_enable(void);
int cs5530_get_video_cursor(unsigned long *key, unsigned long *mask,
			    unsigned short *select_color2,
			    unsigned long *color1, unsigned short *color2);
unsigned long cs5530_read_crc(void);
unsigned long cs5530_read_crc32(void);
unsigned long cs5530_read_window_crc(int source, unsigned short x,
				     unsigned short y, unsigned short width,
				     unsigned short height, int crc32);
int cs5530_get_macrovision_enable(void);

void cs5530_get_alpha_enable(int *enable);
void cs5530_get_alpha_size(unsigned short *x, unsigned short *y,
			   unsigned short *width, unsigned short *height);
void cs5530_get_alpha_value(unsigned char *alpha, char *delta);
void cs5530_get_alpha_priority(int *priority);
void cs5530_get_alpha_color(unsigned long *color);
unsigned long cs5530_get_clock_frequency(void);
int cs5530_get_vsa2_softvga_enable(void);
int cs5530_get_sync_polarities(void);

/*---------------------------------------------------------------------------
 * gfx_set_crt_enable
 * 
 * This routine enables or disables the CRT output from the video processor.
 *---------------------------------------------------------------------------
 */
#if GFX_VIDEO_DYNAMIC
int
cs5530_set_crt_enable(int enable)
#else
int
gfx_set_crt_enable(int enable)
#endif
{
   unsigned long config;

   config = READ_VID32(CS5530_DISPLAY_CONFIG);

   switch (enable) {
   case CRT_DISABLE:			/* Disable everything */

      WRITE_VID32(CS5530_DISPLAY_CONFIG,
		  config & ~(CS5530_DCFG_DIS_EN | CS5530_DCFG_HSYNC_EN |
			     CS5530_DCFG_VSYNC_EN | CS5530_DCFG_DAC_BL_EN |
			     CS5530_DCFG_DAC_PWDNX));
      break;

   case CRT_ENABLE:			/* Enable CRT display, including display logic */

      WRITE_VID32(CS5530_DISPLAY_CONFIG,
		  config | CS5530_DCFG_DIS_EN | CS5530_DCFG_HSYNC_EN |
		  CS5530_DCFG_VSYNC_EN | CS5530_DCFG_DAC_BL_EN |
		  CS5530_DCFG_DAC_PWDNX);
      break;

   case CRT_STANDBY:			/* HSync:Off VSync:On */

      WRITE_VID32(CS5530_DISPLAY_CONFIG,
		  (config &
		   ~(CS5530_DCFG_DIS_EN | CS5530_DCFG_HSYNC_EN |
		     CS5530_DCFG_DAC_BL_EN | CS5530_DCFG_DAC_PWDNX))
		  | CS5530_DCFG_VSYNC_EN);
      break;

   case CRT_SUSPEND:			/* HSync:On VSync:Off */

      WRITE_VID32(CS5530_DISPLAY_CONFIG,
		  (config &
		   ~(CS5530_DCFG_DIS_EN | CS5530_DCFG_VSYNC_EN |
		     CS5530_DCFG_DAC_BL_EN | CS5530_DCFG_DAC_PWDNX))
		  | CS5530_DCFG_HSYNC_EN);
      break;

   default:
      return (GFX_STATUS_BAD_PARAMETER);
   }
   return (GFX_STATUS_OK);
}

/*---------------------------------------------------------------------------
 * gfx_reset_video (PRIVATE ROUTINE: NOT PART OF DURANGO API)
 *
 * This routine is used to disable all components of video overlay before
 * performing a mode switch.
 *---------------------------------------------------------------------------
 */
#if GFX_VIDEO_DYNAMIC
void
cs5530_reset_video(void)
#else
void
gfx_reset_video(void)
#endif
{
   gfx_set_video_enable(0);
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
cs5530_set_display_control(int sync_polarities)
#else
int
gfx_set_display_control(int sync_polarities)
#endif
{
   unsigned long dcfg;

   /* ENABLE DISPLAY OUTPUT FROM CX5530 */

   dcfg = READ_VID32(CS5530_DISPLAY_CONFIG);

   /* CLEAR RELEVANT FIELDS */

   dcfg &= ~(CS5530_DCFG_CRT_SYNC_SKW_MASK | CS5530_DCFG_PWR_SEQ_DLY_MASK |
	     CS5530_DCFG_CRT_HSYNC_POL | CS5530_DCFG_CRT_VSYNC_POL |
	     CS5530_DCFG_FP_PWR_EN | CS5530_DCFG_FP_DATA_EN);

   /* INITIALIZATION */

   dcfg |= (CS5530_DCFG_CRT_SYNC_SKW_INIT |
	    CS5530_DCFG_PWR_SEQ_DLY_INIT | CS5530_DCFG_GV_PAL_BYP);

   if (PanelEnable) {
      dcfg |= CS5530_DCFG_FP_PWR_EN;
      dcfg |= CS5530_DCFG_FP_DATA_EN;
   }

   /* SET APPROPRIATE SYNC POLARITIES */

   if (sync_polarities & 1)
      dcfg |= CS5530_DCFG_CRT_HSYNC_POL;
   if (sync_polarities & 2)
      dcfg |= CS5530_DCFG_CRT_VSYNC_POL;

   WRITE_VID32(CS5530_DISPLAY_CONFIG, dcfg);
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
cs5530_set_clock_frequency(unsigned long frequency)
#else
void
gfx_set_clock_frequency(unsigned long frequency)
#endif
{
   unsigned int index;
   unsigned long value;
   long min, diff;

   /* FIND THE REGISTER VALUES FOR THE DESIRED FREQUENCY */
   /* Search the table for the closest frequency (16.16 format). */

   value = CS5530_PLLtable[0].pll_value;
   min = (long)CS5530_PLLtable[0].frequency - frequency;
   if (min < 0L)
      min = -min;
   for (index = 1; index < NUM_CS5530_FREQUENCIES; index++) {
      diff = (long)CS5530_PLLtable[index].frequency - frequency;
      if (diff < 0L)
	 diff = -diff;
      if (diff < min) {
	 min = diff;
	 value = CS5530_PLLtable[index].pll_value;
      }
   }

   /* SET THE DOT CLOCK REGISTER */

   WRITE_VID32(CS5530_DOT_CLK_CONFIG, value);
   WRITE_VID32(CS5530_DOT_CLK_CONFIG, value | 0x80000100);	/* set reset/bypass */
   gfx_delay_milliseconds(1);		/* wait for PLL to settle */
   WRITE_VID32(CS5530_DOT_CLK_CONFIG, value & 0x7FFFFFFF);	/* clear reset */
   WRITE_VID32(CS5530_DOT_CLK_CONFIG, value & 0x7FFFFEFF);	/* clear bypass */
   return;
}

/*-----------------------------------------------------------------------------
 * gfx_set_video_enable
 *
 * This routine enables or disables the video overlay functionality.
 *-----------------------------------------------------------------------------
 */
#if GFX_VIDEO_DYNAMIC
int
cs5530_set_video_enable(int enable)
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
   vcfg = READ_VID32(CS5530_VIDEO_CONFIG);
   if (enable) {
      /* ENABLE VIDEO OVERLAY FROM DISPLAY CONTROLLER */
      /* Use private routine to abstract the display controller. */

      gfx_set_display_video_enable(1);

      /* SET CS5530 BUS CONTROL PARAMETERS */
      /* Currently always high speed, 8-bit interface. */

      vcfg |= CS5530_VCFG_HIGH_SPD_INT;
      vcfg &= ~(CS5530_VCFG_EARLY_VID_RDY | CS5530_VCFG_16_BIT_EN);

      /* ENABLE CS5530 VIDEO OVERLAY */

      vcfg |= CS5530_VCFG_VID_EN;
      WRITE_VID32(CS5530_VIDEO_CONFIG, vcfg);
   } else {
      /* DISABLE CS5530 VIDEO OVERLAY */

      vcfg &= ~CS5530_VCFG_VID_EN;
      WRITE_VID32(CS5530_VIDEO_CONFIG, vcfg);

      /* DISABLE VIDEO OVERLAY FROM DISPLAY CONTROLLER */
      /* Use private routine to abstract the display controller. */

      gfx_set_display_video_enable(0);
   }
   return (0);
}

/*-----------------------------------------------------------------------------
 * gfx_set_video_format
 *
 * Currently only sets 4:2:0 format, Y1 V Y0 U.
 *-----------------------------------------------------------------------------
 */
#if GFX_VIDEO_DYNAMIC
int
cs5530_set_video_format(unsigned long format)
#else
int
gfx_set_video_format(unsigned long format)
#endif
{
   unsigned long vcfg = 0;

   /* SET THE CS5530 VIDEO INPUT FORMAT */

   vcfg = READ_VID32(CS5530_VIDEO_CONFIG);
   vcfg &= ~(CS5530_VCFG_VID_INP_FORMAT | CS5530_VCFG_4_2_0_MODE);
   vcfg &= ~(CS5530_VCFG_CSC_BYPASS);
   vcfg &= ~(CS5530_VCFG_GV_SEL);

   if (format < 4)
      vcfg |= (format << 2);
   else {
      if (format == VIDEO_FORMAT_Y0Y1Y2Y3) {
	 vcfg |= CS5530_VCFG_4_2_0_MODE;
	 vcfg |= 1 << 2;
      }
      if (format == VIDEO_FORMAT_RGB) {
	 vcfg |= CS5530_VCFG_CSC_BYPASS;
	 vcfg |= CS5530_VCFG_GV_SEL;
      }
   }

   WRITE_VID32(CS5530_VIDEO_CONFIG, vcfg);
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
cs5530_set_video_size(unsigned short width, unsigned short height)
#else
int
gfx_set_video_size(unsigned short width, unsigned short height)
#endif
{
   unsigned long size, vcfg;

   /* SET THE CS5530 VIDEO LINE SIZE */

   vcfg = READ_VID32(CS5530_VIDEO_CONFIG);
   vcfg &= ~(CS5530_VCFG_LINE_SIZE_LOWER_MASK | CS5530_VCFG_LINE_SIZE_UPPER);
   size = (width >> 1);
   vcfg |= (size & 0x00FF) << 8;
   if (size & 0x0100)
      vcfg |= CS5530_VCFG_LINE_SIZE_UPPER;
   WRITE_VID32(CS5530_VIDEO_CONFIG, vcfg);

   /* SET TOTAL VIDEO BUFFER SIZE IN DISPLAY CONTROLLER */
   /* Use private routine to abstract the display controller. */

   gfx_set_display_video_size(width, height);
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
cs5530_set_video_offset(unsigned long offset)
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

/*---------------------------------------------------------------------------
 * gfx_set_video_scale
 * 
 * This routine sets the scale factor for the video overlay window.  The 
 * size of the source and destination regions are specified in pixels.  
 *---------------------------------------------------------------------------
 */
#if GFX_VIDEO_DYNAMIC
int
cs5530_set_video_scale(unsigned short srcw, unsigned short srch,
		       unsigned short dstw, unsigned short dsth)
#else
int
gfx_set_video_scale(unsigned short srcw, unsigned short srch,
		    unsigned short dstw, unsigned short dsth)
#endif
{
   unsigned long xscale, yscale;

   /* SAVE PARAMETERS */
   /* These are needed for clipping the video window later. */

   gfx_vid_srcw = srcw;
   gfx_vid_srch = srch;
   gfx_vid_dstw = dstw;
   gfx_vid_dsth = dsth;

   /* CALCULATE CS5530 SCALE FACTORS */
   /* No downscaling in CS5530 so force to 1x if attempted. */

   if (dstw <= srcw)
      xscale = 0x1FFF;
   else if (dstw == 1 || srcw == 1)
      return GFX_STATUS_BAD_PARAMETER;
   else
      xscale = (0x2000l * (srcw - 1l)) / (dstw - 1l);
   if (dsth <= srch)
      yscale = 0x1FFF;
   else if (dsth == 1 || srch == 1)
      return GFX_STATUS_BAD_PARAMETER;
   else
      yscale = (0x2000l * (srch - 1l)) / (dsth - 1l);
   WRITE_VID32(CS5530_VIDEO_SCALE, (yscale << 16) | xscale);

   /* CALL ROUTINE TO UPDATE WINDOW POSITION */
   /* This is required because the scale values effect the number of */
   /* source data pixels that need to be clipped, as well as the */
   /* amount of data that needs to be transferred. */

   gfx_set_video_window(gfx_vid_xpos, gfx_vid_ypos, gfx_vid_width,
			gfx_vid_height);
   return (0);
}

/*---------------------------------------------------------------------------
 * gfx_set_video_window
 * 
 * This routine sets the position and size of the video overlay window.  The 
 * position is specified in screen relative coordinates, and may be negative.  
 * The size of destination region is specified in pixels.  The line size
 * indicates the number of bytes of source data per scanline.
 *---------------------------------------------------------------------------
 */
#if GFX_VIDEO_DYNAMIC
int
cs5530_set_video_window(short x, short y, unsigned short w, unsigned short h)
#else
int
gfx_set_video_window(short x, short y, unsigned short w, unsigned short h)
#endif
{
   unsigned long vcfg = 0;
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

   hadjust = gfx_get_htotal() - gfx_get_hsync_end() - 13l;
   vadjust = gfx_get_vtotal() - gfx_get_vsync_end() + 1l;

   /* HORIZONTAL START */
   xstart = (unsigned long)x + hadjust;

   /* HORIZONTAL END */
   /* End positions in register are non-inclusive (one more than the actual end) */

   if ((x + w) < gfx_get_hactive())
      xend = (unsigned long)x + (unsigned long)w + hadjust;
   else					/* right clipping needed */
      xend = (unsigned long)gfx_get_hactive() + hadjust;

   /* VERTICAL START */

   ystart = (unsigned long)y + vadjust;

   /* VERTICAL END */

   if ((y + h) < gfx_get_vactive())
      yend = (unsigned long)y + (unsigned long)h + vadjust;
   else					/* bottom clipping needed */
      yend = (unsigned long)gfx_get_vactive() + vadjust;

   /* DISABLE REGISTER UPDATES */

   vcfg = READ_VID32(CS5530_VIDEO_CONFIG);
   vcfg &= ~CS5530_VCFG_VID_REG_UPDATE;
   WRITE_VID32(CS5530_VIDEO_CONFIG, vcfg);

   /* SET VIDEO POSITION */

   WRITE_VID32(CS5530_VIDEO_X_POS, (xend << 16) | xstart);
   WRITE_VID32(CS5530_VIDEO_Y_POS, (yend << 16) | ystart);

   vcfg |= CS5530_VCFG_VID_REG_UPDATE;
   WRITE_VID32(CS5530_VIDEO_CONFIG, vcfg);

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
cs5530_set_video_left_crop(unsigned short x)
#else
int
gfx_set_video_left_crop(unsigned short x)
#endif
{
   unsigned long vcfg, initread;

   /* CLIPPING ON LEFT */
   /* Adjust initial read for scale, checking for divide by zero */

   if (gfx_vid_dstw)
      initread = (unsigned long)x *gfx_vid_srcw / gfx_vid_dstw;

   else
      initread = 0;

   /* SET INITIAL READ ADDRESS AND ENABLE REGISTER UPDATES */

   vcfg = READ_VID32(CS5530_VIDEO_CONFIG);
   vcfg &= ~CS5530_VCFG_INIT_READ_MASK;
   vcfg |= (initread << 15) & CS5530_VCFG_INIT_READ_MASK;
   vcfg |= CS5530_VCFG_VID_REG_UPDATE;
   WRITE_VID32(CS5530_VIDEO_CONFIG, vcfg);
   return (0);
}

/*---------------------------------------------------------------------------
 * gfx_set_video_color_key
 * 
 * This routine specifies the color key value and mask for the video overlay
 * hardware.  To disable color key, the color and mask should both be set to 
 * zero.  The hardware uses the color key in the following equation:
 *
 * ((source data) & (color key mask)) == ((color key) & (color key mask))
 *
 * The source data can be either graphics data or video data.  The bluescreen
 * parameter is set to have the hardware compare video data and clear to
 * comapare graphics data.
 *---------------------------------------------------------------------------
 */
#if GFX_VIDEO_DYNAMIC
int
cs5530_set_video_color_key(unsigned long key, unsigned long mask,
			   int graphics)
#else
int
gfx_set_video_color_key(unsigned long key, unsigned long mask, int graphics)
#endif
{
   unsigned long dcfg = 0;

   /* SET CS5530 COLOR KEY VALUE */

   WRITE_VID32(CS5530_VIDEO_COLOR_KEY, key);
   WRITE_VID32(CS5530_VIDEO_COLOR_MASK, mask);

   /* SELECT GRAPHICS OR VIDEO DATA TO COMPARE TO THE COLOR KEY */

   dcfg = READ_VID32(CS5530_DISPLAY_CONFIG);
   if (graphics & 0x01)
      dcfg &= ~CS5530_DCFG_VG_CK;
   else
      dcfg |= CS5530_DCFG_VG_CK;
   WRITE_VID32(CS5530_DISPLAY_CONFIG, dcfg);
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
cs5530_set_video_filter(int xfilter, int yfilter)
#else
int
gfx_set_video_filter(int xfilter, int yfilter)
#endif
{
   unsigned long vcfg = 0;

   /* ENABLE OR DISABLE CS5530 VIDEO OVERLAY FILTERS */

   vcfg = READ_VID32(CS5530_VIDEO_CONFIG);
   vcfg &= ~(CS5530_VCFG_X_FILTER_EN | CS5530_VCFG_Y_FILTER_EN);
   if (xfilter)
      vcfg |= CS5530_VCFG_X_FILTER_EN;
   if (yfilter)
      vcfg |= CS5530_VCFG_Y_FILTER_EN;
   WRITE_VID32(CS5530_VIDEO_CONFIG, vcfg);

   return (0);
}

/*---------------------------------------------------------------------------
 * gfx_set_video_palette
 * 
 * This routine loads the video hardware palette.  If a NULL pointer is 
 * specified, the palette is bypassed (for CS5530, this means loading the 
 * palette with identity values). 
 *---------------------------------------------------------------------------
 */
#if GFX_VIDEO_DYNAMIC
int
cs5530_set_video_palette(unsigned long *palette)
#else
int
gfx_set_video_palette(unsigned long *palette)
#endif
{
   unsigned long i, entry;

   /* LOAD CS5530 VIDEO PALETTE */

   WRITE_VID32(CS5530_PALETTE_ADDRESS, 0);
   for (i = 0; i < 256; i++) {
      if (palette)
	 entry = palette[i];
      else
	 entry = i | (i << 8) | (i << 16);
      WRITE_VID32(CS5530_PALETTE_DATA, entry);
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
cs5530_set_video_palette_entry(unsigned long index, unsigned long palette)
#else
int
gfx_set_video_palette_entry(unsigned long index, unsigned long palette)
#endif
{
   if (index > 0xFF)
      return GFX_STATUS_BAD_PARAMETER;

   /* SET A SINGLE ENTRY */

   WRITE_VID32(CS5530_PALETTE_ADDRESS, index);
   WRITE_VID32(CS5530_PALETTE_DATA, palette);

   return (0);
}

#define CX55xx_VIDEO_PCI_44 0x80009444

/*---------------------------------------------------------------------------
 * gfx_disable_softvga
 * 
 * Disables SoftVga. This function is only valid with VSA2, Returns 1 if
 * SoftVga can be disabled; 0 if not.
 *---------------------------------------------------------------------------
 */
#if GFX_VIDEO_DYNAMIC
int
cs5530_disable_softvga(void)
#else
int
gfx_disable_softvga(void)
#endif
{
   unsigned long reg_val;

   /* get the current value */
   reg_val = gfx_pci_config_read(CX55xx_VIDEO_PCI_44);
   /* setting video PCI register 44 bit 0 to 1 disables SoftVga */
   reg_val |= 0x1;
   gfx_pci_config_write(CX55xx_VIDEO_PCI_44, reg_val);

   /* see if we set the bit and return the appropriate value */
   reg_val = gfx_pci_config_read(CX55xx_VIDEO_PCI_44);
   if ((reg_val & 0x1) == 0x1)
      return (1);
   else
      return (0);

}

/*---------------------------------------------------------------------------
 * gfx_enable_softvga
 * 
 * Enables SoftVga. This function is only valid with VSA2, Returns 1 if
 * SoftVga can be enbled; 0 if not.
 *---------------------------------------------------------------------------
 */
#if GFX_VIDEO_DYNAMIC
int
cs5530_enable_softvga(void)
#else
int
gfx_enable_softvga(void)
#endif
{
   unsigned long reg_val;

   /* get the current value */
   reg_val = gfx_pci_config_read(CX55xx_VIDEO_PCI_44);
   /* clearing video PCI register 44 bit 0 enables SoftVga */
   gfx_pci_config_write(CX55xx_VIDEO_PCI_44, reg_val & 0xfffffffe);

   /* see if we cleared the bit and return the appropriate value */
   reg_val = gfx_pci_config_read(CX55xx_VIDEO_PCI_44);
   if ((reg_val & 0x1) == 0)
      return (1);
   else
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
cs5530_get_clock_frequency(void)
#else
unsigned long
gfx_get_clock_frequency(void)
#endif
{
   unsigned int index;
   unsigned long value, mask;

   mask = 0x7FFFFEDC;
   value = READ_VID32(CS5530_DOT_CLK_CONFIG) & mask;
   for (index = 0; index < NUM_CS5530_FREQUENCIES; index++) {
      if ((CS5530_PLLtable[index].pll_value & mask) == value)
	 return (CS5530_PLLtable[index].frequency);
   }
   return (0);
}

/*************************************************************/
/*  READ ROUTINES  |  INCLUDED FOR DIAGNOSTIC PURPOSES ONLY  */
/*************************************************************/

#if GFX_READ_ROUTINES

/*---------------------------------------------------------------------------
 * gfx_get_vsa2_softvga_enable
 * 
 * This function returns the enable status of SoftVGA.  It is valid
 * only if VSAII is present.
 *---------------------------------------------------------------------------
 */
#if GFX_VIDEO_DYNAMIC
int
cs5530_get_vsa2_softvga_enable(void)
#else
int
gfx_get_vsa2_softvga_enable(void)
#endif
{
   unsigned long reg_val;

   reg_val = gfx_pci_config_read(CX55xx_VIDEO_PCI_44);
   if ((reg_val & 0x1) == 0)
      return (1);
   else
      return (0);

}

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
cs5530_get_sync_polarities(void)
#else
int
gfx_get_sync_polarities(void)
#endif
{
   int polarities = 0;

   if (READ_VID32(CS5530_DISPLAY_CONFIG) & 0x00000100)
      polarities |= 1;
   if (READ_VID32(CS5530_DISPLAY_CONFIG) & 0x00000200)
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
cs5530_get_video_palette_entry(unsigned long index, unsigned long *palette)
#else
int
gfx_get_video_palette_entry(unsigned long index, unsigned long *palette)
#endif
{
   if (index > 0xFF)
      return GFX_STATUS_BAD_PARAMETER;

   /* READ A SINGLE ENTRY */

   WRITE_VID32(CS5530_PALETTE_ADDRESS, index);
   *palette = READ_VID32(CS5530_PALETTE_DATA);

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
cs5530_get_video_enable(void)
#else
int
gfx_get_video_enable(void)
#endif
{
   if (READ_VID32(CS5530_VIDEO_CONFIG) & CS5530_VCFG_VID_EN)
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
cs5530_get_video_format(void)
#else
int
gfx_get_video_format(void)
#endif
{
   unsigned long vcfg;

   vcfg = READ_VID32(CS5530_VIDEO_CONFIG);
   if (vcfg & CS5530_VCFG_CSC_BYPASS)
      return (VIDEO_FORMAT_RGB);
   if (vcfg & CS5530_VCFG_4_2_0_MODE)
      return (VIDEO_FORMAT_Y0Y1Y2Y3);

   return ((int)((vcfg >> 2) & 3));
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
cs5530_get_video_src_size(void)
#else
unsigned long
gfx_get_video_src_size(void)
#endif
{
   unsigned long width = 0, height = 0;

   /* DETERMINE SOURCE WIDTH FROM THE CS5530 VIDEO LINE SIZE */

   width = (READ_VID32(CS5530_VIDEO_CONFIG) >> 7) & 0x000001FE;
   if (READ_VID32(CS5530_VIDEO_CONFIG) & CS5530_VCFG_LINE_SIZE_UPPER)
      width += 512l;

   if (width) {
      /* DETERMINE HEIGHT BY DIVIDING TOTAL SIZE BY WIDTH */
      /* Get total size from display controller - abtracted. */

      height = gfx_get_display_video_size() / (width << 1);
   }
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
cs5530_get_video_line_size(void)
#else
unsigned long
gfx_get_video_line_size(void)
#endif
{
   unsigned long width = 0;

   /* DETERMINE SOURCE WIDTH FROM THE CS5530 VIDEO LINE SIZE */

   width = (READ_VID32(CS5530_VIDEO_CONFIG) >> 7) & 0x000001FE;
   if (READ_VID32(CS5530_VIDEO_CONFIG) & CS5530_VCFG_LINE_SIZE_UPPER)
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
cs5530_get_video_xclip(void)
#else
unsigned long
gfx_get_video_xclip(void)
#endif
{
   unsigned long clip = 0;

   /* DETERMINE SOURCE WIDTH FROM THE CS5530 VIDEO LINE SIZE */

   clip = (READ_VID32(CS5530_VIDEO_CONFIG) >> 14) & 0x000007FC;
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
cs5530_get_video_offset(void)
#else
unsigned long
gfx_get_video_offset(void)
#endif
{
   return (gfx_get_display_video_offset());
}

/*---------------------------------------------------------------------------
 * gfx_get_video_scale
 * 
 * This routine returns the scale factor for the video overlay window.
 *---------------------------------------------------------------------------
 */
#if GFX_VIDEO_DYNAMIC
unsigned long
cs5530_get_video_scale(void)
#else
unsigned long
gfx_get_video_scale(void)
#endif
{
   return (READ_VID32(CS5530_VIDEO_SCALE));
}

/*---------------------------------------------------------------------------
 * gfx_get_video_dst_size
 * 
 * This routine returns the size of the displayed video overlay window.
 * NOTE: This is the displayed window size, which may be different from 
 * the real window size if clipped.
 *---------------------------------------------------------------------------
 */
#if GFX_VIDEO_DYNAMIC
unsigned long
cs5530_get_video_dst_size(void)
#else
unsigned long
gfx_get_video_dst_size(void)
#endif
{
   unsigned long xsize, ysize;

   xsize = READ_VID32(CS5530_VIDEO_X_POS);
   xsize = ((xsize >> 16) & 0x7FF) - (xsize & 0x07FF);
   ysize = READ_VID32(CS5530_VIDEO_Y_POS);
   ysize = ((ysize >> 16) & 0x7FF) - (ysize & 0x07FF);
   return ((ysize << 16) | xsize);
}

/*---------------------------------------------------------------------------
 * gfx_get_video_position
 * 
 * This routine returns the position of the video overlay window.  The
 * return value is (ypos << 16) | xpos.
 * NOTE: This is the displayed window position, which may be different from 
 * the real window position if clipped.
 *---------------------------------------------------------------------------
 */
#if GFX_VIDEO_DYNAMIC
unsigned long
cs5530_get_video_position(void)
#else
unsigned long
gfx_get_video_position(void)
#endif
{
   unsigned long hadjust, vadjust;
   unsigned long xpos, ypos;

   /* READ HARDWARE POSITION */

   xpos = READ_VID32(CS5530_VIDEO_X_POS) & 0x000007FF;
   ypos = READ_VID32(CS5530_VIDEO_Y_POS) & 0x000007FF;

   /* GET ADJUSTMENT VALUES */
   /* Use routines to abstract version of display controller. */

   hadjust = gfx_get_htotal() - gfx_get_hsync_end() - 13l;
   vadjust = gfx_get_vtotal() - gfx_get_vsync_end() + 1l;
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
cs5530_get_video_color_key(void)
#else
unsigned long
gfx_get_video_color_key(void)
#endif
{
   return (READ_VID32(CS5530_VIDEO_COLOR_KEY));
}

/*---------------------------------------------------------------------------
 * gfx_get_video_color_key_mask
 * 
 * This routine returns the current video color mask value.
 *---------------------------------------------------------------------------
 */
#if GFX_VIDEO_DYNAMIC
unsigned long
cs5530_get_video_color_key_mask(void)
#else
unsigned long
gfx_get_video_color_key_mask(void)
#endif
{
   return (READ_VID32(CS5530_VIDEO_COLOR_MASK));
}

/*---------------------------------------------------------------------------
 * gfx_get_video_color_key_src
 * 
 * This routine returns 0 for video data compare, 1 for graphics data.
 *---------------------------------------------------------------------------
 */
#if GFX_VIDEO_DYNAMIC
int
cs5530_get_video_color_key_src(void)
#else
int
gfx_get_video_color_key_src(void)
#endif
{
   if (READ_VID32(CS5530_DISPLAY_CONFIG) & CS5530_DCFG_VG_CK)
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
cs5530_get_video_filter(void)
#else
int
gfx_get_video_filter(void)
#endif
{
   int retval = 0;

   if (READ_VID32(CS5530_VIDEO_CONFIG) & CS5530_VCFG_X_FILTER_EN)
      retval |= 1;
   if (READ_VID32(CS5530_VIDEO_CONFIG) & CS5530_VCFG_Y_FILTER_EN)
      retval |= 2;
   return (retval);
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
cs5530_read_crc(void)
#else
unsigned long
gfx_read_crc(void)
#endif
{
   unsigned long crc = 0xFFFFFFFF;

   if (gfx_test_timing_active()) {
      /* WAIT UNTIL ACTIVE DISPLAY */

      while (!gfx_test_vertical_active()) ;

      /* RESET CRC DURING ACTIVE DISPLAY */

      WRITE_VID32(CS5530_CRCSIG_TFT_TV, 0);
      WRITE_VID32(CS5530_CRCSIG_TFT_TV, 1);

      /* WAIT UNTIL NOT ACTIVE, THEN ACTIVE, NOT ACTIVE, THEN ACTIVE */

      while (gfx_test_vertical_active()) ;
      while (!gfx_test_vertical_active()) ;
      while (gfx_test_vertical_active()) ;
      while (!gfx_test_vertical_active()) ;
      crc = READ_VID32(CS5530_CRCSIG_TFT_TV) >> 8;
   }
   return (crc);
}

#endif /* GFX_READ_ROUTINES */

/* END OF FILE */

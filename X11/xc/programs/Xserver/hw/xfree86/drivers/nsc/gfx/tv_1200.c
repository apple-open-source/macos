/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/nsc/gfx/tv_1200.c,v 1.1 2002/12/10 15:12:27 alanh Exp $ */
/*
 * $Workfile: tv_1200.c $
 *
 * This file contains routines to control the SC1200 TVOUT and TV encoder.
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

int sc1200_set_tv_format(TVStandardType format, GfxOnTVType resolution);
int sc1200_set_tv_output(int output);
int sc1200_set_tv_enable(int enable);
int sc1200_set_tv_flicker_filter(int ff);
int sc1200_set_tv_sub_carrier_reset(int screset);
int sc1200_set_tv_vphase(int vphase);
int sc1200_set_tv_YC_delay(int delay);
int sc1200_set_tvenc_reset_interval(int interval);
int sc1200_set_tv_cc_enable(int enable);
int sc1200_set_tv_cc_data(unsigned char data1, unsigned char data2);
int sc1200_set_tv_display(int width, int height);
int sc1200_test_tvout_odd_field(void);
int sc1200_test_tvenc_odd_field(void);
int sc1200_set_tv_field_status_invert(int enable);
int sc1200_get_tv_vphase(void);
int sc1200_get_tv_enable(unsigned int *p_on);
int sc1200_get_tv_output(void);
int sc1200_get_tv_mode_count(TVStandardType format);
int sc1200_get_tv_display_mode(int *width, int *height, int *bpp, int *hz);
int sc1200_get_tv_display_mode_frequency(unsigned short width,
					 unsigned short height,
					 TVStandardType format,
					 int *frequency);
int sc1200_is_tv_display_mode_supported(unsigned short width,
					unsigned short height,
					TVStandardType format);

int sc1200_get_tv_standard(unsigned long *p_standard);
int sc1200_get_available_tv_standards(unsigned long *p_standards);
int sc1200_set_tv_standard(unsigned long standard);
int sc1200_get_tv_vga_mode(unsigned long *p_vga_mode);
int sc1200_get_available_tv_vga_modes(unsigned long *p_vga_modes);
int sc1200_set_tv_vga_mode(unsigned long vga_mode);
int sc1200_get_tvout_mode(unsigned long *p_tvout_mode);
int sc1200_set_tvout_mode(unsigned long tvout_mode);
int sc1200_get_sharpness(int *p_sharpness);
int sc1200_set_sharpness(int sharpness);
int sc1200_get_flicker_filter(int *p_flicker);
int sc1200_set_flicker_filter(int flicker);
int sc1200_get_overscan(int *p_x, int *p_y);
int sc1200_set_overscan(int x, int y);
int sc1200_get_position(int *p_x, int *p_y);
int sc1200_set_position(int x, int y);
int sc1200_get_color(int *p_color);
int sc1200_set_color(int color);
int sc1200_get_brightness(int *p_brightness);
int sc1200_set_brightness(int brightness);
int sc1200_get_contrast(int *p_contrast);
int sc1200_set_contrast(int constrast);
int sc1200_get_yc_filter(unsigned int *p_yc_filter);
int sc1200_set_yc_filter(unsigned int yc_filter);
int sc1200_get_aps_trigger_bits(unsigned int *p_trigger_bits);
int sc1200_set_aps_trigger_bits(unsigned int trigger_bits);
unsigned char cc_add_parity_bit(unsigned char data);

/*-----------------------------------------------------------------------------
 * gfx_set_tv_format
 *
 * This routine sets the TV encoder registers to the specified format
 * and resolution.
 *-----------------------------------------------------------------------------
 */
#if GFX_TV_DYNAMIC
int
sc1200_set_tv_format(TVStandardType format, GfxOnTVType resolution)
#else
int
gfx_set_tv_format(TVStandardType format, GfxOnTVType resolution)
#endif
{
   unsigned long ctrl2, mode;

   /* Save TV output mode */
   ctrl2 =
	 READ_VID32(SC1200_TVENC_TIM_CTRL_2) & (SC1200_TVENC_OUTPUT_YCBCR |
						SC1200_TVENC_CFS_MASK);
   /* Save flicker filter setting */
   mode = READ_VID32(SC1200_TVOUT_HORZ_SCALING) &
	 SC1200_TVOUT_FLICKER_FILTER_MASK;

   switch (format) {
   case TV_STANDARD_NTSC:
      /* Horizontal Sync Start is 848 */
      /* Horizontal Sync End is 856 */
      WRITE_VID32(SC1200_TVOUT_HORZ_SYNC, 0x03580350);
      /* Vertical Sync Start is 0 */
      /* Vertical Sync End is 1 */
      /* Vertical Display Start Skew is 1 */
      /* Vertical Display End Skew is 1 */
      WRITE_VID32(SC1200_TVOUT_VERT_SYNC, 0x05001000);
      /* Disable vertical down scaling, take all lines */
      if (gfx_chip_revision <= SC1200_REV_B3)
	 WRITE_VID32(SC1200_TVOUT_VERT_DOWNSCALE, 0xffffffff);
      /* Enable video timing */
      /* Reset sub carrier every two frames */
      /* Disable BLANK */
      /* Enable color burst */
      /* Add the IRE offset */
      /* NTSC color encoding */
      /* Video generator timing is 525 lines / 60Hz */
      /* Horizontal and Vertical counters are initialized to HPHASE & VPHASE */
      /* VPHASE is 2, HPHASE is 0x50 */
      WRITE_VID32(SC1200_TVENC_TIM_CTRL_1, 0xa2a01050);
      /* Increase horizontal blanking interval */
      /* Low Water Mark for Y is 0x1F */
      /* Low Water Mark for Cb is 0xF */
      /* HUE is 0 */
      /* SCPHASE is 0xF9 */
      WRITE_VID32(SC1200_TVENC_TIM_CTRL_2, 0x9ff000f9 | ctrl2);
      /* Subcarrier Frequency is 3.579545 MHz */
      WRITE_VID32(SC1200_TVENC_SUB_FREQ, 0x21f07c1f);
      /* VSTART is 18, HSTART is 113 */
      WRITE_VID32(SC1200_TVENC_DISP_POS, 0x00120071);
      /* Display size: HEIGHT is 239, WIDTH is 719 */
      WRITE_VID32(SC1200_TVENC_DISP_SIZE, 0x00ef02cf);
      switch (resolution) {
      case GFX_ON_TV_SQUARE_PIXELS:
	 if (gfx_chip_revision <= SC1200_REV_B3) {
	    /* Horizontal Display start is 116 */
	    /* Total number of pixels per line is 857 */
	    WRITE_VID32(SC1200_TVOUT_HORZ_TIM, 0x00740359);
	    /* HSYNC generated in the TV Encoder module */
	    /* Interval between resets of TV Encoder is once every odd field */
	    /* Enable Horizontal interpolation */
	    /* Enable Horizontal up scaling 9/8 */
	    /* Disable Horizontal downscale */
	    WRITE_VID32(SC1200_TVOUT_HORZ_SCALING, 0x10020700 | mode);
	    /* Horizontal display end is 919, i.e. 720 active pixels */
	    /* Total number of display lines per field is 240 */
	    WRITE_VID32(SC1200_TVOUT_LINE_END, 0x039700f0);
	 } else {			/* Use new scaler available in Rev. C */
	    /* Horizontal Display start is 111 */
	    /* Total number of pixels per line is 857 */
	    WRITE_VID32(SC1200_TVOUT_HORZ_TIM, 0x006f0359);
	    /* HSYNC generated in the TV Encoder module */
	    /* Interval between resets of TV Encoder is once every odd field */
	    /* Enable Horizontal interpolation */
	    /* Disable Horizontal up scaling 9/8 */
	    /* Disable Horizontal downscale */
	    WRITE_VID32(SC1200_TVOUT_HORZ_SCALING, 0x10020500 | mode);
	    /* Set Horizontal upscaling to 64/58 (~ 11/10) */
	    WRITE_VID32(SC1200_TVOUT_HORZ_PRE_ENCODER_SCALE, 0x3A000000);
	    /* Horizontal display end is 900, i.e. 706 active pixels */
	    /* Total number of display lines per field is 240 */
	    WRITE_VID32(SC1200_TVOUT_LINE_END, 0x038400f0);
	 }
	 break;
      case GFX_ON_TV_NO_SCALING:
	 /* Horizontal Display start is 116 */
	 /* Total number of pixels per line is 857 */
	 WRITE_VID32(SC1200_TVOUT_HORZ_TIM, 0x00740359);
	 /* HSYNC generated in the TV Encoder module */
	 /* Interval between resets of TV Encoder is once every odd field */
	 /* Enable Horizontal interpolation */
	 /* Disable Horizontal up scaling 9/8 */
	 /* Disable Horizontal downscale */
	 WRITE_VID32(SC1200_TVOUT_HORZ_SCALING, 0x10020500 | mode);
	 /* Disable Horizontal scaling (set to 64/64) */
	 if (gfx_chip_revision >= SC1200_REV_C1)
	    WRITE_VID32(SC1200_TVOUT_HORZ_PRE_ENCODER_SCALE, 0x40000000);
	 /* Horizontal display end is 919, i.e. 720 active pixels */
	 /* Total number of display lines per field is 240 */
	 WRITE_VID32(SC1200_TVOUT_LINE_END, 0x039700f0);
	 break;
      default:
	 return (GFX_STATUS_BAD_PARAMETER);
      }
      break;
   case TV_STANDARD_PAL:
      /* Horizontal Sync Start is 854 */
      /* Horizontal Sync End is 862 */
      WRITE_VID32(SC1200_TVOUT_HORZ_SYNC, 0x035e0356);
      /* Vertical Sync Start is 0 */
      /* Vertical Sync End is 1 */
      /* Vertical Display Start Skew is 1 */
      /* Vertical Display End Skew is 1 */
      WRITE_VID32(SC1200_TVOUT_VERT_SYNC, 0x05001000);
      /* Disable vertical down scaling, take all lines */
      if (gfx_chip_revision <= SC1200_REV_B3)
	 WRITE_VID32(SC1200_TVOUT_VERT_DOWNSCALE, 0xffffffff);
      /* Enable video timing */
      /* Never reset sub carrier (should be every 4 frames but doesn't work with genlock) */
      /* Disable BLANK */
      /* Enable color burst */
      /* Do not add the IRE offset */
      /* NTSC color encoding */
      /* Video generator timing is 625 lines / 50Hz */
      /* Horizontal and Vertical counters are initialized to HPHASE & VPHASE */
      /* VPHASE is 2, HPHASE is 50 */
      WRITE_VID32(SC1200_TVENC_TIM_CTRL_1, 0xB1201050);
      /* Increase horizontal blanking interval */
      /* Low Water Mark for Y is 0x1F */
      /* Low Water Mark for Cb is 0xF */
      /* HUE is 0 */
      /* SCPHASE is 0xD9 */
      WRITE_VID32(SC1200_TVENC_TIM_CTRL_2, 0x9ff000d9 | ctrl2);
      /* Subcarrier Frequency is 4.43361875 MHz */
      WRITE_VID32(SC1200_TVENC_SUB_FREQ, 0x2a098acb);
      /* VSTART is 22, HSTART is 123 */
      WRITE_VID32(SC1200_TVENC_DISP_POS, 0x0016007b);
      /* Display size: HEIGHT is 287, WIDTH is 719 */
      WRITE_VID32(SC1200_TVENC_DISP_SIZE, 0x011f02cf);
      switch (resolution) {
      case GFX_ON_TV_NO_SCALING:
	 /* Horizontal Display start is 124 */
	 /* Total number of pixels per line is 863 */
	 WRITE_VID32(SC1200_TVOUT_HORZ_TIM, 0x007c035f);
	 /* HSYNC generated in the TV Encoder module */
	 /* Interval between resets of TV Encoder is once every odd field */
	 /* Enable Horizontal interpolation */
	 /* Disable Horizontal up scaling 9/8 */
	 /* Disable Horizontal downscale */
	 WRITE_VID32(SC1200_TVOUT_HORZ_SCALING, 0x10020500 | mode);
	 /* Disable Horizontal scaling (set to 64/64) */
	 if (gfx_chip_revision >= SC1200_REV_C1)
	    WRITE_VID32(SC1200_TVOUT_HORZ_PRE_ENCODER_SCALE, 0x40000000);
	 /* Horizontal display end is 924, i.e. 720 active pixels */
	 /* Total number of display lines per field is 288 */
	 WRITE_VID32(SC1200_TVOUT_LINE_END, 0x039c0120);
	 break;
      case GFX_ON_TV_SQUARE_PIXELS:
	 /* Horizontal Display start is 122 */
	 /* Total number of pixels per line is 863 */
	 WRITE_VID32(SC1200_TVOUT_HORZ_TIM, 0x007a035f);
	 if (gfx_chip_revision <= SC1200_REV_B3) {
	    /* HSYNC generated in the TV Encoder module */
	    /* Interval between resets of TV Encoder is once every odd field */
	    /* Enable Horizontal interpolation */
	    /* Disable Horizontal up scaling 9/8 */
	    /* Horizontal downscale m/(m+1), m = 11, (i.e. 11/12 - closest possible to 54/59) */
	    WRITE_VID32(SC1200_TVOUT_HORZ_SCALING, 0x1002040b | mode);
	    /* Horizontal display end is 906, i.e. 704 active pixels */
	    /* Total number of display lines per field is 288 */
	    WRITE_VID32(SC1200_TVOUT_LINE_END, 0x038a0120);
	 } else {
	    /* HSYNC generated in the TV Encoder module */
	    /* Interval between resets of TV Encoder is once every odd field */
	    /* Enable Horizontal interpolation */
	    /* Disable Horizontal up scaling 9/8 */
	    /* Disable Horizontal downscale */
	    WRITE_VID32(SC1200_TVOUT_HORZ_SCALING, 0x10020500 | mode);
	    /* Set Horizontal down scaling to 64/70 (closest possible to 54/59) */
	    WRITE_VID32(SC1200_TVOUT_HORZ_PRE_ENCODER_SCALE, 0x46000000);
	    /* Horizontal display end is 904, i.e. 702 active pixels */
	    /* Total number of display lines per field is 288 */
	    WRITE_VID32(SC1200_TVOUT_LINE_END, 0x03880120);
	 }
	 break;
      default:
	 return (GFX_STATUS_BAD_PARAMETER);
      }
      break;
   default:
      return (GFX_STATUS_BAD_PARAMETER);
   }
   return (GFX_STATUS_OK);
}

/*-----------------------------------------------------------------------------
 * gfx_set_tv_output
 *
 * This routine sets the TV encoder registers to the specified output type.
 * Supported output types are : S-VIDEO, Composite, YUV and SCART.
 *-----------------------------------------------------------------------------
 */
#if GFX_TV_DYNAMIC
int
sc1200_set_tv_output(int output)
#else
int
gfx_set_tv_output(int output)
#endif
{
   unsigned long ctrl2, ctrl3;

   ctrl2 = READ_VID32(SC1200_TVENC_TIM_CTRL_2);
   ctrl3 = READ_VID32(SC1200_TVENC_TIM_CTRL_3);
   ctrl2 &= ~(SC1200_TVENC_OUTPUT_YCBCR | SC1200_TVENC_CFS_MASK);
   ctrl3 &= ~(SC1200_TVENC_CM | SC1200_TVENC_SYNCMODE_MASK | SC1200_TVENC_CS);
   switch (output) {
   case TV_OUTPUT_COMPOSITE:
      /* Analog outputs provide Y, C and CVBS */
      /* Chrominance Lowpass filter is 1.3MHz (for composite video output) */
      WRITE_VID32(SC1200_TVENC_TIM_CTRL_2, ctrl2 | SC1200_TVENC_CFS_CVBS);
      WRITE_VID32(SC1200_TVENC_TIM_CTRL_3, ctrl3);
      break;
   case TV_OUTPUT_S_VIDEO:
      /* Analog outputs provide Y, C and CVBS */
      /* Chrominance Lowpass filter is 1.8MHz (for S-video output) */
      WRITE_VID32(SC1200_TVENC_TIM_CTRL_2, ctrl2 | SC1200_TVENC_CFS_SVIDEO);
      WRITE_VID32(SC1200_TVENC_TIM_CTRL_3, ctrl3);
      break;
   case TV_OUTPUT_YUV:
      /* Analog outputs provide Y, Cb and Cr */
      /* A 7.5 IRE setup is applied to the output */
      WRITE_VID32(SC1200_TVENC_TIM_CTRL_2,
		  ctrl2 | SC1200_TVENC_OUTPUT_YCBCR |
		  SC1200_TVENC_CFS_BYPASS);
      WRITE_VID32(SC1200_TVENC_TIM_CTRL_3,
		  ctrl3 | SC1200_TVENC_CM | SC1200_TVENC_CS);
      break;
   case TV_OUTPUT_SCART:
      /* Analog outputs provide SCART (RGB and CVBS) */
      /* Sync is added to green signal */
      WRITE_VID32(SC1200_TVENC_TIM_CTRL_2, ctrl2 | SC1200_TVENC_CFS_CVBS);
      WRITE_VID32(SC1200_TVENC_TIM_CTRL_3,
		  ctrl3 | SC1200_TVENC_CM | SC1200_TVENC_SYNC_ON_GREEN);
      break;
   default:
      return (GFX_STATUS_BAD_PARAMETER);
   }

   /* Adjusts the internal voltage reference */
   ctrl2 = READ_VID32(SC1200_TVENC_DAC_CONTROL);
   ctrl2 &= ~SC1200_TVENC_TRIM_MASK;

   /* Bypass for issue #926 : Inadequate chroma level of S-Video output */
   if ((gfx_chip_revision == SC1200_REV_B3) && (output == TV_OUTPUT_S_VIDEO))
      ctrl2 |= 0x7;
   else
      ctrl2 |= 0x5;

   WRITE_VID32(SC1200_TVENC_DAC_CONTROL, ctrl2);

   /* Disable 4:2:2 to 4:4:4 converter interpolation */
   WRITE_VID32(SC1200_TVOUT_DEBUG, SC1200_TVOUT_CONVERTER_INTERPOLATION);

   return (GFX_STATUS_OK);
}

/*-----------------------------------------------------------------------------
 * gfx_set_tv_enable
 *
 * This routine enables or disables the TV output.
 *-----------------------------------------------------------------------------
 */
#if GFX_TV_DYNAMIC
int
sc1200_set_tv_enable(int enable)
#else
int
gfx_set_tv_enable(int enable)
#endif
{
   unsigned long value_tim, value_dac;

   value_tim = READ_VID32(SC1200_TVENC_TIM_CTRL_1);
   value_dac = READ_VID32(SC1200_TVENC_DAC_CONTROL);

   if (enable) {
      value_tim |= SC1200_TVENC_VIDEO_TIMING_ENABLE;
      value_dac &= ~SC1200_TVENC_POWER_DOWN;
      /* ENABLE GRAPHICS DISPLAY LOGIC IN VIDEO PROCESSOR */
      gfx_set_screen_enable(1);
   } else {
      value_tim &= ~SC1200_TVENC_VIDEO_TIMING_ENABLE;
      value_dac |= SC1200_TVENC_POWER_DOWN;
      /* Do not disable the graphics display logic because it might be needed for CRT */
   }

   WRITE_VID32(SC1200_TVENC_TIM_CTRL_1, value_tim);
   WRITE_VID32(SC1200_TVENC_DAC_CONTROL, value_dac);

   return (GFX_STATUS_OK);
}

/*-----------------------------------------------------------------------------
 * gfx_set_tv_flicker_filter
 *
 * This routine configures the TV out flicker filter.
 *-----------------------------------------------------------------------------
 */
#if GFX_TV_DYNAMIC
int
sc1200_set_tv_flicker_filter(int ff)
#else
int
gfx_set_tv_flicker_filter(int ff)
#endif
{
   unsigned long mode;

   mode = READ_VID32(SC1200_TVOUT_HORZ_SCALING);
   mode &= ~SC1200_TVOUT_FLICKER_FILTER_MASK;
   switch (ff) {
   case TV_FLICKER_FILTER_NONE:
      WRITE_VID32(SC1200_TVOUT_HORZ_SCALING,
		  mode | SC1200_TVOUT_FLICKER_FILTER_DISABLED);
      break;
   case TV_FLICKER_FILTER_NORMAL:
      WRITE_VID32(SC1200_TVOUT_HORZ_SCALING,
		  mode | SC1200_TVOUT_FLICKER_FILTER_FOURTH_HALF_FOURTH);
      break;
   case TV_FLICKER_FILTER_INTERLACED:
      WRITE_VID32(SC1200_TVOUT_HORZ_SCALING,
		  mode | SC1200_TVOUT_FLICKER_FILTER_HALF_ONE_HALF);
      break;
   default:
      return GFX_STATUS_BAD_PARAMETER;
   }
   return (GFX_STATUS_OK);
}

/*-----------------------------------------------------------------------------
 * gfx_set_tv_sub_carrier_reset
 *
 * This routine configures the TV encoder sub carrier reset interval.
 *-----------------------------------------------------------------------------
 */
#if GFX_TV_DYNAMIC
int
sc1200_set_tv_sub_carrier_reset(int screset)
#else
int
gfx_set_tv_sub_carrier_reset(int screset)
#endif
{
   unsigned long mode;

   mode = READ_VID32(SC1200_TVENC_TIM_CTRL_1);
   mode &= ~SC1200_TVENC_SUB_CARRIER_RESET_MASK;
   switch (screset) {
   case TV_SUB_CARRIER_RESET_NEVER:
      WRITE_VID32(SC1200_TVENC_TIM_CTRL_1,
		  mode | SC1200_TVENC_SUB_CARRIER_RESET_NEVER);
      break;
   case TV_SUB_CARRIER_RESET_EVERY_TWO_LINES:
      WRITE_VID32(SC1200_TVENC_TIM_CTRL_1,
		  mode | SC1200_TVENC_SUB_CARRIER_RESET_EVERY_TWO_LINES);
      break;
   case TV_SUB_CARRIER_RESET_EVERY_TWO_FRAMES:
      WRITE_VID32(SC1200_TVENC_TIM_CTRL_1,
		  mode | SC1200_TVENC_SUB_CARRIER_RESET_EVERY_TWO_FRAMES);
      break;
   case TV_SUB_CARRIER_RESET_EVERY_FOUR_FRAMES:
      WRITE_VID32(SC1200_TVENC_TIM_CTRL_1,
		  mode | SC1200_TVENC_SUB_CARRIER_RESET_EVERY_FOUR_FRAMES);
      break;
   default:
      return GFX_STATUS_BAD_PARAMETER;
   }
   return (GFX_STATUS_OK);
}

/*-----------------------------------------------------------------------------
 * gfx_set_tv_vphase
 *
 * This routine sets the tv encoder VPHASE value.
 *-----------------------------------------------------------------------------
 */
#if GFX_TV_DYNAMIC
int
sc1200_set_tv_vphase(int vphase)
#else
int
gfx_set_tv_vphase(int vphase)
#endif
{
   unsigned long mode = READ_VID32(SC1200_TVENC_TIM_CTRL_1);

   mode &= ~SC1200_TVENC_VPHASE_MASK;
   mode |= (vphase << SC1200_TVENC_VPHASE_POS) & SC1200_TVENC_VPHASE_MASK;
   WRITE_VID32(SC1200_TVENC_TIM_CTRL_1, mode);
   return (GFX_STATUS_OK);
}

/*-----------------------------------------------------------------------------
 * gfx_set_tv_YC_delay
 *
 * This routine configures the TV out Y/C delay.
 *-----------------------------------------------------------------------------
 */
#if GFX_TV_DYNAMIC
int
sc1200_set_tv_YC_delay(int delay)
#else
int
gfx_set_tv_YC_delay(int delay)
#endif
{
   unsigned long mode;

   /* This feature is implemented in Rev C1 */
   if (gfx_chip_revision < SC1200_REV_C1)
      return (GFX_STATUS_OK);

   mode = READ_VID32(SC1200_TVOUT_HORZ_PRE_ENCODER_SCALE);
   mode &= ~SC1200_TVOUT_YC_DELAY_MASK;
   switch (delay) {
   case TV_YC_DELAY_NONE:
      WRITE_VID32(SC1200_TVOUT_HORZ_PRE_ENCODER_SCALE,
		  mode | SC1200_TVOUT_YC_DELAY_NONE);
      break;
   case TV_Y_DELAY_ONE_PIXEL:
      WRITE_VID32(SC1200_TVOUT_HORZ_PRE_ENCODER_SCALE,
		  mode | SC1200_TVOUT_Y_DELAY_ONE_PIXEL);
      break;
   case TV_C_DELAY_ONE_PIXEL:
      WRITE_VID32(SC1200_TVOUT_HORZ_PRE_ENCODER_SCALE,
		  mode | SC1200_TVOUT_C_DELAY_ONE_PIXEL);
      break;
   case TV_C_DELAY_TWO_PIXELS:
      WRITE_VID32(SC1200_TVOUT_HORZ_PRE_ENCODER_SCALE,
		  mode | SC1200_TVOUT_C_DELAY_TWO_PIXELS);
      break;
   default:
      return GFX_STATUS_BAD_PARAMETER;
   }
   return (GFX_STATUS_OK);
}

/*-----------------------------------------------------------------------------
 * gfx_set_tvenc_reset_interval
 *
 * This routine sets the interval between external resets of the TV encoder
 * timing generator by the TV out.
 *-----------------------------------------------------------------------------
 */
#if GFX_TV_DYNAMIC
int
sc1200_set_tvenc_reset_interval(int interval)
#else
int
gfx_set_tvenc_reset_interval(int interval)
#endif
{
   unsigned long value;

   value = READ_VID32(SC1200_TVOUT_HORZ_SCALING);
   value &= ~SC1200_TVENC_EXTERNAL_RESET_INTERVAL_MASK;
   switch (interval) {
   case TVENC_RESET_EVERY_ODD_FIELD:
      WRITE_VID32(SC1200_TVOUT_HORZ_SCALING,
		  value | SC1200_TVENC_EXTERNAL_RESET_EVERY_ODD_FIELD);
      break;
   case TVENC_RESET_EVERY_EVEN_FIELD:
      WRITE_VID32(SC1200_TVOUT_HORZ_SCALING,
		  value | SC1200_TVENC_EXTERNAL_RESET_EVERY_EVEN_FIELD);
      break;
   case TVENC_RESET_NEXT_ODD_FIELD:
      WRITE_VID32(SC1200_TVOUT_HORZ_SCALING,
		  value | SC1200_TVENC_EXTERNAL_RESET_NEXT_ODD_FIELD);
      break;
   case TVENC_RESET_NEXT_EVEN_FIELD:
      WRITE_VID32(SC1200_TVOUT_HORZ_SCALING,
		  value | SC1200_TVENC_EXTERNAL_RESET_NEXT_EVEN_FIELD);
      break;
   case TVENC_RESET_EVERY_FIELD:
      WRITE_VID32(SC1200_TVOUT_HORZ_SCALING,
		  value | SC1200_TVENC_EXTERNAL_RESET_EVERY_FIELD);
      break;
   case TVENC_RESET_EVERY_X_ODD_FIELDS:
   case TVENC_RESET_EVERY_X_EVEN_FIELDS:
      return GFX_STATUS_UNSUPPORTED;
   default:
      return GFX_STATUS_BAD_PARAMETER;
   }
   return (GFX_STATUS_OK);
}

/*-----------------------------------------------------------------------------
 * gfx_set_tv_cc_enable
 *
 * This routine enables or disables the use of the hardware CC registers 
 * in the TV encoder.
 *-----------------------------------------------------------------------------
 */
#if GFX_TV_DYNAMIC
int
sc1200_set_tv_cc_enable(int enable)
#else
int
gfx_set_tv_cc_enable(int enable)
#endif
{
   unsigned long value;

   value = READ_VID32(SC1200_TVENC_CC_CONTROL);
   value &= ~(0x0005F);
   if (enable)
      value |= 0x51;
   WRITE_VID32(SC1200_TVENC_CC_CONTROL, value);
   return (0);
}

/*---------------------------------------------------------------------------
 * gfx_set_tv_display
 *
 * This routine sets the timings in the display controller to support a 
 * TV resolution.
 *---------------------------------------------------------------------------
 */
#if GFX_TV_DYNAMIC
int
sc1200_set_tv_display(int width, int height)
#else
int
gfx_set_tv_display(int width, int height)
#endif
{
   DISPLAYMODE *pMode;
   unsigned int i;

   for (i = 0; i < NUM_TV_MODES; i++) {
      pMode = &TVTimings[i];
      if ((unsigned)width == pMode->hactive
	  && (unsigned)height == pMode->vactive)
	 break;
   }

   if (i == NUM_TV_MODES)
      return 0;

   gfx_set_display_timings(gfx_get_display_bpp(),
			   (unsigned short)pMode->flags, pMode->hactive,
			   pMode->hblankstart, pMode->hsyncstart,
			   pMode->hsyncend, pMode->hblankend, pMode->htotal,
			   pMode->vactive, pMode->vblankstart,
			   pMode->vsyncstart, pMode->vsyncend,
			   pMode->vblankend, pMode->vtotal, pMode->frequency);

   return 1;
}

/*-----------------------------------------------------------------------------
 * cc_add_parity_bit
 *
 * This routine adds the (odd) parity bit to the data character.
 *-----------------------------------------------------------------------------
 */
unsigned char
cc_add_parity_bit(unsigned char data)
{
   int i, num = 0;
   unsigned char d = data;

   for (i = 0; i < 7; i++) {
      if (d & 0x1)
	 num++;
      d >>= 1;
   }
   if (num & 0x1)
      return (data & ~0x80);
   else
      return (data | 0x80);
}

/*-----------------------------------------------------------------------------
 * gfx_set_tv_cc_data
 *
 * This routine writes the two specified characters to the CC data register 
 * of the TV encoder.
 *-----------------------------------------------------------------------------
 */
#if GFX_TV_DYNAMIC
int
sc1200_set_tv_cc_data(unsigned char data1, unsigned char data2)
#else
int
gfx_set_tv_cc_data(unsigned char data1, unsigned char data2)
#endif
{
   unsigned long value;

   value = cc_add_parity_bit(data1) | (cc_add_parity_bit(data2) << 8);
   WRITE_VID32(SC1200_TVENC_CC_DATA, value);
   return (0);
}

/*---------------------------------------------------------------------------
 * gfx_test_tvout_odd_field
 * 
 * This routine returns 1 if the current TVout field is odd. Otherwise returns 0.
 *---------------------------------------------------------------------------
 */
#if GFX_TV_DYNAMIC
int
sc1200_test_tvout_odd_field(void)
#else
int
gfx_test_tvout_odd_field(void)
#endif
{
   unsigned long debug = READ_VID32(SC1200_TVOUT_DEBUG);

   WRITE_VID32(SC1200_TVOUT_DEBUG, debug | SC1200_TVOUT_FIELD_STATUS_TV);
   if (READ_VID32(SC1200_TVOUT_DEBUG) & SC1200_TVOUT_FIELD_STATUS_EVEN)
      return (0);
   else
      return (1);
}

/*---------------------------------------------------------------------------
 * gfx_test_tvenc_odd_field
 * 
 * This routine returns 1 if the current TV encoder field is odd. Otherwise returns 0.
 *---------------------------------------------------------------------------
 */
#if GFX_TV_DYNAMIC
int
sc1200_test_tvenc_odd_field(void)
#else
int
gfx_test_tvenc_odd_field(void)
#endif
{
   unsigned long debug = READ_VID32(SC1200_TVOUT_DEBUG);

   WRITE_VID32(SC1200_TVOUT_DEBUG, debug & ~SC1200_TVOUT_FIELD_STATUS_TV);
   if (READ_VID32(SC1200_TVOUT_DEBUG) & SC1200_TVOUT_FIELD_STATUS_EVEN)
      return (0);
   else
      return (1);
}

/*-----------------------------------------------------------------------------
 * gfx_set_tv_field_status_invert
 *
 * This routines determines whether the tvout/tvencoder field status bit is
 * inverted (enable = 1) or not (enable = 0).
 *-----------------------------------------------------------------------------
 */
#if GFX_TV_DYNAMIC
int
sc1200_set_tv_field_status_invert(int enable)
#else
int
gfx_set_tv_field_status_invert(int enable)
#endif
{
   unsigned long value;

   value = READ_VID32(SC1200_TVOUT_DEBUG);

   if (enable) {
      value |= SC1200_TVOUT_FIELD_STATUS_INVERT;
   } else {
      value &= ~(SC1200_TVOUT_FIELD_STATUS_INVERT);
   }

   WRITE_VID32(SC1200_TVOUT_DEBUG, value);

   return (GFX_STATUS_OK);
}

/*---------------------------------------------------------------------------
 * gfx_get_tv_vphase
 * 
 * This routine returns the tv encoder vertical phase.
 *---------------------------------------------------------------------------
 */
#if GFX_TV_DYNAMIC
int
sc1200_get_tv_vphase(void)
#else
int
gfx_get_tv_vphase(void)
#endif
{
   unsigned long mode = READ_VID32(SC1200_TVENC_TIM_CTRL_1);

   return (int)((mode & SC1200_TVENC_VPHASE_MASK) >> SC1200_TVENC_VPHASE_POS);
}

/*---------------------------------------------------------------------------
 * gfx_get_tv_enable
 * 
 * This routine returns the current tv enable status
 *---------------------------------------------------------------------------
 */
#if GFX_TV_DYNAMIC
int
sc1200_get_tv_enable(unsigned int *p_on)
#else
int
gfx_get_tv_enable(unsigned int *p_on)
#endif
{
   unsigned long control = READ_VID32(SC1200_TVENC_DAC_CONTROL);

   *p_on = (unsigned int)(!(control & SC1200_TVENC_POWER_DOWN));

   return GFX_STATUS_OK;
}

/*---------------------------------------------------------------------------
 * gfx_get_tv_output
 * 
 * This routine returns the current programmed TV output type.  It does not
 * detect invalid configurations.
 *---------------------------------------------------------------------------
 */
#if GFX_TV_DYNAMIC
int
sc1200_get_tv_output(void)
#else
int
gfx_get_tv_output(void)
#endif
{
   unsigned long ctrl2, ctrl3;
   int format = 0;

   ctrl2 = READ_VID32(SC1200_TVENC_TIM_CTRL_2);
   ctrl3 = READ_VID32(SC1200_TVENC_TIM_CTRL_3);

   if ((ctrl2 & SC1200_TVENC_CFS_MASK) == SC1200_TVENC_CFS_SVIDEO)
      format = TV_OUTPUT_S_VIDEO;
   else if (ctrl2 & SC1200_TVENC_OUTPUT_YCBCR)
      format = TV_OUTPUT_YUV;
   else if ((ctrl2 & SC1200_TVENC_CFS_MASK) == SC1200_TVENC_CFS_CVBS) {
      if (ctrl3 & SC1200_TVENC_CM)
	 format = TV_OUTPUT_SCART;
      else
	 format = TV_OUTPUT_COMPOSITE;
   }

   return format;
}

/*---------------------------------------------------------------------------
 * gfx_get_tv_mode_count
 * 
 * This routine returns the number of valid TV out resolutions.
 *---------------------------------------------------------------------------
 */
#if GFX_TV_DYNAMIC
int
sc1200_get_tv_mode_count(TVStandardType format)
#else
int
gfx_get_tv_mode_count(TVStandardType format)
#endif
{
   unsigned int mode, count = 0;
   unsigned long flag;

   switch (format) {
   case TV_STANDARD_NTSC:
      flag = GFX_MODE_TV_NTSC;
      break;
   case TV_STANDARD_PAL:
      flag = GFX_MODE_TV_PAL;
      break;
   default:
      return 0;
   }

   for (mode = 0; mode < NUM_TV_MODES; mode++) {
      if (TVTimings[mode].flags & flag)
	 count++;
   }

   return count;
}

/*---------------------------------------------------------------------------
 * gfx_get_tv_display_mode
 * 
 * This routine returns the current TV display parameters.
 *---------------------------------------------------------------------------
 */
#if GFX_TV_DYNAMIC
int
sc1200_get_tv_display_mode(int *width, int *height, int *bpp, int *hz)
#else
int
gfx_get_tv_display_mode(int *width, int *height, int *bpp, int *hz)
#endif
{
   unsigned long frequency;
   unsigned long mode, flags;

   *width = gfx_get_hactive();
   *height = gfx_get_vactive();
   *bpp = gfx_get_display_bpp();
   frequency = gfx_get_clock_frequency();

   for (mode = 0; mode < NUM_TV_MODES; mode++) {
      if (TVTimings[mode].hactive == (unsigned short)(*width) &&
	  TVTimings[mode].vactive == (unsigned short)(*height) &&
	  TVTimings[mode].frequency == frequency) {
	 flags = TVTimings[mode].flags;

	 if (flags & GFX_MODE_TV_NTSC)
	    *hz = 60;
	 else if (flags & GFX_MODE_TV_PAL)
	    *hz = 50;
	 else
	    *hz = 0;
	 return (1);
      }
   }

   return -1;
}

/*---------------------------------------------------------------------------
 * gfx_get_tv_display_mode_frequency
 *
 * This routine returns the PLL frequency of a given TV mode.
 *---------------------------------------------------------------------------
 */
#if GFX_TV_DYNAMIC
int
sc1200_get_tv_display_mode_frequency(unsigned short width,
				     unsigned short height,
				     TVStandardType format, int *frequency)
#else
int
gfx_get_tv_display_mode_frequency(unsigned short width, unsigned short height,
				  TVStandardType format, int *frequency)
#endif
{
   unsigned long mode, flag;
   int retval = -1;

   *frequency = 0;

   switch (format) {
   case TV_STANDARD_NTSC:
      flag = GFX_MODE_TV_NTSC;
      break;
   case TV_STANDARD_PAL:
      flag = GFX_MODE_TV_PAL;
      break;
   default:
      return -1;
   }

   for (mode = 0; mode < NUM_TV_MODES; mode++) {
      if ((TVTimings[mode].hactive == width) &&
	  (TVTimings[mode].vactive == height) &&
	  (TVTimings[mode].flags & flag)) {
	 *frequency = TVTimings[mode].frequency;
	 retval = 1;
      }
   }
   return retval;
}

/*---------------------------------------------------------------------------
 * gfx_is_tv_display_mode_supported
 * 
 * Returns >= 0 if the mode is available, -1 if the mode could not be found
 *---------------------------------------------------------------------------
 */
#if GFX_TV_DYNAMIC
int
sc1200_is_tv_display_mode_supported(unsigned short width,
				    unsigned short height,
				    TVStandardType format)
#else
int
gfx_is_tv_display_mode_supported(unsigned short width, unsigned short height,
				 TVStandardType format)
#endif
{
   unsigned long mode, flag;

   switch (format) {
   case TV_STANDARD_NTSC:
      flag = GFX_MODE_TV_NTSC;
      break;
   case TV_STANDARD_PAL:
      flag = GFX_MODE_TV_PAL;
      break;
   default:
      return -1;
   }

   for (mode = 0; mode < NUM_TV_MODES; mode++) {
      if (TVTimings[mode].hactive == width &&
	  TVTimings[mode].vactive == height &&
	  (TVTimings[mode].flags & flag)) {
	 return ((int)mode);
      }
   }

   return -1;
}

/* END OF FILE */

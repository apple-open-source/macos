/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/nsc/gfx/gfx_dcdr.c,v 1.1 2002/12/10 15:12:25 alanh Exp $ */
/*
 * $Workfile: gfx_dcdr.c $
 *
 * This file contains routines to control the video decoder.
 * 
 *    gfx_set_decoder_defaults
 *    gfx_set_decoder_analog_input
 *    gfx_set_decoder_brightness
 *    gfx_set_decoder_contrast	
 *    gfx_set_decoder_luminance_filter
 *    gfx_set_decoder_hue
 *    gfx_set_decoder_saturation
 *    gfx_set_decoder_input_offset
 *    gfx_set_decoder_input_size
 *    gfx_set_decoder_output_size
 *    gfx_set_decoder_scale
 *    gfx_set_decoder_TV_standard
 *    gfx_set_decoder_vbi_enable
 *    gfx_set_decoder_vbi_format
 *    gfx_set_decoder_vbi_upscale
 *    gfx_decoder_software_reset
 *    gfx_decoder_detect_macrovision
 *    gfx_decoder_detect_video
 *
 * And the following routines if GFX_READ_ROUTINES is set:
 *
 *    gfx_get_decoder_brightness
 *    gfx_get_decoder_contrast
 *    gfx_get_decoder_hue
 *    gfx_get_decoder_saturation
 *    gfx_get_decoder_input_offset
 *    gfx_get_decoder_input_size
 *    gfx_get_decoder_output_size
 *    gfx_get_decoder_vbi_format
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

/* INCLUDE SUPPORT FOR PHILIPS SAA7114 DECODER, IF SPECIFIED */

#if GFX_DECODER_SAA7114
#include "saa7114.c"
#endif

/* WRAPPERS IF DYNAMIC SELECTION */
/* Extra layer to call various decoders.  Currently only the Pillips */
/* decoder is supported, but still organized to easily expand later. */

#if GFX_DECODER_DYNAMIC

/*-----------------------------------------------------------------------------
 * gfx_set_decoder_defaults
 *-----------------------------------------------------------------------------
 */
int
gfx_set_decoder_defaults(void)
{
   int status = GFX_STATUS_UNSUPPORTED;

#	if	GFX_DECODER_SAA7114
   if (gfx_decoder_type == GFX_DECODER_SAA7114)
      status = saa7114_set_decoder_defaults();
#	endif
   return (status);
}

/*-----------------------------------------------------------------------------
 * gfx_set_decoder_analog_input
 *-----------------------------------------------------------------------------
 */
int
gfx_set_decoder_analog_input(unsigned char input)
{
   int status = GFX_STATUS_UNSUPPORTED;

#	if	GFX_DECODER_SAA7114
   if (gfx_decoder_type == GFX_DECODER_SAA7114)
      status = saa7114_set_decoder_analog_input(input);
#	endif
   return (status);
}

/*-----------------------------------------------------------------------------
 * gfx_set_decoder_brightness
 *-----------------------------------------------------------------------------
 */
int
gfx_set_decoder_brightness(unsigned char brightness)
{
   int status = GFX_STATUS_UNSUPPORTED;

#	if	GFX_DECODER_SAA7114
   if (gfx_decoder_type == GFX_DECODER_SAA7114)
      status = saa7114_set_decoder_brightness(brightness);
#	endif
   return (status);
}

/*-----------------------------------------------------------------------------
 * gfx_set_decoder_contrast
 *-----------------------------------------------------------------------------
 */
int
gfx_set_decoder_contrast(unsigned char contrast)
{
   int status = GFX_STATUS_UNSUPPORTED;

#	if	GFX_DECODER_SAA7114
   if (gfx_decoder_type == GFX_DECODER_SAA7114)
      status = saa7114_set_decoder_contrast(contrast);
#	endif
   return (status);
}

/*-----------------------------------------------------------------------------
 * gfx_set_decoder_hue
 *-----------------------------------------------------------------------------
 */
int
gfx_set_decoder_hue(char hue)
{
   int status = GFX_STATUS_UNSUPPORTED;

#	if	GFX_DECODER_SAA7114
   if (gfx_decoder_type == GFX_DECODER_SAA7114)
      status = saa7114_set_decoder_hue(hue);
#	endif
   return (status);
}

/*-----------------------------------------------------------------------------
 * gfx_set_decoder_saturation
 *-----------------------------------------------------------------------------
 */
int
gfx_set_decoder_saturation(unsigned char saturation)
{
   int status = GFX_STATUS_UNSUPPORTED;

#	if	GFX_DECODER_SAA7114
   if (gfx_decoder_type == GFX_DECODER_SAA7114)
      status = saa7114_set_decoder_saturation(saturation);
#	endif
   return (status);
}

/*-----------------------------------------------------------------------------
 * gfx_set_decoder_input_offset
 *-----------------------------------------------------------------------------
 */
int
gfx_set_decoder_input_offset(unsigned short x, unsigned short y)
{
   int status = GFX_STATUS_UNSUPPORTED;

#	if	GFX_DECODER_SAA7114
   if (gfx_decoder_type == GFX_DECODER_SAA7114)
      status = saa7114_set_decoder_input_offset(x, y);
#	endif
   return (status);
}

/*-----------------------------------------------------------------------------
 * gfx_set_decoder_input_size
 *-----------------------------------------------------------------------------
 */
int
gfx_set_decoder_input_size(unsigned short width, unsigned short height)
{
   int status = GFX_STATUS_UNSUPPORTED;

#	if	GFX_DECODER_SAA7114
   if (gfx_decoder_type == GFX_DECODER_SAA7114)
      status = saa7114_set_decoder_input_size(width, height);
#	endif
   return (status);
}

/*-----------------------------------------------------------------------------
 * gfx_set_decoder_output_size
 *-----------------------------------------------------------------------------
 */
int
gfx_set_decoder_output_size(unsigned short width, unsigned short height)
{
   int status = GFX_STATUS_UNSUPPORTED;

#	if	GFX_DECODER_SAA7114
   if (gfx_decoder_type == GFX_DECODER_SAA7114)
      status = saa7114_set_decoder_output_size(width, height);
#	endif
   return (status);
}

/*-----------------------------------------------------------------------------
 * gfx_set_decoder_scale
 *-----------------------------------------------------------------------------
 */
int
gfx_set_decoder_scale(unsigned short srcw, unsigned short srch,
		      unsigned short dstw, unsigned short dsth)
{
   int status = GFX_STATUS_UNSUPPORTED;

#	if	GFX_DECODER_SAA7114
   if (gfx_decoder_type == GFX_DECODER_SAA7114)
      status = saa7114_set_decoder_scale(srcw, srch, dstw, dsth);
#	endif
   return (status);
}

/*-----------------------------------------------------------------------------
 * gfx_set_decoder_vbi_format
 *-----------------------------------------------------------------------------
 */
int
gfx_set_decoder_vbi_format(int start, int end, int format)
{
   int status = GFX_STATUS_UNSUPPORTED;

#	if	GFX_DECODER_SAA7114
   if (gfx_decoder_type == GFX_DECODER_SAA7114)
      status = saa7114_set_decoder_vbi_format(start, end, format);
#	endif
   return (status);
}

/*-----------------------------------------------------------------------------
 * gfx_set_decoder_vbi_enable
 *-----------------------------------------------------------------------------
 */
int
gfx_set_decoder_vbi_enable(int enable)
{
   int status = GFX_STATUS_UNSUPPORTED;

#	if	GFX_DECODER_SAA7114
   if (gfx_decoder_type == GFX_DECODER_SAA7114)
      status = saa7114_set_decoder_vbi_enable(enable);
#	endif
   return (status);
}

/*-----------------------------------------------------------------------------
 * gfx_set_decoder_vbi_upscale
 *-----------------------------------------------------------------------------
 */
int
gfx_set_decoder_vbi_upscale(void)
{
   int status = GFX_STATUS_UNSUPPORTED;

#	if	GFX_DECODER_SAA7114
   if (gfx_decoder_type == GFX_DECODER_SAA7114)
      status = saa7114_set_decoder_vbi_upscale();
#	endif
   return (status);
}

/*-----------------------------------------------------------------------------
 * gfx_set_decoder_TV_standard
 *-----------------------------------------------------------------------------
 */
int
gfx_set_decoder_TV_standard(TVStandardType TVStandard)
{
   int status = GFX_STATUS_UNSUPPORTED;

#	if	GFX_DECODER_SAA7114
   if (gfx_decoder_type == GFX_DECODER_SAA7114)
      status = saa7114_set_decoder_TV_standard(TVStandard);
#	endif
   return (status);
}

/*-----------------------------------------------------------------------------
 * gfx_set_decoder_luminance_filter
 *-----------------------------------------------------------------------------
 */
int
gfx_set_decoder_luminance_filter(unsigned char lufi)
{
   int status = GFX_STATUS_UNSUPPORTED;

#	if	GFX_DECODER_SAA7114
   if (gfx_decoder_type == GFX_DECODER_SAA7114)
      status = saa7114_set_decoder_luminance_filter(lufi);
#	endif
   return (status);
}

/*-----------------------------------------------------------------------------
 * gfx_decoder_software_reset
 *-----------------------------------------------------------------------------
 */
int
gfx_decoder_software_reset(void)
{
   int status = GFX_STATUS_UNSUPPORTED;

#	if	GFX_DECODER_SAA7114
   if (gfx_decoder_type == GFX_DECODER_SAA7114)
      status = saa7114_decoder_software_reset();
#	endif
   return (status);
}

/*-----------------------------------------------------------------------------
 * gfx_decoder_detect_macrovision
 *-----------------------------------------------------------------------------
 */
int
gfx_decoder_detect_macrovision(void)
{
   int status = GFX_STATUS_UNSUPPORTED;

#	if	GFX_DECODER_SAA7114
   if (gfx_decoder_type == GFX_DECODER_SAA7114)
      status = saa7114_decoder_detect_macrovision();
#	endif
   return (status);
}

/*-----------------------------------------------------------------------------
 * gfx_decoder_detect_video
 *-----------------------------------------------------------------------------
 */
int
gfx_decoder_detect_video(void)
{
   int status = GFX_STATUS_UNSUPPORTED;

#	if	GFX_DECODER_SAA7114
   if (gfx_decoder_type == GFX_DECODER_SAA7114)
      status = saa7114_decoder_detect_video();
#	endif
   return (status);
}

/*************************************************************/
/*  READ ROUTINES  |  INCLUDED FOR DIAGNOSTIC PURPOSES ONLY  */
/*************************************************************/

#if GFX_READ_ROUTINES

/*-----------------------------------------------------------------------------
 * gfx_get_decoder_brightness
 *-----------------------------------------------------------------------------
 */
unsigned char
gfx_get_decoder_brightness(void)
{
   unsigned char brightness = 0;

#	if	GFX_DECODER_SAA7114
   if (gfx_decoder_type == GFX_DECODER_SAA7114)
      brightness = saa7114_get_decoder_brightness();
#	endif
   return (brightness);
}

/*-----------------------------------------------------------------------------
 * gfx_get_decoder_contrast
 *-----------------------------------------------------------------------------
 */
unsigned char
gfx_get_decoder_contrast(void)
{
   unsigned char contrast = 0;

#	if	GFX_DECODER_SAA7114
   if (gfx_decoder_type == GFX_DECODER_SAA7114)
      contrast = saa7114_get_decoder_contrast();
#	endif
   return (contrast);
}

/*-----------------------------------------------------------------------------
 * gfx_get_decoder_hue
 *-----------------------------------------------------------------------------
 */
char
gfx_get_decoder_hue(void)
{
   unsigned char hue = 0;

#	if	GFX_DECODER_SAA7114
   if (gfx_decoder_type == GFX_DECODER_SAA7114)
      hue = saa7114_get_decoder_hue();
#	endif
   return ((char)hue);
}

/*-----------------------------------------------------------------------------
 * gfx_get_decoder_saturation
 *-----------------------------------------------------------------------------
 */
unsigned char
gfx_get_decoder_saturation(void)
{
   unsigned char saturation = 0;

#	if	GFX_DECODER_SAA7114
   if (gfx_decoder_type == GFX_DECODER_SAA7114)
      saturation = saa7114_get_decoder_saturation();
#	endif
   return (saturation);
}

/*-----------------------------------------------------------------------------
 * gfx_get_decoder_input_offset
 *-----------------------------------------------------------------------------
 */
unsigned long
gfx_get_decoder_input_offset()
{
   unsigned long offset = 0;

#	if	GFX_DECODER_SAA7114
   if (gfx_decoder_type == GFX_DECODER_SAA7114)
      offset = saa7114_get_decoder_input_offset();
#	endif
   return (offset);
}

/*-----------------------------------------------------------------------------
 * gfx_get_decoder_input_size
 *-----------------------------------------------------------------------------
 */
unsigned long
gfx_get_decoder_input_size()
{
   unsigned long size = 0;

#	if	GFX_DECODER_SAA7114
   if (gfx_decoder_type == GFX_DECODER_SAA7114)
      size = saa7114_get_decoder_input_size();
#	endif
   return (size);
}

/*-----------------------------------------------------------------------------
 * gfx_get_decoder_output_size
 *-----------------------------------------------------------------------------
 */
unsigned long
gfx_get_decoder_output_size()
{
   unsigned long size = 0;

#	if	GFX_DECODER_SAA7114
   if (gfx_decoder_type == GFX_DECODER_SAA7114)
      size = saa7114_get_decoder_output_size();
#	endif
   return (size);
}

/*-----------------------------------------------------------------------------
 * gfx_get_decoder_vbi_format
 *-----------------------------------------------------------------------------
 */
int
gfx_get_decoder_vbi_format(int line)
{
   int format = 0;

#	if	GFX_DECODER_SAA7114
   if (gfx_decoder_type == GFX_DECODER_SAA7114)
      format = saa7114_get_decoder_vbi_format(line);
#	endif
   return (format);
}

#endif /* GFX_READ_ROUTINES */

#endif /* GFX_DECODER_DYNAMIC */

/* END OF FILE */

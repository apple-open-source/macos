/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/nsc/gfx/tv_fs451.c,v 1.2 2003/11/03 05:11:23 tsi Exp $ */
/*
 * $Workfile: tv_fs451.c $
 * $Revision: 1.1.1.2 $
 *
 * This file contains routines to control the FS451 tvout encoder.
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

/*-----------------------------------------------------------------------------
 * gfx_set_tv_format
 *
 * This routine sets the TV encoder registers to the specified format
 * and resolution.
 * Currently only NTSC 640x480 is supported.
 *-----------------------------------------------------------------------------
 */
#if GFX_TV_DYNAMIC
int
fs451_set_tv_format(TVStandardType format, GfxOnTVType resolution)
#else
int
gfx_set_tv_format(TVStandardType format, GfxOnTVType resolution)
#endif
{
   /* ### ADD ### IMPLEMENTATION */
   return (0);
}

/*-----------------------------------------------------------------------------
 * gfx_set_tv_output
 *
 * This routine sets the TV encoder registers to the specified output type.
 * Supported output types are : S-VIDEO and Composite.
 *-----------------------------------------------------------------------------
 */
#if GFX_TV_DYNAMIC
int
fs451_set_tv_output(int output)
#else
int
gfx_set_tv_output(int output)
#endif
{
   /* ### ADD ### IMPLEMENTATION */
   return (0);
}

/*-----------------------------------------------------------------------------
 * gfx_set_tv_defaults
 *
 * This routine sets all of the TV encoder registers to default values for 
 * the specified format.  Currently only NTSC is supported.
 *-----------------------------------------------------------------------------
 */
#if GFX_TV_DYNAMIC
int
fs451_set_tv_defaults(int format)
#else
int
gfx_set_tv_defaults(int format)
#endif
{
   /* ### ADD ### IMPLEMENTATION */
   return (0);
}

/*-----------------------------------------------------------------------------
 * gfx_set_tv_enable
 *
 * This routine enables or disables the TV output.
 *-----------------------------------------------------------------------------
 */
#if GFX_TV_DYNAMIC
int
fs451_set_tv_enable(int enable)
#else
int
gfx_set_tv_enable(int enable)
#endif
{
   /* ### ADD ### IMPLEMENTATION */
   return (0);
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
fs451_set_tv_cc_enable(int enable)
#else
int
gfx_set_tv_cc_enable(int enable)
#endif
{
   /* ### ADD ### IMPLEMENTATION */
   return (0);
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
fs451_set_tv_cc_data(unsigned char data1, unsigned char data2)
#else
int
gfx_set_tv_cc_data(unsigned char data1, unsigned char data2)
#endif
{
   /* ### ADD ### IMPLEMENTATION */
   return (0);
}

/* END OF FILE */

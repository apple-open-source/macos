/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/nsc/gfx/gfx_vip.c,v 1.1 2002/12/10 15:12:26 alanh Exp $ */
/*
 * $Workfile: gfx_vip.c $
 *
 * This file contains routines to control the video input port (VIP).
 * 
 *    gfx_set_vip_enable
 *    gfx_set_vip_capture_run_mode
 *    gfx_set_vip_base
 *    gfx_set_vip_pitch
 *    gfx_set_vip_mode
 *    gfx_set_vbi_enable
 *    gfx_set_vbi_mode
 *    gfx_set_vbi_base
 *    gfx_set_vbi_pitch
 *    gfx_set_vbi_direct
 *    gfx_set_vbi_interrupt
 *    gfx_set_vip_bus_request_threshold_high
 *    gfx_set_vip_last_line
 *    gfx_test_vip_odd_field
 *    gfx_test_vip_bases_updated
 *    gfx_test_vip_fifo_overflow
 *    gfx_get_vip_line
 *    gfx_get_vip_base
 *    gfx_get_vbi_pitch
 *
 * And the following routines if GFX_READ_ROUTINES is set:
 *
 *    gfx_get_vip_enable
 *    gfx_get_vip_pitch
 *    gfx_get_vip_mode
 *    gfx_get_vbi_enable
 *    gfx_get_vbi_mode
 *    gfx_get_vbi_base
 *    gfx_get_vbi_direct
 *    gfx_get_vbi_interrupt
 *    gfx_get_vip_bus_request_threshold_high
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

/* INCLUDE SUPPORT FOR SC1200, IF SPECIFIED. */

#if GFX_VIP_SC1200
#include "vip_1200.c"
#endif

/* WRAPPERS IF DYNAMIC SELECTION */
/* Extra layer to call either CS5530 or SC1200 routines. */

#if GFX_VIP_DYNAMIC

/*-----------------------------------------------------------------------------
 * gfx_set_vip_enable
 *-----------------------------------------------------------------------------
 */
int
gfx_set_vip_enable(int enable)
{
   int status = GFX_STATUS_UNSUPPORTED;

#	if GFX_VIP_SC1200
   if (gfx_vip_type == GFX_VIP_TYPE_SC1200)
      status = sc1200_set_vip_enable(enable);
#	endif
   return (status);
}

/*-----------------------------------------------------------------------------
 * gfx_set_vip_capture_run_mode
 *-----------------------------------------------------------------------------
 */
int
gfx_set_vip_capture_run_mode(int mode)
{
   int status = GFX_STATUS_UNSUPPORTED;

#	if GFX_VIP_SC1200
   if (gfx_vip_type == GFX_VIP_TYPE_SC1200)
      status = sc1200_set_vip_capture_run_mode(mode);
#	endif
   return (status);
}

/*-----------------------------------------------------------------------------
 * gfx_set_vip_base
 *-----------------------------------------------------------------------------
 */
int
gfx_set_vip_base(unsigned long even, unsigned long odd)
{
   int status = GFX_STATUS_UNSUPPORTED;

#	if GFX_VIP_SC1200
   if (gfx_vip_type == GFX_VIP_TYPE_SC1200)
      status = sc1200_set_vip_base(even, odd);
#	endif
   return (status);
}

/*-----------------------------------------------------------------------------
 * gfx_set_vip_pitch
 *-----------------------------------------------------------------------------
 */
int
gfx_set_vip_pitch(unsigned long pitch)
{
   int status = GFX_STATUS_UNSUPPORTED;

#	if GFX_VIP_SC1200
   if (gfx_vip_type == GFX_VIP_TYPE_SC1200)
      status = sc1200_set_vip_pitch(pitch);
#	endif
   return (status);
}

/*-----------------------------------------------------------------------------
 * gfx_set_vip_mode
 *-----------------------------------------------------------------------------
 */
int
gfx_set_vip_mode(int mode)
{
   int status = GFX_STATUS_UNSUPPORTED;

#	if GFX_VIP_SC1200
   if (gfx_vip_type == GFX_VIP_TYPE_SC1200)
      status = sc1200_set_vip_mode(mode);
#	endif
   return (status);
}

/*-----------------------------------------------------------------------------
 * gfx_set_vbi_enable
 *-----------------------------------------------------------------------------
 */
int
gfx_set_vbi_enable(int enable)
{
   int status = GFX_STATUS_UNSUPPORTED;

#	if GFX_VIP_SC1200
   if (gfx_vip_type == GFX_VIP_TYPE_SC1200)
      status = sc1200_set_vbi_enable(enable);
#	endif
   return (status);
}

/*-----------------------------------------------------------------------------
 * gfx_set_vbi_mode
 *-----------------------------------------------------------------------------
 */
int
gfx_set_vbi_mode(int mode)
{
   int status = GFX_STATUS_UNSUPPORTED;

#	if GFX_VIP_SC1200
   if (gfx_vip_type == GFX_VIP_TYPE_SC1200)
      status = sc1200_set_vbi_mode(mode);
#	endif
   return (status);
}

/*-----------------------------------------------------------------------------
 * gfx_set_vbi_base
 *-----------------------------------------------------------------------------
 */
int
gfx_set_vbi_base(unsigned long even, unsigned long odd)
{
   int status = GFX_STATUS_UNSUPPORTED;

#	if GFX_VIP_SC1200
   if (gfx_vip_type == GFX_VIP_TYPE_SC1200)
      status = sc1200_set_vbi_base(even, odd);
#	endif
   return (status);
}

/*-----------------------------------------------------------------------------
 * gfx_set_vbi_pitch
 *-----------------------------------------------------------------------------
 */
int
gfx_set_vbi_pitch(unsigned long pitch)
{
   int status = GFX_STATUS_UNSUPPORTED;

#	if GFX_VIP_SC1200
   if (gfx_vip_type == GFX_VIP_TYPE_SC1200)
      status = sc1200_set_vbi_pitch(pitch);
#	endif
   return (status);
}

/*-----------------------------------------------------------------------------
 * gfx_set_vbi_direct
 *-----------------------------------------------------------------------------
 */
int
gfx_set_vbi_direct(unsigned long even_lines, unsigned long odd_lines)
{
   int status = GFX_STATUS_UNSUPPORTED;

#	if GFX_VIP_SC1200
   if (gfx_vip_type == GFX_VIP_TYPE_SC1200)
      status = sc1200_set_vbi_direct(even_lines, odd_lines);
#	endif
   return (status);
}

/*-----------------------------------------------------------------------------
 * gfx_set_vbi_interrupt
 *-----------------------------------------------------------------------------
 */
int
gfx_set_vbi_interrupt(int enable)
{
   int status = GFX_STATUS_UNSUPPORTED;

#	if GFX_VIP_SC1200
   if (gfx_vip_type == GFX_VIP_TYPE_SC1200)
      status = sc1200_set_vbi_interrupt(enable);
#	endif
   return (status);
}

/*-----------------------------------------------------------------------------
 * gfx_set_vip_bus_request_threshold_high
 *-----------------------------------------------------------------------------
 */
int
gfx_set_vip_bus_request_threshold_high(int enable)
{
   int status = GFX_STATUS_UNSUPPORTED;

#	if GFX_VIP_SC1200
   if (gfx_vip_type == GFX_VIP_TYPE_SC1200)
      status = sc1200_set_vip_bus_request_threshold_high(enable);
#	endif
   return (status);
}

/*-----------------------------------------------------------------------------
 * gfx_set_vip_last_line
 *-----------------------------------------------------------------------------
 */
int
gfx_set_vip_last_line(int last_line)
{
   int status = GFX_STATUS_UNSUPPORTED;

#	if GFX_VIP_SC1200
   if (gfx_vip_type == GFX_VIP_TYPE_SC1200)
      status = sc1200_set_vip_last_line(last_line);
#	endif
   return (status);
}

/*-----------------------------------------------------------------------------
 * gfx_test_vip_odd_field
 *-----------------------------------------------------------------------------
 */
int
gfx_test_vip_odd_field(void)
{
   int status = GFX_STATUS_UNSUPPORTED;

#	if GFX_VIP_SC1200
   if (gfx_vip_type == GFX_VIP_TYPE_SC1200)
      status = sc1200_test_vip_odd_field();
#	endif
   return (status);
}

/*-----------------------------------------------------------------------------
 * gfx_test_vip_bases_updated
 *-----------------------------------------------------------------------------
 */
int
gfx_test_vip_bases_updated(void)
{
   int status = GFX_STATUS_UNSUPPORTED;

#	if GFX_VIP_SC1200
   if (gfx_vip_type == GFX_VIP_TYPE_SC1200)
      status = sc1200_test_vip_bases_updated();
#	endif
   return (status);
}

/*-----------------------------------------------------------------------------
 * gfx_test_vip_fifo_overflow
 *-----------------------------------------------------------------------------
 */
int
gfx_test_vip_fifo_overflow(void)
{
   int status = GFX_STATUS_UNSUPPORTED;

#	if GFX_VIP_SC1200
   if (gfx_vip_type == GFX_VIP_TYPE_SC1200)
      status = sc1200_test_vip_fifo_overflow();
#	endif
   return (status);
}

/*-----------------------------------------------------------------------------
 * gfx_get_vip_line
 *-----------------------------------------------------------------------------
 */
int
gfx_get_vip_line(void)
{
   int status = GFX_STATUS_UNSUPPORTED;

#	if GFX_VIP_SC1200
   if (gfx_vip_type == GFX_VIP_TYPE_SC1200)
      status = sc1200_get_vip_line();
#	endif
   return (status);
}

/*-----------------------------------------------------------------------------
 * gfx_get_vip_base
 *-----------------------------------------------------------------------------
 */
unsigned long
gfx_get_vip_base(int odd)
{
   unsigned long base = 0;

#	if GFX_VIP_SC1200
   if (gfx_vip_type == GFX_VIP_TYPE_SC1200)
      base = sc1200_get_vip_base(odd);
#	endif
   return (base);
}

/*-----------------------------------------------------------------------------
 * gfx_get_vbi_pitch
 *-----------------------------------------------------------------------------
 */
unsigned long
gfx_get_vbi_pitch(void)
{
   unsigned long pitch = 0;

#	if GFX_VIP_SC1200
   if (gfx_vip_type == GFX_VIP_TYPE_SC1200)
      pitch = sc1200_get_vbi_pitch();
#	endif
   return (pitch);
}

/*************************************************************/
/*  READ ROUTINES  |  INCLUDED FOR DIAGNOSTIC PURPOSES ONLY  */
/*************************************************************/

#if GFX_READ_ROUTINES

/*-----------------------------------------------------------------------------
 * gfx_get_vip_enable
 *-----------------------------------------------------------------------------
 */
int
gfx_get_vip_enable(void)
{
   int enable = 0;

#	if GFX_VIP_SC1200
   if (gfx_vip_type == GFX_VIP_TYPE_SC1200)
      enable = sc1200_get_vip_enable();
#	endif
   return (enable);
}

/*-----------------------------------------------------------------------------
 * gfx_get_vip_pitch
 *-----------------------------------------------------------------------------
 */
unsigned long
gfx_get_vip_pitch(void)
{
   unsigned long pitch = 0;

#	if GFX_VIP_SC1200
   if (gfx_vip_type == GFX_VIP_TYPE_SC1200)
      pitch = sc1200_get_vip_pitch();
#	endif
   return (pitch);
}

/*-----------------------------------------------------------------------------
 * gfx_get_vip_mode
 *-----------------------------------------------------------------------------
 */
int
gfx_get_vip_mode(void)
{
   int mode = 0;

#	if GFX_VIP_SC1200
   if (gfx_vip_type == GFX_VIP_TYPE_SC1200)
      mode = sc1200_get_vip_mode();
#	endif
   return (mode);
}

/*-----------------------------------------------------------------------------
 * gfx_get_vbi_enable
 *-----------------------------------------------------------------------------
 */
int
gfx_get_vbi_enable(void)
{
   int enable = 0;

#	if GFX_VIP_SC1200
   if (gfx_vip_type == GFX_VIP_TYPE_SC1200)
      enable = sc1200_get_vbi_enable();
#	endif
   return (enable);
}

/*-----------------------------------------------------------------------------
 * gfx_get_vbi_mode
 *-----------------------------------------------------------------------------
 */
int
gfx_get_vbi_mode(void)
{
   int mode = 0;

#	if GFX_VIP_SC1200
   if (gfx_vip_type == GFX_VIP_TYPE_SC1200)
      mode = sc1200_get_vbi_mode();
#	endif
   return (mode);
}

/*-----------------------------------------------------------------------------
 * gfx_get_vbi_base
 *-----------------------------------------------------------------------------
 */
unsigned long
gfx_get_vbi_base(int odd)
{
   unsigned long base = 0;

#	if GFX_VIP_SC1200
   if (gfx_vip_type == GFX_VIP_TYPE_SC1200)
      base = sc1200_get_vbi_base(odd);
#	endif
   return (base);
}

/*-----------------------------------------------------------------------------
 * gfx_get_vbi_direct
 *-----------------------------------------------------------------------------
 */
unsigned long
gfx_get_vbi_direct(int odd)
{
   unsigned long vbi_direct_lines = 0;

#	if GFX_VIP_SC1200
   if (gfx_vip_type == GFX_VIP_TYPE_SC1200)
      vbi_direct_lines = sc1200_get_vbi_direct(odd);
#	endif
   return (vbi_direct_lines);
}

/*-----------------------------------------------------------------------------
 * gfx_get_vbi_interrupt
 *-----------------------------------------------------------------------------
 */
int
gfx_get_vbi_interrupt(void)
{
   int enable = 0;

#	if GFX_VIP_SC1200
   if (gfx_vip_type == GFX_VIP_TYPE_SC1200)
      enable = sc1200_get_vbi_interrupt();
#	endif
   return (enable);
}

/*-----------------------------------------------------------------------------
 * gfx_get_vip_bus_request_threshold_high
 *-----------------------------------------------------------------------------
 */
int
gfx_get_vip_bus_request_threshold_high(void)
{
   int enable = 0;

#	if GFX_VIP_SC1200
   if (gfx_vip_type == GFX_VIP_TYPE_SC1200)
      enable = sc1200_get_vip_bus_request_threshold_high();
#	endif
   return (enable);
}

#endif /* GFX_READ_ROUTINES */

#endif /* GFX_VIP_DYNAMIC */

/* END OF FILE */

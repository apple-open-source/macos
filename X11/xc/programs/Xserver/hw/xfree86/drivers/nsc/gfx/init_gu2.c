/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/nsc/gfx/init_gu2.c,v 1.1 2002/12/10 15:12:27 alanh Exp $ */
/*
 * $Workfile: init_gu2.c $
 *
 * This file contains routines used in Redcloud initialization.
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

unsigned long gu2_pci_config_read(unsigned long address);
void gu2_pci_config_write(unsigned long address, unsigned long data);
unsigned long gu2_get_core_freq(void);
unsigned long gu2_detect_cpu(void);
unsigned long gu2_detect_video(void);
unsigned long gu2_get_cpu_register_base(void);
unsigned long gu2_get_graphics_register_base(void);
unsigned long gu2_get_frame_buffer_base(void);
unsigned long gu2_get_frame_buffer_size(void);
unsigned long gu2_get_vid_register_base(void);
unsigned long gu2_get_vip_register_base(void);

/*-----------------------------------------------------------------------------
 * gfx_get_core_freq
 *
 * Returns the core clock frequency of a GX2.
 *-----------------------------------------------------------------------------
 */
#if GFX_INIT_DYNAMIC
unsigned long
gu2_get_core_freq(void)
#else
unsigned long
gfx_get_core_freq(void)
#endif
{
   unsigned long value;

   /* CPU SPEED IS REPORTED BY A VSM IN VSA II */
   /* Virtual Register Class = 0x12 (Sysinfo)  */
   /* CPU Speed Register     = 0x01            */

   OUTW(0xAC1C, 0xFC53);
   OUTW(0xAC1C, 0x1201);

   value = (unsigned long)(INW(0xAC1E));

   return (value);
}

/*-----------------------------------------------------------------------------
 * gfx_get_cpu_register_base
 * 
 * This routine returns the base address for display controller registers.  
 *-----------------------------------------------------------------------------
 */
#if GFX_INIT_DYNAMIC
unsigned long
gu2_get_cpu_register_base(void)
#else
unsigned long
gfx_get_cpu_register_base(void)
#endif
{
   return gfx_pci_config_read(0x80000918);
}

/*-----------------------------------------------------------------------------
 * gfx_get_graphics_register_base
 * 
 * This routine returns the base address for the graphics acceleration.  
 *-----------------------------------------------------------------------------
 */
#if GFX_INIT_DYNAMIC
unsigned long
gu2_get_graphics_register_base(void)
#else
unsigned long
gfx_get_graphics_register_base(void)
#endif
{
   return gfx_pci_config_read(0x80000914);
}

/*-----------------------------------------------------------------------------
 * gfx_get_frame_buffer_base
 * 
 * This routine returns the base address for graphics memory.  
 *-----------------------------------------------------------------------------
 */
#if GFX_INIT_DYNAMIC
unsigned long
gu2_get_frame_buffer_base(void)
#else
unsigned long
gfx_get_frame_buffer_base(void)
#endif
{
   return gfx_pci_config_read(0x80000910);
}

/*-----------------------------------------------------------------------------
 * gfx_get_frame_buffer_size
 * 
 * This routine returns the total size of graphics memory, in bytes.
 *-----------------------------------------------------------------------------
 */
#if GFX_INIT_DYNAMIC
unsigned long
gu2_get_frame_buffer_size(void)
#else
unsigned long
gfx_get_frame_buffer_size(void)
#endif
{
   unsigned long value;

   /* FRAME BUFFER SIZE IS REPORTED BY A VSM IN VSA II */
   /* Virtual Register Class     = 0x02                */
   /* VG_MEM_SIZE (512KB units)  = 0x00                */

   OUTW(0xAC1C, 0xFC53);
   OUTW(0xAC1C, 0x0200);

   value = (unsigned long)(INW(0xAC1E)) & 0xFFl;

   return (value << 19);
}

/*-----------------------------------------------------------------------------
 * gfx_get_vid_register_base
 * 
 * This routine returns the base address for the video hardware.  
 *-----------------------------------------------------------------------------
 */
#if GFX_INIT_DYNAMIC
unsigned long
gu2_get_vid_register_base(void)
#else
unsigned long
gfx_get_vid_register_base(void)
#endif
{
   return gfx_pci_config_read(0x8000091C);
}

/* END OF FILE */

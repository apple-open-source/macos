/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/nsc/gfx/init_gu1.c,v 1.1 2002/12/10 15:12:27 alanh Exp $ */
/*
 * $Workfile: init_gu1.c $
 *
 * This file contains routines used in the initialization of Geode-family 
 * processors.
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

unsigned char gfx_gxm_config_read(unsigned char index);
unsigned long gu1_get_core_freq(void);
unsigned long gu1_detect_cpu(void);
unsigned long gu1_detect_video(void);
unsigned long gu1_get_cpu_register_base(void);
unsigned long gu1_get_graphics_register_base(void);
unsigned long gu1_get_frame_buffer_base(void);
unsigned long gu1_get_frame_buffer_size(void);
unsigned long gu1_get_vid_register_base(void);
unsigned long gu1_get_vip_register_base(void);

/*-----------------------------------------------------------------------------
 * gfx_gxm_config_read
 *
 * This routine reads the value of the specified GXm configuration register.
 *-----------------------------------------------------------------------------
 */
unsigned char
gfx_gxm_config_read(unsigned char index)
{
   unsigned char value = 0xFF;
   unsigned char lock;

   OUTB(0x22, GXM_CONFIG_CCR3);
   lock = INB(0x23);
   OUTB(0x22, GXM_CONFIG_CCR3);
   OUTB(0x23, (unsigned char)(lock | 0x10));
   OUTB(0x22, index);
   value = INB(0x23);
   OUTB(0x22, GXM_CONFIG_CCR3);
   OUTB(0x23, lock);
   return (value);
}

/*-----------------------------------------------------------------------------
 * gfx_get_core_freq
 *
 * This routine returns the core clock frequency of a GXm if valid jumper settings are
 * detected; 0 if not. It assumes that a 33.3 MHz PCI clock is being used. 
 *-----------------------------------------------------------------------------
 */
#if GFX_INIT_DYNAMIC
unsigned long
gu1_get_core_freq(void)
#else
unsigned long
gfx_get_core_freq(void)
#endif
{
   unsigned char dir0, dir1;

   dir0 = gfx_gxm_config_read(GXM_CONFIG_DIR0) & 0x0F;
   dir1 = gfx_gxm_config_read(GXM_CONFIG_DIR1);

   /* REVISION 4.0 AND UP */

   if (dir1 >= 0x50) {
      switch (dir0) {
      case 0:
      case 2:
	 return 133;

      case 5:
	 return 166;
      case 3:
	 return 200;
      case 6:
	 return 233;
      case 7:
	 return 266;
      case 4:
	 return 300;
      case 1:
	 return 333;
      default:
	 return (0);
      }
   } else {
      switch (dir0) {
      case 0:
      case 2:
	 return 133;

      case 7:
	 return 166;

      case 1:
      case 3:
	 return 200;

      case 4:
      case 6:
	 return 233;

      case 5:
	 return 266;
      default:
	 return (0);
      }
   }
   return (0);
}

/*-----------------------------------------------------------------------------
 * gfx_get_cpu_register_base
 * 
 * This routine returns the base address for graphics registers.
 *-----------------------------------------------------------------------------
 */
#if GFX_INIT_DYNAMIC
unsigned long
gu1_get_cpu_register_base(void)
#else
unsigned long
gfx_get_cpu_register_base(void)
#endif
{
   unsigned long base;

   base = (unsigned long)gfx_gxm_config_read(GXM_CONFIG_GCR);
   base = (base & 0x03) << 30;
   return (base);
}

/*-----------------------------------------------------------------------------
 * gfx_get_frame_buffer_base
 * 
 * This routine returns the base address for graphics memory.  This is an 
 * offset of 0x00800000 from the base address specified in the GCR register.
 *
 * The function returns zero if the GCR indicates the graphics subsystem
 * is disabled.
 *-----------------------------------------------------------------------------
 */
#if GFX_INIT_DYNAMIC
unsigned long
gu1_get_frame_buffer_base(void)
#else
unsigned long
gfx_get_frame_buffer_base(void)
#endif
{
   unsigned long base;

   base = (unsigned long)gfx_gxm_config_read(GXM_CONFIG_GCR);
   base = (base & 0x03) << 30;
   if (base)
      base |= 0x00800000;
   return (base);
}

/*-----------------------------------------------------------------------------
 * gfx_get_frame_buffer_size
 * 
 * This routine returns the total size of graphics memory, in bytes.
 *
 * Currently this routine is hardcoded to return 2 Meg.
 *-----------------------------------------------------------------------------
 */
#if GFX_INIT_DYNAMIC
unsigned long
gu1_get_frame_buffer_size(void)
#else
unsigned long
gfx_get_frame_buffer_size(void)
#endif
{
#if FB4MB
   return (0x00400000);
#else
   return (0x00200000);
#endif
}

/*-----------------------------------------------------------------------------
 * gfx_get_vid_register_base
 * 
 * This routine returns the base address for the video hardware.  It assumes
 * an offset of 0x00010000 from the base address specified by the GCR.
 *
 * The function returns zero if the GCR indicates the graphics subsystem
 * is disabled.
 *-----------------------------------------------------------------------------
 */
#if GFX_INIT_DYNAMIC
unsigned long
gu1_get_vid_register_base(void)
#else
unsigned long
gfx_get_vid_register_base(void)
#endif
{
   unsigned long base;

   base = (unsigned long)gfx_gxm_config_read(GXM_CONFIG_GCR);
   base = (base & 0x03) << 30;
   if (base)
      base |= 0x00010000;
   return (base);
}

/*-----------------------------------------------------------------------------
 * gfx_get_vip_register_base
 * 
 * This routine returns the base address for the VIP hardware.  This is 
 * only applicable to the SC1200, for which this routine assumes an offset 
 * of 0x00015000 from the base address specified by the GCR.
 *
 * The function returns zero if the GCR indicates the graphics subsystem
 * is disabled.
 *-----------------------------------------------------------------------------
 */
#if GFX_INIT_DYNAMIC
unsigned long
gu1_get_vip_register_base(void)
#else
unsigned long
gfx_get_vip_register_base(void)
#endif
{
   unsigned long base = 0;

   if ((gfx_cpu_version & 0xFF) == GFX_CPU_SC1200) {
      base = (unsigned long)gfx_gxm_config_read(GXM_CONFIG_GCR);
      base = (base & 0x03) << 30;
      if (base)
	 base |= 0x00015000;
   }
   return (base);
}

/* END OF FILE */

/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/nsc/gfx/vip_1200.c,v 1.1 2002/12/10 15:12:27 alanh Exp $ */
/*
 * $Workfile: vip_1200.c $
 *
 * This file contains routines to control the SC1200 video input port (VIP) hardware.
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

int sc1200_set_vip_enable(int enable);
int sc1200_set_vip_capture_run_mode(int mode);
int sc1200_set_vip_base(unsigned long even, unsigned long odd);
int sc1200_set_vip_pitch(unsigned long pitch);
int sc1200_set_vip_mode(int mode);
int sc1200_set_vbi_enable(int enable);
int sc1200_set_vbi_mode(int mode);
int sc1200_set_vbi_base(unsigned long even, unsigned long odd);
int sc1200_set_vbi_pitch(unsigned long pitch);
int sc1200_set_vbi_direct(unsigned long even_lines, unsigned long odd_lines);
int sc1200_set_vbi_interrupt(int enable);
int sc1200_set_vip_bus_request_threshold_high(int enable);
int sc1200_set_vip_last_line(int last_line);
int sc1200_test_vip_odd_field(void);
int sc1200_test_vip_bases_updated(void);
int sc1200_test_vip_fifo_overflow(void);
int sc1200_get_vip_line(void);

/* READ ROUTINES IN GFX_VIP.C */

int sc1200_get_vip_enable(void);
unsigned long sc1200_get_vip_base(int odd);
unsigned long sc1200_get_vip_pitch(void);
int sc1200_get_vip_mode(void);
int sc1200_get_vbi_enable(void);
int sc1200_get_vbi_mode(void);
unsigned long sc1200_get_vbi_base(int odd);
unsigned long sc1200_get_vbi_pitch(void);
unsigned long sc1200_get_vbi_direct(int odd);
int sc1200_get_vbi_interrupt(void);
int sc1200_get_vip_bus_request_threshold_high(void);

/*-----------------------------------------------------------------------------
 * gfx_set_vip_enable
 *
 * This routine enables or disables the writes to memory from the video port.  
 *-----------------------------------------------------------------------------
 */
#if GFX_VIP_DYNAMIC
int
sc1200_set_vip_enable(int enable)
#else
int
gfx_set_vip_enable(int enable)
#endif
{
   unsigned long value;

   value = READ_VIP32(SC1200_VIP_CONTROL);
   if (enable)
      value |= SC1200_VIP_DATA_CAPTURE_EN;
   else
      value &= ~SC1200_VIP_DATA_CAPTURE_EN;
   WRITE_VIP32(SC1200_VIP_CONTROL, value);
   return (0);
}

/*-----------------------------------------------------------------------------
 * gfx_set_vip_capture_run_mode
 *
 * This routine selects VIP capture run mode.
 *-----------------------------------------------------------------------------
 */
#if GFX_VIP_DYNAMIC
int
sc1200_set_vip_capture_run_mode(int mode)
#else
int
gfx_set_vip_capture_run_mode(int mode)
#endif
{
   unsigned long value;

   value = READ_VIP32(SC1200_VIP_CONTROL);
   value &= ~SC1200_CAPTURE_RUN_MODE_MASK;
   switch (mode) {
   case VIP_CAPTURE_STOP_LINE:
      value |= SC1200_CAPTURE_RUN_MODE_STOP_LINE;
      break;
   case VIP_CAPTURE_STOP_FIELD:
      value |= SC1200_CAPTURE_RUN_MODE_STOP_FIELD;
      break;
   case VIP_CAPTURE_START_FIELD:
      value |= SC1200_CAPTURE_RUN_MODE_START;
      break;
   default:
      return GFX_STATUS_BAD_PARAMETER;
   }
   WRITE_VIP32(SC1200_VIP_CONTROL, value);
   return (0);
}

/*-----------------------------------------------------------------------------
 * gfx_set_vip_base
 *
 * This routine sets the odd and even base address values for the VIP memory
 * buffer.
 *-----------------------------------------------------------------------------
 */
#if GFX_VIP_DYNAMIC
int
sc1200_set_vip_base(unsigned long even, unsigned long odd)
#else
int
gfx_set_vip_base(unsigned long even, unsigned long odd)
#endif
{
   /* TRUE OFFSET IS SPECIFIED, NEED TO SET BIT 23 FOR HARDWARE */

   if (even)
      WRITE_VIP32(SC1200_VIP_EVEN_BASE, even + (unsigned long)gfx_phys_fbptr);
   if (odd)
      WRITE_VIP32(SC1200_VIP_ODD_BASE, odd + (unsigned long)gfx_phys_fbptr);
   return (0);
}

/*-----------------------------------------------------------------------------
 * gfx_set_vip_pitch
 *
 * This routine sets the number of bytes between scanlines for the VIP data.
 *-----------------------------------------------------------------------------
 */
#if GFX_VIP_DYNAMIC
int
sc1200_set_vip_pitch(unsigned long pitch)
#else
int
gfx_set_vip_pitch(unsigned long pitch)
#endif
{
   WRITE_VIP32(SC1200_VIP_PITCH, pitch & SC1200_VIP_PITCH_MASK);
   return (0);
}

/*-----------------------------------------------------------------------------
 * gfx_set_vip_mode
 *
 * This routine sets the VIP operating mode.
 *-----------------------------------------------------------------------------
 */
#if GFX_VIP_DYNAMIC
int
sc1200_set_vip_mode(int mode)
#else
int
gfx_set_vip_mode(int mode)
#endif
{
   unsigned long config;

   config = READ_VIP32(SC1200_VIP_CONFIG);
   config &= ~SC1200_VIP_MODE_MASK;
   switch (mode) {
   case VIP_MODE_C:
      WRITE_VIP32(SC1200_VIP_CONFIG, config | SC1200_VIP_MODE_C);
      break;
   default:
      return GFX_STATUS_BAD_PARAMETER;
   }
   return (0);
}

/*-----------------------------------------------------------------------------
 * gfx_set_vbi_enable
 *
 * This routine enables or disables the VBI data capture.
 *-----------------------------------------------------------------------------
 */
#if GFX_VIP_DYNAMIC
int
sc1200_set_vbi_enable(int enable)
#else
int
gfx_set_vbi_enable(int enable)
#endif
{
   unsigned long value;

   value = READ_VIP32(SC1200_VIP_CONTROL);
   if (enable)
      value |= SC1200_VIP_VBI_CAPTURE_EN;
   else
      value &= ~SC1200_VIP_VBI_CAPTURE_EN;
   WRITE_VIP32(SC1200_VIP_CONTROL, value);
   return (0);
}

/*-----------------------------------------------------------------------------
 * gfx_set_vbi_mode
 *
 * This routine sets the VBI data types captured to memory.
 * It receives a mask of all enabled types.
 *-----------------------------------------------------------------------------
 */
#if GFX_VIP_DYNAMIC
int
sc1200_set_vbi_mode(int mode)
#else
int
gfx_set_vbi_mode(int mode)
#endif
{
   unsigned long config;

   config = READ_VIP32(SC1200_VIP_CONFIG);
   config &=
	 ~(SC1200_VBI_ANCILLARY_TO_MEMORY | SC1200_VBI_TASK_A_TO_MEMORY |
	   SC1200_VBI_TASK_B_TO_MEMORY);

   if (mode & VBI_ANCILLARY)
      config |= SC1200_VBI_ANCILLARY_TO_MEMORY;
   if (mode & VBI_TASK_A)
      config |= SC1200_VBI_TASK_A_TO_MEMORY;
   if (mode & VBI_TASK_B)
      config |= SC1200_VBI_TASK_B_TO_MEMORY;
   WRITE_VIP32(SC1200_VIP_CONFIG, config);
   return (0);
}

/*-----------------------------------------------------------------------------
 * gfx_set_vbi_base
 *
 * This routine sets the odd and even base address values for VBI capture.
 *
 * "even" and "odd" should contain 16-byte aligned physical addresses.
 *-----------------------------------------------------------------------------
 */
#if GFX_VIP_DYNAMIC
int
sc1200_set_vbi_base(unsigned long even, unsigned long odd)
#else
int
gfx_set_vbi_base(unsigned long even, unsigned long odd)
#endif
{
   /* VIP HW REQUIRES THAT BASE ADDRESSES BE 16-BYTE ALIGNED */

   if (even)
      WRITE_VIP32(SC1200_VBI_EVEN_BASE, even & ~0xf);
   if (odd)
      WRITE_VIP32(SC1200_VBI_ODD_BASE, odd & ~0xf);

   return (0);
}

/*-----------------------------------------------------------------------------
 * gfx_set_vbi_pitch
 *
 * This routine sets the number of bytes between scanlines for VBI capture.
 *-----------------------------------------------------------------------------
 */
#if GFX_VIP_DYNAMIC
int
sc1200_set_vbi_pitch(unsigned long pitch)
#else
int
gfx_set_vbi_pitch(unsigned long pitch)
#endif
{
   WRITE_VIP32(SC1200_VBI_PITCH, pitch & SC1200_VBI_PITCH_MASK);
   return (0);
}

/*-----------------------------------------------------------------------------
 * gfx_set_vbi_direct
 *
 * This routine sets the VBI lines to be passed to the Direct VIP.
 *-----------------------------------------------------------------------------
 */
#if GFX_VIP_DYNAMIC
int
sc1200_set_vbi_direct(unsigned long even_lines, unsigned long odd_lines)
#else
int
gfx_set_vbi_direct(unsigned long even_lines, unsigned long odd_lines)
#endif
{
   WRITE_VIP32(SC1200_EVEN_DIRECT_VBI_LINE_ENABLE,
	       even_lines & SC1200_DIRECT_VBI_LINE_ENABLE_MASK);
   WRITE_VIP32(SC1200_ODD_DIRECT_VBI_LINE_ENABLE,
	       odd_lines & SC1200_DIRECT_VBI_LINE_ENABLE_MASK);
   return (0);
}

/*-----------------------------------------------------------------------------
 * gfx_set_vbi_interrupt
 *
 * This routine enables or disables the VBI field interrupt.
 *-----------------------------------------------------------------------------
 */
#if GFX_VIP_DYNAMIC
int
sc1200_set_vbi_interrupt(int enable)
#else
int
gfx_set_vbi_interrupt(int enable)
#endif
{
   unsigned long value;

   value = READ_VIP32(SC1200_VIP_CONTROL);
   if (enable)
      value |= SC1200_VIP_VBI_FIELD_INTERRUPT_EN;
   else
      value &= ~SC1200_VIP_VBI_FIELD_INTERRUPT_EN;
   WRITE_VIP32(SC1200_VIP_CONTROL, value);
   return (0);
}

/*-----------------------------------------------------------------------------
 * gfx_set_vip_bus_request_threshold_high
 *
 * This routine sets the VIP FIFO bus request threshold.
 * If enable is TRUE, VIP FIFO will be set to issue a bus request when it filled with 64 bytes.
 * If enable is FALSE, VIP FIFO will be set to issue a bus request when it filled with 32 bytes.
 *-----------------------------------------------------------------------------
 */
#if GFX_VIP_DYNAMIC
int
sc1200_set_vip_bus_request_threshold_high(int enable)
#else
int
gfx_set_vip_bus_request_threshold_high(int enable)
#endif
{
   unsigned long value;

   value = READ_VIP32(SC1200_VIP_CONFIG);
   if (enable)
      value &= ~SC1200_VIP_BUS_REQUEST_THRESHOLD;
   else
      value |= SC1200_VIP_BUS_REQUEST_THRESHOLD;
   WRITE_VIP32(SC1200_VIP_CONFIG, value);
   return (0);
}

/*-----------------------------------------------------------------------------
 * gfx_set_vip_last_line
 *
 * This routine sets the maximum number of lines captured in each field.
 *-----------------------------------------------------------------------------
 */
#if GFX_VIP_DYNAMIC
int
sc1200_set_vip_last_line(int last_line)
#else
int
gfx_set_vip_last_line(int last_line)
#endif
{
   unsigned long value;

   /* This feature is implemented in Rev C1 */
   if (gfx_chip_revision < SC1200_REV_C1)
      return (GFX_STATUS_OK);

   value = READ_VIP32(SC1200_VIP_LINE_TARGET);
   value &= ~SC1200_VIP_LAST_LINE_MASK;
   value |= ((last_line & 0x3FF) << 16);
   WRITE_VIP32(SC1200_VIP_LINE_TARGET, value);
   return (GFX_STATUS_OK);
}

/*-----------------------------------------------------------------------------
 * gfx_test_vip_odd_field
 *
 * This routine returns 1 if the current VIP field is odd. Otherwise returns 0.
 *-----------------------------------------------------------------------------
 */
#if GFX_VIP_DYNAMIC
int
sc1200_test_vip_odd_field(void)
#else
int
gfx_test_vip_odd_field(void)
#endif
{
   if (READ_VIP32(SC1200_VIP_STATUS) & SC1200_VIP_CURRENT_FIELD_ODD)
      return (1);
   else
      return (0);
}

/*-----------------------------------------------------------------------------
 * gfx_test_vip_bases_updated
 *
 * This routine returns 1 if all of the VIP base registers have been updated,
 * i.e. there is no base register which has been written with a new address, 
 * that VIP has not already captured or started capturing into the new address.
 *-----------------------------------------------------------------------------
 */
#if GFX_VIP_DYNAMIC
int
sc1200_test_vip_bases_updated(void)
#else
int
gfx_test_vip_bases_updated(void)
#endif
{
   if (READ_VIP32(SC1200_VIP_STATUS) & SC1200_VIP_BASE_NOT_UPDATED)
      return (0);
   else
      return (1);
}

/*-----------------------------------------------------------------------------
 * gfx_test_vip_fifo_overflow
 *
 * This routine returns 1 if an overflow occurred on the FIFO between the VIP
 * and the fast X-bus, 0 otherwise. 
 * If an overflow occurred, the overflow status indication is reset.
 *-----------------------------------------------------------------------------
 */
#if GFX_VIP_DYNAMIC
int
sc1200_test_vip_fifo_overflow(void)
#else
int
gfx_test_vip_fifo_overflow(void)
#endif
{
   if (READ_VIP32(SC1200_VIP_STATUS) & SC1200_VIP_FIFO_OVERFLOW) {
      /* Bits in vip status register are either read only or reset by writing 1 */
      WRITE_VIP32(SC1200_VIP_STATUS, SC1200_VIP_FIFO_OVERFLOW);
      return (1);
   } else {
      return (0);
   }
}

/*-----------------------------------------------------------------------------
 * gfx_get_vip_line
 *
 * This routine returns the number of the current video line being
 * received by the VIP interface.
 *-----------------------------------------------------------------------------
 */
#if GFX_VIP_DYNAMIC
int
sc1200_get_vip_line(void)
#else
int
gfx_get_vip_line(void)
#endif
{
   return (int)(READ_VIP32(SC1200_VIP_CURRENT_LINE) &
		SC1200_VIP_CURRENT_LINE_MASK);
}

/*-----------------------------------------------------------------------------
 * gfx_get_vip_base
 *-----------------------------------------------------------------------------
 */
#if GFX_VIP_DYNAMIC
unsigned long
sc1200_get_vip_base(int odd)
#else
unsigned long
gfx_get_vip_base(int odd)
#endif
{
   /* MASK BIT 23 AND ABOVE TO MAKE IT A TRUE OFFSET */

   if (odd)
      return (READ_VIP32(SC1200_VIP_ODD_BASE));
   return (READ_VIP32(SC1200_VIP_EVEN_BASE));
}

/*-----------------------------------------------------------------------------
 * gfx_get_vbi_pitch
 *-----------------------------------------------------------------------------
 */
#if GFX_VIP_DYNAMIC
unsigned long
sc1200_get_vbi_pitch(void)
#else
unsigned long
gfx_get_vbi_pitch(void)
#endif
{
   return (READ_VIP32(SC1200_VBI_PITCH) & SC1200_VBI_PITCH_MASK);
}

/*************************************************************/
/*  READ ROUTINES  |  INCLUDED FOR DIAGNOSTIC PURPOSES ONLY  */
/*************************************************************/

#if GFX_READ_ROUTINES

/*-----------------------------------------------------------------------------
 * gfx_get_vip_enable
 *-----------------------------------------------------------------------------
 */
#if GFX_VIP_DYNAMIC
int
sc1200_get_vip_enable(void)
#else
int
gfx_get_vip_enable(void)
#endif
{
   if (READ_VIP32(SC1200_VIP_CONTROL) & SC1200_VIP_DATA_CAPTURE_EN)
      return (1);
   return (0);
}

/*-----------------------------------------------------------------------------
 * gfx_get_vip_pitch
 *-----------------------------------------------------------------------------
 */
#if GFX_VIP_DYNAMIC
unsigned long
sc1200_get_vip_pitch(void)
#else
unsigned long
gfx_get_vip_pitch(void)
#endif
{
   return (READ_VIP32(SC1200_VIP_PITCH) & SC1200_VIP_PITCH_MASK);
}

/*-----------------------------------------------------------------------------
 * gfx_get_vip_mode
 *-----------------------------------------------------------------------------
 */
#if GFX_VIP_DYNAMIC
int
sc1200_get_vip_mode(void)
#else
int
gfx_get_vip_mode(void)
#endif
{
   switch (READ_VIP32(SC1200_VIP_CONFIG) & SC1200_VIP_MODE_MASK) {
   case SC1200_VIP_MODE_C:
      return VIP_MODE_C;
   default:
      return (0);
   }
}

/*-----------------------------------------------------------------------------
 * gfx_get_vbi_enable
 *-----------------------------------------------------------------------------
 */
#if GFX_VIP_DYNAMIC
int
sc1200_get_vbi_enable(void)
#else
int
gfx_get_vbi_enable(void)
#endif
{
   if (READ_VIP32(SC1200_VIP_CONTROL) & SC1200_VIP_VBI_CAPTURE_EN)
      return (1);
   return (0);
}

/*-----------------------------------------------------------------------------
 * gfx_get_vbi_mode
 *-----------------------------------------------------------------------------
 */
#if GFX_VIP_DYNAMIC
int
sc1200_get_vbi_mode(void)
#else
int
gfx_get_vbi_mode(void)
#endif
{
   int config;
   int mode = 0;

   config =
	 (int)(READ_VIP32(SC1200_VIP_CONFIG) &
	       (SC1200_VBI_ANCILLARY_TO_MEMORY | SC1200_VBI_TASK_A_TO_MEMORY |
		SC1200_VBI_TASK_B_TO_MEMORY));
   if (config & SC1200_VBI_ANCILLARY_TO_MEMORY)
      mode |= VBI_ANCILLARY;
   if (config & SC1200_VBI_TASK_A_TO_MEMORY)
      mode |= VBI_TASK_A;
   if (config & SC1200_VBI_TASK_B_TO_MEMORY)
      mode |= VBI_TASK_B;
   return mode;
}

/*-----------------------------------------------------------------------------
 * gfx_get_vbi_base
 *-----------------------------------------------------------------------------
 */
#if GFX_VIP_DYNAMIC
unsigned long
sc1200_get_vbi_base(int odd)
#else
unsigned long
gfx_get_vbi_base(int odd)
#endif
{
   /* MASK BIT 23 AND ABOVE TO MAKE IT A TRUE OFFSET */

   if (odd)
      return (READ_VIP32(SC1200_VBI_ODD_BASE));
   return (READ_VIP32(SC1200_VBI_EVEN_BASE));
}

/*-----------------------------------------------------------------------------
 * gfx_get_vbi_direct
 *-----------------------------------------------------------------------------
 */
#if GFX_VIP_DYNAMIC
unsigned long
sc1200_get_vbi_direct(int odd)
#else
unsigned long
gfx_get_vbi_direct(int odd)
#endif
{
   /* MASK BIT 23 AND ABOVE TO MAKE IT A TRUE OFFSET */

   if (odd)
      return (READ_VIP32(SC1200_ODD_DIRECT_VBI_LINE_ENABLE) &
	      SC1200_DIRECT_VBI_LINE_ENABLE_MASK);
   return (READ_VIP32(SC1200_EVEN_DIRECT_VBI_LINE_ENABLE) &
	   SC1200_DIRECT_VBI_LINE_ENABLE_MASK);
}

/*-----------------------------------------------------------------------------
 * gfx_get_vbi_interrupt
 *-----------------------------------------------------------------------------
 */
#if GFX_VIP_DYNAMIC
int
sc1200_get_vbi_interrupt(void)
#else
int
gfx_get_vbi_interrupt(void)
#endif
{
   if (READ_VIP32(SC1200_VIP_CONTROL) & SC1200_VIP_VBI_FIELD_INTERRUPT_EN)
      return (1);
   return (0);
}

/*-----------------------------------------------------------------------------
 * gfx_get_vip_bus_request_threshold_high
 *-----------------------------------------------------------------------------
 */
#if GFX_VIP_DYNAMIC
int
sc1200_get_vip_bus_request_threshold_high(void)
#else
int
gfx_get_vip_bus_request_threshold_high(void)
#endif
{
   if (READ_VIP32(SC1200_VIP_CONFIG) & SC1200_VIP_BUS_REQUEST_THRESHOLD)
      return (1);
   return (0);
}

#endif /* GFX_READ_ROUTINES */

/* END OF FILE */

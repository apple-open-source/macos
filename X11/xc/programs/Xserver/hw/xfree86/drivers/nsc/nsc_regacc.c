/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/nsc/nsc_regacc.c,v 1.2 2003/01/14 09:34:32 alanh Exp $ */
/*
 * $Workfile: nsc_regacc.c $
 * $Revision: 1.1.1.1 $
 * $Author: jharper $
 *
 * This is the main file used to add Durango graphics support to a software 
 * project.  The main reason to have a single file include the other files
 * is that it centralizes the location of the compiler options.  This file
 * should be tuned for a specific implementation, and then modified as needed
 * for new Durango releases.  The releases.txt file indicates any updates to
 * this main file, such as a new definition for a new hardware platform. 
 *
 * In other words, this file should be copied from the Durango source files
 * once when a software project starts, and then maintained as necessary.  
 * It should not be recopied with new versions of Durango unless the 
 * developer is willing to tune the file again for the specific project.
 */

/* 
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

void gfx_write_reg8(unsigned long offset, unsigned char value);
void gfx_write_reg16(unsigned long offset, unsigned short value);
void gfx_write_reg32(unsigned long offset, unsigned long value);
unsigned short gfx_read_reg16(unsigned long offset);
unsigned long gfx_read_reg32(unsigned long offset);
void gfx_write_vid32(unsigned long offset, unsigned long value);
unsigned long gfx_read_vid32(unsigned long offset);
unsigned long gfx_read_vip32(unsigned long offset);
void gfx_write_vip32(unsigned long offset, unsigned long value);
void gfx_mono_bitmap_to_screen_blt_swp(unsigned short srcx,
				       unsigned short srcy,
				       unsigned short dstx,
				       unsigned short dsty,
				       unsigned short width,
				       unsigned short height,
				       unsigned char *data, short pitch);
unsigned int GetVideoMemSize(void);

/* ROUTINES added accessing hardware reg */
void
gfx_write_reg8(unsigned long offset, unsigned char value)
{
   WRITE_REG8(offset, value);
}

void
gfx_write_reg16(unsigned long offset, unsigned short value)
{
   WRITE_REG16(offset, value);
}

void
gfx_write_reg32(unsigned long offset, unsigned long value)
{
   WRITE_REG32(offset, value);
}
unsigned short
gfx_read_reg16(unsigned long offset)
{
   unsigned short value;

   value = READ_REG16(offset);
   return value;
}
unsigned long
gfx_read_reg32(unsigned long offset)
{
   unsigned long value;

   value = READ_REG32(offset);
   return value;
}

void
gfx_write_vid32(unsigned long offset, unsigned long value)
{
   WRITE_VID32(offset, value);
}
unsigned long
gfx_read_vid32(unsigned long offset)
{
   unsigned long value;

   value = READ_VID32(offset);
   return value;
}

/*Addition for the VIP code */
unsigned long
gfx_read_vip32(unsigned long offset)
{
   unsigned long value;

   value = READ_VIP32(offset);
   return value;
}

void
gfx_write_vip32(unsigned long offset, unsigned long value)
{
   WRITE_VIP32(offset, value);
}

#define SWAP_BITS_IN_BYTES(v) \
 (((0x01010101 & (v)) << 7) | ((0x02020202 & (v)) << 5) | \
  ((0x04040404 & (v)) << 3) | ((0x08080808 & (v)) << 1) | \
  ((0x10101010 & (v)) >> 1) | ((0x20202020 & (v)) >> 3) | \
  ((0x40404040 & (v)) >> 5) | ((0x80808080 & (v)) >> 7))

#define WRITE_GPREG_STRING32_SWP(regoffset, dwords, counter, array, array_offset, temp) \
{                                                                                       \
	temp = (unsigned long)array + (array_offset);                                       \
	for (counter = 0; counter < dwords; counter++)                                      \
		WRITE_GP32 (regoffset, SWAP_BITS_IN_BYTES(*((unsigned long *)temp + counter))); \
}

void
gfx_mono_bitmap_to_screen_blt_swp(unsigned short srcx, unsigned short srcy,
				  unsigned short dstx, unsigned short dsty,
				  unsigned short width, unsigned short height,
				  unsigned char *data, short pitch)
{
   unsigned long dstoffset, size, bytes;
   unsigned long offset, temp_offset, temp1 = 0, temp2 = 0;
   unsigned long i, j = 0, fifo_lines, dwords_extra, bytes_extra;
   unsigned long shift = 0;

   size = (((unsigned long)width) << 16) | height;

   /* CALCULATE STARTING OFFSETS */

   offset = (unsigned long)srcy *pitch + ((unsigned long)srcx >> 3);

   dstoffset = (unsigned long)dsty *gu2_pitch +
	 (((unsigned long)dstx) << gu2_xshift);

   /* CHECK IF PATTERN ORIGINS NEED TO BE SET */

   if (GFXpatternFlags) {
      /* COMBINE X AND Y PATTERN ORIGINS WITH OFFSET */

      dstoffset |= ((unsigned long)(dstx & 7)) << 26;
      dstoffset |= ((unsigned long)(dsty & 7)) << 29;
   }

   bytes = ((srcx & 7) + width + 7) >> 3;
   fifo_lines = bytes >> 5;
   dwords_extra = (bytes & 0x0000001Cl) >> 2;
   bytes_extra = bytes & 0x00000003l;

   /* POLL UNTIL ABLE TO WRITE TO THE REGISTERS */
   /* Put off poll for as long as possible (do most calculations first).   */
   /* The source offset is always 0 since we allow misaligned dword reads. */
   /* Need to wait for busy instead of pending, since hardware clears      */
   /* the host data FIFO at the beginning of a BLT.                        */

   GU2_WAIT_PENDING;
   WRITE_GP32(MGP_RASTER_MODE, gu2_rop32);
   WRITE_GP32(MGP_SRC_OFFSET, ((unsigned long)srcx & 7) << 26);
   WRITE_GP32(MGP_DST_OFFSET, dstoffset);
   WRITE_GP32(MGP_WID_HEIGHT, size);
   WRITE_GP32(MGP_STRIDE, gu2_pitch);
   WRITE_GP16(MGP_BLT_MODE, gu2_blt_mode | MGP_BM_SRC_HOST | MGP_BM_SRC_MONO);

   /* WAIT FOR BLT TO BE LATCHED */

   GU2_WAIT_PENDING;

   /* WRITE ALL OF THE DATA TO THE HOST SOURCE REGISTER */

   while (height--) {
      temp_offset = offset;

      /* WRITE ALL FULL FIFO LINES */

      for (i = 0; i < fifo_lines; i++) {
	 GU2_WAIT_HALF_EMPTY;
	 WRITE_GPREG_STRING32_SWP(MGP_HST_SOURCE, 8, j, data, temp_offset,
				  temp1);
	 temp_offset += 32;
      }

      /* WRITE ALL FULL DWORDS */

      GU2_WAIT_HALF_EMPTY;
      if (dwords_extra) {
	 WRITE_GPREG_STRING32_SWP(MGP_HST_SOURCE, dwords_extra, i, data,
				  temp_offset, temp1);
	 temp_offset += (dwords_extra << 2);
      }

      /* WRITE REMAINING BYTES */

      shift = 0;
      if (bytes_extra)
	 WRITE_GPREG_STRING8(MGP_HST_SOURCE, bytes_extra, shift, i, data,
			     temp_offset, temp1, temp2);

      offset += pitch;
   }
}
unsigned int
GetVideoMemSize(void)
{
   unsigned int graphicsMemBaseAddr;
   unsigned int totalMem = 0;
   int i;
   unsigned int graphicsMemMask, graphicsMemShift;

   /* Read graphics base address. */

   graphicsMemBaseAddr = gfx_read_reg32(0x8414);

   if (1) {
      unsigned int mcBankCfg = gfx_read_reg32(0x8408);
      unsigned int dimmShift = 4;

      graphicsMemMask = 0x7FF;
      graphicsMemShift = 19;

      /* Calculate total memory size for GXm. */

      for (i = 0; i < 2; i++) {
	 if (((mcBankCfg >> dimmShift) & 0x7) != 0x7) {
	    switch ((mcBankCfg >> (dimmShift + 4)) & 0x7) {
	    case 0:
	       totalMem += 0x400000;
	       break;
	    case 1:
	       totalMem += 0x800000;
	       break;
	    case 2:
	       totalMem += 0x1000000;
	       break;
	    case 3:
	       totalMem += 0x2000000;
	       break;
	    case 4:
	       totalMem += 0x4000000;
	       break;
	    case 5:
	       totalMem += 0x8000000;
	       break;
	    case 6:
	       totalMem += 0x10000000;
	       break;
	    case 7:
	       totalMem += 0x20000000;
	       break;
	    default:
	       break;
	    }
	 }
	 dimmShift += 16;
      }
   } else {
      unsigned int mcMemCntrl1 = gfx_read_reg32(0x8400);
      unsigned int bankSizeShift = 12;

      graphicsMemMask = 0x3FF;
      graphicsMemShift = 17;

      /* Calculate total memory size for GX. */

      for (i = 0; i < 4; i++) {
	 switch ((mcMemCntrl1 >> bankSizeShift) & 0x7) {
	 case 1:
	    totalMem += 0x200000;
	    break;
	 case 2:
	    totalMem += 0x400000;
	    break;
	 case 3:
	    totalMem += 0x800000;
	    break;
	 case 4:
	    totalMem += 0x1000000;
	    break;
	 case 5:
	    totalMem += 0x2000000;
	    break;
	 default:
	    break;
	 }
	 bankSizeShift += 3;
      }
   }

   /* Calculate graphics memory base address */

   graphicsMemBaseAddr &= graphicsMemMask;
   graphicsMemBaseAddr <<= graphicsMemShift;

   return (totalMem - graphicsMemBaseAddr);
}

/* END OF FILE */

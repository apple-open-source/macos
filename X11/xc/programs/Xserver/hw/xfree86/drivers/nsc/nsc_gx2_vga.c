/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/nsc/nsc_gx2_vga.c,v 1.3 2003/11/03 05:11:21 tsi Exp $ */
/*
 * $Workfile: nsc_gx2_vga.c $
 * $Revision: 1.1.1.2 $
 * $Author: jharper $
 *
 * This file contains routines to set modes using the VGA registers.  
 * Since this file is for the first generation graphics unit, it interfaces
 * to SoftVGA registers.  It works for both VSA1 and VSA2.
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
 *      nsc XFree86
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

/* VGA STRUCTURE */

#define GU2_STD_CRTC_REGS 25
#define GU2_EXT_CRTC_REGS 15
#define GU2_GDC_REGS 9
#define GU2_SEQ_REGS 5

#define GU2_VGA_FLAG_MISC_OUTPUT	0x1
#define GU2_VGA_FLAG_STD_CRTC		0x2
#define GU2_VGA_FLAG_EXT_CRTC		0x4
#define GU2_VGA_FLAG_GDC		0x10
#define GU2_VGA_FLAG_SEQ		0x20
#define GU2_VGA_FLAG_PALETTE 	0x40
#define GU2_VGA_FLAG_ATTR 	0x80

static unsigned int GDCregs[10];
static unsigned int SEQregs[10];
static unsigned int palette[256];
static unsigned int ATTRregs[32];
static unsigned char *font_data = NULL;

#define VGA_BLOCK 0x40000		/* 256 k */

void gu2_vga_extcrtc(char offset, int reset);
int gu2_get_vga_active(void);
void gu2_vga_font_data(int flag);
void gu2_set_vga(int reset);
int gu2_vga_seq_blanking(void);
int gu2_vga_attr_ctrl(int reset);
void gu2_vga_to_gfx(void);
void gu2_gfx_to_vga(int vga_mode);
int gu2_vga_seq_reset(int reset);
int gu2_vga_save(gfx_vga_struct * vga, int flags);
void gu2_vga_clear_extended(void);
int gu2_vga_restore(gfx_vga_struct * vga, int flags);

int
gu2_get_vga_active(void)
{
   int data = gfx_read_reg32(MDC_GENERAL_CFG);

   if (data & MDC_GCFG_VGAE)
      return 1;
   return 0;
}

void
gu2_vga_font_data(int flag)
{
   if (flag == 0) {
      if (font_data == NULL) {
	 font_data = malloc(VGA_BLOCK);
      }
      DEBUGMSG(1, (0, X_NONE, "Saving VGA Data\n"));
      memcpy(font_data, gfx_virt_fbptr, VGA_BLOCK);
   } else {
      if (font_data) {
	 DEBUGMSG(1, (0, X_NONE, "Restore VGA Data\n"));
	 memcpy(gfx_virt_fbptr, font_data, VGA_BLOCK);
	 free(font_data);
	 font_data = NULL;
      }
   }
}

void
gu2_set_vga(int reset)
{
   int data = gfx_read_reg32(MDC_GENERAL_CFG);

   if (reset)
      data |= MDC_GCFG_VGAE;
   else
      data &= ~MDC_GCFG_VGAE;
   gfx_write_reg32(MDC_GENERAL_CFG, data);
}

int
gu2_vga_seq_blanking(void)
{
   int tmp;

   gfx_outb(0x3C4, 1);
   tmp = gfx_inb(0x3C5);
   tmp |= 0x20;
   tmp |= tmp << 8;
   gfx_outw(0x3C4, tmp);

   gfx_delay_milliseconds(1);
   return (GFX_STATUS_OK);
}

int
gu2_vga_attr_ctrl(int reset)
{
   (void) gfx_inb(0x3DA);
   gfx_outb(0x3C0, (unsigned char)(reset ? 0x00 : 0x20));
   if (reset)
      (void) gfx_inb(0x3DA);
   return (GFX_STATUS_OK);
}

void
gu2_vga_to_gfx(void)
{
   gu2_vga_attr_ctrl(0);

   gu2_vga_seq_blanking();
   gfx_delay_milliseconds(2);

   gu2_vga_extcrtc(0x3F, 1);
}

void
gu2_gfx_to_vga(int vga_mode)
{
   char sequencer;

   gu2_vga_extcrtc(0x40, vga_mode);

   /* clear the display blanking bit */
   gfx_outb(MDC_SEQUENCER_INDEX, MDC_SEQUENCER_CLK_MODE);
   sequencer = gfx_inb(MDC_SEQUENCER_DATA);
   sequencer &= ~MDC_CLK_MODE_SCREEN_OFF;
   sequencer |= 1;
   gfx_outb(MDC_SEQUENCER_DATA, sequencer);

   gfx_delay_milliseconds(1);

   /*restart the sequencer */
   gfx_outw(0x3C4, 0x300);

   /* turn on the attribute controler */
   (void) gfx_inb(0x3DA);
   gfx_outb(0x3C0, 0x20);
   (void) gfx_inb(0x3DA);

   gu2_vga_extcrtc(0x3F, 0);
}

/*-----------------------------------------------------------------------------
 * gfx_vga_seq_reset
 *
 * This routine enables or disables SoftVGA.  It is used to make SoftVGA 
 * "be quiet" and not interfere with any of the direct hardware access from 
 * Durango.  For VSA1, the sequencer is reset to stop text redraws.  VSA2 may 
 * provide a better way to have SoftVGA sit in the background.
 *-----------------------------------------------------------------------------
 */
int
gu2_vga_seq_reset(int reset)
{
   gfx_outb(0x3C4, 0);
   gfx_outb(0x3C5, (unsigned char)(reset ? 0x00 : 0x03));
   return (GFX_STATUS_OK);
}

/*-----------------------------------------------------------------------------
 * gfx_vga_save
 *
 * This routine saves the state of the VGA registers into the specified 
 * structure.  Flags indicate what portions of the register state need to 
 * be saved.
 *-----------------------------------------------------------------------------
 */
int
gu2_vga_save(gfx_vga_struct * vga, int flags)
{
   int i;
   unsigned short crtcindex, crtcdata;

   crtcindex = (gfx_inb(0x3CC) & 0x01) ? 0x3D4 : 0x3B4;
   crtcdata = crtcindex + 1;

   /* CHECK MISCELLANEOUS OUTPUT FLAG */

   if (flags & GU2_VGA_FLAG_MISC_OUTPUT) {
      /* SAVE MISCCELLANEOUS OUTPUT REGISTER */

      vga->miscOutput = gfx_inb(0x3CC);
   }

   /* CHECK SEQ */

   if (flags & GU2_VGA_FLAG_SEQ) {
      /* SAVE STANDARD CRTC REGISTERS */

      for (i = 1; i < GU2_SEQ_REGS; i++) {
	 gfx_outb(0x3C4, (unsigned char)i);
	 SEQregs[i] = gfx_inb(0x3C5);
      }
   }

   /* CHECK STANDARD CRTC FLAG */

   if (flags & GU2_VGA_FLAG_STD_CRTC) {
      /* SAVE STANDARD CRTC REGISTERS */

      for (i = 0; i < GU2_STD_CRTC_REGS; i++) {
	 gfx_outb(crtcindex, (unsigned char)i);
	 vga->stdCRTCregs[i] = gfx_inb(crtcdata);
      }
   }

   /* CHECK GDC */

   if (flags & GU2_VGA_FLAG_GDC) {
      /* SAVE STANDARD CRTC REGISTERS */

      for (i = 0; i < GU2_GDC_REGS; i++) {
	 gfx_outb(0x3CE, (unsigned char)i);
	 GDCregs[i] = gfx_inb(0x3CF);
      }
   }

   /* CHECK EXTENDED CRTC FLAG */

   if (flags & GU2_VGA_FLAG_EXT_CRTC) {
      /* SAVE EXTENDED CRTC REGISTERS */

      for (i = 0; i < GU2_EXT_CRTC_REGS; i++) {
	 gfx_outb(crtcindex, (unsigned char)(0x40 + i));
	 vga->extCRTCregs[i] = gfx_inb(crtcdata);
      }
   }

   if (flags & GU2_VGA_FLAG_PALETTE) {
      /* SAVE PALETTE DATA */

      for (i = 0; i < 0x100; i++) {
	 gfx_outb(0x3C7, i);
	 palette[i] = gfx_inb(0x3C9);
      }
   }

   if (flags & GU2_VGA_FLAG_ATTR) {
      /* SAVE Attribute  DATA */

      for (i = 0; i < 21; i++) {
	 gfx_inb(0x3DA);
	 gfx_outb(0x3C0, i);
	 ATTRregs[i] = gfx_inb(0x3C1);
      }
   }
   /* save the VGA data */
   gu2_vga_font_data(0);
   return (0);
}

/*-----------------------------------------------------------------------------
 * gfx_vga_clear_extended
 *
 * This routine clears the extended SoftVGA register values to have SoftVGA
 * behave like standard VGA. 
 *-----------------------------------------------------------------------------
 */
void
gu2_vga_clear_extended(void)
{
   int i;
   unsigned short crtcindex, crtcdata;

   crtcindex = (gfx_inb(0x3CC) & 0x01) ? 0x3D4 : 0x3B4;
   crtcdata = crtcindex + 1;

   gfx_outb(crtcindex, 0x30);
   gfx_outb(crtcdata, 0x57);
   gfx_outb(crtcdata, 0x4C);
   for (i = 0x41; i <= 0x4F; i++) {
      gfx_outb(crtcindex, (unsigned char)i);
      gfx_outb(crtcdata, 0);
   }
   gfx_outb(crtcindex, 0x30);
   gfx_outb(crtcdata, 0x00);
}

void
gu2_vga_extcrtc(char offset, int reset)
{
   unsigned short crtcindex, crtcdata;

   crtcindex = (gfx_inb(0x3CC) & 0x01) ? 0x3D4 : 0x3B4;
   crtcdata = crtcindex + 1;

   /* UNLOCK EXTENDED CRTC REGISTERS */

   gfx_outb(crtcindex, 0x30);
   gfx_outb(crtcdata, 0x57);
   gfx_outb(crtcdata, 0x4C);

   /* RESTORE EXTENDED CRTC REGISTERS */

   gfx_outb(crtcindex, offset);
   gfx_outb(crtcdata, reset);

#if 0
   /* LOCK EXTENDED CRTC REGISTERS */

   gfx_outb(crtcindex, 0x30);
   gfx_outb(crtcdata, 0x00);
#endif
}

/*-----------------------------------------------------------------------------
 * gfx_vga_restore
 *
 * This routine restores the state of the VGA registers from the specified
 * structure.  Flags indicate what portions of the register state need to 
 * be saved.
 *-----------------------------------------------------------------------------
 */
int
gu2_vga_restore(gfx_vga_struct * vga, int flags)
{
   int i;
   unsigned short crtcindex, crtcdata;

   crtcindex = (gfx_inb(0x3CC) & 0x01) ? 0x3D4 : 0x3B4;
   crtcdata = crtcindex + 1;

   /* CHECK MISCELLANEOUS OUTPUT FLAG */

   if (flags & GU2_VGA_FLAG_MISC_OUTPUT) {
      /* RESTORE MISCELLANEOUS OUTPUT REGISTER VALUE */

      gfx_outb(0x3C2, vga->miscOutput);
   }

   /* CHECK SEQ */

   if (flags & GU2_VGA_FLAG_SEQ) {
      /* RESTORE STANDARD CRTC REGISTERS */

      for (i = 1; i < GU2_SEQ_REGS; i++) {
	 gfx_outb(0x3C4, (unsigned char)i);
	 gfx_outb(0x3C5, SEQregs[i]);
      }
   }

   /* CHECK STANDARD CRTC FLAG */

   if (flags & GU2_VGA_FLAG_STD_CRTC) {
      /* UNLOCK STANDARD CRTC REGISTERS */

      gfx_outb(crtcindex, 0x11);
      gfx_outb(crtcdata, 0);

      /* RESTORE STANDARD CRTC REGISTERS */

      for (i = 0; i < GU2_STD_CRTC_REGS; i++) {
	 gfx_outb(crtcindex, (unsigned char)i);
	 gfx_outb(crtcdata, vga->stdCRTCregs[i]);
      }
   }

   /* CHECK GDC */

   if (flags & GU2_VGA_FLAG_GDC) {
      /* SAVE STANDARD CRTC REGISTERS */

      for (i = 0; i < GU2_GDC_REGS; i++) {
	 gfx_outb(0x3CE, (unsigned char)i);
	 gfx_outb(0x3CF, GDCregs[i]);
      }
   }

   /* CHECK EXTENDED CRTC FLAG */

   if (flags & GU2_VGA_FLAG_EXT_CRTC) {
      /* UNLOCK EXTENDED CRTC REGISTERS */

      gfx_outb(crtcindex, 0x30);
      gfx_outb(crtcdata, 0x57);
      gfx_outb(crtcdata, 0x4C);

      /* RESTORE EXTENDED CRTC REGISTERS */

      for (i = 1; i < GU2_EXT_CRTC_REGS; i++) {
	 gfx_outb(crtcindex, (unsigned char)(0x40 + i));
	 gfx_outb(crtcdata, vga->extCRTCregs[i]);
      }

      /* LOCK EXTENDED CRTC REGISTERS */

      gfx_outb(crtcindex, 0x30);
      gfx_outb(crtcdata, 0x00);

      /* CHECK IF DIRECT FRAME BUFFER MODE (VESA MODE) */

      if (vga->extCRTCregs[0x03] & 1) {
	 /* SET BORDER COLOR TO BLACK */
	 /* This really should be another thing saved/restored, but */
	 /* Durango currently doesn't do the attr controller registers. */

	 gfx_inb(0x3BA);		/* Reset flip-flop */
	 gfx_inb(0x3DA);
	 gfx_outb(0x3C0, 0x11);
	 gfx_outb(0x3C0, 0x00);
      }
   }

   if (flags & GU2_VGA_FLAG_PALETTE) {
      /* RESTORE PALETTE DATA */

      for (i = 0; i < 0x100; i++) {
	 gfx_outb(0x3C8, i);
	 gfx_outb(0x3C9, palette[i]);
      }
   }

   if (flags & GU2_VGA_FLAG_ATTR) {
      /* RESTORE Attribute  DATA */

      for (i = 0; i < 21; i++) {
	 gfx_inb(0x3DA);
	 gfx_outb(0x3C0, i);
	 gfx_outb(0x3C0, ATTRregs[i]);
      }
      /* SAVE Attribute  DATA */

      for (i = 0; i < 21; i++) {
	 gfx_inb(0x3DA);
	 gfx_outb(0x3C0, i);
      }
   }

   /* restore the VGA data */
   gu2_vga_font_data(1);

   return (0);
}

/* END OF FILE */

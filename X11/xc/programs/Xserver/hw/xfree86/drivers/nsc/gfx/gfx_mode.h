/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/nsc/gfx/gfx_mode.h,v 1.1 2002/12/10 15:12:25 alanh Exp $ */
/*
 * $Workfile: gfx_mode.h $
 *
 * This header file contains the mode tables.  It is used by the "gfx_disp.c" 
 * file to set a display mode.
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

#ifndef _gfx_mode_h
#define _gfx_mode_h

/* MODE FLAGS (BITWISE-OR) */

#define GFX_MODE_8BPP		  0x00000001
#define GFX_MODE_12BPP        0x00000002
#define GFX_MODE_15BPP        0x00000004
#define GFX_MODE_16BPP		  0x00000008
#define GFX_MODE_24BPP        0x00000010
#define GFX_MODE_56HZ         0x00000020
#define GFX_MODE_60HZ		  0x00000040
#define GFX_MODE_70HZ		  0x00000080
#define GFX_MODE_72HZ		  0x00000100
#define GFX_MODE_75HZ		  0x00000200
#define GFX_MODE_85HZ		  0x00000400
#define GFX_MODE_NEG_HSYNC	  0x00000800
#define GFX_MODE_NEG_VSYNC	  0x00001000
#define GFX_MODE_PIXEL_DOUBLE 0x00002000
#define GFX_MODE_LINE_DOUBLE  0x00004000
#define GFX_MODE_TV_NTSC      0x00008000
#define GFX_MODE_TV_PAL       0x00010000

#define GFX_MODE_LOCK_TIMING  0x10000000

/* STRUCTURE DEFINITION */

typedef struct tagDISPLAYMODE
{
   /* DISPLAY MODE FLAGS */
   /* Specify valid color depths and the refresh rate. */

   unsigned long flags;

   /* TIMINGS */

   unsigned short hactive;
   unsigned short hblankstart;
   unsigned short hsyncstart;
   unsigned short hsyncend;
   unsigned short hblankend;
   unsigned short htotal;

   unsigned short vactive;
   unsigned short vblankstart;
   unsigned short vsyncstart;
   unsigned short vsyncend;
   unsigned short vblankend;
   unsigned short vtotal;

   /* CLOCK FREQUENCY */

   unsigned long frequency;

}
DISPLAYMODE;

/* For Fixed timings */
typedef struct tagFIXEDTIMINGS
{
   /* DISPLAY MODE FLAGS */
   /* Specify valid color depths and the refresh rate. */

   int panelresx;
   int panelresy;
   unsigned short xres;
   unsigned short yres;

   /* TIMINGS */

   unsigned short hactive;
   unsigned short hblankstart;
   unsigned short hsyncstart;
   unsigned short hsyncend;
   unsigned short hblankend;
   unsigned short htotal;

   unsigned short vactive;
   unsigned short vblankstart;
   unsigned short vsyncstart;
   unsigned short vsyncend;
   unsigned short vblankend;
   unsigned short vtotal;

   /* CLOCK FREQUENCY */

   unsigned long frequency;

}
FIXEDTIMINGS;

#endif /* !_gfx_mode_h */

/* END OF FILE */


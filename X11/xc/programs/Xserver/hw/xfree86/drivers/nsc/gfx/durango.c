/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/nsc/gfx/durango.c,v 1.1 2002/12/10 15:12:25 alanh Exp $ */
/*
 * $Workfile: durango.c $
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

/* COMPILER OPTIONS
 * These compiler options specify how the Durango routines are compiled 
 * for the different hardware platforms.  For best performance, a driver 
 * would build for a specific platform.  The "dynamic" switches are set 
 * by diagnostic applications such as Darwin that will run on a variety
 * of platforms and use the appropriate code at runtime.  Each component
 * may be separately dynamic, so that a driver has the option of being 
 * tuned for a specific 2D accelerator, but will still run with a variety
 * of chipsets. 
 */

#define GFX_DISPLAY_DYNAMIC			1	/* runtime selection */
#define GFX_DISPLAY_GU1				1	/* 1st generation display controller */
#define GFX_DISPLAY_GU2				1	/* 2nd generation display controller */

#define GFX_INIT_DYNAMIC            1	/* runtime selection */
#define GFX_INIT_GU1                1	/* Geode family      */
#define GFX_INIT_GU2                1	/* Redcloud          */

#define GFX_MSR_DYNAMIC             1	/* runtime selection */
#define GFX_MSR_REDCLOUD            1	/* Redcloud          */

#define GFX_2DACCEL_DYNAMIC			1	/* runtime selection */
#define GFX_2DACCEL_GU1				1	/* 1st generation 2D accelerator */
#define GFX_2DACCEL_GU2				1	/* 2nd generation 2D accelerator */

#define GFX_VIDEO_DYNAMIC			1	/* runtime selection */
#define GFX_VIDEO_CS5530			1	/* support for CS5530 */
#define GFX_VIDEO_SC1200			1	/* support for SC1200 */
#define GFX_VIDEO_REDCLOUD          1	/* support for Redcloud */

#define GFX_VIP_DYNAMIC				1	/* runtime selection */
#define GFX_VIP_SC1200				1	/* support for SC1200 */

#define GFX_DECODER_DYNAMIC			1	/* runtime selection */
#define GFX_DECODER_SAA7114			1	/* Philips SAA7114 decoder */

#define GFX_TV_DYNAMIC				1	/* runtime selection */
#define GFX_TV_FS451				1	/* Focus Enhancements FS450 */
#define GFX_TV_SC1200				1	/* SC1200 integrated TV encoder */

#define GFX_I2C_DYNAMIC				1	/* runtime selection */
#define GFX_I2C_ACCESS				1	/* support for ACCESS.BUS */
#define GFX_I2C_GPIO				1	/* support for CS5530 GPIOs */

#define GFX_VGA_DYNAMIC				1	/* runtime selection */
#define GFX_VGA_GU1					1	/* 1st generation graphics unit */

#define FB4MB						1	/* Set to use 4Mb video ram for Pyramid */

#define GFX_NO_IO_IN_WAIT_MACROS    0	/* Set to remove I/O accesses in GP bit testing */

/* ROUTINES TO READ VALUES
 * These are routines used by Darwin or other diagnostics to read the 
 * current state of the hardware.  Display drivers or embedded applications can 
 * reduce the size of the Durango code by not including these routines. 
 */
#define GFX_READ_ROUTINES			1	/* add routines to read values */

/* VARIABLES USED FOR RUNTIME SELECTION
 * If part of the graphics subsystem is declared as dynamic, then the 
 * following variables are used to specify which platform has been detected.
 * The variables are set in the "gfx_detect_cpu" routine.  The values should 
 * be bit flags to allow masks to be used to check for multiple platforms.
 */

#if GFX_DISPLAY_DYNAMIC
int gfx_display_type = 0;
#endif

#if GFX_INIT_DYNAMIC
int gfx_init_type = 0;
#endif

#if GFX_MSR_DYNAMIC
int gfx_msr_type = 0;
#endif

#if GFX_2DACCEL_DYNAMIC
int gfx_2daccel_type = 0;
#endif

#if GFX_VIDEO_DYNAMIC
int gfx_video_type = 0;
#endif

#if GFX_VIP_DYNAMIC
int gfx_vip_type = 0;
#endif

#if GFX_DECODER_DYNAMIC
int gfx_decoder_type = 0;
#endif

#if GFX_TV_DYNAMIC
int gfx_tv_type = 0;
#endif

#if GFX_I2C_DYNAMIC
int gfx_i2c_type = 0;
#endif

#if GFX_VGA_DYNAMIC
int gfx_vga_type = 0;
#endif

/* HEADER FILE FOR DURANGO ROUTINE DEFINITIONS
 * Needed since some of the Durango routines call other Durango routines.
 * Also defines the size of chipset array (GFX_CSPTR_SIZE).
 */
#include "gfx_rtns.h"			/* routine definitions */

/* DEFINE POINTERS TO MEMORY MAPPED REGIONS
 * These pointers are used by the Durango routines to access the hardware. 
 * The variables must be set by the project's initialization code after
 * mapping the regions in the appropriate manner. 
 */

/* DEFINE VIRTUAL ADDRESSES */
/* Note: These addresses define the starting base expected by all    */
/*       Durango offsets.  Under an OS that requires these pointers  */
/*       to be mapped to linear addresses (i.e Windows), it may not  */
/*       be possible to keep these base offsets.  In these cases,    */
/*       the addresses are modified to point to the beginning of the */
/*       relevant memory region and the access macros are adjusted   */
/*       to subtract the offset from the default base.  For example, */
/*       the register pointer could be moved to be 0x40008000, while */
/*       the WRITE_REG* macros are modified to subtract 0x8000 from  */
/*       the offset.                                                 */

unsigned char *gfx_virt_regptr = (unsigned char *)0x40000000;
unsigned char *gfx_virt_fbptr = (unsigned char *)0x40800000;
unsigned char *gfx_virt_vidptr = (unsigned char *)0x40010000;
unsigned char *gfx_virt_vipptr = (unsigned char *)0x40015000;
unsigned char *gfx_virt_spptr = (unsigned char *)0x40000000;
unsigned char *gfx_virt_gpptr = (unsigned char *)0x40000000;

/* DEFINE PHYSICAL ADDRESSES */

unsigned char *gfx_phys_regptr = (unsigned char *)0x40000000;
unsigned char *gfx_phys_fbptr = (unsigned char *)0x40800000;
unsigned char *gfx_phys_vidptr = (unsigned char *)0x40010000;
unsigned char *gfx_phys_vipptr = (unsigned char *)0x40015000;

/* HEADER FILE FOR GRAPHICS REGISTER DEFINITIONS 
 * This contains only constant definitions, so it should be able to be 
 * included in any software project as is.
 */
#include "gfx_regs.h"			/* graphics register definitions */

/* HEADER FILE FOR REGISTER ACCESS MACROS
 * This file contains the definitions of the WRITE_REG32 and similar macros
 * used by the Durango routines to access the hardware.  The file assumes 
 * that the environment can handle 32-bit pointer access.  If this is not
 * the case, or if there are special requirements, then this header file 
 * should not be included and the project must define the macros itself.
 * (A project may define WRITE_REG32 to call a routine, for example).
 */
#include "gfx_defs.h"			/* register access macros */

/* IO MACROS AND ROUTINES
 * These macros must be defined before the initialization or I2C 
 * routines will work properly. 
 */

#if defined(OS_WIN32)			/* For Windows */

/* VSA II CALL */

void
gfx_msr_asm_read(unsigned short msrReg, unsigned long msrAddr,
		 unsigned long *ptrHigh, unsigned long *ptrLow)
{
   unsigned long temp1, temp2;

   _asm {
      mov dx, 0x0AC1C
      mov eax, 0x0FC530007
      out dx, eax 
      add dl, 2 
      mov ecx, msrAddr 
      mov cx, msrReg 
      in ax, dx;
      ;EDX:EAX will contain MSR contents.
      mov temp1, edx 
      mov temp2, eax
   }

   *ptrHigh = temp1;
   *ptrLow = temp2;
}

void
gfx_msr_asm_write(unsigned short msrReg, unsigned long msrAddr,
		  unsigned long *ptrHigh, unsigned long *ptrLow)
{
   unsigned long temp1 = *ptrHigh;
   unsigned long temp2 = *ptrLow;

   _asm {
      mov dx, 0x0AC1C
      mov eax, 0x0FC530007 
      out dx, eax i
      add dl, 2 
      ;ECX contains msrAddr | msrReg 
      mov ecx, msrAddr
      mov cx, msrReg
      ;<OR_mask_hi > 
      mov ebx, temp1

      ;<OR_mask_hi > 
      mov eax, temp2
      ;<AND_mask_hi > 
      mov esi, 0
      ;<AND_mask_lo > 
      mov edi, 0
      ;MSR is written at this point
      out dx, ax
   }
}

unsigned char
gfx_inb(unsigned short port)
{
   unsigned char data;

   _asm {
      pushf 
      mov dx, port 
      in al, dx 
      mov data, al 
      popf
   }
   return (data);
}

unsigned short
gfx_inw(unsigned short port)
{
   unsigned short data;

   _asm {
      pushf 
      mov dx, port 
      in ax, dx 
      mov data, ax 
      popf
   }
   return (data);
}

unsigned long
gfx_ind(unsigned short port)
{
   unsigned long data;

   _asm {
      pushf 
      mov dx, port 
      in eax, dx 
      mov data, eax 
      popf
   }
   return (data);
}

void
gfx_outb(unsigned short port, unsigned char data)
{
   _asm {
      pushf 
      mov al, data 
      mov dx, port 
      out dx, al 
      popf
   }
}

void
gfx_outw(unsigned short port, unsigned short data)
{
   _asm {
      pushf 
      mov ax, data 
      mov dx, port 
      out dx, ax 
      popf
   }
}

void
gfx_outd(unsigned short port, unsigned long data)
{
   _asm {
      pushf 
      mov eax, data 
      mov dx, port 
      out dx, eax 
      popf
   }
}


#elif defined(OS_VXWORKS) || defined (OS_LINUX)	/* VxWorks and Linux */

#if defined(OS_LINUX)
#include "asm/msr.h"
#endif

void
gfx_msr_asm_read(unsigned short msrReg, unsigned long msrAddr,
		 unsigned long *ptrHigh, unsigned long *ptrLow)
{
   unsigned long addr, val1, val2;

   addr = msrAddr | (unsigned long)msrReg;
   rdmsr(addr, val1, val2);

   *ptrHigh = val2;
   *ptrLow = val1;
}

void
gfx_msr_asm_write(unsigned short msrReg, unsigned long msrAddr,
		  unsigned long *ptrHigh, unsigned long *ptrLow)
{
   unsigned long addr, val1, val2;

   val2 = *ptrHigh;
   val1 = *ptrLow;

   addr = (msrAddr & 0xFFFF0000) | (unsigned long)msrReg;
   wrmsr(addr, val1, val2);
}

unsigned char
gfx_inb(unsigned short port)
{
   unsigned char value;
   __asm__ volatile ("inb %1,%0":"=a" (value):"d"(port));

   return value;
}

unsigned short
gfx_inw(unsigned short port)
{
   unsigned short value;
   __asm__ volatile ("in %1,%0":"=a" (value):"d"(port));

   return value;
}

unsigned long
gfx_ind(unsigned short port)
{
   unsigned long value;
   __asm__ volatile ("inl %1,%0":"=a" (value):"d"(port));

   return value;
}

void
gfx_outb(unsigned short port, unsigned char data)
{
   __asm__ volatile ("outb %0,%1"::"a" (data), "d"(port));
}

void
gfx_outw(unsigned short port, unsigned short data)
{
   __asm__ volatile ("out %0,%1"::"a" (data), "d"(port));
}

void
gfx_outd(unsigned short port, unsigned long data)
{
   __asm__ volatile ("outl %0,%1"::"a" (data), "d"(port));
}

#else /* else nothing */

unsigned char
gfx_inb(unsigned short port)
{
   /* ADD OS SPECIFIC IMPLEMENTATION */
   return (0);
}

unsigned short
gfx_inw(unsigned short port)
{
   /* ADD OS SPECIFIC IMPLEMENTATION */
   return (0);
}

unsigned long
gfx_ind(unsigned short port)
{
   /* ADD OS SPECIFIC IMPLEMENTATION */
   return (0);
}

void
gfx_outb(unsigned short port, unsigned char data)
{
   /* ADD OS SPECIFIC IMPLEMENTATION */
}

void
gfx_outw(unsigned short port, unsigned short data)
{
   /* ADD OS SPECIFIC IMPLEMENTATION */
}

void
gfx_outd(unsigned short port, unsigned long data)
{
   /* ADD OS SPECIFIC IMPLEMENTATION */
}
#endif

#define INB(port) gfx_inb(port)
#define INW(port) gfx_inw(port)
#define IND(port) gfx_ind(port)
#define OUTB(port, data) gfx_outb(port, data)
#define OUTW(port, data) gfx_outw(port, data)
#define OUTD(port, data) gfx_outd(port, data)

/* INITIALIZATION ROUTINES 
 * These routines are used during the initialization of the driver to 
 * perform such tasks as detecting the type of CPU and video hardware.  
 * The routines require the use of IO, so the above IO routines need 
 * to be implemented before the initialization routines will work
 * properly.
 */

#include "gfx_init.c"

/* INCLUDE MSR ACCESS ROUTINES */

#include "gfx_msr.c"

/* INCLUDE GRAPHICS ENGINE ROUTINES 
 * These routines are used to program the 2D graphics accelerator.  If
 * the project does not use graphics acceleration (direct frame buffer
 * access only), then this file does not need to be included. 
 */
#include "gfx_rndr.c"			/* graphics engine routines */

/* INCLUDE DISPLAY CONTROLLER ROUTINES 
 * These routines are used if the display mode is set directly.  If the 
 * project uses VGA registers to set a display mode, then these files
 * do not need to be included.
 */
#include "gfx_mode.h"			/* display mode tables */
#include "gfx_disp.c"			/* display controller routines */

/* INCLUDE VIDEO OVERLAY ROUTINES
 * These routines control the video overlay hardware. 
 */
#include "gfx_vid.c"			/* video overlay routines */

/* VIDEO PORT AND VIDEO DECODER ROUTINES
 * These routines rely on the I2C routines.
 */
#include "gfx_vip.c"			/* video port routines */
#include "gfx_dcdr.c"			/* video decoder routines */

/* I2C BUS ACCESS ROUTINES
 * These routines are used by the video decoder and possibly an 
 * external TV encoer. 
 */
#include "gfx_i2c.c"			/* I2C bus access routines */

/* TV ENCODER ROUTINES
 * This file does not need to be included if the system does not
 * support TV output.
 */
#include "gfx_tv.c"			/* TV encoder routines */

/* VGA ROUTINES
 * This file is used if setting display modes using VGA registers.
 */
#include "gfx_vga.c"			/* VGA routines */

/* END OF FILE */

/*
 * Copyright 1997,1998 by Alan Hourihane, Wigan, England.
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Alan Hourihane not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  Alan Hourihane makes no representations
 * about the suitability of this software for any purpose.  It is provided
 * "as is" without express or implied warranty.
 *
 * ALAN HOURIHANE DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL ALAN HOURIHANE BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 *
 * Author:  Alan Hourihane, <alanh@fairlite.demon.co.uk>
 */
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/tga/tga_regs.h,v 1.12 2002/05/13 05:22:37 keithp Exp $ */

/* TGA hardware description (minimal)
 *
 * Offsets within Memory Space
 *
 * Portions taken from linux's own tga driver...
 * Courtesy of Jay Estabrook.
 */

#ifndef TGA_REGS_H
#define TGA_REGS_H

#include "xf86_ansic.h"
#include "compiler.h"

#define TYPE_TGA_8PLANE			0
#define TYPE_TGA_24PLANE		1
#define TYPE_TGA_24PLUSZ		3

#if 1
#define WMB	mem_barrier()
#else
#define WMB	write_mem_barrier()
#endif

#define TGA_WRITE_REG(v,r) \
	do {\
		 *(unsigned int *)((char*)(pTga->IOBase)+(r)) = (v);\
		 WMB;\
	} while (0)

#define TGA_READ_REG(r) \
	( *(unsigned int *)((char*)(pTga->IOBase)+(r)))

#define TGA2_WRITE_CLOCK_REG(v,r) \
	do {\
		 *(unsigned int *)((char*)(pTga->ClkBase)+(r)) = (v);\
		 WMB;\
	} while (0)

#define TGA2_WRITE_RAMDAC_REG(v,r) \
	do {\
		 *(unsigned int *)((char*)(pTga->DACBase)+(r)) = (v);\
		 WMB;\
	} while (0)

#define TGA2_READ_RAMDAC_REG(r) \
	( *(unsigned int *)((char*)(pTga->DACBase)+(r)))

#if defined(__alpha__) && 0 /* ?? disable this for now ?? */
/* we can avoid an mb() if we write to an alternate register space each time */

#define MAX_OFFSET 8192
#define OFFSET_INC 1024

#define TGA_DECL() register unsigned long iobase, offset
#define TGA_GET_IOBASE() iobase = (unsigned long)pTga->IOBase;
#define TGA_GET_OFFSET() offset = pTga->regOffset;
#define TGA_SAVE_OFFSET() pTga->regOffset = offset;

/* #define PROFILE */
#undef PROFILE

#ifdef PROFILE
static __inline__ unsigned int realcc()
{
  u_long cc;
  __asm__ volatile("rpcc %0" : "=r"(cc) : : "memory");
  return cc;
}

#define TGA_FAST_WRITE_REG(v,r) \
do {\
start = realcc();\
  *(unsigned int *)(iobase + offset + (r)) = v;\
  offset += OFFSET_INC;\
  if(offset > MAX_OFFSET) (offset = 0);\
  stop = realcc();\
  ErrorF("TGA_FAST_WRITE_REG = %d\n", stop - start);\
} while (0)

#else /* PROFILE */

#define TGA_FAST_WRITE_REG(v,r) \
do {\
  *(unsigned int *)(iobase + offset + (r)) = v;\
  offset += OFFSET_INC;\
  if(offset > MAX_OFFSET) (offset = 0);\
} while (0)
#endif /* PROFILE */

#else /* __alpha__ */

#define TGA_DECL()
#define TGA_GET_IOBASE() ;
#define TGA_GET_OFFSET() ;
#define TGA_SAVE_OFFSET() ;
#define TGA_FAST_WRITE_REG(v,r) TGA_WRITE_REG(v,r)

#endif /* __alpha__ */

#define	TGA_ROM_OFFSET			0x00000000
#define TGA2_CLOCK_OFFSET		0x00060000
#define TGA2_RAMDAC_OFFSET		0x00080000
#define	TGA_REGS_OFFSET			0x00100000
#define	TGA_8PLANE_FB_OFFSET		0x00200000
#define	TGA_24PLANE_FB_OFFSET		0x00800000
#define	TGA_24PLUSZ_FB_OFFSET		0x01000000

#define TGA_FOREGROUND_REG		0x0020
#define TGA_BACKGROUND_REG		0x0024
#define	TGA_PLANEMASK_REG		0x0028
#define	TGA_MODE_REG			0x0030
#define		SIMPLE			0x00
#define		Z3D			0x10
#define		OPAQUESTIPPLE		0x01
#define		FILL			0x20
#define		TRANSPARENTSTIPPLE	0x05
#define		BLOCKSTIPPLE		0x0D
#define		BLOCKFILL		0x2D
#define		OPAQUELINE		0x02
#define		TRANSPARENTLINE		0x06
#define		BPP8PACKED		(0x00 << 8)
#define		BPP8UNPACK		(0x01 << 8)
#define		BPP12LOW		(0x02 << 8)
#define		BPP12HIGH		(0x06 << 8)
#define		BPP24			(0x03 << 8)
#define		CAP_ENDS		0x8000
#define		X11			0x0000
#define		MODE_WIN32		0x2000
 /* copy mode */
#define         COPY                    0x07
 /* opaque fill mode */
#define         OPAQUEFILL              0x21
#define         TRANSPARENTFILL         0x45
#define	TGA_RASTEROP_REG		0x0034
#define TGA_PIXELSHIFT_REG              0x0038
#define TGA_ADDRESS_REG			0x003c
#define TGA_CONTINUE_REG                0x004c
#define	TGA_DEEP_REG			0x0050
#define	TGA_REVISION_REG		0x0054		/* TGA2 */
#define	TGA_PIXELMASK_REG		0x002c
#define	TGA_PIXELMASK_PERS_REG		0x005c
#define	TGA_CURSOR_BASE_REG		0x0060
#define	TGA_HORIZ_REG			0x0064
#define	TGA_VERT_REG			0x0068
#define	TGA_BASE_ADDR_REG		0x006c
#define	TGA_VALID_REG			0x0070
#define	TGA_CURSOR_XY_REG		0x0074
#define	TGA_INTR_STAT_REG		0x007c
 /* GDAR */
#define TGA_DATA_REG                    0x0080
#define TGA_WIDTH_REG                   0x009c
#define TGA_SPAN_REG			0x00bc
#define	TGA_RAMDAC_SETUP_REG		0x00c0

#define TGA_NOSLOPE7_REG                0x011C
#define TGA_NOSLOPE6_REG                0x0118
#define TGA_NOSLOPE5_REG                0x0114
#define TGA_NOSLOPE4_REG                0x0110
#define TGA_NOSLOPE3_REG                0x010C
#define TGA_NOSLOPE2_REG                0x0108
#define TGA_NOSLOPE1_REG                0x0104
#define TGA_NOSLOPE0_REG                0x0100

#define TGA_SLOPE0_REG                  0x0120
#define TGA_SLOPE1_REG                  0x0124
#define TGA_SLOPE2_REG                  0x0128
#define TGA_SLOPE3_REG                  0x012C
#define TGA_SLOPE4_REG                  0x0130
#define TGA_SLOPE5_REG                  0x0134
#define TGA_SLOPE6_REG                  0x0138
#define TGA_SLOPE7_REG                  0x013C
#define TGA_BRES3_REG                   0x0048
#define TGA_BRES2_REG                   0x0044
#define TGA_BRES1_REG                   0x0040

#define	TGA_BLOCK_COLOR0_REG		0x0140
#define	TGA_BLOCK_COLOR1_REG		0x0144
#define	TGA_BLOCK_COLOR2_REG		0x0148
#define	TGA_BLOCK_COLOR3_REG		0x014c
#define	TGA_BLOCK_COLOR4_REG		0x0150
#define	TGA_BLOCK_COLOR5_REG		0x0154
#define	TGA_BLOCK_COLOR6_REG		0x0158
#define	TGA_BLOCK_COLOR7_REG		0x015c
#define	TGA_CLOCK_REG			0x01e8
#define	TGA_RAMDAC_REG			0x01f0
#define	TGA_CMD_STAT_REG		0x01f8

#define	BT485_READ_BIT			0x01
#define	BT485_WRITE_BIT			0x00

#endif

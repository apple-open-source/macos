/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/sis/sis300_accel.h,v 1.8 2003/01/29 15:42:16 eich Exp $ */
/*
 * 2D acceleration for SiS530/620 and 300 series
 *
 * Copyright 1998,1999 by Alan Hourihane, Wigan, England.
 * Parts Copyright 2002 Thomas Winischhofer, Vienna, Austria
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
 * Authors:  Alan Hourihane, alanh@fairlite.demon.co.uk
 *           Mike Chapman <mike@paranoia.com>, 
 *           Juanjo Santamarta <santamarta@ctv.es>, 
 *           Mitani Hiroshi <hmitani@drl.mei.co.jp> 
 *           David Thomas <davtom@dream.org.uk>. 
 *           Xavier Ducoin <x.ducoin@lectra.com>
 *           Thomas Winischhofer <thomas@winischhofer.net>
 */


/* Definitions for the SIS engine communication. */

#define PATREGSIZE      384  /* Pattern register size. 384 bytes @ 0x8300 */
#define BR(x)   (0x8200 | (x) << 2)
#define PBR(x)  (0x8300 | (x) << 2)

/* SiS300 engine commands */
#define BITBLT                  0x00000000  /* Blit */
#define COLOREXP                0x00000001  /* Color expand */
#define ENCOLOREXP              0x00000002  /* Enhanced color expand */
#define MULTIPLE_SCANLINE       0x00000003  /* ? */
#define LINE                    0x00000004  /* Draw line */
#define TRAPAZOID_FILL          0x00000005  /* Fill trapezoid */
#define TRANSPARENT_BITBLT      0x00000006  /* Transparent Blit */

/* source select */
#define SRCVIDEO                0x00000000  /* source is video RAM */
#define SRCSYSTEM               0x00000010  /* source is system memory */
#define SRCCPUBLITBUF           SRCSYSTEM   /* source is CPU-driven BitBuffer (for color expand) */
#define SRCAGP                  0x00000020  /* source is AGP memory (?) */

/* Pattern flags */
#define PATFG                   0x00000000  /* foreground color */
#define PATPATREG               0x00000040  /* pattern in pattern buffer (0x8300) */
#define PATMONO                 0x00000080  /* mono pattern */

/* blitting direction */
#define X_INC                   0x00010000
#define X_DEC                   0x00000000
#define Y_INC                   0x00020000
#define Y_DEC                   0x00000000

/* Clipping flags */
#define NOCLIP                  0x00000000
#define NOMERGECLIP             0x04000000
#define CLIPENABLE              0x00040000
#define CLIPWITHOUTMERGE        0x04040000

/* Transparency */
#define OPAQUE                  0x00000000
#define TRANSPARENT             0x00100000

/* Trapezoid */
#define T_XISMAJORL             0x00800000  /* X axis is driving axis (left) */
#define T_XISMAJORR             0x01000000  /* X axis is driving axis (right) */
#define T_L_Y_INC               Y_INC       /* left edge direction Y */
#define T_L_X_INC               X_INC       /* left edge direction X */
#define T_R_Y_INC               0x00400000  /* right edge direction Y */
#define T_R_X_INC               0x00200000  /* right edge direction X */

/* ? */
#define DSTAGP                  0x02000000
#define DSTVIDEO                0x02000000

/* Line */
#define LINE_STYLE              0x00800000
#define NO_RESET_COUNTER        0x00400000
#define NO_LAST_PIXEL           0x00200000


/* Macros to do useful things with the SIS BitBLT engine */

/* BR(16) (0x8240):

   bit 31 2D engine: 1 is idle,
   bit 30 3D engine: 1 is idle,
   bit 29 Command queue: 1 is empty

   bits 28:24: Current CPU driven BitBlt buffer stage bit[4:0]

   bits 15:0:  Current command queue length (530/620: 12:0)

*/

/* TW: BR(16)+2 = 0x8242 */

#define CmdQueLen pSiS->cmdQueueLen

#define SiSIdle \
  { \
  while( (MMIO_IN16(pSiS->IOBase, BR(16)+2) & 0xE000) != 0xE000){}; \
  while( (MMIO_IN16(pSiS->IOBase, BR(16)+2) & 0xE000) != 0xE000){}; \
  while( (MMIO_IN16(pSiS->IOBase, BR(16)+2) & 0xE000) != 0xE000){}; \
  CmdQueLen = (MMIO_IN16(pSiS->IOBase, 0x8240) & pSiS->CmdQueLenMask) - pSiS->CmdQueLenFix; \
  }
/* TW: (do three times, because 2D engine seems quite unsure about whether or not it's idle) */

#define SiSSetupSRCBase(base) \
                if (CmdQueLen <= 0)  SiSIdle;\
                MMIO_OUT32(pSiS->IOBase, BR(0), base);\
                CmdQueLen--;

#define SiSSetupSRCPitch(pitch) \
                if (CmdQueLen <= 0)  SiSIdle;\
                MMIO_OUT16(pSiS->IOBase, BR(1), pitch);\
                CmdQueLen--;

#define SiSSetupSRCXY(x,y) \
                if (CmdQueLen <= 0)  SiSIdle;\
                MMIO_OUT32(pSiS->IOBase, BR(2), (x)<<16 | (y) );\
                CmdQueLen--;

#define SiSSetupDSTBase(base) \
                if (CmdQueLen <= 0)  SiSIdle;\
                MMIO_OUT32(pSiS->IOBase, BR(4), base);\
                CmdQueLen--;

#define SiSSetupDSTXY(x,y) \
                if (CmdQueLen <= 0)  SiSIdle;\
                MMIO_OUT32(pSiS->IOBase, BR(3), (x)<<16 | (y) );\
                CmdQueLen--;

#define SiSSetupDSTRect(x,y) \
                if (CmdQueLen <= 0)  SiSIdle;\
                MMIO_OUT32(pSiS->IOBase, BR(5), (y)<<16 | (x) );\
                CmdQueLen--;

#define SiSSetupDSTColorDepth(bpp) \
                if(pSiS->VGAEngine != SIS_530_VGA) { \
                  if (CmdQueLen <= 0)  SiSIdle;\
                  MMIO_OUT16(pSiS->IOBase, BR(1)+2, bpp);\
                  CmdQueLen--; \
		}

#define SiSSetupRect(w,h) \
                if (CmdQueLen <= 0)  SiSIdle;\
                MMIO_OUT32(pSiS->IOBase, BR(6), (h)<<16 | (w) );\
                CmdQueLen--;

#define SiSSetupPATFG(color) \
                if (CmdQueLen <= 0)  SiSIdle;\
                MMIO_OUT32(pSiS->IOBase, BR(7), color);\
                CmdQueLen--;

#define SiSSetupPATBG(color) \
                if (CmdQueLen <= 0)  SiSIdle;\
                MMIO_OUT32(pSiS->IOBase, BR(8), color);\
                CmdQueLen--;

#define SiSSetupSRCFG(color) \
                if (CmdQueLen <= 0)  SiSIdle;\
                MMIO_OUT32(pSiS->IOBase, BR(9), color);\
                CmdQueLen--;

#define SiSSetupSRCBG(color) \
                if (CmdQueLen <= 0)  SiSIdle;\
                MMIO_OUT32(pSiS->IOBase, BR(10), color);\
                CmdQueLen--;

/* 0x8224 src colorkey high */
/* 0x8228 src colorkey low */
/* 0x821c dest colorkey high */
/* 0x8220 dest colorkey low */
#define SiSSetupSRCTrans(color) \
                if (CmdQueLen <= 1)  SiSIdle;\
                MMIO_OUT32(pSiS->IOBase, 0x8224, color);\
		MMIO_OUT32(pSiS->IOBase, 0x8228, color);\
		CmdQueLen -= 2;

#define SiSSetupDSTTrans(color) \
		if (CmdQueLen <= 1)  SiSIdle;\
		MMIO_OUT32(pSiS->IOBase, 0x821C, color); \
		MMIO_OUT32(pSiS->IOBase, 0x8220, color); \
                CmdQueLen -= 2;

#define SiSSetupMONOPAT(p0,p1) \
                if (CmdQueLen <= 1)  SiSIdle;\
                MMIO_OUT32(pSiS->IOBase, BR(11), p0);\
                MMIO_OUT32(pSiS->IOBase, BR(12), p1);\
                CmdQueLen -= 2;

#define SiSSetupClipLT(left,top) \
                if (CmdQueLen <= 0)  SiSIdle;\
                MMIO_OUT32(pSiS->IOBase, BR(13), ((left) & 0xFFFF) | (top)<<16 );\
                CmdQueLen--;

#define SiSSetupClipRB(right,bottom) \
                if (CmdQueLen <= 0)  SiSIdle;\
                MMIO_OUT32(pSiS->IOBase, BR(14), ((right) & 0xFFFF) | (bottom)<<16 );\
                CmdQueLen--;

/* General */
#define SiSSetupROP(rop) \
                pSiS->CommandReg = (rop) << 8;

#define SiSSetupCMDFlag(flags) \
                pSiS->CommandReg |= (flags);

#define SiSDoCMD \
                if (CmdQueLen <= 1)  SiSIdle;\
                MMIO_OUT32(pSiS->IOBase, BR(15), pSiS->CommandReg); \
		CmdQueLen--; \
		if(pSiS->VGAEngine != SIS_530_VGA) { \
                   MMIO_OUT32(pSiS->IOBase, BR(16), 0);\
                   CmdQueLen--; \
		} else { \
		   unsigned long temp; \
		   temp = MMIO_IN32(pSiS->IOBase, BR(16)); \
		} \

/* Line */
#define SiSSetupX0Y0(x,y) \
                if (CmdQueLen <= 0)  SiSIdle;\
                MMIO_OUT32(pSiS->IOBase, BR(2), (y)<<16 | (x) );\
                CmdQueLen--;

#define SiSSetupX1Y1(x,y) \
                if (CmdQueLen <= 0)  SiSIdle;\
                MMIO_OUT32(pSiS->IOBase, BR(3), (y)<<16 | (x) );\
                CmdQueLen--;

#define SiSSetupLineCount(c) \
                if (CmdQueLen <= 0)  SiSIdle;\
                MMIO_OUT16(pSiS->IOBase, BR(6), c);\
                CmdQueLen--;

#define SiSSetupStylePeriod(p) \
                if (CmdQueLen <= 0)  SiSIdle;\
                MMIO_OUT16(pSiS->IOBase, BR(6)+2, p);\
                CmdQueLen--;

#define SiSSetupStyleLow(ls) \
                if (CmdQueLen <= 0)  SiSIdle;\
                MMIO_OUT32(pSiS->IOBase, BR(11), ls);\
                CmdQueLen--;

#define SiSSetupStyleHigh(ls) \
                if (CmdQueLen <= 0)  SiSIdle;\
                MMIO_OUT32(pSiS->IOBase, BR(12), ls);\
                CmdQueLen--;

/* TW: Trapezoid */
#define SiSSetupYH(y,h) \
                if (CmdQueLen <= 0)  SiSIdle;\
                MMIO_OUT32(pSiS->IOBase, 0x8208, (y)<<16 | (h) );\
                CmdQueLen--;

#define SiSSetupLR(left,right) \
                if (CmdQueLen <= 0)  SiSIdle;\
                MMIO_OUT32(pSiS->IOBase, 0x820C, (right)<<16 | (left) );\
                CmdQueLen--;

#define SiSSetupdL(dxL,dyL) \
                if (CmdQueLen <= 0)  SiSIdle;\
                MMIO_OUT32(pSiS->IOBase, 0x8244, (dyL)<<16 | (dxL) );\
                CmdQueLen--;

#define SiSSetupdR(dxR,dyR) \
                if (CmdQueLen <= 0)  SiSIdle;\
		MMIO_OUT32(pSiS->IOBase, 0x8248, (dyR)<<16 | (dxR) );\
                CmdQueLen--;

#define SiSSetupEL(eL) \
                if (CmdQueLen <= 0)  SiSIdle;\
		MMIO_OUT32(pSiS->IOBase, 0x824C, eL);\
		CmdQueLen--;

#define SiSSetupER(eR) \
                if (CmdQueLen <= 0)  SiSIdle;\
                MMIO_OUT32(pSiS->IOBase, 0x8250, eR);\
                CmdQueLen--;


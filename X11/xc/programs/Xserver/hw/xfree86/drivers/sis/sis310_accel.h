/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/sis/sis310_accel.h,v 1.2 2003/01/29 15:42:16 eich Exp $ */
/*
 * Copyright 2002 by Thomas Winischhofer, Vienna, Austria
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Thomas Winischhofer not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  Thomas Winischhofer makes no representations
 * about the suitability of this software for any purpose.  It is provided
 * "as is" without express or implied warranty.
 *
 * THOMAS WINISCHHOFER DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL THOMAS WINISCHHOFER BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 *
 * Based on sis300_accel.h
 *
 * Author:  Thomas Winischhofer <thomas@winischhofer.net>
 *
 */

/* Definitions for the SIS engine communication. */


/* SiS310 engine commands */
#define BITBLT                  0x00000000  /* Blit */
#define COLOREXP                0x00000001  /* Color expand */
#define ENCOLOREXP              0x00000002  /* Enhanced color expand */
#define MULTIPLE_SCANLINE       0x00000003  /* ? */
#define LINE                    0x00000004  /* Draw line */
#define TRAPAZOID_FILL          0x00000005  /* Fill trapezoid */
#define TRANSPARENT_BITBLT      0x00000006  /* Transparent Blit */
#define ALPHA_BLEND		0x00000007  /* Alpha blend ? */
#define A3D_FUNCTION		0x00000008  /* 3D command ? */
#define	CLEAR_Z_BUFFER		0x00000009  /* ? */
#define GRADIENT_FILL		0x0000000A  /* Gradient fill */
#define STRETCH_BITBLT		0x0000000B  /* Stretched Blit */

/* Command bits */

/* Source selection */
#define SRCVIDEO                0x00000000  /* source is video RAM */
#define SRCSYSTEM               0x00000010  /* source is system memory */
#define SRCCPUBLITBUF           SRCSYSTEM   /* source is CPU-driven BitBuffer (for color expand) */
#define SRCAGP                  0x00000020  /* source is AGP memory (?) */

/* Pattern source selection */
#define PATFG                   0x00000000  /* foreground color */
#define PATPATREG               0x00000040  /* pattern in pattern buffer (0x8300) */
#define PATMONO                 0x00000080  /* mono pattern */

/* Clipping flags */
#define NOCLIP                  0x00000000
#define NOMERGECLIP             0x04000000
#define CLIPENABLE              0x00040000
#define CLIPWITHOUTMERGE        0x04040000

/* Transparency */
#define OPAQUE                  0x00000000
#define TRANSPARENT             0x00100000

/* ? */
#define DSTAGP                  0x02000000
#define DSTVIDEO                0x02000000

/* Subfunctions for Color/Enhanced Color Expansion */
#define COLOR_TO_MONO		0x00100000
#define AA_TEXT			0x00200000

/* Line */
#define LINE_STYLE              0x00800000
#define NO_RESET_COUNTER        0x00400000
#define NO_LAST_PIXEL           0x00200000

/* Trapezoid */
#define T_XISMAJORL             0x00800000  /* X axis is driving axis (left) */
#define T_XISMAJORR             0x08000000  /* X axis is driving axis (right) */
#define T_L_Y_INC               0x00000020  /* left edge direction Y */
#define T_L_X_INC               0x00000010  /* left edge direction X */
#define T_R_Y_INC               0x00400000  /* right edge direction Y */
#define T_R_X_INC               0x00200000  /* right edge direction X */

/* Some general registers */
#define SRC_ADDR		0x8200
#define SRC_PITCH		0x8204
#define AGP_BASE		0x8206 /* color-depth dependent value */
#define SRC_Y			0x8208
#define SRC_X			0x820A
#define DST_Y			0x820C
#define DST_X			0x820E
#define DST_ADDR		0x8210
#define DST_PITCH		0x8214
#define DST_HEIGHT		0x8216
#define RECT_WIDTH		0x8218
#define RECT_HEIGHT		0x821A
#define PAT_FGCOLOR		0x821C
#define PAT_BGCOLOR		0x8220
#define SRC_FGCOLOR		0x8224
#define SRC_BGCOLOR		0x8228
#define MONO_MASK		0x822C
#define LEFT_CLIP		0x8234
#define TOP_CLIP		0x8236
#define RIGHT_CLIP		0x8238
#define BOTTOM_CLIP		0x823A
#define COMMAND_READY		0x823C
#define FIRE_TRIGGER      	0x8240

#define PATTERN_REG		0x8300  /* 384 bytes pattern buffer */

/* Line registers */
#define LINE_X0			SRC_Y
#define LINE_X1			DST_Y
#define LINE_Y0			SRC_X
#define LINE_Y1			DST_X
#define LINE_COUNT		RECT_WIDTH
#define LINE_STYLE_PERIOD	RECT_HEIGHT
#define LINE_STYLE_0		MONO_MASK
#define LINE_STYLE_1		0x8230
#define LINE_XN			PATTERN_REG
#define LINE_YN			PATTERN_REG+2

/* Transparent bitblit registers */
#define TRANS_DST_KEY_HIGH	PAT_FGCOLOR
#define TRANS_DST_KEY_LOW	PAT_BGCOLOR
#define TRANS_SRC_KEY_HIGH	SRC_FGCOLOR
#define TRANS_SRC_KEY_LOW	SRC_BGCOLOR

/* Trapezoid registers */
#define TRAP_YH                 SRC_Y    /* 0x8208 */
#define TRAP_LR                 DST_Y    /* 0x820C */
#define TRAP_DL                 0x8244
#define TRAP_DR                 0x8248
#define TRAP_EL                 0x824C
#define TRAP_ER                 0x8250

/* Queue */
#define Q_BASE_ADDR		0x85C0  /* Base address of software queue (?) */
#define Q_WRITE_PTR		0x85C4  /* Current write pointer (?) */
#define Q_READ_PTR		0x85C8  /* Current read pointer (?) */
#define Q_STATUS		0x85CC  /* queue status */

/* Macros to do useful things with the SIS 310 BitBLT engine */

/* Q_STATUS:
   bit 31 = 1: All engines idle and all queues empty
   bit 30 = 1: Hardware Queue (=HW CQ, 2D queue, 3D queue) empty
   bit 29 = 1: 2D engine is idle
   bit 28 = 1: 3D engine is idle
   bit 27 = 1: HW command queue empty
   bit 26 = 1: 2D queue empty
   bit 25 = 1: 3D queue empty
   bit 24 = 1: SW command queue empty
   bits 23:16: 2D counter 3
   bits 15:8:  2D counter 2
   bits 7:0:   2D counter 1

   Where is the command queue length (current amount of commands the queue
   can accept) on the 310 series? (The current implementation is taken
   from 300 series and certainly wrong...)
*/

int     CmdQueLen;

/* TW: FIXME: CmdQueLen is... where....? */
#define SiSIdle \
  { \
  while( (MMIO_IN16(pSiS->IOBase, Q_STATUS+2) & 0x8000) != 0x8000){}; \
  while( (MMIO_IN16(pSiS->IOBase, Q_STATUS+2) & 0x8000) != 0x8000){}; \
  CmdQueLen=MMIO_IN16(pSiS->IOBase, Q_STATUS); \
  }
  /* TW: (do twice like on 300 series?) */

#define SiSSetupSRCBase(base) \
      if (CmdQueLen <= 0)  SiSIdle;\
      MMIO_OUT32(pSiS->IOBase, SRC_ADDR, base);\
      CmdQueLen--;

#define SiSSetupSRCPitch(pitch) \
      if (CmdQueLen <= 0)  SiSIdle;\
      MMIO_OUT16(pSiS->IOBase, SRC_PITCH, pitch);\
      CmdQueLen--;

#define SiSSetupSRCXY(x,y) \
      if (CmdQueLen <= 0)  SiSIdle;\
      MMIO_OUT32(pSiS->IOBase, SRC_Y, (x)<<16 | (y) );\
      CmdQueLen--;

#define SiSSetupDSTBase(base) \
      if (CmdQueLen <= 0)  SiSIdle;\
      MMIO_OUT32(pSiS->IOBase, DST_ADDR, base);\
      CmdQueLen--;

#define SiSSetupDSTXY(x,y) \
      if (CmdQueLen <= 0)  SiSIdle;\
      MMIO_OUT32(pSiS->IOBase, DST_Y, (x)<<16 | (y) );\
      CmdQueLen--;

#define SiSSetupDSTRect(x,y) \
      if (CmdQueLen <= 0)  SiSIdle;\
      MMIO_OUT32(pSiS->IOBase, DST_PITCH, (y)<<16 | (x) );\
      CmdQueLen--;

#define SiSSetupDSTColorDepth(bpp) \
      if (CmdQueLen <= 0)  SiSIdle;\
      MMIO_OUT16(pSiS->IOBase, AGP_BASE, bpp);\
      CmdQueLen--;

#define SiSSetupRect(w,h) \
      if (CmdQueLen <= 0)  SiSIdle;\
      MMIO_OUT32(pSiS->IOBase, RECT_WIDTH, (h)<<16 | (w) );\
      CmdQueLen--;

#define SiSSetupPATFG(color) \
      if (CmdQueLen <= 0)  SiSIdle;\
      MMIO_OUT32(pSiS->IOBase, PAT_FGCOLOR, color);\
      CmdQueLen--;

#define SiSSetupPATBG(color) \
      if (CmdQueLen <= 0)  SiSIdle;\
      MMIO_OUT32(pSiS->IOBase, PAT_BGCOLOR, color);\
      CmdQueLen--;

#define SiSSetupSRCFG(color) \
      if (CmdQueLen <= 0)  SiSIdle;\
      MMIO_OUT32(pSiS->IOBase, SRC_FGCOLOR, color);\
      CmdQueLen--;

#define SiSSetupSRCBG(color) \
      if (CmdQueLen <= 0)  SiSIdle;\
      MMIO_OUT32(pSiS->IOBase, SRC_BGCOLOR, color);\
      CmdQueLen--;

#define SiSSetupSRCTrans(color) \
      if (CmdQueLen <= 1)  SiSIdle;\
      MMIO_OUT32(pSiS->IOBase, TRANS_SRC_KEY_HIGH, color);\
      MMIO_OUT32(pSiS->IOBase, TRANS_SRC_KEY_LOW, color);\
      CmdQueLen -= 2;

#define SiSSetupDSTTrans(color) \
      if (CmdQueLen <= 1)  SiSIdle;\
      MMIO_OUT32(pSiS->IOBase, TRANS_DST_KEY_HIGH, color); \
      MMIO_OUT32(pSiS->IOBase, TRANS_DST_KEY_LOW, color); \
      CmdQueLen -= 2;

#define SiSSetupMONOPAT(p0,p1) \
      if (CmdQueLen <= 1)  SiSIdle;\
      MMIO_OUT32(pSiS->IOBase, MONO_MASK, p0);\
      MMIO_OUT32(pSiS->IOBase, MONO_MASK+4, p1);\
      CmdQueLen=CmdQueLen-2;

#define SiSSetupClipLT(left,top) \
      if (CmdQueLen <= 0)  SiSIdle;\
      MMIO_OUT32(pSiS->IOBase, LEFT_CLIP, ((left) & 0xFFFF) | (top)<<16 );\
      CmdQueLen--;

#define SiSSetupClipRB(right,bottom) \
      if (CmdQueLen <= 0)  SiSIdle;\
      MMIO_OUT32(pSiS->IOBase, RIGHT_CLIP, ((right) & 0xFFFF) | (bottom)<<16 );\
      CmdQueLen--;

#define SiSSetupROP(rop) \
      pSiS->CommandReg = (rop) << 8;

#define SiSSetupCMDFlag(flags) \
      pSiS->CommandReg |= (flags);

#define SiSDoCMD \
      if (CmdQueLen <= 1)  SiSIdle;\
      MMIO_OUT32(pSiS->IOBase, COMMAND_READY, pSiS->CommandReg); \
      MMIO_OUT32(pSiS->IOBase, FIRE_TRIGGER, 0); \
      CmdQueLen=CmdQueLen-2;

#define SiSSetupX0Y0(x,y) \
      if (CmdQueLen <= 0)  SiSIdle;\
      MMIO_OUT32(pSiS->IOBase, LINE_X0, (y)<<16 | (x) );\
      CmdQueLen--;

#define SiSSetupX1Y1(x,y) \
      if (CmdQueLen <= 0)  SiSIdle;\
      MMIO_OUT32(pSiS->IOBase, LINE_X1, (y)<<16 | (x) );\
      CmdQueLen--;

#define SiSSetupLineCount(c) \
      if (CmdQueLen <= 0)  SiSIdle;\
      MMIO_OUT16(pSiS->IOBase, LINE_COUNT, c);\
      CmdQueLen--;

#define SiSSetupStylePeriod(p) \
      if (CmdQueLen <= 0)  SiSIdle;\
      MMIO_OUT16(pSiS->IOBase, LINE_STYLE_PERIOD, p);\
      CmdQueLen--;

#define SiSSetupStyleLow(ls) \
      if (CmdQueLen <= 0)  SiSIdle;\
      MMIO_OUT32(pSiS->IOBase, LINE_STYLE_0, ls);\
      CmdQueLen--;

#define SiSSetupStyleHigh(ls) \
      if (CmdQueLen <= 0)  SiSIdle;\
      MMIO_OUT32(pSiS->IOBase, LINE_STYLE_1, ls);\
      CmdQueLen--;

/* Trapezoid */
#define SiSSetupYH(y,h) \
      if (CmdQueLen <= 0)  SiSIdle;\
      MMIO_OUT32(pSiS->IOBase, TRAP_YH, (y)<<16 | (h) );\
      CmdQueLen --;

#define SiSSetupLR(left,right) \
      if (CmdQueLen <= 0)  SiSIdle;\
      MMIO_OUT32(pSiS->IOBase, TRAP_LR, (right)<<16 | (left) );\
      CmdQueLen --;

#define SiSSetupdL(dxL,dyL) \
      if (CmdQueLen <= 0)  SiSIdle;\
      MMIO_OUT32(pSiS->IOBase, TRAP_DL, (dyL)<<16 | (dxL) );\
      CmdQueLen --;

#define SiSSetupdR(dxR,dyR) \
      if (CmdQueLen <= 0)  SiSIdle;\
      MMIO_OUT32(pSiS->IOBase, TRAP_DR, (dyR)<<16 | (dxR) );\
      CmdQueLen --;

#define SiSSetupEL(eL) \
      if (CmdQueLen <= 0)  SiSIdle;\
      MMIO_OUT32(pSiS->IOBase, TRAP_EL, eL);\
      CmdQueLen --;

#define SiSSetupER(eR) \
      if (CmdQueLen <= 0)  SiSIdle;\
      MMIO_OUT32(pSiS->IOBase, TRAP_ER, eR);\
      CmdQueLen --;



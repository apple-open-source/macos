/*
 * TCX framebuffer - hardware registers.
 *
 * Copyright (C) 2000 Jakub Jelinek (jakub@redhat.com)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * JAKUB JELINEK BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/suntcx/tcx_regs.h,v 1.1 2000/06/30 17:15:17 dawes Exp $ */

#ifndef TCX_REGS_H
#define TCX_REGS_H

/* THC definitions */
#define TCX_THC_MISC_REV_SHIFT       16
#define TCX_THC_MISC_REV_MASK        15
#define TCX_THC_MISC_RESET           (1 << 12)
#define TCX_THC_MISC_VIDEO           (1 << 10)
#define TCX_THC_MISC_SYNC            (1 << 9)
#define TCX_THC_MISC_VSYNC           (1 << 8)
#define TCX_THC_MISC_SYNC_ENAB       (1 << 7)
#define TCX_THC_MISC_CURS_RES        (1 << 6)
#define TCX_THC_MISC_INT_ENAB        (1 << 5)
#define TCX_THC_MISC_INT             (1 << 4)
#define TCX_THC_MISC_INIT            0x9f

typedef struct tcx_thc {
        unsigned int		thc_pad0[512];
	volatile unsigned int	thc_hs;		/* hsync timing */
	volatile unsigned int	thc_hsdvs;
	volatile unsigned int	thc_hd;
	volatile unsigned int	thc_vs;		/* vsync timing */
	volatile unsigned int	thc_vd;
	volatile unsigned int	thc_refresh;
	volatile unsigned int	thc_misc;
	unsigned int		thc_pad1[56];
	volatile unsigned int	thc_cursxy;	/* cursor x,y position (16 bits each) */
	volatile unsigned int	thc_cursmask[32];/* cursor mask bits */
	volatile unsigned int	thc_cursbits[32];/* what to show where mask enabled */
} TcxThc, *TcxThcPtr;

#endif /* TCX_REGS_H */

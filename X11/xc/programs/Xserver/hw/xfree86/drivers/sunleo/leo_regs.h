/*
 * Acceleration for the Leo(ZX) framebuffer - register layout.
 *
 * Copyright (C) 1999, 2000 Jakub Jelinek (jakub@redhat.com)
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
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/sunleo/leo_regs.h,v 1.1 2000/05/18 23:21:40 dawes Exp $ */

#ifndef LEOREGS_H
#define LEOREGS_H

/* rop register */
#define LEO_ATTR_PICK_DISABLE	0x00000000
#define LEO_ATTR_PICK_2D	0x80000000
#define LEO_ATTR_PICK_3D	0xa0000000
#define LEO_ATTR_PICK_2D_REND	0xc0000000
#define LEO_ATTR_PICK_3D_REND	0xe0000000

#define LEO_ATTR_DCE_DISABLE	0x00000000
#define LEO_ATTR_DCE_ENABLE	0x10000000

#define LEO_ATTR_APE_DISABLE	0x00000000
#define LEO_ATTR_APE_ENABLE	0x08000000

#define LEO_ATTR_COLOR_VAR	0x00000000
#define LEO_ATTR_COLOR_CONST	0x04000000

#define LEO_ATTR_AA_DISABLE	0x02000000
#define LEO_ATTR_AA_ENABLE	0x01000000

#define LEO_ATTR_ABE_BG		0x00000000	/* dst + alpha * (src - bg) */
#define LEO_ATTR_ABE_FB		0x00800000	/* dst + alpha * (src - dst) */

#define LEO_ATTR_ABE_DISABLE	0x00000000
#define LEO_ATTR_ABE_ENABLE	0x00400000

#define LEO_ATTR_BLTSRC_A	0x00000000
#define LEO_ATTR_BLTSRC_B	0x00200000

#define LEO_ROP_ZERO		(0x0 << 18)
#define LEO_ROP_NEW_AND_OLD	(0x8 << 18)
#define LEO_ROP_NEW_AND_NOLD	(0x4 << 18)
#define LEO_ROP_NEW		(0xc << 18)
#define LEO_ROP_NNEW_AND_OLD	(0x2 << 18)
#define LEO_ROP_OLD		(0xa << 18)
#define LEO_ROP_NEW_XOR_OLD	(0x6 << 18)
#define LEO_ROP_NEW_OR_OLD	(0xe << 18)
#define LEO_ROP_NNEW_AND_NOLD	(0x1 << 18)
#define LEO_ROP_NNEW_XOR_NOLD	(0x9 << 18)
#define LEO_ROP_NOLD		(0x5 << 18)
#define LEO_ROP_NEW_OR_NOLD	(0xd << 18)
#define LEO_ROP_NNEW		(0x3 << 18)
#define LEO_ROP_NNEW_OR_OLD	(0xb << 18)
#define LEO_ROP_NNEW_OR_NOLD	(0x7 << 18)
#define LEO_ROP_ONES		(0xf << 18)

#define LEO_ATTR_HSR_DISABLE	0x00000000
#define LEO_ATTR_HSR_ENABLE	0x00020000

#define LEO_ATTR_WRITEZ_DISABLE	0x00000000
#define LEO_ATTR_WRITEZ_ENABLE	0x00010000

#define LEO_ATTR_Z_VAR		0x00000000
#define LEO_ATTR_Z_CONST	0x00008000

#define LEO_ATTR_WCLIP_DISABLE	0x00000000
#define LEO_ATTR_WCLIP_ENABLE	0x00004000

#define LEO_ATTR_MONO		0x00000000
#define LEO_ATTR_STEREO_LEFT	0x00001000
#define LEO_ATTR_STEREO_RIGHT	0x00003000

#define LEO_ATTR_WE_DISABLE	0x00000000
#define LEO_ATTR_WE_ENABLE	0x00000800

#define LEO_ATTR_FCE_DISABLE	0x00000000
#define LEO_ATTR_FCE_ENABLE	0x00000400

#define LEO_ATTR_RE_DISABLE	0x00000000
#define LEO_ATTR_RE_ENABLE	0x00000200

#define LEO_ATTR_GE_DISABLE	0x00000000
#define LEO_ATTR_GE_ENABLE	0x00000100

#define LEO_ATTR_BE_DISABLE	0x00000000
#define LEO_ATTR_BE_ENABLE	0x00000080

#define LEO_ATTR_RGBE_DISABLE	0x00000000
#define LEO_ATTR_RGBE_ENABLE	0x00000380

#define LEO_ATTR_OE_DISABLE	0x00000000
#define LEO_ATTR_OE_ENABLE	0x00000040

#define LEO_ATTR_ZE_DISABLE	0x00000000
#define LEO_ATTR_ZE_ENABLE	0x00000020

#define LEO_ATTR_FORCE_WID	0x00000010

#define LEO_ATTR_FC_PLANE_MASK	0x0000000e

#define LEO_ATTR_BUFFER_A	0x00000000
#define LEO_ATTR_BUFFER_B	0x00000001

/* csr */
#define LEO_CSR_BLT_BUSY	0x20000000

typedef struct LeoDraw {
	unsigned char		xxx0[0xe00];
	volatile unsigned int	csr;
	volatile unsigned int	wid;
	volatile unsigned int	wmask;
	volatile unsigned int	widclip;
	volatile unsigned int	vclipmin;
	volatile unsigned int	vclipmax;
	volatile unsigned int	pickmin;	/* SS1 only */
	volatile unsigned int	pickmax;	/* SS1 only */
	volatile unsigned int	fg;
	volatile unsigned int	bg;
	volatile unsigned int	src;		/* Copy/Scroll (SS0 only) */
	volatile unsigned int	dst;		/* Copy/Scroll/Fill (SS0 only) */
	volatile unsigned int	extent;		/* Copy/Scroll/Fill size (SS0 only) */
	unsigned int		xxx1[3];
	volatile unsigned int	setsem;		/* SS1 only */
	volatile unsigned int	clrsem;		/* SS1 only */
	volatile unsigned int	clrpick;	/* SS1 only */
	volatile unsigned int	clrdat;		/* SS1 only */
	volatile unsigned int	alpha;		/* SS1 only */
	unsigned char		xxx2[0x2c];
	volatile unsigned int	winbg;
	volatile unsigned int	planemask;
	volatile unsigned int	rop;
	volatile unsigned int	z;
	volatile unsigned int	dczf;		/* SS1 only */
	volatile unsigned int	dczb;		/* SS1 only */
	volatile unsigned int	dcs;		/* SS1 only */
	volatile unsigned int	dczs;		/* SS1 only */
	volatile unsigned int	pickfb;		/* SS1 only */
	volatile unsigned int	pickbb;		/* SS1 only */
	volatile unsigned int	dcfc;		/* SS1 only */
	volatile unsigned int	forcecol;	/* SS1 only */
	volatile unsigned int	door[8];	/* SS1 only */
	volatile unsigned int	pick[5];	/* SS1 only */
} LeoDraw;

#define LEO_ADDRSPC_OBGR	0x00
#define LEO_ADDRSPC_Z		0x01
#define LEO_ADDRSPC_W		0x02
#define LEO_ADDRSPC_FONT_OBGR	0x04
#define LEO_ADDRSPC_FONT_Z	0x05
#define LEO_ADDRSPC_FONT_W	0x06
#define LEO_ADDRSPC_O		0x08
#define LEO_ADDRSPC_B		0x09
#define LEO_ADDRSPC_G		0x0a
#define LEO_ADDRSPC_R		0x0b

typedef struct LeoCommand0 {
	volatile unsigned int	csr;
	volatile unsigned int	addrspace;
	volatile unsigned int 	fontmsk;
	volatile unsigned int	fontt;
	volatile unsigned int	extent;
	volatile unsigned int	src;
	unsigned int		dst;
	volatile unsigned int	copy;
	volatile unsigned int	fill;
} LeoCommand0;

typedef struct LeoCross {
	volatile unsigned int	type;
	volatile unsigned int	csr;
	volatile unsigned int	value;
} LeoCross;

typedef struct LeoCursor {
	unsigned char		xxx0[16];
	volatile unsigned int	cur_type;
	volatile unsigned int	cur_misc;
	volatile unsigned int	cur_cursxy;
	volatile unsigned int	cur_data;
} LeoCursor;

#endif /* LEOREGS_H */

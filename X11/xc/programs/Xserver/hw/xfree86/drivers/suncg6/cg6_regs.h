/*
 * GX and Turbo GX framebuffer - hardware registers.
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
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/suncg6/cg6_regs.h,v 1.1 2000/05/23 04:47:43 dawes Exp $ */

#ifndef CG6_REGS_H
#define CG6_REGS_H

/* FHC definitions */
#define CG6_FHC_FBID_SHIFT           24
#define CG6_FHC_FBID_MASK            255
#define CG6_FHC_REV_SHIFT            20
#define CG6_FHC_REV_MASK             15
#define CG6_FHC_FROP_DISABLE         (1 << 19)
#define CG6_FHC_ROW_DISABLE          (1 << 18)
#define CG6_FHC_SRC_DISABLE          (1 << 17)
#define CG6_FHC_DST_DISABLE          (1 << 16)
#define CG6_FHC_RESET                (1 << 15)
#define CG6_FHC_LITTLE_ENDIAN        (1 << 13)
#define CG6_FHC_RES_MASK             (3 << 11)
#define CG6_FHC_1024                 (0 << 11)
#define CG6_FHC_1152                 (1 << 11)
#define CG6_FHC_1280                 (2 << 11)
#define CG6_FHC_1600                 (3 << 11)
#define CG6_FHC_CPU_MASK             (3 << 9)
#define CG6_FHC_CPU_SPARC            (0 << 9)
#define CG6_FHC_CPU_68020            (1 << 9)
#define CG6_FHC_CPU_386              (2 << 9)
#define CG6_FHC_TEST		     (1 << 8)
#define CG6_FHC_TEST_X_SHIFT	     4
#define CG6_FHC_TEST_X_MASK	     15
#define CG6_FHC_TEST_Y_SHIFT	     0
#define CG6_FHC_TEST_Y_MASK	     15

/* FBC mode definitions */
#define CG6_FBC_BLIT_IGNORE		0x00000000
#define CG6_FBC_BLIT_NOSRC		0x00100000
#define CG6_FBC_BLIT_SRC		0x00200000
#define CG6_FBC_BLIT_ILLEGAL		0x00300000
#define CG6_FBC_BLIT_MASK		0x00300000

#define CG6_FBC_VBLANK			0x00080000

#define CG6_FBC_MODE_IGNORE		0x00000000
#define CG6_FBC_MODE_COLOR8		0x00020000
#define CG6_FBC_MODE_COLOR1		0x00040000
#define CG6_FBC_MODE_HRMONO		0x00060000
#define CG6_FBC_MODE_MASK		0x00060000

#define CG6_FBC_DRAW_IGNORE		0x00000000
#define CG6_FBC_DRAW_RENDER		0x00008000
#define CG6_FBC_DRAW_PICK		0x00010000
#define CG6_FBC_DRAW_ILLEGAL		0x00018000
#define CG6_FBC_DRAW_MASK		0x00018000

#define CG6_FBC_BWRITE0_IGNORE		0x00000000
#define CG6_FBC_BWRITE0_ENABLE		0x00002000
#define CG6_FBC_BWRITE0_DISABLE		0x00004000
#define CG6_FBC_BWRITE0_ILLEGAL		0x00006000
#define CG6_FBC_BWRITE0_MASK		0x00006000

#define CG6_FBC_BWRITE1_IGNORE		0x00000000
#define CG6_FBC_BWRITE1_ENABLE		0x00000800
#define CG6_FBC_BWRITE1_DISABLE		0x00001000
#define CG6_FBC_BWRITE1_ILLEGAL		0x00001800
#define CG6_FBC_BWRITE1_MASK		0x00001800

#define CG6_FBC_BREAD_IGNORE		0x00000000
#define CG6_FBC_BREAD_0			0x00000200
#define CG6_FBC_BREAD_1			0x00000400
#define CG6_FBC_BREAD_ILLEGAL		0x00000600
#define CG6_FBC_BREAD_MASK		0x00000600

#define CG6_FBC_BDISP_IGNORE		0x00000000
#define CG6_FBC_BDISP_0			0x00000080
#define CG6_FBC_BDISP_1			0x00000100
#define CG6_FBC_BDISP_ILLEGAL		0x00000180
#define CG6_FBC_BDISP_MASK		0x00000180

#define CG6_FBC_INDEX_MOD		0x00000040
#define CG6_FBC_INDEX_MASK		0x00000030

/* THC definitions */
#define CG6_THC_MISC_REV_SHIFT       16
#define CG6_THC_MISC_REV_MASK        15
#define CG6_THC_MISC_RESET           (1 << 12)
#define CG6_THC_MISC_VIDEO           (1 << 10)
#define CG6_THC_MISC_SYNC            (1 << 9)
#define CG6_THC_MISC_VSYNC           (1 << 8)
#define CG6_THC_MISC_SYNC_ENAB       (1 << 7)
#define CG6_THC_MISC_CURS_RES        (1 << 6)
#define CG6_THC_MISC_INT_ENAB        (1 << 5)
#define CG6_THC_MISC_INT             (1 << 4)
#define CG6_THC_MISC_INIT            0x9f

typedef struct cg6_tec {
	volatile unsigned int	tec_matrix;
	volatile unsigned int	tec_clip;
	volatile unsigned int	tec_vdc;
} Cg6Tec, *Cg6TecPtr;

typedef struct cg6_thc {
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
} Cg6Thc, *Cg6ThcPtr;

typedef struct cg6_fbc {
	unsigned int		xxx0[1];
	volatile unsigned int	mode;
	volatile unsigned int	clip;
	unsigned int		xxx1[1];	    
	volatile unsigned int	s;
	volatile unsigned int	draw;
	volatile unsigned int	blit;
	volatile unsigned int	font;
	unsigned int		xxx2[24];
	volatile unsigned int	x0, y0, z0, color0;
	volatile unsigned int	x1, y1, z1, color1;
	volatile unsigned int	x2, y2, z2, color2;
	volatile unsigned int	x3, y3, z3, color3;
	volatile unsigned int	offx, offy;
	unsigned int		xxx3[2];
	volatile unsigned int	incx, incy;
	unsigned int		xxx4[2];
	volatile unsigned int	clipminx, clipminy;
	unsigned int		xxx5[2];
	volatile unsigned int	clipmaxx, clipmaxy;
	unsigned int		xxx6[2];
	volatile unsigned int	fg;
	volatile unsigned int	bg;
	volatile unsigned int	alu;
	volatile unsigned int	pm;
	volatile unsigned int	pixelm;
	unsigned int		xxx7[2];
	volatile unsigned int	patalign;
	volatile unsigned int	pattern[8];
	unsigned int		xxx8[432];
	volatile unsigned int	apointx, apointy, apointz;
	unsigned int		xxx9[1];
	volatile unsigned int	rpointx, rpointy, rpointz;
	unsigned int		xxx10[5];
	volatile unsigned int	pointr, pointg, pointb, pointa;
	volatile unsigned int	alinex, aliney, alinez;
	unsigned int		xxx11[1];
	volatile unsigned int	rlinex, rliney, rlinez;
	unsigned int		xxx12[5];
	volatile unsigned int	liner, lineg, lineb, linea;
	volatile unsigned int	atrix, atriy, atriz;
	unsigned int		xxx13[1];
	volatile unsigned int	rtrix, rtriy, rtriz;
	unsigned int		xxx14[5];
	volatile unsigned int	trir, trig, trib, tria;
	volatile unsigned int	aquadx, aquady, aquadz;
	unsigned int		xxx15[1];
	volatile unsigned int	rquadx, rquady, rquadz;
	unsigned int		xxx16[5];
	volatile unsigned int	quadr, quadg, quadb, quada;
	volatile unsigned int	arectx, arecty, arectz;
	unsigned int		xxx17[1];
	volatile unsigned int	rrectx, rrecty, rrectz;
	unsigned int		xxx18[5];
	volatile unsigned int	rectr, rectg, rectb, recta;
} Cg6Fbc, *Cg6FbcPtr;

#endif /* CG6_REGS_H */

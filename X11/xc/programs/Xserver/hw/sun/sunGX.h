/*
 * $Xorg: sunGX.h,v 1.4 2001/02/09 02:04:44 xorgcvs Exp $
 *
Copyright 1991, 1998  The Open Group

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall not be
used in advertising or otherwise to promote the sale, use or other dealings
in this Software without prior written authorization from The Open Group.
 *
 * Author:  Keith Packard, MIT X Consortium
 */
/* $XFree86: xc/programs/Xserver/hw/sun/sunGX.h,v 1.4 2001/12/14 19:59:43 dawes Exp $ */

typedef unsigned int	Uint;
typedef volatile Uint VUint;

/* modes */
#define GX_INDEX(n)	    ((n) << 4)
#define GX_INDEX_ALL	    0x00000030
#define GX_INDEX_MOD	    0x00000040
#define GX_BDISP_0	    0x00000080
#define GX_BDISP_1	    0x00000100
#define GX_BDISP_ALL	    0x00000180
#define GX_BREAD_0	    0x00000200
#define GX_BREAD_1	    0x00000400
#define GX_BREAD_ALL	    0x00000600
#define GX_BWRITE1_ENABLE   0x00000800
#define GX_BWRITE1_DISABLE  0x00001000
#define GX_BWRITE1_ALL	    0x00001800
#define GX_BWRITE0_ENABLE   0x00002000
#define GX_BWRITE0_DISABLE  0x00004000
#define GX_BWRITE0_ALL	    0x00006000
#define GX_DRAW_RENDER	    0x00008000
#define GX_DRAW_PICK	    0x00010000
#define GX_DRAW_ALL	    0x00018000
#define GX_MODE_COLOR8	    0x00020000
#define GX_MODE_COLOR1	    0x00040000
#define GX_MODE_HRMONO	    0x00060000
#define GX_MODE_ALL	    0x00060000
#define GX_VBLANK	    0x00080000
#define GX_BLIT_NOSRC	    0x00100000
#define GX_BLIT_SRC	    0x00200000
#define GX_BLIT_ALL	    0x00300000

/* rasterops */
#define GX_ROP_CLEAR	    0x0
#define GX_ROP_INVERT	    0x1
#define GX_ROP_NOOP	    0x2
#define GX_ROP_SET	    0x3

#define GX_ROP_00_0(rop)    ((rop) << 0)
#define GX_ROP_00_1(rop)    ((rop) << 2)
#define GX_ROP_01_0(rop)    ((rop) << 4)
#define GX_ROP_01_1(rop)    ((rop) << 6)
#define GX_ROP_10_0(rop)    ((rop) << 8)
#define GX_ROP_10_1(rop)    ((rop) << 10)
#define GX_ROP_11_0(rop)    ((rop) << 12)
#define GX_ROP_11_1(rop)    ((rop) << 14)
#define GX_PLOT_PLOT	    0x00000000
#define GX_PLOT_UNPLOT	    0x00020000
#define GX_RAST_BOOL	    0x00000000
#define GX_RAST_LINEAR	    0x00040000
#define GX_ATTR_UNSUPP	    0x00400000
#define GX_ATTR_SUPP	    0x00800000
#define GX_POLYG_OVERLAP    0x01000000
#define GX_POLYG_NONOVERLAP 0x02000000
#define GX_PATTERN_ZEROS    0x04000000
#define GX_PATTERN_ONES	    0x08000000
#define GX_PATTERN_MASK	    0x0c000000
#define GX_PIXEL_ZEROS	    0x10000000
#define GX_PIXEL_ONES	    0x20000000
#define GX_PIXEL_MASK	    0x30000000
#define GX_PLANE_ZEROS	    0x40000000
#define GX_PLANE_ONES	    0x80000000
#define GX_PLANE_MASK	    0xc0000000

typedef struct _sunGX {
	Uint	junk0[1];
	VUint	mode;
	VUint	clip;
	Uint	junk1[1];	    
	VUint	s;
	VUint	draw;
	VUint	blit;
	VUint	font;
	Uint	junk2[24];
	VUint	x0, y0, z0, color0;
	VUint	x1, y1, z1, color1;
	VUint	x2, y2, z2, color2;
	VUint	x3, y3, z3, color3;
	VUint	offx, offy;
	Uint	junk3[2];
	VUint	incx, incy;
	Uint	junk4[2];
	VUint	clipminx, clipminy;
	Uint	junk5[2];
	VUint	clipmaxx, clipmaxy;
	Uint	junk6[2];
	VUint	fg;
	VUint	bg;
	VUint	alu;
	VUint	pm;
	VUint	pixelm;
	Uint	junk7[2];
	VUint	patalign;
	VUint	pattern[8];
	Uint	junk8[432];
	VUint	apointx, apointy, apointz;
	Uint	junk9[1];
	VUint	rpointx, rpointy, rpointz;
	Uint	junk10[5];
	VUint	pointr, pointg, pointb, pointa;
	VUint	alinex, aliney, alinez;
	Uint	junk11[1];
	VUint	rlinex, rliney, rlinez;
	Uint	junk12[5];
	VUint	liner, lineg, lineb, linea;
	VUint	atrix, atriy, atriz;
	Uint	junk13[1];
	VUint	rtrix, rtriy, rtriz;
	Uint	junk14[5];
	VUint	trir, trig, trib, tria;
	VUint	aquadx, aquady, aquadz;
	Uint	junk15[1];
	VUint	rquadx, rquady, rquadz;
	Uint	junk16[5];
	VUint	quadr, quadg, quadb, quada;
	VUint	arectx, arecty, arectz;
	Uint	junk17[1];
	VUint	rrectx, rrecty, rrectz;
	Uint	junk18[5];
	VUint	rectr, rectg, rectb, recta;
} sunGX, *sunGXPtr;

/* Macros */

#define GX_ROP_USE_PIXELMASK	0x30000000

#define GX_BLT_INPROGRESS	0x20000000

#define GX_INPROGRESS		0x10000000
#define GX_FULL			0x20000000

#define GXWait(gx,r)\
    do\
	(r) = (int) (gx)->s; \
    while ((r) & GX_INPROGRESS)

#define GXDrawDone(gx,r) \
    do \
	(r) = (int) (gx)->draw; \
    while ((r) < 0 && ((r) & GX_FULL))

#define GXBlitDone(gx,r)\
    do\
	(r)= (int) (gx)->blit; \
    while ((r) < 0 && ((r) & GX_BLT_INPROGRESS))

#define GXBlitInit(gx,rop,pmsk) {\
    gx->fg = 0xff;\
    gx->bg = 0x00;\
    gx->pixelm = ~0;\
    gx->s = 0;\
    gx->alu = rop;\
    gx->pm = pmsk;\
    gx->clip = 0;\
}

#define GXDrawInit(gx,fore,rop,pmsk) {\
    gx->fg = fore;\
    gx->bg = 0x00; \
    gx->pixelm = ~0; \
    gx->s = 0; \
    gx->alu = rop; \
    gx->pm = pmsk; \
    gx->clip = 0;\
}

#define GXStippleInit(gx,stipple) {\
    int		_i; \
    Uint	*sp; \
    VUint	*dp; \
    _i = 8;  \
    sp = stipple->bits; \
    dp = gx->pattern; \
    while (_i--) {  \
	dp[_i] =  sp[_i]; \
    } \
    gx->fg = stipple->fore; \
    gx->bg = stipple->back; \
    gx->patalign = stipple->patalign; \
    gx->alu = stipple->alu; \
}

extern int  sunGXScreenPrivateIndex;
extern int  sunGXGCPrivateIndex;
extern int  sunGXWindowPrivateIndex;

#define sunGXGetScreenPrivate(s)    ((sunGXPtr) \
			    (s)->devPrivates[sunGXScreenPrivateIndex].ptr)

typedef struct _sunGXStipple {
    Uint	fore, back;
    Uint	patalign;
    Uint	alu;
    Uint	bits[8];	/* actually 16 shorts */
} sunGXStippleRec, *sunGXStipplePtr;

typedef struct _sunGXPrivGC {
    int		    type;
    sunGXStipplePtr stipple;
} sunGXPrivGCRec, *sunGXPrivGCPtr;

#define sunGXGetGCPrivate(g)	    ((sunGXPrivGCPtr) \
			    (g)->devPrivates[sunGXGCPrivateIndex].ptr)

#define sunGXGetWindowPrivate(w)    ((sunGXStipplePtr) \
			    (w)->devPrivates[sunGXWindowPrivateIndex].ptr)

#define sunGXSetWindowPrivate(w,p) (\
	    (w)->devPrivates[sunGXWindowPrivateIndex].ptr = (pointer) p)


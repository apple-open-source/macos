/*
 * Acceleration for the Leo (ZX) framebuffer - defines.
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
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/sunleo/leo.h,v 1.4 2001/05/04 19:05:46 dawes Exp $ */

#ifndef LEO_H
#define LEO_H

#include "xf86.h"
#include "xf86_OSproc.h"
#include "xf86_ansic.h"
#include "xf86RamDac.h"
#include "Xmd.h"
#include "gcstruct.h"
#include "leo_regs.h"
#include "xf86sbusBus.h"

/* Various offsets in virtual (ie. mmap()) spaces Linux and Solaris support. */
#define LEO_FB0_VOFF		0x00000000
#define LEO_LC0_VOFF		0x00800000
#define LEO_LD0_VOFF		0x00801000
#define LEO_LX0_CURSOR_VOFF	0x00802000
#define LEO_FB1_VOFF		0x00803000
#define LEO_LC1_VOFF		0x01003000
#define LEO_LD1			0x01004000
#define LEO_LX0_VERT_VOFF	0x01005000
#define LEO_LX_KRN_VOFF		0x01006000
#define LEO_LC0_KRN_VOFF	0x01007000
#define LEO_LC1_KRN_VOFF	0x01008000
#define LEO_LD_GBL_VOFF		0x01009000

typedef struct {
	unsigned int fg, bg;			/* FG/BG colors for stipple */
	unsigned int patalign;                  /* X/Y alignment of bits */
        unsigned int alu;			/* Transparent/Opaque + rop */
        unsigned int bits[32];                  /* The stipple bits themselves */
} LeoStippleRec, *LeoStipplePtr;

typedef struct {
	int type;
	LeoStipplePtr stipple;
} LeoPrivGCRec, *LeoPrivGCPtr;

typedef struct {
	LeoCommand0	*lc0;
	LeoDraw		*ld0;
	LeoCursor	*dac;
	unsigned	*fb;
	int		vclipmax;
	int		width;
	int		height;
	/* cache one stipple; figuring out if we can use the stipple is as hard as
	 * computing it, so we just use this one and leave it here if it
	 * can't be used this time
	 */
	LeoStipplePtr	tmpStipple;

	sbusDevicePtr	psdp;
	Bool		HWCursor;
	Bool		NoAccel;
	Bool		vtSema;
	CloseScreenProcPtr CloseScreen;
	xf86CursorInfoPtr CursorInfoRec;
	unsigned char	CursorShiftX, CursorShiftY;
	unsigned char	*CursorData;
	OptionInfoPtr	Options;
} LeoRec, *LeoPtr;

extern int  LeoScreenPrivateIndex;
extern int  LeoGCPrivateIndex;
extern int  LeoWindowPrivateIndex;

#define GET_LEO_FROM_SCRN(p)    ((LeoPtr)((p)->driverPrivate))

#define LeoGetScreenPrivate(s)						\
((LeoPtr) (s)->devPrivates[LeoScreenPrivateIndex].ptr)

#define LeoGetGCPrivate(g)						\
((LeoPrivGCPtr) (g)->devPrivates [LeoGCPrivateIndex].ptr)

#define LeoGetWindowPrivate(w)						\
((LeoStipplePtr) (w)->devPrivates[LeoWindowPrivateIndex].ptr)
                            
#define LeoSetWindowPrivate(w,p) 					\
((w)->devPrivates[LeoWindowPrivateIndex].ptr = (pointer) p)

extern int leoRopTable[];

#endif /* LEO_H */

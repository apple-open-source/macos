/*
 * GX and Turbo GX framebuffer - defines.
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
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/suncg6/cg6.h,v 1.3 2001/05/04 19:05:45 dawes Exp $ */

#ifndef CG6_H
#define CG6_H

#include "xf86.h"
#include "xf86_OSproc.h"
#include "xf86_ansic.h"
#include "xf86RamDac.h"
#include "Xmd.h"
#include "gcstruct.h"
#include "cg6_regs.h"
#include "xf86sbusBus.h"

/* Various offsets in virtual (ie. mmap()) spaces Linux and Solaris support. */
#define CG6_FBC_VOFF	0x70000000
#define CG6_TEC_VOFF	0x70001000
#define CG6_BTREGS_VOFF	0x70002000
#define CG6_FHC_VOFF	0x70004000
#define CG6_THC_VOFF	0x70005000
#define CG6_ROM_VOFF	0x70006000
#define CG6_RAM_VOFF	0x70016000
#define CG6_DHC_VOFF	0x80000000

typedef struct {
	unsigned int fg, bg;			/* FG/BG colors for stipple */
	unsigned int patalign;                  /* X/Y alignment of bits */
        unsigned int alu;			/* Transparent/Opaque + rop */
        unsigned int bits[32];                  /* The stipple bits themselves */
} Cg6StippleRec, *Cg6StipplePtr;

typedef struct {
	int type;
	Cg6StipplePtr stipple;
} Cg6PrivGCRec, *Cg6PrivGCPtr;

typedef struct {
	unsigned char	*fb;
	Cg6FbcPtr	fbc;
	Cg6ThcPtr	thc;
	int		vclipmax;
	int		width;
	int		height;

	sbusDevicePtr	psdp;
	Bool		HWCursor;
	Bool		NoAccel;
	CloseScreenProcPtr CloseScreen;
	xf86CursorInfoPtr CursorInfoRec;
	unsigned int	CursorXY;
	int		CursorBg, CursorFg;
	Bool		CursorEnabled;
	OptionInfoPtr	Options;
} Cg6Rec, *Cg6Ptr;

extern int  Cg6ScreenPrivateIndex;
extern int  Cg6GCPrivateIndex;
extern int  Cg6WindowPrivateIndex;

#define GET_CG6_FROM_SCRN(p)    ((Cg6Ptr)((p)->driverPrivate))

#define Cg6GetScreenPrivate(s)						\
((Cg6Ptr) (s)->devPrivates[Cg6ScreenPrivateIndex].ptr)

#define Cg6GetGCPrivate(g)						\
((Cg6PrivGCPtr) (g)->devPrivates [Cg6GCPrivateIndex].ptr)

#define Cg6GetWindowPrivate(w)						\
((Cg6StipplePtr) (w)->devPrivates[Cg6WindowPrivateIndex].ptr)
                            
#define Cg6SetWindowPrivate(w,p) 					\
((w)->devPrivates[Cg6WindowPrivateIndex].ptr = (pointer) p)

extern int cg6RopTable[];

#endif /* CG6_H */

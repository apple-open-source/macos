/*
 * CG14 framebuffer - defines.
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
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/suncg14/cg14.h,v 1.3 2001/05/04 19:05:45 dawes Exp $ */

#ifndef CG14_H
#define CG14_H

#include "xf86.h"
#include "xf86_OSproc.h"
#include "xf86_ansic.h"
#include "xf86RamDac.h"
#include "Xmd.h"
#include "gcstruct.h"
#include "xf86sbusBus.h"

/* Various offsets in virtual (ie. mmap()) spaces Linux and Solaris support. */
#define CG14_REGS_VOFF		0x00000000	/* registers */
#define CG14_XLUT_VOFF		0x00003000	/* X Look Up Table */
#define CG14_CLUT1_VOFF		0x00004000	/* Color Look Up Table */
#define CG14_CLUT2_VOFF		0x00005000	/* Color Look Up Table */
#define CG14_CLUT3_VOFF		0x00006000	/* Color Look Up Table */
#define CG14_DIRECT_VOFF	0x10000000
#define CG14_CTLREG_VOFF	0x20000000
#define CG14_CURSOR_VOFF	0x30000000
#define CG14_SHDW_VRT_VOFF	0x40000000
#define CG14_XBGR_VOFF		0x50000000
#define CG14_BGR_VOFF		0x60000000
#define CG14_X16_VOFF		0x70000000
#define CG14_C16_VOFF		0x80000000
#define CG14_X32_VOFF		0x90000000
#define CG14_B32_VOFF		0xa0000000
#define CG14_G32_VOFF		0xb0000000
#define CG14_R32_VOFF		0xc0000000

typedef struct {
	unsigned int	*fb;
	unsigned char	*x32;
	unsigned char	*xlut;
	int		width;
	int		height;
	sbusDevicePtr	psdp;
	CloseScreenProcPtr CloseScreen;
	OptionInfoPtr	Options;
} Cg14Rec, *Cg14Ptr;

#define GET_CG14_FROM_SCRN(p)    ((Cg14Ptr)((p)->driverPrivate))

/*
 * This should match corresponding definition in Solaris's
 * '/usr/include/sys/cg14io.h'.
 */
#define CG14_SET_PIXELMODE	(('M' << 8) | 3)

#endif /* CG14_H */

/*
 * TCX framebuffer - defines.
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
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/suntcx/tcx.h,v 1.2 2001/05/04 19:05:46 dawes Exp $ */

#ifndef TCX_H
#define TCX_H

#include "xf86.h"
#include "xf86_OSproc.h"
#include "xf86_ansic.h"
#include "xf86RamDac.h"
#include "Xmd.h"
#include "gcstruct.h"
#include "xf86sbusBus.h"
#include "tcx_regs.h"

/* Various offsets in virtual (ie. mmap()) spaces Linux and Solaris support. */
#define TCX_RAM8_VOFF		0x00000000
#define TCX_RAM24_VOFF		0x01000000
#define TCX_CPLANE_VOFF		0x28000000
#define TCX_TEC_VOFF		0x70000000
#define TCX_BTREGS_VOFF		0x70002000
#define TCX_THC_VOFF		0x70004000
#define TCX_DHC_VOFF		0x70008000
#define TCX_ALT_VOFF		0x7000a000
#define TCX_SYNC_VOFF		0x7000e000

typedef struct {
	unsigned char	*fb;
	int		width;
	int		height;
	unsigned int	*cplane;
	TcxThcPtr	thc;
	sbusDevicePtr	psdp;
	CloseScreenProcPtr CloseScreen;
	Bool		HWCursor;
	xf86CursorInfoPtr CursorInfoRec;
	unsigned int	CursorXY;
	int		CursorBg, CursorFg;
	Bool		CursorEnabled;
	unsigned char	CursorShiftX, CursorShiftY;
	unsigned char	*CursorData;
	OptionInfoPtr	Options;
} TcxRec, *TcxPtr;

#define TCX_CPLANE_MODE		0x03000000

#define GET_TCX_FROM_SCRN(p)    ((TcxPtr)((p)->driverPrivate))

#endif /* TCX_H */

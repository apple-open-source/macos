/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/mga/mga_dripriv.h,v 1.4 2001/04/10 16:08:01 dawes Exp $ */

/*
 * Copyright 2000 VA Linux Systems Inc., Fremont, California.
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * VA LINUX SYSTEMS AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES
 * OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *   Gareth Hughes <gareth@valinux.com>
 */

#ifndef __MGA_DRIPRIV_H__
#define __MGA_DRIPRIV_H__

#include "GL/glxint.h"

#define MGA_MAX_DRAWABLES 256

extern void GlxSetVisualConfigs( int nconfigs,
				 __GLXvisualConfig *configs,
				 void **configprivs );

typedef struct {
  /* Nothing here yet */
  int dummy;
} MGAConfigPrivRec, *MGAConfigPrivPtr;

typedef struct {
  /* Nothing here yet */
  int dummy;
} MGADRIContextRec, *MGADRIContextPtr;

#endif

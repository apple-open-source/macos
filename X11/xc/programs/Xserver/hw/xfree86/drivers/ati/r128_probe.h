/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/ati/r128_probe.h,v 1.7 2003/10/30 17:36:58 tsi Exp $ */
/*
 * Copyright 2000 ATI Technologies Inc., Markham, Ontario, and
 *                VA Linux Systems Inc., Fremont, California.
 *
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation on the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial
 * portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NON-INFRINGEMENT.  IN NO EVENT SHALL ATI, VA LINUX SYSTEMS AND/OR
 * THEIR SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

/*
 * Authors:
 *   Kevin E. Martin <martin@valinux.com>
 *
 * Modified by Marc Aurele La France <tsi@xfree86.org> for ATI driver merge.
 */

#ifndef _R128_PROBE_H_
#define _R128_PROBE_H_ 1

#include "atiproto.h"

#include "xf86str.h"

/* r128_probe.c */
extern const OptionInfoRec * R128AvailableOptions
			     FunctionPrototype((int, int));
extern void                  R128Identify
			     FunctionPrototype((int));
extern Bool                  R128Probe
			     FunctionPrototype((DriverPtr, int));

extern SymTabRec             R128Chipsets[];
extern PciChipsets           R128PciChipsets[];

/* r128_driver.c */
extern void                  R128LoaderRefSymLists
			     FunctionPrototype((void));
extern Bool                  R128PreInit
			     FunctionPrototype((ScrnInfoPtr, int));
extern Bool                  R128ScreenInit
			     FunctionPrototype((int, ScreenPtr, int, char **));
extern Bool                  R128SwitchMode
			     FunctionPrototype((int, DisplayModePtr, int));
extern void                  R128AdjustFrame
			     FunctionPrototype((int, int, int, int));
extern Bool                  R128EnterVT
			     FunctionPrototype((int, int));
extern void                  R128LeaveVT
			     FunctionPrototype((int, int));
extern void                  R128FreeScreen
			     FunctionPrototype((int, int));
extern ModeStatus            R128ValidMode
			     FunctionPrototype((int, DisplayModePtr, Bool,
						int));

extern const OptionInfoRec   R128Options[];

#endif /* _R128_PROBE_H_ */

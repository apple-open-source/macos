/*
 * Acceleration for the Creator and Creator3D framebuffer - clipping defines.
 *
 * Copyright (C) 1999 Jakub Jelinek (jakub@redhat.com)
 * Copyright (C) 1999 David S. Miller (davem@redhat.com)
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
 * JAKUB JELINEK OR DAVID MILLER BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/sunffb/ffb_clip.h,v 1.1 2000/05/18 23:21:36 dawes Exp $ */

#ifndef FFBCLIP_H
#define FFBCLIP_H

extern unsigned int FFBSetClip(FFBPtr pFfb,
			       ffb_fbcPtr ffb,
			       RegionPtr pClip,
			       int numRects);
static __inline__ void
FFBSet1Clip(FFBPtr pFfb, ffb_fbcPtr ffb, BoxPtr extents)
{
	unsigned int xy1, xy2;
	
	xy1 = (extents->y1 << 16) | extents->x1;
	xy2 = ((extents->y2 - 1) << 16) | (extents->x2 - 1);
	if (pFfb->clips[0] == xy1 && pFfb->clips[1] == xy2)
		return;
	FFBFifo(pFfb, 2);
	FFB_WRITE64(&ffb->vclipmin, xy1, xy2);
	pFfb->clips[0] = xy1;
	pFfb->clips[1] = xy2;
}

#endif /* FFBCLIP_H */

/*
 * Acceleration for the Creator and Creator3D framebuffer - fast inner loops.
 *
 * Copyright (C) 1999 David S. Miller (davem@redhat.com)
 * Copyright (C) 1999 Jakub Jelinek (jakub@redhat.com)
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
 *
 */
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/sunffb/ffb_loops.h,v 1.1 2000/05/18 23:21:37 dawes Exp $ */

#ifndef FFBLOOPS_H
#define FFBLOOPS_H

#ifdef USE_VIS
extern void FFB_STIPPLE_LOAD(volatile unsigned int *d,
			     unsigned int *s);

extern void FFB_PPT_BOX_LOOP(FFBPtr ffbpriv,
			     ffb_fbcPtr ffb,
			     BoxPtr pbox, BoxPtr pbox_last,
			     DDXPointPtr ppt);

extern void FFB_BOX_LOOP(FFBPtr ffbpriv,
			 ffb_fbcPtr ffb,
			 BoxPtr pbox,
			 BoxPtr pbox_last);

extern void FFB_RECT_LOOP(FFBPtr ffbpriv,
			  ffb_fbcPtr ffb,
			  xRectangle *prect,
			  xRectangle *prect_last,
			  int xOrg, int yOrg);

extern void FFB_PPT_WIDTH_LOOP(FFBPtr ffbpriv,
			       ffb_fbcPtr ffb,
			       DDXPointPtr ppt, DDXPointPtr ppt_last,
			       int *pwidth);

extern DDXPointPtr FFB_PPT_LOOP1(FFBPtr ffbpriv,
				 ffb_fbcPtr ffb,
				 DDXPointPtr ppt, DDXPointPtr ppt_last,
				 int xOrg, int yOrg);

extern DDXPointPtr FFB_PPT_LOOP2(FFBPtr ffbpriv,
				 ffb_fbcPtr ffb,
				 DDXPointPtr ppt, DDXPointPtr ppt_last,
				 int x, int y);

extern DDXPointPtr FFB_LINE_LOOP1(FFBPtr ffbpriv,
				  ffb_fbcPtr ffb,
				  DDXPointPtr ppt, DDXPointPtr ppt_last,
				  int xOrg, int yOrg);

extern DDXPointPtr FFB_LINE_LOOP2(FFBPtr ffbpriv,
				  ffb_fbcPtr ffb,
				  DDXPointPtr ppt, DDXPointPtr ppt_last,
				  int *x, int *y);

#else /* !USE_VIS */

#define FFB_STIPPLE_LOAD(_d,_s)				\
do {							\
	volatile unsigned int *d = (_d);		\
	unsigned int *s = (_s);				\
	int i;						\
							\
	for (i = 0; i < (32 / 2); i++, d+=2, s+=2)	\
		FFB_WRITE64(d, s[0], s[1]);		\
} while (0)

#define FFB_PPT_BOX_LOOP(pFfb, ffb, pbox, _pbox_last, ppt)				\
do {											\
	BoxPtr pbox_last = (BoxPtr)(_pbox_last);					\
	while (pbox <= pbox_last) {							\
		FFBFifo(pFfb, 7);							\
		ffb->drawop = FFB_DRAWOP_VSCROLL;					\
		FFB_WRITE64(&ffb->by, ppt->y, ppt->x);					\
		FFB_WRITE64_2(&ffb->dy, pbox->y1, pbox->x1);				\
		FFB_WRITE64_3(&ffb->bh, (pbox->y2 - pbox->y1), (pbox->x2 - pbox->x1));	\
		pbox++; ppt++;								\
	}										\
} while (0)

#define FFB_BOX_LOOP(pFfb, ffb, pbox, _pbox_last)					\
do {											\
	BoxPtr pbox_last = (BoxPtr)(_pbox_last);					\
	while (pbox <= pbox_last) {							\
		FFBFifo(pFfb, 4);							\
		FFB_WRITE64(&ffb->by, pbox->y1, pbox->x1);				\
		FFB_WRITE64_2(&ffb->bh, (pbox->y2 - pbox->y1), (pbox->x2 - pbox->x1));	\
		pbox++;									\
	}										\
} while (0)

#define FFB_RECT_LOOP(pFfb, ffb, prect, _prect_last, xOrg, yOrg)			\
do {											\
	xRectangle *prect_last = (xRectangle *)(_prect_last);				\
	for (; prect <= prect_last; prect++) {						\
		register int x, y, w, h;						\
		x = prect->x + xOrg;							\
		y = prect->y + yOrg;							\
		w = prect->width;							\
		h = prect->height;							\
		if (extents->x2 <= x ||							\
		    extents->x1 >= x + w ||						\
		    extents->y2 <= y ||							\
		    extents->y1 >= y + h)						\
			continue;							\
		FFBFifo(pFfb, 4);							\
		FFB_WRITE64(&ffb->by, y, x);						\
		FFB_WRITE64_2(&ffb->bh, h, w);						\
	}										\
} while (0)

#define FFB_PPT_WIDTH_LOOP(pFfb, ffb, ppt, _ppt_last, pwidth)			\
do {											\
	DDXPointPtr ppt_last = (DDXPointPtr)(_ppt_last);				\
	while (ppt <= ppt_last) {							\
		register int x, y, w;							\
		x = ppt->x;								\
		y = ppt->y;								\
		w = *pwidth++;								\
		FFBFifo(pFfb, 5);							\
		ffb->ppc = 0;								\
		FFB_WRITE64(&ffb->by, y, x);						\
		FFB_WRITE64_2(&ffb->bh, y, (x + w));					\
		ppt++;									\
	}										\
} while (0)

static __inline__ DDXPointPtr FFB_PPT_LOOP1(FFBPtr pFfb,
				 ffb_fbcPtr ffb,
				 DDXPointPtr ppt, DDXPointPtr ppt_last,
				 int xOrg, int yOrg)
{
	while (ppt <= ppt_last) {
		FFBFifo(pFfb, 2);
		FFB_WRITE64_2(&ffb->bh, (ppt->y + yOrg), (ppt->x + xOrg));
		ppt++;
	}
	return ppt;
}

static __inline__ DDXPointPtr FFB_PPT_LOOP2(FFBPtr pFfb,
					    ffb_fbcPtr ffb,
					    DDXPointPtr ppt, DDXPointPtr ppt_last,
					    int x, int y)
{
	register int __x = x, __y = y;
	while (ppt <= ppt_last) {
		FFBFifo(pFfb, 2);
		__x += ppt->x;
		__y += ppt->y;
		FFB_WRITE64_2(&ffb->bh, __y, __x);
		ppt++;
	}
	return ppt;
}

static __inline__ DDXPointPtr FFB_LINE_LOOP1(FFBPtr pFfb,
					     ffb_fbcPtr ffb,
					     DDXPointPtr ppt, DDXPointPtr ppt_last,
					     int xOrg, int yOrg)
{
	while (ppt <= ppt_last) {
		FFBFifo(pFfb, 3);
		ffb->ppc = 0;
		FFB_WRITE64_2(&ffb->bh, (ppt->y + yOrg), (ppt->x + xOrg));
		ppt++;
	}
	return ppt;
}

static __inline__ DDXPointPtr FFB_LINE_LOOP2(FFBPtr pFfb,
					     ffb_fbcPtr ffb,
					     DDXPointPtr ppt, DDXPointPtr ppt_last,
					     int *x, int *y)
{
	register int __x = *x, __y = *y;
	while (ppt <= ppt_last) {
		FFBFifo(pFfb, 3);
		ffb->ppc = 0;
		__x += ppt->x;
		__y += ppt->y;
		FFB_WRITE64_2(&ffb->bh, __y, __x);
		ppt++;
	}
	*x = __x;
	*y = __y;
	return ppt;
}

#endif /* !USE_VIS */

#endif /* FFBLOOPS_H */

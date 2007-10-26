/* $Xorg: DrawLogo.c,v 1.4 2001/02/09 02:03:52 xorgcvs Exp $ */

/*

Copyright 1988, 1998  The Open Group

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

*/
/* $XFree86: xc/programs/xlogo/RenderLogo.c,v 1.3tsi Exp $ */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <stdio.h>

#include <X11/Xlib.h>  
#include <X11/extensions/Xrender.h>

#include "RenderLogo.h"

typedef struct _XLineDouble {
    XPointDouble  p1, p2;
} XLineDouble;

static void
intersect(XLineDouble *l1, XLineDouble *l2, XPointDouble *intersection);

/*
 *  Draw the "official" X Window System Logo, designed by Danny Chong
 *
 *  Written by Ollie Jones, Apollo Computer
 *
 *  Does some fancy stuff to make the logo look acceptable even
 *  if it is tiny.  Also makes the various linear elements of
 *  the logo line up as well as possible considering rasterization.
 *
 *  Munged to draw anti-aliased logo using Render extension.
 *  Carl Worth, 2002-05-16
 */
void
RenderLogo(Display *dpy, int op, Picture src, Picture dst, XRenderPictFormat *maskFormat,
	   int x, int y, unsigned int width, unsigned int height)
{
    unsigned int size;
    double thin, thick, gap, d31;
    XPointDouble poly[6];
    XLineDouble thick_left, thick_right, thin_left, thin_right, gap_left, gap_right;

    /* for now, do a centered even-sized square, at least for now */
    size = width;
    if (height < width)
	 size = height;
    size &= ~1;
    x += (width - size) >> 1;
    y += (height - size) >> 1;

    thin = (size / 11.0);
    thick = (size / 4.0);
    gap = thin / 4.0;
    d31 = thin + thin + gap;

    thick_left.p1.x = x;		thick_left.p1.y = y;
    thick_left.p2.x = x + size - thick;	thick_left.p2.y = y + size;

    thick_right.p1.x = x + thick;	thick_right.p1.y = y;
    thick_right.p2.x = x + size;	thick_right.p2.y = y + size;

    thin_left.p1.x = x + size-d31;	thin_left.p1.y = y;
    thin_left.p2.x = x + 0;		thin_left.p2.y = y + size;

    thin_right.p1.x = x + size;		thin_right.p1.y = y;
    thin_right.p2.x = x + d31;		thin_right.p2.y = y + size;

    gap_left.p1.x = x + size-( thin+gap);	gap_left.p1.y = y;
    gap_left.p2.x = x + thin;			gap_left.p2.y = y + size;

    gap_right.p1.x = x + size- thin;	gap_right.p1.y = y;
    gap_right.p2.x = x + thin + gap;	gap_right.p2.y = y + size;

    poly[0] = thick_left.p1;
    poly[1] = thick_right.p1;
    intersect(&thick_right, &gap_left, &poly[2]);
    poly[3] = gap_left.p2;
    poly[4] = thin_left.p2;
    intersect(&thick_left, &thin_left, &poly[5]);

    XRenderCompositeDoublePoly(dpy, op,
			       src, dst, maskFormat,
			       0, 0, 0, 0,
			       poly, 6, 0);

    poly[0] = thin_right.p1;
    poly[1] = gap_right.p1;
    intersect(&thick_left, &gap_right, &poly[2]);
    poly[3] = thick_left.p2;
    poly[4] = thick_right.p2;
    intersect(&thick_right, &thin_right, &poly[5]);

    XRenderCompositeDoublePoly(dpy, op,
			       src, dst, maskFormat,
			       0, 0, 0, 0,
			       poly, 6, 0);
}

static double
compute_inverse_slope (XLineDouble *l)
{
    return ((l->p2.x - l->p1.x) / 
	    (l->p2.y - l->p1.y));
}

static double
compute_x_intercept(XLineDouble *l, double inverse_slope)
{
    return (l->p1.x) - inverse_slope * l->p1.y;
}

static void
intersect(XLineDouble *l1, XLineDouble *l2, XPointDouble *intersection)
{
    double check;
    /*
     * x = m1y + b1
     * x = m2y + b2
     * m1y + b1 = m2y + b2
     * y * (m1 - m2) = b2 - b1
     * y = (b2 - b1) / (m1 - m2)
     */
    double  m1 = compute_inverse_slope (l1);
    double  b1 = compute_x_intercept (l1, m1);
    double  m2 = compute_inverse_slope (l2);
    double  b2 = compute_x_intercept (l2, m2);

    intersection->y = (b2 - b1) / (m1 - m2);
    intersection->x = m1 * intersection->y + b1;

    check = m2 * intersection->y + b2;
    if (check >= intersection->x)
	check -= intersection->x;
    else
	check = intersection->x - check;
    if (check > (1/(double)(1<<16))) {
	fprintf(stderr, "intersect: intersection is off by %f\n", check);
    }
}


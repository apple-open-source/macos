/* $Xorg: transform.c,v 1.4 2001/02/09 02:05:33 xorgcvs Exp $ */
/*

Copyright 1993, 1998  The Open Group

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall
not be used in advertising or otherwise to promote the sale, use or
other dealings in this Software without prior written authorization
from The Open Group.

*/
/* $XFree86: xc/programs/oclock/transform.c,v 3.5 2001/12/14 20:01:00 dawes Exp $ */


/*
 * transformed coordinate system objects for X
 */

#include	<X11/Xlib.h>
#include	"transform.h"
#include	<stdlib.h>

static XPoint * 
TranslatePoints(TPoint *points, int n_points, 
		Transform *t, int mode)
{
	XPoint	*xpoints;
	int	i;
	double	xoff = 0.0, yoff = 0.0;

	xpoints = (XPoint *) malloc ((unsigned)n_points * sizeof (*xpoints));
	if (!xpoints)
		return 0;
	for (i = 0; i < n_points; i++) {
		xpoints[i].x = Xx(points[i].x + xoff, points[i].y + yoff, t);
		xpoints[i].y = Xy(points[i].x + xoff, points[i].y + yoff, t);
		if (mode == CoordModePrevious) {
			xoff += points[i].x;
			yoff += points[i].y;
		}
	}
	return xpoints;
}

void
TFillPolygon (dpy, d, gc, t, points, n_points, shape, mode)
register Display	*dpy;
Drawable		d;
GC			gc;
Transform		*t;
TPoint			*points;
int			n_points;
int			shape;
int			mode;
{
	XPoint	*xpoints;

	xpoints = TranslatePoints (points, n_points, t, mode);
	if (xpoints) {
		XFillPolygon (dpy, d, gc, xpoints, n_points, shape,
				CoordModeOrigin);
		free (xpoints);
	}
}

void
TDrawArc (dpy, d, gc, t, x, y, width, height, angle1, angle2)
	register Display	*dpy;
	Drawable		d;
	GC			gc;
	Transform		*t;
	double			x, y, width, height;
	int			angle1, angle2;
{
	int	xx, xy, xw, xh;

	xx = Xx(x,y,t);
	xy = Xy(x,y,t);
	xw = Xwidth (width, height, t);
	xh = Xheight (width, height, t);
	if (xw < 0) {
		xx += xw;
		xw = -xw;
	}
	if (xh < 0) {
		xy += xh;
		xh = -xh;
	}
	XDrawArc (dpy, d, gc, xx, xy, xw, xh, angle1, angle2);
}

void
TFillArc (dpy, d, gc, t, x, y, width, height, angle1, angle2)
	register Display	*dpy;
	Drawable		d;
	GC			gc;
	Transform		*t;
	double			x, y, width, height;
	int			angle1, angle2;
{
	int	xx, xy, xw, xh;

	xx = Xx(x,y,t);
	xy = Xy(x,y,t);
	xw = Xwidth (width, height, t);
	xh = Xheight (width, height, t);
	if (xw < 0) {
		xx += xw;
		xw = -xw;
	}
	if (xh < 0) {
		xy += xh;
		xh = -xh;
	}
	XFillArc (dpy, d, gc, xx, xy, xw, xh, angle1, angle2);
}

void
SetTransform (t, xx1, xx2, xy1, xy2, tx1, tx2, ty1, ty2)
Transform	*t;
int		xx1, xx2, xy1, xy2;
double		tx1, tx2, ty1, ty2;
{
	t->mx = ((double) xx2 - xx1) / (tx2 - tx1);
	t->bx = ((double) xx1) - t->mx * tx1;
	t->my = ((double) xy2 - xy1) / (ty2 - ty1);
	t->by = ((double) xy1) - t->my * ty1;
}

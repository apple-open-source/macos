/*
 * transformed coordinate system objects for X
 */
/* $XFree86: xc/programs/xeyes/transform.c,v 1.4 2001/08/27 23:35:13 dawes Exp $ */

# include	<X11/Xos.h>
# include	<stdlib.h>
# include	<X11/Xlib.h>
# include	"transform.h"

#if 0
static XPoint *
TranslatePoints (TPoint *points, int n_points, Transform *t, int mode)
{
	XPoint	*xpoints;
	int	i;
	double	xoff = 0.0, yoff = 0.0;

	xpoints = (XPoint *) malloc (n_points * sizeof (*xpoints));
	if (!xpoints)
		return NULL;
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

static void
TFillPolygon (
    Display	*dpy,
    Drawable	d,
    GC		gc,
    Transform	*t,
    TPoint	*points,
    int		n_points,
    int		shape,
    int		mode)
{
	XPoint	*xpoints;

	xpoints = TranslatePoints (points, n_points, t, mode);
	if (xpoints) {
		XFillPolygon (dpy, d, gc, xpoints, n_points, shape,
				CoordModeOrigin);
		free (xpoints);
	}
}

static void
TDrawArc (
    Display	*dpy,
    Drawable	d,
    GC		gc,
    Transform	*t,
    double	x,
    double	y,
    double	width,
    double	height,
    int		angle1,
    int		angle2)
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
#endif

void
TFillArc (dpy, d, gc, t, x, y, width, height, angle1, angle2)
    Display	*dpy;
    Drawable	d;
    GC		gc;
    Transform	*t;
    double	x, y, width, height;
    int		angle1, angle2;
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
    double	tx1, tx2, ty1, ty2;
{
	t->mx = ((double) xx2 - xx1) / (tx2 - tx1);
	t->bx = ((double) xx1) - t->mx * tx1;
	t->my = ((double) xy2 - xy1) / (ty2 - ty1);
	t->by = ((double) xy1) - t->my * ty1;
}

/*
 * $Xorg: Clock.c,v 1.4 2001/02/09 02:05:33 xorgcvs Exp $
 *
Copyright 1989, 1998  The Open Group

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
/* $XFree86: xc/programs/oclock/Clock.c,v 1.7 2001/12/14 20:01:00 dawes Exp $ */

/*
 * Clock.c
 *
 * a NeWS clone clock
 */

#include <X11/IntrinsicP.h>
#include <X11/StringDefs.h>
#include <X11/Xmu/Converters.h>
#include "ClockP.h"
#include <X11/Xos.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <X11/extensions/shape.h>

#include <time.h>
#define Time_t time_t

#define offset(field) XtOffsetOf(ClockRec, clock.field)
#define goffset(field) XtOffsetOf(WidgetRec, core.field)

static XtResource resources[] = {
    {XtNwidth, XtCWidth, XtRDimension, sizeof(Dimension),
        goffset(width), XtRImmediate, (XtPointer) 120},
    {XtNheight, XtCHeight, XtRDimension, sizeof(Dimension),
	goffset(height), XtRImmediate, (XtPointer) 120},
    {XtNminute, XtCForeground, XtRPixel, sizeof (Pixel),
	offset(minute), XtRString, XtDefaultForeground},
    {XtNhour, XtCForeground, XtRPixel, sizeof (Pixel),
	offset(hour), XtRString, XtDefaultForeground},
    {XtNjewel, XtCForeground, XtRPixel, sizeof (Pixel),
	offset(jewel), XtRString, XtDefaultForeground},
    {XtNbackingStore, XtCBackingStore, XtRBackingStore, sizeof (int),
    	offset (backing_store), XtRString, "default"},
    {XtNborderSize, XtCBorderSize, XtRFloat, sizeof (float),
	offset (border_size), XtRString, "0.1"},
    {XtNjewelSize, XtCBorderSize, XtRFloat, sizeof (float),
	offset (jewel_size), XtRString, "0.075"},
    {XtNshapeWindow, XtCShapeWindow, XtRBoolean, sizeof (Boolean),
	offset (shape_window), XtRImmediate, (XtPointer) True},
    {XtNtransparent, XtCTransparent, XtRBoolean, sizeof (Boolean),
	offset (transparent), XtRImmediate, (XtPointer) False},
};

#undef offset
#undef goffset

static void ClassInitialize ( void );
static void Initialize ( Widget greq, Widget gnew, ArgList args, 
			 Cardinal *num_args );
static void Resize ( Widget widget );
static void Realize ( Widget gw, XtValueMask *valueMask, 
		      XSetWindowAttributes *attrs );
static void Destroy ( Widget gw );
static void Redisplay ( Widget gw, XEvent *event, Region region );
static double clock_to_angle ( double clock );
static void new_time ( XtPointer client_data, XtIntervalId *id );

# define BORDER_SIZE(w)    ((w)->clock.border_size)
# define WINDOW_WIDTH(w)    (2.0 - BORDER_SIZE(w)*2)
# define WINDOW_HEIGHT(w)   (2.0 - BORDER_SIZE(w)*2)
# define MINUTE_WIDTH(w)    (0.05)
# define HOUR_WIDTH(w)	    (0.05)
# define JEWEL_SIZE(w)	    ((w)->clock.jewel_size)
# define MINUTE_LENGTH(w)   (JEWEL_Y(w) - JEWEL_SIZE(w) / 2.0)
# define HOUR_LENGTH(w)	    (MINUTE_LENGTH(w) * 0.6)
# define JEWEL_X(w)	    (0.0)
# define JEWEL_Y(w)	    (1.0 - (BORDER_SIZE(w) + JEWEL_SIZE(w)))

ClockClassRec clockClassRec = {
    { /* core fields */
    /* superclass		*/	&widgetClassRec,
    /* class_name		*/	"Clock",
    /* size			*/	sizeof(ClockRec),
    /* class_initialize		*/	ClassInitialize,
    /* class_part_initialize	*/	NULL,
    /* class_inited		*/	FALSE,
    /* initialize		*/	Initialize,
    /* initialize_hook		*/	NULL,
    /* realize			*/	Realize,
    /* actions			*/	NULL,
    /* num_actions		*/	0,
    /* resources		*/	resources,
    /* num_resources		*/	XtNumber(resources),
    /* xrm_class		*/	NULLQUARK,
    /* compress_motion		*/	TRUE,
    /* compress_exposure	*/	TRUE,
    /* compress_enterleave	*/	TRUE,
    /* visible_interest		*/	FALSE,
    /* destroy			*/	Destroy,
    /* resize			*/	Resize,
    /* expose			*/	Redisplay,
    /* set_values		*/	NULL,
    /* set_values_hook		*/	NULL,
    /* set_values_almost	*/	NULL,
    /* get_values_hook		*/	NULL,
    /* accept_focus		*/	NULL,
    /* version			*/	XtVersion,
    /* callback_private		*/	NULL,
    /* tm_table			*/	NULL,
    /* query_geometry		*/	XtInheritQueryGeometry,
    }
};

static void ClassInitialize()
{
    XtAddConverter( XtRString, XtRBackingStore, XmuCvtStringToBackingStore,
		    NULL, 0 );
}

WidgetClass clockWidgetClass = (WidgetClass) &clockClassRec;

/* ARGSUSED */
static void Initialize (greq, gnew, args, num_args)
    Widget greq, gnew;
    ArgList args;
    Cardinal *num_args;
{
    ClockWidget w = (ClockWidget)gnew;
    XtGCMask	valuemask;
    XGCValues	myXGCV;
    int shape_event_base, shape_error_base;

    valuemask = GCForeground;

    if (w->clock.transparent)
    {
	;
    }
    else
    {
    	myXGCV.foreground = w->clock.minute;
    	w->clock.minuteGC = XtGetGC(gnew, valuemask, &myXGCV);

    	myXGCV.foreground = w->clock.hour;
    	w->clock.hourGC = XtGetGC(gnew, valuemask, &myXGCV);

    	myXGCV.foreground = w->clock.jewel;
    	w->clock.jewelGC = XtGetGC(gnew, valuemask, &myXGCV);
    
    	myXGCV.foreground = w->core.background_pixel;
    	w->clock.eraseGC = XtGetGC(gnew, valuemask, &myXGCV);
    }

    /* wait for Realize to add the timeout */
    w->clock.interval_id = 0;

    if (w->clock.shape_window && !XShapeQueryExtension (XtDisplay (w), 
							&shape_event_base, 
							&shape_error_base))
    w->clock.shape_window = False;
    w->clock.shape_mask = 0;
    w->clock.shapeGC = 0;
    w->clock.shape_width = 0;
    w->clock.shape_height = 0;
    w->clock.polys_valid = 0;
}

static void Resize (widget)
    Widget	widget;
{
    ClockWidget	w = (ClockWidget) widget;
    XGCValues	xgcv;
    Widget	parent;
    XWindowChanges	xwc;
    int		face_width, face_height;
    int		x, y;
    Pixmap	shape_mask;

    if (!XtIsRealized((Widget) w))
	return;

    /*
     * compute desired border size
     */

    SetTransform (&w->clock.maskt,
		  0, w->core.width,
		  w->core.height, 0,
		  -1.0, 1.0,
 		  -1.0, 1.0);

    face_width = abs (Xwidth (BORDER_SIZE(w), BORDER_SIZE(w), &w->clock.maskt));
    face_height = abs (Xheight (BORDER_SIZE(w), BORDER_SIZE(w), &w->clock.maskt));

    /*
     *  shape the windows and borders
     */

    if (w->clock.shape_window) {

	SetTransform (&w->clock.t,
			face_width, w->core.width - face_width,
			w->core.height - face_height, face_height,
			-WINDOW_WIDTH(w)/2, WINDOW_WIDTH(w)/2,
			-WINDOW_HEIGHT(w)/2, WINDOW_HEIGHT(w)/2);
    
	/*
	 * allocate a pixmap to draw shapes in
	 */

	if (w->clock.shape_mask &&
	    (w->clock.shape_width != w->core.width ||
	     w->clock.shape_height != w->core.height))
	{
	    XFreePixmap (XtDisplay (w), w->clock.shape_mask);
	    w->clock.shape_mask = None;
	}
	
	if (!w->clock.shape_mask)
	{
	    w->clock.shape_mask = XCreatePixmap (XtDisplay (w), XtWindow (w),
				    w->core.width, w->core.height, 1);
	}
	shape_mask = w->clock.shape_mask;
    	if (!w->clock.shapeGC)
            w->clock.shapeGC = XCreateGC (XtDisplay (w), shape_mask, 0, &xgcv);

	/* erase the pixmap */
    	XSetForeground (XtDisplay (w), w->clock.shapeGC, 0);
    	XFillRectangle (XtDisplay (w), shape_mask, w->clock.shapeGC,
			0, 0, w->core.width, w->core.height);
    	XSetForeground (XtDisplay (w), w->clock.shapeGC, 1);

	/*
	 * draw the bounding shape.  Doing this first
	 * eliminates extra exposure events.
	 */

	if (w->clock.border_size > 0.0 || !w->clock.transparent)
	{
	    TFillArc (XtDisplay (w), shape_mask,
			    w->clock.shapeGC, &w->clock.maskt,
			    -1.0, -1.0,
			    2.0, 2.0,
			    0, 360 * 64);
	}

	if (w->clock.transparent)
	{
	    if (w->clock.border_size > 0.0)
	    {
	    	XSetForeground (XtDisplay (w), w->clock.shapeGC, 0);
	    	TFillArc (XtDisplay (w), shape_mask,
			    	w->clock.shapeGC, &w->clock.t,
			    	-WINDOW_WIDTH(w)/2, -WINDOW_HEIGHT(w)/2,
			    	WINDOW_WIDTH(w), WINDOW_HEIGHT(w),
			    	0, 360 * 64);
	    	XSetForeground (XtDisplay (w), w->clock.shapeGC, 1);
	    }
	    paint_jewel (w, shape_mask, w->clock.shapeGC);
	    paint_hands (w, shape_mask, w->clock.shapeGC, w->clock.shapeGC);
	}
	/*
	 * Find the highest enclosing widget and shape it
	 */

	x = 0;
	y = 0;
	for (parent = (Widget) w; XtParent (parent); parent = XtParent (parent)) {
	    x = x + parent->core.x + parent->core.border_width;
	    y = y + parent->core.y + parent->core.border_width;
	}

	XShapeCombineMask (XtDisplay (parent), XtWindow (parent), ShapeBounding,
			    x, y, shape_mask, ShapeSet);

	/* erase the pixmap */
    	XSetForeground (XtDisplay (w), w->clock.shapeGC, 0);
    	XFillRectangle (XtDisplay (w), shape_mask, w->clock.shapeGC,
			0, 0, w->core.width, w->core.height);
    	XSetForeground (XtDisplay (w), w->clock.shapeGC, 1);

	/*
	 * draw the clip shape
	 */

	if (w->clock.transparent)
	{
	    paint_jewel (w, shape_mask, w->clock.shapeGC);
	    paint_hands (w, shape_mask, w->clock.shapeGC, w->clock.shapeGC);
	}
	else
	{
	    TFillArc (XtDisplay (w), shape_mask,
			    w->clock.shapeGC, &w->clock.t,
			    -WINDOW_WIDTH(w)/2, -WINDOW_HEIGHT(w)/2,
			    WINDOW_WIDTH(w), WINDOW_HEIGHT(w),
			    0, 360 * 64);
	}

	XShapeCombineMask (XtDisplay (w), XtWindow (w), ShapeClip, 
		    0, 0, shape_mask, ShapeSet);

    } else
    {
    	/*
     	 * reconfigure the widget to split the availible
     	 * space between the window and the border
     	 */

     	if (face_width > face_height)
	    xwc.border_width = face_height;
    	else
	    xwc.border_width = face_width;
    	xwc.width = w->core.width - xwc.border_width * 2;
    	xwc.height = w->core.height - xwc.border_width * 2;
    	XConfigureWindow (XtDisplay (w), XtWindow (w),
			    CWWidth|CWHeight|CWBorderWidth,
			    &xwc);
    
    	SetTransform (&w->clock.t,
	    0, xwc.width,
	    xwc.height, 0,
	    -WINDOW_WIDTH(w)/2, WINDOW_WIDTH(w)/2,
	    -WINDOW_HEIGHT(w)/2, WINDOW_HEIGHT(w)/2);
    }
}
 
static void Realize (gw, valueMask, attrs)
     Widget gw;
     XtValueMask *valueMask;
     XSetWindowAttributes *attrs;
{
     ClockWidget	w = (ClockWidget)gw;

    if (w->clock.backing_store != Always + WhenMapped + NotUseful) {
     	attrs->backing_store = w->clock.backing_store;
	*valueMask |= CWBackingStore;
    }
    if (w->clock.transparent)
    {
	attrs->background_pixel = w->clock.minute;
	*valueMask |= CWBackPixel;
	*valueMask &= ~CWBackPixmap;
    }
    XtCreateWindow( gw, (unsigned)InputOutput, (Visual *)CopyFromParent,
		     *valueMask, attrs );
    if (!w->clock.transparent)
	Resize (gw);
    new_time ((XtPointer) gw, 0);
}

static void Destroy (gw)
     Widget gw;
{
     ClockWidget w = (ClockWidget)gw;
     if (w->clock.interval_id) XtRemoveTimeOut (w->clock.interval_id);
     if (! w->clock.transparent) {
	 XtReleaseGC(gw, w->clock.minuteGC);
	 XtReleaseGC(gw, w->clock.hourGC);
	 XtReleaseGC(gw, w->clock.jewelGC);
	 XtReleaseGC(gw, w->clock.eraseGC);
     }
     if (w->clock.shapeGC)
	XFreeGC(XtDisplay(gw), w->clock.shapeGC);
    if (w->clock.shape_mask)
	XFreePixmap (XtDisplay (w), w->clock.shape_mask);
}

/* ARGSUSED */
static void Redisplay(gw, event, region)
     Widget gw;
     XEvent *event;
     Region region;
{
    ClockWidget	w;

    w = (ClockWidget) gw;
    if (!w->clock.transparent)
    {
    	paint_jewel (w, XtWindow (w), w->clock.jewelGC);
    	paint_hands (w, XtWindow (w), w->clock.minuteGC, w->clock.hourGC);
    }
}

/*
 * routines to draw the hands and jewel
 */

#ifndef PI	/* may be found in <math.h> */
# define PI (3.14159265358979323846)
#endif

/*
 * converts a number from 0..1 representing a clockwise radial distance
 * from the 12 oclock position to a radian measure of the counter-clockwise
 * distance from the 3 oclock position
 */

static double
clock_to_angle (clock)
double	clock;
{
	if (clock >= .75)
		clock -= 1.0;
	return -2.0 * PI * clock + PI / 2.0;
}

/* ARGSUSED */
static void new_time (client_data, id)
     XtPointer client_data;
     XtIntervalId *id;		/* unused */
{
        ClockWidget	w = (ClockWidget)client_data;
	Time_t		now;
	struct tm	*tm;
	
	if (!w->clock.transparent)
	if (w->clock.polys_valid) {
		paint_hands (w, XtWindow (w), w->clock.eraseGC, w->clock.eraseGC);
		check_jewel (w, XtWindow (w), w->clock.jewelGC);
	}
	(void) time (&now);
	tm = localtime (&now);
	if (tm->tm_hour >= 12)
		tm->tm_hour -= 12;
	w->clock.hour_angle = clock_to_angle ((((double) tm->tm_hour) +
				((double) tm->tm_min) / 60.0) / 12.0);
	w->clock.minute_angle =
		clock_to_angle (((double) tm->tm_min) / 60.0);
	/*
	 * add the timeout before painting the hands, that may
	 * take a while and we'd like the clock to keep up
	 * with time changes.
	 */
	w->clock.interval_id = 
	    XtAppAddTimeOut (XtWidgetToApplicationContext((Widget) w),
			     (60 - tm->tm_sec) * 1000, new_time, client_data);
	compute_hands (w);
	if (w->clock.transparent)
	    Resize ((Widget)w);
	else
	    paint_hands (w, XtWindow (w), w->clock.minuteGC, w->clock.hourGC);
} /* new_time */

void
paint_jewel (w, d, gc)
ClockWidget w;
Drawable    d;
GC	    gc;
{
    if (JEWEL_SIZE(w) > 0.0)
    {
	TFillArc (XtDisplay (w), d, gc, &w->clock.t,
			JEWEL_X(w) - JEWEL_SIZE(w) / 2.0,
 			JEWEL_Y(w) - JEWEL_SIZE(w) / 2.0,
 			JEWEL_SIZE(w), JEWEL_SIZE(w), 0, 360 * 64);
    }
}

#define sqr(x)	((x)*(x))

/*
 * check to see if the polygon intersects the circular jewel
 */
int
check_jewel_poly (w, poly)
ClockWidget	w;
TPoint		poly[POLY_SIZE];
{
    double	a2, b2, c2, d2;
    double	x, y, size;
    int	i;

    if (JEWEL_SIZE(w) > 0.0)
    {
	x = JEWEL_X(w);
	y = JEWEL_Y(w);
	size = JEWEL_SIZE(w);
	/*
	 * check each side of the polygon to see if the
	 * distance from the line to the center of the
	 * circular jewel is less than the radius.
	 */
	for (i = 0; i < POLY_SIZE-1; i++) {
		a2 = sqr (poly[i].x - x) + sqr (poly[i].y - y);
		b2 = sqr (poly[i+1].x - x) + sqr (poly[i+1].y - y);
		c2 = sqr (poly[i].x - poly[i+1].x) + sqr (poly[i].y - poly[i+1].y);
		d2 = a2 + b2 - c2;
		if ((d2 <= sqr (size) &&
		    a2 <= 2 * c2 && b2 <= 2 * c2) ||
 		    a2 <= sqr (size) ||
		    b2 <= sqr (size))
			return 1;
	}
    }
    return 0;
}

void
check_jewel (w, d, gc)
ClockWidget	w;
Drawable	d;
GC		gc;
{
	if (!w->clock.polys_valid || JEWEL_SIZE(w) <= 0.0)
		return;
	if ((MINUTE_LENGTH(w) >= (JEWEL_Y(w) - JEWEL_SIZE(w)/2.0) &&
	     check_jewel_poly (w, w->clock.minute_poly)) ||
	    (HOUR_LENGTH(w) >= (JEWEL_Y(w) - JEWEL_SIZE(w)/2.0) &&
	     check_jewel_poly (w, w->clock.minute_poly)))
	{
		paint_jewel (w, d, gc);
	}
}

/*
 * A hand is a rectangle with a triangular cap at the far end.
 * This is represented with a five sided polygon.
 */
void
compute_hand (w, a, l, width, poly)
ClockWidget	w;
double		a, l, width;
TPoint		poly[POLY_SIZE];
{
	double	c, s;

	c = cos(a);
	s = sin(a);
	poly[0].x = c * l;	
	poly[0].y = s * l;
	poly[1].x = (l - width) * c - s * width;
	poly[1].y = (l - width) * s + c * width;
	poly[2].x = (-width) * c - s * width;
	poly[2].y = (-width) * s + c * width;
	poly[3].x = (-width) * c + s * width;
	poly[3].y = (-width) * s - c * width;
	poly[4].x = (l - width) * c + s * width;
	poly[4].y = (l - width) * s - c * width;
	poly[5].x = poly[0].x;
	poly[5].y = poly[0].y;
}

void
compute_hands (w)
ClockWidget	w;
{
	compute_hand (w, w->clock.minute_angle,
		MINUTE_LENGTH(w), MINUTE_WIDTH(w), w->clock.minute_poly);
	compute_hand (w, w->clock.hour_angle,
		HOUR_LENGTH(w), HOUR_WIDTH(w), w->clock.hour_poly);
	w->clock.polys_valid = 1;
}

void
paint_hand (w, d, gc, poly)
ClockWidget	w;
Drawable	d;
GC		gc;
TPoint		poly[POLY_SIZE];
{
	TFillPolygon (XtDisplay (w), d, gc, &w->clock.t, poly, POLY_SIZE,
			Convex, CoordModeOrigin);
}

void
paint_hands (w, d, minute_gc, hour_gc)
ClockWidget	w;
Drawable	d;
GC		minute_gc, hour_gc;
{
    if (w->clock.polys_valid) {
	paint_hand (w, d, hour_gc, w->clock.hour_poly);
	paint_hand (w, d, minute_gc, w->clock.minute_poly);
    }
}

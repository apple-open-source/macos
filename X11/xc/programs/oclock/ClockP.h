/* $Xorg: ClockP.h,v 1.4 2001/02/09 02:05:33 xorgcvs Exp $ */
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
/* $XFree86: xc/programs/oclock/ClockP.h,v 1.5 2001/12/14 20:01:00 dawes Exp $ */


#ifndef _ClockP_h
#define _ClockP_h

#include "Clock.h"
#include <X11/CoreP.h>
#include "transform.h"

#define POLY_SIZE	6

/* New fields for the clock widget instance record */
typedef struct {
	Pixel		minute;
	Pixel		hour;
	Pixel		jewel;
	GC		minuteGC;
	GC		hourGC;
	GC		jewelGC;
	GC		eraseGC;
	GC		shapeGC;	/* pointer to GraphicsContext */
/* start of graph stuff */
	int		backing_store;	/* backing store variety */
	Boolean		shape_window;	/* use SetWindowShapeMask */
	Boolean		transparent;	/* make window transparent */
	float		border_size;
	float		jewel_size;
	XtIntervalId	interval_id;
	Transform	t;
	Transform	maskt;
	Pixmap		shape_mask;	/* window shape */
	int		shape_width;	/* window width when shape last made */
	int		shape_height;	/* window height when shape last made */
	double		hour_angle;	/* hour hand position */
	double		minute_angle;	/* minute hand position */
	int		polys_valid;	/* polygons contain good data */
	TPoint		minute_poly[POLY_SIZE];	/* polygon for minute hand */
	TPoint		hour_poly[POLY_SIZE];	/* polygon for hour hand */
} ClockPart;

/* Full instance record declaration */
typedef struct _ClockRec {
	CorePart core;
	ClockPart clock;
} ClockRec;

/* New fields for the Clock widget class record */
typedef struct {int dummy;} ClockClassPart;

/* Full class record declaration. */
typedef struct _ClockClassRec {
	CoreClassPart core_class;
	ClockClassPart clock_class;
} ClockClassRec;

/* Class pointer. */
extern ClockClassRec clockClassRec;

extern void paint_jewel ( ClockWidget w, Drawable d, GC gc );
extern int check_jewel_poly ( ClockWidget w, TPoint poly[6] );
extern void check_jewel ( ClockWidget w, Drawable d, GC gc );
extern void compute_hand ( ClockWidget w, double a, double l, double width, 
			  TPoint poly[6] );
extern void compute_hands ( ClockWidget w );
extern void paint_hand ( ClockWidget w, Drawable d, GC gc, TPoint poly[6] );
extern void paint_hands ( ClockWidget w, Drawable d, GC minute_gc, GC hour_gc );

#endif /* _ClockP_h */

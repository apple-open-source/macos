/*
* $XConsortium: EyesP.h,v 1.9 90/12/01 13:04:43 rws Exp $
*/

#ifndef _EyesP_h
#define _EyesP_h

#include "Eyes.h"
#include <X11/CoreP.h>
#include "transform.h"

#define SEG_BUFF_SIZE		128

/* New fields for the eyes widget instance record */
typedef struct {
	 Pixel		puppixel;	/* foreground pixel */
	 Pixel		outline;	/* outline pixel */
	 Pixel		center;		/* inside pixel */
	 GC		outGC;		/* pointer to GraphicsContext */
	 GC		pupGC;		/* pointer to GraphicsContext */
	 GC		centerGC;	/* pointer to GraphicsContext */
	 GC		shapeGC;	/* pointer to GraphicsContext */
/* start of graph stuff */
	 int		backing_store;	/* backing store variety */
	 Boolean	reverse_video;	/* swap fg and bg pixels */
	 Boolean	shape_window;	/* use SetWindowShapeMask */
	 int		update;		/* current timeout index */
	 TPoint		mouse;		/* old mouse position */
	 TPoint		pupil[2];	/* pupil position */
	 Transform	t;
	 Transform	maskt;
	 XtIntervalId	interval_id;
	 Pixmap		shape_mask;	/* window shape */
   } EyesPart;

/* Full instance record declaration */
typedef struct _EyesRec {
   CorePart core;
   EyesPart eyes;
   } EyesRec;

/* New fields for the Eyes widget class record */
typedef struct {int dummy;} EyesClassPart;

/* Full class record declaration. */
typedef struct _EyesClassRec {
   CoreClassPart core_class;
   EyesClassPart eyes_class;
   } EyesClassRec;

/* Class pointer. */
extern EyesClassRec eyesClassRec;

#endif /* _EyesP_h */


#ifndef _EyesP_h
#define _EyesP_h

#include "Eyes.h"
#include <X11/CoreP.h>
#ifdef XRENDER
#include <X11/extensions/Xrender.h>
#endif
#include "transform.h"

#define SEG_BUFF_SIZE		128

/* New fields for the eyes widget instance record */
typedef struct {
	 Pixel		pixel[PART_SHAPE];
	 GC		gc[PART_MAX];
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
#ifdef XRENDER
	 Boolean    	render;
	 Picture	picture;
	 Picture	fill[PART_SHAPE];
#endif
	 Boolean    	distance;
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

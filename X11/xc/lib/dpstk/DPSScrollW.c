 /* 
 * DPSScrollW.c
 *
 * (c) Copyright 1993-1994 Adobe Systems Incorporated.
 * All rights reserved.
 * 
 * Permission to use, copy, modify, distribute, and sublicense this software
 * and its documentation for any purpose and without fee is hereby granted,
 * provided that the above copyright notices appear in all copies and that
 * both those copyright notices and this permission notice appear in
 * supporting documentation and that the name of Adobe Systems Incorporated
 * not be used in advertising or publicity pertaining to distribution of the
 * software without specific, written prior permission.  No trademark license
 * to use the Adobe trademarks is hereby granted.  If the Adobe trademark
 * "Display PostScript"(tm) is used to describe this software, its
 * functionality or for any other purpose, such use shall be limited to a
 * statement that this software works in conjunction with the Display
 * PostScript system.  Proper trademark attribution to reflect Adobe's
 * ownership of the trademark shall be given whenever any such reference to
 * the Display PostScript system is made.
 * 
 * ADOBE MAKES NO REPRESENTATIONS ABOUT THE SUITABILITY OF THE SOFTWARE FOR
 * ANY PURPOSE.  IT IS PROVIDED "AS IS" WITHOUT EXPRESS OR IMPLIED WARRANTY.
 * ADOBE DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NON- INFRINGEMENT OF THIRD PARTY RIGHTS.  IN NO EVENT SHALL ADOBE BE LIABLE
 * TO YOU OR ANY OTHER PARTY FOR ANY SPECIAL, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE, STRICT LIABILITY OR ANY OTHER ACTION ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.  ADOBE WILL NOT
 * PROVIDE ANY TRAINING OR OTHER SUPPORT FOR THE SOFTWARE.
 * 
 * Adobe, PostScript, and Display PostScript are trademarks of Adobe Systems
 * Incorporated which may be registered in certain jurisdictions
 * 
 * Author:  Adobe Systems Incorporated
 */
/* $XFree86: xc/lib/dpstk/DPSScrollW.c,v 1.2 2000/06/07 22:02:59 tsi Exp $ */

#include <X11/IntrinsicP.h>
#include <X11/StringDefs.h>
#include <X11/ShellP.h>
#include <X11/Xproto.h>
#include <stdlib.h>
#include <Xm/Xm.h>

/* There are no words to describe how I feel about having to do this */

#if XmVersion > 1001		
#include <Xm/ManagerP.h>
#else
#include <Xm/XmP.h>
#endif

#include <Xm/DrawingA.h>
#include <Xm/ScrolledW.h>
#include <Xm/ScrollBar.h>

#include <DPS/dpsXclient.h>
#include "dpsXcommonI.h"
#include <DPS/dpsXshare.h>
#include "DSWwraps.h"
#include <stdio.h>
#include <DPS/DPSScrollWP.h>

#undef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#undef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#undef ABS
#define ABS(x) ((x) >= 0 ? (x) : -(x))
#undef CEIL
#define CEIL(x) ((int) ((float)((int)(x)) == (x) ? (x) : (x) + 1))

/* Define macros to get rectangle entries.  All rectangles are stored as
   x, y, width, height.  NOTE:  ONLY FOR USER SPACE RECTANGLES, NOT X
   RECTANGLES!!!! */

#define LEFT(r) ((r)[0])
#define RIGHT(r) ((r)[0] + (r)[2])
#define BOTTOM(r) ((r)[1])
#define TOP(r) ((r)[1] + (r)[3])
#define WIDTH(r) ((r)[2])
#define HEIGHT(r) ((r)[3])

/* This is used in converting bounding boxes into user space to ensure
   that we don't end up slopping over into another pixel */

#define DELTA .001

#define Offset(field) XtOffsetOf(DPSScrolledWindowRec, sw.field)

static float initScale = 1.0;

static XtResource resources[] = {
    {XtNcontext, XtCContext, XtRDPSContext, sizeof(DPSContext),
	Offset(context), XtRImmediate, (XtPointer) NULL},
    {XtNareaWidth, XtCAreaWidth, XtRInt, sizeof(int),
	Offset(area_width), XtRImmediate, (XtPointer) ((int) (8.5*72))},
    {XtNareaHeight, XtCAreaHeight, XtRInt, sizeof(int),
	Offset(area_height), XtRImmediate, (XtPointer) (11*72)},
    {XtNscale, XtCScale, XtRFloat, sizeof(float),
	Offset(scale), XtRFloat, (XtPointer) &initScale},
    {XtNctm, XtCCtm, XtRFloatArray, sizeof(float *),
	Offset(ctm_ptr), XtRImmediate, (XtPointer) NULL},
    {XtNinvCtm, XtCInvCtm, XtRFloatArray, sizeof(float *),
	Offset(inv_ctm_ptr), XtRImmediate, (XtPointer) NULL},
    {XtNuseBackingPixmap, XtCUseBackingPixmap, XtRBoolean, sizeof(Boolean),
	Offset(use_backing_pixmap), XtRImmediate, (XtPointer) True},
    {XtNuseFeedbackPixmap, XtCUseFeedbackPixmap, XtRBoolean, sizeof(Boolean),
	Offset(use_feedback_pixmap), XtRImmediate, (XtPointer) True},
    {XtNbackingPixmap, XtCBackingPixmap, XtRPixmap, sizeof(Pixmap),
	Offset(backing_pixmap), XtRImmediate, (XtPointer) None},
    {XtNfeedbackPixmap, XtCFeedbackPixmap, XtRPixmap, sizeof(Pixmap),
	Offset(feedback_pixmap), XtRImmediate, (XtPointer) None},
    {XtNdocumentSizePixmaps, XtCDocumentSizePixmaps,
	XtRBoolean, sizeof(Boolean),
	Offset(document_size_pixmaps), XtRImmediate, (XtPointer) False},
    {XtNwindowGState, XtCWindowGState, XtRDPSGState, sizeof(DPSGState),
	Offset(window_gstate), XtRImmediate, (XtPointer) 0},
    {XtNbackingGState, XtCBackingGState, XtRDPSGState, sizeof(DPSGState),
	Offset(backing_gstate), XtRImmediate, (XtPointer) 0},
    {XtNfeedbackGState, XtCFeedbackGState, XtRDPSGState, sizeof(DPSGState),
	Offset(feedback_gstate), XtRImmediate, (XtPointer) 0},
    {XtNdirtyAreas, XtCDirtyAreas, XtRFloatArray, sizeof(float *),
	Offset(dirty_areas), XtRImmediate, (XtPointer) NULL},
    {XtNnumDirtyAreas, XtCNumDirtyAreas, XtRShort, sizeof(short),
	Offset(num_dirty_areas), XtRImmediate, (XtPointer) 0},
    {XtNpixmapLimit, XtCPixmapLimit, XtRInt, sizeof(int),
	Offset(pixmap_limit), XtRImmediate, (XtPointer) -1},
    {XtNabsolutePixmapLimit, XtCAbsolutePixmapLimit, XtRInt, sizeof(int),
	Offset(absolute_pixmap_limit), XtRImmediate, (XtPointer) 0},
    {XtNwatchProgress, XtCWatchProgress, XtRBoolean, sizeof(Boolean),
	Offset(watch_progress), XtRImmediate, (XtPointer) False},
    {XtNwatchProgressDelay, XtCWatchProgressDelay, XtRInt, sizeof(int),
	Offset(watch_progress_delay), XtRImmediate, (XtPointer) 1000},
    {XtNminimalDrawing, XtCMinimalDrawing, XtRBoolean, sizeof(Boolean),
	Offset(minimal_drawing), XtRImmediate, (XtPointer) False},
    {XtNapplicationScrolling, XtCApplicationScrolling,
	XtRBoolean, sizeof(Boolean),
	Offset(application_scrolling), XtRImmediate, (XtPointer) False},
    {XtNsetupCallback, XtCCallback, XtRCallback, sizeof(XtCallbackList),
	Offset(setup_callback), XtRCallback, (XtPointer) NULL},
    {XtNexposeCallback, XtCCallback, XtRCallback, sizeof(XtCallbackList),
	Offset(expose_callback), XtRCallback, (XtPointer) NULL},
    {XtNbackgroundCallback, XtCCallback, XtRCallback, sizeof(XtCallbackList),
	Offset(background_callback), XtRCallback, (XtPointer) NULL},
    {XtNfeedbackCallback, XtCCallback, XtRCallback, sizeof(XtCallbackList),
	Offset(feedback_callback), XtRCallback, (XtPointer) NULL},
    {XtNresizeCallback, XtCCallback, XtRCallback, sizeof(XtCallbackList),
	Offset(resize_callback), XtRCallback, (XtPointer) NULL},
};

static Boolean GiveFeedbackPixmap(Widget w, Pixmap p, int width, int height, int depth, Screen *screen);
static Boolean SetValues(Widget old, Widget req, Widget new, ArgList args, Cardinal *num_args);
static Boolean TakeFeedbackPixmap(Widget w, Pixmap *p, int *width, int *height, int *depth, Screen **screen);
static XtGeometryResult GeometryManager(Widget w, XtWidgetGeometry *desired, XtWidgetGeometry *allowed);
static XtGeometryResult QueryGeometry(Widget w, XtWidgetGeometry *desired, XtWidgetGeometry *allowed);
static void AbortPendingDrawing(Widget w);
static void AddExposureToPending(DPSScrolledWindowWidget dsw, XExposeEvent *ev);
static void AddRectsToDirtyArea(DPSScrolledWindowWidget dsw, float *newRect, int n);
static void AddRectsToPending(DPSScrolledWindowWidget dsw, int *newRect, int n);
static void AddToDirtyArea(Widget w, float *rect, long n);
static void AddUserSpaceRectsToPending(DPSScrolledWindowWidget dsw, float *newRect, int n);
static void CallFeedbackCallback(DPSScrolledWindowWidget dsw, float *r, int n);
static void CheckFeedbackPixmap(DPSScrolledWindowWidget dsw);
static void ClassPartInitialize(WidgetClass widget_class);
static void ConvertPSToX(Widget w, double psX, double psY, int *xX, int *xY);
static void ConvertToOrigPS(DPSScrolledWindowWidget dsw, int xX, int xY, float *psX, float *psY);
static void ConvertToPS(DPSScrolledWindowWidget dsw, float xX, float xY, float *psX, float *psY);
static void ConvertToX(DPSScrolledWindowWidget dsw, float psX, float psY, int *xX, int *xY);
static void ConvertXToPS(Widget w, long xX, long xY, float *psX, float *psY);
static void CopyRectsToCurrentDrawing(DPSScrolledWindowWidget dsw, float *newRect, int n);
static void CopyRectsToDirtyArea(DPSScrolledWindowWidget dsw, float *newRect, int n);
static void CopyToFeedbackPixmap(DPSScrolledWindowWidget dsw, float *rects, int n);
static void Destroy(Widget widget);
static void DrawingAreaExpose(Widget w, XtPointer clientData, XtPointer callData);
static void DrawingAreaGraphicsExpose(Widget w, XtPointer clientData, XEvent *event, Boolean *goOn);
static void EndFeedbackDrawing(Widget w, int restore);
static void FinishDrawing(DPSScrolledWindowWidget dsw);
static void FinishPendingDrawing(Widget w);
static void GetDrawingInfo(Widget w, DSWDrawableType *type, Drawable *drawable, DPSGState *gstate, DPSContext *context);
static void GetScrollInfo(Widget w, int *h_value, int *h_size, int *h_max, int *v_value, int *v_size, int *v_max);
static void HScrollCallback(Widget w, XtPointer clientData, XtPointer callData);
static void Initialize(Widget request, Widget new, ArgList args, Cardinal *num_args);
static void Realize(Widget w, XtValueMask *mask, XSetWindowAttributes *attr);
static void Resize(Widget w);
static void ScrollBy(Widget w, long dx, long dy);
static void ScrollMoved(DPSScrolledWindowWidget dsw);
static void ScrollPoint(Widget w, double psX, double psY, long xX, long xY);
static void ScrollTo(Widget w, long x, long y);
static void SetFeedbackDirtyArea(Widget w, float *rects, int count, XtPointer continue_feedback_data);
static void SetScale(Widget w, double scale, long fixedX, long fixedY);
static void SetScaleAndScroll(Widget w, double scale, double psX, double psY, long xX, long xY);
static void StartFeedbackDrawing(Widget w, XtPointer start_feedback_data);
static void UpdateDrawing(Widget w, float *rects, int count);
static void VScrollCallback(Widget w, XtPointer clientData, XtPointer callData);

DPSScrolledWindowClassRec dpsScrolledWindowClassRec = {
    /* Core class part */
  {
    /* superclass	     */	(WidgetClass) &xmManagerClassRec,
    /* class_name	     */ "DPSScrolledWindow",
    /* widget_size	     */ sizeof(DPSScrolledWindowRec),
    /* class_initialize      */ NULL,
    /* class_part_initialize */ ClassPartInitialize,
    /* class_inited          */	False,
    /* initialize	     */	Initialize,
    /* initialize_hook       */	NULL,
    /* realize		     */	Realize,
    /* actions		     */	NULL,
    /* num_actions	     */	0,
    /* resources	     */	resources,
    /* num_resources	     */	XtNumber(resources),
    /* xrm_class	     */	NULLQUARK,
    /* compress_motion	     */	True,
    /* compress_exposure     */	XtExposeCompressMultiple,
    /* compress_enterleave   */	True,
    /* visible_interest	     */	False,
    /* destroy		     */	Destroy,
    /* resize		     */	Resize,
    /* expose		     */	NULL,
    /* set_values	     */	SetValues,
    /* set_values_hook       */	NULL,			
    /* set_values_almost     */	XtInheritSetValuesAlmost,  
    /* get_values_hook       */	NULL,			
    /* accept_focus	     */	NULL,
    /* version		     */	XtVersion,
    /* callback offsets      */	NULL,
    /* tm_table              */	NULL,
    /* query_geometry	     */	QueryGeometry,
    /* display_accelerator   */	NULL,
    /* extension	     */	NULL,
  },
   /* Composite class part */
  {
    /* geometry_manager	     */	GeometryManager,
    /* change_managed	     */	NULL,
    /* insert_child	     */	XtInheritInsertChild,
    /* delete_child	     */	XtInheritDeleteChild,
    /* extension	     */	NULL,
  },
   /* Constraint class part */
  {
    /* resources	     */ NULL,
    /* num_resources	     */ 0,
    /* constraint_size	     */ 0,
    /* initialize	     */ NULL,
    /* destroy		     */ NULL,
    /* set_values	     */ NULL,
    /* extension	     */ NULL,
  },
   /* Manager class part */
  {
    /* translations	     */ XtInheritTranslations,
    /* syn_resources	     */ NULL,
    /* num_syn_resources     */ 0,
    /* syn_constraint_resources */ NULL,
    /* num_syn_constraint_resources */ 0,
    /* parent_process	     */ XmInheritParentProcess,
    /* extension	     */ NULL,
  },
   /* DPSScrolledWindow class part */
  {
    /* set_scale	     */ SetScale,
    /* scroll_point	     */ ScrollPoint,
    /* scroll_by	     */ ScrollBy,
    /* scroll_to	     */ ScrollTo,
    /* set_scale_and_scroll  */ SetScaleAndScroll,
    /* convert_x_to_ps	     */ ConvertXToPS,
    /* convert_ps_to_x	     */ ConvertPSToX,
    /* add_to_dirty_area     */ AddToDirtyArea,
    /* take_feedback_pixmap  */ TakeFeedbackPixmap,
    /* give_feedback_pixmap  */ GiveFeedbackPixmap,
    /* start_feedback_drawing */ StartFeedbackDrawing,
    /* end_feedback_drawing  */ EndFeedbackDrawing,
    /* set_feedback_dirty_area */ SetFeedbackDirtyArea,
    /* finish_pending_drawing */ FinishPendingDrawing,
    /* abort_pending_drawing */ AbortPendingDrawing,
    /* get_drawing_info	     */ GetDrawingInfo,
    /* update_drawing	     */ UpdateDrawing,
    /* get_scroll_info	     */ GetScrollInfo,				
    /* extension	     */	NULL,
  }
};

WidgetClass dpsScrolledWindowWidgetClass =
	(WidgetClass) &dpsScrolledWindowClassRec;

/***** UTILITY FUNCTIONS *****/

static void PrintRectList(float *r, short num_r)
{
    int i;

    for (i = 0; i < num_r; i++) {
	printf("Rectangle %d:  ", i);
	printf("X %g Y %g W %g H %g\n", r[0], r[1], r[2], r[3]);
	r += 4;
    }
}

/* Make sure the list pointed to by r can hold n more rectangles.  Always
   grow by at least min_grow */

static void GrowRectList(
    float **r,
    short *r_size,
    short num_r,
    int n, int min_grow)
{
    if (*r_size < num_r + n) {
	if (min_grow > 1 && num_r + n - *r_size < min_grow) {
	    *r_size += min_grow;
	} else *r_size = num_r + n;
	*r = (float *) XtRealloc((char *) *r, *r_size * 4 * sizeof(float));
    }
}

static void GrowIntRectList(
    int **r,
    short *r_size,
    short num_r,
    int n, int min_grow)
{
    if (*r_size < num_r + n) {
	if (min_grow > 1 && num_r + n - *r_size < min_grow) {
	    *r_size += min_grow;
	} else *r_size = num_r + n;
	*r = (int *) XtRealloc((char *) *r, *r_size * 4 * sizeof(int));
    }
}

static Boolean Intersects(float *r1, float *r2)
{
    if (RIGHT(r1) <= LEFT(r2)) return False;
    if (RIGHT(r2) <= LEFT(r1)) return False;
    if (TOP(r1) <= BOTTOM(r2)) return False;
    if (TOP(r2) <= BOTTOM(r1)) return False;

    return True;
}

/* Subtract sub from src, putting result into dst.  Return rectangle count */

static int Subtract(float *src, float *sub, float *dst)
{
    int n = 0;

    /* If bottom of sub is greater than bottom of src, there's a
       rectangle across the bottom */
    if (BOTTOM(sub) > BOTTOM(src)) {
	LEFT(dst) = LEFT(src);
	BOTTOM(dst) = BOTTOM(src);
	WIDTH(dst) = WIDTH(src);
	HEIGHT(dst) = BOTTOM(sub) - BOTTOM(src);
	n++;
	dst += 4;
    }

    /* If left of sub is greater than left of src, there's a left rectangle. */
    if (LEFT(sub) > LEFT(src)) {
	LEFT(dst) = LEFT(src);
	BOTTOM(dst) = MAX(BOTTOM(src), BOTTOM(sub));
	WIDTH(dst) = LEFT(sub) - LEFT(src);
	HEIGHT(dst) = MIN(TOP(src), TOP(sub)) - BOTTOM(dst);
	n++;
	dst += 4;
    }
    
    /* If right of sub is less than right of src, there's a right rect */
    if (RIGHT(sub) < RIGHT(src)) {
	LEFT(dst) = RIGHT(sub);
	BOTTOM(dst) = MAX(BOTTOM(src), BOTTOM(sub));
	WIDTH(dst) = RIGHT(src) - RIGHT(sub);	
	HEIGHT(dst) = MIN(TOP(src), TOP(sub)) - BOTTOM(dst);
	n++;
	dst += 4;
    }

    /* If top of sub is less than top of src, there's a top rectangle */
    if (TOP(sub) < TOP(src)) {
	LEFT(dst) = LEFT(src);
	BOTTOM(dst) = TOP(sub);
	WIDTH(dst) = WIDTH(src);
	HEIGHT(dst) = TOP(src) - TOP(sub);
	n++;
	dst += 4;
    }

    return n;
}

static void Copy(float *src, float *dst)
{
    LEFT(dst) = LEFT(src);
    BOTTOM(dst) = BOTTOM(src);
    WIDTH(dst) = WIDTH(src);
    HEIGHT(dst) = HEIGHT(src);
}

static void Intersection(float *r1, float *r2, float *dst)
{
    LEFT(dst) = MAX(LEFT(r1), LEFT(r2));
    BOTTOM(dst) = MAX(BOTTOM(r1), BOTTOM(r2));
    WIDTH(dst) = MIN(RIGHT(r1), RIGHT(r2)) - LEFT(dst);
    HEIGHT(dst) = MIN(TOP(r1), TOP(r2)) - BOTTOM(dst);
}

/* These are used by the SubtractRects and IntersectRects procedures */

static float *rbuf = NULL;
static short rbuf_size = 0;
#define GROW_BUF 10

/* Replace the rectangle list in src with src minus sub */

static void SubtractRects(
    float **src,
    short *src_size,
    short *num_src,
    float *sub,
    int num_sub)
{
    short num_rbuf;
    float *r;
    int i;

    /* Go through, subtracting the first sub rectangle from each src
       rectangle.  Put the result in the internal buffer, then copy this
       list to the src.  Repeat for each sub rectangle. */

    while (num_sub > 0) {
	num_rbuf = 0;
	for (r = *src, i = 0; i < *num_src; r += 4, i++) {
	    if (Intersects(r, sub)) {
		/* Subtract sub from r, putting result into rbuf.  First
		   make sure there are at least 4 spaces in the buffer */
		GrowRectList(&rbuf, &rbuf_size, num_rbuf, 4, GROW_BUF);

		/* Do the subtraction */
		num_rbuf += Subtract(r, sub, rbuf + (num_rbuf*4));
	    } else {
		/* Copy r into buffer */
		GrowRectList(&rbuf, &rbuf_size, num_rbuf, 1, GROW_BUF);
		Copy(r, rbuf + (num_rbuf*4));
		num_rbuf++;
	    }
	}

	/* Copy buffered rectangles back into src */
	GrowRectList(src, src_size, 0, num_rbuf, 1);
	for (i = 0; i < num_rbuf * 4; i++) (*src)[i] = rbuf[i];
	*num_src = num_rbuf;

	/* Check if we've taken everything away */
	if (*num_src == 0) return;

	/* Skip on to the next sub rectangle */
	num_sub--;
	sub += 4;
    }
}

/* Replace list r1 with the intersection of r1 and r2 */

static void IntersectRects(
    float **r1,
    short *r1_size,
    short *num_r1,
    float *r2,
    int num_r2)
{
    short num_rbuf = 0;
    float *r;
    int i;

    /* Fairly straightforward.  Intersect each rectangle in r1 with each
       rectangle in r2, then copy the results to r1 */

    while (num_r2 > 0) {
	for (r = *r1, i = 0; i < *num_r1; r += 4, i++) {
	    if (Intersects(r, r2)) {
		GrowRectList(&rbuf, &rbuf_size, num_rbuf, 1, GROW_BUF);
		Intersection(r, r2, rbuf + (num_rbuf*4));
		num_rbuf++;
	    }
	}
	num_r2--;
	r2 += 4;
    }
    
    /* Copy intersection rectangles back into r1 */
    GrowRectList(r1, r1_size, 0, num_rbuf, 1);
    for (i = 0; i < num_rbuf * 4; i++) (*r1)[i] = rbuf[i];
    *num_r1 = num_rbuf;
}

static void SimplifyRects(float *rect, short *num)
{
    int i, j, k;
    float *r, *r1;

    i = 0;
    while (i < *num) {
	r = rect + (i * 4);
	if (WIDTH(r) == 0 || HEIGHT(r) == 0) {
	    for (k = 4*(i+1); k < *num * 4; k++) rect[k-4] = rect[k];
	    (*num)--;
	    goto LOOPEND;
	}
	j = i+1;
	while (j < *num) {
	    r1 = rect + (j * 4);
	    if (TOP(r1) <= TOP(r) && BOTTOM(r1) >= BOTTOM(r) &&
		LEFT(r1) >= LEFT(r) && RIGHT(r1) <= RIGHT(r)) {
		for (k = 4*(j+1); k < *num * 4; k++) rect[k-4] = rect[k];
		(*num)--;
	    } else if (TOP(r) <= TOP(r1) && BOTTOM(r) >= BOTTOM(r1) &&
		       LEFT(r) >= LEFT(r1) && RIGHT(r) <= RIGHT(r1)) {
		for (k = 4*(i+1); k < *num * 4; k++) rect[k-4] = rect[k];
		(*num)--;
		goto LOOPEND;
	    } else j++;
	}
	i++;
LOOPEND:;
    }
}

static void ComputeOffsets(DPSScrolledWindowWidget dsw, int *dx, int *dy)
{
    if (dsw->sw.doing_feedback && dsw->sw.feedback_pixmap != None) {
	*dx = *dy = 0;
    } else {
	if (dsw->sw.pixmap_width == dsw->sw.drawing_area->core.width) *dx = 0;
	else *dx = -dsw->sw.origin_x;
	if (dsw->sw.pixmap_height == dsw->sw.drawing_area->core.height) *dy = 0;
	else *dy = CEIL(dsw->sw.drawing_height) - dsw->sw.origin_y;
    }
}

static void ClassPartInitialize(WidgetClass widget_class)
{
    register DPSScrolledWindowWidgetClass wc =
	    (DPSScrolledWindowWidgetClass) widget_class;
    DPSScrolledWindowWidgetClass super =
	    (DPSScrolledWindowWidgetClass) wc->core_class.superclass;

    if (wc->sw_class.set_scale == InheritSetScale) {
	wc->sw_class.set_scale = super->sw_class.set_scale;
    }
    if (wc->sw_class.scroll_point == InheritScrollPoint) {
	wc->sw_class.scroll_point = super->sw_class.scroll_point;
    }
    if (wc->sw_class.scroll_by == InheritScrollBy) {
	wc->sw_class.scroll_by = super->sw_class.scroll_by;
    }
    if (wc->sw_class.scroll_to == InheritScrollTo) {
	wc->sw_class.scroll_to = super->sw_class.scroll_to;
    }
    if (wc->sw_class.set_scale_and_scroll == InheritSetScaleAndScroll) {
	wc->sw_class.set_scale_and_scroll =
		super->sw_class.set_scale_and_scroll;
    }
    if (wc->sw_class.convert_x_to_ps == InheritConvertXToPS) {
	wc->sw_class.convert_x_to_ps = super->sw_class.convert_x_to_ps;
    }
    if (wc->sw_class.convert_ps_to_x == InheritConvertPSToX) {
	wc->sw_class.convert_ps_to_x = super->sw_class.convert_ps_to_x;
    }
    if (wc->sw_class.add_to_dirty_area == InheritAddToDirtyArea) {
	wc->sw_class.add_to_dirty_area = super->sw_class.add_to_dirty_area;
    }
    if (wc->sw_class.take_feedback_pixmap == InheritTakeFeedbackPixmap) {
	wc->sw_class.take_feedback_pixmap =
		super->sw_class.take_feedback_pixmap;
    }
    if (wc->sw_class.give_feedback_pixmap == InheritGiveFeedbackPixmap) {
	wc->sw_class.give_feedback_pixmap =
		super->sw_class.give_feedback_pixmap;
    }
    if (wc->sw_class.start_feedback_drawing == InheritStartFeedbackDrawing) {
	wc->sw_class.start_feedback_drawing =
		super->sw_class.start_feedback_drawing;
    }
    if (wc->sw_class.end_feedback_drawing == InheritEndFeedbackDrawing) {
	wc->sw_class.end_feedback_drawing =
		super->sw_class.end_feedback_drawing;
    }
    if (wc->sw_class.set_feedback_dirty_area == InheritSetFeedbackDirtyArea) {
	wc->sw_class.set_feedback_dirty_area =
		super->sw_class.set_feedback_dirty_area;
    }
    if (wc->sw_class.finish_pending_drawing == InheritFinishPendingDrawing) {
	wc->sw_class.finish_pending_drawing =
		super->sw_class.finish_pending_drawing;
    }
    if (wc->sw_class.abort_pending_drawing == InheritAbortPendingDrawing) {
	wc->sw_class.abort_pending_drawing =
		super->sw_class.abort_pending_drawing;
    }
    if (wc->sw_class.get_drawing_info == InheritGetDrawingInfo) {
	wc->sw_class.get_drawing_info = super->sw_class.get_drawing_info;
    }
    if (wc->sw_class.update_drawing == InheritUpdateDrawing) {
	wc->sw_class.update_drawing = super->sw_class.update_drawing;
    }
    if (wc->sw_class.get_scroll_info == InheritGetScrollInfo) {
	wc->sw_class.get_scroll_info = super->sw_class.get_scroll_info;
    }
}

static void CreateChildren(DPSScrolledWindowWidget dsw)
{
    Widget w;

    w = dsw->sw.scrolled_window =
	    XtVaCreateManagedWidget("scrolledWindow",
				    xmScrolledWindowWidgetClass,
				    (Widget) dsw,
				    XtNwidth, dsw->core.width,
				    XtNheight, dsw->core.height,
				    XmNscrollingPolicy, XmAPPLICATION_DEFINED,
				    NULL);

    dsw->sw.h_scroll =
	    XtVaCreateManagedWidget("horizontalScrollBar",
				    xmScrollBarWidgetClass, w,
				    XmNorientation, XmHORIZONTAL,
				    NULL);
    XtAddCallback(dsw->sw.h_scroll, XmNvalueChangedCallback, HScrollCallback,
		  (XtPointer) dsw);
    XtAddCallback(dsw->sw.h_scroll, XmNdragCallback, HScrollCallback,
		  (XtPointer) dsw);

    dsw->sw.v_scroll =
	    XtVaCreateManagedWidget("verticalScrollBar",
				    xmScrollBarWidgetClass, w,
				    XmNorientation, XmVERTICAL,
				    NULL);
    XtAddCallback(dsw->sw.v_scroll, XmNvalueChangedCallback, VScrollCallback,
		  (XtPointer) dsw);
    XtAddCallback(dsw->sw.v_scroll, XmNdragCallback, VScrollCallback,
		  (XtPointer) dsw);


    dsw->sw.drawing_area =
	    XtVaCreateManagedWidget("drawingArea",
				    xmDrawingAreaWidgetClass, w, NULL);
    XtAddCallback(dsw->sw.drawing_area, XtNexposeCallback, DrawingAreaExpose,
		  (XtPointer) dsw);
    XtAddRawEventHandler(dsw->sw.drawing_area, 0, True,
			 DrawingAreaGraphicsExpose, (XtPointer) dsw);

    XmScrolledWindowSetAreas(w, dsw->sw.h_scroll, dsw->sw.v_scroll,
			     dsw->sw.drawing_area);
}

/* ARGSUSED */

static void Initialize(Widget request, Widget new, ArgList args, Cardinal *num_args)
{
    DPSScrolledWindowWidget dsw = (DPSScrolledWindowWidget) new;
    XGCValues gcVal;
    Bool inited;

    if (dsw->sw.area_width <= 0) dsw->sw.area_width = 8.5*72;
    if (dsw->sw.area_height <= 0) dsw->sw.area_height = 11*72;
    if (dsw->sw.scale <= 0) dsw->sw.scale = 1.0;
    dsw->sw.ctm_ptr = dsw->sw.ctm;
    dsw->sw.inv_ctm_ptr = dsw->sw.inv_ctm;
    dsw->sw.backing_pixmap = None;
    dsw->sw.feedback_pixmap = None;
    dsw->sw.window_gstate = 0;
    dsw->sw.backing_gstate = 0;
    dsw->sw.feedback_gstate = 0;
    dsw->sw.scrolling = False;
    dsw->sw.num_pending_expose = dsw->sw.pending_expose_size = 0;
    dsw->sw.pending_expose = NULL;
    dsw->sw.num_pending_dirty = dsw->sw.pending_dirty_size = 0;
    dsw->sw.pending_dirty = NULL;
    dsw->sw.num_current_drawing = dsw->sw.current_drawing_size = 0;
    dsw->sw.current_drawing = NULL;
    dsw->sw.num_prev_dirty_areas = dsw->sw.prev_dirty_areas_size = 0;
    dsw->sw.prev_dirty_areas = NULL;
    dsw->sw.drawing_stage = DSWStart;
    dsw->sw.work = 0;
    dsw->sw.big_pixmap = False;

    /* Set the initial dirty area to everything */

    dsw->sw.dirty_areas_size = 0;
    dsw->sw.dirty_areas = NULL;

    GrowRectList(&dsw->sw.dirty_areas, &dsw->sw.dirty_areas_size, 0, 1, 1);
    dsw->sw.num_dirty_areas = 1;
    LEFT(dsw->sw.dirty_areas) = 0.0;
    BOTTOM(dsw->sw.dirty_areas) = 0.0;
    WIDTH(dsw->sw.dirty_areas) = dsw->sw.area_width;
    HEIGHT(dsw->sw.dirty_areas) = dsw->sw.area_height;

    /* Make the scratch list have at least one element */

    dsw->sw.num_scratch = dsw->sw.scratch_size = 0;
    dsw->sw.scratch = NULL;
    GrowRectList(&dsw->sw.scratch, &dsw->sw.scratch_size, 0, 1, 1);

    /* Get the context */

    if (dsw->sw.context == NULL) {
	dsw->sw.context = XDPSGetSharedContext(XtDisplay(dsw));
    }

    /* Watch progress only works with pass-through event dispatching */

    if (dsw->sw.watch_progress &&
	XDPSSetEventDelivery(XtDisplay(dsw), dps_event_query) !=
		dps_event_pass_through) dsw->sw.watch_progress = False;

    if (_XDPSTestComponentInitialized(dsw->sw.context,
				      dps_init_bit_dsw, &inited) ==
	dps_status_unregistered_context) {
	XDPSRegisterContext(dsw->sw.context, False);
    }

    dsw->sw.use_saved_scroll = False;
    dsw->sw.context_inited = False;
    dsw->sw.doing_feedback = False;
    dsw->sw.feedback_displayed = False;

    CreateChildren(dsw);

    dsw->sw.ge_gc = XtGetGC(dsw->sw.drawing_area, 0, (XGCValues *) NULL);

    gcVal.graphics_exposures = False;
    dsw->sw.no_ge_gc = XtGetGC(dsw->sw.drawing_area, GCGraphicsExposures,
			       &gcVal);
}

static void Destroy(Widget widget)
{
    DPSScrolledWindowWidget dsw = (DPSScrolledWindowWidget) widget;

    if (dsw->sw.backing_pixmap != None) {
	XFreePixmap(XtDisplay(dsw), dsw->sw.backing_pixmap);
    }
    if (dsw->sw.feedback_pixmap != None) {
	XFreePixmap(XtDisplay(dsw), dsw->sw.feedback_pixmap);
    }

    if (dsw->sw.window_gstate != 0) {
	XDPSFreeContextGState(dsw->sw.context, dsw->sw.window_gstate);
    }
    if (dsw->sw.backing_gstate != 0) {
	XDPSFreeContextGState(dsw->sw.context, dsw->sw.backing_gstate);
    }
    if (dsw->sw.feedback_gstate != 0) {
	XDPSFreeContextGState(dsw->sw.context, dsw->sw.feedback_gstate);
    }

    if (dsw->sw.pending_expose != NULL) {
	XtFree((char *) dsw->sw.pending_expose);
    }
    if (dsw->sw.current_drawing != NULL) {
	XtFree((char *) dsw->sw.current_drawing);
    }
    if (dsw->sw.prev_dirty_areas != NULL) {
	XtFree((char *) dsw->sw.prev_dirty_areas);
    }
    if (dsw->sw.dirty_areas != NULL) XtFree((char *) dsw->sw.dirty_areas);
    if (dsw->sw.pending_dirty != NULL) XtFree((char *) dsw->sw.pending_dirty);
    if (dsw->sw.scratch != NULL) XtFree((char *) dsw->sw.scratch);

    XtReleaseGC(widget, dsw->sw.ge_gc);
    XtReleaseGC(widget, dsw->sw.no_ge_gc);
}

static void SetOriginAndGetTransform(DPSScrolledWindowWidget dsw)
{
    float psX, psY;

    ConvertToOrigPS(dsw, dsw->sw.origin_x, dsw->sw.origin_y, &psX, &psY);
    _DPSSWSetMatrixAndGetTransform(dsw->sw.context, psX, psY, dsw->sw.scale,
				   dsw->sw.origin_x, dsw->sw.origin_y,
				   dsw->sw.ctm, dsw->sw.inv_ctm,
				   &dsw->sw.x_offset, &dsw->sw.y_offset);
}

static void SetPixmapOrigin(DPSScrolledWindowWidget dsw)
{
    float psX, psY;

    ConvertToOrigPS(dsw, dsw->sw.origin_x, dsw->sw.origin_y, &psX, &psY);
    _DPSSWSetMatrix(dsw->sw.context, psX, psY, dsw->sw.scale,
		    dsw->sw.origin_x, dsw->sw.origin_y);
}

static void SetPixmapOffset(DPSScrolledWindowWidget dsw)
{
    int ox, oy;

    if (dsw->sw.pixmap_width <= (int) dsw->sw.drawing_area->core.width) ox = 0;
    else ox = -dsw->sw.origin_x;
    if (dsw->sw.pixmap_height <= (int) dsw->sw.drawing_area->core.height) {
	oy = dsw->sw.drawing_area->core.height;
    } else oy = dsw->sw.pixmap_height - dsw->sw.origin_y +
	    dsw->sw.drawing_area->core.height;
    
    DPSsetXoffset(dsw->sw.context, ox, oy);
}

static Boolean pixmapError;
static int (*oldHandler)(Display *, XErrorEvent *);

static int PixmapHandler(Display *dpy, XErrorEvent *error)
{
    if (error->error_code == BadAlloc &&
        error->request_code == X_CreatePixmap) {
        pixmapError = True;
        return 0;
    } else return (*oldHandler) (dpy, error);
}

static Pixmap AllocPixmap(DPSScrolledWindowWidget dsw, unsigned w, unsigned h)
{
    Pixmap p;
    unsigned int dBytes;
    Widget wid = dsw->sw.drawing_area;
    unsigned area = (w * h);

    if (dsw->sw.pixmap_limit > 0) {
	if (area > (unsigned)dsw->sw.pixmap_limit) return None;
    } else if (dsw->sw.pixmap_limit < 0
       && area > (unsigned)(dsw->sw.unscaled_width * dsw->sw.unscaled_height)
       && area > (unsigned)(wid->core.width * wid->core.height)) return None;

    if (dsw->sw.absolute_pixmap_limit > 0) {
	dBytes = (wid->core.depth + 7) / 8;	/* Convert into bytes */
	if (area * dBytes > (unsigned)dsw->sw.absolute_pixmap_limit * 1024) {
	    return None;
	}
    }

    XSync(XtDisplay(dsw), False);
    oldHandler = XSetErrorHandler(PixmapHandler);
    pixmapError = False;
    p = XCreatePixmap(XtDisplay(dsw), XtWindow(dsw->sw.drawing_area), w, h,
		      wid->core.depth);
    XSync(XtDisplay(dsw), False);
    (void) XSetErrorHandler(oldHandler);
    if (pixmapError) return None;
    else return p;
}

static void CreateBackingPixmap(DPSScrolledWindowWidget dsw)
{
    Pixmap p;

    if (dsw->sw.document_size_pixmaps) {
	dsw->sw.pixmap_width =
		MAX(CEIL(dsw->sw.drawing_width),
		    (int) dsw->sw.drawing_area->core.width);
	dsw->sw.pixmap_height =
		MAX(CEIL(dsw->sw.drawing_height),
		    (int) dsw->sw.drawing_area->core.height);

	p = dsw->sw.backing_pixmap =
		AllocPixmap(dsw, dsw->sw.pixmap_width, dsw->sw.pixmap_height);
	if (p != None) {
	    dsw->sw.big_pixmap =
		    dsw->sw.pixmap_width >
			    (int) dsw->sw.drawing_area->core.width ||
		    dsw->sw.pixmap_height >
			    (int) dsw->sw.drawing_area->core.height;
	    return;
	}
    }
    dsw->sw.big_pixmap = False;
    dsw->sw.pixmap_width = dsw->sw.drawing_area->core.width;
    dsw->sw.pixmap_height = dsw->sw.drawing_area->core.height;
    p = dsw->sw.backing_pixmap =
	    AllocPixmap(dsw, dsw->sw.pixmap_width, dsw->sw.pixmap_height);
    if (p == None) dsw->sw.pixmap_width = dsw->sw.pixmap_height = 0;
}

static void FreeBackingPixmap(DPSScrolledWindowWidget dsw)
{
    if (dsw->sw.backing_pixmap == None) return;
    XFreePixmap(XtDisplay(dsw), dsw->sw.backing_pixmap);
    dsw->sw.backing_pixmap = None;
    dsw->sw.big_pixmap = False;
    dsw->sw.pixmap_width = dsw->sw.pixmap_height = 0;
    XDPSFreeContextGState(dsw->sw.context, dsw->sw.backing_gstate);
}    

static void SetDrawingAreaPosition(
    DPSScrolledWindowWidget dsw,
    float ix,
    float iy,
    int vx,
    int vy,
    Boolean setOrigin)
{
    int xoff, yoff;
    int hSize, vSize;
    float scrollX, scrollY;

    /* Convert ix, iy into X units */

    ix *= dsw->sw.drawing_width / dsw->sw.area_width;
    iy *= dsw->sw.drawing_height / dsw->sw.area_height;

    if ((int)dsw->sw.drawing_area->core.width >= CEIL(dsw->sw.drawing_width)) {
	/* The scaled width is narrower than the view window, so
	   center the picture and set scroll bar to be unscrollable */

	xoff = ((int) dsw->sw.drawing_area->core.width -
		CEIL(dsw->sw.drawing_width))
		/ 2.0;
	scrollX = 0;
	hSize = CEIL(dsw->sw.drawing_width);
    } else {
	/* The scaled width is larger than the view window, so
	   turn on the scroll bar, and set up its maximum and
	   slider size.  Do this by converting the image offset into X
	   coordinates and subtracting the view offset */

	scrollX = ix - vx;
	scrollX = MAX(scrollX, 0);
	scrollX = MIN(scrollX, CEIL(dsw->sw.drawing_width) -
		               (int) dsw->sw.drawing_area->core.width);
	hSize = dsw->sw.drawing_area->core.width;
	xoff = -(int) (scrollX + 0.5);
    }

    /* Now do the same thing for the height.  We want to compute the offset
       relative to the lower left corner, but X coordinates are relative
       to the upper left, so the drawing height must be added in.  Also, since
       the coordinates go in the other direction, the view offset must be
       added, not subtracted. */

    if ((int) dsw->sw.drawing_area->core.height >=
	CEIL(dsw->sw.drawing_height)) {
	yoff = ((int) dsw->sw.drawing_area->core.height -
		CEIL(dsw->sw.drawing_height)) / 2.0;
	scrollY = CEIL(dsw->sw.drawing_height) -
		(int) dsw->sw.drawing_area->core.height;
	vSize = CEIL(dsw->sw.drawing_height);
    } else {
	scrollY = iy + vy - (int) dsw->sw.drawing_area->core.height;
	scrollY = MAX(scrollY, 0);
	scrollY = MIN(scrollY, CEIL(dsw->sw.drawing_height) -
		               (int) dsw->sw.drawing_area->core.height);
	vSize = dsw->sw.drawing_area->core.height;
	yoff = -(int) (scrollY + 0.5);
    }

    /* Update the scrollbars */
    dsw->sw.scroll_x = (int) (scrollX + 0.5); 
    dsw->sw.scroll_y = (int) (CEIL(dsw->sw.drawing_height) -
			      (int) dsw->sw.drawing_area->core.height -
			      scrollY + 0.5);

    yoff = dsw->sw.drawing_area->core.height - yoff;

    dsw->sw.scroll_h_value = dsw->sw.scroll_x;
    dsw->sw.scroll_h_size = hSize;
    dsw->sw.scroll_h_max = CEIL(dsw->sw.drawing_width);
    dsw->sw.scroll_v_value = dsw->sw.scroll_y;
    dsw->sw.scroll_v_size = vSize;
    dsw->sw.scroll_v_max = CEIL(dsw->sw.drawing_height);

    if (!dsw->sw.application_scrolling) {
	XtVaSetValues(dsw->sw.h_scroll, XmNmaximum, dsw->sw.scroll_h_max,
		      XmNvalue, dsw->sw.scroll_x, XmNsliderSize, hSize, NULL);
	XtVaSetValues(dsw->sw.v_scroll, XmNmaximum, dsw->sw.scroll_v_max,
		      XmNvalue, dsw->sw.scroll_y, XmNsliderSize, vSize, NULL);
    }
    
    if (setOrigin) {
	/* Set the origin in the X window to reflect the new location */
	dsw->sw.origin_x = xoff;
	dsw->sw.origin_y = yoff;
    }
}

static void DrawBackground(
    DPSScrolledWindowWidget dsw,
    DSWDrawableType which)
{
    DSWExposeCallbackRec e;

    e.type = which;
    e.directions = DSWFinish;
    e.results = DSWUndefined;
    e.first = True;
    e.background = True;
    if (which == DSWBackingPixmap) {
	e.drawable = dsw->sw.backing_pixmap;
	e.gstate = dsw->sw.backing_gstate;
    } else if (which == DSWFeedbackPixmap) {
	e.drawable = dsw->sw.feedback_pixmap;
	e.gstate = dsw->sw.feedback_gstate;
    } else {
	e.drawable = XtWindow(dsw->sw.drawing_area);
	e.gstate = dsw->sw.window_gstate;
    }
    e.context = dsw->sw.context;

    SimplifyRects(dsw->sw.current_drawing, &dsw->sw.num_current_drawing);

    XDPSSetContextGState(dsw->sw.context, e.gstate);
    _DPSSWSetRectViewClip(dsw->sw.context, dsw->sw.current_drawing,
				  dsw->sw.num_current_drawing * 4);
    
    e.rects = dsw->sw.current_drawing;
    e.rect_count = dsw->sw.num_current_drawing;

    do {
	XtCallCallbackList((Widget) dsw, dsw->sw.background_callback,
			   (XtPointer) &e);
	if (e.results == DSWUndefined) {
	    if (XtHasCallbacks((Widget) dsw, XtNbackgroundCallback) !=
		XtCallbackHasNone) {
		XtAppWarningMsg(XtWidgetToApplicationContext((Widget) dsw),
				"returnError", "backgroundCallback",
				"DPSScrollError",
				"Background callback did not set result field",
				(String *) NULL, (Cardinal *) NULL);
	    }
	    e.results = DSWFinished;
	}
    } while (e.results != DSWFinished);


    DPSinitviewclip(dsw->sw.context);
}

static void ClipToDrawingSize(
    DPSScrolledWindowWidget dsw,
    DSWDrawableType which)
{
    int i;
    float r[4];

    if (CEIL(dsw->sw.drawing_width) >= (int) dsw->sw.drawing_area->core.width &&
	CEIL(dsw->sw.drawing_height) >= (int) dsw->sw.drawing_area->core.height) return;

    /* Copy current drawing area to scratch */

    GrowRectList(&dsw->sw.scratch, &dsw->sw.scratch_size, 0,
		 dsw->sw.num_current_drawing, 1);
    dsw->sw.num_scratch = dsw->sw.num_current_drawing;
    for (i = 0; i < dsw->sw.num_current_drawing * 4; i++) {
	dsw->sw.scratch[i] = dsw->sw.current_drawing[i];
    }

    /* Construct a rectangle of the drawing area */

    ConvertToPS(dsw, dsw->sw.origin_x + DELTA, dsw->sw.origin_y - DELTA,
		r, r+1);
    ConvertToPS(dsw, dsw->sw.origin_x + CEIL(dsw->sw.drawing_width) - DELTA,
		dsw->sw.origin_y - CEIL(dsw->sw.drawing_height) + DELTA,
		r+2, r+3);
    r[2] -= r[0];
    r[3] -= r[1];

    /* Subtract the area of the drawing from the current drawing list */

    SubtractRects(&dsw->sw.current_drawing, &dsw->sw.current_drawing_size,
		  &dsw->sw.num_current_drawing, r, 1);

    if (dsw->sw.num_current_drawing != 0) {
	DrawBackground(dsw, which);
	/* Now intersect the rectangle with the current drawing area */
	IntersectRects(&dsw->sw.scratch, &dsw->sw.scratch_size,
		  &dsw->sw.num_scratch, r, 1);
	/* If nothing left, we won't be drawing anything more, so
	   synchronize.  Otherwise wait until we're done drawing */
	if (dsw->sw.num_scratch == 0) DPSWaitContext(dsw->sw.context);
    }

    /* Copy scratch back into the current drawing list */
    GrowRectList(&dsw->sw.current_drawing, &dsw->sw.current_drawing_size, 0,
		 dsw->sw.num_scratch, 1);
    dsw->sw.num_current_drawing = dsw->sw.num_scratch;
    for (i = 0; i < dsw->sw.num_scratch * 4; i++) {
	dsw->sw.current_drawing[i] = dsw->sw.scratch[i];
    }
}    

static DSWResults ClipAndDraw(
    DPSScrolledWindowWidget dsw,
    DSWDrawableType which,
    DSWDirections howMuch,
    Boolean first)
{
    DSWExposeCallbackRec e;

    e.type = which;
    e.directions = howMuch;
    e.results = DSWUndefined;
    e.first = first;
    e.background = False;
    if (which == DSWBackingPixmap) {
	e.drawable = dsw->sw.backing_pixmap;
	e.gstate = dsw->sw.backing_gstate;
    } else if (which == DSWFeedbackPixmap) {
	e.drawable = dsw->sw.feedback_pixmap;
	e.gstate = dsw->sw.feedback_gstate;
    } else {
	e.drawable = XtWindow(dsw->sw.drawing_area);
	e.gstate = dsw->sw.window_gstate;
    }
    e.context = dsw->sw.context;

    if (first) {
	XDPSSetContextGState(dsw->sw.context, e.gstate);
	if (howMuch != DSWAbort) {
	    ClipToDrawingSize(dsw, which);
	    SimplifyRects(dsw->sw.current_drawing,
			  &dsw->sw.num_current_drawing);
	    if (dsw->sw.num_current_drawing == 0) return DSWFinished;
	    _DPSSWSetRectViewClip(dsw->sw.context, dsw->sw.current_drawing,
				  dsw->sw.num_current_drawing * 4);
	}
    }
    
    e.rects = dsw->sw.current_drawing;
    e.rect_count = dsw->sw.num_current_drawing;

    do {
	XtCallCallbackList((Widget) dsw, dsw->sw.expose_callback,
			   (XtPointer) &e);
	if (e.results == DSWUndefined) {
	    if (XtHasCallbacks((Widget) dsw,
			       XtNexposeCallback) != XtCallbackHasNone) {
		XtAppWarningMsg(XtWidgetToApplicationContext((Widget) dsw),
				"returnError", "exposeCallback",
				"DPSScrollError",
				"Expose callback did not set result field",
				(String *) NULL, (Cardinal *) NULL);
	    }
	    e.results = DSWFinished;
	}
    } while ((e.results != DSWFinished && howMuch == DSWFinish) ||
	     (e.results != DSWFinished && e.results != DSWAborted &&
	      howMuch == DSWAbortOrFinish) ||
	     (e.results != DSWAborted && howMuch == DSWAbort));

    if (e.results == DSWFinished) {
	DPSinitviewclip(dsw->sw.context);
	DPSWaitContext(dsw->sw.context);
    }
    return e.results;
}

static void SplitExposeEvent(
    DPSScrolledWindowWidget dsw,
    XExposeEvent *ev)
{
    float *r;
    float llx, lly, urx, ury;
    int xr[4];
    int i;
    int dx, dy;

    ComputeOffsets(dsw, &dx, &dy);

    /* Put the expose event into the scratch list */
    dsw->sw.num_scratch = 1;
    r = dsw->sw.scratch;
    ConvertToPS(dsw, ev->x + DELTA, ev->y + ev->height - DELTA, &llx, &lly);
    ConvertToPS(dsw, ev->x + ev->width - DELTA, ev->y + DELTA, &urx, &ury);
    LEFT(r) = llx;
    BOTTOM(r) = lly;
    WIDTH(r) = urx - llx;
    HEIGHT(r) = ury - lly;

    /* Subtract the dirty area from the exposed area and copy the resulting
       area to the window */
    SubtractRects(&dsw->sw.scratch, &dsw->sw.scratch_size,
		  &dsw->sw.num_scratch,
		  dsw->sw.dirty_areas, dsw->sw.num_dirty_areas);
    for (i = 0; i < dsw->sw.num_scratch; i++) {
	r = dsw->sw.scratch + 4*i;
	ConvertToX(dsw, LEFT(r), TOP(r), xr, xr+1);
	ConvertToX(dsw, RIGHT(r), BOTTOM(r), xr+2, xr+3);
	xr[2] -= xr[0];
	xr[3] -= xr[1];
	XCopyArea(XtDisplay(dsw), dsw->sw.backing_pixmap,
		  XtWindow(dsw->sw.drawing_area), dsw->sw.no_ge_gc,
		  xr[0] + dx, xr[1] + dy, xr[2], xr[3], xr[0], xr[1]);
    }

    /* Now do it again, but intersect the exposed area with the dirty area
       and add the intersection to the pending list */

    dsw->sw.num_scratch = 1;
    r = dsw->sw.scratch;
    LEFT(r) = llx;
    BOTTOM(r) = lly;
    WIDTH(r) = urx - llx;
    HEIGHT(r) = ury - lly;
    IntersectRects(&dsw->sw.scratch, &dsw->sw.scratch_size,
		   &dsw->sw.num_scratch,
		   dsw->sw.dirty_areas, dsw->sw.num_dirty_areas);
    AddUserSpaceRectsToPending(dsw, dsw->sw.scratch, dsw->sw.num_scratch);
}

/* ARGSUSED */

static Bool CheckWatchProgressEvent(
    Display *dpy,
    XEvent *e,
    char *arg)
{
    DPSScrolledWindowWidget dsw = (DPSScrolledWindowWidget) arg;
    
    return (e->xany.window == dsw->sw.backing_pixmap &&
	    (e->type == GraphicsExpose || e->type == NoExpose)) ||
	   (e->xany.window == XtWindow(dsw->sw.drawing_area) &&
	    e->type == Expose);
}

static void CopyWindowToBackingPixmap(
    DPSScrolledWindowWidget dsw)
{
    int llx, lly, urx, ury;
    XEvent e;
    XExposeEvent *ev = (XExposeEvent *) &e;
    int i;
    float *r;
    int copies = 0;
    int dx, dy;

    ComputeOffsets(dsw, &dx, &dy);

    for (i = 0; i < dsw->sw.num_dirty_areas; i++) {
	r = dsw->sw.dirty_areas + i*4;
	ConvertToX(dsw, LEFT(r), BOTTOM(r), &llx, &lly);
	ConvertToX(dsw, RIGHT(r), TOP(r), &urx, &ury);
	XCopyArea(XtDisplay(dsw), XtWindow(dsw->sw.drawing_area),
		  dsw->sw.backing_pixmap, dsw->sw.ge_gc,
		  llx, ury, urx-llx, lly-ury, llx + dx, ury + dy);
	copies++;
    }

    /* Unfortunately the Intrinsics won't let us get ahold of the the
       GraphicsExpose events for the pixmap, so we have to wait and
       get them the old fashioned way.  Yuck. */

    while (copies > 0) {
	XIfEvent(XtDisplay(dsw), &e, CheckWatchProgressEvent,
		 (char *) dsw);
	if (e.type == Expose) {
	    SplitExposeEvent(dsw, ev);
	    continue;
	} else if (e.type == GraphicsExpose) {
	    ev->x -= dx;
	    ev->y -= dy;
	    AddExposureToPending(dsw, ev);
	    if (ev->count == 0) copies--;
	} else copies--;		    /* NoExpose */
    }
    CopyRectsToCurrentDrawing(dsw, dsw->sw.pending_dirty,
			     dsw->sw.num_pending_dirty);

    dsw->sw.num_pending_dirty = 0;
    dsw->sw.num_pending_expose = 0;
    if (dsw->sw.num_current_drawing == 0) {
	dsw->sw.drawing_stage = DSWDone;
	dsw->sw.num_dirty_areas = 0;
    } else {
	/* The dirty area is now the intersection of the old dirty area and
	   the newly-created current drawing list */
	IntersectRects(&dsw->sw.dirty_areas, &dsw->sw.dirty_areas_size,
		       &dsw->sw.num_dirty_areas,
		       dsw->sw.current_drawing, dsw->sw.num_current_drawing);
    }	
}

/* Subtract the window area from the dirty area, and make the
   result be the new current drawing list */

static void SetCurrentDrawingToBackground(
    DPSScrolledWindowWidget dsw)
{
    int i;
    float r[4];

    ConvertToPS(dsw, 0 + DELTA, dsw->sw.drawing_area->core.height - DELTA,
		r, r+1);
    ConvertToPS(dsw, dsw->sw.drawing_area->core.width - DELTA, 0 + DELTA,
		r+2, r+3);
    r[2] -= r[0];
    r[3] -= r[1];

    SubtractRects(&dsw->sw.dirty_areas, &dsw->sw.dirty_areas_size,
		  &dsw->sw.num_dirty_areas, r, 1);

    GrowRectList(&dsw->sw.current_drawing, &dsw->sw.current_drawing_size,
		 0, dsw->sw.num_dirty_areas, 1);
    for (i = 0; i < 4 * dsw->sw.num_dirty_areas; i++) {
	dsw->sw.current_drawing[i] = dsw->sw.dirty_areas[i];
    }
    dsw->sw.num_current_drawing = dsw->sw.num_dirty_areas;
}

static void CopyPendingExpose(DPSScrolledWindowWidget dsw)
{
    int dx, dy;
    int i;
    int *r;

    ComputeOffsets(dsw, &dx, &dy);

    for (i = 0; i < dsw->sw.num_pending_expose; i++) {
	r = dsw->sw.pending_expose + 4*i;
	XCopyArea(XtDisplay(dsw), dsw->sw.backing_pixmap,
		  XtWindow(dsw->sw.drawing_area),
		  dsw->sw.no_ge_gc,
		  LEFT(r) + dx, BOTTOM(r) + dy, WIDTH(r), HEIGHT(r),
		  LEFT(r), BOTTOM(r));
    }
    dsw->sw.num_pending_expose = dsw->sw.num_pending_dirty = 0;
}

static void UpdateWindowFromBackingPixmap(
    DPSScrolledWindowWidget dsw,
    float *rects,
    int n)
{
    int dx, dy;
    int llx, lly, urx, ury;
    int i;
    float *r;

    ComputeOffsets(dsw, &dx, &dy);

    for (i = 0; i < n; i++) {
	r = rects + 4*i;
	ConvertToX(dsw, LEFT(r), BOTTOM(r), &llx, &lly);
	ConvertToX(dsw, RIGHT(r), TOP(r), &urx, &ury);

	XCopyArea(XtDisplay(dsw), dsw->sw.backing_pixmap,
		  XtWindow(dsw->sw.drawing_area),
		  dsw->sw.no_ge_gc,
		  llx+dx-1, ury+dy-1, urx-llx+2, lly-ury+2, llx-1, ury-1);
    }
}

static void UpdateWindowFromFeedbackPixmap(
    DPSScrolledWindowWidget dsw,
    float *rects,
    int n)
{
    int llx, lly, urx, ury;
    int i;
    float *r;

    for (i = 0; i < n; i++) {
	r = rects + (i * 4);
	ConvertToX(dsw, LEFT(r), BOTTOM(r), &llx, &lly);
	ConvertToX(dsw, RIGHT(r), TOP(r), &urx, &ury);

	XCopyArea(XtDisplay(dsw), dsw->sw.feedback_pixmap,
		  XtWindow(dsw->sw.drawing_area), dsw->sw.no_ge_gc,
		  llx-1, ury-1, urx-llx+2, lly-ury+2, llx-1, ury-1);
    }
}    

/* This is the heart of the drawing code; it does one piece of drawing.
   It can be called either directly or as a work procedure */

static Boolean DoDrawing(XtPointer clientData)
{
    DPSScrolledWindowWidget dsw = (DPSScrolledWindowWidget) clientData;
    DSWResults results;
    DSWDrawableType which;

    if (dsw->sw.drawing_stage == DSWStart && dsw->sw.watch_progress &&
	dsw->sw.backing_pixmap != None && dsw->sw.num_current_drawing == 0) {
	dsw->sw.drawing_stage = DSWDrewVisible;
	CopyWindowToBackingPixmap(dsw);
    }

    switch (dsw->sw.drawing_stage) {
	case DSWStart:
	case DSWDrawingVisible:
	    if (dsw->sw.watch_progress || dsw->sw.backing_pixmap == None) {
		which = DSWWindow;
	    } else which = DSWBackingPixmap;
	    results = ClipAndDraw(dsw, which, DSWDrawSome,
				  (dsw->sw.drawing_stage == DSWStart));
	    if (results == DSWFinished) {
		if (dsw->sw.watch_progress && dsw->sw.backing_pixmap != None) {
		    dsw->sw.drawing_stage = DSWDrewVisible;
		    CopyWindowToBackingPixmap(dsw);
		} else {
		    if (dsw->sw.minimal_drawing && dsw->sw.big_pixmap) {
			dsw->sw.drawing_stage = DSWDrewVisible;
			SetCurrentDrawingToBackground(dsw);
		    } else {
			dsw->sw.drawing_stage = DSWDone;
			dsw->sw.num_dirty_areas = 0;
		    }
		    if (dsw->sw.num_pending_expose != 0 &&
			dsw->sw.backing_pixmap != None) {
			CopyPendingExpose(dsw);
		    }
		}
	    } else dsw->sw.drawing_stage = DSWDrawingVisible;
	    break;

	case DSWDrewVisible:
	case DSWDrawingBackground:
	    results = ClipAndDraw(dsw, DSWBackingPixmap, DSWDrawSome,
				  (dsw->sw.drawing_stage == DSWDrewVisible));
	    if (results == DSWFinished) {
		dsw->sw.drawing_stage = DSWDone;
		dsw->sw.num_dirty_areas = 0;
	    }
	    else dsw->sw.drawing_stage = DSWDrawingBackground;
	    break;

	case DSWDone:
	    break;
    }

    if (dsw->sw.drawing_stage == DSWDone && dsw->sw.num_pending_dirty != 0) {
	CopyRectsToCurrentDrawing(dsw, dsw->sw.pending_dirty,
				  dsw->sw.num_pending_dirty);
	CopyRectsToDirtyArea(dsw, dsw->sw.pending_dirty,
			     dsw->sw.num_pending_dirty);
	dsw->sw.num_pending_dirty = 0;
	dsw->sw.num_pending_expose = 0;
	dsw->sw.drawing_stage = DSWStart;
    }
	
    if (dsw->sw.drawing_stage == DSWDone) {
	dsw->sw.work = 0;
	if (dsw->sw.watch_progress) {
	    /* Some of the background drawing may have been to areas that
	       are visible */
	    UpdateWindowFromBackingPixmap(dsw, dsw->sw.current_drawing,
					  dsw->sw.num_current_drawing);
	}
	dsw->sw.num_current_drawing = 0;
	if (dsw->sw.scrolling) {
	    dsw->sw.scrolling = False;
	    ScrollMoved(dsw);
	}
	return True;
    } else return False;
}

static void StartDrawing(DPSScrolledWindowWidget dsw)
{
    float r[4];

    CopyRectsToCurrentDrawing(dsw, dsw->sw.dirty_areas,
			      dsw->sw.num_dirty_areas);

    if (dsw->sw.watch_progress || dsw->sw.backing_pixmap == None) {
	/* Intersect the current drawing area with the pending dirty area 
	   (The pending dirty area represents the window exposures that
	   have happened so far) */
	IntersectRects(&dsw->sw.current_drawing, &dsw->sw.current_drawing_size,
		       &dsw->sw.num_current_drawing,
		       dsw->sw.pending_dirty, dsw->sw.num_pending_dirty);
	dsw->sw.num_pending_dirty = dsw->sw.num_pending_expose = 0;
    } else {
	if (!dsw->sw.big_pixmap || dsw->sw.minimal_drawing) {
	    /* Intersect the current drawing area to the window to start */
	    ConvertToPS(dsw, 0 + DELTA,
			dsw->sw.drawing_area->core.height - DELTA, r, r+1);
	    ConvertToPS(dsw, dsw->sw.drawing_area->core.width - DELTA,
			0 + DELTA, r+2, r+3);
	    r[2] -= r[0];
	    r[3] -= r[1];
	    IntersectRects(&dsw->sw.current_drawing,
			   &dsw->sw.current_drawing_size,
			   &dsw->sw.num_current_drawing, r, 1);
	}
    }

    if (dsw->sw.num_current_drawing == 0 && !dsw->sw.watch_progress) {
	dsw->sw.drawing_stage = DSWFinished;
	dsw->sw.num_dirty_areas = 0;
	return;
    }

    dsw->sw.drawing_stage = DSWStart;
    if (!DoDrawing((XtPointer) dsw)) {
	dsw->sw.work =
		XtAppAddWorkProc(XtWidgetToApplicationContext((Widget) dsw),
				 DoDrawing, (XtPointer) dsw);
    }
}

static void RedisplaySliver(DPSScrolledWindowWidget dsw, int deltaX, int deltaY)
{
    float r[8];
    int xr[8];
    int n;
    int xllx, xlly, xurx, xury;
    float llx, lly, urx, ury;

    /* If one of the deltas is 0, then the area to update is just a
       single rectangle. */
    if (deltaX == 0 || deltaY == 0) {
	if (deltaX == 0) {
	    /* Just a single horizontal rectangle */

	    xllx = 0;
	    xurx = dsw->sw.drawing_area->core.width;
	    if (deltaY > 0) {
		xlly = dsw->sw.drawing_area->core.height;
		xury = dsw->sw.drawing_area->core.height - deltaY;
	    } else {
		xlly = -deltaY;
		xury = 0;
	    }

	} else if (deltaY == 0) {
	    /* Just a single vertical rectangle */
	    xlly = dsw->sw.drawing_area->core.height;
	    xury = 0;
	    if (deltaX > 0) {
		xllx = dsw->sw.drawing_area->core.width - deltaX;
		xurx = dsw->sw.drawing_area->core.width;
	    } else {
		xllx = 0;
		xurx = -deltaX;
	    }
	}
	/* Convert the rectangle into PS coordinates */
	ConvertToPS(dsw, xllx + DELTA, xlly - DELTA, &llx, &lly);
	ConvertToPS(dsw, xurx - DELTA, xury + DELTA, &urx, &ury);
	r[0] = llx;
	r[1] = lly;
	r[2] = urx - llx;
	r[3] = ury - lly;
	xr[0] = xllx;
	xr[1] = xury;
	xr[2] = xurx - xllx;
	xr[3] = xlly - xury;
	n = 1;

    } else {
	/* Scrolling in both directions, so there are two rectangles.
	   It's easiest to do if we let them overlap; fortunately that is
	   legal!  First do the horizontal rectangle. */
	xllx = 0;
	xurx = dsw->sw.drawing_area->core.width;
	if (deltaY > 0) {
	    xlly = dsw->sw.drawing_area->core.height;
	    xury = dsw->sw.drawing_area->core.height - deltaY;
	} else {
	    xlly = -deltaY;
	    xury = 0;
	}
	ConvertToPS(dsw, xllx + DELTA, xlly - DELTA, &llx, &lly);
	ConvertToPS(dsw, xurx - DELTA, xury + DELTA, &urx, &ury);

	/* Store in point array */
	r[0] = llx;
	r[1] = lly;
	r[2] = urx - llx;
	r[3] = ury - lly;
	xr[0] = xllx;
	xr[1] = xury;
	xr[2] = xurx - xllx;
	xr[3] = xlly - xury;

	/* Now do vertical rectangle and store in point array */
	xlly = dsw->sw.drawing_area->core.height;
	xury = 0;
	if (deltaX > 0) {
	    xllx = dsw->sw.drawing_area->core.width - deltaX;
	    xurx = dsw->sw.drawing_area->core.width;
	} else {
	    xllx = 0;
	    xurx = -deltaX;
	}
	ConvertToPS(dsw, xllx + DELTA, xlly - DELTA, &llx, &lly);
	ConvertToPS(dsw, xurx - DELTA, xury + DELTA, &urx, &ury);
	r[4] = llx;
	r[5] = lly;
	r[6] = urx - llx;
	r[7] = ury - lly;
	xr[4] = xllx;
	xr[5] = xury;
	xr[6] = xurx - xllx;
	xr[7] = xlly - xury;
	n = 2;
    }

    AddRectsToDirtyArea(dsw, r, n);
    AddRectsToPending(dsw, xr, n);
    StartDrawing(dsw);
}

static void SetUpInitialPixmap(DPSScrolledWindowWidget dsw)
{
    float *r = dsw->sw.dirty_areas;
    int llx, lly, urx, ury;

    CreateBackingPixmap(dsw);
    if (dsw->sw.backing_pixmap != None) {
	XDPSSetContextDrawable(dsw->sw.context, dsw->sw.backing_pixmap,
			       dsw->sw.pixmap_height);

	SetPixmapOffset(dsw);
	SetPixmapOrigin(dsw);
	XDPSCaptureContextGState(dsw->sw.context, &dsw->sw.backing_gstate);

	if (dsw->sw.pixmap_width != CEIL(dsw->sw.drawing_width) ||
	    dsw->sw.pixmap_height != CEIL(dsw->sw.drawing_height)) {

	    /* Make the dirty area match the window */
	    if (dsw->sw.pixmap_width > (int)dsw->sw.drawing_area->core.width) {
		llx = dsw->sw.origin_x;
		urx = llx + CEIL(dsw->sw.drawing_width);
	    } else {
		llx = 0;
		urx = dsw->sw.drawing_area->core.width;
	    }
	    if (dsw->sw.pixmap_height >
		(int) dsw->sw.drawing_area->core.height) {
		lly = dsw->sw.origin_y;
		ury = dsw->sw.origin_y - CEIL(dsw->sw.drawing_height);
	    } else {
		lly = dsw->sw.drawing_area->core.height;
		ury = 0;
	    }
	    ConvertToPS(dsw, llx + DELTA, lly - DELTA, r, r+1);
	    ConvertToPS(dsw, urx - DELTA, ury + DELTA, r+2, r+3);
	    r[2] -= r[0];
	    r[3] -= r[1];
	    dsw->sw.num_dirty_areas = 1;
	}
	if (dsw->sw.doing_feedback) {
	    CopyRectsToCurrentDrawing(dsw, dsw->sw.dirty_areas,
				      dsw->sw.num_dirty_areas);
	    FinishDrawing(dsw);
	} else if (!dsw->sw.watch_progress) StartDrawing(dsw);
    }
}

/* ARGSUSED */

static void TimerStart(XtPointer clientData, XtIntervalId *id)
{
    DPSScrolledWindowWidget dsw = (DPSScrolledWindowWidget) clientData;

    if (dsw->sw.drawing_stage == DSWStart) StartDrawing(dsw);
}

static void SetUpInitialInformation(DPSScrolledWindowWidget dsw)
{
    int i;
    float llx, lly, urx, ury;
    float xScale, yScale;
    XExposeEvent ev;
    XStandardColormap colorCube, grayRamp;
    int match;

    /* If the context's colormap matches the widget's colormap, assume that
       everything is already set up right in the color cube department.  This
       allows an application to supply us with a custom color cube by
       installing it in the context before calling us */

    _DPSSWColormapMatch(dsw->sw.context,
			dsw->sw.drawing_area->core.colormap, &match);

    if (match) {
	XDPSSetContextParameters(dsw->sw.context, XtScreen(dsw),
				 dsw->sw.drawing_area->core.depth,
				 XtWindow(dsw->sw.drawing_area),
				 dsw->sw.drawing_area->core.height, NULL, NULL,
				 XDPSContextScreenDepth | XDPSContextDrawable);
    } else {
	grayRamp.colormap = colorCube.colormap =
		dsw->sw.drawing_area->core.colormap;

	XDPSCreateStandardColormaps(XtDisplay(dsw), XtWindow(dsw),
				    (Visual *) NULL, 0, 0, 0, 0,
				    &colorCube, &grayRamp, False);

	XDPSSetContextParameters(dsw->sw.context, XtScreen(dsw),
				 dsw->sw.drawing_area->core.depth,
				 XtWindow(dsw->sw.drawing_area),
				 dsw->sw.drawing_area->core.height,
				 (XDPSStandardColormap *) &colorCube,
				 (XDPSStandardColormap *) &grayRamp,
				 XDPSContextScreenDepth | XDPSContextDrawable |
				 XDPSContextRGBMap | XDPSContextGrayMap);
    }

    DPSinitgraphics(dsw->sw.context);
    _DPSSWGetTransform(dsw->sw.context, dsw->sw.ctm, dsw->sw.orig_inv_ctm);
    dsw->sw.x_offset = 0;
    dsw->sw.y_offset = dsw->sw.drawing_area->core.height;
    for (i = 0; i < 6; i++) dsw->sw.inv_ctm[i] = dsw->sw.orig_inv_ctm[i];

    ConvertToPS(dsw, 0.0, 100.0, &llx, &lly);
    ConvertToPS(dsw, 100.0, 0.0, &urx, &ury);
    xScale = ABS(100.0 / (urx - llx));
    yScale = ABS(100.0 / (ury - lly));

    if (dsw->sw.scale != 1.0) {
	DPSscale(dsw->sw.context, dsw->sw.scale, dsw->sw.scale);
	_DPSSWGetTransform(dsw->sw.context, dsw->sw.ctm, dsw->sw.inv_ctm);
    }
	
    dsw->sw.drawing_width = dsw->sw.area_width * xScale * dsw->sw.scale;
    dsw->sw.drawing_height = dsw->sw.area_height * yScale * dsw->sw.scale;

    dsw->sw.unscaled_width = dsw->sw.drawing_width / dsw->sw.scale + 1;
    dsw->sw.unscaled_height = dsw->sw.drawing_height / dsw->sw.scale + 1;

    if (!dsw->sw.use_saved_scroll) {
	dsw->sw.scroll_pic_x = dsw->sw.area_width / 2;
	dsw->sw.scroll_pic_y = dsw->sw.area_height / 2;
	dsw->sw.scroll_win_x = dsw->sw.drawing_area->core.width / 2;
	dsw->sw.scroll_win_y = dsw->sw.drawing_area->core.height / 2;
    }

    SetDrawingAreaPosition(dsw, dsw->sw.scroll_pic_x, dsw->sw.scroll_pic_y,
			   dsw->sw.scroll_win_x, dsw->sw.scroll_win_y, True);
    SetOriginAndGetTransform(dsw);
    XDPSCaptureContextGState(dsw->sw.context, &dsw->sw.window_gstate);

    dsw->sw.drawing_stage = DSWStart;

    if (dsw->sw.use_backing_pixmap) SetUpInitialPixmap(dsw);

    if (dsw->sw.doing_feedback) return;

    if (dsw->sw.watch_progress || dsw->sw.backing_pixmap == None) {
	/* If watching progress or no pixmap, clear the window to ensure
	   that things get started */
	XClearArea(XtDisplay(dsw), XtWindow(dsw->sw.drawing_area),
		   0, 0, 0, 0, True);
	XSync(XtDisplay(dsw), False);
	if (dsw->sw.watch_progress && dsw->sw.watch_progress_delay > 0) {
	    /* Set a timer so that we start drawing if nothing is visible */
	    (void) XtAppAddTimeOut(XtWidgetToApplicationContext((Widget) dsw),
				   dsw->sw.watch_progress_delay,
				   TimerStart, (XtPointer) dsw);
	}
    } else {
	/* Put a synthetic expose event in the pending queue */
	ev.x = ev.y = 0;
	ev.width = dsw->sw.drawing_area->core.width;
	ev.height = dsw->sw.drawing_area->core.height;
	SplitExposeEvent(dsw, &ev);
    }
}

static void Realize(
    Widget w,
    XtValueMask *mask,
    XSetWindowAttributes *attr)
{
    DPSScrolledWindowWidget dsw = (DPSScrolledWindowWidget) w;
    DSWSetupCallbackRec setup;

    /* Let my parent do all the hard work */
    (*dpsScrolledWindowClassRec.core_class.superclass->core_class.realize)
	    (w, mask, attr);

    /* We delay calling the setup callback so that the caller can use
       XtAddCallback to add it */
    setup.context = dsw->sw.context;
    XtCallCallbackList((Widget) dsw, dsw->sw.setup_callback,
		       (XtPointer) &setup);

    /* Now, explicitly realize my children */
    XtRealizeWidget(dsw->sw.scrolled_window);

    /* Et voila, now we have windows! */
    SetUpInitialInformation(dsw);
}

static void AbortOrFinish(DPSScrolledWindowWidget dsw)
{
    DSWResults results;
    DSWDrawableType which;
    
    if (dsw->sw.work != 0) {
	XtRemoveWorkProc(dsw->sw.work);
	dsw->sw.work = 0;
    }

    switch (dsw->sw.drawing_stage) {
	case DSWStart:
	    return;
	    /* break; */

	case DSWDrawingVisible:
	    if (dsw->sw.watch_progress || dsw->sw.backing_pixmap == None) {
		which = DSWWindow;
	    } else which = DSWBackingPixmap;
	    results = ClipAndDraw(dsw, which, DSWAbortOrFinish, False);
	    if (results == DSWAborted) {
		dsw->sw.drawing_stage = DSWStart;
		if (dsw->sw.backing_pixmap == None || dsw->sw.watch_progress) {
		    /* Add the current drawing area back into the pending
		       expose area */
		    AddUserSpaceRectsToPending(dsw, dsw->sw.current_drawing,
					       dsw->sw.num_current_drawing);
		}
		return;
	    }
	    if (dsw->sw.watch_progress && dsw->sw.backing_pixmap != None) {
		dsw->sw.drawing_stage = DSWDrewVisible;
		CopyWindowToBackingPixmap(dsw);
		if (dsw->sw.num_current_drawing != 0) {
		    results = ClipAndDraw(dsw, DSWBackingPixmap,
					  DSWAbortOrFinish, True);
		}
		if (results == DSWAborted) {
		    dsw->sw.drawing_stage = DSWStart;
		    return;
		}
	    } else {
		if (dsw->sw.backing_pixmap != None) {
		    if (dsw->sw.num_pending_expose != 0) {
			CopyPendingExpose(dsw);
		    }

		    if (dsw->sw.minimal_drawing && dsw->sw.big_pixmap) {
			dsw->sw.drawing_stage = DSWDrewVisible;
			SetCurrentDrawingToBackground(dsw);
			results = ClipAndDraw(dsw, which,
					      DSWAbortOrFinish, True);
			if (results == DSWAborted) {
			    dsw->sw.drawing_stage = DSWStart;
			    return;
			}
		    }
		}
	    }
	    break;

	case DSWDrewVisible:
	case DSWDrawingBackground:
	    results = ClipAndDraw(dsw, DSWBackingPixmap, DSWAbortOrFinish,
				  (dsw->sw.drawing_stage == DSWDrewVisible));
	    if (results == DSWAborted) {
		dsw->sw.drawing_stage = DSWStart;
		return;
	    }
	    break;

	case DSWDone:
	    break;
    }

    dsw->sw.drawing_stage = DSWDone;
    dsw->sw.num_dirty_areas = 0;
    return;
}

static void AbortDrawing(DPSScrolledWindowWidget dsw)
{
    DSWDrawableType which;

    if (dsw->sw.work != 0) {
	XtRemoveWorkProc(dsw->sw.work);
	dsw->sw.work = 0;
    }

    switch (dsw->sw.drawing_stage) {
	case DSWStart:
	case DSWDone:
	case DSWDrewVisible:
	    break;

	case DSWDrawingVisible:
	    if (dsw->sw.watch_progress || dsw->sw.backing_pixmap == None) {
		which = DSWWindow;
	    } else which = DSWBackingPixmap;
	    (void) ClipAndDraw(dsw, which, DSWAbort, False);
	    break;

	case DSWDrawingBackground:
	    (void) ClipAndDraw(dsw, DSWBackingPixmap, DSWAbort, False);
	    break;

    }

    dsw->sw.num_pending_expose = dsw->sw.num_pending_dirty = 0;
}

static void FinishDrawing(DPSScrolledWindowWidget dsw)
{
    DSWDrawableType which;
    
    if (dsw->sw.work != 0) {
	XtRemoveWorkProc(dsw->sw.work);
	dsw->sw.work = 0;
    }

    switch (dsw->sw.drawing_stage) {
	case DSWStart:
	case DSWDrawingVisible:
	    if (dsw->sw.watch_progress || dsw->sw.backing_pixmap == None) {
		which = DSWWindow;
	    } else which = DSWBackingPixmap;
	    if (dsw->sw.drawing_stage == DSWStart) {
		ClipToDrawingSize(dsw, which);
		if (dsw->sw.num_current_drawing == 0) return;
	    }
	    (void) ClipAndDraw(dsw, which, DSWFinish,
			       dsw->sw.drawing_stage == DSWStart);
	    if (dsw->sw.watch_progress && dsw->sw.backing_pixmap != None) {
		dsw->sw.drawing_stage = DSWDrewVisible;
		CopyWindowToBackingPixmap(dsw);
		if (dsw->sw.num_current_drawing != 0) {
		    (void) ClipAndDraw(dsw, DSWBackingPixmap, DSWFinish, True);
		}
	    } else {
		if (dsw->sw.backing_pixmap != None) {
		    if (dsw->sw.num_pending_expose != 0) {
			CopyPendingExpose(dsw);
		    }

		    if (dsw->sw.minimal_drawing && dsw->sw.big_pixmap) {
			dsw->sw.drawing_stage = DSWDrewVisible;
			SetCurrentDrawingToBackground(dsw);
			(void) ClipAndDraw(dsw, which, DSWFinish, True);
		    }
		}
	    }
	    break;

	case DSWDrewVisible:
	case DSWDrawingBackground:
	     (void) ClipAndDraw(dsw, DSWBackingPixmap, DSWFinish,
				(dsw->sw.drawing_stage == DSWDrewVisible));
	    break;

	case DSWDone:
	    break;
    }

    dsw->sw.drawing_stage = DSWDone;
    dsw->sw.num_dirty_areas = 0;
}

static void DoScroll(
    DPSScrolledWindowWidget dsw,
    int deltaX, int deltaY)
{
    /* Set the PS origin in the X window to the new settings of 
       the scrollbars */
    dsw->sw.origin_x -= deltaX;
    dsw->sw.origin_y -= deltaY;

    /* Update the graphics states for the window and backing pixmap to
       reflect new origin */
    (void) XDPSSetContextGState(dsw->sw.context, dsw->sw.window_gstate);
    SetOriginAndGetTransform(dsw);
    (void) XDPSUpdateContextGState(dsw->sw.context, dsw->sw.window_gstate);

    if (dsw->sw.backing_pixmap != None && !dsw->sw.big_pixmap) {
	(void) XDPSSetContextGState(dsw->sw.context, dsw->sw.backing_gstate);
	SetPixmapOrigin(dsw);
	(void) XDPSUpdateContextGState(dsw->sw.context,
				       dsw->sw.backing_gstate);
    }

    /* Update the stored position of the scroll bars */
    dsw->sw.scroll_x += deltaX;
    dsw->sw.scroll_y += deltaY;
}

static void ShiftPendingExpose(
    DPSScrolledWindowWidget dsw,
    int deltaX, int deltaY)
{
    int i;
    int *r;

    for (i = 0; i < dsw->sw.num_pending_expose; i++) {
	r = dsw->sw.pending_expose + 4*i;
	r[0] -= deltaX;
	r[1] -= deltaY;
    }
}

static void HScrollCallback(
    Widget w,
    XtPointer clientData, XtPointer callData)
{
    DPSScrolledWindowWidget dsw = (DPSScrolledWindowWidget) clientData;
    XmScrollBarCallbackStruct *sb = (XmScrollBarCallbackStruct *) callData;

    if (!dsw->sw.application_scrolling) {
	dsw->sw.scroll_h_value = sb->value;
	ScrollMoved(dsw);
    }
}

static void VScrollCallback(
    Widget w,
    XtPointer clientData, XtPointer callData)
{
    DPSScrolledWindowWidget dsw = (DPSScrolledWindowWidget) clientData;
    XmScrollBarCallbackStruct *sb = (XmScrollBarCallbackStruct *) callData;

    if (!dsw->sw.application_scrolling) {
	dsw->sw.scroll_v_value = sb->value;
	ScrollMoved(dsw);
    }
}

/* ARGSUSED */

static void ScrollMoved(DPSScrolledWindowWidget dsw)
{
    int deltaX, deltaY;

    /* If we haven't started drawing yet, it must be because we're waiting
       for GraphicsExpose events.  Delay scrolling until they come in */
    if (dsw->sw.scrolling && dsw->sw.drawing_stage == DSWStart) return;

    /* Calculate the delta in the scrolling */
    deltaX = dsw->sw.scroll_h_value - dsw->sw.scroll_x;
    deltaY = dsw->sw.scroll_v_value - dsw->sw.scroll_y;

    /* If there is some scrolling to do, then scroll the pixmap and
       copy the pixmap to the window */
    if (deltaX == 0 && deltaY == 0) return;

    ShiftPendingExpose(dsw, deltaX, deltaY);

    AbortOrFinish(dsw);

    DoScroll(dsw, deltaX, deltaY);

    if (!dsw->sw.big_pixmap) {
	/* Copy visible area to new location */
	if (dsw->sw.backing_pixmap != None) {
	    XCopyArea(XtDisplay(dsw), dsw->sw.backing_pixmap,
		      dsw->sw.backing_pixmap, dsw->sw.no_ge_gc,
		      deltaX, deltaY, dsw->sw.drawing_area->core.width,
		      dsw->sw.drawing_area->core.height, 0, 0);
	    if (!dsw->sw.doing_feedback || dsw->sw.feedback_pixmap == None) {
		XCopyArea(XtDisplay(dsw), dsw->sw.backing_pixmap,
			  XtWindow(dsw->sw.drawing_area), dsw->sw.no_ge_gc,
			  0, 0, dsw->sw.drawing_area->core.width,
			  dsw->sw.drawing_area->core.height, 0, 0);
	    }
	    RedisplaySliver(dsw, deltaX, deltaY);
	} else {
	    if (!dsw->sw.doing_feedback || dsw->sw.feedback_pixmap == None) {
		XCopyArea(XtDisplay(dsw), XtWindow(dsw->sw.drawing_area),
			  XtWindow(dsw->sw.drawing_area), dsw->sw.ge_gc,
			  deltaX, deltaY, dsw->sw.drawing_area->core.width,
			  dsw->sw.drawing_area->core.height, 0, 0);
	    }
	    if (dsw->sw.doing_feedback) RedisplaySliver(dsw, deltaX, deltaY);
	    else dsw->sw.drawing_stage = DSWStart;
	}
    }

    if (dsw->sw.doing_feedback) {
	float *r;

	FinishDrawing(dsw);

	r = dsw->sw.prev_dirty_areas;
	ConvertToPS(dsw, 0 + DELTA, dsw->sw.drawing_area->core.height - DELTA,
		    r, r+1);
	ConvertToPS(dsw, dsw->sw.drawing_area->core.width - DELTA, 0 + DELTA,
		    r+2, r+3);
	r[2] -= r[0];
	r[3] -= r[1];
	dsw->sw.num_prev_dirty_areas = 1;

	if (dsw->sw.feedback_pixmap != None) {
	    XDPSSetContextDrawable(dsw->sw.context, dsw->sw.feedback_pixmap,
				   dsw->sw.drawing_area->core.height);
	    SetPixmapOrigin(dsw);
	    XDPSCaptureContextGState(dsw->sw.context,
				     &dsw->sw.feedback_gstate);

	    /* Initialize the feedback pixmap with a copy of the drawing */
	    if (dsw->sw.backing_pixmap != None) {
		CopyToFeedbackPixmap(dsw, dsw->sw.prev_dirty_areas,
				     dsw->sw.num_prev_dirty_areas);
	    } else {
		CopyRectsToCurrentDrawing(dsw, dsw->sw.prev_dirty_areas,
					  dsw->sw.num_prev_dirty_areas);
		(void) ClipAndDraw(dsw, DSWFeedbackPixmap, DSWFinish, True);
	    }
	    if (!dsw->sw.feedback_displayed) {
		XCopyArea(XtDisplay(dsw), dsw->sw.feedback_pixmap,
			  XtWindow(dsw->sw.drawing_area),
			  dsw->sw.no_ge_gc, 0, 0, 
			  dsw->sw.drawing_area->core.width,
			  dsw->sw.drawing_area->core.height, 0, 0);
	    }
	}

	if (dsw->sw.feedback_displayed) {
	    CallFeedbackCallback(dsw, dsw->sw.prev_dirty_areas,
				 dsw->sw.num_prev_dirty_areas);
	}
	if (dsw->sw.feedback_pixmap != None) {
	    UpdateWindowFromFeedbackPixmap(dsw, dsw->sw.prev_dirty_areas,
					   dsw->sw.num_prev_dirty_areas);
	}
	dsw->sw.num_pending_dirty = dsw->sw.num_pending_expose = 0;

    } else {
	if (dsw->sw.backing_pixmap != None &&
	    dsw->sw.drawing_stage == DSWDone) {
	    ComputeOffsets(dsw, &deltaX, &deltaY);
	    if (!dsw->sw.big_pixmap) dsw->sw.scrolling = True;

	    XCopyArea(XtDisplay(dsw), dsw->sw.backing_pixmap,
		      XtWindow(dsw->sw.drawing_area), dsw->sw.no_ge_gc,
		      deltaX, deltaY,
		      dsw->sw.drawing_area->core.width,
		      dsw->sw.drawing_area->core.height, 0, 0);
	} else dsw->sw.scrolling = True;
    }
}

static void AddExposureToPending(
    DPSScrolledWindowWidget dsw,
    XExposeEvent *ev)
{
    float *f;
    int *i;

    if (dsw->sw.backing_pixmap == None || dsw->sw.watch_progress ||
	dsw->sw.doing_feedback) {
	GrowRectList(&dsw->sw.pending_dirty, &dsw->sw.pending_dirty_size,
		     dsw->sw.num_pending_dirty, 1, 1);

	f = dsw->sw.pending_dirty + (dsw->sw.num_pending_dirty * 4);
	ConvertToPS(dsw, ev->x + DELTA, ev->y + ev->height - DELTA, f, f+1);
	ConvertToPS(dsw, ev->x + ev->width - DELTA, ev->y + DELTA, f+2, f+3);
	f[2] -= f[0];
	f[3] -= f[1];
	dsw->sw.num_pending_dirty++;
    }

    GrowIntRectList(&dsw->sw.pending_expose, &dsw->sw.pending_expose_size,
		    dsw->sw.num_pending_expose, 1, 1);

    i = dsw->sw.pending_expose + (dsw->sw.num_pending_expose * 4);
    i[0] = ev->x;
    i[1] = ev->y;
    i[2] = ev->width;
    i[3] = ev->height;
    dsw->sw.num_pending_expose++;
}

static void AddRectsToPending(
    DPSScrolledWindowWidget dsw,
    int *newRect,
    int n)
{
    int *r;
    int i;
    float *f;

    if (dsw->sw.backing_pixmap == None || dsw->sw.watch_progress) {
	GrowRectList(&dsw->sw.pending_dirty, &dsw->sw.pending_dirty_size,
		     dsw->sw.num_pending_dirty, n, 1);

	for (i = 0; i < n; i++) {
	    f = dsw->sw.pending_dirty + (dsw->sw.num_pending_dirty * 4);
	    r = newRect + (i * 4);
	    ConvertToPS(dsw, r[0] + DELTA, r[1] + r[3] - DELTA, f, f+1);
	    ConvertToPS(dsw, r[0] + r[2] - DELTA, r[1] + DELTA, f+2, f+3);
	    f[2] -= f[0];
	    f[3] -= f[1];
	    dsw->sw.num_pending_dirty++;
	}
    }

    GrowIntRectList(&dsw->sw.pending_expose, &dsw->sw.pending_expose_size,
		    dsw->sw.num_pending_expose, n, 1);

    r = dsw->sw.pending_expose + (dsw->sw.num_pending_expose * 4);
    for (i = 0; i < 4*n; i++) r[i] = newRect[i];
    dsw->sw.num_pending_expose += n;
}

static void AddUserSpaceRectsToPending(
    DPSScrolledWindowWidget dsw,
    float *newRect,
    int n)
{
    int *r;
    int i;
    float *f;

    if (dsw->sw.backing_pixmap == None || dsw->sw.watch_progress) {
	GrowRectList(&dsw->sw.pending_dirty, &dsw->sw.pending_dirty_size,
		     dsw->sw.num_pending_dirty, n, 1);

	f = dsw->sw.pending_dirty + (dsw->sw.num_pending_dirty * 4);
	for (i = 0; i < 4*n; i++) f[i] = newRect[i];
	dsw->sw.num_pending_dirty += n;
    }

    GrowIntRectList(&dsw->sw.pending_expose, &dsw->sw.pending_expose_size,
		    dsw->sw.num_pending_expose, n, 1);

    for (i = 0; i < n; i++) {
	r = dsw->sw.pending_expose + (dsw->sw.num_pending_expose * 4);
	f = newRect + (i * 4);
	ConvertToX(dsw, LEFT(f), TOP(f), r, r+1);
	ConvertToX(dsw, RIGHT(f), BOTTOM(f), r+2, r+3);
	r[2] -= r[0];
	r[3] -= r[1];
	dsw->sw.num_pending_expose++;
    }
}

static void CopyRectsToCurrentDrawing(
    DPSScrolledWindowWidget dsw,
    float *newRect,
    int n)
{
    float *r;
    int i;

    GrowRectList(&dsw->sw.current_drawing, &dsw->sw.current_drawing_size,
		 0, n, 1);

    r = dsw->sw.current_drawing;
    for (i = 0; i < 4*n; i++) r[i] = newRect[i];
    dsw->sw.num_current_drawing = n;
}

static void CopyRectsToDirtyArea(
    DPSScrolledWindowWidget dsw,
    float *newRect,
    int n)
{
    float *r;
    int i;

    GrowRectList(&dsw->sw.dirty_areas, &dsw->sw.dirty_areas_size, 0, n, 1);

    r = dsw->sw.dirty_areas;
    for (i = 0; i < 4*n; i++) r[i] = newRect[i];
    dsw->sw.num_dirty_areas = n;
}

static void AddRectsToDirtyArea(
    DPSScrolledWindowWidget dsw,
    float *newRect,
    int n)
{
    float *r;
    int i;

    GrowRectList(&dsw->sw.dirty_areas, &dsw->sw.dirty_areas_size,
		 dsw->sw.num_dirty_areas, n, 1);

    r = dsw->sw.dirty_areas + (4 * dsw->sw.num_dirty_areas);
    for (i = 0; i < 4*n; i++) r[i] = newRect[i];
    dsw->sw.num_dirty_areas += n;
}

static void CopyRectsToPrevDirtyArea(
    DPSScrolledWindowWidget dsw,
    float *newRect,
    int n)
{
    float *r;
    int i;

    GrowRectList(&dsw->sw.prev_dirty_areas,
		 &dsw->sw.prev_dirty_areas_size, 0, n, 1);

    r = dsw->sw.prev_dirty_areas;
    for (i = 0; i < 4*n; i++) r[i] = newRect[i];
    dsw->sw.num_prev_dirty_areas = n;
}

/* ARGSUSED */

static void DrawingAreaGraphicsExpose(
    Widget w,
    XtPointer clientData,
    XEvent *event,
    Boolean *goOn)
{
    DPSScrolledWindowWidget dsw = (DPSScrolledWindowWidget) clientData;
    XExposeEvent *ev = (XExposeEvent *) event;

    switch (event->type) {
	case GraphicsExpose:
	    /* GraphicsExpose occur during unbuffered scrolling */
	    if (dsw->sw.backing_pixmap == None && dsw->sw.scrolling) {
		/* Unbuffered scrolling case */
		AddExposureToPending(dsw, ev);

		if (ev->count == 0) {
		    AddRectsToDirtyArea(dsw, dsw->sw.pending_dirty,
					 dsw->sw.num_pending_dirty);
		    StartDrawing(dsw);
		    dsw->sw.num_pending_dirty = dsw->sw.num_pending_expose = 0;
		}
	    }
	    break;
    }
}

static Boolean MoreExposes(Widget w)
{
    XEvent event;

    if (XPending(XtDisplay(w)) > 0) {
	if (XCheckTypedWindowEvent(XtDisplay(w), XtWindow(w),
				   Expose, &event)) {
	    XPutBackEvent(XtDisplay(w), &event);
	    return True;;
	}
    }
    return False;
}

static void DrawingAreaExpose(
    Widget w,
    XtPointer clientData, XtPointer callData)
{
    DPSScrolledWindowWidget dsw = (DPSScrolledWindowWidget) clientData;
    XmDrawingAreaCallbackStruct *d = (XmDrawingAreaCallbackStruct *) callData;
    XExposeEvent *ev = (XExposeEvent *) d->event;
    int dx, dy;

    if (dsw->sw.doing_feedback) {
	if (dsw->sw.feedback_pixmap != None) {
	    XCopyArea(XtDisplay(dsw), dsw->sw.feedback_pixmap,
		      XtWindow(dsw->sw.drawing_area),
		      dsw->sw.no_ge_gc,
		      ev->x, ev->y, ev->width, ev->height, ev->x, ev->y);
	} else {
	    if (dsw->sw.backing_pixmap != None) {
		ComputeOffsets(dsw, &dx, &dy);

		XCopyArea(XtDisplay(dsw), dsw->sw.backing_pixmap,
			  XtWindow(dsw->sw.drawing_area),
			  dsw->sw.no_ge_gc, ev->x + dx, ev->y + dy,
			  ev->width, ev->height, ev->x, ev->y);
	    }
	    AddExposureToPending(dsw, ev);
	    if (ev->count != 0 || MoreExposes(w)) return;

	    if (dsw->sw.backing_pixmap == None) {
		CopyRectsToCurrentDrawing(dsw, dsw->sw.pending_dirty,
					  dsw->sw.num_pending_dirty);
		dsw->sw.drawing_stage = DSWStart;
		FinishDrawing(dsw);
	    }
	    if (dsw->sw.feedback_displayed) {
		CallFeedbackCallback(dsw, dsw->sw.pending_dirty,
				     dsw->sw.num_pending_dirty);
	    }
	    dsw->sw.num_pending_dirty = dsw->sw.num_pending_expose = 0;
	}
	return;
    }

    if (dsw->sw.backing_pixmap != None) {
	if (dsw->sw.drawing_stage == DSWStart && dsw->sw.watch_progress) {
	    SplitExposeEvent(dsw, ev);
	    if (ev->count == 0) {
		if (MoreExposes(w)) return;
		StartDrawing(dsw);
		dsw->sw.num_pending_dirty = dsw->sw.num_pending_expose = 0;
	    }
	    return;
	}

	if (dsw->sw.drawing_stage < DSWDrewVisible) {
	    SplitExposeEvent(dsw, ev);
	    return;
	}
	ComputeOffsets(dsw, &dx, &dy);

	XCopyArea(XtDisplay(dsw), dsw->sw.backing_pixmap,
		  XtWindow(dsw->sw.drawing_area),
		  dsw->sw.no_ge_gc,
		  ev->x + dx, ev->y + dy, ev->width, ev->height, ev->x, ev->y);
    } else {
	AddExposureToPending(dsw, ev);
	if (ev->count == 0) {
	    if (MoreExposes(w)) return;
	    if (dsw->sw.drawing_stage == DSWDone ||
		dsw->sw.drawing_stage == DSWStart) {
		CopyRectsToDirtyArea(dsw, dsw->sw.pending_dirty,
				    dsw->sw.num_pending_dirty);
		StartDrawing(dsw);
		dsw->sw.num_pending_dirty = dsw->sw.num_pending_expose = 0;
	    }
	}
    }
}	      

static void Resize(Widget w)
{
    DPSScrolledWindowWidget dsw = (DPSScrolledWindowWidget) w;
    DSWResizeCallbackRec r;
    float x, y;

    if (XtIsRealized(w)) (void) AbortOrFinish(dsw);

    r.oldw = dsw->sw.scrolled_window->core.width;
    r.oldh = dsw->sw.scrolled_window->core.height;
    r.neww = dsw->core.width;
    r.newh = dsw->core.height;
    r.x = r.y = 0;

    XtCallCallbackList(w, dsw->sw.resize_callback, (XtPointer) &r);

    if (XtIsRealized(w)) {
	ConvertToPS(dsw, (float) r.x, (float) r.y, &x, &y);
    } else if (r.x != 0 || r.y != 0) ScrollBy(w, r.x, r.y);

    XtResizeWidget(dsw->sw.scrolled_window, w->core.width, w->core.height, 0);

    if (!XtIsRealized(w)) return;

    if (dsw->sw.backing_pixmap != None &&
	dsw->sw.pixmap_width == CEIL(dsw->sw.drawing_width) &&
	dsw->sw.pixmap_height == CEIL(dsw->sw.drawing_height) &&
	dsw->sw.pixmap_width >= (int) dsw->sw.drawing_area->core.width &&
	dsw->sw.pixmap_height >= (int) dsw->sw.drawing_area->core.width) {
	
	XDPSSetContextGState(dsw->sw.context, dsw->sw.window_gstate);
	DPSinitclip(dsw->sw.context);
	DPSinitviewclip(dsw->sw.context);
	SetDrawingAreaPosition(dsw, x, y, r.x, r.y, True);
	SetOriginAndGetTransform(dsw);
	XDPSUpdateContextGState(dsw->sw.context, dsw->sw.window_gstate);
	XClearArea(XtDisplay(dsw), XtWindow(dsw->sw.drawing_area),
		   0, 0, 0, 0, True);
    } else {
	dsw->sw.use_saved_scroll = True;
	dsw->sw.scroll_pic_x = x;
	dsw->sw.scroll_pic_y = y;
	dsw->sw.scroll_win_x = r.x;
	dsw->sw.scroll_win_y = r.y;

	if (dsw->sw.backing_pixmap != None) {
	    XFreePixmap(XtDisplay(dsw), dsw->sw.backing_pixmap);
	    XDPSFreeContextGState(dsw->sw.context, dsw->sw.backing_gstate);
	}
	dsw->sw.backing_pixmap = None;
	dsw->sw.big_pixmap = False;
	dsw->sw.pixmap_width = dsw->sw.pixmap_height = 0;

	dsw->sw.num_dirty_areas = 1;
	LEFT(dsw->sw.dirty_areas) = 0.0;
	BOTTOM(dsw->sw.dirty_areas) = 0.0;
	WIDTH(dsw->sw.dirty_areas) = dsw->sw.area_width;
	HEIGHT(dsw->sw.dirty_areas) = dsw->sw.area_height;

	SetUpInitialInformation(dsw);
    }

    if (dsw->sw.doing_feedback) {
	float *r;
	int dx, dy;

	CheckFeedbackPixmap(dsw);
	r = dsw->sw.prev_dirty_areas;
	ConvertToPS(dsw, 0 + DELTA, dsw->sw.drawing_area->core.height - DELTA,
		    r, r+1);
	ConvertToPS(dsw, dsw->sw.drawing_area->core.width - DELTA, 0 + DELTA,
		    r+2, r+3);
	r[2] -= r[0];
	r[3] -= r[1];
	dsw->sw.num_prev_dirty_areas = 1;

	if (dsw->sw.feedback_pixmap != None) {
	    /* Initialize the feedback pixmap with a copy of the drawing */
	    if (dsw->sw.backing_pixmap != None) {
		CopyToFeedbackPixmap(dsw, dsw->sw.prev_dirty_areas,
				     dsw->sw.num_prev_dirty_areas);
	    } else {
		CopyRectsToCurrentDrawing(dsw, dsw->sw.prev_dirty_areas,
					  dsw->sw.num_prev_dirty_areas);
		(void) ClipAndDraw(dsw, DSWFeedbackPixmap, DSWFinish, True);
	    }
	    XCopyArea(XtDisplay(dsw), dsw->sw.feedback_pixmap,
		      XtWindow(dsw->sw.drawing_area),
		      dsw->sw.no_ge_gc, 0, 0, 
		      dsw->sw.drawing_area->core.width,
		      dsw->sw.drawing_area->core.height, 0, 0);
	} else {
	    if (dsw->sw.backing_pixmap != None) {
		ComputeOffsets(dsw, &dx, &dy);

		XCopyArea(XtDisplay(dsw), dsw->sw.backing_pixmap,
			  XtWindow(dsw->sw.drawing_area),
			  dsw->sw.no_ge_gc, dx, dy,
			  dsw->sw.drawing_area->core.width,
			  dsw->sw.drawing_area->core.height, 0, 0);
	    } else {
		CopyRectsToCurrentDrawing(dsw, dsw->sw.prev_dirty_areas,
					  dsw->sw.num_prev_dirty_areas);
		dsw->sw.drawing_stage = DSWStart;
		FinishDrawing(dsw);
	    }
	}
	if (dsw->sw.feedback_displayed) {
	    CallFeedbackCallback(dsw, dsw->sw.prev_dirty_areas,
				 dsw->sw.num_prev_dirty_areas);
	}
	if (dsw->sw.feedback_pixmap != None) {
	    UpdateWindowFromFeedbackPixmap(dsw, dsw->sw.prev_dirty_areas,
					   dsw->sw.num_prev_dirty_areas);
	}
	dsw->sw.num_pending_dirty = dsw->sw.num_pending_expose = 0;
    }
}

static void CheckFeedbackPixmap(DPSScrolledWindowWidget dsw)
{
    if (dsw->sw.feedback_pixmap != None &&
	(dsw->sw.feedback_width < (int) dsw->sw.drawing_area->core.width ||
	 dsw->sw.feedback_height < (int) dsw->sw.drawing_area->core.height)) {
	XFreePixmap(XtDisplay(dsw), dsw->sw.feedback_pixmap);
	dsw->sw.feedback_pixmap = None;
	dsw->sw.feedback_width = dsw->sw.feedback_height = 0;
    }
    if (dsw->sw.use_feedback_pixmap && dsw->sw.feedback_pixmap == None) {
	dsw->sw.feedback_pixmap =
		AllocPixmap(dsw, dsw->sw.drawing_area->core.width,
			    dsw->sw.drawing_area->core.height);
	if (dsw->sw.feedback_pixmap != None) {
	    dsw->sw.feedback_width = dsw->sw.drawing_area->core.width;
	    dsw->sw.feedback_height = dsw->sw.drawing_area->core.height;
	}
    }
    if (dsw->sw.feedback_pixmap != None) {
	XDPSSetContextDrawable(dsw->sw.context, dsw->sw.feedback_pixmap,
			       dsw->sw.drawing_area->core.height);
	SetPixmapOrigin(dsw);
	XDPSCaptureContextGState(dsw->sw.context, &dsw->sw.feedback_gstate);
    }
}

static void UpdateGStates(DPSScrolledWindowWidget dsw)
{
    /* Create graphics states for the window and backing pixmap in
       the new context */
    XDPSSetContextDrawable(dsw->sw.context, XtWindow(dsw->sw.drawing_area),
			   dsw->sw.drawing_area->core.height);
    DPSinitgraphics(dsw->sw.context);
    if (dsw->sw.scale != 1.0) {
	DPSscale(dsw->sw.context, dsw->sw.scale, dsw->sw.scale);
    }

    SetOriginAndGetTransform(dsw);
    (void) XDPSCaptureContextGState(dsw->sw.context, &dsw->sw.window_gstate);
    if (dsw->sw.backing_pixmap != None) {
	XDPSSetContextDrawable(dsw->sw.context, dsw->sw.backing_pixmap,
			       dsw->sw.pixmap_height);

	SetPixmapOffset(dsw);
	SetPixmapOrigin(dsw);
	XDPSCaptureContextGState(dsw->sw.context, &dsw->sw.backing_gstate);
    }
}

static void CheckPixmapSize(DPSScrolledWindowWidget dsw)
{
    Boolean freeIt = False;
    int w = dsw->sw.pixmap_width, h = dsw->sw.pixmap_height;
    Widget wid = dsw->sw.drawing_area;
    unsigned int dBytes;

    if (dsw->sw.pixmap_limit > 0) {
	if (w * h > dsw->sw.pixmap_limit) freeIt = True;
    } else if (dsw->sw.pixmap_limit < 0 &&
	       w * h > dsw->sw.unscaled_width * dsw->sw.unscaled_height &&
	       w * h > (int) wid->core.width * (int) wid->core.height) {
	freeIt = True;
    }

    if (dsw->sw.absolute_pixmap_limit > 0) {
	dBytes = (wid->core.depth + 7) / 8;	  /* Convert into bytes */
	if (w * h * dBytes > (unsigned)dsw->sw.absolute_pixmap_limit * 1024) {
	    freeIt = True;
	}
    }
    if (freeIt) {
	XFreePixmap(XtDisplay(dsw), dsw->sw.backing_pixmap);
	dsw->sw.backing_pixmap = None;
	dsw->sw.big_pixmap = False;
	dsw->sw.pixmap_width = dsw->sw.pixmap_height = 0;
	XDPSFreeContextGState(dsw->sw.context, dsw->sw.backing_gstate);
    }
}

static void ResizeArea(DPSScrolledWindowWidget dsw)
{
    AbortDrawing(dsw);

    /* Make everything dirty */
    dsw->sw.num_dirty_areas = 1;
    LEFT(dsw->sw.dirty_areas) = 0.0;
    BOTTOM(dsw->sw.dirty_areas) = 0.0;
    WIDTH(dsw->sw.dirty_areas) = dsw->sw.area_width;
    HEIGHT(dsw->sw.dirty_areas) = dsw->sw.area_height;
    
    if (dsw->sw.big_pixmap) {
	XFreePixmap(XtDisplay(dsw), dsw->sw.backing_pixmap);
	dsw->sw.backing_pixmap = None;
	dsw->sw.big_pixmap = False;
	dsw->sw.pixmap_width = dsw->sw.pixmap_height = 0;
	XDPSFreeContextGState(dsw->sw.context, dsw->sw.backing_gstate);
    }

    if (!dsw->sw.use_saved_scroll) {
	/* Keep the upper left in the same place */
	dsw->sw.scroll_win_x = 0;
	dsw->sw.scroll_win_y = 0;
	ConvertToPS(dsw, 0.0, 0.0,
		    &dsw->sw.scroll_pic_x, &dsw->sw.scroll_pic_y);
	dsw->sw.use_saved_scroll = True;
    }

    SetUpInitialInformation(dsw);
}

static void ClearDirtyAreas(DPSScrolledWindowWidget dsw)
{
    int i;
    float *r;
    int llx, lly, urx, ury;

    for (i = 0; i < dsw->sw.num_dirty_areas; i++) {
	r = dsw->sw.dirty_areas + (i * 4);
	ConvertToX(dsw, LEFT(r), BOTTOM(r), &llx, &lly);
	ConvertToX(dsw, RIGHT(r), TOP(r), &urx, &ury);
	XClearArea(XtDisplay(dsw), XtWindow(dsw->sw.drawing_area),
		  llx, ury, urx-llx, lly-ury, True);
    }
}

static void HandleFeedbackPixmapChange(DPSScrolledWindowWidget dsw)
{
    if (!dsw->sw.use_feedback_pixmap) {
	/* Get rid of one if we have it */
	if (dsw->sw.feedback_pixmap != None) {
	    XFreePixmap(XtDisplay(dsw), dsw->sw.feedback_pixmap);
	    dsw->sw.feedback_pixmap = None;
	    dsw->sw.feedback_width = dsw->sw.feedback_height = 0;
	}
    } else {
	if (dsw->sw.doing_feedback) {
	    float *r;

	    CheckFeedbackPixmap(dsw);
	    if (dsw->sw.feedback_pixmap == None) return;

	    r = dsw->sw.prev_dirty_areas;
	    ConvertToPS(dsw, 0 + DELTA,
			dsw->sw.drawing_area->core.height - DELTA, r, r+1);
	    ConvertToPS(dsw, dsw->sw.drawing_area->core.width - DELTA,
			0 + DELTA, r+2, r+3);
	    r[2] -= r[0];
	    r[3] -= r[1];
	    dsw->sw.num_prev_dirty_areas = 1;

	    /* Initialize the feedback pixmap with a copy of the drawing */
	    if (dsw->sw.backing_pixmap != None) {
		CopyToFeedbackPixmap(dsw, dsw->sw.prev_dirty_areas,
				     dsw->sw.num_prev_dirty_areas);
	    } else {
		CopyRectsToCurrentDrawing(dsw, dsw->sw.prev_dirty_areas,
					  dsw->sw.num_prev_dirty_areas);
		(void) ClipAndDraw(dsw, DSWFeedbackPixmap, DSWFinish, True);
	    }
	    if (dsw->sw.feedback_displayed) {
		CallFeedbackCallback(dsw, dsw->sw.prev_dirty_areas,
				     dsw->sw.num_prev_dirty_areas);
	    }
	}
    }
}	    

/* ARGSUSED */

static Boolean SetValues(
    Widget old, Widget req, Widget new,
    ArgList args,
    Cardinal *num_args)
{
    DPSScrolledWindowWidget olddsw = (DPSScrolledWindowWidget) old;
    DPSScrolledWindowWidget newdsw = (DPSScrolledWindowWidget) new;
    Bool inited;

#define NE(field) newdsw->sw.field != olddsw->sw.field
#define DONT_CHANGE(field) \
    if (NE(field)) newdsw->sw.field = olddsw->sw.field;

    DONT_CHANGE(ctm_ptr);
    DONT_CHANGE(inv_ctm_ptr);
    DONT_CHANGE(backing_pixmap);
    DONT_CHANGE(feedback_pixmap);
    DONT_CHANGE(window_gstate);
    DONT_CHANGE(backing_gstate);
    DONT_CHANGE(feedback_gstate);
    
    if (NE(context)) {
	DSWSetupCallbackRec setup;

	if (newdsw->sw.context == NULL) {
	    newdsw->sw.context = XDPSGetSharedContext(XtDisplay(newdsw));
	} 
	if (_XDPSTestComponentInitialized(newdsw->sw.context,
					  dps_init_bit_dsw, &inited) ==
	    dps_status_unregistered_context) {
	    XDPSRegisterContext(newdsw->sw.context, False);
	}
	if (XtIsRealized(newdsw)) {
	    setup.context = newdsw->sw.context;
	    XtCallCallbackList((Widget) newdsw, newdsw->sw.setup_callback,
			       (XtPointer) &setup);
	}
	UpdateGStates(newdsw);
    }

    /* Watch progress only works with pass-through event dispatching */

    if (NE(watch_progress)) {
	if (newdsw->sw.watch_progress &&
	    XDPSSetEventDelivery(XtDisplay(newdsw), dps_event_query) !=
		    dps_event_pass_through) newdsw->sw.watch_progress = False;
    }

    if (NE(application_scrolling) && !newdsw->sw.application_scrolling) {
	XtVaSetValues(newdsw->sw.h_scroll, XmNmaximum, newdsw->sw.scroll_h_max,
		      XmNvalue, newdsw->sw.scroll_h_value,
		      XmNsliderSize, newdsw->sw.scroll_h_size, NULL);
	XtVaSetValues(newdsw->sw.v_scroll, XmNmaximum, newdsw->sw.scroll_v_max,
		      XmNvalue, newdsw->sw.scroll_v_value,
		      XmNsliderSize, newdsw->sw.scroll_v_size, NULL);
    }
	    
    if (newdsw->sw.doing_feedback) {
	DONT_CHANGE(scale);
	DONT_CHANGE(area_width);
	DONT_CHANGE(area_height);
    }

    if (NE(pixmap_limit) || NE(absolute_pixmap_limit)) CheckPixmapSize(newdsw);

    if (NE(area_width) || NE(area_height) || NE(scale)) ResizeArea(newdsw);

    /* It's too confusing to let any of these things change in the middle
       of drawing */

    if (NE(use_backing_pixmap) || NE(watch_progress) ||
	NE(minimal_drawing) || NE(document_size_pixmaps)) {
	Boolean freeIt = False, setUp = False;
	AbortOrFinish(newdsw);
	if (NE(use_backing_pixmap)) {
	    if (newdsw->sw.use_backing_pixmap) setUp = True;
	    else freeIt = True;
	}
	if (NE(document_size_pixmaps)) {
	    if (newdsw->sw.backing_pixmap != None) freeIt = True;
	    setUp = True;
	}
	if (freeIt) FreeBackingPixmap(newdsw);
	if (setUp) SetUpInitialPixmap(newdsw);
    }

    if (NE(dirty_areas)) {
	float *r = newdsw->sw.dirty_areas;
	int n = newdsw->sw.num_dirty_areas;
	DONT_CHANGE(dirty_areas);
	DONT_CHANGE(num_dirty_areas);
	AbortOrFinish(newdsw);
	AddRectsToDirtyArea(newdsw, r, n);
	if (newdsw->sw.watch_progress || newdsw->sw.backing_pixmap == None) {
	    ClearDirtyAreas(newdsw);
	    newdsw->sw.drawing_stage = DSWStart;
	} else {
	    AddUserSpaceRectsToPending(newdsw, r, n);
	    StartDrawing(newdsw);
	}
    }

    if (NE(use_feedback_pixmap)) HandleFeedbackPixmapChange(newdsw);

    return False;
#undef DONT_CHANGE
}

static XtGeometryResult GeometryManager(
    Widget w,
    XtWidgetGeometry *desired, XtWidgetGeometry *allowed)
{
    /* Pass geometry requests up to our parent */
    return XtMakeGeometryRequest(XtParent(w), desired, allowed);
}

static XtGeometryResult QueryGeometry(
    Widget w,
    XtWidgetGeometry *desired, XtWidgetGeometry *allowed)
{
    DPSScrolledWindowWidget dsw = (DPSScrolledWindowWidget) w;

    /* Pass geometry requests down to our child */
    return XtQueryGeometry(dsw->sw.scrolled_window, desired, allowed);
}

static void CopyToFeedbackPixmap(
    DPSScrolledWindowWidget dsw,
    float *rects,
    int n)
{
    int llx, lly, urx, ury;
    int dx, dy;
    int i;
    float *r;

    ComputeOffsets(dsw, &dx, &dy);

    for (i = 0; i < n; i++) {
	r = rects + (i * 4);
	ConvertToX(dsw, LEFT(r), BOTTOM(r), &llx, &lly);
	ConvertToX(dsw, RIGHT(r), TOP(r), &urx, &ury);

	XCopyArea(XtDisplay(dsw), dsw->sw.backing_pixmap,
		  dsw->sw.feedback_pixmap, dsw->sw.no_ge_gc,
		  llx+dx-1, ury+dy-1, urx-llx+2, lly-ury+2, llx-1, ury-1);
    }
}

static void CallFeedbackCallback(
    DPSScrolledWindowWidget dsw,
    float *r,
    int n)
{
    DSWFeedbackCallbackRec f;

    f.start_feedback_data = dsw->sw.start_feedback_data;
    f.continue_feedback_data = dsw->sw.continue_feedback_data;
    if (dsw->sw.feedback_pixmap == None) {
	f.type = DSWWindow;
	f.drawable = XtWindow(dsw->sw.drawing_area);
	f.gstate = dsw->sw.window_gstate;
    } else {
	f.type = DSWFeedbackPixmap;
	f.drawable = dsw->sw.feedback_pixmap;
	f.gstate = dsw->sw.feedback_gstate;
    }
    f.context = dsw->sw.context;
    f.dirty_rects = r;
    f.dirty_count = n;

    XDPSSetContextGState(dsw->sw.context, f.gstate);
    _DPSSWSetRectViewClip(dsw->sw.context, r, n * 4);
    XtCallCallbackList((Widget) dsw, dsw->sw.feedback_callback,
		       (XtPointer) &f);
    DPSWaitContext(dsw->sw.context);
}

static void SetScale(
    Widget w,
    double scale,
    long fixedX, long fixedY)
{
    float psX, psY;

    ConvertToPS((DPSScrolledWindowWidget) w, (float) fixedX, (float) fixedY,
		&psX, &psY);
    SetScaleAndScroll(w, scale, psX, psY, fixedX, fixedY);
}

void DSWSetScale(
    Widget w,
    double scale,
    long fixedX, long fixedY)
{
    XtCheckSubclass(w, dpsScrolledWindowWidgetClass, NULL);

    (*((DPSScrolledWindowWidgetClass) XtClass(w))->
     sw_class.set_scale) (w, scale, fixedX, fixedY);
}

static void ScrollPoint(
    Widget w,
    double psX, double psY,
    long xX, long xY)
{
    DPSScrolledWindowWidget dsw = (DPSScrolledWindowWidget) w;

    if (!XtIsRealized(w)) {
	dsw->sw.use_saved_scroll = True;
	dsw->sw.scroll_pic_x = psX;
	dsw->sw.scroll_pic_y = psY;
	dsw->sw.scroll_win_x = xX;
	dsw->sw.scroll_win_y = xY;
	return;
    } else {
	SetDrawingAreaPosition(dsw, psX, psY, xX, xY, False);
	ScrollMoved(dsw);
    }
}

void DSWScrollPoint(
    Widget w,
    double psX, double psY,
    long xX, long xY)
{
    XtCheckSubclass(w, dpsScrolledWindowWidgetClass, NULL);

    (*((DPSScrolledWindowWidgetClass) XtClass(w))->
     sw_class.scroll_point) (w, psX, psY, xX, xY);
}

static void ScrollBy(Widget w, long dx, long dy)
{
    DPSScrolledWindowWidget dsw = (DPSScrolledWindowWidget) w;
    int value;

    if (dx == 0 && dy == 0) return;

    if (!XtIsRealized(w) && dsw->sw.use_saved_scroll) {
	dsw->sw.scroll_win_x += dx;
	dsw->sw.scroll_win_y += dy;
    } else {
	value = dsw->sw.scroll_h_value + dx;

	if (value < 0) value = 0;
	else if (value > dsw->sw.scroll_h_max - dsw->sw.scroll_h_size) {
	    value = dsw->sw.scroll_h_max - dsw->sw.scroll_h_size;
	}
	dsw->sw.scroll_h_value = value;

	if (!dsw->sw.application_scrolling) {
	    XtVaSetValues(dsw->sw.h_scroll, XmNvalue, value, NULL);
	}

	value = dsw->sw.scroll_v_value + dy;

	if (value < 0) value = 0;
	else if (value > dsw->sw.scroll_v_max - dsw->sw.scroll_v_size) {
	    value = dsw->sw.scroll_v_max - dsw->sw.scroll_v_size;
	}
	dsw->sw.scroll_v_value = value;

	if (!dsw->sw.application_scrolling) {
	    XtVaSetValues(dsw->sw.v_scroll, XmNvalue, value, NULL);
	}

	ScrollMoved(dsw);
    }
}

void DSWScrollBy(Widget w, long dx, long dy)
{
    XtCheckSubclass(w, dpsScrolledWindowWidgetClass, NULL);

    (*((DPSScrolledWindowWidgetClass) XtClass(w))->
     sw_class.scroll_by) (w, dx, dy);
}

static void ScrollTo(Widget w, long x, long y)
{
    DPSScrolledWindowWidget dsw = (DPSScrolledWindowWidget) w;
    int max, size;

    if (XtIsRealized(w)) {
	if (x < 0) x = 0;
	else if (x > dsw->sw.scroll_h_max - dsw->sw.scroll_h_size) {
	    x = dsw->sw.scroll_h_max - dsw->sw.scroll_h_size;
	}
	dsw->sw.scroll_h_value = x;

	if (y < 0) y = 0;
	else if (y > dsw->sw.scroll_v_max - dsw->sw.scroll_v_size) {
	    y = dsw->sw.scroll_v_max - dsw->sw.scroll_v_size;
	}
	dsw->sw.scroll_v_value = y;

	if (!dsw->sw.application_scrolling) {
	    XtVaSetValues(dsw->sw.h_scroll, XmNvalue, x, NULL);
	    XtVaSetValues(dsw->sw.v_scroll, XmNvalue, y, NULL);
	}

	ScrollMoved(dsw);
    }
}

void DSWScrollTo(Widget w, long x, long y)
{
    XtCheckSubclass(w, dpsScrolledWindowWidgetClass, NULL);

    (*((DPSScrolledWindowWidgetClass) XtClass(w))->
     sw_class.scroll_to) (w, x, y);
}

static void SetScaleAndScroll(
    Widget w,
    double scale,
    double psX, double psY,
    long xX, long xY)
{
    DPSScrolledWindowWidget dsw = (DPSScrolledWindowWidget) w;
    Arg arg;
    union {
	int i;
	float f;
    } kludge;

    dsw->sw.use_saved_scroll = True;
    dsw->sw.scroll_pic_x = psX;
    dsw->sw.scroll_pic_y = psY;
    dsw->sw.scroll_win_x = xX;
    dsw->sw.scroll_win_y = xY;

    kludge.f = scale;
    arg.name = XtNscale;
    if (sizeof(float) > sizeof(XtArgVal)) arg.value = (XtArgVal) &kludge.f;
    else arg.value = (XtArgVal) kludge.i;
    XtSetValues(w, &arg, 1);
}

void DSWSetScaleAndScroll(
    Widget w,
    double scale,
    double psX, double psY,
    long xX, long xY)
{
    XtCheckSubclass(w, dpsScrolledWindowWidgetClass, NULL);

    (*((DPSScrolledWindowWidgetClass) XtClass(w))->
     sw_class.set_scale_and_scroll) (w, scale, psX, psY, xX, xY);
}

static void ConvertXToPS(
    Widget w,
    long xX, long xY,
    float *psX, float *psY)
{
    ConvertToPS((DPSScrolledWindowWidget) w, (float) xX, (float) xY, psX, psY);
}

void DSWConvertXToPS(
    Widget w,
    long xX, long xY,
    float *psX, float *psY)
{
    XtCheckSubclass(w, dpsScrolledWindowWidgetClass, NULL);

    (*((DPSScrolledWindowWidgetClass) XtClass(w))->
     sw_class.convert_x_to_ps) (w, xX, xY, psX, psY);
}

static void ConvertPSToX(
    Widget w,
    double psX, double psY,
    int *xX, int *xY)
{
    ConvertToX((DPSScrolledWindowWidget) w, psX, psY, xX, xY);
}

void DSWConvertPSToX(
    Widget w,
    double psX, double psY,
    int *xX, int *xY)
{
    XtCheckSubclass(w, dpsScrolledWindowWidgetClass, NULL);

    (*((DPSScrolledWindowWidgetClass) XtClass(w))->
     sw_class.convert_ps_to_x) (w, psX, psY, xX, xY);
}

static void AddToDirtyArea(
    Widget w,
    float *rect,
    long n)
{
    DPSScrolledWindowWidget dsw = (DPSScrolledWindowWidget) w;

    if (n == 1 && rect[0] == 0 && rect[1] == 0 &&
	rect[2] == -1 && rect[2] == -1) {
	rect[2] = dsw->sw.area_width;
	rect[3] = dsw->sw.area_height;
    }

    XtVaSetValues(w, XtNdirtyAreas, rect, XtNnumDirtyAreas, n, NULL);
}

void DSWAddToDirtyArea(
    Widget w,
    float *rect,
    long n)
{
    XtCheckSubclass(w, dpsScrolledWindowWidgetClass, NULL);

    (*((DPSScrolledWindowWidgetClass) XtClass(w))->
     sw_class.add_to_dirty_area) (w, rect, n);
}

static Boolean TakeFeedbackPixmap(
    Widget w,
    Pixmap *p,
    int *width, int *height, int *depth,
    Screen **screen)
{
    DPSScrolledWindowWidget dsw = (DPSScrolledWindowWidget) w;
    
    if (dsw->sw.doing_feedback) return False;

    *p = dsw->sw.feedback_pixmap;
    if (*p == None) {
	*width = *height = *depth;
	*screen = NULL;
	return True;
    }

    *width = dsw->sw.feedback_width;
    *height = dsw->sw.feedback_height;
    *depth = dsw->sw.drawing_area->core.depth;
    *screen = dsw->core.screen;

    dsw->sw.feedback_pixmap = None;
    dsw->sw.feedback_width = dsw->sw.feedback_height = 0;
    return True;
}

Boolean DSWTakeFeedbackPixmap(
    Widget w,
    Pixmap *p,
    int *width, int *height, int *depth,
    Screen **screen)
{
    XtCheckSubclass(w, dpsScrolledWindowWidgetClass, NULL);

    return (*((DPSScrolledWindowWidgetClass) XtClass(w))->
	    sw_class.take_feedback_pixmap) (w, p, width, height,
					    depth, screen);
}

static void StartFeedbackDrawing(
    Widget w,
    XtPointer start_feedback_data)
{
    DPSScrolledWindowWidget dsw = (DPSScrolledWindowWidget) w;
    float *r;

    FinishDrawing(dsw);
    CheckFeedbackPixmap(dsw);
    if (dsw->sw.feedback_pixmap != None) {
	/* Initialize the feedback pixmap with a copy of the drawing */
	GrowRectList(&dsw->sw.prev_dirty_areas, &dsw->sw.prev_dirty_areas_size,
		     0, 1, 1);
	r = dsw->sw.prev_dirty_areas;
	ConvertToPS(dsw, 0 + DELTA, dsw->sw.drawing_area->core.height - DELTA,
		    r, r+1);
	ConvertToPS(dsw, dsw->sw.drawing_area->core.width - DELTA, 0 + DELTA,
		    r+2, r+3);
	r[2] -= r[0];
	r[3] -= r[1];
	dsw->sw.num_prev_dirty_areas = 1;

	if (dsw->sw.backing_pixmap != None) {
	    CopyToFeedbackPixmap(dsw, dsw->sw.prev_dirty_areas,
				 dsw->sw.num_prev_dirty_areas);
	} else {
	    CopyRectsToCurrentDrawing(dsw, dsw->sw.prev_dirty_areas,
				      dsw->sw.num_prev_dirty_areas);
	    (void) ClipAndDraw(dsw, DSWFeedbackPixmap, DSWFinish, True);
	}
    }
    dsw->sw.num_prev_dirty_areas = 0;
    dsw->sw.doing_feedback = True;
    dsw->sw.start_feedback_data = start_feedback_data;
}

void DSWStartFeedbackDrawing(
    Widget w,
    XtPointer start_feedback_data)
{
    XtCheckSubclass(w, dpsScrolledWindowWidgetClass, NULL);

    (*((DPSScrolledWindowWidgetClass) XtClass(w))->
     sw_class.start_feedback_drawing) (w, start_feedback_data);
}

static void EndFeedbackDrawing(
    Widget w,
    int restore)
{
    DPSScrolledWindowWidget dsw = (DPSScrolledWindowWidget) w;

    if (restore) {
	if (dsw->sw.backing_pixmap != None) {
	    UpdateWindowFromBackingPixmap(dsw, dsw->sw.prev_dirty_areas,
					  dsw->sw.num_prev_dirty_areas);
	} else {
	    CopyRectsToCurrentDrawing(dsw, dsw->sw.prev_dirty_areas,
				      dsw->sw.num_prev_dirty_areas);
	    (void) ClipAndDraw(dsw, DSWWindow, DSWFinish, True);
	}
    }
    if (dsw->sw.feedback_gstate != 0) {
	XDPSFreeContextGState(dsw->sw.context, dsw->sw.feedback_gstate);
    }
    dsw->sw.doing_feedback = dsw->sw.feedback_displayed = False;
}

void DSWEndFeedbackDrawing(
    Widget w,
    Bool restore)
{
    XtCheckSubclass(w, dpsScrolledWindowWidgetClass, NULL);

    (*((DPSScrolledWindowWidgetClass) XtClass(w))->
     sw_class.end_feedback_drawing) (w, restore);
}

static void SetFeedbackDirtyArea(
    Widget w,
    float *rects,
    int count,
    XtPointer continue_feedback_data)
{
    DPSScrolledWindowWidget dsw = (DPSScrolledWindowWidget) w;
    int i;
    float *r;

    for (i = 0; i < count; i++) {
	r = rects + (i * 4);
	if (WIDTH(r) < 0) {
	    LEFT(r) += WIDTH(r);
	    WIDTH(r) = -WIDTH(r);
	}
	if (HEIGHT(r) < 0) {
	    BOTTOM(r) += HEIGHT(r);
	    HEIGHT(r) = -HEIGHT(r);
	}
    }

    if (dsw->sw.backing_pixmap != None) {
	if (dsw->sw.feedback_pixmap != None) {
	    CopyToFeedbackPixmap(dsw, dsw->sw.prev_dirty_areas,
				 dsw->sw.num_prev_dirty_areas);
	} else {
	    UpdateWindowFromBackingPixmap(dsw, dsw->sw.prev_dirty_areas,
					  dsw->sw.num_prev_dirty_areas);
	}
    } else {
	CopyRectsToCurrentDrawing(dsw, dsw->sw.prev_dirty_areas,
				  dsw->sw.num_prev_dirty_areas);
	(void) ClipAndDraw(dsw, (dsw->sw.feedback_pixmap == None ?
				 DSWWindow : DSWFeedbackPixmap),
			   DSWFinish, True);
    }
    dsw->sw.continue_feedback_data = continue_feedback_data;
    CallFeedbackCallback(dsw, rects, count);

    if (dsw->sw.feedback_pixmap != None) {
	CopyRectsToDirtyArea(dsw, dsw->sw.prev_dirty_areas,
			     dsw->sw.num_prev_dirty_areas);
	AddRectsToDirtyArea(dsw, rects, count);
	SimplifyRects(dsw->sw.dirty_areas, &dsw->sw.num_dirty_areas);
	UpdateWindowFromFeedbackPixmap(dsw, dsw->sw.dirty_areas,
				       dsw->sw.num_dirty_areas);
	dsw->sw.num_dirty_areas = 0;
    }
    CopyRectsToPrevDirtyArea(dsw, rects, count);
    dsw->sw.feedback_displayed = True;
}

void DSWSetFeedbackDirtyArea(
    Widget w,
    float *rects,
    int count,
    XtPointer continue_feedback_data)
{
    XtCheckSubclass(w, dpsScrolledWindowWidgetClass, NULL);

    (*((DPSScrolledWindowWidgetClass) XtClass(w))->
     sw_class.set_feedback_dirty_area) (w, rects, count,
					continue_feedback_data);
}

static void FinishPendingDrawing(Widget w)
{
    FinishDrawing((DPSScrolledWindowWidget) w);
}

void DSWFinishPendingDrawing(Widget w)
{
    XtCheckSubclass(w, dpsScrolledWindowWidgetClass, NULL);

    (*((DPSScrolledWindowWidgetClass) XtClass(w))->
     sw_class.finish_pending_drawing) (w);
}

static void AbortPendingDrawing(Widget w)
{
    AbortDrawing((DPSScrolledWindowWidget) w);
}

void DSWAbortPendingDrawing(Widget w)
{
    XtCheckSubclass(w, dpsScrolledWindowWidgetClass, NULL);

    (*((DPSScrolledWindowWidgetClass) XtClass(w))->
     sw_class.abort_pending_drawing) (w);
}

static void UpdateDrawing(
    Widget w,
    float *rects,
    int count)
{
    DPSScrolledWindowWidget dsw = (DPSScrolledWindowWidget) w;
    int i;
    float *r;
    int llx, lly, urx, ury;
    int dx, dy;

    if (dsw->sw.backing_pixmap == None) {
	AddToDirtyArea(w, rects, count);
	return;
    }

    ComputeOffsets(dsw, &dx, &dy);

    for (i = 0; i < count; i++) {
	r = rects + (i * 4);
	ConvertToX(dsw, LEFT(r), BOTTOM(r), &llx, &lly);
	ConvertToX(dsw, RIGHT(r), TOP(r), &urx, &ury);
	XCopyArea(XtDisplay(dsw), XtWindow(dsw->sw.drawing_area),
		  dsw->sw.backing_pixmap, dsw->sw.no_ge_gc,
		  llx-1, ury-1, urx-llx+2, lly-ury+2, llx+dx-1, ury+dy-1);
    }
}

void DSWUpdateDrawing(
    Widget w,
    float *rects,
    int count)
{
    XtCheckSubclass(w, dpsScrolledWindowWidgetClass, NULL);

    (*((DPSScrolledWindowWidgetClass) XtClass(w))->
     sw_class.update_drawing) (w, rects, count);
}

static void GetScrollInfo(
    Widget w,
    int *h_value, int *h_size, int *h_max, int *v_value, int *v_size, int *v_max)
{
    DPSScrolledWindowWidget dsw = (DPSScrolledWindowWidget) w;

    if (h_value != NULL) *h_value = dsw->sw.scroll_h_value;
    if (h_size != NULL) *h_size = dsw->sw.scroll_h_size;
    if (h_max != NULL) *h_max = dsw->sw.scroll_h_max;
    if (v_value != NULL) *v_value = dsw->sw.scroll_v_value;
    if (v_size != NULL) *v_size = dsw->sw.scroll_v_size;
    if (v_max != NULL) *v_max = dsw->sw.scroll_v_max;
}

void DSWGetScrollInfo(
    Widget w,
    int *h_value, int *h_size, int *h_max, int *v_value, int *v_size, int *v_max)
{
    XtCheckSubclass(w, dpsScrolledWindowWidgetClass, NULL);

    (*((DPSScrolledWindowWidgetClass) XtClass(w))->
     sw_class.get_scroll_info) (w, h_value, h_size, h_max,
				v_value, v_size, v_max);
}

static void GetDrawingInfo(
    Widget w,
    DSWDrawableType *type,
    Drawable *drawable,
    DPSGState *gstate,
    DPSContext *context)
{
    DPSScrolledWindowWidget dsw = (DPSScrolledWindowWidget) w;

    if (dsw->sw.backing_pixmap != None) {
	*type = DSWBackingPixmap;
	*drawable = dsw->sw.backing_pixmap;
	*gstate = dsw->sw.backing_gstate;
    } else {
	*type = DSWWindow;
	*drawable = XtWindow(dsw->sw.drawing_area);
	*gstate = dsw->sw.window_gstate;
    }
    *context = dsw->sw.context;
}

void DSWGetDrawingInfo(
    Widget w,
    DSWDrawableType *type,
    Drawable *drawable,
    DPSGState *gstate,
    DPSContext *context)
{
    XtCheckSubclass(w, dpsScrolledWindowWidgetClass, NULL);

    (*((DPSScrolledWindowWidgetClass) XtClass(w))->
     sw_class.get_drawing_info) (w, type, drawable, gstate, context);
}

static Boolean GiveFeedbackPixmap(
    Widget w,
    Pixmap p,
    int width, int height, int depth,
    Screen *screen)
{
    DPSScrolledWindowWidget dsw = (DPSScrolledWindowWidget) w;
    
    if ((unsigned) depth != dsw->sw.drawing_area->core.depth
     || screen != dsw->core.screen
     || dsw->sw.feedback_pixmap != None) return False;

    dsw->sw.feedback_pixmap = p;
    dsw->sw.feedback_width = width;
    dsw->sw.feedback_height = height;

    return True;
}

Boolean DSWGiveFeedbackPixmap(
    Widget w,
    Pixmap p,
    int width, int height, int depth,
    Screen *screen)
{
    XtCheckSubclass(w, dpsScrolledWindowWidgetClass, NULL);

    return (*((DPSScrolledWindowWidgetClass) XtClass(w))->
	    sw_class.give_feedback_pixmap) (w, p, width, height,
					    depth, screen);
}

static void ConvertToX(
    DPSScrolledWindowWidget dsw,
    float psX,
    float psY,
    int *xX,
    int *xY)
{
    *xX = dsw->sw.ctm[0] * psX + dsw->sw.ctm[2] * psY + dsw->sw.ctm[4] +
	    dsw->sw.x_offset + 0.5;
    *xY = dsw->sw.ctm[1] * psX + dsw->sw.ctm[3] * psY + dsw->sw.ctm[5] +
	    dsw->sw.y_offset + 0.5;
}

static void ConvertToPS(
    DPSScrolledWindowWidget dsw,
    float xX, float xY,
    float *psX, float *psY)
{
    xX -= dsw->sw.x_offset;
    xY -= dsw->sw.y_offset;

    *psX = dsw->sw.inv_ctm[0] * xX + dsw->sw.inv_ctm[2] * xY +
	    dsw->sw.inv_ctm[4];
    *psY = dsw->sw.inv_ctm[1] * xX + dsw->sw.inv_ctm[3] * xY +
	    dsw->sw.inv_ctm[5];
}

static void ConvertToOrigPS(
    DPSScrolledWindowWidget dsw,
    int xX, int xY,
    float *psX, float *psY)
{
    xX -= dsw->sw.x_offset;
    xY -= dsw->sw.y_offset;

    *psX = dsw->sw.orig_inv_ctm[0] * xX + dsw->sw.orig_inv_ctm[2] * xY +
	    dsw->sw.orig_inv_ctm[4];
    *psY = dsw->sw.orig_inv_ctm[1] * xX + dsw->sw.orig_inv_ctm[3] * xY +
	    dsw->sw.orig_inv_ctm[5];
}

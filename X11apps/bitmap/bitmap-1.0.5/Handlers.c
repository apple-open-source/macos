/*

Copyright 1989, 1998  The Open Group

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

/*
 * Author:  Davor Matic, MIT X Consortium
 */

#include <X11/IntrinsicP.h>
#include <X11/StringDefs.h>
#include "BitmapP.h"
    
#include <stdio.h>
#include <math.h>

#define min(x, y)                     (((int)(x) < (int)(y)) ? (x) : (y))
#define max(x, y)                     (((int)(x) > (int)(y)) ? (x) : (y))

#include "Requests.h"


/*****************************************************************************
 *                                  Handlers                                 *
 *****************************************************************************/

#define QueryInSquare(BW, x, y, square_x, square_y)\
    ((InBitmapX(BW, x) == (square_x)) &&\
     (InBitmapY(BW, y) == (square_y)))


static void 
DragOnePointHandler(Widget w, 
		    XtPointer client_data, 
		    XEvent *event, 
		    Boolean *cont) /* ARGSUSED */
{
    BWStatus *status = (BWStatus *)client_data;
    BitmapWidget BW = (BitmapWidget) w;

    if (DEBUG)
	fprintf(stderr, "D1PH ");

    switch (event->type) {
    
    case ButtonPress:
	if (event->xbutton.state != status->state) return;
	if (!QuerySet(status->at_x, status->at_y)) {
	    BWStoreToBuffer(w);
	    status->value = Value(BW, event->xbutton.button);
	    status->btime = event->xbutton.time;
	    status->at_x = InBitmapX(BW, event->xbutton.x);
	    status->at_y = InBitmapY(BW, event->xbutton.y);
	    status->success = status->draw ? True : False;
	    if (status->draw)
		(*(DrawOnePointProc)status->draw)(w,
				status->at_x, status->at_y, status->value);
	}
	break;
	
    case ButtonRelease:
	if (QuerySet(status->at_x, status->at_y)) {
	    status->value = Value(BW, event->xbutton.button);
	    status->btime = event->xbutton.time;
	    status->at_x = InBitmapX(BW, event->xbutton.x);
	    status->at_y = InBitmapY(BW, event->xbutton.y);
	    status->success = status->draw ? True : False;
	    /* SUPPRESS 701 */
	    BWTerminateRequest(w, TRUE); 
	}
	break;

    case MotionNotify:
	if (QuerySet(status->at_x, status->at_y)) {
	    if (!QueryInSquare(BW, event->xmotion.x, event->xmotion.y,
			       status->at_x, status->at_y)) {
		status->at_x = InBitmapX(BW, event->xmotion.x);
		status->at_y = InBitmapY(BW, event->xmotion.y);
		if (status->draw)
		    (*(DrawOnePointProc)status->draw)(w,
				    status->at_x, status->at_y, status->value);
	    }
	}
	break;

    }
}

void 
DragOnePointEngage(Widget w, 
		   BWStatus *status, 
		   XtPointer draw, 
		   int *state)
{
    
    status->at_x = NotSet;
    status->at_y = NotSet;
    status->draw = draw;
    status->success = False;
    status->state = *state;
    
    XtAddEventHandler(w,
		      ButtonPressMask | ButtonReleaseMask | PointerMotionMask,
		      FALSE, DragOnePointHandler, (XtPointer)status);
}

/* ARGSUSED */
void 
DragOnePointTerminate(Widget w, 
		      BWStatus *status, 
		      XtPointer draw)
{
    
    if (status->success) {
	BWChangeNotify(w);
	BWSetChanged(w);
    }
    
    XtRemoveEventHandler(w,
		 ButtonPressMask | ButtonReleaseMask | PointerMotionMask,
		 FALSE, DragOnePointHandler, (XtPointer)status);
    
}

void 
OnePointHandler(Widget w, 
		XtPointer client_data, 
		XEvent *event, 
		Boolean *cont) /* ARGSUSED */
{
    BWStatus    *status = (BWStatus *)client_data;
    BitmapWidget BW = (BitmapWidget) w;
    
    if (DEBUG)
	fprintf(stderr, "1PH ");

    switch (event->type) {
	
    case Expose:
	if (QuerySet(status->at_x, status->at_y)) {
	    BWClip(w, event->xexpose.x, event->xexpose.y,
		   event->xexpose.width, event->xexpose.height);
	    if (status->draw)
		(*(DrawOnePointProc)status->draw)(w,
				status->at_x, status->at_y, Highlight);
	    
	    BWUnclip(w);
	}
	break;
	
    case ButtonPress:
	if (event->xbutton.state != status->state) return;
	if (!QuerySet(status->at_x, status->at_y)) {
	    status->value = Value(BW, event->xbutton.button);
	    status->btime = event->xbutton.time;
	    status->at_x = InBitmapX(BW, event->xbutton.x);
	    status->at_y = InBitmapY(BW, event->xbutton.y);
	    if (status->draw)
		(*(DrawOnePointProc)status->draw)(w,
				status->at_x, status->at_y, Highlight);
	}
	break;
	
    case ButtonRelease:
	if (QuerySet(status->at_x, status->at_y)) {
	    if (status->draw)
		(*(DrawOnePointProc)status->draw)(w,
				status->at_x, status->at_y, Highlight);
	    
	    status->value = Value(BW, event->xbutton.button);
	    status->btime = event->xbutton.time;
	    status->at_x = InBitmapX(BW, event->xbutton.x);
	    status->at_y = InBitmapY(BW, event->xbutton.y);
	    status->success = True;
	    
	    BWTerminateRequest(w, TRUE);
	}
	break;
	
    case MotionNotify:
	if (QuerySet(status->at_x, status->at_y)) {
	    if (!QueryInSquare(BW, event->xmotion.x, event->xmotion.y,
			       status->at_x, status->at_y)) {
		if (status->draw)
		    (*(DrawOnePointProc)status->draw)(w,
				    status->at_x, status->at_y, Highlight);
		status->at_x = InBitmapX(BW, event->xmotion.x);
		status->at_y = InBitmapY(BW, event->xmotion.y);
		if (status->draw)
		    (*(DrawOnePointProc)status->draw)(w,
				    status->at_x, status->at_y, Highlight);
	    }
	}      
	break;
    }
}

void 
OnePointEngage(Widget w, 
	       BWStatus *status, 
	       XtPointer draw, 
	       int *state)
{
    status->at_x = NotSet;
    status->at_y = NotSet;
    status->draw = draw;
    status->success = False;
    status->state = *state;

    XtAddEventHandler(w,
		      ButtonPressMask | ButtonReleaseMask | 
		      ExposureMask | PointerMotionMask,
		      FALSE, OnePointHandler, (XtPointer)status);
}

#if 0
void 
OnePointImmediateEngage(Widget w, 
			BWStatus *status, 
			XtPointer draw, 
			int *state)
{
    status->at_x = 0;
    status->at_y = 0;
    status->draw = draw;
    status->success = False;
    status->state = *state;
    
    if (status->draw)
	(*(DrawOnePointProc)status->draw)(w,
			status->at_x, status->at_y, Highlight);
    
    XtAddEventHandler(w,
		      ButtonPressMask | ButtonReleaseMask | 
		      ExposureMask | PointerMotionMask,
		      FALSE, OnePointHandler, (XtPointer)status);
}
#endif

void 
OnePointTerminate(Widget w, 
		  BWStatus *status, 
		  XtPointer draw)
{
    
    if (status->success && draw) {
	BWStoreToBuffer(w);
	(*(DrawOnePointProc)draw)(w,
		status->at_x, status->at_y,
		status->value);
	BWChangeNotify(w);
	BWSetChanged(w);
    }    
    else
	if (QuerySet(status->at_x, status->at_y))
	    if (status->draw)
		(*(DrawOnePointProc)status->draw)(w,
				status->at_x, status->at_y, Highlight);
    
    XtRemoveEventHandler(w,
			 ButtonPressMask | ButtonReleaseMask | 
			 ExposureMask | PointerMotionMask,
			 FALSE, OnePointHandler, (XtPointer)status);
}

void 
OnePointTerminateTransparent(Widget w, 
			     BWStatus *status, 
			     XtPointer draw)
{
    
    if (status->success && draw)
	(*(DrawOnePointProc)draw)(w,
		status->at_x, status->at_y,
		status->value);
    else
	if (QuerySet(status->at_x, status->at_y))
	    if (status->draw)
		(*(DrawOnePointProc)status->draw)(w,
				status->at_x, status->at_y, Highlight);
    
    XtRemoveEventHandler(w,
			 ButtonPressMask | ButtonReleaseMask | 
			 ExposureMask | PointerMotionMask,
			 FALSE, OnePointHandler, (XtPointer)status);
    
}


void 
TwoPointsHandler(Widget w, 
		 XtPointer client_data, 
		 XEvent *event, 
		 Boolean *cont) /* ARGSUSED */
{
    BitmapWidget BW = (BitmapWidget) w;
    BWStatus   *status = (BWStatus *)client_data;
    if (DEBUG)
	fprintf(stderr, "2PH ");
    
    switch (event->type) {
	
    case Expose:
	if (QuerySet(status->from_x, status->from_y) && 
	    QuerySet(status->to_x, status->to_y)) {
	    BWClip(w, event->xexpose.x, event->xexpose.y,
		   event->xexpose.width, event->xexpose.height);
	    if (status->draw)
		(*(DrawTwoPointProc)status->draw)(w,
				status->from_x, status->from_y, 
				status->to_x, status->to_y, Highlight);
	    BWUnclip(w);
	}
	break;
	
    case ButtonPress:
	if (event->xbutton.state != status->state) return;
	if (!QuerySet(status->from_x, status->from_y)) {
	    status->value = Value(BW, event->xbutton.button);
	    status->btime = event->xbutton.time;
	    status->from_x = InBitmapX(BW, event->xbutton.x);
	    status->from_y = InBitmapY(BW, event->xbutton.y);
	    status->to_x = InBitmapX(BW, event->xbutton.x);
	    status->to_y = InBitmapY(BW, event->xbutton.y);
	    if (status->draw)
		(*(DrawTwoPointProc)status->draw)(w,
				status->from_x, status->from_y, 
				status->to_x, status->to_y, Highlight);
	}
	break;
	
    case ButtonRelease:
	if (QuerySet(status->from_x, status->from_y)) {
	    if (status->draw)
		(*(DrawTwoPointProc)status->draw)(w,
				status->from_x, status->from_y, 
				status->to_x, status->to_y, Highlight);
	    status->value = Value(BW, event->xbutton.button);
	    status->btime = event->xbutton.time;	    
	    status->to_x = InBitmapX(BW, event->xbutton.x);
	    status->to_y = InBitmapY(BW, event->xbutton.y);
	    status->success = True;
	    
	    BWTerminateRequest(w, TRUE);
	}
	break;
	
    case MotionNotify:
	if (QuerySet(status->from_x, status->from_y)) {
	    if (QuerySet(status->to_x, status->to_y)) {
		if (!QueryInSquare(BW, event->xmotion.x, event->xmotion.y,
				   status->to_x, status->to_y)) {
		    if (status->draw)
			(*(DrawTwoPointProc)status->draw)(w,
					status->from_x, status->from_y, 
					status->to_x, status->to_y, Highlight);
		    status->to_x = InBitmapX(BW, event->xmotion.x);
		    status->to_y = InBitmapY(BW, event->xmotion.y);
		    if (status->draw)
			(*(DrawTwoPointProc)status->draw)(w,
					status->from_x, status->from_y, 
					status->to_x, status->to_y, Highlight);
		}
	    }
	    else {
		status->to_x = InBitmapX(BW, event->xmotion.x);
		status->to_y = InBitmapY(BW, event->xmotion.y);
		if (status->draw)
		    (*(DrawTwoPointProc)status->draw)(w,
				    status->from_x, status->from_y, 
				    status->to_x, status->to_y, Highlight);
	    }
	}
	break;
    }
}

void 
TwoPointsEngage(Widget w, 
		BWStatus *status, 
		XtPointer draw, 
		int *state)
{
    
    status->from_x = NotSet;
    status->from_y = NotSet;
    status->to_x = NotSet;
    status->to_y = NotSet;
    status->draw = draw;
    status->success = False;
    status->state = *state;

    XtAddEventHandler(w,
		      ButtonPressMask | ButtonReleaseMask | 
		      ExposureMask | PointerMotionMask,
		      FALSE, TwoPointsHandler, (XtPointer)status);
}

void 
TwoPointsTerminate(Widget w, 
		   BWStatus *status, 
		   XtPointer draw)
{
    
    if (status->success && draw) {
	BWStoreToBuffer(w);
	(*(DrawTwoPointProc)draw)(w,
		status->from_x, status->from_y,
		status->to_x, status->to_y,
		status->value);
	BWChangeNotify(w);
	BWSetChanged(w);
    }
    else
	if (QuerySet(status->from_x, status->from_y) && 
	    QuerySet(status->to_x, status->to_y))
	    if (status->draw)
		(*(DrawTwoPointProc)status->draw)(w,
				status->from_x, status->from_y, 
				status->to_x, status->to_y, Highlight);
    
    XtRemoveEventHandler(w,
			 ButtonPressMask | ButtonReleaseMask | 
			 ExposureMask | PointerMotionMask,
			 FALSE, TwoPointsHandler, (XtPointer)status);
}

void 
TwoPointsTerminateTransparent(Widget w, 
			      BWStatus *status, 
			      XtPointer draw)
{
    
    if (status->success && draw)
	(*(DrawTwoPointProc)draw)(w,
		status->from_x, status->from_y,
		status->to_x, status->to_y,
		status->value);
    else
	if (QuerySet(status->from_x, status->from_y) && 
	    QuerySet(status->to_x, status->to_y))
	    if (status->draw)
		(*(DrawTwoPointProc)status->draw)(w,
				status->from_x, status->from_y, 
				status->to_x, status->to_y, Highlight);
    
    XtRemoveEventHandler(w,
			 ButtonPressMask | ButtonReleaseMask | 
			 ExposureMask | PointerMotionMask,
			 FALSE, TwoPointsHandler, (XtPointer)status);
}

void 
TwoPointsTerminateTimed(Widget w, 
			BWStatus *status, 
			XtPointer draw)
{
    
    if (status->success && draw)
	(*(DrawTwoPointProc)draw)(w,
		status->from_x, status->from_y,
		status->to_x, status->to_y,
		status->btime);
    else
	if (QuerySet(status->from_x, status->from_y) && 
	    QuerySet(status->to_x, status->to_y))
	    if (status->draw)
		(*(DrawTwoPointProc)status->draw)(w,
				status->from_x, status->from_y, 
				status->to_x, status->to_y, Highlight);
    
    XtRemoveEventHandler(w,
			 ButtonPressMask | ButtonReleaseMask | 
			 ExposureMask | PointerMotionMask,
			 FALSE, TwoPointsHandler, (XtPointer)status);
}

/* ARGSUSED */
void 
Interface(Widget w, 
	  BWStatus *status, 
	  XtPointer action)
{
    (*(InterfaceProc)action)(w);
}

void 
Paste(Widget w, 
      Position at_x, 
      Position at_y, 
      int value)
{
    BitmapWidget    BW = (BitmapWidget) w;
    BWStatus       *my_status;
    BWRequest       request;

    my_status = (BWStatus *) 
	BW->bitmap.request_stack[BW->bitmap.current].status;

    my_status->draw = NULL;

   request = (BWRequest)
   BW->bitmap.request_stack[BW->bitmap.current].request->terminate_client_data;
	
    BWTerminateRequest(w, FALSE);
    
    if ((at_x == max(BW->bitmap.mark.from_x, min(at_x, BW->bitmap.mark.to_x)))
	&&
      (at_y == max(BW->bitmap.mark.from_y, min(at_y, BW->bitmap.mark.to_y)))) {
	
	BWStatus *status;
	
	if (DEBUG)
	    fprintf(stderr, "Prepaste request: %s\n", request);
	
	BWEngageRequest(w, request, False, (char *)&(my_status->state), sizeof(int));
	
	status = (BWStatus *) 
	    BW->bitmap.request_stack[BW->bitmap.current].status;
	
	status->at_x = at_x;
	status->at_y = at_y;
	status->value = value;
	(*(DrawOnePointProc)status->draw) (w, at_x, at_y, Highlight);	
    }
    else {

	BWStatus *status;
	
      BWEngageRequest(w, MarkRequest, False, (char *)&(my_status->state), sizeof(int));
	
	status = (BWStatus *) 
	    BW->bitmap.request_stack[BW->bitmap.current].status;
	
	status->from_x = status->to_x = at_x;
	status->from_y = status->to_y = at_y;
	status->value = value;
	(*(DrawTwoPointProc)status->draw) (w, at_x, at_y, at_x, at_y, Highlight);
    }
}


void 
DragTwoPointsHandler(Widget w, 
		     XtPointer client_data, 
		     XEvent *event, 
		     Boolean *cont) /* ARGSUSED */
{
    BitmapWidget BW = (BitmapWidget) w;
    BWStatus   *status = (BWStatus *)client_data;

    if (DEBUG)
	fprintf(stderr, "D2PH ");

    switch (event->type) {
	
    case ButtonPress:
	if (event->xbutton.state != status->state) return;
	if (!QuerySet(status->from_x, status->from_y)) {
	    BWStoreToBuffer(w);
	    status->value = Value(BW, event->xbutton.button);
	    status->btime = event->xbutton.time;
	    status->from_x = InBitmapX(BW, event->xbutton.x);
	    status->from_y = InBitmapY(BW, event->xbutton.y);
	    status->to_x = InBitmapX(BW, event->xbutton.x);
	    status->to_y = InBitmapY(BW, event->xbutton.y);
	    status->success = status->draw ? True : False;
	    if (status->draw)
		(*(DrawTwoPointProc)status->draw)(w,
				status->from_x, status->from_y, 
				status->to_x, status->to_y, status->value);
	}
	break;
	
    case ButtonRelease:
	if (QuerySet(status->from_x, status->from_y)) {
	    status->value = Value(BW, event->xbutton.button);
	    status->btime = event->xbutton.time;	    
	    status->from_x = status->to_x;
	    status->from_y = status->to_y;
	    status->to_x = InBitmapX(BW, event->xbutton.x);
	    status->to_y = InBitmapY(BW, event->xbutton.y);
	    status->success = True;
	    
	    BWTerminateRequest(w, TRUE);
	}
	break;
	
    case MotionNotify:
	if (QuerySet(status->from_x, status->from_y)) {
	    if (QuerySet(status->to_x, status->to_y)) {
		if (!QueryInSquare(BW, event->xmotion.x, event->xmotion.y,
				   status->to_x, status->to_y)) {
		    status->from_x = status->to_x;
		    status->from_y = status->to_y;
		    status->to_x = InBitmapX(BW, event->xmotion.x);
		    status->to_y = InBitmapY(BW, event->xmotion.y);
		    if (status->draw)
			(*(DrawTwoPointProc)status->draw)(w,
					status->from_x, status->from_y, 
					status->to_x, status->to_y, status->value);
		}
	    }
	}
	break;
    }
}

void 
DragTwoPointsEngage(Widget w, 
		    BWStatus *status, 
		    XtPointer draw, 
		    int *state)
{
    
    status->from_x = NotSet;
    status->from_y = NotSet;
    status->to_x = NotSet;
    status->to_y = NotSet;
    status->draw = draw;
    status->success = False;
    status->state = *state;

    XtAddEventHandler(w,
		      ButtonPressMask | ButtonReleaseMask | PointerMotionMask,
		      FALSE, DragTwoPointsHandler, (XtPointer)status);
}

void 
DragTwoPointsTerminate(Widget w, 
		       BWStatus *status, 
		       XtPointer draw)
{
    
    if (status->success && draw) {
	if ((status->from_x != status->to_x) 
	    || 
	    (status->from_y != status->to_y))
	    (*(DrawTwoPointProc)draw)(w,
		    status->from_x, status->from_y,
		    status->to_x, status->to_y,
		    status->value);
	BWChangeNotify(w);
	BWSetChanged(w);
    }
    
    XtRemoveEventHandler(w,
		         ButtonPressMask | ButtonReleaseMask | PointerMotionMask,
			 FALSE, DragTwoPointsHandler, (XtPointer)status);
}

/*****************************************************************************/

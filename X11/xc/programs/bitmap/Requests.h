/* $Xorg: Requests.h,v 1.4 2001/02/09 02:05:28 xorgcvs Exp $ */
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
/* $XFree86: xc/programs/bitmap/Requests.h,v 1.5 2001/12/14 20:00:41 dawes Exp $ */

/*
 * Author:  Davor Matic, MIT X Consortium
 */

#ifndef _Requests_h
#define _Requests_h

typedef int (*DrawOnePointProc)(Widget, Position, Position, int);
typedef int (*DrawTwoPointProc)(Widget, Position, Position,
				Position, Position, int);
typedef int (*InterfaceProc)(Widget);

typedef struct {
    Boolean  success;
    Position at_x, at_y;
    Position from_x, from_y,
             to_x, to_y;
    XtPointer draw;
    int      value;
    Time     btime;
    int      state;
} BWStatus;

extern void OnePointEngage ( Widget w, BWStatus *status, XtPointer draw, int *state );
extern void OnePointTerminate ( Widget w, BWStatus *status, XtPointer draw );
extern void OnePointTerminateTransparent ( Widget w, BWStatus *status, XtPointer draw );
extern void DragOnePointEngage ( Widget w, BWStatus *status, XtPointer draw, int *state );
extern void DragOnePointTerminate ( Widget w, BWStatus *status, XtPointer client_data );
extern void TwoPointsEngage ( Widget w, BWStatus *status, XtPointer draw, int *state );
extern void TwoPointsTerminate ( Widget w, BWStatus *status, XtPointer draw );
extern void TwoPointsTerminateTransparent ( Widget w, BWStatus *status, XtPointer draw );
extern void TwoPointsTerminateTimed ( Widget w, BWStatus *status, XtPointer draw );
extern void DragTwoPointsEngage ( Widget w, BWStatus *status, XtPointer draw, int *state );
extern void DragTwoPointsTerminate ( Widget w, BWStatus *status, XtPointer draw );
extern void Interface ( Widget w, BWStatus *status, XtPointer action );
extern void Paste ( Widget w, Position at_x, Position at_y, int value );

extern void BWDragMarked(Widget w, Position at_x, Position at_y);
extern void BWDragStored(Widget w, Position at_x, Position at_y);
extern void BWCopy(Widget w, Position at_x, Position at_y, int value);
extern void BWMove(Widget w, Position at_x, Position at_y, int value);
extern void BWRestore(Widget w, Position at_x, Position at_y, int value);
extern void BWDrawPoint(Widget w, Position x, Position y, bit value);
extern void BWBlindLine(Widget w, Position from_x, Position from_y, 
			Position to_x, Position to_y, int value);
extern void BWDrawHotSpot(Widget w, Position x, Position y, int value);
extern void BWZoomIn(Widget w, Position from_x, Position from_y, 
		     Position to_x, Position to_y);

extern void OnePointHandler(Widget w, XtPointer client_data, 
			    XEvent *event, Boolean *cont);
extern void TwoPointsHandler(Widget w, XtPointer client_data, 
			     XEvent *event, Boolean *cont);
extern void DragTwoPointsHandler(Widget w, XtPointer client_data, 
				 XEvent *event, Boolean *cont);

#endif /* _Requests_h */

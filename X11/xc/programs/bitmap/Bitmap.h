/* $Xorg: Bitmap.h,v 1.4 2001/02/09 02:05:28 xorgcvs Exp $ */
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
/* $XFree86: xc/programs/bitmap/Bitmap.h,v 1.5 2001/12/14 20:00:40 dawes Exp $ */

/*
 * Author:  Davor Matic, MIT X Consortium
 */


#ifndef _Bitmap_h
#define _Bitmap_h

/****************************************************************
 *
 * Bitmap widget
 *
 ****************************************************************/

#include <X11/Xaw/Simple.h>

/* Resources:

 Name		     Class		RepType		Default Value
 ----		     -----		-------		------------- 
 background	     Background		Pixel		XtDefaultBackground
 foreground          Foredround         Pixel           XtDefaultForeground
 highlight           Highlight          Pixel           XtDefaultForeground
 frame               Frame              Pixel           XtDefaultForeground
 border		     BorderColor	Pixel		XtDefaultForeground
 borderWidth	     BorderWidth	Dimension	1
 mappedWhenManaged   MappedWhenManaged	Boolean		True
 resize              Resize             Boolean         True
 sensitive	     Sensitive		Boolean		True
 width		     Width		Dimension	0
 height		     Height		Dimension	0
 size                Size               String          32x32
 squareWidht         SquareWidht        Dimension       16
 squareHeight        SquareHeight       Dimension       16
 x		     Position		Position	320
 y		     Position		Position	320
 xHot                XHot               Position        NotSet
 yHot                YHot               Position        NotSet
 margin              Margin             Dimension       16
 grid                Grid               Boolean         True
 gridTolerance       GridTolerance      Dimension       8
 dashed              Dashed             Boolean         True
 dashes              Dashes             Bitmap          XtUnspecifiedPixmap
 stippled            Stippled           Boolean         True
 stipple             Stipple            Bitmap          XtUnspecifiedPixmap
 proportional        Proportional       Boolean         True
 axes                Axes               Boolean         True
 button1Function     Button1Function    ButtonFunction  Set  
 button2Function     Button2Function    ButtonFunction  Invert
 button3Function     Button3Function    ButtonFunction  Clear
 button4Function     Button4Function    ButtonFunction  Invert
 button5Function     Button5Function    ButtonFunction  Invert
 filename            Filename           String          None
 basename            Basename           String          None
*/

/* define any special resource names here that are not in <X11/StringDefs.h> */

#define XtNbitmapResource "bitmapResource"
#define XtNstipple "stipple"
#define XtNstippled "stippled"
#define XtNdashes "dashes"
#define XtNdashed "dashed"
#define XtNgrid "grid"
#define XtNgridTolerance "gridTolerance"
#define XtNaxes "axes"
#define XtNbitmapSize "bitmapSize"
#define XtNsize "size"
#define XtNsquareWidth "squareWidth"
#define XtNsquareHeight "squareHeight"
#define XtNxHot "xHot"
#define XtNyHot "yHot"
#define XtNbutton1Function "button1Function"
#define XtNbutton2Function "button2Function"
#define XtNbutton3Function "button3Function"
#define XtNbutton4Function "button4Function"
#define XtNbutton5Function "button5Function"
#define XtNfilename "filename"
#define XtNbasename "basename"
#define XtNmouseForeground "mouseForeground"
#define XtNmouseBackground "mouseBackground"
#define XtNframe "frame"
#define XtNmargin "margin"
#define XtNproportional "proportional"

#define XtCBitmapResource "BitmapResource"
#define XtCHighlight "Highlight"
#define XtCStipple "Stipple"
#define XtCStippled "Stippled"
#define XtCDashes "Dashes"
#define XtCDashed "Dashed"
#define XtCGrid "Grid"
#define XtCGridTolerance "GridTolerance"
#define XtCAxes "Axes"
#define XtBitmapSize "BitmapSize"
#define XtCSize "Size"
#define XtCSquareWidth "SquareWidth"
#define XtCSquareHeight "SquareHeight"
#define XtCXHot "XHot"
#define XtCYHot "YHot"
#define XtCButton1Function "Button1Function"
#define XtCButton2Function "Button2Function"
#define XtCButton3Function "Button3Function"
#define XtCButton4Function "Button4Function"
#define XtCButton5Function "Button5Function"
#define XtCFilename "Filename"
#define XtCBasename "Basename"
#define XtCFrame "Frame"
#ifndef XtCMargin
#define XtCMargin "Margin"
#endif
#define XtCProportional "Proportional"

#define XtRButtonFunction "ButtonFunction"

/* bitmap defines */

#define NotSet   -1
#define Clear     0
#define Set       1
#define Invert    2
#define Highlight 3
#define On        True
#define Off       False

#define XtClear "clear"
#define XtSet "set"
#define XtInvert "invert"

#define MarkRequest "MarkRequest"
#define StoreRequest "StoreRequest"
#define RestoreRequest "RestoreRequest"
#define CopyRequest "CopyRequest"
#define MoveRequest "MoveRequest"
#define PointRequest "PointRequest"
#define LineRequest "LineRequest"
#define CurveRequest "CurveRequest"
#define RectangleRequest "RectangleRequest"
#define FilledRectangleRequest "FilledRectangleRequest"
#define CircleRequest "CircleRequest"
#define FilledCircleRequest "FilledCircleRequest"
#define FloodFillRequest "FloodFillRequest"
#define HotSpotRequest "HotSpotRequest"
#define ZoomInRequest "ZoomInRequest"
#define PasteRequest "PasteRequest"
#define ImmediateCopyRequest "ImmediateCopyRequest"
#define ImmediateMoveRequest "ImmediateMoveRequest"

/* bitmap exports */

typedef struct _BWRequestRec BWRequestRec;
typedef char *BWRequest;

/* declare specific BitmapWidget class and instance datatypes */

typedef struct _BitmapClassRec *BitmapWidgetClass;
typedef struct _BitmapRec      *BitmapWidget;
/* declare the class constant */

extern WidgetClass bitmapWidgetClass;

extern Boolean BWEngageRequest(Widget w, BWRequest name, Boolean trap, 
			       XtPointer call_data, Cardinal call_data_size);
extern Boolean BWTerminateRequest(Widget w, Boolean cont);

extern void BWUp ( Widget w );
extern void BWDown ( Widget w );
extern void BWLeft ( Widget w );
extern void BWRight ( Widget w );
extern void BWRotateRight ( Widget w );
extern void BWRotateLeft ( Widget w );
extern void BWSwitchGrid ( Widget w );
extern void BWGrid ( Widget w, Boolean _switch );
extern void BWSwitchDashed ( Widget w );
extern void BWDashed ( Widget w, Boolean _switch );
extern void BWSwitchAxes ( Widget w );
extern void BWAxes ( Widget w, Boolean _switch );
extern void BWRedrawAxes( Widget w );
extern void BWDrawLine ( Widget w, Position from_x, Position from_y, Position to_x, Position to_y, int value );
extern void BWDrawRectangle ( Widget w, Position from_x, Position from_y, Position to_x, Position to_y, int value );
extern void BWDrawFilledRectangle ( Widget w, Position from_x, Position from_y, Position to_x, Position to_y, int value );
extern void BWDrawCircle ( Widget w, Position origin_x, Position origin_y, Position point_x, Position point_y, int value );
extern void BWDrawFilledCircle ( Widget w, Position origin_x, Position origin_y, Position point_x, Position point_y, int value );
extern void BWFloodFill ( Widget w, Position x, Position y, int value );
extern void BWMark ( Widget w, Position from_x, Position from_y, Position to_x, Position to_y );
extern void BWMarkAll ( Widget w );
extern void BWUnmark ( Widget w );
extern void BWSelect ( Widget w, Position from_x, Position from_y, Position to_x, Position to_y, Time btime );
extern void BWStore ( Widget w );
extern void BWStoreToBuffer ( Widget w );
extern void BWUndo ( Widget w );
extern void BWResize ( Widget w, Dimension width, Dimension height );
extern void BWClip ( Widget w, Position x, Position y, Dimension width, Dimension height );
extern void BWUnclip ( Widget w );
extern void BWGrabSelection ( Widget w, Time btime );
extern void BWRequestSelection ( Widget w, Time btime, Boolean wait );
extern void BWSetChanged ( Widget w );
extern Boolean BWQueryChanged ( Widget w );
extern int BWReadFile ( Widget w, String filename, String basename );
extern int BWWriteFile ( Widget w, String filename, String basename );
extern String BWUnparseStatus ( Widget w );
extern String BWGetFilename ( Widget w, String *str );
extern String BWGetBasename ( Widget w, String *str );
extern void BWChangeBasename ( Widget w, String str );
extern void BWRemoveAllRequests ( Widget w );
extern void BWClearHotSpot ( Widget w );
extern Boolean BWQueryMarked ( Widget w );
extern void BWFold ( Widget w );
extern void BWClear ( Widget w );
extern void BWSet ( Widget w );
extern void BWInvert ( Widget w );
extern void BWFlipHoriz ( Widget w );
extern void BWFlipVert ( Widget w );
extern void BWClearMarked ( Widget w );
extern Boolean BWAddRequest ( Widget w, BWRequest name, Boolean trap, XtPointer call_data, Cardinal call_data_size );
extern void BWChangeNotify ( Widget w );
extern Pixmap BWGetUnzoomedPixmap ( Widget w );
extern void BWClearChanged ( Widget w );
extern Boolean BWQueryStored ( Widget w );
extern Boolean BWQueryStippled ( Widget w );
extern void BWSwitchStippled ( Widget w );
extern void BWRedrawMark ( Widget w );
extern Boolean BWQueryAxes ( Widget w );
extern void BWHighlightAxes ( Widget w );
extern String BWGetFilepath ( Widget w, String *str );
extern void BWZoomOut ( Widget w );
extern void BWZoomMarked ( Widget w );
extern void BWRescale ( Widget w, Dimension width, Dimension height );
extern Boolean BWQueryZooming ( Widget w );
extern void BWRedrawGrid ( Widget w, Position x, Position y, Dimension width, Dimension height );
extern void BWRedrawSquares ( Widget w, Position x, Position y, Dimension width, Dimension height );
extern void BWRedrawHotSpot ( Widget w );
extern void BWSetHotSpot(Widget w, Position x, Position y);
extern Boolean BWQueryGrid ( Widget w );
extern Boolean BWQueryDashed ( Widget w );
extern Boolean BWQueryProportional ( Widget w );
extern void BWSwitchProportional ( Widget w );
extern void BWDrawGrid ( Widget w, Position from_x, Position from_y, Position to_x, Position to_y );
extern void BWChangeFilename ( Widget w, String str );
extern Boolean BWParseSize ( String size, Dimension *width, Dimension *height );
extern Boolean BWQuerySelection ( Widget w, Time btime );
extern int BWStoreFile ( Widget w, String filename, String *basename );
extern void BWNotify ( Widget w, XtActionProc proc );
extern void BWTMark ( Widget w, XEvent *event, String *params, Cardinal *num_params  );
extern void BWTMarkAll ( Widget w, XEvent *event, String *params, Cardinal *num_params );
extern void BWTUnmark ( Widget w, XEvent *event, String *params, Cardinal *num_params );
extern void BWTPaste ( Widget w, XEvent *event, String *params, Cardinal *num_params );
extern void BWDebug ( Widget w, XEvent *event, String *params, Cardinal *num_params );
extern void BWAbort ( Widget w );
extern Boolean BWRemoveRequest ( Widget w );
extern void BWRedraw ( Widget w );
extern Pixmap BWGetPixmap( Widget w );

#endif /* _Bitmap_h */



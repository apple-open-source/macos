/* $Xorg: BitmapP.h,v 1.4 2001/02/09 02:05:28 xorgcvs Exp $ */
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
/* $XFree86: xc/programs/bitmap/BitmapP.h,v 1.4 2001/12/14 20:00:40 dawes Exp $ */

/*
 * Author:  Davor Matic, MIT X Consortium
 */



#ifndef _BitmapP_h
#define _BitmapP_h

#define bit int

#include "Bitmap.h"
#include "Requests.h"
#include <X11/Xaw/SimpleP.h>

typedef struct {
    Atom           *targets;
    Cardinal        num_targets;
    BWRequestRec   *requests;
    Cardinal        num_requests;
    BWRequestRec   *request[100];
  
} BitmapClassPart;

/* Full class record declaration */
typedef struct _BitmapClassRec {
  CoreClassPart          core_class;
  SimpleClassPart        simple_class;
  BitmapClassPart        bitmap_class;
} BitmapClassRec;

extern BitmapClassRec bitmapClassRec;

typedef void (*EngageProc)(Widget, BWStatus *, XtPointer, int *);
typedef void (*TerminateProc)( Widget, BWStatus *, XtPointer);
typedef void (*RemoveProc)(Widget w, BWStatus *, XtPointer);

/**********/
struct _BWRequestRec {
  char         *name;
  int           status_size;
  EngageProc    engage;
  XtPointer     engage_client_data;
  TerminateProc terminate;
  XtPointer     terminate_client_data;
  RemoveProc    remove;
  XtPointer     remove_client_data;
} ;

typedef struct {
  Position from_x, from_y,
           to_x, to_y;
} BWArea;

typedef struct {
    BWRequestRec *request;
    XtPointer     status;
    Boolean       trap;
    XtPointer     call_data;
} BWRequestStack;

typedef struct {
    XImage   *image, *buffer;
    XPoint    hot;
    Position  at_x, at_y;
    Boolean   fold;
    Boolean   grid;
    Boolean   changed;
} BWZoom;

typedef struct {
    Boolean   own;
    Boolean   limbo;
} BWSelection;

/* New fields for the Bitmap widget record */
typedef struct {
  /* resources */
  Pixel            foreground_pixel;
  Pixel            highlight_pixel;
  Pixel            frame_pixel;
  Pixmap           stipple;
  Boolean          stippled;
  Boolean          proportional;
  Boolean          grid;
  Dimension        grid_tolerance;
  Pixmap           dashes;
  Boolean          dashed;
  Boolean          axes;
  Boolean          resize;
  Dimension        margin, squareW, squareH, width, height;
  XPoint           hot;
  int              button_function[5];
  String           filename, basename;
  /* private state */
  String           size;
  Position         horizOffset, vertOffset;
  XtActionProc     notify;
  BWRequestStack  *request_stack;
  Cardinal         cardinal, current;
  /*Boolean          trapping;*/
  XImage          *image, *buffer, *storage;
  XPoint           buffer_hot;
  BWArea           mark, buffer_mark;
  GC               drawing_gc;
  GC               highlighting_gc;
  GC               frame_gc;
  GC               axes_gc;
  Boolean          changed;
  Boolean          fold;
  Boolean          zooming;
  BWZoom           zoom;
  XtPointer        *value;
  char             status[80];
  BWSelection      selection;
  Boolean          stipple_change_expose_event;
} BitmapPart;

/* Full instance record declaration */
typedef struct _BitmapRec {
  CorePart      core;
  SimplePart    simple;
  BitmapPart    bitmap;
} BitmapRec;

/* Private functions */

#define Length(width, height)\
        (((int)(width) + 7) / 8 * (height))

#define InBitmapX(BW, x)\
	(Position)(min((Position)((Dimension)(max(BW->bitmap.horizOffset,x)  -\
				   BW->bitmap.horizOffset) /\
				   BW->bitmap.squareW), BW->bitmap.width - 1))
    
#define InBitmapY(BW, y)\
	(Position)(min((Position)((Dimension)(max(BW->bitmap.vertOffset, y)  -\
				   BW->bitmap.vertOffset) /\
				   BW->bitmap.squareH), BW->bitmap.height - 1))
    
#define InWindowX(BW, x)\
	(Position) (BW->bitmap.horizOffset + ((x) * BW->bitmap.squareW))

#define InWindowY(BW, y)\
	(Position) (BW->bitmap.vertOffset + ((y) * BW->bitmap.squareH))
     
#define GetPixmap(BW, image)\
    XCreateBitmapFromData(XtDisplay(BW), XtWindow(BW),\
			  image->data, image->width, image->height)


#define QuerySet(x, y) (((x) != NotSet) && ((y) != NotSet))

#define bit int

#define QueryZero(x, y) (((x) == 0) || ((y) == 0))

#define Swap(x, y) {Position t; t = x; x = y; y = t;}

#define QuerySwap(x, y) if(x > y) Swap(x, y)

#define QueryInBitmap(BW, x, y)\
  (((x) >= 0) && ((x) < BW->bitmap.image->width) &&\
   ((y) >= 0) && ((y) < BW->bitmap.image->height))

#define Value(BW, button)   (BW->bitmap.button_function[button - 1])

#define CreateCleanData(length) XtCalloc(length, sizeof(char))
XImage *CreateBitmapImage(BitmapWidget BW, char *data, Dimension width, Dimension height);
void DestroyBitmapImage(XImage **image);
void  TransferImageData(XImage *source, XImage *destination);
void CopyImageData(XImage *source, XImage *destination, 
	      Position from_x, Position from_y, 
	      Position to_x, Position to_y, 
	      Position at_x, Position at_y);
XImage *GetImage(BitmapWidget BW, Pixmap pixmap);
XImage *ConvertToBitmapImage(BitmapWidget BW, XImage *image);
XImage *ScaleBitmapImage(BitmapWidget BW, XImage *src, 
			 double scale_x, double scale_y);

#endif /* _BitmapP_h */

/* $Xorg: ScaleP.h,v 1.4 2001/02/09 02:05:55 xorgcvs Exp $ */
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

#ifndef _ScaleP_h
#define _ScaleP_h

#include "Scale.h"
#include <X11/Xaw/SimpleP.h>

typedef struct {
    int foo;
} ScaleClassPart;

/* Full class record declaration */
typedef struct _ScaleClassRec {
  CoreClassPart          core_class;
  SimpleClassPart        simple_class;
  ScaleClassPart         scale_class;
} ScaleClassRec;

extern ScaleClassRec scaleClassRec;

typedef struct {
    Position *x, *y;
    Dimension *width, *height;
} Table;

/* New fields for the Scale widget record */

#ifndef XtGravity
#define  XtGravity int
#endif

typedef struct {
  /* resources */
    Pixel       foreground_pixel;
    Dimension   internal_width;
    Dimension   internal_height;
    XtGravity   gravity;
    String      scale_x_str, scale_y_str;
    String      aspect_ratio_str;
    String      precision_str;
    XImage      *image;
    Boolean     resize;
    Boolean     autoscale;
    Boolean     proportional;
    Boolean     paste_buffer;
    Cardinal    buffer_size;
    XtPointer   userData;
    Visual      *visual;
  /* private */
    float       scale_x, scale_y;
    float       aspect_ratio;
    float       precision;
    GC          gc;
    Position    x, y;
    Dimension   width, height;
    Table       table;
    XRectangle  *rectangles;
    Cardinal    nrectangles;
} ScalePart;

/* Full instance record declaration */
typedef struct _ScaleRec {
  CorePart      core;
  SimplePart    simple;
  ScalePart scale;
} ScaleRec;

#endif /* _ScaleP_h */





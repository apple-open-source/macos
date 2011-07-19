/* $Xorg: LEDP.h,v 1.3 2000/08/17 19:54:51 cpqbld Exp $ */
/************************************************************
 Copyright (c) 1995 by Silicon Graphics Computer Systems, Inc.

 Permission to use, copy, modify, and distribute this
 software and its documentation for any purpose and without
 fee is hereby granted, provided that the above copyright
 notice appear in all copies and that both that copyright
 notice and this permission notice appear in supporting
 documentation, and that the name of Silicon Graphics not be 
 used in advertising or publicity pertaining to distribution 
 of the software without specific prior written permission.
 Silicon Graphics makes no representation about the suitability 
 of this software for any purpose. It is provided "as is"
 without any express or implied warranty.
 
 SILICON GRAPHICS DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS 
 SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY 
 AND FITNESS FOR A PARTICULAR PURPOSE. IN NO EVENT SHALL SILICON
 GRAPHICS BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL 
 DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, 
 DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE 
 OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION  WITH
 THE USE OR PERFORMANCE OF THIS SOFTWARE.

 ********************************************************/
#ifndef _XawLEDP_h
#define _XawLEDP_h

#include "LED.h"
#include <X11/Xaw/SimpleP.h>

/* New fields for the Label widget class record */

typedef struct {int foo;} LEDClassPart;

/* Full class record declaration */
typedef struct _LEDClassRec {
    CoreClassPart	core_class;
    SimpleClassPart	simple_class;
    LEDClassPart	led_class;
} LEDClassRec;

extern LEDClassRec ledClassRec;

/* New fields for the LED widget record */
typedef struct {
    /* resources */
    Pixel	foreground;
    Pixel	on_color;
    Pixel	off_color;
    Pixel	top_color;
    Pixel	bottom_color;
    Dimension	bevel;
    Dimension	led_width;
    Dimension	led_height;
    Boolean	on;

    /* private state */
    Pixmap	on_pixmap;
    Pixmap      off_pixmap;
} LEDPart;

/****************************************************************
 *
 * Full instance record declaration
 *
 ****************************************************************/

typedef struct _LEDRec {
    CorePart	core;
    SimplePart	simple;
    LEDPart	led;
} LEDRec;

#endif /* _XawLEDP_h */

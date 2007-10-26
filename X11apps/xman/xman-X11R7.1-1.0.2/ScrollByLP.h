/* $XConsortium: ScrollByLP.h,v 1.9 94/04/17 20:43:49 keith Exp $ */
/*

Copyright (c) 1987, 1988  X Consortium

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE X CONSORTIUM BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of the X Consortium shall
not be used in advertising or otherwise to promote the sale, use or
other dealings in this Software without prior written authorization
from the X Consortium.

*/


#ifndef _XtScrollByLinePrivate_h
#define _XtScrollByLinePrivate_h

#include <X11/Xaw/SimpleP.h>

#include "ScrollByL.h"

/***********************************************************************
 *
 * ScrollByLine Widget Private Data
 *
 ***********************************************************************/

/* New fields for the ScrollByLine widget class record */
typedef struct {
     int mumble;   /* No new procedures */
} ScrollByLineClassPart;

/* Full class record declaration */
typedef struct _ScrollByLineClassRec {
    CoreClassPart	  core_class;
    SimpleClassPart       simple_class;
    ScrollByLineClassPart scrolled_widget_class;
} ScrollByLineClassRec;

extern ScrollByLineClassRec scrollByLineClassRec;

/* New fields for the ScrollByLine widget record */
typedef struct _ScrollByLinePart {
  Pixel foreground;		/* The color for the forground of the text. */
  Boolean force_vert,		/* Must have scrollbar visable */
    use_right;			/* put scroll bar on right side of window. */
  FILE * file;			/* The file to display. */
  Dimension indent;		/* amount to indent the file. */
  XFontStruct * bold_font,	/* The four fonts. */
    * normal_font,
    * italic_font,
    * symbol_font;
  int h_width;			/* Main font width */
  
/* variables not in resource list. */

  Widget bar;			/* The scrollbar. */
  int font_height;		/* the height of the font. */
  int line_pointer;		/* The line that currently is at the top 
				   of the window being displayed. */
  Dimension offset;		/* Drawing offset because of scrollbar. */
  GC move_gc;			/* GC to use when moving the text. */
  GC bold_gc, normal_gc, italic_gc, symbol_gc; /* gc for drawing. */

  char ** top_line;		/* The top line of the file. */
  int lines;		/* Total number of line in the file. */
  int num_visible_lines;  /* Number of lines visible */
} ScrollByLinePart;

/****************************************************************
 *
 * Full instance record declaration
 *
 ****************************************************************/

typedef struct _ScrollByLineRec {
    CorePart	      core;
    SimplePart        simple;
    ScrollByLinePart  scroll;
} ScrollByLineRec;

#endif /* _XtScrollByLinePrivate_h --- DON'T ADD STUFF AFTER THIS LINE */

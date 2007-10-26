/* $Xorg: gridP.h,v 1.4 2001/02/09 02:05:41 xorgcvs Exp $ */
/*

Copyright 1993, 1998  The Open Group

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
/* $XFree86$ */

#ifndef _FontGridP_h_
#define _FontGridP_h_

#include "grid.h"

typedef struct _FontGridClassPart { int dummy; } FontGridClassPart;

typedef struct _FontGridClassRec {
    CoreClassPart	core_class;
    SimpleClassPart	simple_class;
    FontGridClassPart	grid_class;
} FontGridClassRec;
extern FontGridClassRec fontgridClassRec;

typedef struct _FontGridPart {
    XFontStruct *	text_font;		/* font to display */
    int			cell_cols, cell_rows;  /* number of cells */
    int			cell_width, cell_height;  /* size of cell */
#ifndef XRENDER
    Pixel		foreground_pixel;	/* color of text */
#endif
    Pixel		box_pixel;	/* for box_chars */
    Boolean		center_chars;	/* center characters in grid */
    Boolean		box_chars;	/* put box around logical width */
    XtCallbackList	callbacks;	/* for notifying caller */
    int			internal_pad;	/* extra padding inside grid */
    long		start_char;	/* first character of grid */
    int			grid_width;	/* width of grid lines */
    /* private data */
    GC			text_gc;	/* printing text */
    GC			box_gc;		/* for box_chars */
    int			xoff, yoff;	/* extra offsets within grid */
#ifdef XRENDER
    XftDraw		*draw;
    XftColor		fg_color;
    XftFont		*text_face;
#endif
} FontGridPart;

typedef struct _FontGridRec {
    CorePart		core;
    SimplePart		simple;
    FontGridPart	fontgrid;
} FontGridRec;

#ifdef XRENDER

#define GridFontHeight(g)   ((g)->fontgrid.text_face ? \
			     (g)->fontgrid.text_face->height : \
			     (g)->fontgrid.text_font ? \
			     (g)->fontgrid.text_font->ascent + \
			     (g)->fontgrid.text_font->descent : 1)
#define GridFontAscent(g)   ((g)->fontgrid.text_face ? \
			     (g)->fontgrid.text_face->ascent : \
			     (g)->fontgrid.text_font ? \
			     (g)->fontgrid.text_font->ascent: 1)
#define GridFontWidth(g)    ((g)->fontgrid.text_face ? \
			     (g)->fontgrid.text_face->max_advance_width : \
			     (g)->fontgrid.text_font ? \
			     (g)->fontgrid.text_font->max_bounds.width : 1)
#define GridForeground(g)   ((g)->fontgrid.fg_color.pixel)
			     
#else /* XRENDER */
			     
#define GridFontHeight(g)   ((g)->fontgrid.text_font->ascent + \
			     (g)->fontgrid.text_font->descent)
#define GridFontAscent(g)   ((g)->fontgrid.text_font ? \
			     (g)->fontgrid.text_font->ascent: 1)
#define GridFontWidth(g)    ((g)->fontgrid.text_font->max_bounds.width)
#define GridForeground(g)   ((g)->fontgrid.foreground_pixel)
			     
#endif /* else XRENDER */

#define DefaultCellWidth(fgw) (GridFontWidth(fgw) \
			       + ((fgw)->fontgrid.internal_pad * 2))
#define DefaultCellHeight(fgw) (GridFontHeight(fgw) + \
				((fgw)->fontgrid.internal_pad * 2))

#define CellWidth(fgw) (((int)(fgw)->core.width - (fgw)->fontgrid.grid_width) \
			/ (fgw)->fontgrid.cell_cols \
			- (fgw)->fontgrid.grid_width)
#define CellHeight(fgw) (((int)(fgw)->core.height - (fgw)->fontgrid.grid_width)\
			 / (fgw)->fontgrid.cell_rows \
			 - (fgw)->fontgrid.grid_width)

#endif /* !_FontGridP_h_ */

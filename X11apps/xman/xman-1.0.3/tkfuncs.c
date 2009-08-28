/* $XConsortium: tkfuncs.c,v 1.6 94/04/17 20:43:59 rws Exp $ */
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
/* $XFree86$ */

#include <X11/X.h>
#include <X11/Xos.h>
#include <X11/Xlib.h>
#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <X11/IntrinsicP.h>

#include "globals.h"

/* 
 * I am doing the "wrong" thing here by looking in the core field for the
 * widget to get this info, the "right" thing to do is to do a XtGetValues
 * to get this information.
 */

/*	Function Name: Width
 *	Description: finds the width of a widget.
 *	Arguments: w - the widget.
 *	Returns: the width of that widget.
 */

int
Width(Widget w)
{
  return( (int) w->core.width );
}

/*	Function Name: Height
 *	Description: finds the height of a widget.
 *	Arguments: w - the widget.
 *	Returns: the height of that widget.
 */

int
Height(Widget w)
{
  return( (int) w->core.height );
}

/*	Function Name: BorderWidth
 *	Description: finds the BorderWidth of a widget.
 *	Arguments: w - the widget.
 *	Returns: the BorderWidth of that widget.
 */

int
BorderWidth(Widget w)
{
  return( (int) w->core.border_width );
}

/* 
 * These functions have got to be able to get at the widget tree, I don't see
 * any way around this one.
 */

/*	Function Name: Name
 *	Description: This function returns the correct popup child
 *	Arguments: w - widget
 *	Returns: the popup child.
 */

char * 
Name(Widget w)
{
  return( w->core.name);
}

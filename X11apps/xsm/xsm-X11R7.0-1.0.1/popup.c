/* $Xorg: popup.c,v 1.4 2001/02/09 02:06:01 xorgcvs Exp $ */

/*

Copyright 1994, 1998  The Open Group

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
/* $XFree86: xc/programs/xsm/popup.c,v 1.4 2001/01/17 23:46:30 dawes Exp $ */

#include "xsm.h"
#include <X11/Shell.h>
#include "popup.h"


void
PopupPopup(Widget parent, Widget popup, Bool transient, Bool first_time,
    int offset_x, int offset_y, String delAction)
{
    if (!transient && !first_time)
    {
	/*
	 * For non-transient windows, if this isn't the first time
	 * it's being popped up, just pop it up in the old position.
	 */

	XtPopup (popup, XtGrabNone);
	return;
    }
    else
    {
	Position parent_x, parent_y;
	Position root_x, root_y;
	Position popup_x, popup_y;
	Dimension parent_width, parent_height, parent_border;
	Dimension popup_width, popup_height, popup_border;
	char geom[16];
	Bool repos = 0;

	/*
	 * The window we pop up must be visible on the screen.  We first
	 * try to position it at the desired location (relative to the
	 * parent widget).  Once we are able to compute the popup's
	 * geometry (after it's realized), we can determine if we need
	 * to reposition it.
	 */

	XtVaGetValues (parent,
	    XtNx, &parent_x,
	    XtNy, &parent_y,
	    XtNwidth, &parent_width,
	    XtNheight, &parent_height,
	    XtNborderWidth, &parent_border,
	    NULL);

	XtTranslateCoords (parent, parent_x, parent_y, &root_x, &root_y);

	popup_x = root_x + offset_x;
	popup_y = root_y + offset_y;

	if (transient)
	{
	    XtVaSetValues (popup,
	        XtNx, popup_x,
	        XtNy, popup_y,
                NULL);
	}
	else
	{
	    sprintf (geom, "+%d+%d", popup_x, popup_y);

	    XtVaSetValues (popup,
	        XtNgeometry, geom,
                NULL);
	}

	if (first_time)
	{
	    /*
	     * Realize it for the first time
	     */

	    XtRealizeWidget (popup);


	    /*
	     * Set support for WM_DELETE_WINDOW
	     */

	    (void) SetWM_DELETE_WINDOW (popup, delAction);
	}

	/*
	 * Now make sure it's visible.
	 */

	XtVaGetValues (popup,
	    XtNwidth, &popup_width,
	    XtNheight, &popup_height,
	    XtNborderWidth, &popup_border,
	    NULL);

	popup_border <<= 1;

	if ((int) (popup_x + popup_width + popup_border) >
	    WidthOfScreen (XtScreen (topLevel)))
	{
	    popup_x = WidthOfScreen (XtScreen (topLevel)) -
		popup_width - popup_border - parent_width - parent_border;

	    repos = 1;
	}

	if ((int) (popup_y + popup_height + popup_border) >
	    HeightOfScreen (XtScreen (topLevel)))
	{
	    popup_y = HeightOfScreen (XtScreen (topLevel)) -
		popup_height - popup_border - parent_height - parent_border;

	    repos = 1;
	}

	if (repos)
	{
	    if (transient)
	    {
		XtVaSetValues (popup,
	            XtNx, popup_x,
	            XtNy, popup_y,
                    NULL);
	    }
	    else
	    {
		/*
		 * The only way we can reposition a non-transient
		 * is by unrealizing it, setting the position, then
		 * doing a realize.
		 */

		XtUnrealizeWidget (popup);

		sprintf (geom, "+%d+%d", popup_x, popup_y);

		XtVaSetValues (popup,
	            XtNgeometry, geom,
                    NULL);

		XtRealizeWidget (popup);

		(void) SetWM_DELETE_WINDOW (popup, delAction);
	    }
	}

	XtPopup (popup, XtGrabNone);
    }
}

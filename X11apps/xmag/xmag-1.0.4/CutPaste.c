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
#include <X11/Xmu/StdSel.h>
#include <X11/Xmu/Atoms.h>
#include <X11/Xatom.h>
#include "ScaleP.h"	/* This file should be part of the Scale widget */
#include "Scale.h"
#include "CutPaste.h"
#include <stdio.h>


/*ARGSUSED*/
static Boolean
ConvertSelection(Widget w, Atom *selection, Atom *target, Atom *type, 
		 XtPointer *value, unsigned long *length, int *format)
{
    Boolean success;

    if (*target == XA_PIXMAP || *target == XA_BITMAP) {
	ScaleWidget sw = (ScaleWidget) w;
	Pixmap *pixmap = (Pixmap *) XtMalloc(sizeof(Pixmap));
	*pixmap = XCreatePixmap(XtDisplay(w), XtWindow(w),
				sw->scale.image->width, 
				sw->scale.image->height, 
				sw->scale.image->depth);
	XPutImage(XtDisplay(w), *pixmap, sw->scale.gc, sw->scale.image,
		  0, 0, 0, 0, sw->scale.image->width, sw->scale.image->height);
	*type = XA_PIXMAP;
	*value = (XtPointer) pixmap;
	*length = 1;
	*format = 32;
	success = True;
    } else {
	/* Xt will always respond to selection requests for the TIMESTAMP
	   target, so we can pass a bogus time to XmuConvertStandardSelection.
	   In addition to the targets provided by XmuConvertStandardSelection,
	   Xt converts MULTIPLE, and we convert PIXMAP and BITMAP.
	 */
	success = XmuConvertStandardSelection(w, (Time)0, selection, target,
					      type, (XPointer *)value, length,
					      format);
	if (success && *target == XA_TARGETS(XtDisplay(w))) {
	    Atom* tmp;
	    tmp = (Atom *) XtRealloc(*value, (*length + 3) * sizeof(Atom));
	    tmp[(*length)++] = XInternAtom(XtDisplay(w), "MULTIPLE", False);
	    tmp[(*length)++] = XA_PIXMAP;
	    tmp[(*length)++] = XA_BITMAP;
	    *value = (XtPointer) tmp;
	}
    }
    return success;
}

void 
SWGrabSelection(Widget w, Time time)
{
    (void) XtOwnSelection(w, XA_PRIMARY, time, ConvertSelection, NULL, NULL);
}


/*ARGSUSED*/
static void
SelectionCallback(Widget w, XtPointer client_data, Atom *selection, 
		  Atom *type, XtPointer value, unsigned long *length, 
		  int *format)
{

    if  (*type == XA_PIXMAP) {
	Pixmap *pixmap;
	XImage *image;
	Window root;
	int x, y;
	unsigned int width, height, border_width, depth;

	pixmap = (Pixmap *) value;
	XGetGeometry(XtDisplay(w), *pixmap, &root, &x, &y,
		     &width, &height, &border_width, &depth);
	image = XGetImage(XtDisplay(w), *pixmap, 0, 0, width, height, 
			  AllPlanes, ZPixmap);
	SWAutoscale(w, NULL, NULL, NULL);
	SWSetImage(w, image);
	XtFree(value);
	XDestroyImage(image);
    }
}

void 
SWRequestSelection(Widget w, Time time)
{
    XtGetSelectionValue(w, XA_PRIMARY, XA_PIXMAP, SelectionCallback, NULL,
			time);
}

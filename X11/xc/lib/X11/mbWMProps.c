/* $Xorg: mbWMProps.c,v 1.4 2001/02/09 02:03:40 xorgcvs Exp $ */
/*

Copyright 1991, 1998  The Open Group

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
/* $XFree86: xc/lib/X11/mbWMProps.c,v 1.5 2001/12/14 19:54:10 dawes Exp $ */

#include <X11/Xlibint.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/Xlocale.h>

#if NeedFunctionPrototypes
void XmbSetWMProperties (
    Display *dpy,
    Window w,
    _Xconst char *windowName,
    _Xconst char *iconName,
    char **argv,
    int argc,
    XSizeHints *sizeHints,
    XWMHints *wmHints,
    XClassHint *classHints)
#else
void XmbSetWMProperties (dpy, w, windowName, iconName, argv, argc, sizeHints,
			 wmHints, classHints)
     Display *dpy;
     Window w;			/* window to decorate */
     _Xconst char *windowName;	/* name of application */
     _Xconst char *iconName;	/* name string for icon */
     char **argv;		/* command line */
     int argc;			/* size of command line */
     XSizeHints *sizeHints;	/* size hints for window in its normal state */
     XWMHints *wmHints;		/* miscelaneous window manager hints */
     XClassHint *classHints;	/* resource name and class */
#endif
{
    XTextProperty wname, iname;
    XTextProperty *wprop = NULL;
    XTextProperty *iprop = NULL;

    if (windowName &&
	XmbTextListToTextProperty(dpy, (char**)&windowName, 1,
				  XStdICCTextStyle, &wname) >= Success)
	wprop = &wname;
    if (iconName &&
	XmbTextListToTextProperty(dpy, (char**)&iconName, 1,
				  XStdICCTextStyle, &iname) >= Success)
	iprop = &iname;
    XSetWMProperties(dpy, w, wprop, iprop, argv, argc,
		     sizeHints, wmHints, classHints);
    if (wprop)
	Xfree((char *)wname.value);
    if (iprop)
	Xfree((char *)iname.value);

    /* Note: The WM_LOCALE_NAME property is set by XSetWMProperties. */
}

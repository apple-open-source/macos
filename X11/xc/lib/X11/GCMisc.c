/* $Xorg: GCMisc.c,v 1.4 2001/02/09 02:03:33 xorgcvs Exp $ */
/*

Copyright 1986, 1998  The Open Group

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall not be
used in advertising or otherwise to promote the sale, use or other dealings
in this Software without prior written authorization from The Open Group.

*/
/* $XFree86: xc/lib/X11/GCMisc.c,v 1.4 2001/12/14 19:54:00 dawes Exp $ */

#include "Xlibint.h"

int
XSetArcMode (dpy, gc, arc_mode)
register Display *dpy;
register GC gc;
int arc_mode;
{
    LockDisplay(dpy);
    if (gc->values.arc_mode != arc_mode) {
	gc->values.arc_mode = arc_mode;
	gc->dirty |= GCArcMode;
    }
    UnlockDisplay(dpy);
    SyncHandle();
    return 1;
}

int
XSetFillRule (dpy, gc, fill_rule)
register Display *dpy;
register GC gc;
int fill_rule;
{
    LockDisplay(dpy);
    if (gc->values.fill_rule != fill_rule) {
	gc->values.fill_rule = fill_rule;
	gc->dirty |= GCFillRule;
    }
    UnlockDisplay(dpy);
    SyncHandle();
    return 1;
}

int
XSetFillStyle (dpy, gc, fill_style)
register Display *dpy;
register GC gc;
int fill_style;
{
    LockDisplay(dpy);
    if (gc->values.fill_style != fill_style) {
	gc->values.fill_style = fill_style;
	gc->dirty |= GCFillStyle;
    }
    UnlockDisplay(dpy);
    SyncHandle();
    return 1;
}

int
XSetGraphicsExposures (dpy, gc, graphics_exposures)
register Display *dpy;
register GC gc;
Bool graphics_exposures;
{
    LockDisplay(dpy);
    if (gc->values.graphics_exposures != graphics_exposures) {
	gc->values.graphics_exposures = graphics_exposures;
	gc->dirty |= GCGraphicsExposures;
    }
    UnlockDisplay(dpy);
    SyncHandle();
    return 1;
}

int
XSetSubwindowMode (dpy, gc, subwindow_mode)
register Display *dpy;
register GC gc;
int subwindow_mode;
{
    LockDisplay(dpy);
    if (gc->values.subwindow_mode != subwindow_mode) {
	gc->values.subwindow_mode = subwindow_mode;
	gc->dirty |= GCSubwindowMode;
    }
    UnlockDisplay(dpy);
    SyncHandle();
    return 1;
}

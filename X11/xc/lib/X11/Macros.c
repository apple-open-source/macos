/* $Xorg: Macros.c,v 1.4 2001/02/09 02:03:34 xorgcvs Exp $ */
/*

Copyright 1987, 1998  The Open Group

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
/* $XFree86: xc/lib/X11/Macros.c,v 1.4 2001/12/14 19:54:02 dawes Exp $ */

#include "Xlibint.h"
/*
 * This file makes full definitions of routines for each macro.
 * We do not expect C programs to use these, but other languages may
 * need them.
 */

int XConnectionNumber(dpy) Display *dpy; { return (ConnectionNumber(dpy)); }

Window XRootWindow (dpy, scr)
    Display *dpy; int scr;
	{	return (RootWindow(dpy,scr));	}

int XDefaultScreen(dpy) Display *dpy; { return (DefaultScreen(dpy)); }

Window XDefaultRootWindow (dpy)
    Display *dpy;
	{	return (RootWindow(dpy,DefaultScreen(dpy)));	}

Visual *XDefaultVisual(dpy, scr)
    Display *dpy; int scr;
	{	return (DefaultVisual(dpy, scr)); }

GC XDefaultGC(dpy, scr) 
    Display *dpy; int scr;
	{	return (DefaultGC(dpy,scr)); }

unsigned long XBlackPixel(dpy, scr) 
    Display *dpy; int scr; 
	{	return (BlackPixel(dpy, scr)); }

unsigned long XWhitePixel(dpy, scr) 
    Display *dpy; int scr; 
	{	return (WhitePixel(dpy,scr)); }

unsigned long XAllPlanes() { return AllPlanes; }

int XQLength(dpy) Display *dpy; { return (QLength(dpy)); }

int XDisplayWidth(dpy, scr)
    Display *dpy; int scr;
	{ return (DisplayWidth(dpy,scr)); }

int XDisplayHeight(dpy, scr)
    Display *dpy; int scr;
	{ return (DisplayHeight(dpy, scr)); }

int XDisplayWidthMM(dpy, scr)
    Display *dpy; int scr;
	{ return (DisplayWidthMM(dpy, scr)); }

int XDisplayHeightMM(dpy, scr)
    Display *dpy; int scr;
	{ return (DisplayHeightMM(dpy, scr)); }

int XDisplayPlanes(dpy, scr)
    Display *dpy; int scr;
	{ return (DisplayPlanes(dpy, scr)); }

int XDisplayCells(dpy, scr)
    Display *dpy; int scr;
	{ return (DisplayCells (dpy, scr)); }

int XScreenCount(dpy) Display *dpy; { return (ScreenCount(dpy)); }

char *XServerVendor(dpy)  Display *dpy; { return (ServerVendor(dpy)); }

int XProtocolVersion(dpy)  Display *dpy; { return (ProtocolVersion(dpy)); }

int XProtocolRevision(dpy)  Display *dpy; { return (ProtocolRevision(dpy));}

int XVendorRelease(dpy)  Display *dpy; { return (VendorRelease(dpy)); }

char *XDisplayString(dpy)  Display *dpy; { return (DisplayString(dpy)); }

int XDefaultDepth(dpy, scr) 
    Display *dpy; int scr;
	{ return(DefaultDepth(dpy, scr)); }

Colormap XDefaultColormap(dpy, scr)
    Display *dpy; int scr;
	{ return (DefaultColormap(dpy, scr)); }

int XBitmapUnit(dpy) Display *dpy; { return (BitmapUnit(dpy)); }

int XBitmapBitOrder(dpy) Display *dpy; { return (BitmapBitOrder(dpy)); }

int XBitmapPad(dpy) Display *dpy; { return (BitmapPad(dpy)); }

int XImageByteOrder(dpy) Display *dpy; { return (ImageByteOrder(dpy)); }

unsigned long XNextRequest(dpy)
    Display *dpy;
    {
#ifdef WORD64
	WORD64ALIGN
	return dpy->request + 1;
#else
	return (NextRequest(dpy));
#endif
    }

unsigned long XLastKnownRequestProcessed(dpy)
    Display *dpy;
    { return (LastKnownRequestProcessed(dpy)); }

/* screen oriented macros (toolkit) */
Screen *XScreenOfDisplay(dpy, scr) Display *dpy; int scr;
	{ return (ScreenOfDisplay(dpy, scr)); }

Screen *XDefaultScreenOfDisplay(dpy) Display *dpy;
	{ return (DefaultScreenOfDisplay(dpy)); }

Display *XDisplayOfScreen(s) Screen *s; { return (DisplayOfScreen(s)); }

Window XRootWindowOfScreen(s) Screen *s; { return (RootWindowOfScreen(s)); }

unsigned long XBlackPixelOfScreen(s) Screen *s; 
	{ return (BlackPixelOfScreen(s)); }

unsigned long XWhitePixelOfScreen(s) Screen *s; 
	{ return (WhitePixelOfScreen(s)); }

Colormap XDefaultColormapOfScreen(s) Screen *s; 
	{ return (DefaultColormapOfScreen(s)); }

int XDefaultDepthOfScreen(s) Screen *s; { return (DefaultDepthOfScreen(s)); }

GC XDefaultGCOfScreen(s) Screen *s; { return (DefaultGCOfScreen(s)); }

Visual *XDefaultVisualOfScreen(s) Screen *s; 
	{ return (DefaultVisualOfScreen(s)); }

int XWidthOfScreen(s) Screen *s; { return (WidthOfScreen(s)); }

int XHeightOfScreen(s) Screen *s; { return (HeightOfScreen(s)); }

int XWidthMMOfScreen(s) Screen *s; { return (WidthMMOfScreen(s)); }

int XHeightMMOfScreen(s) Screen *s; { return (HeightMMOfScreen(s)); }

int XPlanesOfScreen(s) Screen *s; { return (PlanesOfScreen(s)); }

int XCellsOfScreen(s) Screen *s; { return (CellsOfScreen(s)); }

int XMinCmapsOfScreen(s) Screen *s; { return (MinCmapsOfScreen(s)); }

int XMaxCmapsOfScreen(s) Screen *s; { return (MaxCmapsOfScreen(s)); }

Bool XDoesSaveUnders(s) Screen *s; { return (DoesSaveUnders(s)); }

int XDoesBackingStore(s) Screen *s; { return (DoesBackingStore(s)); }

long XEventMaskOfScreen(s) Screen *s; { return (EventMaskOfScreen(s)); }

int XScreenNumberOfScreen (scr)
    register Screen *scr;
{
    register Display *dpy = scr->display;
    register Screen *dpyscr = dpy->screens;
    register int i;

    for (i = 0; i < dpy->nscreens; i++, dpyscr++) {
	if (scr == dpyscr) return i;
    }
    return -1;
}

/*
 * These macros are used to give some sugar to the image routines so that
 * naive people are more comfortable with them.
 */
#undef XDestroyImage
int
XDestroyImage(ximage)
	XImage *ximage;
{
	return((*((ximage)->f.destroy_image))((ximage)));
}
#undef XGetPixel
unsigned long XGetPixel(ximage, x, y)
	XImage *ximage;
	int x, y;
{
	return ((*((ximage)->f.get_pixel))((ximage), (x), (y)));
}
#undef XPutPixel
int XPutPixel(ximage, x, y, pixel)
	XImage *ximage;
	int x, y;
	unsigned long pixel;
{
	return((*((ximage)->f.put_pixel))((ximage), (x), (y), (pixel)));
}
#undef XSubImage
XImage *XSubImage(ximage, x, y, width, height)
	XImage *ximage;
	int x, y;
	unsigned int width, height;
{
	return((*((ximage)->f.sub_image))((ximage), (x),
		(y), (width), (height)));
}
#undef XAddPixel
int XAddPixel(ximage, value)
	XImage *ximage;
	long value;
{
	return((*((ximage)->f.add_pixel))((ximage), (value)));
}


int
XNoOp (dpy)
    register Display *dpy;
{
    register xReq *req;

    LockDisplay(dpy);
    GetEmptyReq(NoOperation, req);

    UnlockDisplay(dpy);
    SyncHandle();
    return 1;
}

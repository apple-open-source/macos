/* $Xorg: init.c,v 1.4 2001/02/09 02:05:31 xorgcvs Exp $ */
/*

Copyright 1998  The Open Group

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
/*
 * Copyright 1992 Network Computing Devices
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of NCD. not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  NCD. makes no representations about the
 * suitability of this software for any purpose.  It is provided "as is"
 * without express or implied warranty.
 *
 * NCD. DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL NCD.
 * BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN 
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 */
/* $XFree86: xc/programs/lbxproxy/di/init.c,v 1.6 2001/12/14 20:00:51 dawes Exp $ */

/*
 * This file is used for anything that needs to dip into Xlib structures
 */

/* lbxproxy source files are compiled with -D_XSERVER64 on 64 bit machines.
 * For the most part, this is appropriate.  However, for this file it
 * is not, because we make Xlib calls here.
 */
#undef _XSERVER64
#include "wire.h"
#include "util.h"
#include <X11/Xlibint.h>
#include <X11/Xutil.h>
#include <stdio.h>
#include "XLbx.h"

/* Hook up to an X server and set up a multiplexing LBX encoded connection */
struct _XDisplay*
DisplayOpen (dpy_name, requestp, eventp, errorp, sequencep)
    char    *dpy_name;
    int	    *requestp, *eventp, *errorp;
    int	    *sequencep;
{
    Display *dpy;
    int	    lbxMajor, lbxMinor;
    Window  win;
    XTextProperty name;
    XClassHint class;

    dpy = XOpenDisplay (dpy_name);
    if (!dpy)
	return NULL;
    
    win = XCreateSimpleWindow(dpy, DefaultRootWindow(dpy),
			      -1, -1, 1, 1, 0, 0, 0);
    class.res_name = "lbxproxy";
    class.res_class = "LbxProxy";
    XStringListToTextProperty(&class.res_name, 1, &name);
    XSetWMProperties(dpy, win, &name, NULL, NULL, 0, NULL, NULL, &class);

    if (!XLbxQueryExtension (dpy, requestp, eventp, errorp)) {
	ErrorF("X server doesn't have LBX extension\n");
	XCloseDisplay(dpy);
	return NULL;
    }
    if (!XLbxQueryVersion (dpy, &lbxMajor, &lbxMinor)) {
	ErrorF("LBX extension query failed\n");
	XCloseDisplay(dpy);
	return NULL;
    }
    if (lbxMajor != LBX_MAJOR_VERSION || lbxMinor != LBX_MINOR_VERSION)
	ErrorF("LBX version numbers don't match (proxy %d.%d, server %d.%d), good luck!\n", LBX_MAJOR_VERSION, LBX_MINOR_VERSION, lbxMajor, lbxMinor);
    *sequencep = NextRequest (dpy) - 1;
    return dpy;
}

int DisplayConnectionNumber (dpy)
    struct _XDisplay* dpy;
{
    return ConnectionNumber (dpy);
}

void DisplayGetConnSetup (dpy, cs, cs_len, change_type, changes, change_len)
    struct _XDisplay* dpy;
    xConnSetup** cs;
    int* cs_len;
    int change_type;
    CARD32* changes;
    int change_len;
{
    char* datap;
    xConnSetup setup;
    int olen;
    int i, s, d, v;
    static int padlength[4] = {0, 3, 2, 1};
    XPixmapFormatValues* pixmap_formats;
    int npixmap_formats;
    int min_keycode, max_keycode;
    xWindowRoot root;
    Screen* scrnp;
    int numScreens;
    int numVideoScreens;
    int scrlist[16]; /* are there servers with more than 16 screens? */
    int scrldex;

    if (change_type == 2) {
	numScreens = change_len - 6;
	numVideoScreens = 1;
    } else
	numScreens = numVideoScreens = ScreenCount(dpy);

    pixmap_formats = XListPixmapFormats (dpy, &npixmap_formats);
    XDisplayKeycodes (dpy, &min_keycode, &max_keycode);

    setup.release = VendorRelease(dpy);
    setup.ridBase = dpy->resource_base;
    setup.ridMask = dpy->resource_mask;
    setup.motionBufferSize = XDisplayMotionBufferSize (dpy);
    setup.nbytesVendor = strlen (ServerVendor (dpy));
    setup.maxRequestSize = XMaxRequestSize (dpy);
    setup.numRoots = ScreenCount(dpy);
    setup.numFormats = npixmap_formats;
    setup.imageByteOrder = ImageByteOrder(dpy);
    setup.bitmapBitOrder = BitmapBitOrder(dpy);
    setup.bitmapScanlineUnit = BitmapUnit(dpy);
    setup.bitmapScanlinePad = BitmapPad(dpy);
    setup.minKeyCode = min_keycode;
    setup.maxKeyCode = max_keycode;

    olen = sizeof (xConnSetup) + ((setup.nbytesVendor + 3) & ~3) +
	(setup.numFormats * sizeof (xPixmapFormat));

    scrldex = 0;
    if (change_type != 2) {
	olen += (setup.numRoots * sizeof (xWindowRoot));

	for (s = 0; s < numScreens; s++) {
	    scrlist[scrldex++] = s;
	    for (d = 0; d < ScreenOfDisplay(dpy,s)->ndepths; d++) {
		olen += sizeof (xDepth) + 
			sizeof (xVisualType) * ScreenOfDisplay(dpy,s)->depths[d].nvisuals;
	    }
	}
    } else {
	olen += ((1 + numScreens - numVideoScreens) * sizeof (xWindowRoot));

	/* find the one video screen that matches the appgroup's */
	for (s = 0; s < numScreens; s++)
	    if (RootWindow (dpy, s) == (Window) changes[1])
		break;
	scrlist[scrldex++] = s;
	for (d = 0; d < ScreenOfDisplay(dpy,s)->ndepths; d++) {
	    olen += sizeof (xDepth) + 
		    sizeof (xVisualType) * ScreenOfDisplay(dpy,s)->depths[d].nvisuals;
	}
	/* and any print screens */
	for (s = numVideoScreens; s < numScreens; s++) {
	    scrlist[scrldex++] = s;
	    for (d = 0; d < ScreenOfDisplay(dpy,s)->ndepths; d++) {
		olen += sizeof (xDepth) + 
			sizeof (xVisualType) * ScreenOfDisplay(dpy,s)->depths[d].nvisuals;
	    }
	}
    }

    datap = (char*) xalloc (olen);
    if (datap == NULL) {
	Xfree (pixmap_formats);
	*cs = NULL;
	*cs_len = 0;
	return;
    }

    *cs = (xConnSetup*) datap;
    *cs_len = olen;
    memmove (datap, &setup, sizeof (xConnSetup));
    datap += sizeof (xConnSetup);
    memmove (datap, ServerVendor(dpy), setup.nbytesVendor);
    datap += setup.nbytesVendor;
    i = padlength[setup.nbytesVendor & 3];
    while (--i >= 0)
	*datap++ = 0;

    for (i = 0; i < npixmap_formats; i++) {
	xPixmapFormat pf;

	pf.depth = pixmap_formats[i].depth;
	pf.bitsPerPixel = pixmap_formats[i].bits_per_pixel;
	pf.scanLinePad = pixmap_formats[i].scanline_pad;
	memmove (datap, &pf, sizeof (xPixmapFormat));
	datap += sizeof (xPixmapFormat);
    }

    for (s = 0; s < scrldex; s++) {
	xDepth dep;
	Depth* depthp;

	scrnp = ScreenOfDisplay(dpy,scrlist[s]);
	root.windowId = scrnp->root;
	root.defaultColormap = scrnp->cmap;
	root.whitePixel = scrnp->white_pixel;
	root.blackPixel = scrnp->black_pixel;
	root.currentInputMask = scrnp->root_input_mask;
	root.pixWidth = scrnp->width;
	root.pixHeight = scrnp->height;
	root.mmWidth = scrnp->mwidth;
	root.mmHeight = scrnp->mheight;
	root.minInstalledMaps = scrnp->min_maps;
	root.maxInstalledMaps = scrnp->max_maps;
	root.rootVisualID = scrnp->root_visual->visualid;
	root.backingStore = scrnp->backing_store;
	root.saveUnders = scrnp->save_unders;
	root.rootDepth = scrnp->root_depth;
	root.nDepths = scrnp->ndepths;
	memmove (datap, &root, sizeof (xWindowRoot));
	datap += sizeof (xWindowRoot);

	for (d = 0; d < scrnp->ndepths; d++) {
	    xVisualType vis;
	    Visual* visp;

	    depthp = &scrnp->depths[d];
	    dep.depth = depthp->depth;
	    dep.nVisuals = depthp->nvisuals;
	    memmove (datap, &dep, sizeof (xDepth));
	    datap += sizeof (xDepth);

	    for (v = 0; v < depthp->nvisuals; v++) {
		visp = &depthp->visuals[v];
		vis.visualID = visp->visualid;
		vis.class = visp->class;
		vis.bitsPerRGB = visp->bits_per_rgb;
		vis.colormapEntries = visp->map_entries;
		vis.redMask = visp->red_mask;
		vis.greenMask = visp->green_mask;
		vis.blueMask = visp->blue_mask;
		memmove (datap, &vis, sizeof (xVisualType));
		datap += sizeof (xVisualType);
	    }
	}
    }
    Xfree (pixmap_formats);
}

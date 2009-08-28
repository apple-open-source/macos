/*
 * imgWindow.c --
 *
 * A photo image file handler to put the content of a window in a photo.
 *
 * Author : Jan Nijtmans
 *
 * $Id: window.c 174 2008-12-01 15:39:10Z nijtmans $
 *
 */

/*
 * Generic initialization code, parameterized via CPACKAGE and PACKAGE.
 */

#include "init.c"

#include "X11/Xutil.h"
#ifndef	__WIN32__
#  ifndef MAC_OSX_TK
#   include "X11/Xproto.h"
#  else
#   include "X11/Xlib.h"
#   include "X11/Xfuncproto.h"
#   undef X_GetImage
#  endif
#else
/*#   include <windows.h>*/
#   include "X11/Xlib.h"
#   include "tkInt.h"
#   include "tkWinInt.h"
#   include "X11/Xfuncproto.h"
#   undef X_GetImage
#endif

/*
 * The format record for the Win data format:
 */

#ifdef X_GetImage
static int xerrorhandler _ANSI_ARGS_((ClientData clientData,
	                              XErrorEvent *e));
#endif

typedef struct ColormapData {	/* Hold color information for a window */
    int separated;		/* Whether to use separate color bands */
    int color;			/* Whether window is color or black/white */
    int ncolors;		/* Number of color values stored */
    XColor *colors;		/* Pixel value -> RGB mappings */
    int red_mask, green_mask, blue_mask;	/* Masks and shifts for each */
    int red_shift, green_shift, blue_shift;	/* color band */
} ColormapData;

/*
 * Prototypes for local procedures defined in this file:
 */

#define UCHAR(c) ((unsigned char) (c))
/*
 *--------------------------------------------------------------
 *
 * xerrorhandler --
 *
 *	This is a dummy function to catch X11 errors during an
 *	attempt to convert a window to a photo image.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

#ifdef X_GetImage
static int
xerrorhandler(clientData, e)
    ClientData clientData;
    XErrorEvent *e;
{
    return 0;
}
#endif

/* OPA TODO: Must be a better way to specify non-existing format functions. */
static int
ChnRead (interp, chan, fileName, format, imageHandle,
         destX, destY, width, height, srcX, srcY)
    Tcl_Interp *interp;
    Tcl_Channel chan;
    const char *fileName;
    Tcl_Obj *format;
    Tk_PhotoHandle imageHandle;
    int destX, destY;
    int width, height;
    int srcX, srcY;
{
    return 0;
}

static int
ChnWrite (interp, filename, format, blockPtr)
    Tcl_Interp *interp;
    CONST84 char *filename;
    Tcl_Obj *format;
    Tk_PhotoImageBlock *blockPtr;
{
    return 0;
}

static int
StringWrite (interp, dataPtr, format, blockPtr)
    Tcl_Interp *interp;
    Tcl_DString *dataPtr;
    Tcl_Obj *format;
    Tk_PhotoImageBlock *blockPtr;
{
    return 0;
}

/*
 *----------------------------------------------------------------------
 *
 * ChnMatch --
 *
 *	This procedure is invoked by the photo image type to see if
 *	a file contains image data in WINDOW format.
 *
 * Results:
 *	The return value is always 0, because a window cannot be
 *	read from a file.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int ChnMatch(interp, chan, filename, format, widthPtr, heightPtr)
    Tcl_Interp *interp;
    Tcl_Channel chan;
    const char *filename;
    Tcl_Obj *format;
    int *widthPtr, *heightPtr;
{
    return 0;
}

/*
 *----------------------------------------------------------------------
 *
 * ObjMatch --
 *
 *  This procedure is invoked by the photo image type to see if
 *  an object contains image data which can be read from a window.
 *
 * Results:
 *  The return value is 1 if data contains a valid window name.
 *
 * Side effects:
 *  the size of the image is placed in widthPtr and heightPtr.
 *
 *----------------------------------------------------------------------
 */

static int ObjMatch(interp, data, format, widthPtr, heightPtr)
    Tcl_Interp *interp;
    Tcl_Obj *data;
    Tcl_Obj *format;
    int *widthPtr, *heightPtr;
{
    Tk_Window tkwin;
    const char *name;

    tkimg_FixObjMatchProc(&interp, &data, &format, &widthPtr, &heightPtr);

    name = tkimg_GetStringFromObj(data, NULL);

    if (interp && name && (name[0] == '.') &&
        ((name[1] == 0) || islower(UCHAR(name[1])))) {
	tkwin = Tk_MainWindow(interp);
	if (tkwin == NULL) {
	    return 0;
	}
	tkwin = Tk_NameToWindow(interp, name, tkwin);
	if (tkwin == NULL) {
	    *widthPtr = *heightPtr = 0;
	    return 1;
	}
	*widthPtr =  Tk_Width(tkwin);
	*heightPtr = Tk_Height(tkwin);
	return 1;
    }
    return 0;
}

/*
 *----------------------------------------------------------------------
 *
 * ObjRead --
 *
 *	This procedure is called by the photo image type to read
 *	the contents of a window and give it to the photo image.
 *
 * Results:
 *	A standard TCL completion code.  If TCL_ERROR is returned
 *	then an error message is left in interp->result.
 *
 * Side effects:
 *	new data is added to the image given by imageHandle.
 *
 *----------------------------------------------------------------------
 */

typedef struct myblock {
    Tk_PhotoImageBlock ck;
    int dummy; /* extra space for offset[3], in case it is not
		  included already in Tk_PhotoImageBlock */
} myblock;

#define block bl.ck

static int ObjRead(interp, data, format, imageHandle,
                   destX, destY, width, height, srcX, srcY)
    Tcl_Interp *interp;
    Tcl_Obj *data;
    Tcl_Obj *format;
    Tk_PhotoHandle imageHandle;
    int destX, destY;
    int width, height;
    int srcX, srcY;
{
    myblock bl;
    Tk_Window tkwin;
    int fileWidth, fileHeight, depth, nBytes, x, y;
    const char *name;
#ifndef	__WIN32__
    XImage *ximage;
    ColormapData cdata;
    Colormap cmap;
    int i, ncolors;
#else
#   undef XGetPixel
#   define XGetPixel(P,X,Y) GetPixel(P, X, Y)
    TkWinDCState DCi;
    HDC			ximage;
#endif
    Visual *visual;
    unsigned char *p;
#ifdef X_GetImage
    Tk_ErrorHandler	handle;
#endif
    int green, blue;

    name = tkimg_GetStringFromObj(data, NULL);

    tkwin = Tk_NameToWindow(interp, name, Tk_MainWindow(interp));

    if (!tkwin) {
	Tcl_AppendResult(interp, "Window \"", name,"\" doesn't exist", (char *) NULL);
	return TCL_ERROR;
    }

    if (!Tk_WindowId(tkwin)) {
	Tcl_AppendResult(interp, "Window \"", name,"\" is not mapped", (char *) NULL);
	return TCL_ERROR;
    }

    fileWidth = Tk_Width(tkwin);
    fileHeight = Tk_Height(tkwin);

    if ((srcX + width) > fileWidth) {
	width = fileWidth - srcX;
    }
    if ((srcY + height) > fileHeight) {
	height = fileHeight - srcY;
    }
    if ((width <= 0) || (height <= 0)) {
	return TCL_OK;
    }

    /*
     * If the window is off the screen it will generate an BadMatch/XError
     * We catch any BadMatch errors here
     */

#ifdef X_GetImage
    handle = Tk_CreateErrorHandler(Tk_Display(tkwin), BadMatch,
	    X_GetImage, -1, xerrorhandler, (ClientData) tkwin);
#endif

#ifndef	__WIN32__
    /*
     * Generate an XImage from the window.  We can then read pixel
     * values out of the XImage.
     */

    ximage = XGetImage(Tk_Display(tkwin), Tk_WindowId(tkwin), srcX, srcY,
	width, height, AllPlanes, ZPixmap);

#ifdef X_GetImage
    Tk_DeleteErrorHandler(handle);
#endif

    if (ximage == (XImage*) NULL) {
	Tcl_AppendResult(interp, "Window \"", name,
		"\" cannot be transformed into a pixmap (possibly obscured?)",
		(char *) NULL);
	return TCL_ERROR;
    }
#else
    ximage = TkWinGetDrawableDC(Tk_Display(tkwin), Tk_WindowId(tkwin), &DCi);
#endif

    depth = Tk_Depth(tkwin);
    visual = Tk_Visual(tkwin);
#ifndef	__WIN32__
    cmap = Tk_Colormap(tkwin);

    /*
     * Obtain information about the colormap, ie the mapping between
     * pixel values and RGB values.  The code below should work
     * for all Visual types.
     */

    ncolors = visual->map_entries;
    cdata.colors = (XColor *) ckalloc(sizeof(XColor) * ncolors);
    cdata.ncolors = ncolors;
    if (visual->class == DirectColor || visual->class == TrueColor) {
	cdata.separated = 1;
	cdata.red_mask = visual->red_mask;
	cdata.green_mask = visual->green_mask;
	cdata.blue_mask = visual->blue_mask;
	cdata.red_shift = 0;
	cdata.green_shift = 0;
	cdata.blue_shift = 0;
	while ((0x0001 & (cdata.red_mask >> cdata.red_shift)) == 0)
	    cdata.red_shift ++;
	while ((0x0001 & (cdata.green_mask >> cdata.green_shift)) == 0)
	    cdata.green_shift ++;
	while ((0x0001 & (cdata.blue_mask >> cdata.blue_shift)) == 0)
	    cdata.blue_shift ++;
	for (i = 0; i < ncolors; i ++)
	    cdata.colors[i].pixel =
		    ((i << cdata.red_shift) & cdata.red_mask) |
		    ((i << cdata.green_shift) & cdata.green_mask) |
		    ((i << cdata.blue_shift) & cdata.blue_mask);
    } else {
	cdata.separated = 0;
	cdata.red_mask = 0;
	cdata.green_mask = 0;
	cdata.blue_mask = 0;
	cdata.red_shift = 0;
	cdata.green_shift = 0;
	cdata.blue_shift = 0;
	for (i = 0; i < ncolors; i ++) cdata.colors[i].pixel = i;
    }
    cdata.color = !(visual->class == StaticGray || visual->class == GrayScale);

    XQueryColors(Tk_Display(tkwin), cmap, cdata.colors, ncolors);
#endif

    Tk_PhotoExpand(imageHandle, destX + width, destY + height);
    block.offset[0] = 0;
    block.offset[3] = 0;
#ifndef	__WIN32__
    if (cdata.color) {
#endif
	block.pixelSize = 3;
	block.offset[1] = green = 1;
	block.offset[2] = blue = 2;
#ifndef	__WIN32__
    } else {
	block.pixelSize = 1;
	block.offset[1] = green = 0;
	block.offset[2] = blue = 0;
    }
#endif
    block.width = width;
    block.height = height;
    block.pitch = block.pixelSize * width;
    nBytes = block.pitch * height;
    block.pixelPtr = (unsigned char *) ckalloc((unsigned) nBytes);

    p = block.pixelPtr;
    for (y = 0; y<height; y++) {
	for (x = 0; x<width; x++) {
	    unsigned long pixel = XGetPixel(ximage, x, y);
#ifndef	__WIN32__
	    if (cdata.separated) {
		int r = (pixel & cdata.red_mask) >> cdata.red_shift;
		p[0] = cdata.colors[r].red >> 8;
		if (cdata.color) {
		    int g = (pixel & cdata.green_mask) >> cdata.green_shift;
		    int b = (pixel & cdata.blue_mask) >> cdata.blue_shift;
		    p[1] = cdata.colors[g].green >> 8;
		    p[2] = cdata.colors[b].blue >> 8;
		}
	    } else {
		p[0] = cdata.colors[pixel].red >> 8;
		if (cdata.color) {
		    p[1] = cdata.colors[pixel].green >> 8;
		    p[2] = cdata.colors[pixel].blue >> 8;
		}
	    }
#else
	    p[0] = GetRValue(pixel);
	    p[1] = GetGValue(pixel);
	    p[2] = GetBValue(pixel);
#endif
	    p += block.pixelSize;
	}
    }

    tkimg_PhotoPutBlock(interp, imageHandle, &block, destX, destY, width, height, TK_PHOTO_COMPOSITE_SET);

#ifndef	__WIN32__
    XDestroyImage(ximage);
    ckfree((char *) cdata.colors);
#else
#   undef XGetPixel
    TkWinReleaseDrawableDC(Tk_WindowId(tkwin), ximage, &DCi);
#endif
    ckfree((char *) block.pixelPtr);
    return TCL_OK;
}

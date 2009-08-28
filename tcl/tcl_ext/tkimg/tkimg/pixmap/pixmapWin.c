/*
 *imgWinPmap.c --
 *
 *	Implement the Windows specific function calls for the pixmap
 *	image type.
 *
 * Copyright (c) 1996, Expert Interface Technologies
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 */

/*#include <windows.h>*/
#include "tkInt.h"
#include "tkWinInt.h"
#include <stdlib.h>
#include <pixmapInt.h>
/* #include "imgInt.h" */ /* ---- what is used ?? */
#include <X11/Xlib.h>
#include <X11/Xutil.h>

typedef struct PixmapData {
    HDC bitmapDC;               /* Bitmap used on Windows platforms */
    HDC maskDC;                 /* Mask used on Windows platforms */
    HBITMAP bitmap, bitmapOld;
    HBITMAP maskBm, maskBmOld;
} PixmapData;

static void		CopyTransparent _ANSI_ARGS_((Display* display,
			    HDC srcDC, Drawable dest,
			    int src_x, int src_y, int width,
			    int height, int dest_x, int dest_y,
			    HDC maskDC));


/*----------------------------------------------------------------------
 * TkimgInitPixmapInstance --
 *
 *	Initializes the platform-specific data of a pixmap instance
 *
 *----------------------------------------------------------------------
 */

void
TkimgInitPixmapInstance(masterPtr, instancePtr)
    PixmapMaster *masterPtr;	/* Pointer to master for image. */
    PixmapInstance *instancePtr;/* The pixmap instance. */
{
    PixmapData * dataPtr;

    dataPtr = (PixmapData *)ckalloc(sizeof(PixmapData));
    dataPtr->maskDC = NULL;
    dataPtr->bitmapDC = NULL;

    instancePtr->clientData = (ClientData)dataPtr;
}


/*----------------------------------------------------------------------
 * TkimgXpmAllocTmpBuffer --
 *
 *	Allocate a temporary space to draw the image.
 *
 *----------------------------------------------------------------------
 */
void
TkimgXpmAllocTmpBuffer(masterPtr, instancePtr, imagePtr, maskPtr)
    PixmapMaster * masterPtr;
    PixmapInstance * instancePtr;
    XImage ** imagePtr;
    XImage ** maskPtr;
{
    XImage * image = NULL, * mask = NULL;
    Display *display = Tk_Display(instancePtr->tkwin);
    int depth;

    depth = Tk_Depth(instancePtr->tkwin);

    instancePtr->pixmap = Tk_GetPixmap(display,
	Tk_WindowId(instancePtr->tkwin),
	masterPtr->size[0], masterPtr->size[1], depth);

    mask = (XImage*)ckalloc(sizeof(XImage));

    mask->width = masterPtr->size[0];
    mask->height = masterPtr->size[1];
    mask->bytes_per_line = (mask->width+7)/8;

    mask->data =
      (char *)ckalloc(mask->bytes_per_line  * masterPtr->size[1]);

    *imagePtr = image;
    *maskPtr = mask;
}


void
TkimgXpmFreeTmpBuffer(masterPtr, instancePtr, image, mask)
    PixmapMaster * masterPtr;
    PixmapInstance * instancePtr;
    XImage * image;
    XImage * mask;
{
    if (image) {
	ckfree((char*)image->data);
	image->data = NULL;
	XDestroyImage(image);
    }
    if (mask) {
	ckfree((char*)mask->data);
	mask->data = NULL;
	ckfree((char*)mask);
    }
}


/*----------------------------------------------------------------------
 * TkimgXpmSetPixel --
 *
 *	Sets the pixel at the given (x,y) coordinate to be the given
 *	color.
 *----------------------------------------------------------------------
 */
void
TkimgXpmSetPixel(instancePtr, image, mask, x, y, colorPtr, isTranspPtr)
    PixmapInstance * instancePtr;
    XImage * image;
    XImage * mask;
    int x;
    int y;
    XColor * colorPtr;
    int * isTranspPtr;
{
    char * p;
    int n;
    GC gc;
    XGCValues gcValues;
    Display *display = Tk_Display(instancePtr->tkwin);

    if (colorPtr != NULL) {
	gcValues.foreground = colorPtr->pixel;
	gc = Tk_GetGC(instancePtr->tkwin, GCForeground, &gcValues);
	XDrawRectangle(display, instancePtr->pixmap, gc, x, y, 1, 1);
	Tk_FreeGC(display, gc);
    }

    p = mask->data;
    p+= y*(mask->bytes_per_line);
    p+= x/8;
    n = x%8;

    if (colorPtr != NULL) {
	*p |=  (1 << (7-n));
    } else {
	*p &= ~(1 << (7-n));
	*isTranspPtr = 1;
    }
}

/*----------------------------------------------------------------------
 * TkimgXpmRealizePixmap --
 *
 *	On Unix: 	Create the pixmap from the buffer.
 *	On Windows:	Free the mask if there are no transparent pixels.
 *----------------------------------------------------------------------
 */
void
TkimgXpmRealizePixmap(masterPtr, instancePtr, image, mask, isTransp)
    PixmapMaster * masterPtr;
    PixmapInstance * instancePtr;
    XImage * image;
    XImage * mask;
{
    Display *display = Tk_Display(instancePtr->tkwin);
    PixmapData *dataPtr = (PixmapData*)instancePtr->clientData;
    HDC dc, bitmapDC;
    TkWinDCState dcState;
    HBITMAP bitmap, bitmapOld;
    int w, h;

    w = masterPtr->size[0];
    h = masterPtr->size[1];

    dc = TkWinGetDrawableDC(display, instancePtr->pixmap, &dcState);
    bitmapDC = CreateCompatibleDC(dc);

    bitmap = CreateCompatibleBitmap(dc, w, h);
    bitmapOld = SelectObject(bitmapDC, bitmap);

    BitBlt(bitmapDC, 0, 0, w, h, dc, 0, 0, SRCCOPY);

    if (isTransp) {
	HDC maskDC;
	HBITMAP maskBm, maskBmOld;

	/*
	 * There are transparent pixels. We need a mask.
	 */
	maskDC = CreateCompatibleDC(dc);
	maskBm = CreateBitmap(w, h, 1, 1, (const VOID*)mask->data);
	maskBmOld = SelectObject(maskDC, maskBm);

	BitBlt(bitmapDC, 0, 0, w, h, maskDC, 0, 0, SRCAND);
	BitBlt(maskDC,   0, 0, w, h, maskDC, 0, 0, NOTSRCCOPY);

	TkWinReleaseDrawableDC(instancePtr->pixmap, dc, &dcState);
	dataPtr->maskDC = maskDC;
	dataPtr->maskBm = maskBm;
	dataPtr->maskBmOld = maskBmOld;
    } else {
	dataPtr->maskDC = NULL;
    }
    dataPtr->bitmapDC = bitmapDC;
    dataPtr->bitmap = bitmap;
    dataPtr->bitmapOld = bitmapOld;
}

void
TkimgXpmFreeInstanceData(instancePtr, delete)
    PixmapInstance *instancePtr;	/* Pixmap instance. */
    int delete;				/* Should the instance data structure
					 * be deleted as well? */
{
    PixmapData *dataPtr = (PixmapData*)instancePtr->clientData;

    if (dataPtr->maskDC != NULL) {
	DeleteObject(SelectObject(dataPtr->maskDC,
	    dataPtr->maskBmOld));
	DeleteDC(dataPtr->maskDC);
	dataPtr->maskDC = NULL;
    }
    if (dataPtr->bitmapDC != NULL) {
	DeleteObject(SelectObject(dataPtr->bitmapDC,
	    dataPtr->bitmapOld));
	DeleteDC(dataPtr->bitmapDC);
	dataPtr->bitmapDC = NULL;
    }
    if (delete) {
	ckfree((char*)dataPtr);
	instancePtr->clientData = NULL;
    }
}

void
TkimgpXpmDisplay(clientData, display, drawable, imageX, imageY, width,
	height, drawableX, drawableY)
    ClientData clientData;	/* Pointer to PixmapInstance structure for
				 * for instance to be displayed. */
    Display *display;		/* Display on which to draw image. */
    Drawable drawable;		/* Pixmap or window in which to draw image. */
    int imageX, imageY;		/* Upper-left corner of region within image
				 * to draw. */
    int width, height;		/* Dimensions of region within image to draw.*/
    int drawableX, drawableY;	/* Coordinates within drawable that
				 * correspond to imageX and imageY. */
{
    PixmapInstance *instancePtr = (PixmapInstance *) clientData;
    PixmapData *dataPtr = (PixmapData*)instancePtr->clientData;

    CopyTransparent(display, dataPtr->bitmapDC, drawable,
	imageX, imageY, width, height,
	drawableX, drawableY, dataPtr->maskDC);
}

static void
CopyTransparent(display, srcDC, dest, src_x, src_y, width, height, dest_x,
        dest_y,	maskDC)
    Display* display;
    HDC srcDC;
    Drawable dest;
    int src_x;
    int src_y;
    int width;
    int height;
    int dest_x;
    int dest_y;
    HDC maskDC;
{
    HDC destDC;
    TkWinDCState destState;

    destDC = TkWinGetDrawableDC(display, dest, &destState);

    if (maskDC) {
	BitBlt(destDC, dest_x, dest_y, width, height, maskDC, src_x, src_y,
	    SRCAND);
	BitBlt(destDC, dest_x, dest_y, width, height, srcDC, src_x, src_y,
	    SRCPAINT);
    } else {
	BitBlt(destDC, dest_x, dest_y, width, height, srcDC, src_x, src_y,
	    SRCCOPY);
    }

    TkWinReleaseDrawableDC(dest, destDC, &destState);
}


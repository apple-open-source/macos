/*
 * $XFree86: xc/lib/Xcursor/cursor.c,v 1.5 2003/01/26 03:22:42 eich Exp $
 *
 * Copyright © 2002 Keith Packard, member of The XFree86 Project, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Keith Packard not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  Keith Packard makes no
 * representations about the suitability of this software for any purpose.  It
 * is provided "as is" without express or implied warranty.
 *
 * KEITH PACKARD DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL KEITH PACKARD BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#include "xcursorint.h"
#include <X11/Xlibint.h>
#include <X11/Xutil.h>

XcursorCursors *
XcursorCursorsCreate (Display *dpy, int size)
{
    XcursorCursors  *cursors;

    cursors = malloc (sizeof (XcursorCursors) +
		      size * sizeof (Cursor));
    if (!cursors)
	return 0;
    cursors->ref = 1;
    cursors->dpy = dpy;
    cursors->ncursor = 0;
    cursors->cursors = (Cursor *) (cursors + 1);
    return cursors;
}

void
XcursorCursorsDestroy (XcursorCursors *cursors)
{
    int	    n;

    --cursors->ref;
    if (cursors->ref > 0)
	return;
    
    for (n = 0; n < cursors->ncursor; n++)
	XFreeCursor (cursors->dpy, cursors->cursors[n]);
    free (cursors);
}

XcursorAnimate *
XcursorAnimateCreate (XcursorCursors *cursors)
{
    XcursorAnimate  *animate;

    animate = malloc (sizeof (XcursorAnimate));
    if (!animate)
	return 0;
    animate->cursors = cursors;
    cursors->ref++;
    animate->sequence = 0;
    return animate;
}

void
XcursorAnimateDestroy (XcursorAnimate *animate)
{
    XcursorCursorsDestroy (animate->cursors);
    free (animate);
}

Cursor
XcursorAnimateNext (XcursorAnimate *animate)
{
    Cursor  cursor = animate->cursors->cursors[animate->sequence++];

    if (animate->sequence >= animate->cursors->ncursor)
	animate->sequence = 0;
    return cursor;
}

static int
nativeByteOrder (void)
{
    int	x = 1;

    return (*((char *) &x) == 1) ? LSBFirst : MSBFirst;
}

static XcursorUInt
_XcursorPixelBrightness (XcursorPixel p)
{
    XcursorPixel    alpha = p >> 24;
    XcursorPixel    r, g, b;

    if (!alpha)
	return 0;
    r = ((p >> 8) & 0xff00) / alpha;
    if (r > 0xff) r = 0xff;
    g = ((p >> 0) & 0xff00) / alpha;
    if (g > 0xff) g = 0xff;
    b = ((p << 8) & 0xff00) / alpha;
    if (b > 0xff) b = 0xff;
    return (r * 153 + g * 301 + b * 58) >> 9;
}

static unsigned short
_XcursorDivideAlpha (XcursorUInt value, XcursorUInt alpha)
{
    if (!alpha)
	return 0;
    value = value * 255 / alpha;
    if (value > 255)
	value = 255;
    return value | (value << 8);
}

static void
_XcursorPixelToColor (XcursorPixel p, XColor *color)
{
    XcursorPixel    alpha = p >> 24;

    color->pixel = 0;
    color->red =   _XcursorDivideAlpha ((p >> 16) & 0xff, alpha);
    color->green = _XcursorDivideAlpha ((p >>  8) & 0xff, alpha);
    color->blue =  _XcursorDivideAlpha ((p >>  0) & 0xff, alpha);
    color->flags = DoRed|DoGreen|DoBlue;
}

#undef DEBUG_IMAGE
#ifdef DEBUG_IMAGE
static void
_XcursorDumpImage (XImage *image)
{
    FILE    *f = fopen ("/tmp/images", "a");
    int	    x, y;
    if (!f)
	return;
    fprintf (f, "%d x %x\n", image->width, image->height);
    for (y = 0; y < image->height; y++)
    {
	for (x = 0; x < image->width; x++)
	    fprintf (f, "%c", XGetPixel (image, x, y) ? '*' : ' ');
	fprintf (f, "\n");
    }
    fflush (f);
    fclose (f);
}

static void
_XcursorDumpColor (XColor *color, char *name)
{
    FILE    *f = fopen ("/tmp/images", "a");
    fprintf (f, "%s: %x %x %x\n", name,
	     color->red, color->green, color->blue);
    fflush (f);
    fclose (f);
}
#endif

static int
_XcursorCompareRed (const void *a, const void *b)
{
    const XcursorPixel    *ap = a, *bp = b;

    return (int) (((*ap >> 16) & 0xff) - ((*bp >> 16) & 0xff));
}

static int
_XcursorCompareGreen (const void *a, const void *b)
{
    const XcursorPixel    *ap = a, *bp = b;

    return (int) (((*ap >> 8) & 0xff) - ((*bp >> 8) & 0xff));
}

static int
_XcursorCompareBlue (const void *a, const void *b)
{
    const XcursorPixel    *ap = a, *bp = b;

    return (int) (((*ap >> 0) & 0xff) - ((*bp >> 0) & 0xff));
}

static XcursorPixel
_XcursorAverageColor (XcursorPixel *pixels, int npixels)
{
    XcursorPixel    p;
    XcursorPixel    red, green, blue;
    int		    n = npixels;

    blue = green = red = 0;
    while (n--)
    {
	p = *pixels++;
	red += (p >> 16) & 0xff;
	green += (p >> 8) & 0xff;
	blue += (p >> 0) & 0xff;
    }
    if (!n)
	return 0;
    return (0xff << 24) | ((red/npixels) << 16) | ((green/npixels) << 8) | (blue/npixels);
}

typedef struct XcursorCoreCursor {
    XImage  *src_image;
    XImage  *msk_image;
    XColor  on_color;
    XColor  off_color;
} XcursorCoreCursor;

static Bool
_XcursorHeckbertMedianCut (const XcursorImage *image, XcursorCoreCursor *core)
{
    XImage	    *src_image = core->src_image, *msk_image = core->msk_image;
    int		    npixels = image->width * image->height;
    int		    ncolors;
    int		    n;
    XcursorPixel    *po, *pn, *pc;
    XcursorPixel    p;
    XcursorPixel    red, green, blue, alpha;
    XcursorPixel    max_red, min_red, max_green, min_green, max_blue, min_blue;
    XcursorPixel    *temp, *pixels, *colors;
    int		    split;
    XcursorPixel    leftColor, centerColor, rightColor;
    int		    (*compare) (const void *, const void *);
    int		    x, y;
    
    /*
     * Temp space for converted image and converted colors
     */
    temp = malloc (npixels * sizeof (XcursorPixel) * 2);
    if (!temp)
	return False;
    
    pixels = temp;
    colors = pixels + npixels;
    
    /*
     * Convert to 2-value alpha and build
     * array of opaque color values and an
     */
    po = image->pixels;
    pn = pixels;
    pc = colors;
    max_blue = max_green = max_red = 0;
    min_blue = min_green = min_red = 255;
    n = npixels;
    while (n--)
    {
	p = *po++;
	alpha = (p >> 24) & 0xff;
	red = (p >> 16) & 0xff;
	green = (p >> 8) & 0xff;
	blue = (p >> 0) & 0xff;
	if (alpha >= 0x80)
	{
	    red = red * 255 / alpha;
	    green = green * 255 / alpha;
	    blue = blue * 255 / alpha;
	    if (red < min_red) min_red = red;
	    if (red > max_red) max_red = red;
	    if (green < min_green) min_green = green;
	    if (green > max_green) max_green = green;
	    if (blue < min_blue) min_blue = blue;
	    if (blue > max_blue) max_blue = blue;
	    p = ((0xff << 24) | (red << 16) | 
		 (green << 8) | (blue << 0));
	    *pc++ = p;
	}
	else
	    p = 0;
	*pn++ = p;
    }
    ncolors = pc - colors;
    
    /*
     * Compute longest dimension and sort
     */
    if ((max_green - min_green) >= (max_red - min_red) &&
	(max_green - min_green) >= (max_blue - min_blue))
	compare = _XcursorCompareGreen;
    else if ((max_red - min_red) >= (max_blue - min_blue))
	compare = _XcursorCompareRed;
    else
	compare = _XcursorCompareBlue;
    qsort (colors, ncolors, sizeof (XcursorPixel), compare);
    /*
     * Compute average colors on both sides of the cut
     */
    split = ncolors >> 1;
    leftColor  = _XcursorAverageColor (colors, split);
    centerColor = colors[split];
    rightColor = _XcursorAverageColor (colors + split, ncolors - split);
    /*
     * Select best color for each pixel
     */
    pn = pixels;
    for (y = 0; y < image->height; y++)
	for (x = 0; x < image->width; x++)
	{
	    p = *pn++;
	    if (p & 0xff000000)
	    {
		XPutPixel (msk_image, x, y, 1);
		if ((*compare) (&p, &centerColor) >= 0)
		    XPutPixel (src_image, x, y, 0);
		else
		    XPutPixel (src_image, x, y, 1);
	    }
	    else
	    {
		XPutPixel (msk_image, x, y, 0);
		XPutPixel (src_image, x, y, 0);
	    }
	}
    free (temp);
    _XcursorPixelToColor (rightColor, &core->off_color);
    _XcursorPixelToColor (leftColor, &core->on_color);
    return True;
}

#if 0
#define DITHER_DIM  4
static XcursorPixel orderedDither[4][4] = {
    {  1,  9,  3, 11 },
    { 13,  5, 15,  7 },
    {  4, 12,  2, 10 },
    { 16,  8, 14,  6 }
};
#else
#define DITHER_DIM 2
static XcursorPixel orderedDither[2][2] = {
    {  1,  3,  },
    {  4,  2,  },
};
#endif

#define DITHER_SIZE  ((sizeof orderedDither / sizeof orderedDither[0][0]) + 1)

static Bool
_XcursorBayerOrderedDither (const XcursorImage *image, XcursorCoreCursor *core)
{
    int		    x, y;
    XcursorPixel    *pixel, p;
    XcursorPixel    a, i, d;

    pixel = image->pixels;
    for (y = 0; y < image->height; y++)
	for (x = 0; x < image->width; x++)
	{
	    p = *pixel++;
	    a = ((p >> 24) * DITHER_SIZE + 127) / 255;
	    i = (_XcursorPixelBrightness (p) * DITHER_SIZE + 127) / 255;
	    d = orderedDither[y&(DITHER_DIM-1)][x&(DITHER_DIM-1)];
	    if (a > d)
	    {
		XPutPixel (core->msk_image, x, y, 1);
		if (i > d)
		    XPutPixel (core->src_image, x, y, 0);   /* white */
		else
		    XPutPixel (core->src_image, x, y, 1);   /* black */
	    }
	    else
	    {
		XPutPixel (core->msk_image, x, y, 0);
		XPutPixel (core->src_image, x, y, 0);
	    }
	}
    core->on_color.red = 0;
    core->on_color.green = 0;
    core->on_color.blue = 0;
    core->off_color.red = 0xffff;
    core->off_color.green = 0xffff;
    core->off_color.blue = 0xffff;
    return True;
}

static Bool
_XcursorFloydSteinberg (const XcursorImage *image, XcursorCoreCursor *core)
{
    int		    *aPicture, *iPicture, *aP, *iP;
    XcursorPixel    *pixel, p;
    int		    aR, iR, aA, iA;
    int		    npixels = image->width * image->height;
    int		    n;
    int		    right = 1;
    int		    belowLeft = image->width - 1;
    int		    below = image->width;
    int		    belowRight = image->width + 1;
    int		    iError, aError;
    int		    iErrorRight, aErrorRight;
    int		    iErrorBelowLeft, aErrorBelowLeft;
    int		    iErrorBelow, aErrorBelow;
    int		    iErrorBelowRight, aErrorBelowRight;
    int		    x, y;
    int		    max_inten, min_inten, mean_inten;

    iPicture = malloc (npixels * sizeof (int) * 2);
    if (!iPicture)
	return False;
    aPicture = iPicture + npixels;

    /*
     * Compute raw gray and alpha arrays
     */
    pixel = image->pixels;
    iP = iPicture;
    aP = aPicture;
    n = npixels;
    max_inten = 0;
    min_inten = 0xff;
    while (n--)
    {
	p = *pixel++;
	*aP++ = (int) (p >> 24);
	iR = (int) _XcursorPixelBrightness (p);
	if (iR > max_inten) max_inten = iR;
	if (iR < min_inten) min_inten = iR;
	*iP++ = iR;
    }
    /*
     * Draw the image while diffusing the error
     */
    iP = iPicture;
    aP = aPicture;
    mean_inten = (max_inten + min_inten + 1) >> 1;
    for (y = 0; y < image->height; y++)
	for (x = 0; x < image->width; x++)
	{
	    aR = *aP;
	    iR = *iP;
	    if (aR >= 0x80)
	    {
		XPutPixel (core->msk_image, x, y, 1);
		aA = 0xff;
	    }
	    else
	    {
		XPutPixel (core->msk_image, x, y, 0);
		aA = 0x00;
	    }
	    if (iR >= mean_inten)
	    {
		XPutPixel (core->src_image, x, y, 0);
		iA = max_inten;
	    }
	    else
	    {
		XPutPixel (core->src_image, x, y, 1);
		iA = min_inten;
	    }
	    iError = iR - iA;
	    aError = aR - aA;
	    iErrorRight = (iError * 7) >> 4;
	    iErrorBelowLeft = (iError * 3) >> 4;
	    iErrorBelow = (iError * 5) >> 4;
	    iErrorBelowRight = (iError - iErrorRight - 
				iErrorBelowLeft - iErrorBelow);
	    aErrorRight = (aError * 7) >> 4;
	    aErrorBelowLeft = (aError * 3) >> 4;
	    aErrorBelow = (aError * 5) >> 4;
	    aErrorBelowRight = (aError - aErrorRight - 
				aErrorBelowLeft - aErrorBelow);
	    if (x < image->width - 1)
	    {
		iP[right] += iErrorRight; 
		aP[right] += aErrorRight;
	    }
	    if (y < image->height - 1)
	    {
		if (x)
		{
		    iP[belowLeft] += iErrorBelowLeft;
		    aP[belowLeft] += aErrorBelowLeft;
		}
		iP[below] += iErrorBelow;
		aP[below] += aErrorBelow;
		if (x < image->width - 1)
		{
		    iP[belowRight] += iErrorBelowRight;
		    aP[belowRight] += aErrorBelowRight;
		}
	    }
	    aP++;
	    iP++;
	}
    free (iPicture);
    core->on_color.red =
    core->on_color.green = 
    core->on_color.blue = (min_inten | min_inten << 8);
    core->off_color.red = 
    core->off_color.green =
    core->off_color.blue = (max_inten | max_inten << 8);
    return True;
}

static Bool
_XcursorThreshold (const XcursorImage *image, XcursorCoreCursor *core)
{
    XcursorPixel    *pixel, p;
    int		    x, y;

    /*
     * Draw the image, picking black for dark pixels and white for light
     */
    pixel = image->pixels;
    for (y = 0; y < image->height; y++)
	for (x = 0; x < image->width; x++)
	{
	    p = *pixel++;
	    if ((p >> 24) >= 0x80)
	    {
		XPutPixel (core->msk_image, x, y, 1);
		if (_XcursorPixelBrightness (p) > 0x80)
		    XPutPixel (core->src_image, x, y, 0);
		else
		    XPutPixel (core->src_image, x, y, 1);
	    }
	    else
	    {
		XPutPixel (core->msk_image, x, y, 0);
		XPutPixel (core->src_image, x, y, 0);
	    }
	}
    core->on_color.red =
    core->on_color.green = 
    core->on_color.blue = 0;
    core->off_color.red = 
    core->off_color.green =
    core->off_color.blue = 0xffff;
    return True;
}

Cursor
XcursorImageLoadCursor (Display *dpy, const XcursorImage *image)
{
    Cursor  cursor;
    
#if RENDER_MAJOR > 0 || RENDER_MINOR >= 5
    if (XcursorSupportsARGB (dpy))
    {
	XImage		    ximage;
	int		    screen = DefaultScreen (dpy);
	Pixmap		    pixmap;
	Picture		    picture;
	GC		    gc;
	XRenderPictFormat   *format;

	ximage.width = image->width;
	ximage.height = image->height;
	ximage.xoffset = 0;
	ximage.format = ZPixmap;
	ximage.data = (char *) image->pixels;
	ximage.byte_order = nativeByteOrder ();
	ximage.bitmap_unit = 32;
	ximage.bitmap_bit_order = ximage.byte_order;
	ximage.bitmap_pad = 32;
	ximage.depth = 32;
	ximage.bits_per_pixel = 32;
	ximage.bytes_per_line = image->width * 4;
	ximage.red_mask = 0xff0000;
	ximage.green_mask = 0x00ff00;
	ximage.blue_mask = 0x0000ff;
	ximage.obdata = 0;
	if (!XInitImage (&ximage))
	    return None;
	pixmap = XCreatePixmap (dpy, RootWindow (dpy, screen),
				image->width, image->height, 32);
	gc = XCreateGC (dpy, pixmap, 0, 0);
	XPutImage (dpy, pixmap, gc, &ximage, 
		   0, 0, 0, 0, image->width, image->height);
	XFreeGC (dpy, gc);
	format = XRenderFindStandardFormat (dpy, PictStandardARGB32);
	picture = XRenderCreatePicture (dpy, pixmap, format, 0, 0);
	XFreePixmap (dpy, pixmap);
	cursor = XRenderCreateCursor (dpy, picture, 
				      image->xhot, image->yhot);
	XRenderFreePicture (dpy, picture);
    }
    else
#endif
    {
	XcursorDisplayInfo  *info = _XcursorGetDisplayInfo (dpy);
	int		    screen = DefaultScreen (dpy);
	XcursorCoreCursor   core;
	Pixmap		    src_pixmap, msk_pixmap;
	GC		    gc;
	XGCValues	    gcv;

	core.src_image = XCreateImage (dpy, 0, 1, ZPixmap,
				       0, 0, image->width, image->height,
				       32, 0);
	core.src_image->data = Xmalloc (image->height * 
					core.src_image->bytes_per_line);
	core.msk_image = XCreateImage (dpy, 0, 1, ZPixmap,
				       0, 0, image->width, image->height,
				       32, 0);
	core.msk_image->data = Xmalloc (image->height * 
					core.msk_image->bytes_per_line);

	switch (info->dither) {
	case XcursorDitherThreshold:
	    if (!_XcursorThreshold (image, &core))
		return 0;
	    break;
	case XcursorDitherMedian:
	    if (!_XcursorHeckbertMedianCut (image, &core))
		return 0;
	    break;
	case XcursorDitherOrdered:
	    if (!_XcursorBayerOrderedDither (image, &core))
		return 0;
	    break;
	case XcursorDitherDiffuse:
	    if (!_XcursorFloydSteinberg (image, &core))
		return 0;
	    break;
	default:
	    return 0;
	}

	/*
	 * Create the cursor
	 */
	src_pixmap = XCreatePixmap (dpy, RootWindow (dpy, screen),
				    image->width, image->height, 1);
	msk_pixmap = XCreatePixmap (dpy, RootWindow (dpy, screen),
				    image->width, image->height, 1);
	gcv.foreground = 1;
	gcv.background = 0;
	gc = XCreateGC (dpy, src_pixmap, 
			GCForeground|GCBackground,
			&gcv);
	XPutImage (dpy, src_pixmap, gc, core.src_image,
		   0, 0, 0, 0, image->width, image->height);
	
	XPutImage (dpy, msk_pixmap, gc, core.msk_image,
		   0, 0, 0, 0, image->width, image->height);
	XFreeGC (dpy, gc);
	
#ifdef DEBUG_IMAGE
	_XcursorDumpColor (&core.on_color, "on_color");
	_XcursorDumpColor (&core.off_color, "off_color");
	_XcursorDumpImage (core.src_image);
	_XcursorDumpImage (core.msk_image);
#endif
	XDestroyImage (core.src_image);
	XDestroyImage (core.msk_image);

	cursor = XCreatePixmapCursor (dpy, src_pixmap, msk_pixmap,
				      &core.on_color, &core.off_color,
				      image->xhot, image->yhot);
	XFreePixmap (dpy, src_pixmap);
	XFreePixmap (dpy, msk_pixmap);
    }
    return cursor;
}

XcursorCursors *
XcursorImagesLoadCursors (Display *dpy, const XcursorImages *images)
{
    XcursorCursors  *cursors = XcursorCursorsCreate (dpy, images->nimage);
    int		    n;

    if (!cursors)
	return 0;
    for (n = 0; n < images->nimage; n++)
    {
	cursors->cursors[n] = XcursorImageLoadCursor (dpy, images->images[n]);
	if (!cursors->cursors[n])
	{
	    XcursorCursorsDestroy (cursors);
	    return 0;
	}
	cursors->ncursor++;
    }
    return cursors;
}

Cursor
XcursorImagesLoadCursor (Display *dpy, const XcursorImages *images)
{
    if (images->nimage == 1 || !XcursorSupportsAnim (dpy))
	return XcursorImageLoadCursor (dpy, images->images[0]);
    else
    {
	XcursorCursors	*cursors = XcursorImagesLoadCursors (dpy, images);
	XAnimCursor	*anim;
	int		n;
	Cursor		cursor;
	
	if (!cursors)
	    return 0;
	anim = malloc (cursors->ncursor * sizeof (XAnimCursor));
	if (!anim)
	{
	    XcursorCursorsDestroy (cursors);
	    return 0;
	}
	for (n = 0; n < cursors->ncursor; n++)
	{
	    anim[n].cursor = cursors->cursors[n];
	    anim[n].delay = images->images[n]->delay;
	}
	cursor = XRenderCreateAnimCursor (dpy, cursors->ncursor, anim);
	free (anim);
	return cursor;
    }
}


Cursor
XcursorFilenameLoadCursor (Display *dpy, const char *file)
{
    int		    size = XcursorGetDefaultSize (dpy);
    XcursorImages   *images = XcursorFilenameLoadImages (file, size);
    Cursor	    cursor;
    
    if (!images)
	return None;
    cursor = XcursorImagesLoadCursor (dpy, images);
    XcursorImagesDestroy (images);
    return cursor;
}

XcursorCursors *
XcursorFilenameLoadCursors (Display *dpy, const char *file)
{
    int		    size = XcursorGetDefaultSize (dpy);
    XcursorImages   *images = XcursorFilenameLoadImages (file, size);
    XcursorCursors  *cursors;
    
    if (!images)
	return 0;
    cursors = XcursorImagesLoadCursors (dpy, images);
    XcursorImagesDestroy (images);
    return cursors;
}

/*
 * Stolen from XCreateGlyphCursor (which we cruelly override)
 */

Cursor
_XcursorCreateGlyphCursor(Display	    *dpy,
			  Font		    source_font,
			  Font		    mask_font,
			  unsigned int	    source_char,
			  unsigned int	    mask_char,
			  XColor _Xconst    *foreground,
			  XColor _Xconst    *background)
{       
    Cursor cid;
    register xCreateGlyphCursorReq *req;

    LockDisplay(dpy);
    GetReq(CreateGlyphCursor, req);
    cid = req->cid = XAllocID(dpy);
    req->source = source_font;
    req->mask = mask_font;
    req->sourceChar = source_char;
    req->maskChar = mask_char;
    req->foreRed = foreground->red;
    req->foreGreen = foreground->green;
    req->foreBlue = foreground->blue;
    req->backRed = background->red;
    req->backGreen = background->green;
    req->backBlue = background->blue;
    UnlockDisplay(dpy);
    SyncHandle();
    return (cid);
}

/*
 * Stolen from XCreateFontCursor (which we cruelly override)
 */

Cursor
_XcursorCreateFontCursor (Display *dpy, unsigned int shape)
{
    static XColor _Xconst foreground = { 0,    0,     0,     0  };  /* black */
    static XColor _Xconst background = { 0, 65535, 65535, 65535 };  /* white */

    /* 
     * the cursor font contains the shape glyph followed by the mask
     * glyph; so character position 0 contains a shape, 1 the mask for 0,
     * 2 a shape, etc.  <X11/cursorfont.h> contains hash define names
     * for all of these.
     */

    if (dpy->cursor_font == None) 
    {
	dpy->cursor_font = XLoadFont (dpy, CURSORFONT);
	if (dpy->cursor_font == None)
	    return None;
    }

    return _XcursorCreateGlyphCursor (dpy, dpy->cursor_font, dpy->cursor_font, 
				      shape, shape + 1, &foreground, &background);
}


/*
 * $XFree86: xc/lib/Xft1/xftdraw.c,v 1.1.1.1 2002/02/15 01:26:15 keithp Exp $
 *
 * Copyright © 2000 Keith Packard, member of The XFree86 Project, Inc.
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "xftint.h"
#include <X11/Xutil.h>

XftDraw *
XftDrawCreate (Display   *dpy,
	       Drawable  drawable,
	       Visual    *visual,
	       Colormap  colormap)
{
    XftDraw	*draw;

    draw = (XftDraw *) malloc (sizeof (XftDraw));
    if (!draw)
	return 0;
    draw->dpy = dpy;
    draw->drawable = drawable;
    draw->visual = visual;
    draw->colormap = colormap;
    draw->core_set = False;
    draw->render_set = False;
    draw->render_able = False;
    draw->clip = 0;
    return draw;
}

XftDraw *
XftDrawCreateBitmap (Display	*dpy,
		     Pixmap	bitmap)
{
    XftDraw	*draw;

    draw = (XftDraw *) malloc (sizeof (XftDraw));
    if (!draw)
	return 0;
    draw->dpy = dpy;
    draw->drawable = (Drawable) bitmap;
    draw->visual = 0;
    draw->colormap = 0;
    draw->core_set = False;
    draw->render_set = False;
    draw->render_able = False;
    draw->clip = 0;
    return draw;
}

static XRenderPictFormat *
_XftDrawFormat (XftDraw	*draw)
{
    if (draw->visual == 0)
    {
	XRenderPictFormat   pf;

	pf.type = PictTypeDirect;
	pf.depth = 1;
	pf.direct.alpha = 0;
	pf.direct.alphaMask = 1;
	return XRenderFindFormat (draw->dpy,
				  (PictFormatType|
				   PictFormatDepth|
				   PictFormatAlpha|
				   PictFormatAlphaMask),
				  &pf,
				  0);
    }
    else
	return XRenderFindVisualFormat (draw->dpy, draw->visual);
}

static XRenderPictFormat *
_XftDrawFgFormat (XftDraw	*draw)
{
    XRenderPictFormat   pf;

    if (draw->visual == 0)
    {
	pf.type = PictTypeDirect;
	pf.depth = 1;
	pf.direct.alpha = 0;
	pf.direct.alphaMask = 1;
	return XRenderFindFormat (draw->dpy,
				  (PictFormatType|
				   PictFormatDepth|
				   PictFormatAlpha|
				   PictFormatAlphaMask),
				  &pf,
				  0);
    }
    else
    {
	pf.type = PictTypeDirect;
	pf.depth = 32;
	pf.direct.redMask = 0xff;
	pf.direct.greenMask = 0xff;
	pf.direct.blueMask = 0xff;
	pf.direct.alphaMask = 0xff;
	return XRenderFindFormat (draw->dpy,
				  (PictFormatType|
				   PictFormatDepth|
				   PictFormatRedMask|
				   PictFormatGreenMask|
				   PictFormatBlueMask|
				   PictFormatAlphaMask),
				  &pf,
				  0);
    }
}

void
XftDrawChange (XftDraw	*draw,
	       Drawable	drawable)
{
    draw->drawable = drawable;
    if (draw->render_able)
    {
	XRenderPictFormat	    *format;
	
	XRenderFreePicture (draw->dpy, draw->render.pict);
	format = XRenderFindVisualFormat (draw->dpy, draw->visual);
	draw->render.pict = XRenderCreatePicture (draw->dpy, draw->drawable,
						  format, 0, 0);
    }
}

void
XftDrawDestroy (XftDraw	*draw)
{
    int	n;
    
    if (draw->render_able)
    {
	XRenderFreePicture (draw->dpy, draw->render.pict);
	for (n = 0; n < XFT_DRAW_N_SRC; n++)
	    XRenderFreePicture (draw->dpy, draw->render.src[n].pict);
    }
    if (draw->core_set)
	XFreeGC (draw->dpy, draw->core.draw_gc);
    if (draw->clip)
	XDestroyRegion (draw->clip);
    free (draw);
}

Bool
XftDrawRenderPrepare (XftDraw	*draw,
		      XftColor	*color,
		      XftFont	*font,
		      int	src)
{
    if (!draw->render_set)
    {
	XRenderPictFormat	    *format;
	XRenderPictFormat	    *pix_format;
	XRenderPictureAttributes    pa;
	int			    n;
	Pixmap			    pix;

	draw->render_set = True;
	draw->render_able = False;
	format = _XftDrawFormat (draw);
	pix_format = _XftDrawFgFormat (draw);
	if (format && pix_format)
	{
	    draw->render_able = True;

	    draw->render.pict = XRenderCreatePicture (draw->dpy, draw->drawable,
						      format, 0, 0);
	    for (n = 0; n < XFT_DRAW_N_SRC; n++)
	    {
		pix = XCreatePixmap (draw->dpy, draw->drawable,
				     1, 1, pix_format->depth);
		pa.repeat = True;
		draw->render.src[n].pict = XRenderCreatePicture (draw->dpy, 
								 pix,
								 pix_format,
								 CPRepeat, &pa);
		XFreePixmap (draw->dpy, pix);
		
		draw->render.src[n].color = color->color;
		XRenderFillRectangle (draw->dpy, PictOpSrc, 
				      draw->render.src[n].pict,
				      &color->color, 0, 0, 1, 1);
	    }
	    if (draw->clip)
		XRenderSetPictureClipRegion (draw->dpy, draw->render.pict,
					     draw->clip);
	}
    }
    if (!draw->render_able)
	return False;
    if (memcmp (&color->color, &draw->render.src[src].color, 
		sizeof (XRenderColor)))
    {
	if (_XftFontDebug () & XFT_DBG_DRAW)
	{
	    printf ("Switching to color %04x,%04x,%04x,%04x\n",
		    color->color.alpha,
		    color->color.red,
		    color->color.green,
		    color->color.blue);
	}
	XRenderFillRectangle (draw->dpy, PictOpSrc, 
			      draw->render.src[src].pict,
			      &color->color, 0, 0, 1, 1);
	draw->render.src[src].color = color->color;
    }
    return True;
}

Bool
XftDrawCorePrepare (XftDraw	*draw,
		    XftColor	*color,
		    XftFont	*font)
{

    if (!draw->core_set)
    {
	XGCValues	    gcv;
	unsigned long	    mask;
	draw->core_set = True;

	draw->core.fg = color->pixel;
	gcv.foreground = draw->core.fg;
	mask = GCForeground;
	if (font)
	{
	    draw->core.font = font->u.core.font->fid;
	    gcv.font = draw->core.font;
	    mask |= GCFont;
	}
	draw->core.draw_gc = XCreateGC (draw->dpy, draw->drawable, 
					mask, &gcv);
	if (draw->clip)
	    XSetRegion (draw->dpy, draw->core.draw_gc, draw->clip);
    }
    if (draw->core.fg != color->pixel)
    {
	draw->core.fg = color->pixel;
	XSetForeground (draw->dpy, draw->core.draw_gc, draw->core.fg);
    }
    if (font && draw->core.font != font->u.core.font->fid)
    {
	draw->core.font = font->u.core.font->fid;
	XSetFont (draw->dpy, draw->core.draw_gc, draw->core.font);
    }
    return True;
}

void
XftDrawString8 (XftDraw		*draw,
		XftColor	*color,
		XftFont		*font,
		int		x,
		int		y,
		XftChar8	*string,
		int		len)
{
    if (_XftFontDebug () & XFT_DBG_DRAW)
    {
	printf ("DrawString \"%*.*s\"\n", len, len, string);
    }
    if (font->core)
    {
	XftDrawCorePrepare (draw, color, font);
	XDrawString (draw->dpy, draw->drawable, draw->core.draw_gc, x, y, 
		     (char *) string, len);
    }
#ifdef FREETYPE2
    else if (XftDrawRenderPrepare (draw, color, font, XFT_DRAW_SRC_TEXT))
    {
	XftRenderString8 (draw->dpy,
			  draw->render.src[XFT_DRAW_SRC_TEXT].pict, 
			  font->u.ft.font,
			  draw->render.pict, 0, 0, x, y, string, len);
    }
#endif
}

#define N16LOCAL    256

void
XftDrawString16 (XftDraw	*draw,
		 XftColor	*color,
		 XftFont	*font,
		 int		x,
		 int		y,
		 XftChar16	*string,
		 int		len)
{
    if (font->core)
    {
	XChar2b	    *xc;
	XChar2b	    xcloc[XFT_CORE_N16LOCAL];
	
	XftDrawCorePrepare (draw, color, font);
	xc = XftCoreConvert16 (string, len, xcloc);
	XDrawString16 (draw->dpy, draw->drawable, draw->core.draw_gc, x, y, 
		       xc, len);
	if (xc != xcloc)
	    free (xc);
    }
#ifdef FREETYPE2
    else if (XftDrawRenderPrepare (draw, color, font, XFT_DRAW_SRC_TEXT))
    {
	XftRenderString16 (draw->dpy, 
			   draw->render.src[XFT_DRAW_SRC_TEXT].pict, 
			   font->u.ft.font,
			   draw->render.pict, 0, 0, x, y, string, len);
    }
#endif
}

void
XftDrawString32 (XftDraw	*draw,
		 XftColor	*color,
		 XftFont	*font,
		 int		x,
		 int		y,
		 XftChar32	*string,
		 int		len)
{
    if (font->core)
    {
	XChar2b	    *xc;
	XChar2b	    xcloc[XFT_CORE_N16LOCAL];
	
	XftDrawCorePrepare (draw, color, font);
	xc = XftCoreConvert32 (string, len, xcloc);
	XDrawString16 (draw->dpy, draw->drawable, draw->core.draw_gc, x, y, 
		       xc, len);
	if (xc != xcloc)
	    free (xc);
    }
#ifdef FREETYPE2
    else if (XftDrawRenderPrepare (draw, color, font, XFT_DRAW_SRC_TEXT))
    {
	XftRenderString32 (draw->dpy, 
			   draw->render.src[XFT_DRAW_SRC_TEXT].pict, 
			   font->u.ft.font,
			   draw->render.pict, 0, 0, x, y, string, len);
    }
#endif
}

void
XftDrawStringUtf8 (XftDraw	*draw,
		   XftColor	*color,
		   XftFont	*font,
		   int		x,
		   int		y,
		   XftChar8	*string,
		   int		len)
{
    if (font->core)
    {
	XChar2b	    *xc;
	XChar2b	    xcloc[XFT_CORE_N16LOCAL];
	int	    n;
	
	XftDrawCorePrepare (draw, color, font);
	xc = XftCoreConvertUtf8 (string, len, xcloc, &n);
	if (xc)
	{
	    XDrawString16 (draw->dpy, draw->drawable, draw->core.draw_gc, x, y, 
			   xc, n);
	}
	if (xc != xcloc)
	    free (xc);
    }
#ifdef FREETYPE2
    else if (XftDrawRenderPrepare (draw, color, font, XFT_DRAW_SRC_TEXT))
    {
	XftRenderStringUtf8 (draw->dpy,
			     draw->render.src[XFT_DRAW_SRC_TEXT].pict, 
			     font->u.ft.font,
			     draw->render.pict, 0, 0, x, y, string, len);
    }
#endif
}


void
XftDrawRect (XftDraw	    *draw,
	     XftColor	    *color,
	     int	    x, 
	     int	    y,
	     unsigned int   width,
	     unsigned int   height)
{
    if (XftDrawRenderPrepare (draw, color, 0, XFT_DRAW_SRC_RECT))
    {
	XRenderFillRectangle (draw->dpy, PictOpOver, draw->render.pict,
			      &color->color, x, y, width, height);
    }
    else
    {
	XftDrawCorePrepare (draw, color, 0);
	XFillRectangle (draw->dpy, draw->drawable, draw->core.draw_gc,
			x, y, width, height);
    }
}

Bool
XftDrawSetClip (XftDraw	*draw,
		Region	r)
{
    Region			n = 0;

    if (!r && !draw->clip)
	return True;

    if (r)
    {
	n = XCreateRegion ();
	if (n)
	{
	    if (!XUnionRegion (n, r, n))
	    {
		XDestroyRegion (n);
		return False;
	    }
	}
    }
    if (draw->clip)
    {
	XDestroyRegion (draw->clip);
    }
    draw->clip = n;
    if (draw->render_able)
    {
	XRenderPictureAttributes	pa;
        if (n)
	{
	    XRenderSetPictureClipRegion (draw->dpy, draw->render.pict, n);
	}
	else
	{
	    pa.clip_mask = None;
	    XRenderChangePicture (draw->dpy, draw->render.pict,
				  CPClipMask, &pa);
	}
    }
    if (draw->core_set)
    {
	XGCValues   gv;
	
	if (n)
	    XSetRegion (draw->dpy, draw->core.draw_gc, n);
	else
	{
	    gv.clip_mask = None;
	    XChangeGC (draw->dpy, draw->core.draw_gc,
		       GCClipMask, &gv);
	}
    }
    return True;
}

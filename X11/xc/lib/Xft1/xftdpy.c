/*
 * $XFree86: xc/lib/Xft1/xftdpy.c,v 1.2 2002/11/24 01:56:56 keithp Exp $
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
#include <ctype.h>
#include <X11/Xlibint.h>
#include "xftint.h"

XftDisplayInfo	*_XftDisplayInfo;

static int
_XftCloseDisplay (Display *dpy, XExtCodes *codes)
{
    XftDisplayInfo  *info, **prev;

    for (prev = &_XftDisplayInfo; (info = *prev); prev = &(*prev)->next)
	if (info->codes == codes)
	    break;
    if (!info)
	return 0;
    *prev = info->next;
    if (info->defaults)
	XftPatternDestroy (info->defaults);
    if (info->coreFonts)
	XftFontSetDestroy (info->coreFonts);
    free (info);
    return 0;
}

XftDisplayInfo *
_XftDisplayInfoGet (Display *dpy)
{
    XftDisplayInfo  *info, **prev;

    for (prev = &_XftDisplayInfo; (info = *prev); prev = &(*prev)->next)
    {
	if (info->display == dpy)
	{
	    /*
	     * MRU the list
	     */
	    if (prev != &_XftDisplayInfo)
	    {
		*prev = info->next;
		info->next = _XftDisplayInfo;
		_XftDisplayInfo = info;
	    }
	    return info;
	}
    }
    info = (XftDisplayInfo *) malloc (sizeof (XftDisplayInfo));
    if (!info)
	goto bail0;
    info->codes = XAddExtension (dpy);
    if (!info->codes)
	goto bail1;
    (void) XESetCloseDisplay (dpy, info->codes->extension, _XftCloseDisplay);

    info->display = dpy;
    info->defaults = 0;
    info->coreFonts = 0;
    info->hasRender = XRenderFindVisualFormat (dpy, DefaultVisual (dpy, DefaultScreen (dpy))) != 0;
    info->glyphSets = 0;
    if (_XftFontDebug () & XFT_DBG_RENDER)
    {
	Visual		    *visual = DefaultVisual (dpy, DefaultScreen (dpy));
	XRenderPictFormat   *format = XRenderFindVisualFormat (dpy, visual);
	
	printf ("XftDisplayInfoGet Default visual 0x%x ", 
		(int) visual->visualid);
	if (format)
	{
	    if (format->type == PictTypeDirect)
	    {
		printf ("format %d,%d,%d,%d\n",
			format->direct.alpha,
			format->direct.red,
			format->direct.green,
			format->direct.blue);
	    }
	    else
	    {
		printf ("format indexed\n");
	    }
	}
	else
	    printf ("No Render format for default visual\n");
	
	printf ("XftDisplayInfoGet initialized, hasRender set to \"%s\"\n",
		info->hasRender ? "True" : "False");
    }
    
    info->next = _XftDisplayInfo;
    _XftDisplayInfo = info;
    return info;
    
bail1:
    free (info);
bail0:
    if (_XftFontDebug () & XFT_DBG_RENDER)
    {
	printf ("XftDisplayInfoGet failed to initialize, Xft unhappy\n");
    }
    return 0;
}

Bool
XftDefaultHasRender (Display *dpy)
{
#ifdef FREETYPE2
    XftDisplayInfo  *info = _XftDisplayInfoGet (dpy);

    if (!info)
	return False;
    return info->hasRender;
#else
    return False;
#endif
}

Bool
XftDefaultSet (Display *dpy, XftPattern *defaults)
{
    XftDisplayInfo  *info = _XftDisplayInfoGet (dpy);

    if (!info)
	return False;
    if (info->defaults)
	XftPatternDestroy (info->defaults);
    info->defaults = defaults;
    return True;
}

int
XftDefaultParseBool (char *v)
{
    char    c0, c1;

    c0 = *v;
    if (isupper (c0))
	c0 = tolower (c0);
    if (c0 == 't' || c0 == 'y' || c0 == '1')
	return 1;
    if (c0 == 'f' || c0 == 'n' || c0 == '0')
	return 0;
    if (c0 == 'o')
    {
	c1 = v[1];
	if (isupper (c1))
	    c1 = tolower (c1);
	if (c1 == 'n')
	    return 1;
	if (c1 == 'f')
	    return 0;
    }
    return -1;
}

static Bool
_XftDefaultInitBool (Display *dpy, XftPattern *pat, char *option)
{
    char    *v;
    int	    i;

    v = XGetDefault (dpy, "Xft", option);
    if (v && (i = XftDefaultParseBool (v)) >= 0)
	return XftPatternAddBool (pat, option, i != 0);
    return True;
}

static Bool
_XftDefaultInitDouble (Display *dpy, XftPattern *pat, char *option)
{
    char    *v, *e;
    double  d;

    v = XGetDefault (dpy, "Xft", option);
    if (v)
    {
	d = strtod (v, &e);
	if (e != v)
	    return XftPatternAddDouble (pat, option, d);
    }
    return True;
}

static Bool
_XftDefaultInitInteger (Display *dpy, XftPattern *pat, char *option)
{
    char    *v, *e;
    int	    i;

    v = XGetDefault (dpy, "Xft", option);
    if (v)
    {
	if (XftNameConstant (v, &i))
	    return XftPatternAddInteger (pat, option, i);
	i = strtol (v, &e, 0);
	if (e != v)
	    return XftPatternAddInteger (pat, option, i);
    }
    return True;
}

static XftPattern *
_XftDefaultInit (Display *dpy)
{
    XftPattern	*pat;

    pat = XftPatternCreate ();
    if (!pat)
	goto bail0;

    if (!_XftDefaultInitBool (dpy, pat, XFT_CORE))
	goto bail1;
    if (!_XftDefaultInitDouble (dpy, pat, XFT_SCALE))
	goto bail1;
    if (!_XftDefaultInitDouble (dpy, pat, XFT_DPI))
	goto bail1;
    if (!_XftDefaultInitBool (dpy, pat, XFT_RENDER))
	goto bail1;
    if (!_XftDefaultInitInteger (dpy, pat, XFT_RGBA))
	goto bail1;
    if (!_XftDefaultInitBool (dpy, pat, XFT_ANTIALIAS))
	goto bail1;
    if (!_XftDefaultInitBool (dpy, pat, XFT_MINSPACE))
	goto bail1;
    
    return pat;
    
bail1:
    XftPatternDestroy (pat);
bail0:
    return 0;
}

static XftResult
_XftDefaultGet (Display *dpy, const char *object, int screen, XftValue *v)
{
    XftDisplayInfo  *info = _XftDisplayInfoGet (dpy);
    XftResult	    r;

    if (!info)
	return XftResultNoMatch;
    
    if (!info->defaults)
    {
	info->defaults = _XftDefaultInit (dpy);
	if (!info->defaults)
	    return XftResultNoMatch;
    }
    r = XftPatternGet (info->defaults, object, screen, v);
    if (r == XftResultNoId && screen > 0)
	r = XftPatternGet (info->defaults, object, 0, v);
    return r;
}

Bool
XftDefaultGetBool (Display *dpy, const char *object, int screen, Bool def)
{
    XftResult	    r;
    XftValue	    v;

    r = _XftDefaultGet (dpy, object, screen, &v);
    if (r != XftResultMatch || v.type != XftTypeBool)
	return def;
    return v.u.b;
}

int
XftDefaultGetInteger (Display *dpy, const char *object, int screen, int def)
{
    XftResult	    r;
    XftValue	    v;

    r = _XftDefaultGet (dpy, object, screen, &v);
    if (r != XftResultMatch || v.type != XftTypeInteger)
	return def;
    return v.u.i;
}

double
XftDefaultGetDouble (Display *dpy, const char *object, int screen, double def)
{
    XftResult	    r;
    XftValue	    v;

    r = _XftDefaultGet (dpy, object, screen, &v);
    if (r != XftResultMatch || v.type != XftTypeDouble)
	return def;
    return v.u.d;
}

XftFontSet *
XftDisplayGetFontSet (Display *dpy)
{
    XftDisplayInfo  *info = _XftDisplayInfoGet (dpy);

    if (!info)
	return 0;
    if (!info->coreFonts)
    {
	info->coreFonts = XftFontSetCreate ();
	if (info->coreFonts)
	{
	    if (!XftCoreAddFonts (info->coreFonts, dpy,
				  XftDefaultGetBool (dpy, XFT_SCALABLE,
						     DefaultScreen (dpy),
						     False)))
	    {
		XftFontSetDestroy (info->coreFonts);
		info->coreFonts = 0;
	    }
	}
    }
    return info->coreFonts;
}

void
XftDefaultSubstitute (Display *dpy, int screen, XftPattern *pattern)
{
    XftValue	v;
    double	size;
    double	scale;

    if (XftPatternGet (pattern, XFT_STYLE, 0, &v) == XftResultNoMatch)
    {
	if (XftPatternGet (pattern, XFT_WEIGHT, 0, &v) == XftResultNoMatch )
	{
	    XftPatternAddInteger (pattern, XFT_WEIGHT, XFT_WEIGHT_MEDIUM);
	}
	if (XftPatternGet (pattern, XFT_SLANT, 0, &v) == XftResultNoMatch)
	{
	    XftPatternAddInteger (pattern, XFT_SLANT, XFT_SLANT_ROMAN);
	}
    }
    if (XftPatternGet (pattern, XFT_ENCODING, 0, &v) == XftResultNoMatch)
	XftPatternAddString (pattern, XFT_ENCODING, "iso8859-1");
    if (XftPatternGet (pattern, XFT_RENDER, 0, &v) == XftResultNoMatch)
    {
	XftPatternAddBool (pattern, XFT_RENDER,
			   XftDefaultGetBool (dpy, XFT_RENDER, screen, 
					      XftDefaultHasRender (dpy)));
    }
    if (XftPatternGet (pattern, XFT_CORE, 0, &v) == XftResultNoMatch)
    {
	XftPatternAddBool (pattern, XFT_CORE,
			   XftDefaultGetBool (dpy, XFT_CORE, screen, 
					      !XftDefaultHasRender (dpy)));
    }
    if (XftPatternGet (pattern, XFT_ANTIALIAS, 0, &v) == XftResultNoMatch)
    {
	XftPatternAddBool (pattern, XFT_ANTIALIAS,
			   XftDefaultGetBool (dpy, XFT_ANTIALIAS, screen,
					      True));
    }
    if (XftPatternGet (pattern, XFT_RGBA, 0, &v) == XftResultNoMatch)
    {
	int	subpixel = XFT_RGBA_NONE;
#if RENDER_MAJOR > 0 || RENDER_MINOR >= 6
	int render_order = XRenderQuerySubpixelOrder (dpy, screen);
	switch (render_order) {
	default:
	case SubPixelUnknown:		subpixel = XFT_RGBA_NONE; break;
	case SubPixelHorizontalRGB:	subpixel = XFT_RGBA_RGB; break;
	case SubPixelHorizontalBGR:	subpixel = XFT_RGBA_BGR; break;
	case SubPixelVerticalRGB:	subpixel = XFT_RGBA_VRGB; break;
	case SubPixelVerticalBGR:	subpixel = XFT_RGBA_VBGR; break;
	case SubPixelNone:		subpixel = XFT_RGBA_NONE; break;
	}
#endif
	XftPatternAddInteger (pattern, XFT_RGBA,
			      XftDefaultGetInteger (dpy, XFT_RGBA, screen, 
						    subpixel));
    }
    if (XftPatternGet (pattern, XFT_MINSPACE, 0, &v) == XftResultNoMatch)
    {
	XftPatternAddBool (pattern, XFT_MINSPACE,
			   XftDefaultGetBool (dpy, XFT_MINSPACE, screen,
					      False));
    }
    if (XftPatternGet (pattern, XFT_PIXEL_SIZE, 0, &v) == XftResultNoMatch)
    {
	int	pixels, mm;
	double	dpi;

	if (XftPatternGet (pattern, XFT_SIZE, 0, &v) != XftResultMatch)
	{
	    size = 12.0;
	    XftPatternAddDouble (pattern, XFT_SIZE, size);
	}
	else
	{
	    switch (v.type) {
	    case XftTypeInteger:
		size = (double) v.u.i;
		break;
	    case XftTypeDouble:
		size = v.u.d;
		break;
	    default:
		size = 12.0;
		break;
	    }
	}
	scale = XftDefaultGetDouble (dpy, XFT_SCALE, screen, 1.0);
	size *= scale;
	pixels = DisplayHeight (dpy, screen);
	mm = DisplayHeightMM (dpy, screen);
	dpi = (((double) DisplayHeight (dpy, screen) * 25.4) / 
	       (double) DisplayHeightMM (dpy, screen));
	dpi = XftDefaultGetDouble (dpy, XFT_DPI, screen, dpi);
	size = size * dpi / 72.0;
	XftPatternAddDouble (pattern, XFT_PIXEL_SIZE, size);
    }
}


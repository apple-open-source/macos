/*
 * $XFree86: xc/lib/Xft1/xftlist.c,v 1.4 2002/07/09 17:40:20 keithp Exp $
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

#include <stdlib.h>
#include "xftint.h"
#include <fontconfig/fcprivate.h>

XftObjectSet *
XftObjectSetCreate (void)
{
    return FcObjectSetCreate ();
}

Bool
XftObjectSetAdd (XftObjectSet *os, const char *object)
{
    return FcObjectSetAdd (os, object);
}

void
XftObjectSetDestroy (XftObjectSet *os)
{
    FcObjectSetDestroy (os);
}


XftObjectSet *
XftObjectSetVaBuild (const char *first, va_list va)
{
    XftObjectSet    *ret;

    FcObjectSetVapBuild (ret, first, va);
    return ret;
}

XftObjectSet *
XftObjectSetBuild (const char *first, ...)
{
    va_list	    va;
    XftObjectSet    *os;

    va_start (va, first);
    FcObjectSetVapBuild (os, first, va);
    va_end (va);
    return os;
}

XftFontSet *
XftListFontSets (XftFontSet	**sets,
		 int		nsets,
		 XftPattern	*p,
		 XftObjectSet	*os)
{
    return FcFontSetList (0, sets, nsets, p, os);
}

XftFontSet *
XftListFontsPatternObjects (Display	    *dpy,
			    int		    screen,
			    XftPattern	    *pattern,
			    XftObjectSet    *os)
{
    XftFontSet	*sets[2];
    int		nsets = 0;
#ifdef FREETYPE2
    Bool	core, render;
    XftResult	result;
#endif
    XftPattern	*pattern_trim;
    XftFontSet	*ret;
    
    if (!XftInit (0))
	return 0;

    pattern_trim = XftPatternDuplicate (pattern);
    if (!pattern_trim)
	return 0;
    
    XftPatternDel (pattern_trim, XFT_CORE);
    XftPatternDel (pattern_trim, XFT_RENDER);
#ifdef FREETYPE2
    render = core = False;
    result = XftPatternGetBool (pattern, XFT_CORE, 0, &core);
    if (result != XftResultMatch)
	core = XftDefaultGetBool (dpy, XFT_CORE, screen,
				  !XftDefaultHasRender (dpy));

    result = XftPatternGetBool (pattern, XFT_RENDER, 0, &render);
    if (result != XftResultMatch)
	render = XftDefaultGetBool (dpy, XFT_RENDER, screen,
				    XftDefaultHasRender (dpy));
    if (render)
    {
	/*
	 * fontconfig fonts never include encoding values.
	 * deleting it is something of a kludge as it eliminates the
	 * ability to list core fonts and render fonts of a specific
	 * encoding.  Fortunately, Xft1 apps generally don't want core
	 * fonts in any case.
	 */
	XftPatternDel (pattern_trim, XFT_ENCODING);
	if (XftInitFtLibrary())
	{
	    sets[nsets] = _XftFontSet;
	    if (sets[nsets])
		nsets++;
	}
    }
    if (core)
#endif
    {
	sets[nsets] = XftDisplayGetFontSet (dpy);
	if (sets[nsets])
	    nsets++;
    }
    ret = XftListFontSets (sets, nsets, pattern_trim, os);
    XftPatternDestroy (pattern_trim);
    return ret;
}

XftFontSet *
XftListFonts (Display	*dpy,
	      int	screen,
	      ...)
{
    va_list	    va;
    XftFontSet	    *fs;
    XftObjectSet    *os;
    XftPattern	    *pattern;
    const char	    *first;

    va_start (va, screen);
    
    FcPatternVapBuild (pattern, 0, va);
    
    first = va_arg (va, const char *);
    FcObjectSetVapBuild (os, first, va);
    
    va_end (va);
    
    fs = XftListFontsPatternObjects (dpy, screen, pattern, os);
    XftPatternDestroy (pattern);
    XftObjectSetDestroy (os);
    return fs;
}

/*
 * $XFree86: xc/lib/Xft1/xftfont.c,v 1.2 2002/03/01 01:00:53 keithp Exp $
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
#include "xftint.h"

XftPattern *
XftFontMatch (Display *dpy, int screen, XftPattern *pattern, XftResult *result)
{
    XftPattern	*new;
    XftPattern	*match;
    XftFontSet	*sets[2];
    int		nsets;
#ifdef FREETYPE2
    Bool	render, core;
#endif

    if (!XftInit (0))
	return 0;
    
    new = XftPatternDuplicate (pattern);
    if (!new)
	return 0;

    if (_XftFontDebug () & XFT_DBG_OPENV)
    {
	printf ("XftFontMatch pattern ");
	XftPatternPrint (new);
    }
    XftConfigSubstitute (new);
    if (_XftFontDebug () & XFT_DBG_OPENV)
    {
	printf ("XftFontMatch after XftConfig substitutions ");
	XftPatternPrint (new);
    }
    XftDefaultSubstitute (dpy, screen, new);
    if (_XftFontDebug () & XFT_DBG_OPENV)
    {
	printf ("XftFontMatch after X resource substitutions ");
	XftPatternPrint (new);
    }
    nsets = 0;
    
#ifdef FREETYPE2
    render = False;
    core = True;
    (void) XftPatternGetBool (new, XFT_RENDER, 0, &render);
    (void) XftPatternGetBool (new, XFT_CORE, 0, &core);
    if (_XftFontDebug () & XFT_DBG_OPENV)
    {
	printf ("XftFontMatch: use core fonts \"%s\", use render fonts \"%s\"\n",
		core ? "True" : "False", render ? "True" : "False");
    }

    if (render)
    {
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
    
    match = XftFontSetMatch (sets, nsets, new, result);
    XftPatternDestroy (new);
    return match;
}

XftFont *
XftFontOpenPattern (Display *dpy, XftPattern *pattern)
{
    Bool	    core = True;
    XFontStruct	    *xfs = 0;
    XftFont	    *font;
#ifdef FREETYPE2
    XftFontStruct   *fs = 0;

    if (XftPatternGetBool (pattern, XFT_CORE, 0, &core) != XftResultMatch)
	return 0;
    if (core)
#endif
    {
	xfs = XftCoreOpen (dpy, pattern);
	if (!xfs) return 0;
    }
#ifdef FREETYPE2
    else
    {
	fs = XftFreeTypeOpen (dpy, pattern);
	if (!fs) return 0;
    }
#endif
    font = (XftFont *) malloc (sizeof (XftFont));
    font->core = core;
    font->pattern = pattern;
#ifdef FREETYPE2
    if (core)
#endif
    {
	font->u.core.font = xfs;
	font->ascent = xfs->ascent;
	font->descent = xfs->descent;
	font->height = xfs->ascent + xfs->descent;
	font->max_advance_width = xfs->max_bounds.width;
    }
#ifdef FREETYPE2
    else
    {
	font->u.ft.font = fs;
	font->ascent = fs->ascent;
	font->descent = fs->descent;
	font->height = fs->height;
	font->max_advance_width = fs->max_advance_width;
    }
#endif
    return font;
}

int
_XftFontDebug (void)
{
    static int	initialized;
    static int	debug;

    if (!initialized)
    {
	char	*e;
	
	initialized = 1;
	e = getenv ("XFT_DEBUG");
	if (e)
	{
	    printf ("XFT_DEBUG=%s\n", e);
	    debug = atoi (e);
	    if (debug <= 0)
		debug = 1;
	}
    }
    return debug;
}

XftFont *
XftFontOpen (Display *dpy, int screen, ...)
{
    va_list	    va;
    XftPattern	    *pat;
    XftPattern	    *match;
    XftResult	    result;
    XftFont	    *font;

    va_start (va, screen);
    pat = XftPatternVaBuild (0, va);
    va_end (va);
    if (!pat)
    {
	if (_XftFontDebug () & XFT_DBG_OPEN)
	    printf ("XftFontOpen: Invalid pattern argument\n");
	return 0;
    }
    match = XftFontMatch (dpy, screen, pat, &result);
    if (_XftFontDebug () & XFT_DBG_OPEN)
    {
	printf ("Pattern ");
	XftPatternPrint (pat);
	if (match)
	{
	    printf ("Match ");
	    XftPatternPrint (match);
	}
	else
	    printf ("No Match\n");
    }
    XftPatternDestroy (pat);
    if (!match)
	return 0;
    
    font = XftFontOpenPattern (dpy, match);
    if (!font)
    {
	if (_XftFontDebug () & XFT_DBG_OPEN)
	    printf ("No Font\n");
	XftPatternDestroy (match);
    }

    return font;
}

XftFont *
XftFontOpenName (Display *dpy, int screen, const char *name)
{
    XftPattern	    *pat;
    XftPattern	    *match;
    XftResult	    result;
    XftFont   *font;

    pat = XftNameParse (name);
    if (_XftFontDebug () & XFT_DBG_OPEN)
    {
	printf ("XftFontOpenName \"%s\": ", name);
	if (pat)
	    XftPatternPrint (pat);
	else
	    printf ("Invalid name\n");
    }
			     
    if (!pat)
	return 0;
    match = XftFontMatch (dpy, screen, pat, &result);
    if (_XftFontDebug () & XFT_DBG_OPEN)
    {
	if (match)
	{
	    printf ("Match ");
	    XftPatternPrint (match);
	}
	else
	    printf ("No Match\n");
    }
    XftPatternDestroy (pat);
    if (!match)
	return 0;
    
    font = XftFontOpenPattern (dpy, match);
    if (!font)
	XftPatternDestroy (match);
    
    return font;
}

XftFont *
XftFontOpenXlfd (Display *dpy, int screen, const char *xlfd)
{
    XftPattern	    *pat;
    XftPattern	    *match;
    XftResult	    result;
    XftFont   *font;

    pat = XftXlfdParse (xlfd, False, False);
    if (_XftFontDebug () & XFT_DBG_OPEN)
    {
	printf ("XftFontOpenXlfd \"%s\": ", xlfd);
	if (pat)
	    printf ("Invalid xlfd\n");
	else
	    XftPatternPrint (pat);
    }
			     
    if (!pat)
	return 0;
    match = XftFontMatch (dpy, screen, pat, &result);
    if (_XftFontDebug () & XFT_DBG_OPEN)
    {
	if (match)
	{
	    printf ("Match ");
	    XftPatternPrint (match);
	}
	else
	    printf ("No Match\n");
    }
    XftPatternDestroy (pat);
    if (!match)
	return 0;
    
    font = XftFontOpenPattern (dpy, match);
    if (!font)
	XftPatternDestroy (match);
    
    return font;
}

void
XftFontClose (Display *dpy, XftFont *font)
{
    if (font->core)
	XftCoreClose (dpy, font->u.core.font);
#ifdef FREETYPE2
    else
	XftFreeTypeClose (dpy, font->u.ft.font);
#endif
    if (font->pattern)
	XftPatternDestroy (font->pattern);
    free (font);
}

Bool
XftGlyphExists (Display *dpy, XftFont *font, XftChar32 glyph)
{
    if (font->core)
	return XftCoreGlyphExists (dpy, font->u.core.font, glyph);
    else
#ifdef FREETYPE2
	return XftFreeTypeGlyphExists (dpy, font->u.ft.font, glyph);
#else
	return False;
#endif
}

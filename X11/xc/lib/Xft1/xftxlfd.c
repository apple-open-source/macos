/*
 * $XFree86: xc/lib/Xft1/xftxlfd.c,v 1.1.1.1 2002/02/15 01:26:16 keithp Exp $
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
#include <string.h>
#include <stdio.h>
#include "xftint.h"

static XftSymbolic XftXlfdWeights[] = {
    {	"light",    XFT_WEIGHT_LIGHT	},
    {	"medium",   XFT_WEIGHT_MEDIUM	},
    {	"regular",  XFT_WEIGHT_MEDIUM	},
    {	"demibold", XFT_WEIGHT_DEMIBOLD },
    {	"bold",	    XFT_WEIGHT_BOLD	},
    {	"black",    XFT_WEIGHT_BLACK	},
};

#define NUM_XLFD_WEIGHTS    (sizeof XftXlfdWeights/sizeof XftXlfdWeights[0])

static XftSymbolic XftXlfdSlants[] = {
    {	"r",	    XFT_SLANT_ROMAN	},
    {	"i",	    XFT_SLANT_ITALIC	},
    {	"o",	    XFT_SLANT_OBLIQUE	},
};

#define NUM_XLFD_SLANTS    (sizeof XftXlfdSlants/sizeof XftXlfdSlants[0])

XftPattern *
XftXlfdParse (const char *xlfd_orig, Bool ignore_scalable, Bool complete)
{
    XftPattern	*pat;
    const char	*xlfd = xlfd_orig;
    const char	*foundry;
    const char	*family;
    const char	*weight_name;
    const char	*slant;
    const char	*registry;
    const char	*encoding;
    char	*save;
    char	style[128];
    int		pixel;
    int		point;
    int		resx;
    int		resy;
    int		slant_value, weight_value;
    double	dpixel;

    if (*xlfd != '-')
	return 0;
    if (!(xlfd = strchr (foundry = ++xlfd, '-'))) return 0;
    if (!(xlfd = strchr (family = ++xlfd, '-'))) return 0;
    if (!(xlfd = strchr (weight_name = ++xlfd, '-'))) return 0;
    if (!(xlfd = strchr (slant = ++xlfd, '-'))) return 0;
    if (!(xlfd = strchr (/* setwidth_name = */ ++xlfd, '-'))) return 0;
    if (!(xlfd = strchr (/* add_style_name = */ ++xlfd, '-'))) return 0;
    if (!(xlfd = _XftGetInt (++xlfd, &pixel))) return 0;
    if (!(xlfd = _XftGetInt (++xlfd, &point))) return 0;
    if (!(xlfd = _XftGetInt (++xlfd, &resx))) return 0;
    if (!(xlfd = _XftGetInt (++xlfd, &resy))) return 0;
    if (!(xlfd = strchr (/* spacing = */ ++xlfd, '-'))) return 0;
    if (!(xlfd = strchr (/* average_width = */ ++xlfd, '-'))) return 0;
    if (!(xlfd = strchr (registry = ++xlfd, '-'))) return 0;
    /* make sure no fields follow this one */
    if ((xlfd = strchr (encoding = ++xlfd, '-'))) return 0;

    if (ignore_scalable && !pixel)
	return 0;
    
    pat = XftPatternCreate ();
    if (!pat)
	return 0;
    
    save = (char *) malloc (strlen (foundry) + 1);
    
    if (!save)
	return 0;

    if (!XftPatternAddString (pat, XFT_XLFD, xlfd_orig)) goto bail;
    
    _XftSplitStr (foundry, save);
    if (save[0] && strcmp (save, "*") != 0)
	if (!XftPatternAddString (pat, XFT_FOUNDRY, save)) goto bail;
    
    _XftSplitStr (family, save);
    if (save[0] && strcmp (save, "*") != 0)
	if (!XftPatternAddString (pat, XFT_FAMILY, save)) goto bail;
    
    weight_value = _XftMatchSymbolic (XftXlfdWeights, NUM_XLFD_WEIGHTS,
				      _XftSplitStr (weight_name, save),
				      XFT_WEIGHT_MEDIUM);
    if (!XftPatternAddInteger (pat, XFT_WEIGHT, weight_value)) 
	goto bail;
    
    slant_value = _XftMatchSymbolic (XftXlfdSlants, NUM_XLFD_SLANTS,
				     _XftSplitStr (slant, save),
				     XFT_SLANT_ROMAN);
    if (!XftPatternAddInteger (pat, XFT_SLANT, slant_value)) 
	goto bail;
    
    dpixel = (double) pixel;
    
    if (complete)
    {
	/*
	 * Build a style name
	 */
	style[0] = '\0';
	switch (weight_value) {
	case XFT_WEIGHT_LIGHT: strcat (style, "light"); break;
	case XFT_WEIGHT_DEMIBOLD: strcat (style, "demibold"); break;
	case XFT_WEIGHT_BOLD: strcat (style, "bold"); break;
	case XFT_WEIGHT_BLACK: strcat (style, "black"); break;
	}
	if (slant_value != XFT_SLANT_ROMAN) {
	    if (style[0])
		strcat (style, " ");
	    switch (slant_value) {
	    case XFT_SLANT_ITALIC: strcat (style, "italic"); break;
	    case XFT_SLANT_OBLIQUE: strcat (style, "oblique"); break;
	    }
	}
	if (!style[0])
	    strcat (style, "Regular");
	
	if (!XftPatternAddString (pat, XFT_STYLE, style))
	    goto bail;
	if (!XftPatternAddBool (pat, XFT_SCALABLE, pixel == 0)) goto bail;
	if (!XftPatternAddBool (pat, XFT_CORE, True)) goto bail;
	if (!XftPatternAddBool (pat, XFT_ANTIALIAS, False)) goto bail;
    }
    else
    {
	if (point > 0)
	{
	    if (!XftPatternAddDouble (pat, XFT_SIZE, ((double) point) / 10.0)) goto bail;
	    if (pixel <= 0 && resy > 0)
	    {
		dpixel = (double) point * (double) resy / 720.0;
	    }
	}
    }
    
    if (dpixel > 0)
	if (!XftPatternAddDouble (pat, XFT_PIXEL_SIZE, dpixel)) goto bail;
    
    _XftDownStr (registry, save);
    if (registry[0] && !strchr (registry, '*'))
	if (!XftPatternAddString (pat, XFT_ENCODING, save)) goto bail;

    free (save);
    return pat;
    
bail:
    free (save);
    XftPatternDestroy (pat);
    return 0;
}

Bool
XftCoreAddFonts (XftFontSet *set, Display *dpy, Bool ignore_scalable)
{
    char	**xlfds;
    int		num;
    int		i;
    XftPattern	*font;
    Bool	ret;

    xlfds = XListFonts (dpy,
			"-*-*-*-*-*-*-*-*-*-*-*-*-*-*",
			10000, &num);
    if (!xlfds)
	return False;
    ret = True;
    for (i = 0; ret && i < num; i++)
    {
	font = XftXlfdParse (xlfds[i], ignore_scalable, True);
	if (font)
	{
	    if (!XftFontSetAdd (set, font))
	    {
		XftPatternDestroy (font);
		ret = False;
	    }
	}
    }
    XFreeFontNames (xlfds);
    return ret;
}

typedef struct _XftCoreFont {
    struct _XftCoreFont	*next;
    int			ref;

    XFontStruct		*font;
    Display		*display;
    char		*xlfd;
} XftCoreFont;

static XftCoreFont *_XftCoreFonts;

XFontStruct*
XftCoreOpen (Display *dpy, XftPattern *pattern)
{
    XftCoreFont	*cf;
    char	*xlfd;
    char	*xlfd_pixel = 0;
    char	*i, *o;
    int		d;
    Bool	scalable;
    double	pixel_size;
    int		pixel_int;
    XFontStruct	*ret;

#if 0
    printf ("Core ");
    XftPatternPrint (pattern);
#endif
    if (XftPatternGetString (pattern, XFT_XLFD, 0, &xlfd) != XftResultMatch)
	return 0;
    if (XftPatternGetBool (pattern, XFT_SCALABLE, 0, &scalable) != XftResultMatch)
	return 0;
    if (scalable)
    {
	if (XftPatternGetDouble (pattern, XFT_PIXEL_SIZE, 0, &pixel_size) != XftResultMatch)
	    return 0;
	pixel_int = (int) (pixel_size + 0.5);
	if (pixel_int)
	{
	    xlfd_pixel = (char *) malloc (strlen (xlfd) + 32);
	    i = xlfd;
	    o = xlfd_pixel;
	    d = 0;
	    while (d != 7 && *i)
	    {
		if ((*o++ = *i++) == '-')
		    d++;
	    }
	    if (*i)
	    {
		sprintf (o, "%d", pixel_int);
		o += strlen (o);
		while (*i != '-')
		    ++i;
	    }
	    while ((*o++ = *i++));
#if 0
	    printf ("original %s sized %s\n", xlfd, xlfd_pixel);
#endif
	    xlfd = xlfd_pixel;
	}
    }
    for (cf = _XftCoreFonts; cf; cf = cf->next)
    {
	if (cf->display == dpy &&
	    !_XftStrCmpIgnoreCase (cf->xlfd, xlfd))
	{
	    cf->ref++;
	    if (_XftFontDebug () & XFT_DBG_REF)
	    {
		printf ("Xlfd \"%s\" matches existing font (%d)\n",
			xlfd, cf->ref);
	    }
	    break;
	}
    }
    if (!cf)
    {
	ret = XLoadQueryFont (dpy, xlfd);
	if (!ret)
	    return 0;

	cf = (XftCoreFont *) malloc (sizeof (XftCoreFont) +
				     strlen (xlfd) + 1);
	if (!cf)
	{
	    XFreeFont (dpy, ret);
	    return 0;
	}
	
        if (_XftFontDebug () & XFT_DBG_REF)
	    printf ("Xlfd \"%s\" matches new font\n", xlfd);
	
	cf->next = _XftCoreFonts;
	_XftCoreFonts = cf;
	cf->ref = 1;
	
	cf->font = ret;
	cf->xlfd = (char *) (cf + 1);
	strcpy (cf->xlfd, xlfd);
    }
    if (xlfd_pixel)
	free (xlfd_pixel);
    return cf->font;
}

void
XftCoreClose (Display *dpy, XFontStruct *font)
{
    XftCoreFont	*cf, **prev;

    for (prev = &_XftCoreFonts; (cf = *prev); prev = &cf->next)
    {
	if (cf->display == dpy && cf->font == font)
	{
	    if (--cf->ref == 0)
	    {
		XFreeFont (dpy, cf->font);
		*prev = cf->next;
		free (cf);
	    }
	    break;
	}
    }
}

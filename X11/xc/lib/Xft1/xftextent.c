/*
 * $XFree86: xc/lib/Xft1/xftextent.c,v 1.1.1.1 2002/02/15 01:26:15 keithp Exp $
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
#include "xftint.h"

void
XftTextExtents8 (Display	*dpy,
		 XftFont	*font,
		 XftChar8	*string, 
		 int		len,
		 XGlyphInfo	*extents)
{
    if (font->core)
    {
	XftCoreExtents8 (dpy, font->u.core.font, string, len, extents);
    }
#ifdef FREETYPE2
    else
    {
	XftRenderExtents8 (dpy, font->u.ft.font, string, len, extents);
    }
#endif
}

void
XftTextExtents16 (Display	    *dpy,
		  XftFont	    *font,
		  XftChar16	    *string, 
		  int		    len,
		  XGlyphInfo	    *extents)
{
    if (font->core)
    {
	XftCoreExtents16 (dpy, font->u.core.font, string, len, extents);
    }
#ifdef FREETYPE2
    else
    {
	XftRenderExtents16 (dpy, font->u.ft.font, string, len, extents);
    }
#endif
}

void
XftTextExtents32 (Display	*dpy,
		  XftFont	*font,
		  XftChar32	*string, 
		  int		len,
		  XGlyphInfo	*extents)
{
    if (font->core)
    {
	XftCoreExtents32 (dpy, font->u.core.font, string, len, extents);
    }
#ifdef FREETYPE2
    else
    {
	XftRenderExtents32 (dpy, font->u.ft.font, string, len, extents);
    }
#endif
}

void
XftTextExtentsUtf8 (Display	*dpy,
		    XftFont	*font,
		    XftChar8	*string, 
		    int		len,
		    XGlyphInfo	*extents)
{
    XftChar8	*src;
    XftChar32	c;
    XftChar32	lbuf[4096];
    XftChar32	*dst;
    XftChar8	*dst8;
    XftChar16	*dst16;
    XftChar32	*dst32;
    int		rlen, clen;
    int		width = 1;
    int		n;

    /* compute needed width */
    src = string;
    rlen = len;
    n = 0;
    while (rlen)
    {
	clen = XftUtf8ToUcs4 (src, &c, rlen);
	if (clen <= 0)	/* malformed UTF8 string */
	{
	    memset (extents, 0, sizeof (XGlyphInfo));
	    return;
	}
	if (c >= 0x10000)
	    width = 4;
	else if (c >= 0x100)
	{
	    if (width == 1)
		width = 2;
	}
	src += clen;
	rlen -= clen;
	n++;
    }
    dst = lbuf;
    if (n * width > sizeof (lbuf))
    {
	dst = (XftChar32 *) malloc (n * width);
	if (!dst)
	{
	    memset (extents, 0, sizeof (XGlyphInfo));
	    return;
	}
    }
    
    switch (width) {
    case 4:
	src = string;
	rlen = len;
	dst32 = dst;
	while (rlen)
	{
	    clen = XftUtf8ToUcs4 (src, &c, rlen);
	    if (clen <= 0)	/* malformed UTF8 string */
		return;
	    *dst32++ = c;
	    src += clen;
	    rlen -= clen;
	}
	dst32 = dst;
	XftTextExtents32 (dpy, font, dst32, n, extents);
	break;
    case 2:
	src = string;
	rlen = len;
	dst16 = (XftChar16 *) dst;
	while (rlen)
	{
	    clen = XftUtf8ToUcs4 (src, &c, rlen);
	    if (clen <= 0)	/* malformed UTF8 string */
		return;
	    *dst16++ = c;
	    src += clen;
	    rlen -= clen;
	}
	dst16 = (XftChar16 *) dst;
	XftTextExtents16 (dpy, font, dst16, n, extents);
	break;
    case 1:
	src = string;
	rlen = len;
	dst8 = (XftChar8 *) dst;
	while (rlen)
	{
	    clen = XftUtf8ToUcs4 (src, &c, rlen);
	    if (clen <= 0)	/* malformed UTF8 string */
		return;
	    *dst8++ = c;
	    src += clen;
	    rlen -= clen;
	}
	dst8 = (XftChar8 *) dst;
	XftTextExtents8 (dpy, font, dst8, n, extents);
	break;
    }
    if (dst != lbuf)
	free (dst);
}

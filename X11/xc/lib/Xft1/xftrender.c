/*
 * $XFree86: xc/lib/Xft1/xftrender.c,v 1.1.1.1 2002/02/15 01:26:15 keithp Exp $
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

void
XftRenderString8 (Display *dpy, Picture src, 
		  XftFontStruct *font, Picture dst,
		  int srcx, int srcy,
		  int x, int y,
		  XftChar8 *string, int len)
{
    XftChar32	    missing[XFT_NMISSING];
    int		    nmissing;
    XftChar8	    *s;
    int		    l;

    s = string;
    l = len;
    nmissing = 0;
    while (l--)
	XftGlyphCheck (dpy, font, (XftChar32) *s++, missing, &nmissing);
    if (nmissing)
	XftGlyphLoad (dpy, font, missing, nmissing);
    XRenderCompositeString8 (dpy, PictOpOver, src, dst,
			     font->format, font->glyphset,
			     srcx, srcy, x, y, (char *) string, len);
}

void
XftRenderString16 (Display *dpy, Picture src, 
		   XftFontStruct *font, Picture dst,
		   int srcx, int srcy,
		   int x, int y,
		   XftChar16 *string, int len)
{
    XftChar32	    missing[XFT_NMISSING];
    int		    nmissing;
    XftChar16	    *s;
    int		    l;

    s = string;
    l = len;
    nmissing = 0;
    while (l--)
	XftGlyphCheck (dpy, font, (XftChar32) *s++, missing, &nmissing);
    if (nmissing)
	XftGlyphLoad (dpy, font, missing, nmissing);
    XRenderCompositeString16 (dpy, PictOpOver, src, dst,
			      font->format, font->glyphset,
			      srcx, srcy, x, y, string, len);
}

void
XftRenderString32 (Display *dpy, Picture src, 
		   XftFontStruct *font, Picture dst,
		   int srcx, int srcy,
		   int x, int y,
		   XftChar32 *string, int len)
{
    XftChar32	    missing[XFT_NMISSING];
    int		    nmissing;
    XftChar32	    *s;
    int		    l;

    s = string;
    l = len;
    nmissing = 0;
    while (l--)
	XftGlyphCheck (dpy, font, (XftChar32) *s++, missing, &nmissing);
    if (nmissing)
	XftGlyphLoad (dpy, font, missing, nmissing);
    XRenderCompositeString32 (dpy, PictOpOver, src, dst,
			      font->format, font->glyphset,
			      srcx, srcy, x, y, string, len);
}

void
XftRenderStringUtf8 (Display *dpy, Picture src, 
		     XftFontStruct *font, Picture dst,
		     int srcx, int srcy,
		     int x, int y,
		     XftChar8 *string, int len)
{
    XftChar8	*s;
    XftChar32	c;
    XftChar32	lbuf[4096];
    XftChar32	*d;
    XftChar8	*dst8;
    XftChar16	*dst16;
    XftChar32	*dst32;
    int		rlen, clen;
    int		width = 1;
    int		n;

    /* compute needed width */
    if (!XftUtf8Len (string, len, &n, &width))
	return;
    
    d = lbuf;
    if (n * width > sizeof (lbuf))
    {
	d = (XftChar32 *) malloc (n * width);
	if (!d)
	    return;
    }
    
    switch (width) {
    case 4:
	s = string;
	rlen = len;
	dst32 = d;
	while (rlen)
	{
	    clen = XftUtf8ToUcs4 (s, &c, rlen);
	    if (clen <= 0)	/* malformed UTF8 string */
		return;
	    *dst32++ = c;
	    s += clen;
	    rlen -= clen;
	}
	dst32 = d;
	XftRenderString32 (dpy, src, font, dst, srcx, srcy, x, y,
			 dst32, n);
	break;
    case 2:
	s = string;
	rlen = len;
	dst16 = (XftChar16 *) d;
	while (rlen)
	{
	    clen = XftUtf8ToUcs4 (s, &c, rlen);
	    if (clen <= 0)	/* malformed UTF8 string */
		return;
	    *dst16++ = c;
	    s += clen;
	    rlen -= clen;
	}
	dst16 = (XftChar16 *) d;
	XftRenderString16 (dpy, src, font, dst, srcx, srcy, x, y,
			   dst16, n);
	break;
    case 1:
	s = string;
	rlen = len;
	dst8 = (XftChar8 *) d;
	while (rlen)
	{
	    clen = XftUtf8ToUcs4 (s, &c, rlen);
	    if (clen <= 0)	/* malformed UTF8 string */
		return;
	    *dst8++ = c;
	    s += clen;
	    rlen -= clen;
	}
	dst8 = (XftChar8 *) d;
	XftRenderString8 (dpy, src, font, dst, srcx, srcy, x, y,
			  dst8, n);
	break;
    }
    if (d != lbuf)
	free (d);
}
   
void
XftRenderExtents8 (Display	    *dpy,
		   XftFontStruct    *font,
		   XftChar8    *string, 
		   int		    len,
		   XGlyphInfo	    *extents)
{
    XftChar32	    missing[XFT_NMISSING];
    int		    nmissing;
    XftChar8	    *s, c;
    int		    l;
    XGlyphInfo	    *gi;
    int		    x, y;
    int		    left, right, top, bottom;
    int		    overall_left, overall_right;
    int		    overall_top, overall_bottom;

    s = string;
    l = len;
    nmissing = 0;
    while (l--)
	XftGlyphCheck (dpy, font, (XftChar32) *s++, missing, &nmissing);
    if (nmissing)
	XftGlyphLoad (dpy, font, missing, nmissing);
    
    gi = 0;
    while (len)
    {
	c = *string++;
	len--;
	gi = c < font->nrealized ? font->realized[c] : 0;
	if (gi)
	    break;
    }
    if (len == 0 && !gi)
    {
	extents->width = 0;
	extents->height = 0;
	extents->x = 0;
	extents->y = 0;
	extents->yOff = 0;
	extents->xOff = 0;
	return;
    }
    x = 0;
    y = 0;
    overall_left = x - gi->x;
    overall_top = y - gi->y;
    overall_right = overall_left + (int) gi->width;
    overall_bottom = overall_top + (int) gi->height;
    x += gi->xOff;
    y += gi->yOff;
    while (len--)
    {
	c = *string++;
	gi = c < font->nrealized ? font->realized[c] : 0;
	if (!gi)
	    continue;
	left = x - gi->x;
	top = y - gi->y;
	right = left + (int) gi->width;
	bottom = top + (int) gi->height;
	if (left < overall_left)
	    overall_left = left;
	if (top < overall_top)
	    overall_top = top;
	if (right > overall_right)
	    overall_right = right;
	if (bottom > overall_bottom)
	    overall_bottom = bottom;
	x += gi->xOff;
	y += gi->yOff;
    }
    extents->x = -overall_left;
    extents->y = -overall_top;
    extents->width = overall_right - overall_left;
    extents->height = overall_bottom - overall_top;
    extents->xOff = x;
    extents->yOff = y;
}

void
XftRenderExtents16 (Display	    *dpy,
		    XftFontStruct   *font,
		    XftChar16	    *string,
		    int		    len,
		    XGlyphInfo	    *extents)
{
    XftChar32	    missing[XFT_NMISSING];
    int		    nmissing;
    XftChar16	    *s, c;
    int		    l;
    XGlyphInfo	    *gi;
    int		    x, y;
    int		    left, right, top, bottom;
    int		    overall_left, overall_right;
    int		    overall_top, overall_bottom;

    s = string;
    l = len;
    nmissing = 0;
    while (l--)
	XftGlyphCheck (dpy, font, (XftChar32) *s++, missing, &nmissing);
    if (nmissing)
	XftGlyphLoad (dpy, font, missing, nmissing);
    
    gi = 0;
    while (len)
    {
	c = *string++;
	len--;
	gi = c < font->nrealized ? font->realized[c] : 0;
	if (gi)
	    break;
    }
    if (len == 0 && !gi)
    {
	extents->width = 0;
	extents->height = 0;
	extents->x = 0;
	extents->y = 0;
	extents->yOff = 0;
	extents->xOff = 0;
	return;
    }
    x = 0;
    y = 0;
    overall_left = x - gi->x;
    overall_top = y - gi->y;
    overall_right = overall_left + (int) gi->width;
    overall_bottom = overall_top + (int) gi->height;
    x += gi->xOff;
    y += gi->yOff;
    while (len--)
    {
	c = *string++;
	gi = c < font->nrealized ? font->realized[c] : 0;
	if (!gi)
	    continue;
	left = x - gi->x;
	top = y - gi->y;
	right = left + (int) gi->width;
	bottom = top + (int) gi->height;
	if (left < overall_left)
	    overall_left = left;
	if (top < overall_top)
	    overall_top = top;
	if (right > overall_right)
	    overall_right = right;
	if (bottom > overall_bottom)
	    overall_bottom = bottom;
	x += gi->xOff;
	y += gi->yOff;
    }
    extents->x = -overall_left;
    extents->y = -overall_top;
    extents->width = overall_right - overall_left;
    extents->height = overall_bottom - overall_top;
    extents->xOff = x;
    extents->yOff = y;
}

void
XftRenderExtents32 (Display	    *dpy,
		    XftFontStruct   *font,
		    XftChar32	    *string,
		    int		    len,
		    XGlyphInfo	    *extents)
{
    XftChar32	    missing[XFT_NMISSING];
    int		    nmissing;
    XftChar32	    *s, c;
    int		    l;
    XGlyphInfo	    *gi;
    int		    x, y;
    int		    left, right, top, bottom;
    int		    overall_left, overall_right;
    int		    overall_top, overall_bottom;

    s = string;
    l = len;
    nmissing = 0;
    while (l--)
	XftGlyphCheck (dpy, font, (XftChar32) *s++, missing, &nmissing);
    if (nmissing)
	XftGlyphLoad (dpy, font, missing, nmissing);
    
    gi = 0;
    while (len)
    {
	c = *string++;
	len--;
	gi = c < font->nrealized ? font->realized[c] : 0;
	if (gi)
	    break;
    }
    if (len == 0 && !gi)
    {
	extents->width = 0;
	extents->height = 0;
	extents->x = 0;
	extents->y = 0;
	extents->yOff = 0;
	extents->xOff = 0;
	return;
    }
    x = 0;
    y = 0;
    overall_left = x - gi->x;
    overall_top = y - gi->y;
    overall_right = overall_left + (int) gi->width;
    overall_bottom = overall_top + (int) gi->height;
    x += gi->xOff;
    y += gi->yOff;
    while (len--)
    {
	c = *string++;
	gi = c < font->nrealized ? font->realized[c] : 0;
	if (!gi)
	    continue;
	left = x - gi->x;
	top = y - gi->y;
	right = left + (int) gi->width;
	bottom = top + (int) gi->height;
	if (left < overall_left)
	    overall_left = left;
	if (top < overall_top)
	    overall_top = top;
	if (right > overall_right)
	    overall_right = right;
	if (bottom > overall_bottom)
	    overall_bottom = bottom;
	x += gi->xOff;
	y += gi->yOff;
    }
    extents->x = -overall_left;
    extents->y = -overall_top;
    extents->width = overall_right - overall_left;
    extents->height = overall_bottom - overall_top;
    extents->xOff = x;
    extents->yOff = y;
}

void
XftRenderExtentsUtf8 (Display	    *dpy,
		      XftFontStruct *font,
		      XftChar8	    *string, 
		      int	    len,
		      XGlyphInfo    *extents)
{
    XftChar32	    missing[XFT_NMISSING];
    int		    nmissing;
    XftChar8	    *s;
    XftChar32	    c;
    int		    l, clen;
    XGlyphInfo	    *gi;
    int		    x, y;
    int		    left, right, top, bottom;
    int		    overall_left, overall_right;
    int		    overall_top, overall_bottom;

    s = string;
    l = len;
    nmissing = 0;
    while (l)
    {
	clen = XftUtf8ToUcs4 (s, &c, l);
	if (clen < 0)
	    break;
	XftGlyphCheck (dpy, font, (XftChar32) c, missing, &nmissing);
	s += clen;
	l -= clen;
    }
    if (nmissing)
	XftGlyphLoad (dpy, font, missing, nmissing);
    
    gi = 0;
    while (len)
    {
	clen = XftUtf8ToUcs4 (string, &c, len);
	if (clen < 0)
	{
	    len = 0;
	    break;
	}
	len -= clen;
	string += clen;
	gi = c < font->nrealized ? font->realized[c] : 0;
	if (gi)
	    break;
    }
    if (len == 0 && !gi)
    {
	extents->width = 0;
	extents->height = 0;
	extents->x = 0;
	extents->y = 0;
	extents->yOff = 0;
	extents->xOff = 0;
	return;
    }
    x = 0;
    y = 0;
    overall_left = x - gi->x;
    overall_top = y - gi->y;
    overall_right = overall_left + (int) gi->width;
    overall_bottom = overall_top + (int) gi->height;
    x += gi->xOff;
    y += gi->yOff;
    while (len)
    {
	clen = XftUtf8ToUcs4 (string, &c, len);
	if (clen < 0)
	    break;
	len -= clen;
	string += clen;
	gi = c < font->nrealized ? font->realized[c] : 0;
	if (!gi)
	    continue;
	left = x - gi->x;
	top = y - gi->y;
	right = left + (int) gi->width;
	bottom = top + (int) gi->height;
	if (left < overall_left)
	    overall_left = left;
	if (top < overall_top)
	    overall_top = top;
	if (right > overall_right)
	    overall_right = right;
	if (bottom > overall_bottom)
	    overall_bottom = bottom;
	x += gi->xOff;
	y += gi->yOff;
    }
    extents->x = -overall_left;
    extents->y = -overall_top;
    extents->width = overall_right - overall_left;
    extents->height = overall_bottom - overall_top;
    extents->xOff = x;
    extents->yOff = y;
}

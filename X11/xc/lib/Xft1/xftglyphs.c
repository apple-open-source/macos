/*
 * $XFree86: xc/lib/Xft1/xftglyphs.c,v 1.2 2002/03/02 22:09:05 keithp Exp $
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
#include <freetype/ftoutln.h>

static const int    filters[3][3] = {
    /* red */
#if 0
{    65538*4/7,65538*2/7,65538*1/7 },
    /* green */
{    65536*1/4, 65536*2/4, 65537*1/4 },
    /* blue */
{    65538*1/7,65538*2/7,65538*4/7 },
#endif
{    65538*9/13,65538*3/13,65538*1/13 },
    /* green */
{    65538*1/6, 65538*4/6, 65538*1/6 },
    /* blue */
{    65538*1/13,65538*3/13,65538*9/13 },
};

void
XftGlyphLoad (Display		*dpy,
	      XftFontStruct	*font,
	      XftChar32		*glyphs,
	      int		nglyph)
{
    FT_Error	    error;
    FT_ULong	    charcode;
    FT_UInt	    glyphindex;
    FT_GlyphSlot    glyph;
    XGlyphInfo	    *gi;
    Glyph	    g;
    unsigned char   bufLocal[4096];
    unsigned char   *bufBitmap = bufLocal;
    unsigned char   *b;
    int		    bufSize = sizeof (bufLocal);
    int		    size, pitch;
    unsigned char   bufLocalRgba[4096];
    unsigned char   *bufBitmapRgba = bufLocalRgba;
    int		    bufSizeRgba = sizeof (bufLocalRgba);
    int		    sizergba, pitchrgba, widthrgba;
    int		    width;
    int		    height;
    int		    i;
    int		    left, right, top, bottom;
    int		    hmul = 1;
    int		    vmul = 1;
    FT_Bitmap	    ftbit;
    FT_Matrix	    matrix;
    FT_Vector	    vector;
    Bool	    subpixel = False;

    if (!XftFreeTypeSetFace (font->face, font->size, font->charmap, &font->matrix))
	return ;

    matrix.xx = matrix.yy = 0x10000L;
    matrix.xy = matrix.yx = 0;

    if (font->antialias)
    {
	switch (font->rgba) {
	case XFT_RGBA_RGB:
	case XFT_RGBA_BGR:
	    matrix.xx *= 3;
	    subpixel = True;
	    hmul = 3;
	    break;
	case XFT_RGBA_VRGB:
	case XFT_RGBA_VBGR:
	    matrix.yy *= 3;
	    vmul = 3;
	    subpixel = True;
	    break;
	}
    }

    while (nglyph--)
    {
	charcode = (FT_ULong) *glyphs++;
	gi = font->realized[charcode];
	if (!gi)
	    continue;
	
	if (font->charmap != -1)
	{
	    glyphindex = FT_Get_Char_Index (font->face, charcode);
#if 0	    
	    if (!glyphindex)
	    {
		if (_XftFontDebug() & XFT_DBG_GLYPH)
		    printf ("glyph (%c) %d missing\n",
			    (int) charcode, (int) charcode);
		continue;
	    }
#endif
	}
	else
	    glyphindex = (FT_UInt) charcode;
	error = FT_Load_Glyph (font->face, glyphindex, FT_LOAD_NO_BITMAP);
	if (error)
	    continue;

#define FLOOR(x)    ((x) & -64)
#define CEIL(x)	    (((x)+63) & -64)
#define TRUNC(x)    ((x) >> 6)
#define ROUND(x)    (((x)+32) & -64)
		
	glyph = font->face->glyph;

	if(font->transform) 
	{
	    /*
	     * calculate the true width by transforming all four corners.
	     */
	    int xc, yc;
	    left = right = top = bottom = 0;
	    for(xc = 0; xc <= 1; xc ++) {
		for(yc = 0; yc <= 1; yc++) {
		    vector.x = glyph->metrics.horiBearingX + xc * glyph->metrics.width;
		    vector.y = glyph->metrics.horiBearingY - yc * glyph->metrics.height;
		    FT_Vector_Transform(&vector, &font->matrix);   
		    if (_XftFontDebug() & XFT_DBG_GLYPH)
			printf("Trans %d %d: %d %d\n", (int) xc, (int) yc, 
			       (int) vector.x, (int) vector.y);
		    if(xc == 0 && yc == 0) {
			left = right = vector.x;
			top = bottom = vector.y;
		    } else {
			if(left > vector.x) left = vector.x;
			if(right < vector.x) right = vector.x;
			if(bottom > vector.y) bottom = vector.y;
			if(top < vector.y) top = vector.y;
		    }

		}
	    }
	    left = FLOOR(left);
	    right = CEIL(right);
	    bottom = FLOOR(bottom);
	    top = CEIL(top);

	} else {
	    left  = FLOOR( glyph->metrics.horiBearingX );
	    right = CEIL( glyph->metrics.horiBearingX + glyph->metrics.width );

	    top    = CEIL( glyph->metrics.horiBearingY );
	    bottom = FLOOR( glyph->metrics.horiBearingY - glyph->metrics.height );
	}

	width = TRUNC(right - left);
	height = TRUNC( top - bottom );


	/*
	 * Try to keep monospace fonts ink-inside
	 * XXX transformed?
	 */
	if (font->spacing != XFT_PROPORTIONAL && !font->transform)
	{
	    if (TRUNC(right) > font->max_advance_width)
	    {
		int adjust;

		adjust = right - (font->max_advance_width << 6);
		if (adjust > left)
		    adjust = left;
		left -= adjust;
		right -= adjust;
		width = font->max_advance_width;
	    }
	}

	if ( glyph->format == ft_glyph_format_outline )
	{
	    if (font->antialias)
		pitch = (width * hmul + 3) & ~3;
	    else
		pitch = ((width + 31) & ~31) >> 3;
	    
	    size = pitch * height * vmul;
	    
	    if (size > bufSize)
	    {
		if (bufBitmap != bufLocal)
		    free (bufBitmap);
		bufBitmap = (unsigned char *) malloc (size);
		if (!bufBitmap)
		    continue;
		bufSize = size;
	    }
	    memset (bufBitmap, 0, size);

	    ftbit.width      = width * hmul;
	    ftbit.rows       = height * vmul;
	    ftbit.pitch      = pitch;
	    if (font->antialias)
		ftbit.pixel_mode = ft_pixel_mode_grays;
	    else
		ftbit.pixel_mode = ft_pixel_mode_mono;
	    
	    ftbit.buffer     = bufBitmap;
	    
	    if (subpixel)
		FT_Outline_Transform (&glyph->outline, &matrix);

	    FT_Outline_Translate ( &glyph->outline, -left*hmul, -bottom*vmul );

	    FT_Outline_Get_Bitmap( _XftFTlibrary, &glyph->outline, &ftbit );
	    i = size;
	    b = (unsigned char *) bufBitmap;
	    /*
	     * swap bit order around
	     */
	    if (!font->antialias)
	    {
		if (BitmapBitOrder (dpy) != MSBFirst)
		{
		    unsigned char   *line;
		    unsigned char   c;
		    int		    i;

		    line = (unsigned char *) bufBitmap;
		    i = size;
		    while (i--)
		    {
			c = *line;
			c = ((c << 1) & 0xaa) | ((c >> 1) & 0x55);
			c = ((c << 2) & 0xcc) | ((c >> 2) & 0x33);
			c = ((c << 4) & 0xf0) | ((c >> 4) & 0x0f);
			*line++ = c;
		    }
		}
	    }
	    if (_XftFontDebug() & XFT_DBG_GLYPH)
	    {
		printf ("char 0x%x (%c):\n", (int) charcode, (char) charcode);
		printf (" xywh (%d %d %d %d), trans (%d %d %d %d) wh (%d %d)\n",
			    (int) glyph->metrics.horiBearingX,
			    (int) glyph->metrics.horiBearingY,
			    (int) glyph->metrics.width,
			    (int) glyph->metrics.height,
			    left, right, top, bottom,
			    width, height);
		if (_XftFontDebug() & XFT_DBG_GLYPHV)
		{
		    int		x, y;
		    unsigned char	*line;

		    line = bufBitmap;
		    for (y = 0; y < height * vmul; y++)
		    {
			if (font->antialias) 
			{
			    static char    den[] = { " .:;=+*#" };
			    for (x = 0; x < pitch; x++)
				printf ("%c", den[line[x] >> 5]);
			}
			else
			{
			    for (x = 0; x < pitch * 8; x++)
			    {
				printf ("%c", line[x>>3] & (1 << (x & 7)) ? '#' : ' ');
			    }
			}
			printf ("|\n");
			line += pitch;
		    }
		    printf ("\n");
		}
	    }
	}
	else
	{
	    if (_XftFontDebug() & XFT_DBG_GLYPH)
		printf ("glyph (%c) %d no outline\n",
			(int) charcode, (int) charcode);
	    continue;
	}
	
	gi->width = width;
	gi->height = height;
	gi->x = -TRUNC(left);
	gi->y = TRUNC(top);
	if (font->spacing != XFT_PROPORTIONAL)
	{
	    if (font->transform)
	    {
		vector.x = font->max_advance_width;
		vector.y = 0;
		FT_Vector_Transform (&vector, &font->matrix);
		gi->xOff = vector.x;
		gi->yOff = -vector.y;
	    }
	    else
	    {
		gi->xOff = font->max_advance_width;
		gi->yOff = 0;
	    }
	}
	else
	{
	    gi->xOff = TRUNC(ROUND(glyph->advance.x));
	    gi->yOff = -TRUNC(ROUND(glyph->advance.y));
	}
	g = charcode;

	if (subpixel)
	{
	    int		    x, y;
	    unsigned char   *in_line, *out_line, *in;
	    unsigned int    *out;
	    unsigned int    red, green, blue;
	    int		    rf, gf, bf;
	    int		    s;
	    int		    o, os;
	    
	    widthrgba = width;
	    pitchrgba = (widthrgba * 4 + 3) & ~3;
	    sizergba = pitchrgba * height;

	    os = 1;
	    switch (font->rgba) {
	    case XFT_RGBA_VRGB:
		os = pitch;
	    case XFT_RGBA_RGB:
	    default:
		rf = 0;
		gf = 1;
		bf = 2;
		break;
	    case XFT_RGBA_VBGR:
		os = pitch;
	    case XFT_RGBA_BGR:
		bf = 0;
		gf = 1;
		rf = 2;
		break;
	    }
	    if (sizergba > bufSizeRgba)
	    {
		if (bufBitmapRgba != bufLocalRgba)
		    free (bufBitmapRgba);
		bufBitmapRgba = (unsigned char *) malloc (sizergba);
		if (!bufBitmapRgba)
		    continue;
		bufSizeRgba = sizergba;
	    }
	    memset (bufBitmapRgba, 0, sizergba);
	    in_line = bufBitmap;
	    out_line = bufBitmapRgba;
	    for (y = 0; y < height; y++)
	    {
		in = in_line;
		out = (unsigned int *) out_line;
		in_line += pitch * vmul;
		out_line += pitchrgba;
		for (x = 0; x < width * hmul; x += hmul)
		{
		    red = green = blue = 0;
		    o = 0;
		    for (s = 0; s < 3; s++)
		    {
			red += filters[rf][s]*in[x+o];
			green += filters[gf][s]*in[x+o];
			blue += filters[bf][s]*in[x+o];
			o += os;
		    }
		    red = red / 65536;
		    green = green / 65536;
		    blue = blue / 65536;
		    *out++ = (green << 24) | (red << 16) | (green << 8) | blue;
		}
	    }
	    
	    XRenderAddGlyphs (dpy, font->glyphset, &g, gi, 1, 
			      (char *) bufBitmapRgba, sizergba);
	}
	else
	{
	    XRenderAddGlyphs (dpy, font->glyphset, &g, gi, 1, 
			      (char *) bufBitmap, size);
	}
    }
    if (bufBitmap != bufLocal)
	free (bufBitmap);
    if (bufBitmapRgba != bufLocalRgba)
	free (bufBitmapRgba);
}

#define STEP	    256

/*
 * Return whether the given glyph generates any image on the screen,
 * this means it exists or a default glyph exists
 */
static Bool
XftGlyphDrawable (Display	*dpy,
		  XftFontStruct	*font,
		  XftChar32	glyph)
{
    if (font->charmap != -1)
    {
	FT_Set_Charmap (font->face, font->face->charmaps[font->charmap]);
	glyph = (XftChar32) FT_Get_Char_Index (font->face, (FT_ULong) glyph);
    }
    return glyph <= font->face->num_glyphs;
}
	    
void
XftGlyphCheck (Display		*dpy,
	       XftFontStruct	*font,
	       XftChar32	glyph,
	       XftChar32	*missing,
	       int		*nmissing)
{
    XGlyphInfo	    **realized;
    int		    nrealized;
    int		    n;
    
    if (glyph >= font->nrealized)
    {
	nrealized = glyph + STEP;
	
	if (font->realized)
	    realized = (XGlyphInfo **) realloc ((void *) font->realized,
						nrealized * sizeof (XGlyphInfo *));
	else
	    realized = (XGlyphInfo **) malloc (nrealized * sizeof (XGlyphInfo *));
	if (!realized)
	    return;
	for (n = font->nrealized; n < nrealized; n++)
	    realized[n] = XftUntestedGlyph;
	
	font->realized = realized;
	font->nrealized = nrealized;
    }
    if (font->realized[glyph] == XftUntestedGlyph)
    {
	if (XftGlyphDrawable (dpy, font, glyph))
	{
	    font->realized[glyph] = (XGlyphInfo *) malloc (sizeof (XGlyphInfo));
	    n = *nmissing;
	    missing[n++] = glyph;
	    if (n == XFT_NMISSING)
	    {
		XftGlyphLoad (dpy, font, missing, n);
		n = 0;
	    }
	    *nmissing = n;
	}
	else
	    font->realized[glyph] = 0;
    }
}

Bool
XftFreeTypeGlyphExists (Display		*dpy,
			XftFontStruct	*font,
			XftChar32	glyph)
{
    if (font->charmap != -1)
    {
	FT_Set_Charmap (font->face, font->face->charmaps[font->charmap]);
	glyph = (XftChar32) FT_Get_Char_Index (font->face, (FT_ULong) glyph);
    }
    return glyph && glyph <= font->face->num_glyphs;
}

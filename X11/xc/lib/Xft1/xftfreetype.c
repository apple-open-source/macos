/*
 * $XFree86: xc/lib/Xft1/xftfreetype.c,v 1.6 2002/11/24 01:49:41 keithp Exp $
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
#include <stdio.h>
#include <string.h>
#include "xftint.h"

FT_Library  _XftFTlibrary;

typedef struct _XftFtEncoding {
    const char	*name;
    FT_Encoding	encoding;
} XftFtEncoding;

static XftFtEncoding xftFtEncoding[] = {
    { "iso10646-1",	    ft_encoding_unicode, },
    { "iso8859-1",	    ft_encoding_unicode, },
    { "apple-roman",	    ft_encoding_apple_roman },
    { "adobe-fontspecific", ft_encoding_symbol,  },
    { "glyphs-fontspecific",ft_encoding_none,	 },
};

#define NUM_FT_ENCODINGS    (sizeof xftFtEncoding / sizeof xftFtEncoding[0])

#define FT_Matrix_Equal(a,b)	((a)->xx == (b)->xx && \
				 (a)->yy == (b)->yy && \
				 (a)->xy == (b)->xy && \
				 (a)->yx == (b)->yx)

XftPattern *
XftFreeTypeQuery (const char *file, int id, int *count)
{
    FT_Face	face;
    XftPattern	*pat;
    int		slant;
    int		weight;
    int		i, j;
    
    if (FT_New_Face (_XftFTlibrary, file, id, &face))
	return 0;

    *count = face->num_faces;
    
    pat = XftPatternCreate ();
    if (!pat)
	goto bail0;


    if (!XftPatternAddBool (pat, XFT_CORE, False))
	goto bail1;
    
    if (!XftPatternAddBool (pat, XFT_OUTLINE,
			    (face->face_flags & FT_FACE_FLAG_SCALABLE) != 0))
	goto bail1;
    
    if (!XftPatternAddBool (pat, XFT_SCALABLE,
			    (face->face_flags & FT_FACE_FLAG_SCALABLE) != 0))
	goto bail1;
    

    slant = XFT_SLANT_ROMAN;
    if (face->style_flags & FT_STYLE_FLAG_ITALIC)
	slant = XFT_SLANT_ITALIC;

    if (!XftPatternAddInteger (pat, XFT_SLANT, slant))
	goto bail1;
    
    weight = XFT_WEIGHT_MEDIUM;
    if (face->style_flags & FT_STYLE_FLAG_BOLD)
	weight = XFT_WEIGHT_BOLD;
    
    if (!XftPatternAddInteger (pat, XFT_WEIGHT, weight))
	goto bail1;
    
    if (!XftPatternAddString (pat, XFT_FAMILY, face->family_name))
	goto bail1;

    if (!XftPatternAddString (pat, XFT_STYLE, face->style_name))
	goto bail1;

    if (!XftPatternAddString (pat, XFT_FILE, file))
	goto bail1;

    if (!XftPatternAddInteger (pat, XFT_INDEX, id))
	goto bail1;
    
#if 0
    if ((face->face_flags & FT_FACE_FLAG_FIXED_WIDTH) != 0)
	if (!XftPatternAddInteger (pat, XFT_SPACING, XFT_MONO))
	    goto bail1;
#endif
    
    if (!(face->face_flags & FT_FACE_FLAG_SCALABLE))
    {
	for (i = 0; i < face->num_fixed_sizes; i++)
	    if (!XftPatternAddDouble (pat, XFT_PIXEL_SIZE,
				      (double) face->available_sizes[i].height))
		goto bail1;
    }
    
    for (i = 0; i < face->num_charmaps; i++)
    {
#if 0	
	printf ("face %s encoding %d %c%c%c%c\n",
		face->family_name, i, 
		face->charmaps[i]->encoding >> 24,
		face->charmaps[i]->encoding >> 16,
		face->charmaps[i]->encoding >> 8,
		face->charmaps[i]->encoding >> 0);
#endif
	for (j = 0; j < NUM_FT_ENCODINGS; j++)
	{
	    if (face->charmaps[i]->encoding == xftFtEncoding[j].encoding)
	    {
		if (!XftPatternAddString (pat, XFT_ENCODING, 
					  xftFtEncoding[j].name))
		    goto bail1;
	    }
	}
    }

    if (!XftPatternAddString (pat, XFT_ENCODING, 
			      "glyphs-fontspecific"))
	goto bail1;


    FT_Done_Face (face);
    return pat;
    
bail1:
    XftPatternDestroy (pat);
bail0:
    FT_Done_Face (face);
    return 0;
}

/*
 * List of all open files (each face in a file is managed separately)
 */
typedef struct _XftFtFile {
    struct _XftFtFile	*next;
    int			ref;
    
    char		*file;
    int			id;

    FT_Face		face;
    FT_F26Dot6		size;
    FT_Matrix		matrix;
    int			charmap;
} XftFtFile;

static XftFtFile *_XftFtFiles;

static XftFtFile *
_XftFreeTypeOpenFile (char *file, int id)
{
    XftFtFile	*f;
    FT_Face	face;

    for (f = _XftFtFiles; f; f = f->next)
    {
	if (!strcmp (f->file, file) && f->id == id)
	{
	    ++f->ref;
	    if (_XftFontDebug () & XFT_DBG_REF)
		printf ("FontFile %s/%d matches existing (%d)\n",
			file, id, f->ref);
	    return f;
	}
    }
    if (FT_New_Face (_XftFTlibrary, file, id, &face))
	return 0;
    
    f = malloc (sizeof (XftFtFile) + strlen (file) + 1);
    if (!f)
	return 0;
    
    if (_XftFontDebug () & XFT_DBG_REF)
    	printf ("FontFile %s/%d matches new\n",
		file, id);
    f->next = _XftFtFiles;
    _XftFtFiles = f;
    f->ref = 1;
    
    f->file = (char *) (f+1);
    strcpy (f->file, file);
    f->id = id;
    
    f->face = face;
    f->size = 0;
    f->charmap = -1;
    f->matrix.xx = 0x10000;
    f->matrix.xy = 0x0;
    f->matrix.yy = 0x10000;
    f->matrix.yx = 0x0;
    return f;
}

Bool
XftFreeTypeSetFace (FT_Face face, FT_F26Dot6 size, int charmap, FT_Matrix *matrix)
{
    XftFtFile	*f, **prev;
    
    for (prev = &_XftFtFiles; (f = *prev); prev = &f->next)
    {
	if (f->face == face)
	{
	    /* LRU */
	    if (prev != &_XftFtFiles)
	    {
		*prev = f->next;
		f->next = _XftFtFiles;
		_XftFtFiles = f;
	    }
	    if (f->size != size)
	    {
		if (_XftFontDebug() & XFT_DBG_GLYPH)
		    printf ("Set face size to %d (%d)\n", 
			    (int) (size >> 6), (int) size);
		if (FT_Set_Char_Size (face, size, size, 0, 0))
		    return False;
		f->size = size;
	    }
	    if (f->charmap != charmap && charmap != -1)
	    {
		if (_XftFontDebug() & XFT_DBG_GLYPH)
		    printf ("Set face charmap to %d\n", charmap);
		if (FT_Set_Charmap (face, face->charmaps[charmap]))
		    return False;
		f->charmap = charmap;
	    }
	    if (!FT_Matrix_Equal (&f->matrix, matrix))
	    {
		if (_XftFontDebug() & XFT_DBG_GLYPH)
		    printf ("Set face matrix to (%g,%g,%g,%g)\n",
			    (double) matrix->xx / 0x10000,
			    (double) matrix->xy / 0x10000,
			    (double) matrix->yx / 0x10000,
			    (double) matrix->yy / 0x10000);
		FT_Set_Transform (face, matrix, 0);
		f->matrix = *matrix;
	    }
	    break;
	}
    }
    return True;
}

static void
_XftFreeTypeCloseFile (XftFtFile *f)
{
    XftFtFile	**prev;
    
    if (--f->ref != 0)
	return;
    for (prev = &_XftFtFiles; *prev; prev = &(*prev)->next)
    {
	if (*prev == f)
	{
	    *prev = f->next;
	    break;
	}
    }
    FT_Done_Face (f->face);
    free (f);
}

/*
 * Cache of all glyphsets
 */
typedef struct _XftFtGlyphSet {
    struct _XftFtGlyphSet   *next;
    int			    ref;
    
    XftFtFile		    *file;
    Bool		    minspace;
    int			    char_width;
    
    XftFontStruct	    font;
} XftFtGlyphSet;

XftFontStruct *
XftFreeTypeOpen (Display *dpy, XftPattern *pattern)
{
    XftDisplayInfo  *info = _XftDisplayInfoGet (dpy);
    XftFtFile	    *file;
    FT_Face	    face;
    XftFtGlyphSet   *gs;
    char	    *filename;
    int		    id;
    double	    dsize;
    FT_F26Dot6	    size;
    int		    rgba;
    int		    spacing;
    int		    char_width;
    Bool	    antialias;
    Bool	    minspace;
    char	    *encoding_name;
    XftFontStruct   *font;
    int		    j;
    FT_Encoding	    encoding;
    int		    charmap;
    FT_Matrix	    matrix;
    XftMatrix	    *font_matrix;

#if 0
    int		    extra;
#endif
    int		    height, ascent, descent;
    XRenderPictFormat	pf, *format;
    
    /*
     * Open the file
     */
    if (XftPatternGetString (pattern, XFT_FILE, 0, &filename) != XftResultMatch)
	goto bail0;
    
    if (XftPatternGetInteger (pattern, XFT_INDEX, 0, &id) != XftResultMatch)
	goto bail0;
    
    file = _XftFreeTypeOpenFile (filename, id);
    if (!file)
	goto bail0;
    
    face = file->face;

    /*
     * Extract the glyphset information from the pattern
     */
    if (XftPatternGetString (pattern, XFT_ENCODING, 0, &encoding_name) != XftResultMatch)
	goto bail0;
    
    if (XftPatternGetDouble (pattern, XFT_PIXEL_SIZE, 0, &dsize) != XftResultMatch)
	goto bail0;
    
    switch (XftPatternGetInteger (pattern, XFT_RGBA, 0, &rgba)) {
    case XftResultNoMatch:
	rgba = XFT_RGBA_NONE;
	break;
    case XftResultMatch:
	break;
    default:
	goto bail0;
    }

    switch (XftPatternGetBool (pattern, XFT_ANTIALIAS, 0, &antialias)) {
    case XftResultNoMatch:
	antialias = True;
	break;
    case XftResultMatch:
	break;
    default:
	goto bail0;
    }
    
    switch (XftPatternGetBool (pattern, XFT_MINSPACE, 0, &minspace)) {
    case XftResultNoMatch:
	minspace = False;
	break;
    case XftResultMatch:
	break;
    default:
	goto bail0;
    }
    
    switch (XftPatternGetInteger (pattern, XFT_SPACING, 0, &spacing)) {
    case XftResultNoMatch:
	spacing = XFT_PROPORTIONAL;
	break;
    case XftResultMatch:
	break;
    default:
	goto bail1;
    }

    if (XftPatternGetInteger (pattern, XFT_CHAR_WIDTH, 
			      0, &char_width) != XftResultMatch)
    {
	char_width = 0;
    }
    else if (char_width)
	spacing = XFT_MONO;
    
    matrix.xx = matrix.yy = 0x10000;
    matrix.xy = matrix.yx = 0;
    
    switch (XftPatternGetMatrix (pattern, XFT_MATRIX, 0, &font_matrix)) {
    case XftResultNoMatch:
	break;
    case XftResultMatch:
	matrix.xx = 0x10000L * font_matrix->xx;
	matrix.yy = 0x10000L * font_matrix->yy;
	matrix.xy = 0x10000L * font_matrix->xy;
	matrix.yx = 0x10000L * font_matrix->yx;
	break;
    default:
	goto bail1;
    }

    
    
    if (XftPatternGetInteger (pattern, XFT_CHAR_WIDTH, 
			      0, &char_width) != XftResultMatch)
    {
	char_width = 0;
    }
    else if (char_width)
	spacing = XFT_MONO;

    encoding = face->charmaps[0]->encoding;
    
    for (j = 0; j < NUM_FT_ENCODINGS; j++)
	if (!strcmp (encoding_name, xftFtEncoding[j].name))
	{
	    encoding = xftFtEncoding[j].encoding;
	    break;
	}
    
    size = (FT_F26Dot6) (dsize * 64.0);
    
    if (encoding == ft_encoding_none)
	charmap = -1;
    else
    {
	for (charmap = 0; charmap < face->num_charmaps; charmap++)
	    if (face->charmaps[charmap]->encoding == encoding)
		break;

	if (charmap == face->num_charmaps)
	    goto bail1;
    }

    
    /*
     * Match an existing glyphset
     */
    for (gs = info->glyphSets; gs; gs = gs->next)
    {
	if (gs->file == file &&
	    gs->minspace == minspace &&
	    gs->char_width == char_width &&
	    gs->font.size == size &&
	    gs->font.spacing == spacing &&
	    gs->font.charmap == charmap &&
	    gs->font.rgba == rgba &&
	    gs->font.antialias == antialias &&
	    FT_Matrix_Equal (&gs->font.matrix, &matrix))
	{
	    ++gs->ref;
	    if (_XftFontDebug () & XFT_DBG_REF)
	    {
		printf ("Face size %g matches existing (%d)\n",
			dsize, gs->ref);
	    }
	    return &gs->font;
	}
    }
    
    if (_XftFontDebug () & XFT_DBG_REF)
    {
	printf ("Face size %g matches new\n",
		dsize);
    }
    /*
     * No existing glyphset, create another
     */
    gs = malloc (sizeof (XftFtGlyphSet));
    if (!gs)
	goto bail1;

    gs->ref = 1;
    
    gs->file = file;
    gs->minspace = minspace;
    gs->char_width = char_width;

    font = &gs->font;
    
    if (antialias)
    {
	switch (rgba) {
	case FC_RGBA_RGB:
	case FC_RGBA_BGR:
	case FC_RGBA_VRGB:
	case FC_RGBA_VBGR:
	    pf.depth = 32;
	    pf.type = PictTypeDirect;
	    pf.direct.alpha = 24;
	    pf.direct.alphaMask = 0xff;
	    pf.direct.red = 16;
	    pf.direct.redMask = 0xff;
	    pf.direct.green = 8;
	    pf.direct.greenMask = 0xff;
	    pf.direct.blue = 0;
	    pf.direct.blueMask = 0xff;
	    format = XRenderFindFormat(dpy, 
				       PictFormatType|
				       PictFormatDepth|
				       PictFormatAlpha|
				       PictFormatAlphaMask|
				       PictFormatRed|
				       PictFormatRedMask|
				       PictFormatGreen|
				       PictFormatGreenMask|
				       PictFormatBlue|
				       PictFormatBlueMask,
				       &pf, 0);
	    break;
	default:
	    pf.depth = 8;
	    pf.type = PictTypeDirect;
	    pf.direct.alpha = 0;
	    pf.direct.alphaMask = 0xff;
	    format = XRenderFindFormat(dpy, 
				       PictFormatType|
				       PictFormatDepth|
				       PictFormatAlpha|
				       PictFormatAlphaMask,
				       &pf, 0);
	    break;
	}
    }
    else
    {
	pf.depth = 1;
	pf.type = PictTypeDirect;
	pf.direct.alpha = 0;
	pf.direct.alphaMask = 0x1;
	format = XRenderFindFormat(dpy, 
				   PictFormatType|
				   PictFormatDepth|
				   PictFormatAlpha|
				   PictFormatAlphaMask,
				   &pf, 0);
    }
    
    if (!format)
	goto bail2;
    
    if (!XftFreeTypeSetFace (face, size, charmap, &matrix))
	goto bail2;

    descent = -(face->size->metrics.descender >> 6);
    ascent = face->size->metrics.ascender >> 6;
    if (minspace)
    {
	height = ascent + descent;
    }
    else
    {
	height = face->size->metrics.height >> 6;
#if 0
	extra = (height - (ascent + descent));
	if (extra > 0)
	{
	    ascent = ascent + extra / 2;
	    descent = height - ascent;
	}
	else if (extra < 0)
	    height = ascent + descent;
#endif
    }
    font->ascent = ascent;
    font->descent = descent;
    font->height = height;
    
    if (char_width)
	font->max_advance_width = char_width;
    else
	font->max_advance_width = face->size->metrics.max_advance >> 6;
    
    gs->next = info->glyphSets;
    info->glyphSets = gs;
    
    font->glyphset = XRenderCreateGlyphSet (dpy, format);

    font->size = size;
    font->spacing = spacing;
    font->format = format;
    font->realized =0;
    font->nrealized = 0;
    font->rgba = rgba;
    font->antialias = antialias;
    font->charmap = charmap;
    font->transform = (matrix.xx != 0x10000 || matrix.xy != 0 ||
		       matrix.yx != 0 || matrix.yy != 0x10000);
    font->matrix = matrix;
    font->face = face;

    return font;
    
bail2:
    free (gs);
bail1:
    _XftFreeTypeCloseFile (file);
bail0:
    return 0;
}

void
XftFreeTypeClose (Display *dpy, XftFontStruct *font)
{
    XftFtGlyphSet   *gs, **prev;
    XftDisplayInfo  *info = _XftDisplayInfoGet (dpy);
    int		    i;
    XGlyphInfo	    *gi;

    for (prev = &info->glyphSets; (gs = *prev); prev = &gs->next)
    {
	if (&gs->font == font)
	{
	    if (--gs->ref == 0)
	    {
		XRenderFreeGlyphSet (dpy, font->glyphset);
		for (i = 0; i < font->nrealized; i++)
		{
		    gi = font->realized[i];
		    if (gi && gi != XftUntestedGlyph)
			free (gi);
		}
		if (font->realized)
		    free (font->realized);
		
		_XftFreeTypeCloseFile (gs->file);

		*prev = gs->next;
		free (gs);
	    }
	    break;
	}
    }
}
		  
XftFontStruct *
XftFreeTypeGet (XftFont *font)
{
    if (font->core)
	return 0;
    return font->u.ft.font;
}

/* #define XFT_DEBUG_FONTSET */

Bool
XftInitFtLibrary (void)
{
    if (_XftFTlibrary)
	return True;
    if (FT_Init_FreeType (&_XftFTlibrary))
	return False;
    _XftFontSet = FcConfigGetFonts (0, FcSetSystem);
    if (!_XftFontSet)
	return False;
    return True;
}

/*
 * font.c
 *
 * map dvi fonts to X fonts
 */
/* $XFree86: xc/programs/xditview/font.c,v 1.5 2001/08/27 23:35:12 dawes Exp $ */

#include <X11/Xos.h>
#include <X11/IntrinsicP.h>
#include <X11/StringDefs.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include "DviP.h"
#include "XFontName.h"

static char *
savestr (char *s)
{
	char	*n;

	if (!s)
		return 0;
	n = XtMalloc (strlen (s) + 1);
	if (n)
		strcpy (n, s);
	return n;
}

static DviFontList *
LookupFontByPosition (DviWidget dw, int position)
{
	DviFontList	*f;

	for (f = dw->dvi.fonts; f; f=f->next)
		if (f->dvi_number == position)
			break;
	return f;
}

static DviFontSizeList *
LookupFontSizeBySize (DviWidget dw, DviFontList *f, int size)
{
    DviFontSizeList *fs, *best = 0;
    int		    bestdist;
    char	    fontNameString[2048];
    XFontName	    fontName;
    unsigned int    fontNameAttributes;
    int		    dist;

    if (f->scalable)
    {
	for (best = f->sizes; best; best = best->next)
	    if (best->size == size)
		return best;
	best = (DviFontSizeList *) XtMalloc (sizeof *best);
	best->next = f->sizes;
	best->size = size;
	XParseFontName (f->x_name, &fontName, &fontNameAttributes);
	fontNameAttributes &= ~(FontNamePixelSize|FontNameAverageWidth);
	fontNameAttributes |= FontNameResolutionX;
	fontNameAttributes |= FontNameResolutionY;
	fontNameAttributes |= FontNamePointSize;
	fontName.ResolutionX = dw->dvi.screen_resolution;
	fontName.ResolutionY = dw->dvi.screen_resolution;
	fontName.PointSize = size * 10 / dw->dvi.size_scale;
	XFormatFontName (&fontName, fontNameAttributes, fontNameString);
	best->x_name = savestr (fontNameString);
#ifdef USE_XFT
	/*
	 * Force a match of a core font for adobe-fontspecific
	 * encodings; we dont have a scalable font in
	 * the right encoding
	 */
	best->core = False;
	if (!strcmp (fontName.CharSetRegistry, "adobe") &&
	    !strcmp (fontName.CharSetEncoding, "fontspecific"))
	{
	    best->core = True;
	}
#endif
	best->doesnt_exist = 0;
	best->font = 0;
	f->sizes = best;
    }
    else
    {
	bestdist = 65536;
    	for (fs = f->sizes; fs; fs=fs->next) {
	    dist = size - fs->size;
	    if (dist < 0)
		dist = -dist * 16;
	    if (dist < bestdist)
	    {
		best = fs;
		bestdist = dist;
	    }
    	}
    }
    return best;
}

static char *
SkipFontNameElement (char *n)
{
	while (*n != '-')
		if (!*++n)
			return 0;
	return n+1;
}

# define SizePosition		8
# define EncodingPosition	13

#ifndef USE_XFT
static int
ConvertFontNameToSize (char *n)
{
	int	i, size;

	for (i = 0; i < SizePosition; i++) {
		n = SkipFontNameElement (n);
		if (!n)
			return -1;
	}
	size = atoi (n);
	return size/10;
}
#endif

static char *
ConvertFontNameToEncoding (char *n)
{
        int i;
	for (i = 0; i < EncodingPosition; i++) {
		n = SkipFontNameElement (n);
		if (!n)
			return 0;
	}
	return n;
}

static void
DisposeFontSizes (DviWidget dw, DviFontSizeList *fs)
{
    DviFontSizeList	*next;

    for (; fs; fs=next) {
	next = fs->next;
	if (fs->x_name)
		XtFree (fs->x_name);
	if (fs->font)
	{
#ifdef USE_XFT
	    XftFontClose (XtDisplay (dw), fs->font);
#else
	    XUnloadFont (XtDisplay (dw), fs->font->fid);
	    XFree ((char *)fs->font);
#endif
	}
	XtFree ((char *) fs);
    }
}

void
ResetFonts (DviWidget dw)
{
    DviFontList	*f;
    
    for (f = dw->dvi.fonts; f; f = f->next)
    {
	if (f->initialized)
	{
	    DisposeFontSizes (dw, f->sizes);
	    f->sizes = 0;
	    f->initialized = FALSE;
	    f->scalable = FALSE;
	}
    }
    /* 
     * force requery of fonts
     */
    dw->dvi.font = 0;
    dw->dvi.font_number = -1;
    dw->dvi.cache.font = 0;
    dw->dvi.cache.font_number = -1;
}

static DviFontSizeList *
InstallFontSizes (DviWidget dw, char *x_name, Boolean *scalablep)
{
#ifndef USE_XFT
    char	    fontNameString[2048];
    char	    **fonts;
    int		    i, count;
    int		    size;
    DviFontSizeList *new;
    XFontName	    fontName;
    unsigned int    fontNameAttributes;
#endif
    DviFontSizeList *sizes;

    sizes = 0;
#ifdef USE_XFT
    *scalablep = TRUE;
#else
    *scalablep = FALSE;
    if (!XParseFontName (x_name, &fontName, &fontNameAttributes))
	return 0;
    
    fontNameAttributes &= ~(FontNamePixelSize|FontNamePointSize);
    fontNameAttributes |= FontNameResolutionX;
    fontNameAttributes |= FontNameResolutionY;
    fontName.ResolutionX = dw->dvi.screen_resolution;
    fontName.ResolutionY = dw->dvi.screen_resolution;
    XFormatFontName (&fontName, fontNameAttributes, fontNameString);
    fonts = XListFonts (XtDisplay (dw), fontNameString, 10000000, &count);
    for (i = 0; i < count; i++) {
	size = ConvertFontNameToSize (fonts[i]);
	if (size == 0)
	{
	    DisposeFontSizes (dw, sizes);
	    *scalablep = TRUE;
	    sizes = 0;
	    break;
	}
	if (size != -1) {
	    new = (DviFontSizeList *) XtMalloc (sizeof *new);
	    new->next = sizes;
	    new->size = size;
	    new->x_name = savestr (fonts[i]);
	    new->doesnt_exist = 0;
	    new->font = 0;
	    sizes = new;
	}
    }
    XFreeFontNames (fonts);
#endif
    return sizes;
}

static DviFontList *
InstallFont (DviWidget dw, int position, char *dvi_name, char *x_name)
{
    DviFontList	*f;
    char		*encoding;

    f = LookupFontByPosition (dw, position);
    if (f) {
	/*
	 * ignore gratuitous font loading
	 */
	if (!strcmp (f->dvi_name, dvi_name) && !strcmp (f->x_name, x_name))
	    return f;

	DisposeFontSizes (dw, f->sizes);
	if (f->dvi_name)
	    XtFree (f->dvi_name);
	if (f->x_name)
	    XtFree (f->x_name);
    } else {
	f = (DviFontList *) XtMalloc (sizeof (*f));
	f->next = dw->dvi.fonts;
	dw->dvi.fonts = f;
    }
    f->initialized = FALSE;
    f->dvi_name = savestr (dvi_name);
    f->x_name = savestr (x_name);
    f->dvi_number = position;
    f->sizes = 0;
    f->scalable = FALSE;
    if (f->x_name) {
	encoding = ConvertFontNameToEncoding (f->x_name);
	f->char_map = DviFindMap (encoding);
    } else
	f->char_map = 0;
    /* 
     * force requery of fonts
     */
    dw->dvi.font = 0;
    dw->dvi.font_number = -1;
    dw->dvi.cache.font = 0;
    dw->dvi.cache.font_number = -1;
    return f;
}

static char *
MapDviNameToXName (DviWidget dw, char *dvi_name)
{
    DviFontMap	*fm;
    
    for (fm = dw->dvi.font_map; fm; fm=fm->next)
	if (!strcmp (fm->dvi_name, dvi_name))
	    return fm->x_name;
    ++dvi_name;
    for (fm = dw->dvi.font_map; fm; fm=fm->next)
	if (!strcmp (fm->dvi_name, "R"))
	    return fm->x_name;
    if (dw->dvi.font_map->x_name)
	return dw->dvi.font_map->x_name;
    return "-*-*-*-*-*-*-*-*-*-*-*-*-iso8859-1";
}

#ifdef NOTUSED
static char *
MapXNameToDviName (dw, x_name)
	DviWidget	dw;
	char		*x_name;
{
    DviFontMap	*fm;
    
    for (fm = dw->dvi.font_map; fm; fm=fm->next)
	if (!strcmp (fm->x_name, x_name))
	    return fm->dvi_name;
    return 0;
}
#endif

void
ParseFontMap (dw)
	DviWidget	dw;
{
    char		dvi_name[1024];
    char		x_name[2048];
    char		*m, *s;
    DviFontMap	*fm, *new;

    if (dw->dvi.font_map)
	    DestroyFontMap (dw->dvi.font_map);
    fm = 0;
    m = dw->dvi.font_map_string;
    while (*m) {
	s = m;
	while (*m && !isspace (*m))
	    ++m;
	strncpy (dvi_name, s, m-s);
	dvi_name[m-s] = '\0';
	while (isspace (*m))
	    ++m;
	s = m;
	while (*m && *m != '\n')
	    ++m;
	strncpy (x_name, s, m-s);
	x_name[m-s] = '\0';
	new = (DviFontMap *) XtMalloc (sizeof *new);
	new->x_name = savestr (x_name);
	new->dvi_name = savestr (dvi_name);
	new->next = fm;
	fm = new;
	++m;
    }
    dw->dvi.font_map = fm;
}

void
DestroyFontMap (font_map)
    DviFontMap	*font_map;
{
    DviFontMap	*next;

    for (; font_map; font_map = next) {
	next = font_map->next;
	if (font_map->x_name)
	    XtFree (font_map->x_name);
	if (font_map->dvi_name)
	    XtFree (font_map->dvi_name);
	XtFree ((char *) font_map);
    }
}

/*ARGSUSED*/
void
SetFontPosition (dw, position, dvi_name, extra)
    DviWidget	dw;
    int		position;
    char	*dvi_name;
    char	*extra;	/* unused */
{
    char	*x_name;

    x_name = MapDviNameToXName (dw, dvi_name);
    (void) InstallFont (dw, position, dvi_name, x_name);
}

#ifdef USE_XFT
XftFont *
#else
XFontStruct *
#endif
QueryFont (dw, position, size)
    DviWidget	dw;
    int		position;
    int		size;
{
    DviFontList	*f;
    DviFontSizeList	*fs;

    f = LookupFontByPosition (dw, position);
    if (!f)
	return dw->dvi.default_font;
    if (!f->initialized) {
	f->sizes = InstallFontSizes (dw, f->x_name, &f->scalable);
	f->initialized = TRUE;
    }
    fs = LookupFontSizeBySize (dw, f, size);
    if (!fs)
	return dw->dvi.default_font;
    if (!fs->font) {
	if (fs->x_name)
	{
#ifdef USE_XFT
	    XftPattern	*pat;
	    XftPattern	*match;
	    XftResult	result;

	    pat = XftXlfdParse (fs->x_name, False, False);
	    XftPatternAddBool (pat, XFT_CORE, fs->core);
	    match = XftFontMatch (XtDisplay (dw),
				  XScreenNumberOfScreen(dw->core.screen),
				  pat, &result);
	    XftPatternDestroy (pat);
	    if (match)
	    {
		fs->font = XftFontOpenPattern (XtDisplay (dw),
					       match);
		if (!fs->font)
		    XftPatternDestroy (match);
	    }
	    else
		fs->font = 0;
#else
	    fs->font = XLoadQueryFont (XtDisplay (dw), fs->x_name);
#endif
	}
	if (!fs->font)
	    fs->font = dw->dvi.default_font;
    }
    return fs->font;
}

DviCharNameMap *
QueryFontMap (dw, position)
	DviWidget	dw;
	int		position;
{
	DviFontList	*f;

	f = LookupFontByPosition (dw, position);
	if (f)
	    return f->char_map;
	else
	    return 0;
}

unsigned char *
DviCharIsLigature (map, name)
    DviCharNameMap  *map;
    char	    *name;
{
    int	    i;

    for (i = 0; i < DVI_MAX_LIGATURES; i++) {
	if (!map->ligatures[i][0])
	    break;
	if (!strcmp (name, map->ligatures[i][0]))
	    return (unsigned char *) map->ligatures[i][1];
    }
    return 0;
}

#if 0
LoadFont (dw, position, size)
	DviWidget	dw;
	int		position;
	int		size;
{
	XFontStruct	*font;

	font = QueryFont (dw, position, size);
	dw->dvi.font_number = position;
	dw->dvi.font_size = size;
	dw->dvi.font = font;
	XSetFont (XtDisplay (dw), dw->dvi.normal_GC, font->fid);
	return;
}
#endif

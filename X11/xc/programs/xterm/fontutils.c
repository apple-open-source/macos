/*
 * $XFree86: xc/programs/xterm/fontutils.c,v 1.44 2004/01/09 00:10:32 dickey Exp $
 */

/************************************************************

Copyright 1998-2002,2003 by Thomas E. Dickey

                        All Rights Reserved

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE ABOVE LISTED COPYRIGHT HOLDER(S) BE LIABLE FOR ANY
CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name(s) of the above copyright
holders shall not be used in advertising or otherwise to promote the
sale, use or other dealings in this Software without prior written
authorization.

********************************************************/

/*
 * A portion of this module (for FontNameProperties) is adapted from EMU 1.3;
 * it constructs font names with specific properties changed, e.g., for bold
 * and double-size characters.
 */

#define RES_OFFSET(field)	XtOffsetOf(SubResourceRec, field)

#include <fontutils.h>

#include <main.h>
#include <data.h>
#include <menu.h>
#include <xstrings.h>
#include <xterm.h>

#include <stdio.h>
#include <ctype.h>

/* from X11/Xlibint.h - not all vendors install this file */
#define CI_NONEXISTCHAR(cs) (((cs)->width == 0) && \
			     (((cs)->rbearing|(cs)->lbearing| \
			       (cs)->ascent|(cs)->descent) == 0))

#define CI_GET_CHAR_INFO_1D(fs,col,def,cs) \
{ \
    cs = def; \
    if (col >= fs->min_char_or_byte2 && col <= fs->max_char_or_byte2) { \
	if (fs->per_char == NULL) { \
	    cs = &fs->min_bounds; \
	} else { \
	    cs = &fs->per_char[(col - fs->min_char_or_byte2)]; \
	    if (CI_NONEXISTCHAR(cs)) cs = def; \
	} \
    } \
}

#define CI_GET_CHAR_INFO_2D(fs,row,col,def,cs) \
{ \
    cs = def; \
    if (row >= fs->min_byte1 && row <= fs->max_byte1 && \
	col >= fs->min_char_or_byte2 && col <= fs->max_char_or_byte2) { \
	if (fs->per_char == NULL) { \
	    cs = &fs->min_bounds; \
	} else { \
	    cs = &fs->per_char[((row - fs->min_byte1) * \
				(fs->max_char_or_byte2 - \
				 fs->min_char_or_byte2 + 1)) + \
			       (col - fs->min_char_or_byte2)]; \
	    if (CI_NONEXISTCHAR(cs)) cs = def; \
	} \
    } \
}

#define MAX_FONTNAME 200

/*
 * A structure to hold the relevant properties from a font
 * we need to make a well formed font name for it.
 */
typedef struct {
    /* registry, foundry, family */
    char *beginning;
    /* weight */
    char *weight;
    /* slant */
    char *slant;
    /* wideness */
    char *wideness;
    /* add style */
    char *add_style;
    int pixel_size;
    char *point_size;
    int res_x;
    int res_y;
    char *spacing;
    int average_width;
    /* charset registry, charset encoding */
    char *end;
} FontNameProperties;

/*
 * Returns the fields from start to stop in a dash- separated string.  This
 * function will modify the source, putting '\0's in the appropiate place and
 * moving the beginning forward to after the '\0'
 *
 * This will NOT work for the last field (but we won't need it).
 */
static char *
n_fields(char **source, int start, int stop)
{
    int i;
    char *str, *str1;

    /*
     * find the start-1th dash
     */
    for (i = start - 1, str = *source; i; i--, str++)
	if ((str = strchr(str, '-')) == 0)
	    return 0;

    /*
     * find the stopth dash
     */
    for (i = stop - start + 1, str1 = str; i; i--, str1++)
	if ((str1 = strchr(str1, '-')) == 0)
	    return 0;

    /*
       * put a \0 at the end of the fields
     */
    *(str1 - 1) = '\0';

    /*
       * move source forward
     */
    *source = str1;

    return str;
}

/*
 * Gets the font properties from a given font structure.  We use the FONT name
 * to find them out, since that seems easier.
 *
 * Returns a pointer to a static FontNameProperties structure
 * or NULL on error.
 */
static FontNameProperties *
get_font_name_props(Display * dpy, XFontStruct * fs, char *result)
{
    static FontNameProperties props;
    static char *last_name;

    register XFontProp *fp;
    register int i;
    Atom fontatom = XInternAtom(dpy, "FONT", False);
    char *name;
    char *str;

    /*
     * first get the full font name
     */
    for (name = 0, i = 0, fp = fs->properties;
	 i < fs->n_properties;
	 i++, fp++)
	if (fp->name == fontatom)
	    name = XGetAtomName(dpy, fp->card32);

    if (name == 0)
	return 0;

    /*
     * XGetAtomName allocates memory - don't leak
     */
    if (last_name != 0)
	XFree(last_name);
    last_name = name;
    if (result != 0)
	strcpy(result, name);

    /*
     * Now split it up into parts and put them in
     * their places. Since we are using parts of
     * the original string, we must not free the Atom Name
     */

    /* registry, foundry, family */
    if ((props.beginning = n_fields(&name, 1, 3)) == 0)
	return 0;

    /* weight is the next */
    if ((props.weight = n_fields(&name, 1, 1)) == 0)
	return 0;

    /* slant */
    if ((props.slant = n_fields(&name, 1, 1)) == 0)
	return 0;

    /* width */
    if ((props.wideness = n_fields(&name, 1, 1)) == 0)
	return 0;

    /* add style */
    if ((props.add_style = n_fields(&name, 1, 1)) == 0)
	return 0;

    /* pixel size */
    if ((str = n_fields(&name, 1, 1)) == 0)
	return 0;
    if ((props.pixel_size = atoi(str)) == 0)
	return 0;

    /* point size */
    if ((props.point_size = n_fields(&name, 1, 1)) == 0)
	return 0;

    /* res_x */
    if ((str = n_fields(&name, 1, 1)) == 0)
	return 0;
    if ((props.res_x = atoi(str)) == 0)
	return 0;

    /* res_y */
    if ((str = n_fields(&name, 1, 1)) == 0)
	return 0;
    if ((props.res_y = atoi(str)) == 0)
	return 0;

    /* spacing */
    if ((props.spacing = n_fields(&name, 1, 1)) == 0)
	return 0;

    /* average width */
    if ((str = n_fields(&name, 1, 1)) == 0)
	return 0;
    if ((props.average_width = atoi(str)) == 0)
	return 0;

    /* the rest: charset registry and charset encoding */
    props.end = name;

    return &props;
}

/*
 * Take the given font props and try to make a well formed font name specifying
 * the same base font and size and everything, but in bold.  The return value
 * comes from a static variable, so be careful if you reuse it.
 */
static char *
bold_font_name(FontNameProperties * props, Bool useWidth)
{
    static char ret[MAX_FONTNAME];
    char average_width[MAX_FONTNAME];

    /*
     * Put together something in the form
     * "<beginning>-bold-<middle>-<pixel_size>-<point_size>-<res_x>-<res_y>"\
     * "-<spacing>-*-<end>"
     */
    if (useWidth)
	sprintf(average_width, "%d", props->average_width);
    else
	strcpy(average_width, "*");
    sprintf(ret, "%s-bold-%s-%s-%s-%d-%s-%d-%d-%s-%s-%s",
	    props->beginning,
	    props->slant,
	    props->wideness,
	    props->add_style,
	    props->pixel_size,
	    props->point_size,
	    props->res_x,
	    props->res_y,
	    props->spacing,
	    average_width,
	    props->end);
    return ret;
}

#if OPT_WIDE_CHARS
/* like bold_font_name, but doubles AVERAGE_WIDTH */
static char *
wide_font_name(FontNameProperties * props)
{
    static char ret[MAX_FONTNAME];

    /*
     * Put together something in the form
     * "<beginning>-bold-<middle>-<pixel_size>-<point_size>-<res_x>-<res_y>"\
     * "-<spacing>-*-<end>"
     */
    sprintf(ret, "%s-%s-%s-*-*-%d-%s-%d-%d-%s-%i-%s",
	    props->beginning,
	    props->weight,
	    props->slant,
	    props->pixel_size,
	    props->point_size,
	    props->res_x,
	    props->res_y,
	    props->spacing,
	    props->average_width * 2,
	    props->end);

    return ret;
}
#endif /* OPT_WIDE_CHARS */

#if OPT_DEC_CHRSET
/*
 * Take the given font props and try to make a well formed font name specifying
 * the same base font but changed depending on the given attributes and chrset.
 *
 * For double width fonts, we just double the X-resolution, for double height
 * fonts we double the pixel-size and Y-resolution
 */
char *
xtermSpecialFont(unsigned atts, unsigned chrset)
{
#if OPT_TRACE
    static char old_spacing[80];
    static FontNameProperties old_props;
#endif
    TScreen *screen = &term->screen;
    FontNameProperties *props;
    char tmp[MAX_FONTNAME];
    char *ret;
    char *width;
    int pixel_size;
    int res_x;
    int res_y;

    props = get_font_name_props(screen->display, screen->fnt_norm, (char *) 0);
    if (props == 0)
	return 0;

    pixel_size = props->pixel_size;
    res_x = props->res_x;
    res_y = props->res_y;
    if (atts & BOLD)
	width = "bold";
    else
	width = props->weight;

    if (CSET_DOUBLE(chrset))
	res_x *= 2;

    if (chrset == CSET_DHL_TOP
	|| chrset == CSET_DHL_BOT) {
	res_y *= 2;
	pixel_size *= 2;
    }
#if OPT_TRACE
    if (old_props.res_x != res_x
	|| old_props.res_x != res_y
	|| old_props.pixel_size != pixel_size
	|| strcmp(old_props.spacing, props->spacing)) {
	TRACE(("xtermSpecialFont(atts = %#x, chrset = %#x)\n", atts, chrset));
	TRACE(("res_x      = %d\n", res_x));
	TRACE(("res_y      = %d\n", res_y));
	TRACE(("point_size = %s\n", props->point_size));
	TRACE(("pixel_size = %d\n", pixel_size));
	TRACE(("spacing    = %s\n", props->spacing));
	old_props.res_x = res_x;
	old_props.res_x = res_y;
	old_props.pixel_size = pixel_size;
	old_props.spacing = strcpy(old_spacing, props->spacing);
    }
#endif

    if (atts & NORESOLUTION)
	sprintf(tmp, "%s-%s-%s-%s-%s-%d-%s-*-*-%s-*-%s",
		props->beginning,
		width,
		props->slant,
		props->wideness,
		props->add_style,
		pixel_size,
		props->point_size,
		props->spacing,
		props->end);
    else
	sprintf(tmp, "%s-%s-%s-%s-%s-%d-%s-%d-%d-%s-*-%s",
		props->beginning,
		width,
		props->slant,
		props->wideness,
		props->add_style,
		pixel_size,
		props->point_size,
		res_x,
		res_y,
		props->spacing,
		props->end);

    ret = XtMalloc(strlen(tmp) + 1);
    strcpy(ret, tmp);

    return ret;
}
#endif /* OPT_DEC_CHRSET */

/*
 * Case-independent comparison for font-names, including wildcards.
 * XLFD allows '?' as a wildcard, but we do not handle that (no one seems
 * to use it).
 */
static Boolean
same_font_name(char *pattern, char *match)
{
    while (*pattern && *match) {
	if (*pattern == *match) {
	    pattern++;
	    match++;
	} else if (*pattern == '*' || *match == '*') {
	    if (same_font_name(pattern + 1, match)) {
		return True;
	    } else if (same_font_name(pattern, match + 1)) {
		return True;
	    } else {
		return False;
	    }
	} else {
	    if (char2lower(*pattern++) != char2lower(*match++))
		return False;
	}
    }
    return (*pattern == *match);	/* both should be NUL */
}

/*
 * Double-check the fontname that we asked for versus what the font server
 * actually gave us.  The larger fixed fonts do not always have a matching bold
 * font, and the font server may try to scale another font or otherwise
 * substitute a mismatched font.
 *
 * If we cannot get what we requested, we will fallback to the original
 * behavior, which simulates bold by overstriking each character at one pixel
 * offset.
 */
static int
got_bold_font(Display * dpy, XFontStruct * fs, char *requested)
{
    char actual[MAX_FONTNAME];
    int got;

    if (get_font_name_props(dpy, fs, actual) == 0)
	got = 0;
    else
	got = same_font_name(requested, actual);
    return got;
}

/*
 * If the font server tries to adjust another font, it may not adjust it
 * properly.  Check that the bounding boxes are compatible.  Otherwise we'll
 * leave trash on the display when we mix normal and bold fonts.
 */
static int
same_font_size(XFontStruct * nfs, XFontStruct * bfs)
{
    TRACE(("same_font_size height %d/%d, min %d/%d max %d/%d\n",
	   nfs->ascent + nfs->descent,
	   bfs->ascent + bfs->descent,
	   nfs->min_bounds.width, bfs->min_bounds.width,
	   nfs->max_bounds.width, bfs->max_bounds.width));
    return term->screen.free_bold_box
	|| ((nfs->ascent + nfs->descent) == (bfs->ascent + bfs->descent)
	    && (nfs->min_bounds.width == bfs->min_bounds.width
		|| nfs->min_bounds.width == bfs->min_bounds.width + 1)
	    && (nfs->max_bounds.width == bfs->max_bounds.width
		|| nfs->max_bounds.width == bfs->max_bounds.width + 1));
}

/*
 * Check if the font looks like it has fixed width
 */
static int
is_fixed_font(XFontStruct * fs)
{
    if (fs)
	return (fs->min_bounds.width == fs->max_bounds.width);
    return 1;
}

/*
 * Check if the font looks like a double width font (i.e. contains
 * characters of width X and 2X
 */
#if OPT_WIDE_CHARS
static int
is_double_width_font(XFontStruct * fs)
{
    return ((2 * fs->min_bounds.width) == fs->max_bounds.width);
}
#else
#define is_double_width_font(fs) 0
#endif

#if OPT_WIDE_CHARS && defined(XRENDERFONT) && defined(HAVE_TYPE_FCCHAR32)
#define HALF_WIDTH_TEST_STRING "1234567890"

/* '1234567890' in Chinese characters in UTF-8 */
#define FULL_WIDTH_TEST_STRING "\xe4\xb8\x80\xe4\xba\x8c\xe4\xb8\x89" \
                               "\xe5\x9b\x9b\xe4\xba\x94" \
			       "\xef\xa7\x91\xe4\xb8\x83\xe5\x85\xab" \
			       "\xe4\xb9\x9d\xef\xa6\xb2"

/* '1234567890' in Korean script in UTF-8 */
#define FULL_WIDTH_TEST_STRING2 "\xec\x9d\xbc\xec\x9d\xb4\xec\x82\xbc" \
                                "\xec\x82\xac\xec\x98\xa4" \
			        "\xec\x9c\xa1\xec\xb9\xa0\xed\x8c\x94" \
			        "\xea\xb5\xac\xec\x98\x81"

#define HALF_WIDTH_CHAR1  0x0031	/* 'l' */
#define HALF_WIDTH_CHAR2  0x0057	/* 'W' */
#define FULL_WIDTH_CHAR1  0x4E00	/* CJK Ideograph 'number one' */
#define FULL_WIDTH_CHAR2  0xAC00	/* Korean script syllable 'Ka' */

static int
is_double_width_font_xft(Display * dpy, XftFont * font)
{
    XGlyphInfo gi1, gi2;
    FcChar32 c1 = HALF_WIDTH_CHAR1, c2 = HALF_WIDTH_CHAR2;
    char *fwstr = FULL_WIDTH_TEST_STRING;
    char *hwstr = HALF_WIDTH_TEST_STRING;

    /* Some Korean fonts don't have Chinese characters at all. */
    if (!XftCharExists(dpy, font, FULL_WIDTH_CHAR1)) {
	if (!XftCharExists(dpy, font, FULL_WIDTH_CHAR2))
	    return 0;		/* Not a CJK font */
	else			/* a Korean font without CJK Ideographs */
	    fwstr = FULL_WIDTH_TEST_STRING2;
    }

    XftTextExtents32(dpy, font, &c1, 1, &gi1);
    XftTextExtents32(dpy, font, &c2, 1, &gi2);
    if (gi1.xOff != gi2.xOff)	/* Not a fixed-width font */
	return 0;

    XftTextExtentsUtf8(dpy, font, (FcChar8 *) hwstr, strlen(hwstr), &gi1);
    XftTextExtentsUtf8(dpy, font, (FcChar8 *) fwstr, strlen(fwstr), &gi2);

    /*
     * fontconfig and Xft prior to 2.2(?) set the width of half-width
     * characters identical to that of full-width character in CJK double-width
     * (bi-width / monospace) font even though the former is half as wide as
     * the latter.  This was fixed sometime before the release of fontconfig
     * 2.2 in early 2003.  See
     *  http://bugzilla.mozilla.org/show_bug.cgi?id=196312
     * In the meantime, we have to check both possibilities.
     */
    return ((2 * gi1.xOff == gi2.xOff) || (gi1.xOff == gi2.xOff));
}
#else
#define is_double_width_font_xft(dpy, xftfont) 0
#endif

#define EmptyFont(fs) (fs != 0 \
		   && ((fs)->ascent + (fs)->descent == 0 \
		    || (fs)->max_bounds.width == 0))

#define FontSize(fs) (((fs)->ascent + (fs)->descent) \
		    *  (fs)->max_bounds.width)

const VTFontNames *
xtermFontName(char *normal)
{
    static VTFontNames data;
    memset(&data, 0, sizeof(data));
    data.f_n = normal;
    return &data;
}

int
xtermLoadFont(TScreen * screen,
	      const VTFontNames * fonts,
	      Bool doresize,
	      int fontnum)
{
    VTFontNames myfonts;
    /* FIXME: use XFreeFontInfo */
    FontNameProperties *fp;
    XFontStruct *nfs = NULL;
    XFontStruct *bfs = NULL;
#if OPT_WIDE_CHARS
    XFontStruct *wfs = NULL;
    XFontStruct *wbfs = NULL;
#endif
    XGCValues xgcv;
    unsigned long mask;
    GC new_normalGC = NULL;
    GC new_normalboldGC = NULL;
    GC new_reverseGC = NULL;
    GC new_reverseboldGC = NULL;
    Pixel new_normal;
    Pixel new_revers;
    char *tmpname = NULL;
    char normal[MAX_FONTNAME];
    Boolean proportional = False;
    int ch;

    memset(&myfonts, 0, sizeof(myfonts));
    if (fonts != 0)
	myfonts = *fonts;
    if (myfonts.f_n == 0)
	return 0;

    if (fontnum == fontMenu_fontescape
	&& myfonts.f_n != screen->menu_font_names[fontnum]) {
	if ((tmpname = x_strdup(myfonts.f_n)) == 0)
	    return 0;
    }

    TRACE(("xtermLoadFont normal %s\n", myfonts.f_n));

    if (!(nfs = XLoadQueryFont(screen->display, myfonts.f_n)))
	goto bad;
    if (EmptyFont(nfs))
	goto bad;		/* can't use a 0-sized font */

    strcpy(normal, myfonts.f_n);
    if (myfonts.f_b == 0) {
	fp = get_font_name_props(screen->display, nfs, normal);
	if (fp != 0) {
	    myfonts.f_b = bold_font_name(fp, True);
	    if ((bfs = XLoadQueryFont(screen->display, myfonts.f_b)) == 0) {
		myfonts.f_b = bold_font_name(fp, False);
		bfs = XLoadQueryFont(screen->display, myfonts.f_b);
	    }
	    TRACE(("...derived bold %s\n", myfonts.f_b));
	}
	if (fp == 0 || bfs == 0) {
	    bfs = nfs;
	    TRACE(("...cannot load a matching bold font\n"));
	} else if (same_font_size(nfs, bfs)
		   && got_bold_font(screen->display, bfs, myfonts.f_b)) {
	    TRACE(("...got a matching bold font\n"));
	} else {
	    XFreeFont(screen->display, bfs);
	    bfs = nfs;
	    TRACE(("...did not get a matching bold font\n"));
	}
    } else if ((bfs = XLoadQueryFont(screen->display, myfonts.f_b)) == 0) {
	bfs = nfs;
	TRACE(("...cannot load bold font %s\n", myfonts.f_b));
    }

    /*
     * If there is no widefont specified, fake it by doubling AVERAGE_WIDTH
     * of normal fonts XLFD, and asking for it.  This plucks out 18x18ja
     * and 12x13ja as the corresponding fonts for 9x18 and 6x13.
     */
    if_OPT_WIDE_CHARS(screen, {
	if (myfonts.f_w == 0 && !is_double_width_font(nfs)) {
	    fp = get_font_name_props(screen->display, nfs, normal);
	    if (fp != 0) {
		myfonts.f_w = wide_font_name(fp);
		TRACE(("...derived wide %s\n", myfonts.f_w));
	    }
	}

	if (myfonts.f_w) {
	    wfs = XLoadQueryFont(screen->display, myfonts.f_w);
	} else {
	    wfs = nfs;
	}

	if (myfonts.f_wb) {
	    wbfs = XLoadQueryFont(screen->display, myfonts.f_wb);
	} else if (is_double_width_font(bfs)) {
	    wbfs = bfs;
	} else {
	    wbfs = wfs;
	    TRACE(("...cannot load wide bold font %s\n", myfonts.f_wb));
	}

	if (EmptyFont(wbfs))
	    goto bad;		/* can't use a 0-sized font */
    });

    /*
     * Most of the time this call to load the font will succeed, even if
     * there is no wide font :  the X server doubles the width of the
     * normal font, or similar.
     *
     * But if it did fail for some reason, then nevermind.
     */
    if (EmptyFont(bfs))
	goto bad;		/* can't use a 0-sized font */

    if (!same_font_size(nfs, bfs)
	&& (is_fixed_font(nfs) && is_fixed_font(bfs))) {
	XFreeFont(screen->display, bfs);
	bfs = nfs;
	TRACE(("...fixing mismatched normal/bold fonts\n"));
	/*
	 * If we're given a nonnull bold fontname here, it came from a
	 * resource setting.  Perhaps the user did something like set
	 * the "*font" in a resource file.  But they would be startled
	 * to see a mismatched bold font.  Try again, asking the font
	 * server for the appropriate font.
	 */
	if (myfonts.f_b != 0) {
	    myfonts.f_b = 0;	/* throw if away! */
	    return xtermLoadFont(screen,
				 &myfonts,
				 doresize,
				 fontnum);
	}
    }

    if_OPT_WIDE_CHARS(screen, {
	if (wfs != 0
	    && wbfs != 0
	    && !same_font_size(wfs, wbfs)
	    && (is_fixed_font(wfs) && is_fixed_font(wbfs))) {
	    XFreeFont(screen->display, wbfs);
	    wbfs = wfs;
	    TRACE(("...fixing mismatched normal/bold wide fonts\n"));
	    if (myfonts.f_wb != 0) {
		myfonts.f_wb = 0;
		return xtermLoadFont(screen,
				     &myfonts,
				     doresize,
				     fontnum);
	    }
	}
    });

    /*
     * Normal/bold fonts should be the same width.  Also, the min/max
     * values should be the same.
     */
    if (!is_fixed_font(nfs)
	|| !is_fixed_font(bfs)
	|| nfs->max_bounds.width != bfs->max_bounds.width) {
	TRACE(("Proportional font! normal %d/%d, bold %d/%d\n",
	       nfs->min_bounds.width,
	       nfs->max_bounds.width,
	       bfs->min_bounds.width,
	       bfs->max_bounds.width));
	proportional = True;
    }

    if_OPT_WIDE_CHARS(screen, {
	if (wfs != 0
	    && wbfs != 0
	    && (!is_fixed_font(wfs)
		|| !is_fixed_font(wbfs)
		|| wfs->max_bounds.width != wbfs->max_bounds.width)) {
	    TRACE(("Proportional font! wide %d/%d, wide bold %d/%d\n",
		   wfs->min_bounds.width,
		   wfs->max_bounds.width,
		   wbfs->min_bounds.width,
		   wbfs->max_bounds.width));
	    proportional = True;
	}
    });

    /* TODO : enforce that the width of the wide font is 2* the width
       of the narrow font */

    mask = (GCFont | GCForeground | GCBackground | GCGraphicsExposures |
	    GCFunction);

    new_normal = getXtermForeground(term->flags, term->cur_foreground);
    new_revers = getXtermBackground(term->flags, term->cur_background);

    xgcv.font = nfs->fid;
    xgcv.foreground = new_normal;
    xgcv.background = new_revers;
    xgcv.graphics_exposures = TRUE;	/* default */
    xgcv.function = GXcopy;

    new_normalGC = XtGetGC((Widget) term, mask, &xgcv);
    if (!new_normalGC)
	goto bad;

    if (nfs == bfs) {		/* there is no bold font */
	new_normalboldGC = new_normalGC;
    } else {
	xgcv.font = bfs->fid;
	new_normalboldGC = XtGetGC((Widget) term, mask, &xgcv);
	if (!new_normalboldGC)
	    goto bad;
    }

    xgcv.font = nfs->fid;
    xgcv.foreground = new_revers;
    xgcv.background = new_normal;
    new_reverseGC = XtGetGC((Widget) term, mask, &xgcv);
    if (!new_reverseGC)
	goto bad;

    if (nfs == bfs) {		/* there is no bold font */
	new_reverseboldGC = new_reverseGC;
    } else {
	xgcv.font = bfs->fid;
	new_reverseboldGC = XtGetGC((Widget) term, mask, &xgcv);
	if (!new_reverseboldGC)
	    goto bad;
    }

    if (NormalGC(screen) != NormalBoldGC(screen))
	XtReleaseGC((Widget) term, NormalBoldGC(screen));
    XtReleaseGC((Widget) term, NormalGC(screen));

    if (ReverseGC(screen) != ReverseBoldGC(screen))
	XtReleaseGC((Widget) term, ReverseBoldGC(screen));
    XtReleaseGC((Widget) term, ReverseGC(screen));

    NormalGC(screen) = new_normalGC;
    NormalBoldGC(screen) = new_normalboldGC;
    ReverseGC(screen) = new_reverseGC;
    ReverseBoldGC(screen) = new_reverseboldGC;

    /*
     * If we're switching fonts, free the old ones.  Otherwise we'll leak
     * the memory that is associated with the old fonts.  The
     * XLoadQueryFont call allocates a new XFontStruct.
     */
    if (screen->fnt_bold != 0
	&& screen->fnt_bold != screen->fnt_norm)
	XFreeFont(screen->display, screen->fnt_bold);
    if (screen->fnt_norm != 0)
	XFreeFont(screen->display, screen->fnt_norm);

    screen->fnt_norm = nfs;
    screen->fnt_bold = bfs;
#if OPT_WIDE_CHARS
    screen->fnt_dwd = wfs;
    if (wbfs == NULL)
	wbfs = wfs;
    screen->fnt_dwdb = wbfs;
#endif
    screen->fnt_prop = proportional;
    screen->fnt_boxes = True;

#if OPT_BOX_CHARS
    /*
     * Xterm uses character positions 1-31 of a font for the line-drawing
     * characters.  Check that they are all present.  The null character
     * (0) is special, and is not used.
     */
#ifdef XRENDERFONT
    if (screen->renderFont != 0) {
	/*
	 * FIXME: we shouldn't even be here if we're using Xft.
	 */
	screen->fnt_boxes = False;
    } else
#endif
    {
	for (ch = 1; ch < 32; ch++) {
	    int n = ch;
#if OPT_WIDE_CHARS
	    if (screen->utf8_mode) {
		n = dec2ucs(ch);
		if (n == UCS_REPL)
		    continue;
	    }
#endif
	    if (xtermMissingChar(n, nfs)
		|| xtermMissingChar(n, bfs)) {
		screen->fnt_boxes = False;
		break;
	    }
	}
    }
    TRACE(("Will %suse internal line-drawing characters\n",
	   screen->fnt_boxes ? "not " : ""));
#endif

    screen->enbolden = screen->bold_mode
	&& ((nfs == bfs) || same_font_name(normal, myfonts.f_b));
    TRACE(("Will %suse 1-pixel offset/overstrike to simulate bold\n",
	   screen->enbolden ? "" : "not "));

    set_menu_font(False);
    screen->menu_font_number = fontnum;
    set_menu_font(True);
    if (tmpname) {		/* if setting escape or sel */
	if (screen->menu_font_names[fontnum])
	    free(screen->menu_font_names[fontnum]);
	screen->menu_font_names[fontnum] = tmpname;
	if (fontnum == fontMenu_fontescape) {
	    set_sensitivity(term->screen.fontMenu,
			    fontMenuEntries[fontMenu_fontescape].widget,
			    TRUE);
	}
#if OPT_SHIFT_FONTS
	screen->menu_font_sizes[fontnum] = FontSize(nfs);
#endif
    }
    set_cursor_gcs(screen);
    xtermUpdateFontInfo(screen, doresize);
    return 1;

  bad:
    if (tmpname)
	free(tmpname);
    if (new_normalGC)
	XtReleaseGC((Widget) term, new_normalGC);
    if (new_normalboldGC && new_normalGC != new_normalboldGC)
	XtReleaseGC((Widget) term, new_normalboldGC);
    if (new_reverseGC)
	XtReleaseGC((Widget) term, new_reverseGC);
    if (new_reverseboldGC && new_reverseGC != new_reverseboldGC)
	XtReleaseGC((Widget) term, new_reverseboldGC);
    if (nfs)
	XFreeFont(screen->display, nfs);
    if (bfs && nfs != bfs)
	XFreeFont(screen->display, bfs);
#if OPT_WIDE_CHARS
    if (wfs)
	XFreeFont(screen->display, wfs);
    if (wbfs && wbfs != wfs)
	XFreeFont(screen->display, wbfs);
#endif
    return 0;
}

#if OPT_LOAD_VTFONTS || OPT_WIDE_CHARS
/*
 * Collect font-names that we can modify with the load-vt-fonts() action.
 */
typedef struct {
    VTFontNames default_font;
    char *menu_font_names[fontMenu_lastBuiltin + 1];
} SubResourceRec;

#define MERGE_SUBFONT(src,dst,name) if (dst.name == 0) dst.name = src.name

#define COPY_MENU_FONTS(src,dst) \
	for (n = fontMenu_font1; n <= fontMenu_lastBuiltin; ++n) \
	    dst.menu_font_names[n] = src.menu_font_names[n]

/*
 * Load the "VT" font names from the given subresource name/class.  These
 * correspond to the VT100 resources.
 */
Bool
xtermLoadVTFonts(XtermWidget w, char *myName, char *myClass)
{
    static Boolean initialized = False;
    static SubResourceRec original, referenceRec, subresourceRec;

    /*
     * These are duplicates of the VT100 font resources, but with a special
     * application/classname passed in to distinguish them.
     */
    static XtResource font_resources[] =
    {
	Sres(XtNfont, XtCFont, default_font.f_n, DEFFONT),
	Sres(XtNboldFont, XtCBoldFont, default_font.f_b, DEFBOLDFONT),
#if OPT_WIDE_CHARS
	Sres(XtNwideFont, XtCWideFont, default_font.f_w, DEFWIDEFONT),
	Sres(XtNwideBoldFont, XtCWideBoldFont, default_font.f_wb, DEFWIDEBOLDFONT),
#endif
	Sres(XtNfont1, XtCFont1, menu_font_names[fontMenu_font1], NULL),
	Sres(XtNfont2, XtCFont2, menu_font_names[fontMenu_font2], NULL),
	Sres(XtNfont3, XtCFont3, menu_font_names[fontMenu_font3], NULL),
	Sres(XtNfont4, XtCFont4, menu_font_names[fontMenu_font4], NULL),
	Sres(XtNfont5, XtCFont5, menu_font_names[fontMenu_font5], NULL),
	Sres(XtNfont6, XtCFont6, menu_font_names[fontMenu_font6], NULL),
    };
    Cardinal n;
    Boolean status = True;

    if (!initialized) {

	initialized = True;
	TRACE(("xtermLoadVTFonts saving original\n"));
	original.default_font = w->misc.default_font;
	COPY_MENU_FONTS(w->screen, original);
    }

    if (myName == 0 || *myName == 0) {
	TRACE(("xtermLoadVTFonts restoring original\n"));
	w->misc.default_font = original.default_font;
	COPY_MENU_FONTS(original, w->screen);
	for (n = 0; n < XtNumber(original.menu_font_names); ++n)
	    w->screen.menu_font_names[n] = original.menu_font_names[n];
    } else {
	TRACE(("xtermLoadVTFonts(%s, %s)\n", myName, myClass));

	memset(&subresourceRec, 0, sizeof(subresourceRec));
	XtGetSubresources((Widget) w, (XtPointer) & subresourceRec,
			  myName, myClass,
			  font_resources,
			  (Cardinal) XtNumber(font_resources),
			  NULL, (Cardinal) 0);
	if (memcmp(&referenceRec, &subresourceRec, sizeof(referenceRec))) {

	    /*
	     * If a particular resource value was not found, use the original.
	     */
	    MERGE_SUBFONT(w->misc, subresourceRec, default_font.f_n);
	    MERGE_SUBFONT(w->misc, subresourceRec, default_font.f_b);
#if OPT_WIDE_CHARS
	    MERGE_SUBFONT(w->misc, subresourceRec, default_font.f_w);
	    MERGE_SUBFONT(w->misc, subresourceRec, default_font.f_wb);
#endif
	    for (n = fontMenu_font1; n <= fontMenu_lastBuiltin; ++n)
		MERGE_SUBFONT(w->screen, subresourceRec, menu_font_names[n]);

	    /*
	     * Finally, copy the subresource data to the widget.
	     */
	    w->misc.default_font = subresourceRec.default_font;
	    COPY_MENU_FONTS(subresourceRec, w->screen);
	    w->screen.menu_font_names[fontMenu_fontdefault] = w->misc.default_font.f_n;
	} else {
	    TRACE(("...no resources found\n"));
	    status = False;
	}
    }
    return status;
}
#endif /* OPT_LOAD_VTFONTS || OPT_WIDE_CHARS */

#if OPT_LOAD_VTFONTS
void
HandleLoadVTFonts(Widget w GCC_UNUSED,
		  XEvent * event GCC_UNUSED,
		  String * params GCC_UNUSED,
		  Cardinal * param_count GCC_UNUSED)
{
    char buf[80];
    char *myName = (*param_count > 0) ? params[0] : "";
    char *convert = (*param_count > 1) ? params[1] : myName;
    char *myClass = (char *) MyStackAlloc(strlen(convert), buf);
    int n;

    TRACE(("HandleLoadVTFonts(%d)\n", *param_count));
    strcpy(myClass, convert);
    if (*param_count == 1
	&& islower(CharOf(myClass[0])))
	myClass[0] = toupper(CharOf(myClass[0]));

    if (xtermLoadVTFonts(term, myName, myClass)) {
	/*
	 * When switching fonts, try to preserve the font-menu selection, since
	 * it is less surprising to do that (if the font-switching can be
	 * undone) than to switch to "Default".
	 */
	int font_number = term->screen.menu_font_number;
	if (font_number > fontMenu_lastBuiltin)
	    font_number = fontMenu_lastBuiltin;
	for (n = 0; n < NMENUFONTS; ++n)
	    term->screen.menu_font_sizes[n] = 0;
	SetVTFont(font_number, TRUE,
		  ((font_number == fontMenu_fontdefault)
		   ? &(term->misc.default_font)
		   : NULL));
    }

    MyStackFree(myClass, buf);
}
#endif /* OPT_LOAD_VTFONTS */

/*
 * Set the limits for the box that outlines the cursor.
 */
void
xtermSetCursorBox(TScreen * screen)
{
    static XPoint VTbox[NBOX];
    XPoint *vp;

    vp = &VTbox[1];
    (vp++)->x = FontWidth(screen) - 1;
    (vp++)->y = FontHeight(screen) - 1;
    (vp++)->x = -(FontWidth(screen) - 1);
    vp->y = -(FontHeight(screen) - 1);
    screen->box = VTbox;
}

/*
 * Compute useful values for the font/window sizes
 */
void
xtermComputeFontInfo(TScreen * screen,
		     struct _vtwin *win,
		     XFontStruct * font,
		     int sbwidth)
{
    int i, j, width, height;

#ifdef XRENDERFONT
    Display *dpy = screen->display;
    if (!screen->renderFont && term->misc.face_name) {
	XftPattern *pat, *match;
	XftResult result;

	pat = XftNameParse(term->misc.face_name);
	XftPatternBuild(pat,
			XFT_FAMILY, XftTypeString, "mono",
			XFT_SIZE, XftTypeInteger, term->misc.face_size,
			XFT_SPACING, XftTypeInteger, XFT_MONO,
			(void *) 0);
	match = XftFontMatch(dpy, DefaultScreen(dpy), pat, &result);
	screen->renderFont = XftFontOpenPattern(dpy, match);
	if (!screen->renderFont && match)
	    XftPatternDestroy(match);
	if (screen->renderFont) {
	    XftPatternBuild(pat,
			    XFT_WEIGHT, XftTypeInteger, XFT_WEIGHT_BOLD,
			    XFT_CHAR_WIDTH, XftTypeInteger, screen->renderFont->max_advance_width,
			    (void *) 0);
	    match = XftFontMatch(dpy, DefaultScreen(dpy), pat, &result);
	    screen->renderFontBold = XftFontOpenPattern(dpy, match);
	    if (!screen->renderFontBold && match)
		XftPatternDestroy(match);

	    /*
	     * FIXME:  just assume that the corresponding font has no graphics
	     * characters.
	     */
	    if (screen->fnt_boxes) {
		screen->fnt_boxes = False;
		TRACE(("Xft opened - will %suse internal line-drawing characters\n",
		       screen->fnt_boxes ? "not " : ""));
	    }
	}
	if (pat)
	    XftPatternDestroy(pat);
    }
    if (screen->renderFont) {
	win->f_width = screen->renderFont->max_advance_width;
	win->f_height = screen->renderFont->height;
	win->f_ascent = screen->renderFont->ascent;
	win->f_descent = screen->renderFont->descent;
	if (win->f_height < win->f_ascent + win->f_descent)
	    win->f_height = win->f_ascent + win->f_descent;
	if (is_double_width_font_xft(screen->display, screen->renderFont))
	    win->f_width >>= 1;
    } else
#endif
    {
	if (is_double_width_font(font)) {
	    win->f_width = (font->min_bounds.width);
	} else {
	    win->f_width = (font->max_bounds.width);
	}
	win->f_height = (font->ascent + font->descent);
	win->f_ascent = font->ascent;
	win->f_descent = font->descent;
    }
    i = 2 * screen->border + sbwidth;
    j = 2 * screen->border;
    width = (screen->max_col + 1) * win->f_width + i;
    height = (screen->max_row + 1) * win->f_height + j;
    win->fullwidth = width;
    win->fullheight = height;
    win->width = width - i;
    win->height = height - j;
}

/* save this information as a side-effect for double-sized characters */
void
xtermSaveFontInfo(TScreen * screen, XFontStruct * font)
{
    screen->fnt_wide = (font->max_bounds.width);
    screen->fnt_high = (font->ascent + font->descent);
}

/*
 * After loading a new font, update the structures that use its size.
 */
void
xtermUpdateFontInfo(TScreen * screen, Bool doresize)
{
    int scrollbar_width;
    struct _vtwin *win = &(screen->fullVwin);

    scrollbar_width = (term->misc.scrollbar
		       ? screen->scrollWidget->core.width +
		       screen->scrollWidget->core.border_width
		       : 0);
    xtermComputeFontInfo(screen, win, screen->fnt_norm, scrollbar_width);
    xtermSaveFontInfo(screen, screen->fnt_norm);

    if (doresize) {
	if (VWindow(screen)) {
	    XClearWindow(screen->display, VWindow(screen));
	}
	DoResizeScreen(term);	/* set to the new natural size */
	if (screen->scrollWidget)
	    ResizeScrollBar(screen);
	Redraw();
    }
    xtermSetCursorBox(screen);
}

#if OPT_BOX_CHARS

/*
 * Returns true if the given character is missing from the specified font.
 */
Bool
xtermMissingChar(unsigned ch, XFontStruct * font)
{
    if (font != 0
	&& font->per_char != 0
	&& !font->all_chars_exist) {
	static XCharStruct dft, *tmp = &dft, *pc = 0;

	if (font->max_byte1 == 0) {
#if OPT_WIDE_CHARS
	    if (ch > 255) {
		TRACE(("xtermMissingChar %#04x (row)\n", ch));
		return True;
	    }
#endif
	    CI_GET_CHAR_INFO_1D(font, E2A(ch), tmp, pc);
	}
#if OPT_WIDE_CHARS
	else {
	    CI_GET_CHAR_INFO_2D(font, (ch >> 8), (ch & 0xff), tmp, pc);
	}
#else

	if (!pc)
	    return False;	/* Urgh! */
#endif

	if (CI_NONEXISTCHAR(pc)) {
	    TRACE(("xtermMissingChar %#04x (!exists)\n", ch));
	    return True;
	}
    }
    if (ch < 32
	&& term->screen.force_box_chars) {
	TRACE(("xtermMissingChar %#04x (forced off)\n", ch));
	return True;
    }
    return False;
}

/*
 * The grid is abitrary, enough resolution that nothing's lost in initialization.
 */
#define BOX_HIGH 60
#define BOX_WIDE 60

#define MID_HIGH (BOX_HIGH/2)
#define MID_WIDE (BOX_WIDE/2)

/*
 * ...since we'll scale the values anyway.
 */
#define SCALE_X(n) n = (n * (font_width-1)) / (BOX_WIDE-1)
#define SCALE_Y(n) n = (n * (font_height-1)) / (BOX_HIGH-1)

#define SEG(x0,y0,x1,y1) x0,y0, x1,y1

/*
 * Draw the given graphic character, if it is simple enough (i.e., a
 * line-drawing character).
 */
void
xtermDrawBoxChar(TScreen * screen, int ch, unsigned flags, GC gc, int x, int y)
{
    /* *INDENT-OFF* */
    static const short diamond[] =
    {
	SEG(  MID_WIDE,	    BOX_HIGH/4, 3*BOX_WIDE/4,   MID_WIDE),
	SEG(3*BOX_WIDE/4,   MID_WIDE,	  MID_WIDE,   3*BOX_HIGH/4),
	SEG(  MID_WIDE,   3*BOX_HIGH/4,	  BOX_WIDE/4,   MID_HIGH),
	SEG(  BOX_WIDE/4,   MID_HIGH,	  MID_WIDE,	BOX_HIGH/4),
	SEG(  MID_WIDE,	    BOX_HIGH/3, 2*BOX_WIDE/3,   MID_WIDE),
	SEG(2*BOX_WIDE/3,   MID_WIDE,	  MID_WIDE,   2*BOX_HIGH/3),
	SEG(  MID_WIDE,   2*BOX_HIGH/3,	  BOX_WIDE/3,   MID_HIGH),
	SEG(  BOX_WIDE/3,   MID_HIGH,	  MID_WIDE,	BOX_HIGH/3),
	SEG(  BOX_WIDE/4,   MID_HIGH,	3*BOX_WIDE/4,   MID_HIGH),
	SEG(  MID_WIDE,     BOX_HIGH/4,	  MID_WIDE,   3*BOX_HIGH/4),
	-1
    }, degrees[] =
    {
	SEG(  MID_WIDE,	    BOX_HIGH/4, 2*BOX_WIDE/3, 3*BOX_HIGH/8),
	SEG(2*BOX_WIDE/3, 3*BOX_HIGH/8,	  MID_WIDE,	MID_HIGH),
	SEG(  MID_WIDE,	    MID_HIGH,	  BOX_WIDE/3, 3*BOX_HIGH/8),
	SEG(  BOX_WIDE/3, 3*BOX_HIGH/8,	  MID_WIDE,	BOX_HIGH/4),
	-1
    }, lower_right_corner[] =
    {
	SEG(  0,	    MID_HIGH,	  MID_WIDE,	MID_HIGH),
	SEG(  MID_WIDE,	    MID_HIGH,	  MID_WIDE,	0),
	-1
    }, upper_right_corner[] =
    {
	SEG(  0,	    MID_HIGH,	  MID_WIDE,	MID_HIGH),
	SEG( MID_WIDE,	    MID_HIGH,	  MID_WIDE,	BOX_HIGH),
	-1
    }, upper_left_corner[] =
    {
	SEG(  MID_WIDE,	    MID_HIGH,	  BOX_WIDE,	MID_HIGH),
	SEG(  MID_WIDE,	    MID_HIGH,	  MID_WIDE,	BOX_HIGH),
	-1
    }, lower_left_corner[] =
    {
	SEG(  MID_WIDE,	    0,		  MID_WIDE,	MID_HIGH),
	SEG(  MID_WIDE,	    MID_WIDE,	  BOX_WIDE,	MID_HIGH),
	-1
    }, cross[] =
    {
	SEG(  0,	    MID_HIGH,	  BOX_WIDE,	MID_HIGH),
	SEG(  MID_WIDE,	    0,		  MID_WIDE,	BOX_HIGH),
	-1
    }, scan_line_1[] =
    {
	SEG(  0,	    0,		  BOX_WIDE,	0),
	-1
    }, scan_line_3[] =
    {
	SEG(  0,	    BOX_HIGH/4,	  BOX_WIDE,	BOX_HIGH/4),
	-1
    }, scan_line_7[] =
    {
	SEG( 0,		    MID_HIGH,	  BOX_WIDE,	MID_HIGH),
	-1
    }, scan_line_9[] =
    {
	SEG(  0,	    3*BOX_HIGH/4, BOX_WIDE, 3 * BOX_HIGH / 4),
	-1
    }, horizontal_line[] =
    {
	SEG(  0,	    BOX_HIGH,	  BOX_WIDE,	BOX_HIGH),
	-1
    }, left_tee[] =
    {
	SEG(  MID_WIDE,	    0,		  MID_WIDE,	BOX_HIGH),
	SEG(  MID_WIDE,	    MID_HIGH,	  BOX_WIDE,	MID_HIGH),
	-1
    }, right_tee[] =
    {
	SEG(  MID_WIDE,	    0, MID_WIDE,		BOX_HIGH),
	SEG(  MID_WIDE,	    MID_HIGH,	  0,		MID_HIGH),
	-1
    }, bottom_tee[] =
    {
	SEG(  0,	    MID_HIGH,	  BOX_WIDE,	MID_HIGH),
	SEG(  MID_WIDE,	    0,		  MID_WIDE,	MID_HIGH),
	-1
    }, top_tee[] =
    {
	SEG(  0,	    MID_HIGH,	  BOX_WIDE,	MID_HIGH),
	SEG(  MID_WIDE,	    MID_HIGH,	  MID_WIDE,	BOX_HIGH),
	-1
    }, vertical_line[] =
    {
	SEG(  MID_WIDE,	    0,		  MID_WIDE,	BOX_HIGH),
	-1
    }, less_than_or_equal[] =
    {
	SEG(5*BOX_WIDE/6,   BOX_HIGH/6,	  BOX_WIDE/5,	MID_HIGH),
	SEG(5*BOX_WIDE/6, 5*BOX_HIGH/6,	  BOX_WIDE/5,	MID_HIGH),
	SEG(  BOX_WIDE/6, 5*BOX_HIGH/6, 5*BOX_WIDE/6, 5*BOX_HIGH/6),
	-1
    }, greater_than_or_equal[] =
    {
	SEG(  BOX_WIDE/6,  BOX_HIGH /6, 5*BOX_WIDE/6,   MID_HIGH),
	SEG(  BOX_WIDE/6, 5*BOX_HIGH/6, 5*BOX_WIDE/6,   MID_HIGH),
	SEG(  BOX_WIDE/6, 5*BOX_HIGH/6, 5*BOX_WIDE/6, 5*BOX_HIGH/6),
	-1
    };
    /* *INDENT-ON* */

    static const short *lines[] =
    {
	0,			/* 00 */
	diamond,		/* 01 */
	0,			/* 02 */
	0,			/* 03 */
	0,			/* 04 */
	0,			/* 05 */
	0,			/* 06 */
	degrees,		/* 07 */
	0,			/* 08 */
	0,			/* 09 */
	0,			/* 0A */
	lower_right_corner,	/* 0B */
	upper_right_corner,	/* 0C */
	upper_left_corner,	/* 0D */
	lower_left_corner,	/* 0E */
	cross,			/* 0F */
	scan_line_1,		/* 10 */
	scan_line_3,		/* 11 */
	scan_line_7,		/* 12 */
	scan_line_9,		/* 13 */
	horizontal_line,	/* 14 */
	left_tee,		/* 15 */
	right_tee,		/* 16 */
	bottom_tee,		/* 17 */
	top_tee,		/* 18 */
	vertical_line,		/* 19 */
	less_than_or_equal,	/* 1A */
	greater_than_or_equal,	/* 1B */
	0,			/* 1C */
	0,			/* 1D */
	0,			/* 1E */
	0,			/* 1F */
    };

    XGCValues values;
    GC gc2;
    const short *p;
    int font_width = ((flags & DOUBLEWFONT) ? 2 : 1) * screen->fnt_wide;
    int font_height = ((flags & DOUBLEHFONT) ? 2 : 1) * screen->fnt_high;

#if OPT_WIDE_CHARS
    /*
     * Try to show line-drawing characters if we happen to be in UTF-8
     * mode, but have gotten an old-style font.
     */
    if (screen->utf8_mode
#ifdef XRENDERFONT
	&& screen->renderFont == 0
#endif
	&& (ch > 127)
	&& (ch != UCS_REPL)) {
	unsigned n;
	for (n = 1; n < 32; n++) {
	    if (dec2ucs(n) == ch
		&& !xtermMissingChar(n, (flags & BOLD)
				     ? screen->fnt_bold
				     : screen->fnt_norm)) {
		TRACE(("...use xterm-style linedrawing\n"));
		ch = n;
		break;
	    }
	}
    }
#endif

    TRACE(("DRAW_BOX(%d) cell %dx%d at %d,%d%s\n",
	   ch, font_height, font_width, y, x,
	   ((ch < 0 || ch >= (int) (sizeof(lines) / sizeof(lines[0])))
	    ? "-BAD"
	    : "")));

    if (!XGetGCValues(screen->display, gc, GCBackground, &values))
	return;

    values.foreground = values.background;
    gc2 = XCreateGC(screen->display, VWindow(screen), GCForeground, &values);

    if (!(flags & NOBACKGROUND))
	XFillRectangle(
			  screen->display, VWindow(screen), gc2, x, y,
			  font_width,
			  font_height);

    XCopyGC(screen->display, gc, (1 << GCLastBit) - 1, gc2);
    XSetLineAttributes(screen->display, gc2,
		       (flags & BOLD)
		       ? ((font_height > 6)
			  ? font_height / 6
			  : 1)
		       : ((font_height > 8)
			  ? font_height / 8
			  : 1),
		       LineSolid,
		       CapProjecting,
		       JoinMiter);

    if (ch >= 0
	&& ch < (int) (sizeof(lines) / sizeof(lines[0]))
	&& (p = lines[ch]) != 0) {
	int coord[4];
	int n = 0;
	while (*p >= 0) {
	    coord[n++] = *p++;
	    if (n == 4) {
		SCALE_X(coord[0]);
		SCALE_Y(coord[1]);
		SCALE_X(coord[2]);
		SCALE_Y(coord[3]);
		XDrawLine(
			     screen->display,
			     VWindow(screen), gc2,
			     x + coord[0], y + coord[1],
			     x + coord[2], y + coord[3]);
		n = 0;
	    }
	}
    }
#if 0				/* bounding rectangle, for debugging */
    else {
	XDrawRectangle(
			  screen->display, VWindow(screen), gc, x, y,
			  font_width - 1,
			  font_height - 1);
    }
#endif

    XFreeGC(screen->display, gc2);
}
#endif

#if OPT_WIDE_CHARS
#define MY_UCS(ucs,dec) case ucs: result = dec; break
int
ucs2dec(int ch)
{
    int result = ch;
    if ((ch > 127)
	&& (ch != UCS_REPL)) {
	switch (ch) {
	    MY_UCS(0x25ae, 0);	/* black vertical rectangle                   */
	    MY_UCS(0x25c6, 1);	/* black diamond                              */
	    MY_UCS(0x2592, 2);	/* medium shade                               */
	    MY_UCS(0x2409, 3);	/* symbol for horizontal tabulation           */
	    MY_UCS(0x240c, 4);	/* symbol for form feed                       */
	    MY_UCS(0x240d, 5);	/* symbol for carriage return                 */
	    MY_UCS(0x240a, 6);	/* symbol for line feed                       */
	    MY_UCS(0x00b0, 7);	/* degree sign                                */
	    MY_UCS(0x00b1, 8);	/* plus-minus sign                            */
	    MY_UCS(0x2424, 9);	/* symbol for newline                         */
	    MY_UCS(0x240b, 10);	/* symbol for vertical tabulation             */
	    MY_UCS(0x2518, 11);	/* box drawings light up and left             */
	    MY_UCS(0x2510, 12);	/* box drawings light down and left           */
	    MY_UCS(0x250c, 13);	/* box drawings light down and right          */
	    MY_UCS(0x2514, 14);	/* box drawings light up and right            */
	    MY_UCS(0x253c, 15);	/* box drawings light vertical and horizontal */
	    MY_UCS(0x23ba, 16);	/* box drawings scan 1                        */
	    MY_UCS(0x23bb, 17);	/* box drawings scan 3                        */
	    MY_UCS(0x2500, 18);	/* box drawings light horizontal              */
	    MY_UCS(0x23bc, 19);	/* box drawings scan 7                        */
	    MY_UCS(0x23bd, 20);	/* box drawings scan 9                        */
	    MY_UCS(0x251c, 21);	/* box drawings light vertical and right      */
	    MY_UCS(0x2524, 22);	/* box drawings light vertical and left       */
	    MY_UCS(0x2534, 23);	/* box drawings light up and horizontal       */
	    MY_UCS(0x252c, 24);	/* box drawings light down and horizontal     */
	    MY_UCS(0x2502, 25);	/* box drawings light vertical                */
	    MY_UCS(0x2264, 26);	/* less-than or equal to                      */
	    MY_UCS(0x2265, 27);	/* greater-than or equal to                   */
	    MY_UCS(0x03c0, 28);	/* greek small letter pi                      */
	    MY_UCS(0x2260, 29);	/* not equal to                               */
	    MY_UCS(0x00a3, 30);	/* pound sign                                 */
	    MY_UCS(0x00b7, 31);	/* middle dot                                 */
	}
    }
    return result;
}

#undef  MY_UCS
#define MY_UCS(ucs,dec) case dec: result = ucs; break

int
dec2ucs(int ch)
{
    int result = ch;
    if (ch < 32) {
	switch (ch) {
	    MY_UCS(0x25ae, 0);	/* black vertical rectangle                   */
	    MY_UCS(0x25c6, 1);	/* black diamond                              */
	    MY_UCS(0x2592, 2);	/* medium shade                               */
	    MY_UCS(0x2409, 3);	/* symbol for horizontal tabulation           */
	    MY_UCS(0x240c, 4);	/* symbol for form feed                       */
	    MY_UCS(0x240d, 5);	/* symbol for carriage return                 */
	    MY_UCS(0x240a, 6);	/* symbol for line feed                       */
	    MY_UCS(0x00b0, 7);	/* degree sign                                */
	    MY_UCS(0x00b1, 8);	/* plus-minus sign                            */
	    MY_UCS(0x2424, 9);	/* symbol for newline                         */
	    MY_UCS(0x240b, 10);	/* symbol for vertical tabulation             */
	    MY_UCS(0x2518, 11);	/* box drawings light up and left             */
	    MY_UCS(0x2510, 12);	/* box drawings light down and left           */
	    MY_UCS(0x250c, 13);	/* box drawings light down and right          */
	    MY_UCS(0x2514, 14);	/* box drawings light up and right            */
	    MY_UCS(0x253c, 15);	/* box drawings light vertical and horizontal */
	    MY_UCS(0x23ba, 16);	/* box drawings scan 1                        */
	    MY_UCS(0x23bb, 17);	/* box drawings scan 3                        */
	    MY_UCS(0x2500, 18);	/* box drawings light horizontal              */
	    MY_UCS(0x23bc, 19);	/* box drawings scan 7                        */
	    MY_UCS(0x23bd, 20);	/* box drawings scan 9                        */
	    MY_UCS(0x251c, 21);	/* box drawings light vertical and right      */
	    MY_UCS(0x2524, 22);	/* box drawings light vertical and left       */
	    MY_UCS(0x2534, 23);	/* box drawings light up and horizontal       */
	    MY_UCS(0x252c, 24);	/* box drawings light down and horizontal     */
	    MY_UCS(0x2502, 25);	/* box drawings light vertical                */
	    MY_UCS(0x2264, 26);	/* less-than or equal to                      */
	    MY_UCS(0x2265, 27);	/* greater-than or equal to                   */
	    MY_UCS(0x03c0, 28);	/* greek small letter pi                      */
	    MY_UCS(0x2260, 29);	/* not equal to                               */
	    MY_UCS(0x00a3, 30);	/* pound sign                                 */
	    MY_UCS(0x00b7, 31);	/* middle dot                                 */
	}
    }
    return result;
}

#endif /* OPT_WIDE_CHARS */

#if OPT_SHIFT_FONTS
static XFontStruct *
xtermFindFont(TScreen * screen, int fontnum)
{
    XFontStruct *nfs = 0;
    char *name;

    if ((name = screen->menu_font_names[fontnum]) != 0
	&& (nfs = XLoadQueryFont(screen->display, name)) != 0) {
	if (EmptyFont(nfs)) {
	    XFreeFont(screen->display, nfs);
	    nfs = 0;
	}
    }
    return nfs;
}

/*
 * Cache the font-sizes so subsequent larger/smaller font actions will go fast.
 */
static void
lookupFontSizes(TScreen * screen)
{
    int n;

    for (n = 0; n < NMENUFONTS; n++) {
	if (screen->menu_font_sizes[n] == 0) {
	    XFontStruct *fs = xtermFindFont(screen, n);
	    screen->menu_font_sizes[n] = -1;
	    if (fs != 0) {
		screen->menu_font_sizes[n] = FontSize(fs);
		TRACE(("menu_font_sizes[%d] = %ld\n", n,
		       screen->menu_font_sizes[n]));
		XFreeFont(screen->display, fs);
	    }
	}
    }
}

/*
 * Find the index of a larger/smaller font (according to the sign of 'relative'
 * and its magnitude), starting from the 'old' index.
 */
int
lookupRelativeFontSize(TScreen * screen, int old, int relative)
{
    int n, m = -1;

    lookupFontSizes(screen);
    if (relative != 0) {
	for (n = 0; n < NMENUFONTS; ++n) {
	    if (screen->menu_font_sizes[n] > 0 &&
		screen->menu_font_sizes[n] != screen->menu_font_sizes[old]) {
		int cmp_0 = ((screen->menu_font_sizes[n] >
			      screen->menu_font_sizes[old])
			     ? relative
			     : -relative);
		int cmp_m = ((m < 0)
			     ? 1
			     : ((screen->menu_font_sizes[n] <
				 screen->menu_font_sizes[m])
				? relative
				: -relative));
		if (cmp_0 > 0 && cmp_m > 0) {
		    m = n;
		}
	    }
	}
	if (m >= 0) {
	    if (relative > 1)
		m = lookupRelativeFontSize(screen, m, relative - 1);
	    else if (relative < -1)
		m = lookupRelativeFontSize(screen, m, relative + 1);
	}
    }
    return m;
}

/* ARGSUSED */
void
HandleLargerFont(Widget w GCC_UNUSED,
		 XEvent * event GCC_UNUSED,
		 String * params GCC_UNUSED,
		 Cardinal * param_count GCC_UNUSED)
{
    if (term->misc.shift_fonts) {
	TScreen *screen = &term->screen;
	int m;

	m = lookupRelativeFontSize(screen, screen->menu_font_number, 1);
	if (m >= 0) {
	    SetVTFont(m, TRUE, NULL);
	} else {
	    Bell(XkbBI_MinorError, 0);
	}
    }
}

/* ARGSUSED */
void
HandleSmallerFont(Widget w GCC_UNUSED,
		  XEvent * event GCC_UNUSED,
		  String * params GCC_UNUSED,
		  Cardinal * param_count GCC_UNUSED)
{
    if (term->misc.shift_fonts) {
	TScreen *screen = &term->screen;
	int m;

	m = lookupRelativeFontSize(screen, screen->menu_font_number, -1);
	if (m >= 0) {
	    SetVTFont(m, TRUE, NULL);
	} else {
	    Bell(XkbBI_MinorError, 0);
	}
    }
}
#endif

/* ARGSUSED */
void
HandleSetFont(Widget w GCC_UNUSED,
	      XEvent * event GCC_UNUSED,
	      String * params,
	      Cardinal * param_count)
{
    int fontnum;
    VTFontNames fonts;

    memset(&fonts, 0, sizeof(fonts));

    if (*param_count == 0) {
	fontnum = fontMenu_fontdefault;
    } else {
	Cardinal maxparams = 1;	/* total number of params allowed */

	switch (params[0][0]) {
	case 'd':
	case 'D':
	case '0':
	    fontnum = fontMenu_fontdefault;
	    break;
	case '1':
	    fontnum = fontMenu_font1;
	    break;
	case '2':
	    fontnum = fontMenu_font2;
	    break;
	case '3':
	    fontnum = fontMenu_font3;
	    break;
	case '4':
	    fontnum = fontMenu_font4;
	    break;
	case '5':
	    fontnum = fontMenu_font5;
	    break;
	case '6':
	    fontnum = fontMenu_font6;
	    break;
	case 'e':
	case 'E':
	    fontnum = fontMenu_fontescape;
#if OPT_WIDE_CHARS
	    maxparams = 5;
#else
	    maxparams = 3;
#endif
	    break;
	case 's':
	case 'S':
	    fontnum = fontMenu_fontsel;
	    maxparams = 2;
	    break;
	default:
	    Bell(XkbBI_MinorError, 0);
	    return;
	}
	if (*param_count > maxparams) {		/* see if extra args given */
	    Bell(XkbBI_MinorError, 0);
	    return;
	}
	switch (*param_count) {	/* assign 'em */
#if OPT_WIDE_CHARS
	case 5:
	    fonts.f_wb = params[4];
	    /* FALLTHRU */
	case 4:
	    fonts.f_w = params[3];
	    /* FALLTHRU */
#endif
	case 3:
	    fonts.f_b = params[2];
	    /* FALLTHRU */
	case 2:
	    fonts.f_n = params[1];
	    break;
	}
    }

    SetVTFont(fontnum, True, &fonts);
}

void
SetVTFont(int i,
	  Bool doresize,
	  const VTFontNames * fonts)
{
    TScreen *screen = &term->screen;

    TRACE(("SetVTFont(i=%d, f_n=%s, f_b=%s)\n", i,
	   (fonts && fonts->f_n) ? fonts->f_n : "<null>",
	   (fonts && fonts->f_b) ? fonts->f_b : "<null>"));

    if (i >= 0 && i < NMENUFONTS) {
	VTFontNames myfonts;

	memset(&myfonts, 0, sizeof(myfonts));
	if (fonts != 0)
	    myfonts = *fonts;

	if (i == fontMenu_fontsel) {	/* go get the selection */
	    FindFontSelection(myfonts.f_n, False);
	    return;
	} else {
	    if (myfonts.f_n == 0)
		myfonts.f_n = screen->menu_font_names[i];
	    if (xtermLoadFont(screen,
			      &myfonts,
			      doresize, i)) {
		return;
	    }
	}
    }

    Bell(XkbBI_MinorError, 0);
    return;
}

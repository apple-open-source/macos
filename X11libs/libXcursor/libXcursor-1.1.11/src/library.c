/*
 * Copyright © 2002 Keith Packard
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

#include "xcursorint.h"
#include <stdlib.h>
#include <string.h>

#ifndef ICONDIR
#define ICONDIR "/usr/X11R6/lib/X11/icons"
#endif

#ifndef XCURSORPATH
#define XCURSORPATH "~/.icons:/usr/share/icons:/usr/share/pixmaps:"ICONDIR
#endif

const char *
XcursorLibraryPath (void)
{
    static const char	*path;

    if (!path)
    {
	path = getenv ("XCURSOR_PATH");
	if (!path)
	    path = XCURSORPATH;
    }
    return path;
}

static  void
_XcursorAddPathElt (char *path, const char *elt, int len)
{
    int	    pathlen = strlen (path);
    
    /* append / if the path doesn't currently have one */
    if (path[0] == '\0' || path[pathlen - 1] != '/')
    {
	strcat (path, "/");
	pathlen++;
    }
    if (len == -1)
	len = strlen (elt);
    /* strip leading slashes */
    while (len && elt[0] == '/')
    {
	elt++;
	len--;
    }
    strncpy (path + pathlen, elt, len);
    path[pathlen + len] = '\0';
}

static char *
_XcursorBuildThemeDir (const char *dir, const char *theme)
{
    const char	    *colon;
    const char	    *tcolon;
    char	    *full;
    char	    *home;
    int		    dirlen;
    int		    homelen;
    int		    themelen;
    int		    len;

    if (!dir || !theme)
        return NULL;
    
    colon = strchr (dir, ':');
    if (!colon)
	colon = dir + strlen (dir);
    
    dirlen = colon - dir;

    tcolon = strchr (theme, ':');
    if (!tcolon)
	tcolon = theme + strlen (theme);

    themelen = tcolon - theme;
    
    home = NULL;
    homelen = 0;
    if (*dir == '~')
    {
	home = getenv ("HOME");
	if (!home)
	    return NULL;
	homelen = strlen (home);
	dir++;
	dirlen--;
    }

    /*
     * add space for any needed directory separators, one per component,
     * and one for the trailing null
     */
    len = 1 + homelen + 1 + dirlen + 1 + themelen + 1;
    
    full = malloc (len);
    if (!full)
	return NULL;
    full[0] = '\0';

    if (home)
	_XcursorAddPathElt (full, home, -1);
    _XcursorAddPathElt (full, dir, dirlen);
    _XcursorAddPathElt (full, theme, themelen);
    return full;
}

static char *
_XcursorBuildFullname (const char *dir, const char *subdir, const char *file)
{
    char    *full;

    if (!dir || !subdir || !file)
        return NULL;

    full = malloc (strlen (dir) + 1 + strlen (subdir) + 1 + strlen (file) + 1);
    if (!full)
	return NULL;
    full[0] = '\0';
    _XcursorAddPathElt (full, dir, -1);
    _XcursorAddPathElt (full, subdir, -1);
    _XcursorAddPathElt (full, file, -1);
    return full;
}

static const char *
_XcursorNextPath (const char *path)
{
    char    *colon = strchr (path, ':');

    if (!colon)
	return NULL;
    return colon + 1;
}

#define XcursorWhite(c)	((c) == ' ' || (c) == '\t' || (c) == '\n')
#define XcursorSep(c) ((c) == ';' || (c) == ',')

static char *
_XcursorThemeInherits (const char *full)
{
    char    line[8192];
    char    *result = NULL;
    FILE    *f;

    if (!full)
        return NULL;

    f = fopen (full, "r");
    if (f)
    {
	while (fgets (line, sizeof (line), f))
	{
	    if (!strncmp (line, "Inherits", 8))
	    {
		char    *l = line + 8;
		char    *r;
		while (*l == ' ') l++;
		if (*l != '=') continue;
		l++;
		while (*l == ' ') l++;
		result = malloc (strlen (l));
		if (result)
		{
		    r = result;
		    while (*l) 
		    {
			while (XcursorSep(*l) || XcursorWhite (*l)) l++;
			if (!*l)
			    break;
			if (r != result)
			    *r++ = ':';
			while (*l && !XcursorWhite(*l) && 
			       !XcursorSep(*l))
			    *r++ = *l++;
		    }
		    *r++ = '\0';
		}
		break;
	    }
	}
	fclose (f);
    }
    return result;
}

#define XCURSOR_SCAN_CORE   ((FILE *) 1)

static FILE *
XcursorScanTheme (const char *theme, const char *name)
{
    FILE	*f = NULL;
    char	*full;
    char	*dir;
    const char  *path;
    char	*inherits = NULL;
    const char	*i;

    if (!theme || !name)
        return NULL;

    /*
     * XCURSOR_CORE_THEME is a magic name; cursors from the core set
     * are never found in any directory.  Instead, a magic value is
     * returned which truncates any search so that overlying functions
     * can switch to equivalent core cursors
     */
    if (!strcmp (theme, XCURSOR_CORE_THEME) && XcursorLibraryShape (name) >= 0)
	return XCURSOR_SCAN_CORE;
    /*
     * Scan this theme
     */
    for (path = XcursorLibraryPath ();
	 path && f == NULL;
	 path = _XcursorNextPath (path))
    {
	dir = _XcursorBuildThemeDir (path, theme);
	if (dir)
	{
	    full = _XcursorBuildFullname (dir, "cursors", name);
	    if (full)
	    {
		f = fopen (full, "r");
		free (full);
	    }
	    if (!f && !inherits)
	    {
		full = _XcursorBuildFullname (dir, "", "index.theme");
		if (full)
		{
		    inherits = _XcursorThemeInherits (full);
		    free (full);
		}
	    }
	    free (dir);
	}
    }
    /*
     * Recurse to scan inherited themes
     */
    for (i = inherits; i && f == NULL; i = _XcursorNextPath (i))
	f = XcursorScanTheme (i, name);
    if (inherits != NULL)
	free (inherits);
    return f;
}

XcursorImage *
XcursorLibraryLoadImage (const char *file, const char *theme, int size)
{
    FILE	    *f = NULL;
    XcursorImage    *image = NULL;

    if (!file)
        return NULL;

    if (theme)
	f = XcursorScanTheme (theme, file);
    if (!f)
	f = XcursorScanTheme ("default", file);
    if (f == XCURSOR_SCAN_CORE)
	return NULL;
    if (f)
    {
	image = XcursorFileLoadImage (f, size);
	fclose (f);
    }
    return image;
}

XcursorImages *
XcursorLibraryLoadImages (const char *file, const char *theme, int size)
{
    FILE	    *f = NULL;
    XcursorImages   *images = NULL;

    if (!file)
        return NULL;

    if (theme)
	f = XcursorScanTheme (theme, file);
    if (!f)
	f = XcursorScanTheme ("default", file);
    if (f == XCURSOR_SCAN_CORE)
	return NULL;
    if (f)
    {
	images = XcursorFileLoadImages (f, size);
	if (images)
	    XcursorImagesSetName (images, file);
	fclose (f);
    }
    return images;
}

Cursor
XcursorLibraryLoadCursor (Display *dpy, const char *file)
{
    int		    size = XcursorGetDefaultSize (dpy);
    char	    *theme = XcursorGetTheme (dpy);
    XcursorImages   *images = XcursorLibraryLoadImages (file, theme, size);
    Cursor	    cursor;

    if (!file)
        return 0;
    
    if (!images)
    {
	int id = XcursorLibraryShape (file);

	if (id >= 0)
	    return _XcursorCreateFontCursor (dpy, id);
	else
	    return 0;
    }
    cursor = XcursorImagesLoadCursor (dpy, images);
    XcursorImagesDestroy (images);
#if defined HAVE_XFIXES && XFIXES_MAJOR >= 2
    XFixesSetCursorName (dpy, cursor, file);
#endif
    return cursor;
}

XcursorCursors *
XcursorLibraryLoadCursors (Display *dpy, const char *file)
{
    int		    size = XcursorGetDefaultSize (dpy);
    char	    *theme = XcursorGetTheme (dpy);
    XcursorImages   *images = XcursorLibraryLoadImages (file, theme, size);
    XcursorCursors  *cursors;
    
    if (!file)
        return NULL;
    
    if (!images)
    {
	int id = XcursorLibraryShape (file);

	if (id >= 0)
	{
	    cursors = XcursorCursorsCreate (dpy, 1);
	    if (cursors)
	    {
		cursors->cursors[0] = _XcursorCreateFontCursor (dpy, id);
		if (cursors->cursors[0] == None)
		{
		    XcursorCursorsDestroy (cursors);
		    cursors = NULL;
		}
		else
		    cursors->ncursor = 1;
	    }
	}
	else
	    cursors = NULL;
    }
    else
    {
	cursors = XcursorImagesLoadCursors (dpy, images);
	XcursorImagesDestroy (images);
    }
    return cursors;
}

static const char _XcursorStandardNames[] =
	"X_cursor\0"
	"arrow\0"
	"based_arrow_down\0"
	"based_arrow_up\0"
	"boat\0"
	"bogosity\0"
	"bottom_left_corner\0"
	"bottom_right_corner\0"
	"bottom_side\0"
	"bottom_tee\0"
	"box_spiral\0"
	"center_ptr\0"
	"circle\0"
	"clock\0"
	"coffee_mug\0"
	"cross\0"
	"cross_reverse\0"
	"crosshair\0"
	"diamond_cross\0"
	"dot\0"
	"dotbox\0"
	"double_arrow\0"
	"draft_large\0"
	"draft_small\0"
	"draped_box\0"
	"exchange\0"
	"fleur\0"
	"gobbler\0"
	"gumby\0"
	"hand1\0"
	"hand2\0"
	"heart\0"
	"icon\0"
	"iron_cross\0"
	"left_ptr\0"
	"left_side\0"
	"left_tee\0"
	"leftbutton\0"
	"ll_angle\0"
	"lr_angle\0"
	"man\0"
	"middlebutton\0"
	"mouse\0"
	"pencil\0"
	"pirate\0"
	"plus\0"
	"question_arrow\0"
	"right_ptr\0"
	"right_side\0"
	"right_tee\0"
	"rightbutton\0"
	"rtl_logo\0"
	"sailboat\0"
	"sb_down_arrow\0"
	"sb_h_double_arrow\0"
	"sb_left_arrow\0"
	"sb_right_arrow\0"
	"sb_up_arrow\0"
	"sb_v_double_arrow\0"
	"shuttle\0"
	"sizing\0"
	"spider\0"
	"spraycan\0"
	"star\0"
	"target\0"
	"tcross\0"
	"top_left_arrow\0"
	"top_left_corner\0"
	"top_right_corner\0"
	"top_side\0"
	"top_tee\0"
	"trek\0"
	"ul_angle\0"
	"umbrella\0"
	"ur_angle\0"
	"watch\0"
	"xterm";

static const unsigned short _XcursorStandardNameOffsets[] = {
	0, 9, 15, 32, 47, 52, 61, 80, 100, 112, 123, 134, 145, 152, 158,
	169, 175, 189, 199, 213, 217, 224, 237, 249, 261, 272, 281, 287,
	295, 301, 307, 313, 319, 324, 335, 344, 354, 363, 374, 383, 392,
	396, 409, 415, 422, 429, 434, 449, 459, 470, 480, 492, 501, 510,
	524, 542, 556, 571, 583, 601, 609, 616, 623, 632, 637, 644, 651,
	666, 682, 699, 708, 716, 721, 730, 739, 748, 754
};

#define NUM_STANDARD_NAMES  (sizeof _XcursorStandardNameOffsets / sizeof _XcursorStandardNameOffsets[0])

#define STANDARD_NAME(id) \
    _XcursorStandardNames + _XcursorStandardNameOffsets[id]

XcursorImage *
XcursorShapeLoadImage (unsigned int shape, const char *theme, int size)
{
    unsigned int    id = shape >> 1;

    if (id < NUM_STANDARD_NAMES)
	return XcursorLibraryLoadImage (STANDARD_NAME (id), theme, size);
    else
	return NULL;
}

XcursorImages *
XcursorShapeLoadImages (unsigned int shape, const char *theme, int size)
{
    unsigned int    id = shape >> 1;

    if (id < NUM_STANDARD_NAMES)
	return XcursorLibraryLoadImages (STANDARD_NAME (id), theme, size);
    else
	return NULL;
}

Cursor
XcursorShapeLoadCursor (Display *dpy, unsigned int shape)
{
    unsigned int    id = shape >> 1;

    if (id < NUM_STANDARD_NAMES)
	return XcursorLibraryLoadCursor (dpy, STANDARD_NAME (id));
    else
	return 0;
}

XcursorCursors *
XcursorShapeLoadCursors (Display *dpy, unsigned int shape)
{
    unsigned int    id = shape >> 1;

    if (id < NUM_STANDARD_NAMES)
	return XcursorLibraryLoadCursors (dpy, STANDARD_NAME (id));
    else
	return NULL;
}

int
XcursorLibraryShape (const char *library)
{
    int	low, high;
    int	mid;
    int	c;

    low = 0;
    high = NUM_STANDARD_NAMES - 1;
    while (low < high - 1)
    {
	mid = (low + high) >> 1;
	c = strcmp (library, STANDARD_NAME (mid));
	if (c == 0)
	    return (mid << 1);
	if (c > 0)
	    low = mid;
	else
	    high = mid;
    }
    while (low <= high)
    {
	if (!strcmp (library, STANDARD_NAME (low)))
	    return (low << 1);
	low++;
    }
    return -1;
}

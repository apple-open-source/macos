/*
 * $XFree86: xc/lib/Xcursor/library.c,v 1.2 2003/01/26 03:22:42 eich Exp $
 *
 * Copyright © 2002 Keith Packard, member of The XFree86 Project, Inc.
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

#define CURSORPATH "~/.icons:/usr/share/icons:/usr/share/pixmaps:"ICONDIR

static const char *
_XcursorLibraryPath (void)
{
    static const char	*path;

    if (!path)
    {
	path = getenv ("XCURSOR_PATH");
	if (!path)
	    path = CURSORPATH;
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

    colon = strchr (dir, ':');
    if (!colon)
	colon = dir + strlen (dir);
    
    dirlen = colon - dir;

    tcolon = strchr (theme, ':');
    if (!tcolon)
	tcolon = theme + strlen (theme);

    themelen = tcolon - theme;
    
    home = 0;
    homelen = 0;
    if (*dir == '~')
    {
	home = getenv ("HOME");
	if (!home)
	    return 0;
	homelen = strlen (home);
	dir++;
	dirlen--;
    }

    len = homelen + dirlen + 1 + themelen + 1;
    
    full = malloc (len);
    if (!full)
	return 0;
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

    full = malloc (strlen (dir) + 1 + strlen (subdir) + 1 + strlen (file) + 1);
    if (!full)
	return 0;
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
	return 0;
    return colon + 1;
}

#define XcursorWhite(c)	((c) == ' ' || (c) == '\t' || (c) == '\n')
#define XcursorSep(c) ((c) == ';' || (c) == ',')

static char *
_XcursorThemeInherits (const char *full)
{
    char    line[8192];
    char    *result = 0;
    FILE    *f;

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
    FILE	*f = 0;
    char	*full;
    char	*dir;
    const char  *path;
    char	*inherits = 0;
    const char	*i;

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
    for (path = _XcursorLibraryPath ();
	 path && f == 0;
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
    for (i = inherits; i && f == 0; i = _XcursorNextPath (i))
	f = XcursorScanTheme (i, name);
    if (inherits)
	free (inherits);
    return f;
}

XcursorImage *
XcursorLibraryLoadImage (const char *file, const char *theme, int size)
{
    FILE	    *f = 0;
    XcursorImage    *image = 0;

    if (theme)
	f = XcursorScanTheme (theme, file);
    if (!f)
	f = XcursorScanTheme ("default", file);
    if (f == XCURSOR_SCAN_CORE)
	return 0;
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
    FILE	    *f = 0;
    XcursorImages   *images = 0;

    if (theme)
	f = XcursorScanTheme (theme, file);
    if (!f)
	f = XcursorScanTheme ("default", file);
    if (f == XCURSOR_SCAN_CORE)
	return 0;
    if (f)
    {
	images = XcursorFileLoadImages (f, size);
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
    return cursor;
}

XcursorCursors *
XcursorLibraryLoadCursors (Display *dpy, const char *file)
{
    int		    size = XcursorGetDefaultSize (dpy);
    char	    *theme = XcursorGetTheme (dpy);
    XcursorImages   *images = XcursorLibraryLoadImages (file, theme, size);
    XcursorCursors  *cursors;
    
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
		    cursors = 0;
		}
		else
		    cursors->ncursor = 1;
	    }
	}
	else
	    cursors = 0;
    }
    else
    {
	cursors = XcursorImagesLoadCursors (dpy, images);
	XcursorImagesDestroy (images);
    }
    return cursors;
}

const static char   *_XcursorStandardNames[] = {
    /* 0 */
    "X_cursor",		"arrow",	    "based_arrow_down",     "based_arrow_up", 
    "boat",		"bogosity",	    "bottom_left_corner",   "bottom_right_corner", 
    "bottom_side",	"bottom_tee",	    "box_spiral",	    "center_ptr", 
    "circle",		"clock",	    "coffee_mug",	    "cross", 
    
    /* 32 */
    "cross_reverse",	"crosshair",	    "diamond_cross",	    "dot",
    "dotbox",		"double_arrow",	    "draft_large",	    "draft_small",
    "draped_box",	"exchange",	    "fleur",		    "gobbler",
    "gumby",		"hand1",	    "hand2",		    "heart",
    
    /* 64 */
    "icon",		"iron_cross",	    "left_ptr",		    "left_side", 
    "left_tee",		"leftbutton",	    "ll_angle",		    "lr_angle", 
    "man",		"middlebutton",     "mouse",		    "pencil", 
    "pirate",		"plus",		    "question_arrow",	    "right_ptr", 
    
    /* 96 */
    "right_side",	"right_tee",	    "rightbutton",	    "rtl_logo", 
    "sailboat",		"sb_down_arrow",    "sb_h_double_arrow",    "sb_left_arrow", 
    "sb_right_arrow",	"sb_up_arrow",	    "sb_v_double_arrow",    "shuttle", 
    "sizing",		"spider",	    "spraycan",		    "star", 
    
    /* 128 */
    "target",		"tcross",	    "top_left_arrow",	    "top_left_corner", 
    "top_right_corner",	"top_side",	    "top_tee",		    "trek", 
    "ul_angle",		"umbrella",	    "ur_angle",		    "watch", 
    "xterm", 
};

#define NUM_STANDARD_NAMES  (sizeof _XcursorStandardNames / sizeof _XcursorStandardNames[0])

XcursorImage *
XcursorShapeLoadImage (unsigned int shape, const char *theme, int size)
{
    unsigned int    id = shape >> 1;

    if (id < NUM_STANDARD_NAMES)
	return XcursorLibraryLoadImage (_XcursorStandardNames[id],
					theme, size);
    else
	return 0;
}

XcursorImages *
XcursorShapeLoadImages (unsigned int shape, const char *theme, int size)
{
    unsigned int    id = shape >> 1;

    if (id < NUM_STANDARD_NAMES)
	return XcursorLibraryLoadImages (_XcursorStandardNames[id],
					 theme, size);
    else
	return 0;
}

Cursor
XcursorShapeLoadCursor (Display *dpy, unsigned int shape)
{
    unsigned int    id = shape >> 1;

    if (id < NUM_STANDARD_NAMES)
	return XcursorLibraryLoadCursor (dpy, _XcursorStandardNames[id]);
    else
	return 0;
}

XcursorCursors *
XcursorShapeLoadCursors (Display *dpy, unsigned int shape)
{
    unsigned int    id = shape >> 1;

    if (id < NUM_STANDARD_NAMES)
	return XcursorLibraryLoadCursors (dpy, _XcursorStandardNames[id]);
    else
	return 0;
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
	c = strcmp (library, _XcursorStandardNames[mid]);
	if (c == 0)
	    return (mid << 1);
	if (c > 0)
	    low = mid;
	else
	    high = mid;
    }
    while (low <= high)
    {
	if (!strcmp (library, _XcursorStandardNames[low]))
	    return (low << 1);
	low++;
    }
    return -1;
}

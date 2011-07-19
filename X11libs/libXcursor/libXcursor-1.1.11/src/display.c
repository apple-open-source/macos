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
#include <X11/Xlibint.h>
#include <ctype.h>

static XcursorDisplayInfo *_XcursorDisplayInfo;

static void
_XcursorFreeDisplayInfo (XcursorDisplayInfo *info)
{
    if (info->theme)
	free (info->theme);

    if (info->theme_from_config)
	free (info->theme_from_config);

    free (info);
}

static int
_XcursorCloseDisplay (Display *dpy, XExtCodes *codes)
{
    XcursorDisplayInfo  *info, **prev;

    /*
     * Unhook from the global list
     */
    _XLockMutex (_Xglobal_lock);
    for (prev = &_XcursorDisplayInfo; (info = *prev); prev = &(*prev)->next)
	if (info->display == dpy)
	{
            *prev = info->next;
	    break;
	}
    _XUnlockMutex (_Xglobal_lock);

    if (info)
	_XcursorFreeDisplayInfo (info);
    return 0;
}

static int
_XcursorDefaultParseBool (char *v)
{
    char    c0, c1;

    c0 = *v;
    if (isupper ((int)c0))
	c0 = tolower (c0);
    if (c0 == 't' || c0 == 'y' || c0 == '1')
	return 1;
    if (c0 == 'f' || c0 == 'n' || c0 == '0')
	return 0;
    if (c0 == 'o')
    {
	c1 = v[1];
	if (isupper ((int)c1))
	    c1 = tolower (c1);
	if (c1 == 'n')
	    return 1;
	if (c1 == 'f')
	    return 0;
    }
    return -1;
}

XcursorDisplayInfo *
_XcursorGetDisplayInfo (Display *dpy)
{
    XcursorDisplayInfo	*info, **prev, *old;
    int			event_base, error_base;
    int			major, minor;
    char		*v;
    int			i;

    _XLockMutex (_Xglobal_lock);
    for (prev = &_XcursorDisplayInfo; (info = *prev); prev = &(*prev)->next)
    {
	if (info->display == dpy)
	{
	    /*
	     * MRU the list
	     */
	    if (prev != &_XcursorDisplayInfo)
	    {
		*prev = info->next;
		info->next = _XcursorDisplayInfo;
		_XcursorDisplayInfo = info;
	    }
	    break;
	}
    }
    _XUnlockMutex (_Xglobal_lock);
    if (info)
        return info;
    info = (XcursorDisplayInfo *) malloc (sizeof (XcursorDisplayInfo));
    if (!info)
	return NULL;
    info->next = NULL;
    info->display = dpy;
    
    info->codes = XAddExtension (dpy);
    if (!info->codes)
    {
	free (info);
	return NULL;
    }
    (void) XESetCloseDisplay (dpy, info->codes->extension, _XcursorCloseDisplay);

    /*
     * Check whether the display supports the Render CreateCursor request
     */
    info->has_render_cursor = XcursorFalse;
    info->has_anim_cursor = XcursorFalse;
    if (XRenderQueryExtension (dpy, &event_base, &error_base) &&
	XRenderQueryVersion (dpy, &major, &minor))
    {
	if (major > 0 || minor >= 5)
	{
	    info->has_render_cursor = XcursorTrue;
	    v = getenv ("XCURSOR_CORE");
	    if (!v)
		v = XGetDefault (dpy, "Xcursor", "core");
	    if (v && _XcursorDefaultParseBool (v) == 1)
		info->has_render_cursor = XcursorFalse;
	}
	if (info->has_render_cursor && (major > 0 || minor >= 8))
	{
	    info->has_anim_cursor = XcursorTrue;
	    v = getenv ("XCURSOR_ANIM");
	    if (!v)
		v = XGetDefault (dpy, "Xcursor", "anim");
	    if (v && _XcursorDefaultParseBool (v) == 0)
		info->has_anim_cursor = XcursorFalse;
	}
    }
    
    info->size = 0;

    /*
     * Get desired cursor size
     */
    v = getenv ("XCURSOR_SIZE");
    if (!v)
	v = XGetDefault (dpy, "Xcursor", "size");
    if (v)
	info->size = atoi (v);
    
    /*
     * Use the Xft size to guess a size; make cursors 16 "points" tall
     */
    if (info->size == 0)
    {
	int dpi = 0;
	v = XGetDefault (dpy, "Xft", "dpi");
	if (v)
	    dpi = atoi (v);
	if (dpi)
	    info->size = dpi * 16 / 72;
    }
    
    /*
     * Use display size to guess a size
     */
    if (info->size == 0)
    {
	int dim;
	    
	if (DisplayHeight (dpy, DefaultScreen (dpy)) < 
	    DisplayWidth (dpy, DefaultScreen (dpy)))
	    dim = DisplayHeight (dpy, DefaultScreen (dpy));
	else
	    dim = DisplayWidth (dpy, DefaultScreen (dpy));
	/*
	 * 16 pixels on a display of dimension 768
	 */
	info->size = dim / 48;
    }
    
    info->theme = NULL;
    info->theme_from_config = NULL;

    /*
     * Get the desired theme
     */
    v = getenv ("XCURSOR_THEME");
    if (!v)
	v = XGetDefault (dpy, "Xcursor", "theme");
    if (v)
    {
	int len;

	len = strlen (v) + 1;

	info->theme = malloc (len);
	if (info->theme)
	    strcpy (info->theme, v);

	info->theme_from_config = malloc (len);
	if (info->theme_from_config)
	    strcpy (info->theme_from_config, v);
    }

    /*
     * Get the desired dither
     */
    info->dither = XcursorDitherThreshold;
    v = getenv ("XCURSOR_DITHER");
    if (!v)
	v = XGetDefault (dpy, "Xcursor", "dither");
    if (v)
    {
	if (!strcmp (v, "threshold"))
	    info->dither = XcursorDitherThreshold;
	if (!strcmp (v, "median"))
	    info->dither = XcursorDitherMedian;
	if (!strcmp (v, "ordered"))
	    info->dither = XcursorDitherOrdered;
	if (!strcmp (v, "diffuse"))
	    info->dither = XcursorDitherDiffuse;
    }

    info->theme_core = False;
    /*
     * Find out if core cursors should
     * be themed
     */
    v = getenv ("XCURSOR_THEME_CORE");
    if (!v)
	v = XGetDefault (dpy, "Xcursor", "theme_core");
    if (v)
    {
	i = _XcursorDefaultParseBool (v);
	if (i >= 0)
	    info->theme_core = i;
    }

    info->fonts = NULL;
    for (i = 0; i < NUM_BITMAPS; i++)
	info->bitmaps[i].bitmap = None;

    /*
     * Link new info info list, making sure another
     * thread hasn't inserted something into the list while
     * this one was busy setting up the data
     */
    _XLockMutex (_Xglobal_lock);
    for (old = _XcursorDisplayInfo; old; old = old->next)
	if (old->display == dpy)
	    break;
    if (old)
    {
	_XcursorFreeDisplayInfo (info);
	info = old;
    }
    else
    {
	info->next = _XcursorDisplayInfo;
	_XcursorDisplayInfo = info;
    }
    _XUnlockMutex (_Xglobal_lock);
    
    return info;
}

XcursorBool
XcursorSupportsARGB (Display *dpy)
{
    XcursorDisplayInfo	*info = _XcursorGetDisplayInfo (dpy);

    return info && info->has_render_cursor;
}

XcursorBool
XcursorSupportsAnim (Display *dpy)
{
    XcursorDisplayInfo	*info = _XcursorGetDisplayInfo (dpy);

    return info && info->has_anim_cursor;
}

XcursorBool
XcursorSetDefaultSize (Display *dpy, int size)
{
    XcursorDisplayInfo	*info = _XcursorGetDisplayInfo (dpy);

    if (!info)
	return XcursorFalse;
    info->size = size;
    return XcursorTrue;
}

int
XcursorGetDefaultSize (Display *dpy)
{
    XcursorDisplayInfo	*info = _XcursorGetDisplayInfo (dpy);

    if (!info)
	return 0;
    return info->size;
}

XcursorBool
XcursorSetTheme (Display *dpy, const char *theme)
{
    XcursorDisplayInfo	*info = _XcursorGetDisplayInfo (dpy);
    char		*copy;

    if (!info)
	return XcursorFalse;

    if (!theme)
	theme = info->theme_from_config;

    if (theme)
    {
	copy = malloc (strlen (theme) + 1);
	if (!copy)
	    return XcursorFalse;
	strcpy (copy, theme);
    }
    else
	copy = NULL;
    if (info->theme)
	free (info->theme);
    info->theme = copy;
    return XcursorTrue;
}

char *
XcursorGetTheme (Display *dpy)
{
    XcursorDisplayInfo	*info = _XcursorGetDisplayInfo (dpy);

    if (!info)
	return NULL;
    return info->theme;
}

XcursorBool
XcursorGetThemeCore (Display *dpy)
{
    XcursorDisplayInfo	*info = _XcursorGetDisplayInfo (dpy);

    if (!info)
	return XcursorFalse;
    return info->theme_core;
    
}

XcursorBool
XcursorSetThemeCore (Display *dpy, XcursorBool theme_core)
{
    XcursorDisplayInfo	*info = _XcursorGetDisplayInfo (dpy);

    if (!info)
	return XcursorFalse;
    info->theme_core = theme_core;
    return XcursorTrue;
}

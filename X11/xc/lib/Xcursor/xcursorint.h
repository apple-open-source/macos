/*
 * $XFree86: xc/lib/Xcursor/xcursorint.h,v 1.4 2003/01/26 03:22:42 eich Exp $
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

#ifndef _XCURSORINT_H_
#define _XCURSORINT_H_

#include <X11/Xlib.h>
#include <X11/cursorfont.h>
#include <X11/extensions/Xrender.h>
#include "Xcursor.h"

typedef struct _XcursorFontInfo {
    struct _XcursorFontInfo	*next;
    Font			font;
    XcursorBool			is_cursor_font;
} XcursorFontInfo;

/*
 * Track a few recently created bitmaps to see
 * if they get used to create cursors.  This
 * is done by hooking into Xlib and watching
 * for XCreatePixmap, XPutImage, XCreatePixmapCursor
 * with appropriate arguments.  When this happens
 * Xcursor computes a hash value for the source image
 * and tries to load a library cursor of that name.
 */
 
/* large bitmaps are unlikely to be cursors */
#define MAX_BITMAP_CURSOR_SIZE	64
/* don't need to remember very many; in fact, 2 is likely sufficient */
#define NUM_BITMAPS	    8

typedef struct _XcursorBitmapInfo {
    Pixmap	    bitmap;
    unsigned long   sequence;
    unsigned int    width, height;
    Bool	    has_image;
    unsigned char   hash[XCURSOR_BITMAP_HASH_SIZE];
} XcursorBitmapInfo;

typedef enum _XcursorDither {
    XcursorDitherThreshold,
    XcursorDitherMedian,
    XcursorDitherOrdered,
    XcursorDitherDiffuse
} XcursorDither;

typedef struct _XcursorDisplayInfo {
    struct _XcursorDisplayInfo	*next;
    Display			*display;
    XExtCodes			*codes;
    XcursorBool			has_render_cursor;
    XcursorBool			has_anim_cursor;
    XcursorBool			theme_core;
    int				size;
    XcursorFontInfo		*fonts;
    char			*theme;
    XcursorDither		dither;
    XcursorBitmapInfo		bitmaps[NUM_BITMAPS];
} XcursorDisplayInfo;

XcursorDisplayInfo *
_XcursorGetDisplayInfo (Display *dpy);

Cursor
_XcursorCreateGlyphCursor(Display	    *dpy,
			  Font		    source_font,
			  Font		    mask_font,
			  unsigned int	    source_char,
			  unsigned int	    mask_char,
			  XColor _Xconst    *foreground,
			  XColor _Xconst    *background);

Cursor
_XcursorCreateFontCursor (Display *dpy, unsigned int shape);
    
#endif /* _XCURSORINT_H_ */

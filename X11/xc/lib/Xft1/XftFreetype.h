/*
 * $XFree86: xc/lib/Xft1/XftFreetype.h,v 1.4 2003/11/20 22:36:34 dawes Exp $
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

#ifndef _XFTFREETYPE_H_
#define _XFTFREETYPE_H_

#include "Xft.h"
#include <ft2build.h>
#include FT_FREETYPE_H

#include <X11/Xfuncproto.h>
#include <X11/Xosdefs.h>

extern FT_Library	_XftFTlibrary;

struct _XftFontStruct {
    FT_Face		face;      /* handle to face object */
    GlyphSet		glyphset;
    int			min_char;
    int			max_char;
    FT_F26Dot6		size;
    int			ascent;
    int			descent;
    int			height;
    int			max_advance_width;
    int			spacing;
    int			rgba;
    Bool		antialias;
    int			charmap;    /* -1 for unencoded */
    XRenderPictFormat	*format;
    XGlyphInfo		**realized;
    int			nrealized;
    Bool		transform;
    FT_Matrix		matrix;
};

#define XftUntestedGlyph	((XGlyphInfo *) 1)

_XFUNCPROTOBEGIN

/* xftdir.c */
Bool
XftDirScan (XftFontSet *set, const char *dir, Bool force);

Bool
XftDirSave (XftFontSet *set, const char *dir);

/* xftfreetype.c */
XftPattern *
XftFreeTypeQuery (const char *file, int id, int *count);

Bool
XftFreeTypeSetFace (FT_Face face, FT_F26Dot6 size, int charmap, FT_Matrix *matrix);

XftFontStruct *
XftFreeTypeOpen (Display *dpy, XftPattern *pattern);

void
XftFreeTypeClose (Display *dpy, XftFontStruct *font);

XftFontStruct *
XftFreeTypeGet (XftFont *font);

Bool
XftInitFtLibrary(void);

/* xftglyphs.c */
void
XftGlyphLoad (Display		*dpy,
	      XftFontStruct	*font,
	      XftChar32		*glyphs,
	      int		nglyph);

void
XftGlyphCheck (Display		*dpy,
	       XftFontStruct	*font,
	       XftChar32	glyph,
	       XftChar32	*missing,
	       int		*nmissing);

Bool
XftFreeTypeGlyphExists (Display		*dpy,
			XftFontStruct	*font,
			XftChar32	glyph);

/* xftrender.c */

void
XftRenderString8 (Display *dpy, Picture src, 
		  XftFontStruct *font, Picture dst,
		  int srcx, int srcy,
		  int x, int y,
		  XftChar8 *string, int len);

void
XftRenderString16 (Display *dpy, Picture src, 
		   XftFontStruct *font, Picture dst,
		   int srcx, int srcy,
		   int x, int y,
		   XftChar16 *string, int len);

void
XftRenderString32 (Display *dpy, Picture src, 
		   XftFontStruct *font, Picture dst,
		   int srcx, int srcy,
		   int x, int y,
		   XftChar32 *string, int len);

void
XftRenderStringUtf8 (Display *dpy, Picture src, 
		     XftFontStruct *font, Picture dst,
		     int srcx, int srcy,
		     int x, int y,
		     XftChar8 *string, int len);

void
XftRenderExtents8 (Display	    *dpy,
		   XftFontStruct    *font,
		   XftChar8	    *string, 
		   int		    len,
		   XGlyphInfo	    *extents);

void
XftRenderExtents16 (Display	    *dpy,
		    XftFontStruct   *font,
		    XftChar16	    *string, 
		    int		    len,
		    XGlyphInfo	    *extents);

void
XftRenderExtents32 (Display	    *dpy,
		    XftFontStruct   *font,
		    XftChar32	    *string, 
		    int		    len,
		    XGlyphInfo	    *extents);

void
XftRenderExtentsUtf8 (Display	    *dpy,
		      XftFontStruct *font,
		      XftChar8	    *string, 
		      int	    len,
		      XGlyphInfo    *extents);

_XFUNCPROTOEND

#endif /* _XFTFREETYPE_H_ */

/*
 * $XFree86: xc/lib/Xft1/Xft.h,v 1.2 2002/03/01 01:00:53 keithp Exp $
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

#ifndef _XFT_H_
#define _XFT_H_

#include <X11/extensions/Xrender.h>
#include <fontconfig/fontconfig.h>
#include <stdarg.h>

#include <X11/Xfuncproto.h>
#include <X11/Xosdefs.h>

typedef unsigned char	XftChar8;
typedef unsigned short	XftChar16;
typedef unsigned int	XftChar32;

#define XFT_FAMILY	    "family"	/* String */
#define XFT_STYLE	    "style"	/* String */
#define XFT_SLANT	    "slant"	/* Int */
#define XFT_WEIGHT	    "weight"	/* Int */
#define XFT_SIZE	    "size"	/* Double */
#define XFT_PIXEL_SIZE	    "pixelsize"	/* Double */
#define XFT_ENCODING	    "encoding"	/* String */
#define XFT_SPACING	    "spacing"	/* Int */
#define XFT_FOUNDRY	    "foundry"	/* String */
#define XFT_CORE	    "core"	/* Bool */
#define XFT_ANTIALIAS	    "antialias"	/* Bool */
#define XFT_XLFD	    "xlfd"	/* String */
#define XFT_FILE	    "file"	/* String */
#define XFT_INDEX	    "index"	/* Int */
#define XFT_RASTERIZER	    "rasterizer"/* String */
#define XFT_OUTLINE	    "outline"	/* Bool */
#define XFT_SCALABLE	    "scalable"	/* Bool */
#define XFT_RGBA	    "rgba"	/* Int */

/* defaults from resources */
#define XFT_SCALE	    "scale"	/* double */
#define XFT_RENDER	    "render"	/* Bool */
#define XFT_MINSPACE	    "minspace"	/* Bool use minimum line spacing */
#define XFT_DPI		    "dpi"	/* double */

/* specific to FreeType rasterizer */
#define XFT_CHAR_WIDTH	    "charwidth"	/* Int */
#define XFT_CHAR_HEIGHT	    "charheight"/* Int */
#define XFT_MATRIX	    "matrix"    /* XftMatrix */

#define XFT_WEIGHT_LIGHT	0
#define XFT_WEIGHT_MEDIUM	100
#define XFT_WEIGHT_DEMIBOLD	180
#define XFT_WEIGHT_BOLD		200
#define XFT_WEIGHT_BLACK	210

#define XFT_SLANT_ROMAN		0
#define XFT_SLANT_ITALIC	100
#define XFT_SLANT_OBLIQUE	110

#define XFT_PROPORTIONAL    0
#define XFT_MONO	    100
#define XFT_CHARCELL	    110

#define XFT_RGBA_NONE	    0
#define XFT_RGBA_RGB	    1
#define XFT_RGBA_BGR	    2
#define XFT_RGBA_VRGB	    3
#define XFT_RGBA_VBGR	    4

typedef FcType XftType;

typedef FcMatrix XftMatrix;

#define XftMatrixInit(m)	FcMatrixInit(m)

typedef FcResult XftResult;

#define XftResultMatch		FcResultMatch
#define XftResultNoMatch	FcResultNoMatch
#define XftResultTypeMismatch	FcResultTypeMismatch
#define XftResultNoId		FcResultNoId

#define XftTypeVoid	FcTypeVoid
#define XftTypeInteger	FcTypeInteger
#define XftTypeDouble	FcTypeDouble
#define XftTypeString	FcTypeString
#define XftTypeBool	FcTypeBool
#define XftTypeMatrix	FcTypeMatrix

typedef FcValue	XftValue;


typedef FcPattern XftPattern;

typedef FcFontSet XftFontSet;

typedef struct _XftFontStruct	XftFontStruct;

typedef struct _XftFont {
    int		ascent;
    int		descent;
    int		height;
    int		max_advance_width;
    Bool	core;
    XftPattern	*pattern;
    union {
	struct {
	    XFontStruct	    *font;
	} core;
	struct {
	    XftFontStruct   *font;
	} ft;
    } u;
} XftFont;

typedef struct _XftDraw XftDraw;

typedef struct _XftColor {
    unsigned long   pixel;
    XRenderColor    color;
} XftColor;

typedef FcObjectSet XftObjectSet;

_XFUNCPROTOBEGIN

/* xftcfg.c */
Bool
XftConfigSubstitute (XftPattern *p);

/* xftcolor.c */
Bool
XftColorAllocName (Display  *dpy,
		   Visual   *visual,
		   Colormap cmap,
		   char	    *name,
		   XftColor *result);

Bool
XftColorAllocValue (Display	    *dpy,
		    Visual	    *visual,
		    Colormap	    cmap,
		    XRenderColor    *color,
		    XftColor	    *result);

void
XftColorFree (Display	*dpy,
	      Visual	*visual,
	      Colormap	cmap,
	      XftColor	*color);


/* xftcore.c */
/* xftdbg.c */
void
XftValuePrint (XftValue v);

void
XftPatternPrint (XftPattern *p);

void
XftFontSetPrint (XftFontSet *s);

/* xftdir.c */
/* xftdpy.c */
Bool
XftDefaultHasRender (Display *dpy);
    
Bool
XftDefaultSet (Display *dpy, XftPattern *defaults);

void
XftDefaultSubstitute (Display *dpy, int screen, XftPattern *pattern);
    
/* xftdraw.c */

XftDraw *
XftDrawCreate (Display   *dpy,
	       Drawable  drawable,
	       Visual    *visual,
	       Colormap  colormap);

XftDraw *
XftDrawCreateBitmap (Display  *dpy,
		     Pixmap   bitmap);

void
XftDrawChange (XftDraw	*draw,
	       Drawable	drawable);

void
XftDrawDestroy (XftDraw	*draw);

void
XftDrawString8 (XftDraw		*d,
		XftColor	*color,
		XftFont		*font,
		int		x, 
		int		y,
		XftChar8	*string,
		int		len);

void
XftDrawString16 (XftDraw	*draw,
		 XftColor	*color,
		 XftFont	*font,
		 int		x,
		 int		y,
		 XftChar16	*string,
		 int		len);

void
XftDrawString32 (XftDraw	*draw,
		 XftColor	*color,
		 XftFont	*font,
		 int		x,
		 int		y,
		 XftChar32	*string,
		 int		len);

void
XftDrawStringUtf8 (XftDraw	*d,
		   XftColor	*color,
		   XftFont	*font,
		   int		x, 
		   int		y,
		   XftChar8	*string,
		   int		len);

void
XftDrawRect (XftDraw	    *d,
	     XftColor	    *color,
	     int	    x, 
	     int	    y,
	     unsigned int   width,
	     unsigned int   height);


Bool
XftDrawSetClip (XftDraw	    *d,
		Region	    r);

/* xftextent.c */

void
XftTextExtents8 (Display	*dpy,
		 XftFont	*font,
		 XftChar8	*string, 
		 int		len,
		 XGlyphInfo	*extents);

void
XftTextExtents16 (Display	    *dpy,
		  XftFont	    *font,
		  XftChar16	    *string, 
		  int		    len,
		  XGlyphInfo	    *extents);

void
XftTextExtents32 (Display	*dpy,
		  XftFont	*font,
		  XftChar32	*string, 
		  int		len,
		  XGlyphInfo	*extents);
    
void
XftTextExtentsUtf8 (Display	*dpy,
		    XftFont	*font,
		    XftChar8	*string, 
		    int		len,
		    XGlyphInfo	*extents);

/* xftfont.c */
XftPattern *
XftFontMatch (Display *dpy, int screen, XftPattern *pattern, XftResult *result);

XftFont *
XftFontOpenPattern (Display *dpy, XftPattern *pattern);

XftFont *
XftFontOpen (Display *dpy, int screen, ...);

XftFont *
XftFontOpenName (Display *dpy, int screen, const char *name);

XftFont *
XftFontOpenXlfd (Display *dpy, int screen, const char *xlfd);

void
XftFontClose (Display *dpy, XftFont *font);

Bool
XftGlyphExists (Display *dpy, XftFont *font, XftChar32 glyph);
    
/* xftfreetype.c */
/* xftfs.c */

XftFontSet *
XftFontSetCreate (void);

void
XftFontSetDestroy (XftFontSet *s);

Bool
XftFontSetAdd (XftFontSet *s, XftPattern *font);

/* xftglyphs.c */
/* see XftFreetype.h */

/* xftgram.y */

/* xftinit.c */
Bool
XftInit (char *config);
    
/* xftlex.l */

/* xftlist.c */
XftObjectSet *
XftObjectSetCreate (void);

Bool
XftObjectSetAdd (XftObjectSet *os, const char *object);

void
XftObjectSetDestroy (XftObjectSet *os);

XftObjectSet *
XftObjectSetVaBuild (const char *first, va_list va);

XftObjectSet *
XftObjectSetBuild (const char *first, ...);

XftFontSet *
XftListFontSets (XftFontSet	**sets,
		 int		nsets,
		 XftPattern	*p,
		 XftObjectSet	*os);

XftFontSet *
XftListFontsPatternObjects (Display	    *dpy,
			    int		    screen,
			    XftPattern	    *pattern,
			    XftObjectSet    *os);

XftFontSet *
XftListFonts (Display	*dpy,
	      int	screen,
	      ...);

/* xftmatch.c */
XftPattern *
XftFontSetMatch (XftFontSet	**sets, 
		 int		nsets, 
		 XftPattern	*p, 
		 XftResult	*result);

/* xftmatrix.c */
int
XftMatrixEqual (const XftMatrix *mat1, const XftMatrix *mat2);

void
XftMatrixMultiply (XftMatrix *result, XftMatrix *a, XftMatrix *b);

void
XftMatrixRotate (XftMatrix *m, double c, double s);

void
XftMatrixScale (XftMatrix *m, double sx, double sy);

void
XftMatrixShear (XftMatrix *m, double sh, double sv);

/* xftname.c */
XftPattern *
XftNameParse (const char *name);

Bool
XftNameUnparse (XftPattern *pat, char *dest, int len);

/* xftpat.c */
XftPattern *
XftPatternCreate (void);

XftPattern *
XftPatternDuplicate (XftPattern *p);

void
XftValueDestroy (XftValue v);

void
XftPatternDestroy (XftPattern *p);

Bool
XftPatternAdd (XftPattern *p, const char *object, XftValue value, Bool append);
    
XftResult
XftPatternGet (XftPattern *p, const char *object, int id, XftValue *v);
    
Bool
XftPatternDel (XftPattern *p, const char *object);

Bool
XftPatternAddInteger (XftPattern *p, const char *object, int i);

Bool
XftPatternAddDouble (XftPattern *p, const char *object, double d);

Bool
XftPatternAddString (XftPattern *p, const char *object, const char *s);

Bool
XftPatternAddMatrix (XftPattern *p, const char *object, const XftMatrix *s);

Bool
XftPatternAddBool (XftPattern *p, const char *object, Bool b);

XftResult
XftPatternGetInteger (XftPattern *p, const char *object, int n, int *i);

XftResult
XftPatternGetDouble (XftPattern *p, const char *object, int n, double *d);

XftResult
XftPatternGetString (XftPattern *p, const char *object, int n, char **s);

XftResult
XftPatternGetMatrix (XftPattern *p, const char *object, int n, XftMatrix **s);

XftResult
XftPatternGetBool (XftPattern *p, const char *object, int n, Bool *b);

XftPattern *
XftPatternVaBuild (XftPattern *orig, va_list va);
    
XftPattern *
XftPatternBuild (XftPattern *orig, ...);

/* xftrender.c */
/* see XftFreetype.h */

/* xftstr.c */

int
XftUtf8ToUcs4 (XftChar8    *src_orig,
	       XftChar32   *dst,
	       int	    len);

Bool
XftUtf8Len (XftChar8	*string,
	    int		len,
	    int		*nchar,
	    int		*wchar);

/* xftxlfd.c */
XftPattern *
XftXlfdParse (const char *xlfd_orig, Bool ignore_scalable, Bool complete);
    
XFontStruct *
XftCoreOpen (Display *dpy, XftPattern *pattern);

void
XftCoreClose (Display *dpy, XFontStruct *font);

_XFUNCPROTOEND

#endif /* _XFT_H_ */

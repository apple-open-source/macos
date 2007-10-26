#ifndef XKBUIPRIV_H
#define	XKBUIPRIV_H 1
/* $XConsortium: XKBuiPriv.h /main/2 1995/12/07 21:18:19 kaleb $ */
/************************************************************
 Copyright (c) 1996 by Silicon Graphics Computer Systems, Inc.

 Permission to use, copy, modify, and distribute this
 software and its documentation for any purpose and without
 fee is hereby granted, provided that the above copyright
 notice appear in all copies and that both that copyright
 notice and this permission notice appear in supporting
 documentation, and that the name of Silicon Graphics not be 
 used in advertising or publicity pertaining to distribution 
 of the software without specific prior written permission.
 Silicon Graphics makes no representation about the suitability 
 of this software for any purpose. It is provided "as is"
 without any express or implied warranty.
 
 SILICON GRAPHICS DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS 
 SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY 
 AND FITNESS FOR A PARTICULAR PURPOSE. IN NO EVENT SHALL SILICON
 GRAPHICS BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL 
 DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, 
 DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE 
 OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION  WITH
 THE USE OR PERFORMANCE OF THIS SOFTWARE.

 ********************************************************/

#include "XKBui.h"

typedef struct _XkbUI_Point {
	double			x;
	double			y;
} XkbUI_PointRec,*XkbUI_PointPtr;

typedef struct _XkbUI_View {
	Display *		dpy;
	XkbDescPtr		xkb;
	Window			win;
	GC			gc;
	XkbUI_ViewOptsRec	opts;
	unsigned int		canvas_width;
	unsigned int		canvas_height;
	unsigned char		state[XkbMaxLegalKeyCode];

	double			xscale;
	double			yscale;
} XkbUI_ViewRec;

#define	_XkbAlloc(s)		malloc((s))
#define	_XkbCalloc(n,s)		calloc((n),(s))
#define	_XkbRealloc(o,s)	realloc((o),(s))
#define	_XkbTypedAlloc(t)	((t *)malloc(sizeof(t)))
#define	_XkbTypedCalloc(n,t)	((t *)calloc((n),sizeof(t)))
#define	_XkbTypedRealloc(o,n,t) \
	((o)?(t *)realloc((o),(n)*sizeof(t)):_XkbTypedCalloc(n,t))
#define	_XkbClearElems(a,f,l,t)	bzero(&(a)[f],((l)-(f)+1)*sizeof(t))
#define	_XkbFree(p)		free(p)

	/*
	 * private values for key appearance flags 
	 * must not conflict with values from XKBui.h
	 */
#define	XkbUI_Obscured		(1<<7)

_XFUNCPROTOBEGIN

_XFUNCPROTOEND

#endif /* XKBUIPRIV_H */

/*
 * Copyright (c) 2000 by The XFree86 Project, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE XFREE86 PROJECT BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF
 * OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 * 
 * Except as contained in this notice, the name of the XFree86 Project shall
 * not be used in advertising or otherwise to promote the sale, use or other
 * dealings in this Software without prior written authorization from the
 * XFree86 Project.
 */
/* $XFree86: xc/lib/GLw/GLwXm/XmStrDefs.h,v 1.1 2000/11/02 20:39:08 dawes Exp $ */

/*
 * This file is a fake Motif(TM) header intended to allow the compilation of
 * libGLw without having a real Motif.  Do not use this file to compile any
 * application because it declares only a small subset of the API!
 */

#ifndef GLWXM_STRDEFS_H
#define GLWXM_STRDEFS_H

#ifndef _XmConst
#define _XmConst /**/
#endif

#ifdef XMSTRINGDEFINES

#define XmCBackgroundPixmap	"BackgroundPixmap"
#define XmRBackgroundPixmap	"BackgroundPixmap"
#define XmRBooleanDimension	"BooleanDimension"
#define XmNbottomAttachment	"bottomAttachment"
#define XmNbottomWidget		"bottomWidget"
#define XmCForegroundThreshold	"ForegroundThreshold"
#define XmNforegroundThreshold	"foregroundThreshold"
#define XmCHighlightColor	"HighlightColor"
#define XmNhighlightColor	"highlightColor"
#define XmCHighlightOnEnter	"HighlightOnEnter"
#define XmNhighlightOnEnter	"highlightOnEnter"
#define XmCHighlightThickness	"HighlightThickness"
#define XmNhighlightThickness	"highlightThickness"
#define XmCHighlightPixmap	"HighlightPixmap"
#define XmNhighlightPixmap	"highlightPixmap"
#define XmRHighlightPixmap	"HighlightPixmap"
#define XmRHorizontalDimension	"HorizontalDimension"
#define XmNleftAttachment	"leftAttachment"
#define XmNrightAttachment	"rightAttachment"
#define XmRStringDirection	"StringDirection"
#define XmNtopAttachment	"topAttachment"
#define XmCTraversalOn		"TraversalOn"
#define XmNtraversalOn		"traversalOn"
#define XmRXmBackgroundPixmap	"XmBackgroundPixmap"

#else /* XMSTRINGDEFINES */

extern _XmConst char _XmStrings[];

#define XmCBackgroundPixmap	((char *)&_XmStrings[299])
#define XmRBackgroundPixmap	((char *)&_XmStrings[10844])
#define XmRBooleanDimension	((char *)&_XmStrings[10872])
#define XmNbottomAttachment	((char *)&_XmStrings[5017])
#define XmNbottomWidget		((char *)&_XmStrings[5099])
#define XmCHighlightColor	((char *)&_XmStrings[1844])
#define XmNhighlightColor	((char *)&_XmStrings[6996])
#define XmCHighlightOnEnter	((char *)&_XmStrings[1859])
#define XmNhighlightOnEnter	((char *)&_XmStrings[7011])
#define XmCHighlightThickness	((char *)&_XmStrings[1892])
#define XmNhighlightThickness	((char *)&_XmStrings[7044])
#define XmCForegroundThreshold	((char *)&_XmStrings[1808])
#define XmNforegroundThreshold	((char *)&_XmStrings[6914])
#define XmCHighlightPixmap	((char *)&_XmStrings[1876])
#define XmNhighlightPixmap	((char *)&_XmStrings[7028])
#define XmRHighlightPixmap	((char *)&_XmStrings[11299])
#define XmRHorizontalDimension	((char *)&_XmStrings[11315])
#define XmNleftAttachment	((char *)&_XmStrings[7523])
#define XmNrightAttachment	((char *)&_XmStrings[9077])
#define XmRStringDirection	((char *)&_XmStrings[11981])
#define XmNtopAttachment	((char *)&_XmStrings[10165])
#define XmCTraversalOn		((char *)&_XmStrings[4318])
#define XmNtraversalOn		((char *)&_XmStrings[10361])
#define XmRXmBackgroundPixmap	((char *)&_XmStrings[12210])

#endif /* XMSTRINGDEFINES */

/* copy Xt constant definitions */
#include <X11/StringDefs.h>

#define XmCBackground		XtCBackground
#define XmNbackground		XtNbackground
#define XmNbackgroundPixmap	XtNbackgroundPixmap
#define XmCBoolean		XtCBoolean
#define XmRBoolean		XtRBoolean
#define XmRCallProc		XtRCallProc
#define XmCForeground		XtCForeground
#define XmNforeground		XtNforeground
#define XmRImmediate		XtRImmediate
#define XmRPixel		XtRPixel
#define XmCPixmap		XtCPixmap
#define XmNpixmap		XtNpixmap
#define XmRPixmap		XtRPixmap
#define XmRPrimHighlightPixmap	XmRHighlightPixmap
#define XmRString		XtRString
#define XmRStringArray		XtRStringArray
#define XmRStringTable		XtRStringTable

#endif /* GLWXM_STRDEFS_H */

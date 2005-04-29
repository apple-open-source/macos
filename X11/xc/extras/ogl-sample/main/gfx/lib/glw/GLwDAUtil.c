/*
** License Applicability. Except to the extent portions of this file are
** made subject to an alternative license as permitted in the SGI Free
** Software License B, Version 1.1 (the "License"), the contents of this
** file are subject only to the provisions of the License. You may not use
** this file except in compliance with the License. You may obtain a copy
** of the License at Silicon Graphics, Inc., attn: Legal Services, 1600
** Amphitheatre Parkway, Mountain View, CA 94043-1351, or at:
** 
** http://oss.sgi.com/projects/FreeB
** 
** Note that, as provided in the License, the Software is distributed on an
** "AS IS" basis, with ALL EXPRESS AND IMPLIED WARRANTIES AND CONDITIONS
** DISCLAIMED, INCLUDING, WITHOUT LIMITATION, ANY IMPLIED WARRANTIES AND
** CONDITIONS OF MERCHANTABILITY, SATISFACTORY QUALITY, FITNESS FOR A
** PARTICULAR PURPOSE, AND NON-INFRINGEMENT.
** 
** Original Code. The Original Code is: OpenGL Sample Implementation,
** Version 1.2.1, released January 26, 2000, developed by Silicon Graphics,
** Inc. The Original Code is Copyright (c) 1991-2000 Silicon Graphics, Inc.
** Copyright in any portions created by third parties is as indicated
** elsewhere herein. All Rights Reserved.
** 
** Additional Notice Provisions: The application programming interfaces
** established by SGI in conjunction with the Original Code are The
** OpenGL(R) Graphics System: A Specification (Version 1.2.1), released
** April 1, 1999; The OpenGL(R) Graphics System Utility Library (Version
** 1.3), released November 4, 1998; and OpenGL(R) Graphics with the X
** Window System(R) (Version 1.3), released October 19, 1998. This software
** was created using the OpenGL(R) version 1.2.1 Sample Implementation
** published by SGI, but has not been independently verified as being
** compliant with the OpenGL(R) version 1.2.1 Specification.
**
*/
/* utility routines for GLX.
 * Since these routines are used both in the Motif and X versions of
 * the widget, they cannot peek directly into the structure
 *
 */

/* $XFree86: xc/extras/ogl-sample/main/gfx/lib/glw/GLwDAUtil.c,v 1.4 2003/11/06 18:37:54 tsi Exp $ */

#include <X11/IntrinsicP.h>
#include <X11/StringDefs.h>
#include <GL/glx.h>
#include <GL/gl.h>

/*
 * By default only a static library is created because most of the UNIX
 * loaders, if not all, complain about unresolved symbols even if the
 * application doesn't use the modules in which such symbols are referenced.
 * However, if your system supports libraries with weak symbols (e.g.
 * Solaris, FreeBSD and Linux) it is possible to fool the loader using these
 * weak symbols.
 */
#ifdef USE_XM_STUBS
#pragma weak _XmPrimitiveHighlightPixmapDefault	= _Xm_Stub_Pointer_
#pragma weak _XmHighlightColorDefault		= _Xm_Stub_Pointer_
#pragma weak _XmForegroundColorDefault		= _Xm_Stub_Pointer_
#pragma weak _XmBackgroundColorDefault		= _Xm_Stub_Pointer_
#pragma weak _XmStrings				= _Xm_Stub_Pointer_
#pragma weak xmPrimitiveClassRec		= _Xm_Stub_Pointer_

XtPointer _Xm_Stub_Pointer_ = NULL;
#endif

void GLwDrawingAreaMakeCurrent (Widget w, GLXContext ctx)
{
    glXMakeCurrent (XtDisplay(w), XtWindow(w), ctx);
}

void GLwDrawingAreaSwapBuffers (Widget w)
{
    glXSwapBuffers (XtDisplay(w), XtWindow(w));
}

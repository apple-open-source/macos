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
/* $XFree86: xc/lib/GLw/GLwXm/XmP.h,v 1.1 2000/11/02 20:39:08 dawes Exp $ */

/*
 * This file is a fake Motif(TM) header intended to allow the compilation of
 * libGLw without having a real Motif.  Do not use this file to compile any
 * application because it declares only a small subset of the API!
 */

#ifndef GLWXM_P_H
#define GLWXM_P_H

#include "GLwXm/Xm.h"
#include <X11/IntrinsicP.h>

#define XmInheritBorderHighlight   ((XtWidgetProc) _XtInherit)
#define XmInheritBorderUnhighlight ((XtWidgetProc) _XtInherit)

void _XmBackgroundColorDefault(Widget widget, int offset, XrmValue *value);
void _XmForegroundColorDefault(Widget widget, int offset, XrmValue *value);
void _XmHighlightColorDefault(Widget widget, int offset, XrmValue *value);
void _XmPrimitiveHighlightPixmapDefault(Widget widget, int offset,
					XrmValue *value);

typedef enum { 
    XmSYNTHETIC_NONE, 
    XmSYNTHETIC_LOAD 
} XmImportOperator;

typedef void (*XmExportProc)(Widget, int, XtArgVal *);

typedef XmImportOperator (*XmImportProc)(Widget, int, XtArgVal*);

typedef struct _XmSyntheticResource {
    String resource_name;
    Cardinal resource_size;
    Cardinal resource_offset;
    XmExportProc export_proc;
    XmImportProc import_proc;
} XmSyntheticResource;

#endif /* GLWXM_P_H */

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
/* $XFree86: xc/lib/GLw/GLwXm/PrimitiveP.h,v 1.1 2000/11/02 20:39:08 dawes Exp $ */

/*
 * This file is a fake Motif(TM) header intended to allow the compilation of
 * libGLw without having a real Motif.  Do not use this file to compile any
 * application because it declares only a small subset of the API!
 */

#ifndef GLWXM_PRIMITIVE_P_H
#define GLWXM_PRIMITIVE_P_H

#include "GLwXm/XmP.h"

/* primitive instance part */
typedef struct {
    Pixel foreground;
    Dimension shadow_thickness;
    Pixel top_shadow_color;
    Pixmap top_shadow_pixmap;
    Pixel bottom_shadow_color;
    Pixmap bottom_shadow_pixmap;
    Dimension highlight_thickness;
    Pixel highlight_color;
    Pixmap highlight_pixmap;
    XtCallbackList help_callback;
    XtPointer user_data;
    Boolean traversal_on;
    Boolean highlight_on_enter;
    Boolean have_traversal;
    unsigned char unit_type;
    XmNavigationType navigation_type;
    Boolean highlight_drawn;
    Boolean highlighted;
    GC highlight_GC;
    GC top_shadow_GC;
    GC bottom_shadow_GC;
#if XmVERSION > 1
    XtCallbackList convert_callback; 
    XtCallbackList popup_handler_callback; 
    XmDirection layout_direction; 
#endif
} XmPrimitivePart;

/* primitive class part */
typedef struct {
    XtWidgetProc border_highlight;
    XtWidgetProc border_unhighlight;
    String translations;
    XtActionProc arm_and_activate;
    XmSyntheticResource *syn_resources;
    int num_syn_resources;
    XtPointer extension;
} XmPrimitiveClassPart;

/* class record */
typedef struct _XmPrimitiveClassRec {
    CoreClassPart core_class;
    XmPrimitiveClassPart primitive_class;
} XmPrimitiveClassRec;

/* declare class record */
extern XmPrimitiveClassRec xmPrimitiveClassRec;

#endif /* GLWXM_PRIMITIVE_P_H */

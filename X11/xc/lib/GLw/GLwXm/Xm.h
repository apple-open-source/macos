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
/* $XFree86: xc/lib/GLw/GLwXm/Xm.h,v 1.1 2000/11/02 20:39:08 dawes Exp $ */

/*
 * This file is a fake Motif(TM) header intended to allow the compilation of
 * libGLw without having a real Motif.  Do not use this file to compile any
 * application because it declares only a small subset of the API!
 */

#ifndef GLWXM_H
#define GLWXM_H

#include <X11/Intrinsic.h>
#include <X11/Shell.h>
#include <X11/Xatom.h>
#include "GLwXm/XmStrDefs.h"

enum {
    XmATTACH_NONE,
    XmATTACH_FORM,
    XmATTACH_OPPOSITE_FORM,
    XmATTACH_WIDGET,
    XmATTACH_OPPOSITE_WIDGET,
    XmATTACH_POSITION,
    XmATTACH_SELF
};

enum {
    XmCR_NONE,
    XmCR_HELP,
    XmCR_VALUE_CHANGED,
    XmCR_INCREMENT,
    XmCR_DECREMENT,
    XmCR_PAGE_INCREMENT,
    XmCR_PAGE_DECREMENT,
    XmCR_TO_TOP,
    XmCR_TO_BOTTOM,
    XmCR_DRAG,
    XmCR_ACTIVATE,
    XmCR_ARM,
    XmCR_DISARM,
    XmCR_DUMMY13,
    XmCR_DUMMY14,
    XmCR_DUMMY15,
    XmCR_MAP,
    XmCR_UNMAP,
    XmCR_FOCUS,
    XmCR_LOSING_FOCUS,
    XmCR_MODIFYING_TEXT_VALUE,
    XmCR_MOVING_INSERT_CURSOR,
    XmCR_EXECUTE,
    XmCR_SINGLE_SELECT,
    XmCR_MULTIPLE_SELECT,
    XmCR_EXTENDED_SELECT,
    XmCR_BROWSE_SELECT,
    XmCR_DEFAULT_ACTION,
    XmCR_CLIPBOARD_DATA_REQUEST,
    XmCR_CLIPBOARD_DATA_DELETE,
    XmCR_CASCADING,
    XmCR_OK,
    XmCR_CANCEL,
    XmCR_DUMMY33,
    XmCR_APPLY,
    XmCR_NO_MATCH,
    XmCR_COMMAND_ENTERED,
    XmCR_COMMAND_CHANGED,
    XmCR_EXPOSE,
    XmCR_RESIZE,
    XmCR_INPUT,
    XmCR_GAIN_PRIMARY,
    XmCR_LOSE_PRIMARY,
    XmCR_CREATE,
    XmCR_TEAR_OFF_ACTIVATE,
    XmCR_TEAR_OFF_DEACTIVATE,
    XmCR_OBSCURED_TRAVERSAL,
#if XmVERSION < 2
    XmCR_PROTOCOLS
#else
    XmCR_FOCUS_MOVED,
    XmCR_DUMMY48,
    XmCR_DUMMY49,
    XmCR_DUMMY50,
    XmCR_DUMMY51,
    XmCR_DUMMY52,
    XmCR_DUMMY53,
    XmCR_REPOST,
    XmCR_COLLAPSED,
    XmCR_EXPANDED,
    XmCR_SELECT,
    XmCR_DRAG_START,
    XmCR_NO_FONT,
    XmCR_NO_RENDITION,
    XmCR_POST,
    XmCR_SPIN_NEXT,
    XmCR_SPIN_PRIOR,
    XmCR_SPIN_FIRST,
    XmCR_SPIN_LAST,
    XmCR_PAGE_SCROLLER_INCREMENT,
    XmCR_PAGE_SCROLLER_DECREMENT,
    XmCR_MAJOR_TAB,
    XmCR_MINOR_TAB,
    XmCR_PROTOCOLS = 6666
#endif
};

typedef unsigned char XmDirection;

typedef struct {
    int reason;
    XEvent *event;
    Window window;
} XmDrawingAreaCallbackStruct;

#define XmUNSPECIFIED_PIXMAP 2

typedef unsigned char XmNavigationType;

#endif /* GLWXM_H */

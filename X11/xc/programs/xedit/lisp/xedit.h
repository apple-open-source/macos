/*
 * Copyright (c) 2002 by The XFree86 Project, Inc.
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
 *
 * Author: Paulo César Pereira de Andrade
 */

/* $XFree86: xc/programs/xedit/lisp/xedit.h,v 1.5 2002/11/10 23:22:00 paulo Exp $ */

#ifndef Lisp_xedit_h
#define Lisp_xedit_h

#ifdef XEDIT_LISP_PRIVATE
#include "private.h"
#include "io.h"
#include "read.h"
#include "write.h"

LispObj *Xedit_AddEntity(LispBuiltin*);
LispObj *Xedit_AutoFill(LispBuiltin*);
LispObj *Xedit_Background(LispBuiltin*);
LispObj *Xedit_CharAfter(LispBuiltin*);
LispObj *Xedit_CharBefore(LispBuiltin*);
LispObj *Xedit_ClearEntities(LispBuiltin*);
LispObj *Xedit_ConvertPropertyList(LispBuiltin*);
LispObj *Xedit_Font(LispBuiltin*);
LispObj *Xedit_Foreground(LispBuiltin*);
LispObj *Xedit_GotoChar(LispBuiltin*);
LispObj *Xedit_HorizontalScrollbar(LispBuiltin*);
LispObj *Xedit_Insert(LispBuiltin*);
LispObj *Xedit_Justification(LispBuiltin*);
LispObj *Xedit_LeftColumn(LispBuiltin*);
LispObj *Xedit_Point(LispBuiltin*);
LispObj *Xedit_PointMax(LispBuiltin*);
LispObj *Xedit_PointMin(LispBuiltin*);
LispObj *Xedit_PropertyList(LispBuiltin*);
LispObj *Xedit_ReadText(LispBuiltin*);
LispObj *Xedit_ReplaceText(LispBuiltin*);
LispObj *Xedit_RightColumn(LispBuiltin*);
LispObj *Xedit_Scan(LispBuiltin*);
LispObj *Xedit_SearchBackward(LispBuiltin*);
LispObj *Xedit_SearchForward(LispBuiltin*);
LispObj *Xedit_VerticalScrollbar(LispBuiltin*);
LispObj *Xedit_WrapMode(LispBuiltin*);
LispObj *Xedit_XrmStringToQuark(LispBuiltin*);
#else
#define LispObj void
#endif /* XEDIT_LISP_PRIVATE */

typedef struct _EditModeInfo {
    char *desc;			/* Mode description */
    Widget sme;			/* Menu entry */
    LispObj *symbol;		/* Symbol holding syntax data */
    LispObj *syntax;		/* The syntax definition */
} EditModeInfo;

/* Typedef'ed to XeditLispData in ../xedit.h */
struct _XeditLispData {
    LispObj *syntax;		/* Syntax definition */
    LispObj *syntable;		/* Syntax-table the cursor is located */
    int disable_highlight;	/* Working in the buffer */
};

void LispXeditInitialize(void);
void XeditLispExecute(Widget, XawTextPosition, XawTextPosition);
void XeditLispSetEditMode(xedit_flist_item*, LispObj*);
void XeditLispUnsetEditMode(xedit_flist_item*);

extern EditModeInfo *mode_infos;
extern Cardinal num_mode_infos;

#endif /* Lisp_xedit_h */

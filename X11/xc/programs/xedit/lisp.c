/*
 * Copyright (c) 2001 by The XFree86 Project, Inc.
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

/* $XFree86: xc/programs/xedit/lisp.c,v 1.20 2002/12/04 05:27:56 paulo Exp $ */

#include "xedit.h"
#include "lisp/lisp.h"
#include "lisp/xedit.h"
#include <unistd.h>
#include <locale.h>
#include <ctype.h>

#include <X11/Xaw/SmeBSB.h>
#include <X11/Xaw/SimpleMenu.h>

/*
 * Prototypes
 */
static void XeditDoLispEval(Widget);
static void EditModeCallback(Widget, XtPointer, XtPointer);

/*
 * Initialization
 */
static int lisp_initialized;
extern Widget scratch;
static Widget edit_mode_menu, edit_mode_entry, edit_mode_none;

/*
 * Implementation
 */
void
XeditLispInitialize(void)
{
    setlocale(LC_NUMERIC, "C");
    lisp_initialized = 1;
    LispBegin();
    LispXeditInitialize();
}

void
XeditLispCleanUp(void)
{
    LispEnd();
}

void
XeditLispEval(Widget w, XEvent *event, String *params, Cardinal *num_params)
{
    XeditDoLispEval(messwidget);
}

void
XeditPrintLispEval(Widget w, XEvent *event, String *params, Cardinal *num_params)
{
    if (XawTextGetSource(w) == scratch) {
	XtCallActionProc(w, "newline", event, params, *num_params);
	XeditDoLispEval(w);
    }
    else
	XtCallActionProc(w, "newline-and-indent", event, params, *num_params);
}

void
XeditKeyboardReset(Widget w, XEvent *event, String *params, Cardinal *num_params)
{
    XtCallActionProc(w, "keyboard-reset", event, params, *num_params);
}

void
SetTextProperties(xedit_flist_item *item)
{
    if (lisp_initialized) {
	Widget source = XawTextGetSource(textwindow);
	XawTextPosition top = XawTextTopPosition(textwindow);

	if (source != item->source)
	    XawTextSetSource(textwindow, item->source, 0);
	XeditLispSetEditMode(item, NULL);
	if (source != item->source)
	    XawTextSetSource(textwindow, source, top);
    }
}

void
UnsetTextProperties(xedit_flist_item *item)
{
    XeditLispUnsetEditMode(item);
}

static void
XeditDoLispEval(Widget output)
{
    Widget src;
    XawTextBlock block;
    XawTextPosition position, end;

    /* get lisp expression */
    src = XawTextGetSource(textwindow);
    position = XawTextGetInsertionPoint(textwindow);
    --position;
    while (position >= 0) {
	(void)XawTextSourceRead(src, position, &block, 1);
	if (!isspace(block.ptr[0]))
	    break;
	--position;
    }
    end = position + 1;

    if (block.ptr[0] != ')') {
	while (position >= 0) {
	    (void)XawTextSourceRead(src, position, &block, 1);
	    if (isspace(block.ptr[0]) ||
		block.ptr[0] == '(' ||
		block.ptr[0] == ')' ||
		block.ptr[0] == '"' ||
		block.ptr[0] == '|')
		break;
	    --position;
	}
	if (!isspace(block.ptr[0]))
	    ++position;
    }
    else {
	/* XXX note that embedded '(' and ')' will confuse this code */
	XawTextPosition last, tmp;
	int level = 0;
	char ptr[2];

	last = position;
	ptr[1] = '\0';
	block.ptr = ptr;
	do {
	    block.ptr[0] = '(';
	    position = XawTextSourceSearch(src, last, XawsdLeft, &block);
	    if (position == XawTextSearchError) {
		Feep();
		return;
	    }
	    block.ptr[0] = ')';
	    tmp = position;
	    do {
		tmp = XawTextSourceSearch(src, tmp, XawsdRight, &block);
		if (tmp == XawTextSearchError) {
		    Feep();
		    return;
		}
		if (tmp <= last)
		    ++level;
	    } while (++tmp <= last);
	    --level;
	    last = position;
	} while (level);
	/* check for extra characters */
	while (position > 0) {
	    (void)XawTextSourceRead(src, position - 1, &block, 1);
	    if (block.length != 1 ||
		isspace(block.ptr[0]) ||
		block.ptr[0] == ')' ||
		block.ptr[0] == '"' ||
		block.ptr[0] == '|')
		break;
	    --position;
	}
    }

    if (position < 0 || position >= end)
	Feep();
    else
	XeditLispExecute(output, position, end);
}

void
CreateEditModePopup(Widget parent)
{
    int i;
    Widget sme;
    static char *editModes = "editModes";

    XtVaCreateManagedWidget("modeMenuItem", smeBSBObjectClass, parent,
			    XtNmenuName, editModes, NULL);
    edit_mode_menu = XtCreatePopupShell(editModes, simpleMenuWidgetClass,
					parent, NULL, 0);
    XtRealizeWidget(edit_mode_menu);

    edit_mode_none = XtCreateManagedWidget("none", smeBSBObjectClass,
					   edit_mode_menu, NULL, 0);
    XtAddCallback(edit_mode_none, XtNcallback, EditModeCallback, NULL);

    for (i = 0; i < num_mode_infos; i++) {
	sme = XtVaCreateManagedWidget("mode", smeBSBObjectClass, edit_mode_menu,
				      XtNlabel, mode_infos[i].desc, NULL);
	XtAddCallback(sme, XtNcallback, EditModeCallback,
		      (XtPointer)(mode_infos + i));
	mode_infos[i].sme = sme;
    }
}

void
SetEditModeMenu(void)
{
    int i;
    Widget old_entry = edit_mode_entry, new_entry = edit_mode_none;
    xedit_flist_item *item = FindTextSource(XawTextGetSource(textwindow), NULL);

    for (i = 0; i < num_mode_infos; i++) {
	if (item->xldata && item->xldata->syntax &&
	    mode_infos[i].syntax == item->xldata->syntax) {
	    new_entry = mode_infos[i].sme;
	    break;
	}
    }

    if (old_entry != new_entry) {
	if (old_entry)
	    XtVaSetValues(old_entry, XtNleftBitmap, None, NULL);
	XtVaSetValues(new_entry, XtNleftBitmap, flist.pixmap, NULL);
	edit_mode_entry = new_entry;
    }
}

static void
EditModeCallback(Widget w, XtPointer client_data, XtPointer call_data)
{
    Widget source = XawTextGetSource(textwindow);
    EditModeInfo *info = (EditModeInfo*)client_data;
    xedit_flist_item *item = FindTextSource(source, NULL);

    /* If no edit mode selected and selecting the plain/none mode */
    if ((info == NULL &&
	 (item->xldata == NULL || item->xldata->syntax == NULL)) ||
	/* if selecting the current mode */
	(info && item && item->xldata && info->syntax &&
	 info->syntax == item->xldata->syntax))
	return;

    XawTextSourceClearEntities(source,
			       XawTextSourceScan(source, 0, XawsdLeft,
						 XawstAll, 1, True),
			       XawTextSourceScan(source, 0, XawsdRight,
						 XawstAll, 1, True));
    XeditLispUnsetEditMode(item);
    if (info)
	XeditLispSetEditMode(item, info->symbol);
    else
	item->properties = NULL;
    UpdateTextProperties(1);
}

/*
 * $XConsortium: menu.c,v 1.7 94/04/17 20:24:02 kaleb Exp $
 *
Copyright (c) 1989  X Consortium

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
X CONSORTIUM BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of the X Consortium shall not be
used in advertising or otherwise to promote the sale, use or other dealings
in this Software without prior written authorization from the X Consortium.
 *
 */
/* $XFree86: xc/programs/xmh/menu.c,v 1.3 2001/10/28 03:34:38 tsi Exp $ */

#include "xmh.h"
#include "bboxint.h"


void AttachMenuToButton(button, menu, menu_name)
    Button	button;
    Widget	menu;
    char *	menu_name;
{
    Arg		args[3];

    if (button == NULL) return;
    button->menu = menu;
    /* Yup, this is a memory leak. :-) */
    XtSetArg(args[0], XtNmenuName, XtNewString(menu_name));
    XtSetValues(button->widget, args, (Cardinal) 1);
}


/*ARGSUSED*/
void DoRememberMenuSelection(widget, client_data, call_data)
    Widget	widget;		/* menu entry object */
    XtPointer	client_data;
    XtPointer	call_data;
{
    static Arg	args[] = {
	{ XtNpopupOnEntry,	(XtArgVal) NULL },
    };
    args[0].value = (XtArgVal) widget;
    XtSetValues(XtParent(widget), args, XtNumber(args));
}


void SendMenuEntryEnableMsg(button, entry_name, value)
    Button	button;
    char *	entry_name;
    int		value;
{
    Widget	entry;
    static Arg	args[] = {{XtNsensitive, (XtArgVal) NULL}};

    if ((entry = XtNameToWidget(button->menu, entry_name)) != NULL) {
	args[0].value = (XtArgVal) ((value == 0) ? False : True);
	XtSetValues(entry, args, (Cardinal) 1);
    }
}


void ToggleMenuItem(Widget entry, Boolean state)
{
    Arg		args[1];

    XtSetArg(args[0], XtNleftBitmap, (state ? MenuItemBitmap : None));
    XtSetValues(entry, args, (Cardinal) 1);
}

/*
 * $Xorg: svpopup.c,v 1.4 2001/02/09 02:05:30 xorgcvs Exp $
 *
Copyright 1989, 1998  The Open Group

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall not be
used in advertising or otherwise to promote the sale, use or other dealings
in this Software without prior written authorization from The Open Group.
 *
 * Author:  Chris D. Peterson, MIT X Consortium
 */
/* $XFree86: xc/programs/editres/svpopup.c,v 1.6 2001/12/14 20:00:43 dawes Exp $ */

#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>	/* Get standard string definations. */
#include <X11/Xatom.h>
#include <X11/cursorfont.h>
#include <X11/Shell.h>

#include <X11/Xaw/AsciiText.h>
#include <X11/Xaw/Cardinals.h>	
#include <X11/Xaw/Command.h>	
#include <X11/Xaw/Form.h>
#include <X11/Xaw/Label.h>	

#include <stdio.h>

#ifdef XKB
#include <X11/extensions/XKBbells.h>
#endif

#include "editresP.h"

static void _SetField ( Widget new, Widget old );
static void CreateSetValuesPopup ( Widget parent, ScreenData * scr_data );
static void DoSetValues ( Widget w, XtPointer junk, XtPointer garbage );
static void CancelSetValues ( Widget w, XtPointer junk, XtPointer garbage );

/*	Function Name: PopupSetValues
 *	Description: This function pops up the setvalues dialog
 *	Arguments: parent - the parent of the setvalues popup.
 *                 event - the event that caused this popup, or NULL.
 *	Returns: none
 */

/* ARGSUSED */
void
PopupSetValues(parent, event)
Widget parent;
XEvent * event;
{
    Arg args[1];

    if (global_tree_info == NULL) {
	SetMessage(global_screen_data.info_label,
		   res_labels[17]);
	return;
    }

/* 
 * Check and possibly create the popup.
 */

    if (global_screen_data.set_values_popup == NULL)
	CreateSetValuesPopup(parent, &global_screen_data);

/*
 * Clear out the old strings, and set the active widget to the name widget.
 */

    XtSetArg(args[0], XtNstring, "");
    XtSetValues(global_screen_data.res_text, args, ONE);
    XtSetValues(global_screen_data.val_text, args, ONE);

    _SetField(global_screen_data.res_text, global_screen_data.val_text);

/*
 * Pop it up.
 */

    PopupCentered(event, global_screen_data.set_values_popup, XtGrabNone);
}

/*	Function Name: ModifySVEntry
 *	Description: Action routine that can be bound to the set values 
 *                   dialog box's Text Widget that will send input to the 
 *                   field specified.
 *	Arguments:   (Standard Action Routine args) 
 *	Returns:     none.
 */

/* ARGSUSED */
void 
ModifySVEntry(w, event, params, num_params)
Widget w;
XEvent *event;
String * params;
Cardinal * num_params;
{
    Widget new, old;
    char msg[BUFSIZ];
    
    if (*num_params != 1) {
	strcpy(msg, 
	       res_labels[21]);
	SetMessage(global_screen_data.info_label, msg);
	return;
    }
    
    switch (params[0][0]) {
    case 'r':
    case 'R':
	new = global_screen_data.res_text;
	old = global_screen_data.val_text;
	break;
    case 'v':
    case 'V':
	new = global_screen_data.val_text;
	old = global_screen_data.res_text;
	break;
    default:
	sprintf(msg, res_labels[22]);
	SetMessage(global_screen_data.info_label, msg);
	return;
    }
    
    _SetField(new, old);
}

/************************************************************
 *
 * Private Functions
 *
 ************************************************************/

/*	Function Name: _SetField
 *	Description: Sets the current text entry field.
 *	Arguments: new, old - new and old text fields.
 *	Returns: none
 */

static void
_SetField(new, old)
Widget new, old;
{
    Arg args[2];
    Pixel new_border, old_border, old_bg;
    
    if (!XtIsSensitive(new)) {
#ifdef XKB
	/* Don't set field to an inactive Widget. */
	XkbStdBell(XtDisplay(old), XtWindow(new), 0, XkbBI_InvalidLocation); 
#else
	XBell(XtDisplay(old), 0); /* Don't set field to an inactive Widget. */
#endif
	return;
    }
    
    XtSetKeyboardFocus(XtParent(new), new); 
    
    XtSetArg(args[0], XtNborderColor, &old_border);
    XtSetArg(args[1], XtNbackground, &old_bg);
    XtGetValues(new, args, TWO);
    
    XtSetArg(args[0], XtNborderColor, &new_border);
    XtGetValues(old, args, ONE);
    
    if (old_border != old_bg)	/* Colors are already correct, return. */
	return;

    XtSetArg(args[0], XtNborderColor, old_border);
    XtSetValues(old, args, ONE);

    XtSetArg(args[0], XtNborderColor, new_border);
    XtSetValues(new, args, ONE);
}

/*	Function Name: CreateSetValuesPopup
 *	Description: Creates the setvalues popup.
 *	Arguments: parent - the parent of the popup.
 *                 scr_data - the data about this screen.
 *	Returns: the set values popup.
 */

static void
CreateSetValuesPopup(parent, scr_data)
Widget parent;
ScreenData * scr_data;
{
    Widget form, cancel, do_it, label;
    Widget res_label;
    Arg args[10];
    Cardinal num_args;
    
    scr_data->set_values_popup = XtCreatePopupShell("setValuesPopup", 
						    transientShellWidgetClass, 
						    parent, NULL, ZERO);

    form = XtCreateManagedWidget("form", formWidgetClass, 
				 scr_data->set_values_popup, NULL, ZERO);

    num_args = 0;
    label = XtCreateManagedWidget("label", labelWidgetClass,
				  form, args, num_args);


    num_args = 0;
    XtSetArg(args[num_args], XtNfromVert, label); num_args++;
    res_label = XtCreateManagedWidget("resourceLabel", labelWidgetClass,
				  form, args, num_args);

    num_args = 0;
    XtSetArg(args[num_args], XtNfromVert, label); num_args++;
    XtSetArg(args[num_args], XtNfromHoriz, res_label); num_args++;
    scr_data->res_text = XtCreateManagedWidget("resourceText", 
						  asciiTextWidgetClass,
						  form, args, num_args);

    num_args = 0;
    XtSetArg(args[num_args], XtNfromVert, scr_data->res_text); num_args++;
    (void)  XtCreateManagedWidget("valueLabel", labelWidgetClass,
				  form, args, num_args);

    num_args = 0;
    XtSetArg(args[num_args], XtNfromHoriz, res_label); num_args++;
    XtSetArg(args[num_args], XtNfromVert, scr_data->res_text); num_args++;
    scr_data->val_text = XtCreateManagedWidget("valueText", 
						  asciiTextWidgetClass,
						  form, args, num_args);
  
    num_args = 0;
    XtSetArg(args[num_args], XtNfromVert, scr_data->val_text); num_args++;
    do_it = XtCreateManagedWidget("setValues", commandWidgetClass, 
					  form, args, num_args);

    num_args = 0;
    XtSetArg(args[num_args], XtNfromVert, scr_data->val_text); num_args++;
    XtSetArg(args[num_args], XtNfromHoriz, do_it); num_args++;
    cancel = XtCreateManagedWidget("cancel", commandWidgetClass,
				   form, args, num_args);

    XtAddCallback(do_it, XtNcallback, DoSetValues, NULL);
    XtAddCallback(cancel, XtNcallback, CancelSetValues, NULL);

/*
 * Initialize the text entry fields.
 */

    {
	Pixel color;

	num_args = 0;
	XtSetArg(args[num_args], XtNbackground, &color); num_args++;
	XtGetValues(scr_data->val_text, args, num_args);

	num_args = 0;
	XtSetArg(args[num_args], XtNborderColor, color); num_args++;
	XtSetValues(scr_data->val_text, args, num_args);

	XtSetKeyboardFocus(form, scr_data->res_text);
    }
}

/*	Function Name: DoSetValues
 *	Description: Performs a SetValues.
 *	Arguments: w - the widget that called this.
 *                 junk, garbage - ** UNUSED **.
 *	Returns: none.
 */

/* ARGSUSED */
static void
DoSetValues(w, junk, garbage)
Widget w;
XtPointer junk, garbage;
{
    ProtocolStream * stream = &(global_client.stream);
    char *res_name, *res_value;
    Arg args[1];
    Cardinal i;

    if (global_tree_info->num_nodes == 0) {
	SetMessage(global_screen_data.info_label,
		   res_labels[23]);
	return;
    }
		
    XtSetArg(args[0], XtNstring, &res_name);
    XtGetValues(global_screen_data.res_text, args, ONE);

    XtSetArg(args[0], XtNstring, &res_value);
    XtGetValues(global_screen_data.val_text, args, ONE);
    
    _XEditResResetStream(stream);
    _XEditResPutString8(stream, res_name);
    _XEditResPutString8(stream, XtRString);
    _XEditResPutString8(stream, res_value);
    _XEditResPut16(stream, global_tree_info->num_nodes);

    for (i = 0; i < global_tree_info->num_nodes; i++) 
	InsertWidgetFromNode(stream, global_tree_info->active_nodes[i]);

    SetCommand(w, LocalSetValues, NULL);
}

/*	Function Name: CancelSetValues
 *	Description: Pops down the setvalues popup.
 *	Arguments: w - any grandchild of the popup.
 *                 junk, garbage - ** UNUSED **.
 *	Returns: none.
 */

/* ARGSUSED */
static void
CancelSetValues(w, junk, garbage)
Widget w;
XtPointer junk, garbage;
{
    XtPopdown(XtParent(XtParent(w))); 
}

/* $XConsortium: popup.c,v 2.38 94/08/26 18:04:22 swick Exp $
 *
 *
 *			  COPYRIGHT 1989
 *		   DIGITAL EQUIPMENT CORPORATION
 *		       MAYNARD, MASSACHUSETTS
 *			ALL RIGHTS RESERVED.
 *
 * THE INFORMATION IN THIS SOFTWARE IS SUBJECT TO CHANGE WITHOUT NOTICE AND
 * SHOULD NOT BE CONSTRUED AS A COMMITMENT BY DIGITAL EQUIPMENT CORPORATION.
 * DIGITAL MAKES NO REPRESENTATIONS ABOUT THE SUITABILITY OF THIS SOFTWARE FOR
 * ANY PURPOSE.  IT IS SUPPLIED "AS IS" WITHOUT EXPRESS OR IMPLIED WARRANTY.
 *
 * IF THE SOFTWARE IS MODIFIED IN A MANNER CREATING DERIVATIVE COPYRIGHT
 * RIGHTS, APPROPRIATE LEGENDS MAY BE PLACED ON THE DERIVATIVE WORK IN
 * ADDITION TO THAT SET FORTH ABOVE.
 *
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose and without fee is hereby granted, provided
 * that the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Digital Equipment Corporation not be
 * used in advertising or publicity pertaining to distribution of the software
 * without specific, written prior permission.
 */
/* $XFree86: xc/programs/xmh/popup.c,v 1.3 2002/07/01 02:26:05 tsi Exp $ */

/* popup.c -- Handle pop-up widgets. */

#include "xmh.h"
#include "actions.h"

#include <X11/Xaw/Cardinals.h>

typedef struct _PopupStatus {
	Widget popup;		/* order of fields same as CommandStatusRec */
	struct _LastInput lastInput;
	char*  shell_command;	/* NULL, or contains sh -c command */
} PopupStatusRec, *PopupStatus;

/* these are just strings which are used more than one place in the code */
static String XmhNconfirm = "confirm";
static String XmhNdialog = "dialog";
static String XmhNerror = "error";
static String XmhNnotice = "notice";
static String XmhNokay = "okay";
static String XmhNprompt = "prompt";
static String XmhNvalue = "value";
    
/* The popups were originally parented from toplevel and neglected the
 * transientFor resource.  In order not to break existing user resource
 * settings for the popups, transientFor is set independent of the parent,
 * which remains the toplevel widget.
 */

static void DeterminePopupPosition(
    Position	*x_ptr,
    Position	*y_ptr,
    Widget	*transFor_return) /* return a suitable top level shell */
{
    if (lastInput.win != (Window) -1) {
	if (transFor_return) {
	    Widget	source;
	    source = XtWindowToWidget(XtDisplay(toplevel), lastInput.win);
	    while (source && !XtIsWMShell(source))
		source = XtParent(source);
	    *transFor_return = source;
	}
	/* use the site of the last KeyPress or ButtonPress */
	*x_ptr = lastInput.x;
	*y_ptr = lastInput.y;
    } else {
	Widget	source;
	int i = 0;
	Dimension width, height;
	Arg args[2];

	/* %%% need to keep track of last screen */
	/* guess which screen and use the the center of it */
	while (i < numScrns && !scrnList[i]->mapped)
	    i++;
	source = ((i < numScrns) ? scrnList[i]->parent : toplevel);
	XtSetArg(args[0], XtNwidth, &width);
	XtSetArg(args[1], XtNheight, &height);
	XtGetValues(source, args, TWO);
	XtTranslateCoords(source, (Position) (width / 2),
			  (Position) (height / 2), x_ptr, y_ptr);
	if (transFor_return) *transFor_return = source;
    }
}

static Boolean PositionThePopup(
    Widget	popup,
    Position	x,
    Position	y)
{
    /* Hack.  Fix up the position of the popup.  The xmh app defaults file
     * contains an Xmh*Geometry specification; the effects of that on 
     * popups, and the lack of any user-supplied geometry specification for
     * popups, are mitigated here, by giving the popup shell a position.
     * (Xmh*Geometry is needed in case there is no user-supplied default.)
     * Returns True if an explicit geometry was inferred; false if the
     * widget was repositioned to (x,y).
     */

    Arg		args[4];
    String 	top_geom, pop_geom;

    XtSetArg( args[0], XtNgeometry, &top_geom );
    XtGetValues( toplevel, args, ONE );
    XtSetArg( args[0], XtNgeometry, &pop_geom );
    XtGetValues( popup, args, ONE );

    if (pop_geom == NULL || pop_geom == top_geom) {
	/* if same db entry, then ... */
	XtSetArg( args[0], XtNgeometry, (String) NULL);
	XtSetArg( args[1], XtNx, x);
	XtSetArg( args[2], XtNy, y);
	XtSetArg( args[3], XtNwinGravity, SouthWestGravity);
	XtSetValues( popup, args, FOUR);
	return False;
    }
    return True;
}


static void CenterPopupPosition(
    Widget	widget,
    Widget	popup,
    Position	px,
    Position	py)
{
    Position	x, y;
    Position	nx, ny;
    Arg		args[3];

    if (widget == NULL) return;
    XtSetArg(args[0], XtNx, &x);
    XtSetArg(args[1], XtNy, &y);
    XtGetValues(popup, args, TWO);
    if (x == px && y == py) {

	/* Program sets geometry.  Correct our earlier calculations. */

	nx = (GetWidth(widget) - GetWidth(popup)) / 2;
	ny = (GetHeight(widget) - GetHeight(popup)) / 2;
	if (nx < 0) nx = 0;
	if (ny < 0) ny = 0;
	XtTranslateCoords(widget, nx, ny, &x, &y);
	XtSetArg(args[0], XtNx, x);
	XtSetArg(args[1], XtNy, y);
	XtSetArg(args[2], XtNwinGravity, CenterGravity);
	XtSetValues(popup, args, THREE);
    }
}
	 

/* Insure that the popup is wholly showing on the screen.
   Optionally center the widget horizontally and/or vertically
   on current position.
 */

static void InsureVisibility(
    Widget	popup,
    Widget	popup_child,
    Position	x,		/* assert: current position = (x,y) */
    Position	y,
    Boolean	centerX,
    Boolean	centerY)
{
    Position	root_x, root_y;
    Dimension	width, height, border;
    Arg		args[3];


    XtSetArg( args[0], XtNwidth, &width );
    XtSetArg( args[1], XtNheight, &height );
    XtSetArg( args[2], XtNborderWidth, &border );
    XtGetValues( popup, args, THREE );

    XtTranslateCoords(popup_child, (Position)0, (Position)0, &root_x, &root_y);
    if (centerX) root_x -= width/2 + border;
    if (centerY) root_y -= height/2 + border;
    if (root_x < 0) root_x = 0;
    if (root_y < 0) root_y = 0;
    border <<= 1;

    if ((int)(root_x + width + border) > WidthOfScreen(XtScreen(toplevel))) {
	root_x = WidthOfScreen(XtScreen(toplevel)) - width - border;
    }
    if ((int)(root_y + height + border) > HeightOfScreen(XtScreen(toplevel))) {
	root_y = HeightOfScreen(XtScreen(toplevel)) - height - border;
    }

    if (root_x != x || root_y != y) {
	XtSetArg( args[0], XtNx, root_x );
	XtSetArg( args[1], XtNy, root_y );
	XtSetValues( popup, args, TWO );
    }
}


/*ARGSUSED*/
void DestroyPopup(
    Widget		widget,		/* unused */
    XtPointer		client_data,
    XtPointer		call_data)	/* unused */
{
    Widget		popup = (Widget) client_data;
    XtPopdown(popup);
    XtDestroyWidget(popup);
}

void WMDeletePopup(
    Widget	popup,	/* transient shell */
    XEvent*	event)
{
    String	shellName;
    String	buttonName;
    Widget	button;

    shellName = XtName(popup);
    if (strcmp(shellName, XmhNconfirm) == 0)
	buttonName = "*no";
    else if (strcmp(shellName, XmhNprompt) == 0)
	buttonName = "*cancel";
    else if (strcmp(shellName, XmhNnotice) == 0)
	buttonName = "*confirm";
    else if (strcmp(shellName, XmhNerror) == 0)
	buttonName = "*OK";
    else
	return;		/* WM may kill us */

    button = XtNameToWidget(popup, buttonName);
    if (! button) return;
    XtCallActionProc(button, "set", event, (String*)NULL, ZERO);
    XtCallActionProc(button, "notify", event, (String*)NULL, ZERO);
    XtCallActionProc(button, "unset", event, (String*)NULL, ZERO);
}

static void TheUsual(
    Widget	popup)	/* shell */
{
    XtInstallAllAccelerators(popup, popup);
    XtAugmentTranslations(popup, app_resources.wm_protocols_translations);
    XtRealizeWidget(popup);
    XDefineCursor(XtDisplay(popup), XtWindow(popup), app_resources.cursor);
    (void) XSetWMProtocols(XtDisplay(popup), XtWindow(popup), 
			   protocolList, XtNumber(protocolList));
}


/*ARGSUSED*/
void XmhPromptOkayAction(
    Widget	w,		/* the "value" widget in the Dialog box */
    XEvent	*event,		/* unused */
    String	*params,	/* unused */
    Cardinal	*num_params)	/* unused */
{
    XtCallCallbacks(XtNameToWidget(XtParent(w), XmhNokay), XtNcallback,
		    (XtPointer)XtParent(w));
}


void PopupPrompt(
    Widget		transientFor,	/* required to be a top-level shell */
    String		question,		/* the prompting string */
    XtCallbackProc	okayCallback)		/* CreateFolder() */
{
    Widget		popup;
    Widget		dialog;
    Widget		value;
    Position		x, y;
    Boolean		positioned;
    Arg			args[3];
    static XtTranslations PromptTextTranslations = NULL;

    DeterminePopupPosition(&x, &y, (Widget*)NULL);
    XtSetArg(args[0], XtNallowShellResize, True);
    XtSetArg(args[1], XtNinput, True);
    XtSetArg(args[2], XtNtransientFor, transientFor);
    popup = XtCreatePopupShell(XmhNprompt, transientShellWidgetClass, toplevel,
			       args, THREE);
    positioned = PositionThePopup(popup, x, y);

    XtSetArg(args[0], XtNlabel, question);
    XtSetArg(args[1], XtNvalue, "");
    dialog = XtCreateManagedWidget(XmhNdialog, dialogWidgetClass, popup, args,
				   TWO);
    XtSetArg(args[0], XtNresizable, True);
    XtSetValues( XtNameToWidget(dialog, "label"), args, ONE);
    value = XtNameToWidget(dialog, XmhNvalue);
    XtSetValues( value, args, ONE);
    if (! PromptTextTranslations)
	PromptTextTranslations = XtParseTranslationTable
	    ("<Key>Return: XmhPromptOkayAction()\n\
              Ctrl<Key>R:  no-op(RingBell)\n\
              Ctrl<Key>S:  no-op(RingBell)\n");
    XtOverrideTranslations(value, PromptTextTranslations);

    XawDialogAddButton(dialog, XmhNokay, okayCallback, (XtPointer) dialog);
    XawDialogAddButton(dialog, "cancel", DestroyPopup, (XtPointer) popup);
    TheUsual(popup);
    InsureVisibility(popup, dialog, x, y, !positioned, False);
    XtPopup(popup, XtGrabNone);
}


/* ARGSUSED */
static void FreePopupStatus(
    Widget w,			/* unused */
    XtPointer closure,
    XtPointer call_data)	/* unused */
{
    PopupStatus popup = (PopupStatus)closure;
    XtPopdown(popup->popup);
    XtDestroyWidget(popup->popup);
    if (popup->shell_command)
	XtFree(popup->shell_command);
    XtFree((char *) closure);
}


void PopupNotice(
    String		message,
    XtCallbackProc	callback,
    XtPointer		closure)
{
    PopupStatus popup_status = (PopupStatus)closure;
    Widget transientFor;
    Widget dialog;
    Widget value;
    Position x, y;
    Arg args[3];
    char command[65], label[128];

    if (popup_status == (PopupStatus)NULL) {
	popup_status = XtNew(PopupStatusRec);
	popup_status->lastInput = lastInput;
	popup_status->shell_command = (char*)NULL;
    }
    if (! popup_status->shell_command) {
	/* MH command */
	if (sscanf( message, "%64s", command ) != 1)
	    (void) strcpy( command, "system" );
	else {
	    int l = strlen(command);
	    if (l && command[--l] == ':')
		command[l] = '\0';
	}
	(void) sprintf( label, "%.64s command returned:", command );
    } else {
	/* arbitrary shell command */
	int len = strlen(popup_status->shell_command);
	(void) sprintf(label, "%.88s %s\nshell command returned:",
		       popup_status->shell_command,
		       ((len > 88) ? "[truncated]" : ""));
    }

    DeterminePopupPosition(&x, &y, &transientFor);
    XtSetArg( args[0], XtNallowShellResize, True );
    XtSetArg( args[1], XtNinput, True );
    XtSetArg( args[2], XtNtransientFor, transientFor);
    popup_status->popup = XtCreatePopupShell(XmhNnotice,
			     transientShellWidgetClass, toplevel, args, THREE);
    PositionThePopup(popup_status->popup, x, y);

    XtSetArg( args[0], XtNlabel, label );
    XtSetArg( args[1], XtNvalue, message );
    dialog = XtCreateManagedWidget(XmhNdialog, dialogWidgetClass,
				   popup_status->popup, args, TWO);

    /* The text area of the dialog box will not be editable. */
    value = XtNameToWidget(dialog, XmhNvalue);
    XtSetArg( args[0], XtNeditType, XawtextRead);
    XtSetArg( args[1], XtNdisplayCaret, False);
    XtSetValues( value, args, TWO);
    XtOverrideTranslations(value, NoTextSearchAndReplace);

    XawDialogAddButton( dialog, XmhNconfirm,
		       ((callback != (XtCallbackProc) NULL)
		          ? callback : (XtCallbackProc) FreePopupStatus), 
		       (XtPointer) popup_status
		      );

    TheUsual(popup_status->popup);
    InsureVisibility(popup_status->popup, dialog, x, y, False, False);
    XtPopup(popup_status->popup, XtGrabNone);
}


void PopupConfirm(
    Widget		center_widget,	/* where to center; may be NULL */
    String		question,
    XtCallbackList	affirm_callbacks,
    XtCallbackList	negate_callbacks)
{
    Widget	popup;
    Widget	dialog;
    Widget	button;
    Widget	transientFor;
    Position	x, y;
    Arg		args[3];
    static XtCallbackRec callbacks[] = {
	{DestroyPopup,		(XtPointer) NULL},
	{(XtCallbackProc) NULL,	(XtPointer) NULL}
    };

    DeterminePopupPosition(&x, &y, &transientFor);
    XtSetArg(args[0], XtNinput, True);
    XtSetArg(args[1], XtNallowShellResize, True);
    XtSetArg(args[2], XtNtransientFor, transientFor);
    popup = XtCreatePopupShell(XmhNconfirm, transientShellWidgetClass,
			       toplevel, args, THREE);
    PositionThePopup(popup, x, y); 

    XtSetArg(args[0], XtNlabel, question);
    dialog = XtCreateManagedWidget(XmhNdialog, dialogWidgetClass, popup, args,
				   ONE);
    
    callbacks[0].closure = (XtPointer) popup;
    XtSetArg(args[0], XtNcallback, callbacks);
    button = XtCreateManagedWidget("yes", commandWidgetClass, dialog, 
				   args, ONE);
    if (affirm_callbacks)
	XtAddCallbacks(button, XtNcallback, affirm_callbacks);

    button = XtCreateManagedWidget("no", commandWidgetClass, dialog, 
				   args, ZERO);
    XtAddCallback(button, XtNcallback, DestroyPopup, (XtPointer) popup);
    if (negate_callbacks)
	XtAddCallbacks(button, XtNcallback, negate_callbacks);

    TheUsual(popup);
    CenterPopupPosition(center_widget ? center_widget : transientFor,
			popup, x, y);
    InsureVisibility(popup, dialog, x, y, False, False);
    XtPopup(popup, XtGrabNone);
}


void PopupError(
    Widget	widget,	/* transient for this top-level shell, or NULL */
    String	message)
{
    Widget	transFor, error_popup, dialog;
    Position	x, y;
    Boolean	positioned;
    Arg		args[3];
    static XtCallbackRec callbacks[] = {
	{DestroyPopup,		(XtPointer) NULL},
	{(XtCallbackProc) NULL,	(XtPointer) NULL}
    };

    transFor = widget;
    DeterminePopupPosition(&x, &y, transFor ? (Widget*)NULL : &transFor);

    XtSetArg(args[0], XtNallowShellResize, True);
    XtSetArg(args[1], XtNinput, True);
    XtSetArg(args[2], XtNtransientFor, transFor);
    error_popup = XtCreatePopupShell(XmhNerror, transientShellWidgetClass,
				     toplevel, args, THREE);
    positioned = PositionThePopup(error_popup, x, y);

    XtSetArg(args[0], XtNlabel, message);
    dialog = XtCreateManagedWidget(XmhNdialog, dialogWidgetClass, error_popup,
				   args, ONE);
    callbacks[0].closure = (XtPointer) error_popup;
    XtSetArg(args[0], XtNcallback, callbacks);
    XawDialogAddButton(dialog, "OK", DestroyPopup, (XtPointer) error_popup);
    TheUsual(error_popup);
    InsureVisibility(error_popup, dialog, x, y, !positioned, !positioned);
    XtPopup(error_popup, XtGrabNone);
}

/*ARGSUSED*/
void PopupWarningHandler(
    String name,
    String type,
    String class,
    String msg,
    String *params,
    Cardinal *num)
{
    char *ptr;
    int i;
    String par[10];
    char message[500];
    char buffer[500];
    static Boolean allowPopup = True; /* protect against recursion */

    XtGetErrorDatabaseText(name, type, class, msg, buffer, 500);

    if (params && num && *num) {
	i = (*num <= 10) ? *num : 10;
	memmove( (char*)par, (char*)params, i * sizeof(String));
	bzero( &par[i], (10-i) * sizeof(String));
	if (*num > 10)
	    par[9] = "(truncated)";
	(void) sprintf(message, buffer, par[0], par[1], par[2], par[3],
		       par[4], par[5], par[6], par[7], par[8], par[9]);
	ptr = message;
    } else {
	ptr = buffer;
    }
    if (allowPopup) {
	allowPopup = False;
	PopupError((Widget)NULL, ptr); 
	allowPopup = True;
    } else {
	fprintf(stderr, ptr);
    }
}

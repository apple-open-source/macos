/*
 * $Xorg: xclipboard.c,v 1.4 2001/02/09 02:05:38 xorgcvs Exp $
 *
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
 * *
 * Author:  Ralph Swick, DEC/Project Athena
 * Updated for R4:  Chris D. Peterson,  MIT X Consortium.
 * Reauthored by: Keith Packard, MIT X Consortium.
 */
/* $XFree86: xc/programs/xclipboard/xclipboard.c,v 1.9 2003/05/27 22:26:59 tsi Exp $ */

#include <stdio.h>
#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <X11/Xatom.h>

#include <X11/Xmu/Atoms.h>
#include <X11/Xmu/StdSel.h>

#include <X11/Shell.h>
#include <X11/Xaw/Form.h>
#include <X11/Xaw/Label.h>
#include <X11/Xaw/Command.h>
#include <X11/Xaw/AsciiText.h>
#include <X11/Xaw/Dialog.h>
#include <X11/Xaw/Cardinals.h>
#include <X11/IntrinsicP.h>
#include <X11/Xaw/TextP.h>
#include <X11/Xfuncs.h>

#ifdef XKB
#include <X11/extensions/XKBbells.h>
#endif

#include <stdlib.h>

#define Command commandWidgetClass
#define Label	labelWidgetClass
#define Text    asciiTextWidgetClass

#define INFINITY 10000000	/* pretty big, huh? */

typedef struct _Clip {
    struct _Clip    *next, *prev;
    char	    *clip;
    char	    *filename;
    int		    avail;
} ClipRec, *ClipPtr;

static Atom wm_delete_window;
static Atom wm_protocols;

static void EraseTextWidget ( void );
static void NewCurrentClipContents ( char *data, int len );

static long 
TextLength(Widget w)
{
    return XawTextSourceScan (XawTextGetSource (w),
			      (XawTextPosition) 0,
 			      XawstAll, XawsdRight, 1, TRUE);
}

static void
SaveClip(Widget w, ClipPtr clip)
{
    Arg	    args[1];
    char    *data;
    int	    len;
    Widget  source;

    source = XawTextGetSource (w);
    XtSetArg (args[0], XtNstring, &data);
    XtGetValues (source, args, 1);
    len = strlen (data);
    if (len >= clip->avail)
    {
	if (clip->clip)
	    free (clip->clip);
	clip->clip = malloc (len + 1);
	if (!clip->clip)
	    clip->avail = 0;
	else
	    clip->avail = len + 1;
    }
    if (clip->avail)
    {
	strcpy (clip->clip, data);
    }
}

static void
RestoreClip(Widget w, ClipPtr clip)
{
    Arg	    args[1];
    Widget  source;

    source = XawTextGetSource (w);
    XtSetArg (args[0], XtNstring, clip->clip);
    XtSetValues (source, args, 1);
}

/*ARGSUSED*/
static ClipPtr 
NewClip(Widget w, ClipPtr old)
{
    ClipPtr newClip;

    newClip = (ClipPtr) malloc (sizeof (ClipRec));
    if (!newClip)
	return newClip;
    newClip->clip = 0;
    newClip->avail = 0;
    newClip->prev = old;
    newClip->next = NULL;
    newClip->filename = NULL;
    if (old)
    {
	newClip->next = old->next;
	old->next = newClip;
    }
    return newClip;
}

/*ARGSUSED*/
static void
DeleteClip(Widget w, ClipPtr clip)
{
    if (clip->prev)
	clip->prev->next = clip->next;
    if (clip->next)
	clip->next->prev = clip->prev;
    if (clip->clip)
	free (clip->clip);
    free ((char *) clip);
}

static ClipPtr	currentClip;
static Widget	top;
static Widget	text, nextButton, prevButton, indexLabel;
static Widget	fileDialog, fileDialogShell;
static Widget	failDialog, failDialogShell;

static int
IndexCurrentClip (void)
{
    int	i = 0;
    ClipPtr clip;

    for (clip = currentClip; clip; clip = clip->prev)
	i++;
    return i;
}

static void
set_button_state (void)
{
    Boolean prevvalid, nextvalid;
    Arg arg;
    char labelString[10];

    prevvalid = currentClip->prev != NULL;
    nextvalid = currentClip->next != NULL;
    XtSetArg (arg, XtNsensitive, prevvalid);
    XtSetValues (prevButton, &arg, ONE);
    XtSetArg (arg, XtNsensitive, nextvalid);
    XtSetValues (nextButton, &arg, ONE);
    sprintf (labelString, "%d", IndexCurrentClip ());
    XtSetArg (arg, XtNlabel, labelString);
    XtSetValues (indexLabel, &arg, ONE);
}

/* ARGSUSED */
static void
NextCurrentClip(Widget w, XEvent *ev, String *parms, Cardinal *np)
{
    if (currentClip->next)
    {
	SaveClip (text, currentClip);
	currentClip = currentClip->next;
	RestoreClip (text, currentClip);
	set_button_state ();
    }
}

/* ARGSUSED */
static void
PrevCurrentClip(Widget w, XEvent *ev, String *parms, Cardinal *np)
{
    if (currentClip->prev)
    {
	SaveClip (text, currentClip);
	currentClip = currentClip->prev;
	RestoreClip (text, currentClip);
	set_button_state ();
    }
}

/* ARGSUSED */
static void
DeleteCurrentClip(Widget w, XEvent *ev, String *parms, Cardinal *np)
{
    ClipPtr newCurrent;

    if (currentClip->prev)
	newCurrent = currentClip->prev;
    else
	newCurrent = currentClip->next;
    if (newCurrent)
    {
	DeleteClip (text, currentClip);
	currentClip = newCurrent;
	RestoreClip (text, currentClip);
    }
    else
	EraseTextWidget ();
    set_button_state ();
}

/* ARGSUSED */
static void
Quit(Widget w, XEvent *ev, String *parms, Cardinal *np)
{
    XtCloseDisplay  (XtDisplay (text));
    exit (0);
}

static void
CenterWidgetAtPoint(Widget w, int x, int y)
{
    Arg	args[2];
    Dimension	width, height;

    XtSetArg(args[0], XtNwidth, &width);
    XtSetArg(args[1], XtNheight, &height);
    XtGetValues (w, args, 2);
    x = x - (int) width / 2;
    y = y - (int) height / 2;
    if (x < 0)
	x = 0;
    else {
	int scr_width = WidthOfScreen (XtScreen(w));
	if (x + (int)width > scr_width)
	    x = scr_width - width;
    }
    if (y < 0)
	y = 0;
    else {
	int scr_height = HeightOfScreen (XtScreen(w));
	if (y + (int)height > scr_height)
	    y = scr_height - height;
    }
    XtSetArg(args[0], XtNx, x);
    XtSetArg(args[1], XtNy, y);
    XtSetValues (w, args, 2);
}

static void
CenterWidgetOnEvent(Widget w, XEvent *e)
{
    CenterWidgetAtPoint (w, e->xbutton.x_root, e->xbutton.y_root);
}

static void
CenterWidgetOnWidget(Widget w, Widget wT)
{
    Position	rootX, rootY;
    Dimension	width, height;
    Arg		args[2];

    XtSetArg (args[0], XtNwidth, &width);
    XtSetArg (args[1], XtNheight, &height);
    XtGetValues (wT, args, 2);
    XtTranslateCoords (wT, (Position) width/2, (Position) height/2, &rootX, &rootY);
    CenterWidgetAtPoint (w, (int) rootX, (int) rootY);
}

/*ARGSUSED*/
static void
SaveToFile(Widget w, XEvent *e, String *argv, Cardinal *argc)
{
    Arg	    args[1];
    char    *filename;

    filename = "clipboard";
    if (currentClip->filename)
	filename = currentClip->filename;
    XtSetArg(args[0], XtNvalue, filename);
    XtSetValues (fileDialog, args, 1);
    CenterWidgetOnEvent (fileDialogShell, e);
    XtPopup (fileDialogShell, XtGrabNone);
}

/*ARGSUSED*/
static void
AcceptSaveFile(Widget w, XEvent *e, String *argv, Cardinal *argc)
{
    char    *filename;
    Boolean success;
    Arg	    args[1];

    filename = XawDialogGetValueString (fileDialog);
    success = XawAsciiSaveAsFile (XawTextGetSource (text), filename);
    XtPopdown (fileDialogShell);
    if (!success)
    {
	char	failMessage[1024];

	sprintf (failMessage, "Can't open file \"%s\"", filename);
	XtSetArg (args[0], XtNlabel, failMessage);
	XtSetValues (failDialog, args, 1);
	CenterWidgetOnEvent (failDialogShell, e);
	XtPopup (failDialogShell, XtGrabNone);
    }
    else
    {
	if (currentClip->filename)
	    free (currentClip->filename);
	currentClip->filename = malloc (strlen (filename) + 1);
	if (currentClip->filename)
	    strcpy (currentClip->filename, filename);
    }
}

/* ARGSUSED */
static void
CancelSaveFile(Widget w, XEvent *ev, String *parms, Cardinal *np)
{
    XtPopdown (fileDialogShell);
}

/* ARGSUSED */
static void
FailContinue(Widget w, XEvent *ev, String *parms, Cardinal *np)
{
    XtPopdown (failDialogShell);
}

/*ARGSUSED*/
static void 
WMProtocols(Widget w, XEvent *ev, String *params, Cardinal *n)
{
    if (ev->type == ClientMessage &&
	ev->xclient.message_type == wm_protocols &&
	ev->xclient.data.l[0] == (long) wm_delete_window) {
	while (w && !XtIsShell(w))
	    w = XtParent(w);
	if (w == top)
	    Quit(w, ev, params, n);
	else if (w == fileDialogShell)
	    CancelSaveFile(w, ev, params, n);
	else if (w == failDialogShell)
	    FailContinue(w, ev, params, n);
    }
}

/* ARGUSED */
static void
NewCurrentClip(Widget w, XEvent *ev, String *parms, Cardinal *np)
{
    NewCurrentClipContents ("", 0);
}

static void
NewCurrentClipContents(char *data, int len)
{
    XawTextBlock textBlock;

    SaveClip (text, currentClip);

    /* append new clips at the end */
    while (currentClip && currentClip->next)
	currentClip = currentClip->next;
    /* any trailing clips with no text get overwritten */
    if (strlen (currentClip->clip) != 0)
	currentClip = NewClip (text, currentClip);
    
    textBlock.ptr = data;
    textBlock.firstPos = 0;
    textBlock.length = len;
    textBlock.format = FMT8BIT;
    if (XawTextReplace(text, 0, TextLength (text), &textBlock)) {
#ifdef XKB
	XkbStdBell(XtDisplay(text), XtWindow(text), 0, XkbBI_Info);
#else
	XBell( XtDisplay(text), 0);
#endif
    }
    set_button_state ();
}

static void
EraseTextWidget(void)
{
    XawTextBlock block;

    block.ptr = "";
    block.length = 0;
    block.firstPos = 0;
    block.format = FMT8BIT;

    XawTextReplace(text, 0, INFINITY, &block);
    /* If this fails, too bad. */
}


XtActionsRec xclipboard_actions[] = {
    { "NewClip", 	NewCurrentClip }, 
    { "NextClip",	NextCurrentClip },
    { "PrevClip",	PrevCurrentClip },
    { "DeleteClip",	DeleteCurrentClip },
    { "Save",		SaveToFile },
    { "AcceptSave",	AcceptSaveFile },
    { "CancelSave",	CancelSaveFile },
    { "FailContinue",	FailContinue },
    { "Quit",		Quit },
    { "WMProtocols",	WMProtocols }
};

static XrmOptionDescRec table[] = {
    {"-w",	    "wrap",		XrmoptionNoArg,  "on"},
/*    {"-nw",	    "wrap",		XrmoptionNoArg,  "False"} */
};

static Boolean ConvertSelection ( Widget w, Atom *selection, Atom *target, 
				  Atom *type, XtPointer *value, 
				  unsigned long *length, int *format );
static void LoseSelection ( Widget w, Atom *selection );

static Atom	ManagerAtom, ClipboardAtom;

/*ARGSUSED*/
static void 
InsertClipboard(Widget w, XtPointer client_data, Atom *selection, 
		Atom *type, XtPointer value, unsigned long *length, 
		int *format)
{
    if (*type != XT_CONVERT_FAIL)
	NewCurrentClipContents ((char *) value, *length);
    else
    {
	Arg arg;
	XtSetArg (arg, XtNlabel, "CLIPBOARD selection conversion failed");
	XtSetValues (failDialog, &arg, 1);
	CenterWidgetOnWidget (failDialogShell, text);
	XtPopup (failDialogShell, XtGrabNone);
#ifdef XKB
	XkbStdBell( XtDisplay(w), XtWindow(w), 0, XkbBI_MinorError );
#else
	XBell( XtDisplay(w), 0 );
#endif
    }
    
    XtOwnSelection(top, ClipboardAtom, CurrentTime,
		   ConvertSelection, LoseSelection, NULL);
    XFree(value);
}

static Boolean 
ConvertSelection(Widget w, Atom *selection, Atom *target,
		 Atom *type, XtPointer *value, unsigned long *length, 
		 int *format)
{
    Display* d = XtDisplay(w);
    XSelectionRequestEvent* req =
	XtGetSelectionRequest(w, *selection, (XtRequestId)NULL);

    if (*target == XA_TARGETS(d)) {
	Atom* targetP;
	Atom* std_targets;
	unsigned long std_length;
	XmuConvertStandardSelection(w, req->time, selection, target, type,
				    (XPointer*)&std_targets, &std_length,
				    format);
	*value = XtMalloc(sizeof(Atom)*(std_length + 5));
	targetP = *(Atom**)value;
	*targetP++ = XA_STRING;
	*targetP++ = XA_TEXT(d);
	*targetP++ = XA_LENGTH(d);
	*targetP++ = XA_LIST_LENGTH(d);
	*targetP++ = XA_CHARACTER_POSITION(d);
	*length = std_length + (targetP - (*(Atom **) value));
	memmove( (char*)targetP, (char*)std_targets, sizeof(Atom)*std_length);
	XtFree((char*)std_targets);
	*type = XA_ATOM;
	*format = 32;
	return True;
    }

    if (*target == XA_LIST_LENGTH(d) ||
	*target == XA_LENGTH(d))
    {
    	long * temp;
    	
    	temp = (long *) XtMalloc(sizeof(long));
    	if (*target == XA_LIST_LENGTH(d))
      	  *temp = 1L;
    	else			/* *target == XA_LENGTH(d) */
      	  *temp = (long) TextLength (text);
    	
    	*value = (XPointer) temp;
    	*type = XA_INTEGER;
    	*length = 1L;
    	*format = 32;
    	return True;
    }
    
    if (*target == XA_CHARACTER_POSITION(d))
    {
    	long * temp;
    	
    	temp = (long *) XtMalloc(2 * sizeof(long));
    	temp[0] = (long) 0;
    	temp[1] = TextLength (text);
    	*value = (XPointer) temp;
    	*type = XA_SPAN(d);
    	*length = 2L;
    	*format = 32;
    	return True;
    }
    
    if (*target == XA_STRING ||
      *target == XA_TEXT(d) ||
      *target == XA_COMPOUND_TEXT(d))
    {
    	if (*target == XA_COMPOUND_TEXT(d))
	    *type = *target;
    	else
	    *type = XA_STRING;
	*length = TextLength (text);
    	*value = _XawTextGetSTRING((TextWidget) text, 0, *length);
    	*format = 8;
    	return True;
    }
    
    if (XmuConvertStandardSelection(w, req->time, selection, target, type,
				    (XPointer *) value, length, format))
	return True;

    return False;
}

static void 
LoseSelection(Widget w, Atom *selection)
{
    XtGetSelectionValue(w, *selection, XA_STRING, InsertClipboard,
			NULL, CurrentTime);
}

/*ARGSUSED*/
static Boolean 
RefuseSelection(Widget w, Atom *selection, Atom *target,
		Atom *type, XtPointer *value, unsigned long *length, 
		int *format)
{
    return False;
}

/*ARGSUSED*/
static void 
LoseManager(Widget w, Atom *selection)
{
    XtError("another clipboard has taken over control\n");
}

typedef struct {
  Boolean wrap;
} ResourceData, *ResourceDataPtr;

static ResourceData userOptions;

#define Offset(field) XtOffsetOf(ResourceData, field)

XtResource resources[] = {
  {"wrap", "Wrap", XtRBoolean, sizeof(Boolean),
     Offset(wrap), XtRImmediate, (XtPointer)False}
};

#undef Offset

int
main(int argc, char *argv[])
{
    Arg args[4];
    Cardinal n;
    XtAppContext xtcontext;
    Widget parent;

    XtSetLanguageProc(NULL, NULL, NULL);

    top = XtAppInitialize( &xtcontext, "XClipboard", table, XtNumber(table),
			  &argc, argv, NULL, NULL, 0);

    XtGetApplicationResources(top, (XtPointer)&userOptions, resources, 
			      XtNumber(resources), NULL, 0);

    XtAppAddActions (xtcontext,
		     xclipboard_actions, XtNumber (xclipboard_actions));
    /* CLIPBOARD_MANAGER is a non-standard mechanism */
    ManagerAtom = XInternAtom(XtDisplay(top), "CLIPBOARD_MANAGER", False);
    ClipboardAtom = XA_CLIPBOARD(XtDisplay(top));
    if (XGetSelectionOwner(XtDisplay(top), ManagerAtom))
	XtError("another clipboard is already running\n");

    parent = XtCreateManagedWidget("form", formWidgetClass, top, NULL, ZERO);
    (void) XtCreateManagedWidget("quit", Command, parent, NULL, ZERO);
    (void) XtCreateManagedWidget("delete", Command, parent, NULL, ZERO);
    (void) XtCreateManagedWidget("new", Command, parent, NULL, ZERO);
    (void) XtCreateManagedWidget("save", Command, parent, NULL, ZERO);
    nextButton = XtCreateManagedWidget("next", Command, parent, NULL, ZERO);
    prevButton = XtCreateManagedWidget("prev", Command, parent, NULL, ZERO);
    indexLabel = XtCreateManagedWidget("index", Label, parent, NULL, ZERO);

    n=0;
    XtSetArg(args[n], XtNtype, XawAsciiString); n++;
    XtSetArg(args[n], XtNeditType, XawtextEdit); n++;
    if (userOptions.wrap) {
	XtSetArg(args[n], XtNwrap, XawtextWrapWord); n++;
	XtSetArg(args[n], XtNscrollHorizontal, False); n++;
    }

    text = XtCreateManagedWidget( "text", Text, parent, args, n);
    
    currentClip = NewClip (text, (ClipPtr) 0);

    set_button_state ();

    fileDialogShell = XtCreatePopupShell("fileDialogShell",
					 transientShellWidgetClass,
					 top, NULL, ZERO);
    fileDialog = XtCreateManagedWidget ("fileDialog", dialogWidgetClass,
					fileDialogShell, NULL, ZERO);
    XawDialogAddButton(fileDialog, "accept", NULL, NULL);
    XawDialogAddButton(fileDialog, "cancel", NULL, NULL);

    failDialogShell = XtCreatePopupShell("failDialogShell",
					 transientShellWidgetClass,
					 top, NULL, ZERO);
    failDialog = XtCreateManagedWidget ("failDialog", dialogWidgetClass,
					failDialogShell, NULL, ZERO);
    XawDialogAddButton (failDialog, "continue", NULL, NULL);

    XtRealizeWidget(top);
    XtRealizeWidget(fileDialogShell);
    XtRealizeWidget(failDialogShell);
    XtOwnSelection(top, ManagerAtom, CurrentTime,
		   RefuseSelection, LoseManager, NULL);
    if (XGetSelectionOwner (XtDisplay(top), ClipboardAtom)) {
	LoseSelection (top, &ClipboardAtom);
    } else {
    	XtOwnSelection(top, ClipboardAtom, CurrentTime,
		       ConvertSelection, LoseSelection, NULL);
    }
    wm_delete_window = XInternAtom(XtDisplay(top), "WM_DELETE_WINDOW", False);
    wm_protocols = XInternAtom(XtDisplay(top), "WM_PROTOCOLS", False);
    (void) XSetWMProtocols(XtDisplay(top), XtWindow(top), &wm_delete_window,1);
    (void) XSetWMProtocols(XtDisplay(top), XtWindow(fileDialogShell),
			   &wm_delete_window,1);
    (void) XSetWMProtocols(XtDisplay(top), XtWindow(failDialogShell),
			   &wm_delete_window,1);
    XtAppMainLoop(xtcontext);
    exit(0);
}

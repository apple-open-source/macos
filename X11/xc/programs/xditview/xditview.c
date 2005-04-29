/* $XConsortium: xditview.c,v 1.32 94/04/17 20:43:36 eswu Exp $ */
/*

Copyright (c) 1991  X Consortium

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE X CONSORTIUM BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of the X Consortium shall
not be used in advertising or otherwise to promote the sale, use or
other dealings in this Software without prior written authorization
from the X Consortium.

*/
/* $XFree86: xc/programs/xditview/xditview.c,v 1.5 2003/05/27 22:27:00 tsi Exp $ */
/*
 * xditview -- 
 *
 *   Display ditroff output in an X window
 */

#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <X11/Xatom.h>
#include <X11/Shell.h>
#include <X11/Xos.h>		/* rindex declaration */
#include <X11/Xaw/Paned.h>
#include <X11/Xaw/Panner.h>
#include <X11/Xaw/Porthole.h>
#include <X11/Xaw/Viewport.h>
#include <X11/Xaw/Box.h>
#include <X11/Xaw/Command.h>
#include <X11/Xaw/Dialog.h>
#include <X11/Xaw/Label.h>
#include <X11/Xaw/MenuButton.h>
#include <X11/Xaw/SimpleMenu.h>
#include <X11/Xaw/SmeBSB.h>
#include <X11/Xaw/AsciiText.h>

#include "Dvi.h"

#include "xdit.bm"
#include "xdit_mask.bm"
#include <stdio.h>
#include <stdlib.h>

/* Command line options table.  Only resources are entered here...there is a
   pass over the remaining options after XtParseCommand is let loose. */

static XrmOptionDescRec options[] = {
{"-page",	    "*dvi.pageNumber",	    XrmoptionSepArg,	NULL},
{"-backingStore",   "*dvi.backingStore",    XrmoptionSepArg,	NULL},
{"-noPolyText",	    "*dvi.noPolyText",	    XrmoptionNoArg,	"TRUE"},
{"-resolution",	    "*dvi.screenResolution",XrmoptionSepArg,    NULL},
};

static char	current_file_name[1024];
static FILE	*current_file;

static void MakePrompt(Widget, char *, void (*)(char *), char *);

/*
 * Report the syntax for calling xditview.
 */

static void
Syntax(char *call)
{
	(void) printf ("Usage: %s [-fg <color>] [-bg <color>]\n", call);
	(void) printf ("       [-bd <color>] [-bw <pixels>] [-help]\n");
	(void) printf ("       [-display displayname] [-geometry geom]\n");
	(void) printf ("       [-page <page-number>] [-backing <backing-store>]\n");
	(void) printf ("       [-resolution <screen-resolution>]\n\n");
	exit(1);
}

static void	NewResolution (char *resString);
static void	NewFile (char *name);
static void	DisplayPageNumber (void);
static void	VisitFile (char *name, Boolean resetPage);
static Widget	toplevel, paned, porthole, dvi;
#ifdef NOTDEF
static Widget	form, panner;
#endif
static Widget	popupMenu;
static Widget	menuBar;
static Widget	fileMenuButton, fileMenu;
static Widget	pageNumber;

static void	NextPage(Widget entry, XtPointer name, XtPointer data);
static void	PreviousPage(Widget entry, XtPointer name, XtPointer data);
static void	SetResolution(Widget entry, XtPointer name, XtPointer data);
static void	OpenFile(Widget entry, XtPointer name, XtPointer data);
static void	RevisitFile(Widget entry, XtPointer name, XtPointer data);
static void	Quit(Widget entry, XtPointer closure, XtPointer data);

struct menuEntry {
    char    *name;
    void    (*function)(Widget entry, XtPointer name, XtPointer data);
};

static struct menuEntry popupMenuEntries[] = {
    { "nextPage",	    NextPage },
    { "previousPage",	    PreviousPage },
    { "setResolution",	    SetResolution },
    { "openFile",	    OpenFile },
    { "revisitFile",	    RevisitFile },
    { "quit",		    Quit }
};

static struct menuEntry fileMenuEntries[] = {
    { "openFile",	    OpenFile },
    { "revisitFile",	    RevisitFile },
    { "setResolution",	    SetResolution },
    { "quit",		    Quit }
};

static void	NextPageAction(Widget, XEvent *, String *, Cardinal *);
static void	PreviousPageAction(Widget, XEvent *, String *, Cardinal *);
static void	SetResolutionAction(Widget, XEvent *, String *, Cardinal *);
static void	OpenFileAction(Widget, XEvent *, String *, Cardinal *);
static void	RevisitFileAction(Widget, XEvent *, String *, Cardinal *);
static void	QuitAction(Widget, XEvent *, String *, Cardinal *);
static void	AcceptAction(Widget, XEvent *, String *, Cardinal *);
static void	CancelAction(Widget, XEvent *, String *, Cardinal *);
static void	UpdatePageNumber(Widget, XEvent *, String *, Cardinal *);
static void	Noop(Widget, XEvent *, String *, Cardinal *);

XtActionsRec xditview_actions[] = {
    { "NextPage",	    NextPageAction },
    { "PreviousPage",	    PreviousPageAction },
    { "SetResolution",	    SetResolutionAction },
    { "OpenFile",	    OpenFileAction },
    { "Quit",		    QuitAction },
    { "Accept",		    AcceptAction },
    { "Cancel",		    CancelAction },
    { "SetPageNumber",	    UpdatePageNumber },
    { "Noop",		    Noop }
};

static Atom wm_delete_window;

#ifdef NOTDEF
/*	Function Name: PannerCallback
 *	Description: called when the panner has moved.
 *	Arguments: panner - the panner widget.
 *                 closure - *** NOT USED ***.
 *                 report_ptr - the panner record.
 *	Returns: none.
 */

/* ARGSUSED */
static void 
PannerCallback(Widget w, XtPointer closure, XtPointer report_ptr)
{
    Arg args[2];
    XawPannerReport *report = (XawPannerReport *) report_ptr;

    if (!dvi)
	return;
    XtSetArg (args[0], XtNx, -report->slider_x);
    XtSetArg (args[1], XtNy, -report->slider_y);

    XtSetValues(dvi, args, 2);
}

/*	Function Name: PortholeCallback
 *	Description: called when the porthole or its child has
 *                   changed 
 *	Arguments: porthole - the porthole widget.
 *                 panner_ptr - the panner widget.
 *                 report_ptr - the porthole record.
 *	Returns: none.
 */

/* ARGSUSED */
static void 
PortholeCallback(Widget w, XtPointer panner_ptr, XtPointer report_ptr)
{
    Arg args[10];
    Cardinal n = 0;
    XawPannerReport *report = (XawPannerReport *) report_ptr;
    Widget panner = (Widget) panner_ptr;

    XtSetArg (args[n], XtNsliderX, report->slider_x); n++;
    XtSetArg (args[n], XtNsliderY, report->slider_y); n++;
    if (report->changed != (XawPRSliderX | XawPRSliderY)) {
	XtSetArg (args[n], XtNsliderWidth, report->slider_width); n++;
	XtSetArg (args[n], XtNsliderHeight, report->slider_height); n++;
	XtSetArg (args[n], XtNcanvasWidth, report->canvas_width); n++;
	XtSetArg (args[n], XtNcanvasHeight, report->canvas_height); n++;
    }
    XtSetValues (panner, args, n);
}
#endif

int
main(int argc, char **argv)
{
    char	    *file_name = 0;
    int		    i;
    XtAppContext    xtcontext;
    Arg		    topLevelArgs[2];
    Widget          entry;

    XtSetLanguageProc(NULL, (XtLanguageProc) NULL, NULL);

    toplevel = XtAppInitialize(&xtcontext, "Xditview",
			       options, XtNumber (options),
			       &argc, argv, NULL, NULL, 0);
    if (argc > 2)
	Syntax(argv[0]);

    XtAppAddActions(xtcontext, xditview_actions, XtNumber (xditview_actions));
    XtOverrideTranslations
	(toplevel, XtParseTranslationTable ("<Message>WM_PROTOCOLS: Quit()"));

    XtSetArg (topLevelArgs[0], XtNiconPixmap,
	      XCreateBitmapFromData (XtDisplay (toplevel),
				     XtScreen(toplevel)->root,
				     (char *) xdit_bits,
				     xdit_width, xdit_height));
				    
    XtSetArg (topLevelArgs[1], XtNiconMask,
	      XCreateBitmapFromData (XtDisplay (toplevel),
				     XtScreen(toplevel)->root,
				     (char *) xdit_mask_bits, 
				     xdit_mask_width, xdit_mask_height));
    XtSetValues (toplevel, topLevelArgs, 2);
    if (argc > 1)
	file_name = argv[1];

    /*
     * create the popup menu and insert the entries
     */
    popupMenu = XtCreatePopupShell ("popupMenu", simpleMenuWidgetClass, toplevel,
				    NULL, 0);
    for (i = 0; i < XtNumber (popupMenuEntries); i++) {
	entry = XtCreateManagedWidget(popupMenuEntries[i].name, 
				      smeBSBObjectClass, popupMenu,
				      NULL, (Cardinal) 0);
	XtAddCallback(entry, XtNcallback, popupMenuEntries[i].function, NULL);
    }

    paned = XtCreateManagedWidget("paned", panedWidgetClass, toplevel,
				    NULL, (Cardinal) 0);
    menuBar = XtCreateManagedWidget ("menuBar", boxWidgetClass, paned, 0, 0);

    fileMenuButton = XtCreateManagedWidget ("fileMenuButton", menuButtonWidgetClass,
				    menuBar, NULL, (Cardinal) 0);
    fileMenu = XtCreatePopupShell ("fileMenu", simpleMenuWidgetClass,
				    fileMenuButton, NULL, (Cardinal) 0);
    for (i = 0; i < XtNumber (fileMenuEntries); i++) {
	entry = XtCreateManagedWidget(fileMenuEntries[i].name,
				      smeBSBObjectClass, fileMenu,
				      NULL, (Cardinal) 0);
	XtAddCallback (entry, XtNcallback, fileMenuEntries[i].function, NULL);
    }

    (void) XtCreateManagedWidget ("prevButton", commandWidgetClass,
				  menuBar, NULL, (Cardinal) 0);

    pageNumber = XtCreateManagedWidget("pageNumber", asciiTextWidgetClass,
					menuBar, NULL, (Cardinal) 0);
  
    (void) XtCreateManagedWidget ("nextButton", commandWidgetClass,
				  menuBar, NULL, (Cardinal) 0);

#ifdef NOTDEF
    form = XtCreateManagedWidget ("form", formWidgetClass, paned,
				    NULL, (Cardinal) 0);
    panner = XtCreateManagedWidget ("panner", pannerWidgetClass,
				    form, NULL, 0);
    porthole = XtCreateManagedWidget ("porthole", portholeWidgetClass,
				      form, NULL, 0);
    XtAddCallback(porthole, 
		  XtNreportCallback, PortholeCallback, (XtPointer) panner);
    XtAddCallback(panner, 
		  XtNreportCallback, PannerCallback, (XtPointer) porthole);
#else
    porthole = XtCreateManagedWidget ("viewport", viewportWidgetClass,
				      paned, NULL, 0);
#endif
    dvi = XtCreateManagedWidget ("dvi", dviWidgetClass, porthole, NULL, 0);
    if (file_name)
	VisitFile (file_name, FALSE);
    XtRealizeWidget (toplevel);
    wm_delete_window = XInternAtom(XtDisplay(toplevel), "WM_DELETE_WINDOW",
				   False);
    (void) XSetWMProtocols (XtDisplay(toplevel), XtWindow(toplevel),
                            &wm_delete_window, 1);
    XtAppMainLoop(xtcontext);

    return 0;
}

static void
DisplayPageNumber ()
{
    Arg	arg[2];
    int	actual_number, last_page;
    XawTextBlock    text;
    int		    length;
    char	    value[128];
    char	    *cur;

    XtSetArg (arg[0], XtNpageNumber, &actual_number);
    XtSetArg (arg[1], XtNlastPageNumber, &last_page);
    XtGetValues (dvi, arg, 2);
    if (actual_number == 0)
	sprintf (value, "<none>");
    else if (last_page > 0)
	sprintf (value, "%d of %d", actual_number, last_page);
    else
	sprintf (value, "%d", actual_number);
    text.firstPos = 0;
    text.length = strlen (value);
    text.ptr = value;
    text.format = FMT8BIT;
    XtSetArg (arg[0], XtNstring, &cur);
    XtGetValues (XawTextGetSource (pageNumber), arg, 1);
    length = strlen (cur);
    XawTextReplace (pageNumber, 0, length, &text);
}

static void
SetPageNumber (int number)
{
    Arg	arg[1];

    XtSetArg (arg[0], XtNpageNumber, number);
    XtSetValues (dvi, arg, 1);
    DisplayPageNumber ();
}

static void
UpdatePageNumber (Widget w, XEvent *xev, String *s, Cardinal *c)
{
    char    *string;
    Arg	    arg[1];

    XtSetArg (arg[0], XtNstring, &string);
    XtGetValues (XawTextGetSource(pageNumber), arg, 1);
    SetPageNumber (atoi(string));
}

static void
NewResolution(resString)
char	*resString;
{
    int	res;
    Arg	arg[1];
    
    res = atoi (resString);
    if (res <= 0)
	return;
    XtSetArg (arg[0], XtNscreenResolution, res);
    XtSetValues (dvi, arg, 1);
}

static void
VisitFile (char *name, Boolean resetPage)
{
    Arg	    arg[3];
    char    *n;
    FILE    *new_file;
    Boolean seek = 0;
    int	    i;

    if (current_file) {
	if (!strcmp (current_file_name, "-"))
	    ;
	else if (current_file_name[0] == '|')
	    pclose (current_file);
	else
	    fclose (current_file);
    }
    if (!strcmp (name, "-"))
	new_file = stdin;
    else if (name[0] == '|')
	new_file = popen (name+1, "r");
    else {
	new_file = fopen (name, "r");
	seek = 1;
    }
    if (!new_file) {
	/* XXX display error message */
	return;
    }
    i = 0;
    XtSetArg (arg[i], XtNfile, new_file); i++;
    XtSetArg (arg[i], XtNseek, seek); i++;
    if (resetPage) {
	XtSetArg (arg[i], XtNpageNumber, 1); i++;
    }
    XtSetValues (dvi, arg, i);
    XtSetArg (arg[0], XtNtitle, name);
    if (name[0] != '/' && (n = rindex (name, '/')))
	n = n + 1;
    else
	n = name;
    XtSetArg (arg[1], XtNiconName, n);
    XtSetValues (toplevel, arg, 2);
    strcpy (current_file_name, name);
    current_file = new_file;
    DisplayPageNumber ();
}

static void
NewFile (name)
char	*name;
{
    VisitFile (name, TRUE);
}

static char fileBuf[1024];
static char resolutionBuf[1024];

static void
ResetMenuEntry (Widget entry)
{
    Arg	arg[1];

    XtSetArg (arg[0], XtNpopupOnEntry, entry);
    XtSetValues (XtParent(entry) , arg, (Cardinal) 1);
}

/*ARGSUSED*/
static void
NextPage (entry, name, data)
    Widget  entry;
    XtPointer name, data;
{
    NextPageAction(entry, NULL, NULL, NULL);
    ResetMenuEntry (entry);
}

static void
NextPageAction (Widget w, XEvent *xev, String *s, Cardinal *c)
{
    Arg	args[1];
    int	number;

    XtSetArg (args[0], XtNpageNumber, &number);
    XtGetValues (dvi, args, 1);
    SetPageNumber (number+1);
}

/*ARGSUSED*/
static void
PreviousPage (entry, name, data)
    Widget  entry;
    XtPointer name, data;
{
    PreviousPageAction (entry, NULL, NULL, NULL);
    ResetMenuEntry (entry);
}

static void
PreviousPageAction (Widget w, XEvent *xev, String *s, Cardinal *c)
{
    Arg	args[1];
    int	number;

    XtSetArg (args[0], XtNpageNumber, &number);
    XtGetValues (dvi, args, 1);
    SetPageNumber (number-1);
}

/*ARGSUSED*/
static void
SetResolution (entry, name, data)
    Widget  entry;
    XtPointer name, data;
{
    SetResolutionAction (entry, NULL, NULL, NULL);
    ResetMenuEntry (entry);
}

static void
SetResolutionAction (Widget w, XEvent *xev, String *s, Cardinal *c)
{
    Arg	    args[1];
    int	    cur;

    XtSetArg (args[0], XtNscreenResolution, &cur);
    XtGetValues (dvi, args, 1);
    sprintf (resolutionBuf, "%d", cur);
    MakePrompt (toplevel, "Screen resolution:", NewResolution, resolutionBuf);
}

/*ARGSUSED*/
static void
OpenFile (entry, name, data)
    Widget  entry;
    XtPointer name, data;
{
    OpenFileAction (entry, NULL, NULL, NULL);
    ResetMenuEntry (entry);
}

static void
OpenFileAction (Widget w, XEvent *xev, String *s, Cardinal *c)
{
    if (current_file_name[0])
	strcpy (fileBuf, current_file_name);
    else
	fileBuf[0] = '\0';
    MakePrompt (toplevel, "File to open:", NewFile, fileBuf);
}

/*ARGSUSED*/
static void
RevisitFile (entry, name, data)
    Widget  entry;
    XtPointer name, data;
{
    RevisitFileAction (entry, NULL, NULL, NULL);
    ResetMenuEntry (entry);
}

static void
RevisitFileAction (Widget w, XEvent *xev, String *s, Cardinal *c)
{
    if (current_file_name[0])
	VisitFile (current_file_name, FALSE);
}

/*ARGSUSED*/
static void
Quit (entry, closure, data)
    Widget  entry;
    XtPointer closure, data;
{
    QuitAction (entry, NULL, NULL, NULL);
}

static void
QuitAction (Widget w, XEvent *xev, String *s, Cardinal *c)
{
    exit (0);
}

Widget	promptShell, promptDialog;
void	(*promptfunction)(char *);

/* ARGSUSED */
static
void CancelAction (widget, event, params, num_params)
    Widget	widget;
    XEvent	*event;
    String	*params;
    Cardinal	*num_params;
{
    if (promptShell) {
	XtSetKeyboardFocus(toplevel, (Widget) None);
	XtDestroyWidget(promptShell);
	promptShell = (Widget) 0;
    }
}


/* ARGSUSED */
static
void AcceptAction (widget, event, params, num_params)
    Widget	widget;
    XEvent	*event;
    String	*params;
    Cardinal	*num_params;
{
    (*promptfunction)(XawDialogGetValueString(promptDialog));
    CancelAction (widget, event, params, num_params);
}

static
void Noop (Widget w, XEvent *xev, String *s, Cardinal *c)
{
}

static void
MakePrompt(centerw, prompt, func, def)
Widget	centerw;
char *prompt;
void (*func)(char *);
char	*def;
{
    static Arg dialogArgs[] = {
	{XtNlabel, (XtArgVal) 0},
	{XtNvalue, (XtArgVal) 0},
    };
    Arg valueArgs[1];
    Arg centerArgs[2];
    Position	source_x, source_y;
    Position	dest_x, dest_y;
    Dimension center_width, center_height;
    Dimension prompt_width, prompt_height;
    Widget  valueWidget;
    
    CancelAction ((Widget)NULL, (XEvent *) 0, (String *) 0, (Cardinal *) 0);
    promptShell = XtCreatePopupShell ("promptShell", transientShellWidgetClass,
				      toplevel, NULL, (Cardinal) 0);
    dialogArgs[0].value = (XtArgVal)prompt;
    dialogArgs[1].value = (XtArgVal)def;
    promptDialog = XtCreateManagedWidget( "promptDialog", dialogWidgetClass,
		    promptShell, dialogArgs, XtNumber (dialogArgs));
    XawDialogAddButton(promptDialog, "accept", NULL, NULL);
    XawDialogAddButton(promptDialog, "cancel", NULL, NULL);
    valueWidget = XtNameToWidget (promptDialog, "value");
    if (valueWidget) {
    	XtSetArg (valueArgs[0], XtNresizable, TRUE);
    	XtSetValues (valueWidget, valueArgs, 1);
	/*
	 * as resizable isn't set until just above, the
	 * default value will be displayed incorrectly.
	 * rectify the situation by resetting the values
	 */
        XtSetValues (promptDialog, dialogArgs, XtNumber (dialogArgs));
    }
    XtSetKeyboardFocus (promptDialog, valueWidget);
    XtSetKeyboardFocus (toplevel, valueWidget);
    XtRealizeWidget (promptShell);
    /*
     * place the widget in the center of the "parent"
     */
    XtSetArg (centerArgs[0], XtNwidth, &center_width);
    XtSetArg (centerArgs[1], XtNheight, &center_height);
    XtGetValues (centerw, centerArgs, 2);
    XtSetArg (centerArgs[0], XtNwidth, &prompt_width);
    XtSetArg (centerArgs[1], XtNheight, &prompt_height);
    XtGetValues (promptShell, centerArgs, 2);
    source_x = (int)(center_width - prompt_width) / 2;
    source_y = (int)(center_height - prompt_height) / 3;
    XtTranslateCoords (centerw, source_x, source_y, &dest_x, &dest_y);
    XtSetArg (centerArgs[0], XtNx, dest_x);
    XtSetArg (centerArgs[1], XtNy, dest_y);
    XtSetValues (promptShell, centerArgs, 2);
    XtMapWidget(promptShell);
    promptfunction = func;
}

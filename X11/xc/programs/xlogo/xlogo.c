/*
 * $Xorg: xlogo.c,v 1.4 2001/02/09 02:05:54 xorgcvs Exp $
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
 */

/* $XFree86: xc/programs/xlogo/xlogo.c,v 3.9 2002/05/23 23:53:59 keithp Exp $ */

#include <stdio.h>
#include <X11/Intrinsic.h>
#include <X11/Shell.h>
#include "Logo.h"
#include <X11/Xaw/Cardinals.h>
#ifdef XKB
#include <X11/extensions/XKBbells.h>
#endif
#include <stdlib.h>

static void quit(Widget w, XEvent *event, String *params, 
		 Cardinal *num_params);

static XrmOptionDescRec options[] = {
{ "-shape", "*shapeWindow", XrmoptionNoArg, (XPointer) "on" },
#ifdef XRENDER
{"-render", "*render",XrmoptionNoArg, "TRUE"},
{"-sharp", "*sharp", XrmoptionNoArg, "TRUE"},
#endif
};

static XtActionsRec actions[] = {
    {"quit",	quit}
};

static Atom wm_delete_window;

String fallback_resources[] = {
    "*iconPixmap:    xlogo32",
    "*iconMask:      xlogo32",
    NULL,
};

static void 
die(Widget w, XtPointer client_data, XtPointer call_data)
{
    XCloseDisplay(XtDisplay(w));
    exit(0);
}

static void 
save(Widget w, XtPointer client_data, XtPointer call_data)
{
    return;
}

/*
 * Report the syntax for calling xlogo.
 */

static void 
Syntax(Widget toplevel, char *call)
{
    Arg arg;
    SmcConn connection;
    String reasons[7];
    int i, num_reasons = 7;

    reasons[0] = "Usage: ";
    reasons[1] = call;
    reasons[2] = " [-fg <color>] [-bg <color>] [-rv] [-bw <pixels>] [-bd <color>]\n";
    reasons[3] = "             [-d [<host>]:[<vs>]]\n";
    reasons[4] = "             [-g [<width>][x<height>][<+-><xoff>[<+-><yoff>]]]\n";
#ifdef XRENDER
    reasons[5] = "             [-render] [-sharp]\n";
#else
    reasons[5] = "";
#endif
    reasons[6] = "             [-shape]\n\n";

    XtSetArg(arg, XtNconnection, &connection);
    XtGetValues(toplevel, &arg, (Cardinal)1);
    if (connection) 
	SmcCloseConnection(connection, num_reasons, reasons);
    else {
	for (i=0; i < num_reasons; i++)
	    printf(reasons[i]);
    }
    exit(1);
}

int 
main(int argc, char *argv[])
{
    Widget toplevel;
    XtAppContext app_con;

    toplevel = XtOpenApplication(&app_con, "XLogo",
				 options, XtNumber(options), 
				 &argc, argv, fallback_resources,
				 sessionShellWidgetClass, NULL, ZERO);
    if (argc != 1)
	Syntax(toplevel, argv[0]);

    XtAddCallback(toplevel, XtNsaveCallback, save, NULL);
    XtAddCallback(toplevel, XtNdieCallback, die, NULL);
    XtAppAddActions
	(XtWidgetToApplicationContext(toplevel), actions, XtNumber(actions));
    XtOverrideTranslations
	(toplevel, XtParseTranslationTable ("<Message>WM_PROTOCOLS: quit()"));
    XtCreateManagedWidget("xlogo", logoWidgetClass, toplevel, NULL, ZERO);
    XtRealizeWidget(toplevel);
    wm_delete_window = XInternAtom(XtDisplay(toplevel), "WM_DELETE_WINDOW",
				   False);
    (void) XSetWMProtocols (XtDisplay(toplevel), XtWindow(toplevel),
                            &wm_delete_window, 1);
    XtAppMainLoop(app_con);
    exit(0);
}

/*ARGSUSED*/
static void 
quit(Widget w, XEvent *event, String *params, Cardinal *num_params)
{
    Arg arg;

    if (event->type == ClientMessage && 
	event->xclient.data.l[0] != wm_delete_window) {
#ifdef XKB
	XkbStdBell(XtDisplay(w), XtWindow(w), 0, XkbBI_BadValue);
#else
	XBell(XtDisplay(w), 0);
#endif
    } else {
	/* resign from the session */
	XtSetArg(arg, XtNjoinSession, False);
	XtSetValues(w, &arg, ONE);
	die(w, NULL, NULL);
    }
}


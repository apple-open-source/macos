/*
 * $Xorg: oclock.c,v 1.4 2001/02/09 02:05:33 xorgcvs Exp $
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
 */
/* $XFree86: xc/programs/oclock/oclock.c,v 1.7 2001/12/14 20:01:00 dawes Exp $ */

#include <X11/Intrinsic.h>
#include <X11/Xatom.h>
#include <X11/StringDefs.h>
#include <X11/Shell.h>
#include <X11/Xmu/Editres.h>
#include "Clock.h"
#include <stdio.h> 
#include <stdlib.h>

#ifdef XKB
#include <X11/extensions/XKBbells.h>
#endif

#include "oclock.bit"
#include "oclmask.bit"

static void die ( Widget w, XtPointer client_data, XtPointer call_data );
static void save ( Widget w, XtPointer client_data, XtPointer call_data );
static void usage ( void );
static void quit ( Widget w, XEvent *event, String *params, 
		   Cardinal *num_params );


static XtActionsRec actions[] = {
    {"quit",	quit}
};

static Atom wm_delete_window;

static void die(w, client_data, call_data)
    Widget	w;
    XtPointer	client_data, call_data;
{
    XCloseDisplay(XtDisplay(w));
    exit(0);
}

static void save(w, client_data, call_data)
    Widget w;
    XtPointer client_data, call_data;
{
    return;	/* stateless */
}

/* Exit with message describing command line format */

static void usage()
{
    fprintf(stderr,
"usage: oclock\n");
    fprintf (stderr, 
"       [-geometry [{width}][x{height}][{+-}{xoff}[{+-}{yoff}]]] [-display [{host}]:[{vs}]]\n");
    fprintf(stderr,
"       [-fg {color}] [-bg {color}] [-bd {color}] [-bw {pixels}]\n");
    fprintf(stderr,
"       [-minute {color}] [-hour {color}] [-jewel {color}]\n");
    fprintf(stderr,
"       [-backing {backing-store}] [-shape] [-noshape] [-transparent]\n");
    exit(1);
}

/* Command line options table.  Only resources are entered here...there is a
   pass over the remaining options after XtParseCommand is let loose. */

static XrmOptionDescRec options[] = {
{"-fg",		"*Foreground",		XrmoptionSepArg,	NULL},
{"-bg",		"*Background",		XrmoptionSepArg,	NULL},
{"-foreground",	"*Foreground",		XrmoptionSepArg,	NULL},
{"-background",	"*Background",		XrmoptionSepArg,	NULL},
{"-minute",	"*clock.minute",	XrmoptionSepArg,	NULL},
{"-hour",	"*clock.hour",		XrmoptionSepArg,	NULL},
{"-jewel",	"*clock.jewel",		XrmoptionSepArg,	NULL},
{"-backing",	"*clock.backingStore",	XrmoptionSepArg,	NULL},
{"-shape",	"*clock.shapeWindow",	XrmoptionNoArg,		"TRUE"},
{"-noshape",	"*clock.shapeWindow",	XrmoptionNoArg,		"FALSE"},
{"-transparent","*clock.transparent",	XrmoptionNoArg,		"TRUE"},
};

int 
main(int argc, char *argv[])
{
    XtAppContext xtcontext;
    Widget toplevel;
    Widget clock;
    Arg arg[2];
    int	i;
    
    toplevel = XtOpenApplication(&xtcontext, "Clock",
				 options, XtNumber(options), &argc, argv, NULL,
				 sessionShellWidgetClass, NULL, 0);
    if (argc != 1) usage();
    XtAddCallback(toplevel, XtNsaveCallback, save, NULL);
    XtAddCallback(toplevel, XtNdieCallback, die, NULL);

    XtAppAddActions
	(xtcontext, actions, XtNumber(actions));
    XtOverrideTranslations
	(toplevel, XtParseTranslationTable ("<Message>WM_PROTOCOLS: quit()"));

    i = 0;
    XtSetArg (arg[i], XtNiconPixmap, 
	      XCreateBitmapFromData (XtDisplay(toplevel),
				     XtScreen(toplevel)->root,
				     (char *)oclock_bits,
				     oclock_width, oclock_height));
    i++;
    XtSetArg (arg[i], XtNiconMask,
	      XCreateBitmapFromData (XtDisplay(toplevel),
				     XtScreen(toplevel)->root,
				     (char *)oclmask_bits,
				     oclmask_width, oclmask_height));
    i++;
    XtSetValues (toplevel, arg, i);

    clock=XtCreateManagedWidget("clock", clockWidgetClass, toplevel, NULL, 0);
    XtRealizeWidget (toplevel);

    wm_delete_window = XInternAtom(XtDisplay(toplevel), "WM_DELETE_WINDOW",
				   False);
    (void) XSetWMProtocols (XtDisplay(toplevel), XtWindow(toplevel),
                            &wm_delete_window, 1);

    XtAddEventHandler(toplevel, (EventMask) 0, TRUE, 
		      _XEditResCheckMessages, NULL);

    XtAppMainLoop(xtcontext);
    exit(0);
}

static void quit(w, event, params, num_params)
    Widget w;
    XEvent *event;
    String *params;
    Cardinal *num_params;
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
	XtSetArg(arg, XtNjoinSession, False);
	XtSetValues(w, &arg, (Cardinal)1);
	die(w, NULL, NULL);
    }
}

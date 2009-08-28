static char*id="$XConsortium: xmessage.c,v 1.6 95/01/04 16:29:54 gildea Exp $";
/*

Copyright (c) 1988, 1991, 1994  X Consortium

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
/* $XFree86: xc/programs/xmessage/xmessage.c,v 1.4 2000/02/17 16:53:03 dawes Exp $ */

#include <assert.h>
#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <X11/Shell.h>
#include <stdio.h>
#include <stdlib.h>

#include "xmessage.h"
#include "readfile.h"

/*
 * data used by xmessage
 */

const char *ProgramName;

static struct _QueryResources {
    char *file;
    char *button_list;
    char *default_button;
    Boolean print_value;
    Boolean center;
    Boolean nearmouse;
    int timeout_secs;
    Dimension maxHeight;
    Dimension maxWidth;
} qres;				/* initialized by resources below */

#define offset(field) XtOffsetOf(struct _QueryResources, field)
static XtResource resources[] = {
    { "file", "File", XtRString, sizeof (char *), 
      offset(file), XtRString, (XtPointer) NULL },
    { "buttons", "Buttons", XtRString, sizeof (char *),
      offset(button_list), XtRString, (XtPointer) "okay:0" },
    { "defaultButton", "DefaultButton", XtRString, sizeof (char *),
      offset(default_button), XtRString, (XtPointer) NULL },
    { "printValue", "PrintValue", XtRBoolean, sizeof (Boolean),
      offset(print_value), XtRString, "false" },
    { "center", "Center", XtRBoolean, sizeof (Boolean),
      offset(center), XtRString, "false" },
    { "nearMouse", "NearMouse", XtRBoolean, sizeof (Boolean),
      offset(nearmouse), XtRString, "false" },
    { "timeout", "Timeout", XtRInt, sizeof (int),
      offset(timeout_secs), XtRInt, 0 },
    { "maxHeight", "Maximum", XtRDimension, sizeof (Dimension),
      offset(maxHeight), XtRDimension, 0 },
    { "maxWidth", "Maximum", XtRDimension, sizeof (Dimension),
      offset(maxWidth), XtRDimension, 0 },
};
#undef offset

static XrmOptionDescRec optionList[] =  {
    { "-file",    ".file", XrmoptionSepArg, (XPointer) NULL },
    { "-buttons", ".buttons", XrmoptionSepArg, (XPointer) NULL },
    { "-default", ".defaultButton", XrmoptionSepArg, (XPointer) NULL },
    { "-print",   ".printValue", XrmoptionNoArg, (XPointer) "on" },
    { "-center",  ".center", XrmoptionNoArg, (XPointer) "on" },
    { "-nearmouse", ".nearMouse", XrmoptionNoArg, (XPointer) "on" },
    { "-timeout", ".timeout", XrmoptionSepArg, (XPointer) NULL },
};

static String fallback_resources[] = {
    "*baseTranslations: #override :<Key>Return: default-exit()",
    "*message.Scroll: whenNeeded",
    NULL};


/*
 * usage
 */

static void 
usage (FILE *outf)
{
    static const char *options[] = {
"    -file filename              file to read message from, \"-\" for stdin",
"    -buttons string             comma-separated list of label:exitcode",
"    -default button             button to activate if Return is pressed",
"    -print                      print the button label when selected",
"    -center                     pop up at center of screen",
"    -nearmouse                  pop up near the mouse cursor",
"    -timeout secs               exit with status 0 after \"secs\" seconds",
"",
NULL};
    const char **cpp;

    fprintf (outf, "usage: %s [-options] [message ...]\n\n",
	     ProgramName);
    fprintf (outf, "where options include:\n");
    for (cpp = options; *cpp; cpp++)
	fprintf (outf, "%s\n", *cpp);
    fprintf (outf, "%s\n", id+1);
}

/*
 * Action to implement ICCCM delete_window and other translations.
 * Takes one argument, the exit status.
 */
static Atom wm_delete_window;
/* ARGSUSED */
static void
exit_action(Widget w, XEvent *event, String *params, Cardinal *num_params)
{
    int exit_status = 0;

    if(event->type == ClientMessage
       && event->xclient.data.l[0] != wm_delete_window)
	return;
    
    if (*num_params == 1)
	exit_status = atoi(params[0]);
    exit(exit_status);
}

int default_exitstatus = -1;		/* value of button named by -default */

/* ARGSUSED */
static void
default_exit_action(Widget w, XEvent *event, String *params, 
    Cardinal *num_params)
{
    if (default_exitstatus >= 0)
	exit(default_exitstatus);
}

/* Convert tabs to spaces in *messagep,*lengthp, copying to a new block of
   memory.  */
void
detab (char **messagep, int *lengthp)
{
  int   i, n, col, psize;
  char  *p;
  
  /* count how many tabs there are */
  n = 0;
  for (i = 0; i < *lengthp; i++)
    if ((*messagep)[i] == '\t')
      n++;

  /* length increases by at most seven extra spaces for each tab */
  psize = *lengthp + n*7 + 1;
  p = XtMalloc (psize);

  /* convert tabs to spaces, copying into p */
  n = 0;
  col = 0;
  for (i = 0; i < *lengthp; i++)
    {
      switch ((*messagep)[i]) {
      case '\n':
        p[n++] = '\n';
        col = 0;
        break;
      case '\t':
        do
          {
            p[n++] = ' ';
            col++;
          }
        while ((col % 8) != 0);
        break;
      default:
        p[n++] = (*messagep)[i];
        col++;
        break;
      }
    }

  assert (n < psize);

  /* null-terminator needed by Label widget */
  p[n] = '\0';

  free (*messagep);

  *messagep = p;
  *lengthp = n;
}

static XtActionsRec actions_list[] = {
    {"exit", exit_action},
    {"default-exit", default_exit_action},
};

static String top_trans =
    "<ClientMessage>WM_PROTOCOLS: exit(1)\n";

/* assumes shell widget has already been realized */

static void
position_near(Widget shell, int x, int y)
{
    int max_x, max_y;
    Dimension width, height, border;
    int gravity;

    /* some of this is copied from CenterWidgetOnPoint in Xaw/TextPop.c */

    XtVaGetValues(shell,
		  XtNwidth, &width,
		  XtNheight, &height,
		  XtNborderWidth, &border,
		  NULL);

    width += 2 * border;
    height += 2 * border;

    max_x = WidthOfScreen(XtScreen(shell));
    max_y = HeightOfScreen(XtScreen(shell));

    /* set gravity hint based on position on screen */
    gravity = 1;
    if (x > max_x/3) gravity += 1;
    if (x > max_x*2/3) gravity += 1;
    if (y > max_y/3) gravity += 3;
    if (y > max_y*2/3) gravity += 3;

    max_x -= width;
    max_y -= height;

    x -= ( (Position) width/2 );
    if (x < 0) x = 0;
    if (x > max_x) x = max_x;

    y -= ( (Position) height/2 );
    if (y < 0) y = 0;
    if ( y > max_y ) y = max_y;

    XtVaSetValues(shell,
		  XtNx, (Position)x,
		  XtNy, (Position)y,
		  XtNwinGravity, gravity,
		  NULL);
}

static void
position_near_mouse(Widget shell)
{
    int x, y;
    Window root, child;
    int winx, winy;
    unsigned int mask;

    XQueryPointer(XtDisplay(shell), XtWindow(shell),
		  &root, &child, &x, &y, &winx, &winy, &mask);
    position_near(shell, x, y);
}

static void
position_near_center(Widget shell)
{
    position_near(shell,
		  WidthOfScreen(XtScreen(shell))/2,
		  HeightOfScreen(XtScreen(shell))/2);
}

/* ARGSUSED */
static void
time_out(XtPointer client_data, XtIntervalId *iid)
{
    exit(0);
}

/*
 * xmessage main program - make sure that there is a message,
 * then create the query box and go.  Callbacks take care of exiting.
 */
int
main (int argc, char *argv[])
{
    Widget top, queryform;
    XtAppContext app_con;
    char *message_str;
    int message_len;

    ProgramName = argv[0];

    XtSetLanguageProc(NULL, (XtLanguageProc) NULL, NULL);

    top = XtAppInitialize (&app_con, "Xmessage",
			   optionList, XtNumber(optionList), &argc, argv,
			   fallback_resources, NULL, 0);

    XtGetApplicationResources (top, (XtPointer) &qres, resources,
			       XtNumber(resources), NULL, 0);

    if (argc > 1 && !strcmp(argv[1], "-help")) {
	usage(stdout);
	exit(0);
    }
    if (argc == 1 && qres.file != NULL) {
	message_str = read_file (qres.file, &message_len);
	if (message_str == NULL) {
	    fprintf (stderr, "%s: problems reading message file\n",
		     ProgramName);
	    exit (1);
	}
    } else if (argc > 1 && qres.file == NULL) {
	int i, len;
	char *cp;

	len = argc - 1;		/* spaces between words and final NULL */
	for (i=1; i<argc; i++)
	    len += strlen(argv[i]);
	message_str = malloc(len);
	if (!message_str) {
	    fprintf (stderr, "%s: cannot get memory for message string\n",
		     ProgramName);
	    exit (1);
	}
	cp = message_str;
	for (i=1; i<argc; i++) {
	    strcpy(cp, argv[i]);
	    cp += strlen(argv[i]);
	    if (i != argc-1)
		*cp++ = ' ';
	    else
		*cp = '\0';
	}
	message_len = len;
    } else {
	usage(stderr);
	exit(1);
    }

    wm_delete_window = XInternAtom(XtDisplay(top), "WM_DELETE_WINDOW", False);
    XtAppAddActions(app_con, actions_list, XtNumber(actions_list));
    XtOverrideTranslations(top, XtParseTranslationTable(top_trans));

    detab (&message_str, &message_len);

    /*
     * create the query form; this is where most of the real work is done
     */
    queryform = make_queryform (top, message_str, message_len,
				qres.button_list,
				qres.print_value, qres.default_button,
				qres.maxWidth, qres.maxHeight);
    if (!queryform) {
	fprintf (stderr,
		 "%s: unable to create query form with buttons: %s\n",
		 ProgramName, qres.button_list);
	exit (1);
    }

    XtSetMappedWhenManaged(top, FALSE);
    XtRealizeWidget(top);

    /* do WM_DELETE_WINDOW before map */
    XSetWMProtocols(XtDisplay(top), XtWindow(top), &wm_delete_window, 1);

    if (qres.center)
	position_near_center(top);
    else if (qres.nearmouse)
	position_near_mouse(top);

    XtMapWidget(top);

    if (qres.timeout_secs)
	XtAppAddTimeOut(app_con, 1000*qres.timeout_secs, time_out, NULL);

    XtAppMainLoop(app_con);

    exit (0);
}

/* $XConsortium: makeform.c,v 1.6 95/01/04 16:28:51 gildea Exp $ */
/*

Copyright (c) 1988, 1991  X Consortium

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
/* $XFree86: xc/programs/xmessage/makeform.c,v 1.6 2002/11/22 03:56:39 paulo Exp $ */

#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <stdio.h>
#include <stdlib.h>

#include <X11/Shell.h>
#include <X11/Xaw/Form.h>
#include <X11/Xaw/AsciiText.h>
#include <X11/Xaw/Command.h>
#include <X11/Xaw/Label.h>
#include <X11/Xaw/Scrollbar.h>

#include "xmessage.h"

typedef struct _ButtonRecord {
    char *name;
    int exitstatus;
    Boolean print_value;
    Widget widget;
} ButtonRecord;

static void 
unquote_pairs (ButtonRecord *br, int n)
{
    int i;

    for (i = 0; i < n; i++, br++) {
	char *dst, *src;
	int quoted = 0;

	for (src = dst = br->name; *src; src++) {
	    if (quoted) {
		*dst++ = *src;
		quoted = 0;
	    } else if (src[0] == '\\') {
		quoted = 1;
	    } else {
		*dst++ = *src;
	    }
	}
	*dst = '\0';
    }
    return;
}

/*
 * parses string of form "yes:11,no:12, ..."
 * sets brptr to point to parsed table
 * returns 0 if successful, -1 if not
 */
static int 
parse_name_and_exit_code_list (char *buttonlist, ButtonRecord **brptr)
{
    char *cp;
    int shouldfind = 0, npairs = 0;
    int default_exitcode = 100;
    int quoted = 0;
    ButtonRecord *br;
    int len;
    char *copy;

    *brptr = NULL;
    if (!buttonlist) return 0;

    /*
     * Figure out how many matches we will find so that we can preallocate
     * space for button structures.  If you add stripping of white space,
     * make sure that you update this as well as the walking algorithm below.
     */
    if (buttonlist[0]) shouldfind++;
    for (cp = buttonlist; *cp; cp++) {
	if (quoted == 1) quoted = 0;
	else if (*cp == '\\') quoted = 1;
	else if (*cp == ',') shouldfind++;
    }
    len = (cp - buttonlist);

    /*
     * allocate space for button record
     */
    br = (ButtonRecord *) malloc (sizeof(ButtonRecord) * shouldfind);
    if (!br) return -1;

    cp = malloc (len + 1);
    if (!cp) {
	(void) free ((char *) br);
	return -1;
    }
    copy = cp;
    strcpy (copy, buttonlist);


    /*
     * walk down list separating into name:exitcode pairs
     */
    while (*cp) {
	char *start, *colon, *comma;
	int exitcode;

	start = cp;
	colon = comma = NULL;
	exitcode = ++default_exitcode;
	quoted = 0;

	/* find the next name and exit code */
	for (; *cp; cp++) {
	    if (quoted) quoted = 0;
	    else if (*cp == '\\') quoted = 1;
	    else if (*cp == ':') colon = cp;
	    else if (*cp == ',') {
		comma = cp;
		break;
	    }
	}

	/*
	 * If comma is NULL then we are at the end of the string.  If colon
	 * is NULL, then there was no exit code given, so default to zero.
	 */

	if (comma) *comma = '\0';

	if (colon) {
	    exitcode = atoi (colon+1);
	    *colon = '\0';
	}

	/*
	 * make sure that we aren't about to stomp on memory
	 */
	if (npairs >= shouldfind) {
	    fprintf (stderr,
		     "%s:  internal error, found extra pairs (should be %d)\n",
		     ProgramName, shouldfind);
	    (void) free ((char *) br);
	    (void) free (copy);
	    return -1;
	}

	/*
	 * got it!  start and exitcode contain the right values
	 */
	br[npairs].name = start;
	br[npairs].exitstatus = exitcode;
	npairs++;

	if (comma) cp++;
    }


    if (npairs != shouldfind) {
	fprintf (stderr, "%s:  internal error found %d instead of %d pairs\n",
		 ProgramName, npairs, shouldfind);
	(void) free ((char *) br);
	(void) free (copy);
	return -1;
    }

    /*
     * now, strip any quoted characters
     */
    unquote_pairs (br, npairs);
    *brptr = br;
    return npairs;
}

/* ARGSUSED */
static void 
handle_button (Widget w, XtPointer closure, XtPointer client_data)
{
    ButtonRecord *br = (ButtonRecord *) closure;

    if (br->print_value)
	puts (br->name);
    exit (br->exitstatus);
}

Widget 
make_queryform(Widget parent,	/* into whom widget should be placed */
    char *msgstr,		/* message string */
    int msglen,			/* characters in msgstr */
    char *button_list,		/* list of button title:status */
    Boolean print_value,	/* print button string on stdout? */
    char *default_button,	/* button activated by Return */
    Dimension max_width,
    Dimension max_height)
{
    ButtonRecord *br;
    int npairs, i;
    Widget form, text, prev;
    Arg args[10];
    Cardinal n, thisn;
    char *shell_geom;
    int x, y, geom_flags;
    unsigned int shell_w, shell_h;

    npairs = parse_name_and_exit_code_list (button_list, &br);

    form = XtCreateManagedWidget ("form", formWidgetClass, parent, NULL, 0);

    text = XtVaCreateManagedWidget
	("message", asciiTextWidgetClass, form,
	 XtNleft, XtChainLeft,
	 XtNright, XtChainRight,
	 XtNtop, XtChainTop,
	 XtNbottom, XtChainBottom,
	 XtNdisplayCaret, False,
	 XtNlength, msglen,
	 XtNstring, msgstr,
	 NULL);
    /*
     * Did the user specify our geometry?
     * If so, don't bother computing it ourselves, since we will be overridden.
     */
    XtVaGetValues(parent, XtNgeometry, &shell_geom, NULL);
    geom_flags = XParseGeometry(shell_geom, &x, &y, &shell_w, &shell_h);
    if (!(geom_flags & WidthValue && geom_flags & HeightValue)) {
	Dimension width, height, height_addons = 0;
	Dimension scroll_size, border_width;
	Widget label, scroll;
	Position left, right, top, bottom;
	char *tmp;
	/*
	 * A Text widget is used for the automatic scroll bars.
	 * But Text widget doesn't automatically compute its size.
	 * The Label widget does that nicely, so we create one and examine it.
	 * This widget is never visible.
	 */
	XtVaGetValues(text, XtNtopMargin, &top, XtNbottomMargin, &bottom,
		      XtNleftMargin, &left, XtNrightMargin, &right, NULL);
	label = XtVaCreateWidget("message", labelWidgetClass, form,
				 XtNlabel, msgstr,
				 XtNinternalWidth, (left+right+1)/2,
				 XtNinternalHeight, (top+bottom+1)/2,
				 NULL);
	XtVaGetValues(label, XtNwidth, &width, XtNheight, &height, NULL);
	XtDestroyWidget(label);
	if (max_width == 0)
	    max_width = .7 * WidthOfScreen(XtScreen(text));
	if (max_height == 0)
	    max_height = .7 * HeightOfScreen(XtScreen(text));
	if (width > max_width) {
	    width = max_width;
	    /* add in the height of any horizontal scroll bar */
	    scroll = XtVaCreateWidget("hScrollbar", scrollbarWidgetClass, text,
				      XtNorientation, XtorientHorizontal,
				      NULL);
	    XtVaGetValues(scroll, XtNheight, &scroll_size,
			  XtNborderWidth, &border_width, NULL);
	    XtDestroyWidget(scroll);
	    height_addons = scroll_size + border_width;
	}

	/* This fixes the xmessage assumption that the label widget and the
	 * text widget have the same size. In Xaw 7, the text widget has
	 * one extra pixel between lines.
	 * Xmessage is not internationalized, so the code bellow is harmless.
	 */
	tmp = msgstr;
	while (tmp != NULL && *tmp) {
	    ++tmp;
	    ++height;
	    tmp = strchr(tmp, '\n');
	}

	if (height > max_height) {
	    height = max_height;
	    /* add in the width of any vertical scroll bar */
	    scroll = XtVaCreateWidget("vScrollbar", scrollbarWidgetClass, text,
				      XtNorientation, XtorientVertical, NULL);
	    XtVaGetValues(scroll, XtNwidth, &scroll_size,
			  XtNborderWidth, &border_width, NULL);
	    XtDestroyWidget(scroll);
	    width += scroll_size + border_width;
	}
	height += height_addons;
	XtVaSetValues(text, XtNwidth, width, XtNheight, height, NULL);
    }
    /*
     * Create the buttons
     */
    n = 0;
    XtSetArg (args[n], XtNleft, XtChainLeft); n++;
    XtSetArg (args[n], XtNright, XtChainLeft); n++;
    XtSetArg (args[n], XtNtop, XtChainBottom); n++;
    XtSetArg (args[n], XtNbottom, XtChainBottom); n++;
    XtSetArg (args[n], XtNfromVert, text); n++;
    XtSetArg (args[n], XtNvertDistance, 5); n++;

    prev = NULL;
    for (i = 0; i < npairs; i++) {
	thisn = n;
	XtSetArg (args[thisn], XtNfromHoriz, prev); thisn++;
	prev = XtCreateManagedWidget (br[i].name, commandWidgetClass,
				      form, args, thisn);
	br[i].widget = prev;
	br[i].print_value = print_value;
	XtAddCallback (prev, XtNcallback, handle_button, (XtPointer) &br[i]);
	if (default_button && !strcmp(default_button, br[i].name)) {
	    Dimension border;

	    default_exitstatus = br[i].exitstatus;
	    XtVaGetValues(br[i].widget, XtNborderWidth, &border, NULL);
	    border *= 2;
	    XtVaSetValues(br[i].widget, XtNborderWidth, border, NULL);
	}
    }
    return form;
}

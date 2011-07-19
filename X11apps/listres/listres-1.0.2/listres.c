/*
 * $XConsortium: listres.c,v 1.32 94/04/17 20:43:22 dave Exp $
 *
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
 * *
 * Author:  Jim Fulton, MIT X Consortium
 */
/* $XFree86: xc/programs/listres/listres.c,v 1.3 2000/02/17 14:00:32 dawes Exp $ */

#include <stdio.h>
#include <stdlib.h>
#include <X11/Xos.h>
#include <X11/StringDefs.h>
#include <X11/IntrinsicP.h>
#include <X11/Xaw/Cardinals.h>
#include <X11/Core.h>
#include <X11/Xmu/CharSet.h>
#include <X11/Xmu/WidgetNode.h>
#include <X11/Xaw/AllWidgets.h>

#define widget_list XawWidgetArray  /* or motif or ol or ... */
#define nwidgets XawWidgetCount


static XrmOptionDescRec Options[] = {
  { "-top", "*topObject", XrmoptionSepArg, (caddr_t) NULL },
  { "-format", "*resourceFormat", XrmoptionSepArg, (caddr_t) NULL },
  { "-tree", "*showTree", XrmoptionNoArg, (caddr_t) "on" },
  { "-nosuper", "*showSuper", XrmoptionNoArg, (caddr_t) "off" },
  { "-variable", "*showVariable", XrmoptionNoArg, (caddr_t) "on" },
};

typedef struct {
    Boolean show_tree;
    Boolean show_all;
    Boolean show_variable;
    Boolean show_superclass;
    char *top_object;
    char *format;
  } OptionsRec;

static OptionsRec options;

#define Offset(field) XtOffsetOf(OptionsRec, field)

static XtResource Resources[] = {
  { "showTree", "ShowTree", XtRBoolean, sizeof(Boolean),
      Offset(show_tree), XtRImmediate, (XtPointer) FALSE },
  { "showSuper", "ShowSuper", XtRBoolean, sizeof(Boolean),
      Offset(show_superclass), XtRImmediate, (caddr_t) TRUE },
  { "showVariable", "ShowVariable", XtRBoolean, sizeof(Boolean),
      Offset(show_variable), XtRImmediate, (caddr_t) FALSE },
  { "topObject", "TopObject", XtRString, sizeof(char *),
      Offset(top_object), XtRString, (caddr_t) "core" },
  { "resourceFormat", "ResourceFormat", XtRString, sizeof(char *),
      Offset(format), XtRString, (caddr_t) " %-16s %20s  %-20s  %s" },
};

#undef Offset

static const char *ProgramName;

static void
usage (void)
{
    fprintf(stderr, "usage:  %s [-options...]\n", ProgramName);
    fprintf(stderr, "\nwhere options include:\n");
    fprintf(stderr,
	    "    -all             list all known widget and object classes\n");
    fprintf(stderr,
	    "    -tree            list all widgets and objects in a tree\n");
    fprintf(stderr,
	    "    -nosuper         do not print superclass resources\n");
    fprintf(stderr,
	    "    -variable        show variable name instead of class name\n");
    fprintf(stderr,
	    "    -top name        object to be top of tree\n");
    fprintf(stderr,
	    "    -format string   printf format for instance, class, type\n");
    fprintf(stderr, "\n");
    exit (1);
}

static void print_tree_level (register XmuWidgetNode *wn, register int level)
{
    register int i;

    if (!wn) return;

    for (i = 0; i < level; i++) {
	putchar (' '); putchar (' '); 
    }
    printf ("%d:  %s/%s\n", level, wn->label, XmuWnClassname(wn));
    print_tree_level (wn->children, level + 1);
    print_tree_level (wn->siblings, level);
}

static void tree_known_widgets (void)
{
    register int i;
    register XmuWidgetNode *wn;

    for (i = 0, wn = widget_list; i < nwidgets; i++, wn++) {
	if (!wn->superclass) {		/* list all rooted objects */
	    print_tree_level (wn, 0);
	}
    }
}


/*
 * print_classname - print out the superclass-to-subclass hierchy of names
 * in the form super\sub\sub....
 */
static int print_classname (XmuWidgetNode *node, XmuWidgetNode *topnode,
			    int level, Bool showvar)
{
    int retval;

    if (node && node != topnode) {
	retval = print_classname (node->superclass, topnode, level + 1,
				  showvar);
    } else {
	retval = level - 1;
    }
    if (node)
      printf ("%s%s", showvar ? node->label : XmuWnClassname(node),
	      level ? "\\" : "");

    return retval;
}

static void list_known_widgets (void)
{
    int i;
    XmuWidgetNode *wn;
    int width = 0;
    char format[20];

    for (i = 0, wn = widget_list; i < nwidgets; i++, wn++) {
	int l = strlen (wn->label);
	if (l > width) width = l;
    }
    sprintf (format, "%%-%ds  ", width);
    for (i = 0, wn = widget_list; i < nwidgets; i++, wn++) {
	printf (format, wn->label);
	print_classname (wn, (XmuWidgetNode *) NULL, 0, False);
	putchar ('\n');
    }
}

/* ARGSUSED */
static void print_resources (XmuWidgetNode *node, const char *format,
			     XmuWidgetNode *topnode, Bool showsuper,
			     Bool showvar)
{
    int i;
    XtResourceList res = node->resources;
    XmuWidgetNode **wn = node->resourcewn;

    for (i = 0; i < node->nresources; i++, res++, wn++) {
	if (!showsuper && *wn != node) continue;
	printf (format, showvar ? (*wn)->label : XmuWnClassname(*wn),
		res->resource_name, res->resource_class, res->resource_type);
	putchar ('\n');
    }
    if (node->nconstraints > 0) {
	printf (format, "----", "----", "----", "----");
	putchar ('\n');
    }
    res = node->constraints;
    wn = node->constraintwn;
    for (i = 0; i < node->nconstraints; i++, res++, wn++) {
	if (!showsuper && *wn != node) continue;
	printf (format, showvar ? (*wn)->label : XmuWnClassname(*wn),
		res->resource_name, res->resource_class, res->resource_type);
	putchar ('\n');
    }
    return;
}


/*
 * list_resources - display resources of a widget, identifying class from
 * which they come
 */
static void
list_resources (XmuWidgetNode *node, const char *format,
		XmuWidgetNode *topnode, Widget toplevel,
		Bool showsuper, Bool showvar)
{
    static Bool first = True;

    XmuWnFetchResources (node, toplevel, topnode);
    if (first) {
	printf (format, showvar ? "Variable" : "WidgetClass",
		"Instance", "Class", "Type");
	putchar ('\n');
	printf (format, showvar ? "--------" : "-----------",
		"--------", "-----", "----");
	putchar ('\n');
	first = False;
    }
    printf ("%s:  ", node->label);
    print_classname (node, topnode, 0, showvar);
    putchar ('\n');
    print_resources (node, format, topnode, showsuper, showvar);
    putchar ('\n');
}


int
main (int argc, char **argv)
{
    int i;
    XtAppContext appcon;
    XmuWidgetNode *topnode;
    Widget toplevel, container;

    ProgramName = argv[0];

    XtSetLanguageProc(NULL, (XtLanguageProc) NULL, NULL);

    toplevel = XtAppInitialize (&appcon, "Listres", Options, XtNumber(Options),
				&argc, argv, NULL, NULL, 0);
    container = XtCreateWidget ("dummy", widgetClass, toplevel, NULL, ZERO);

    XtGetApplicationResources (toplevel, (caddr_t) &options,
			       Resources, XtNumber(Resources), NULL, ZERO);
    XmuWnInitializeNodes (widget_list, nwidgets);
    if (argc == 1) {
	if (options.show_tree) {
	    tree_known_widgets();
	} else {
	    list_known_widgets();
	}
	exit (0);
    }

    topnode = XmuWnNameToNode (widget_list, nwidgets, options.top_object);
    argc--, argv++;			/* skip command */

    if (argc > 0 && argv[0][0] == '-') {
	int len = strlen (argv[0]);
	if (len >= 2 && strncmp(argv[0], "-all", len) == 0) {
	    XmuWidgetNode *wn;
	    for (i = 0, wn = widget_list; i < nwidgets; i++, wn++) {
		list_resources (wn, options.format, topnode, container,
				(Bool) options.show_superclass,
				(Bool) options.show_variable);
	    }
	} else
	  usage();
    } else {
	for (; argc > 0; argc--, argv++) {
	    XmuWidgetNode *node;

	    if (argv[0][0] == '-') usage ();
	    node = XmuWnNameToNode (widget_list, nwidgets, *argv);
	    if (!node) {
		fprintf (stderr, "%s:  unable to find widget \"%s\"\n",
			 ProgramName, *argv);
		continue;
	    }
	    list_resources (node, options.format, topnode, container,
			    (Bool) options.show_superclass,
			    (Bool) options.show_variable);
	}
    }
    exit (0);
}

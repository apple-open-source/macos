/*
 * $XConsortium: viewres.c,v 1.74 94/04/17 20:43:24 converse Exp $
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
/* $XFree86: xc/programs/viewres/viewres.c,v 1.6 2003/05/27 22:26:58 tsi Exp $ */

#include <stdio.h>
#include <stdlib.h>
#include <X11/StringDefs.h>
#include <X11/IntrinsicP.h>
#include <X11/Xaw/Cardinals.h>
#include <X11/Xaw/Box.h>
#include <X11/Xaw/Form.h>
#include <X11/Xaw/Command.h>
#include <X11/Xaw/MenuButton.h>
#include <X11/Xaw/SimpleMenu.h>
#include <X11/Xaw/Sme.h>
#include <X11/Xaw/SmeBSB.h>
#include <X11/Xaw/SmeLine.h>
#include <X11/Xaw/Paned.h>
#include <X11/Xaw/Porthole.h>
#include <X11/Xaw/Toggle.h>
#include <X11/Xaw/Text.h>
#include <X11/Xaw/List.h>
#include <X11/Xaw/Scrollbar.h>
#include <X11/Xaw/Panner.h>
#include <X11/Xaw/Tree.h>
#include <X11/Xmu/Converters.h>
#include <X11/Xmu/CharSet.h>
#include <X11/Xmu/WidgetNode.h>
#include <X11/Xaw/AllWidgets.h>

#define widget_list XawWidgetArray  /* or motif or ol or ... */
#define nwidgets XawWidgetCount

typedef struct {
    const char **resource_labels;	/* names of res added by widget */
    Cardinal nnewresources;		/* number res added by widget */
    Cardinal nnewconstraints;		/* number res added by widget */
    Cardinal nnew;			/* number new */
    Widget instance;			/* Label widget in box in tree */
    Widget resource_lw;			/* List widget showing resources */
    int selection_index;		/* -1 or index into selection_list */
} ViewresData;

#define VData(node) ((ViewresData *) (node)->data)


#define IsShowing(node) (VData(node)->resource_lw && \
			 XtIsManaged(VData(node)->resource_lw))


struct WidgetList {
    int n_elements;
    int max_elements;
    XmuWidgetNode **elements;
};

static struct WidgetList selected_list = { 0, 0, (XmuWidgetNode **) NULL };

#define INSERT_NODE(node,i) \
  selected_list.elements[VData(node)->selection_index = (i)] = (node)

#define REMOVE_NODE(node) \
  selected_list.elements[VData(node)->selection_index] = \
  (XmuWidgetNode *) NULL; VData(node)->selection_index = (-1)

static const char *ProgramName;
static int NumberShowing = 0;

static Arg sensitiveargs[2] = {{ XtNsensitive, (XtArgVal) FALSE },
			       { XtNsensitive, (XtArgVal) TRUE }};

static const char *help_message[] = {
    "-top name        object to be top of tree",
    "-variable        show variable name instead of class name",
    "-vertical        list the tree vertically",
    NULL
};

static XrmOptionDescRec Options[] = {
    { "-top", "*topObject", XrmoptionSepArg, (XPointer) NULL },
    { "-variable", "*showVariable", XrmoptionNoArg, (XPointer) "on" },
    { "-vertical", "*Tree.Gravity", XrmoptionNoArg, (XPointer) "north" }
};


typedef struct {
    char *top_object;
    Boolean show_variable;
} OptionsRec;

static OptionsRec options;

#define Offset(field) XtOffsetOf(OptionsRec, field)

static XtResource Resources[] = {
    { "topObject", "TopObject", XtRString, sizeof(char *),
	Offset(top_object), XtRString, (XtPointer) "object" },
    { "showVariable", "ShowVariable", XtRBoolean, sizeof(Boolean),
	Offset(show_variable), XtRImmediate, (XtPointer) FALSE },
};

#undef Offset

static const char *fallback_resources[] = {
    "*allowShellResize: true",
    "*Porthole.top: ChainTop",
    "*Porthole.left: ChainLeft",
    "*Porthole.bottom: ChainBottom",
    "*Porthole.right:  ChainRight",
    "*Porthole.resizable: on",
    "*Panner.top: ChainTop",
    "*Panner.left: ChainLeft",
    "*Panner.bottom: ChainTop",
    "*Panner.right:  ChainLeft",
    "*Panner.resizable: on",
    "*Tree*ShapeStyle: rectangle",
    "*Tree*Toggle*BorderWidth: 0",
    "*Porthole*Box.BorderWidth: 0",
    "*Porthole*Box.HSpace: 0",
    "*Porthole*Box.VSpace: 0",
    "*Paned*allowResize: true",
    "*buttonbox.quit.Translations:  #override \\n <Btn1Down>,<Btn1Up>: Quit() unset()",
    "*Toggle.Translations: #augment \\n <Btn2Down>,<Btn2Up>: set() notify() Resources(toggle)",
    NULL
};

static void ActionQuit(Widget, XEvent *, String *, Cardinal *);
static void ActionSetLableType(Widget, XEvent *, String *, Cardinal *);
static void ActionSetOrientation(Widget, XEvent *, String *, Cardinal *);
static void ActionSelect(Widget, XEvent *, String *, Cardinal *);
static void ActionResources(Widget, XEvent *, String *, Cardinal *);
static void set_labeltype_menu(Boolean, Boolean);
static void set_orientation_menu(XtGravity, Boolean);
static void build_tree(XmuWidgetNode *, Widget, Widget);
static void set_node_labels(XmuWidgetNode *, int);

static XtActionsRec viewres_actions[] = {
    { "Quit", ActionQuit },
    { "SetLabelType", ActionSetLableType },
    { "SetOrientation", ActionSetOrientation },
    { "Select", ActionSelect },
    { "Resources", ActionResources },
};

static Atom wm_delete_window;

#define BOOL_OFF 0
#define BOOL_ON 1
#define BOOL_TOGGLE 2

#define VIEW_HORIZONTAL 0
#define VIEW_VERTICAL 1
#define VIEW_VARIABLES 2
#define VIEW_CLASSES 3
#define VIEW_SHOW_RESOURCES 4
#define VIEW_HIDE_RESOURCES 5
#define VIEW_number 6

#define SELECT_NOTHING 0
#define SELECT_ALL 1
#define SELECT_INVERT 2
#define SELECT_PARENT 3
#define SELECT_ANCESTORS 4
#define SELECT_CHILDREN 5
#define SELECT_DESCENDANTS 6
#define SELECT_HAS_RESOURCES 7
#define SELECT_SHOWN_RESOURCES 8
#define SELECT_number 9

static struct _nametable {
    const char *name;
    int value;
} select_nametable[] = {
    { "nothing", SELECT_NOTHING },
    { "all", SELECT_ALL },
    { "invert", SELECT_INVERT },
    { "parent", SELECT_PARENT },
    { "ancestors", SELECT_ANCESTORS },
    { "children", SELECT_CHILDREN },
    { "descendants", SELECT_DESCENDANTS },
    { "resources", SELECT_HAS_RESOURCES },
    { "shown", SELECT_SHOWN_RESOURCES },
}, boolean_nametable[] = {
    { "off", BOOL_OFF },
    { "false", BOOL_OFF },
    { "no", BOOL_OFF },
    { "on", BOOL_ON },
    { "true", BOOL_ON },
    { "yes", BOOL_ON },
    { "toggle", BOOL_TOGGLE },
};

static Widget treeWidget;
static Widget viewButton, viewMenu, selectButton, selectMenu;
static Widget view_widgets[VIEW_number];
static Widget select_widgets[SELECT_number];
static XmuWidgetNode *topnode;

static Arg false_args[1] = {{ XtNstate, (XtArgVal) FALSE }};
static Arg true_args[1] = {{ XtNstate, (XtArgVal) TRUE }};


/*
 * routines
 */
static void 
usage (void)
{
    const char **cpp;
    fprintf (stderr, "usage:  %s [-options...]\n", ProgramName);
    fprintf(stderr, "\nwhere options include:\n");
    for (cpp = help_message; *cpp; cpp++) {
	fprintf (stderr, "    %s\n", *cpp);
    }
    fprintf(stderr, "\n");
    exit (1);
}


static XmuWidgetNode *
widget_to_node (Widget gw)
{
    XmuWidgetNode *node;
    int i;

    if (XtIsSubclass (gw, toggleWidgetClass)) {
	for (i = 0, node = widget_list; i < nwidgets; i++, node++) {
	    if (VData(node)->instance == gw) return node;
	}
    } else if (XtIsSubclass (gw, listWidgetClass)) {
	for (i = 0, node = widget_list; i < nwidgets; i++, node++) {
	    if (VData(node)->resource_lw == gw) return node;
	}
    }
    return (XmuWidgetNode *) NULL;
}


static void 
initialize_widgetnode_list (XmuWidgetNode ***listp, int *sizep, int n)
{
    int i;
    XmuWidgetNode **l;

    if (!*listp) {
        *listp = (XmuWidgetNode **)
	  XtCalloc ((unsigned int) n, (unsigned int)sizeof(XmuWidgetNode **));
        *sizep = ((*listp) ? n : 0);
        return;
    }
    if (n > *sizep) {
        *listp = (XmuWidgetNode **) XtRealloc ((char *) *listp,
					       (unsigned int) 
					       (n * sizeof(XmuWidgetNode **)));
	if (!*listp) {
	    *sizep = 0;
	    return;
	}
	for (i = *sizep, l = (*listp) + i; i < n; i++, l++) *l =
	  (XmuWidgetNode *) NULL;
	*sizep = n;
    }
    return;
}


static Boolean 
set_resource_labels (XmuWidgetNode *node)
{
    int i;
    const char **cur;
    XtResourceList res;
    XmuWidgetNode **wn;
    ViewresData *d = VData(node);

    if (!d->resource_labels) {
	d->resource_labels =
	  (const char **) calloc ((unsigned) d->nnew * 3,
				  (unsigned) sizeof (const char *));
	if (!d->resource_labels) return FALSE;
    }

    cur = d->resource_labels;
    res = node->resources;
    wn = node->resourcewn;
    for (i = 0; i < node->nresources; i++, res++, wn++) {
	if (*wn == node) {		/* should match nnew */
	    *cur++ = res->resource_name;
	    *cur++ = res->resource_class;
	    *cur++ = res->resource_type;
	}
    }
    if (d->nnewconstraints > 0) {
	const char *s;

	*cur++ = s = "";
	*cur++ = s;
	*cur++ = s;
    }
    res = node->constraints;
    wn = node->constraintwn;
    for (i = 0; i < node->nconstraints; i++, res++, wn++) {
	if (*wn == node) {		/* should match nnew */
	    *cur++ = res->resource_name;
	    *cur++ = res->resource_class;
	    *cur++ = res->resource_type;
	}
    }
    return TRUE;
}


static ViewresData *
create_viewres_data (XmuWidgetNode *node)
{
    ViewresData *d =
      (ViewresData *) malloc ((unsigned) sizeof(ViewresData));

    if (d) {
	d->resource_labels = NULL;
	d->nnewresources = XmuWnCountOwnedResources (node, node, False);
	d->nnewconstraints = XmuWnCountOwnedResources (node, node, True);
	d->nnew = (d->nnewresources + (d->nnewconstraints 
				       ? d->nnewconstraints + 1 : 0));
	d->instance = (Widget) NULL;
	d->resource_lw = (Widget) NULL;
	d->selection_index = -1;
    }
    return d;
}

static int 
copydown (int start)
{
    XmuWidgetNode **src = &selected_list.elements[start];
    XmuWidgetNode **dst = src;
    int cur;

    for (cur = start; start < selected_list.n_elements; start++, src++) {
	if (*src) {
	    VData((*src))->selection_index = cur++;
	    *dst++ = *src;
	}
    }
    return (start - cur);
}


static void 
add_to_selected_list (XmuWidgetNode *node, Boolean updatewidget)
{
    ViewresData *d = VData(node);
    if (!d->instance || d->selection_index >= 0) return;

    if (selected_list.n_elements >= selected_list.max_elements) {
	initialize_widgetnode_list (&selected_list.elements,
				    &selected_list.max_elements, 
				    (selected_list.max_elements * 3) / 2);
    }
    INSERT_NODE (node, selected_list.n_elements);
    selected_list.n_elements++;

    if (updatewidget) XtSetValues (d->instance, true_args, ONE);
}

static Boolean 
remove_from_selected_list (XmuWidgetNode *node, Boolean updatewidget)
{
    int i, skips;
    ViewresData *d = VData(node);

    if ((i = d->selection_index) < 0) return FALSE;

    REMOVE_NODE (node);

    /* copy down */
    if (selected_list.n_elements > 1) {
	skips = copydown (i);
    } else {
	skips = 1;
    }
    selected_list.n_elements -= skips;

    if (updatewidget) XtSetValues (d->instance, false_args, ONE);
    return TRUE;
}

static void 
remove_nodes_from_selected_list (int start, int count, Boolean updatewidget)
{
    int i;

    for (i = 0; i < count; i++) {
	XmuWidgetNode *p = selected_list.elements[start+i];
	ViewresData *d = VData(p);
	REMOVE_NODE (p);
	if (updatewidget) XtSetValues (d->instance, false_args, ONE);
    }
    selected_list.n_elements -= copydown (start);
}
	    
static void 
add_subtree_to_selected_list (XmuWidgetNode *node, Boolean updatewidget)
{
    if (!node) return;

    add_to_selected_list (node, updatewidget);
    for (node = node->children; node; node = node->siblings) {
	add_subtree_to_selected_list (node, updatewidget);
    }
}


/* ARGSUSED */
static void 
variable_labeltype_callback (Widget gw, 
    XtPointer closure,		/* TRUE or FALSE */
    XtPointer data)
{
    set_labeltype_menu ((Boolean) (long) closure, True);
}

/* ARGSUSED */
static void 
gravity_callback (Widget gw, 
    XtPointer closure,		/* TRUE or FALSE */ 
    XtPointer data)
{
    set_orientation_menu ((XtGravity) (long) closure, True);
}


static Boolean 
create_resource_lw (XmuWidgetNode *node)
{
    Arg args[4];
    Cardinal n;
    ViewresData *d = VData(node);

    if (d->nnew == 0) return FALSE;

    if (!d->resource_labels &&
	!set_resource_labels (node)) return FALSE;

    n = 0;
    XtSetArg (args[n], XtNnumberStrings, 3 * d->nnew); n++;
    XtSetArg (args[n], XtNlist, d->resource_labels); n++;
    XtSetArg (args[n], XtNdefaultColumns, 3); n++;
    XtSetArg (args[n], XtNforceColumns, TRUE); n++;
    d->resource_lw = XtCreateManagedWidget (node->label, listWidgetClass,
					    XtParent(d->instance),
					    args, n);
    XtRealizeWidget (d->resource_lw);
    return TRUE;
}

static void 
update_selection_items (void)
{
    int i;
    static Arg args[1] = {{ XtNsensitive, (XtArgVal) FALSE }};
    Boolean show = FALSE, hide = FALSE, ancestors = FALSE;
    Boolean descendants = FALSE;

    for (i = 0; i < selected_list.n_elements; i++) {
	XmuWidgetNode *node = selected_list.elements[i];
	ViewresData *d = VData(node);

	/*
	 * If node has any new resources then may be shown (if not
	 * already being shown).  If node has widget and is managed,
	 * then may be hidden.
	 */
	if (d->nnew > 0) {
	    if (IsShowing(node)) {
		hide = TRUE;
	    } else {
		show = TRUE;
	    }
	}
	if (node != topnode) ancestors = TRUE;
	if (node->children) descendants = TRUE;
    }
	    
    args[0].value = (XtArgVal) show;
    XtSetValues (view_widgets[VIEW_SHOW_RESOURCES], args, ONE);
    args[0].value = (XtArgVal) hide;
    XtSetValues (view_widgets[VIEW_HIDE_RESOURCES], args, ONE);
    args[0].value = (XtArgVal) (selected_list.n_elements > 0 ? TRUE : FALSE);
    XtSetValues (select_widgets[SELECT_NOTHING], args, ONE);
    args[0].value = (XtArgVal) ancestors;
    XtSetValues (select_widgets[SELECT_PARENT], args, ONE);
    XtSetValues (select_widgets[SELECT_ANCESTORS], args, ONE);
    args[0].value = (XtArgVal) descendants;
    XtSetValues (select_widgets[SELECT_CHILDREN], args, ONE);
    XtSetValues (select_widgets[SELECT_DESCENDANTS], args, ONE);
    args[0].value = (XtArgVal) ((Boolean) (NumberShowing > 0));
    XtSetValues (select_widgets[SELECT_SHOWN_RESOURCES], args, ONE);
}


static void 
do_resources (XmuWidgetNode *node, Boolean op, Boolean updatewidget)
{
    ViewresData *d = VData(node);
    if (op == BOOL_TOGGLE) op = (IsShowing(node) ? BOOL_OFF : BOOL_ON);

    if (op == BOOL_ON) {
	if (d->resource_lw) {		/* if already created */
	    if (!XtIsManaged(d->resource_lw)) {
		NumberShowing++;
		XtManageChild (d->resource_lw);
	    }				/* else ignore it */
	} else if (create_resource_lw (node))	/* create it */
	  NumberShowing++;
    } else if (d->resource_lw) {		/* if already created */
	if (XtIsManaged (d->resource_lw)) {
	    NumberShowing--;
	    XtUnmanageChild (d->resource_lw);
	    XawListUnhighlight (d->resource_lw);
	    if (updatewidget) remove_from_selected_list (node, TRUE);
	}				/* else ignore it */
    }
}



/* ARGSUSED */
static void 
show_resources_callback (
    Widget gw,				/* menu or toggle button */
    XtPointer closure,			/* BOOL_OFF, BOOL_ON, BOOL_TOGGLE */
    XtPointer data)			/* undefined */
{
    int op = (long) closure;
    XmuWidgetNode *node = widget_to_node (gw);

    if (node) {
	XUnmapWindow (XtDisplay(treeWidget), XtWindow(treeWidget));
	do_resources (node, op, TRUE);
    } else if (selected_list.n_elements <= 0) {
	return;
    } else {
	int i;

	XUnmapWindow (XtDisplay(treeWidget), XtWindow(treeWidget));
	for (i = 0; i < selected_list.n_elements; i++) {
	    do_resources (selected_list.elements[i], op, FALSE);
	}
    }
    XawTreeForceLayout (treeWidget);
    XMapWindow (XtDisplay(treeWidget), XtWindow(treeWidget));
    update_selection_items ();
}


/* ARGSUSED */
static void 
select_callback (
    Widget gw,				/* entry widget */
    XtPointer closure,			/* TRUE or FALSE */
    XtPointer data)			/* undefined */
{
    int i;
    int nselected = selected_list.n_elements;
    XmuWidgetNode *node;

    switch ((long) closure) {
      case SELECT_NOTHING:		/* clear selection_list */
	remove_nodes_from_selected_list (0, nselected, True);
	break;

      case SELECT_ALL:			/* put everything on selection_list */
	add_subtree_to_selected_list (topnode, TRUE);
	break;

      case SELECT_INVERT:		/* toggle selection state */
	for (i = 0, node = widget_list; i < nwidgets; i++, node++) {
	    ViewresData *d = VData(node);
	    if (d->selection_index < 0) add_to_selected_list (node, TRUE);
	}
	remove_nodes_from_selected_list (0, nselected, True);
	break;


      case SELECT_PARENT:		/* choose immediate parent */
	node = widget_to_node (gw);
	if (node) {
	    if (node->superclass)
	      add_to_selected_list (node->superclass, TRUE);
	} else {
	    for (i = 0; i < nselected; i++) {
		XmuWidgetNode *node = selected_list.elements[i];
		if (node->superclass)
		  add_to_selected_list (node->superclass, TRUE);
	    }
	}
	break;

      case SELECT_ANCESTORS:		/* chain up adding to selection_list */
	node = widget_to_node (gw);
	if (node) {
	    do {
		add_to_selected_list (node, TRUE);
	    } while ((node = node->superclass) != NULL);
	} else {
	    for (i = 0; i < nselected; i++) {
		XmuWidgetNode *parent = selected_list.elements[i];

		/*
		 * chain up the tree, but stop if we get to nodes that
		 * are already in the selected list.
		 */
		while ((parent = parent->superclass) != NULL) {
		    if (VData(parent)->selection_index >= 0) break;
		    add_to_selected_list (parent, TRUE);
		}
	    }
	}
	break;

      case SELECT_CHILDREN:		/* all direct sub nodes */
	node = widget_to_node (gw);
	if (node) {
	    add_to_selected_list (node, TRUE);
	    for (node = node->children; node; node = node->siblings) {
		add_to_selected_list (node, TRUE);
	    }
	} else {
	    for (i = 0; i < nselected; i++) {
		XmuWidgetNode *node = selected_list.elements[i];

		add_to_selected_list (node, TRUE);
		for (node = node->children; node; node = node->siblings) {
		    add_to_selected_list (node, TRUE);
		}
	    }
	}
	break;

      case SELECT_DESCENDANTS:		/* all sub nodes */
	node = widget_to_node (gw);
	if (node) {
	    add_subtree_to_selected_list (node, TRUE);
	} else {
	    for (i = 0; i < nselected; i++) {
		XmuWidgetNode *parent = selected_list.elements[i];

		add_subtree_to_selected_list (parent, TRUE);
	    }
	}
	break;

      case SELECT_HAS_RESOURCES:	/* put all w/ rescnt > 0 on sel_list */
	for (i = 0, node = widget_list; i < nwidgets; i++, node++) {
	    if (VData(node)->nnew > 0)
	      add_to_selected_list (node, TRUE);
	}
	break;

      case SELECT_SHOWN_RESOURCES:
	for (i = 0, node = widget_list; i < nwidgets; i++, node++) {
	    if (IsShowing(node)) add_to_selected_list (node, TRUE);
	}
	break;

      default:				/* error!!! */
	XBell (XtDisplay(gw), 0);
	return;
    }

    update_selection_items ();
}

/* ARGSUSED */
static void 
toggle_callback (Widget gw, 
    XtPointer closure,		/* XmuWidgetNode for this widget */
    XtPointer data)		/* on or off */
{
    XmuWidgetNode *node = (XmuWidgetNode *) closure;
    Boolean selected = (Boolean) (long) data;

    if (selected) {
	add_to_selected_list (node, FALSE);
    } else {
	(void) remove_from_selected_list (node, FALSE);
    }

    update_selection_items ();
}


/*
 * panner/porthole controls - called when the other changes
 */
/* ARGSUSED */
static void 
panner_callback (
    Widget gw,				/* panner widget */
    XtPointer closure,			/* porthole widget */
    XtPointer data)			/* report */
{
    XawPannerReport *rep = (XawPannerReport *) data;
    Arg args[2];

    if (!treeWidget) return;

    XtSetArg (args[0], XtNx, -rep->slider_x);
    XtSetArg (args[1], XtNy, -rep->slider_y);
    XtSetValues (treeWidget, args, TWO);	/* just assume... */
}

/* ARGSUSED */
static void 
porthole_callback (
    Widget gw,				/* porthole widget */
    XtPointer closure,			/* panner widget */
    XtPointer data)			/* report */
{
    Widget panner = (Widget) closure;
    XawPannerReport *rep = (XawPannerReport *) data;
    Arg args[6];
    Cardinal n = TWO;

    XtSetArg (args[0], XtNsliderX, rep->slider_x);
    XtSetArg (args[1], XtNsliderY, rep->slider_y);
    if (rep->changed != (XawPRSliderX | XawPRSliderY)) {
	XtSetArg (args[2], XtNsliderWidth, rep->slider_width);
	XtSetArg (args[3], XtNsliderHeight, rep->slider_height);
	XtSetArg (args[4], XtNcanvasWidth, rep->canvas_width);
	XtSetArg (args[5], XtNcanvasHeight, rep->canvas_height);
	n = SIX;
    }
    XtSetValues (panner, args, n);
}



static void 
build_tree (XmuWidgetNode *node, Widget tree, Widget super)
{
    ViewresData *d = VData (node);
    Widget box, w;			/* widget for this Class */
    XmuWidgetNode *child;			/* iterator over children */
    Arg args[3];			/* need to set super node */
    Cardinal n;				/* count of args */
    static XtCallbackRec callback_rec[2] = {{ toggle_callback, NULL },
					     { NULL, NULL }};


    n = 0;
    XtSetArg (args[n], XtNtreeParent, super); n++;
    box = XtCreateManagedWidget (node->label, boxWidgetClass, tree, args, n);

    n = 0;
    XtSetArg (args[n], XtNlabel, (options.show_variable ?
				  node->label : XmuWnClassname(node))); n++;
    XtSetArg (args[n], XtNcallback, callback_rec); n++;

    callback_rec[0].closure = (XtPointer) node;
    w = XtCreateManagedWidget (node->label, toggleWidgetClass, box, args, n);
    d->instance = w;

    /*
     * recursively build the rest of the tree
     */
    for (child = node->children; child; child = child->siblings) {
	build_tree (child, tree, box);
    }
}


static void 
set_node_labels (XmuWidgetNode *node, int depth)
{
    Arg args[1];
    XmuWidgetNode *child;
    ViewresData *d = VData(node);

    if (!node) return;
    XtSetArg (args[0], XtNlabel, (options.show_variable ?
				  node->label : XmuWnClassname(node)));
    XtSetValues (d->instance, args, ONE);

    for (child = node->children; child; child = child->siblings) {
	set_node_labels (child, depth + 1);
    }
}


static void 
oneof_sensitive (Boolean choosea, Widget a, Widget b)
{
    static Arg args[1] = { { XtNsensitive, (XtArgVal) NULL } };

    args[0].value = (XtArgVal) TRUE;
    XtSetValues (choosea ? a : b, args, ONE);
    args[0].value = (XtArgVal) FALSE;
    XtSetValues (choosea ? b : a, args, ONE);
}

static void 
set_labeltype_menu (Boolean isvar, Boolean doall)
{
    options.show_variable = isvar;
    oneof_sensitive (isvar, view_widgets[VIEW_CLASSES],
		     view_widgets[VIEW_VARIABLES]);

    if (doall) {
	XUnmapWindow (XtDisplay(treeWidget), XtWindow(treeWidget));
	set_node_labels (topnode, 0);
	XawTreeForceLayout (treeWidget);
	XMapWindow (XtDisplay(treeWidget), XtWindow(treeWidget));
    }
}

static void 
set_orientation_menu (XtGravity grav, Boolean dosetvalues)
{
#define CHOOSE(val) (sensitiveargs + (grav != (val)))
    XtSetValues (view_widgets[VIEW_HORIZONTAL], CHOOSE(WestGravity), ONE);
    XtSetValues (view_widgets[VIEW_VERTICAL], CHOOSE(NorthGravity), ONE);
#undef CHOOSE

    if (dosetvalues) {
	Arg args[1];

	XtSetArg (args[0], XtNgravity, grav);
	XUnmapWindow (XtDisplay(treeWidget), XtWindow(treeWidget));
 	XtSetValues (treeWidget, args, ONE);
	XMapWindow (XtDisplay(treeWidget), XtWindow(treeWidget));
    }
}


/*****************************************************************************
 *                                                                           *
 *		     viewres - visual class browser for Xt                   *
 *                                                                           *
 *****************************************************************************/

int
main (int argc, char *argv[])
{
    Widget toplevel, pane, box, dummy, porthole, panner, form;
    XmuWidgetNode *rootNode; /* always the root of the resource hierarchy */
    XtAppContext app_con;
    Arg args[6];
    Dimension canvasWidth, canvasHeight, sliderWidth, sliderHeight;
    static XtCallbackRec callback_rec[2] = {{ NULL, NULL }, { NULL, NULL }};
    XtGravity grav;
    int i;

    ProgramName = argv[0];

    XtSetLanguageProc(NULL, (XtLanguageProc) NULL, NULL);

    toplevel = XtAppInitialize (&app_con, "Viewres", 
				Options, XtNumber (Options),
				&argc, argv, (char **) fallback_resources, 
				(ArgList) NULL, ZERO);
    if (argc != 1) usage ();

    initialize_widgetnode_list (&selected_list.elements,
				&selected_list.max_elements, 10);

    XtGetApplicationResources (toplevel, (XtPointer) &options,
			       Resources, XtNumber(Resources), NULL, ZERO);
    XmuWnInitializeNodes (widget_list, nwidgets);

    topnode = XmuWnNameToNode (widget_list, nwidgets, options.top_object);
    if (!topnode) {
	fprintf(stderr, "%s: no widget with name \"%s\" found.\n",
		ProgramName, options.top_object);
	exit(1);
    }

    XtAppAddActions (app_con, viewres_actions, XtNumber (viewres_actions));
    XtOverrideTranslations
	(toplevel, XtParseTranslationTable ("<Message>WM_PROTOCOLS: Quit()"));

    /*
     * create dummy widgets to initialize resources
     */
    XtSetArg (args[0], XtNwidth, 1);
    XtSetArg (args[1], XtNheight, 1);
    dummy = XtCreateWidget ("dummy", widgetClass, toplevel, args, TWO);
    rootNode = XmuWnNameToNode(widget_list, nwidgets, "Object");
    for (i = 0; i < nwidgets; i++) {
	XmuWidgetNode *node = &widget_list[i];
	XmuWnFetchResources (node, dummy, rootNode);
	node->data = (XtPointer) create_viewres_data (node);
    }
    XtDestroyWidget (dummy);

    pane = XtCreateManagedWidget ("pane", panedWidgetClass, toplevel,
				  (ArgList) NULL, ZERO);

    box = XtCreateManagedWidget ("buttonbox", boxWidgetClass, pane,
				 (ArgList) NULL, ZERO);
    (void) XtCreateManagedWidget ("quit", commandWidgetClass, box,
				  (ArgList) NULL, ZERO);

    /*
     * Format menu
     */
    XtSetArg (args[0], XtNmenuName, "viewMenu");
    viewButton = XtCreateManagedWidget ("view", menuButtonWidgetClass, box,
					args, ONE);
    viewMenu = XtCreatePopupShell ("viewMenu", simpleMenuWidgetClass, 
				   viewButton, (ArgList) NULL, ZERO);
    XtSetArg (args[0], XtNcallback, callback_rec);

#define MAKE_VIEW(n,v,name) \
    callback_rec[0].closure = (XtPointer) v; \
    view_widgets[n] = XtCreateManagedWidget (name, smeBSBObjectClass, \
					     viewMenu, args, ONE)
    callback_rec[0].callback = (XtCallbackProc) gravity_callback;
    MAKE_VIEW (VIEW_HORIZONTAL, WestGravity, "layoutHorizontal");
    MAKE_VIEW (VIEW_VERTICAL, NorthGravity, "layoutVertical");

    (void) XtCreateManagedWidget ("line1", smeLineObjectClass, viewMenu,
				  (ArgList) NULL, ZERO);

    callback_rec[0].callback = (XtCallbackProc) variable_labeltype_callback;
    MAKE_VIEW (VIEW_VARIABLES, TRUE, "namesVariable");
    MAKE_VIEW (VIEW_CLASSES, FALSE, "namesClass");

    (void) XtCreateManagedWidget ("line2", smeLineObjectClass, viewMenu,
				  (ArgList) NULL, ZERO);

    callback_rec[0].callback = (XtCallbackProc) show_resources_callback;
    MAKE_VIEW (VIEW_SHOW_RESOURCES, BOOL_ON, "viewResources");
    MAKE_VIEW (VIEW_HIDE_RESOURCES, BOOL_OFF, "viewNoResources");
#undef MAKE_VIEW

    /*
     * Select menu
     */
    XtSetArg (args[0], XtNmenuName, "selectMenu");
    selectButton = XtCreateManagedWidget ("select", menuButtonWidgetClass, box,
					  args, ONE);
    selectMenu = XtCreatePopupShell ("selectMenu", simpleMenuWidgetClass, 
				     selectButton, (ArgList) NULL, ZERO);
    XtSetArg (args[0], XtNcallback, callback_rec);
    callback_rec[0].callback = (XtCallbackProc) select_callback;
#define MAKE_SELECT(n,name) \
    callback_rec[0].closure = (XtPointer) n; \
    select_widgets[n] = XtCreateManagedWidget (name, smeBSBObjectClass, \
					       selectMenu, args, ONE)
    MAKE_SELECT (SELECT_NOTHING, "unselect");
    MAKE_SELECT (SELECT_ALL, "selectAll");
    MAKE_SELECT (SELECT_INVERT, "selectInvert");
    (void) XtCreateManagedWidget ("line1", smeLineObjectClass, selectMenu,
				  (ArgList) NULL, ZERO);
    MAKE_SELECT (SELECT_PARENT, "selectParent");
    MAKE_SELECT (SELECT_ANCESTORS, "selectAncestors");
    MAKE_SELECT (SELECT_CHILDREN, "selectChildren");
    MAKE_SELECT (SELECT_DESCENDANTS, "selectDescendants");
    (void) XtCreateManagedWidget ("line2", smeLineObjectClass, selectMenu,
				  (ArgList) NULL, ZERO);
    MAKE_SELECT (SELECT_HAS_RESOURCES, "selectHasResources");
    MAKE_SELECT (SELECT_SHOWN_RESOURCES, "selectShownResources");
#undef MAKE_SELECT

    form = XtCreateManagedWidget ("treeform", formWidgetClass, pane,
				  (ArgList) NULL, ZERO);
    /*
     * create the panner and the porthole and then connect them with the
     * callbacks (passing the other widget each callback)
     */
    XtSetArg (args[0], XtNbackgroundPixmap, None);  /* faster updates */
    porthole = XtCreateManagedWidget ("porthole", portholeWidgetClass, form,
				      args, ONE);
    panner = XtCreateManagedWidget ("panner", pannerWidgetClass, form,
				    (ArgList) NULL, ZERO);

    XtSetArg (args[0], XtNreportCallback, callback_rec);
    callback_rec[0].callback = (XtCallbackProc) panner_callback;
    callback_rec[0].closure = (XtPointer) porthole;
    XtSetValues (panner, args, ONE);

    callback_rec[0].callback = (XtCallbackProc) porthole_callback;
    callback_rec[0].closure = (XtPointer) panner;
    XtSetValues (porthole, args, ONE);

    /*
     * now that the panner and porthole are set up, insert the tree and 
     * fix up the menu, fill in the nodes
     */
    treeWidget = XtCreateManagedWidget ("tree", treeWidgetClass,
					porthole, (ArgList) NULL, ZERO);

    set_labeltype_menu (options.show_variable, FALSE);
    XtSetArg (args[0], XtNgravity, &grav);
    XtGetValues (treeWidget, args, ONE);
    set_orientation_menu (grav, FALSE);
    update_selection_items ();
    build_tree (topnode, treeWidget, (Widget) NULL);

    /*
     * Realize the tree, but do not map it (we set mappedWhenManaged to 
     * false up above).  Get the initial size of the tree so that we can
     * size the panner appropriately.
     */
    XtRealizeWidget (toplevel);

    wm_delete_window = XInternAtom(XtDisplay(toplevel), "WM_DELETE_WINDOW",
				   False);
    (void) XSetWMProtocols (XtDisplay(toplevel), XtWindow(toplevel),
                            &wm_delete_window, 1);

    XtSetArg (args[0], XtNwidth, &canvasWidth);
    XtSetArg (args[1], XtNheight, &canvasHeight);
    XtGetValues (treeWidget, args, TWO);

    XtSetArg (args[0], XtNwidth, &sliderWidth);
    XtSetArg (args[1], XtNheight, &sliderHeight);
    XtGetValues (porthole, args, TWO);

    XtSetArg (args[0], XtNcanvasWidth, canvasWidth);
    XtSetArg (args[1], XtNcanvasHeight, canvasHeight);
    XtSetArg (args[2], XtNsliderWidth, sliderWidth);
    XtSetArg (args[3], XtNsliderHeight, sliderHeight);
    XtSetValues (panner, args, FOUR);

    XRaiseWindow (XtDisplay(panner), XtWindow(panner));
    XtAppMainLoop (app_con);

    return 0;
}



/*****************************************************************************
 *                                                                           *
 *		   viewres translation table action routines                 *
 *                                                                           *
 *****************************************************************************/

/* ARGSUSED */
static void 
ActionQuit (Widget w, XEvent *event, 
    String *params, Cardinal *num_params)
{
    exit (0);
}

/* ARGSUSED */
static void 
ActionSetLableType (Widget w, XEvent *event, 
    String *params, Cardinal *num_params)
{
    const char *cmd;
    Boolean oldvar = options.show_variable, newvar;

    switch (*num_params) {
      case 0:
	cmd = "toggle";
	break;
      case 1:
	cmd = params[0];
	break;
      default:
	XBell (XtDisplay(w), 0);
	return;
    }

    if (XmuCompareISOLatin1 (cmd, "toggle") == 0) {
	newvar = !oldvar;
    } else if (XmuCompareISOLatin1 (cmd, "variable") == 0) {
	newvar = TRUE;
    } else if (XmuCompareISOLatin1 (cmd, "class") == 0) {
	newvar = FALSE;
    } else {
	XBell (XtDisplay(w), 0);
	return;
    }

    if (newvar != oldvar) set_labeltype_menu (newvar, TRUE);
    return;
}

/* ARGSUSED */
static void 
ActionSetOrientation (Widget w, XEvent *event, 
    String *params, Cardinal *num_params)
{
    XtGravity newgrav = ForgetGravity;

    if (*num_params < 1) {
	Arg arg;
	XtGravity oldgrav = ForgetGravity;

	XtSetArg (arg, XtNgravity, &oldgrav);
	XtGetValues (treeWidget, &arg, ONE);
	switch (oldgrav) {
	  case WestGravity:  newgrav = NorthGravity; break;
	  case NorthGravity:  newgrav = WestGravity; break;
	  case EastGravity:  newgrav = SouthGravity; break;
	  case SouthGravity:  newgrav = EastGravity; break;
	  default:
	    return;
	}
    } else {
	XrmValue fromval, toval;

	fromval.size = sizeof (String);
	fromval.addr = (XPointer) params[0];
	toval.size = sizeof (XtGravity);
	toval.addr = (XPointer) &newgrav;
	XtConvertAndStore (treeWidget, XtRString, &fromval,
			   XtRGravity, &toval);
    }

    switch (newgrav) {
      case WestGravity: case NorthGravity: case EastGravity: case SouthGravity:
	break;
      default:
	XBell (XtDisplay(w), 0);
	return;
    }

    set_orientation_menu (newgrav, TRUE);
    return;
}


static void 
do_single_arg (Widget w, String *params, Cardinal nparams, 
    struct _nametable table[] , int nentries, XtCallbackProc proc)
{
    int obj;
    int i;

    if (nparams != 1) {
	XBell (XtDisplay(w), 0);
	return;
    }

    for (i = 0; i < nentries; i++) {
	if (XmuCompareISOLatin1 (params[0], table[i].name) == 0) {
	    obj = table[i].value;
	    goto found;
	}
    }
    XBell (XtDisplay(w), 0);
    return;

  found:
    /*
     * use any old widget
     */
    (*proc) (w, (XtPointer) (long) obj, (XtPointer) NULL);
}


/* ARGSUSED */
static void 
ActionSelect (Widget w, XEvent *event, 
    String *params, Cardinal *num_params)
{
    do_single_arg (w, params, *num_params, select_nametable, 
		   (int) XtNumber(select_nametable), select_callback);
}


/* ARGSUSED */
static void ActionResources (Widget w, XEvent *event, 
    String *params, Cardinal *num_params)
{
    if (*num_params == 0) {
	show_resources_callback (w, (XtPointer) BOOL_TOGGLE, (XtPointer) NULL);
    } else {
	do_single_arg (w, params, *num_params, boolean_nametable,
		       (int) XtNumber(boolean_nametable),
		       show_resources_callback);
    }
}


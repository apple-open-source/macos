/*
 * $Xorg: widgets.c,v 1.4 2001/02/09 02:05:30 xorgcvs Exp $
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
/* $XFree86: xc/programs/editres/widgets.c,v 1.5 2001/01/17 23:44:52 dawes Exp $ */

/*
 * Code for creating all widgets used by EditRes.
 */

#include <stdio.h>
#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>	/* Get standard string definations. */

#include <X11/Xaw/AsciiText.h>
#include <X11/Xaw/Box.h>	
#include <X11/Xaw/Cardinals.h>	
#include <X11/Xaw/Label.h>	
#include <X11/Xaw/List.h>	
#include <X11/Xaw/MenuButton.h>	
#include <X11/Xaw/Paned.h>	
#include <X11/Xaw/Panner.h>	
#include <X11/Xaw/Porthole.h>	
#include <X11/Xaw/SmeBSB.h>	
#include <X11/Xaw/SmeLine.h>	
#include <X11/Xaw/SimpleMenu.h>	
#include <X11/Xaw/Toggle.h>	
#include <X11/Xaw/Tree.h>
#include <X11/Xaw/Viewport.h>	

#include "editresP.h"



/*
 * functions.
 */

static Widget CreateTopArea ( Widget parent );
static void CreateCommandMenu ( Widget parent, String toolkit );
static void CreateTreeCommandMenu ( Widget parent, String toolkit );
static void CreateResourceNameForm ( Widget parent, WNode * node );
static void SetToggleGroupLeaders ( WNode * node );
static void MakeBoxLookNice ( Widget dot, Widget star, Widget any, 
			      Widget single, Widget name, Widget class, 
			      int endbox );
static void CreateLists ( Widget parent, WNode * node, char **names, 
			  char **cons_names );
static void CreateValueWidget ( Widget parent, WNode * node );
static void PopupOnNode ( WNode * node, Widget shell );
static void FreeClientData ( Widget w, XtPointer ptr, XtPointer junk );
static void FreeResBox ( Widget w, XtPointer ptr, XtPointer junk );



/*      Function Name: RebuildMenusAndLabel
 *      Description: Determins if the user has selected an application
 *                   which uses a different toolkit.  Xt is the default.
 *                   If this is so, destroys and recreates the menus and
 *                   information label at the top of the application.
 *      Arguments: toolkit - name of the toolkit.
 *      Returns: none.
 */

static Widget box = NULL;
static Widget hPane = NULL;

#define Offset(index) sizeof(String) * index

#define res_entry(index, name, class) \
  {name, class, XtRString, sizeof(String), \
     Offset(index), XtRString, (XtPointer)NULL}

static XtResource resources[] = {
  res_entry(0, "label0", "Label0"),
  res_entry(1, "label1", "Label1"),
  res_entry(2, "label2", "Label2"),
  res_entry(3, "label3", "Label3"),
  res_entry(4, "label4", "Label4"),
  res_entry(5, "label5", "Label5"),
  res_entry(6, "label6", "Label6"),
  res_entry(7, "label7", "Label7"),
  res_entry(8, "label8", "Label8"),
  res_entry(9, "label9", "Label9"),
  res_entry(11, "label11", "Label11"),
  res_entry(12, "label12", "Label12"),
  res_entry(13, "label13", "Label13"),
  res_entry(14, "label14", "Label14"),
  res_entry(15, "label15", "Label15"),
  res_entry(16, "label16", "Label16"),
  res_entry(17, "label17", "Label17"),
  res_entry(18, "label18", "Label18"),
  res_entry(19, "label19", "Label19"),
  res_entry(20, "label20", "Label20"),
  res_entry(21, "label21", "Label21"),
  res_entry(22, "label22", "Label22"),
  res_entry(23, "label23", "Label23"),
  res_entry(24, "label24", "Label24"),
  res_entry(25, "label25", "Label25"),
  res_entry(26, "label26", "Label26"),
  res_entry(27, "label27", "Label27"),
  res_entry(28, "label28", "Label28"),
  res_entry(29, "label29", "Label29"),
  res_entry(30, "label30", "Label30"),
  res_entry(31, "label31", "Label31"),
  res_entry(32, "label32", "Label32"),
  res_entry(33, "label33", "Label33"),
  res_entry(34, "label34", "Label34"),
  res_entry(35, "label35", "Label35"),
  res_entry(36, "label36", "Label36")
};

#undef res_entry

#undef Offset

void 
RebuildMenusAndLabel(String toolkit)
{
  if (strcmp(global_effective_toolkit, toolkit)) {
    CreateCommandMenu(box, toolkit);
    CreateTreeCommandMenu(box, toolkit);
    XtDestroyWidget(global_screen_data.info_label);
    global_screen_data.info_label = XtCreateManagedWidget(toolkit, 
							  labelWidgetClass,
							  hPane, NULL, ZERO);
    /* get the new toolkit label application resources for info_label */
    XtGetApplicationResources(global_screen_data.info_label,
			      res_labels, resources,
			      XtNumber(resources), NULL, 0);
				  
    global_effective_toolkit = toolkit;
  }
}



/*      Function Name: BuildWidgetTree
 *      Description: Creates all widgets for Editres.
 *      Arguments: parent - the shell to put them into.
 *      Returns: none.
 */

void 
BuildWidgetTree(Widget parent)
{
    Widget paned, porthole, panner;

    paned = XtCreateManagedWidget("paned", panedWidgetClass, parent,
                                  NULL, ZERO);

    panner = CreateTopArea(paned);

    porthole = XtCreateManagedWidget("porthole", portholeWidgetClass,
                                     paned, NULL, ZERO);

/*
 * Allow the panner and porthole to talk to each other.
 */

    XtAddCallback(porthole, 
                  XtNreportCallback, PortholeCallback, (XtPointer) panner);
    XtAddCallback(panner, 
                  XtNreportCallback, PannerCallback, (XtPointer) porthole);

    global_tree_parent = porthole;
}




/*	Function Name: CreateTopArea
 *	Description: Creates the top part of the display
 *	Arguments: parent - widget to put this menu bar into.
 *	Returns: none. 
 */


static Widget
CreateTopArea(Widget parent)
{
    Widget panner;

    box = XtCreateManagedWidget("box", boxWidgetClass, parent, NULL, ZERO);

    CreateCommandMenu(box, "xt");
    CreateTreeCommandMenu(box, "xt");

    hPane = XtCreateManagedWidget("hPane",panedWidgetClass, parent, NULL,ZERO);

    {
	panner = XtCreateManagedWidget("panner", pannerWidgetClass, 
				       hPane, NULL, ZERO);

	global_screen_data.info_label = XtCreateManagedWidget("xt", 
							     labelWidgetClass,
							     hPane, NULL,ZERO);

	/* get the "xt label" application resources for info_label */
	XtGetApplicationResources(global_screen_data.info_label,
				  res_labels, resources,
				  XtNumber(resources), NULL, 0);
				  
    }

    return(panner);
}



/*	Function Name: SetEntriesInsensitive
 *	Description: Make menu entries unusable.
 *	Arguments: entries - address of widget array.
 *                 num - number of widgets.
 *                 sensitive - whether to sensitize or desensitize.
 *	Returns: none.
 */
void
SetEntriesSensitive(Widget *entries, int num, Boolean sensitive)
{
int i; for (i=0; i<num; i++) XtSetSensitive(entries[i], sensitive); 
}



/*	Function Name: CreateCommandMenu
 *	Description: Creates the command menu.
 *	Arguments: parent - widget to put this menu into.
 *                 toolkit - name given to the SimpleMenu widget.
 *	Returns: none.
 */

static Widget cmenu = NULL, cbutton = NULL;
/* at first most menu entries are insensitive */
static Boolean CM_set_insensitive = True;
Widget CM_entries[NUM_CM_ENTRIES];

static void
CreateCommandMenu(Widget parent, String toolkit)
{
    Arg args[1];

    if (cmenu) { XtDestroyWidget(cmenu); CM_set_insensitive = False; }
    else 
      cbutton = XtCreateManagedWidget("commands", menuButtonWidgetClass,
				      parent, NULL, ZERO);

    /* set the menu name to the toolkit name */
    XtSetArg(args[0], XtNmenuName, toolkit);
    XtSetValues(cbutton, args, ONE);

    cmenu = XtCreatePopupShell(toolkit, simpleMenuWidgetClass, cbutton,
			       NULL, ZERO);
    
    CM_entries[0] = XtCreateManagedWidget("sendTree", smeBSBObjectClass,cmenu,
					   NULL, ZERO);
    XtAddCallback(CM_entries[0], XtNcallback, SendTree, (XtPointer) TRUE);

    CM_entries[1]=XtCreateManagedWidget("refreshTree",smeBSBObjectClass,cmenu,
					 NULL, ZERO);
    XtAddCallback(CM_entries[1], XtNcallback, SendTree, (XtPointer) FALSE);

    CM_entries[2] = XtCreateManagedWidget("dumpTreeToFile", 
					   smeBSBObjectClass,cmenu,
					   NULL, ZERO);
    XtAddCallback(CM_entries[2], XtNcallback, DumpTreeToFile, NULL);

    CM_entries[3] = XtCreateManagedWidget("line", smeLineObjectClass, cmenu,
				  NULL, ZERO);
    CM_entries[4]= XtCreateManagedWidget("getResourceList", 
					  smeBSBObjectClass,cmenu,
					  NULL, ZERO);
    XtAddCallback(CM_entries[4], XtNcallback, GetResourceList, NULL);

    CM_entries[5] = XtCreateManagedWidget("setValues", smeBSBObjectClass, 
					   cmenu,
					   NULL, ZERO);
    XtAddCallback(CM_entries[5], XtNcallback, InitSetValues, NULL);

    CM_entries[6] = XtCreateManagedWidget("line", smeLineObjectClass, cmenu,
					   NULL, ZERO);

    CM_entries[7] = XtCreateManagedWidget("quit", smeBSBObjectClass, cmenu,
					   NULL, ZERO);
    XtAddCallback(CM_entries[7], XtNcallback, Quit, NULL);

    if (CM_set_insensitive) 
      SetEntriesSensitive(&CM_entries[CM_OFFSET], CM_NUM, False);
}



/*	Function Name: CreateTreeCommandMenu
 *	Description: Creats the command menu.
 *	Arguments: parent - widget to put this menu into.
 *	Returns: none.
 */

#define SELECT 0
#define ACTIVATE 1
#define LABEL 2
#define LINE 3
#define FIND 4
#define FLASH 5

struct tree_ops_menu {
    char * name;
    int type;
    XtPointer data;
};

static Widget tmenu = NULL, tbutton = NULL;
/* at first most menu entries are insensitive */
static Boolean TM_set_insensitive = True;
Widget TM_entries[NUM_TM_ENTRIES];

static void
CreateTreeCommandMenu(Widget parent, String toolkit)
{
    int i, number;
    static struct tree_ops_menu tree_menu[] = {
	{ "showClientWidget", FIND, (XtPointer) NULL },
        { "selectAll", SELECT, (XtPointer) SelectAll },
	{ "unselectAll", SELECT, (XtPointer) SelectNone },
	{ "invertAll", SELECT, (XtPointer) SelectInvert },
	{ "line", LINE, (XtPointer) NULL },
	{ "selectChildren", SELECT, (XtPointer) SelectChildren },
        { "selectParent", SELECT, (XtPointer) SelectParent },
	{ "selectDescendants", SELECT, (XtPointer) SelectDescendants },
        { "selectAncestors", SELECT, (XtPointer) SelectAncestors },
        { "line", LINE, (XtPointer) NULL },
        { "showWidgetNames", LABEL, (XtPointer) NameLabel },
        { "showClassNames", LABEL, (XtPointer) ClassLabel },
        { "showWidgetIDs", LABEL, (XtPointer) IDLabel},
        { "showWidgetWindows", LABEL, (XtPointer) WindowLabel },
        { "line", LINE, (XtPointer) NULL },
	{ "flashActiveWidgets", FLASH, (XtPointer) NULL }
    };
    Arg args[1];

    if (tmenu) { XtDestroyWidget(tmenu); TM_set_insensitive = False; }
    else
      tbutton = XtCreateManagedWidget("treeCommands", menuButtonWidgetClass,
				      parent, NULL, ZERO);

    XtSetArg(args[0], XtNmenuName, toolkit);
    XtSetValues(tbutton, args, ONE);

    tmenu = XtCreatePopupShell(toolkit, simpleMenuWidgetClass, tbutton,
			      NULL, ZERO);

    for ( i = 0, number = XtNumber(tree_menu) ; i < number ; i++) {
	XtCallbackProc func;
	WidgetClass class = smeBSBObjectClass;

	switch (tree_menu[i].type) {
	case SELECT:
	    func = TreeSelect;
	    break;
	case LABEL:
	    func = TreeRelabel;
	    break;
	case LINE:
	    func = NULL;
	    class = smeLineObjectClass;
	    break;
	case FIND:
	    func = FindWidget;
	    break;
	case FLASH:
	    func = FlashActiveWidgets;
	    break;
	default:
	    continue;
	}

	TM_entries[i] = XtCreateManagedWidget(tree_menu[i].name, class, tmenu,
				      NULL, ZERO);
	if (func != NULL) 
	    XtAddCallback(TM_entries[i], XtNcallback, func,tree_menu[i].data);
    }
    if (TM_set_insensitive) SetEntriesSensitive(&TM_entries[TM_OFFSET],
						 TM_NUM, False);
}



static Pixmap old_pixmap;

/*	Function Name: PrepareToLayoutTree
 *	Description: prepares the Tree widget to be layed out.
 *	Arguments: tree - the Tree widget.
 *	Returns: none
 */

void
PrepareToLayoutTree(Widget tree)
{
    Arg args[1];

    XtSetArg(args[0], XtNbackgroundPixmap, &old_pixmap);
    XtGetValues(XtParent(tree), args, ONE);

    XtSetArg(args[0], XtNbackgroundPixmap, None);
    XtSetValues(XtParent(tree), args, ONE);

    XtUnmapWidget(tree);
}



/*	Function Name: LayoutTree
 *	Description: Laysout the tree widget.
 *	Arguments: tree - the widget tree.
 *	Returns: none.
 */

void
LayoutTree(Widget tree)
{
    Arg args[1];
    
    XawTreeForceLayout(tree);
    XtMapWidget(tree); 

    XtSetArg(args[0], XtNbackgroundPixmap, old_pixmap);
    XtSetValues(XtParent(tree), args, ONE);
}



/************************************************************
 *
 * Functions for creating the Resource Box.
 *
 ************************************************************/

/*	Function Name: CreateResourceBoxWidgets
 *	Description: Creates the widgets that make up the resource box.
 *	Arguments: node - the widget node.
 *                 names - the list of names that make up the normal resources.
 *                 cons_names - the list of names that make up 
 *                              the constraint resources. 
 *	Returns: none.
 */

void
CreateResourceBoxWidgets(WNode *node, char **names, char **cons_names)
{
    Widget pane, box, button, viewport, pane_child;
    ResourceBoxInfo * res_box;
    Dimension max_width = WidthOfScreen(XtScreen(node->widget)) - 20;
    Dimension max_height = HeightOfScreen(XtScreen(node->widget)) - 40;

    res_box = (ResourceBoxInfo *) XtMalloc(sizeof(ResourceBoxInfo));
    node->resources->res_box = res_box;

    res_box->shell = XtVaCreatePopupShell(global_effective_toolkit,
					  /*RESOURCE_BOX,*/
					  transientShellWidgetClass,
					  node->widget,
					  XtNmaxWidth, max_width,
					  XtNmaxHeight, max_height, NULL);
    XtAddCallback(res_box->shell, XtNdestroyCallback,
		  FreeResBox, (XtPointer) node);

    pane = XtCreateManagedWidget("pane", panedWidgetClass, 
				 res_box->shell, NULL, ZERO);

    viewport = XtCreateManagedWidget("mainViewport", viewportWidgetClass,
				     pane, NULL, 0);
    pane_child = XtCreateManagedWidget("pane", panedWidgetClass, 
				       viewport, NULL, ZERO);

    res_box->res_label = XtCreateManagedWidget("resourceLabel", 
					       labelWidgetClass, 
					       pane_child, NULL, ZERO);

    CreateResourceNameForm(pane_child, node);
    CreateLists(pane_child, node, names, cons_names);
    CreateValueWidget(pane, node);

    XtSetKeyboardFocus(pane, res_box->value_wid); /* send keyboard to value. */

    box = XtCreateManagedWidget("commandBox", boxWidgetClass,
				 pane, NULL, ZERO);

    button = XtCreateManagedWidget("setFile", commandWidgetClass,
				   box, NULL, ZERO);
    XtAddCallback(button, XtNcallback, SetFile, NULL);

    button = XtCreateManagedWidget("save", commandWidgetClass,
				   box, NULL, ZERO);
    XtAddCallback(button, XtNcallback, SaveResource,(XtPointer) res_box);

    button = XtCreateManagedWidget("apply", commandWidgetClass,
				   box, NULL, ZERO);
    XtAddCallback(button, XtNcallback, ApplyResource,(XtPointer) node);

    button = XtCreateManagedWidget("saveAndApply", commandWidgetClass,
				   box, NULL, ZERO);
    XtAddCallback(button, XtNcallback, SaveResource,(XtPointer) res_box);
    XtAddCallback(button, XtNcallback, ApplyResource,(XtPointer) node);

    button = XtCreateManagedWidget("cancel", commandWidgetClass,
				   box, NULL, ZERO);
    XtAddCallback(button,XtNcallback,PopdownResBox,(XtPointer)res_box->shell);

    SetToggleGroupLeaders(node);
    PopupOnNode(node, res_box->shell);
}



/*	Function Name: CreateResourceNameForm
 *	Description: Creates the Form widget with children that represent
 *                   the full resource name for this object.
 *	Arguments: parent - parent of the form.
 *                 node - the node corrosponding to this object.
 *	Returns: none
 */

static void
CreateResourceNameForm(Widget parent, WNode *node)
{
    ResourceBoxInfo * res_box = node->resources->res_box;
    AnyInfo *new_info = NULL, *old_info;
    char **names, **classes;
    Widget form;
    NameInfo * name_info = NULL;
    Cardinal num_args;
    Arg args[10];
    int i;
    Widget dot, star, name, class, single, any;

    GetNamesAndClasses(node, &names, &classes);

    form = XtCreateManagedWidget("namesAndClasses", formWidgetClass,
				 parent, NULL, ZERO);

    name = class = any = NULL;
    i = 0;
    old_info = NULL;
    while (TRUE) {

	num_args = 0;
	XtSetArg(args[num_args], XtNfromHoriz, name); num_args++;
	XtSetArg(args[num_args], XtNradioData, "."); num_args++;
	dot = XtCreateManagedWidget("dot", toggleWidgetClass, 
				    form, args, num_args);
	XtAddCallback(dot, XtNcallback, 
		      ActivateWidgetsAndSetResourceString,(XtPointer) node);

	num_args = 0;
	XtSetArg(args[num_args], XtNfromHoriz, class); num_args++;
	XtSetArg(args[num_args], XtNfromVert, dot); num_args++;
	XtSetArg(args[num_args], XtNradioGroup, dot); num_args++;
	XtSetArg(args[num_args], XtNradioData, "*"); num_args++;
	star = XtCreateManagedWidget("star", toggleWidgetClass, 
				     form, args, num_args);
	XtAddCallback(star,XtNcallback, 
		      ActivateWidgetsAndSetResourceString, (XtPointer) node);

	if (name_info != NULL) {
	    name_info->next = (NameInfo *) XtMalloc(sizeof(NameInfo));
	    name_info = name_info->next;
	}
	else
	    res_box->name_info = 
		     name_info = (NameInfo *) XtMalloc(sizeof(NameInfo));

	name_info->sep_leader = dot;
	name_info->name_leader = NULL;

	if (names[i] != NULL) {
	    new_info = (AnyInfo *) XtMalloc(sizeof(AnyInfo));
	    new_info->node = node;
	    new_info->left_dot = dot;
	    new_info->left_star = star;
	    new_info->left_count = 0;
	    if (old_info != NULL) 
		old_info->right_count = &(new_info->left_count);
	}
	else if (old_info != NULL) 
	    old_info->right_count = NULL;

	if (old_info != NULL) {
	    old_info->right_dot = dot;
	    old_info->right_star = star;

	    XtAddCallback(any, XtNcallback, AnyChosen, (XtPointer) old_info);
	    XtAddCallback(any, XtNdestroyCallback, 
			  FreeClientData, (XtPointer) old_info);
	}

	if ( names[i] == NULL) /* no more name and class boxes. */
	    break;

	old_info = new_info;

	num_args = 0;
	XtSetArg(args[num_args], XtNfromHoriz, dot); num_args++;
	XtSetArg(args[num_args], XtNlabel, names[i]); num_args++;
	XtSetArg(args[num_args], XtNradioData, names[i]); num_args++;
	name = XtCreateManagedWidget("name", toggleWidgetClass, 
				     form, args, num_args);
	XtAddCallback(name,XtNcallback,
		      ActivateWidgetsAndSetResourceString,(XtPointer) node);

	num_args = 0;
	XtSetArg(args[num_args], XtNfromHoriz, star); num_args++;
	XtSetArg(args[num_args], XtNfromVert, name); num_args++;
	XtSetArg(args[num_args], XtNlabel, classes[i]); num_args++;
	XtSetArg(args[num_args], XtNradioGroup, name); num_args++;
	XtSetArg(args[num_args], XtNradioData, classes[i]); num_args++;
	class = XtCreateManagedWidget("class", toggleWidgetClass, 
				      form,args,num_args);
	XtAddCallback(class, XtNcallback,
		      ActivateWidgetsAndSetResourceString,(XtPointer) node);

	num_args = 0;
	XtSetArg(args[num_args], XtNfromHoriz, star); num_args++;
	XtSetArg(args[num_args], XtNfromVert, class); num_args++;
	XtSetArg(args[num_args], XtNradioData, "?"); num_args++;
	XtSetArg(args[num_args], XtNradioGroup, name); num_args++;
	single = XtCreateManagedWidget("single", toggleWidgetClass, 
				       form, args, num_args);
	XtAddCallback(single,XtNcallback,
		      ActivateWidgetsAndSetResourceString,(XtPointer) node);

	num_args = 0;
	XtSetArg(args[num_args], XtNfromHoriz, any); num_args++;
	XtSetArg(args[num_args], XtNfromVert, single); num_args++;
	XtSetArg(args[num_args], XtNradioGroup, name); num_args++;
	XtSetArg(args[num_args], XtNradioData, ANY_RADIO_DATA); num_args++;
	any = XtCreateManagedWidget("any", toggleWidgetClass, 
				    form, args, num_args);

	name_info->name_leader = name;

	MakeBoxLookNice(dot, star, any, single, name, class,
			(i == 0 ? -1 : (names[i + 1] ? 0 : 1)));

	i++;
    }

    name_info->next = NULL;
    XtFree((char *)names);		/* Free what you allocate... */
    XtFree((char *)classes);
}



/*	Function Name: SetToggleGroupLeaders
 *	Description: Sets the leaders of each toggle group.
 *                 node - The widget node containing this res box.
 *	Returns: none
 */

static void
SetToggleGroupLeaders(WNode *node)
{
    NameInfo *name;
    ResourceBoxInfo * res_box = node->resources->res_box;
    static Arg args[] = {
	{XtNstate, (XtArgVal) TRUE}
    };

    for (name  = res_box->name_info; name != NULL; name = name->next) {
	XtSetValues(name->sep_leader, args, XtNumber(args));
	if (name->name_leader != NULL)
	    XtSetValues(name->name_leader, args, XtNumber(args));
    }
    SetResourceString(NULL, (XtPointer) node, NULL);
}



/*	Function Name: MakeBoxLookNice
 *	Description: Resizes the box that contains the resource names
 *                   to look a bit nicer.
 *	Arguments: dot, star - the widgets containing the separator types.
 *                 any, single, name, class - the widgets that contain the
 *                                     name and class of this object.
 *	Returns: none.
 */
 
static void
MakeBoxLookNice(Widget dot, Widget star, Widget any, Widget single,
		Widget name, Widget class, int endbox)
{

#define MAX_HDIST 3

    Arg args[10];
    Cardinal num_args;
    Dimension any_width, name_class_width, dot_star_width;
    Dimension width_1, width_2;
    int h_dist[MAX_HDIST];
    int i;

    /*
     * Make sure that the dot and star widgets are the same size.
     */

    num_args = 0;
    XtSetArg(args[num_args], XtNhorizDistance, &(h_dist[0])); num_args++;
    XtSetArg(args[num_args], XtNwidth, &width_1); num_args++;
    XtGetValues(dot, args, num_args);

    num_args = 0;
    XtSetArg(args[num_args], XtNhorizDistance, &(h_dist[1])); num_args++;
    XtSetArg(args[num_args], XtNwidth, &width_2); num_args++;
    XtGetValues(star, args, num_args);

    num_args = 0;
    XtSetArg(args[num_args], XtNhorizDistance, &(h_dist[2])); num_args++;
    XtSetArg(args[num_args], XtNwidth, &any_width); num_args++;
    XtGetValues(any, args, num_args);
    
    dot_star_width = (width_1 > width_2) ? width_1 : width_2;
    for (i = 1 ; i < MAX_HDIST; i++) {
	if (h_dist[i] > h_dist[0]) h_dist[0] = h_dist[i];
    }

    num_args = 0;
    XtSetArg(args[num_args], XtNhorizDistance, h_dist[0]); num_args++;
    XtSetValues(any, args, num_args);
    
    /*
     * Add a new arg, and continue...
     */
    XtSetArg(args[num_args], XtNwidth, dot_star_width); num_args++; 
    XtSetValues(star, args, num_args);
    XtSetValues(dot, args, num_args);


    /*
     * Now make sure that the Any Widget is as wide as the longest
     * of the name and class widgets, plus space for the dot and star widgets.
     * Don't forget the Form widget's internal space.
     */

    num_args = 0;
    XtSetArg(args[num_args], XtNwidth, &width_1); num_args++;
    XtSetArg(args[num_args], XtNhorizDistance, &(h_dist[0])); num_args++;
    XtGetValues(name, args, num_args);

    num_args = 0;
    XtSetArg(args[num_args], XtNwidth, &width_2); num_args++;
    XtSetArg(args[num_args], XtNhorizDistance, &(h_dist[1])); num_args++;
    XtGetValues(class, args, num_args);

    if (width_2 > width_1) width_1 = width_2;
    if (h_dist[1] > h_dist[0]) h_dist[0] = h_dist[1];

    num_args = 0;
    XtSetArg(args[num_args], XtNwidth, &width_2); num_args++;
    XtSetArg(args[num_args], XtNhorizDistance, &(h_dist[1])); num_args++;
    XtGetValues(single, args, num_args);

    name_class_width = (width_1 > width_2) ? width_1 : width_2;
    if (h_dist[1] > h_dist[0]) h_dist[0] = h_dist[1];
    if (any_width > name_class_width)
	name_class_width = any_width;
    any_width = dot_star_width + h_dist[0] + name_class_width;

    num_args = 0;
    if (endbox < 0) {
	any_width -= dot_star_width & 1;
	XtSetArg(args[num_args], XtNhorizDistance,
		 h_dist[2] + (dot_star_width >> 1) + (dot_star_width & 1));
	++num_args;
    }

    XtSetArg(args[num_args], XtNwidth, any_width); num_args++;
    XtSetValues(any, args, num_args);	

    num_args = 0;
    XtSetArg(args[num_args], XtNwidth, name_class_width); num_args++;
    XtSetArg(args[num_args], XtNhorizDistance, h_dist[0]); num_args++;
    XtSetValues(name, args, num_args);	
    XtSetValues(class, args, num_args);	
    XtSetValues(single, args, num_args);	
}



/*	Function Name: CreateLists
 *	Description: Creates the list widgets for the normal and constraint 
 *                   resources
 *	Arguments: parent - parent of the lists.
 *                 node - The widget node containing this res box.
 *                 names, cons_names - lists for norm and cons resource boxes.
 *	Returns: none
 */

static char* noneList[] = {"None"};

static void
CreateLists(Widget parent, WNode *node, char **names, char **cons_names)
{
    Widget viewport;
    Cardinal num_args;
    ResourceBoxInfo * res_box = node->resources->res_box;
    Arg args[3];

    (void) XtCreateManagedWidget("namesLabel", labelWidgetClass, 
				 parent, NULL, ZERO);
    
    num_args = 0;
    /* if the first list item is the widget name we want an empty
     * list.
     */
    if (!names) {
        XtSetArg(args[num_args], XtNlist, noneList); num_args++;
        XtSetArg(args[num_args], XtNnumberStrings, 1); num_args++;
        XtSetArg(args[num_args], XtNsensitive, False); num_args++;
    }
    else { XtSetArg(args[num_args], XtNlist, names); num_args++; }
    viewport = XtCreateManagedWidget("normalViewport", viewportWidgetClass,
				     parent, NULL, 0);
    res_box->norm_list = XtCreateManagedWidget("namesList", listWidgetClass, 
				      viewport, args, num_args);
    XtAddCallback(res_box->norm_list, XtNcallback, 
		  ResourceListCallback, (XtPointer) node);
    XtAddCallback(res_box->norm_list, XtNdestroyCallback, 
		  FreeClientData, (XtPointer) names);

    if (cons_names != NULL) {
	(void) XtCreateManagedWidget("constraintLabel", labelWidgetClass, 
				     parent, NULL, ZERO);
	
	num_args = 0;
	XtSetArg(args[num_args], XtNlist, cons_names); num_args++;	
	viewport = XtCreateManagedWidget("constraintViewport", viewportWidgetClass,
					 parent, NULL, 0);
	res_box->cons_list = XtCreateManagedWidget("constraintList", 
						   listWidgetClass, 
						   viewport, args, num_args);
	XtAddCallback(res_box->cons_list, XtNcallback, 
		      ResourceListCallback, (XtPointer) node);
	XtAddCallback(res_box->cons_list, XtNdestroyCallback, 
		      FreeClientData, (XtPointer) cons_names);
    }
    else 
	res_box->cons_list = NULL;
}

/*	Function Name: CreateValueWidget
 *	Description: Creates the value widget for entering the resources value.
 *	Arguments: parent - parent of this widget.
 *                 res_box - the resource box info.
 *	Returns: none.
 */

static void
CreateValueWidget(Widget parent, WNode *node)
{
    Widget form, label;
    Cardinal num_args;
    Arg args[10];
    ResourceBoxInfo * res_box = node->resources->res_box;
    
    form = XtCreateManagedWidget("valueForm", formWidgetClass,
				 parent, NULL, ZERO);

    num_args = 0;
    XtSetArg(args[num_args], XtNleft, XawChainLeft); num_args++;
    XtSetArg(args[num_args], XtNright, XawChainLeft); num_args++;
    XtSetArg(args[num_args], XtNtop, XawChainTop); num_args++;
    XtSetArg(args[num_args], XtNbottom, XawChainBottom); num_args++;
    label = XtCreateManagedWidget("valueLabel", labelWidgetClass, 
				 form, args, num_args);

    num_args = 0;
    XtSetArg(args[num_args], XtNfromHoriz, label); num_args++;
    XtSetArg(args[num_args], XtNleft, XawChainLeft); num_args++;
    XtSetArg(args[num_args], XtNright, XawChainRight); num_args++;
    XtSetArg(args[num_args], XtNtop, XawChainTop); num_args++;
    XtSetArg(args[num_args], XtNbottom, XawChainBottom); num_args++;
    res_box->value_wid = XtCreateManagedWidget("valueText", 
					       asciiTextWidgetClass, 
					       form, args, num_args);
#ifdef notdef
    XtAddCallback(XawTextGetSource(res_box->value_wid), XtNcallback,
		  SetResourceString, (XtPointer) node);
#endif
}



/*	Function Name: PopupOnNode
 *	Description: Pops a shell widget up centered on the node specified.
 *	Arguments: node - the node.
 *                 shell - the shell to popup.
 *	Returns: none.
 */

static void
PopupOnNode(WNode *node, Widget shell)
{
    Arg args[3];
    Cardinal num_args;
    Position x, y, max_loc;
    Dimension width, height, bw;

    num_args = 0;
    XtSetArg(args[num_args], XtNwidth, &width); num_args++;
    XtSetArg(args[num_args], XtNheight, &height); num_args++;
    XtSetArg(args[num_args], XtNborderWidth, &bw); num_args++;
    XtGetValues(node->widget, args, num_args);
    XtTranslateCoords(node->widget, 
		      (Position) (width/2 + bw), (Position) (height/2 + bw),
		      &x, &y);
    
    XtOverrideTranslations
      (shell, XtParseTranslationTable ("<Message>WM_PROTOCOLS: quit()"));
    XtRealizeWidget(shell);
    wm_delete_window = XInternAtom(XtDisplay(shell), "WM_DELETE_WINDOW",
				   False);
    (void) XSetWMProtocols (XtDisplay(shell), XtWindow(shell),
                            &wm_delete_window, 1);
    XtGetValues(shell, args, num_args);	/* use same arg_list. */

    x -= (Position) (width/2 + bw);
    y -= (Position) (height/2 + bw);

    max_loc = WidthOfScreen(XtScreen(shell)) - (Position) (width + 2 * bw);
    if (x > max_loc)
	x = max_loc;
    if (x < 0)
	x = 0;

    max_loc = HeightOfScreen(XtScreen(shell)) - (Position) (height + 2 * bw);
    if (y > max_loc)
	y = max_loc;
    if (y < 0) 
	y = 0;

    num_args = 0;
    XtSetArg(args[num_args], XtNx, x); num_args++;
    XtSetArg(args[num_args], XtNy, y); num_args++;
    XtSetValues(shell, args, num_args);

    XtPopup(shell, XtGrabNone);
}



/*	Function Name: FreeClientData
 *	Description: Frees the client data passed to this function.
 *	Arguments: w - UNUSED.
 *                 list_ptr - pointer to the list to check.
 *                 junk - UNUSED.
 *	Returns: none
 */

/* ARGSUSED */
static void
FreeClientData(Widget w, XtPointer ptr, XtPointer junk)
{
    XtFree(ptr);
}



/*	Function Name: FreeResBox.
 *	Description: Frees resource box allocated memory.
 *	Arguments: w - UNUSED.
 *                 ptr - pointer to the node that has this resources box.
 *                 junk - UNUSED.
 *	Returns: none
 */

/* ARGSUSED */
static void
FreeResBox(Widget w, XtPointer ptr, XtPointer junk)
{
    WNode * node = (WNode *) ptr;
    NameInfo *old_name, *name = node->resources->res_box->name_info;

    global_resource_box_up = FALSE;
    
    XtFree((XtPointer) node->resources->res_box);
    node->resources->res_box = NULL;

    while (name != NULL) {
	old_name = name;
	name = name->next;
	XtFree((XtPointer) old_name);
    } 
}


    

/*
 * $Xorg: wtree.c,v 1.4 2001/02/09 02:05:30 xorgcvs Exp $
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
/* $XFree86: xc/programs/editres/wtree.c,v 1.4 2001/01/17 23:44:53 dawes Exp $ */

#include <stdio.h>
#include <X11/Intrinsic.h>
#include <X11/Xutil.h>
#include <X11/StringDefs.h>

#include <X11/Xaw/Cardinals.h>
#include <X11/Xaw/Toggle.h>
#include <X11/Xaw/Viewport.h>	
#include <X11/Xaw/Tree.h>

#include "editresP.h"

static void AddNodeToActiveList ( WNode * node );
static void RemoveNodeFromActiveList ( WNode * node );
static Boolean IsActiveNode ( WNode * node );
static void AddNode ( WNode ** top_node, WidgetTreeInfo * info, 
		      TreeInfo * tree_info );
static void FillNode ( WidgetTreeInfo * info, WNode * node, 
		       TreeInfo * tree_info );
static void AddChild ( WNode * parent, WNode * child );
static WNode ** CopyActiveNodes ( TreeInfo * tree_info );

/*	Function Name: BuildVisualTree
 *	Description: Creates the Tree and shows it.
 *	Arguments: tree_parent - parent of the tree widget.
 *                 event - the event that caused this action.
 *	Returns: none.
 */

/* ARGSUSED */
void
BuildVisualTree(tree_parent, event)
Widget tree_parent;
Event * event;
{
    WNode * top;
    char msg[BUFSIZ];

    if (global_tree_info != NULL) {
	XtDestroyWidget(global_tree_info->tree_widget);
	XtFree((char *)global_tree_info->active_nodes);
	XtFree((char *)global_tree_info);
    }

    global_tree_info = CreateTree(event);
    top = global_tree_info->top_node;

    global_tree_info->tree_widget = XtCreateWidget("tree", treeWidgetClass,
						   tree_parent, NULL, ZERO);

    if (top == NULL) {
	SetMessage(global_screen_data.info_label,
		   res_labels[27]);
	return;
    }

    AddTreeNode(global_tree_info->tree_widget, top);

    if (XtIsRealized(tree_parent)) /* hack around problems in Xt. */
	XtRealizeWidget(global_tree_info->tree_widget);

    XtManageChild(global_tree_info->tree_widget);

    sprintf(msg, res_labels[11], top->name, top->class);
    SetMessage(global_screen_data.info_label, msg);
}

/*	Function Name: AddTreeNode
 *	Description: Adds all nodes below this to the Tree widget.
 *	Arguments: parent - parent of the tree widget.
 *                 top - the top node of the tree.
 *	Returns: the tree widget.
 *
 * NOTE: This is a recursive function.
 */

void
AddTreeNode(tree, top) 
Widget tree;
WNode * top;
{
    int i;
    Arg args[1];
    Cardinal num_args = 0;
    char msg[BUFSIZ];

    if (top->parent != NULL) {
	if (top->parent->widget == NULL) {
	    sprintf( msg, res_labels[28],
		    top->name, top->parent->name, "not been created yet");
	    SetMessage(global_screen_data.info_label, msg);
	}
	XtSetArg(args[num_args], XtNtreeParent, top->parent->widget);
	num_args++;
    }

    top->widget = XtCreateManagedWidget(top->name, toggleWidgetClass, tree,
					args, num_args);

    if (XSaveContext(XtDisplay(top->widget), (Window) top->widget, 
		     NODE_INFO, (XPointer) top) != 0) {
	sprintf( msg, res_labels[29], top->name);
	SetMessage(global_screen_data.info_label, msg);
    }	

    XtAddCallback(top->widget, XtNcallback, TreeToggle, (XtPointer) top);

    for (i = 0; i < top->num_children; i++) 
	AddTreeNode(tree, top->children[i]);
}

/*	Function Name: TreeToggle
 *	Description: Called whenever a tree node is toggled.
 *	Arguments: w - the tree widget.
 *                 node_ptr - pointer to this node's information.
 *                 state_ptr - state of the toggle.
 *	Returns: none.
 */

/* ARGSUSED */
void
TreeToggle(w, node_ptr, state_ptr)
Widget w;
XtPointer node_ptr, state_ptr;
{
    Boolean state = (Boolean)(long) state_ptr;
    WNode * node = (WNode *) node_ptr;

    if (state) 
	AddNodeToActiveList(node);
    else
	RemoveNodeFromActiveList(node);
}

/*	Function Name: AddNodeToActiveList
 *	Description: Adds this node to the list of active toggles.
 *	Arguments: node - node to add.
 *	Returns: none.
 */

static void
AddNodeToActiveList(node)
WNode * node;
{
    TreeInfo * info = node->tree_info;

    if (IsActiveNode(node))	/* node already active. */
	return;

    if (info->num_nodes >= info->alloc_nodes) {
	info->alloc_nodes += NUM_INC;
	info->active_nodes =(WNode **)XtRealloc((XtPointer) info->active_nodes,
						sizeof(WNode *) * 
						     info->alloc_nodes);
    }

    info->active_nodes[info->num_nodes++] = node;
}

/*	Function Name: RemoveNodeFromActiveList
 *	Description: Removes a node from the active list.
 *	Arguments: node - node to remove.
 *	Returns: none.
 */

static void
RemoveNodeFromActiveList(node)
WNode * node;
{
    TreeInfo * info = node->tree_info;
    Boolean found_node = FALSE;
    int i;

    if (!IsActiveNode(node))	/* This node is not active. */
	return;

    for (i = 0; i < info->num_nodes; i++) {
	if (found_node)
	    info->active_nodes[i - 1] = info->active_nodes[i];
	else if (info->active_nodes[i] == node) 
	    found_node = TRUE;
    }

    info->num_nodes--;
}

/*	Function Name: IsActiveNode
 *	Description: returns TRUE is this node is on the active list.
 *	Arguments: node - node to check.
 *	Returns: see above.
 */

static Boolean
IsActiveNode(node)
WNode * node;
{
    TreeInfo * info = node->tree_info;
    int i;

    for (i = 0; i < info->num_nodes; i++) 
	if (info->active_nodes[i] == node)
	    return(TRUE);

    return(FALSE);
}
    
/*	Function Name: CreateTree
 *	Description: Creates a widget tree give a list of names and classes.
 *	Arguments: event - the information from the client.
 *	Returns: The tree_info about this new tree.
 */
    
TreeInfo *
CreateTree(event)
Event * event;
{
    SendWidgetTreeEvent * send_event = (SendWidgetTreeEvent *) event;
    int i;

    TreeInfo * tree_info;

    tree_info = (TreeInfo *) XtMalloc( (Cardinal) sizeof(TreeInfo));

    tree_info->tree_widget = NULL;
    tree_info->top_node = NULL;
    tree_info->active_nodes = NULL;
    tree_info->num_nodes = tree_info->alloc_nodes = 0;
    tree_info->flash_widgets = NULL;
    tree_info->num_flash_widgets = tree_info->alloc_flash_widgets = 0;

    for ( i = 0; i < (int)send_event->num_entries; i++)
	AddNode(&(tree_info->top_node), (send_event->info + i), tree_info);

    return(tree_info);
}

/*	Function Name: PrintNodes
 *	Description: Prints all nodes.
 *	Arguments: top - the top node.
 *	Returns: none.
 */

void
PrintNodes(top)
WNode * top;
{
    int i;

    if (top->parent == NULL) 
	printf("Top of Tree, Name: %10s, ID: %10ld, Class: %10s\n", 
	       top->name, top->id, top->class);
    else
	printf("Parent %10s, Name: %10s, ID: %10ld, Class: %10s\n", 
	       top->parent->name, top->name, top->id, top->class);

    for (i = 0; i < top->num_children; i++) 
	PrintNodes(top->children[i]);
}

/*	Function Name: _TreeRelabel
 *	Description: Modifies the selected elements of the tree
 *	Arguments: tree_info - the tree we are working on.
 *                 type - type of selection to perform
 *	Returns: none.
 */

void
_TreeRelabel(tree_info, type)
TreeInfo * tree_info;
LabelTypes type;
{
    WNode * top;

    if (tree_info == NULL) {
	SetMessage(global_screen_data.info_label,
		   res_labels[17]);
	return;
    }

    top = tree_info->top_node;

    PrepareToLayoutTree(tree_info->tree_widget); 
    _TreeRelabelNode(top, type, TRUE);
    LayoutTree(tree_info->tree_widget); 
}

/*	Function Name: _TreeSelect
 *	Description: Activates relatives of the active nodes, as specified
 *                   by type, or Selects all nodes as specified by type.
 *	Arguments: tree_info - information about the tree to work on.
 *                 type - type of activate to invode.
 *	Returns: none.
 */

void
_TreeSelect(TreeInfo *tree_info, SelectTypes type)
{
    WNode ** active_nodes;
    Cardinal num_active_nodes;
    int i;

    if (tree_info == NULL) {
	SetMessage(global_screen_data.info_label,
		   res_labels[17]);
	return;
    }

    switch(type) {
    case SelectNone:
    case SelectAll:
    case SelectInvert:
	_TreeSelectNode(tree_info->top_node, type, TRUE);
	return;
    default:
	break;			/* otherwise continue. */
    }

    if (tree_info->num_nodes == 0) {
	SetMessage(global_screen_data.info_label,
		   res_labels[18]);
	return;
    }

    active_nodes = CopyActiveNodes(tree_info);
    num_active_nodes = tree_info->num_nodes;

    for (i = 0; i < num_active_nodes; i++)
	_TreeActivateNode(active_nodes[i], type);

    XtFree((XtPointer) active_nodes);
}

/*	Function Name: _TreeSelectNode
 *	Description: Modifies the state of a node and all its decendants.
 *	Arguments: node - node to operate on.
 *                 type - type of selection to perform.
 *                 recurse - whether to continue on down the tree.
 *	Returns: none.
 */

void
_TreeSelectNode(WNode *node, SelectTypes type, Boolean recurse)
{
    int i;
    Arg args[1];
    Boolean state;

    switch(type) {
    case SelectAll:
	state = TRUE;
	break;
    case SelectNone:
	state = FALSE;
	break;
    case SelectInvert:
	XtSetArg(args[0], XtNstate, &state);
	XtGetValues(node->widget, args, ONE);
	
	state = !state;
	break;
    default:
	SetMessage(global_screen_data.info_label,
		   res_labels[16]);
	return;
    }

    XtSetArg(args[0], XtNstate, state);
    XtSetValues(node->widget, args, ONE);
    TreeToggle(node->widget, (XtPointer) node, (XtPointer)(long) state);

    if (!recurse)
	return;

    for (i = 0; i < node->num_children; i++) 
	_TreeSelectNode(node->children[i], type, recurse);
}

/*	Function Name: _TreeRelabelNodes
 *	Description: Modifies the node and all its decendants label.
 *	Arguments: node - node to operate on.
 *                 type - type of selection to perform.
 *                 recurse - whether to continue on down the tree.
 *	Returns: none.
 */

void
_TreeRelabelNode(WNode *node, LabelTypes type, Boolean recurse)
{
    int i;
    Arg args[1];
    char buf[30];
    char *label;

    switch(type) {
    case ClassLabel:
	XtSetArg(args[0], XtNlabel, node->class);
	break;
    case NameLabel:
	XtSetArg(args[0], XtNlabel, node->name);
	break;
    case IDLabel:
	sprintf(buf, "id: 0x%lx", node->id);
	XtSetArg(args[0], XtNlabel, buf);
	break;
    case WindowLabel:
	if (node->window == EDITRES_IS_UNREALIZED) 
	    strcpy(buf, "unrealized widget");
	else if (node->window == EDITRES_IS_OBJECT) 
	    strcpy(buf, "non windowed object");	    
	else
	    sprintf(buf, "win: 0x%lx", node->window);
	    
	XtSetArg(args[0], XtNlabel, buf);
	break;
    case ToggleLabel:
	XtSetArg(args[0], XtNlabel, &label);
	XtGetValues(node->widget, args, ONE);
	if (label && !strcmp(label, node->name))
	    XtSetArg(args[0], XtNlabel, node->class);
	else
	    XtSetArg(args[0], XtNlabel, node->name);
	break;
    default:
	SetMessage(global_screen_data.info_label,
		   res_labels[32]);
	return;
    }

    XtSetValues(node->widget, args, ONE);

    if (!recurse)
	return;

    for (i = 0; i < node->num_children; i++) 
	_TreeRelabelNode(node->children[i], type, recurse);
}

/*	Function Name: _TreeActivateNode
 *	Description: Activates relatives of the node specfied, as specified
 *                   by type.
 *	Arguments: node - node to opererate on.
 *                 type - type of activate to invode.
 *	Returns: none.
 */

void
_TreeActivateNode(node, type)
WNode * node;
SelectTypes type;
{
    Arg args[1];
    int i;

    XtSetArg(args[0], XtNstate, TRUE);

    if ((type == SelectParent) || (type == SelectAncestors)) {
	node = node->parent;
	if (node == NULL)
	    return;

	XtSetValues(node->widget, args, ONE);	
	AddNodeToActiveList(node);

	if (type == SelectAncestors)
	    _TreeActivateNode(node, type);	
    }
    else if ((type == SelectChildren) || (type == SelectDescendants)) 
	for (i = 0; i < node->num_children; i++) {
	    AddNodeToActiveList(node->children[i]);
	    XtSetValues(node->children[i]->widget, args, ONE);
	    if (type == SelectDescendants)
		_TreeActivateNode(node->children[i], type);
	}
    else
	SetMessage(global_screen_data.info_label,
		   res_labels[33]);	
}

/************************************************************
 *
 * Non - Exported Functions. 
 *
 ************************************************************/


/*	Function Name: AddNode
 *	Description: adds a node to the widget tree.
 *	Arguments: top_node - a pointer to the current top node.
 *                 info - the info from the client about the widget tree.
 *                 tree_info - global information on this tree.
 *	Returns: none.
 */

static void
AddNode(top_node, info, tree_info)
WNode ** top_node;
WidgetTreeInfo * info;
TreeInfo * tree_info;
{
    WNode *node, *parent;
    Boolean early_break = FALSE;
    Cardinal number = info->widgets.num_widgets;

    if ( (node = FindNode(*top_node, info->widgets.ids, number)) == NULL) {
	node = (WNode *) XtCalloc(sizeof(WNode), ONE);

	node->id = info->widgets.ids[number - 1];
	FillNode(info, node, tree_info);

	for ( number--; number > 0; number--, node = parent) {
	    parent = FindNode(*top_node, info->widgets.ids, number);
	    if (parent == NULL) {
		parent = (WNode *) XtCalloc(sizeof(WNode), ONE);
		parent->id = info->widgets.ids[number - 1];
	    }
	    else
		early_break = TRUE;

	    AddChild(parent, node);

	    if (early_break) 
		break;
	}

	if (!early_break) {
	    if (node->parent == NULL)
		*top_node = node;
	    else
		*top_node = node->parent;
	}
    }
    else
	FillNode(info, node, tree_info);
}

/*	Function Name: FillNode
 *	Description: Fills in everything but the node id in the node.
 *	Arguments: info - the info from the client.
 *                 node - node to fill.
 *                 tree_info - global information on this tree.
 *	Returns: none
 */

static void
FillNode(info, node, tree_info)
WidgetTreeInfo * info;
WNode * node;
TreeInfo * tree_info;
{
    node->class = info->class;
    info->class = NULL;	/* keeps it from deallocating. */
    node->name = info->name;
    info->name = NULL;
    node->window = info->window;
    node->tree_info = tree_info;
}

/*	Function Name: AddChild
 *	Description: Adds a child to an existing node.
 *	Arguments: parent - parent node.
 *                 child - child node to add.
 *	Returns: none.
 */

static void
AddChild(parent, child)
WNode * parent, * child;
{
    if (parent->num_children >= parent->alloc_children) {
	parent->alloc_children += NUM_INC;
	parent->children = (WNode **) XtRealloc((char *)parent->children, 
				     sizeof(WNode *) * parent->alloc_children);
    }

    parent->children[parent->num_children] = child;
    (parent->num_children)++;

    child->parent = parent;
}

/************************************************************
 *
 *  Functions that operate of the current tree.
 * 
 ************************************************************/
    
/*	Function Name: CopyActiveNodes
 *	Description: returns a copy of the currently selected nodes.
 *	Arguments: tree_info - the tree info struct.
 *	Returns: a copy of the selected nodes.
 */

static WNode ** 
CopyActiveNodes(tree_info)
TreeInfo * tree_info;
{
    WNode ** list;
    int i;

    if ( (tree_info == NULL) || (tree_info->num_nodes == 0))
	return(NULL);

    list = (WNode **) XtMalloc(sizeof(WNode *) * tree_info->num_nodes);

    for (i = 0; i < tree_info->num_nodes; i++)
	list[i] = tree_info->active_nodes[i];

    return(list);
}

/*	Function Name: SetAndCenterTreeNode
 *	Description: Deactivates all nodes, activates the one specified, and
 *                   and moves the tree to be centered on the current node.
 *	Arguments: node - node to use.
 *	Returns: none.
 */

void
SetAndCenterTreeNode(node)
WNode * node;
{
    Arg args[5];
    Cardinal num_args;
    Position node_x, node_y;
    Dimension port_width, port_height;
    Dimension node_width, node_height, node_bw;

    _TreeSelect(node->tree_info, SelectNone); /* Unselect all nodes */
    _TreeSelectNode(node, SelectAll, FALSE);  /* Select this node */

    /*
     * Get porthole dimensions.
     */

    num_args = 0;
    XtSetArg(args[num_args], XtNwidth, &port_width); num_args++;
    XtSetArg(args[num_args], XtNheight, &port_height); num_args++;
    XtGetValues(XtParent(node->tree_info->tree_widget), args, num_args);

    /*
     * Get node widget dimensions.
     */

    num_args = 0;
    XtSetArg(args[num_args], XtNwidth, &node_width); num_args++;
    XtSetArg(args[num_args], XtNheight, &node_height); num_args++;
    XtSetArg(args[num_args], XtNborderWidth, &node_bw); num_args++;
    XtSetArg(args[num_args], XtNx, &node_x); num_args++;
    XtSetArg(args[num_args], XtNy, &node_y); num_args++;
    XtGetValues(node->widget, args, num_args);

    /*
     * reset the node x and y location to be the new x and y location of
     * the tree relative to the porthole.
     */
 
    node_x = port_width/2 - (node_x + node_width/2 + node_bw);
    node_y = port_height/2 - (node_y + node_height/2 + node_bw);

    num_args = 0;
    XtSetArg(args[num_args], XtNx, node_x); num_args++;
    XtSetArg(args[num_args], XtNy, node_y); num_args++;
    XtSetValues(node->tree_info->tree_widget, args, num_args);    
}

/*	Function Name: PerformTreeToFileDump
 *	Description: Dumps the contents of the current widget tree to
 *                   the file specified. 
 *	Arguments: node - node to dump.
 *                 num_tabs - number of spaces to indent.
 *                 fp - pointer to the file to write to.
 *	Returns: none.
 */

void
PerformTreeToFileDump(node, num_tabs, fp)
WNode * node;
int num_tabs;
FILE * fp;
{
    int i;

    for (i = 0; i < num_tabs; i++) 
	fprintf(fp, "\t");
    fprintf(fp, "%s  %s\n", node->class, node->name);

    num_tabs++;
    for (i = 0; i < node->num_children; i++)
	PerformTreeToFileDump(node->children[i], num_tabs, fp);
}


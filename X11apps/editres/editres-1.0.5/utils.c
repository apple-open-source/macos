/*
 * $Xorg: utils.c,v 1.4 2001/02/09 02:05:30 xorgcvs Exp $
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
/* $XFree86: xc/programs/editres/utils.c,v 1.5 2001/04/01 14:00:17 tsi Exp $ */

#include <X11/Intrinsic.h>
#include <X11/Xutil.h>
#include <X11/Xos.h>
#include <X11/Shell.h>
#include <X11/StringDefs.h>

#include <X11/Xaw/Cardinals.h>
#include <X11/Xaw/Dialog.h>

#include <stdio.h>
#include <stdlib.h>
#include <X11/Xmu/Error.h>

#include "editresP.h"

static WNode * FindWidgetFromWindowGivenNode ( WNode * node, Window win );
static WidgetResources * ParseResources ( GetResourcesInfo * info, 
					  char **error );
static int CompareResourceEntries ( const void *e1, 
				    const void *e2 );
static void AddResource ( ResourceInfo * res_info, 
			  WidgetResourceInfo * resource );
static void FreeResources ( WidgetResources * resources );


/*	Function Name: SetMessage(w, str)
 *	Description: shows the message to the user.
 *	Arguments: w - a label widget to show the message in.
 *                 str - the string to show.
 *	Returns: none.
 */

void
SetMessage(Widget w, char *str)
{
    Arg args[1];

    XtSetArg(args[0], XtNlabel, str);
    XtSetValues(w, args, ONE);
}

/*	Function Name: GetAllStrings
 *	Description: Returns a list of strings that have been borken up by
 *                   the character specified.
 *	Arguments: in - the string to parse.
 *                 sep - the separator character.
 *                 out - the strings to send out.
 *                 num - the number of strings in out.
 *	Returns: none
 */

void
GetAllStrings(char *in, char sep, char ***out, int *num)
{
    int size, i;
    char * ptr;

    if (*in == sep)		/* jump over first char if it is the sep. */
	in++;

    /*
     * count the number of strings.
     */

    for (*num = 1, ptr = in; (ptr = strchr(ptr, sep)) != NULL; (*num)++)
	ptr++;

/*
 * Create Enough space for pointers and string.
 */

    size = (sizeof(char *) * *num) + (sizeof(char) * (strlen(in) + 1));
    *out = (char **) XtMalloc( (Cardinal) size);

    ptr = (char *) (*out + *num);
    strcpy(ptr, in);

/*
 * Change all `sep' characters to '\0' and stuff the pointer into
 * the next pointer slot.
 */

    i = 1;
    (*out)[0] = ptr;
    while (TRUE) {
	if ((ptr = strchr(ptr, sep)) == NULL)
	    break;

	*ptr++ = '\0';
	(*out)[i++] = ptr;
    }

/*
 * If last string is empty then strip it off.
 */

    if ( *((*out)[i - 1]) == '\0' )
	(*num)--;
}

/*	Function Name: AddString
 *	Description: Mallocs and strcats the string onto the end of
 *                   the given string.
 *	Arguments: str - string to add on to.
 *                 add - string to add.
 *	Returns: none.
 */

void
AddString(char ** str, char *add)
{
    int len_str, len_add;
    char * ptr;

    len_str = ((*str) ? strlen(*str) : 0);
    len_add = strlen(add);

    *str = XtRealloc(*str, sizeof(char) * (len_str + len_add + 1));
    ptr = *str + len_str;
    strcpy(ptr, add);
}
 
/*	Function Name: FindNode
 *	Description: Finds a node give the top node, and a node id number.
 *	Arguments: top_node - the top node.
 *                 id - the node id.
 *	Returns: node.
 */

WNode *
FindNode(WNode *top_node, unsigned long *ids, Cardinal number)
{
    int i, j;
    WNode *node;

    if (top_node == NULL)
	return(NULL);

    if (ids[0] != top_node->id)
	return(NULL);

    for (node = top_node, i = 1 ; i < number; i++) {
	Boolean found_it = FALSE;

	for (j = 0; j < node->num_children; j++) {
	    if (node->children[j]->id == ids[i]) {
		node = node->children[j];
		found_it = TRUE;
		break;
	    }
	}
	if (!found_it)
	    return(NULL);
    }	    
    return(node);
}

/*	Function Name: FindWidgetFromWindow
 *	Description: finds a widget in the current tree given its window id.
 *	Arguments: tree_info - information about this tree.
 *                 win - window to search for.
 *	Returns: node - the node corrosponding to this widget.
 */

WNode * 
FindWidgetFromWindow(TreeInfo *tree_info, Window win)
{
    if (tree_info == NULL)
	return(NULL);

    return(FindWidgetFromWindowGivenNode(tree_info->top_node, win));
}

/*	Function Name: FindWidgetFromWindowGivenNode
 *	Description: finds a widget in the current tree given its window id.
 *	Arguments: node - current node.
 *                 win - window to search for.
 *	Returns: node - the node corrosponding to this widget.
 */

static WNode *
FindWidgetFromWindowGivenNode(WNode *node, Window win)
{
    int i;
    WNode * ret_node;

    if (node->window == win)
	return(node);

    for (i = 0; i < node->num_children; i++) {
	ret_node = FindWidgetFromWindowGivenNode(node->children[i], win);
	if (ret_node != NULL)
	    return(ret_node);
    }
    return(NULL);
}

/*	Function Name: HandleXErrors
 *	Description: Handles error codes from the server.
 *	Arguments: display - the display.
 *                 error - error information.
 *	Returns: none.
 */

/* ARGSUSED */
int
HandleXErrors(Display *display, XErrorEvent *error)
{
    if (error->serial != global_serial_num) {
	(*global_old_error_handler) (display, error);
	return(0);
    }

    if (error->error_code == BadWindow)
	global_error_code = NO_WINDOW;    
    else {
	if (XmuPrintDefaultErrorMessage(display, error, stderr) != 0)
	    exit(1);
    }
    return(0);
}

/*	Function Name: _DumpTreeToFile
 *	Description: Dumps the widget tree to a file
 *	Arguments: w - a random widget in the application on the
 *                     currently active display
 *                 tree_ptr - pointer to the widget tree info.
 *                 filename - name of the file.
 *	Returns: none.
 */

/* ARGSUSED */

void
_DumpTreeToFile(Widget w, XtPointer tree_ptr, XtPointer filename)
{
    TreeInfo * tree_info = (TreeInfo *) tree_ptr;
    FILE * fp; 

    if (tree_info == NULL) {
	SetMessage(global_screen_data.info_label,
		   res_labels[17]);
	return;
    }

    if ( (fp = fopen((char *)filename, "w")) == NULL ) {
	char buf[BUFSIZ];

	sprintf(buf, res_labels[24], (char *)filename);
	SetMessage(global_screen_data.info_label, buf);
	return;
    }

    PerformTreeToFileDump(tree_info->top_node, 0, fp);
    fclose(fp);
}

/************************************************************
 * 
 * The file dialog boxes are handled with this code.
 *
 * It automatically calls the function specified when the
 * user selects okay, or hits <CR>.
 *
 * A translation is required in the app-defaults file.
 *
 ************************************************************/

/*	Function Name: _PopupFileDialog
 *	Description: Puts up a dialog box to get the filename.
 *	Arguments: str - message.
 *                 default_value - the default value of the filename;
 *                 func - function to call when filename has been entered.
 *                 data - generic data to pass to func.
 *	Returns: none
 */

static XContext file_dialog_context = None;

typedef struct _FileDialogInfo {
    XtCallbackProc func;
    XtPointer data;
} FileDialogInfo;

void
_PopupFileDialog(Widget w, String str, String default_value,
		 XtCallbackProc func, XtPointer data)
{
    FileDialogInfo * file_info;
    Widget shell, dialog;
    Arg args[2];
    Cardinal num_args;

    if (file_dialog_context == None)
	file_dialog_context = XUniqueContext();

    shell = XtCreatePopupShell("fileDialog", transientShellWidgetClass, w,
			       NULL, ZERO);

    num_args = 0;
    XtSetArg(args[num_args], XtNlabel, str); num_args++;
    XtSetArg(args[num_args], XtNvalue, default_value); num_args++;
    dialog = XtCreateManagedWidget("dialog", dialogWidgetClass, 
				   shell, args, num_args);

    file_info = XtNew(FileDialogInfo);

    file_info->func = func;
    file_info->data = data;

    if  (XSaveContext(XtDisplay(dialog), (Window) dialog, file_dialog_context, 
		      (XPointer) file_info) != 0) {
	SetMessage(global_screen_data.info_label,
	    "Error while trying to save Context\nAborting file dialog popup.");
	XtDestroyWidget(shell);
	return;
    }

    XawDialogAddButton(dialog, "okay", _PopdownFileDialog, (XtPointer) TRUE);
    XawDialogAddButton(dialog, "cancel", _PopdownFileDialog,(XtPointer) FALSE);

    PopupCentered(NULL, shell, XtGrabNone);
}

/*	Function Name: PopupCentered
 *	Description: Pops up the window specified under the location passed
 *                   in the event, or under the cursor.
 *	Arguments: event - the event that we should use.
 *                 w - widget to popup.
 *                 mode - mode to pop it up in.
 *	Returns: none
 */

void
PopupCentered(XEvent *event, Widget w, XtGrabKind mode)
{
    Boolean get_from_cursor = FALSE;
    Arg args[3];
    Cardinal num_args;
    Dimension width, height, b_width;
    int x, y, max_x, max_y;

    XtRealizeWidget(w);

    if (event == NULL)
	get_from_cursor = TRUE;
    else {
	switch (event->type) {
	case ButtonPress:
	case ButtonRelease:
	    x = event->xbutton.x_root;
	    y = event->xbutton.y_root;
	    break;
	case KeyPress:
	case KeyRelease:
	    x = event->xkey.x_root;
	    y = event->xkey.y_root;
	    break;
	default:
	    get_from_cursor = TRUE;
	    break;
	}
    }

    if (get_from_cursor) {
	Window root, child;
	int win_x, win_y;
	unsigned int mask;
	
	XQueryPointer(XtDisplay(w), XtWindow(w),
		      &root, &child, &x, &y, &win_x, &win_y, &mask);
    }

    num_args = 0;
    XtSetArg(args[num_args], XtNwidth, &width); num_args++;
    XtSetArg(args[num_args], XtNheight, &height); num_args++;
    XtSetArg(args[num_args], XtNborderWidth, &b_width); num_args++;
    XtGetValues(w, args, num_args);

    width += 2 * b_width;
    height += 2 * b_width;

    x -= ((int) width/2);
    if (x < 0) 
	x = 0;
    if ( x > (max_x = (int) (XtScreen(w)->width - width)) )
	x = max_x;

    y -= ( (Position) height/2 );
    if (y < 0) 
	y = 0;
    if ( y > (max_y = (int) (XtScreen(w)->height - height)) )
	y = max_y;
  
    num_args = 0;
    XtSetArg(args[num_args], XtNx, x); num_args++;
    XtSetArg(args[num_args], XtNy, y); num_args++;
    XtSetValues(w, args, num_args);

    XtPopup(w, mode);
}

/*	Function Name: _PopdownFileDialog
 *	Description: Destroys the file dialog, and calls the correct function.
 *	Arguments:  w - a child of the dialog widget.
 *                  client_data - TRUE if command was sucessful.
 *                  junk - ** UNUSED **.
 *	Returns: none.
 */

/* ARGSUSED */

void 
_PopdownFileDialog(Widget w, XtPointer client_data, XtPointer junk)
{
    Widget dialog = XtParent(w);
    XPointer file_info_ptr;
    FileDialogInfo * file_info;

    if (XFindContext(XtDisplay(dialog), (Window) dialog, file_dialog_context,
		     &file_info_ptr) == XCNOENT) {
	SetMessage(global_screen_data.info_label,	
		   "Error while trying to find Context\nAborting...");	
    }

    (void) XDeleteContext(XtDisplay(dialog), (Window)dialog, 
			  file_dialog_context);

    file_info = (FileDialogInfo *) file_info_ptr;

    if ( ((Boolean)(long) client_data) == TRUE ) {
	String filename = XawDialogGetValueString(dialog);

	(*file_info->func)(w, file_info->data, filename); /* call handler */
    }

    XtFree( (XtPointer) file_info); /* Free data. */

    XtPopdown(XtParent(dialog));
    XtDestroyWidget(XtParent(dialog)); /* Remove file dialog. */
}

/************************************************************
 *
 * Functions for dealing with the Resource Box.
 *
 ************************************************************/

/*    Function Name: GetNamesAndClasses
 *    Description: Gets a list of names and classes for this widget.
 *    Arguments: node - this widget's node.
 *                 names, classes - list of names and classes. ** RETURNED **
 *    Returns: none.
 */

void
GetNamesAndClasses(WNode *node, char ***names, char ***classes)
{
    int i, total_widgets;
    WNode * temp = node;

    for (total_widgets = 1 ; temp->parent != NULL ;
       total_widgets++, temp = temp->parent) {}

    *names = (char **) XtMalloc(sizeof(char *) * (total_widgets + 1));
    *classes = (char **) XtMalloc(sizeof(char *) * (total_widgets + 1));

    (*names)[total_widgets] = (*classes)[total_widgets] = NULL;

    for ( i = (total_widgets - 1); i >= 0 ; node = node->parent, i--) {
      (*names)[i] = node->name;
      (*classes)[i] = node->class;
    }
}

/*	Function Name: HandleGetResources
 *	Description: Gets the resources.
 *	Arguments: event - the information from the client.
 *	Returns: an error message to display.
 */

char *
HandleGetResources(Event *event)
{
    GetResourcesEvent * get_event = (GetResourcesEvent *) event;
    char buf[BUFSIZ], * errors = NULL;
    int i;
    WNode * node;

    for (i = 0; i < (int)get_event->num_entries; i++) {
	node = FindNode(global_tree_info->top_node,
			get_event->info[i].widgets.ids, 
			get_event->info[i].widgets.num_widgets);

	if (node == NULL) {
	    sprintf(buf, res_labels[16]);
	    AddString(&errors, buf); 
	    continue;	
	}

	if (node->resources != NULL) 
	    FreeResources(node->resources);

	if (!get_event->info[i].error) {
	    node->resources = ParseResources(get_event->info + i, &errors);
	    CreateResourceBox(node, &errors);
	}
	else {
	    AddString(&errors, get_event->info[i].message);
	    AddString(&errors, "\n");
	}
    }

    return(errors);
}

/*	Function Name: CreateResourceBox
 *	Description: Creates a resource box for the widget specified.
 *	Arguments: node - the node of the widget in question.
 *                 errors - an error string.
 *	Returns: none.
 */

void
CreateResourceBox(WNode *node, char **errors)
{
    WidgetResources * resources = node->resources;
    char ** names, ** cons_names;
    int i;

    if (global_resource_box_up) {
	AddString(errors, res_labels[34]);
	return;
    }
    else
	global_resource_box_up = TRUE;

    if (resources->num_normal > 0) {
	names = (char **) XtMalloc(sizeof(char *) *
				   (resources->num_normal + 1));
	for (i = 0 ; i < resources->num_normal ; i++) 
	    names[i] = resources->normal[i].name;
	names[i] = NULL;
    }
    else
	names = NULL;

    if (resources->num_constraint > 0) {
	cons_names = (char **) XtMalloc(sizeof(char *) *
					(resources->num_constraint + 1));
	
	for (i = 0 ; i < resources->num_constraint ; i++) 
	    cons_names[i] = resources->constraint[i].name;
	cons_names[i] = NULL;
    }
    else
	cons_names = NULL;

    CreateResourceBoxWidgets(node, names, cons_names);
}

/*	Function Name: ParseResources
 *	Description: Parses the resource values returned from the client
 *                   into a resources structure.
 *	Arguments: info - info about a widget's resources.
 *                 error - where to place error info.
 *	Returns: The resource information.
 */

static WidgetResources * 
ParseResources(GetResourcesInfo *info, char **error)
{
    WidgetResources * resources;
    WidgetResourceInfo * normal;
    int i;

    resources = (WidgetResources *) XtMalloc(sizeof(WidgetResources)); 
    
    /*
     * Allocate enough space for both the normal and constraint resources,
     * then add the normal resources from the top, and the constraint resources
     * from the bottom.  This assures that enough memory is allocated, and
     * that there is no overlap.
     */

    resources->normal = (WidgetResourceInfo *) 
	            XtMalloc(sizeof(WidgetResourceInfo) * info->num_resources);

    normal = resources->normal;
    resources->constraint = resources->normal + info->num_resources - 1;

    resources->num_constraint = resources->num_normal = 0;

    for (i = 0; i < (int)info->num_resources; i++) {
	switch((int) info->res_info[i].res_type) {
	case NormalResource:
	    resources->num_normal++;
	    AddResource(info->res_info + i, normal++);	    
	    break;
	case ConstraintResource:
	    resources->num_constraint++;
	    AddResource(info->res_info + i, resources->constraint--);
	    break;
	default:
	    {
		char buf[BUFSIZ];
		sprintf(buf, "Unknown resource type %d\n", 
			info->res_info[i].res_type);
		AddString(error, buf);
	    }
	    break;
	}
    }

    /*
     * Sort the resources alphabetically. 
     */

    qsort(resources->normal, resources->num_normal,
	  sizeof(WidgetResourceInfo), CompareResourceEntries);

    if (resources->num_constraint > 0) {
	resources->constraint++;
	qsort(resources->constraint, resources->num_constraint,
	      sizeof(WidgetResourceInfo), CompareResourceEntries);
    }
    else
	resources->constraint = NULL;

    return(resources);
}

/*	Function Name: CompareResourceEntries
 *	Description: Compares two resource entries.
 *	Arguments: e1, e2 - the entries to compare.
 *	Returns: an integer >, < or = 0.
 */

static int 
CompareResourceEntries(const void *e1, const void *e2)
{
    return (strcmp(((WidgetResourceInfo *)e1)->name, 
		   ((WidgetResourceInfo *)e2)->name));
}

/*	Function Name: AddResource
 *	Description: Parses the resource string a stuffs in individual
 *                   parts into the resource info struct.
 *	Arguments: res_info - the resource info from the event.
 *                 resource - location to stuff the resource into.
 *	Returns: none.
 */

static void
AddResource(ResourceInfo *res_info, WidgetResourceInfo *resource)
{
    resource->name = res_info->name;
    res_info->name = NULL;	/* Keeps it from being deallocated. */
    resource->class = res_info->class;
    res_info->class = NULL;	/* Keeps it from being deallocated. */
    resource->type = res_info->type;
    res_info->type = NULL;	/* Keeps it from being deallocated. */
}


/*	Function Name: FreeResources
 *	Description: frees the resource inforation.
 *	Arguments: resources.
 *	Returns: none.
 */

static void
FreeResources(WidgetResources *resources)
{
    int i;

    if (resources->num_normal > 0) {
	for (i = 0; i < resources->num_normal; i++) {
	    XtFree(resources->normal[i].name);
	    XtFree(resources->normal[i].class);
	    XtFree(resources->normal[i].type);
	}
	XFree((char *)resources->normal);
    }

    if (resources->num_constraint > 0) {
	for (i = 0; i < resources->num_constraint; i++) {
	    XtFree(resources->constraint[i].name);
	    XtFree(resources->constraint[i].class);
	    XtFree(resources->constraint[i].type);
	}
	XFree((char *)resources->constraint);
    }

    XFree((char *)resources);
}
	

/*	Function Name: CheckDatabase
 *	Description: Checks to see if the node is in the database.
 *	Arguments: db - the db to check
 *                 names, clases - names and clases, represented as quarks.
 *	Returns: True if this entry is found.
 */

Boolean
CheckDatabase(XrmDatabase db, XrmQuarkList names, XrmQuarkList classes)
{
    XrmRepresentation junk;
    XrmValue garbage;

    return(XrmQGetResource(db, names, classes, &junk, &garbage));
}

/*	Function Name: Quarkify
 *	Description: Quarkifies the string list specifed.
 *	Arguments: list - list of strings to quarkify
 *                 ptr - an additional string to quarkify.
 *	Returns: none.
 */

XrmQuarkList
Quarkify(char **list, char *ptr)
{
    int i;
    char ** tlist;
    XrmQuarkList quarks, tquarks;

    for (i = 0, tlist = list; *tlist != NULL; tlist++, i++) {}
    if (ptr != NULL)
	i++;
    i++;			/* leave space for NULLQUARK */

    quarks = (XrmQuarkList) XtMalloc(sizeof(XrmQuark) * i);

    for (tlist = list, tquarks = quarks; *tlist != NULL; tlist++, tquarks++) 
	*tquarks = XrmStringToQuark(*tlist);

    if (ptr != NULL) 
	*tquarks++ = XrmStringToQuark(ptr);
	
    *tquarks = NULLQUARK;
    return(quarks);
}

/*	Function Name: ExecuteOverAllNodes
 *	Description: Executes the given function over all nodes.
 *	Arguments: top_node - top node of the tree.
 *                 func - the function to execute.
 *                 data - a data pointer to pass to the function.
 *	Returns: none
 */

void
ExecuteOverAllNodes(WNode *top_node, void (*func)(WNode *, XtPointer),
		    XtPointer data)
{
    int i;

    (*func)(top_node, data);

    for (i = 0; i < top_node->num_children; i++) 
	ExecuteOverAllNodes(top_node->children[i], func, data);
}

/*	Function Name: InsertWidgetFromNode
 *	Description: Inserts the widget info for this widget represented
 *                   by this node.
 *	Arguments: stream - the stream to insert it info into.
 *                 none - the widget node to insert.
 *	Returns: none
 */

void
InsertWidgetFromNode(ProtocolStream *stream, WNode *node)
{
    WNode *temp;
    unsigned long * widget_list;
    register int i, num_widgets;

    for (temp = node, i = 0; temp != NULL; temp = temp->parent, i++) {}

    num_widgets = i;
    widget_list = (unsigned long *) 
	          XtMalloc(sizeof(unsigned long) * num_widgets);

    /*
     * Put the widgets into the list.
     * Make sure that they are inserted in the list from parent -> child.
     */

    for (i--, temp = node; temp != NULL; temp = temp->parent, i--) 
	widget_list[i] = temp->id;
	
    _XEditResPut16(stream, num_widgets);	/* insert number of widgets. */
    for (i = 0; i < num_widgets; i++) 	/* insert Widgets themselves. */
	_XEditResPut32(stream, widget_list[i]);
    
    XtFree((char *)widget_list);
}

/*	Function Name: GetFailureMesssage
 *	Description: returns the message returned from a failed request.
 *	Arguments: stream - the protocol stream containing the message.
 *	Returns: message to show.
 */

char * 
GetFailureMessage(ProtocolStream *stream)
{
    char * return_str;

    if (_XEditResGetString8(stream, &return_str)) 
	return(return_str);

    return(XtNewString(res_labels[35]));
}

/*	Function Name: ProtocolFailure
 *	Description: Gets the version of the protocol the client is
 *                   willing to speak.
 *	Arguments: stream - the protocol stream containing the message.
 *	Returns: message to show.
 */

char * 
ProtocolFailure(ProtocolStream *stream)
{
    char buf[BUFSIZ];
    unsigned char version;
    char* old_version_string;

    if (!_XEditResGet8(stream, &version)) 
	return(XtNewString(res_labels[35]));

    switch ((int)version) {
    case PROTOCOL_VERSION_ONE_POINT_ZERO: old_version_string = "1.0"; break;
    default: old_version_string = "1.0";
    }
    
    sprintf(buf, res_labels[36], 
	    CURRENT_PROTOCOL_VERSION_STRING, old_version_string);
    return(XtNewString(buf));
}
	

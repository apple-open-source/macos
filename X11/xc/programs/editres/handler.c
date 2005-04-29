/*
 * $Xorg: handler.c,v 1.4 2001/02/09 02:05:29 xorgcvs Exp $
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

/* $XFree86: xc/programs/editres/handler.c,v 1.8 2003/10/24 20:38:10 tsi Exp $ */

#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>

#include <X11/Xaw/Cardinals.h>
#include <X11/Xaw/List.h>
#include <X11/Xaw/Panner.h>
#include <X11/Xaw/Toggle.h>
#include <X11/Xfuncs.h>
#include <X11/Xos.h>		/* for W_OK def */

#include <stdio.h>
#include <stdlib.h>

#include "editresP.h"

/* 
 * Local function definitions.
 */
static char * GetResourceName ( ResourceBoxInfo * res_box );
static void _AppendResourceString ( Widget w, XtPointer res_box_ptr, 
				    XtPointer filename_ptr );
static void _SetResourcesFile ( Widget w, XtPointer junk, 
				XtPointer filename_ptr );
static void ObtainResource ( XtPointer node_ptr );
static void CreateSetValuesCommand ( WNode * node, XtPointer info_ptr );
static void SetOnlyMatchingWidgets ( WNode * node, XtPointer info_ptr );

/*	Function Name: Quit
 *	Description: This function prints a message to stdout.
 *	Arguments: w - ** UNUSED **
 *                 call_data - ** UNUSED **
 *                 client_data - ** UNUSED **
 *	Returns: none
 */

/* ARGSUSED */
void
Quit(w, client_data, call_data)
Widget w;
XtPointer call_data, client_data;
{
    XtDestroyApplicationContext(XtWidgetToApplicationContext(w));
    exit(0);
}

/*	Function Name: SendTree
 *	Description: This function initiates the client communication.
 *                   by getting the resource tree.
 *	Arguments: w - the widget that made the selection.
 *                 value - a boolean value stored as a pointer.
 *                         if True then get a new client, otherwise
 *                         refresh the current client.
 *                 call_data - ** UNUSED **
 *	Returns: none
 */

/* ARGSUSED */
void
SendTree(w, value, call_data)
Widget w;
XtPointer value, call_data;
{
    if ((Boolean)(long) value)
	global_client.window = None;

    if (!XtIsWidget(w))     /* Make sure that we use a "Real" widget here. */
	w = XtParent(w);

    _XEditResResetStream(&(global_client.stream)); /* an empty message. */
    
    SetCommand(w, LocalSendWidgetTree, NULL); 
}

/*	Function Name: FindWidget
 *	Description: Maps a widget in the client to one in the currently
 *                   displayed widget tree.
 *	Arguments: w - the widget that invoked this action.
 *                 call_data, client_data ** UNUSED **
 *	Returns: none
 */

/* ARGSUSED */
void
FindWidget(w, client_data, call_data)
Widget w;
XtPointer client_data, call_data;
{

    _FindWidget(XtParent(w));	/* Use parent since it is a "real"
				   widget not a rect_obj. */
}

/*	Function Name: InitSetValues
 *	Description: This function pops up the setvalues dialog
 *	Arguments: w - the widget caused this action.
 *                 call_data - ** UNUSED **
 *                 client_data - ** UNUSED **
 *	Returns: none
 */

/* ARGSUSED */
void
InitSetValues(w, client_data, call_data)
Widget w;
XtPointer call_data, client_data;
{
    if (!XtIsWidget(w))     /* Make sure that we use a "Real" widget here. */
	w = XtParent(w);

    PopupSetValues(w, NULL);
}

/*	Function Name: TreeSelect
 *	Description: Selects all widgets.
 *	Arguments: w - the widget caused this action.
 *                 call_data - ** UNUSED **
 *                 client_data - The type of thing to select.
 *	Returns: none
 */

/* ARGSUSED */
void
TreeSelect(w, client_data, call_data)
Widget w;
XtPointer call_data, client_data;
{
    SelectTypes type = (SelectTypes) (unsigned long) client_data;

    _TreeSelect(global_tree_info, type);
}

/*	Function Name: TreeRelabel
 *	Description: Relabels a tree to the type specified.
 *	Arguments: w - the widget caused this action.
 *                 call_data - ** UNUSED **
 *                 client_data - the type of label to assign to each node.
 *	Returns: none
 */

/* ARGSUSED */
void
TreeRelabel(w, client_data, call_data)
Widget w;
XtPointer call_data, client_data;
{
    LabelTypes type = (LabelTypes) (unsigned long) client_data;

    _TreeRelabel(global_tree_info, type);
}

/*	Function Name: PannerCallback
 *	Description: called when the panner has moved.
 *	Arguments: panner - the panner widget.
 *                 closure - *** NOT USED ***.
 *                 report_ptr - the panner record.
 *	Returns: none.
 */

/* ARGSUSED */
void 
PannerCallback(w, closure, report_ptr)
Widget w;
XtPointer closure, report_ptr;
{
    Arg args[2];
    XawPannerReport *report = (XawPannerReport *) report_ptr;

    if (global_tree_info == NULL) 
	return;

    XtSetArg (args[0], XtNx, -report->slider_x);
    XtSetArg (args[1], XtNy, -report->slider_y);

    XtSetValues(global_tree_info->tree_widget, args, TWO);
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
void 
PortholeCallback(w, panner_ptr, report_ptr)
Widget w;
XtPointer panner_ptr, report_ptr;
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

/*	Function Name: FlashActiveWidgets
 *	Description: called to flass all active widgets in the display.
 *	Arguments: *** NOT USED ***
 *	Returns: none.
 */

/* ARGSUSED */
void 
FlashActiveWidgets(w, junk, garbage)
Widget w;
XtPointer junk, garbage;
{
    _FlashActiveWidgets(global_tree_info);
}

/*	Function Name: GetResourceList
 *	Description: Gets the resources lists of all active widgets.
 *	Arguments: ** NOT USED **
 *	Returns: none
 */

/* ARGSUSED */
void
GetResourceList(w, junk, garbage)
Widget w;
XtPointer junk, garbage;
{
    WNode * node;
    ProtocolStream * stream = &(global_client.stream);

    if (global_tree_info == NULL) {
	SetMessage(global_screen_data.info_label,
		   res_labels[17]);
	return;
    }

    if (global_tree_info->num_nodes != 1) {
	SetMessage(global_screen_data.info_label,
	      res_labels[19]);
	return;
    }

    node = global_tree_info->active_nodes[0];
    if (node->resources != NULL) {
	char * errors = NULL;
	CreateResourceBox(node, &errors);
	if (errors != NULL) {
	    SetMessage(global_screen_data.info_label, errors);
	    XtFree(errors);
	}
	return;
    }

    /*
     * No resoruces, fetch them from the client.
     */

    _XEditResResetStream(stream); 
    _XEditResPut16(stream, (unsigned short) 1);
    InsertWidgetFromNode(stream, node);
    SetCommand(global_tree_info->tree_widget, LocalGetResources, NULL);
}

/*	Function Name: DumpTreeToFile
 *	Description: Dumps all widgets in the tree to a file.
 *	Arguments: w - the widget that activated this callback.
 *                 junk, garbage - ** NOT USED **.
 *	Returns: none.
 */

/* ARGSUSED */
void 
DumpTreeToFile(w, junk, garbage)
Widget w;
XtPointer junk, garbage;
{
    _PopupFileDialog(XtParent(w), "Enter the filename:", "",
		     _DumpTreeToFile, (XtPointer) global_tree_info);
}

/************************************************************
 * 
 * Callbacks for the Resource Box.
 *
 ************************************************************/


/*	Function Name: AnyChosen
 *	Description: Callback that is called when the "any" widget 
 *                   is activated.
 *	Arguments: w - the "any" widget that activated this callback.
 *                 any_info_ptr - pointer to struct containing 
 *                                dot and star widgets to lock.
 *                 state_ptr - state of the any toggle.
 *	Returns: none.
 */

/* ARGSUSED */
void 
AnyChosen(w, any_info_ptr, state_ptr)
Widget w;
XtPointer any_info_ptr, state_ptr;
{
    AnyInfo * any_info = (AnyInfo *) any_info_ptr;
    Boolean state = (Boolean)(long) state_ptr;
    Arg args[1];

    if (state) {

	if (any_info->left_count == 0) {
	    XtSetSensitive(any_info->left_dot, FALSE);
	    XtSetSensitive(any_info->left_star, FALSE);

	    XtSetArg(args[0], XtNstate, TRUE);
	    XtSetValues(any_info->left_star, args, ONE);
	}

	if ((any_info->right_count == NULL)||(*any_info->right_count == 0)) {
	    XtSetSensitive(any_info->right_dot, FALSE);
	    XtSetSensitive(any_info->right_star, FALSE);

	    XtSetArg(args[0], XtNstate, TRUE);
	    XtSetValues(any_info->right_star, args, ONE);
	}
	any_info->left_count++;

	if (any_info->right_count != NULL)
	    (*any_info->right_count)++;
    }
    else {			/* state == 0 */
	if (any_info->left_count > 0) 
	    any_info->left_count--;
	if ((any_info->right_count != NULL)&&(*any_info->right_count > 0)) 
	    (*any_info->right_count)--;

	if (any_info->left_count == 0) {
	    XtSetSensitive(any_info->left_dot, TRUE);
	    XtSetSensitive(any_info->left_star, TRUE);

	    XtSetArg(args[0], XtNstate, TRUE);
	    XtSetValues(any_info->left_dot, args, ONE);
	}

	if ((any_info->right_count == NULL)||(*any_info->right_count == 0)) {
	    XtSetSensitive(any_info->right_dot, TRUE);
	    XtSetSensitive(any_info->right_star, TRUE);

	    XtSetArg(args[0], XtNstate, TRUE);
	    XtSetValues(any_info->right_dot, args, ONE);
	}
    }
    SetResourceString(NULL, (XtPointer) any_info->node, NULL);
    ActivateResourceWidgets(NULL, (XtPointer) any_info->node, NULL);
}

/*	Function Name: GetResourceName
 *	Description: Gets the name of the current resource.
 *	Arguments: res_box - the resource box.
 *	Returns: the name of the currently selected resource.
 */


static char *
GetResourceName(res_box)
ResourceBoxInfo * res_box;
{
    XawListReturnStruct * list_info;
    char * result;
    
    list_info = XawListShowCurrent(res_box->norm_list);
    if ((list_info->list_index == XAW_LIST_NONE) && 
	(res_box->cons_list != NULL)) {
	list_info = XawListShowCurrent(res_box->cons_list);
    }

    if (list_info->list_index == XAW_LIST_NONE) 
	result = "unknown";
    else
	result = list_info->string;    

    return(result);
}


/*	Function Name: ActivateWidgetsAndSetResourceString
 *	Description: Sets the new resources string, then
 *                   activates all widgets that match this resource,
 *	Arguments: w - the widget that activated this.
 *                 node_ptr - the node that owns this resource box.
 *                 call_data - passed on to other callbacks.
 *	Returns: none.
 *
 * NOTE: I cannot just have two callback routines, since I care which
 *       order that these are executed in, sigh...
 */

void
ActivateWidgetsAndSetResourceString(w, node_ptr, call_data)
Widget w;
XtPointer node_ptr, call_data;
{
    SetResourceString(w, node_ptr, call_data);
    ActivateResourceWidgets(w, node_ptr, call_data);
}

/*	Function Name: SetResourceString
 *	Description: Sets the resource label to correspond to the currently
 *                   chosen string.
 *	Arguments: w - The widget that invoked this callback, or NULL.
 *                 node_ptr - pointer to widget node contating this res box.
 *                 call_data - The call data for the action that invoked
 *                             this callback.
 *	Returns: none.
 */

void
SetResourceString(w, node_ptr, junk)
Widget w;
XtPointer node_ptr, junk;
{
    static char * malloc_string; /* These are both inited to zero. */
    static Cardinal malloc_size;

    WNode * node = (WNode *) node_ptr;
    ResourceBoxInfo * res_box = node->resources->res_box;
    char * temp, buf[BUFSIZ * 10];	/* here's hoping it's big enough. */
    NameInfo * name_node = res_box->name_info;
    Arg args[1];
    int len;

    if ((w != NULL) && XtIsSubclass(w, toggleWidgetClass)) {
	/*
	 * Only set resources when toggles are activated, not when they are
	 * deactivated. 
	 */
	if (!((Boolean)(long) junk))
	    return;
    }

    buf[0] = '\0';		/* clear out string. */

    /*
     * Get the widget name/class info.
     */

    if ((temp = (char *) XawToggleGetCurrent(name_node->sep_leader)) != NULL)
	strcat(buf, temp);

    for ( ; name_node->next != NULL ; name_node = name_node->next) {
	temp = (char *) XawToggleGetCurrent(name_node->name_leader);
	if ( (temp != NULL) && !streq(temp, ANY_RADIO_DATA) ) {
	    strcat(buf, temp);
	    temp = (char *) XawToggleGetCurrent(name_node->next->sep_leader);
	    if (temp == NULL) 
		strcat(buf, "!");
	    else
		strcat(buf, temp);
	}
    }
		
    strcat(buf, GetResourceName(res_box));
    len = strlen(buf) + 2; /* Leave space for ':' and '\0' */

#ifdef notdef
    XtSetArg(args[0], XtNstring, &temp);
    XtGetValues(res_box->value_wid, args, ONE);
    len += strlen(temp);
#endif

    if (len > malloc_size) {
	malloc_string = XtRealloc(malloc_string, sizeof(char) * len);
	malloc_size = len;
    }
    
    strcpy(malloc_string, buf);
    strcat(malloc_string, ":");
#ifdef notdef
    strcat(malloc_string, temp);
#endif

    XtSetArg(args[0], XtNlabel, malloc_string);
    XtSetValues(res_box->res_label, args, ONE);
}
    
/*	Function Name: ResourceListCallback
 *	Description: Callback functions for the resource lists.  This 
 *                   routine is essentialy called by the list widgets
 *                   Notify action.  If action EnableGetVal has been
 *                   invoked,  ResourceListCallback will perform a
 *                   GetValues protocol request.
 *	Arguments: list - the list widget that we are dealing with.
 *                 node_ptr - pointer to widget node contating this res box.
 *                 junk - UNUSED.
 *	Returns: none
 */

extern Boolean do_get_values;

void
ResourceListCallback(list, node_ptr, junk)
Widget list;
XtPointer node_ptr, junk;
{
    Widget o_list;
    WNode * node = (WNode *) node_ptr;
    ResourceBoxInfo * res_box = node->resources->res_box;

    if (list == res_box->norm_list) 
	o_list = res_box->cons_list;
    else
	o_list = res_box->norm_list;

    if (o_list != NULL)
	XawListUnhighlight(o_list);

    SetResourceString(list, node_ptr, junk);

    /* get the resource value from the application */
    if (global_effective_protocol_version >=
	PROTOCOL_VERSION_ONE_POINT_ONE && do_get_values) {
      ObtainResource(node_ptr);
      do_get_values = False;
    }
}

/*	Function Name: PopdownResBox
 *	Description: Pops down the resource box.
 *	Arguments: w - UNUSED
 *                 shell_ptr - pointer to the shell to pop down.
 *                 junk - UNUSED.
 *	Returns: none
 */

/* ARGSUSED */
void
PopdownResBox(w, shell_ptr, junk)
Widget w;
XtPointer shell_ptr, junk;
{
    Widget shell = (Widget) shell_ptr;

    XtPopdown(shell);
    XtDestroyWidget(shell);
}

/* ARGSUSED */
static void
_AppendResourceString(w, res_box_ptr, filename_ptr)
Widget w;
XtPointer res_box_ptr, filename_ptr;
{
    Arg args[1];
    FILE * fp;
    char buf[BUFSIZ], * resource_string, *filename = (char *) filename_ptr;
    ResourceBoxInfo * res_box = (ResourceBoxInfo *) res_box_ptr;
    char *value_ptr;

    if (filename != NULL) {
	if (global_resources.allocated_save_resources_file) 
	    XtFree(global_resources.save_resources_file);
	else
	    global_resources.allocated_save_resources_file = TRUE;
	
	global_resources.save_resources_file = XtNewString(filename);
    }

    if ((fp = fopen(global_resources.save_resources_file, "a+")) == NULL) {
	sprintf(buf, "Unable to open this file for writing, would %s",
		"you like To try again?");
	_PopupFileDialog(global_toplevel ,buf,
			global_resources.save_resources_file,
			_AppendResourceString, res_box_ptr);
	return;
    }

    XtSetArg(args[0], XtNlabel, &resource_string);
    XtGetValues(res_box->res_label, args, ONE);

    XtSetArg(args[0], XtNstring, &value_ptr);
    XtGetValues(res_box->value_wid, args, ONE);

    fprintf(fp, "%s %s\n", resource_string, value_ptr);

    fclose(fp);
}

/*	Function Name: SaveResource
 *	Description: Save the current resource to your resource file
 *	Arguments: w - any widget in the application.
 *                 res_box_ptr - the resource box info.
 *                 junk - UNUSED.
 *	Returns: none
 */

/* ARGSUSED */
void
SaveResource(w, res_box_ptr, junk)
Widget w;
XtPointer res_box_ptr, junk;
{
    /* 
     * If there is no filename the ask for one, otherwise just save to
     * current file.
     */

    if (streq(global_resources.save_resources_file, ""))
	_PopupFileDialog(XtParent(w), "Enter file to dump resources into:",
			 global_resources.save_resources_file,
			 _AppendResourceString, res_box_ptr);
    else
	_AppendResourceString(w, res_box_ptr, NULL);
}

/*	Function Name: _SetResourcesFile
 *	Description: Sets the filename of the file to save the resources to.
 *	Arguments: w - UNUSED
 *                 junk - UNUSED
 *                 filename_ptr - a pointer to the filename;
 *	Returns: none
 */

/* ARGSUSED */
static void
_SetResourcesFile(w, junk, filename_ptr)
Widget w;
XtPointer junk, filename_ptr;
{
    char *filename = (char *) filename_ptr;

    if (global_resources.allocated_save_resources_file) 
	XtFree(global_resources.save_resources_file);
    else
	global_resources.allocated_save_resources_file = TRUE;

    global_resources.save_resources_file = XtNewString(filename);
}

/*	Function Name: SetFile
 *	Description: Changes the current save file
 *	Arguments: w - UNUSED.
 *                 res_box_ptr - UNUSED.
 *                 junk - UNUSED.
 *	Returns: none
 */

/* ARGSUSED */
void
SetFile(w, junk, garbage)
Widget w;
XtPointer junk, garbage;
{
    /* 
     * If there is no filename the ask for one, otherwise just save to
     * current file.
     */

    _PopupFileDialog(XtParent(w), "Enter file to dump resources into:",
		     global_resources.save_resources_file,
		     _SetResourcesFile, NULL);
}

/*	Function Name: ApplyResource
 *	Description: Apply the current resource to the running application.
 *	Arguments: w - any widget in the application.
 *                 node_ptr - a pointer to the node containing 
 *                            the current resouce box.
 *                 junk - UNUSED.
 *	Returns: none
 */

/* ARGSUSED */
void
ApplyResource(w, node_ptr, junk)
Widget w;
XtPointer node_ptr, junk;
{
    ProtocolStream * stream = &(global_client.stream);
    ApplyResourcesInfo info;
    WNode * node = (WNode *) node_ptr;	       
    char * value;
    unsigned short size, i;
    long len;
    Arg args[1];

    info.name = GetResourceName(node->resources->res_box);
    info.class = "IGNORE_ME";	/* Not currently used.  */
    info.stream = stream;
    info.count = 0;

    XtSetArg(args[0], XtNlabel, &value);
    XtGetValues(node->resources->res_box->res_label, args, ONE);

    info.database = NULL;
    XrmPutLineResource(&(info.database), value);


    _XEditResResetStream(stream);
    _XEditResPutString8(stream, info.name); /* Insert name */
    _XEditResPutString8(stream, XtRString); /* insert type */

    /*
     * Insert value.
     */

    value = GetResourceValueForSetValues(node, &size);
    _XEditResPut16(stream, size);    
    for (i = 0; i < size; i++) 
	_XEditResPut8(stream, value[i]);
    XtFree(value);
    len = stream->current - stream->top;

    /* 
     * Insert the widget count, overriden later. 
     */

    _XEditResPut16(stream, 0); 

    ExecuteOverAllNodes(node->tree_info->top_node,
			CreateSetValuesCommand, (XtPointer) &info);
    
    if (info.count > 0) {
	*(stream->top + len++) = info.count >> XER_NBBY; /* Set the correct */
	*(stream->top + len) = info.count;               /* count. */

	SetCommand(node->tree_info->tree_widget, LocalSetValues, NULL);
    }
    else 
	SetMessage(global_screen_data.info_label,
		   res_labels[20]);
	
    XrmDestroyDatabase(info.database);
}

/*	Function Name: ObtainResource
 *	Description: Obtain the current resource from the running application.
 *	Arguments: node_ptr - a pointer to the node containing 
 *                            the current resouce box.
 *	Returns: none
 */

/* ARGSUSED */
static void
ObtainResource(node_ptr)
XtPointer node_ptr;
{
    ProtocolStream * stream = &(global_client.stream);
    ObtainResourcesInfo info;
    WNode * node = (WNode *) node_ptr;	       
    char * value;
    Arg args[1];

    info.name = GetResourceName(node->resources->res_box);
    info.class = "IGNORE_ME";	/* Not currently used.  */
    info.stream = stream;
    info.count = 1;

    XtSetArg(args[0], XtNlabel, &value);
    XtGetValues(node->resources->res_box->res_label, args, ONE);

    info.database = NULL;
    XrmPutLineResource(&(info.database), value);

    _XEditResResetStream(stream);
    _XEditResPutString8(stream, info.name); /* insert name */

    /* 
     * Insert the widget count, always 1
     */

    _XEditResPut16(stream, 1); 

    /*CreateGetValuesCommand(node, (XtPointer)&info);  Inserts widget */

    /* Insert widget */
    InsertWidgetFromNode(stream, node);

    SetCommand(node->tree_info->tree_widget, LocalGetValues, NULL);
}

/*	Function Name: CreateSetValuesCommand
 *	Description: Creates the SetValues command if this widget
 *                   matches the resource string in the database.
 *	Arguments: node - the current node.
 *                 info_ptr - the pointer to the apply info.
 *	Returns: none
 */

static void
CreateSetValuesCommand(node, info_ptr)
WNode * node;
XtPointer info_ptr;
{
    ApplyResourcesInfo * info = (ApplyResourcesInfo *) info_ptr;
    XrmNameList name_quarks;
    XrmClassList class_quarks;
    char ** names, **classes;

    GetNamesAndClasses(node, &names, &classes);
    name_quarks = (XrmNameList) Quarkify(names, info->name);
    class_quarks = (XrmNameList) Quarkify(classes, info->class);

    if (CheckDatabase(info->database, name_quarks, class_quarks)) {
	InsertWidgetFromNode(info->stream, node);
	info->count++;
    }

    XtFree((char *)names);
    XtFree((char *)classes);
    XtFree((char *)name_quarks);
    XtFree((char *)class_quarks);
}

/*	Function Name: CreateGetValuesCommand
 *	Description: Creates the GetValues command.
 *	Arguments: node - the current node.
 *                 info_ptr - the pointer to the apply info.
 *	Returns: none
 */

/*****

static void
CreateGetValuesCommand(node, info_ptr)
WNode * node;
XtPointer info_ptr;
{
    ApplyResourcesInfo * info = (ApplyResourcesInfo *) info_ptr;
    XrmNameList name_quarks;
    XrmClassList class_quarks;
    char ** names, **classes;

    GetNamesAndClasses(node, &names, &classes);
    name_quarks = (XrmNameList) Quarkify(names, info->name);
    class_quarks = (XrmNameList) Quarkify(classes, info->class);

    if (CheckDatabase(info->database, name_quarks, class_quarks)) {
	InsertWidgetFromNode(info->stream, node);
	info->count++;
    }

    XtFree((char *)names);
    XtFree((char *)classes);
    XtFree((char *)name_quarks);
    XtFree((char *)class_quarks);
}

*****/

/*	Function Name: ActivateResourceWidgets
 *	Description: Activates all widgets that match this resource.
 *	Arguments: w - UNUSED.
 *                 node_ptr - the node that owns this resource box.
 *                 junk - UNUSED. 
 *	Returns: none.
 */

/* ARGSUSED */
void
ActivateResourceWidgets(w, node_ptr, junk)
Widget w;
XtPointer node_ptr, junk;
{
    WNode * node = (WNode *) node_ptr;	       
    ApplyResourcesInfo info;
    char * line;
    Arg args[1];

    info.name = GetResourceName(node->resources->res_box);
    info.class = "IGNORE_ME";	/* Not currently used.  */

    /* 
     * Unused fields.
     */

    info.count = 0;
    info.stream = NULL;

    XtSetArg(args[0], XtNlabel, &line);
    XtGetValues(node->resources->res_box->res_label, args, ONE);

    info.database = NULL;
    XrmPutLineResource(&(info.database), line);


    ExecuteOverAllNodes(node->tree_info->top_node,
			SetOnlyMatchingWidgets, (XtPointer) &info);
    
    XrmDestroyDatabase(info.database);
}

/*	Function Name: SetOnlyMatchingWidgets
 *	Description: Activates all widgets in the tree that match this
 *                   resource specifiction.
 *	Arguments: node - the current node.
 *                 info_ptr - the pointer to the apply info.
 *	Returns: none
 */

static void
SetOnlyMatchingWidgets(node, info_ptr)
WNode * node;
XtPointer info_ptr;
{
    ApplyResourcesInfo * info = (ApplyResourcesInfo *) info_ptr;
    XrmNameList name_quarks;
    XrmClassList class_quarks;
    char ** names, **classes;
    Boolean state;
    Arg args[1];

    GetNamesAndClasses(node, &names, &classes);
    name_quarks = (XrmNameList) Quarkify(names, info->name);
    class_quarks = (XrmNameList) Quarkify(classes, info->class);

    state = CheckDatabase(info->database, name_quarks, class_quarks);

    XtSetArg(args[0], XtNstate, state);
    XtSetValues(node->widget, args, ONE);
    TreeToggle(node->widget, (XtPointer) node, (XtPointer)(long) state);

    XtFree((char *)names);
    XtFree((char *)classes);
    XtFree((char *)name_quarks);
    XtFree((char *)class_quarks);
}

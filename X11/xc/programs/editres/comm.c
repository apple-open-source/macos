/* $Xorg: comm.c,v 1.5 2001/02/09 02:05:29 xorgcvs Exp $ */
/*

Copyright 1990, 1998  The Open Group

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall
not be used in advertising or otherwise to promote the sale, use or
other dealings in this Software without prior written authorization
from The Open Group.

*/
/* $XFree86: xc/programs/editres/comm.c,v 1.5 2001/12/14 20:00:42 dawes Exp $ */


/*
 * This file contains the code to communicate with the client that is
 * being edited.
 */

#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>	/* Get standard string definitions. */
#include <X11/Xatom.h>
#include <X11/cursorfont.h>	/* For crosshair cursor. */
#include <X11/Xproto.h>
#include <X11/Xos.h>		/* for XtNewString */

#include <stdio.h>
#include <X11/Xmu/Error.h>
#include <X11/Xmu/WinUtil.h>

#include "editresP.h"

/*
 * static Globals.
 */

static Atom atom_comm, atom_command, atom_resource_editor, atom_client_value;
static Atom atom_editres_protocol;

extern Widget CM_entries[NUM_CM_ENTRIES], TM_entries[NUM_TM_ENTRIES];

static void ClientTimedOut ( XtPointer data, XtIntervalId * id );
static void TellUserAboutMessage ( Widget label, ResCommand command );
static Boolean ConvertCommand ( Widget w, Atom * selection, Atom * target, 
				Atom * type_ret, XtPointer *value_ret, 
				unsigned long * length_ret, int * format_ret );
static void SelectionDone ( Widget w, Atom *sel, Atom *targ );
static void LoseSelection ( Widget w, Atom * sel );
static void GetClientValue ( Widget w, XtPointer data, Atom *selection, 
			     Atom *type, XtPointer value, 
			     unsigned long *length, int * format );
static void BuildHeader ( CurrentClient * client_data );
static Event * BuildEvent ( ProtocolStream * stream );
static void FreeEvent ( Event * event );
static char * DispatchEvent ( Event * event );



/*	Function Name: ClientTimedOut
 *	Description: Called if the client takes too long to take our selection.
 *	Arguments: data - The widget that owns the client 
 *                        communication selection.
 *                 id - *** UNUSED ***
 *	Returns: none.
 */

/* ARGSUSED */
static void
ClientTimedOut(data, id)
XtPointer data;
XtIntervalId * id;
{
  char msg[BUFSIZ];
  Widget w = (Widget) data;
  
  global_client.ident = NO_IDENT;
  XtDisownSelection(w, global_client.atom, 
		    XtLastTimestampProcessed(XtDisplay(w)));
  
  sprintf(msg, res_labels[4],
	  "the Editres Protocol.");
  SetMessage(global_screen_data.info_label, msg);
}



/*	Function Name: GetClientWindow
 *	Description: Gets the Client's window by asking the user.
 *	Arguments: w - a widget.
 *	Returns: a clients window, or None.
 */

Window 
GetClientWindow(w, x, y)
Widget w;
int *x, *y;
{
  int status;
  Cursor cursor;
  XEvent event;
  int buttons = 0;
  Display * dpy = XtDisplayOfObject(w);
  Window target_win = None, root = RootWindowOfScreen(XtScreenOfObject(w));
  XtAppContext app = XtWidgetToApplicationContext(w);
  
  /* Make the target cursor */
  cursor = XCreateFontCursor(dpy, XC_crosshair);
  
  /* Grab the pointer using target cursor, letting it room all over */
  status = XGrabPointer(dpy, root, False,
			ButtonPressMask|ButtonReleaseMask, GrabModeSync,
			GrabModeAsync, root, cursor, CurrentTime);
  if (status != GrabSuccess) {
    SetMessage(global_screen_data.info_label, res_labels[5]);
    return(None);
  }
  
  /* Let the user select a window... */
  while ((target_win == None) || (buttons != 0)) {
    /* allow one more event */
    XAllowEvents(dpy, SyncPointer, CurrentTime);
    XtAppNextEvent(app, &event);
    switch (event.type) {
    case ButtonPress:
      if (event.xbutton.window != root) {
	XtDispatchEvent(&event);
	break;
      }
      
      if (target_win == None) {
	target_win = event.xbutton.subwindow; /* window selected */
	if (x != NULL)
	  *x = event.xbutton.x_root;
	if (y != NULL)
	  *y = event.xbutton.y_root;
      }
      buttons++;
      break;
    case ButtonRelease:
      if (event.xbutton.window != root) {
	XtDispatchEvent(&event);
	break;
      }
      
      if (buttons > 0) /* There may have been some
			  down before we started */
	buttons--;
      break;
    default:
      XtDispatchEvent(&event);
      break;
    }
  } 
  
  XUngrabPointer(dpy, CurrentTime);      /* Done with pointer */
  
  return(XmuClientWindow(dpy, target_win));
}



/*	Function Name: SetCommand
 *	Description: Causes this widget to own the resource editor's 
 *                   command selection.
 *	Arguments: w - the widget that will own the selection.
 *                 command - command to send to client.
 *                 msg - message to prompt the user to select a client.
 *	Returns: none.
 */

/* ARGSUSED */
void
SetCommand(w, command, msg)
     Widget w;
     ResCommand command;
     char * msg;
{
  XClientMessageEvent client_event;
  Display * dpy = XtDisplay(w);
  
  if (msg == NULL) 
    msg = res_labels[6];
  
  SetMessage(global_screen_data.info_label, msg);
  
  if (global_client.window == None)
    if ( (global_client.window = GetClientWindow(w, NULL, NULL)) == None) 
      return;

  global_client.ident = GetNewIdent();
    
  global_client.command = command;
  global_client.atom = atom_comm;

  BuildHeader(&(global_client)); 

  if (!XtOwnSelection(w, global_client.atom, CurrentTime, ConvertCommand, 
		      LoseSelection, SelectionDone))
    SetMessage(global_screen_data.info_label,
	       res_labels[7]);

  client_event.window = global_client.window;
  client_event.type = ClientMessage;
  client_event.message_type = atom_resource_editor;
  client_event.format = EDITRES_SEND_EVENT_FORMAT;
  client_event.data.l[0] = XtLastTimestampProcessed(dpy);
  client_event.data.l[1] = global_client.atom;
  client_event.data.l[2] = (long) global_client.ident;
  client_event.data.l[3] = global_effective_protocol_version;
  
  global_error_code = NO_ERROR;                 /* Reset Error code. */
  global_old_error_handler = XSetErrorHandler(HandleXErrors);
  global_serial_num = NextRequest(dpy);
  
  XSendEvent(dpy, global_client.window, FALSE, (long) 0, 
	     (XEvent *) &client_event);
  
  XSync(dpy, FALSE);
  XSetErrorHandler(global_old_error_handler);
  if (global_error_code == NO_WINDOW) {
    char error_buf[BUFSIZ];
    
    global_error_code = NO_ERROR;	/* Reset Error code. */
    sprintf(error_buf, "The communication window with%s%s.",
	    " application is no longer available\n",
	    "Please select a new widget tree");
    
    global_client.window = None;
    SetCommand(w, LocalSendWidgetTree, error_buf);
    return;
  }   
  
  TellUserAboutMessage(global_screen_data.info_label, command);
  global_client.timeout = XtAppAddTimeOut(XtWidgetToApplicationContext(w),
					  CLIENT_TIME_OUT, 
					  ClientTimedOut, (XtPointer) w);
}



/*	Function Name: TellUserAboutMessage
 *	Description: Informs the user that we have sent a message to the client
 *	Arguments: label - the info label.
 *                 command - command that we have executed.
 *	Returns: none.
 */

static void
TellUserAboutMessage(label, command)
Widget label;
ResCommand command;
{
    char msg[BUFSIZ], *str;

    switch(command) {
    case LocalSendWidgetTree:
	str = " asking for widget tree";
	break;
    case LocalSetValues:
	str = " asking it to perform SetValues()";
	break;
    case LocalFlashWidget:
    case LocalGetGeometry:
	str = " asking it to perform GetGeometry()";
	break;
    case LocalGetResources:
	str = " asking it to get a widget's resource list";
	break;
    case LocalFindChild:
	str = " asking it to find the child Widget.";
	break;
    default:
	str = "";
	break;
    }

    sprintf(msg, res_labels[8], str);
    SetMessage(label, msg);
}



/*	Function Name: ConvertCommand
 *	Description: Converts the command string into a selection that can
 *                   be sent to the client.
 *	Arguments: (see Xt)
 *	Returns: TRUE if we could convert the selection and target asked for.
 */

/* ARGSUSED */
static Boolean
ConvertCommand(w,selection,target,type_ret, value_ret, length_ret, format_ret)
Widget w;
Atom * selection, * target, * type_ret;
XtPointer *value_ret;
unsigned long * length_ret;
int * format_ret;
{
    if ((*selection != atom_comm) || (*target != atom_command))
	return(FALSE);

    *type_ret = atom_editres_protocol;
    *value_ret = (XtPointer) global_client.stream.real_top;
    *length_ret = global_client.stream.size + HEADER_SIZE;
    *format_ret = EDITRES_FORMAT;
    
    return(TRUE);
}



/*	Function Name: SelectionDone
 *	Description: done with the selection.
 *	Arguments: *** UNUSED ***
 *	Returns: none.
 */

/* ARGSUSED */
static void
SelectionDone(w, sel, targ)
    Widget w;
    Atom *sel, *targ;
{
    /* Keep the toolkit from automaticaly freeing the selection value */
}



/*	Function Name: LoseSelection
 *	Description: Called when we have lost the selection, asks client
 *                   for the selection value.
 *	Arguments: w - the widget that just lost the selection.
 *                 sel - the selection.
 *	Returns: none.
 */

static void
LoseSelection(w, sel)
Widget w;
Atom * sel;
{
    if (global_client.timeout != 0) {
	XtRemoveTimeOut(global_client.timeout);
	global_client.timeout = 0;
    }

    XtGetSelectionValue(w, *sel, atom_client_value, GetClientValue,
			NULL, XtLastTimestampProcessed(XtDisplay(w)));
}



/*	Function Name: GetClientValue
 *	Description: Gets the value out of the client, and does good things
 *                   to it.
 *	Arguments: w - the widget that asked for the selection.
 *                 data - client_data *** UNUSED ***.
 *                 sel - the selection.
 *                 type - the type of the selection.
 *                 value - the selection's value.
 *                 length - the length of the selection's value.
 *                 format - the format of the selection.
 *	Returns: none.
 */

static Boolean reset_protocol_level = True;

/* ARGSUSED */
static void
GetClientValue(w, data, selection, type, value, length, format)
Widget w;
XtPointer data, value;
Atom *selection, *type;
unsigned long *length;
int * format;
{
    Event * event;
    ProtocolStream alloc_stream, *stream;
    unsigned char ident, error_code;
    char * error_str = NULL, msg[BUFSIZ];

    if (*length == 0)
	return;

    stream = &alloc_stream;	/* easier to think of it this way... */

    stream->current = stream->top = (unsigned char *) value;
    stream->size = HEADER_SIZE;		/* size of header. */

    /*
     * Retrieve the Header.
     */

    if (*length < HEADER_SIZE) {
	SetMessage(global_screen_data.info_label,
		   res_labels[9]);
	return;
    }

    (void) _XEditResGet8(stream, &ident);
    if (global_client.ident != ident) {
#ifdef DEBUG
	if (global_resources.debug)
	    printf("Incorrect ident from client.\n");
#endif 
	if (!XtOwnSelection(w, *selection, CurrentTime, ConvertCommand, 
			    LoseSelection, SelectionDone))
	    SetMessage(global_screen_data.info_label,
		       res_labels[10]);
	return;
    }

    (void) _XEditResGet8(stream, &error_code); 
    (void) _XEditResGet32(stream, &(stream->size));
    stream->top = stream->current; /* reset stream to top of value.*/

    switch ((int) error_code) {
    case PartialSuccess:
/*****
        if (global_client.command == LocalSendWidgetTree &&
	    global_effective_protocol_version < CURRENT_PROTOCOL_VERSION)
	  ++global_effective_protocol_version;
*****/
	if ((event = BuildEvent(stream)) != NULL) {
	    error_str = DispatchEvent(event);
	    FreeEvent(event);
	}
	else {
	    sprintf(msg, "Unable to unpack protocol request.");
	    error_str = XtNewString(msg);
	}
	break;
    case Failure:
	error_str = GetFailureMessage(stream);
	break;
    case ProtocolMismatch:
	error_str = ProtocolFailure(stream);
	--global_effective_protocol_version;
	/* normaly protocol version is reset to current during a SendWidgetTree
         * request, however, after a protocol failure this is skiped once for
         * a retry.
         */
	reset_protocol_level = False;
	SetCommand(w, LocalSendWidgetTree, NULL);
	break;
    default:
	sprintf(msg, res_labels[11], (int) error_code);
	SetMessage(global_screen_data.info_label, msg);
	break;
    }

    if (error_str == NULL) {
	WNode * top;
	
	if (global_tree_info == NULL)
	    return;
	
	top = global_tree_info->top_node;
	sprintf(msg, res_labels[12], top->name, top->class);
	SetMessage(global_screen_data.info_label, msg);
	return;
    }
    SetMessage(global_screen_data.info_label, error_str);
    XtFree(error_str);
}



/*	Function Name: BuildHeader
 *	Description: Puts the header into the message.
 *	Arguments: client_data - the client data.
 *	Returns: none.
 */

static void
BuildHeader(client_data)
CurrentClient * client_data;
{
    unsigned long old_alloc, old_size;
    unsigned char * old_current;
    EditresCommand command;
    ProtocolStream * stream = &(client_data->stream);

    /*
     * We have cleverly keep enough space at the top of the header
     * for the return protocol stream, so all we have to do is
     * fill in the space.
     */

    /* 
     * Fool the insert routines into putting the header in the right
     * place while being damn sure not to realloc (that would be very bad.
     */
    
    old_current = stream->current;
    old_alloc = stream->alloc;
    old_size = stream->size;

    stream->current = stream->real_top;
    stream->alloc = stream->size + (2 * HEADER_SIZE);	
    
    _XEditResPut8(stream, client_data->ident);
    switch(client_data->command) {
    case LocalSendWidgetTree: 
        if (reset_protocol_level) global_effective_protocol_version = 
	  CURRENT_PROTOCOL_VERSION;
        reset_protocol_level = True;
	command = SendWidgetTree;
	break;
    case LocalSetValues:
	command = SetValues;
	break;
    case LocalFlashWidget:
	command = GetGeometry;
	break;
    case LocalGetResources:
	command = GetResources;
	break;
    case LocalFindChild:
	command = FindChild;
	break;
    case LocalGetValues:
	command = GetValues;
	break;
    default:
	command = SendWidgetTree;
	break;
    }
				  
    _XEditResPut8(stream, (unsigned char) command);
    _XEditResPut32(stream, old_size);

    stream->alloc = old_alloc;
    stream->current = old_current;
    stream->size = old_size;
}



/*	Function Name: BuildEvent
 *	Description: Builds the event structure from the 
 *	Arguments: stream - the protocol data stream.
 *	Returns: event - the event.
 */

static Event * 
BuildEvent(stream)
ProtocolStream * stream;
{
    int i;
    Event * event = (Event *) XtCalloc(sizeof(Event), 1);

    /*
     * The return value will be different depending upon the
     * request sent out.
     */

    switch(global_client.command) {
    case LocalSendWidgetTree:
        {
	    SendWidgetTreeEvent * send_event = (SendWidgetTreeEvent *) event;

	    send_event->type = SendWidgetTree;

	    if (!_XEditResGet16(stream, &(send_event->num_entries)))
		goto done;
	    
	    send_event->info = (WidgetTreeInfo *)
		                XtCalloc(sizeof(WidgetTreeInfo),
					 send_event->num_entries);

	    for (i = 0; i < (int)send_event->num_entries; i++) {
		WidgetTreeInfo * info = send_event->info + i;
		if (!(_XEditResGetWidgetInfo(stream, &(info->widgets)) &&
		      _XEditResGetString8(stream, &(info->name)) &&
		      _XEditResGetString8(stream, &(info->class)) &&
		      _XEditResGet32(stream, &(info->window)))) 
		{
		    goto done;
		}
	    }

	    if (global_effective_protocol_version == 
		CURRENT_PROTOCOL_VERSION) {
				/* get toolkit type and reset if necessary */
	      if (!_XEditResGetString8(stream, &(send_event->toolkit)))
 	        goto done;
	    }
	    /* set the command menu entries senitive */
	    SetEntriesSensitive(&CM_entries[CM_OFFSET], CM_NUM, True);
	    /* set the tree menu entries senitive */
	    SetEntriesSensitive(TM_entries, TM_NUM, True);
	    if (global_effective_protocol_version == 
		CURRENT_PROTOCOL_VERSION) {
	      if (!strcmp(send_event->toolkit, "InterViews")) 
	        RebuildMenusAndLabel("iv");
            } 
            else
              RebuildMenusAndLabel("xt");
	}
	break;
    case LocalSetValues:
        {
	    SetValuesEvent * sv_event = (SetValuesEvent *) event;

	    sv_event->type = SetValues;

	    if (!_XEditResGet16(stream, &(sv_event->num_entries)))
		goto done;
	    
	    sv_event->info = (SetValuesInfo *) XtCalloc(sizeof(SetValuesInfo),
							sv_event->num_entries);

	    for (i = 0; i < (int)sv_event->num_entries; i++) {
		SetValuesInfo * info = sv_event->info + i;
		if (!(_XEditResGetWidgetInfo(stream, &(info->widgets)) &&
		      _XEditResGetString8(stream, &(info->message))))
		{
		    goto done;
		}
	    }
	}
	break;
    case LocalGetResources:
        {
	    GetResourcesEvent * res_event = (GetResourcesEvent *) event;
	    
	    res_event->type = GetGeometry;

	    if (!_XEditResGet16(stream, &(res_event->num_entries)))
		goto done;

	    res_event->info = (GetResourcesInfo *) 
		                   XtCalloc(sizeof(GetResourcesInfo),
					    res_event->num_entries);

	    for (i = 0; i < (int)res_event->num_entries; i++) {
		GetResourcesInfo * res_info = res_event->info + i;
		if (!(_XEditResGetWidgetInfo(stream, &(res_info->widgets)) &&
		      _XEditResGetBoolean(stream, &(res_info->error))))
		{
		    goto done;
		}
		if (res_info->error) {
		    if (!_XEditResGetString8(stream, &(res_info->message))) 
			goto done;
		}
		else {
		    unsigned int j;

		    if (!_XEditResGet16(stream, &(res_info->num_resources)))
			goto done;

		    res_info->res_info = (ResourceInfo *) 
			                  XtCalloc(sizeof(ResourceInfo),
						   res_info->num_resources);

		    for (j = 0; j < res_info->num_resources; j++) {
			unsigned char temp;
			ResourceInfo * info = res_info->res_info + j;
			if (!(_XEditResGetResType(stream, &(temp)) &&
			      _XEditResGetString8(stream, &(info->name)) &&
			      _XEditResGetString8(stream, &(info->class)) &&
			      _XEditResGetString8(stream, &(info->type))))
			{
			    goto done;
			}
			else
			    info->res_type = (ResourceType) temp;
		    } /* for */
		} /* else */
	    } /* for */
	}
	break;
    case LocalFlashWidget:
    case LocalGetGeometry:
        {
	    GetGeomEvent * geom_event = (GetGeomEvent *) event;

	    geom_event->type = GetGeometry;

	    if (!_XEditResGet16(stream, &(geom_event->num_entries)))
		goto done;
	    
	    geom_event->info = (GetGeomInfo *) XtCalloc(sizeof(GetGeomInfo),
						      geom_event->num_entries);

	    for (i = 0; i < (int)geom_event->num_entries; i++) {
		GetGeomInfo * info = geom_event->info + i;
		if (!(_XEditResGetWidgetInfo(stream, &(info->widgets)) &&
		      _XEditResGetBoolean(stream, &(info->error))))
		{
		    goto done;
		}
		if (info->error) {
		    if (!_XEditResGetString8(stream, &(info->message)))
			goto done;
		}
		else {
		    if (!(_XEditResGetBoolean(stream, &(info->visable)) &&
			  _XEditResGetSigned16(stream, &(info->x)) &&
			  _XEditResGetSigned16(stream, &(info->y)) &&
			  _XEditResGet16(stream, &(info->width)) &&
			  _XEditResGet16(stream, &(info->height)) &&
			  _XEditResGet16(stream, &(info->border_width))))
		    {
			goto done;
		    }
		}
	    }
	}
	break;
    case LocalFindChild:
        {
	    FindChildEvent * find_event = (FindChildEvent *) event;

	    find_event->type = FindChild;

	    if (!_XEditResGetWidgetInfo(stream, &(find_event->widgets)))
		goto done;
	}
	break;
    case LocalGetValues: /* This is for REPLY... */
	{
	  Arg args[1];
	  GetValuesEvent * gv_event = (GetValuesEvent *) event;
	  
	  gv_event->type = GetValues;
	  
	  if (!_XEditResGet16(stream, &(gv_event->num_entries)))
	    goto done;

	  gv_event->info = (GetValuesInfo*)XtCalloc(sizeof(GetValuesInfo),1);

	  {
	    GetValuesInfo * info = gv_event->info;
	    if (!(_XEditResGetString8(stream, &(info->value))))
	      {
		goto done;
	      }

	    /* set the string value of the asciitext widget. note that only
             * one active node is dealt with here.  This is ok because only
	     * one node can be active when the resource box is up.
	     */

	    XtSetArg (args[0], XtNstring, info->value);

	    XtSetValues(
	      global_tree_info->active_nodes[0]->resources->res_box->value_wid,
 	      args, 1
	    );
	  }
	}
	break;

    default:
	goto done;
    }

    return(event);

 done:
    FreeEvent(event);
    return(NULL);
}



/*	Function Name: FreeEvent
 *	Description: Frees all memory associated with the event. 
 *	Arguments: event - the event.
 *	Returns: none.
 *
 * NOTE: XtFree() returns w/o freeing if ptr is NULL.
 */

static void
FreeEvent(event)
Event * event;
{
    unsigned int i;

    switch(event->any_event.type) {
    case SendWidgetTree:
        {
	    SendWidgetTreeEvent * send_event = (SendWidgetTreeEvent *) event;
	    WidgetTreeInfo * info = send_event->info;
	    
	    if (info != NULL) {
		for (i = 0; i < send_event->num_entries; i++, info++) {
		    XtFree((char *)info->widgets.ids);
		    XtFree(info->name);
		    XtFree(info->class);
		}
		XtFree((char *)send_event->info);
	    }
	}
	break;
    case SetValues:
        {
	    SetValuesEvent * sv_event = (SetValuesEvent *) event;
	    SetValuesInfo * info = sv_event->info;
	    
	    if (info != NULL) {
		for (i = 0; i < sv_event->num_entries; i++, info++) {
		    XtFree((char *)info->widgets.ids);
		    XtFree(info->message);
		}
		XtFree((char *)sv_event->info);
	    }
	}
	break;
    case GetResources:
        {
	    GetResourcesEvent * get_event = (GetResourcesEvent *) event;
	    GetResourcesInfo * info = get_event->info;

	    if (info != NULL) {
		for (i = 0; i < get_event->num_entries; i++, info++) {
		    XtFree((char *)info->widgets.ids);
		    if (info->error) 
			XtFree(info->message);
		    else {
			unsigned int j;
			ResourceInfo * res_info = info->res_info;
			
			if (res_info != NULL) {
			    for (j = 0; 
				 j < info->num_resources; j++, res_info++) 
			    {
				XtFree(res_info->name);
				XtFree(res_info->class);
				XtFree(res_info->type);
			    }
			    XtFree((char *)info->res_info);
			}
		    }
		} 
		XtFree((char *)get_event->info);
	    }
	}
	break;
    case GetGeometry:
        {
	    GetGeomEvent * geom_event = (GetGeomEvent *) event;
	    GetGeomInfo * info = geom_event->info;

	    if (info != NULL) {
		for (i = 0; i < geom_event->num_entries; i++, info++) {
		    XtFree((char *)info->widgets.ids);
		    if (info->error) 
			XtFree(info->message);
		}
		XtFree((char *)geom_event->info);
	    }
	}
	break;
    case FindChild:
        {
	    FindChildEvent * find_event = (FindChildEvent *) event;
	    
	    XtFree((char *)find_event->widgets.ids);
	}
	break;
    default:
	break;
    }
}



/*	Function Name: DispatchEvent
 *	Description: Handles the event, calling the proper function.
 *	Arguments: event - the event.
 *	Returns: one.
 */
	
static char *
DispatchEvent(event)
Event * event;
{
    char * error = NULL;

    switch(global_client.command) {
    case LocalSendWidgetTree:
	BuildVisualTree(global_tree_parent, event);
	break;
    case LocalSetValues:
	error = PrintSetValuesError(event);
	break;
    case LocalFlashWidget:
	error = HandleFlashWidget(event);
	break;
    case LocalGetResources:
	error = HandleGetResources(event);
	break;
    case LocalFindChild:
	DisplayChild(event);
	break;
    case LocalGetValues:
	break;
    default:
        {
	    char msg[BUFSIZ];
	    sprintf(msg, "Internal error: Unknown command %d.", 
		    global_client.command);
	    error = XtNewString(msg);
	}
	break;
    }
    return(error);
}



/*	Function Name: InternAtoms
 *	Description: interns all static atoms.
 *	Arguments: display - the current display.
 *	Returns: none.
 */

void
InternAtoms(dpy)
Display * dpy;
{
    atom_comm = XInternAtom(dpy, EDITRES_COMM_ATOM, False);
    atom_command = XInternAtom(dpy, EDITRES_COMMAND_ATOM, False);
    atom_resource_editor = XInternAtom(dpy, EDITRES_NAME, False);
    atom_client_value = XInternAtom(dpy, EDITRES_CLIENT_VALUE, False);
    atom_editres_protocol = XInternAtom(dpy, EDITRES_PROTOCOL_ATOM, False);
}

ResIdent
GetNewIdent()
{
    static ResIdent ident = 1;

    return(ident++);
}


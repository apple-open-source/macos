/******************************************************************************

Copyright 1994, 1998  The Open Group

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

Author:  Ralph Mor, X Consortium
******************************************************************************/

#include "smproxy.h"
#include <unistd.h>
#include <X11/Xmu/WinUtil.h>

static XtAppContext appContext;
static Display *disp;

static Atom wmProtocolsAtom;
static Atom wmSaveYourselfAtom;
static Atom wmStateAtom;
static Atom smClientIdAtom;
static Atom wmClientLeaderAtom;

static Bool debug = 0;

static SmcConn proxy_smcConn;
static XtInputId proxy_iceInputId;
static char *proxy_clientId = NULL;

WinInfo *win_head = NULL;

static int proxy_count = 0;
static int die_count = 0;

static Bool ok_to_die = 0;

static Bool caught_error = 0;

static Bool sent_save_done = 0;

static int Argc;
static char **Argv;

static Bool HasSaveYourself ( Window window );
static Bool HasXSMPsupport ( Window window );
static WinInfo * GetClientLeader ( WinInfo *winptr );
static char * CheckFullyQuantifiedName ( char *name, int *newstring );
static void FinishSaveYourself ( WinInfo *winInfo, Bool has_WM_SAVEYOURSELF );
static void SaveYourselfCB ( SmcConn smcConn, SmPointer clientData, int saveType, 
			    Bool shutdown, int interactStyle, Bool fast );
static void DieCB ( SmcConn smcConn, SmPointer clientData );
static void SaveCompleteCB ( SmcConn smcConn, SmPointer clientData );
static void ShutdownCancelledCB ( SmcConn smcConn, SmPointer clientData );
static void ProcessIceMsgProc ( XtPointer client_data, int *source, XtInputId *id );
static void NullIceErrorHandler ( IceConn iceConn, Bool swap, 
			   int offendingMinorOpCode, 
			   unsigned long offendingSequence, 
			   int errorClass, int severity, IcePointer values );
static void ConnectClientToSM ( WinInfo *winInfo );
static int MyErrorHandler ( Display *display, XErrorEvent *event );
static Bool LookupWindow ( Window window, WinInfo **ptr_ret, WinInfo **prev_ptr_ret );
static WinInfo * AddNewWindow ( Window window );
static void RemoveWindow ( WinInfo *winptr );
static void Got_WM_STATE ( WinInfo *winptr );
static void HandleCreate ( XCreateWindowEvent *event );
static void HandleDestroy ( XDestroyWindowEvent *event );
static void HandleUpdate ( XPropertyEvent *event );
static void ProxySaveYourselfPhase2CB ( SmcConn smcConn, SmPointer clientData );
static void ProxySaveYourselfCB ( SmcConn smcConn, SmPointer clientData, 
			   int saveType, Bool shutdown, int interactStyle, 
			   Bool fast );
static void ProxyDieCB ( SmcConn smcConn, SmPointer clientData );
static void ProxySaveCompleteCB ( SmcConn smcConn, SmPointer clientData );
static void ProxyShutdownCancelledCB ( SmcConn smcConn, SmPointer clientData );
static Status ConnectProxyToSM ( char *previous_id );
static void CheckForExistingWindows ( Window root );


static Bool
HasSaveYourself(Window window)
{
    Atom *protocols;
    int numProtocols;
    int i, found;

    protocols = NULL;

    if (XGetWMProtocols (disp, window, &protocols, &numProtocols) != True)
	return (False);

    found = 0;

    if (protocols != NULL)
    {
	for (i = 0; i < numProtocols; i++)
	    if (protocols[i] == wmSaveYourselfAtom)
		found = 1;

	XFree (protocols);
    }

    return (found);
}



static Bool
HasXSMPsupport(Window window)
{
    XTextProperty tp;
    Bool hasIt = 0;

    if (XGetTextProperty (disp, window, &tp, smClientIdAtom))
    {
	if (tp.encoding == XA_STRING && tp.format == 8 && tp.nitems != 0)
	    hasIt = 1;

	if (tp.value)
	    XFree ((char *) tp.value);
    }

    return (hasIt);
}



static WinInfo *
GetClientLeader(WinInfo *winptr)
{
    Atom actual_type;
    int actual_format;
    unsigned long nitems, bytesafter;
    unsigned long *datap = NULL;
    WinInfo *leader_winptr = NULL;
    Bool failure = 0;

    if (XGetWindowProperty (disp, winptr->window, wmClientLeaderAtom,
	0L, 1L, False, AnyPropertyType,	&actual_type, &actual_format,
	&nitems, &bytesafter, (unsigned char **) &datap) == Success)
    {
	if (actual_type == XA_WINDOW && actual_format == 32 &&
	    nitems == 1 && bytesafter == 0)
	{
	    Window leader_win = *((Window *) datap);

	    if (!LookupWindow (leader_win, &leader_winptr, NULL))
		failure = 1;
	}

	if (datap)
	    XFree (datap);
    }

    if (failure)
    {
	/* The client leader was defined, but we couldn't find the window */

	return (NULL);
    }
    else if (leader_winptr)
    {
	/* We found the real client leader */

	return (leader_winptr);
    }
    else
    {
	/* There is no client leader defined, return this window */

	return (winptr);
    }
}



static char *
CheckFullyQuantifiedName(char *name, int *newstring)
{
    /*
     * Due to a bug in Xlib (for hpux in particular), some clients
     * will have a WM_CLIENT_MACHINE that is not fully quantified.
     * For example, we might get "excon" instead of "excon.x.org".
     * This really stinks.  The best we can do is tag on our own
     * domain name.
     */

    if (strchr (name, '.') != NULL)
    {
	*newstring = 0;
	return (name);
    }
    else
    {
	char hostnamebuf[80];
	char *firstDot;

	gethostname (hostnamebuf, sizeof hostnamebuf);
	firstDot = strchr (hostnamebuf, '.');

	if (!firstDot)
	{
	    *newstring = 0;
	    return (name);
	}
	else
	{
	    int bytes = strlen (name) + strlen (firstDot + 1) + 2;
	    char *newptr;

	    newptr = (char *) malloc (bytes);
	    sprintf (newptr, "%s.%s", name, firstDot + 1);

	    *newstring = 1;
	    return (newptr);
	}
    }
}



static void
FinishSaveYourself(WinInfo *winInfo, Bool has_WM_SAVEYOURSELF)
{
    SmProp prop1, prop2, prop3, *props[3];
    SmPropValue prop1val, prop2val, prop3val;
    int i;

    if (!winInfo->got_first_save_yourself)
    {
	char userId[20], restartService[80];
	char *fullyQuantifiedName;
	int newstring;

	prop1.name = SmProgram;
	prop1.type = SmARRAY8;
	prop1.num_vals = 1;
	prop1.vals = &prop1val;
	prop1val.value = (SmPointer) winInfo->wm_command[0];
	prop1val.length = strlen (winInfo->wm_command[0]);
    
	sprintf (userId, "%ld", (long)getuid());
	prop2.name = SmUserID;
	prop2.type = SmARRAY8;
	prop2.num_vals = 1;
	prop2.vals = &prop2val;
	prop2val.value = (SmPointer) userId;
	prop2val.length = strlen (userId);
    
	fullyQuantifiedName = CheckFullyQuantifiedName (
	    (char *) winInfo->wm_client_machine.value, &newstring);
	sprintf (restartService, "rstart-rsh/%s", fullyQuantifiedName);
	if (newstring)
	    free (fullyQuantifiedName);

	prop3.name = "_XC_RestartService";
	prop3.type = SmLISTofARRAY8;
	prop3.num_vals = 1;
	prop3.vals = &prop3val;
	prop3val.value = (SmPointer) restartService;
	prop3val.length = strlen (restartService);

	props[0] = &prop1;
	props[1] = &prop2;
	props[2] = &prop3;
	
	SmcSetProperties (winInfo->smc_conn, 3, props);

	winInfo->got_first_save_yourself = 1;
    }

    prop1.name = SmRestartCommand;
    prop1.type = SmLISTofARRAY8;
    prop1.num_vals = winInfo->wm_command_count;
    
    prop1.vals = (SmPropValue *) malloc (
	winInfo->wm_command_count * sizeof (SmPropValue));
    
    if (!prop1.vals)
    {
	SmcSaveYourselfDone (winInfo->smc_conn, False);
	return;
    }

    for (i = 0; i < winInfo->wm_command_count; i++)
    {
	prop1.vals[i].value = (SmPointer) winInfo->wm_command[i];
	prop1.vals[i].length = strlen (winInfo->wm_command[i]);
    }
    
    prop2.name = SmCloneCommand;
    prop2.type = SmLISTofARRAY8;
    prop2.num_vals = winInfo->wm_command_count;
    prop2.vals = prop1.vals;
    
    props[0] = &prop1;
    props[1] = &prop2;
    
    SmcSetProperties (winInfo->smc_conn, 2, props);
    
    free ((char *) prop1.vals);
    
    /*
     * If the client doesn't support WM_SAVE_YOURSELF, we should
     * return failure for the save, since we really don't know if
     * the application needed to save state.
     */

    SmcSaveYourselfDone (winInfo->smc_conn, has_WM_SAVEYOURSELF);
}



static void
SaveYourselfCB(SmcConn smcConn, SmPointer clientData, int saveType,
	       Bool shutdown, int interactStyle, Bool fast)
{
    WinInfo *winInfo = (WinInfo *) clientData;

    if (!winInfo->has_save_yourself)
    {
	FinishSaveYourself (winInfo, False);
    }
    else
    {
	XClientMessageEvent saveYourselfMessage;


	/* Send WM_SAVE_YOURSELF */

	saveYourselfMessage.type = ClientMessage;
	saveYourselfMessage.window = winInfo->window;
	saveYourselfMessage.message_type = wmProtocolsAtom;
	saveYourselfMessage.format = 32;
	saveYourselfMessage.data.l[0] = wmSaveYourselfAtom;
	saveYourselfMessage.data.l[1] = CurrentTime;

	if (XSendEvent (disp, winInfo->window, False, NoEventMask,
	    (XEvent *) &saveYourselfMessage))
	{
	    winInfo->waiting_for_update = 1;

	    if (debug)
	    {
		printf ("Sent SAVE YOURSELF to 0x%x\n", 
			(unsigned int)winInfo->window);    
		printf ("\n");
	    }
	}
	else
	{
	    if (debug)
	    {
		printf ("Failed to send SAVE YOURSELF to 0x%x\n",
		    (unsigned int)winInfo->window);    
		printf ("\n");
	    }
	}
    }
}



static void
DieCB(SmcConn smcConn, SmPointer clientData)
{
    WinInfo *winInfo = (WinInfo *) clientData;

    SmcCloseConnection (winInfo->smc_conn, 0, NULL);
    winInfo->smc_conn = NULL;
    XtRemoveInput (winInfo->input_id);

    /* Now tell the client to die */

    if (debug)
	printf ("Trying to kill 0x%x\n", (unsigned int)winInfo->window);

    XSync (disp, 0);
    XKillClient (disp, winInfo->window);
    XSync (disp, 0);


    /*
     * Proxy must exit when all clients die, and the proxy itself
     * must have received a Die.
     */

    die_count++;

    if (die_count == proxy_count && ok_to_die)
    {
	exit (0);
    }
}



static void
SaveCompleteCB(SmcConn smcConn, SmPointer clientData)
{
    /*
     * Nothing to do here.
     */
}



static void
ShutdownCancelledCB(SmcConn smcConn, SmPointer clientData)
{
    /*
     * Since we did not request to interact or request save yourself
     * phase 2, we know we already sent the save yourself done, so
     * there is nothing to do here.
     */
}



static void
ProcessIceMsgProc(XtPointer client_data, int *source, XtInputId *id)
{
    IceConn	ice_conn = (IceConn) client_data;

    IceProcessMessages (ice_conn, NULL, NULL);
}



static void
NullIceErrorHandler(IceConn iceConn, Bool swap, int offendingMinorOpcode,
		    unsigned long offendingSequence, int errorClass,
		    int severity, IcePointer values)
{
    return;
}


static void
ConnectClientToSM(WinInfo *winInfo)
{
    char errorMsg[256];
    unsigned long mask;
    SmcCallbacks callbacks;
    IceConn ice_conn;
    char *prevId;

    mask = SmcSaveYourselfProcMask | SmcDieProcMask |
	SmcSaveCompleteProcMask | SmcShutdownCancelledProcMask;

    callbacks.save_yourself.callback = SaveYourselfCB;
    callbacks.save_yourself.client_data = (SmPointer) winInfo;

    callbacks.die.callback = DieCB;
    callbacks.die.client_data = (SmPointer) winInfo;

    callbacks.save_complete.callback = SaveCompleteCB;
    callbacks.save_complete.client_data = (SmPointer) winInfo;

    callbacks.shutdown_cancelled.callback = ShutdownCancelledCB;
    callbacks.shutdown_cancelled.client_data = (SmPointer) winInfo;

    prevId = LookupClientID (winInfo);

    /*
     * In case a protocol error occurs when opening the connection,
     * (e.g. an authentication error), we set a null error handler
     * before the open, then restore the default handler after the open.
     */

    IceSetErrorHandler (NullIceErrorHandler);

    winInfo->smc_conn = SmcOpenConnection (
	NULL, 			/* use SESSION_MANAGER env */
	(SmPointer) winInfo,	/* force a new connection */
	SmProtoMajor,
	SmProtoMinor,
	mask,
	&callbacks,
	prevId,
	&winInfo->client_id,
	256, errorMsg);

    IceSetErrorHandler (NULL);

    if (winInfo->smc_conn == NULL)
	return;

    ice_conn = SmcGetIceConnection (winInfo->smc_conn);

    winInfo->input_id = XtAppAddInput (
	    appContext,
	    IceConnectionNumber (ice_conn),
            (XtPointer) XtInputReadMask,
	    ProcessIceMsgProc,
	    (XtPointer) ice_conn);

    if (debug)
    {
	printf ("Connected to SM, window = 0x%x\n", 
		(unsigned int)winInfo->window);
	printf ("\n");
    }

    proxy_count++;
}



static int
MyErrorHandler(Display *display, XErrorEvent *event)
{
    caught_error = 1;
    return 0;
}



static Bool
LookupWindow(Window window, WinInfo **ptr_ret, WinInfo **prev_ptr_ret)
{
    WinInfo *ptr, *prev;

    ptr = win_head;
    prev = NULL;

    while (ptr)
    {
	if (ptr->window == window)
	    break;
	else
	{
	    prev = ptr;
	    ptr = ptr->next;
	}
    }

    if (ptr)
    {
	if (ptr_ret)
	    *ptr_ret = ptr;
	if (prev_ptr_ret)
	    *prev_ptr_ret = prev;
	return (1);
    }
    else
	return (0);
}



static WinInfo *
AddNewWindow(Window window)
{
    WinInfo *newptr;

    if (LookupWindow (window, NULL, NULL))
	return (NULL);

    newptr = (WinInfo *) malloc (sizeof (WinInfo));

    if (newptr == NULL)
	return (NULL);

    newptr->next = win_head;
    win_head = newptr;

    newptr->window = window;
    newptr->smc_conn = NULL;
    newptr->tested_for_sm_client_id = 0;
    newptr->client_id = NULL;
    newptr->wm_command = NULL;
    newptr->wm_command_count = 0;
    newptr->class.res_name = NULL;
    newptr->class.res_class = NULL;
    newptr->wm_name = NULL;
    newptr->wm_client_machine.value = NULL;
    newptr->wm_client_machine.nitems = 0;
    newptr->has_save_yourself = 0;
    newptr->waiting_for_update = 0;
    newptr->got_first_save_yourself = 0;

    return (newptr);
}



static void
RemoveWindow(WinInfo *winptr)
{
    WinInfo *ptr, *prev;

    if (LookupWindow (winptr->window, &ptr, &prev))
    {
	if (prev == NULL)
	    win_head = ptr->next;
	else
	    prev->next = ptr->next;

	if (ptr->client_id)
	    free (ptr->client_id);
	
	if (ptr->wm_command)
	    XFreeStringList (ptr->wm_command);
	
	if (ptr->wm_name)
	    XFree (ptr->wm_name);
	
	if (ptr->wm_client_machine.value)
	    XFree (ptr->wm_client_machine.value);
	
	if (ptr->class.res_name)
	    XFree (ptr->class.res_name);
	
	if (ptr->class.res_class)
	    XFree (ptr->class.res_class);
	
	free ((char *) ptr);
    }
}



static void
Got_WM_STATE(WinInfo *winptr)
{
    WinInfo *leader_winptr;

    /*
     * If we already got WM_STATE and tested for SM_CLIENT_ID, we
     * shouldn't do it again.
     */

    if (winptr->tested_for_sm_client_id)
    {
	return;
    }


    /*
     * Set a null error handler, in case this window goes away
     * behind our back.
     */

    caught_error = 0;
    XSetErrorHandler (MyErrorHandler);


    /*
     * Get the client leader window.
     */

    leader_winptr = GetClientLeader (winptr);

    if (caught_error)
    {
	caught_error = 0;
	RemoveWindow (winptr);
	XSetErrorHandler (NULL);
	return;
    }


    /*
     * If we already checked for SM_CLIENT_ID on the client leader
     * window, don't do it again.
     */

    if (!leader_winptr || leader_winptr->tested_for_sm_client_id)
    {
	caught_error = 0;
	XSetErrorHandler (NULL);
	return;
    }

    leader_winptr->tested_for_sm_client_id = 1;

    if (!HasXSMPsupport (leader_winptr->window))
    {
	XFetchName (disp, leader_winptr->window, &leader_winptr->wm_name);

	XGetCommand (disp, leader_winptr->window,
	    &leader_winptr->wm_command,
	    &leader_winptr->wm_command_count);

	XGetClassHint (disp, leader_winptr->window, &leader_winptr->class);

	XGetWMClientMachine (disp, leader_winptr->window,
	    &leader_winptr->wm_client_machine);

	if (leader_winptr->wm_name != NULL &&
	    leader_winptr->wm_command != NULL &&
	    leader_winptr->wm_command_count > 0 &&
	    leader_winptr->class.res_name != NULL &&
	    leader_winptr->class.res_class != NULL &&
	    leader_winptr->wm_client_machine.value != NULL &&
	    leader_winptr->wm_client_machine.nitems != 0)
	{
	    leader_winptr->has_save_yourself =
		HasSaveYourself (leader_winptr->window);

	    ConnectClientToSM (leader_winptr);
	}
    }

    XSync (disp, 0);
    XSetErrorHandler (NULL);

    if (caught_error)
    {
	caught_error = 0;
	RemoveWindow (leader_winptr);
    }
}



static void
HandleCreate(XCreateWindowEvent *event)
{
    Atom actual_type;
    int actual_format;
    unsigned long nitems, bytesafter;
    unsigned long *datap = NULL;
    WinInfo *winptr;
    Bool got_wm_state = 0;

    /*
     * We are waiting for all proxy connections to close so we can die.
     * Don't handle new connections.
     */

    if (ok_to_die)
	return;


    /*
     * Add the new window
     */

    if ((winptr = AddNewWindow (event->window)) == NULL)
	return;


    /*
     * Right after the window was created, it might have been destroyed,
     * so the following Xlib calls might fail.  Need to catch the error
     * by installing an error handler.
     */

    caught_error = 0;
    XSetErrorHandler (MyErrorHandler);


    /*
     * Select for Property Notify on the window so we can determine
     * when WM_STATE is defined.  To avoid a race condition, we must
     * do this _before_ we check for WM_STATE right now.
     *
     * Select for Substructure Notify so we can determine when the
     * window is destroyed.
     */

    XSelectInput (disp, event->window,
	SubstructureNotifyMask | PropertyChangeMask);	


    /*
     * WM_STATE may already be there.  Check now.
     */

    if (XGetWindowProperty (disp, event->window, wmStateAtom,
	0L, 2L, False, AnyPropertyType,
	&actual_type, &actual_format, &nitems, &bytesafter,
	(unsigned char **) &datap) == Success && datap)
    {
	if (nitems > 0)
	    got_wm_state = 1;

	if (datap)
	    XFree ((char *) datap);
    }

    XSync (disp, 0);
    XSetErrorHandler (NULL);

    if (caught_error)
    {
	caught_error = 0;
	RemoveWindow (winptr);
    }
    else if (got_wm_state)
    {
	Got_WM_STATE (winptr);
    }
}



static void
HandleDestroy(XDestroyWindowEvent *event)
{
    WinInfo *winptr;

    if (LookupWindow (event->window, &winptr, NULL))
    {
	if (winptr->smc_conn)
	{
	    SmcCloseConnection (winptr->smc_conn, 0, NULL);
	    XtRemoveInput (winptr->input_id);
	    proxy_count--;
	}

	if (debug)
	{
	    printf ("Removed window (window = 0x%x)\n", 
		    (unsigned int)winptr->window);
	    printf ("\n");
	}

	RemoveWindow (winptr);
    }
}



static void
HandleUpdate(XPropertyEvent *event)
{
    Window window = event->window;
    WinInfo *winptr;

    if (!LookupWindow (window, &winptr, NULL))
	return;

    if (event->atom == wmStateAtom)
    {
	Got_WM_STATE (winptr);
    }
    else if (event->atom == XA_WM_COMMAND && winptr->waiting_for_update)
    {
	/* Finish off the Save Yourself */

	if (winptr->wm_command)
	{
	    XFreeStringList (winptr->wm_command);
	    winptr->wm_command = NULL;
	    winptr->wm_command_count = 0;
	}

	XGetCommand (disp, window,
	    &winptr->wm_command,
	    &winptr->wm_command_count);

	winptr->waiting_for_update = 0;
	FinishSaveYourself (winptr, True);
    }
}



static void
ProxySaveYourselfPhase2CB(SmcConn smcConn, SmPointer clientData)
{
    char *filename;
    Bool success = True;
    SmProp prop1, prop2, prop3, *props[3];
    SmPropValue prop1val, prop2val, prop3val;
    char discardCommand[80];
    int numVals, i;
    static int first_time = 1;

    if (first_time)
    {
	char userId[20];
	char hint = SmRestartIfRunning;

	prop1.name = SmProgram;
	prop1.type = SmARRAY8;
	prop1.num_vals = 1;
	prop1.vals = &prop1val;
	prop1val.value = Argv[0];
	prop1val.length = strlen (Argv[0]);

	sprintf (userId, "%ld", (long)getuid());
	prop2.name = SmUserID;
	prop2.type = SmARRAY8;
	prop2.num_vals = 1;
	prop2.vals = &prop2val;
	prop2val.value = (SmPointer) userId;
	prop2val.length = strlen (userId);
	
	prop3.name = SmRestartStyleHint;
	prop3.type = SmCARD8;
	prop3.num_vals = 1;
	prop3.vals = &prop3val;
	prop3val.value = (SmPointer) &hint;
	prop3val.length = 1;
	
	props[0] = &prop1;
	props[1] = &prop2;
	props[2] = &prop3;

	SmcSetProperties (smcConn, 3, props);

	first_time = 0;
    }

    if ((filename = WriteProxyFile ()) == NULL)
    {
	success = False;
	goto finishUp;
    }

    prop1.name = SmRestartCommand;
    prop1.type = SmLISTofARRAY8;

    prop1.vals = (SmPropValue *) malloc (
	(Argc + 4) * sizeof (SmPropValue));

    if (!prop1.vals)
    {
	success = False;
	goto finishUp;
    }

    numVals = 0;

    for (i = 0; i < Argc; i++)
    {
	if (strcmp (Argv[i], "-clientId") == 0 ||
	    strcmp (Argv[i], "-restore") == 0)
	{
	    i++;
	}
	else
	{
	    prop1.vals[numVals].value = (SmPointer) Argv[i];
	    prop1.vals[numVals++].length = strlen (Argv[i]);
	}
    }

    prop1.vals[numVals].value = (SmPointer) "-clientId";
    prop1.vals[numVals++].length = 9;

    prop1.vals[numVals].value = (SmPointer) proxy_clientId;
    prop1.vals[numVals++].length = strlen (proxy_clientId);

    prop1.vals[numVals].value = (SmPointer) "-restore";
    prop1.vals[numVals++].length = 8;

    prop1.vals[numVals].value = (SmPointer) filename;
    prop1.vals[numVals++].length = strlen (filename);

    prop1.num_vals = numVals;


    sprintf (discardCommand, "rm %s", filename);
    prop2.name = SmDiscardCommand;
    prop2.type = SmARRAY8;
    prop2.num_vals = 1;
    prop2.vals = &prop2val;
    prop2val.value = (SmPointer) discardCommand;
    prop2val.length = strlen (discardCommand);

    props[0] = &prop1;
    props[1] = &prop2;

    SmcSetProperties (smcConn, 2, props);
    free ((char *) prop1.vals);

 finishUp:

    SmcSaveYourselfDone (smcConn, success);
    sent_save_done = 1;

    if (filename)
	free (filename);
}



static void
ProxySaveYourselfCB(SmcConn smcConn, SmPointer clientData, int saveType,
		    Bool shutdown, int interactStyle, Bool fast)
{
    /*
     * We want the proxy to respond to the Save Yourself after all
     * the regular XSMP clients have finished with the save (and possibly
     * interacted with the user).
     */

    if (!SmcRequestSaveYourselfPhase2 (smcConn,
	ProxySaveYourselfPhase2CB, NULL))
    {
	SmcSaveYourselfDone (smcConn, False);
	sent_save_done = 1;
    }
    else
	sent_save_done = 0;
}



static void
ProxyDieCB(SmcConn smcConn, SmPointer clientData)
{
    SmcCloseConnection (proxy_smcConn, 0, NULL);
    XtRemoveInput (proxy_iceInputId);

    if (die_count == proxy_count)
	exit (0);
    else
	ok_to_die = 1;
}



static void
ProxySaveCompleteCB(SmcConn smcConn, SmPointer clientData)
{
    ;
}



static void
ProxyShutdownCancelledCB(SmcConn smcConn, SmPointer clientData)
{
    if (!sent_save_done)
    {
	SmcSaveYourselfDone (smcConn, False);
	sent_save_done = 1;
    }
}



static Status
ConnectProxyToSM(char *previous_id)
{
    char errorMsg[256];
    unsigned long mask;
    SmcCallbacks callbacks;
    IceConn iceConn;

    mask = SmcSaveYourselfProcMask | SmcDieProcMask |
	SmcSaveCompleteProcMask | SmcShutdownCancelledProcMask;

    callbacks.save_yourself.callback = ProxySaveYourselfCB;
    callbacks.save_yourself.client_data = (SmPointer) NULL;

    callbacks.die.callback = ProxyDieCB;
    callbacks.die.client_data = (SmPointer) NULL;

    callbacks.save_complete.callback = ProxySaveCompleteCB;
    callbacks.save_complete.client_data = (SmPointer) NULL;

    callbacks.shutdown_cancelled.callback = ProxyShutdownCancelledCB;
    callbacks.shutdown_cancelled.client_data = (SmPointer) NULL;

    proxy_smcConn = SmcOpenConnection (
	NULL, 			/* use SESSION_MANAGER env */
	(SmPointer) appContext,
	SmProtoMajor,
	SmProtoMinor,
	mask,
	&callbacks,
	previous_id,
	&proxy_clientId,
	256, errorMsg);

    if (proxy_smcConn == NULL)
	return (0);

    iceConn = SmcGetIceConnection (proxy_smcConn);

    proxy_iceInputId = XtAppAddInput (
	    appContext,
	    IceConnectionNumber (iceConn),
            (XtPointer) XtInputReadMask,
	    ProcessIceMsgProc,
	    (XtPointer) iceConn);

    return (1);
}



static void
CheckForExistingWindows(Window root)
{
    Window dontCare1, dontCare2, *children, client_window;
    unsigned int nchildren, i;
    XCreateWindowEvent event;

    /*
     * We query the root tree for all windows created thus far.
     * Note that at any moment after XQueryTree is called, a
     * window may be deleted.  So we must take extra care to make
     * sure a window really exists.
     */

    XQueryTree (disp, root, &dontCare1, &dontCare2, &children, &nchildren);

    for (i = 0; i < nchildren; i++)
    {
	event.window = children[i];

	HandleCreate (&event);

	caught_error = 0;
	XSetErrorHandler (MyErrorHandler);

	client_window = XmuClientWindow (disp, children[i]);

	XSetErrorHandler (NULL);

	if (!caught_error && client_window != children[i])
	{
	    event.window = client_window;
	    HandleCreate (&event);
	}
    }
}



int
main (int argc, char *argv[])
{
    char *restore_filename = NULL;
    char *client_id = NULL;
    int i, zero = 0;

    Argc = argc;
    Argv = argv;

    for (i = 1; i < argc; i++)
    {
	if (argv[i][0] == '-')
	{
	    switch (argv[i][1])
	    {
	      case 'd':				/* -debug */
		debug = 1;
		continue;

	      case 'c':				/* -clientId */
		if (++i >= argc) goto usage;
		client_id = argv[i];
		continue;

	      case 'r':				/* -restore */
		if (++i >= argc) goto usage;
		restore_filename = argv[i];
		continue;
	    }
	}

    usage:

	fprintf (stderr,
	    "usage:  %s [-clientId id] [-restore file] [-debug]\n", argv[0]);
	exit (1);
    }


    XtToolkitInitialize ();
    appContext = XtCreateApplicationContext ();

    if (!(disp = XtOpenDisplay (appContext, NULL, "SM-PROXY", "SM-PROXY",
	NULL, 0, &zero, NULL)))
    {
	fprintf (stderr, "smproxy: unable to open display\n");
	exit (1);
    }

    if (restore_filename)
	ReadProxyFile (restore_filename);

    if (!ConnectProxyToSM (client_id))
    {
	fprintf (stderr, "smproxy: unable to connect to session manager\n");
	exit (1);
    }

    wmProtocolsAtom = XInternAtom (disp, "WM_PROTOCOLS", False);
    wmSaveYourselfAtom = XInternAtom (disp, "WM_SAVE_YOURSELF", False);
    wmStateAtom = XInternAtom (disp, "WM_STATE", False);
    smClientIdAtom = XInternAtom (disp, "SM_CLIENT_ID", False);
    wmClientLeaderAtom = XInternAtom (disp, "WM_CLIENT_LEADER", False);

    for (i = 0; i < ScreenCount (disp); i++)
    {
	Window root = RootWindow (disp, i);
	XSelectInput (disp, root, SubstructureNotifyMask | PropertyChangeMask);
	CheckForExistingWindows (root);
    }

    while (1)
    {
	XEvent event;

	XtAppNextEvent (appContext, &event);

	switch (event.type)
	{
	case CreateNotify:
	    HandleCreate (&event.xcreatewindow);
	    break;

	case DestroyNotify:
	    HandleDestroy (&event.xdestroywindow);
	    break;

	case PropertyNotify:
	    HandleUpdate (&event.xproperty);
	    break;

	default:
	    XtDispatchEvent (&event);
	    break;
	}
    }
    exit(0);
}

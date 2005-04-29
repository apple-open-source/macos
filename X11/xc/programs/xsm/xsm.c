/* $Xorg: xsm.c,v 1.7 2001/02/09 02:06:01 xorgcvs Exp $ */
/******************************************************************************

Copyright 1993, 1998  The Open Group

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
******************************************************************************/
/* $XFree86: xc/programs/xsm/xsm.c,v 1.10 2003/09/13 21:33:12 dawes Exp $ */

/*
 * X Session Manager.
 *
 * Authors:
 *	Ralph Mor, X Consortium
 *      Jordan Brown, Quarterdeck Office Systems
 */

#include "xsm.h"
#include "globals.c"
#include "xtwatch.h"
#include "prop.h"
#include "choose.h"
#include "mainwin.h"
#include "info.h"
#include "log.h"
#include "save.h"
#include "auth.h"
#include "restart.h"
#include "saveutil.h"
#include "lock.h"

#include <X11/Shell.h>
#include <X11/Xatom.h>
#include <X11/Xaw/List.h>

Atom wmStateAtom;
Atom wmDeleteAtom;
static char *cmd_line_display = NULL;

/*
 * Forward declarations
 */
static void PropertyChangeXtHandler(Widget w, XtPointer closure, 
				    XEvent *event, 
				    Boolean *continue_to_dispatch);
static void GetEnvironment(void);
static Status RegisterClientProc(SmsConn smsConn, SmPointer managerData, 
				 char *previousId);
static Bool OkToEnterInteractPhase(void);
static void InteractRequestProc(SmsConn smsConn, SmPointer managerData, 
				int dialogType);
static void InteractDoneProc(SmsConn smsConn, SmPointer managerData, 
			     Bool cancelShutdown);
static void SaveYourselfReqProc(SmsConn smsConn, SmPointer managerData, 
				int saveType, Bool shutdown, 
				int interactStyle, Bool fast, Bool global);
static Bool OkToEnterPhase2(void);
static void SaveYourselfPhase2ReqProc(SmsConn smsConn, SmPointer managerData);
static void SaveYourselfDoneProc(SmsConn smsConn, SmPointer managerData, 
				 Bool success);
static void CloseConnectionProc(SmsConn smsConn, SmPointer managerData, 
				int count, char **reasonMsgs);
static Status NewClientProc(SmsConn smsConn, SmPointer managerData, 
			    unsigned long *maskRet, 
			    SmsCallbacks *callbacksRet, 
			    char **failureReasonRet);
static void NewConnectionXtProc(XtPointer client_data, int *source, 
				XtInputId *id);
static void MyIoErrorHandler(IceConn ice_conn);
static void InstallIOErrorHandler(void);
static void CloseListeners(void);

/*
 * Extern declarations
 */

extern Widget clientInfoPopup;
extern Widget clientPropPopup;
extern Widget clientInfoButton;
extern Widget logButton;
extern Widget checkPointButton;
extern Widget shutdownButton;
extern Widget clientListWidget;
extern Widget savePopup;

extern int checkpoint_from_signal;

static IceListenObj *listenObjs;


/*
 * Main program
 */
int
main(int argc, char *argv[])
{
    char	*p;
    char 	errormsg[256];
    static	char environment_name[] = "SESSION_MANAGER";
    int		success, found_command_line_name, i;

    Argc = argc;
    Argv = argv;

    for (i = 1; i < argc; i++)
    {
	if (argv[i][0] == '-')
	{
	    switch (argv[i][1])
	    {
	    case 'd':					/* -display */
		if (++i >= argc) goto usage;
		cmd_line_display = (char *) XtNewString (argv[i]);
		continue;

	    case 's':					/* -session */
		if (++i >= argc) goto usage;
		session_name = XtNewString (argv[i]);
		continue;

	    case 'v':					/* -verbose */
		verbose = 1;
		continue;
	    }
	}

    usage:
	fprintf (stderr,
	 "usage: xsm [-display display] [-session session_name] [-verbose]\n");
	exit (1);
    }

    topLevel = XtVaAppInitialize (&appContext, "XSm", NULL, 0,
	&argc, argv, NULL,
	XtNmappedWhenManaged, False,
	XtNwindowRole, "xsm main window",
	NULL);
	
    wmStateAtom = XInternAtom (
	XtDisplay (topLevel), "WM_STATE", False);
    wmDeleteAtom = XInternAtom (
	XtDisplay (topLevel), "WM_DELETE_WINDOW", False);

    register_signals (appContext);


    /*
     * Install an IO error handler.  For an explanation,
     * see the comments for InstallIOErrorHandler().
     */

    InstallIOErrorHandler ();


    /*
     * Init SM lib
     */

    if (!SmsInitialize ("SAMPLE-SM", "1.0",
	NewClientProc, NULL,
	HostBasedAuthProc, 256, errormsg))
    {
	fprintf (stderr, "%s\n", errormsg);
	exit (1);
    }

    if (!IceListenForConnections (&numTransports, &listenObjs,
	256, errormsg))
    {
	fprintf (stderr, "%s\n", errormsg);
	exit (1);
    }

    atexit(CloseListeners);

    if (!SetAuthentication (numTransports, listenObjs, &authDataEntries))
    {
	fprintf (stderr, "Could not set authorization\n");
	exit (1);
    }

    InitWatchProcs (appContext);

    for (i = 0; i < numTransports; i++)
    {
	XtAppAddInput (appContext,
	    IceGetListenConnectionNumber (listenObjs[i]),
	    (XtPointer) XtInputReadMask,
	    NewConnectionXtProc, (XtPointer) listenObjs[i]);
    }

    /* the sizeof includes the \0, so we don't need to count the '=' */
    networkIds = IceComposeNetworkIdList (numTransports, listenObjs);
    p = (char *) XtMalloc((sizeof environment_name) + strlen(networkIds) + 1);
    if(!p) nomem();
    sprintf(p, "%s=%s", environment_name, networkIds);
    putenv(p);

    if (cmd_line_display)
    {
	/*
	 * If a display was passed on the command line, set the DISPLAY
	 * environment in this process so all applications started by
	 * the session manager will run on the specified display.
	 */

	p = (char *) XtMalloc(8 + strlen(cmd_line_display) + 1);
	sprintf(p, "DISPLAY=%s", cmd_line_display);
	putenv(p);
    }

    if (verbose)
	printf ("setenv %s %s\n", environment_name, networkIds);

    create_choose_session_popup ();
    create_main_window ();
    create_client_info_popup ();
    create_save_popup ();
    create_log_popup ();


    /*
     * Initalize all lists
     */

    RunningList = ListInit();
    if(!RunningList) nomem();

    PendingList = ListInit();
    if(!PendingList) nomem();

    RestartAnywayList = ListInit();
    if(!RestartAnywayList) nomem();

    RestartImmedList = ListInit();
    if(!RestartImmedList) nomem();

    WaitForSaveDoneList = ListInit();
    if (!WaitForSaveDoneList) nomem();

    InitialSaveList = ListInit();
    if (!InitialSaveList) nomem();

    FailedSaveList = ListInit();
    if (!FailedSaveList) nomem();

    WaitForInteractList = ListInit();
    if (!WaitForInteractList) nomem();

    WaitForPhase2List = ListInit();
    if (!WaitForPhase2List) nomem();


    /*
     * Get list of session names.  If a session name was found on the
     * command line, and it is in the list of session names we got, then
     * use that session name.  If there were no session names found, then
     * use the default session name.  Otherwise, present a list of session
     * names for the user to choose from.
     */

    success = GetSessionNames (&sessionNameCount,
	&sessionNamesShort, &sessionNamesLong, &sessionsLocked);

    found_command_line_name = 0;
    if (success && session_name)
    {
	for (i = 0; i < sessionNameCount; i++)
	    if (strcmp (session_name, sessionNamesShort[i]) == 0)
	    {
		found_command_line_name = 1;

		if (sessionsLocked[i])
		{
		    fprintf (stderr, "Session '%s' is locked\n", session_name);
		    exit (1);
		}

		break;
	    }
    }

    if (!success || found_command_line_name)
    {
	FreeSessionNames (sessionNameCount,
	    sessionNamesShort, sessionNamesLong, sessionsLocked);

	if (!found_command_line_name)
	    session_name = XtNewString (DEFAULT_SESSION_NAME);

    	if (!StartSession (session_name, !found_command_line_name))
	    UnableToLockSession (session_name);
    }
    else
    {
	ChooseSession ();
    }
    

    /*
     * Main loop
     */

    XtAppMainLoop (appContext);
    exit(0);
}



static void
PropertyChangeXtHandler(Widget w, XtPointer closure, XEvent *event, 
			Boolean *continue_to_dispatch)
{
    if (w == topLevel && event->type == PropertyNotify &&
	event->xproperty.atom == wmStateAtom)
    {
	XtRemoveEventHandler (topLevel, PropertyChangeMask, False,
	    PropertyChangeXtHandler, NULL);

	/*
	 * Restart the rest of the session aware clients.
	 */

	Restart (RESTART_REST_OF_CLIENTS);


	/*
	 * Start apps that aren't session aware that were specified
	 * by the user.
	 */

	StartNonSessionAwareApps ();
    }
}



void
SetWM_DELETE_WINDOW(Widget widget, String delAction)
{
    char translation[64];

    sprintf (translation, "<Message>WM_PROTOCOLS: %s", delAction);
    XtOverrideTranslations (widget, XtParseTranslationTable (translation));

    XSetWMProtocols (XtDisplay(widget), XtWindow (widget),
	&wmDeleteAtom, 1);
}



static void
GetEnvironment(void)
{
    static char	envDISPLAY[]="DISPLAY";
    static char	envSESSION_MANAGER[]="SESSION_MANAGER";
    static char	envAUDIOSERVER[]="AUDIOSERVER";
    char	*p, *temp;

    remote_allowed = 1;

    display_env = NULL;
    if((p = cmd_line_display) || (p = (char *) getenv(envDISPLAY))) {
	display_env = (char *) XtMalloc(strlen(envDISPLAY)+1+strlen(p)+1);
	if(!display_env) nomem();
	sprintf(display_env, "%s=%s", envDISPLAY, p);

	/*
	 * When we restart a remote client, we have to make sure the
	 * display environment we give it has the SM's hostname.
	 */

	if ((temp = strchr (p, '/')) == 0)
	    temp = p;
	else
	    temp++;

	if (*temp != ':')
	{
	    /* we have a host name */

	    non_local_display_env = (char *) XtMalloc (
		strlen (display_env) + 1);
	    if (!non_local_display_env) nomem();

	    strcpy (non_local_display_env, display_env);
	}
	else
	{
	    char hostnamebuf[256];

	    gethostname (hostnamebuf, sizeof hostnamebuf);
	    non_local_display_env = (char *) XtMalloc (
		strlen (envDISPLAY) + 1 +
		strlen (hostnamebuf) + strlen (temp) + 1);
	    if (!non_local_display_env) nomem();
	    sprintf(non_local_display_env, "%s=%s%s",
		envDISPLAY, hostnamebuf, temp);
	}
    }

    session_env = NULL;
    if((p = (char *) getenv(envSESSION_MANAGER))) {
	session_env = (char *) XtMalloc(
	    strlen(envSESSION_MANAGER)+1+strlen(p)+1);
	if(!session_env) nomem();
	sprintf(session_env, "%s=%s", envSESSION_MANAGER, p);

	/*
	 * When we restart a remote client, we have to make sure the
	 * session environment does not have the SM's local connection port.
	 */

	non_local_session_env = (char *) XtMalloc (strlen (session_env) + 1);
	if (!non_local_session_env) nomem();
	strcpy (non_local_session_env, session_env);

	if ((temp = Strstr (non_local_session_env, "local/")) != NULL)
	{
	    char *delim = strchr (temp, ',');
	    if (delim == NULL)
	    {
		if (temp == non_local_session_env +
		    strlen (envSESSION_MANAGER) + 1)
		{
		    *temp = '\0';
		    remote_allowed = 0;
		}
		else
		    *(temp - 1) = '\0';
	    }
	    else
	    {
		int bytes = strlen (delim + 1);
		memmove (temp, delim + 1, bytes);
		*(temp + bytes) = '\0';
	    }
	}
    }

    audio_env = NULL;
    if((p = (char *) getenv(envAUDIOSERVER))) {
	audio_env = (char *) XtMalloc(strlen(envAUDIOSERVER)+1+strlen(p)+1);
	if(!audio_env) nomem();
	sprintf(audio_env, "%s=%s", envAUDIOSERVER, p);
    }
}



Status
StartSession(char *name, Bool use_default)
{
    int database_read = 0;
    Dimension width;
    char title[256];


    /*
     * If we're not using the default session, lock it.
     * If using the default session, it will be locked as
     * soon as the user assigns the session a name.
     */

    if (!use_default && !LockSession (name, True))
	return (0);


    /*
     * Get important environment variables.
     */

    GetEnvironment ();


    /*
     * Set the main window's title to the session name.
     */

    sprintf (title, "xsm: %s", name);

    XtVaSetValues (topLevel,
	XtNtitle, title,		/* session name */
	NULL);

    XtRealizeWidget (topLevel);


    /*
     * Set WM_DELETE_WINDOW support on main window.  If the user tries
     * to delete the main window, the shutdown prompt will come up.
     */

    SetWM_DELETE_WINDOW (topLevel, "DelMainWinAction()");


    /*
     * Read the session save file.  Make sure the session manager
     * has an SM_CLIENT_ID, so that other managers (like the WM) can
     * identify it.
     */

    set_session_save_file_name (name);

    if (use_default)
	need_to_name_session = True;
    else
    {
	database_read = ReadSave (name, &sm_id);
	need_to_name_session = !database_read;
    }

    if (!sm_id)
    {
	sm_id = SmsGenerateClientID (NULL);
	if (!sm_id) return (0);
    }
    XChangeProperty (XtDisplay (topLevel), XtWindow (topLevel),
	XInternAtom (XtDisplay (topLevel), "SM_CLIENT_ID", False),
	XA_STRING, 8, PropModeReplace,
	(unsigned char *) sm_id, strlen (sm_id));


    /*
     * Adjust some label widths
     */

    XtVaGetValues (clientInfoButton,
	XtNwidth, &width,
	NULL);

    XtVaSetValues (checkPointButton,
	XtNwidth, width,
	NULL);

    XtVaGetValues (logButton,
	XtNwidth, &width,
	NULL);

    XtVaSetValues (shutdownButton,
	XtNwidth, width,
	NULL);
    

    XtMapWidget (topLevel);


    if (!database_read)
    {
	/*
	 * Start default apps (e.g. twm, smproxy)
	 */

	StartDefaultApps ();
    }
    else
    {
	/*
	 * Restart window manager first.  When the session manager
	 * gets a WM_STATE stored on its top level window, we know
	 * the window manager is running.  At that time, we can start
	 * the rest of the applications.
	 */

	XtAddEventHandler (topLevel, PropertyChangeMask, False,
	    PropertyChangeXtHandler, NULL);

	if (!Restart (RESTART_MANAGERS))
	{
	    XtRemoveEventHandler (topLevel, PropertyChangeMask, False,
	        PropertyChangeXtHandler, NULL);

	    /*
	     * Restart the rest of the session aware clients.
	     */

	    Restart (RESTART_REST_OF_CLIENTS);

	    /*
	     * Start apps that aren't session aware that were specified
	     * by the user.
	     */
	    
	    StartNonSessionAwareApps ();
	}
    }

    return (1);
}



void
EndSession(int status)
{
    if (verbose)
	printf ("\nSESSION MANAGER GOING AWAY!\n");

    FreeAuthenticationData (numTransports, authDataEntries);

    if (session_name)
    {
	UnlockSession (session_name);
	XtFree (session_name);
    }

    if (display_env)
	XtFree (display_env);
    if (session_env)
	XtFree (session_env);
    if (cmd_line_display)
	XtFree (cmd_line_display);
    if (non_local_display_env)
	XtFree (non_local_display_env);
    if (non_local_session_env)
	XtFree (non_local_session_env);
    if (audio_env)
	XtFree (audio_env);
    if (networkIds)
	free (networkIds);

    exit (status);
}



void
FreeClient(ClientRec *client, Bool freeProps)
{
    if (freeProps)
    {
	List *pl;

	for (pl = ListFirst (client->props); pl; pl = ListNext (pl))
	    FreeProp ((Prop *) pl->thing);

	ListFreeAll (client->props);
    }

    if (client->clientId)
	free (client->clientId);		/* malloc'd by SMlib */
    if (client->clientHostname)
	free (client->clientHostname);		/* malloc'd by SMlib */

    if (client->discardCommand)
	XtFree (client->discardCommand);
    if (client->saveDiscardCommand)
	XtFree (client->saveDiscardCommand);

    XtFree ((char *) client);
}



/*
 * Session Manager callbacks
 */

static Status
RegisterClientProc(SmsConn smsConn, SmPointer managerData, char *previousId)
{
    ClientRec	*client = (ClientRec *) managerData;
    char 	*id;
    List	*cl;
    int		send_save;

    if (verbose)
    {
	printf (
	"On IceConn fd = %d, received REGISTER CLIENT [Previous Id = %s]\n",
	IceConnectionNumber (client->ice_conn),
	previousId ? previousId : "NULL");
	printf ("\n");
    }

    if (!previousId)
    {
	id = SmsGenerateClientID (smsConn);
	send_save = 1;
    }
    else
    {
	int found_match = 0;
	send_save = 1;

	for (cl = ListFirst (PendingList); cl; cl = ListNext (cl))
	{
	    PendingClient *pendClient = (PendingClient *) cl->thing;

	    if (!strcmp (pendClient->clientId, previousId))
	    {
		SetInitialProperties (client, pendClient->props);
		XtFree (pendClient->clientId);
		XtFree (pendClient->clientHostname);
		XtFree ((char *) pendClient);
		ListFreeOne (cl);
		found_match = 1;
		send_save = 0;
		break;
	    }
	}

	if (!found_match)
	{
	    for (cl = ListFirst (RestartAnywayList); cl; cl = ListNext (cl))
	    {
		ClientRec *rClient = (ClientRec *) cl->thing;

		if (!strcmp (rClient->clientId, previousId))
		{
		    SetInitialProperties (client, rClient->props);
		    FreeClient (rClient, False /* don't free props */);
		    ListFreeOne (cl);
		    found_match = 1;
		    send_save = 0;
		    break;
		}
	    }
	}

	if (!found_match)
	{
	    for (cl = ListFirst (RestartImmedList); cl; cl = ListNext (cl))
	    {
		ClientRec *rClient = (ClientRec *) cl->thing;

		if (!strcmp (rClient->clientId, previousId))
		{
		    SetInitialProperties (client, rClient->props);
		    FreeClient (rClient, False /* don't free props */);
		    ListFreeOne (cl);
		    found_match = 1;
		    send_save = 0;
		    break;
		}
	    }
	}

	if (!found_match)
	{
	    /*
	     * previous-id was bogus: return bad status and the client
	     * should re-register with a NULL previous-id
	     */

	    free (previousId);
	    return (0);
	}
	else
	{
	    id = previousId;
	}
    }

    SmsRegisterClientReply (smsConn, id);

    if (verbose)
    {
	printf (
	"On IceConn fd = %d, sent REGISTER CLIENT REPLY [Client Id = %s]\n",
	IceConnectionNumber (client->ice_conn), id);
	printf ("\n");
    }

    client->clientId = id;
    client->clientHostname = SmsClientHostName (smsConn);
    client->restarted = (previousId != NULL);

    if (send_save)
    {
	SmsSaveYourself (smsConn, SmSaveLocal,
	    False, SmInteractStyleNone, False);

	ListAddLast (InitialSaveList, (char *) client);
    }
    else if (client_info_visible)
    {
	/* We already have all required client info */

	UpdateClientList ();
	XawListHighlight (clientListWidget, current_client_selected);
    }

    return (1);
}



static Bool
OkToEnterInteractPhase(void)
{
    return ((ListCount (WaitForInteractList) +
	ListCount (WaitForPhase2List)) == ListCount (WaitForSaveDoneList));
}



static void
InteractRequestProc(SmsConn smsConn, SmPointer managerData, int dialogType)
{
    ClientRec	*client = (ClientRec *) managerData;

    if (verbose)
    {
	printf ("Client Id = %s, received INTERACT REQUEST [Dialog Type = ",
		client->clientId);
	if (dialogType == SmDialogError)
	    printf ("Error]\n");
	else if (dialogType == SmDialogNormal)
	    printf ("Normal]\n");
	else
	    printf ("Error in SMlib: should have checked for bad value]\n");
    }

    ListAddLast (WaitForInteractList, (char *) client);

    if (OkToEnterInteractPhase ())
    {
	LetClientInteract (ListFirst (WaitForInteractList));
    }
}



static void
InteractDoneProc(SmsConn smsConn, SmPointer managerData, Bool cancelShutdown)
{
    ClientRec	*client = (ClientRec *) managerData;
    List	*cl;

    if (verbose)
    {
	printf (
	"Client Id = %s, received INTERACT DONE [Cancel Shutdown = %s]\n",
	client->clientId, cancelShutdown ? "True" : "False");
    }

    if (cancelShutdown)
    {
	ListFreeAllButHead (WaitForInteractList);
	ListFreeAllButHead (WaitForPhase2List);
    }

    if (cancelShutdown)
    {
	if (shutdownCancelled)
	{
	    /* Shutdown was already cancelled */
	    return;
	}

	shutdownCancelled = True;

	for (cl = ListFirst (RunningList); cl; cl = ListNext (cl))
	{
	    client = (ClientRec *) cl->thing;

	    SmsShutdownCancelled (client->smsConn);

	    if (verbose) 
	    {
		printf ("Client Id = %s, sent SHUTDOWN CANCELLED\n",
			client->clientId);
	    }
	}
    }
    else
    {
	if ((cl = ListFirst (WaitForInteractList)) != NULL)
	{
	    LetClientInteract (cl);
	}
	else
	{
	    if (verbose)
	    {
		printf ("\n");
		printf ("Done interacting with all clients.\n");
		printf ("\n");
	    }

	    if (ListCount (WaitForPhase2List) > 0)
	    {
		StartPhase2 ();
	    }
	}
    }
}



static void
SaveYourselfReqProc(SmsConn smsConn, SmPointer managerData, int saveType,
    Bool shutdown, int interactStyle, Bool fast, Bool global)
{
    if (verbose) 
	printf("SAVE YOURSELF REQUEST not supported!\n");
}



static Bool
OkToEnterPhase2(void)

{
    return (ListCount (WaitForPhase2List) == ListCount (WaitForSaveDoneList));
}



static void
SaveYourselfPhase2ReqProc(SmsConn smsConn, SmPointer managerData)
{
    ClientRec	*client = (ClientRec *) managerData;

    if (verbose)
    {
	printf ("Client Id = %s, received SAVE YOURSELF PHASE 2 REQUEST\n",
	    client->clientId);
    }

    if (!saveInProgress)
    {
	/*
	 * If we are not in the middle of a checkpoint (ie. we just
	 * started the client and sent the initial save yourself), just
	 * send the save yourself phase2 now.
	 */
	 
	SmsSaveYourselfPhase2 (client->smsConn);
    }
    else
    {
	ListAddLast (WaitForPhase2List, (char *) client);

	if (ListCount (WaitForInteractList) > 0 && OkToEnterInteractPhase ())
	{
	    LetClientInteract (ListFirst (WaitForInteractList));
	}
	else if (OkToEnterPhase2 ())
	{
	    StartPhase2 ();
	}
    }
}



static void
SaveYourselfDoneProc(SmsConn smsConn, SmPointer managerData, Bool success)
{
    ClientRec	*client = (ClientRec *) managerData;

    if (verbose) 
    {
	printf("Client Id = %s, received SAVE YOURSELF DONE [Success = %s]\n",
	       client->clientId, success ? "True" : "False");
    }

    if (!ListSearchAndFreeOne (WaitForSaveDoneList, (char *) client))
    {
	if (ListSearchAndFreeOne (InitialSaveList, (char *) client))
	    SmsSaveComplete (client->smsConn);
	return;
    }

    if (!success)
    {
	ListAddLast (FailedSaveList, (char *) client);
    }

    if (ListCount (WaitForSaveDoneList) == 0)
    {
	if (ListCount (FailedSaveList) > 0 && !checkpoint_from_signal)
	    PopupBadSave ();
	else
	    FinishUpSave ();
    }
    else if (ListCount (WaitForInteractList) > 0 && OkToEnterInteractPhase ())
    {
	LetClientInteract (ListFirst (WaitForInteractList));
    }
    else if (ListCount (WaitForPhase2List) > 0 && OkToEnterPhase2 ())
    {
	StartPhase2 ();
    }
}



void
CloseDownClient(ClientRec *client)
{
    int index_deleted = 0;

    if (verbose) {
	printf ("ICE Connection closed, IceConn fd = %d\n",
		IceConnectionNumber (client->ice_conn));
	printf ("\n");
    }

    SmsCleanUp (client->smsConn);
    IceSetShutdownNegotiation (client->ice_conn, False);
    IceCloseConnection (client->ice_conn);

    client->ice_conn = NULL;
    client->smsConn = NULL;

    if (!shutdownInProgress && client_info_visible)
    {
	for (index_deleted = 0;
	    index_deleted < numClientListNames; index_deleted++)
	{
	    if (clientListRecs[index_deleted] == client)
		break;
	}
    }

    ListSearchAndFreeOne (RunningList, (char *) client);

    if (saveInProgress)
    {
	Status delStatus = ListSearchAndFreeOne (
	    WaitForSaveDoneList, (char *) client);

	if (delStatus)
	{
	    ListAddLast (FailedSaveList, (char *) client);
	    client->freeAfterBadSavePopup = True;
	}

	ListSearchAndFreeOne (WaitForInteractList, (char *) client);
	ListSearchAndFreeOne (WaitForPhase2List, (char *) client);

	if (delStatus && ListCount (WaitForSaveDoneList) == 0)
	{
	    if (ListCount (FailedSaveList) > 0 && !checkpoint_from_signal)
		PopupBadSave ();
	    else
		FinishUpSave ();
	}
	else if (ListCount (WaitForInteractList) > 0 &&
	    OkToEnterInteractPhase ())
	{
	    LetClientInteract (ListFirst (WaitForInteractList));
	}
	else if (!phase2InProgress &&
	    ListCount (WaitForPhase2List) > 0 && OkToEnterPhase2 ())
	{
	    StartPhase2 ();
	}
    }

    if (client->restartHint == SmRestartImmediately && !shutdownInProgress)
    {
	Clone (client, True /* use saved state */);

	ListAddLast (RestartImmedList, (char *) client);
    }
    else if (client->restartHint == SmRestartAnyway)
    {
	ListAddLast (RestartAnywayList, (char *) client);
    }
    else if (!client->freeAfterBadSavePopup)
    {
	FreeClient (client, True /* free props */);
    }

    if (shutdownInProgress)
    {
	if (ListCount (RunningList) == 0)
	    EndSession (0);
    }
    else if (client_info_visible)
    {
	UpdateClientList ();

	if (current_client_selected == index_deleted)
	{
	    if (current_client_selected == numClientListNames)
		current_client_selected--;

	    if (current_client_selected >= 0)
	    {
		XawListHighlight (clientListWidget, current_client_selected);
		ShowHint (clientListRecs[current_client_selected]);
		if (client_prop_visible)
		{
		    DisplayProps (clientListRecs[current_client_selected]);
		}
	    }
	}
	else
	{
	    if (index_deleted < current_client_selected)
		current_client_selected--;
	    XawListHighlight (clientListWidget, current_client_selected);
	}
    }
}




static void
CloseConnectionProc(SmsConn smsConn, SmPointer managerData, 
		    int count, char **reasonMsgs)
{
    ClientRec	*client = (ClientRec *) managerData;

    if (verbose)
    {
	int i;

	printf ("Client Id = %s, received CONNECTION CLOSED\n",
	    client->clientId);

	for (i = 0; i < count; i++)
	    printf ("   Reason string %d: %s\n", i + 1, reasonMsgs[i]);
	printf ("\n");
    }

    SmFreeReasons (count, reasonMsgs);

    CloseDownClient (client);
}



static Status
NewClientProc(SmsConn smsConn, SmPointer managerData, unsigned long *maskRet, 
	      SmsCallbacks *callbacksRet, char **failureReasonRet)
{
    ClientRec *newClient = (ClientRec *) XtMalloc (sizeof (ClientRec));

    *maskRet = 0;

    if (!newClient)
    {
	char *str = "Memory allocation failed";

	if ((*failureReasonRet = (char *) XtMalloc (strlen (str) + 1)) != NULL)
	    strcpy (*failureReasonRet, str);

	return (0);
    }

    newClient->smsConn = smsConn;
    newClient->ice_conn = SmsGetIceConnection (smsConn);
    newClient->clientId = NULL;
    newClient->clientHostname = NULL;
    newClient->restarted = False; /* wait till RegisterClient for true value */
    newClient->userIssuedCheckpoint = False;
    newClient->receivedDiscardCommand = False;
    newClient->freeAfterBadSavePopup = False;
    newClient->props = ListInit ();
    newClient->discardCommand = NULL;
    newClient->saveDiscardCommand = NULL;
    newClient->restartHint = SmRestartIfRunning;

    ListAddLast (RunningList, (char *) newClient);

    if (verbose) {
	printf("On IceConn fd = %d, client set up session mngmt protocol\n\n",
	       IceConnectionNumber (newClient->ice_conn));
    }

    /*
     * Set up session manager callbacks.
     */

    *maskRet |= SmsRegisterClientProcMask;
    callbacksRet->register_client.callback 	= RegisterClientProc;
    callbacksRet->register_client.manager_data  = (SmPointer) newClient;

    *maskRet |= SmsInteractRequestProcMask;
    callbacksRet->interact_request.callback 	= InteractRequestProc;
    callbacksRet->interact_request.manager_data = (SmPointer) newClient;

    *maskRet |= SmsInteractDoneProcMask;
    callbacksRet->interact_done.callback	= InteractDoneProc;
    callbacksRet->interact_done.manager_data    = (SmPointer) newClient;

    *maskRet |= SmsSaveYourselfRequestProcMask;
    callbacksRet->save_yourself_request.callback     = SaveYourselfReqProc;
    callbacksRet->save_yourself_request.manager_data = (SmPointer) newClient;

    *maskRet |= SmsSaveYourselfP2RequestProcMask;
    callbacksRet->save_yourself_phase2_request.callback =
	SaveYourselfPhase2ReqProc;
    callbacksRet->save_yourself_phase2_request.manager_data =
	(SmPointer) newClient;

    *maskRet |= SmsSaveYourselfDoneProcMask;
    callbacksRet->save_yourself_done.callback 	   = SaveYourselfDoneProc;
    callbacksRet->save_yourself_done.manager_data  = (SmPointer) newClient;

    *maskRet |= SmsCloseConnectionProcMask;
    callbacksRet->close_connection.callback 	 = CloseConnectionProc;
    callbacksRet->close_connection.manager_data  = (SmPointer) newClient;

    *maskRet |= SmsSetPropertiesProcMask;
    callbacksRet->set_properties.callback 	= SetPropertiesProc;
    callbacksRet->set_properties.manager_data   = (SmPointer) newClient;

    *maskRet |= SmsDeletePropertiesProcMask;
    callbacksRet->delete_properties.callback	= DeletePropertiesProc;
    callbacksRet->delete_properties.manager_data   = (SmPointer) newClient;

    *maskRet |= SmsGetPropertiesProcMask;
    callbacksRet->get_properties.callback	= GetPropertiesProc;
    callbacksRet->get_properties.manager_data   = (SmPointer) newClient;

    return (1);
}



/*
 * Xt callback invoked when a client attempts to connect.
 */

static void
NewConnectionXtProc(XtPointer client_data, int *source, XtInputId *id)
{
    IceConn 	ice_conn;
    char	*connstr;
    IceAcceptStatus status;

    if (shutdownInProgress)
    {
	/*
	 * Don't accept new connections if we are in the middle
	 * of a shutdown.
	 */

	return;
    }

    ice_conn = IceAcceptConnection((IceListenObj) client_data, &status);
    if (! ice_conn) {
	if (verbose)
	    printf ("IceAcceptConnection failed\n");
    } else {
	IceConnectStatus cstatus;

	while ((cstatus = IceConnectionStatus (ice_conn))==IceConnectPending) {
	    XtAppProcessEvent (appContext, XtIMAll);
	}

	if (cstatus == IceConnectAccepted) {
	    if (verbose) {
		printf ("ICE Connection opened by client, IceConn fd = %d, ",
			IceConnectionNumber (ice_conn));
		connstr = IceConnectionString (ice_conn);
		printf ("Accept at networkId %s\n", connstr);
		free (connstr);
		printf ("\n");
	    }
	} else {
	    if (verbose)
	    {
		if (cstatus == IceConnectIOError)
		    printf ("IO error opening ICE Connection!\n");
		else
		    printf ("ICE Connection rejected!\n");
	    }

	    IceCloseConnection (ice_conn);
	}
    }
}



void
SetAllSensitive(Bool on)
{
    XtSetSensitive (mainWindow, on);
    SetSaveSensitivity (on);
    XtSetSensitive (clientInfoPopup, on);
    XtSetSensitive (clientPropPopup, on);

    if (on && current_client_selected >= 0)
	XawListHighlight (clientListWidget, current_client_selected);
}



/*
 * The real way to handle IO errors is to check the return status
 * of IceProcessMessages.  xsm properly does this.
 *
 * Unfortunately, a design flaw exists in the ICE library in which
 * a default IO error handler is invoked if no IO error handler is
 * installed.  This default handler exits.  We must avoid this.
 *
 * To get around this problem, we install an IO error handler that
 * does a little magic.  Since a previous IO handler might have been
 * installed, when we install our IO error handler, we do a little
 * trick to get both the previous IO error handler and the default
 * IO error handler.  When our IO error handler is called, if the
 * previous handler is not the default handler, we call it.  This
 * way, everyone's IO error handler gets called except the stupid
 * default one which does an exit!
 */

static IceIOErrorHandler prev_handler;

static void
MyIoErrorHandler(IceConn ice_conn)
{
    if (prev_handler)
	(*prev_handler) (ice_conn);
}    

static void
InstallIOErrorHandler(void)

{
    IceIOErrorHandler default_handler;

    prev_handler = IceSetIOErrorHandler (NULL);
    default_handler = IceSetIOErrorHandler (MyIoErrorHandler);
    if (prev_handler == default_handler)
	prev_handler = NULL;
}

static void
CloseListeners(void)

{
    IceFreeListenObjs (numTransports, listenObjs);
}


/* $Xorg: info.c,v 1.5 2001/02/09 02:05:59 xorgcvs Exp $ */
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
/* $XFree86: xc/programs/xsm/info.c,v 1.6 2001/12/14 20:02:25 dawes Exp $ */

#include "xsm.h"
#include "restart.h"
#include "popup.h"
#include "info.h"
#include "prop.h"

#include <X11/Shell.h>
#include <X11/Xaw/Form.h>
#include <X11/Xaw/List.h>
#include <X11/Xaw/Command.h>
#include <X11/Xaw/SimpleMenu.h>
#include <X11/Xaw/MenuButton.h>
#include <X11/Xaw/SmeBSB.h>
#include <X11/Xaw/AsciiText.h>

static Pixmap checkBitmap;

Widget clientInfoPopup;
Widget   clientInfoForm;
Widget     viewPropButton;
Widget	   cloneButton;
Widget	   killClientButton;
Widget	   clientInfoDoneButton;
Widget	   restartHintButton;
Widget	     restartHintMenu;
Widget		restartIfRunning;
Widget		restartAnyway;
Widget		restartImmediately;
Widget		restartNever;
Widget     clientListWidget;
Widget	   noClientsLabel;
Widget	   manualRestartLabel;
Widget	   manualRestartCommands;

Widget clientPropPopup;
Widget   clientPropForm;
Widget     clientPropDoneButton;
Widget     clientPropTextWidget;



void
ShowHint(ClientRec *client)
{
    static Widget active = NULL;
    int hint = client->restartHint;

    if (active)
	XtVaSetValues (active, XtNleftBitmap, None, NULL);

    if (hint == SmRestartIfRunning)
	active = restartIfRunning;
    else if (hint == SmRestartAnyway)
	active = restartAnyway;
    else if (hint == SmRestartImmediately)
	active = restartImmediately;
    else if (hint == SmRestartNever)
	active = restartNever;

    XtVaSetValues (active, XtNleftBitmap, checkBitmap, NULL);
}



typedef struct {
    char *bufStart;
    char *bufPtr;
    int bufSize;
    int bytesLeft;
} Buffer;

#define BUF_START_SIZE 1024
#define BUF_GROW_SIZE 256


static void
AppendStr(Buffer *buffer, char *str)
{
    int len = strlen (str);

    if ((buffer->bytesLeft - 1) < len)
    {
	int newBufSize = buffer->bufSize + len + BUF_GROW_SIZE;
	char *newbuf = (char *) malloc (newBufSize);
	int bytesUsed = buffer->bufPtr - buffer->bufStart;
	memcpy (newbuf, buffer->bufStart, bytesUsed);
	newbuf[bytesUsed] = '\0';
	free (buffer->bufStart);
	buffer->bufStart = newbuf;
	buffer->bufPtr = newbuf + bytesUsed;
	buffer->bufSize = newBufSize;
	buffer->bytesLeft = newBufSize - bytesUsed;
    }

    strcat (buffer->bufPtr, str);
    buffer->bufPtr += len;
    buffer->bytesLeft -= len;
}


void
DisplayProps(ClientRec *client)
{
    int index;
    List *pl, *pj, *vl;
    PropValue *pval;
    Buffer buffer;
    static int first_time = 1;

    for (index = 0; index < numClientListNames; index++)
	if (clientListRecs[index] == client)
	    break;

    if (index >= numClientListNames)
	return;

    buffer.bufStart = buffer.bufPtr = (char *) malloc (BUF_START_SIZE);
    buffer.bufSize = buffer.bytesLeft = BUF_START_SIZE;
    buffer.bufStart[0] = '\0';

    if (ListCount (client->props) > 0)
    {
	char number[10];
	char *ptr;

	AppendStr (&buffer, "*** ID = ");
	AppendStr (&buffer, client->clientId);
	AppendStr (&buffer, " ***\n\n");

	for (pl = ListFirst (client->props); pl; pl = ListNext (pl))
	{
	    Prop *pprop = (Prop *) pl->thing;

	    AppendStr (&buffer, "Name:		");
	    AppendStr (&buffer, pprop->name);
	    AppendStr (&buffer, "\n");
	    AppendStr (&buffer, "Type:		");
	    AppendStr (&buffer, pprop->type);
	    AppendStr (&buffer, "\n");
	    AppendStr (&buffer, "Num values:	");
	    sprintf (number, "%d", ListCount (pprop->values));
	    AppendStr (&buffer, number);
	    AppendStr (&buffer, "\n");

	    if (strcmp (pprop->type, SmCARD8) == 0)
	    {
		char *card8;
		int value;

		vl = ListFirst (pprop->values);
		pval = (PropValue *) vl->thing;

		card8 = pval->value;
		value = *card8;

		AppendStr (&buffer, "Value 1:	");
		sprintf (number, "%d", value);
		AppendStr (&buffer, number);

		if (strcmp (pprop->name, SmRestartStyleHint) == 0)
		{
		    if (value == SmRestartAnyway)
			AppendStr (&buffer, " (Restart Anyway)");
		    else if (value == SmRestartImmediately)
			AppendStr (&buffer, " (Restart Immediately)");
		    else if (value == SmRestartNever)
			AppendStr (&buffer, " (Restart Never)");
		    else
			AppendStr (&buffer, " (Restart If Running)");
		}

		AppendStr (&buffer, "\n");
	    }
	    else
	    {
		int propnum = 0;

		for (pj = ListFirst (pprop->values); pj; pj = ListNext (pj))
		{
		    propnum++;

		    pval = (PropValue *) pj->thing;
		    AppendStr (&buffer, "Value ");
		    sprintf (number, "%d", propnum);
		    AppendStr (&buffer, number);
		    AppendStr (&buffer, ":	");
		    AppendStr (&buffer, (char *) pval->value);
		    AppendStr (&buffer, "\n");
		}
	    }

	    AppendStr (&buffer, "\n");
	}

	XtVaSetValues (clientPropTextWidget,
	    XtNstring, buffer.bufStart,
	    NULL);

	sprintf (buffer.bufStart,
	    "SM Properties : %s", clientListNames[index]);

	ptr = Strstr (buffer.bufStart, ")   Restart");
	if (ptr) *(ptr + 1) = '\0';

	XtVaSetValues (clientPropPopup,
	    XtNtitle, buffer.bufStart,
	    XtNiconName, buffer.bufStart,
	    NULL);

	if (client_prop_visible)
	{
	    /* Make sure it is visible */

	    XMapRaised (XtDisplay (topLevel), XtWindow (clientPropPopup));
	}
	else
	{
	    PopupPopup (mainWindow, clientPropPopup,
		False, first_time, 50, 150, "DelPropWinAction()");

	    client_prop_visible = 1;

	    if (first_time)
		first_time = 0;
	}
    }

    free (buffer.bufStart);
}



static void
ClientListXtProc(Widget w, XtPointer client_data, XtPointer callData)
{
    XawListReturnStruct *current = (XawListReturnStruct *) callData;
    ClientRec *client;

    if (!current || current->list_index < 0)
	return;
	
    client = clientListRecs[current->list_index];
    ShowHint (client);
    current_client_selected = current->list_index;
    if (client_prop_visible)
	DisplayProps (client);
}




static void
ViewPropXtProc(Widget w, XtPointer client_data, XtPointer callData)
{
    ClientRec *client;
    XawListReturnStruct *current;

    current = XawListShowCurrent (clientListWidget);

    if (!current || current->list_index < 0)
    {
	if (current)
	    XtFree ((char *) current);
	return;
    }

    client = clientListRecs[current->list_index];
    DisplayProps (client);
    XtFree ((char *) current);
}



static void
CloneXtProc(Widget w, XtPointer client_data, XtPointer callData)
{
    ClientRec *client;
    XawListReturnStruct *current;

    current = XawListShowCurrent (clientListWidget);

    if (!current || current->list_index < 0)
    {
	if (current)
	    XtFree ((char *) current);
	return;
    }

    client = clientListRecs[current->list_index];

    if (client)
	Clone (client, False /* don't use saved state */);

    XtFree ((char *) current);
}



static void
KillClientXtProc(Widget w, XtPointer client_data, XtPointer callData)
{
    ClientRec *client;
    XawListReturnStruct *current;

    current = XawListShowCurrent (clientListWidget);

    if (!current || current->list_index < 0)
    {
	if (current)
	    XtFree ((char *) current);
	return;
    }

    client = clientListRecs[current->list_index];

    SmsDie (client->smsConn);

    XtFree ((char *) current);
}



static void
listDoneXtProc(Widget w, XtPointer client_data, XtPointer callData)
{
    XtPopdown (clientInfoPopup);
    client_info_visible = 0;
}



char *
GetProgramName(char *fullname)
{
    char *lastSlash = NULL;
    int i;

    for (i = 0; i < (int) strlen (fullname); i++)
	if (fullname[i] == '/')
	    lastSlash = &fullname[i];
    
    if (lastSlash)
	return (lastSlash + 1);
    else
	return (fullname);
}



void
UpdateClientList(void)
{
    ClientRec *client;
    char *progName, *hostname, *tmp1, *tmp2;
    String clientInfo;
    int maxlen1, maxlen2;
    char extraBuf1[80], extraBuf2[80];
    char *restart_service_prop;
    List *cl, *pl;
    int i, k;
    static int reenable_asap = 0;

    if (clientListNames)
    {
	/*
	 * Free the previous list of names.  Xaw doesn't make a copy of
	 * our list, so we need to keep it around.
	 */

	for (i = 0; i < numClientListNames; i++)
	    XtFree (clientListNames[i]);

	XtFree ((char *) clientListNames);

	clientListNames = NULL;
    }

    if (clientListRecs)
    {
	/*
	 * Free the mapping of client names to client records
	 */

	XtFree ((char *) clientListRecs);
	clientListRecs = NULL;
    }

    maxlen1 = maxlen2 = 0;
    numClientListNames = 0;

    for (cl = ListFirst (RunningList); cl; cl = ListNext (cl))
    {
	client = (ClientRec *) cl->thing;

	progName = NULL;
	restart_service_prop = NULL;

	for (pl = ListFirst (client->props); pl; pl = ListNext (pl))
	{
	    Prop *pprop = (Prop *) pl->thing;
	    List *vl = ListFirst (pprop->values);
	    if (vl != NULL)
	    {
	    	PropValue *pval = (PropValue *) vl->thing;

	    	if (strcmp (pprop->name, SmProgram) == 0)
	    	{
			progName = GetProgramName ((char *) pval->value);

			if ((int) strlen (progName) > maxlen1)
			    maxlen1 = strlen (progName);
	    	}
	    	else if (strcmp (pprop->name, "_XC_RestartService") == 0)
	    	{
			restart_service_prop = (char *) pval->value;
	    	}
	    }
	}

	if (!progName)
	    continue;

	if (restart_service_prop)
	    tmp1 = restart_service_prop;
	else if (client->clientHostname)
	    tmp1 = client->clientHostname;
	else
	    continue;

	if ((tmp2 = (char *) strchr (tmp1, '/')) == NULL)
	    hostname = tmp1;
	else
	    hostname = tmp2 + 1;

	if ((int) strlen (hostname) > maxlen2)
	    maxlen2 = strlen (hostname);

	numClientListNames++;
    }

    if (numClientListNames == 0)
    {
	XtSetSensitive (viewPropButton, 0);
	XtSetSensitive (cloneButton, 0);
	XtSetSensitive (killClientButton, 0);
	XtSetSensitive (restartHintButton, 0);

	XtUnmanageChild (clientListWidget);
	XtManageChild (noClientsLabel);

	reenable_asap = 1;

	return;
    }

    if (reenable_asap)
    {
	XtSetSensitive (viewPropButton, 1);
	XtSetSensitive (cloneButton, 1);
	XtSetSensitive (killClientButton, 1);
	XtSetSensitive (restartHintButton, 1);

	XtUnmanageChild (noClientsLabel);
	XtManageChild (clientListWidget);

	reenable_asap = 0;
    }

    clientListNames = (String *) XtMalloc (
	numClientListNames * sizeof (String));
    clientListRecs = (ClientRec **) XtMalloc (
	numClientListNames * sizeof (ClientRec *));

    i = 0;
    for (cl = ListFirst (RunningList); cl; cl = ListNext (cl))
    {
	ClientRec *client = (ClientRec *) cl->thing;
	int extra1, extra2;
	char *hint;

	progName = NULL;
	restart_service_prop = NULL;

	for (pl = ListFirst (client->props); pl; pl = ListNext (pl))
	{
	    Prop *pprop = (Prop *) pl->thing;
	    List *vl = ListFirst (pprop->values);
	    
	    if (vl != NULL)
	    {
	    	PropValue *pval = (PropValue *) vl->thing;
	    	if (strcmp (pprop->name, SmProgram) == 0)
	    	{
			progName = GetProgramName ((char *) pval->value);
	    	}
	    	else if (strcmp (pprop->name, "_XC_RestartService") == 0)
	    	{
			restart_service_prop = (char *) pval->value;
	    	}
	    }
	}

	if (!progName)
	    continue;

	if (restart_service_prop)
	    tmp1 = restart_service_prop;
	else if (client->clientHostname)
	    tmp1 = client->clientHostname;
	else
	    continue;

	if ((tmp2 = (char *) strchr (tmp1, '/')) == NULL)
	    hostname = tmp1;
	else
	    hostname = tmp2 + 1;

	extra1 = maxlen1 - strlen (progName) + 5;
	extra2 = maxlen2 - strlen (hostname);

	if (client->restartHint == SmRestartIfRunning)
	    hint = "Restart If Running";
	else if (client->restartHint == SmRestartAnyway)
	    hint = "Restart Anyway";
	else if (client->restartHint == SmRestartImmediately)
	    hint = "Restart Immediately";
	else if (client->restartHint == SmRestartNever)
	    hint = "Restart Never";
	else
	    hint = "";

	clientInfo = (String) XtMalloc (strlen (progName) +
	    extra1 + extra2 + 3 + strlen (hostname) + 3 + strlen (hint) + 1);

	for (k = 0; k < extra1; k++)
	    extraBuf1[k] = ' ';
	extraBuf1[extra1] = '\0';

	for (k = 0; k < extra2; k++)
	    extraBuf2[k] = ' ';
	extraBuf2[extra2] = '\0';

	sprintf (clientInfo, "%s%s (%s%s)   %s", progName, extraBuf1,
	    hostname, extraBuf2, hint);

	clientListRecs[i] = client;
	clientListNames[i++] = clientInfo;
    }

    XawListChange (clientListWidget,
	clientListNames, numClientListNames, 0, True);
}



static void
RestartHintXtProc(Widget w, XtPointer client_data, XtPointer callData)
{
    XawListReturnStruct *current;
    ClientRec *client;
    Widget active;
    int found = 0;
    List *pl;
    char hint;

    current = XawListShowCurrent (clientListWidget);

    if (!current || current->list_index < 0)
    {
	if (current)
	    XtFree ((char *) current);
	return;
    }

    client = clientListRecs[current->list_index];

    active = XawSimpleMenuGetActiveEntry (restartHintMenu);

    if (active == restartIfRunning)
	hint = SmRestartIfRunning;
    else if (active == restartAnyway)
	hint = SmRestartAnyway;
    else if (active == restartImmediately)
	hint = SmRestartImmediately;
    else if (active == restartNever)
	hint = SmRestartNever;
    else
    {
	XtFree ((char *) current);
	return;
    }

    client->restartHint = hint;

    for (pl = ListFirst (client->props); pl; pl = ListNext (pl))
    {
	Prop *pprop = (Prop *) pl->thing;

	if (strcmp (SmRestartStyleHint, pprop->name) == 0)
	{
	    List *vl = ListFirst (pprop->values);
	    
	    PropValue *pval = (PropValue *) vl->thing;

	    *((char *) (pval->value)) = hint;
	    found = 1;
	    break;
	}
    }

    if (!found)
    {
	SmProp prop;
	SmPropValue propval;

	prop.name = SmRestartStyleHint;
	prop.type = SmCARD8;
	prop.num_vals = 1;
	prop.vals = &propval;
	propval.value = (SmPointer) &hint;
	propval.length = 1;
	
	SetProperty (client, &prop, False /* don't free it */);
    }

    UpdateClientList ();
    XawListHighlight (clientListWidget, current_client_selected);
    ShowHint (client);

    if (client_prop_visible && clientListRecs &&
	clientListRecs[current_client_selected] == client)
    {
	DisplayProps (client);
    }

    XtFree ((char *) current);
}



static void
clientPropDoneXtProc(Widget w, XtPointer client_data, XtPointer callData)
{
    XtPopdown (clientPropPopup);
    client_prop_visible = 0;
}



void
ClientInfoStructureNotifyXtHandler(Widget w, XtPointer closure, 
				   XEvent *event, 
				   Boolean *continue_to_dispatch)
{
    if (event->type == MapNotify)
    {
	UpdateClientList ();

	if (current_client_selected >= 0)
	    XawListHighlight (clientListWidget, current_client_selected);

	XtRemoveEventHandler (clientInfoPopup, StructureNotifyMask, False,
	    ClientInfoStructureNotifyXtHandler, NULL);
    }
}



void
ClientInfoXtProc(Widget w, XtPointer client_data, XtPointer callData)
{
    static int first_time = 1;

    if (client_info_visible)
    {
	/* Make sure it is visible */

	XMapRaised (XtDisplay (topLevel), XtWindow (clientInfoPopup));
    }
    else
    {
	UpdateClientList ();

	if (clientListRecs && clientListRecs[0])
	{
	    current_client_selected = 0;
	    XawListHighlight (clientListWidget, 0);
	    ShowHint (clientListRecs[0]);
	    if (client_prop_visible)
		DisplayProps (clientListRecs[0]);
	}
	else
	    current_client_selected = -1;

	if (first_time && num_clients_in_last_session > 0 &&
	    num_clients_in_last_session != numClientListNames)
	{
	    XtAddEventHandler (clientInfoPopup, StructureNotifyMask, False,
	        ClientInfoStructureNotifyXtHandler, NULL);
	}

	PopupPopup (mainWindow, clientInfoPopup,
	    False, first_time, 100, 50, "DelClientInfoWinAction()");

	client_info_visible = 1;

	if (first_time)
	    first_time = 0;
    }
}


#define CHECK_WIDTH 9
#define CHECK_HEIGHT 8

static unsigned char check_bits[] = {
   0x00, 0x01, 0x80, 0x01, 0xc0, 0x00, 0x60, 0x00,
   0x31, 0x00, 0x1b, 0x00, 0x0e, 0x00, 0x04, 0x00
};



static void
DelClientInfoWinAction(Widget w, XEvent *event, String *params, 
		       Cardinal *num_params)
{
    XtCallCallbacks (clientInfoDoneButton, XtNcallback, NULL);
}



static void
DelPropWinAction(Widget w, XEvent *event, String *params, Cardinal *num_params)
{
    XtCallCallbacks (clientPropDoneButton, XtNcallback, NULL);
}



void
create_client_info_popup(void)

{
    /*
     * Install actions for WM_DELETE_WINDOW
     */

    static XtActionsRec actions[] = {
        {"DelClientInfoWinAction", DelClientInfoWinAction},
        {"DelPropWinAction", DelPropWinAction}
    };

    XtAppAddActions (appContext, actions, XtNumber (actions));


    /*
     * Make checkmark bitmap
     */

    checkBitmap = XCreateBitmapFromData (
	XtDisplay (topLevel), RootWindowOfScreen (XtScreen (topLevel)),
	(char *) check_bits, CHECK_WIDTH, CHECK_HEIGHT);


    /*
     * Pop up for List Clients button.
     */

    clientInfoPopup = XtVaCreatePopupShell (
	"clientInfoPopup", topLevelShellWidgetClass, topLevel,
	XtNallowShellResize, True,
	NULL);
    

    clientInfoForm = XtVaCreateManagedWidget (
	"clientInfoForm", formWidgetClass, clientInfoPopup,
	NULL);

    viewPropButton = XtVaCreateManagedWidget (
	"viewPropButton", commandWidgetClass, clientInfoForm,
        XtNfromHoriz, NULL,
        XtNfromVert, NULL,
	XtNtop, XawChainTop,
	XtNbottom, XawChainTop,
	XtNleft, XawChainLeft,
	XtNright, XawChainLeft,
        NULL);
    
    XtAddCallback (viewPropButton, XtNcallback, ViewPropXtProc, 0);


    cloneButton = XtVaCreateManagedWidget (
	"cloneButton", commandWidgetClass, clientInfoForm,
        XtNfromHoriz, viewPropButton,
        XtNfromVert, NULL,
	XtNtop, XawChainTop,
	XtNbottom, XawChainTop,
	XtNleft, XawChainLeft,
	XtNright, XawChainLeft,
        NULL);
    
    XtAddCallback (cloneButton, XtNcallback, CloneXtProc, 0);


    killClientButton = XtVaCreateManagedWidget (
	"killClientButton", commandWidgetClass, clientInfoForm,
        XtNfromHoriz, cloneButton,
        XtNfromVert, NULL,
	XtNtop, XawChainTop,
	XtNbottom, XawChainTop,
	XtNleft, XawChainLeft,
	XtNright, XawChainLeft,
        NULL);
    
    XtAddCallback (killClientButton, XtNcallback, KillClientXtProc, 0);


    restartHintButton = XtVaCreateManagedWidget (
	"restartHintButton", menuButtonWidgetClass, clientInfoForm,
	XtNmenuName, "restartHintMenu",
        XtNfromHoriz, killClientButton,
        XtNfromVert, NULL,
	XtNtop, XawChainTop,
	XtNbottom, XawChainTop,
	XtNleft, XawChainLeft,
	XtNright, XawChainLeft,
	NULL);

    restartHintMenu = XtVaCreatePopupShell (
	"restartHintMenu", simpleMenuWidgetClass, clientInfoForm,
	NULL);

    restartIfRunning = XtVaCreateManagedWidget (
	"restartIfRunning", smeBSBObjectClass, restartHintMenu,
	XtNleftMargin, 18,
	NULL);

    restartAnyway = XtVaCreateManagedWidget (
	"restartAnyway", smeBSBObjectClass, restartHintMenu,
	XtNleftMargin, 18,
	NULL);

    restartImmediately = XtVaCreateManagedWidget (
	"restartImmediately", smeBSBObjectClass, restartHintMenu,
	XtNleftMargin, 18,
	NULL);

    restartNever = XtVaCreateManagedWidget (
	"restartNever", smeBSBObjectClass, restartHintMenu,
	XtNleftMargin, 18,
	NULL);

    XtAddCallback (restartIfRunning, XtNcallback, RestartHintXtProc, 0);
    XtAddCallback (restartAnyway, XtNcallback, RestartHintXtProc, 0);
    XtAddCallback (restartImmediately, XtNcallback, RestartHintXtProc, 0);
    XtAddCallback (restartNever, XtNcallback, RestartHintXtProc, 0);


    clientInfoDoneButton = XtVaCreateManagedWidget (
	"clientInfoDoneButton", commandWidgetClass, clientInfoForm,
        XtNfromHoriz, restartHintButton,
        XtNfromVert, NULL,
	XtNtop, XawChainTop,
	XtNbottom, XawChainTop,
	XtNleft, XawChainLeft,
	XtNright, XawChainLeft,
        NULL);

    XtAddCallback (clientInfoDoneButton, XtNcallback, listDoneXtProc, 0);


    clientListWidget = XtVaCreateManagedWidget (
	"clientListWidget", listWidgetClass, clientInfoForm,
        XtNdefaultColumns, 1,
	XtNforceColumns, True,
        XtNfromHoriz, NULL,
        XtNfromVert, viewPropButton,
	XtNresizable, True,
	XtNtop, XawChainTop,
	XtNbottom, XawChainTop,
	NULL);

    XtAddCallback (clientListWidget, XtNcallback, ClientListXtProc, 0);

    noClientsLabel = XtVaCreateWidget (
	"noClientsLabel", labelWidgetClass, clientInfoForm,
        XtNfromHoriz, NULL,
        XtNfromVert, viewPropButton,
        XtNborderWidth, 0,
	XtNtop, XawChainTop,
	XtNbottom, XawChainTop,
	NULL);

    manualRestartLabel = XtVaCreateManagedWidget (
	"manualRestartLabel", labelWidgetClass, clientInfoForm,
        XtNfromHoriz, NULL,
        XtNfromVert, clientListWidget,
        XtNborderWidth, 0,
        XtNvertDistance, 20,
	XtNtop, XawChainBottom,
	XtNbottom, XawChainBottom,
	XtNleft, XawChainLeft,
	XtNright, XawChainLeft,
	NULL);

    manualRestartCommands = XtVaCreateManagedWidget (
	"manualRestartCommands", asciiTextWidgetClass, clientInfoForm,
        XtNfromHoriz, NULL,
        XtNfromVert, manualRestartLabel,
	XtNeditType, XawtextEdit,
	XtNresizable, True,
	XtNresize, XawtextResizeWidth,
	XtNscrollVertical, XawtextScrollAlways,
	XtNwidth, 350,
	XtNheight, 100,
	XtNtop, XawChainBottom,
	XtNbottom, XawChainBottom,
	NULL);

    /*
     * Pop up for viewing client properties
     */

    clientPropPopup = XtVaCreatePopupShell (
	"clientPropPopup", topLevelShellWidgetClass, topLevel,
	XtNallowShellResize, True,
	NULL);
    

    clientPropForm = XtVaCreateManagedWidget (
	"clientPropForm", formWidgetClass, clientPropPopup,
	NULL);

    clientPropDoneButton = XtVaCreateManagedWidget (
	"clientPropDoneButton", commandWidgetClass, clientPropForm,
        XtNfromHoriz, NULL,
        XtNfromVert, NULL,
	XtNtop, XawChainTop,
	XtNbottom, XawChainTop,
	XtNleft, XawChainLeft,
	XtNright, XawChainLeft,
        NULL);

    XtAddCallback (clientPropDoneButton, XtNcallback, clientPropDoneXtProc, 0);


    clientPropTextWidget = XtVaCreateManagedWidget (
	"clientPropTextWidget", asciiTextWidgetClass, clientPropForm,
        XtNfromHoriz, NULL,
        XtNfromVert, clientPropDoneButton,
	XtNeditType, XawtextRead,
        XtNdisplayCaret, False,
	XtNscrollVertical, XawtextScrollWhenNeeded,
	XtNscrollHorizontal, XawtextScrollWhenNeeded,
	XtNresizable, True,
	XtNtop, XawChainTop,
	XtNbottom, XawChainBottom,
	NULL);
}

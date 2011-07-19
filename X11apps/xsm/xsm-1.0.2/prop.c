/* $Xorg: prop.c,v 1.4 2001/02/09 02:06:01 xorgcvs Exp $ */
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
/* $XFree86: xc/programs/xsm/prop.c,v 1.5tsi Exp $ */

#include "xsm.h"
#include "info.h"
#include "prop.h"
#include <X11/Xaw/List.h>


void
FreePropValues(List *propValues)
{
    List	*pv;
    PropValue	*pval;

    for (pv = ListFirst (propValues); pv; pv = ListNext (pv))
    {
	pval = (PropValue *) pv->thing;
	XtFree ((char *) pval->value);
	XtFree ((char *) pval);
    }

    ListFreeAll (propValues);
}



void
FreeProp(Prop *prop)
{
    FreePropValues (prop->values);
    XtFree (prop->name);
    XtFree (prop->type);
    XtFree ((char *) prop);
}



void
SetInitialProperties(ClientRec *client, List *props)
{
    List *pl;

    if (verbose)
	printf("Setting initial properties for %s\n", client->clientId);

    if (client->props)
    {
	/*
	 * The only way client->props could be non-NULL is if the list
	 * was initialized, but nothing was added yet.  So we just free
	 * the head of the list.
	 */

	XtFree ((char *) client->props);
    }

    client->props = props;

    for (pl = ListFirst (props); pl; pl = ListNext (pl))
    {
	Prop		*pprop;
	PropValue	*pval;
	List		*vl;

	pprop = (Prop *) pl->thing;

	if (strcmp (pprop->name, SmDiscardCommand) == 0)
	{
	    if (client->discardCommand)
		XtFree (client->discardCommand);

	    vl = ListFirst (pprop->values);
	    pval = (PropValue *) vl->thing;

	    client->discardCommand = (char *) XtNewString (
		(char *) pval->value);
	}
	else if (strcmp (pprop->name, SmRestartStyleHint) == 0)
	{
	    int hint;

	    vl = ListFirst (pprop->values);
	    pval = (PropValue *) vl->thing;

	    hint = (int) *((char *) (pval->value));

	    if (hint == SmRestartIfRunning || hint == SmRestartAnyway ||
		hint == SmRestartImmediately || hint == SmRestartNever)
	    {
		client->restartHint = hint;
	    }
	}
    }
}



void
SetProperty(ClientRec *client, SmProp *theProp, Bool freeIt)
{
    List 	*pl;
    Prop	*pprop = NULL;
    int		found = 0, i;

    /*
     * If the property exists, delete the property values.  We can
     * re-use the actual property header.
     */

    for (pl = ListFirst (client->props); pl; pl = ListNext (pl))
    {
	pprop = (Prop *) pl->thing;

	if (strcmp (theProp->name, pprop->name) == 0 &&
	    strcmp (theProp->type, pprop->type) == 0)
	{
	    FreePropValues (pprop->values);
	    found = 1;
	    break;
	}
    }


    /*
     * Add the new property
     */

    if (!found)
    {
	pprop = (Prop *) XtMalloc (sizeof (Prop));
	pprop->name = XtNewString (theProp->name);
	pprop->type = XtNewString (theProp->type);
    }

    pprop->values = ListInit ();

    for (i = 0; i < theProp->num_vals; i++)
    {
	PropValue *pval = (PropValue *) XtMalloc (sizeof (PropValue));

	pval->length = theProp->vals[i].length;
	pval->value = (XtPointer) XtMalloc (theProp->vals[i].length + 1);
	memcpy (pval->value, theProp->vals[i].value, theProp->vals[i].length);
	((char *) pval->value)[theProp->vals[i].length] = '\0';

	ListAddLast (pprop->values, (char *) pval);
    }

    if (pl)
	pl->thing = (char *) pprop;
    else
	ListAddLast (client->props, (char *) pprop);

    if (strcmp (theProp->name, SmDiscardCommand) == 0)
    {
	if (saveInProgress)
	{
	    /*
	     * We are in the middle of a save yourself.  We save the
	     * discard command we get now, and make it the current discard
	     * command when the save is over.
	     */

	    if (client->saveDiscardCommand)
		XtFree (client->saveDiscardCommand);
	    client->saveDiscardCommand =
		(char *) XtNewString ((char *) theProp->vals[0].value);

	    client->receivedDiscardCommand = True;
	}
	else
	{
	    if (client->discardCommand)
		XtFree (client->discardCommand);
	    client->discardCommand =
		(char *) XtNewString ((char *) theProp->vals[0].value);
	}
    }
    else if (strcmp (theProp->name, SmRestartStyleHint) == 0)
    {
	int hint = (int) *((char *) (theProp->vals[0].value));

	if (hint == SmRestartIfRunning || hint == SmRestartAnyway ||
	    hint == SmRestartImmediately || hint == SmRestartNever)
	{
	    client->restartHint = hint;
	}
    }

    if (freeIt)
	SmFreeProperty (theProp);
}



void
DeleteProperty(ClientRec *client, char *propname)
{
    List *pl;

    for (pl = ListFirst (client->props); pl; pl = ListNext (pl))
    {
	Prop *pprop = (Prop *) pl->thing;

	if (strcmp (pprop->name, propname) == 0)
	{
	    FreeProp (pprop);
	    ListFreeOne (pl);

	    if (strcmp (propname, SmDiscardCommand) == 0)
	    {
		if (client->discardCommand)
		{
		    XtFree (client->discardCommand);
		    client->discardCommand = NULL;
		}

		if (client->saveDiscardCommand)
		{
		    XtFree (client->saveDiscardCommand);
		    client->saveDiscardCommand = NULL;
		}
	    }
	    break;
	}
    }
}



void
SetPropertiesProc(SmsConn smsConn, SmPointer managerData, int numProps, 
		  SmProp **props)
{
    ClientRec	*client = (ClientRec *) managerData;
    int		updateList, i;

    if (verbose)
    {
	printf ("Client Id = %s, received SET PROPERTIES ", client->clientId);
	printf ("[Num props = %d]\n", numProps);
    }

    updateList = (ListCount (client->props) == 0) &&
	numProps > 0 && client_info_visible;

    for (i = 0; i < numProps; i++)
    {
	SetProperty (client, props[i], True /* free it */);
    }

    free ((char *) props);

    if (updateList)
    {
	/*
	 * We have enough info from the client to display it in our list.
	 */

	UpdateClientList ();
	XawListHighlight (clientListWidget, current_client_selected);
    }
    else if (client_prop_visible && clientListRecs &&
	clientListRecs[current_client_selected] == client)
    {
	DisplayProps (client);
    }
}



void
DeletePropertiesProc(SmsConn smsConn, SmPointer managerData, 
		     int numProps, char **propNames)

{
    ClientRec	*client = (ClientRec *) managerData;
    int		i;

    if (verbose) {
	printf ("Client Id = %s, received DELETE PROPERTIES ",
	    client->clientId);
	printf ("[Num props = %d]\n", numProps);
    }

    for (i = 0; i < numProps; i++)
    {
	if (verbose)
	    printf ("   Name:	%s\n", propNames[i]);

	DeleteProperty (client, propNames[i]);

	free (propNames[i]);
    }

    free ((char *) propNames);
}



void
GetPropertiesProc(SmsConn smsConn, SmPointer managerData)
{
    ClientRec	*client = (ClientRec *) managerData;
    SmProp	**propsRet, *propRet;
    SmPropValue *propValRet;
    Prop	*pprop;
    PropValue	*pval;
    List	*pl, *pj;
    int		numProps;
    int		index, i;

    if (verbose)
    {
	printf ("Client Id = %s, received GET PROPERTIES\n", client->clientId);
	printf ("\n");
    }

    /*
     * Unfortunately, we store the properties in a format different
     * from the one required by SMlib.
     */

    numProps = ListCount (client->props);
    propsRet = (SmProp **) XtMalloc (numProps * sizeof (SmProp *));

    index = 0;
    for (pl = ListFirst (client->props); pl; pl = ListNext (pl))
    {
	propsRet[index] = propRet = (SmProp *) XtMalloc (sizeof (SmProp));

	pprop = (Prop *) pl->thing;

	propRet->name = XtNewString (pprop->name);
	propRet->type = XtNewString (pprop->type);
	propRet->num_vals = ListCount (pprop->values);
	propRet->vals = propValRet = (SmPropValue *) XtMalloc (
	    propRet->num_vals * sizeof (SmPropValue));

	for (pj = ListFirst (pprop->values); pj; pj = ListNext (pj))
	{
	    pval = (PropValue *) pj->thing;

	    propValRet->length = pval->length;
	    propValRet->value = (SmPointer) XtMalloc (pval->length);
	    memcpy (propValRet->value, pval->value, pval->length);

	    propValRet++;
	}

	index++;
    }

    SmsReturnProperties (smsConn, numProps, propsRet);

    if (verbose)
    {
	printf ("Client Id = %s, sent PROPERTIES REPLY [Num props = %d]\n",
		client->clientId, numProps);
    }

    for (i = 0; i < numProps; i++)
	SmFreeProperty (propsRet[i]);
    XtFree ((char *) propsRet);
}

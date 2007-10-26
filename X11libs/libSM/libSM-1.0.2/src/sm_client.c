/* $XdotOrg: $ */
/* $Xorg: sm_client.c,v 1.4 2001/02/09 02:03:30 xorgcvs Exp $ */

/*

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

*/

/*
 * Author: Ralph Mor, X Consortium
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <X11/SM/SMlib.h>
#include "SMlibint.h"
#include "globals.h"

static void set_callbacks();


SmcConn
SmcOpenConnection (networkIdsList, context,
    xsmpMajorRev, xsmpMinorRev, mask, callbacks,
    previousId, clientIdRet, errorLength, errorStringRet)

char 		*networkIdsList;
SmPointer	context;
int		xsmpMajorRev;
int		xsmpMinorRev;
unsigned long   mask;
SmcCallbacks	*callbacks;
char 		*previousId;
char 		**clientIdRet;
int  		errorLength;
char 		*errorStringRet;

{
    SmcConn			smcConn;
    IceConn			iceConn;
    char 			*ids;
    IceProtocolSetupStatus	setupstat;
    int				majorVersion;
    int				minorVersion;
    char			*vendor = NULL;
    char			*release = NULL;
    smRegisterClientMsg 	*pMsg;
    char 			*pData;
    int				extra, len;
    IceReplyWaitInfo		replyWait;
    _SmcRegisterClientReply	reply;
    Bool			gotReply, ioErrorOccured;

    *clientIdRet = NULL;

    if (errorStringRet && errorLength > 0)
	*errorStringRet = '\0';

    if (!_SmcOpcode)
    {
	/*
	 * For now, there is only one version of XSMP, so we don't
	 * have to check {xsmpMajorRev, xsmpMinorRev}.  In the future,
	 * we will check against _SmcVersions and generate the list
	 * of versions the application actually supports.
	 */

	if ((_SmcOpcode = IceRegisterForProtocolSetup ("XSMP",
	    SmVendorString, SmReleaseString, _SmVersionCount, _SmcVersions,
            _SmAuthCount, _SmAuthNames, _SmcAuthProcs, NULL)) < 0)
	{
	    if (errorStringRet && errorLength > 0) {
		strncpy (errorStringRet,
			 "Could not register XSMP protocol with ICE",
			 errorLength);
		errorStringRet[errorLength - 1] = '\0';
	    }

	    return (NULL);
	}
    }

    if (networkIdsList == NULL || *networkIdsList == '\0')
    {
	if ((ids = (char *) getenv ("SESSION_MANAGER")) == NULL)
	{
	    if (errorStringRet && errorLength > 0) {
		strncpy (errorStringRet,
			 "SESSION_MANAGER environment variable not defined",
			 errorLength);
		errorStringRet[errorLength - 1] = '\0';
	    }
	    return (NULL);
	}
    }
    else
    {
	ids = networkIdsList;
    }

    if ((iceConn = IceOpenConnection (
	ids, context, 0, _SmcOpcode, errorLength, errorStringRet)) == NULL)
    {
	return (NULL);
    }

    if ((smcConn = (SmcConn) malloc (sizeof (struct _SmcConn))) == NULL)
    {
	if (errorStringRet && errorLength > 0) {
	    strncpy (errorStringRet, "Can't malloc", errorLength);
	    errorStringRet[errorLength - 1] = '\0';
	}
	IceCloseConnection (iceConn);
	return (NULL);
    }

    setupstat = IceProtocolSetup (iceConn, _SmcOpcode,
	(IcePointer) smcConn,
	False /* mustAuthenticate */,
	&majorVersion, &minorVersion,
	&vendor, &release, errorLength, errorStringRet);

    if (setupstat == IceProtocolSetupFailure ||
	setupstat == IceProtocolSetupIOError)
    {
	IceCloseConnection (iceConn);
	free ((char *) smcConn);
	return (NULL);
    }
    else if (setupstat == IceProtocolAlreadyActive)
    {
	/*
	 * This case should never happen, because when we called
	 * IceOpenConnection, we required that the ICE connection
	 * may not already have XSMP active on it.
	 */

	free ((char *) smcConn);
	if (errorStringRet && errorLength > 0) {
	    strncpy (errorStringRet, "Internal error in IceOpenConnection",
		     errorLength);
	    errorStringRet[errorLength - 1] = '\0';
	}
	return (NULL);
    }

    smcConn->iceConn = iceConn;
    smcConn->proto_major_version = majorVersion;
    smcConn->proto_minor_version = minorVersion;
    smcConn->vendor = vendor;
    smcConn->release = release;
    smcConn->client_id = NULL;

    bzero ((char *) &smcConn->callbacks, sizeof (SmcCallbacks));
    set_callbacks (smcConn, mask, callbacks);

    smcConn->interact_waits = NULL;
    smcConn->phase2_wait = NULL;
    smcConn->prop_reply_waits = NULL;

    smcConn->save_yourself_in_progress = False;
    smcConn->shutdown_in_progress = False;


    /*
     * Now register the client
     */

    len = previousId ? strlen (previousId) : 0;
    extra = ARRAY8_BYTES (len);

    IceGetHeaderExtra (iceConn, _SmcOpcode, SM_RegisterClient,
	SIZEOF (smRegisterClientMsg), WORD64COUNT (extra),
	smRegisterClientMsg, pMsg, pData);

    STORE_ARRAY8 (pData, len, previousId);

    IceFlush (iceConn);

    replyWait.sequence_of_request = IceLastSentSequenceNumber (iceConn);
    replyWait.major_opcode_of_request = _SmcOpcode;
    replyWait.minor_opcode_of_request = SM_RegisterClient;
    replyWait.reply = (IcePointer) &reply;

    gotReply = False;
    ioErrorOccured = False;

    while (!gotReply && !ioErrorOccured)
    {
	ioErrorOccured = (IceProcessMessages (
	    iceConn, &replyWait, &gotReply) == IceProcessMessagesIOError);

	if (ioErrorOccured)
	{
	    if (errorStringRet && errorLength > 0) {
		strncpy (errorStringRet, "IO error occured opening connection",
			 errorLength);
		errorStringRet[errorLength - 1] = '\0';
	    }
	    free (smcConn->vendor);
	    free (smcConn->release);
	    free ((char *) smcConn);

	    return (NULL);
	}
	else if (gotReply)
	{
	    if (reply.status == 1)
	    {
		/*
		 * The client successfully registered.
		 */

		*clientIdRet = reply.client_id;

		smcConn->client_id = (char *) malloc (
		    strlen (*clientIdRet) + 1);

		strcpy (smcConn->client_id, *clientIdRet);
	    }
	    else
	    {
		/*
		 * Could not register the client because the previous ID
		 * was bad.  So now we register the client with the
		 * previous ID set to NULL.
		 */

		extra = ARRAY8_BYTES (0);

		IceGetHeaderExtra (iceConn, _SmcOpcode, SM_RegisterClient,
		    SIZEOF (smRegisterClientMsg), WORD64COUNT (extra),
		    smRegisterClientMsg, pMsg, pData);

		STORE_ARRAY8 (pData, 0, NULL);

		IceFlush (iceConn);

		replyWait.sequence_of_request =
		    IceLastSentSequenceNumber (iceConn);

		gotReply = False;
	    }
	}
    }

    return (smcConn);
}



SmcCloseStatus
SmcCloseConnection (smcConn, count, reasonMsgs)

SmcConn smcConn;
int	count;
char    **reasonMsgs;

{
    IceConn			iceConn = smcConn->iceConn;
    smCloseConnectionMsg 	*pMsg;
    char 			*pData;
    int				extra, i;
    IceCloseStatus	        closeStatus;
    SmcCloseStatus		statusRet;

    extra = 8;

    for (i = 0; i < count; i++)
	extra += ARRAY8_BYTES (strlen (reasonMsgs[i]));

    IceGetHeaderExtra (iceConn, _SmcOpcode, SM_CloseConnection,
	SIZEOF (smCloseConnectionMsg), WORD64COUNT (extra),
	smCloseConnectionMsg, pMsg, pData);

    STORE_CARD32 (pData, count);
    pData += 4;

    for (i = 0; i < count; i++)
	STORE_ARRAY8 (pData, strlen (reasonMsgs[i]), reasonMsgs[i]); 

    IceFlush (iceConn);

    IceProtocolShutdown (iceConn, _SmcOpcode);
    IceSetShutdownNegotiation (iceConn, False);
    closeStatus = IceCloseConnection (iceConn);

    if (smcConn->vendor)
	free (smcConn->vendor);

    if (smcConn->release)
	free (smcConn->release);

    if (smcConn->client_id)
	free (smcConn->client_id);

    if (smcConn->prop_reply_waits)
    {
	_SmcPropReplyWait *ptr = smcConn->prop_reply_waits;
	_SmcPropReplyWait *next;

	while (ptr)
	{
	    next = ptr->next;
	    free ((char *) ptr);
	    ptr = next;
	}
	
    }

    free ((char *) smcConn);

    if (closeStatus == IceClosedNow)
	statusRet = SmcClosedNow;
    else if (closeStatus == IceClosedASAP)
	statusRet = SmcClosedASAP;
    else
	statusRet = SmcConnectionInUse;

    return (statusRet);
}



void
SmcModifyCallbacks (smcConn, mask, callbacks)

SmcConn    	smcConn;
unsigned long 	mask;
SmcCallbacks	*callbacks;

{
    set_callbacks (smcConn, mask, callbacks);
}



void
SmcSetProperties (smcConn, numProps, props)

SmcConn    	smcConn;
int      	numProps;
SmProp       	**props;

{
    IceConn		iceConn = smcConn->iceConn;
    smSetPropertiesMsg	*pMsg;
    char		*pBuf;
    char		*pStart;
    int			bytes;

    IceGetHeader (iceConn, _SmcOpcode, SM_SetProperties,
	SIZEOF (smSetPropertiesMsg), smSetPropertiesMsg, pMsg);

    LISTOF_PROP_BYTES (numProps, props, bytes);
    pMsg->length += WORD64COUNT (bytes);

    pBuf = pStart = IceAllocScratch (iceConn, bytes);

    STORE_LISTOF_PROPERTY (pBuf, numProps, props);

    IceWriteData (iceConn, bytes, pStart);
    IceFlush (iceConn);
}



void
SmcDeleteProperties (smcConn, numProps, propNames)

SmcConn smcConn;
int     numProps;
char	**propNames;

{
    IceConn			iceConn = smcConn->iceConn;
    smDeletePropertiesMsg 	*pMsg;
    char 			*pData;
    int				extra, i;

    extra = 8;

    for (i = 0; i < numProps; i++)
	extra += ARRAY8_BYTES (strlen (propNames[i]));

    IceGetHeaderExtra (iceConn, _SmcOpcode, SM_DeleteProperties,
	SIZEOF (smDeletePropertiesMsg), WORD64COUNT (extra),
	smDeletePropertiesMsg, pMsg, pData);

    STORE_CARD32 (pData, numProps);
    pData += 4;

    for (i = 0; i < numProps; i++)
	STORE_ARRAY8 (pData, strlen (propNames[i]), propNames[i]); 

    IceFlush (iceConn);
}



Status
SmcGetProperties (smcConn, propReplyProc, clientData)

SmcConn		 smcConn;
SmcPropReplyProc propReplyProc;
SmPointer	 clientData;

{
    IceConn		iceConn = smcConn->iceConn;
    _SmcPropReplyWait 	*wait, *ptr;

    if ((wait = (_SmcPropReplyWait *) malloc (
	sizeof (_SmcPropReplyWait))) == NULL)
    {
	return (0);
    }

    wait->prop_reply_proc = propReplyProc;
    wait->client_data = clientData;
    wait->next = NULL;

    ptr = smcConn->prop_reply_waits;
    while (ptr && ptr->next)
	ptr = ptr->next;

    if (ptr == NULL)
	smcConn->prop_reply_waits = wait;
    else
	ptr->next = wait;

    IceSimpleMessage (iceConn, _SmcOpcode, SM_GetProperties);
    IceFlush (iceConn);

    return (1);
}



Status
SmcInteractRequest (smcConn, dialogType, interactProc, clientData)

SmcConn 	smcConn;
int		dialogType;
SmcInteractProc	interactProc;
SmPointer	clientData;

{
    IceConn			iceConn = smcConn->iceConn;
    smInteractRequestMsg	*pMsg;
    _SmcInteractWait 		*wait, *ptr;

    if ((wait = (_SmcInteractWait *) malloc (
	sizeof (_SmcInteractWait))) == NULL)
    {
	return (0);
    }

    wait->interact_proc = interactProc;
    wait->client_data = clientData;
    wait->next = NULL;

    ptr = smcConn->interact_waits;
    while (ptr && ptr->next)
	ptr = ptr->next;

    if (ptr == NULL)
	smcConn->interact_waits = wait;
    else
	ptr->next = wait;

    IceGetHeader (iceConn, _SmcOpcode, SM_InteractRequest,
	SIZEOF (smInteractRequestMsg), smInteractRequestMsg, pMsg);

    pMsg->dialogType = dialogType;

    IceFlush (iceConn);

    return (1);
}



void
SmcInteractDone (smcConn, cancelShutdown)

SmcConn smcConn;
Bool 	cancelShutdown;

{
    IceConn		iceConn = smcConn->iceConn;
    smInteractDoneMsg	*pMsg;

    IceGetHeader (iceConn, _SmcOpcode, SM_InteractDone,
	SIZEOF (smInteractDoneMsg), smInteractDoneMsg, pMsg);

    pMsg->cancelShutdown = cancelShutdown;

    IceFlush (iceConn);
}



void
SmcRequestSaveYourself (smcConn, saveType, shutdown, interactStyle,
	fast, global)

SmcConn smcConn;
int	saveType;
Bool 	shutdown;
int	interactStyle;
Bool	fast;
Bool	global;

{
    IceConn			iceConn = smcConn->iceConn;
    smSaveYourselfRequestMsg	*pMsg;

    IceGetHeader (iceConn, _SmcOpcode, SM_SaveYourselfRequest,
	SIZEOF (smSaveYourselfRequestMsg), smSaveYourselfRequestMsg, pMsg);

    pMsg->saveType = saveType;
    pMsg->shutdown = shutdown;
    pMsg->interactStyle = interactStyle;
    pMsg->fast = fast;
    pMsg->global = global;

    IceFlush (iceConn);
}



Status
SmcRequestSaveYourselfPhase2 (smcConn, saveYourselfPhase2Proc, clientData)

SmcConn 			smcConn;
SmcSaveYourselfPhase2Proc	saveYourselfPhase2Proc;
SmPointer			clientData;

{
    IceConn		iceConn = smcConn->iceConn;
    _SmcPhase2Wait 	*wait;

    if (smcConn->phase2_wait)
	wait = smcConn->phase2_wait;
    else
    {
	if ((wait = (_SmcPhase2Wait *) malloc (
	    sizeof (_SmcPhase2Wait))) == NULL)
	{
	    return (0);
	}
    }

    wait->phase2_proc = saveYourselfPhase2Proc;
    wait->client_data = clientData;

    smcConn->phase2_wait = wait;

    IceSimpleMessage (iceConn, _SmcOpcode, SM_SaveYourselfPhase2Request);
    IceFlush (iceConn);

    return (1);
}



void
SmcSaveYourselfDone (smcConn, success)

SmcConn smcConn;
Bool	success;

{
    IceConn			iceConn = smcConn->iceConn;
    smSaveYourselfDoneMsg	*pMsg;

    IceGetHeader (iceConn, _SmcOpcode, SM_SaveYourselfDone,
	SIZEOF (smSaveYourselfDoneMsg), smSaveYourselfDoneMsg, pMsg);

    pMsg->success = success;

    IceFlush (iceConn);
}



static void
set_callbacks (smcConn, mask, callbacks)

SmcConn    	smcConn;
unsigned long 	mask;
SmcCallbacks	*callbacks;

{
    if (mask & SmcSaveYourselfProcMask)
    {
	smcConn->callbacks.save_yourself.callback =
	    callbacks->save_yourself.callback;
	smcConn->callbacks.save_yourself.client_data =
	    callbacks->save_yourself.client_data;
    }

    if (mask & SmcDieProcMask)
    {
	smcConn->callbacks.die.callback = callbacks->die.callback;
	smcConn->callbacks.die.client_data = callbacks->die.client_data;
    }

    if (mask & SmcSaveCompleteProcMask)
    {
	smcConn->callbacks.save_complete.callback =
	    callbacks->save_complete.callback;
	smcConn->callbacks.save_complete.client_data =
	    callbacks->save_complete.client_data;
    }

    if (mask & SmcShutdownCancelledProcMask)
    {
	smcConn->callbacks.shutdown_cancelled.callback =
	    callbacks->shutdown_cancelled.callback;
	smcConn->callbacks.shutdown_cancelled.client_data =
	    callbacks->shutdown_cancelled.client_data;
    }
}

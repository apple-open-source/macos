/* $Xorg: sm_manager.c,v 1.4 2001/02/09 02:03:30 xorgcvs Exp $ */

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

#include <X11/SM/SMlib.h>
#include "SMlibint.h"
#include <X11/Xtrans.h>



Status
SmsInitialize (vendor, release, newClientProc, managerData,
    hostBasedAuthProc, errorLength, errorStringRet)

char 		 		*vendor;
char 		 		*release;
SmsNewClientProc 		newClientProc;
SmPointer	 		managerData;
IceHostBasedAuthProc		hostBasedAuthProc;
int  		 		errorLength;
char 		 		*errorStringRet;

{
    if (errorStringRet && errorLength > 0)
	*errorStringRet = '\0';

    if (!newClientProc)
    {
	strncpy (errorStringRet,
	    "The SmsNewClientProc callback can't be NULL", errorLength);

	return (0);
    }

    if (!_SmsOpcode)
    {
	Status _SmsProtocolSetupProc ();

	if ((_SmsOpcode = IceRegisterForProtocolReply ("XSMP",
	    vendor, release, _SmVersionCount, _SmsVersions,
	    _SmAuthCount, _SmAuthNames, _SmsAuthProcs, hostBasedAuthProc,
	    _SmsProtocolSetupProc,
	    NULL,	/* IceProtocolActivateProc - we don't care about
			   when the Protocol Reply is sent, because the
			   session manager can not immediately send a
			   message - it must wait for RegisterClient. */
	    NULL	/* IceIOErrorProc */
            )) < 0)
	{
	    strncpy (errorStringRet,
	        "Could not register XSMP protocol with ICE", errorLength);

	    return (0);
	}
    }

    _SmsNewClientProc = newClientProc;
    _SmsNewClientData = managerData;

    return (1);
}



Status
_SmsProtocolSetupProc (iceConn,
    majorVersion, minorVersion, vendor, release,
    clientDataRet, failureReasonRet)

IceConn    iceConn;
int	   majorVersion;
int	   minorVersion;
char  	   *vendor;
char 	   *release;
IcePointer *clientDataRet;
char	   **failureReasonRet;

{
    SmsConn  		smsConn;
    unsigned long 	mask;
    Status		status;

    /*
     * vendor/release are undefined for ProtocolSetup in XSMP.
     */

    if (vendor)
	free (vendor);
    if (release)
	free (release);


    /*
     * Allocate new SmsConn.
     */

    if ((smsConn = (SmsConn) malloc (sizeof (struct _SmsConn))) == NULL)
    {
	char *str = "Memory allocation failed";

	if ((*failureReasonRet = (char *) malloc (strlen (str) + 1)) != NULL)
	    strcpy (*failureReasonRet, str);

	return (0);
    }

    smsConn->iceConn = iceConn;
    smsConn->proto_major_version = majorVersion;
    smsConn->proto_minor_version = minorVersion;
    smsConn->client_id = NULL;

    smsConn->save_yourself_in_progress = False;
    smsConn->interaction_allowed = SmInteractStyleNone;
    smsConn->can_cancel_shutdown = False;
    smsConn->interact_in_progress = False;

    *clientDataRet = (IcePointer) smsConn;


    /*
     * Now give the session manager the new smsConn and get back the
     * callbacks to invoke when messages arrive from the client.
     *
     * In the future, we can use the mask return value to check
     * if the SM is expecting an older rev of SMlib.
     */

    bzero ((char *) &smsConn->callbacks, sizeof (SmsCallbacks));

    status = (*_SmsNewClientProc) (smsConn, _SmsNewClientData,
	&mask, &smsConn->callbacks, failureReasonRet);

    return (status);
}



char *
SmsClientHostName (smsConn)

SmsConn smsConn;

{
    return (_IceTransGetPeerNetworkId (smsConn->iceConn->trans_conn));
}



Status
SmsRegisterClientReply (smsConn, clientId)

SmsConn smsConn;
char	*clientId;

{
    IceConn			iceConn = smsConn->iceConn;
    int				extra;
    smRegisterClientReplyMsg 	*pMsg;
    char 			*pData;

    if ((smsConn->client_id = (char *) malloc (strlen (clientId) + 1)) == NULL)
    {
	return (0);
    }

    strcpy (smsConn->client_id, clientId);

    extra = ARRAY8_BYTES (strlen (clientId));

    IceGetHeaderExtra (iceConn, _SmsOpcode, SM_RegisterClientReply,
	SIZEOF (smRegisterClientReplyMsg), WORD64COUNT (extra),
	smRegisterClientReplyMsg, pMsg, pData);

    STORE_ARRAY8 (pData, strlen (clientId), clientId);

    IceFlush (iceConn);

    return (1);
}



void
SmsSaveYourself (smsConn, saveType, shutdown, interactStyle, fast)

SmsConn smsConn;
int	saveType;
Bool 	shutdown;
int	interactStyle;
Bool	fast;

{
    IceConn		iceConn = smsConn->iceConn;
    smSaveYourselfMsg	*pMsg;

    IceGetHeader (iceConn, _SmsOpcode, SM_SaveYourself,
	SIZEOF (smSaveYourselfMsg), smSaveYourselfMsg, pMsg);

    pMsg->saveType = saveType;
    pMsg->shutdown = shutdown;
    pMsg->interactStyle = interactStyle;
    pMsg->fast = fast;

    IceFlush (iceConn);

    smsConn->save_yourself_in_progress = True;

    if (interactStyle == SmInteractStyleNone ||
	interactStyle == SmInteractStyleErrors ||
	interactStyle == SmInteractStyleAny)
    {
	smsConn->interaction_allowed = interactStyle;
    }
    else
    {
	smsConn->interaction_allowed = SmInteractStyleNone;
    }

    smsConn->can_cancel_shutdown = shutdown &&
	(interactStyle == SmInteractStyleAny ||
	interactStyle == SmInteractStyleErrors);
}



void
SmsSaveYourselfPhase2 (smsConn)

SmsConn smsConn;

{
    IceConn	iceConn = smsConn->iceConn;

    IceSimpleMessage (iceConn, _SmsOpcode, SM_SaveYourselfPhase2);
    IceFlush (iceConn);
}



void
SmsInteract (smsConn)

SmsConn smsConn;

{
    IceConn	iceConn = smsConn->iceConn;

    IceSimpleMessage (iceConn, _SmsOpcode, SM_Interact);
    IceFlush (iceConn);

    smsConn->interact_in_progress = True;
}



void
SmsDie (smsConn)

SmsConn smsConn;

{
    IceConn	iceConn = smsConn->iceConn;

    IceSimpleMessage (iceConn, _SmsOpcode, SM_Die);
    IceFlush (iceConn);
}



void
SmsSaveComplete (smsConn)

SmsConn smsConn;

{
    IceConn	iceConn = smsConn->iceConn;

    IceSimpleMessage (iceConn, _SmsOpcode, SM_SaveComplete);
    IceFlush (iceConn);
}



void
SmsShutdownCancelled (smsConn)

SmsConn smsConn;

{
    IceConn	iceConn = smsConn->iceConn;

    IceSimpleMessage (iceConn, _SmsOpcode, SM_ShutdownCancelled);
    IceFlush (iceConn);

    smsConn->can_cancel_shutdown = False;
}



void
SmsReturnProperties (smsConn, numProps, props)

SmsConn	smsConn;
int	numProps;
SmProp  **props;

{
    IceConn			iceConn = smsConn->iceConn;
    int 			bytes;
    smPropertiesReplyMsg	*pMsg;
    char 			*pBuf;
    char			*pStart;

    IceGetHeader (iceConn, _SmsOpcode, SM_PropertiesReply,
	SIZEOF (smPropertiesReplyMsg), smPropertiesReplyMsg, pMsg);

    LISTOF_PROP_BYTES (numProps, props, bytes);
    pMsg->length += WORD64COUNT (bytes);

    pBuf = pStart = IceAllocScratch (iceConn, bytes);

    STORE_LISTOF_PROPERTY (pBuf, numProps, props);

    IceWriteData (iceConn, bytes, pStart);
    IceFlush (iceConn);
}



void
SmsCleanUp (smsConn)

SmsConn smsConn;

{
    IceProtocolShutdown (smsConn->iceConn, _SmsOpcode);

    if (smsConn->client_id)
	free (smsConn->client_id);

    free ((char *) smsConn);
}

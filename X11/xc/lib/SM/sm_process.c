/* $Xorg: sm_process.c,v 1.4 2001/02/09 02:03:30 xorgcvs Exp $ */

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
/* $XFree86: xc/lib/SM/sm_process.c,v 1.3 2003/05/27 22:26:25 tsi Exp $ */

/*
 * Author: Ralph Mor, X Consortium
 */

#include <X11/SM/SMlib.h>
#include "SMlibint.h"


/*
 * Check for bad length
 */

#define CHECK_SIZE_MATCH(_iceConn, _majorOp, _minorOp, _expected_len, _actual_len, _severity) \
    if ((((_actual_len) - SIZEOF (iceMsg)) >> 3) != _expected_len) \
    { \
       _IceErrorBadLength (_iceConn, _majorOp, _minorOp, _severity); \
       return; \
    }

#define CHECK_AT_LEAST_SIZE(_iceConn, _majorOp, _minorOp, _expected_len, _actual_len, _severity) \
    if ((((_actual_len) - SIZEOF (iceMsg)) >> 3) > _expected_len) \
    { \
       _IceErrorBadLength (_iceConn, _majorOp, _minorOp, _severity); \
       return; \
    }

#define CHECK_COMPLETE_SIZE(_iceConn, _majorOp, _minorOp, _expected_len, _actual_len, _pStart, _severity) \
    if (((PADDED_BYTES64((_actual_len)) - SIZEOF (iceMsg)) >> 3) \
        != _expected_len) \
    { \
       _IceErrorBadLength (_iceConn, _majorOp, _minorOp, _severity); \
       IceDisposeCompleteMessage (iceConn, _pStart); \
       return; \
    }



void
_SmcProcessMessage (iceConn, clientData, opcode,
    length, swap, replyWait, replyReadyRet)

IceConn		 iceConn;
IcePointer	 clientData;
int		 opcode;
unsigned long	 length;
Bool		 swap;
IceReplyWaitInfo *replyWait;
Bool		 *replyReadyRet;

{
    SmcConn	smcConn = (SmcConn) clientData;

    if (replyWait)
	*replyReadyRet = False;

    if (!smcConn->client_id &&
        opcode != SM_RegisterClientReply && opcode != SM_Error)
    {
	_IceReadSkip (iceConn, length << 3);

	_IceErrorBadState (iceConn, _SmcOpcode, opcode, IceFatalToProtocol);
	return;
    }

    switch (opcode)
    {
    case SM_Error:
    {
	iceErrorMsg 	*pMsg;
	char	    	*pData;

	CHECK_AT_LEAST_SIZE (iceConn, _SmcOpcode, opcode,
	    length, SIZEOF (iceErrorMsg), IceFatalToProtocol);

	IceReadCompleteMessage (iceConn, SIZEOF (iceErrorMsg),
	    iceErrorMsg, pMsg, pData);

	if (!IceValidIO (iceConn))
	{
	    IceDisposeCompleteMessage (iceConn, pData);
	    return;
	}

	if (swap)
	{
	    pMsg->errorClass = lswaps (pMsg->errorClass);
	    pMsg->offendingSequenceNum = lswapl (pMsg->offendingSequenceNum);
	}

	if (replyWait &&
	    replyWait->minor_opcode_of_request == SM_RegisterClient &&
            pMsg->errorClass == IceBadValue &&
	    pMsg->offendingMinorOpcode == SM_RegisterClient &&
	    pMsg->offendingSequenceNum == replyWait->sequence_of_request)
	{
	    /*
	     * For Register Client, the previous ID was bad.
	     */

	    _SmcRegisterClientReply *reply =
		(_SmcRegisterClientReply *) (replyWait->reply);

	    reply->status = 0;

	    *replyReadyRet = True;
	}
	else
	{
	    (*_SmcErrorHandler) (smcConn, swap,
		pMsg->offendingMinorOpcode,
	        pMsg->offendingSequenceNum,
		pMsg->errorClass, pMsg->severity,
		(SmPointer) pData);
	}

	IceDisposeCompleteMessage (iceConn, pData);
	break;
    }

    case SM_RegisterClientReply:

	if (!replyWait ||
	    replyWait->minor_opcode_of_request != SM_RegisterClient)
	{
	    _IceReadSkip (iceConn, length << 3);

	    _IceErrorBadState (iceConn, _SmcOpcode,
		SM_RegisterClientReply, IceFatalToProtocol);
	}
        else
	{
	    smRegisterClientReplyMsg 	*pMsg;
	    char			*pData, *pStart;
	    _SmcRegisterClientReply 	*reply = 
	        (_SmcRegisterClientReply *) (replyWait->reply);

#if 0 /* No-op */
	    CHECK_AT_LEAST_SIZE (iceConn, _SmcOpcode, opcode,
		length, SIZEOF (smRegisterClientReplyMsg), IceFatalToProtocol);
#endif

	    IceReadCompleteMessage (iceConn, SIZEOF (smRegisterClientReplyMsg),
		smRegisterClientReplyMsg, pMsg, pStart);

	    if (!IceValidIO (iceConn))
	    {
		IceDisposeCompleteMessage (iceConn, pStart);
		return;
	    }

	    pData = pStart;

	    SKIP_ARRAY8 (pData, swap);		/* client id */

	    CHECK_COMPLETE_SIZE (iceConn, _SmcOpcode, opcode,
		length, pData - pStart + SIZEOF (smRegisterClientReplyMsg),
		pStart, IceFatalToProtocol);

	    pData = pStart;

	    EXTRACT_ARRAY8_AS_STRING (pData, swap, reply->client_id);

	    reply->status = 1;
	    *replyReadyRet = True;

	    IceDisposeCompleteMessage (iceConn, pStart);
	}
	break;

    case SM_SaveYourself:
    {
	smSaveYourselfMsg 	*pMsg;
	unsigned char		errVal;
	int			errOffset = -1;

	CHECK_SIZE_MATCH (iceConn, _SmcOpcode, opcode,
	    length, SIZEOF (smSaveYourselfMsg),
	    IceFatalToProtocol);

	IceReadMessageHeader (iceConn, SIZEOF (smSaveYourselfMsg),
	    smSaveYourselfMsg, pMsg);

	if (!IceValidIO (iceConn))
	{
	    return;
	}

	if (pMsg->saveType != SmSaveGlobal &&
	    pMsg->saveType != SmSaveLocal &&
	    pMsg->saveType != SmSaveBoth)
	{
	    errVal = pMsg->saveType;
	    errOffset = 8;
	}
	else if (pMsg->shutdown != 1 && pMsg->shutdown != 0)
	{
	    errVal = pMsg->shutdown;
	    errOffset = 9;
	}
	else if (pMsg->interactStyle != SmInteractStyleNone &&
	    pMsg->interactStyle != SmInteractStyleErrors &&
	    pMsg->interactStyle != SmInteractStyleAny)
	{
	    errVal = pMsg->interactStyle;
	    errOffset = 10;
	}
	else if (pMsg->fast != 1 && pMsg->fast != 0)
	{
	    errVal = pMsg->fast;
	    errOffset = 11;
	}

	if (errOffset >= 0)
	{
	    _IceErrorBadValue (iceConn, _SmcOpcode,
	        SM_SaveYourself, errOffset, 1, (IcePointer) &errVal);
	}
	else
	{
	    (*smcConn->callbacks.save_yourself.callback) (smcConn,
	        smcConn->callbacks.save_yourself.client_data,
                pMsg->saveType, pMsg->shutdown,
		pMsg->interactStyle, pMsg->fast);

	    smcConn->save_yourself_in_progress = True;

	    if (pMsg->shutdown)
		smcConn->shutdown_in_progress = True;
	}
	break;
    }

    case SM_SaveYourselfPhase2:

	if (!smcConn->phase2_wait)
	{
	    _IceErrorBadState (iceConn, _SmcOpcode,
		SM_SaveYourselfPhase2, IceCanContinue);
	}
        else
	{
	    CHECK_SIZE_MATCH (iceConn, _SmcOpcode, opcode,
	        length, SIZEOF (smSaveYourselfPhase2Msg),
	        IceFatalToProtocol);

	    (*smcConn->phase2_wait->phase2_proc) (smcConn,
		smcConn->phase2_wait->client_data);

	    free ((char *) smcConn->phase2_wait);
	    smcConn->phase2_wait  = NULL;
	}
	break;

    case SM_Interact:

        if (!smcConn->interact_waits)
	{
	    _IceErrorBadState (iceConn, _SmcOpcode,
		SM_Interact, IceCanContinue);
	}
        else
	{
	    _SmcInteractWait *next = smcConn->interact_waits->next;

	    CHECK_SIZE_MATCH (iceConn, _SmcOpcode, opcode,
	        length, SIZEOF (smInteractMsg),
	        IceFatalToProtocol);

	    (*smcConn->interact_waits->interact_proc) (smcConn,
		smcConn->interact_waits->client_data);

	    free ((char *) smcConn->interact_waits);
	    smcConn->interact_waits = next;
	}
	break;

    case SM_SaveComplete:

	if (!smcConn->save_yourself_in_progress)
	{
	    _IceErrorBadState (iceConn, _SmcOpcode,
		SM_SaveComplete, IceCanContinue);
	}
	else
	{
	    CHECK_SIZE_MATCH (iceConn, _SmcOpcode, opcode,
	        length, SIZEOF (smSaveCompleteMsg),
	        IceFatalToProtocol);

	    smcConn->save_yourself_in_progress = False;

	    (*smcConn->callbacks.save_complete.callback) (smcConn,
	        smcConn->callbacks.save_complete.client_data);
	}
	break;

    case SM_Die:

	CHECK_SIZE_MATCH (iceConn, _SmcOpcode, opcode,
	    length, SIZEOF (smDieMsg),
	    IceFatalToProtocol);

	(*smcConn->callbacks.die.callback) (smcConn,
	    smcConn->callbacks.die.client_data);
	break;

    case SM_ShutdownCancelled:

	if (!smcConn->shutdown_in_progress)
	{
	    _IceErrorBadState (iceConn, _SmcOpcode,
		SM_ShutdownCancelled, IceCanContinue);
	}
	else
	{
	    CHECK_SIZE_MATCH (iceConn, _SmcOpcode, opcode,
	        length, SIZEOF (smShutdownCancelledMsg),
	        IceFatalToProtocol);

	    smcConn->shutdown_in_progress = False;

	    (*smcConn->callbacks.shutdown_cancelled.callback) (smcConn,
	        smcConn->callbacks.shutdown_cancelled.client_data);
	}
	break;

    case SM_PropertiesReply:

        if (!smcConn->prop_reply_waits)
	{
	    _IceReadSkip (iceConn, length << 3);

	    _IceErrorBadState (iceConn, _SmcOpcode,
		SM_PropertiesReply, IceCanContinue);
	}
        else
	{
	    smPropertiesReplyMsg 	*pMsg;
	    char 			*pData, *pStart;
	    int				numProps;
	    SmProp			**props = NULL;
	    _SmcPropReplyWait 		*next;

#if 0 /* No-op */
	    CHECK_AT_LEAST_SIZE (iceConn, _SmcOpcode, opcode,
		length, SIZEOF (smPropertiesReplyMsg), IceFatalToProtocol);
#endif

	    IceReadCompleteMessage (iceConn, SIZEOF (smPropertiesReplyMsg),
		smPropertiesReplyMsg, pMsg, pStart);

	    if (!IceValidIO (iceConn))
	    {
		IceDisposeCompleteMessage (iceConn, pStart);
		return;
	    }

	    pData = pStart;

	    SKIP_LISTOF_PROPERTY (pData, swap);

	    CHECK_COMPLETE_SIZE (iceConn, _SmcOpcode, opcode,
	        length, pData - pStart + SIZEOF (smPropertiesReplyMsg),
	        pStart, IceFatalToProtocol);

	    pData = pStart;

	    EXTRACT_LISTOF_PROPERTY (pData, swap, numProps, props);

	    next = smcConn->prop_reply_waits->next;

	    (*smcConn->prop_reply_waits->prop_reply_proc) (smcConn,
		smcConn->prop_reply_waits->client_data, numProps, props);

	    free ((char *) smcConn->prop_reply_waits);
	    smcConn->prop_reply_waits = next;

	    IceDisposeCompleteMessage (iceConn, pStart);
	}
	break;

    default:
    {
	_IceErrorBadMinor (iceConn, _SmcOpcode, opcode, IceCanContinue);
	_IceReadSkip (iceConn, length << 3);
	break;
    }
    }
}



void
_SmsProcessMessage (iceConn, clientData, opcode, length, swap)

IceConn		 iceConn;
IcePointer       clientData;
int		 opcode;
unsigned long	 length;
Bool		 swap;

{
    SmsConn	smsConn = (SmsConn) clientData;

    if (!smsConn->client_id &&
        opcode != SM_RegisterClient && opcode != SM_Error)
    {
	_IceReadSkip (iceConn, length << 3);

	_IceErrorBadState (iceConn, _SmsOpcode, opcode, IceFatalToProtocol);

	return;
    }

    switch (opcode)
    {
    case SM_Error:
    {
	iceErrorMsg 	*pMsg;
	char	    	*pData;

	CHECK_AT_LEAST_SIZE (iceConn, _SmsOpcode, opcode,
	    length, SIZEOF (iceErrorMsg), IceFatalToProtocol);

	IceReadCompleteMessage (iceConn, SIZEOF (iceErrorMsg),
	    iceErrorMsg, pMsg, pData);

	if (!IceValidIO (iceConn))
	{
	    IceDisposeCompleteMessage (iceConn, pData);
	    return;
	}

	if (swap)
	{
	    pMsg->errorClass = lswaps (pMsg->errorClass);
	    pMsg->offendingSequenceNum = lswapl (pMsg->offendingSequenceNum);
	}

	(*_SmsErrorHandler) (smsConn, swap,
	    pMsg->offendingMinorOpcode,
	    pMsg->offendingSequenceNum,
	    pMsg->errorClass, pMsg->severity,
            (SmPointer) pData);

	IceDisposeCompleteMessage (iceConn, pData);
	break;
    }

    case SM_RegisterClient:
    {
	smRegisterClientMsg 	*pMsg;
	char 			*pData, *pStart;
	char 			*previousId;

#if 0 /* No-op */
	CHECK_AT_LEAST_SIZE (iceConn, _SmsOpcode, opcode,
	    length, SIZEOF (smRegisterClientMsg), IceFatalToProtocol);
#endif

	IceReadCompleteMessage (iceConn, SIZEOF (smRegisterClientMsg),
	    smRegisterClientMsg, pMsg, pStart);

	if (!IceValidIO (iceConn))
	{
	    IceDisposeCompleteMessage (iceConn, pStart);
	    return;
	}

	pData = pStart;

	SKIP_ARRAY8 (pData, swap);	/* previous id */

	CHECK_COMPLETE_SIZE (iceConn, _SmsOpcode, opcode,
	   length, pData - pStart + SIZEOF (smRegisterClientMsg),
	   pStart, IceFatalToProtocol);

	pData = pStart;

	EXTRACT_ARRAY8_AS_STRING (pData, swap, previousId);

	if (*previousId == '\0')
	{
	    free (previousId);
	    previousId = NULL;
	}

	if (!(*smsConn->callbacks.register_client.callback) (smsConn,
            smsConn->callbacks.register_client.manager_data, previousId))
	{
	    /*
	     * The previoudId was bad.  Generate BadValue error.
	     */

	    int length = previousId ? strlen (previousId) : 0;
	    int bytes = ARRAY8_BYTES (length);

	    _IceErrorBadValue (smsConn->iceConn, _SmsOpcode, SM_RegisterClient,
		8, bytes, (IcePointer) pStart);
	}

	IceDisposeCompleteMessage (iceConn, pStart);
	break;
    }

    case SM_InteractRequest:

        if (!smsConn->save_yourself_in_progress ||
	    smsConn->interaction_allowed == SmInteractStyleNone)
	{
	    _IceErrorBadState (iceConn, _SmsOpcode,
		SM_InteractRequest, IceCanContinue);
	}
        else
	{
	    smInteractRequestMsg 	*pMsg;

	    CHECK_SIZE_MATCH (iceConn, _SmsOpcode, opcode,
	        length, SIZEOF (smInteractRequestMsg),
	        IceFatalToProtocol);

	    IceReadSimpleMessage (iceConn, smInteractRequestMsg, pMsg);

	    if (pMsg->dialogType != SmDialogNormal &&
		pMsg->dialogType != SmDialogError)
	    {
		unsigned char errVal = pMsg->dialogType;

		_IceErrorBadValue (iceConn, _SmsOpcode,
	            SM_InteractRequest, 2, 1, (IcePointer) &errVal);
	    }
	    else if (pMsg->dialogType == SmDialogNormal &&
		smsConn->interaction_allowed != SmInteractStyleAny)
	    {
		_IceErrorBadState (iceConn, _SmsOpcode,
		    SM_InteractRequest, IceCanContinue);
	    }
	    else
	    {
		(*smsConn->callbacks.interact_request.callback) (smsConn,
	            smsConn->callbacks.interact_request.manager_data,
		    pMsg->dialogType);
	    }
	}
	break;

    case SM_InteractDone:

        if (!smsConn->interact_in_progress)
	{
	    _IceErrorBadState (iceConn, _SmsOpcode,
		SM_InteractDone, IceCanContinue);
	}
        else
	{
	    smInteractDoneMsg 	*pMsg;

	    CHECK_SIZE_MATCH (iceConn, _SmsOpcode, opcode,
	        length, SIZEOF (smInteractDoneMsg),
	        IceFatalToProtocol);

	    IceReadSimpleMessage (iceConn, smInteractDoneMsg, pMsg);

	    if (pMsg->cancelShutdown != 1 &&
		pMsg->cancelShutdown != 0)
	    {
		unsigned char errVal = pMsg->cancelShutdown;

		_IceErrorBadValue (iceConn, _SmsOpcode,
	            SM_InteractDone, 2, 1, (IcePointer) &errVal);
	    }
	    else if (pMsg->cancelShutdown && !smsConn->can_cancel_shutdown)
	    {
		_IceErrorBadState (iceConn, _SmsOpcode,
		    SM_InteractDone, IceCanContinue);
	    }
	    else
	    {
		smsConn->interact_in_progress = False;

		(*smsConn->callbacks.interact_done.callback) (smsConn,
	            smsConn->callbacks.interact_done.manager_data,
	            pMsg->cancelShutdown);
	    }
	}
	break;

    case SM_SaveYourselfRequest:
    {
	smSaveYourselfRequestMsg 	*pMsg;
	unsigned char			errVal;
	int				errOffset = -1;

	CHECK_SIZE_MATCH (iceConn, _SmsOpcode, opcode,
	    length, SIZEOF (smSaveYourselfRequestMsg),
	    IceFatalToProtocol);

	IceReadMessageHeader (iceConn, SIZEOF (smSaveYourselfRequestMsg),
	    smSaveYourselfRequestMsg, pMsg);

	if (!IceValidIO (iceConn))
	{
	    IceDisposeCompleteMessage (iceConn, pMsg);
	    return;
	}

	if (pMsg->saveType != SmSaveGlobal &&
	    pMsg->saveType != SmSaveLocal &&
	    pMsg->saveType != SmSaveBoth)
	{
	    errVal = pMsg->saveType;
	    errOffset = 8;
	}
	else if (pMsg->shutdown != 1 && pMsg->shutdown != 0)
	{
	    errVal = pMsg->shutdown;
	    errOffset = 9;
	}
	else if (pMsg->interactStyle != SmInteractStyleNone &&
	    pMsg->interactStyle != SmInteractStyleErrors &&
	    pMsg->interactStyle != SmInteractStyleAny)
	{
	    errVal = pMsg->interactStyle;
	    errOffset = 10;
	}
	else if (pMsg->fast != 1 && pMsg->fast != 0)
	{
	    errVal = pMsg->fast;
	    errOffset = 11;
	}
	else if (pMsg->global != 1 && pMsg->global != 0)
	{
	    errVal = pMsg->fast;
	    errOffset = 11;
	}

	if (errOffset >= 0)
	{
	    _IceErrorBadValue (iceConn, _SmsOpcode,
	        SM_SaveYourselfRequest, errOffset, 1, (IcePointer) &errVal);
	}
	else
	{
	    (*smsConn->callbacks.save_yourself_request.callback) (smsConn,
	        smsConn->callbacks.save_yourself_request.manager_data,
                pMsg->saveType, pMsg->shutdown, pMsg->interactStyle,
	        pMsg->fast, pMsg->global);
	}
	break;
    }

    case SM_SaveYourselfPhase2Request:

        if (!smsConn->save_yourself_in_progress)
	{
	    _IceErrorBadState (iceConn, _SmsOpcode,
		SM_SaveYourselfPhase2Request, IceCanContinue);
	}
        else
	{
	    CHECK_SIZE_MATCH (iceConn, _SmsOpcode, opcode,
	        length, SIZEOF (smSaveYourselfPhase2RequestMsg),
	        IceFatalToProtocol);

	    (*smsConn->callbacks.save_yourself_phase2_request.callback) (
		smsConn, smsConn->callbacks.
		save_yourself_phase2_request.manager_data);
	}
	break;

    case SM_SaveYourselfDone:

        if (!smsConn->save_yourself_in_progress)
	{
	    _IceErrorBadState (iceConn, _SmsOpcode,
		SM_SaveYourselfDone, IceCanContinue);
	}
        else
	{
	    smSaveYourselfDoneMsg 	*pMsg;

	    CHECK_SIZE_MATCH (iceConn, _SmsOpcode, opcode,
	        length, SIZEOF (smSaveYourselfDoneMsg),
	        IceFatalToProtocol);

	    IceReadSimpleMessage (iceConn, smSaveYourselfDoneMsg, pMsg);

	    if (pMsg->success != 1 && pMsg->success != 0)
	    {
		unsigned char errVal = pMsg->success;

		_IceErrorBadValue (iceConn, _SmsOpcode,
	            SM_SaveYourselfDone, 2, 1, (IcePointer) &errVal);
	    }
	    else
	    {
		smsConn->save_yourself_in_progress = False;
		smsConn->interaction_allowed = SmInteractStyleNone;

		(*smsConn->callbacks.save_yourself_done.callback) (smsConn,
	            smsConn->callbacks.save_yourself_done.manager_data,
		    pMsg->success);
	    }
	}
	break;

    case SM_CloseConnection:
    {
	smCloseConnectionMsg 	*pMsg;
	char 			*pData, *pStart;
	int 			count, i;
	char 			**reasonMsgs = NULL;

#if 0 /* No-op */
	CHECK_AT_LEAST_SIZE (iceConn, _SmsOpcode, opcode,
	    length, SIZEOF (smCloseConnectionMsg), IceFatalToProtocol);
#endif

	IceReadCompleteMessage (iceConn, SIZEOF (smCloseConnectionMsg),
	    smCloseConnectionMsg, pMsg, pStart);

	if (!IceValidIO (iceConn))
	{
	    IceDisposeCompleteMessage (iceConn, pStart);
	    return;
	}

	pData = pStart;

	EXTRACT_CARD32 (pData, swap, count);
	pData += 4;

	for (i = 0; i < count; i++)
	    SKIP_ARRAY8 (pData, swap);

	CHECK_COMPLETE_SIZE (iceConn, _SmsOpcode, opcode,
	   length, pData - pStart + SIZEOF (smCloseConnectionMsg),
	   pStart, IceFatalToProtocol);

	pData = pStart + 8;

	reasonMsgs = (char **) malloc (count * sizeof (char *));
	for (i = 0; i < count; i++)
	    EXTRACT_ARRAY8_AS_STRING (pData, swap, reasonMsgs[i]);

	IceDisposeCompleteMessage (iceConn, pStart);

	(*smsConn->callbacks.close_connection.callback) (smsConn,
	    smsConn->callbacks.close_connection.manager_data,
	    count, reasonMsgs);
	break;
    }

    case SM_SetProperties:
    {
	smSetPropertiesMsg 	*pMsg;
	char 			*pData, *pStart;
	SmProp			**props = NULL;
	int 			numProps;
	
#if 0 /* No-op */
	CHECK_AT_LEAST_SIZE (iceConn, _SmsOpcode, opcode,
	    length, SIZEOF (smSetPropertiesMsg), IceFatalToProtocol);
#endif

	IceReadCompleteMessage (iceConn, SIZEOF (smSetPropertiesMsg),
	    smSetPropertiesMsg, pMsg, pStart);

	if (!IceValidIO (iceConn))
	{
	    IceDisposeCompleteMessage (iceConn, pStart);
	    return;
	}

	pData = pStart;

	SKIP_LISTOF_PROPERTY (pData, swap);

	CHECK_COMPLETE_SIZE (iceConn, _SmsOpcode, opcode,
	   length, pData - pStart + SIZEOF (smSetPropertiesMsg),
	   pStart, IceFatalToProtocol);

	pData = pStart;

	EXTRACT_LISTOF_PROPERTY (pData, swap, numProps, props);

	(*smsConn->callbacks.set_properties.callback) (smsConn,
	    smsConn->callbacks.set_properties.manager_data, numProps, props);

	IceDisposeCompleteMessage (iceConn, pStart);
	break;
    }

    case SM_DeleteProperties:
    {
	smDeletePropertiesMsg 	*pMsg;
	char 			*pData, *pStart;
	int 			count, i;
	char 			**propNames = NULL;

#if 0 /* No-op */
	CHECK_AT_LEAST_SIZE (iceConn, _SmsOpcode, opcode,
	    length, SIZEOF (smDeletePropertiesMsg), IceFatalToProtocol);
#endif

	IceReadCompleteMessage (iceConn, SIZEOF (smDeletePropertiesMsg),
	    smDeletePropertiesMsg, pMsg, pStart);

	if (!IceValidIO (iceConn))
	{
	    IceDisposeCompleteMessage (iceConn, pStart);
	    return;
	}

	pData = pStart;

	EXTRACT_CARD32 (pData, swap, count);
	pData += 4;

	for (i = 0; i < count; i++)
	    SKIP_ARRAY8 (pData, swap);	/* prop names */

	CHECK_COMPLETE_SIZE (iceConn, _SmsOpcode, opcode,
	   length, pData - pStart + SIZEOF (smDeletePropertiesMsg),
	   pStart, IceFatalToProtocol);

	pData = pStart + 8;

	propNames = (char **) malloc (count * sizeof (char *));
	for (i = 0; i < count; i++)
	    EXTRACT_ARRAY8_AS_STRING (pData, swap, propNames[i]);

	IceDisposeCompleteMessage (iceConn, pStart);

	(*smsConn->callbacks.delete_properties.callback) (smsConn,
	    smsConn->callbacks.delete_properties.manager_data,
	    count, propNames);

	break;
    }

    case SM_GetProperties:

	CHECK_SIZE_MATCH (iceConn, _SmsOpcode, opcode,
	    length, SIZEOF (smGetPropertiesMsg),
	    IceFatalToProtocol);

	(*smsConn->callbacks.get_properties.callback) (smsConn,
	    smsConn->callbacks.get_properties.manager_data);
	break;

    default:
    {
	_IceErrorBadMinor (iceConn, _SmsOpcode, opcode, IceCanContinue);
	_IceReadSkip (iceConn, length << 3);
	break;
    }
    }
}

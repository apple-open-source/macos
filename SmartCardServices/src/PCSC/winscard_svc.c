/*
 *  Copyright (c) 2000-2007 Apple Inc. All Rights Reserved.
 * 
 *  @APPLE_LICENSE_HEADER_START@
 *  
 *  This file contains Original Code and/or Modifications of Original Code
 *  as defined in and that are subject to the Apple Public Source License
 *  Version 2.0 (the 'License'). You may not use this file except in
 *  compliance with the License. Please obtain a copy of the License at
 *  http://www.opensource.apple.com/apsl/ and read it before using this
 *  file.
 *  
 *  The Original Code and all software distributed under the License are
 *  distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 *  EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 *  INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 *  Please see the License for the specific language governing rights and
 *  limitations under the License.
 *  
 *  @APPLE_LICENSE_HEADER_END@
 */

/*
 *  winscard_svc.c
 *  SmartCardServices
 */

/*
 * MUSCLE SmartCard Development ( http://www.linuxnet.com )
 *
 * Copyright (C) 2001-2004
 *  David Corcoran <corcoran@linuxnet.com>
 *  Damien Sauveron <damien.sauveron@labri.fr>
 *  Ludovic Rousseau <ludovic.rousseau@free.fr>
 *
 * $Id: winscard_svc.c 2377 2007-02-05 13:13:56Z rousseau $
 */

/**
 * @file
 * @brief This demarshalls functions over the message queue and keeps
 * track of clients and their handles.
 *
 * Each Client message is deald by creating a thread (\c CreateContextThread).
 * The thread establishes reands and demarshalls the message and calls the
 * appropriate function to threat it.
 */

#include "config.h"
#include <time.h>
#include <stdio.h>
#include <string.h>

#include "wintypes.h"
#include "pcsclite.h"
#include "winscard.h"
#include "debuglog.h"
#include "winscard_msg.h"
#include "winscard_svc.h"
#include "sys_generic.h"
#include "thread_generic.h"
#include "readerfactory.h"
#include "hotplug.h"

#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/ioctl.h>
#include <stdlib.h>

/**
 * @brief Represents the an Application Context on the Server side.
 *
 * An Application Context contains Channels (\c hCard).
 */
static struct _psContext
{
	SCARDCONTEXT hContext;
	SCARDHANDLE hCard[PCSCLITE_MAX_APPLICATION_CONTEXT_CHANNELS];
	DWORD dwClientID;			/* Connection ID used to reference the Client. */
	PCSCLITE_THREAD_T pthThread;		/* Event polling thread's ID */
	sharedSegmentMsg msgStruct;		/* Msg sent by the Client */
	int protocol_major, protocol_minor;	/* Protocol number agreed between client and server*/
} psContext[PCSCLITE_MAX_APPLICATIONS_CONTEXTS];

LONG MSGCheckHandleAssociation(SCARDHANDLE, DWORD);
LONG MSGFunctionDemarshall(psharedSegmentMsg, DWORD, uint32_t *replySize);
LONG MSGAddContext(SCARDCONTEXT, DWORD);
LONG MSGRemoveContext(SCARDCONTEXT, DWORD);
LONG MSGAddHandle(SCARDCONTEXT, SCARDHANDLE, DWORD);
LONG MSGRemoveHandle(SCARDHANDLE, DWORD);
LONG MSGCleanupClient(DWORD);

static void ContextThread(LPVOID pdwIndex);

LONG ContextsInitialize(void)
{
	memset(psContext, 0, sizeof(struct _psContext)*PCSCLITE_MAX_APPLICATIONS_CONTEXTS);
	return 1;
}

/**
 * @brief Creates threads to handle messages received from Clients.
 *
 * @param[in] pdwClientID Connection ID used to reference the Client.
 *
 * @return Error code.
 * @retval SCARD_S_SUCCESS Success.
 * @retval SCARD_F_INTERNAL_ERROR Exceded the maximum number of simultaneous Application Contexts.
 * @retval SCARD_E_NO_MEMORY Error creating the Context Thread.
 */
LONG CreateContextThread(PDWORD pdwClientID)
{
	int i;

	for (i = 0; i < PCSCLITE_MAX_APPLICATIONS_CONTEXTS; i++)
	{
		if (psContext[i].dwClientID == 0)
		{
			psContext[i].dwClientID = *pdwClientID;
			*pdwClientID = 0;
			break;
		}
	}

	if (i == PCSCLITE_MAX_APPLICATIONS_CONTEXTS)
	{
		SYS_CloseFile(psContext[i].dwClientID);
		psContext[i].dwClientID = 0;
		Log2(PCSC_LOG_CRITICAL, "No more context available (max: %d)",
			PCSCLITE_MAX_APPLICATIONS_CONTEXTS);
		return SCARD_F_INTERNAL_ERROR;
	}

	if (SYS_ThreadCreate(&psContext[i].pthThread, THREAD_ATTR_DETACHED,
		(PCSCLITE_THREAD_FUNCTION( )) ContextThread,
		(LPVOID) i) != 1)
	{
		SYS_CloseFile(psContext[i].dwClientID);
		psContext[i].dwClientID = 0;
		Log1(PCSC_LOG_CRITICAL, "SYS_ThreadCreate failed");
		return SCARD_E_NO_MEMORY;
	}

	return SCARD_S_SUCCESS;
}

/*
 * A list of local functions used to keep track of clients and their
 * connections
 */

/**
 * @brief Handles messages received from Clients.
 *
 * For each Client message a new instance of this thread is created.
 *
 * @param[in] dwIndex Index of an avaiable Application Context slot in
 * \c psContext.
 */
 
/*
	To handle the possible case where the client is one architecture and the server is another
	(e.g. a PPC app running through Rosetta on OS X talking to a native i386 pcscd), we convert
	everything going OUT over the pipe to network byte order. Conversely, everything coming IN
	over the pipe is converted to host byte order.
*/

static void ContextThread(LPVOID dwIndex)
{
	LONG rv;
	DWORD dwContextIndex = (DWORD)dwIndex;

	Log2(PCSC_LOG_DEBUG, "Thread is started: %d",
		psContext[dwContextIndex].dwClientID);

	while (1)
	{
		sharedSegmentMsg msgStruct = {0,};
		
		systemAwakeAndReadyCheck();

		/*
			Note: SHSharedSegmentMsgToHostOrder(&msgStruct) was called in SHMProcessEventsContext
			This means that msgStruct contains host-order fields
		*/
		switch (rv = SHMProcessEventsContext(&psContext[dwContextIndex].dwClientID, &msgStruct, 0))
		{
		case 0:
			if (msgStruct.mtype == CMD_CLIENT_DIED)
			{
				/*
				 * Clean up the dead client
				 */
				Log2(PCSC_LOG_DEBUG, "Client die: %d",
					psContext[dwContextIndex].dwClientID);
				MSGCleanupClient(dwContextIndex);
				SYS_ThreadExit((LPVOID) NULL);
			}
			break;

		case 1:
			if (msgStruct.mtype == CMD_FUNCTION)
			{
				/*
				 * Command must be found
				 */
				uint32_t replySize = 0;
				MSGFunctionDemarshall(&msgStruct, dwContextIndex, &replySize);

				/* the SCARD_TRANSMIT_EXTENDED anwser is already sent by
				 * MSGFunctionDemarshall */
				if ((msgStruct.command != SCARD_TRANSMIT_EXTENDED)
					&& (msgStruct.command != SCARD_CONTROL_EXTENDED))
				{
					sharedSegmentMsg tmpMsgStruct;
					replySize += (sizeof(sharedSegmentMsg) - sizeof(msgStruct.data));
					memcpy(&tmpMsgStruct, &msgStruct, replySize);
					SHSharedSegmentMsgToNetworkOrder(&tmpMsgStruct);
					rv = SHMMessageSend(&tmpMsgStruct, replySize,
						psContext[dwContextIndex].dwClientID,
						SHMCommunicationTimeout());
				}
			}
			else
				/* pcsc-lite client/server protocol version */
				if (msgStruct.mtype == CMD_VERSION)
				{
					version_struct *veStr;
					veStr = (version_struct *) msgStruct.data;
					ntohlVersionStruct(veStr);
					
					/* get the client protocol version */
					psContext[dwContextIndex].protocol_major = veStr->major;
					psContext[dwContextIndex].protocol_minor = veStr->minor;

					Log3(PCSC_LOG_DEBUG,
						"Client is protocol version %d:%d",
						veStr->major, veStr->minor);

					veStr->rv = SCARD_S_SUCCESS;

					/* client is newer than server */
					if ((veStr->major > PROTOCOL_VERSION_MAJOR)
						|| (veStr->major == PROTOCOL_VERSION_MAJOR
							&& veStr->minor > PROTOCOL_VERSION_MINOR))
					{
						Log3(PCSC_LOG_CRITICAL,
							"Client protocol is too new %d:%d",
							veStr->major, veStr->minor);
						Log3(PCSC_LOG_CRITICAL,
							"Server protocol is %d:%d",
							PROTOCOL_VERSION_MAJOR, PROTOCOL_VERSION_MINOR);
						veStr->rv = SCARD_E_NO_SERVICE;
					}

					/* set the server protocol version */
					veStr->major = PROTOCOL_VERSION_MAJOR;
					veStr->minor = PROTOCOL_VERSION_MINOR;
					htonlVersionStruct(veStr);
					
					/* send back the response */
					sharedSegmentMsg tmpMsgStruct = msgStruct;
					SHSharedSegmentMsgToNetworkOrder(&tmpMsgStruct);
					rv = SHMMessageSend(&tmpMsgStruct, SHMCalculateMessageSize(sizeof(version_struct)),
						psContext[dwContextIndex].dwClientID,
					    SHMCommunicationTimeout());
				}
				else
					continue;

			break;

		case 2:
			/*
			 * timeout in SHMProcessEventsContext(): do nothing
			 * this is used to catch the Ctrl-C signal at some time when
			 * nothing else happens
			 */
			break;

		case -1:
			Log1(PCSC_LOG_ERROR, "Error in SHMProcessEventsContext");
			break;

		default:
			Log2(PCSC_LOG_ERROR,
				"SHMProcessEventsContext unknown retval: %d", rv);
			break;
		}
	}
}

/**
 * @brief Find out which message was sent by the Client and execute the right task.
 *
 * According to the command type sent by the client (\c pcsc_msg_commands),
 * cast the message data to the correct struct so that is can be demarshalled.
 * Then call the appropriate function to handle the request.
 *
 * Possible structs are: \c establish_struct \c release_struct
 * \c connect_struct \c reconnect_struct \c disconnect_struct \c begin_struct
 * \c cancel_struct \c end_struct \c status_struct \c transmit_struct
 * \c control_struct \c getset_struct.
 *
 * @param[in] msgStruct Message to be demarshalled and executed.
 * @param[in] dwContextIndex
 */
LONG MSGFunctionDemarshall(psharedSegmentMsg msgStruct, DWORD dwContextIndex, uint32_t *replySize)
{
	LONG rv;
	establish_struct *esStr;
	release_struct *reStr;
	connect_struct *coStr;
	reconnect_struct *rcStr;
	disconnect_struct *diStr;
	begin_struct *beStr;
	cancel_struct *caStr;
	end_struct *enStr;
	status_struct *stStr;
	transmit_struct *trStr;
	control_struct *ctStr;
	getset_struct *gsStr;

	/*
	 * Zero out everything
	 */
	rv = 0;
	*replySize = 0;

	/*
		Note that we need to convert structs back out to network byte order
		after the various calls are made, as this is how results are passed back
		to the client
	*/
	switch (msgStruct->command)
	{

	case SCARD_ESTABLISH_CONTEXT:
		esStr = ((establish_struct *) msgStruct->data);
		ntohlEstablishStruct(esStr);
		esStr->rv = SCardEstablishContext(esStr->dwScope, 0, 0,
			&esStr->phContext);

		if (esStr->rv == SCARD_S_SUCCESS)
			esStr->rv =
				MSGAddContext(esStr->phContext, dwContextIndex);
		htonlEstablishStruct(esStr);
		*replySize = sizeof(establish_struct);
		break;

	case SCARD_RELEASE_CONTEXT:
		reStr = ((release_struct *) msgStruct->data);
		ntohlReleaseStruct(reStr);

		reStr->rv = SCardReleaseContext(reStr->hContext);

		if (reStr->rv == SCARD_S_SUCCESS)
			reStr->rv =
				MSGRemoveContext(reStr->hContext, dwContextIndex);

		htonlReleaseStruct(reStr);
		*replySize = sizeof(release_struct);
		break;

	case SCARD_CONNECT:
		coStr = ((connect_struct *) msgStruct->data);
		ntohlConnectStruct(coStr);
		Log3(PCSC_LOG_DEBUG, "SCardConnect hContext: 0x%08X, phCard: 0x%08X", coStr->hContext, coStr->phCard);
		coStr->rv = SCardConnect(coStr->hContext, coStr->szReader,
			coStr->dwShareMode, coStr->dwPreferredProtocols,
			&coStr->phCard, &coStr->pdwActiveProtocol);
		Log3(PCSC_LOG_DEBUG, "SCardConnect result: %d [0x%08X]", coStr->rv, coStr->rv);

		if (coStr->rv == SCARD_S_SUCCESS)
		{
			coStr->rv =
				MSGAddHandle(coStr->hContext, coStr->phCard, dwContextIndex);
			Log3(PCSC_LOG_DEBUG, "MSGAddHandle result: %d [0x%08X]", coStr->rv, coStr->rv);
		}
		htonlConnectStruct(coStr);
		*replySize = sizeof(connect_struct);
		break;

	case SCARD_RECONNECT:
		rcStr = ((reconnect_struct *) msgStruct->data);
		ntohlReconnectStruct(rcStr);
		rv = MSGCheckHandleAssociation(rcStr->hCard, dwContextIndex);
		if (rv != 0) return rv;

		rcStr->rv = SCardReconnect(rcStr->hCard, rcStr->dwShareMode,
			rcStr->dwPreferredProtocols,
			rcStr->dwInitialization, &rcStr->pdwActiveProtocol);
		htonlReconnectStruct(rcStr);
		*replySize = sizeof(reconnect_struct);
		break;

	case SCARD_DISCONNECT:
		diStr = ((disconnect_struct *) msgStruct->data);
		ntohlDisconnectStruct(diStr);
		rv = MSGCheckHandleAssociation(diStr->hCard, dwContextIndex);
		if (rv != 0) return rv;
		diStr->rv = SCardDisconnect(diStr->hCard, diStr->dwDisposition);

		if (diStr->rv == SCARD_S_SUCCESS)
			diStr->rv =
				MSGRemoveHandle(diStr->hCard, dwContextIndex);
		htonlDisconnectStruct(diStr);
		*replySize = sizeof(disconnect_struct);
		break;

	case SCARD_BEGIN_TRANSACTION:
		{
		beStr = ((begin_struct *) msgStruct->data);
		int ix;
		unsigned char *px = &msgStruct->data[sizeof(begin_struct)];
		for (ix = 0; ix < 32; ++ix)
			*px++ = 0xEE;
		beStr->rv = -99;	// test
		ntohlBeginStruct(beStr);
		rv = MSGCheckHandleAssociation(beStr->hCard, dwContextIndex);
		if (rv != 0) return rv;
		beStr->rv = SCardBeginTransaction(beStr->hCard);
		htonlBeginStruct(beStr);
		}
		*replySize = sizeof(begin_struct);
		break;

	case SCARD_END_TRANSACTION:
		enStr = ((end_struct *) msgStruct->data);
		ntohlEndStruct(enStr);
		rv = MSGCheckHandleAssociation(enStr->hCard, dwContextIndex);
		if (rv != 0) return rv;
		enStr->rv =
			SCardEndTransaction(enStr->hCard, enStr->dwDisposition);
		htonlEndStruct(enStr);
		*replySize = sizeof(end_struct);
		break;

	case SCARD_CANCEL_TRANSACTION:
		caStr = ((cancel_struct *) msgStruct->data);
		ntohlCancelStruct(caStr);
		rv = MSGCheckHandleAssociation(caStr->hCard, dwContextIndex);
		if (rv != 0) return rv;
		caStr->rv = SCardCancelTransaction(caStr->hCard);
		htonlCancelStruct(caStr);
		*replySize = sizeof(cancel_struct);
		break;

	case SCARD_STATUS:
		stStr = ((status_struct *) msgStruct->data);
		ntohlStatusStruct(stStr);
		rv = MSGCheckHandleAssociation(stStr->hCard, dwContextIndex);
		if (rv != 0) return rv;
		stStr->rv = SCardStatus(stStr->hCard, stStr->mszReaderNames,
			&stStr->pcchReaderLen, &stStr->pdwState,
			&stStr->pdwProtocol, stStr->pbAtr, &stStr->pcbAtrLen);
		htonlStatusStruct(stStr);
		*replySize = sizeof(status_struct);
		break;

	case SCARD_TRANSMIT:
		trStr = ((transmit_struct *) msgStruct->data);
		ntohlTransmitStruct(trStr);
		Log2(PCSC_LOG_DEBUG, "SCardTransmit cbSendLength: %d", trStr->cbSendLength);
		rv = MSGCheckHandleAssociation(trStr->hCard, dwContextIndex);
		if (rv != 0) return rv;
		trStr->rv = SCardTransmit(trStr->hCard, &trStr->pioSendPci,
			trStr->pbSendBuffer, trStr->cbSendLength,
			&trStr->pioRecvPci, trStr->pbRecvBuffer,
			&trStr->pcbRecvLength);
		Log2(PCSC_LOG_DEBUG, "SCardTransmit pcbRecvLength: %d", trStr->pcbRecvLength);
		htonlTransmitStruct(trStr);
		*replySize = sizeof(transmit_struct);
		break;

	case SCARD_CONTROL:
		ctStr = ((control_struct *) msgStruct->data);
		ntohlControlStruct(ctStr);
		rv = MSGCheckHandleAssociation(ctStr->hCard, dwContextIndex);
		if (rv != 0) return rv;
		ctStr->rv = SCardControl(ctStr->hCard, ctStr->dwControlCode,
			ctStr->pbSendBuffer, ctStr->cbSendLength,
			ctStr->pbRecvBuffer, ctStr->cbRecvLength,
			&ctStr->dwBytesReturned);
		htonlControlStruct(ctStr);
		*replySize = sizeof(control_struct);
		break;

	case SCARD_GET_ATTRIB:
		gsStr = ((getset_struct *) msgStruct->data);
		ntohlGetSetStruct(gsStr);
		rv = MSGCheckHandleAssociation(gsStr->hCard, dwContextIndex);
		if (rv != 0) return rv;
		gsStr->rv = SCardGetAttrib(gsStr->hCard, gsStr->dwAttrId,
			gsStr->pbAttr, &gsStr->cbAttrLen);
		htonlGetSetStruct(gsStr);
		*replySize = sizeof(getset_struct);
		break;

	case SCARD_SET_ATTRIB:
		gsStr = ((getset_struct *) msgStruct->data);
		ntohlGetSetStruct(gsStr);
		rv = MSGCheckHandleAssociation(gsStr->hCard, dwContextIndex);
		if (rv != 0) return rv;
		gsStr->rv = SCardSetAttrib(gsStr->hCard, gsStr->dwAttrId,
			gsStr->pbAttr, gsStr->cbAttrLen);
		htonlGetSetStruct(gsStr);
		*replySize = sizeof(getset_struct);
		break;

	case SCARD_TRANSMIT_EXTENDED:
		{
			transmit_struct_extended *treStr;
			unsigned char pbSendBuffer[MAX_BUFFER_SIZE_EXTENDED];
			unsigned char pbRecvBuffer[MAX_BUFFER_SIZE_EXTENDED];

			treStr = ((transmit_struct_extended *) msgStruct->data);
			ntohlTransmitStructExtended(treStr);
			Log2(PCSC_LOG_DEBUG, "SCardTransmitExt cbSendLength: %d", treStr->cbSendLength);
			rv = MSGCheckHandleAssociation(treStr->hCard, dwContextIndex);
			if (rv != 0) return rv;

			/* one more block to read? */
			if (treStr->size > PCSCLITE_MAX_MESSAGE_SIZE)
			{
				/* copy the first APDU part */
				memcpy(pbSendBuffer, treStr->data,
					PCSCLITE_MAX_MESSAGE_SIZE-sizeof(*treStr));

				/* receive the second block */
				rv = SHMMessageReceive(
					pbSendBuffer+PCSCLITE_MAX_MESSAGE_SIZE-sizeof(*treStr),
					treStr->size - PCSCLITE_MAX_MESSAGE_SIZE,
					psContext[dwContextIndex].dwClientID,
					SHMCommunicationTimeout());
				if (rv)
					Log1(PCSC_LOG_CRITICAL, "reception failed");
			}
			else
				memcpy(pbSendBuffer, treStr->data, treStr->cbSendLength);

			treStr->rv = SCardTransmit(treStr->hCard, &treStr->pioSendPci,
				pbSendBuffer, treStr->cbSendLength,
				&treStr->pioRecvPci, pbRecvBuffer,
				&treStr->pcbRecvLength);

			treStr->size = sizeof(*treStr) + treStr->pcbRecvLength;
			Log3(PCSC_LOG_DEBUG, "SCardTransmitExt pcbRecvLength: %d, size: %d", 
				treStr->pcbRecvLength, treStr->size);
			Log3(PCSC_LOG_DEBUG, "SCardTransmitExt SCardTransmit result: %d [0x%08X]", 
				treStr->rv, treStr->rv);
			if (treStr->size > PCSCLITE_MAX_MESSAGE_SIZE)
			{
				/* two blocks */
				memcpy(treStr->data, pbRecvBuffer, PCSCLITE_MAX_MESSAGE_SIZE
					- sizeof(*treStr));

			//	sharedSegmentMsg tmpMsgStruct = *msgStruct;
			//  we don't copy because of the size, and because it is not used after here
			//	SHSharedSegmentMsgToNetworkOrder(&tmpMsgStruct);
				SHSharedSegmentMsgToNetworkOrder(msgStruct);
				htonlTransmitStructExtended(treStr);
				rv = SHMMessageSend(msgStruct, sizeof(*msgStruct),
					psContext[dwContextIndex].dwClientID,
					SHMCommunicationTimeout());
				if (rv)
					Log1(PCSC_LOG_CRITICAL, "transmission failed");

				rv = SHMMessageSend(pbRecvBuffer + PCSCLITE_MAX_MESSAGE_SIZE
					- sizeof(*treStr),
					treStr->size - PCSCLITE_MAX_MESSAGE_SIZE,
					psContext[dwContextIndex].dwClientID,
					SHMCommunicationTimeout());
				if (rv)
					Log1(PCSC_LOG_CRITICAL, "transmission failed");
			}
			else
			{
				/* one block only */
				size_t dataSize = treStr->pcbRecvLength;
				memcpy(treStr->data, pbRecvBuffer, dataSize);
				
				// the 4 is to drop the "BYTE data[1]", which rounds to 4 bytes
				size_t replySize = dataSize + sizeof(transmit_struct_extended) - 4;	
				Log3(PCSC_LOG_DEBUG, "SCardTransmitExt/SHMMessageSend one block: data: %d, total: %d", 
					dataSize, replySize);
				htonlTransmitStructExtended(treStr);
				rv = WrapSHMWrite(SCARD_TRANSMIT_EXTENDED, psContext[dwContextIndex].dwClientID,
					replySize, SHMCommunicationTimeout(), treStr);
	
#if 0
				// the 4 is to drop the "BYTE data[1]", which rounds to 4 bytes
				size_t replySize = sizeof(sharedSegmentMsg) - sizeof(msgStruct->data) +	// header portion of msgStruct
					dataSize + sizeof(transmit_struct_extended) - 4;	

				Log3(PCSC_LOG_DEBUG, "SCardTransmitExt/SHMMessageSend one block: data: %d, total: %d", 
					dataSize, replySize);
				//  we don't copy because of the potential size
				SHSharedSegmentMsgToNetworkOrder(msgStruct);
				htonlTransmitStructExtended(treStr);
				rv = SHMMessageSend(msgStruct, replySize,
					psContext[dwContextIndex].dwClientID,
					SHMCommunicationTimeout());
#endif
				if (rv)
					Log1(PCSC_LOG_CRITICAL, "transmission failed");
				// We flip back the header, since the SHMProcessEventsContext loop 
				// tests msgStruct.command after MSGFunctionDemarshall is called
#if 0
				SHSharedSegmentMsgToHostOrder(msgStruct);
#endif
			}
		}
		break;

	case SCARD_CONTROL_EXTENDED:
		{
			control_struct_extended *cteStr;
			unsigned char pbSendBuffer[MAX_BUFFER_SIZE_EXTENDED];
			unsigned char pbRecvBuffer[MAX_BUFFER_SIZE_EXTENDED];

			cteStr = ((control_struct_extended *) msgStruct->data);
			ntohlControlStructExtended(cteStr);
			Log2(PCSC_LOG_DEBUG, "SCardControlExt cbSendLength: %d", cteStr->cbSendLength);
			rv = MSGCheckHandleAssociation(cteStr->hCard, dwContextIndex);
			if (rv != 0) return rv;

			/* one more block to read? */
			if (cteStr->size > PCSCLITE_MAX_MESSAGE_SIZE)
			{
				/* copy the first data part */
				memcpy(pbSendBuffer, cteStr->data,
					PCSCLITE_MAX_MESSAGE_SIZE-sizeof(*cteStr));

				/* receive the second block */
				rv = SHMMessageReceive(
					pbSendBuffer+PCSCLITE_MAX_MESSAGE_SIZE-sizeof(*cteStr),
					cteStr->size - PCSCLITE_MAX_MESSAGE_SIZE,
					psContext[dwContextIndex].dwClientID,
					SHMCommunicationTimeout());
				if (rv)
					Log1(PCSC_LOG_CRITICAL, "reception failed");
			}
			else
				memcpy(pbSendBuffer, cteStr->data, cteStr->cbSendLength);

			cteStr->rv = SCardControl(cteStr->hCard, cteStr->dwControlCode,
				pbSendBuffer, cteStr->cbSendLength,
				pbRecvBuffer, cteStr->cbRecvLength,
				&cteStr->pdwBytesReturned);

			cteStr->size = sizeof(*cteStr) + cteStr->pdwBytesReturned;
			Log3(PCSC_LOG_DEBUG, "SCardControlExt pdwBytesReturned: %d, size: %d", 
				cteStr->pdwBytesReturned, cteStr->size);
			if (cteStr->size > PCSCLITE_MAX_MESSAGE_SIZE)
			{
				/* two blocks */
				memcpy(cteStr->data, pbRecvBuffer, PCSCLITE_MAX_MESSAGE_SIZE
					- sizeof(*cteStr));

				sharedSegmentMsg tmpMsgStruct = *msgStruct;
				SHSharedSegmentMsgToNetworkOrder(&tmpMsgStruct);
				htonlControlStructExtended(cteStr);
				rv = SHMMessageSend(&tmpMsgStruct, sizeof(tmpMsgStruct),
					psContext[dwContextIndex].dwClientID,
					SHMCommunicationTimeout());
				if (rv)
					Log1(PCSC_LOG_CRITICAL, "transmission failed");

				rv = SHMMessageSend(pbRecvBuffer + PCSCLITE_MAX_MESSAGE_SIZE
					- sizeof(*cteStr),
					cteStr->size - PCSCLITE_MAX_MESSAGE_SIZE,
					psContext[dwContextIndex].dwClientID,
					SHMCommunicationTimeout());
				if (rv)
					Log1(PCSC_LOG_CRITICAL, "transmission failed");
			}
			else
			{
				/* one block only */
				size_t dataSize = cteStr->pdwBytesReturned;
				memcpy(cteStr->data, pbRecvBuffer, dataSize);

				sharedSegmentMsg tmpMsgStruct = *msgStruct;
				SHSharedSegmentMsgToNetworkOrder(&tmpMsgStruct);
				htonlControlStructExtended(cteStr);
				rv = SHMMessageSend(&tmpMsgStruct, SHMCalculateMessageSize(dataSize),
					psContext[dwContextIndex].dwClientID,
					SHMCommunicationTimeout());
				if (rv)
					Log1(PCSC_LOG_CRITICAL, "transmission failed");
			}
		}
		break;

	default:
		Log2(PCSC_LOG_CRITICAL, "Unknown command: %d", msgStruct->command);
		return -1;
	}

	return 0;
}

LONG MSGAddContext(SCARDCONTEXT hContext, DWORD dwContextIndex)
{
	psContext[dwContextIndex].hContext = hContext;
	return SCARD_S_SUCCESS;
}

LONG MSGRemoveContext(SCARDCONTEXT hContext, DWORD dwContextIndex)
{
	int i;
	LONG rv;

	if (psContext[dwContextIndex].hContext == hContext)
	{
		for (i = 0; i < PCSCLITE_MAX_APPLICATION_CONTEXT_CHANNELS; i++)
		{
			/*
			 * Disconnect each of these just in case
			 */

			if (psContext[dwContextIndex].hCard[i] != 0)
			{
				PREADER_CONTEXT rContext = NULL;
				DWORD dwLockId;

				/*
				 * Unlock the sharing
				 */
				rv = RFReaderInfoById(psContext[dwContextIndex].hCard[i],
					&rContext);
				if (rv != SCARD_S_SUCCESS)
					return rv;

				dwLockId = rContext->dwLockId;
				rContext->dwLockId = 0;

				if (psContext[dwContextIndex].hCard[i] != dwLockId) 
				{
					/*
					 * if the card is locked by someone else we do not reset it
					 * and simulate a card removal
					 */
					rv = SCARD_W_REMOVED_CARD;
				}
				else
				{
					/*
					 * We will use SCardStatus to see if the card has been
					 * reset there is no need to reset each time
					 * Disconnect is called
					 */
					rv = SCardStatus(psContext[dwContextIndex].hCard[i], NULL,
						NULL, NULL, NULL, NULL, NULL);
				}

				if (rv == SCARD_W_RESET_CARD || rv == SCARD_W_REMOVED_CARD)
					SCardDisconnect(psContext[dwContextIndex].hCard[i],
						SCARD_LEAVE_CARD);
				else
					SCardDisconnect(psContext[dwContextIndex].hCard[i],
						SCARD_RESET_CARD);

				psContext[dwContextIndex].hCard[i] = 0;
			}
		}

		psContext[dwContextIndex].hContext = 0;
		return SCARD_S_SUCCESS;
	}

	return SCARD_E_INVALID_VALUE;
}

LONG MSGAddHandle(SCARDCONTEXT hContext, SCARDHANDLE hCard, DWORD dwContextIndex)
{
	int i;

	if (psContext[dwContextIndex].hContext == hContext)
	{

		/*
		 * Find an empty spot to put the hCard value
		 */
		for (i = 0; i < PCSCLITE_MAX_APPLICATION_CONTEXT_CHANNELS; i++)
		{
			if (psContext[dwContextIndex].hCard[i] == 0)
			{
				psContext[dwContextIndex].hCard[i] = hCard;
				break;
			}
		}

		if (i == PCSCLITE_MAX_APPLICATION_CONTEXT_CHANNELS)
		{
			return SCARD_F_INTERNAL_ERROR;
		} else
		{
			return SCARD_S_SUCCESS;
		}

	}

	return SCARD_E_INVALID_VALUE;
}

LONG MSGRemoveHandle(SCARDHANDLE hCard, DWORD dwContextIndex)
{
	int i;

	for (i = 0; i < PCSCLITE_MAX_APPLICATION_CONTEXT_CHANNELS; i++)
	{
		if (psContext[dwContextIndex].hCard[i] == hCard)
		{
			psContext[dwContextIndex].hCard[i] = 0;
			return SCARD_S_SUCCESS;
		}
	}

	return SCARD_E_INVALID_VALUE;
}


LONG MSGCheckHandleAssociation(SCARDHANDLE hCard, DWORD dwContextIndex)
{
	int i;

	for (i = 0; i < PCSCLITE_MAX_APPLICATION_CONTEXT_CHANNELS; i++)
	{
		if (psContext[dwContextIndex].hCard[i] == hCard)
		{
			return 0;
		}
	}

	/* Must be a rogue client, debug log and sleep a couple of seconds */
	Log2(PCSC_LOG_ERROR, "Client failed to authenticate (hCard: 0x%08X)", hCard);
	SYS_Sleep(2);

	return SCARD_E_INVALID_HANDLE;
}

LONG MSGCleanupClient(DWORD dwContextIndex)
{
	if (psContext[dwContextIndex].hContext != 0)
	{
		SCardReleaseContext(psContext[dwContextIndex].hContext);
		MSGRemoveContext(psContext[dwContextIndex].hContext, dwContextIndex);
	}

	psContext[dwContextIndex].dwClientID = 0;
	psContext[dwContextIndex].protocol_major = 0;
	psContext[dwContextIndex].protocol_minor = 0;

	return 0;
}



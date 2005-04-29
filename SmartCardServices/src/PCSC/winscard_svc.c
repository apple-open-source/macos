/*
 * Copyright (c) 2000-2002 Apple Computer, Inc. All Rights Reserved.
 * The contents of this file constitute Original Code as defined in and are
 * subject to the Apple Public Source License Version 1.2 (the 'License').
 * You may not use this file except in compliance with the License. Please
 * obtain a copy of the License at http://www.apple.com/publicsource and
 * read it before using this file.
 *
 * This Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. Please
 * see the License for the specific language governing rights and
 * limitations under the License.
 */

/******************************************************************

	MUSCLE SmartCard Development ( http://www.linuxnet.com )
	    Title  : winscard_svc.c
	    Package: pcsc lite
            Author : David Corcoran
            Date   : 03/30/01
	    License: Copyright (C) 2001 David Corcoran
	             <corcoran@linuxnet.com>
            Purpose: This demarshalls functions over the message
	             queue and keeps track of clients and their
                     handles.

********************************************************************/

#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/ioctl.h>
#include <string.h>
#include <errno.h>

#include "config.h"
#include "wintypes.h"
#include "pcsclite.h"
#include "winscard.h"
#include "winscard_msg.h"
#include "winscard_svc.h"
#include "debuglog.h"
#include "sys_generic.h"
#include <stdlib.h>

#ifndef WIN32
#include <syslog.h>
#endif

/* The standard ccidDriver.bundle that we shipped on Panther
   writes 10 bytes beyond the legal end of the passed in
   recieve buffer. */
static const unsigned char receive_buffer_padding[] =
	{ 0xCC, 0x1D, 0xCC, 0x1D, 0xCC, 0x1D, 0xCC, 0x1D, 0xCC, 0x1D };

static int commonSocket = 0;

struct _clientSockets
{
	int sd;
};

static struct _clientSockets clientSockets[PCSCLITE_MAX_APPLICATIONS];

static struct _psChannelMap
{
	SCARDCONTEXT hContext;
	SCARDHANDLE hCard[PCSCLITE_MAX_CONTEXTS];
	DWORD dwClientID;
	DWORD dwHandleID;
}
psChannelMap[PCSCLITE_MAX_CHANNELS];

LONG MSGCheckHandleAssociation(DWORD, SCARDHANDLE);

/*
 * A list of local functions used to keep track of clients and their
 * connections 
 */

int MSGServerSetupCommonChannel()
{

	int i;
	static struct sockaddr_un serv_adr;

	for (i = 0; i < PCSCLITE_MAX_APPLICATIONS; i++)
	{
		clientSockets[i].sd = -1;
	}

	/*
	 * Create the common shared connection socket 
	 */
	if ((commonSocket = socket(AF_UNIX, SOCK_STREAM, 0)) < 0)
	{
		DebugLogB
			("SHMInitializeSharedSegment: Unable to create common socket: %s",
			strerror(errno));
		return -1;
	}

	serv_adr.sun_family = AF_UNIX;
	strncpy(serv_adr.sun_path, PCSCLITE_CSOCK_NAME,
		sizeof(serv_adr.sun_path));
	unlink(PCSCLITE_CSOCK_NAME);

	if (bind(commonSocket, (struct sockaddr *) &serv_adr,
			sizeof(serv_adr.sun_family) + strlen(serv_adr.sun_path) + 1) <
		0)
	{
		DebugLogB
			("SHMInitializeSharedSegment: Unable to bind common socket: %s",
			strerror(errno));
		MSGServerCleanupCommonChannel(commonSocket, PCSCLITE_CSOCK_NAME);
		return -1;
	}

	if (listen(commonSocket, 1) < 0)
	{
		DebugLogB
			("SHMInitializeSharedSegment: Unable to listen common socket: %s",
			strerror(errno));
		MSGServerCleanupCommonChannel(commonSocket, PCSCLITE_CSOCK_NAME);
		return -1;
	}

	/*
	 * Chmod the public entry channel 
	 */
	SYS_Chmod(PCSCLITE_CSOCK_NAME,
        S_IROTH | S_IWOTH | S_IRGRP | S_IWGRP | S_IRUSR | S_IWUSR);

	return 0;
}

void MSGServerCleanupCommonChannel(int sockValue, char *pcFilePath)
{
	SYS_CloseFile(sockValue);
	SYS_Unlink(pcFilePath);
}

int MSGServerProcessCommonChannelRequest()
{

	int i, clnt_len;
	int new_sock;
	struct sockaddr_un clnt_addr;
	int one;

	clnt_len = sizeof(clnt_addr);

	if ((new_sock = accept(commonSocket, (struct sockaddr *) &clnt_addr,
				&clnt_len)) < 0)
	{
		DebugLogB
			("MSGServerProcessCommonChannelRequest: ER: Accept on common socket: %s",
			strerror(errno));
		return -1;
	}

	for (i = 0; i < PCSCLITE_MAX_APPLICATIONS; i++)
	{
		if (clientSockets[i].sd == -1)
		{
			break;
		}
	}

	if (i == PCSCLITE_MAX_APPLICATIONS)
	{
		SYS_CloseFile(new_sock);
		return -1;
	}

	clientSockets[i].sd = new_sock;

	one = 1;
	if (ioctl(clientSockets[i].sd, FIONBIO, &one) < 0)
	{
		DebugLogB("SHMInitializeSharedSegment: Error: cannot set socket "
			"nonblocking: %s", strerror(errno));
		SYS_CloseFile(clientSockets[i].sd);
		clientSockets[i].sd = -1;
		return -1;
	}

	return 0;
}

int MSGServerProcessEvents(request_object *requestObj, int blockAmount)
{

	static fd_set read_fd;
	int i, selret, largeSock, rv;

	largeSock = 0;

	FD_ZERO(&read_fd);

	/*
	 * Set up the bit masks for select 
	 */
	FD_SET(commonSocket, &read_fd);
	largeSock = commonSocket;

	for (i = 0; i < PCSCLITE_MAX_APPLICATIONS; i++)
	{
		if (clientSockets[i].sd != -1)
		{
			FD_SET(clientSockets[i].sd, &read_fd);
			if (clientSockets[i].sd > largeSock)
			{
				largeSock = clientSockets[i].sd;
			}
		}
	}

	selret = select(largeSock + 1, &read_fd, (fd_set *) NULL,
		(fd_set *) NULL, NULL);

	if (selret < 0)
	{
		DebugLogB("MSGServerProcessEvents: Select returns with failure: %s",
			strerror(errno));
		return -1;
	}

	if (selret == 0)
		// timeout
		return 2;

	/*
	 * A common pipe packet has arrived - it could be a new application or 
	 * it could be a reader event packet coming from another thread 
	 */

	if (FD_ISSET(commonSocket, &read_fd))
	{
		DebugLogA("MSGServerProcessEvents: Common channel packet arrival");
		if (MSGServerProcessCommonChannelRequest() == -1)
		{
			return -1;
		} else
		{
			return 0;
		}
	}

	for (i = 0; i < PCSCLITE_MAX_APPLICATIONS; i++)
	{
		if (clientSockets[i].sd != -1)
		{
			if (FD_ISSET(clientSockets[i].sd, &read_fd))
			{
				request_header *header = &requestObj->message.header;

				/* Read the request header from the client socket */
				rv = MSGRecieveData(clientSockets[i].sd,
					PCSCLITE_SERVER_ATTEMPTS, header, sizeof(*header));
				if (!rv)
				{
					/* Receive the remainder of the request. */
					size_t bytes_left = header->size - sizeof(*header);
					if (bytes_left > sizeof(requestObj->message))
					{	/* The client is sending bogus data, pretend it died */
						rv = -1;
					}
					else
					{
						rv = MSGRecieveData(clientSockets[i].sd,
							PCSCLITE_SERVER_ATTEMPTS, header + 1, bytes_left);
						if (!rv && header->additional_data_size)
						{
							requestObj->additional_data =
								malloc(header->additional_data_size);
							rv = MSGRecieveData(clientSockets[i].sd,
								PCSCLITE_SERVER_ATTEMPTS,
								requestObj->additional_data,
								header->additional_data_size);
						}
						else
						{
							requestObj->additional_data = NULL;
							header->additional_data_size = 0;
						}
					}
				}

				/*
				 * Set the identifier handle 
				 */
				requestObj->socket = clientSockets[i].sd;

				if (rv == -1)
				{	/* The client has died */
					requestObj->mtype = CMD_CLIENT_DIED;
					requestObj->additional_data = NULL;
					header->command = 0;
					SYS_CloseFile(clientSockets[i].sd);
					clientSockets[i].sd = -1;
					return 0;
				}

				requestObj->mtype = CMD_FUNCTION;
				return 1;
			}
		}
	}

	return -1;
}

LONG MSGFunctionDemarshall(const request_object *request, reply_object *reply)
{
	const request_message *requestm = &request->message;
	const request_header *requesth = &requestm->header;
	reply_message *replym = &reply->message;
	reply_header *replyh = &replym->header;
	//LONG rv;

	/*
	 * Zero out everything 
	 */
	//rv = 0;

	switch (requesth->command)
	{

	case SCARD_ESTABLISH_CONTEXT:
		if (requesth->size != sizeof(requestm->establish))
			return -1;
		replyh->size = sizeof(replym->establish);
		replyh->rv = SCardEstablishContext(requestm->establish.dwScope,
			0, 0, &replym->establish.phContext);

		if (replyh->rv == SCARD_S_SUCCESS)
			replyh->rv = MSGAddContext(replym->establish.phContext,
				request->socket);
		break;

	case SCARD_RELEASE_CONTEXT:
		if (requesth->size != sizeof(requestm->release))
			return -1;
		replyh->size = sizeof(replym->release);
		replyh->rv = SCardReleaseContext(requestm->release.hContext);

		if (replyh->rv == SCARD_S_SUCCESS)
			replyh->rv = MSGRemoveContext(requestm->release.hContext,
				request->socket);
		break;

	case SCARD_CONNECT:
		if (requesth->size != sizeof(requestm->connect))
			return -1;
		replyh->size = sizeof(replym->connect);
		replyh->rv = SCardConnect(requestm->connect.hContext,
			requestm->connect.szReader,
			requestm->connect.dwShareMode,
			requestm->connect.dwPreferredProtocols,
			&replym->connect.phCard,
			&replym->connect.pdwActiveProtocol);

		if (replyh->rv == SCARD_S_SUCCESS)
			replyh->rv = MSGAddHandle(requestm->connect.hContext,
				request->socket, replym->connect.phCard);
		break;

	case SCARD_RECONNECT:
		if (requesth->size != sizeof(requestm->reconnect))
			return -1;
		replyh->size = sizeof(replym->reconnect);
		replyh->rv = MSGCheckHandleAssociation(request->socket,
			requestm->reconnect.hCard);
		if (replyh->rv != 0)
			return 0;
		replyh->rv = SCardReconnect(requestm->reconnect.hCard,
			requestm->reconnect.dwShareMode,
			requestm->reconnect.dwPreferredProtocols,
			requestm->reconnect.dwInitialization,
			&replym->reconnect.pdwActiveProtocol);
		break;

	case SCARD_DISCONNECT:
		if (requesth->size != sizeof(requestm->disconnect))
			return -1;
		replyh->size = sizeof(replym->disconnect);
		replyh->rv = MSGCheckHandleAssociation(request->socket,
			requestm->disconnect.hCard);
		if (replyh->rv != 0)
			return 0;
		replyh->rv = SCardDisconnect(requestm->disconnect.hCard,
			requestm->disconnect.dwDisposition);

		if (replyh->rv == SCARD_S_SUCCESS)
			replyh->rv = MSGRemoveHandle(0, request->socket,
				requestm->disconnect.hCard);
		break;

	case SCARD_BEGIN_TRANSACTION:
		if (requesth->size != sizeof(requestm->begin))
			return -1;
		replyh->size = sizeof(replym->begin);
		replyh->rv = MSGCheckHandleAssociation(request->socket,
			requestm->begin.hCard);
		if (replyh->rv != 0)
			return 0;
		replyh->rv = SCardBeginTransaction(requestm->begin.hCard);
		break;

	case SCARD_END_TRANSACTION:
		if (requesth->size != sizeof(requestm->end))
			return -1;
		replyh->size = sizeof(replym->end);
		replyh->rv = MSGCheckHandleAssociation(request->socket,
			requestm->end.hCard);
		if (replyh->rv != 0)
			return 0;
		replyh->rv = SCardEndTransaction(requestm->end.hCard,
			requestm->end.dwDisposition);
		break;

	case SCARD_CANCEL_TRANSACTION:
		if (requesth->size != sizeof(requestm->cancel))
			return -1;
		replyh->size = sizeof(replym->cancel);
		replyh->rv = MSGCheckHandleAssociation(request->socket,
			requestm->cancel.hCard);
		if (replyh->rv != 0)
			return 0;
		replyh->rv = SCardCancelTransaction(requestm->cancel.hCard);
		break;

	case SCARD_STATUS:
		if (requesth->size != sizeof(requestm->status))
			return -1;
		replyh->size = sizeof(replym->status);
		replyh->rv = MSGCheckHandleAssociation(request->socket,
			requestm->status.hCard);
		if (replyh->rv != 0)
			return 0;
		replyh->rv = SCardStatus(requestm->status.hCard,
			replym->status.mszReaderNames,
			&replym->status.pcchReaderLen,
			&replym->status.pdwState,
			&replym->status.pdwProtocol,
			replym->status.pbAtr,
			&replym->status.pcbAtrLen);
		break;

	case SCARD_TRANSMIT:
		if (requesth->size != sizeof(requestm->transmit))
			return -1;
		replyh->size = sizeof(replym->transmit);
		replyh->rv = MSGCheckHandleAssociation(request->socket,
			requestm->transmit.hCard);
		if (replyh->rv != 0)
			return 0;
		reply->additional_data =
			malloc(requestm->transmit.cbMaxRecvLength
				+ sizeof(receive_buffer_padding));
		if (!reply->additional_data)
			replym->transmit.cbRecvLength = 0;
		else
		{
			/* Fill the padding part of the rcv buffer
			   with RCV_BUFFER_PADDING */
			memcpy(((char *)reply->additional_data)
				+ requestm->transmit.cbMaxRecvLength,
				receive_buffer_padding, sizeof(receive_buffer_padding));
			replym->transmit.cbRecvLength =
				requestm->transmit.cbMaxRecvLength;
		}

		replyh->rv = SCardTransmit(requestm->transmit.hCard,
			&requestm->transmit.pioSendPci,
			request->additional_data, requesth->additional_data_size,
			&replym->transmit.pioRecvPci,
			reply->additional_data, &replym->transmit.cbRecvLength);

		/* Check to make sure the RCV_BUFFER_PADDING was untouched. */
		if (memcmp(((char *)reply->additional_data)
				+ requestm->transmit.cbMaxRecvLength,
				receive_buffer_padding, sizeof(receive_buffer_padding)))
		{
		DebugLogA("SCardTransmit: wrote past end of receive buffer");
#ifndef WIN32
		syslog(LOG_WARNING, "SCardTransmit: wrote past end receive buffer");
#endif
		}

		if (replyh->rv == SCARD_S_SUCCESS)
			replyh->additional_data_size =
				replym->transmit.cbRecvLength;
		break;

	default:
		return -1;
	}

	return 0;
}

LONG MSGAddContext(SCARDCONTEXT hContext, DWORD dwClientID)
{

	int i;

	for (i = 0; i < PCSCLITE_MAX_CHANNELS; i++)
	{
		if (psChannelMap[i].dwClientID == 0)
		{
			psChannelMap[i].hContext = hContext;
			psChannelMap[i].dwClientID = dwClientID;
			break;
		}
	}

	if (i == PCSCLITE_MAX_CHANNELS)
	{
		return SCARD_F_INTERNAL_ERROR;
	} else
	{
		return SCARD_S_SUCCESS;
	}

}

LONG MSGRemoveContext(SCARDCONTEXT hContext, DWORD dwClientID)
{

	int i, j;
	LONG rv;

	for (i = 0; i < PCSCLITE_MAX_CHANNELS; i++)
	{
		if (psChannelMap[i].hContext == hContext &&
			psChannelMap[i].dwClientID == dwClientID)
		{

			for (j = 0; j < PCSCLITE_MAX_CONTEXTS; j++)
			{
				/*
				 * Disconnect each of these just in case 
				 */

				if (psChannelMap[i].hCard[j] != 0)
				{

					/*
					 * We will use SCardStatus to see if the card has been 
					 * reset there is no need to reset each time
					 * Disconnect is called 
					 */

					rv = SCardStatus(psChannelMap[i].hCard[j], 0, 0, 0, 0,
						0, 0);

					if (rv == SCARD_W_RESET_CARD
						|| rv == SCARD_W_REMOVED_CARD)
					{
						SCardDisconnect(psChannelMap[i].hCard[j],
							SCARD_LEAVE_CARD);
					} else
					{
						SCardDisconnect(psChannelMap[i].hCard[j],
							SCARD_RESET_CARD);
					}

					psChannelMap[i].hCard[j] = 0;
				}

				psChannelMap[i].hContext = 0;
				psChannelMap[i].dwClientID = 0;

			}

			SCardReleaseContext(hContext);
			return SCARD_S_SUCCESS;
		}
	}

	return SCARD_E_INVALID_VALUE;
}

LONG MSGAddHandle(SCARDCONTEXT hContext, DWORD dwClientID,
	SCARDHANDLE hCard)
{

	int i, j;

	for (i = 0; i < PCSCLITE_MAX_CHANNELS; i++)
	{
		if (psChannelMap[i].hContext == hContext &&
			psChannelMap[i].dwClientID == dwClientID)
		{

			/*
			 * Find an empty spot to put the hCard value 
			 */
			for (j = 0; j < PCSCLITE_MAX_CONTEXTS; j++)
			{
				if (psChannelMap[i].hCard[j] == 0)
				{
					psChannelMap[i].hCard[j] = hCard;
					break;
				}
			}

			if (j == PCSCLITE_MAX_CONTEXTS)
			{
				return SCARD_F_INTERNAL_ERROR;
			} else
			{
				return SCARD_S_SUCCESS;
			}

		}

	}	/* End of for */

	return SCARD_E_INVALID_VALUE;

}

LONG MSGRemoveHandle(SCARDCONTEXT hContext, DWORD dwClientID,
	SCARDHANDLE hCard)
{

	int i, j;

	for (i = 0; i < PCSCLITE_MAX_CHANNELS; i++)
	{
		if (psChannelMap[i].dwClientID == dwClientID)
		{
			for (j = 0; j < PCSCLITE_MAX_CONTEXTS; j++)
			{
				if (psChannelMap[i].hCard[j] == hCard)
				{
					psChannelMap[i].hCard[j] = 0;
					return SCARD_S_SUCCESS;
				}
			}
		}
	}

	return SCARD_E_INVALID_VALUE;
}


LONG MSGCheckHandleAssociation(DWORD dwClientID, SCARDHANDLE hCard)
{

	int i, j;

	for (i = 0; i < PCSCLITE_MAX_CHANNELS; i++)
	{
		if (psChannelMap[i].dwClientID == dwClientID)
		{
			for (j = 0; j < PCSCLITE_MAX_CONTEXTS; j++)
			{
				if (psChannelMap[i].hCard[j] == hCard)
				{
				  return 0;
				}
			}
		}
	}

	/* Must be a rogue client, debug log and sleep a couple of seconds */
	DebugLogA("MSGCheckHandleAssociation: Client failed to authenticate\n");
	SYS_Sleep(2);

	return SCARD_E_INVALID_HANDLE;
}

LONG MSGCleanupClient(request_object *request)
{

	int i;

	for (i = 0; i < PCSCLITE_MAX_CHANNELS; i++)
	{
		if (psChannelMap[i].dwClientID == request->socket)
		{
			MSGRemoveContext(psChannelMap[i].hContext, request->socket);
		}
	}
	return 0;
}

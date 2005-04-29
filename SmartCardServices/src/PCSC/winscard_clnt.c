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
	Title  : winscard_clnt.c
	Package: pcsc lite
	Author : David Corcoran
	Date   : 10/27/99
	License: Copyright (C) 1999 David Corcoran
			<corcoran@linuxnet.com>
	Purpose: This handles smartcard reader communications. 
	This file forwards requests over message queues.

$Id: winscard_clnt.c,v 1.3 2004/10/21 01:17:53 mb Exp $

********************************************************************/

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/ioctl.h>
#include <errno.h>

#include "config.h"
#include "wintypes.h"
#include "pcsclite.h"
#include "winscard.h"
#include "debuglog.h"

#ifdef USE_THREAD_SAFETY
#include "thread_generic.h"
#endif

#include "readerfactory.h"
#include "eventhandler.h"
#include "sys_generic.h"

#include "winscard_msg.h"


static int appSocket = 0;

static struct _psChannelMap
{
	SCARDHANDLE hCard;
	LPSTR readerName;
}
psChannelMap[PCSCLITE_MAX_CONTEXTS];

static struct _psContextMap
{
	SCARDCONTEXT hContext;
	DWORD contextBlockStatus;
}
psContextMap[PCSCLITE_MAX_CONTEXTS];

static short isExecuted = 0;
static int mapAddr = 0;

#ifdef USE_THREAD_SAFETY
static PCSCLITE_MUTEX clientMutex = PTHREAD_MUTEX_INITIALIZER;
#endif

static PREADER_STATES readerStates[PCSCLITE_MAX_CONTEXTS];

SCARD_IO_REQUEST g_rgSCardT0Pci, g_rgSCardT1Pci, g_rgSCardRawPci;

/* Client only functions. */
static int MSGClientSetupSession();
static int MSGClientCloseSession();
static int MSGClientSendRequest(unsigned int command,
	int blockAmount, void *request, unsigned int requestSize,
	const void *additionalData, unsigned int additionalDataSize);
static int MSGClientReceiveReply(void *reply, unsigned int replySize,
	void *additionalData, unsigned int maxAdditionalDataSize);

static LONG SCardAddContext(SCARDCONTEXT);
static LONG SCardGetContextIndice(SCARDCONTEXT);
static LONG SCardRemoveContext(SCARDCONTEXT);

static LONG SCardAddHandle(SCARDHANDLE, LPSTR);
static LONG SCardGetHandleIndice(SCARDHANDLE);
static LONG SCardRemoveHandle(SCARDHANDLE);

static LONG SCardCheckDaemonAvailability();
static LONG SCardCheckReaderAvailability(LPSTR, LONG);

/*
 * Thread safety functions 
 */
// static LONG SCardSetupThreadSafety ( );
static LONG SCardLockThread();
static LONG SCardUnlockThread();

/*
 * by najam 
 */
static LONG SCardEstablishContextTH(DWORD, LPCVOID, LPCVOID,
	LPSCARDCONTEXT);
static LONG SCardReleaseContextTH(SCARDCONTEXT hContext);
static LONG SCardConnectTH(SCARDCONTEXT, LPCSTR, DWORD, DWORD,
	LPSCARDHANDLE, LPDWORD);
static LONG SCardReconnectTH(SCARDHANDLE, DWORD, DWORD, DWORD, LPDWORD);
static LONG SCardDisconnectTH(SCARDHANDLE, DWORD);
static LONG SCardEndTransactionTH(SCARDHANDLE, DWORD);
static LONG SCardCancelTransactionTH(SCARDHANDLE);
static LONG SCardStatusTH(SCARDHANDLE, LPSTR, LPDWORD, LPDWORD, LPDWORD,
	LPBYTE, LPDWORD);
static LONG SCardTransmitTH(SCARDHANDLE, LPCSCARD_IO_REQUEST, LPCBYTE,
	DWORD, LPSCARD_IO_REQUEST, LPBYTE, LPDWORD);
static LONG SCardListReadersTH(SCARDCONTEXT, LPCSTR, LPSTR, LPDWORD);
static LONG SCardListReaderGroupsTH(SCARDCONTEXT, LPSTR, LPDWORD);
static LONG SCardCancelTH(SCARDCONTEXT);

/*
 * -------by najam-------------------------------------- 
 */

/* Open a session to the server. */
static int MSGClientSetupSession()
{

	struct sockaddr_un svc_addr;
	int one;

	if ((appSocket = socket(AF_UNIX, SOCK_STREAM, 0)) < 0)
	{
		DebugLogB
			("SHMInitializeSharedSegment: Error: create on client socket: %s",
			strerror(errno));
		return -1;
	}

	svc_addr.sun_family = AF_UNIX;
	strncpy(svc_addr.sun_path, PCSCLITE_CSOCK_NAME,
		sizeof(svc_addr.sun_path));

	if (connect(appSocket, (struct sockaddr *) &svc_addr,
			sizeof(svc_addr.sun_family) + strlen(svc_addr.sun_path) + 1) <
		0)
	{
		DebugLogB
			("SHMInitializeSharedSegment: Error: connect to client socket: %s",
			strerror(errno));

		SYS_CloseFile(appSocket);
		return -1;
	}

	one = 1;
	if (ioctl(appSocket, FIONBIO, &one) < 0)
	{
		DebugLogB("SHMInitializeSharedSegment: Error: cannot set socket "
			"nonblocking: %s", strerror(errno));
		SYS_CloseFile(appSocket);
		return -1;
	}

	return 0;
}

static int MSGClientCloseSession()
{
	SYS_CloseFile(appSocket);
	return 0;
}

/* Send a request from a client. */
static int MSGClientSendRequest(unsigned int command,
	int blockAmount, void *request, unsigned int requestSize,
	const void *additionalData, unsigned int additionalDataSize)
{
	request_header *header = (request_header *)request;
	int retval;

	/* Setup the request header */
	header->size = requestSize;
	header->additional_data_size = additionalDataSize;
	header->command = command;

	retval = MSGSendData(appSocket, blockAmount, request, requestSize);
	if (!retval && additionalDataSize)
		retval = MSGSendData(appSocket, blockAmount, additionalData,
			additionalDataSize);

	return retval;
}

/* Recieve a reply on a client. */
static int MSGClientReceiveReply(void *reply, unsigned int replySize,
		void *additionalData, unsigned int maxAdditionalDataSize)
{
	reply_header *header = (reply_header *)reply;
	int retval;

	retval = MSGRecieveData(appSocket, PCSCLITE_CLIENT_ATTEMPTS, reply,
		replySize);
	if (!retval && header->additional_data_size > 0)
	{
		/* Server will never send us back more data than
		   the maximum amount we requested. */
		assert(header->additional_data_size <= maxAdditionalDataSize);

		retval = MSGRecieveData(appSocket, PCSCLITE_CLIENT_ATTEMPTS,
			additionalData, header->additional_data_size);
	}

	return retval;
}


/*
 * By najam 
 */
LONG SCardEstablishContext(DWORD dwScope, LPCVOID pvReserved1,
	LPCVOID pvReserved2, LPSCARDCONTEXT phContext)
{
	long rv;

	SCardLockThread();
	rv = SCardEstablishContextTH(dwScope, pvReserved1,
		pvReserved2, phContext);
	SCardUnlockThread();

	return rv;

}

/*
 * -----------by najam 
 */

static LONG SCardEstablishContextTH(DWORD dwScope, LPCVOID pvReserved1,
	LPCVOID pvReserved2, LPSCARDCONTEXT phContext)
{

	LONG liIndex, rv;
	int i, pageSize;
	establish_request request;
	establish_reply reply;

	/*
	 * Zero out everything 
	 */
	liIndex = 0;
	rv = 0;
	i = 0;
	pageSize = 0;

	if (phContext == 0)
	{
		return SCARD_E_INVALID_PARAMETER;
	} else
	{
		*phContext = 0;
	}

	/*
	 * Do this only once 
	 */
	if (isExecuted == 0)
	{
		g_rgSCardT0Pci.dwProtocol = SCARD_PROTOCOL_T0;
		g_rgSCardT1Pci.dwProtocol = SCARD_PROTOCOL_T1;
		g_rgSCardRawPci.dwProtocol = SCARD_PROTOCOL_RAW;

		/*
		 * Do any system initilization here 
		 */
		SYS_Initialize();

		/*
		 * Set up the memory mapped reader stats structures 
		 */
		mapAddr = SYS_OpenFile(PCSCLITE_PUBSHM_FILE, O_RDONLY, 0);
		if (mapAddr < 0)
		{
			DebugLogA("ERROR: Cannot open public shared file");
			return SCARD_F_INTERNAL_ERROR;
		}

		pageSize = SYS_GetPageSize();

		/*
		 * Allocate each reader structure 
		 */
		for (i = 0; i < PCSCLITE_MAX_CONTEXTS; i++)
		{
			/*
			 * Initially set the context, hcard structs to zero 
			 */
			psChannelMap[i].hCard = 0;
			psChannelMap[i].readerName = 0;
			psContextMap[i].hContext = 0;
			psContextMap[i].contextBlockStatus = BLOCK_STATUS_RESUME;

			readerStates[i] = (PREADER_STATES)
				SYS_PublicMemoryMap(sizeof(READER_STATES),
				mapAddr, (i * pageSize));
			if (readerStates[i] == 0)
			{
				DebugLogA("ERROR: Cannot public memory map");
				SYS_CloseFile(mapAddr);	/* Close the memory map file */
				return SCARD_F_INTERNAL_ERROR;
			}
		}

		if (MSGClientSetupSession() != 0)
		{
			SYS_CloseFile(mapAddr);
			return SCARD_E_NO_SERVICE;
		}

		isExecuted = 1;
	}

	if (dwScope != SCARD_SCOPE_USER && dwScope != SCARD_SCOPE_TERMINAL &&
		dwScope != SCARD_SCOPE_SYSTEM && dwScope != SCARD_SCOPE_GLOBAL)
	{

		return SCARD_E_INVALID_VALUE;
	}

	request.dwScope = dwScope;

	rv = MSGClientSendRequest(SCARD_ESTABLISH_CONTEXT,
		PCSCLITE_MCLIENT_ATTEMPTS, &request, sizeof(request), NULL, 0);

	if (rv == -1)
	{
		return SCARD_E_NO_SERVICE;
	}

	/*
	 * Read a message from the server 
	 */
	rv = MSGClientReceiveReply(&reply, sizeof(reply), NULL, 0);

	if (rv == -1)
	{
		return SCARD_F_COMM_ERROR;
	}

	if (reply.header.rv != SCARD_S_SUCCESS)
	{
		return reply.header.rv;
	}

	*phContext = reply.phContext;

	/*
	 * Allocate the new hContext - if allocator full return an error 
	 */

	rv = SCardAddContext(*phContext);

	return rv;
}

/*
 * by najam 
 */
LONG SCardReleaseContext(SCARDCONTEXT hContext)
{
	long rv;

	SCardLockThread();
	rv = SCardReleaseContextTH(hContext);
	SCardUnlockThread();

	return rv;
}

/*
 * ------------by najam 
 */

static LONG SCardReleaseContextTH(SCARDCONTEXT hContext)
{

	LONG rv;
	release_request request;
	release_reply reply;

	/*
	 * Zero out everything 
	 */
	rv = 0;

	/*
	 * Make sure this context has been opened 
	 */
	if (SCardGetContextIndice(hContext) == -1)
	{
		return SCARD_E_INVALID_HANDLE;
	}

	request.hContext = hContext;

	if (SCardCheckDaemonAvailability() != SCARD_S_SUCCESS)
		return SCARD_E_NO_SERVICE;

	rv = MSGClientSendRequest(SCARD_RELEASE_CONTEXT,
		PCSCLITE_MCLIENT_ATTEMPTS, &request, sizeof(request), NULL, 0);

	if (rv == -1)
	{
		return SCARD_E_NO_SERVICE;
	}

	/*
	 * Read a message from the server 
	 */
	rv = MSGClientReceiveReply(&reply, sizeof(reply), NULL, 0);

	if (rv == -1)
		return SCARD_F_COMM_ERROR;

	/*
	 * Remove the local context from the stack 
	 */
	SCardRemoveContext(hContext);

	return reply.header.rv;
}

LONG SCardSetTimeout(SCARDCONTEXT hContext, DWORD dwTimeout)
{

	/*
	 * Deprecated 
	 */

	return SCARD_S_SUCCESS;
}

LONG SCardConnect(SCARDCONTEXT hContext, LPCSTR szReader,
	DWORD dwShareMode, DWORD dwPreferredProtocols, LPSCARDHANDLE phCard,
	LPDWORD pdwActiveProtocol)
{
	long rv;

	SCardLockThread();
	rv = SCardConnectTH(hContext, szReader, dwShareMode,
		dwPreferredProtocols, phCard, pdwActiveProtocol);
	SCardUnlockThread();
	return rv;

}

static LONG SCardConnectTH(SCARDCONTEXT hContext, LPCSTR szReader,
	DWORD dwShareMode, DWORD dwPreferredProtocols, LPSCARDHANDLE phCard,
	LPDWORD pdwActiveProtocol)
{

	LONG rv;
	connect_request request;
	connect_reply reply;

	/*
	 * Zero out everything 
	 */
	rv = 0;

	/*
	 * Check for NULL parameters 
	 */
	if (phCard == 0 || pdwActiveProtocol == 0)
	{
		return SCARD_E_INVALID_PARAMETER;
	} else
	{
		*phCard = 0;
	}

	if (szReader == 0)
	{
		return SCARD_E_UNKNOWN_READER;
	}

	/*
	 * Check for uninitialized strings 
	 */
	if (strlen(szReader) > MAX_READERNAME)
	{
		return SCARD_E_INVALID_VALUE;
	}

	/*
	 * Make sure this context has been opened 
	 */
	if (SCardGetContextIndice(hContext) == -1)
	{
		return SCARD_E_INVALID_HANDLE;
	}

	if (!(dwPreferredProtocols & SCARD_PROTOCOL_T0) &&
		!(dwPreferredProtocols & SCARD_PROTOCOL_T1) &&
		!(dwPreferredProtocols & SCARD_PROTOCOL_RAW) &&
		!(dwPreferredProtocols & SCARD_PROTOCOL_ANY))
	{

		return SCARD_E_INVALID_VALUE;
	}

	/* @@@ Consider sending this as additionalData instead. */
	strncpy(request.szReader, szReader, MAX_READERNAME);

	request.hContext = hContext;
	request.dwShareMode = dwShareMode;
	request.dwPreferredProtocols = dwPreferredProtocols;
	//request.phCard = *phCard;
	//request.pdwActiveProtocol = *pdwActiveProtocol;

	if (SCardCheckDaemonAvailability() != SCARD_S_SUCCESS)
		return SCARD_E_NO_SERVICE;

	rv = MSGClientSendRequest(SCARD_CONNECT,
		PCSCLITE_CLIENT_ATTEMPTS, &request, sizeof(request), NULL, 0);

	if (rv == -1)
	{
		return SCARD_E_NO_SERVICE;
	}

	/*
	 * Read a message from the server 
	 */
	rv = MSGClientReceiveReply(&reply, sizeof(reply), NULL, 0);

	if (rv == -1)
		return SCARD_F_COMM_ERROR;

	*phCard = reply.phCard;
	*pdwActiveProtocol = reply.pdwActiveProtocol;

	if (reply.header.rv == SCARD_S_SUCCESS)
	{
		/*
		 * Keep track of the handle locally 
		 */
		rv = SCardAddHandle(*phCard, (LPSTR) szReader);
		return rv;
	}

	return reply.header.rv;
}

LONG SCardReconnect(SCARDHANDLE hCard, DWORD dwShareMode,
	DWORD dwPreferredProtocols, DWORD dwInitialization,
	LPDWORD pdwActiveProtocol)
{
	long rv;

	SCardLockThread();
	rv = SCardReconnectTH(hCard, dwShareMode, dwPreferredProtocols,
		dwInitialization, pdwActiveProtocol);
	SCardUnlockThread();
	return rv;

}

LONG SCardReconnectTH(SCARDHANDLE hCard, DWORD dwShareMode,
	DWORD dwPreferredProtocols, DWORD dwInitialization,
	LPDWORD pdwActiveProtocol)
{

	LONG liIndex, rv;
	reconnect_request request;
	reconnect_reply reply;
	int i;

	/*
	 * Zero out everything 
	 */
	liIndex = 0;
	i = 0;
	rv = 0;

	if (dwInitialization != SCARD_LEAVE_CARD &&
		dwInitialization != SCARD_RESET_CARD &&
		dwInitialization != SCARD_UNPOWER_CARD &&
		dwInitialization != SCARD_EJECT_CARD)
	{

		return SCARD_E_INVALID_VALUE;
	}

	if (!(dwPreferredProtocols & SCARD_PROTOCOL_T0) &&
		!(dwPreferredProtocols & SCARD_PROTOCOL_T1) &&
		!(dwPreferredProtocols & SCARD_PROTOCOL_RAW) &&
		!(dwPreferredProtocols & SCARD_PROTOCOL_ANY))
	{

		return SCARD_E_INVALID_VALUE;
	}

	if (pdwActiveProtocol == 0)
	{
		return SCARD_E_INVALID_PARAMETER;
	}

	liIndex = SCardGetHandleIndice(hCard);

	if (liIndex < 0)
	{
		return SCARD_E_INVALID_HANDLE;
	}

	for (i = 0; i < PCSCLITE_MAX_CONTEXTS; i++)
	{
		if (strcmp(psChannelMap[liIndex].readerName,
				(readerStates[i])->readerName) == 0)
		{
			break;
		}
	}

	if (i == PCSCLITE_MAX_CONTEXTS)
	{
		return SCARD_E_READER_UNAVAILABLE;
	}

	request.hCard = hCard;
	request.dwShareMode = dwShareMode;
	request.dwPreferredProtocols = dwPreferredProtocols;
	request.dwInitialization = dwInitialization;
	//request.pdwActiveProtocol = *pdwActiveProtocol;

	if (SCardCheckDaemonAvailability() != SCARD_S_SUCCESS)
		return SCARD_E_NO_SERVICE;

	rv = MSGClientSendRequest(SCARD_RECONNECT,
		PCSCLITE_CLIENT_ATTEMPTS, &request, sizeof(request), NULL, 0);

	if (rv == -1)
	{

		return SCARD_E_NO_SERVICE;
	}

	/*
	 * Read a message from the server 
	 */
	rv = MSGClientReceiveReply(&reply, sizeof(reply), NULL, 0);

	if (rv == -1)
		return SCARD_F_COMM_ERROR;

	*pdwActiveProtocol = reply.pdwActiveProtocol;

	return SCardCheckReaderAvailability(psChannelMap[liIndex].readerName,
		reply.header.rv);
}

/*
 * by najam 
 */
LONG SCardDisconnect(SCARDHANDLE hCard, DWORD dwDisposition)
{

	long rv;

	SCardLockThread();
	rv = SCardDisconnectTH(hCard, dwDisposition);
	SCardUnlockThread();
	return rv;

}

/*
 * -----by najam 
 */

static LONG SCardDisconnectTH(SCARDHANDLE hCard, DWORD dwDisposition)
{

	LONG liIndex, rv;
	disconnect_request request;
	disconnect_reply reply;

	/*
	 * Zero out everything 
	 */
	liIndex = 0;
	rv = 0;

	if (dwDisposition != SCARD_LEAVE_CARD &&
		dwDisposition != SCARD_RESET_CARD &&
		dwDisposition != SCARD_UNPOWER_CARD &&
		dwDisposition != SCARD_EJECT_CARD)
	{

		return SCARD_E_INVALID_VALUE;
	}

	liIndex = SCardGetHandleIndice(hCard);

	if (liIndex < 0)
	{
		return SCARD_E_INVALID_HANDLE;
	}

	if (SCardCheckDaemonAvailability() != SCARD_S_SUCCESS)
		return SCARD_E_NO_SERVICE;

	request.hCard = hCard;
	request.dwDisposition = dwDisposition;

	rv = MSGClientSendRequest(SCARD_DISCONNECT,
		PCSCLITE_CLIENT_ATTEMPTS, &request, sizeof(request), NULL, 0);

	if (rv == -1)
	{

		return SCARD_E_NO_SERVICE;
	}

	/*
	 * Read a message from the server 
	 */
	rv = MSGClientReceiveReply(&reply, sizeof(reply), NULL, 0);

	if (rv == -1)
		return SCARD_F_COMM_ERROR;

	SCardRemoveHandle(hCard);

	return reply.header.rv;
}

LONG SCardBeginTransaction(SCARDHANDLE hCard)
{

	LONG liIndex, rv;
	begin_request request;
	begin_reply reply;
	int timeval, randnum, i, j;

	/*
	 * Zero out everything 
	 */
	liIndex = 0;
	timeval = 0;
	randnum = 0;
	i = 0;
	rv = 0;

	/*
	 * change by najam 
	 */
	SCardLockThread();
	liIndex = SCardGetHandleIndice(hCard);
	/*
	 * change by najam 
	 */
	SCardUnlockThread();

	if (liIndex < 0)
	{
		return SCARD_E_INVALID_HANDLE;
	}

	for (i = 0; i < PCSCLITE_MAX_CONTEXTS; i++)
	{
		if (strcmp(psChannelMap[liIndex].readerName,
				(readerStates[i])->readerName) == 0)
		{
			break;
		}
	}

	if (i == PCSCLITE_MAX_CONTEXTS)
	{
		return SCARD_E_READER_UNAVAILABLE;
	}

	request.hCard = hCard;

	/*
	 * Query the server every so often until the sharing violation ends
	 * and then hold the lock for yourself.  
	 */

	do
	{

		/*
		 * Look to see if it is locked before polling the server for
		 * admission to the readers resources 
		 */
		if ((readerStates[i])->lockState != 0)
		{
			for (j = 0; j < 100; j++)
			{
				/*
				 * This helps prevent starvation 
				 */
				randnum = SYS_Random(randnum, 1000.0, 10000.0);
				SYS_USleep(randnum);

				if ((readerStates[i])->lockState == 0)
				{
					break;
				}
			}
		}

		if (SCardCheckDaemonAvailability() != SCARD_S_SUCCESS)
			return SCARD_E_NO_SERVICE;

		/*
		 * Begin lock 
		 */
		SCardLockThread();
		rv = MSGClientSendRequest(SCARD_BEGIN_TRANSACTION,
			PCSCLITE_CLIENT_ATTEMPTS, &request, sizeof(request), NULL, 0);

		if (rv == -1)
		{
			/*
			 * End of lock 
			 */
			SCardUnlockThread();
			return SCARD_E_NO_SERVICE;
		}

		/*
		 * Read a message from the server 
		 */
		rv = MSGClientReceiveReply(&reply, sizeof(reply), NULL, 0);

		SCardUnlockThread();
		/*
		 * End of lock 
		 */

		if (rv == -1)
			return SCARD_F_COMM_ERROR;

	}
	while (reply.header.rv == SCARD_E_SHARING_VIOLATION);

	return reply.header.rv;
}

LONG SCardEndTransaction(SCARDHANDLE hCard, DWORD dwDisposition)
{

	long rv;

	SCardLockThread();
	rv = SCardEndTransactionTH(hCard, dwDisposition);
	SCardUnlockThread();
	return rv;
}

LONG SCardEndTransactionTH(SCARDHANDLE hCard, DWORD dwDisposition)
{

	LONG liIndex, rv;
	end_request request;
	end_reply reply;
	int randnum, i;

	/*
	 * Zero out everything 
	 */
	liIndex = 0;
	randnum = 0;
	i = 0;
	rv = 0;

	if (dwDisposition != SCARD_LEAVE_CARD &&
		dwDisposition != SCARD_RESET_CARD &&
		dwDisposition != SCARD_UNPOWER_CARD &&
		dwDisposition != SCARD_EJECT_CARD)
	{

		return SCARD_E_INVALID_VALUE;
	}

	liIndex = SCardGetHandleIndice(hCard);

	if (liIndex < 0)
	{
		return SCARD_E_INVALID_HANDLE;
	}

	for (i = 0; i < PCSCLITE_MAX_CONTEXTS; i++)
	{
		if (strcmp(psChannelMap[liIndex].readerName,
				(readerStates[i])->readerName) == 0)
		{
			break;
		}
	}

	if (i == PCSCLITE_MAX_CONTEXTS)
	{
		return SCARD_E_READER_UNAVAILABLE;
	}

	request.hCard = hCard;
	request.dwDisposition = dwDisposition;

	if (SCardCheckDaemonAvailability() != SCARD_S_SUCCESS)
		return SCARD_E_NO_SERVICE;

	rv = MSGClientSendRequest(SCARD_END_TRANSACTION,
		PCSCLITE_CLIENT_ATTEMPTS, &request, sizeof(request), NULL, 0);

	if (rv == -1)
	{
		return SCARD_E_NO_SERVICE;
	}

	/*
	 * Read a message from the server 
	 */
	rv = MSGClientReceiveReply(&reply, sizeof(reply), NULL, 0);

	if (rv == -1)
		return SCARD_F_COMM_ERROR;

	/*
	 * This helps prevent starvation 
	 */
	randnum = SYS_Random(randnum, 1000.0, 10000.0);
	SYS_USleep(randnum);

	return reply.header.rv;
}

LONG SCardCancelTransaction(SCARDHANDLE hCard)
{

	long rv;

	SCardLockThread();
	rv = SCardCancelTransactionTH(hCard);
	SCardUnlockThread();
	return rv;
}

LONG SCardCancelTransactionTH(SCARDHANDLE hCard)
{

	LONG liIndex, rv;
	cancel_request request;
	cancel_reply reply;
	int i;

	/*
	 * Zero out everything 
	 */
	liIndex = 0;
	i = 0;
	rv = 0;

	liIndex = SCardGetHandleIndice(hCard);

	if (liIndex < 0)
		return SCARD_E_INVALID_HANDLE;

	for (i = 0; i < PCSCLITE_MAX_CONTEXTS; i++)
	{
		if (strcmp(psChannelMap[liIndex].readerName,
				(readerStates[i])->readerName) == 0)
		{
			break;
		}
	}

	if (i == PCSCLITE_MAX_CONTEXTS)
		return SCARD_E_READER_UNAVAILABLE;

	request.hCard = hCard;

	if (SCardCheckDaemonAvailability() != SCARD_S_SUCCESS)
		return SCARD_E_NO_SERVICE;

	rv = MSGClientSendRequest(SCARD_CANCEL_TRANSACTION,
		PCSCLITE_CLIENT_ATTEMPTS, &request, sizeof(request), NULL, 0);

	if (rv == -1)
	{
		return SCARD_E_NO_SERVICE;
	}

	/*
	 * Read a message from the server 
	 */
	rv = MSGClientReceiveReply(&reply, sizeof(reply), NULL, 0);

	if (rv == -1)
		return SCARD_F_COMM_ERROR;

	return reply.header.rv;
}

LONG SCardStatus(SCARDHANDLE hCard, LPSTR mszReaderNames,
	LPDWORD pcchReaderLen, LPDWORD pdwState,
	LPDWORD pdwProtocol, LPBYTE pbAtr, LPDWORD pcbAtrLen)
{
	long rv;

	SCardLockThread();
	rv = SCardStatusTH(hCard, mszReaderNames, pcchReaderLen, pdwState,
		pdwProtocol, pbAtr, pcbAtrLen);
	SCardUnlockThread();
	return rv;
}

LONG SCardStatusTH(SCARDHANDLE hCard, LPSTR mszReaderNames,
	LPDWORD pcchReaderLen, LPDWORD pdwState,
	LPDWORD pdwProtocol, LPBYTE pbAtr, LPDWORD pcbAtrLen)
{

	DWORD dwReaderLen;
	LONG liIndex, rv;
	int i;
	status_request request;
	status_reply reply;

	/*
	 * Zero out everything 
	 */
	liIndex = 0;
	dwReaderLen = 0;
	rv = 0;
	i = 0;

	/*
	 * Check for NULL parameters 
	 */

	if (pcchReaderLen == 0 || pdwState == 0 ||
		pdwProtocol == 0 || pcbAtrLen == 0)
	{
		return SCARD_E_INVALID_PARAMETER;
	}

	liIndex = SCardGetHandleIndice(hCard);

	if (liIndex < 0)
	{
		*pcchReaderLen = 0;
		*pcbAtrLen = 0;
		*pdwState = 0;
		*pdwProtocol = 0;
		return SCARD_E_INVALID_HANDLE;
	}

	for (i = 0; i < PCSCLITE_MAX_CONTEXTS; i++)
	{
		if (strcmp(psChannelMap[liIndex].readerName,
				(readerStates[i])->readerName) == 0)
		{
			break;
		}
	}

	if (i == PCSCLITE_MAX_CONTEXTS)
	{
		*pcchReaderLen = 0;
		*pcbAtrLen = 0;
		*pdwState = 0;
		*pdwProtocol = 0;
		return SCARD_E_READER_UNAVAILABLE;
	}

	/*
	 * A small call must be made to pcscd to find out the event status of
	 * other applications such as reset/removed.  Only hCard is needed so
	 * I will not fill in the other information. 
	 */

	request.hCard = hCard;

	if (SCardCheckDaemonAvailability() != SCARD_S_SUCCESS)
		return SCARD_E_NO_SERVICE;

	rv = MSGClientSendRequest(SCARD_STATUS,
		PCSCLITE_CLIENT_ATTEMPTS, &request, sizeof(request), NULL, 0);

	if (rv == -1)
	{
		*pcchReaderLen = 0;
		*pcbAtrLen = 0;
		*pdwState = 0;
		*pdwProtocol = 0;
		return SCARD_E_NO_SERVICE;
	}

	/*
	 * Read a message from the server 
	 */
	rv = MSGClientReceiveReply(&reply, sizeof(reply), NULL, 0);

	if (rv == -1)
	{
		*pcchReaderLen = 0;
		*pcbAtrLen = 0;
		*pdwState = 0;
		*pdwProtocol = 0;
		return SCARD_F_COMM_ERROR;
	}

	if (reply.header.rv != SCARD_S_SUCCESS)
	{
		/*
		 * An event must have occurred 
		 */
		*pcchReaderLen = 0;
		*pcbAtrLen = 0;
		*pdwState = 0;
		*pdwProtocol = 0;
		return reply.header.rv;
	}

	/*
	 * Now continue with the client side SCardStatus 
	 */

	dwReaderLen = strlen(psChannelMap[liIndex].readerName) + 1;

	if (mszReaderNames == 0)
	{
		*pcchReaderLen = dwReaderLen;
		*pcbAtrLen = 0;
		*pdwState = 0;
		*pdwProtocol = 0;
		return SCARD_S_SUCCESS;
	}

	if (*pcchReaderLen == 0)
	{
		*pcchReaderLen = dwReaderLen;
		*pcbAtrLen = 0;
		*pdwState = 0;
		*pdwProtocol = 0;
		return SCARD_S_SUCCESS;
	}

	if (*pcchReaderLen < dwReaderLen)
	{
		*pcchReaderLen = dwReaderLen;
		*pcbAtrLen = 0;
		*pdwState = 0;
		*pdwProtocol = 0;
		return SCARD_E_INSUFFICIENT_BUFFER;
	}

	*pcchReaderLen = dwReaderLen;
	*pdwState = (readerStates[i])->readerState;
	*pdwProtocol = (readerStates[i])->cardProtocol;
	*pcbAtrLen = (readerStates[i])->cardAtrLength;

	strcpy(mszReaderNames, psChannelMap[liIndex].readerName);
	memcpy(pbAtr, (readerStates[i])->cardAtr,
		(readerStates[i])->cardAtrLength);

	return SCARD_S_SUCCESS;
}

LONG SCardGetStatusChange(SCARDCONTEXT hContext, DWORD dwTimeout,
	LPSCARD_READERSTATE_A rgReaderStates, DWORD cReaders)
{

	LONG rv, contextIndice;
	PSCARD_READERSTATE_A currReader;
	PREADER_STATES rContext;
	LPSTR lpcReaderName;
	DWORD dwTime;
	DWORD dwState;
	DWORD dwBreakFlag;
	int i, j;

	/*
	 * Zero out everything 
	 */
	rv = 0;
	rContext = 0;
	lpcReaderName = 0;
	dwTime = 0;
	j = 0;
	dwState = 0;
	i = 0;
	currReader = 0;
	contextIndice = 0;
	dwBreakFlag = 0;

	if (rgReaderStates == 0 && cReaders > 0)
	{
		return SCARD_E_INVALID_PARAMETER;
	}

	if (cReaders < 0)
	{
		return SCARD_E_INVALID_VALUE;
	}

	/*
	 * Make sure this context has been opened 
	 */

	/*
	 * change by najam 
	 */
	SCardLockThread();
	contextIndice = SCardGetContextIndice(hContext);
	/*
	 * change by najam 
	 */
	SCardUnlockThread();

	if (contextIndice == -1)
	{
		return SCARD_E_INVALID_HANDLE;
	}

	if (SCardCheckDaemonAvailability() != SCARD_S_SUCCESS)
		return SCARD_E_NO_SERVICE;

	/*
	 * Application is waiting for a reader - return the first available
	 * reader 
	 */

	if (cReaders == 0)
	{
		while (1)
		{
			if (SCardCheckDaemonAvailability() != SCARD_S_SUCCESS)
				return SCARD_E_NO_SERVICE;

			for (i = 0; i < PCSCLITE_MAX_CONTEXTS; i++)
			{
				if ((readerStates[i])->readerID != 0)
				{
					/*
					 * Reader was found 
					 */
					return SCARD_S_SUCCESS;
				}
			}

			if (dwTimeout == 0)
			{
				/*
				 * return immediately - no reader available 
				 */
				return SCARD_E_READER_UNAVAILABLE;
			}

			SYS_USleep(PCSCLITE_STATUS_WAIT);

			if (dwTimeout != INFINITE)
			{
				dwTime += PCSCLITE_STATUS_WAIT;

				if (dwTime >= (dwTimeout * 1000))
				{
					return SCARD_E_TIMEOUT;
				}
			}
		}
	} else if (cReaders > PCSCLITE_MAX_CONTEXTS)
	{
		return SCARD_E_INVALID_VALUE;
	}

	/*
	 * Check the integrity of the reader states structures 
	 */

	for (j = 0; j < cReaders; j++)
	{
		currReader = &rgReaderStates[j];
		if (currReader->szReader == 0)
		{
			return SCARD_E_INVALID_VALUE;
		}
	}

	/*
	 * End of search for readers 
	 */

	/*
	 * Clear the event state for all readers 
	 */
	for (j = 0; j < cReaders; j++)
	{
		currReader = &rgReaderStates[j];
		currReader->dwEventState = 0;
	}

	/*
	 * Now is where we start our event checking loop 
	 */

	/*
	 * DebugLogA("SCardGetStatusChange: Event Loop Start"_); 
	 */

	psContextMap[contextIndice].contextBlockStatus = BLOCK_STATUS_BLOCKING;

	j = 0;

	do
	{

		if (SCardCheckDaemonAvailability() != SCARD_S_SUCCESS)
			return SCARD_E_NO_SERVICE;

		currReader = &rgReaderStates[j];

	/************ Look for IGNORED readers ****************************/

		if (currReader->dwCurrentState & SCARD_STATE_IGNORE)
		{
			currReader->dwEventState = SCARD_STATE_IGNORE;
		} else
		{

	  /************ Looks for correct readernames *********************/

			lpcReaderName = (char *) currReader->szReader;

			for (i = 0; i < PCSCLITE_MAX_CONTEXTS; i++)
			{
				if (strcmp(lpcReaderName,
						(readerStates[i])->readerName) == 0)
				{
					break;
				}
			}

			/*
			 * The requested reader name is not recognized 
			 */
			if (i == PCSCLITE_MAX_CONTEXTS)
			{
				if (currReader->dwCurrentState & SCARD_STATE_UNKNOWN)
				{
					currReader->dwEventState = SCARD_STATE_UNKNOWN;
				} else
				{
					currReader->dwEventState =
						SCARD_STATE_UNKNOWN | SCARD_STATE_CHANGED;
					/*
					 * Spec says use SCARD_STATE_IGNORE but a removed USB
					 * reader with eventState fed into currentState will
					 * be ignored forever 
					 */
					dwBreakFlag = 1;
				}
			} else
			{

				/*
				 * The reader has come back after being away 
				 */
				if (currReader->dwCurrentState & SCARD_STATE_UNKNOWN)
				{
					currReader->dwEventState |= SCARD_STATE_CHANGED;
					currReader->dwEventState &= ~SCARD_STATE_UNKNOWN;
					dwBreakFlag = 1;
				}

	/*****************************************************************/

				/*
				 * Set the reader status structure 
				 */
				rContext = readerStates[i];

				/*
				 * Now we check all the Reader States 
				 */
				dwState = rContext->readerState;

	/*********** Check if the reader is in the correct state ********/
				if (dwState & SCARD_UNKNOWN)
				{
					/*
					 * App thinks reader is in bad state and it is 
					 */
					if (currReader->
						dwCurrentState & SCARD_STATE_UNAVAILABLE)
					{
						currReader->dwEventState = SCARD_STATE_UNAVAILABLE;
					} else
					{
						/*
						 * App thinks reader is in good state and it is
						 * not 
						 */
						currReader->dwEventState = SCARD_STATE_CHANGED |
							SCARD_STATE_UNAVAILABLE;
						dwBreakFlag = 1;
					}
				} else
				{
					/*
					 * App thinks reader in bad state but it is not 
					 */
					if (currReader->
						dwCurrentState & SCARD_STATE_UNAVAILABLE)
					{
						currReader->dwEventState &=
							~SCARD_STATE_UNAVAILABLE;
						currReader->dwEventState |= SCARD_STATE_CHANGED;
						dwBreakFlag = 1;
					}
				}

	/********** Check for card presence in the reader **************/

				if (dwState & SCARD_PRESENT)
				{
					currReader->cbAtr = rContext->cardAtrLength;
					memcpy(currReader->rgbAtr, rContext->cardAtr,
						currReader->cbAtr);
				} else
				{
					currReader->cbAtr = 0;
				}

				/*
				 * Card is now absent 
				 */
				if (dwState & SCARD_ABSENT)
				{
					currReader->dwEventState |= SCARD_STATE_EMPTY;
					currReader->dwEventState &= ~SCARD_STATE_PRESENT;
					currReader->dwEventState &= ~SCARD_STATE_UNAWARE;
					currReader->dwEventState &= ~SCARD_STATE_IGNORE;
					currReader->dwEventState &= ~SCARD_STATE_UNKNOWN;
					currReader->dwEventState &= ~SCARD_STATE_UNAVAILABLE;
					currReader->dwEventState &= ~SCARD_STATE_ATRMATCH;
					currReader->dwEventState &= ~SCARD_STATE_MUTE;
					currReader->dwEventState &= ~SCARD_STATE_INUSE;

					/*
					 * After present the rest are assumed 
					 */
					if (currReader->dwCurrentState & SCARD_STATE_PRESENT ||
						currReader->dwCurrentState & SCARD_STATE_ATRMATCH
						|| currReader->
						dwCurrentState & SCARD_STATE_EXCLUSIVE
						|| currReader->dwCurrentState & SCARD_STATE_INUSE)
					{
						currReader->dwEventState |= SCARD_STATE_CHANGED;
						dwBreakFlag = 1;
					}

					/*
					 * Card is now present 
					 */
				} else if (dwState & SCARD_PRESENT)
				{
					currReader->dwEventState |= SCARD_STATE_PRESENT;
					currReader->dwEventState &= ~SCARD_STATE_EMPTY;
					currReader->dwEventState &= ~SCARD_STATE_UNAWARE;
					currReader->dwEventState &= ~SCARD_STATE_IGNORE;
					currReader->dwEventState &= ~SCARD_STATE_UNKNOWN;
					currReader->dwEventState &= ~SCARD_STATE_UNAVAILABLE;
					currReader->dwEventState &= ~SCARD_STATE_MUTE;

					if (currReader->dwCurrentState & SCARD_STATE_EMPTY)
					{
						currReader->dwEventState |= SCARD_STATE_CHANGED;
						dwBreakFlag = 1;
					}

					if (dwState & SCARD_SWALLOWED)
					{
						if (currReader->dwCurrentState & SCARD_STATE_MUTE)
						{
							currReader->dwEventState |= SCARD_STATE_MUTE;
						} else
						{
							currReader->dwEventState |= SCARD_STATE_MUTE;
							if (currReader->dwCurrentState !=
								SCARD_STATE_UNAWARE)
							{
								currReader->dwEventState |=
									SCARD_STATE_CHANGED;
							}
							dwBreakFlag = 1;
						}
					} else
					{
						/*
						 * App thinks card is mute but it is not 
						 */
						if (currReader->dwCurrentState & SCARD_STATE_MUTE)
						{
							currReader->dwEventState |=
								SCARD_STATE_CHANGED;
							dwBreakFlag = 1;
						}
					}
				}

				/*
				 * Now figure out sharing modes 
				 */
				if (rContext->readerSharing == -1)
				{
					currReader->dwEventState |= SCARD_STATE_EXCLUSIVE;
					currReader->dwEventState &= ~SCARD_STATE_INUSE;
					if (currReader->dwCurrentState & SCARD_STATE_INUSE)
					{
						currReader->dwEventState |= SCARD_STATE_CHANGED;
						dwBreakFlag = 1;
					}
				} else if (rContext->readerSharing >= 1)
				{
					/*
					 * A card must be inserted for it to be INUSE 
					 */
					if (dwState & SCARD_PRESENT)
					{
						currReader->dwEventState |= SCARD_STATE_INUSE;
						currReader->dwEventState &= ~SCARD_STATE_EXCLUSIVE;
						if (currReader->
							dwCurrentState & SCARD_STATE_EXCLUSIVE)
						{
							currReader->dwEventState |=
								SCARD_STATE_CHANGED;
							dwBreakFlag = 1;
						}
					}
				} else if (rContext->readerSharing == 0)
				{
					currReader->dwEventState &= ~SCARD_STATE_INUSE;
					currReader->dwEventState &= ~SCARD_STATE_EXCLUSIVE;

					if (currReader->dwCurrentState & SCARD_STATE_INUSE)
					{
						currReader->dwEventState |= SCARD_STATE_CHANGED;
						dwBreakFlag = 1;
					} else if (currReader->
						dwCurrentState & SCARD_STATE_EXCLUSIVE)
					{
						currReader->dwEventState |= SCARD_STATE_CHANGED;
						dwBreakFlag = 1;
					}
				}

				if (currReader->dwCurrentState == SCARD_STATE_UNAWARE)
				{
					/*
					 * Break out of the while .. loop and return status
					 * once all the status's for all readers is met 
					 */
					currReader->dwEventState |= SCARD_STATE_CHANGED;
					dwBreakFlag = 1;
				}

				SYS_USleep(PCSCLITE_STATUS_WAIT);

			}	/* End of SCARD_STATE_UNKNOWN */

		}	/* End of SCARD_STATE_IGNORE */

		/*
		 * Counter and resetter 
		 */
		j = j + 1;
		if (j == cReaders)
		{
			j = 0;
		}

		if (dwTimeout != INFINITE && dwTimeout != 0)
		{

			/*
			 * If time is greater than timeout and all readers have been
			 * checked 
			 */
			if ((dwTime >= (dwTimeout * 1000)) && (j == 0))
			{
				return SCARD_E_TIMEOUT;
			}

			dwTime += PCSCLITE_STATUS_WAIT;
		}

		/*
		 * Declare all the break conditions 
		 */

		if (psContextMap[contextIndice].contextBlockStatus ==
			BLOCK_STATUS_RESUME)
		{
			break;
		}

		/*
		 * Break if UNAWARE is set and all readers have been checked 
		 */
		if ((dwBreakFlag == 1) && (j == 0))
		{
			break;
		}

		/*
		 * Timeout has occurred and all readers checked 
		 */
		if ((dwTimeout == 0) && (j == 0))
		{
			break;
		}

	}
	while (1);

	/*
	 * DebugLogA("SCardGetStatusChange: Event Loop End"); 
	 */

	if (psContextMap[contextIndice].contextBlockStatus ==
		BLOCK_STATUS_RESUME)
	{
		return SCARD_E_CANCELLED;
	}

	return SCARD_S_SUCCESS;
}

LONG SCardControl(SCARDHANDLE hCard, LPCBYTE pbSendBuffer,
	DWORD cbSendLength, LPBYTE pbRecvBuffer, LPDWORD pcbRecvLength)
{

	SCARD_IO_REQUEST pioSendPci, pioRecvPci;

	pioSendPci.dwProtocol = SCARD_PROTOCOL_RAW;
	pioRecvPci.dwProtocol = SCARD_PROTOCOL_RAW;

	return SCardTransmit(hCard, &pioSendPci, pbSendBuffer, cbSendLength,
		&pioRecvPci, pbRecvBuffer, pcbRecvLength);
}

LONG SCardTransmit(SCARDHANDLE hCard, LPCSCARD_IO_REQUEST pioSendPci,
	LPCBYTE pbSendBuffer, DWORD cbSendLength,
	LPSCARD_IO_REQUEST pioRecvPci, LPBYTE pbRecvBuffer,
	LPDWORD pcbRecvLength)
{

	long rv;

	SCardLockThread();
	rv = SCardTransmitTH(hCard, pioSendPci, pbSendBuffer, cbSendLength,
		pioRecvPci, pbRecvBuffer, pcbRecvLength);
	SCardUnlockThread();

	return rv;

}

/*
 * --------by najam 
 */

LONG SCardTransmitTH(SCARDHANDLE hCard, LPCSCARD_IO_REQUEST pioSendPci,
	LPCBYTE pbSendBuffer, DWORD cbSendLength,
	LPSCARD_IO_REQUEST pioRecvPci, LPBYTE pbRecvBuffer,
	LPDWORD pcbRecvLength)
{

	LONG liIndex, rv;
	transmit_request request;
	transmit_reply reply;
	int i;

	/*
	 * Zero out everything 
	 */
	liIndex = 0;
	rv = 0;
	i = 0;

	if (pbSendBuffer == 0 || pbRecvBuffer == 0 ||
		pcbRecvLength == 0 || pioSendPci == 0)
	{
		return SCARD_E_INVALID_PARAMETER;
	}

	liIndex = SCardGetHandleIndice(hCard);

	if (liIndex < 0)
	{
		*pcbRecvLength = 0;
		return SCARD_E_INVALID_HANDLE;
	}

	for (i = 0; i < PCSCLITE_MAX_CONTEXTS; i++)
	{
		if (strcmp(psChannelMap[liIndex].readerName,
				(readerStates[i])->readerName) == 0)
		{
			break;
		}
	}

	if (i == PCSCLITE_MAX_CONTEXTS)
		return SCARD_E_READER_UNAVAILABLE;

	request.hCard = hCard;
	request.header.additional_data_size = cbSendLength;
	request.cbMaxRecvLength = *pcbRecvLength;
	memcpy(&request.pioSendPci, pioSendPci, sizeof(SCARD_IO_REQUEST));

	if (SCardCheckDaemonAvailability() != SCARD_S_SUCCESS)
		return SCARD_E_NO_SERVICE;

	rv = MSGClientSendRequest(SCARD_TRANSMIT,
		PCSCLITE_CLIENT_ATTEMPTS, &request, sizeof(request), pbSendBuffer,
		cbSendLength);

	if (rv == -1)
	{
		return SCARD_E_NO_SERVICE;
	}

	/*
	 * Read a message from the server 
	 */
	rv = MSGClientReceiveReply(&reply, sizeof(reply), pbRecvBuffer,
		request.cbMaxRecvLength);

	if (rv == -1)
		return SCARD_F_COMM_ERROR;

	*pcbRecvLength = reply.cbRecvLength;
	if (reply.header.rv != SCARD_S_SUCCESS)
	{
		return reply.header.rv;
	}

	if (pioRecvPci)
	{
		memcpy(pioRecvPci, &reply.pioRecvPci, sizeof(SCARD_IO_REQUEST));
	}

	return SCardCheckReaderAvailability(psChannelMap[liIndex].
		readerName, reply.header.rv);
}

LONG SCardListReaders(SCARDCONTEXT hContext, LPCSTR mszGroups,
	LPSTR mszReaders, LPDWORD pcchReaders)
{

	long rv;

	SCardLockThread();
	rv = SCardListReadersTH(hContext, mszGroups, mszReaders, pcchReaders);
	SCardUnlockThread();

	return rv;
}

LONG SCardListReadersTH(SCARDCONTEXT hContext, LPCSTR mszGroups,
	LPSTR mszReaders, LPDWORD pcchReaders)
{

	LONG liIndex;
	DWORD dwGroupsLen, dwReadersLen;
	int i, lastChrPtr;

	/*
	 * Zero out everything 
	 */
	liIndex = 0;
	dwGroupsLen = 0;
	dwReadersLen = 0;
	i = 0;
	lastChrPtr = 0;

	/*
	 * Check for NULL parameters 
	 */
	if (pcchReaders == 0)
	{
		return SCARD_E_INVALID_PARAMETER;
	}

	/*
	 * Make sure this context has been opened 
	 */
	if (SCardGetContextIndice(hContext) == -1)
	{
		return SCARD_E_INVALID_HANDLE;
	}

	if (SCardCheckDaemonAvailability() != SCARD_S_SUCCESS)
		return SCARD_E_NO_SERVICE;

	for (i = 0; i < PCSCLITE_MAX_CONTEXTS; i++)
	{
		if ((readerStates[i])->readerID != 0)
		{
			dwReadersLen += (strlen((readerStates[i])->readerName) + 1);
		}
	}

	dwReadersLen += 1;

	if (mszReaders == 0)
	{
		*pcchReaders = dwReadersLen;
		return SCARD_S_SUCCESS;
	} else if (*pcchReaders == 0)
	{
		*pcchReaders = dwReadersLen;
		return SCARD_S_SUCCESS;
	} else if (*pcchReaders < dwReadersLen)
	{
		*pcchReaders = dwReadersLen;
		return SCARD_E_INSUFFICIENT_BUFFER;
	} else
	{
		for (i = 0; i < PCSCLITE_MAX_CONTEXTS; i++)
		{
			if ((readerStates[i])->readerID != 0)
			{
				/*
				 * Build the multi-string 
				 */
				strcpy(&mszReaders[lastChrPtr],
					(readerStates[i])->readerName);
				lastChrPtr += strlen((readerStates[i])->readerName);
				mszReaders[lastChrPtr] = 0;	/* Add the null */
				lastChrPtr += 1;
			}
		}
		mszReaders[lastChrPtr] = 0;	/* Add the last null */
	}

	*pcchReaders = dwReadersLen;
	return SCARD_S_SUCCESS;
}

LONG SCardListReaderGroups(SCARDCONTEXT hContext, LPSTR mszGroups,
	LPDWORD pcchGroups)
{

	long rv;

	SCardLockThread();
	rv = SCardListReaderGroupsTH(hContext, mszGroups, pcchGroups);
	SCardUnlockThread();

	return rv;
}

/*
 * For compatibility purposes only 
 */
LONG SCardListReaderGroupsTH(SCARDCONTEXT hContext, LPSTR mszGroups,
	LPDWORD pcchGroups)
{

	LONG rv = SCARD_S_SUCCESS;

	const char ReaderGroup[] = "SCard$DefaultReaders";
	const int dwGroups = strlen(ReaderGroup) + 2;

	/*
	 * Make sure this context has been opened 
	 */
	if (SCardGetContextIndice(hContext) == -1)
	{
		return SCARD_E_INVALID_HANDLE;
	}

	if (SCardCheckDaemonAvailability() != SCARD_S_SUCCESS)
		return SCARD_E_NO_SERVICE;

	if (mszGroups)
	{

		if (*pcchGroups < dwGroups)
			rv = SCARD_E_INSUFFICIENT_BUFFER;
		else
		{
			memset(mszGroups, 0, dwGroups);
			memcpy(mszGroups, ReaderGroup, strlen(ReaderGroup));
		}
	}

	*pcchGroups = dwGroups;

	return rv;
}

LONG SCardCancel(SCARDCONTEXT hContext)
{

	long rv;

	SCardLockThread();
	rv = SCardCancelTH(hContext);
	SCardUnlockThread();

	return rv;
}

LONG SCardCancelTH(SCARDCONTEXT hContext)
{

	LONG hContextIndice;

	hContextIndice = SCardGetContextIndice(hContext);

	if (hContextIndice == -1)
	{
		return SCARD_E_INVALID_HANDLE;
	}

	/*
	 * Set the block status for this Context so blocking calls will
	 * complete 
	 */
	psContextMap[hContextIndice].contextBlockStatus = BLOCK_STATUS_RESUME;

	return SCARD_S_SUCCESS;
}

/*
 * Functions for managing instances of SCardEstablishContext These
 * functions keep track of Context handles and associate the blocking
 * variable contextBlockStatus to an hContext 
 */

static LONG SCardAddContext(SCARDCONTEXT hContext)
{

	int i;
	i = 0;

	for (i = 0; i < PCSCLITE_MAX_CONTEXTS; i++)
	{
		if (psContextMap[i].hContext == hContext)
		{
			return SCARD_S_SUCCESS;
		}
	}

	for (i = 0; i < PCSCLITE_MAX_CONTEXTS; i++)
	{
		if (psContextMap[i].hContext == 0)
		{
			psContextMap[i].hContext = hContext;
			psContextMap[i].contextBlockStatus = BLOCK_STATUS_RESUME;
			return SCARD_S_SUCCESS;
		}
	}

	return SCARD_E_NO_MEMORY;
}

static LONG SCardGetContextIndice(SCARDCONTEXT hContext)
{

	int i;
	i = 0;

	/*
	 * Find this context and return it's spot in the array 
	 */
	for (i = 0; i < PCSCLITE_MAX_CONTEXTS; i++)
	{
		if ((hContext == psContextMap[i].hContext) && (hContext != 0))
		{
			return i;
		}
	}

	return -1;
}

static LONG SCardRemoveContext(SCARDCONTEXT hContext)
{

	LONG  retIndice;
	UCHAR isLastContext;

	retIndice = 0; isLastContext = 1;
	retIndice = SCardGetContextIndice(hContext);

	if (retIndice == -1)
	{
		return SCARD_E_INVALID_HANDLE;
	} else
	{
		psContextMap[retIndice].hContext = 0;
		psContextMap[retIndice].contextBlockStatus = 
		  BLOCK_STATUS_RESUME;
		return SCARD_S_SUCCESS;
	}

}

/*
 * Functions for managing hCard values returned from SCardConnect. 
 */

LONG SCardGetHandleIndice(SCARDHANDLE hCard)
{

	int i;
	i = 0;

	for (i = 0; i < PCSCLITE_MAX_CONTEXTS; i++)
	{
		if ((hCard == psChannelMap[i].hCard) && (hCard != 0))
		{
			return i;
		}
	}

	return -1;
}

LONG SCardAddHandle(SCARDHANDLE hCard, LPSTR readerName)
{

	int i;
	i = 0;

	for (i = 0; i < PCSCLITE_MAX_CONTEXTS; i++)
	{
		if (psChannelMap[i].hCard == hCard)
		{
			return SCARD_S_SUCCESS;
		}
	}

	for (i = 0; i < PCSCLITE_MAX_CONTEXTS; i++)
	{
		if (psChannelMap[i].hCard == 0)
		{
			psChannelMap[i].hCard = hCard;
			psChannelMap[i].readerName = strdup(readerName);
			return SCARD_S_SUCCESS;
		}
	}

	return SCARD_E_NO_MEMORY;
}

LONG SCardRemoveHandle(SCARDHANDLE hCard)
{

	LONG retIndice;
	retIndice = 0;

	retIndice = SCardGetHandleIndice(hCard);

	if (retIndice == -1)
	{
		return SCARD_E_INVALID_HANDLE;
	} else
	{
		psChannelMap[retIndice].hCard = 0;
		free(psChannelMap[retIndice].readerName);
		return SCARD_S_SUCCESS;
	}
}

/*
 * This function sets up the mutex when the first call to EstablishContext 
 * is made by the first thread 
 */

/*
 * LONG SCardSetupThreadSafety() {
 * 
 * #ifdef USE_THREAD_SAFETY return 0; #else return 0; #endif }
 */

/*
 * This function locks a mutex so another thread must wait to use this
 * function 
 */

LONG SCardLockThread()
{

#ifdef USE_THREAD_SAFETY
	return SYS_MutexLock(&clientMutex);
#else
	return 0;
#endif

}

/*
 * This function unlocks a mutex so another thread may use the client
 * library 
 */

LONG SCardUnlockThread()
{

#ifdef USE_THREAD_SAFETY
	return SYS_MutexUnLock(&clientMutex);
#else
	return 0;
#endif

}

LONG SCardCheckDaemonAvailability()
{

	LONG rv;
	struct stat statBuffer;

	rv = SYS_Stat(PCSCLITE_IPC_DIR, &statBuffer);

	if (rv != 0)
	{
		debug_msg("SCardCheckDaemonAvailability: PCSC Not Running\n");
		return SCARD_E_NO_SERVICE;
	}

	return SCARD_S_SUCCESS;
}

/*
 * This function takes an error response and checks to see if the reader
 * is still available if it is it returns the original errorCode, if not
 * it returns SCARD_E_READER_UNAVAILABLE 
 */

LONG SCardCheckReaderAvailability(LPSTR readerName, LONG errorCode)
{

	LONG retIndice;
	int i;

	retIndice = 0;
	i = 0;

	if (errorCode != SCARD_S_SUCCESS)
	{
		for (i = 0; i < PCSCLITE_MAX_CONTEXTS; i++)
		{
			if (strcmp(psChannelMap[i].readerName, readerName) == 0)
			{
				return errorCode;
			}
		}

		return SCARD_E_READER_UNAVAILABLE;

	} else
	{
		return SCARD_S_SUCCESS;
	}

}


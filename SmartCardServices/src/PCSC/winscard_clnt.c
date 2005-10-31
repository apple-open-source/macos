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

$Id: winscard_clnt.c,v 1.3.30.1 2005/06/17 22:40:12 mb Exp $

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



/* Client context not in use */
#define CONTEXT_STATUS_FREE             0x0000
/* Client context is allocated, but no pcscd context established yet. */
#define CONTEXT_STATUS_ALLOCATED        0x0001
/* Client context is allocated, and we have a valid pcscd context. */
#define CONTEXT_STATUS_CONNECTED        0x0002


static int appSocket = 0;

static struct _psChannelMap
{
	SCARDHANDLE hCard;
	LPSTR readerName;
}
psChannelMap[PCSCLITE_MAX_CONTEXTS];

static struct _psContextMap
{
	SCARDCONTEXT hServerContext; /* Server side context handle. */
	DWORD contextBlockStatus;
	DWORD contextConnectStatus; /* Status of this context slot. */
	DWORD dwScope;
}
psContextMap[PCSCLITE_MAX_CONTEXTS];

static short isConnected = 0;
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

static void SCardInitializeOnce(void);
static LONG SCardEnsureClientSession(void);
static LONG SCardSetupClientSession(void);
static void SCardCloseClientSession(void);
static LONG SCardGetServerContext(SCARDCONTEXT hContext,
	LPSCARDCONTEXT phContext);

static LONG SCardGetContextIndice(SCARDCONTEXT);

static LONG SCardAddHandle(SCARDHANDLE, LPSTR);
static LONG SCardGetHandleIndice(SCARDHANDLE);
static LONG SCardRemoveHandle(SCARDHANDLE);
static LONG SCardEnsureHandleIndices(SCARDHANDLE hCard, LONG *pliChannelIndex,
	LONG *pliReaderIndex);

static LONG SCardCheckReaderAvailability(LPSTR, LONG);

/*
 * Thread safety functions 
 */
static LONG SCardLockThread(void);
static LONG SCardUnlockThread(void);

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
			("MSGClientSetupSession: Error: create on client socket: %s",
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
			("MSGClientSetupSession: Error: connect to client socket: %s",
			strerror(errno));

		SYS_CloseFile(appSocket);
		return -1;
	}

	one = 1;
	if (ioctl(appSocket, FIONBIO, &one) < 0)
	{
		DebugLogB("MSGClientSetupSession: Error: cannot set socket "
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

	/* If the message send failed we close the connection. */
	if (retval)
		SCardCloseClientSession();

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

	/* If the message receive failed we close the connection. */
	if (retval)
		SCardCloseClientSession();

	return retval;
}


static void SCardInitializeOnce(void)
{
	int i;

	g_rgSCardT0Pci.dwProtocol = SCARD_PROTOCOL_T0;
	g_rgSCardT1Pci.dwProtocol = SCARD_PROTOCOL_T1;
	g_rgSCardRawPci.dwProtocol = SCARD_PROTOCOL_RAW;

	/*
	 * Do any system initilization here 
	 */
	SYS_Initialize();

	/*
	 * Initially set the context, hcard structs to zero 
	 */
	for (i = 0; i < PCSCLITE_MAX_CONTEXTS; i++)
	{
		psChannelMap[i].hCard = 0;
		psChannelMap[i].readerName = NULL;
		psContextMap[i].hServerContext = 0;
		psContextMap[i].contextConnectStatus = CONTEXT_STATUS_FREE;
		psContextMap[i].contextBlockStatus = BLOCK_STATUS_RESUME;
		psContextMap[i].dwScope = 0;

		readerStates[i] = NULL;
	}
}

/*
 * By najam 
 */
LONG SCardEstablishContext(DWORD dwScope, LPCVOID pvReserved1,
	LPCVOID pvReserved2, LPSCARDCONTEXT phContext)
{
	LONG rv;

	SCardLockThread();
	rv = SCardEstablishContextTH(dwScope, pvReserved1,
		pvReserved2, phContext);
	SCardUnlockThread();

	return rv;

}

/*
 *  SCardEstablishContextTH Doesn't actually connect to pcscd.  This is
 *  deferred until a context is actually used for something.
 */
static LONG SCardEstablishContextTH(DWORD dwScope, LPCVOID pvReserved1,
	LPCVOID pvReserved2, LPSCARDCONTEXT phContext)
{
	int i;

	/* Since this is the first API any PCSC client will ever call, we do
	   our one-time initialization from here.  Make sure SCardInitializeOnce
	   is only called once. */
	if (isExecuted == 0)
	{
		SCardInitializeOnce();
		isExecuted = 1;
	}

	if (phContext == 0)
		return SCARD_E_INVALID_PARAMETER;

	if (dwScope != SCARD_SCOPE_USER && dwScope != SCARD_SCOPE_TERMINAL &&
		dwScope != SCARD_SCOPE_SYSTEM && dwScope != SCARD_SCOPE_GLOBAL)
		return SCARD_E_INVALID_VALUE;

	/*
	 * Allocate a slot in the psContextMap array and return it in *phContext.
	 * Return SCARD_E_NO_MEMORY if there are no available slots.
	 */
	for (i = 0; i < PCSCLITE_MAX_CONTEXTS; i++)
	{
		if (psContextMap[i].contextConnectStatus == CONTEXT_STATUS_FREE)
		{
			psContextMap[i].hServerContext = 0;
			psContextMap[i].contextBlockStatus = BLOCK_STATUS_RESUME;
			psContextMap[i].contextConnectStatus = CONTEXT_STATUS_ALLOCATED;
			psContextMap[i].dwScope = dwScope;
			*phContext = i + 1;
			return SCARD_S_SUCCESS;
		}
	}

	*phContext = 0;
	return SCARD_E_NO_MEMORY;
}

/*
 * by najam 
 */
LONG SCardReleaseContext(SCARDCONTEXT hContext)
{
	LONG rv;

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
	LONG contextIndice;
	LONG rv;

	/*
	 * Make sure this context has been opened 
	 */
	contextIndice = SCardGetContextIndice(hContext);
	if (contextIndice == -1)
		return SCARD_E_INVALID_HANDLE;

	/* We only need to tell pcscd about the context going away if pcscd knows
	   about it in the first place.  This is only the case when the context
	   is connected. */
	if (psContextMap[contextIndice].contextConnectStatus
		== CONTEXT_STATUS_CONNECTED)
	{
		release_request request;
		release_reply reply;

		request.hContext = psContextMap[contextIndice].hServerContext;

		/* We ignore all the server dieing during a SCardReleaseContext,
		   since that just means we completed the release. */
		rv = MSGClientSendRequest(SCARD_RELEASE_CONTEXT,
			PCSCLITE_MCLIENT_ATTEMPTS, &request, sizeof(request), NULL, 0);
		if (rv)
			rv = SCARD_S_SUCCESS;
		else
		{
			rv = MSGClientReceiveReply(&reply, sizeof(reply), NULL, 0);
			if (rv)
				rv = SCARD_S_SUCCESS;
			else
				rv = reply.header.rv;
		}
	}

	if (!rv)
	{
		psContextMap[contextIndice].hServerContext = 0;
		/* Cancel any pending SCardGetStatusChange() calls. */
		psContextMap[contextIndice].contextBlockStatus = BLOCK_STATUS_RESUME;
		psContextMap[contextIndice].contextConnectStatus = CONTEXT_STATUS_FREE;
		psContextMap[contextIndice].dwScope = 0;
	}

	return rv;
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
	LONG rv;

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

	if (!(dwPreferredProtocols & SCARD_PROTOCOL_T0) &&
		!(dwPreferredProtocols & SCARD_PROTOCOL_T1) &&
		!(dwPreferredProtocols & SCARD_PROTOCOL_RAW) &&
		!(dwPreferredProtocols & SCARD_PROTOCOL_ANY))
	{
		return SCARD_E_INVALID_VALUE;
	}

	rv = SCardGetServerContext(hContext, &request.hContext);
	if (rv)
	{
		/* Attempting to connect to a reader when pcscd isn't running is
		   the same as connecting to an unknown reader since pcscd not running
		   implies there are 0 readers available. */
		if (rv == SCARD_E_NO_SERVICE)
			return SCARD_E_UNKNOWN_READER;

		return rv;
	}

	/* @@@ Consider sending this as additionalData instead. */
	strncpy(request.szReader, szReader, MAX_READERNAME);
	
	request.dwShareMode = dwShareMode;
	request.dwPreferredProtocols = dwPreferredProtocols;

	rv = MSGClientSendRequest(SCARD_CONNECT,
		PCSCLITE_CLIENT_ATTEMPTS, &request, sizeof(request), NULL, 0);
	if (rv)
		return SCARD_E_UNKNOWN_READER;

	/*
	 * Read a message from the server 
	 */
	rv = MSGClientReceiveReply(&reply, sizeof(reply), NULL, 0);
	if (rv)
		return SCARD_E_UNKNOWN_READER;

	if (reply.header.rv == SCARD_S_SUCCESS)
	{
		/*
		 * Keep track of the handle locally 
		 */
		*pdwActiveProtocol = reply.pdwActiveProtocol;
		rv = SCardAddHandle(reply.phCard, (LPSTR) szReader);
		if (rv)
			return rv;

		*phCard = reply.phCard;
	}

	return reply.header.rv;
}

LONG SCardReconnect(SCARDHANDLE hCard, DWORD dwShareMode,
	DWORD dwPreferredProtocols, DWORD dwInitialization,
	LPDWORD pdwActiveProtocol)
{
	LONG rv;

	SCardLockThread();
	rv = SCardReconnectTH(hCard, dwShareMode, dwPreferredProtocols,
		dwInitialization, pdwActiveProtocol);
	SCardUnlockThread();
	return rv;

}

static LONG SCardReconnectTH(SCARDHANDLE hCard, DWORD dwShareMode,
	DWORD dwPreferredProtocols, DWORD dwInitialization,
	LPDWORD pdwActiveProtocol)
{

	LONG liIndex, rv;
	reconnect_request request;
	reconnect_reply reply;

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

	rv = SCardEnsureHandleIndices(hCard, &liIndex, NULL);
	if (rv)
		return rv;

	request.hCard = hCard;
	request.dwShareMode = dwShareMode;
	request.dwPreferredProtocols = dwPreferredProtocols;
	request.dwInitialization = dwInitialization;

	rv = MSGClientSendRequest(SCARD_RECONNECT,
		PCSCLITE_CLIENT_ATTEMPTS, &request, sizeof(request), NULL, 0);
	if (rv)
		return SCARD_E_READER_UNAVAILABLE;

	/*
	 * Read a message from the server 
	 */
	rv = MSGClientReceiveReply(&reply, sizeof(reply), NULL, 0);
	if (rv)
		return SCARD_E_READER_UNAVAILABLE;

	*pdwActiveProtocol = reply.pdwActiveProtocol;

	return SCardCheckReaderAvailability(psChannelMap[liIndex].readerName,
		reply.header.rv);
}

/*
 * by najam 
 */
LONG SCardDisconnect(SCARDHANDLE hCard, DWORD dwDisposition)
{

	LONG rv;

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

	LONG rv;
	disconnect_request request;
	disconnect_reply reply;

	if (dwDisposition != SCARD_LEAVE_CARD &&
		dwDisposition != SCARD_RESET_CARD &&
		dwDisposition != SCARD_UNPOWER_CARD &&
		dwDisposition != SCARD_EJECT_CARD)
	{
		return SCARD_E_INVALID_VALUE;
	}

	/* In theory we could end up returning invalid handle here if pcscd went
	   away and some other call detected that, since this would end up erasing
	   all know handles.  @@@ We should investigate not wiping the handle map
	   on disconnect, but flagging the entires as stale instead somehow.  */
	rv = SCardRemoveHandle(hCard);
	if (rv)
		return rv;

	rv = SCardEnsureClientSession();
	if (rv)
	{
		/* If pcscd went away we successfully removed the local handle,
		   and the reader is now gone so we are done. */
		if (rv == SCARD_E_NO_SERVICE)
			return SCARD_S_SUCCESS;

		return rv;
	}

	request.hCard = hCard;
	request.dwDisposition = dwDisposition;

	rv = MSGClientSendRequest(SCARD_DISCONNECT,
		PCSCLITE_CLIENT_ATTEMPTS, &request, sizeof(request), NULL, 0);
	if (rv)
		return SCARD_S_SUCCESS;

	/*
	 * Read a message from the server 
	 */
	rv = MSGClientReceiveReply(&reply, sizeof(reply), NULL, 0);
	if (rv)
		return SCARD_S_SUCCESS;

	return reply.header.rv;
}

LONG SCardBeginTransaction(SCARDHANDLE hCard)
{

	LONG i, rv;
	begin_request request;
	begin_reply reply;
	int timeval, randnum, j;

	/*
	 * Zero out everything 
	 */
	timeval = 0;
	randnum = 0;
	i = 0;

	SCardLockThread();
	rv = SCardEnsureHandleIndices(hCard, NULL, &i);
	SCardUnlockThread();
	if (rv)
		return rv;

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

		/*
		 * Begin lock 
		 */
		SCardLockThread();
		rv = SCardEnsureClientSession();
		if (rv)
		{
			/*
			 * End of lock 
			 */
			SCardUnlockThread();
			if (rv == SCARD_E_NO_SERVICE)
				return SCARD_E_READER_UNAVAILABLE;

			return rv;
		}
		rv = MSGClientSendRequest(SCARD_BEGIN_TRANSACTION,
			PCSCLITE_CLIENT_ATTEMPTS, &request, sizeof(request), NULL, 0);
		if (rv)
		{
			/*
			 * End of lock 
			 */
			SCardUnlockThread();
			return SCARD_E_READER_UNAVAILABLE;
		}

		/*
		 * Read a message from the server 
		 */
		rv = MSGClientReceiveReply(&reply, sizeof(reply), NULL, 0);
		/*
		 * End of lock 
		 */
		SCardUnlockThread();
		if (rv)
			return SCARD_E_READER_UNAVAILABLE;

	}
	while (reply.header.rv == SCARD_E_SHARING_VIOLATION);

	return reply.header.rv;
}

LONG SCardEndTransaction(SCARDHANDLE hCard, DWORD dwDisposition)
{
	LONG rv;

	SCardLockThread();
	rv = SCardEndTransactionTH(hCard, dwDisposition);
	SCardUnlockThread();
	return rv;
}

static LONG SCardEndTransactionTH(SCARDHANDLE hCard, DWORD dwDisposition)
{

	LONG rv;
	end_request request;
	end_reply reply;
	int randnum;

	/*
	 * Zero out everything 
	 */
	randnum = 0;

	if (dwDisposition != SCARD_LEAVE_CARD &&
		dwDisposition != SCARD_RESET_CARD &&
		dwDisposition != SCARD_UNPOWER_CARD &&
		dwDisposition != SCARD_EJECT_CARD)
	{
		return SCARD_E_INVALID_VALUE;
	}

	rv = SCardEnsureHandleIndices(hCard, NULL, NULL);
	if (rv)
		return rv;

	request.hCard = hCard;
	request.dwDisposition = dwDisposition;

	rv = MSGClientSendRequest(SCARD_END_TRANSACTION,
		PCSCLITE_CLIENT_ATTEMPTS, &request, sizeof(request), NULL, 0);
	if (rv)
		return SCARD_E_READER_UNAVAILABLE;

	/*
	 * Read a message from the server 
	 */
	rv = MSGClientReceiveReply(&reply, sizeof(reply), NULL, 0);
	if (rv)
		return SCARD_E_READER_UNAVAILABLE;

	/*
	 * This helps prevent starvation 
	 */
	randnum = SYS_Random(randnum, 1000.0, 10000.0);
	SYS_USleep(randnum);

	return reply.header.rv;
}

LONG SCardCancelTransaction(SCARDHANDLE hCard)
{
	LONG rv;

	SCardLockThread();
	rv = SCardCancelTransactionTH(hCard);
	SCardUnlockThread();
	return rv;
}

static LONG SCardCancelTransactionTH(SCARDHANDLE hCard)
{
	LONG rv;
	cancel_request request;
	cancel_reply reply;

	rv = SCardEnsureHandleIndices(hCard, NULL, NULL);
	if (rv)
		return rv;

	request.hCard = hCard;

	rv = MSGClientSendRequest(SCARD_CANCEL_TRANSACTION,
		PCSCLITE_CLIENT_ATTEMPTS, &request, sizeof(request), NULL, 0);
	if (rv)
		return SCARD_E_READER_UNAVAILABLE;

	/*
	 * Read a message from the server 
	 */
	rv = MSGClientReceiveReply(&reply, sizeof(reply), NULL, 0);
	if (rv)
		return SCARD_E_READER_UNAVAILABLE;

	return reply.header.rv;
}

LONG SCardStatus(SCARDHANDLE hCard, LPSTR mszReaderNames,
	LPDWORD pcchReaderLen, LPDWORD pdwState,
	LPDWORD pdwProtocol, LPBYTE pbAtr, LPDWORD pcbAtrLen)
{
	LONG rv;

	SCardLockThread();
	rv = SCardStatusTH(hCard, mszReaderNames, pcchReaderLen, pdwState,
		pdwProtocol, pbAtr, pcbAtrLen);
	SCardUnlockThread();
	return rv;
}

static LONG SCardStatusTH(SCARDHANDLE hCard, LPSTR mszReaderNames,
	LPDWORD pcchReaderLen, LPDWORD pdwState,
	LPDWORD pdwProtocol, LPBYTE pbAtr, LPDWORD pcbAtrLen)
{

	DWORD dwReaderLen;
	LONG liIndex, rv;
	LONG i;
	status_request request;
	status_reply reply;

	/*
	 * Zero out everything 
	 */
	liIndex = 0;
	i = 0;

	/*
	 * Check for NULL parameters 
	 */
	if (pcchReaderLen == 0 || pdwState == 0 ||
		pdwProtocol == 0 || pcbAtrLen == 0)
	{
		return SCARD_E_INVALID_PARAMETER;
	}

	rv = SCardEnsureHandleIndices(hCard, &liIndex, &i);
	if (rv)
	{
		*pcchReaderLen = 0;
		*pcbAtrLen = 0;
		*pdwState = 0;
		*pdwProtocol = 0;
		return rv;
	}

	/*
	 * A small call must be made to pcscd to find out the event status of
	 * other applications such as reset/removed.  Only hCard is needed so
	 * I will not fill in the other information. 
	 */
	request.hCard = hCard;

	rv = MSGClientSendRequest(SCARD_STATUS,
		PCSCLITE_CLIENT_ATTEMPTS, &request, sizeof(request), NULL, 0);
	if (rv)
	{
		*pcchReaderLen = 0;
		*pcbAtrLen = 0;
		*pdwState = 0;
		*pdwProtocol = 0;
		return SCARD_E_READER_UNAVAILABLE;
	}

	/*
	 * Read a message from the server 
	 */
	rv = MSGClientReceiveReply(&reply, sizeof(reply), NULL, 0);
	if (rv)
	{
		*pcchReaderLen = 0;
		*pcbAtrLen = 0;
		*pdwState = 0;
		*pdwProtocol = 0;
		return SCARD_E_READER_UNAVAILABLE;
	}

	if (reply.header.rv)
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


	/*
	 * Application is waiting for a reader - return the first available
	 * reader 
	 */

	if (cReaders == 0)
	{
		while (1)
		{
			rv = SCardEnsureClientSession();
			if (rv && rv != SCARD_E_NO_SERVICE)
				return rv;

			if (rv == SCARD_S_SUCCESS)
			{
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

	rv = SCardEnsureClientSession();
	if (rv)
	{
		if (rv == SCARD_E_NO_SERVICE)
			return SCARD_E_READER_UNAVAILABLE;

		return rv;
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
		rv = SCardEnsureClientSession();
		if (rv)
		{
			if (rv == SCARD_E_NO_SERVICE)
				return SCARD_E_READER_UNAVAILABLE;

			return rv;
		}

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
				if (lpcReaderName && readerStates[i]->readerName &&
					!strcmp(lpcReaderName, readerStates[i]->readerName))
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

	LONG rv;

	SCardLockThread();
	rv = SCardTransmitTH(hCard, pioSendPci, pbSendBuffer, cbSendLength,
		pioRecvPci, pbRecvBuffer, pcbRecvLength);
	SCardUnlockThread();

	return rv;

}

/*
 * --------by najam 
 */

static LONG SCardTransmitTH(SCARDHANDLE hCard, LPCSCARD_IO_REQUEST pioSendPci,
	LPCBYTE pbSendBuffer, DWORD cbSendLength,
	LPSCARD_IO_REQUEST pioRecvPci, LPBYTE pbRecvBuffer,
	LPDWORD pcbRecvLength)
{

	LONG liIndex, rv;
	transmit_request request;
	transmit_reply reply;

	/*
	 * Zero out everything 
	 */
	liIndex = 0;
	rv = 0;

	if (pbSendBuffer == 0 || pbRecvBuffer == 0 ||
		pcbRecvLength == 0 || pioSendPci == 0)
	{
		return SCARD_E_INVALID_PARAMETER;
	}

	rv = SCardEnsureHandleIndices(hCard, &liIndex, NULL);
	if (rv)
	{
		*pcbRecvLength = 0;
		return rv;
	}

	request.hCard = hCard;
	request.header.additional_data_size = cbSendLength;
	request.cbMaxRecvLength = *pcbRecvLength;
	memcpy(&request.pioSendPci, pioSendPci, sizeof(SCARD_IO_REQUEST));

	rv = MSGClientSendRequest(SCARD_TRANSMIT,
		PCSCLITE_CLIENT_ATTEMPTS, &request, sizeof(request), pbSendBuffer,
		cbSendLength);
	if (rv)
	{
		*pcbRecvLength = 0;
		return SCARD_E_READER_UNAVAILABLE;
	}

	/*
	 * Read a message from the server 
	 */
	rv = MSGClientReceiveReply(&reply, sizeof(reply), pbRecvBuffer,
		request.cbMaxRecvLength);
	if (rv)
	{
		*pcbRecvLength = 0;
		return SCARD_E_READER_UNAVAILABLE;
	}

	*pcbRecvLength = reply.cbRecvLength;
	if (reply.header.rv)
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

	LONG rv;

	SCardLockThread();
	rv = SCardListReadersTH(hContext, mszGroups, mszReaders, pcchReaders);
	SCardUnlockThread();

	return rv;
}

static LONG SCardListReadersTH(SCARDCONTEXT hContext, LPCSTR mszGroups,
	LPSTR mszReaders, LPDWORD pcchReaders)
{
	LONG rv, liIndex;
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

	/* Attempt to (re)connect to pcscd, but don't fail if it's not there. */
	rv = SCardEnsureClientSession();
	if (rv && rv != SCARD_E_NO_SERVICE)
		return rv;

	/* If we aren't connected we just pretend there are no readers
	   available. */
	if (!rv)
	{
		for (i = 0; i < PCSCLITE_MAX_CONTEXTS; i++)
		{
			if ((readerStates[i])->readerID != 0)
			{
				dwReadersLen += (readerStates[i]->readerName ?
					strlen((readerStates[i])->readerName) + 1 : 1);
			}
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
		if (!rv)
		{
			for (i = 0; i < PCSCLITE_MAX_CONTEXTS; i++)
			{
				if ((readerStates[i])->readerID != 0)
				{
					/*
					 * Build the multi-string 
					 */
					if (readerStates[i]->readerName)
					{
						int readerLen = strlen(readerStates[i]->readerName);
						memcpy(&mszReaders[lastChrPtr],
							   readerStates[i]->readerName, readerLen);
						lastChrPtr += readerLen;
					}
					mszReaders[lastChrPtr++] = 0;	/* Add the null */
				}
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

	LONG rv;

	SCardLockThread();
	rv = SCardListReaderGroupsTH(hContext, mszGroups, pcchGroups);
	SCardUnlockThread();

	return rv;
}

/*
 * For compatibility purposes only, this doesn't fail if we aren't connected,
 * nor does it attempt to connect.
 */
static LONG SCardListReaderGroupsTH(SCARDCONTEXT hContext, LPSTR mszGroups,
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

	LONG rv;

	SCardLockThread();
	rv = SCardCancelTH(hContext);
	SCardUnlockThread();

	return rv;
}

/*
 * This function doesn't fail if we aren't connected, to pcscd, nor does
 * it attempt to connect.
 */
static LONG SCardCancelTH(SCARDCONTEXT hContext)
{

	LONG hContextIndice;

	hContextIndice = SCardGetContextIndice(hContext);
	if (hContextIndice == -1)
	{
		return SCARD_E_INVALID_HANDLE;
	}

	/* Cancel any pending SCardGetStatusChange() calls. */
	psContextMap[hContextIndice].contextBlockStatus = BLOCK_STATUS_RESUME;

	return SCARD_S_SUCCESS;
}

/* Check to see if our connection to pcscd is still alive and reconnect if
   it's not.  */
static LONG SCardEnsureClientSession(void)
{
	if (isConnected)
	{
		char buf[1];

		/* We think we are connected, let's make sure we really are by peeking
		   ahead and seeing if we can read 1 byte from our socket.  If we are
		   connected this will return EAGAIN, if not it will return 0 bytes
		   read, to indicate that the other end of the socket was closed.  */
		int num_bytes;
		do
		{
			num_bytes = recvfrom(appSocket, buf, 1, MSG_PEEK, NULL, NULL);
			if (num_bytes == -1 && errno == EAGAIN)
			{
				/* Looks good we're still connected. */
				return SCARD_S_SUCCESS;
			}
		} while (num_bytes == -1 && errno == EINTR);

		SCardCloseClientSession();
	}

	return SCardSetupClientSession();
}

static LONG SCardSetupClientSession(void)
{
	int pageSize;
	int i;

	if (MSGClientSetupSession() != 0)
		return SCARD_E_NO_SERVICE;

	/*
	 * Set up the memory mapped reader stats structures 
	 */
	mapAddr = SYS_OpenFile(PCSCLITE_PUBSHM_FILE, O_RDONLY, 0);
	if (mapAddr < 0)
	{
		/* We successfully connected to the socket but we can't map the
		   shared memory region, so bail out, this could be a race if pcscd
		   was going down just as we were connecting. */
		DebugLogA("ERROR: Cannot open public shared file");
		MSGClientCloseSession();
		return SCARD_E_NO_SERVICE;
	}

	pageSize = SYS_GetPageSize();

	for (i = 0; i < PCSCLITE_MAX_CONTEXTS; i++)
	{
		/* Map each reader structure. */
		readerStates[i] = (PREADER_STATES)
			SYS_PublicMemoryMap(sizeof(READER_STATES),
			mapAddr, (i * pageSize));
		if (readerStates[i] == 0)
		{
			DebugLogA("ERROR: Cannot public memory map");
			SCardCloseClientSession();	/* Cleanup everything */
			return SCARD_F_INTERNAL_ERROR;
		}
	}

	/* All done we're connected (again). */
	isConnected = 1;

	return SCARD_S_SUCCESS;
}

static void SCardCloseClientSession(void)
{
	int i;

	/* We are no longer connected. */
	isConnected = 0;

	/* Close the unix domain socket. */
	MSGClientCloseSession();

	for (i = 0; i < PCSCLITE_MAX_CONTEXTS; i++)
	{
		/* All card handles and readerNames get reset to their initial
		   state. */
		if (psChannelMap[i].hCard)
		{
			psChannelMap[i].hCard = 0;
			if (psChannelMap[i].readerName)
			{
				free(psChannelMap[i].readerName);
				psChannelMap[i].readerName = NULL;
			}
		}

		if (readerStates[i])
		{
			if (SYS_MUnmap(readerStates[i], sizeof(READER_STATES)) != 0)
				DebugLogA("ERROR: Cannot unmap public memory map");

			readerStates[i] = NULL;
		}

		/* For each context which is connected we revert to allocated.   The
		   next time a context is used it will trigger a reconnect. */
		if (psContextMap[i].contextConnectStatus == CONTEXT_STATUS_CONNECTED)
		{
			/* Don't mess with contextBlockStatus since that causes pending
			   SCardGetStatusChange() calls to potentially return
			   SCARD_E_CANCELLED instead of SCARD_E_READER_UNAVAILABLE. */
			psContextMap[i].contextConnectStatus = CONTEXT_STATUS_ALLOCATED;
			psContextMap[i].hServerContext = 0;
		}
	}

	SYS_CloseFile(mapAddr);	/* Close the memory map file */
}

static LONG SCardGetServerContext(SCARDCONTEXT hContext,
	LPSCARDCONTEXT phContext)
{
	LONG rv, contextIndice;

	contextIndice = SCardGetContextIndice(hContext);
	if (contextIndice == -1)
		return SCARD_E_INVALID_HANDLE;

	rv = SCardEnsureClientSession();
	if (rv)
		return rv;

	if (psContextMap[contextIndice].contextConnectStatus ==
		CONTEXT_STATUS_ALLOCATED)
	{
		/* Context is allocated but not yet connected to server, so let's
		   attempt to do that now. */
		establish_request request;
		establish_reply reply;

		request.dwScope = psContextMap[contextIndice].dwScope;

		rv = MSGClientSendRequest(SCARD_ESTABLISH_CONTEXT,
			PCSCLITE_MCLIENT_ATTEMPTS, &request, sizeof(request), NULL, 0);
		if (rv)
		{
			return SCARD_E_NO_SERVICE;
		}

		/*
		 * Read a message from the server 
		 */
		rv = MSGClientReceiveReply(&reply, sizeof(reply), NULL, 0);
		if (rv)
		{
			return SCARD_E_NO_SERVICE;
		}

		if (reply.header.rv)
		{
			return reply.header.rv;
		}

		psContextMap[contextIndice].contextConnectStatus =
			CONTEXT_STATUS_CONNECTED;
		psContextMap[contextIndice].hServerContext = reply.phContext;
	}

	*phContext = psContextMap[contextIndice].hServerContext;

	return rv;
}


/*
 * Find this hContext and return it's spot in the psContextMap array 
 */
static LONG SCardGetContextIndice(SCARDCONTEXT hContext)
{
	if (hContext > 0 && hContext <= PCSCLITE_MAX_CONTEXTS
		&& psContextMap[hContext - 1].contextConnectStatus !=
			CONTEXT_STATUS_FREE)
		return hContext - 1;

	return -1;
}

/*
 * Functions for managing hCard values returned from SCardConnect. 
 */

static LONG SCardGetHandleIndice(SCARDHANDLE hCard)
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

static LONG SCardAddHandle(SCARDHANDLE hCard, LPSTR readerName)
{
	int i;
	i = 0;

	/* Make sure we didn't get a bogus hCard. */
	if (!hCard)
		return SCARD_F_INTERNAL_ERROR;

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

static LONG SCardRemoveHandle(SCARDHANDLE hCard)
{
	LONG retIndice;

	retIndice = SCardGetHandleIndice(hCard);

	if (retIndice == -1)
	{
		return SCARD_E_INVALID_HANDLE;
	}
	else
	{
		psChannelMap[retIndice].hCard = 0;
		free(psChannelMap[retIndice].readerName);
		psChannelMap[retIndice].readerName = NULL;
		return SCARD_S_SUCCESS;
	}
}

static LONG SCardEnsureHandleIndices(SCARDHANDLE hCard, LONG *pliChannelIndex,
	LONG *pliReaderIndex)
{
	LONG liChannel, liReader, rv;

	liChannel = SCardGetHandleIndice(hCard);
	if (liChannel < 0)
	{
		return SCARD_E_INVALID_HANDLE;
	}

	rv = SCardEnsureClientSession();
	if (rv)
	{
		if (rv == SCARD_E_NO_SERVICE)
			return SCARD_E_READER_UNAVAILABLE;

		return rv;
	}

	for (liReader = 0; liReader < PCSCLITE_MAX_CONTEXTS; ++liReader)
	{
		if (psChannelMap[liChannel].readerName &&
			!strcmp(psChannelMap[liChannel].readerName,
				readerStates[liReader]->readerName))
		{
			if (pliChannelIndex)
				*pliChannelIndex = liChannel;

			if (pliReaderIndex)
				*pliReaderIndex = liReader;

			return SCARD_S_SUCCESS;
		}
	}

	return SCARD_E_READER_UNAVAILABLE;
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

static LONG SCardLockThread(void)
{

#ifdef USE_THREAD_SAFETY
	return SYS_MutexLock(&clientMutex);
#else
	return SCARD_S_SUCCESS;
#endif

}

/*
 * This function unlocks a mutex so another thread may use the client
 * library 
 */

static LONG SCardUnlockThread(void)
{

#ifdef USE_THREAD_SAFETY
	return SYS_MutexUnLock(&clientMutex);
#else
	return SCARD_S_SUCCESS;
#endif

}

/*
 * This function takes an error response and checks to see if the reader
 * is still available if it is it returns the original errorCode, if not
 * it returns SCARD_E_READER_UNAVAILABLE 
 */

static LONG SCardCheckReaderAvailability(LPSTR readerName, LONG errorCode)
{
	int i;

	i = 0;

	if (errorCode)
	{
		for (i = 0; i < PCSCLITE_MAX_CONTEXTS; i++)
		{
			if (readerName && psChannelMap[i].readerName &&
				!strcmp(psChannelMap[i].readerName, readerName))
			{
				return errorCode;
			}
		}

		return SCARD_E_READER_UNAVAILABLE;
	}
	else
	{
		return SCARD_S_SUCCESS;
	}
}


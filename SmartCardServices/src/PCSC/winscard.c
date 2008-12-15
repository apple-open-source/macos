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
 *  winscard.c
 *  SmartCardServices
 */

/*
 * MUSCLE SmartCard Development ( http://www.linuxnet.com )
 *
 * Copyright (C) 1999-2004
 *  David Corcoran <corcoran@linuxnet.com>
 *  Ludovic Rousseau <ludovic.rousseau@free.fr>
 *
 * $Id: winscard.c 2385 2007-02-05 13:55:01Z rousseau $
 */

/**
 * @mainpage MUSCLE PC/SC-Lite API Documentation
 *
 * @section Introduction
 *
 * This document contains the reference API calls for communicating to the
 * MUSCLE PC/SC Smart Card Resource Manager. PC/SC is a standard proposed by
 * the PC/SC workgroup http://www.pcscworkgroup.com/ which is a conglomerate of
 * representative from major smart card manufacturers and other companies. This
 * specification tries to abstract the smart card layer into a high level API
 * so that smart cards and their readers can be accessed in a homogeneous
 * fashion.
 *
 * This toolkit was written in ANSI C that can be used with most compilers and
 * does NOT use complex and large data structures such as vectors, etc. The C
 * API emulates the winscard API that is used on the Windows platform. It is
 * contained in the library <tt>libpcsclite.so</tt> that is linked to your
 * application.
 *
 * I would really like to hear from you. If you have any feedback either on
 * this documentation or on the MUSCLE project please feel free to email me at:
 * corcoran@musclecard.com.
 *
 *
 * @section API Routines
 *
 * These routines specified here are winscard routines like those in the
 * winscard API provided under Windows(R). These are compatible with the
 * Microsoft(R) API calls. This list of calls is mainly an abstraction of
 * readers. It gives a common API for communication to most readers in a
 * homogeneous fashion.
 *
 * Since all functions can produce a wide array of errors, please refer to 
 * Error codes for a list of error returns.
 *
 * For a human readable representation of an error the function
 * pcsc_stringify_error() is declared in pcsclite.h. This function is not
 * available on Microsoft(R) winscard API and is pcsc-lite specific.
 *
 * @section Internals
 *
 * PC/SC Lite is formed by a server deamon (<tt>pcscd</tt>) and a client
 * library (<tt>libpcsclite.so</tt>) that communicate via IPC.
 *
 * The file \em winscard_clnt.c in the client-side exposes the API for
 * applications.\n The file \em winscard.c has the server-side counterpart
 * functions present in \em winscard_clnt.c.\n The file \em winscard_msg.c is
 * the communication interface between \em winscard_clnt.c and \em
 * winscard.c.\n The file pcscdaemon.c has the main server-side function,
 * including a loop for accepting client requests.\n The file \em
 * winscard_svc.c has the functions called by \em pcscdaemon.c to serve clients
 * requests.
 *
 * When a function from \em winscard_clnt.c is called by a client application,
 * it calls a function in \em winscard_msg.c to send the message to \em
 * pcscdaemon.c.  When \em pcscdaemon.c a client detects a request arrived, it
 * calls \em winscard_svc.c which identifies what command the message contains
 * and requests \em winscard.c to execute the command.\n Meanwhile
 * winscard_clnt.c waits for the response until a timeout occurs.
 */

/**
 * @file
 * @brief This handles smartcard reader communications.
 * This is the heart of the MS smartcard API.
 *
 * Here are the main server-side functions which execute the requests from the
 * clients.
 */

#include "config.h"
#include <stdlib.h>
#include <sys/time.h>
#include <string.h>

#include "wintypes.h"
#include "pcsclite.h"
#include "winscard.h"
#include "ifdhandler.h"
#include "debuglog.h"
#include "readerfactory.h"
#include "prothandler.h"
#include "ifdwrapper.h"
#include "atrhandler.h"
#include "configfile.h"
#include "sys_generic.h"
#include "eventhandler.h"
#include "readerstate.h"

#include <security_utilities/debugging.h>

/** used for backward compatibility */
#define SCARD_PROTOCOL_ANY_OLD	 0x1000

/** Some defines for context stack. */
#define SCARD_LAST_CONTEXT       1
/** Some defines for context stack. */
#define SCARD_NO_CONTEXT         0
/** Some defines for context stack. */
#define SCARD_EXCLUSIVE_CONTEXT -1
/** Some defines for context stack. */
#define SCARD_NO_LOCK            0

SCARD_IO_REQUEST g_rgSCardT0Pci = { SCARD_PROTOCOL_T0, 8 };
SCARD_IO_REQUEST g_rgSCardT1Pci = { SCARD_PROTOCOL_T1, 8 };
SCARD_IO_REQUEST g_rgSCardRawPci = { SCARD_PROTOCOL_RAW, 8 };

#define PCSCLITE_LOCK_POLL_RATE		100000		/**< Lock polling rate */

static LONG NotifyOfCardReset(DWORD state, PREADER_CONTEXT rContext, SCARDHANDLE hCard);
static LONG EjectCard(PREADER_CONTEXT rContext);


/**
 * @brief Creates an Application Context for a client.
 *
 * This must be the first function called in a PC/SC application.
 *
 * @param[in] dwScope Scope of the establishment.
 * This can either be a local or remote connection.
 * <ul>
 *   <li>SCARD_SCOPE_USER - Not used.
 *   <li>SCARD_SCOPE_TERMINAL - Not used.
 *   <li>SCARD_SCOPE_GLOBAL - Not used.
 *   <li>SCARD_SCOPE_SYSTEM - Services on the local machine.
 * </ul>
 * @param[in] pvReserved1 Reserved for future use. Can be used for remote connection.
 * @param[in] pvReserved2 Reserved for future use.
 * @param[out] phContext Returned Application Context.
 *
 * @return Connection status.
 * @retval SCARD_S_SUCCESS Successful (\ref SCARD_S_SUCCESS)
 * @retval SCARD_E_INVALID_VALUE Invalid scope type passed (\ref SCARD_E_INVALID_VALUE)
 * @retval SCARD_E_INVALID_PARAMETER phContext is null (\ref SCARD_E_INVALID_PARAMETER)
 */
LONG SCardEstablishContext(DWORD dwScope, LPCVOID pvReserved1,
	LPCVOID pvReserved2, LPSCARDCONTEXT phContext)
{
	/*
	 * Check for NULL pointer
	 */
	if (phContext == 0)
		return SCARD_E_INVALID_PARAMETER;

	if (dwScope != SCARD_SCOPE_USER && dwScope != SCARD_SCOPE_TERMINAL &&
		dwScope != SCARD_SCOPE_SYSTEM && dwScope != SCARD_SCOPE_GLOBAL)
	{

		*phContext = 0;
		return SCARD_E_INVALID_VALUE;
	}

	/*
	 * Unique identifier for this server so that it can uniquely be
	 * identified by clients and distinguished from others
	 */

	*phContext = (PCSCLITE_SVC_IDENTITY + SYS_Random(SYS_GetSeed(),
			1.0, 65535.0));

	Log3(PCSC_LOG_DEBUG, "Establishing Context: %d [0x%08X]", *phContext, *phContext);

	return SCARD_S_SUCCESS;
}

LONG SCardReleaseContext(SCARDCONTEXT hContext)
{
	/*
	 * Nothing to do here RPC layer will handle this
	 */

	Log2(PCSC_LOG_DEBUG, "Releasing Context: %d", hContext);

	return SCARD_S_SUCCESS;
}

LONG SCardSetTimeout(SCARDCONTEXT hContext, DWORD dwTimeout)
{
	/*
	 * This is only used at the client side of an RPC call but just in
	 * case someone calls it here
	 */

	return SCARD_E_UNSUPPORTED_FEATURE;
}

LONG SCardConnect(SCARDCONTEXT hContext, LPCSTR szReader,
	DWORD dwShareMode, DWORD dwPreferredProtocols, LPSCARDHANDLE phCard,
	LPDWORD pdwActiveProtocol)
{
	LONG rv;
	PREADER_CONTEXT rContext = NULL;
	DWORD dwStatus;

	/*
	 * Check for NULL parameters
	 */
	if (szReader == NULL || phCard == NULL || pdwActiveProtocol == NULL)
		return SCARD_E_INVALID_PARAMETER;
	else
		*phCard = 0;

	if (!(dwPreferredProtocols & SCARD_PROTOCOL_T0) &&
			!(dwPreferredProtocols & SCARD_PROTOCOL_T1) &&
			!(dwPreferredProtocols & SCARD_PROTOCOL_RAW) &&
			!(dwPreferredProtocols & SCARD_PROTOCOL_ANY_OLD))
		return SCARD_E_PROTO_MISMATCH;

	if (dwShareMode != SCARD_SHARE_EXCLUSIVE &&
			dwShareMode != SCARD_SHARE_SHARED &&
			dwShareMode != SCARD_SHARE_DIRECT)
		return SCARD_E_INVALID_VALUE;

	Log3(PCSC_LOG_DEBUG, "Attempting Connect to %s using protocol: %d",
		szReader, dwPreferredProtocols);

	rv = RFReaderInfo((LPSTR) szReader, &rContext);

	if (rv != SCARD_S_SUCCESS)
	{
		Log2(PCSC_LOG_ERROR, "Reader %s Not Found", szReader);
		return rv;
	}

	/*
	 * Make sure the reader is working properly
	 */
	rv = RFCheckReaderStatus(rContext);
	if (rv != SCARD_S_SUCCESS)
		return rv;
	
	/*******************************************
	 *
	 * This section checks for simple errors
	 *
	 *******************************************/

	/*
	 * Connect if not exclusive mode
	 */
	if (rContext->dwContexts == SCARD_EXCLUSIVE_CONTEXT)
	{
		Log1(PCSC_LOG_ERROR, "Error Reader Exclusive");
		return SCARD_E_SHARING_VIOLATION;
	}

	/*
	 * wait until a possible transaction is finished
	 */
	if (rContext->dwLockId != 0)
	{
		Log1(PCSC_LOG_INFO, "Waiting for release of lock");
		while (rContext->dwLockId != 0)
			SYS_USleep(PCSCLITE_LOCK_POLL_RATE);
		Log1(PCSC_LOG_INFO, "Lock released");

		/* Allow the status thread to convey information */
		SYS_USleep(PCSCLITE_STATUS_POLL_RATE + 10);
	}

	/*******************************************
	 *
	 * This section tries to determine the
	 * presence of a card or not
	 *
	 *******************************************/
	dwStatus = SharedReaderState_State(rContext->readerState);

	if (dwShareMode != SCARD_SHARE_DIRECT)
	{
		if (!(dwStatus & SCARD_PRESENT))
		{
			Log1(PCSC_LOG_ERROR, "Card Not Inserted");
			return SCARD_E_NO_SMARTCARD;
		}

		if (dwStatus & SCARD_SWALLOWED)
		{
			Log1(PCSC_LOG_ERROR, "Card Not Powered");
			return SCARD_W_UNPOWERED_CARD;
		}
	}


	/*******************************************
	 *
	 * This section tries to decode the ATR
	 * and set up which protocol to use
	 *
	 *******************************************/
	if (dwPreferredProtocols & SCARD_PROTOCOL_RAW)
		SharedReaderState_SetProtocol(rContext->readerState, SCARD_PROTOCOL_RAW);
	else
	{
		if (dwShareMode != SCARD_SHARE_DIRECT)
		{
			/* the protocol is not yet set (no PPS yet) */
			if (SCARD_PROTOCOL_UNSET == SharedReaderState_Protocol(rContext->readerState))
			{
				UCHAR ucAvailable, ucDefault;
				int ret;

				ucDefault = PHGetDefaultProtocol(SharedReaderState_CardAtr(rContext->readerState), 
					SharedReaderState_CardAtrLength(rContext->readerState));
				ucAvailable =
					PHGetAvailableProtocols(SharedReaderState_CardAtr(rContext->readerState), 
					SharedReaderState_CardAtrLength(rContext->readerState));

				/*
				 * If it is set to ANY let it do any of the protocols
				 */
				if (dwPreferredProtocols & SCARD_PROTOCOL_ANY_OLD)
					dwPreferredProtocols = SCARD_PROTOCOL_T0 | SCARD_PROTOCOL_T1;

				ret = PHSetProtocol(rContext, dwPreferredProtocols,
					ucAvailable, ucDefault);

				/* keep cardProtocol = SCARD_PROTOCOL_UNSET in case of error  */
				if (SET_PROTOCOL_PPS_FAILED == ret)
					return SCARD_W_UNRESPONSIVE_CARD;

				if (SET_PROTOCOL_WRONG_ARGUMENT == ret)
					return SCARD_E_PROTO_MISMATCH;

				/* use negociated protocol */
				SharedReaderState_SetProtocol(rContext->readerState, ret);
			}
			else
			{
				if (! (dwPreferredProtocols & SharedReaderState_Protocol(rContext->readerState)))
					return SCARD_E_PROTO_MISMATCH;
			}
		}
	}

	*pdwActiveProtocol = SharedReaderState_Protocol(rContext->readerState);

	if (dwShareMode != SCARD_SHARE_DIRECT)
	{
		if ((*pdwActiveProtocol != SCARD_PROTOCOL_T0)
			&& (*pdwActiveProtocol != SCARD_PROTOCOL_T1))
			Log2(PCSC_LOG_ERROR, "Active Protocol: unknown %d",
				*pdwActiveProtocol);
		else
			Log2(PCSC_LOG_DEBUG, "Active Protocol: T=%d",
				(*pdwActiveProtocol == SCARD_PROTOCOL_T0) ? 0 : 1);
	}
	else
		Log1(PCSC_LOG_DEBUG, "Direct access: no protocol selected");

	/*
	 * Prepare the SCARDHANDLE identity
	 */
	*phCard = RFCreateReaderHandle(rContext);

	Log2(PCSC_LOG_DEBUG, "hCard Identity: %x", *phCard);

	/*******************************************
	 *
	 * This section tries to set up the
	 * exclusivity modes. -1 is exclusive
	 *
	 *******************************************/

	if (dwShareMode == SCARD_SHARE_EXCLUSIVE)
	{
		if (rContext->dwContexts == SCARD_NO_CONTEXT)
		{
			rContext->dwContexts = SCARD_EXCLUSIVE_CONTEXT;
			RFLockSharing(*phCard);
		}
		else
		{
			RFDestroyReaderHandle(*phCard);
			*phCard = 0;
			Log1(PCSC_LOG_ERROR, "SCardConnect: share mode is exclusive, but already in use");
			return SCARD_E_SHARING_VIOLATION;
		}
	}
	else
	{
		/*
		 * Add a connection to the context stack
		 */
		rContext->dwContexts += 1;
	}

	/*
	 * Add this handle to the handle list
	 */
	rv = RFAddReaderHandle(rContext, *phCard);

	if (rv != SCARD_S_SUCCESS)
	{
		/*
		 * Clean up - there is no more room
		 */
		RFDestroyReaderHandle(*phCard);
		if (rContext->dwContexts == SCARD_EXCLUSIVE_CONTEXT)
			rContext->dwContexts = SCARD_NO_CONTEXT;
		else
			if (rContext->dwContexts > SCARD_NO_CONTEXT)
				rContext->dwContexts -= 1;

		*phCard = 0;
		return SCARD_F_INTERNAL_ERROR;
	}

	/*
	 * Allow the status thread to convey information
	 */
	SYS_USleep(PCSCLITE_STATUS_POLL_RATE + 10);

	return SCARD_S_SUCCESS;
}

LONG SCardReconnect(SCARDHANDLE hCard, DWORD dwShareMode,
	DWORD dwPreferredProtocols, DWORD dwInitialization,
	LPDWORD pdwActiveProtocol)
{
	LONG rv;
	PREADER_CONTEXT rContext = NULL;
	int do_sleep = 1;

	Log1(PCSC_LOG_DEBUG, "Attempting reconnect to token.");

	if (hCard == 0)
		return SCARD_E_INVALID_HANDLE;

	/*
	 * Handle the dwInitialization
	 */
	if (dwInitialization != SCARD_LEAVE_CARD &&
			dwInitialization != SCARD_RESET_CARD &&
			dwInitialization != SCARD_UNPOWER_CARD)
		return SCARD_E_INVALID_VALUE;

	if (dwShareMode != SCARD_SHARE_SHARED &&
			dwShareMode != SCARD_SHARE_EXCLUSIVE &&
			dwShareMode != SCARD_SHARE_DIRECT)
		return SCARD_E_INVALID_VALUE;

	if (!(dwPreferredProtocols & SCARD_PROTOCOL_T0) &&
			!(dwPreferredProtocols & SCARD_PROTOCOL_T1) &&
			!(dwPreferredProtocols & SCARD_PROTOCOL_RAW) &&
			!(dwPreferredProtocols & SCARD_PROTOCOL_ANY_OLD))
		return SCARD_E_PROTO_MISMATCH;

	if (pdwActiveProtocol == NULL)
		return SCARD_E_INVALID_PARAMETER;

	rv = RFReaderInfoById(hCard, &rContext);
	if (rv != SCARD_S_SUCCESS)
		return rv;

	/*
	 * Make sure the reader is working properly
	 */
	rv = RFCheckReaderStatus(rContext);
	if (rv != SCARD_S_SUCCESS)
		return rv;

	rv = RFFindReaderHandle(hCard);
	if (rv != SCARD_S_SUCCESS)
		return rv;

	/*
	 * Make sure no one has a lock on this reader
	 */
	rv = RFCheckSharing(hCard);
	if (rv != SCARD_S_SUCCESS)
		return rv;

	/*
	 * RFUnblockReader( rContext ); FIX - this doesn't work
	 */

	if (dwInitialization == SCARD_RESET_CARD ||
		dwInitialization == SCARD_UNPOWER_CARD)
	{
		LONG ret = NotifyOfCardReset(dwInitialization, rContext, hCard);
		if (ret != SCARD_S_SUCCESS)
			return ret;

		do_sleep = 1;
	}
	else if (dwInitialization == SCARD_LEAVE_CARD)
		{
			/*
			 * Do nothing
			 */
			do_sleep = 0;
		}

	/*******************************************
	 *
	 * This section tries to decode the ATR
	 * and set up which protocol to use
	 *
	 *******************************************/


	if (dwPreferredProtocols & SCARD_PROTOCOL_RAW)
		SharedReaderState_SetProtocol(rContext->readerState, SCARD_PROTOCOL_RAW);
	else
	{
		if (dwShareMode != SCARD_SHARE_DIRECT)
		{
			/* the protocol is not yet set (no PPS yet) */
			if (SCARD_PROTOCOL_UNSET == SharedReaderState_Protocol(rContext->readerState))
			{
				UCHAR ucAvailable, ucDefault;
				int ret;

				ucDefault = PHGetDefaultProtocol(SharedReaderState_CardAtr(rContext->readerState), 
					SharedReaderState_CardAtrLength(rContext->readerState));
				ucAvailable =
					PHGetAvailableProtocols(SharedReaderState_CardAtr(rContext->readerState), 
					SharedReaderState_CardAtrLength(rContext->readerState));

				/* If it is set to ANY let it do any of the protocols */
				if (dwPreferredProtocols & SCARD_PROTOCOL_ANY_OLD)
					dwPreferredProtocols = SCARD_PROTOCOL_T0 | SCARD_PROTOCOL_T1;

				ret = PHSetProtocol(rContext, dwPreferredProtocols,
					ucAvailable, ucDefault);

				/* keep cardProtocol = SCARD_PROTOCOL_UNSET in case of error  */
				if (SET_PROTOCOL_PPS_FAILED == ret)
					return SCARD_W_UNRESPONSIVE_CARD;

				if (SET_PROTOCOL_WRONG_ARGUMENT == ret)
					return SCARD_E_PROTO_MISMATCH;

				/* use negociated protocol */
				SharedReaderState_SetProtocol(rContext->readerState, ret);
			}
			else
			{
				if (! (dwPreferredProtocols & SharedReaderState_Protocol(rContext->readerState)))
					return SCARD_E_PROTO_MISMATCH;
			}
		}
	}

	*pdwActiveProtocol = SharedReaderState_Protocol(rContext->readerState);

	if (dwShareMode == SCARD_SHARE_EXCLUSIVE)
	{
		if (rContext->dwContexts == SCARD_EXCLUSIVE_CONTEXT)
		{
			/*
			 * Do nothing - we are already exclusive
			 */
		} else
		{
			if (rContext->dwContexts == SCARD_LAST_CONTEXT)
			{
				rContext->dwContexts = SCARD_EXCLUSIVE_CONTEXT;
				RFLockSharing(hCard);
			} else
			{
				Log1(PCSC_LOG_ERROR, "SCardReConnect: share mode is exclusive, but already in use");
				return SCARD_E_SHARING_VIOLATION;
			}
		}
	} else if (dwShareMode == SCARD_SHARE_SHARED)
	{
		if (rContext->dwContexts != SCARD_EXCLUSIVE_CONTEXT)
		{
			/*
			 * Do nothing - in sharing mode already
			 */
		} else
		{
			/*
			 * We are in exclusive mode but want to share now
			 */
			RFUnlockSharing(hCard);
			rContext->dwContexts = SCARD_LAST_CONTEXT;
		}
	} else if (dwShareMode == SCARD_SHARE_DIRECT)
	{
		if (rContext->dwContexts != SCARD_EXCLUSIVE_CONTEXT)
		{
			/*
			 * Do nothing - in sharing mode already
			 */
		} else
		{
			/*
			 * We are in exclusive mode but want to share now
			 */
			RFUnlockSharing(hCard);
			rContext->dwContexts = SCARD_LAST_CONTEXT;
		}
	} else
		return SCARD_E_INVALID_VALUE;

	/*
	 * Clear a previous event to the application
	 */
	RFClearReaderEventState(rContext, hCard);

	/*
	 * Allow the status thread to convey information
	 */
	if (do_sleep)
		SYS_USleep(PCSCLITE_STATUS_POLL_RATE + 10);

	return SCARD_S_SUCCESS;
}

LONG SCardDisconnect(SCARDHANDLE hCard, DWORD dwDisposition)
{
	LONG rv;
	PREADER_CONTEXT rContext = NULL;

	if (hCard == 0)
		return SCARD_E_INVALID_HANDLE;

	rv = RFReaderInfoById(hCard, &rContext);
	if (rv != SCARD_S_SUCCESS)
		return rv;

	rv = RFFindReaderHandle(hCard);
	if (rv != SCARD_S_SUCCESS)
		return rv;

	if ((dwDisposition != SCARD_LEAVE_CARD)
		&& (dwDisposition != SCARD_UNPOWER_CARD)
		&& (dwDisposition != SCARD_RESET_CARD)
		&& (dwDisposition != SCARD_EJECT_CARD))
		return SCARD_E_INVALID_VALUE;

	/*
	 * wait until a possible transaction is finished
	 */
	if ((rContext->dwLockId != 0) && (rContext->dwLockId != (uint32_t)hCard))
	{
		Log1(PCSC_LOG_INFO, "Waiting for release of lock");
		while (rContext->dwLockId != 0)
			SYS_USleep(PCSCLITE_LOCK_POLL_RATE);
		Log1(PCSC_LOG_INFO, "Lock released");
	}

	/*
	 * Unlock any blocks on this context
	 */
	rv = RFUnlockSharing(hCard);
	if (rv != SCARD_S_SUCCESS)
		return rv;

	Log2(PCSC_LOG_DEBUG, "Active Contexts: %d", rContext->dwContexts);

	if (dwDisposition == SCARD_RESET_CARD ||
		dwDisposition == SCARD_UNPOWER_CARD)
	{
		/* LONG ret = */ NotifyOfCardReset(dwDisposition, rContext, hCard);
		/* we ignore the return values in this case */
		
		/*
		 * Allow the status thread to convey information
		 */
		SYS_USleep(PCSCLITE_STATUS_POLL_RATE + 10);

	}
	else
	if (dwDisposition == SCARD_EJECT_CARD)
		EjectCard(rContext);
	else if (dwDisposition == SCARD_LEAVE_CARD)
	{
		/*
		 * Do nothing
		 */
	}

	/*
	 * Remove and destroy this handle
	 */
	RFRemoveReaderHandle(rContext, hCard);
	RFDestroyReaderHandle(hCard);

	/*
	 * For exclusive connection reset it to no connections
	 */
	if (rContext->dwContexts == SCARD_EXCLUSIVE_CONTEXT)
	{
		rContext->dwContexts = SCARD_NO_CONTEXT;
		return SCARD_S_SUCCESS;
	}

	/*
	 * Remove a connection from the context stack
	 */
	rContext->dwContexts -= 1;

	if (rContext->dwContexts < 0)
		rContext->dwContexts = 0;

	return SCARD_S_SUCCESS;
}

LONG SCardBeginTransaction(SCARDHANDLE hCard)
{
	LONG rv;
	PREADER_CONTEXT rContext;

	if (hCard == 0)
		return SCARD_E_INVALID_HANDLE;

	rv = RFReaderInfoById(hCard, &rContext);

	/*
	 * Cannot find the hCard in this context
	 */
	if (rv != SCARD_S_SUCCESS)
	{
		Log3(PCSC_LOG_DEBUG, "SCardBeginTransaction: cannot find hCard: 0x%08X [0x%08X]", hCard, rv);
		return rv;
	}
	
	/*
	 * Make sure the reader is working properly
	 */
	rv = RFCheckReaderStatus(rContext);
	if (rv != SCARD_S_SUCCESS)
	{
		Log3(PCSC_LOG_DEBUG, "SCardBeginTransaction: reader status fail: 0x%08X [0x%08X]", hCard, rv);
		return rv;
	}

	rv = RFFindReaderHandle(hCard);
	if (rv != SCARD_S_SUCCESS)
	{
		Log3(PCSC_LOG_DEBUG, "SCardBeginTransaction: reader handle fail: 0x%08X [0x%08X]", hCard, rv);
		return rv;
	}

	/*
	 * Make sure some event has not occurred
	 */
	if ((rv = RFCheckReaderEventState(rContext, hCard)) != SCARD_S_SUCCESS)
	{
		Log3(PCSC_LOG_DEBUG, "SCardBeginTransaction: reader event fail: 0x%08X [0x%08X]", hCard, rv);
		return rv;
	}

	rv = RFLockSharing(hCard);

	/* if the transaction is not yet ready we sleep a bit so the client
	 * do not retry immediately */
	if (SCARD_E_SHARING_VIOLATION == (uint32_t)rv)
		SYS_USleep(PCSCLITE_LOCK_POLL_RATE);

	Log2(PCSC_LOG_DEBUG, "SCardBeginTransaction ending status: 0x%08X", rv);

	return rv;
}

LONG SCardEndTransaction(SCARDHANDLE hCard, DWORD dwDisposition)
{
	LONG rv;
	PREADER_CONTEXT rContext = NULL;

	/*
	 * Ignoring dwDisposition for now
	 */
	if (hCard == 0)
		return SCARD_E_INVALID_HANDLE;

	if ((dwDisposition != SCARD_LEAVE_CARD)
		&& (dwDisposition != SCARD_UNPOWER_CARD)
		&& (dwDisposition != SCARD_RESET_CARD)
		&& (dwDisposition != SCARD_EJECT_CARD))
	return SCARD_E_INVALID_VALUE;

	rv = RFReaderInfoById(hCard, &rContext);

	/*
	 * Cannot find the hCard in this context
	 */
	if (rv != SCARD_S_SUCCESS)
		return rv;

	rv = RFFindReaderHandle(hCard);
	if (rv != SCARD_S_SUCCESS)
		return rv;

	/*
	 * Make sure some event has not occurred
	 */
	if ((rv = RFCheckReaderEventState(rContext, hCard)) != SCARD_S_SUCCESS)
		return rv;

	if (dwDisposition == SCARD_RESET_CARD ||
		dwDisposition == SCARD_UNPOWER_CARD)
	{
		/* LONG ret = */ NotifyOfCardReset(dwDisposition, rContext, hCard);
	}
	else if (dwDisposition == SCARD_EJECT_CARD)
		EjectCard(rContext);
	else if (dwDisposition == SCARD_LEAVE_CARD)
	{
		/*
		 * Do nothing
		 */
	}

	/*
	 * Unlock any blocks on this context
	 */
	RFUnlockSharing(hCard);

	Log2(PCSC_LOG_DEBUG, "Status: 0x%08X", rv);

	return rv;
}

LONG SCardCancelTransaction(SCARDHANDLE hCard)
{
	LONG rv;
	PREADER_CONTEXT rContext = NULL;

	/*
	 * Ignoring dwDisposition for now
	 */
	if (hCard == 0)
		return SCARD_E_INVALID_HANDLE;

	rv = RFReaderInfoById(hCard, &rContext);

	/*
	 * Cannot find the hCard in this context
	 */
	if (rv != SCARD_S_SUCCESS)
		return rv;

	rv = RFFindReaderHandle(hCard);
	if (rv != SCARD_S_SUCCESS)
		return rv;

	/*
	 * Make sure some event has not occurred
	 */
	if ((rv = RFCheckReaderEventState(rContext, hCard)) != SCARD_S_SUCCESS)
		return rv;

	rv = RFUnlockSharing(hCard);

	Log2(PCSC_LOG_DEBUG, "Status: 0x%08X", rv);

	return rv;
}

LONG SCardStatus(SCARDHANDLE hCard, LPSTR mszReaderNames,
	LPDWORD pcchReaderLen, LPDWORD pdwState,
	LPDWORD pdwProtocol, LPBYTE pbAtr, LPDWORD pcbAtrLen)
{
	LONG rv;
	PREADER_CONTEXT rContext = NULL;

	rv = RFReaderInfoById(hCard, &rContext);

	/*
	 * Cannot find the hCard in this context
	 */
	if (rv != SCARD_S_SUCCESS)
		return rv;

	if (strlen(rContext->lpcReader) > MAX_BUFFER_SIZE
			|| SharedReaderState_CardAtrLength(rContext->readerState) > MAX_ATR_SIZE)
		return SCARD_F_INTERNAL_ERROR;

	/*
	 * This is a client side function however the server maintains the
	 * list of events between applications so it must be passed through to
	 * obtain this event if it has occurred
	 */

	/*
	 * Make sure some event has not occurred
	 */
	if ((rv = RFCheckReaderEventState(rContext, hCard)) != SCARD_S_SUCCESS)
		return rv;

	/*
	 * Make sure the reader is working properly
	 */
	rv = RFCheckReaderStatus(rContext);
	if (rv != SCARD_S_SUCCESS)
		return rv;

	if (mszReaderNames)			/* want reader name */
	{
		int cchReaderLen;
		if (!pcchReaderLen)		/* present buf & no buflen */
			return SCARD_E_INVALID_PARAMETER;

		cchReaderLen = strlen(rContext->lpcReader);
		if(*pcchReaderLen < cchReaderLen)
			rv = SCARD_E_INSUFFICIENT_BUFFER;
		else   /* There's enough room in the buffer */
			strncpy(mszReaderNames, rContext->lpcReader, cchReaderLen);
		*pcchReaderLen = cchReaderLen;
	}
	else if (pcchReaderLen) /* want the reader length but not the name */
		*pcchReaderLen = strlen(rContext->lpcReader);

	if (pdwState)
		*pdwState = SharedReaderState_State(rContext->readerState);

	if (pdwProtocol)
		*pdwProtocol = SharedReaderState_Protocol(rContext->readerState);

	if (pbAtr)     /* want ATR */
	{
		int cbAtrLen;
		if (!pcbAtrLen)
			return SCARD_E_INVALID_PARAMETER;
		cbAtrLen = SharedReaderState_CardAtrLength(rContext->readerState);

		if(cbAtrLen >= *pcbAtrLen)
			rv = SCARD_E_INSUFFICIENT_BUFFER;
		else
		{
			*pcbAtrLen = cbAtrLen;
			memcpy(pbAtr, SharedReaderState_CardAtr(rContext->readerState), cbAtrLen);
		}
	}
	else if (pcbAtrLen)
		*pcbAtrLen = SharedReaderState_CardAtrLength(rContext->readerState);

	return rv;
}

LONG SCardGetStatusChange(SCARDCONTEXT hContext, DWORD dwTimeout,
	LPSCARD_READERSTATE_A rgReaderStates, DWORD cReaders)
{
	/*
	 * Client side function
	 */
	return SCARD_S_SUCCESS;
}

#undef SCardControl

LONG SCardControl(SCARDHANDLE hCard, const void *pbSendBuffer,
	DWORD cbSendLength, void *pbRecvBuffer, LPDWORD pcbRecvLength)
{
	// Pre pcsclite 1.3.2 version
	
	uint32_t dwControlCode = 0;
	
	uint32_t cbRecvLength = *pcbRecvLength;
	uint32_t bytesReturned = 0;
	int32_t rv = SCardControl132(hCard, dwControlCode, pbSendBuffer, cbSendLength,
		pbRecvBuffer, cbRecvLength, &bytesReturned);
	*pcbRecvLength = bytesReturned;
	return rv;
}

int32_t SCardControl132(SCARDHANDLE hCard, uint32_t dwControlCode,
		const void *pbSendBuffer, uint32_t cbSendLength,
		void *pbRecvBuffer, uint32_t cbRecvLength, uint32_t *lpBytesReturned)
{
	LONG rv;
	PREADER_CONTEXT rContext = NULL;

	/* 0 bytes returned by default */
	*lpBytesReturned = 0;

	if (0 == hCard)
		return SCARD_E_INVALID_HANDLE;

	/*
	 * Make sure no one has a lock on this reader
	 */
	if ((rv = RFCheckSharing(hCard)) != SCARD_S_SUCCESS)
		return rv;

	rv = RFReaderInfoById(hCard, &rContext);
	if (rv != SCARD_S_SUCCESS)
		return rv;

	if (IFD_HVERSION_2_0 == rContext->dwVersion)
		if (NULL == pbSendBuffer || 0 == cbSendLength)
			return SCARD_E_INVALID_PARAMETER;

	/*
	 * Make sure the reader is working properly
	 */
	rv = RFCheckReaderStatus(rContext);
	if (rv != SCARD_S_SUCCESS)
		return rv;

	rv = RFFindReaderHandle(hCard);
	if (rv != SCARD_S_SUCCESS)
		return rv;

	/*
	 * Make sure some event has not occurred
	 */
	if ((rv = RFCheckReaderEventState(rContext, hCard)) != SCARD_S_SUCCESS)
		return rv;

	if (IFD_HVERSION_2_0 == rContext->dwVersion)
	{
		/* we must wrap a API 3.0 client in an API 2.0 driver */
		*lpBytesReturned = cbRecvLength;
		return IFDControl_v2(rContext, (PUCHAR)pbSendBuffer,
			cbSendLength, (uint8_t *)pbRecvBuffer, lpBytesReturned);
	}
	else
		if (IFD_HVERSION_3_0 == rContext->dwVersion)
			return IFDControl(rContext, dwControlCode, pbSendBuffer,
				cbSendLength, pbRecvBuffer, cbRecvLength, lpBytesReturned);
		else
			return SCARD_E_UNSUPPORTED_FEATURE;
}

LONG SCardGetAttrib(SCARDHANDLE hCard, DWORD dwAttrId,
	LPBYTE pbAttr, LPDWORD pcbAttrLen)
{
	LONG rv;
	PREADER_CONTEXT rContext = NULL;

	if (0 == hCard)
		return SCARD_E_INVALID_HANDLE;

	/*
	 * Make sure no one has a lock on this reader
	 */
	if ((rv = RFCheckSharing(hCard)) != SCARD_S_SUCCESS)
		return rv;

	rv = RFReaderInfoById(hCard, &rContext);
	if (rv != SCARD_S_SUCCESS)
		return rv;

	/*
	 * Make sure the reader is working properly
	 */
	rv = RFCheckReaderStatus(rContext);
	if (rv != SCARD_S_SUCCESS)
		return rv;

	rv = RFFindReaderHandle(hCard);
	if (rv != SCARD_S_SUCCESS)
		return rv;

	/*
	 * Make sure some event has not occurred
	 */
	if ((rv = RFCheckReaderEventState(rContext, hCard)) != SCARD_S_SUCCESS)
		return rv;

	rv = IFDGetCapabilities(rContext, dwAttrId, pcbAttrLen, pbAttr);
	if (rv == IFD_SUCCESS)
		return SCARD_S_SUCCESS;
	else
		if (rv == IFD_ERROR_TAG)
			return SCARD_E_UNSUPPORTED_FEATURE;
		else
			return SCARD_E_NOT_TRANSACTED;
}

LONG SCardSetAttrib(SCARDHANDLE hCard, DWORD dwAttrId,
	LPCBYTE pbAttr, DWORD cbAttrLen)
{
	LONG rv;
	PREADER_CONTEXT rContext = NULL;

	if (0 == hCard)
		return SCARD_E_INVALID_HANDLE;

	/*
	 * Make sure no one has a lock on this reader
	 */
	if ((rv = RFCheckSharing(hCard)) != SCARD_S_SUCCESS)
		return rv;

	rv = RFReaderInfoById(hCard, &rContext);
	if (rv != SCARD_S_SUCCESS)
		return rv;

	/*
	 * Make sure the reader is working properly
	 */
	rv = RFCheckReaderStatus(rContext);
	if (rv != SCARD_S_SUCCESS)
		return rv;

	rv = RFFindReaderHandle(hCard);
	if (rv != SCARD_S_SUCCESS)
		return rv;

	/*
	 * Make sure some event has not occurred
	 */
	if ((rv = RFCheckReaderEventState(rContext, hCard)) != SCARD_S_SUCCESS)
		return rv;

	rv = IFDSetCapabilities(rContext, dwAttrId, cbAttrLen, (PUCHAR)pbAttr);
	if (rv == IFD_SUCCESS)
		return SCARD_S_SUCCESS;
	else
		if (rv == IFD_ERROR_TAG)
			return SCARD_E_UNSUPPORTED_FEATURE;
		else
			return SCARD_E_NOT_TRANSACTED;
}

#define kSCARD_LE_IN_SW2	0x6C
#define kReadBinaryAPDU		0xB0
#define kReadBinaryLe		4

LONG SCardTransmit(SCARDHANDLE hCard, LPCSCARD_IO_REQUEST pioSendPci,
	LPCBYTE pbSendBuffer, DWORD cbSendLength,
	LPSCARD_IO_REQUEST pioRecvPci, LPBYTE pbRecvBuffer,
	LPDWORD pcbRecvLength)
{
	/*
		See for example:
		NIST IR 6887	"Government Smart Card Interoperability Specification (GSC-IS), v2.1",
		July 2003
		http://csrc.nist.gov/publications/nistir/nistir-6887.pdf
		for info on error conditions. One define is SCARD_LE_IN_SW2
	*/
	LONG rv;
	PREADER_CONTEXT rContext = NULL;
	SCARD_IO_HEADER sSendPci, sRecvPci;
	DWORD dwRxLength, tempRxLength;

	if (pcbRecvLength == 0)
		return SCARD_E_INVALID_PARAMETER;

	dwRxLength = *pcbRecvLength;
	*pcbRecvLength = 0;

	if (hCard == 0)
		return SCARD_E_INVALID_HANDLE;

	if (pbSendBuffer == NULL || pbRecvBuffer == NULL || pioSendPci == NULL)
		return SCARD_E_INVALID_PARAMETER;

	/*
	 * Must at least send a 4 bytes APDU
	 */
	if (cbSendLength < 4)
		return SCARD_E_INVALID_PARAMETER;

	/*
	 * Must at least have 2 status words even for SCardControl
	 */
	if (dwRxLength < 2)
		return SCARD_E_INSUFFICIENT_BUFFER;

	/*
	 * Make sure no one has a lock on this reader
	 */
	if ((rv = RFCheckSharing(hCard)) != SCARD_S_SUCCESS)
		return rv;

	rv = RFReaderInfoById(hCard, &rContext);
	if (rv != SCARD_S_SUCCESS)
		return rv;

	/*
	 * Make sure the reader is working properly
	 */
	rv = RFCheckReaderStatus(rContext);
	if (rv != SCARD_S_SUCCESS)
		return rv;

	rv = RFFindReaderHandle(hCard);
	if (rv != SCARD_S_SUCCESS)
		return rv;

	/*
	 * Make sure some event has not occurred
	 */
	if ((rv = RFCheckReaderEventState(rContext, hCard)) != SCARD_S_SUCCESS)
		return rv;

	/*
	 * Check for some common errors
	 */
	if (pioSendPci->dwProtocol != SCARD_PROTOCOL_RAW)
	{
		if (SharedReaderState_State(rContext->readerState) & SCARD_ABSENT)
		{
			return SCARD_E_NO_SMARTCARD;
		}
	}

	if (pioSendPci->dwProtocol != SCARD_PROTOCOL_RAW)
	{
		if (pioSendPci->dwProtocol != SCARD_PROTOCOL_ANY_OLD)
		{
			if (pioSendPci->dwProtocol != SharedReaderState_Protocol(rContext->readerState))
			{
				return SCARD_E_PROTO_MISMATCH;
			}
		}
	}

	/*
	 * Quick fix: PC/SC starts at 1 for bit masking but the IFD_Handler
	 * just wants 0 or 1
	 */

	sSendPci.Protocol = 0; /* protocol T=0 by default */

	if (pioSendPci->dwProtocol == SCARD_PROTOCOL_T1)
	{
		sSendPci.Protocol = 1;
	} else if (pioSendPci->dwProtocol == SCARD_PROTOCOL_RAW)
	{
		/*
		 * This is temporary ......
		 */
		sSendPci.Protocol = SCARD_PROTOCOL_RAW;
	} else if (pioSendPci->dwProtocol == SCARD_PROTOCOL_ANY_OLD)
	{
	  /* Fix by Amira (Athena) */
		unsigned long i;
		unsigned long prot = SharedReaderState_Protocol(rContext->readerState);

		for (i = 0 ; prot != 1 ; i++)
			prot >>= 1;

		sSendPci.Protocol = i;
	}

	sSendPci.Length = pioSendPci->cbPciLength;

	/* the protocol number is decoded a few lines above */
	Log2(PCSC_LOG_DEBUG, "Send Protocol: T=%d", sSendPci.Protocol);

	tempRxLength = dwRxLength;

	if (pioSendPci->dwProtocol == SCARD_PROTOCOL_RAW)
	{
		rv = IFDControl_v2(rContext, (PUCHAR)pbSendBuffer , cbSendLength,
			pbRecvBuffer, &dwRxLength);
	} else
	{
		rv = IFDTransmit(rContext, sSendPci, (PUCHAR)pbSendBuffer,
			cbSendLength, pbRecvBuffer, &dwRxLength, &sRecvPci);
	}

	if (pioRecvPci)
	{
		pioRecvPci->dwProtocol = sRecvPci.Protocol;
		pioRecvPci->cbPciLength = sRecvPci.Length;
	}
	
	Log3(PCSC_LOG_DEBUG, "IFDControl_v2/IFDTransmit result: 0x%08X, received: %d", rv, dwRxLength);
	Log3(PCSC_LOG_DEBUG, " pbRecvBuffer: [0]: 0x%02X, [1]: 0x%02X", pbRecvBuffer[0], pbRecvBuffer[1]);

	/*
	 * Check for any errors that might have occurred
	 */
	
	if (rv != SCARD_S_SUCCESS)
	{
		*pcbRecvLength = 0;
		Log2(PCSC_LOG_ERROR, "Card not transacted: 0x%08lX", rv);
		return SCARD_E_NOT_TRANSACTED;
	}

	/*
	 * Available is less than received
	 */
	if (tempRxLength < dwRxLength)
	{
		Log3(PCSC_LOG_DEBUG, "Available is less than received: avail: %d, received: %d", tempRxLength, dwRxLength);
		*pcbRecvLength = 0;
		return SCARD_E_INSUFFICIENT_BUFFER;
	}

	/*
	 * Successful return
	 */
	*pcbRecvLength = dwRxLength;
	return SCARD_S_SUCCESS;
}

LONG SCardListReaders(SCARDCONTEXT hContext, LPCSTR mszGroups,
	LPSTR mszReaders, LPDWORD pcchReaders)
{
	/*
	 * Client side function
	 */
	return SCARD_S_SUCCESS;
}

LONG SCardCancel(SCARDCONTEXT hContext)
{
	/*
	 * Client side function
	 */
	return SCARD_S_SUCCESS;
}

static LONG NotifyOfCardReset(DWORD state, PREADER_CONTEXT rContext, SCARDHANDLE hCard)
{
	/*
	 * Currently pcsc-lite keeps the card powered constantly
	 * Note that although EndTransaction initially sets dwAction in one
	 * case to IFD_POWER_DOWN, it then sets it to IFD_RESET
	 */

	LONG rv = SCARD_S_SUCCESS, ret = SCARD_S_SUCCESS;

	DWORD tmpCardAtrLength = SharedReaderState_CardAtrLength(rContext->readerState);
	if (SCARD_RESET_CARD == state)
		rv = IFDPowerICC(rContext, IFD_RESET, SharedReaderState_CardAtr(rContext->readerState), &tmpCardAtrLength);
	else
	{
		rv = IFDPowerICC(rContext, IFD_POWER_DOWN, SharedReaderState_CardAtr(rContext->readerState), &tmpCardAtrLength);
		rv = IFDPowerICC(rContext, IFD_POWER_UP,   SharedReaderState_CardAtr(rContext->readerState), &tmpCardAtrLength);
	}
	SharedReaderState_SetCardAtrLength(rContext->readerState, tmpCardAtrLength);

	/* the protocol is unset after a power on */
	SharedReaderState_SetProtocol(rContext->readerState, SCARD_PROTOCOL_UNSET);

	/*
	 * Notify the card has been reset
	 * Not doing this could result in deadlock
	 */
	ret = RFCheckReaderEventState(rContext, hCard);
	
	/*
		Note: there is disagreement on which value of rv to use for the switch below:
		
		SCardReconnect:			result of RFCheckReaderEventState
		SCardDisconnect:		result of IFDPowerICC
		SCardEndTransaction: 	result of IFDPowerICC
		
		We use the result of IFDPowerICC here; this seems more sensible
	*/
	switch (rv)
	{
	/* avoid deadlock */
	case SCARD_W_RESET_CARD:
		break;

	case SCARD_W_REMOVED_CARD:
		Log1(PCSC_LOG_ERROR, "card removed");
		return SCARD_W_REMOVED_CARD;

	/* invalid EventStatus */
	case SCARD_E_INVALID_VALUE:
		Log1(PCSC_LOG_ERROR, "invalid EventStatus");
		return SCARD_F_INTERNAL_ERROR;

	/* invalid hCard, but hCard was widely used some lines above :( */
	case SCARD_E_INVALID_HANDLE:
		Log1(PCSC_LOG_ERROR, "invalid handle");
		return SCARD_F_INTERNAL_ERROR;

	case SCARD_S_SUCCESS:
		/*
		 * Notify the card has been reset
		 */
		RFSetReaderEventState(rContext, SCARD_RESET);

		/*
		 * Set up the status bit masks on dwStatus
		 */
		DWORD readerStateTmp = SharedReaderState_State(rContext->readerState);
		if (rv == SCARD_S_SUCCESS)
		{
			readerStateTmp |= SCARD_PRESENT;
			readerStateTmp &= ~SCARD_ABSENT;
			readerStateTmp |= SCARD_POWERED;
			readerStateTmp |= SCARD_NEGOTIABLE;
			readerStateTmp &= ~SCARD_SPECIFIC;
			readerStateTmp &= ~SCARD_SWALLOWED;
			readerStateTmp &= ~SCARD_UNKNOWN;
		}
		else
		{
			readerStateTmp |= SCARD_PRESENT;
			readerStateTmp &= ~SCARD_ABSENT;
			readerStateTmp |= SCARD_SWALLOWED;
			readerStateTmp &= ~SCARD_POWERED;
			readerStateTmp &= ~SCARD_NEGOTIABLE;
			readerStateTmp &= ~SCARD_SPECIFIC;
			readerStateTmp &= ~SCARD_UNKNOWN;
			SharedReaderState_SetCardAtrLength(rContext->readerState, 0);
		}
		SharedReaderState_SetState(rContext->readerState, readerStateTmp);

		if (SharedReaderState_CardAtrLength(rContext->readerState) > 0)
		{
			Log1(PCSC_LOG_ERROR, "Reset complete.");
			LogXxd(PCSC_LOG_DEBUG, "Card ATR: ", SharedReaderState_CardAtr(rContext->readerState), 
				SharedReaderState_CardAtrLength(rContext->readerState));
		}
		else
		{
			DWORD dwStatus, dwAtrLen;
			UCHAR ucAtr[MAX_ATR_SIZE];

			Log1(PCSC_LOG_ERROR, "Error resetting card.");
			IFDStatusICC(rContext, &dwStatus, ucAtr, &dwAtrLen);
			if (dwStatus & SCARD_PRESENT)
				return SCARD_W_UNRESPONSIVE_CARD;
			else
				return SCARD_E_NO_SMARTCARD;
		}
		break;
	default:
		Log2(PCSC_LOG_ERROR, "invalid retcode from RFCheckReaderEventState (%X)", rv);
		return SCARD_F_INTERNAL_ERROR;
	}
	return SCARD_S_SUCCESS;
}

static LONG EjectCard(PREADER_CONTEXT rContext)
{
	LONG rv = SCARD_S_SUCCESS;

	UCHAR controlBuffer[5];
	UCHAR receiveBuffer[MAX_BUFFER_SIZE];
	DWORD receiveLength;

	/*
	 * Set up the CTBCS command for Eject ICC
	 */
	controlBuffer[0] = 0x20;
	controlBuffer[1] = 0x15;
	controlBuffer[2] = (rContext->dwSlot & 0x0000FFFF) + 1;
	controlBuffer[3] = 0x00;
	controlBuffer[4] = 0x00;
	receiveLength = 2;
	rv = IFDControl_v2(rContext, controlBuffer, 5, receiveBuffer, &receiveLength);

	if (rv == SCARD_S_SUCCESS)
	{
		if (receiveLength == 2 && receiveBuffer[0] == 0x90)	// Successful
			Log1(PCSC_LOG_ERROR, "Card ejected successfully.");
		else
		{
			Log3(PCSC_LOG_ERROR, "Error ejecting card: %02X%02X", receiveBuffer[0], receiveBuffer[1]);
			rv = SCARD_F_UNKNOWN_ERROR;
		}
	}
	else
		Log1(PCSC_LOG_ERROR, "Error ejecting card.");
		
	return rv;
}



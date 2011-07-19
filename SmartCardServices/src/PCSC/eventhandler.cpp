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
 *  eventhandler.cpp
 *  SmartCardServices
 */

/*
 * MUSCLE SmartCard Development ( http://www.linuxnet.com )
 *
 * Copyright (C) 2000
 *  David Corcoran <corcoran@linuxnet.com>
 * Copyright (C) 2004
 *  Ludovic Rousseau <ludovic.rousseau@free.fr>
 *
 * $Id: eventhandler.cpp 123 2010-03-27 10:50:42Z ludovic.rousseau@gmail.com $
 */

/**
 * @file
 * @brief This keeps track of card insertion/removal events
 * and updates ATR, protocol, and status information.
 */

#include "config.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/errno.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>

#include "wintypes.h"
#include "pcsclite.h"
#include "ifdhandler.h"
#include "debuglog.h"
#include "thread_generic.h"
#include "readerfactory.h"
#include "eventhandler.h"
#include "dyn_generic.h"
#include "sys_generic.h"
#include "ifdwrapper.h"
#include "prothandler.h"
#include "readerstate.h"

#include <security_utilities/debugging.h>

static PREADER_STATE readerStates[PCSCLITE_MAX_READERS_CONTEXTS];

void EHStatusHandlerThread(PREADER_CONTEXT);

LONG EHInitializeEventStructures(void)
{
	int fd, i, pageSize;

	fd = 0;
	i = 0;
	pageSize = 0;

	/*
		Do not truncate to avoid possible SIGSEG on clients
		Do not remove the file to allow long-term clients such as securityd to 
			stay connected to the same file
	*/
	fd = SYS_OpenFile(PCSCLITE_PUBSHM_FILE, O_RDWR | O_CREAT , 00644);
	if (fd < 0)
	{
		Log3(PCSC_LOG_CRITICAL, "Cannot create public shared file %s: %s",
			PCSCLITE_PUBSHM_FILE, strerror(errno));
		exit(1);
	}

	SYS_Chmod(PCSCLITE_PUBSHM_FILE,
		S_IRGRP | S_IREAD | S_IWRITE | S_IROTH);

	pageSize = SYS_GetPageSize();

	int rx = ftruncate(fd, pageSize * PCSCLITE_MAX_READERS_CONTEXTS);
	if (rx)
		Log3(PCSC_LOG_CRITICAL, "Cannot truncate public shared file %d: %s",
				errno, strerror(errno));
	/*
	 * Jump to end of file space and allocate zero's
	 */
	SYS_SeekFile(fd, pageSize * PCSCLITE_MAX_READERS_CONTEXTS);
	SYS_WriteFile(fd, "", 1);

	/*
	 * Allocate each reader structure
	 */
	for (i = 0; i < PCSCLITE_MAX_READERS_CONTEXTS; i++)
	{
		readerStates[i] = (PREADER_STATE)
			SYS_MemoryMap(sizeof(READER_STATE), fd, (i * pageSize));
		if (readerStates[i] == MAP_FAILED)
		{
			Log3(PCSC_LOG_CRITICAL, "Cannot memory map public shared file %s: %s",
				PCSCLITE_PUBSHM_FILE, strerror(errno));
			exit(1);
		}

		/*
		 * Zero out each value in the struct
		 */
		memset((readerStates[i])->readerName, 0, MAX_READERNAME);
		memset((readerStates[i])->cardAtr, 0, MAX_ATR_SIZE);
		(readerStates[i])->readerID = 0;
		(readerStates[i])->readerState = 0;
		(readerStates[i])->lockState = 0;
		(readerStates[i])->readerSharing = 0;
		(readerStates[i])->cardAtrLength = 0;
		(readerStates[i])->cardProtocol = SCARD_PROTOCOL_UNSET;	// ok since this is 0
	}

	return SCARD_S_SUCCESS;
}

LONG EHDestroyEventHandler(PREADER_CONTEXT rContext)
{
	if (NULL == rContext->readerState)
	{
		Log1(PCSC_LOG_ERROR, "Thread never started (reader init failed?)");
		return SCARD_S_SUCCESS;
	}

	PCSCD::SharedReaderState *rs = PCSCD::SharedReaderState::overlay(rContext->readerState);
	if ((rContext->pthThread == 0) || !rs || (rs->readerNameLength() == 0))
	{
		Log1(PCSC_LOG_INFO, "Thread already stomped.");
		return SCARD_S_SUCCESS;
	}

	secdebug("pcscd", "EHDestroyEventHandler: pthThread: %p, reader name len: %ld",
		rContext->pthThread, rs->readerNameLength());

	/*
	 * Zero out the public status struct to allow it to be recycled and
	 * used again
	 */

	rs->xreaderNameClear();
	rs->xcardAtrClear();
	rs->xreaderID(0);
	rs->xreaderState(0);
	rs->xlockState(0);
	rs->sharing(0);
	rs->xcardAtrLength(0);
	rs->xcardProtocol(SCARD_PROTOCOL_UNSET);		// we only set this one to write to memory cache

	/*
	 * Set the thread to 0 to exit thread
	 */
	ReaderContextLock(rContext);

	Log1(PCSC_LOG_INFO, "Stomping thread.");

	int ix;
	for (ix = 0; (ix < 100) && ReaderContextIsLocked(rContext); ++ix)
	{
		/*
		 * Wait 0.05 seconds for the child to respond
		 */
		SYS_USleep(50000);
	}

	secdebug("pcscd", "EHDestroyEventHandler: post-stop dwLockId: %d", rContext->dwLockId);


	/* Zero the thread */
	rContext->pthThread = 0;

	Log1(PCSC_LOG_INFO, "Thread stomped.");

	return SCARD_S_SUCCESS;
}

LONG EHSpawnEventHandler(PREADER_CONTEXT rContext)
{
	LONG rv;
	DWORD dwStatus = 0;
	int i;
	UCHAR ucAtr[MAX_ATR_SIZE];
	DWORD dwAtrLen = 0;

	secdebug("pcscd", "EHSpawnEventHandler: rContext: 0x%p", rContext);
	rv = IFDStatusICC(rContext, &dwStatus, ucAtr, &dwAtrLen);
	if (rv != SCARD_S_SUCCESS)
	{
		Log2(PCSC_LOG_ERROR, "Initial Check Failed on %s", rContext->lpcReader);
		return SCARD_F_UNKNOWN_ERROR;
	}

	/*
	 * Find an empty reader slot and insert the new reader
	 */
	for (i = 0; i < PCSCLITE_MAX_READERS_CONTEXTS; i++)
	{
		PCSCD::SharedReaderState *rstmp = PCSCD::SharedReaderState::overlay(readerStates[i]);
		if (rstmp->xreaderID() == 0)
			break;
	}

	if (i == PCSCLITE_MAX_READERS_CONTEXTS)
		return SCARD_F_INTERNAL_ERROR;

	/*
	 * Set all the attributes to this reader
	 */
	PCSCD::SharedReaderState *rs = PCSCD::SharedReaderState::overlay(readerStates[i]);
	rContext->readerState = readerStates[i];
	rs->xreaderName(rContext->lpcReader);
	rs->xcardAtr(ucAtr, dwAtrLen);	// also sets cardAtrLength

	rs->xreaderID(i + 100);
	rs->xreaderState(dwStatus);
	rs->sharing(rContext->dwContexts);
	rs->xcardProtocol(SCARD_PROTOCOL_UNSET);
	
	rv = SYS_ThreadCreate(&rContext->pthThread, THREAD_ATTR_DETACHED,
		(PCSCLITE_THREAD_FUNCTION( ))EHStatusHandlerThread, (LPVOID) rContext);
	secdebug("pcscd", "EHSpawnEventHandler after thread create: %d [%04X]", rv, rv);
	if (rv == 1)
		return SCARD_S_SUCCESS;
	else
		return SCARD_E_NO_MEMORY;
}

void EHStatusHandlerThread(PREADER_CONTEXT rContext)
{
	LONG rv;
	LPCSTR lpcReader;
	DWORD dwStatus, dwReaderSharing;
	DWORD dwCurrentState;
	int pageSize = SYS_GetPageSize();

	/*
	 * Zero out everything
	 */
	dwStatus = 0;
	dwReaderSharing = 0;
	dwCurrentState = 0;

	secdebug("pcscd", "EHStatusHandlerThread: rContext: 0x%p", rContext);
	lpcReader = rContext->lpcReader;

	PCSCD::SharedReaderState *rs = PCSCD::SharedReaderState::overlay(rContext->readerState);

	DWORD tmpCardAtrLength = MAX_ATR_SIZE;
	rv = IFDStatusICC(rContext, &dwStatus, rs->xcardAtr(), &tmpCardAtrLength);
	secdebug("pcscd", "EHStatusHandlerThread: initial call to IFDStatusICC: %d [%04X]", rv, rv);

	if (dwStatus & SCARD_PRESENT)
	{
		tmpCardAtrLength = MAX_ATR_SIZE;
		rv = IFDPowerICC(rContext, IFD_POWER_UP, rs->xcardAtr(), &tmpCardAtrLength);

		/* the protocol is unset after a power on */
		rs->xcardProtocol(SCARD_PROTOCOL_UNSET);

		secdebug("pcscd", "EHStatusHandlerThread: initial call to IFDPowerICC: %d [%04X]", rv, rv);

		if (rv == IFD_SUCCESS)
		{
			rs->xcardAtrLength(tmpCardAtrLength);

			dwStatus |= SCARD_PRESENT;
			dwStatus &= ~SCARD_ABSENT;
			dwStatus |= SCARD_POWERED;
			dwStatus |= SCARD_NEGOTIABLE;
			dwStatus &= ~SCARD_SPECIFIC;
			dwStatus &= ~SCARD_SWALLOWED;
			dwStatus &= ~SCARD_UNKNOWN;

			if (rs->xcardAtrLength() > 0)
			{
				LogXxd(PCSC_LOG_INFO, "Card ATR: ",
					rs->xcardAtr(),
					rs->xcardAtrLength());
			}
			else
				Log1(PCSC_LOG_INFO, "Card ATR: (NULL)");
		}
		else
		{
			dwStatus |= SCARD_PRESENT;
			dwStatus &= ~SCARD_ABSENT;
			dwStatus |= SCARD_SWALLOWED;
			dwStatus &= ~SCARD_POWERED;
			dwStatus &= ~SCARD_NEGOTIABLE;
			dwStatus &= ~SCARD_SPECIFIC;
			dwStatus &= ~SCARD_UNKNOWN;
			Log3(PCSC_LOG_ERROR, "Error powering up card: %d 0x%04X", rv, rv);
		}

		dwCurrentState = SCARD_PRESENT;
	}
	else
	{
		dwStatus |= SCARD_ABSENT;
		dwStatus &= ~SCARD_PRESENT;
		dwStatus &= ~SCARD_POWERED;
		dwStatus &= ~SCARD_NEGOTIABLE;
		dwStatus &= ~SCARD_SPECIFIC;
		dwStatus &= ~SCARD_SWALLOWED;
		dwStatus &= ~SCARD_UNKNOWN;
		rs->xcardAtrLength(0);
		rs->xcardProtocol(SCARD_PROTOCOL_UNSET);

		dwCurrentState = SCARD_ABSENT;
	}

	/*
	 * Set all the public attributes to this reader
	 */
	rs->xreaderState(dwStatus);
	dwReaderSharing = rContext->dwContexts;
	rs->sharing(dwReaderSharing);

	SYS_MMapSynchronize((void *) rContext->readerState, pageSize);

	while (1)
	{
		dwStatus = 0;

		// Defensive measure
		if (!rContext->vHandle)
		{
			// Exit and notify the caller
			secdebug("pcscd", "EHStatusHandlerThread: lost dynamic callbacks ??");
			ReaderContextUnlock(rContext);
			SYS_ThreadDetach(rContext->pthThread);
			SYS_ThreadExit(0);
		}

		DWORD tmpCardAtrLength = MAX_ATR_SIZE;
		rv = IFDStatusICC(rContext, &dwStatus, rs->xcardAtr(), &tmpCardAtrLength);

		if (rv != SCARD_S_SUCCESS)
		{
			Log2(PCSC_LOG_ERROR, "Error communicating to: %s", lpcReader);

			/*
			 * Set error status on this reader while errors occur
			 */

			DWORD readerStateTmp = rs->xreaderState();
			readerStateTmp &= ~SCARD_ABSENT;
			readerStateTmp &= ~SCARD_PRESENT;
			readerStateTmp &= ~SCARD_POWERED;
			readerStateTmp &= ~SCARD_NEGOTIABLE;
			readerStateTmp &= ~SCARD_SPECIFIC;
			readerStateTmp &= ~SCARD_SWALLOWED;
			readerStateTmp |= SCARD_UNKNOWN;
			rs->xcardAtrLength(0);
			rs->xcardProtocol(SCARD_PROTOCOL_UNSET);
			rs->xreaderState(readerStateTmp);

			dwCurrentState = SCARD_UNKNOWN;

			SYS_MMapSynchronize((void *) rContext->readerState, pageSize);

			/*
			 * This code causes race conditions on G4's with USB
			 * insertion
			 */
			/*
			 * dwErrorCount += 1; SYS_Sleep(1);
			 */
			/*
			 * After 10 seconds of errors, try to reinitialize the reader
			 * This sometimes helps bring readers out of *crazy* states.
			 */
			/*
			 * if ( dwErrorCount == 10 ) { RFUnInitializeReader( rContext
			 * ); RFInitializeReader( rContext ); dwErrorCount = 0; }
			 */

			/*
			 * End of race condition code block
			 */
		}

		if (dwStatus & SCARD_ABSENT)
		{
			if (dwCurrentState == SCARD_PRESENT ||
				dwCurrentState == SCARD_UNKNOWN)
			{
				/*
				 * Change the status structure
				 */
				Log2(PCSC_LOG_INFO, "Card Removed From %s", lpcReader);
				/*
				 * Notify the card has been removed
				 */
				RFSetReaderEventState(rContext, SCARD_REMOVED);

				rs->xcardAtrLength(0);
				rs->xcardProtocol(SCARD_PROTOCOL_UNSET);
				DWORD readerStateTmp = rs->xreaderState();
				readerStateTmp |= SCARD_ABSENT;
				readerStateTmp &= ~SCARD_UNKNOWN;
				readerStateTmp &= ~SCARD_PRESENT;
				readerStateTmp &= ~SCARD_POWERED;
				readerStateTmp &= ~SCARD_NEGOTIABLE;
				readerStateTmp &= ~SCARD_SWALLOWED;
				readerStateTmp &= ~SCARD_SPECIFIC;
				rs->xreaderState(readerStateTmp);
				dwCurrentState = SCARD_ABSENT;

				SYS_MMapSynchronize((void *) rContext->readerState, pageSize);
			}

		}
		else if (dwStatus & SCARD_PRESENT)
		{
			if (dwCurrentState == SCARD_ABSENT ||
				dwCurrentState == SCARD_UNKNOWN)
			{
				/*
				 * Power and reset the card
				 */
				SYS_USleep(PCSCLITE_STATUS_WAIT);
				DWORD tmpCardAtrLength = MAX_ATR_SIZE;
				rv = IFDPowerICC(rContext, IFD_POWER_UP, rs->xcardAtr(), &tmpCardAtrLength);

				/* the protocol is unset after a power on */
				rs->xcardProtocol(SCARD_PROTOCOL_UNSET);

				secdebug("pcscd", "EHStatusHandlerThread: power-and-reset call to IFDPowerICC: %d [%04X]", rv, rv);

				DWORD readerStateTmp = rs->xreaderState();
				if (rv == IFD_SUCCESS)
				{
					rs->xcardAtrLength(tmpCardAtrLength);

					readerStateTmp |= SCARD_PRESENT;
					readerStateTmp &= ~SCARD_ABSENT;
					readerStateTmp |= SCARD_POWERED;
					readerStateTmp |= SCARD_NEGOTIABLE;
					readerStateTmp &= ~SCARD_SPECIFIC;
					readerStateTmp &= ~SCARD_UNKNOWN;
					readerStateTmp &= ~SCARD_SWALLOWED;
					rs->xreaderState(readerStateTmp);

					/*
					 * Notify the card has been reset
					 */
					RFSetReaderEventState(rContext, SCARD_RESET);
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
					rs->xreaderState(readerStateTmp);
					rs->xcardAtrLength(0);
				}

				dwCurrentState = SCARD_PRESENT;

				SYS_MMapSynchronize((void *) rContext->readerState, pageSize);

				Log2(PCSC_LOG_INFO, "Card inserted into %s", lpcReader);

				if (rv == IFD_SUCCESS)
				{
					if (rs->xcardAtrLength() > 0)
						LogXxd(PCSC_LOG_INFO, "Card ATR: ", rs->xcardAtr(), rs->xcardAtrLength());
					else
						Log1(PCSC_LOG_INFO, "Card ATR: (NULL)");
				}
				else
					Log1(PCSC_LOG_ERROR,"Error powering up card.");
			}
		}

		if (ReaderContextIsLocked(rContext))
		{
			/*
			 * Exit and notify the caller
			 */
			secdebug("pcscd", "EHStatusHandlerThread: parent requested shutdown");
			ReaderContextUnlock(rContext);
			SYS_ThreadDetach(rContext->pthThread);
			SYS_ThreadExit(0);
		}

		/*
		 * Sharing may change w/o an event pass it on
		 */

		if (dwReaderSharing != (uint32_t)rContext->dwContexts)
		{
			dwReaderSharing = rContext->dwContexts;
			rs->sharing(dwReaderSharing);
			SYS_MMapSynchronize((void *) rContext->readerState, pageSize);
		}

		SYS_USleep(PCSCLITE_STATUS_POLL_RATE);
	}
}

void EHSetSharingEvent(PREADER_CONTEXT rContext, DWORD dwValue)
{
	PCSCD::SharedReaderState *rs = PCSCD::SharedReaderState::overlay(rContext->readerState);
	rs->xlockState(dwValue);
}

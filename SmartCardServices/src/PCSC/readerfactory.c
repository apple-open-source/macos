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
 *  readerfactory.c
 *  SmartCardServices
 */


/******************************************************************

	MUSCLE SmartCard Development ( http://www.linuxnet.com )
	Title  : readerfactory.c
	Package: pcsc lite
	Author : David Corcoran
	Date   : 7/27/99
	License: Copyright (C) 1999 David Corcoran
			<corcoran@linuxnet.com>
	Purpose: This keeps track of a list of currently 
	available reader structures.

$Id: readerfactory.c 123 2010-03-27 10:50:42Z ludovic.rousseau@gmail.com $

********************************************************************/

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/errno.h>
#include <fcntl.h>

#include "wintypes.h"
#include "pcsclite.h"
#include "ifdhandler.h"
#include "debuglog.h"
#include "thread_generic.h"
#include "readerfactory.h"
#include "dyn_generic.h"
#include "sys_generic.h"
#include "eventhandler.h"
#include "ifdwrapper.h"
#include "readerState.h"

#include <security_utilities/debugging.h>

/*
 64 bit
 */

#include <mach/machine.h>
#include <sys/sysctl.h>
#include "configfile.h"

static cpu_type_t architectureForPid(pid_t pid);

#ifndef PCSCLITE_HP_BASE_PORT
#define PCSCLITE_HP_BASE_PORT       0x200000
#endif /* PCSCLITE_HP_BASE_PORT */

static LONG RFLoadReader(PREADER_CONTEXT);
static LONG RFUnBindFunctions(PREADER_CONTEXT);
static LONG RFUnloadReader(PREADER_CONTEXT);

static PREADER_CONTEXT sReadersContexts[PCSCLITE_MAX_READERS_CONTEXTS];
static DWORD dwNumReadersContexts = 0;
static DWORD lastLockID = 0;
static PCSCLITE_MUTEX_T sReadersContextsLock = NULL;

static int ReaderContextConstructor(PREADER_CONTEXT ctx, LPCSTR lpcReader, 
	DWORD dwPort, LPCSTR lpcLibrary, LPCSTR lpcDevice);
static void ReaderContextDestructor(PREADER_CONTEXT ctx);
static void ReaderContextFree(PREADER_CONTEXT ctx);
static void ReaderContextClear(PREADER_CONTEXT ctx);
static int ReaderContextInsert(PREADER_CONTEXT ctx);
static int ReaderContextRemove(PREADER_CONTEXT ctx);
static int ReaderContextCheckDuplicateReader(LPCSTR lpcReader, DWORD dwPort);
static int ReaderSlotCount(PREADER_CONTEXT ctx);
static BOOL ReaderDriverIsThreadSafe(PREADER_CONTEXT ctx, BOOL testSlot);
static BOOL ReaderNameMatchForIndex(DWORD dwPort, LPCSTR lpcReader, int index);
static void ReaderContextDuplicateSlot(PREADER_CONTEXT ctxBase, PREADER_CONTEXT ctxSlot, int slotNumber, BOOL baseIsThreadSafe);
static int ReaderCheckForClone(PREADER_CONTEXT ctx, LPCSTR lpcReader, 
	DWORD dwPort, LPCSTR lpcLibrary);

extern int DBUpdateReaders(const char *readerconf);

LONG RFAllocateReaderSpace()
{
	int i;

	sReadersContextsLock = (PCSCLITE_MUTEX_T) malloc(sizeof(PCSCLITE_MUTEX));
	SYS_MutexInit(sReadersContextsLock);

	/*
	 * Allocate each reader structure
	 */
	for (i = 0; i < PCSCLITE_MAX_READERS_CONTEXTS; i++)
		sReadersContexts[i] = (PREADER_CONTEXT) calloc(1, sizeof(READER_CONTEXT));

	/*
	 * Create public event structures
	 */
	return EHInitializeEventStructures();
}

LONG RFAddReader(LPSTR lpcReader, DWORD dwPort, LPSTR lpcLibrary, LPSTR lpcDevice)
{
	int slotCount;
	LONG rv = SCARD_E_NO_MEMORY;
	int slot;
	PREADER_CONTEXT baseContext = NULL;

	if ((lpcReader == NULL) || (lpcLibrary == NULL) || (lpcDevice == NULL))
		return SCARD_E_INVALID_VALUE;

	/* Reader name too long? */
	if (strlen(lpcReader) >= MAX_READERNAME)
	{
		Log3(PCSC_LOG_ERROR, "Reader name too long: %d chars instead of max %d",
			strlen(lpcReader), MAX_READERNAME);
		return SCARD_E_INVALID_VALUE;
	}

	/* Library name too long? */
	if (strlen(lpcLibrary) >= MAX_LIBNAME)
	{
		Log3(PCSC_LOG_ERROR, "Library name too long: %d chars instead of max %d",
			strlen(lpcLibrary), MAX_LIBNAME);
		return SCARD_E_INVALID_VALUE;
	}

	/* Device name too long? */
	if (strlen(lpcDevice) >= MAX_DEVICENAME)
	{
		Log3(PCSC_LOG_ERROR, "Device name too long: %d chars instead of max %d",
			strlen(lpcDevice), MAX_DEVICENAME);
		return SCARD_E_INVALID_VALUE;
	}

	rv = ReaderContextCheckDuplicateReader(lpcReader, dwPort);
	if (rv)
		return rv;

	// Make sure we have an empty slot to put the reader structure
	rv = ReaderContextInsert(NULL);
	if (rv != SCARD_S_SUCCESS)
		return rv;

	// Allocate a temporary reader context struct
	baseContext = (PREADER_CONTEXT) calloc(1, sizeof(READER_CONTEXT));

	rv = ReaderContextConstructor(baseContext, lpcReader, dwPort, lpcLibrary, lpcDevice);
	if (rv != SCARD_S_SUCCESS)
		goto xit;

	rv = ReaderCheckForClone(baseContext, lpcReader, dwPort, lpcLibrary);
	if (rv != SCARD_S_SUCCESS)
		goto xit;

	rv = RFInitializeReader(baseContext);
	if (rv != SCARD_S_SUCCESS)
		goto xit;

	rv = ReaderContextInsert(baseContext);
	if (rv != SCARD_S_SUCCESS)
		goto xit;

	rv = EHSpawnEventHandler(baseContext);
	if (rv != SCARD_S_SUCCESS)
		goto xit;

	slotCount = ReaderSlotCount(baseContext);
	if (slotCount <= 1)
		return SCARD_S_SUCCESS;

	/*
	 * Check the number of slots and create a different
	 * structure for each one accordingly
	 */

	BOOL baseIsThreadSafe = ReaderDriverIsThreadSafe(baseContext, 1);
	
	for (slot = 1; slot < slotCount; slot++)
	{
		// Make sure we have an empty slot to put the reader structure
		// If not, we remove the whole reader
		rv = ReaderContextInsert(NULL);
		if (rv != SCARD_S_SUCCESS)
		{
			rv = RFRemoveReader(lpcReader, dwPort);
			return rv;
		}

		// Allocate a temporary reader context struct
		PREADER_CONTEXT ctxSlot = (PREADER_CONTEXT) calloc(1, sizeof(READER_CONTEXT));

		rv = ReaderContextConstructor(ctxSlot, lpcReader, dwPort, lpcLibrary, lpcDevice);
		if (rv != SCARD_S_SUCCESS)
		{
			ReaderContextDestructor(ctxSlot);
			free(ctxSlot);
			return rv;
		}

		ReaderContextDuplicateSlot(baseContext, ctxSlot, slot, baseIsThreadSafe);

		rv = RFInitializeReader(ctxSlot);
		if (rv != SCARD_S_SUCCESS)
		{
			Log2(PCSC_LOG_ERROR, "%s init failed.", lpcReader);
			ReaderContextDestructor(ctxSlot);
			free(ctxSlot);
			return rv;
		}

		rv = ReaderContextInsert(ctxSlot);
		if (rv != SCARD_S_SUCCESS)
			return rv;

		rv = EHSpawnEventHandler(ctxSlot);
		if (rv != SCARD_S_SUCCESS)
			return rv;
	}

xit:
	if (rv != SCARD_S_SUCCESS)
	{
		// Cannot connect to reader, so exit gracefully
		Log3(PCSC_LOG_ERROR, "RFAddReader: %s init failed: %d", lpcReader, rv);
		ReaderContextDestructor(baseContext);
		free(baseContext);
	}

	return rv;
}

LONG RFRemoveReader(LPSTR lpcReader, DWORD dwPort)
{
	LONG rv;
	PREADER_CONTEXT tmpContext = NULL;

	if (lpcReader == 0)
		return SCARD_E_INVALID_VALUE;

	secdebug("pcscd", "RFRemoveReader: removing %s", lpcReader);
	while ((rv = RFReaderInfoNamePort(dwPort, lpcReader, &tmpContext)) == SCARD_S_SUCCESS)
	{
		// Try to destroy the thread
		rv = EHDestroyEventHandler(tmpContext);

		rv = RFUnInitializeReader(tmpContext);
		if (rv != SCARD_S_SUCCESS)
			return rv;

		ReaderContextRemove(tmpContext);
	}

	return SCARD_S_SUCCESS;
}

LONG RFSetReaderName(PREADER_CONTEXT rContext, LPCSTR readerName,
	LPCSTR libraryName, DWORD dwPort, DWORD dwSlot)
{
	LONG parent = -1;	/* reader number of the parent of the clone */
	DWORD valueLength;
	int currentDigit = -1;
	int supportedChannels = 0;
	int usedDigits[PCSCLITE_MAX_READERS_CONTEXTS] = {0,};
	int i;

	if ((0 == dwSlot) && (dwNumReadersContexts != 0))
	{
		for (i = 0; i < PCSCLITE_MAX_READERS_CONTEXTS; i++)
		{
			if (sReadersContexts[i] == NULL)
				continue;
			if ((sReadersContexts[i])->vHandle != 0)
			{
				if (strcmp((sReadersContexts[i])->lpcLibrary, libraryName) == 0)
				{
					UCHAR tagValue[1];
					LONG ret;

					/*
					 * Ask the driver if it supports multiple channels
					 */
					valueLength = sizeof(tagValue);
					ret = IFDGetCapabilities((sReadersContexts[i]),
						TAG_IFD_SIMULTANEOUS_ACCESS,
						&valueLength, tagValue);

					if ((ret == IFD_SUCCESS) && (valueLength == 1) &&
						(tagValue[0] > 1))
					{
						supportedChannels = tagValue[0];
						Log2(PCSC_LOG_INFO,
							"Support %d simultaneous readers", tagValue[0]);
					}
					else
						supportedChannels = 1;

					/*
					 * Check to see if it is a hotplug reader and
					 * different
					 */
					if (((((sReadersContexts[i])->dwPort & 0xFFFF0000) ==
							PCSCLITE_HP_BASE_PORT)
						&& ((sReadersContexts[i])->dwPort != dwPort))
						|| (supportedChannels > 1))
					{
						char *lpcReader = sReadersContexts[i]->lpcReader;

						/*
						 * tells the caller who the parent of this
						 * clone is so it can use it's shared
						 * resources like mutex/etc.
						 */
						parent = i;

						/*
						 * If the same reader already exists and it is
						 * hotplug then we must look for others and
						 * enumerate the readername
						 */
						currentDigit = strtol(lpcReader + strlen(lpcReader) - 5, NULL, 16);

						/*
						 * This spot is taken
						 */
						usedDigits[currentDigit] = 1;
					}
				}
			}
		}

	}

	/* default value */
	i = 0;

	/* Other identical readers exist on the same bus */
	if (currentDigit != -1)
	{
		for (i = 0; i < PCSCLITE_MAX_READERS_CONTEXTS; i++)
		{
			/* get the first free digit */
			if (usedDigits[i] == 0)
				break;
		}

		if (i == PCSCLITE_MAX_READERS_CONTEXTS)
		{
			Log2(PCSC_LOG_ERROR, "Max number of readers reached: %d", PCSCLITE_MAX_READERS_CONTEXTS);
			return -2;
		}

		if (i >= supportedChannels)
		{
			Log3(PCSC_LOG_ERROR, "Driver %s does not support more than "
				"%d reader(s). Maybe the driver should support "
				"TAG_IFD_SIMULTANEOUS_ACCESS", libraryName, supportedChannels);
			return -2;
		}
	}

	sprintf(rContext->lpcReader, "%s %02X %02X", readerName, i, dwSlot);

	/*
	 * Set the slot in 0xDDDDCCCC
	 */
	rContext->dwSlot = (i << 16) + dwSlot;

	return parent;
}

LONG RFReaderInfo(LPSTR lpcReader, PREADER_CONTEXT * sReader)
{
	int i;
	LONG rv = SCARD_E_UNKNOWN_READER;
	
	if (lpcReader == 0)
		return SCARD_E_UNKNOWN_READER;

	SYS_MutexLock(sReadersContextsLock);
	for (i = 0; i < PCSCLITE_MAX_READERS_CONTEXTS; i++)
	{
		if ((sReadersContexts[i]!=NULL) && ((sReadersContexts[i])->vHandle != 0))
		{
			if (strcmp(lpcReader, (sReadersContexts[i])->lpcReader) == 0)
			{
				*sReader = sReadersContexts[i];
				rv = SCARD_S_SUCCESS;
				break;
			}
		}
	}
	SYS_MutexUnLock(sReadersContextsLock);

	return rv;
}

LONG RFReaderInfoNamePort(DWORD dwPort, LPSTR lpcReader,
	PREADER_CONTEXT * sReader)
{
	int ix;
	LONG rv = SCARD_E_INVALID_VALUE;

	SYS_MutexLock(sReadersContextsLock);
	for (ix = 0; ix < PCSCLITE_MAX_READERS_CONTEXTS; ix++)
	{
		if ((sReadersContexts[ix]!=NULL) && ((sReadersContexts[ix])->vHandle != 0) &&
			ReaderNameMatchForIndex(dwPort, lpcReader, ix))
			{
				*sReader = sReadersContexts[ix];
				rv = SCARD_S_SUCCESS;
				break;
			}
	}
	SYS_MutexUnLock(sReadersContextsLock);

	return rv;
}

LONG RFReaderInfoById(DWORD dwIdentity, PREADER_CONTEXT * sReader)
{
	int i;
	LONG rv = SCARD_E_INVALID_VALUE;

	/*
	 * Strip off the lower nibble and get the identity
	 */
	dwIdentity = dwIdentity >> (sizeof(DWORD) / 2) * 8;
	dwIdentity = dwIdentity << (sizeof(DWORD) / 2) * 8;

	SYS_MutexLock(sReadersContextsLock);
	for (i = 0; i < PCSCLITE_MAX_READERS_CONTEXTS; i++)
	{
		if ((sReadersContexts[i]!=NULL) && (dwIdentity == (sReadersContexts[i])->dwIdentity))
		{
			*sReader = sReadersContexts[i];
			rv = SCARD_S_SUCCESS;
			break;
		}
	}
	SYS_MutexUnLock(sReadersContextsLock);

	return rv;
}

static LONG RFLoadReader(PREADER_CONTEXT rContext)
{
	if (rContext->vHandle != 0)
	{
		Log1(PCSC_LOG_ERROR, "Warning library pointer not NULL");
		/*
		 * Another reader exists with this library loaded
		 */
		return SCARD_S_SUCCESS;
	}

	return DYN_LoadLibrary(&rContext->vHandle, rContext->lpcLibrary);
}

LONG RFBindFunctions(PREADER_CONTEXT rContext)
{
	int rv1, rv2, rv3;
	void *f;

	/*
	 * Use this function as a dummy to determine the IFD Handler version
	 * type  1.0/2.0/3.0.  Suppress error messaging since it can't be 1.0,
	 * 2.0 and 3.0.
	 */

	Log1(PCSC_LOG_INFO, "Binding driver functions");

//	DebugLogSuppress(DEBUGLOG_IGNORE_ENTRIES);

	rv1 = DYN_GetAddress(rContext->vHandle, &f, "IO_Create_Channel");
	rv2 = DYN_GetAddress(rContext->vHandle, &f, "IFDHCreateChannel");
	rv3 = DYN_GetAddress(rContext->vHandle, &f, "IFDHCreateChannelByName");

//	DebugLogSuppress(DEBUGLOG_LOG_ENTRIES);

	if (rv1 != SCARD_S_SUCCESS && rv2 != SCARD_S_SUCCESS && rv3 != SCARD_S_SUCCESS)
	{
		/*
		 * Neither version of the IFD Handler was found - exit
		 */
		Log1(PCSC_LOG_CRITICAL, "IFDHandler functions missing");

		exit(1);
	} else if (rv1 == SCARD_S_SUCCESS)
	{
		/*
		 * Ifd Handler 1.0 found
		 */
		rContext->dwVersion = IFD_HVERSION_1_0;
	} else if (rv3 == SCARD_S_SUCCESS)
	{
		/*
		 * Ifd Handler 3.0 found
		 */
		rContext->dwVersion = IFD_HVERSION_3_0;
	}
	else
	{
		/*
		 * Ifd Handler 2.0 found
		 */
		rContext->dwVersion = IFD_HVERSION_2_0;
	}

	/*
	 * The following binds version 1.0 of the IFD Handler specs
	 */

	if (rContext->dwVersion == IFD_HVERSION_1_0)
	{
		Log1(PCSC_LOG_INFO, "Loading IFD Handler 1.0");

#define GET_ADDRESS_OPTIONALv1(field, function, code) \
{ \
	void *f1 = NULL; \
	if (SCARD_S_SUCCESS != DYN_GetAddress(rContext->vHandle, &f1, "IFD_" #function)) \
	{ \
		code \
	} \
	rContext->psFunctions.psFunctions_v1.pvf ## field = f1; \
}

#define GET_ADDRESSv1(field, function) \
	GET_ADDRESS_OPTIONALv1(field, function, \
		Log1(PCSC_LOG_CRITICAL, "IFDHandler functions missing: " #function ); \
		exit(1); )

		DYN_GetAddress(rContext->vHandle, &f, "IO_Create_Channel");
		rContext->psFunctions.psFunctions_v1.pvfCreateChannel = f;

		if (SCARD_S_SUCCESS != DYN_GetAddress(rContext->vHandle, &f,
			"IO_Close_Channel"))
		{
			Log1(PCSC_LOG_CRITICAL, "IFDHandler functions missing");
			exit(1);
		}
		rContext->psFunctions.psFunctions_v1.pvfCloseChannel = f;

		GET_ADDRESSv1(GetCapabilities, Get_Capabilities)
		GET_ADDRESSv1(SetCapabilities, Set_Capabilities)
		GET_ADDRESSv1(PowerICC, Power_ICC)
		GET_ADDRESSv1(TransmitToICC, Transmit_to_ICC)
		GET_ADDRESSv1(ICCPresence, Is_ICC_Present)

		GET_ADDRESS_OPTIONALv1(SetProtocolParameters, Set_Protocol_Parameters, )
	}
	else if (rContext->dwVersion == IFD_HVERSION_2_0)
	{
		/*
		 * The following binds version 2.0 of the IFD Handler specs
		 */

#define GET_ADDRESS_OPTIONALv2(s, code) \
{ \
	void *f1 = NULL; \
	if (SCARD_S_SUCCESS != DYN_GetAddress(rContext->vHandle, &f1, "IFDH" #s)) \
	{ \
		code \
	} \
	rContext->psFunctions.psFunctions_v2.pvf ## s = f1; \
}

#define GET_ADDRESSv2(s) \
	GET_ADDRESS_OPTIONALv2(s, \
		Log1(PCSC_LOG_CRITICAL, "IFDHandler functions missing: " #s ); \
		exit(1); )

		Log1(PCSC_LOG_INFO, "Loading IFD Handler 2.0");

		GET_ADDRESSv2(CreateChannel)
		GET_ADDRESSv2(CloseChannel)
		GET_ADDRESSv2(GetCapabilities)
		GET_ADDRESSv2(SetCapabilities)
		GET_ADDRESSv2(PowerICC)
		GET_ADDRESSv2(TransmitToICC)
		GET_ADDRESSv2(ICCPresence)
		GET_ADDRESS_OPTIONALv2(SetProtocolParameters, )

		GET_ADDRESSv2(Control)
	}
	else if (rContext->dwVersion == IFD_HVERSION_3_0)
	{
		/*
		 * The following binds version 3.0 of the IFD Handler specs
		 */

#define GET_ADDRESS_OPTIONALv3(s, code) \
{ \
	void *f1 = NULL; \
	if (SCARD_S_SUCCESS != DYN_GetAddress(rContext->vHandle, &f1, "IFDH" #s)) \
	{ \
		code \
	} \
	rContext->psFunctions.psFunctions_v3.pvf ## s = f1; \
}

#define GET_ADDRESSv3(s) \
	GET_ADDRESS_OPTIONALv3(s, \
		Log1(PCSC_LOG_CRITICAL, "IFDHandler functions missing: " #s ); \
		exit(1); )

		Log1(PCSC_LOG_INFO, "Loading IFD Handler 3.0");

		GET_ADDRESSv2(CreateChannel)
		GET_ADDRESSv2(CloseChannel)
		GET_ADDRESSv2(GetCapabilities)
		GET_ADDRESSv2(SetCapabilities)
		GET_ADDRESSv2(PowerICC)
		GET_ADDRESSv2(TransmitToICC)
		GET_ADDRESSv2(ICCPresence)
		GET_ADDRESS_OPTIONALv2(SetProtocolParameters, )

		GET_ADDRESSv3(CreateChannelByName)
		GET_ADDRESSv3(Control)
	}
	else
	{
		/*
		 * Who knows what could have happenned for it to get here.
		 */
		Log1(PCSC_LOG_CRITICAL, "IFD Handler not 1.0/2.0 or 3.0");
		exit(1);
	}

	return SCARD_S_SUCCESS;
}

static LONG RFUnBindFunctions(PREADER_CONTEXT rContext)
{
	/*
	 * Zero out everything
	 */

	Log1(PCSC_LOG_INFO, "Unbinding driver functions");
	memset(&rContext->psFunctions, 0, sizeof(rContext->psFunctions));

	return SCARD_S_SUCCESS;
}

static LONG RFUnloadReader(PREADER_CONTEXT rContext)
{
	/*
	 * Make sure no one else is using this library
	 */

		Log1(PCSC_LOG_INFO, "Unloading reader driver.");
	if (*rContext->pdwFeeds == 1)
	{
		Log1(PCSC_LOG_INFO, "--- closing dynamic library");
		DYN_CloseLibrary(&rContext->vHandle);
	}

	rContext->vHandle = 0;

	return SCARD_S_SUCCESS;
}

LONG RFCheckSharing(DWORD hCard)
{
	LONG rv;
	PREADER_CONTEXT rContext = NULL;

	rv = RFReaderInfoById(hCard, &rContext);

	if (rv != SCARD_S_SUCCESS)
		return rv;

	if (rContext->dwLockId == 0 || rContext->dwLockId == hCard)
		return SCARD_S_SUCCESS;
	else
	{
		secdebug("pcscd", "RFCheckSharing: sharing violation, dwLockId: 0x%02X", rContext->dwLockId);
		return SCARD_E_SHARING_VIOLATION;
	}
}

LONG RFLockSharing(DWORD hCard)
{
	PREADER_CONTEXT rContext = NULL;

	RFReaderInfoById(hCard, &rContext);

	if (RFCheckSharing(hCard) == SCARD_S_SUCCESS)
	{
		EHSetSharingEvent(rContext, 1);
		rContext->dwLockId = hCard;
	}
	else
		return SCARD_E_SHARING_VIOLATION;

	return SCARD_S_SUCCESS;
}

LONG RFUnlockSharing(DWORD hCard)
{
	PREADER_CONTEXT rContext = NULL;
	LONG rv;

	rv = RFReaderInfoById(hCard, &rContext);
	if (rv != SCARD_S_SUCCESS)
		return rv;

	rv = RFCheckSharing(hCard);
	if (rv != SCARD_S_SUCCESS)
		return rv;

	EHSetSharingEvent(rContext, 0);
	rContext->dwLockId = 0;

	return SCARD_S_SUCCESS;
}

LONG RFUnblockContext(SCARDCONTEXT hContext)
{
	int i;

	SYS_MutexLock(sReadersContextsLock);
	for (i = 0; i < PCSCLITE_MAX_READERS_CONTEXTS; i++)
		if (sReadersContexts[i])
			(sReadersContexts[i])->dwBlockStatus = hContext;
	SYS_MutexUnLock(sReadersContextsLock);

	return SCARD_S_SUCCESS;
}

LONG RFUnblockReader(PREADER_CONTEXT rContext)
{
	rContext->dwBlockStatus = BLOCK_STATUS_RESUME;
	return SCARD_S_SUCCESS;
}

LONG RFInitializeReader(PREADER_CONTEXT rContext)
{
	LONG rv;

	/*
	 * Spawn the event handler thread
	 */
	Log3(PCSC_LOG_INFO, "Attempting startup of %s using %s",
		rContext->lpcReader, rContext->lpcLibrary);

  /******************************************/
	/*
	 * This section loads the library
	 */
  /******************************************/
	rv = RFLoadReader(rContext);
	if (rv != SCARD_S_SUCCESS)
	{
		Log2(PCSC_LOG_ERROR, "RFLoadReader failed: %X", rv);
		return rv;
	}

  /*******************************************/
	/*
	 * This section binds the functions
	 */
  /*******************************************/
	rv = RFBindFunctions(rContext);

	if (rv != SCARD_S_SUCCESS)
	{
		Log2(PCSC_LOG_ERROR, "RFBindFunctions failed: %X", rv);
		RFUnloadReader(rContext);
		return rv;
	}

  /*******************************************/
	/*
	 * This section tries to open the port
	 */
  /*******************************************/

	rv = IFDOpenIFD(rContext);

	if (rv != IFD_SUCCESS)
	{
		Log3(PCSC_LOG_CRITICAL, "Open Port %X Failed (%s)",
			rContext->dwPort, rContext->lpcDevice);
		RFUnBindFunctions(rContext);
		RFUnloadReader(rContext);
		return SCARD_E_INVALID_TARGET;
	}

	return SCARD_S_SUCCESS;
}

LONG RFUnInitializeReader(PREADER_CONTEXT rContext)
{
	Log2(PCSC_LOG_INFO, "Attempting shutdown of %s.",
		rContext->lpcReader);

	/*
	 * Close the port, unbind the functions, and unload the library
	 */

	/*
	 * If the reader is getting uninitialized then it is being unplugged
	 * so I can't send a IFDPowerICC call to it
	 *
	 * IFDPowerICC( rContext, IFD_POWER_DOWN, Atr, &AtrLen );
	 */
	IFDCloseIFD(rContext);
	RFUnBindFunctions(rContext);
	RFUnloadReader(rContext);

	return SCARD_S_SUCCESS;
}

SCARDHANDLE RFCreateReaderHandle(PREADER_CONTEXT rContext)
{
	USHORT randHandle;

	/*
	 * Create a random handle with 16 bits check to see if it already is
	 * used.
	 */
	randHandle = SYS_Random(SYS_GetSeed(), 10, 65000);

	while (1)
	{
		int i;

		SYS_MutexLock(sReadersContextsLock);
		for (i = 0; i < PCSCLITE_MAX_READERS_CONTEXTS; i++)
		{
			if ((sReadersContexts[i]!=NULL) && ((sReadersContexts[i])->vHandle != 0))
			{
				int j;

				for (j = 0; j < PCSCLITE_MAX_READER_CONTEXT_CHANNELS; j++)
				{
					if ((rContext->dwIdentity + randHandle) ==
						(sReadersContexts[i])->psHandles[j].hCard)
					{
						/*
						 * Get a new handle and loop again
						 */
						randHandle = SYS_Random(randHandle, 10, 65000);
						continue;
					}
				}
			}
		}
		SYS_MutexUnLock(sReadersContextsLock);

		/*
		 * Once the for loop is completed w/o restart a good handle was
		 * found and the loop can be exited.
		 */

		if (i == PCSCLITE_MAX_READERS_CONTEXTS)
			break;
	}

	return rContext->dwIdentity + randHandle;
}

LONG RFFindReaderHandle(SCARDHANDLE hCard)
{
	int i;
	LONG rv = SCARD_E_INVALID_HANDLE;
	
	SYS_MutexLock(sReadersContextsLock);
	for (i = 0; i < PCSCLITE_MAX_READERS_CONTEXTS; i++)
	{
		if ((sReadersContexts[i]!=NULL) && ((sReadersContexts[i])->vHandle != 0))
		{
			int j;

			for (j = 0; j < PCSCLITE_MAX_READER_CONTEXT_CHANNELS; j++)
			{
				if (hCard == (sReadersContexts[i])->psHandles[j].hCard)
				{
					rv = SCARD_S_SUCCESS;
					goto xit;
				}
			}
		}
	}
xit:
	SYS_MutexUnLock(sReadersContextsLock);

	return rv;
}

LONG RFDestroyReaderHandle(SCARDHANDLE hCard)
{
	return SCARD_S_SUCCESS;
}

LONG RFAddReaderHandle(PREADER_CONTEXT rContext, SCARDHANDLE hCard)
{
	int i;

	for (i = 0; i < PCSCLITE_MAX_READER_CONTEXT_CHANNELS; i++)
	{
		if (rContext->psHandles[i].hCard == 0)
		{
			rContext->psHandles[i].hCard = hCard;
			rContext->psHandles[i].dwEventStatus = 0;
			break;
		}
	}

	if (i == PCSCLITE_MAX_READER_CONTEXT_CHANNELS)
		/* List is full */
		return SCARD_E_INSUFFICIENT_BUFFER;

	return SCARD_S_SUCCESS;
}

LONG RFRemoveReaderHandle(PREADER_CONTEXT rContext, SCARDHANDLE hCard)
{
	int i;

	for (i = 0; i < PCSCLITE_MAX_READER_CONTEXT_CHANNELS; i++)
	{
		if (rContext->psHandles[i].hCard == hCard)
		{
			rContext->psHandles[i].hCard = 0;
			rContext->psHandles[i].dwEventStatus = 0;
			break;
		}
	}

	if (i == PCSCLITE_MAX_READER_CONTEXT_CHANNELS)
		/* Not Found */
		return SCARD_E_INVALID_HANDLE;

	return SCARD_S_SUCCESS;
}

LONG RFSetReaderEventState(PREADER_CONTEXT rContext, DWORD dwEvent)
{
	int i;

	/*
	 * Set all the handles for that reader to the event
	 */
	for (i = 0; i < PCSCLITE_MAX_READER_CONTEXT_CHANNELS; i++)
	{
		if (rContext->psHandles[i].hCard != 0)
			rContext->psHandles[i].dwEventStatus = dwEvent;
	}

	return SCARD_S_SUCCESS;
}

LONG RFCheckReaderEventState(PREADER_CONTEXT rContext, SCARDHANDLE hCard)
{
	int i;

	for (i = 0; i < PCSCLITE_MAX_READER_CONTEXT_CHANNELS; i++)
	{
		if (rContext->psHandles[i].hCard == hCard)
		{
			if (rContext->psHandles[i].dwEventStatus == SCARD_REMOVED)
				return SCARD_W_REMOVED_CARD;
			else
			{
				if (rContext->psHandles[i].dwEventStatus == SCARD_RESET)
					return SCARD_W_RESET_CARD;
				else
				{
					if (rContext->psHandles[i].dwEventStatus == 0)
						return SCARD_S_SUCCESS;
					else
						return SCARD_E_INVALID_VALUE;
				}
			}
		}
	}

	return SCARD_E_INVALID_HANDLE;
}

LONG RFClearReaderEventState(PREADER_CONTEXT rContext, SCARDHANDLE hCard)
{
	int i;

	for (i = 0; i < PCSCLITE_MAX_READER_CONTEXT_CHANNELS; i++)
	{
		if (rContext->psHandles[i].hCard == hCard)
			rContext->psHandles[i].dwEventStatus = 0;
	}

	if (i == PCSCLITE_MAX_READER_CONTEXT_CHANNELS)
		/* Not Found */
		return SCARD_E_INVALID_HANDLE;

	return SCARD_S_SUCCESS;
}

LONG RFCheckReaderStatus(PREADER_CONTEXT rContext)
{
	LONG rx = 0;
	rx = ((rContext == NULL) || (rContext->readerState == NULL) || 
		(SharedReaderState_State(rContext->readerState) & SCARD_UNKNOWN))?SCARD_E_READER_UNAVAILABLE:SCARD_S_SUCCESS;
	return rx;
}

void RFCleanupReaders(int shouldExit)
{
	int i;

	Log1(PCSC_LOG_INFO, "entering cleaning function");
	for (i = 0; i < PCSCLITE_MAX_READERS_CONTEXTS; i++)
	{
		if ((sReadersContexts[i]!=NULL) && (sReadersContexts[i]->vHandle != 0))
		{
			LONG rv;
			char lpcStripReader[MAX_READERNAME];

			Log2(PCSC_LOG_INFO, "Stopping reader: %s",
				sReadersContexts[i]->lpcReader);

			strncpy(lpcStripReader, (sReadersContexts[i])->lpcReader,
				sizeof(lpcStripReader));
			/*
			 * strip the 6 last char ' 00 00'
			 */
			lpcStripReader[strlen(lpcStripReader) - 6] = '\0';

			rv = RFRemoveReader(lpcStripReader, sReadersContexts[i]->dwPort);

			if (rv != SCARD_S_SUCCESS)
				Log2(PCSC_LOG_ERROR, "RFRemoveReader error: 0x%08X", rv);
		}
	}

	secdebug("pcscd", "RFCleanupReaders: exiting cleaning function");
	/*
	 * exit() will call at_exit()
	 */

	if (shouldExit)
		exit(0);
}

int RFStartSerialReaders(const char *readerconf)
{
	return DBUpdateReaders((char *)readerconf);
}

void RFReCheckReaderConf(void)
{
}

void RFSuspendAllReaders() 
{
	int ix;
	secdebug("pcscd", "RFSuspendAllReaders");
	Log1(PCSC_LOG_DEBUG, "zzzzz zzzzz zzzzz zzzzz RFSuspendAllReaders zzzzz zzzzz zzzzz zzzzz ");

	// @@@ We still need code to mark state first as "trying to sleep", in case
	// not all of it gets done before we sleep
	for (ix = 0; ix < PCSCLITE_MAX_READERS_CONTEXTS; ix++)
	{
		if ((sReadersContexts[ix]!=NULL) && ((sReadersContexts[ix])->vHandle != 0))
		{
			EHDestroyEventHandler(sReadersContexts[ix]);
			IFDCloseIFD(sReadersContexts[ix]);
		}
	}
}

void RFAwakeAllReaders(void)
{
	LONG rv = IFD_SUCCESS;
	int i;

	secdebug("pcscd", "RFAwakeAllReaders");
	Log1(PCSC_LOG_DEBUG, "----- ----- ----- ----- RFAwakeAllReaders ----- ----- ----- -----  ");
	for (i = 0; i < PCSCLITE_MAX_READERS_CONTEXTS; i++)
	{
		if (sReadersContexts[i]==NULL)
			continue;
		/* If the library is loaded and the event handler is not running */
		if ( ((sReadersContexts[i])->vHandle   != 0) &&
		     ((sReadersContexts[i])->pthThread == 0) )
		{
			int jx;
			int alreadyInitializedFlag = 0;

			// If a clone of this already did the initialization, 
			// set flag so we don't do again
			for (jx=0; jx < i; jx++)
			{
				if (((sReadersContexts[jx])->vHandle == (sReadersContexts[i])->vHandle)&&
					((sReadersContexts[jx])->dwPort  == (sReadersContexts[i])->dwPort)&&
					((sReadersContexts[jx])->dwSlot  == (sReadersContexts[i])->dwSlot))
				{
					alreadyInitializedFlag = 1;
				}
			}

			if (!alreadyInitializedFlag)
			{
				SYS_USleep(100000L);	// 0.1s (in microseconds)
				rv = IFDOpenIFD(sReadersContexts[i]);
			}
			
			RFSetReaderEventState(sReadersContexts[i], SCARD_RESET);
			if (rv != IFD_SUCCESS)
			{
				Log3(PCSC_LOG_ERROR, "Open Port %X Failed (%s)",
					(sReadersContexts[i])->dwPort, (sReadersContexts[i])->lpcDevice);
				Log2(PCSC_LOG_ERROR, "  with error 0x%08X", rv);
				continue;
			}

			EHSpawnEventHandler(sReadersContexts[i]);
		}
	}
}

#pragma mark ---------- Context Share Lock Tracking ----------

void ReaderContextLock(PREADER_CONTEXT rContext)
{
	if (rContext)
	{
		secdebug("pcscd", "===> ReaderContextLock [was: %02X]", rContext->dwLockId);
		rContext->dwLockId = 0xFFFF;
		lastLockID = -3;			// something different
	}
}

void ReaderContextUnlock(PREADER_CONTEXT rContext)
{
	if (rContext)
	{
		secdebug("pcscd", "<=== ReaderContextUnlock [was: %02X]", rContext->dwLockId);
		rContext->dwLockId = 0;
		lastLockID = -2;			// something different
	}
}

int ReaderContextIsLocked(PREADER_CONTEXT rContext)
{
	if (rContext)
	{
		if (rContext->dwLockId && (rContext->dwLockId != lastLockID))		// otherwise too many messages
		{
			lastLockID = rContext->dwLockId;
			secdebug("pcscd", ".... ReaderContextLock state: %02X", rContext->dwLockId);
		}
		return (rContext->dwLockId == 0xFFFF)?1:0;
	}
	else
		return 0;
}

#pragma mark ---------- Reader Context Management ----------

static int ReaderContextConstructor(PREADER_CONTEXT ctx, LPCSTR lpcReader, 
	DWORD dwPort, LPCSTR lpcLibrary, LPCSTR lpcDevice)
{
	// We assume the struct was created with a calloc, so we don't call ReaderContextClear
	if (!ctx)
		return SCARD_E_NO_MEMORY;
	
	strlcpy(ctx->lpcLibrary, lpcLibrary, sizeof(ctx->lpcLibrary));
	strlcpy(ctx->lpcDevice,  lpcDevice,  sizeof(ctx->lpcDevice));
	ctx->dwPort = dwPort;

	/*	
		Initialize pdwFeeds to 1, otherwise multiple cloned readers will cause 
		pcscd to crash when RFUnloadReader unloads the driver library
		and there are still devices attached using it
	*/
	ctx->pdwFeeds = malloc(sizeof(DWORD));
	*ctx->pdwFeeds = 1;

	ctx->mMutex = (PCSCLITE_MUTEX_T) malloc(sizeof(PCSCLITE_MUTEX));
	SYS_MutexInit(ctx->mMutex);

	ctx->pdwMutex = malloc(sizeof(DWORD));
	*ctx->pdwMutex = 1;

	return SCARD_S_SUCCESS;
}

static int ReaderCheckForClone(PREADER_CONTEXT ctx, LPCSTR lpcReader, 
	DWORD dwPort, LPCSTR lpcLibrary)
{
	// Check and set the readername to see if it must be enumerated
	// A parentNode of -2 or less indicates fatal error
	
	LONG parentNode = RFSetReaderName(ctx, lpcReader, lpcLibrary, dwPort, 0);
	if (parentNode < -1)			
		return SCARD_E_NO_MEMORY;

	// If a clone to this reader exists take some values from that clone
	if ((parentNode >= 0) && (parentNode < PCSCLITE_MAX_READERS_CONTEXTS)
		&& sReadersContexts[parentNode])
	{
		SYS_MutexLock(sReadersContextsLock);
		ctx->pdwFeeds = (sReadersContexts[parentNode])->pdwFeeds;
		*ctx->pdwFeeds += 1;
		ctx->vHandle = (sReadersContexts[parentNode])->vHandle;
		ctx->mMutex = (sReadersContexts[parentNode])->mMutex;
		ctx->pdwMutex = (sReadersContexts[parentNode])->pdwMutex;
		SYS_MutexUnLock(sReadersContextsLock);

		if (0 && ReaderDriverIsThreadSafe(sReadersContexts[parentNode], 0))
		{
			ctx->mMutex = 0;
			ctx->pdwMutex = NULL;
		}
		else
			*ctx->pdwMutex += 1;
	}

	return SCARD_S_SUCCESS;
}

static void ReaderContextDestructor(PREADER_CONTEXT ctx)
{
	ReaderContextFree(ctx);
}

static void ReaderContextFree(PREADER_CONTEXT ctx)
{
	if (!ctx)
		return;

	// Destroy and free the mutex
	if (ctx->pdwMutex)
	{
		if (*ctx->pdwMutex == 1)
		{
			SYS_MutexDestroy(ctx->mMutex);
			free(ctx->mMutex);
		}
		*ctx->pdwMutex -= 1;
	}
	
	// Destroy and free the mutex counter
	if (ctx->pdwMutex && (*ctx->pdwMutex == 0))
	{
		free(ctx->pdwMutex);
		ctx->pdwMutex = NULL;
	}

	if (ctx->pdwFeeds)
	{
		*ctx->pdwFeeds -= 1;
		if (*ctx->pdwFeeds == 0)
		{
			free(ctx->pdwFeeds);
			ctx->pdwFeeds = NULL;
		}
	}
	
	// zero out everything else
	ReaderContextClear(ctx);
}

static void ReaderContextClear(PREADER_CONTEXT ctx)
{
	// This assumes that ReaderContextFree has already been called if necessary
	if (ctx)
		memset(ctx, 0, sizeof(READER_CONTEXT));
}

static int ReaderContextInsert(PREADER_CONTEXT ctx)
{
	// Find an empty slot to put the reader structure, and copy it in
	// If NULL is passed in, just return whether a spot is available or not

	int ix, rv = SCARD_E_NO_MEMORY;
	
	SYS_MutexLock(sReadersContextsLock);
	for (ix = 0; ix < PCSCLITE_MAX_READERS_CONTEXTS; ix++)
	{
		if ((sReadersContexts[ix] == NULL) || (sReadersContexts[ix])->vHandle == 0)
		{
			if (ctx)
			{
				if (sReadersContexts[ix])
					free(sReadersContexts[ix]);
				sReadersContexts[ix] = ctx;
				(sReadersContexts[ix])->dwIdentity = (ix + 1) << (sizeof(DWORD) / 2) * 8;
				dwNumReadersContexts += 1;
			}
			rv = SCARD_S_SUCCESS;
			break;
		}
	}
	SYS_MutexUnLock(sReadersContextsLock);
	return rv;
}

static int ReaderContextRemove(PREADER_CONTEXT ctx)
{
	int ix, rv = SCARD_E_UNKNOWN_READER;
	PREADER_CONTEXT ctxToRemove = NULL;
	DWORD dwPort = ctx->dwPort;
	LPSTR lpcReader = ctx->lpcReader;
	SYS_MutexLock(sReadersContextsLock);
	for (ix = 0; ix < PCSCLITE_MAX_READERS_CONTEXTS; ix++)
	{
		if (!ReaderNameMatchForIndex(dwPort, lpcReader, ix))
			continue;

		ctxToRemove = sReadersContexts[ix];
		sReadersContexts[ix] = NULL;
		dwNumReadersContexts -= 1;
		rv = SCARD_S_SUCCESS;
		break;
	}
	SYS_MutexUnLock(sReadersContextsLock);
	// We can do this cleanup outside the lock
	if (ctxToRemove)
	{
		ReaderContextDestructor(ctxToRemove);
		free(ctxToRemove);
	}
	return rv;
}

static int ReaderContextCheckDuplicateReader(LPCSTR lpcReader, DWORD dwPort)
{
	// Readers with the same name and same port cannot be used

	if (dwNumReadersContexts == 0)
		return SCARD_S_SUCCESS;

	int ix, rv = SCARD_S_SUCCESS;
	SYS_MutexLock(sReadersContextsLock);
	for (ix = 0; ix < PCSCLITE_MAX_READERS_CONTEXTS; ix++)
	{
		if ((sReadersContexts[ix]==NULL) || ((sReadersContexts[ix])->vHandle == 0))
			continue;
		
		if (ReaderNameMatchForIndex(dwPort, lpcReader, ix))
		{
			Log1(PCSC_LOG_ERROR, "Duplicate reader found.");
			rv = SCARD_E_DUPLICATE_READER;
			break;
		}
	}
	SYS_MutexUnLock(sReadersContextsLock);
	return rv;
}

static int ReaderSlotCount(PREADER_CONTEXT ctx)
{
	// Call on the driver to see if there are multiple slots
	// If we encounter errors, pretend it is just a single slot reader
	
	UCHAR ucGetData[1];
	DWORD dwGetSize = sizeof(ucGetData);
	int rv = IFDGetCapabilities(ctx, TAG_IFD_SLOTS_NUMBER, &dwGetSize, ucGetData);

	//Reader does not have this defined, so assume a single slot
	if (rv != IFD_SUCCESS || dwGetSize != 1 || ucGetData[0] == 0)
		return 1;

	// Reader has this defined and it only has one slot
	if (rv == IFD_SUCCESS && dwGetSize == 1 && ucGetData[0] == 1)
		return 1;

	return (int)ucGetData[0];
}

static BOOL ReaderDriverIsThreadSafe(PREADER_CONTEXT ctx, BOOL testSlot)
{
	// Call on the driver to see if it is thread safe
	UCHAR ucThread[1];
	DWORD dwGetSize = sizeof(ucThread);
	int rv = IFDGetCapabilities(ctx, testSlot?TAG_IFD_SLOT_THREAD_SAFE:TAG_IFD_THREAD_SAFE, 
		&dwGetSize, ucThread);
	if (rv == IFD_SUCCESS && dwGetSize == 1 && ucThread[0] == 1)
	{
		Log1(PCSC_LOG_INFO, "Driver is thread safe");
		return 1;
	}
	else
	{
		Log1(PCSC_LOG_INFO, "Driver is not thread safe");
		return 0;
	}
}

static BOOL ReaderNameMatchForIndex(DWORD dwPort, LPCSTR lpcReader, int index)
{
	// "index" is index in sReadersContexts
	char lpcStripReader[MAX_READERNAME];
	int tmplen;

	if (sReadersContexts[index]==NULL)
		return 0;

	strncpy(lpcStripReader, (sReadersContexts[index])->lpcReader, sizeof(lpcStripReader));
	tmplen = strlen(lpcStripReader);
	lpcStripReader[tmplen - 6] = 0;

	return ((strcmp(lpcReader, lpcStripReader) == 0) && (dwPort == (sReadersContexts[index])->dwPort))?1:0;
}

static void ReaderContextDuplicateSlot(PREADER_CONTEXT ctxBase, PREADER_CONTEXT ctxSlot, int slotNumber, BOOL baseIsThreadSafe)
{
	// Copy the previous reader name and set the slot number
	// The slot number for the base is 0

	int ix;
	char *tmpReader = ctxSlot->lpcReader;
	strlcpy(tmpReader, ctxBase->lpcReader, sizeof(ctxSlot->lpcReader));
	sprintf(tmpReader + strlen(tmpReader) - 2, "%02X", slotNumber);

	strlcpy(ctxSlot->lpcLibrary, ctxBase->lpcLibrary, sizeof(ctxSlot->lpcLibrary));
	strlcpy(ctxSlot->lpcDevice,  ctxBase->lpcDevice,  sizeof(ctxSlot->lpcDevice));

	ctxSlot->dwVersion = ctxBase->dwVersion;
	ctxSlot->dwPort = ctxBase->dwPort;
	ctxSlot->vHandle = ctxBase->vHandle;
	ctxSlot->mMutex = ctxBase->mMutex;
	ctxSlot->pdwMutex = ctxBase->pdwMutex;
	ctxSlot->dwSlot = ctxBase->dwSlot + slotNumber;

	ctxSlot->pdwFeeds = ctxBase->pdwFeeds;

	*ctxSlot->pdwFeeds += 1;

	ctxSlot->dwBlockStatus = 0;
	ctxSlot->dwContexts = 0;
	ctxSlot->dwLockId = 0;
	ctxSlot->readerState = NULL;
	ctxSlot->dwIdentity = (slotNumber + 1) << (sizeof(DWORD) / 2) * 8;

	for (ix = 0; ix < PCSCLITE_MAX_READER_CONTEXT_CHANNELS; ix++)
		ctxSlot->psHandles[ix].hCard = 0;

	if (!ctxSlot->pdwMutex)
		ctxSlot->pdwMutex = malloc(sizeof(DWORD));
	if (baseIsThreadSafe)
	{
		ctxSlot->mMutex = malloc(sizeof(PCSCLITE_MUTEX));
		SYS_MutexInit(ctxSlot->mMutex);
		*ctxSlot->pdwMutex = 1;
	}
	else
		*ctxSlot->pdwMutex += 1;
}

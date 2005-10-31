/*
 * Copyright (c) 2004 Apple Computer, Inc. All Rights Reserved.
 * 
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */



#include <Security/Security.h>
#include "DataStorageLibrary.h"



/*
	stubs for CSSM SPI's -- these simply call the next level
*/



extern "C" CSSM_RETURN CSSM_SPI_ModuleUnload (const CSSM_GUID *CssmGuid,
											  const CSSM_GUID *ModuleGuid,
											  CSSM_SPI_ModuleEventHandler CssmNotifyCallback,
											  void* CssmNotifyCallbackCtx)
{
	try
	{
		StaticMutex _mutex (*DataStorageLibrary::gGlobalLock);
		MutexLocker _lock (_mutex);
		
		if (DataStorageLibrary::gDL != NULL)
		{
			// delete our instance
			delete DataStorageLibrary::gDL;
			DataStorageLibrary::gDL = NULL;
		}
		return 0;
	}
	catch (...)
	{
		return CSSMERR_CSSM_INTERNAL_ERROR;
	}
}



extern "C" CSSM_RETURN CSSM_SPI_ModuleAttach (const CSSM_GUID *ModuleGuid,
											  const CSSM_VERSION *Version,
											  uint32 SubserviceID,
											  CSSM_SERVICE_TYPE SubserviceType,
											  CSSM_ATTACH_FLAGS AttachFlags,
											  CSSM_MODULE_HANDLE ModuleHandle,
											  CSSM_KEY_HIERARCHY KeyHierarchy,
											  const CSSM_GUID *CssmGuid,
											  const CSSM_GUID *ModuleManagerGuid,
											  const CSSM_GUID *CallerGuid,
											  const CSSM_UPCALLS *Upcalls,
											  CSSM_MODULE_FUNCS_PTR *FuncTbl)
{
	try
	{
		if (DataStorageLibrary::gDL == NULL)
		{
			return CSSMERR_CSSM_MODULE_NOT_LOADED;
		}
		
		DataStorageLibrary::gDL->Attach (ModuleGuid, Version, SubserviceID, SubserviceType, AttachFlags, ModuleHandle, KeyHierarchy,
										 CssmGuid, ModuleManagerGuid, CallerGuid, Upcalls, FuncTbl);
		return 0;
	}
	catch (...)
	{
		return CSSMERR_CSSM_INTERNAL_ERROR;
	}
}



extern "C" CSSM_RETURN CSSM_SPI_ModuleDetach (CSSM_MODULE_HANDLE ModuleHandle)
{
	try
	{
		if (DataStorageLibrary::gDL == NULL)
		{
			return CSSMERR_CSSM_MODULE_NOT_LOADED;
		}
		
		DataStorageLibrary::gDL->Detach (ModuleHandle);
		return 0;
	}
	catch (...)
	{
		return CSSMERR_CSSM_INTERNAL_ERROR;
	}
}

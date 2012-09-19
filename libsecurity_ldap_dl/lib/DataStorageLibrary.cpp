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



#include "DataStorageLibrary.h"
#include "CommonCode.h"


DataStorageLibrary *DataStorageLibrary::gDL;
pthread_mutex_t *DataStorageLibrary::gGlobalLock;

DataStorageLibrary::DataStorageLibrary (pthread_mutex_t *globalLock,
										CSSM_SPI_ModuleEventHandler eventHandler,
										void* CssmNotifyCallbackCtx)
	: mEventHandler (eventHandler), mCallbackContext (CssmNotifyCallbackCtx)
{
	// retain a global pointer to this library (OK because we only instantiate this object once
	gDL = this;
	gGlobalLock = globalLock;
}



DataStorageLibrary::~DataStorageLibrary ()
{
}



void DataStorageLibrary::Attach (const CSSM_GUID *ModuleGuid,
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
	// make and initialize a new AttachedInstance
	AttachedInstance* ai = MakeAttachedInstance ();
	ai->SetUpcalls (ModuleHandle, Upcalls);
	ai->Initialize (ModuleGuid, Version, SubserviceID, SubserviceType, AttachFlags,
					KeyHierarchy, CssmGuid, ModuleManagerGuid, CallerGuid);
	
	*FuncTbl = AttachedInstance::gFunctionTablePtr;
	
	// map the function to the id
	mInstanceMap[ModuleHandle] = ai;
}



void DataStorageLibrary::Detach (CSSM_MODULE_HANDLE moduleHandle)
{
	MutexLocker m (mInstanceMapMutex);
	AttachedInstance* ai = mInstanceMap[moduleHandle];
	delete ai;
}



AttachedInstance* DataStorageLibrary::HandleToInstance (CSSM_MODULE_HANDLE handle)
{
	MutexLocker _m (mInstanceMapMutex);
	
	InstanceMap::iterator m = mInstanceMap.find (handle);
	if (m == mInstanceMap.end ())
	{
		CSSMError::ThrowCSSMError(CSSMERR_DL_INVALID_DL_HANDLE);
	}

	return m->second;
}

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



#ifndef __DATA_STORAGE_LIBRARY_H__
#define __DATA_STORAGE_LIBRARY_H__



#include <Security/Security.h>
#include "AttachedInstance.h"
#include "Mutex.h"

#include <map>


typedef std::map<CSSM_MODULE_HANDLE, AttachedInstance*> InstanceMap;

// a class which creates, deallocates, and finds attached instances

class DataStorageLibrary
{
protected:

	CSSM_SPI_ModuleEventHandler mEventHandler;
	void* mCallbackContext;
	InstanceMap mInstanceMap;
	DynamicMutex mInstanceMapMutex;

public:
	static DataStorageLibrary* gDL;
	static pthread_mutex_t *gGlobalLock;

	DataStorageLibrary (pthread_mutex_t *globalLock, CSSM_SPI_ModuleEventHandler eventHandler, void* CssmNotifyCallbackCtx);
	virtual ~DataStorageLibrary ();

	void Attach (const CSSM_GUID *ModuleGuid,
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
			     CSSM_MODULE_FUNCS_PTR *FuncTbl);
	
	void Detach (CSSM_MODULE_HANDLE moduleHandle);

	virtual AttachedInstance* MakeAttachedInstance () = 0;

	AttachedInstance* HandleToInstance (CSSM_MODULE_HANDLE handle);
};



#endif

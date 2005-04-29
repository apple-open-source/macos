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

/*
	C interface to C++ bridge
	
	LDAP_DL
*/



#include <Security/Security.h>
#include "Mutex.h"

#include "Database.h"
#include "LDAPDLModule.h"

static pthread_mutex_t gLockMutex = PTHREAD_MUTEX_INITIALIZER;

extern "C" CSSM_RETURN CSSM_SPI_ModuleLoad (const CSSM_GUID *CssmGuid,
											const CSSM_GUID *ModuleGuid,
											CSSM_SPI_ModuleEventHandler CssmNotifyCallback,
											void* CssmNotifyCallbackCtx)
{
	try
	{
		StaticMutex _lockMutex (gLockMutex);
		MutexLocker _lock (_lockMutex);

		// instantiate your Module object here
		if (DataStorageLibrary::gDL == NULL)
		{
			new LDAPDLModule (&gLockMutex, CssmNotifyCallback, CssmNotifyCallbackCtx);
		}
	}
	catch (...)
	{
		return CSSMERR_CSSM_INTERNAL_ERROR;
	}
	
	return 0;
}

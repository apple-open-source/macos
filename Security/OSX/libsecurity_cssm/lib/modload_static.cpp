/*
 * Copyright (c) 2000-2001,2003-2004,2011,2014 Apple Inc. All Rights Reserved.
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


//
// modload_static - pseudo-loading of statically linked plugins
//
#include "modload_static.h"


namespace Security {


//
// Pass module entry points to the statically linked functions
//
CSSM_RETURN StaticPlugin::load(const CSSM_GUID *CssmGuid,
                             const CSSM_GUID *ModuleGuid,
                             CSSM_SPI_ModuleEventHandler CssmNotifyCallback,
                             void *CssmNotifyCallbackCtx)
{
	return entries.load(CssmGuid, ModuleGuid,
		CssmNotifyCallback, CssmNotifyCallbackCtx);
}

CSSM_RETURN StaticPlugin::unload(const CSSM_GUID *CssmGuid,
                             const CSSM_GUID *ModuleGuid,
                             CSSM_SPI_ModuleEventHandler CssmNotifyCallback,
                             void *CssmNotifyCallbackCtx)
{
	return entries.unload(CssmGuid, ModuleGuid,
		CssmNotifyCallback, CssmNotifyCallbackCtx);
}

CSSM_RETURN StaticPlugin::attach(const CSSM_GUID *ModuleGuid,
                               const CSSM_VERSION *Version,
                               uint32 SubserviceID,
                               CSSM_SERVICE_TYPE SubServiceType,
                               CSSM_ATTACH_FLAGS AttachFlags,
                               CSSM_MODULE_HANDLE ModuleHandle,
                               CSSM_KEY_HIERARCHY KeyHierarchy,
                               const CSSM_GUID *CssmGuid,
                               const CSSM_GUID *ModuleManagerGuid,
                               const CSSM_GUID *CallerGuid,
                               const CSSM_UPCALLS *Upcalls,
                               CSSM_MODULE_FUNCS_PTR *FuncTbl)
{
	return entries.attach(ModuleGuid, Version, SubserviceID, SubServiceType,
		AttachFlags, ModuleHandle, KeyHierarchy, CssmGuid, ModuleManagerGuid,
		CallerGuid, Upcalls, FuncTbl);
}

CSSM_RETURN StaticPlugin::detach(CSSM_MODULE_HANDLE ModuleHandle)
{
	return entries.detach(ModuleHandle);
}


}	// end namespace Security

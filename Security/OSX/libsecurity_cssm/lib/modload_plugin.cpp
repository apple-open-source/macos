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
// modload_plugin - loader interface for dynamically loaded plugin modules
//
#include "modload_plugin.h"


namespace Security {


//
// During construction, a LoadablePlugin loads itself into memory and locates
// the canonical (CDSA defined) four entrypoints. If anything fails, we throw.
//    
LoadablePlugin::LoadablePlugin(const char *path) : LoadableBundle(path)
{
	secdebug("cssm", "LoadablePlugin(%s)", path);
    load();
}


//
// Loading and unloading devolves directly onto LoadableBundle
//
void LoadablePlugin::load()
{
	secdebug("cssm", "LoadablePlugin::load() path %s", path().c_str());
    LoadableBundle::load();
    findFunction(mFunctions.load, "CSSM_SPI_ModuleLoad");
    findFunction(mFunctions.attach, "CSSM_SPI_ModuleAttach");
    findFunction(mFunctions.detach, "CSSM_SPI_ModuleDetach");
    findFunction(mFunctions.unload, "CSSM_SPI_ModuleUnload");
}

void LoadablePlugin::unload()
{
	secdebug("cssm", "LoadablePlugin::unload() path %s", path().c_str());
	/* skipping for workaround for radar 3774226 
    LoadableBundle::unload(); */ 
}

bool LoadablePlugin::isLoaded() const
{
    return LoadableBundle::isLoaded();
}


//
// Pass module entry points to the statically linked functions
//
CSSM_RETURN LoadablePlugin::load(const CSSM_GUID *CssmGuid,
                             const CSSM_GUID *ModuleGuid,
                             CSSM_SPI_ModuleEventHandler CssmNotifyCallback,
                             void *CssmNotifyCallbackCtx)
{
	secdebug("cssm", "LoadablePlugin::load(guid,...) path %s", path().c_str());
	return mFunctions.load(CssmGuid, ModuleGuid,
		CssmNotifyCallback, CssmNotifyCallbackCtx);
}

CSSM_RETURN LoadablePlugin::unload(const CSSM_GUID *CssmGuid,
                             const CSSM_GUID *ModuleGuid,
                             CSSM_SPI_ModuleEventHandler CssmNotifyCallback,
                             void *CssmNotifyCallbackCtx)
{
	secdebug("cssm", "LoadablePlugin::unload(guid,...) path %s", path().c_str());
	return mFunctions.unload(CssmGuid, ModuleGuid,
		CssmNotifyCallback, CssmNotifyCallbackCtx);
}

CSSM_RETURN LoadablePlugin::attach(const CSSM_GUID *ModuleGuid,
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
	return mFunctions.attach(ModuleGuid, Version, SubserviceID, SubServiceType,
		AttachFlags, ModuleHandle, KeyHierarchy, CssmGuid, ModuleManagerGuid,
		CallerGuid, Upcalls, FuncTbl);
}

CSSM_RETURN LoadablePlugin::detach(CSSM_MODULE_HANDLE ModuleHandle)
{
	return mFunctions.detach(ModuleHandle);
}


}	// end namespace Security

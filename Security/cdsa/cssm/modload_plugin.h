/*
 * Copyright (c) 2000-2001 Apple Computer, Inc. All Rights Reserved.
 * 
 * The contents of this file constitute Original Code as defined in and are
 * subject to the Apple Public Source License Version 1.2 (the 'License').
 * You may not use this file except in compliance with the License. Please obtain
 * a copy of the License at http://www.apple.com/publicsource and read it before
 * using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESS
 * OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES, INCLUDING WITHOUT
 * LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. Please see the License for the
 * specific language governing rights and limitations under the License.
 */


//
// modload_plugin - loader interface for dynamically loaded plugin modules
//
#ifndef _H_MODLOAD_PLUGIN
#define _H_MODLOAD_PLUGIN

#include "modloader.h"


namespace Security {


//
// A LoadablePlugin implements itself as a LoadableBundle
//
class LoadablePlugin : public Plugin, public CodeSigning::LoadableBundle {
public:
    LoadablePlugin(const char *path);
    
    void load();
    void unload();
    bool isLoaded() const;
    
    CSSM_RETURN CSSM_SPI_ModuleLoad (const CSSM_GUID *CssmGuid,
        const CSSM_GUID *ModuleGuid,
        CSSM_SPI_ModuleEventHandler CssmNotifyCallback,
        void *CssmNotifyCallbackCtx)
    { return fLoad(CssmGuid, ModuleGuid, CssmNotifyCallback, CssmNotifyCallbackCtx); }

	CSSM_RETURN CSSM_SPI_ModuleUnload (const CSSM_GUID *CssmGuid,
        const CSSM_GUID *ModuleGuid,
        CSSM_SPI_ModuleEventHandler CssmNotifyCallback,
        void *CssmNotifyCallbackCtx)
    { return fUnload(CssmGuid, ModuleGuid, CssmNotifyCallback, CssmNotifyCallbackCtx); }

	CSSM_RETURN CSSM_SPI_ModuleAttach (const CSSM_GUID *ModuleGuid,
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
        return fAttach(ModuleGuid, Version, SubserviceID, SubServiceType,
            AttachFlags, ModuleHandle, KeyHierarchy, CssmGuid,
            ModuleManagerGuid, CallerGuid, Upcalls, FuncTbl);
    }

	CSSM_RETURN CSSM_SPI_ModuleDetach (CSSM_MODULE_HANDLE ModuleHandle)
    { return fDetach(ModuleHandle); }
    
private:
    CSSM_SPI_ModuleLoadFunction *fLoad;
    CSSM_SPI_ModuleAttachFunction *fAttach;
    CSSM_SPI_ModuleDetachFunction *fDetach;
    CSSM_SPI_ModuleUnloadFunction *fUnload;

    template <class FunctionType>
    void findFunction(FunctionType * &func, const char *name)
    { func = (FunctionType *)lookupSymbol(name); }
};


} // end namespace Security


#endif //_H_MODLOAD_PLUGIN

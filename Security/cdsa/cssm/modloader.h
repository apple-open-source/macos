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
// modloader.h - CSSM module loader interface
//
// This is a thin abstraction of plugin module loading/handling for CSSM.
// The resulting module ("Plugin") notion is specific to CSSM plugin modules.
// This implementation uses MacOS X bundles.
//
#ifndef _H_MODLOADER
#define _H_MODLOADER

#include <exception>
#include <Security/utilities.h>
#include <Security/osxsigning.h>
#include <Security/cssmint.h>
#include <map>
#include <string>


namespace Security {


//
// An abstract representation of a loadable plugin.
// Note that "loadable" doesn't mean that actual code loading
// is necessarily happening, but let's just assume it might.
//
class Plugin {
    NOCOPY(Plugin)
public:
    Plugin() { }
    virtual ~Plugin() { }

    virtual void load() = 0;
    virtual void unload() = 0;
    virtual bool isLoaded() const = 0;
    
    virtual CSSM_RETURN CSSM_SPI_ModuleLoad (const CSSM_GUID *CssmGuid,
        const CSSM_GUID *ModuleGuid,
        CSSM_SPI_ModuleEventHandler CssmNotifyCallback,
        void *CssmNotifyCallbackCtx) = 0;
	virtual CSSM_RETURN CSSM_SPI_ModuleUnload (const CSSM_GUID *CssmGuid,
        const CSSM_GUID *ModuleGuid,
        CSSM_SPI_ModuleEventHandler CssmNotifyCallback,
        void *CssmNotifyCallbackCtx) = 0;
	virtual CSSM_RETURN CSSM_SPI_ModuleAttach (const CSSM_GUID *ModuleGuid,
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
        CSSM_MODULE_FUNCS_PTR *FuncTbl) = 0;
	virtual CSSM_RETURN CSSM_SPI_ModuleDetach (CSSM_MODULE_HANDLE ModuleHandle) = 0;
};


//
// The supervisor class that manages searching and loading.
//
class ModuleLoader {
    NOCOPY(ModuleLoader)
public:
    ModuleLoader();
    
    Plugin *operator () (const char *path);
        
private:
    // the table of all loaded modules
    typedef map<string, Plugin *> PluginTable;
    PluginTable mPlugins;
};



} // end namespace Security


#endif //_H_MODLOADER

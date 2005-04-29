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
// pluginspi - "roof" level entry points into a CSSM plugin.
//
// This file is meant to be included into the top-level source file
// for a CSSM plugin written to the C++ alternate interface.
// It contains actual code that defines the four required entry points.
//
#include <security_cdsa_utilities/cssmbridge.h>


//
// Provide some flexibility for the includer
//
#if !defined(SPIPREFIX)
# define SPIPREFIX	extern "C" CSSMSPI
#endif

#if !defined(SPINAME)
# define SPINAME(s) s
#endif


SPIPREFIX CSSM_RETURN SPINAME(CSSM_SPI_ModuleLoad) (const CSSM_GUID *CssmGuid,
    const CSSM_GUID *ModuleGuid,
    CSSM_SPI_ModuleEventHandler CssmNotifyCallback,
    void *CssmNotifyCallbackCtx)
{
    BEGIN_API
    plugin().moduleLoad(Guid::required(CssmGuid),
        Guid::required(ModuleGuid),
        ModuleCallback(CssmNotifyCallback, CssmNotifyCallbackCtx));
    END_API(CSSM)
}

SPIPREFIX CSSM_RETURN SPINAME(CSSM_SPI_ModuleUnload) (const CSSM_GUID *CssmGuid,
    const CSSM_GUID *ModuleGuid,
    CSSM_SPI_ModuleEventHandler CssmNotifyCallback,
    void *CssmNotifyCallbackCtx)
{
    BEGIN_API
    plugin().moduleUnload(Guid::required(CssmGuid),
        Guid::required(ModuleGuid),
        ModuleCallback(CssmNotifyCallback, CssmNotifyCallbackCtx));
    END_API(CSSM)
}

SPIPREFIX CSSM_RETURN SPINAME(CSSM_SPI_ModuleAttach) (const CSSM_GUID *ModuleGuid,
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
    BEGIN_API
    plugin().moduleAttach(ModuleHandle,
        Guid::required(CssmGuid),
        Guid::required(ModuleGuid),
        Guid::required(ModuleManagerGuid),
        Guid::required(CallerGuid),
        *Version,
        SubserviceID,
        SubServiceType,
        AttachFlags,
        KeyHierarchy,
        Required(Upcalls),
        Required(FuncTbl));
    END_API(CSSM)
}

SPIPREFIX CSSM_RETURN SPINAME(CSSM_SPI_ModuleDetach) (CSSM_MODULE_HANDLE ModuleHandle)
{
    BEGIN_API
    plugin().moduleDetach(ModuleHandle);
    END_API(CSSM)
}

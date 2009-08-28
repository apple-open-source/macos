/*
 * Copyright (c) 2000-2001,2003-2004 Apple Computer, Inc. All Rights Reserved.
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
// This file contains the core CSSM public functions.
// Note that hardly anything happens in here; we just hand off requests
// to the various objects representing CSSM state.
//
#include "manager.h"
#include "module.h"
#include <security_utilities/globalizer.h>
#include <security_cdsa_utilities/cssmbridge.h>


//
// We currently use exactly one instance of CssmManager.
//
static ModuleNexus<CssmManager> gManager;


//
// Public API: (Re)Intiailize CSSM
//
CSSM_RETURN CSSMAPI
CSSM_Init (const CSSM_VERSION *Version,
           CSSM_PRIVILEGE_SCOPE Scope,
           const CSSM_GUID *CallerGuid,
           CSSM_KEY_HIERARCHY KeyHierarchy,
           CSSM_PVC_MODE *PvcPolicy,
           const void *)
{
    BEGIN_API
    gManager().initialize(Required(Version),
                        Scope,
                        Guid::required(CallerGuid),
                        KeyHierarchy,
                        Required(PvcPolicy));
    END_API(CSSM)
}


//
// Public API: Terminate CSSM.
//
CSSM_RETURN CSSMAPI
CSSM_Terminate (void)
{
    BEGIN_API
    gManager().terminate();
    END_API(CSSM)
}


//
// Public API: Load a CSSM module
//
CSSM_RETURN CSSMAPI
CSSM_ModuleLoad (const CSSM_GUID *ModuleGuid,
                 CSSM_KEY_HIERARCHY KeyHierarchy,
                 CSSM_API_ModuleEventHandler AppNotifyCallback,
                 void *AppNotifyCallbackCtx)
{
    BEGIN_API
    gManager().loadModule(Guid::required(ModuleGuid),
                            KeyHierarchy,
                            ModuleCallback(AppNotifyCallback, AppNotifyCallbackCtx));
    END_API(CSSM)
}


//
// Public API: Unload a module
//
CSSM_RETURN CSSMAPI
CSSM_ModuleUnload (const CSSM_GUID *ModuleGuid,
                   CSSM_API_ModuleEventHandler AppNotifyCallback,
                   void *AppNotifyCallbackCtx)
{
    BEGIN_API
    gManager().unloadModule(Guid::required(ModuleGuid),
                              ModuleCallback(AppNotifyCallback, AppNotifyCallbackCtx));
    END_API(CSSM)
}


CSSM_RETURN CSSMAPI
CSSM_Introduce (const CSSM_GUID *ModuleID,
                CSSM_KEY_HIERARCHY KeyHierarchy)
{
    BEGIN_API
    gManager().introduce(Guid::required(ModuleID), KeyHierarchy);
    END_API(CSSM)
}

CSSM_RETURN CSSMAPI
CSSM_Unintroduce (const CSSM_GUID *ModuleID)
{
    BEGIN_API
    gManager().unIntroduce(Guid::required(ModuleID));
    END_API(CSSM)
}


CSSM_RETURN CSSMAPI
CSSM_ModuleAttach (const CSSM_GUID *ModuleGuid,
                   const CSSM_VERSION *Version,
                   const CSSM_API_MEMORY_FUNCS *MemoryFuncs,
                   uint32 SubserviceID,
                   CSSM_SERVICE_TYPE SubServiceType,
                   CSSM_ATTACH_FLAGS AttachFlags,
                   CSSM_KEY_HIERARCHY KeyHierarchy,
                   CSSM_FUNC_NAME_ADDR *FunctionTable,
                   uint32 NumFunctionTable,
                   const void *,
                   CSSM_MODULE_HANDLE_PTR NewModuleHandle)
{
    BEGIN_API
    Required(NewModuleHandle) = gManager().getModule(Guid::required(ModuleGuid))->attach(
                                  Required(Version),
                                  SubserviceID, SubServiceType,
                                  Required(MemoryFuncs),
                                  AttachFlags,
                                  KeyHierarchy,
                                  FunctionTable, NumFunctionTable
                                  );
    END_API(CSSM)
}

CSSM_RETURN CSSMAPI
CSSM_ModuleDetach (CSSM_MODULE_HANDLE ModuleHandle)
{
    BEGIN_API
    Attachment *attachment = &HandleObject::findAndKill<Attachment>(ModuleHandle, CSSMERR_CSSM_INVALID_ADDIN_HANDLE);
    attachment->detach(true);		// expect locked, will unlock
    // the attachment is now off the maps, known idle, and unhooked
    delete attachment;
    END_API(CSSM)
}


CSSM_RETURN CSSMAPI
CSSM_SetPrivilege (CSSM_PRIVILEGE Privilege)
{
    BEGIN_API
    gManager().setPrivilege(Privilege);
    END_API(CSSM)
}

CSSM_RETURN CSSMAPI
CSSM_GetPrivilege (CSSM_PRIVILEGE *Privilege)
{
    BEGIN_API
    Required(Privilege) = gManager().getPrivilege();
    END_API(CSSM)
}


CSSM_RETURN CSSMAPI
CSSM_GetModuleGUIDFromHandle (CSSM_MODULE_HANDLE ModuleHandle,
                              CSSM_GUID_PTR ModuleGUID)
{
    BEGIN_API
    Attachment &attachment = HandleObject::findAndLock<Attachment>(ModuleHandle, CSSMERR_CSSM_INVALID_ADDIN_HANDLE);
    StLock<Mutex> _(attachment, true);
    Required(ModuleGUID) = attachment.module.myGuid();
    END_API(CSSM)
}

CSSM_RETURN CSSMAPI
CSSM_GetSubserviceUIDFromHandle (CSSM_MODULE_HANDLE ModuleHandle,
                                 CSSM_SUBSERVICE_UID_PTR SubserviceUID)
{
    BEGIN_API
    Attachment &attachment = HandleObject::findAndLock<Attachment>(ModuleHandle, CSSMERR_CSSM_INVALID_ADDIN_HANDLE);
    StLock<Mutex> _(attachment, true);
    CSSM_SUBSERVICE_UID &result =  Required(SubserviceUID);
    result.Guid = attachment.module.myGuid();
    result.Version = attachment.pluginVersion();
    result.SubserviceId = attachment.subserviceId();
    result.SubserviceType = attachment.subserviceType();
    END_API(CSSM)
}


CSSM_RETURN CSSMAPI
CSSM_ListAttachedModuleManagers (uint32 *NumberOfModuleManagers,
                                 CSSM_GUID_PTR)
{
    BEGIN_API
    *NumberOfModuleManagers = 0;    // EMMs not implemented
    END_API(CSSM)
}

CSSM_RETURN CSSMAPI
CSSM_GetAPIMemoryFunctions (CSSM_MODULE_HANDLE AddInHandle,
                            CSSM_API_MEMORY_FUNCS_PTR AppMemoryFuncs)
{
    BEGIN_API
    Attachment &attachment = HandleObject::findAndLock<Attachment>(AddInHandle, CSSMERR_CSSM_INVALID_ADDIN_HANDLE);
    StLock<Mutex> _(attachment, true);
    Required(AppMemoryFuncs) = attachment;
    END_API(CSSM)
}

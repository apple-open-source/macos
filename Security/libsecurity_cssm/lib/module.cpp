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
// module - CSSM Module objects
//
#include "module.h"
#include "manager.h"
#include "attachment.h"
#include <security_cdsa_utilities/cssmbridge.h>


//
// Module object construction.
//
Module::Module(CssmManager *mgr, const MdsComponent &info, Plugin *plug)
: MdsComponent(info), cssm(*mgr), plugin(plug)
{
    // invoke module's load entry (tell it it's being loaded)
    if (CSSM_RETURN err = plugin->load(&gGuidCssm, // CSSM's Guid
            &myGuid(),			// module's Guid
            spiEventRelay, this)) {
        plugin->unload();
        CssmError::throwMe(err);	// self-destruct this module
    }
}


//
// Destroy the module object.
// The unload() method must have succeeded and returned true before
// you get to delete a Module. A destructor is too precarious a place
// to negotiate with a plugin...
//
Module::~Module()
{
}


bool Module::unload(const ModuleCallback &callback)
{
    StLock<Mutex> _(mLock);
    // locked module - no more attachment creations possible
    if (callbackCount() == 1) {
        // would be last callback if successful, check for actual unload
        if (attachmentCount() > 0)
            CssmError::throwMe(CSSM_ERRCODE_FUNCTION_FAILED);	// @# module is busy
        // no attachments active - we are idle and ready to unload
        if (CSSM_RETURN err = plugin->unload(&gGuidCssm, // CSSM's Guid
                &myGuid(),		// module's Guid
                spiEventRelay, this)) // our callback
            CssmError::throwMe(err);	// tough...
        // okay, commit
        remove(callback);
        plugin->unload();
        return true;
    } else {
        // more callbacks - we're not going to unload
        remove(callback);
        return false;
    }
}


//
// Create a new attachment for this module
//
CSSM_HANDLE Module::attach(const CSSM_VERSION &version,
                           uint32 subserviceId,
                           CSSM_SERVICE_TYPE subserviceType,
                           const CSSM_API_MEMORY_FUNCS &memoryOps,
                           CSSM_ATTACH_FLAGS attachFlags,
                           CSSM_KEY_HIERARCHY keyHierarchy,
                           CSSM_FUNC_NAME_ADDR *functionTable,
                           uint32 functionTableSize)
{
    StLock<Mutex> _(mLock);
    
    // check if the module can do this kind of service
    if (!supportsService(subserviceType))
        CssmError::throwMe(CSSMERR_CSSM_INVALID_SERVICE_MASK);

    Attachment *attachment = cssm.attachmentMakerFor(subserviceType)->make(this,
                                   version,
                                   subserviceId, subserviceType,
                                   memoryOps,
                                   attachFlags,
                                   keyHierarchy,
                                   functionTable, functionTableSize);

    try {
        // add to module's attachment map
        attachmentMap.insert(AttachmentMap::value_type(attachment->handle(), attachment));
    } catch (...) {
        delete attachment;
        throw;
    }

    // all done
    return attachment->handle();
}


//
// Detach an Attachment from this module.
// THREADS: Requires the attachment to be idled out, i.e. caller
//  is responsible for keeping more users from entering it.
//
void Module::detach(Attachment *attachment)
{
    StLock<Mutex> _(mLock);
    attachmentMap.erase(attachment->handle());
}


//
// Handle events sent by the loaded module.
//
void Module::spiEvent(CSSM_MODULE_EVENT event,
                      const Guid &guid,
                      uint32 subserviceId,
                      CSSM_SERVICE_TYPE serviceType)
{
    StLock<Mutex> _(mLock);
    if (guid != myGuid())
        CssmError::throwMe(CSSM_ERRCODE_INTERNAL_ERROR);
    callbackSet(event, guid, subserviceId, serviceType);
}

// static shim
CSSM_RETURN Module::spiEventRelay(const CSSM_GUID *ModuleGuid,
                                   void *Context,
                                   uint32 SubserviceId,
                                   CSSM_SERVICE_TYPE ServiceType,
                                   CSSM_MODULE_EVENT EventType)
{
    BEGIN_API
    static_cast<Module *>(Context)->spiEvent(EventType,
                                             Guid::required(ModuleGuid),
                                             SubserviceId,
                                             ServiceType);
    END_API(CSSM)
}

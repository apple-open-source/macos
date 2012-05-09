/*
 * Copyright (c) 2000-2004 Apple Computer, Inc. All Rights Reserved.
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
// attachment - CSSM module attachment objects
//
#include "attachment.h"
#include "module.h"
#include "manager.h"
#include "cssmcontext.h"
#include <security_cdsa_utilities/cssmbridge.h>

//
// Construct an Attachment object.
// This constructor does almost all the work: it initializes the Attachment
// object, calls the plugin's attach function, and initializes everything.
// The only job left for the subclass's constructor is to take the spiFunctionTable
// field and extract from it the plugin's dispatch table in suitable form.
//
Attachment::Attachment(Module *parent,
                       const CSSM_VERSION &version,
                       uint32 ssId,
                       CSSM_SERVICE_TYPE ssType,
                       const CSSM_API_MEMORY_FUNCS &memoryOps,
                       CSSM_ATTACH_FLAGS attachFlags,
                       CSSM_KEY_HIERARCHY keyHierarchy)
	: CssmMemoryFunctionsAllocator(memoryOps), module(*parent)
{
    // record our origins
    mVersion = version;
    mSubserviceId = ssId;
    mSubserviceType = ssType;
    mAttachFlags = attachFlags;
    mKeyHierarchy = keyHierarchy;

    // we are not (yet) attached to our plugin
    mIsActive = false;
    
    // build the upcalls table
    // (we could do this once in a static, but then we'd have to lock on it)
    upcalls.malloc_func = upcallMalloc;
    upcalls.free_func = upcallFree;
    upcalls.realloc_func = upcallRealloc;
    upcalls.calloc_func = upcallCalloc;
    upcalls.CcToHandle_func = upcallCcToHandle;
    upcalls.GetModuleInfo_func = upcallGetModuleInfo;

    // tell the module to create an attachment
    spiFunctionTable = NULL;	// preset invalid
    if (CSSM_RETURN err = module.plugin->attach(&module.myGuid(),
            &mVersion,
            mSubserviceId,
            mSubserviceType,
            mAttachFlags,
            handle(),
            mKeyHierarchy,
            &gGuidCssm,			// CSSM's Guid
            &gGuidCssm,			// module manager Guid
            &module.cssm.callerGuid(), // caller Guid
            &upcalls,
            &spiFunctionTable)) {
        // attach rejected by module
		secdebug("cssm", "attach of module %p(%s) failed",
			&module, module.name().c_str());
        CssmError::throwMe(err);
    }
    try {
        if (spiFunctionTable == NULL || spiFunctionTable->ServiceType != subserviceType())
            CssmError::throwMe(CSSMERR_CSSM_INVALID_ADDIN_FUNCTION_TABLE);
        mIsActive = true;	// now officially attached to plugin
		secdebug("cssm", "%p attached module %p(%s) (ssid %ld type %ld)",
			this, parent, parent->name().c_str(), (long)ssId, (long)ssType);
        // subclass is responsible for taking spiFunctionTable and build
        // whatever dispatch is needed
    } catch (...) {
        module.plugin->detach(handle());	// with extreme prejudice
        throw;
    }
}


//
// Detach an attachment.
// This is the polite way to detach from the plugin. It may be refused safely
// (though perhaps not meaningfully).
// THREADS: mLock is locked on entry IFF isLocked, and will be unlocked on exit.
//
void Attachment::detach(bool isLocked)
{
    StLock<Mutex> locker(*this, isLocked);	// pre-state locker
	locker.lock();	// make sure it's locked

    if (mIsActive) {
        if (!isIdle())
            CssmError::throwMe(CSSM_ERRCODE_FUNCTION_FAILED);	//@#attachment busy
        if (CSSM_RETURN error = module.plugin->detach(handle()))
			CssmError::throwMe(error);	// I'm sorry Dave, ...
		secdebug("cssm", "%p detach module %p(%s)", this,
			&module, module.name().c_str());
        mIsActive = false;
        module.detach(this);
    }
}


//
// Destroy the Attachment object
//
Attachment::~Attachment()
{
    try {
        detach(false);
    } catch (...) {
        // too bad - you're dead
    }
}


//
// Upcall relays.
// These do not lock the attachment object. The attachment can't go away
// because we incremented the busy count on entry to the plugin; and these
// fields are quite constant for the life of the Attachment.
//
void *Attachment::upcallMalloc(CSSM_HANDLE handle, size_t size)
{
    BEGIN_API
    return HandleObject::find<Attachment>(handle, CSSMERR_CSSM_INVALID_ADDIN_HANDLE).malloc(size);
    END_API1(NULL)
}

void Attachment::upcallFree(CSSM_HANDLE handle, void *mem)
{
    BEGIN_API
    return HandleObject::find<Attachment>(handle, CSSMERR_CSSM_INVALID_ADDIN_HANDLE).free(mem);
    END_API0
}

void *Attachment::upcallRealloc(CSSM_HANDLE handle, void *mem, size_t size)
{
    BEGIN_API
    return HandleObject::find<Attachment>(handle, CSSMERR_CSSM_INVALID_ADDIN_HANDLE).realloc(mem, size);
    END_API1(NULL)
}

void *Attachment::upcallCalloc(CSSM_HANDLE handle, size_t num, size_t size)
{
    BEGIN_API
    return HandleObject::find<Attachment>(handle, CSSMERR_CSSM_INVALID_ADDIN_HANDLE).calloc(num, size);
    END_API1(NULL)
}

CSSM_RETURN Attachment::upcallCcToHandle(CSSM_CC_HANDLE handle,
                                         CSSM_MODULE_HANDLE *modHandle)
{
    BEGIN_API
    Required(modHandle) = HandleObject::find<HandleContext>(handle, CSSMERR_CSSM_INVALID_ADDIN_HANDLE).attachment.handle();
    END_API(CSP)
}

CSSM_RETURN Attachment::upcallGetModuleInfo(CSSM_MODULE_HANDLE handle,
                                            CSSM_GUID_PTR guid,
                                            CSSM_VERSION_PTR version,
                                            uint32 *subserviceId,
                                            CSSM_SERVICE_TYPE *subserviceType,
                                            CSSM_ATTACH_FLAGS *attachFlags,
                                            CSSM_KEY_HIERARCHY *keyHierarchy,
                                            CSSM_API_MEMORY_FUNCS_PTR memoryOps,
                                            CSSM_FUNC_NAME_ADDR_PTR FunctionTable,
                                            uint32 NumFunctions)
{
    BEGIN_API
    Attachment &attachment = HandleObject::find<Attachment>(handle, CSSMERR_CSSM_INVALID_ADDIN_HANDLE);
    Required(guid) = attachment.myGuid();
    Required(version) = attachment.mVersion;
    Required(subserviceId) = attachment.mSubserviceId;
    Required(subserviceType) = attachment.mSubserviceType;
    Required(attachFlags) = attachment.mAttachFlags;
    Required(keyHierarchy) = attachment.mKeyHierarchy;
    Required(memoryOps) = attachment;
    if (FunctionTable)
        attachment.resolveSymbols(FunctionTable, NumFunctions);
    END_API(CSSM)
}

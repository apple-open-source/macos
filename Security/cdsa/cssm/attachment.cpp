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
// attachment - CSSM module attachment objects
//
#ifdef __MWERKS__
#define _CPP_ATTACHMENT
#endif
#include "attachment.h"
#include "module.h"
#include "manager.h"
#include "cssmcontext.h"

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
    if (CSSM_RETURN err = module.plugin->CSSM_SPI_ModuleAttach(&module.myGuid(),
            &mVersion,
            mSubserviceId,
            mSubserviceType,
            mAttachFlags,
            handle(),
            mKeyHierarchy,
            &module.cssm.myGuid(),	// CSSM's Guid
            &module.cssm.myGuid(),	// module manager Guid
            &module.cssm.callerGuid(), // caller Guid
            &upcalls,
            &spiFunctionTable)) {
        // attach rejected by module
        CssmError::throwMe(err);
    }
    try {
        if (spiFunctionTable == NULL || spiFunctionTable->ServiceType != subserviceType())
            CssmError::throwMe(CSSMERR_CSSM_INVALID_ADDIN_FUNCTION_TABLE);
        mIsActive = true;	// now officially attached to plugin
        // subclass is responsible for taking spiFunctionTable and build
        // whatever dispatch is needed
    } catch (...) {
        module.plugin->CSSM_SPI_ModuleDetach(handle());	// with extreme prejudice
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
        if (CSSM_RETURN error = module.plugin->CSSM_SPI_ModuleDetach(handle()))
			CssmError::throwMe(error);	// I'm sorry Dave, ...
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
void *Attachment::upcallMalloc(CSSM_HANDLE handle, uint32 size)
{
    BEGIN_API
    return findHandle<Attachment>(handle).malloc(size);
    END_API1(NULL)
}

void Attachment::upcallFree(CSSM_HANDLE handle, void *mem)
{
    BEGIN_API
    return findHandle<Attachment>(handle).free(mem);
    END_API0
}

void *Attachment::upcallRealloc(CSSM_HANDLE handle, void *mem, uint32 size)
{
    BEGIN_API
    return findHandle<Attachment>(handle).realloc(mem, size);
    END_API1(NULL)
}

void *Attachment::upcallCalloc(CSSM_HANDLE handle, uint32 num, uint32 size)
{
    BEGIN_API
    return findHandle<Attachment>(handle).calloc(num, size);
    END_API1(NULL)
}

CSSM_RETURN Attachment::upcallCcToHandle(CSSM_CC_HANDLE handle,
                                         CSSM_MODULE_HANDLE *modHandle)
{
    BEGIN_API
    Required(modHandle) = findHandle<HandleContext>(handle).attachment.handle();
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
    Attachment &attachment = findHandle<Attachment>(handle);
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

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
// pluginsession - an attachment session for a CSSM plugin
//
#ifdef __MWERKS__
#define _CPP_PLUGINSESSION
#endif
#include <Security/pluginsession.h>
#include <Security/cssmplugin.h>
#include <Security/DLsession.h>


//
// Construct the PluginSession base object.
//
PluginSession::PluginSession(CSSM_MODULE_HANDLE theHandle,
                             CssmPlugin &plug,
                             const CSSM_VERSION &version,
                             uint32 subserviceId,
                             CSSM_SERVICE_TYPE subserviceType,
                             CSSM_ATTACH_FLAGS attachFlags,
                             const CSSM_UPCALLS &inUpcalls)
: HandledObject(theHandle), plugin(plug), upcalls(inUpcalls)
{
    // fill in passed flags
    mVersion = version;
    mSubserviceId = subserviceId;
    mSubserviceType = subserviceType;
    mAttachFlags = attachFlags;
}

PluginSession::~PluginSession()
{
}

void PluginSession::detach()
{
}


//
// Allocation management
//
void *PluginSession::malloc(size_t size) throw(std::bad_alloc)
{
    if (void *addr = upcalls.malloc_func(handle(), size))
        return addr;
    CssmError::throwMe(CSSM_ERRCODE_MEMORY_ERROR);
}

void *PluginSession::realloc(void *oldAddr, size_t size) throw(std::bad_alloc)
{
    if (void *addr = upcalls.realloc_func(handle(), oldAddr, size))
        return addr;
    CssmError::throwMe(CSSM_ERRCODE_MEMORY_ERROR);
}


//
// Dispatch events through the plugin module object.
// Subsystem ID and subservice type default to our own.
//

void PluginSession::sendCallback(CSSM_MODULE_EVENT event,
                                 uint32 subId,
                                 CSSM_SERVICE_TYPE serviceType) const
{
    plugin.sendCallback(event,
                        (subId == uint32(-1)) ? mSubserviceId : subId,
                        serviceType ? serviceType : mSubserviceType);
}

/*
 * Copyright (c) 2000-2001,2011,2014 Apple Inc. All Rights Reserved.
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
// DLsession - Plugin framework for CSP plugin modules
//
#ifdef __MWERKS__
#define _CPP_DLSESSION
#endif

#include <security_cdsa_plugin/DLsession.h>
#include <security_cdsa_plugin/cssmplugin.h>


//
// Construct a DLPluginSession
//
DLPluginSession::DLPluginSession(CSSM_MODULE_HANDLE theHandle,
                                 CssmPlugin &plug,
                                 const CSSM_VERSION &version,
                                 uint32 subserviceId,
                                 CSSM_SERVICE_TYPE subserviceType,
                                 CSSM_ATTACH_FLAGS attachFlags,
                                 const CSSM_UPCALLS &upcalls,
                                 DatabaseManager &databaseManager)
  : PluginSession(theHandle, plug, version, subserviceId, subserviceType, attachFlags, upcalls),
    DatabaseSession (databaseManager)
{
}


//
// Implement Allocator methods from the PluginSession side
//
void *DLPluginSession::malloc(size_t size) throw(std::bad_alloc)
{ return PluginSession::malloc(size); }

void DLPluginSession::free(void *addr) throw()
{ return PluginSession::free(addr); }

void *DLPluginSession::realloc(void *addr, size_t size) throw(std::bad_alloc)
{ return PluginSession::realloc(addr, size); }

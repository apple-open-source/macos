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
// cssmplugin - adapter framework for C++-based CDSA plugin modules
//
// A note on locking: Attachments are effectively reference counted in CSSM.
// CSSM will not let a client detach an attachment that has a(nother) thread
// active in its code. Thus, our locks merely protect global maps; they do not
// need (or try) to close the classic use-and-delete window.
//
#ifdef __MWERKS__
#define _CPP_CSSMPLUGIN
#endif
#include <Security/cssmplugin.h>
#include <Security/pluginsession.h>


ModuleNexus<CssmPlugin::SessionMap> CssmPlugin::sessionMap;


CssmPlugin::CssmPlugin()
{
    haveCallback = false;
}

CssmPlugin::~CssmPlugin()
{
	// Note: if haveCallback, we're being unloaded forcibly.
	// (CSSM wouldn't do this to us in normal operation.)
}


void CssmPlugin::moduleLoad(const Guid &cssmGuid,
                const Guid &moduleGuid,
                const ModuleCallback &newCallback)
{
    // add the callback vector
    if (haveCallback)	// re-entering moduleLoad - not currently supported
        CssmError::throwMe(CSSM_ERRCODE_INTERNAL_ERROR);
        
	mMyGuid = moduleGuid;

    // let the implementation know that we're loading
    load();

    // commit
    callback = newCallback;
    haveCallback = true;
}


void CssmPlugin::moduleUnload(const Guid &cssmGuid,
                  const Guid &moduleGuid,
                      const ModuleCallback &oldCallback)
{
    // check the callback vector
    if (!haveCallback || oldCallback != callback)
        CssmError::throwMe(CSSM_ERRCODE_INTERNAL_ERROR);

    // tell our subclass that we're closing down
    unload();

    // commit closure
    haveCallback = false;
}


void CssmPlugin::moduleAttach(CSSM_MODULE_HANDLE theHandle,
                              const Guid &newCssmGuid,
                              const Guid &moduleGuid,
                              const Guid &moduleManagerGuid,
                              const Guid &callerGuid,
                              const CSSM_VERSION &version,
                              uint32 subserviceId,
                              CSSM_SERVICE_TYPE subserviceType,
                              CSSM_ATTACH_FLAGS attachFlags,
                              CSSM_KEY_HIERARCHY keyHierarchy,
                              const CSSM_UPCALLS &upcalls,
                              CSSM_MODULE_FUNCS_PTR &funcTbl)
{
    // insanity checks
    // @@@ later
    
    // make the new session object, hanging in thin air
    PluginSession *session = makeSession(theHandle,
                                         version,
                                         subserviceId, subserviceType,
                                         attachFlags,
                                         upcalls);

    try {
        // haggle with the implementor
        funcTbl = session->construct();

        // commit this session creation
        StLock<Mutex> _(sessionMap());
		sessionMap()[theHandle] = session;
    } catch (...) {
        delete session;
        throw;
    }
}

void CssmPlugin::moduleDetach(CSSM_MODULE_HANDLE handle)
{
	// locate the plugin and hold the sessionMapLock
	PluginSession *session;
	{
		StLock<Mutex> _(sessionMap());
		SessionMap::iterator it = sessionMap().find(handle);
		if (it == sessionMap().end())
			CssmError::throwMe(CSSMERR_CSSM_INVALID_ADDIN_HANDLE);
		session = it->second;
		sessionMap().erase(it);
	}
		
	// let the session know it is going away
	try {
		session->detach();
	} catch (...) {
		// session detach failed - put the plugin back and fail
		StLock<Mutex> _(sessionMap());
		sessionMap()[handle] = session;
		throw;
	}
	
	// everything's fine, delete the session
	delete session;
}

void CssmPlugin::sendCallback(CSSM_MODULE_EVENT event, uint32 subId,
                     		  CSSM_SERVICE_TYPE serviceType) const
{
	assert(haveCallback);
	callback(event, mMyGuid, subId, serviceType);
}


//
// Default subclass hooks.
// The default implementations succeed without doing anything
//
void CssmPlugin::load() { }

void CssmPlugin::unload() { }

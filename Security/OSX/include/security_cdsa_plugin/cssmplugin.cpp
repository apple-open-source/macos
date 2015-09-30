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
// cssmplugin - adapter framework for C++-based CDSA plugin modules
//
// A note on locking: Attachments are effectively reference counted in CSSM.
// CSSM will not let a client detach an attachment that has a(nother) thread
// active in its code. Thus, our locks merely protect global maps; they do not
// need (or try) to close the classic use-and-delete window.
//
#include <security_cdsa_plugin/cssmplugin.h>
#include <security_cdsa_plugin/pluginsession.h>
#include <memory>


ModuleNexus<CssmPlugin::SessionMap> CssmPlugin::sessionMap;


CssmPlugin::CssmPlugin()
	: mLoaded(false)
{
}

CssmPlugin::~CssmPlugin()
{
	// Note: if mLoaded, we're being unloaded forcibly.
	// (CSSM wouldn't do this to us in normal operation.)
}


//
// Load processing.
// CSSM only calls this once for a module, and multiplexes any additional
// CSSM_ModuleLoad calls internally. So this is only called when we have just
// been loaded (and not yet attached).
//
void CssmPlugin::moduleLoad(const Guid &cssmGuid,
                const Guid &moduleGuid,
                const ModuleCallback &newCallback)
{
    if (mLoaded)
        CssmError::throwMe(CSSM_ERRCODE_INTERNAL_ERROR);
        
	mMyGuid = moduleGuid;

    // let the implementation know that we're loading
	this->load();

    // commit
    mCallback = newCallback;
    mLoaded = true;
}


//
// Unload processing.
// The callback passed here will be the same passed to load.
// CSSM only calls this on a "final" CSSM_ModuleUnload, after all attachments
// are destroyed and (just) before we are physically unloaded.
//
void CssmPlugin::moduleUnload(const Guid &cssmGuid,
				const Guid &moduleGuid,
                const ModuleCallback &oldCallback)
{
    // check the callback vector
    if (!mLoaded || oldCallback != mCallback)
        CssmError::throwMe(CSSM_ERRCODE_INTERNAL_ERROR);

    // tell our subclass that we're closing down
	this->unload();

    // commit closure
    mLoaded = false;
}


//
// Create one attachment session. This is what CSSM calls to process
// a CSSM_ModuleAttach call. moduleLoad() has already been called and has
// returned successfully.
//
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
	// basic (in)sanity checks
	if (moduleGuid != mMyGuid)
		CssmError::throwMe(CSSM_ERRCODE_INVALID_GUID);
    
    // make the new session object, hanging in thin air
    auto_ptr<PluginSession> session(this->makeSession(theHandle,
                                         version,
                                         subserviceId, subserviceType,
                                         attachFlags,
                                         upcalls));

	// haggle with the implementor
	funcTbl = session->construct();

	// commit this session creation
    StLock<Mutex> _(sessionMap());
	sessionMap()[theHandle] = session.release();
}


//
// Undo a (single) module attachment. This calls the detach() method on
// the Session object representing the attachment. This is only called
// if session->construct() has succeeded previously.
// If session->detach() fails, we do not destroy the session and it continues
// to live, though its handle may have (briefly) been invalid. This is for
// desperate "mustn't go right now" situations and should not be abused.
// CSSM always has the ability to ditch you without your consent if you are
// obstreporous.
//
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
		delete session;
	} catch (...) {
		// session detach failed - put the plugin back and fail
		StLock<Mutex> _(sessionMap());
		sessionMap()[handle] = session;
		throw;
	}
}


//
// Send an official CSSM module callback message upstream
//
void CssmPlugin::sendCallback(CSSM_MODULE_EVENT event, uint32 ssid,
                     		  CSSM_SERVICE_TYPE serviceType) const
{
	assert(mLoaded);
	mCallback(event, mMyGuid, ssid, serviceType);
}


//
// Default subclass hooks.
// The default implementations succeed without doing anything.
//
void CssmPlugin::load() { }

void CssmPlugin::unload() { }

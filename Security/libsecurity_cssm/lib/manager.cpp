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
// manager - CSSM manager/supervisor objects.
//
#include "manager.h"
#include "module.h"
#include <security_utilities/debugging.h>


//
// Constructing a CssmManager instance.
// This does almost nothing - the actual intialization happens in the initialize() method.
//
CssmManager::CssmManager()
{
    initCount = 0;	// not yet initialized
}

CssmManager::~CssmManager()
{
	if (initCount > 0)
		secdebug("cssm", "CSSM forcibly shutting down");
}


//
// CSSM initialization.
// THREADS: This function must run in an uncontested environment.
//
void CssmManager::initialize (const CSSM_VERSION &version,
                              CSSM_PRIVILEGE_SCOPE scope,
                              const Guid &callerGuid,
                              CSSM_KEY_HIERARCHY keyHierarchy,
                              CSSM_PVC_MODE &pvcPolicy)
{
    StLock<Mutex> _(mLock);
    
    // check version first
    checkVersion(version);
    
    if (initCount) {
        // re-initialization processing as per CSSM spec
        if (pvcPolicy != mPvcPolicy) {
            pvcPolicy = mPvcPolicy; // return old value
            CssmError::throwMe(CSSMERR_CSSM_PVC_ALREADY_CONFIGURED);
        }
        initCount++;
        secdebug("cssm", "re-initializing CSSM (%d levels)", initCount);
        return;
    }
    
    // we don't support thread scope privileges
    if (scope == CSSM_PRIVILEGE_SCOPE_THREAD)
        CssmError::throwMe(CSSMERR_CSSM_SCOPE_NOT_SUPPORTED);

    // keep the init arguments for future use - these become instance constants
    mPrivilegeScope = scope;
    mKeyHierarchy = keyHierarchy;
    mPvcPolicy = pvcPolicy;
    mCallerGuid = callerGuid;

    // we are ready now
    initCount = 1;
    secdebug("cssm", "CSSM initialized");
}


//
// CSSM Termination processing.
// Returns true if this was the final (true) termination, false if a nested Init was undone.
//
bool CssmManager::terminate()
{
    StLock<Mutex> _(mLock);
    switch (initCount) {
    case 0:
        CssmError::throwMe(CSSMERR_CSSM_NOT_INITIALIZED);
    case 1:
        secdebug("cssm", "Terminating CSSM");
        if (!moduleMap.empty())
            CssmError::throwMe(CSSM_ERRCODE_FUNCTION_FAILED);	// @#can't terminate with modules loaded
        initCount = 0;	// mark uninitialized
        return true;
    default:
        initCount--;	// nested INIT, just count down
        secdebug("cssm", "CSSM nested termination (%d remaining)", initCount);
        return false;
    }
}


#if defined(RESTRICTED_CSP_LOADING)
static const char * const allowedCSPs[] = {
	"/System/Library/Security/AppleCSP.bundle",
	"/System/Library/Security/AppleCSPDL.bundle",
	NULL
};
#endif


//
// Load a module (well, try).
//
void CssmManager::loadModule(const Guid &guid,
                             CSSM_KEY_HIERARCHY,
                             const ModuleCallback &callback)
{
    StLock<Mutex> _(mLock);
    ModuleMap::iterator it = moduleMap.find(guid);
    Module *module;
    if (it == moduleMap.end()) {
        MdsComponent info(guid);
#if defined(RESTRICTED_CSP_LOADING)
		// An abominable temporary hack for legal reasons. They made me do it!
		if (info.supportsService(CSSM_SERVICE_CSP)) {
			string loadPath = info.path();
			for (const char * const *pp = allowedCSPs; *pp; pp++)
				if (loadPath == *pp)
					goto allowed;
			CssmError::throwMe(CSSM_ERRCODE_MODULE_MANIFEST_VERIFY_FAILED);
		  allowed: ;
		}
#endif
		secdebug("cssm", "loading module %s(%s) from %s",
			info.name().c_str(), info.description().c_str(), info.path().c_str());
        module = new Module(this, info, loader(info.path()));
        moduleMap[guid] = module;
    } else {
        module = it->second;
		secdebug("cssm", "%p reloaded module %s(%s) at %s",
			module, module->name().c_str(), module->description().c_str(),
			module->path().c_str());
	}
	
	// We are not playing the "key hierarchy" game around here.
	// if we did, this is where we'd check the manifest.

    module->add(callback);
}


//
// Unload a module.
// THREADS: Locking Manager(1), Module(2).
//
void CssmManager::unloadModule(const Guid &guid,
                               const ModuleCallback &callback)
{
    StLock<Mutex> _(mLock);
    Module *module = getModule(guid);
    if (module->unload(callback)) {
		secdebug("cssm", "%p module %s(%s) final unload",
			module, module->name().c_str(), module->description().c_str());
        moduleMap.erase(guid);
        delete module;
    } else
		secdebug("cssm", "%p module %s(%s) load count now %u", module,
			module->name().c_str(), module->description().c_str(), module->callbackCount());
}


//
// Introductions
//
void CssmManager::introduce(const Guid &,
                            CSSM_KEY_HIERARCHY)
{
    StLock<Mutex> _(mLock);
    CssmError::throwMe(CSSM_ERRCODE_FUNCTION_NOT_IMPLEMENTED);
}

void CssmManager::unIntroduce(const Guid &)
{
    StLock<Mutex> _(mLock);
    CssmError::throwMe(CSSM_ERRCODE_FUNCTION_NOT_IMPLEMENTED);
}


//
// Support.
// THREADS: These utilities run under lock protection by the caller.
//
void CssmManager::checkVersion(const CSSM_VERSION &version)
{
    if (version.Major != 2)
        CssmError::throwMe(CSSM_ERRCODE_INCOMPATIBLE_VERSION);
    if (version.Minor != 0)
        CssmError::throwMe(CSSM_ERRCODE_INCOMPATIBLE_VERSION);
}

Module *CssmManager::getModule(const Guid &guid)
{
    ModuleMap::iterator it = moduleMap.find(guid);
    if (it == moduleMap.end())
        CssmError::throwMe(CSSMERR_CSSM_MODULE_NOT_LOADED);
    return it->second;
}

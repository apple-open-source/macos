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
// handleobject - give an object a process-global unique handle
//
#include <Security/handleobject.h>


//
// Static members of HandleObject
//
ModuleNexus<HandleObject::State> HandleObject::state;


//
// Bring the State constructor out of line
//
HandleObject::State::State()
{ }


//
// HandleObject destructor (virtual)
//
HandleObject::~HandleObject()
{
	State &st = state();
	StLock<Mutex> _(st);
	st.erase(this);
}


//
// Assign a HandleObject's (new) Handle.
//
void HandleObject::State::make(HandleObject *obj)
{
    StLock<Mutex> _(*this);
	for (;;) {
		Handle handle = reinterpret_cast<uint32>(obj) ^ (++sequence << 19);
		if (handleMap[handle] == NULL) {
			secdebug("handleobj", "create 0x%lx for %p", handle, obj);
			obj->setHandle(handle);
			handleMap[handle] = obj;
			return;
		}
	}
}


//
// Clean up a HandleObject that dies.
// Note that an object MAY clear its handle before (in which case we do nothing).
// In particular, killHandle will do this.
//
void HandleObject::State::erase(HandleObject *obj)
{
    if (obj->validHandle())
        handleMap.erase(obj->handle());
}

void HandleObject::State::erase(HandleMap::iterator &it)
{
    if (it->second->validHandle())
        handleMap.erase(it);
}


//
// Observing proper map locking, locate a handle in the global handle map
// and return a pointer to its object. Throw CssmError(error) if it cannot
// be found, or it is corrupt.
//
HandleObject *HandleObject::State::find(CSSM_HANDLE h, CSSM_RETURN error)
{
	StLock<Mutex> _(*this);
	HandleMap::const_iterator it = handleMap.find(h);
	if (it == handleMap.end())
		CssmError::throwMe(error);
	HandleObject *obj = it->second;
	if (obj == NULL || obj->handle() != h)
		CssmError::throwMe(error);
	return obj;
}


//
// Look up the handle given in the global handle map.
// If not found, or if the object is corrupt, throw an exception.
// Otherwise, hold the State lock and return an iterator to the map entry.
// Caller must release the State lock in a timely manner.
//
HandleObject::HandleMap::iterator HandleObject::State::locate(CSSM_HANDLE h, CSSM_RETURN error)
{
	StLock<Mutex> locker(*this);
	HandleMap::iterator it = handleMap.find(h);
	if (it == handleMap.end())
		CssmError::throwMe(error);
	HandleObject *obj = it->second;
	if (obj == NULL || obj->handle() != h)
		CssmError::throwMe(error);
	locker.release();
	return it;
}


//
// The default locking virtual methods do nothing and succeed.
//
void HandleObject::lock() { }

bool HandleObject::tryLock() { return true; }

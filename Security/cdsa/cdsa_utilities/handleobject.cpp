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
#ifdef __MWERKS__
#define _CPP_HANDLEOBJECT
#endif
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
// Assign a HandleObject's (new) Handle.
//
void HandleObject::State::make(HandleObject *obj)
{
    StLock<Mutex> _(mLock);
	for (;;) {
		Handle handle = reinterpret_cast<uint32>(obj) ^ (++sequence << 19);
		if (handleMap[handle] == NULL) {
			debug("handleobj", "create 0x%lx for %p", handle, obj);
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
    StLock<Mutex> _(mLock);
    if (obj->validHandle())
        handleMap.erase(obj->handle());
}


//
// This is the main locator driver. It translates an object handle
// into an object pointer, on the way atomically locking it and/or
// removing it from the handle map for atomic deletion.
//
HandleObject *HandleObject::State::locate(CSSM_HANDLE h, LocateMode mode, CSSM_RETURN error)
{
    for (;;) {
		{
			StLock<Mutex> _(mLock);
			HandleMap::iterator it = handleMap.find(h);
			if (it == handleMap.end())
				CssmError::throwMe(error);
			HandleObject *obj = it->second;
			if (obj == NULL || obj->handle() != h)
				CssmError::throwMe(error);
			if (mode == findTarget)
				return obj;		// that's all, folks
			// atomic find-and-lock requested (implicit in remove operation)
			if (obj->tryLock()) {
				// got object lock - assured of exit path
				if (mode == removeTarget) {
					debug("handleobj", "killing %p", obj);
					handleMap.erase(h);
					obj->clearHandle();
				}
				return obj;
			}
			// obj is busy; relinquish maplock and try again later
			debug("handleobj", "object %p (handle 0x%lx) is busy - backing off",
				obj, h);
		}
#if _USE_THREADS == _USE_NO_THREADS
		assert(false);		// impossible; tryLock above always succeeds
#else // real threads
        Thread::yield();
#endif // real threads
    }
}


//
// The default locking virtual methods do nothing and succeed.
//
void HandleObject::lock() { }

bool HandleObject::tryLock() { return true; }

/*
 * Copyright (c) 2000-2006 Apple Computer, Inc. All Rights Reserved.
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
// handleobject - give an object a process-global unique handle
//
#ifndef _H_HANDLEOBJECT
#define _H_HANDLEOBJECT

#include <Security/cssm.h>
#include <security_utilities/refcount.h>
#include <security_utilities/threading.h>
#include <security_utilities/globalizer.h>
#include <security_cdsa_utilities/cssmerrors.h>

#if __GNUC__ > 2
#include <ext/hash_map>
using __gnu_cxx::hash_map;
#else
#include <hash_map>
#endif

namespace Security
{

//
// A HandledObject is a trivial mixin class whose only feature is that
// it has a *handle*, in the form of (currently) a CSSM_HANDLE of some kind.
// Subclasses need to assign such a handle during creation.
//
class HandledObject {
public:
    typedef CSSM_HANDLE Handle;
    static const Handle invalidHandle = 0;

    Handle handle() const { return mMyHandle; }
    bool validHandle() const { return mValid; }

protected:
    HandledObject(Handle h) : mMyHandle(h), mValid(true) { }
    HandledObject() { /*IFDEBUG(*/ mMyHandle = invalidHandle/*)*/ ; mValid = false; }

    void setHandle(Handle h)
    {
        assert(!mValid);	// guard against redefinition
        mMyHandle = h;
        mValid = true;
    }
    void clearHandle()
    { assert(mValid); mValid = false; }
    
private:
    Handle mMyHandle;			// our handle value
    bool mValid;				// is the handle (still) valid?
};


//
// Mapping CSSM_HANDLE values to object pointers and back.
// A HandleObject is a HandledObject (see above) that makes up its own handle
// based on some mechanism that you know nothing about.
//
// Please be very careful about the limits of the object contract here.
// We promise to invent a suitable, unique Handle for each HandleObject in
// existence within one address space. We promise that if you hand that handle
// to the various findHandle<>() variants, we will give you back the HandleObject
// that created it. This is the entire contract.
// We *will* make some efforts to diagnose invalid handles and throw exceptions on
// them, but the find() operation is supposed to be *fast*, so no heroic measures
// will be taken.
//
class HandleObject : public HandledObject {
    NOCOPY(HandleObject)
    class State;

public:
    HandleObject()				{ state().make(this); }
    virtual ~HandleObject();
	
public:
	template <class Subtype>
	static Subtype &find(CSSM_HANDLE handle, CSSM_RETURN error);
	
	template <class Subtype>
	static Subtype &findAndLock(CSSM_HANDLE handle, CSSM_RETURN error);
	
	template <class Subtype>
	static Subtype &findAndKill(CSSM_HANDLE handle, CSSM_RETURN error);
	
	template <class Subtype>
	static RefPointer<Subtype> findRef(CSSM_HANDLE handle, CSSM_RETURN error);

	template <class Subtype>
	static RefPointer<Subtype> findRefAndLock(CSSM_HANDLE handle, CSSM_RETURN error);

	template <class Subtype>
	static RefPointer<Subtype> findRefAndKill(CSSM_HANDLE handle, CSSM_RETURN error);
    
    // @@@  Remove when 4003540 is fixed
    template <class Subtype>
    static void findAllRefs(std::vector<CSSM_HANDLE> &refs) {
        state().findAllRefs<Subtype>(refs);
    }
    
protected:
    virtual void lock();
    virtual bool tryLock();

private:
    typedef hash_map<CSSM_HANDLE, HandleObject *> HandleMap;
    class State : public Mutex {
    public:
        State();
        void make(HandleObject *obj);
		HandleObject *find(Handle h, CSSM_RETURN error);
        HandleMap::iterator locate(Handle h, CSSM_RETURN error);
        void erase(HandleObject *obj);
		void erase(HandleMap::iterator &it);
		// @@@  Remove when 4003540 is fixed
        template <class Subtype> void findAllRefs(std::vector<CSSM_HANDLE> &refs);
        
    private:
        HandleMap handleMap;
        uint32 sequence;
    };
    
    static ModuleNexus<State> state;
};


//
// Type-specific ways to access the HandleObject map in various ways
//
template <class Subclass>
inline Subclass &HandleObject::find(CSSM_HANDLE handle, CSSM_RETURN error)
{
	Subclass *sub;
	if (!(sub = dynamic_cast<Subclass *>(state().find(handle, error))))
		CssmError::throwMe(error);
	return *sub;
}

template <class Subclass>
inline Subclass &HandleObject::findAndLock(CSSM_HANDLE handle,
	CSSM_RETURN error)
{
	for (;;) {
		HandleMap::iterator it = state().locate(handle, error);
		StLock<Mutex> _(state(), true);	// locate() locked it
		Subclass *sub;
		if (!(sub = dynamic_cast<Subclass *>(it->second)))
			CssmError::throwMe(error);	// bad type
		if (it->second->tryLock())		// try to lock it
			return *sub;				// okay, go
		Thread::yield();				// object lock failed, backoff and retry
	}
}

template <class Subclass>
inline Subclass &HandleObject::findAndKill(CSSM_HANDLE handle,
	CSSM_RETURN error)
{
	for (;;) {
		HandleMap::iterator it = state().locate(handle, error);
		StLock<Mutex> _(state(), true);	// locate() locked it
		Subclass *sub;
		if (!(sub = dynamic_cast<Subclass *>(it->second)))
			CssmError::throwMe(error);	// bad type
		if (it->second->tryLock()) {	// try to lock it
			state().erase(it);			// kill the handle
			return *sub;				// okay, go
		}
		Thread::yield();				// object lock failed, backoff and retry
	}
}

template <class Subclass>
inline RefPointer<Subclass> HandleObject::findRef(CSSM_HANDLE handle,
	CSSM_RETURN error)
{
	HandleMap::iterator it = state().locate(handle, error);
	StLock<Mutex> _(state(), true); // locate() locked it
	Subclass *sub;
	if (!(sub = dynamic_cast<Subclass *>(it->second)))
		CssmError::throwMe(error);
	return sub;
}

template <class Subclass>
inline RefPointer<Subclass> HandleObject::findRefAndLock(CSSM_HANDLE handle,
	CSSM_RETURN error)
{
	for (;;) {
		HandleMap::iterator it = state().locate(handle, error);
		StLock<Mutex> _(state(), true);	// locate() locked it
		Subclass *sub;
		if (!(sub = dynamic_cast<Subclass *>(it->second)))
			CssmError::throwMe(error);	// bad type
		if (it->second->tryLock())		// try to lock it
			return sub;				// okay, go
		Thread::yield();				// object lock failed, backoff and retry
	}
}

template <class Subclass>
inline RefPointer<Subclass> HandleObject::findRefAndKill(CSSM_HANDLE handle,
	CSSM_RETURN error)
{
	for (;;) {
		HandleMap::iterator it = state().locate(handle, error);
		StLock<Mutex> _(state(), true);	// locate() locked it
		Subclass *sub;
		if (!(sub = dynamic_cast<Subclass *>(it->second)))
			CssmError::throwMe(error);	// bad type
		if (it->second->tryLock()) {	// try to lock it
			state().erase(it);			// kill the handle
			return sub;					// okay, go
		}
		Thread::yield();				// object lock failed, backoff and retry
	}
}

//
// @@@  Remove when 4003540 is fixed
//
// This is a hack to fix 3981388 and should NOT be used elsewhere.  
// Also, do not follow this code's example: HandleObject::State methods 
// should not implement type-specific behavior.  
//
template <class Subtype>
inline void HandleObject::State::findAllRefs(std::vector<CSSM_HANDLE> &refs)
{
    StLock<Mutex> _(*this);
    HandleMap::iterator it = handleMap.begin();
    for (; it != handleMap.end(); ++it)
    {
        HandleObject *h = it->second;
        Subtype *obj = dynamic_cast<Subtype *>(h);
        if (obj)
            refs.push_back(it->first);
    }
}


//
// Compatibility with old (global function) accessors
//
template <class Subclass>
inline Subclass &findHandle(CSSM_HANDLE handle,
                     CSSM_RETURN error = CSSMERR_CSSM_INVALID_ADDIN_HANDLE)
{ return HandleObject::find<Subclass>(handle, error); }

template <class Subclass>
inline Subclass &findHandleAndLock(CSSM_HANDLE handle,
                            CSSM_RETURN error = CSSMERR_CSSM_INVALID_ADDIN_HANDLE)
{ return HandleObject::findAndLock<Subclass>(handle, error); }

template <class Subclass>
inline Subclass &killHandle(CSSM_HANDLE handle,
                     CSSM_RETURN error = CSSMERR_CSSM_INVALID_ADDIN_HANDLE)
{ return HandleObject::findAndKill<Subclass>(handle, error); }


} // end namespace Security

#endif //_H_HANDLEOBJECT

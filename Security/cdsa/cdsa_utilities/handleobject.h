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
#ifndef _H_HANDLEOBJECT
#define _H_HANDLEOBJECT

#include <Security/cssm.h>
#include <Security/utilities.h>
#include <Security/threading.h>
#include <Security/globalizer.h>
#include <hash_map>


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
    class State; friend class State;
    template <class Subtype> friend Subtype &findHandle(CSSM_HANDLE, CSSM_RETURN);
    template <class Subtype> friend Subtype &findHandleAndLock(CSSM_HANDLE, CSSM_RETURN);
    template <class Subtype> friend Subtype &killHandle(CSSM_HANDLE, CSSM_RETURN);
public:
    HandleObject()				{ state().make(this); }
    virtual ~HandleObject()		{ state().erase(this); }

protected:
    virtual void lock();
    virtual bool tryLock();

private:
    enum LocateMode { lockTarget, findTarget, removeTarget };

private:
    typedef hash_map<CSSM_HANDLE, HandleObject *> HandleMap;
    class State {
    public:
        State();
        void make(HandleObject *obj);
        HandleObject *locate(Handle h, LocateMode mode, CSSM_RETURN error);
        void erase(HandleObject *obj);

    private:
        HandleMap handleMap;
        uint32 sequence;
        Mutex mLock;
    };
    
    static ModuleNexus<State> state;
};


//
// Type-specific ways to access the HandleObject map in various ways
//
template <class Subclass>
Subclass &findHandle(CSSM_HANDLE handle,
                     CSSM_RETURN error = CSSMERR_CSSM_INVALID_ADDIN_HANDLE)
{
    Subclass *sub;
    if (!(sub = dynamic_cast<Subclass *>(HandleObject::state().locate(handle, HandleObject::findTarget, error))))
        CssmError::throwMe(error);
    return *sub;
}

template <class Subclass>
Subclass &findHandleAndLock(CSSM_HANDLE handle,
                            CSSM_RETURN error = CSSMERR_CSSM_INVALID_ADDIN_HANDLE)
{
    Subclass *sub;
    if (!(sub = dynamic_cast<Subclass *>(HandleObject::state().locate(handle, HandleObject::lockTarget, error))))
        CssmError::throwMe(error);
    return *sub;
}

template <class Subclass>
Subclass &killHandle(CSSM_HANDLE handle,
                     CSSM_RETURN error = CSSMERR_CSSM_INVALID_ADDIN_HANDLE)
{
    Subclass *sub;
    if (!(sub = dynamic_cast<Subclass *>(HandleObject::state().locate(handle, HandleObject::removeTarget, error))))
        CssmError::throwMe(error);
	return *sub;
}

} // end namespace Security

#endif //_H_HANDLEOBJECT

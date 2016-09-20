/*
 * Copyright (c) 2008,2011-2012 Apple Inc. All Rights Reserved.
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
// adjunct to handletemplates.h
//
// this header should only be #included by source files defining 
// TypedHandle or MappingHandle subclasses
//
// @@@  Should use non-CSSM error codes
//
#ifndef _H_HANDLETEMPLATES_DEFS
#define _H_HANDLETEMPLATES_DEFS

#include <Security/cssm.h>
#include <security_utilities/refcount.h>
#include <security_utilities/threading.h>
#include <security_utilities/globalizer.h>
#include <security_cdsa_utilities/cssmerrors.h>
#include <security_cdsa_utilities/handletemplates.h>

namespace Security
{

//
// TypedHandle
//
template <class _Handle>
TypedHandle<_Handle>::TypedHandle()
    : mMyHandle(invalidHandle), mValid(false)
{
}

template <class _Handle>
TypedHandle<_Handle>::TypedHandle(_Handle h)
    : mMyHandle(h), mValid(true)
{
}


//
// MappingHandle instance methods
//
template <class _Handle>
MappingHandle<_Handle>::MappingHandle() : TypedHandle<_Handle>()
{
    make();
}
    
template <class _Handle>
void MappingHandle<_Handle>::make()
{
    StLock<Mutex> _(state());
    
    _Handle hbase = (_Handle)reinterpret_cast<uintptr_t>(this);
    for (;;) {
        _Handle handle = hbase ^ state().nextSeq();
        if (!state().handleInUse(handle)) {
            // assumes sizeof(unsigned long) >= sizeof(handle)
            secinfo("handleobj", "create %#lx for %p", static_cast<unsigned long>(handle), this);
            TypedHandle<_Handle>::setHandle(handle);
            state().add(handle, this);
            return;
        }
    }
}
    
// The default locking virtual methods do nothing and succeed.
template <class _Handle>
void MappingHandle<_Handle>::lock() { }

template <class _Handle>
bool MappingHandle<_Handle>::tryLock() { return true; }
    

//
// MappingHandle::State
//

// The default State constructor should not be inlined in a standard
// header: its use via ModuleNexus would result in the inlined code
// appearing *everywhere* the State object might have to be constructed.  
template <class _Handle>
MappingHandle<_Handle>::State::State()
    : sequence(1)
{
}

// 
// Check if the handle is already in the map.  Caller must already hold 
// the map lock.  Intended for use by a subclass' implementation of 
// MappingHandle<...>::make().  
//
template <class _Handle>
bool MappingHandle<_Handle>::State::handleInUse(_Handle h)
{
    return (HandleMap::find(h) != (*this).end());
}

//
// Observing proper map locking, locate a handle in the global handle map
// and return a pointer to its object. Throw CssmError(error) if it cannot
// be found, or it is corrupt.
//
template <class _Handle>
MappingHandle<_Handle> *MappingHandle<_Handle>::State::find(_Handle h, CSSM_RETURN error)
{
	StLock<Mutex> _(*this);
	typename HandleMap::const_iterator it = HandleMap::find(h);
	if (it == (*this).end())
		CssmError::throwMe(error);
	MappingHandle<_Handle> *obj = it->second;
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
template <class _Handle>
typename MappingHandle<_Handle>::HandleMap::iterator 
MappingHandle<_Handle>::State::locate(_Handle h, CSSM_RETURN error)
{
	StLock<Mutex> locker(*this);
	typename HandleMap::iterator it = HandleMap::find(h);
	if (it == (*this).end())
		CssmError::throwMe(error);
	MappingHandle<_Handle> *obj = it->second;
	if (obj == NULL || obj->handle() != h)
		CssmError::throwMe(error);
	locker.release();
	return it;
}

//
// Add a handle and its associated object to the map.  Caller must already
// hold the map lock, and is responsible for collision-checking prior to
// calling this method.  Intended for use by a subclass' implementation of 
// MappingHandle<...>::make().  
//
template <class _Handle>
void MappingHandle<_Handle>::State::add(_Handle h, MappingHandle<_Handle> *obj)
{
    (*this)[h] = obj;
}

//
// Clean up the handle for an object that dies.  Caller must already hold
// the map lock.  
// Note that an object MAY clear its handle before (in which case we do nothing).
// In particular, killHandle will do this.
//
template <class _Handle>
void MappingHandle<_Handle>::State::erase(MappingHandle<_Handle> *obj)
{
    if (obj->validHandle())
        HandleMap::erase(obj->handle());
}

template <class _Handle>
void MappingHandle<_Handle>::State::erase(typename HandleMap::iterator &it)
{
    if (it->second->validHandle())
        HandleMap::erase(it);
}


//
// All explicit instantiations of MappingHandle subclasses get the 
// generation of their 'state' member for free (if they #include this
// file).  
//
template <class _Handle>
ModuleNexus<typename MappingHandle<_Handle>::State> MappingHandle<_Handle>::state;


} // end namespace Security

#endif //_H_HANDLETEMPLATES_DEFS

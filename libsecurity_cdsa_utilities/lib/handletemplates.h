/*
 * Copyright (c) 2008 Apple Inc. All Rights Reserved.
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
// Templates to support HandleObject-like objects
//
#ifndef _H_HANDLETEMPLATES
#define _H_HANDLETEMPLATES

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
// A TypedHandle is a trivial mixin class whose only feature is that
// it has a *handle* whose type is of the caller's choosing.  Subclasses 
// need to assign such a handle during creation.
//
template <class _Handle>
struct TypedHandle
{
public:
    typedef _Handle Handle;
    
    static const _Handle invalidHandle = 0;

    _Handle handle() const     { return mMyHandle; }
    bool validHandle() const  { return mValid; }

protected:
    TypedHandle(_Handle h);
    TypedHandle();

    void setHandle(_Handle h)
    {
        assert(!mValid);    // guard against redefinition
        mMyHandle = h;
        mValid = true;
    }
    void clearHandle()
    {
        assert(mValid);
        mValid = false;
    }
    
private:
    _Handle mMyHandle;           // our handle value
    bool mValid;                // is the handle (still) valid?
};

//
// MappingHandle wraps a map indexed by handles of the chosen type.  
// A MappingHandle makes up its own handle based on some mechanism that you 
// know nothing about.
//
// Please be very careful about the limits of the object contract here.
// We promise to invent a suitable, unique handle for each MappingHandle in
// existence within one address space. We promise that if you hand that 
// handle to the various MappingHandle<>::find() variants, we will give you 
// back the MappingHandle that created it. We promise to throw if you pass
// a bad handle to those MappingHandle<>::find() variants. This is the
// entire contract. 
//
template <class _Handle>
class MappingHandle : public TypedHandle<_Handle>
{
protected:
    class State;
    
public: 
    typedef typename TypedHandle<_Handle>::Handle Handle;
    virtual ~MappingHandle()
    {
        State &st = state();
        StLock<Mutex> _(st);
        st.erase(this);
    }

    template <class SubType>
    static SubType &find(_Handle handle, CSSM_RETURN error);

	template <class Subtype>
	static Subtype &findAndLock(_Handle handle, CSSM_RETURN error);
	
	template <class Subtype>
	static Subtype &findAndKill(_Handle handle, CSSM_RETURN error);
	
	template <class Subtype>
	static RefPointer<Subtype> findRef(_Handle handle, CSSM_RETURN error);

	template <class Subtype>
	static RefPointer<Subtype> findRefAndLock(_Handle handle, CSSM_RETURN error);

	template <class Subtype>
	static RefPointer<Subtype> findRefAndKill(_Handle handle, CSSM_RETURN error);
    
    // @@@  Remove when 4003540 is fixed
    template <class Subtype>
    static void findAllRefs(std::vector<_Handle> &refs) {
        state().findAllRefs<Subtype>(refs);
    }
    
protected:
    virtual void lock();
    virtual bool tryLock();
    
    typedef hash_map<_Handle, MappingHandle<_Handle> *> HandleMap;

    MappingHandle();

    class State : public Mutex, public HandleMap
    {
    public:
        State();
        uint32_t nextSeq()  { return ++sequence; }

        bool handleInUse(_Handle h);
        MappingHandle<_Handle> *find(_Handle h, CSSM_RETURN error);
        typename HandleMap::iterator locate(_Handle h, CSSM_RETURN error);
        void add(_Handle h, MappingHandle<_Handle> *obj);
        void erase(MappingHandle<_Handle> *obj);
        void erase(typename HandleMap::iterator &it);
        // @@@  Remove when 4003540 is fixed
        template <class SubType> void findAllRefs(std::vector<_Handle> &refs);

    private:
        uint32_t sequence;
    };
    
private:
    // 
    // Create the handle to be used by the map
    //
    void make();

    static ModuleNexus<typename MappingHandle<_Handle>::State> state;
};

//
// MappingHandle class methods
// Type-specific ways to access the map in various ways
//
template <class _Handle>
template <class Subclass>
inline Subclass &MappingHandle<_Handle>::find(_Handle handle, CSSM_RETURN error)
{
    Subclass *sub;
    if (!(sub = dynamic_cast<Subclass *>(state().find(handle, error))))
        CssmError::throwMe(error);
    return *sub;
}

template <class _Handle>
template <class Subclass>
inline Subclass &MappingHandle<_Handle>::findAndLock(_Handle handle,
                                             CSSM_RETURN error)
{
    for (;;) {
        typename HandleMap::iterator it = state().locate(handle, error);
        StLock<Mutex> _(state(), true);	// locate() locked it
        Subclass *sub;
        if (!(sub = dynamic_cast<Subclass *>(it->second)))
            CssmError::throwMe(error);	// bad type
        if (it->second->tryLock())		// try to lock it
            return *sub;				// okay, go
        Thread::yield();				// object lock failed, backoff and retry
    }
}

template <class _Handle>
template <class Subclass>
inline Subclass &MappingHandle<_Handle>::findAndKill(_Handle handle,
                                             CSSM_RETURN error)
{
    for (;;) {
        typename HandleMap::iterator it = state().locate(handle, error);
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

template <class _Handle>
template <class Subclass>
inline RefPointer<Subclass> MappingHandle<_Handle>::findRef(_Handle handle,
                                                    CSSM_RETURN error)
{
    typename HandleMap::iterator it = state().locate(handle, error);
    StLock<Mutex> _(state(), true); // locate() locked it
    Subclass *sub;
    if (!(sub = dynamic_cast<Subclass *>(it->second)))
        CssmError::throwMe(error);
    return sub;
}

template <class _Handle>
template <class Subclass>
inline RefPointer<Subclass> MappingHandle<_Handle>::findRefAndLock(_Handle handle,
                                                           CSSM_RETURN error)
{
    for (;;) {
        typename HandleMap::iterator it = state().locate(handle, error);
        StLock<Mutex> _(state(), true);	// locate() locked it
        Subclass *sub;
        if (!(sub = dynamic_cast<Subclass *>(it->second)))
            CssmError::throwMe(error);	// bad type
        if (it->second->tryLock())		// try to lock it
            return sub;				// okay, go
        Thread::yield();				// object lock failed, backoff and retry
    }
}

template <class _Handle>
template <class Subclass>
inline RefPointer<Subclass> MappingHandle<_Handle>::findRefAndKill(_Handle handle,
                                                           CSSM_RETURN error)
{
    for (;;) {
        typename HandleMap::iterator it = state().locate(handle, error);
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
// Also, do not follow this code's example: State methods should not 
// implement type-specific behavior.  
//
template <class _Handle>
template <class Subtype>
void MappingHandle<_Handle>::State::findAllRefs(std::vector<_Handle> &refs)
{
    StLock<Mutex> _(*this);
    typename HandleMap::iterator it = (*this).begin();
    for (; it != (*this).end(); ++it)
    {
        Subtype *obj = dynamic_cast<Subtype *>(it->second);
        if (obj)
            refs.push_back(it->first);
    }
}


} // end namespace Security

#endif //_H_HANDLETEMPLATES

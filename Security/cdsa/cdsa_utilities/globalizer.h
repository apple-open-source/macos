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


/*
 * globalizer - multiscope globalization services
 */
#ifndef _H_GLOBALIZER
#define _H_GLOBALIZER

#include <Security/threading.h>
#include <memory>

namespace Security {


//
// GlobalNexus is the common superclass of all globality scopes.
// A Nexus is an *access point* to the *single* object of a given
// type in the Nexus's particular scope.
//
class GlobalNexus {
public:
    class Error : public std::exception {
    public:
        virtual ~Error() throw();
        const char * const message;
        Error(const char *m) : message(m) { }
        const char *what() const throw() { return message; }
    };
};


//
// A module-scope nexus is tied to the linker Nexus object itself.
// Its scope is all code accessing that particular Nexus object
// from within a process. Any number of ModuleNexus objects can
// exist, and each implements a different scope.
//
// IMPORTANT notes on this class can be found in globalizer.cpp.
// DO NOT change anything here before carefully reading them.
//
#if defined(_HAVE_ATOMIC_OPERATIONS)

class ModuleNexusCommon : public GlobalNexus {
protected:
    AtomicWord create(void *(*make)());
    
protected:
    // both of these will be statically initialized to zero
    AtomicWord pointer;
    StaticAtomicCounter<uint32> sync;
};

template <class Type>
class ModuleNexus : public ModuleNexusCommon {
public:
    Type &operator () ()
    {
        AtomicWord p = pointer;	// latch pointer
        return *reinterpret_cast<Type *>((p && !(p & 0x1)) ? p : create(make));
    }
    
    void reset()
    {
        if (pointer && !(pointer & 0x1)) {
            delete reinterpret_cast<Type *>(pointer);
            pointer = 0;
        }
    }
    
private:
    static void *make() { return new Type; }
};

template <class Type>
class CleanModuleNexus : public ModuleNexus<Type> {
public:
    ~CleanModuleNexus()
    {
        debug("nexus", "ModuleNexus %p destroyed object 0x%x", this, pointer);
        delete reinterpret_cast<Type *>(pointer);
    }
};

#else	// !_HAVE_ATOMIC_OPERATIONS

template <class Type>
class ModuleNexus : public GlobalNexus {
public:
    Type &operator () ()
    {
#if !defined(PTHREAD_STRICT)
        // not strictly kosher POSIX, but pointers are usually atomic types
        if (mSingleton)
            return *mSingleton;
#endif
        StLock<Mutex> _(mLock);
        if (mSingleton == NULL)
            mSingleton = new Type;
        return *mSingleton;
    }
    
    void reset()		{ delete mSingleton; mSingleton = NULL; }
    
protected:
    Type *mSingleton;		// pointer to singleton static initialized to NULL
    Mutex mLock;			// construction lock
};

template <class Type>
class CleanModuleNexus : public ModuleNexus<Type> {
public:
    ~CleanModuleNexus()
    {
        debug("nexus", "ModuleNexus %p destroyed object 0x%x", this, mSingleton);
        delete mSingleton;
    }
};

#endif // _HAVE_ATOMIC_OPERATIONS


//
// A thread-scope nexus is tied to a particular native thread AND
// a particular nexus object. Its scope is all code in any one thread
// that access that particular Nexus object. Any number of Nexus objects
// can exist, and each implements a different scope for each thread.
// NOTE: ThreadNexus is dynamically constructed. If you want static,
// zero-initialization ThreadNexi, put them inside a ModuleNexus.
//
#if _USE_THREADS == _USE_PTHREADS

template <class Type>
class ThreadNexus : public GlobalNexus {
public:
    ThreadNexus() : mSlot(true) { }
    Type &operator () ()
    {
        // no thread contention here!
        if (Type *p = mSlot)
            return *p;
        mSlot = new Type;
        return *mSlot;
    }

private:
    PerThreadPointer<Type> mSlot;
};

#endif //_USE_PTHREADS


//
// A ProcessNexus is global within a single process, regardless of
// load module boundaries. You can have any number of ProcessNexus
// scopes, each identified by a C string (compared by value, not pointer).
//
class ProcessNexusBase : public GlobalNexus {
protected:
	ProcessNexusBase(const char *identifier);

	struct Store {
		void *mObject;
		Mutex mLock;
	};
	Store *mStore;
};

template <class Type>
class ProcessNexus : public ProcessNexusBase {
public:
	ProcessNexus(const char *identifier) : ProcessNexusBase(identifier) { }
	Type &operator () ();
	
private:
	Type *mObject;
};

template <class Type>
Type &ProcessNexus<Type>::operator () ()
{
#if !defined(PTHREAD_STRICT)
    // not strictly kosher POSIX, but pointers are usually atomic types
    if (mStore->mObject)
        return *reinterpret_cast<Type *>(mStore->mObject);
#endif
    StLock<Mutex> _(mStore->mLock);
    if (mStore->mObject == NULL)
        mStore->mObject = new Type;
    return *reinterpret_cast<Type *>(mStore->mObject);
};


} // end namespace Security

#endif //_H_GLOBALIZER

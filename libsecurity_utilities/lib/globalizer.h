/*
 * Copyright (c) 2000-2004 Apple Computer, Inc. All Rights Reserved.
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


/*
 * globalizer - multiscope globalization services
 */
#ifndef _H_GLOBALIZER
#define _H_GLOBALIZER

#include <security_utilities/threading.h>
#include <memory>
#include <set>

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
class ModuleNexusCommon : public GlobalNexus {
protected:
    void *create(void *(*make)());
    
protected:
    // both of these will be statically initialized to zero
	void *pointer;
    StaticAtomicCounter<uint32_t> sync;
};

template <class Type>
class ModuleNexus : public ModuleNexusCommon {
public:
    Type &operator () ()
    {
        void *p = Atomic<void *>::load(pointer);	// latch pointer
		if (!p || (uintptr_t(p) & 0x1)) {
			p = create(make);
			secdebug("nexus", "module %s 0x%p", Debug::typeName<Type>().c_str(), pointer);
		}
		return *reinterpret_cast<Type *>(p);
    }
	
	// does the object DEFINITELY exist already?
	bool exists() const
	{
		return Atomic<void *>::load(pointer) != NULL;
	}
    
	// destroy the object (if any) and start over - not really thread-safe
    void reset()
    {
        if (pointer && !(uintptr_t(pointer) & 0x1)) {
            delete reinterpret_cast<Type *>(pointer);
            Atomic<void *>::store(0, pointer);
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
        secdebug("nexus", "ModuleNexus %p destroyed object 0x%x",
			this, ModuleNexus<Type>::pointer);
        delete reinterpret_cast<Type *>(ModuleNexus<Type>::pointer);
    }
};

typedef std::set<void*> RetentionSet;

class ThreadNexusBase {
protected:
	static ModuleNexus<Mutex> mInstanceLock;
	static ModuleNexus<RetentionSet> mInstances;
};



//
// A thread-scope nexus is tied to a particular native thread AND
// a particular nexus object. Its scope is all code in any one thread
// that access that particular Nexus object. Any number of Nexus objects
// can exist, and each implements a different scope for each thread.
// NOTE: ThreadNexus is dynamically constructed. If you want static,
// zero-initialization ThreadNexi, put them inside a ModuleNexus.
//
template <class Type>
class ThreadNexus : public GlobalNexus, private ThreadNexusBase {
public:
    ThreadNexus() : mSlot(true) { }

    Type &operator () ()
    {
        // no thread contention here!
        if (Type *p = mSlot)
            return *p;
        mSlot = new Type;
		{
			StLock<Mutex> _(mInstanceLock ());
			mInstances ().insert(mSlot);
		}
        return *mSlot;
    }

private:
    PerThreadPointer<Type> mSlot;
};


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

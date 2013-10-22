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
	Based on code donated by Perry Kiehtreiber
 */
#ifndef _SECURITY_REFCOUNT_H_
#define _SECURITY_REFCOUNT_H_

#include <security_utilities/threading.h>
#include <libkern/OSAtomic.h>

namespace Security {


//
// RefCount/RefPointer - a simple reference counting facility.
//
// To make an object reference-counted, inherit from RefCount. To track refcounted
// objects, use RefPointer<TheType>, where TheType must inherit from RefCount.
//
// RefCount is thread safe - any number of threads can hold and manipulate references
// in parallel. It does however NOT protect the contents of your object - just the
// reference count itself. If you need to share your object contents, you must provide
// appropriate locking yourself.
//
// There is no (thread safe) way to determine whether you are the only thread holding
// a pointer to a particular RefCount object. Thus there is no (thread safe)
// way to "demand copy" a RefCount subclass. Trust me; it's been tried. Don't.
//

#if !defined(DEBUG_REFCOUNTS)
# define DEBUG_REFCOUNTS 1
#endif

#if DEBUG_REFCOUNTS
# define RCDEBUG(_kind, _args...)	SECURITY_DEBUG_REFCOUNT_##_kind((void *)this, ##_args)
#else
# define RCDEBUG(kind)	/* nothing */
#endif


//
// Base class for reference counted objects
//
class RefCount {	
public:
	RefCount() : mRefCount(0) { RCDEBUG(CREATE); }

protected:
	template <class T> friend class RefPointer;

	void ref() const
    {
        OSAtomicIncrement32(&mRefCount);
        RCDEBUG(UP, mRefCount);
    }
	
    unsigned int unref() const
    {
        RCDEBUG(DOWN, mRefCount - 1);
        return OSAtomicDecrement32(&mRefCount);
    }
	
	// if you call this for anything but debug output, you will go to hell (free handbasket included)
	unsigned int refCountForDebuggingOnly() const { return mRefCount; }

private:
    volatile mutable int32_t mRefCount;
};


//
// A pointer type supported by reference counts.
// T must be derived from RefCount.
//
template <class T>
class RefPointer {
	template <class Sub> friend class RefPointer; // share with other instances
public:
	RefPointer() : ptr(0) {}			// default to NULL pointer
	RefPointer(const RefPointer& p) { if (p) p->ref(); ptr = p.ptr; }
	RefPointer(T *p) { if (p) p->ref(); ptr = p; }
	
	template <class Sub>
	RefPointer(const RefPointer<Sub>& p) { if (p) p->ref(); ptr = p.ptr; }
    
	~RefPointer() { release(); }

	RefPointer& operator = (const RefPointer& p)	{ setPointer(p.ptr); return *this; }
 	RefPointer& operator = (T * p)					{ setPointer(p); return *this; }

	template <class Sub>
	RefPointer& operator = (const RefPointer<Sub>& p) { setPointer(p.ptr); return *this; }

	// dereference operations
    T* get () const				{ _check(); return ptr; }	// mimic auto_ptr
	operator T * () const		{ _check(); return ptr; }
	T * operator -> () const	{ _check(); return ptr; }
	T & operator * () const		{ _check(); return *ptr; }

protected:
	void release_internal()
    {
        if (ptr && ptr->unref() == 0)
        {
            delete ptr;
            ptr = NULL;
        }
    }
	
    void release()
    {
        StLock<Mutex> mutexLock(mMutex);
        release_internal();
    }
    
    void setPointer(T *p)
    {
        StLock<Mutex> mutexLock(mMutex);
        if (p)
        {
            p->ref();
        }
        
        release_internal();
        ptr = p;
    }
	
	void _check() const { }

	T *ptr;
    Mutex mMutex;
};

template <class T>
bool operator <(const RefPointer<T> &r1, const RefPointer<T> &r2)
{
	T *p1 = r1.get(), *p2 = r2.get();
	return p1 && p2 ? *p1 < *p2 : p1 < p2;
}

template <class T>
bool operator ==(const RefPointer<T> &r1, const RefPointer<T> &r2)
{
	T *p1 = r1.get(), *p2 = r2.get();
	return p1 && p2 ? *p1 == *p2 : p1 == p2;
}

template <class T>
bool operator !=(const RefPointer<T> &r1, const RefPointer<T> &r2)
{
	T *p1 = r1.get(), *p2 = r2.get();
	return p1 && p2 ? *p1 != *p2 : p1 != p2;
}

} // end namespace Security

#endif // !_SECURITY_REFCOUNT_H_

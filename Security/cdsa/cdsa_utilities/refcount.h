/*
 * Copyright (c) 2000-2002 Apple Computer, Inc. All Rights Reserved.
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
	Based on code donated by Perry Kiehtreiber
 */
#ifndef _SECURITY_REFCOUNT_H_
#define _SECURITY_REFCOUNT_H_

#include <Security/threading.h>

namespace Security
{

//
// RefCount/RefPointer - a simple reference counting facility.
//
// To make an object reference-counted, derive it from RefCount. To track refcounted
// objects, use RefPointer<TheType>, where TheType must be derived from RefCount.
//
// RefCount is thread safe - any number of threads can hold and manipulate references
// in parallel. It does however NOT protect the contents of your object - just the
// reference count itself. If you need to share your object contents, you must engage
// in appropriate locking yourself.
//
// There is no (thread safe) way to determine whether you are the only thread holding
// a pointer to a particular RefCount object.
//


//
// Base class for reference counted objects
//
class RefCount {
public:
	RefCount() : mRefCount(0) { }

protected:
	template <class T> friend class RefPointer;

	void ref() const			{ ++mRefCount; }
	unsigned int unref() const	{ return --mRefCount; }

private:
    mutable AtomicCounter<unsigned int> mRefCount;
};


//
// A pointer type supported by reference counts.
// T must be derived from RefCount.
//
template <class T>
class RefPointer {
public:
	RefPointer() : ptr(0) {}			// default to NULL pointer
	RefPointer(const RefPointer& p) { if (p) p->ref(); ptr = p.ptr; }
	RefPointer(T *p) { if (p) p->ref(); ptr = p; }
    
	~RefPointer() { release(); }

	RefPointer& operator = (const RefPointer& p)	{ setPointer(p.ptr); return *this; }
 	RefPointer& operator = (T * p)					{ setPointer(p); return *this; }

	// dereference operations
    T* get () const				{ return ptr; }	// mimic auto_ptr
	operator T * () const		{ return ptr; }
	T * operator -> () const	{ return ptr; }
	T & operator * () const		{ return *ptr; }

protected:
	void release() { if (ptr && ptr->unref() == 0) delete ptr; }
	void setPointer(T *p) { if (p) p->ref(); release(); ptr = p; }

	T *ptr;
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

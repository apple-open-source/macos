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



#ifndef _SECCFOBJECT_H
#define _SECCFOBJECT_H

#include <CoreFoundation/CFRuntime.h>
#include <new>
#include "threading.h"

namespace Security {

class CFClass;

RecursiveMutex& GetDOMutex();

#define SECCFFUNCTIONS(OBJTYPE, APIPTR, ERRCODE, CFCLASS) \
\
void *operator new(size_t size) throw(std::bad_alloc) \
{ return SecCFObject::allocate(size, CFCLASS); } \
\
operator APIPTR() const \
{ return (APIPTR)(this->operator CFTypeRef()); } \
\
OBJTYPE *retain() \
{ SecCFObject::handle(true); return this; } \
APIPTR handle(bool retain = true) \
{ return (APIPTR)SecCFObject::handle(retain); } \
\
static OBJTYPE *required(APIPTR ptr) \
{ if (OBJTYPE *p = dynamic_cast<OBJTYPE *>(SecCFObject::required(ptr, ERRCODE))) \
	return p; else MacOSError::throwMe(ERRCODE); } \
\
static OBJTYPE *optional(APIPTR ptr) \
{ if (SecCFObject *p = SecCFObject::optional(ptr)) \
	if (OBJTYPE *pp = dynamic_cast<OBJTYPE *>(p)) return pp; else MacOSError::throwMe(ERRCODE); \
  else return NULL; }

#define SECALIGNUP(SIZE, ALIGNMENT) (((SIZE - 1) & ~(ALIGNMENT - 1)) + ALIGNMENT)

struct SecRuntimeBase: CFRuntimeBase
{
	bool isNew;
};

class SecCFObject
{
private:
	void *operator new(size_t) throw(std::bad_alloc);

	// Align up to a multiple of 16 bytes
	static const size_t kAlignedRuntimeSize = SECALIGNUP(sizeof(SecRuntimeBase), 4);

public:
	// For use by SecPointer only. Returns true once the first time it's called after the object has been created.
	bool isNew()
	{
		SecRuntimeBase *base = reinterpret_cast<SecRuntimeBase *>(reinterpret_cast<uint8_t *>(this) - kAlignedRuntimeSize);
		bool isNew = base->isNew;
		base->isNew = false;
		return isNew;
	}

	static SecCFObject *optional(CFTypeRef) throw();
	static SecCFObject *required(CFTypeRef, OSStatus error);
	static void *allocate(size_t size, const CFClass &cfclass) throw(std::bad_alloc);

	virtual ~SecCFObject();

	void operator delete(void *object) throw();
	operator CFTypeRef() const throw()
	{
		return reinterpret_cast<CFTypeRef>(reinterpret_cast<const uint8_t *>(this) - kAlignedRuntimeSize);
	}

	// This bumps up the retainCount by 1, by calling CFRetain(), iff retain is true
	CFTypeRef handle(bool retain = true) throw();

    virtual bool equal(SecCFObject &other);
    virtual CFHashCode hash();
	virtual CFStringRef copyFormattingDesc(CFDictionaryRef dict);
	virtual CFStringRef copyDebugDesc();
	virtual void aboutToDestruct();
};

//
// A pointer type for SecCFObjects.
// T must be derived from SecCFObject.
//
class SecPointerBase
{
public:
	SecPointerBase() : ptr(NULL)
	{}
	SecPointerBase(const SecPointerBase& p);
	SecPointerBase(SecCFObject *p);
	~SecPointerBase();
	SecPointerBase& operator = (const SecPointerBase& p);

protected:
 	void assign(SecCFObject * p);
	void copy(SecCFObject * p);
	SecCFObject *ptr;
};

template <class T>
class SecPointer : public SecPointerBase
{
public:
	SecPointer() : SecPointerBase() {}
	SecPointer(const SecPointer& p) : SecPointerBase(p) {}
	SecPointer(T *p): SecPointerBase(p) {}
	SecPointer &operator =(T *p) { this->assign(p); return *this; }
	SecPointer &take(T *p) { this->copy(p); return *this; }
	T *yield() { T *result = static_cast<T *>(ptr); ptr = NULL; return result; }
	
	// dereference operations
    T* get () const				{ return static_cast<T*>(ptr); }	// mimic auto_ptr
	operator T * () const		{ return static_cast<T*>(ptr); }
	T * operator -> () const	{ return static_cast<T*>(ptr); }
	T & operator * () const		{ return *static_cast<T*>(ptr); }
};

template <class T>
bool operator <(const SecPointer<T> &r1, const SecPointer<T> &r2)
{
	T *p1 = r1.get(), *p2 = r2.get();
	return p1 && p2 ? *p1 < *p2 : p1 < p2;
}

template <class T>
bool operator ==(const SecPointer<T> &r1, const SecPointer<T> &r2)
{
	T *p1 = r1.get(), *p2 = r2.get();
	return p1 && p2 ? *p1 == *p2 : p1 == p2;
}

template <class T>
bool operator !=(const SecPointer<T> &r1, const SecPointer<T> &r2)
{
	T *p1 = r1.get(), *p2 = r2.get();
	return p1 && p2 ? *p1 != *p2 : p1 != p2;
}

} // end namespace Security


#endif

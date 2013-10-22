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


//
// alloc - abstract malloc-like allocator abstraction
//
#ifndef _H_ALLOC
#define _H_ALLOC

#include <security_utilities/utilities.h>
#include <cstring>

namespace Security
{


//
// An abstract allocator superclass, based on the simple malloc/realloc/free paradigm
// that CDSA loves so much. If you have an allocation strategy and want objects
// to be allocated through it, inherit from this.
//
class Allocator {
public:
	virtual ~Allocator();
	virtual void *malloc(size_t) throw(std::bad_alloc) = 0;
	virtual void free(void *) throw() = 0;
	virtual void *realloc(void *, size_t) throw(std::bad_alloc) = 0;

	//
	// Template versions for added expressiveness.
	// Note that the integers are element counts, not byte sizes.
	//
	template <class T> T *alloc() throw(std::bad_alloc)
	{ return reinterpret_cast<T *>(malloc(sizeof(T))); }

	template <class T> T *alloc(UInt32 count) throw(std::bad_alloc)
	{ return reinterpret_cast<T *>(malloc(sizeof(T) * count)); }

	template <class T> T *alloc(T *old, UInt32 count) throw(std::bad_alloc)
	{ return reinterpret_cast<T *>(realloc(old, sizeof(T) * count)); }
	
        
	//
	// Happier malloc/realloc for any type. Note that these still have
	// the original (byte-sized) argument profile.
	//
	template <class T> T *malloc(size_t size) throw(std::bad_alloc)
	{ return reinterpret_cast<T *>(malloc(size)); }
	
	template <class T> T *realloc(void *addr, size_t size) throw(std::bad_alloc)
	{ return reinterpret_cast<T *>(realloc(addr, size)); }

	// All right, if you *really* have to have calloc...
	void *calloc(size_t size, size_t count) throw(std::bad_alloc)
	{
		void *addr = malloc(size * count);
		memset(addr, 0, size * count);
		return addr;
	}
	
	// compare Allocators for identity
	virtual bool operator == (const Allocator &alloc) const throw();

public:
	// allocator chooser options
	enum {
		normal = 0x0000,
		sensitive = 0x0001
	};

	static Allocator &standard(UInt32 request = normal);
};


//
// You'd think that this is operator delete(const T *, Allocator &), but you'd
// be wrong. Specialized operator delete is only called during constructor cleanup.
// Use this to cleanly destroy things.
//
template <class T>
inline void destroy(T *obj, Allocator &alloc) throw()
{
	obj->~T();
	alloc.free(obj);
}

// untyped (release memory only, no destructor call)
inline void destroy(void *obj, Allocator &alloc) throw()
{
	alloc.free(obj);
}


//
// A mixin class to automagically manage your allocator.
// To allow allocation (of your object) from any instance of Allocator,
// inherit from CssmHeap. Your users can then create heap instances of your thing by
//		new (an-allocator) YourClass(...)
// or (still)
//		new YourClass(...)
// for the default allocation source. The beauty is that when someone does a
//		delete pointer-to-your-instance
// then the magic fairies will find the allocator that created the object and ask it
// to free the memory (by calling its free() method).
// The price of all that glory is memory overhead - typically one pointer per object.
//
class CssmHeap {
public:    
	void *operator new (size_t size, Allocator *alloc = NULL) throw(std::bad_alloc);
	void operator delete (void *addr, size_t size) throw();
	void operator delete (void *addr, size_t size, Allocator *alloc) throw();
};


//
// Here is a version of auto_ptr that works with Allocators. It is designed
// to be pretty much a drop-in replacement. It requires an allocator as a constructor
// argument, of course.
// Note that CssmAutoPtr<void> is perfectly valid, unlike its auto_ptr look-alike.
// You can't dereference it, naturally.
//
template <class T>
class CssmAutoPtr {
public:
	Allocator &allocator;

	CssmAutoPtr(Allocator &alloc = Allocator::standard())
	: allocator(alloc), mine(NULL) { }
	CssmAutoPtr(Allocator &alloc, T *p)
	: allocator(alloc), mine(p) { }
	CssmAutoPtr(T *p)
	: allocator(Allocator::standard()), mine(p) { }
	template <class T1> CssmAutoPtr(CssmAutoPtr<T1> &src)
	: allocator(src.allocator), mine(src.release()) { }
	template <class T1> CssmAutoPtr(Allocator &alloc, CssmAutoPtr<T1> &src)
	: allocator(alloc), mine(src.release()) { assert(allocator == src.allocator); }
	
	~CssmAutoPtr()				{ allocator.free(mine); }
	
	T *get() const throw()		{ return mine; }
	T *release()				{ T *result = mine; mine = NULL; return result; }
	void reset()				{ allocator.free(mine); mine = NULL; }

	operator T * () const		{ return mine; }
	T *operator -> () const		{ return mine; }
	T &operator * () const		{ assert(mine); return *mine; }

private:
	T *mine;
};

// specialization for void (i.e. void *), omitting the troublesome dereferencing ops.
template <>
class CssmAutoPtr<void> {
public:
	Allocator &allocator;

	CssmAutoPtr(Allocator &alloc) : allocator(alloc), mine(NULL) { }
	CssmAutoPtr(Allocator &alloc, void *p) : allocator(alloc), mine(p) { }
	template <class T1> CssmAutoPtr(CssmAutoPtr<T1> &src)
	: allocator(src.allocator), mine(src.release()) { }
	template <class T1> CssmAutoPtr(Allocator &alloc, CssmAutoPtr<T1> &src)
	: allocator(alloc), mine(src.release()) { assert(allocator == src.allocator); }
	
	~CssmAutoPtr()				{ destroy(mine, allocator); }
	
	void *get() throw()		{ return mine; }
	void *release()				{ void *result = mine; mine = NULL; return result; }
	void reset()				{ allocator.free(mine); mine = NULL; }

private:
	void *mine;
};


//
// Convenience forms of CssmAutoPtr that automatically make their (initial) object.
//
template <class T>
class CssmNewAutoPtr : public CssmAutoPtr<T> {
public:
	CssmNewAutoPtr(Allocator &alloc = Allocator::standard())
	: CssmAutoPtr<T>(alloc, new(alloc) T) { }
	
	template <class A1>
	CssmNewAutoPtr(Allocator &alloc, A1 &arg1) : CssmAutoPtr<T>(alloc, new(alloc) T(arg1)) { }
	template <class A1>
	CssmNewAutoPtr(Allocator &alloc, const A1 &arg1)
	: CssmAutoPtr<T>(alloc, new(alloc) T(arg1)) { }
	
	template <class A1, class A2>
	CssmNewAutoPtr(Allocator &alloc, A1 &arg1, A2 &arg2)
	: CssmAutoPtr<T>(alloc, new(alloc) T(arg1, arg2)) { }
	template <class A1, class A2>
	CssmNewAutoPtr(Allocator &alloc, const A1 &arg1, A2 &arg2)
	: CssmAutoPtr<T>(alloc, new(alloc) T(arg1, arg2)) { }
	template <class A1, class A2>
	CssmNewAutoPtr(Allocator &alloc, A1 &arg1, const A2 &arg2)
	: CssmAutoPtr<T>(alloc, new(alloc) T(arg1, arg2)) { }
	template <class A1, class A2>
	CssmNewAutoPtr(Allocator &alloc, const A1 &arg1, const A2 &arg2)
	: CssmAutoPtr<T>(alloc, new(alloc) T(arg1, arg2)) { }
};


} // end namespace Security


//
// Global C++ allocation hooks to use Allocators (global namespace)
//
inline void *operator new (size_t size, Allocator &allocator) throw (std::bad_alloc)
{ return allocator.malloc(size); }

inline void *operator new[] (size_t size, Allocator &allocator) throw (std::bad_alloc)
{ return allocator.malloc(size); }


#endif //_H_ALLOC

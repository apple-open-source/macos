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
// cssmalloc - memory allocation in the CDSA world
//
#ifndef _H_CSSMALLOC
#define _H_CSSMALLOC

#include <Security/utilities.h>
#include <Security/cssm.h>
#include <cstring>

#ifdef _CPP_CSSMALLOC
# pragma export on
#endif

namespace Security
{

//
// An abstract allocator superclass, based on the simple malloc/realloc/free paradigm
// that CDSA loves so much. If you have an allocation strategy and want objects
// to be allocated through it, inherit from this.
//
class CssmAllocator {
public:
	virtual ~CssmAllocator();
	virtual void *malloc(size_t) throw(std::bad_alloc) = 0;
	virtual void free(void *) throw() = 0;
	virtual void *realloc(void *, size_t) throw(std::bad_alloc) = 0;

	//
	// Template versions for added expressiveness.
	// Note that the integers are element counts, not byte sizes.
	//
	template <class T> T *alloc() throw(std::bad_alloc)
	{ return reinterpret_cast<T *>(malloc(sizeof(T))); }

	template <class T> T *alloc(uint32 count) throw(std::bad_alloc)
	{ return reinterpret_cast<T *>(malloc(sizeof(T) * count)); }

	template <class T> T *alloc(T *old, uint32 count) throw(std::bad_alloc)
	{ return reinterpret_cast<T *>(realloc(old, sizeof(T) * count)); }
	
	template <class Data> CssmData alloc(const Data &source) throw(std::bad_alloc)
	{
		size_t length = source.length();
		return CssmData(memcpy(malloc(length), source.data(), length), length);
	}
	
	//
	// Happier malloc/realloc for any type. Note that these still have
	// the original (byte-sized) argument profile.
	//
	template <class T> T *malloc(size_t size) throw(std::bad_alloc)
	{ return reinterpret_cast<T *>(malloc(size)); }
	
	template <class T> T *realloc(void *addr, size_t size) throw(std::bad_alloc)
	{ return reinterpret_cast<T *>(realloc(addr, size)); }

	// All right, if you *really* have to have calloc...
	void *calloc(size_t size, unsigned int count) throw(std::bad_alloc)
	{
		void *addr = malloc(size * count);
		memset(addr, 0, size * count);
		return addr;
	}
	
	// compare CssmAllocators for identity
	virtual bool operator == (const CssmAllocator &alloc) const throw();

public:
	// allocator chooser options
	enum {
		normal = 0x0000,
		sensitive = 0x0001
	};

	static CssmAllocator &standard(uint32 request = normal);
};


//
// A POD wrapper for the memory functions structure passed around in CSSM.
//
class CssmMemoryFunctions : public PodWrapper<CssmMemoryFunctions, CSSM_MEMORY_FUNCS> {
public:
	CssmMemoryFunctions(const CSSM_MEMORY_FUNCS &funcs)
	{ *(CSSM_MEMORY_FUNCS *)this = funcs; }
	CssmMemoryFunctions() { }

	void *malloc(size_t size) const throw(std::bad_alloc);
	void free(void *mem) const throw() { free_func(mem, AllocRef); }
	void *realloc(void *mem, size_t size) const throw(std::bad_alloc);
	void *calloc(uint32 count, size_t size) const throw(std::bad_alloc);
	
	bool operator == (const CSSM_MEMORY_FUNCS &other) const throw()
	{ return !memcmp(this, &other, sizeof(*this)); }
};

inline void *CssmMemoryFunctions::malloc(size_t size) const throw(std::bad_alloc)
{
	if (void *addr = malloc_func(size, AllocRef))
		return addr;
	throw std::bad_alloc();
}

inline void *CssmMemoryFunctions::calloc(uint32 count, size_t size) const throw(std::bad_alloc)
{
	if (void *addr = calloc_func(count, size, AllocRef))
		return addr;
	throw std::bad_alloc();
}

inline void *CssmMemoryFunctions::realloc(void *mem, size_t size) const throw(std::bad_alloc)
{
	if (void *addr = realloc_func(mem, size, AllocRef))
		return addr;
	throw std::bad_alloc();
}


//
// A CssmAllocator based on CssmMemoryFunctions
//
class CssmMemoryFunctionsAllocator : public CssmAllocator {
public:
	CssmMemoryFunctionsAllocator(const CssmMemoryFunctions &memFuncs) : functions(memFuncs) { }
	
	void *malloc(size_t size) throw(std::bad_alloc);
	void free(void *addr) throw();
	void *realloc(void *addr, size_t size) throw(std::bad_alloc);
	
	operator const CssmMemoryFunctions & () const throw() { return functions; }

private:
	const CssmMemoryFunctions functions;
};

} // end namespace Security

//
// Global C++ allocation hooks to use CssmAllocators
//
inline void *operator new (size_t size, CssmAllocator &allocator) throw(std::bad_alloc)
{ return allocator.malloc(size); }

//
// You'd think that this is operator delete(const T *, CssmAllocator &), but you'd
// be wrong. Specialized operator delete is only called during constructor cleanup.
// Use this to cleanly destroy things.
//
template <class T>
inline void destroy(T *obj, CssmAllocator &alloc) throw()
{
	obj->~T();
	alloc.free(obj);
}

// untyped (release memory only, no destructor call)
inline void destroy(void *obj, CssmAllocator &alloc) throw()
{
	alloc.free(obj);
}

namespace Security
{

//
// A MemoryFunctions object based on a CssmAllocator.
// Note that we don't copy the CssmAllocator object. It needs to live (at least)
// as long as any CssmAllocatorMemoryFunctions object based on it.
//
class CssmAllocatorMemoryFunctions : public CssmMemoryFunctions {
public:
	CssmAllocatorMemoryFunctions(CssmAllocator &alloc);
	CssmAllocatorMemoryFunctions() { /*IFDEBUG(*/ AllocRef = NULL /*)*/ ; }	// later assignment req'd
	
private:
	static void *relayMalloc(size_t size, void *ref) throw(std::bad_alloc);
	static void relayFree(void *mem, void *ref) throw();
	static void *relayRealloc(void *mem, size_t size, void *ref) throw(std::bad_alloc);
	static void *relayCalloc(uint32 count, size_t size, void *ref) throw(std::bad_alloc);

	static CssmAllocator &allocator(void *ref) throw()
	{ return *reinterpret_cast<CssmAllocator *>(ref); }
};


//
// A mixin class to automagically manage your allocator.
// To allow allocation (of your object) from any instance of CssmAllocator,
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
	void *operator new (size_t size, CssmAllocator *alloc = NULL) throw(std::bad_alloc);
	void operator delete (void *addr, size_t size) throw();
	void operator delete (void *addr, size_t size, CssmAllocator *alloc) throw();
};


//
// Here is a version of auto_ptr that works with CssmAllocators. It is designed
// to be pretty much a drop-in replacement. It requires an allocator as a constructor
// argument, of course.
// Note that CssmAutoPtr<void> is perfectly valid, unlike its auto_ptr look-alike.
// You can't dereference it, naturally.
//
template <class T>
class CssmAutoPtr {
public:
	CssmAllocator &allocator;

	CssmAutoPtr(CssmAllocator &alloc = CssmAllocator::standard())
	: allocator(alloc), mine(NULL) { }
	CssmAutoPtr(CssmAllocator &alloc, T *p)
	: allocator(alloc), mine(p) { }
	CssmAutoPtr(T *p)
	: allocator(CssmAllocator::standard()), mine(p) { }
	template <class T1> CssmAutoPtr(CssmAutoPtr<T1> &src)
	: allocator(src.allocator), mine(src.release()) { }
	template <class T1> CssmAutoPtr(CssmAllocator &alloc, CssmAutoPtr<T1> &src)
	: allocator(alloc), mine(rc.release()) { assert(allocator == src.allocator); }
	
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
	CssmAllocator &allocator;

	CssmAutoPtr(CssmAllocator &alloc) : allocator(alloc), mine(NULL) { }
	CssmAutoPtr(CssmAllocator &alloc, void *p) : allocator(alloc), mine(p) { }
	template <class T1> CssmAutoPtr(CssmAutoPtr<T1> &src)
	: allocator(src.allocator), mine(src.release()) { }
	template <class T1> CssmAutoPtr(CssmAllocator &alloc, CssmAutoPtr<T1> &src)
	: allocator(alloc), mine(rc.release()) { assert(allocator == src.allocator); }
	
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
	CssmNewAutoPtr(CssmAllocator &alloc = CssmAllocator::standard())
	: CssmAutoPtr<T>(alloc, new(alloc) T) { }
	
	template <class A1>
	CssmNewAutoPtr(CssmAllocator &alloc, A1 &arg1) : CssmAutoPtr<T>(alloc, new(alloc) T(arg1)) { }
	template <class A1>
	CssmNewAutoPtr(CssmAllocator &alloc, const A1 &arg1)
	: CssmAutoPtr<T>(alloc, new(alloc) T(arg1)) { }
	
	template <class A1, class A2>
	CssmNewAutoPtr(CssmAllocator &alloc, A1 &arg1, A2 &arg2)
	: CssmAutoPtr<T>(alloc, new(alloc) T(arg1, arg2)) { }
	template <class A1, class A2>
	CssmNewAutoPtr(CssmAllocator &alloc, const A1 &arg1, A2 &arg2)
	: CssmAutoPtr<T>(alloc, new(alloc) T(arg1, arg2)) { }
	template <class A1, class A2>
	CssmNewAutoPtr(CssmAllocator &alloc, A1 &arg1, const A2 &arg2)
	: CssmAutoPtr<T>(alloc, new(alloc) T(arg1, arg2)) { }
	template <class A1, class A2>
	CssmNewAutoPtr(CssmAllocator &alloc, const A1 &arg1, const A2 &arg2)
	: CssmAutoPtr<T>(alloc, new(alloc) T(arg1, arg2)) { }
};


//
// A generic helper for the unhappily ubiquitous CSSM-style
// (count, pointer-to-array) style of arrays.
//
template <class Base, class Wrapper = Base>
class CssmVector {
public:
    CssmVector(uint32 &cnt, Base * &vec, CssmAllocator &alloc = CssmAllocator::standard())
        : count(cnt), vector(reinterpret_cast<Wrapper * &>(vec)),
          allocator(alloc)
    {
        count = 0;
        vector = NULL;
    }
    
    ~CssmVector()	{ allocator.free(vector); }
        
    uint32 &count;
    Wrapper * &vector;
    CssmAllocator &allocator;

public:
    Wrapper &operator [] (uint32 ix)
    { assert(ix < count); return vector[ix]; }
    
    void operator += (const Wrapper &add)
    {
        vector = reinterpret_cast<Wrapper *>(allocator.realloc(vector, (count + 1) * sizeof(Wrapper)));
        //@@@???compiler bug??? vector = allocator.alloc<Wrapper>(vector, count + 1);
        vector[count++] = add;
    }
};


} // end namespace Security

#ifdef _CPP_CSSMALLOC
# pragma export off
#endif

#endif //_H_CSSMALLOC

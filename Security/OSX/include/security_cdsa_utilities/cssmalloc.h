/*
 * Copyright (c) 2000-2004,2006,2011,2014 Apple Inc. All Rights Reserved.
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
// cssmalloc - memory allocation in the CDSA world
//
#ifndef _H_CSSMALLOC
#define _H_CSSMALLOC

#include <security_utilities/alloc.h>
#include <Security/cssm.h>
#include <cstring>


namespace Security
{


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
// A Allocator based on CssmMemoryFunctions
//
class CssmMemoryFunctionsAllocator : public Allocator {
public:
	CssmMemoryFunctionsAllocator(const CssmMemoryFunctions &memFuncs) : functions(memFuncs) { }
	
	void *malloc(size_t size) throw(std::bad_alloc);
	void free(void *addr) throw();
	void *realloc(void *addr, size_t size) throw(std::bad_alloc);
	
	operator const CssmMemoryFunctions & () const throw() { return functions; }

private:
	const CssmMemoryFunctions functions;
};


//
// A MemoryFunctions object based on a Allocator.
// Note that we don't copy the Allocator object. It needs to live (at least)
// as long as any CssmAllocatorMemoryFunctions object based on it.
//
class CssmAllocatorMemoryFunctions : public CssmMemoryFunctions {
public:
	CssmAllocatorMemoryFunctions(Allocator &alloc);
	CssmAllocatorMemoryFunctions() { /*IFDEBUG(*/ AllocRef = NULL /*)*/ ; }	// later assignment req'd
	
private:
	static void *relayMalloc(size_t size, void *ref) throw(std::bad_alloc);
	static void relayFree(void *mem, void *ref) throw();
	static void *relayRealloc(void *mem, size_t size, void *ref) throw(std::bad_alloc);
	static void *relayCalloc(uint32 count, size_t size, void *ref) throw(std::bad_alloc);

	static Allocator &allocator(void *ref) throw()
	{ return *reinterpret_cast<Allocator *>(ref); }
};


//
// A generic helper for the unhappily ubiquitous CSSM-style
// (count, pointer-to-array) style of arrays.
//
template <class Base, class Wrapper = Base>
class CssmVector {
public:
    CssmVector(uint32 &cnt, Base * &vec, Allocator &alloc = Allocator::standard())
        : count(cnt), vector(reinterpret_cast<Wrapper * &>(vec)),
          allocator(alloc)
    {
        count = 0;
        vector = NULL;
    }
    
    ~CssmVector()	{ allocator.free(vector); }
        
    uint32 &count;
    Wrapper * &vector;
    Allocator &allocator;

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

#endif //_H_CSSMALLOC

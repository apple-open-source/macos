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
// cssmalloc - memory allocation in the CDSA world.
//
// Don't eat heavily before inspecting this code.
//
#include <Security/cssmalloc.h>
#include <Security/memutils.h>
#include <Security/globalizer.h>
#include <Security/trackingallocator.h>
#include <stdlib.h>
#include <errno.h>

using LowLevelMemoryUtilities::alignof;
using LowLevelMemoryUtilities::increment;
using LowLevelMemoryUtilities::alignUp;

extern "C" size_t malloc_size(void *);


//
// Features of the CssmAllocator root class
//
bool CssmAllocator::operator == (const CssmAllocator &alloc) const throw()
{
	return this == &alloc;
}

CssmAllocator::~CssmAllocator()
{
}


//
// Standard CssmAllocator variants.
// Note that all calls to CssmAllocator::standard(xxx) with the same xxx argument
// must produce compatible allocators (i.e. they must be work on a common memory
// pool). This is trivially achieved here by using singletons.
//
struct DefaultCssmAllocator : public CssmAllocator {
	void *malloc(size_t size) throw(std::bad_alloc);
	void free(void *addr) throw();
	void *realloc(void *addr, size_t size) throw(std::bad_alloc);
};

struct SensitiveCssmAllocator : public DefaultCssmAllocator {
    void free(void *addr) throw();
    void *realloc(void *addr, size_t size) throw(std::bad_alloc);
};

struct DefaultAllocators {
    DefaultCssmAllocator standard;
    SensitiveCssmAllocator sensitive;
};

static ModuleNexus<DefaultAllocators> defaultAllocators;


CssmAllocator &CssmAllocator::standard(uint32 request)
{
    switch (request) {
    case normal:
        return defaultAllocators().standard;
    case sensitive:
        return defaultAllocators().sensitive;
    default:
        CssmError::throwMe(CSSM_ERRCODE_MEMORY_ERROR);
    }
}

void *DefaultCssmAllocator::malloc(size_t size) throw(std::bad_alloc)
{
	if (void *result = ::malloc(size))
		return result;
	throw std::bad_alloc();
}

void DefaultCssmAllocator::free(void *addr) throw()
{
	::free(addr);
}

void *DefaultCssmAllocator::realloc(void *addr, size_t newSize) throw(std::bad_alloc)
{
	if (void *result = ::realloc(addr, newSize))
		return result;
	throw std::bad_alloc();
}

void SensitiveCssmAllocator::free(void *addr) throw()
{
    memset(addr, 0, malloc_size(addr));
    DefaultCssmAllocator::free(addr);
}

void *SensitiveCssmAllocator::realloc(void *addr, size_t newSize) throw(std::bad_alloc)
{
    size_t oldSize = malloc_size(addr);
    if (newSize < oldSize)
        memset(increment(addr, newSize), 0, oldSize - newSize);
    return DefaultCssmAllocator::realloc(addr, newSize);
}

TrackingAllocator::~TrackingAllocator()
{
	AllocSet::iterator first = mAllocSet.begin(), last = mAllocSet.end();
	for (; first != last; ++first)
		mAllocator.free(*first);
}

//
// CssmMemoryFunctionsAllocators
//
void *CssmMemoryFunctionsAllocator::malloc(size_t size) throw(std::bad_alloc)
{ return functions.malloc(size); }

void CssmMemoryFunctionsAllocator::free(void *addr) throw()
{ return functions.free(addr); }

void *CssmMemoryFunctionsAllocator::realloc(void *addr, size_t size) throw(std::bad_alloc)
{ return functions.realloc(addr, size); }


//
// CssmAllocatorMemoryFunctions
//
CssmAllocatorMemoryFunctions::CssmAllocatorMemoryFunctions(CssmAllocator &alloc)
{
	AllocRef = &alloc;
	malloc_func = relayMalloc;
	free_func = relayFree;
	realloc_func = relayRealloc;
	calloc_func = relayCalloc;
}

void *CssmAllocatorMemoryFunctions::relayMalloc(size_t size, void *ref) throw(std::bad_alloc)
{ return allocator(ref).malloc(size); }

void CssmAllocatorMemoryFunctions::relayFree(void *mem, void *ref) throw()
{ allocator(ref).free(mem); }

void *CssmAllocatorMemoryFunctions::relayRealloc(void *mem, size_t size, void *ref) throw(std::bad_alloc)
{ return allocator(ref).realloc(mem, size); }

void *CssmAllocatorMemoryFunctions::relayCalloc(uint32 count, size_t size, void *ref) throw(std::bad_alloc)
{
	// CssmAllocator doesn't have a calloc() method
	void *mem = allocator(ref).malloc(size * count);
	memset(mem, 0, size * count);
	return mem;
}


//
// Memory allocators for CssmHeap objects.
// This implementation stores a pointer to the allocator used into memory
// *after* the object's proper storage block. This allows the usual free()
// functions to safely free our (hidden) pointer without knowing about it.
// An allocator argument of NULL is interpreted as the standard allocator.
//
void *CssmHeap::operator new (size_t size, CssmAllocator *alloc) throw(std::bad_alloc)
{
	if (alloc == NULL)
		alloc = &CssmAllocator::standard();
	size = alignUp(size, alignof<CssmAllocator *>());
	size_t totalSize = size + sizeof(CssmAllocator *);
	void *addr = alloc->malloc(totalSize);
	*(CssmAllocator **)increment(addr, size) = alloc;
	return addr;
}

void CssmHeap::operator delete (void *addr, size_t size, CssmAllocator *alloc) throw()
{
	alloc->free(addr);	// as per C++ std, called (only) if construction fails
}

void CssmHeap::operator delete (void *addr, size_t size) throw()
{
	void *end = increment(addr, alignUp(size, alignof<CssmAllocator *>()));
	(*(CssmAllocator **)end)->free(addr);
}


//
// CssmVector
//

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
// cssmalloc - memory allocation in the CDSA world.
//
// Don't eat heavily before inspecting this code.
//
#include <security_utilities/alloc.h>
#include <security_utilities/memutils.h>
#include <security_utilities/globalizer.h>
#include <stdlib.h>
#include <errno.h>

using LowLevelMemoryUtilities::alignof;
using LowLevelMemoryUtilities::increment;
using LowLevelMemoryUtilities::alignUp;

extern "C" size_t malloc_size(void *);


//
// Features of the Allocator root class
//
bool Allocator::operator == (const Allocator &alloc) const throw()
{
	return this == &alloc;
}

Allocator::~Allocator()
{
}


//
// Standard Allocator variants.
// Note that all calls to Allocator::standard(xxx) with the same xxx argument
// must produce compatible allocators (i.e. they must be work on a common memory
// pool). This is trivially achieved here by using singletons.
//
struct DefaultAllocator : public Allocator {
	void *malloc(size_t size) throw(std::bad_alloc);
	void free(void *addr) throw();
	void *realloc(void *addr, size_t size) throw(std::bad_alloc);
};

struct SensitiveAllocator : public DefaultAllocator {
    void free(void *addr) throw();
    void *realloc(void *addr, size_t size) throw(std::bad_alloc);
};

struct DefaultAllocators {
    DefaultAllocator standard;
    SensitiveAllocator sensitive;
};

static ModuleNexus<DefaultAllocators> defaultAllocators;


Allocator &Allocator::standard(UInt32 request)
{
    switch (request) {
    case normal:
        return defaultAllocators().standard;
    case sensitive:
        return defaultAllocators().sensitive;
    default:
        UnixError::throwMe(ENOMEM);
    }
}

void *DefaultAllocator::malloc(size_t size) throw(std::bad_alloc)
{
	if (void *result = ::malloc(size))
		return result;
	throw std::bad_alloc();
}

void DefaultAllocator::free(void *addr) throw()
{
	::free(addr);
}

void *DefaultAllocator::realloc(void *addr, size_t newSize) throw(std::bad_alloc)
{
	if (void *result = ::realloc(addr, newSize))
		return result;
	throw std::bad_alloc();
}

void SensitiveAllocator::free(void *addr) throw()
{
	if (addr != NULL)
	{
		memset(addr, 0, malloc_size(addr));
		DefaultAllocator::free(addr);
	}
}

void *SensitiveAllocator::realloc(void *addr, size_t newSize) throw(std::bad_alloc)
{
    size_t oldSize = malloc_size(addr);
    if (newSize < oldSize)
        memset(increment(addr, newSize), 0, oldSize - newSize);
    return DefaultAllocator::realloc(addr, newSize);
}


//
// Memory allocators for CssmHeap objects.
// This implementation stores a pointer to the allocator used into memory
// *after* the object's proper storage block. This allows the usual free()
// functions to safely free our (hidden) pointer without knowing about it.
// An allocator argument of NULL is interpreted as the standard allocator.
//
void *CssmHeap::operator new (size_t size, Allocator *alloc) throw(std::bad_alloc)
{
	if (alloc == NULL)
		alloc = &Allocator::standard();
	size = alignUp(size, alignof<Allocator *>());
	size_t totalSize = size + sizeof(Allocator *);
	void *addr = alloc->malloc(totalSize);
	*(Allocator **)increment(addr, size) = alloc;
	return addr;
}

void CssmHeap::operator delete (void *addr, size_t size, Allocator *alloc) throw()
{
	alloc->free(addr);	// as per C++ std, called (only) if construction fails
}

void CssmHeap::operator delete (void *addr, size_t size) throw()
{
	void *end = increment(addr, alignUp(size, alignof<Allocator *>()));
	(*(Allocator **)end)->free(addr);
}


//
// CssmVector
//

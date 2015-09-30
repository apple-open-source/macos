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
// cssmalloc - memory allocation in the CDSA world.
//
// Don't eat heavily before inspecting this code.
//
#include <security_cdsa_utilities/cssmalloc.h>
#include <stdlib.h>
#include <errno.h>



namespace Security {


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
CssmAllocatorMemoryFunctions::CssmAllocatorMemoryFunctions(Allocator &alloc)
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
	// Allocator doesn't have a calloc() method
	void *mem = allocator(ref).malloc(size * count);
	memset(mem, 0, size * count);
	return mem;
}


//
// CssmVector
//


}   // namespace Security

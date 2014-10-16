/*
 * Copyright (c) 2000-2004,2011,2014 Apple Inc. All Rights Reserved.
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
// trackingallocator - Allocator that keeps track of allocations
//
#ifndef _H_TRACKINGALLOCATOR
#define _H_TRACKINGALLOCATOR

#include <security_utilities/alloc.h>
#include <set>

namespace Security
{

//
// A Allocator that keeps track of allocations and can throw everything
// away unless explicitly committed.
//
class TrackingAllocator : public Allocator
{
public:
	TrackingAllocator(Allocator &inAllocator) : subAllocator(inAllocator) {}
	virtual ~TrackingAllocator();
	
	Allocator &subAllocator;

	// standard Allocator operations
	void *malloc(size_t inSize) throw(std::bad_alloc);
	void free(void *inAddress) throw();
	void *realloc(void *inAddress, size_t inNewSize) throw(std::bad_alloc);

	// reset frees all memory; commit forgets about it all
	void reset();
	void commit();
	
private:
	typedef std::set<void *> AllocSet;
	AllocSet mAllocSet;
};

} // end namespace Security

#endif //_H_TRACKINGALLOCATOR

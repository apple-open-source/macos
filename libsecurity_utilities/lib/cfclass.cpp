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

#include <security_utilities/cfclass.h>
#include <security_utilities/seccfobject.h>
#include <security_utilities/threading.h>


//
// CFClass
//
CFClass::CFClass(const char *name, CFAllocator *anAllocator)
{
	// If we are given anAllocator it does the work of calling finalizeType
	// from it's dealloc method.
	if (anAllocator)
	{
		allocator = anAllocator->allocator;
		finalize = NULL;
	}
	else
	{
		allocator = NULL;
		finalize = finalizeType;
	}
	
	// Initialize the remainder of the CFRuntimeClass structure
	version = 0;
	className = name;
	init = NULL;
	copy = NULL;

	equal = equalType;
	hash = hashType;
	copyFormattingDesc = copyFormattingDescType;
	copyDebugDesc = copyDebugDescType;

	// register
	typeID = _CFRuntimeRegisterClass(this);
	assert(typeID != _kCFRuntimeNotATypeID);
}

void
CFClass::finalizeType(CFTypeRef cf) throw()
{
	SecCFObject *obj = SecCFObject::optional(cf);
	if (!obj->isNew())
	{
		try {
			// Call the destructor.
			obj->~SecCFObject();
		} catch (...) {}
	}
}

Boolean
CFClass::equalType(CFTypeRef cf1, CFTypeRef cf2) throw()
{
	// CF checks for pointer equality and ensures type equality already
	return SecCFObject::optional(cf1)->equal(*SecCFObject::optional(cf2));
}

CFHashCode
CFClass::hashType(CFTypeRef cf) throw()
{
	return SecCFObject::optional(cf)->hash();
}

CFStringRef
CFClass::copyFormattingDescType(CFTypeRef cf, CFDictionaryRef dict) throw()
{
	return SecCFObject::optional(cf)->copyFormattingDesc(dict);
}

CFStringRef
CFClass::copyDebugDescType(CFTypeRef cf) throw()
{
	return SecCFObject::optional(cf)->copyDebugDesc();
}


//
// CFAllocator
//
CFAllocator::CFAllocator(Mutex &lock) :
mLock(lock)
{
	mContext.version = 0;
	mContext.info = this;
	mContext.retain = NULL;
	mContext.release = NULL;
	mContext.copyDescription = NULL;
	mContext.allocate = allocate;
	mContext.reallocate = reallocate;
	mContext.deallocate = deallocate;
	mContext.preferredSize = preferredSize;
	
 	allocator = CFAllocatorCreate(NULL, &mContext);
}

void *
CFAllocator::allocate(CFIndex allocSize, CFOptionFlags hint, void *info) throw()
{
	return malloc(allocSize);
}

void *
CFAllocator::reallocate(void *ptr, CFIndex newsize, CFOptionFlags hint, void *info) throw()
{
	return realloc(ptr, newsize);
}

void
CFAllocator::deallocate(void *ptr, void *info) throw()
{
    /* Called by CFRelease.  The runtime object we are dealing with has no finalize callback.  Instead we lock mLock here and check to see if the objects retaincount is still 1 after that.  If not then some other thread must of grabed the object and CFRetained it.  In that case we do what CFRelease would of done if the retainCount had not just hit 0.  If the retaincount is still 1 however we actaully finalize the object and free it's storage.  */
	CFAllocator &allocator = *reinterpret_cast<CFAllocator *>(info);
	CFTypeRef cf = reinterpret_cast<CFTypeRef>(reinterpret_cast<intptr_t>(ptr) + sizeof(CFAllocatorRef));

	// Lock the external lock
    allocator.mLock.lock();
	CFIndex rc = CFGetRetainCount(cf);
	if (rc == 1)
	{
		// The retainCount is still 1, so call the objects destructor,
		// release the lock and free the memory.
		CFClass::finalizeType(cf);
		allocator.mLock.unlock();
		free(ptr);
	}
	else
	{
		// The retainCount was no longer 1, so release the lock and do what CFRetain would of
		// done if the retainCount was 2 or higher to being with.  Also counter the CFRelease
		// on the alloctor which CFRelease will do when we return.
		allocator.mLock.unlock();
		//CFRetain(CFGetAllocator(cf));
		CFRetain(allocator.allocator);
		CFRelease(cf);
	}
}

CFIndex
CFAllocator::preferredSize(CFIndex size, CFOptionFlags hint, void *info) throw()
{
	//return malloc_good_size(size);
	return size;
}

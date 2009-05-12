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

#include <security_utilities/seccfobject.h>
#include <security_utilities/cfclass.h>
#include <security_utilities/errors.h>
#include <security_utilities/debugging.h>

#include <list>
#include <security_utilities/globalizer.h>

SecPointerBase::SecPointerBase(const SecPointerBase& p)
{
	if (p.ptr)
	{
		CFRetain(p.ptr->operator CFTypeRef());
	}
	ptr = p.ptr;
}




SecPointerBase::SecPointerBase(SecCFObject *p)
{
	if (p && !p->isNew())
	{
		CFRetain(p->operator CFTypeRef());
	}
	ptr = p;
}



SecPointerBase::~SecPointerBase()
{
	if (ptr)
	{
		CFRelease(ptr->operator CFTypeRef());
	}
}



SecPointerBase& SecPointerBase::operator = (const SecPointerBase& p)
{
	if (p.ptr)
	{
		CFRetain(p.ptr->operator CFTypeRef());
	}
	if (ptr)
	{
		CFRelease(ptr->operator CFTypeRef());
	}
	ptr = p.ptr;
	return *this;
}



void SecPointerBase::assign(SecCFObject * p)
{
	if (p && !p->isNew())
	{
		CFRetain(p->operator CFTypeRef());
	}
	if (ptr)
	{
		CFRelease(ptr->operator CFTypeRef());
	}
	ptr = p;
}



void SecPointerBase::copy(SecCFObject * p)
{
	if (ptr)
	{
		CFRelease(ptr->operator CFTypeRef());
	}
	
	ptr = p;
}



// define information about a deletable object
typedef std::list<CFTypeRef> DeferredObjectList;

struct DeferredObjectInfo
{
	DeferredObjectList objectList;
};



ModuleNexus<DeferredObjectInfo> gDeferredObjects;

//
// SecCFObject
//
SecCFObject *
SecCFObject::optional(CFTypeRef cfTypeRef) throw()
{
	if (!cfTypeRef)
		return NULL;

	return const_cast<SecCFObject *>(reinterpret_cast<const SecCFObject *>(reinterpret_cast<const uint8_t *>(cfTypeRef) + kAlignedRuntimeSize));
}

SecCFObject *
SecCFObject::required(CFTypeRef cfTypeRef, OSStatus error)
{
	SecCFObject *object = optional(cfTypeRef);
	if (!object)
		MacOSError::throwMe(error);

	return object;
}

void
SecCFObject::clearDeletedObjects() throw()
{
	// we don't need to worry about protecting this code as it is only called from a Sec call, which is protected
	// by the global mutex
	DeferredObjectList::iterator it;
	it = gDeferredObjects().objectList.begin();
	
	while (it != gDeferredObjects().objectList.end())
	{
		DeferredObjectList::iterator current = it++;
		CFTypeRef t = *current;
		CFIndex rCount = CFGetRetainCount(t);
		
		if (rCount == 1)
		{
			CFRelease(t);
			gDeferredObjects().objectList.erase(current);
		}
	}
}
				
void *
SecCFObject::allocate(size_t size, const CFClass &cfclass) throw(std::bad_alloc)
{
	CFTypeRef p = _CFRuntimeCreateInstance(NULL, cfclass.typeID,
		size + kAlignedRuntimeSize - sizeof(CFRuntimeBase), NULL);
	if (p == NULL)
		throw std::bad_alloc();

	if (cfclass.isDeferrable)
	{
		gDeferredObjects().objectList.push_front(p);
		CFRetain(p);
	}
	
	((SecRuntimeBase*) p)->isNew = true;

	void *q = ((u_int8_t*) p) + kAlignedRuntimeSize;

#if !defined(NDEBUG)
	const CFRuntimeClass *runtimeClass = _CFRuntimeGetClassWithTypeID(cfclass.typeID);
	secdebug("sec", "allocated: %p: %s(%lu)", q,
		runtimeClass && runtimeClass->className ? runtimeClass->className
		: "SecCFObject", cfclass.typeID);
#endif
	return q;
}

void
SecCFObject::operator delete(void *object) throw()
{
	CFTypeRef cfType = reinterpret_cast<CFTypeRef>(reinterpret_cast<const uint8_t *>(object) - kAlignedRuntimeSize);
	CFRelease(cfType);
}

SecCFObject::~SecCFObject()
{
#if !defined(NDEBUG)
	CFTypeRef cfType = *this;
	CFTypeID typeID = CFGetTypeID(cfType);
	const CFRuntimeClass *runtimeClass = _CFRuntimeGetClassWithTypeID(typeID);
	secdebug("sec", "destroyed: %p: %s(%lu)", this,
		runtimeClass && runtimeClass->className ? runtimeClass->className
		: "SecCFObject", typeID);
#endif
}

bool
SecCFObject::equal(SecCFObject &other)
{
	return this == &other;
}

CFHashCode
SecCFObject::hash()
{
	return CFHashCode(this);
}

CFStringRef
SecCFObject::copyFormattingDesc(CFDictionaryRef dict)
{
	return NULL;
}

CFStringRef
SecCFObject::copyDebugDesc()
{
	return NULL;
}

CFTypeRef
SecCFObject::handle(bool retain) throw()
{
	CFTypeRef cfType = *this;
	if (retain && !isNew()) CFRetain(cfType);
	return cfType;
}

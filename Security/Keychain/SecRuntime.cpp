/*
 * Copyright (c) 2002 Apple Computer, Inc. All Rights Reserved.
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
// SecRuntime.cpp - CF runtime interface
//

#include <Security/SecRuntime.h>

#ifndef NDEBUG
#include <Security/debugging.h>
#endif

using namespace KeychainCore;

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

void *
SecCFObject::allocate(size_t size, CFTypeID typeID) throw(std::bad_alloc)
{
	void *p = const_cast<void *>(_CFRuntimeCreateInstance(gTypes().allocator, typeID,
		size + kAlignedRuntimeSize - sizeof(CFRuntimeBase), NULL));
	if (p == NULL)
		throw std::bad_alloc();

	reinterpret_cast<SecRuntimeBase *>(p)->isNew = true;

	void *q = reinterpret_cast<void *>(reinterpret_cast<uint8_t *>(p) + kAlignedRuntimeSize);

	secdebug("sec", "SecCFObject allocated %p of type %lu", q, typeID);

	return q;
}

void
SecCFObject::operator delete(void *object) throw()
{
	secdebug("sec", "SecCFObject operator delete %p", object);
	CFTypeRef cfType = reinterpret_cast<CFTypeRef>(reinterpret_cast<const uint8_t *>(object) - kAlignedRuntimeSize);
	CFRelease(cfType);
}

SecCFObject::~SecCFObject() throw()
{
	secdebug("sec", "SecCFObject::~SecCFObject %p", this);
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

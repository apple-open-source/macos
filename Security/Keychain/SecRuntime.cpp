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
#include <Security/SecCFTypes.h>

using namespace KeychainCore;

//
// SecCFObject
//
SecCFObject::~SecCFObject()
{
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


//
// SecCFType
//
SecCFType::SecCFType(SecCFObject *obj) :
	mObject(obj)
{
}

SecCFType::~SecCFType()
{
	mObject = NULL;
}

//
// CFClassBase
//
CFClassBase::CFClassBase(const char *name)
{
	// initialize the CFRuntimeClass structure
	version = 0;
	className = name;
	init = NULL;
	copy = NULL;
	finalize = finalizeType;
	equal = equalType;
	hash = hashType;
	copyFormattingDesc = NULL;
	copyDebugDesc = NULL;
	
	// register
	typeId = _CFRuntimeRegisterClass(this);
	assert(typeId != _kCFRuntimeNotATypeID);
}
    
void
CFClassBase::finalizeType(CFTypeRef cf)
{
	const SecCFType *type = reinterpret_cast<const SecCFType *>(cf);
	StLock<Mutex> _(gTypes().mapLock);
	gTypes().map.erase(type->mObject.get());
    type->~SecCFType();
}
    
Boolean
CFClassBase::equalType(CFTypeRef cf1, CFTypeRef cf2)
{
	const SecCFType *t1 = reinterpret_cast<const SecCFType *>(cf1); 
	const SecCFType *t2 = reinterpret_cast<const SecCFType *>(cf2);
	// CF checks for pointer equality and ensures type equality already
	return t1->mObject->equal(*t2->mObject);
}

CFHashCode
CFClassBase::hashType(CFTypeRef cf)
{
	return reinterpret_cast<const SecCFType *>(cf)->mObject->hash();
}

const SecCFType *
CFClassBase::makeNew(SecCFObject *obj)
{
	void *p = const_cast<void *>(_CFRuntimeCreateInstance(NULL, typeId,
		sizeof(SecCFType) - sizeof(CFRuntimeBase), NULL));
	new (p) SecCFType(obj);
	return reinterpret_cast<const SecCFType *>(p);
}

const SecCFType *
CFClassBase::handle(SecCFObject *obj)
{
	SecCFTypes::Map &map = gTypes().map;
	StLock<Mutex> _(gTypes().mapLock);
	SecCFTypes::Map::const_iterator it = map.find(obj);
	if (it == map.end())
	{
		const SecCFType *p = makeNew(obj);
		map[obj] = p;
		return p;
	}
	else
	{
		CFRetain(it->second);
		return it->second;
	}
}

SecCFObject *
CFClassBase::required(const SecCFType *type, OSStatus errorCode)
{
	if (!type)
		MacOSError::throwMe(errorCode);

	return type->mObject.get();
}

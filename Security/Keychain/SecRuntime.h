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
// SecRuntime.h - CF runtime interface
//
#ifndef _SECURITY_SECRUNTIME_H_
#define _SECURITY_SECRUNTIME_H_

#include <CoreFoundation/CFRuntime.h>
#include <Security/refcount.h>


namespace Security
{

namespace KeychainCore
{

class SecCFObject : public RefCount
{
public:
	virtual ~SecCFObject();
    virtual bool equal(SecCFObject &other);
    virtual CFHashCode hash();
};


class SecCFType : public CFRuntimeBase
{
public:
	SecCFType(SecCFObject *obj);
	~SecCFType();

    RefPointer<SecCFObject> mObject;
};


class CFClassBase : protected CFRuntimeClass
{
protected:
    CFClassBase(const char *name);
    
    const SecCFType *makeNew(SecCFObject *obj);
    const SecCFType *handle(SecCFObject *obj);
    SecCFObject *required(const SecCFType *type, OSStatus errorCode);

private:
    static void finalizeType(CFTypeRef cf);
    static Boolean equalType(CFTypeRef cf1, CFTypeRef cf2);
    static CFHashCode hashType(CFTypeRef cf);

public:
    CFTypeID typeId;
};


template <class Object, class APITypePtr, OSStatus ErrorCode>
class CFClass : public CFClassBase
{
public:
    CFClass(const char *name) : CFClassBase(name) {}

    APITypePtr handle(Object &obj)
    {
        return APITypePtr(CFClassBase::handle(&obj));
    }

    Object *required(APITypePtr type)
    {
		Object *object = dynamic_cast<Object *>(CFClassBase::required
			(reinterpret_cast<const SecCFType *>(type), ErrorCode));
		if (!object)
			MacOSError::throwMe(ErrorCode);

		return object;
    }
	
	// CF generator functions
	APITypePtr operator () (Object *obj)
	{ return handle(*obj); }

	APITypePtr operator () (const RefPointer<Object> &obj)
	{ return handle(*obj); }
	
	Object * operator () (APITypePtr ref)
	{ return required(ref); }
};


} // end namespace KeychainCore

} // end namespace Security


#endif // !_SECURITY_SECRUNTIME_H_

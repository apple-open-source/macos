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

#if 0
//
// Refs.h
//
#ifndef _H_REFS
#define _H_REFS

#include <Security/handleobject.h>
#include <set>

namespace Security
{

namespace KeychainCore
{

class ReferencedObject : public RefCount
{
public:
	ReferencedObject() : mHandle(0) {}
	virtual ~ReferencedObject() {}

    void addedRef(CSSM_HANDLE handle) { mHandle = handle; }
    void removedRef(CSSM_HANDLE handle) { mHandle = 0; }
    CSSM_HANDLE handle() const { return mHandle; }

    void killRef();

private:
    CSSM_HANDLE mHandle;
};


class RefObject : public HandleObject, public RefCount
{
public:
    RefObject(ReferencedObject &object) : mObject(&object)
	{
		if (mObject)
			mObject->addedRef(reinterpret_cast<CSSM_HANDLE>(HandleObject::handle()));
	}

	void ref() const { RefCount::ref(); }
	unsigned int unref() const { return RefCount::unref(); }

    RefPointer<ReferencedObject> mObject;
};


inline void ReferencedObject::killRef()
{
	delete &killHandle<RefObject>(mHandle);
	mHandle = 0;
}


template <class _Object, class _ObjectImpl, class _Handle, OSStatus _ErrorCode>
class Ref
{
public:
    static _Handle handle(const _Object &object)
	{
		if (!object)
			return 0;

		_Handle handle = reinterpret_cast<_Handle>(object->handle()); // Return the existing handle if it exists
		if (handle)
		{
			retain(handle);
			return handle;
		}
	
		RefObject *ref = new RefObject(*object);
		ref->ref();
		return reinterpret_cast<_Handle>(ref->HandleObject::handle());
	}

    static void retain(_Handle handle)
    { findHandle<RefObject>(CSSM_HANDLE(handle), _ErrorCode).ref(); }

    static void release(_Handle handle)
    {
		RefObject &ref = findHandle<RefObject>(CSSM_HANDLE(handle), _ErrorCode);
        if (ref.unref() == 0)
        {
			if (ref.mObject)
				ref.mObject->removedRef(CSSM_HANDLE(handle));

			delete &killHandle<RefObject>(CSSM_HANDLE(handle), _ErrorCode);
        }
    }

	static _Object required(_Handle handle)
    {
		RefObject &ref = findHandle<RefObject>(CSSM_HANDLE(handle), _ErrorCode);
		if (!ref.mObject)
			MacOSError::throwMe(_ErrorCode);
                _ObjectImpl *impl = dynamic_cast<_ObjectImpl *>(&(*ref.mObject));
                if (!impl)
                        MacOSError::throwMe(_ErrorCode);
		return _Object(impl);
	}
};

}; // end namespace KeychainCore

} // end namespace Security

#endif // _H_REFS
#endif

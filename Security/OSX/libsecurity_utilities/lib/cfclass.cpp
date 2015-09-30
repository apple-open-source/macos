 /*
 * Copyright (c) 2000-2004,2011,2013-2014 Apple Inc. All Rights Reserved.
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
#include <CoreFoundation/CFString.h>
#include <sys/time.h>
#include <auto_zone.h>
#include <objc/objc-auto.h>

//
// CFClass
//
CFClass::CFClass(const char *name)
{
	// initialize the CFRuntimeClass structure
	version = 0;
	className = name;
	init = NULL;
	copy = NULL;
	finalize = finalizeType;
	equal = equalType;
	hash = hashType;
	copyFormattingDesc = copyFormattingDescType;
	copyDebugDesc = copyDebugDescType;
	
    // update because we are now doing our own reference counting
    version |= _kCFRuntimeCustomRefCount; // see ma, no hands!
    refcount = refCountForType;

	// register
	typeID = _CFRuntimeRegisterClass(this);
	assert(typeID != _kCFRuntimeNotATypeID);
}

uint32_t
CFClass::cleanupObject(intptr_t op, CFTypeRef cf, bool &zap)
{
    // the default is to not throw away the object
    zap = false;
    
    bool isGC = CF_IS_COLLECTABLE(cf);
    
    uint32_t currentCount;
    SecCFObject *obj = SecCFObject::optional(cf);

    uint32_t oldCount;
    currentCount = obj->updateRetainCount(op, &oldCount);
    
    if (isGC)
    {
        auto_zone_t* zone = objc_collectableZone();
        
        if (op == -1 && oldCount == 0)
        {
            auto_zone_release(zone, (void*) cf);
        }
        else if (op == 1 && oldCount == 0 && currentCount == 1)
        {
           auto_zone_retain(zone, (void*) cf);
        }
        else if (op == -1 && oldCount == 1 && currentCount == 0)
        {
            /*
                To prevent accidental resurrection, just pull it out of the
                cache.
            */
            obj->aboutToDestruct();
            auto_zone_release(zone, (void*) cf);
        }
        else if (op == 0)
        {
            return currentCount;
        }
        
        return 0;
    }

    if (op == 0)
    {
        return currentCount;
    }
    else if (currentCount == 0)
    {
        // we may not be able to delete if the caller has active children
        if (obj->mayDelete())
        {
            finalizeType(cf);
            zap = true; // ask the caller to release the mutex and zap the object
            return 0;
        }
        else
        {
            return currentCount;
        }
    }
    else 
    {
        return 0;
    }
}

uint32_t
CFClass::refCountForType(intptr_t op, CFTypeRef cf) throw()
{
    uint32_t result = 0;
    bool zap = false;

    try
    {
        SecCFObject *obj = SecCFObject::optional(cf);
		Mutex* mutex = obj->getMutexForObject();
		if (mutex == NULL)
		{
			// if the object didn't have a mutex, it wasn't cached.
			// Just clean it up and get out.
            result = cleanupObject(op, cf, zap);
		}
		else
        {
            // we have a mutex, so we need to do our cleanup operation under its control
            StLock<Mutex> _(*mutex);
            result = cleanupObject(op, cf, zap);
        }
        
        if (zap) // did we release the object?
        {
            delete obj; // should call the overloaded delete for the object
        }
    }
    catch (...)
    {
    }
    
    // keep the compiler happy
    return result;
}



void
CFClass::finalizeType(CFTypeRef cf) throw()
{
    /*
        Why are we asserting the mutex here as well as in refCountForType?
        Because the way we control the objects and the queues are different
        under GC than they are under non-GC operations.
        
        In non-GC, we need to control the lifetime of the object.  This means
        that the cache lock has to be asserted while we are determining if the
        object should live or die.  The mutex is recursive, which means that
        we won't end up with mutex inversion.
        
        In GC, GC figures out the lifetime of the object.  We probably don't need
        to assert the mutex here, but it doesn't hurt.
    */
    
    SecCFObject *obj = SecCFObject::optional(cf);

	bool isCollectable = CF_IS_COLLECTABLE(cf);

    try
	{
		Mutex* mutex = obj->getMutexForObject();
		if (mutex == NULL)
		{
			// if the object didn't have a mutex, it wasn't cached.
			// Just clean it up and get out.
			obj->aboutToDestruct(); // removes the object from its associated cache.
		}
		else
        {
            StLock<Mutex> _(*mutex);
            
            if (obj->isNew())
            {
                // New objects aren't in the cache.
                // Just clean it up and get out.
                obj->aboutToDestruct(); // removes the object from its associated cache.
                return;
            }
            
            obj->aboutToDestruct(); // removes the object from its associated cache.
        }
	}
	catch(...)
	{
	}
    
    if (isCollectable)
    {
        delete obj;
    }
}

Boolean
CFClass::equalType(CFTypeRef cf1, CFTypeRef cf2) throw()
{
	// CF checks for pointer equality and ensures type equality already
	try {
		return SecCFObject::optional(cf1)->equal(*SecCFObject::optional(cf2));
	} catch (...) {
		return false;
	}
}

CFHashCode
CFClass::hashType(CFTypeRef cf) throw()
{
	try {
		return SecCFObject::optional(cf)->hash();
	} catch (...) {
		return 666; /* Beasty return for error */
	}
}

CFStringRef
CFClass::copyFormattingDescType(CFTypeRef cf, CFDictionaryRef dict) throw()
{
	try {
		return SecCFObject::optional(cf)->copyFormattingDesc(dict);
	} catch (...) {
		return CFSTR("Exception thrown trying to format object");
	}
}

CFStringRef
CFClass::copyDebugDescType(CFTypeRef cf) throw()
{
	try {
		return SecCFObject::optional(cf)->copyDebugDesc();
	} catch (...) {
		return CFSTR("Exception thrown trying to format object");
	}
}



/*
 * Copyright (c) 1998-2003 Apple Computer, Inc. All rights reserved.
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
#include <libkern/OSAtomic.h>
#include <libkern/c++/OSDictionary.h>
#include <IOKit/IOLib.h>

#include <IOKit/IOService.h>
#include <IOKit/usb/IOUSBDevice.h>
#include <IOKit/usb/IOUSBInterface.h>
#include <IOKit/usb/IOUSBUserClient.h>

OSDefineMetaClassAndStructors(IOUSBUserClientInit, IOService);
#define super IOService

bool
IOUSBUserClientInit::init(OSDictionary * propTable)
{
    return super::init(propTable) ;
}



IOService*
IOUSBUserClientInit::probe(IOService* provider, SInt32* score)
{
    return super::probe(provider, score) ;
}



bool
IOUSBUserClientInit::start(IOService* provider)
{
    OSObject*		dictObj = getProperty("IOProviderMergeProperties");
    OSDictionary*	providerMergeProperties = NULL;
    
    //IOLog("+%s(%p)::start(%p) - provider = %s\n", getName(), this, provider, provider->getName());

    providerMergeProperties = OSDynamicCast(OSDictionary, dictObj);
    
    if ( !providerMergeProperties )
    {
        return false;
    }

    const OSSymbol*	userClientClass;
    OSObject* temp = providerMergeProperties->getObject( gIOUserClientClassKey ) ;

    //IOLog("%s(%p)::start temp = %p\n", getName(), this, temp);
    
    if ( OSDynamicCast(OSSymbol, temp) )
        userClientClass = NULL;			// already in correct form, so don't need to re-add
    else if ( OSDynamicCast(OSString, temp) )
        userClientClass = OSSymbol::withString((const OSString *) temp);	// convert to OSSymbol
    else
    {
	userClientClass = NULL;			// unknown form for key
	providerMergeProperties->removeObject(gIOUserClientClassKey);
    }

    //IOLog("%s(%p)::start userClientClass = %p\n", getName(), this, userClientClass);
    if (userClientClass)
	providerMergeProperties->setObject(gIOUserClientClassKey, (OSObject *) userClientClass);

    OSDictionary*	providerProps = provider->getPropertyTable();
    if (providerProps)
    {
        mergeProperties(providerProps, providerMergeProperties) ;
    }

    //IOLog("-%s(%p)::start(%p)\n", getName(), this, provider);
    return true ;
}



void
IOUSBUserClientInit::stop(IOService* provider)
{
    IOService::stop(provider) ;
}



void
IOUSBUserClientInit::mergeProperties(OSObject* inDest, OSObject* inSrc)
{
    OSDictionary*	dest = OSDynamicCast(OSDictionary, inDest) ;
    OSDictionary*	src = OSDynamicCast(OSDictionary, inSrc) ;

    //IOLog("+%s(%p)::mergeProperties(%p, %p)=>(%p,%p)\n", getName(), this, inDest, inSrc, dest, src);
    if (!src || !dest)
            return ;

    OSCollectionIterator*	srcIterator = OSCollectionIterator::withCollection(src) ;
    //IOLog("%s(%p)::mergeProperties: srcIterator = %p, rc = %d\n", getName(), this, srcIterator, srcIterator->getRetainCount());
   
    OSSymbol*	keyObject = NULL ;
    OSObject*	destObject = NULL ;
    while (NULL != (keyObject = OSDynamicCast(OSSymbol, srcIterator->getNextObject())))
    {
        if ((NULL != (destObject = dest->getObject(keyObject))) && (OSDynamicCast(OSDictionary,src->getObject(keyObject))))
	{
	    //IOLog("%s(%p)::mergeProperties recursing\n", getName(), this);
            mergeProperties(destObject, src->getObject(keyObject)) ;
	}
        else
	{
            dest->setObject(keyObject, src->getObject(keyObject)) ;
	}

	//IOLog("%s(%p)::mergeProperties might be releasing keyObject, rc=%d\n", getName(), this, keyObject->getRetainCount());
        //keyObject->release() ; BAD thing to do
    }
    //IOLog("%s(%p)::mergeProperties: done with srcIterator = %p, rc = %d\n", getName(), this, srcIterator, srcIterator->getRetainCount());
    srcIterator->release();
    //IOLog("-%s(%p)::mergeProperties(%p, %p)=>(%p,%p)\n", getName(), this, inDest, inSrc, dest, src);
}

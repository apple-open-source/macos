/*
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
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

#include <libkern/c++/OSDictionary.h>

#include <IOKit/IOService.h>

class IOHIDLibUserClientIniter : public IOService 
{
    OSDeclareDefaultStructors(IOHIDLibUserClientIniter);

public:
    virtual bool 		start(IOService * provider);
    
protected:
    virtual void		mergeProperties(OSObject* dest, OSObject* src) ;
};

#define super IOService
OSDefineMetaClassAndStructors(IOHIDLibUserClientIniter, IOService);

bool IOHIDLibUserClientIniter::start(IOService * provider)
{
    OSDictionary *	dictObj = OSDynamicCast(OSDictionary, getProperty("IOProviderMergeProperties"));
    OSDictionary *	providerMergeProperties = NULL;

    if (!super::start(provider))
        return false;

    
    providerMergeProperties = (dictObj) ? (OSDictionary *)dictObj->copyCollection() : NULL;
    
    if ( !providerMergeProperties )
    {
        return false;
    }

    const OSSymbol *	userClientClass = NULL;
    OSObject * 		temp = providerMergeProperties->getObject( gIOUserClientClassKey ) ;

    
    if ( userClientClass = OSDynamicCast(OSSymbol, temp) )
    {}
    else if ( OSDynamicCast(OSString, temp) )
    {
        userClientClass = OSSymbol::withString((const OSString *) temp);	// convert to OSSymbol
        providerMergeProperties->setObject(gIOUserClientClassKey, (OSObject *) userClientClass);
        userClientClass->release();
    }
    else
    {
	providerMergeProperties->removeObject(gIOUserClientClassKey);
    }

    if (userClientClass)
    {
        provider->setProperty(gIOUserClientClassKey, (OSObject *)userClientClass);
    }

    OSDictionary * providerProps = NULL;

    dictObj = provider->getPropertyTable();    
    providerProps = (dictObj) ? (OSDictionary *)dictObj->copyCollection() : NULL;
    
    if (providerProps)
    {
        mergeProperties(providerProps, providerMergeProperties) ;
        provider->setPropertyTable(providerProps);
        providerProps->release();
    }
        
    setProperty("IOProviderMergeProperties", providerMergeProperties);
    providerMergeProperties->release();

    return true ;

}

void
IOHIDLibUserClientIniter::mergeProperties(OSObject* inDest, OSObject* inSrc)
{
    OSDictionary*	dest = OSDynamicCast(OSDictionary, inDest) ;
    OSDictionary*	src = OSDynamicCast(OSDictionary, inSrc) ;

    if (!src || !dest)
            return ;

    OSCollectionIterator*	srcIterator = OSCollectionIterator::withCollection(src) ;
    
    OSSymbol*       keyObject	= NULL ;
    OSObject*       destObject	= NULL ;
    OSObject*       srcObject	= NULL ;
    while (NULL != (keyObject = OSDynamicCast(OSSymbol, srcIterator->getNextObject())))
    {
        srcObject 	= src->getObject(keyObject) ;
        destObject	= dest->getObject(keyObject) ;
                    
        if (destObject && OSDynamicCast(OSDictionary, srcObject))
            mergeProperties(destObject, srcObject );
        else if ( !destObject )
            dest->setObject(keyObject, srcObject) ;
            
    }
    
    // have to release this, or we'll leak.
    srcIterator->release() ;
}

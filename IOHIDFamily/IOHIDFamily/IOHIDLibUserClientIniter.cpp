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
    virtual OSDictionary *	copyDictionaryProperty(OSDictionary*	srcDictionary) ;
};

#define super IOService
OSDefineMetaClassAndStructors(IOHIDLibUserClientIniter, IOService);

bool IOHIDLibUserClientIniter::start(IOService * provider)
{
    OSObject *		dictObj = getProperty("IOProviderMergeProperties");
    OSDictionary *	providerMergeProperties = NULL;

    if (!super::start(provider))
        return false;

    
    providerMergeProperties = OSDynamicCast(OSDictionary, dictObj);
    
    if ( !providerMergeProperties )
    {
        return false;
    }

    const OSSymbol *	userClientClass;
    OSObject * 		temp = providerMergeProperties->getObject( gIOUserClientClassKey ) ;

    
    if ( OSDynamicCast(OSSymbol, temp) )
        userClientClass = NULL;			// already in correct form, so don't need to re-add
    else if ( OSDynamicCast(OSString, temp) )
        userClientClass = OSSymbol::withString((const OSString *) temp);	// convert to OSSymbol
    else
    {
	userClientClass = NULL;			// unknown form for key
	providerMergeProperties->removeObject(gIOUserClientClassKey);
    }

    if (userClientClass)
	providerMergeProperties->setObject(gIOUserClientClassKey, (OSObject *) userClientClass);

    OSDictionary*	providerProps = provider->getPropertyTable();
    if (providerProps)
    {
        mergeProperties(providerProps, providerMergeProperties) ;
    }

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
    
    OSSymbol*	keyObject	= NULL ;
    OSObject*	destObject	= NULL ;
    OSObject*	srcObject	= NULL ;
    while (NULL != (keyObject = OSDynamicCast(OSSymbol, srcIterator->getNextObject())))
    {
            srcObject 	= src->getObject(keyObject) ;
            destObject	= dest->getObject(keyObject) ;
            
            if (OSDynamicCast(OSDictionary, srcObject))
                    srcObject = copyDictionaryProperty((OSDictionary*)srcObject) ;
                    
            if (destObject && OSDynamicCast(OSDictionary, srcObject))
                    mergeProperties(destObject, srcObject );
            else
                    dest->setObject(keyObject, srcObject) ;

    }
    
    // have to release this, or we'll leak.
    srcIterator->release() ;
}

OSDictionary *
IOHIDLibUserClientIniter::copyDictionaryProperty( OSDictionary *	srcDictionary)
{
    OSDictionary*		result			= NULL ;
    OSObject*			srcObject		= NULL ;
    OSCollectionIterator*	srcIterator		= NULL ;
    OSSymbol*			keyObject		= NULL ;
    
    result = OSDictionary::withCapacity(srcDictionary->getCount()) ;
    if (result)
    {
        srcIterator = OSCollectionIterator::withCollection(srcDictionary) ;
        if (srcIterator)
        {
            while ( keyObject = OSDynamicCast(OSSymbol, srcIterator->getNextObject()) )
            {
                srcObject = srcDictionary->getObject(keyObject) ;
                if (OSDynamicCast(OSDictionary, srcObject))
                        srcObject = copyDictionaryProperty((OSDictionary*)srcObject) ;
                            
                result->setObject(keyObject, srcObject) ;
            }
            
        srcIterator->release() ;
        }
    }
    
    return result ;
}

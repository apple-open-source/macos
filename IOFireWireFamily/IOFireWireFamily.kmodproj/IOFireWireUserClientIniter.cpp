/*
 * Copyright (c) 1998-2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
 *  IOFireWireUserClientIniter.cpp
 *  IOFireWireFamily
 *
 *  Created by NWG on Wed Jan 24 2001.
 *  Copyright (c) 2001 Apple Computer, Inc. All rights reserved.
 *
 */

#include <IOKit/assert.h>
#include <IOKit/IOLib.h>

#include <IOKit/IOService.h>
#include <libkern/OSAtomic.h>

#include <IOKit/firewire/IOFireWireUserClientIniter.h>

#ifndef DEBUGGING_LEVEL
#define DEBUGGING_LEVEL 1
#endif

OSDefineMetaClassAndStructors(IOFireWireUserClientIniter, IOService);
OSMetaClassDefineReservedUnused(IOFireWireUserClientIniter, 0);
OSMetaClassDefineReservedUnused(IOFireWireUserClientIniter, 1);
OSMetaClassDefineReservedUnused(IOFireWireUserClientIniter, 2);
OSMetaClassDefineReservedUnused(IOFireWireUserClientIniter, 3);

UInt32						IOFireWireUserClientIniter::fHasUCIniter = false ;
IOFireWireUserClientIniter*	IOFireWireUserClientIniter::fUCIniter = NULL ;
OSDictionary*				IOFireWireUserClientIniter::fProviderMergeProperties = NULL ;
OSDictionary*				IOFireWireUserClientIniter::fPropTable = NULL ;
IOService*					IOFireWireUserClientIniter::fProvider = NULL ;


bool
IOFireWireUserClientIniter::init(OSDictionary * propTable)
{
	fPropTable = propTable ;
	fProvider = NULL ;
	
	return IOService::init(propTable) ;
}

bool
IOFireWireUserClientIniter::start(
	IOService*	provider)
{
	fProvider = provider ;

	OSObject*	dictObj = getProperty("IOProviderMergeProperties");
	
	fProviderMergeProperties = OSDynamicCast(OSDictionary, dictObj);
	
	if ( !fProviderMergeProperties )
	{
		fHasUCIniter = false;
		IOLog("%s %u: couldn't get fProviderMergeProperties\n", __FILE__, __LINE__ ) ;
		return false;
	}

	const OSSymbol*	userClientClass;
	OSObject*		temp = fProviderMergeProperties->getObject( gIOUserClientClassKey ) ;

	if ( OSDynamicCast(OSSymbol, temp) )
		userClientClass = 0;
	else if ( OSDynamicCast(OSString, temp) )
		userClientClass = OSSymbol::withString((const OSString *) temp);
	else
	{
		userClientClass = 0;
		fProviderMergeProperties->removeObject(gIOUserClientClassKey);
	}

	if (userClientClass)
		fProviderMergeProperties->setObject(gIOUserClientClassKey, (OSObject *) userClientClass);

    OSDictionary*	providerProps = fProvider->getPropertyTable() ;
	if (providerProps)
	{
		mergeProperties(providerProps, fProviderMergeProperties) ;
//		fProviderMergeProperties->flushCollection() ;
	}

    return true ;
}

void
IOFireWireUserClientIniter::stop(IOService* provider)
{
	IOService::stop(provider) ;
}

void
IOFireWireUserClientIniter::mergeProperties(OSObject* inDest, OSObject* inSrc)
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

OSDictionary*
IOFireWireUserClientIniter::copyDictionaryProperty(
	OSDictionary*	srcDictionary)
{
	OSDictionary*			result			= NULL ;
	OSObject*				srcObject		= NULL ;
	OSCollectionIterator*	srcIterator		= NULL ;
	OSSymbol*				keyObject		= NULL ;
	
	result = OSDictionary::withCapacity(srcDictionary->getCount()) ;
	if (result)
	{
		srcIterator = OSCollectionIterator::withCollection(srcDictionary) ;
		if (srcIterator)
		{
			while ( keyObject = OSDynamicCast(OSSymbol, srcIterator->getNextObject()) )
			{
				srcObject	= srcDictionary->getObject(keyObject) ;
				if (OSDynamicCast(OSDictionary, srcObject))
					srcObject = copyDictionaryProperty((OSDictionary*)srcObject) ;
				
				result->setObject(keyObject, srcObject) ;
			}
		
			srcIterator->release() ;
		}
	}
	
	return result ;
}